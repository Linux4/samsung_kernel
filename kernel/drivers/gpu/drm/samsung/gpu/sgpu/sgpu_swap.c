// SPDX-License-Identifier: GPL-2.0
/*
 * TTM swap implementation for SGPU
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 */

#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/shmem_fs.h>

#include <drm/drm_print.h>

#include "amdgpu.h"
#include "amdgpu_ttm.h"
#include "amdgpu_trace.h"

#define SGPU_TTM_TT_FLAG_SWAPPED BIT(30)

void sgpu_tt_destroy_notify(struct ttm_tt *ttm)
{
	if (unlikely(!!(ttm->page_flags & SGPU_TTM_TT_FLAG_SWAPPED)))
		sgpu_sub_swap_pages(ttm->num_pages);
}

int sgpu_tt_swapin(struct ttm_tt *ttm)
{
	struct address_space *swap_space;
	struct file *swap_storage;
	struct page *from_page;
	struct page *to_page;
	gfp_t gfp_mask;
	int i, ret;

	if (likely(!(ttm->page_flags & SGPU_TTM_TT_FLAG_SWAPPED)))
		return 0;

	swap_storage = ttm->swap_storage;
	BUG_ON(swap_storage == NULL);

	swap_space = swap_storage->f_mapping;
	gfp_mask = mapping_gfp_mask(swap_space);

	for (i = 0; i < ttm->num_pages; ++i) {
		from_page = shmem_read_mapping_page_gfp(swap_space, i,
							gfp_mask);
		if (IS_ERR(from_page)) {
			ret = PTR_ERR(from_page);
			goto out_err;
		}
		to_page = ttm->pages[i];
		if (unlikely(to_page == NULL)) {
			ret = -ENOMEM;
			goto out_err;
		}

		copy_highpage(to_page, from_page);
		put_page(from_page);
	}

	fput(swap_storage);
	ttm->swap_storage = NULL;
	ttm->page_flags &= ~SGPU_TTM_TT_FLAG_SWAPPED;

	sgpu_sub_swap_pages(ttm->num_pages);

	return 0;

out_err:
	return ret;
}

static int sgpu_tt_swapout(struct ttm_device *bdev, struct ttm_tt *ttm, gfp_t gfp_flags)
{
	loff_t size = (loff_t)ttm->num_pages << PAGE_SHIFT;
	struct address_space *swap_space;
	struct file *swap_storage;
	struct page *from_page;
	struct page *to_page;
	int i, ret;

	if (!ttm_tt_is_populated(ttm))
		return 0;

	swap_storage = shmem_file_setup("sgpu swap", size, 0);
	if (IS_ERR(swap_storage)) {
		DRM_ERROR("Failed allocating swap storage\n");
		return PTR_ERR(swap_storage);
	}

	swap_space = swap_storage->f_mapping;
	gfp_flags &= mapping_gfp_mask(swap_space);

	for (i = 0; i < ttm->num_pages; ++i) {
		from_page = ttm->pages[i];
		if (unlikely(from_page == NULL))
			continue;

		to_page = shmem_read_mapping_page_gfp(swap_space, i, gfp_flags);
		if (IS_ERR(to_page)) {
			ret = PTR_ERR(to_page);
			goto out_err;
		}
		copy_highpage(to_page, from_page);
		set_page_dirty(to_page);
		mark_page_accessed(to_page);
		put_page(to_page);
	}

	ret = reclaim_shmem_address_space(swap_space);
	if (ret < 0) {
		DRM_ERROR("Failed to reclaim bounce shm: %d bytes (err %d)\n",
			  ttm->num_pages, ret);
		goto out_err;
	}

	ttm_tt_unpopulate(bdev, ttm);

	ttm->swap_storage = swap_storage;
	ttm->page_flags |= SGPU_TTM_TT_FLAG_SWAPPED;

	sgpu_add_swap_pages(ttm->num_pages);

	return ret;
out_err:
	fput(swap_storage);

	return ret;
}

