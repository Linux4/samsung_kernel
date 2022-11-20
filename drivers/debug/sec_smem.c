/*
 * Copyright (C) 2016 Samsung Electronics
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
#include <linux/notifier.h>
#include <linux/qcom/sec_debug_partition.h>

#ifdef CONFIG_SEC_DEBUG_APPS_CLK_LOGGING
extern void* clk_osm_get_log_addr(void);
#endif

#define SUSPEND	0x1
#define RESUME	0x0

#define MAX_DDR_VENDOR 16

static ap_health_t *p_health;

/*
 * CONFIG_SAMSUNG_BSP
 *
 * LPDDR4(JESD209-4) Manufacturer ID
 *
 * JC-42.6 Manufacturer Identification
 * (ID) Code for Low Power Memories
 * (JEP166B)
 *
 *  0000 0000B : Reserved
 *  0000 0001B : Samsung
 *  0000 0010B : Reserved*
 *  0000 0011B : Reserved*
 *  0000 0100B : Reserved*
 *  0000 0101B : Nanya
 *  0000 0110B : SK hynix
 *  0000 0111B : Reserved*
 *  0000 1000B : Winbond
 *  0000 1001B : ESMT
 *  0000 1010B : Reserved
 *  0000 1011B : Reserved*
 *  0000 1100B : Reserved*
 *  0000 1101B : Reserved*
 *  0000 1110B : Reserved*
 *  0000 1111B : Reserved*
 *  0001 0010B : Reserved*
 *  0001 0011B : Reserved
 *  0001 1011B : Reserved*
 *  0001 1100B : Reserved*
 *  1010 1010B : Reserved*
 *  1100 0010B : Reserved*
 *  1111 1000B : Fidelix
 *  1111 1100B : Reserved*
 *  1111 1101B : AP Memory
 *  1111 1110B : Reserved*
 *  1111 1111B : Micron
 *  All others : Reserved
 *
 */

typedef enum {
	LP4_SAMSUNG = 0x1,
	LP4_NANYA = 0x5,
	LP4_HYNIX = 0x6,
	LP4_WINBOND = 0x8,
	LP4_ESMT = 0x9,
	LP4_FIDELIX = 0xF8,
	LP4_APMEMORY = 0xFD,
	LP4_MICRON = 0xFF,
} LPDDR4_DDR_MANUFACTURES;

static char *lpddr4_manufacture_name[] = {
	[LP4_SAMSUNG] = "SEC", [LP4_NANYA] = "NAN", [LP4_HYNIX] = "HYP",
	[LP4_WINBOND] = "WIN", [LP4_ESMT] = "ESM", [LP4_FIDELIX] = "FID",
	[LP4_APMEMORY] = "APM", [LP4_MICRON] = "MIC"
};

char* get_ddr_vendor_name(void)
{
	unsigned size;
	sec_smem_id_vendor0_v2_t *vendor0 = NULL;

	vendor0 = smem_get_entry(SMEM_ID_VENDOR0, &size,
					SMEM_APPS, SMEM_ANY_HOST_FLAG);

	if (!vendor0 || !size) {
		pr_err("%s: unable to read smem entry\n", __func__);
		return "NA";
	}

	if (lpddr4_manufacture_name[vendor0->ddr_vendor & 0xFF])
		return lpddr4_manufacture_name[vendor0->ddr_vendor & 0xFF];

	return "NA";
}
EXPORT_SYMBOL(get_ddr_vendor_name);

uint8_t get_ddr_revision_id_1(void)
{
	unsigned int size;
	sec_smem_id_vendor0_v2_t *vendor0 = NULL;

	vendor0 = smem_get_entry(SMEM_ID_VENDOR0, &size,
					SMEM_APPS, SMEM_ANY_HOST_FLAG);

	if (!vendor0 || !size) {
		pr_err("%s: unable to read smem entry\n", __func__);
		return 0;
	}

	return (vendor0->ddr_vendor >> 8) & 0xFF;
}
EXPORT_SYMBOL(get_ddr_revision_id_1);

uint8_t get_ddr_revision_id_2(void)
{
	unsigned int size;
	sec_smem_id_vendor0_v2_t *vendor0 = NULL;

	vendor0 = smem_get_entry(SMEM_ID_VENDOR0, &size,
					SMEM_APPS, SMEM_ANY_HOST_FLAG);

	if (!vendor0 || !size) {
		pr_err("%s: unable to read smem entry\n", __func__);
		return 0;
	}

	return (vendor0->ddr_vendor >> 16) & 0xFF;
}
EXPORT_SYMBOL(get_ddr_revision_id_2);

