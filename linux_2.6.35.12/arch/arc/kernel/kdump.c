/*
 * (C) Copyright 2011 Quantenna Communications Inc.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/jiffies.h>
#include <asm/board/kdump.h>
#include <asm/cache.h>
#include <asm/cacheflush.h>
#include <linux/crc32.h>
#include <common/queue.h>

#define PROC_NAME	"kdump"

extern unsigned long _text, _etext, __sram_text_start, __sram_text_end;

#if CONFIG_KERNEL_TEXT_SNAPSHOT_COUNT > 0

#define KERNEL_TEXT_SNAPSHOT_DATA_BYTES	(6 * 1024 * 1024)

/*
 * kernel dram, kernel sram,
 * one for each of dram and sram for each module
 */
#define KDUMP_MODULE_SNAPSHOT_SECTIONS	16
#define KDUMP_BASE_SNAPSHOT_SECTIONS	2	/* dram and sram */
#define KDUMP_MAX_SNAPSHOT_SECTIONS	(KDUMP_MODULE_SNAPSHOT_SECTIONS + KDUMP_BASE_SNAPSHOT_SECTIONS)

static struct module *modules[KDUMP_MODULE_SNAPSHOT_SECTIONS];

struct kdump_snapshot_section {
	const char		*name;
	unsigned long		crc32;
	unsigned long		*text_store;
	const unsigned long	*start;
	const unsigned long	*end;
	unsigned long		bytes;
};

struct kdump_snapshot {
	unsigned long		jiff;
	const char		*description;
	unsigned long		_text[KERNEL_TEXT_SNAPSHOT_DATA_BYTES / sizeof(unsigned long)];
	struct kdump_snapshot_section	sections[KDUMP_MAX_SNAPSHOT_SECTIONS];
	unsigned int		section_index;
};

static struct kdump_snapshot snapshots[CONFIG_KERNEL_TEXT_SNAPSHOT_COUNT];
static int snapshot_index = 0;

static unsigned long kdump_snapshot_section_crc(const struct kdump_snapshot_section *kss)
{
	return crc32_le(0, (const uint8_t*)kss->text_store, kss->bytes);
}

static int kdump_verify_snapshot_section_crc(const struct kdump_snapshot_section *kss)
{
	return kdump_snapshot_section_crc(kss) == kss->crc32;
}

static void kdump_take_snapshot_section(struct kdump_snapshot_section *kss)
{
	const unsigned long *pkern = kss->start;
	unsigned long *pdump = kss->text_store;

	kss->bytes = 0;

	while (pkern < kss->end) {
		unsigned long d = arc_read_uncached_32(pkern);
		*pdump = d;
		kss->bytes += sizeof(d);

		pkern++;
		pdump++;
	}

	kss->crc32 = kdump_snapshot_section_crc(kss);

	printk(KERN_WARNING "%s %s stored from 0x%p to 0x%p\n\t%lu bytes at 0x%p, crc 0x%lx\n",
			__FUNCTION__, kss->name,
			kss->start, kss->end,
			kss->bytes, kss->text_store, kss->crc32);
}

static void kdump_take_base_snapshots(struct kdump_snapshot *snap)
{
	const unsigned long * const dram_start = &_text;
	const unsigned long * const dram_end = &_etext;
	const unsigned long dram_size = (unsigned long)dram_end - (unsigned long)dram_start;
	const unsigned long * const sram_start = &__sram_text_start;
	const unsigned long * const sram_end = &__sram_text_end;
	const unsigned long sram_size = (unsigned long)sram_end - (unsigned long)sram_start;

	snap->sections[0].name = "dram";
	snap->sections[0].text_store = &snap->_text[0];
	snap->sections[0].start = dram_start;
	snap->sections[0].end = dram_end;
	kdump_take_snapshot_section(&snap->sections[0]);

	snap->sections[1].name = "sram";
	snap->sections[1].text_store = &snap->_text[dram_size / sizeof(snap->_text[0])];
	snap->sections[1].start = sram_start;
	snap->sections[1].end = sram_end;
	kdump_take_snapshot_section(&snap->sections[1]);

	snap->section_index = 2;
}

static void kdump_take_module_snapshot(struct kdump_snapshot *snap, struct module *mod)
{
	int i;
	int section_index;
	int found = 0;
	struct kdump_snapshot_section *sec;
	struct kdump_snapshot_section *prevsec;

	for (i = 0; found == 0 && i < KDUMP_MODULE_SNAPSHOT_SECTIONS; i++) {
		if (modules[i] == mod) {
			section_index = i;
			found = 1;
		}
	}

	for (i = 0; found == 0 && i < KDUMP_MODULE_SNAPSHOT_SECTIONS; i++) {
		if (modules[i] == NULL) {
			modules[i] = mod;
			section_index = i;
			found = 1;
		}
	}

	if (!found) {
		printk(KERN_ERR "%s: no module snapshot space left\n", __FUNCTION__);
		return;
	}

	sec = &snap->sections[snap->section_index];
	prevsec = &snap->sections[snap->section_index - 1];
	sec->name = mod->name;
	sec->text_store = ((uint8_t*)prevsec->text_store) + prevsec->bytes;
	sec->start = mod->module_core;
	sec->end = (unsigned long*)(((uint8_t*)sec->start) + mod->core_text_size);
	snap->section_index++;
	kdump_take_snapshot_section(sec);

	if (mod->module_sram) {
		sec = &snap->sections[snap->section_index];
		prevsec = &snap->sections[snap->section_index - 1];
		sec->name = mod->name;
		sec->text_store = ((uint8_t*)prevsec->text_store) + prevsec->bytes;
		sec->start = mod->module_sram;
		sec->end = (unsigned long*)(((uint8_t*)sec->start) + mod->sram_text_size);
		snap->section_index++;
		kdump_take_snapshot_section(sec);
	}
}

