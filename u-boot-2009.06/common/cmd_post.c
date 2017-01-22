/*
 * Implement following commands:
 *	(1). LED
 *	(2). DRAM Address Line Continuity
 *	(3). RF Loop Back
 *	(4). Ethernet RJ-45 Continuity
 */

#include <common.h>

#include <asm/arch/platform.h>
#include <asm/io.h>

#include <command.h>
#include "net.h"

/* Seconds waiting LED turn off	*/
#define POST_LED_WAIT_TIME	1

#define POST_MEM_BIT   0
#define POST_PHY_BIT   1
#define POST_RF_BIT    2
#define POST_MEM_LEVEL_BIT   4
#define POST_MEM_LEVEL_MASK  3

int memory_post(unsigned long *start, unsigned long size);
void disable_phy_loopback(void);

int do_post_led(unsigned int mask)
{
	unsigned val;
	int offset;
	int i;

	if (!mask)
		mask = 0xcc;
	/*
	  * 0xcc: bit2:wifi link led, bit3:wps led,
	  *bit6:mode led, bit7:link quality led
	  */
	val = readl(RUBY_GPIO_AFSEL);
	val &= ~mask;
	writel(val, RUBY_GPIO_AFSEL);

	/* Set GPIO to Output mode and Mask */
	for (i = 0; i < RUBY_GPIO_MAX; i++) {
		if (!((1 << i) & mask))
			continue;

		if (i < RUBY_GPIO_MODE1_MAX) {
			offset = i * 2;

			val = readl(RUBY_GPIO_MODE1);
			val &= ~(0x3 << offset);
			val |= (0x1 << offset);
			writel(val, RUBY_GPIO_MODE1);
		} else {
			offset = (i - RUBY_GPIO_MODE1_MAX) * 2;

			val = readl(RUBY_GPIO_MODE2);
			val &= ~(0x3 << offset);
			val |= (0x1 << offset);
			writel(val, RUBY_GPIO_MODE2);
		}
	}

	writel(mask, GPIO_OUTPUT_MASK);

	/* Turn on LED	*/
	writel(mask, GPIO_OUTPUT);

	udelay(POST_LED_WAIT_TIME * 1000000);

	/* Turn off LED	*/
	writel(0, GPIO_OUTPUT);

	writel(0, GPIO_OUTPUT_MASK);

	return 0;

}

int post_led(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	unsigned int mask;
	int ret;

	/* GPIO mask used for LED */
	if (argc > 1)
		mask = (ulong)simple_strtoul(argv[1], NULL, 16);
	else
		mask = 0;

	ret = do_post_led(mask);

	return ret;
}

U_BOOT_CMD(
	post_led, CONFIG_SYS_MAXARGS, 0, post_led,
	"POST LED test",
	"gpio_mask\n"
	"    - Set gpio_mask bits of gpio pins\n"
);

int post_phy(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int ret;

	ret = NetLoop(ETHLOOP);
	#ifdef CONFIG_CMD_ETHLOOP
	disable_phy_loopback();
	#endif
	if (ret < 0)
		printf("phy loopback test failed\n");
	else
		printf("phy loopback test pass\n");

	return ret;
}

U_BOOT_CMD(
	post_phy, CONFIG_SYS_MAXARGS, 0, post_phy,
	"POST phy loopback test"
	"ethernet loopback test\n"
	);

int do_post_mem(unsigned int iteration_limit)
{
	ulong	*start, size;
	unsigned int iterations;
	int ret;
	char *p;
	int dcache;

	dcache = dcache_status();
	if (dcache)
		dcache_disable();

	start = (ulong *)CONFIG_SYS_MEMTEST_START;
	p = getenv("post_mem_size");
	if (p) {
		size = simple_strtoul(p, NULL, 16);
		printf("user defined mem size 0x%x\n", size);
	} else {
		size = RUBY_MIN_DRAM_SIZE;
	}

	if (!iteration_limit) {  /* 0 for loop */
		for (;;) {
			if (ctrlc()) {
				/* printf("stop by ctrl-c ... "); */
				break;
			}
			ret = memory_post(start, size);
			if (ret < 0)
				break;
			iterations++;
		}
	} else {
		for (iterations = 0; iterations < iteration_limit; iterations++) {
			if (ctrlc()) {
				/* printf("stop by ctrl-c ... "); */
				break;
			}
			ret = memory_post(start, size);
			if (ret < 0)
				break;
		}
	}

	if (dcache)
		dcache_enable ();

	return ret;
}

