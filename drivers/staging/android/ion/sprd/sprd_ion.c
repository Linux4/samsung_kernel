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

#include <linux/export.h>
#include <linux/err.h>
#include "../ion.h"
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <video/ion_sprd.h>
#include "../ion_priv.h"

#include <asm/cacheflush.h>

#include "sprd_fence.h"
#include <linux/dma-buf.h>

struct ion_device *idev;
int num_heaps = 0;
struct ion_heap **heaps;

static unsigned long user_va2pa(struct mm_struct *mm, unsigned long addr)
{
	pgd_t *pgd = pgd_offset(mm, addr);
	unsigned long pa = 0;
	
	if (!pgd_none(*pgd)) {
		pud_t *pud = pud_offset(pgd, addr);
		if (!pud_none(*pud)) {
			pmd_t *pmd = pmd_offset(pud, addr);
			if (!pmd_none(*pmd)) {
				pte_t *ptep, pte;
				
				ptep = pte_offset_map(pmd, addr);
				pte = *ptep;
				if (pte_present(pte))
					pa = pte_val(pte) & PAGE_MASK;
				pte_unmap(ptep);
			}
		}
	}
	
	return pa;
}

static int get_iommu_id(int master_id, int *iommu_id)
{
	int ret = 0;

	switch (master_id) {
	case ION_GSP:
		*iommu_id = IOMMU_GSP;
		break;
	case ION_MM:
		*iommu_id = IOMMU_MM;
		break;
	case ION_VSP:
		*iommu_id = IOMMU_VSP;
		break;
	case ION_DCAM:
		*iommu_id = IOMMU_DCAM;
		break;
	case ION_DISPC:
		*iommu_id = IOMMU_DISPC;
		break;
	case ION_GSP0:
		*iommu_id = IOMMU_GSP0;
		break;
	case ION_GSP1:
		*iommu_id = IOMMU_GSP1;
		break;
	case ION_VPP:
		*iommu_id = IOMMU_VPP;
		break;
	default:
		ret = -1;
		pr_err("%s, unknow master id, return error!\n", __func__);
		break;
	}

	return ret;
}

struct ion_client *sprd_ion_client_create(const char *name)
{
	return ion_client_create(idev, name);
}
EXPORT_SYMBOL(sprd_ion_client_create);

int sprd_ion_get_gsp_addr(struct ion_addr_data *data)
{
	int ret = 0;
	struct dma_buf *dmabuf;
	struct ion_buffer *buffer;

	dmabuf = dma_buf_get(data->fd_buffer);
	if (IS_ERR(dmabuf)) {
		pr_err("sprd_ion_get_gsp_addr() dmabuf=0x%lx dma_buf_get error!\n",
			(unsigned long)dmabuf);
		return -1;
	}
	/* if this memory came from ion */
#if 0
	if (dmabuf->ops != &dma_buf_ops) {
		pr_err("%s: can not import dmabuf from another exporter\n",
		       __func__);
		dma_buf_put(dmabuf);
		return ERR_PTR(-EINVAL);
	}
#endif
	buffer = dmabuf->priv;
	dma_buf_put(dmabuf);

	if (ION_HEAP_TYPE_SYSTEM == buffer->heap->type) {
#if defined(CONFIG_SPRD_IOMMU)
		mutex_lock(&buffer->lock);
		if(0 == buffer->iomap_cnt[IOMMU_GSP]) {
			buffer->iova[IOMMU_GSP] = sprd_iova_alloc(IOMMU_GSP, buffer->size);
			ret = sprd_iova_map(IOMMU_GSP, buffer->iova[IOMMU_GSP],
							buffer->size, buffer->sg_table);
		}
		if (ret) {
			pr_err("%s, sprd_iova_map error, iova: 0x%lx, ret: %d!\n",
				__func__, buffer->iova[IOMMU_GSP], ret);
			sprd_iova_free(IOMMU_GSP, buffer->iova[IOMMU_GSP], buffer->size);
			buffer->iova[IOMMU_GSP] = 0;
			data->iova_addr = 0;
			data->size = 0;
		} else {
			buffer->iomap_cnt[IOMMU_GSP]++;
			data->iova_addr = buffer->iova[IOMMU_GSP];
			data->size = buffer->size;
		}
		data->iova_enabled = true;
		mutex_unlock(&buffer->lock);
#else
		ret = -1;
#endif
	} else {
		if (!buffer->heap->ops->phys) {
			pr_err("%s: ion_phys is not implemented by this heap.\n",
			       __func__);
			return -ENODEV;
		}
		ret = buffer->heap->ops->phys(buffer->heap, buffer, &(data->phys_addr),
								&(data->size));
		data->iova_enabled = false;
		if (ret) {
			pr_err("%s, get phys error %d!\n", __func__, ret);
		}
	}

	return ret;
}
EXPORT_SYMBOL(sprd_ion_get_gsp_addr);