/* copied from ttm_resource_add_bulk_move() in ttm_resource.c */
static void sgpu_ttm_resource_add_bulk_move(struct ttm_resource *res, struct ttm_buffer_object *bo)
{
	if (bo->bulk_move && !bo->pin_count) {
		struct ttm_lru_bulk_move *bulk = bo->bulk_move;
		struct ttm_lru_bulk_move_pos *pos = &bulk->pos[res->mem_type][res->bo->priority];

		if (!pos->first) {
			pos->first = res;
			pos->last = res;
		} else {
			if (pos->last != res) {
				list_move(&res->lru, &pos->last->lru);
				pos->last = res;
			}
		}
	}
}

static void sgpu_ttm_resource_del_bulk_move(struct ttm_resource *res, struct ttm_buffer_object *bo)
{
	if (bo->bulk_move && !bo->pin_count) {
		struct ttm_lru_bulk_move *bulk = bo->bulk_move;
		struct ttm_lru_bulk_move_pos *pos = &bulk->pos[res->mem_type][res->bo->priority];

		if (unlikely(pos->first == res && pos->last == res)) {
			pos->first = NULL;
			pos->last = NULL;
		} else if (pos->first == res) {
			pos->first = list_next_entry(res, lru);
		} else if (pos->last == res) {
			pos->last = list_prev_entry(res, lru);
		} else {
			list_move(&res->lru, &pos->last->lru);
		}
	}
}

#define TTM_PAGE_FLAG_NOT_TO_SWAP \
	(TTM_TT_FLAG_EXTERNAL | TTM_TT_FLAG_SWAPPED | SGPU_TTM_TT_FLAG_SWAPPED)
static int sgpu_bo_swapout(struct ttm_buffer_object *tbo)
{
	struct ttm_operation_ctx ttm_ctx = { };
	struct ttm_resource *mem;
	struct ttm_place hop = { .mem_type = TTM_PL_SYSTEM };
	bool locked;
	int ret = -EBUSY;

	locked = dma_resv_trylock(tbo->base.resv);
	if (!locked)
		return ret;

	if (!tbo->ttm || !ttm_tt_is_populated(tbo->ttm) || tbo->deleted ||
	    tbo->ttm->page_flags & TTM_PAGE_FLAG_NOT_TO_SWAP)
		goto err;

	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem) {
		ret = -ENOMEM;
		goto err;
	}

	ttm_resource_init(tbo, &hop, mem);

	spin_lock(&tbo->bdev->lru_lock);
	sgpu_ttm_resource_add_bulk_move(mem, tbo);
	spin_unlock(&tbo->bdev->lru_lock);

	ttm_bo_unmap_virtual(tbo);

	ret = dma_resv_reserve_fences(tbo->base.resv, 1);
	if (ret)
		goto err2;

	ret = amdgpu_bo_move(tbo, true, &ttm_ctx, mem, &hop);
	WARN_ON(ret == -EMULTIHOP);
	if (ret)
		goto err2;

	ret = ttm_bo_wait(tbo, false, false);
	if (ret)
		goto err;

	ret = sgpu_tt_swapout(tbo->bdev, tbo->ttm, GFP_KERNEL);
	if (ret < 0)
		goto err;

	dma_resv_unlock(tbo->base.resv);

	return ret;
err2:
	spin_lock(&tbo->bdev->lru_lock);
	sgpu_ttm_resource_del_bulk_move(mem, tbo);
	spin_unlock(&tbo->bdev->lru_lock);
	kfree(mem);
err:
	dma_resv_unlock(tbo->base.resv);

	return ret == -EBUSY ? -ENOSPC : ret;
}

struct sgpu_proc_node {
	struct kobject kobj;
	pid_t tgid;
	struct mutex lock;
	struct list_head ctx_list;
	struct list_head node;
	struct mutex swap_lock;
	atomic_t swap_lock_count;
	atomic_t swapin_pending;
	bool swap_in_progress;
	wait_queue_head_t wait_swap;
};

