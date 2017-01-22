/**
  Copyright (c) 2008 - 2015 Quantenna Communications Inc
  All Rights Reserved

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 **/

#include "qlink_server.h"
#include "hapr_log.h"
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <linux/filter.h>
#include <pthread.h>
#include <sys/time.h>
#include <fcntl.h>

#define QC_TIMEOUT 10000
#define QC_ACK_TIMEOUT 500

struct qlink_server
{
	uint8_t own_addr[ETH_ALEN];
	uint8_t client_addr[ETH_ALEN];

	struct qlink_server_ops *ops;
	void *ops_ptr;

	struct hapr_eloop *eloop;

	int fd;
	int ifindex;

	pthread_mutex_t m;
	pthread_cond_t c;

	uint8_t cmd_buff[QLINK_MSG_BUF_SIZE];
	__le32 cmd_seq;
	__le16 cmd_type;
	uint32_t cmd_frag;
	size_t cmd_size;
	uint8_t cmd_addr[ETH_ALEN];
	int cmd_is_complete;

	uint8_t reply_buff[QLINK_MSG_BUF_SIZE];
	__le32 reply_seq;
	__le16 reply_type;
	uint32_t reply_frag;
	size_t reply_size;

	uint32_t cmd_ack_seq;
	uint32_t cmd_ack_frag;

	uint32_t reply_ack_seq;
	uint32_t reply_ack_frag;

	uint32_t next_out_seq;
	uint32_t in_seq;
};

#define qlink_seq_after(a, b) ((int32_t)(b) - (int32_t)(a) < 0)

static void qlink_server_force_disconnect(void *user_data)
{
	struct qlink_server *qs = user_data;

	qs->ops->disconnect(qs->ops_ptr);
}

static int qlink_server_set_filter(int sock)
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
	fp.len = sizeof(filter)/sizeof(filter[0]);

	if (setsockopt(sock, SOL_SOCKET, SO_ATTACH_FILTER, &fp, sizeof(fp)) < 0) {
		HAPR_LOG(ERROR, "setsockopt[SO_ATTACH_FILTER]: %s", strerror(errno));
		return -1;
	}

	return 0;
}

static void qlink_server_dispatch_cmd(struct qlink_server *qs);

static void qlink_server_dispatch_cmd_timeout(void *user_data)
{
	struct qlink_server *qs = user_data;
	qlink_server_dispatch_cmd(qs);
}

static int qlink_server_process_rx(struct qlink_server *qs);

static void qlink_server_send_reply(struct qlink_server *qs, uint32_t seq, qlink_reply reply,
	const void *data, uint32_t len)
{
	fd_set rfds;
	struct timeval tv_start, tv_cur, tv;
	uint8_t buff[ETH_DATA_LEN];
	struct ethhdr eh;
	struct qlink_header hdr;
	uint32_t passed_ms = 0, last_send_ms = 0;
	uint32_t frag = 0;
	uint32_t num_send = 0;
	int ret = 0;
	int resend = 0;

	if (memcmp(qs->client_addr, "\x00\x00\x00\x00\x00\x00", sizeof(qs->client_addr)) == 0) {
		HAPR_LOG(ERROR, "no client connected, not sending");
		return;
	}

	memcpy(eh.h_dest, qs->client_addr, ETH_ALEN);
	memcpy(eh.h_source, qs->own_addr, ETH_ALEN);
	eh.h_proto = htons(QLINK_PROTO);

	hdr.seq = host_to_le32(seq);
	hdr.type = host_to_le16(reply);

	qs->reply_ack_seq = 0;
	qs->reply_ack_frag = 0;

	gettimeofday(&tv_start, NULL);

	while (passed_ms <= QC_TIMEOUT) {
		uint32_t remain_ms = QC_TIMEOUT - passed_ms;

		if (memcmp(qs->client_addr, "\x00\x00\x00\x00\x00\x00", sizeof(qs->client_addr)) == 0) {
			return;
		}

		if (qs->reply_ack_seq == 0) {
			if ((len == 0) && (frag > 0)) {
				return;
			}

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

			qs->reply_ack_seq = seq;
			qs->reply_ack_frag = frag;

			++frag;
			data += num_send;

			last_send_ms = passed_ms - QC_ACK_TIMEOUT;
			resend = 0;
		}

		if ((passed_ms - last_send_ms) >= QC_ACK_TIMEOUT) {
			if (resend) {
				HAPR_LOG(DEBUG, "resending (type = %d, seq = %u, frag = %u)",
					reply, seq, frag - 1);
			}

			do {
				ret = send(qs->fd, buff,
					sizeof(eh) + sizeof(hdr) + num_send, 0);
			} while ((ret < 0) && ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)));

			if (ret < 0) {
				HAPR_LOG(ERROR, "send error: %s", strerror(errno));
				goto fail;
			}

			last_send_ms = passed_ms;
			resend = 1;
		}

		if (remain_ms > QC_ACK_TIMEOUT)
			remain_ms = QC_ACK_TIMEOUT;

		FD_ZERO(&rfds);
		FD_SET(qs->fd, &rfds);
		tv.tv_sec = remain_ms / 1000;
		tv.tv_usec = (remain_ms % 1000) * 1000;

		do {
			ret = select(qs->fd + 1, &rfds, NULL, NULL, &tv);
		} while ((ret < 0) && (errno == EINTR));

		if (ret < 0) {
			HAPR_LOG(ERROR, "select error during qlink wait: %s", strerror(errno));
			goto fail;
		}

		if (FD_ISSET(qs->fd, &rfds)) {
			ret = qlink_server_process_rx(qs);

			if (ret != 0) {
				goto fail;
			}

			if (qs->cmd_is_complete &&
				!hapr_eloop_is_timeout_registered(qs->eloop, &qlink_server_dispatch_cmd_timeout, qs))
				hapr_eloop_register_timeout(qs->eloop, &qlink_server_dispatch_cmd_timeout, qs, 0);
		}

		gettimeofday(&tv_cur, NULL);

		timersub(&tv_cur, &tv_start, &tv);

		passed_ms = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
	}

	HAPR_LOG(ERROR, "timeout during qlink send reply (seq = %u, type = %d)", seq, reply);

