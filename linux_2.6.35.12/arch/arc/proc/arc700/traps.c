/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * vineetg: May 2011
 *  -user-space unaligned access emulation
 *
 * Vineetg: June 10th 2008
 *  -Added show_callee_regs to display CALLEE REGS
 *  -Added show_fault_diagnostics as a common function to display all
 *     the REGS, trigger event logging etc
 *
 * Rahul Trivedi: Codito Technologies 2004
 */

#include <linux/sched.h>
#include <linux/kdebug.h>
#include <asm/event-log.h>
#include <asm/uaccess.h>

#include <asm/board/unaligned_accounting.h>

extern int fixup_exception(struct pt_regs *regs);
void show_kernel_fault_diag(const char *str, struct pt_regs *regs,
                        unsigned long address, unsigned long cause_reg);


/* "volatile" because it causes compiler to optimize away code.
 * Since running_on_hw is init to 1 at compile time, with -O2 compiler
 * throws away the code in die( ) which is a problem on ISS
 */
volatile int running_on_hw = 1;

void die(const char *str, struct pt_regs *regs, unsigned long address,
	unsigned long cause_reg)
{
	if (running_on_hw) {
		oops_in_progress = 1;
		show_kernel_fault_diag(str, regs, address, cause_reg);
	}

	// DEAD END
	__asm__("flag 1");
}

static int noinline do_fatal_exception(unsigned long cause, char *str,
        struct pt_regs *regs, siginfo_t * info)
{
    if (user_mode(regs)) {
        struct task_struct *tsk = current;

        tsk->thread.fault_address = (unsigned int)info->si_addr;
        tsk->thread.cause_code = cause;

		force_sig_info(info->si_signo, info, tsk);

    } else {
        /* Are we prepared to handle this kernel fault?
         *
         * (The kernel has valid exception-points in the source
         *  when it acesses user-memory. When it fails in one
         *  of those points, we find it in a table and do a jump
         *  to some fixup code that loads an appropriate error
         *  code)
         */
        if (fixup_exception(regs))
            return 0;

        /*
         * Oops. The kernel tried to access some bad page.
         * We'll have to terminate things with extreme prejudice.
         */
        die(str, regs, (unsigned long)info->si_addr, cause);
    }

    return 1;
}

#define DO_ERROR_INFO(signr, str, name, sicode) \
int name(unsigned long cause, unsigned long address, struct pt_regs *regs) \
{ \
    siginfo_t info;\
    info.si_signo = signr;\
    info.si_errno = 0;\
    info.si_code = sicode;\
    info.si_addr = (void *)address;\
    return do_fatal_exception(cause,str,regs,&info);\
}


#ifdef CONFIG_ARC_MISALIGNED_ACCESS

//#define DBG_MISALIGNED_FIXUP
#ifdef DBG_MISALIGNED_FIXUP							
#define DBG_MISALIGNED_FIXUP_BUFSIZE 	1024
#define DBG_BUF(args...) 								\
	if ((__dbg_p - __debug_buf) < DBG_MISALIGNED_FIXUP_BUFSIZE - 80) { 		\
		__dbg_p += sprintf(__dbg_p, "%s %d: ", __FUNCTION__, __LINE__);		\
		__dbg_p += sprintf(__dbg_p, args);					\
	}
#define DBG(args...) DBG_BUF(args)

#else

#define DBG_PRINT(args...) do {						\
	printk(KERN_INFO "%s %d: ", __FUNCTION__, __LINE__);		\
	printk(args);							\
} while (0)

//#define DBG(args...) DBG_PRINT(args)
#define DBG(args...)

#endif



#define INST16_OPCODE_START	0xc
#define REG_LIMM	62

#define __get8_unaligned_check(val,addr,err)				\
	__asm__(							\
			"1:	ldb.ab	%1, [%2, 1]\n"			\
			"2:\n"						\
			"	.section .fixup,\"ax\"\n"		\
			"	.align	4\n"				\
			"3:	mov	%0, 1\n"			\
			"	b	2b\n"				\
			"	.previous\n"				\
			"	.section __ex_table,\"a\"\n"		\
			"	.align	4\n"				\
			"	.long	1b, 3b\n"			\
			"	.previous\n"				\
			: "=r" (err), "=&r" (val), "=r" (addr)		\
			: "0" (err), "2" (addr))

