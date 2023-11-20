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
#include <asm/io.h>
#include "sprd_iommu_sysfs.h"

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include <linux/memblock.h>

static int sprd_iommu_probe(struct platform_device *pdev);
static int sprd_iommu_remove(struct platform_device *pdev);
static int sprd_iommu_suspend(struct platform_device *pdev, pm_message_t state);
static int sprd_iommu_resume(struct platform_device *pdev);

struct sprd_iommu_dev *iommu_devs[IOMMU_MAX] = {NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL};

static struct sprd_iommu_init_data sprd_iommu_gsp_data = {
	.id = IOMMU_GSP,
	.name = "sprd_iommu_gsp",
};

static struct sprd_iommu_init_data sprd_iommu_mm_data = {
	.id = IOMMU_MM,
	.name = "sprd_iommu_mm",
};

static struct sprd_iommu_init_data sprd_iommu_vsp_data = {
	.id = IOMMU_VSP,
	.name = "sprd_iommu_vsp",
};

static struct sprd_iommu_init_data sprd_iommu_dcam_data = {
	.id = IOMMU_DCAM,
	.name = "sprd_iommu_dcam",
};

static struct sprd_iommu_init_data sprd_iommu_dispc_data = {
	.id = IOMMU_DISPC,
	.name = "sprd_iommu_dispc",
};

static struct sprd_iommu_init_data sprd_iommu_gsp0_data = {
	.id = IOMMU_GSP0,
	.name = "sprd_iommu_gsp0",
};

static struct sprd_iommu_init_data sprd_iommu_gsp1_data = {
	.id = IOMMU_GSP1,
	.name = "sprd_iommu_gsp1",
};

static struct sprd_iommu_init_data sprd_iommu_vpp_data = {
	.id = IOMMU_VPP,
	.name = "sprd_iommu_vpp",
};

const struct of_device_id iommu_ids[] = {
	{ .compatible = "sprd,sprd_iommu", },
	{},
};

static struct platform_driver iommu_driver = {
	.probe = sprd_iommu_probe,
	.remove = sprd_iommu_remove,
#ifndef CONFIG_ARCH_SCX35L
	.suspend = sprd_iommu_suspend,
	.resume = sprd_iommu_resume,
#endif
	.driver = {
		.owner=THIS_MODULE,
		.name="sprd_iommu",
		.of_match_table = iommu_ids,
	},
};

unsigned long sprd_iova_alloc(int iommu_id, unsigned long iova_length)
{
	if (iommu_devs[iommu_id]) {
		return iommu_devs[iommu_id]->ops->iova_alloc(
					iommu_devs[iommu_id], iova_length);
	} else {
		pr_err("%s, id:%d iommu don't exist, error!",
				__func__, iommu_id);
		return 0;
	}
}

void sprd_iova_free(int iommu_id, unsigned long iova,
		unsigned long iova_length)
{
	if (iommu_devs[iommu_id]) {
		iommu_devs[iommu_id]->ops->iova_free(
				iommu_devs[iommu_id], iova, iova_length);
	} else {
		pr_err("%s, id:%d iommu don't exist, error!",
				__func__, iommu_id);
	}
}

int sprd_iova_map(int iommu_id, unsigned long iova, unsigned long iova_length,
		struct sg_table *table)
{
	if (iommu_devs[iommu_id]) {
		return iommu_devs[iommu_id]->ops->iova_map(
				iommu_devs[iommu_id], iova, iova_length, table);
	} else {
		pr_err("%s, id:%d iommu don't exist, error!",
				__func__, iommu_id);
		return -1;
	}
}

int sprd_iova_unmap(int iommu_id, unsigned long iova,
		unsigned long iova_length)
{
	if (iommu_devs[iommu_id]) {
		return iommu_devs[iommu_id]->ops->iova_unmap(
					iommu_devs[iommu_id], iova, iova_length);
	} else {
		pr_err("%s, id:%d iommu don't exist, error!",
				__func__, iommu_id);
		return -1;
	}
}

void sprd_iommu_module_enable(uint32_t iommu_id)
{
	if (iommu_devs[iommu_id]->light_sleep_en ||
		IOMMU_GSP == iommu_id ||
		IOMMU_DISPC == iommu_id ||
		IOMMU_GSP0 == iommu_id ||
		IOMMU_GSP1 == iommu_id) {
		if (iommu_devs[iommu_id])
			iommu_devs[iommu_id]->ops->enable(iommu_devs[iommu_id]);
		else
			pr_err("%s, id:%d iommu don't exist, error!",
					__func__, iommu_id);
	}
}

