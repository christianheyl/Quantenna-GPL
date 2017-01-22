/*
 * (C) Copyright 2010 Quantenna Communications Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/*
 * Header file which describes Ruby platform.
 * Has to be used by both kernel and bootloader.
 */

#ifndef __RUBY_PLATFORM_H
#define __RUBY_PLATFORM_H

#include "ruby_config.h"

/*****************************************************************************/
/*****************************************************************************/
/* Common                                                                    */
/*****************************************************************************/
#define RUBY_BIT(x)			(1 << (x))
/*****************************************************************************/
/* DRAM registers                                                            */
/*****************************************************************************/
#define RUBY_DDR_BASE_ADDR		TOPAZ_ALIAS_MAP_SWITCH(0xF6000000, 0xE40E0000)
#define RUBY_DDR_CONTROL		(RUBY_DDR_BASE_ADDR + 0x0)
#define RUBY_DDR_CONTROL_POWERDOWN_EN	RUBY_BIT(1)
#define RUBY_DDR_SETTLE_US		(4)
/*****************************************************************************/
/*****************************************************************************/
/* GPIO constants                                                            */
/*****************************************************************************/
#define RUBY_GPIO_MAX			(32)
#define RUBY_GPIO_MODE1_MAX		(11)
#define RUBY_GPIO_MODE2_MAX		(22)
#define RUBY_GPIO_IRQ_MAX		(16)
/*****************************************************************************/
/* GPIO registers                                                            */
/*****************************************************************************/
#define RUBY_GPIO_REGS_ADDR		TOPAZ_ALIAS_MAP_SWITCH(0xF1000000, 0xE4090000)
#define RUBY_GPIO_INPUT			(RUBY_GPIO_REGS_ADDR + 0x00)
#define GPIO_INPUT			RUBY_GPIO_INPUT
#define RUBY_GPIO_OMASK			(RUBY_GPIO_REGS_ADDR + 0x04)
#define GPIO_OUTPUT_MASK		RUBY_GPIO_OMASK
#define RUBY_GPIO_OUTPUT		(RUBY_GPIO_REGS_ADDR + 0x08)
#define GPIO_OUTPUT			RUBY_GPIO_OUTPUT
#define RUBY_GPIO_MODE1			(RUBY_GPIO_REGS_ADDR + 0x0c)
#define GPIO_MODE1			RUBY_GPIO_MODE1
#define RUBY_GPIO_MODE2			(RUBY_GPIO_REGS_ADDR + 0x10)
#define GPIO_MODE2			RUBY_GPIO_MODE2
#define RUBY_GPIO_AFSEL			(RUBY_GPIO_REGS_ADDR + 0x14)
#define GPIO_ALTFN			RUBY_GPIO_AFSEL
#define RUBY_GPIO_DEF			(RUBY_GPIO_REGS_ADDR + 0x18)
#define	RUBY_GPIO1_PWM0			(RUBY_GPIO_REGS_ADDR + 0x20) /* AFSEL: UART1 (input) */
#define	RUBY_GPIO9_PWM2			(RUBY_GPIO_REGS_ADDR + 0x28) /* AFSEL: UART1 (output) */
/*****************************************************************************/
/* GPIO pins                                                                 */
/*****************************************************************************/
#define RUBY_GPIO_PIN0			(0)
#define RUBY_GPIO_PIN1			(1)
#define RUBY_GPIO_PIN2			(2)
#define RUBY_GPIO_PIN3			(3)
#define RUBY_GPIO_PIN4			(4)
#define RUBY_GPIO_PIN5			(5)
#define RUBY_GPIO_PIN6			(6)
#define RUBY_GPIO_PIN7			(7)
#define RUBY_GPIO_PIN8			(8)
#define RUBY_GPIO_PIN9			(9)
#define RUBY_GPIO_PIN10			(10)
#define RUBY_GPIO_PIN11			(11)
#define RUBY_GPIO_PIN12			(12)
#define RUBY_GPIO_PIN13			(13)
#define RUBY_GPIO_PIN14			(14)
#define RUBY_GPIO_PIN15			(15)
#define RUBY_GPIO_PIN16			(16)
#define RUBY_GPIO_PIN17			(17)

#define RUBY_GPIO_UART0_SI		(0)
#define RUBY_GPIO_UART0_SO		(8)
#define RUBY_GPIO_UART1_SI		(1)
#define RUBY_GPIO_UART1_SO		(9)
/* these are for spi1, bga has dedicated spi0 pins */
#define RUBY_GPIO_SPI_MISO		(2)
#define RUBY_GPIO_SPI_SCK		(6)
#define RUBY_GPIO_SPI_MOSI		(5)
#define RUBY_GPIO_SPI_nCS		(4)

#define RUBY_GPIO_LNA_TOGGLE		(7)

#define RUBY_GPIO_RTD			(3)	/* Reset to default */
#define RUBY_GPIO_WPS			(7)
#define RUBY_GPIO_I2C_SCL		(10)
#define RUBY_GPIO_I2C_SDA		(11)
#define RUBY_GPIO_WLAN_DISABLE		(12)
#define RUBY_GPIO_LED1			(13)
#define RUBY_GPIO_RFIC_INTR		(14)

#ifndef TOPAZ_AMBER_IP
#define RUBY_GPIO_RFIC_RESET		(15)
#else
#define RUBY_GPIO_RFIC_RESET		(10)
#endif

#define RUBY_GPIO_LED2			(16)

/*****************************************************************************/
/* GPIO function constants                                                   */
/*****************************************************************************/
#define RUBY_GPIO_MODE_INPUT		(0)
#define GPIO_MODE_INPUT				RUBY_GPIO_MODE_INPUT
#define RUBY_GPIO_MODE_OUTPUT		(1)
#define GPIO_MODE_OUTPUT			RUBY_GPIO_MODE_OUTPUT
#define RUBY_GPIO_MODE_OPEN_SOURCE	(2)
#define RUBY_GPIO_MODE_OPEN_DRAIN	(3)
#define RUBY_GPIO_ALT_INPUT			(4)
#define RUBY_GPIO_ALT_OUTPUT		(5)
#define RUBY_GPIO_ALT_OPEN_SOURCE	(6)
#define RUBY_GPIO_ALT_OPEN_DRAIN	(7)
#define	GPIO_PIN(x)			(x)
#define	GPIO_OUTPUT_LO			(0)
#define	GPIO_OUTPUT_HI			(1)
/*****************************************************************************/
/*****************************************************************************/
/* UART FIFO size                                                            */
/*****************************************************************************/
#define RUBY_UART_FIFO_SIZE		(16)
/*****************************************************************************/
/* UART register addresses                                                   */
/*****************************************************************************/
#define RUBY_UART0_BASE_ADDR		TOPAZ_ALIAS_MAP_SWITCH(0xF0000000, 0xE4080000)
#define RUBY_UART0_RBR_THR_DLL		(RUBY_UART0_BASE_ADDR + 0x00)
#define RUBY_UART0_DLH_IER		(RUBY_UART0_BASE_ADDR + 0x04)
#define RUBY_UART0_IIR_FCR_LCR		(RUBY_UART0_BASE_ADDR + 0x08)
#define RUBY_UART0_LCR			(RUBY_UART0_BASE_ADDR + 0x0c)
#define RUBY_UART0_MCR			(RUBY_UART0_BASE_ADDR + 0x10)
#define RUBY_UART0_LSR			(RUBY_UART0_BASE_ADDR + 0x14)
#define RUBY_UART0_MSR			(RUBY_UART0_BASE_ADDR + 0x18)
#define RUBY_UART0_SCR			(RUBY_UART0_BASE_ADDR + 0x1c)
#define RUBY_UART0_LPDLL		(RUBY_UART0_BASE_ADDR + 0x20)
#define RUBY_UART0_LPDLH		(RUBY_UART0_BASE_ADDR + 0x24)
#define RUBY_UART0_SRBR			(RUBY_UART0_BASE_ADDR + 0x30)
#define RUBY_UART0_STHR			(RUBY_UART0_BASE_ADDR + 0x34)
#define RUBY_UART0_FAR			(RUBY_UART0_BASE_ADDR + 0x70)
#define RUBY_UART0_TFR			(RUBY_UART0_BASE_ADDR + 0x74)
#define RUBY_UART0_RFW			(RUBY_UART0_BASE_ADDR + 0x78)
#define RUBY_UART0_USR			(RUBY_UART0_BASE_ADDR + 0x7c)
#define RUBY_UART0_TFL			(RUBY_UART0_BASE_ADDR + 0x80)
#define RUBY_UART0_RFL			(RUBY_UART0_BASE_ADDR + 0x84)
#define RUBY_UART0_SRR			(RUBY_UART0_BASE_ADDR + 0x88)
#define RUBY_UART0_SRTS			(RUBY_UART0_BASE_ADDR + 0x8c)
#define RUBY_UART0_SBCR			(RUBY_UART0_BASE_ADDR + 0x90)
#define RUBY_UART0_SDMAM		(RUBY_UART0_BASE_ADDR + 0x94)
#define RUBY_UART0_SFE			(RUBY_UART0_BASE_ADDR + 0x98)
#define RUBY_UART0_SRT			(RUBY_UART0_BASE_ADDR + 0x9c)
#define RUBY_UART0_STET			(RUBY_UART0_BASE_ADDR + 0xa0)
#define RUBY_UART0_HTX			(RUBY_UART0_BASE_ADDR + 0xa4)
#define RUBY_UART0_DMASA		(RUBY_UART0_BASE_ADDR + 0xa8)
#define RUBY_UART0_CPR			(RUBY_UART0_BASE_ADDR + 0xf4)
#define RUBY_UART0_UCV			(RUBY_UART0_BASE_ADDR + 0xf8)
#define RUBY_UART0_CTR			(RUBY_UART0_BASE_ADDR + 0xfc)

