/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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
#include <linux/module.h>
#include <linux/genalloc.h>
#include <linux/mm.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/sched.h>

#include <linux/sipc.h>

struct smem_pool {
	struct list_head 	smem_head;
	spinlock_t 		lock;

	uint32_t		addr;
	uint32_t		size;
	atomic_t 		used;

	struct gen_pool		*gen;
};

struct smem_record {
	struct list_head smem_list;
	struct task_struct *task;
	uint32_t size;
	uint32_t addr;
};

static struct smem_pool		mem_pool;

int smem_init(uint32_t addr, uint32_t size)
{
	struct smem_pool *spool = &mem_pool;

	spool->addr = addr;
	spool->size = PAGE_ALIGN(size);
	atomic_set(&spool->used, 0);
	spin_lock_init(&spool->lock);
	INIT_LIST_HEAD(&spool->smem_head);

	/* allocator block size is times of pages */
	spool->gen = gen_pool_create(PAGE_SHIFT, -1);
	if (!spool->gen) {
		printk(KERN_ERR "Failed to create smem gen pool!\n");
		return -1;
	}

	if (gen_pool_add(spool->gen, spool->addr, spool->size, -1) != 0) {
		printk(KERN_ERR "Failed to add smem gen pool!\n");
		return -1;
	}

	return 0;
}

/* ****************************************************************** */

uint32_t smem_alloc(uint32_t size)
{
	struct smem_pool *spool = &mem_pool;
	struct smem_record *recd;
	unsigned long flags;
	uint32_t addr;

	recd = kzalloc(sizeof(struct smem_record), GFP_KERNEL);
	if (!recd) {
		printk(KERN_ERR "failed to alloc smem record\n");
		addr = 0;
		goto error;
	}

	size = PAGE_ALIGN(size);
	addr = gen_pool_alloc(spool->gen, size);
	if (!addr) {
		printk(KERN_ERR "failed to alloc smem from gen pool\n");
		kfree(recd);
		goto error;
	}

	/* record smem alloc info */
	atomic_add(size, &spool->used);
	recd->size = size;
	recd->task = current;
	recd->addr = addr;
	spin_lock_irqsave(&spool->lock, flags);
	list_add_tail(&recd->smem_list, &spool->smem_head);
	spin_unlock_irqrestore(&spool->lock, flags);

error:
	return addr;
}

void smem_free(uint32_t addr, uint32_t size)
{
	struct smem_pool *spool = &mem_pool;
	struct smem_record *recd, *next;
	unsigned long flags;

	size = PAGE_ALIGN(size);
	atomic_sub(size, &spool->used);
	gen_pool_free(spool->gen, addr, size);
	/* delete record node from list */
	spin_lock_irqsave(&spool->lock, flags);
	list_for_each_entry_safe(recd, next, &spool->smem_head, smem_list) {
		if (recd->addr == addr) {
			list_del(&recd->smem_list);
			kfree(recd);
			break;
		}
	}
	spin_unlock_irqrestore(&spool->lock, flags);
}

#ifdef CONFIG_DEBUG_FS
static int smem_debug_show(struct seq_file *m, void *private)
{
	struct smem_pool *spool = &mem_pool;
	struct smem_record *recd;
	uint32_t fsize;
	unsigned long flags;

	fsize = gen_pool_avail(spool->gen);

	seq_printf(m, "smem pool infomation:\n");
	seq_printf(m, "phys_addr=0x%x, total=0x%x, used=0x%x, free=0x%x\n",
		spool->addr, spool->size, spool->used, fsize);
	seq_printf(m, "smem record list:\n");

	spin_lock_irqsave(&spool->lock, flags);
	list_for_each_entry(recd, &spool->smem_head, smem_list) {
		seq_printf(m, "task %s: pid=%u, addr=0x%x, size=0x%x\n",
			recd->task->comm, recd->task->pid, recd->addr, recd->size);
	}
	spin_unlock_irqrestore(&spool->lock, flags);
	return 0;
}

static int smem_debug_open(struct inode *inode, struct file *file)
{
        return single_open(file, smem_debug_show, inode->i_private);
}

static const struct file_operations smem_debug_fops = {
        .open = smem_debug_open,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = single_release,
};

int smem_init_debugfs(void *root)
{
        if (!root)
                return -ENXIO;
        debugfs_create_file("smem", S_IRUGO, (struct dentry *)root, NULL, &smem_debug_fops);
        return 0;
}

#endif //endof CONFIG_DEBUG_FS

EXPORT_SYMBOL(smem_alloc);
EXPORT_SYMBOL(smem_free);

MODULE_AUTHOR("Chen Gaopeng");
MODULE_DESCRIPTION("SIPC/SMEM driver");
MODULE_LICENSE("GPL");