void kdump_do_take_snapshot(const char* description, int index)
{
	struct kdump_snapshot *snap;
	int i;

	printk(KERN_WARNING "%s: taking snapshot '%s' index %d\n",
			__FUNCTION__, description, index);

	snap = &snapshots[index];
	memset(snap, 0, sizeof(snap));
	snap->jiff = jiffies;
	snap->description = description;

	kdump_take_base_snapshots(snap);

	for (i = 0; i < KDUMP_MODULE_SNAPSHOT_SECTIONS; i++) {
		if (modules[i]) {
			kdump_take_module_snapshot(snap, modules[i]);
		}
	}
}

void kdump_add_module(struct module *mod)
{
	// add to current snapshot
	int current_snap_index = (snapshot_index + CONFIG_KERNEL_TEXT_SNAPSHOT_COUNT - 1) % CONFIG_KERNEL_TEXT_SNAPSHOT_COUNT;
	flush_cache_all();
	kdump_take_module_snapshot(&snapshots[current_snap_index], mod);
}

void kdump_remove_module(struct module *mod)
{
	int i;
	printk(KERN_ERR "%s not fully implemented, kernel text snapshots will break.\n", __FUNCTION__);
	for (i = 0; i < KDUMP_MODULE_SNAPSHOT_SECTIONS; i++) {
		if (modules[i] == mod) {
			modules[i] = NULL;
		}
	}
}

int kdump_take_snapshot(const char *description)
{
	int index = snapshot_index;

	kdump_do_take_snapshot(description, snapshot_index);
	snapshot_index = (snapshot_index + 1) % CONFIG_KERNEL_TEXT_SNAPSHOT_COUNT;

	return index;
}
EXPORT_SYMBOL(kdump_take_snapshot);

static void kdump_compare_snapshot_section(const char *context,
		const struct kdump_snapshot_section *kss0,
		const struct kdump_snapshot_section *kss1)
{
	const unsigned long *d0 = kss0->text_store;
	const unsigned long *d1 = kss1->text_store;
	const unsigned long start = (unsigned long)kss0->start;
	int i;

	if (kss0->bytes != kss1->bytes) {
		printk(KERN_WARNING "%s '%s': snapshot lengths differ\n",
				__FUNCTION__, context);
	} else if (!kdump_verify_snapshot_section_crc(kss0)) {
		printk(KERN_WARNING "%s '%s': snapshot 0 crc is bad\n",
				__FUNCTION__, context);
	} else if (!kdump_verify_snapshot_section_crc(kss1)) {
		printk(KERN_WARNING "%s '%s': snapshot 1 crc is bad\n",
				__FUNCTION__, context);
	} else if (kss0->crc32 == kss1->crc32) {
		printk(KERN_WARNING "%s '%s': checksums match\n",
				__FUNCTION__, context);
	} else {
		printk(KERN_WARNING "%s '%s': checksums differ...\n",
				__FUNCTION__, context);
		for (i = 0; i < kss0->bytes; i += sizeof(*d0)) {
			if (*d0 != *d1) {
				const unsigned long addr = start + i;
				printk(KERN_WARNING "\tcorruption at 0x%lx: 0x%lx vs 0x%lx\n",
						addr, *d0, *d1);
			}

			d0++;
			d1++;
		}
	}
}

void kdump_compare_snapshots(int index0, int index1)
{
	int i;
	const struct kdump_snapshot *s0 = &snapshots[index0];
	const struct kdump_snapshot *s1 = &snapshots[index1];

	printk(KERN_WARNING "%s: comparing kernel text section snapshots:\n"
			"\t'%s' vs '%s'...\n",
			__FUNCTION__, s0->description, s1->description);

	for (i = 0; i < KDUMP_MAX_SNAPSHOT_SECTIONS; i++) {
		const struct kdump_snapshot_section *ss0 = &s0->sections[i];
		const struct kdump_snapshot_section *ss1 = &s1->sections[i];
		if (ss0->text_store && ss1->text_store && ss0->start == ss1->start) {
			kdump_compare_snapshot_section(ss0->name, ss0, ss1);
		}
	}
}

