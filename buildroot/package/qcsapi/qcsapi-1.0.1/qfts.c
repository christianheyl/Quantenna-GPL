/*
 * Copyright (c) 2015 Quantenna Communications, Inc.
 * All rights reserved.
 */
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <net/ethernet.h>
#include <linux/limits.h>
#include <linux/if.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <zlib.h>
#include "qcsapi_rpc_common/common/rpc_raw.h"
#include "uboot_header.h"

#define IMAGE_FILE_DIR		"/tmp"
#define QFTS_READ_TIMEOUT	(1000 * 2)
#define QFTS_RECV_RETRY_LIMIT	5
#define QFTS_CONFIG_FILE	"/mnt/jffs2/qfts.conf"

typedef enum {
	IMG_INIT = 0,
	IMG_NOT_MKIMAGE,
	IMG_CRC_ERROR,
	IMG_CALC_IN_PROGRESS
} img_check_state_t;

static int sock_fd = -1;
static int image_fd = -1;
static image_header_t img_hdr;
static img_check_state_t img_state = IMG_INIT;

static void check_img_and_calc_crc(char *buf, ssize_t size)
{
	static int32_t total_bytes = 0;
	static int32_t img_len = 0;
	static uint32_t crc = 0;

	if (!size)
		return;

	if ((img_state == IMG_INIT) && (size >= sizeof(image_header_t))) {
		memcpy(&img_hdr, buf, sizeof(image_header_t));
		img_len = ntohl(img_hdr.ih_size);

		if (ntohl(img_hdr.ih_magic) == IH_MAGIC) {
			uint32_t hcrc = ntohl(img_hdr.ih_hcrc);
			uint32_t cal_crc;

			img_hdr.ih_hcrc = 0;
			cal_crc = crc32(0, (Bytef *)&img_hdr, sizeof(img_hdr));
			if (cal_crc != hcrc) {
				img_state = IMG_CRC_ERROR;
			} else {
				img_state = IMG_CALC_IN_PROGRESS;
			}
		} else {
			img_state = IMG_NOT_MKIMAGE;
		}
	} else {
		if (total_bytes > image_get_header_size()) {
			crc = crc32(crc, (Bytef *)buf, MIN(size, img_len));
			img_len -= MIN(size, img_len);
		}
	}

	if ((total_bytes < image_get_header_size()) &&
		((total_bytes + size) > image_get_header_size())) {
		crc = crc32(0, (Bytef *)(buf + image_get_header_size() - total_bytes),
				total_bytes + size - image_get_header_size());
		img_len -= total_bytes + size - image_get_header_size();
	}

	total_bytes += size;
	if (img_len <= 0) {
		if (crc != ntohl(img_hdr.ih_dcrc)) {
			img_state = IMG_CRC_ERROR;
		}
	}
}

static int get_bind_if_from_file(char *if_name, const int size)
{
	int fd;
	ssize_t bytes_read;
	char *nl;

	fd = open(QFTS_CONFIG_FILE, O_RDONLY);

	if (fd < 0)
		return -1;

	bytes_read = read(fd, if_name, size - 1);

	close(fd);
	if (bytes_read < 0) {
		if_name[0] = '\0';
		return -1;
	}
	if_name[bytes_read] = '\0';

	nl = strchr(if_name, '\n');
	if (nl) {
		*nl = '\0';
	}

	return 0;
}

static void qfts_clean(void)
{
	if (sock_fd >= 0)
		close(sock_fd);
	if (image_fd >= 0)
		close(image_fd);
}

static void dump_data_to_file(int fd, const void *buf, size_t count)
{
	ssize_t written = 0;
	const size_t total = count;

	while ((written < 0 && errno == EINTR) || (count > 0)) {
		written = write(fd, buf + total - count, count);
		count -= written;
	}
}

