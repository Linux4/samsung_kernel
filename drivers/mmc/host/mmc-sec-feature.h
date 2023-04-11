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

/* Only [0:4] bits in response are reserved. The other bits shouldn't be used */
#define HALT_UNHALT_ERR		0x00000001
#define CQ_EN_DIS_ERR		0x00000002
#define RPMB_SWITCH_ERR		0x00000004
#define HW_RST			0x00000008
#define MMC_ERR_MASK		(HALT_UNHALT_ERR | CQ_EN_DIS_ERR |\
				RPMB_SWITCH_ERR | HW_RST)

void sd_sec_set_features(struct mmc_host *host, struct platform_device *pdev);
void sd_sec_detect_interrupt(struct mmc_host *host);
void sd_sec_check_cd_gpio(int det_status);
void mmc_sec_log_err_count(u32 status);
void mmc_sec_set_features(struct mmc_host *host);
void mmc_sd_sec_check_req_err(struct mmc_host *host, struct mmc_request *mrq);
#endif
