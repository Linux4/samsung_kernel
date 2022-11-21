/*
 * Copyright (C) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <soc/qcom/smsm.h>

#define SUSPEND	0x1
#define RESUME	0x0

/**
 * struct smem_sec_hw_info_type - for SMEM_ID_VENDOR0
 * @afe_rx/tx_good :	Request from CP system team
 * @afe_rx/tx_mute :	Request from CP system team
 */
struct smem_afe_log_type {
    uint64_t afe_rx_good;
    uint64_t afe_rx_mute;
    uint64_t afe_tx_good;
    uint64_t afe_tx_mute;
};

struct sec_smem_id_vendor0_type {
	uint32_t ddr_vendor;
	uint32_t reserved;
	struct smem_afe_log_type afe_log;
};

/**
 * struct smem_sec_hw_info_type - for SMEM_ID_VENDOR1
 * @hw_rev:	        Samsung Board HW Revision
 * @ap_suspended:   Indicate whether device goes to suspend mode
 */
struct sec_smem_hw_info_type {
	uint64_t hw_rev;
	uint64_t ap_suspended;
};

struct sec_smem_id_vendor1_type {
	struct sec_smem_hw_info_type hw_info;
};

static int sec_smem_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sec_smem_id_vendor1_type *vendor1 = platform_get_drvdata(pdev);

	vendor1->hw_info.ap_suspended = SUSPEND;

	pr_debug("%s : smem_sec_hw_info->ap_suspended(%d)\n",
			__func__, (uint32_t)vendor1->hw_info.ap_suspended);
	return 0;
}

static int sec_smem_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sec_smem_id_vendor1_type *vendor1 = platform_get_drvdata(pdev);

	vendor1->hw_info.ap_suspended = RESUME;

	pr_debug("%s : smem_sec_hw_info->ap_suspended(%d)\n",
			__func__, (uint32_t)vendor1->hw_info.ap_suspended);
	return 0;
}

static int sec_smem_probe(struct platform_device *pdev)
{
	struct sec_smem_id_vendor1_type *sec_smem_hw_info;

	sec_smem_hw_info = smem_find(SMEM_ID_VENDOR1,
						sizeof(struct sec_smem_id_vendor1_type),
						0, SMEM_ANY_HOST_FLAG);

	if (!sec_smem_hw_info) {
		pr_err("%s size(%u): smem_sec_hw_info alloc error\n", __func__,
				(unsigned int)sizeof(struct sec_smem_id_vendor1_type));
		return -EINVAL;
	}

	platform_set_drvdata(pdev, sec_smem_hw_info);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id sec_smem_dt_ids[] = {
	{ .compatible = "samsung,sec-smem" },
	{ }
};
MODULE_DEVICE_TABLE(of, sec_smem_dt_ids);
#endif /* CONFIG_OF */

static const struct dev_pm_ops sec_smem_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sec_smem_suspend, sec_smem_resume)
};

struct platform_driver sec_smem_driver = {
	.probe		= sec_smem_probe,
	.driver		= {
		.name = "sec_smem",
		.owner	= THIS_MODULE,
		.pm = &sec_smem_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = sec_smem_dt_ids,
#endif
	},
};

static int __init sec_smem_init(void)
{
	int err;

	err = platform_driver_register(&sec_smem_driver);

	if (err)
		pr_err("%s: Failed to register platform driver: %d \n", __func__, err);

	return 0;
}

static void __exit sec_smem_exit(void)
{
	platform_driver_unregister(&sec_smem_driver);
}

device_initcall(sec_smem_init);
module_exit(sec_smem_exit);

MODULE_DESCRIPTION("SEC SMEM");
MODULE_LICENSE("GPL v2");
