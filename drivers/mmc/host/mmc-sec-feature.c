// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Storage Driver <storage.sec@samsung.com>
 */

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/device.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/sec_class.h>

#include "dw_mmc-exynos.h"
#include "mmc-sec-feature.h"
#include "mmc-sec-sysfs.h"

struct mmc_sec_sdcard_op_info sd_op_info;

struct mmc_sec_sdcard_err_info sd_err_info[2][10];
struct mmc_sec_status_err_info sd_status_err_info[2];

void mmc_sec_detect_interrupt(struct dw_mci *host)
{
	struct mmc_host *mmc;

	if (sd_op_info.card_detect_cnt < 0x7FFFFFFF)
		sd_op_info.card_detect_cnt++;

	if (host->slot && host->slot->mmc) {
		mmc = host->slot->mmc;
		mmc->trigger_card_event = true;
	}
}

void mmc_set_sec_features(struct platform_device *pdev)
{
	struct dw_mci *host = dev_get_drvdata(&pdev->dev);
	struct device_node *np = host->dev->of_node;
	struct dw_mci_exynos_priv_data *priv = host->priv;

	if (of_get_property(np, "sec-sd-slot-type", NULL))
		of_property_read_u32(np,
				"sec-sd-slot-type", &priv->sec_sd_slot_type);
	else {
		if (priv->cd_gpio != -1) /* treat default SD slot if cd_gpio is defined */
			priv->sec_sd_slot_type = SEC_HOTPLUG_SD_SLOT;
		else
			priv->sec_sd_slot_type = -1;
	}

	mmc_sec_init_sysfs(host);
}
