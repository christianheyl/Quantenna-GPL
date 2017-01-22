/*
 * pci_rpc_svc.c,
 * Server side for UDP/IP based RPC.  (Does some caching in the hopes of
 * achieving execute-at-most-once semantics.)
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <errno.h>
//#include <libintl.h>
#include <linux/netlink.h>
#include <qcsapi_rpc_common/common/rpc_pci.h>

#ifdef IP_PKTINFO
#include <sys/uio.h>
#endif

#ifdef USE_IN_LIBIO
# include <wchar.h>
# include <libio/iolibio.h>
#endif

#define rpc_buffer(xprt) ((xprt)->xp_p1)
#ifndef MAX
#define MAX(a, b)     ((a > b) ? a : b)
#endif

#ifndef PCIE_RPC_TYPE
	#error "Not configure PCIE_RPC_TYPE"
#else
	#if (PCIE_RPC_TYPE != RPC_TYPE_CALL_QCSAPI_PCIE) && (PCIE_RPC_TYPE != RPC_TYPE_QCSAPI_PCIE)
	#error "Configuration invalid value for PCIE_RPC_TYPE"
	#endif
#endif

static bool_t svc_pci_recv(SVCXPRT *, struct rpc_msg *);
static bool_t svc_pci_reply(SVCXPRT *, struct rpc_msg *);
static enum xprt_stat svc_pci_stat(SVCXPRT *);
static bool_t svc_pci_getargs(SVCXPRT *, xdrproc_t, caddr_t);
static bool_t svc_pci_freeargs(SVCXPRT *, xdrproc_t, caddr_t);
static void svc_pci_destroy(SVCXPRT *);

static const struct xp_ops svc_pci_op = {
	svc_pci_recv,
	svc_pci_stat,
	svc_pci_getargs,
	svc_pci_reply,
	svc_pci_freeargs,
	svc_pci_destroy
};

/*
 * kept in xprt->xp_p2
 */
struct svc_pci_data {
	u_int su_iosz;		/* byte size of send.recv buffer */
	u_long su_xid;		/* transaction id */
	XDR su_xdrs;		/* XDR handle */
	char su_verfbody[MAX_AUTH_BYTES];	/* verifier body */
	char *su_cache;		/* cached data, NULL if no cache */
	struct nlmsghdr *su_nlh;
	struct sockaddr_nl su_daddr;
};

#define	su_data(xprt)	((struct svc_pci_data *)(xprt->xp_p2))

/*
 * Usage:
 *      xprt = svc_pci_create(sock);
 *
 * If sock<0 then a socket is created, else sock is used.
 * If the socket, sock is not bound to a port then svc_pci_create
 * binds it to an arbitrary port.  In any (successful) case,
 * xprt->xp_sock is the registered socket number and xprt->xp_port is the
 * associated port number.
 * Once *xprt is initialized, it is registered as a transporter;
 * see (svc.h, xprt_register).
 * The routines returns NULL if a problem occurred.
 */
//static SVCXPRT *
//svc_pci_bufcreate (sock, sendsz, recvsz)
//     int sock;
//     u_int sendsz, recvsz;

static SVCXPRT *svc_pci_bufcreate(int sock, u_int sendsz, u_int recvsz)
{
	//bool_t madesock = FALSE;
	SVCXPRT *xprt;
	struct svc_pci_data *su;
	//struct sockaddr_in addr;
	//socklen_t len = sizeof (struct sockaddr_in);
	int pad;
	void *buf;
	struct sockaddr_nl src_addr, dest_addr;
	struct nlmsghdr *nlh = NULL;
	struct iovec iov;
	int sock_fd;
	struct msghdr msg;
	//int ret = 0;

	sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_RPC_PCI_SVC);
	if (sock_fd < 0)
		return NULL;

	xprt = (SVCXPRT *) malloc(sizeof(SVCXPRT));
	su = (struct svc_pci_data *)malloc(sizeof(*su));
	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(PCIMSGSIZE));

	if (xprt == NULL || su == NULL || nlh == NULL) {
		//(void) fprintf (stderr, "svc_pci_create out of memory\n");
		if (xprt)
			free(xprt);
		if (su)
			free(su);
		if (nlh)
			free(nlh);
		close(sock_fd);
		return NULL;
	}

	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid();	/* self pid */

	bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr));

	memset(&dest_addr, 0, sizeof(dest_addr));
	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pid = 0;	/* For Linux Kernel */
	dest_addr.nl_groups = 0;	/* unicast */

	memset(nlh, 0, NLMSG_SPACE(PCIMSGSIZE));
	nlh->nlmsg_len = NLMSG_SPACE(PCIMSGSIZE);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = NLM_F_REQUEST;
	buf = NLMSG_DATA(nlh);

	su->su_iosz = PCIMSGSIZE;
	rpc_buffer(xprt) = buf;	// xp_p1
	xdrmem_create(&(su->su_xdrs), rpc_buffer(xprt), su->su_iosz,
		      XDR_DECODE);
	su->su_cache = NULL;
	su->su_nlh = nlh;

	su->su_daddr = dest_addr;

	xprt->xp_p2 = (caddr_t) su;
	xprt->xp_verf.oa_base = su->su_verfbody;
	xprt->xp_ops = &svc_pci_op;
	//xprt->xp_port = ntohs (addr.sin_port);
	xprt->xp_sock = sock_fd;

	/* Clear the padding. */
	pad = 0;
	memset(&xprt->xp_pad[0], pad, sizeof(xprt->xp_pad));

	xprt_register(xprt);

	// Register the server. FIXME
	nlh->nlmsg_len = 0;
	nlh->nlmsg_type = NETLINK_TYPE_SVC_REGISTER;

	iov.iov_base = (void *)nlh;
	iov.iov_len = NLMSG_SPACE(0);

	memset((caddr_t) & msg, 0, sizeof(msg));
	msg.msg_name = (void *)&dest_addr;
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	sendmsg(sock_fd, &msg, 0);

	return xprt;
}

