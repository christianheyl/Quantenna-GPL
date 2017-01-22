/*
 * (C) Copyright 2012 Quantenna Communications Inc.
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

#ifndef __TOPAZ_HBM_CPUIF_PLATFORM_H
#define __TOPAZ_HBM_CPUIF_PLATFORM_H

#include "mproc_sync.h"
#include "qtn_buffers.h"
#include "qtn_arc_processor.h"

/**
 * HBM Requestors
 * 0	- LHost
 * 1	- MuC
 * 2	- emac0	hw wired
 * 3	- emac1	hw wired
 * 4	- wmac	hw wired
 * 5	- tqe	hw wired
 * 6	- AuC
 * 7	- DSP
 * 8	- PCIE(?)
 *
 * Note: qdrv_pktlogger_get_hbm_stats must be updated if this list is changed.
 */
#define TOPAZ_HBM_REQUESTOR_NAMES	{ "lhost", "muc", "emac0", "emac1", "wmac", "tqe", "AuC", "DSP", "PCIe" }

#if defined(__linux__)
	#define TOPAZ_HBM_LOCAL_CPU	0
	#define topaz_hbm_local_irq_save	local_irq_save
	#define topaz_hbm_local_irq_restore	local_irq_restore
#elif defined(MUC_BUILD)
	#define	TOPAZ_HBM_LOCAL_CPU	1
	#define topaz_hbm_local_irq_save(x)	do { (x) = _save_disable(); } while(0)
	#define topaz_hbm_local_irq_restore(x)	do { _restore_enable((x)); } while(0)
#elif defined(AUC_BUILD)
	#define	TOPAZ_HBM_LOCAL_CPU	6
	#define topaz_hbm_local_irq_save(x)	do { (void)(x); } while(0)
	#define topaz_hbm_local_irq_restore(x)	do { (void)(x); } while(0)
#elif defined(DSP_BUILD)
	#define	TOPAZ_HBM_LOCAL_CPU	7
#else
	#error No TOPAZ_HBM_LOCAL_CPU set
#endif

#define TOPAZ_HBM_NUM_POOLS			4

#ifdef QTN_RC_ENABLE_HDP
#define TOPAZ_HBM_PAYLOAD_HEADROOM		128
#else
#define TOPAZ_HBM_PAYLOAD_HEADROOM		64
#endif

#define TOPAZ_HBM_PAYLOAD_END_GUARD_SIZE	32		/* equal to cacheline size */
#define TOPAZ_HBM_BUF_GUARD_MAGIC		0xDEADBEEF
#define TOPAZ_HBM_BUF_CORRUPTED_MAGIC		0xDEADBEEF
#define TOPAZ_HBM_BUF_PAYLOAD_POISON		0xA5
#define TOPAZ_HBM_BUF_PAYLOAD_POISON32		(TOPAZ_HBM_BUF_PAYLOAD_POISON |                \
							(TOPAZ_HBM_BUF_PAYLOAD_POISON << 8) |  \
							(TOPAZ_HBM_BUF_PAYLOAD_POISON << 16) | \
							(TOPAZ_HBM_BUF_PAYLOAD_POISON << 24))

#define TOPAZ_HBM_BUF_EXTERNAL_META		1	/* move meta data outside of buffer */
#define TOPAZ_HBM_BUF_WMAC_RX_QUARANTINE	1	/* quarantine wmac rx buffers when deliver important packets */

#if defined(CONFIG_TOPAZ_PCIE_TARGET) || defined(CONFIG_TOPAZ_DBDC_HOST)
/*
 * Checking emac rx pool buffers lead to performance impact in PCIe, so limit
 * the check for wmac rx pool only.
 */
#define TOPAZ_HBM_BUF_MAGIC_CHK_ALLPOOL		0
#else
#define TOPAZ_HBM_BUF_MAGIC_CHK_ALLPOOL		1
#endif

#define TOPAZ_HBM_DEBUG_DUMP			0	/* inline debug dump functions */
#define TOPAZ_HBM_DEBUG_STAMPS			0	/* extra trace info in meta data */

#if TOPAZ_HBM_DEBUG_STAMPS
#define TOPAZ_HBM_OWNER_MUC_FREE	0x9
#define TOPAZ_HBM_OWNER_AUC_FREE	0xa
#define TOPAZ_HBM_OWNER_LH_TX_TQE	0xb
#define TOPAZ_HBM_OWNER_LH_RX_TQE	0xc
#define TOPAZ_HBM_OWNER_LH_RX_MBOX	0xd
#define TOPAZ_HBM_OWNER_INIT		0xe
#define TOPAZ_HBM_OWNER_FREE		0xf
#endif

#define TOPAZ_HBM_ERR_NONE		0
#define TOPAZ_HBM_ERR_PTR		(-1)
#define TOPAZ_HBM_ERR_MAGIC		(-2)
#define TOPAZ_HBM_ERR_TAILGUARD		(-3)

