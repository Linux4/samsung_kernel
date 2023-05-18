// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Storage Driver <storage.sec@samsung.com>
 */

#ifndef __UFS_SEC_FEATURE_H__
#define __UFS_SEC_FEATURE_H__

#include "ufshcd.h"
#include "ufshci.h"

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

/* unique number */
#define UFS_UN_20_DIGITS 20
#define UFS_UN_MAX_DIGITS (UFS_UN_20_DIGITS + 1) //current max digit + 1

#define SERIAL_NUM_SIZE 7

#define UFS_WB_ISSUED_SIZE_CNT_MAX 4

/* the user LU for mtk solution is set as 2 */
#define UFS_USER_LUN 2
#define UFS_UPIU_MAX_GENERAL_LUN 8

struct ufs_vendor_dev_info {
	struct ufs_hba *hba;
	char unique_number[UFS_UN_MAX_DIGITS];
	u8 lifetime;
	unsigned int lc_info;
};

struct ufs_sec_cmd_info {
	u8 opcode;
	u32 lba;
	int transfer_len;
	u8 lun;
};

enum ufs_sec_wb_state {
	WB_OFF = 0,
	WB_ON_READY,
	WB_OFF_READY,
	WB_ON,
	NR_WB_STATE
};

struct ufs_sec_wb_info {
	struct ufs_hba *hba;
	bool support;		/* feature support and enabled */
	bool setup_done;		/* setup is done or not */
	bool wb_off;			/* WB off or not */
	atomic_t wb_off_cnt;		/* WB off count */

	enum ufs_sec_wb_state state;	/* current state */
	unsigned long state_ts;		/* current state timestamp */

	int up_threshold_block;		/* threshold for WB on : block(4KB) count */
	int up_threshold_rqs;		/*               WB on : request count */
	int down_threshold_block;	/* threshold for WB off : block count */
	int down_threshold_rqs;		/*               WB off : request count */

	int disable_threshold_lt;	/* LT threshold that WB is not allowed */

	int on_delay;			/* WB on delay for WB_ON_READY -> WB_ON */
	int off_delay;			/* WB off delay for WB_OFF_READY -> WB_OFF */

	/* below values will be used when (wb_off == true) */
	int lp_up_threshold_block;	/* threshold for WB on : block(4KB) count */
	int lp_up_threshold_rqs;	/*               WB on : request count */
	int lp_down_threshold_block;	/* threshold for WB off : block count */
	int lp_down_threshold_rqs;	/*               WB off : request count */
	int lp_on_delay;		/* on_delay multiplier when (wb_off == true) */
	int lp_off_delay;		/* off_delay multiplier when (wb_off == true) */

	int current_block;		/* current block counts in WB_ON state */
	int current_rqs;		/* request counts in WB_ON */

	int curr_issued_min_block;		/* min. issued block count */
	int curr_issued_max_block;		/* max. issued block count */
	unsigned int curr_issued_block;	/* amount issued block count during current WB_ON session */
	/* volume count of amount issued block per WB_ON session */
	unsigned int issued_size_cnt[UFS_WB_ISSUED_SIZE_CNT_MAX];

	unsigned int total_issued_mb;	/* amount issued Write Size(MB) in all WB_ON */

	struct workqueue_struct *wb_workq;
	struct work_struct on_work;
	struct work_struct off_work;
};

void ufs_set_sec_features(struct ufs_hba *hba);
void ufs_sec_get_health_desc(struct ufs_hba *hba);
void ufs_sec_register_vendor_hooks(void);
void ufs_sec_remove_features(struct ufs_hba *hba);
void ufs_sec_feature_config(struct ufs_hba *hba);
void ufs_sec_op_err_check(struct ufs_hba *hba, enum ufs_event_type evt, void *data);
void ufs_sec_check_hwrst_cnt(void);
void ufs_sec_print_err_info(struct ufs_hba *hba);
void ufs_sec_wb_force_off(struct ufs_hba *hba);
#endif
