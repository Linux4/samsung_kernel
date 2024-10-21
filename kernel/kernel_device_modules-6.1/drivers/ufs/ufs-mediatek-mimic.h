/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */
#ifndef _UFS_MEDIATEK_MIMIC_
#define _UFS_MEDIATEK_MIMIC_

#include <linux/types.h>
#include <ufs/ufshcd.h>

/* UIC layer error flags */
enum {
	UFSM_UIC_DL_PA_INIT_ERROR = (1 << 0), /* Data link layer error */
	UFSM_UIC_DL_NAC_RECEIVED_ERROR = (1 << 1), /* Data link layer error */
	UFSM_UIC_DL_TCx_REPLAY_ERROR = (1 << 2), /* Data link layer error */
	UFSM_UIC_NL_ERROR = (1 << 3), /* Network layer error */
	UFSM_UIC_TL_ERROR = (1 << 4), /* Transport Layer error */
	UFSM_UIC_DME_ERROR = (1 << 5), /* DME error */
	UFSM_UIC_PA_GENERIC_ERROR = (1 << 6), /* Generic PA error */
};

/* Error handling flags */
enum {
	UFSM_EH_IN_PROGRESS = (1 << 0),
};

#define ufsm_eh_in_progress(h) \
	((h)->eh_flags & UFSM_EH_IN_PROGRESS)

int ufsm_wait_for_doorbell_clr(struct ufs_hba *hba,
				 u64 wait_timeout_us);

void ufsm_scsi_unblock_requests(struct ufs_hba *hba);
void ufsm_scsi_block_requests(struct ufs_hba *hba);
void ufsm_disable_intr(struct ufs_hba *hba, u32 intrs);

#endif /* _UFS_MEDIATEK_MIMIC_ */
