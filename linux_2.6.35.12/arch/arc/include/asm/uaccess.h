/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * vineetg: Apr 2011 - access_ok( ) simplification
 *    -Copied from IA-64
 *    -No need to seperately check for kernel/user segments
 *    -With addr_limit now having the exact last valid addr, it can simply
 *     be add < get_fs()
 *
 * vineetg: June 2010
 *    -__clear_user( ) called multiple times during elf load was byte loop
 *    converted to do as much word clear as possible.
 *
 * vineetg: Dec 2009
 *    -Hand crafted constant propagation for "constant" copy sizes
 *    -stock kernel shrunk by 33K at -O3
 *
 * vineetg: Sept 2009
 *    -Added option to (UN)inline copy_(to|from)_user to reduce code sz
 *    -kernel shrunk by 200K even at -O3
 *    -Enabled when doing -Os
 *
 * Amit Bhor, Sameer Dhavale: Codito Technologies 2004
 */

#ifndef _ASM_ARC_UACCESS_H
#define _ASM_ARC_UACCESS_H

#include <linux/sched.h>
#include <asm/errno.h>
#include <linux/string.h>   /* for generic string functions */
#include <asm/mem.h>

#define VERIFY_READ 0
#define VERIFY_WRITE 1
//#define ARC_HIGH_LATENCY_MEMORY

/*
 * The fs value determines whether argument validity checking should be
 * performed or not.  If get_fs() == USER_DS, checking is performed, with
 * get_fs() == KERNEL_DS, checking is bypassed.
 *
 * For historical reasons, these macros are grossly misnamed.
 */

#define MAKE_MM_SEG(s)  ((mm_segment_t) { (s) })

/* addr_limit is the maximum accessible address for the task. we misuse
 * the KERNEL_DS and USER_DS values to both assign and compare the
 * addr_limit values through the equally misnamed get/set_fs macros.
 * (see above)
 */

#define KERNEL_DS   MAKE_MM_SEG(0)
#define USER_DS     MAKE_MM_SEG(TASK_SIZE)

#define get_ds()    (KERNEL_DS)

/* Sameer : addr_limit is now member of thread_info struct and NOT
            of task_struct as was the case in the 2.4 days */
#define get_fs()    (current_thread_info()->addr_limit)
#define set_fs(x)   (current_thread_info()->addr_limit = (x))

#define segment_eq(a,b) ((a) == (b))

#define __kernel_ok (segment_eq(get_fs(), KERNEL_DS))
#define __user_ok(addr,size) (\
                ((size) <= TASK_SIZE) && \
                (((addr)+(size)) <= get_fs()) \
                )
#define __access_ok(addr,size) (unlikely(__kernel_ok) || likely(__user_ok((addr),(size))))
#define access_ok(type,addr,size) __access_ok((unsigned long)(addr),(size))

static inline int verify_area(int type, const void * addr, unsigned long size)
{
    return access_ok(type,addr,size) ? 0 : -EFAULT;
}

/*
 * The exception table consists of pairs of addresses: the first is the
 * address of an instruction that is allowed to fault, and the second is
 * the address at which the program should continue.  No registers are
 * modified, so it is entirely up to the continuation code to figure out
 * what to do.
 *
 * All the routines below use bits of fixup code that are out of line
 * with the main instruction path.  This means when everything is well,
 * we don't even have to jump over them.  Further, they do not intrude
 * on our cache or tlb entries.
 */

struct exception_table_entry
{
    unsigned long insn, nextinsn;
};

/* Returns 0 if exception not found and fixup otherwise.  */
extern unsigned long search_exception_table(unsigned long);

/*
 * These are the main single-value transfer routines.  They automatically
 * use the right size if we just have the right pointer type.
 *
 * This gets kind of ugly. We want to return _two_ values in "get_user()"
 * and yet we don't want to do any pointers, because that is too much
 * of a performance impact. Thus we have a few rather ugly macros here,
 * and hide all the uglyness from the user.
 *
 * The "__xxx" versions of the user access functions are versions that
 * do not verify the address space, that must have been done previously
 * with a separate "access_ok()" call (this is used when we do multiple
 * accesses to the same area of user memory).
 *
 * As we use the same address space for kernel and user data on
 * ARC, we can just do these as direct assignments.  (Of course, the
 * exception handling means that it's no longer "just"...)
 */

#define get_user(x,p)       __get_user_check((x),(p),sizeof(*(p)))
#define __get_user(x,p)     __get_user_nocheck((x),(p),sizeof(*(p)))

#define put_user(x,p)       __put_user_check((__typeof(*(p)))(x),(p),sizeof(*(p)))
#define __put_user(x,p)     __put_user_nocheck((__typeof(*(p)))(x),(p),sizeof(*(p)))

#define __get_user_check(x,ptr,size)                            \
({                                                              \
    long __gu_err = -EFAULT, __gu_val = 0;                      \
    const __typeof__(*(ptr)) *__gu_addr = (ptr);                \
    if (access_ok(VERIFY_READ,__gu_addr,size)) {                \
        __gu_err = 0;                                           \
        __get_user_size(__gu_val,__gu_addr,(size),__gu_err);    \
    }                                                           \
    (x) = (__typeof__(*(ptr)))__gu_val;                         \
    __gu_err;                                                   \
})

#define __get_user_nocheck(x,ptr,size)                  \
({                                                      \
    long __gu_err = 0, __gu_val;                        \
    __get_user_size(__gu_val,(ptr),(size),__gu_err);    \
    (x) = (__typeof__(*(ptr)))__gu_val;                 \
    __gu_err;                                           \
})

