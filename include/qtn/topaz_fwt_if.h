/*SH1
*******************************************************************************
**                                                                           **
**         Copyright (c) 2013 Quantenna Communications Inc                   **
**                            All Rights Reserved                            **
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
EH1*/

#ifndef FWT_INTERFACE_H_
#define FWT_INTERFACE_H_

#include <qtn/br_types.h>

/* Must match FWT_IF_KEY_xxx macros */
typedef enum fwt_if_usr_cmd {
	FWT_IF_CMD_CLEAR = 0,
	FWT_IF_CMD_ON,
	FWT_IF_CMD_OFF,
	FWT_IF_CMD_PRINT,
	FWT_IF_CMD_ADD_STATIC_MC,
	FWT_IF_CMD_DEL_STATIC_MC,
	FWT_IF_CMD_GET_MC_LIST,
	FWT_IF_CMD_ADD,
	FWT_IF_CMD_DELETE,
	FWT_IF_CMD_AUTO,
	FWT_IF_CMD_MANUAL,
	FWT_IF_CMD_4ADDR,
	FWT_IF_CMD_DEBUG,
	FWT_IF_CMD_HELP,
	FWT_IF_CMD_AGEING,
	FWT_IF_MAX_CMD,
} fwt_if_usr_cmd;

#include <linux/types.h>

#define FWT_IF_USER_NODE_MAX (6)

struct fwt_if_id {
	uint8_t mac_be[ETH_ALEN];
	struct br_ip ip;
};

struct fwt_if_common {
	struct fwt_if_id id;
	uint8_t port;
	uint8_t node[FWT_IF_USER_NODE_MAX];
	uint32_t param;
	void *extra;
};

typedef int (*fwt_if_sw_cmd_hook)(fwt_if_usr_cmd cmd, struct fwt_if_common *data);

void fwt_if_register_cbk_t(fwt_if_sw_cmd_hook cbk_func);

#endif /* FWT_INTERFACE_H_ */
