/*
 *  host/umsdl.c
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
 * Common definitions for the umsdl and bin2ums applications.
 */
 
#ifndef _HOST_UMSDL_H
#define _HOST_UMSDL_H

/* Special marker and escape characters in the serial download stream */
#define FLAG_CHAR (0x80)
#define ESC_CHAR (0x10)
#define PACKET_ACK (0x2e)
#define PACKET_NAK (0x78)

/* Programs downloaded serially and run can signal to the download
 * program that they have finished by sending an un-escaped Ctrl-Z
 * back on the serial line.  The download program will then continue
 * with other downloads it may have.
 * If the program fails, then it returns NAK (ASCII 21) and the download
 * will terminate.
 */
#define END_OF_PROGRAM_OK (26)
#define END_OF_PROGRAM_FAIL (21)

/* Frame specific definitions. The frame format is
 *
 * [FLAG_CHAR][header][data]
 *
 * The flag character acts as a frame delimiter.  If it appears in the
 * header or data then it is escaped to a two byte sequence using ESC_CHAR
 * followed by ~FLAG_CHAR.  Escape characters are sent as <ESC_CHAR><~ESC_CHAR>.
 *
 * The header structure is defined by umsdl_hdr and contains the first four
 * bytes of any data.  If the frame contains more than four bytes then an 
 * optional [data] section is added which contains 4 to UMS_MAX_DATA bytes,
 * is a multiple of 4 bytes in size and is followed by a CRC-8 byte checksum
 * (of the bytes in the data section only).  Any trailing bytes are handled by
 * generating an additional frame.
 * If a frame contains 4 or more bytes then these are written as words on
 * the target system.  If the frame contains fewer than 4 bytes then they
 * are written as bytes. 
 * The target responds with one of three characters.  PACKET_ACK is sent when
 * a frame (header & optional data) has been received correctly.  PACKET_NAK
 * is sent if the header or data fails the CRC check, or if the header length
 * field is > 4 but not a multiple of 4 bytes.  FLAG_NAK is sent if the target
 * is expecting the start of a frame FLAG, but got some other character.
 * All responses are sent immediately the error is detected, so the timeout
 * on the sender for lost responses can be very short (i.e. ~ 2 characters).
 *
 */

typedef unsigned char u8;
typedef unsigned long u32;

/* Header format of each frame, preceeded by a FLAG_CHAR */
struct umsdl_hdr {
	u8 addr[4]; /* Target physical address, addr[0]=lsb */
	u8 length; /* Total bytes in header and any data section */
	u8 data[4]; /* First 4 bytes of data, data[0] = first byte */
	u8 crc; /* CRC-8 checksum of preceeding header bytes */
};

/* The maximum number of bytes in the data section of the frame */
#define UMS_MAX_DATA (248)

int ums_exec(FILE *out, u32 address);
int ums_single_write(FILE *out, u32 address, int len, u32 data);
int bin2ums(FILE *in, FILE *out, u32 addr);

#endif
