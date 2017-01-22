/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2011 Quantenna Communications Inc                   **
**                                                                           **
**  File        : call_qcsapi_sockrpcd.c                                     **
**  Description : Wrapper from rpc server daemon to call_qcsapi,             **
**                starting from an rpcgen generated server stub.             **
**                                                                           **
*******************************************************************************
**                                                                           **
**  Redistribution and use in source and binary forms, with or without       **
**  modification, are permitted provided that the following conditions       **
**  are met:                                                                 **
**  1. Redistributions of source code must retain the above copyright        **
**     notice, this list of conditions and the following disclaimer.         **
**  2. Redistributions in binary form must reproduce the above copyright     **
**     notice, this list of conditions and the following disclaimer in the   **
**     documentation and/or other materials provided with the distribution.  **
**  3. The name of the author may not be used to endorse or promote products **
**     derived from this software without specific prior written permission. **
**                                                                           **
**  Alternatively, this software may be distributed under the terms of the   **
**  GNU General Public License ("GPL") version 2, or (at your option) any    **
**  later version as published by the Free Software Foundation.              **
**                                                                           **
**  In the case this software is distributed under the GPL license,          **
**  you should have received a copy of the GNU General Public License        **
**  along with this software; if not, write to the Free Software             **
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA  **
**                                                                           **
**  THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR       **
**  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES**
**  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  **
**  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,         **
**  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT **
**  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,**
**  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY    **
**  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT      **
**  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF **
**  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.        **
**                                                                           **
*******************************************************************************
EH0*/

#include <qcsapi_rpc_common/server/qtn_start_rpc_svc.h>
#include <qcsapi_rpc_common/common/rpc_pci.h>
#include <qcsapi_rpc_common/common/rpc_raw.h>
#include <errno.h>

#define QTN_START_RPCSVC_UDP	0x1
#define QTN_START_RPCSVC_TCP	0x2
#define QTN_START_RPCSVC_PCI	0x4
#define QTN_START_RPCSVC_RAW	0x8
#define QTN_START_RPCSVC_SOCK	(QTN_START_RPCSVC_UDP | QTN_START_RPCSVC_TCP)
#define QTN_START_RPCSVC_ALL	(QTN_START_RPCSVC_UDP | QTN_START_RPCSVC_TCP | \
					QTN_START_RPCSVC_PCI | QTN_START_RPCSVC_RAW)

struct svc_arg {
	unsigned long flag;
	unsigned long ipproto;
	SVCXPRT *(*create_cb)(int sock);
	const char *arg;
};

static SVCXPRT *svc_raw_create_wrap(int sock);
static SVCXPRT *svctcp_create_wrap(int sock);
static const struct svc_arg svc_args[] = {
	{ QTN_START_RPCSVC_UDP, IPPROTO_UDP, svcudp_create, "--udp" },
	{ QTN_START_RPCSVC_TCP, IPPROTO_TCP, svctcp_create_wrap, "--tcp" },
	{ QTN_START_RPCSVC_PCI, 0, svc_pci_create, "--pcie" },
	{ QTN_START_RPCSVC_RAW, 0, svc_raw_create_wrap, "--raw" },
};

static char *bind_interface = "eth1_0";
static uint8_t session_id;
static SVCXPRT *svc_raw_create_wrap(int sock)
{
	return qrpc_svc_raw_create(sock, bind_interface, session_id);
}

static SVCXPRT *svctcp_create_wrap(int sock)
{
	return svctcp_create(sock, 0, 0);
}

static unsigned long qtn_start_rpc_get_services(int argc, char **argv)
{
	unsigned long which_services = 0;
	int i;
	int j;

	for (i = 1; i < argc; i++) {
		for (j = 0; j < ARRAY_SIZE(svc_args); j++) {
			if (strcmp(argv[i], svc_args[j].arg) == 0) {
				which_services |= svc_args[j].flag;
			}
		}
	}

	if (!which_services) {
		which_services = QTN_START_RPCSVC_UDP;
	}

	return which_services;
}

static int qtn_start_rpc_get_bind_interface(const int argc, char **argv)
{
	int cur_arg = 0;
	int found = 0;

	while (cur_arg < argc) {
		if (strcmp(argv[cur_arg], "--bind") == 0) {
			if (cur_arg + 1 < argc) {
				bind_interface = argv[cur_arg + 1];
				found = 1;
			}
			break;
		}
		++cur_arg;
	}

	return found;
}

int qtn_start_rpc_svc(int argc, char **argv,
		unsigned long prog, unsigned long vers,
		void (*svc_func)(struct svc_req *, SVCXPRT *))
{
	SVCXPRT *transp;
	unsigned long which_services;
	uint32_t svc_idx;

	which_services = qtn_start_rpc_get_services(argc, argv);

	if (!(which_services & QTN_START_RPCSVC_ALL)) {
		fprintf(stderr, "%s: no servers specified!\n", __FUNCTION__);
		return -EINVAL;
	}

	if (which_services & QTN_START_RPCSVC_RAW) {
		if (!qtn_start_rpc_get_bind_interface(argc, argv))
			fprintf(stderr, "--bind interface is not specified, using default interface: %s\n",
				bind_interface);
		if (strstr(argv[0], "call_qcsapi_rpcd"))
			session_id = QRPC_CALL_QCSAPI_RPCD_SID;
		else if (strstr(argv[0], "qcsapi_rpcd"))
			session_id = QRPC_QCSAPI_RPCD_SID;
		else
			return -EINVAL;
	}

	if (which_services & QTN_START_RPCSVC_SOCK) {
		pmap_unset (prog, vers);
	}

	for (svc_idx = 0; svc_idx < ARRAY_SIZE(svc_args); ++svc_idx) {
		if (!(which_services & svc_args[svc_idx].flag))
			continue;

		transp = svc_args[svc_idx].create_cb(RPC_ANYSOCK);
		if (transp == NULL) {
			fprintf(stderr, "%s: cannot create %s service\n",
					__FUNCTION__, svc_args[svc_idx].arg + 2);
			return -1;
		}

		if (!svc_register(transp, prog, vers, svc_func, svc_args[svc_idx].ipproto)) {
			fprintf(stderr, "%s: unable to register (%lu, %lu, %s)\n",
					__FUNCTION__, prog, vers, svc_args[svc_idx].arg + 2);
			if (transp->xp_ops->xp_destroy)
				transp->xp_ops->xp_destroy(transp);
			return -1;
		}
	}

	svc_run();
	fprintf(stderr, "%s: svc_run returned", __FUNCTION__);

	return -1;
}

