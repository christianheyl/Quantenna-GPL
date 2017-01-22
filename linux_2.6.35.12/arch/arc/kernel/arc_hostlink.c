/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/fs.h>       // file_operations
#include <linux/device.h>   // class_create
#include <linux/cdev.h>     // cdev
#include <linux/mm.h>       // VM_IO
#include <linux/module.h>
#include <asm/uaccess.h>

static unsigned char __HOSTLINK__ [4 * PAGE_SIZE]
         __attribute__((aligned (PAGE_SIZE)));


static int arc_hl_mmap(struct file *fp, struct vm_area_struct *vma);
static int arc_hl_ioctl(struct inode *inode, struct file *file,
			  unsigned int cmd, unsigned long arg);


static struct file_operations arc_hl_fops = {
    .owner = THIS_MODULE,
    .ioctl = arc_hl_ioctl,
    .mmap = arc_hl_mmap,
};

static int arc_hl_major;
static int arc_hl_minor;
static int arc_hl_nr_devs = 1;
static char arc_hl_devnm[]="hostlink";
static struct cdev arc_hl_cdev;
static struct class *arc_hl_class;

static int __init arc_hl_linux_glue(void)
{
	dev_t arc_hl_dev;
    int i;

	if (arc_hl_major) {	// Preallocated MAJOR

		arc_hl_dev = MKDEV( arc_hl_major, arc_hl_minor );
		register_chrdev_region( arc_hl_dev, arc_hl_nr_devs, arc_hl_devnm);
	}
	else {			// allocates Major to devices

		alloc_chrdev_region(&arc_hl_dev, 0, arc_hl_nr_devs, arc_hl_devnm);
		arc_hl_major = MAJOR(arc_hl_dev);
	}

	// Populate sysfs entries: creates /sys/class/ sub-node for your device

	arc_hl_class = class_create(THIS_MODULE, arc_hl_devnm);

	// connect file ops with cdev

	cdev_init(&arc_hl_cdev, &arc_hl_fops);
	arc_hl_cdev.owner = THIS_MODULE;

	// Connect major/minor number to cdev
	// makes device available.
	//  device nodes created with 'mknod` are probably already active.

	cdev_add(&arc_hl_cdev, arc_hl_dev, arc_hl_nr_devs);

	// creates /sys/devices/virtual/<dev_name>/<names[i]> node with
	//	link from/sys/class/<dev_name>, needed by mdev.

	for (i = 0; i < arc_hl_nr_devs; i++)
		device_create(arc_hl_class,NULL,MKDEV(MAJOR(arc_hl_dev), i), NULL, arc_hl_devnm);

    printk("Hostlink dev to mknod is %d:%d\n",arc_hl_major, arc_hl_minor);
    return 0;
}

static int __init arc_hl_init(void)
{
    arc_hl_linux_glue();

    printk("Hostlink shared mem PHY addr to mmap is 0x%p\n",__HOSTLINK__);

    return 0;
}

static int arc_hl_mmap(struct file *fp, struct vm_area_struct *vma)
{
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

//    printk("@ ARC Linux HostLink Drv %lx %lx %lx %lx\n",
//        vma->vm_start,  vma->vm_end, vma->vm_pgoff, vma->vm_page_prot);

    if (io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			       vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
        printk("@#$## ARC Hostlink ERROR in mmap\n");
		return -EAGAIN;
    }
	return 0;
}

static int arc_hl_ioctl(struct inode *inode, struct file *file,
			  unsigned int cmd, unsigned long arg)
{
    // we only support, returning the physical addr to mmap in user space
    put_user(__HOSTLINK__, (int __user *)arg);
    return 0;
}
module_init(arc_hl_init);
