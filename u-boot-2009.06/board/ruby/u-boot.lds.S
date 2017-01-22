/*
 * Quantenna U-Boot linkmap. Ruby / Topaz platform
 */

#include <common/common_mem.h>
#include <configs/ruby.h>

OUTPUT_FORMAT("elf32-littlearc", "elf32-littlearc", "elf32-littlearc")
OUTPUT_ARCH(arc)

ENTRY(_start)

#if TOPAZ_MMAP_UNIFIED
	#define SRAM_BEGIN	RUBY_SRAM_BEGIN
	#define SRAM_UC_BEGIN	RUBY_SRAM_BEGIN
#else
	#define SRAM_BEGIN	RUBY_SRAM_BEGIN
	#define SRAM_UC_BEGIN	RUBY_SRAM_NOFLIP_BEGIN
#endif

#define UC_MEM_DIFF	(SRAM_BEGIN - SRAM_UC_BEGIN)

MEMORY
{
	sram    : ORIGIN = (SRAM_BEGIN + CONFIG_ARC_TEXT_OFFSET),    LENGTH = CONFIG_ARC_TEXT_SIZE
	sram_uc	: ORIGIN = (SRAM_UC_BEGIN + CONFIG_ARC_TEXT_OFFSET), LENGTH = CONFIG_ARC_TEXT_SIZE
}


SECTIONS
{
	.text :
	{
		__uboot_begin = .;
		board/ruby/start.o (.text) /* must be first */

#if defined(CONFIG_CMD_UC)
		. = MAX(. , MAX(__uboot_begin + (__muc_start_end - __muc_start_begin), __uboot_begin + (__dsp_start_end - __dsp_start_begin)));
		board/ruby/muc_start.o (.text)
		board/ruby/dsp_start.o (.text)
#endif

		*(.text)
	} > sram
	. = ALIGN(4);

	.rodata :
	{
		*(.rodata)
		*(.rodata.str*)
	} > sram
	. = ALIGN(4);

	.data :
	{
		*(.data)
	} > sram
	. = ALIGN(4);

	__u_boot_cmd_start = .;
	.u_boot_cmd :
	{
		*(.u_boot_cmd)
	} > sram
	__u_boot_cmd_end = .;
	. = ALIGN(4);

#if defined(CONFIG_CMD_UC)
	/****MuC/DSP begin***********************************************************************************************************************/
	/*
	 * LHOST and MuC/DSP have SRAM mapped to different addresses (memmap flip mode).
	 * To have MuC/DSP funcs to be part of LHOST uboot image let's play with linker script.
	 * If memmap flip mode is changed, or uboot moved to DRAM, please correct this file!
	 */
	.text.uc ABSOLUTE(.) - UC_MEM_DIFF : AT(ADDR(.text.uc) + UC_MEM_DIFF) /* move relocation address to MuC/DSP view, load address - to LHOST view */
	{
		*(.text.uc)
	} > sram_uc
	. = ALIGN(4);
	.data.uc :
	{
		*(.data.uc)
	} > sram_uc
	. = ALIGN(4);
	__uc_bss_begin = .;
	.bss.uc :
	{
		*(.bss.uc)
	} > sram_uc
	__uc_bss_end = .;
	. = ALIGN(4);
	. += UC_MEM_DIFF; /* move relocation address back to LHOST view */
	/****MuC/DSP end*************************************************************************************************************************/
#endif

	__bss_start = .;
	.bss ABSOLUTE(.) :
	{
		*(.bss)
		*(.bss.atags)
	} > sram
	. = ALIGN(4);
	__bss_end = .;

	__uboot_end = .;
}

