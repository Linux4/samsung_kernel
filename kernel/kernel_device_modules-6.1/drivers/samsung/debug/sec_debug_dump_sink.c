/*
 *sec_debug_dump_sink.c
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/proc_fs.h>
#include <linux/sec_debug.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>
#include <linux/memblock.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <uapi/linux/fiemap.h>

#define SHA256_DIGEST_SIZE      32
#define SHA256_BLOCK_SIZE       64
#define SHA256_DIGEST_LENGTH  SHA256_DIGEST_SIZE

int dumpsink_initialized;
EXPORT_SYMBOL(dumpsink_initialized);

static unsigned int __read_mostly upload_count;
module_param(upload_count, uint, 0440);

unsigned int __read_mostly dump_sink;
EXPORT_SYMBOL(dump_sink);

extern struct reserved_mem *secdbg_log_get_rmem(const char *compatible);

static int sec_sdcard_ramdump(const char *val, const struct kernel_param *kp)
{
	int ret = 0;

	ret = kstrtouint(val, 16, &dump_sink);
	if (ret < 0) {
		pr_err("%s: fail to set dump_sink(%d)\n", __func__, ret);
		return -1;
	}

	pr_crit("%s: %s %x\n", __func__, val, dump_sink);

	return 0;
}

static const struct kernel_param_ops sec_dump_sink_ops = {
	.set	= sec_sdcard_ramdump,
	.get	= param_get_uint,
};

module_param_cb(dump_sink, &sec_dump_sink_ops, &dump_sink, 0644);

static phys_addr_t sec_rdx_bootdev_paddr;
static unsigned long long sec_rdx_bootdev_size;
static DEFINE_MUTEX(rdx_bootdev_mutex);

/* FIXME: this is a copy of 'free_reserved_area' of 'page_alloc.c' */
static unsigned long __free_reserved_area(void *start, void *end, int poison, const char *s)
{
	void *pos;
	unsigned long pages = 0;

	start = (void *)PAGE_ALIGN((unsigned long)start);
	end = (void *)((unsigned long)end & PAGE_MASK);
	for (pos = start; pos < end; pos += PAGE_SIZE, pages++) {
		struct page *page = virt_to_page(pos);
		void *direct_map_addr;

		/*
		 * 'direct_map_addr' might be different from 'pos'
		 * because some architectures' virt_to_page()
		 * work with aliases.  Getting the direct map
		 * address ensures that we get a _writeable_
		 * alias for the memset().
		 */
		direct_map_addr = page_address(page);
		/*
		 * Perform a kasan-unchecked memset() since this memory
		 * has not been initialized.
		 */
		direct_map_addr = kasan_reset_tag(direct_map_addr);
		if ((unsigned int)poison <= 0xFF)
			memset(direct_map_addr, poison, PAGE_SIZE);

		free_reserved_page(page);
	}

	if (pages && s)
		pr_info("Freeing %s memory: %ldK\n",
			s, pages << (PAGE_SHIFT - 10));

	return pages;
}

static void sec_free_rdx_bootdev(phys_addr_t paddr, u64 size)
{
/* caution : this fuction should be called in rdx_bootdev_mutex protected region. */
	pr_info("start (0x%llx, 0x%llx)\n", paddr, size);

	if (!sec_rdx_bootdev_paddr) {
		pr_err("reserved addr is null\n");
		goto out;
	}

	if (!sec_rdx_bootdev_size) {
		pr_err("reserved size is zero\n");
		goto out;
	}

	if (paddr < sec_rdx_bootdev_paddr) {
		pr_err("paddr is not valid\n");
		goto out;
	}

	if (paddr + size > sec_rdx_bootdev_paddr + sec_rdx_bootdev_size) {
		pr_err("range is not valid\n");
		goto out;
	}

	memset(phys_to_virt(paddr), 0, size);

	memblock_free((void *)paddr, size);

	__free_reserved_area(phys_to_virt(paddr), phys_to_virt(paddr) + size, -1, "secdbg-rdx-bootdev");

	if (sec_rdx_bootdev_paddr == paddr) {
		sec_rdx_bootdev_paddr = 0;
	}

	sec_rdx_bootdev_size -= size;

out:
	pr_info("end\n");
}

