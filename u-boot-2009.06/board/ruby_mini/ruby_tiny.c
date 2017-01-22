/*
 * (C) Copyright 2015 Quantenna Communications Inc.
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
#include <common.h>
#include <config.h>
#include <command.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <asm/types.h>
#include <asm/cache.h>
#include <timestamp.h>
#include <image.h>

#include "ruby.h"
#include <shared_defs_common.h>
#include "ruby_version.h"
#include "ruby_mini_common.h"
#include "spi_flash.h"

#define UBOOT_LIVE_READ_RETRIES		5
#define UBOOT_SAFETY_READ_RETRIES	3
#define UBOOT_TINY_PREFIX	"UBOOT_TINY: "

void free(void *ptr)
{
        return;
}

int image_check_hcrc(image_header_t *hdr)
{
        ulong hcrc;
        image_header_t header;

        /* Copy header so we can blank CRC field for re-calculation */
        memmove(&header, (char *)hdr, sizeof(header));
        image_set_hcrc(&header, 0);

        hcrc = crc32(0, (unsigned char *)&header, sizeof(header));

        return (hcrc == image_get_hcrc(hdr));
}

int image_check_dcrc(image_header_t *hdr)
{
        ulong data = image_get_data(hdr);
        ulong len = image_get_data_size(hdr);
        ulong dcrc = crc32(0, (unsigned char *)data, len);

        return (dcrc == image_get_dcrc(hdr));
}

static int recover_uboot_live_partition(u8 *mem_addr)
{
	u32 ret_size = 0;

	printf(UBOOT_TINY_PREFIX "recovering live partition\n");
	if (spi_flash_read(UBOOT_SAFE_PARTITION_ADDR, mem_addr, TEXT_BASE_OFFSET, &ret_size) ||
				ret_size != TEXT_BASE_OFFSET) {
		printf(UBOOT_TINY_PREFIX "failed to read safety partition\n");
		return -1;
	}

	if (spi_flash_erase(UBOOT_LIVE_PARTITION_ADDR, UBOOT_TEXT_PARTITION_SIZE)) {
		printf(UBOOT_TINY_PREFIX "failed to erase live partition\n");
		return -1;
	}

	if (spi_flash_write(UBOOT_LIVE_PARTITION_ADDR, mem_addr, TEXT_BASE_OFFSET, &ret_size) ||
				ret_size != TEXT_BASE_OFFSET) {
		printf(UBOOT_TINY_PREFIX "failed to rewrite live partition\n");
		return -1;
	}

	return 0;
}

static int read_uboot_partition(u32 partition_addr, u8 *mem_addr)
{
	image_header_t *image = (image_header_t *)mem_addr;
	u32 ret_size = 0;
	char *part_name = partition_addr == UBOOT_LIVE_PARTITION_ADDR ? "live" : "safety";

	if (spi_flash_read(partition_addr, mem_addr, TEXT_BASE_OFFSET, &ret_size) ||
			ret_size != TEXT_BASE_OFFSET) {
		printf(UBOOT_TINY_PREFIX "failed to read stage 2 from %s partition\n", part_name);
		return -1;
	}

	if (image_check_magic(image) &&
			image_check_hcrc(image) &&
			image_check_dcrc(image)) {
		memcpy(image, (void *)image_get_data(image), image_get_data_size(image));
		return 0;
	}

	return -1;
}

static void get_stage2_uboot(void)
{
	u32 retries = 0;
	int safe_to_start = 0;
	void (*start_stage2_uboot)(void) = (void *)load_addr;

	while (retries < UBOOT_LIVE_READ_RETRIES) {
		if (read_uboot_partition(UBOOT_LIVE_PARTITION_ADDR, (u8 *)load_addr)) {
			recover_uboot_live_partition((u8 *)load_addr);
		} else {
			safe_to_start = 1;
			break;
		}
		++retries;
	}

	if (retries >= UBOOT_LIVE_READ_RETRIES) {
		printf(UBOOT_TINY_PREFIX "failed to recover live partition\n");
		for (retries = 0; retries < UBOOT_SAFETY_READ_RETRIES; ++retries) {
			if (!read_uboot_partition(UBOOT_SAFE_PARTITION_ADDR, (u8 *)load_addr)) {
				printf(UBOOT_TINY_PREFIX "booting from safety partition\n");
				safe_to_start = 1;
				break;
			}
		}

		if (retries >= UBOOT_SAFETY_READ_RETRIES) {
			printf(UBOOT_TINY_PREFIX "failed to boot from safety partition\n");
		}
	}

	if (safe_to_start) {
		flush_and_inv_dcache_all();
		invalidate_icache_all();
		start_stage2_uboot();
	}
}

void start_arcboot(void)
{
	ruby_mini_init();

	printf("\nQuantenna Tiny U-Boot\n");
	printf("Version: %s Built: %s at %s\n",
			RUBY_UBOOT_VERSION, U_BOOT_DATE, U_BOOT_TIME);

	get_stage2_uboot();
	board_reset(UBOOT_TINY_PREFIX "failed to start second stage U-Boot\n");
}