/*
 * The usage of the HBM buffer headroom and meta data, depending on TOPAZ_HBM_BUF_EXTERNAL_META:
 * 1. When it is 1, all the below meta data except magic in head and tail, and pointer to meta are
 * in separate memory region, outside of the buffer.
 * 2. When it is 0, all the below meta data are in HBM buffer headroom.
 * To avoid define different offset values for above 2 cases, we use the same definition. The only
 * difference is where the meta data is stored.
 */
enum QTN_HBM_BUF_HEADROOM_OFFSET {
	HBM_HR_OFFSET_ENQ_CNT = 1,	/* in word */
	HBM_HR_OFFSET_FREE_CNT = 2,	/* in word */
	HBM_HR_OFFSET_OCS_FRM_ID = 3,	/* in word */
	HBM_HR_OFFSET_FREE_JIFF = 4,	/* debugging; jiffies of last free. leaked buffer heuristic */
	HBM_HR_OFFSET_OWNER = 5,	/* debugging; buffer owner */
	HBM_HR_OFFSET_SIZE = 6,		/* debugging; buffer size */
	HBM_HR_OFFSET_STATE = 7,	/* state about the buffer */
	HBM_HR_OFFSET_META_PTR = 8,	/* pointer and back pointer bwtween buffer and meta, bus addr */
	HBM_HR_OFFSET_MAGIC = 9,	/* the magic, keep it biggest thus first in headroom */
	HBM_HR_OFFSET_MAX = HBM_HR_OFFSET_MAGIC,
};

#define QTN_HBM_SANITY_BAD_HR_MAGIC	BIT(0)
#define QTN_HBM_SANITY_BAD_TAIL_GUARD	BIT(1)
#define QTN_HBM_SANITY_BAD_ALREADY	BIT(31)

#define TOPAZ_HBM_BUF_DUMP_MAX		0xFFFF
#define TOPAZ_HBM_BUF_DUMP_DFT		512U
#define TOPAZ_HBM_BUF_DUMP_TAIL_DFT	2048U

#if TOPAZ_HBM_DEBUG_DUMP
#if defined(DSP_BUILD)
#elif defined(AUC_BUILD)
	#define CPU_PRINT auc_os_printf
	#define CPU_INV_DCACHE_RANGE(_v, _range)
	#define CPU_HZ		AUC_CPU_TIMER_HZ
#elif defined(MUC_BUILD)
	#define CPU_PRINT uc_printk
	#define CPU_INV_DCACHE_RANGE invalidate_dcache_range_safe
	#define CPU_HZ		HZ
#else
	#ifdef __KERNEL__
		#include <qtn/dmautil.h>
		#define CPU_PRINT printk
		#define CPU_INV_DCACHE_RANGE inv_dcache_sizerange_safe
		#define CPU_HZ		HZ
	#endif
#endif
#endif // TOPAZ_HBM_DEBUG_DUMP

RUBY_INLINE uint32_t topaz_hbm_buf_offset_from_start_bus(void *buf_bus, uint8_t pool, uint8_t is_aligned)
{
	if (is_aligned) {
		return ((uint32_t)buf_bus) & (TOPAZ_HBM_BUF_ALIGN - 1);
	}
	if (pool == TOPAZ_HBM_BUF_WMAC_RX_POOL) {
		return (((uint32_t)buf_bus) - TOPAZ_HBM_BUF_WMAC_RX_BASE) % TOPAZ_HBM_BUF_WMAC_RX_SIZE;
	} else if (pool == TOPAZ_HBM_BUF_EMAC_RX_POOL) {
		return (((uint32_t)buf_bus) - TOPAZ_HBM_BUF_EMAC_RX_BASE) % TOPAZ_HBM_BUF_EMAC_RX_SIZE;
	} else {
		return 0;
	}
}

RUBY_INLINE int topaz_hbm_buf_identify_buf_bus(const void *buf_bus, uint32_t *sizep, uint32_t *idxp)
{
	if (__in_mem_range((uint32_t)buf_bus, TOPAZ_HBM_BUF_EMAC_RX_BASE, TOPAZ_HBM_BUF_EMAC_RX_TOTAL)) {
		*sizep = TOPAZ_HBM_BUF_EMAC_RX_SIZE;
		*idxp = (((uint32_t)buf_bus) - TOPAZ_HBM_BUF_EMAC_RX_BASE) / TOPAZ_HBM_BUF_EMAC_RX_SIZE;
		return TOPAZ_HBM_BUF_EMAC_RX_POOL;
	} else if (__in_mem_range((uint32_t)buf_bus, TOPAZ_HBM_BUF_WMAC_RX_BASE, TOPAZ_HBM_BUF_WMAC_RX_TOTAL)) {
		*sizep = TOPAZ_HBM_BUF_WMAC_RX_SIZE;
		*idxp = (((uint32_t)buf_bus) - TOPAZ_HBM_BUF_WMAC_RX_BASE) / TOPAZ_HBM_BUF_WMAC_RX_SIZE;
		return TOPAZ_HBM_BUF_WMAC_RX_POOL;
	} else {
		return -1;
	}
}