fail:
	qlink_server_set_client_addr(qs, NULL);

	hapr_eloop_register_timeout(qs->eloop, &qlink_server_force_disconnect, qs, 0);
}

static void qlink_server_dispatch_cmd(struct qlink_server *qs)
{
	int ret = 0;
	struct qlink_reply_status rs;
	uint32_t cmd_seq = le_to_host32(qs->cmd_seq);

	if (!qs->cmd_is_complete)
		return;

	qs->cmd_seq = 0;
	qs->cmd_is_complete = 0;

	switch (le_to_host16(qs->cmd_type)) {
	case QLINK_CMD_PING: {
		qs->ops->ping(qs->ops_ptr);
		return;
	}
	case QLINK_CMD_HANDSHAKE: {
		struct qlink_cmd_handshake *data =
			(struct qlink_cmd_handshake *)&qs->cmd_buff[0];
		qs->ops->handshake(qs->ops_ptr, qs->cmd_addr, le_to_host32(data->version),
			data->guid, le_to_host32(data->ping_timeout), le_to_host32(data->flags));
		return;
	}
	case QLINK_CMD_DISCONNECT: {
		uint8_t old_client_addr[ETH_ALEN];
		uint8_t client_addr[ETH_ALEN];

		memcpy(old_client_addr, qs->client_addr, sizeof(old_client_addr));
		ret = qs->ops->disconnect(qs->ops_ptr);
		if (ret == 0) {
			memcpy(client_addr, qs->client_addr, sizeof(client_addr));
			memcpy(qs->client_addr, old_client_addr, sizeof(old_client_addr));
		}
		rs.status = 0;
		qlink_server_send_reply(qs, cmd_seq, QLINK_REPLY_STATUS, &rs, sizeof(rs));
		if (ret == 0) {
			memcpy(qs->client_addr, client_addr, sizeof(client_addr));
		}
		return;
	}
	case QLINK_CMD_UPDATE_SECURITY_CONFIG: {
		struct qlink_cmd_update_security_config *data =
			(struct qlink_cmd_update_security_config *)&qs->cmd_buff[0];
		if (qs->cmd_size > 0)
			data->data[qs->cmd_size - 1] = '\0';
		ret = qs->ops->update_security_config(qs->ops_ptr, (char *)&data->data[0]);
		break;
	}
	case QLINK_CMD_BSS_ADD: {
		struct qlink_cmd_bss_add *data =
			(struct qlink_cmd_bss_add *)&qs->cmd_buff[0];
		struct qlink_reply_bss_add reply;
		ret = qs->ops->bss_add(qs->ops_ptr, le_to_host32(data->qid), data->ifname,
			data->brname, reply.own_addr);
		if (ret != 0)
			break;
		qlink_server_send_reply(qs, cmd_seq, QLINK_REPLY_BSS_ADD, &reply, sizeof(reply));
		return;
	}
	case QLINK_CMD_BSS_REMOVE: {
		struct qlink_cmd_bss_remove *data =
			(struct qlink_cmd_bss_remove *)&qs->cmd_buff[0];
		ret = qs->ops->bss_remove(qs->ops_ptr, le_to_host32(data->qid));
		break;
	}
	case QLINK_CMD_HANDLE_BRIDGE_EAPOL: {
		struct qlink_cmd_handle_bridge_eapol *data =
			(struct qlink_cmd_handle_bridge_eapol *)&qs->cmd_buff[0];
		ret = qs->ops->handle_bridge_eapol(qs->ops_ptr, le_to_host32(data->bss_qid));
		break;
	}
	case QLINK_CMD_GET_DRIVER_CAPA: {
		struct qlink_cmd_get_driver_capa *data =
			(struct qlink_cmd_get_driver_capa *)&qs->cmd_buff[0];
		struct qlink_reply_get_driver_capa *reply;
		int we_version;
		uint8_t extended_capa_len;
		uint8_t *extended_capa;
		uint8_t *extended_capa_mask;
		ret = qs->ops->get_driver_capa(qs->ops_ptr, le_to_host32(data->bss_qid),
			&we_version, &extended_capa_len, &extended_capa, &extended_capa_mask);
		if (ret != 0)
			break;
		reply = malloc(sizeof(*reply) + (extended_capa_len * 2));
		if (!reply) {
			free(extended_capa);
			free(extended_capa_mask);
			ret = -ENOMEM;
			break;
		}
		reply->we_version = host_to_le32(we_version);
		reply->extended_capa_len = extended_capa_len;
		memcpy(&reply->data[0], extended_capa, extended_capa_len);
		memcpy(&reply->data[0] + extended_capa_len, extended_capa_mask, extended_capa_len);
		free(extended_capa);
		free(extended_capa_mask);
		qlink_server_send_reply(qs, cmd_seq, QLINK_REPLY_GET_DRIVER_CAPA, reply,
			sizeof(*reply) + (extended_capa_len * 2));
		free(reply);
		return;
	}
	case QLINK_CMD_SET_SSID: {
		struct qlink_cmd_set_ssid *data =
			(struct qlink_cmd_set_ssid *)&qs->cmd_buff[0];
		ret = qs->ops->set_ssid(qs->ops_ptr, le_to_host32(data->bss_qid), &data->ssid[0],
			qs->cmd_size - hapr_offsetof(struct qlink_cmd_set_ssid, ssid) - 1);
		break;
	}
	case QLINK_CMD_SET_80211_PARAM: {
		struct qlink_cmd_set_80211_param *data =
			(struct qlink_cmd_set_80211_param *)&qs->cmd_buff[0];
		ret = qs->ops->set_80211_param(qs->ops_ptr, le_to_host32(data->bss_qid),
			le_to_host32(data->op), le_to_host32(data->arg));
		break;
	}
	case QLINK_CMD_SET_80211_PRIV: {
		struct qlink_cmd_set_80211_priv *data =
			(struct qlink_cmd_set_80211_priv *)&qs->cmd_buff[0];
		uint32_t data_size = qs->cmd_size -
			hapr_offsetof(struct qlink_cmd_set_80211_priv, data);
		ret = qs->ops->set_80211_priv(qs->ops_ptr, le_to_host32(data->bss_qid),
			le_to_host32(data->op), &data->data[0], data_size);
		if (ret != 0)
			break;
		qlink_server_send_reply(qs, cmd_seq, QLINK_REPLY_SET_80211_PRIV, &data->data[0],
			data_size);
		return;
	}
	case QLINK_CMD_COMMIT: {
		struct qlink_cmd_commit *data =
			(struct qlink_cmd_commit *)&qs->cmd_buff[0];
		ret = qs->ops->commit(qs->ops_ptr, le_to_host32(data->bss_qid));
		break;
	}
	case QLINK_CMD_SET_INTERWORKING: {
		struct qlink_cmd_set_interworking *data =
			(struct qlink_cmd_set_interworking *)&qs->cmd_buff[0];
		const uint8_t *hessid = NULL;
		if (memcmp(data->hessid, "\x00\x00\x00\x00\x00\x00", sizeof(data->hessid)) != 0) {
			hessid = data->hessid;
		}
		ret = qs->ops->set_interworking(qs->ops_ptr, le_to_host32(data->bss_qid),
			data->eid, data->interworking, data->access_network_type, hessid);
		break;
	}
	case QLINK_CMD_BRCM: {
		struct qlink_cmd_brcm *data =
			(struct qlink_cmd_brcm *)&qs->cmd_buff[0];
		uint32_t data_size = qs->cmd_size -
			hapr_offsetof(struct qlink_cmd_brcm, data);
		ret = qs->ops->brcm(qs->ops_ptr, le_to_host32(data->bss_qid), &data->data[0],
			data_size);
		if (ret != 0)
			break;
		qlink_server_send_reply(qs, cmd_seq, QLINK_REPLY_BRCM, &data->data[0], data_size);
		return;
	}
	case QLINK_CMD_CHECK_IF: {
		struct qlink_cmd_check_if *data =
			(struct qlink_cmd_check_if *)&qs->cmd_buff[0];
		ret = qs->ops->check_if(qs->ops_ptr, le_to_host32(data->bss_qid));
		break;
	}
	case QLINK_CMD_SET_CHANNEL: {
		struct qlink_cmd_set_channel *data =
			(struct qlink_cmd_set_channel *)&qs->cmd_buff[0];
		ret = qs->ops->set_channel(qs->ops_ptr, le_to_host32(data->bss_qid),
			le_to_host32(data->channel));
		break;
	}
	case QLINK_CMD_GET_SSID: {
		struct qlink_cmd_get_ssid *data =
			(struct qlink_cmd_get_ssid *)&qs->cmd_buff[0];
		int len = le_to_host32(data->len);
		uint8_t *buf;

		buf = malloc(len);

		if (!buf) {
			ret = -ENOMEM;
			break;
		}

		ret = qs->ops->get_ssid(qs->ops_ptr, le_to_host32(data->bss_qid), buf, &len);

		if (ret != 0) {
			free(buf);
			break;
		}

		qlink_server_send_reply(qs, cmd_seq, QLINK_REPLY_GET_SSID, buf, len);
		free(buf);
		return;
	}
	case QLINK_CMD_SEND_ACTION: {
		struct qlink_cmd_send_action *data =
			(struct qlink_cmd_send_action *)&qs->cmd_buff[0];
		ret = qs->ops->send_action(qs->ops_ptr, le_to_host32(data->bss_qid), &data->data[0],
			qs->cmd_size - hapr_offsetof(struct qlink_cmd_send_action, data));
		break;
	}
	case QLINK_CMD_INIT_CTRL: {
		struct qlink_cmd_init_ctrl *data =
			(struct qlink_cmd_init_ctrl *)&qs->cmd_buff[0];
		ret = qs->ops->init_ctrl(qs->ops_ptr, data->ctrl_dir, data->ctrl_name);
		break;
	}
	case QLINK_CMD_SEND_CTRL: {
		struct qlink_cmd_send_ctrl *data =
			(struct qlink_cmd_send_ctrl *)&qs->cmd_buff[0];
		ret = qs->ops->send_ctrl(qs->ops_ptr, data->addr, (char *)&data->data[0],
			qs->cmd_size - hapr_offsetof(struct qlink_cmd_send_ctrl, data));
		break;
	}
	case QLINK_CMD_VAP_ADD: {
		struct qlink_cmd_vap_add *data =
			(struct qlink_cmd_vap_add *)&qs->cmd_buff[0];
		struct qlink_reply_vap_add reply;
		ret = qs->ops->vap_add(qs->ops_ptr, le_to_host32(data->qid), data->bssid,
			data->ifname, data->brname, reply.own_addr);
		if (ret != 0)
			break;
		qlink_server_send_reply(qs, cmd_seq, QLINK_REPLY_VAP_ADD, &reply, sizeof(reply));
		return;
	}
	case QLINK_CMD_VLAN_SET_STA: {
		struct qlink_cmd_vlan_set_sta *data =
			(struct qlink_cmd_vlan_set_sta *)&qs->cmd_buff[0];
		ret = qs->ops->vlan_set_sta(qs->ops_ptr, data->addr, le_to_host32(data->vlan_id));
		break;
	}
	case QLINK_CMD_VLAN_SET_DYN: {
		struct qlink_cmd_vlan_set_dyn *data =
			(struct qlink_cmd_vlan_set_dyn *)&qs->cmd_buff[0];
		ret = qs->ops->vlan_set_dyn(qs->ops_ptr, data->ifname, data->enable);
		break;
	}
	case QLINK_CMD_VLAN_SET_GROUP: {
		struct qlink_cmd_vlan_set_group *data =
			(struct qlink_cmd_vlan_set_group *)&qs->cmd_buff[0];
		ret = qs->ops->vlan_set_group(qs->ops_ptr, data->ifname,
			le_to_host32(data->vlan_id), data->enable);
		break;
	}
	case QLINK_CMD_READY: {
		qs->ops->ready(qs->ops_ptr);
		ret = 0;
		break;
	}
	case QLINK_CMD_SET_QOS_MAP: {
		struct qlink_cmd_set_qos_map *data =
			(struct qlink_cmd_set_qos_map *)&qs->cmd_buff[0];
		ret = qs->ops->set_qos_map(qs->ops_ptr, le_to_host32(data->bss_qid), &data->qos_map[0]);
		break;
	}
	default:
		HAPR_LOG(ERROR, "unhandled command received: %d, protocol error",
			(int)le_to_host16(qs->cmd_type));
		return;
	}

	rs.status = host_to_le32(ret);

	qlink_server_send_reply(qs, cmd_seq, QLINK_REPLY_STATUS, &rs, sizeof(rs));
}

