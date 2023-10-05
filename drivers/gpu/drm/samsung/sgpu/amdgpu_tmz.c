/*
 * Copyright 2022 Advanced Micro Devices, Inc.
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
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/pci.h>
#include "amdgpu.h"
#include "amdgpu_vm.h"
#include "amdgpu_sws.h"
#include "amdgpu_tmz.h"
#include "amdgpu_gmc.h"
#include "amdgpu_ring.h"

static int amdgpu_tmz_static_map(struct amdgpu_device *adev,
				 struct amdgpu_vm *vm,
				 struct amdgpu_bo *bo,
				 struct amdgpu_bo_va **bo_va,
				 u64 addr,
				 u32 size)
{
	struct ww_acquire_ctx ticket;
	struct list_head list;
	struct amdgpu_bo_list_entry pd;
	struct ttm_validate_buffer tv;
	u64 pte_flag = 0;
	int r;

	INIT_LIST_HEAD(&list);
	INIT_LIST_HEAD(&tv.head);
	tv.bo = &bo->tbo;
	tv.num_shared = 1;

	list_add(&tv.head, &list);
	amdgpu_vm_get_pd_bo(vm, &list, &pd);

	DRM_DEBUG_DRIVER("map addr 0x%llx, size 0x%x\n", addr, size);

	r = ttm_eu_reserve_buffers(&ticket, &list, true, NULL);
	if (r) {
		DRM_ERROR("failed to reserve tmz BOs: err=%d\n", r);
		return r;
	}

	*bo_va = amdgpu_vm_bo_add(adev, vm, bo);
	if (!*bo_va) {
		r = -ENOMEM;
		DRM_ERROR("failed to create va for static tmz map\n");
		goto err1;
	}

	r = amdgpu_vm_clear_freed(adev, vm, NULL);
	if (r) {
		DRM_ERROR("failed to clear bo table, err=%d\n", r);
		goto err1;
	}

	//make sure size is PAGE_SIZE aligned
	size = round_up(size, 1 << 12);
	pte_flag = AMDGPU_PTE_READABLE | AMDGPU_PTE_WRITEABLE |
		AMDGPU_PTE_EXECUTABLE |
		amdgpu_gmc_map_mtype(adev, AMDGPU_VM_MTYPE_UC);
	r = amdgpu_vm_bo_map(adev, *bo_va, addr, 0, size, pte_flag);
	if (r) {
		DRM_ERROR("failed to do tmz bo map, err=%d\n", r);
		goto err2;
	}

	r = amdgpu_vm_bo_update(adev, *bo_va, false, NULL);
	if (r) {
		DRM_ERROR("failed to update tmz bo table, err=%d\n", r);
		goto err3;
	}

	if ((*bo_va)->last_pt_update) {
		r = dma_fence_wait((*bo_va)->last_pt_update, true);
		if (r) {
			DRM_ERROR("failed to get pde update fence, err=%d\n",
				  r);
			goto err3;
		}
	}

	r = amdgpu_vm_update_pdes(adev, vm, false);
	if (r) {
		DRM_ERROR("failed to update pde, err=%d\n", r);
		goto err3;
	}

	if (vm->last_update) {
		r = dma_fence_wait(vm->last_update, true);
		if (r) {
			DRM_ERROR("failed to get pde update fence, err=%d\n",
				  r);
			goto err3;
		}
	}

	ttm_eu_backoff_reservation(&ticket, &list);

	return 0;

err3:
	amdgpu_vm_bo_unmap(adev, *bo_va, addr);
err2:
	amdgpu_vm_bo_rmv(adev, *bo_va);
err1:
	ttm_eu_backoff_reservation(&ticket, &list);
	return r;
}

static int amdgpu_tmz_static_unmap(struct amdgpu_device *adev,
				   struct amdgpu_vm *vm,
				   struct amdgpu_bo *bo,
				   struct amdgpu_bo_va *bo_va,
				   uint64_t addr)
{
	struct list_head list;
	struct amdgpu_bo_list_entry pd;
	struct ttm_validate_buffer tv;
	struct ww_acquire_ctx ticket;
	int r;

	INIT_LIST_HEAD(&list);
	INIT_LIST_HEAD(&tv.head);
	tv.bo = &bo->tbo;
	tv.num_shared = 1;

	list_add(&tv.head, &list);
	amdgpu_vm_get_pd_bo(vm, &list, &pd);

	DRM_DEBUG_DRIVER("unmap addr 0x%llx\n", addr);
	r = ttm_eu_reserve_buffers(&ticket, &list, true, NULL);
	if (r) {
		DRM_ERROR("failed to reserve tmz BOs: err=%d\n", r);
		return r;
	}

	r = amdgpu_vm_bo_unmap(adev, bo_va, addr);
	if (r) {
		DRM_ERROR("failed to unmap tmz BOs: err=%d\n", r);
		return r;
	}

	amdgpu_vm_clear_freed(adev, vm, &bo_va->last_pt_update);
	if (bo_va->last_pt_update) {
		r = dma_fence_wait(bo_va->last_pt_update, true);
		if (r) {
			DRM_ERROR("failed to get pde clear fence, err=%d\n",
				  r);
		}
	}

	r = amdgpu_vm_update_pdes(adev, vm, false);
	if (r) {
		DRM_ERROR("failed to update pde, err=%d\n", r);
		return r;
	}

	if (vm->last_update) {
		r = dma_fence_wait(vm->last_update, true);
		if (r) {
			DRM_ERROR("failed to get pde update fence, err=%d\n",
				  r);
			return r;
		}
	}

	amdgpu_vm_bo_rmv(adev, bo_va);

	ttm_eu_backoff_reservation(&ticket, &list);

	return r;
}

static void amdgpu_tmz_wb_free(struct amdgpu_fpriv *fpriv, u32 wb)
{
	wb >>= 3;
	if (wb < fpriv->wb.num_wb)
		__clear_bit(wb, fpriv->wb.used);
}

static int amdgpu_tmz_wb_get(struct amdgpu_fpriv *fpriv, u32 *wb)
{
	unsigned long offset;

	offset = find_first_zero_bit(fpriv->wb.used,
				     fpriv->wb.num_wb);

	if (offset < fpriv->wb.num_wb) {
		__set_bit(offset, fpriv->wb.used);
		*wb = offset << 3; /* convert to dw offset */

		return 0;
	}

	return -EINVAL;
}

