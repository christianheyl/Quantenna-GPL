/*SH1
*******************************************************************************
**                                                                           **
**         Copyright (c) 2008 - 2010 Quantenna Communications Inc            **
**                            All Rights Reserved                            **
**                                                                           **
**  Author      : Quantenna Communications Inc                               **
**  File        : registers.h                                                **
**  Description :                                                            **
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

#ifndef _QTN_REGISTERS_H
#define _QTN_REGISTERS_H

#include <asm/io.h>

#if defined(MUC_BUILD) || defined(DSP_BUILD) || defined(AUC_BUILD)
#include <qtn/registers_muc.h>
#endif

#if (CONFIG_ARCH_ARC)
# define CONFIG_RUBY 1
#else
# define CONFIG_ENVY 1
#endif

#define QTN_BIT(_i)				(1L << (_i))

#define SYS_REG_BASE		0xE0000000
#define SYS_RESET_VECTOR_MASK	(SYS_REG_BASE + 0x0)
#define SYS_RESET_VECTOR	(SYS_REG_BASE + 0x4)
#define SYS_CONTROL_MASK	(SYS_REG_BASE + 0x8)
#define SYS_CONTROL_REG		(SYS_REG_BASE + 0xC)

#define ULA_RESET_VECTOR_MASK 0xE0000000
#define ULA_RESET_VECTOR 0xE0000004

#define NDP_PKT_SRC_BBO (0xE60B0000)
#define NDP_PKT_SRC_BB1 (0xE68B0000)

#define DSP_MASTER_GPIO_ENABLE (1<<15)

#define MUC_BASE_ADDR 0xE5000000
#define MUC_OFFSET_CTRL_REG 0x42000

#define MUC_CTRL_REG_SIZE 0xFF

/* Interrupts */
#define MUC_INT0  0
#define MUC_INT1  1
#define MUC_INT2  2
#define MUC_INT3  3
#define MUC_INT4  4
#define MUC_INT5  5
#define MUC_INT6  6
#define MUC_INT7  7
#define MUC_INT8  8
#define MUC_INT9  9
#define MUC_INT10 10
#define MUC_INT11 11
#define MUC_INT12 12
#define MUC_INT13 13
#define MUC_INT14 14

/* UMS reset Masks */
#define ARMSS_RESET 0x00000001   
#define EBI_RESET   0x00000002
#define DRAM_RESET  0x00000004   
#define SRAM_RESET  0x00000008   
#define DSPSS_RESET 0x00000010   
#define DSP_RESET   0x00000020   
#define MUC_RESET   0x00000040   
#define NETSS_RESET 0x00000080   
#define MMC_RESET   0x00000100   
#define ENET_RESET  0x00000200   
#define IOSS_RESET  0x00000400   
#define PCI_RESET   0x00000800   
#define SDIO_RESET  0x00001000   
#define USB_RESET   0x00002000 
#define BB_RESET    0x00004000  
#define MB_RESET    0x00008000  
#define ULA_RESET   0x00010000  

struct muc_ctrl_reg
{
	volatile u32 int_mask;
	volatile u32 int_status;
	volatile u32 host_sem0;
	volatile u32 host_sem1;
	volatile u32 uc_sem0;
	volatile u32 uc_sem1;
	volatile u32 mac0_host_int_pri;
	volatile u32 mac0_host_int_mask;
	volatile u32 mac0_host_int_status;
	volatile u32 mac0_host_int_gen;
	volatile u32 mac1_host_int_pri; //Not in Ruby
	volatile u32 mac1_host_int_mask; //Not in Ruby
	volatile u32 mac1_host_int_status; //Not in Ruby
	volatile u32 mac1_host_int_gen; //Not in Ruby
	volatile u32 soft_rst;
	volatile u32 lpbck_cntl; //Not in Ruby
#if CONFIG_RUBY
	volatile u32 global_tim0;
	volatile u32 global_tim1;
	volatile u32 global_csr;
	volatile u32 mac_debug_sel;
#endif
} __attribute__ ((packed));

