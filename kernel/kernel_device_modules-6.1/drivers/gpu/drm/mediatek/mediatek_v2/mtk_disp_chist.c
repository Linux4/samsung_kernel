// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_log.h"
#include "mtk_disp_chist.h"
#include "mtk_dump.h"
#include "mtk_disp_pq_helper.h"

#define DISP_CHIST_COLOR_FORMAT 0x3ff
/* channel 0~3 has 256 bins, 4~6 has 128 bins */
#define DISP_CHIST_MAX_BIN 256
#define DISP_CHIST_MAX_BIN_LOW 128

/* chist custom info */
#define DISP_CHIST_HWC_CHANNEL_INDEX 4
/* chist custom info end*/

#define DISP_CHIST_YUV_PARAM_COUNT  12
#define DISP_CHIST_POST_PARAM_INDEX 9

#define DISP_CHIST_MAX_RGB 0x0321

#define DISP_CHIST_DUAL_PIPE_OVERLAP 0

#define DISP_CHIST_EN                0x0
#define DISP_CHIST_INTEN             0x08
#define DISP_CHIST_INSTA             0x0C
#define DISP_CHIST_CFG               0x20
#define DISP_CHIST_SIZE              0x30
#define DISP_CHIST_CONFIG            0x40
#define DISP_CHIST_Y2R_PAPA_R0       0x50
#define DISP_CHIST_Y2R_PAPA_POST_A0  0x80
#define DISP_CHIST_SHADOW_CTRL       0xF0
#define FLD_APB_MCYC_RD REG_FLD_MSB_LSB(3, 3)
#define FLD_READ_WRK_REG REG_FLD_MSB_LSB(2, 2)
#define FLD_FORCE_COMMIT REG_FLD_MSB_LSB(1, 1)
#define FLD_BYPASS_SHADOW REG_FLD_MSB_LSB(0, 0)
// channel_n_win_x_main = DISP_CHIST_CH0_WIN_X_MAIN + n * 0x10
#define DISP_CHIST_CH0_WIN_X_MAIN    0x0460
#define DISP_CHIST_CH0_WIN_Y_MAIN    0x0464
#define DISP_CHIST_CH0_BLOCK_INFO    0x0468
#define DISP_CHIST_CH0_BLOCK_CROP    0x046C
#define DISP_CHIST_WEIGHT            0x0500
#define DISP_CHIST_BLD_CONFIG        0x0504
#define DISP_CHIST_HIST_CH_CFG1      0x0510
#define DISP_CHIST_HIST_CH_CFG2      0x0514
#define DISP_CHIST_HIST_CH_CFG3      0x0518
#define DISP_CHIST_HIST_CH_CFG4      0x051C
#define DISP_CHIST_HIST_CH_CFG5      0x0520
#define DISP_CHIST_HIST_CH_CFG6      0x0524
#define DISP_CHIST_HIST_CH_CNF0      0x0528
#define DISP_CHIST_HIST_CH_CNF1      0x052C
#define DISP_CHIST_HIST_CH_CH0_CNF0  0x0530
#define DISP_CHIST_HIST_CH_CH0_CNF1  0x0534
#define DISP_CHIST_HIST_CH_MON       0x0568

#define DISP_CHIST_APB_READ          0x0600
#define DISP_CHIST_SRAM_R_IF         0x0680

static bool debug_dump_hist;

static unsigned int sel_index;
static void mtk_chist_primary_data_init(struct mtk_ddp_comp *comp);

static int g_rgb_2_yuv[4][DISP_CHIST_YUV_PARAM_COUNT] = {
	// BT601 full
	{0x1322D,  0x25916,  0x74BC,   // RMR,RMG,RMB
	 0x7F5337, 0x7EACCA, 0x20000,  // GMR,GMG,GMB
	 0x20000,  0x7E5344, 0x7FACBD, // BMR,BMG,BMB
	 0X0,      0X7FF,    0X7FF},   // POST_RA,POST_GA,POST_BA}
	 // BT709 full
	{0xD9B3,   0x2D999,  0x49EE,   // RMR,RMG,RMB
	 0x7F8AAE, 0x7E76D0, 0x20000,  // GMR,GMG,GMB
	 0x20000,  0x7E30B4, 0x7FD10E, // BMR,BMG,BMB
	 0X0,      0X7FF,    0X7FF},   // POST_RA,POST_GA,POST_BA}
	 // BT601 limit
	{0x106F3,  0x2043A,  0x6441,   // RMR,RMG,RMB
	 0x7F6839, 0x7ED606, 0x1C1C1,  // GMR,GMG,GMB
	 0x1C1C1,  0x7E8763, 0x7FB6DC, // BMR,BMG,BMB
	 0X100,      0X7FF,    0X7FF},    // POST_RA,POST_GA,POST_BA}
	 // BT709 limit
	{0xBAF7,   0x27298,  0x3F7E,   // RMR,RMG,RMB
	 0x7F98F1, 0x7EA69D, 0x1C1C1,  // GMR,GMG,GMB
	 0x1C1C1,  0x7E6907, 0x7FD6C3, // BMR,BMG,BMB
	 0X100,      0X7FF,    0X7FF}  // POST_RA,POST_GA,POST_BA
};

enum CHIST_IOCTL_CMD {
	CHIST_CONFIG = 0,
	CHIST_UNKNOWN,
};

