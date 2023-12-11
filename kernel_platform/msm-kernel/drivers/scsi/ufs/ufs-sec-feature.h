// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *      Storage Driver <storage.sec@samsung.com>
 */

#ifndef __UFS_SEC_FEATURE_H__
#define __UFS_SEC_FEATURE_H__

#include "ufshcd.h"
#include "ufshci.h"

/*unique number*/
#define UFS_UN_20_DIGITS 20
#define UFS_UN_MAX_DIGITS (UFS_UN_20_DIGITS + 1)

#define SERIAL_NUM_SIZE 7

struct ufs_vendor_dev_info {
	struct ufs_hba *hba;
	char unique_number[UFS_UN_MAX_DIGITS];
	u8 lt;
	unsigned int lc;
};

struct ufs_sec_cmd_info {
	u8 opcode;
	u32 lba;
	int transfer_len;
	u8 lun;
};

struct ufs_sec_feature_info {
	struct ufs_vendor_dev_info *vdi;
	struct ufs_sec_err_info *ufs_err;

	u32 last_ucmd;
	bool ucmd_complete;

	enum query_opcode last_qcmd;
	enum dev_cmd_type qcmd_type;
	bool qcmd_complete;
	bool skip_flush;
};

/* call by vendor module */
void ufs_sec_get_health_desc(struct ufs_hba *hba);
inline bool ufs_sec_is_err_cnt_allowed(void);
void ufs_sec_inc_op_err(struct ufs_hba *hba, enum ufs_event_type evt, void *data);
void ufs_sec_print_err_info(struct ufs_hba *hba);
void ufs_sec_init_logging(struct device *dev);
void ufs_sec_set_features(struct ufs_hba *hba);
void ufs_sec_remove_features(struct ufs_hba *hba);
void ufs_sec_register_vendor_hooks(void);

#endif
