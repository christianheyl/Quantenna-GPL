/**
 * Copyright (c) 2012-2013 Quantenna Communications, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/io.h>

#include <asm/hardware.h>

#include <asm/board/platform.h>

#define TOPAZ_TQE_PROC_FILENAME	"topaz_tqe"

static int topaz_tqe_stat_rd(char *page, char **start, off_t offset,
		int count, int *eof, void *data)
{
	char *p = page;
	int i;
	struct {
		uint32_t reg;
		const char *name;
	} regs[] = {
		{ TOPAZ_TQE_OUTPORT_EMAC0_CNT, "emac0", },
		{ TOPAZ_TQE_OUTPORT_EMAC1_CNT, "emac1", },
		{ TOPAZ_TQE_OUTPORT_WMAC_CNT, "wmac", },
		{ TOPAZ_TQE_OUTPORT_LHOST_CNT, "lhost", },
		{ TOPAZ_TQE_OUTPORT_MUC_CNT, "muc", },
		{ TOPAZ_TQE_OUTPORT_DSP_CNT, "dsp", },
		{ TOPAZ_TQE_OUTPORT_AUC_CNT, "auc", },
		{ TOPAZ_TQE_OUTPORT_PCIE_CNT, "pcie", },
		{ TOPAZ_TQE_DROP_CNT, "drop", },
		{ TOPAZ_TQE_DROP_EMAC0_CNT, "emac0 d", },
		{ TOPAZ_TQE_DROP_EMAC1_CNT, "emac1 d", },
		{ TOPAZ_TQE_DROP_WMAC_CNT, "wmac d", },
		{ TOPAZ_TQE_DROP_LHOST_CNT, "lhost d", },
		{ TOPAZ_TQE_DROP_MUC_CNT, "muc d", },
		{ TOPAZ_TQE_DROP_DSP_CNT, "dsp d", },
		{ TOPAZ_TQE_DROP_AUC_CNT, "auc d", },
		{ TOPAZ_TQE_DROP_PCIE_CNT, "pcie d", },
	};

	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		uint32_t reg = readl(regs[i].reg);
		p += sprintf(p, "%8s = %u 0x%08x\n", regs[i].name, reg, reg);
	}

	return p - page;
}

static int __init topaz_tqe_stat_init(void)
{
	if (!create_proc_read_entry(TOPAZ_TQE_PROC_FILENAME, 0,
				NULL, topaz_tqe_stat_rd, NULL)) {
		return -EEXIST;
	}
	return 0;
}

late_initcall(topaz_tqe_stat_init);

MODULE_DESCRIPTION("TQE");
MODULE_AUTHOR("Quantenna");
MODULE_LICENSE("GPL");