RUBY_INLINE int topaz_hbm_buf_identify_buf_virt(const void *buf_virt, uint32_t *sizep, uint32_t *idxp)
{
	if (__in_mem_range((uint32_t)buf_virt, TOPAZ_HBM_BUF_EMAC_RX_BASE_VIRT, TOPAZ_HBM_BUF_EMAC_RX_TOTAL)) {
		*sizep = TOPAZ_HBM_BUF_EMAC_RX_SIZE;
		*idxp = (((uint32_t)buf_virt) - TOPAZ_HBM_BUF_EMAC_RX_BASE_VIRT) / TOPAZ_HBM_BUF_EMAC_RX_SIZE;
		return TOPAZ_HBM_BUF_EMAC_RX_POOL;
	} else if (__in_mem_range((uint32_t)buf_virt, TOPAZ_HBM_BUF_WMAC_RX_BASE_VIRT, TOPAZ_HBM_BUF_WMAC_RX_TOTAL)) {
		*sizep = TOPAZ_HBM_BUF_WMAC_RX_SIZE;
		*idxp = (((uint32_t)buf_virt) - TOPAZ_HBM_BUF_WMAC_RX_BASE_VIRT) / TOPAZ_HBM_BUF_WMAC_RX_SIZE;
		return TOPAZ_HBM_BUF_WMAC_RX_POOL;
	} else {
		return -1;
	}
}

RUBY_INLINE int topaz_hbm_buf_ptr_valid(const void *buf_virt)
{
	uint32_t offset;

	if (__in_mem_range((uint32_t)buf_virt, TOPAZ_HBM_BUF_EMAC_RX_BASE_VIRT, TOPAZ_HBM_BUF_EMAC_RX_TOTAL)) {
		offset = (((uint32_t)buf_virt) - TOPAZ_HBM_BUF_EMAC_RX_BASE_VIRT) % TOPAZ_HBM_BUF_EMAC_RX_SIZE;
	} else if (__in_mem_range((uint32_t)buf_virt, TOPAZ_HBM_BUF_WMAC_RX_BASE_VIRT, TOPAZ_HBM_BUF_WMAC_RX_TOTAL)) {
		offset = (((uint32_t)buf_virt) - TOPAZ_HBM_BUF_WMAC_RX_BASE_VIRT) % TOPAZ_HBM_BUF_WMAC_RX_SIZE;
	} else {
		return 0;
	}

	return (offset == TOPAZ_HBM_PAYLOAD_HEADROOM);
}

#if TOPAZ_HBM_BUF_EXTERNAL_META
RUBY_INLINE void* topaz_hbm_buf_get_meta(const void *buf_virt)
{
	uint32_t idx;

	if (__in_mem_range((uint32_t)buf_virt, TOPAZ_HBM_BUF_EMAC_RX_BASE_VIRT, TOPAZ_HBM_BUF_EMAC_RX_TOTAL)) {
		idx = (((uint32_t)buf_virt) - TOPAZ_HBM_BUF_EMAC_RX_BASE_VIRT) / TOPAZ_HBM_BUF_EMAC_RX_SIZE;
		return (void*)(TOPAZ_HBM_BUF_META_EMAC_RX_BASE_VIRT + TOPAZ_HBM_BUF_META_SIZE +
			idx * TOPAZ_HBM_BUF_META_SIZE);
	} else if (__in_mem_range((uint32_t)buf_virt, TOPAZ_HBM_BUF_WMAC_RX_BASE_VIRT, TOPAZ_HBM_BUF_WMAC_RX_TOTAL)) {
		idx = (((uint32_t)buf_virt) - TOPAZ_HBM_BUF_WMAC_RX_BASE_VIRT) / TOPAZ_HBM_BUF_WMAC_RX_SIZE;
		return (void*)(TOPAZ_HBM_BUF_META_WMAC_RX_BASE_VIRT + TOPAZ_HBM_BUF_META_SIZE +
			idx * TOPAZ_HBM_BUF_META_SIZE);
	} else {
		return NULL;
	}
}

/*
 * A fast way to get the meta address.
 * However this assume the meta address in buffer headroom is not corrupted as long as the magic in
 * buffer headroom is not corrupted. So this is not 100% correct, but it can be used to speed up for
 * some non-real-world test.
 */
