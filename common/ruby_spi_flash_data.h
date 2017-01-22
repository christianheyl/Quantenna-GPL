/*
 * (C) Copyright 2010 Quantenna Communications Inc.
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

/*
 * MTD SPI driver for ST M25Pxx (and similar) serial flash chips
 *
 * Author: Mike Lavender, mike@steroidmicros.com
 *
 * Copyright (c) 2005, Intec Automation Inc.
 *
 * Some parts are based on lart.c by Abraham Van Der Merwe
 *
 * Cleaned up and generalized based on mtd_dataflash.c
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */


/*****************************************************
 * Structure definitions as well as data was borrowed
 * from Linux kernel, 2.6.30 version.
 * File name is drivers/mtd/devices/m25p80.c
 */

#ifndef __SPI_FLASH_DATA_H
#define __SPI_FLASH_DATA_H

#include "ruby_spi_api.h"

#define FREQ_UNKNOWN		0
#define FREQ_MHZ(num)		(num * 1000000)

/* NOTE: double check command sets and memory organization when you add
 * more flash chips.  This current list focusses on newer chips, which
 * have been converging on command sets which including JEDEC ID.
 */

static struct flash_info flash_data [] = {

	/* Atmel -- some are (confusingly) marketed as "DataFlash" */
	{ "at25fs010", 0x1f6601, 32 * 1024, 4, 0, FREQ_UNKNOWN, ATMEL, },
	{ "at25fs040", 0x1f6604, 64 * 1024, 8, 0, FREQ_UNKNOWN, ATMEL, },

	{ "at25df041a", 0x1f4401, 64 * 1024, 8, 0, FREQ_UNKNOWN, ATMEL, },
	{ "at25df641", 0x1f4800, 64 * 1024, 128, 0, FREQ_UNKNOWN, ATMEL, },

	{ "at26f004", 0x1f0400, 64 * 1024, 8, 0, FREQ_UNKNOWN, ATMEL, },
	{ "at26df081a", 0x1f4501, 64 * 1024, 16, 0, FREQ_UNKNOWN, ATMEL, },
	{ "at26df161a", 0x1f4601, 64 * 1024, 32, 0, FREQ_UNKNOWN, ATMEL, },
	{ "at26df321",  0x1f4701, 64 * 1024, 64, 0, FREQ_UNKNOWN, ATMEL, },
	{ "at25f512b",  0x1f6500,  4 * 1024, 16, SECTOR_ERASE_OP20, FREQ_MHZ(50), ATMEL, },

	/* Spansion -- single (large) sector size only, at least
	 * for the chips listed here (without boot sectors).
	 */
	{ "s25sl004a", 0x010212, 64 * 1024, 8, 0, FREQ_UNKNOWN, SPANSION },
	{ "s25sl008a", 0x010213, 64 * 1024, 16, 0, FREQ_UNKNOWN, SPANSION },
	{ "s25sl016a", 0x010214, 64 * 1024, 32, 0, FREQ_UNKNOWN, SPANSION },
	{ "s25fl032", 0x010215, 64 * 1024, 64, 0, FREQ_UNKNOWN, SPANSION },
	{ "s25sl064a", 0x010216, 64 * 1024, 128, 0, FREQ_UNKNOWN, SPANSION },
	{ "s25sl12801", 0x012018, 64 * 1024, 256, 0, FREQ_MHZ(40), SPANSION }, /* S25FL129P has same jedec_id, but higher frequency - so it should be supported, but at not at maximum speed */
	{ "s25fl256s", 0x010219, 64 * 1024, 512, 0, FREQ_UNKNOWN, SPANSION },

	/* Micron */
	{ "N25Q032", 0x20ba16, 64 * 1024, 64, 0, FREQ_UNKNOWN, MICRON },

	/* SST -- large erase sizes are "overlays", "sectors" are 4K */
	{ "sst25vf040b", 0xbf258d, 64 * 1024, 8, 0, FREQ_UNKNOWN, SST },
	{ "sst25vf080b", 0xbf258e, 64 * 1024, 16, 0, FREQ_UNKNOWN, SST },
	{ "sst25vf016b", 0xbf2541, 64 * 1024, 32, 0, FREQ_UNKNOWN, SST },
	{ "sst25vf032b", 0xbf254a, 64 * 1024, 64, 0, FREQ_UNKNOWN, SST },