int main(void)
{
	struct qftp_raw_ethpkt recv_buf __attribute__ ((aligned(4)));
	struct qftp_raw_ethpkt send_buf __attribute__ ((aligned(4)));
	struct sockaddr_ll dst_addr;
	ssize_t frame_len;
	struct sockaddr_ll lladdr;
	socklen_t addrlen = sizeof(lladdr);
	ssize_t	bytes_recv = 0;
	ssize_t sent_bytes;
	struct qftp_connect_pkt *connect_payload = (struct qftp_connect_pkt *)&recv_buf.payload;
	char image_pathname[PATH_MAX];
	char if_name[IFNAMSIZ];
	char *img_fname;
	int image_size = 0;
	int retry_count = 0;
	int op_failed = 0;

	sock_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sock_fd < 0) {
		qfts_clean();
		return -1;
	}

	/* Allow receiving only QFTP packets */
	if (qrpc_set_prot_filter(sock_fd, QFTP_RAW_SOCK_PROT) < 0) {
		qfts_clean();
		return -1;
	}

	if (get_bind_if_from_file(if_name, sizeof(if_name)) < 0) {
		/* Set bind interface to pcie0 if interface configuration file is missing */
		strcpy(if_name, "pcie0");
	}

	if (qrpc_raw_bind(sock_fd, if_name, ETH_P_ALL) < 0) {
		strcpy(if_name, "br0");
		if (qrpc_raw_bind(sock_fd, if_name, ETH_P_ALL) < 0) {
			qfts_clean();
			return -1;
		}
	}

	memset(&lladdr, 0, sizeof(lladdr));
	/* Reading CONNECT frame */
	do {
		if (!qrpc_raw_read_timeout(sock_fd, QFTS_READ_TIMEOUT)) {
			do {
				bytes_recv = recvfrom(sock_fd, &recv_buf, sizeof(recv_buf), 0,
							(struct sockaddr *)&lladdr, &addrlen);
			} while (bytes_recv < 0 && errno == EINTR);
		} else if (++retry_count > QFTS_RECV_RETRY_LIMIT) {
			break;
		}
	} while (lladdr.sll_pkttype != PACKET_HOST);

	if (retry_count > QFTS_RECV_RETRY_LIMIT ||
			connect_payload->sub_type != QFTP_FRAME_TYPE_CONNECT) {
		qfts_clean();
		return -1;
	}

	image_size = connect_payload->image_size;
	strcpy(image_pathname, IMAGE_FILE_DIR "/");
	img_fname = basename(connect_payload->image_name);
	if ((sizeof(image_pathname) - strlen(image_pathname) - 1) < strlen(img_fname)) {
		qfts_clean();
		return -1;
	}
	strcat(image_pathname, img_fname);
	image_fd = open(image_pathname, O_RDWR | O_CREAT | O_TRUNC, 0600);

	if (image_fd < 0) {
		qfts_clean();
		return -1;
	}

	/* Reply ACK */
	if (qrpc_clnt_raw_config_dst(sock_fd, if_name,
					&dst_addr, lladdr.sll_addr,
						&send_buf.hdr, QFTP_RAW_SOCK_PROT) < 0) {
		qfts_clean();
		return -1;
	}

	struct qftp_ack_nack_pkt *reply_payload = (struct qftp_ack_nack_pkt *)&send_buf.payload;
	struct qftp_data_pkt *data_payload = (struct qftp_data_pkt *)&recv_buf.payload;

	reply_payload->sub_type = QFTP_FRAME_TYPE_ACK;
	reply_payload->seq = connect_payload->seq;

	frame_len = QFTP_ACK_NACK_FRAME_LEN;
	do {
		sent_bytes = sendto(sock_fd, &send_buf, frame_len, 0,
					(struct sockaddr *)&dst_addr, sizeof(dst_addr));
	} while (sent_bytes < 0 && errno == EINTR);

	if (sent_bytes < 0) {
		qfts_clean();
		return -1;
	}

	/* Receiving image file here */
	do {
		retry_count = 0;
		bytes_recv = 0;
		do {
			if (!qrpc_raw_read_timeout(sock_fd, QFTS_READ_TIMEOUT)) {
				do {
					bytes_recv = recvfrom(sock_fd, &recv_buf, sizeof(recv_buf),
								0, (struct sockaddr *)&lladdr,
								&addrlen);
				} while (bytes_recv < 0 && errno == EINTR);
			} else if (++retry_count > QFTS_RECV_RETRY_LIMIT) {
				op_failed = 1;
				break;
			}

			if (bytes_recv <= QFTP_DATA_PKT_HDR_SIZE) {
				op_failed = 1;
				break;
			}
		} while (lladdr.sll_pkttype != PACKET_HOST);

		if (!op_failed && reply_payload->seq == data_payload->seq) {
			continue;
		}

		if (!op_failed && (data_payload->sub_type == QFTP_FRAME_TYPE_DATA)) {
			dump_data_to_file(image_fd, data_payload->data,
						bytes_recv - QFTP_DATA_PKT_HDR_SIZE);
			if ((img_state != IMG_NOT_MKIMAGE) && (img_state != IMG_CRC_ERROR)) {
				check_img_and_calc_crc(data_payload->data,
							bytes_recv - QFTP_DATA_PKT_HDR_SIZE);
			}
		} else {
			op_failed = 1;
			break;
		}

		if (img_state != IMG_CRC_ERROR)
			reply_payload->sub_type = QFTP_FRAME_TYPE_ACK;
		else
			reply_payload->sub_type = QFTP_FRAME_TYPE_NACK;

		reply_payload->seq = data_payload->seq;
		frame_len = QFTP_ACK_NACK_FRAME_LEN;

		do {
			sent_bytes = sendto(sock_fd, &send_buf, frame_len, 0,
						 (struct sockaddr *)&dst_addr, sizeof(dst_addr));
		} while (sent_bytes < 0 && errno == EINTR);

		if (sent_bytes < 0) {
			op_failed = 1;
			break;
		}

		image_size -= (bytes_recv - QFTP_DATA_PKT_HDR_SIZE);

	} while (image_size > 0);

	sync();
	qfts_clean();

	if (op_failed) {
		unlink(image_pathname);
	}

	return 0;
}