static int amdgpu_tmz_init_wb(struct amdgpu_device *adev,
			      struct amdgpu_fpriv *fpriv)
{
	int r;
	u32 wb_size;

	if (fpriv->wb.wb_obj)
		return 0;

	/* AMDGPU_MAX_WB * sizeof(uint32_t) * 8
	 * = AMDGPU_MAX_WB 256bit slots
	 */
	wb_size = AMDGPU_MAX_WB * sizeof(uint32_t) * 8;
	r = amdgpu_bo_create_kernel(adev, wb_size,
				    PAGE_SIZE, AMDGPU_GEM_DOMAIN_GTT,
				    &fpriv->wb.wb_obj,
				    NULL,
				    (void **)&fpriv->wb.wb);
	if (r) {
		DRM_ERROR("create WB bo failed(%d)\n", r);
		return r;
	}

	fpriv->wb.num_wb = AMDGPU_MAX_WB;
	memset(&fpriv->wb.used, 0, sizeof(fpriv->wb.used));

	/* clear wb memory */
	memset((char *)fpriv->wb.wb, 0, wb_size);

	r = amdgpu_tmz_static_map(adev, &fpriv->vm,
				  fpriv->wb.wb_obj,
				  &fpriv->wb_va,
				  AMDGPU_PRIV_WB_OFFSET,
				  wb_size);

