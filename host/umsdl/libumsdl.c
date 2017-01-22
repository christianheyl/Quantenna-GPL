/*
 *  host/libumsdl.c
 *
 *  Copyright (c) Quantenna Communications Incorporated 2007.
 *  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * This library contains the core functions for converting binary data
 * into UMS serial download data packets.  It is used by the umsdl and
 * bin2ums programs.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "umsdl.h"

static u8 crc_table[] = {
	0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
	0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
	0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
	0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
	0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
	0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
	0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
	0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
	0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
	0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
	0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
	0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
	0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
	0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
	0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
	0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
	0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
	0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
	0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
	0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
	0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
	0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
	0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
	0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
	0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
	0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
	0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
	0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
	0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
	0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
	0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
	0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3,
};

static u8 crc8(u8 ch)
{
	return crc_table[ch];
}

static int outchar(FILE *out, u8 ch)
{
	int rc;
	
	rc = fputc(ch, out);
	if (rc == EOF) {
		return 0;
	}
	return 1;
}

static int esc_outchar(FILE *out, u8 ch, u8 *pcrc)
{
	/* The FLAG character delimits frames and should not appear
	 * in the general data.  FLAG characters in the data are
	 * sent as <ESC><~FLAG> and ESC characters are sent as <ESC><~ESC>.
	 */
	int rc = 0;

	if ((ch == FLAG_CHAR) || (ch == ESC_CHAR)) {
		rc = outchar(out, ESC_CHAR) && outchar(out, ch ^ 0xff);
	} else {
		rc = outchar(out, ch);
	}
	if (pcrc) {
		*pcrc = crc8(*pcrc ^ ch);
	}
	return rc;
}

static int write_header(FILE *out, u32 addr, int len, u8 *pdata)
{
	u8 crc;
	int i, rc;

	if (len > 0 && !pdata) {
		return 0;
	}
	
	crc = 0;
	rc =	outchar(out, FLAG_CHAR) &&	
		esc_outchar(out, (u8)addr, &crc) &&
		esc_outchar(out, (u8)(addr >> 8), &crc) &&
		esc_outchar(out, (u8)(addr >> 16), &crc) &&
		esc_outchar(out, (u8)(addr >> 24), &crc) &&
		esc_outchar(out, (u8)len, &crc);
	
	if (!rc) {
		return rc;
	}
	
	for (i = 0; i < len && i < 4; i++) {
		if (!(rc = esc_outchar(out, *pdata++, &crc))) {
			return rc;
		}
	}
	for (; i < 4; i++) {
		if (!(rc = esc_outchar(out, 0, &crc))) {
			return rc;
		}
	}
	return esc_outchar(out, crc, NULL);
}

static int write_data(FILE *out, u8 *pc, int len)
{
	int i, rc;
	u8 dcrc;
	
	dcrc = 0;
	rc = 1;
	
	for (i = 0; (i < len) && rc; i++) {
		rc = esc_outchar(out, *pc++, &dcrc);
	}
	if (len > 0 && rc) {
		rc = esc_outchar(out, dcrc, NULL);
	}
	return rc;
}

int ums_exec(FILE *out, u32 address)
{
	/* Create a "jump to address" command frame */
	return write_header(out, address, 0, NULL);
}

int ums_single_write(FILE *out, u32 addr, int len, u32 data)
{
	/* Create a "write 8/16/32 bits" command frame */
	u8 data1[4];
	
	if (len < 1 || len > 4 || len == 3) {
		return 0;
	}
	
	data1[0] = (unsigned char)data;
	data1[1] = (unsigned char)(data >> 8);
	data1[2] = (unsigned char)(data >> 16);
	data1[3] = (unsigned char)(data >> 24);
	return write_header(out, addr, len, &data1[0]);
}

int bin2ums(FILE *in, FILE *out, u32 addr)
{
	u8 data1[4];
	u8 data2[UMS_MAX_DATA];
	int ch, rc;
	int pos, trailing;

	rc = 1;
	pos = 0;
	
	do {
		ch = fgetc(in);
		if (ch != EOF) {
			if (pos < 4) {
				data1[pos] = (u8)ch;
			} else {
				data2[pos - 4] = (u8)ch;
			}
			pos++;
			if (pos == UMS_MAX_DATA + 4) {
				/* A full buffer to go */
				rc = write_header(out, addr, pos, &data1[0]) &&
					write_data(out, &data2[0], pos - 4);
				addr += pos;
				pos = 0;
			}
		} else {
			/* Run out of data - write what's left */
			if (pos > 4) {
				trailing = pos & 3;
				pos = pos & ~3;
				rc = write_header(out, addr, pos, &data1[0]) &&
					write_data(out, &data2[0], pos - 4);
				addr += pos;
				if (trailing > 0 && rc) {
					rc = write_header(out, addr, trailing,
								&data2[pos - 4]);
				}
			} else if (pos > 0) {
				rc = write_header(out, addr, pos, &data1[0]);
			}
		}
	} while (ch != EOF && rc);
	return rc;
}
