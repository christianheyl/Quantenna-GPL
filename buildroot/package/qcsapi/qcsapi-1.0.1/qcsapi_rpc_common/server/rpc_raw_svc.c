/*
 * Copyright (C) 1987, Sun Microsystems, Inc.
 * Copyright (C) 2014 Quantenna Communications Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *     * Neither the name of Sun Microsystems, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *   FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *   COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 *   INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *   GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <qcsapi_rpc_common/common/rpc_raw.h>

static bool_t qrpc_svc_raw_recv(SVCXPRT *xprt, struct rpc_msg *msg);
static enum xprt_stat qrpc_svc_raw_stat(SVCXPRT *xprt);
static bool_t qrpc_svc_raw_getargs(SVCXPRT *xprt, xdrproc_t xdr_args, caddr_t args_ptr);
static bool_t qrpc_svc_raw_reply(SVCXPRT *xprt, struct rpc_msg *msg);
static bool_t qrpc_svc_raw_freeargs(SVCXPRT *xprt, xdrproc_t xdr_args, caddr_t args_ptr);
static void qrpc_svc_raw_destroy(SVCXPRT *xprt);

struct qrpc_svc_raw_priv {
	uint8_t			*inbuf;
	uint8_t			*in_pktbuf;
	uint8_t			*out_pktbuf;
	struct qrpc_frame_hdr	out_hdr;
	char			verfbody[MAX_AUTH_BYTES];
	XDR			xdrs;
	struct sockaddr_ll	dst_addr;
	u_long			xid;
	uint8_t			sess_id;
};

static const struct xp_ops qrpc_svc_raw_op = {
	qrpc_svc_raw_recv,
	qrpc_svc_raw_stat,
	qrpc_svc_raw_getargs,
	qrpc_svc_raw_reply,
	qrpc_svc_raw_freeargs,
	qrpc_svc_raw_destroy
};

static void qrpc_svc_raw_free_priv(struct qrpc_svc_raw_priv *priv)
{
	free(priv->inbuf);
	free(priv->in_pktbuf);
	free(priv->out_pktbuf);
	free(priv);
}

SVCXPRT *qrpc_svc_raw_create(int sock, const char *const bind_interface, uint8_t sess_id)
{
	SVCXPRT *xprt;
	int rawsock_fd;
	struct qrpc_svc_raw_priv *priv;

	rawsock_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (rawsock_fd < 0)
		return NULL;

	if (qrpc_set_prot_filter(rawsock_fd, QRPC_RAW_SOCK_PROT) < 0) {
		close(rawsock_fd);
		return NULL;
	}

	if (qrpc_raw_bind(rawsock_fd, bind_interface, ETH_P_ALL) < 0) {
		close(rawsock_fd);
		return NULL;
	}

	xprt = calloc(1, sizeof(*xprt));
	priv = calloc(1, sizeof(*priv));

	if (!(xprt && priv)) {
		close(rawsock_fd);
		free(xprt);
		free(priv);
		return NULL;
	}

	priv->inbuf = calloc(1, QRPC_BUFFER_LEN);
	priv->in_pktbuf = calloc(1, ETH_FRAME_LEN);
	priv->out_pktbuf = calloc(1, ETH_FRAME_LEN);
	if (!priv->inbuf || !priv->in_pktbuf || !priv->out_pktbuf) {
		close(rawsock_fd);
		free(xprt);
		qrpc_svc_raw_free_priv(priv);
		return NULL;
	}

	priv->dst_addr.sll_family = AF_PACKET;
	priv->dst_addr.sll_protocol = htons(ETH_P_OUI_EXT);
	priv->dst_addr.sll_halen = ETH_ALEN;

	priv->sess_id = sess_id;

	xprt->xp_sock = rawsock_fd;
	xprt->xp_p1 = (char *)priv->inbuf + sizeof(struct qrpc_frame_hdr);
	xprt->xp_p2 = (caddr_t) priv;
	xprt->xp_verf.oa_base = priv->verfbody;
	xprt->xp_ops = &qrpc_svc_raw_op;
	xdrmem_create(&priv->xdrs, xprt->xp_p1, QRPC_BUFFER_LEN - sizeof(struct qrpc_frame_hdr), XDR_DECODE);
	xprt_register(xprt);

	return xprt;
}

static void qrpc_svc_raw_macaddr_swap(struct ethhdr *const eh)
{
	uint32_t octet_idx = 0;
	int8_t holder;

	while (octet_idx < ETH_ALEN) {
		holder = eh->h_dest[octet_idx];
		eh->h_dest[octet_idx] = eh->h_source[octet_idx];
		eh->h_source[octet_idx] = holder;
		++octet_idx;
	}
}

static bool_t qrpc_svc_raw_recv(SVCXPRT *xprt, struct rpc_msg *msg)
{
	struct qrpc_svc_raw_priv *priv = (struct qrpc_svc_raw_priv *)xprt->xp_p2;
	XDR *xdrs = &priv->xdrs;
	struct sockaddr_ll lladdr;
	socklen_t addrlen = sizeof(lladdr);
	int ret;
	struct qrpc_frame_hdr hdr;
	uint16_t payload_done = sizeof(struct qrpc_frame_hdr);
	struct qrpc_old_frame_hdr *shdr;

	do {
		do {
			ret = recvfrom(xprt->xp_sock, priv->in_pktbuf, ETH_FRAME_LEN,
					0, (struct sockaddr *)&lladdr, &addrlen);
		} while (ret < 0 && errno == EINTR);

		if (ret < sizeof(struct ethhdr) || lladdr.sll_pkttype != PACKET_HOST) {
			return FALSE;
		}

		if (qrpc_is_old_rpc((struct ethhdr*) priv->in_pktbuf)) {
			if ((ret < sizeof(struct qrpc_old_frame_hdr)) ||
					(ret > QRPC_BUFFER_LEN)) {
				return FALSE;
			}

			shdr = (struct qrpc_old_frame_hdr *)priv->in_pktbuf;
			memcpy(&hdr, &(shdr->eth_hdr), sizeof(struct ethhdr));
			memcpy(&hdr.seq, &(shdr->seq), sizeof(hdr.seq));
			memcpy(priv->inbuf + payload_done,
					priv->in_pktbuf + sizeof(struct qrpc_old_frame_hdr),
					ret - sizeof(struct qrpc_old_frame_hdr));
			break;
		}

		if (ret < sizeof(struct qrpc_frame_hdr)
				|| ret - sizeof(struct qrpc_frame_hdr) + payload_done > QRPC_BUFFER_LEN)
			return FALSE;

		/* check session ID */
		memcpy(&hdr, priv->in_pktbuf, sizeof(struct qrpc_frame_hdr));
		if (hdr.sid != priv->sess_id)
			return FALSE;

		/* assemble the packet */
		memcpy(priv->inbuf + payload_done, priv->in_pktbuf + sizeof(struct qrpc_frame_hdr),
			ret - sizeof(struct qrpc_frame_hdr));

		payload_done += (ret - sizeof(struct qrpc_frame_hdr));

	} while(hdr.sub_type == QRPC_FRAME_TYPE_FRAG);

	memcpy(priv->inbuf, &hdr, sizeof(struct qrpc_frame_hdr));

	xdrs->x_op = XDR_DECODE;
	XDR_SETPOS(xdrs, 0);
	if (!xdr_callmsg(xdrs, msg))
		return FALSE;

	priv->dst_addr.sll_ifindex = lladdr.sll_ifindex;
	memcpy(priv->dst_addr.sll_addr, lladdr.sll_addr, ETH_ALEN);
	priv->xid = msg->rm_xid;

	return TRUE;
}