static struct sgpu_sysfs_proc_struct {
	struct kobject kobj;
	struct list_head list;
} sgpu_sysfs_proc;

static DEFINE_MUTEX(sgpu_proc_lock);

struct swap_stat {
	unsigned int nr_bo_tried;
	unsigned int nr_bo_swapped;
	unsigned int nr_byte_tried;
	unsigned int nr_byte_swapped;
};

void sgpu_swap_lock(struct amdgpu_fpriv *fpriv)
{
	struct sgpu_proc_node *proc = fpriv->proc_ctx.proc;
	struct drm_file *drmfile = fpriv->drm_file;

	get_file(drmfile->filp);

	mutex_lock(&proc->swap_lock);
	atomic_inc(&proc->swap_lock_count);
	wait_event(proc->wait_swap, !proc->swap_in_progress);
	mutex_unlock(&proc->swap_lock);
}

void sgpu_swap_unlock(struct amdgpu_fpriv *fpriv)
{
	struct sgpu_proc_node *proc = fpriv->proc_ctx.proc;
	struct drm_file *drmfile = fpriv->drm_file;

	atomic_dec(&proc->swap_lock_count);
	WARN_ON(atomic_read(&proc->swap_lock_count) < 0);

	fput(drmfile->filp);
}

static bool sgpu_swap_continue(struct sgpu_proc_node *proc)
{
	if (!mutex_trylock(&proc->swap_lock))
		return false;

	if (atomic_read(&proc->swap_lock_count) > 0 || atomic_read(&proc->swapin_pending) > 0) {
		mutex_unlock(&proc->swap_lock);
		return false;
	}

	proc->swap_in_progress = true;
	mutex_unlock(&proc->swap_lock);

	return true;
}

static void sgpu_swap_done(struct sgpu_proc_node *proc)
{
	proc->swap_in_progress = false;
	wake_up(&proc->wait_swap);
}

static struct sgpu_proc_node *sgpu_sysfs_find_proc(pid_t tgid)
{
	struct sgpu_proc_node *proc;

	list_for_each_entry(proc, &sgpu_sysfs_proc.list, node)
		if (proc->tgid == tgid)
			return proc;
	return NULL;
}

void sgpu_swap_init_bo(struct amdgpu_bo *bo)
{
	INIT_LIST_HEAD(&bo->swap_node);
}

void sgpu_swap_add_bo(struct amdgpu_fpriv *fpriv, struct amdgpu_bo *bo)
{
	mutex_lock(&fpriv->proc_ctx.lock);
	list_add_tail(&bo->swap_node, &fpriv->proc_ctx.bo_list);
	mutex_unlock(&fpriv->proc_ctx.lock);
}

void sgpu_swap_remove_bo(struct amdgpu_fpriv *fpriv, struct amdgpu_bo *bo)
{
	mutex_lock(&fpriv->proc_ctx.lock);
	list_del_init(&bo->swap_node);
	mutex_unlock(&fpriv->proc_ctx.lock);
}

static bool sgpu_proc_swapout(struct sgpu_proc_ctx *ctx,
			      unsigned long max_size, struct swap_stat *stat)
{
	struct amdgpu_bo *bo;

	list_for_each_entry(bo, &ctx->bo_list, swap_node) {
		struct ttm_buffer_object *tbo;
		struct ttm_tt *ttm;
		int ret;

		if (amdgpu_bo_encrypted(bo))
			continue;

		tbo = &bo->tbo;
		ttm = tbo->ttm;

		if (ttm->page_flags & SGPU_TTM_TT_FLAG_SWAPPED)
			continue;

		if (max_size < (stat->nr_byte_swapped + tbo->base.size))
			return false;

		if (!sgpu_swap_continue(ctx->proc))
			return false;

		stat->nr_bo_tried++;
		stat->nr_byte_tried += tbo->base.size;

		ret = sgpu_bo_swapout(tbo);
		if (ret >= 0) {
			stat->nr_bo_swapped++;
			stat->nr_byte_swapped += ret << PAGE_SHIFT;
		} else if (ret != -EBUSY) {
			DRM_ERROR("Failed to swapout: %zu bytes (err %d)\n", tbo->base.size, ret);
		}

		sgpu_swap_done(ctx->proc);
	}

	return true;
}