SVCXPRT *svc_pci_create(int sock)
{
	return svc_pci_bufcreate(sock, PCIMSGSIZE, PCIMSGSIZE);
}

static enum xprt_stat svc_pci_stat(SVCXPRT * xprt)
{

	return XPRT_IDLE;
}

static bool_t svc_pci_recv(SVCXPRT * xprt, struct rpc_msg *msg)
{
	struct svc_pci_data *su = su_data(xprt);
	XDR *xdrs = &(su->su_xdrs);
	int rlen;
	//char *reply;
	// u_long replylen;
	struct iovec iov;
	struct msghdr hmsg;

	/* It is very tricky when you have IP aliases. We want to make sure
	   that we are sending the packet from the IP address where the
	   incoming packet is addressed to. H.J. */

	memset((caddr_t) & hmsg, 0, sizeof(hmsg));
again:
	/* FIXME -- should xp_addrlen be a size_t?  */
	iov.iov_base = (void *)su->su_nlh;
	su->su_nlh->nlmsg_len = NLMSG_SPACE(PCIMSGSIZE);
	iov.iov_len = su->su_nlh->nlmsg_len;
	hmsg.msg_name = (void *)&su->su_daddr;
	hmsg.msg_namelen = sizeof(su->su_daddr);
	hmsg.msg_iov = &iov;
	hmsg.msg_iovlen = 1;

	//printf("Waiting for message from kernel\n");

	/* Read message from kernel */
	rlen = recvmsg(xprt->xp_sock, &hmsg, 0);
	//printf("Received message payload: len %d \n", rlen );

	if (rlen == -1 && errno == EINTR)
		goto again;
	if (rlen < 16)		/* < 4 32-bit ints? */
		return FALSE;
	xdrs->x_op = XDR_DECODE;
	XDR_SETPOS(xdrs, 0);
	if (!xdr_callmsg(xdrs, msg))
		return FALSE;
	su->su_xid = msg->rm_xid;

	return TRUE;
}

static bool_t svc_pci_reply(SVCXPRT * xprt, struct rpc_msg *msg)
{
	struct svc_pci_data *su = su_data(xprt);
	XDR *xdrs = &(su->su_xdrs);
	int slen, sent;
	bool_t stat = FALSE;

	struct iovec iov;
	struct msghdr hmsg;

	memset((caddr_t) & hmsg, 0, sizeof(hmsg));

	xdrs->x_op = XDR_ENCODE;
	XDR_SETPOS(xdrs, 0);
	msg->rm_xid = su->su_xid;

	iov.iov_base = (void *)su->su_nlh;
	hmsg.msg_name = (void *)&su->su_daddr;
	hmsg.msg_namelen = sizeof(su->su_daddr);
	hmsg.msg_iov = &iov;
	hmsg.msg_iovlen = 1;

	su->su_nlh->nlmsg_type = NETLINK_TYPE_SVC_RESPONSE;

	if (xdr_replymsg(xdrs, msg)) {
		slen = (int)XDR_GETPOS(xdrs);
		su->su_nlh->nlmsg_len = slen;
		iov.iov_len = NLMSG_SPACE(slen);
		sent = sendmsg(xprt->xp_sock, &hmsg, 0);
		if (sent == slen) {
			stat = TRUE;
		}
		//fprintf(stderr, "svc_pci_reply sent:%d, len:%lu\n", sent, iov.iov_len);
	}
	return stat;
}

static bool_t
svc_pci_getargs(SVCXPRT * xprt, xdrproc_t xdr_args, caddr_t args_ptr)
{

	return (*xdr_args) (&(su_data(xprt)->su_xdrs), args_ptr);
}

static bool_t
svc_pci_freeargs(SVCXPRT * xprt, xdrproc_t xdr_args, caddr_t args_ptr)
{
	XDR *xdrs = &(su_data(xprt)->su_xdrs);

	xdrs->x_op = XDR_FREE;
	return (*xdr_args) (xdrs, args_ptr);
}

static void svc_pci_destroy(SVCXPRT * xprt)
{
	struct svc_pci_data *su = su_data(xprt);

	xprt_unregister(xprt);
	(void)close(xprt->xp_sock);
	XDR_DESTROY(&(su->su_xdrs));
	free((caddr_t) su->su_nlh);
	free((caddr_t) su);
	free((caddr_t) xprt);
}