#define RUBY_UART1_BASE_ADDR		TOPAZ_ALIAS_MAP_SWITCH(0xF5000000, 0xE40D0000)
#define RUBY_UART1_RBR_THR_DLL		(RUBY_UART1_BASE_ADDR + 0x00)
#define RUBY_UART1_DLH_IER		(RUBY_UART1_BASE_ADDR + 0x04)
#define RUBY_UART1_IIR_FCR_LCR		(RUBY_UART1_BASE_ADDR + 0x08)
#define RUBY_UART1_LCR			(RUBY_UART1_BASE_ADDR + 0x0c)
#define RUBY_UART1_MCR			(RUBY_UART1_BASE_ADDR + 0x10)
#define RUBY_UART1_LSR			(RUBY_UART1_BASE_ADDR + 0x14)
#define RUBY_UART1_MSR			(RUBY_UART1_BASE_ADDR + 0x18)
#define RUBY_UART1_SCR			(RUBY_UART1_BASE_ADDR + 0x1c)
#define RUBY_UART1_LPDLL		(RUBY_UART1_BASE_ADDR + 0x20)
#define RUBY_UART1_LPDLH		(RUBY_UART1_BASE_ADDR + 0x24)
#define RUBY_UART1_SRBR			(RUBY_UART1_BASE_ADDR + 0x30)
#define RUBY_UART1_STHR			(RUBY_UART1_BASE_ADDR + 0x34)
#define RUBY_UART1_FAR			(RUBY_UART1_BASE_ADDR + 0x70)
#define RUBY_UART1_TFR			(RUBY_UART1_BASE_ADDR + 0x74)
#define RUBY_UART1_RFW			(RUBY_UART1_BASE_ADDR + 0x78)
#define RUBY_UART1_USR			(RUBY_UART1_BASE_ADDR + 0x7c)
#define RUBY_UART1_TFL			(RUBY_UART1_BASE_ADDR + 0x80)
#define RUBY_UART1_RFL			(RUBY_UART1_BASE_ADDR + 0x84)
#define RUBY_UART1_SRR			(RUBY_UART1_BASE_ADDR + 0x88)
#define RUBY_UART1_SRTS			(RUBY_UART1_BASE_ADDR + 0x8c)
#define RUBY_UART1_SBCR			(RUBY_UART1_BASE_ADDR + 0x90)
#define RUBY_UART1_SDMAM		(RUBY_UART1_BASE_ADDR + 0x94)
#define RUBY_UART1_SFE			(RUBY_UART1_BASE_ADDR + 0x98)
#define RUBY_UART1_SRT			(RUBY_UART1_BASE_ADDR + 0x9c)
#define RUBY_UART1_STET			(RUBY_UART1_BASE_ADDR + 0xa0)
#define RUBY_UART1_HTX			(RUBY_UART1_BASE_ADDR + 0xa4)
#define RUBY_UART1_DMASA		(RUBY_UART1_BASE_ADDR + 0xa8)
#define RUBY_UART1_CPR			(RUBY_UART1_BASE_ADDR + 0xf4)
#define RUBY_UART1_UCV			(RUBY_UART1_BASE_ADDR + 0xf8)
#define RUBY_UART1_CTR			(RUBY_UART1_BASE_ADDR + 0xfc)

/*****************************************************************************/
/* UART Status Register - USR                                                */
/*****************************************************************************/
#define RUBY_USR_TX_Fifo_Empty		0x04
#define RUBY_USR_RX_Fifo_Full		0x10
#define RUBY_USR_TX_Fifo_nFull		0x02
#define RUBY_USR_RX_Fifo_nEmpty		0x08
#define RUBY_USR_Busy			0x01
/*****************************************************************************/
/* Line Status Register - LSR                                                */
/*****************************************************************************/
#define RUBY_LSR_TX_Empty		0x40
#define RUBY_LSR_RX_Ready		0x01
/*****************************************************************************/
/* Line Control Register - LCR                                               */
/*****************************************************************************/
#define RUBY_LCR_Data_Word_Length_5	0x0
#define RUBY_LCR_Data_Word_Length_6	0x1
#define RUBY_LCR_Data_Word_Length_7	0x2
#define RUBY_LCR_Data_Word_Length_8	0x3
#define RUBY_LCR_Stop_Bit_1		0x0
#define RUBY_LCR_Stop_Bit_2		0x4
#define RUBY_LCR_No_Parity		0x0
#define RUBY_LCR_Odd_Parity		0x8
#define RUBY_LCR_Even_Parity		0x18
#define RUBY_LCR_High_Parity		0x28
#define RUBY_LCR_Low_Parity		0x38
#define RUBY_LCR_Break_Disable		0x0
#define RUBY_LCR_Break_Enable		0x40
#define RUBY_LCR_DLAB			0x80
/*****************************************************************************/
/*****************************************************************************/
/* Timer constants                                                           */
/*****************************************************************************/
#define RUBY_TIMER_INT_MASK		(RUBY_BIT(2))
#define RUBY_TIMER_SINGLE		(RUBY_BIT(1))
#define RUBY_TIMER_ENABLE		(RUBY_BIT(0))
/*****************************************************************************/
#define RUBY_CPU_TIMERS			(2)
#define RUBY_NUM_TIMERS			(4)
#define RUBY_TIMER_MUC_CCA              (3)
#define RUBY_TIMER_FREQ                 125000000

