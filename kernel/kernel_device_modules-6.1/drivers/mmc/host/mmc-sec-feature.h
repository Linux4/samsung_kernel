// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *  Storage Driver <storage.sec@samsung.com>
 */

#ifndef __MMC_SEC_FEATURE_H__
#define __MMC_SEC_FEATURE_H__

#include <linux/platform_device.h>

void sd_sec_set_features(struct platform_device *pdev);
void sd_sec_card_event(void);
void sd_sec_check_req_err(struct msdc_host *host, struct mmc_request *mrq);

extern struct device *sec_sdcard_cmd_dev;

#endif
