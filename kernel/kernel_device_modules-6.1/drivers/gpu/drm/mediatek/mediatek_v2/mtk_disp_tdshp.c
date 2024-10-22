// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_disp_tdshp.h"
#include "mtk_disp_pq_helper.h"
#include "mtk_disp_aal.h"

enum TDSHP_IOCTL_CMD {
	SET_TDSHP_REG,
	BYPASS_TDSHP,
};

struct mtk_disp_tdshp_data {
	bool support_shadow;
	bool need_bypass_shadow;
};

struct mtk_disp_tdshp_tile_overhead {
	unsigned int in_width;
	unsigned int overhead;
	unsigned int comp_overhead;
};

struct mtk_disp_tdshp_tile_overhead_v {
	unsigned int overhead_v;
	unsigned int comp_overhead_v;
};

struct mtk_disp_tdshp_primary {
	wait_queue_head_t size_wq;
	bool get_size_available;
	struct DISP_TDSHP_DISPLAY_SIZE tdshp_size;
	struct mutex global_lock;
	spinlock_t clock_lock;
	unsigned int relay_value;
	struct DISP_TDSHP_REG *tdshp_regs;
	int *aal_clarity_support;
};

struct mtk_disp_tdshp {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_tdshp_data *data;
	bool is_right_pipe;
	int path_order;
	struct mtk_ddp_comp *companion;
	struct mtk_disp_tdshp_primary *primary_data;
	struct mtk_disp_tdshp_tile_overhead tile_overhead;
	struct mtk_disp_tdshp_tile_overhead_v tile_overhead_v;
	atomic_t is_clock_on;
	bool set_partial_update;
	unsigned int roi_height;
};

static inline struct mtk_disp_tdshp *comp_to_tdshp(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_tdshp, ddp_comp);
}