#define RUBY_TIMER_MUC_CCA_FREQ_SHIFT    2    /* shift from 1ms base */
#define RUBY_TIMER_MUC_CCA_FREQ          (1000 << RUBY_TIMER_MUC_CCA_FREQ_SHIFT)
#define RUBY_TIMER_MUC_CCA_LIMIT         (RUBY_TIMER_FREQ / RUBY_TIMER_MUC_CCA_FREQ)
#define RUBY_TIMER_MUC_CCA_INTV          (1000 >> RUBY_TIMER_MUC_CCA_FREQ_SHIFT)    /* microseconds */
#define RUBY_TIMER_MUC_CCA_CNT2MS(_v)    ((_v) >> RUBY_TIMER_MUC_CCA_FREQ_SHIFT)
/*****************************************************************************/
#define RUBY_TIMER_BASE_ADDR		TOPAZ_ALIAS_MAP_SWITCH(0xF3000000, 0xE40B0000)
#define RUBY_TIMER_CHANNEL		(0x14)
#define RUBY_TIMER_LOAD_COUNT(x)	(RUBY_TIMER_BASE_ADDR + ((x)*RUBY_TIMER_CHANNEL) + 0)
#define RUBY_TIMER_VALUE(x)		(RUBY_TIMER_BASE_ADDR + ((x)*RUBY_TIMER_CHANNEL) + 4)
#define RUBY_TIMER_CONTROL(x)		(RUBY_TIMER_BASE_ADDR + ((x)*RUBY_TIMER_CHANNEL) + 8)
#define RUBY_TIMER_EOI(x)		(RUBY_TIMER_BASE_ADDR + ((x)*RUBY_TIMER_CHANNEL) + 12)
#define RUBY_TIMER_INTSTAT(x)		(RUBY_TIMER_BASE_ADDR + ((x)*RUBY_TIMER_CHANNEL) + 16)
/*****************************************************************************/
#define RUBY_TIMER_GLOBAL_INT_STATUS	(RUBY_TIMER_BASE_ADDR + 0xa0)
#define RUBY_TIMER_GLOBAL_EOI		(RUBY_TIMER_BASE_ADDR + 0xa4)
#define RUBY_TIMER_GLOBAL_RAW_STATUS	(RUBY_TIMER_BASE_ADDR + 0xa8)
#define RUBY_TIMER_GLOBAL_COMP_VER	(RUBY_TIMER_BASE_ADDR + 0xac)
/*****************************************************************************/
#define RUBY_TIMER_ORINT_EN(x)		(1 << (18 + (x)))
/*****************************************************************************/
/*****************************************************************************/
/* ENET registers                                                            */
/*****************************************************************************/
#define RUBY_ENET0_BASE_ADDR		TOPAZ_ALIAS_MAP_SWITCH(0xED000000, 0xE4070000)
#define RUBY_ENET1_BASE_ADDR		TOPAZ_ALIAS_MAP_SWITCH(0xE8000000, 0xE4040000)
/*****************************************************************************/
/*****************************************************************************/
/* System controller registers                                               */
/*****************************************************************************/
#define RUBY_SYS_CTL_BASE_ADDR_NOMAP		0xE0000000
#define RUBY_SYS_CTL_BASE_ADDR			TOPAZ_ALIAS_MAP_SWITCH(RUBY_SYS_CTL_BASE_ADDR_NOMAP, 0xE4000000)
#define RUBY_SYS_CTL_CPU_VEC_MASK		(RUBY_SYS_CTL_BASE_ADDR + 0x00)
#define RUBY_SYS_CTL_CPU_VEC			(RUBY_SYS_CTL_BASE_ADDR + 0x04)
#define RUBY_SYS_CTL_MASK			(RUBY_SYS_CTL_BASE_ADDR + 0x08)
#define RUBY_SYS_CTL_CTRL			(RUBY_SYS_CTL_BASE_ADDR + 0x0c)
#define RUBY_SYS_CTL_RESET_CAUSE		(RUBY_SYS_CTL_BASE_ADDR + 0x10)
#define RUBY_SYS_CTL_CSR			(RUBY_SYS_CTL_BASE_ADDR + 0x14)
#define RUBY_SYS_CTL_DEBUG_SEL			(RUBY_SYS_CTL_BASE_ADDR + 0x18)
#define RUBY_SYS_CTL_L2M_INT			(RUBY_SYS_CTL_BASE_ADDR + 0x1C)
#define RUBY_SYS_CTL_L2M_INT_MASK		(RUBY_SYS_CTL_BASE_ADDR + 0x20)
#define RUBY_SYS_CTL_L2D_INT			(RUBY_SYS_CTL_BASE_ADDR + PLATFORM_REG_SWITCH(0x24, 0x34))
#define RUBY_SYS_CTL_L2D_INT_MASK		(RUBY_SYS_CTL_BASE_ADDR + PLATFORM_REG_SWITCH(0x28, 0x38))
#define RUBY_SYS_CTL_M2L_INT			(RUBY_SYS_CTL_BASE_ADDR + 0x2C)
#define RUBY_SYS_CTL_M2L_INT_MASK		(RUBY_SYS_CTL_BASE_ADDR + 0x30)
#define RUBY_SYS_CTL_M2D_INT			(RUBY_SYS_CTL_BASE_ADDR + PLATFORM_REG_SWITCH(0x34, 0x24))
#define RUBY_SYS_CTL_M2D_INT_MASK		(RUBY_SYS_CTL_BASE_ADDR + PLATFORM_REG_SWITCH(0x38, 0x28))
#define RUBY_SYS_CTL_D2L_INT			(RUBY_SYS_CTL_BASE_ADDR + 0x3C)
#define RUBY_SYS_CTL_D2L_INT_MASK		(RUBY_SYS_CTL_BASE_ADDR + 0x40)
#define RUBY_SYS_CTL_D2M_INT			(RUBY_SYS_CTL_BASE_ADDR + 0x44)
#define RUBY_SYS_CTL_D2M_INT_MASK		(RUBY_SYS_CTL_BASE_ADDR + 0x48)
#define RUBY_SYS_CTL_LHOST_INT_EN		(RUBY_SYS_CTL_BASE_ADDR + 0x4C)
#define RUBY_SYS_CTL_MUC_INT_EN			(RUBY_SYS_CTL_BASE_ADDR + 0x50)
#define RUBY_SYS_CTL_DSP_INT_EN			(RUBY_SYS_CTL_BASE_ADDR + 0x54)
#define RUBY_SYS_CTL_LHOST_ORINT_EN		(RUBY_SYS_CTL_BASE_ADDR + 0x58)
#define RUBY_SYS_CTL_MUC_ORINT_EN		(RUBY_SYS_CTL_BASE_ADDR + 0x5C)
#define RUBY_SYS_CTL_DSP_ORINT_EN		(RUBY_SYS_CTL_BASE_ADDR + 0x60)
#define RUBY_SYS_CTL_MUC_REMAP			(RUBY_SYS_CTL_BASE_ADDR + 0x64)
#define RUBY_SYS_CTL_DSP_REMAP			(RUBY_SYS_CTL_BASE_ADDR + 0x68)
#define RUBY_SYS_CTL_PCIE_CFG0			(RUBY_SYS_CTL_BASE_ADDR + 0x6C)
#define RUBY_SYS_CTL_PCIE_CFG1			(RUBY_SYS_CTL_BASE_ADDR + 0x70)
#define RUBY_SYS_CTL_PCIE_CFG2			(RUBY_SYS_CTL_BASE_ADDR + 0x74)
#define RUBY_SYS_CTL_PCIE_CFG3			(RUBY_SYS_CTL_BASE_ADDR + 0x78)
#define RUBY_SYS_CTL_PCIE_CFG4			(RUBY_SYS_CTL_BASE_ADDR + 0x7C)
#define RUBY_SYS_CTL_PLL0_CTRL			(RUBY_SYS_CTL_BASE_ADDR + 0x80)
#define RUBY_SYS_CTL_PLL1_CTRL			(RUBY_SYS_CTL_BASE_ADDR + 0x84)
#define RUBY_SYS_CTL_LHOST_ID			(RUBY_SYS_CTL_BASE_ADDR + 0x88)
#define RUBY_SYS_CTL_PLL2_CTRL			(RUBY_SYS_CTL_BASE_ADDR + 0x8C)
#define RUBY_SYS_CTL_MUC_ID			(RUBY_SYS_CTL_BASE_ADDR + 0x90)
#define RUBY_SYS_CTL_L2M_SEM			(RUBY_SYS_CTL_BASE_ADDR + 0x94)
#define RUBY_SYS_CTL_M2L_SEM			(RUBY_SYS_CTL_BASE_ADDR + 0x98)
#define RUBY_SYS_CTL_L2D_SEM			(RUBY_SYS_CTL_BASE_ADDR + 0x9C)
#define RUBY_SYS_CTL_D2L_SEM			(RUBY_SYS_CTL_BASE_ADDR + 0xA0)
#define RUBY_SYS_CTL_M2D_SEM			(RUBY_SYS_CTL_BASE_ADDR + 0xA4)
#define RUBY_SYS_CTL_D2M_SEM			(RUBY_SYS_CTL_BASE_ADDR + 0xA8)
#define RUBY_SYS_CTL_INTR_INV0			(RUBY_SYS_CTL_BASE_ADDR + 0xAC)
#define RUBY_SYS_CTL_INTR_INV1			(RUBY_SYS_CTL_BASE_ADDR + 0xB0)
#define RUBY_SYS_CTL_GMII_CLKDLL		(RUBY_SYS_CTL_BASE_ADDR + 0xB4)
#define RUBY_SYS_CTL_DEBUG_BUS			(RUBY_SYS_CTL_BASE_ADDR + 0xB8)
#define RUBY_SYS_CTL_SPARE			(RUBY_SYS_CTL_BASE_ADDR + 0xBC)
#define RUBY_SYS_CTL_PCIE_INT_MASK		(RUBY_SYS_CTL_BASE_ADDR + 0xC0)
#define	RUBY_SYS_CTL_GPIO_IRQ_SEL		(RUBY_SYS_CTL_BASE_ADDR + 0xc4)
#define RUBY_SYS_CTL_PCIE_SLV_REQ_MISC_INFO	(RUBY_SYS_CTL_BASE_ADDR + 0xCC)
#define RUBY_SYS_CTL_DDR_CTRL			(RUBY_SYS_CTL_BASE_ADDR + 0xE8)
#define RUBY_SYS_CTL_GPIO_INT_STATUS		(RUBY_SYS_CTL_BASE_ADDR + 0x154)
#define RUBY_SYS_AHB_MON_INT_MASK		(RUBY_SYS_CTL_BASE_ADDR + 0x160)
#define RUBY_SYS_CTL_BOND_OPT			(RUBY_SYS_CTL_BASE_ADDR + 0x16C)

