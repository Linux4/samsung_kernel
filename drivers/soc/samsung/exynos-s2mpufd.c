/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - S2MPU fail detector
 * Author: Junho Choi <junhosj.choi@samsung.com>
 *	   Siheung Kim <sheung.kim@samsung.com>
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/hvc.h>
#include <asm/cacheflush.h>

#include <soc/samsung/exynos-s2mpufd.h>
#include <soc/samsung/exynos-s2mpu.h>

static irqreturn_t exynos_s2mpufd_irq_handler(int irq, void *dev_id)
{
	struct s2mpufd_info_data *data = dev_id;
	uint32_t irq_idx;
	pr_info("s2mpu interrupt called %d !!\n",irq);
	for (irq_idx = 0; irq_idx < data->irqcnt; irq_idx++) {
		if (irq == data->irq[irq_idx])
			break;
	}

	/*
	 * Interrupt status register in S2MPUFD will be cleared
	 * in this HVC handler
	 */
	data->need_log = exynos_hvc(HVC_CMD_GET_S2MPUFD_FAIL_INFO,
					data->fail_info_pa,
				    	irq_idx,
				    	data->info_flag,
				    	0);
	__inval_dcache_area((void *)data->fail_info, sizeof(struct s2mpufd_fail_info) * data->ch_num);
	return IRQ_WAKE_THREAD;
}

static irqreturn_t exynos_s2mpufd_irq_handler_thread(int irq, void *dev_id)
{
	struct s2mpufd_info_data *data = dev_id;
	unsigned int intr_stat, addr_low, fail_vid, fail_rw;
	uint64_t addr_high;
	unsigned int len, axid;
	uint32_t irq_idx;

	pr_info("===============[S2MPU FAIL DETECTION]===============\n");
	if (data->need_log == S2MPUFD_SKIP_FAIL_INFO_LOGGING) {
		pr_debug("S2MPU_FAIL_DETECTOR: Ignore S2MPU illegal reads\n");
		return IRQ_HANDLED;
	}

	/* find right irq index */
	for (irq_idx = 0; irq_idx < data->irqcnt; irq_idx++) {
		if (irq == data->irq[irq_idx])
		break;
	}

	pr_auto(ASL4, "[Channel %d] Instance : %s\n", irq_idx, irq_idx ? "PCIe(CP)" : "PCIe(Wi-Fi)");

	intr_stat = data->fail_info[irq_idx].s2mpufd_intr_stat;
	addr_low = data->fail_info[irq_idx].s2mpufd_fail_addr_low;
	addr_high = data->fail_info[irq_idx].s2mpufd_fail_addr_high;
	fail_vid = data->fail_info[irq_idx].s2mpufd_fail_vid;
	fail_rw = data->fail_info[irq_idx].s2mpufd_rw;
	len = data->fail_info[irq_idx].s2mpufd_len;
	axid = data->fail_info[irq_idx].s2mpufd_axid;


	pr_auto(ASL4, "- Fail Adddress : %#lx\n",
		addr_high ?
		(addr_high << 32) | addr_low :
		addr_low);

	pr_auto(ASL4, "- Fail Direction : %s\n",
		fail_rw ?
		"WRITE" :
		"READ");

	pr_auto(ASL4, "- Fail Virtual Domain ID : %d\n",
		fail_vid);

	pr_auto(ASL4, "- Fail RW Length : %#x\n",
		len);

	pr_auto(ASL4, "- Fail AxID : %#x\n",
		axid);

	pr_info("\n");

#ifdef CONFIG_EXYNOS_S2MPUFD_ILLEGAL_ACCESS_PANIC
	BUG();
#endif
	return IRQ_HANDLED;
}

