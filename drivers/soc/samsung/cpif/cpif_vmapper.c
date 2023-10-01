// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Samsung Electronics.
 *
 */

#include "cpif_vmapper.h"
#include <linux/dma-direction.h>
#include <linux/mm.h>
#include <soc/samsung/exynos-cpif-iommu.h>
#include "modem_v1.h"

struct cpif_va_mapper *cpif_vmap_create(u64 va_start, u64 va_size, u64 instance_size,
					u32 num_item)
{
	struct cpif_va_mapper *vmap;

	vmap = kvzalloc(sizeof(struct cpif_va_mapper), GFP_KERNEL);
	if (vmap == NULL)
		return NULL;

	vmap->va_start = va_start;
	vmap->va_size = va_size;
	vmap->va_end = va_start + va_size;
	vmap->instance_size = instance_size;
	vmap->item_arr_size = num_item;

	if (!num_item) /* no need to use item list */
		goto skip_item_arr;
	vmap->item_arr = kvzalloc(sizeof(struct cpif_vmap_item) * num_item, GFP_KERNEL);
	if (vmap->item_arr == NULL) {
		kvfree(vmap);
		return NULL;
	}

skip_item_arr:
	cpif_sysmmu_set_use_iocc();
	cpif_sysmmu_enable();

	return vmap;
}
EXPORT_SYMBOL(cpif_vmap_create);

void cpif_vmap_init(struct cpif_va_mapper *vmap)
{
	if (unlikely(!vmap)) {
		mif_err("no vmap to initialize\n");
		return;
	}

	if (!vmap->item_arr) {
		/* when va and pa is mapped at once */
		if (unlikely(!cpif_iommu_unmap(vmap->va_start, vmap->va_size)))
			mif_err("failed to perform iommu unmapping\n");
	} else if (vmap->item_arr[vmap->in_idx].vaddr_base) { /* item_list not empty */
		u32 idx = vmap->out_idx;

		while (idx != vmap->in_idx) {
			cpif_iommu_unmap(vmap->item_arr[idx].vaddr_base,
						vmap->item_arr[idx].item_size);
			if (++idx == vmap->item_arr_size)
				idx = 0;
		}
		cpif_iommu_unmap(vmap->item_arr[idx].vaddr_base,
				vmap->item_arr[idx].item_size);

		vmap->out_idx = 0;
		vmap->in_idx = 0;
		memset(vmap->item_arr, 0, sizeof(struct cpif_vmap_item) * vmap->item_arr_size);
	}
}
EXPORT_SYMBOL(cpif_vmap_init);

void cpif_vmap_free(struct cpif_va_mapper *vmap)
{
	if (unlikely(!vmap)) {
		mif_err("no vmap to free\n");
		return;
	}

	cpif_vmap_init(vmap);

	kfree(vmap->item_arr);
	kvfree(vmap);
	vmap = NULL;
}
EXPORT_SYMBOL(cpif_vmap_free);

u64 cpif_vmap_map_area(struct cpif_va_mapper *vmap, u64 item_paddr, u64 item_size,
			u64 instance_paddr)
{
	int err;
	struct cpif_vmap_item *in_item;

	if (!vmap->item_arr) { /* when va and pa is mapped at once */
		err = cpif_iommu_map(vmap->va_start, instance_paddr, vmap->va_size,
					DMA_BIDIRECTIONAL);
		if (unlikely(err)) {
			mif_err("failed to perform iommu mapping\n");
			return 0;
		}
		return vmap->va_start;
	}

	in_item = &vmap->item_arr[vmap->in_idx];
	if (in_item->vaddr_base == 0) {/* first time to map */
		err = cpif_iommu_map(vmap->va_start, item_paddr, item_size,
					DMA_BIDIRECTIONAL);
		if (unlikely(err)) {
			mif_err_limited("failed to perform iommu mapping\n");
			return 0;
		}
		in_item->vaddr_base = vmap->va_start;
		in_item->paddr_base = item_paddr;
		in_item->item_size = item_size;
		atomic_set(&in_item->ref, 1);
	} else if (in_item->paddr_base != item_paddr) {
		/* normal case
		 * if in's vmap item is fully mapped, enqueue that item to
		 * item_list and create new item
		 */
		u64 next_vaddr_base = in_item->vaddr_base + in_item->item_size;

		if ((next_vaddr_base + item_size) >= vmap->va_end) /* back to va start */
			next_vaddr_base = vmap->va_start;

		err = cpif_iommu_map(next_vaddr_base, item_paddr, item_size,
					DMA_BIDIRECTIONAL);

		if (unlikely(err)) {
			mif_err_limited("failed to perform iommu mapping\n");
			return 0;
		}
		vmap->in_idx++;
		if (vmap->in_idx == vmap->item_arr_size)
			vmap->in_idx = 0;
		in_item = &vmap->item_arr[vmap->in_idx];
		in_item->vaddr_base = next_vaddr_base;
		in_item->paddr_base = item_paddr;
		in_item->item_size = item_size;
		atomic_set(&in_item->ref, 1);
	} else /* item "in" still has room to use, no need to iommu this time */
		atomic_inc(&in_item->ref);

	return in_item->vaddr_base + (instance_paddr - item_paddr);
}
EXPORT_SYMBOL(cpif_vmap_map_area);

u64 cpif_vmap_unmap_area(struct cpif_va_mapper *vmap, u64 vaddr)
{
	int err = 0;
	u64 ret = 0;
	struct cpif_vmap_item *out_item;

	if (!vmap->item_arr) { /* when va and pa is mapped at once */
		err = cpif_iommu_unmap(vmap->va_start, vmap->va_size);
		if (unlikely(err == 0)) {
			mif_err_limited("failed to perform iommu unmapping\n");
			return 0;
		}
		return vmap->va_start;
	}

	out_item = &vmap->item_arr[vmap->out_idx];

	atomic_dec(&out_item->ref);

	ret = out_item->paddr_base + (vaddr - out_item->vaddr_base);

	/* unmap this item when ref count goes to 0 */
	if (atomic_read(&out_item->ref) == 0) {
		err = cpif_iommu_unmap(out_item->vaddr_base, out_item->item_size);
		if (err == 0) {
			mif_err_limited("failed to unmap\n");
			return 0;
		}
		vmap->out_idx++;
		if (vmap->out_idx == vmap->item_arr_size)
			vmap->out_idx = 0;
	}

	return ret;
}
EXPORT_SYMBOL(cpif_vmap_unmap_area);