#define get16_unaligned_check(val,addr)					\
	do {								\
		unsigned int err = 0, v, a = addr;			\
		__get8_unaligned_check(v,a,err);			\
		val =  v ;						\
		__get8_unaligned_check(v,a,err);			\
		val |= v << 8;						\
		if (err)						\
		goto fault;					\
	} while (0)

#define get32_unaligned_check(val,addr)					\
	do {								\
		unsigned int err = 0, v, a = addr;			\
		__get8_unaligned_check(v,a,err);			\
		val =  v << 0;						\
		__get8_unaligned_check(v,a,err);			\
		val |= v << 8;						\
		__get8_unaligned_check(v,a,err);			\
		val |= v << 16;						\
		__get8_unaligned_check(v,a,err);			\
		val |= v << 24;						\
		if (err)						\
		goto fault;						\
	} while (0)

#define put16_unaligned_check(val,addr)					\
	do {								\
		unsigned int err = 0, v = val, a = addr;		\
		__asm__( 						\
				"1:	stb.ab	%1, [%2, 1]\n"		\
				"	lsr %1, %1, 8\n"		\
				"2:	stb	%1, [%2]\n"		\
				"3:\n"					\
				"	.section .fixup,\"ax\"\n"	\
				"	.align	4\n"			\
				"4:	mov	%0, 1\n"		\
				"	b	3b\n"			\
				"	.previous\n"			\
				"	.section __ex_table,\"a\"\n"	\
				"	.align	4\n"			\
				"	.long	1b, 4b\n"		\
				"	.long	2b, 4b\n"		\
				"	.previous\n"			\
				: "=r" (err), "=&r" (v), "=&r" (a)	\
				: "0" (err), "1" (v), "2" (a));		\
		if (err)						\
		goto fault;						\
	} while (0)

#define put32_unaligned_check(val,addr)					\
	do {								\
		unsigned int err = 0, v = val, a = addr;		\
		__asm__( 						\
				"1:	stb.ab	%1, [%2, 1]\n"		\
				"	lsr %1, %1, 8\n"		\
				"2:	stb.ab	%1, [%2, 1]\n"		\
				"	lsr %1, %1, 8\n"		\
				"3:	stb.ab	%1, [%2, 1]\n"		\
				"	lsr %1, %1, 8\n"		\
				"4:	stb	%1, [%2]\n"		\
				"5:\n"					\
				"	.section .fixup,\"ax\"\n"	\
				"	.align	4\n"			\
				"6:	mov	%0, 1\n"		\
				"	b	5b\n"			\
				"	.previous\n"			\
				"	.section __ex_table,\"a\"\n"	\
				"	.align	4\n"			\
				"	.long	1b, 6b\n"		\
				"	.long	2b, 6b\n"		\
				"	.long	3b, 6b\n"		\
				"	.long	4b, 6b\n"		\
				"	.previous\n"			\
		: "=r" (err), "=&r" (v), "=&r" (a)			\
		: "0" (err), "1" (v), "2" (a));				\
		if (err)						\
		goto fault;						\
	} while (0)

/* 16 bit instructions use the registers r0 r1 r2 r3 r12 r13 r14 r15. */
/*   e.g. if reg from the instruction = 5, we want r13 */
static inline unsigned long fix_reg16(const unsigned int reg) 
{
	if (reg & 0x04)
		return reg | 0x08;
	return reg;
}

static int get_reg(unsigned int reg, struct pt_regs *regs, struct callee_regs *cregs, long *val, bool inst_16)
{
	long *p;

	if (inst_16)
		reg = fix_reg16(reg);

	if (reg <= 12) {
		p = &regs->r0;
		*val = p[-reg];
		return 0;
	}

	if (reg <= 25) {
		p = &cregs->r13;
		*val = p[13-reg];
		return 0;
	}

	if (reg == 26) {
		*val = regs->r26;
		return 0;
	}

	if (reg == 27) {
		*val = regs->fp;
		return 0;
	}

	if (reg == 28) {
		*val = regs->sp;
		return 0;
	}

	if (reg == 31) {
		*val = regs->blink;
		return 0;
	}

	return 1;
}