	if (r) {
		DRM_ERROR("map tmz wb failed(%d)\n", r);
		amdgpu_bo_free_kernel(&fpriv->wb.wb_obj,
				      &fpriv->wb.gpu_addr,
				      (void **)&fpriv->wb.wb);
		return r;
	}

	fpriv->wb.gpu_addr = AMDGPU_PRIV_WB_OFFSET;

	return 0;
}

static void amdgpu_tmz_deinit_wb(struct amdgpu_device *adev,
				 struct amdgpu_fpriv *fpriv)
{
	if (fpriv->wb.wb_obj) {
		amdgpu_tmz_static_unmap(adev,
					&fpriv->vm,
					fpriv->wb.wb_obj,
					fpriv->wb_va,
					fpriv->wb.gpu_addr);

		fpriv->wb_va = NULL;

		amdgpu_bo_free_kernel(&fpriv->wb.wb_obj,
				      &fpriv->wb.gpu_addr,
				      (void **)&fpriv->wb.wb);
	}
}

static int amdgpu_tmz_init_hqd_eop(struct amdgpu_device *adev,
				   struct amdgpu_fpriv *fpriv)
{
	int r;
	size_t mec_hqd_size;

	if (fpriv->hqd_eop_obj)
		return 0;

	mec_hqd_size = AMDGPU_PRIV_MEC_HQD_EOP_SIZE * 8;

	r = amdgpu_bo_create_kernel(adev, mec_hqd_size, PAGE_SIZE,
				    AMDGPU_GEM_DOMAIN_GTT,
				    &fpriv->hqd_eop_obj,
				    NULL, &fpriv->hqd_cpu_addr);
	if (r)
		return r;

	memset(fpriv->hqd_cpu_addr, 0, mec_hqd_size);

	r = amdgpu_tmz_static_map(adev, &fpriv->vm,
				  fpriv->hqd_eop_obj,
				  &fpriv->hqd_eop_va,
				  AMDGPU_PRIV_HQD_EOP_OFFSET,
				  mec_hqd_size);
	if (r) {
		DRM_ERROR("map tmz hqd failed(%d)\n", r);
		amdgpu_bo_free_kernel(&fpriv->hqd_eop_obj, NULL, NULL);
	}

	return r;
}

static void amdgpu_tmz_deinit_hqd_eop(struct amdgpu_device *adev,
				      struct amdgpu_fpriv *fpriv)
{
	if (fpriv->hqd_eop_obj) {
		amdgpu_tmz_static_unmap(adev,
					&fpriv->vm,
					fpriv->hqd_eop_obj,
					fpriv->hqd_eop_va,
					AMDGPU_PRIV_HQD_EOP_OFFSET);

		fpriv->hqd_eop_va = NULL;

		amdgpu_bo_free_kernel(&fpriv->hqd_eop_obj, NULL,
				      &fpriv->hqd_cpu_addr);
	}
}

static int amdgpu_tmz_init_mqd(struct amdgpu_device *adev,
			       struct amdgpu_fpriv *fpriv,
			       struct amdgpu_ring *ring)
{
	int r;

	if (ring->mqd_obj)
		return 0;

	r = amdgpu_bo_create_kernel(adev,
				    AMDGPU_PRIV_MQD_SIZE,
				    PAGE_SIZE,
				    AMDGPU_GEM_DOMAIN_GTT,
				    &ring->mqd_obj,
				    NULL, &ring->mqd_ptr);
	if (r) {
		DRM_ERROR("failed to create ring mqd bo (%d)", r);
		return r;
	}

	memset(ring->mqd_ptr, 0, AMDGPU_PRIV_MQD_SIZE);

