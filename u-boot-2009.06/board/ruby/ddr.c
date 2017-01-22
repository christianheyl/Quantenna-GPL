/*
 * Copyright (c) 2009 Quantenna Communications, Inc.
 * All rights reserved.
 *
 *  Initialize DDR
 */

#include <shared_defs_common.h>
#include "ruby.h"
#include "ddr.h"

#define lhost_ahb_read_data(x,y) (y=REG_READ(x))
#define lhost_ahb_write(x,y) (REG_WRITE(x,y))

u32 ddr_size(void)
{
	unsigned long i = 0, val = 0, size = 0;
	const unsigned long test_pattern = 0x12345678;
	volatile unsigned long *pmarker = (volatile unsigned long *) 0;

	*pmarker = ~test_pattern;
        udelay(1);

	/* Probe SDRAM to find out how much we have */
        val = *pmarker;
	if (*pmarker != ~test_pattern) {
		printf("No DDR detected\n");
		/* If running in SRAM do not flag no SDRAM as a fatal error */
		return 0;
	}

	for (i = 0x200000; i < 512 * 1024 * 1024; i <<= 1) {
		if (i < (unsigned long)pmarker) {
			/* Only start testing for memory address wraps
			 * above our current location.
			 */
			continue;
		}
		val = *(volatile unsigned long *)(i + (unsigned long)pmarker);
		*(volatile unsigned long *)(i + (unsigned long)pmarker) = test_pattern;
		if (*pmarker == test_pattern) {
			/* Writes have wrapped */
			size	 = i;
			break;
		}
		*(volatile unsigned long *)(i + (unsigned long)pmarker) = val;
	}

	if (size == 0) {
		printf("Cannot autodetect DDR size\n");
	} else {
		printf("DDR size %d MB\n",(int)size/(1024*1024));
	}

	return size;
}

