/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2009 - 2011 Quantenna Communications Inc            **
**                                                                           **
**  File        : vsp_doc.c                                                  **
**  Description : Automatically create VSP QCSAPI documentation              **
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

#define inline

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "qcsapi.h"

static const struct qvsp_rule_param qvsp_rule_params[] = QVSP_RULE_PARAMS;
static const struct qvsp_cfg_param qvsp_cfg_params[] = QVSP_CFG_PARAMS;
static const char *qvsp_rule_order_desc[] = QVSP_RULE_ORDER_DESCS;

#define E(x)	case x: return #x

static const char *qvspdoc_enumstr_cfg(enum qvsp_cfg_param_e e)
{
	switch (e) {
	E(QVSP_CFG_ENABLED);
	E(QVSP_CFG_ENABLED_ALWAYS);
	E(QVSP_CFG_FAT_MIN);
	E(QVSP_CFG_FAT_MIN_SOFT);
	E(QVSP_CFG_FAT_MIN_SOFT_CONSEC);
	E(QVSP_CFG_FAT_MIN_SAFE);
	E(QVSP_CFG_FAT_MIN_CHECK_INTV);
	E(QVSP_CFG_FAT_MAX_SOFT);
	E(QVSP_CFG_FAT_MAX_SOFT_CONSEC);
	E(QVSP_CFG_FAT_MAX_SAFE);
	E(QVSP_CFG_FAT_MAX_CHECK_INTV);
	E(QVSP_CFG_NODE_DATA_MIN);
	E(QVSP_CFG_DISABLE_DEMOTE);
	E(QVSP_CFG_DISABLE_DEMOTE_FIX_FAT);
	E(QVSP_CFG_DISABLE_WAIT);
	E(QVSP_CFG_DISABLE_PER_EVENT_MAX);
	E(QVSP_CFG_ENABLE_WAIT);
	E(QVSP_CFG_ENABLE_PER_EVENT_MAX);
	E(QVSP_CFG_STRM_RMT_DIS_TCP);
	E(QVSP_CFG_STRM_RMT_DIS_UDP);
	E(QVSP_CFG_STRM_TPUT_MIN);
	E(QVSP_CFG_STRM_DISABLED_MAX);
	E(QVSP_CFG_STRM_ADPT_THROT);
	E(QVSP_CFG_STRM_ADPT_THROT_STEP);
	E(QVSP_CFG_STRM_ADPT_THROT_MARGIN);
	E(QVSP_CFG_STRM_TPUT_SMPL_MIN);
	E(QVSP_CFG_STRM_COST_RC_ADJUST);
	E(QVSP_CFG_STRM_MAX);
	E(QVSP_CFG_STRM_MAX_AC0);
	E(QVSP_CFG_STRM_MAX_AC1);
	E(QVSP_CFG_STRM_MAX_AC2);
	E(QVSP_CFG_STRM_MAX_AC3);
	E(QVSP_CFG_STRM_MIN);
	E(QVSP_CFG_STRM_MIN_AC0);
	E(QVSP_CFG_STRM_MIN_AC1);
	E(QVSP_CFG_STRM_MIN_AC2);
	E(QVSP_CFG_STRM_MIN_AC3);
	E(QVSP_CFG_STRM_TPUT_MAX_TCP);
	E(QVSP_CFG_STRM_TPUT_MAX_TCP_AC0);
	E(QVSP_CFG_STRM_TPUT_MAX_TCP_AC1);
	E(QVSP_CFG_STRM_TPUT_MAX_TCP_AC2);
	E(QVSP_CFG_STRM_TPUT_MAX_TCP_AC3);
	E(QVSP_CFG_STRM_TPUT_MAX_UDP);
	E(QVSP_CFG_STRM_TPUT_MAX_UDP_AC0);
	E(QVSP_CFG_STRM_TPUT_MAX_UDP_AC1);
	E(QVSP_CFG_STRM_TPUT_MAX_UDP_AC2);
	E(QVSP_CFG_STRM_TPUT_MAX_UDP_AC3);
	E(QVSP_CFG_STRM_ENABLE_WAIT);
	E(QVSP_CFG_STRM_AGE_MAX);
	E(QVSP_CFG_AGE_CHK_INTV);
	E(QVSP_CFG_3RDPT_CTL);
	E(QVSP_CFG_3RDPT_LOCAL_THROT);
	E(QVSP_CFG_3RDPT_QTN);
	E(QVSP_CFG_BA_THROT_INTV);
	E(QVSP_CFG_BA_THROT_DUR_MIN);
	E(QVSP_CFG_BA_THROT_DUR_STEP);
	E(QVSP_CFG_BA_THROT_WINSIZE_MIN);
	E(QVSP_CFG_BA_THROT_WINSIZE_MAX);
	E(QVSP_CFG_WME_THROT_AC);
	E(QVSP_CFG_WME_THROT_AIFSN);
	E(QVSP_CFG_WME_THROT_ECWMIN);
	E(QVSP_CFG_WME_THROT_ECWMAX);
	E(QVSP_CFG_WME_THROT_TXOPLIMIT);
	E(QVSP_CFG_WME_THROT_THRSH_DISABLED);
	E(QVSP_CFG_WME_THROT_THRSH_VICTIM);
	E(QVSP_CFG_EVENT_LOG_LVL);
	E(QVSP_CFG_DEBUG_LOG_LVL);
	E(QVSP_CFG_MAX);
	}
	return "unknown!";
}

