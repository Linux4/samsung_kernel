/*
 * Copyright 2008 Advanced Micro Devices, Inc.
 * Copyright 2008 Red Hat Inc.
 * Copyright 2009 Jerome Glisse.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Dave Airlie
 *          Alex Deucher
 *          Jerome Glisse
 */
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/pci.h>
#include <linux/dma-buf.h>
#include <linux/kernel.h>

#include <drm/sgpu_drm.h>
#include <drm/drm_drv.h>
#include <drm/drm_gem_ttm_helper.h>
#include <drm/drm_debugfs.h>

#include "amdgpu.h"
#include "amdgpu_display.h"
#include "amdgpu_dma_buf.h"
#include <trace/events/gpu_mem.h>

static const struct drm_gem_object_funcs amdgpu_gem_object_funcs;

static vm_fault_t amdgpu_gem_fault(struct vm_fault *vmf)
{
	struct ttm_buffer_object *bo = vmf->vma->vm_private_data;
	struct drm_device *ddev = bo->base.dev;
	vm_fault_t ret;
	int idx;

	ret = ttm_bo_vm_reserve(bo, vmf);
	if (ret)
		return ret;

	if (drm_dev_enter(ddev, &idx)) {
		ret = amdgpu_bo_fault_reserve_notify(bo);
		if (ret) {
			drm_dev_exit(idx);
			goto unlock;
		}

		ret = ttm_bo_vm_fault_reserved(vmf, vmf->vma->vm_page_prot,
					       TTM_BO_VM_NUM_PREFAULT);

		drm_dev_exit(idx);
	} else {
		ret = ttm_bo_vm_dummy_page(vmf, vmf->vma->vm_page_prot);
	}
	if (ret == VM_FAULT_RETRY && !(vmf->flags & FAULT_FLAG_RETRY_NOWAIT))
		return ret;

unlock:
	dma_resv_unlock(bo->base.resv);
	return ret;
}

static const struct vm_operations_struct amdgpu_gem_vm_ops = {
	.fault = amdgpu_gem_fault,
	.open = ttm_bo_vm_open,
	.close = ttm_bo_vm_close,
	.access = ttm_bo_vm_access
};

static void amdgpu_gem_object_free(struct drm_gem_object *gobj)
{
	struct amdgpu_bo *robj = gem_to_amdgpu_bo(gobj);

	if (robj) {
		amdgpu_bo_unref(&robj);
	}
}

int amdgpu_gem_object_create(struct amdgpu_device *adev, unsigned long size,
			     int alignment, u32 initial_domain,
			     u64 flags, enum ttm_bo_type type,
			     struct dma_resv *resv,
			     struct drm_gem_object **obj)
{
	struct amdgpu_bo *bo;
	struct amdgpu_bo_param bp;
	int r;

	memset(&bp, 0, sizeof(bp));
	*obj = NULL;

	bp.size = size;
	bp.byte_align = alignment;
	bp.type = type;
	bp.resv = resv;
	bp.preferred_domain = initial_domain;
	bp.flags = flags;
	if (amdgpu_force_gtt && (initial_domain & AMDGPU_GEM_DOMAIN_VRAM))
		bp.domain = AMDGPU_GEM_DOMAIN_GTT;
	else
		bp.domain = initial_domain;
	r = amdgpu_bo_create(adev, &bp, &bo);
	if (r)
		return r;

	*obj = &bo->tbo.base;
	(*obj)->funcs = &amdgpu_gem_object_funcs;

	return 0;
}

void amdgpu_gem_force_release(struct amdgpu_device *adev)
{
	struct drm_device *ddev = adev_to_drm(adev);
	struct drm_file *file;

	mutex_lock(&ddev->filelist_mutex);

	list_for_each_entry(file, &ddev->filelist, lhead) {
		struct drm_gem_object *gobj;
		int handle;

		WARN_ONCE(1, "Still active user space clients!\n");
		spin_lock(&file->table_lock);
		idr_for_each_entry(&file->object_idr, gobj, handle) {
			WARN_ONCE(1, "And also active allocations!\n");
			drm_gem_object_put(gobj);
		}
		idr_destroy(&file->object_idr);
		spin_unlock(&file->table_lock);
	}

	mutex_unlock(&ddev->filelist_mutex);
}

/*
 * Call from drm_gem_handle_create which appear in both new and open ioctl
 * case.
 */
static int amdgpu_gem_object_open(struct drm_gem_object *obj,
				  struct drm_file *file_priv)
{
	struct amdgpu_bo *abo = gem_to_amdgpu_bo(obj);
	struct amdgpu_device *adev = amdgpu_ttm_adev(abo->tbo.bdev);
	struct amdgpu_fpriv *fpriv = file_priv->driver_priv;
	struct amdgpu_vm *vm = &fpriv->vm;
	struct amdgpu_bo_va *bo_va;
	struct mm_struct *mm;
	int r;

	mm = amdgpu_ttm_tt_get_usermm(abo->tbo.ttm);
	if (mm && mm != current->mm)
		return -EPERM;

	if (abo->flags & AMDGPU_GEM_CREATE_VM_ALWAYS_VALID &&
	    abo->tbo.base.resv != vm->root.base.bo->tbo.base.resv)
		return -EPERM;

