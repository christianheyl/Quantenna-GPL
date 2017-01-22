/*
 * Copyright (c) 2010 Quantenna Communications, Inc.
 * All rights reserved.
 *
 *  SPI driver
 */


///////////////////////////////////////////////////////////////////////////////
//             Includes
///////////////////////////////////////////////////////////////////////////////
#include <common.h>
#include <command.h>
#include <asm/arch/platform.h>
#include <environment.h>
#include "ruby.h"
#include "pcie.h"
#include "pcie2.h"
#include "ruby_pcie_bda.h"

///////////////////////////////////////////////////////////////////////////////
//              Defines
///////////////////////////////////////////////////////////////////////////////
#define REG_READ(reg) readl(reg)
#ifdef REG_ACCESS_PRINT
#define REG_WRITE(reg, val)   do {\
		printf("\nwrite reg=%08x,val=%08x", reg, val);\
		writel((val), (reg));\
		printf(" read back val %08x", REG_READ(reg));} while(0)
#endif

#define REG_WRITE(reg, val)  writel((val), (reg))
#define MAX_ELE_NUM	16

#define PKT_CTRL_VAL	 (DMA_CTL_DST_M2 | DMA_CTL_SRC_M2 |\
				DMA_CTL_LLP_SRC_EN | DMA_CTL_LLP_DST_EN |\
				DMA_CTL_DST_WIDTH16 | DMA_CTL_SRC_WIDTH64 |\
				DMA_CTL_DST_BURST16 | DMA_CTL_SRC_BURST16)
#define DMA_BLOCK_LEN	0x600
#define CHANNEL		0


///////////////////////////////////////////////////////////////////////////////
//             Prototypes
///////////////////////////////////////////////////////////////////////////////
extern void setup_atu_outbound(volatile qdpc_pcie_bda_t *bda);
extern int memcmp(const void*, const void *, int);