static int mtk_disp_tdshp_write_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int lock)
{
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);
	struct mtk_disp_tdshp_primary *primary_data = tdshp_data->primary_data;
	struct DISP_TDSHP_REG *disp_tdshp_regs;
	int ret = 0;

	if (lock)
		mutex_lock(&primary_data->global_lock);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_TDSHP_CFG, 0x2 | primary_data->relay_value, 0x3);

	/* to avoid different show of dual pipe, pipe1 use pipe0's config data */
	disp_tdshp_regs = primary_data->tdshp_regs;
	if (disp_tdshp_regs == NULL) {
		DDPINFO("%s: comp %d not initialized\n", __func__, comp->id);
		ret = -EFAULT;
		goto thshp_write_reg_unlock;
	}

	DDPINFO("tdshp_en: %x, tdshp_limit: %x, tdshp_ylev_256: %x, tdshp_gain_high:%d, tdshp_gain_mid:%d\n",
			disp_tdshp_regs->tdshp_en, disp_tdshp_regs->tdshp_limit,
			disp_tdshp_regs->tdshp_ylev_256, disp_tdshp_regs->tdshp_gain_high,
			disp_tdshp_regs->tdshp_gain_mid);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_00,
		(disp_tdshp_regs->tdshp_softcoring_gain << 0 |
				disp_tdshp_regs->tdshp_gain_high << 8 |
				disp_tdshp_regs->tdshp_gain_mid << 16 |
				disp_tdshp_regs->tdshp_ink_sel << 24 |
				disp_tdshp_regs->tdshp_bypass_high << 29 |
				disp_tdshp_regs->tdshp_bypass_mid << 30 |
				disp_tdshp_regs->tdshp_en << 31), ~0);


	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_01,
		(disp_tdshp_regs->tdshp_limit_ratio << 0 |
				disp_tdshp_regs->tdshp_gain << 4 |
				disp_tdshp_regs->tdshp_coring_zero << 16 |
				disp_tdshp_regs->tdshp_coring_thr << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_02,
		(disp_tdshp_regs->tdshp_coring_value << 8 |
				disp_tdshp_regs->tdshp_bound << 16 |
				disp_tdshp_regs->tdshp_limit << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_03,
		(disp_tdshp_regs->tdshp_sat_proc << 0 |
				disp_tdshp_regs->tdshp_ac_lpf_coe << 8 |
				disp_tdshp_regs->tdshp_clip_thr << 16 |
				disp_tdshp_regs->tdshp_clip_ratio << 24 |
				disp_tdshp_regs->tdshp_clip_en << 31), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_05,
		(disp_tdshp_regs->tdshp_ylev_p048 << 0 |
		disp_tdshp_regs->tdshp_ylev_p032 << 8 |
		disp_tdshp_regs->tdshp_ylev_p016 << 16 |
		disp_tdshp_regs->tdshp_ylev_p000 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_06,
		(disp_tdshp_regs->tdshp_ylev_p112 << 0 |
		disp_tdshp_regs->tdshp_ylev_p096 << 8 |
		disp_tdshp_regs->tdshp_ylev_p080 << 16 |
		disp_tdshp_regs->tdshp_ylev_p064 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_07,
		(disp_tdshp_regs->tdshp_ylev_p176 << 0 |
		disp_tdshp_regs->tdshp_ylev_p160 << 8 |
		disp_tdshp_regs->tdshp_ylev_p144 << 16 |
		disp_tdshp_regs->tdshp_ylev_p128 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_08,
		(disp_tdshp_regs->tdshp_ylev_p240 << 0 |
		disp_tdshp_regs->tdshp_ylev_p224 << 8 |
		disp_tdshp_regs->tdshp_ylev_p208 << 16 |
		disp_tdshp_regs->tdshp_ylev_p192 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_09,
		(disp_tdshp_regs->tdshp_ylev_en << 14 |
		disp_tdshp_regs->tdshp_ylev_alpha << 16 |
		disp_tdshp_regs->tdshp_ylev_256 << 24), ~0);

	// PBC1
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_00,
		(disp_tdshp_regs->pbc1_radius_r << 0 |
		disp_tdshp_regs->pbc1_theta_r << 6 |
		disp_tdshp_regs->pbc1_rslope_1 << 12 |
		disp_tdshp_regs->pbc1_gain << 22 |
		disp_tdshp_regs->pbc1_lpf_en << 30 |
		disp_tdshp_regs->pbc1_en << 31), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_01,
		(disp_tdshp_regs->pbc1_lpf_gain << 0 |
		disp_tdshp_regs->pbc1_tslope << 6 |
		disp_tdshp_regs->pbc1_radius_c << 16 |
		disp_tdshp_regs->pbc1_theta_c << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_02,
		(disp_tdshp_regs->pbc1_edge_slope << 0 |
		disp_tdshp_regs->pbc1_edge_thr << 8 |
		disp_tdshp_regs->pbc1_edge_en << 14 |
		disp_tdshp_regs->pbc1_conf_gain << 16 |
		disp_tdshp_regs->pbc1_rslope << 22), ~0);
	// PBC2
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_03,
		(disp_tdshp_regs->pbc2_radius_r << 0 |
		disp_tdshp_regs->pbc2_theta_r << 6 |
		disp_tdshp_regs->pbc2_rslope_1 << 12 |
		disp_tdshp_regs->pbc2_gain << 22 |
		disp_tdshp_regs->pbc2_lpf_en << 30 |
		disp_tdshp_regs->pbc2_en << 31), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_04,
		(disp_tdshp_regs->pbc2_lpf_gain << 0 |
		disp_tdshp_regs->pbc2_tslope << 6 |
		disp_tdshp_regs->pbc2_radius_c << 16 |
		disp_tdshp_regs->pbc2_theta_c << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_05,
		(disp_tdshp_regs->pbc2_edge_slope << 0 |
		disp_tdshp_regs->pbc2_edge_thr << 8 |
		disp_tdshp_regs->pbc2_edge_en << 14 |
		disp_tdshp_regs->pbc2_conf_gain << 16 |
		disp_tdshp_regs->pbc2_rslope << 22), ~0);
	// PBC3
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_06,
		(disp_tdshp_regs->pbc3_radius_r << 0 |
		disp_tdshp_regs->pbc3_theta_r << 6 |
		disp_tdshp_regs->pbc3_rslope_1 << 12 |
		disp_tdshp_regs->pbc3_gain << 22 |
		disp_tdshp_regs->pbc3_lpf_en << 30 |
		disp_tdshp_regs->pbc3_en << 31), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_07,
		(disp_tdshp_regs->pbc3_lpf_gain << 0 |
		disp_tdshp_regs->pbc3_tslope << 6 |
		disp_tdshp_regs->pbc3_radius_c << 16 |
		disp_tdshp_regs->pbc3_theta_c << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_08,
		(disp_tdshp_regs->pbc3_edge_slope << 0 |
		disp_tdshp_regs->pbc3_edge_thr << 8 |
		disp_tdshp_regs->pbc3_edge_en << 14 |
		disp_tdshp_regs->pbc3_conf_gain << 16 |
		disp_tdshp_regs->pbc3_rslope << 22), ~0);

//#ifdef TDSHP_2_0
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_10,
		(disp_tdshp_regs->tdshp_mid_softlimit_ratio << 0 |
		disp_tdshp_regs->tdshp_mid_coring_zero << 16 |
		disp_tdshp_regs->tdshp_mid_coring_thr << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_11,
		(disp_tdshp_regs->tdshp_mid_softcoring_gain << 0 |
		disp_tdshp_regs->tdshp_mid_coring_value << 8 |
		disp_tdshp_regs->tdshp_mid_bound << 16 |
		disp_tdshp_regs->tdshp_mid_limit << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_12,
		(disp_tdshp_regs->tdshp_high_softlimit_ratio << 0 |
		disp_tdshp_regs->tdshp_high_coring_zero << 16 |
		disp_tdshp_regs->tdshp_high_coring_thr << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_13,
		(disp_tdshp_regs->tdshp_high_softcoring_gain << 0 |
		disp_tdshp_regs->tdshp_high_coring_value << 8 |
		disp_tdshp_regs->tdshp_high_bound << 16 |
		disp_tdshp_regs->tdshp_high_limit << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_EDF_GAIN_00,
		(disp_tdshp_regs->edf_clip_ratio_inc << 0 |
		disp_tdshp_regs->edf_edge_gain << 8 |
		disp_tdshp_regs->edf_detail_gain << 16 |
		disp_tdshp_regs->edf_flat_gain << 24 |
		disp_tdshp_regs->edf_gain_en << 31), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_EDF_GAIN_01,
		(disp_tdshp_regs->edf_edge_th << 0 |
		disp_tdshp_regs->edf_detail_fall_th << 9 |
		disp_tdshp_regs->edf_detail_rise_th << 18 |
		disp_tdshp_regs->edf_flat_th << 25), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_EDF_GAIN_02,
		(disp_tdshp_regs->edf_edge_slope << 0 |
		disp_tdshp_regs->edf_detail_fall_slope << 8 |
		disp_tdshp_regs->edf_detail_rise_slope << 16 |
		disp_tdshp_regs->edf_flat_slope << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_EDF_GAIN_03,
		(disp_tdshp_regs->edf_edge_mono_slope << 0 |
		disp_tdshp_regs->edf_edge_mono_th << 8 |
		disp_tdshp_regs->edf_edge_mag_slope << 16 |
		disp_tdshp_regs->edf_edge_mag_th << 24), 0xFFFFFFFF);

	// DISP TDSHP no DISP_EDF_GAIN_04
	//cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_EDF_GAIN_04,
	//	(disp_tdshp_regs->edf_edge_trend_flat_mag << 8 |
	//	disp_tdshp_regs->edf_edge_trend_slope << 16 |
	//	disp_tdshp_regs->edf_edge_trend_th << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_EDF_GAIN_05,
		(disp_tdshp_regs->edf_bld_wgt_mag << 0 |
		disp_tdshp_regs->edf_bld_wgt_mono << 8), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_C_BOOST_MAIN,
		(disp_tdshp_regs->tdshp_cboost_gain << 0 |
		disp_tdshp_regs->tdshp_cboost_en << 13 |
		disp_tdshp_regs->tdshp_cboost_lmt_l << 16 |
		disp_tdshp_regs->tdshp_cboost_lmt_u << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_C_BOOST_MAIN_2,
		(disp_tdshp_regs->tdshp_cboost_yoffset << 0 |
		disp_tdshp_regs->tdshp_cboost_yoffset_sel << 16 |
		disp_tdshp_regs->tdshp_cboost_yconst << 24), 0xFFFFFFFF);

//#endif // TDSHP_2_0

//#ifdef TDSHP_3_0
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_POST_YLEV_00,
		(disp_tdshp_regs->tdshp_post_ylev_p048 << 0 |
		disp_tdshp_regs->tdshp_post_ylev_p032 << 8 |
		disp_tdshp_regs->tdshp_post_ylev_p016 << 16 |
		disp_tdshp_regs->tdshp_post_ylev_p000 << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_POST_YLEV_01,
		(disp_tdshp_regs->tdshp_post_ylev_p112 << 0 |
		disp_tdshp_regs->tdshp_post_ylev_p096 << 8 |
		disp_tdshp_regs->tdshp_post_ylev_p080 << 16 |
		disp_tdshp_regs->tdshp_post_ylev_p064 << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_POST_YLEV_02,
		(disp_tdshp_regs->tdshp_post_ylev_p176 << 0 |
		disp_tdshp_regs->tdshp_post_ylev_p160 << 8 |
		disp_tdshp_regs->tdshp_post_ylev_p144 << 16 |
		disp_tdshp_regs->tdshp_post_ylev_p128 << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_POST_YLEV_03,
		(disp_tdshp_regs->tdshp_post_ylev_p240 << 0 |
		disp_tdshp_regs->tdshp_post_ylev_p224 << 8 |
		disp_tdshp_regs->tdshp_post_ylev_p208 << 16 |
		disp_tdshp_regs->tdshp_post_ylev_p192 << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_POST_YLEV_04,
		(disp_tdshp_regs->tdshp_post_ylev_en << 14 |
		disp_tdshp_regs->tdshp_post_ylev_alpha << 16 |
		disp_tdshp_regs->tdshp_post_ylev_256 << 24), 0xFFFFFFFF);
//#endif // TDSHP_3_0

thshp_write_reg_unlock:
	if (lock)
		mutex_unlock(&primary_data->global_lock);

	return ret;
}

static int mtk_disp_tdshp_set_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct DISP_TDSHP_REG *user_tdshp_regs)
{
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);
	struct mtk_disp_tdshp_primary *primary_data = tdshp_data->primary_data;
	int ret = 0;
	struct DISP_TDSHP_REG *tdshp_regs, *old_tdshp_regs;

	pr_notice("%s\n", __func__);

	tdshp_regs = kmalloc(sizeof(struct DISP_TDSHP_REG), GFP_KERNEL);
	if (tdshp_regs == NULL) {
		DDPPR_ERR("%s: no memory\n", __func__);
		return -EFAULT;
	}

	if (user_tdshp_regs == NULL) {
		ret = -EFAULT;
		kfree(tdshp_regs);
	} else {
		memcpy(tdshp_regs, user_tdshp_regs,
			sizeof(struct DISP_TDSHP_REG));

		mutex_lock(&primary_data->global_lock);

		old_tdshp_regs = primary_data->tdshp_regs;
		primary_data->tdshp_regs = tdshp_regs;

		pr_notice("%s: Set module(%d) lut\n", __func__, comp->id);
		ret = mtk_disp_tdshp_write_reg(comp, handle, 0);

		mutex_unlock(&primary_data->global_lock);

		if (old_tdshp_regs != NULL)
			kfree(old_tdshp_regs);
	}

	return ret;
}

static int disp_tdshp_wait_size(struct mtk_ddp_comp *comp, unsigned long timeout)
{
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);
	struct mtk_disp_tdshp_primary *primary_data = tdshp_data->primary_data;
	int ret = 0;

	if (primary_data->get_size_available == false) {
		ret = wait_event_interruptible(primary_data->size_wq,
			primary_data->get_size_available == true);

		DDPINFO("size_available = 1, Wake up, ret = %d\n", ret);
	} else {
		DDPINFO("size_available = 0\n");
	}

	return ret;
}

static int mtk_tdshp_cfg_set_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{

	int ret = 0;
	struct DISP_TDSHP_REG *config = data;
	struct mtk_disp_tdshp *tdshp = comp_to_tdshp(comp);

	if (mtk_disp_tdshp_set_reg(comp, handle, config) < 0) {
		DDPPR_ERR("%s: failed\n", __func__);
		return -EFAULT;
	}
	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_ddp_comp *comp_tdshp1 = tdshp->companion;

		if (mtk_disp_tdshp_set_reg(comp_tdshp1, handle, config) < 0) {
			DDPPR_ERR("%s: comp_tdshp1 failed\n", __func__);
			return -EFAULT;
		}
	}
	return ret;
}

int mtk_drm_ioctl_tdshp_set_reg_impl(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;

	return mtk_crtc_user_cmd(&mtk_crtc->base, comp, SET_TDSHP_REG, data);
}

int mtk_drm_ioctl_tdshp_set_reg(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_TDSHP, 0);

	return mtk_drm_ioctl_tdshp_set_reg_impl(comp, data);
}

int mtk_drm_ioctl_tdshp_get_size_impl(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);
	struct mtk_disp_tdshp_primary *primary_data = tdshp_data->primary_data;
	u32 width = 0, height = 0;
	struct DISP_TDSHP_DISPLAY_SIZE *dst =
			(struct DISP_TDSHP_DISPLAY_SIZE *)data;

	pr_notice("%s", __func__);

	mtk_drm_crtc_get_panel_original_size(crtc, &width, &height);
	if (width == 0 || height == 0) {
		DDPFUNC("panel original size error(%dx%d).\n", width, height);
		width = crtc->mode.hdisplay;
		height = crtc->mode.vdisplay;
	}

	primary_data->tdshp_size.lcm_width = width;
	primary_data->tdshp_size.lcm_height = height;

	disp_tdshp_wait_size(comp, 60);

	pr_notice("%s ---", __func__);
	memcpy(dst, &primary_data->tdshp_size, sizeof(primary_data->tdshp_size));

	return 0;
}

int mtk_drm_ioctl_tdshp_get_size(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_TDSHP, 0);

	return mtk_drm_ioctl_tdshp_get_size_impl(comp, data);
}

static void mtk_disp_tdshp_config_overhead(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg)
{
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);

	DDPINFO("line: %d\n", __LINE__);

	if (cfg->tile_overhead.is_support) {
		/*set component overhead*/
		if (!tdshp_data->is_right_pipe) {
			tdshp_data->tile_overhead.comp_overhead = 3;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.left_overhead +=
				tdshp_data->tile_overhead.comp_overhead;
			cfg->tile_overhead.left_in_width +=
					tdshp_data->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			tdshp_data->tile_overhead.in_width =
				cfg->tile_overhead.left_in_width;
			tdshp_data->tile_overhead.overhead =
				cfg->tile_overhead.left_overhead;
		} else {
			tdshp_data->tile_overhead.comp_overhead = 3;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.right_overhead +=
				tdshp_data->tile_overhead.comp_overhead;
			cfg->tile_overhead.right_in_width +=
				tdshp_data->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			tdshp_data->tile_overhead.in_width =
				cfg->tile_overhead.right_in_width;
			tdshp_data->tile_overhead.overhead =
				cfg->tile_overhead.right_overhead;
		}
	}
}

static void mtk_disp_tdshp_config_overhead_v(struct mtk_ddp_comp *comp,
	struct total_tile_overhead_v  *tile_overhead_v)
{
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);

	DDPDBG("line: %d\n", __LINE__);

	/*set component overhead*/
	tdshp_data->tile_overhead_v.comp_overhead_v = 2;
	/*add component overhead on total overhead*/
	tile_overhead_v->overhead_v +=
		tdshp_data->tile_overhead_v.comp_overhead_v;
	/*copy from total overhead info*/
	tdshp_data->tile_overhead_v.overhead_v = tile_overhead_v->overhead_v;
}

static void mtk_disp_tdshp_config(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	unsigned int in_width, out_width;
	unsigned int in_val, out_val;
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);
	struct mtk_disp_tdshp_primary *primary_data = tdshp_data->primary_data;
	unsigned int overhead_v;
	unsigned int comp_overhead_v;

	DDPINFO("line: %d\n", __LINE__);

	if (cfg->source_bpc == 8)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_CTRL, ((0x1 << 2) | 0x1), ~0);
	else if (cfg->source_bpc == 10)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_CTRL, ((0x0 << 2) | 0x1), ~0);
	else
		DDPPR_ERR("%s: Invalid bpc: %u\n", __func__, cfg->bpc);

	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support) {
		in_width = tdshp_data->tile_overhead.in_width;
		out_width = in_width - tdshp_data->tile_overhead.comp_overhead;
	} else {
		if (comp->mtk_crtc->is_dual_pipe)
			in_width = cfg->w / 2;
		else
			in_width = cfg->w;

		out_width = in_width;
	}

	if (!tdshp_data->set_partial_update) {
		in_val = (in_width << 16) | (cfg->h);
		out_val = (out_width << 16) | (cfg->h);
	} else {
		overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
					? 0 : tdshp_data->tile_overhead_v.overhead_v;
		comp_overhead_v = (!overhead_v) ? 0 : tdshp_data->tile_overhead_v.comp_overhead_v;

		in_val = (in_width << 16) | (tdshp_data->roi_height + overhead_v * 2);
		out_val = (out_width << 16) |
				  (tdshp_data->roi_height + (overhead_v - comp_overhead_v) * 2);
	}

	DDPINFO("%s: in: 0x%08x, out: 0x%08x\n", __func__, in_val, out_val);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_TDSHP_INPUT_SIZE, in_val, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_TDSHP_OUTPUT_SIZE, out_val, ~0);
	// DISP_TDSHP_OUTPUT_OFFSET
	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support) {
		if (!tdshp_data->is_right_pipe)
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_TDSHP_OUTPUT_OFFSET, 0x0, ~0);
		else
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_TDSHP_OUTPUT_OFFSET,
				tdshp_data->tile_overhead.comp_overhead << 16 | 0, ~0);
	} else {
		if (!tdshp_data->set_partial_update)
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_TDSHP_OUTPUT_OFFSET, 0x0, ~0);
		else
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_TDSHP_OUTPUT_OFFSET, comp_overhead_v, ~0);
	}

	// DISP_TDSHP_SWITCH
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_TDSHP_CFG, 0x1 << 13, 0x1 << 13);

	// for Display Clarity
	if (primary_data->aal_clarity_support && *primary_data->aal_clarity_support) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_00, 0x1 << 31, 0x1 << 31);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_CFG, 0x1F << 12, 0x1F << 12);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_CFG, 0, 0x1 << 12);
	}

	primary_data->tdshp_size.height = cfg->h;
	primary_data->tdshp_size.width = cfg->w;
	if (primary_data->get_size_available == false) {
		primary_data->get_size_available = true;
		wake_up_interruptible(&primary_data->size_wq);
		pr_notice("size available: (w, h)=(%d, %d)+\n", cfg->w, cfg->h);
	}
}

