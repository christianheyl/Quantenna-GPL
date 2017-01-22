/*
 * Copyright Codito Technologies (www.codito.com)  
 *
 *  include/asm-arc/arcregs.h
 *
 *  Copyright (C) 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors: Sandeep Patil (sandeep.patil@codito.com)
 * 			Pradeep Sawlani (pradeep.sawlani@codito.com)
 * Auxiliary register definitions and macros to read and write to them. 
 */

#ifndef	_ASM_ARC_ARCDEFS_H
#define	_ASM_ARC_ARCDEFS_H

/* These masks correspond to the status word(STATUS_32) bits */
#define	STATUS_H_SET			0x01		/* Mask for Halt bit */
#define	STATUS_E1_MASK			0x02		/* Mask for Interrupt 1 enable */
#define	STATUS_E2_MASK			0x04		/* Mask for Interrupt 2 enable */
#define STATUS_DISABLE_INTERRUPTS	0xFFFFFFF9	/* Mask to disable Interrupts  */

/* Auxiliary register values	*/
#define ARC_REG_STATUS32	0x0A /* status 32 register */ 		
#define ARC_REG_TIMER0_LIMIT	0x23 /* timer 0 limit */
#define ARC_REG_TIMER0_CTRL	0x22 /* timer 0 control */
#define ARC_REG_TIMER0_CNT   0x21 /* timer 0 count */
#define ARC_REG_PC		0x06 /* program counter */

/* MMU related Auxiliary registers */
#define ARC_REG_DATA_UNCACHED	0x6A
#define ARC_REG_TLBPD0		0x405
#define ARC_REG_TLBPD1		0x406
#define ARC_REG_TLBINDEX	0x407
#define ARC_REG_TLBCOMMAND	0x408
#define ARC_REG_PID		0x409
#define SCRATCH_DATA0		0x418

/* Timer related Aux registers */
#define ARC_REG_TIMER0_LIMIT 0x23 /* timer 0 limit */
#define ARC_REG_TIMER0_CTRL  0x22 /* timer 0 control */
#define ARC_REG_TIMER0_CNT   0x21 /* timer 0 count */
#define ARC_REG_TIMER1_LIMIT 0x102 /* timer 1 limit */
#define ARC_REG_TIMER1_CTRL  0x101 /* timer 1 control */
#define ARC_REG_TIMER1_CNT   0x100 /* timer 1 count */

#define TIMER_CTRL_IE    (1 << 0)    /* Interupt when Count reachs limit */
#define TIMER_CTRL_NH    (1 << 1)    /* Count only when CPU NOT halted */

/* Interrupt related Auxilliary registers */
#define ARC_REG_INTR_VEC_BASE		0x25
#define ARC_REG_INTR_ENABLE		0x40c

/* Instruction cache related Auxiliary registers */
#define ARC_REG_IC_BUILD_REG		0x77
#define ARC_REG_IC_IVIC			0x10
#define ARC_REG_IC_CTRL			0x11
#define ARC_REG_IC_IVIL			0x19
#if (PLATFORM_ARC7_MMU_VER >= 3)
	#define ARC_REG_IC_PTAG		0x1E
#endif

/* Data cache related Auxiliary registers */
#define ARC_REG_DC_BUILD_REG		0x72
#define ARC_REG_DC_IVDC			0x47
#define ARC_REG_DC_CTRL			0x48
#define ARC_REG_DC_IVDL			0x4A
#define ARC_REG_DC_FLSH			0x4B
#define ARC_REG_DC_FLDL			0x4C
#if (PLATFORM_ARC7_MMU_VER >= 3)
	#define ARC_REG_DC_PTAG		0x5C
#endif

/* ARC_REG_DC_CTRL fields */
#define ARC_INV_MODE_FLUSH		0x40
#define ARC_DC_FLUSH_STATUS_BIT		0x100
#define ARC_DC_DISABLE			0x01

/* ARC_REG_IC_CTRL fields */
#define ARC_IC_DISABLE			0x01

/* Cache Line lengths fixed to 32 bytes */
#define L1_CACHE_SHIFT		5
#define L1_CACHE_BYTES		(1 << L1_CACHE_SHIFT)
#define ARC_ICACHE_LINE_LEN	L1_CACHE_BYTES
#define ARC_DCACHE_LINE_LEN	L1_CACHE_BYTES


/* Inline macros for reading, writing into auxiliary registers	*/

#ifndef	__ASSEMBLY__
/* Read an auxiliary register */
#define	read_new_aux_reg(reg)						\
	({ unsigned int __ret;							\
	 	__asm__ __volatile__("lr	%0, [%1]":"=r"(__ret):"i"(reg));\
	 	__ret;								\
	 })
		
/* Write to an auxiliary register */

#define	write_new_aux_reg(reg, val)					\
	({ 									\
	 	__asm__ __volatile__ ("sr	%0, [%1]"::"r"(val),"i"(reg));		\
	 })

#endif	/* __ASSEMBLY__ */

#endif	/* _ASM_ARC_ARCDEFS_H */

