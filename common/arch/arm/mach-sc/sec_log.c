/*
 *  arch/arm/mach-sc8825/sec-log.c
 *
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

#include <linux/kernel.h>
#include <linux/bootmem.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>

#include <mach/sec_debug.h>
#include <asm/mach/map.h>
#include <asm/tlbflush.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#ifdef CONFIG_SEC_LOG_LAST_KMSG
#include <linux/proc_fs.h>
#endif
#ifdef CONFIG_SEC_LOG_BUF_NOCACHE
#include <linux/memblock.h>
#endif

/*
 * Example usage: sec_log=256K@0x45000000
 * In above case, log_buf size is 256KB and its base address is
 * 0x45000000 physically. Actually, *(int *)(base - 8) is log_magic and
 * *(int *)(base - 4) is log_ptr. So we reserve (size + 8) bytes from
 * (base - 8).
 */
#define LOG_MAGIC 0x4d474f4c	/* "LOGM" */
#define RAM_CONSOLE_SIG 0x43474244 /* match in bootloader for panic display*/
//extern void register_log_char_hook(void (*f) (char c));

/* These variables are also protected by logbuf_lock */
static unsigned *sec_log_ptr;
static char *sec_log_buf;
static unsigned sec_log_size;

struct sec_log_buffer *sec_log_buffer;

#ifdef CONFIG_SEC_LOG_LAST_KMSG
static char *last_kmsg_buffer;
static unsigned last_kmsg_size;
static void __init sec_log_save_old(void);
#else
static inline void sec_log_save_old(void)
{
}
#endif
#define LOG_LENGTH_B (1024*1024)

extern void register_log_text_hook(void (*f)(char *text, size_t size),
	char *buf, unsigned *position, size_t bufsize);

static inline void emit_sec_log(char *text, size_t size)
{
	if (sec_log_buf && sec_log_ptr) {

		size_t pos = 0; 

		pos = *sec_log_ptr;

		if (likely(size + pos <= sec_log_size)) {
			memcpy(&sec_log_buf[pos], text, size);
			(*sec_log_ptr) += size;
		}
		else {
			size_t first = sec_log_size - pos;
			size_t second = size - first;
			memcpy(&sec_log_buf[pos], text, first);
			memcpy(&sec_log_buf[0], text + first, second);
			(*sec_log_ptr) = second;
		}
		sec_log_buffer->start = *sec_log_ptr;			  // Update start pointer for panic display
		sec_log_buffer->size += size;					  // Updated size based on print parameters
		sec_log_buffer->size = sec_log_buffer->size < sec_log_size ? sec_log_buffer->size : sec_log_size; // Boundary checking
		
	}
}

int sec_log_buf_nocache_enable=-1;
phys_addr_t sec_logbuf_base_addr;
unsigned sec_logbuf_size;

static int __init sec_log_setup(char *str)
{
	unsigned size=memparse(str, &str);
	unsigned long base = 0;
	unsigned alloc_size=0;
	unsigned size_of_struct_sec_log_buffer=0;

	if (!size /*|| size != roundup_pow_of_two(size)*/ || *str != '@'
				    || kstrtoul(str + 1, 0, &base))
		return -1;
	sec_logbuf_base_addr = base;
	sec_logbuf_size = size;
#ifdef CONFIG_SEC_LOG_BUF_NOCACHE
	sec_log_buf_nocache_enable=1;
#else
	size_of_struct_sec_log_buffer = ((sizeof(struct sec_log_buffer)+4)/4)*4;
	// sec_logbuf_size(1M) + sec_log_buffer(16B) + sec_log_ptr(4B) + sec_log_mag(4B)
	alloc_size = sec_logbuf_size + size_of_struct_sec_log_buffer + 4 + 4; 
	if(reserve_bootmem(sec_logbuf_base_addr,alloc_size, BOOTMEM_EXCLUSIVE))
	{
		printk("%s: fail to reserve memory\n",__func__);
		return -1;
	}
	else {
			printk("%s: success to reserve memory, base_phys=%x, size=%x\n",__func__,sec_logbuf_base_addr, alloc_size);
	}	
	sec_log_buf_nocache_enable=0;
#endif	
	return 0;
	
}
#ifdef CONFIG_SEC_LOG_BUF_NOCACHE
early_param("sec_log",sec_log_setup);
#else
__setup("sec_log=", sec_log_setup);
#endif

#ifdef CONFIG_SEC_DEBUG_REG_ACCESS
void sec_debug_set_last_regs_access(void)
{
	unsigned addr = (unsigned)sec_log_buf;
	unsigned size_struct_sec_log_buffer = sizeof(struct sec_log_buffer);
	extern struct sec_debug_regs_access *sec_debug_last_regs_access;
	extern char *sec_debug_local_hwlocks_status;
	
	size_struct_sec_log_buffer = (((size_struct_sec_log_buffer + 4) / 4)* 4);
	addr = addr+sec_logbuf_size+size_struct_sec_log_buffer;
	sec_debug_last_regs_access = addr;
	sec_debug_local_hwlocks_status = sec_debug_last_regs_access + NR_CPUS;
	memset(sec_debug_local_hwlocks_status,0,64);
}
#endif

