/*
 * hostapd - qlink client code
 * Copyright (c) 2008 - 2015 Quantenna Communications Inc
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 */

#include "qlink_client.h"
#include "eloop.h"
#include "utils/list.h"
#include <sys/ioctl.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <linux/filter.h>
#include <fcntl.h>

#define QC_TIMEOUT 10000
#define QC_ACK_TIMEOUT 500

struct qlink_client
{
	uint8_t own_addr[ETH_ALEN];
	uint8_t server_addr[ETH_ALEN];
	struct qlink_client_ops *ops;
	void *ops_ptr;
	int active;
	int fd;
	int ifindex;

	uint8_t cmd_proc_buff[QLINK_MSG_BUF_SIZE];
	uint8_t cmd_buff[QLINK_MSG_BUF_SIZE];
	le32 cmd_seq;
	le16 cmd_type;
	uint32_t cmd_frag;
	size_t cmd_size;
	int cmd_is_complete;

	uint8_t reply_buff[QLINK_MSG_BUF_SIZE];
	le16 reply_type;
	uint32_t reply_frag;
	size_t reply_size;
	int reply_is_complete;

	int ack_is_cmd;
	uint32_t ack_seq;
	uint32_t ack_frag;

	uint32_t next_out_seq;
	uint32_t in_seq;
};

#define qlink_seq_after(a, b) ((int32_t)(b) - (int32_t)(a) < 0)

static void qlink_client_force_disconnect(void *eloop_data, void *user_ctx)
{
	struct qlink_client *qc = eloop_data;

	qc->ops->disconnect(qc->ops_ptr);
}

static int qlink_client_set_filter(int sock)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_LD + BPF_H + BPF_ABS, ETH_ALEN << 1),	/* read packet type id */
		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K,
			QLINK_PROTO, 0, 1),				/* if qlink packet */
		BPF_STMT(BPF_RET + BPF_K, ETH_FRAME_LEN),		/* accept packet */
		BPF_STMT(BPF_RET + BPF_K, 0)				/* else ignore packet */
	};
	struct sock_fprog fp;

	fp.filter = filter;
	fp.len = ARRAY_SIZE(filter);

	if (setsockopt(sock, SOL_SOCKET, SO_ATTACH_FILTER, &fp, sizeof(fp)) < 0) {
		wpa_printf(MSG_ERROR, "setsockopt[SO_ATTACH_FILTER]: %s", strerror(errno));
		return -1;
	}

	return 0;
}

static void qlink_client_process_ack(struct qlink_client *qc, struct qlink_header *hdr)
{
	if (qc->ack_seq == 0) {
		/*
		 * we're not waiting for ack, ignore.
		 */
		return;
	}

	if ((qc->ack_is_cmd && (le_to_host16(hdr->type) != QLINK_ACK_CMD)) ||
		(!qc->ack_is_cmd && (le_to_host16(hdr->type) != QLINK_ACK_REPLY))) {
		/*
		 * different type of ack.
		 */
		return;
	}

	if ((qc->ack_seq != le_to_host32(hdr->seq)) ||
		(qc->ack_frag != le_to_host32(hdr->frag))) {
		/*
		 * ack for different fragment.
		 */
		return;
	}

	/*
	 * mark as acked.
	 */
	qc->ack_seq = 0;
	qc->ack_frag = 0;
}

static void qlink_client_process_cmd(struct qlink_client *qc, struct qlink_header *hdr, uint32_t len)
{
	if ((qc->cmd_seq == 0) || (qc->cmd_seq != hdr->seq)) {
		if (le_to_host32(hdr->frag) != 0) {
			return;
		}

		if ((le_to_host16(hdr->type) != QLINK_CMD_HANDSHAKE) &&
			!qlink_seq_after(le_to_host32(hdr->seq), qc->in_seq)) {
			wpa_printf(MSG_WARNING, "old qlink cmd (seq = %u, type = %d) in_seq = %u, ignoring",
				le_to_host32(hdr->seq), (int)le_to_host16(hdr->type),
				qc->in_seq);
			return;
		}

		if (qc->cmd_is_complete) {
			wpa_printf(MSG_WARNING, "replacing qlink cmd (seq = %u, type = %d) with (seq = %u, type = %d)",
				le_to_host32(qc->cmd_seq), (int)le_to_host16(qc->cmd_type),
				le_to_host32(hdr->seq), (int)le_to_host16(hdr->type));
		}

		qc->cmd_seq = hdr->seq;
		qc->cmd_type = hdr->type;
		qc->cmd_frag = 0;
		qc->cmd_size = 0;
		qc->cmd_is_complete = 0;
	}

	if ((qc->cmd_type != hdr->type) || (qc->cmd_frag != le_to_host32(hdr->frag))) {
		wpa_printf(MSG_WARNING, "out of order qlink cmd fragment: seq = %u, type = %d, frag = %u, is_last = %d, size = %d",
			le_to_host32(hdr->seq), (int)le_to_host16(hdr->type),
			le_to_host32(hdr->frag), hdr->is_last, len);
		return;
	}

	if ((qc->cmd_size + len) > sizeof(qc->cmd_buff)) {
		wpa_printf(MSG_ERROR, "qlink cmd too large: seq = %u, type = %d, frag = %u, is_last = %d, size = %d",
			le_to_host32(hdr->seq), (int)le_to_host16(hdr->type),
			le_to_host32(hdr->frag), hdr->is_last, len);
		qc->cmd_seq = 0;
		return;
	}

	memcpy(&qc->cmd_buff[qc->cmd_size], hdr + 1, len);

	++qc->cmd_frag;
	qc->cmd_size += len;

	qc->cmd_is_complete = hdr->is_last;

	if (qc->cmd_is_complete)
		qc->in_seq = le_to_host32(hdr->seq);
}

