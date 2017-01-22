/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleloader.h>
#include <linux/kernel.h>
#include <linux/elf.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <asm/unwind.h>

static inline void arc_write_me(unsigned short *addr, unsigned long value)
{
    *addr = (value & 0xffff0000) >> 16;
    *(addr+1) = (value & 0xffff) ;
}

void *module_alloc(unsigned long size)
{
	void *ret;

	if(size == 0) {
		return NULL;
	}

	ret = kmalloc(size, GFP_KERNEL); /* kmalloc() allocated memory does not require TLB - so this should work faster */
	if (!ret) {
		printk(KERN_WARNING"Module cannot be allocated using kmalloc(), let's try vmalloc()\n");
		ret = vmalloc(size);
	}

	return ret;

}

void module_free(struct module *module, void *region)
{
	if (!region) {
		return;
	}

	if (unlikely(is_vmalloc_addr(region))) {
		vfree(region);
	} else {
		kfree(region);
	}
}

/* ARC specific section quirks - before relocation loop in generic loader
 *
 * For dwarf unwinding out of modules, this needs to
 * 1. Ensure the .debug_frame is allocatable (ARC Linker bug: despite
 *    -fasynchronous-unwind-tables it doesn't).
 * 2. Since we are iterating thru sec hdr tbl anyways, make a note of
 *    the exact section index, for later use.
 */
int module_frob_arch_sections(Elf_Ehdr *hdr, Elf_Shdr *sechdrs,
			      char *secstr, struct module *mod)
{
#ifdef CONFIG_ARC_STACK_UNWIND
	int i;

	mod->arch.unw_sec_idx = 0;
	mod->arch.unw_info = NULL;

	for (i = 1; i < hdr->e_shnum; i++) {
		if (strcmp(secstr+sechdrs[i].sh_name, ".debug_frame") == 0) {
			sechdrs[i].sh_flags |= SHF_ALLOC;
			mod->arch.unw_sec_idx = i;
			break;
		}
	}
#endif
    return 0;
}

void module_arch_cleanup(struct module *mod)
{
#ifdef CONFIG_ARC_STACK_UNWIND
	if (mod->arch.unw_info)
		unwind_remove_table(mod->arch.unw_info, 0);
#endif
}

int
apply_relocate(Elf32_Shdr *sechdrs, const char *strtab, unsigned int symindex,
           unsigned int relindex, struct module *module)
{
    printk(KERN_ERR "module %s: SHT_REL type unsupported\n",
           module->name);

    return 0;
}

int apply_relocate_add(Elf32_Shdr *sechdrs,
           const char *strtab,
           unsigned int symindex,
           unsigned int relsec,
           struct module *module)
{
    int i, n;
    Elf32_Rela *rel_entry = (void *)sechdrs[relsec].sh_addr;
    Elf32_Sym *sym_entry, *sym_sec;
    Elf32_Addr relocation;
    Elf32_Addr location;
    Elf32_Addr sec_to_patch;
    int relo_type;

    sec_to_patch = sechdrs[sechdrs[relsec].sh_info].sh_addr;
    sym_sec = (Elf32_Sym *)sechdrs[symindex].sh_addr;
    n = sechdrs[relsec].sh_size / sizeof(*rel_entry);

    pr_debug("Section to fixup %x\n", sec_to_patch);
    pr_debug("===========================================================\n");
    pr_debug("rela->r_off | rela->addend | sym->st_value | ADDR | VALUE\n");
    pr_debug("===========================================================\n");