int mtk_drm_ioctl_get_chist_caps(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_mtk_chist_caps *caps_info = data;
	struct mtk_ddp_comp *comp;
	struct drm_crtc *crtc;
	struct mtk_disp_chist *chist_data;
	unsigned int crtc_id = caps_info->device_id >> 16 & 0xffff;
	unsigned int index = caps_info->device_id & 0xffff;
	int i = 0;

	if (crtc_id > 0)
		crtc = drm_crtc_find(dev, file_priv, crtc_id);
	else
		crtc = private->crtc[0];
	if (!crtc) {
		DDPPR_ERR("%s, crtc is null id:%d!\n", __func__, crtc_id);
		return -1;
	}

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(to_mtk_crtc(crtc), MTK_DISP_CHIST, index);
	if (comp == NULL) {
		DDPPR_ERR("%s, null pointer! index:%d\n", __func__, index);
		return -1;
	}
	chist_data = comp_to_chist(comp);

	caps_info->lcm_width = chist_data->primary_data->frame_width ?
				chist_data->primary_data->frame_width : crtc->mode.hdisplay;
	caps_info->lcm_height = chist_data->primary_data->frame_height ?
				chist_data->primary_data->frame_height : crtc->mode.vdisplay;
	caps_info->support_color = DISP_CHIST_COLOR_FORMAT;
	for (; i < DISP_CHIST_CHANNEL_COUNT; i++) {
		memcpy(&(caps_info->chist_config[i]),
			&chist_data->primary_data->chist_config[i],
			sizeof(chist_data->primary_data->chist_config[i]));

		// pqservice use channel 0, 1, 2, 3, if has one chist
		if (index == 0 && i >= DISP_CHIST_HWC_CHANNEL_INDEX)
			caps_info->chist_config[i].channel_id = DISP_CHIST_CHANNEL_COUNT;
		else
			caps_info->chist_config[i].channel_id = i;
	}
	DDPINFO("%s chist id:%d, w:%d,h:%d\n", __func__, caps_info->device_id,
		caps_info->lcm_width, caps_info->lcm_height);
	DDPINFO("%s --\n", __func__);
	return 0;
}

int mtk_drm_ioctl_set_chist_config(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_mtk_chist_config *config = data;
	struct mtk_ddp_comp *comp;
	struct drm_crtc *crtc;
	struct mtk_disp_chist *chist_data;
	unsigned int crtc_id = config->device_id >> 16 & 0xffff;
	unsigned int index = config->device_id & 0xffff;
	int i = 0;
	int ret = 0;

	if (crtc_id > 0)
		crtc = drm_crtc_find(dev, file_priv, crtc_id);
	else
		crtc = private->crtc[0];
	if (!crtc) {
		DDPPR_ERR("%s, crtc is null id:%d!\n", __func__, crtc_id);
		return -1;
	}

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(to_mtk_crtc(crtc), MTK_DISP_CHIST, index);
	if (comp == NULL) {
		DDPPR_ERR("%s, null pointer! index:%d\n", __func__, index);
		return -1;
	}

	chist_data = comp_to_chist(comp);

	if (config->config_channel_count == 0 ||
			config->config_channel_count > DISP_CHIST_CHANNEL_COUNT) {
		DDPPR_ERR("%s, invalid config channel count:%u\n",
				__func__, config->config_channel_count);
		return -EINVAL;
	}

	DDPINFO("%s  chist id:%d, caller:%d, config count:%d\n", __func__,
		config->device_id, config->caller, config->config_channel_count);

	if (config->caller == MTK_DRM_CHIST_CALLER_HWC) {
		unsigned int channel_id = DISP_CHIST_HWC_CHANNEL_INDEX;

		for (; i < config->config_channel_count &&
			channel_id < DISP_CHIST_CHANNEL_COUNT; i++) {
			config->chist_config[i].channel_id = channel_id;
			channel_id++;
		}
	} else {
		if (chist_data->path_order == 0
			&& config->config_channel_count > DISP_CHIST_HWC_CHANNEL_INDEX)
			config->config_channel_count = DISP_CHIST_HWC_CHANNEL_INDEX;
	}
	chist_data->primary_data->present_fence = 0;
	chist_data->primary_data->need_restore = 1;
	chist_data->primary_data->pre_frame_width = 0;
	mtk_drm_idlemgr_kick(__func__, crtc, 1);
	ret = mtk_crtc_user_cmd(crtc, comp, CHIST_CONFIG, data);
	mtk_crtc_check_trigger(comp->mtk_crtc, false, true);
	DDPINFO("%s --\n", __func__);
	return ret;

}

// need dither to call this api
void mtk_chist_set_tile_overhead(struct mtk_drm_crtc *mtk_crtc, int overhead, bool is_right)
{
	struct mtk_ddp_comp *comp;
	struct mtk_disp_chist *chist_data;

	if (overhead > 0) {
		// set the tile over head of chist which is after pq
		if (is_right)
			comp = mtk_ddp_comp_sel_in_dual_pipe(mtk_crtc, MTK_DISP_CHIST, 0);
		else
			comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_CHIST, 0);

		if (comp) {
			chist_data = comp_to_chist(comp);
			chist_data->tile_overhead = overhead;
		}
	}
}

static void disp_chist_set_interrupt(struct mtk_ddp_comp *comp,
					bool enabled, struct cmdq_pkt *handle)
{
	if (enabled && (readl(comp->regs + DISP_CHIST_INTEN) & 0x2))
		return;
	else if (!enabled && !(readl(comp->regs + DISP_CHIST_INTEN) & 0x2))
		return;

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_INTEN, enabled ? 0x2 : 0, ~0);
}

static int disp_chist_copy_hist_to_user(struct drm_device *dev, struct drm_file *file_priv,
	struct drm_mtk_chist_info *hist)
{
	unsigned long flags;
	int ret = 0;
	unsigned int i = 0;
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp;
	struct drm_crtc *crtc;
	struct mtk_disp_chist *chist_data;
	unsigned int crtc_id = hist->device_id >> 16 & 0xffff;
	unsigned int index = hist->device_id & 0xffff;

	if (crtc_id > 0)
		crtc = drm_crtc_find(dev, file_priv, crtc_id);
	else
		crtc = private->crtc[0];
	if (!crtc) {
		DDPPR_ERR("%s, crtc is null id:%d!\n", __func__, crtc_id);
		return -1;
	}

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(to_mtk_crtc(crtc), MTK_DISP_CHIST, index);
	if (comp == NULL) {
		DDPPR_ERR("%s, null pointer! index:%d\n", __func__, index);
		return -1;
	}

	chist_data = comp_to_chist(comp);
	if (chist_data->primary_data->present_fence == 0) {
		hist->present_fence = 0;
		return ret;
	}

	if (chist_data->primary_data->pre_frame_width > 0
		&& (chist_data->primary_data->pre_frame_width
		!= chist_data->primary_data->frame_width)) {
		DDPPR_ERR("%s, need reconfig pre width:%d, current width:%d!\n", __func__,
				chist_data->primary_data->pre_frame_width,
				chist_data->primary_data->frame_width);
		return -EAGAIN;
	}

	spin_lock_irqsave(&chist_data->primary_data->data_lock, flags);

