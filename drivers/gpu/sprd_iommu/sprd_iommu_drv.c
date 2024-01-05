/*
 * drivers/gpu/iommu/iommu.c *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/ion.h>
#include <linux/clk.h>
#include <linux/miscdevice.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/genalloc.h>
#include <linux/sprd_iommu.h>
#include <linux/scatterlist.h>
#include <mach/hardware.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include "../ion/ion_priv.h"
#include "sprd_iommu_sysfs.h"

#define SPRD_IOMMU_PAGE_SIZE	0x1000
/**
 * Page table index from address
 * Calculates the page table index from the given address
*/
#define SPRD_IOMMU_PTE_ENTRY(address) (((address)>>12) & 0x3FFF)

struct sprd_iommu_dev *iommu_devs[IOMMU_MAX]={NULL,NULL};
extern struct sprd_iommu_ops iommu_gsp_ops;
extern struct sprd_iommu_ops iommu_mm_ops;

unsigned long sprd_iova_alloc(int iommu_id, unsigned long iova_length)
{
	return iommu_devs[iommu_id]->ops->iova_alloc(iommu_devs[iommu_id],iova_length);
}

void sprd_iova_free(int iommu_id, unsigned long iova, unsigned long iova_length)
{
	iommu_devs[iommu_id]->ops->iova_free(iommu_devs[iommu_id],iova,iova_length);
}

int sprd_iova_map(int iommu_id, unsigned long iova, struct ion_buffer *handle)
{
	return iommu_devs[iommu_id]->ops->iova_map(iommu_devs[iommu_id],iova, handle->size, handle);
}
int sprd_iova_unmap(int iommu_id, unsigned long iova,  struct ion_buffer *handle)
{
	return iommu_devs[iommu_id]->ops->iova_unmap(iommu_devs[iommu_id],iova,handle->size,handle);
}

void sprd_iommu_module_enable(uint32_t iommu_id)
{
	iommu_devs[iommu_id]->ops->enable(iommu_devs[iommu_id]);
}

void sprd_iommu_module_disable(uint32_t iommu_id)
{
	iommu_devs[iommu_id]->ops->disable(iommu_devs[iommu_id]);
}

static int sprd_iommu_suspend(struct platform_device *pdev, pm_message_t state)
{
	int err=-1;
	struct sprd_iommu_dev *iommu_dev=platform_get_drvdata(pdev); 
	printk("sprd_iommu id:%d, name:%s mmu_clock:0x%p  mmu_mclock:0x%p suspend\n",
		iommu_dev->init_data->id,iommu_dev->init_data->name,iommu_dev->mmu_clock,iommu_dev->mmu_mclock);
	err=iommu_dev->ops->backup(iommu_dev);
	return err;
}

static int sprd_iommu_resume(struct platform_device *pdev)
{
	int err=-1;
	struct sprd_iommu_dev *iommu_dev=platform_get_drvdata(pdev); 
	printk("sprd_iommu id:%d, name:%s mmu_clock:0x%p  mmu_mclock:0x%p resume\n",
		iommu_dev->init_data->id,iommu_dev->init_data->name,iommu_dev->mmu_clock,iommu_dev->mmu_mclock);
	err=iommu_dev->ops->restore(iommu_dev);
	return err;
}

static int sprd_iommu_probe(struct platform_device *pdev)
{
	int err=-1;
	struct sprd_iommu_init_data *pdata = pdev->dev.platform_data;
	struct sprd_iommu_dev *iommu_dev;
	iommu_dev = kzalloc(sizeof(struct sprd_iommu_dev), GFP_KERNEL);
	if (NULL==iommu_dev)
		return -1;
	iommu_dev->init_data = pdata;
	mutex_init(&iommu_dev->mutex_pgt);
	if(0==strncmp("sprd_iommu_gsp",pdata->name,14))
	{
		iommu_dev->ops=&iommu_gsp_ops;
		iommu_devs[pdata->id]=iommu_dev;
	}
	else if(0==strncmp("sprd_iommu_mm",pdata->name,13))
	{
		iommu_dev->ops=&iommu_mm_ops;
		iommu_devs[pdata->id]=iommu_dev;
	}
	err=iommu_dev->ops->init(iommu_dev,pdata);
	if(err<0)
	{
		kfree(iommu_dev);
		pr_err("iommu %s : failed to init device %d.\n",pdata->name, err);
		return err;
	}

	iommu_dev->misc_dev.minor = MISC_DYNAMIC_MINOR;
	iommu_dev->misc_dev.name = pdata->name;
	iommu_dev->misc_dev.fops = NULL;
	iommu_dev->misc_dev.parent = NULL;
	err = misc_register(&iommu_dev->misc_dev);
	if (err) {
		pr_err("iommu %s : failed to register misc device.\n",pdata->name);
		return err;
	}
	sprd_iommu_sysfs_register(iommu_dev,iommu_dev->init_data->name);
	platform_set_drvdata(pdev, iommu_dev);
	printk("sprd_iommu id:%d, name:%s, dev->pgt:0x%lx, dev->pool:0x%p mmu_clock:0x%p\n",
		pdata->id,pdata->name,iommu_dev->pgt,iommu_dev->pool,iommu_dev->mmu_clock);
	return err;
}

static int sprd_iommu_remove(struct platform_device *pdev)
{
	struct sprd_iommu_dev *iommu_dev=platform_get_drvdata(pdev); 
	misc_deregister(&iommu_dev->misc_dev);
	sprd_iommu_sysfs_unregister(iommu_dev,iommu_dev->init_data->name);
	iommu_dev->ops->exit(iommu_dev);
	gen_pool_destroy(iommu_dev->pool);
	kfree(iommu_dev);
	return 0;
}

static struct platform_driver iommu_driver = {
	.probe = sprd_iommu_probe,
	.remove = sprd_iommu_remove,
	.suspend = sprd_iommu_suspend,
	.resume = sprd_iommu_resume,
	.driver = {
		.owner=THIS_MODULE,
		.name="sprd_iommu",
	},
};

static int __init iommu_init(void)
{
	int err=-1;
	err=platform_driver_register(&iommu_driver);
	if(err<0)
	{
		pr_err("sprd_iommu register err: %d\n",err);
	}
	return err;
}

static void __exit iommu_exit(void)
{
	platform_driver_unregister(&iommu_driver);
}

module_init(iommu_init);
module_exit(iommu_exit);

MODULE_DESCRIPTION("SPRD IOMMU Driver");
MODULE_LICENSE("GPL");