static void qlink_server_process_ack(struct qlink_server *qs, struct qlink_header *hdr)
{
	if (le_to_host16(hdr->type) == QLINK_ACK_CMD) {
		pthread_mutex_lock(&qs->m);
		if (qs->cmd_ack_seq == 0) {
			/*
			 * we're not waiting for ack, ignore.
			 */
			pthread_mutex_unlock(&qs->m);
			return;
		}

		if ((qs->cmd_ack_seq != le_to_host32(hdr->seq)) ||
			(qs->cmd_ack_frag != le_to_host32(hdr->frag))) {
			/*
			 * ack for different fragment.
			 */
			pthread_mutex_unlock(&qs->m);
			return;
		}

		/*
		 * mark as acked.
		 */
		qs->cmd_ack_seq = 0;
		qs->cmd_ack_frag = 0;
		pthread_mutex_unlock(&qs->m);

		pthread_cond_signal(&qs->c);
	} else {
		if (qs->reply_ack_seq == 0) {
			/*
			 * we're not waiting for ack, ignore.
			 */
			return;
		}

		if ((qs->reply_ack_seq != le_to_host32(hdr->seq)) ||
			(qs->reply_ack_frag != le_to_host32(hdr->frag))) {
			/*
			 * ack for different fragment.
			 */
			return;
		}

		/*
		 * mark as acked.
		 */
		qs->reply_ack_seq = 0;
		qs->reply_ack_frag = 0;
	}
}