void sprd_iommu_module_disable(uint32_t iommu_id)
{
	if (iommu_devs[iommu_id]->light_sleep_en ||
		IOMMU_GSP == iommu_id ||
		IOMMU_DISPC == iommu_id ||
		IOMMU_GSP0 == iommu_id ||
		IOMMU_GSP1 == iommu_id) {
		if (iommu_devs[iommu_id])
			iommu_devs[iommu_id]->ops->disable(iommu_devs[iommu_id]);
		else
			pr_err("%s, id:%d iommu don't exist, error!",
					__func__, iommu_id);
	}
}

static int sprd_iommu_suspend(struct platform_device *pdev, pm_message_t state)
{
	int err = -1;
	struct sprd_iommu_dev *iommu_dev = platform_get_drvdata(pdev);

	pr_info("%s, id:%d, name:%s mmu_clock:0x%p  mmu_mclock:0x%p suspend\n",
		__func__, iommu_dev->init_data->id, iommu_dev->init_data->name,
		iommu_dev->mmu_clock, iommu_dev->mmu_mclock);
	err = iommu_dev->ops->backup(iommu_dev);

	return err;
}

static int sprd_iommu_resume(struct platform_device *pdev)
{
	int err = -1;
	struct sprd_iommu_dev *iommu_dev=platform_get_drvdata(pdev);

	pr_info("%s, id:%d, name:%s mmu_clock:0x%p  mmu_mclock:0x%p resume\n",
		__func__, iommu_dev->init_data->id, iommu_dev->init_data->name,
		iommu_dev->mmu_clock, iommu_dev->mmu_mclock);
	err = iommu_dev->ops->restore(iommu_dev);

	return err;
}

static inline int sprd_iommu_get_description(struct device_node *np,
					char* desc_name, char**desc)
{
	return  of_property_read_string(np, (const char*)desc_name,(const char**) desc);
}

static int sprd_iommu_get_resource(struct device_node *np,
				struct sprd_iommu_init_data *pdata)
{
	int err = 0;
	uint32_t val;
	struct resource res;

	pr_info("%s,name:%s\n", __func__, pdata->name);

	err = of_address_to_resource(np, 0, &res);
	if (err < 0)
		return err;

	pdata->iova_base = res.start;
	pdata->iova_size = resource_size(&res);
	pr_info("%s,iova_base:0x%lx,iova_size:%zx\n", __func__,
		pdata->iova_base,pdata->iova_size);

	err = of_address_to_resource(np, 1, &res);
	if (err < 0)
		return err;

	pr_info("%s,pgt_base phy:0x%x\n", __func__, res.start);
	pdata->pgt_base = (unsigned long)ioremap_nocache(res.start,
		resource_size(&res));
	if (!pdata->pgt_base)
		BUG();

	pdata->pgt_size = resource_size(&res);
	pr_info("%s,pgt_base:%lx,pgt_size:%zx\n", __func__, pdata->pgt_base,
		pdata->pgt_size);

	err = of_address_to_resource(np, 2, &res);
	if (err < 0)
		return err;

	pr_info("%s,ctrl_reg phy:0x%x\n", __func__, res.start);
	pdata->ctrl_reg = (unsigned long)ioremap_nocache(res.start,
		resource_size(&res));
	if (!pdata->ctrl_reg)
		BUG();
	pr_info("%s,ctrl_reg:0x%lx\n", __func__, pdata->ctrl_reg);

	err = of_property_read_u32(np, "sprd,reserved-fault-page", &val);
	if (err) {
#if 0
		pr_info("%s, can't get reserved fault page addr\n", __func__);
		pdata->fault_page = 0;
#else
		pr_info("%s, can't get reserved fault page addr from dts\n", __func__);
		pdata->fault_page = __pa(__get_free_page(GFP_KERNEL));
		if(!pdata->fault_page)
			BUG();
		else
			pr_info("%s, succeed to get reserved fault page, phy:0x%lx\n", __func__, pdata->fault_page);
#endif
		pdata->re_route_page = 0;

	} else {
		pr_info("%s, reserved fault page phy:0x%lx\n", __func__, val);
		pdata->fault_page = val;
		pdata->re_route_page = val + PAGE_SIZE;
	}

	err = of_property_read_u32(np, "sprd,reserved-rr-page", &val);
	if (err) {
		pr_info("%s, can't get reserved rr page addr\n", __func__);
		pdata->re_route_page = 0;
	} else {
		pr_info("%s, reserved rr page phy:0x%lx\n", __func__, val);
		pdata->re_route_page = val;
	}

	err = of_property_read_u32(np, "sprd,iommu-rev", &val);
	if (err) {
		pr_info("%s, can't get iommu-rev\n", __func__);
		pdata->iommu_rev = 1;
	} else {
		pr_info("%s, iommu-rev: %u\n", __func__, val);
		pdata->iommu_rev = val;
	}

	return 0;
}