	r = amdgpu_tmz_static_map(adev, &fpriv->vm,
				  ring->mqd_obj,
				  &ring->mqd_va,
				  AMDGPU_PRIV_MQD_OFFSET +
				  ring->resv_slot_idx *
				  AMDGPU_PRIV_MQD_SIZE,
				  AMDGPU_PRIV_MQD_SIZE);
	if (r) {
		DRM_ERROR("failed to map tmz ring mqd bo (%d)", r);
		amdgpu_bo_free_kernel(&ring->mqd_obj,
				      NULL,
				      &ring->mqd_ptr);
	}

	return r;
}

static void amdgpu_tmz_deinit_mqd(struct amdgpu_device *adev,
				  struct amdgpu_fpriv *fpriv,
				  struct amdgpu_ring *ring)
{
	if (ring->mqd_obj) {
		amdgpu_tmz_static_unmap(adev,
					&fpriv->vm,
					ring->mqd_obj,
					ring->mqd_va,
					AMDGPU_PRIV_MQD_OFFSET +
					ring->resv_slot_idx *
					AMDGPU_PRIV_MQD_SIZE);

		ring->mqd_va = NULL;

		amdgpu_bo_free_kernel(&ring->mqd_obj,
				      NULL,
				      &ring->mqd_ptr);
	}
}

static int amdgpu_tmz_init_ring_bo(struct amdgpu_device *adev,
				   struct amdgpu_fpriv *fpriv,
				   struct amdgpu_ring *ring)
{
	int r;

	if (ring->ring_obj)
		return 0;

	r = amdgpu_bo_create_kernel(adev,
				    AMDGPU_PRIV_RING_BUF_SIZE,
				    PAGE_SIZE,
				    AMDGPU_GEM_DOMAIN_GTT,
				    &ring->ring_obj,
				    NULL,
				    (void **)&ring->ring);
	if (r) {
		DRM_ERROR("ring create failed(%d)\n", r);
		return r;
	}

	r = amdgpu_tmz_static_map(adev, &fpriv->vm,
				  ring->ring_obj,
				  &ring->ring_va,
				  AMDGPU_PRIV_RING_BUF_OFFSET +
				  ring->resv_slot_idx *
				  AMDGPU_PRIV_RING_BUF_SIZE,
				  AMDGPU_PRIV_RING_BUF_SIZE);
	if (r) {
		dev_err(adev->dev, "map tmz ring bo failed(%d)\n", r);
		amdgpu_bo_free_kernel(&ring->mqd_obj,
				      NULL,
				     (void **)&ring->ring);
	}

	return r;
}

static void amdgpu_tmz_deinit_ring_bo(struct amdgpu_device *adev,
				      struct amdgpu_fpriv *fpriv,
				      struct amdgpu_ring *ring)
{
	if (ring->ring_obj) {
		amdgpu_tmz_static_unmap(adev,
					&fpriv->vm,
					ring->ring_obj,
					ring->ring_va,
					AMDGPU_PRIV_RING_BUF_OFFSET +
					ring->resv_slot_idx *
					AMDGPU_PRIV_RING_BUF_SIZE);

		ring->ring_va = NULL;

		amdgpu_bo_free_kernel(&ring->ring_obj,
				      NULL,
				      (void **)&ring->ring);
	}
}

static int amdgpu_tmz_init_ring_wb(struct amdgpu_device *adev,
				   struct amdgpu_ring *ring,
				   struct amdgpu_fpriv *fpriv)
{
	int r;

	r = amdgpu_tmz_wb_get(fpriv, &ring->rptr_offs);
	if (r) {
		DRM_ERROR("(%d) failed to get rptr_offs\n", r);
		return r;
	}

	ring->rptr_gpu_addr =
		fpriv->wb.gpu_addr + (ring->rptr_offs * 4);
	ring->rptr_cpu_addr = &fpriv->wb.wb[ring->rptr_offs];
	*ring->rptr_cpu_addr = 0;

	r = amdgpu_tmz_wb_get(fpriv, &ring->wptr_offs);
	if (r) {
		DRM_ERROR("(%d) ring wptr_offs wb alloc failed\n", r);
		goto err1;
	}

