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


#include <call_qcsapi.h>
#include <call_qcsapi_rpc/generated/call_qcsapi_rpc.h>
#include <qcsapi_rpc_common/server/qtn_start_rpc_svc.h>

static char blank[4] = { 0 };

bool_t call_qcsapi_remote_1_svc(struct call_qcsapi_rpc_request *argp, call_qcsapi_rpc_result *result, struct svc_req *rqstp)
{
	struct qcsapi_output print;
	char *stdout_buf = NULL;
	char *stderr_buf = NULL;

	print = qcsapi_output_buf_adapter(&stdout_buf, 0, &stderr_buf, 0, 1);

	result->return_code = qcsapi_main(&print, argp->argv.argv_len, argp->argv.argv_val);
	result->stdout_produced = stdout_buf ? stdout_buf : blank;
	result->stderr_produced = stderr_buf ? stderr_buf : blank;

	return 1;
}

static void call_qcsapi_remote_1_free(call_qcsapi_rpc_result *result)
{
	if (result->stdout_produced && result->stdout_produced != blank)
		free(result->stdout_produced);
	result->stdout_produced = NULL;
	if (result->stderr_produced && result->stderr_produced != blank)
		free(result->stderr_produced);
	result->stderr_produced = NULL;
}

int call_qcsapi_prog_1_freeresult (SVCXPRT *transp, xdrproc_t xdr_result, caddr_t cresult)
{
	/*
	 * NB: If more than one API is added to this server,
	 * we need to demux return types
	 */
	call_qcsapi_remote_1_free((call_qcsapi_rpc_result*)cresult);

	xdr_free (xdr_result, cresult);

	return 1;
}

void call_qcsapi_prog_1(struct svc_req *rqstp, register SVCXPRT *transp);

int main (int argc, char **argv)
{
	return qtn_start_rpc_svc(argc, argv, CALL_QCSAPI_PROG, CALL_QCSAPI_VERS, &call_qcsapi_prog_1);
}

