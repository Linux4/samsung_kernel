/* linux/arch/arm/mach-sc8810/sprd_mem_pool.c
 *
 * Copyright (C) 2010 Spreadtrum
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/mutex.h>
#include <asm/current.h>
#include <asm/uaccess.h>
#include <linux/pagemap.h>
#include <linux/pfn.h>
#include <linux/memory.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/sched.h>

/**
 * add another order pool, plz modify:
 * 1.PAGE_POOL_MAX_COUNT: +1
 * 2.page_pool struct: +node
 * */

/*Magic Numbers*/
#define DEBUG_PRINT	1	/*used for debug*/
#define UNSIGNED_LONG_MAX	0xffffffff
#define PAGE_POOL_MAX_COUNT	5

/*status record struct declare*/
struct status {
	unsigned long used;
	unsigned long free;
	unsigned long peak;
	unsigned long total;
};

/*page pool struct definition*/
static struct {
	unsigned long order;
	unsigned long threshold;
	unsigned long node_count;
	unsigned long *buffer;
	struct mutex lock;
	struct status stat;
	struct workqueue_struct *sprd_alloc_wq;
	struct work_struct sprd_alloc_work;
} page_pool[PAGE_POOL_MAX_COUNT] = {
	{
		.order = 0,
		.buffer = NULL,
		.sprd_alloc_wq = NULL,
	}, {
		.order = 1,
		.threshold = 2,
		.node_count = 15,
		.buffer = NULL,
		.lock = __MUTEX_INITIALIZER(page_pool[1].lock),
		.stat.used = 0,
		.stat.free = 0,
		.stat.peak = 0,
		.stat.total = 0,
		.sprd_alloc_wq = NULL,
	}, {
		.order = 2,
		.threshold = 2,
		.node_count = 10,
		.buffer = NULL,
		.lock = __MUTEX_INITIALIZER(page_pool[2].lock),
		.stat.used = 0,
		.stat.free = 0,
		.stat.peak = 0,
		.stat.total = 0,
		.sprd_alloc_wq = NULL,
	}, {
		.order = 3,
		.threshold = 1,
		.node_count = 3,
		.buffer = NULL,
		.lock = __MUTEX_INITIALIZER(page_pool[3].lock),
		.stat.used = 0,
		.stat.free = 0,
		.stat.peak = 0,
		.stat.total = 0,
		.sprd_alloc_wq = NULL,
	}, {
		.order = 4,
		.threshold = 1,
		.node_count = 2,
		.buffer = NULL,
		.lock = __MUTEX_INITIALIZER(page_pool[4].lock),
		.stat.used = 0,
		.stat.free = 0,
		.stat.peak = 0,
		.stat.total = 0,
		.sprd_alloc_wq = NULL,
	}
};

/*global variables*/
static unsigned long sys_buddy_info[MAX_ORDER];
static unsigned long alloc_flag = 1;
static int sprd_alloc_workqueue_pid = -1;

/*extern functions*/
extern void msleep(unsigned int msecs);
extern void dump_stack(void);
struct mutex sprd_mem_stat_lock;
static int sprd_mem_pool_stat = 0;
static unsigned long long sprd_mem_alloc_count = 0;

/*internal functions*/
static unsigned long pow2(unsigned long x, unsigned long y)
{
	unsigned long result = 0;

	/*x == 0*/
	if(!x) return 0;

	/*y == 0*/
	if(!y) return 1;

	/*x > 0, y > 0*/
	result = x * pow2(x, (y - 1));

	if(result > UNSIGNED_LONG_MAX) {
		result = UNSIGNED_LONG_MAX;
		printk("__SPRD__INIT__: call pow2(%lu, %lu) error!!!\n", x, y);
	}

	return result;
}

static void dumpstack(void)
{
	printk("Process Name: %s, Process Pid: %d, Parent Name: %s, Parent Pid: %d\n", 
			current->comm, current->pid, current->parent->comm, current->parent->pid);
	dump_stack();
}

static struct page *address_to_pages(unsigned long address)
{
	if(!address) return NULL;

#if defined(WANT_PAGE_VIRTUAL)
	return container_of((void *)(address), struct page, virtual);
#else
	return virt_to_page(address);
#endif
}

static void get_buddy_info(void)
{
	struct zone *zone;
	unsigned int order = 0;

	/*normal zone only*/
	for_each_zone(zone) {
		if(is_normal(zone)) break;
	}

	/*global buddyinfo*/
	for(order = 0; order < MAX_ORDER; order++) {
		sys_buddy_info[order] = zone->free_area[order].nr_free;
	}
}