uint8_t get_ddr_total_density(void)
{
	unsigned int size;
	sec_smem_id_vendor0_v2_t *vendor0 = NULL;

	vendor0 = smem_get_entry(SMEM_ID_VENDOR0, &size,
					SMEM_APPS, SMEM_ANY_HOST_FLAG);

	if (!vendor0 || !size) {
		pr_err("%s: unable to read smem entry\n", __func__);
		return 0;
	}

	return (vendor0->ddr_vendor >> 24) & 0xFF;
}
EXPORT_SYMBOL(get_ddr_total_density);

uint32_t get_ddr_DSF_version(void)
{
	unsigned int size;
	sec_smem_id_vendor1_v4_t *vendor1 = NULL;

	vendor1 = smem_get_entry(SMEM_ID_VENDOR1, &size,
					0, SMEM_ANY_HOST_FLAG);

	if (!vendor1 || !size) {
		pr_err("%s: unable to read smem entry\n", __func__);
		return 0;
	}

	return vendor1->ddr_training.version;
}
EXPORT_SYMBOL(get_ddr_DSF_version);

uint16_t get_ddr_wr_eyeRect(uint32_t ch, uint32_t cs, uint32_t dq)
{
	unsigned int size;
	sec_smem_id_vendor1_v4_t *vendor1 = NULL;

	vendor1 = smem_get_entry(SMEM_ID_VENDOR1, &size,
					0, SMEM_ANY_HOST_FLAG);

	if (!vendor1 || !size) {
		pr_err("%s: unable to read smem entry\n", __func__);
		return 0;
	}

	return vendor1->ddr_training.wr_dqdqs_eye.rectangle[ch][cs][dq];
}
EXPORT_SYMBOL(get_ddr_wr_eyeRect);

uint8_t get_ddr_wr_eyeVref(uint32_t ch, uint32_t cs, uint32_t dq)
{
	unsigned int size;
	sec_smem_id_vendor1_v4_t *vendor1 = NULL;

	vendor1 = smem_get_entry(SMEM_ID_VENDOR1, &size,
					0, SMEM_ANY_HOST_FLAG);

	if (!vendor1 || !size) {
		pr_err("%s: unable to read smem entry\n", __func__);
		return 0;
	}

	return vendor1->ddr_training.wr_dqdqs_eye.vref[ch][cs][dq];
}
EXPORT_SYMBOL(get_ddr_wr_eyeVref);

uint8_t get_ddr_wr_eyeHeight(uint32_t ch, uint32_t cs, uint32_t dq)
{
	unsigned int size;
	sec_smem_id_vendor1_v4_t *vendor1 = NULL;

	vendor1 = smem_get_entry(SMEM_ID_VENDOR1, &size,
					0, SMEM_ANY_HOST_FLAG);

	if (!vendor1 || !size) {
		pr_err("%s: unable to read smem entry\n", __func__);
		return 0;
	}

	return vendor1->ddr_training.wr_dqdqs_eye.height[ch][cs][dq];
}
EXPORT_SYMBOL(get_ddr_wr_eyeHeight);

uint8_t get_ddr_wr_eyeWidth(uint32_t ch, uint32_t cs, uint32_t dq)
{
	unsigned int size;
	sec_smem_id_vendor1_v4_t *vendor1 = NULL;

	vendor1 = smem_get_entry(SMEM_ID_VENDOR1, &size,
					0, SMEM_ANY_HOST_FLAG);

	if (!vendor1 || !size) {
		pr_err("%s: unable to read smem entry\n", __func__);
		return 0;
	}

	return vendor1->ddr_training.wr_dqdqs_eye.width[ch][cs][dq];
}
EXPORT_SYMBOL(get_ddr_wr_eyeWidth);

uint8_t get_ddr_rcw_tDQSCK(uint32_t ch, uint32_t cs, uint32_t dq)
{
	unsigned size;
	sec_smem_id_vendor1_v4_t *vendor1 = NULL;
	vendor1 = smem_get_entry(SMEM_ID_VENDOR1, &size,
					0, SMEM_ANY_HOST_FLAG);

	if (!vendor1 || !size) {
		pr_err("%s: unable to read smem entry\n", __func__);
		return 0;
	}

	return vendor1->ddr_training.rcw.tDQSCK[ch][cs][dq];
}
EXPORT_SYMBOL(get_ddr_rcw_tDQSCK);

