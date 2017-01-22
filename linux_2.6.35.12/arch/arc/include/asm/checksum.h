/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * vineetg: May 2010
 *  -Rewrote ip_fast_cscum( ) and csum_fold( ) with fast inline asm
 */

#ifndef _ASM_ARC_CHECKSUM_H
#define _ASM_ARC_CHECKSUM_H

#include <linux/in6.h>
/*
 * computes the checksum of a memory block at buff, length len,
 * and adds in "sum" (32-bit)
 *
 * returns a 32-bit number suitable for feeding into itself
 * or csum_tcpudp_magic
 *
 * this function must be called with even lengths, except
 * for the last fragment, which may be odd
 *
 * it's best to have buff aligned on a 32-bit boundary
 */
unsigned int csum_partial(const void * buff, int len, unsigned int sum);

/*
 * the same as csum_partial, but copies from src while it
 * checksums, and handles user-space pointer exceptions correctly, when needed.
 *
 * here even more important to align src and dst on a 32-bit (or even
 * better 64-bit) boundary
 */

unsigned int csum_partial_copy(const char *src, char *dst, int len, int sum);

/*
 * the same as csum_partial_copy, but copies from user space.
 *
 * here even more important to align src and dst on a 32-bit (or even
 * better 64-bit) boundary
 */

extern unsigned int csum_partial_copy_from_user(const char *src, char *dst,
						int len, int sum, int *csum_err);

#define csum_partial_copy_nocheck(src, dst, len, sum)	\
	csum_partial_copy((src), (dst), (len), (sum))

/*
 *	Fold a partial checksum
 */
static inline __sum16 csum_fold(__wsum sum)
{
    unsigned int lower, upper;
    __sum16 folded;

    /* The 32 bit sum is broken into two 16 bit halves which are then added
     * We test if this results in a carry (bit 16 set), in which case it is
     * added back. This is slightly optimal than unconditionally adding the
     * upper bits of 16 bit sum (in anticipation of carry) back to sum
     *
     * Also as per the csum algorithm, after adding the carry back to sum,
     * the carry portion needs to be discarded before doing the invert.
     * However we leave the discard to end, when gcc for the purpose of
     * returning a sword, anyways downconverts the word to s-word, discarding
     * that portion
     */
    __asm__ __volatile__(
        "bmsk   %0, %3, 15  \n"  // break 32 bit @sum into two 16 bit words
        "lsr    %1, %3, 16  \n"
        "add    %0, %0, %1  \n"  // add them together
        "btst   %0, 16      \n"  // was there a carry in 16 bit sum
        "add.nz %0, %0, 1   \n"  // if yes, add it back to sum
                                 // extw needed - deferred (see below)
        "not    %2, %0      \n"  // csum req a final invert of sum
        :"=&r"(lower), "=&r" (upper), "=r" (folded)
        :"r" (sum)
        :"cc"
    );

   /* This implies extw insn, which down-conv 32-bit word to 16 bit word,
    * for the 16 bit return semantics. It also takes care of discarding
    * the carry portion of 16 bit sum, which we avoided purposefully
    * in the inline asm above
    */
    return folded;
}

/*
 *	This is a version of ip_compute_csum() optimized for IP headers,
 *	which always checksum on 4 octet boundaries.
 */
static inline __sum16 ip_fast_csum(void *iph, unsigned int ihl)
{
    void *ptr = iph;
    unsigned int sum, tmp;

    __asm__ __volatile__(
        "ld.ab   %0, [%2, 4]        \n\t"
        "sub.f   lp_count, %3, 1    \n\t"
        "lpnz  1f                   \n\t"
        "ld.ab   %1, [%2, 4]        \n\t"
        "adc.f   %0, %0, %1         \n\t"
        "1:                         \n\t"
        "adc     %0, %0, 0          \n\t"
        :"=&r"(sum), "=r"(tmp), "+r"(ptr)
        :"r"(ihl)
        :"cc"
    );

    return csum_fold(sum);
}

static inline unsigned long csum_tcpudp_nofold(unsigned long saddr,
						   unsigned long daddr,
						   unsigned short len,
						   unsigned short proto,
						   unsigned int sum)
{
	__asm__ __volatile__(
		"add.f %0, %0, %1   \n"
		"adc.f %0, %0, %2   \n"
		"adc.f %0, %0, %3   \n"
		"adc.f %0, %0, %4   \n"
		"adc   %0, %0, 0    \n"
		: "=r" (sum)
		: "r" (saddr), "r" (daddr), "r" (ntohs(len) << 16),
		  "r" (proto << 8), "0" (sum)
        : "cc"
	);

    return sum;
}

/*
 * computes the checksum of the TCP/UDP pseudo-header
 * returns a 16-bit checksum, already complemented
 */
static inline unsigned short int csum_tcpudp_magic(unsigned long saddr,
						   unsigned long daddr,
						   unsigned short len,
						   unsigned short proto,
						   unsigned int sum)
{
	return csum_fold(csum_tcpudp_nofold(saddr,daddr,len,proto,sum));
}

unsigned short ip_compute_csum(const unsigned char * buff, int len);

#if 0
/* AmitS - This is done in arch/arcnommu/lib/checksum.c */
/*
 * this routine is used for miscellaneous IP-like checksums, mainly
 * in icmp.c
 */

static inline unsigned short ip_compute_csum(unsigned char * buff, int len)
{
    return csum_fold (csum_partial(buff, len, 0));
}
#endif

//#define _HAVE_ARCH_IPV6_CSUM
#if 0
static inline unsigned short int csum_ipv6_magic(struct in6_addr *saddr,
						     struct in6_addr *daddr,
						     __u32 len,
						     unsigned short proto,
						     unsigned int sum)
{
	__asm__ (
		/* just clear the flags for the adc.f inst. in the loop */
		"sub.f 0, r0, r0      \n"
		/* Loop 4 times for the {sd}addr fields */
		"mov   lp_count, 0x04 \n"
		/* There have to be atleast 3 cycles betn loop count
		 * setup and last inst. in the loop
		 */
		"nop                  \n"
		"lp    1f             \n"
		"adc.f %0, %0, [%1]   \n"
		"add   %1, %1, 4      \n"
		"1:                   \n"
		"mov   lp_count, 0x04 \n"
		"nop                  \n"
		"lp    2f             \n"
		"adc.f %0, %0, [%2]   \n"
		"add   %2, %2, 4      \n"
		"2:                   \n"
		"adc.f %0, %0, %3     \n"
		"nop                  \n"
		"adc.f %0, %0, %4     \n"
		"nop                  \n"
		"adc.f %0, %0, 0x0    \n"
		: "=&r" (sum), "=r" (saddr), "=r" (daddr)
		: "r" (htonl(len)), "r" (htonl(proto)),
		  "0" (sum), "1" (saddr), "2" (daddr)
		);
	return csum_fold(sum);
}

#endif

#if 0
/*
 *	Copy and checksum to user
 */
#define HAVE_CSUM_COPY_USER
static inline unsigned int csum_and_copy_to_user(const char *src, char *dst,
				    int len, int sum, int *err_ptr)
{
	if (access_ok(VERIFY_WRITE, dst, len))
		return csum_partial_copy(src, dst, len, sum);

	if (len)
		*err_ptr = -EFAULT;

	return -1; /* invalid checksum */
}
#endif

extern unsigned short ip_compute_csum(const unsigned char * buff, int len);
#endif /* _ASM_ARC_CHECKSUM_H */