	ring->wptr_gpu_addr =
		fpriv->wb.gpu_addr + (ring->wptr_offs * 4);
	ring->wptr_cpu_addr = &fpriv->wb.wb[ring->wptr_offs];
	*ring->wptr_cpu_addr = 0;

	r = amdgpu_tmz_wb_get(fpriv, &ring->fence_offs);
	if (r) {
		dev_err(adev->dev, "(%d) ring fence_offs wb alloc failed\n", r);
		goto err2;
	}
	ring->fence_gpu_addr =
		fpriv->wb.gpu_addr + (ring->fence_offs * 4);
	ring->fence_cpu_addr = &fpriv->wb.wb[ring->fence_offs];
	*ring->fence_cpu_addr = 0;

	r = amdgpu_tmz_wb_get(fpriv, &ring->trail_fence_offs);
	if (r) {
		dev_err(adev->dev,
			"(%d) ring trail_fence_offs wb alloc failed\n", r);
		goto err3;
	}
	ring->trail_fence_gpu_addr =
		fpriv->wb.gpu_addr + (ring->trail_fence_offs * 4);
	ring->trail_fence_cpu_addr = &fpriv->wb.wb[ring->trail_fence_offs];
	*ring->trail_fence_cpu_addr = 0;

	r = amdgpu_tmz_wb_get(fpriv, &ring->cond_exe_offs);
	if (r) {
		dev_err(adev->dev, "(%d) ring cond_exec_offs wb alloc failed\n",
			r);
		goto err4;
	}
	ring->cond_exe_gpu_addr = fpriv->wb.gpu_addr
				  + (ring->cond_exe_offs * 4);
	ring->cond_exe_cpu_addr = &fpriv->wb.wb[ring->cond_exe_offs];
	*ring->cond_exe_cpu_addr = 1;

	return 0;

err4:
	amdgpu_tmz_wb_free(fpriv, ring->trail_fence_offs);
err3:
	amdgpu_tmz_wb_free(fpriv, ring->fence_offs);
err2:
	amdgpu_tmz_wb_free(fpriv, ring->wptr_offs);
err1:
	amdgpu_tmz_wb_free(fpriv, ring->rptr_offs);

	return r;
}

static void amdgpu_tmz_deinit_ring_wb(struct amdgpu_ring *ring,
				      struct amdgpu_fpriv *fpriv)
{
	amdgpu_tmz_wb_free(fpriv, ring->cond_exe_offs);
	amdgpu_tmz_wb_free(fpriv, ring->trail_fence_offs);
	amdgpu_tmz_wb_free(fpriv, ring->fence_offs);
	amdgpu_tmz_wb_free(fpriv, ring->wptr_offs);
	amdgpu_tmz_wb_free(fpriv, ring->rptr_offs);
}

static int amdgpu_tmz_init_ring(struct amdgpu_device *adev,
				struct amdgpu_ring *ring,
				struct amdgpu_ctx *ctx,
				struct amdgpu_fpriv *fpriv)
{
	int r;

	ring->tmz = true;

	ring->ring_obj = NULL;
	ring->use_doorbell = true;
	ring->use_pollfence = amdgpu_poll_eop;

	ring->eop_gpu_addr = AMDGPU_PRIV_HQD_EOP_OFFSET +
		ctx->resv_slot_idx * AMDGPU_PRIV_MEC_HQD_EOP_SIZE;
	memset(fpriv->hqd_cpu_addr +
	       ctx->resv_slot_idx * AMDGPU_PRIV_MEC_HQD_EOP_SIZE,
	       0, AMDGPU_PRIV_MEC_HQD_EOP_SIZE);

	ring->adev = adev;

	ring->resv_slot_idx = ctx->resv_slot_idx;

	r = amdgpu_tmz_init_ring_wb(adev, ring, fpriv);
	if (r) {
		DRM_ERROR("(%d) failed to init tmz wb\n", r);
		goto err1;
	}