RUBY_INLINE void* topaz_hbm_buf_get_meta_fast(const void *buf_virt)
{
	uint32_t *magicp = (uint32_t*)buf_virt - HBM_HR_OFFSET_MAGIC;

	/* assume ptr is valid when magic is not corrupted */
	if (likely(arc_read_uncached_32(magicp) == TOPAZ_HBM_BUF_GUARD_MAGIC)) {
		uint32_t *meta_ptr_p = (uint32_t*)buf_virt - HBM_HR_OFFSET_META_PTR;
		return (void*)bus_to_virt(arc_read_uncached_32(meta_ptr_p));
	}

	return topaz_hbm_buf_get_meta(buf_virt);
}
#else
#define topaz_hbm_buf_get_meta_fast(_buf_virt)	(_buf_virt)
#define topaz_hbm_buf_get_meta(_buf_virt)	(_buf_virt)
#endif

RUBY_INLINE uint32_t topaz_hbm_buf_offset_from_start_virt(void *buf_virt, uint8_t pool, uint8_t is_aligned)
{
	return topaz_hbm_buf_offset_from_start_bus((void *)virt_to_bus(buf_virt), pool, is_aligned);
}

RUBY_INLINE void *topaz_hbm_payload_store_align_bus(void *buf_bus, uint8_t pool, uint8_t is_aligned)
{
	return ((uint8_t *)buf_bus) - topaz_hbm_buf_offset_from_start_bus(buf_bus, pool, is_aligned)
			+ TOPAZ_HBM_PAYLOAD_HEADROOM;
}

RUBY_INLINE void *topaz_hbm_payload_store_align_virt(void *buf_virt, uint8_t pool, uint8_t is_aligned)
{
	return ((uint8_t *)buf_virt) - topaz_hbm_buf_offset_from_start_virt(buf_virt, pool, is_aligned)
			+ TOPAZ_HBM_PAYLOAD_HEADROOM;
}

RUBY_INLINE unsigned long topaz_hbm_payload_store_align_from_index(int8_t pool, uint16_t index)
{
	if (pool == TOPAZ_HBM_BUF_EMAC_RX_POOL && index < TOPAZ_HBM_BUF_EMAC_RX_COUNT) {
		return RUBY_DRAM_BEGIN + TOPAZ_HBM_BUF_EMAC_RX_BASE +
			(index * TOPAZ_HBM_BUF_EMAC_RX_SIZE) + TOPAZ_HBM_PAYLOAD_HEADROOM;
	} else if (pool == TOPAZ_HBM_BUF_WMAC_RX_POOL && index < TOPAZ_HBM_BUF_WMAC_RX_COUNT) {
		return RUBY_DRAM_BEGIN + TOPAZ_HBM_BUF_WMAC_RX_BASE +
			(index * TOPAZ_HBM_BUF_WMAC_RX_SIZE) + TOPAZ_HBM_PAYLOAD_HEADROOM;
	}
	return 0;
}

RUBY_INLINE long topaz_hbm_payload_buff_ptr_offset_bus(void *buf_bus, uint8_t pool, void *align_bus)
{
	unsigned long buf_align = (unsigned long)topaz_hbm_payload_store_align_bus(
			align_bus ? align_bus : buf_bus, pool, !!align_bus);

	return buf_align - (unsigned long)buf_bus;
}

RUBY_INLINE long topaz_hbm_payload_buff_ptr_offset_virt(void *buf_virt, uint8_t pool, void *align_virt)
{
	return topaz_hbm_payload_buff_ptr_offset_bus((void *)virt_to_bus(buf_virt), pool,
			align_virt ? (void *)virt_to_bus(align_virt) : NULL);
}

RUBY_INLINE int __topaz_hbm_is_done(void)
{
	return qtn_mproc_sync_mem_read(TOPAZ_HBM_POOL_REQ(TOPAZ_HBM_LOCAL_CPU)) & TOPAZ_HBM_DONE;
}

RUBY_INLINE void __topaz_hbm_release_buf(void *buf, uint8_t pool)
{
	/* assumes previous operations are complete */
	qtn_mproc_sync_mem_write_wmb(TOPAZ_HBM_POOL_DATA(TOPAZ_HBM_LOCAL_CPU),
			(unsigned long) buf);
	qtn_mproc_sync_mem_write_wmb(TOPAZ_HBM_POOL_REQ(TOPAZ_HBM_LOCAL_CPU),
			TOPAZ_HBM_POOL_NUM(pool) | TOPAZ_HBM_RELEASE_BUF);
}

RUBY_INLINE void __topaz_hbm_request_start(uint8_t pool)
{
	qtn_mproc_sync_mem_write_wmb(TOPAZ_HBM_POOL_REQ(TOPAZ_HBM_LOCAL_CPU),
			TOPAZ_HBM_POOL_NUM(pool) | TOPAZ_HBM_REQUEST_BUF);
}