static void mtk_disp_tdshp_bypass(struct mtk_ddp_comp *comp, int bypass,
	struct cmdq_pkt *handle)
{
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);
	struct mtk_disp_tdshp_primary *primary_data = tdshp_data->primary_data;

	pr_notice("%s, comp_id: %d, bypass: %d\n",
			__func__, comp->id, bypass);

	if (bypass == 1) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_CFG, 0x1, 0x1);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_00, (0x1 << 31), (0x1 << 31));
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_CTRL, 0xfffffffd, ~0);
		primary_data->relay_value = 0x1;
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_CFG, 0x0, 0x1);
		primary_data->relay_value = 0x0;
	}
}

static int mtk_disp_tdshp_user_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	unsigned int cmd, void *data)
{
	struct mtk_disp_tdshp *tdshp = comp_to_tdshp(comp);

	pr_notice("%s, cmd: %d\n", __func__, cmd);
	switch (cmd) {
	case SET_TDSHP_REG:
	{
		struct DISP_TDSHP_REG *config = data;

		if (mtk_disp_tdshp_set_reg(comp, handle, config) < 0) {
			DDPPR_ERR("%s: failed\n", __func__);
			return -EFAULT;
		}
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_ddp_comp *comp_tdshp1 = tdshp->companion;

			if (mtk_disp_tdshp_set_reg(comp_tdshp1, handle, config) < 0) {
				DDPPR_ERR("%s: comp_tdshp1 failed\n", __func__);
				return -EFAULT;
			}
		}

		mtk_crtc_check_trigger(comp->mtk_crtc, true, false);
	}
	break;
	case BYPASS_TDSHP:
	{
		unsigned int *value = data;

		mtk_disp_tdshp_bypass(comp, *value, handle);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_ddp_comp *comp_tdshp1 = tdshp->companion;

			mtk_disp_tdshp_bypass(comp_tdshp1, *value, handle);
		}
	}
	break;
	default:
		DDPPR_ERR("%s: error cmd: %d\n", __func__, cmd);
		return -EINVAL;
	}
	return 0;
}