static int set_reg(unsigned int reg, long val, struct pt_regs *regs, struct callee_regs *cregs, bool inst_16)
{
	long *p;

	if (inst_16)
		reg = fix_reg16(reg);

	if (reg <= 12) {
		p = &regs->r0;
		p[-reg] = val;
		return 0;
	}

	if (reg <= 25) {
		p = &cregs->r13;
		p[13-reg] = val;
		return 0;
	}

	if (reg == 26) {
		regs->r26 = val;
		return 0;
	}

	if (reg == 27) {
		regs->fp = val;
		return 0;
	}

	if (reg == 28) {
		regs->sp = val;
		return 0;
	}

	if (reg == 31) {
		regs->blink = val;
		return 0;
	}

	return 1;
}

#define get_limm(x) 								\
	do {									\
		if (usermode) { 						\
			if (copy_from_user(&x, (u32 *)(instrptr + 4), 4)) 	\
			goto fault;						\
		} else { 							\
			x = *((unsigned*)(instrptr + 4)); 			\
		} 								\
		x = ((x & 0xffff) << 16) | (x >> 16);				\
	} while(0)

/* accounting for unaligned accesses */
struct unaligned_access_accounting unaligned_access_stats = {0};
EXPORT_SYMBOL(unaligned_access_stats);

static int misaligned_fixup(const unsigned long address,
		struct pt_regs *regs, const unsigned long cause, struct callee_regs *cregs)
{
	unsigned long instrptr;
	unsigned long instr, instr_lo;
	bool inst_16 = true;
	long src1, src2, src3 = 0, dest = 0, val;
	unsigned zz, aa, write, x, pref, limm, di, wb_reg = 0;

#ifdef DBG_MISALIGNED_FIXUP
	char __debug_buf[DBG_MISALIGNED_FIXUP_BUFSIZE] = {0};
	char *__dbg_p = __debug_buf;
	src1 = 0, src2 = 0, src3 = 0, dest = 0, val = 0;
	zz = 0, aa = 0, write = 0, x = 0, pref = 0, limm = 0, di = 0, wb_reg = 0;
#endif

	struct unaligned_access_accounting* stats = &unaligned_access_stats;

	/* set cregs for fault printout */
	current->thread.callee_reg = (unsigned long)cregs;

	const int usermode = user_mode(regs);

	di = 0;
	aa = 0;
	write = 0;
	x = 0;
	pref = 0;
	limm = 0;
	instr = 0;

	instrptr = instruction_pointer(regs);

	if (usermode) {
		if (get_user(instr, (u16 *)(instrptr & ~1))) {
			goto fault;
		}
	} else {
		instr = *((u16 *)(instrptr & ~1)); 
	}

	inst_16 = (((instr >> 11) & 0x1f) >= INST16_OPCODE_START); 
	if (!inst_16) {
		if (usermode) {
			if (get_user(instr_lo, (u16 *)((instrptr & ~1) + 2))) {
				goto fault;
			}
		} else {
			instr_lo = *((u16 *)((instrptr & ~1) + 2));
		}
		instr = (instr << 16) | instr_lo;
	}

	DBG(KERN_ERR "alignment fix: 0x%0*lx at [<0x%08lx>] usermode: %d delay slot: %d\n",
			inst_16 ? 4 : 8,
			instr, instrptr, usermode, delay_mode(regs));

	/* rate limited complaint about faulting userland programs */
	if (usermode && printk_ratelimit()) 
		printk(KERN_ERR "Misaligned access trap for user program '%s' (parent '%s'): instr: 0x%0*lx at [<0x%08lx>]\n",
				current->comm, current->parent ? current->parent->comm : "<none>",
				inst_16 ? 4 : 8, instr, instrptr);

	/* accounting */
	if (usermode) {
		stats->user++;
	} else {	
		stats->kernel_iptr[stats->kernel % UNALIGNED_INSTPTR_BUFSIZE] = instrptr;
		stats->kernel++;
	}

	if (inst_16)
		stats->inst_16++;
	else 
		stats->inst_32++;