static int exynos_s2mpufd_probe(struct platform_device *pdev)
{
	struct s2mpufd_info_data *data;
	int ret, i;

	data = devm_kzalloc(&pdev->dev, sizeof(struct s2mpufd_info_data), GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "Fail to allocate memory(s2mpufd_info_data)\n");
		ret = -ENOMEM;
		goto out;
	}

	platform_set_drvdata(pdev, data);
	data->dev = &pdev->dev;

	ret = of_property_read_u32(data->dev->of_node,
				   "channel",
				   &data->ch_num);
	if (ret) {
		dev_err(data->dev,
			"Fail to get S2MPUFD channel number(%d) from dt\n",
			data->ch_num);
		goto out;
	}

	dev_info(data->dev,
		"S2MPUFD channel number is valid\n");

	/*
	 * Allocate S2MPUFD fail information buffers as to subsystems
	 *
	 */
	ret = dma_set_mask(data->dev, DMA_BIT_MASK(36));
	if (ret) {
		dev_err(data->dev,
			"Fail to dma_set_mask. ret[%d]\n",
			ret);
		goto out;
	}

	data->fail_info = dma_zalloc_coherent(data->dev,
						sizeof(struct s2mpufd_fail_info) *
						data->ch_num,
						&data->fail_info_pa,
						GFP_KERNEL);
	if (!data->fail_info) {
		dev_err(data->dev, "Fail to allocate memory(s2mpufd_fail_info)\n");
		ret = -ENOMEM;
		goto out;
	}

	dev_dbg(data->dev,
		"VA of s2mpufd_fail_info : %lx\n",
		(unsigned long)data->fail_info);
	dev_dbg(data->dev,
		"PA of s2mpufd_fail_info : %llx\n",
		data->fail_info_pa);

	ret = of_property_read_u32(data->dev->of_node, "irqcnt", &data->irqcnt);
	if (ret) {
		dev_err(data->dev,
			"Fail to get irqcnt(%d) from dt\n",
			data->irqcnt);
		goto out_with_dma_free;
	}

	dev_dbg(data->dev,
		"The number of S2MPUFD interrupt : %d\n",
		data->irqcnt);

	for (i = 0; i < data->irqcnt; i++) {
		data->irq[i] = irq_of_parse_and_map(data->dev->of_node, i);
		if (!data->irq[i]) {
			dev_err(data->dev,
				"Fail to get irq(%d) from dt\n",
				data->irq[i]);
			ret = -EINVAL;
			goto out_with_dma_free;
		}
		ret = devm_request_threaded_irq(data->dev,
						data->irq[i],
						exynos_s2mpufd_irq_handler,
						exynos_s2mpufd_irq_handler_thread,
						IRQF_ONESHOT,
						pdev->name,
						data);
		if (ret) {
			dev_err(data->dev,
				"Fail to request IRQ handler. ret(%d) irq(%d)\n",
				ret, data->irq[i]);
			goto out_with_dma_free;
		}
	}
	ret = exynos_hvc(HVC_CMD_INIT_S2MPUFD,
			data->fail_info_pa,
			data->ch_num,
			sizeof(struct s2mpufd_fail_info),
			0);
	if (ret) {
		switch (ret) {
		case S2MPUFD_ERROR_INVALID_CH_NUM:
			dev_err(data->dev,
				"The channel number(%d) defined in DT is invalid\n",
				data->ch_num);
			break;
		case S2MPUFD_ERROR_INVALID_FAIL_INFO_SIZE:
			dev_err(data->dev,
				"The size of struct s2mpufd_fail_info(%#lx) is invalid\n",
				sizeof(struct s2mpufd_fail_info));
			break;
		default:
			dev_err(data->dev,
				"Unknown error from SMC. ret[%#x]\n",
				ret);
			break;
		}
		ret = -EINVAL;
		goto out;
	}

#ifdef CONFIG_EXYNOS_S2MPUFD_ILLEGAL_READ_LOGGING
	data->info_flag = STR_INFO_FLAG;
#endif

	dev_info(data->dev, "Exynos S2MPUFD driver probe done!\n");

	return 0;

out_with_dma_free:
	platform_set_drvdata(pdev, NULL);

	dma_free_coherent(data->dev,
			  sizeof(struct s2mpufd_fail_info) *
			  data->ch_num,
			  data->fail_info,
			  data->fail_info_pa);

	data->fail_info = NULL;
	data->fail_info_pa = 0;

	data->ch_num = 0;
	data->irqcnt = 0;
	data->info_flag = 0;

out:
	return ret;
}

static int exynos_s2mpufd_remove(struct platform_device *pdev)
{
	struct s2mpufd_info_data *data = platform_get_drvdata(pdev);
	int i;

	platform_set_drvdata(pdev, NULL);

	if (data->fail_info) {
		dma_free_coherent(data->dev,
				  sizeof(struct s2mpufd_fail_info) *
				  data->ch_num,
				  data->fail_info,
				  data->fail_info_pa);

		data->fail_info = NULL;
		data->fail_info_pa = 0;
	}

	for (i = 0; i < data->ch_num; i++)
		data->irq[i] = 0;

	data->ch_num = 0;
	data->irqcnt = 0;
	data->info_flag = 0;

	return 0;
}

static const struct of_device_id exynos_s2mpufd_of_match_table[] = {
	{ .compatible = "samsung,exynos-s2mpufd", },
	{ },
};

static struct platform_driver exynos_s2mpufd_driver = {
	.probe = exynos_s2mpufd_probe,
	.remove = exynos_s2mpufd_remove,
	.driver = {
		.name = "exynos-s2mpufd",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_s2mpufd_of_match_table),
	}
};

static int __init exynos_s2mpufd_init(void)
{
	return platform_driver_register(&exynos_s2mpufd_driver);
}

static void __exit exynos_s2mpufd_exit(void)
{
	platform_driver_unregister(&exynos_s2mpufd_driver);
}

core_initcall(exynos_s2mpufd_init);
module_exit(exynos_s2mpufd_exit);

MODULE_DESCRIPTION("Exynos S2MPU Fail Detector(S2MPUFD) driver");
MODULE_AUTHOR("<sheung.kim@samsung.com>");
MODULE_LICENSE("GPL");