    // Loop thru entries in relocation section.
    for (i = 0; i < n; i++) {

        /* This is where to make the change */
        location = sec_to_patch + rel_entry[i].r_offset;

        /* This is the symbol it is referring to.  Note that all
           undefined symbols have been resolved.  */
        sym_entry = sym_sec + ELF32_R_SYM(rel_entry[i].r_info);

        relocation = sym_entry->st_value + rel_entry[i].r_addend;

        pr_debug("\t%x\t\t%x\t\t%x  %x %x [%s]\n",
            rel_entry[i].r_offset, rel_entry[i].r_addend,
            sym_entry->st_value, location, relocation,
            strtab + sym_entry->st_name);

        /* This assumes modules are built with -mlong-calls
        * so any branches/jumps are absolute 32 bit jmps
        * global data access again is abs 32 bit.
        * Both of these are handled by same relocation type
        */
        relo_type = ELF32_R_TYPE(rel_entry[i].r_info);

        if (likely(R_ARC_32_ME == relo_type)) {
            arc_write_me((unsigned short *)location, relocation);
        }
        else if (R_ARC_32 == relo_type) {
            *((Elf32_Addr *)location) = relocation;
        }
        else
            goto relo_err;

    }
    return 0;

relo_err:
            printk(KERN_ERR "%s: unknown relocation: %u\n",
                   module->name, ELF32_R_TYPE(rel_entry[i].r_info));
            return -ENOEXEC;

}

/* Just before lift off: After sections have been relocated, we add the
 * dwarf section to unwinder table pool
 * This couldn't be done in module_frob_arch_sections() because
 * relocations had not been applied by then
 */
int module_finalize(const Elf32_Ehdr *hdr, const Elf_Shdr *sechdrs,
		    struct module *mod)
{
#ifdef CONFIG_ARC_STACK_UNWIND
	void *unw;
	int unwsec = mod->arch.unw_sec_idx;

	if (unwsec) {
		unw = unwind_add_table(mod, (void *)sechdrs[unwsec].sh_addr,
				       sechdrs[unwsec].sh_size);
		mod->arch.unw_info = unw;
	}
#endif
    return 0;
}

#ifdef CONFIG_ARCH_RUBY_NUMA

/* roundmb - Round address up to size of memblock  */
#define roundmb(x)      (void *)( (0x07 + (unsigned long)(x)) & ~0x07 )
/* truncmb - Truncate address down to size of memblock */
#define truncmb(x)      (void *)( ((unsigned long)(x)) & ~0x07 )

struct memblock
{
	struct memblock *next;	/* Pointer to next memory block       */
	unsigned long length;	/* Size of memory block (with struct) */
};

static struct memblock s_memlist = { NULL, 0 }; /* List of free memory blocks */
static DEFINE_SPINLOCK(s_heap_lock);

extern char __sram_end;

int heap_sram_ptr(void *pmem)
{
#ifdef QTN_RC_ENABLE_HDP
	return (pmem >= (void*)&__sram_end) &&
		(pmem < (void*)(RUBY_SRAM_BEGIN + CONFIG_ARC_SRAM_END));
#else
	return (pmem >= (void*)&__sram_end) &&
		(pmem < (void*)(RUBY_SRAM_BEGIN + CONFIG_ARC_KERNEL_SRAM_B2_END));
#endif
}
EXPORT_SYMBOL(heap_sram_ptr);

static __init struct memblock* qtn_new_memblock_init(struct memblock *pmblock, void *begin, void *end)
{
	if (!pmblock) {
		pmblock = (struct memblock *) begin;
		s_memlist.next = pmblock;
	} else {
		pmblock->next = (struct memblock *) begin;
		pmblock = pmblock->next;
	}

	pmblock->next = NULL;
	pmblock->length = (unsigned) (end - begin);

	return pmblock;
}

static int __init qtn_meminit(void)
{
	s_memlist.next = NULL;

	void *heapbegin = roundmb(&__sram_end);
#ifdef QTN_RC_ENABLE_HDP
	void *heapend = truncmb(RUBY_SRAM_BEGIN + CONFIG_ARC_SRAM_END);
#else
	void *heapend = truncmb(RUBY_SRAM_BEGIN + CONFIG_ARC_KERNEL_SRAM_B2_END);
#endif

	printk(KERN_INFO"Topaz heap in SRAM %p<->%p, B2 BASE %x\n",
		heapbegin, heapend, CONFIG_ARC_KERNEL_SRAM_B2_BASE);

	if (heapbegin < heapend) {
		qtn_new_memblock_init(NULL, heapbegin, heapend);
	}

	return 0;
}
arch_initcall(qtn_meminit);