static enum xprt_stat qrpc_svc_raw_stat(SVCXPRT *xprt)
{
	return XPRT_IDLE;
}

static bool_t qrpc_svc_raw_getargs(SVCXPRT *xprt, xdrproc_t xdr_args, caddr_t args_ptr)
{
	struct qrpc_svc_raw_priv *const priv = (struct qrpc_svc_raw_priv *)xprt->xp_p2;

	return (*xdr_args) (&priv->xdrs, args_ptr);
}

static bool_t qrpc_svc_raw_reply(SVCXPRT *xprt, struct rpc_msg *msg)
{
	struct qrpc_svc_raw_priv *const priv = (struct qrpc_svc_raw_priv *)xprt->xp_p2;
	XDR *const xdrs = &priv->xdrs;
	size_t	packet_len;
	int ret;
	uint16_t pkt_nr;
	uint16_t i;
	uint16_t payload_done = 0;
	static const uint16_t payload_max = ETH_FRAME_LEN - sizeof(struct qrpc_frame_hdr);
	struct qrpc_frame_hdr *hdr;
	struct qrpc_old_frame_hdr *old_hdr;
	size_t tot_len = 0;

	xdrs->x_op = XDR_ENCODE;
	XDR_SETPOS(xdrs, 0);
	msg->rm_xid = priv->xid;

	if (!xdr_replymsg(xdrs, msg))
		return FALSE;

	memcpy(&priv->out_hdr, priv->in_pktbuf, sizeof(priv->out_hdr));

	qrpc_svc_raw_macaddr_swap((struct ethhdr *)&priv->out_hdr.qhdr.eth_hdr);

	packet_len = XDR_GETPOS(xdrs);
	pkt_nr = (packet_len + payload_max - 1) / payload_max;

	for (i = 0; i < pkt_nr; i++) {
		uint16_t	payload_len = MIN((uint16_t)packet_len - payload_done, payload_max);

		if (qrpc_is_old_rpc((struct ethhdr*) priv->in_pktbuf)) {
			old_hdr = (struct qrpc_old_frame_hdr *)priv->out_pktbuf;
			memcpy(old_hdr, &priv->out_hdr, sizeof(struct qrpc_old_frame_hdr));
			memcpy(priv->out_pktbuf + sizeof(struct qrpc_old_frame_hdr),
					priv->inbuf + sizeof(struct qrpc_frame_hdr),
					payload_len);
			tot_len = sizeof(struct qrpc_old_frame_hdr) + payload_len;
		} else {
			/* build a EthII frame */
			priv->out_hdr.sub_type = ((i != pkt_nr - 1) ?
							QRPC_FRAME_TYPE_FRAG :
							QRPC_FRAME_TYPE_COMPLETE);
			priv->out_hdr.sid = priv->sess_id;
			hdr = (struct qrpc_frame_hdr *)priv->out_pktbuf;
			memcpy(hdr, &priv->out_hdr, sizeof(priv->out_hdr));
			memcpy(hdr + 1, priv->inbuf + sizeof(struct qrpc_frame_hdr) + payload_done, payload_len);
			tot_len = sizeof(struct qrpc_frame_hdr) + payload_len;
		}

		payload_done += payload_len;

		do {
			ret = sendto(xprt->xp_sock, priv->out_pktbuf, tot_len, 0,
					(struct sockaddr *)&priv->dst_addr, sizeof(priv->dst_addr));
		} while (ret < 0 && errno == EINTR);

		if (ret < tot_len)
			return FALSE;
	}

	return TRUE;
}

static bool_t qrpc_svc_raw_freeargs(SVCXPRT *xprt, xdrproc_t xdr_args, caddr_t args_ptr)
{
	struct qrpc_svc_raw_priv *const priv = (struct qrpc_svc_raw_priv *)xprt->xp_p2;
	XDR *const xdrs = &priv->xdrs;

	xdrs->x_op = XDR_FREE;
	return (*xdr_args) (xdrs, args_ptr);
}

static void qrpc_svc_raw_destroy(SVCXPRT *xprt)
{
	struct qrpc_svc_raw_priv *const priv = (struct qrpc_svc_raw_priv *)xprt->xp_p2;

	xprt_unregister(xprt);
	if (xprt->xp_sock >= 0)
		close(xprt->xp_sock);
	XDR_DESTROY(&priv->xdrs);
	qrpc_svc_raw_free_priv(priv);
	free(xprt);
}
