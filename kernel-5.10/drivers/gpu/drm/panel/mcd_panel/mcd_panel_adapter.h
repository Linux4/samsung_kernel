/*
 * drivers/gpu/drm/panel/mcd_panel/mcd_panel_adapter.h
 *
 * Header file for Samsung Common LCD Driver.
 *
 * Copyright (c) 2022 Samsung Electronics
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __MCD_PANEL_ADAPTER_H__
#define __MCD_PANEL_ADAPTER_H__

#include <video/mipi_display.h>
#include <linux/printk.h>
#include <linux/bits.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/backlight.h>
#include <linux/version.h>

#include <drm/drm_bridge.h>
#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_property.h>
#include <drm/drm_mipi_dsi.h>

/* MCD Panel Driver header */
#include "mcd_panel_header.h"

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0))
#include "../../mediatek/mediatek_v2/mtk_cust.h"
#include "../../mediatek/mediatek_v2/mtk_debug.h"
#include "../../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#else
#include "../../mediatek/mtk_debug.h"
#include "../../mediatek/mtk_panel_ext.h"
#endif
#endif

#define MCD_PANEL_PROBE_DELAY_MSEC (5000)

#define call_mcd_panel_func(p, func, args...) \
	(((p) && (p)->funcs && (p)->funcs->func) ? (p)->funcs->func(p, ##args) : -EINVAL)

#define MTK_MODE_ARG(m) \
	(m)->output_mode, (m)->data_rate, (m)->dsc_params.enable

#define MTK_MODE_FMT    "%d %d %d"

enum mcd_drm_state {
	MCD_DRM_STATE_DISABLED,
	MCD_DRM_STATE_ENABLED,
};

enum {
	MCD_PANEL_WQ_VSYNC = 0,
	MCD_PANEL_WQ_FSYNC = 1,
	MCD_PANEL_WQ_DISPON = 2,
	MCD_PANEL_WQ_PANEL_PROBE = 3,
	MAX_MCD_PANEL_WQ,
};

enum {
	PANEL_CMD_LOG_DSI_TX,
	PANEL_CMD_LOG_DSI_RX,
};

struct mcd_panel_wq {
	int index;
	wait_queue_head_t wait;
	struct workqueue_struct *wq;
	struct delayed_work dwork;
	atomic_t count;
	void *data;
	int ret;
	char *name;
};

#define wq_to_mtk_panel(w)	\
	container_of(w, struct mtk_panel, wqs[((w)->index)])

extern int bypass_display;
extern int panel_cmd_log_level;
extern int commit_retry;

struct mtk_panel_mode {
	/* @mode: drm display mode info */
	struct drm_display_mode mode;
	/* @mtk_mode: mtk driver specific mode info */
	struct mtk_panel_params mtk_ext;
};

struct mtk_ddi_spec {
	u32 data_lane_cnt;
	u32 data_rate;
	u32 size_mm[2];
	u32 multi_drop;
};

struct mtk_panel_desc {
	/* const */ struct mtk_panel_mode *modes;
	size_t num_modes;
};

struct mtk_panel {
	struct device *dev;
	struct drm_panel panel;
	struct drm_bridge bridge;
	struct drm_encoder *encoder;
	struct mipi_dsi_device *dsi;

	struct mtk_ddi_spec	spec;
	struct mtk_panel_desc *desc;
	const struct mtk_panel_mode *current_mode;

	bool enabled;
	bool mcd_panel_probed;
	struct mutex probe_lock;
	struct panel_device *mcd_panel_dev;
	enum mcd_drm_state mcd_drm_state;
	struct rw_semaphore mcd_drm_state_lock;
	struct mcd_panel_wq wqs[MAX_MCD_PANEL_WQ];
	struct ddi_properties ddi_props;

	struct mtk_ddic_dsi_msg mtk_dsi_msg;

#if IS_ENABLED(CONFIG_USDM_PANEL_MASK_LAYER)
	bool fingerprint_mask;
#endif
};

extern int mtk_wait_frame_start(unsigned int timeout);
extern int mtk_wait_frame_done(unsigned int timeout);

#endif /* __MCD_PANEL_ADAPTER_H__ */
