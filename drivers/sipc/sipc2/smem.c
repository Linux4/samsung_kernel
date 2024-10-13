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

#include <linux/debugfs.h>
#include <linux/genalloc.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#include <linux/sipc.h>
#include <linux/sipc_priv.h>

struct smem_phead {
	struct list_head 	smem_phead;
	spinlock_t		lock;
	uint32_t		poolnum;
	uint32_t		main_pool_addr;
};

struct smem_pool {
	struct list_head	smem_head;
	struct list_head 	smem_plist;
	spinlock_t		lock;

	uint32_t		addr;
	uint32_t		size;
	atomic_t		used;

	struct gen_pool		*gen;
};

struct smem_record {
	struct list_head smem_list;
	struct task_struct *task;
	uint32_t size;
	uint32_t addr;
};

struct smem_map_list {
	struct list_head map_head;
	spinlock_t       lock;
	uint32_t         inited;
};

struct smem_map {
	struct list_head map_list;
	struct task_struct *task;
	const void *mem;
	unsigned int count;
};

static struct smem_phead	sipc_smem_phead;
static struct smem_map_list	mem_mp;

int smem_init_phead(uint32_t addr)
{
	struct smem_phead *phead = &sipc_smem_phead;

	phead->main_pool_addr = addr;
	phead->poolnum = 0;
	spin_lock_init(&phead->lock);
	INIT_LIST_HEAD(&phead->smem_phead);
	pr_info("smem_init_phead: main_pool_addr = 0x%x.\n", phead->main_pool_addr);

	return 0;
}

int smem_init(uint32_t addr, uint32_t size)
{
	struct smem_phead *phead = &sipc_smem_phead;
	struct smem_map_list *smem = &mem_mp;
	struct smem_pool *spool, *pos;
	unsigned long flags;

	spin_lock_irqsave(&phead->lock, flags);
	list_for_each_entry(pos, &phead->smem_phead, smem_plist) {
		if(pos->addr == addr){
			pr_err("smem_init: pool addr 0x%x, size %d has been initialized!\n", addr, pos->size);
			spin_unlock_irqrestore(&phead->lock, flags);
			return -1;
		}
	}
	spool = kzalloc(sizeof(struct smem_pool), GFP_KERNEL);
	if (!spool) {
		pr_err("smem_init: failed to alloc smem pool\n");
		spin_unlock_irqrestore(&phead->lock, flags);
		return -1;
	}
	list_add_tail(&spool->smem_plist, &phead->smem_phead);
	phead->poolnum++;
	spin_unlock_irqrestore(&phead->lock, flags);

	spool->addr = addr;
	spool->size = PAGE_ALIGN(size);
	atomic_set(&spool->used, 0);
	spin_lock_init(&spool->lock);
	INIT_LIST_HEAD(&spool->smem_head);

	spin_lock_init(&smem->lock);
	INIT_LIST_HEAD(&smem->map_head);
	smem->inited = 1;

	/* allocator block size is times of pages */
	if (spool->size >= SMEM_ALIGN_POOLSZ)
		spool->gen = gen_pool_create(PAGE_SHIFT, -1);
	else
		spool->gen = gen_pool_create(SMEM_MIN_ORDER, -1);

	if (!spool->gen) {
		pr_err("Failed to create smem gen pool!\n");
		return -1;
	}

	if (gen_pool_add(spool->gen, spool->addr, spool->size, -1) != 0) {
		pr_err("Failed to add smem gen pool!\n");
		return -1;
	}
	pr_info("smem_init: pool addr = 0x%x, size = 0x%x added.\n", spool->addr, spool->size);

	return 0;
}

/* ****************************************************************** */