	for (; i < hist->get_channel_count; i++) {
		unsigned int channel_id = hist->channel_hist[i].channel_id;

		if (channel_id < DISP_CHIST_CHANNEL_COUNT &&
			chist_data->primary_data->chist_config[channel_id].enabled) {
			memcpy(&(hist->channel_hist[i]),
				&chist_data->primary_data->disp_hist[channel_id],
				sizeof(chist_data->primary_data->disp_hist[channel_id]));
		}
	}
	hist->present_fence = chist_data->primary_data->present_fence;
	spin_unlock_irqrestore(&chist_data->primary_data->data_lock, flags);

	//dump all regs
	if (debug_dump_hist)
		mtk_chist_dump(comp);

	return ret;
}

static inline int chist_shift_num(struct mtk_ddp_comp *comp)
{
	return comp_to_chist(comp)->data->chist_shift_num;
}

void mtk_chist_dump_impl(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	int i = 0;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp), comp->regs_pa);

	mtk_cust_dump_reg(baddr, DISP_CHIST_EN, DISP_CHIST_CFG,
		DISP_CHIST_SIZE, DISP_CHIST_INTEN);
	mtk_cust_dump_reg(baddr, DISP_CHIST_HIST_CH_CFG1, DISP_CHIST_HIST_CH_CFG2,
		DISP_CHIST_HIST_CH_CFG3, DISP_CHIST_HIST_CH_CFG4);
	mtk_cust_dump_reg(baddr, DISP_CHIST_WEIGHT, DISP_CHIST_BLD_CONFIG,
		DISP_CHIST_HIST_CH_CFG5, DISP_CHIST_HIST_CH_CFG6);
	for (; i < 7; i++) {
		mtk_cust_dump_reg(baddr, DISP_CHIST_CH0_WIN_X_MAIN + i * 0x10,
			DISP_CHIST_CH0_WIN_Y_MAIN + i * 0x10,
			DISP_CHIST_CH0_BLOCK_INFO + i * 0x10,
			DISP_CHIST_CH0_BLOCK_CROP + i * 0x10);
		mtk_cust_dump_reg(baddr, DISP_CHIST_HIST_CH_CNF0, DISP_CHIST_HIST_CH_CNF1,
			DISP_CHIST_HIST_CH_CH0_CNF0 + i * 8,
			DISP_CHIST_HIST_CH_CH0_CNF1 + i * 8);
	}
	for (i = 0; i < 3; i++)
		mtk_cust_dump_reg(baddr, DISP_CHIST_Y2R_PAPA_R0 + i * 4, 0x05c + i * 4,
			0x068 + i * 4, DISP_CHIST_Y2R_PAPA_POST_A0 + i * 4);
}

void mtk_chist_dump(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_chist *chist_data = comp_to_chist(comp);

	mtk_chist_dump_impl(comp);
	if (chist_data->companion)
		mtk_chist_dump_impl(chist_data->companion);
}

int mtk_chist_analysis(struct mtk_ddp_comp *comp)
{
	DDPDUMP("== %s ANALYSIS ==\n", mtk_dump_comp_str(comp));

	return 0;
}

int mtk_drm_ioctl_get_chist(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct drm_mtk_chist_info *hist = data;
	int i = 0;
	int ret = 0;

	if (hist == NULL) {
		DDPPR_ERR("%s drm_mtk_hist_info is NULL\n", __func__);
		return -EFAULT;
	}

	DDPINFO("%s chist id:%d, get count:%d\n", __func__,
		hist->device_id, hist->get_channel_count);

	if (hist->get_channel_count == 0 ||
			hist->get_channel_count > DISP_CHIST_CHANNEL_COUNT) {
		DDPPR_ERR("%s invalid get channel count is %u\n",
				__func__, hist->get_channel_count);
		return -EFAULT;
	}

	if (hist->caller == MTK_DRM_CHIST_CALLER_HWC) {
		unsigned int channel_id = DISP_CHIST_HWC_CHANNEL_INDEX;

		for (; i < hist->get_channel_count &&
			channel_id < DISP_CHIST_CHANNEL_COUNT; i++) {
			hist->channel_hist[i].channel_id = channel_id;
			channel_id++;
		}
	}

	ret = disp_chist_copy_hist_to_user(dev, file_priv, hist);
	DDPINFO("%s --\n", __func__);
	return ret;
}

static void mtk_chist_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s, comp->id:%s\n", __func__, mtk_dump_comp_str(comp));

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_CFG, 0x106, ~0);
}

static void mtk_chist_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s, comp->id:%s\n", __func__, mtk_dump_comp_str(comp));

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_CFG, 0x1, ~0);
}

static void mtk_chist_bypass(struct mtk_ddp_comp *comp, int bypass,
	struct cmdq_pkt *handle)
{
	DDPINFO("%s, comp->id:%s\n", __func__, mtk_dump_comp_str(comp));

	if (bypass == 1)
		mtk_chist_stop(comp, handle);
	else
		mtk_chist_start(comp, handle);
}

static void mtk_chist_channel_enabled(unsigned int channel,
	bool enabled, struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	if (channel > DISP_CHIST_CHANNEL_COUNT)
		return;

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_HIST_CH_CFG1,
		(enabled  ? 1 : 0) << channel, 1 << channel);
}

static unsigned int mtk_chist_bin_count_regs(unsigned int bin_count)
{
	switch (bin_count) {
	case 256:
		return 0;
	case 128:
		return 1;
	case 64:
		return 2;
	case 32:
		return 3;
	case 16:
		return 4;
	case 8:
		return 5;
	default:
		return 5;
	}
}