static void qlink_server_process_cmd(struct qlink_server *qs, struct qlink_header *hdr, uint32_t len,
	const uint8_t* addr)
{
	if ((qs->cmd_seq == 0) || (qs->cmd_seq != hdr->seq)) {
		if (le_to_host32(hdr->frag) != 0) {
			return;
		}

		if ((le_to_host16(hdr->type) != QLINK_CMD_HANDSHAKE) &&
			!qlink_seq_after(le_to_host32(hdr->seq), qs->in_seq)) {
			HAPR_LOG(WARN, "old qlink cmd (seq = %u, type = %d) in_seq = %u, ignoring",
				le_to_host32(hdr->seq), (int)le_to_host16(hdr->type),
				qs->in_seq);
			return;
		}

		if (qs->cmd_is_complete) {
			HAPR_LOG(WARN, "replacing qlink cmd (seq = %u, type = %d) with (seq = %u, type = %d)",
				le_to_host32(qs->cmd_seq), (int)le_to_host16(qs->cmd_type),
				le_to_host32(hdr->seq), (int)le_to_host16(hdr->type));
		}

		qs->cmd_seq = hdr->seq;
		qs->cmd_type = hdr->type;
		qs->cmd_frag = 0;
		qs->cmd_size = 0;
		qs->cmd_is_complete = 0;
	}

	if ((qs->cmd_type != hdr->type) || (qs->cmd_frag != le_to_host32(hdr->frag))) {
		HAPR_LOG(WARN, "out of order qlink cmd fragment: seq = %u, type = %d, frag = %u, is_last = %d, size = %d",
			le_to_host32(hdr->seq), (int)le_to_host16(hdr->type),
			le_to_host32(hdr->frag), hdr->is_last, len);
		return;
	}

	if ((qs->cmd_size + len) > sizeof(qs->cmd_buff)) {
		HAPR_LOG(ERROR, "qlink cmd too large: seq = %u, type = %d, frag = %u, is_last = %d, size = %d",
			le_to_host32(hdr->seq), (int)le_to_host16(hdr->type),
			le_to_host32(hdr->frag), hdr->is_last, len);
		qs->cmd_seq = 0;
		return;
	}

	memcpy(&qs->cmd_buff[qs->cmd_size], hdr + 1, len);

	++qs->cmd_frag;
	qs->cmd_size += len;

	qs->cmd_is_complete = hdr->is_last;

	if (qs->cmd_is_complete) {
		memcpy(qs->cmd_addr, addr, sizeof(qs->cmd_addr));
		qs->in_seq = le_to_host32(hdr->seq);
	}
}