uint32_t smem_alloc_ex(uint32_t size, uint32_t paddr)
{
	struct smem_phead *phead = &sipc_smem_phead;
	struct smem_pool *spool = NULL;
	struct smem_pool *pos;
	struct smem_record *recd;
	unsigned long flags;
	uint32_t addr = 0;

	spin_lock_irqsave(&phead->lock, flags);
	list_for_each_entry(pos, &phead->smem_phead, smem_plist) {
		if(pos->addr == paddr)
			spool = pos;

	}
	spin_unlock_irqrestore(&phead->lock, flags);

	if(spool == NULL) {
		pr_err("smem_alloc_ex: pool addr 0x%x is not existed!\n", paddr);
		addr = 0;
		goto error;
	}

	recd = kzalloc(sizeof(struct smem_record), GFP_KERNEL);
	if (!recd) {
		pr_err("failed to alloc smem record\n");
		addr = 0;
		goto error;
	}

	if (spool->size >= SMEM_ALIGN_POOLSZ) {
		size = PAGE_ALIGN(size);
	} else {
		size = SMEM_ALCSZ_ALIGN(size, SMEM_ALIGN_BYTES);
	}
	addr = gen_pool_alloc(spool->gen, size);
	if (!addr) {
		pr_err("failed to alloc smem from gen pool\n");
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
EXPORT_SYMBOL(smem_alloc_ex);

uint32_t smem_alloc(uint32_t size)
{
	struct smem_phead *phead = &sipc_smem_phead;

	return smem_alloc_ex(size, phead->main_pool_addr);
}
EXPORT_SYMBOL(smem_alloc);

static void *shmem_ram_vmap(phys_addr_t start, size_t size, int noncached)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	unsigned int i;
	void *vaddr;
	phys_addr_t addr;
	unsigned long flags;
	struct smem_map *map;
	struct smem_map_list *smem = &mem_mp;

	map = kzalloc(sizeof(struct smem_map), GFP_KERNEL);
	if (!map) {
		return NULL;
	}

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);
	if (noncached)
		prot = pgprot_noncached(PAGE_KERNEL);
	else
		prot = pgprot_writecombine(PAGE_KERNEL);

	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		pr_err("%s: Failed to allocate array for %u pages\n",
		       __func__, page_count);
		return NULL;
	}

	for (i = 0; i < page_count; i++) {
		addr = page_start + i * PAGE_SIZE;
		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
	vaddr = vm_map_ram(pages, page_count, -1, prot);
	kfree(pages);

	map->count = page_count;
	map->mem = vaddr;
	map->task = current;

	if (smem->inited) {
		spin_lock_irqsave(&smem->lock, flags);
		list_add_tail(&map->map_list, &smem->map_head);
		spin_unlock_irqrestore(&smem->lock, flags);
	}
	return vaddr;
}

void shmem_ram_unmap(const void *mem)
{
	struct smem_map *map, *next;
	unsigned long flags;
	struct smem_map_list *smem = &mem_mp;

	if (smem->inited) {
		spin_lock_irqsave(&smem->lock, flags);
		list_for_each_entry_safe(map, next, &smem->map_head, map_list) {
		if (map->mem == mem) {
				list_del(&map->map_list);
				vm_unmap_ram(mem, map->count);
				kfree(map);
				break;
			}
		}
		spin_unlock_irqrestore(&smem->lock, flags);
	}
}
EXPORT_SYMBOL(shmem_ram_unmap);

void *shmem_ram_vmap_nocache(phys_addr_t start, size_t size)
{
	return shmem_ram_vmap(start, size, 1);
}
EXPORT_SYMBOL(shmem_ram_vmap_nocache);

void *shmem_ram_vmap_cache(phys_addr_t start, size_t size)
{
	return shmem_ram_vmap(start, size, 0);
}
EXPORT_SYMBOL(shmem_ram_vmap_cache);

void smem_free_ex(uint32_t addr, uint32_t size, uint32_t paddr)
{
	struct smem_phead *phead = &sipc_smem_phead;
	struct smem_pool *spool = NULL;
	struct smem_pool *pos;
	struct smem_record *recd, *next;
	unsigned long flags;

	spin_lock_irqsave(&phead->lock, flags);
	list_for_each_entry(pos, &phead->smem_phead, smem_plist) {
		if(pos->addr == paddr){
			spool = pos;
		}
	}
	spin_unlock_irqrestore(&phead->lock, flags);

	if(spool == NULL) {
		pr_err("smem_free_ex: pool addr 0x%x is not existed!\n", paddr);
		return;
	}

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
EXPORT_SYMBOL(smem_free_ex);

void smem_free(uint32_t addr, uint32_t size)
{
	struct smem_phead *phead = &sipc_smem_phead;

	smem_free_ex(addr, size, phead->main_pool_addr);
}
EXPORT_SYMBOL(smem_free);

#ifdef CONFIG_DEBUG_FS
static int smem_debug_show(struct seq_file *m, void *private)
{
	struct smem_phead *phead = &sipc_smem_phead;
	struct smem_pool *spool, *pos;
	struct smem_record *recd;
	uint32_t fsize;
	unsigned long flags;

	spin_lock_irqsave(&phead->lock, flags);
	list_for_each_entry(pos, &phead->smem_phead, smem_plist) {
		spool = pos;
		fsize = gen_pool_avail(spool->gen);

		seq_puts(m, "*********************************************************\n");
		seq_puts(m, "smem pool infomation:\n");
		seq_printf(m, "phys_addr=0x%x, total=0x%x, used=0x%x, free=0x%x\n",
			spool->addr, spool->size, spool->used.counter, fsize);
		seq_puts(m, "smem record list:\n");

		list_for_each_entry(recd, &spool->smem_head, smem_list) {
			seq_printf(m, "task %s: pid=%u, addr=0x%x, size=0x%x\n",
				recd->task->comm,
				recd->task->pid,
				recd->addr,
				recd->size);
		}
	}
	spin_unlock_irqrestore(&phead->lock, flags);
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
	debugfs_create_file("smem", S_IRUGO,
			    (struct dentry *)root, NULL,
			    &smem_debug_fops);
	return 0;
}

#endif /* endof CONFIG_DEBUG_FS */


MODULE_AUTHOR("Chen Gaopeng");
MODULE_DESCRIPTION("SIPC/SMEM driver");
MODULE_LICENSE("GPL");
