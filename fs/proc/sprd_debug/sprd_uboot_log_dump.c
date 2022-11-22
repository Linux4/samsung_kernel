/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/memblock.h>
#include <linux/io.h>
#include <linux/proc_fs.h>

#define PROC_DUMP_PARENT "bootloader"
#define PROC_DUMP	 "log_buf"

static unsigned int log_start = ~0x1;
static unsigned int log_size  = ~0x1;

static int __init set_ram_range(char *str)
{

	if (get_option(&str, &log_start) == 2)
		get_option(&str, &log_size);

	return 1;
}

__setup("boot_ram_log=", set_ram_range);

static int log_buf_open(struct inode *inode, struct file *filp)
{
	void  *base;

	/* determine the ram area is valid or not. */
	if (!valid_phys_addr_range(log_start, log_size)
	    || !memblock_is_region_reserved(log_start, log_size)) {
		pr_err("%s: check memory range failed!\n", __func__);

		return -EINVAL;
	}

	base = ioremap(log_start, log_size);
	filp->private_data = base;
	if (!base) {
		pr_err("%s: remap error!\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

static int log_buf_release(struct inode *inode, struct file *filp)
{
	void *base = filp->private_data;

	if (base)
		iounmap(base);

	return 0;
}


static ssize_t log_buf_read(struct file *filp, char __user *buffer,
			size_t len, loff_t *offset)
{
	void  *base = filp->private_data;

	if (!base)
		return -ENOMEM;

	return simple_read_from_buffer(buffer, len, offset, base, log_size);
}


static int log_buf_mmap(struct file *filp, struct vm_area_struct *vma)
{
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	return vm_iomap_memory(vma, log_start, log_size);
}


static const struct file_operations log_buf_fops = {
	.owner	 = THIS_MODULE,
	.read	 = log_buf_read,
	.open	 = log_buf_open,
	.mmap	 = log_buf_mmap,
	.release = log_buf_release,
};


static int __init uboot_log_dump(void)
{
	int err;
	struct proc_dir_entry *entry_parent, *entry_dump;

	entry_parent = proc_mkdir("bootloader", NULL);
	if (!entry_parent) {
		pr_err("u-boot log dump: create proc file failed\n");
		return -ENOMEM;
	}

	/* other user is NOT permitted to dump buffer */
	entry_dump = proc_create_data(PROC_DUMP, S_IRUSR, entry_parent,
			&log_buf_fops, NULL);

	err = entry_dump ? 0 : -ENOMEM;

	return err;
}

module_init(uboot_log_dump);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Spreadtrum U-boot log dump support");