	r = amdgpu_bo_reserve(abo, false);
	if (r)
		return r;

	bo_va = amdgpu_vm_bo_find(vm, abo);
	if (!bo_va) {
		bo_va = amdgpu_vm_bo_add(adev, vm, abo);
	} else {
		++bo_va->ref_count;
	}
	amdgpu_bo_unreserve(abo);
	return 0;
}

static void amdgpu_gem_object_close(struct drm_gem_object *obj,
				    struct drm_file *file_priv)
{
	struct amdgpu_bo *bo = gem_to_amdgpu_bo(obj);
	struct amdgpu_device *adev;
	struct amdgpu_fpriv *fpriv = file_priv->driver_priv;
	struct amdgpu_vm *vm = &fpriv->vm;

	struct amdgpu_bo_list_entry vm_pd;
	struct list_head list, duplicates;
	struct dma_fence *fence = NULL;
	struct ttm_validate_buffer tv;
	struct ww_acquire_ctx ticket;
	struct amdgpu_bo_va *bo_va;
	long r;

	INIT_LIST_HEAD(&list);
	INIT_LIST_HEAD(&duplicates);

	if (!bo)
		return;

	sgpu_swap_remove_bo(fpriv, bo);

	adev = amdgpu_ttm_adev(bo->tbo.bdev);

	tv.bo = &bo->tbo;
	tv.num_shared = 2;
	list_add(&tv.head, &list);

	amdgpu_vm_get_pd_bo(vm, &list, &vm_pd);

	r = ttm_eu_reserve_buffers(&ticket, &list, false, &duplicates);
	if (r) {
		dev_err(adev->dev, "leaking bo va because "
			"we fail to reserve bo (%ld)\n", r);
		return;
	}
	bo_va = amdgpu_vm_bo_find(vm, bo);
	if (!bo_va || --bo_va->ref_count)
		goto out_unlock;

	if (!amdgpu_ttm_tt_is_userptr(bo->tbo.ttm))
		amdgpu_gem_bo_size(obj, file_priv, SUB_BO_SIZE);

	amdgpu_vm_bo_rmv(adev, bo_va);
	if (!amdgpu_vm_ready(NULL, vm))
		goto out_unlock;
//FIXME: seems like it is not needed
#if 0
	r = dma_resv_get_singleton(bo->tbo.base.resv,
		DMA_RESV_USAGE_BOOKKEEP, &fence);
	if (r == 0) {
		amdgpu_bo_fence(bo, fence, true);
		fence = NULL;
	}
#endif
	r = amdgpu_vm_clear_freed(adev, vm, &fence);
	if (r || !fence)
		goto out_unlock;

	amdgpu_bo_fence(bo, fence, true);
	dma_fence_put(fence);

out_unlock:
	if (unlikely(r < 0))
		dev_err(adev->dev, "failed to clear page "
			"tables on GEM object close (%ld)\n", r);
	ttm_eu_backoff_reservation(&ticket, &list);
}

static int sgpu_gem_create(struct drm_device *dev, union drm_amdgpu_gem_create *args,
			   struct drm_file *filp)
{
	struct amdgpu_device *adev = drm_to_adev(dev);
	struct amdgpu_fpriv *fpriv = filp->driver_priv;
	struct amdgpu_vm *vm = &fpriv->vm;
	uint64_t flags = args->in.domain_flags;
	uint64_t size = args->in.bo_size;
	struct dma_resv *resv = NULL;
	struct drm_gem_object *gobj;
	uint32_t handle, initial_domain;
	int r;

	/* reject invalid gem flags */
	if (flags & ~(AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED |
		      AMDGPU_GEM_CREATE_NO_CPU_ACCESS |
		      AMDGPU_GEM_CREATE_CPU_GTT_USWC |
		      AMDGPU_GEM_CREATE_VRAM_CLEARED |
		      AMDGPU_GEM_CREATE_VM_ALWAYS_VALID |
		      AMDGPU_GEM_CREATE_EXPLICIT_SYNC |
		      AMDGPU_GEM_CREATE_ENCRYPTED |
			  AMDGPU_GEM_CREATE_UNCACHED))

		return -EINVAL;

	/* reject invalid gem domains */
	if (args->in.domains & ~AMDGPU_GEM_DOMAIN_MASK)
		return -EINVAL;

	if (!amdgpu_is_tmz(adev) && (flags & AMDGPU_GEM_CREATE_ENCRYPTED)) {
		DRM_NOTE_ONCE("Cannot allocate secure buffer since TMZ is disabled\n");
		return -EINVAL;
	}

	/* create a gem object to contain this object in */
	if (args->in.domains & (AMDGPU_GEM_DOMAIN_GDS |
	    AMDGPU_GEM_DOMAIN_GWS | AMDGPU_GEM_DOMAIN_OA)) {
		if (flags & AMDGPU_GEM_CREATE_VM_ALWAYS_VALID) {
			/* if gds bo is created from user space, it must be
			 * passed to bo list
			 */
			DRM_ERROR("GDS bo cannot be per-vm-bo\n");
			return -EINVAL;
		}
		flags |= AMDGPU_GEM_CREATE_NO_CPU_ACCESS;
	}

	if (flags & AMDGPU_GEM_CREATE_VM_ALWAYS_VALID) {
		r = amdgpu_bo_reserve(vm->root.base.bo, false);
		if (r)
			return r;

		resv = vm->root.base.bo->tbo.base.resv;
	}