static int qlink_client_process_reply(struct qlink_client *qc, uint32_t reply_seq,
	struct qlink_header *hdr, uint32_t len)
{
	if (host_to_le32(reply_seq) != hdr->seq) {
		wpa_printf(MSG_ERROR, "unsolicited qlink reply fragment: seq = %u, type = %d, frag = %u, is_last = %d, size = %d",
			le_to_host32(hdr->seq), (int)le_to_host16(hdr->type),
			le_to_host32(hdr->frag), hdr->is_last, len);
		return 0;
	}

	if (qc->reply_frag == 0) {
		qc->reply_type = hdr->type;
		qc->reply_size = 0;
		qc->reply_is_complete = 0;
	}

	if ((qc->reply_type != hdr->type) || (qc->reply_frag != le_to_host32(hdr->frag))) {
		wpa_printf(MSG_WARNING, "out of order qlink reply fragment: seq = %u, type = %d, frag = %u, is_last = %d, size = %d",
			le_to_host32(hdr->seq), (int)le_to_host16(hdr->type),
			le_to_host32(hdr->frag), hdr->is_last, len);
		return 0;
	}

	if ((qc->reply_size + len) > sizeof(qc->reply_buff)) {
		wpa_printf(MSG_ERROR, "qlink reply too large: seq = %u, type = %d, frag = %u, is_last = %d, size = %d",
			le_to_host32(hdr->seq), (int)le_to_host16(hdr->type),
			le_to_host32(hdr->frag), hdr->is_last, len);
		qc->reply_frag = 0;
		return -1;
	}

	memcpy(&qc->reply_buff[qc->reply_size], hdr + 1, len);

	++qc->reply_frag;
	qc->reply_size += len;

	qc->reply_is_complete = hdr->is_last;

	return 0;
}

static void qlink_client_send_ack(struct qlink_client *qc, struct ethhdr *src_eh)
{
	uint8_t buff[ETH_FRAME_LEN];
	struct qlink_header *src_hdr = (struct qlink_header *)(src_eh + 1);
	struct ethhdr *dst_eh = (struct ethhdr *)&buff[0];
	struct qlink_header *dst_hdr = (struct qlink_header *)(dst_eh + 1);
	int ret;

	if ((le_to_host16(src_hdr->type) == QLINK_CMD_HANDSHAKE) ||
		(le_to_host16(src_hdr->type) == QLINK_CMD_PING)) {
		return;
	}

	os_memcpy(dst_eh->h_dest, src_eh->h_source, ETH_ALEN);
	os_memcpy(dst_eh->h_source, src_eh->h_dest, ETH_ALEN);
	dst_eh->h_proto = htons(QLINK_PROTO);

	dst_hdr->seq = src_hdr->seq;
	dst_hdr->type = host_to_le16((le_to_host16(src_hdr->type) < QLINK_REPLY_FIRST) ?
		QLINK_ACK_CMD : QLINK_ACK_REPLY);
	dst_hdr->frag = src_hdr->frag;
	dst_hdr->size = 0;
	dst_hdr->is_last = 1;

	do {
		ret = send(qc->fd, buff, sizeof(*dst_eh) + sizeof(*dst_hdr), 0);
	} while ((ret < 0) && ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)));

	if (ret < 0) {
		wpa_printf(MSG_ERROR, "send error: %s", strerror(errno));
	}
}

static int qlink_client_process_rx(struct qlink_client *qc, uint32_t reply_seq)
{
	uint8_t buff[ETH_FRAME_LEN];
	int ret;
	struct sockaddr_ll ll;
	socklen_t fromlen;
	struct qlink_header *hdr;
	uint32_t sz;
	int type;

	os_memset(&ll, 0, sizeof(ll));
	fromlen = sizeof(ll);
	do {
		ret = recvfrom(qc->fd, buff, sizeof(buff), 0, (struct sockaddr *)&ll, &fromlen);
	} while ((ret < 0) && (errno == EINTR));

	if (ret < 0) {
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
			return 0;
		}
		wpa_printf(MSG_DEBUG, "qlink recvfrom error: %s", strerror(errno));
		return -1;
	}

	if (ll.sll_pkttype != PACKET_HOST) {
		return 0;
	}

	if (ret < (sizeof(struct ethhdr) + sizeof(*hdr))) {
		wpa_printf(MSG_DEBUG, "qlink header too small: %d bytes", ret);
		return 0;
	}

	hdr = (struct qlink_header *)(&buff[0] + sizeof(struct ethhdr));

	sz = le_to_host16(hdr->size);

	if (sz > (ret - (sizeof(struct ethhdr) + sizeof(*hdr)))) {
		wpa_printf(MSG_ERROR, "qlink invalid size in header: %u bytes", sz);
		return 0;
	}

	type = le_to_host16(hdr->type);

	if (type >= QLINK_CMD_FIRST) {
		qlink_client_send_ack(qc, (struct ethhdr *)&buff[0]);
	}

	if (type < QLINK_CMD_FIRST) {
		qlink_client_process_ack(qc, hdr);
	} else if (type < QLINK_REPLY_FIRST) {
		qlink_client_process_cmd(qc, hdr, sz);
	} else if (reply_seq) {
		return qlink_client_process_reply(qc, reply_seq, hdr, sz);
	} else {
		wpa_printf(MSG_ERROR, "unsolicited qlink reply received (seq = %u, type = %d)",
			le_to_host32(hdr->seq), (int)le_to_host16(hdr->type));
	}

	return 0;
}

static void qlink_client_dispatch_cmd(struct qlink_client *qc);

static void qlink_client_dispatch_cmd_timeout(void *eloop_data, void *user_ctx)
{
	struct qlink_client *qc = eloop_data;
	qlink_client_dispatch_cmd(qc);
}