static void mtk_chist_channel_config(unsigned int channel,
	struct drm_mtk_channel_config *config,
	struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_chist *chist_data = comp_to_chist(comp);

	DDPINFO("%s channel:%d, config->blk_height:%d, config->blk_width:%d\n", __func__,
			channel, config->blk_height, config->blk_width);

	// roi
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_CH0_WIN_X_MAIN + channel * 0x10,
		(config->roi_end_x << 16) | config->roi_start_x, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_CH0_WIN_Y_MAIN + channel * 0x10,
		(config->roi_end_y << 16) | config->roi_start_y, ~0);

	// block
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_CH0_BLOCK_INFO + channel * 0x10,
		(config->blk_height << 16) | config->blk_width, ~0);

	if (channel >= DISP_CHIST_CHANNEL_COUNT)
		return;

	if (chist_data->is_right_pipe)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_CHIST_CH0_BLOCK_CROP + channel * 0x10,
			chist_data->primary_data->block_config[channel].blk_xofs, ~0);
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_CHIST_CH0_BLOCK_CROP + channel * 0x10, 0x0, ~0);

	// bin count, 0:256,1:128,2:64,3:32,4:16,5:8
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_HIST_CH_CFG3,
		mtk_chist_bin_count_regs(config->bin_count) << channel * 4,
		0x07 << channel * 4);
	// channel sel color format, channel bin count
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_HIST_CH_CFG2,
		config->color_format << channel * 4, 0x0F << channel * 4);

	// (HS)V = max(R G B)
	if (config->color_format == MTK_DRM_COLOR_FORMAT_M)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_CHIST_HIST_CH_CFG4, DISP_CHIST_MAX_RGB, ~0);

	mtk_chist_channel_enabled(channel, 1, comp, handle);
}

static void ceil(int num, int divisor, int *result)
{
	if (divisor > 0) {
		if (num % divisor == 0)
			*result = num / divisor;
		else
			*result = num / divisor + 1;
	} else
		*result = 0;
}

static void mtk_chist_block_config(struct drm_mtk_channel_config *channel_config,
	struct drm_mtk_channel_config *channel_config1,
	unsigned int channel_id, struct mtk_disp_chist *chist_data)
{
	int roi_left_width = chist_data->primary_data->pipe_width - channel_config->roi_start_x;
	unsigned long flags;
	struct mtk_disp_chist *chist_right = comp_to_chist(chist_data->companion);

	if (channel_config->roi_end_x > chist_data->primary_data->pipe_width
		&& channel_config->roi_start_x >= chist_data->primary_data->pipe_width) {
		// roi is in right pipe only
		channel_config1->roi_start_x = channel_config->roi_start_x
					- chist_data->primary_data->pipe_width;
		channel_config1->roi_end_x = channel_config->roi_end_x
					- chist_data->primary_data->pipe_width;
		channel_config->roi_start_x = 0;
		channel_config->roi_end_x = 0;
	} else if (channel_config->roi_end_x < chist_data->primary_data->pipe_width) {
		// roi is in left pipe only
		channel_config1->roi_start_x = 0;
		channel_config1->roi_end_x = 0;
	} else {
		channel_config1->roi_start_x = chist_right->tile_overhead;
		channel_config1->roi_end_x = channel_config->roi_end_x -
			chist_data->primary_data->pipe_width + chist_right->tile_overhead;
		channel_config->roi_end_x = chist_data->primary_data->pipe_width - 1;
	}

	if (channel_config->blk_width < chist_data->primary_data->pipe_width) {
		int right_blk_xfos = roi_left_width % channel_config->blk_width;
		int left_blk_column = roi_left_width / channel_config->blk_width;

		if (channel_id >= DISP_CHIST_CHANNEL_COUNT)
			return;

		spin_lock_irqsave(&chist_data->primary_data->data_lock, flags);
		if (right_blk_xfos) {
			chist_data->primary_data->block_config[channel_id].merge_column
							= left_blk_column;
			left_blk_column++;
			chist_data->primary_data->block_config[channel_id].blk_xofs
							= right_blk_xfos;
		} else
			chist_data->primary_data->block_config[channel_id].merge_column = -1;

		chist_data->primary_data->block_config[channel_id].left_column = left_blk_column;
		spin_unlock_irqrestore(&chist_data->primary_data->data_lock, flags);
	}
}

static int mtk_chist_user_cmd(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data)
{
	struct drm_mtk_chist_config *config = data;
	struct mtk_disp_chist *chist_data = comp_to_chist(comp);
	unsigned long flags;
	int i = 0;
	int bypass = 1;

	if (config->config_channel_count == 0)
		return -EINVAL;

	for (; i < config->config_channel_count; i++) {
		struct drm_mtk_channel_config channel_config;
		unsigned int channel_id = 0;

		memcpy(&channel_config, &config->chist_config[i],
				sizeof(config->chist_config[i]));
		channel_id = channel_config.channel_id;

		if (channel_id >= DISP_CHIST_CHANNEL_COUNT)
			continue;

		spin_lock_irqsave(&chist_data->primary_data->data_lock, flags);
		memset(&(chist_data->primary_data->chist_config[channel_id]), 0,
			sizeof(struct drm_mtk_channel_config));
		chist_data->primary_data->chist_config[channel_id].channel_id = channel_id;
		memset(&(chist_data->primary_data->block_config[channel_id]), 0,
			sizeof(struct mtk_disp_block_config));
		spin_unlock_irqrestore(&chist_data->primary_data->data_lock, flags);

		if (channel_config.enabled) {
			int blk_column = 0;
			// end of roi, width & height of block can't be 0
			channel_config.roi_end_x = channel_config.roi_end_x
			? channel_config.roi_end_x : chist_data->primary_data->frame_width - 1;
			channel_config.roi_end_y = channel_config.roi_end_y
				? channel_config.roi_end_y
				: chist_data->primary_data->frame_height - 1;
			channel_config.blk_width = channel_config.blk_width
				? channel_config.blk_width
				: channel_config.roi_end_x - channel_config.roi_start_x + 1;
			channel_config.blk_height = channel_config.blk_height
				? channel_config.blk_height
				: channel_config.roi_end_y - channel_config.roi_start_y + 1;

			ceil((channel_config.roi_end_x - channel_config.roi_start_x + 1),
				channel_config.blk_width, &blk_column);

			spin_lock_irqsave(&chist_data->primary_data->data_lock, flags);
			chist_data->primary_data->block_config[channel_id].sum_column = blk_column;

			memcpy(&(chist_data->primary_data->chist_config[channel_id]),
				&channel_config, sizeof(channel_config));
			spin_unlock_irqrestore(&chist_data->primary_data->data_lock, flags);

			if (comp->mtk_crtc->is_dual_pipe) {
				struct mtk_ddp_comp *dual_comp = chist_data->companion;
				struct drm_mtk_channel_config channel_config1;

				memcpy(&channel_config1, &channel_config, sizeof(channel_config));

				if (!dual_comp)
					return 1;

				if (channel_config.roi_start_x >=
				chist_data->primary_data->pipe_width) {
					// roi is in the right half, just config right
					channel_config1.roi_start_x = channel_config1.roi_start_x
						- chist_data->primary_data->pipe_width;
					channel_config1.roi_end_x = channel_config1.roi_end_x
						- chist_data->primary_data->pipe_width;
					mtk_chist_channel_config(channel_id,
						&channel_config1, dual_comp, handle);
				} else if (channel_config.roi_end_x > 0 &&
					channel_config.roi_end_x <
					chist_data->primary_data->pipe_width) {
					// roi is in the left half, just config left module
					mtk_chist_channel_config(channel_id,
							&channel_config, comp, handle);
				} else {
					mtk_chist_block_config(&channel_config,
						&channel_config1, channel_id, chist_data);

					mtk_chist_channel_config(channel_id, &channel_config,
						comp, handle);
					mtk_chist_channel_config(channel_id, &channel_config1,
						dual_comp, handle);
				}
			} else {
				mtk_chist_channel_config(channel_id, &channel_config,
					comp, handle);
			}
		} else {
			mtk_chist_channel_enabled(channel_id, 0, comp, handle);
			if (comp->mtk_crtc->is_dual_pipe) {
				if (chist_data->companion)
					mtk_chist_channel_enabled(channel_id, 0,
						chist_data->companion, handle);
			}
		}
	}
	spin_lock_irqsave(&chist_data->primary_data->data_lock, flags);
	for (i = 0; i < DISP_CHIST_CHANNEL_COUNT; i++) {
		if (chist_data->primary_data->chist_config[i].enabled) {
			bypass = 0;
			break;
		}
	}
	spin_unlock_irqrestore(&chist_data->primary_data->data_lock, flags);

	mtk_chist_bypass(comp, bypass, handle);

	if (comp->mtk_crtc->is_dual_pipe) {
		if (chist_data->companion)
			mtk_chist_bypass(chist_data->companion, bypass, handle);
	}
	return 0;
}

