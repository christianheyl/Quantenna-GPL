/**
  Copyright (c) 2008 - 2013 Quantenna Communications Inc
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

#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#include <linux/version.h>

#include <linux/device.h>
#include <linux/delay.h>
#include <asm/hardware.h>
#include <asm/gpio.h>
#include "qdrv_features.h"
#include "qdrv_debug.h"
#include "qdrv_hal.h"
#include "qdrv_mac.h"
#include "qdrv_soc.h"
#include <qtn/registers.h>
#include <qtn/shared_params.h>
#include <qtn/txbf_mbox.h>
#include <qtn/qtn_bb_mutex.h>
#include <common/topaz_reset.h>

/* unaligned little endian access */
#define LE_READ_4(p)	((u_int32_t)						\
				((((const u_int8_t *)(p))[0]) |			\
					(((const u_int8_t *)(p))[1] <<  8) |	\
					(((const u_int8_t *)(p))[2] << 16) |	\
					(((const u_int8_t *)(p))[3] << 24)))

void hal_get_tsf(uint64_t *ret)
{
	uint64_t tsf64;
	uint32_t *tsf = (uint32_t *) &tsf64;
	uint32_t temp_tsf1;

	qtn_bb_mutex_enter(QTN_LHOST_SOC_CPU);

	temp_tsf1 = readl(HAL_REG(HAL_REG_TSF_HI));
	tsf[0] = readl(HAL_REG(HAL_REG_TSF_LO));
	tsf[1] = readl(HAL_REG(HAL_REG_TSF_HI));
	if (temp_tsf1 != tsf[1]) /* handling wrap-around case. */
		tsf[0] = readl(HAL_REG(HAL_REG_TSF_LO));

	qtn_bb_mutex_leave(QTN_LHOST_SOC_CPU);

	*ret = tsf64;
}

void hal_reset(void)
{
	u32 reset;

#ifdef CONFIG_ARCH_ARC
	reset = RUBY_SYS_CTL_RESET_NETSS | RUBY_SYS_CTL_RESET_MAC | RUBY_SYS_CTL_RESET_BB;

	/* Reset MAC HW */
	writel(reset, SYS_RESET_VECTOR_MASK);
	writel(reset, SYS_RESET_VECTOR);
	udelay(50);
	writel(0, SYS_RESET_VECTOR_MASK);
	/*
	 * After MAC reset the Rx path is enabled, disabling it here.
	 * Note: This may need to be revisited for Mu-MIMO, or QAC3.
	 */
	writel(0, HAL_REG(HAL_REG_RX_CSR));
# if !TOPAZ_FPGA_PLATFORM
	/*
	 * Special programming to turn off the power amplifiers
	 * immediately after bringing the baseband out of reset.
	 *
	 * Baseband has to be put into Soft Reset for this operation to work.
	 */
	writel(0x04, QT3_BB_GLBL_SOFT_RST);
	writel(0x0bb5, QT3_BB_TD_PA_CONF);
	writel(0x00, QT3_BB_GLBL_SOFT_RST);
	/*
	 * Bring the RFIC out of reset by first driving GPIO 15 (RFIC RESET) to be low
	 * for 10 usec, and then driving it high.
	 */
	if (gpio_request(RUBY_GPIO_RFIC_RESET, "rfic_reset") < 0)
		printk(KERN_ERR "Failed to request GPIO%d for GPIO rfic_reset\n",
				RUBY_GPIO_RFIC_RESET);

	gpio_direction_output(RUBY_GPIO_RFIC_RESET, 0);
	udelay(10);
	gpio_set_value(RUBY_GPIO_RFIC_RESET, 1);
	gpio_free(RUBY_GPIO_RFIC_RESET);
# endif /* !TOPAZ_FPGA_PLATFORM */
#else
	/* BBIC2 - non-ARC processor */
	reset = DSPSS_RESET | NETSS_RESET | MMC_RESET | BB_RESET | MB_RESET | SRAM_RESET | BB_RESET;
	/* Reset MAC HW */
	writel(reset, SYS_RESET_VECTOR_MASK);
	writel(reset, SYS_RESET_VECTOR);
	udelay(50);
	/* fix soft-reset polarity issue BBIC2 */
	writel(0x2030, BB_PREG_SPARE_0(0));
#endif /* CONFIG_ARCH_ARC */
}