	/* ST Microelectronics -- newer production may have feature updates */
	{ "m25p05", 0x202010, 32 * 1024, 2, 0, FREQ_UNKNOWN, ST_MICROELECTRONICS },
	{ "m25p10", 0x202011, 32 * 1024, 4, 0, FREQ_UNKNOWN, ST_MICROELECTRONICS },
	{ "m25p20", 0x202012, 64 * 1024, 4, 0, FREQ_UNKNOWN, ST_MICROELECTRONICS },
	{ "m25p40", 0x202013, 64 * 1024, 8, 0, FREQ_UNKNOWN, ST_MICROELECTRONICS },
	{ "m25p80", 0x202014, 64 * 1024, 16, 0, FREQ_UNKNOWN, ST_MICROELECTRONICS },
	{ "m25p16", 0x202015, 64 * 1024, 32, 0, FREQ_UNKNOWN, ST_MICROELECTRONICS },
	{ "m25p32", 0x202016, 64 * 1024, 64, 0, FREQ_MHZ(75), ST_MICROELECTRONICS },
	{ "m25p64", 0x202017, 64 * 1024, 128, 0, FREQ_UNKNOWN, ST_MICROELECTRONICS },
	{ "m25p128", 0x202018, 256 * 1024, 64, 0, FREQ_MHZ(54), ST_MICROELECTRONICS },

	{ "m45pe80", 0x204014, 64 * 1024, 16, 0, FREQ_UNKNOWN, ST_MICROELECTRONICS },
	{ "m45pe16", 0x204015, 64 * 1024, 32, 0, FREQ_UNKNOWN, ST_MICROELECTRONICS },

	{ "m25pe80", 0x208014, 64 * 1024, 16, 0, FREQ_UNKNOWN, ST_MICROELECTRONICS },
	{ "m25pe16", 0x208015, 64 * 1024, 32, 0, FREQ_UNKNOWN, ST_MICROELECTRONICS },

	/* Winbond -- w25x "blocks" are 64K, except w25q128 is 4K  */
	{ "w25x05", 0xef3010, 4 * 1024,  16, SECTOR_ERASE_OP20, FREQ_MHZ(104), WINBOND },
	{ "w25x10", 0xef3011, 64 * 1024, 2, 0, FREQ_UNKNOWN, WINBOND },
	{ "w25x20", 0xef3012, 64 * 1024, 4, 0, FREQ_UNKNOWN, WINBOND },
	{ "w25x40", 0xef3013, 64 * 1024, 8, 0, FREQ_UNKNOWN, WINBOND },
	{ "w25x80", 0xef3014, 64 * 1024, 16, 0, FREQ_UNKNOWN, WINBOND },
	{ "w25x16", 0xef3015, 64 * 1024, 32, 0, FREQ_UNKNOWN, WINBOND },
	{ "w25x32", 0xef3016, 64 * 1024, 64, 0, FREQ_UNKNOWN, WINBOND },
	{ "w25x64", 0xef3017, 64 * 1024, 128, 0, FREQ_UNKNOWN, WINBOND },
	{ "w25q64", 0xef4017, 64 * 1024, 128, 0, FREQ_MHZ(80), WINBOND },
	{ "w25q128", 0xef4018, 64 * 1024, 256, 0, FREQ_MHZ(104), WINBOND },

	/* Macronix -- MX25L "blocks" are 64K, except mx25l12836e and mx25l25635 are 4K */
	{ "mx25l512e", 0xc22010, 4 * 1024, 16, SECTOR_ERASE_OP20, FREQ_MHZ(104), MACRONIX },
	{ "mx25l1606e", 0xc22015, 64 * 1024, 32, 0, FREQ_MHZ(50), MACRONIX },
	{ "mx25l6405", 0xc22017, 64 * 1024, 128, 0, FREQ_MHZ(50), MACRONIX },
	{ "mx25l12836e", 0xc22018, 64 * 1024, 256, 0, FREQ_MHZ(104), MACRONIX },
	{ "mx25l25635f", 0xc22019, 64 * 1024, 512, 0, FREQ_MHZ(104), MACRONIX },
	{ "mx25l51245g", 0xc2201A, 64 * 1024, 1024, 0, FREQ_MHZ(104), MACRONIX },

	/* ESMT -- F25L "blocks are 64K, "sectors" are 4KiB */
	{ "f25l05pa", 0x8c3010, 4 * 1024, 16, SECTOR_ERASE_OP20, FREQ_MHZ(50), ESMT },
	{ "f25l64qa", 0x8c4117, 64 * 1024, 128, 0, FREQ_MHZ(50), ESMT },

	/* EON -- EN25Q "blocks are 64K, "sectors" are 4KiB */
	{ "en25f05", 0x1c3110, 4 * 1024, 16, SECTOR_ERASE_OP20, FREQ_MHZ(100), EON },
	{ "en25q64", 0x1c3017, 64 * 1024, 128, 0, FREQ_MHZ(104), EON },
	{ "en25q128", 0x1c3018, 64 * 1024, 256, 0, FREQ_MHZ(104), EON },

	/* GD -- GD25Q "blocks are 64K, "sectors" are 4KiB */
	{ "qd25q512", 0xc84010, 4 * 1024, 16, SECTOR_ERASE_OP20, FREQ_MHZ(104), GD },
	{ "gd25q128", 0xc84018, 64 * 1024, 256, 0, FREQ_MHZ(104), GD },

};

#endif // #ifndef __SPI_FLASH_DATA_H

