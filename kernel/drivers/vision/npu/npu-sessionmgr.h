/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_SESSIONMGR_H_
#define _NPU_SESSIONMGR_H_

#include <linux/time.h>
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
#include <linux/list.h>
#include <linux/hashtable.h>
#endif
#include "npu-session.h"
#include "npu-util-common.h"
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
#include "vs4l.h"
#endif

#define NPU_MAX_SESSION 128
#define NPU_MAX_PROFILE_SESSION 32

#define NPU_MAX_CORES_ALLOWED	11 /* 1 greather than max(10) to ease indexing */

#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
struct npu_cmdq_progress {
	s64 deadline;
	struct list_head list;
	u8 infer_freq_index;
	bool freq_changed;
	u8 buff_cnt;
	u8 table_index;
	u32 total_time;
	struct npu_session *session;
};
#endif

struct npu_sessionmgr {
	struct npu_session *session[NPU_MAX_SESSION];
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	u32 cmdq_list_changed_from;
	struct npu_cmdq_progress cmdqp[NPU_MAX_SESSION];
	struct list_head cmdq_head;
	struct list_head active_session_list;
	struct mutex active_list_lock;
	struct mutex cmdq_lock;
	struct mutex cancel_lock;
	struct mutex freq_set_lock;
	struct mutex model_lock;
	atomic_t freq_index_cur;
	atomic_t queue_cnt;
	struct exynos_pm_qos_request npu_qos_req_dnc;
	struct exynos_pm_qos_request npu_qos_req_npu0;
	struct exynos_pm_qos_request npu_qos_req_npu1;
	struct exynos_pm_qos_request npu_qos_req_dsp;
	struct exynos_pm_qos_request npu_qos_req_mif;
	struct exynos_pm_qos_request npu_qos_req_int;
	struct exynos_pm_qos_request npu_qos_req_mif_for_bw;
	struct exynos_pm_qos_request npu_qos_req_int_for_bw;

	u32 time_for_setting_freq;
	u32 dsp_flag;
	u32 npu_flag;

	DECLARE_HASHTABLE(model_info_hash_head, 8);
#endif

	unsigned long state;
	u32 npu_hw_cnt_saved, dsp_hw_cnt_saved;
	struct npu_util_bitmap unified_id_map;
	u32 unified_id_map_cnt[NPU_MAX_SESSION];
	atomic_t session_cnt;
	struct mutex mlock;
};

int npu_sessionmgr_probe(struct npu_sessionmgr *sessionmgr);
/*
int npu_sessionmgr_start(struct npu_sessionmgr *sessionmgr);
int npu_sessionmgr_stop(struct npu_sessionmgr *sessionmgr);
int npu_sessionmgr_close(struct npu_sessionmgr *sessionmgr);
*/

int npu_sessionmgr_regID(struct npu_sessionmgr *sessionmgr, struct npu_session *session);
int npu_sessionmgr_unregID(struct npu_sessionmgr *sessionmgr, struct npu_session *session);
int npu_sessionmgr_open(struct npu_sessionmgr *sessionmgr);
int npu_sessionmgr_set_unifiedID(struct npu_session *session);
void npu_sessionmgr_unset_unifiedID(struct npu_session *session);
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
struct npu_session *get_session(struct npu_sessionmgr *sessionmgr, int uid);
int npu_reduce_mif_all_session(struct npu_sessionmgr *sessionmgr);
int npu_sessionmgr_release(struct npu_sessionmgr *sessionmgr);
void npu_sessionmgr_init_vars(struct npu_sessionmgr *sessionmgr);
#endif
#endif