/*****************************************************************************/
/* System controller constants                                               */
/*****************************************************************************/
#define RUBY_SYS_CTL_REMAP(x)		(((x) & 0x3) << 3)
#define RUBY_SYS_CTL_LINUX_MAP(x)	(((x) & 0x1) << 31)
#define RUBY_SYS_CTL_SPICLK(x)		(((x) & 0x3) << 15)
#define RUBY_SYS_CTL_CLKSEL(x)		(((x) & 0x3) << 5)
#define RUBY_SYS_CTL_MUC_REMAP_SHIFT	15
#define RUBY_SYS_CTL_MUC_REMAP_VAL(x)	(RUBY_BIT(31) | ((x) >> RUBY_SYS_CTL_MUC_REMAP_SHIFT))
#define RUBY_SYS_CTL_DSP_REMAP_SHIFT	15
#define RUBY_SYS_CTL_DSP_REMAP_VAL(x)	(RUBY_BIT(31) | ((x) >> RUBY_SYS_CTL_DSP_REMAP_SHIFT))
/* reset bits - names match rtl */
#define RUBY_SYS_CTL_RESET_LHOST_CORE	(RUBY_BIT(0))
#define RUBY_SYS_CTL_RESET_LHOST_BUS	(RUBY_BIT(1))
#define RUBY_SYS_CTL_RESET_DDR		(RUBY_BIT(2))
#define RUBY_SYS_CTL_RESET_SRAM		(RUBY_BIT(3))
#define RUBY_SYS_CTL_RESET_DSP		(RUBY_BIT(4))
#define RUBY_SYS_CTL_RESET_IOSS		(RUBY_BIT(5))
#define RUBY_SYS_CTL_RESET_NETSS	(RUBY_BIT(7))
#define RUBY_SYS_CTL_RESET_MAC		(RUBY_BIT(8))
#define RUBY_SYS_CTL_RESET_ENET0	(RUBY_BIT(9))
#define RUBY_SYS_CTL_RESET_MUC		(RUBY_BIT(11))
#define RUBY_SYS_CTL_RESET_ENET1	(RUBY_BIT(12))
#define RUBY_SYS_CTL_RESET_PCIE		(RUBY_BIT(13))
#define RUBY_SYS_CTL_RESET_BB		(RUBY_BIT(14))
#define RUBY_SYS_CTL_RESET_EXT		(RUBY_BIT(15))
/* reset useful constants */
#define RUBY_SYS_CTL_RESET_ALL		(~0x0)
#define RUBY_SYS_CTL_RESET_MUC_ALL	RUBY_SYS_CTL_RESET_MUC
#define RUBY_SYS_CTL_RESET_DSP_ALL	RUBY_SYS_CTL_RESET_DSP
/* reset cause definitions */
#define	RUBY_SYS_CTL_RESET_CAUSE_PO	(RUBY_BIT(0))
#define	RUBY_SYS_CTL_RESET_CAUSE_SR	(RUBY_BIT(1))
#define	RUBY_SYS_CTL_RESET_CAUSE_WD	(RUBY_BIT(2))
#define RUBY_SYS_CTL_INTR_TIMER_MSK(t)	(1 << (20 + (t)))
/* sysctl vector/mask bit definitions */
#define RUBY_SYS_CTL_MASK_BOOTMODE	(0x7 << 0)
#define RUBY_SYS_CTL_MASK_REMAP		(0x3 << 3)
#define RUBY_SYS_CTL_MASK_CLKSEL	(0x3 << 5)
/* clksel: 00 = cpu(400)      bus(200) */
#define RUBY_SYS_CTL_CLKSEL_00_BUS_FREQ	200000000
/* clksel: 01 = cpu(320)      bus(160) */
#define RUBY_SYS_CTL_CLKSEL_01_BUS_FREQ	160000000
/* clksel: 10 = cpu(250)      bus(125) */
#define RUBY_SYS_CTL_CLKSEL_10_BUS_FREQ	125000000
/* clksel: 11 = cpu(200)      bus(100) */
#define RUBY_SYS_CTL_CLKSEL_11_BUS_FREQ	100000000
#define RUBY_SYS_CTL_MASK_DDRDRV	(0x1 << 7)
#define RUBY_SYS_CTL_MASK_DDRODT	(0x3 << 8)
#define RUBY_SYS_CTL_MASK_NODDR		(0x1 << 12)
#define RUBY_SYS_CTL_MASK_MII		(0x3 << 13)
#define RUBY_SYS_CTL_MASK_MII_EMAC0	(0x1 << 13)
#define RUBY_SYS_CTL_MASK_MII_EMAC1	(0x1 << 14)
#define RUBY_SYS_CTL_MASK_SPICLK	(0x3 << 15)
#define RUBY_SYS_CTL_MASK_JTAGCHAIN	(0x1 << 17)