static int qlink_client_send(struct qlink_client *qc, uint32_t seq, int type, const void *data,
	uint32_t len)
{
	fd_set rfds;
	struct timeval tv_start, tv_cur, tv;
	uint8_t buff[ETH_DATA_LEN];
	struct ethhdr eh;
	struct qlink_header hdr;
	uint32_t passed_ms = 0, last_send_ms = 0;
	uint32_t frag = 0;
	uint32_t num_send = 0;
	int send_complete = 0, ret = 0;
	int resend = 0;

	if (!qc->active) {
		wpa_printf(MSG_ERROR, "no server connected, not sending");
		return -1;
	}

	os_memcpy(eh.h_dest, qc->server_addr, ETH_ALEN);
	os_memcpy(eh.h_source, qc->own_addr, ETH_ALEN);
	eh.h_proto = htons(QLINK_PROTO);

	hdr.seq = host_to_le32(seq);
	hdr.type = host_to_le16(type);

	qc->reply_is_complete = 0;
	qc->reply_frag = 0;

	qc->ack_is_cmd = (type < QLINK_REPLY_FIRST);
	qc->ack_seq = 0;
	qc->ack_frag = 0;

	gettimeofday(&tv_start, NULL);

	while (passed_ms <= QC_TIMEOUT) {
		uint32_t remain_ms = QC_TIMEOUT - passed_ms;

		if (!qc->active) {
			return -1;
		}

		if (!send_complete && (qc->ack_seq == 0) && (len == 0) && (frag > 0)) {
			send_complete = 1;

			if ((type >= QLINK_REPLY_FIRST)) {
				return 0;
			}
		}

		if (!send_complete) {
			if (qc->ack_seq == 0) {
				num_send = sizeof(buff) - sizeof(hdr) - sizeof(eh);

				if (num_send > len)
					num_send = len;

				len -= num_send;

				hdr.frag = host_to_le32(frag);
				hdr.size = host_to_le16(num_send);
				hdr.is_last = (len == 0);

				memcpy(buff, &eh, sizeof(eh));
				memcpy(&buff[0] + sizeof(eh), &hdr, sizeof(hdr));
				memcpy(&buff[0] + sizeof(eh) + sizeof(hdr), data, num_send);

				qc->ack_seq = seq;
				qc->ack_frag = frag;

				++frag;
				data += num_send;

				last_send_ms = passed_ms - QC_ACK_TIMEOUT;
				resend = 0;
			}

			if ((passed_ms - last_send_ms) >= QC_ACK_TIMEOUT) {
				if (resend) {
					wpa_printf(MSG_DEBUG, "resending (type = %d, seq = %u, frag = %u)",
						type, seq, frag - 1);
				}

				do {
					ret = send(qc->fd, buff,
						sizeof(eh) + sizeof(hdr) + num_send, 0);
				} while ((ret < 0) && ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)));

				if (ret < 0) {
					wpa_printf(MSG_ERROR, "send error: %s", strerror(errno));
					goto fail;
				}

				last_send_ms = passed_ms;
				resend = 1;
			}

			if (remain_ms > QC_ACK_TIMEOUT)
				remain_ms = QC_ACK_TIMEOUT;
		}

		FD_ZERO(&rfds);
		FD_SET(qc->fd, &rfds);
		tv.tv_sec = remain_ms / 1000;
		tv.tv_usec = (remain_ms % 1000) * 1000;

		do {
			ret = select(qc->fd + 1, &rfds, NULL, NULL, &tv);
		} while ((ret < 0) && (errno == EINTR));

		if (ret < 0) {
			wpa_printf(MSG_ERROR, "select error during qlink wait: %s", strerror(errno));
			goto fail;
		}

		if (FD_ISSET(qc->fd, &rfds)) {
			ret = qlink_client_process_rx(qc,
				((type < QLINK_REPLY_FIRST) && (len == 0)) ? seq : 0);

			if (ret != 0) {
				return ret;
			}

			if (qc->cmd_is_complete &&
				!eloop_is_timeout_registered(&qlink_client_dispatch_cmd_timeout, qc, NULL))
				eloop_register_timeout(0, 0, &qlink_client_dispatch_cmd_timeout, qc, NULL);

			if (qc->reply_is_complete && send_complete) {
				return 0;
			}
		}

		gettimeofday(&tv_cur, NULL);

		timersub(&tv_cur, &tv_start, &tv);

		passed_ms = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
	}

	wpa_printf(MSG_ERROR, "timeout during qlink send (seq = %u, type = %d)", seq, type);

fail:
	qlink_client_set_active(qc, 0);

	eloop_register_timeout(0, 0, &qlink_client_force_disconnect, qc, NULL);

	return -1;
}