struct ruby_sys_ctrl_reg
{
	volatile u32 reset_vector_mask; /* 0xE0000000 */
	volatile u32 reset_vector;
	volatile u32 system_control_mask;
	volatile u32 system_control;
	volatile u32 reset_cause;       /* 0xE0000010 */
	volatile u32 csr;
	volatile u32 debug_select;
	volatile u32 l2m_int;
	volatile u32 l2m_int_mask;      /* 0xE0000020 */
	volatile u32 l2d_int;
	volatile u32 l2d_int_mask;
	volatile u32 m2l_int;
	volatile u32 m2l_int_mask;      /* 0xE0000030 */
	volatile u32 m2d_int;
	volatile u32 m2d_int_mask;
	volatile u32 d2l_int;
	volatile u32 d2l_int_mask;      /* 0xE0000040 */
	volatile u32 d2m_int;
	volatile u32 d2m_int_mask;
	volatile u32 lhost_int_en;
	volatile u32 muc_int_en;        /* 0xE0000050 */
	volatile u32 dsp_int_en;
	volatile u32 lhost_int_or_en;
	volatile u32 muc_int_or_en;
	volatile u32 dsp_int_or_en;     /* 0xE0000060 */
	volatile u32 muc_remap;
	volatile u32 dsp_remap;
	volatile u32 pcie_cfg_0;
	volatile u32 pcie_cfg_1;        /* 0xE0000070 */
	volatile u32 pcie_cfg_2;
	volatile u32 pcie_cfg_3;
	volatile u32 pcie_cfg_4;
	volatile u32 pll0_ctrl;         /* 0xE0000080 */
	volatile u32 pll1_ctrl;
	volatile u32 proc_id;
	volatile u32 pll2_ctrl;
	volatile u32 reserved1;         /* 0xE0000090 */
	volatile u32 l2m_sem;
	volatile u32 m2l_sem;
	volatile u32 l2d_sem;
	volatile u32 d2l_sem;           /* 0xE00000A0 */
	volatile u32 m2d_sem;
	volatile u32 d2m_sem;
	volatile u32 intr_inv0;
	volatile u32 intr_inv1;         /* 0xE00000B0 */
	volatile u32 gmii_clkdll;
	volatile u32 debug_bus;
	volatile u32 spare;
	volatile u32 pcie_int_mask;     /* 0xE00000C0 */
} __attribute__ ((packed));

#define NETDEV_F_ALLOC		0x0001	/* dev allocated */
#define	NETDEV_F_VALID		0x0002	/* dev is valid */
#define	NETDEV_F_RUNNING	0x0004	/* dev is running */
#define	NETDEV_F_PROMISC	0x0008	/* dev is in promiscuous mode */
#define	NETDEV_F_ALLMULTI	0x0010
#define	NETDEV_F_UP		0x0020	/* dev is up */
#define	NETDEV_F_EXTERNAL	0x0040	/* dev is exposed to tcp/ip stack */

#define SCAN_BASEBAND_RESET	0x0000
#define SCAN_CHAN_CHANGE	0x0001

extern void enable_host_irq(int devid, int irq);
extern void disable_host_irq(int devid, int irq);

#define BB_BASE_ADDR(_i)	(0xE6000000 + (_i * 0x800000))

#define BB_SPI_BASE(_i)		(BB_BASE_ADDR(_i) + 0x40000)
#define BB_RF_BASE(_i)		(BB_BASE_ADDR(_i) + 0x70000)
#define BB_RDR_BASE(_i)		(BB_BASE_ADDR(_i) + 0x80000)

/*
 * Define RFIC version register.
 * Expected to map across multiple versions of the RFIC
 */

#define RFIC_VERSION				(BB_SPI_BASE(0) + 0X0018)

/*Define SPI registers */