static void mtk_chist_prepare(struct mtk_ddp_comp *comp)
{
	unsigned long flags;
	struct mtk_disp_chist *chist_data = comp_to_chist(comp);
	DDPINFO("%s, comp->id:%d\n", __func__, comp->id);

	mtk_ddp_comp_clk_prepare(comp);
	spin_lock_irqsave(&chist_data->primary_data->power_lock, flags);
	atomic_set(&chist_data->primary_data->clock_on, 1);
	spin_unlock_irqrestore(&chist_data->primary_data->power_lock, flags);
}

static void mtk_chist_unprepare(struct mtk_ddp_comp *comp)
{
	unsigned long flags;
	struct mtk_disp_chist *chist_data = comp_to_chist(comp);
	DDPINFO("%s, comp->id:%d\n", __func__, comp->id);

	spin_lock_irqsave(&chist_data->primary_data->power_lock, flags);
	atomic_set(&chist_data->primary_data->clock_on, 0);
	spin_unlock_irqrestore(&chist_data->primary_data->power_lock, flags);
	mtk_ddp_comp_clk_unprepare(comp);
}

static void disp_chist_restore_setting(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct drm_mtk_chist_config config;
	unsigned long flags;
	int i = 0;
	struct mtk_disp_chist *chist_data = comp_to_chist(comp);

	config.config_channel_count = DISP_CHIST_CHANNEL_COUNT;
	for (; i < DISP_CHIST_CHANNEL_COUNT; i++) {
		spin_lock_irqsave(&chist_data->primary_data->data_lock, flags);
		memcpy(&(config.chist_config[i]), &(chist_data->primary_data->chist_config[i]),
			sizeof(chist_data->primary_data->chist_config[i]));
		spin_unlock_irqrestore(&chist_data->primary_data->data_lock, flags);
	}
	mtk_chist_user_cmd(comp, handle, CHIST_CONFIG, &config);
}

static void mtk_chist_get_resolution(struct mtk_ddp_comp *comp,
		struct mtk_ddp_config *cfg, unsigned int *width, unsigned int *height)
{
	struct drm_crtc *crtc = &(comp->mtk_crtc->base);
	struct mtk_disp_chist *chist_data = comp_to_chist(comp);
	bool is_dual = comp->mtk_crtc->is_dual_pipe;

	*height = cfg->h;
	*width = is_dual ? cfg->w / 2 : cfg->w;

	if (cfg->tile_overhead.is_support) {
		if (chist_data->path_order) {
			if (chist_data->is_right_pipe) {
				chist_data->tile_overhead = cfg->tile_overhead.right_overhead;
				*width = cfg->tile_overhead.right_in_width;
			} else {
				chist_data->tile_overhead = cfg->tile_overhead.left_overhead;
				*width = cfg->tile_overhead.left_in_width;
			}
			*height = crtc->mode.vdisplay;
		} else {
			*width += chist_data->tile_overhead;
		}
	} else if (chist_data->path_order) {
		*width = is_dual ? crtc->mode.hdisplay / 2 : crtc->mode.hdisplay;
		*height = crtc->mode.vdisplay;
	}
	if (!chist_data->is_right_pipe) {
		chist_data->primary_data->frame_width = chist_data->path_order ?
						crtc->mode.hdisplay : cfg->w;
		chist_data->primary_data->frame_height = *height;

		chist_data->primary_data->pipe_width = is_dual ?
			chist_data->primary_data->frame_width / 2
			: chist_data->primary_data->frame_width;
	}

}

static void mtk_chist_config(struct mtk_ddp_comp *comp,
			     struct mtk_ddp_config *cfg,
			     struct cmdq_pkt *handle)
{
	unsigned int width, height, value;
	int i = 0;
	struct mtk_disp_chist *chist_data = comp_to_chist(comp);

	mtk_chist_get_resolution(comp, cfg, &width, &height);