	/* instruction decoding */
	if (inst_16) {
		/* 16 bit instruction */
		const int decode = (instr >> 11) & 0x1f;
		DBG("decode=0x%02x\n", decode);
		stats->inst[decode % UNALIGNED_INST_OPCODES]++;
		switch (decode) {
			case 0x0c:	/* LD_S|LDB_S|LDW_S a,[b,c] */
				zz = (instr >> 3) & 3;
				src1 = (instr >> 8) & 7;
				src2 = (instr >> 5) & 7;
				dest = instr & 7;
				if ((get_reg(src1, regs, cregs, &src1, inst_16)) ||
						(get_reg(src2, regs, cregs, &src2, inst_16)))
					goto bad;
				break;

			case 0x10:  /* LD_S  c,[b,u7] */
				zz = 0;
				src1 = (instr >> 8) & 7;
				src2 = (instr & 0x1f) << 2;
				dest = (instr >> 5) & 7;
				//DBG("0x10: src1 %ld src2 %ld dest %ld\n", src1, src2, dest);
				if (get_reg(src1, regs, cregs, &src1, inst_16))
					goto bad;
				//DBG("0x10: src1 0x%08lx\n", src1);
				break;

			case 0x11:  /* LDB_S c,[b,u5] */
				zz = 1;
				break;

			case 0x12:  /* LDW_S c,[b,u6] */
				zz = 2;
				src1 = (instr >> 8) & 7;
				src2 = (instr & 0x1f) << 1;
				dest = (instr >> 5) & 7;
				if (get_reg(src1, regs, cregs, &src1, inst_16))
					goto bad;
				break;

			case 0x13:  /* LDW_S.X c,[b,u6] */
				x = 1;
				zz = 2;
				src1 = (instr >> 8) & 7;
				src2 = (instr & 0x1f) << 1;
				dest = (instr >> 5) & 7;
				if (get_reg(src1, regs, cregs, &src1, inst_16))
					goto bad;
				break;

			case 0x14: /* ST_S c,[b,u7] */
				write = 1;
				zz = 0;
				src1 = (instr >> 5) & 7;
				src2 = (instr >> 8) & 7;
				src3 = (instr & 0x1f) << 2;
				if ((get_reg(src1, regs, cregs, &src1, inst_16)) ||
						(get_reg(src2, regs, cregs, &src2, inst_16)))
					goto bad;
				break;				

			case 0x15: /* STB_S c,[b,u6] */
				zz = 1;	/* STB should not have unaligned exception */
				break;				

			case 0x16: /* STW_S c,[b,u6] */
				write = 1;
				zz = 2;
				src1 = (instr >> 5) & 7;
				src2 = (instr >> 8) & 7;
				src3 = (instr & 0x1f) << 1;
				if ((get_reg(src1, regs, cregs, &src1, inst_16)) ||
						(get_reg(src2, regs, cregs, &src2, inst_16)))
					goto bad;
				break;

			case 0x18:  /* LD_S|LDB_S b,[sp,u7], ST_S|STB_S b,[sp,u7] */
				write = (instr >> 6) & 1;
				zz = (instr >> 5) & 1;
				if (zz == 1)
					break;
				if (write == 0) {
					src1 = 28;
					src2 = (instr & 0x1f) << 2;
					dest = (instr >> 8) & 7;
					if (get_reg(src1, regs, cregs, &src1, inst_16))
						goto bad;

				} else {
					src1 = (instr >> 8) & 7;
					src2 = 28;
					src3 = (instr & 0x1f) << 2;
					if ((get_reg(src1, regs, cregs, &src1, inst_16)) ||
							(get_reg(src2, regs, cregs, &src2, inst_16)))
						goto bad;
				}
				break;

			case 0x19:  /* LD_S|LDB_S|LDW_S r0,[gp,s11/s9/s10] */
				zz = (instr >> 9) & 3;
				src1 = 26;
				src2 = instr & 0x1ff;
				if (zz == 0)
					src2 = (src2 << 23) >> 21;	/* s11 */
				else if (zz == 1)
					break;
				else if (zz == 2)
					src2 = (src2 << 23) >> 22;	/* s10 */
				dest = 0;
				if (get_reg(src1, regs, cregs, &src1, inst_16))
					goto bad;
				break;

			case 0x1a:  /* LD_S b,[pcl,u10] */ 
				zz = 0;
				src1 = regs->ret;
				src2 = (instr & 0xff) << 2;
				dest = (instr >> 8) & 7;
				break;

			default:
				goto bad;
		}

	} else {
		/* 32 bit instruction */
		const int decode = (instr >> 27) & 0x1f;
		DBG("decode=0x%02x\n", decode);	
		stats->inst[decode % UNALIGNED_INST_OPCODES]++;
		switch (decode) {
			case 0x02:  /* LD<zz> a,[b,s9] */
				di = (instr >> 11) & 1;
				if (di)
					break;
				x = (instr >> 6) & 1;
				zz = (instr >> 7) & 3;
				aa = (instr >> 9) & 3;
				wb_reg = (((instr >> 12) & 7) << 3) | ((instr >> 24) & 7);
				if (wb_reg == REG_LIMM) {
					limm = 1;
					aa = 0;
					get_limm(src1);
				} else {
					if (get_reg(wb_reg, regs, cregs, &src1, inst_16))
						goto bad;
				}
				src2 = ((instr >> 16) & 0xff) | (((instr >> 15) & 1) << 8);
				src2 = (src2 << 23) >> 23;
				dest = instr & 0x3f;
				if (dest == REG_LIMM) {
					pref = 1;
					break;
				}
				break;

			case 0x03:  /* ST<zz> c,[b,s9] */
				write = 1;
				di = (instr >> 5) & 1;
				if (di)
					break;
				aa = (instr >> 3) & 3;
				zz = (instr >> 1) & 3;
				src1 = (instr >> 6) & 0x3f;
				//DBG("32 bit st: src1 reg %ld\n", src1);
				if (src1 == REG_LIMM) {
					limm = 1;
					get_limm(src1);
				} else {
					if (get_reg(src1, regs, cregs, &src1, inst_16))
						goto bad;
				}
				//DBG("32 bit st: src1 val %ld\n", src1);
				wb_reg = (((instr >> 12) & 7) << 3) | ((instr >> 24) & 7);
				if (wb_reg == REG_LIMM) {
					aa = 0;
					limm = 1;
					get_limm(src2);
				} else {
					if (get_reg(wb_reg, regs, cregs, &src2, inst_16))
						goto bad;
				}
				src3 = ((instr >> 16) & 0xff) | (((instr >> 15) & 1) << 8);
				src3 = (src3 << 23) >> 23;
				break;

			case 0x04:  /* LD<zz> a,[b,c] */
				di = (instr >> 15) & 1;
				if (di)
					break;
				x = (instr >> 16) & 1;
				zz = (instr >> 17) & 3;
				aa = (instr >> 22) & 3;
				wb_reg = (((instr >> 12) & 7) << 3) | ((instr >> 24) & 7);
				if (wb_reg == REG_LIMM) {
					limm = 1;
					get_limm(src1);
				} else {
					if (get_reg(wb_reg, regs, cregs, &src1, inst_16))
						goto bad;
				}
				src2 = (instr >> 6) & 0x3f;
				if (src2 == REG_LIMM) {
					limm = 1;
					get_limm(src2);
				} else {
					if (get_reg(src2, regs, cregs, &src2, inst_16))
						goto bad;
				}
				dest = instr & 0x3f;
				if (dest == REG_LIMM)
					pref = 1;
				break;

			default:
				goto bad;
		}
	}