	initial_domain = (u32)(0xffffffff & args->in.domains);

retry:
	if (IS_ENABLED(CONFIG_DRM_SGPU_FORCE_WRITECOMBINE) && !sgpu_is_dma_coherent(adev))
		flags |= AMDGPU_GEM_CREATE_CPU_GTT_USWC;

	r = amdgpu_gem_object_create(adev, size, args->in.alignment,
				     initial_domain,
				     flags, ttm_bo_type_device, resv, &gobj);
	if (r) {
		if (r != -ERESTARTSYS) {
			if (flags & AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED) {
				flags &= ~AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED;
				goto retry;
			}

			if (initial_domain == AMDGPU_GEM_DOMAIN_VRAM) {
				initial_domain |= AMDGPU_GEM_DOMAIN_GTT;
				goto retry;
			}
			DRM_DEBUG("Failed to allocate GEM object (%llu, %d, %llu, %d)\n",
				  size, initial_domain, args->in.alignment, r);
		}
	}

	if (flags & AMDGPU_GEM_CREATE_VM_ALWAYS_VALID) {
		if (!r) {
			struct amdgpu_bo *abo = gem_to_amdgpu_bo(gobj);
			abo->parent = amdgpu_bo_ref(vm->root.base.bo);
		}
		amdgpu_bo_unreserve(vm->root.base.bo);
	}
	if (r)
		return r;

	amdgpu_gem_bo_size(gobj, filp, ADD_BO_SIZE);

	r = drm_gem_handle_create(filp, gobj, &handle);
	/* drop reference from allocate - handle holds it now */
	drm_gem_object_put(gobj);
	if (r)
		return r;

	memset(args, 0, sizeof(*args));
	args->out.handle = handle;

	sgpu_swap_add_bo(fpriv, gem_to_amdgpu_bo(gobj));

	return 0;
}

static int amdgpu_gem_object_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma)
{
	struct amdgpu_bo *bo = gem_to_amdgpu_bo(obj);
	int r;

	if (amdgpu_ttm_tt_get_usermm(bo->tbo.ttm))
		return -EPERM;
	if (bo->flags & AMDGPU_GEM_CREATE_NO_CPU_ACCESS)
		return -EPERM;

	r = drm_gem_ttm_mmap(obj, vma);
	/* in the case of non-cacheable pages set the pgprot appropriately */
	if (bo->tbo.ttm->caching == ttm_write_combined)
		vma->vm_page_prot = pgprot_writecombine(vm_get_page_prot(vma->vm_flags));

	return r;

}

static const struct drm_gem_object_funcs amdgpu_gem_object_funcs = {
	.free = amdgpu_gem_object_free,
	.open = amdgpu_gem_object_open,
	.close = amdgpu_gem_object_close,
	.export = amdgpu_gem_prime_export,
	.vmap = drm_gem_ttm_vmap,
	.vunmap = drm_gem_ttm_vunmap,
	.mmap = amdgpu_gem_object_mmap,
	.vm_ops = &amdgpu_gem_vm_ops,
};

/*
 * GEM ioctls.
 */
int amdgpu_gem_create_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *filp)
{
	union drm_amdgpu_gem_create *args = data;
	return sgpu_gem_create(dev, args, filp);
}

int sgpu_gem_create_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *filp)
{
	struct drm_sgpu_gem_create *args = data;
	struct drm_gem_object *gobj = NULL;
	struct amdgpu_bo *robj = NULL;
	int r = 0;

	BUG_ON(args == NULL);
	if (args->data.data_size_bytes > sizeof(args->data.data))
		return -EINVAL;

	r = sgpu_gem_create(dev, &(args->gem_create_info), filp);
	if (r)
		return r;

	/* set metadata */
	gobj = drm_gem_object_lookup(filp, args->gem_create_info.out.handle);
	if (gobj == NULL) {
		pr_err("sgpu_gem_create_ioctl failed, gobj == NULL\n");
		return -ENOENT;
	}

	robj = gem_to_amdgpu_bo(gobj);

	r = amdgpu_bo_set_tiling_flags_and_metadata(robj, args->data.data,
						    args->data.data_size_bytes,
						    args->data.flags,
						    args->data.tiling_info);

	drm_gem_object_put(gobj);
	return r;
}