#define SPI_VCO_FREQ_CC_OUTPUT(_i)		(BB_SPI_BASE(_i) + 0X0004)
#define SPI_LX_PLL_COUNTER_DATA_LSB(_i)		(BB_SPI_BASE(_i) + 0X0008)
#define SPI_LX_PLL_COUNTER_DATA_MSB(_i)		(BB_SPI_BASE(_i) + 0X000C)
#define SPI_RC_TUNING_DATA_OUT(_i)		(BB_SPI_BASE(_i) + 0X0010)
#define SPI_CTRL_DATA_AGC_TST(_i)		(BB_SPI_BASE(_i) + 0X00C4)
#define SPI_READ_AGC_RX1(_i)			(BB_SPI_BASE(_i) + 0X00C8)
#define SPI_READ_AGC_RX2(_i)			(BB_SPI_BASE(_i) + 0X00CC)
#define SPI_RX12_AGC_OUT(_i)			(BB_SPI_BASE(_i) + 0X00D0)
#define SPI_ET_DAC_TX12(_i)			(BB_SPI_BASE(_i) + 0X00D4)
#define SPI_RX1_DAC_I(_i)			(BB_SPI_BASE(_i) + 0X00D8)
#define SPI_RX1_DAC_Q(_i)			(BB_SPI_BASE(_i) + 0X00DC)
#define SPI_RX2_DAC_I(_i)			(BB_SPI_BASE(_i) + 0X00E0)
#define SPI_RX2_DAC_Q(_i)			(BB_SPI_BASE(_i) + 0X00E4)
#define SPI_PDN_RX_BUS_CTRL_BUS_1(_i)		(BB_SPI_BASE(_i) + 0X00E8)
#define SPI_PDN_RX_BUS_CTRL_BUS_2(_i)		(BB_SPI_BASE(_i) + 0X00EC)
#define SPI_PDN_RX_BUS_CTRL_BUS_3(_i)		(BB_SPI_BASE(_i) + 0x00F0)
#define SPI_PDN_RX_BUS_CTRL_BUS_4(_i)		(BB_SPI_BASE(_i) + 0x00F4)
#define SPI_MUX_IN(_i)				(BB_SPI_BASE(_i) + 0x00F8)
#define SPI_MUX_OUT(_i)				(BB_SPI_BASE(_i) + 0x00FC)
#define SPI_DAC_RX1TB1_I(_i)			(BB_SPI_BASE(_i) + 0x0100)
#define SPI_DAC_RX1TB2_I(_i)			(BB_SPI_BASE(_i) + 0x0104)
#define SPI_DAC_RX1TB3_I(_i)			(BB_SPI_BASE(_i) + 0x0108)
#define SPI_DAC_RX1TB4_I(_i)			(BB_SPI_BASE(_i) + 0x010C)
#define SPI_DAC_RX1TB1_Q(_i)			(BB_SPI_BASE(_i) + 0x0110)
#define SPI_DAC_RX1TB2_Q(_i)			(BB_SPI_BASE(_i) + 0x0114)
#define SPI_DAC_RX1TB3_Q(_i)			(BB_SPI_BASE(_i) + 0x0118)
#define SPI_DAC_RX1TB4_Q(_i)			(BB_SPI_BASE(_i) + 0x011C)
#define SPI_DAC_RX2TB1_I(_i)			(BB_SPI_BASE(_i) + 0x0120)
#define SPI_DAC_RX2TB2_I(_i)			(BB_SPI_BASE(_i) + 0x0124)
#define SPI_DAC_RX2TB3_I(_i)			(BB_SPI_BASE(_i) + 0x0128)
#define SPI_DAC_RX2TB4_I(_i)			(BB_SPI_BASE(_i) + 0x012C)
#define SPI_DAC_RX2TB1_Q(_i)			(BB_SPI_BASE(_i) + 0x0130)
#define SPI_DAC_RX2TB2_Q(_i)			(BB_SPI_BASE(_i) + 0x0134)
#define SPI_DAC_RX2TB3_Q(_i)			(BB_SPI_BASE(_i) + 0x0138)
#define SPI_DAC_RX2TB4_Q(_i)			(BB_SPI_BASE(_i) + 0x013C)
#define SPI_RX_PDN_11_0(_i)			(BB_SPI_BASE(_i) + 0x0140)
#define SPI_RX_PDN_23_12(_i)			(BB_SPI_BASE(_i) + 0x0144)
#define SPI_TX1_PDN_11_0(_i)			(BB_SPI_BASE(_i) + 0x0148)
#define SPI_TX1_PDN_23_12(_i)			(BB_SPI_BASE(_i) + 0x014C)
#define SPI_TX2_PDN_11_0(_i)			(BB_SPI_BASE(_i) + 0X0150)
#define SPI_TX2_PDN_23_12(_i)			(BB_SPI_BASE(_i) + 0X0154)
#define SPI_ALX_PDN_11_0(_i)			(BB_SPI_BASE(_i) + 0X0158)
#define SPI_ALX_PDN_23_12(_i)			(BB_SPI_BASE(_i) + 0X015C)
#define SPI_PDN_VAL_23_0(_i)			(BB_SPI_BASE(_i) + 0X0160)
#define SPI_RX_CAL_OVRD_VAL(_i)			(BB_SPI_BASE(_i) + 0X0164)
#define SPI_AGC_CTRL_DAC_RC_TST(_i)		(BB_SPI_BASE(_i) + 0X0168)
#define SPI_PLL_FRACTIONAL(_i)			(BB_SPI_BASE(_i) + 0X016C)
#define SPI_PLL_N_CH(_i)			(BB_SPI_BASE(_i) + 0X0170)
#define SPI_CP_D_U_VCO(_i)			(BB_SPI_BASE(_i) + 0X0174)
#define SPI_VCO_CLK_PFD_LPF(_i)			(BB_SPI_BASE(_i) + 0X0178)
#define SPI_BG_BIAS_EXT_PTAT(_i)		(BB_SPI_BASE(_i) + 0X017C)
#define SPI_CURR_OPT_DAC_TX_RX_SSB(_i)		(BB_SPI_BASE(_i) + 0X0180)
#define SPI_RXMX_BBMX_VBIAS_ADJ(_i)		(BB_SPI_BASE(_i) + 0X0184)
#define SPI_TX_SSB_DAC(_i)			(BB_SPI_BASE(_i) + 0X0188)
#define SPI_BYP_PWR_OFS_PROBE_IQ(_i)		(BB_SPI_BASE(_i) + 0X018C)
#define SPI_LNA12_GAIN_FREQ_CURR_LOOPBK(_i)	(BB_SPI_BASE(_i) + 0X0190)
#define SPI_ICT_WCT_TX1_PA(_i)			(BB_SPI_BASE(_i) + 0X0194)
#define SPI_ICT_WCT_TX2_PA(_i)			(BB_SPI_BASE(_i) + 0X0198)
#define SPI_GCT_FCT_TX1_PA(_i)			(BB_SPI_BASE(_i) + 0X019C)
#define SPI_WCT_TX12_PA(_i)			(BB_SPI_BASE(_i) + 0X01A0)
#define SPI_MOD_ICT_ABS_VRRP(_i)		(BB_SPI_BASE(_i) + 0X01A4)
#define SPI_MOD_CCT_RCT(_i)			(BB_SPI_BASE(_i) + 0X01A8)
#define SPI_MOD_LO_GCT_TX_1MA(_i)		(BB_SPI_BASE(_i) + 0X01AC)
#define SPI_MODDAC_1M_IQ(_i)			(BB_SPI_BASE(_i) + 0X01B0)
#define SPI_MODDAC_1A_IQ(_i)			(BB_SPI_BASE(_i) + 0X01B4)
#define SPI_MOD_LO_GCT_TX_2MA(_i)		(BB_SPI_BASE(_i) + 0X01B8)
#define SPI_MODDAC_2M_IQ(_i)			(BB_SPI_BASE(_i) + 0X01BC)
#define SPI_MODDAC_2A_IQ(_i)			(BB_SPI_BASE(_i) + 0X01C0)
#define SPI_RX12_MUX_DATA_CNTL(_i)		(BB_SPI_BASE(_i) + 0X01C4)

// timer register definition
#define ARM_TIMER_BASE_ADDR				(0XF3000000)
#define ARM_TIMER_MEM_LEN					(0x30)
#define ARM_TIMER_PRESCALE_EN				(0x0000)
#define ARM_TIMER_PRESCALE_0				(0x0004)
#define ARM_TIMER_PRESCALE_1				(0x0008)
#define ARM_TIMER_CTL(_i)					(0x000c + (_i)*8)
#define ARM_TIMER_ENABLE					(QTN_BIT(0))
#define ARM_TIMER_PERIODIC					(QTN_BIT(1))
#define ARM_TIMER_PRESCALER_1				(QTN_BIT(2))

#define ARM_TIMER_CNT(_i)					(0x0010 + (_i)*8)

#if CONFIG_RUBY
#define  QT3_BB_GLBL_SOFT_RST		0xE6000008
#define  QT3_BB_TD_PA_CONF		0xE609048C

/* The following definitions used to install the steering vectors */
#define QT3_RF_NUM_CHAINS		4
#define QT3_BB_Q_MATRIX_MEM		0xE6100000
#define QT3_BB_Q_MATRIX_SIZE		0x8000
#endif

#endif /* _QTN_REGISTERS_H */