int sprd_ion_free_gsp_addr(int fd)
{
	int ret = 0;
	struct dma_buf *dmabuf;
	struct ion_buffer *buffer;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf)) {
		pr_err("sprd_ion_free_gsp_addr() dmabuf=0x%lx dma_buf_get error!\n",
			(unsigned long)dmabuf);
		return -1;
	}
	/* if this memory came from ion */
#if 0
	if (dmabuf->ops != &dma_buf_ops) {
		pr_err("%s: can not import dmabuf from another exporter\n",
		       __func__);
		dma_buf_put(dmabuf);
		return ERR_PTR(-EINVAL);
	}
#endif
	buffer = dmabuf->priv;
	dma_buf_put(dmabuf);

	if (ION_HEAP_TYPE_SYSTEM == buffer->heap->type) {
#if defined(CONFIG_SPRD_IOMMU)
		mutex_lock(&buffer->lock);
		if (buffer->iomap_cnt[IOMMU_GSP] > 0) {
			buffer->iomap_cnt[IOMMU_GSP]--;
			if(0 == buffer->iomap_cnt[IOMMU_GSP]) {
				ret = sprd_iova_unmap(IOMMU_GSP, buffer->iova[IOMMU_GSP],
									buffer->size);
				sprd_iova_free(IOMMU_GSP, buffer->iova[IOMMU_GSP],
							buffer->size);
				buffer->iova[IOMMU_GSP] = 0;
			}
		}
		mutex_unlock(&buffer->lock);
#else
		ret = -1;
#endif
	}

	if (ret) {
		pr_err("sprd_ion_free_gsp_addr, error %d!\n",ret);
	}

	return ret;
}
EXPORT_SYMBOL(sprd_ion_free_gsp_addr);