	// get ring buffer object
	ring->ring_size = AMDGPU_PRIV_RING_BUF_SIZE;
	ring->buf_mask = (ring->ring_size / 4) - 1;
	ring->ptr_mask = ring->funcs->support_64bit_ptrs ?
			0xffffffffffffffff : ring->buf_mask;
	r = amdgpu_tmz_init_ring_bo(adev, fpriv, ring);
	if (r) {
		DRM_ERROR("failed to get ring buffer object(%d).\n", r);
		goto err2;
	}
	amdgpu_ring_clear_ring(ring);

	ring->gpu_addr = AMDGPU_PRIV_RING_BUF_OFFSET +
			 ring->resv_slot_idx *
			 AMDGPU_PRIV_RING_BUF_SIZE;

	ring->max_dw = AMDGPU_PRIV_RING_MAX_DW;
	ring->priority = DRM_SCHED_PRIORITY_NORMAL;
	mutex_init(&ring->priority_mutex);

	return 0;

err2:
	amdgpu_tmz_deinit_ring_wb(ring, fpriv);
err1:
	ring->adev = NULL;

	return r;
}

static void amdgpu_tmz_deinit_ring(struct amdgpu_ring *ring,
				   struct amdgpu_fpriv *fpriv)
{
	struct amdgpu_device *adev;

	adev = ring->adev;

	/* Don't deinit a ring which is not initialized */
	if (!adev)
		return;

	amdgpu_tmz_deinit_ring_bo(adev, fpriv, ring);

	amdgpu_tmz_deinit_ring_wb(ring, fpriv);

	amdgpu_irq_put(adev, &adev->gfx.eop_irq,
		       ring->fence_drv.irq_type);

	ring->vmid_wait = NULL;
	ring->me = 0;
	ring->adev = NULL;
}

static int amdgpu_tmz_init_res(struct amdgpu_device *adev,
			       struct amdgpu_fpriv *fpriv)
{
	int r;

	if (fpriv->tmz_ready) {
		atomic_inc(&fpriv->tmz_ctx_ref);
		return 0;
	}

	//init wb bo
	r = amdgpu_tmz_init_wb(adev, fpriv);
	if (r)
		return r;

	//allocate eop buffer
	r = amdgpu_tmz_init_hqd_eop(adev, fpriv);
	if (r)
		goto err1;

	atomic_set(&fpriv->tmz_ctx_ref, 1);

	fpriv->tmz_ready = true;
	return 0;

err1:
	amdgpu_tmz_deinit_wb(adev, fpriv);

	return r;
}

static void amdgpu_tmz_deinit_res(struct amdgpu_device *adev,
				  struct amdgpu_fpriv *fpriv)
{
	if (!fpriv->tmz_ready)
		return;

	if (atomic_dec_return(&fpriv->tmz_ctx_ref) > 0)
		return;

	fpriv->tmz_ready = false;
	amdgpu_tmz_deinit_hqd_eop(adev, fpriv);
	amdgpu_tmz_deinit_wb(adev, fpriv);
	DRM_DEBUG_DRIVER("deinit tmz per VM global resource\n");
}

static void amdgpu_tmz_fence_driver_start_ring(struct amdgpu_ring *ring,
					       struct amdgpu_fpriv *fpriv)
{
	u32 seq;
	u32 irq_type;
	struct amdgpu_device *adev;
	struct amdgpu_irq_src *irq_src;

	adev = ring->adev;

	/* 1.0.2 is a static queue for TMZ */
	ring->queue = AMDGPU_TMZ_QUEUE;
	ring->pipe = AMDGPU_TMZ_PIPE;
	ring->me = AMDGPU_TMZ_MEC + adev->gfx.me.num_me;
	irq_src = &adev->gfx.eop_irq;
	irq_type = AMDGPU_CP_IRQ_COMPUTE_MEC1_PIPE0_EOP
		   + (AMDGPU_TMZ_MEC * adev->gfx.mec.num_pipe_per_mec)
		   + ring->pipe;

