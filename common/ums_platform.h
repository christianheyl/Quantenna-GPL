/*
 *  common/ums_platform.h
 *
 *  Copyright (c) Quantenna Communications Incorporated 2007.
 *  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This file holds the hardware specific memory map and common declarations
 * for the UMS build system.  The defines here are used in the bootrom,
 * the U-Boot bootloader and the linux kernel.
 *
 * This file should only contain definitions that are assembler-friendly.
 */

#ifndef __UMS_PLATFORM_H
#define __UMS_PLATFORM_H	1

/* ================ System boot modes ================ */
/* These are used in the boot rom which is hardcoded into
 * the chip.  Do not change them unless the chip changes.
 */
#define BMODE_PRODUCTION_TEST 0
#define BMODE_SERIAL_ICC 1
#define BMODE_NOR 2

/* ================ Physical address map ================ */
#define UMS_DDR				0x00000000
#define UMS_SRAM			0x80000000
#define UMS_SRAM_SIZE			0x00080000
#define UMS_EBB_CS0			0x90000000
#define UMS_EBB_CS1			0x91000000
#define UMS_EBB_CS2			0x92000000
#define UMS_EBB_CS3			0x93000000
#define UMS_EBB_CS4			0x94000000
#define UMS_BOOTROM			0xA0000000
#define UMS_ARM_ITCM			0xB0000000
#define UMS_ITCM_SIZE			0x00008000
#define UMS_ARM_DTCM			0xB0100000
#define UMS_DTCM_SIZE			0x00008000
#define UMS_REGS_DSP_UART		0xD0000000
#define UMS_REGS_DSP_GPIO		0xD1000000
#define UMS_REGS_DSP_SPI		0xD3000000
#define UMS_REGS_DSP_CTRLRESET		0xD8000000
#define UMS_REGS_DSP_MSP		0xD9000000
#define UMS_REGS_DSP_XMEM		0xDCF80000
#define UMS_REGS_DSP_YMEM		0xDCFA0000
#define UMS_REGS_SYSCTRL		0xE0000000
#define UMS_REGS_DDR			0xE1000000
#define UMS_REGS_EBI			0xE2000000
#define UMS_REGS_SRAM			0xE3000000
#define UMS_REGS_MB			0xE4000000
#define UMS_REGS_MAC			0xE5000000
#define UMS_REGS_BB			0xE6000000
#define UMS_REGS_RADAR			0xE6080000
#define UMS_REGS_BB2			0xE6800000
#define UMS_REGS_RADAR2			0xE6880000
#define UMS_REGS_ICC			0xE8000000
#define UMS_REGS_USB			0xE9000000
#define UMS_REGS_ULA			0xEC000000
#define UMS_REGS_ULA_MB         0xEC200000
#define UMS_REGS_ETHERNET0		0xED000000
#define UMS_REGS_ARM_UART0		0xF0000000
#define UMS_REGS_ARM_GPIO		0xF1000000
#define UMS_REGS_ARM_SPI		0xF2000000
#define UMS_REGS_ARM_TIMERS		0xF3000000
#define UMS_REGS_ARM_WATCHDOG		0xF4000000
#define UMS_REGS_ARM_UART1		0xF5000000
#define UMS_REGS_ARM_DMA		0xF8000000
#define UMS_REGS_ARM_DSPI2C		0xF9000000
#define UMS_REGS_ARM_VICS		0xFFF00000

/* Explicit virtual address mappings for TCMs */
#define UMS_ARM_ITCM_VA			0xFB000000
#define UMS_ARM_DTCM_VA			0xFB100000

#define UMS_ARM_SRAM_AREA		UMS_SRAM + CONFIG_ARCH_UMS_MUC_SRAM_REQUIREMENT
#define UMS_ARM_SRAM_AREA_VA		IO_ADDRESS(UMS_ARM_SRAM_AREA)

/* !!! FIXME_UMS - at present SRAM lives in IO address space */
#define UMS_IO_AREA_START		UMS_SRAM

/* ============== Interrupt functions ============== */

/* Set bits in these values to make an interrupt a FIQ rather than an IRQ.
   SELECT0 is for interrupts 0-31, SELECT1 for the others.
*/
#define FIQ_SELECT0 (0)
#define FIQ_SELECT1 (0)