	DDPINFO("%s, chist:%s\n", __func__, mtk_dump_comp_str(comp));
	// rgb 2 yuv regs
	for (; i < DISP_CHIST_YUV_PARAM_COUNT; i++) {
		if (i >= DISP_CHIST_POST_PARAM_INDEX)
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_CHIST_Y2R_PAPA_POST_A0 +
				(i % DISP_CHIST_POST_PARAM_INDEX) * 4,
				g_rgb_2_yuv[sel_index][i], ~0);
		else
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_CHIST_Y2R_PAPA_R0 + i * 4,
				g_rgb_2_yuv[sel_index][i], ~0);
	}
	cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + DISP_CHIST_EN,
				   0x1, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa + DISP_CHIST_SIZE,
			   (width << 16) | height, ~0);
	if (chist_data->data->need_bypass_shadow)
		value = REG_FLD_VAL((FLD_BYPASS_SHADOW), 1);
	else
		value = REG_FLD_VAL((FLD_FORCE_COMMIT), 1) |
				REG_FLD_VAL((FLD_APB_MCYC_RD), 1);
	cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + DISP_CHIST_SHADOW_CTRL,
				   value, ~0);

	if (!chist_data->is_right_pipe) {
		disp_chist_set_interrupt(comp, 1, handle);
		if (chist_data->primary_data->need_restore)
			disp_chist_restore_setting(comp, handle);
		else
			mtk_chist_bypass(comp, 1, handle);
	}
}

static int mtk_chist_frame_config(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data, unsigned int data_size)
{
	struct drm_mtk_chist_config *config = data;
	struct mtk_disp_chist *chist = comp_to_chist(comp);
	int i = 0;

	if (config == NULL || sizeof(struct drm_mtk_chist_config) != data_size)
		return -EINVAL;

	if (config->config_channel_count == 0 ||
			config->config_channel_count > DISP_CHIST_CHANNEL_COUNT) {
		DDPPR_ERR("%s, invalid config channel count:%u\n",
				__func__, config->config_channel_count);
		return -EINVAL;
	}

	if ((config->device_id & 0xffff) != chist->path_order)
		return 0;

	DDPINFO("%s  chist id:%d, caller:%d, config count:%d\n", __func__,
		config->device_id, config->caller, config->config_channel_count);

	if (config->caller == MTK_DRM_CHIST_CALLER_HWC) {
		unsigned int channel_id = DISP_CHIST_HWC_CHANNEL_INDEX;

		for (; i < config->config_channel_count &&
			channel_id < DISP_CHIST_CHANNEL_COUNT; i++) {
			config->chist_config[i].channel_id = channel_id;
			channel_id++;
		}
	}
	chist->primary_data->present_fence = 0;
	chist->primary_data->need_restore = 1;
	chist->primary_data->pre_frame_width = 0;
	DDPINFO("%s --\n", __func__);

	return mtk_chist_user_cmd(comp, handle, CHIST_CONFIG, data);
}

int mtk_chist_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	      enum mtk_ddp_io_cmd cmd, void *params)
{
	switch (cmd) {
	case PQ_FILL_COMP_PIPE_INFO:
	{
		struct mtk_disp_chist *data = comp_to_chist(comp);
		bool *is_right_pipe = &data->is_right_pipe;
		int ret, *path_order = &data->path_order;
		struct mtk_ddp_comp **companion = &data->companion;
		struct mtk_disp_chist *companion_data;

		if (atomic_read(&comp->mtk_crtc->pq_data->pipe_info_filled) == 1)
			break;
		ret = mtk_pq_helper_fill_comp_pipe_info(comp, path_order, is_right_pipe, companion);
		if (!ret && comp->mtk_crtc->is_dual_pipe && data->companion) {
			companion_data = comp_to_chist(data->companion);
			companion_data->path_order = data->path_order;
			companion_data->is_right_pipe = !data->is_right_pipe;
			companion_data->companion = comp;
		}
		mtk_chist_primary_data_init(comp);
		if (comp->mtk_crtc->is_dual_pipe && data->companion)
			mtk_chist_primary_data_init(data->companion);
	}
		break;
	default:
		break;
	}

	return 0;
}

