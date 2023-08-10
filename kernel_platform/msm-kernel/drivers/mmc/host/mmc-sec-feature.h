// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *      Storage Driver <storage.sec@samsung.com>
 */

#ifndef __MMC_SEC_FEATURE_H__
#define __MMC_SEC_FEATURE_H__

#include "sdhci-pltfm.h"

void sd_sec_set_features(struct mmc_host *host, struct platform_device *pdev);
void mmc_sd_sec_check_req_err(struct mmc_host *host, struct mmc_request *mrq);

extern struct device *sec_sdcard_cmd_dev;

#endif