static void qlink_server_process_reply(struct qlink_server *qs, struct qlink_header *hdr,
	uint32_t len)
{
	pthread_mutex_lock(&qs->m);

	if ((qs->reply_seq == 0) || (host_to_le32(qs->reply_seq) != hdr->seq)) {
		pthread_mutex_unlock(&qs->m);
		HAPR_LOG(ERROR, "unsolicited qlink reply fragment: seq = %u, type = %d, frag = %u, is_last = %d, size = %d",
			le_to_host32(hdr->seq), (int)le_to_host16(hdr->type),
			le_to_host32(hdr->frag), hdr->is_last, len);
		return;
	}

	if (qs->reply_frag == 0) {
		qs->reply_type = hdr->type;
		qs->reply_size = 0;
	}

	if ((qs->reply_type != hdr->type) || (qs->reply_frag != le_to_host32(hdr->frag))) {
		HAPR_LOG(WARN, "out of order qlink reply fragment: seq = %u, type = %d, frag = %u, is_last = %d, size = %d",
			le_to_host32(hdr->seq), (int)le_to_host16(hdr->type),
			le_to_host32(hdr->frag), hdr->is_last, len);
		pthread_mutex_unlock(&qs->m);
		return;
	}

	if ((qs->reply_size + len) > sizeof(qs->reply_buff)) {
		HAPR_LOG(ERROR, "qlink reply too large: seq = %u, type = %d, frag = %u, is_last = %d, size = %d",
			le_to_host32(hdr->seq), (int)le_to_host16(hdr->type),
			le_to_host32(hdr->frag), hdr->is_last, len);
		qs->reply_frag = 0;
		pthread_mutex_unlock(&qs->m);
		return;
	}

	memcpy(&qs->reply_buff[qs->reply_size], hdr + 1, len);

	++qs->reply_frag;
	qs->reply_size += len;

	if (hdr->is_last) {
		qs->reply_seq = 0;
	}

	pthread_mutex_unlock(&qs->m);

	if (hdr->is_last) {
		pthread_cond_signal(&qs->c);
	}
}

static void qlink_server_send_ack(struct qlink_server *qs, struct ethhdr *src_eh)
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

	memcpy(dst_eh->h_dest, src_eh->h_source, ETH_ALEN);
	memcpy(dst_eh->h_source, src_eh->h_dest, ETH_ALEN);
	dst_eh->h_proto = htons(QLINK_PROTO);

	dst_hdr->seq = src_hdr->seq;
	dst_hdr->type = host_to_le16((le_to_host16(src_hdr->type) < QLINK_REPLY_FIRST) ?
		QLINK_ACK_CMD : QLINK_ACK_REPLY);
	dst_hdr->frag = src_hdr->frag;
	dst_hdr->size = 0;
	dst_hdr->is_last = 1;

	do {
		ret = send(qs->fd, buff, sizeof(*dst_eh) + sizeof(*dst_hdr), 0);
	} while ((ret < 0) && ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)));

	if (ret < 0) {
		HAPR_LOG(ERROR, "send error: %s", strerror(errno));
	}
}

static int qlink_server_process_rx(struct qlink_server *qs)
{
	uint8_t buff[ETH_FRAME_LEN];
	int ret;
	struct sockaddr_ll ll;
	socklen_t fromlen;
	struct qlink_header *hdr;
	uint32_t sz;
	int type;

	memset(&ll, 0, sizeof(ll));
	fromlen = sizeof(ll);
	do {
		ret = recvfrom(qs->fd, buff, sizeof(buff), 0, (struct sockaddr *)&ll, &fromlen);
	} while ((ret < 0) && (errno == EINTR));

	if (ret < 0) {
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
			return 0;
		}
		HAPR_LOG(ERROR, "qlink recvfrom error: %s", strerror(errno));
		return -1;
	}

	if (ll.sll_pkttype != PACKET_HOST) {
		return 0;
	}

	if (ret < (sizeof(struct ethhdr) + sizeof(*hdr))) {
		HAPR_LOG(ERROR, "qlink header too small: %d bytes", ret);
		return 0;
	}

	hdr = (struct qlink_header *)(&buff[0] + sizeof(struct ethhdr));

	sz = le_to_host16(hdr->size);

	if (sz > (ret - (sizeof(struct ethhdr) + sizeof(*hdr)))) {
		HAPR_LOG(ERROR, "qlink invalid size in header: %u bytes", sz);
		return 0;
	}

	type = le_to_host16(hdr->type);

	if (type >= QLINK_CMD_FIRST) {
		qlink_server_send_ack(qs, (struct ethhdr *)&buff[0]);
	}

	if (type < QLINK_CMD_FIRST) {
		qlink_server_process_ack(qs, hdr);
	} else if (type < QLINK_REPLY_FIRST) {
		qlink_server_process_cmd(qs, hdr, sz, ll.sll_addr);
	} else {
		qlink_server_process_reply(qs, hdr, sz);
	}

	return 0;
}

