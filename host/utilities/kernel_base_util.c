/*
 * (C) Copyright 2013 Quantenna Communications, Inc.
 *
 * Script to print the kernel base address.
 */

#include <stdio.h>
#include <string.h>
#include "ruby_mem.h"

int main(int argc, char **argv)
{
	const char *prog = argv[0];

	if (argc == 2 && strcmp(argv[1], "-e") == 0) {
		fprintf(stdout, "0x%08x", RUBY_DRAM_BEGIN + CONFIG_ARC_KERNEL_BASE);
	} else if (argc == 2 && strcmp(argv[1], "-a") == 0) {
		fprintf(stdout, "0x%08x", RUBY_DRAM_BEGIN + CONFIG_ARC_KERNEL_BOOT_BASE);
	} else {
		fprintf(stderr, "Usage, %s {-a | -e}\n", prog);
		return 1;
	}

	return 0;
}

