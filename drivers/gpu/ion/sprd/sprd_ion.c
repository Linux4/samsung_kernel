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
#include <linux/ion.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <video/ion_sprd.h>
#include "../ion_priv.h"

#include <asm/cacheflush.h>

#include "sprd_fence.h"

struct ion_device *idev;
int num_heaps;
struct ion_heap **heaps;

struct fence_sync sprd_fence;
struct sync_fence *current_fence = NULL;

#if 1
static uint32_t user_va2pa(struct mm_struct *mm, uint32_t addr)
{
	pgd_t *pgd = pgd_offset(mm, addr);
	uint32_t pa = 0;
	
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
#endif

struct ion_client *sprd_ion_client_create(const char *name)
{
	return ion_client_create(idev,name);
}
EXPORT_SYMBOL(sprd_ion_client_create);

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

		if (IS_ERR(handle)){
			pr_err("sprd_heap_ioctl alloc handle=0x%x error!\n", (unsigned long)handle);
			return PTR_ERR(handle);
		}

		ret = ion_phys(client, handle, &data.phys, &data.size);
		ion_free(client, handle);
		
		if (ret){
			pr_err("sprd_heap_ioctl alloc ret=0x%x error!\n",ret);
			return ret;
		}
		
		if (copy_to_user((void __user *)arg,
				&data, sizeof(data))) {
			pr_err("sprd_heap_ioctl alloc copy_to_user error!\n");
			return -EFAULT;
		}

		pr_debug("sprd_heap_ioctl alloc paddress=0x%x size=0x%x\n",data.phys,data.size);
		break;
	}
	case ION_SPRD_CUSTOM_MSYNC:
	{
#if 0
		struct ion_msync_data data;
		void *kaddr;
		void *paddr;
		size_t size;
		if (copy_from_user(&data, (void __user *)arg,
				sizeof(data))) {
			return -EFAULT;
		}
		kaddr = data.vaddr;
		paddr = data.paddr;	
		size = data.size;
		dmac_flush_range(kaddr, kaddr + size);
		outer_clean_range((phys_addr_t)paddr, (phys_addr_t)(paddr + size));

/*maybe open in future if support discrete page map so keep this code unremoved here*/
#else
		struct ion_msync_data data;
		void *v_addr;

		if (copy_from_user(&data, (void __user *)arg,
				sizeof(data))) {
			pr_err("sprd_heap_ioctl free copy_from_user error!\n");
			return -EFAULT;
		}

		if ((int)data.vaddr & (PAGE_SIZE - 1)){
			pr_err("sprd_heap_ioctl free data.vaddr=0x%x error!\n",data.vaddr);
			return -EFAULT;
		}

		pr_debug("sprd_heap_ioctl free vaddress=0x%x paddress=0x%x size=0x%x\n",data.vaddr,data.paddr,data.size);
		dmac_flush_range(data.vaddr, data.vaddr + data.size);

		v_addr = data.vaddr;
		while (v_addr < data.vaddr + data.size) {
			uint32_t phy_addr = user_va2pa(current->mm, (uint32_t)v_addr);
			if (phy_addr) {
				outer_flush_range(phy_addr, phy_addr + PAGE_SIZE);
			}
			v_addr += PAGE_SIZE;
		}
#endif
		break;
	}