#define VIC0_OFFSET	0x000FF000
#define VIC1_OFFSET	0x000FE000
#define INTERRUPT_VA0(a) (IO_ADDRESS(UMS_REGS_ARM_VICS) + VIC0_OFFSET + (a))
#define INTERRUPT_VA1(a) (IO_ADDRESS(UMS_REGS_ARM_VICS) + VIC1_OFFSET + (a))
#define PL192_IRQSTATUS (0)
#define PL192_INTSELECT (0x0c)
#define PL192_ENABLE (0x10)
#define PL192_DISABLE (0x14)
#define PL192_SOFTINT (0x18)
#define PL192_SOFTINT_CLEAR (0x1c)
#define PL192_PROTECTION (0x20)
#define PL192_PRIORITY_MASK (0x24)
#define PL192_PRIORITY_DAISY (0x28)
#define PL192_VECTOR_ADDR (0x100)
#define PL192_VECTOR_PRIORITY (0x200)
#define PL192_VECTORADDRESS	0x0F00

/* ============== Timer functions ============== */

#define TIMER_VA(a) (IO_ADDRESS(UMS_REGS_ARM_TIMERS) + (a))

#define TIMER_PRESCALAR_ENABLE (0x00)
#define TIMER_PRESCALAR0 (0x04)
#define TIMER_PRESCALAR1 (0x08)
#define TIMER_CONTROL0 (0x0c)
#define TIMER_VALUE0 (0x10)
#define TIMER_CONTROL1 (0x14)
#define TIMER_VALUE1 (0x18)
#define TIMER_CONTROL2 (0x1c)
#define TIMER_VALUE2 (0x20)
#define TIMER_CONTROL3 (0x24)
#define TIMER_VALUE3 (0x28)
#define TIMER_INT_ENABLE (0x2c)
#define TIMER_INT_STATUS (0x30)
#define TIMER_INT_CLEAR (0x34)

/* GPIO block register offsets */
#define GPIO_INPUT		(0x00)
#define GPIO_OUTPUT_MASK	(0x04)
#define GPIO_OUTPUT		(0x08)
#define GPIO_MODE1		(0x0c)
#define GPIO_MODE2		(0x10)
#define GPIO_ALTFN		(0x14)
#define GPIO_ALTFN_DEFVAL	(0x18)

/* GPIO special function GPIO line assignments (ARM GPIO block) */
#define GPIO_UART0_SI		(0)
#define GPIO_UART0_nRI		(1)
#define GPIO_UART0_DSR		(2)
#define GPIO_UART0_nDCD		(3)
#define GPIO_UART0_nCTS		(4)
#define GPIO_SPI_MISO		(5)
#define GPIO_UART1_SI		(6)
#define GPIO_UART0_SO		(8)
#define GPIO_UART0_nRTS		(9)
#define GPIO_UART0_nDTR		(10)
#define GPIO_SPI_SCK		(11)
#define GPIO_SPI_MOSI		(12)
#define GPIO_UART1_SO		(13)
#define GPIO_SPI_nCS		(14)

/* alternate use for gpio5 */
#define GPIO_RGMII_MODE		(5)

/* GPIO mode register values */
#define GPIO_MODE_INPUT		(0)
#define GPIO_MODE_OUTPUT	(1)
#define GPIO_MODE_OSOURCE	(2)
#define GPIO_MODE_ODRAIN	(3)

/* SPI controller register offsets */
#define SPI_SPCR		(0x00)
#define SPI_SPSR		(0x04)
#define SPI_SPDR		(0x08)
#define SPI_SPER		(0x0c)
#define SPI_SLVN		(0x10)

/* SPI status register bits */
#define SPI_SPSR_RFEMPTY	(1 << 0)
#define SPI_SPSR_RFFULL		(1 << 1)
#define SPI_SPSR_WFEMPTY	(1 << 2)
#define SPI_SPSR_WFFULL		(1 << 3)