long sprd_ion_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	struct ion_client *client = filp->private_data;
	int ret = 0;

	switch (cmd) {
	case ION_SPRD_CUSTOM_PHYS:
	{
		struct ion_phys_data data;
		struct ion_handle *handle;

		if (copy_from_user(&data, (void __user *)arg,
				sizeof(data))) {
			pr_err("sprd_heap_ioctl alloc copy_from_user error!\n");
			return -EFAULT;
		}

		handle = ion_import_dma_buf(client, data.fd_buffer);

		if (IS_ERR(handle)) {
			pr_err("sprd_heap_ioctl alloc handle=0x%lx error!\n",
				(unsigned long)handle);
			return PTR_ERR(handle);
		}

		ret = ion_phys(client, handle, &data.phys, &data.size);
		ion_free(client, handle);

		if (ret) {
			pr_err("sprd_heap_ioctl alloc ret=0x%x error!\n",ret);
			return ret;
		}

		if (copy_to_user((void __user *)arg,
				&data, sizeof(data))) {
			pr_err("sprd_heap_ioctl alloc copy_to_user error!\n");
			return -EFAULT;
		}

		pr_debug("sprd_heap_ioctl alloc paddress=0x%lx size=0x%zx\n",
			data.phys,data.size);
		break;
	}
	case ION_SPRD_CUSTOM_MSYNC:
	{
		struct ion_msync_data data;
		void *v_addr;

		if (copy_from_user(&data, (void __user *)arg,
				sizeof(data))) {
			pr_err("sprd_heap_ioctl free copy_from_user error!\n");
			return -EFAULT;
		}

		if ((unsigned long)data.vaddr & (PAGE_SIZE - 1)) {
			pr_err("sprd_heap_ioctl free data.vaddr=%p error!\n",data.vaddr);
			return -EFAULT;
		}

		pr_debug("sprd_heap_ioctl free vaddress=%p paddress=%p size=0x%zx\n",
			data.vaddr,data.paddr,data.size);
#ifdef CONFIG_64BIT
		__dma_flush_range(data.vaddr, data.vaddr + data.size);
#else
		dmac_flush_range(data.vaddr, data.vaddr + data.size);
#endif
		v_addr = data.vaddr;
		while (v_addr < data.vaddr + data.size) {
			unsigned long phy_addr = user_va2pa(current->mm,
										(unsigned long)v_addr);
			if (phy_addr) {
#ifdef CONFIG_64BIT
				/* not implement in arm64 now */
#else
				outer_flush_range(phy_addr, phy_addr + PAGE_SIZE);
#endif
			}
			v_addr += PAGE_SIZE;
		}
		break;
	}
#if defined(CONFIG_SPRD_IOMMU)
	case ION_SPRD_CUSTOM_MAP:
	{
		struct ion_mmu_data data;
		struct ion_handle *handle;
		struct ion_buffer *buffer;
		int iommu_id;

		if (copy_from_user(&data, (void __user *)arg,
				sizeof(data))) {
			pr_err("%s, iommu_id[%d] map copy_from_user error!\n",
					__func__, iommu_id);
			return -EFAULT;
		}

		if (get_iommu_id(data.master_id, &iommu_id)) {
			return -EINVAL;
		}

		handle = ion_import_dma_buf(client, data.fd_buffer);

		if (IS_ERR(handle)) {
			pr_err("%s, iommu_id[%d] map handle=0x%lx error!\n",
					__func__, iommu_id, (unsigned long)handle);
			return PTR_ERR(handle);
		}

		buffer = ion_handle_buffer(handle);

		mutex_lock(&buffer->lock);
		if (0 == buffer->iomap_cnt[iommu_id]) {
			buffer->iova[iommu_id] = sprd_iova_alloc(iommu_id, buffer->size);
			ret = sprd_iova_map(iommu_id, buffer->iova[iommu_id], buffer->size,
							buffer->sg_table);
		}
		if (ret) {
			pr_err("%s, id[%d] sprd_iova_map error, iova: 0x%lx, ret: %d!\n",
					__func__, iommu_id, buffer->iova[iommu_id], ret);
			sprd_iova_free(iommu_id, buffer->iova[iommu_id], buffer->size);
			buffer->iova[iommu_id] = 0;
			data.iova_addr = 0;
			data.iova_size = 0;
		} else {
			buffer->iomap_cnt[iommu_id]++;
			data.iova_addr = buffer->iova[iommu_id];
			data.iova_size = buffer->size;
		}
		mutex_unlock(&buffer->lock);
		ion_free(client, handle);

		if (ret) {
			pr_err("%s, iommu_id[%d] sprd_iova_map error %d!\n",
					__func__, iommu_id, ret);
			return ret;
		}

		if (copy_to_user((void __user *)arg,
				&data, sizeof(data))) {
			pr_err("%s, iommu_id[%d] map copy_to_user error!\n",
					__func__, iommu_id);
			sprd_iova_free(iommu_id, data.iova_addr, data.iova_size);
			return -EFAULT;
		}
		break;
	}
	case ION_SPRD_CUSTOM_UNMAP:
	{
		struct ion_mmu_data data;
		struct ion_handle *handle;
		struct ion_buffer *buffer;
		int iommu_id;

		if (copy_from_user(&data, (void __user *)arg,
				sizeof(data))) {
			pr_err("%s, iommu_id[%d] unmap copy_from_user error!\n",
					__func__, iommu_id);
			return -EFAULT;
		}

		if (get_iommu_id(data.master_id, &iommu_id)) {
			return -EINVAL;
		}

		handle = ion_import_dma_buf(client, data.fd_buffer);

		if (IS_ERR(handle)) {
			pr_err("%s, iommu_id[%d] unmap handle=0x%lx error!\n",
					__func__, iommu_id, (unsigned long)handle);
			return PTR_ERR(handle);
		}

		buffer = ion_handle_buffer(handle);

		mutex_lock(&buffer->lock);
		if (buffer->iomap_cnt[iommu_id] > 0) {
			buffer->iomap_cnt[iommu_id]--;
			if (0 == buffer->iomap_cnt[iommu_id]) {
				ret = sprd_iova_unmap(iommu_id, buffer->iova[iommu_id],
								buffer->size);
				sprd_iova_free(iommu_id, buffer->iova[iommu_id], buffer->size);
				buffer->iova[iommu_id] = 0;
			}
		}
		mutex_unlock(&buffer->lock);
		data.iova_addr = 0;
		data.iova_size = 0;
		ion_free(client, handle);
		if (ret) {
			pr_err("%s, iommu_id[%d] sprd_iova_unmap error %d!\n",
					__func__, iommu_id, ret);
			return ret;
		}

		if (copy_to_user((void __user *)arg,
				&data, sizeof(data))) {
			pr_err("%s, iommu_id[%d] unmap copy_to_user error!\n",
					__func__, iommu_id);
			return -EFAULT;
		}
		break;
	}
#endif
	case ION_SPRD_CUSTOM_FENCE_CREATE:
	{
		int ret = -1;
		struct ion_fence_data data;

		if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
			pr_err("FENCE_CREATE user data is err\n");
			return -EFAULT;
		}

		ret = sprd_fence_build(&data);
		if (ret != 0) {
			pr_err("sprd_fence_build failed\n");
			return -EFAULT;
		}

		if (copy_to_user((void __user *)arg, &data, sizeof(data))) {
			sprd_fence_destroy(&data);
			pr_err("copy_to_user fence failed\n");
			return -EFAULT;
		}

		break;
    }
	case ION_SPRD_CUSTOM_FENCE_SIGNAL:
	{
		struct ion_fence_data data;

		if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
			pr_err("FENCE_CREATE user data is err\n");
			return -EFAULT;
		}

		sprd_fence_signal(&data);

		break;
	}
	case ION_SPRD_CUSTOM_FENCE_DUP:
	{
		break;

	}
	default:
		pr_err("sprd_ion Do not support cmd: %d\n", cmd);
		return -ENOTTY;
	}

	return ret;
}
EXPORT_SYMBOL(sprd_ion_ioctl);