RUBY_INLINE void *__topaz_hbm_rd_buf(uint8_t pool)
{
	/* must be preceded by __topaz_hbm_rd_req, then polling on __topaz_hbm_is_done */
	return (void *) qtn_mproc_sync_mem_read(TOPAZ_HBM_POOL_DATA(TOPAZ_HBM_LOCAL_CPU));
}

RUBY_INLINE void __topaz_hbm_wait(void)
{
	unsigned int timeouts = 0;
	unsigned int timeout_reached = 0;

	while (1) {
		int i;
		if (__topaz_hbm_is_done()) {
			return;
		}

		/* busy wait until buffer is available */
		for (i = 0; i < 10; i++) {
#if defined(AUC_BUILD) && defined(_ARC)
			/*
			 * This is a workaround for MetaWare C Compiler v7.4.0
			 * bug in Zero-Delay Loop code generation for ARC 600 family cores.
			 * Without it the LP_START register will be written to, two
			 * instruction before the end address of the loop, but at least three
			 * instructions are required according to the ARC ISA Programmer's
			 * Reference.
			 */
			_nop();
			_nop();
			_nop();
#else
			qtn_pipeline_drain();
#endif
		}

		if (unlikely(timeout_reached == 0 && timeouts++ == 1000)) {
			timeout_reached = 1;
			qtn_mproc_sync_log("__topaz_hbm_wait timeout");
		}
	}

	if (unlikely(timeout_reached)) {
		qtn_mproc_sync_log("__topaz_hbm_wait succeeded");
	}
}

RUBY_INLINE int __topaz_hbm_put_buf_nowait(void *buf, uint8_t pool)
{
	if (__topaz_hbm_is_done()) {
		__topaz_hbm_release_buf(buf, pool);
		return 0;
	}
	return -EBUSY;
}

RUBY_INLINE void __topaz_hbm_put_buf(void *buf, uint8_t pool)
{
	__topaz_hbm_wait();
	__topaz_hbm_release_buf(buf, pool);
}

#if defined(MUC_BUILD)
RUBY_INLINE void topaz_hbm_put_buf(void *buf, uint8_t pool)
#else
RUBY_WEAK(topaz_hbm_put_buf) void topaz_hbm_put_buf(void *buf, uint8_t pool)
#endif
{
	unsigned long flags;

	topaz_hbm_local_irq_save(flags);
	__topaz_hbm_put_buf(buf, pool);
	topaz_hbm_local_irq_restore(flags);
}

RUBY_INLINE void *__topaz_hbm_get_buf(uint8_t pool)
{
	__topaz_hbm_wait();
	__topaz_hbm_request_start(pool);
	__topaz_hbm_wait();
	return __topaz_hbm_rd_buf(pool);
}

#if defined(MUC_BUILD)
RUBY_INLINE void *topaz_hbm_get_buf(uint8_t pool)
#else
RUBY_WEAK(topaz_hbm_get_buf) void *topaz_hbm_get_buf(uint8_t pool)
#endif
{
	unsigned long flags;
	void *buf;

	topaz_hbm_local_irq_save(flags);
	buf = __topaz_hbm_get_buf(pool);
	topaz_hbm_local_irq_restore(flags);

	return buf;
}

RUBY_INLINE void topaz_hbm_init(void *pool_list_bus, uint16_t payload_count_s, uint8_t pool, int full)
{
	unsigned long csr;
	const uint16_t payload_count = BIT(payload_count_s);

	qtn_mproc_sync_mem_write_wmb(TOPAZ_HBM_BASE_REG(pool), (unsigned long) pool_list_bus);
	qtn_mproc_sync_mem_write_wmb(TOPAZ_HBM_LIMIT_REG(pool), payload_count);
	qtn_mproc_sync_mem_write_wmb(TOPAZ_HBM_WR_PTR(pool), full ? payload_count : 0);
	qtn_mproc_sync_mem_write_wmb(TOPAZ_HBM_RD_PTR(pool), 0);

	csr = qtn_mproc_sync_mem_read(TOPAZ_HBM_CSR_REG);
	qtn_mproc_sync_mem_write_wmb(TOPAZ_HBM_CSR_REG, csr | TOPAZ_HBM_CSR_Q_EN(pool));
}

RUBY_INLINE uint32_t topaz_hbm_pool_buf_whole_size(int8_t pool)
{
	if (pool == TOPAZ_HBM_BUF_EMAC_RX_POOL) {
		return TOPAZ_HBM_BUF_EMAC_RX_SIZE;
	} else if (pool == TOPAZ_HBM_BUF_WMAC_RX_POOL) {
		return TOPAZ_HBM_BUF_WMAC_RX_SIZE;
	} else {
		return 0;
	}
}