/* SPI control register bits */
#define SPI_SPCR_SPR(x)		(((x) & 3) << 0)
#define SPI_SPCR_CPHA		(1 << 2)
#define SPI_SPCR_CPOL		(1 << 3)
#define SPI_SPCR_MSTR		(1 << 4)
#define SPI_SPCR_SPE		(1 << 6)
#define SPI_SPCR_SPIE		(1 << 7)

/* SPI extended control register bits */
#define SPI_SPER_ESPR(x)	(((x) & 3) << 0)
#define SPI_SPER_ICNT(x)	(((x) & 3) << 6)

/* System controller register offset and bit position definitions */
#define SYSCTRL_RESET_MASK	(0x00)
#define SYSCTRL_RESET		(0x04)
#define SYSCTRL_CTRL_MASK	(0x08)
#define SYSCTRL_CTRL		(0x0c)
#define SYSCTRL_RESET_CAUSE	(0x10)
#define SYSCTRL_REV_NUMBER	(0x14)
#define SYSCTRL_RGMII_DELAY	(0x1c)

/* Reset bit positions for RESET_MASK and RESET_VEC registers */
#define SYSCTRL_ARM_RUN		(1 << 0)
#define SYSCTRL_EBI_RUN		(1 << 1)
#define SYSCTRL_DDR_RUN		(1 << 2)
#define SYSCTRL_SRAM_RUN	(1 << 3)
#define SYSCTRL_DSPSS_RUN	(1 << 4)
#define SYSCTRL_DSP_RUN		(1 << 5)
#define SYSCTRL_MUC_RUN		(1 << 6)
#define SYSCTRL_NETSS_RUN	(1 << 7)
#define SYSCTRL_MMC_RUN		(1 << 8)
#define SYSCTRL_ETHERNET_RUN	(1 << 9)
#define SYSCTRL_IOSS_RUN	(1 << 10)
#define SYSCTRL_ICC_RUN		(1 << 12)
#define SYSCTRL_USB_RUN		(1 << 13)
#define SYSCTRL_RESET_OUT   (1 << 31)

/* System controller control register */
#define SYSCTRL_BOOT_MODE(x)	(((x) & 7) << 0)
#define SYSCTRL_REMAP(x)	(((x) & 3) << 3)
#define SYSCTRL_CLKSEL(x)	(((x) & 3) << 5)
#define SYSCTRL_ARM_IS_2X	(1 << 7)
#define SYSCTRL_DSP_CLK		(1 << 8)
#define SYSCTRL_MAC_CLK(x)	(((x) & 7) << 9)
#define SYSCTRL_REMAP_SRAM	(1 << 12)
#define SYSCTRL_ULPI_ENABLE	(1 << 13)
#define SYSCTRL_ARM_GPIO_ENABLE	(1 << 14)
#define SYSCTRL_DSP_GPIO_ENABLE	(1 << 15)
#define SYSCTRL_EBI_MUXMODE	(1 << 16)
#define SYSCTRL_ARBITER_MODE(x)	(((x) & 0xf) << 17)
#define SYSCTRL_SPLIT_DISABLE	(1 << 21)
#define SYSCTRL_EXT_USBCLK	(1 << 22)
#define SYSCTRL_PCIE_ENABLE	(1 << 23)
#define SYSCTRL_NETBUS_SWAP	(1 << 24)
#define SYSCTRL_IOBUS_SWAP	(1 << 25)
#define SYSCTRL_DSPBUS_SWAP	(1 << 26)

#define SYSCTRL_REMAP_DDR	(0)
#define SYSCTRL_REMAP_ROM	(1)
#define SYSCTRL_REMAP_NOR	(2)
#define SYSCTRL_REMAP_NAND	(3)

/* Reset cause definitions */
#define SYSCTRL_HARD_RESET	(1 << 0)
#define SYSCTRL_SOFT_RESET	(1 << 1)
#define SYSCTRL_WATCHDOG	(1 << 2)
#define SYSCTRL_PLL_DRIFT	(1 << 3)
#define SYSCTRL_EBI_STRAP(x)	(((x) & 3) >> 16)

/* bbic2 bit to switch between 100M and 1000M */
#define SYS_CTL_GMII_CLK_SEL	(1 << 23)
#define SYS_CTL_FORCE_RGMII		(0xc0000000)