static const char *qvspdoc_enumstr_rule_param(enum qvsp_rule_param_e e)
{
	switch (e) {
	E(QVSP_RULE_PARAM_DIR);
	E(QVSP_RULE_PARAM_VAPPRI);
	E(QVSP_RULE_PARAM_AC);
	E(QVSP_RULE_PARAM_PROTOCOL);
	E(QVSP_RULE_PARAM_TPUT_MIN);
	E(QVSP_RULE_PARAM_TPUT_MAX);
	E(QVSP_RULE_PARAM_COST_MIN);
	E(QVSP_RULE_PARAM_COST_MAX);
	E(QVSP_RULE_PARAM_ORDER);
	E(QVSP_RULE_PARAM_THROT_POLICY);
	E(QVSP_RULE_PARAM_DEMOTE);
	E(QVSP_RULE_PARAM_MAX);
	}
	return "unknown!";
}

static const char *qvspdoc_enumstr_rule_order(enum qvsp_rule_order_e e)
{
	switch (e) {
	E(QVSP_RULE_ORDER_GREATEST_COST_NODE);
	E(QVSP_RULE_ORDER_LEAST_COST_NODE);
	E(QVSP_RULE_ORDER_GREATEST_NODE_INV_PHY_RATE);
	E(QVSP_RULE_ORDER_LEAST_NODE_INV_PHY_RATE);
	E(QVSP_RULE_ORDER_GREATEST_COST_STREAM);
	E(QVSP_RULE_ORDER_LEAST_COST_STREAM);
	E(QVSP_RULE_ORDER_NEWEST);
	E(QVSP_RULE_ORDER_OLDEST);
	E(QVSP_RULE_ORDER_LOWEST_TPUT);
	E(QVSP_RULE_ORDER_HIGHEST_TPUT);
	E(QVSP_RULE_ORDER_MAX);
	}
	return "unknown!";
}

static const char *qvspdoc_enumstr_rule_dir(enum qvsp_rule_dir_e e)
{
	switch (e) {
	E(QVSP_RULE_DIR_ANY);
	E(QVSP_RULE_DIR_TX);
	E(QVSP_RULE_DIR_RX);
	}
	return "unknown!";
}

static const char *qvsp_rule_dir_descs[] = QVSP_RULE_DIR_DESCS;