int amdgpu_gem_userptr_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *filp)
{
	struct ttm_operation_ctx ctx = { true, false };
	struct amdgpu_device *adev = drm_to_adev(dev);
	struct drm_amdgpu_gem_userptr *args = data;
	struct drm_gem_object *gobj;
	struct amdgpu_bo *bo;
	uint32_t handle;
	int r;

	args->addr = untagged_addr(args->addr);

	if (offset_in_page(args->addr | args->size))
		return -EINVAL;

	if (IS_ENABLED(CONFIG_DRM_SGPU_FORCE_WRITECOMBINE) && !sgpu_is_dma_coherent(adev))
		return -EINVAL;

	/* reject unknown flag values */
	if (args->flags & ~(AMDGPU_GEM_USERPTR_READONLY |
	    AMDGPU_GEM_USERPTR_ANONONLY | AMDGPU_GEM_USERPTR_VALIDATE |
	    AMDGPU_GEM_USERPTR_REGISTER))
		return -EINVAL;

	if (!(args->flags & AMDGPU_GEM_USERPTR_READONLY) &&
	     !(args->flags & AMDGPU_GEM_USERPTR_REGISTER)) {

		/* if we want to write to it we must install a MMU notifier */
		return -EACCES;
	}

	/* create a gem object to contain this object in */
	r = amdgpu_gem_object_create(adev, args->size, 0, AMDGPU_GEM_DOMAIN_CPU,
				     0, ttm_bo_type_device, NULL, &gobj);
	if (r)
		return r;

	bo = gem_to_amdgpu_bo(gobj);
	bo->preferred_domains = AMDGPU_GEM_DOMAIN_GTT;
	bo->allowed_domains = AMDGPU_GEM_DOMAIN_GTT;
	r = amdgpu_ttm_tt_set_userptr(&bo->tbo, args->addr, args->flags);
	if (r)
		goto release_object;

	if (args->flags & AMDGPU_GEM_USERPTR_VALIDATE) {
		r = amdgpu_bo_reserve(bo, true);
		if (r)
			goto release_object;

		amdgpu_bo_placement_from_domain(bo, AMDGPU_GEM_DOMAIN_GTT);
		r = ttm_bo_validate(&bo->tbo, &bo->placement, &ctx);
		amdgpu_bo_unreserve(bo);
		if (r)
			goto release_object;
	}

	r = drm_gem_handle_create(filp, gobj, &handle);
	if (r)
		goto release_object;

	args->handle = handle;

release_object:
	drm_gem_object_put(gobj);

	return r;
}

int amdgpu_mode_dumb_mmap(struct drm_file *filp,
			  struct drm_device *dev,
			  uint32_t handle, uint64_t *offset_p)
{
	struct drm_gem_object *gobj;
	struct amdgpu_bo *robj;

	gobj = drm_gem_object_lookup(filp, handle);
	if (gobj == NULL) {
		return -ENOENT;
	}
	robj = gem_to_amdgpu_bo(gobj);
	if (amdgpu_ttm_tt_get_usermm(robj->tbo.ttm) ||
	    (robj->flags & AMDGPU_GEM_CREATE_NO_CPU_ACCESS)) {
		drm_gem_object_put(gobj);
		return -EPERM;
	}
	*offset_p = amdgpu_bo_mmap_offset(robj);
	drm_gem_object_put(gobj);
	return 0;
}

int amdgpu_gem_mmap_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *filp)
{
	union drm_amdgpu_gem_mmap *args = data;
	uint32_t handle = args->in.handle;
	memset(args, 0, sizeof(*args));
	return amdgpu_mode_dumb_mmap(filp, dev, handle, &args->out.addr_ptr);
}

/**
 * amdgpu_gem_timeout - calculate jiffies timeout from absolute value
 *
 * @timeout_ns: timeout in ns
 *
 * Calculate the timeout in jiffies from an absolute timeout in ns.
 */
unsigned long amdgpu_gem_timeout(uint64_t timeout_ns)
{
	unsigned long timeout_jiffies;
	ktime_t timeout;

	if (sgpu_no_timeout != 0) {
		return MAX_SCHEDULE_TIMEOUT;
	}

	if (IS_ENABLED(CONFIG_DRM_SGPU_EMULATOR_WORKAROUND))
		timeout_ns *= 10;

	/* clamp timeout if it's to large */
	if (((int64_t)timeout_ns) < 0)
		return MAX_SCHEDULE_TIMEOUT;

	timeout = ktime_sub(ns_to_ktime(timeout_ns), ktime_get());
	if (ktime_to_ns(timeout) < 0)
		return 0;

	timeout_jiffies = nsecs_to_jiffies(ktime_to_ns(timeout));
	/*  clamp timeout to avoid unsigned-> signed overflow */
	if (timeout_jiffies > MAX_SCHEDULE_TIMEOUT )
		return MAX_SCHEDULE_TIMEOUT - 1;

	return timeout_jiffies;
}

int amdgpu_gem_wait_idle_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *filp)
{
	union drm_amdgpu_gem_wait_idle *args = data;
	struct drm_gem_object *gobj;
	struct amdgpu_bo *robj;
	uint32_t handle = args->in.handle;
	unsigned long timeout = amdgpu_gem_timeout(args->in.timeout);
	int r = 0;
	long ret;

	gobj = drm_gem_object_lookup(filp, handle);
	if (gobj == NULL) {
		return -ENOENT;
	}
	robj = gem_to_amdgpu_bo(gobj);
	ret = dma_resv_wait_timeout(robj->tbo.base.resv, DMA_RESV_USAGE_READ,
				    true, timeout);

	/* ret == 0 means not signaled,
	 * ret > 0 means signaled
	 * ret < 0 means interrupted before timeout
	 */
	if (ret >= 0) {
		memset(args, 0, sizeof(*args));
		args->out.status = (ret == 0);
	} else
		r = ret;

	drm_gem_object_put(gobj);
	return r;
}

int amdgpu_gem_metadata_ioctl(struct drm_device *dev, void *data,
				struct drm_file *filp)
{
	struct drm_amdgpu_gem_metadata *args = data;
	struct drm_gem_object *gobj;
	struct amdgpu_bo *robj;
	int r = -1;