static void qlink_server_rx(int sock, void *user_data)
{
	struct qlink_server *qs = user_data;

	qlink_server_process_rx(qs);

	qlink_server_dispatch_cmd(qs);
}

static int qlink_server_send_cmd(struct qlink_server *qs, uint32_t seq, qlink_cmd cmd, const void *data,
	uint32_t len)
{
	uint8_t buff[ETH_DATA_LEN];
	struct ethhdr eh;
	struct qlink_header hdr;
	struct timespec ts;
	struct timeval tv_start, tv_cur, tv;
	uint32_t passed_ms = 0, last_send_ms = 0;
	uint32_t frag = 0;
	uint32_t num_send = 0;
	int send_complete = 0, ret = 0, reply_started = 0;
	int resend = 0;

	if (memcmp(qs->client_addr, "\x00\x00\x00\x00\x00\x00", sizeof(qs->client_addr)) == 0) {
		HAPR_LOG(ERROR, "no client connected, not sending");
		return -1;
	}

	memcpy(eh.h_dest, qs->client_addr, ETH_ALEN);
	memcpy(eh.h_source, qs->own_addr, ETH_ALEN);
	eh.h_proto = htons(QLINK_PROTO);

	hdr.seq = host_to_le32(seq);
	hdr.type = host_to_le16(cmd);

	pthread_mutex_lock(&qs->m);

	qs->cmd_ack_seq = 0;
	qs->cmd_ack_frag = 0;

	qs->reply_frag = 0;
	qs->reply_seq = 0;

	gettimeofday(&tv_start, NULL);

	while (passed_ms <= QC_TIMEOUT) {
		uint32_t remain_ms = QC_TIMEOUT - passed_ms;

		if (memcmp(qs->client_addr, "\x00\x00\x00\x00\x00\x00", sizeof(qs->client_addr)) == 0) {
			pthread_mutex_unlock(&qs->m);
			return -1;
		}

		if (!send_complete && (qs->cmd_ack_seq == 0) && (len == 0) && (frag > 0)) {
			send_complete = 1;
		}

		if (!send_complete) {
			if (qs->cmd_ack_seq == 0) {
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

				qs->cmd_ack_seq = seq;
				qs->cmd_ack_frag = frag;

				++frag;
				data += num_send;

				last_send_ms = passed_ms - QC_ACK_TIMEOUT;
				resend = 0;
			}

			if ((passed_ms - last_send_ms) >= QC_ACK_TIMEOUT) {
				if (resend) {
					HAPR_LOG(DEBUG, "resending (type = %d, seq = %u, frag = %u)",
						cmd, seq, frag - 1);
				}

				if ((len == 0) && !reply_started) {
					qs->reply_seq = host_to_le32(seq);
					reply_started = 1;
				}

				do {
					ret = send(qs->fd, buff,
						sizeof(eh) + sizeof(hdr) + num_send, 0);
				} while ((ret < 0) && ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)));

				if (ret < 0) {
					HAPR_LOG(ERROR, "send error: %s", strerror(errno));

					pthread_mutex_unlock(&qs->m);

					goto fail;
				}

				last_send_ms = passed_ms;
				resend = 1;
			}

			if (remain_ms > QC_ACK_TIMEOUT)
				remain_ms = QC_ACK_TIMEOUT;
		}

		if ((qs->reply_seq == 0) && send_complete) {
			pthread_mutex_unlock(&qs->m);
			return 0;
		}

		ts.tv_sec = remain_ms / 1000;
		ts.tv_nsec = (uint64_t)(remain_ms % 1000) * 1000000;

		ret = pthread_cond_timedwait(&qs->c, &qs->m, &ts);

		if ((ret != 0) && (ret != ETIMEDOUT)) {
			qs->cmd_ack_seq = 0;
			qs->reply_seq = 0;

			pthread_mutex_unlock(&qs->m);

			HAPR_LOG(ERROR, "cond_timedwait error");

			goto fail;
		}

		gettimeofday(&tv_cur, NULL);

		timersub(&tv_cur, &tv_start, &tv);

		passed_ms = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
	}

	qs->cmd_ack_seq = 0;
	qs->reply_seq = 0;

	pthread_mutex_unlock(&qs->m);

	HAPR_LOG(ERROR, "timeout during qlink send cmd (seq = %u, type = %d)", seq, cmd);

fail:
	qlink_server_set_client_addr(qs, NULL);

	hapr_eloop_register_timeout(qs->eloop, &qlink_server_force_disconnect, qs, 0);

	return -1;
}

static int qlink_server_send_sync(struct qlink_server *qs, qlink_cmd cmd, const void *cmd_data,
	uint32_t cmd_len, qlink_reply reply, uint32_t timeout_ms, void **reply_data,
	uint32_t *reply_size)
{
	int ret;

	++qs->next_out_seq;
	if (qs->next_out_seq == 0)
		++qs->next_out_seq;

	ret = qlink_server_send_cmd(qs, qs->next_out_seq, cmd, cmd_data, cmd_len);

	if (ret != 0) {
		return ret;
	}

	if (qs->reply_type == host_to_le16(QLINK_REPLY_STATUS)) {
		struct qlink_reply_status *rs = (struct qlink_reply_status *)&qs->reply_buff[0];

		ret = le_to_host32(rs->status);

		if (ret > 0)
			ret = 0;

		if ((reply != QLINK_REPLY_STATUS) && (ret == 0)) {
			HAPR_LOG(ERROR, "reply status 0 not expected, protocol error");
			return -1;
		}

		return ret;
	}

	if (qs->reply_type != host_to_le16(reply)) {
		HAPR_LOG(ERROR, "bad reply type %d for cmd (seq = %u, type = %d), protocol error",
			le_to_host16(qs->reply_type), qs->next_out_seq, cmd);
		return -1;
	}

	if (reply_data)
		*reply_data = &qs->reply_buff[0];

	if (reply_size)
		*reply_size = qs->reply_size;

	return 0;
}