#if defined(CONFIG_SPRD_IOMMU)
	case ION_SPRD_CUSTOM_GSP_MAP:
	{
		struct ion_mmu_data data;
		struct ion_handle *handle;

		if (copy_from_user(&data, (void __user *)arg,
				sizeof(data))) {
			pr_err("sprd_heap_ioctl gsp map copy_from_user error!\n");
			return -EFAULT;
		}

		handle = ion_import_dma_buf(client, data.fd_buffer);

		if (IS_ERR(handle)) {
			pr_err("sprd_heap_ioctl gsp map handle=0x%x error!\n", (unsigned long)handle);
			return PTR_ERR(handle);
		}

		mutex_lock(&(ion_handle_buffer(handle))->lock);
		if(0==(ion_handle_buffer(handle))->iomap_cnt[IOMMU_GSP])
		{
			(ion_handle_buffer(handle))->iova[IOMMU_GSP]=sprd_iova_alloc(IOMMU_GSP,(ion_handle_buffer(handle))->size);
			ret = sprd_iova_map(IOMMU_GSP,(ion_handle_buffer(handle))->iova[IOMMU_GSP],ion_handle_buffer(handle));
		}
		(ion_handle_buffer(handle))->iomap_cnt[IOMMU_GSP]++;
		data.iova_addr=(ion_handle_buffer(handle))->iova[IOMMU_GSP];
		data.iova_size=(ion_handle_buffer(handle))->size;
		mutex_unlock(&(ion_handle_buffer(handle))->lock);
		ion_free(client, handle);
		if (ret)
		{
			pr_err("sprd_heap_ioctl gsp map sprd_iova_map error %d!\n",ret);
			sprd_iova_free(IOMMU_GSP,data.iova_addr,data.iova_size);
			return ret;
		}

		if (copy_to_user((void __user *)arg,
				&data, sizeof(data))) {
			pr_err("sprd_heap_ioctl gsp map copy_to_user error!\n");
			return -EFAULT;
		}
		break;
	}
	case ION_SPRD_CUSTOM_GSP_UNMAP:
	{
		struct ion_mmu_data data;
		struct ion_handle *handle;

		if (copy_from_user(&data, (void __user *)arg,
				sizeof(data))) {
			pr_err("sprd_heap_ioctl gsp unmap copy_from_user error!\n");
			return -EFAULT;
		}

		handle = ion_import_dma_buf(client, data.fd_buffer);

		if (IS_ERR(handle)) {
			pr_err("sprd_heap_ioctl gsp unmap handle=0x%x ion_import_dma_buf error!\n", (unsigned long)handle);
			return PTR_ERR(handle);
		}

		mutex_lock(&(ion_handle_buffer(handle))->lock);
		(ion_handle_buffer(handle))->iomap_cnt[IOMMU_GSP]--;
		if(0==(ion_handle_buffer(handle))->iomap_cnt[IOMMU_GSP])
		{
			ret=sprd_iova_unmap(IOMMU_GSP,(ion_handle_buffer(handle))->iova[IOMMU_GSP],ion_handle_buffer(handle));
			sprd_iova_free(IOMMU_GSP,(ion_handle_buffer(handle))->iova[IOMMU_GSP],(ion_handle_buffer(handle))->size);
			(ion_handle_buffer(handle))->iova[IOMMU_GSP]=0;
		}
		mutex_unlock(&(ion_handle_buffer(handle))->lock);
		data.iova_addr=0;
		data.iova_size=0;
		ion_free(client, handle);
		if (ret) {
			pr_err("sprd_heap_ioctl gsp unmap sprd_iova_unmap error %d!\n",ret);
			return ret;
		}

		if (copy_to_user((void __user *)arg,
				&data, sizeof(data))) {
			pr_err("sprd_heap_ioctl gsp unmap copy_to_user error!\n");
			return -EFAULT;
		}
		break;
	}
	case ION_SPRD_CUSTOM_MM_MAP:
	{
		struct ion_mmu_data data;
		struct ion_handle *handle;

		if (copy_from_user(&data, (void __user *)arg,
				sizeof(data))) {
			pr_err("sprd_heap_ioctl mm map copy_from_user error!\n");
			return -EFAULT;
		}

		handle = ion_import_dma_buf(client, data.fd_buffer);

		if (IS_ERR(handle)){
			pr_err("sprd_heap_ioctl mm map handle=0x%x error!\n", (unsigned long)handle);
			return PTR_ERR(handle);
		}

		mutex_lock(&(ion_handle_buffer(handle))->lock);
		if(0==(ion_handle_buffer(handle))->iomap_cnt[IOMMU_MM])
		{
			(ion_handle_buffer(handle))->iova[IOMMU_MM]=sprd_iova_alloc(IOMMU_MM,(ion_handle_buffer(handle))->size);
			ret = sprd_iova_map(IOMMU_MM,(ion_handle_buffer(handle))->iova[IOMMU_MM],ion_handle_buffer(handle));
		}
		(ion_handle_buffer(handle))->iomap_cnt[IOMMU_MM]++;
		data.iova_size=(ion_handle_buffer(handle))->size;
		data.iova_addr=(ion_handle_buffer(handle))->iova[IOMMU_MM];
		
		mutex_unlock(&(ion_handle_buffer(handle))->lock);
		ion_free(client, handle);
		if (ret)
		{
			pr_err("sprd_heap_ioctl mm map ret=0x%x error!\n",ret);
			sprd_iova_free(IOMMU_MM,data.iova_addr,data.iova_size);
			return ret;
		}

		if (copy_to_user((void __user *)arg,
				&data, sizeof(data))) {
			pr_err("sprd_heap_ioctl mm map copy_to_user error!\n");
			return -EFAULT;
		}

		pr_debug("sprd_heap_ioctl mm map vaddress=0x%x size=0x%x\n",data.iova_addr,data.iova_size);
		break;
	}
	case ION_SPRD_CUSTOM_MM_UNMAP:
	{
		struct ion_mmu_data data;
		struct ion_handle *handle;

		if (copy_from_user(&data, (void __user *)arg,
				sizeof(data))) {
			pr_err("sprd_heap_ioctl mm unmap copy_from_user error!\n");
			return -EFAULT;
		}

		pr_debug("sprd_heap_ioctl mm unmap vaddress=0x%x size=0x%x\n",data.iova_addr,data.iova_size);
		handle = ion_import_dma_buf(client, data.fd_buffer);

		if (IS_ERR(handle)){
			pr_err("sprd_heap_ioctl mm unmap handle=0x%x error!\n", (unsigned long)handle);
			return PTR_ERR(handle);
		}

		mutex_lock(&(ion_handle_buffer(handle))->lock);
		(ion_handle_buffer(handle))->iomap_cnt[IOMMU_MM]--;
		if(0==(ion_handle_buffer(handle))->iomap_cnt[IOMMU_MM])
		{
			ret=sprd_iova_unmap(IOMMU_MM,(ion_handle_buffer(handle))->iova[IOMMU_MM],(ion_handle_buffer(handle)));
			sprd_iova_free(IOMMU_MM,(ion_handle_buffer(handle))->iova[IOMMU_MM],(ion_handle_buffer(handle))->size);
			(ion_handle_buffer(handle))->iova[IOMMU_MM]=0;
		}
		mutex_unlock(&(ion_handle_buffer(handle))->lock);
		data.iova_addr=0;
		data.iova_size=0;
		ion_free(client, handle);
		if (ret){
			pr_err("sprd_heap_ioctl mm unmap ret=0x%x error!\n",ret);
			return ret;
		}
		
		if (copy_to_user((void __user *)arg,
				&data, sizeof(data))) {
			pr_err("sprd_heap_ioctl mm unmap copy_to_user error!\n");
			return -EFAULT;
		}
		break;
	}
