/* *  sec-log64.c
 *
 * Copyright (C) 2015 Samsung Electronics Co, Ltd.
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

#include <asm/tlbflush.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/memblock.h>
#include <linux/proc_fs.h>
#include <soc/sprd/sec_log64.h>

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
static unsigned long *sec_log_ptr;
static char *sec_log_buf;
static unsigned long sec_log_size;

struct sec_log_buffer *sec_log_buffer;

static char *last_kmsg_buffer;
static unsigned last_kmsg_size;
static void __init sec_log_save_old(void);

phys_addr_t sec_logbuf_base_addr;
unsigned long sec_logbuf_size;
struct sched_log *psec_debug_log = NULL;
phys_addr_t phy_sec_sched_log_struct;
size_t phy_sec_sched_log_struct_size;
static int bStopLogging;
extern void register_log_text_hook(void (*f)(char *text, size_t size),
	char *buf, unsigned *position, size_t bufsize);

void *s_memcpy(void *dest, const void *src, size_t count)
{
	char *tmp = dest;
	const char *s = src;

	while (count--)
		*tmp++ = *s++;
	return dest;
}

static inline void emit_sec_log(char *text, size_t size)
{
	if (sec_log_buf && sec_log_ptr) {

		size_t pos = 0;

		pos = *sec_log_ptr;

		if (likely(size + pos <= sec_log_size)) {
			s_memcpy(&sec_log_buf[pos], text, size);
			(*sec_log_ptr) += size;
		}
		else {
			size_t first = sec_log_size - pos;
			size_t second = size - first;
			s_memcpy(&sec_log_buf[pos], text, first);
			s_memcpy(&sec_log_buf[0], text + first, second);
			(*sec_log_ptr) = second;
		}
		sec_log_buffer->start = *sec_log_ptr;			  // Update start pointer for panic display
		sec_log_buffer->size += size;					  // Updated size based on print parameters
		sec_log_buffer->size = sec_log_buffer->size < sec_log_size ? sec_log_buffer->size : sec_log_size; // Boundary checking
	}
}

#if NR_CPUS == 1
static atomic_t task_log_idx[NR_CPUS] = { ATOMIC_INIT(-1) };
static atomic_t irq_log_idx[NR_CPUS] = { ATOMIC_INIT(-1) };
static atomic_t work_log_idx[NR_CPUS] = { ATOMIC_INIT(-1) };
static atomic_t timer_log_idx[NR_CPUS] = { ATOMIC_INIT(-1) };
#elif NR_CPUS == 2
static atomic_t task_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
static atomic_t irq_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
static atomic_t work_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
static atomic_t timer_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
#elif NR_CPUS == 4
static atomic_t task_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1),
					ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
static atomic_t irq_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1),
					ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
static atomic_t work_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1),
					ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
static atomic_t timer_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1),
					ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
#elif NR_CPUS == 8
static atomic_t task_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1),
					ATOMIC_INIT(-1), ATOMIC_INIT(-1),
					ATOMIC_INIT(-1), ATOMIC_INIT(-1),
					ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
static atomic_t irq_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1),
					ATOMIC_INIT(-1), ATOMIC_INIT(-1),
					ATOMIC_INIT(-1), ATOMIC_INIT(-1),
					ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
static atomic_t work_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1),
					ATOMIC_INIT(-1), ATOMIC_INIT(-1),
					ATOMIC_INIT(-1), ATOMIC_INIT(-1),
					ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
static atomic_t timer_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1),
					ATOMIC_INIT(-1), ATOMIC_INIT(-1),
					ATOMIC_INIT(-1), ATOMIC_INIT(-1),
					ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
#else
#error "Please check NR_CPUS"
#endif

#ifdef CONFIG_SEC_DEBUG_REG_ACCESS
struct sec_debug_regs_access *sec_debug_last_regs_access=NULL;
unsigned char *sec_debug_local_hwlocks_status=NULL;
EXPORT_SYMBOL(sec_debug_last_regs_access);
EXPORT_SYMBOL(sec_debug_local_hwlocks_status);

void sec_debug_set_last_regs_access(void)
{
	unsigned addr = (unsigned)sec_log_buf;
	unsigned size_struct_sec_log_buffer = sizeof(struct sec_log_buffer);
	extern struct sec_debug_regs_access *sec_debug_last_regs_access;
	extern char *sec_debug_local_hwlocks_status;

	size_struct_sec_log_buffer = (((size_struct_sec_log_buffer + 4) / 4)* 4);
	addr = addr + sec_log_size + size_struct_sec_log_buffer;
	sec_debug_last_regs_access = addr;
	sec_debug_local_hwlocks_status = sec_debug_last_regs_access + NR_CPUS;
	memset(sec_debug_local_hwlocks_status,0,64);
}
#endif

void sec_debug_panic_message(int en)
{
	if(unlikely(!sec_log_buffer || !sec_log_ptr))
		return;
	if(!en && !sec_log_buffer->from)
		sec_log_buffer->from = *sec_log_ptr;
	else if(!sec_log_buffer->to)
		sec_log_buffer->to = *sec_log_ptr;
}

static void map_noncached_sched_log_buf(void)
{
	void __iomem *base=0;

	base = ioremap_nocache(phy_sec_sched_log_struct ,phy_sec_sched_log_struct_size );
	if(base == NULL) {
		printk("ioremap nocache fail\n");
		return -1;
	}
	psec_debug_log = base;
}

static int __init sec_sched_log_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;

	if (!size || *str != '@' || kstrtoul(str + 1, 0, &base))
		return -1;
	phy_sec_sched_log_struct  = base;
	phy_sec_sched_log_struct_size = size;
	return 0;
}
early_param("sec_sched_log", sec_sched_log_setup);

static int __init sec_log_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;
	unsigned alloc_size = 0;
	unsigned size_of_struct_sec_log_buffer = 0;

	if (!size || *str != '@' || kstrtoul(str + 1, 0, &base))
		return -1;
	sec_logbuf_base_addr = base;
	sec_logbuf_size = size;
	return 0;
}
early_param("sec_log", sec_log_setup);

static int __init sec_logbuf_conf_memtype(void)
{
	void __iomem *base = 0;
	unsigned long *sec_log_mag;
	unsigned long size_of_log_mag = sizeof(*sec_log_mag);
	unsigned long size_of_log_ptr = sizeof(*sec_log_ptr);
	unsigned long size_struct_sec_log_buffer = sizeof(struct sec_log_buffer);

	/* Round-up to 8 byte alignment */
	size_struct_sec_log_buffer = (((size_struct_sec_log_buffer + 8) / 8) * 8);
	pr_info("%s: size_of_log_ptr:%d size_of_log_mag:%d"
		" size_struct_sec_log_buffer:%d\n",
		__func__, size_of_log_ptr, size_of_log_mag,
					size_struct_sec_log_buffer);

	/**********************************************************************
	 ___________________
	|   sec_log_buffer  |   16 bytes,4 byte aligned sec_log_buffer struct
	|___________________|
	|                   |
	|   sec_log_buf     |	1 MB : 960KB sec log buf
	|                   |   2 MB : 1.5MB sec log buf
	|___________________|
	|   sec_log_ptr	    |	4 bytes, which holds the index of sec_log_buf[]
	|___________________|
	|   sec_log_mag     |	4 bytes, which holds the magic address
	|___________________|

	***********************************************************************/

	base = ioremap_nocache(sec_logbuf_base_addr, sec_logbuf_size) ;
	if(base == NULL) {
		printk("ioremap nocache fail\n");
		return -1;
	}

	map_noncached_sched_log_buf();

	if( sec_logbuf_size == 0x200000 )
		sec_log_size = 0x180000;
	else if( sec_logbuf_size == 0x100000 )
		sec_log_size = 0xF0000;
	else {
		printk("sec_log size input fail : use 1MB or 2MB\n");
		return -1;
	}

	sec_log_buffer = base;
	sec_log_buf = (char *)sec_log_buffer + size_struct_sec_log_buffer;
	sec_log_ptr = sec_log_buf + sec_log_size;
	sec_log_mag = (char *)sec_log_ptr + size_of_log_ptr;

	pr_info("%s addr : struct sec_log_buffer:%p sec_log_buf:%p sec_log_ptr:%p sec_log_mag:%p\n",
		__func__, sec_log_buffer, sec_log_buf, sec_log_ptr, sec_log_mag);

	pr_info("%s: *sec_log_mag:%x *sec_log_ptr:%x sec_log_buf:%p sec_log_size:%d\n",
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
			pr_err("%s: *sec_log_ptr is zero, skip sec_old_log_save\n", __func__);
	}