static bool sgpu_proc_swapin(struct sgpu_proc_ctx *ctx, unsigned long zero, struct swap_stat *stat)
{
	struct amdgpu_bo *bo;

	list_for_each_entry(bo, &ctx->bo_list, swap_node) {
		uint32_t domain = bo->preferred_domains;
		struct ttm_buffer_object *tbo = &bo->tbo;
		struct ttm_operation_ctx ttm_ctx = {
			.interruptible = true,
			.no_wait_gpu = false,
			.resv = tbo->base.resv
		};
		struct ttm_tt *ttm = tbo->ttm;
		int ret = 0;

		if (!(ttm->page_flags & SGPU_TTM_TT_FLAG_SWAPPED))
			continue;

		stat->nr_bo_tried++;
		stat->nr_byte_tried += tbo->base.size;

		dma_resv_lock(tbo->base.resv, NULL);
		amdgpu_bo_placement_from_domain(bo, domain);
		ret = ttm_bo_validate(tbo, &bo->placement, &ttm_ctx);
		dma_resv_unlock(tbo->base.resv);

		if (ret) {
			DRM_ERROR("Failed to swapin: %zu bytes (err %d)\n", tbo->base.size, ret);
		} else {
			stat->nr_bo_swapped++;
			stat->nr_byte_swapped += tbo->base.size;
		}
	}

	return true;
}

static void sgpu_swap_proc(struct sgpu_proc_node *proc, unsigned long val)
{
	bool (*swap_func)(struct sgpu_proc_ctx *c, unsigned long l, struct swap_stat *s);
	struct sgpu_proc_ctx *ctx;
	bool swapin = !val;
	struct swap_stat stat = { 0 };
	ktime_t begin = ktime_get();
	const char *opname = "";

	if (swapin) {
		swap_func = &sgpu_proc_swapin;
		atomic_inc(&proc->swapin_pending);
		opname = "in";
	} else {
		swap_func = &sgpu_proc_swapout;
		opname = "out";
	}

	mutex_lock(&proc->lock);

	if (swapin)
		atomic_dec(&proc->swapin_pending);

	list_for_each_entry(ctx, &proc->ctx_list, node) {
		bool cont;

		mutex_lock(&ctx->lock);
		cont = swap_func(ctx, val, &stat);
		mutex_unlock(&ctx->lock);
		if (!cont)
			break;
	}

	mutex_unlock(&proc->lock);

	if (swapin)
		trace_gmr_swapin(proc->tgid, stat.nr_byte_swapped / PAGE_SIZE,
				 ktime_us_delta(ktime_get(), begin));
	else
		trace_gmr_swapout(proc->tgid, val / PAGE_SIZE, 0, stat.nr_byte_swapped / PAGE_SIZE,
				  ktime_us_delta(ktime_get(), begin));

	DRM_INFO("TGID %d Swap-%s: %u/%u buffers, %u/%u bytes\n", proc->tgid, opname,
		 stat.nr_bo_swapped, stat.nr_bo_tried, stat.nr_byte_tried, stat.nr_byte_swapped);
}

static ssize_t swap_store(struct kobject *kobj, struct kobj_attribute *attr,
			  const char *buf, size_t count)
{
	struct sgpu_proc_node *proc = container_of(kobj, typeof(*proc), kobj);
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	sgpu_swap_proc(proc, val);

	return count;
}

