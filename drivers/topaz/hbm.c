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

/*
 * Quantenna HBM skb payload pool
 */
#include <linux/kernel.h>
#include <linux/cache.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/skbuff.h>
#include <qtn/dmautil.h>
#include <asm/io.h>

#include <common/queue.h>

#include <qtn/topaz_hbm.h>
#include <qtn/dmautil.h>

#include <net80211/if_ethersubr.h>

#define isspace(c) ((((c) == ' ') || (((unsigned int)((c) - 9)) <= (13 - 9))))

#define TOPAZ_HBM_PROC_FILENAME "topaz_hbm"
#define TOPAZ_HBM_IF_PROC_NAME "topaz_hbm_if"

#define HBM_BUF_DEPLETION_TH		(20)
#define HBM_BUF_POLL_S_INTRVAL		(10 * HZ)
#define HBM_BUF_POLL_L_INTRVAL		(60 * HZ)
#define HBM_BUF_MINIMUM_AVAIL_NUM	(3)
#define HBM_BUF_MINIMUM_REL_NUM		(10 * HBM_BUF_POLL_S_INTRVAL)

typedef enum hbm_if_usr_cmd {
	HBM_IF_CMD_DUMPCTL = 0,
	HBM_IF_CMD_STATS = 1,
	HBM_IF_MAX_CMD,
} hbm_if_usr_cmd;

#define HBM_IF_KEY_DUMPCTL	"dumpctl"
#define HBM_IF_KEY_STATS	"stats"
static char* str_cmd[HBM_IF_MAX_CMD] = {
	HBM_IF_KEY_DUMPCTL,
	HBM_IF_KEY_STATS,
};

typedef enum hbm_stats {
	HBM_CNT_INVALID_POOL = 0,
	HBM_CNT_MISALIGNED = 1,
	HBM_CNT_MAGIC_CORRUPTED = 2,
	HBM_CNT_QUARANTINE_CORRUPTED = 3,
	HBM_CNT_QUARANTINE_ALLOC_FAIL = 4,
	HBM_CNT_QUARANTINE_OK = 5,
	HBM_CNT_NUM,
} hbm_stats;

struct hbm_pool_cnt {
	uint32_t prev_release_cnt;
	uint32_t pool_depleted_cnt;
};
struct topaz_hbm_mnt {
	uint32_t prev_unflow_cnt;
	uint32_t unflow_flag;
	struct hbm_pool_cnt wmac_pl;
	struct hbm_pool_cnt emac_pl;
};

static DEFINE_TIMER(hbm_timer, NULL, 0, 0);
static uint32_t topaz_hbm_stats[HBM_CNT_NUM] = {0};
static const char *topaz_hbm_stats_names[HBM_CNT_NUM] = {
	"Invalid pool",
	"Misaligned pointer",
	"Magic corrupted",
	"Quarantine corrupted buffer",
	"Quarantine allocation fail",
	"Quarantine ok",
};
#define HBM_STATS(_idx, _num)	(topaz_hbm_stats[(_idx)] += (_num))

#ifdef TOPAZ_EMAC_NULL_BUF_WR
#define HBM_UFLOW_RECOVER_TH	32
void __attribute__((section(".sram.data")))(*topaz_emac_null_buf_del_cb)(void) = NULL;
EXPORT_SYMBOL(topaz_emac_null_buf_del_cb);
#endif

static const char *topaz_hbm_requestor_names[TOPAZ_HBM_MASTER_COUNT] = TOPAZ_HBM_REQUESTOR_NAMES;

unsigned int topaz_hbm_pool_available(int8_t pool)
{
	uint32_t wr_ptr;
	uint32_t rd_ptr;
	static const unsigned int TOPAZ_HBM_MAX_POOL_COUNT = (1 << 16);

	if (!topaz_hbm_pool_valid(pool)) {
		printk(KERN_ERR"%s: Invalid pool %d\n", __func__, pool);
		return TOPAZ_HBM_MAX_POOL_COUNT;
	}

	wr_ptr = readl(TOPAZ_HBM_WR_PTR(pool));
	rd_ptr = readl(TOPAZ_HBM_RD_PTR(pool));

	if (wr_ptr >= rd_ptr)
		return (wr_ptr - rd_ptr);
	else
		return (TOPAZ_HBM_MAX_POOL_COUNT - rd_ptr + wr_ptr);
}
EXPORT_SYMBOL(topaz_hbm_pool_available);

#define HBM_DUMP_SORT_ORDER_INV_BASE	100
typedef enum hbm_dump_sort_type {
	HBM_DUMP_SORT_ADDR = 0,					/* lower addr first */
	HBM_DUMP_SORT_JIFF,					/* newest freed first */
	HBM_DUMP_SORT_BAD_MAGIC,				/* bad magic first */
	HBM_DUMP_SORT_ADDR_MAX = HBM_DUMP_SORT_ORDER_INV_BASE - 1,
} hbm_dump_sort_type;

static hbm_dump_sort_type topaz_hbm_dump_sort_type = HBM_DUMP_SORT_ADDR;
static int topaz_hbm_dump_sort_range_min = 0;			/* meaning dependent on sort type */
static int topaz_hbm_dump_sort_range_max = 0xFFFFFFFF;		/* meaning dependent on sort type */
static int topaz_hbm_dump_num = 5;				/* max dump number */
static int topaz_hbm_dump_len = 128;				/* bytes dump at head */
static int topaz_hbm_dump_taillen = 32;				/* bytes dump at tail */
static int topaz_hbm_dumped_num = 0;				/* currently dumpped number */

#define TOPAZ_HBM_POOL_SIZE_MAX		(TOPAZ_HBM_BUF_EMAC_RX_COUNT + 1)
uint32_t* topaz_hbm_dump_bufs_sorted[TOPAZ_HBM_POOL_SIZE_MAX] = {0};

