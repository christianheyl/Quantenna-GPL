/*
 * (C) Copyright 2015
 *
 *  Quantenna Communications Inc.
 *
 *  SPDX-License-Identifier:     GPL-2.0+
 */

#include "ruby.h"

#ifdef CONFIG_CMD_QMEMTEST

#include <linux/types.h>
#include <command.h>
#include <asm/cache.h>
#include <asm/string.h>
#include "ruby_board_cfg.h"
#include "board_cfg.h"
#include "common.h"

#define QMEMTEST_STRESS_ITER_NUM	1000000	/* randomly chosen */

static inline void qmtest_checkerboard_print_error(vu_long *addr, ulong must_be, ulong actual)
{
	printf("\nERROR at 0x%p: must be 0x%lx real 0x%lx (0x%lx)\n", addr, must_be, actual, *addr);
}

static inline int do_single_checkerboard_test(vu_long *start, vu_long *end, const ulong pattern)
{
	vu_long *cur_p = start;
	const ulong rev_pattern = ~pattern;
	/*
	 * We need to do reads into temporary variable. In this case, should any
	 * read operation fail during the test, we can print unexpected read value on the
	 * screen.
	 */
	register ulong tmp1;
	register ulong tmp2;

	printf("0x%08lx ~ 0x%08lx.. ", pattern, rev_pattern);

	while (cur_p < end) {
		*cur_p++ = pattern;
		*cur_p++ = rev_pattern;
	};

	cur_p = start;
	while (cur_p < end) {
		tmp1 = *cur_p;
		*cur_p++ = rev_pattern;
		tmp2 = *cur_p;
		*cur_p++ = pattern;

		if (tmp1 != pattern) {
			qmtest_checkerboard_print_error(cur_p - 2, pattern, tmp1);
			return -1;
		}

		if (tmp2 != rev_pattern) {
			qmtest_checkerboard_print_error(cur_p - 1, rev_pattern, tmp2);
			return -1;
		}
	};

	printf("2nd pass.. ");

	cur_p = start;
	while (cur_p < end) {
		tmp1 = *cur_p++;
		tmp2 = *cur_p++;

		if (tmp1 != rev_pattern) {
			qmtest_checkerboard_print_error(cur_p - 2, rev_pattern, tmp1);
			return -1;
		}

		if (tmp2 != pattern) {
			qmtest_checkerboard_print_error(cur_p - 1, pattern, tmp2);
			return -1;
		}
	};

	printf("OK\n");
	return 0;
}

/*
 * Checkerboard test:
 * - fill entire memory with alternating patterns, reversing every other memory
 * address location
 * - verify that content of memory locations are as expected and immediately write
 * an inverse of previously used pattern for each location;
 * - again verify that content of memory locations are as expected;
 * Try several check patterns that stress the data bus the most.
 */
static int do_checkerboard_test(ulong start, ulong end)
{
	const ulong check_patterns[] = {
			0xAAAAAAAA,
			0x0000ffff,
			0xAAAA5555,
	};
	int i = 0;

	puts("CHECKERBOARD:\n");

	for (i = 0; i < ARRAY_SIZE(check_patterns); ++i) {
		if (do_single_checkerboard_test((vu_long *)start, (vu_long *)end, check_patterns[i]))
			return -1;
	}

	return 0;
}

static inline int do_single_stress_test(vu_long *addr1, vu_long *addr2, ulong pattern)
{
	unsigned int i;
	register ulong tmp1;
	register ulong tmp2;

	printf("addr 0x%p & 0x%p patt 0x%lx: ", addr1, addr2, pattern);

	for (i = 0; i < QMEMTEST_STRESS_ITER_NUM; ++i) {
		*addr1 = pattern;
		*addr2 = pattern;
		tmp1 = *addr1;
		tmp2 = *addr2;
		if (tmp1 != tmp2) {
			goto error;
		}

		*addr1 = ~pattern;
		*addr2 = ~pattern;
		tmp1 = *addr1;
		tmp2 = *addr2;
		if (tmp1 != tmp2) {
			goto error;
		}
	}

	puts("OK\n");

	return 0;

error:
	printf("ERROR i=%u: addr 0x%p (0x%lx - 0x%lx) != 0x%p (0x%lx - 0x%lx)\n",
			i, addr1, tmp1, *addr1, addr2, tmp2, *addr2);
	return -1;
}