#endif
	case ION_SPRD_CUSTOM_FENCE_CREATE:
	{
		struct ion_fence_data data;
		
		if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
		{
			pr_err("FENCE_CREATE user data is err\n");
			return -EFAULT;
		}
		
		data.fence_fd = sprd_fence_create(data.name, &sprd_fence, data.value, &current_fence);
		if (current_fence == NULL)
		{
			pr_err("sprd_create_fence failed\n");
			return -EFAULT;
		}
		
		if (copy_to_user((void __user *)arg, &data, sizeof(data)))
		{
			sync_fence_put(current_fence);
			pr_err("copy_to_user fence failed\n");
			return -EFAULT;
		}
		
		break;
    }
	case ION_SPRD_CUSTOM_FENCE_SIGNAL:
	{
		sprd_fence_signal(&sprd_fence);
		
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


extern struct ion_heap *ion_custom_heap_create(struct ion_platform_heap *heap_data, struct device *dev);
extern void ion_custom_heap_destroy(struct ion_heap *heap);


static struct ion_heap *__ion_heap_create(struct ion_platform_heap *heap_data, struct device *dev)
{
	struct ion_heap *heap = NULL;

	switch ((int)heap_data->type) {
	case ION_HEAP_TYPE_CUSTOM:
		heap = ion_custom_heap_create(heap_data, dev);
		break;
	default:
		return ion_heap_create(heap_data);
	}

	if (IS_ERR_OR_NULL(heap)) {
		pr_err("%s: error creating heap %s type %d base %lu size %u\n",
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

int sprd_ion_probe(struct platform_device *pdev)
{
	struct ion_platform_data *pdata = pdev->dev.platform_data;
	int err = -1;
	int i;

	num_heaps = pdata->nr;

	heaps = kzalloc(sizeof(struct ion_heap *) * pdata->nr, GFP_KERNEL);

	idev = ion_device_create(&sprd_heap_ioctl);
	if (IS_ERR_OR_NULL(idev)) {
		kfree(heaps);
		return PTR_ERR(idev);
	}

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &pdata->heaps[i];

		heaps[i] = __ion_heap_create(heap_data, &pdev->dev);
		if (IS_ERR_OR_NULL(heaps[i])) {
			err = PTR_ERR(heaps[i]);
			goto error_out;
		}
		ion_device_add_heap(idev, heaps[i]);
	}
	platform_set_drvdata(pdev, idev);
	
	err = sprd_create_timeline(&sprd_fence);
	if (err != 0)
	{
		pr_err("sprd_create_timeline failed\n");
		goto error_out;
	}

	return 0;
error_out:
	for (i = 0; i < num_heaps; i++) {
		if (heaps[i])
			ion_heap_destroy(heaps[i]);
	}
	kfree(heaps);
	return err;
}

int sprd_ion_remove(struct platform_device *pdev)
{
	struct ion_device *idev = platform_get_drvdata(pdev);
	int i;

	ion_device_destroy(idev);
	for (i = 0; i < num_heaps; i++)
		__ion_heap_destroy(heaps[i]);
	kfree(heaps);
	
	sprd_destroy_timeline(&sprd_fence);

	return 0;
}

static struct platform_driver ion_driver = {
	.probe = sprd_ion_probe,
	.remove = sprd_ion_remove,
	.driver = { .name = "ion-sprd" }
};

static int __init ion_init(void)
{
	return platform_driver_register(&ion_driver);
}

static void __exit ion_exit(void)
{
	platform_driver_unregister(&ion_driver);
}

module_init(ion_init);
module_exit(ion_exit);