static int topaz_hbm_stat_rd(char *page, char **start, off_t offset,
		int count, int *eof, void *data)
{
	char *p = page;
	uint32_t wr_ptr;
	uint32_t rd_ptr;

	int req_rel_diff = 0;
	int req_rel_perpool_diff[TOPAZ_HBM_POOL_COUNT];
	int master, pool;

	/* offsets for initial pool loading */
	req_rel_perpool_diff[TOPAZ_HBM_BUF_EMAC_RX_POOL] = TOPAZ_HBM_BUF_EMAC_RX_COUNT;
	req_rel_perpool_diff[TOPAZ_HBM_BUF_WMAC_RX_POOL] = TOPAZ_HBM_BUF_WMAC_RX_COUNT;
	req_rel_perpool_diff[TOPAZ_HBM_AUC_FEEDBACK_POOL] = 0;
	req_rel_perpool_diff[TOPAZ_HBM_EMAC_TX_DONE_POOL] = 0;

	for (pool = 0; pool < TOPAZ_HBM_POOL_COUNT; ++pool) {
		for (master = 0; master < TOPAZ_HBM_MASTER_COUNT; ++master) {
			uint32_t req = readl(TOPAZ_HBM_POOL_REQUEST_CNT(master, pool));
			uint32_t rel = readl(TOPAZ_HBM_POOL_RELEASE_CNT(master, pool));

			req_rel_perpool_diff[pool] += req;
			req_rel_perpool_diff[pool] -= rel;

			p += sprintf(p, "master %5s pool %d req %u rel %u\n",
					topaz_hbm_requestor_names[master], pool, req, rel);
		}
	}

	for (pool = 0; pool < TOPAZ_HBM_POOL_COUNT; ++pool) {
		req_rel_diff += req_rel_perpool_diff[pool];
		wr_ptr = readl(TOPAZ_HBM_WR_PTR(pool));
		rd_ptr = readl(TOPAZ_HBM_RD_PTR(pool));
		p += sprintf(p, "pool %u rd_ptr %u wr_ptr %u\n", pool, rd_ptr, wr_ptr);
	}

	uint32_t overflow = readl(TOPAZ_HBM_OVERFLOW_CNT);
	uint32_t underflow = readl(TOPAZ_HBM_UNDERFLOW_CNT);
	int allocated = req_rel_diff - underflow;

	p += sprintf(p, "underflow %u overflow %u req rel diff %d allocated %d\n",
		underflow, overflow, req_rel_diff, allocated);

	if (overflow) {
		p += sprintf(p, "ERROR: overflow counter must be zero\n");
	}

	if (underflow) {
		p += sprintf(p, "WARNING: underflow counter is non zero, may need to increase pool\n");
	}

	for (pool = 0; pool < TOPAZ_HBM_POOL_COUNT; ++pool) {
		p += sprintf(p, "pool %d req rel diff %d, available %u\n", pool, req_rel_perpool_diff[pool],
				topaz_hbm_pool_available(pool));
	}

	*eof = 1;
	return p - page;
}

static int __init topaz_hbm_stat_init(void)
{
	if (!create_proc_read_entry(TOPAZ_HBM_PROC_FILENAME, 0,
			NULL, topaz_hbm_stat_rd, NULL)) {
		return -EEXIST;
	}
	return 0;
}

static void __exit topaz_hbm_stat_exit(void)
{
	remove_proc_entry(TOPAZ_HBM_PROC_FILENAME, 0);
}

void topaz_hbm_init_pool_list(unsigned long *const pool_list, const uint16_t payload_count_s,
		const uintptr_t payloads_bus, const uint32_t payload_size,
		const uint32_t payload_headroom, const int8_t pool)
{
	uint32_t i;
	const uint16_t payload_count = (1 << payload_count_s);

	topaz_hbm_init((void *) virt_to_bus(pool_list), payload_count_s, pool, 0);

	if (payloads_bus) {
		for (i = 0; i < payload_count; i++) {
			uintptr_t buf_bus = payloads_bus + (i * payload_size) + payload_headroom;
			uint32_t *_p = bus_to_virt(buf_bus);
			uint32_t *_m = topaz_hbm_buf_get_meta(_p);
			uint32_t *enqueuep = _m - HBM_HR_OFFSET_ENQ_CNT;
			uint32_t *freep = _m - HBM_HR_OFFSET_FREE_CNT;
			uint32_t *statep = _m - HBM_HR_OFFSET_STATE;
			uint32_t *magicp = _p - HBM_HR_OFFSET_MAGIC;
			uint32_t *guardp =(uint32_t*)((uint32_t)_p + payload_size - payload_headroom -
							TOPAZ_HBM_PAYLOAD_END_GUARD_SIZE);
#if TOPAZ_HBM_BUF_EXTERNAL_META
			uint32_t *meta_ptr_p = _p - HBM_HR_OFFSET_META_PTR;
			uint32_t *meta_backptr_p = _m - HBM_HR_OFFSET_META_PTR;
#endif
			int j;
#if TOPAZ_HBM_DEBUG_STAMPS
			uint32_t *jiffp = _m - HBM_HR_OFFSET_FREE_JIFF;
			uint32_t *ownerp = _m - HBM_HR_OFFSET_OWNER;
			arc_write_uncached_32(jiffp, jiffies);
			arc_write_uncached_32(ownerp, TOPAZ_HBM_OWNER_INIT);
#endif
			/* always setup magic and guard area to provide minimum detection */
			arc_write_uncached_32(magicp, TOPAZ_HBM_BUF_GUARD_MAGIC);
			arc_write_uncached_32(statep, 0);
			for (j = 0; j < (TOPAZ_HBM_PAYLOAD_END_GUARD_SIZE >> 2); j++) {
				arc_write_uncached_32((guardp + j), TOPAZ_HBM_BUF_GUARD_MAGIC);
			}
			arc_write_uncached_32(enqueuep, 1);
			arc_write_uncached_32(freep, 0);

#if TOPAZ_HBM_BUF_EXTERNAL_META
			arc_write_uncached_32(meta_ptr_p, virt_to_bus(_m));
			arc_write_uncached_32(meta_backptr_p, buf_bus);
#endif

			topaz_hbm_put_buf((void *) buf_bus, pool);
		}
	}

	printk(KERN_INFO "%s pool %u pool_list 0x%p bus_range 0x%lx to 0x%lx sz %u count %u\n",
			__FUNCTION__, pool, pool_list,
			payloads_bus, payloads_bus + payload_size * payload_count,
			payload_size, payload_count);
}

