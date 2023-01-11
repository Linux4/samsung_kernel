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
#include <linux/sec-debug.h>
#include <linux/uaccess.h>
#ifdef CONFIG_SEC_LOG_LAST_KMSG
#include <linux/proc_fs.h>
#endif
#ifdef CONFIG_ARM
#include <linux/vmalloc.h>
#endif

#define LOG_MAGIC 0x4d474f4c	/* "LOGM" */

/* These variables are also protected by logbuf_lock */
static unsigned int *sec_log_pos;
static char *sec_log_buf;
static unsigned int sec_log_size;

static int seclogging;

module_param_named(enable_klog, seclogging, int, 0664);

#ifdef CONFIG_SEC_LOG_LAST_KMSG
static char *last_kmsg_buffer;
static unsigned int last_kmsg_size;
static void __init sec_log_save_old(unsigned int lastkbase, unsigned int lastksize);
#else
static inline void sec_log_save_old(unsigned int lastkbase, unsigned int lastksize)
{
}
#endif

extern void register_log_text_hook(void (*f) (char *text, unsigned int size),
				   char *buf, unsigned int *position,
				   unsigned int bufsize);

static void sec_log_buf_write(struct console *console, const char *text,
			      unsigned int size)
{
	if (sec_log_buf && sec_log_pos) {
		unsigned int pos = *sec_log_pos;

		if (likely(size + pos <= sec_log_size))
			memcpy(&sec_log_buf[pos], text, size);
		else {
			unsigned int first = sec_log_size - pos;
			unsigned int second = size - first;
			memcpy(&sec_log_buf[pos], text, first);
			memcpy(&sec_log_buf[0], text + first, second);
		}
		(*sec_log_pos) += size;

		/* Check overflow */
		if (unlikely(*sec_log_pos >= sec_log_size))
			*sec_log_pos -= sec_log_size;
	}
}

static struct console sec_console = {
	.name	= "sec_log_buf",
	.write	= sec_log_buf_write,
	.flags	= CON_PRINTBUFFER | CON_ENABLED | CON_ANYTIME,
	.index	= -1,
};

static inline void emit_sec_log(char *text, unsigned int size)
{
	unsigned int pos = *sec_log_pos;

	if (likely(size + pos <= sec_log_size))
		memcpy(&sec_log_buf[pos], text, size);
	else {
		unsigned int first = sec_log_size - pos;
		unsigned int second = size - first;
		memcpy(&sec_log_buf[pos], text, first);
		memcpy(&sec_log_buf[0], text + first, second);
	}
	(*sec_log_pos) += size;

	/* Check overflow */
	if (unlikely(*sec_log_pos >= sec_log_size))
		*sec_log_pos -= sec_log_size;

	return;
}

static u32 __initdata seclog_base = 0x08140000;
static u32 __initdata seclog_size = 0x00100000;
static u32 __initdata seclastklog_base = 0x08240000;
static u32 __initdata seclastklog_size = 0x00100000;
static u32 __initdata seclog_reserved;

#ifdef CONFIG_OF
static int __init mmp_seclog_fdt_find_info(unsigned long node, const char *uname,
			int depth, void *data)
{
	__be32 *prop;
	unsigned long len;

	if (!of_flat_dt_is_compatible(node, "marvell,seclog-heap"))
		return 0;

	prop = of_get_flat_dt_prop(node, "seclog-base", &len);
	if (!prop || (len != sizeof(unsigned int))) {
		pr_err("%s: Can't find seclog-base property\n", __func__);
		return 0;
	}
	seclog_base = be32_to_cpu(prop[0]);

	prop = of_get_flat_dt_prop(node, "seclog-size", &len);
	if (!prop || (len != sizeof(unsigned int))) {
		pr_err("%s: Can't find seclog-size property\n", __func__);
		return 0;
	}
	seclog_size = be32_to_cpu(prop[0]);

	prop = of_get_flat_dt_prop(node, "seclastklog-base", &len);
	if (!prop || (len != sizeof(unsigned int))) {
		pr_err("%s: Can't find seclastklog-base property\n", __func__);
		return 0;
	}
	seclastklog_base = be32_to_cpu(prop[0]);

	prop = of_get_flat_dt_prop(node, "seclastklog-size", &len);
	if (!prop || (len != sizeof(unsigned int))) {
		pr_err("%s: Can't find seclastklog-base property\n", __func__);
		return 0;
	}
	seclastklog_size = be32_to_cpu(prop[0]);

	return 1;
}

