// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *      Storage Driver <storage.sec@samsung.com>
 */

#ifndef __MMC_SEC_FEATURE_H__
#define __MMC_SEC_FEATURE_H__

#include "sdhci-pltfm.h"

void mmc_set_sec_features(struct mmc_host *host, struct platform_device *pdev);
void mmc_sec_save_tuning_phase(u8 phase);

#if IS_ENABLED(CONFIG_MMC_SDHCI_MSM)
void mmc_sec_check_request_error(struct sdhci_host *host,
		struct mmc_request *mrq);
#endif

struct mmc_sec_op_info {
	struct mmc_host *host;
	unsigned int card_detect_cnt;
	bool tray_status;
	unsigned int sd_bus_speed;
	int sd_slot_type;
	u8 saved_tuning_phase;
	unsigned long tstamp_last_cmd;
	struct work_struct noti_work;
};

enum {
	SEC_NO_DET_SD_SLOT = 0,
	SEC_HOTPLUG_SD_SLOT,
	SEC_HYBRID_SD_SLOT,
};
#endif