static long sprd_heap_ioctl(struct ion_client *client, unsigned int cmd,
				unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
	case ION_SPRD_CUSTOM_PHYS:
	{
		struct ion_phys_data data;
		struct ion_handle *handle;

		if (copy_from_user(&data, (void __user *)arg,
				sizeof(data))) {
			pr_err("sprd_heap_ioctl alloc copy_from_user error!\n");
			return -EFAULT;
		}

		handle = ion_import_dma_buf(client, data.fd_buffer);

		if (IS_ERR(handle)) {
			pr_err("sprd_heap_ioctl alloc handle=0x%lx error!\n",
				(unsigned long)handle);
			return PTR_ERR(handle);
		}

		ret = ion_phys(client, handle, &data.phys, &data.size);
		ion_free(client, handle);

		if (ret) {
			pr_err("sprd_heap_ioctl alloc ret=0x%x error!\n",ret);
			return ret;
		}

		if (copy_to_user((void __user *)arg,
				&data, sizeof(data))) {
			pr_err("sprd_heap_ioctl alloc copy_to_user error!\n");
			return -EFAULT;
		}

		pr_debug("sprd_heap_ioctl alloc paddress=0x%lx size=0x%zx\n",
			data.phys,data.size);
		break;
	}
	case ION_SPRD_CUSTOM_MSYNC:
	{
		struct ion_msync_data data;
		void *v_addr;

		if (copy_from_user(&data, (void __user *)arg,
				sizeof(data))) {
			pr_err("sprd_heap_ioctl free copy_from_user error!\n");
			return -EFAULT;
		}

		if ((unsigned long)data.vaddr & (PAGE_SIZE - 1)) {
			pr_err("sprd_heap_ioctl free data.vaddr=%p error!\n",data.vaddr);
			return -EFAULT;
		}

		pr_debug("sprd_heap_ioctl free vaddress=%p paddress=%p size=0x%zx\n",
			data.vaddr,data.paddr,data.size);
#ifdef CONFIG_64BIT
		__dma_flush_range(data.vaddr, data.vaddr + data.size);
#else
		dmac_flush_range(data.vaddr, data.vaddr + data.size);
#endif
		v_addr = data.vaddr;
		while (v_addr < data.vaddr + data.size) {
			unsigned long phy_addr = user_va2pa(current->mm,
										(unsigned long)v_addr);
			if (phy_addr) {
#ifdef CONFIG_64BIT
				/* not implement in arm64 now */
#else
				outer_flush_range(phy_addr, phy_addr + PAGE_SIZE);
#endif
			}
			v_addr += PAGE_SIZE;
		}
		break;
	}
#if defined(CONFIG_SPRD_IOMMU)
	case ION_SPRD_CUSTOM_MAP:
	{
		struct ion_mmu_data data;
		struct ion_handle *handle;
		struct ion_buffer *buffer;
		int iommu_id;

		if (copy_from_user(&data, (void __user *)arg,
				sizeof(data))) {
			pr_err("%s, iommu_id[%d] map copy_from_user error!\n",
					__func__, iommu_id);
			return -EFAULT;
		}

		if (get_iommu_id(data.master_id, &iommu_id)) {
			return -EINVAL;
		}

		handle = ion_import_dma_buf(client, data.fd_buffer);

		if (IS_ERR(handle)) {
			pr_err("%s, iommu_id[%d] map handle=0x%lx error!\n",
					__func__, iommu_id, (unsigned long)handle);
			return PTR_ERR(handle);
		}

		buffer = ion_handle_buffer(handle);

		mutex_lock(&buffer->lock);
		if (0 == buffer->iomap_cnt[iommu_id]) {
			buffer->iova[iommu_id] = sprd_iova_alloc(iommu_id, buffer->size);
			ret = sprd_iova_map(iommu_id, buffer->iova[iommu_id], buffer->size,
							buffer->sg_table);
		}
		if (ret) {
			pr_err("%s, id[%d] sprd_iova_map error, iova: 0x%lx, ret: %d!\n",
					__func__, iommu_id, buffer->iova[iommu_id], ret);
			sprd_iova_free(iommu_id, buffer->iova[iommu_id], buffer->size);
			buffer->iova[iommu_id] = 0;
			data.iova_addr = 0;
			data.iova_size = 0;
		} else {
			buffer->iomap_cnt[iommu_id]++;
			data.iova_addr = buffer->iova[iommu_id];
			data.iova_size = buffer->size;
		}
		mutex_unlock(&buffer->lock);
		ion_free(client, handle);

		if (ret) {
			pr_err("%s, iommu_id[%d] sprd_iova_map error %d!\n",
					__func__, iommu_id, ret);
			return ret;
		}

		if (copy_to_user((void __user *)arg,
				&data, sizeof(data))) {
			pr_err("%s, iommu_id[%d] map copy_to_user error!\n",
					__func__, iommu_id);
			sprd_iova_free(iommu_id, data.iova_addr, data.iova_size);
			return -EFAULT;
		}
		break;
	}
	case ION_SPRD_CUSTOM_UNMAP:
	{
		struct ion_mmu_data data;
		struct ion_handle *handle;
		struct ion_buffer *buffer;
		int iommu_id;

		if (copy_from_user(&data, (void __user *)arg,
				sizeof(data))) {
			pr_err("%s, iommu_id[%d] unmap copy_from_user error!\n",
					__func__, iommu_id);
			return -EFAULT;
		}

		if (get_iommu_id(data.master_id, &iommu_id)) {
			return -EINVAL;
		}

		handle = ion_import_dma_buf(client, data.fd_buffer);

		if (IS_ERR(handle)) {
			pr_err("%s, iommu_id[%d] unmap handle=0x%lx error!\n",
					__func__, iommu_id, (unsigned long)handle);
			return PTR_ERR(handle);
		}

		buffer = ion_handle_buffer(handle);

		mutex_lock(&buffer->lock);
		if (buffer->iomap_cnt[iommu_id] > 0) {
			buffer->iomap_cnt[iommu_id]--;
			if (0 == buffer->iomap_cnt[iommu_id]) {
				ret = sprd_iova_unmap(iommu_id, buffer->iova[iommu_id],
								buffer->size);
				sprd_iova_free(iommu_id, buffer->iova[iommu_id], buffer->size);
				buffer->iova[iommu_id] = 0;
			}
		}
		mutex_unlock(&buffer->lock);
		data.iova_addr = 0;
		data.iova_size = 0;
		ion_free(client, handle);
		if (ret) {
			pr_err("%s, iommu_id[%d] sprd_iova_unmap error %d!\n",
					__func__, iommu_id, ret);
			return ret;
		}

		if (copy_to_user((void __user *)arg,
				&data, sizeof(data))) {
			pr_err("%s, iommu_id[%d] unmap copy_to_user error!\n",
					__func__, iommu_id);
			return -EFAULT;
		}
		break;
	}
#endif
	case ION_SPRD_CUSTOM_FENCE_CREATE:
	{
		int ret = -1;
		struct ion_fence_data data;
		
		if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
			pr_err("FENCE_CREATE user data is err\n");
			return -EFAULT;
		}
		
		ret = sprd_fence_build(&data);
		if (ret != 0) {
			pr_err("sprd_fence_build failed\n");
			return -EFAULT;
		}
		
		if (copy_to_user((void __user *)arg, &data, sizeof(data))) {
			sprd_fence_destroy(&data);
			pr_err("copy_to_user fence failed\n");
			return -EFAULT;
		}
		
		break;
    }
	case ION_SPRD_CUSTOM_FENCE_SIGNAL:
	{
		struct ion_fence_data data;

		if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
			pr_err("FENCE_CREATE user data is err\n");
			return -EFAULT;
		}

		sprd_fence_signal(&data);
		
		break;
	}
	case ION_SPRD_CUSTOM_FENCE_DUP:
	{
		break;
		
	}
	default:
		pr_err("sprd_ion Do not support cmd: %d\n", cmd);
		return -ENOTTY;
	}

	return ret;
}