void hal_enable_muc(u32 muc_start_addr)
{
	const unsigned long reset = RUBY_SYS_CTL_RESET_MUC_ALL;
#ifdef FIXME_NOW
	volatile u32 value;
#endif

#ifdef CONFIG_ARCH_ARC
	/* Check that we can start this address */
	if (muc_start_addr & ((1 << RUBY_SYS_CTL_MUC_REMAP_SHIFT) - 1)) {
		panic("MuC address 0x%x cannot be used as entry point\n", (unsigned)muc_start_addr);
	}
	/* Tells MuC its boot address */
	writel(RUBY_SYS_CTL_MUC_REMAP_VAL(muc_start_addr), RUBY_SYS_CTL_MUC_REMAP);
	/* Take MUC out of reset */
	topaz_set_reset_vec(1, reset);

	DBGPRINTF(DBG_LL_INFO, QDRV_LF_TRACE, "Reset MuC and enabled MuC boot remap %08X\n", readl(RUBY_SYS_CTL_MUC_REMAP));
#else
	/* Take MUC out of reset */
	*(volatile u32 *)IO_ADDRESS(SYS_RESET_VECTOR_MASK) = MUC_RESET;
	*(volatile u32 *)IO_ADDRESS(SYS_RESET_VECTOR) = MUC_RESET;
#endif //CONFIG_ARCH_ARC

#ifdef FIXME_NOW
	/* Set bit 15 for DGPIO enable */
	value = *(volatile u32 *)IO_ADDRESS(SYS_CONTROL_MASK);
	*(volatile u32 *)IO_ADDRESS(SYS_CONTROL_MASK) = value | DSP_MASTER_GPIO_ENABLE;

	value = *(volatile u32 *) IO_ADDRESS(SYS_CONTROL_REG);
	*(volatile u32 *)IO_ADDRESS(SYS_CONTROL_REG) = value | DSP_MASTER_GPIO_ENABLE;
#endif
}

void hal_disable_muc(void)
{
#ifdef CONFIG_ARCH_ARC
	topaz_set_reset_vec(0, RUBY_SYS_CTL_RESET_MUC_ALL);
#else
	*(volatile u32 *)IO_ADDRESS(SYS_RESET_VECTOR_MASK) = MUC_RESET;
	*(volatile u32 *)IO_ADDRESS(SYS_RESET_VECTOR) = 0;
#endif
}
EXPORT_SYMBOL(hal_disable_muc);

void hal_enable_mbx(void)
{
	*(volatile u32 *)IO_ADDRESS(SYS_RESET_VECTOR_MASK) = MB_RESET;
	*(volatile u32 *)IO_ADDRESS(SYS_RESET_VECTOR) = MB_RESET;
}

void hal_disable_mbx(void)
{
	/* Clear the mailboxes by cycling them */
	*(volatile u32 *)IO_ADDRESS(SYS_RESET_VECTOR_MASK) = MB_RESET;
	*(volatile u32 *)IO_ADDRESS(SYS_RESET_VECTOR) = 0;
}

void hal_disable_dsp(void)
{
#ifdef CONFIG_ARCH_ARC
# if 0
# error writing to this hangs the bus
	const unsigned long reset = RUBY_SYS_CTL_RESET_DSP_ALL;

	topaz_set_reset_vec(0, reset);
# endif
#else
	/* Hold the DSP in reset */
	*(volatile u32 *)IO_ADDRESS(SYS_RESET_VECTOR_MASK) = DSP_RESET;
	*(volatile u32 *)IO_ADDRESS(SYS_RESET_VECTOR) = 0;
#endif
}

void hal_enable_dsp(void)
{
#ifdef CONFIG_ARCH_ARC
	const unsigned long reset = RUBY_SYS_CTL_RESET_DSP_ALL;

	qtn_txbf_lhost_init();

	topaz_set_reset_vec(1, reset);
#else
	/* Bring the DSP out of reset */
	*(volatile u32 *)IO_ADDRESS(SYS_RESET_VECTOR_MASK) = DSP_RESET;
	*(volatile u32 *)IO_ADDRESS(SYS_RESET_VECTOR) = DSP_RESET;
#endif
}