static ssize_t swap_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct sgpu_proc_node *proc = container_of(kobj, typeof(*proc), kobj);
	struct sgpu_proc_ctx *ctx;
	size_t populated = 0;

	mutex_lock(&proc->lock);
	list_for_each_entry(ctx, &proc->ctx_list, node) {
		struct amdgpu_bo *bo;

		mutex_lock(&ctx->lock);
		list_for_each_entry(bo, &ctx->bo_list, swap_node) {
			struct ttm_buffer_object *tbo = &bo->tbo;
			struct ttm_tt *ttm = tbo->ttm;

			ttm_bo_get(tbo);
			if (ttm && ttm_tt_is_populated(ttm))
				populated += ttm->num_pages;
			ttm_bo_put(tbo);
		}
		mutex_unlock(&ctx->lock);
	}
	mutex_unlock(&proc->lock);

	return sysfs_emit(buf, "%zu\n", populated << PAGE_SHIFT);
}

static ssize_t summary_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct sgpu_proc_node *proc = container_of(kobj, typeof(*proc), kobj);
	struct sgpu_proc_ctx *ctx;
	size_t total = 0;
	size_t populated = 0;
	int nr_buffer = 0, nr_context = 0;

	mutex_lock(&proc->lock);
	list_for_each_entry(ctx, &proc->ctx_list, node) {
		struct amdgpu_bo *bo;

		mutex_lock(&ctx->lock);
		list_for_each_entry(bo, &ctx->bo_list, swap_node) {
			struct ttm_buffer_object *tbo = &bo->tbo;
			struct ttm_tt *ttm = tbo->ttm;

			ttm_bo_get(tbo);
			total += tbo->base.size;
			if (ttm && ttm_tt_is_populated(ttm))
				populated += ttm->num_pages;
			ttm_bo_put(tbo);

			nr_buffer++;
		}
		mutex_unlock(&ctx->lock);
		nr_context++;
	}
	mutex_unlock(&proc->lock);

	return sysfs_emit(buf, "TOTAL %d contexts, %d buffers, %zu bytes, %zu bytes populated\n",
			  nr_context, nr_buffer, total, populated << PAGE_SHIFT);
}

static void sgpu_proc_release(struct kobject *kobj)
{
	struct sgpu_proc_node *proc = container_of(kobj, typeof(*proc), kobj);

	DRM_INFO("Removing proc node %s\n", kobject_name(kobj));
	/*
	 * lock should be held while a sysfs node is being destroyed to prevent a new amdgpu_fpriv
	 * is added to the sysfs.
	 */
	BUG_ON(!mutex_is_locked(&sgpu_proc_lock));
	WARN_ON(atomic_read(&proc->swap_lock_count) != 0);

	list_del(&proc->node);

	kfree(proc);
}

static struct kobj_attribute sgpu_buffer_summary = __ATTR_RO(summary);
static struct kobj_attribute sgpu_buffer_swap = __ATTR_RW_MODE(swap, 0664);

static struct attribute *sgpu_proc_attrs[] = {
	&sgpu_buffer_summary.attr,
	&sgpu_buffer_swap.attr,
	NULL,
};
ATTRIBUTE_GROUPS(sgpu_proc);

static void sgpu_kobj_get_ownership(struct kobject *kobj, kuid_t *uid, kgid_t *gid)
{
	*uid = current_euid();
	*gid = current_egid();
}

static struct kobj_type sgpu_proc_ktype = {
	.release = sgpu_proc_release,
	.sysfs_ops = &kobj_sysfs_ops,
	.default_groups = sgpu_proc_groups,
	.get_ownership = sgpu_kobj_get_ownership,
};

static void sgpu_proc_init_ctx(struct sgpu_proc_ctx *ctx)
{
	INIT_LIST_HEAD(&ctx->bo_list);
	mutex_init(&ctx->lock);
}