static void qlink_client_dispatch_cmd(struct qlink_client *qc)
{
	int ret = 0;
	struct qlink_reply_status rs;
	uint32_t cmd_seq;
	uint8_t *cmd_buff;

	if (!qc->cmd_is_complete)
		return;

	cmd_seq = le_to_host32(qc->cmd_seq);

	qc->cmd_seq = 0;
	qc->cmd_is_complete = 0;

	/*
	 * command handlers may call back into server, when waiting for reply they may
	 * receive another command from the server (e.g. due to timeout), this will cause cmd_buff
	 * modification, thus, it may corrupt handler arguments. so make a copy of cmd_buff
	 * and use it instead.
	 */
	memcpy(qc->cmd_proc_buff, qc->cmd_buff, qc->cmd_size);

	cmd_buff = qc->cmd_proc_buff;

	switch (le_to_host16(qc->cmd_type)) {
	case QLINK_CMD_PING: {
		qc->ops->ping(qc->ops_ptr);
		return;
	}
	case QLINK_CMD_HANDSHAKE: {
		struct qlink_cmd_handshake *data =
			(struct qlink_cmd_handshake *)cmd_buff;
		qc->ops->handshake(qc->ops_ptr, le_to_host32(data->version), data->guid,
			le_to_host32(data->ping_timeout));
		return;
	}
	case QLINK_CMD_DISCONNECT: {
		int old_active;

		ret = qc->ops->disconnect(qc->ops_ptr);

		old_active = qc->active;

		if (ret == 0) {
			qlink_client_set_active(qc, 1);
		}

		rs.status = 0;
		qlink_client_send(qc, cmd_seq, QLINK_REPLY_STATUS, &rs, sizeof(rs));

		if (ret == 0) {
			qlink_client_set_active(qc, old_active);
		}
		return;
	}
	case QLINK_CMD_UPDATE_SECURITY_CONFIG: {
		struct qlink_cmd_update_security_config *data =
			(struct qlink_cmd_update_security_config *)cmd_buff;
		if (qc->cmd_size > 0)
			data->data[qc->cmd_size - 1] = '\0';
		ret = qc->ops->update_security_config(qc->ops_ptr, (char *)&data->data[0]);
		break;
	}
	case QLINK_CMD_RECV_PACKET: {
		struct qlink_cmd_recv_packet *data =
			(struct qlink_cmd_recv_packet *)cmd_buff;
		ret = qc->ops->recv_packet(qc->ops_ptr, le_to_host32(data->bss_qid),
			le_to_host16(data->protocol), data->src_addr, &data->data[0],
			qc->cmd_size - offsetof(struct qlink_cmd_recv_packet, data));
		break;
	}
	case QLINK_CMD_RECV_IWEVENT: {
		struct qlink_cmd_recv_iwevent *data =
			(struct qlink_cmd_recv_iwevent *)cmd_buff;
		ret = qc->ops->recv_iwevent(qc->ops_ptr, le_to_host32(data->bss_qid),
			le_to_host16(data->cmd), (char *)&data->data[0],
			qc->cmd_size - offsetof(struct qlink_cmd_recv_iwevent, data));
		break;
	}
	case QLINK_CMD_RECV_CTRL: {
		struct qlink_cmd_recv_ctrl *data =
			(struct qlink_cmd_recv_ctrl *)cmd_buff;
		ret = qc->ops->recv_ctrl(qc->ops_ptr, data->addr,
			(char *)&data->data[0],
			qc->cmd_size - offsetof(struct qlink_cmd_recv_ctrl, data));
		break;
	}
	case QLINK_CMD_INVALIDATE_CTRL: {
		struct qlink_cmd_invalidate_ctrl *data =
			(struct qlink_cmd_invalidate_ctrl *)cmd_buff;
		ret = qc->ops->invalidate_ctrl(qc->ops_ptr, data->addr);
		break;
	}
	default:
		wpa_printf(MSG_ERROR, "unhandled command received: %d, protocol error",
			(int)le_to_host16(qc->cmd_type));
		return;
	}

	rs.status = host_to_le32(ret);

	qlink_client_send(qc, cmd_seq, QLINK_REPLY_STATUS, &rs, sizeof(rs));
}

static int qlink_client_wait_cmd(struct qlink_client *qc, uint32_t timeout_ms)
{
	fd_set rfds;
	struct timeval tv_start, tv_cur, tv;
	uint32_t passed_ms = 0, remain_ms;
	int ret;

	gettimeofday(&tv_start, NULL);

	while (passed_ms <= timeout_ms) {
		remain_ms = timeout_ms - passed_ms;

		FD_ZERO(&rfds);
		FD_SET(qc->fd, &rfds);
		tv.tv_sec = remain_ms / 1000;
		tv.tv_usec = (remain_ms % 1000) * 1000;

		do {
			ret = select(qc->fd + 1, &rfds, NULL, NULL, &tv);
		} while ((ret < 0) && (errno == EINTR));

		if (ret < 0) {
			wpa_printf(MSG_ERROR, "select error during qlink wait: %s", strerror(errno));
			return -1;
		}

		if (FD_ISSET(qc->fd, &rfds)) {
			ret = qlink_client_process_rx(qc, 0);

			if (ret != 0) {
				return ret;
			}

			if (qc->cmd_is_complete) {
				return 0;
			}
		}

		gettimeofday(&tv_cur, NULL);

		timersub(&tv_cur, &tv_start, &tv);

		passed_ms = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
	}

	return 0;
}

static int qlink_client_send_sync(struct qlink_client *qc, qlink_cmd cmd, const void *cmd_data,
	uint32_t cmd_len, qlink_reply reply, uint32_t timeout_ms, void **reply_data,
	uint32_t *reply_size)
{
	int ret;

	++qc->next_out_seq;
	if (qc->next_out_seq == 0)
		++qc->next_out_seq;

	ret = qlink_client_send(qc, qc->next_out_seq, cmd, cmd_data, cmd_len);

	if (ret != 0)
		return ret;

	if (qc->reply_type == host_to_le16(QLINK_REPLY_STATUS)) {
		struct qlink_reply_status *rs = (struct qlink_reply_status *)&qc->reply_buff[0];

		ret = le_to_host32(rs->status);

		if (ret > 0)
			ret = 0;

		if ((reply != QLINK_REPLY_STATUS) && (ret == 0)) {
			wpa_printf(MSG_ERROR, "reply status 0 not expected, protocol error");
			return -1;
		}

		return ret;
	}

	if (qc->reply_type != host_to_le16(reply)) {
		wpa_printf(MSG_ERROR, "bad reply type %d for cmd (seq = %u, type = %d), protocol error",
			le_to_host16(qc->reply_type), qc->next_out_seq, cmd);
		return -1;
	}

	if (reply_data)
		*reply_data = &qc->reply_buff[0];

	if (reply_size)
		*reply_size = qc->reply_size;

	return 0;
}

static void qlink_client_rx(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct qlink_client *qc = eloop_ctx;

	qlink_client_process_rx(qc, 0);

	qlink_client_dispatch_cmd(qc);
}