	ring->fence_drv.irq_type = irq_type;
	amdgpu_irq_get(adev, irq_src, irq_type);

	ring->fence_drv.cpu_addr = &fpriv->wb.wb[ring->fence_offs];
	ring->fence_drv.gpu_addr = fpriv->wb.gpu_addr
				   + (ring->fence_offs * 4);
	seq = atomic_read(&ring->fence_drv.last_seq);
	*ring->fence_drv.cpu_addr = cpu_to_le32(seq);

	ring->fence_drv.irq_src = irq_src;
	ring->fence_drv.irq_type = irq_type;
	ring->fence_drv.initialized = true;
}

static int amdgpu_tmz_init_ctx(struct amdgpu_device *adev,
			       struct amdgpu_fpriv *fpriv,
			       struct amdgpu_ctx *ctx)
{
	int r;
	struct drm_gpu_scheduler *sched = NULL;
	struct drm_sched_entity *entity;
	struct amdgpu_ring *ring;

	r = ida_simple_get(&fpriv->res_slots, 0,
			   AMDGPU_PRIV_MAX_RING, GFP_KERNEL);
	if (r < 0) {
		DRM_DEBUG_DRIVER("no valid solt for CWSR\n");
		return -EINVAL;
	}
	ctx->resv_slot_idx = r;
	DRM_DEBUG_DRIVER("get tmz slot idx:%u\n", r);

	r = amdgpu_sws_early_init_ctx(ctx);
	if (r) {
		ida_simple_remove(&fpriv->res_slots, ctx->resv_slot_idx);
		DRM_WARN("failed to do early ring init\n");
		return r;
	}

	ring = ctx->resv_ring;
	r = amdgpu_tmz_init_ring(adev, ring, ctx, fpriv);
	if (r) {
		DRM_ERROR("failed to init tmz ring\n");
		goto err1;
	}

	r = amdgpu_tmz_init_mqd(adev, fpriv, ring);
	if (r) {
		DRM_ERROR("failed to get mqd for tmz\n");
		goto err2;
	}

	ring->mqd_gpu_addr = AMDGPU_PRIV_MQD_OFFSET +
			 ring->resv_slot_idx *
			 AMDGPU_PRIV_MQD_SIZE;

	/* make sure 1 CS in the ring */
	r = amdgpu_fence_driver_init_ring(ring, 1, NULL);
	if (r) {
		DRM_ERROR("(%d) failed to init tmz fence drv\n", r);
		goto err3;
	}

	entity = &ctx->priv_entities->entity;
	sched = &ring->sched;
	r = drm_sched_entity_init(entity,
				  ctx->init_priority,
				  &sched,
				  1, &ctx->guilty);
	if (r) {
		DRM_ERROR("(%d) failed to init entity\n", r);
		goto err4;
	}

	amdgpu_tmz_fence_driver_start_ring(ring, fpriv);

	r = amdgpu_sws_init_ctx(ctx, fpriv);
	if (r) {
		DRM_ERROR("(%d) failed to init sws ctx\n", r);
		goto err5;
	}

	if (amdgpu_debugfs_ring_init(adev, ring))
		DRM_WARN("failed to init debugfs for ring:%s!\n",
			 ring->name);

	return 0;

err5:
	drm_sched_entity_destroy(entity);
err4:
	amdgpu_fence_driver_deinit_ring(ring);
err3:
	amdgpu_tmz_deinit_mqd(adev, fpriv, ring);
err2:
	amdgpu_tmz_deinit_ring(ring, fpriv);
err1:
	amdgpu_sws_late_deinit_ctx(ctx);
	ida_simple_remove(&fpriv->res_slots, ctx->resv_slot_idx);
	return r;
}

static void amdgpu_tmz_deinit_ctx(struct amdgpu_device *adev,
				  struct amdgpu_ctx *ctx)
{
	struct amdgpu_ring *ring;
	struct amdgpu_fpriv *fpriv;

