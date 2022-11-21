/*
 * Copyright 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/ion.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/pxa_ion.h>
#include <asm/cacheflush.h>
#include "../ion_priv.h"

struct pxa_ion_info {
	struct ion_device       *idev;
	struct ion_heap         **heaps;
	int                     heap_cnt;
};

struct ion_device *pxa_ion_dev;
EXPORT_SYMBOL(pxa_ion_dev);

static long pxa_ion_ioctl(struct ion_client *client, unsigned int cmd,
							unsigned long arg)
{
	switch (cmd) {
	case ION_PXA_PHYS:
	       {
			struct ion_pxa_region region ;
			if (copy_from_user(&region, (void __user *)arg,
						sizeof(struct ion_pxa_region)))
				return -EFAULT;

			ion_phys(client, region.handle,
					&region.addr, &region.len);
			if (copy_to_user((void __user *)arg, &region,
						sizeof(struct ion_pxa_region)))
				return -EFAULT;
			break;
		}
	case ION_PXA_SYNC:
	{
		struct ion_pxa_cache_region region;
		struct ion_buffer *buffer;
		struct vm_area_struct *vma;
		bool valid_handle;
		phys_addr_t pstart, pend;
		unsigned long start, end;

		if (copy_from_user(&region, (void __user *)arg,
				   sizeof(struct ion_pxa_cache_region)))
			return -EFAULT;
		valid_handle = ion_handle_validate(client, region.handle);
		if (!valid_handle) {
			pr_err("%s:invalid handle 0x%x\n",
				__func__, (unsigned int)region.handle);
			return -EINVAL;
		}
		buffer = ion_handle_buffer(region.handle);
		mutex_lock(&buffer->lock);
		if (region.offset > buffer->size) {
			mutex_unlock(&buffer->lock);
			pr_err("%s: invalid offset exceeds buffer size\n",
				__func__);
			return -EINVAL;
		}
		if (region.offset + region.len > buffer->size)
			region.len = buffer->size - region.offset;
		/*
		 * Limitation: There's only one vma in one process context.
		 * If we want to support multiple vmas in one process context,
		 * we need to extend the interface.
		 */
		vma = pxa_ion_find_vma(buffer);
		if (!vma) {
			mutex_unlock(&buffer->lock);
			pr_err("%s: Buffer doesn't map to virtual address\n",
				__func__);
			return -EINVAL;
		}
		start = vma->vm_start + region.offset;
		end = start + region.len;
		/* don't check direction */
		dmac_flush_range((void *)start, (void *)end);

		pstart = buffer->priv_phys + region.offset;
		pend = pstart + region.len;
		outer_flush_range(pstart, pend);
		mutex_unlock(&buffer->lock);
		break;
	}
	default:
	       pr_err("%s: Unknown ioctl %d\n", __func__, cmd);
	       return -EINVAL;
	}
	return 0;
}

static int pxa_ion_probe(struct platform_device *pdev)
{
	struct ion_platform_data *pdata = pdev->dev.platform_data;
	struct pxa_ion_info *info;
	int err, i;

	if (!pdata)
		return -EINVAL;

	info = devm_kzalloc(&pdev->dev, sizeof(struct pxa_ion_info),
								GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->heaps = devm_kzalloc(&pdev->dev,
		sizeof(struct ion_heap *) * pdata->nr, GFP_KERNEL);
	if (!info->heaps) {
		devm_kfree(&pdev->dev, info);
		return -ENOMEM;
	}

	info->heap_cnt = pdata->nr;

	info->idev = ion_device_create(pxa_ion_ioctl);
	if (IS_ERR_OR_NULL(info->idev)) {
		devm_kfree(&pdev->dev, info->heaps);
		devm_kfree(&pdev->dev, info);
		return PTR_ERR(info->idev);
	}

	pxa_ion_dev = info->idev;

	/* create the heaps as specified in the board file */
	for (i = 0; i < pdata->nr; i++) {
		struct ion_platform_heap *heap_data = &pdata->heaps[i];

		info->heaps[i] = ion_heap_create(heap_data);
		if (IS_ERR_OR_NULL(info->heaps[i])) {
			err = PTR_ERR(info->heaps[i]);
			goto err_heap;
		}
		ion_device_add_heap(info->idev, info->heaps[i]);
	}
	platform_set_drvdata(pdev, info);

	return 0;

err_heap:
	for (; i > 0; i--)
		ion_heap_destroy(info->heaps[i]);
	ion_device_destroy(info->idev);
	devm_kfree(&pdev->dev, info->heaps);
	devm_kfree(&pdev->dev, info);
	return err;
}

static int pxa_ion_remove(struct platform_device *pdev)
{
	struct pxa_ion_info *info = platform_get_drvdata(pdev);
	int i;

	if (info) {
		for (i = 0; i < info->heap_cnt; i++)
			ion_heap_destroy(info->heaps[i]);
		ion_device_destroy(info->idev);
		devm_kfree(&pdev->dev, info->heaps);
		devm_kfree(&pdev->dev, info);
		platform_set_drvdata(pdev, NULL);
	}
	return 0;
}

static struct platform_driver pxa_ion_driver = {
	.probe = pxa_ion_probe,
	.remove = pxa_ion_remove,
	.driver = { .name = "pxa-ion" }
};

module_platform_driver(pxa_ion_driver);

MODULE_LICENSE("GPL v2");