RUBY_INLINE uint32_t topaz_hbm_pool_buf_max_size(int8_t pool)
{
	uint32_t size;

	size = topaz_hbm_pool_buf_whole_size(pool);
	if (!size)
		return 0;

	return size -
		TOPAZ_HBM_PAYLOAD_HEADROOM -
		TOPAZ_HBM_PAYLOAD_END_GUARD_SIZE;
}

RUBY_INLINE int8_t topaz_hbm_pool_valid(int8_t pool)
{
	return pool >= 0 && pool < TOPAZ_HBM_NUM_POOLS;
}

RUBY_INLINE int8_t topaz_hbm_payload_get_pool_bus(const void *buf_bus)
{
	const unsigned long v = (const unsigned long) buf_bus;
	if (__in_mem_range(v, TOPAZ_HBM_BUF_EMAC_RX_BASE, TOPAZ_HBM_BUF_EMAC_RX_TOTAL)) {
		return TOPAZ_HBM_BUF_EMAC_RX_POOL;
	} else if (__in_mem_range(v, TOPAZ_HBM_BUF_WMAC_RX_BASE, TOPAZ_HBM_BUF_WMAC_RX_TOTAL)) {
		return TOPAZ_HBM_BUF_WMAC_RX_POOL;
	} else {
		return -1;
	}
}

RUBY_INLINE int8_t topaz_hbm_payload_get_free_pool_bus(const void *buf_bus)
{
	int8_t orig_pool = topaz_hbm_payload_get_pool_bus(buf_bus);
	if (orig_pool == TOPAZ_HBM_BUF_EMAC_RX_POOL || orig_pool == TOPAZ_HBM_BUF_WMAC_RX_POOL) {
		return TOPAZ_HBM_EMAC_TX_DONE_POOL;
	}
	return -1;
}

RUBY_INLINE void topaz_hbm_put_payload_aligned_bus(void *buf_bus, int8_t pool)
{
	if (likely(topaz_hbm_pool_valid(pool))) {
		topaz_hbm_put_buf(topaz_hbm_payload_store_align_bus(buf_bus, pool, 1), pool);
	}
}

RUBY_INLINE void topaz_hbm_put_payload_realign_bus(void *buf_bus, int8_t pool)
{
	if (likely(topaz_hbm_pool_valid(pool))) {
		topaz_hbm_put_buf(topaz_hbm_payload_store_align_bus(buf_bus, pool, 0), pool);
	}
}

RUBY_INLINE void topaz_hbm_put_payload_aligned_virt(void *buff_virt, int8_t pool)
{
	topaz_hbm_put_payload_aligned_bus((void *) virt_to_bus(buff_virt), pool);
}

RUBY_INLINE void topaz_hbm_put_payload_realign_virt(void *buff_virt, int8_t pool)
{
	topaz_hbm_put_payload_realign_bus((void *) virt_to_bus(buff_virt), pool);
}

#ifdef __KERNEL__
#define topaz_hbm_get_payload_bus(pool)		__topaz_hbm_get_payload_bus((pool), __FILE__, __LINE__, __FUNCTION__)
RUBY_INLINE void *__topaz_hbm_get_payload_bus(int8_t pool, const char *file, const int line, const char *func)
{
	if (likely(topaz_hbm_pool_valid(pool))) {
		return topaz_hbm_get_buf(pool);
	}
	if (printk_ratelimit()) {
		printk("%s:%u%s null buffer from pool %hhd\n",
				file, line, func, pool);
	}
	return NULL;
}

#define topaz_hbm_get_payload_virt(pool)	__topaz_hbm_get_payload_virt((pool), __FILE__, __LINE__, __FUNCTION__)
RUBY_INLINE void *__topaz_hbm_get_payload_virt(int8_t pool, const char *file, const int line, const char *func)
{
	void *buf_bus = topaz_hbm_get_payload_bus(pool);
	if (unlikely(!buf_bus)) {
		if (printk_ratelimit()) {
			printk("%s:%u%s null buffer from pool %hhd\n",
					file, line, func, pool);
		}
		return NULL;
	}
	return bus_to_virt((unsigned long) buf_bus);
}

#else

RUBY_INLINE void *topaz_hbm_get_payload_bus(int8_t pool)
{
	if (likely(topaz_hbm_pool_valid(pool))) {
		return topaz_hbm_get_buf(pool);
	}
	return NULL;
}

RUBY_INLINE void *topaz_hbm_get_payload_virt(int8_t pool)
{
	void *buf_bus = topaz_hbm_get_payload_bus(pool);
	if (unlikely(!buf_bus)) {
		return NULL;
	}
	return bus_to_virt((unsigned long) buf_bus);
}
#endif