static void mtk_disp_tdshp_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("line: %d\n", __LINE__);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_TDSHP_CTRL, DISP_TDSHP_EN, 0x1);
	mtk_disp_tdshp_write_reg(comp, handle, 0);
}

static void mtk_disp_tdshp_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("line: %d\n", __LINE__);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_TDSHP_CTRL, 0x0, 0x1);
}

static void mtk_disp_tdshp_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);

	DDPINFO("id(%d)\n", comp->id);
	mtk_ddp_comp_clk_prepare(comp);
	atomic_set(&tdshp_data->is_clock_on, 1);

	if (tdshp_data->data->need_bypass_shadow)
		mtk_ddp_write_mask_cpu(comp, TDSHP_BYPASS_SHADOW,
			DISP_TDSHP_SHADOW_CTRL, TDSHP_BYPASS_SHADOW);
	else
		mtk_ddp_write_mask_cpu(comp, 0,
			DISP_TDSHP_SHADOW_CTRL, TDSHP_BYPASS_SHADOW);
}

static void mtk_disp_tdshp_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);
	struct mtk_disp_tdshp_primary *primary_data = tdshp_data->primary_data;
	unsigned long flags;

	DDPINFO("id(%d)\n", comp->id);
	spin_lock_irqsave(&primary_data->clock_lock, flags);
	DDPINFO("%s @ %d......... spin_trylock_irqsave -- ",
		__func__, __LINE__);
	atomic_set(&tdshp_data->is_clock_on, 0);
	spin_unlock_irqrestore(&primary_data->clock_lock, flags);
	DDPINFO("%s @ %d......... spin_unlock_irqrestore ",
		__func__, __LINE__);
	mtk_ddp_comp_clk_unprepare(comp);
}

