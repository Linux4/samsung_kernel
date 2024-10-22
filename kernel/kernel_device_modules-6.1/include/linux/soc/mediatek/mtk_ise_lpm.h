/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef __ISE_LPM_H__
#define __ISE_LPM_H__


/*
 * This is a iSE usage white list,
 * ise awake namming: ISE_XX_YY
 * XX : Host feature
 * YY : function of the feature
 */

enum mtk_ise_awake_id_t {
	ISE_PM_INIT,
	ISE_PM_UT,
	ISE_REE,
	ISE_TEE,
	ISE_AWAKE_ID_NUM
};

enum mtk_ise_awake_ack_t {
	ISE_SUCCESS = 0x0,
	ISE_ERR_WAKELOCK_DISABLE,
	ISE_ERR_UID,
	ISE_ERR_UNLOCK_BEFORE_LOCK,
	ISE_ERR_REF_CNT,
	ISE_ERR_NUM
};

enum mtk_ise_awake_ack_t mtk_ise_awake_lock(enum mtk_ise_awake_id_t mtk_ise_awake_id);
enum mtk_ise_awake_ack_t mtk_ise_awake_unlock(enum mtk_ise_awake_id_t mtk_ise_awake_id);
#endif