	DRM_DEBUG("%d \n", args->handle);
	gobj = drm_gem_object_lookup(filp, args->handle);
	if (gobj == NULL)
		return -ENOENT;
	robj = gem_to_amdgpu_bo(gobj);

	r = amdgpu_bo_reserve(robj, false);
	if (unlikely(r != 0))
		goto out;

	if (args->op == AMDGPU_GEM_METADATA_OP_GET_METADATA) {
		amdgpu_bo_get_tiling_flags(robj, &args->data.tiling_info);
		r = amdgpu_bo_get_metadata(robj, args->data.data,
					   sizeof(args->data.data),
					   &args->data.data_size_bytes,
					   &args->data.flags);
	} else if (args->op == AMDGPU_GEM_METADATA_OP_SET_METADATA) {
		if (args->data.data_size_bytes > sizeof(args->data.data)) {
			r = -EINVAL;
			goto unreserve;
		}
		r = amdgpu_bo_set_tiling_flags_and_metadata(robj, args->data.data,
					   args->data.data_size_bytes,
					   args->data.flags,
					   args->data.tiling_info);
	} else if (args->op == AMDGPU_GEM_METADATA_OP_GET_BO_FLAGS) {
		args->data.flags = robj->flags;
	}

unreserve:
	amdgpu_bo_unreserve(robj);
out:
	drm_gem_object_put(gobj);
	return r;
}

/**
 * amdgpu_gem_va_update_vm -update the bo_va in its VM
 *
 * @adev: amdgpu_device pointer
 * @vm: vm to update
 * @bo_va: bo_va to update
 * @operation: map, unmap or clear
 *
 * Update the bo_va directly after setting its address. Errors are not
 * vital here, so they are not reported back to userspace.
 */
static void amdgpu_gem_va_update_vm(struct amdgpu_device *adev,
				    struct amdgpu_vm *vm,
				    struct amdgpu_bo_va *bo_va,
				    uint32_t operation)
{
	int r;

	if (!amdgpu_vm_ready(bo_va, vm))
		return;

	r = amdgpu_vm_clear_freed(adev, vm, NULL);
	if (r)
		goto error;

	if (operation == AMDGPU_VA_OP_MAP ||
	    operation == AMDGPU_VA_OP_REPLACE) {
		r = amdgpu_vm_bo_update(adev, bo_va, false);
		if (r)
			goto error;
	}

	r = amdgpu_vm_update_pdes(adev, vm, false);

error:
	if (r && r != -ERESTARTSYS)
		DRM_ERROR("Couldn't update BO_VA (%d)\n", r);
}

/**
 * amdgpu_gem_va_map_flags - map GEM UAPI flags into hardware flags
 *
 * @adev: amdgpu_device pointer
 * @flags: GEM UAPI flags
 *
 * Returns the GEM UAPI flags mapped into hardware for the ASIC.
 */
uint64_t amdgpu_gem_va_map_flags(struct amdgpu_device *adev, uint32_t flags)
{
	uint64_t pte_flag = 0;

	if (flags & AMDGPU_VM_PAGE_EXECUTABLE)
		pte_flag |= AMDGPU_PTE_EXECUTABLE;
	if (flags & AMDGPU_VM_PAGE_READABLE)
		pte_flag |= AMDGPU_PTE_READABLE;
	if (flags & AMDGPU_VM_PAGE_WRITEABLE)
		pte_flag |= AMDGPU_PTE_WRITEABLE;
	if (flags & AMDGPU_VM_PAGE_PRT)
		pte_flag |= AMDGPU_PTE_PRT;

	if (adev->gmc.gmc_funcs->map_mtype)
		pte_flag |= amdgpu_gmc_map_mtype(adev,
						 flags & AMDGPU_VM_MTYPE_MASK);

	return pte_flag;
}