void kdump_compare_all_snapshots(void)
{
	int i;
	int j;

	for (i = 0; i < CONFIG_KERNEL_TEXT_SNAPSHOT_COUNT - 1; i++) {
		for (j = i + 1; j < CONFIG_KERNEL_TEXT_SNAPSHOT_COUNT; j++) {
			kdump_compare_snapshots(i, j);
		}
	}
}
EXPORT_SYMBOL(kdump_compare_all_snapshots);

#endif


unsigned long kdump_calc_ktext_checksum(void)
{
	const unsigned long * const start = &_text;
	const unsigned long * const end = &_etext;
	const unsigned long *p;
	unsigned long hash = 0;
	for (p = start; p < end; p++) {
		hash = hash * 17 + *p;
	}
	return hash;
}

void kdump_print_ktext_checksum(void)
{
	const unsigned long * const start = &_text;
	const unsigned long * const end = &_etext;
	printk(KERN_WARNING "Kernel text section: %p to %p checksum %lu\n",
			start, end, kdump_calc_ktext_checksum());
}
EXPORT_SYMBOL(kdump_print_ktext_checksum);

#define KDUMP_WORDS_ROUNDUP_S	2
#define KDUMP_WORDS_ROUNDUP_M	((1 << KDUMP_WORDS_ROUNDUP_S) - 1)


static __inline__ __pure int kdump_dram_words(void)
{
	const unsigned long dram_start = (unsigned long)&_text;
	const unsigned long dram_end = (unsigned long)&_etext;
	return (dram_end - dram_start) / sizeof(unsigned long);
}

static __inline__ __pure int kdump_sram_words(void)
{
	const unsigned long sram_start = (unsigned long)&__sram_text_start;
	const unsigned long sram_end = (unsigned long)&__sram_text_end;
	return (sram_end - sram_start) / sizeof(unsigned long);
}

static __inline__ __pure int kdump_done(loff_t pos)
{
	int words = kdump_dram_words() + kdump_sram_words();
	return pos >= words;
}

static __inline__ __pure int kdump_section_offset(loff_t pos)
{
	int dram_words = kdump_dram_words();
	if (pos >= dram_words) {
		return pos - dram_words;
	}
	return pos;
}

static __inline__ __pure int kdump_section_lastword(loff_t pos)
{
	return pos + 1 == kdump_dram_words() || kdump_done(pos + 1);
}

static unsigned long* __pure kdump_word_from_index(loff_t pos)
{
	unsigned long *p;

	if (pos < kdump_dram_words()) {
		p = &_text;
		return &p[pos];
	} else {
		p = &__sram_text_start;
		return &p[pos - kdump_dram_words()];
	}
}

static void* kdump_seq_start(struct seq_file *sfile, loff_t *pos)
{
	loff_t *ppos = kmalloc(sizeof(*ppos), GFP_KERNEL);

	if (ppos == NULL || kdump_done(*pos + 1)) {
		return NULL;
	}

	*ppos = *pos;

	return ppos;
}

static void* kdump_seq_next(struct seq_file *sfile, void *v, loff_t *pos)
{
	loff_t *ppos = v;

	if (ppos == NULL || kdump_done(*pos + 1)) {
		return NULL;
	}

	*pos = *pos + 1;
	*ppos = *pos;

	return ppos;
}

static void kdump_seq_stop(struct seq_file *sfile, void *v)
{
	loff_t *ppos = v;
	if (ppos) {
		kfree(ppos);
	}
}

static int kdump_seq_show(struct seq_file *sfile, void *v)
{
	static const int values_per_line = 4;

	loff_t *ppos = v;
	int section_idx = kdump_section_offset(*ppos);
	unsigned long *word = kdump_word_from_index(*ppos);
	int print_addr = section_idx % values_per_line == 0;
	int print_newline = (section_idx + 1) % values_per_line == 0 ||
		kdump_section_lastword(*ppos);

	if (print_addr) {
		seq_printf(sfile, "%p:", word);
	}

	seq_printf(sfile, " %lx", *word);

	if (print_newline) {
		seq_printf(sfile, "\n");
	}

	return 0;
}

static struct seq_operations kdump_seq_ops = {
	.start = kdump_seq_start,
	.next  = kdump_seq_next,
	.stop  = kdump_seq_stop,
	.show  = kdump_seq_show
};

static int kdump_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &kdump_seq_ops);
}

static struct file_operations kdump_proc_ops = {
	.owner   = THIS_MODULE,
	.open    = kdump_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release
};

static int __init kdump_init(void)
{
	printk(KERN_INFO "loading %s ...\n", PROC_NAME);

	kdump_take_snapshot("kdump module load");

	struct proc_dir_entry *kdump_entry = create_proc_entry(PROC_NAME, 0, NULL);
	if (!kdump_entry) {
		return -ENOENT;
	}
	kdump_entry->proc_fops = &kdump_proc_ops;

	return 0;
}

static void __exit kdump_exit(void)
{
	printk(KERN_INFO "unloading %s ...\n", PROC_NAME);
	remove_proc_entry(PROC_NAME, NULL);
}

module_init(kdump_init);
module_exit(kdump_exit);

MODULE_LICENSE("GPL");