#ifdef CONFIG_SEC_DEBUG_REG_ACCESS
	sec_debug_set_last_regs_access();
#endif
	sec_log_buffer->sig = RAM_CONSOLE_SIG;
	sec_log_buffer->size = 0;
	sec_log_buffer->start = *sec_log_ptr;
	sec_log_buffer->from = 0;
	sec_log_buffer->to = 0;

	register_log_text_hook(emit_sec_log, sec_log_buf, sec_log_ptr, sec_log_size);

	sec_getlog_supply_kloginfo(sec_log_buf);

	return 0;
}
module_init(sec_logbuf_conf_memtype);

void __sec_debug_task_log(int cpu, struct task_struct *task)
{
	unsigned i;

	if (bStopLogging)
		return;

	i = atomic_inc_return(&task_log_idx[cpu]) & (SCHED_LOG_MAX - 1);
	psec_debug_log->task[cpu][i].time = cpu_clock(cpu);
	strcpy(psec_debug_log->task[cpu][i].comm, task->comm);
	psec_debug_log->task[cpu][i].pid = task->pid;
}

void __sec_debug_irq_log(unsigned int irq, void *fn, int en)
{
	int cpu = raw_smp_processor_id();
	unsigned i;

	if (bStopLogging)
		return;

	i = atomic_inc_return(&irq_log_idx[cpu]) & (SCHED_LOG_MAX - 1);
	psec_debug_log->irq[cpu][i].time = cpu_clock(cpu);
	psec_debug_log->irq[cpu][i].irq = irq;
	psec_debug_log->irq[cpu][i].fn = (void *)fn;
	psec_debug_log->irq[cpu][i].en = en;
}