int amdgpu_gem_va_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *filp)
{
	const uint32_t valid_flags = AMDGPU_VM_DELAY_UPDATE |
		AMDGPU_VM_PAGE_READABLE | AMDGPU_VM_PAGE_WRITEABLE |
		AMDGPU_VM_PAGE_EXECUTABLE | AMDGPU_VM_MTYPE_MASK;
	const uint32_t prt_flags = AMDGPU_VM_DELAY_UPDATE |
		AMDGPU_VM_PAGE_PRT;

	struct drm_amdgpu_gem_va *args = data;
	struct drm_gem_object *gobj;
	struct amdgpu_device *adev = drm_to_adev(dev);
	struct amdgpu_fpriv *fpriv = filp->driver_priv;
	struct amdgpu_bo *abo;
	struct amdgpu_bo_va *bo_va;
	struct amdgpu_bo_list_entry vm_pd;
	struct ttm_validate_buffer tv;
	struct ww_acquire_ctx ticket;
	struct list_head list, duplicates;
	uint64_t va_flags;
	uint64_t vm_size;
	int r = 0;
	struct ttm_operation_ctx ctx = {
		.interruptible = true,
		.no_wait_gpu = false,
		.resv = 0
	};

	if (args->va_address < AMDGPU_VA_RESERVED_SIZE) {
		dev_dbg(dev->dev,
			"va_address 0x%LX is in reserved area 0x%LX\n",
			args->va_address, AMDGPU_VA_RESERVED_SIZE);
		return -EINVAL;
	}

	if (args->va_address >= AMDGPU_GMC_HOLE_START &&
	    args->va_address < AMDGPU_GMC_HOLE_END) {
		dev_dbg(dev->dev,
			"va_address 0x%LX is in VA hole 0x%LX-0x%LX\n",
			args->va_address, AMDGPU_GMC_HOLE_START,
			AMDGPU_GMC_HOLE_END);
		return -EINVAL;
	}

	args->va_address &= AMDGPU_GMC_HOLE_MASK;

	vm_size = adev->vm_manager.max_pfn * AMDGPU_GPU_PAGE_SIZE;
	vm_size -= AMDGPU_VA_RESERVED_SIZE;
	if (args->va_address + args->map_size > vm_size) {
		dev_dbg(dev->dev,
			"va_address 0x%llx is in top reserved area 0x%llx\n",
			args->va_address + args->map_size, vm_size);
		return -EINVAL;
	}

	if ((args->flags & ~valid_flags) && (args->flags & ~prt_flags)) {
		dev_dbg(dev->dev, "invalid flags combination 0x%08X\n",
			args->flags);
		return -EINVAL;
	}

	switch (args->operation) {
	case AMDGPU_VA_OP_MAP:
	case AMDGPU_VA_OP_UNMAP:
	case AMDGPU_VA_OP_CLEAR:
	case AMDGPU_VA_OP_REPLACE:
		break;
	default:
		dev_dbg(dev->dev, "unsupported operation %d\n",
			args->operation);
		return -EINVAL;
	}

	INIT_LIST_HEAD(&list);
	INIT_LIST_HEAD(&duplicates);
	if ((args->operation != AMDGPU_VA_OP_CLEAR) &&
	    !(args->flags & AMDGPU_VM_PAGE_PRT)) {
		gobj = drm_gem_object_lookup(filp, args->handle);
		if (gobj == NULL)
			return -ENOENT;
		abo = gem_to_amdgpu_bo(gobj);
		tv.bo = &abo->tbo;
		if (abo->flags & AMDGPU_GEM_CREATE_VM_ALWAYS_VALID)
			tv.num_shared = 1;
		else
			tv.num_shared = 0;
		list_add(&tv.head, &list);
	} else {
		gobj = NULL;
		abo = NULL;
	}

	amdgpu_vm_get_pd_bo(&fpriv->vm, &list, &vm_pd);

	r = ttm_eu_reserve_buffers(&ticket, &list, true, &duplicates);
	if (r)
		goto error_unref;

	if (abo) {
		bo_va = amdgpu_vm_bo_find(&fpriv->vm, abo);
		if (!bo_va) {
			r = -ENOENT;
			goto error_backoff;
		}
	} else if (args->operation != AMDGPU_VA_OP_CLEAR) {
		bo_va = fpriv->prt_va;
	} else {
		bo_va = NULL;
	}

	switch (args->operation) {
	case AMDGPU_VA_OP_MAP:
		va_flags = amdgpu_gem_va_map_flags(adev, args->flags);
		r = amdgpu_vm_bo_map(adev, bo_va, args->va_address,
				     args->offset_in_bo, args->map_size,
				     va_flags);
		break;
	case AMDGPU_VA_OP_UNMAP:
		r = amdgpu_vm_bo_unmap(adev, bo_va, args->va_address);
		break;

	case AMDGPU_VA_OP_CLEAR:
		r = amdgpu_vm_bo_clear_mappings(adev, &fpriv->vm,
						args->va_address,
						args->map_size);
		break;
	case AMDGPU_VA_OP_REPLACE:
		va_flags = amdgpu_gem_va_map_flags(adev, args->flags);
		r = amdgpu_vm_bo_replace_map(adev, bo_va, args->va_address,
					     args->offset_in_bo, args->map_size,
					     va_flags);
		break;
	default:
		break;
	}

	if (abo && abo->tbo.base.import_attach && abo->tbo.base.dma_buf) {
		ctx.resv = abo->tbo.base.resv;
		amdgpu_bo_placement_from_domain(abo, AMDGPU_GEM_DOMAIN_GTT);
		r = ttm_bo_validate(&abo->tbo, &abo->placement, &ctx);
	}
	if (!r && !(args->flags & AMDGPU_VM_DELAY_UPDATE) && !amdgpu_vm_debug)
		amdgpu_gem_va_update_vm(adev, &fpriv->vm, bo_va,
					args->operation);

error_backoff:
	ttm_eu_backoff_reservation(&ticket, &list);

error_unref:
	drm_gem_object_put(gobj);
	return r;
}

int amdgpu_gem_op_ioctl(struct drm_device *dev, void *data,
			struct drm_file *filp)
{
	struct amdgpu_device *adev = drm_to_adev(dev);
	struct drm_amdgpu_gem_op *args = data;
	struct drm_gem_object *gobj;
	struct amdgpu_bo *robj;
	int r;

	gobj = drm_gem_object_lookup(filp, args->handle);
	if (gobj == NULL) {
		return -ENOENT;
	}
	robj = gem_to_amdgpu_bo(gobj);

	r = amdgpu_bo_reserve(robj, false);
	if (unlikely(r))
		goto out;