struct qlink_client *qlink_client_init(struct qlink_client_ops *ops, void *ops_ptr,
	const char *ifname, const uint8_t *server_addr)
{
	struct qlink_client *qc;
	struct ifreq ifr;
	struct sockaddr_ll ll;

	qc = os_malloc(sizeof(*qc));

	if (qc == NULL) {
		wpa_printf(MSG_ERROR, "cannot allocate qlink client, no mem");
		goto fail_alloc;
	}

	os_memset(qc, 0, sizeof(*qc));

	os_memcpy(qc->server_addr, server_addr, sizeof(qc->server_addr));

	qc->ops = ops;
	qc->ops_ptr = ops_ptr;

	qc->fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (qc->fd < 0) {
		wpa_printf(MSG_ERROR, "%s: socket(PF_PACKET): %s", __func__, strerror(errno));
		goto fail_socket;
	}

	if (qlink_client_set_filter(qc->fd) < 0) {
		goto fail_filter;
	}

	if (fcntl(qc->fd, F_SETFL, O_NONBLOCK) != 0)
		goto fail_filter;

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

	if (ioctl(qc->fd, SIOCGIFINDEX, &ifr) < 0) {
		wpa_printf(MSG_ERROR, "%s: ioctl[SIOCGIFINDEX]: %s", __func__, strerror(errno));
		goto fail_filter;
	}
	qc->ifindex = ifr.ifr_ifindex;

	os_memset(&ll, 0, sizeof(ll));
	ll.sll_family = PF_PACKET;
	ll.sll_ifindex = ifr.ifr_ifindex;
	ll.sll_protocol = htons(ETH_P_ALL);

	if (bind(qc->fd, (struct sockaddr *)&ll, sizeof(ll)) < 0) {
		wpa_printf(MSG_ERROR, "%s: bind[PF_PACKET]: %s", __func__, strerror(errno));
		goto fail_filter;
	}

	if (ioctl(qc->fd, SIOCGIFHWADDR, &ifr) < 0) {
		wpa_printf(MSG_ERROR, "%s: ioctl[SIOCGIFHWADDR]: %s", __func__, strerror(errno));
		goto fail_filter;
	}

	os_memcpy(qc->own_addr, ifr.ifr_hwaddr.sa_data, sizeof(qc->own_addr));

	eloop_register_read_sock(qc->fd, &qlink_client_rx, qc, NULL);

	return qc;

fail_filter:
	close(qc->fd);
fail_socket:
	os_free(qc);
fail_alloc:

	return NULL;
}

void qlink_client_deinit(struct qlink_client *qc)
{
	eloop_cancel_timeout(&qlink_client_force_disconnect, qc, NULL);
	eloop_cancel_timeout(&qlink_client_dispatch_cmd_timeout, qc, NULL);
	eloop_unregister_read_sock(qc->fd);
	close(qc->fd);
	os_free(qc);
}

void qlink_client_handshake(struct qlink_client *qc, const uint8_t *guid,
	uint32_t ping_timeout_ms, uint32_t flags)
{
	uint8_t buff[ETH_FRAME_LEN];
	struct ethhdr *eh = (struct ethhdr *)&buff[0];
	struct qlink_header *hdr = (struct qlink_header *)(eh + 1);
	struct qlink_cmd_handshake *cmd = (struct qlink_cmd_handshake *)(hdr + 1);
	int ret;

	++qc->next_out_seq;
	if (qc->next_out_seq == 0)
		++qc->next_out_seq;

	os_memcpy(eh->h_dest, qc->server_addr, ETH_ALEN);
	os_memcpy(eh->h_source, qc->own_addr, ETH_ALEN);
	eh->h_proto = htons(QLINK_PROTO);

	hdr->seq = host_to_le32(qc->next_out_seq);
	hdr->type = host_to_le16(QLINK_CMD_HANDSHAKE);
	hdr->frag = 0;
	hdr->size = host_to_le16(sizeof(*cmd));
	hdr->is_last = 1;

	os_memcpy(cmd->guid, guid, sizeof(cmd->guid));
	cmd->version = host_to_le32(QLINK_VERSION);
	cmd->ping_timeout = host_to_le32(ping_timeout_ms);
	cmd->flags = host_to_le32(flags);

	do {
		ret = send(qc->fd, buff, sizeof(*eh) + sizeof(*hdr) + sizeof(*cmd), 0);
	} while ((ret < 0) && ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)));

	if (ret < 0) {
		wpa_printf(MSG_ERROR, "send error: %s", strerror(errno));
	}
}

int qlink_client_wait_handshake(struct qlink_client *qc, uint32_t timeout_ms, uint32_t *version,
	uint8_t *guid, uint32_t *ping_timeout_ms)
{
	struct timeval tv_start, tv_cur, tv;
	uint32_t passed_ms = 0, remain_ms;
	int ret;

	gettimeofday(&tv_start, NULL);

	while (passed_ms <= timeout_ms) {
		remain_ms = timeout_ms - passed_ms;

		ret = qlink_client_wait_cmd(qc, remain_ms);

		if (ret != 0)
			return ret;

		if (qc->cmd_is_complete) {
			qc->cmd_seq = 0;
			qc->cmd_is_complete = 0;

			if (le_to_host16(qc->cmd_type) == QLINK_CMD_HANDSHAKE) {
				struct qlink_cmd_handshake *data =
					(struct qlink_cmd_handshake *)&qc->cmd_buff[0];
				*version = le_to_host32(data->version);
				os_memcpy(guid, data->guid, sizeof(data->guid));
				*ping_timeout_ms = le_to_host32(data->ping_timeout);

				return 0;
			}
		}

		gettimeofday(&tv_cur, NULL);

		timersub(&tv_cur, &tv_start, &tv);

		passed_ms = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
	}

	return 1;
}