int sgpu_proc_add_context(struct amdgpu_fpriv *fpriv)
{
	struct sgpu_proc_node *proc;
	int ret = -ENOMEM;

	mutex_lock(&sgpu_proc_lock);
	/* fpriv->tgid is not set at this time */
	proc = sgpu_sysfs_find_proc(current->tgid);
	if (proc) {
		kobject_get(&proc->kobj);

		sgpu_proc_init_ctx(&fpriv->proc_ctx);

		mutex_lock(&proc->lock);
		list_add(&fpriv->proc_ctx.node, &proc->ctx_list);
		mutex_unlock(&proc->lock);

		fpriv->proc_ctx.proc = proc;
		mutex_unlock(&sgpu_proc_lock);
		return 0;
	}

	proc = kzalloc(sizeof(*proc), GFP_KERNEL);
	if (!proc)
		goto err_alloc;

	INIT_LIST_HEAD(&proc->ctx_list);
	mutex_init(&proc->lock);
	mutex_init(&proc->swap_lock);
	proc->tgid = current->tgid;
	init_waitqueue_head(&proc->wait_swap);
	sgpu_proc_init_ctx(&fpriv->proc_ctx);

	list_add(&proc->node, &sgpu_sysfs_proc.list);
	list_add(&fpriv->proc_ctx.node, &proc->ctx_list);
	fpriv->proc_ctx.proc = proc;

	ret = kobject_init_and_add(&proc->kobj, &sgpu_proc_ktype, &sgpu_sysfs_proc.kobj,
				   "%d", proc->tgid);
	if (ret) {
		DRM_ERROR("Failed to add sysfs swap node for PID %d\n", proc->tgid);
		goto err_sysfs;
	}

	mutex_unlock(&sgpu_proc_lock);

	return 0;

err_sysfs:
	list_del_init(&fpriv->proc_ctx.node);
	list_del(&proc->node);
	kfree(proc);
err_alloc:
	mutex_unlock(&sgpu_proc_lock);
	return ret;
}

void sgpu_proc_remove_context(struct amdgpu_fpriv *fpriv)
{
	struct sgpu_proc_node *proc;

	mutex_lock(&sgpu_proc_lock);

	proc = sgpu_sysfs_find_proc(fpriv->tgid);
	if (proc) {
		mutex_lock(&proc->lock);
		list_del_init(&fpriv->proc_ctx.node);
		mutex_unlock(&proc->lock);

		kobject_put(&proc->kobj);
	}

	mutex_unlock(&sgpu_proc_lock);

	WARN_ON(!proc);
}

static void sgpu_sysfs_proc_release(struct kobject *kobj)
{
}

static struct kobj_type sgpu_sysfs_proc_ktype = {
	.release = sgpu_sysfs_proc_release,
	.sysfs_ops = &kobj_sysfs_ops,
};

int sgpu_sysfs_proc_init(struct kobject *gpu_root)
{
	int ret;

	INIT_LIST_HEAD(&sgpu_sysfs_proc.list);

	ret = kobject_init_and_add(&sgpu_sysfs_proc.kobj, &sgpu_sysfs_proc_ktype, gpu_root, "proc");
	if (ret)
		DRM_ERROR("Failed to add 'proc' node to sysfs (err %d)\n", ret);

	return ret;
}

static ssize_t sgpu_swap_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
				    const char *buf, size_t count)
{
	struct sgpu_proc_node *proc;
	unsigned long val;
	int ret, tgid;

	ret = sscanf(buf, "%d %lu", &tgid, &val);
	if (ret != 2)
		return -EINVAL;

	mutex_lock(&sgpu_proc_lock);
	proc = sgpu_sysfs_find_proc(tgid);
	if (!proc) {
		mutex_unlock(&sgpu_proc_lock);
		return -EINVAL;
	}

	kobject_get(&proc->kobj);
	mutex_unlock(&sgpu_proc_lock);

	sgpu_swap_proc(proc, val);

	mutex_lock(&sgpu_proc_lock);
	kobject_put(&proc->kobj);
	mutex_unlock(&sgpu_proc_lock);
	return count;
}
struct kobj_attribute attr_gpu_swap_ctrl = __ATTR_WO(sgpu_swap_ctrl);
