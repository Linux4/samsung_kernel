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

#define MAX_REQ_TYPE_INDEX  5   // sbc, cmd, data, stop, busy
#define MAX_ERR_TYPE_INDEX  2   // crc, timeout
#define MAX_LOG_INDEX   (MAX_REQ_TYPE_INDEX * MAX_ERR_TYPE_INDEX)

#define SD_SBC_OFFSET 0
#define SD_CMD_OFFSET 2
#define SD_DATA_OFFSET 4
#define SD_STOP_OFFSET 6
#define SD_BUSY_OFFSET 8

#define STATUS_MASK (R1_ERROR | R1_CC_ERROR | R1_CARD_ECC_FAILED | \
		R1_WP_VIOLATION | R1_OUT_OF_RANGE)

struct mmc_sd_sec_err_info {
	char type[MAX_REQ_TYPE_INDEX];  // sbc, cmd, data, stop, busy
	int err_type;
	u32 status;
	u64 first_issue_time;
	u64 last_issue_time;
	u32 count;
};

struct mmc_sd_sec_status_err_info {
	u32 ge_cnt;	// status[19] : general error or unknown error
	u32 cc_cnt;	// status[20] : internal card controller error
	u32 ecc_cnt;	// status[21] : ecc error
	u32 wp_cnt;	// status[26] : write protection error
	u32 oor_cnt;	// status[31] : out of range error
	u32 noti_cnt;	// uevent notification count
};

struct mmc_sd_sec_device_info {
	unsigned long tstamp_last_cmd;
	struct mmc_sd_sec_err_info err_info[MAX_LOG_INDEX];
	struct mmc_sd_sec_status_err_info status_err;
	struct mmc_sd_sec_err_info saved_err_info[MAX_LOG_INDEX];
	struct mmc_sd_sec_status_err_info saved_status_err;
};

extern struct mmc_sd_sec_device_info sdi;

void mmc_sd_sec_clear_err_count(void);
void mmc_sd_sec_check_req_err(struct mmc_host *host, struct mmc_request *mrq);
void sd_sec_set_features(struct mmc_host *host);

#endif

