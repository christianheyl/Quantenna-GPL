/*
 *(C) Copyright 2014 Quantenna Communications Inc.
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

#ifndef __RUBY_SPI_API_H__
#define __RUBY_SPI_API_H__

/*
 * Swap bytes
 */

#define SWAP32(x)	((((x) & 0x000000ff) << 24)  | \
			(((x)  & 0x0000ff00) << 8)   | \
			(((x)  & 0x00ff0000) >> 8)   | \
			(((x)  & 0xff000000) >> 24))


#define SPI_WR_IN_PROGRESS	(BIT(0))
#define SPI_SR_QUAD_MODE	(BIT(6))
#define SPI_SCUR_WPSEL		(BIT(7))
#define SPI_PROTECTION		(0x3c)
#define SPI_WPS_SELECT		(0x4)
#define SPI_SECTOR_SIZE_4K	4
#define ADDRESS_MASK		(0x0fffffff)
#define SPI_WRITE_DELAY         (9)
#define SPI_FLASH_ADDR		(0x90000000)
#define SECTOR_MASK		(0x01ffff00)
#define SECTOR_ERASE_WAIT_TIME	(90)
#define CHIP_ERASE_WAIT_TIME	(50)
#define SPI_WRITE_TIMEOUT       (1) /*sec*/
#define SPI_ERASE_TIMEOUT       (5) /*sec*/
#define SPI_UNLOCK_TIMEOUT      (5) /*sec*/
#define MX_25L12805_ID		(0xc22018)
#define MX_25L25635_ID		(0xc22019)
#define MX_25L6405_ID		(0xc22017)
#define M25P32_ID		(0x202016)
#define W25Q128_ID		(0xef4018)
#define	MX25L512E		(0xc22010)
#define S25FL129P		(0x012018)
#define SPI_SECTOR_64K		(64 * 1024)
#define SPI_SECTOR_COUNT_256	(256)
#define SPI_SECTOR_4K		(4 * 1024)
#define SPI_SECTOR_COUNT_4K	(4 * 1024)
#define SPI_SECTOR_INDEX	(16)
#define SPI_SR_QUAD_MODE_MASK(X)	(((X) & 0xfffff0ff) | (2<<8))
#define SPI_SR_SINGLE_MODE_MASK(X)	((X) & 0xfffff0ff)
#define RUBY_SPI_READ_SCUR_MASK(X)	(((X) & 0xffffff00) | 2)
#define RUBY_SPI_READ_DPB_MASK(X)	(((X) & 0xffffff00) | 0x86)
#define RUBY_SPI_GBLOCK_LOCK_MASK(X)	(((X) & 0xffffff00) | 0x1)
#define RUBY_SPI_GBLOCK_UNLOCK_MASK(X)	(((X) & 0xffffff00) | 0x1)
#define RUBY_SPI_WRITE_PRO_SEL_MASK(X)	(((X) & 0xffffff00) | 1)
#define RUBY_SPI_WRITE_WPS_SEL_MASK(X)	(((X) & 0xffffff00) | 2)   /* writing 2 bytes */
#define RUBY_SPI_WRITE_DPB_MASK(X)	(((X) & 0xffffff00) | 0x86)
#define RUBY_SPI_WRITE_IBUP_MASK(X)	(((X) & 0xffffff00) | 0x06)
#define RUBY_SPI_WRITE_WPS_MASK(X)	(((X) & 0xffffff00) | 0x64)
#define RUBY_SPI_READ_ID_MASK		(0xffffff)
#define RUBY_SPI_READ_STATUS_MASK	(0xff)
#define SECTOR_ERASE_OP20		(0x02)
#define SPI_WPS_ENABLE			(0x00640000)
#define SPI_PROTECT_MODE		"spi_protect"
#define SPI_PROTECT_MODE_STR		17
#define SPI_PROTECT_MODE_ENABLE		"enable"
#define SPI_PROTECT_MODE_FLAG_DISABLE	1
#define SPI_PROTECT_MODE_FLAG_ENABLE	0
/*
*
* Ruby uses 3 msb bytes to form addresses.
* Topaz uses all 4 bytes, just skip first msb if in 3-bytes address mode.
*
*/
#define SPI_MEM_ADDR(addr)      (((addr) & 0x00FFFFFF))
#define SPI_MEM_ADDR_4B(addr)	(((addr) & 0xFFFFFFFF))

enum SPI_TYPES{
	NOT_SUPPORTED,
	ATMEL,
	SPANSION,
	SST,
	ST_MICROELECTRONICS,
	WINBOND,
	MACRONIX,
	ESMT,
	EON,
	MICRON,
	GD
};

struct flash_info {
	char *name;
	/* JEDEC id zero means "no ID" (most older chips); otherwise it has
	 * a high byte of zero plus three data bytes: the manufacturer id,
	 * then a two byte device id.
	 */
	u32 jedec_id;

	/* The size listed here is what works with OPCODE_SE, which isn't
	 * necessarily called a "sector" by the vendor.
	 */
	unsigned sector_size;
	u16 n_sectors;

	u16 flags;

	unsigned freq;
	enum SPI_TYPES single_unprotect_mode;
};


#endif