	switch (args->op) {
	case AMDGPU_GEM_OP_GET_GEM_CREATE_INFO: {
		struct drm_amdgpu_gem_create_in info;
		void __user *out = u64_to_user_ptr(args->value);

		info.bo_size = robj->tbo.base.size;
		info.alignment = robj->tbo.page_alignment << PAGE_SHIFT;
		info.domains = robj->preferred_domains;
		info.domain_flags = robj->flags;
		amdgpu_bo_unreserve(robj);
		if (copy_to_user(out, &info, sizeof(info)))
			r = -EFAULT;
		break;
	}
	/* @todo GFXSW-10164  AMDGPU_GEM_OP_SET_PLACEMENT is not used in the libDRM.
	 * Remove this.
	 */
	case AMDGPU_GEM_OP_SET_PLACEMENT:
		if (robj->tbo.base.import_attach &&
		    (args->value & AMDGPU_GEM_DOMAIN_VRAM)) {
			r = -EINVAL;
			amdgpu_bo_unreserve(robj);
			break;
		}
		if (amdgpu_ttm_tt_get_usermm(robj->tbo.ttm)) {
			r = -EPERM;
			amdgpu_bo_unreserve(robj);
			break;
		}

		robj->preferred_domains = args->value & (AMDGPU_GEM_DOMAIN_VRAM |
							AMDGPU_GEM_DOMAIN_GTT |
							AMDGPU_GEM_DOMAIN_CPU);
		robj->allowed_domains = robj->preferred_domains;
		if (robj->allowed_domains == AMDGPU_GEM_DOMAIN_VRAM)
			robj->allowed_domains |= AMDGPU_GEM_DOMAIN_GTT;

		if (robj->flags & AMDGPU_GEM_CREATE_VM_ALWAYS_VALID)
			amdgpu_vm_bo_invalidate(adev, robj, true);

		amdgpu_bo_unreserve(robj);
		break;
	case AMDGPU_GEM_OP_CACHESYNC: {
		struct drm_sgpu_cache_sync sync;
		enum dma_data_direction dir = DMA_TO_DEVICE;
		size_t length = robj->tbo.base.size;
		off_t offset = 0;
		bool for_gpu;

		if (copy_from_user(&sync, u64_to_user_ptr(args->value), sizeof(sync)))
			return -EFAULT;

		if (sync.op_flags & SGPU_GEM_CACHESYNC_TYPE_PARTIAL) {
			if (length < sync.offset + sync.length)
				return -EINVAL;
			offset = sync.offset;
			length = sync.length;
		}

		for_gpu = !(sync.op_flags & SGPU_GEM_CACHESYNC_MODE_FOR_CPU);

		if (sync.op_flags & SGPU_GEM_CACHESYNC_TYPE_GPU_WRITE) {
			dir = DMA_FROM_DEVICE;
			if (sync.op_flags & SGPU_GEM_CACHESYNC_TYPE_GPU_READ)
				dir = DMA_BIDIRECTIONAL;
		}

		amdgpu_ttm_cache_sync(adev, robj, dir, for_gpu, offset, length);

		amdgpu_bo_unreserve(robj);
		break;
	}
	default:
		amdgpu_bo_unreserve(robj);
		r = -EINVAL;
	}

out:
	drm_gem_object_put(gobj);
	return r;
}

int amdgpu_mode_dumb_create(struct drm_file *file_priv,
			    struct drm_device *dev,
			    struct drm_mode_create_dumb *args)
{
	struct amdgpu_device *adev = drm_to_adev(dev);
	struct drm_gem_object *gobj;
	uint32_t handle;
	u64 flags = AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED |
		    AMDGPU_GEM_CREATE_CPU_GTT_USWC;
	u32 domain;
	int r;

	/*
	 * The buffer returned from this function should be cleared, but
	 * it can only be done if the ring is enabled or we'll fail to
	 * create the buffer.
	 */
	if (adev->mman.buffer_funcs_enabled)
		flags |= AMDGPU_GEM_CREATE_VRAM_CLEARED;

	args->pitch = amdgpu_align_pitch(adev, args->width,
					 DIV_ROUND_UP(args->bpp, 8), 0);
	args->size = (u64)args->pitch * args->height;
	args->size = ALIGN(args->size, PAGE_SIZE);
	domain = amdgpu_bo_get_preferred_pin_domain(adev,
				amdgpu_display_supported_domains(adev, flags));
	r = amdgpu_gem_object_create(adev, args->size, 0, domain, flags,
				     ttm_bo_type_device, NULL, &gobj);
	if (r)
		return -ENOMEM;

	r = drm_gem_handle_create(file_priv, gobj, &handle);
	/* drop reference from allocate - handle holds it now */
	drm_gem_object_put(gobj);
	if (r) {
		return r;
	}
	args->handle = handle;
	return 0;
}

void amdgpu_backoff_reservation(struct ww_acquire_ctx *ticket,
		struct list_head *list)
{
	struct ttm_validate_buffer *entry;

	if (list_empty(list))
		return;

	list_for_each_entry(entry, list, head) {
		struct ttm_buffer_object *bo = entry->bo;
		dma_resv_unlock(bo->base.resv);
	}

	if (ticket)
		ww_acquire_fini(ticket);
}

