/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTKFB_DEBUG_H
#define __MTKFB_DEBUG_H

#include "mtk_drm_plane.h"
#include "mtk_panel_ext.h"

#define LOGGER_BUFFER_SIZE (16 * 1024)
#define ERROR_BUFFER_COUNT 16
#define FENCE_BUFFER_COUNT 80
#define DEBUG_BUFFER_COUNT 100
#define DUMP_BUFFER_COUNT 40
#define STATUS_BUFFER_COUNT 3
#define _DRM_P_H_
#if defined(CONFIG_MT_ENG_BUILD) || !defined(CONFIG_MTK_GMO_RAM_OPTIMIZE)
#define DEBUG_BUFFER_SIZE                                                      \
	(4096 +                                                                \
	 (ERROR_BUFFER_COUNT + FENCE_BUFFER_COUNT + DEBUG_BUFFER_COUNT +       \
	  DUMP_BUFFER_COUNT + STATUS_BUFFER_COUNT) *                           \
		 LOGGER_BUFFER_SIZE)
#else
#define DEBUG_BUFFER_SIZE 10240
#endif

enum MTK_DRM_DEBUG_LOG_SWITCH_OPS {
	MTK_DRM_OTHER = 0,
	MTK_DRM_MOBILE_LOG,
	MTK_DRM_DETAIL_LOG,
	MTK_DRM_FENCE_LOG,
	MTK_DRM_IRQ_LOG,
};

enum DISP_FRAME_STATE {
	DISP_FRAME_START = 0,
	DISP_FRAME_DONE,
};

#define CUSTOMER_USE_SIMPLE_API 1
enum mtk_set_lcm_sceanario {
	SET_LCM_NONE = 0,
	SET_LCM_POWER_MODE_SWITCH,

	SET_LCM_CMDQ_AVAILABLE,

	SET_LCM_FPS_CHANGE,

	SET_LCM_HBM_CMD,

	SET_LCM_BL,

	SET_LCM_CMDQ_FRAME_DONE,

	SET_LCM_POWER_MODE_NEED_CMDQ,
	SET_LCM_DISP_ON,
};

#define SET_LCM_BLOCKING_NOWAIT 3
#define SET_LCM_NONBLOCKING 2
#define SET_LCM_BLOCKING 1

struct frame_condition_wq {
	wait_queue_head_t wq;
	atomic_t condition;
};