static unsigned long get_page_count(unsigned int entry_order)
{
	struct zone *zone;
	unsigned int order = 0;
	unsigned long page_count = 0;

	/*normal zone only*/
	for_each_zone(zone) {
		if(is_normal(zone)) break;
	}

	/*forall buddy pages*/
	for(order = 0; order < MAX_ORDER; order++) {
		/*get page counts*/
		if(order < entry_order) continue;

		page_count += (zone->free_area[order].nr_free << (order - entry_order));
	}

	return page_count;
}

static unsigned long sprd_alloc_one(unsigned long order)
{
	unsigned long sub = 0;
	unsigned long address = 0;

	/*lock*/
	mutex_lock(&(page_pool[order].lock));

	/*for order page pool*/
	for(sub = 0; sub < page_pool[order].node_count; sub++) {

		/*jump over null page*/
		if(!page_pool[order].buffer[sub]) continue;

		/*get first !null page*/
		address = page_pool[order].buffer[sub];
		page_pool[order].buffer[sub] = 0;

		/*status record*/
		page_pool[order].stat.free--;
		page_pool[order].stat.used++;

		mutex_lock(&sprd_mem_stat_lock);
		sprd_mem_alloc_count++;
		sprd_mem_pool_stat = 1;
		mutex_unlock(&sprd_mem_stat_lock);

		if(strncmp(current->comm, "sprd-page-alloc", 13)) {
			if(page_pool[order].stat.peak < page_pool[order].stat.used)
				page_pool[order].stat.peak = page_pool[order].stat.used;
			if(page_pool[order].stat.total < UNSIGNED_LONG_MAX)
				page_pool[order].stat.total++;
		}
	goto end;
	}

end:
	/*unlock*/
	mutex_unlock(&(page_pool[order].lock));

#if DEBUG_PRINT
	printk("__SPRD__ALLOC__ONE__: buffer for order = %lu empty; free = %lu, used = %lu, sys = %lu; peak = %lu, total = %lu;  Call Stack:\n",
		order, page_pool[order].stat.free, page_pool[order].stat.used,
		get_page_count(order), page_pool[order].stat.peak, page_pool[order].stat.total);
	dumpstack();
#endif
	return address;
}

int sprd_page_mask_check(int pid)
{
	if(pid == sprd_alloc_workqueue_pid) return -1;

	return 0;
}

struct page *sprd_page_alloc(gfp_t gfp_mask, unsigned int order, unsigned long zoneidx)
{
	unsigned long address = 0;
	struct page *page = NULL;

	/*check some flags*/
	if((!alloc_flag) || (GFP_KERNEL != gfp_mask) || (ZONE_NORMAL != zoneidx) || (order > PAGE_POOL_MAX_COUNT - 1) || (order < 1))
		goto Failed;

	/*alloc*/
	address = sprd_alloc_one((unsigned long)order);
	if(!address) goto Failed;

	/*convert address to page*/
	page = address_to_pages(address);

	return page;

Failed:
	return NULL;
}

static int sprd_show_pages_info(struct seq_file *m, void *v)
{
	unsigned long i = 0;
	unsigned long start = 0;

	seq_printf(m,
		"sprd pool summery:\n"
		"    status: %s\n",
		(alloc_flag ? "open" : "closed"));
	for(start = 1; start < PAGE_POOL_MAX_COUNT; start++) {
		seq_printf(m,
			"    free node (order=%lu) = %lu, used node (order=%lu) = %lu\n",
			start, page_pool[start].stat.free, start, page_pool[start].stat.used);
	}
	for(start = 1; start < PAGE_POOL_MAX_COUNT; start++) {
		seq_printf(m,
			"    system memory count (order=%lu) = %lu\n",
			start, get_page_count(start));
	}
	for(start = 1; start < PAGE_POOL_MAX_COUNT; start++) {
		seq_printf(m,
			"    alloc peak (order=%lu) = %lu, alloc total (order=%lu) = %lu\n",
			start, page_pool[start].stat.peak, start, page_pool[start].stat.total);
	}

	seq_printf(m, "buddy info:\n");
	get_buddy_info();
	for(i = 0; i < MAX_ORDER; i++) {
		seq_printf(m, "    orders = %lu, number = %lu\n", i, sys_buddy_info[i]);
	}

	for(start = 1; start < PAGE_POOL_MAX_COUNT; start++) {
		seq_printf(m, "pool node (order=%lu) details:\n", start);
		for(i = 0; i < page_pool[start].node_count; i++) {
			struct page *page = NULL;
			page = address_to_pages(page_pool[start].buffer[i]);
			seq_printf(m, "    subscript = %lu, value = %p, page = %p, page->flags %lx\n",
					i, (void *)(page_pool[start].buffer[i]), page, (page ? page->flags : 0x00));
		}
	}

	seq_printf(m, "sprd_mem_alloc_count %d\n", sprd_mem_alloc_count);
	seq_printf(m, "sprd_mem_pool_stat %d\n", sprd_mem_pool_stat);
	sprd_alloc_one(3);
	return 0;
}