int amdgpu_gem_bo_size(struct drm_gem_object *gobj, struct drm_file *filp, int flag)
{
	struct amdgpu_bo *bo = gem_to_amdgpu_bo(gobj);
	struct amdgpu_fpriv *afpriv = filp->driver_priv;

	if (afpriv == NULL)
		return 0;

	mutex_lock(&afpriv->memory_lock);
	if ((bo != NULL) && !(bo->tbo.base.import_attach) && !(bo->tbo.base.dma_buf)) {
		if (flag == SUB_BO_SIZE) {
			afpriv->total_pages -= bo->tbo.resource->num_pages;
			sgpu_sub_total_pages(bo->tbo.resource->num_pages);
		}
		else if (flag == ADD_BO_SIZE) {
			afpriv->total_pages += bo->tbo.resource->num_pages;
			sgpu_add_total_pages(bo->tbo.resource->num_pages);
		}
	}
	mutex_unlock(&afpriv->memory_lock);

	trace_gpu_mem_total(0, afpriv->tgid, afpriv->total_pages << PAGE_SHIFT);

	return 0;
}

#if defined(CONFIG_DEBUG_FS)

#define amdgpu_debugfs_gem_bo_print_flag(m, bo, flag)	\
	if (bo->flags & (AMDGPU_GEM_CREATE_ ## flag)) {	\
		seq_printf((m), " " #flag);		\
	}

static int amdgpu_debugfs_gem_bo_info(int id, void *ptr, void *data)
{
	struct drm_gem_object *gobj = ptr;
	struct amdgpu_bo *bo = gem_to_amdgpu_bo(gobj);
	struct seq_file *m = data;

	struct dma_buf_attachment *attachment;
	struct dma_buf *dma_buf;
	unsigned domain;
	const char *placement;
	unsigned pin_count;

	domain = amdgpu_mem_type_to_domain(bo->tbo.resource->mem_type);
	switch (domain) {
	case AMDGPU_GEM_DOMAIN_VRAM:
		placement = "VRAM";
		break;
	case AMDGPU_GEM_DOMAIN_GTT:
		placement = " GTT";
		break;
	case AMDGPU_GEM_DOMAIN_CPU:
	default:
		placement = " CPU";
		break;
	}
	seq_printf(m, "\t0x%08x: %12ld byte %s",
		   id, amdgpu_bo_size(bo), placement);

	pin_count = READ_ONCE(bo->tbo.pin_count);
	if (pin_count)
		seq_printf(m, " pin count %d", pin_count);

	dma_buf = READ_ONCE(bo->tbo.base.dma_buf);
	attachment = READ_ONCE(bo->tbo.base.import_attach);

	if (attachment)
		seq_printf(m, " imported from %p%s", dma_buf,
			   attachment->peer2peer ? " P2P" : "");
	else if (dma_buf)
		seq_printf(m, " exported as %p", dma_buf);

	amdgpu_debugfs_gem_bo_print_flag(m, bo, CPU_ACCESS_REQUIRED);
	amdgpu_debugfs_gem_bo_print_flag(m, bo, NO_CPU_ACCESS);
	amdgpu_debugfs_gem_bo_print_flag(m, bo, CPU_GTT_USWC);
	amdgpu_debugfs_gem_bo_print_flag(m, bo, VRAM_CLEARED);
	amdgpu_debugfs_gem_bo_print_flag(m, bo, SHADOW);
	amdgpu_debugfs_gem_bo_print_flag(m, bo, VRAM_CONTIGUOUS);
	amdgpu_debugfs_gem_bo_print_flag(m, bo, VM_ALWAYS_VALID);
	amdgpu_debugfs_gem_bo_print_flag(m, bo, EXPLICIT_SYNC);

	seq_printf(m, "\n");

	return 0;
}

static int amdgpu_debugfs_gem_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *dev = node->minor->dev;
	struct drm_file *file;
	int r;

	r = mutex_lock_interruptible(&dev->filelist_mutex);
	if (r)
		return r;

	list_for_each_entry(file, &dev->filelist, lhead) {
		struct task_struct *task;

		/*
		 * Although we have a valid reference on file->pid, that does
		 * not guarantee that the task_struct who called get_pid() is
		 * still alive (e.g. get_pid(current) => fork() => exit()).
		 * Therefore, we need to protect this ->comm access using RCU.
		 */
		rcu_read_lock();
		task = pid_task(file->pid, PIDTYPE_PID);
		seq_printf(m, "pid %8d command %s:\n", pid_nr(file->pid),
			   task ? task->comm : "<unknown>");
		rcu_read_unlock();

		spin_lock(&file->table_lock);
		idr_for_each(&file->object_idr, amdgpu_debugfs_gem_bo_info, m);
		spin_unlock(&file->table_lock);
	}

	mutex_unlock(&dev->filelist_mutex);
	return 0;
}

static const struct drm_info_list amdgpu_debugfs_gem_list[] = {
	{"amdgpu_gem_info", &amdgpu_debugfs_gem_info, 0, NULL},
};
#endif

int amdgpu_debugfs_gem_init(struct amdgpu_device *adev)
{
#if defined(CONFIG_DEBUG_FS)
	return amdgpu_debugfs_add_files(adev, amdgpu_debugfs_gem_list,
					ARRAY_SIZE(amdgpu_debugfs_gem_list));
#endif
	return 0;
}