static void create_cfg_table(void)
{
	int i;
	const struct qvsp_cfg_param *param;

	if (0) {
		printf(" * <TABLE>\n");
		printf(" * <TR> <TD> \\b Enum </TD> <TD> \\b Name </TD> <TD> \\b Units </TD> "
				"<TD> \\b Default </TD> <TD> \\b Min </TD> <TD> \\b Max </TD> <TD> \\b Description </TD> </TR>\n");

		for (i = 0; i < QVSP_CFG_MAX; i++) {
			param = &qvsp_cfg_params[i];
			printf(" * <TR><TD>%s (%d)</TD><TD>%s</TD><TD>%s</TD><TD>%u</TD><TD>%u</TD><TD>%u</TD><TD>%s</TD></TR>\n",
					qvspdoc_enumstr_cfg(i),
					i,
					param->name,
					param->units,
					param->default_val,
					param->min_val,
					param->max_val,
					param->desc);
		}

		printf(" * </TABLE>\n");
	} else if(0) {
		static int colwidths[] = {34, 23, 10, 8, 8, 8, 32};

		printf(" * @section vsp_cfg_table VSP Configuration options\n");
		printf(" *\n");
		printf(" * @code\n");


		printf(" *  %-*s%-*s%-*s%-*s%-*s%-*s%-*s\n",
				colwidths[0], "Enum",
				colwidths[1], "Name",
				colwidths[2], "Units",
				colwidths[3], "Default",
				colwidths[4], "Minimum",
				colwidths[5], "Maximum",
				colwidths[6], "Description");

		for (i = 0; i < QVSP_CFG_MAX; i++) {
			param = &qvsp_cfg_params[i];
			printf(" * %2d %-*s%-*s%-*s%-*u%-*u%-*u%-*s\n",
					i, colwidths[0] - 2, qvspdoc_enumstr_cfg(i),
					colwidths[1], param->name,
					colwidths[2], param->units,
					colwidths[3], param->default_val,
					colwidths[4], param->min_val,
					colwidths[5], param->max_val,
					colwidths[6], param->desc);
		}

		printf(" * @endcode\n *\n");
	} else {
		printf(" * @section vsp_cfg_table VSP Configuration options\n");

		for (i = 0; i < QVSP_CFG_MAX; i++) {
			param = &qvsp_cfg_params[i];
			printf(" * \\li %d: %s - %s<br>Default: %u %s [%u - %u].  %s\n",
					i,
					qvspdoc_enumstr_cfg(i),
					param->name,
					param->default_val,
					param->units,
					param->min_val,
					param->max_val,
					param->desc);
		}
	}
}

static void create_rule_table(void)
{
	int i;
	int j;
	const struct qvsp_rule_param *rule_param;

	printf(" * @section vsp_rule_table VSP Rule options\n");
	for (i = 0; i < QVSP_RULE_PARAM_MAX; i++) {
		rule_param = &qvsp_rule_params[i];
		printf(" * - %d: %s - %s<br>[%u - %u %s].  %s\n",
				i,
				qvspdoc_enumstr_rule_param(i),
				rule_param->name,
				rule_param->min_val,
				rule_param->max_val,
				rule_param->units,
				rule_param->desc);
		switch (i) {
			case QVSP_RULE_PARAM_DIR:
				printf(" * <br>Possible values are:\n");
				for (j = 0; j < ARRAY_SIZE(qvsp_rule_dir_descs); j++) {
					printf("    - %s - %s\n",
							qvspdoc_enumstr_rule_dir(j),
							qvsp_rule_dir_descs[j]);
				}
				break;
			case QVSP_RULE_PARAM_VAPPRI:
				printf(" * <br>Possible values are:\n");
				printf("     - 0x01 = VAP priority 0\n");
				printf("     - 0x02 = VAP priority 1\n");
				printf("     - 0x04 = VAP priority 2\n");
				printf("     - 0x08 = VAP priority 3\n");
				break;
			case QVSP_RULE_PARAM_AC:
				printf(" * <br>Possible values are:\n");
				printf("     - 0x01 = Best Effort (0)\n");
				printf("     - 0x02 = Background (1)\n");
				printf("     - 0x04 = Voice (2)\n");
				printf("     - 0x08 = Video (3)\n");
				break;
			case QVSP_RULE_PARAM_ORDER:
				printf(" * <br>Allowed match orderings are (see enum qvsp_rule_order_e):\n");
				for (j = 0; j < QVSP_RULE_ORDER_MAX; j++) {
					printf("    - %d: %s<br>%s\n",
							j,
							qvspdoc_enumstr_rule_order(j),
							qvsp_rule_order_desc[j]);
				}
				break;
			default:
				break;
		}
	}
}

int main(int argc, char **argv)
{
	printf("/** @addtogroup vsp_group\n\n");
	create_cfg_table();
	create_rule_table();
	printf(" */\n");
	return 0;
}
