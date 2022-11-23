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

/**
 * add another order pool, plz modify:
 * 1.PAGE_POOL_MAX_COUNT: +1
 * 2.page_pool struct: +node
 * */

/*Magic Numbers*/
#define DEBUG_PRINT	0	/*used for debug*/
#define STATUS_PRINT	1	/*used for status*/
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
#if STATUS_PRINT
	struct status stat;
#endif
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
#if STATUS_PRINT
		.stat.used = 0,
		.stat.free = 0,
		.stat.peak = 0,
		.stat.total = 0,
#endif
		.sprd_alloc_wq = NULL,
	}, {
		.order = 2,
		.threshold = 2,
		.node_count = 10,
		.buffer = NULL,
		.lock = __MUTEX_INITIALIZER(page_pool[2].lock),
#if STATUS_PRINT
		.stat.used = 0,
		.stat.free = 0,
		.stat.peak = 0,
		.stat.total = 0,
#endif
		.sprd_alloc_wq = NULL,
	}, {
		.order = 3,
		.threshold = 1,
		.node_count = 3,
		.buffer = NULL,
		.lock = __MUTEX_INITIALIZER(page_pool[3].lock),
#if STATUS_PRINT
		.stat.used = 0,
		.stat.free = 0,
		.stat.peak = 0,
		.stat.total = 0,
#endif
		.sprd_alloc_wq = NULL,
	}, {
		.order = 4,
		.threshold = 1,
		.node_count = 2,
		.buffer = NULL,
		.lock = __MUTEX_INITIALIZER(page_pool[4].lock),
#if STATUS_PRINT
		.stat.used = 0,
		.stat.free = 0,
		.stat.peak = 0,
		.stat.total = 0,
#endif
		.sprd_alloc_wq = NULL,
	}
};

/*global variables*/
static unsigned long alloc_flag = 1;
static unsigned long sys_buddy_info[MAX_ORDER];

/*extern functions*/
extern void msleep(unsigned int msecs);
extern void dump_stack(void);

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

