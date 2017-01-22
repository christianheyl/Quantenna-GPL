/*
 * (C) Copyright 2012 Quantenna Communications Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * This is implementation of Peterson's algorithm for mutual exclusion for 2 processes.
 * Implemented for little endian system only now.
 *
 *		//flag[] is boolean array; and turn is an integer
 *		flag[0]   = false;
 *		flag[1]   = false;
 *		turn;
 *
 *		P0: flag[0] = true;
 *		turn = 1;
 *		while (flag[1] == true && turn == 1)
 *		{
 *			// busy wait
 *		}
 *		// critical section
 *		...
 *		// end of critical section
 *		flag[0] = false;
 *
 *		P1: flag[1] = true;
 *		turn = 0;
 *		while (flag[0] == true && turn == 0)
 *		{
 *			// busy wait
 *		}
 *		// critical section
 *		...
 *		// end of critical section
 *		flag[1] = false;
 *
 */

#ifndef __QTN_MPROC_SYNC_MUTEX_H
#define __QTN_MPROC_SYNC_MUTEX_H

#include "mproc_sync_base.h"

#ifndef __ASSEMBLY__

/* Initial value must be zero. */
typedef union __qtn_mproc_sync_mutex
{
	uint32_t dword;
	struct
	{
		uint16_t raw_w0;
		uint16_t raw_w1;
	} words;
	struct
	{
		uint8_t __reserved0;
		uint8_t flag1;
		uint8_t turn;
		uint8_t flag0;
	} bytes;
} qtn_mproc_sync_mutex;

RUBY_INLINE void
qtn_mproc_sync_mutex_init(volatile qtn_mproc_sync_mutex *mutex)
{
	mutex->dword = 0;
}

#if !defined(__GNUC__) && defined(_ARC)
_Inline _Asm void
__qtn_mproc_sync_mutex_relax(int count)
{
	% reg count;
	mov_s	%r12, count;
1:
	sub.f	%r12, %r12, 1;
	bnz_s	1b;
}
RUBY_INLINE void
qtn_mproc_sync_mutex_relax(int count)
{
	if (count) {
		__qtn_mproc_sync_mutex_relax(count);
	}
}
#else
RUBY_INLINE void
qtn_mproc_sync_mutex_relax(int count)
{
	int i;
	for (i = 0; i < count; ++i) {
		qtn_pipeline_drain();
	}
}
#endif // #if !defined(__GNUC__) && defined(_ARC)

RUBY_INLINE void
qtn_mproc_sync_mutex0_lock(volatile qtn_mproc_sync_mutex *mutex, int relax_count)
{
	mutex->words.raw_w1 = 0x0101;

	while ((mutex->dword & 0x00FFFF00) == 0x00010100) {
		qtn_mproc_sync_mutex_relax(relax_count);
	}
}

RUBY_INLINE void
qtn_mproc_sync_mutex0_unlock(volatile qtn_mproc_sync_mutex *mutex)
{
	mutex->bytes.flag0 = 0;
}

RUBY_INLINE void
qtn_mproc_sync_mutex1_lock(volatile qtn_mproc_sync_mutex *mutex, int relax_count)
{
	mutex->bytes.flag1 = 1;
	mutex->bytes.turn = 0;

	while (mutex->words.raw_w1 == 0x0100) {
		qtn_mproc_sync_mutex_relax(relax_count);
	}
}

RUBY_INLINE void
qtn_mproc_sync_mutex1_unlock(volatile qtn_mproc_sync_mutex *mutex)
{
	mutex->bytes.flag1 = 0;
}

#endif // #ifndef __ASSEMBLY__

#endif // #ifndef __QTN_MPROC_SYNC_MUTEX_H

