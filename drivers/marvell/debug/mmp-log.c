/*
 * Copyright (C) 2014 Samsung Electronics Co, Ltd.
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

#include <asm/tlbflush.h>
#include <linux/bootmem.h>
#include <linux/console.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/memblock.h>
#include <linux/module.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#endif
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#define LOG_MAGIC 0x4d474f4c	/* "LOGM" */

/* These variables are also protected by logbuf_lock */
static char *mmp_log_buf;
static unsigned int *mmp_log_pos;
static unsigned int mmp_log_size;
static int mmplogging;

module_param_named(enable_klog, mmplogging, int, 0664);

extern void register_log_text_hook(void (*f) (char *text, unsigned int size),
		char *buf, unsigned int bufsize);

static void mmp_log_buf_write(struct console *console, const char *text,
			      unsigned int size)
{
	if (mmp_log_buf && mmp_log_pos) {
		unsigned int pos = *mmp_log_pos;

		if (likely(size + pos <= mmp_log_size))
			memcpy(&mmp_log_buf[pos], text, size);
		else {
			unsigned int first = mmp_log_size - pos;
			unsigned int second = size - first;
			memcpy(&mmp_log_buf[pos], text, first);
			memcpy(&mmp_log_buf[0], text + first, second);
		}
        (*mmp_log_pos) += size; /* Check overflow */
		if (unlikely(*mmp_log_pos >= mmp_log_size))
			*mmp_log_pos -= mmp_log_size;
	}
}

static struct console mmp_console = {
	.name	= "mmp_log_buf",
	.write	= mmp_log_buf_write,
	.flags	= CON_PRINTBUFFER | CON_ENABLED | CON_ANYTIME,
	.index	= -1,
};

static inline void emit_mmp_log(char *text, unsigned int size)
{
	if (mmplogging && mmp_log_buf && mmp_log_pos) {
		unsigned int pos = *mmp_log_pos;

		if (likely(size + pos <= mmp_log_size))
			memcpy(&mmp_log_buf[pos], text, size);
		else {
			unsigned int first = mmp_log_size - pos;
			unsigned int second = size - first;
			memcpy(&mmp_log_buf[pos], text, first);
			memcpy(&mmp_log_buf[0], text + first, second);
		}
		(*mmp_log_pos) += size;

		/* Check overflow */
		if (unlikely(*mmp_log_pos >= mmp_log_size))
			*mmp_log_pos -= mmp_log_size;
	}
	return;
}

static u32 __initdata mmplog_base;
static u32 __initdata mmplog_size;
static u32 __initdata mmplog_reserved;

#ifdef CONFIG_OF
static int __init mmplog_fdt_find_info(unsigned long node, const char *uname,
			int depth, void *data)
{
	__be32 *prop;
	unsigned long len;

	if (!of_flat_dt_is_compatible(node, "marvell,mmplog-heap")) {
		return 0;
	}

	prop = of_get_flat_dt_prop(node, "mmplog-base", &len);
	if (!prop || (len != sizeof(unsigned int))) {
		pr_err("%s: Can't find mmplog-base property\n", __func__);
		return 0;
	}
	mmplog_base = be32_to_cpu(prop[0]);

	prop = of_get_flat_dt_prop(node, "mmplog-size", &len);
	if (!prop || (len != sizeof(unsigned int))) {
		pr_err("%s: Can't find mmplog-size property\n", __func__);
		return 0;
	}
	mmplog_size = be32_to_cpu(prop[0]);

	return 1;
}

void __init pxa_reserve_logmem(void)
{
		if (of_scan_flat_dt(mmplog_fdt_find_info, NULL)) {
			BUG_ON(memblock_reserve(mmplog_base, mmplog_size) != 0);
			pr_info("Reserved mmplog memory : %dMB at %#.8x\n",
				((unsigned)mmplog_size >> 20), (unsigned)mmplog_base);
			mmplog_reserved = 1;
		} else
			pr_err("no mmplog buffer was reserved\n");
}
#endif

static int __init mmp_log_buf_init(void)
{
	unsigned int i, base, size, page_count;
	unsigned int *mmp_log_mag;
	phys_addr_t page_start;
	pgprot_t prot;
	void *vaddr;
	struct page **pages;

	base = mmplog_base;
	size = mmplog_size;

	if (!base) {
		pr_err("%s: base missing\n", __func__);
		return -EINVAL;
	}

	if (!size) {
		pr_err("%s: size missing\n", __func__);
		return -EINVAL;
	}

	if (mmplog_reserved) {
		page_start = base - offset_in_page(base);
		page_count = DIV_ROUND_UP(size + offset_in_page(base), PAGE_SIZE);

#ifdef CONFIG_ARM64
		prot = pgprot_writecombine(PAGE_KERNEL);
#else
		prot = pgprot_noncached(PAGE_KERNEL);
#endif
		pages = kmalloc(sizeof(struct page *) * page_count, GFP_KERNEL);
		if (!pages) {
			pr_err("%s: Failed to allocate array for %u pages\n", __func__,
			       page_count);
			return -ENOMEM;
		}

		for (i = 0; i < page_count; i++) {
			phys_addr_t addr = page_start + i * PAGE_SIZE;
			pages[i] = pfn_to_page(addr >> PAGE_SHIFT);

			dma_sync_single_for_device(NULL, addr, size, DMA_TO_DEVICE);
		}
		vaddr = vmap(pages, page_count, VM_MAP, prot);
		kfree(pages);

		mmp_log_size = size - (sizeof(*mmp_log_pos) + sizeof(*mmp_log_mag));
		mmp_log_buf = vaddr;
		mmp_log_pos = (unsigned int *)(vaddr + mmp_log_size);
		mmp_log_mag = (unsigned int *)(vaddr + mmp_log_size + sizeof(*mmp_log_pos));

		pr_info("%s: *mmp_log_mag:%x *mmp_log_pos:%x "
		"mmp_log_buf:%p mmp_log_size:%u\n",
		__func__, *mmp_log_mag, *mmp_log_pos, mmp_log_buf,
		mmp_log_size);

		if (*mmp_log_mag != LOG_MAGIC) {
			pr_info("%s: no old log found\n", __func__);
			*mmp_log_pos = 0;
			*mmp_log_mag = LOG_MAGIC;
		}

		/* save the pre-console log into mmp log buffer */
		register_console(&mmp_console);
		unregister_console(&mmp_console);
		mmplogging = 1;
		register_log_text_hook(emit_mmp_log, mmp_log_buf, mmp_log_size);
	}

	return 0;
}
arch_initcall(mmp_log_buf_init);