static ssize_t sec_rdx_bootdev_proc_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	int err = 0;
	struct fiemap *pfiemap;
	phys_addr_t paddr;
	u64 size;

	mutex_lock(&rdx_bootdev_mutex);

	paddr = sec_rdx_bootdev_paddr;
	size = sec_rdx_bootdev_size;

	if (!sec_rdx_bootdev_paddr) {
		pr_err("sec_rdx_bootdev_paddr is null\n");
		err = -EFAULT;
		goto out;
	}

	if (!buf) {
		err = -ENODEV;
	} else {
		if (count > sec_rdx_bootdev_size) {
			pr_err("size is wrong %lu > %llu\n", count, sec_rdx_bootdev_size);
			err = -EINVAL;
			goto out;
		}

		if (copy_from_user(phys_to_virt(sec_rdx_bootdev_paddr),
				buf, count)) {
			pr_err("copy_from_user failed\n");
			err = -EFAULT;
			goto out;
		}

		pfiemap = phys_to_virt(sec_rdx_bootdev_paddr) + SHA256_DIGEST_LENGTH;
		paddr = virt_to_phys(&pfiemap->fm_extents[pfiemap->fm_mapped_extents]);
		if (paddr <
			sec_rdx_bootdev_paddr + sec_rdx_bootdev_size) {
			paddr = ALIGN(paddr, PAGE_SIZE);
			size = paddr - sec_rdx_bootdev_paddr;
			size = sec_rdx_bootdev_size - size;
		}
	}

out:
	sec_free_rdx_bootdev(paddr, size);
	mutex_unlock(&rdx_bootdev_mutex);
	return err < 0 ? err : count;
}

static const struct proc_ops sec_rdx_bootdev_fops = {
	.proc_write = sec_rdx_bootdev_proc_write,
};

static int sec_upload_count_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d", upload_count);
	return 0;
}

static int sec_upload_count_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_upload_count_show, NULL);
}

static const struct proc_ops sec_upload_count_proc_fops = {
	.proc_open = sec_upload_count_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int sec_map_rdx_bootdev_region(void)
{
	struct reserved_mem *rmem;

	rmem = secdbg_log_get_rmem("samsung,secdbg-rdx-bootdev");
	if (!rmem) {
		pr_err("%s, failed to find secdbg-rdx-bootdev\n", __func__);
		return -EINVAL;
	}

	sec_rdx_bootdev_paddr = rmem->base;
	sec_rdx_bootdev_size = rmem->size;

	pr_info("%s, secdbg-rdx-bootdev : addr [0x%llx], size [0x%llx]\n", __func__,
		sec_rdx_bootdev_paddr, sec_rdx_bootdev_size);

	if (is_debug_level_low()) {
		pr_info("%s, debug level is low. sec_debug is not enabled\n", __func__);
		sec_free_rdx_bootdev(sec_rdx_bootdev_paddr, sec_rdx_bootdev_size);
		return 0;
	}

	memset(phys_to_virt(sec_rdx_bootdev_paddr), 0, SHA256_DIGEST_LENGTH);

	return 0;
}

int sec_dump_sink_init(void)
{
	struct proc_dir_entry *entry;

	sec_map_rdx_bootdev_region();

	entry = proc_create("rdx_bootdev", 0220, NULL, &sec_rdx_bootdev_fops);

	if (!entry) {
		pr_err("%s: fail to create proc entry (rdx_bootdev)\n", __func__);
		return -ENOMEM;
	}
	entry = proc_create("upload_count", 0440, NULL, &sec_upload_count_proc_fops);

	if (!entry) {
		pr_err("%s: fail to create proc entry (upload_count)\n", __func__);
		return -ENOMEM;
	}
	pr_info("%s: success to create proc entry\n", __func__);
	
	dumpsink_initialized = 1;

	return 0;
}
EXPORT_SYMBOL(sec_dump_sink_init);