void qlink_client_set_active(struct qlink_client *qc, int active)
{
	qc->active = active;
}

void qlink_client_ping(struct qlink_client *qc)
{
	uint8_t buff[ETH_FRAME_LEN];
	struct ethhdr *eh = (struct ethhdr *)&buff[0];
	struct qlink_header *hdr = (struct qlink_header *)(eh + 1);
	int ret;

	++qc->next_out_seq;
	if (qc->next_out_seq == 0)
		++qc->next_out_seq;

	os_memcpy(eh->h_dest, qc->server_addr, ETH_ALEN);
	os_memcpy(eh->h_source, qc->own_addr, ETH_ALEN);
	eh->h_proto = htons(QLINK_PROTO);

	hdr->seq = host_to_le32(qc->next_out_seq);
	hdr->type = host_to_le16(QLINK_CMD_PING);
	hdr->frag = 0;
	hdr->size = 0;
	hdr->is_last = 1;

	do {
		ret = send(qc->fd, buff, sizeof(*eh) + sizeof(*hdr), 0);
	} while ((ret < 0) && ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)));

	if (ret < 0) {
		wpa_printf(MSG_ERROR, "send error: %s", strerror(errno));
	}
}

int qlink_client_disconnect(struct qlink_client *qc)
{
	return qlink_client_send_sync(qc, QLINK_CMD_DISCONNECT, NULL,
		0, QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);
}

int qlink_client_update_security_config(struct qlink_client *qc, const char *security_config)
{
	return qlink_client_send_sync(qc, QLINK_CMD_UPDATE_SECURITY_CONFIG, security_config,
		strlen(security_config) + 1, QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);
}

int qlink_client_bss_add(struct qlink_client *qc, uint32_t qid, const char *ifname,
	const char *brname, uint8_t *own_addr)
{
	int ret;
	struct qlink_cmd_bss_add cmd;
	struct qlink_reply_bss_add *reply;

	os_memset(&cmd, 0, sizeof(cmd));

	cmd.qid = host_to_le32(qid);
	strncpy(cmd.ifname, ifname, sizeof(cmd.ifname) - 1);
	strncpy(cmd.brname, brname, sizeof(cmd.brname) - 1);

	ret = qlink_client_send_sync(qc, QLINK_CMD_BSS_ADD, &cmd, sizeof(cmd),
		QLINK_REPLY_BSS_ADD, QC_TIMEOUT, (void *)&reply, NULL);

	if (ret != 0)
		return ret;

	os_memcpy(own_addr, reply->own_addr, sizeof(reply->own_addr));

	return 0;
}

int qlink_client_bss_remove(struct qlink_client *qc, uint32_t bss_qid)
{
	struct qlink_cmd_bss_remove cmd;

	os_memset(&cmd, 0, sizeof(cmd));

	cmd.qid = host_to_le32(bss_qid);

	return qlink_client_send_sync(qc, QLINK_CMD_BSS_REMOVE, &cmd, sizeof(cmd),
		QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);
}

int qlink_client_handle_bridge_eapol(struct qlink_client *qc, uint32_t bss_qid)
{
	struct qlink_cmd_handle_bridge_eapol cmd;

	os_memset(&cmd, 0, sizeof(cmd));

	cmd.bss_qid = host_to_le32(bss_qid);

	return qlink_client_send_sync(qc, QLINK_CMD_HANDLE_BRIDGE_EAPOL, &cmd, sizeof(cmd),
		QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);
}

int qlink_client_get_driver_capa(struct qlink_client *qc, uint32_t bss_qid, int *we_version,
	uint8_t *extended_capa_len, uint8_t **extended_capa, uint8_t **extended_capa_mask)
{
	int ret;
	struct qlink_cmd_get_driver_capa cmd;
	struct qlink_reply_get_driver_capa *reply;
	uint8_t *extended_capa_tmp;
	uint8_t *extended_capa_mask_tmp;

	os_memset(&cmd, 0, sizeof(cmd));

	cmd.bss_qid = host_to_le32(bss_qid);

	ret = qlink_client_send_sync(qc, QLINK_CMD_GET_DRIVER_CAPA, &cmd, sizeof(cmd),
		QLINK_REPLY_GET_DRIVER_CAPA, QC_TIMEOUT, (void *)&reply, NULL);

	if (ret != 0)
		return ret;

	extended_capa_tmp = os_malloc(reply->extended_capa_len);

	if (!extended_capa_tmp && (reply->extended_capa_len > 0)) {
		wpa_printf(MSG_ERROR, "cannot allocate extended_capa, no mem");
		return -1;
	}

	extended_capa_mask_tmp = os_malloc(reply->extended_capa_len);

	if (!extended_capa_mask_tmp && (reply->extended_capa_len > 0)) {
		wpa_printf(MSG_ERROR, "cannot allocate extended_capa_mask, no mem");
		os_free(extended_capa_tmp);
		return -1;
	}

	os_memcpy(extended_capa_tmp, &reply->data[0], reply->extended_capa_len);
	os_memcpy(extended_capa_mask_tmp, &reply->data[0] + reply->extended_capa_len,
		reply->extended_capa_len);

	*we_version = le_to_host32(reply->we_version);
	*extended_capa_len = reply->extended_capa_len;
	*extended_capa = extended_capa_tmp;
	*extended_capa_mask = extended_capa_mask_tmp;

	return 0;
}

int qlink_client_set_ssid(struct qlink_client *qc, uint32_t bss_qid, const uint8_t *buf, int len)
{
	struct qlink_cmd_set_ssid *cmd;
	int ret, cmd_size = offsetof(struct qlink_cmd_set_ssid, ssid) + len + 1;

	cmd = os_zalloc(cmd_size);

	if (cmd == NULL) {
		wpa_printf(MSG_ERROR, "cannot allocate cmd_set_ssid, no mem");
		return -1;
	}

	cmd->bss_qid = host_to_le32(bss_qid);
	os_memcpy(&cmd->ssid[0], buf, len + 1);

	ret = qlink_client_send_sync(qc, QLINK_CMD_SET_SSID, cmd, cmd_size,
		QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);

	os_free(cmd);

	return ret;
}