void __init mmp_reserve_seclogmem(void)
{
	if (sec_debug_level.uint_val == 1)
		if (of_scan_flat_dt(mmp_seclog_fdt_find_info, NULL)) {
			BUG_ON(memblock_reserve(seclog_base, seclog_size) != 0);
			BUG_ON(memblock_reserve(seclastklog_base, seclastklog_size) != 0);
			pr_info("Reserved seclog memory : %dMB at %#.8x\n",
				(unsigned)seclog_size/0x100000,
				(unsigned)seclog_base);
			pr_info("Reserved seclastklog memory : %dMB at %#.8x\n",
				(unsigned)seclastklog_size/0x100000,
				(unsigned)seclastklog_base);
			seclog_reserved = 1;
		}
}
#endif

static int __init sec_log_buf_init(void)
{
	unsigned int i, base, size, lastkbase, lastksize, page_count;
	unsigned int *sec_log_mag;
	phys_addr_t page_start;
	pgprot_t prot;
	void *vaddr;
	struct page **pages;

	base = seclog_base;
	size = seclog_size;
	lastkbase = seclastklog_base;
	lastksize = seclastklog_size;

	if (!base || !lastkbase) {
		pr_err("%s: base missing\n", __func__);
		return -EINVAL;
	}

	if (!size || !lastksize) {
		pr_err("%s: size missing\n", __func__);
		return -EINVAL;
	}

	if (sec_debug_level.uint_val == 1 && seclog_reserved) {
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

		sec_log_size = size - (sizeof(*sec_log_pos) + sizeof(*sec_log_mag));
		sec_log_buf = vaddr;
		sec_log_pos = (unsigned int *)(vaddr + sec_log_size);
		sec_log_mag = (unsigned int *)(vaddr + sec_log_size + sizeof(*sec_log_pos));

		pr_info("%s: *sec_log_mag:%x *sec_log_pos:%x "
		"sec_log_buf:%p sec_log_size:%u\n",
		__func__, *sec_log_mag, *sec_log_pos, sec_log_buf,
		sec_log_size);

		if (*sec_log_mag != LOG_MAGIC) {
			pr_info("%s: no old log found\n", __func__);
			*sec_log_pos = 0;
			*sec_log_mag = LOG_MAGIC;
		} else {
			sec_log_save_old(lastkbase, lastksize);
		}

		register_console(&sec_console);
		unregister_console(&sec_console);
		seclogging = 1;
		register_log_text_hook(emit_sec_log, sec_log_buf, sec_log_pos,
				       sec_log_size);

		sec_getlog_supply_kloginfo(phys_to_virt(base), last_kmsg_buffer);
	}

	return 0;
}
arch_initcall(sec_log_buf_init);

#ifdef CONFIG_SEC_LOG_LAST_KMSG
static void __init sec_log_save_old(unsigned int lastkbase, unsigned int lastksize)
{
	/* provide previous log as last_kmsg */
	last_kmsg_size = min(sec_log_size, *sec_log_pos);
	last_kmsg_buffer = phys_to_virt(lastkbase);

	if (last_kmsg_size && last_kmsg_buffer) {
		memcpy(last_kmsg_buffer, sec_log_buf, lastksize);

		pr_info("%s: saved old log at %u@%p\n",
			__func__, last_kmsg_size, last_kmsg_buffer);
	} else
		pr_err("%s: failed saving old log %u@%p\n",
		       __func__, last_kmsg_size, last_kmsg_buffer);
}

static ssize_t sec_log_read_old(struct file *file, char __user * buf,
				size_t len, loff_t * offset)
{
	loff_t pos = *offset;
	ssize_t count;

	if (pos >= last_kmsg_size)
		return 0;

	count = min(len, (size_t) (last_kmsg_size - pos));
	if (copy_to_user(buf, last_kmsg_buffer + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}

static const struct file_operations last_kmsg_file_ops = {
	.owner = THIS_MODULE,
	.read = sec_log_read_old,
};

static int __init sec_log_late_init(void)
{
	struct proc_dir_entry *entry;

	if (last_kmsg_buffer == NULL)
		return 0;

	entry = proc_create("last_kmsg", S_IFREG | S_IRUGO,
			    NULL, &last_kmsg_file_ops);
	if (!entry) {
		pr_err("%s: failed to create proc entry\n", __func__);
		return 0;
	}
	//entry->size = last_kmsg_size;
	proc_set_size(entry, last_kmsg_size);

	return 0;
}

late_initcall(sec_log_late_init);
#endif