#define __put_user_check(x,ptr,size)                    \
({                                                      \
    long __pu_err = -EFAULT;                            \
    __typeof__(*(ptr)) *__pu_addr = (ptr);              \
    if (access_ok(VERIFY_WRITE,__pu_addr,size)) {       \
        __pu_err = 0;                                   \
        __put_user_size((x),__pu_addr,(size),__pu_err); \
    }                                                   \
    __pu_err;                                           \
})

#define __put_user_nocheck(x,ptr,size)              \
({                                                  \
    long __pu_err = 0;                              \
    __typeof__(*(ptr)) *__pu_addr = (ptr);          \
    __put_user_size((x),__pu_addr,(size),__pu_err); \
    __pu_err;                                       \
})


extern long __get_user_bad(void);

#define __get_user_size(x,ptr,size,retval)              \
do {                                                    \
    switch (size) {                                     \
    case 1: __get_user_asm(x,ptr,retval,"ldb"); break;  \
    case 2: __get_user_asm(x,ptr,retval,"ldw"); break;  \
    case 4: __get_user_asm(x,ptr,retval,"ld");  break;  \
    case 8: __get_user_asm_64(x,ptr,retval);    break;  \
    default: (x) = __get_user_bad();                    \
    }                                                   \
} while (0)

// FIXME :: check if the "nop" is required
#define __get_user_asm(x,addr,err,op)       \
    __asm__ __volatile__(                   \
    "1: "op"    %1,[%2]\n"                  \
    "   mov %0, 0x0\n"                      \
    "2: ;nop\n"                              \
    "   .section .fixup, \"ax\"\n"          \
    "   .align 4\n"                         \
    "3: mov %0, %3\n"                       \
    "   j   2b\n"                           \
    "   .previous\n"                        \
    "   .section __ex_table, \"a\"\n"       \
    "   .align 4\n"                         \
    "   .word 1b,3b\n"                      \
    "   .previous\n"                        \
    : "=r" (err), "=&r" (x)                 \
    : "r" (addr), "i" (-EFAULT), "0" (err))

#define __get_user_asm_64(x,addr,err)       \
    __asm__ __volatile__(                   \
    "1: ld  %1,[%2]\n"                      \
    "2: ld  %R1,[%2, 4]\n"                  \
    "   mov %0, 0x0\n"                      \
    "3: ;nop\n"                              \
    "   .section .fixup, \"ax\"\n"          \
    "   .align 4\n"                         \
    "4: mov %0, %3\n"                       \
    "   mov %1, 0x0\n"                      \
    "   j   3b\n"                           \
    "   .previous\n"                        \
    "   .section __ex_table, \"a\"\n"       \
    "   .align 4\n"                         \
    "   .word 1b,4b\n"                      \
    "   .word 2b,4b\n"                      \
    "   .previous\n"                        \
    : "=r" (err), "=&r" (x)                 \
    : "r" (addr), "i" (-EFAULT), "0" (err))

extern long __put_user_bad(void);

#define __put_user_size(x,ptr,size,retval)              \
do {                                                    \
    switch (size) {                                     \
    case 1: __put_user_asm(x,ptr,retval,"stb"); break;  \
    case 2: __put_user_asm(x,ptr,retval,"stw"); break;  \
    case 4: __put_user_asm(x,ptr,retval,"st");  break;  \
    case 8: __put_user_asm_64(x,ptr,retval);    break;  \
    default: __put_user_bad();                          \
    }                                                   \
} while (0)

#define __put_user_asm(x,addr,err,op)       \
    __asm__ __volatile__(                   \
    "1: "op"    %1,[%2]\n"                  \
    "   mov %0, 0x0\n"                      \
    "2: ;nop\n"                              \
    "   .section .fixup, \"ax\"\n"          \
    "   .align 4\n"                         \
    "3: mov %0, %3\n"                       \
    "   j   2b\n"                           \
    "   .previous\n"                        \
    "   .section __ex_table, \"a\"\n"       \
    "   .align 4\n"                         \
    "   .word 1b,3b\n"                      \
    "   .previous\n"                        \
    : "=r" (err)                            \
    : "r" (x), "r" (addr),"i" (-EFAULT), "0" (err))

#define __put_user_asm_64(x,addr,err)   \
    __asm__ __volatile__(               \
    "1: st  %1,[%2]\n"                  \
    "2: st  %R1,[%2, 4]\n"              \
    "   mov %0, 0x0\n"                  \
    "3: ;nop\n"                          \
    "   .section .fixup, \"ax\"\n"      \
    "   .align 4\n"                     \
    "4: mov %0, %3\n"                   \
    "   mov %1, 0x0\n"                  \
    "   j   3b\n"                       \
    "   .previous\n"                    \
    "   .section __ex_table, \"a\"\n"   \
    "   .align 4\n"                     \
    "   .word 1b,4b\n"                  \
    "   .word 2b,4b\n"                  \
    "   .previous\n"                    \
    : "=r" (err)                        \
    : "r" (x), "r" (addr),"i" (-EFAULT), "0" (err))


extern unsigned long slowpath_copy_from_user(
     void *to, const void *from, unsigned long n);