	/* ldb/stb should not have unaligned exception */
	if ((zz == 1) || (di))
		goto bad;

	DBG("write %d aa %d zz %d pref %d wb_reg %d src1 0x%lx src2 0x%lx src3 %ld dest %ld val 0x%08lx\n",
			write, aa, zz, pref, wb_reg, src1, src2, src3, dest, val);

	/* ld instructions */
	if (write == 0) {
		/* register write back */
		if ((aa == 1) || (aa == 2)) {
			if (set_reg(wb_reg, src1 + src2, regs, cregs, inst_16))
				goto bad;
			if (aa == 2)
				src2 = 0;
		}

		unsigned long faulting_address = src1 + src2;
		if (aa == 3 && zz == 2)
			faulting_address = src1 + (src2 << 1);
		else if (aa == 3 && zz == 0)
			faulting_address = src1 + (src2 << 2);

		if (faulting_address != address)
			printk(KERN_ERR "%s ld address 0x%lx does not match those of operands: s1/s2: 0x%lx 0x%lx\n", 
					__FUNCTION__, address, src1, src2);

		if (zz == 0)
			get32_unaligned_check(val, faulting_address);
		else {
			get16_unaligned_check(val, faulting_address);

			if (x)
				val = (val << 16) >> 16;
		}

		if (pref == 0)
			if (set_reg(dest, val, regs, cregs, inst_16))
				goto bad;

		/* store instructions */
	} else {
		/* register write back */
		if ((aa == 1) || (aa == 2)) {
			if (set_reg(wb_reg, src2 + src3, regs, cregs, inst_16))
				goto bad;
			if (aa == 2)
				src3 = 0;
		}

		unsigned long faulting_address = src2 + src3;
		if (aa == 3 && zz == 2)
			faulting_address = src2 + (src3 << 1);
		else if (aa == 3 && zz == 0)
			faulting_address = src2 + (src3 << 2);

		if (faulting_address != address)
			printk(KERN_ERR "%s st address 0x%lx does not match those of operands: s2/s3: 0x%lx 0x%lx\n", 
					__FUNCTION__, address, src2, src3);

		/* write fix-up */
		if (zz == 0)
			put32_unaligned_check(src1, faulting_address);
		else
			put16_unaligned_check(src1, faulting_address);
	}