static int g_pools_inited = 0;

static void topaz_hbm_init_payload_pools(void)
{
	unsigned long flags;
	uintptr_t *topaz_hbm_emac_rx_ptrs = (void *) (RUBY_SRAM_BEGIN + TOPAZ_HBM_POOL_EMAC_RX_START);
	uintptr_t *topaz_hbm_wmac_rx_ptrs = (void *) (RUBY_SRAM_BEGIN + TOPAZ_HBM_POOL_WMAC_RX_START);
	uintptr_t *topaz_hbm_emac_free_ptrs = (void *) (RUBY_SRAM_BEGIN + TOPAZ_HBM_POOL_EMAC_TX_DONE_START);

	printk("HBM pool: emac rx 0x%x to 0x%x, wmac rx 0x%x to 0x%x\n",
		TOPAZ_HBM_POOL_EMAC_RX_START,
		TOPAZ_HBM_POOL_EMAC_RX_END,
		TOPAZ_HBM_POOL_WMAC_RX_START,
		TOPAZ_HBM_POOL_WMAC_RX_END);

	memset((void *) (RUBY_DRAM_BEGIN + TOPAZ_HBM_BUF_EMAC_RX_BASE), TOPAZ_HBM_BUF_PAYLOAD_POISON,
			TOPAZ_HBM_BUF_EMAC_RX_TOTAL);
	memset((void *) (RUBY_DRAM_BEGIN + TOPAZ_HBM_BUF_WMAC_RX_BASE), TOPAZ_HBM_BUF_PAYLOAD_POISON,
			TOPAZ_HBM_BUF_WMAC_RX_TOTAL);
	flush_and_inv_dcache_sizerange_safe((void *) (RUBY_DRAM_BEGIN + TOPAZ_HBM_BUF_EMAC_RX_BASE), TOPAZ_HBM_BUF_EMAC_RX_TOTAL);
	flush_and_inv_dcache_sizerange_safe((void *) (RUBY_DRAM_BEGIN + TOPAZ_HBM_BUF_WMAC_RX_BASE), TOPAZ_HBM_BUF_WMAC_RX_TOTAL);

	memset((void *) (RUBY_DRAM_BEGIN + TOPAZ_HBM_BUF_META_BASE), TOPAZ_HBM_BUF_PAYLOAD_POISON,
			TOPAZ_HBM_BUF_META_TOTAL);
	flush_and_inv_dcache_sizerange_safe((void *)(RUBY_DRAM_BEGIN + TOPAZ_HBM_BUF_META_BASE),
			TOPAZ_HBM_BUF_META_TOTAL);
#if TOPAZ_HBM_BUF_EXTERNAL_META
	printk("HBM meta: emac rx 0x%x to 0x%x, wmac rx 0x%x to 0x%x\n",
			TOPAZ_HBM_BUF_META_EMAC_RX_BASE,
			TOPAZ_HBM_BUF_META_EMAC_RX_END,
			TOPAZ_HBM_BUF_META_WMAC_RX_BASE,
			TOPAZ_HBM_BUF_META_WMAC_RX_END);
#else
	printk("HBM used internal meta\n");
#endif

	local_irq_save(flags);

	topaz_hbm_init_pool_list(topaz_hbm_emac_rx_ptrs, TOPAZ_HBM_BUF_EMAC_RX_COUNT_S,
			TOPAZ_HBM_BUF_EMAC_RX_BASE, TOPAZ_HBM_BUF_EMAC_RX_SIZE, TOPAZ_HBM_PAYLOAD_HEADROOM,
			TOPAZ_HBM_BUF_EMAC_RX_POOL);
	topaz_hbm_init_pool_list(topaz_hbm_wmac_rx_ptrs, TOPAZ_HBM_BUF_WMAC_RX_COUNT_S,
			TOPAZ_HBM_BUF_WMAC_RX_BASE, TOPAZ_HBM_BUF_WMAC_RX_SIZE, TOPAZ_HBM_PAYLOAD_HEADROOM,
			TOPAZ_HBM_BUF_WMAC_RX_POOL);
	topaz_hbm_init_pool_list(topaz_hbm_emac_free_ptrs, TOPAZ_HBM_EMAC_TX_DONE_COUNT_S,
			0, 0, 0, TOPAZ_HBM_EMAC_TX_DONE_POOL);

	local_irq_restore(flags);

	g_pools_inited = 1;
}

int topaz_hbm_handle_buf_err(void *const buf_virt, int8_t dest_pool)
{
	if (!topaz_hbm_buf_ptr_valid(buf_virt)) {
		HBM_STATS(HBM_CNT_MISALIGNED, 1);
		printk(KERN_CRIT "%s: buf 0x%x misaligned: pool %u\n",
				__FUNCTION__,
				(unsigned int)buf_virt, dest_pool);
		return 0;
	}

	HBM_STATS(HBM_CNT_MAGIC_CORRUPTED, 1);
	hbm_buf_fix_buf_magic(buf_virt);

	return 1;
}