static void mtk_tdshp_primary_data_init(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);
	struct mtk_disp_tdshp *companion_data = comp_to_tdshp(tdshp_data->companion);
	struct mtk_disp_tdshp_primary *primary_data = tdshp_data->primary_data;
	struct mtk_ddp_comp *aal_comp;
	struct mtk_disp_aal *aal_data = NULL;

	if (tdshp_data->is_right_pipe) {
		kfree(tdshp_data->primary_data);
		tdshp_data->primary_data = companion_data->primary_data;
		return;
	}
	aal_comp = mtk_ddp_comp_sel_in_cur_crtc_path(comp->mtk_crtc, MTK_DISP_AAL, 0);
	if (aal_comp) {
		aal_data = comp_to_aal(aal_comp);
		primary_data->aal_clarity_support = &aal_data->primary_data->disp_clarity_support;
	}
	init_waitqueue_head(&primary_data->size_wq);
	spin_lock_init(&primary_data->clock_lock);
	mutex_init(&primary_data->global_lock);
}

static int mtk_tdshp_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
							enum mtk_ddp_io_cmd cmd, void *params)
{

	switch (cmd) {
	case PQ_FILL_COMP_PIPE_INFO:
	{
		struct mtk_disp_tdshp *data = comp_to_tdshp(comp);
		bool *is_right_pipe = &data->is_right_pipe;
		int ret, *path_order = &data->path_order;
		struct mtk_ddp_comp **companion = &data->companion;
		struct mtk_disp_tdshp *companion_data;

		if (atomic_read(&comp->mtk_crtc->pq_data->pipe_info_filled) == 1)
			break;
		ret = mtk_pq_helper_fill_comp_pipe_info(comp, path_order, is_right_pipe, companion);
		if (!ret && comp->mtk_crtc->is_dual_pipe && data->companion) {
			companion_data = comp_to_tdshp(data->companion);
			companion_data->path_order = data->path_order;
			companion_data->is_right_pipe = !data->is_right_pipe;
			companion_data->companion = comp;
		}
		mtk_tdshp_primary_data_init(comp);
		if (comp->mtk_crtc->is_dual_pipe && data->companion)
			mtk_tdshp_primary_data_init(data->companion);
	}
		break;
	default:
		break;
	}
	return 0;
}

