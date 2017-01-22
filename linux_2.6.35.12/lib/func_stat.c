/**
 * Copyright (c) 2012-2013 Quantenna Communications, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/func_stat.h>

#ifdef CONFIG_FUNC_PROFILER_STATS

#define FUNC_PF_HASH_SHIFT 10
#define FUNC_PF_HASH_SIZE (1 << FUNC_PF_HASH_SHIFT)
#define FUNC_PF_HASH_MASK (FUNC_PF_HASH_SIZE - 1)

#define FUNC_PF_MAX_ENTRIES 10

static int num_entries = 0;

struct func_pf_entry {
	char *file;
	int line;
	uint32_t count;
};

static struct func_pf_entry func_pf_entries[FUNC_PF_HASH_SIZE];

static DEFINE_SPINLOCK(func_pf_lock);

static inline unsigned int func_pf_hash(char *file, int line)
{
	uint32_t hash = line << 3;
	return (hash & FUNC_PF_HASH_MASK);
}

void func_pf_entry_update(char *file, int line)
{
	int hash_idx = func_pf_hash(file, line);
	struct func_pf_entry  *hash_entry = &func_pf_entries[hash_idx];
	unsigned long flags;

	if (num_entries >= FUNC_PF_HASH_SIZE) {
		printk("%: hash entry insert failed.\n", __func__);
		return;
	}

	spin_lock_irqsave(&func_pf_lock, flags);

	while (hash_entry->file !=NULL && strcmp(hash_entry->file, file) != 0 &&
		hash_entry->line != line) {

		hash_idx = (hash_idx + 1)  & FUNC_PF_HASH_MASK;
		hash_entry = &func_pf_entries[hash_idx];
	}

	if (hash_entry->file == NULL) {
		hash_entry->file = file;
		hash_entry->line = line;
		num_entries++;
	}

	hash_entry->count++;

	spin_unlock_irqrestore(&func_pf_lock, flags);
}
EXPORT_SYMBOL(func_pf_entry_update);

int func_pf_get_stats(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	struct func_pf_entry max_entries[FUNC_PF_MAX_ENTRIES];
	struct func_pf_entry *pf_entry = func_pf_entries;
	unsigned long flags;
	int i, j, n = 0;

	memset(max_entries, 0, sizeof(max_entries));
	spin_lock_irqsave(&func_pf_lock, flags);

	for (i = 0; i < FUNC_PF_HASH_SIZE; i++) {
		if (pf_entry->count > max_entries[FUNC_PF_MAX_ENTRIES - 1].count) {
			j = FUNC_PF_MAX_ENTRIES - 2;
			while (pf_entry->count > max_entries[j].count && j >= 0) {
				max_entries[j+1] = max_entries[j];
				j--;
			}
			max_entries[j + 1] = *pf_entry;
		}
		pf_entry->count = 0;
		pf_entry++;
	}

	spin_unlock_irqrestore(&func_pf_lock, flags);

	for (i = 0; i < FUNC_PF_MAX_ENTRIES && max_entries[i].file != NULL; i++) {
		char *file = strrchr(max_entries[i].file, '/');
		if (file == NULL) {
			file = max_entries[i].file;
		} else {
			file++;
		}

		n += sprintf(page + n, "%d - %s:%d\n", max_entries[i].count, file, max_entries[i].line);
	}

	return n;
}
EXPORT_SYMBOL(func_pf_get_stats);

#endif /* CONFIG_FUNC_PROFILER_STATS */