void __sec_debug_work_log(struct worker *worker,
			  struct work_struct *work, work_func_t f, int en)
{
	int cpu = raw_smp_processor_id();
	unsigned i;

	if (bStopLogging)
		return;

	i = atomic_inc_return(&work_log_idx[cpu]) & (SCHED_LOG_MAX - 1);
	psec_debug_log->work[cpu][i].time = cpu_clock(cpu);
	psec_debug_log->work[cpu][i].worker = worker;
	psec_debug_log->work[cpu][i].work = work;
	psec_debug_log->work[cpu][i].f = f;
	psec_debug_log->work[cpu][i].en = en;
}

void __sec_debug_timer_log(unsigned int type, void *fn)
{
	int cpu = raw_smp_processor_id();
	unsigned i;

	if (bStopLogging)
		return;

	i = atomic_inc_return(&timer_log_idx[cpu]) & (SCHED_LOG_MAX - 1);
	psec_debug_log->timer[cpu][i].time = cpu_clock(cpu);
	psec_debug_log->timer[cpu][i].type = type;
	psec_debug_log->timer[cpu][i].fn = fn;
}

/* ++ last_kmsg */
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

static int __init sec_last_kmsg_late_init(void)
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

	proc_set_size(entry, last_kmsg_size);
	return 0;
}

late_initcall(sec_last_kmsg_late_init);
