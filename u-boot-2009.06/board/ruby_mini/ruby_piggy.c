/*
 * (C) Copyright 2011 - 2013 Quantenna Communications Inc.
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
 */

#include "common_mem.h"
#include <config.h>
#include <asm/cache.h>
#include <asm/arcregs.h>

#ifdef PIGGY_DEBUG
#include "common.h"
#include "ruby_mini_common.h"

static void debug_setup(void)
{
	ruby_mini_init();
	printf("Quantenna U-Boot Piggy DEBUG\n");
}

#define	DBGVAR(x)	do {					\
	printf("%s:%d:%s %s = %u 0x%x\n",			\
			__FILE__, __LINE__, __FUNCTION__,	\
			(#x), (unsigned int)(x), (int)(x));	\
} while(0)
#else
#define debug_setup()
#define DBGVAR(x)
#endif

int lzmaBuffToBuffDecompress (unsigned char *outStream, unsigned long *uncompressedSize,
			      unsigned char *inStream,  unsigned long length);

void __attribute__((noreturn)) start_arcboot(void)
{
	extern unsigned char payload[];
	extern unsigned int payload_len;

	extern unsigned int early_flash_config_start;
	extern unsigned int early_flash_config_end;
	unsigned int *pin;
	unsigned int *pout;
	int rc;

	unsigned long decompression_addr = RUBY_SRAM_BEGIN + TEXT_BASE_OFFSET_CHILD;
	unsigned long uncompressed_size;
	void (*start)(void) __attribute__((noreturn));

	debug_setup();

	DBGVAR(&start);
	DBGVAR(decompression_addr);
	DBGVAR(TEXT_BASE_OFFSET);
	DBGVAR(TEXT_BASE_OFFSET_CHILD);

	/* decompress u-boot mini */
	rc = lzmaBuffToBuffDecompress((void *) decompression_addr, &uncompressed_size,
			payload, payload_len);
	DBGVAR(rc);

	/* copy over early config bytes */
	pin = &early_flash_config_start;
	pout = (unsigned int*)((unsigned long)pin
			- (RUBY_SRAM_BEGIN + TEXT_BASE_OFFSET)
			+ decompression_addr);
	while (pin < &early_flash_config_end) {
		*pout++ = *pin++;
	}

	start = (void *)(decompression_addr +  read_new_aux_reg(SCRATCH_DATA0));

	flush_and_inv_dcache_all();
	invalidate_icache_all();

	start();
}

#ifndef PIGGY_DEBUG
void * memset(void * s, int c, unsigned int count)
{
	char *xs = (char *) s;

	while (count--)
		*xs++ = c;

	return s;
}

void * memcpy(void * dest, const void *src, unsigned int count)
{
	char *tmp = (char *) dest, *s = (char *) src;

	while (count--)
		*tmp++ = *s++;

	return dest;
}
#endif	// PIGGY_DEBUG