static inline unsigned long
__copy_from_user_inline(void *to, const void *from, unsigned long n)
{
    long res=0;
    char val;
    unsigned long tmp1,tmp2,tmp3,tmp4;
    unsigned long orig_n = n;


    if (n == 0)
        return 0;

    if( ((unsigned long) to & 0x3) || ((unsigned long) from & 0x3))
    {
        // unaligned
        res = slowpath_copy_from_user(to, from, n);
    }
    else
    {
#ifdef ARC_HIGH_LATENCY_MEMORY
    __asm__ __volatile__ (
        "       pf      [%1,32]         \n"
        "       sync                    \n"
        "       mov     %0,%3           \n"
        "       lsr.f   lp_count, %3,5  \n"  // number of words
        "       lpnz    3f              \n"
        "1:     pf      [%2,32]         \n"
        "       nop_s                   \n"
        "10:    ld.ab   %5, [%2, 4]     \n"  // 0
        "       nop_s                   \n"
        "11:    ld.ab   %6, [%2, 4]     \n"  // 1
        "       nop_s                   \n"
        "12:    ld.ab   %7, [%2, 4]     \n"  // 2
        "       nop_s                   \n"
        "       st.ab   %5, [%1, 4]     \n"  // 0
        "       nop_s                   \n"
        "       st.ab   %6, [%1, 4]     \n"  // 1
        "       nop_s                   \n"
        "       st.ab   %7, [%1, 4]     \n"  // 2
        "       nop_s                   \n"
        "13:    ld.ab   %5, [%2, 4]     \n"  // 3
        "       nop_s                   \n"
        "14:    ld.ab   %6, [%2, 4]     \n"  // 4
        "       nop_s                   \n"
        "144:   pf      [%2,0]          \n"
        "       st.ab   %5, [%1, 4]     \n"  // 3
        "       nop_s                   \n"
        "       st.ab   %6, [%1, 4]     \n"  // 4
        "       nop_s                   \n"
        "15:    ld.ab   %5, [%2, 4]     \n"  // 5
        "       nop_s                   \n"
        "16:    ld.ab   %6, [%2, 4]     \n"  // 6
        "       nop_s                   \n"
        "17:    ld.ab   %7, [%2, 4]     \n"  // 7
        "       nop_s                   \n"
        "       st.ab   %5, [%1, 4]     \n"  // 5
        "       nop_s                   \n"
        "       st.ab   %6, [%1, 4]     \n"  // 6
        "       nop_s                   \n"
        "       st.ab   %7, [%1, 4]     \n"  // 7
        "       sync                    \n"
        "       sub     %0,%0,32        \n"
        "3:     and.f   %3,%3,0x1f      \n" // any left over bytes ?
        "       bz 34f                  \n" // no stragglers
        "       bbit0   %3,4,30f        \n"
        "18:    ld.ab   %5, [%2, 4]     \n"
        "19:    ld.ab   %6, [%2, 4]     \n"
        "20:    ld.ab   %7, [%2, 4]     \n"
        "21:    ld.ab   %8, [%2, 4]     \n"
        "       st.ab   %5, [%1, 4]     \n"
        "       st.ab   %6, [%1, 4]     \n"
        "       st.ab   %7, [%1, 4]     \n"
        "       st.ab   %8, [%1, 4]     \n"
        "       sub.f   %0, %0, 16      \n"
        "30:    bbit0   %3,3,31f        \n" // 8 bytes left
        "22:    ld.ab   %5, [%2,4]      \n"
        "23:    ld.ab   %6, [%2,4]      \n"
        "       st.ab   %5, [%1,4]      \n"
        "       st.ab   %6, [%1,4]      \n"
        "       sub.f   %0,%0,8         \n"
        "31:    bbit0   %3,2,32f        \n" // 4 bytes left.
        "24:    ld.ab   %5, [%2,4]      \n"
        "       st.ab   %5, [%1,4]      \n"
        "       sub.f   %0,%0,4         \n"
        "32:    bbit0   %3,1,33f        \n" // 2 bytes left
        "25:    ldw.ab  %5, [%2,2]      \n"
        "       stw.ab  %5, [%1,2]      \n"
        "       sub.f   %0,%0,2         \n"
        "33:    bbit0   %3,0,34f        \n"
        "26:    ldb.ab  %5, [%2,1]      \n" // just one byte left
        "       stb.ab  %5, [%1,1]      \n"
        "       sub.f   %0,%0,1         \n"
        "34:    nop                     \n"
        "   .section .fixup, \"ax\"     \n"
        "   .align 4                    \n"
        "4: j   34b                     \n"
        "   .previous                   \n"
        "   .section __ex_table, \"a\"  \n"
        "   .align 4                    \n"
        "   .word   1b,4b           \n"
        "   .word   10b, 4b         \n"
        "   .word   11b,4b          \n"
        "   .word   12b,4b          \n"
        "   .word   13b,4b          \n"
        "   .word   14b,4b          \n"
        "   .word   144b,4b         \n"
        "   .word   15b,4b          \n"
        "   .word   16b,4b          \n"
        "   .word   17b,4b          \n"
        "   .word   18b,4b          \n"
        "   .word   19b,4b          \n"
        "   .word   20b,4b          \n"
        "   .word   21b,4b          \n"
        "   .word   22b,4b          \n"
        "   .word   23b,4b          \n"
        "   .word   24b,4b          \n"
        "   .word   25b,4b          \n"
        "   .word   26b,4b          \n"
        "   .previous           \n"
        :"=r"(res), "=r"(to), "=r"(from), "=r"(n), "=r"(val) ,"=r"(tmp1),"=r"(tmp2),"=r"(tmp3),"=r"(tmp4)
        :"3"(n), "1"(to), "2"(from)
        : "lp_count"
    );
#else
        if (__builtin_constant_p(orig_n))
        {
            /* Hand-crafted constant propagation to reduce code sz
             * of the laddred copy 16x,8,4,2,1
             */
            res = orig_n;

            if (orig_n / 16)
            {
                orig_n = orig_n % 16;

                __asm__ __volatile__ (
                "       lsr   lp_count, %7,4            \n"  // 16byte iters
                "       lp    3f                        \n"
                "1:     ld.ab   %3, [%2, 4]             \n"
                "11:    ld.ab   %4, [%2, 4]             \n"
                "12:    ld.ab   %5, [%2, 4]             \n"
                "13:    ld.ab   %6, [%2, 4]             \n"
                "       st.ab   %3, [%1, 4]             \n"
                "       st.ab   %4, [%1, 4]             \n"
                "       st.ab   %5, [%1, 4]             \n"
                "       st.ab   %6, [%1, 4]             \n"
                "       sub     %0,%0,16                \n"
                "3:     ;nop                            \n"
                "   .section .fixup, \"ax\"             \n"
                "   .align 4                            \n"
                "4:     j   3b                          \n"
                "   .previous                           \n"
                "   .section __ex_table, \"a\"          \n"
                "   .align 4                            \n"
                "   .word   1b, 4b                      \n"
                "   .word   11b,4b                      \n"
                "   .word   12b,4b                      \n"
                "   .word   13b,4b                      \n"
                "   .previous                           \n"

                :"+r"(res), "+r"(to), "+r"(from),
                 "=r"(tmp1),"=r"(tmp2),"=r"(tmp3),"=r"(tmp4)
                :"ir"(n)
                :"lp_count");
            }
            if (orig_n / 8)
            {
                orig_n = orig_n % 8;

                __asm__ __volatile__ (
                "14:    ld.ab   %3, [%2,4]              \n"
                "15:    ld.ab   %4, [%2,4]              \n"
                "       st.ab   %3, [%1,4]              \n"
                "       st.ab   %4, [%1,4]              \n"
                "       sub     %0,%0,8                 \n"
                "31:    ;nop                            \n"
                "   .section .fixup, \"ax\"             \n"
                "   .align 4                            \n"
                "4:     j   31b                         \n"
                "   .previous                           \n"
                "   .section __ex_table, \"a\"          \n"
                "   .align 4                            \n"
                "   .word   14b,4b                      \n"
                "   .word   15b,4b                      \n"
                "   .previous                           \n"

                :"+r"(res), "+r"(to), "+r"(from),
                 "=r"(tmp1),"=r"(tmp2)
                );
            }
            if (orig_n / 4)
            {
                orig_n = orig_n % 4;

                __asm__ __volatile__ (
                "16:    ld.ab   %3, [%2,4]              \n"
                "       st.ab   %3, [%1,4]              \n"
                "       sub     %0,%0,4                 \n"
                "32:     ;nop                            \n"
                "   .section .fixup, \"ax\"             \n"
                "   .align 4                            \n"
                "4:     j   32b                         \n"
                "   .previous                           \n"
                "   .section __ex_table, \"a\"          \n"
                "   .align 4                            \n"
                "   .word   16b,4b                      \n"
                "   .previous                           \n"

                :"+r"(res), "+r"(to), "+r"(from),
                 "=r"(tmp1)
               );
            }
            if (orig_n / 2)
            {
                orig_n = orig_n % 2;

                __asm__ __volatile__ (
                "17:    ldw.ab   %3, [%2,2]              \n"
                "       stw.ab   %3, [%1,2]              \n"
                "       sub      %0,%0,2                 \n"
                "33:     ;nop                            \n"
                "   .section .fixup, \"ax\"             \n"
                "   .align 4                            \n"
                "4:     j   33b                         \n"
                "   .previous                           \n"
                "   .section __ex_table, \"a\"          \n"
                "   .align 4                            \n"
                "   .word   17b,4b                      \n"
                "   .previous                           \n"

                :"+r"(res), "+r"(to), "+r"(from),
                 "=r"(tmp1)
                );
            }
            if (orig_n & 1)
            {
                __asm__ __volatile__ (
                "18:    ldb.ab   %3, [%2,2]             \n"
                "       stb.ab   %3, [%1,2]             \n"
                "       sub      %0,%0,1                 \n"
                "34:    ; nop                            \n"
                "   .section .fixup, \"ax\"             \n"
                "   .align 4                            \n"
                "4:     j   34b                         \n"
                "   .previous                           \n"
                "   .section __ex_table, \"a\"          \n"
                "   .align 4                            \n"
                "   .word   18b,4b                      \n"
                "   .previous                           \n"

                :"+r"(res), "+r"(to), "+r"(from),
                 "=r"(tmp1)
                );
            }
        }
        else       /* n is NOT constant, so laddered copy of 16x,8,4,2,1  */
        {

    __asm__ __volatile__ (
        "       mov %0,%3                       \n"
        "       lsr.f   lp_count, %3,4          \n"  // number of words
        "       lpnz    3f                      \n"
        "1:     ld.ab   %5, [%2, 4]             \n"
        "11:    ld.ab   %6, [%2, 4]             \n"
        "12:    ld.ab   %7, [%2, 4]             \n"
        "13:    ld.ab   %8, [%2, 4]             \n"
        "       st.ab   %5, [%1, 4]             \n"
        "       st.ab   %6, [%1, 4]             \n"
        "       st.ab   %7, [%1, 4]             \n"
        "       st.ab   %8, [%1, 4]             \n"
        "       sub     %0,%0,16                \n"
        "3:     and.f   %3,%3,0xf               \n" // any left over bytes ?
        "       bz      34f                     \n" // no stragglers
        "       bbit0   %3,3,31f                \n" // 8 bytes left
        "14:    ld.ab   %5, [%2,4]              \n"
        "15:    ld.ab   %6, [%2,4]              \n"
        "       st.ab   %5, [%1,4]              \n"
        "       st.ab   %6, [%1,4]              \n"
        "       sub.f   %0,%0,8                 \n"
        "31:    bbit0   %3,2,32f                \n" // 4 bytes left.
        "16:    ld.ab   %5, [%2,4]              \n"
        "       st.ab   %5, [%1,4]              \n"
        "       sub.f   %0,%0,4                 \n"
        "32:    bbit0   %3,1,33f                \n" // 2 bytes left
        "17:    ldw.ab  %5, [%2,2]              \n"
        "       stw.ab  %5, [%1,2]              \n"
        "       sub.f   %0,%0,2                 \n"
        "33:    bbit0   %3,0,34f                \n"
        "18:    ldb.ab  %5, [%2,1]              \n" // just one byte left
        "       stb.ab  %5, [%1,1]              \n"
        "       sub.f   %0,%0,1                 \n"
        "34:    ;nop                             \n"
        "   .section .fixup, \"ax\"             \n"
        "   .align 4                            \n"
        "4: j   34b                             \n"
        "   .previous                           \n"
        "   .section __ex_table, \"a\"          \n"
        "   .align 4                            \n"
        "   .word   1b, 4b                      \n"
        "   .word   11b,4b                      \n"
        "   .word   12b,4b                      \n"
        "   .word   13b,4b                      \n"
        "   .word   14b,4b                      \n"
        "   .word   15b,4b                      \n"
        "   .word   16b,4b                      \n"
        "   .word   17b,4b                      \n"
        "   .word   18b,4b                      \n"
        "   .previous                           \n"

        :"=r"(res), "=r"(to), "=r"(from), "=r"(n), "=r"(val) ,"=r"(tmp1),"=r"(tmp2),"=r"(tmp3),"=r"(tmp4)
        :"3"(n), "1"(to), "2"(from)
        :"lp_count"
    );
       }
#endif

    }
    return (res);
}

