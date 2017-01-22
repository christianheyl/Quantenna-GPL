/**
  Copyright (c) 2008 - 2015 Quantenna Communications Inc
  All Rights Reserved

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 **/

#ifndef _HAPR_TYPES_H_
#define _HAPR_TYPES_H_

#include "include/qlink_protocol.h"
#include <stdint.h>
#include <stddef.h>
#include <endian.h>
#include <byteswap.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define le_to_host16(n) ((__u16) (__le16) (n))
#define host_to_le16(n) ((__le16) (__u16) (n))
#define be_to_host16(n) bswap_16((__u16) (be16) (n))
#define host_to_be16(n) ((__be16) bswap_16((n)))
#define le_to_host32(n) ((__u32) (__le32) (n))
#define host_to_le32(n) ((__le32) (__u32) (n))
#define be_to_host32(n) bswap_32((__u32) (__be32) (n))
#define host_to_be32(n) ((__be32) bswap_32((n)))
#define le_to_host64(n) ((__u64) (__le64) (n))
#define host_to_le64(n) ((__le64) (__u64) (n))
#define be_to_host64(n) bswap_64((__u64) (__be64) (n))
#define host_to_be64(n) ((__be64) bswap_64((n)))
#elif __BYTE_ORDER == __BIG_ENDIAN
#define le_to_host16(n) bswap_16(n)
#define host_to_le16(n) bswap_16(n)
#define be_to_host16(n) (n)
#define host_to_be16(n) (n)
#define le_to_host32(n) bswap_32(n)
#define host_to_le32(n) bswap_32(n)
#define be_to_host32(n) (n)
#define host_to_be32(n) (n)
#define le_to_host64(n) bswap_64(n)
#define host_to_le64(n) bswap_64(n)
#define be_to_host64(n) (n)
#define host_to_be64(n) (n)
#else
#error Could not determine CPU byte order
#endif

#define hapr_offsetof(type, member) ((size_t)&((type*)0)->member)

#define hapr_containerof(ptr, type, member) ((type*)((char*)(ptr) - hapr_offsetof(type, member)))

#endif