static int mtk_disp_chist_bind(struct device *dev, struct device *master,
			       void *data)
{
	struct mtk_disp_chist *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPINFO("%s\n", __func__);

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_disp_chist_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_chist *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	DDPINFO("%s\n", __func__);

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_chist_component_ops = {
	.bind = mtk_disp_chist_bind, .unbind = mtk_disp_chist_unbind,
};

static void mtk_get_hist_dual_pipe(struct mtk_ddp_comp *comp,
	unsigned int i, int sum_bins)
{
	unsigned int j = 0;
	struct mtk_disp_chist *data = comp_to_chist(comp);
	struct mtk_ddp_comp *dual_comp = data->companion;
	struct mtk_disp_chist_primary *prim_data = data->primary_data;

	if (!dual_comp)
		return;
	// select channel id
	writel(0x30 | i, dual_comp->regs + DISP_CHIST_APB_READ);

	if (i >= DISP_CHIST_CHANNEL_COUNT)
		return;

	for (; j < sum_bins; j++) {
		if (prim_data->chist_config[i].roi_start_x >= prim_data->pipe_width)
			// read right
			prim_data->disp_hist[i].hist[j] = readl(dual_comp->regs
				+ DISP_CHIST_SRAM_R_IF) >> chist_shift_num(comp);
		else if (prim_data->chist_config[i].roi_end_x < prim_data->pipe_width)
			// read left
			prim_data->disp_hist[i].hist[j] = readl(comp->regs + DISP_CHIST_SRAM_R_IF)
				>> chist_shift_num(comp);
		else {
			if (prim_data->block_config[i].sum_column > 1) {
				int current_column = (j / prim_data->chist_config[i].bin_count)
					% prim_data->block_config[i].sum_column;

				if (current_column < prim_data->block_config[i].left_column) {
					prim_data->disp_hist[i].hist[j] = readl(comp->regs
						+ DISP_CHIST_SRAM_R_IF) >> chist_shift_num(comp);

					if (prim_data->block_config[i].merge_column >= 0 &&
						prim_data->block_config[i].merge_column
						== current_column) {
						prim_data->disp_hist[i].hist[j] +=
							(readl(dual_comp->regs
							+ DISP_CHIST_SRAM_R_IF) - 0x10)
							>> chist_shift_num(comp);
					}
				} else {
					prim_data->disp_hist[i].hist[j] = readl(dual_comp->regs
						+ DISP_CHIST_SRAM_R_IF) >> chist_shift_num(comp);
				}
			} else {
				prim_data->disp_hist[i].hist[j] = readl(comp->regs
					+ DISP_CHIST_SRAM_R_IF) >> chist_shift_num(comp);
				prim_data->disp_hist[i].hist[j] += (readl(dual_comp->regs
					+ DISP_CHIST_SRAM_R_IF) - 0x10) >> chist_shift_num(comp);
			}
		}
	}
}

static void mtk_get_chist(struct mtk_ddp_comp *comp)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = NULL;
	struct mtk_drm_private *priv = NULL;
	unsigned long flags, clock_flags;
	int max_bins = 0;
	unsigned int i = 0;
	unsigned int cur_present_fence;
	struct mtk_disp_chist *data = comp_to_chist(comp);
	struct mtk_disp_chist_primary *prim_data = data->primary_data;

	if (mtk_crtc == NULL)
		return;

	crtc = &mtk_crtc->base;
	priv = crtc->dev->dev_private;

	if (prim_data->pre_frame_width == 0)
		prim_data->pre_frame_width = prim_data->frame_width;
	else if (prim_data->pre_frame_width != prim_data->frame_width)
		return;//need new config by current resolution

	spin_lock_irqsave(&prim_data->power_lock, clock_flags);

	if (atomic_read(&(prim_data->clock_on)) == 0) {
		spin_unlock_irqrestore(&prim_data->power_lock, clock_flags);
		return;
	}
	spin_lock_irqsave(&prim_data->data_lock, flags);
	for (; i < DISP_CHIST_CHANNEL_COUNT; i++) {
		prim_data->chist_config[i].channel_id = i;
		if (prim_data->chist_config[i].enabled) {
			prim_data->disp_hist[i].bin_count = prim_data->chist_config[i].bin_count;
			prim_data->disp_hist[i].color_format
				= prim_data->chist_config[i].color_format;
			prim_data->disp_hist[i].channel_id = i;

			if (i >= DISP_CHIST_HWC_CHANNEL_INDEX)
				max_bins = DISP_CHIST_MAX_BIN_LOW;
			else
				max_bins = DISP_CHIST_MAX_BIN;

			// select channel id
			writel(0x30 | i, comp->regs + DISP_CHIST_APB_READ);

			if (mtk_crtc->is_dual_pipe)
				mtk_get_hist_dual_pipe(comp, i, max_bins);
			else {
				int j = 0;

				for (; j < max_bins; j++) {
					prim_data->disp_hist[i].hist[j] = readl(comp->regs
						+ DISP_CHIST_SRAM_R_IF) >> chist_shift_num(comp);
				}
			}
		}
	}

	spin_unlock_irqrestore(&prim_data->data_lock, flags);
	spin_unlock_irqrestore(&prim_data->power_lock, clock_flags);
	cur_present_fence = *(unsigned int *)(mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_PRESENT_FENCE(0)));
	if (cur_present_fence != 0) {
		if (prim_data->present_fence == cur_present_fence - 1)
			prim_data->present_fence = cur_present_fence;
		else
			prim_data->present_fence = cur_present_fence - 1;
	}
}

static int mtk_chist_read_kthread(void *data)
{
	struct mtk_ddp_comp *comp = (struct mtk_ddp_comp *)data;
	struct mtk_disp_chist *chist_data = comp_to_chist(comp);

	while (!kthread_should_stop()) {
		int ret = 0;
		int pm_ret = 0;

		if (atomic_read(&(chist_data->primary_data->irq_event)) == 0) {
			DDPDBG("%s: wait_event_interruptible ++ ", __func__);
			ret = wait_event_interruptible(chist_data->primary_data->event_wq,
				atomic_read(&(chist_data->primary_data->irq_event)) == 1);
			if (ret < 0)
				DDPPR_ERR("wait %s fail, ret=%d\n", __func__, ret);
			else
				DDPDBG("%s: wait_event_interruptible -- ", __func__);
		} else {
			DDPDBG("%s: get_irq = 0", __func__);
		}
		atomic_set(&(chist_data->primary_data->irq_event), 0);
		pm_ret = mtk_vidle_pq_power_get(__func__);
		mtk_get_chist((struct mtk_ddp_comp *)data);
		if (!pm_ret)
			mtk_vidle_pq_power_put(__func__);
	}
	return 0;
}

static void mtk_chist_primary_data_init(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_chist *chist_data = comp_to_chist(comp);
	struct mtk_disp_chist *companion_data = comp_to_chist(chist_data->companion);
	char thread_name[20] = {0};
	int len = 0;

	if (chist_data->is_right_pipe) {
		kfree(chist_data->primary_data);
		chist_data->primary_data = NULL;
		chist_data->primary_data = companion_data->primary_data;
		return;
	}

	// init primary data
	init_waitqueue_head(&(chist_data->primary_data->event_wq));
	spin_lock_init(&(chist_data->primary_data->power_lock));
	spin_lock_init(&(chist_data->primary_data->data_lock));

	memset(&(chist_data->primary_data->block_config), 0,
			sizeof(chist_data->primary_data->block_config));
	memset(&(chist_data->primary_data->chist_config), 0,
			sizeof(chist_data->primary_data->chist_config));
	memset(&(chist_data->primary_data->disp_hist), 0,
			sizeof(chist_data->primary_data->disp_hist));

	atomic_set(&(chist_data->primary_data->irq_event), 0);
	atomic_set(&(chist_data->primary_data->clock_on), 0);
	chist_data->primary_data->need_restore = 0;
	chist_data->primary_data->pre_frame_width = 0;
	chist_data->primary_data->present_fence = 0;
	chist_data->primary_data->pipe_width = 0;
	chist_data->primary_data->frame_width = 0;
	chist_data->primary_data->frame_height = 0;

	// start thread for hist read
	len = sprintf(thread_name, "mtk_chist_read_%d", comp->id);
	if (len < 0)
		strcpy(thread_name, "mtk_chist_read_0");
	kthread_run(mtk_chist_read_kthread,
		comp, thread_name);
}

void mtk_chist_first_cfg(struct mtk_ddp_comp *comp,
	       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	DDPINFO("%s, comp->id:%s\n", __func__, mtk_dump_comp_str(comp));
	mtk_chist_config(comp, cfg, handle);
}