extern unsigned long slowpath_copy_to_user(
     void *to, const void *from, unsigned long n);

static inline unsigned long
__copy_to_user_inline(void *to, const void *from, unsigned long n)
{
    long res=0;
    char val;
    unsigned long tmp1,tmp2,tmp3,tmp4;
    unsigned long orig_n = n;

    if (n == 0)
        return 0;

    if( ((unsigned long) to & 0x3) || ((unsigned long) from & 0x3))
    {
        // unaligned
        res = slowpath_copy_to_user(to, from, n);
    }
    else   // 32 bit aligned.
    {

#ifdef ARC_HIGH_LATENCY_MEMORY
    __asm__ __volatile__ (
        "       sync                        \n"
        "       mov     %0,%3               \n"
        "       lsr.f   lp_count, %3,5      \n"  // number of words
        "       lpnz    3f                  \n"
        "1:     pf      [%2,32]             \n"
        "       nop_s                       \n"
        "       ld.ab   %5, [%2, 4]         \n"  // 0
        "       nop_s                       \n"
        "       ld.ab   %6, [%2, 4]         \n"  // 1
        "       nop_s                       \n"
        "       ld.ab   %7, [%2, 4]         \n"  // 2
        "       nop_s                       \n"
        "10:    st.ab   %5, [%1, 4]         \n"  // 0
        "       nop_s                       \n"
        "11:    st.ab   %6, [%1, 4]         \n"  // 1
        "       nop_s                       \n"
        "12:    st.ab   %7, [%1, 4]         \n"  // 2
        "       nop_s                       \n"
        "       ld.ab   %5, [%2, 4]         \n"  // 3
        "       nop_s                       \n"
        "       ld.ab   %6, [%2, 4]         \n"  // 4
        "       nop_s                       \n"
        "       pf      [%2,0]              \n"
        "13:    st.ab   %5, [%1, 4]         \n"  // 3
        "       nop_s                       \n"
        "14:    st.ab   %6, [%1, 4]         \n"  // 4
        "       nop_s                       \n"
        "       ld.ab   %5, [%2, 4]         \n"  // 5
        "       nop_s                       \n"
        "       ld.ab   %6, [%2, 4]         \n"  // 6
        "       nop_s                       \n"
        "       ld.ab   %7, [%2, 4]         \n"  // 7
        "       nop_s                       \n"
        "15:    st.ab   %5, [%1, 4]         \n"  // 5
        "       nop_s                       \n"
        "16:    st.ab   %6, [%1, 4]         \n"  // 6
        "       nop_s                       \n"
        "17:    st.ab   %7, [%1, 4]         \n"  // 7
        "       sync                        \n"
        "       sub     %0,%0,32            \n"
        "3:     and.f   %3,%3,0x1f          \n" // any left over bytes ?
        "       bz 34f                      \n" // no stragglers
        "       bbit0   %3,4,30f            \n"
        "       ld.ab   %5, [%2, 4]         \n"
        "       ld.ab   %6, [%2, 4]         \n"
        "       ld.ab   %7, [%2, 4]         \n"
        "       ld.ab   %8, [%2, 4]         \n"
        "18:    st.ab   %5, [%1, 4]         \n"
        "19:    st.ab   %6, [%1, 4]         \n"
        "20:    st.ab   %7, [%1, 4]         \n"
        "21:    st.ab   %8, [%1, 4]         \n"
        "       sub.f   %0, %0, 16          \n"
        "30:    bbit0   %3,3,31f            \n" // 8 bytes left
        "       ld.ab   %5, [%2,4]          \n"
        "       ld.ab   %6, [%2,4]          \n"
        "22:    st.ab   %5, [%1,4]          \n"
        "23:    st.ab   %6, [%1,4]          \n"
        "       sub.f   %0,%0,8             \n"
        "31:    bbit0   %3,2,32f            \n" // 4 bytes left.
        "       ld.ab   %5, [%2,4]          \n"
        "24:    st.ab   %5, [%1,4]          \n"
        "       sub.f   %0,%0,4             \n"
        "32:    bbit0   %3,1,33f            \n" // 2 bytes left
        "       ldw.ab  %5, [%2,2]          \n"
        "25:    stw.ab  %5, [%1,2]          \n"
        "       sub.f   %0,%0,2             \n"
        "33:    bbit0   %3,0,34f            \n"
        "       ldb.ab  %5, [%2,1]          \n" // just one byte left
        "26:    stb.ab  %5, [%1,1]          \n"
        "       sub.f   %0,%0,1             \n"
        "34:    nop                         \n"
        "   .section .fixup, \"ax\"         \n"
        "   .align 4                        \n"
        "4: j   34b                         \n"
        "   .previous                       \n"
        "   .section __ex_table, \"a\"      \n"
        "   .align 4                        \n"
        "   .word   1b,4b                   \n"
        "   .word   10b,4b                  \n"
        "   .word   11b,4b                  \n"
        "   .word   12b,4b                  \n"
        "   .word   13b,4b                  \n"
        "   .word   14b,4b                  \n"
        "   .word   15b,4b                  \n"
        "   .word   16b,4b                  \n"
        "   .word   17b,4b                  \n"
        "   .word   18b,4b                  \n"
        "   .word   19b,4b                  \n"
        "   .word   20b,4b                  \n"
        "   .word   21b,4b                  \n"
        "   .word   22b,4b                  \n"
        "   .word   23b,4b                  \n"
        "   .word   24b,4b                  \n"
        "   .word   25b,4b                  \n"
        "   .word   26b,4b                  \n"
        "   .previous                       \n"
        :"=r"(res), "=r"(to), "=r"(from), "=r"(n), "=r"(val) ,"=r"(tmp1),"=r"(tmp2),"=r"(tmp3),"=r"(tmp4)
        :"3"(n), "1"(to), "2"(from)
        : "lp_count"


    );

#else
        if (__builtin_constant_p(orig_n))
        {
            /* Hand-crafted constant propagation to reduce code sz
             * of the laddred copy 16x,8,4,2,1
             */
            res = orig_n;

            if (orig_n / 16)
            {
                orig_n = orig_n % 16;

                __asm__ __volatile__ (
                "     lsr lp_count, %7,4        \n"  // 16byte iters
                "     lp  3f                    \n"
                "     ld.ab %3, [%2, 4]         \n"
                "     ld.ab %4, [%2, 4]         \n"
                "     ld.ab %5, [%2, 4]         \n"
                "     ld.ab %6, [%2, 4]         \n"
                "1:   st.ab %3, [%1, 4]         \n"
                "11:  st.ab %4, [%1, 4]         \n"
                "12:  st.ab %5, [%1, 4]         \n"
                "13:  st.ab %6, [%1, 4]         \n"
                "     sub   %0, %0, 16          \n"
                "3:   ;nop                      \n"
                "   .section .fixup, \"ax\"     \n"
                "   .align 4                    \n"
                "4: j   3b                      \n"
                "   .previous                   \n"
                "   .section __ex_table, \"a\"  \n"
                "   .align 4                    \n"
                "   .word   1b, 4b              \n"
                "   .word   11b,4b              \n"
                "   .word   12b,4b              \n"
                "   .word   13b,4b              \n"
                "   .previous                   \n"

                :"+r"(res), "+r"(to), "+r"(from),
                 "=r"(tmp1),"=r"(tmp2),"=r"(tmp3),"=r"(tmp4)
                :"ir"(n)
                :"lp_count");

            }
            if (orig_n / 8)
            {
                orig_n = orig_n % 8;

                __asm__ __volatile__ (
                "     ld.ab   %3, [%2,4]        \n"
                "     ld.ab   %4, [%2,4]        \n"
                "14:  st.ab   %3, [%1,4]        \n"
                "15:  st.ab   %4, [%1,4]        \n"
                "     sub     %0, %0, 8         \n"
                "31:  ;nop                      \n"
                "   .section .fixup, \"ax\"     \n"
                "   .align 4                    \n"
                "4: j   31b                     \n"
                "   .previous                   \n"
                "   .section __ex_table, \"a\"  \n"
                "   .align 4                    \n"
                "   .word   14b,4b              \n"
                "   .word   15b,4b              \n"
                "   .previous                   \n"

                :"+r"(res), "+r"(to), "+r"(from),
                 "=r"(tmp1),"=r"(tmp2)
                );
            }
            if (orig_n / 4)
            {
                orig_n = orig_n % 4;

                __asm__ __volatile__ (
                "     ld.ab   %3, [%2,4]        \n"
                "16:  st.ab   %3, [%1,4]        \n"
                "     sub     %0, %0, 4         \n"
                "32:  ;nop                      \n"
                "   .section .fixup, \"ax\"     \n"
                "   .align 4                    \n"
                "4: j   32b                     \n"
                "   .previous                   \n"
                "   .section __ex_table, \"a\"  \n"
                "   .align 4                    \n"
                "   .word   16b,4b              \n"
                "   .previous                   \n"

                :"+r"(res), "+r"(to), "+r"(from),
                 "=r"(tmp1)
                );
            }
            if (orig_n / 2)
            {
                orig_n = orig_n % 2;

                __asm__ __volatile__ (
                "     ldw.ab    %3, [%2,2]      \n"
                "17:  stw.ab    %3, [%1,2]      \n"
                "     sub       %0, %0, 2       \n"
                "33:  ;nop                      \n"
                "   .section .fixup, \"ax\"     \n"
                "   .align 4                    \n"
                "4: j   33b                     \n"
                "   .previous                   \n"
                "   .section __ex_table, \"a\"  \n"
                "   .align 4                    \n"
                "   .word   17b,4b              \n"
                "   .previous                   \n"

                :"+r"(res), "+r"(to), "+r"(from),
                 "=r"(tmp1)
                );
            }
            if (orig_n & 1)
            {
                __asm__ __volatile__ (
                "     ldb.ab  %3, [%2,1]        \n" // just one byte left
                "18:  stb.ab  %3, [%1,1]        \n"
                "     sub     %0, %0, 1         \n"
                "34:  ;nop                      \n"
                "   .section .fixup, \"ax\"     \n"
                "   .align 4                    \n"
                "4: j   34b                     \n"
                "   .previous                   \n"
                "   .section __ex_table, \"a\"  \n"
                "   .align 4                    \n"
                "   .word   18b,4b              \n"
                "   .previous                   \n"

                :"+r"(res), "+r"(to), "+r"(from),
                 "=r"(tmp1)
                );
            }
        }
        else       /* n is NOT constant, so laddered copy of 16x,8,4,2,1  */
        {
    __asm__ __volatile__ (
        "     mov   %0,%3               \n"
        "     lsr.f lp_count, %3,4      \n"  // number of words
        "     lpnz  3f                  \n"
        "     ld.ab %5, [%2, 4]         \n"
        "     ld.ab %6, [%2, 4]         \n"
        "     ld.ab %7, [%2, 4]         \n"
        "     ld.ab %8, [%2, 4]         \n"
        "1:   st.ab %5, [%1, 4]         \n"
        "11:  st.ab %6, [%1, 4]         \n"
        "12:  st.ab %7, [%1, 4]         \n"
        "13:  st.ab %8, [%1, 4]         \n"
        "     sub   %0, %0, 16          \n"
        "3:   and.f %3,%3,0xf           \n" // any left over bytes ?
        "     bz 34f                    \n" // no stragglers
        "     bbit0   %3,3,31f          \n" // 8 bytes left
        "     ld.ab   %5, [%2,4]        \n"
        "     ld.ab   %6, [%2,4]        \n"
        "14:  st.ab   %5, [%1,4]        \n"
        "15:  st.ab   %6, [%1,4]        \n"
        "     sub.f   %0, %0, 8         \n"
        "31:  bbit0   %3,2,32f          \n" // 4 bytes left.
        "     ld.ab   %5, [%2,4]        \n"
        "16:  st.ab   %5, [%1,4]        \n"
        "     sub.f   %0, %0, 4         \n"
        "32:  bbit0 %3,1,33f            \n" // 2 bytes left
        "     ldw.ab    %5, [%2,2]      \n"
        "17:  stw.ab    %5, [%1,2]      \n"
        "     sub.f %0, %0, 2           \n"
        "33:  bbit0 %3,0,34f            \n"
        "     ldb.ab    %5, [%2,1]      \n" // just one byte left
        "18:  stb.ab  %5, [%1,1]        \n"
        "     sub.f %0, %0, 1           \n"
        "34:  ;nop                       \n"
        "   .section .fixup, \"ax\"     \n"
        "   .align 4                    \n"
        "4: j   34b                     \n"
        "   .previous                   \n"
        "   .section __ex_table, \"a\"  \n"
        "   .align 4                    \n"
        "   .word   1b, 4b              \n"
        "   .word   11b,4b              \n"
        "   .word   12b,4b              \n"
        "   .word   13b,4b              \n"
        "   .word   14b,4b              \n"
        "   .word   15b,4b              \n"
        "   .word   16b,4b              \n"
        "   .word   17b,4b              \n"
        "   .word   18b,4b              \n"
        "   .previous                   \n"

        :"=r"(res), "=r"(to), "=r"(from), "=r"(n), "=r"(val) ,"=r"(tmp1),"=r"(tmp2),"=r"(tmp3),"=r"(tmp4)
        :"3"(n), "1"(to), "2"(from)
        :"lp_count"
    );
       }
#endif
    }

    return (res);
}