extern struct ion_heap *ion_cma_heap_create(
								struct ion_platform_heap *heap_data,
								struct device *dev);

extern void ion_cma_heap_destroy(struct ion_heap *heap);


static struct ion_heap *__ion_heap_create(struct ion_platform_heap *heap_data,
								struct device *dev)
{
	struct ion_heap *heap = NULL;

	switch ((int)heap_data->type) {
	case ION_HEAP_TYPE_CUSTOM:
		heap = ion_cma_heap_create(heap_data, dev);
		break;
	default:
		return ion_heap_create(heap_data);
	}

	if (IS_ERR_OR_NULL(heap)) {
		pr_err("%s: error creating heap %s type %d base %lu size %zd\n",
		       __func__, heap_data->name, heap_data->type,
		       heap_data->base, heap_data->size);
		return ERR_PTR(-EINVAL);
	}

	heap->name = heap_data->name;
	heap->id = heap_data->id;

	return heap;
}

static void __ion_heap_destroy(struct ion_heap *heap)
{
	if (!heap)
		return;

	switch ((int)heap->type) {
	case ION_HEAP_TYPE_CUSTOM:
		ion_cma_heap_destroy(heap);
		break;
	default:
		ion_heap_destroy(heap);
	}
}

static struct ion_platform_data *sprd_ion_parse_dt(struct platform_device *pdev)
{
	int i = 0, ret = 0;
	const struct device_node *parent = pdev->dev.of_node;
	struct device_node *child = NULL;
	struct ion_platform_data *pdata = NULL;
	struct ion_platform_heap *ion_heaps = NULL;
	struct platform_device *new_dev = NULL;
	uint32_t val = 0, type = 0;
	const char *name;
	uint32_t out_values[2];