#define RUBY_SYS_CTL_MASK_GMII0_TXCLK	(0x3 << 18)
#define RUBY_SYS_CTL_MASK_GMII0_10M	(0x0 << 18)
#define RUBY_SYS_CTL_MASK_GMII0_100M	(0x1 << 18)
#define RUBY_SYS_CTL_MASK_GMII0_1000M	(0x2 << 18)

#define RUBY_SYS_CTL_MASK_GMII1_TXCLK	(0x3 << 20)
#define RUBY_SYS_CTL_MASK_GMII1_10M	(0x0 << 20)
#define RUBY_SYS_CTL_MASK_GMII1_100M	(0x1 << 20)
#define RUBY_SYS_CTL_MASK_GMII1_1000M	(0x2 << 20)

#define RUBY_SYS_CTL_MASK_GMII_10M	(0)
#define RUBY_SYS_CTL_MASK_GMII_100M	(1)
#define RUBY_SYS_CTL_MASK_GMII_1000M	(2)
#define RUBY_SYS_CTL_MASK_GMII_TXCLK	(3)
#define RUBY_SYS_CTL_MASK_GMII0_SHIFT	(18)
#define RUBY_SYS_CTL_MASK_GMII1_SHIFT	(20)

#define RUBY_SYS_CTL_MASK_DDRCLK	(0x7 << 22)
#define RUBY_SYS_CTL_MASK_LINUX_MAP	(0x1 << 31)

#define RUBY_RESET_CAUSE_UART_SHIFT	(7)
#define RUBY_RESET_CAUSE_UART(x)	(1 << (RUBY_RESET_CAUSE_UART_SHIFT + x))

/* global[30:25,11] unused */
/* for compatibility */
#define SYSCTRL_CTRL_MASK		(RUBY_SYS_CTL_MASK - RUBY_SYS_CTL_BASE_ADDR)
#define SYSCTRL_CTRL			(RUBY_SYS_CTL_CTRL - RUBY_SYS_CTL_BASE_ADDR)
#define SYSCTRL_RESET_MASK		(RUBY_SYS_CTL_CPU_VEC_MASK - RUBY_SYS_CTL_BASE_ADDR)
#define SYSCTRL_RESET			(RUBY_SYS_CTL_CPU_VEC - RUBY_SYS_CTL_BASE_ADDR)
#define SYSCTRL_REV_NUMBER		(RUBY_SYS_CTL_CSR - RUBY_SYS_CTL_BASE_ADDR)
#define SYSCTRL_RGMII_DLL		(RUBY_SYS_CTL_GMII_CLKDLL - RUBY_SYS_CTL_BASE_ADDR)
/*****************************************************************************/
/*****************************************************************************/
/* Watchdog registers                                                        */
/*****************************************************************************/
#define RUBY_WDT_BASE_ADDR		TOPAZ_ALIAS_MAP_SWITCH(0xF4000000, 0xE40C0000)
#define RUBY_WDT_CTL			(RUBY_WDT_BASE_ADDR + 0x00)
#define RUBY_WDT_TIMEOUT_RANGE		(RUBY_WDT_BASE_ADDR + 0x04)
#define RUBY_WDT_CURRENT_VALUE		(RUBY_WDT_BASE_ADDR + 0x08)
#define RUBY_WDT_COUNTER_RESTART	(RUBY_WDT_BASE_ADDR + 0x0c)
#define RUBY_WDT_INT_STAT		(RUBY_WDT_BASE_ADDR + 0x10)
#define RUBY_WDT_INT_CLEAR		(RUBY_WDT_BASE_ADDR + 0x14)
/*****************************************************************************/
/* Watchdog constants                                                        */
/*****************************************************************************/
#define RUBY_WDT_ENABLE_IRQ_WARN	(RUBY_BIT(1))
#define RUBY_WDT_ENABLE			(RUBY_BIT(0))
#define RUBY_WDT_MAGIC_NUMBER		(0x76)
#define RUBY_WDT_MAX_TIMEOUT		(0xF)
#define RUBY_WDT_RESET_TIMEOUT		(0x8)
/*****************************************************************************/
/*****************************************************************************/
/* SPI registers                                                             */
/*****************************************************************************/
#define RUBY_SPI_BASE_ADDR		TOPAZ_ALIAS_MAP_SWITCH(0xE2000000, 0xE4030000)
#define RUBY_SPI_COMMIT			(RUBY_SPI_BASE_ADDR + 0x0000)
#define RUBY_SPI_CONTROL		(RUBY_SPI_BASE_ADDR + 0x0004)
#define RUBY_SPI_WRITE_STATUS		(RUBY_SPI_BASE_ADDR + 0x0100)
#define RUBY_SPI_PAGE_PROGRAM		(RUBY_SPI_BASE_ADDR + 0x0200)
#define RUBY_SPI_WRITE_DIS		(RUBY_SPI_BASE_ADDR + 0x0400)
#define RUBY_SPI_READ_STATUS		(RUBY_SPI_BASE_ADDR + 0x0500)
#define RUBY_SPI_WRITE_EN		(RUBY_SPI_BASE_ADDR + 0x0600)
#define RUBY_SPI_FAST_READ		(RUBY_SPI_BASE_ADDR + 0x0B00)
#define RUBY_SPI_WRITE_REG3		(RUBY_SPI_BASE_ADDR + 0x1100)
#define RUBY_SPI_READ_REG3              (RUBY_SPI_BASE_ADDR + 0x1500)
#define RUBY_SPI_SECTOR_ERASE_20	(RUBY_SPI_BASE_ADDR + 0x2000)
#define RUBY_SPI_READ_SCUR              (RUBY_SPI_BASE_ADDR + 0x2b00)
#define RUBY_SPI_WRITE_IBUP		(RUBY_SPI_BASE_ADDR + 0x3900)
#define RUBY_SPI_WRITE_PRO_SEL          (RUBY_SPI_BASE_ADDR + 0x6800)
#define RUBY_SPI_GBLOCK_LOCK            (RUBY_SPI_BASE_ADDR + 0x7e00)
#define RUBY_SPI_GBLOCK_UNLOCK          (RUBY_SPI_BASE_ADDR + 0x9800)
#define TOPAZ_SPI_GBLOCK_UNLOCK		(RUBY_SPI_BASE_ADDR + 0x9800)
#define RUBY_SPI_READ_ID		(RUBY_SPI_BASE_ADDR + 0x9F00)
#define RUBY_SPI_BULK_ERASE		(RUBY_SPI_BASE_ADDR + 0xC700)
#define RUBY_SPI_SECTOR_ERASE_D8	(RUBY_SPI_BASE_ADDR + 0xD800)
#define RUBY_SPI_READ_DPB               (RUBY_SPI_BASE_ADDR + 0xe000)
#define RUBY_SPI_WRITE_DPB              (RUBY_SPI_BASE_ADDR + 0xe100)
#define RUBY_SPI_PAGE_PROGRAM_4B	(RUBY_SPI_BASE_ADDR + 0x1200)
#define RUBY_SPI_SECTOR_ERASE_D8_4B	(RUBY_SPI_BASE_ADDR + 0xDC00)
#define RUBY_SPI_SECTOR_ERASE_20_4B	(RUBY_SPI_BASE_ADDR + 0x2100)
#define RUBY_SPI_ADDRESS_MODE_4B	0x85
#define RUBY_SPI_BOUNDARY_4B		0x1000000

/*
 * UBOOT_VERSION_LOCATION:
 * This is hardwired in u-boot's start.S; the first instruction generates a
 * 32 bit branch instruction.  The next several locations holds a human
 * readable ascii version string that is visible in the file and in memory.
 * The branch target of the first instruction is the next 4 byte aligned
 * address following the version string.
 *
 */