/*
 * Zero Userspace
 */

static inline unsigned long
__clear_user_inline(void *to, unsigned long n)
{
    long res = n;
    unsigned char *d_char = to;

        __asm__ __volatile__ (
        "   bbit0   %0, 0, 1f \n"
        "75:   stb.ab  %2, [%0,1]\n"
        "   sub %1, %1, 1     \n"
        "1: \n"
        "   bbit0   %0, 1, 2f \n"
        "76:   stw.ab  %2, [%0,2]\n"
        "   sub %1, %1, 2     \n"
        "2: \n"
        "   asr.f   lp_count, %1, 2\n"
        "   lpnz    3f\n"
        "77:   st.ab   %2, [%0,4]\n"
        "   sub %1, %1, 4     \n"
        "3:\n"
        "   bbit0   %1, 1, 4f \n"
        "78:   stw.ab  %2, [%0,2]\n"
        "   sub %1, %1, 2     \n"
        "4: \n"
        "   bbit0   %1, 0, 5f \n"
        "79:   stb.ab  %2, [%0,1]\n"
        "   sub %1, %1, 1     \n"
        "5: \n"

        "   .section .fixup, \"ax\"     \n"
        "   .align 4                    \n"
        "3: j   5b                      \n"
        "   .previous                   \n"

        "   .section __ex_table, \"a\"  \n"
        "   .align 4                    \n"
        "   .word   75b, 3b              \n"
        "   .word   76b, 3b              \n"
        "   .word   77b, 3b              \n"
        "   .word   78b, 3b              \n"
        "   .word   79b, 3b              \n"
        "   .previous                   \n"

        :"+r" (d_char), "+r" (res)
        :"i" (0)
        :"lp_count","lp_start","lp_end");

    return (res);
}