	for_each_child_of_node(parent, child)
		num_heaps++;
	if (!num_heaps)
		return ERR_PTR(-EINVAL);

	pr_info("%s: num_heaps=%d\n", __func__, num_heaps);

	pdata = kzalloc(sizeof(struct ion_platform_data), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	ion_heaps = kzalloc(sizeof(struct ion_platform_heap)*num_heaps,
				GFP_KERNEL);
	if (!ion_heaps) {
		kfree(pdata);
		return ERR_PTR(-ENOMEM);
	}

	pdata->heaps = ion_heaps;
	pdata->nr = num_heaps;

	for_each_child_of_node(parent, child) {
		new_dev = of_platform_device_create(child, NULL, &pdev->dev);
		if (!new_dev) {
			pr_err("Failed to create device %s\n", child->name);
			goto out;
		}

		pdata->heaps[i].priv = &new_dev->dev;

		ret = of_property_read_u32(child, "reg", &val);
		if (ret) {
			pr_err("%s: Unable to find reg key, ret=%d", __func__, ret);
			goto out;
		}
		pdata->heaps[i].id = val;

		ret = of_property_read_string(child, "reg-names", &name);
		if (ret) {
			pr_err("%s: Unable to find reg-names key, ret=%d", __func__, ret);
			goto out;
		}
		pdata->heaps[i].name = name;

		ret = of_property_read_u32(child, "sprd,ion-heap-type", &type);
		if (ret) {
			pr_err("%s: Unable to find ion-heap-type key, ret=%d",
				__func__, ret);
			goto out;
		}
		pdata->heaps[i].type = type;

		ret = of_property_read_u32_array(child, "sprd,ion-heap-mem",
				out_values, 2);
		if (!ret) {
			pdata->heaps[i].base = out_values[0];
			pdata->heaps[i].size = out_values[1];
		}

		pr_info("%s: heaps[%d]: %s type: %d base: %lu size %zd\n",
				__func__, i, pdata->heaps[i].name, pdata->heaps[i].type,
				pdata->heaps[i].base, pdata->heaps[i].size);
		++i;
	}
	return pdata;
out:
	kfree(pdata->heaps);
	kfree(pdata);
	return ERR_PTR(ret);
}

int sprd_ion_probe(struct platform_device *pdev)
{
	int i = 0, ret = -1;
	struct ion_platform_data *pdata = NULL;
	uint32_t need_free_pdata;

	if(pdev->dev.of_node) {
		pdata = sprd_ion_parse_dt(pdev);
		if (IS_ERR(pdata)) {
			return PTR_ERR(pdata);
		}
		need_free_pdata = 1;
	} else {
		pdata = pdev->dev.platform_data;

		if (!pdata) {
			pr_err("sprd_ion_probe failed: No platform data!\n");
			return -ENODEV;
		}

		num_heaps = pdata->nr;
		if (!num_heaps)
			return -EINVAL;
		need_free_pdata = 0;
	}

	heaps = kzalloc(sizeof(struct ion_heap *) * pdata->nr, GFP_KERNEL);
	if(!heaps) {
		ret = -ENOMEM;
		goto out1;
	}

	idev = ion_device_create(&sprd_heap_ioctl);
	if (IS_ERR_OR_NULL(idev)) {
		pr_err("%s,idev is null\n", __FUNCTION__);
		kfree(heaps);
		ret = PTR_ERR(idev);
		goto out1;
	}

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &pdata->heaps[i];

		if(!pdev->dev.of_node) {
			heap_data->priv = &pdev->dev;
		}
		heaps[i] = __ion_heap_create(heap_data, &pdev->dev);
		if (IS_ERR_OR_NULL(heaps[i])) {
			pr_err("%s,heaps is null, i:%d\n", __FUNCTION__,i);
			ret = PTR_ERR(heaps[i]);
			goto out;
		}
		ion_device_add_heap(idev, heaps[i]);
	}
	platform_set_drvdata(pdev, idev);