#define UBOOT_VERSION_LOCATION		(RUBY_SPI_FLASH_ADDR + 4)

/*****************************************************************************/
/* SPI constants                                                             */
/*****************************************************************************/
#define RUBY_SPI_WR_IN_PROGRESS		(RUBY_BIT(0))
#define RUBY_SPI_PROTECTION		(0x3C)

/*****************************************************************************/
/* SPI1 registers                                                             */
/*****************************************************************************/
#define RUBY_SPI1_BASE_ADDR		TOPAZ_ALIAS_MAP_SWITCH(0xF2000000, 0xE40A0000)
#define RUBY_SPI1_SPCR			(RUBY_SPI1_BASE_ADDR + 0x0000)
#define RUBY_SPI1_SPSR			(RUBY_SPI1_BASE_ADDR + 0x0004)
#define RUBY_SPI1_SPDR			(RUBY_SPI1_BASE_ADDR + 0x0008)
#define RUBY_SPI1_SPER			(RUBY_SPI1_BASE_ADDR + 0x000C)
#define RUBY_SPI1_SLVN			(RUBY_SPI1_BASE_ADDR + 0x0010)
/*****************************************************************************/
/* SPI1 constants                                                            */
/*****************************************************************************/
#define RUBY_SPI1_SPCR_SPIE_BIT		7
#define RUBY_SPI1_SPCR_SPIE		(RUBY_BIT(RUBY_SPI1_SPCR_SPIE_BIT))
#define RUBY_SPI1_SPCR_SPE		(RUBY_BIT(6))
#define RUBY_SPI1_SPCR_MSTR		(RUBY_BIT(4))
#define RUBY_SPI1_SPCR_CPOL		(RUBY_BIT(3))
#define RUBY_SPI1_SPCR_CPHA		(RUBY_BIT(2))
#define RUBY_SPI1_SPCR_SPR(x)		((x) & 0x3)
#define RUBY_SPI1_SPSR_SPIF		(RUBY_BIT(7))
#define RUBY_SPI1_SPSR_WCOL		(RUBY_BIT(6))
#define RUBY_SPI1_SPSR_WFFULL		(RUBY_BIT(3))
#define RUBY_SPI1_SPSR_WFEMPTY		(RUBY_BIT(2))
#define RUBY_SPI1_SPSR_RFFULL		(RUBY_BIT(1))
#define RUBY_SPI1_SPSR_RFEMPTY		(RUBY_BIT(0))
#define RUBY_SPI1_SPER_ICNT(x)		(((x) & 0x3) << 6)
#define RUBY_SPI1_SPER_ESPR(x)		((x) & 0x3)
/*****************************************************************************/

/*****************************************************************************/
/* I2C constants                                                            */
/*****************************************************************************/
#define RUBY_I2C_BASE_ADDR			TOPAZ_ALIAS_MAP_SWITCH(0xF9000000, 0xE40F0000)
#define RUBY_I2C_MEM_SIZE			(0x0A08)
#define RUBY_I2C_ADAPTER_NUM		(0)

/*****************************************************************************/
/* Interrupts                                                                */
/*****************************************************************************/
#define RUBY_IRQ_RESET			(0)
#define RUBY_IRQ_MEM_ERR		(1)
#define RUBY_IRQ_INS_ERR		(2)
#define RUBY_IRQ_CPUTIMER0		(3)
#define RUBY_IRQ_CPUTIMER1		(4)
#define RUBY_IRQ_WATCHDOG		(5)
#define RUBY_IRQ_DMA0			(6)
#define RUBY_IRQ_BB			(7)
#define RUBY_IRQ_IPC_LO			(8)
#define IRQ_MAC0_0			RUBY_IRQ_IPC_LO
#define RUBY_IRQ_DSP			(9)
#define RUBY_IRQ_IPC_HI			(10)
#define RUBY_IRQ_MAC_TX_DONE		(11)
#define RUBY_IRQ_MAC_TX_ALERT		(12)
#define RUBY_IRQ_MAC_RX_DONE		(13)
#define RUBY_IRQ_MAC_RX_TYPE		(14)
#define RUBY_IRQ_MAC_MIB		(15)
#define RUBY_IRQ_MAC_0			(16)
#define RUBY_IRQ_MAC_1			(17)
#define RUBY_IRQ_PCIE			(18)
#define RUBY_IRQ_ENET0			(19)
#define RUBY_IRQ_ENET1			(20)
#define RUBY_IRQ_DMA1			(21)
#define RUBY_IRQ_DMA2			(22)
#define RUBY_IRQ_DMA3			(23)
#define RUBY_IRQ_UART			(24)
#define RUBY_IRQ_GPIO			(25)
#define RUBY_IRQ_TIMER			(26)
#define RUBY_IRQ_MISC			(27)
/* Combined PCIe Interrupt IRQ 28 */
#define RUBY_IRQ_MSI			(28)
#define RUBY_IRQ_INTA			(28)
/* Combined PCIe DMA Legacy/MSI Interrupt IRQ 22 */
#define TOPAZ_IRQ_PCIE_DMA_INT		(22)
#define TOPAZ_IRQ_PCIE_IPC4_INT		(29)
/* Combined PCIe Legacy/MSI Interrupt IRQ 28 */
#define TOPAZ_IRQ_PCIE_INT		RUBY_IRQ_INTA
#define RUBY_IRQ_SPI			(30)
#define RUBY_IRQ_BB_PER_PACKET		(31)
#define RUBY_MAX_IRQ_VECTOR		(31)
#define RUBY_IRQ_VECTORS_NUM		(RUBY_MAX_IRQ_VECTOR + 1)
/* these are extended (shared) irqs */
#define GPIO2IRQ(x)			((x) + RUBY_IRQ_GPIO0)
#define RUBY_IRQ_GPIO0			(32)
#define RUBY_IRQ_GPIO1			(33)
#define RUBY_IRQ_GPIO2			(34)
#define RUBY_IRQ_GPIO3			(35)
#define RUBY_IRQ_GPIO4			(36)
#define RUBY_IRQ_GPIO5			(37)
#define RUBY_IRQ_GPIO6			(38)
#define RUBY_IRQ_GPIO7			(39)
#define RUBY_IRQ_GPIO8			(40)
#define RUBY_IRQ_GPIO9			(41)
#define RUBY_IRQ_GPIO10			(42)
#define RUBY_IRQ_GPIO11			(43)
#define RUBY_IRQ_GPIO12			(44)
#define RUBY_IRQ_GPIO13			(45)
#define RUBY_IRQ_GPIO14			(46)
#define RUBY_IRQ_GPIO15			(47)
#define RUBY_IRQ_UART0			(48)
#define RUBY_IRQ_UART1			(49)
#define RUBY_IRQ_TIMER0			(50)
#define RUBY_IRQ_TIMER1			(51)
#define RUBY_IRQ_TIMER2			(52)
#define RUBY_IRQ_TIMER3			(53)
#define RUBY_IRQ_MISC_I2C		(56)
#define RUBY_IRQ_MISC_SRAM		(57)
#define RUBY_IRQ_MISC_NETSS		(58)
#define RUBY_IRQ_MISC_PLL1		(59)
#define RUBY_IRQ_MISC_PLL2		(60)
#define RUBY_IRQ_MISC_EXT_IRQ_COUNT		(5)
#define RUBY_IRQ_MISC_EXT_IRQ_START		(56)
#define RUBY_IRQ_MISC_RST_CAUSE_START	(26)

#define QTN_IRQ_MISC_EXT_IRQ_COUNT		TOPAZ_IRQ_MISC_EXT_IRQ_COUNT
#define QTN_IRQ_MISC_RST_CAUSE_START	TOPAZ_IRQ_MISC_RST_CAUSE_START