void mtk_disp_tdshp_first_cfg(struct mtk_ddp_comp *comp,
		struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	pr_notice("%s\n", __func__);
	mtk_disp_tdshp_config(comp, cfg, handle);
}

static int mtk_tdshp_pq_frame_config(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;
	/* will only call left path */
	switch (cmd) {
	/* TYPE1 no user cmd */
	case PQ_TDSHP_SET_REG:
		ret = mtk_tdshp_cfg_set_reg(comp, handle, data, data_size);
		break;
	default:
		break;
	}
	return ret;
}

static int mtk_tdshp_ioctl_transact(struct mtk_ddp_comp *comp,
		unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;

	switch (cmd) {
	case PQ_TDSHP_SET_REG:
		ret = mtk_drm_ioctl_tdshp_set_reg_impl(comp, data);
		break;
	case PQ_TDSHP_GET_SIZE:
		ret = mtk_drm_ioctl_tdshp_get_size_impl(comp, data);
		break;
	default:
		break;
	}
	return ret;
}

static int mtk_tdshp_set_partial_update(struct mtk_ddp_comp *comp,
				struct cmdq_pkt *handle, struct mtk_rect partial_roi, bool enable)
{
	struct mtk_disp_tdshp *tdshp_data = comp_to_tdshp(comp);
	unsigned int full_height = mtk_crtc_get_height_by_comp(__func__,
						&comp->mtk_crtc->base, comp, true);
	unsigned int overhead_v;
	unsigned int comp_overhead_v;

