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
#include "include/sec_smem.h"


#define SUSPEND	0x1
#define RESUME	0x0

#define MAX_DDR_VENDOR 16

static char* lpddr4_manufacture_name[MAX_DDR_VENDOR] =
	{"NA",
	"SEC"/* Samsung */,
	"NA",
	"NA",
	"NA",
	"NAN" /* Nanya */,
	"HYN" /* SK hynix */,
	"NA",
	"WIN" /* Winbond */,
	"ESM" /* ESMT */,
	"NA",
	"NA",
	"NA",
	"NA",
	"NA",
	"MIC" /* Micron */,};

char* get_ddr_vendor_name(void)
{
	unsigned size;
	struct sec_smem_id_vendor0_type *vendor0 = NULL;

	vendor0 = smem_get_entry(SMEM_ID_VENDOR0, &size,
					SMEM_APPS, SMEM_ANY_HOST_FLAG);

	if (!vendor0 || !size) {
		pr_err("%s: unable to read smem entry\n", __func__);
		return 0;
	}

	return lpddr4_manufacture_name[vendor0->ddr_vendor & 0x0F];
}
EXPORT_SYMBOL(get_ddr_vendor_name);

static int sec_smem_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	sec_smem_id_vendor1_v3_t *vendor1 = platform_get_drvdata(pdev);

	vendor1->ven1_v2.ap_suspended = SUSPEND;

	pr_debug("%s : smem_vendor1 ap_suspended(%d)\n",
			__func__, (uint32_t)vendor1->ven1_v2.ap_suspended);
	return 0;
}

static int sec_smem_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	sec_smem_id_vendor1_v3_t *vendor1 = platform_get_drvdata(pdev);

	vendor1->ven1_v2.ap_suspended = RESUME;

	pr_debug("%s : smem_vendor1 ap_suspended(%d)\n",
			__func__, (uint32_t)vendor1->ven1_v2.ap_suspended);
	return 0;
}

static int sec_smem_probe(struct platform_device *pdev)
{
	sec_smem_id_vendor1_v3_t *vendor1 = NULL;
	unsigned size = 0;

	vendor1 = smem_get_entry(SMEM_ID_VENDOR1, &size,
					0, SMEM_ANY_HOST_FLAG);

	if (!vendor1) {
		pr_err("%s size(%lu, %u): SMEM_ID_VENDOR1 get entry error\n", __func__,
				sizeof(sec_smem_id_vendor1_v3_t), size);
		panic("sec_smem_probe fail");
		return -EINVAL;
	}

	platform_set_drvdata(pdev, vendor1);

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