static int sprd_pages_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_show_pages_info, NULL);
}

static int sprd_pages_info_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	char flag;

	if (copy_from_user(&flag, buf, 1)) return -EFAULT;
	
	switch (flag) {
		case '0': alloc_flag = 0; break;
		case '1': alloc_flag = 1; break;
		default: break;
	}

	return 1;
}

static const struct file_operations sprd_page_info_fops = {
	.open		= sprd_pages_info_open,
	.read		= seq_read,
	.write		= sprd_pages_info_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int sprd_mem_pool_daemon(void *arg)
{
	while (1) {
		int start = 0;

		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(msecs_to_jiffies(1000));
		set_current_state(TASK_RUNNING);

		if (!sprd_mem_pool_stat)
			continue;

		mutex_lock(&sprd_mem_stat_lock);
		sprd_mem_pool_stat = 0;
		mutex_unlock(&sprd_mem_stat_lock);

		for(start = PAGE_POOL_MAX_COUNT - 1; start > 0; start--) {
			unsigned long sub = 0;

			if (page_pool[start].node_count == page_pool[start].stat.free)
				continue;
			else {
				mutex_lock(&sprd_mem_stat_lock);
				if (!sprd_mem_pool_stat)
					sprd_mem_pool_stat = 1;
				mutex_unlock(&sprd_mem_stat_lock);
			}

	                /*for single page buffer*/
			for(sub = 0; sub < page_pool[start].node_count; sub++) {
				mutex_lock(&(page_pool[start].lock));
				gfp_t gfp_mask = GFP_KERNEL;

				/*jump !null node*/
				if(page_pool[start].buffer[sub]) {
					mutex_unlock(&(page_pool[start].lock));
					continue;
				}

				/*check if sys pages enough*/
				if(page_pool[start].threshold > get_page_count(page_pool[start].order)) {
					mutex_unlock(&(page_pool[start].lock));
					break;
				}

				/*alloc page from sys for null node*/
				page_pool[start].buffer[sub] = __get_free_pages(gfp_mask, page_pool[start].order);
				if(!page_pool[start].buffer[sub]) {
					mutex_unlock(&(page_pool[start].lock));
					break;
				}

	                        /*stats record*/
				page_pool[start].stat.free++;
				page_pool[start].stat.used--;
				mutex_unlock(&(page_pool[start].lock));
			}
		}

		mutex_lock(&sprd_mem_stat_lock);
		if(!sprd_mem_pool_stat)
			sprd_mem_alloc_count = 0;
		mutex_unlock(&sprd_mem_stat_lock);
	}
}

static int __init sprd_pages_init(void)
{
	int start = 0, mem_sum = 0;
	gfp_t gfp_mask = GFP_KERNEL;
	struct workqueue_struct *sprd_alloc_wq = NULL;
	struct task_struct *p = NULL;

	/*init global*/
	memset(sys_buddy_info, 0, sizeof(sys_buddy_info));
	mutex_init(&sprd_mem_stat_lock);

	/*init page pool*/
	for(start = 1; start < PAGE_POOL_MAX_COUNT; start++) {

		/*node buffer*/
		unsigned long sub = 0;
		page_pool[start].buffer = kmalloc((page_pool[start].node_count * sizeof(unsigned long)), gfp_mask);
		memset(page_pool[start].buffer, 0, (page_pool[start].node_count * sizeof(unsigned long)));

		for(sub = 0; sub < page_pool[start].node_count; sub++) {
			/*alloc page from sys*/
			page_pool[start].buffer[sub] = __get_free_pages(gfp_mask, page_pool[start].order);
			/*stats record*/
			page_pool[start].buffer[sub] ? page_pool[start].stat.free++ : page_pool[start].stat.used++;
		}

		/*alloc total size*/
		mem_sum += page_pool[start].node_count * 4 * pow2(2, page_pool[start].order);

#if DEBUG_PRINT
		printk("__SPRD__INIT__: page_pool[%d].node_count = %lu, page_pool[%d].order = %lu, mem_sum = %d KiB\n",
			start, page_pool[start].node_count, start, page_pool[start].order, mem_sum);
#endif
	}
	p = kthread_create(sprd_mem_pool_daemon, NULL, "%s", "sprdmempool");
	wake_up_process(p);
	sprd_alloc_workqueue_pid = p->pid;

	printk("__SPRD__INIT__: Sprd memory pool initialized, %d KiB memory allocated.\n", mem_sum);

	/*create proc file*/
	proc_create("sprd_pages", 0, NULL, &sprd_page_info_fops);
	return 0;
}

module_init(sprd_pages_init);

