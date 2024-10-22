/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_DRM_LOWPOWER_H_
#define _MTK_DRM_LOWPOWER_H_

#include <drm/drm_crtc.h>
#include <linux/pm_qos.h>
#include "mtk_drm_crtc.h"

struct mtk_idle_private_data {
	//the target cpu bind to idlemgr
	unsigned int cpu_mask;
	//min freq settings, unit of HZ
	unsigned int cpu_freq;
	//cpu dma latency control of c-state
	int cpu_dma_latency;
	//vblank off async is supported or not
	bool hw_async;
	bool vblank_async;
	bool sram_sleep;
};

struct mtk_drm_idlemgr_context {
	unsigned long long idle_check_interval;
	unsigned long long idlemgr_last_kick_time;
	unsigned long long enter_idle_ts;
	unsigned int enterulps;
	int session_mode_before_enter_idle;
	int is_idle;
	int cur_lp_cust_mode;
	bool wb_entered;
	struct mtk_idle_private_data priv;
};

struct mtk_drm_idlemgr_perf {
	unsigned long long enter_total_cost; //us
	unsigned long long enter_max_cost; //us
	unsigned long long enter_min_cost; //us
	unsigned long long leave_total_cost; //us
	unsigned long long leave_max_cost; //us
	unsigned long long leave_min_cost; //us
	unsigned long long count;
	unsigned long long cpu_total_bind;
	unsigned long long cpu_bind_count;
	unsigned long long cpu_total_unbind;
	atomic_t detail;
};

struct mtk_drm_idlemgr {
	struct task_struct *idlemgr_task;
	struct task_struct *kick_task;
	struct task_struct *async_vblank_task;
	struct task_struct *async_handler_task;
	wait_queue_head_t idlemgr_wq;
	wait_queue_head_t kick_wq;
	wait_queue_head_t async_vblank_wq;
	wait_queue_head_t async_handler_wq;
	wait_queue_head_t async_event_wq;
	atomic_t idlemgr_task_active;
	atomic_t kick_task_active;
	atomic_t async_vblank_active;
	//async is only enabled when enter/leave idle
	atomic_t async_enabled;
	//async event reference count
	atomic_t async_ref;
	//lock protection of async_cb_list management
	spinlock_t async_lock;
	//maintain cmdq_pkt to be complete and free
	struct list_head async_cb_list;
	//async_cb_list length
	atomic_t async_cb_count;
	atomic_t async_cb_pending;
	struct pm_qos_request cpu_qos_req;
	struct drm_framebuffer *wb_fb;
	struct drm_framebuffer *wb_fb_r;
	dma_addr_t wb_buffer_iova;
	dma_addr_t wb_buffer_r_iova;
	struct mtk_drm_idlemgr_context *idlemgr_ctx;
	struct mtk_drm_idlemgr_perf *perf;
};

struct mtk_drm_async_cb_data {
	struct drm_crtc *crtc;
	struct cmdq_pkt *handle;
	bool free_handle;
	unsigned int user_id;
};

struct mtk_drm_async_cb {
	struct mtk_drm_async_cb_data *data;
	struct list_head link;
};

enum mtk_drm_async_user_id {
	USER_TRIG_LOOP = 0xf001,
	USER_HW_BLOCK,
	USER_ADDON_CONNECT_MODULE,
	USER_ADDON_DISCONNECT_MODULE,
	USER_ADDON_CONNECT_CONNECTOR, //0xf005
	USER_RESTORE_PLANE,
	USER_CONFIG_PATH,
	USER_STOP_CRTC,
	USER_ATF_INSTR,
	USER_VBLANK_OFF, //0xf00a
	USER_COMP_RST,
};

enum mtk_drm_cpu_cmd {
	MTK_DRM_CPU_CMD_FREQ,
	MTK_DRM_CPU_CMD_MASK,
	MTK_DRM_CPU_CMD_LATENCY,
};

//check if async is enabled
bool mtk_drm_idlemgr_get_async_status(struct drm_crtc *crtc);
//check if sram is enabled
bool mtk_drm_idlemgr_get_sram_status(struct drm_crtc *crtc);

/* flush cmdq pkt with async wait,
 * this can be applied when user wants to free cmdq pkt by async task.
 */
int mtk_drm_idle_async_flush(struct drm_crtc *crtc,
	unsigned int user_id, struct cmdq_pkt *cmdq_handle);

/* flush cmdq pkt with async wait,
 * this can be applied when user customization of:
 *    1. free cmdq pkt or not after async job done.
 *    2. use private callback or default callback.
 */
int mtk_drm_idle_async_flush_cust(struct drm_crtc *crtc,
	unsigned int user_id, struct cmdq_pkt *cmdq_handle,
	bool free_handle, cmdq_async_flush_cb cb);

/* maintain async event reference count,
 * only when reference count is 0, async wait can be finished.
 */
void mtk_drm_idlemgr_async_get(struct drm_crtc *crtc, unsigned int user_id);
void mtk_drm_idlemgr_async_put(struct drm_crtc *_crtc, unsigned int user_id);

/* async wait of gce job done */
void mtk_drm_idlemgr_async_complete(struct drm_crtc *crtc, unsigned int user_id,
	struct mtk_drm_async_cb_data *cb_data);

void mtk_drm_idlemgr_kick(const char *source, struct drm_crtc *crtc,
			  int need_lock);
bool mtk_drm_is_idle(struct drm_crtc *crtc);

int mtk_drm_idlemgr_init(struct drm_crtc *crtc, int index);
unsigned int mtk_drm_set_idlemgr(struct drm_crtc *crtc, unsigned int flag,
				 bool need_lock);
unsigned long long
mtk_drm_set_idle_check_interval(struct drm_crtc *crtc,
				unsigned long long new_interval);
unsigned long long
mtk_drm_get_idle_check_interval(struct drm_crtc *crtc);

void mtk_drm_idlemgr_kick_async(struct drm_crtc *crtc);

/* enable cmd panel performance monitor tool */
void mtk_drm_idlemgr_monitor(bool enable, struct drm_crtc *crtc);
/* dump cmd panel performance */
void mtk_drm_idlemgr_perf_dump(struct drm_crtc *crtc);
/* enable cmd panel performance detail flow trace */
void mtk_drm_idlemgr_async_perf_detail_control(bool enable,
				struct drm_crtc *crtc);
/* adjust cmd panel idle thread cpu settings */
void mtk_drm_idlemgr_cpu_control(struct drm_crtc *crtc,
				int cmd, unsigned int data);
/* enable cmd panel idle async function */
void mtk_drm_idlemgr_async_control(struct drm_crtc *crtc, bool enable);
/* enable cmd panel sram sleep function */
void mtk_drm_idlemgr_sram_control(struct drm_crtc *crtc, bool sleep);
/* enable cmd panel perf aee dump */
void mtk_drm_idlegmr_perf_aee_control(unsigned int timeout);
void mtk_drm_clear_async_cb_list(struct drm_crtc *crtc);

bool mtk_drm_idlemgr_wb_is_entered(struct mtk_drm_crtc *mtk_crtc);
void mtk_drm_idlemgr_wb_enter(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle);
void mtk_drm_idlemgr_wb_leave(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle);
void mtk_drm_idlemgr_wb_leave_post(struct mtk_drm_crtc *mtk_crtc);
void mtk_drm_idlemgr_wb_fill_buf(struct drm_crtc *crtc, int value);
void mtk_drm_idlemgr_wb_test(int value);

#endif