struct qlink_server *qlink_server_create(struct hapr_eloop *eloop, struct qlink_server_ops *ops,
	void *ops_ptr, const char *ifname)
{
	struct qlink_server *qs;
	struct ifreq ifr;
	struct sockaddr_ll ll;

	qs = malloc(sizeof(*qs));

	if (qs == NULL) {
		HAPR_LOG(ERROR, "cannot allocate qlink server, no mem");
		goto fail_alloc;
	}

	memset(qs, 0, sizeof(*qs));

	if (pthread_mutex_init(&qs->m, NULL) != 0) {
		HAPR_LOG(ERROR, "cannot create mutex");
		goto fail_mtx;
	}

	if (pthread_cond_init(&qs->c, NULL) != 0) {
		HAPR_LOG(ERROR, "cannot create condvar");
		goto fail_cond;
	}

	qs->ops = ops;
	qs->ops_ptr = ops_ptr;

	qs->eloop = eloop;

	qs->fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (qs->fd < 0) {
		HAPR_LOG(ERROR, "socket(PF_PACKET): %s", strerror(errno));
		goto fail_socket;
	}

	if (qlink_server_set_filter(qs->fd) < 0) {
		goto fail_filter;
	}

	if (fcntl(qs->fd, F_SETFL, O_NONBLOCK) != 0)
		goto fail_filter;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);

	if (ioctl(qs->fd, SIOCGIFINDEX, &ifr) < 0) {
		HAPR_LOG(ERROR, "ioctl[SIOCGIFINDEX]: %s", strerror(errno));
		goto fail_filter;
	}
	qs->ifindex = ifr.ifr_ifindex;

	memset(&ll, 0, sizeof(ll));
	ll.sll_family = PF_PACKET;
	ll.sll_ifindex = ifr.ifr_ifindex;
	ll.sll_protocol = htons(ETH_P_ALL);

	if (bind(qs->fd, (struct sockaddr *)&ll, sizeof(ll)) < 0) {
		HAPR_LOG(ERROR, "bind[PF_PACKET]: %s", strerror(errno));
		goto fail_filter;
	}

	if (ioctl(qs->fd, SIOCGIFHWADDR, &ifr) < 0) {
		HAPR_LOG(ERROR, "ioctl[SIOCGIFHWADDR]: %s", strerror(errno));
		goto fail_filter;
	}

	memcpy(qs->own_addr, ifr.ifr_hwaddr.sa_data, sizeof(qs->own_addr));

	HAPR_LOG(INFO, "using HWADDR %s", ether_ntoa(qs->own_addr));

	hapr_eloop_register_read_sock(qs->eloop, qs->fd, &qlink_server_rx, qs);

	return qs;

fail_filter:
	close(qs->fd);
fail_socket:
	pthread_cond_destroy(&qs->c);
fail_cond:
	pthread_mutex_destroy(&qs->m);
fail_mtx:
	free(qs);
fail_alloc:

	return NULL;
}

void qlink_server_destroy(struct qlink_server *qs)
{
	hapr_eloop_cancel_timeout(qs->eloop, &qlink_server_dispatch_cmd_timeout, qs);
	hapr_eloop_cancel_timeout(qs->eloop, &qlink_server_force_disconnect, qs);
	hapr_eloop_unregister_read_sock(qs->eloop, qs->fd);
	close(qs->fd);
	pthread_cond_destroy(&qs->c);
	pthread_mutex_destroy(&qs->m);
	free(qs);
}

void qlink_server_ping(struct qlink_server *qs)
{
	uint8_t buff[ETH_FRAME_LEN];
	struct ethhdr *eh = (struct ethhdr *)&buff[0];
	struct qlink_header *hdr = (struct qlink_header *)(eh + 1);
	int ret;

	if (memcmp(qs->client_addr, "\x00\x00\x00\x00\x00\x00", sizeof(qs->client_addr)) == 0) {
		HAPR_LOG(ERROR, "no client connected, not sending");
		return;
	}

	++qs->next_out_seq;
	if (qs->next_out_seq == 0)
		++qs->next_out_seq;

	memcpy(eh->h_dest, qs->client_addr, ETH_ALEN);
	memcpy(eh->h_source, qs->own_addr, ETH_ALEN);
	eh->h_proto = htons(QLINK_PROTO);

	hdr->seq = host_to_le32(qs->next_out_seq);
	hdr->type = host_to_le16(QLINK_CMD_PING);
	hdr->frag = 0;
	hdr->size = 0;
	hdr->is_last = 1;

	do {
		ret = send(qs->fd, buff, sizeof(*eh) + sizeof(*hdr), 0);
	} while ((ret < 0) && ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)));

	if (ret < 0) {
		HAPR_LOG(ERROR, "send error: %s", strerror(errno));
	}
}