void hal_enable_gpio(void)
{
	u32 value;

	/* Set bit 15 for DGPIO enable */
	value = *(volatile u32 *)IO_ADDRESS(SYS_CONTROL_MASK);
	*(volatile u32 *)IO_ADDRESS(SYS_CONTROL_MASK) =
		value | DSP_MASTER_GPIO_ENABLE;

	value = *(volatile u32 *)IO_ADDRESS(SYS_CONTROL_REG);
	*(volatile u32 *)IO_ADDRESS(SYS_CONTROL_REG) =
		value | DSP_MASTER_GPIO_ENABLE;
}

#define DSP_JUMP_INSTR_SWAP	0x0F802020

void hal_dsp_start(u32 dsp_start_addr)
{
#ifdef CONFIG_ARCH_ARC
	/* Check that we can start this address */
	if (dsp_start_addr & ((1 << RUBY_SYS_CTL_DSP_REMAP_SHIFT) - 1)) {
		panic("DSP address 0x%x cannot be used as entry point\n", (unsigned)dsp_start_addr);
	}
	/* Tells DSP from which address start execution */
	writel(RUBY_SYS_CTL_DSP_REMAP_VAL(dsp_start_addr), RUBY_SYS_CTL_DSP_REMAP);
#else
	/* Swap upper and lower half words for DSP instruction */
	dsp_start_addr = ((dsp_start_addr >> 16) & 0xFFFF) | (dsp_start_addr << 16);

	/* Push the jump instr and location into the mbx */
	*(volatile u32*)IO_ADDRESS(UMS_REGS_MB + UMS_MBX_DSP_PUSH)
		= DSP_JUMP_INSTR_SWAP;
	*(volatile u32*)IO_ADDRESS(UMS_REGS_MB + UMS_MBX_DSP_PUSH)
		= dsp_start_addr;
#endif
}

void hal_disable_auc(void)
{
}

void hal_enable_auc(void)
{
	const unsigned long reset = TOPAZ_SYS_CTL_RESET_AUC;

	topaz_set_reset_vec(1, reset);
}

int hal_range_check_sram_addr(void *addr)
{
	/*
	* FIXME!!!
	*
	* On Ruby platform MuC firmware can use several sections mapped to different
	* (not joined) address ranges. So there are no simple way to detect whether address
	* is valid or not (check should determine whether address belongs to any of these
	* sections or not).
	* And for sure this function should not have "sram" in its name.
	* For Ruby not only SRAM is used for MuC!
	* For now let's always return success.
	*/
	return(0);
}



void hal_rf_enable()
{
	volatile u32 *addr;

	/* set bit 15 for DGPIO enable */
	addr = (volatile u32 *)IO_ADDRESS(SYS_CONTROL_MASK);
	*addr |= DSP_MASTER_GPIO_ENABLE;

	addr = (volatile u32 *)IO_ADDRESS(SYS_CONTROL_REG);
	*addr |= DSP_MASTER_GPIO_ENABLE;

	addr = (volatile u32 *)IO_ADDRESS(RUBY_GPIO_REGS_ADDR + GPIO_MODE1);
	*addr |= GPIO_MODE_OUTPUT << (15 << 1);

	addr = (volatile u32 *)IO_ADDRESS(RUBY_GPIO_REGS_ADDR + GPIO_MODE2);
	*addr |= (GPIO_MODE_OUTPUT << (9 << 1)) | (GPIO_MODE_OUTPUT << (13 << 1));

	addr = (volatile u32 *)IO_ADDRESS(RUBY_GPIO_REGS_ADDR + GPIO_OUTPUT_MASK);
	*addr |= (1 << 15) | (1 << 25);

	addr = (volatile u32 *)IO_ADDRESS(RUBY_GPIO_REGS_ADDR + GPIO_OUTPUT);
	*addr |= (1 << 15) | (1 << 25);

	addr = (volatile u32 *)IO_ADDRESS(RUBY_GPIO_REGS_ADDR + GPIO_ALTFN);
	*addr |= (1 << 29);
}