/*
 * Copy a null terminated string from userspace.
 *
 * Must return:
 * -EFAULT      for an exception
 * count        if we hit the buffer limit
 * bytes copied     if we hit a null byte
 * (without the null byte)
 */

static inline long
__strncpy_from_user_inline(char *dst, const char *src, long count)
{
    long res = count;
    char val;
    unsigned int hw_count;

    if (count == 0)
        return 0;

    __asm__ __volatile__ (
        "   lp 2f   \n"
        "1: ldb.ab  %3, [%2, 1]         \n"
        "   breq.d  %3, 0, 2f           \n"
        "   stb.ab  %3, [%1, 1]         \n"
        "2: sub %0, %6, %4              \n"
        "3: ;nop                         \n"
        "   .section .fixup, \"ax\"     \n"
        "   .align 4                    \n"
        "4: mov %0, %5                  \n"
        "   j   3b                      \n"
        "   .previous                   \n"
        "   .section __ex_table, \"a\"  \n"
        "   .align 4                    \n"
        "   .word   1b, 4b              \n"
        "   .previous                   \n"

        :"=r"(res), "+r"(dst), "+r"(src), "=&r"(val),"=l"(hw_count)
        :"g" (-EFAULT), "ir"(count),"4"(count)  // this "4" seeds lp_count abv
        :"memory"
    );

    return (res);
}


