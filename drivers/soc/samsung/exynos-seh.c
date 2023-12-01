/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - Smart Exception Handler
 * Author: Jang Hyunsung <hs79.jang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>

#include <soc/samsung/exynos-smc.h>
#include <soc/samsung/exynos-el3_mon.h>
#include <soc/samsung/exynos-seh.h>

#define ALLOC_SIZE	(PAGE_SIZE * 3)

static char *smc_debug_mem;
static unsigned long cm_debug_func_addr = 0;

void exynos_seh_set_cm_debug_function(unsigned long addr)
{
	cm_debug_func_addr = addr;
}
EXPORT_SYMBOL(exynos_seh_set_cm_debug_function);

static void exynos_smart_exception_handler(unsigned int id,
				unsigned long elr, unsigned long esr,
				unsigned long sctlr, unsigned long ttbr,
				unsigned long far, unsigned long x6,
				unsigned int offset)
{
	int i;
	unsigned long *ptr;
	unsigned long tmp;

	pr_err("========================================="
		"=========================================\n");

	if (id)
		pr_err("%s: There has been an unexpected exception from "
			"a LDFW which has smc id 0x%x\n\n", __func__, id);
	else
		pr_err("%s: There has been an unexpected exception from "
			"the EL3 monitor.\n\n", __func__);

	if (id) {
		pr_err("elr_el1   : 0x%016lx, \tesr_el1  : 0x%016lx\n",
								elr, esr);
		pr_err("sctlr_el1 : 0x%016lx, \tttbr_el1 : 0x%016lx\n",
								sctlr, ttbr);
		pr_err("far_el1   : 0x%016lx, \tlr (EL1) : 0x%016lx\n\n",
								far, x6);
	} else {
		pr_err("elr_el3   : 0x%016lx, \tesr_el3  : 0x%016lx\n",
								elr, esr);
		pr_err("sctlr_el3 : 0x%016lx, \tttbr_el3 : 0x%016lx\n",
								sctlr, ttbr);
		pr_err("far_el3   : 0x%016lx, \tscr_el3  : 0x%016lx\n\n",
								far, x6);
	}

	if ((offset > 0x0 && offset < (PAGE_SIZE * 2))
			&& !(offset % 0x8) && (smc_debug_mem)) {

		tmp = (unsigned long)smc_debug_mem;
		tmp += (unsigned long)offset;
		ptr = (unsigned long *)tmp;

		for (i = 0; i < 15; i++) {
			pr_err("x%02d : 0x%016lx, \tx%02d : 0x%016lx\n",
					i * 2, ptr[i * 2],
					i * 2 + 1, ptr[i * 2 + 1]);
		}
		pr_err("x%02d : 0x%016lx\n", i * 2,  ptr[i * 2]);
	}

	pr_err("\n[WARNING] IT'S GOING TO CAUSE KERNEL PANIC FOR DEBUGGING.\n\n");

	pr_err("========================================="
		"=========================================\n");

#if IS_ENABLED(CONFIG_EXYNOS_CRYPTOMANAGER)
	/* If exception is occured in CM LDFW, call debug function */
	if ((id & 0x1000) & (cm_debug_func_addr != 0)) {
		((void (*) (void))(cm_debug_func_addr))();
	}
#endif
	/* make kernel panic */
	BUG();

	/* SHOULD NOT be here */
	while(1);
}

static int exynos_seh_probe(struct platform_device *pdev)
{
	struct seh_info_data *data;
	unsigned long addr = (unsigned long)exynos_smart_exception_handler;
	unsigned long ret;

	data = devm_kzalloc(&pdev->dev, sizeof(struct seh_info_data), GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "Fail to allocate memory(seh_info_data)\n");
		ret = -ENOMEM;
		goto out;
	}

	platform_set_drvdata(pdev, data);
	data->dev = &pdev->dev;

	/*
	 * EL3_Monitor has mapped Kernel region with non-cacheable,
	 * so Kernel allocates it by dma_alloc_coherent
	 */

	ret = dma_set_mask_and_coherent(data->dev, DMA_BIT_MASK(36));
	if (ret) {
		dev_err(data->dev, "Fail to dma_set_mask. ret[%d]\n", ret);
		goto out;
	}

	data->fail_info = dmam_alloc_coherent(data->dev,
			ALLOC_SIZE,
			&data->fail_info_pa,
			__GFP_ZERO);
	if (!data->fail_info) {
		dev_err(data->dev, "Fail to allocate memory(seh_fail_info)\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = exynos_smc(SMC_CMD_SET_DEBUG_MEM, (u64)data->fail_info_pa, (u64)ALLOC_SIZE, 0);

	/* correct return value is input size */
	if (ret != ALLOC_SIZE) {
		dev_err(data->dev,"Can not set the address to el3 monitor. "
				"ret = 0x%lx. free the kmem\n", ret);
		goto out_with_dma_free;
	}

	/* Set Virtual Address */
	smc_debug_mem = data->fail_info;

	ret = exynos_smc(SMC_CMD_SET_SEH_ADDRESS, addr, 0, 0);

	/* return value not zero means failure */
	if (ret) {
		dev_err(data->dev,"did not set the seh address to el3 monitor. "
				"ret = 0x%lx.\n", ret);
		goto out_with_dma_free;
	} else
		dev_err(data->dev,"set the seh address to el3 monitor well.\n");

	return 0;

out_with_dma_free:
	dmam_free_coherent(data->dev,
			ALLOC_SIZE,
			data->fail_info,
			data->fail_info_pa);

	platform_set_drvdata(pdev, NULL);
	data->fail_info = NULL;
	data->fail_info_pa = 0;

out:
	return ret;

}

static int exynos_seh_remove(struct platform_device *pdev)
{
	struct seh_info_data *data = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);

	if (data->fail_info) {
		dma_free_coherent(data->dev,
				ALLOC_SIZE,
				data->fail_info,
				data->fail_info_pa);

		data->fail_info = NULL;
		data->fail_info_pa = 0;
	}

	return 0;
}

static const struct of_device_id exynos_seh_of_match_table[] = {
	{ .compatible = "samsung,exynos-seh", },
	{ },
};
MODULE_DEVICE_TABLE(of, exynos_seh_of_match_table);

static struct platform_driver exynos_seh_driver = {
	.probe = exynos_seh_probe,
	.remove = exynos_seh_remove,
	.driver = {
		.name = "exynos-seh",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_seh_of_match_table),
	}
};

static int __init exynos_seh_init(void)
{
	return platform_driver_register(&exynos_seh_driver);
}

static void __exit exynos_seh_exit(void)
{
	platform_driver_unregister(&exynos_seh_driver);
}

module_init(exynos_seh_init);
module_exit(exynos_seh_exit);

MODULE_LICENSE("GPL");