static irqreturn_t mtk_disp_chist_irq_handler(int irq, void *dev_id)
{
	struct mtk_disp_chist *chist = dev_id;
	struct mtk_ddp_comp *comp = &chist->ddp_comp;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	unsigned int intsta;
	irqreturn_t ret = IRQ_NONE;

	if (IS_ERR_OR_NULL(chist))
		return IRQ_NONE;

	if (mtk_drm_top_clk_isr_get("chist_irq") == false) {
		DDPIRQ("%s, top clk off\n", __func__);
		return IRQ_NONE;
	}

	mtk_crtc = chist->ddp_comp.mtk_crtc;
	if (!mtk_crtc) {
		DDPPR_ERR("%s mtk_crtc is NULL\n", __func__);
		ret = IRQ_NONE;
		goto out;
	}

	intsta = readl(comp->regs + DISP_CHIST_INSTA);
	if (intsta & 0x2) {
		writel(0, comp->regs + DISP_CHIST_INSTA);
		atomic_set(&(chist->primary_data->irq_event), 1);
		wake_up_interruptible(&chist->primary_data->event_wq);
	}

	ret = IRQ_HANDLED;
out:
	mtk_drm_top_clk_isr_put("chist_irq");
	return ret;
}

/* don't need start funcs, chist will start by ioctl_set_config*/
static const struct mtk_ddp_comp_funcs mtk_disp_chist_funcs = {
	.config = mtk_chist_config,
	.first_cfg = mtk_chist_first_cfg,
	.bypass = mtk_chist_bypass,
	.user_cmd = mtk_chist_user_cmd,
	.prepare = mtk_chist_prepare,
	.unprepare = mtk_chist_unprepare,
	.pq_frame_config = mtk_chist_frame_config,
	.io_cmd = mtk_chist_io_cmd,
};

static int mtk_disp_chist_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_chist *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPINFO("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	priv->primary_data = kzalloc(sizeof(*priv->primary_data), GFP_KERNEL);
	if (priv->primary_data == NULL) {
		ret = -ENOMEM;
		DDPPR_ERR("Failed to alloc primary_data %d\n", ret);
		goto error_dev_init;
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_CHIST);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		ret = comp_id;
		goto error_primary;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_chist_funcs);
	if (ret != 0) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		ret = -ENOMEM;
		goto error_primary;
	}

	priv->data = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, priv);
	ret = devm_request_irq(dev, priv->ddp_comp.irq, mtk_disp_chist_irq_handler,
		IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev), priv);
	if (ret)
		dev_err(dev, "devm_request_irq fail: %d\n", ret);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_chist_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

error_primary:
	if (ret < 0)
		kfree(priv->primary_data);
error_dev_init:
	if (ret < 0)
		devm_kfree(dev, priv);

	DDPINFO("%s-\n", __func__);
	return ret;
}

static int mtk_disp_chist_remove(struct platform_device *pdev)
{
	struct mtk_disp_chist *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_chist_component_ops);

	mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	return 0;
}

static const struct mtk_disp_chist_data mt6983_chist_driver_data = {
	.support_shadow = true,
	.need_bypass_shadow = true,
	.module_count = 2,
	.color_format = DISP_CHIST_COLOR_FORMAT,
	.max_channel = 3,
	.max_bin = DISP_CHIST_MAX_BIN_LOW,
	.chist_shift_num = 5,
};

static const struct mtk_disp_chist_data mt6895_chist_driver_data = {
	.support_shadow = true,
	.need_bypass_shadow = true,
	.module_count = 2,
	.color_format = DISP_CHIST_COLOR_FORMAT,
	.max_channel = 3,
	.max_bin = DISP_CHIST_MAX_BIN_LOW,
	.chist_shift_num = 5,
};

static const struct mtk_disp_chist_data mt6879_chist_driver_data = {
	.support_shadow = true,
	.need_bypass_shadow = true,
	.module_count = 1,
	.color_format = DISP_CHIST_COLOR_FORMAT,
	.max_channel = 3,
	.max_bin = DISP_CHIST_MAX_BIN_LOW,
	.chist_shift_num = 5,
};

static const struct mtk_disp_chist_data mt6985_chist_driver_data = {
	.support_shadow = true,
	.need_bypass_shadow = true,
	.module_count = 2,
	.color_format = DISP_CHIST_COLOR_FORMAT,
	.max_channel = 3,
	.max_bin = DISP_CHIST_MAX_BIN_LOW,
	.chist_shift_num = 7,
};

static const struct mtk_disp_chist_data mt6897_chist_driver_data = {
	.support_shadow = true,
	.need_bypass_shadow = true,
	.module_count = 2,
	.color_format = DISP_CHIST_COLOR_FORMAT,
	.max_channel = 3,
	.max_bin = DISP_CHIST_MAX_BIN_LOW,
	.chist_shift_num = 7,
};

static const struct mtk_disp_chist_data mt6886_chist_driver_data = {
	.support_shadow = true,
	.need_bypass_shadow = true,
	.module_count = 1,
	.color_format = DISP_CHIST_COLOR_FORMAT,
	.max_channel = 3,
	.max_bin = DISP_CHIST_MAX_BIN_LOW,
	.chist_shift_num = 5,
};

static const struct mtk_disp_chist_data mt6989_chist_driver_data = {
	.support_shadow = true,
	.need_bypass_shadow = true,
	.module_count = 2,
	.color_format = DISP_CHIST_COLOR_FORMAT,
	.max_channel = 3,
	.max_bin = DISP_CHIST_MAX_BIN_LOW,
	.chist_shift_num = 7,
};

static const struct of_device_id mtk_disp_chist_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6983-disp-chist",
	  .data = &mt6983_chist_driver_data},
	{ .compatible = "mediatek,mt6895-disp-chist",
	  .data = &mt6895_chist_driver_data},
	{ .compatible = "mediatek,mt6879-disp-chist",
	  .data = &mt6879_chist_driver_data},
	{ .compatible = "mediatek,mt6985-disp-chist",
	  .data = &mt6985_chist_driver_data},
	{ .compatible = "mediatek,mt6897-disp-chist",
	  .data = &mt6897_chist_driver_data},
	{ .compatible = "mediatek,mt6886-disp-chist",
	  .data = &mt6886_chist_driver_data},
	{ .compatible = "mediatek,mt6989-disp-chist",
	  .data = &mt6989_chist_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_chist_driver_dt_match);

struct platform_driver mtk_disp_chist_driver = {
	.probe = mtk_disp_chist_probe,
	.remove = mtk_disp_chist_remove,
	.driver = {
			.name = "mediatek-disp-chist",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_chist_driver_dt_match,
		},
};
