/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.

 *(C) Copyright 2008 Marvell International Ltd.
 * All Rights Reserved
 */

#include <linux/version.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/tty_ldisc.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/string.h>

#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <asm/pgtable.h>


#define DRIVER_VERSION "v1.0"
#define DRIVER_AUTHOR "Marvell"
#define DRIVER_DESC "Driver for HW access via physical address"

int hwmap_major;
int hwmap_minor;
static struct class *hwmap_class;
struct cdev *cdev;
/* Module information */
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

static void hwmap_vma_open(struct vm_area_struct *vma)
{
#if defined(HWMAP_DEBUG)
	pr_debug("HWMAP OPEN 0x%lx -> 0x%lx (0x%lx)\n", vma->vm_start,
	       vma->vm_pgoff << PAGE_SHIFT, vma->vm_page_prot);
#endif
}

static void hwmap_vma_close(struct vm_area_struct *vma)
{
#if defined(HWMAP_DEBUG)
	pr_debug("HWMAP CLOSE 0x%lx -> 0x%lx\n", vma->vm_start,
	       vma->vm_pgoff << PAGE_SHIFT);
#endif
}

/* These are mostly for debug: do nothing useful otherwise */
static struct vm_operations_struct vm_ops = {
	.open = hwmap_vma_open,
	.close = hwmap_vma_close
};

/* device memory map method */
/*
 * vma->vm_end, vma->vm_start: specify the user space process address range
 *                             assigned when mmap has been called;
 * vma->vm_pgoff: is the physical address supplied by user to mmap in the
 *                last argument (off)
 * However, mmap restricts the offset, so we pass this shifted 12 bits right.
 */
int hwmap_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long pa = vma->vm_pgoff;

	/* we do not want to have this area swapped out, lock it */
	vma->vm_flags |= (VM_IO | VM_DONTEXPAND | VM_DONTDUMP);
	/* see linux/drivers/char/mem.c */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	/* TBD: without this vm_page_prot=0x53 and write seem to not
	 *      reach the destination:
	 * - no fault on write
	 * - read immediately (same user process) return the new value
	 * - read after a second by another user process instance return
	 *   original value
	 * Why PROT_WRITE specified by mmap caller does not take effect?
	 * MAP_PRIVATE used by app results in copy-on-write behaviour,
	 *     which is irrelevant for this application
	 * vma->vm_page_prot|=L_PTE_WRITE; */

	/* check if addr is normal memory */
	if (pfn_valid(pa)) {
		if (remap_pfn_range(vma, vma->vm_start, pa,
				       size, vma->vm_page_prot)) {
			pr_err("remap page range failed\n");
			return -ENXIO;
		}
	} else if (io_remap_pfn_range(vma, vma->vm_start, pa,
			       size, vma->vm_page_prot)) {
		pr_err("remap page range failed\n");
		return -ENXIO;
	}
	vma->vm_ops = &vm_ops;
	hwmap_vma_open(vma);
	return 0;
}

static const struct file_operations hwmap_fops = {
	.owner =	THIS_MODULE,
	.mmap =		hwmap_mmap
};

void hwmap_cleanup_module(void)
{
	dev_t devno = MKDEV(hwmap_major, hwmap_minor);

	if (cdev) {
		cdev_del(cdev);
		device_destroy(hwmap_class, devno);
	}
	class_destroy(hwmap_class);
	unregister_chrdev_region(devno, 1);

}

static int __init hwmap_init(void)
{
	int err = 0;
	dev_t dev = 0;
	if (hwmap_major) {
		dev = MKDEV(hwmap_major, hwmap_minor);
		err = register_chrdev_region(dev, 1, "hwmap");
	} else {
		err = alloc_chrdev_region(&dev, hwmap_minor, 1, "hwmap");
		hwmap_major = MAJOR(dev);
	}

	if (err < 0) {
		pr_err("%s: can't get major %d\n", __func__,
		       hwmap_major);
		return err;
	}
	hwmap_class = class_create(THIS_MODULE, "hwmap");
	cdev = cdev_alloc();
	if (!cdev) {
		err = -ENOMEM;
		goto fail;
	}
	dev = MKDEV(hwmap_major, hwmap_minor);

	cdev->ops = &hwmap_fops;
	cdev->owner = THIS_MODULE;
	err = cdev_add(cdev, dev, 1);
	if (err < 0)
		pr_err("%s: Error %d adding hwmap\n", __func__,
		       err);
	device_create(hwmap_class, NULL, dev, NULL, "hwmap");
	return 0;
fail:
	hwmap_cleanup_module();

	return err;
}

static void __exit hwmap_exit(void)
{
	hwmap_cleanup_module();
	pr_debug("%s unregistering chrdev done\n", __func__);
}

#ifdef MODULE
module_init(hwmap_init);
module_exit(hwmap_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("HW memory mapped access");
#else
subsys_initcall(hwmap_init);
#endif