	ret = open_sprd_sync_timeline();
	if (ret != 0) {
		pr_err("%s: sprd_create_timeline failed\n", __func__);
		goto out;
        }

        if(need_free_pdata) {
		kfree(pdata->heaps);
		kfree(pdata);
	}
	return 0;
out:
	for (i = 0; i < num_heaps; i++) {
		if (heaps[i])
			ion_heap_destroy(heaps[i]);
	}
	kfree(heaps);
out1:
	if(need_free_pdata) {
		kfree(pdata->heaps);
		kfree(pdata);
	}
	return ret;
}

int sprd_ion_remove(struct platform_device *pdev)
{
	struct ion_device *idev = platform_get_drvdata(pdev);
	int i;

	ion_device_destroy(idev);
	for (i = 0; i < num_heaps; i++)
		__ion_heap_destroy(heaps[i]);
	kfree(heaps);
	
	close_sprd_sync_timeline();

	return 0;
}

static const struct of_device_id sprd_ion_ids[] = {
	{ .compatible = "sprd,ion-sprd"},
	{},
};

static struct platform_driver ion_driver = {
	.probe = sprd_ion_probe,
	.remove = sprd_ion_remove,
	.driver = {
		.name = "ion-sprd" ,
		.of_match_table = of_match_ptr(sprd_ion_ids),
	}
};