static void *qtn_memget(unsigned long nbytes)
{
	struct memblock *memlist = &s_memlist;
	unsigned long flags;
	register struct memblock *prev, *curr, *leftover;

	if(0 == nbytes)
	{
		return NULL;
	}

	/* Round to multiple of memblock size   */
	nbytes = (unsigned long) roundmb(nbytes);

	spin_lock_irqsave(&s_heap_lock, flags);

	prev = memlist;
	curr = memlist->next;

	while(curr != NULL)
	{
		if(curr->length == nbytes)
		{
			prev->next = curr->next;
			memlist->length -= nbytes;

			goto return_curr;
		}
		else if(curr->length > nbytes)
		{
			/* Split block into two */
			leftover = (struct memblock *)((unsigned long) curr + nbytes);
			prev->next = leftover;
			leftover->next = curr->next;
			leftover->length = curr->length - nbytes;
			memlist->length -= nbytes;

			goto return_curr;
		}

		prev = curr;
		curr = curr->next;
	}

	spin_unlock_irqrestore(&s_heap_lock, flags);
	return NULL;

return_curr:
	spin_unlock_irqrestore(&s_heap_lock, flags);
	return (void *)curr;
}

static int qtn_memfree(void *memptr, unsigned long nbytes)
{
	struct memblock *memlist = &s_memlist;
	unsigned long flags;
	register struct memblock *block, *next, *prev;
	unsigned long top;

	block = (struct memblock *) memptr;
	nbytes = (unsigned long) roundmb(nbytes);

	spin_lock_irqsave(&s_heap_lock, flags);

	prev = memlist;
	next = memlist->next;
	while((next != NULL) && (next < block))
	{
		prev = next;
		next = next->next;
	}

	/* Find top of previous memblock */
	if(prev == memlist)
	{
		top = (unsigned long) NULL;
	}
	else
	{
		top = (unsigned long) prev + prev->length;
	}

	/* Make sure block is not overlapping on prev or next blocks */
	if ((top > (unsigned long) block)
		|| ((next != NULL) && ((unsigned long) block + nbytes) > (unsigned long)next))
	{
		spin_unlock_irqrestore(&s_heap_lock, flags);
		return -1;
	}

	memlist->length += nbytes;

	/* Coalesce with previous block if adjacent */
	if(top == (unsigned long) block)
	{
		prev->length += nbytes;
		block = prev;
	}
	else
	{
		block->next = next;
		block->length = nbytes;
		prev->next = block;
	}

	/* coalesce with next block if adjacent */
	if(((unsigned long) block + block->length) == (unsigned long) next)
	{
		block->length += next->length;
		block->next = next->next;
	}

	spin_unlock_irqrestore(&s_heap_lock, flags);

	return 0;
}

void *heap_sram_alloc(unsigned long nbytes)
{
	struct memblock *pmem;

	/* We don't allocate 0 bytes. */
	if(0 == nbytes)
	{
		return(NULL);
	}

	/* Make room for accounting info */
	nbytes += sizeof(struct memblock);

	/* Acquire memory from system */
	if((pmem = (struct memblock *) qtn_memget(nbytes)) == NULL)
	{
		printk(KERN_DEBUG"Warning: %s failed to allocate 0x%x bytes from caller %p\n", __func__,
				nbytes, __builtin_return_address(0));
		return(NULL);
	}

	/* set accounting info */
	pmem->next = pmem;
	pmem->length = nbytes;

	return((void *)(pmem + 1));  /* +1 to skip accounting info */
}
EXPORT_SYMBOL(heap_sram_alloc);

void heap_sram_free(void *pmem)
{
	struct memblock *block;

	/* We skip NULL pointers. */
	if(NULL == pmem)
	{
		return;
	}

	/* Block points at the memblock we want to free */
	block = (struct memblock *) pmem;

	/* Back up to accounting information */
	block--;

	/* Don't memfree if we fail basic checks */
	if(block->next != block)
	{
		return;
	}

	qtn_memfree(block, block->length);
}
EXPORT_SYMBOL(heap_sram_free);

#endif // #ifdef CONFIG_ARCH_RUBY_NUMA