#define RUBY_MAX_IRQ_EXT_VECTOR		(63)
#define RUBY_IRQ_EXT_VECTORS_NUM	(RUBY_MAX_IRQ_EXT_VECTOR + 1)

/* M2L interrupt register is [31:16] high prio, [15:0] low prio */
#define RUBY_M2L_IRQ_NUM_HI		(16)
#define RUBY_M2L_IRQ_NUM_LO		(16)
/* M2L High priority interrupts, sent to RUBY_IRQ_IPC_HI */
#define RUBY_M2L_IRQ_HI_REBOOT		RUBY_M2L_IPC_HI_IRQ(13)
#define RUBY_M2L_IRQ_HI_DIE		RUBY_M2L_IPC_HI_IRQ(14)
/* M2L Low priority interrupts, sent to RUBY_IRQ_IPC_LO */
#define RUBY_M2L_IRQ_LO_MEAS		(8)
#define RUBY_M2L_IRQ_LO_OCAC		(9)
#define RUBY_M2L_IRQ_LO_CSA		(10)
#define RUBY_M2L_IRQ_LO_SCS		(11)
#define RUBY_M2L_IRQ_LO_SCAN		(12)
#define RUBY_M2L_IRQ_LO_VSP		(13)
#define RUBY_M2L_IRQ_LO_PRINT		(14)
#define RUBY_M2L_IRQ_LO_HLINK		(15)

/* these are DSP interrupts */
#define RUBY_DSP_IRQ_TIMER0		(3)
#define RUBY_DSP_IRQ_TIMER1		(5)
#define RUBY_DSP_IRQ_IPC_MUC2DSP	(10)
#define RUBY_DSP_IRQ_IPC_LHOST2DSP	(11)
#define RUBY_DSP_IRQ_IPC_MUC2DSP_HI	(17)
#define RUBY_DSP_IRQ_WMAC_COMBINED	(20)

/* M2D High priority interrupts, sent to RUBY_DSP_IRQ_IPC_MUC2DSP_HI */
#define RUBY_M2D_IRQ_HI_DIE		PLATFORM_REG_SWITCH(RUBY_IPC_HI_IRQ(7), 0)

/* L2M Low priority interrupts */
#define RUBY_L2M_IRQ_HLINK		(6)
#define RUBY_L2M_IRQ_HIGH		(7)

/*****************************************************************************/
/*****************************************************************************/
/* BB registers                                                             */
/*****************************************************************************/
#define RUBY_BB_BASE_ADDR			0xE6000000
/* FIXME: no BB2 on Ruby - this should expand to something else */
#define UMS_REGS_BB2				RUBY_BB_BASE_ADDR
#define RUBY_QT3_BB_GLBL_PREG_RIF_ENABLE	(RUBY_BB_BASE_ADDR + 0x1F4)
#define RUBY_QT3_BB_GLBL_PREG_RIF_ENABLE_ON	0x1
#define RUBY_QT3_BB_GLBL_PREG_RIF_ENABLE_OFF	0x0
#define RUBY_QT3_BB_GLBL_PREG_INTR_STATUS	(RUBY_BB_BASE_ADDR + 0x320)
#define RUBY_QT3_BB_FFT_INTR			(0x1000)
#define RUBY_QT3_BB_MIMO_PREG_RX_IPG_RST_ENABLE	(RUBY_BB_BASE_ADDR + 0x50268)
#define RUBY_QT3_BB_MIMO_BF_RX			(RUBY_BB_BASE_ADDR + 0x501FC)
#define RUBY_QT3_BB_MIMO_BF_RX_DUMP_ENABLE	(1 << 1) /*11n HT-LF dump enable*/

#define RUBY_QT3_BB_GLBL_SOFT_RST		(RUBY_BB_BASE_ADDR + 0x0008)
#define RUBY_QT3_BB_GLBL_SPI_CTRL		(RUBY_BB_BASE_ADDR + 0x0024)
#define RUBY_QT3_BB_TD_BASE_ADDR		(RUBY_BB_BASE_ADDR + 0x90000)

#define RUBY_QT3_BB_RF1_BASE_ADDR		0xE6080000
#define RUBY_QT3_BB_BONDING_4SS			0x01

#define RUBY_QT3_BB_TD_SPARE_0			(RUBY_QT3_BB_TD_BASE_ADDR + 0x5f4)
#define RUBY_QT3_BB_TD_SPARE_1			(RUBY_QT3_BB_TD_BASE_ADDR + 0x5f8)
#define RUBY_QT3_BB_TD_MAX_GAIN			(RUBY_QT3_BB_TD_BASE_ADDR + 0x36c)

/*****************************************************************************/
/* Radar registers                                                           */
/*****************************************************************************/
#define RUBY_RADAR_CNT_L	0xE6090558

/*****************************************************************************/
/* ARC addresses                                                             */
/*****************************************************************************/
#define RUBY_ARC_TLB_BYPASS	0x80000000
#define RUBY_ARC_CACHE_BYPASS	0xC0000000

/*****************************************************************************/
/*               DMA registers and bit defines.                              */
/*****************************************************************************/
#define RUBY_DMA_NUM_CHANNELS	(4)
#define RUBY_DMA_BASE_ADDR		TOPAZ_ALIAS_MAP_SWITCH(0xEA000000, 0xE4060000)
#define	RUBY_DMA_SAR(x)			(RUBY_DMA_BASE_ADDR + 0x00 + (x)*0x58)
#define	RUBY_DMA_DAR(x)			(RUBY_DMA_BASE_ADDR + 0x08 + (x)*0x58)
#define	RUBY_DMA_LLP(x)			(RUBY_DMA_BASE_ADDR + 0x10 + (x)*0x58)
#define	RUBY_DMA_CTL(x)			(RUBY_DMA_BASE_ADDR + 0x18 + (x)*0x58)
#define	RUBY_DMA_SIZE(x)		(RUBY_DMA_BASE_ADDR + 0x1c + (x)*0x58)
#define	RUBY_DMA_SSTAT(x)		(RUBY_DMA_BASE_ADDR + 0x20 + (x)*0x58)
#define	RUBY_DMA_DSTAT(x)		(RUBY_DMA_BASE_ADDR + 0x28 + (x)*0x58)
#define	RUBY_DMA_SSTATAR(x)		(RUBY_DMA_BASE_ADDR + 0x30 + (x)*0x58)
#define	RUBY_DMA_DSTATAR(x)		(RUBY_DMA_BASE_ADDR + 0x38 + (x)*0x58)
#define	RUBY_DMA_CFG(x)			(RUBY_DMA_BASE_ADDR + 0x40 + (x)*0x58)
#define	RUBY_DMA_SGR(x)			(RUBY_DMA_BASE_ADDR + 0x48 + (x)*0x58)
#define	RUBY_DMA_DSR(x)			(RUBY_DMA_BASE_ADDR + 0x50 + (x)*0x58)

#define RUBY_DMA_MASK_BLK		(RUBY_DMA_BASE_ADDR + 0x318)
#define RUBY_DMA_BLK_CLR		(RUBY_DMA_BASE_ADDR + 0x340)
#define RUBY_DMA_DMA_CFG		(RUBY_DMA_BASE_ADDR + 0x398)
#define RUBY_DMA_CH_EN			(RUBY_DMA_BASE_ADDR + 0x3a0)
/************** RUBY_DMA_CTL(x) bit defines. *********************************/
#define RUBY_DMA_CTL_INT_EN		RUBY_BIT(0)
#define RUBY_DMA_CTL_DINC		(0)
#define RUBY_DMA_CTL_DDEC		RUBY_BIT(7)
#define RUBY_DMA_CTL_DNOCHNG		RUBY_BIT(8)
#define RUBY_DMA_CTL_SINC		(0)
#define RUBY_DMA_CTL_SDEC		RUBY_BIT(9)
#define RUBY_DMA_CTL_SNOCHNG		RUBY_BIT(10)
#define RUBY_DMA_CTL_DMS_LHOST		(0)
#define RUBY_DMA_CTL_DMS_MUC		RUBY_BIT(23)
#define RUBY_DMA_CTL_SMS_LHOST		(0)		// ahb master bus 1
#define RUBY_DMA_CTL_SMS_MUC		RUBY_BIT(25)	// ahb master bus 2