void  __attribute__((section(".sram.text"))) topaz_hbm_filter_txdone_buf(void *const buf_bus)
{
	const int8_t dest_pool = topaz_hbm_payload_get_pool_bus(buf_bus);

	if (dest_pool == TOPAZ_HBM_BUF_WMAC_RX_POOL ||
			dest_pool == TOPAZ_HBM_BUF_EMAC_RX_POOL) {
		uint32_t *const _p = bus_to_virt((uintptr_t) buf_bus);
		uint32_t *_m = topaz_hbm_buf_get_meta(_p);
		uint32_t *const enqueuep = _m - HBM_HR_OFFSET_ENQ_CNT;
		uint32_t *const freep = _m - HBM_HR_OFFSET_FREE_CNT;
		const uint32_t ec = arc_read_uncached_32(enqueuep);
		const uint32_t fc = arc_read_uncached_32(freep) + 1;
		const bool release = (ec && (fc == ec));

		if (release) {
			/* only fix magic corruption when we are sure no one else is using it right now */
			if (TOPAZ_HBM_BUF_MAGIC_CHK_ALLPOOL || (dest_pool == TOPAZ_HBM_BUF_WMAC_RX_POOL)) {
				int state = hbm_buf_check_buf_magic(_p);
				if (unlikely(state)) {
					if (!topaz_hbm_handle_buf_err(_p, dest_pool)) {
						/* shouldn't put it back to pool */
						return;
					}
				}
			}
#if TOPAZ_HBM_DEBUG_STAMPS
			uint32_t *jiffp = _m - HBM_HR_OFFSET_FREE_JIFF;
			uint32_t *ownerp = _m - HBM_HR_OFFSET_OWNER;
			const uint32_t owner = arc_read_uncached_32(ownerp);
			const uint8_t owner1 = (owner & 0xF) >> 0;

			if (owner1 == TOPAZ_HBM_OWNER_FREE) {
				/*
				 * Double free check is already broken because a lot of free places
				 * doesn't update the owner, both VB and PCIe platform.
				 */
				/*
				uint32_t *sizep = _m - HBM_HR_OFFSET_SIZE;
				const uint32_t size = arc_read_uncached_32(sizep);
				printk(KERN_ERR "%s: double free of buf_bus %p size %u owner %08x\n",
						__FUNCTION__, buf_bus, size, owner);
				topaz_hbm_buf_show(_p, TOPAZ_HBM_BUF_DUMP_DFT, 0);
				*/
			}
			arc_write_uncached_32(jiffp, jiffies);
			arc_write_uncached_32(ownerp, (owner << 4) | TOPAZ_HBM_OWNER_FREE);
#endif

			if (ec != 1) {
				arc_write_uncached_32(enqueuep, 1);
				arc_write_uncached_32(freep, 0);
			}
			topaz_hbm_put_payload_aligned_bus(buf_bus, dest_pool);
		} else {
			arc_write_uncached_32(freep, fc);
		}
	} else {
		HBM_STATS(HBM_CNT_INVALID_POOL, 1);
		printk(KERN_CRIT "%s: unknown pool %hhd for buf_bus 0x%p\n",
				__FUNCTION__, dest_pool, buf_bus);
	}
}
EXPORT_SYMBOL(topaz_hbm_filter_txdone_buf);

/*
 * Safely return the buf.
 * @pkt_bus needn't to be the pointer from the pool. It can be any location in the buffer.
 */
void topaz_hbm_release_buf_safe(void *const pkt_bus)
{
	const int8_t dest_pool = topaz_hbm_payload_get_pool_bus(pkt_bus);
	void *buf_bus = topaz_hbm_payload_store_align_bus(pkt_bus, dest_pool, 0);
	unsigned long flags;

	local_irq_save(flags);
	topaz_hbm_filter_txdone_buf(buf_bus);
	local_irq_restore(flags);
}
EXPORT_SYMBOL(topaz_hbm_release_buf_safe);

void topaz_hbm_filter_txdone_pool(void)
{
	unsigned long flags;
	void *buf_bus;
	const int8_t src_pool = TOPAZ_HBM_EMAC_TX_DONE_POOL;

	const uint32_t mask = TOPAZ_HBM_EMAC_TX_DONE_COUNT - 1;
	uint32_t wr_ptr;
	uint32_t rd_ptr;
	uint32_t wr_raw;
	uint32_t rd_raw;
	uint32_t i;
	uint32_t count;
	uint32_t full;

	if (unlikely(!g_pools_inited)) {
		return;
	}

	local_irq_save(flags);

	wr_raw = readl(TOPAZ_HBM_WR_PTR(src_pool));
	rd_raw = readl(TOPAZ_HBM_RD_PTR(src_pool));
	wr_ptr = wr_raw & mask;
	rd_ptr = rd_raw & mask;
	full = ((wr_raw != rd_raw) && (wr_ptr == rd_ptr));

	for (count = 0, i = rd_ptr; ((i != wr_ptr) || full); i = (i + 1) & mask, count++) {
		buf_bus = topaz_hbm_get_payload_bus(src_pool);
		if (buf_bus != NULL) {
			topaz_hbm_filter_txdone_buf(buf_bus);
		} else if (printk_ratelimit()) {
			printk(KERN_CRIT "%s: read NULL from pool %d\n",
					__FUNCTION__, src_pool);
			break;
		}
		full = 0;
	}
#ifdef TOPAZ_EMAC_NULL_BUF_WR
	if (topaz_emac_null_buf_del_cb) {
		uint32_t n;
		wr_ptr = readl(TOPAZ_HBM_WR_PTR(TOPAZ_HBM_BUF_EMAC_RX_POOL));
		rd_ptr = readl(TOPAZ_HBM_RD_PTR(TOPAZ_HBM_BUF_EMAC_RX_POOL));
		n = (wr_ptr - rd_ptr) % TOPAZ_HBM_BUF_EMAC_RX_COUNT;
		if (n > HBM_UFLOW_RECOVER_TH)
			topaz_emac_null_buf_del_cb();
	}
#endif
	local_irq_restore(flags);

	if (unlikely(count > (TOPAZ_HBM_EMAC_TX_DONE_COUNT * 3 / 4))) {
		if (printk_ratelimit())
			printk("Warning! %s count: %u\n", __FUNCTION__, count);
	}
}
EXPORT_SYMBOL(topaz_hbm_filter_txdone_pool);

static struct kmem_cache *shinfo_cache;

static uint8_t *topaz_hbm_skb_allocator_payload_alloc(struct skb_shared_info **shinfo,
		size_t size, gfp_t gfp_mask, int node)
{
	uint8_t *data;

	size = SKB_DATA_ALIGN(size);

	*shinfo = kmem_cache_alloc(shinfo_cache, gfp_mask);
	if (*shinfo == NULL) {
		return NULL;
	}

	if (size < topaz_hbm_pool_buf_max_size(TOPAZ_HBM_BUF_EMAC_RX_POOL)) {
		data = topaz_hbm_get_payload_virt(TOPAZ_HBM_BUF_EMAC_RX_POOL);
	} else {
		data = kmalloc(size, gfp_mask);
	}

	if (data == NULL) {
		kmem_cache_free(shinfo_cache, *shinfo);
		*shinfo = NULL;
	}

	return data;
}