/*
 * Stress test for DRAM.
 *
 * The purpose is to stress system the most by driving DRAM address and data lines massively
 * up and down as fast as possible. As a simple example, we would want to write test data at
 * address 0x0 and then immediately at address 0xFFFFFFFF, then do a readback test.
 *
 * But things are a little more complicated for the types of DRAM generally used on embedded
 * platform:
 * - DRAM chips used with Quantenna platforms usually have only 16 physical data lines, and
 * each 32-bit word is transfered with two bus operations rather then one. Therefore, in case
 * of 16-bit data line, the most stressful would be transfers of values like 0xFFFF0000.
 * - address lines usually include separate BankSelect lines and multiplexed Row/Column
 * selection lines.
 *
 * As an example, we can look at Winbond memory chip organization.
 * Winnbond memory has  8388608 words x 8banks x 16bit organization, 128 Mbyte, it has
 * 13 physical address lines. Address lines are multiplexed for row and column selection,
 * meaning that for any memory access Memory Controller first sends row number on address
 * lines, and then immediately sends column number.
 * Here's the mapping scheme used to map 32-bit physical address to DDR controller signals:
 * |b31...b27|b26...b14|b13...b11|b10...b1|bit0    |  <--- 32bit physical address bits
 * | unused  |  row #  | bank ID | col #  | unused |  <--- mapping to DDR interface signals
 *
 * From this example it's obvious that to stress address lines the most we have to access
 * addresses which will generate inverse electrical signals for Row and Column parts of address.
 * But because memory chips organization can vary a lot, there is no best solution for a
 * general case. That's why to generate a test location address, we simply divide address
 * in half based on memory size, and make first half an inverse of another half.
 */
static inline int do_stress_test(ulong start, ulong end)
{
	ulong size = end - start;
	ulong mask = size - 1;
	ulong half_mask;
	ulong test_addr1;
	ulong test_addr2;
	unsigned i = 0;
	unsigned j;
	ulong stress_addr_pattern[] = {
		0xAAAAAAAA,
		0xFFFFFFFF
	};
	ulong stress_data_pattern[] = {
		0xFFFF0000,
		0x5555AAAA
	};

	if (size == 0 || (size & (size - 1))) {
		printf("Bad size 0x%lx\n", size);
		return -1;
	}

	while (mask) {
		++i;
		mask >>= 1;
	}

	/* Addresses must always be 4-byte aligned */
	mask = (size - 1) & ~0x3;
	half_mask = mask >> (i / 2);

	printf("STRESS: mask=0x%08lx half_mask=0x%08lx\n", mask, half_mask);

	for (i = 0; i < ARRAY_SIZE(stress_addr_pattern); ++i) {
		test_addr1 = start + (((stress_addr_pattern[i] & half_mask) |
				(~stress_addr_pattern[i] & ~half_mask)) & mask);
		test_addr2 = start + (((stress_addr_pattern[i] & ~half_mask) |
				(~stress_addr_pattern[i] & half_mask)) & mask);

		for (j = 0; j < ARRAY_SIZE(stress_data_pattern); ++j) {
			if (do_single_stress_test((vu_long *)test_addr1, (vu_long *)test_addr2,
					stress_data_pattern[j])) {
				return -1;
			}
		}
	}

	return 0;
}

/*
 * One-To-Many Linear Feedback Shift Register implementation based on equation:
 * x^15 + x^13 + x^12 + x^10 + 1
 * It will generate 65535 distinct 16-bit values before it repeats itself.
 * This length of random number sequence feats us well since integer number of
 * 65535 values sequence will not fit into RAM array, meaning that every new iteration
 * of this Prand generator will start at different low-order address bits.
 */
static uint16_t qmtest_rand_gen(uint16_t seed)
{
	int need_toggle = !!(seed & 0x1);

	seed >>= 1;
	if (need_toggle) {
		seed ^= 0xB400;
	}

	return seed;
}

/*
 * Random test:
 * Use a pseudo-random generator to fill entire RAM array. Then use the same generator
 * again to do a readback test.
 */
