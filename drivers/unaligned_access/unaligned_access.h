#ifndef __UNALIGNED_ACCESS_H
#define __UNALIGNED_ACCESS_H

#ifdef __KERNEL__
#define PRINT	printk
#else
#include <stdio.h>	
#define PRINT	printf
#endif

#define REGS_SAVED 29

struct regs {
	unsigned long reg[REGS_SAVED];	// registers 0-12 are caller saves, 13-25 callee saves, gp, fp, sp
};

/* fit 4k (1024 * 32bit) of aligned values, 4k of unaligned, and a tiny pad */
static char ua_test_buf[8200];
static void* ua_test_aligned_buf;
static void* ua_test_unaligned_buf;

/* look through the memory at and address +- 16b */
/* N.B. this printk itself triggers unaligned accesses */
static inline void look_buf(char* buf, const unsigned long* values) {
	int i;
	for (i = -4; i <= 4; i++)
		buf += sprintf(buf, "%08lx ", values[i]);
}

/* check for differences in the registers, caused by either the load or by register write back.
   put the results as sprintfs into 'buf'. returns the number of bytes written into 'buf', i.e. 0 = nothing to report */
static inline int check_regs(char* buf,
		const struct regs *regs_pre_a, const struct regs *regs_post_a,
		const struct regs *regs_pre_ua, const struct regs *regs_post_ua) 
{
	char *p = buf;
	int i;
	for (i = 0; i < REGS_SAVED; i++) { 
		unsigned long pre_a = regs_pre_a->reg[i]; 			
		unsigned long post_a = regs_post_a->reg[i]; 			
		unsigned long pre_ua = regs_pre_ua->reg[i]; 			
		unsigned long post_ua = regs_post_ua->reg[i]; 			
		if ((pre_a != post_a || pre_a != pre_ua || pre_a != post_ua) 
				&& !(pre_a == (unsigned long)ua_test_aligned_buf 
					&& pre_ua == (unsigned long)ua_test_unaligned_buf 
					&& post_a == (unsigned long)ua_test_aligned_buf 
					&& post_ua == (unsigned long)ua_test_unaligned_buf) && i != 11) {
			p += sprintf(p, "register %d differs: pre a/ua: %lu (0x%08lx) / %lu (0x%08lx) post %lu (0x%08lx) / %lu (0x%08lx)",
					i, pre_a, pre_a, pre_ua, pre_ua, post_a, post_a, post_ua, post_ua); 
			if (pre_a != pre_ua || post_a != post_ua) 
				p += sprintf(p, "A/UA DIFFERS");
			p += sprintf(p, "\n");
		}
	}

	return p > buf;	
}

/* register stores, to observe difference in register state before and after instructions */
static struct regs regs_aligned_pre, regs_aligned_post, regs_unaligned_pre, regs_unaligned_post;

#define COPY_REGS(r)						\
	rtp = &r;						\
asm volatile (	"sub	%0, %0, 4\n"  	\
		"st.a	r0, [%0, 4]\n"	\
		"st.a	r1, [%0, 4]\n"	\
		"st.a	r2, [%0, 4]\n"	\
		"st.a	r3, [%0, 4]\n"	\
		"st.a	r4, [%0, 4]\n"	\
		"st.a	r5, [%0, 4]\n"	\
		"st.a	r6, [%0, 4]\n"	\
		"st.a	r7, [%0, 4]\n"	\
		"st.a	r8, [%0, 4]\n"	\
		"st.a	r9, [%0, 4]\n"	\
		"st.a	r10, [%0, 4]\n"	\
		"st.a	r11, [%0, 4]\n"	\
		"st.a	r12, [%0, 4]\n"	\
		"st.a	r13, [%0, 4]\n"	\
		"st.a	r14, [%0, 4]\n"	\
		"st.a	r15, [%0, 4]\n"	\
		"st.a	r16, [%0, 4]\n"	\
		"st.a	r17, [%0, 4]\n"	\
		"st.a	r18, [%0, 4]\n"	\
		"st.a	r19, [%0, 4]\n"	\
"st.a	r20, [%0, 4]\n"	\
"st.a	r21, [%0, 4]\n"	\
"st.a	r22, [%0, 4]\n"	\
"st.a	r23, [%0, 4]\n"	\
"st.a	r24, [%0, 4]\n"	\
"st.a	r25, [%0, 4]\n"	\
"st.a	r26, [%0, 4]\n"	\
"st.a	r27, [%0, 4]\n"	\
"st.a	r28, [%0, 4]\n"	\
:: "r" (rtp)	\
);		

/* fill registers with values to set up a test, then execute for both val1a and val1b 
   which are typically 2 memory areas, 1 for aligned and 1 for unaligned data */
