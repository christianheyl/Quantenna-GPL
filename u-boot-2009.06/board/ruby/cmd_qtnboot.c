/*
 * Quantenna boot support
 */
#include <common.h>
#include <command.h>
#include <environment.h>

#include "ruby.h"
#include "spi_flash.h"

#define BUFSIZE 256

static int qtnboot_check(char *buf,
		unsigned long flash_size, unsigned long sector_size,
		unsigned long safety_addr, unsigned long safety_size,
		unsigned long live_addr, unsigned long live_size)
{
	/* check partition sizes */
	if ((safety_size == 0) || (live_size == 0)) {
		sprintf(buf, "linux partition sizes must be non-zero");
		return -1;
	}

	/* check that partition addresses and sizes are sector aligned */
	if ((safety_addr % sector_size) ||
			(live_addr % sector_size) ||
			(safety_size % sector_size) ||
			(live_size % sector_size)) {
		sprintf(buf, "all values must fit sector alignment: 0x%lx", sector_size);
		return -1;
	}

	/* check that safety partition start clears the u-boot partitions */
	if (safety_addr < IMAGES_START_ADDR) {
		sprintf(buf, "%s 0x%lx overlaps u-boot partitions (ending 0x%x)",
				SAFETY_IMG_ADDR_ARG, safety_addr, IMAGES_START_ADDR);
		return -1;
	}

	/* check that live is at least as large as safety, so safety can be copied over live */
	if (live_size < safety_size) {
		sprintf(buf, "safety image size exceeds live image size");
		return -1;
	}

	/* check that safety end doesn't overlap live start */
	if (live_addr < safety_addr + safety_size) {
		sprintf(buf, "live_addr 0x%lx not after safety end 0x%lx",
				live_addr, safety_addr + safety_size);
		return -1;
	}

	/* check that live end doesn't overrun flash end */
	if (live_addr + live_size > flash_size) {
		sprintf(buf, "live end 0x%lx exceeds flash size 0x%lx",
				live_addr + live_size, flash_size);
		return -1;
	}

	return 0;
}

/* get all of the pointers, vaguely validate them, 1 on failure, 0 on success */
static int get_qtnboot_envvars(unsigned long *safety_addr, unsigned long *live_addr,
		unsigned long *safety_size, unsigned long *live_size)
{
	const unsigned long sector_size = spi_flash_sector_size();
	const unsigned long flash_size = spi_flash_size();

	const char *safety_addr_str = getenv(SAFETY_IMG_ADDR_ARG);
	const char *safety_size_str = getenv(SAFETY_IMG_SIZE_ARG);
	const char *live_addr_str = getenv(LIVE_IMG_ADDR_ARG);
	const char *live_size_str = getenv(LIVE_IMG_SIZE_ARG);
	char errbuf[BUFSIZE] = {0};
	int error;

	*safety_addr = 0;
	*safety_size = 0;
	*live_addr = 0;
	*live_size = 0;

	/*
	 * Look for environment variables for each parameter,
	 * or provide sensible defaults based on flash size.
	 * No defaults for 8MB, which isn't officially supported
	 */
	if (safety_addr_str) {
		*safety_addr = simple_strtoul(safety_addr_str, NULL, 0);
	} else {
		*safety_addr = IMAGES_START_ADDR;
	}

	if (safety_size_str) {
		*safety_size = simple_strtoul(safety_size_str, NULL, 0);
	} else if (flash_size == FLASH_16MB) {
		*safety_size = IMG_SIZE_16M_FLASH_2_IMG;
	}

	if (live_addr_str) {
		*live_addr = simple_strtoul(live_addr_str, NULL, 0);
	} else {
		*live_addr = *safety_addr + *safety_size;
	}

	if (live_size_str) {
		*live_size = simple_strtoul(live_size_str, NULL, 0);
	} else {
		*live_size = *safety_size;
	}

	error = qtnboot_check(errbuf, flash_size, sector_size,
			*safety_addr, *safety_size, *live_addr, *live_size);

	printf("%s: vars: %s 0x%lx %s 0x%lx %s 0x%lx %s 0x%lx\n",
			__FUNCTION__,
			SAFETY_IMG_ADDR_ARG, *safety_addr,
			SAFETY_IMG_SIZE_ARG, *safety_size,
			LIVE_IMG_ADDR_ARG, *live_addr,
			LIVE_IMG_SIZE_ARG, *live_size);
	if (error) {
		printf("%s: vars invalid: %s\n", __FUNCTION__, errbuf);
	}

	return error;
}

/* setup the mtdargs parameter to pass a partition table into linux */
static void set_mtdparts(unsigned long safety_size, unsigned long live_size)
{
	char mtdparts[BUFSIZE];

	sprintf(mtdparts, "spi_flash:"
			"%dk(" MTD_PARTNAME_UBOOT_BIN "),"
			"%dk(" MTD_PARTNAME_UBOOT_ENV "),"
			"%dk(" MTD_PARTNAME_UBOOT_ENV_BAK "),"
			"%luk(" MTD_PARTNAME_LINUX_SAFETY "),"
			"%luk(" MTD_PARTNAME_LINUX_LIVE "),"
			"-(" MTD_PARTNAME_DATA ")",
			UBOOT_TEXT_PARTITION_SIZE / 1024,
			UBOOT_ENV_PARTITION_SIZE / 1024,
			UBOOT_ENV_PARTITION_SIZE / 1024,
			safety_size / 1024,
			live_size / 1024
	       );

	setenv("mtdparts", mtdparts);
}

