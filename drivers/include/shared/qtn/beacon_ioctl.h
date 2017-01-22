/*
 * Copyright (c) 2015 Quantenna Communications, Inc.
 * All rights reserved.
 */

#ifndef __BEACON_IOCTL_H__
#define __BEACON_IOCTL_H__
/*
#define LHOST_DEBUG_BEACON
#define MUC_DEBUG_BEACON
*/

#define BEACON_PARAM_SIZE		512
/*
 * A general ie descriptor shared between sender (LHOST) and receiver (MuC).
 * To avoid issues of alignment compatibility between different hosts, all fields has 32bits
 * aligned.
 */
struct beacon_shared_ie_t
{
	dma_addr_t	buf;			/* MuC reference to the ie buffer */
	uint8_t *	lhost_buf;		/* LHOST reference to the ie buffer */
	uint32_t	size;			/* total length of ie including id + len */
	uint32_t	next_muc_addr;		/* next ie descriptor address presented in MuC addr mapping */
	struct		beacon_shared_ie_t *next;	/* next ie descriptor */
};
#endif /* __BEACON_IOCTL_H__ */