	DDPDBG("%s, %s set partial update, height:%d, enable:%d\n",
			__func__, mtk_dump_comp_str(comp), partial_roi.height, enable);

	tdshp_data->set_partial_update = enable;
	tdshp_data->roi_height = partial_roi.height;
	overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
				? 0 : tdshp_data->tile_overhead_v.overhead_v;
	comp_overhead_v = (!overhead_v) ? 0 : tdshp_data->tile_overhead_v.comp_overhead_v;

	DDPINFO("%s, %s overhead_v:%d, comp_overhead_v:%d\n",
			__func__, mtk_dump_comp_str(comp), overhead_v, comp_overhead_v);

	if (tdshp_data->set_partial_update) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_INPUT_SIZE,
			tdshp_data->roi_height + overhead_v * 2, 0xffff);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_OUTPUT_SIZE,
			tdshp_data->roi_height + (overhead_v - comp_overhead_v) * 2, 0xffff);

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_OUTPUT_OFFSET, comp_overhead_v, 0xff);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_INPUT_SIZE,
			full_height, 0xffff);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_OUTPUT_SIZE, full_height, 0xffff);

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_OUTPUT_OFFSET, 0, 0xff);
	}

	return 0;

}

static const struct mtk_ddp_comp_funcs mtk_disp_tdshp_funcs = {
	.config = mtk_disp_tdshp_config,
	.first_cfg = mtk_disp_tdshp_first_cfg,
	.start = mtk_disp_tdshp_start,
	.stop = mtk_disp_tdshp_stop,
	.bypass = mtk_disp_tdshp_bypass,
	.user_cmd = mtk_disp_tdshp_user_cmd,
	.prepare = mtk_disp_tdshp_prepare,
	.unprepare = mtk_disp_tdshp_unprepare,
	.config_overhead = mtk_disp_tdshp_config_overhead,
	.config_overhead_v = mtk_disp_tdshp_config_overhead_v,
	.io_cmd = mtk_tdshp_io_cmd,
	.pq_frame_config = mtk_tdshp_pq_frame_config,
	.pq_ioctl_transact = mtk_tdshp_ioctl_transact,
	.partial_update = mtk_tdshp_set_partial_update,
};

static int mtk_disp_tdshp_bind(struct device *dev, struct device *master,
			void *data)
{
	struct mtk_disp_tdshp *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	pr_notice("%s+\n", __func__);
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}
	pr_notice("%s-\n", __func__);
	return 0;
}

static void mtk_disp_tdshp_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_tdshp *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	pr_notice("%s+\n", __func__);
	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
	pr_notice("%s-\n", __func__);
}

static const struct component_ops mtk_disp_tdshp_component_ops = {
	.bind = mtk_disp_tdshp_bind,
	.unbind = mtk_disp_tdshp_unbind,
};

void mtk_disp_tdshp_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	mtk_cust_dump_reg(baddr, 0x0, 0x4, 0x8, 0xC);
	mtk_cust_dump_reg(baddr, 0x14, 0x18, 0x1C, 0x20);
	mtk_cust_dump_reg(baddr, 0x24, 0x40, 0x44, 0x48);
	mtk_cust_dump_reg(baddr, 0x4C, 0x50, 0x54, 0x58);
	mtk_cust_dump_reg(baddr, 0x5C, 0x60, 0xE0, 0xE4);
	mtk_cust_dump_reg(baddr, 0xFC, 0x100, 0x104, 0x108);
	mtk_cust_dump_reg(baddr, 0x10C, 0x110, 0x114, 0x118);
	mtk_cust_dump_reg(baddr, 0x11C, 0x120, 0x124, 0x128);
	mtk_cust_dump_reg(baddr, 0x12C, 0x14C, 0x300, 0x304);
	mtk_cust_dump_reg(baddr, 0x308, 0x30C, 0x314, 0x320);
	mtk_cust_dump_reg(baddr, 0x324, 0x328, 0x32C, 0x330);
	mtk_cust_dump_reg(baddr, 0x334, 0x338, 0x33C, 0x340);
	mtk_cust_dump_reg(baddr, 0x344, 0x354, 0x358, 0x360);
	mtk_cust_dump_reg(baddr, 0x368, 0x36C, 0x374, 0x378);
	mtk_cust_dump_reg(baddr, 0x37C, 0x384, 0x388, 0x480);
	mtk_cust_dump_reg(baddr, 0x484, 0x488, 0x48C, 0x490);
	mtk_cust_dump_reg(baddr, 0x67C, -1, -1, -1);
	mtk_cust_dump_reg(baddr, 0x644, 0x648, 0x64C, 0x650);
	mtk_cust_dump_reg(baddr, 0x654, 0x658, 0x65C, 0x660);
	mtk_cust_dump_reg(baddr, 0x664, 0x668, 0x66C, 0x670);
}