static int sprd_iommu_probe(struct platform_device *pdev)
{
	char func_name[] = "func-name";
	char func_desc[128];
	char* pdesc=&(func_desc[0]);
	struct device_node *np = pdev->dev.of_node;
	int err = 0;
	struct sprd_iommu_init_data *pdata;
	struct sprd_iommu_dev *iommu_dev;

	pr_info("%s, begin\n", __func__);
	iommu_dev = kzalloc(sizeof(struct sprd_iommu_dev), GFP_KERNEL);
	if (NULL == iommu_dev) {
		pr_err("%s, fail to kzalloc\n", __func__);
		return -ENOMEM;
	}

	if (-1 == sprd_iommu_get_description(np, func_name, &pdesc)) {
		pr_err("%s, fail to get description\n", __func__);
		err = -EINVAL;
		goto errout;
	}
	pr_info("%s, function name is %s\n", __func__, pdesc);

	if (0 == strncmp("sprd_iommu_gsp", pdesc, 14)) {
		pdata = &sprd_iommu_gsp_data;
		iommu_dev->ops = &iommu_gsp_ops;
	} else if (0 == strncmp("sprd_iommu_mm", pdesc, 13)) {
		pdata = &sprd_iommu_mm_data;
		iommu_dev->ops = &iommu_mm_ops;
	} else if (0 == strncmp("sprd_iommu_vsp", pdesc, 14)) {
		pdata = &sprd_iommu_vsp_data;
		iommu_dev->ops = &iommu_vsp_ops;
	} else if (0 == strncmp("sprd_iommu_dcam", pdesc, 15)) {
		pdata = &sprd_iommu_dcam_data;
		iommu_dev->ops = &iommu_dcam_ops;
	} else if (0 == strncmp("sprd_iommu_dispc", pdesc, 16)) {
		pdata = &sprd_iommu_dispc_data;
		iommu_dev->ops = &iommu_dispc_ops;
	} else if (0 == strncmp("sprd_iommu_gsp0", pdesc, 15)) {
		pdata = &sprd_iommu_gsp0_data;
		iommu_dev->ops = &iommu_gsp0_ops;
	} else if (0 == strncmp("sprd_iommu_gsp1", pdesc, 15)) {
		pdata = &sprd_iommu_gsp1_data;
		iommu_dev->ops = &iommu_gsp1_ops;
	} else if (0 == strncmp("sprd_iommu_vpp", pdesc, 14)) {
		pdata = &sprd_iommu_vpp_data;
		iommu_dev->ops = &iommu_vpp_ops;
	} else {
		pr_err("%s, function name mismatch\n", __func__);
		err = -EINVAL;
		goto errout;
	}

	err = sprd_iommu_get_resource(np, pdata);
	if (err) {
	    dev_err(&pdev->dev, "no reg of property specified\n");
	    pr_err("%s: failed to get resource!\n", pdesc);
	    goto errout;
	}

	iommu_devs[pdata->id] = iommu_dev;
	iommu_dev->init_data = pdata;
	mutex_init(&iommu_dev->mutex_pgt);
	mutex_init(&iommu_dev->mutex_map);
	/*If ddr frequency >= 500Mz, iommu need enable div2 frequency,
	 * because of limit of iommu iram frq.*/
	iommu_dev->div2_frq = 500;
	iommu_dev->light_sleep_en = false;
	iommu_dev->misc_dev.minor = MISC_DYNAMIC_MINOR;
	iommu_dev->misc_dev.name = pdata->name;
	iommu_dev->misc_dev.fops = NULL;
	iommu_dev->misc_dev.parent = NULL;

	err = misc_register(&iommu_dev->misc_dev);
	if (err) {
		pr_err("iommu %s : failed to register misc device %d.\n",
			pdata->name, err);
		goto errout;
	}

	iommu_dev->misc_dev.this_device->of_node = pdev->dev.of_node;
	err = iommu_dev->ops->init(iommu_dev, pdata);
	if (err) {
		pr_err("iommu %s : failed to init device %d.\n", pdata->name, err);
		goto errout;
	}

	sprd_iommu_sysfs_register(iommu_dev, iommu_dev->init_data->name);
	platform_set_drvdata(pdev, iommu_dev);

	pr_info("%s, name:%s, dev->pgt:0x%lx, dev->pool:0x%p mmu_clock:0x%p\n",
		__func__, pdata->name, iommu_dev->pgt,
		iommu_dev->pool, iommu_dev->mmu_clock);
	pr_info("%s, end\n", __func__);

	return 0;

errout:
	iommu_devs[pdata->id] = NULL;
	kfree(iommu_dev);
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

static int __init iommu_init(void)
{
	int err=-1;
	pr_info("%s, begin\n", __func__);
	err=platform_driver_register(&iommu_driver);
	if (err < 0)
		pr_err("sprd_iommu register err: %d\n",err);
	pr_info("%s, end\n", __func__);
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