static int __init sec_logbuf_conf_memtype(void)
{
#ifdef CONFIG_SEC_LOG_BUF_NOCACHE
	void __iomem *base = 0;
#else
	unsigned long base = 0;
#endif
	unsigned *sec_log_mag;
	unsigned size_of_log_ptr = sizeof(*sec_log_mag);
	unsigned size_of_log_mag = sizeof(*sec_log_ptr);
	unsigned size_struct_sec_log_buffer = sizeof(struct sec_log_buffer);
	
	/* Round-up to 4 byte alignment */
	size_struct_sec_log_buffer = (((size_struct_sec_log_buffer + 4) / 4)* 4);
	pr_info("%s: size_of_log_ptr:%d size_of_log_mag:%d"
		"size_struct_sec_log_buffer:%d\n",
		__func__, size_of_log_ptr, size_of_log_mag,
					size_struct_sec_log_buffer);

	/**********************************************************************
	 ___________________
	|   sec_log_buffer  |   16 bytes,4 byte aligned sec_log_buffer struct
	|___________________|
	|                   |
	|   sec_log_buf     |	1 MB, sec log buffer
	|                   |
	|___________________|
	|   sec_log_ptr	    |	4 bytes,which holds the index of sec_log_buf[]
	|___________________|
	|   sec_log_mag     |	4 bytes, which holds the magic address
	|___________________|

	***********************************************************************/

	if(sec_log_buf_nocache_enable == 1)
	{
		base = ioremap_nocache(sec_logbuf_base_addr, LOG_LENGTH_B);
		if(base == NULL){
			printk("ioremap nocache fail\n");
			return -1;
		}
		sec_log_buffer = base;
		sec_log_mag = base+ size_struct_sec_log_buffer +
							sec_logbuf_size + size_of_log_ptr;
		sec_log_ptr = base + size_struct_sec_log_buffer + sec_logbuf_size;
		sec_log_buf = base + size_struct_sec_log_buffer;
		sec_log_size = sec_logbuf_size;
	}
	else if(sec_log_buf_nocache_enable == 0) {
		base = sec_logbuf_base_addr;
		sec_log_mag = phys_to_virt(base) + size_struct_sec_log_buffer +
							sec_logbuf_size + size_of_log_ptr;
		sec_log_ptr = phys_to_virt(base) + size_struct_sec_log_buffer + sec_logbuf_size;
		sec_log_buffer = phys_to_virt(base);                
		sec_log_buf = phys_to_virt(base) + size_struct_sec_log_buffer;
		sec_log_size = sec_logbuf_size;

	}
	else {
		printk("%s: sec logbuffer fail to alloc both cache and non-cache\n",__func__);
		return -1;
	}

	pr_info("%s: Addresses: struct sec_log_buffer:%p sec_log_buf:%p"
		"sec_log_ptr:%p sec_log_mag:%p\n",
		__func__, sec_log_buffer, sec_log_buf, sec_log_ptr,
		sec_log_mag);

	pr_info("%s: *sec_log_mag:%x *sec_log_ptr:%x "
		"sec_log_buf:%p sec_log_size:%d\n",
		__func__, *sec_log_mag, *sec_log_ptr, sec_log_buf,
		sec_log_size);

	if (*sec_log_mag != LOG_MAGIC) {
		pr_info("%s: no old log found\n", __func__);
		*sec_log_ptr = 0;
		*sec_log_mag = LOG_MAGIC;
	} else {
		if (*sec_log_ptr)
			sec_log_save_old();
		else
			pr_err("%s: unlikely *sec_log_ptr is zero, skip sec_log_save_old\n", __func__);
	}
#ifdef CONFIG_SEC_DEBUG_REG_ACCESS
	sec_debug_set_last_regs_access();
#endif
	sec_log_buffer->sig = RAM_CONSOLE_SIG;
	sec_log_buffer->size = 0;
	sec_log_buffer->start = *sec_log_ptr;
	register_log_text_hook(emit_sec_log, sec_log_buf, sec_log_ptr,
		sec_log_size);
		
	sec_getlog_supply_kloginfo(sec_log_buf );

	return 0;
}
module_init(sec_logbuf_conf_memtype);

#ifdef CONFIG_SEC_LOG_LAST_KMSG
static void __init sec_log_save_old(void)
{
	/* provide previous log as last_kmsg */
	last_kmsg_size =
	    min((unsigned)(1 << CONFIG_LOG_BUF_SHIFT), *sec_log_ptr);
	last_kmsg_buffer = (char *)kmalloc(last_kmsg_size,GFP_KERNEL);

	if (last_kmsg_size && last_kmsg_buffer) {
		unsigned i;
		for (i = 0; i < last_kmsg_size; i++)
			last_kmsg_buffer[i] =
			    sec_log_buf[(*sec_log_ptr - last_kmsg_size +
					 i) & (sec_log_size - 1)];

		pr_info("%s: saved old log at %d@%p\n",
			__func__, last_kmsg_size, last_kmsg_buffer);
	} else
		pr_err("%s: failed saving old log %d@%p\n",
		       __func__, last_kmsg_size, last_kmsg_buffer);
}

static ssize_t sec_log_read_old(struct file *file, char __user *buf,
				size_t len, loff_t *offset)
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