void pcie_msi_map(u32 base);
void pcie_dma_rd(u32 sar, u32 dar, u32 size);
void pcie_dma_wr(u32 sar,u32 dar, u32 size);
void pcie_map_dump(void);
void topaz_ep_mmap(u32);
void pcie_mem_map(u32 base,u32 target,u32 size,u32 ch,u32 dir);
static int pcie_dma_ll(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
unsigned int hdma_move(unsigned long src, unsigned long dst, int len);
unsigned int hdma_ll_mv(unsigned long src, unsigned long dst, int len);

///////////////////////////////////////////////////////////////////////////////
//             Globals
///////////////////////////////////////////////////////////////////////////////
int atu_is_init = 0;

/***************************************************************************
   Function:
   Purpose:
   Returns:
   Note:    // todo - pci irqs have been combined on bbic4
 **************************************************************************/
void pcie_msi(void)
{
        REG_WRITE(PCIE_MSI_STATUS,BIT(0));
        printf("msi recieved\n");
}

void fill8_inc(u8 *dst, u32 len)
{
        u8 pattern = 0;
        while (len--)
                *dst++ = pattern++;
}

ulong inline get_time_val(void)
{
        return readl(RUBY_TIMER_VALUE(0));
}

void show_bandwidth(unsigned int prev_time, unsigned int cur_time, int len)
{
	unsigned int intval;
	unsigned int nsec;
	unsigned int tmp;

        intval = prev_time - cur_time;
        tmp = RUBY_FIXED_DEV_CLK/1000000;
        nsec = 1000/tmp;
        tmp *= len;
        tmp *= 8;
        tmp = (tmp + intval/2)/intval;
        nsec *= intval;

        printf("%dnS elapsed, bandwidth is %dMbps\n", nsec, tmp);

}

/* typpe
 * 0 ep
 * 1 rc
 */
int pcie_init(u32 type)
{
#define ARCSHELL_MMAP

	u32 rdata;

	REG_WRITE(RUBY_SYS_CTL_CPU_VEC_MASK,
		RUBY_SYS_CTL_RESET_IOSS | RUBY_SYS_CTL_RESET_PCIE);
	REG_WRITE(RUBY_SYS_CTL_CPU_VEC,
		RUBY_SYS_CTL_RESET_IOSS | RUBY_SYS_CTL_RESET_PCIE);

	rdata = 0;
	rdata = 1 << 16;
	rdata = rdata + 0xa2;
	REG_WRITE(PCIE_PORT_LINK_CTL, rdata);

	rdata = type * 4;
	rdata += (0x7fe0641 << 4);

	REG_WRITE(RUBY_SYS_CTL_PCIE_CFG0, rdata);
	REG_WRITE(RUBY_SYS_CTL_PCIE_CFG1, 0x00000001);
	REG_WRITE(RUBY_SYS_CTL_PCIE_CFG2, 0x0);
	REG_WRITE(RUBY_SYS_CTL_PCIE_CFG3, 0x45220000);
	REG_WRITE(PCIE_CMDSTS, 0x00100007);

	// set byte enables for cfg transactions
	REG_WRITE(RUBY_SYS_CTL_PCIE_SLV_REQ_MISC_INFO, 0xf << 22);
	return 0;
}

static int do_pcie_cmd(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	u32 type;
	int dma_access_dir = 1;

	if (strcmp(argv[1], "init") == 0) {
		if (argc < 3) {
			printf("pcie init type, type=rc or ep\n");
			return -1;
		}

		if (strcmp(argv[2], "rc") == 0) {
			type = 1;
		 } else if (strcmp(argv[2], "ep") == 0) {
			type = 0;
		 } else {
			 return -1;
		 }
		pcie_init(type);

	} else if (strcmp(argv[1], "mem_map") == 0) {
		u32 base, target, size, chan, dir;

		if (argc < 7) {
			printf("pcie mem_map base target size chan dir.\n");
			return -1;
		}
		base = simple_strtoul(argv[2], NULL, 16);
		target = simple_strtoul(argv[3], NULL, 16);
		size = simple_strtoul(argv[4], NULL, 16);
		chan = simple_strtoul(argv[5], NULL, 16);
		dir = simple_strtoul(argv[6], NULL, 16);
		pcie_mem_map(base, target, size, chan, dir);

	} else if (strcmp(argv[1], "cfg_map") == 0) {

	} else if ((strcmp(argv[1], "dma_wr") == 0) ||
			((dma_access_dir = strcmp(argv[1], "dma_rd")) == 0)) {
		u32 sar,dar,dma_size;
		if (argc < 5) {
			printf("pcie dma_wr sar dar dma_size.\n");
			return -1;
		}
		sar			= simple_strtoul(argv[2], NULL, 16);
		dar			= simple_strtoul(argv[3], NULL, 16);
		dma_size	= simple_strtoul(argv[4], NULL, 16);
		dcache_disable();
		topaz_ep_mmap(0);
		if(dma_access_dir)
			pcie_dma_wr(sar, dar, dma_size);
		else
			pcie_dma_rd(sar, dar, dma_size);

	} else if (strcmp(argv[1], "bar_map") == 0) {
		printf("\nbar_map hasn't implemented");
	} else if (strcmp(argv[1], "bar") == 0) {
		printf("\nbar hasn't implemented");
	} else if (strcmp(argv[1], "dump") == 0) {
		pcie_map_dump();
	} else if (strcmp(argv[1], "epmap") == 0) {
		u32 target;
		if (argc < 3)
			target = 0;
		else
			target = simple_strtoul(argv[2], NULL, 16);
		topaz_ep_mmap(target);
	} else if (strcmp(argv[1], "enmsi") == 0) {
		pcie_msi_map(0xce000000);
	} else if (!pcie_dma_ll(cmdtp, flag, argc, argv)){
		return 0;
	} else {
		printf("unknown command\n");
		return -1;
	}

	return 0;
}

/******************************************************************************
   Function:	pcie_cfg_map
   Purpose:		Set up ATU channel to generate config cycle
   Returns:
   Note:		RC only
 *****************************************************************************/
void pcie_cfg_map(u32 base,u32 size, u32 ch)
{
	u32 tmp = 0;

	REG_WRITE(PCIE_ATU_VIEW, ch);
	REG_WRITE(PCIE_ATU_LBAR, base);
	REG_WRITE(PCIE_ATU_UBAR, 0x00000000);
	REG_WRITE(PCIE_ATU_LAR, base + size);
	REG_WRITE(PCIE_ATU_LTAR, 0);
	REG_WRITE(PCIE_ATU_UTAR, 0);
	REG_WRITE(PCIE_ATU_CTL1, 4);
	REG_WRITE(PCIE_ATU_CTL2, PCIE_ATU_EN_REGION);
	tmp = REG_READ(PCIE_ATU_CTL2);
	printf("map config base:%x size:%x ch:%x\n", base,size, ch);
}

/******************************************************************************
   Function:	pcie_cfg_write
   Purpose:		Write EP config register
   Returns:
   Note:		RC only
 *****************************************************************************/
void pcie_cfg_write(u32 reg, u32 value)
{

	REG_WRITE(reg, value);

	#if 0
	pcie_cfg_map(PCIE_OB_REG_REGION,PCIE_CONFIG_SIZE);
	if (reg & 4) {
		u32 temp = REG_READ(PCIE_OB_REG_REGION + 0x10);
		REG_WRITE(PCIE_OB_REG_REGION + 0x10,value);
		REG_WRITE(PCIE_OB_REG_REGION + reg,value);
		REG_WRITE(PCIE_OB_REG_REGION + 0x10,temp);
	} else {
		REG_WRITE(PCIE_OB_REG_REGION + reg,value);
	}
	#endif
	printf("cfg write %x:%x\n",reg,value);
}

/******************************************************************************
   Function:	pcie_cfg_read
   Purpose:		Read EP config register
   Returns:
   Note:		RC only
 *****************************************************************************/
u32 pcie_cfg_read(u32 reg)
{
	//pcie_cfg_map(PCIE_OB_REG_REGION,PCIE_CONFIG_SIZE);
	//return REG_READ(PCIE_OB_REG_REGION + reg);
	return REG_READ(reg);
}

/**************************************************************************
    Function:	pcie_reg_write
    Purpose:	Write EP register
    Returns:
    Note:
 *************************************************************************/
void pcie_reg_write( u32 reg, u32 value)
{

	pcie_mem_map(PCIE_OB_REG_REGION,reg & 0xf0000000,PCIE_CONFIG_SIZE,PCIE_CONFIG_CH,PCIE_ATU_OB_REGION);
	REG_WRITE(PCIE_OB_REG_REGION + (reg & 0xffff),value);
}
/**************************************************************************
    Function:	pcie_reg_read
    Purpose:	read EP register
    Returns:
    Note:
 *************************************************************************/
u32 pcie_reg_read( u32 reg)
{

	pcie_mem_map(PCIE_OB_REG_REGION,reg & 0xf0000000,PCIE_CONFIG_SIZE,PCIE_CONFIG_CH,PCIE_ATU_OB_REGION);
	return REG_READ(PCIE_OB_REG_REGION + (reg & 0xffff));
}

void pcie_bar_map(u32 bar, u32 target, u32 size, u32 atu_chan)
{
	u32 tmp;
	REG_WRITE(PCIE_ATU_VIEW, atu_chan | PCIE_ATU_VIEW_INBOUND);
	REG_WRITE(PCIE_ATU_LAR, size);
	REG_WRITE(PCIE_ATU_LTAR, target);
	REG_WRITE(PCIE_ATU_UTAR, 0);
	REG_WRITE(PCIE_ATU_CTL1, 0);

	// inbound
	REG_WRITE(PCIE_ATU_LBAR, 0);
	REG_WRITE(PCIE_ATU_UBAR, 0);

	// base for inbound specifies BAR numbers
	REG_WRITE(PCIE_ATU_CTL2,
		((bar & 7) << 8) | PCIE_ATU_EN_REGION | PCIE_ATU_EN_MATCH);

	tmp = REG_READ(PCIE_ATU_CTL2);
	printf("map bar:%x target:%x size:%x, ch:%x\n",
		bar, target, size, atu_chan);
}

/******************************************************************************
   Function:	pcie_map
   Purpose:		Setup mapping backdoor from rc
   Returns:
   Note:		dir =  PCIE_ATU_VIEW_INBOUND or PCIE_ATU_VIEW_OUTBOUND
 *****************************************************************************/
void pcie_mem_map(u32 base,u32 target,u32 size,u32 ch,u32 dir)
{
	u32 tmp;

	REG_WRITE(PCIE_ATU_VIEW,ch|dir);
	REG_WRITE(PCIE_ATU_LBAR,base);
	REG_WRITE(PCIE_ATU_UBAR,0);
	REG_WRITE(PCIE_ATU_LAR,base + size-1);
	REG_WRITE(PCIE_ATU_LTAR,target);
	REG_WRITE(PCIE_ATU_UTAR,0);
	REG_WRITE(PCIE_ATU_CTL1,0);
	REG_WRITE(PCIE_ATU_CTL2,PCIE_ATU_EN_REGION);
	tmp = REG_READ(PCIE_ATU_CTL2);
	printf("map memory base:%x target:%x size:%x, ch:%x dir:%s\n",
		base,target,size,ch,dir==0 ? "out": "in");
}

/******************************************************************************
   Function:	pcie_msi_map
   Purpose:		Set up msi generation
   Returns:
   Note:
 *****************************************************************************/
void pcie_msi_map(u32 base)
{
	u32 tmp;
	REG_WRITE(PCIE_MSI_ADDR,base);
	REG_WRITE(PCIE_MSI_ADDR_UPPER,0);
	REG_WRITE(PCIE_MSI_ENABLE,BIT(0));
	REG_WRITE(PCIE_MSI_MASK,0);

	tmp = REG_READ(PCIE_MSI_STATUS);
}

/******************************************************************************
   Function:	pcie_int_map
   Purpose:		Setup intA ... mapping
   Returns:
   Note:
 *****************************************************************************/
void pcie_int_map(u32 base,u32 channel)
{
}

/******************************************************************************
   Function:	pcie_map
   Purpose:		Setup mapping backdoor from rc
   Returns:
   Note:
 *****************************************************************************/
void pcie_bar(u32 base,u32 bar)
{
	printf("set bar %x, addr:%x\n",bar,base);
	switch (bar) {
		case 0:
		REG_WRITE(PCIE_BAR0,base);
		break;

		case 1:
		REG_WRITE(PCIE_BAR1,base);
		break;

		case 2:
		REG_WRITE(PCIE_BAR2,base);
		break;

		case 3:
		REG_WRITE(PCIE_BAR3,base);
		break;

		case 4:
		REG_WRITE(PCIE_BAR4,base);
		break;
	}
	REG_WRITE(PCIE_CMD,6);
}

void pcie_map_dump(void)
{
	int i;
	int j;
	uint32_t tmp;
	/* read the inband */
	for (i = 0, j = 0; i < 4; i++) {
		REG_WRITE(PCIE_ATU_VIEW, PCIE_ATU_VIEW_INBOUND | i);
		tmp = REG_READ(PCIE_ATU_CTL2);
		if ((tmp & PCIE_ATU_EN_REGION) != PCIE_ATU_EN_REGION) {
			continue;
		}
		j++;
		if (tmp & PCIE_ATU_EN_MATCH) {
			tmp = (tmp >> 8) & 7;
			printf("i=%d, in barmatch: barno=%d, baraddr=%08x,"
				"target=%08x, size=%08x\n",i, tmp,
				REG_READ(RUBY_PCIE_BAR(tmp)),
				REG_READ(PCIE_ATU_LTAR),
					REG_READ(PCIE_ATU_LAR));
		} else {
			printf("i=%d, in: base=%08x, target=%08x, size=%08x\n", i,
				REG_READ(PCIE_ATU_LBAR), REG_READ(PCIE_ATU_LTAR),
				REG_READ(PCIE_ATU_LAR));
		}
	}
	if (j)
		printf("\n");
	else
		printf("No inATU enabled\n");

	for (i = 0, j = 0; i < 4; i++) {
		REG_WRITE(PCIE_ATU_VIEW, PCIE_ATU_VIEW_OUTBOUND | i);
		tmp = REG_READ(PCIE_ATU_CTL2);
		if ((tmp & PCIE_ATU_EN_REGION) != PCIE_ATU_EN_REGION) {
			continue;
		}
		j++;
		printf("i=%d,out: base=%08x, target=%08x, size=%08x\n", i,
			REG_READ(PCIE_ATU_LBAR), REG_READ(PCIE_ATU_LTAR),
			REG_READ(PCIE_ATU_LAR));
	}
	if (!j)
		printf("No outATU enabled\n");
}

/******************************************************************************
   Function:	pcie_dma_wr
   Purpose:	Use pcie dma to transfer from local mem to remote mem (write channel).
   Returns:
   Note:
 *****************************************************************************/

void pcie_dma_wr(u32 sar,u32 dar, u32 size)
{
	u32 tmp = 0;
	unsigned int prev_time = 0, cur_time = 0;
	sar = (sar >= PCIE_BASE_REGION) ? sar : virt_to_bus((void *)sar);
	dar = (dar >= PCIE_BASE_REGION) ? dar : virt_to_bus((void *)dar);

	REG_WRITE(PCIE_DMA_WR_ENABLE, 0x00000001);
	REG_WRITE(PCIE_DMA_WR_INTMASK, 0x00000000);
	REG_WRITE(PCIE_DMA_CHNL_CONTEXT, 0x00000000);
	REG_WRITE(PCIE_DMA_CHNL_CNTRL, 0x04000008);
	REG_WRITE(PCIE_DMA_XFR_SIZE,size);
	REG_WRITE(PCIE_DMA_SAR_LOW, sar);
	REG_WRITE(PCIE_DMA_SAR_HIGH, 0x00000000);
	REG_WRITE(PCIE_DMA_DAR_LOW, dar);
	REG_WRITE(PCIE_DMA_DAR_HIGH,0x00000000);
	prev_time = get_time_val();
	REG_WRITE(PCIE_DMA_WR_DOORBELL, 0x00000000);

	while((tmp & 1) == 0) {
		tmp = REG_READ(PCIE_DMA_WR_INTSTS);
	}
	cur_time = get_time_val();

	//Clear status bit so can be used for next transfer
	tmp |= 1;
	REG_WRITE(PCIE_DMA_WR_INTCLER, tmp);

	show_bandwidth(prev_time, cur_time, size);
}

/******************************************************************************
   Function:	pcie_dma_rd
   Purpose:	Use pcie dma to transfer from remote mem to local mem(Read channel).
   Returns:
   Note:
 *****************************************************************************/
void pcie_dma_rd(u32 sar, u32 dar, u32 size)
{
	u32 tmp;
	unsigned int prev_time, cur_time;
	tmp = 0;

	sar = (sar >= PCIE_BASE_REGION) ? sar : virt_to_bus((void *)sar);
	dar = (dar >= PCIE_BASE_REGION) ? dar : virt_to_bus((void *)dar);

	//printf(" INFO : Start PCIE-DMA programming RD Channel");
	REG_WRITE(PCIE_DMA_RD_ENABLE, 0x00000001);
	REG_WRITE(PCIE_DMA_RD_INTMASK, 0x00000000);
	REG_WRITE(PCIE_DMA_CHNL_CONTEXT, 0x80000000);
	REG_WRITE(PCIE_DMA_CHNL_CNTRL, 0x04000008);
	REG_WRITE(PCIE_DMA_XFR_SIZE,size);
	REG_WRITE(PCIE_DMA_SAR_LOW, sar);
	REG_WRITE(PCIE_DMA_SAR_HIGH,0x00000000);
	REG_WRITE(PCIE_DMA_DAR_LOW, dar);
	REG_WRITE(PCIE_DMA_DAR_HIGH,0x00000000);
	REG_WRITE(PCIE_DMA_RD_DOORBELL, 0x00000000);
	prev_time = get_time_val();
	//Now check if DMA transfer is done
	while((tmp & 1) == 0) {
		tmp = REG_READ(PCIE_DMA_RD_INTSTS);
	}
	cur_time = get_time_val();
	//DMA transfer is done. Check packet in SRAM.
	//Clear status bit so can be used for next transfer
	tmp = tmp | 1;
	REG_WRITE(PCIE_DMA_RD_INTCLER, tmp);

	show_bandwidth(prev_time, cur_time, size);
}

 /*linked list element structure*/
struct pcie_dma_ll_data {
	uint32_t ll_cb : 1;	/* cycle bit of the element*/
	uint32_t rsvd : 1;	/* reserved for data element */
	uint32_t ll_llp : 1;	/* load link pointer, 0 for data element*/
	uint32_t ll_lie : 1;	/* local interrupt enable, used only for data element*/
	uint32_t ll_rie : 1;	/* remote interrupt enable, only used for data element*/
	uint32_t ll_trans_size;	/* transfer size for the data block pointed by the sar */
	uint32_t ll_sar_low;	/* low source address in local memory for data element; */
	uint32_t ll_sar_high;	/* high source address in local memory for data element*/
	uint32_t ll_dar_low;	/* low source address in the remote memory for data element */
	uint32_t ll_dar_high;	/* high source address in the remote memory for data element */
};

struct pcie_dma_ll_desc {
	uint32_t ll_cb : 1;	/* cycle bit of the element*/
	uint32_t ll_tcb : 1;	/* toggle cycle bit, used only for descriptor element */
	uint32_t ll_llp : 1;	/* load link pointer, 1 for descriptor element*/
	uint32_t rsvd1;			/* reserved for descriptor element */
	uint32_t ll_elem_ptr_low;	/* low  pointer value for the LL element */
	uint32_t ll_elem_ptr_high;	/* high pointer value for the LL element */
	uint32_t rsvd2[2];
};


static int pcie_dma_ll(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	struct pcie_dma_ll_data dma_param[MAX_ELE_NUM + 1];
	struct pcie_dma_ll_desc *dma_llp;
	unsigned int prev_time = 0;
	unsigned int cur_time = 0;
	int i;
	u32 tmp = 0;
	u32 sar;
	u32 dar;
	int sumsz = 0;
	u32 bsar;
	u32 bdar;
	int size;

	if (argc < 1) {
		printf("pcie llr/llw src dst sumsz\n");
		return -1;
	}

	dcache_disable();
	topaz_ep_mmap(0);

	memset(dma_param, 0, sizeof(dma_param));

	sar = simple_strtoul(argv[2], NULL, 16);
	dar = simple_strtoul(argv[3], NULL, 16);
	sumsz = simple_strtoul(argv[4], NULL, 16);
	if ((sumsz + DMA_BLOCK_LEN - 1) / DMA_BLOCK_LEN > MAX_ELE_NUM) {
		printf("dma desc isn't enough\n");
		sumsz = DMA_BLOCK_LEN * MAX_ELE_NUM;
	}

	size = sumsz;
	bsar = (sar >= PCIE_BASE_REGION) ? sar : virt_to_bus((void *)sar);
	bdar = (dar >= PCIE_BASE_REGION) ? dar : virt_to_bus((void *)dar);
	printf("bsar\t\tbdar\t\tsize\n");
	for (i = 0; i < MAX_ELE_NUM; i++) {
		dma_param[i].ll_cb = 1;
		dma_param[i].ll_trans_size = (size > DMA_BLOCK_LEN) ? DMA_BLOCK_LEN : size;
		dma_param[i].ll_sar_low = bsar;
		dma_param[i].ll_dar_low = bdar;
		printf("%08x\t%08x\t%08x\n", bsar, bdar, dma_param[i].ll_trans_size);

		bsar += DMA_BLOCK_LEN;
		bdar += DMA_BLOCK_LEN;
		size -= DMA_BLOCK_LEN;
		if (size <= 0)
			break;
	}
	dma_param[i].ll_lie = 1;

	memset((void *)dar, 0, sumsz);
	/* Don't memset sar address, it may crash linux kernel if RC run with linux */

	dma_llp = (struct pcie_dma_ll_desc *)&dma_param[i + 1];
	dma_llp->ll_llp = 1;

	dcache_enable();

	if (!strcmp(argv[1], "llr")) {
#if 0
		memset((void *)0x83000000, 0x58, 0x4000);
		memset((void *)0xb3000000, 0, 0x4000);
		//hdma_move(0x88004000, 0xb3000000, 0x4000, 16);
		hdma_ll_mv(0x88040000, 0xb3000000, 0x4000);
#endif
		REG_WRITE(PCIE_DMA_RD_ENABLE, 0x00000001);
		REG_WRITE(PCIE_DMA_RD_INTMASK, 0x00000000);
		REG_WRITE(PCIE_DMA_CHNL_CONTEXT, 0x80000000);
		REG_WRITE(PCIE_DMA_CHNL_CNTRL, 0x04000308);

		REG_WRITE(PCIE_DMA_LLPTR_LOW, virt_to_bus(dma_param));
		REG_WRITE(PCIE_DMA_LLPTR_HIGH, 0);

		REG_WRITE(PCIE_DMA_RD_DOORBELL, 0x00000000);
		prev_time = get_time_val();


		for (i = 0; i < 0x1000000; i++) {
			tmp = REG_READ(PCIE_DMA_RD_INTSTS);
			if (tmp & 1) {
				cur_time = get_time_val();
				break;
			}
		}
		REG_WRITE(PCIE_DMA_RD_INTCLER, tmp);

	} else if (!strcmp(argv[1], "llw")) {
		REG_WRITE(PCIE_DMA_WR_ENABLE, 0x00000001);
		REG_WRITE(PCIE_DMA_WR_INTMASK, 0x00000000);
		REG_WRITE(PCIE_DMA_CHNL_CONTEXT, 0x00000000);
		REG_WRITE(PCIE_DMA_CHNL_CNTRL, 0x04000308);

		REG_WRITE(PCIE_DMA_LLPTR_LOW, virt_to_bus(dma_param));
		REG_WRITE(PCIE_DMA_LLPTR_HIGH, 0);

		REG_WRITE(PCIE_DMA_WR_DOORBELL, 0x00000000);
		prev_time = get_time_val();

		for (i = 0; i < 0x1000000; i++) {
			tmp = REG_READ(PCIE_DMA_WR_INTSTS);
			if (tmp & 1) {
				cur_time = get_time_val();
				break;
			}
		}
		REG_WRITE(PCIE_DMA_WR_INTCLER, tmp);
	} else {
		return -1;
	}

	show_bandwidth(prev_time, cur_time, sumsz);

	if (memcmp((void *)dar, (void *)sar, sumsz) != 0)
		printf("DMA failed\n");

	return 0;
}

void topaz_ep_mmap(u32 target)
{
	volatile qdpc_pcie_bda_t *bda = (qdpc_pcie_bda_t *)(RUBY_PCIE_BDA_ADDR);
	u32 tmp;

	if (atu_is_init)
		return;
	printf("Waiting for the pcie link up, ctl + c to break\n");
	while (1) {
		if ((readl(TOPAZ_PCIE_STAT) & TOPAZ_PCIE_LINKUP) == TOPAZ_PCIE_LINKUP)
		break;
		if (ctrlc()) {
			printf("fail to link to host, break.\n");
			break;
		}
		udelay(10);
	}

	writel(0xce000000, PCIE_MSI_LOW_ADDR);
	writel(readl(PCIE_MSI_CAP) & ~RUBY_PCIE_MSI_ENABLE, PCIE_MSI_CAP); /* Enabled by RC if need to */
	setup_atu_outbound(bda);
	pcie_bar_map(PCIE_BAR_DMAREG, PCIE_BAR_DMAREG_LO, PCIE_BAR_DMAREG_LEN,
			PCIE_DMAREG_REGION);
	pcie_mem_map(PCIE_BASE_REGION, target, 0xff70000, 1,
			PCIE_ATU_VIEW_OUTBOUND);
	pcie_mem_map(0xbff70000, 0x80000000, 0x80000, 2, PCIE_ATU_VIEW_OUTBOUND);
	writel(PCIE_BAR_CFG(0) | 0xc0000008, RUBY_PCIE_BAR(PCIE_BAR_SYSCTL));
	writel(PCIE_BAR_CFG(0) | 0xc0010008, RUBY_PCIE_BAR(PCIE_BAR_SHMEM));
	writel(PCIE_BAR_CFG(0) | 0xc0020008, RUBY_PCIE_BAR(PCIE_BAR_DMAREG));
	writel(PCIE_MEM_EN | PCIE_BUS_MASTER_EN, RUBY_PCIE_CMD_REG);
	atu_is_init = 1;

	tmp = readl(0xb0000000);
	tmp = readl(0xb0000000);
	writel(PCIE_LINK_GEN2, PCIE_LINK_CTL2);
	udelay(10000);
	printf("PCIe Gen: %x\n", PCIE_LINK_MODE(readl(PCIE_LINK_STAT)));
}

unsigned int hdma_move(unsigned long src, unsigned long dst, int len)
{
	dst = (dst >= PCIE_BASE_REGION) ? dst : virt_to_bus((void *)dst);
	src = (src >= PCIE_BASE_REGION) ? src : virt_to_bus((void *)src);
	printf("dst=%x, src=%x, len = %x\n", (unsigned int)dst, (unsigned int)src, len);

	REG_WRITE(DMA_DMA_CFG, DMA_CFG_ENABLE);
	REG_WRITE(DMA_MSK_TFR, 0xf0f);
	REG_WRITE(DMA_CH_EN, BIT(CHANNEL) << 8);
	REG_WRITE(DMA_SAR(CHANNEL), src);
	REG_WRITE(DMA_DAR(CHANNEL), dst);
	REG_WRITE(DMA_LLP(CHANNEL), 0);
	REG_WRITE(DMA_CTL(CHANNEL), PKT_CTRL_VAL);
	REG_WRITE(DMA_SIZE(CHANNEL), (len + 7) / 8);
	REG_WRITE(DMA_CH_EN,(BIT(CHANNEL) <<8) | BIT(CHANNEL));

	return get_time_val();
}

typedef struct dma_desc {
	u32		sar;
	u32		dar;
	struct dma_desc *llp;
	u32		ctl;
	u32		size;
} dma_desc_t;

unsigned int hdma_ll_mv(unsigned long src, unsigned long dst, int len)
{
	struct dma_desc hdma_dsc[MAX_ELE_NUM];
	struct dma_desc *dsc = hdma_dsc;
	int i;

	if (((len + DMA_BLOCK_LEN - 1) / DMA_BLOCK_LEN) > MAX_ELE_NUM) {
		printf("chain isn't engough\n");
		len = MAX_ELE_NUM * DMA_BLOCK_LEN;
	}

	dst = (dst >= PCIE_BASE_REGION) ? dst : virt_to_bus((void *)dst);
	src = (src >= PCIE_BASE_REGION) ? src : virt_to_bus((void *)src);
	printf("ctl\t\tsrc\t\tdar\t\tllp\t\tsize\n");
	for (i = 0; i < MAX_ELE_NUM; i++) {
		dsc->ctl = PKT_CTRL_VAL;
		dsc->sar = src;
		dsc->dar = dst;
		dsc->llp = (void *)virt_to_bus(dsc + 1);
		dsc->size = ((len > DMA_BLOCK_LEN) ? DMA_BLOCK_LEN : len) / 8;
		printf("0x%08x\t0x%08x\t0x%08x\t0x%p\t0x%08x\n",
			dsc->ctl, dsc->sar, dsc->dar, dsc->llp, dsc->size);
		dst += DMA_BLOCK_LEN;
		src += DMA_BLOCK_LEN;
		len -= DMA_BLOCK_LEN;
		if (len <= 0) {
			dsc->llp = NULL;
			break;
		}
		dsc++;
	}

	dcache_disable();

	REG_WRITE(DMA_DMA_CFG, DMA_CFG_ENABLE);
	REG_WRITE(DMA_MSK_TFR, 0xf0f);
	REG_WRITE(DMA_CTL(CHANNEL), PKT_CTRL_VAL);
	REG_WRITE(DMA_LLP(CHANNEL), virt_to_bus(hdma_dsc));
	REG_WRITE(DMA_CH_EN,(BIT(CHANNEL) << 8) | BIT(CHANNEL));
	return get_time_val();
}

static int do_hdmaw(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	unsigned long dst;
	unsigned long src;
	int len;
	int mode;
	unsigned int prev_time;
	unsigned int cur_time;

	src = simple_strtoul(argv[1], NULL, 16);
	dst = simple_strtoul(argv[2], NULL, 16);
	len = simple_strtoul(argv[3], NULL, 16);

	dcache_disable();
	topaz_ep_mmap(0);

	fill8_inc((void *)src, len);

	memset((void *)dst, 0, len);

	if (argc >= 5)
		mode = simple_strtoul(argv[4], NULL, 16);
	else
		mode = 0;

	if (mode) {
		printf("ll mode\n");
		prev_time = hdma_ll_mv(src, dst, len);
	} else {
		printf("non-ll mode\n");
		prev_time = hdma_move(src, dst, len);
	}

	while (REG_READ(DMA_CH_EN) & BIT(CHANNEL));

	cur_time = get_time_val();

	show_bandwidth(prev_time, cur_time, len);

	return 0;
}

/* pcie test command */
U_BOOT_CMD(pcie,CONFIG_SYS_MAXARGS, 0, do_pcie_cmd,
	"pcie operation sub-system",
	"pcie init rc/ep                        - initialize the pcie as rc/ep\n"
	"pcie mem_map base target size chan dir - configure the inbout(dir=1) and outbound(dir=0)\n"
	"pcie dma_wr sar dar size               - Use pcie dma to transfer from local mem to remote mem\n"
	"pcie dma_rd sar dar size               - Use pcie dma to transfer from remote mem to local mem\n"
	NULL);

/* pcie test command */
U_BOOT_CMD(hdmaw, CONFIG_SYS_MAXARGS, 0, do_hdmaw,
		"Hdma debug",
                "dst, src, len, mode\n"
                NULL);