RUBY_INLINE int hbm_buf_check_wmac_rx_buf_overrun(void *v, int fix)
{
	uint32_t *guardp;

	/* only check last 4 bytes guard */
	guardp =(uint32_t*)((uint32_t)v + TOPAZ_HBM_BUF_WMAC_RX_SIZE - TOPAZ_HBM_PAYLOAD_HEADROOM - 4);
	if (likely(arc_read_uncached_32(guardp) == TOPAZ_HBM_BUF_GUARD_MAGIC)) {
		return TOPAZ_HBM_ERR_NONE;
	}

	/*
	 * It is best if we do the buffer pointer check first, but as we only do the overrun check after wmac rx,
	 * if it is bad, the memory is already corrupted.
	 */

	if (fix) {
		arc_write_uncached_32(guardp, TOPAZ_HBM_BUF_GUARD_MAGIC);
	}

	return TOPAZ_HBM_ERR_TAILGUARD;
}

RUBY_INLINE int hbm_buf_check_buf_magic(void *v)
{
	uint32_t *magicp = (uint32_t*)v - HBM_HR_OFFSET_MAGIC;

	if (likely(arc_read_uncached_32(magicp) == TOPAZ_HBM_BUF_GUARD_MAGIC)) {
		return TOPAZ_HBM_ERR_NONE;
	}

	return TOPAZ_HBM_ERR_MAGIC;
}

RUBY_INLINE void hbm_buf_fix_buf_magic(void *v)
{
	uint32_t *magicp = (uint32_t*)v - HBM_HR_OFFSET_MAGIC;

	arc_write_uncached_32(magicp, TOPAZ_HBM_BUF_GUARD_MAGIC);
}

#if TOPAZ_HBM_DEBUG_DUMP
/* assume v is 4 bytes aligned */
RUBY_INLINE void topaz_buf_dump_range(const void *v, int len)
{
#if defined(DSP_BUILD)
#elif defined(AUC_BUILD)
	int i;
	const uint32_t *d32;
	int dump_loop;

	d32 = v;
	dump_loop = ((len + 3) >> 2) >> 3;
	for (i = 0; i < dump_loop; i++) {
		CPU_PRINT("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
			d32[0], d32[1], d32[2], d32[3], d32[4], d32[5], d32[6], d32[7]);
		d32 += 8;
	}
#else
	int i;
	const uint8_t *d;

	d = v;
	for (i = 0; i < len; ) {
		if (!(i % 32))
			CPU_PRINT("%08x ", (i - i % 32));
		++i;
		CPU_PRINT("%02x%s", *d++, (i % 32) == 0 ? "\n" : " ");
	}
	CPU_PRINT("\n");
#endif
}

RUBY_INLINE void topaz_hbm_buf_show(void *v, const uint32_t len, const uint32_t tail_len)
{
#if defined(DSP_BUILD)
#else
	const uint32_t *_p = v;
	const uint32_t *_m = topaz_hbm_buf_get_meta(_p);
	const uint32_t *enqueuep = _m - HBM_HR_OFFSET_ENQ_CNT;
	const uint32_t *freep = _m - HBM_HR_OFFSET_FREE_CNT;
	const uint32_t *jiffp = _m - HBM_HR_OFFSET_FREE_JIFF;
	const uint32_t *ownerp = _m - HBM_HR_OFFSET_OWNER;
	const uint32_t *sizep = _m - HBM_HR_OFFSET_SIZE;
	const uint32_t *magicp = _p - HBM_HR_OFFSET_MAGIC;
	const uint32_t ec = arc_read_uncached_32(enqueuep);
	const uint32_t fc = arc_read_uncached_32(freep);
	const uint32_t jc = arc_read_uncached_32(jiffp);
	const uint32_t oc = arc_read_uncached_32(ownerp);
	const uint32_t sz = arc_read_uncached_32(sizep);
	const uint32_t magic = arc_read_uncached_32(magicp);
	uint32_t *guardp;
	uint32_t guard;
	int dump_bytes;
	uint32_t whole_size;
	uint32_t payload_size;
	int pool;
	uint32_t idx;

	pool = topaz_hbm_buf_identify_buf_virt(v, &whole_size, &idx);
	if (pool < 0) {
		return;
	}
	payload_size = whole_size - TOPAZ_HBM_PAYLOAD_HEADROOM;
	CPU_INV_DCACHE_RANGE((void*)v, payload_size);
	dump_bytes = (len == TOPAZ_HBM_BUF_DUMP_MAX) ? payload_size : len;

	/* only check last 4 bytes guard */
	guardp =(uint32_t*)((uint32_t)_p + payload_size - 4);
	guard = arc_read_uncached_32(guardp);

	CPU_PRINT("buf start 0x%x pool %d idx %u size %u dump %u\n",
			(unsigned int)v, pool, idx, whole_size, dump_bytes);
#ifdef __KERNEL__
	CPU_PRINT("%p ec %u fp %u own %08x size %u j %u (%u s ago)\n",
			v, ec, fc, oc, sz, jc, (((uint32_t) jiffies) - jc) / CPU_HZ);
#else
	/* free jiffies is only set by Lhost, so no way to do jiffies diff */
	CPU_PRINT("%p ec %u fp %u own %08x size %u j %u (local j %u)\n",
			v, ec, fc, oc, sz, jc, ((uint32_t) jiffies));
#endif
	if (magic != TOPAZ_HBM_BUF_GUARD_MAGIC) {
		CPU_PRINT("magic %x corrupted\n", magic);
	}
	if (guard != TOPAZ_HBM_BUF_GUARD_MAGIC) {
		CPU_PRINT("guard %x corrupted\n", guard);
	}

	topaz_buf_dump_range(v, dump_bytes);

	if (tail_len) {
		uint32_t tail;

		tail = (uint32_t)v;
		tail += payload_size;
		tail -= tail_len;

		CPU_PRINT("buf tail 0x%x\n", tail);
		topaz_buf_dump_range((void*)tail, tail_len);
	}
#endif
}