uint8_t get_ddr_wr_coarseCDC(uint32_t ch, uint32_t cs, uint32_t dq)
{
	unsigned size;
	sec_smem_id_vendor1_v4_t *vendor1 = NULL;
	vendor1 = smem_get_entry(SMEM_ID_VENDOR1, &size,
					0, SMEM_ANY_HOST_FLAG);

	if (!vendor1 || !size) {
		pr_err("%s: unable to read smem entry\n", __func__);
		return 0;
	}

	return vendor1->ddr_training.wr_dqdqs.coarse_cdc[ch][cs][dq];
}
EXPORT_SYMBOL(get_ddr_wr_coarseCDC);

uint8_t get_ddr_wr_fineCDC(uint32_t ch, uint32_t cs, uint32_t dq)
{
	unsigned size;
	sec_smem_id_vendor1_v4_t *vendor1 = NULL;
	vendor1 = smem_get_entry(SMEM_ID_VENDOR1, &size,
					0, SMEM_ANY_HOST_FLAG);

	if (!vendor1 || !size) {
		pr_err("%s: unable to read smem entry\n", __func__);
		return 0;
	}

	return vendor1->ddr_training.wr_dqdqs.fine_cdc[ch][cs][dq];
}
EXPORT_SYMBOL(get_ddr_wr_fineCDC);

static int sec_smem_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	sec_smem_id_vendor1_v4_t *vendor1 = platform_get_drvdata(pdev);

	vendor1->ven1_v2.ap_suspended = SUSPEND;

	pr_debug("%s : smem_vendor1 ap_suspended(%d)\n",
			__func__, (uint32_t)vendor1->ven1_v2.ap_suspended);
	return 0;
}

static int sec_smem_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	sec_smem_id_vendor1_v4_t *vendor1 = platform_get_drvdata(pdev);

	vendor1->ven1_v2.ap_suspended = RESUME;

	pr_debug("%s : smem_vendor1 ap_suspended(%d)\n",
			__func__, (uint32_t)vendor1->ven1_v2.ap_suspended);
	return 0;
}

static int sec_smem_probe(struct platform_device *pdev)
{
	sec_smem_id_vendor1_v4_t *vendor1 = NULL;
	unsigned size = 0;

	vendor1 = smem_get_entry(SMEM_ID_VENDOR1, &size,
					0, SMEM_ANY_HOST_FLAG);

	if (!vendor1) {
		pr_err("%s size(%lu, %u): SMEM_ID_VENDOR1 get entry error\n", __func__,
				sizeof(sec_smem_id_vendor1_v4_t), size);
		panic("sec_smem_probe fail");
		return -EINVAL;
	}
#ifdef CONFIG_SEC_DEBUG_APPS_CLK_LOGGING
	vendor1->apps_stat.clk = (void *)virt_to_phys(clk_osm_get_log_addr());
	pr_err("%s vendor1->apps_stat.clk = %p\n", __func__, vendor1->apps_stat.clk);
#endif
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

static int sec_smem_dbg_part_notifier_callback(
	struct notifier_block *nfb, unsigned long action, void *data)
{
	unsigned size = 0;
	sec_smem_id_vendor1_v4_t *vendor1 = NULL;

	switch (action) {
		case DBG_PART_DRV_INIT_DONE:
			p_health = ap_health_data_read();

			vendor1 = smem_get_entry(SMEM_ID_VENDOR1, &size,
					0, SMEM_ANY_HOST_FLAG);

			if (!vendor1) {
				pr_err("%s size(%lu, %u): SMEM_ID_VENDOR1 get entry error\n", __func__,
					sizeof(sec_smem_id_vendor1_v4_t), size);
				return NOTIFY_DONE;
			}

			vendor1->ap_health = (void *)virt_to_phys(p_health);
			break;
		default:
			return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}


static struct notifier_block sec_smem_dbg_part_notifier = {
	.notifier_call = sec_smem_dbg_part_notifier_callback,
};

static int __init sec_smem_init(void)
{
	int err;

	err = platform_driver_register(&sec_smem_driver);

	if (err)
		pr_err("%s: Failed to register platform driver: %d \n", __func__, err);

	dbg_partition_notifier_register(&sec_smem_dbg_part_notifier);

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