static void sprd_alloc_delay(struct work_struct *work)
{
	int start = 0;

	printk("__SPRD__ALLOC__DELAY__: work active!!!\n"); 

	/*forall page pool*/
	for(start = PAGE_POOL_MAX_COUNT - 1; start > 0; start--) {
		unsigned long sub = 0;
		unsigned long queueworkflag = 0;

		/*for single page buffer*/
		for(sub = 0; sub < page_pool[start].node_count; sub++) {
			gfp_t gfp_mask = GFP_KERNEL;

			/*jump !null node*/
			if(page_pool[start].buffer[sub]) continue;

			/*check if sys pages enough*/
			if(page_pool[start].threshold > get_page_count(page_pool[start].order)) {
				queueworkflag = 1;
				break;
			}

			/*alloc page from sys for null node*/
			page_pool[start].buffer[sub] = __get_free_pages(gfp_mask, page_pool[start].order);
			if(!page_pool[start].buffer[sub]) {
				queueworkflag = 1;
				break;
			}

#if STATUS_PRINT
	#if DEBUG_PRINT
			printk("__SPRD__ALLOC__DELAY__: fill buffer 1 for order = %d; free = %lu, used = %lu, sys =%lu; peak = %lu, total = %lu;  Call Stack:\n",
				start, page_pool[start].stat.free, page_pool[start].stat.used, get_page_count(start), page_pool[start].stat.peak, page_pool[start].stat.total);
			dumpstack();
	#endif

			/*stats record*/
			mutex_lock(&(page_pool[start].lock));
			page_pool[start].stat.free++;
			page_pool[start].stat.used--;
			mutex_unlock(&(page_pool[start].lock));

	#if DEBUG_PRINT
			printk("__SPRD__ALLOC__DELAY__: fill buffer 2 for order = %d; free = %lu, used = %lu, sys =%lu; peak = %lu, total = %lu;  Call Stack:\n",
				start, page_pool[start].stat.free, page_pool[start].stat.used, get_page_count(start), page_pool[start].stat.peak, page_pool[start].stat.total);
			dumpstack();
	#endif
#endif

			printk("__SPRD__ALLOC__DELAY__: work done!!!\n"); 

			/*fill first null node ok*/
			break;
		}

		/*check if still exist null node*/
		if(!queueworkflag) {
			for(sub = 0; sub < page_pool[start].node_count; sub++) {
				/*jump !null node*/
				if(page_pool[start].buffer[sub]) continue;
				/*still exist null node*/
				queueworkflag = 1;
				break;
			}
		}

		/*queue work*/
		if(queueworkflag) {
			msleep(1000);
			queue_work(page_pool[start].sprd_alloc_wq, &(page_pool[start].sprd_alloc_work));
#if STATUS_PRINT
	#if DEBUG_PRINT
			printk("__SPRD__ALLOC__DELAY__: queuework again: buffer for order = %d; free = %lu, used = %lu, sys = %lu; peak = %lu, total = %lu;  Call Stack:\n",
				start, page_pool[start].stat.free, page_pool[start].stat.used, get_page_count(start), page_pool[start].stat.peak, page_pool[start].stat.total);
			dumpstack();
	#endif
#endif
			printk("__SPRD__ALLOC__DELAY__: work delay!!!\n"); 
		}
	}

	return;
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

#if STATUS_PRINT
	#if DEBUG_PRINT
		printk("__SPRD__ALLOC__ONE__: allocated from buffer 1, order = %lu; free = %lu, used = %lu, sys = %lu; peak = %lu, total = %lu;  call stack:\n",
			order, page_pool[order].stat.free, page_pool[order].stat.used, get_page_count(order), page_pool[order].stat.peak, page_pool[order].stat.total);
	#endif
		/*status record*/
		page_pool[order].stat.free--;
		page_pool[order].stat.used++;
	#if DEBUG_PRINT
		printk("__SPRD__ALLOC__ONE__: allocated from buffer 2, order = %lu; free = %lu, used = %lu, sys = %lu; peak = %lu, total = %lu;  call stack:\n",
			order, page_pool[order].stat.free, page_pool[order].stat.used, get_page_count(order), page_pool[order].stat.peak, page_pool[order].stat.total);
	#endif
		if(strncmp(current->comm, "sprd-page-alloc", 13)) {
			if(page_pool[order].stat.peak < page_pool[order].stat.used)
				page_pool[order].stat.peak = page_pool[order].stat.used;
			if(page_pool[order].stat.total < UNSIGNED_LONG_MAX)
				page_pool[order].stat.total++;
		}
#endif

		/*unlock*/
		mutex_unlock(&(page_pool[order].lock));

		/*queue work*/
		goto queuework;
	}

	/*unlock*/
	mutex_unlock(&(page_pool[order].lock));

#if STATUS_PRINT
	printk("__SPRD__ALLOC__ONE__: buffer for order = %lu empty; free = %lu, used = %lu, sys = %lu; peak = %lu, total = %lu;  Call Stack:\n",
			order, page_pool[order].stat.free, page_pool[order].stat.used, get_page_count(order), page_pool[order].stat.peak, page_pool[order].stat.total);
	dumpstack();
#endif

queuework:
	/*feather-weight : reduce "self-work aquire self-pool" counts*/
	if(strncmp(current->comm, "sprd-page-alloc", 13)) {
		queue_work(page_pool[order].sprd_alloc_wq, &(page_pool[order].sprd_alloc_work));

#if STATUS_PRINT
	#if DEBUG_PRINT
		printk("__SPRD__ALLOC__ONE__: queue work, order = %lu; free = %lu, used = %lu, sys = %lu; peak = %lu, total = %lu;  Call stack:\n",
			order, page_pool[order].stat.free, page_pool[order].stat.used, get_page_count(order), page_pool[order].stat.peak, page_pool[order].stat.total);
		dumpstack();
	#endif
#endif
	}

	return address;
}

struct page *sprd_page_alloc(gfp_t gfp_mask, unsigned int order, unsigned long zoneidx)
{
	unsigned long address = 0;
	struct page *page = NULL;

#if STATUS_PRINT
	printk("__SPRD__ALLOC__: gfp_mask = 0x%lx, alloc_flag = %lu, zoneidx = %lu, order = %u, free = %lu, used = %lu, peak = %lu, total = %lu\n",
			(unsigned long)gfp_mask, alloc_flag, zoneidx, order, page_pool[order].stat.free, page_pool[order].stat.used, page_pool[order].stat.peak, page_pool[order].stat.total);
#endif