static inline long
__strnlen_user_inline(const char *s, long n)
{
    long res, tmp1, cnt;
    char val;


    __asm__ __volatile__ (
        "   mov %2, %1                  \n"
        "1: ldb.ab  %3, [%0, 1]         \n"
        "   breq.d  %3, 0, 2f           \n"
        "   sub.f   %2, %2, 1           \n"
        "   bnz 1b                      \n"
        "   sub %2, %2, 1               \n"
        "2: sub %0, %1, %2              \n"
        "3: ;nop                         \n"
        "   .section .fixup, \"ax\"     \n"
        "   .align 4                    \n"
        "4: mov %0, 0                   \n"
        "   j   3b                      \n"
        "   .previous                   \n"
        "   .section __ex_table, \"a\"  \n"
        "   .align 4                    \n"
        "   .word 1b, 4b                \n"
        "   .previous                   \n"
        :"=r" (res), "=r"(tmp1), "=r"(cnt), "=r" (val)
        :"0" (s), "1"(n)
    );

    return (res);
}

#ifndef NONINLINE_USR_CPY
/*
  documentation says that copy_from_user should return the number of
  bytes that couldn't be copied, we return 0 indicating that all data
  was successfully copied.
  NOTE: quite a few architectures return the size of the copy (n), this
  is wrong because the refernces of copy_from_user consider it an error
  when a positive value is returned
*/
static inline unsigned long
copy_from_user(void *to, const void *from, unsigned long n)
{

    if(access_ok(VERIFY_READ, from, n)) {
        return (__copy_from_user_inline(to, from, n));
    }
    return n;
}