#ifdef CONFIG_E_SHOW_MEM
static int ion_e_show_mem_handler(struct notifier_block *nb,
				unsigned long val, void *data)
{
	int i;
	enum e_show_mem_type type = (enum e_show_mem_type)val;

	printk("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	printk("Enhanced Mem-info :ION\n");
	for (i = 0; i < num_heaps; i++) {
		if ((E_SHOW_MEM_BASIC != type)
			|| (ION_HEAP_TYPE_SYSTEM == heaps[i]->type))
		ion_debug_heap_show_printk(heaps[i], type);
	}
	return 0;
}

static struct notifier_block ion_e_show_mem_notifier = {
	.notifier_call = ion_e_show_mem_handler,
};
#endif

static int __init ion_init(void)
{
	int result;
	result= platform_driver_register(&ion_driver);
	pr_info("%s,result:%d\n",__FUNCTION__,result);
#ifdef CONFIG_E_SHOW_MEM
	register_e_show_mem_notifier(&ion_e_show_mem_notifier);
#endif
	return result;
}

static void __exit ion_exit(void)
{
	platform_driver_unregister(&ion_driver);
#ifdef CONFIG_E_SHOW_MEM
	unregister_e_show_mem_notifier(&ion_e_show_mem_notifier);
#endif
}

module_init(ion_init);
module_exit(ion_exit);