static void topaz_hbm_skb_allocator_payload_free(struct sk_buff *skb)
{
	void *buf_bus = (void *) virt_to_bus(skb->head);
	const int8_t pool = topaz_hbm_payload_get_free_pool_bus(buf_bus);

	buf_bus = topaz_hbm_payload_store_align_bus(buf_bus,
		topaz_hbm_payload_get_pool_bus(buf_bus), 0);
	kmem_cache_free(shinfo_cache, skb_shinfo(skb));

	if (topaz_hbm_pool_valid(pool)) {
		if (!skb->hbm_no_free) {
			unsigned long flags;

			local_irq_save(flags);

			topaz_hbm_flush_skb_cache(skb);
			topaz_hbm_filter_txdone_buf(buf_bus);

			local_irq_restore(flags);
		}
	} else {
		kfree(skb->head);
	}

	topaz_hbm_filter_txdone_pool();
}

const struct skb_allocator topaz_hbm_skb_allocator = {
	.name = "topaz_hbm",
	.skb_alloc = &skb_allocator_kmem_caches_skb_alloc,
	.skb_free = &skb_allocator_kmem_caches_skb_free,
	.payload_alloc = &topaz_hbm_skb_allocator_payload_alloc,
	.payload_free = &topaz_hbm_skb_allocator_payload_free,
	.max_size = 0,
};

#define QTN_HBM_MAX_FRAME_LEN	12000	/* 12000 is over maximum vht frame size,
					and there is no rx frame whose size is over 12000 */
struct sk_buff *_topaz_hbm_attach_skb(void *buf_virt, int8_t pool, int inv, uint8_t headroom
		QTN_SKB_ALLOC_TRACE_ARGS)
{
	struct sk_buff *skb;
	struct skb_shared_info *shinfo;
	uint32_t buf_size;
	uint8_t *buf_head;

	if (unlikely(headroom > TOPAZ_HBM_PAYLOAD_HEADROOM)) {
		printk(KERN_WARNING "specified headroom(%u) should be smaller than %u\n",
			headroom, TOPAZ_HBM_PAYLOAD_HEADROOM);
		return NULL;
	}

	shinfo = kmem_cache_alloc(shinfo_cache, GFP_ATOMIC);
	if (!shinfo) {
		return NULL;
	}

	skb = skb_allocator_kmem_caches_skb_alloc(GFP_ATOMIC, 0, -1);
	if (!skb) {
		kmem_cache_free(shinfo_cache, shinfo);
		return NULL;
	}

	/*
	 * TODO FIXME: Restrict the buffer size less than 12k, because we saw ping failed
	 * if we set skb buffer size as 17K
	 */
	buf_size = min((int)topaz_hbm_pool_buf_max_size(pool), QTN_HBM_MAX_FRAME_LEN);
	buf_head = topaz_hbm_payload_store_align_virt(buf_virt, pool, 0) - headroom;

	/* invalidate all packet dcache before passing to the kernel */
	if (inv)
		inv_dcache_sizerange_safe(buf_head, buf_size);

	__alloc_skb_init(skb, shinfo, buf_head,
			buf_size, 0, &topaz_hbm_skb_allocator
			QTN_SKB_ALLOC_TRACE_ARGVARS);
	skb_reserve(skb, ((uint8_t *) buf_virt) - buf_head);

	return skb;
}
EXPORT_SYMBOL(_topaz_hbm_attach_skb);

/*
 * Allocate a new buffer to hold the pkt. The new buffer is guranteed to be safe from wmac rx dma overrun.
 * The original buffer is not used anymore. Caller should be responsible for freeing the buffer.
 */
struct sk_buff *topaz_hbm_attach_skb_quarantine(void *buf_virt, int pool, int len, uint8_t **whole_frm_hdr_p)
{
	uint8_t *buf_head;
	struct sk_buff *skb = NULL;
	uint32_t prev_len;
	uint32_t buf_size;

	KASSERT((pool == TOPAZ_HBM_BUF_WMAC_RX_POOL), ("buf quarantine is only for wmac rx pool, %d", pool));

	buf_head = topaz_hbm_payload_store_align_virt(buf_virt, pool, 0);
	if (hbm_buf_check_buf_magic(buf_head)) {
		/* don't copy to new skb if it is aleady corrupted */
		HBM_STATS(HBM_CNT_QUARANTINE_CORRUPTED, 1);
		return NULL;
	}

	/* copy from buffer head in case mac header is needed */
	prev_len = (uint32_t)buf_virt - (uint32_t)buf_head;
	buf_size = prev_len + len;

	skb = dev_alloc_skb(buf_size);
	if (!skb) {
		HBM_STATS(HBM_CNT_QUARANTINE_ALLOC_FAIL, 1);
		return NULL;
	}

	/* caller only invalidate this pkt, not entire buffer */
	inv_dcache_sizerange_safe(buf_head, prev_len);
	if (whole_frm_hdr_p)
		*whole_frm_hdr_p = skb->data;
	memcpy(skb->data, buf_head, buf_size);
	if (hbm_buf_check_buf_magic(buf_head)) {
		/* if corruption happens during data copying */
		HBM_STATS(HBM_CNT_QUARANTINE_CORRUPTED, 1);
		goto post_check_fail;
	}

	/* reserve head space so that later caller's skb_put() covers the packet */
	skb_reserve(skb, prev_len);
	HBM_STATS(HBM_CNT_QUARANTINE_OK, 1);

	/*
	 * Quarantine is done. now we are sure the data copy in skb is not corrupted and won't
	 */
	return skb;

post_check_fail:
	if (skb)
		dev_kfree_skb(skb);
	return NULL;
}
EXPORT_SYMBOL(topaz_hbm_attach_skb_quarantine);

