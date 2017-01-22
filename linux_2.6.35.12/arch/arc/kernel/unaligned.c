/*
 * Copyright (C) 2011-2012 Synopsys (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * vineetg : May 2011
 *  -Adapted (from .26 to .35)
 *  -original contribution by Tim.yao@amlogic.com
 *
 */

#include <linux/types.h>
#include <asm/ptrace.h>
#include <asm/uaccess.h>

#define INST16_OPCODE_START	0xc
#define REG_LIMM	62

#define __get8_unaligned_check(val,addr,err)	\
	__asm__(					\
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

#define get16_unaligned_check(val,addr)			\
	do {							\
		unsigned int err = 0, v, a = addr;		\
		__get8_unaligned_check(v,a,err);		\
		val =  v ;			\
		__get8_unaligned_check(v,a,err);		\
		val |= v << 8;			\
		if (err)					\
			goto fault;				\
	} while (0)

#define get32_unaligned_check(val,addr)			\
	do {							\
		unsigned int err = 0, v, a = addr;		\
		__get8_unaligned_check(v,a,err);		\
		val =  v << 0;			\
		__get8_unaligned_check(v,a,err);		\
		val |= v << 8;			\
		__get8_unaligned_check(v,a,err);		\
		val |= v << 16;			\
		__get8_unaligned_check(v,a,err);		\
		val |= v << 24;			\
		if (err)					\
			goto fault;				\
	} while (0)

#define put16_unaligned_check(val,addr)			\
	do {							\
		unsigned int err = 0, v = val, a = addr;	\
		__asm__( 				\
		"1:	stb.ab	%1, [%2, 1]\n"			\
		"	lsr %1, %1, 8\n"		\
		"2:	stb	%1, [%2]\n"			\
		"3:\n"						\
		"	.section .fixup,\"ax\"\n"		\
		"	.align	4\n"				\
		"4:	mov	%0, 1\n"			\
		"	b	3b\n"				\
		"	.previous\n"				\
		"	.section __ex_table,\"a\"\n"		\
		"	.align	4\n"				\
		"	.long	1b, 4b\n"			\
		"	.long	2b, 4b\n"			\
		"	.previous\n"				\
		: "=r" (err), "=&r" (v), "=&r" (a)		\
		: "0" (err), "1" (v), "2" (a));			\
		if (err)					\
			goto fault;				\
	} while (0)

#define put32_unaligned_check(val,addr)			\
	do {							\
		unsigned int err = 0, v = val, a = addr;	\
		__asm__( 				\
		"1:	stb.ab	%1, [%2, 1]\n"			\
		"	lsr %1, %1, 8\n"		\
		"2:	stb.ab	%1, [%2, 1]\n"			\
		"	lsr %1, %1, 8\n"		\
		"3:	stb.ab	%1, [%2, 1]\n"			\
		"	lsr %1, %1, 8\n"		\
		"4:	stb	%1, [%2]\n"			\
		"5:\n"						\
		"	.section .fixup,\"ax\"\n"		\
		"	.align	4\n"				\
		"6:	mov	%0, 1\n"			\
		"	b	5b\n"				\
		"	.previous\n"				\
		"	.section __ex_table,\"a\"\n"		\
		"	.align	4\n"				\
		"	.long	1b, 6b\n"			\
		"	.long	2b, 6b\n"			\
		"	.long	3b, 6b\n"			\
		"	.long	4b, 6b\n"			\
		"	.previous\n"				\
		: "=r" (err), "=&r" (v), "=&r" (a)		\
		: "0" (err), "1" (v), "2" (a));			\
		if (err)					\
			goto fault;				\
	} while (0)

static int get_reg(unsigned int reg, struct pt_regs *regs, struct callee_regs *cregs, long *val)
{
	long *p;

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

static int set_reg(unsigned int reg, long val, struct pt_regs *regs, struct callee_regs *cregs)
{
	long *p;

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

#define get_limm(x) ({                                  \
	if (copy_from_user(&x, (u32 *)(instrptr + 4), 4))   \
		goto fault;                                     \
	x = ((x & 0xffff) << 16) | (x >> 16);               \
    x;                                                  \
})

int misaligned_fixup(unsigned long address, struct pt_regs *regs,
                unsigned long cause,  struct callee_regs *cregs)
{
	unsigned long instrptr;
	unsigned long instr, instr_lo;
	bool inst_16 = true;
	long src1, src2, src3 = 0, dest = 0, val;
	unsigned zz, aa, write, x, pref, limm, di, wb_reg = 0;

	/* handle user mode only */
	if (!user_mode(regs))
		return 1;

	di = 0;
	aa = 0;
	write = 0;
	x = 0;
	pref = 0;
	limm = 0;
	instr = 0;

	instrptr = instruction_pointer(regs);

	if (get_user(instr, (u16 *)(instrptr & ~1)))
		goto fault;

	inst_16 = (((instr >> 11) & 0x1f) >= INST16_OPCODE_START);
	if (!inst_16) {
		if (get_user(instr_lo, (u16 *)((instrptr & ~1) + 2)))
			goto fault;

		instr = (instr << 16) | instr_lo;
	}

	/* instruction decoding */
	if (inst_16) {
		/* 16 bit instruction */
		switch ((instr >> 11) & 0x1f) {
			case 0x0c:	/* LD_S|LDB_S|LDW_S a,[b,c] */
				zz = (instr >> 3) & 3;
				src1 = (instr >> 8) & 7;
				src2 = (instr >> 5) & 7;
				dest = instr & 7;
				if ((get_reg(src1, regs, cregs, &src1)) ||
					(get_reg(src2, regs, cregs, &src2)))
					goto bad;
				break;

			case 0x10:  /* LD_S  c,[b,u7] */
				zz = 0;
				src1 = (instr >> 8) & 7;
				src2 = (instr & 0x1f) << 2;
				dest = (instr >> 5) & 7;
				if (get_reg(src1, regs, cregs, &src1))
					goto bad;
				break;

			case 0x11:  /* LDB_S c,[b,u5] */
				zz = 1;
				break;

			case 0x12:  /* LDW_S c,[b,u6] */
				zz = 2;
				src1 = (instr >> 8) & 7;
				src2 = (instr & 0x1f) << 1;
				dest = (instr >> 5) & 7;
				if (get_reg(src1, regs, cregs, &src1))
					goto bad;
				break;

			case 0x13:  /* LDW_S.X c,[b,u6] */
				x = 1;
				zz = 2;
				src1 = (instr >> 8) & 7;
				src2 = (instr & 0x1f) << 1;
				dest = (instr >> 5) & 7;
				if (get_reg(src1, regs, cregs, &src1))
					goto bad;
				break;

			case 0x14: /* ST_S c,[b,u7] */
				write = 1;
				zz = 0;
				src1 = (instr >> 5) & 7;
				src2 = (instr >> 8) & 7;
				src3 = (instr & 0x1f) << 2;
				if ((get_reg(src1, regs, cregs, &src1)) ||
					(get_reg(src2, regs, cregs, &src2)))
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
				if ((get_reg(src1, regs, cregs, &src1)) ||
					(get_reg(src2, regs, cregs, &src2)))
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
					if (get_reg(src1, regs, cregs, &src1))
						goto bad;

				} else {
					src1 = (instr >> 8) & 7;
					src2 = 28;
					src3 = (instr & 0x1f) << 2;
					if ((get_reg(src1, regs, cregs, &src1)) ||
						(get_reg(src2, regs, cregs, &src2)))
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
				if (get_reg(src1, regs, cregs, &src1))
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
		switch ((instr >> 27) & 0x1f) {
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
					if (get_reg(wb_reg, regs, cregs, &src1))
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
				src1 = (instr >> 6) & 3;
				if (src1 == REG_LIMM) {
					limm = 1;
					get_limm(src1);
				} else {
					if (get_reg(src1, regs, cregs, &src1))
						goto bad;
				}
				wb_reg = (((instr >> 12) & 7) << 3) | ((instr >> 24) & 7);
				if (wb_reg == REG_LIMM) {
					aa = 0;
					limm = 1;
					get_limm(src2);
				} else {
					if (get_reg(wb_reg, regs, cregs, &src2))
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
					if (get_reg(src1, regs, cregs, &src1))
						goto bad;
				}
				src2 = (instr >> 6) & 0x3f;
				if (src2 == REG_LIMM) {
					limm = 1;
					get_limm(src2);
				} else {
					if (get_reg(src2, regs, cregs, &src2))
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

	if (write == 0) {
		/* register write back */
		if ((aa == 1) || (aa == 2)) {
			if (set_reg(wb_reg, src1 + src2, regs, cregs))
				goto bad;
			if (aa == 2)
				src2 = 0;
		}

		if (zz == 0)
			get32_unaligned_check(val, src1 + src2);
		else {
			get16_unaligned_check(val, src1 + src2);

			if (x)
				val = (val << 16) >> 16;
		}

		if (pref == 0)
			if (set_reg(dest, val, regs, cregs))
				goto bad;

	} else {
		/* register write back */
		if ((aa == 1) || (aa == 2)) {
			if (set_reg(wb_reg, src2 + src3, regs, cregs))
				goto bad;
			if (aa == 3)
				src3 = 0;
		}
		else if (aa == 3) {
			if (zz == 2) {
				if (set_reg(wb_reg, src2 + (src3 << 1), regs, cregs))
					goto bad;
			}
			else if (zz == 0) {
				if (set_reg(wb_reg, src2 + (src3 << 2), regs, cregs))
					goto bad;
			}
			else
				goto bad;
		}

		/* write fix-up */
		if (zz == 0)
			put32_unaligned_check(src1, src2 + src3);
		else
			put16_unaligned_check(src1, src2 + src3);
	}

	if (delay_mode(regs)) {
		regs->ret = regs->bta;
		regs->status32 &= ~STATUS_DE_MASK;
	} else
		regs->ret += (inst_16) ? 2 :
					 (limm) ? 8 : 4;

	return 0;

bad:
	/*
	 * Oops, we didn't handle the instruction.
	 */
	printk(KERN_ERR "Alignment trap: not handling instruction "
		"%0*lx at [<%08lx>]\n",
		inst_16 ? 4 : 8,
		instr, instrptr);

	return 1;

fault:
	printk(KERN_ERR "Alignment trap: fault in fix-up "
		"%0*lx at [<%08lx>]\n",
		inst_16 ? 4 : 8,
		instr, instrptr);

	return 1;
}
