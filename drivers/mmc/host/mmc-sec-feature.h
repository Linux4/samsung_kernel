// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Storage Driver <storage.sec@samsung.com>
 */
#ifndef __MMC_SEC_FEATURE_H__
#define __MMC_SEC_FEATURE_H__

#include "dw_mmc.h"
#include <linux/platform_device.h>

void mmc_set_sec_features(struct platform_device *pdev);
void mmc_sec_detect_interrupt(struct dw_mci *host);

struct mmc_sec_sdcard_op_info {
	unsigned int card_detect_cnt;
};

enum {
	SEC_NO_DET_SD_SLOT = 0,
	SEC_HOTPLUG_SD_SLOT,
	SEC_HYBRID_SD_SLOT,
};
#endif