static int topaz_hbm_bufs_sort_need_swap(const uint32_t *buf0, const uint32_t *buf1)
{
	int type;
	int inv;
	uint32_t v0;
	uint32_t v1;

	type = topaz_hbm_dump_sort_type;
	inv = 0;
	if (topaz_hbm_dump_sort_type >= HBM_DUMP_SORT_ORDER_INV_BASE) {
		type -= HBM_DUMP_SORT_ORDER_INV_BASE;
		inv = 1;
	}

	switch(type) {
	case HBM_DUMP_SORT_ADDR:
		v0 = (uint32_t)buf0;
		v1 = (uint32_t)buf1;
		break;
	case HBM_DUMP_SORT_JIFF:
		v0 = jiffies - arc_read_uncached_32(topaz_hbm_buf_get_meta(buf0) - HBM_HR_OFFSET_FREE_JIFF);
		v1 = jiffies - arc_read_uncached_32(topaz_hbm_buf_get_meta(buf1) - HBM_HR_OFFSET_FREE_JIFF);
		break;
	case HBM_DUMP_SORT_BAD_MAGIC:
		v0 = (arc_read_uncached_32(buf0 - HBM_HR_OFFSET_MAGIC) == TOPAZ_HBM_BUF_GUARD_MAGIC);
		v1 = (arc_read_uncached_32(buf1 - HBM_HR_OFFSET_MAGIC) == TOPAZ_HBM_BUF_GUARD_MAGIC);
		break;
	default:
		return 0;
		break;
	}

	return (inv ? (v1 > v0) : (v0 > v1));
}

static void topaz_hbm_bufs_sort(int pool, int pool_size)
{
	int i;
	int j;
	uint32_t *buf0;
	uint32_t *buf1;
	uint32_t *buf;
	int swapped;

	memset(topaz_hbm_dump_bufs_sorted, 0, sizeof(topaz_hbm_dump_bufs_sorted));
	for (i = 0; i < pool_size; i++) {
		topaz_hbm_dump_bufs_sorted[i] = (uint32_t*)
			topaz_hbm_payload_store_align_from_index(pool, i);
	}

	/* bubble sort */
	for (i = 0; i < (pool_size - 1); i++) {
		swapped = 0;
		for (j = 0; j < (pool_size - i - 1); j++) {
			buf0 = topaz_hbm_dump_bufs_sorted[j];
			buf1 = topaz_hbm_dump_bufs_sorted[j + 1];
			if (topaz_hbm_bufs_sort_need_swap(buf0, buf1)) {
				buf = buf0;
				topaz_hbm_dump_bufs_sorted[i] = buf1;
				topaz_hbm_dump_bufs_sorted[j] = buf;
				swapped = 1;
			}
		}
		if (!swapped)
			break;
	}

	topaz_hbm_dumped_num = 0;
}

static int topaz_hbm_buf_in_range(const uint32_t *buf)
{
	int type;
	uint32_t v;

	type = topaz_hbm_dump_sort_type;
	if (topaz_hbm_dump_sort_type >= HBM_DUMP_SORT_ORDER_INV_BASE) {
		type -= HBM_DUMP_SORT_ORDER_INV_BASE;
	}

	switch(type) {
	case HBM_DUMP_SORT_ADDR:
		v = (uint32_t)buf;
		break;
	case HBM_DUMP_SORT_JIFF:
		v = jiffies - arc_read_uncached_32(topaz_hbm_buf_get_meta(buf) - HBM_HR_OFFSET_FREE_JIFF);
		break;
	case HBM_DUMP_SORT_BAD_MAGIC:
		v = (arc_read_uncached_32(buf - HBM_HR_OFFSET_MAGIC) == TOPAZ_HBM_BUF_GUARD_MAGIC);
		break;
	default:
		return 0;
		break;
	}

	return ((topaz_hbm_dump_sort_range_min <= v) && (v <= topaz_hbm_dump_sort_range_max));
}

static void *topaz_hbm_bufs_emac_seq_start(struct seq_file *sfile, loff_t *pos)
{
	if (*pos > (TOPAZ_HBM_POOL_SIZE_MAX - 1))
		return NULL;

	if (*pos == 0)
		topaz_hbm_bufs_sort(TOPAZ_HBM_BUF_EMAC_RX_POOL, TOPAZ_HBM_BUF_EMAC_RX_COUNT);

	return topaz_hbm_dump_bufs_sorted[*pos];
}

static void *topaz_hbm_bufs_wmac_seq_start(struct seq_file *sfile, loff_t *pos)
{
	if (*pos > (TOPAZ_HBM_POOL_SIZE_MAX - 1))
		return NULL;

	if (*pos == 0)
		topaz_hbm_bufs_sort(TOPAZ_HBM_BUF_WMAC_RX_POOL, TOPAZ_HBM_BUF_WMAC_RX_COUNT);

	return topaz_hbm_dump_bufs_sorted[*pos];
}

static void* topaz_hbm_bufs_seq_next(struct seq_file *sfile, void *v, loff_t *pos)
{
	if (*pos > (TOPAZ_HBM_POOL_SIZE_MAX - 1))
		return NULL;

	*pos += 1;

	return topaz_hbm_dump_bufs_sorted[*pos];
}

static void topaz_hbm_bufs_seq_stop(struct seq_file *sfile, void *v)
{
}

