/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_GAMMA_H__
#define __MTK_DISP_GAMMA_H__

#include <linux/uaccess.h>
#include <uapi/drm/mediatek_drm.h>
struct gamma_color_protect {
	unsigned int gamma_color_protect_support;
	unsigned int gamma_color_protect_lsb;
};

struct gamma_color_protect_mode {
	unsigned int red_support;
	unsigned int green_support;
	unsigned int blue_support;
	unsigned int black_support;
	unsigned int white_support;
};

struct mtk_disp_gamma_data {
	bool support_gamma_gain;
	unsigned int gamma_gain_range;
};

struct mtk_disp_gamma_tile_overhead {
	unsigned int width;
	unsigned int comp_overhead;
};

struct mtk_disp_gamma_tile_overhead_v {
	unsigned int overhead_v;
	unsigned int comp_overhead_v;
};

struct mtk_disp_gamma_sb_param {
	unsigned int gain[3];
	unsigned int bl;
	unsigned int gain_range;
};

struct mtk_disp_gamma_primary {
	struct mtk_disp_gamma_sb_param sb_param;
	struct gamma_color_protect color_protect;
	struct DISP_GAMMA_LUT_T gamma_lut_cur;
	struct DISP_GAMMA_12BIT_LUT_T gamma_12b_lut;

	atomic_t irq_event;

	struct mutex power_lock;
	/* lock for hw reg & related data, inner lock of sram_lock/crtc_lock */
	struct mutex global_lock;
	/* lock for sram ops and data, outer lock of global_lock/crtc_lock */
	struct mutex sram_lock;
	struct cmdq_pkt *sram_pkt;
	struct wakeup_source *gamma_wake_lock;

	atomic_t clock_on;
	atomic_t sof_filp;
	atomic_t force_delay_check_trig;

	atomic_t force_relay;
	unsigned int back_up_cfg;
	unsigned int data_mode;
	unsigned int table_config_sel;
	unsigned int table_out_sel;
	bool hwc_ctl_silky_brightness_support;
	bool gamma_wake_locked;
	bool need_refinalize;
	atomic_t gamma_sram_hw_init;
};

struct mtk_disp_gamma {
	struct mtk_ddp_comp ddp_comp;
	const struct mtk_disp_gamma_data *data;
	bool is_right_pipe;
	int path_order;
	struct mtk_disp_gamma_tile_overhead tile_overhead;
	struct mtk_disp_gamma_tile_overhead_v tile_overhead_v;
	struct mtk_ddp_comp *companion;
	struct mtk_disp_gamma_primary *primary_data;
	atomic_t gamma_is_clock_on;
	bool pkt_reused;
	struct cmdq_reuse reuse_gamma_lut[DISP_GAMMA_12BIT_LUT_SIZE * 2 + 6];
	bool set_partial_update;
	unsigned int roi_height;
};

static inline struct mtk_disp_gamma *comp_to_gamma(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_gamma, ddp_comp);
}


#define GAMMA_ENTRY(r10, g10, b10) (((r10) << 20) | ((g10) << 10) | (b10))

int mtk_drm_ioctl_set_gammalut(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_set_12bit_gammalut(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_bypass_disp_gamma(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_gamma_mul_disable(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
void mtk_trans_gain_to_gamma(struct mtk_ddp_comp *comp,
	unsigned int gain[3], unsigned int bl, void *param);
int mtk_cfg_trans_gain_to_gamma(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle, struct DISP_AAL_PARAM *aal_param, unsigned int bl, void *param);
void disp_gamma_set_bypass(struct drm_crtc *crtc, int bypass);
void mtk_gamma_regdump(struct mtk_ddp_comp *comp);
void mtk_disp_gamma_debug(struct drm_crtc *crtc, const char *opt);

// for HWC LayerBrightness, backlight & gamma gain update by atomic
int mtk_gamma_set_silky_brightness_gain(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	unsigned int gain[3], unsigned int gain_range);
// for displayPQ update to swpm tppa
unsigned int disp_gamma_bypass_info(struct mtk_drm_crtc *mtk_crtc);
#endif