int qlink_client_set_80211_param(struct qlink_client *qc, uint32_t bss_qid, int op, int arg)
{
	struct qlink_cmd_set_80211_param cmd;

	os_memset(&cmd, 0, sizeof(cmd));

	cmd.bss_qid = host_to_le32(bss_qid);
	cmd.op = host_to_le32(op);
	cmd.arg = host_to_le32(arg);

	return qlink_client_send_sync(qc, QLINK_CMD_SET_80211_PARAM, &cmd, sizeof(cmd),
		QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);
}

int qlink_client_set_80211_priv(struct qlink_client *qc, uint32_t bss_qid, int op, void *data,
	int len)
{
	struct qlink_cmd_set_80211_priv *cmd;
	int ret, cmd_size = offsetof(struct qlink_cmd_set_80211_priv, data) + len;
	struct qlink_reply_set_80211_priv *reply;
	uint32_t reply_size = 0;

	cmd = os_zalloc(cmd_size);

	if (cmd == NULL) {
		wpa_printf(MSG_ERROR, "cannot allocate cmd_set_80211_priv, no mem");
		return -1;
	}

	cmd->bss_qid = host_to_le32(bss_qid);
	cmd->op = host_to_le32(op);
	os_memcpy(&cmd->data[0], data, len);

	ret = qlink_client_send_sync(qc, QLINK_CMD_SET_80211_PRIV, cmd, cmd_size,
		QLINK_REPLY_SET_80211_PRIV, QC_TIMEOUT, (void *)&reply, &reply_size);

	os_free(cmd);

	if (ret != 0)
		return ret;

	if (reply_size != len) {
		wpa_printf(MSG_ERROR, "reply data size is not the same as cmd data size");
		return -1;
	}

	os_memcpy(data, &reply->data[0], len);

	return 0;
}

int qlink_client_commit(struct qlink_client *qc, uint32_t bss_qid)
{
	struct qlink_cmd_commit cmd;

	os_memset(&cmd, 0, sizeof(cmd));

	cmd.bss_qid = host_to_le32(bss_qid);

	return qlink_client_send_sync(qc, QLINK_CMD_COMMIT, &cmd, sizeof(cmd),
		QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);
}

int qlink_client_set_interworking(struct qlink_client *qc, uint32_t bss_qid, uint8_t eid,
	int interworking, uint8_t access_network_type, const uint8_t *hessid)
{
	struct qlink_cmd_set_interworking cmd;

	os_memset(&cmd, 0, sizeof(cmd));

	cmd.bss_qid = host_to_le32(bss_qid);
	cmd.eid = eid;
	cmd.interworking = interworking;
	cmd.access_network_type = access_network_type;
	if (hessid)
		os_memcpy(cmd.hessid, hessid, sizeof(cmd.hessid));

	return qlink_client_send_sync(qc, QLINK_CMD_SET_INTERWORKING, &cmd, sizeof(cmd),
		QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);
}

int qlink_client_brcm(struct qlink_client *qc, uint32_t bss_qid, uint8_t *data, uint32_t len)
{
	struct qlink_cmd_brcm *cmd;
	int ret, cmd_size = offsetof(struct qlink_cmd_brcm, data) + len;
	struct qlink_reply_brcm *reply;
	uint32_t reply_size = 0;

	cmd = os_zalloc(cmd_size);

	if (cmd == NULL) {
		wpa_printf(MSG_ERROR, "cannot allocate cmd_brcm, no mem");
		return -1;
	}

	cmd->bss_qid = host_to_le32(bss_qid);
	os_memcpy(&cmd->data[0], data, len);

	ret = qlink_client_send_sync(qc, QLINK_CMD_BRCM, cmd, cmd_size,
		QLINK_REPLY_BRCM, QC_TIMEOUT, (void *)&reply, &reply_size);

	os_free(cmd);

	if (ret != 0)
		return ret;

	if (reply_size != len) {
		wpa_printf(MSG_ERROR, "reply data size is not the same as cmd data size");
		return -1;
	}

	os_memcpy(data, &reply->data[0], len);

	return 0;
}

int qlink_client_check_if(struct qlink_client *qc, uint32_t bss_qid)
{
	struct qlink_cmd_check_if cmd;

	os_memset(&cmd, 0, sizeof(cmd));

	cmd.bss_qid = host_to_le32(bss_qid);

	return qlink_client_send_sync(qc, QLINK_CMD_CHECK_IF, &cmd, sizeof(cmd),
		QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);
}

int qlink_client_set_channel(struct qlink_client *qc, uint32_t bss_qid, int channel)
{
	struct qlink_cmd_set_channel cmd;

	os_memset(&cmd, 0, sizeof(cmd));

	cmd.bss_qid = host_to_le32(bss_qid);
	cmd.channel = host_to_le32(channel);

	return qlink_client_send_sync(qc, QLINK_CMD_SET_CHANNEL, &cmd, sizeof(cmd),
		QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);
}

int qlink_client_get_ssid(struct qlink_client *qc, uint32_t bss_qid, uint8_t *buf, int *len)
{
	struct qlink_cmd_get_ssid cmd;
	struct qlink_reply_get_ssid *reply;
	uint32_t reply_size = 0;
	int ret;

	cmd.bss_qid = host_to_le32(bss_qid);
	cmd.len = host_to_le32(*len);

	ret = qlink_client_send_sync(qc, QLINK_CMD_GET_SSID, &cmd, sizeof(cmd),
		QLINK_REPLY_GET_SSID, QC_TIMEOUT, (void *)&reply, &reply_size);

	if (ret != 0)
		return ret;

	if ((int)reply_size > *len) {
		wpa_printf(MSG_ERROR, "reply ssid is longer than buffer size");
		return -1;
	}

	os_memcpy(buf, &reply->ssid[0], reply_size);

	*len = reply_size;

	return 0;
}

