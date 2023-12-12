// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature : sysfs-nodes
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Storage Driver <storage.sec@samsung.com>
 */
#ifndef __UFS_SEC_SYSFS_H__
#define __UFS_SEC_SYSFS_H__

#include "ufs-sec-feature.h"

#include <linux/sec_class.h>
#include <linux/sec_debug.h>

extern struct ufs_sec_feature_info ufs_sec_features;

/* UFSHCD states : in ufshcd.c */
enum {
	UFSHCD_STATE_RESET,
	UFSHCD_STATE_ERROR,
	UFSHCD_STATE_OPERATIONAL,
	UFSHCD_STATE_EH_SCHEDULED_FATAL,
	UFSHCD_STATE_EH_SCHEDULED_NON_FATAL,
};

#define SEC_UFS_ERR_COUNT_INC(count, max) ((count) += ((count) < (max)) ? 1 : 0)

/* SEC error info : begin */
struct SEC_UFS_op_count {
	unsigned int HW_RESET_count;
	unsigned int link_startup_count;
	unsigned int Hibern8_enter_count;
	unsigned int Hibern8_exit_count;
	unsigned int op_err;
};

struct SEC_UFS_UIC_cmd_count {
	u8 DME_GET_err;
	u8 DME_SET_err;
	u8 DME_PEER_GET_err;
	u8 DME_PEER_SET_err;
	u8 DME_POWERON_err;
	u8 DME_POWEROFF_err;
	u8 DME_ENABLE_err;
	u8 DME_RESET_err;
	u8 DME_END_PT_RST_err;
	u8 DME_LINK_STARTUP_err;
	u8 DME_HIBER_ENTER_err;
	u8 DME_HIBER_EXIT_err;
	u8 DME_TEST_MODE_err;
	unsigned int UIC_cmd_err;
};

struct SEC_UFS_UIC_err_count {
	u8 PA_ERR_cnt;
	u8 DL_PA_INIT_ERROR_cnt;
	u8 DL_NAC_RECEIVED_ERROR_cnt;
	u8 DL_TC_REPLAY_ERROR_cnt;
	u8 NL_ERROR_cnt;
	u8 TL_ERROR_cnt;
	u8 DME_ERROR_cnt;
	unsigned int UIC_err;
	unsigned int PA_ERR_linereset;
	unsigned int PA_ERR_lane[3];
};

struct SEC_UFS_Fatal_err_count {
	u8 DFE; // Device_Fatal
	u8 CFE; // Controller_Fatal
	u8 SBFE; // System_Bus_Fatal
	u8 CEFE; // Crypto_Engine_Fatal
	u8 LLE; // Link Lost
	unsigned int Fatal_err;
};

struct SEC_UFS_UTP_count {
	u8 UTMR_query_task_count;
	u8 UTMR_abort_task_count;
	u8 UTMR_logical_reset_count;
	u8 UTR_read_err;
	u8 UTR_write_err;
	u8 UTR_sync_cache_err;
	u8 UTR_unmap_err;
	u8 UTR_etc_err;
	unsigned int UTP_err;
};

struct SEC_UFS_QUERY_count {
	u8 NOP_err;
	u8 R_Desc_err;
	u8 W_Desc_err;
	u8 R_Attr_err;
	u8 W_Attr_err;
	u8 R_Flag_err;
	u8 Set_Flag_err;
	u8 Clear_Flag_err;
	u8 Toggle_Flag_err;
	unsigned int Query_err;
};

struct SEC_SCSI_SENSE_count {
	unsigned int scsi_medium_err;
	unsigned int scsi_hw_err;
};

struct ufs_sec_err_info {
	struct SEC_UFS_op_count op_count;
	struct SEC_UFS_UIC_cmd_count UIC_cmd_count;
	struct SEC_UFS_UIC_err_count UIC_err_count;
	struct SEC_UFS_Fatal_err_count Fatal_err_count;
	struct SEC_UFS_UTP_count UTP_count;
	struct SEC_UFS_QUERY_count Query_count;
	struct SEC_SCSI_SENSE_count Sense_count;
};

#define get_err_member(member) ufs_sec_features.ufs_err->member
/* SEC error info : end */

#define SEC_UFS_DATA_ATTR_RO(name, fmt, args...)                                         \
static ssize_t name##_show(struct device *dev, struct device_attribute *attr, char *buf) \
{                                                                                        \
	return sprintf(buf, fmt, args);                                                  \
}                                                                                        \
static DEVICE_ATTR_RO(name)

void ufs_sec_add_sysfs_nodes(struct ufs_hba *hba);
void ufs_sec_remove_sysfs_nodes(struct ufs_hba *hba);
#endif