static int topaz_hbm_bufs_seq_show(struct seq_file *sfile, void *v)
{
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
	const uint8_t *d;
	int dump_bytes;
	int i;
	uint8_t *tail;
	int tail_bytes;
	uint32_t whole_size;
	uint32_t payload_size;
	int pool;
	uint32_t idx;

	if (!topaz_hbm_buf_in_range(_p)) {
		return 0;
	}

	if (topaz_hbm_dumped_num++ >= topaz_hbm_dump_num) {
		return 0;
	}

	pool = topaz_hbm_buf_identify_buf_virt(v, &whole_size, &idx);
	if (pool < 0) {
		seq_printf(sfile, "invalid hbm buffer %x\n", (unsigned int)v);
		return 0;
	}
	payload_size = whole_size - TOPAZ_HBM_PAYLOAD_HEADROOM;

	dump_bytes = (topaz_hbm_dump_len == TOPAZ_HBM_BUF_DUMP_MAX) ? payload_size : topaz_hbm_dump_len;

	d = v;
	inv_dcache_sizerange_safe(v, dump_bytes);
	seq_printf(sfile, "%p ec %u fp %u own %08x size %u j %u (%u s ago) mg %x\n",
			v, ec, fc, oc, sz, jc, (((uint32_t) jiffies) - jc) / HZ, magic);
	for (i = 0; i < dump_bytes; ) {
		if (!(i % 32))
			seq_printf(sfile, "%08x ", (i - i % 32));
		++i;
		seq_printf(sfile, "%02x%s", *d++, (i % 32) == 0 ? "\n" : " ");
	}

	if (topaz_hbm_dump_taillen) {
		seq_printf(sfile, "\n");
		tail_bytes = topaz_hbm_dump_taillen;
		tail = (uint8_t*)((uint32_t)v + payload_size - tail_bytes);
		inv_dcache_sizerange_safe(tail, tail_bytes);
		seq_printf(sfile, "%p tail %p\n", v, tail);
		for (i = 0; i < tail_bytes; ) {
			if (!(i % 32))
				seq_printf(sfile, "%08x ", (i - i % 32));
			++i;
			seq_printf(sfile, "%02x%s", *tail++, (i % 32) == 0 ? "\n" : " ");
		}
	}
	seq_printf(sfile, "\n");

	return 0;
}

static struct seq_operations topaz_hbm_bufs_emac_seq_ops = {
	.start = topaz_hbm_bufs_emac_seq_start,
	.next  = topaz_hbm_bufs_seq_next,
	.stop  = topaz_hbm_bufs_seq_stop,
	.show  = topaz_hbm_bufs_seq_show
};

static struct seq_operations topaz_hbm_bufs_wmac_seq_ops = {
	.start = topaz_hbm_bufs_wmac_seq_start,
	.next  = topaz_hbm_bufs_seq_next,
	.stop  = topaz_hbm_bufs_seq_stop,
	.show  = topaz_hbm_bufs_seq_show
};

static int topaz_hbm_bufs_emac_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &topaz_hbm_bufs_emac_seq_ops);
}

static int topaz_hbm_bufs_wmac_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &topaz_hbm_bufs_wmac_seq_ops);
}

static struct file_operations topaz_hbm_bufs_emac_proc_ops = {
	.owner   = THIS_MODULE,
	.open    = topaz_hbm_bufs_emac_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release
};

static struct file_operations topaz_hbm_bufs_wmac_proc_ops = {
	.owner   = THIS_MODULE,
	.open    = topaz_hbm_bufs_wmac_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release
};

static inline int hbm_if_split_words(char **words, char *str)
{
	int word_count = 0;

	/* skip leading space */
	while (str && *str && isspace(*str)) {
		str++;
	}

	while (str && *str) {
		words[word_count++] = str;

		/* skip this word */
		while (str && *str && !isspace(*str)) {
			str++;
		}

		/* replace spaces with NULL */
		while (str && *str && isspace(*str)) {
			*str = 0;
			str++;
		}
	}

	return word_count;
}

static int hbm_if_cmd_dumpctl(char **words, uint8_t word_count)
{
	int idx = 1;

	if (word_count >= (idx + 1))
		sscanf(words[idx++], "%u", &topaz_hbm_dump_sort_type);
	if (word_count >= (idx + 1))
		sscanf(words[idx++], "0x%x", &topaz_hbm_dump_sort_range_min);
	if (word_count >= (idx + 1))
		sscanf(words[idx++], "0x%x", &topaz_hbm_dump_sort_range_max);
	if (word_count >= (idx + 1))
		sscanf(words[idx++], "%u", &topaz_hbm_dump_num);
	if (word_count >= (idx + 1))
		sscanf(words[idx++], "%u", &topaz_hbm_dump_len);
	if (word_count >= (idx + 1))
		sscanf(words[idx++], "%u", &topaz_hbm_dump_taillen);

	printk("hbm_if set dump ctl: sort_type %u sort_range [0x%x 0x%x] num %u len %u %u\n",
			topaz_hbm_dump_sort_type,
			topaz_hbm_dump_sort_range_min,
			topaz_hbm_dump_sort_range_max,
			topaz_hbm_dump_num,
			topaz_hbm_dump_len,
			topaz_hbm_dump_taillen
			);
	return 0;
}

static int hbm_if_cmd_show_stats(char **words, uint8_t word_count)
{
	int i;

	printk("HBM stats:\n");

	for (i = 0; i < HBM_CNT_NUM; i++) {
		printk("%s = %u\n", topaz_hbm_stats_names[i], topaz_hbm_stats[i]);
	}

	return 0;
}

/* Apply user command.
 * User command can control the HBM interface.
 * @param cmd_num: command number
 * @param words: the split words without spaces from the user space console interface
 * @param word_count: number of words after split
 * @return: status indication
 */
static int hbm_if_apply_user_command(hbm_if_usr_cmd cmd_num, char **words, uint8_t word_count)
{
	int rc = -EINVAL;

	if ((word_count == 0) || (!words)) {
		goto cmd_failure;
	}

	switch(cmd_num) {
		case HBM_IF_CMD_DUMPCTL:
			rc = hbm_if_cmd_dumpctl(words, word_count);
			break;
		case HBM_IF_CMD_STATS:
			rc = hbm_if_cmd_show_stats(words, word_count);
			break;
		default:
			goto cmd_failure;
			break;
	}

	if (rc < 0) {
		goto cmd_failure;
	}

	return 1;

cmd_failure:
	if (words)
		printk(KERN_INFO "Failed to parse command:%s, word count:%d\n", *words, word_count);
	else
		printk(KERN_INFO "Failed to parse command:(NULL)\n");

	return -EPERM;
}