int qlink_client_send_action(struct qlink_client *qc, uint32_t bss_qid, const void *data, int len)
{
	struct qlink_cmd_send_action *cmd;
	int ret, cmd_size = offsetof(struct qlink_cmd_send_action, data) + len;

	cmd = os_zalloc(cmd_size);

	if (cmd == NULL) {
		wpa_printf(MSG_ERROR, "cannot allocate cmd_send_action, no mem");
		return -1;
	}

	cmd->bss_qid = host_to_le32(bss_qid);
	os_memcpy(&cmd->data[0], data, len);

	ret = qlink_client_send_sync(qc, QLINK_CMD_SEND_ACTION, cmd, cmd_size,
		QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);

	os_free(cmd);

	return ret;
}

int qlink_client_init_ctrl(struct qlink_client *qc, const char *ctrl_dir,
	const char *ctrl_name)
{
	struct qlink_cmd_init_ctrl cmd;

	os_memset(&cmd, 0, sizeof(cmd));

	strncpy(cmd.ctrl_dir, ctrl_dir, sizeof(cmd.ctrl_dir) - 1);
	strncpy(cmd.ctrl_name, ctrl_name, sizeof(cmd.ctrl_name) - 1);

	return qlink_client_send_sync(qc, QLINK_CMD_INIT_CTRL, &cmd, sizeof(cmd),
		QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);
}

int qlink_client_send_ctrl(struct qlink_client *qc, const char *addr, const char *data, int len)
{
	struct qlink_cmd_send_ctrl *cmd;
	int ret, cmd_size = offsetof(struct qlink_cmd_send_ctrl, data) + len;

	cmd = os_zalloc(cmd_size);

	if (cmd == NULL) {
		wpa_printf(MSG_ERROR, "cannot allocate cmd_send_ctrl, no mem");
		return -1;
	}

	strncpy(cmd->addr, addr, sizeof(cmd->addr) - 1);
	os_memcpy(&cmd->data[0], data, len);

	ret = qlink_client_send_sync(qc, QLINK_CMD_SEND_CTRL, cmd, cmd_size,
		QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);

	os_free(cmd);

	return ret;
}

int qlink_client_vap_add(struct qlink_client *qc, uint32_t qid, const uint8_t *bssid,
	const char *ifname, const char *brname, uint8_t *own_addr)
{
	int ret;
	struct qlink_cmd_vap_add cmd;
	struct qlink_reply_vap_add *reply;

	os_memset(&cmd, 0, sizeof(cmd));

	cmd.qid = host_to_le32(qid);
	memcpy(cmd.bssid, bssid, sizeof(cmd.bssid));
	strncpy(cmd.ifname, ifname, sizeof(cmd.ifname) - 1);
	strncpy(cmd.brname, brname, sizeof(cmd.brname) - 1);

	ret = qlink_client_send_sync(qc, QLINK_CMD_VAP_ADD, &cmd, sizeof(cmd),
		QLINK_REPLY_VAP_ADD, QC_TIMEOUT, (void *)&reply, NULL);

	if (ret != 0)
		return ret;

	os_memcpy(own_addr, reply->own_addr, sizeof(reply->own_addr));

	return 0;
}

int qlink_client_vlan_set_sta(struct qlink_client *qc, const uint8_t *addr, int vlan_id)
{
	struct qlink_cmd_vlan_set_sta cmd;

	os_memset(&cmd, 0, sizeof(cmd));

	memcpy(cmd.addr, addr, sizeof(cmd.addr));
	cmd.vlan_id = host_to_le32(vlan_id);

	return qlink_client_send_sync(qc, QLINK_CMD_VLAN_SET_STA, &cmd, sizeof(cmd),
		QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);
}

int qlink_client_vlan_set_dyn(struct qlink_client *qc, const char *ifname, int enable)
{
	struct qlink_cmd_vlan_set_dyn cmd;

	os_memset(&cmd, 0, sizeof(cmd));

	strncpy(cmd.ifname, ifname, sizeof(cmd.ifname) - 1);
	cmd.enable = enable;

	return qlink_client_send_sync(qc, QLINK_CMD_VLAN_SET_DYN, &cmd, sizeof(cmd),
		QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);
}

int qlink_client_vlan_set_group(struct qlink_client *qc, const char *ifname, int vlan_id,
	int enable)
{
	struct qlink_cmd_vlan_set_group cmd;

	os_memset(&cmd, 0, sizeof(cmd));

	strncpy(cmd.ifname, ifname, sizeof(cmd.ifname) - 1);
	cmd.vlan_id = host_to_le32(vlan_id);
	cmd.enable = enable;

	return qlink_client_send_sync(qc, QLINK_CMD_VLAN_SET_GROUP, &cmd, sizeof(cmd),
		QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);
}

int qlink_client_ready(struct qlink_client *qc)
{
	return qlink_client_send_sync(qc, QLINK_CMD_READY, NULL, 0,
		QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);
}

int qlink_client_set_qos_map(struct qlink_client *qc, uint32_t bss_qid, const uint8_t *buf)
{
	struct qlink_cmd_set_qos_map cmd;

	os_memset(&cmd, 0, sizeof(cmd));

	cmd.bss_qid = host_to_le32(bss_qid);
	os_memcpy(cmd.qos_map, buf, sizeof(cmd.qos_map));

	return qlink_client_send_sync(qc, QLINK_CMD_SET_QOS_MAP, &cmd, sizeof(cmd),
		QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);
}
