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

#include <linux/platform_device.h>
#include "mtk-mmc.h"

void mmc_set_sec_features(struct platform_device *pdev);
void mmc_sec_register_vendor_hooks(void);
void sd_sec_check_req_err(struct msdc_host *host, struct mmc_request *mrq);

#define MAX_REQ_TYPE_INDEX  5	// sbc, cmd, data, stop, busy
#define MAX_ERR_TYPE_INDEX  2	// crc, timeout
#define MAX_LOG_INDEX   (MAX_REQ_TYPE_INDEX * MAX_ERR_TYPE_INDEX)

#define SD_SBC_OFFSET 0
#define SD_CMD_OFFSET 2
#define SD_DATA_OFFSET 4
#define SD_STOP_OFFSET 6
#define SD_BUSY_OFFSET 8

#define SD_STATUS_MASK (R1_ERROR | R1_CC_ERROR | R1_CARD_ECC_FAILED | \
		     R1_WP_VIOLATION | R1_OUT_OF_RANGE)

struct sd_sec_err_info {
	char type[MAX_REQ_TYPE_INDEX];	// sbc, cmd, data, stop, busy
	int err_type;
	u32 status;
	u64 first_issue_time;
	u64 last_issue_time;
	u32 count;
};

struct sd_sec_status_err_info {
	u32 ge_cnt;		// status[19] : general error or unknown error
	u32 cc_cnt;		// status[20] : internal card controller error
	u32 ecc_cnt;	// status[21] : ecc error
	u32 wp_cnt;		// status[26] : write protection error
	u32 oor_cnt;	// status[31] : out of range error
	u32 noti_cnt;	// uevent notification count
};

struct sd_sec_vendor_info {
	bool failed_init;
	unsigned long tstamp_last_cmd;
	struct sd_sec_err_info err_info[MAX_LOG_INDEX];
	struct sd_sec_status_err_info status_err;
};

#endif