	/* accounting */
	if (write) 
		stats->write++;
	else
		stats->read++;
	if (zz == 0)
		stats->word++;
	else 
		stats->half++;

	if (delay_mode(regs)) {
		regs->ret = regs->bta & ~0x1;
		regs->status32 &= ~STATUS_DE_MASK;
	} else {
		regs->ret += (inst_16) ? 2 :
			(limm) ? 8 : 4;
	}

	return 0;

bad:
#ifdef DBG_MISALIGNED_FIXUP
	printk(KERN_ERR "%s\n", __debug_buf);
#endif
	/*
	 * Oops, we didn't handle the instruction.
	 */
	printk(KERN_ERR "Alignment trap: not handling instruction "
			"0x%0*lx at [<0x%08lx>] usermode: %d\n",
			inst_16 ? 4 : 8,
			instr, instrptr, usermode);

	stats->skipped++;

	return 1;

fault:
#ifdef DBG_MISALIGNED_FIXUP
	printk(KERN_ERR "%s\n", __debug_buf);
#endif
	printk(KERN_ERR "Alignment trap: fault in fix-up  "
			"0x%0*lx at [<0x%08lx>] usermode: %d\n",
			inst_16 ? 4 : 8,
			instr, instrptr, usermode);

	stats->skipped++;

	return 1;
}

int do_misaligned_access(unsigned long cause, unsigned long address,
             struct pt_regs *regs, struct callee_regs *cregs)
{
	if (misaligned_fixup(address, regs, cause, cregs) != 0) {
	    siginfo_t info;

        info.si_signo = SIGSEGV;
        info.si_errno = 0;
        info.si_code = SEGV_ACCERR;
        info.si_addr = (void *)address;
        return do_fatal_exception(cause,"Misaligned Access", regs,&info);
    }
    return 0;
}

#else
DO_ERROR_INFO(SIGSEGV, "Misaligned Access", do_misaligned_access, SEGV_ACCERR)
#endif

DO_ERROR_INFO(SIGILL, "Privileged Operation/Disabled Extension/Actionpoint Hit",
                do_privilege_fault, ILL_PRVOPC)
DO_ERROR_INFO(SIGILL, "Extenion Instruction Exception",
                do_extension_fault, ILL_ILLOPC)
DO_ERROR_INFO(SIGILL, "Illegal Instruction/Illegal Instruction Sequence",
                do_instruction_error, ILL_ILLOPC)
DO_ERROR_INFO(SIGBUS, "Access to Invalid Memory", do_memory_error, BUS_ADRERR)
DO_ERROR_INFO(SIGTRAP, "Breakpoint Set", do_trap_is_brkpt, TRAP_BRKPT)


void do_machine_check_fault( unsigned long cause, unsigned long address,
    struct pt_regs *regs)
{
    die("Machine Check Exception",regs, address, cause);
}

void __init trap_init(void)
{
    return;
}


asmlinkage void do_trap_is_kprobe(unsigned long cause, unsigned long address,
                                                            struct pt_regs *regs)
{
    notify_die(DIE_TRAP, "kprobe_trap", regs, address, cause, SIGTRAP);
}

asmlinkage void do_trap(unsigned long cause, unsigned long address,
                  struct pt_regs *regs)
{
    unsigned int param = cause & 0xff;

    switch(param)
    {
        case 1:
            do_trap_is_brkpt(cause, address, regs);
            break;

        case 2:
            do_trap_is_kprobe(param, address, regs);
            break;

        default:
            break;
    }
}

asmlinkage void do_insterror_or_kprobe(unsigned long cause,
    unsigned long address, struct pt_regs *regs)
{
    /* Check if this exception is caused by kprobes */
    if(notify_die(DIE_IERR, "kprobe_ierr", regs, address,
                    cause, SIGILL) == NOTIFY_STOP)
        return;

    do_instruction_error(cause, address, regs);
}