void mtk_tdshp_regdump(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_tdshp *tdshp = comp_to_tdshp(comp);
	void __iomem *baddr = comp->regs;
	int k;

	DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp),
			&comp->regs_pa);
	DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(comp));
	for (k = 0; k <= 0x720; k += 16) {
		DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
			readl(baddr + k),
			readl(baddr + k + 0x4),
			readl(baddr + k + 0x8),
			readl(baddr + k + 0xc));
	}
	DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(comp));
	if (comp->mtk_crtc->is_dual_pipe && tdshp->companion) {
		baddr = tdshp->companion->regs;
		DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(tdshp->companion),
				&tdshp->companion->regs_pa);
		DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(tdshp->companion));
		for (k = 0; k <= 0x720; k += 16) {
			DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
				readl(baddr + k),
				readl(baddr + k + 0x4),
				readl(baddr + k + 0x8),
				readl(baddr + k + 0xc));
		}
		DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(tdshp->companion));
	}
}

static int mtk_disp_tdshp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_tdshp *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret = -1;

	pr_notice("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		goto error_dev_init;

	priv->primary_data = kzalloc(sizeof(*priv->primary_data), GFP_KERNEL);
	if (priv->primary_data == NULL) {
		ret = -ENOMEM;
		DDPPR_ERR("Failed to alloc primary_data %d\n", ret);
		goto error_dev_init;
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_TDSHP);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		goto error_primary;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_tdshp_funcs);
	if (ret != 0) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		goto error_primary;
	}

	priv->data = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_tdshp_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}
	pr_notice("%s-\n", __func__);

error_primary:
	if (ret < 0)
		kfree(priv->primary_data);
error_dev_init:
	if (ret < 0)
		devm_kfree(dev, priv);

	return ret;
}

static int mtk_disp_tdshp_remove(struct platform_device *pdev)
{
	struct mtk_disp_tdshp *priv = dev_get_drvdata(&pdev->dev);

	pr_notice("%s+\n", __func__);
	component_del(&pdev->dev, &mtk_disp_tdshp_component_ops);

	mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	pr_notice("%s-\n", __func__);
	return 0;
}

static const struct mtk_disp_tdshp_data mt6983_tdshp_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_tdshp_data mt6895_tdshp_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_tdshp_data mt6879_tdshp_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_tdshp_data mt6855_tdshp_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_tdshp_data mt6985_tdshp_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_tdshp_data mt6897_tdshp_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_tdshp_data mt6989_tdshp_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_tdshp_data mt6878_tdshp_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct of_device_id mtk_disp_tdshp_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6983-disp-tdshp",
	  .data = &mt6983_tdshp_driver_data},
	{ .compatible = "mediatek,mt6895-disp-tdshp",
	  .data = &mt6895_tdshp_driver_data},
	{ .compatible = "mediatek,mt6879-disp-tdshp",
	  .data = &mt6879_tdshp_driver_data},
	{ .compatible = "mediatek,mt6855-disp-tdshp",
	  .data = &mt6855_tdshp_driver_data},
	{ .compatible = "mediatek,mt6985-disp-tdshp",
	  .data = &mt6985_tdshp_driver_data},
	{ .compatible = "mediatek,mt6897-disp-tdshp",
	  .data = &mt6897_tdshp_driver_data},
	{ .compatible = "mediatek,mt6989-disp-tdshp",
	  .data = &mt6989_tdshp_driver_data},
	{ .compatible = "mediatek,mt6878-disp-tdshp",
	  .data = &mt6878_tdshp_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_tdshp_driver_dt_match);

struct platform_driver mtk_disp_tdshp_driver = {
	.probe = mtk_disp_tdshp_probe,
	.remove = mtk_disp_tdshp_remove,
	.driver = {
			.name = "mediatek-disp-tdshp",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_tdshp_driver_dt_match,
		},
};

void disp_tdshp_set_bypass(struct drm_crtc *crtc, int bypass)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			mtk_crtc, MTK_DISP_TDSHP, 0);
	int ret;

	ret = mtk_crtc_user_cmd(crtc, comp, BYPASS_TDSHP, &bypass);

	DDPINFO("%s : ret = %d", __func__, ret);
}

unsigned int disp_tdshp_bypass_info(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_ddp_comp *comp;
	struct mtk_disp_tdshp *tdshp_data;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_TDSHP, 0);
	tdshp_data = comp_to_tdshp(comp);

	return tdshp_data->primary_data->relay_value;
}