/* Chip revision macros - use with SYSCTRL_REV_NUMBER */
#define SYSCTRL_CHIP_MINOR(x)	((x) & 0xff)
#define SYSCTRL_CHIP_MAJOR(x)	(((x) & 0xff) >> 8)
#define SYSCTRL_CHIP_TYPE(x)	(((x) & 0xff) >> 16)
#define SYSCTRL_CHIP_TYPE_UMS	(0)

/* UART register offsets */
#define PL011_DR	(0x00)
#define PL011_RSR_ECR	(0x04)
#define PL011_FR	(0x18)
#define PL011_ILPR	(0x20)
#define PL011_IBRD	(0x24)
#define PL011_FBRD	(0x28)
#define PL011_LCR_H	(0x2c)
#define PL011_CR	(0x30)
#define PL011_IFLS	(0x34)
#define PL011_IMSC	(0x38)
#define PL011_RIS	(0x3c)
#define PL011_MIS	(0x40)
#define PL011_ICR	(0x44)
#define PL011_DMACR	(0x48)
#define PL011_PERIPHID0	(0xfe0)
#define PL011_PERIPHID1	(0xfe4)
#define PL011_PERIPHID2	(0xfe8)
#define PL011_PERIPHID3	(0xfec)
#define PL011_CELLID0	(0xff0)
#define PL011_CELLID1	(0xff4)
#define PL011_CELLID2	(0xff8)
#define PL011_CELLID3	(0xffc)

/* Static memory controller offsets */
#define PL241_DIRCMD	(0x1010)
#define PL241_SETCYC	(0x1014)
#define PL241_SETOPM	(0x1018)
#define PL241_SETCYC0	(0x1100)
#define PL241_SETOPM0	(0x1104)

/* ICC register offsets */
#define ICC_SRC		(0x00)
#define ICC_DST		(0x04)
#define ICC_CTRL	(0x08)
#define ICC_ISR		(0x0C)
#define ICC_MASKED_ISR	(0x10)
#define ICC_IEN		(0x14)
#define ICC_CLR_RIP	(0x18)
#define ICC_RD_CMD	(0x20)
#define ICC_BUSY_FLAG	(1 << 31) /* Busy bit in CTRL register */
#define ICC_RD_COMPLETE	(1 << 1)  /* Read complete bit in ISR register */
#define ICC_MAX_XFER	(0x8000) /* Max ICC length = 64kB !!! FIXME */

/* MAC register offsets */
#define UMS_MAC_IMEM	(0x00000)
#define UMS_MAC_DMEM	(0x20000)
#define UMS_MAC_PKTMEM	(0x30000)
#define UMS_MAC_TXMEM	(0x40000)
#define UMS_MAC_GBLCTRL	(0x42000)
#define UMS_MAC0_TXREGS	(0x50000)
#define UMS_MAC0_RXREGS	(0x52000)
#define UMS_MAC0_SHARED	(0x53000)
#define UMS_MAC1_TXREGS	(0x60000)
#define UMS_MAC1_RXREGS	(0x62000)
#define UMS_MAC1_SHARED	(0x63000)
#define UMS_MAC_DMA	(0x70000)
#define UMS_MAC_HOSTIF	(0x71000)

/* BB register offsets*/
/* MAY need revisit XXX */
#define UMS_BB_SPI  (0x40000)
#define UMS_BB_GAIN (0x50000) 
#define UMS_BB_XREF (0x60000) 
#define UMS_BB_RFIC (0x70000)
#define UMS_BB_RDR  (0x80000)
#define UMS_BB_COMPQ_MEM  (0xB0000)

/* MBX register offsets */
#define UMS_MBX_DSP_POP  (0x0000)
#define UMS_MBX_DSP_PUSH (0x0040)
#define UMS_MBX_CTRL     (0x0080)
#define UMS_MBX_STATUS   (0x0084)
#define UMS_MBX_INT_MSK  (0x0088)
#define UMS_MBX_INT_CLR  (0x008C)

/* MBX register bitfields */
#define UMS_MBX_INT0 (1 << 0)
#define UMS_MBX_INT1 (1 << 1)

#define UMS_MBX_DSP_TO_ARM_EMPTY ( 1 << 24 )

#endif
