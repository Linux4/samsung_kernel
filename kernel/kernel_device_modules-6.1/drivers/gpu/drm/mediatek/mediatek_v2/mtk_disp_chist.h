/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __MTK_DISP_CHIST_H__
#define __MTK_DISP_CHIST_H__

#include <linux/uaccess.h>
#include <uapi/drm/mediatek_drm.h>
#define DISP_CHIST_CHANNEL_COUNT 7

struct mtk_disp_chist_data {
	bool support_shadow;
	bool need_bypass_shadow;
	unsigned int module_count;
	unsigned int color_format;
	unsigned int max_channel;
	unsigned int max_bin;
	unsigned int chist_shift_num;
};

struct mtk_disp_block_config {
	unsigned int blk_xofs;
	unsigned int left_column;
	unsigned int sum_column;
	int merge_column;
};

struct mtk_disp_chist_primary {
	struct mtk_disp_block_config block_config[DISP_CHIST_CHANNEL_COUNT];
	struct drm_mtk_channel_config chist_config[DISP_CHIST_CHANNEL_COUNT];
	struct drm_mtk_channel_hist disp_hist[DISP_CHIST_CHANNEL_COUNT];

	atomic_t irq_event;
	struct wait_queue_head event_wq;
	spinlock_t power_lock;
	spinlock_t data_lock;
	atomic_t clock_on;
	bool need_restore;
	unsigned int present_fence;
	unsigned int pipe_width;
	unsigned int pre_frame_width;
	unsigned int frame_width;
	unsigned int frame_height;
};

struct mtk_disp_chist {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_chist_data *data;

	unsigned int tile_overhead;
	bool is_right_pipe;
	int path_order;
	struct mtk_ddp_comp *companion;
	struct mtk_disp_chist_primary *primary_data;
};

static inline struct mtk_disp_chist *comp_to_chist(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_chist, ddp_comp);
}

int mtk_drm_ioctl_get_chist(struct drm_device *dev, void *data,
	struct drm_file *file_priv);


int mtk_drm_ioctl_get_chist_caps(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

int mtk_drm_ioctl_set_chist_config(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

void mtk_chist_set_tile_overhead(struct mtk_drm_crtc *mtk_crtc, int overhead, bool is_right);

#endif

