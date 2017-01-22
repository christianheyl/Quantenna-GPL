OUTPUT_FORMAT("elf32-littlearc", "elf32-littlearc", "elf32-littlearc")
OUTPUT_ARCH(arc)
ENTRY(_start)
MEMORY
{
 sram : ORIGIN = (0x88000000 + (4 * (64 * 1024))), LENGTH = ((8 * (64 * 1024)) - (4 * (64 * 1024)))
 sram_uc : ORIGIN = (0x80000000 + (4 * (64 * 1024))), LENGTH = ((8 * (64 * 1024)) - (4 * (64 * 1024)))
}
SECTIONS
{
 .text :
 {
  __uboot_begin = .;
  board/ruby/start.o (.text)
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
