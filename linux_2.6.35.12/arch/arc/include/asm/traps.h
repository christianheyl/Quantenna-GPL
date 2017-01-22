/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Exception Cause Code Definitions
 *
 * Rahul Trivedi:  Codito Technologies 2004
 */

/* Instruction Error Cause Code Values */
#define INSTRUCTION_ERROR    0x02
#define ILLEGAL_INST     0x0000
#define ILLEGAL_INST_SEQ 0x0100

/* Machine Check Exception Cause Code Values */
#define MACHINE_CHECK        0x20
#define DOUBLE_FAULT         0x0000
#define TLB_OVERLAP_ERR      0x0100
#define FATAL_TLB_ERR        0x0200
#define FATAL_CACHE_ERR      0x0300
#define KERNEL_DATA_MEM_ERR  0x0400
#define D_FLUSH_MEM_ERR      0x0500
#define INST_FETCH_MEM_ERR   0x0600

/* Privilege Violation Exception Cause Code Values */
#define PRIVILEGE_VIOL  0x0000
#define DISABLED_EXTN   0x0100
#define ACTIONPOINT_HIT 0x0200

/* Protection Violation Exception Cause Code Values */
#define PROTECTION_VIOL           0x23   // Protection violation vector
#define INST_FETCH_PROT_VIOL      0x0000
#define LOAD_PROT_VIOL            0x0100
#define STORE_PROT_VIOL           0x0200
#define XCHG_PROT_VIOL            0x0300
#define MISALIGNED_DATA           0x0400

/* Data TLB Miss Exception Cause Code Values */
#define DATA_TLB_MISS  0x22
#define DATA_TLB_LOAD  0x0100
#define DATA_TLB_STORE 0x0200
#define DATA_TLB_XCHG  0x0300

#define DTLB_LD_MISS_BIT	8
#define DTLB_ST_MISS_BIT	9