/*
  documentation says that __copy_from_user should return the number of
  bytes that couldn't be copied, we return 0 indicating that all data
  was successfully copied.
  NOTE: quite a few architectures return the size of the copy (n), this
  is wrong because the refernces of __copy_from_user consider it an error
  when a positive value is returned
*/
static inline unsigned long
__copy_from_user(void *to, const void *from, unsigned long n)
{
    return (__copy_from_user_inline(to, from, n));
}

/*
  documentation says that copy_to_user should return the number of
  bytes that couldn't be copied, we return 0 indicating that all data
  was successfully copied.
  NOTE: quite a few architectures return the size of the copy (n), this
  is wrong because the refernces of copy_to_user consider it an error
  when a positive value is returned
*/
static inline unsigned long
copy_to_user(void *to, const void *from, unsigned long n)
{
    if(access_ok(VERIFY_READ, to, n))
        return (__copy_to_user_inline(to, from, n));
    return n;
}

/*
  documentation says that __copy_to_user should return the number of
  bytes that couldn't be copied, we return 0 indicating that all data
  was successfully copied.
  NOTE: quite a few architectures return the size of the copy (n), this
  is wrong because the refernces of __copy_to_user consider it an error
  when a positive value is returned
*/
static inline unsigned long
__copy_to_user(void *to, const void *from, unsigned long n)
{
    return(__copy_to_user_inline(to, from, n));
}

static inline unsigned long __clear_user(void *to, unsigned long n)
{
    return __clear_user_inline(to,n);
}

static inline unsigned long clear_user(void *to, unsigned long n)
{
    if(access_ok(VERIFY_WRITE, to, n))
        return __clear_user_inline(to,n);

    return n;
}

static inline long
strncpy_from_user(char *dst, const char *src, long count)
{
    long res = -EFAULT;

    if (access_ok(VERIFY_READ, src, 1))
        res = __strncpy_from_user_inline(dst, src, count);
    return res;
}


/*
 * Return the size of a string (including the ending 0)
 *
 * Return length of string in userspace including terminating 0
 * or 0 for error.  Return a value greater than N if too long.
 */

static inline long
strnlen_user(const char *s, long n)
{
    if (!access_ok(VERIFY_READ, s, 0))
        return 0;

    return __strnlen_user_inline(s, n);
}

#else

unsigned long copy_from_user(void *to, const void *from, unsigned long n);
unsigned long __copy_from_user(void *to, const void *from, unsigned long n);

unsigned long copy_to_user(void *to, const void *from, unsigned long n);
unsigned long __copy_to_user(void *to, const void *from, unsigned long n);

unsigned long clear_user(void *to, unsigned long n);
unsigned long __clear_user(void *to, unsigned long n);

long strncpy_from_user(char *dst, const char *src, long count);
long strnlen_user(const char *s, long n);

#endif

#define __copy_to_user_inatomic __copy_to_user
#define __copy_from_user_inatomic __copy_from_user


#define strlen_user(str)    strnlen_user((str), 0x7ffffffe)

#endif