	fpriv = ctx->fpriv;
	ring = ctx->resv_ring;

	amdgpu_debugfs_ring_fini(ring);

	amdgpu_sws_deinit_ctx(ctx, fpriv);

	amdgpu_fence_driver_deinit_ring(ring);

	amdgpu_tmz_deinit_mqd(adev, fpriv, ring);
	amdgpu_tmz_deinit_ring(ring, fpriv);
	amdgpu_sws_late_deinit_ctx(ctx);
	ida_simple_remove(&fpriv->res_slots, ctx->resv_slot_idx);
}

int amdgpu_tmz_init(struct amdgpu_ctx *ctx)
{
	struct amdgpu_fpriv *fpriv;
	struct amdgpu_device *adev;
	int r = 0;

	adev = ctx->adev;
	fpriv = ctx->fpriv;

	if (!amdgpu_tmz)
		return 0;

	mutex_lock(&fpriv->lock);

	r = amdgpu_tmz_init_res(adev, fpriv);
	if (r) {
		DRM_WARN("failed to init tmz res\n");
		mutex_unlock(&fpriv->lock);
		return r;
	}

	r = amdgpu_tmz_init_ctx(adev, fpriv, ctx);
	if (r) {
		DRM_WARN("failed to init tmz ctx\n");
		goto err1;
	}

	ctx->tmz = true;
	mutex_unlock(&fpriv->lock);
	return r;

err1:
	amdgpu_tmz_deinit_res(adev, fpriv);
	mutex_unlock(&fpriv->lock);
	return r;
}

void amdgpu_tmz_deinit(struct amdgpu_ctx *ctx)
{
	struct amdgpu_fpriv *fpriv;
	struct amdgpu_device *adev;

	adev = ctx->adev;
	fpriv = ctx->fpriv;
	if (!ctx->tmz)
		return;

	mutex_lock(&fpriv->lock);
	ctx->tmz = false;
	amdgpu_tmz_deinit_ctx(adev, ctx);
	amdgpu_tmz_deinit_res(adev, fpriv);
	mutex_unlock(&fpriv->lock);
}

int amdgpu_tmz_unmap_queue(struct amdgpu_ring *ring)
{
	int r;

	struct amdgpu_device *adev = ring->adev;

	if (ring->sws_ctx.queue_state != AMDGPU_SWS_QUEUE_ENABLED)
		return 0;

	r = adev->gfx.mec.unmap_priv_queue(ring,
					   AMDGPU_CP_HQD_DEQUEUE_MODE_SDD);
	if (r)
		ring->sws_ctx.queue_state = AMDGPU_SWS_QUEUE_DISABLED;
	else
		ring->sws_ctx.queue_state = AMDGPU_SWS_QUEUE_DEQUEUED;

	return r;
}

int amdgpu_tmz_map_queue(struct amdgpu_ring *ring)
{
	int r;
	struct amdgpu_device *adev = ring->adev;

	if (ring->sws_ctx.queue_state == AMDGPU_SWS_QUEUE_ENABLED)
		return 0;

	if (ring->sws_ctx.queue_state == AMDGPU_SWS_QUEUE_DISABLED) {
		//init mqd for the first mapping
		r = amdgpu_ring_compute_mqd_init(ring);
		if (r) {
			DRM_ERROR("failed to init mqd for tmz\n");
			return r;
		}
	} else {
		r = amdgpu_ring_compute_mqd_update(ring);
		if (r) {
			DRM_ERROR("failed to update mqd for tmz\n");
			return r;
		}
	}

	r = adev->gfx.mec.map_priv_queue(ring);
	if (r) {
		DRM_ERROR("failed to map queue for tmz\n");

		return r;
	}

	ring->sws_ctx.queue_state = AMDGPU_SWS_QUEUE_ENABLED;

	return r;
}