void qlink_server_handshake(struct qlink_server *qs, const uint8_t *guid,
	uint32_t ping_timeout_ms)
{
	uint8_t buff[ETH_FRAME_LEN];
	struct ethhdr *eh = (struct ethhdr *)&buff[0];
	struct qlink_header *hdr = (struct qlink_header *)(eh + 1);
	struct qlink_cmd_handshake *cmd = (struct qlink_cmd_handshake *)(hdr + 1);
	int ret;

	if (memcmp(qs->client_addr, "\x00\x00\x00\x00\x00\x00", sizeof(qs->client_addr)) == 0) {
		HAPR_LOG(ERROR, "no client connected, not sending");
		return;
	}

	++qs->next_out_seq;
	if (qs->next_out_seq == 0)
		++qs->next_out_seq;

	memcpy(eh->h_dest, qs->client_addr, ETH_ALEN);
	memcpy(eh->h_source, qs->own_addr, ETH_ALEN);
	eh->h_proto = htons(QLINK_PROTO);

	hdr->seq = host_to_le32(qs->next_out_seq);
	hdr->type = host_to_le16(QLINK_CMD_HANDSHAKE);
	hdr->frag = 0;
	hdr->size = host_to_le16(sizeof(*cmd));
	hdr->is_last = 1;

	memcpy(cmd->guid, guid, sizeof(cmd->guid));
	cmd->version = host_to_le32(QLINK_VERSION);
	cmd->ping_timeout = host_to_le32(ping_timeout_ms);
	cmd->flags = 0;

	do {
		ret = send(qs->fd, buff, sizeof(*eh) + sizeof(*hdr) + sizeof(*cmd), 0);
	} while ((ret < 0) && ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)));

	if (ret < 0) {
		HAPR_LOG(ERROR, "send error: %s", strerror(errno));
	}
}

int qlink_server_disconnect(struct qlink_server *qs)
{
	return qlink_server_send_sync(qs, QLINK_CMD_DISCONNECT, NULL, 0,
		QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);
}

void qlink_server_set_client_addr(struct qlink_server *qs, const uint8_t *client_addr)
{
	if (client_addr) {
		memcpy(qs->client_addr, client_addr, sizeof(qs->client_addr));
	} else {
		/*
		 * marking client disconnected, if there's an outstanding request
		 * then finish it ASAP without waiting for timeout.
		 */

		memcpy(qs->client_addr, "\x00\x00\x00\x00\x00\x00", sizeof(qs->client_addr));

		pthread_cond_signal(&qs->c);
	}
}

int qlink_server_update_security_config(struct qlink_server *qs, const char *security_config)
{
	return qlink_server_send_sync(qs, QLINK_CMD_UPDATE_SECURITY_CONFIG, security_config,
		strlen(security_config) + 1, QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);
}

int qlink_server_recv_packet(struct qlink_server *qs, uint32_t bss_qid, uint16_t protocol,
	const uint8_t *src_addr, const uint8_t *buf, size_t len)
{
	struct qlink_cmd_recv_packet *cmd;
	int ret, cmd_size = hapr_offsetof(struct qlink_cmd_recv_packet, data) + len;

	cmd = malloc(cmd_size);

	if (cmd == NULL) {
		HAPR_LOG(ERROR, "cannot allocate cmd_recv_packet, no mem");
		return -1;
	}

	cmd->bss_qid = host_to_le32(bss_qid);
	cmd->protocol = host_to_le16(protocol);
	memcpy(cmd->src_addr, src_addr, sizeof(cmd->src_addr));
	memcpy(&cmd->data[0], buf, len);

	ret = qlink_server_send_sync(qs, QLINK_CMD_RECV_PACKET, cmd, cmd_size,
		QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);

	free(cmd);

	return ret;
}

int qlink_server_recv_iwevent(struct qlink_server *qs, uint32_t bss_qid, uint16_t wcmd,
	const char *data, int len)
{
	struct qlink_cmd_recv_iwevent *cmd;
	int ret, cmd_size = hapr_offsetof(struct qlink_cmd_recv_iwevent, data) + len;

	cmd = malloc(cmd_size);

	if (cmd == NULL) {
		HAPR_LOG(ERROR, "cannot allocate cmd_recv_iwevent, no mem");
		return -1;
	}

	cmd->bss_qid = host_to_le32(bss_qid);
	cmd->cmd = host_to_le16(wcmd);
	memcpy(&cmd->data[0], data, len);

	ret = qlink_server_send_sync(qs, QLINK_CMD_RECV_IWEVENT, cmd, cmd_size,
		QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);

	free(cmd);

	return ret;
}

int qlink_server_recv_ctrl(struct qlink_server *qs, const char *addr, char *data, int len)
{
	struct qlink_cmd_recv_ctrl *cmd;
	int ret, cmd_size = hapr_offsetof(struct qlink_cmd_recv_ctrl, data) + len;

	cmd = malloc(cmd_size);

	if (cmd == NULL) {
		HAPR_LOG(ERROR, "cannot allocate cmd_recv_ctrl, no mem");
		return -1;
	}

	memset(cmd, 0, cmd_size);
	strncpy(cmd->addr, addr, sizeof(cmd->addr) - 1);
	memcpy(&cmd->data[0], data, len);

	ret = qlink_server_send_sync(qs, QLINK_CMD_RECV_CTRL, cmd, cmd_size,
		QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);

	free(cmd);

	return ret;
}

int qlink_server_invalidate_ctrl(struct qlink_server *qs, const char *addr)
{
	struct qlink_cmd_invalidate_ctrl cmd;

	memset(&cmd, 0, sizeof(cmd));
	strncpy(cmd.addr, addr, sizeof(cmd.addr) - 1);

	return qlink_server_send_sync(qs, QLINK_CMD_INVALIDATE_CTRL, &cmd, sizeof(cmd),
		QLINK_REPLY_STATUS, QC_TIMEOUT, NULL, NULL);
}
