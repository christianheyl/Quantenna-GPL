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

#include <common.h>
#include <config.h>
#include <command.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <asm/types.h>
#include <timestamp.h>

#include "ruby.h"
#include "board_cfg.h"
#include <shared_defs_common.h>
#include "ruby_version.h"
#include "ruby_mini_common.h"

#define CONFIG_SYS_MALLOC_LEN	0x10000

extern int do_pcieboot(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);

extern void qtn_mini_hw_init(int);
extern int board_cfg_init(int *);
extern void env_relocate (void);
extern void board_pmu_init(void);

/*
 * Because dlmalloc will comsume more than 2k memory space
 * so implement a simple memory allocator
 * just used for access the u-boot env variable
 */
extern char __uboot_end;
static ulong mem_malloc_start = 0;
static ulong mem_malloc_end = 0;
static void mem_malloc_init (void)
{
	mem_malloc_start =  (ulong)&__uboot_end;
	mem_malloc_end = mem_malloc_start + CONFIG_SYS_MALLOC_LEN;
}

void *malloc (int size)
{
	if (mem_malloc_start + size > mem_malloc_end) {
		printf("Fail to allocate memory!!!\n");
		return NULL;
	}
	memset((void *)mem_malloc_start, 0, size);
	mem_malloc_start += size;
	return (void *)(mem_malloc_start - size);
}

void free(void *ptr)
{
	return;
}

static void get_stage2(void)
{
	int board_id;

	board_pmu_init();

	if (board_cfg_init(&board_id) != 0) {
		printf("error: board configuration not found\n");
		return;
	}

	qtn_mini_hw_init(board_id);

	printf("Quantenna Mini U-Boot\n");
	printf("Version: %s Built: %s at %s\n",
			RUBY_UBOOT_VERSION, U_BOOT_DATE, U_BOOT_TIME);

	do_pcieboot(NULL, 0, 0, NULL);
}

void start_arcboot(void)
{
	ruby_mini_init();

	mem_malloc_init();

	env_relocate();

	get_stage2();

	board_reset("get_stage2 returned");
}