/************** RUBY_DMA_CFG(x) bit defines. *********************************/
#define RUBY_DMA_CFG_ENABLE		RUBY_BIT(0)

/*****************************************************************************/
/* PCI registers & memory regions for target driver.                         */
/*****************************************************************************/
#define RUBY_PCIE_REG_BASE			TOPAZ_ALIAS_MAP_SWITCH(0xE9000000, 0xE4050000)
#define RUBY_PCIE_CONFIG_REGION			(0xCF000000)

#define RUBY_PCIE_CMD_REG			(RUBY_PCIE_REG_BASE + 0x0004)
#define RUBY_PCIE_BAR_NUM			(6)
#define	RUBY_PCIE_BAR_BASE			(RUBY_PCIE_REG_BASE + 0x0010)
#define	RUBY_PCIE_BAR(n)			(RUBY_PCIE_BAR_BASE + (n << 2))
#define	RUBY_PCIE_BAR_MASK_ADDR			(RUBY_PCIE_REG_BASE + 0x1010)
#define	RUBY_PCIE_BAR_MASK(n)			(RUBY_PCIE_BAR_MASK_ADDR + (n << 2))

#define RUBY_PCIE_ATU_VIEW			(RUBY_PCIE_REG_BASE + 0x0900)
#define RUBY_PCIE_ATU_CTL1			(RUBY_PCIE_REG_BASE + 0x0904)
#define RUBY_PCIE_ATU_CTL2			(RUBY_PCIE_REG_BASE + 0x0908)
#define RUBY_PCIE_ATU_BASE_LO			(RUBY_PCIE_REG_BASE + 0x090c)
#define RUBY_PCIE_ATU_BASE_HI			(RUBY_PCIE_REG_BASE + 0x0910)
#define RUBY_PCIE_ATU_BASE_LIMIT		(RUBY_PCIE_REG_BASE + 0x0914)
#define RUBY_PCIE_ATU_TARGET_LO			(RUBY_PCIE_REG_BASE + 0x0918)
#define RUBY_PCIE_ATU_TARGET_HI			(RUBY_PCIE_REG_BASE + 0x091c)

#define RUBY_PCIE_ATU_OB_REGION(n)		(0x0 + n)
#define RUBY_PCIE_ATU_IB_REGION(n)		(0x80000000 + n)
#define RUBY_PCIE_ATU_CFG_SHIFT			RUBY_BIT(28)
#define RUBY_PCIE_ATU_OB_ENABLE			RUBY_BIT(31)

#define RUBY_PCI_RC_MEM_WINDOW			(32 << 20) /* 32MB  MEMORY window in Root Complex for pcie tree  */
#define RUBY_PCI_RC_CFG_SIZE			(64 << 10) /* 64KB  CFG size in Root Complex for pcie tree */
#define RUBY_PCI_RC_MEM_START			(0xc0000000) /* PCI memory region in Root Complex's kernel address space */

#define RUBY_PCIE_INT_MASK			(RUBY_SYS_CTL_BASE_ADDR + 0xC0)
#define RUBY_PCIE_MSI_ENABLE			RUBY_BIT(16)
#define MSI_CTL_OFFSET				(0x50)
#define RUBY_MSI_ADDR_LOWER			(RUBY_PCIE_REG_BASE + 0x820)
#define RUBY_MSI_ADDR_UPPER			(RUBY_PCIE_REG_BASE + 0x824)
#define RUBY_MSI_INT_ENABLE			(RUBY_PCIE_REG_BASE + 0x828)
#define RUBY_PCIE_MSI_MASK			(RUBY_PCIE_REG_BASE + 0x82c)
#define RUBY_PCIE_MSI_STATUS			(RUBY_PCIE_REG_BASE + 0x830)
#define RUBY_PCIE_MSI_REGION			(0xce000000) /* msi message address */
#define RUBY_MSI_DATA				(0x0)	     /* msi message data */
#define RUBY_PCIE_MSI_CLEAR			RUBY_BIT(0)  /* clear msi intr */

#define TOPAZ_PCIE_STAT				(RUBY_SYS_CTL_BASE_ADDR + 0x017C)
#if TOPAZ_FPGA_PLATFORM
	#define TOPAZ_PCIE_LINKUP		(0xe)
#else
	#define TOPAZ_PCIE_LINKUP		(0x7)
#endif

#define PCIE_LINK_STAT				(RUBY_PCIE_REG_BASE + 0x80)
#define PCIE_LINK_CTL2				(RUBY_PCIE_REG_BASE + 0xa0)
#define PCIE_ASPM_L1_CTRL			(RUBY_PCIE_REG_BASE + 0x70c)
#define PCIE_ASPM_LINK_CTRL			(PCIE_LINK_STAT)
#define PCIE_PORT_LINK_CTL			(RUBY_PCIE_REG_BASE + 0x710)
#define PCIE_ASPM_L1_SUBSTATE_TIMING		(RUBY_PCIE_REG_BASE + 0xB44)
#define PCIE_L1SUB_CTRL1			(RUBY_PCIE_REG_BASE + 0x150)
#define PCIE_PMCSR				(RUBY_PCIE_REG_BASE + 0x44)

/* PCIe link defines */
#define PCIE_LINK_GEN1				(BIT(0))
#define PCIE_LINK_GEN2				(BIT(1))
#define PCIE_LINK_GEN3				(BIT(2))
#define PCIE_LINK_MODE(x)			(((x) >> 16) & 0x7)

/* ATU setting for Host Buffer Descriptor Mapping */
#define PCIE_HOSTBD_REGION			(RUBY_PCIE_ATU_OB_REGION(2))
#define PCIE_ATU_BAR_MIN_SIZE			0x00010000 /* 64k */
#define PCIE_HOSTBD_SIZE			(2 * PCIE_ATU_BAR_MIN_SIZE)
#define PCIE_HOSTBD_SIZE_MASK			(PCIE_HOSTBD_SIZE - 1)
#define PCIE_HOSTBD_START_HI			0x00000000
#define PCIE_HOSTBD_REGION_ENABLE		RUBY_PCIE_ATU_OB_ENABLE

/* Extra system controller bits */
#define TOPAZ_SYS_CTL_PLLCLKOUT_EN		(RUBY_BIT(10))

/* Board platform and revision */
#define TOPAZ_BOARD_REVA			0x40
#define TOPAZ_BOARD_REVB			0x41
#define TOPAZ_BOARD_REVA2			0x43

/*
 * WOWLAN GPIO assignment
 * On RGMII module - GPIO_B_10 (set as output for WoWLAN)
 * On RGMII host board - GPIO_B_10 (set as input to wake up the host)
 *
 * On PCIe module - WAKE# or GPIO_B_12 ( set as output for WoWLAN, use WAKE# by default)
 * On PCIe host board - GPIO_B_14 (set as input to wake up the host)
 */
#ifdef CONFIG_TOPAZ_PCIE_TARGET
#define WOWLAN_GPIO_OUTPUT_PIN	12
#else
#define WOWLAN_GPIO_OUTPUT_PIN	10
#endif
#endif // #ifndef __RUBY_PLATFORM_H