	/*check some flags*/
	if((!alloc_flag) || (GFP_KERNEL != gfp_mask) || (ZONE_NORMAL != zoneidx) || (order > PAGE_POOL_MAX_COUNT - 1) || (order < 1))
		goto Failed;

	/*alloc*/
	address = sprd_alloc_one((unsigned long)order);
	if(!address) goto Failed;

	/*convert address to page*/
	page = address_to_pages(address);

#if STATUS_PRINT
	printk("__SPRD__ALLOC__: Process Name: %s, Process Pid: %d, Parent Name: %s, Parent Pid: %d, address: %p, page: %p, free: %lu, used: %lu, peak: %lu, total: %lu\n",
			current->comm, current->pid, current->parent->comm, current->parent->pid, (void *)address, page,
			page_pool[order].stat.free, page_pool[order].stat.used, page_pool[order].stat.peak, page_pool[order].stat.total);
#endif

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
#if STATUS_PRINT
	for(start = 1; start < PAGE_POOL_MAX_COUNT; start++) {
		seq_printf(m,
			"    free node (order=%lu) = %lu, used node (order=%lu) = %lu\n",
			start, page_pool[start].stat.free, start, page_pool[start].stat.used);
	}
#endif
	for(start = 1; start < PAGE_POOL_MAX_COUNT; start++) {
		seq_printf(m,
			"    system memory count (order=%lu) = %lu\n",
			start, get_page_count(start));
	}
#if STATUS_PRINT
	for(start = 1; start < PAGE_POOL_MAX_COUNT; start++) {
		seq_printf(m,
			"    alloc peak (order=%lu) = %lu, alloc total (order=%lu) = %lu\n",
			start, page_pool[start].stat.peak, start, page_pool[start].stat.total);
	}
#endif

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


static int __init sprd_pages_init(void)
{
	int start = 0, mem_sum = 0;
	gfp_t gfp_mask = GFP_KERNEL;
	struct workqueue_struct *sprd_alloc_wq = NULL;

	/*init global*/
	memset(sys_buddy_info, 0, sizeof(sys_buddy_info));

	/*init page pool*/
	for(start = 1; start < PAGE_POOL_MAX_COUNT; start++) {

		/*node buffer*/
		unsigned long sub = 0;
		page_pool[start].buffer = kmalloc((page_pool[start].node_count * sizeof(unsigned long)), gfp_mask);
		memset(page_pool[start].buffer, 0, (page_pool[start].node_count * sizeof(unsigned long)));

		for(sub = 0; sub < page_pool[start].node_count; sub++) {
			/*alloc page from sys*/
			page_pool[start].buffer[sub] = __get_free_pages(gfp_mask, page_pool[start].order);
#if STATUS_PRINT
			/*stats record*/
			page_pool[start].buffer[sub] ? page_pool[start].stat.free++ : page_pool[start].stat.used++;
#endif
		}

		/*alloc total size*/
		mem_sum += page_pool[start].node_count * 4 * pow2(2, page_pool[start].order);

#if DEBUG_PRINT
		printk("__SPRD__INIT__: page_pool[%d].node_count = %lu, page_pool[%d].order = %lu, mem_sum = %d KiB\n",
			start, page_pool[start].node_count, start, page_pool[start].order, mem_sum);
#endif

		/*work queue*/
		if(!sprd_alloc_wq)
			sprd_alloc_wq = create_singlethread_workqueue("sprd-page-alloc");
		page_pool[start].sprd_alloc_wq = sprd_alloc_wq;

		/*work*/
		INIT_WORK(&(page_pool[start].sprd_alloc_work), sprd_alloc_delay);
	}

	printk("__SPRD__INIT__: Sprd memory pool initialized, %d KiB memory allocated.\n", mem_sum);

	/*create proc file*/
	proc_create("sprd_pages", 0, NULL, &sprd_page_info_fops);
	return 0;
}

module_init(sprd_pages_init);