int post_mem(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	unsigned int iteration_limit;
	int ret;

	if (argc > 1)
		iteration_limit = (ulong)simple_strtoul(argv[1], NULL, 16);
	else
		iteration_limit = 1;

	ret = do_post_mem(iteration_limit);

	return ret;
}

U_BOOT_CMD(
	post_mem,	CONFIG_SYS_MAXARGS,	0,	post_mem,
	"POST RAM test",
	"[iterations]"
	"    - RAM Address/Data lines read/write test\n"
);

int do_post(void){
	char *p;
	int post_mask, old_result = 0, new_result = 0, result_update = 0;
	int old_function = 0, new_function = 0, functionn_update = 0;
	int ret = 0, mem_level;
	char buf[32];

	p = getenv("post_result");
	if (p)
		old_result = simple_strtoul(p, NULL, 16);

	p = getenv("post_function");
	if (p)
		old_function = simple_strtoul(p, NULL, 16);

	new_function |= (1<<POST_PHY_BIT);
	if ((new_function&(1<<POST_PHY_BIT)) != (old_function&(1<<POST_PHY_BIT)))
		functionn_update = 1;

	new_function |= (1<<POST_MEM_BIT);
	if ((new_function&(1<<POST_MEM_BIT)) != (old_function&(1<<POST_MEM_BIT)))
		functionn_update = 1;

	p = getenv("post_mask");
	if (p) {
		post_mask = simple_strtoul(p, NULL, 16);
		printf("post_mask 0x%x\n", post_mask);

		if ((post_mask & 1<<POST_MEM_BIT)) {
			mem_level = ((post_mask >> POST_MEM_LEVEL_BIT) & POST_MEM_LEVEL_MASK);
			switch (mem_level) {
			case 0:
				ret = do_post_mem(1);
				break;
			case 1:
				ret = do_post_mem(1000);
				break;
			case 2:
				ret = do_post_mem(1000000);
				break;
			case 3:
				ret = do_post_mem(0);
				break;
			}
			printf("mem test ... ");
			if (ret < 0) {
				printf("fail\n");
				new_result &= ~(1<<POST_MEM_BIT);
			} else {
				printf("pass\n");
				new_result |= (1<<POST_MEM_BIT);
			}
			if ((new_result&(1<<POST_MEM_BIT)) != (old_result&(1<<POST_MEM_BIT))) {
				old_result &= ~(1<<POST_MEM_BIT);
				old_result |= new_result&(1<<POST_MEM_BIT);
				result_update = 1;
			}
		}

		if (post_mask & 1<<POST_PHY_BIT) {
			ret = NetLoop(ETHLOOP);
			#ifdef CONFIG_CMD_ETHLOOP
			disable_phy_loopback();
			#endif
			if (ret < 0) {
				new_result &= ~(1<<POST_PHY_BIT);
				printf("phy loopback test ... fail\n");
			} else {
				new_result |= (1<<POST_PHY_BIT);
				printf("phy loopback test ... pass\n");
			}
			if ((new_result&(1<<POST_PHY_BIT)) != (old_result&(1<<POST_PHY_BIT))) {
				old_result &= ~(1<<POST_PHY_BIT);
				old_result |= new_result&(1<<POST_PHY_BIT);
				result_update = 1;
			}
		}
	} else {
		printf("post_mask is null\n");
	}

	if (functionn_update | result_update) {
		if (functionn_update) {
			sprintf(buf, "%x", new_function);
			setenv("post_function", buf);
		}
		if (result_update) {
			sprintf(buf, "%x", old_result);
			setenv("post_result", buf);
		}
		saveenv();
	}
}

int post (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int ret;

	ret = do_post();

	return ret;
}

#ifdef POST_BOOT
U_BOOT_CMD(
	post,	CONFIG_SYS_MAXARGS,	0,	post,
	"POST test",
	"POST test\n"
);

#endif