/*
 * Full sanity check suitable for all buffers.
 * Debug build use only, not suitable for release because of performance impact.
 */
RUBY_INLINE int hbm_buf_check_sanity(void *v)
{
	const uint32_t *magicp = (uint32_t*)v - HBM_HR_OFFSET_MAGIC;
	uint32_t *_m;
	uint32_t *statep;
	uint32_t state;
	uint32_t magic;
	uint32_t payload_size;
	uint32_t *guardp;
	uint32_t guard;
	uint32_t size = 0;
	uint32_t idx = 0;
	uint32_t bad = 0;
	int pool;

	magic = arc_read_uncached_32(magicp);
	if (unlikely(magic != TOPAZ_HBM_BUF_GUARD_MAGIC)) {
		bad |= QTN_HBM_SANITY_BAD_HR_MAGIC;
	}

	pool = topaz_hbm_buf_identify_buf_virt(v, &size, &idx);
	payload_size = size - TOPAZ_HBM_PAYLOAD_HEADROOM;
	/* only check last 4 bytes guard */
	guardp =(uint32_t*)((uint32_t)v + payload_size - 4);
	guard = arc_read_uncached_32(guardp);
	if (unlikely(guard != TOPAZ_HBM_BUF_GUARD_MAGIC)) {
		bad |= QTN_HBM_SANITY_BAD_TAIL_GUARD;
	}

	if (likely(!bad))
		return 0;

	/* avoid multiple alert */
	_m = topaz_hbm_buf_get_meta(v);
	statep = (uint32_t*)_m - HBM_HR_OFFSET_STATE;
	state = arc_read_uncached_32(statep);
	if ((bad & (~state)) == 0) {
		return (bad | QTN_HBM_SANITY_BAD_ALREADY);
	}

	/* new corruption */
	arc_write_uncached_32(statep, bad);
	CPU_PRINT("ERROR: hbm buffer %x corrupted, pool %d, idx %u\n",
			(unsigned int)v, pool, idx);

	topaz_hbm_buf_show(v, TOPAZ_HBM_BUF_DUMP_DFT, 0);

	/* new corruption of tail guard */
	if ((bad & QTN_HBM_SANITY_BAD_TAIL_GUARD) && !(state & QTN_HBM_SANITY_BAD_TAIL_GUARD)) {
		/* find the corruption extent */
		int i;
		int j = 0;
		int lines = (size * 4) / 16;
		uint32_t pos = 0;
		for (i = 0; i < lines ; i++) {
			for (j = 0; j < 4; j++) {
				pos = (uint32_t)v + i * 16 + j * 4;
				if (*(uint32_t*)pos != (uint32_t)TOPAZ_HBM_BUF_PAYLOAD_POISON32)
					break;
			}
			if (j == 4)
				break;
		}
		CPU_PRINT("guess tail corruption length %d %x\n", (i * 16) + (j * 4), pos);
	}

	return bad;
}
#endif // TOPAZ_HBM_DEBUG_DUMP

#if TOPAZ_HBM_DEBUG_STAMPS
RUBY_INLINE void topaz_hbm_debug_stamp(void *buf, uint8_t port, uint32_t size)
{
	uint32_t *p = buf;
	uint32_t *_m = topaz_hbm_buf_get_meta(p);
	uint32_t *ownerp = _m - HBM_HR_OFFSET_OWNER;
	uint32_t *sizep = _m - HBM_HR_OFFSET_SIZE;

	arc_write_uncached_32(ownerp, (arc_read_uncached_32(ownerp) << 4) | (port & 0xF));
	if (size) {
		arc_write_uncached_32(sizep, size);
	}
}

#else
#define topaz_hbm_debug_stamp(_buf, _port, _size)
#endif /* TOPAZ_HBM_DEBUG_STAMPS */

#endif	/* __TOPAZ_HBM_CPUIF_PLATFORM_H */
