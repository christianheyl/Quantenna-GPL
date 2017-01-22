/*
 * Copyright (c) 2011 Quantenna Communications, Inc.
 * All rights reserved.
 */

#ifndef _QTN_ARC_PROCESSOR_H_
#define _QTN_ARC_PROCESSOR_H_

#if defined(STATIC_CHECK)
uint32_t get_sp(void);
uint32_t get_ilink1(void);
uint32_t get_ilink2(void);
uint32_t get_blink(void);
uint32_t get_status32(void);
uint8_t arc_read_uncached_8(const uint8_t *addr);
void arc_write_uncached_8(uint8_t *addr, uint8_t value);
uint16_t arc_read_uncached_16(const uint16_t *addr);
void arc_write_uncached_16(uint16_t *addr, uint32_t value);
uint32_t arc_read_uncached_32(const uint32_t *addr);
void arc_write_uncached_32(uint32_t *addr, uint32_t value);
#elif defined(_ARC)

_Inline _Asm uint32_t get_sp(void)
{
	mov %r0, %r28
}

_Inline _Asm uint32_t get_ilink1(void)
{
	mov %r0, %r29
}

_Inline _Asm uint32_t get_ilink2(void)
{
	mov %r0, %r30
}

_Inline _Asm uint32_t get_blink(void)
{
	mov %r0, %r31
}

_Inline _Asm uint32_t get_status32(void)
{
	lr %r0, [%status32]
}

_Inline _Asm uint8_t arc_read_uncached_8(const uint8_t *addr)
{
	%reg addr
	ldb.di %r0, [addr]
}

_Inline _Asm void arc_write_uncached_8(uint8_t *addr, uint8_t value)
{
	%reg addr, value
	stb.di value, [addr]
}

_Inline _Asm uint16_t arc_read_uncached_16(const uint16_t *addr)
{
	%reg addr
	ldw.di %r0, [addr]
}

_Inline _Asm void arc_write_uncached_16(uint16_t *addr, uint32_t value)
{
	%reg addr, value
	stw.di value, [addr]
}

_Inline _Asm uint32_t arc_read_uncached_32(const uint32_t *addr)
{
	%reg addr
	ld.di %r0, [addr]
}

_Inline _Asm void arc_write_uncached_32(uint32_t *addr, uint32_t value)
{
	%reg addr, value
	st.di value, [addr]
}

#else
/* implementations provided elsewhere */
#endif	// STATIC_CHECK
#endif	// _QTN_ARC_PROCESSOR_H_