int do_qtn_setmtdparts (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	unsigned long safety_addr = 0;
	unsigned long safety_size = 0;
	unsigned long live_addr = 0;
	unsigned long live_size = 0;

	if (get_qtnboot_envvars(&safety_addr, &live_addr, &safety_size, &live_size))
		return -1;
	set_mtdparts(safety_size, live_size);
	return 0;
}

U_BOOT_CMD(qtn_setmtdparts, CONFIG_SYS_MAXARGS, 0, do_qtn_setmtdparts,
           "set the environment variable 'mtdparts'",
           "sets mtdparts to a string appropriate for the mtdparts kernel \n"
           "command line argument. Partitions are derived from the environment\n"
           "variables: ${" LIVE_IMG_ADDR_ARG "}, ${" SAFETY_IMG_ADDR_ARG "},\n"
           "${" SAFETY_IMG_SIZE_ARG "} and ${" LIVE_IMG_SIZE_ARG "}\n"
          );

int _run(const char *function_name, const char *fmt, ...)
{
	va_list args;
	char cmdbuf[BUFSIZE];

	va_start(args, fmt);
	vsprintf(cmdbuf, fmt, args);
	va_end(args);

	printf("%s: %s\n", function_name, cmdbuf);

	return run_command(cmdbuf, 0);
}

int do_qtnboot (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	unsigned long safety_addr = 0;
	unsigned long safety_size = 0;
	unsigned long live_addr = 0;
	unsigned long live_size = 0;
	const unsigned long mem_addr = QTNBOOT_COPY_DRAM_ADDR;

	if (get_qtnboot_envvars(&safety_addr, &live_addr, &safety_size, &live_size)) {
		return -1;
	}

	set_mtdparts(safety_size, live_size);
	RUN("setenv bootargs ${bootargs} mtdparts=${mtdparts}");

	// attempt to load the live image into memory and boot it.
	RUN("spi_flash read 0x%08lx 0x%08lx 0x%08lx", live_addr, mem_addr, live_size);
	RUN("bootm 0x%08lx", mem_addr);

	// if control returns, it failed
	// load the safety image into memory, copy it over the live image, then boot/reset
	RUN("spi_flash read 0x%08lx 0x%08lx 0x%08lx", safety_addr, mem_addr, safety_size);

	// run_command returns -1 for errors, or repeatability, rather than return codes
	if (RUN("imi 0x%08lx", mem_addr) < 0) {
		printf("FATAL: safety image at 0x%08lx appears corrupt\n", safety_addr);
		return -1;
	}

	RUN("spi_flash unlock");
	RUN("spi_flash erase 0x%08lx 0x%08lx", live_addr, live_size);
	RUN("spi_flash write 0x%08lx 0x%08lx 0x%08lx", live_addr, mem_addr, safety_size);
	RUN("sleep 2");
	RUN("reset");

	// never gets to here
	return 0;
}

U_BOOT_CMD(qtnboot, CONFIG_SYS_MAXARGS, 0, do_qtnboot,
		"boot from live image, recover safety image if necessary",
		"Quantenna dual boot with recovery. Attempts to boot the live image\n"
		"found at address ${" LIVE_IMG_ADDR_ARG "}. If the checksum fails, \n"
		"the safety image at ${" SAFETY_IMG_ADDR_ARG "} is copied over the \n"
		"live image, then booted.\n"
	  );

int do_bootselect(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	unsigned long safety_addr = 0;
	unsigned long safety_size = 0;
	unsigned long live_addr = 0;
	unsigned long live_size = 0;
	unsigned long addr;
	unsigned long size;
	const unsigned long mem_addr = QTNBOOT_COPY_DRAM_ADDR;
	unsigned long bootsel_val = 0;
	const char* bootsel_str = NULL;
	int num_of_image = 2;

	if (get_qtnboot_envvars(&safety_addr, &live_addr, &safety_size, &live_size)) {
		return -1;
	}

	bootsel_str = getenv("bootselect");
	if (bootsel_str) {
		bootsel_val = simple_strtoul(bootsel_str, NULL, 0);
	}

	while (num_of_image--) {
		if (bootsel_val) {
			addr = safety_addr;
			size = safety_size;
		} else {
			addr = live_addr;
			size = live_size;
		}

		RUN("spi_flash read 0x%08lx 0x%08lx 0x%08lx", addr, mem_addr, size);
		RUN("bootm 0x%08lx", mem_addr);

		//if control return,switch to boot another image
		bootsel_val = !bootsel_val;
		RUN("setenv bootselect %d", bootsel_val);
		saveenv();
	}

	return -1;
}

U_BOOT_CMD(bootselect, CONFIG_SYS_MAXARGS, 0, do_bootselect,
		"boot live/safety depending on value of 'bootselect'",
		"If env variable 'bootselect' is 1, boot from image at safety address;\n"
		"Otherwise boot from image at live address\n"
	  );


int do_setgpio(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	unsigned long ngpio;
	unsigned long value;

	if (argc < 2) {
		cmd_usage(cmdtp);
		return 1;
	}

	ngpio = simple_strtoul(argv[1], NULL, 10);
	if (ngpio >= RUBY_GPIO_MAX)
		return 1;

	if (argc > 2) {
		value = simple_strtoul(argv[2], NULL, 10);
		value = !!value;
	} else
		value = 1;

	gpio_output(ngpio, value);
	gpio_config(ngpio, RUBY_GPIO_MODE_OUTPUT);

	return 0;
}

U_BOOT_CMD(setgpio, CONFIG_SYS_MAXARGS, 2, do_setgpio,
	"Configure gpio as output and set the output value",
	"ngpio [value], drive the ngpio to 'value'\n"
	"drive the ngpio to 1 if value is absent\n"
);

