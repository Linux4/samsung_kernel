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

/* unique number */
#define SERIAL_NUM_SIZE 7

#define SEC_UFS_ERR_COUNT_INC(count, max) ((count) += ((count) < (max)) ? 1 : 0)

struct SEC_UFS_op_count {
	unsigned int HW_RESET_count;
	unsigned int link_startup_count;
	unsigned int Hibern8_enter_count;
	unsigned int Hibern8_exit_count;
	unsigned int op_err;
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
	u8 DFE;         // Device_Fatal
	u8 CFE;         // Controller_Fatal
	u8 SBFE;        // System_Bus_Fatal
	u8 CEFE;        // Crypto_Engine_Fatal
	u8 LLE;         // Link Lost
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

#define SEC_MAX_LBA_LOGGING	10
#define SEC_ISSUE_REGION_STEP	(250 * 1024 / 4)	/* 250MB : 1 LBA = 4KB */
struct SEC_SCSI_SENSE_err_log {
	unsigned long issue_LBA_list[SEC_MAX_LBA_LOGGING];
	unsigned int issue_LBA_count;
	u64 issue_region_map;
};

struct ufs_sec_cmd_info {
	u8 opcode;
	u32 lba;
	int transfer_len;
	u8 lun;
};

struct ufs_sec_err_info {
	struct SEC_UFS_op_count op_count;
	struct SEC_UFS_UIC_err_count UIC_err_count;
	struct SEC_UFS_Fatal_err_count Fatal_err_count;
	struct SEC_UFS_UTP_count UTP_count;
	struct SEC_UFS_UIC_cmd_count UIC_cmd_count;
	struct SEC_UFS_QUERY_count query_count;
	struct SEC_SCSI_SENSE_count sense_count;
	struct SEC_SCSI_SENSE_err_log sense_err_log;
};

void ufs_sec_get_health_info(struct ufs_hba *hba);
void ufs_set_sec_features(struct ufs_hba *hba, u8 *str_desc_buf, u8 *desc_buf);
void ufs_remove_sec_features(struct ufs_hba *hba);
void ufs_sec_check_hwrst_cnt(void);
void ufs_sec_check_op_err(struct ufs_hba *hba, enum ufs_event_type evt, void *data);
void ufs_sec_uic_cmd_error_check(struct ufs_hba *hba, u32 cmd);
void ufs_sec_tm_error_check(u8 tm_cmd);
void ufs_sec_compl_cmd_check(struct ufs_hba *hba, struct ufshcd_lrb *lrbp);
void ufs_sec_print_err_info(struct ufs_hba *hba);
void ufs_sec_send_errinfo(struct ufs_hba *hba);


#endif