static int hbm_if_write_proc(struct file *file, const char __user *buffer,
		unsigned long count, void *_unused)
{
	char *cmd;
	int rc, i;
	char **words;
	uint8_t word_count;
	hbm_if_usr_cmd cmd_num = 0;

	cmd = kmalloc(count, GFP_KERNEL);
	words = kmalloc(count * sizeof(char *) / 2, GFP_KERNEL);
	if (!cmd || !words) {
		rc = -ENOMEM;
		goto out;
	}

	if (copy_from_user(cmd, buffer, count)) {
		rc = -EFAULT;
		goto out;
	}

	/* Set null at last byte, note that count already gives +1 byte count*/
	cmd[count - 1] = '\0';

	word_count = hbm_if_split_words(words, cmd);
	for (i = 0; i < HBM_IF_MAX_CMD; i++, cmd_num++) {
		/* Extract command from first word */
		if (strcmp(words[0], str_cmd[i]) == 0) {
			printk(KERN_INFO"HBM user command:%s  \n", str_cmd[i]);
			break;
		}
	}

	/* Exclude softirqs whilst manipulating forwarding table */
	local_bh_disable();

	rc = hbm_if_apply_user_command(cmd_num, words, word_count);

	local_bh_enable();

	out:
	if (cmd) {
		kfree(cmd);
	}
	if (words) {
		kfree(words);
	}
	return count;
}

static inline uint32_t hbm_get_rel_cnt(int pool)
{
	int master;
	uint32_t rel = 0;

	for (master = 0; master < TOPAZ_HBM_MASTER_COUNT; ++master)
		rel += readl(TOPAZ_HBM_POOL_RELEASE_CNT(master, pool));

	return rel;
}

static int topaz_hbm_pool_poll_stat(int pool, struct hbm_pool_cnt *ps)
{
	uint32_t free;
	uint32_t fdelt;
	int rc;

	free = hbm_get_rel_cnt(pool);
	fdelt = free - ps->prev_release_cnt;
	ps->prev_release_cnt = free;

	if ((topaz_hbm_pool_available(pool) < HBM_BUF_MINIMUM_AVAIL_NUM) &&
			(fdelt < HBM_BUF_MINIMUM_REL_NUM)) {
		ps->pool_depleted_cnt++;
		rc = -1;
	} else {
		ps->pool_depleted_cnt = 0;
		rc = 0;
	}

	return rc;
}

void topaz_hbm_monitor(unsigned long data)
{
	struct topaz_hbm_mnt *hm =  (struct topaz_hbm_mnt *)data;
	uint32_t uf;
	int rc;
	unsigned long intval;

	intval = HBM_BUF_POLL_L_INTRVAL;
	if (!hm->unflow_flag) {
		uf = readl(TOPAZ_HBM_UNDERFLOW_CNT);
		if (uf - hm->prev_unflow_cnt) {
			hm->unflow_flag = 1;
			hm->prev_unflow_cnt = uf;
		} else {
			goto exit;
		}
	}

	rc = topaz_hbm_pool_poll_stat(TOPAZ_HBM_BUF_WMAC_RX_POOL, &hm->wmac_pl);
	rc += topaz_hbm_pool_poll_stat(TOPAZ_HBM_BUF_EMAC_RX_POOL, &hm->emac_pl);

	if (rc == 0) {
		hm->unflow_flag = 0;
	} else {
		if ((hm->wmac_pl.pool_depleted_cnt > HBM_BUF_DEPLETION_TH) ||
			(hm->emac_pl.pool_depleted_cnt > HBM_BUF_DEPLETION_TH)) {
			panic("HBM pool is depleted, wmac pool:%u, emac rx pool:%u\n",
				topaz_hbm_pool_available(TOPAZ_HBM_BUF_WMAC_RX_POOL),
				topaz_hbm_pool_available(TOPAZ_HBM_BUF_EMAC_RX_POOL));
		}
		intval = HBM_BUF_POLL_S_INTRVAL;
	}
exit:
	mod_timer(&hbm_timer, jiffies + intval);
}

static int __init topaz_hbm_bufs_init(void)
{
	struct topaz_hbm_mnt *hm;
	struct proc_dir_entry *e;

	if ((e = create_proc_entry("hbm_bufs_emac", 0, NULL)) != NULL) {
		e->proc_fops = &topaz_hbm_bufs_emac_proc_ops;
	}

	if ((e = create_proc_entry("hbm_bufs_wmac", 0, NULL)) != NULL) {
		e->proc_fops = &topaz_hbm_bufs_wmac_proc_ops;
	}

	struct proc_dir_entry *entry = create_proc_entry(TOPAZ_HBM_IF_PROC_NAME, 0600, NULL);
	if (entry) {
		entry->write_proc = hbm_if_write_proc;
		entry->read_proc = NULL;
	}

	hm =  (struct topaz_hbm_mnt *)kzalloc(sizeof(*hm), GFP_KERNEL);
	if (!hm) {
		printk(KERN_ERR"%s: fail to allocate hm", __func__);
		return -1;
	}

	init_timer(&hbm_timer);
	hbm_timer.data = (unsigned long)hm;
	hbm_timer.function = &topaz_hbm_monitor;
	mod_timer(&hbm_timer, jiffies + HBM_BUF_POLL_L_INTRVAL);

	return 0;
}

static void __exit topaz_hbm_bufs_exit(void)
{
	remove_proc_entry("hbm_bufs_wmac", 0);
	remove_proc_entry("hbm_bufs_emac", 0);
	remove_proc_entry(TOPAZ_HBM_IF_PROC_NAME, NULL);

	del_timer(&hbm_timer);
	if (hbm_timer.data)
		kfree((void *)hbm_timer.data);
}

static int __init topaz_hbm_module_init(void)
{
	COMPILE_TIME_ASSERT(TOPAZ_HBM_BUF_META_SIZE >= (HBM_HR_OFFSET_MAX * 4));

	topaz_hbm_init_payload_pools();

	shinfo_cache = kmem_cache_create("topaz_skb_shinfo_cache",
			sizeof(struct skb_shared_info),
			0,
			SLAB_HWCACHE_ALIGN | SLAB_PANIC,
			NULL);

	skb_allocator_register(TOPAZ_HBM_SKB_ALLOCATOR, &topaz_hbm_skb_allocator, 0);

	topaz_hbm_stat_init();
	topaz_hbm_bufs_init();

	return 0;
}
module_init(topaz_hbm_module_init)

static void __exit topaz_hbm_module_exit(void)
{
	topaz_hbm_stat_exit();
	topaz_hbm_bufs_exit();
}
module_exit(topaz_hbm_module_exit)


