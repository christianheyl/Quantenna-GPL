/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ASM_ARC_ARCDEFS_H
#define _ASM_ARC_ARCDEFS_H

#ifdef __KERNEL__

/* These are extension BCR's*/
#define ARC_REG_DCCMBASE_BCR 0x61   // DCCM Base Addr
#define ARC_REG_CRC_BCR      0x62
#define ARC_REG_DVFB_BCR     0x64
#define ARC_REG_EXTARITH_BCR 0x65
#define ARC_REG_VECBASE_BCR  0x68
#define ARC_REG_PERIBASE_BCR 0x69
#define ARC_REG_FP_BCR       0x6B   // Single-Precision FPU
#define ARC_REG_DPFP_BCR     0x6C   // Dbl Precision FPU
#define ARC_REG_MMU_BCR      0x6f
#define ARC_REG_DCCM_BCR     0x74   // DCCM Present + SZ
#define ARC_REG_TIMERS_BCR   0x75
#define ARC_REG_ICCM_BCR     0x78
#define ARC_REG_XY_MEM_BCR   0x79
#define ARC_REG_MAC_BCR      0x7a
#define ARC_REG_MUL_BCR      0x7b
#define ARC_REG_SWAP_BCR     0x7c
#define ARC_REG_NORM_BCR     0x7d
#define ARC_REG_MIXMAX_BCR   0x7e
#define ARC_REG_BARREL_BCR   0x7f
#define ARC_REG_D_UNCACH_BCR 0x6A
#define ARC_REG_STATUS32     0x0A

/* status32 Bits Positions */
#define STATUS_H_BIT    0
#define STATUS_E1_BIT   1
#define STATUS_E2_BIT   2
#define STATUS_A1_BIT   3
#define STATUS_A2_BIT   4
#define STATUS_AE_BIT   5
#define STATUS_DE_BIT   6
#define STATUS_U_BIT    7
#define STATUS_L_BIT    12

/* These masks correspond to the status word(STATUS_32) bits */
#define STATUS_H_MASK   (1<<STATUS_H_BIT)   /* Mask for Halt bit */
#define STATUS_E1_MASK  (1<<STATUS_E1_BIT)  /* Mask for Int 1 enable */
#define STATUS_E2_MASK  (1<<STATUS_E2_BIT)  /* Mask for Int 2 enable */
#define STATUS_A1_MASK  (1<<STATUS_A1_BIT)  /* Interrupt 1 active */
#define STATUS_A2_MASK  (1<<STATUS_A2_BIT)  /* Interrupt 2 active */
#define STATUS_AE_MASK  (1<<STATUS_AE_BIT)  /* Exception active */
#define STATUS_DE_MASK  (1<<STATUS_DE_BIT)  /* PC is in delay slot */
#define STATUS_U_MASK   (1<<STATUS_U_BIT)   /* User/Kernel mode bit */
#define STATUS_L_MASK   (1<<STATUS_L_BIT)   /* Loop inhibit bit */

/* These masks correspond to the exception cause register (ecause) bits */

#define ECAUSE_VECTOR_MASK     0xff0000
#define ECAUSE_CODE_MASK       0x00ff00
#define ECAUSE_PARAMETER_MASK  0x0000ff


/* Auxiliary registers not supported by the assembler */
#define AUX_IDENTITY        4
#define AUX_INTR_VEC_BASE   0x25
#define AUX_IRQ_LEV         0x200   /* interrupt priority level setting */
#define AUX_IRQ_HINT        0x201   /* Aux register for generating software interrupts */
#define AUX_IRQ_LV12        0x43    /* interrupt level register */

#define AUX_IENABLE         0x40c   /* Known to the assembler as auxienable, so don't really need this */
#define AUX_ITRIGGER        0x40d

/* Privileged MMU auxiliary register definitions */
#define ARC_REG_TLBPD0      0x405
#define ARC_REG_TLBPD1      0x406
#define ARC_REG_TLBINDEX    0x407
#define ARC_REG_TLBCOMMAND  0x408
#define ARC_REG_PID         0x409
#define ARC_REG_SCRATCH_DATA0   0x418
#define ARC_REG_SASID       0x40e

/* Bits in MMU PID register */
#define TLB_ENABLE          (1 << 31)   /* Enable MMU for process */

/* Software can choose whether to utilize SASID (provided by v3) */
#ifdef CONFIG_ARC_MMU_SASID
#define SASID_ENABLE        (1 << 29)   /* enable SASID for process */
#else
#define SASID_ENABLE        0
#endif

/* In MMU-v3, there is option to enable sasid per process.
 * However for now, it is enabled for all.
 * Non-relevant processes won't have a shared TLB entry to begin with
 * thus this bit being set for them won't matter anyways
 * Also it will help catch bugs initially - due to stray shared TLB entries
 */
#define MMU_ENABLE          (TLB_ENABLE|SASID_ENABLE)

/* Error code if probe fails */
#define TLB_LKUP_ERR    0x80000000