#define _CHECK(reg1, val1a, val1b, reg2, val2, reg3, val3, code) {		\
	tp = (void*)val1a;							\
	asm volatile ("mov " #reg1 ",%0" ::"r"(tp) : #reg1);			\
	tp = (void*)val2;							\
	asm volatile ("mov " #reg2 ",%0" ::"r"(tp) : #reg2); 			\
	tp = (void*)val3;							\
	asm volatile ("mov " #reg3 ",%0" ::"r"(tp) : #reg3); 			\
	COPY_REGS(regs_aligned_pre);						\
	asm volatile (code);							\
	COPY_REGS(regs_aligned_post);						\
	tp = (void*)val1b;							\
	asm volatile ("mov " #reg1 ",%0" ::"r"(tp) : #reg1);			\
	tp = (void*)val2;							\
	asm volatile ("mov " #reg2 ",%0" ::"r"(tp) : #reg2); 			\
	tp = (void*)val3;							\
	asm volatile ("mov " #reg3 ",%0" ::"r"(tp) : #reg3); 			\
	COPY_REGS(regs_unaligned_pre);						\
	asm volatile (code);							\
	COPY_REGS(regs_unaligned_post);						\
	err = check_regs(buf, &regs_aligned_pre, &regs_aligned_post, &regs_unaligned_pre, &regs_unaligned_post);\
	PRINT("%s: fragment %s\n", __FUNCTION__, #code);			\
	if (err) PRINT("%s\n", buf);						\
	look_buf(buf, (unsigned long*)ua_test_aligned_buf);			\
	PRINT("%s aligned:   %s\n", __FUNCTION__, buf);				\
	look_buf(buf, (unsigned long*)ua_test_unaligned_buf);			\
	PRINT("%s unaligned: %s\n", __FUNCTION__, buf);				\
}

#define CHECK(reg1, reg2, code) _CHECK(reg1, ua_test_aligned_buf, ua_test_unaligned_buf, reg2, (void*) 0x12345678, reg2, (void*)0x12345678, code)

static inline int test_fragments(void) 
{
	char buf[1024] = {0};							
	int err;
	void* tp;
	register void* rtp asm ("r11"); (void)rtp;

	CHECK(r6,  r13, "ld 	r13, [r6, 0]\n"::);
	CHECK(r8,  r4,  "ld 	r4,[r8,16]\n"::);
	CHECK(r8,  r4,  "ld 	r4,[r8,16]\n"::);
	CHECK(r16, r2,  "ld	r2,[r16,4]\n"::);
	CHECK(r16, r5,  "ld	r5,[r16,8]\n"::);
	CHECK(r16, r5,  "ld.a	r5,[r16,8]\n"::);
	CHECK(r16, r5,  "ld.ab	r5,[r16,8]\n"::);
	CHECK(r16, r3,  "ld	r3,[r16,4]\n"::);
	CHECK(r13, r1,  "ld_s	r1,[r13,16]\n"::);
	CHECK(r13, r2,  "ld_s	r2,[r13,12]\n"::);
	CHECK(r16, r3,  "ld	r3,[r16,12]\n"::);
	CHECK(r14, r2,  "ld_s	r2,[r14,4]\n"::);
	CHECK(r14, r3,  "ld_s	r3,[r14,8]\n"::);
	CHECK(r5,  r2,  "ld	r2,[r5,12]\n"::);

	// one of each permutation of each instruction, as found in a kernel objdump
	CHECK(r3,  r2,  "ld	r2,[r3,12]\n"::);
	CHECK(r3,  r2,  "ld.a	r2,[r3,8]\n"::);
	CHECK(r3,  r5,  "ld.ab	r5,[r3,4]\n"::);
	// .di instructions are not handled by the routine, they cause a sigsegv
	//CHECK(r3, "ld.a.di	r8,[r3,4]\n"::);
	CHECK(r3,  r2,  "ld.as	r2,[r3,92]\n"::);
	//CHECK(r3, "ld.di	r2,[r3,8]\n"::);
	CHECK(r3,  r2,  "ld_s	r2,[r3,4]\n"::);
	CHECK(r3,  r2,  "ldw	r2,[r3,2]\n"::);
	CHECK(r17, r4,  "ldw	r4,[r17,6]\n"::);
	CHECK(r3,  r2,  "ldw	r2,[r3,0]\n"::);
	CHECK(r3,  r2,  "ldw.a	r2,[r3,-2]\n"::);
	CHECK(r3,  r5,  "ldw.ab	r5,[r3,2]\n"::);
	//CHECK(r3, "ldw.ab.di	r8,[r3,12]\n"::);
	//CHECK(r3, "ldw.a.di	r8,[r3,16]\n"::);
	CHECK(r3,  r2,  "ldw.as	r2,[r3,161]\n"::);
	//CHECK(r3, "ldw.as.di	r8,[r3,20]\n"::);
	//CHECK(r3, "ldw.di	r2,[r3,24]\n"::);
	CHECK(r3,  r0,  "ldw_s	r0,[r3,8]\n"::);
	CHECK(r3,  r2, "ldw_s.x	r2,[r3,18]\n"::);
	CHECK(r3,  r2,  "ldw.x	r2,[r3,18]\n"::);
	//CHECK(r3, "ldw.x.ab.di	r4,[r3,-184]\n"::);
	CHECK(r3,  r8,"ldw.x.as	r8,[r3,-256]\n"::);
	
	CHECK(r3, r6, "st	r6,[r3,12]\n"::);
	CHECK(r3, r6, "st.a	r6,[r3,-12]\n"::);
	CHECK(r3, r6, "st.ab	r6,[r3,4]\n"::);
	CHECK(r3, r7,  "st	r7,[r3,12]\n"::);
	CHECK(r3, r8,  "st.as	r8,[r3,-8]\n"::);
	//CHECK(r3, "st.di	r2,[r3,-24]\n"::);
	CHECK(r3, r12, "st_s	r12,[r3,4]\n"::);
	CHECK(r3, r5, "stw.ab	r5,[r3,2]\n"::);
	CHECK(r3, r6, "stw.a	r6,[r3,-6]\n"::);
	CHECK(r3, r5, "stw.ab	r5,[r3,6]\n"::);
	CHECK(r3, r2, "stw.as	r2,[r3,-10]\n"::);
	CHECK(r3, r2, "stw_s	r2,[r3,10]\n"::);

	// higher registers
	CHECK(r22,  r18, "ld	r18,[r22,12]\n"::);
	// altering sp and fp crash the test harness (make sense...). compiler doesn't like gp being clobbered
	//CHECK(sp,  r2, "ld.a	r2,[sp,8]\n"::);
	//CHECK(fp,  r5, "ld.ab	r5,[fp,4]\n"::);
	//CHECK(gp,  r2, "ld.as	r2,[gp,92]\n"::);
	CHECK(r15, r2,  "ld_s	r2,[r15,4]\n"::);
	CHECK(r22, r24, "ldw	r24,[r22,-10]\n"::);
	//CHECK(sp,  r2, "ldw.a	r2,[sp,-2]\n"::);
	//CHECK(gp,  r5, "ldw.ab	r5,[gp,2]\n"::);
	//CHECK(fp,  r2, "ldw.as	r2,[fp,161]\n"::);
	CHECK(r12, r14, "ldw_s	r14,[r12,8]\n"::);
	CHECK(r13, r15, "ldw_s.x r15,[r13,18]\n"::);
	CHECK(r19, r16, "ldw.x	r16,[r19,18]\n"::);
	CHECK(r24, r22, "ldw.x.as r22,[r24,-256]\n"::);
	
	_CHECK(r20, ua_test_aligned_buf, ua_test_unaligned_buf, r21, 16, r22, 0x14, "ld	r22,[r20,r21]\n"::);
	_CHECK(r20, ua_test_aligned_buf, ua_test_unaligned_buf, r21, -16, r23, 0x14, "ld	r23,[r20,r21]\n"::);
	_CHECK(r20, ua_test_aligned_buf, ua_test_unaligned_buf, r21, 0, r24, 0x14, "ld	r24,[r20,r21]\n"::);

	CHECK(r20, r21, "st	r21,[r20,12]\n"::);
	CHECK(r21, r22, "st.a	r22,[r21,-12]\n"::);
	CHECK(r22, r23, "st.ab	r23,[r22,4]\n"::);
	CHECK(r23, r24, "st	r24,[r23,12]\n"::);
	CHECK(r24, r19, "st.as	r19,[r24,-8]\n"::);
	CHECK(r15, r14, "st_s	r14,[r15,4]\n"::);
	CHECK(r23, r17, "stw.ab	r17,[r23,2]\n"::);
	//CHECK(sp, r6, "stw.a	r6,[sp,-6]\n"::);
	//CHECK(gp, r5, "stw.ab	r5,[gp,6]\n"::);
	//CHECK(fp, r2, "stw.as	r2,[fp,-10]\n"::);
	CHECK(r14, r2, "stw_s	r2,[r14,14]\n"::);

	return 0;
}

/* copy an int to a buffer at arbitrary offset, using byte operations to avoid faulting */
static inline void copy_ua(const int i, char* buf) {
	// little endian
	buf[3] = (i >> 24) & 0xFF;
	buf[2] = (i >> 16) & 0xFF;
	buf[1] = (i >> 8)  & 0xFF;
	buf[0] = (i >> 0)  & 0xFF;
}

static inline int do_unaligned_access(int n) {

	// the first half of the buffer will be aligned values
	// second half will be unaligned, same stuff but 1kb+1b offset
	int i;
	for (i = 0; i < 1024; i++) { 
		copy_ua(i * n, &ua_test_buf[4*i]);
		copy_ua(i * n, &ua_test_buf[4*i + 4096 + 1]);
	}

	ua_test_aligned_buf = (void*)&ua_test_buf[2048];	// 2k of offset in either direction
	ua_test_unaligned_buf = (void*)&ua_test_buf[4096 + 2048 + 1];

	PRINT("%s ua_test_buf %p ua_test_aligned_buf %p ua_test_unaligned_buf %p\n", 
			__FUNCTION__, ua_test_buf, ua_test_aligned_buf, ua_test_unaligned_buf);

	int r = test_fragments();

	return r;
}

#endif // __UNALIGNED_ACCESS_H