int mtk_drm_ioctl_pq_get_persist_property(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

extern int mtk_disp_hrt_bw_dbg(void);

struct cb_data_store {
	struct cmdq_cb_data data;
	struct list_head link;
};
#ifdef _DRM_P_H_
struct disp_rect {
	u32 x;
	u32 y;
	u32 width;
	u32 height;
};
void disp_dbg_probe(void);
void disp_dbg_init(struct drm_device *drm_dev);
void disp_dbg_deinit(void);
void mtk_wakeup_pf_wq(unsigned int m_id);
void mtk_drm_cwb_backup_copy_size(void);
int mtk_dprec_mmp_dump_ovl_layer(struct mtk_plane_state *plane_state);
int mtk_dprec_mmp_dump_wdma_layer(struct drm_crtc *crtc,
	struct drm_framebuffer *wb_fb);
int mtk_dprec_mmp_dump_cwb_buffer(struct drm_crtc *crtc,
	void *buffer, unsigned int buf_idx);
int disp_met_set(void *data, u64 val);
void mtk_drm_idlemgr_kick_ext(const char *source);
unsigned int mtk_dbg_get_lfr_mode_value(void);
unsigned int mtk_dbg_get_lfr_type_value(void);
unsigned int mtk_dbg_get_lfr_enable_value(void);
unsigned int mtk_dbg_get_lfr_update_value(void);
unsigned int mtk_dbg_get_lfr_vse_dis_value(void);
unsigned int mtk_dbg_get_lfr_skip_num_value(void);
unsigned int mtk_dbg_get_lfr_dbg_value(void);
int mtk_drm_add_cb_data(struct cb_data_store *cb_data, unsigned int crtc_id);
struct cb_data_store *mtk_drm_get_cb_data(unsigned int crtc_id);
void mtk_drm_del_cb_data(struct cmdq_cb_data data, unsigned int crtc_id);
int hrt_lp_switch_get(void);
void mtk_dump_mminfra_ck(void *priv);
void mtk_dprec_snapshot(void);

void mtkfb_set_partial_roi_highlight(int en);
bool mtkfb_is_partial_roi_highlight(void);
void mtkfb_set_force_partial_roi(int en);
bool mtkfb_is_force_partial_roi(void);
int mtkfb_force_partial_y_offset(void);
int mtkfb_force_partial_height(void);

int mtk_ddic_dsi_send_cmd(struct mtk_ddic_dsi_msg *cmd_msg,
			int blocking, bool need_lock);
int mtk_ddic_dsi_read_cmd(struct mtk_ddic_dsi_msg *cmd_msg, bool need_lock);
int set_lcm_wrapper(struct cmdq_pkt *handle, struct mtk_ddic_dsi_msg *cmd_msg, int blocking);
int read_lcm_wrapper(struct cmdq_pkt *handle, struct mtk_ddic_dsi_msg *cmd_msg);
void reset_frame_wq(struct frame_condition_wq *wq);
void wakeup_frame_wq(struct frame_condition_wq *wq);
int wait_frame_condition(enum DISP_FRAME_STATE state, unsigned int timeout);
int mtk_drm_set_frame_skip(bool skip);
#endif
#if IS_ENABLED(CONFIG_DRM_PANEL_MCD_COMMON)
int mtk_drm_wait_one_vblank(void);
void mtk_disp_mipi_ccci_callback(unsigned int en, unsigned int usrdata);
int mtk_drm_uevent_trigger(int state);
#endif

enum mtk_drm_mml_dbg {
	DISP_MML_DBG_LOG = 0x0001,
	DISP_MML_MMCLK_UNLIMIT = 0x0002,
	DISP_MML_IR_CLEAR = 0x0004,
	MMP_ADDON_CONNECT = 0x1000,
	MMP_ADDON_DISCONNECT = 0x2000,
	MMP_MML_SUBMIT = 0x4000,
	MMP_MML_IDLE = 0x8000,
	MMP_MML_REPAINT = 0x10000,
};

#if IS_ENABLED(CONFIG_MTK_DISP_DEBUG)
struct reg_dbg {
	uint32_t addr;
	uint32_t val;
	uint32_t mask;
};

struct wr_online_dbg {
	struct reg_dbg reg[64];
	uint32_t index;
	uint32_t after_commit;
};

extern struct wr_online_dbg g_wr_reg;
#endif

enum GCE_COND_REVERSE_COND {
	R_CMDQ_NOT_EQUAL = CMDQ_EQUAL,
	R_CMDQ_EQUAL = CMDQ_NOT_EQUAL,
	R_CMDQ_LESS = CMDQ_GREATER_THAN_AND_EQUAL,
	R_CMDQ_GREATER = CMDQ_LESS_THAN_AND_EQUAL,
	R_CMDQ_LESS_EQUAL = CMDQ_GREATER_THAN,
	R_CMDQ_GREATER_EQUAL = CMDQ_LESS_THAN,
};

#define GCE_COND_DECLARE \
	u32 _inst_condi_jump, _inst_jump_end; \
	u64 _jump_pa; \
	u64 *_inst; \
	struct cmdq_pkt *_cond_pkt; \
	u16 _gpr, _reg_jump

#define GCE_COND_ASSIGN(pkt, addr, gpr) do { \
	_cond_pkt = pkt; \
	_reg_jump = addr; \
	_gpr = gpr; \
} while (0)

#define GCE_IF(lop, cond, rop) do { \
	_inst_condi_jump = _cond_pkt->cmd_buf_size; \
	cmdq_pkt_assign_command(_cond_pkt, _reg_jump, 0); \
	cmdq_pkt_cond_jump_abs(_cond_pkt, _reg_jump, &lop, &rop, (enum CMDQ_CONDITION_ENUM) cond); \
	_inst_jump_end = _inst_condi_jump; \
} while (0)

#define GCE_ELSE do { \
	_inst_jump_end = _cond_pkt->cmd_buf_size; \
	cmdq_pkt_jump_addr(_cond_pkt, 0); \
	_inst = cmdq_pkt_get_va_by_offset(_cond_pkt, _inst_condi_jump); \
	_jump_pa = cmdq_pkt_get_pa_by_offset(_cond_pkt, _cond_pkt->cmd_buf_size); \
	*_inst = *_inst | CMDQ_REG_SHIFT_ADDR(_jump_pa); \
} while (0)

#define GCE_FI do { \
	_inst = cmdq_pkt_get_va_by_offset(_cond_pkt, _inst_jump_end); \
	_jump_pa = cmdq_pkt_get_pa_by_offset(_cond_pkt, _cond_pkt->cmd_buf_size); \
	*_inst = *_inst & ((u64)0xFFFFFFFF << 32); \
	*_inst = *_inst | CMDQ_REG_SHIFT_ADDR(_jump_pa); \
} while (0)

#define GCE_DO(act, name) cmdq_pkt_##act(_cond_pkt, mtk_crtc->gce_obj.event[name])

#define GCE_SLEEP(us) cmdq_pkt_sleep(_cond_pkt, us, _gpr)
int mtk_disp_ioctl_debug_log_switch(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

#endif