/* Instruction cache related Auxiliary registers */
#define ARC_REG_I_CACHE_BUILD_REG 0x77
#define ARC_REG_IC_IVIC     0x10
#define ARC_REG_IC_CTRL     0x11
#define ARC_REG_IC_IVIL     0x19
#define ARC_REG_IC_RAM_ADDR 0x1A
#define ARC_REG_IC_DATA     0x1D
#if (CONFIG_ARC_MMU_VER > 2)
#define ARC_REG_IC_PTAG     0x1E
#endif

/* Bit val in IC_CTRL */
#define BIT_IC_CTRL_CACHE_DISABLE      0x01
#define BIT_IC_CTRL_SB                 0x08
#define BIT_IC_CTRL_ADDRESS_DEBUG_TYPE 0x20

/* Data cache related Auxiliary registers */
#define ARC_REG_DC_IVDC     0x47
#define ARC_REG_DC_CTRL     0x48
#define ARC_REG_DC_IVDL     0x4A
#define ARC_REG_DC_FLSH     0x4B
#define ARC_REG_DC_FLDL     0x4C
#if (CONFIG_ARC_MMU_VER > 2)
#define ARC_REG_DC_PTAG     0x5C
#endif
#define ARC_REG_DC_RAM_ADDR             0x58
#define ARC_REG_DC_TAG                  0x59
#define ARC_REG_DC_WP                   0x5A
#define ARC_REG_DC_DATA                 0x5B
#define ARC_REG_DC_BUILD_REG            0x72


/* Bit val in DC_CTRL */
#define BIT_DC_CTRL_INV_MODE_FLUSH  0x40
#define BIT_DC_CTRL_FLUSH_STATUS    0x100

/* Timer related Aux registers */
#define ARC_REG_TIMER0_LIMIT 0x23 /* timer 0 limit */
#define ARC_REG_TIMER0_CTRL  0x22 /* timer 0 control */
#define ARC_REG_TIMER0_CNT   0x21 /* timer 0 count */
#define ARC_REG_TIMER1_LIMIT 0x102 /* timer 1 limit */
#define ARC_REG_TIMER1_CTRL  0x101 /* timer 1 control */
#define ARC_REG_TIMER1_CNT   0x100 /* timer 1 count */

#define TIMER_CTRL_IE    (1 << 0)    /* Interupt when Count reachs limit */
#define TIMER_CTRL_NH    (1 << 1)    /* Count only when CPU NOT halted */

#ifdef CONFIG_ARCH_ARC800

#define ARC_AUX_IDU_REG_CMD     0x2000
#define ARC_AUX_IDU_REG_PARAM   0x2001

#define ARC_AUX_XTL_REG_CMD     0x2002
#define ARC_AUX_XTL_REG_PARAM   0x2003

#define ARC_REG_MP_BCR          0x2021

#define ARC_XTL_REG_SYNTAX_PARAM_PC     1   /* Left shift by 1 */
#define ARC_XTL_REG_SYNTAX_CMD_CPU_ID   8   /* Left shift by 8 */

#define ARC_XTL_CMD_WRITE_PC    0x04
#define ARC_XTL_CMD_CLEAR_HALT  0x02


#endif

// Profiling AUX regs.


#define ARC_PCT_CONTROL         0x255
#define ARC_HWP_CTRL            0xc0fcb018 //Periphbase + b000 + 18
#define PR_CTRL_EN              (1<<0)
#define ARC_PR_ID                   0xc0fcb000

/* Floating Pt Registers
 * Status regs are read-only (build-time) so need not be saved/restored
 */
#define ARC_AUX_FP_STAT         0x300
#define ARC_AUX_DPFP_1L         0x301
#define ARC_AUX_DPFP_1H         0x302
#define ARC_AUX_DPFP_2L         0x303
#define ARC_AUX_DPFP_2H         0x304
#define ARC_AUX_DPFP_STAT       0x305

#ifndef __ASSEMBLY__


/******************************************************************
 *      Inline ASM macros to read/write AUX Regs
 *      Essentially invocation of lr/sr insns from "C"
 *****************************************************************/

#if 0

#define read_new_aux_reg(reg)               __builtin_arc_lr(reg)

// gcc builtin sr needs reg param to be long immediate
#define write_new_aux_reg(reg_immed, val)   \
                    __builtin_arc_sr((unsigned int)val, reg_immed)

#else

#define read_new_aux_reg(reg)                                   \
({ unsigned int __ret;                                          \
    __asm__ __volatile__("lr    %0, [%1]":"=r"(__ret):"i"(reg));\
    __ret;                                                      \
})

/* Aux Reg address is specified as long immediate by caller
 * e.g.
 *    write_new_aux_reg(0x69, some_val);
 * This generates tightest code.
 */
#define write_new_aux_reg(reg_immed, val)   \
({                                          \
    __asm__ __volatile__ (                  \
        "sr   %0, [%1]"                     \
        :                                   \
        :"ir"(val),"i"(reg_immed));          \
})

/* Aux Reg address is specified in a variable
 *  * e.g.
 *      reg_num = 0x69
 *      write_new_aux_reg2(reg_num, some_val);
 * This has to generate glue code to load the reg num from
 *  memory to a reg hence not recommended.
 */
