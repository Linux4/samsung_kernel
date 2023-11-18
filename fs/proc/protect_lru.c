/*
 * Protect lru of task support.It's between normal lru and mlock,
 * that means we will reclaim protect lru pages as late as possible.
 *
 * Copyright (C) 2001-2021, Huawei Tech.Co., Ltd. All rights reserved.
 *
 * Authors:
 * Shaobo Feng <fengshaobo@huawei.com>
 * Xishi Qiu <qiuxishi@huawei.com>
 * Yisheng Xie <xieyisheng1@huawei.com>
 * jing Xia <jing.xia@unisoc.com>
 *
 * This program is free software; you can redistibute it and/or modify
 * it under the t erms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_inline.h>
#include <linux/memcontrol.h>
#include <linux/sched/mm.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/protect_lru.h>
#include <linux/swap.h>
#include <linux/module.h>
#include "internal.h"

#include <trace/events/plru.h>


static unsigned long zero;
static unsigned long one = 1;
static unsigned long one_hundred = 100;
static unsigned long fifty = 50;

static unsigned long protect_max_mbytes[PROTECT_HEAD_END] = {
	100,
	100,
	100,
};

static unsigned long protect_cur_mbytes[PROTECT_HEAD_END];

unsigned long protect_reclaim_ratio = 50;
unsigned long protect_lru_enable __read_mostly = 1;

static ssize_t protect_level_write(struct file *file, const char __user *buf,
				   size_t count, loff_t *ppos)
{
	char buffer[200];
	struct task_struct *task;
	struct mm_struct *mm;
	int protect_level;
	int err;

	memset(buffer, 0, sizeof(buffer));

	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	err = kstrtoint(strstrip(buffer), 0, &protect_level);
	if (err)
		return -EINVAL;

	if (protect_level > PROTECT_HEAD_END || protect_level < 0)
		return -EINVAL;

	task = get_proc_task(file->f_path.dentry->d_inode);
	if (!task)
		return -ESRCH;

	mm = get_task_mm(task);
	if (!mm)
		goto out;

	down_write(&mm->mmap_sem);
	mm->protect = protect_level;
	up_write(&mm->mmap_sem);
	mmput(mm);

out:
	put_task_struct(task);
	return count;
}

static ssize_t protect_level_read(struct file *file, char __user *buf,
				  size_t count,	loff_t *ppos)
{
	struct task_struct *task;
	struct mm_struct *mm;
	char buffer[PROC_NUMBUF];
	int protect_level;
	size_t len;

	task = get_proc_task(file->f_path.dentry->d_inode);
	if (!task)
		return -ESRCH;

	mm = get_task_mm(task);
	if (!mm) {
		put_task_struct(task);
		return -ENOENT;
	}

	protect_level = mm->protect;
	mmput(mm);
	put_task_struct(task);

	len = snprintf(buffer, sizeof(buffer), "%d\n", protect_level);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static inline bool check_file_page(struct page *page)
{
	if (PageSwapBacked(page))
		return false;

	if (PageUnevictable(page))
		return false;

	return true;
}

static inline bool check_protect_page(struct page *page)
{
	if (!PageLRU(page))
		return false;

	return check_file_page(page);
}

void protect_lru_set_from_process(struct page *page)
{
	struct mm_struct *mm = current->mm;
	int level;

	if (!mm || !protect_lru_enable)
		return;

	level = mm->protect;

	if (level > 0) {
		struct lruvec *lruvec;

		lruvec = mem_cgroup_page_lruvec(page, page_pgdat(page));

		if (!lruvec->protect)
			return;

		if (check_file_page(page) && !PageProtect(page) &&
		    lruvec->heads[level-1].max_pages) {
			SetPageActive(page);
			SetPageProtect(page);
			set_page_protect_num(page, level);
		}
	}
}

void del_page_from_protect_lru_list(struct page *page, struct lruvec *lruvec)
{
	int num, nr_pages;
	enum lru_list lru;

	if (!lruvec->protect)
		return;

	if (!check_protect_page(page))
		return;

	num = get_page_protect_num(page) - 1;
	lru = page_lru(page);

	if (unlikely(PageProtect(page))) {
		nr_pages = hpage_nr_pages(page);
		lruvec->heads[num].cur_pages -= nr_pages;
		__mod_node_page_state(lruvec_pgdat(lruvec),
				      NR_PROTECT_LRU_BASE + lru, -nr_pages);
		trace_plru_page_del(page, nr_pages, num + 1, lru);
	}
}

void add_page_to_protect_lru_list(struct page *page, struct lruvec *lruvec,
				  bool lru_head)
{
	int num, nr_pages;
	enum lru_list lru;
	struct list_head *head;

	if (!lruvec->protect)
		return;

	if (!check_protect_page(page))
		return;

	lru = page_lru(page);
	num = get_page_protect_num(page) - 1;

	if (PageProtect(page)) {
		nr_pages = hpage_nr_pages(page);
		head = &lruvec->heads[num].protect_page[lru].lru;
		lruvec->heads[num].cur_pages += nr_pages;
		__mod_node_page_state(lruvec_pgdat(lruvec),
				      NR_PROTECT_LRU_BASE + lru, nr_pages);
		trace_plru_page_add(page, nr_pages, num + 1, lru);
	} else
		head = &lruvec->heads[PROTECT_HEAD_END].protect_page[lru].lru;

	if (lru_head)
		list_move(&page->lru, head);
	else if (PageProtect(page)) {
		head = &lruvec->heads[num+1].protect_page[lru].lru;
		list_move_tail(&page->lru, head);
	} else {
		head = &lruvec->lists[lru];
		list_move_tail(&page->lru, head);
	}
}

bool protect_lru_is_full(struct lruvec *lruvec)
{
	int i;
	unsigned long cur, max;

	for (i = 0; i < PROTECT_HEAD_END; i++) {
		cur = lruvec->heads[i].cur_pages;
		max = lruvec->heads[i].max_pages;

		if (cur && cur > max)
			return true;
	}
	return false;
}

void shrink_protect_lru(struct lruvec *lruvec, bool force)
{
	int i, count, lru, index = 0;
	unsigned long cur, max;
	struct list_head *head;
	struct page *tail;

	if (!lruvec || !lruvec->protect)
		return;

	for (i = 0; i < PROTECT_HEAD_END; i++) {
		cur = lruvec->heads[i].cur_pages;
		max = lruvec->heads[i].max_pages;

		if (!cur)
			continue;

		if (!force && cur <= max)
			continue;

		index = i + 1;
		lru = LRU_INACTIVE_FILE;

		head = &lruvec->heads[index].protect_page[lru].lru;

		tail = list_entry(head->prev, struct page, lru);

		if (PageReserved(tail)) {
			lru = LRU_ACTIVE_FILE;
			head = &lruvec->heads[index].protect_page[lru].lru;
		}

		count = SWAP_CLUSTER_MAX;
		while (count--) {
			tail = list_entry(head->prev, struct page, lru);

			if (PageReserved(tail))
				break;

			del_page_from_protect_lru_list(tail, lruvec);
			trace_plru_page_unprotect(tail,
						  get_page_protect_num(tail),
						  lru);
			ClearPageProtect(tail);
			set_page_protect_num(tail, 0);
			add_page_to_protect_lru_list(tail, lruvec, true);
		}
	}
}

struct page *protect_lru_move_and_shrink(struct page *page)
{
	struct lruvec *lruvec;
	struct pglist_data *pgdat;
	unsigned long flags;

	if (!page || !protect_lru_enable)
		return page;

	pgdat = page_pgdat(page);
	lruvec = mem_cgroup_page_lruvec(page, pgdat);

	if (protect_lru_is_full(lruvec)) {
		spin_lock_irqsave(&(pgdat->lru_lock), flags);
		shrink_protect_lru(lruvec, false);
		spin_unlock_irqrestore(&(pgdat->lru_lock), flags);
	}

	if (PageProtect(page))
		mark_page_accessed(page);

	return page;
}

static int sysctl_protect_max_mbytes_handler(struct ctl_table *table, int write,
	void __user *buffer, size_t *length, loff_t *ppos)
{
	unsigned long total_prot_pages, flags;
	struct pglist_data *pgdat;
	struct lruvec *lruvec;
	int i, ret;

	ret = proc_doulongvec_minmax(table, write, buffer, length, ppos);
	if (ret)
		return ret;

	if (write) {
		for (i = 0; i < PROTECT_HEAD_END; i++) {
			total_prot_pages = protect_max_mbytes[i] << (20 - PAGE_SHIFT);

			for_each_online_pgdat(pgdat) {
				lruvec = get_protect_lruvec(pgdat);

				if (lruvec->protect)
					lruvec->heads[i].max_pages = total_prot_pages;
			}
		}

		for_each_online_pgdat(pgdat) {
			lruvec = get_protect_lruvec(pgdat);

			while (protect_lru_is_full(lruvec)) {
				spin_lock_irqsave(&(pgdat->lru_lock), flags);
				shrink_protect_lru(lruvec, false);
				spin_unlock_irqrestore(&(pgdat->lru_lock), flags);

				if (signal_pending(current))
					return ret;

				cond_resched();
			}
		}
	}

	return ret;
}

static int sysctl_protect_cur_mbytes_handler(struct ctl_table *table, int write,
	void __user *buffer, size_t *length, loff_t *ppos)
{
	int i;
	struct pglist_data *pgdat;
	struct lruvec *lruvec;

	for (i = 0; i < PROTECT_HEAD_END; i++) {
		protect_cur_mbytes[i] = 0;

		for_each_online_pgdat(pgdat) {
			lruvec = get_protect_lruvec(pgdat);

			if (lruvec->protect)
				protect_cur_mbytes[i] += lruvec->heads[i].cur_pages;
		}
		protect_cur_mbytes[i] >>= (20 - PAGE_SHIFT);
	}

	return proc_doulongvec_minmax(table, write, buffer, length, ppos);
}


const struct file_operations proc_protect_level_operations = {
	.write	= protect_level_write,
	.read	= protect_level_read,
	.llseek = noop_llseek,
};

struct ctl_table protect_lru_table[] = {
	{
		.procname	= "protect_lru_enable",
		.data		= &protect_lru_enable,
		.maxlen		= sizeof(unsigned long),
		.mode		= 0640,
		.proc_handler	= proc_doulongvec_minmax,
		.extra1		= &zero,
		.extra2		= &one,
	},
	{
		.procname	= "protect_max_mbytes",
		.data		= &protect_max_mbytes,
		.maxlen		= sizeof(protect_max_mbytes),
		.mode		= 0640,
		.proc_handler	= sysctl_protect_max_mbytes_handler,
		.extra1		= &zero,
		.extra2		= &one_hundred,
	},
	{
		.procname	= "protect_cur_mbytes",
		.data		= &protect_cur_mbytes,
		.maxlen		= sizeof(protect_cur_mbytes),
		.mode		= 0440,
		.proc_handler	= sysctl_protect_cur_mbytes_handler,
	},
	{
		.procname	= "protect_reclaim_ratio",
		.data		= &protect_reclaim_ratio,
		.maxlen		= sizeof(protect_reclaim_ratio),
		.mode		= 0640,
		.proc_handler	= proc_doulongvec_minmax,
		.extra1		= &fifty,
		//.extra2		= &one_hundred,
	},
	{},
};

void protect_lruvec_init(struct mem_cgroup *memcg, struct lruvec *lruvec)
{
	enum lru_list lru;
	struct page *page;
	int i;

	if (mem_cgroup_disabled())
		goto populate;

	if (!memcg)
		return;

#ifdef CONFIG_MEMCG
	if (!root_mem_cgroup)
		goto populate;
#endif

	return;

populate:
	lruvec->protect = true;

	for (i = 0; i < PROTECT_HEAD_MAX; i++) {
		for_each_evictable_lru(lru) {
			page = &lruvec->heads[i].protect_page[lru];

			init_page_count(page);
			page_mapcount_reset(page);
			SetPageReserved(page);
			SetPageLRU(page);
			set_page_protect_num(page, i + 1);
			INIT_LIST_HEAD(&page->lru);

			if (lru >= LRU_INACTIVE_FILE)
				list_add_tail(&page->lru, &lruvec->lists[lru]);
		}
		lruvec->heads[i].max_pages = 0;
		lruvec->heads[i].cur_pages = 0;
	}
}

EXPORT_SYMBOL(protect_lruvec_init);

static int
protect_num_show_debug(struct seq_file *s, struct pglist_data *pgdat,
		       struct list_head *file_lru, enum lru_list lru)
{
	struct page *page, *page2;
	int tmp_num, num = 0;
	int all_count[PROTECT_HEAD_MAX+1] = {0};
	int err_count[PROTECT_HEAD_MAX+1] = {0};
	int i;

	spin_lock_irq(&pgdat->lru_lock);
	list_for_each_entry_safe(page, page2, file_lru, lru) {
		tmp_num = get_page_protect_num(page);
		if (PageReserved(page))	{
			num = (tmp_num == 0) ? PROTECT_HEAD_MAX : tmp_num;
			continue;
		}

		all_count[num]++;

		if (!num)
			continue;

		if (tmp_num == num || (num == PROTECT_HEAD_MAX && !tmp_num))
			continue;
		else {
			err_count[num]++;
			seq_printf(s, "page in error location: page->level=%d, lru_num=%d,"
				   "all_count=%d, PageProtect=%d\n",
				   tmp_num, num, all_count[num], PageProtect(page) ? 1 : 0);
		}
	}
	spin_unlock_irq(&pgdat->lru_lock);

	seq_printf(s, "%s LRU:",
		   lru == LRU_INACTIVE_FILE ? "INACTIVE " : "ACTIVE ");

	for (i = 0; i < PROTECT_HEAD_MAX; i++) {
		seq_printf(s, "L%d: %d/%d ", i, all_count[i], err_count[i]);
	}

	seq_printf(s, "normal: %d/%d\n",
		   all_count[PROTECT_HEAD_MAX], err_count[PROTECT_HEAD_MAX]);

	return 0;
}

static int protect_lru_show_debug(struct seq_file *s, void *unused)
{
	struct lruvec *lruvec;
	struct list_head *file_lru;
	struct pglist_data *pgdat;
	enum lru_list lru;
	ssize_t ret = 0;

	drain_all_pages(NULL);

	for_each_online_pgdat(pgdat) {
		lruvec = get_protect_lruvec(pgdat);

		seq_printf(s, "protect area is in %s\n",
			   mem_cgroup_disabled() ? "pgdat->lruvec" : "root_mem_cgroup");

		if (!lruvec->protect)
			continue;

		lru = LRU_INACTIVE_FILE;
		file_lru = &lruvec->lists[lru];
		ret = protect_num_show_debug(s, pgdat, file_lru, lru);

		lru = LRU_ACTIVE_FILE;
		file_lru = &lruvec->lists[lru];
		ret = protect_num_show_debug(s, pgdat, file_lru, lru);
	}

	return ret;
}

static int protect_page_count_open(struct inode *inode, struct file *file)
{
	return single_open(file, protect_lru_show_debug, NULL);
}

static const struct file_operations proc_protect_lru_operations = {
	.open	= protect_page_count_open,
	.read	= seq_read,
	.llseek	= seq_lseek,
	.release = single_release,
};

static int __init protect_lru_init(void)
{
	int i;
	struct pglist_data *pgdat;
	struct lruvec *lruvec;
	unsigned long prot_pages;
	struct dentry *dentry;

	for (i = 0; i < PROTECT_HEAD_END; i++) {
		prot_pages = protect_max_mbytes[i] << (20 - PAGE_SHIFT);

		for_each_online_pgdat(pgdat) {
			lruvec = get_protect_lruvec(pgdat);

			pr_info("%s:%s pgdat[%d]'s lruvec.\n",
				__func__,
				mem_cgroup_disabled() ? "" : "root_mem_cgroup's",
				pgdat->node_id);

			if (lruvec->protect)
				lruvec->heads[i].max_pages = prot_pages;
		}
	}

	dentry = debugfs_create_file("protect_page_count", 0400, NULL,
				     NULL, &proc_protect_lru_operations);

	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	return 0;
}

module_init(protect_lru_init);