static int do_random_test(uint16_t *start, uint16_t *end, const uint16_t seed)
{
	uint16_t pattern = seed;
	uint16_t *cur_p = start;

	printf("RANDOM test: seed 0x%04x.. ", seed);

	/* Seed value for our LFSR can not be 0 or 0xffff, otherwise it will lockup */
	if (seed == 0 || seed == 0xFFFF) {
		puts("Bab seed value\n");
		return -1;
	}

	while (cur_p < end) {
		pattern = qmtest_rand_gen(pattern);
		*cur_p++ = pattern;
	}

	pattern = seed;
	cur_p = start;
	while (cur_p < end) {
		pattern = qmtest_rand_gen(pattern);
		if (*cur_p != pattern) {
			printf("\nERROR at 0x%p: must be 0x%04x actual 0x%04x\n", cur_p, *cur_p, pattern);
			return -1;
		}
		++cur_p;
	}

	puts("OK\n");
	return 0;
}

/*
 * Iterate through all 16-bit values using each as a seed for random test.
 * It will take forever to complete by itself.
 * Can be interrupted with ctrl+C.
 */
static inline int do_long_random_test(uint16_t *start, uint16_t *end)
{
	unsigned int curr_seed = 1;

	printf("Long STRESS test: ctrl-C to abort\n");

	while (curr_seed < 0xffff) {
		if (do_random_test(start, end, curr_seed)) {
			return -1;
		}
		curr_seed += 0xD;

		if (ctrlc()) {
			puts("Abort\n");
			break;
		}
	}

	return 0;
}

enum {
	QTEST_CHECKERBOARD = (1 << 0),
	QTEST_STRESS = (1 << 1),
	QTEST_RANDOM = (1 << 2),
	QTEST_RANDOM_LONG = (1 << 3),
};

int do_qmemtest(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{

	ulong start_addr = CONFIG_SYS_MEMTEST_START;
	ulong end_addr;
	int dcache_was_en = dcache_status();
	int ret = 1;
	uint16_t seed = 0xD31B;
	DECLARE_GLOBAL_DATA_PTR;
	int ddr_size = board_config(gd->bd->bi_board_id, BOARD_CFG_DDR_SIZE);
	unsigned int test_bitm = 0;

	if (argc > 3) {
		cmd_usage(cmdtp);
		return 1;
	} else if (argc > 1) {
		if (strcmp(argv[1], "checkerboard") == 0) {
			test_bitm |= QTEST_CHECKERBOARD;
		} else if (strcmp(argv[1], "stress") == 0) {
			test_bitm |= QTEST_STRESS;
		} else if (strcmp(argv[1], "random") == 0) {
			test_bitm |= QTEST_RANDOM;
			if (argc == 3) {
				seed = (uint16_t)simple_strtoul(argv[2], NULL, 16);
			}
		} else if (strcmp(argv[1], "random_long") == 0) {
			test_bitm |= QTEST_RANDOM_LONG;
		} else {
			cmd_usage(cmdtp);
			return 1;
		}
	} else {
		test_bitm |= QTEST_CHECKERBOARD | QTEST_STRESS | QTEST_RANDOM;
	}

	if (ddr_size > 0) {
		end_addr = start_addr + ddr_size;
	} else {
		end_addr = CONFIG_SYS_MEMTEST_END;
		printf("Couldn't get real DDR size\n");
	}

	printf("DRAM test, range [0x%08lx, 0x%08lx) %lu MB\n",
		start_addr, end_addr, (end_addr - start_addr) / 1024 / 1024);

	dcache_disable();

	if ((test_bitm & QTEST_CHECKERBOARD) && do_checkerboard_test(start_addr, end_addr))
		goto finished;

	if ((test_bitm & QTEST_STRESS) && do_stress_test(start_addr, end_addr))
		goto finished;

	if ((test_bitm & QTEST_RANDOM) &&
			do_random_test((uint16_t *)start_addr, (uint16_t *)end_addr, seed))
		goto finished;

	if ((test_bitm & QTEST_RANDOM_LONG) &&
			do_long_random_test((uint16_t *)start_addr, (uint16_t *)end_addr))
		goto finished;

	ret = 0;
finished:
	if (dcache_was_en)
		dcache_enable();

	return ret;
}

U_BOOT_CMD(qmemtest, 3, 0, do_qmemtest,
	"qmemtest [checkerboard, stress, random [16bit seed], random_long]",
	"Run all or a single specified testcase\n"
);

#endif