#if TOPAZ_FPGA_PLATFORM
int ddr_init (u32 type, u32 speed, u32 size)
{
        uint32_t rdata;
	printf("Topaz FPGA - DDR3 UMCTL-2\n");
	udelay(1000);

	REG_WRITE(SYS_RST_MASK, SYS_RST_DRAM);
	REG_WRITE(SYS_RST_VEC, 0);

        /* SYNOP step 1 - program umctl2 registers */
        rdata = 0x00040001; //x32
        REG_WRITE(DDR_MSTR, rdata);
        rdata = REG_READ(DDR_STAT);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.MRCTRL0 -- 4 bytes @'h0000000000000010
           mr_rank[4-4] = 1'h1
           mr_addr[14-12] = 3'h0
           mr_wr[31-31] = 1'h0 */
        rdata = 0x00000010;
        REG_WRITE(DDR_MRCTRL0, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.MRCTRL1 -- 2 bytes @'h0000000000000014
           mr_data[15-0] = 16'hcdb8 */
        rdata = 0x0000cdb8;
        REG_WRITE(DDR_MRCTRL1, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.MRSTAT -- 1 bytes @'h0000000000000018
           mr_wr_busy[0-0] = 1'h0 */
        rdata = REG_READ(DDR_MRSTAT);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.PWRCTL -- 1 bytes @'h0000000000000030
           selfref_en[0-0] = 1'h0
           powerdown_en[1-1] = 1'h0
           en_dfi_dram_clk_disable[3-3] = 1'h0 */
        rdata = 0x00000000;
        REG_WRITE(DDR_PWRCTL, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.PWRTMG -- 1 bytes @'h0000000000000034
           powerdown_to_x32[4-0] = 5'h03 */
        rdata = 0x00000003;
        REG_WRITE(DDR_PWRTMG, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.RFSHCTL0 -- 3 bytes @'h0000000000000050
           refresh_burst[10-8] = 3'h1
           refresh_to_x32[16-12] = 5'h1f
           refresh_margin[23-20] = 4'h2 */
        rdata = 0x0021f100;
        REG_WRITE(DDR_RFSHCTL0, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.RFSHTMG -- 4 bytes @'h0000000000000064
           t_rfc_min[8-0] = 9'h015
           t_rfc_nom_x32[27-16] = 12'h00a */
        rdata = 0x000a0015;
        REG_WRITE(DDR_RFSHTMG, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.RFSHCTL3 -- 1 bytes @'h0000000000000060
           dis_auto_refresh[0-0] = 1'h1
           refresh_update_level[1-1] = 1'h1 */
        rdata = 0x00000003;
        REG_WRITE(DDR_RFSHCTL3, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.PARCTL -- 1 bytes @'h00000000000000c0
           dfi_parity_err_int_en[0-0] = 1'h0
           dfi_parity_err_int_clr[1-1] = 1'h0
           dfi_parity_err_cnt_clr[2-2] = 1'h0 */
        rdata = 0x00000000;
        REG_WRITE(DDR_PARCTL, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.PARSTAT -- 3 bytes @'h00000000000000c4
           dfi_parity_err_cnt[15-0] = 16'h0000
           dfi_parity_err_int[16-16] = 1'h0 */
        rdata = REG_READ(DDR_PARSTAT);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.INIT0 -- 4 bytes @'h00000000000000d0
           pre_cke_x1024[9-0] = 10'h001
           post_cke_x1024[25-16] = 10'h008 */
        rdata = 0x00080001;
        REG_WRITE(DDR_INIT0, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.INIT1 -- 3 bytes @'h00000000000000d4
           pre_ocd_x32[3-0] = 4'h0
           final_wait_x32[14-8] = 7'h00
           dram_rstn_x1024[23-16] = 8'h01 */
        rdata = 0x00010000;
        REG_WRITE(DDR_INIT1, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.INIT3 -- 4 bytes @'h00000000000000dc
           emr[15-0] = 16'h000b
           mr[31-16] = 16'h0420 */
        rdata = 0x0420000b;
        REG_WRITE(DDR_INIT3, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.INIT4 -- 4 bytes @'h00000000000000e0
           emr3[15-0] = 16'h0000
           emr2[31-16] = 16'h0008 */
        rdata = 0x00080000;
        REG_WRITE(DDR_INIT4, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.INIT5 -- 3 bytes @'h00000000000000e4
           dev_zqinit_x32[23-16] = 8'h10 */
        rdata = 0x00100000;
        REG_WRITE(DDR_INIT5, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.DIMMCTL -- 1 bytes @'h00000000000000f0
           dimm_stagger_cs_en[0-0] = 1'h0
           dimm_addr_mirr_en[1-1] = 1'h0 */
        rdata = 0x00000000;
        REG_WRITE(DDR_DIMMCTL, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.DRAMTMG0 -- 4 bytes @'h0000000000000100
           t_ras_min[5-0] = 6'h0c
           t_ras_max[13-8] = 6'h2
           t_faw[21-16] = 6'h04
           wr2pre[29-24] = 6'h0a */
        rdata = 0x0a04020c;
        REG_WRITE(DDR_DRAMTMG0, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.DRAMTMG1 -- 3 bytes @'h0000000000000104
           t_rc[5-0] = 6'h0f
           rd2pre[12-8] = 5'h04
           t_xp[20-16] = 5'h02 */
        rdata = 0x0002040f;
        REG_WRITE(DDR_DRAMTMG1, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.DRAMTMG2 -- 2 bytes @'h0000000000000108
           wr2rd[5-0] = 6'h0a
           rd2wr[12-8] = 5'h03 */
        rdata = 0x0000030a;
        REG_WRITE(DDR_DRAMTMG2, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.DRAMTMG3 -- 2 bytes @'h000000000000010c
           t_mod[9-0] = 10'h006
           t_mrd[14-12] = 3'h2 */
        rdata = 0x00002006;
        REG_WRITE(DDR_DRAMTMG3, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.DRAMTMG4 -- 4 bytes @'h0000000000000110
           t_rp[3-0] = 4'h3
           t_rrd[10-8] = 3'h2
           t_ccd[18-16] = 3'h2
           t_rcd[27-24] = 4'h1 */
        rdata = 0x01020203;
        REG_WRITE(DDR_DRAMTMG4, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.DRAMTMG5 -- 4 bytes @'h0000000000000114
           t_cke[3-0] = 4'h2
           t_ckesr[13-8] = 6'h03
           t_cksre[19-16] = 4'h4
           t_cksrx[27-24] = 4'h4 */
        rdata = 0x04040302;
        REG_WRITE(DDR_DRAMTMG5, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.DRAMTMG8 -- 1 bytes @'h0000000000000120
           post_selfref_gap_x32[6-0] = 7'h10 */
        rdata = 0x00000010;
        REG_WRITE(DDR_DRAMTMG8, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.ZQCTL0 -- 4 bytes @'h0000000000000180
           t_zq_short_nop[9-0] = 10'h020
           t_zq_long_nop[25-16] = 10'h080
           dis_srx_zqcl[30-30] = 1'h0
           dis_auto_zq[31-31] = 1'h0 */
         rdata = 0x00800020;
         REG_WRITE(DDR_ZQCTL0, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.ZQCTL1 -- 3 bytes @'h0000000000000184
            t_zq_short_interval_x1024[19-0] = 20'h00070 */
         rdata = 0x00000070;
         REG_WRITE(DDR_ZQCTL1, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.DFITMG0 -- 4 bytes @'h0000000000000190
            write_latency[4-0] = 5'h04
            dfi_tphy_wrdata[12-8] = 5'h01
            dfi_t_rddata_en[20-16] = 5'h04
            dfi_t_ctrl_delay[27-24] = 4'h2 */
         rdata = 0x02040104;
         REG_WRITE(DDR_DFITMG0, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.DFITMG1 -- 2 bytes @'h0000000000000194
            dfi_t_dram_clk_enable[3-0] = 4'h2
            dfi_t_dram_clk_disable[11-8] = 4'h2 */
         rdata = 0x00000202;
         REG_WRITE(DDR_DFITMG1, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.DFIUPD0 -- 4 bytes @'h00000000000001a0
            dfi_t_ctrlup_min[9-0] = 10'h003
            dfi_t_ctrlup_max[25-16] = 10'h040
            dis_dll_calib[31-31] = 1'h1 */
         rdata = 0x80400003;
         REG_WRITE(DDR_DFIUPD0, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.DFIUPD1 -- 3 bytes @'h00000000000001a4
            dfi_t_ctrlupd_interval_max_x1024[7-0] = 8'h26
            dfi_t_ctrlupd_interval_min_x1024[23-16] = 8'h22 */
         rdata = 0x00220026;
         REG_WRITE(DDR_DFIUPD1, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.DFIUPD2 -- 4 bytes @'h00000000000001a8
            dfi_phyupd_type0[11-0] = 12'h94f
            dfi_phyupd_type1[27-16] = 12'hfff
            dfi_phyupd_en[31-31] = 1'h0 */
         rdata = 0x0fff094f;
         REG_WRITE(DDR_DFIUPD2, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.DFIUPD3 -- 4 bytes @'h00000000000001ac
            dfi_phyupd_type2[11-0] = 12'h4d6
            dfi_phyupd_type3[27-16] = 12'h954 */
         rdata = 0x095404d6;
         REG_WRITE(DDR_DFIUPD3, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.DFIMISC -- 1 bytes @'h00000000000001b0
            dfi_init_complete_en[0-0] = 1'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_DFIMISC, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.ADDRMAP1 -- 3 bytes @'h0000000000000204
	    for x32
		    addrmap_bank_b0[3-0] = 4'h7
		    addrmap_bank_b1[11-8] = 4'h7
		    addrmap_bank_b2[19-16] = 4'h7
	    for x16
		    addrmap_bank_b0[3-0] = 4'h6
		    addrmap_bank_b1[11-8] = 4'h6
		    addrmap_bank_b2[19-16] = 4'h6 */
         rdata = 0x00070707;
         REG_WRITE(DDR_ADDRMAP1, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.ADDRMAP2 -- 4 bytes @'h0000000000000208
            addrmap_col_b2[3-0] = 4'h0
            addrmap_col_b3[11-8] = 4'h0
            addrmap_col_b4[19-16] = 4'h0
            addrmap_col_b5[27-24] = 4'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_ADDRMAP2, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.ADDRMAP3 -- 4 bytes @'h000000000000020c
            addrmap_col_b6[3-0] = 4'h0
            addrmap_col_b7[11-8] = 4'h0
	    addrmap_col_b8[19-16] = 4'hf for x16
            addrmap_col_b8[19-16] = 4'h0 for x32
            addrmap_col_b9[27-24] = 4'hf */
         rdata = 0x0f000000;
         REG_WRITE(DDR_ADDRMAP3, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.ADDRMAP4 -- 2 bytes @'h0000000000000210
            addrmap_col_b10[3-0] = 4'hf
            addrmap_col_b11[11-8] = 4'hf */
         rdata = 0x00000f0f;
         REG_WRITE(DDR_ADDRMAP4, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.ADDRMAP5 -- 4 bytes @'h0000000000000214
	    for x32
		    addrmap_row_b0[3-0] = 4'h6
		    addrmap_row_b1[11-8] = 4'h6
		    addrmap_row_b2_10[19-16] = 4'h6
		    addrmap_row_b11[27-24] = 4'h6
	    for x16
		    addrmap_row_b0[3-0] = 4'h5
		    addrmap_row_b1[11-8] = 4'h5
		    addrmap_row_b2_10[19-16] = 4'h5
		    addrmap_row_b11[27-24] = 4'h5 */
         rdata = 0x06060606;
         REG_WRITE(DDR_ADDRMAP5, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.ADDRMAP6 -- 4 bytes @'h0000000000000218
	    for x32
		addrmap_row_b12[3-0] = 4'h6
		addrmap_row_b13[11-8] = 4'hf
	    for x16
		addrmap_row_b12[3-0] = 4'h5
		addrmap_row_b13[11-8] = 4'h5
		addrmap_row_b14[19-16] = 4'hf
		addrmap_row_b15[27-24] = 4'hf */
         rdata = 0x0f0f0f06;
         REG_WRITE(DDR_ADDRMAP6, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.ODTCFG -- 4 bytes @'h0000000000000240
            wr_odt_delay[19-16] = 4'h0
            wr_odt_hold[27-24] = 4'h2 */
         rdata = 0x02000000;
         REG_WRITE(DDR_ODTCFG, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.ODTMAP -- 1 bytes @'h0000000000000244
            rank0_wr_odt[0-0] = 1'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_ODTMAP, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.SCHED -- 4 bytes @'h0000000000000250
            force_low_pri_n[0-0] = 1'h1
            prefer_write[1-1] = 1'h0
            pageclose[2-2] = 1'h1
            lpr_num_entries[11-8] = 4'hc
            go2critical_hysteresis[23-16] = 8'h1d
            rdwr_idle_gap[30-24] = 7'h10 */
         rdata = 0x101d0c05;
         REG_WRITE(DDR_SCHED, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.PERFHPR0 -- 2 bytes @'h0000000000000258
            hpr_min_non_critical[15-0] = 16'hda6f
            rdata = 0x0000da6f;
            REG_WRITE(DDR_PERFHPR0, rdata);
            Register DWC_ddr_umctl2_map_UMCTL2_REGS.PERFHPR1 -- 4 bytes @'h000000000000025c
            hpr_max_starve[15-0] = 16'h1a0c
            hpr_xact_run_length[31-24] = 8'hd5
            rdata = 0xd5001a0c;
            REG_WRITE(DDR_PERFHPR1, rdata);
            Register DWC_ddr_umctl2_map_UMCTL2_REGS.PERFLPR0 -- 2 bytes @'h0000000000000260
            lpr_min_non_critical[15-0] = 16'hd248
            rdata = 0x0000d248;
            REG_WRITE(DDR_PERFLPR0, rdata);
            Register DWC_ddr_umctl2_map_UMCTL2_REGS.PERFLPR1 -- 4 bytes @'h0000000000000264
            lpr_max_starve[15-0] = 16'h49fa
            lpr_xact_run_length[31-24] = 8'h25
            rdata = 0x250049fa;
            REG_WRITE(DDR_PERFLPR1, rdata);
            Register DWC_ddr_umctl2_map_UMCTL2_REGS.PERFWR0 -- 2 bytes @'h0000000000000268
            w_min_non_critical[15-0] = 16'h7390
            rdata = 0x00007390;
            REG_WRITE(DDR_PERFWR0, rdata);
            Register DWC_ddr_umctl2_map_UMCTL2_REGS.PERFWR1 -- 4 bytes @'h000000000000026c
            w_max_starve[15-0] = 16'hcafe
            w_xact_run_length[31-24] = 8'h97
            rdata = 0x9700cafe;
            REG_WRITE(DDR_PERFWR1, rdata);
            Register DWC_ddr_umctl2_map_UMCTL2_REGS.DBG0 -- 1 bytes @'h0000000000000300
            dis_wc[0-0] = 1'h1
            dis_rd_bypass[1-1] = 1'h0
            dis_act_bypass[2-2] = 1'h0
            dis_collision_page_opt[4-4] = 1'h1 */
         rdata = 0x00000011;
         REG_WRITE(DDR_DBG0, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.DBG1 -- 1 bytes @'h0000000000000304
            dis_dq[0-0] = 1'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_DBG1, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.DBGCAM -- 4 bytes @'h0000000000000308
            dbg_hpr_q_depth[4-0] = 5'h00
            dbg_lpr_q_depth[12-8] = 5'h00
            dbg_w_q_depth[20-16] = 5'h00
            dbg_stall[24-24] = 1'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_DBGCAM, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCCFG -- 1 bytes @'h0000000000000400
            go2critical_en[0-0] = 1'h0
            pagematch_limit[4-4] = 1'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCCFG, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGR_0 -- 2 bytes @'h0000000000000404
            rd_port_priority[9-0] = 10'h000
            read_reorder_bypass_en[11-11] = 1'h0
            rd_port_aging_en[12-12] = 1'h0
            rd_port_urgent_en[13-13] = 1'h0
            rd_port_pagematch_en[14-14] = 1'h0
            rd_port_hpr_en[15-15] = 1'h0
	    rdwr_ordered_en[16-16] = 1 */
         rdata = 0x00010000;
         REG_WRITE(DDR_PCFGR_0, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGW_0 -- 2 bytes @'h0000000000000408
            wr_port_priority[9-0] = 10'h000
            wr_port_aging_en[12-12] = 1'h0
            wr_port_urgent_en[13-13] = 1'h0
            wr_port_pagematch_en[14-14] = 1'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGW_0, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDMASKCH0_0 -- 1 bytes @'h000000000000040c
            id_mask[3-0] = 4'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGIDMASKCH0_0, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDVALUECH0_0 -- 1 bytes @'h0000000000000410
            id_value[3-0] = 4'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGIDVALUECH0_0, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGR_1 -- 2 bytes @'h00000000000004b4
            rd_port_priority[9-0] = 10'h000
            read_reorder_bypass_en[11-11] = 1'h0
            rd_port_aging_en[12-12] = 1'h0
            rd_port_urgent_en[13-13] = 1'h0
            rd_port_pagematch_en[14-14] = 1'h0
            rd_port_hpr_en[15-15] = 1'h0
	    rdwr_ordered_en[16-16] = 1 */
         rdata = 0x00010000;
         REG_WRITE(DDR_PCFGR_1, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGW_1 -- 2 bytes @'h00000000000004b8
            wr_port_priority[9-0] = 10'h000
            wr_port_aging_en[12-12] = 1'h0
            wr_port_urgent_en[13-13] = 1'h0
            wr_port_pagematch_en[14-14] = 1'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGW_1, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDMASKCH0_1 -- 1 bytes @'h00000000000004bc
            id_mask[3-0] = 4'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGIDMASKCH0_1, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDVALUECH0_1 -- 1 bytes @'h00000000000004c0
            id_value[3-0] = 4'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGIDVALUECH0_1, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGR_2 -- 2 bytes @'h0000000000000564
            rd_port_priority[9-0] = 10'h000
            read_reorder_bypass_en[11-11] = 1'h0
            rd_port_aging_en[12-12] = 1'h0
            rd_port_urgent_en[13-13] = 1'h0
            rd_port_pagematch_en[14-14] = 1'h0
            rd_port_hpr_en[15-15] = 1'h0
	    rdwr_ordered_en[16-16] = 1 */
         rdata = 0x00010000;
         REG_WRITE(DDR_PCFGR_2, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGW_2 -- 2 bytes @'h0000000000000568
            wr_port_priority[9-0] = 10'h000
            wr_port_aging_en[12-12] = 1'h0
            wr_port_urgent_en[13-13] = 1'h0
            wr_port_pagematch_en[14-14] = 1'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGW_2, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDMASKCH0_2 -- 1 bytes @'h000000000000056c
            id_mask[3-0] = 4'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGIDMASKCH0_2, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDVALUECH0_2 -- 1 bytes @'h0000000000000570
            id_value[3-0] = 4'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGIDVALUECH0_2, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGR_3 -- 2 bytes @'h0000000000000614
            rd_port_priority[9-0] = 10'h000
            read_reorder_bypass_en[11-11] = 1'h0
            rd_port_aging_en[12-12] = 1'h0
            rd_port_urgent_en[13-13] = 1'h0
            rd_port_pagematch_en[14-14] = 1'h0
            rd_port_hpr_en[15-15] = 1'h0
	    rdwr_ordered_en[16-16] = 1 */
         rdata = 0x00010000;
         REG_WRITE(DDR_PCFGR_3, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGW_3 -- 2 bytes @'h0000000000000618
            wr_port_priority[9-0] = 10'h000
            wr_port_aging_en[12-12] = 1'h0
            wr_port_urgent_en[13-13] = 1'h0
            wr_port_pagematch_en[14-14] = 1'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGW_3, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDMASKCH0_3 -- 1 bytes @'h000000000000061c
            id_mask[3-0] = 4'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGIDMASKCH0_3, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDVALUECH0_3 -- 1 bytes @'h0000000000000620
            id_value[3-0] = 4'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGIDVALUECH0_3, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGR_4 -- 2 bytes @'h00000000000006c4
            rd_port_priority[9-0] = 10'h000
            read_reorder_bypass_en[11-11] = 1'h0
            rd_port_aging_en[12-12] = 1'h0
            rd_port_urgent_en[13-13] = 1'h0
            rd_port_pagematch_en[14-14] = 1'h0
            rd_port_hpr_en[15-15] = 1'h0
	    rdwr_ordered_en[16-16] = 1 */
         rdata = 0x00010000;
         REG_WRITE(DDR_PCFGR_4, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGW_4 -- 2 bytes @'h00000000000006c8
            wr_port_priority[9-0] = 10'h000
            wr_port_aging_en[12-12] = 1'h0
            wr_port_urgent_en[13-13] = 1'h0
            wr_port_pagematch_en[14-14] = 1'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGW_4, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDMASKCH0_4 -- 1 bytes @'h00000000000006cc
            id_mask[3-0] = 4'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGIDMASKCH0_4, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDVALUECH0_4 -- 1 bytes @'h00000000000006d0
            id_value[3-0] = 4'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGIDVALUECH0_4, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGR_5 -- 2 bytes @'h0000000000000774
            rd_port_priority[9-0] = 10'h000
            read_reorder_bypass_en[11-11] = 1'h0
            rd_port_aging_en[12-12] = 1'h0
            rd_port_urgent_en[13-13] = 1'h0
            rd_port_pagematch_en[14-14] = 1'h0
            rd_port_hpr_en[15-15] = 1'h0
	    rdwr_ordered_en[16-16] = 1 */
         rdata = 0x00010000;
         REG_WRITE(DDR_PCFGR_5, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGW_5 -- 2 bytes @'h0000000000000778
            wr_port_priority[9-0] = 10'h000
            wr_port_aging_en[12-12] = 1'h0
            wr_port_urgent_en[13-13] = 1'h0
            wr_port_pagematch_en[14-14] = 1'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGW_5, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDMASKCH0_5 -- 1 bytes @'h000000000000077c
            id_mask[3-0] = 4'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGIDMASKCH0_5, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDVALUECH0_5 -- 1 bytes @'h0000000000000780
            id_value[3-0] = 4'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGIDVALUECH0_5, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGR_6 -- 2 bytes @'h0000000000000824
            rd_port_priority[9-0] = 10'h000
            read_reorder_bypass_en[11-11] = 1'h0
            rd_port_aging_en[12-12] = 1'h0
            rd_port_urgent_en[13-13] = 1'h0
            rd_port_pagematch_en[14-14] = 1'h0
            rd_port_hpr_en[15-15] = 1'h0
	    rdwr_ordered_en[16-16] = 1 */
         rdata = 0x00010000;
         REG_WRITE(DDR_PCFGR_6, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGW_6 -- 2 bytes @'h0000000000000828
            wr_port_priority[9-0] = 10'h000
            wr_port_aging_en[12-12] = 1'h0
            wr_port_urgent_en[13-13] = 1'h0
            wr_port_pagematch_en[14-14] = 1'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGW_6, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDMASKCH0_6 -- 1 bytes @'h000000000000082c
            id_mask[3-0] = 4'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGIDMASKCH0_6, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDVALUECH0_6 -- 1 bytes @'h0000000000000830
            id_value[3-0] = 4'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGIDVALUECH0_6, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGR_7 -- 2 bytes @'h00000000000008d4
            rd_port_priority[9-0] = 10'h000
            read_reorder_bypass_en[11-11] = 1'h0
            rd_port_aging_en[12-12] = 1'h0
            rd_port_urgent_en[13-13] = 1'h0
            rd_port_pagematch_en[14-14] = 1'h0
            rd_port_hpr_en[15-15] = 1'h0
	    rdwr_ordered_en[16-16] = 1 */
         rdata = 0x00010000;
         REG_WRITE(DDR_PCFGR_7, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGW_7 -- 2 bytes @'h00000000000008d8
            wr_port_priority[9-0] = 10'h000
            wr_port_aging_en[12-12] = 1'h0
            wr_port_urgent_en[13-13] = 1'h0
            wr_port_pagematch_en[14-14] = 1'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGW_7, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDMASKCH0_7 -- 1 bytes @'h00000000000008dc
            id_mask[3-0] = 4'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGIDMASKCH0_7, rdata);

         /* Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDVALUECH0_7 -- 1 bytes @'h00000000000008e0
            id_value[3-0] = 4'h0 */
         rdata = 0x00000000;
         REG_WRITE(DDR_PCFGIDVALUECH0_7, rdata);

        /* SYNOP step2 - deassert soft reset reg */
	REG_WRITE(SYS_RST_MASK, SYS_RST_DRAM);
	REG_WRITE(SYS_RST_VEC, SYS_RST_DRAM);
        udelay(1);

	/* SYNOP step3 */
	/* Following programming is for DDR Phy Utility Block(PUB). */

	/* Read DRAM Control Register */
	rdata = REG_READ(DDR_PUB_DCR);
	printf("DDR_INIT: PUB_DCR = %x\n", rdata);

	/* Enable it for DDR3 */
	rdata = 0x0000040b;
	REG_WRITE(DDR_PUB_DCR, rdata);
        rdata = 0xf000641d;
	REG_WRITE(DDR_PUB_DSGCR, rdata);
	rdata = REG_READ(DDR_PUB_DSGCR);
	printf("DDR_INIT: PUB_DSGCR = %x\n", rdata);
        rdata = 0;
        REG_WRITE(DDR_PUB_ODTCR, rdata);

	/* Configure Mode Register 0 of PUB */
	rdata = 0x00000420; /* DLL disable, CL=6, tWR=6 */
	REG_WRITE(DDR_PUB_MR0, rdata);
	rdata = 0x000b; /* DLL disable */
	REG_WRITE(DDR_PUB_MR1, rdata);
	rdata = 0x0008; /* DLL disable, CWL=6 */
	REG_WRITE(DDR_PUB_MR2, rdata);
	rdata = 0x0000;
	REG_WRITE(DDR_PUB_MR3, rdata);

	/* Configure Timing parameter registers */
	rdata = 0x79186664; /* DLL disable - tRTP 4, tWTR 6, tRP 6, tRCD 6, tRAS 24, tRRD 4, tRC 30 */
	REG_WRITE(DDR_PUB_DTPR0, rdata);
	rdata = 0x3e80a880; /* DLL disable - tMRD 4, tMOD 12, tFAW 4, tRFC 21, tWLMRD 40, tWLO 15, tAOND 0 */
	REG_WRITE(DDR_PUB_DTPR1, rdata);
	rdata = 0x10018c16; /* DLL disable - tXS 22, tXP 3, tCKE 3, tDLLK 512, tRTODT 0, tRTW 0, tCCD 0 */
	REG_WRITE(DDR_PUB_DTPR2, rdata);

	/* 1.4.2.2 PHY databook for synopsys */
	/* REG_WRITE(DDR_PUB_DTCR,0xa00); */

	/* Configure PHY general configuration registers.
           rdata = 0x00f015f0;
           REG_WRITE(DDR_PUB_PGCR2, rdata); */
	/* Configure PHY Timing registers. */
	rdata = 0x10000010;
	REG_WRITE(DDR_PUB_PTR0, rdata);
	rdata = 0x01000100;
	REG_WRITE(DDR_PUB_PTR1, rdata);

        /* rdata = 0x000083cef;
        REG_WRITE(DDR_PUB_PTR2, rdata); */
        rdata = 0x01e09c40;
        REG_WRITE(DDR_PUB_PTR3, rdata);
        rdata = 0x00d03e80;
        REG_WRITE(DDR_PUB_PTR4, rdata);

        /* rdata = 0x44000f01;
        REG_WRITE(DDR_PUB_DX0GCR, rdata);
        REG_WRITE(DDR_PUB_DX1GCR, rdata);
        REG_WRITE(DDR_PUB_DX2GCR, rdata);
        REG_WRITE(DDR_PUB_DX3GCR, rdata); */

        rdata = REG_READ(DDR_PUB_DX0GCR);
	printf("DDR_INIT: DDR_PUB_DX0GCR = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX1GCR);
	printf("DDR_INIT: DDR_PUB_DX1GCR = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX2GCR);
	printf("DDR_INIT: DDR_PUB_DX2GCR = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX3GCR);
	printf("DDR_INIT: DDR_PUB_DX3GCR = %x\n", rdata);

	/* SYNOP step5 */
	/* Now wait till PHY initialization done. */
	rdata = 0x0;
	while ((rdata&1) != 1) {
		rdata = REG_READ(DDR_PUB_PGSR0);
	        printf("DDR_INIT: DDR_PUB_PGSR0 = %x\n", rdata);
	}
	printf("DDR_INIT: finish step5\n");

	/* Start Phy Initialization Register through controller */
        /*
	rdata = REG_READ(DDR_PUB_PLLCR);
	printf("DDR_INIT: DDR_PUB_PLLCR = %x\n", rdata);
        rdata = 0x80000000 | rdata;
	REG_WRITE(DDR_PUB_PLLCR, rdata);
        */
	rdata = 0x00040001;
	REG_WRITE(DDR_PUB_PIR, rdata);
        udelay(1);

	/* Now wait till PHY initialization done after local line register changes. */
	rdata = 0x0;
	while ((rdata&0x1f) != 0x1f) {
	    rdata = REG_READ(DDR_PUB_PGSR0);
	    printf("DDR_INIT: DDR_PUB_PGSR0 = %x\n", rdata);
	}
	printf("DDR_INIT: finish step5\n");

        /* SYNOP step6 - set DFIMISC.dfi_init_complete_en to 1 */
        rdata = 0x00000001;
        REG_WRITE(DDR_DFIMISC, rdata);

	printf("DDR_INIT: finish step6\n");

	/* SYNOP step7 - wait for DWC_ddr_umctl2 to move to "normal" by monitoring STAT.operating_mode signal */
        rdata = 0;
        while ((rdata&0x3) != 0x1) {
            rdata = REG_READ(DDR_STAT);
	    printf("DDR_INIT: DDR_STAT = %x\n", rdata);
        }
        printf("DDR_INIT: finish step7\n");

        /* SYNOP step6 - set DFIMISC.dfi_init_complete_en to 0 */
        rdata = 0;
        REG_WRITE(DDR_DFIMISC, rdata);

        /* disable auto-refresh and powerdown
           Register DWC_ddr_umctl2_map_UMCTL2_REGS.RFSHCTL3 -- 1 bytes @'h0000000000000060
           dis_auto_refresh[0-0] = 1'h1
           refresh_update_level[1-1] = 1'h0 */
        rdata = 0x00000001;
        REG_WRITE(DDR_RFSHCTL3, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.PWRCTL -- 1 bytes @'h0000000000000030
           selfref_en[0-0] = 1'h0
           powerdown_en[1-1] = 1'h0
           en_dfi_dram_clk_disable[3-3] = 1'h0 */
        rdata = 0x00000000;
        REG_WRITE(DDR_PWRCTL, rdata);
        printf("DDR_INIT: finish step8\n");

	/* SYNOP step9 */
	/* Use PUB to do training */
	REG_WRITE(DDR_PUB_MR1, 0x0000000b); /* DLL disable */
        REG_WRITE(DDR_PUB_DTCR, 0x01003583);

        /*  rdata = REG_READ(DDR_PUB_DTAR0);
            printf("DDR_INIT: DDR_PUB_DTAR0 = %x\n", rdata);
            write leveling routine */
	rdata = 0x00000201;
	REG_WRITE(DDR_PUB_PIR, rdata);
        udelay(1);
	rdata = 0x0;
	while ((rdata&0x21) != 0x21) {
	    rdata = REG_READ(DDR_PUB_PGSR0);
	    printf("DDR_INIT: DDR_PUB_PGSR0 = %x\n", rdata);
	}

        rdata = REG_READ(DDR_PUB_DX0GSR0);
	printf("DDR_INIT: DDR_PUB_DX0GSR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX1GSR0);
	printf("DDR_INIT: DDR_PUB_DX1GSR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX2GSR0);
	printf("DDR_INIT: DDR_PUB_DX2GSR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX3GSR0);
	printf("DDR_INIT: DDR_PUB_DX3GSR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_PGSR0);
	printf("DDR_INIT: DDR_PUB_PGSR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX0LCDLR0);
	printf("DDR_INIT: DDR_PUB_DX0LCDLR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX1LCDLR0);
	printf("DDR_INIT: DDR_PUB_DX1LCDLR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX2LCDLR0);
	printf("DDR_INIT: DDR_PUB_DX2LCDLR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX3LCDLR0);
	printf("DDR_INIT: DDR_PUB_DX3LCDLR0 = %x\n", rdata);
	printf("DDR_INIT: finish write leveling\n");

        /* dqs training */
	rdata = 0x00000401;
	REG_WRITE(DDR_PUB_PIR, rdata);
        udelay(1);
	rdata = 0x0;
	while ((rdata&0x41) != 0x41) {
	    rdata = REG_READ(DDR_PUB_PGSR0);
	    printf("DDR_INIT: DDR_PUB_PGSR0 = %x\n", rdata);
	}

        rdata = REG_READ(DDR_PUB_DX0BDLR0);
	printf("DDR_INIT: DDR_PUB_DX0BDLR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX0BDLR1);
	printf("DDR_INIT: DDR_PUB_DX0BDLR1 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX0BDLR2);
	printf("DDR_INIT: DDR_PUB_DX0BDLR2 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX1BDLR0);
	printf("DDR_INIT: DDR_PUB_DX1BDLR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX1BDLR1);
	printf("DDR_INIT: DDR_PUB_DX1BDLR1 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX1BDLR2);
	printf("DDR_INIT: DDR_PUB_DX1BDLR2 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX2BDLR0);
	printf("DDR_INIT: DDR_PUB_DX2BDLR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX2BDLR1);
	printf("DDR_INIT: DDR_PUB_DX2BDLR1 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX2BDLR2);
	printf("DDR_INIT: DDR_PUB_DX2BDLR2 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX3BDLR0);
	printf("DDR_INIT: DDR_PUB_DX3BDLR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX3BDLR1);
	printf("DDR_INIT: DDR_PUB_DX3BDLR1 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX3BDLR2);
	printf("DDR_INIT: DDR_PUB_DX3BDLR2 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX0GSR0);
	printf("DDR_INIT: DDR_PUB_DX0GSR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX1GSR0);
	printf("DDR_INIT: DDR_PUB_DX1GSR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX2GSR0);
	printf("DDR_INIT: DDR_PUB_DX2GSR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX3GSR0);
	printf("DDR_INIT: DDR_PUB_DX3GSR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_PGSR0);
	printf("DDR_INIT: DDR_PUB_PGSR0 = %x\n", rdata);
	printf("DDR_INIT: finish dqs training\n");

        /*
	REG_WRITE(DDR_PUB_DX0LCDLR0 ,0x3);
	REG_WRITE(DDR_PUB_DX1LCDLR0 ,0x2);
	REG_WRITE(DDR_PUB_DX2LCDLR0 ,0x4);
	REG_WRITE(DDR_PUB_DX3LCDLR0 ,0x4);
	REG_WRITE(DDR_PUB_DX0BDLR0  ,0xf);
	REG_WRITE(DDR_PUB_DX0BDLR1  ,0x36);
	REG_WRITE(DDR_PUB_DX0BDLR2  ,0x5);
	REG_WRITE(DDR_PUB_DX1BDLR0  ,0xe);
	REG_WRITE(DDR_PUB_DX1BDLR1  ,0x36);
	REG_WRITE(DDR_PUB_DX1BDLR2  ,0x5);
	REG_WRITE(DDR_PUB_DX2BDLR0  ,0xe);
	REG_WRITE(DDR_PUB_DX2BDLR1  ,0x36);
	REG_WRITE(DDR_PUB_DX2BDLR2  ,0x5);
	REG_WRITE(DDR_PUB_DX3BDLR0  ,0xe);
	REG_WRITE(DDR_PUB_DX3BDLR1  ,0x36);
	REG_WRITE(DDR_PUB_DX3BDLR2  ,0x5);
        */

        /* enable auto-refresh and powerdown
           Register DWC_ddr_umctl2_map_UMCTL2_REGS.RFSHCTL3 -- 1 bytes @'h0000000000000060
           dis_auto_refresh[0-0] = 1'h0
           refresh_update_level[1-1] = 1'h1 */
        rdata = 0x00000002;
        REG_WRITE(DDR_RFSHCTL3, rdata);

        /* Register DWC_ddr_umctl2_map_UMCTL2_REGS.PWRCTL -- 1 bytes @'h0000000000000030
           selfref_en[0-0] = 1'h0
           powerdown_en[1-1] = 1'h0
           en_dfi_dram_clk_disable[3-3] = 1'h0 */
        rdata = 0x00000000;
        REG_WRITE(DDR_PWRCTL, rdata);
        rdata = 1;
        REG_WRITE(DDR_DFIMISC, rdata);
        printf("DDR_INIT: ddr_init finishes\n");

	return 0;

}
#else /* ENDof TOPAZ_FPGA_PLATFORM */
/* Start of TOPAZ_ASIC_PLATFORM */

#define SYS_CPU_CTRL_MASK	RUBY_SYS_CTL_MASK
#define SYS_CPU_CTRL		RUBY_SYS_CTL_CTRL
#define SYS_DDR_CTRL		RUBY_SYS_CTL_DDR_CTRL

/* BEGIN COPY-PASTE FROM ARCSHELL */

#define lhost_ahb_read_data(x,y) (y=REG_READ(x))
#define lhost_ahb_write(x,y) (REG_WRITE(x,y))
/******************************************
 * type = 0 for 16-bit, 1 for 32-bit
 * speed = 1=640, 2=500, 3=400MHz DRAM clock
 */

int ddr3_init(u32 type, u32 speed, u32 size)
{
	u32 rdata;

	printf("DDR_INIT: type = %u, speed = %u, size = %u\n", type, speed, size);

    // speed related DDR controller programming
	REG_WRITE(SYS_CPU_CTRL_MASK,7<<22);
	REG_WRITE(SYS_CPU_CTRL,speed<<22);
	REG_WRITE(SYS_DDR_CTRL,0x04243333);

    switch (speed) {
    case 0: //800MHz
	printf("800MHz\n");
//	rdata = 0x0061002C; //1Gb
	rdata = 0x00610040; //2Gb
	lhost_ahb_write(DDR_RFSHTMG, rdata);
	rdata = 0x00D000C4;
	lhost_ahb_write(DDR_INIT0, rdata);
	rdata = 0x004F0000;
	lhost_ahb_write(DDR_INIT1, rdata);
	rdata = 0x1D70000E;
	lhost_ahb_write(DDR_INIT3, rdata);
	rdata = 0x00180000;
	lhost_ahb_write(DDR_INIT4, rdata);
	rdata = 0x11101B0E;
	lhost_ahb_write(DDR_DRAMTMG0, rdata);
	rdata = 0x000A0814;
	lhost_ahb_write(DDR_DRAMTMG1, rdata);
	rdata = 0x0000050E;
	lhost_ahb_write(DDR_DRAMTMG2, rdata);
	rdata = 0x00002006;
	lhost_ahb_write(DDR_DRAMTMG3, rdata);
//	rdata = 0x01030306;
	rdata = 0x01020306; //tCCD=4
	lhost_ahb_write(DDR_DRAMTMG4, rdata);
	rdata = 0x04040302;
	lhost_ahb_write(DDR_DRAMTMG5, rdata);
	rdata = 0x00000010;
	lhost_ahb_write(DDR_DRAMTMG8, rdata);
	rdata = 0x03090107;
	lhost_ahb_write(DDR_DFITMG0, rdata);
        break;
    case 1: //640MHz
	printf("640MHz\n");
//	rdata = 0x004E0024; //1Gb
	rdata = 0x004E0034; //2Gb
	lhost_ahb_write(DDR_RFSHTMG, rdata);
	rdata = 0x0100009D;
	lhost_ahb_write(DDR_INIT0, rdata);
	rdata = 0x003F0000;
	lhost_ahb_write(DDR_INIT1, rdata);
	rdata = 0x1B60000E;
	lhost_ahb_write(DDR_INIT3, rdata);
	rdata = 0x00100000;
	lhost_ahb_write(DDR_INIT4, rdata);
	rdata = 0x0F0D150C;
	lhost_ahb_write(DDR_DRAMTMG0, rdata);
	rdata = 0x00080710;
	lhost_ahb_write(DDR_DRAMTMG1, rdata);
	rdata = 0x0000050D;
	lhost_ahb_write(DDR_DRAMTMG2, rdata);
	rdata = 0x00002006;
	lhost_ahb_write(DDR_DRAMTMG3, rdata);
//	rdata = 0x01030305;
	rdata = 0x01020305; //tCCD=4
	lhost_ahb_write(DDR_DRAMTMG4, rdata);
	rdata = 0x04040302;
	lhost_ahb_write(DDR_DRAMTMG5, rdata);
	rdata = 0x00000010;
	lhost_ahb_write(DDR_DRAMTMG8, rdata);
	rdata = 0x03080106;
	lhost_ahb_write(DDR_DFITMG0, rdata);
        break;
    case 3: //400MHz
	printf("400MHz\n");
//	rdata = 0x00300016; //1Gb
	rdata = 0x00300020; //2Gb
	lhost_ahb_write(DDR_RFSHTMG, rdata);
	rdata = 0x00670062;
	lhost_ahb_write(DDR_INIT0, rdata);
	rdata = 0x00280000;
	lhost_ahb_write(DDR_INIT1, rdata);
	rdata = 0x1520000E;
	lhost_ahb_write(DDR_INIT3, rdata);
	rdata = 0x00000000;
	lhost_ahb_write(DDR_INIT4, rdata);
	rdata = 0x0A080D07;
	lhost_ahb_write(DDR_DRAMTMG0, rdata);
	rdata = 0x0005040A;
	lhost_ahb_write(DDR_DRAMTMG1, rdata);
	rdata = 0x00000409;
	lhost_ahb_write(DDR_DRAMTMG2, rdata);
	rdata = 0x00002006;
	lhost_ahb_write(DDR_DRAMTMG3, rdata);
//	rdata = 0x01030204;
	rdata = 0x01020204; //tCCD=4
	lhost_ahb_write(DDR_DRAMTMG4, rdata);
	rdata = 0x03030202;
	lhost_ahb_write(DDR_DRAMTMG5, rdata);
	rdata = 0x00000010;
	lhost_ahb_write(DDR_DRAMTMG8, rdata);
	rdata = 0x03040103;
	lhost_ahb_write(DDR_DFITMG0, rdata);
        break;
    case 4: //320MHz
	printf("320MHz\n");
//	rdata = 0x00270012; //1Gb
	rdata = 0x0027001A; //2Gb
	lhost_ahb_write(DDR_RFSHTMG, rdata);
	rdata = 0x0052004F;
	lhost_ahb_write(DDR_INIT0, rdata);
	rdata = 0x00200000;
	lhost_ahb_write(DDR_INIT1, rdata);
	rdata = 0x1320000E;
	lhost_ahb_write(DDR_INIT3, rdata);
	rdata = 0x00000000;
	lhost_ahb_write(DDR_INIT4, rdata);
	rdata = 0x09070A06;
	lhost_ahb_write(DDR_DRAMTMG0, rdata);
	rdata = 0x00050408;
	lhost_ahb_write(DDR_DRAMTMG1, rdata);
	rdata = 0x00000409;
	lhost_ahb_write(DDR_DRAMTMG2, rdata);
	rdata = 0x00002006;
	lhost_ahb_write(DDR_DRAMTMG3, rdata);
//	rdata = 0x01030203;
	rdata = 0x01020203; //tCCD=4
	lhost_ahb_write(DDR_DRAMTMG4, rdata);
	rdata = 0x03030202;
	lhost_ahb_write(DDR_DRAMTMG5, rdata);
	rdata = 0x00000010;
	lhost_ahb_write(DDR_DRAMTMG8, rdata);
	rdata = 0x03040103;
	lhost_ahb_write(DDR_DFITMG0, rdata);
        break;
    default:
	printf("500MHz\n");
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.RFSHTMG -- 4 bytes @'h0000000000000064
//   t_rfc_min[8-0] = 9'h028
//   t_rfc_nom_x32[27-16] = 12'h03c
	if (size == DDR_256MB) {
		rdata = 0x001e0028;
	} else {
		rdata = 0x001e001c;
	}
//	rdata = 0x003c001c; //new
	lhost_ahb_write(DDR_RFSHTMG, rdata);
	//SYNOP step 1 - program umctl2 registers
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.INIT0 -- 4 bytes @'h00000000000000d0
//   pre_cke_x1024[9-0] = 10'h001
//   post_cke_x1024[25-16] = 10'h001
//	rdata = 0x00280062;
	rdata = 0x0080007B; //new
	lhost_ahb_write(DDR_INIT0, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.INIT1 -- 3 bytes @'h00000000000000d4
//   pre_ocd_x32[3-0] = 4'h0
//   final_wait_x32[14-8] = 7'h00
//   dram_rstn_x1024[23-16] = 8'h01
	rdata = 0x00310000;
	lhost_ahb_write(DDR_INIT1, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.INIT3 -- 4 bytes @'h00000000000000dc
//   emr[15-0] = 16'h000e
//   mr[31-16] = 16'h1930
	rdata = 0x1940000E; //AL=CL-1
//	rdata = 0x19400006; //AL=0
	lhost_ahb_write(DDR_INIT3, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.INIT4 -- 4 bytes @'h00000000000000e0
//   emr3[15-0] = 16'h0000
//   emr2[31-16] = 16'h0008
	rdata = 0x00080000;
	lhost_ahb_write(DDR_INIT4, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.DRAMTMG0 -- 4 bytes @'h0000000000000100
//   t_ras_min[5-0] = 6'h0a
//   t_ras_max[13-8] = 6'h11
//   t_faw[21-16] = 6'h0a
//   wr2pre[29-24] = 6'h0c
	rdata = 0x0d0a1109; //AL=CL-1
//	rdata = 0x090a1109; //AL=0
	lhost_ahb_write(DDR_DRAMTMG0, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.DRAMTMG1 -- 3 bytes @'h0000000000000104
//   t_rc[5-0] = 6'h1b
//   rd2pre[12-8] = 5'h0b
//   t_xp[20-16] = 5'h04
//	rdata = 0x00040b1b;
	rdata = 0x0006050d; //AL=CL-1
//	rdata = 0x0006020d; //AL=0
	lhost_ahb_write(DDR_DRAMTMG1, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.DRAMTMG2 -- 2 bytes @'h0000000000000108
//   wr2rd[5-0] = 6'h0a
//   rd2wr[12-8] = 5'h04
	rdata = 0x0000040b; //AL=CL-1
//	rdata = 0x00000408; //AL=0
	lhost_ahb_write(DDR_DRAMTMG2, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.DRAMTMG3 -- 2 bytes @'h000000000000010c
//   t_mod[9-0] = 10'h006
//   t_mrd[14-12] = 3'h2
	rdata = 0x00002006;
	lhost_ahb_write(DDR_DRAMTMG3, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.DRAMTMG4 -- 4 bytes @'h0000000000000110
//   t_rp[3-0] = 4'h4
//   t_rrd[10-8] = 3'h2
//   t_ccd[18-16] = 3'h2
//   t_rcd[27-24] = 4'h1
//	rdata = 0x01030204; //AL=CL-1
	rdata = 0x01020204; //tCCD=4
//	rdata = 0x04010204; //AL=0
	lhost_ahb_write(DDR_DRAMTMG4, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.DRAMTMG5 -- 4 bytes @'h0000000000000114
//   t_cke[3-0] = 4'h2
//   t_ckesr[13-8] = 6'h02
//   t_cksre[19-16] = 4'h3
//   t_cksrx[27-24] = 4'h3
	rdata = 0x03030202;
	lhost_ahb_write(DDR_DRAMTMG5, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.DRAMTMG8 -- 1 bytes @'h0000000000000120
//   post_selfref_gap_x32[6-0] = 7'h62
	rdata = 0x00000062;
	lhost_ahb_write(DDR_DRAMTMG8, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.DFITMG0 -- 4 bytes @'h0000000000000190
//   write_latency[4-0] = 5'h04
//   dfi_tphy_wrdata[12-8] = 5'h01
//   dfi_t_rddata_en[20-16] = 5'h05
//   dfi_t_ctrl_delay[27-24] = 4'h3
	rdata = 0x03060105; //AL=CL-1
//	rdata = 0x03020102; //AL=0
	lhost_ahb_write(DDR_DFITMG0, rdata);
        break;
    }

    // width related programming
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.MSTR -- 3 bytes @'h0000000000000000
//   ddr3[0-0] = 1'h1
//   burst_mode[8-8] = 1'h0
//   burstchop[9-9] = 1'h0
//   en_2t_timing_mode[10-10] = 1'h0
//   data_bus_width[13-12] = 2'h1
//   burst_rdwr[19-16] = 4'h4
	rdata = (type==32)?0x00040001:0x00041001;
	lhost_ahb_write(DDR_MSTR, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.ADDRMAP1 -- 3 bytes @'h0000000000000204
//   addrmap_bank_b0[3-0] = 4'h6
//   addrmap_bank_b1[11-8] = 4'h6
//   addrmap_bank_b2[19-16] = 4'h6
	rdata = (type==32)?0x00070707:0x00060606;
	//rdata = 0x0;
	//rdata[19:16] = ddr_conf.addrmap_bank_b2;
	//rdata[11:8]  = ddr_conf.addrmap_bank_b1;
	//rdata[3:0]   = ddr_conf.addrmap_bank_b0;
	lhost_ahb_write(DDR_ADDRMAP1, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.ADDRMAP2 -- 4 bytes @'h0000000000000208
//   addrmap_col_b2[3-0] = 4'h0
//   addrmap_col_b3[11-8] = 4'h0
//   addrmap_col_b4[19-16] = 4'h0
//   addrmap_col_b5[27-24] = 4'h0
	rdata = 0x00000000;
	//rdata[27:24] = ddr_conf.addrmap_col_b5;
	//rdata[19:16] = ddr_conf.addrmap_col_b4;
	//rdata[11:8]  = ddr_conf.addrmap_col_b3;
	//rdata[3:0]   = ddr_conf.addrmap_col_b2;
	lhost_ahb_write(DDR_ADDRMAP2, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.ADDRMAP3 -- 4 bytes @'h000000000000020c
//   addrmap_col_b6[3-0] = 4'h0
//   addrmap_col_b7[11-8] = 4'h0
//   addrmap_col_b8[19-16] = 4'hf
//   addrmap_col_b9[27-24] = 4'hf
	rdata = (type==32)?0x0f000000:0x0f0f0000;
	//rdata = 0x0;
//              rdata[27:24] = ddr_conf.addrmap_col_b9;
//              rdata[19:16] = ddr_conf.addrmap_col_b8;
//              rdata[11:8]  = ddr_conf.addrmap_col_b7;
//              rdata[3:0]   = ddr_conf.addrmap_col_b6;
	lhost_ahb_write(DDR_ADDRMAP3, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.ADDRMAP4 -- 2 bytes @'h0000000000000210
//   addrmap_col_b10[3-0] = 4'hf
//   addrmap_col_b11[11-8] = 4'hf
	rdata = 0x00000f0f;
//    rdata = 0x0;
//              rdata[11:8]  = ddr_conf.addrmap_col_b11;
//              rdata[3:0]   = ddr_conf.addrmap_col_b10;
	lhost_ahb_write(DDR_ADDRMAP4, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.ADDRMAP5 -- 4 bytes @'h0000000000000214
//   addrmap_row_b0[3-0] = 4'h5
//   addrmap_row_b1[11-8] = 4'h5
//   addrmap_row_b2_10[19-16] = 4'h5
//   addrmap_row_b11[27-24] = 4'hf
	rdata = (type==32)?0x06060606:0x05050505;
//    rdata = 0x0;
//              rdata[27:24] = ddr_conf.addrmap_row_b11;
//              rdata[19:16] = ddr_conf.addrmap_row_b2_10;
//              rdata[11:8]  = ddr_conf.addrmap_row_b1;
//              rdata[3:0]   = ddr_conf.addrmap_row_b0;
	lhost_ahb_write(DDR_ADDRMAP5, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.ADDRMAP6 -- 4 bytes @'h0000000000000218
//   addrmap_row_b12[3-0] = 4'hf
//   addrmap_row_b13[11-8] = 4'hf
//   addrmap_row_b14[19-16] = 4'hf
//   addrmap_row_b15[27-24] = 4'hf

	if (size == DDR_256MB) {
		rdata = (type==32) ? 0x0f060606 : 0x0f0f0505; //2Gb                ;
	} else {
		rdata = (type==32) ? 0x0f0f0606 : 0x0f0f0f05; //1Gb
	}
//	rdata = (type==32)?0x0f0f0606:0x0f0f0505; //2Gb
//    rdata = 0x0;
//              rdata[27:24] = ddr_conf.addrmap_row_b15;
//              rdata[19:16] = ddr_conf.addrmap_row_b14;
//              rdata[11:8]  = ddr_conf.addrmap_row_b13;
//              rdata[3:0]   = ddr_conf.addrmap_row_b12;
	lhost_ahb_write(DDR_ADDRMAP6, rdata);

        // non-speed-width related programming
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.STAT -- 1 bytes @'h0000000000000004
//   operating_mode[1-0] = 2'h0
	lhost_ahb_read_data(DDR_STAT, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.MRCTRL0 -- 4 bytes @'h0000000000000010
//   mr_rank[4-4] = 1'h1
//   mr_addr[14-12] = 3'h0
//   mr_wr[31-31] = 1'h0
//	rdata = 0x00000010;
//	lhost_ahb_write(DDR_MRCTRL0, rdata);
////Register DWC_ddr_umctl2_map_UMCTL2_REGS.MRCTRL1 -- 2 bytes @'h0000000000000014
////   mr_data[15-0] = 16'hcdb8
//	rdata = 0x0000cdb8;
//	lhost_ahb_write(DDR_MRCTRL1, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.MRSTAT -- 1 bytes @'h0000000000000018
//   mr_wr_busy[0-0] = 1'h0
	lhost_ahb_read_data(DDR_MRSTAT, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.PWRCTL -- 1 bytes @'h0000000000000030
//   selfref_en[0-0] = 1'h0
//   powerdown_en[1-1] = 1'h0
//   en_dfi_dram_clk_disable[3-3] = 1'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_PWRCTL, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.PWRTMG -- 1 bytes @'h0000000000000034
//   powerdown_to_x32[4-0] = 5'h03
	rdata = 0x00000020;
	lhost_ahb_write(DDR_PWRTMG, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.RFSHCTL0 -- 3 bytes @'h0000000000000050
//   refresh_burst[10-8] = 3'h0
//   refresh_to_x32[16-12] = 5'h1f
//   refresh_margin[23-20] = 4'h2
	rdata = 0x0021f200;
	lhost_ahb_write(DDR_RFSHCTL0, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.RFSHCTL3 -- 1 bytes @'h0000000000000060
//   dis_auto_refresh[0-0] = 1'h0
//   refresh_update_level[1-1] = 1'h0
	rdata = 0x00000001;
	lhost_ahb_write(DDR_RFSHCTL3, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.PARCTL -- 1 bytes @'h00000000000000c0
//   dfi_parity_err_int_en[0-0] = 1'h0
//   dfi_parity_err_int_clr[1-1] = 1'h0
//   dfi_parity_err_cnt_clr[2-2] = 1'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_PARCTL, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.PARSTAT -- 3 bytes @'h00000000000000c4
//   dfi_parity_err_cnt[15-0] = 16'h0000
//   dfi_parity_err_int[16-16] = 1'h0
	lhost_ahb_read_data(DDR_PARSTAT, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.INIT5 -- 3 bytes @'h00000000000000e4
//   dev_zqinit_x32[23-16] = 8'h10
	rdata = 0x00100000;
	lhost_ahb_write(DDR_INIT5, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.DIMMCTL -- 1 bytes @'h00000000000000f0
//   dimm_stagger_cs_en[0-0] = 1'h0
//   dimm_addr_mirr_en[1-1] = 1'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_DIMMCTL, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.ZQCTL0 -- 4 bytes @'h0000000000000180
//   t_zq_short_nop[9-0] = 10'h020
//   t_zq_long_nop[25-16] = 10'h080
//   dis_srx_zqcl[30-30] = 1'h0
//   dis_auto_zq[31-31] = 1'h0
	rdata = 0x00800020;
	lhost_ahb_write(DDR_ZQCTL0, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.ZQCTL1 -- 3 bytes @'h0000000000000184
//   t_zq_short_interval_x1024[19-0] = 20'h00070
	rdata = 0x00000070;
	lhost_ahb_write(DDR_ZQCTL1, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.DFITMG1 -- 2 bytes @'h0000000000000194
//   dfi_t_dram_clk_enable[3-0] = 4'h2
//   dfi_t_dram_clk_disable[11-8] = 4'h2
	rdata = 0x00000202;
	lhost_ahb_write(DDR_DFITMG1, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.DFIUPD0 -- 4 bytes @'h00000000000001a0
//   dfi_t_ctrlup_min[9-0] = 10'h003
//   dfi_t_ctrlup_max[25-16] = 10'h040
//   dis_dll_calib[31-31] = 1'h0
	rdata = 0x00400003;
	lhost_ahb_write(DDR_DFIUPD0, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.DFIUPD1 -- 3 bytes @'h00000000000001a4
//   dfi_t_ctrlupd_interval_max_x1024[7-0] = 8'h26
//   dfi_t_ctrlupd_interval_min_x1024[23-16] = 8'h22
	rdata = 0x00220026;
	lhost_ahb_write(DDR_DFIUPD1, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.DFIUPD2 -- 4 bytes @'h00000000000001a8
//   dfi_phyupd_type0[11-0] = 12'h94f
//   dfi_phyupd_type1[27-16] = 12'hfff
//   dfi_phyupd_en[31-31] = 1'h0
	rdata = 0x0fff094f;
	lhost_ahb_write(DDR_DFIUPD2, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.DFIUPD3 -- 4 bytes @'h00000000000001ac
//   dfi_phyupd_type2[11-0] = 12'h4d6
//   dfi_phyupd_type3[27-16] = 12'h954
	rdata = 0x095404d6;
	lhost_ahb_write(DDR_DFIUPD3, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.DFIMISC -- 1 bytes @'h00000000000001b0
//   dfi_init_complete_en[0-0] = 1'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_DFIMISC, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.ODTCFG -- 4 bytes @'h0000000000000240
//   wr_odt_delay[19-16] = 4'h0
//   wr_odt_hold[27-24] = 4'h2
	rdata = (speed>=3)?0x02000200:0x02000204;
	lhost_ahb_write(DDR_ODTCFG, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.ODTMAP -- 1 bytes @'h0000000000000244
//   rank0_wr_odt[0-0] = 1'h1
	rdata = 0x00000001;
	lhost_ahb_write(DDR_ODTMAP, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.SCHED -- 4 bytes @'h0000000000000250
//   force_low_pri_n[0-0] = 1'h1
//   prefer_write[1-1] = 1'h0
//   pageclose[2-2] = 1'h1
//   lpr_num_entries[11-8] = 4'hc
//   go2critical_hysteresis[23-16] = 8'h1d
//   rdwr_idle_gap[30-24] = 7'h10
        rdata = 0x081d0c01;
	lhost_ahb_write(DDR_SCHED, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.PERFHPR0 -- 2 bytes @'h0000000000000258
//   hpr_min_non_critical[15-0] = 16'hda6f
	rdata = 0x0000006f;
	lhost_ahb_write(DDR_PERFHPR0, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.PERFHPR1 -- 4 bytes @'h000000000000025c
//   hpr_max_starve[15-0] = 16'h1a0c
//   hpr_xact_run_length[31-24] = 8'hd5
	rdata = 0x05000001;
	lhost_ahb_write(DDR_PERFHPR1, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.PERFLPR0 -- 2 bytes @'h0000000000000260
//   lpr_min_non_critical[15-0] = 16'hd248
	rdata = 0x00000048;
	lhost_ahb_write(DDR_PERFLPR0, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.PERFLPR1 -- 4 bytes @'h0000000000000264
//   lpr_max_starve[15-0] = 16'h49fa
//   lpr_xact_run_length[31-24] = 8'h25
	rdata = 0x050000fa;
	lhost_ahb_write(DDR_PERFLPR1, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.PERFWR0 -- 2 bytes @'h0000000000000268
//   w_min_non_critical[15-0] = 16'h7390
	rdata = 0x00000090;
	lhost_ahb_write(DDR_PERFWR0, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.PERFWR1 -- 4 bytes @'h000000000000026c
//   w_max_starve[15-0] = 16'hcafe
//   w_xact_run_length[31-24] = 8'h97
	rdata = 0x040000fe;
	lhost_ahb_write(DDR_PERFWR1, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.DBG0 -- 1 bytes @'h0000000000000300
//   dis_wc[0-0] = 1'h0
//   dis_rd_bypass[1-1] = 1'h0
//   dis_act_bypass[2-2] = 1'h0
//   dis_collision_page_opt[4-4] = 1'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_DBG0, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.DBG1 -- 1 bytes @'h0000000000000304
//   dis_dq[0-0] = 1'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_DBG1, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.DBGCAM -- 4 bytes @'h0000000000000308
//   dbg_hpr_q_depth[4-0] = 5'h00
//   dbg_lpr_q_depth[12-8] = 5'h00
//   dbg_w_q_depth[20-16] = 5'h00
//   dbg_stall[24-24] = 1'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_DBGCAM, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCCFG -- 1 bytes @'h0000000000000400
//   go2critical_en[0-0] = 1'h0
//   pagematch_limit[4-4] = 1'h1
	rdata = 0x00000010;
	lhost_ahb_write(DDR_PCCFG, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGR_0 -- 2 bytes @'h0000000000000404
//   rd_port_priority[9-0] = 10'h000
//   read_reorder_bypass_en[11-11] = 1'h1
//   rd_port_aging_en[12-12] = 1'h0
//   rd_port_urgent_en[13-13] = 1'h0
//   rd_port_pagematch_en[14-14] = 1'h0
//   rd_port_hpr_en[15-15] = 1'h0
//   rdwr_ordered_en[16-16] = 1'h1
	rdata = 0x00014000;
	lhost_ahb_write(DDR_PCFGR_0, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGW_0 -- 2 bytes @'h0000000000000408
//   wr_port_priority[9-0] = 10'h000
//   wr_port_aging_en[12-12] = 1'h0
//   wr_port_urgent_en[13-13] = 1'h0
//   wr_port_pagematch_en[14-14] = 1'h0

	rdata = 0x00004000;
	lhost_ahb_write(DDR_PCFGW_0, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDMASKCH0_0 -- 1 bytes @'h000000000000040c
//   id_mask[3-0] = 4'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_PCFGIDMASKCH0_0, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDVALUECH0_0 -- 1 bytes @'h0000000000000410
//   id_value[3-0] = 4'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_PCFGIDVALUECH0_0, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGR_1 -- 2 bytes @'h00000000000004b4
//   rd_port_priority[9-0] = 10'h000
//   read_reorder_bypass_en[11-11] = 1'h0
//   rd_port_aging_en[12-12] = 1'h0
//   rd_port_urgent_en[13-13] = 1'h0
//   rd_port_pagematch_en[14-14] = 1'h0
//   rd_port_hpr_en[15-15] = 1'h0
	rdata = 0x0001c000;
	lhost_ahb_write(DDR_PCFGR_1, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGW_1 -- 2 bytes @'h00000000000004b8
//   wr_port_priority[9-0] = 10'h000
//   wr_port_aging_en[12-12] = 1'h0
//   wr_port_urgent_en[13-13] = 1'h0
//   wr_port_pagematch_en[14-14] = 1'h0
	rdata = 0x00004000;
	lhost_ahb_write(DDR_PCFGW_1, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDMASKCH0_1 -- 1 bytes @'h00000000000004bc
//   id_mask[3-0] = 4'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_PCFGIDMASKCH0_1, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDVALUECH0_1 -- 1 bytes @'h00000000000004c0
//   id_value[3-0] = 4'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_PCFGIDVALUECH0_1, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGR_2 -- 2 bytes @'h0000000000000564
//   rd_port_priority[9-0] = 10'h000
//   read_reorder_bypass_en[11-11] = 1'h0
//   rd_port_aging_en[12-12] = 1'h0
//   rd_port_urgent_en[13-13] = 1'h0
//   rd_port_pagematch_en[14-14] = 1'h0
//   rd_port_hpr_en[15-15] = 1'h0
	rdata = 0x0001c000;
	lhost_ahb_write(DDR_PCFGR_2, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGW_2 -- 2 bytes @'h0000000000000568
//   wr_port_priority[9-0] = 10'h000
//   wr_port_aging_en[12-12] = 1'h0
//   wr_port_urgent_en[13-13] = 1'h0
//   wr_port_pagematch_en[14-14] = 1'h0
	rdata = 0x00004000;
	lhost_ahb_write(DDR_PCFGW_2, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDMASKCH0_2 -- 1 bytes @'h000000000000056c
//   id_mask[3-0] = 4'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_PCFGIDMASKCH0_2, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDVALUECH0_2 -- 1 bytes @'h0000000000000570
//   id_value[3-0] = 4'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_PCFGIDVALUECH0_2, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGR_3 -- 2 bytes @'h0000000000000614
//   rd_port_priority[9-0] = 10'h000
//   read_reorder_bypass_en[11-11] = 1'h0
//   rd_port_aging_en[12-12] = 1'h0
//   rd_port_urgent_en[13-13] = 1'h0
//   rd_port_pagematch_en[14-14] = 1'h1
//   rd_port_hpr_en[15-15] = 1'h1
	rdata = 0x0001c000;
	lhost_ahb_write(DDR_PCFGR_3, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGW_3 -- 2 bytes @'h0000000000000618
//   wr_port_priority[9-0] = 10'h000
//   wr_port_aging_en[12-12] = 1'h1
//   wr_port_urgent_en[13-13] = 1'h0
//   wr_port_pagematch_en[14-14] = 1'h0
	rdata = 0x00004000;
	lhost_ahb_write(DDR_PCFGW_3, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDMASKCH0_3 -- 1 bytes @'h000000000000061c
//   id_mask[3-0] = 4'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_PCFGIDMASKCH0_3, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDVALUECH0_3 -- 1 bytes @'h0000000000000620
//   id_value[3-0] = 4'h0

	rdata = 0x00000000;
	lhost_ahb_write(DDR_PCFGIDVALUECH0_3, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGR_4 -- 2 bytes @'h00000000000006c4
//   rd_port_priority[9-0] = 10'h000
//   read_reorder_bypass_en[11-11] = 1'h0
//   rd_port_aging_en[12-12] = 1'h0
//   rd_port_urgent_en[13-13] = 1'h0
//   rd_port_pagematch_en[14-14] = 1'h0
//   rd_port_hpr_en[15-15] = 1'h0
	rdata = 0x00014000;
	lhost_ahb_write(DDR_PCFGR_4, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGW_4 -- 2 bytes @'h00000000000006c8
//   wr_port_priority[9-0] = 10'h000
//   wr_port_aging_en[12-12] = 1'h0
//   wr_port_urgent_en[13-13] = 1'h0
//   wr_port_pagematch_en[14-14] = 1'h0
	rdata = 0x00004000;
	lhost_ahb_write(DDR_PCFGW_4, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDMASKCH0_4 -- 1 bytes @'h00000000000006cc
//   id_mask[3-0] = 4'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_PCFGIDMASKCH0_4, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDVALUECH0_4 -- 1 bytes @'h00000000000006d0

//   id_value[3-0] = 4'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_PCFGIDVALUECH0_4, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGR_5 -- 2 bytes @'h0000000000000774
//   rd_port_priority[9-0] = 10'h000
//   read_reorder_bypass_en[11-11] = 1'h0
//   rd_port_aging_en[12-12] = 1'h0
//   rd_port_urgent_en[13-13] = 1'h0
//   rd_port_pagematch_en[14-14] = 1'h0
//   rd_port_hpr_en[15-15] = 1'h0
	rdata = 0x00014000;
	lhost_ahb_write(DDR_PCFGR_5, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGW_5 -- 2 bytes @'h0000000000000778
//   wr_port_priority[9-0] = 10'h000
//   wr_port_aging_en[12-12] = 1'h0
//   wr_port_urgent_en[13-13] = 1'h0
//   wr_port_pagematch_en[14-14] = 1'h0
	rdata = 0x00004000;
	lhost_ahb_write(DDR_PCFGW_5, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDMASKCH0_5 -- 1 bytes @'h000000000000077c
//   id_mask[3-0] = 4'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_PCFGIDMASKCH0_5, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDVALUECH0_5 -- 1 bytes @'h0000000000000780
//   id_value[3-0] = 4'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_PCFGIDVALUECH0_5, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGR_6 -- 2 bytes @'h0000000000000824
//   rd_port_priority[9-0] = 10'h000
//   read_reorder_bypass_en[11-11] = 1'h0
//   rd_port_aging_en[12-12] = 1'h0
//   rd_port_urgent_en[13-13] = 1'h0
//   rd_port_pagematch_en[14-14] = 1'h0
//   rd_port_hpr_en[15-15] = 1'h0
	rdata = 0x00014000;
	lhost_ahb_write(DDR_PCFGR_6, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGW_6 -- 2 bytes @'h0000000000000828
//   wr_port_priority[9-0] = 10'h000
//   wr_port_aging_en[12-12] = 1'h0
//   wr_port_urgent_en[13-13] = 1'h0
//   wr_port_pagematch_en[14-14] = 1'h0
	rdata = 0x00004000;
	lhost_ahb_write(DDR_PCFGW_6, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDMASKCH0_6 -- 1 bytes @'h000000000000082c
//   id_mask[3-0] = 4'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_PCFGIDMASKCH0_6, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDVALUECH0_6 -- 1 bytes @'h0000000000000830
//   id_value[3-0] = 4'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_PCFGIDVALUECH0_6, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGR_7 -- 2 bytes @'h00000000000008d4
//   rd_port_priority[9-0] = 10'h000
//   read_reorder_bypass_en[11-11] = 1'h0
//   rd_port_aging_en[12-12] = 1'h0
//   rd_port_urgent_en[13-13] = 1'h0
//   rd_port_pagematch_en[14-14] = 1'h0
//   rd_port_hpr_en[15-15] = 1'h0
	rdata = 0x00014000;
	lhost_ahb_write(DDR_PCFGR_7, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGW_7 -- 2 bytes @'h00000000000008d8
//   wr_port_priority[9-0] = 10'h000
//   wr_port_aging_en[12-12] = 1'h0
//   wr_port_urgent_en[13-13] = 1'h0
//   wr_port_pagematch_en[14-14] = 1'h0
	rdata = 0x00004000;
	lhost_ahb_write(DDR_PCFGW_7, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDMASKCH0_7 -- 1 bytes @'h00000000000008dc
//   id_mask[3-0] = 4'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_PCFGIDMASKCH0_7, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_MP.PCFGIDVALUECH0_7 -- 1 bytes @'h00000000000008e0
//   id_value[3-0] = 4'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_PCFGIDVALUECH0_7, rdata);

	//SYNOP step2 - deassert soft reset reg
	udelay(1);
	lhost_ahb_write(0xe0000000, 0x00000004);
	lhost_ahb_write(0xe0000004, 0x00000004);
	udelay(1);

	//SYNOP step3 - start PHY init
	//Following programming is for DDR Phy Utility Block(PUB).

        // speed related PUB programming
    switch (speed) {
    case 0: //800MHz
//	rdata = 0xA19CCB66; //1Gb
	rdata = 0x9D9CCB66; //2Gb
	lhost_ahb_write(DDR_PUB_DTPR0, rdata);
//	rdata = 0x2282C400; //1Gb
	rdata = 0x22840400; //2Gb
	lhost_ahb_write(DDR_PUB_DTPR1, rdata);
//	rdata = 0x9002D200;
	rdata = 0x1002D200; //tCCD=4
	lhost_ahb_write(DDR_PUB_DTPR2, rdata);
	rdata = 0x00001D70;
	lhost_ahb_write(DDR_PUB_MR0, rdata);
	rdata = 0x00000018;
	lhost_ahb_write(DDR_PUB_MR2, rdata);
	rdata = 0x00f00AA0;
	lhost_ahb_write(DDR_PUB_PGCR2, rdata);
        break;
    case 1: //640MHz
	rdata = 0x8157B955;
	lhost_ahb_write(DDR_PUB_DTPR0, rdata);
//	rdata = 0x1E823B40; //1Gb
	rdata = 0x1E833B40; //2Gb
	lhost_ahb_write(DDR_PUB_DTPR1, rdata);
//	rdata = 0x9002C200;
	rdata = 0x1002C200; //tCCD=4
	lhost_ahb_write(DDR_PUB_DTPR2, rdata);
	rdata = 0x00001B60;
	lhost_ahb_write(DDR_PUB_MR0, rdata);
	rdata = 0x00000010;
	lhost_ahb_write(DDR_PUB_MR2, rdata);
	rdata = 0x00f00830;
	lhost_ahb_write(DDR_PUB_PGCR2, rdata);
        break;
    case 3: //400MHz
	rdata = 0x510E7644;
	lhost_ahb_write(DDR_PUB_DTPR0, rdata);
//	rdata = 0x16816200; //1Gb
	rdata = 0x16820200; //2Gb
	lhost_ahb_write(DDR_PUB_DTPR1, rdata);
//	rdata = 0x90022A00;
	rdata = 0x10022A00; //tCCD=4
	lhost_ahb_write(DDR_PUB_DTPR2, rdata);
	rdata = 0x00001520;
	lhost_ahb_write(DDR_PUB_MR0, rdata);
	rdata = 0x00000000;
	lhost_ahb_write(DDR_PUB_MR2, rdata);
	rdata = 0x00f00488;
	lhost_ahb_write(DDR_PUB_PGCR2, rdata);
        break;
    case 4: //320MHz
	rdata = 0x410C7544;
	lhost_ahb_write(DDR_PUB_DTPR0, rdata);
//	rdata = 0x168121A0; //1Gb
	rdata = 0x1681A1A0; //2Gb
	lhost_ahb_write(DDR_PUB_DTPR1, rdata);
//	rdata = 0x90022A00;
	rdata = 0x10022A00; //tCCD=4
	lhost_ahb_write(DDR_PUB_DTPR2, rdata);
	rdata = 0x00001320;
	lhost_ahb_write(DDR_PUB_MR0, rdata);
	rdata = 0x00000000;
	lhost_ahb_write(DDR_PUB_MR2, rdata);
	rdata = 0x00F00350;
	lhost_ahb_write(DDR_PUB_PGCR2, rdata);
        break;
    default:
	//Configure Timing parameter registers
	rdata = 0x65129744; //AL=CL-1
//	rdata = 0x65127744; //AL=0
	lhost_ahb_write(DDR_PUB_DTPR0, rdata);
//	rdata = 0x1A81BA80; //1Gb

	if (size == DDR_256MB){
		rdata = 0x1A828280; //2Gb
	} else {
		rdata = 0x1A81BA80; //1Gb
	}

	lhost_ahb_write(DDR_PUB_DTPR1, rdata);
//	rdata = 0x90023200;
	rdata = 0x10023200; //tCCD=4
	lhost_ahb_write(DDR_PUB_DTPR2, rdata);
	rdata = 0x1940; //CL8
	lhost_ahb_write(DDR_PUB_MR0, rdata);
	rdata = 0x0008;
	lhost_ahb_write(DDR_PUB_MR2, rdata);
	//Configure PHY general configuration registers.
	rdata = 0x00f0023f;
	lhost_ahb_write(DDR_PUB_PGCR2, rdata);
        break;
    }

	rdata = (type==32)?0x7c000e81:0x7c000e80;
	lhost_ahb_write(DDR_PUB_DX2GCR, rdata);
	lhost_ahb_write(DDR_PUB_DX3GCR, rdata);
        // non-speed related PUB programming
	//Read DRAM Control Register
	lhost_ahb_read_data(DDR_PUB_DCR, rdata);
	//Enable it for DDR3
	rdata = 0x0000040b;
	lhost_ahb_write(DDR_PUB_DCR, rdata);
	//Configure Mode Register 0 of PUB
	rdata = 0x000E; // Rtt_Nom 60 ohms, AL=CL-1
//	rdata = 0x0006; // Rtt_Nom 60 ohms, AL=0
	lhost_ahb_write(DDR_PUB_MR1, rdata);
	rdata = 0x0000;
	lhost_ahb_write(DDR_PUB_MR3, rdata);

	//Configure PHY Timing registers.
	rdata = 0x0fa0fa10;
	lhost_ahb_write(DDR_PUB_PTR0, rdata);
//	rdata = 0x00800080;
	rdata = 0x61a808ca;
	lhost_ahb_write(DDR_PUB_PTR1, rdata);
//      rdata = 0x00080421;
//    lhost_ahb_write(DDR_PUB_PTR2, rdata); // for simulation only, should not need to be written

	//SYNOP step4 - monitor PHY init status
	//Now wait till PHY initialization done.
	rdata = 0x0;
#ifdef ENABLE_DDR_VERBOSE
        printf("DDR_INIT: in step 4\n");
#endif
	while ((rdata & 1) != 1) {
		lhost_ahb_read_data(DDR_PUB_PGSR0, rdata);
	}
	//Calibralte local line registers.
//	rdata = 0x0000006E;
//	lhost_ahb_write(DDR_PUB_DX0LCDLR2, rdata);
//	lhost_ahb_write(DDR_PUB_DX1LCDLR2, rdata);
//	lhost_ahb_write(DDR_PUB_DX2LCDLR2, rdata);
//	lhost_ahb_write(DDR_PUB_DX3LCDLR2, rdata);

	//SYNOP step5 - Start Phy Initialization Register through controller
	rdata = 0x00040001;
	lhost_ahb_write(DDR_PUB_PIR, rdata);
	udelay(1);

	//Now wait till PHY initialization done after local line register changes.
	rdata = 0x0;
#ifdef ENABLE_DDR_VERBOSE
        printf("DDR_INIT: wait for PHY init\n");
#endif
	while ((rdata & 0x1f) != 0x1f) {
		lhost_ahb_read_data(DDR_PUB_PGSR0, rdata);
	}

	//SYNOP step6 - set DFIMISC.dfi_init_complete_en to 1
	rdata = 0x00000001;
	lhost_ahb_write(DDR_DFIMISC, rdata);

	//SYNOP step7 - wait for DWC_ddr_umctl2 to move to "normal" by monitoring STAT.operating_mode signal
	rdata = 0x0;
#ifdef ENABLE_DDR_VERBOSE
        printf("DDR_INIT: in step7\n");
#endif
	while ((rdata & 3) != 1) {
		lhost_ahb_read_data(DDR_STAT, rdata);
	}

	//SYNOP step8 - disable auto-refreshes and powerdown
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.RFSHCTL3 -- 1 bytes @'h0000000000000060
//   dis_auto_refresh[0-0] = 1'h1
//   refresh_update_level[1-1] = 1'h0
	rdata = 0x00000001;
	lhost_ahb_write(DDR_RFSHCTL3, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.PWRCTL -- 1 bytes @'h0000000000000030
//   selfref_en[0-0] = 1'h0
//   powerdown_en[1-1] = 1'h0
//   en_dfi_dram_clk_disable[3-3] = 1'h0
	rdata = 0x00000000;
	lhost_ahb_write(DDR_PWRCTL, rdata);
	rdata = 0x00000000;
	lhost_ahb_write(DDR_DFIMISC, rdata);
	rdata = 0x44181ee4;
	lhost_ahb_write(DDR_PUB_DXCCR, rdata);

// Zo of 34 ohms (0xd) or 40 ohms (0xb) and ODT of 60ohms (5), 120 ohms (1), 40ohms (8)
	rdata = 0x101d;
	lhost_ahb_write(DDR_PUB_ZQ0CR1, rdata);

	//SYNOP step9 - user MPR for training
//	rdata = 0x9f003587;//default
	rdata = 0x9f0035c7;
	lhost_ahb_write(DDR_PUB_DTCR, rdata);

	//SYNOP step10a - configure PUB PIR register for write leveling
//	rdata = 0x00005601;
	rdata = 0x0000FE01;
	lhost_ahb_write(DDR_PUB_PIR, rdata);
	udelay(1);
	rdata = 0x0;
	//SYNOP step11a - monitor PGSR0.IDONE
#ifdef ENABLE_DDR_VERBOSE
        printf("DDR_INIT: in step11a\n");
#endif
	while ((rdata & 0xFE1) != 0xFE1) {
//	while ((rdata & 0x561) != 0x561) {
		lhost_ahb_read_data(DDR_PUB_PGSR0, rdata);
#ifdef ENABLE_DDR_VERBOSE
//		printf("DDR_INIT: DDR_PUB_PGSR0 = %x\n", rdata);
#endif
	}
	if((rdata & 0x0FE00000) != 0x0) {
		printf("DDR_INIT: ERROR : DDR training fails");
	}

	//SYNOP step9 - program PUB DTCR back to default
	rdata = 0x9f003587;//default
	lhost_ahb_write(DDR_PUB_DTCR, rdata);

	//SYNOP step10a - configure PUB PIR register for write leveling
//	rdata = 0x00005601;
	rdata = 0x0000FC01;
	lhost_ahb_write(DDR_PUB_PIR, rdata);
	udelay(1);
	rdata = 0x0;
	//SYNOP step11a - monitor PGSR0.IDONE
#ifdef ENABLE_DDR_VERBOSE
        printf("DDR_INIT: in step11a\n");
#endif
	while ((rdata & 0xFC1) != 0xFC1) {
//	while ((rdata & 0x561) != 0x561) {
		lhost_ahb_read_data(DDR_PUB_PGSR0, rdata);
#ifdef ENABLE_DDR_VERBOSE
//		printf("DDR_INIT: DDR_PUB_PGSR0 = %x\n", rdata);
#endif
	}
	if((rdata & 0x0FC00000) != 0x0) {
		printf("DDR_INIT: ERROR : DDR training fails");
	}
#ifdef ENABLE_DDR_VERBOSE
        rdata = REG_READ(DDR_PUB_ZQ0SR0);
        printf("DDR_INIT: DDR_PUB_ZQ0SR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_ZQ0SR1);
        printf("DDR_INIT: DDR_PUB_ZQ0SR1 = %x\n", rdata);

        rdata = REG_READ(DDR_PUB_DX0GSR0);
        printf("DDR_INIT: DDR_PUB_DX0GSR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX0GSR1);
        printf("DDR_INIT: DDR_PUB_DX0GSR1 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX0GSR2);
        printf("DDR_INIT: DDR_PUB_DX0GSR2 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX0BDLR0);
        printf("DDR_INIT: DDR_PUB_DX0BDLR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX0BDLR1);
        printf("DDR_INIT: DDR_PUB_DX0BDLR1 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX0BDLR2);
        printf("DDR_INIT: DDR_PUB_DX0BDLR2 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX0BDLR3);
        printf("DDR_INIT: DDR_PUB_DX0BDLR3 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX0BDLR4);
        printf("DDR_INIT: DDR_PUB_DX0BDLR4 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX0LCDLR0);
        printf("DDR_INIT: DDR_PUB_DX0LCDLR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX0LCDLR1);
        printf("DDR_INIT: DDR_PUB_DX0LCDLR1 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX0LCDLR2);
        printf("DDR_INIT: DDR_PUB_DX0LCDLR2 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX0MDLR);
        printf("DDR_INIT: DDR_PUB_DX0MDLR = %x\n", rdata);

        rdata = REG_READ(DDR_PUB_DX1GSR0);
        printf("DDR_INIT: DDR_PUB_DX1GSR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX1GSR1);
        printf("DDR_INIT: DDR_PUB_DX1GSR1 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX1GSR2);
        printf("DDR_INIT: DDR_PUB_DX1GSR2 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX1BDLR0);
        printf("DDR_INIT: DDR_PUB_DX1BDLR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX1BDLR1);
        printf("DDR_INIT: DDR_PUB_DX1BDLR1 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX1BDLR2);
        printf("DDR_INIT: DDR_PUB_DX1BDLR2 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX1BDLR3);
        printf("DDR_INIT: DDR_PUB_DX1BDLR3 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX1BDLR4);
        printf("DDR_INIT: DDR_PUB_DX1BDLR4 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX1LCDLR0);
        printf("DDR_INIT: DDR_PUB_DX1LCDLR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX1LCDLR1);
        printf("DDR_INIT: DDR_PUB_DX1LCDLR1 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX1LCDLR2);
        printf("DDR_INIT: DDR_PUB_DX1LCDLR2 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX1MDLR);
        printf("DDR_INIT: DDR_PUB_DX1MDLR = %x\n", rdata);

        rdata = REG_READ(DDR_PUB_DX2GSR0);
        printf("DDR_INIT: DDR_PUB_DX2GSR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX2GSR1);
        printf("DDR_INIT: DDR_PUB_DX2GSR1 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX2GSR2);
        printf("DDR_INIT: DDR_PUB_DX2GSR2 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX2BDLR0);
        printf("DDR_INIT: DDR_PUB_DX2BDLR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX2BDLR1);
        printf("DDR_INIT: DDR_PUB_DX2BDLR1 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX2BDLR2);
        printf("DDR_INIT: DDR_PUB_DX2BDLR2 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX2BDLR3);
        printf("DDR_INIT: DDR_PUB_DX2BDLR3 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX2BDLR4);
        printf("DDR_INIT: DDR_PUB_DX2BDLR4 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX2LCDLR0);
        printf("DDR_INIT: DDR_PUB_DX2LCDLR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX2LCDLR1);
        printf("DDR_INIT: DDR_PUB_DX2LCDLR1 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX2LCDLR2);
        printf("DDR_INIT: DDR_PUB_DX2LCDLR2 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX2MDLR);
        printf("DDR_INIT: DDR_PUB_DX2MDLR = %x\n", rdata);

        rdata = REG_READ(DDR_PUB_DX3GSR0);
        printf("DDR_INIT: DDR_PUB_DX3GSR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX3GSR1);
        printf("DDR_INIT: DDR_PUB_DX3GSR1 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX3GSR2);
        printf("DDR_INIT: DDR_PUB_DX3GSR2 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX3BDLR0);
        printf("DDR_INIT: DDR_PUB_DX3BDLR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX3BDLR1);
        printf("DDR_INIT: DDR_PUB_DX3BDLR1 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX3BDLR2);
        printf("DDR_INIT: DDR_PUB_DX3BDLR2 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX3BDLR3);
        printf("DDR_INIT: DDR_PUB_DX3BDLR3 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX3BDLR4);
        printf("DDR_INIT: DDR_PUB_DX3BDLR4 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX3LCDLR0);
        printf("DDR_INIT: DDR_PUB_DX3LCDLR0 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX3LCDLR1);
        printf("DDR_INIT: DDR_PUB_DX3LCDLR1 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX3LCDLR2);
        printf("DDR_INIT: DDR_PUB_DX3LCDLR2 = %x\n", rdata);
        rdata = REG_READ(DDR_PUB_DX3MDLR);
        printf("DDR_INIT: DDR_PUB_DX3MDLR = %x\n", rdata);
#endif
	//lhost_ahb_write(DDR_PUB_DTCR, 0x00003583);    // disable data training
	//enable auto-refresh and powerdown
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.RFSHCTL3 -- 1 bytes @'h0000000000000060
//   dis_auto_refresh[0-0] = 1'h0
//   refresh_update_level[1-1] = 1'h0
        rdata = 0x0;
	lhost_ahb_write(DDR_RFSHCTL3, rdata);
//Register DWC_ddr_umctl2_map_UMCTL2_REGS.PWRCTL -- 1 bytes @'h0000000000000030
//   selfref_en[0-0] = 1'h0
//   powerdown_en[1-1] = 1'h0
//   en_dfi_dram_clk_disable[3-3] = 1'h0
        rdata = 0x0;
        lhost_ahb_write(DDR_PWRCTL, rdata);
	        //dqs training
	//rdata = (0x14<<0) | (0x14<<6) | (0x14<<12) | (0x14<<18);
	//REG_WRITE(DDR_PUB_DX0BDLR3, rdata);
	//REG_WRITE(DDR_PUB_DX0BDLR4, rdata);
	//
#ifdef ENABLE_DDR_VERBOSE
	rdata = REG_READ(DDR_PUB_DX0LCDLR2);
	printf("DDR_INIT: DDR_PUB_DX0LCDLR2 = %x\n", rdata);
#endif
	return 0;
}

/* END COPY-PASTE FROM ARCSHELL */

int ddr_init(u32 type, u32 speed, u32 size)
{
	u32 ddr_speed[] = {DDR3_800MHz, DDR3_640MHz, DDR3_500MHz, DDR3_400MHz, DDR3_320MHz};
	u32 idx;

	for (idx = 0; idx < (sizeof(ddr_speed) / sizeof(ddr_speed[0])); idx++)
		if (ddr_speed[idx] == speed) break;

	if (type == DDR3_16_WINBOND) {
		return ddr3_init(16, idx, size);
	} else if (type == DDR3_32_WINBOND) {
		return ddr3_init(32, idx, size);
	} else {
		return -1;
	}
}
#endif /* TOPAZ_ASIC_PLATFORM */