#define write_new_aux_reg2(reg_in_var, val) \
({                                          \
    unsigned int tmp;                       \
                                            \
    __asm__ __volatile__ (                  \
        "ld   %0, [%2]  \n\t"               \
        "sr   %1, [%0]  \n\t"               \
        :"=&r"(tmp)                         \
        :"r"(val),"memory"(&reg_in_var));   \
})

#endif

/****************************************************************
 * Register Layouts using bitfields so that we dont have to write
 * bit fiddling ourselves; the compiler can do that for us
 */
struct bcr_identity {
    unsigned int
        family: 8,
        cpu_id: 8,
        chip_id:16;
};

struct bcr_mmu_1_2 {
    unsigned int
        u_dtlb  :8,
        u_itlb  :8,
        sets    :4,
        ways    :4,
        ver     :8;
};

struct bcr_mmu_3 {
    unsigned int
        u_dtlb  :4,
        u_itlb  :4,
        pg_sz   :4,
        reserv  :3,
        osm     :1,
        sets    :4,
        ways    :4,
        ver     :8;
};


#define EXTN_SWAP_VALID     0x1
#define EXTN_NORM_VALID     0x2
#define EXTN_MINMAX_VALID   0x2
#define EXTN_BARREL_VALID   0x2

struct bcr_extn {
    unsigned int

        /* Prog Ref Manual */
        swap:1, norm:2, minmax:2, barrel:2, mul:2, ext_arith:2,

        crc:1,              /* DSP-LIB Ref Manual */
        dvfb:1,             /* Dual Viterbi Butterfly Instrn:
                               Exotic but not supported by 700
                             */
        padding:1;
};

/* DSP Options Ref Manual */
struct bcr_extn_mac_mul {
    int ver:8, type:8;
};

struct bcr_extn_xymem {
    unsigned int   ver:8,
                    bank_sz:4, num_banks:4,
                    ram_org:2;
};

struct bcr_cache {
    unsigned long ver:8, config:4, sz:4, line_len:4, pad:12;
};

struct bcr_perip {
    unsigned long pad:8, sz:8, pad2:8, start:8;
};
struct bcr_iccm {
    unsigned long ver:8, sz:3, reserved:5, base:16;
};

/* DCCM Base Address Register: ARC_REG_DCCMBASE_BCR */
struct bcr_dccm_base {
    unsigned long ver:8, addr:24;
};

/* DCCM RAM Configuration Register : ARC_REG_DCCM_BCR */
struct bcr_dccm {
    unsigned long ver:8, sz: 3, res:21;
};

// Both SP and DP FPU BCRs have same format
struct bcr_fp {
    unsigned int ver:8,
                 fast:1;
};

struct bcr_ccm {
    unsigned int base_addr;
    unsigned int sz;
};

#ifdef CONFIG_ARCH_ARC800
struct bcr_mp {
    unsigned int ver:8, scu:1, idu:1, sdu:1, padding:5, mp_arch:16;
};
#endif

struct cpuinfo_arc_cache {
    unsigned int has_aliasing, sz, line_len, assoc, ver;
};

struct cpuinfo_arc_mmu {
    unsigned int ver,
                 pg_sz,           /* MMU Page Size */
                 sets, ways,      /* JTLB Geometry */
                 u_dtlb, u_itlb,  /* entries in Full-assoc I-uTLB, D-uTLB */
                 num_tlb;         /* sets x ways */
};


/*************************************************************
 * Top level structure which exports the CPU info such as
 * Cache info, MMU info etc to rest of ARCH specific Code
 */
struct cpuinfo_arc {
    struct cpuinfo_arc_cache icache, dcache;
    struct cpuinfo_arc_mmu mmu;
    struct bcr_identity core;
    unsigned int timers;
    unsigned int vec_base;
    unsigned int perip_base;
    struct bcr_perip uncached_space;   // For mapping Periph Regs etc
    struct bcr_extn extn;
    struct bcr_extn_xymem extn_xymem;
    struct bcr_extn_mac_mul extn_mac_mul;// DCCM RAM SZ
    struct bcr_ccm iccm, dccm;
#ifdef CONFIG_ARCH_ARC800
    struct bcr_mp  mp;
#endif
    struct bcr_fp  fp, dpfp;
};

#ifdef CONFIG_ARCH_ARC_FPU
/* These DPFP regs need to be saved/restored across ctx-sw */
struct arc_fpu {
    struct {
        unsigned int l, h;
    }
    aux_dpfp[2];
};
#endif


/* Helpers */
#define TO_KB(x) (x >> 10)

#define READ_BCR(reg, into)             \
{                                       \
    unsigned int tmp;                   \
    tmp = read_new_aux_reg(reg);        \
    if (sizeof(tmp) == sizeof(into))    \
        into = *((typeof(into) *)&tmp); \
    else  {                             \
        extern void bogus_undefined(void);\
        bogus_undefined();              \
    }                                   \
}

#endif  /* __KERNEL__ */

#endif  /* __ASEMBLY__ */

#endif  /* _ASM_ARC_ARCDEFS_H */
