// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef __SPRD_DPU1_H__
#define __SPRD_DPU1_H__

#include <linux/delay.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/bug.h>
#include <video/videomode.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_vblank.h>
#include <uapi/drm/drm_mode.h>
#include "disp_lib.h"

#define DRM_FORMAT_P010		fourcc_code('P', '0', '1', '0')

#define DRM_MODE_BLEND_PREMULTI		2
#define DRM_MODE_BLEND_COVERAGE		1
#define DRM_MODE_BLEND_PIXEL_NONE	0

#define DISPC_INT_DONE_MASK		BIT(0)
#define DISPC_INT_ERR_MASK		BIT(2)
#define DISPC_INT_UPDATE_DONE_MASK	BIT(4)
#define DISPC_INT_DPI_VSYNC_MASK	BIT(5)
#define DISPC_INT_WB_DONE_MASK		BIT(6)
#define DISPC_INT_WB_FAIL_MASK		BIT(7)

enum {
	SPRD_DISPC_IF_DBI = 0,
	SPRD_DISPC_IF_DPI,
	SPRD_DISPC_IF_EDPI,
	SPRD_DISPC_IF_LIMIT
};

enum {
	SPRD_IMG_DATA_ENDIAN_B0B1B2B3 = 0,
	SPRD_IMG_DATA_ENDIAN_B3B2B1B0,
	SPRD_IMG_DATA_ENDIAN_B2B3B0B1,
	SPRD_IMG_DATA_ENDIAN_B1B0B3B2,
	SPRD_IMG_DATA_ENDIAN_LIMIT
};

struct sprd_dpu_layer {
	u8 index;
	u8 planes;
	u32 addr[4];
	u32 pitch[4];
	s16 src_x;
	s16 src_y;
	s16 src_w;
	s16 src_h;
	s16 dst_x;
	s16 dst_y;
	u16 dst_w;
	u16 dst_h;
	u32 format;
	u32 alpha;
	u32 blending;
	u32 rotation;
	u32 xfbc;
	u32 height;
	u32 header_size_r;
	u32 header_size_y;
	u32 header_size_uv;
	u32 y2r_coef;
	u8 pallete_en;
	u32 pallete_color;
};

struct dpu_capability {
	u32 max_layers;
	const u32 *fmts_ptr;
	u32 fmts_cnt;
};

struct dpu_context;

struct dpu_core_ops {
	int (*parse_dt)(struct dpu_context *ctx,
			struct device_node *np);
	u32 (*version)(struct dpu_context *ctx);
	int (*init)(struct dpu_context *ctx);
	void (*uninit)(struct dpu_context *ctx);
	void (*run)(struct dpu_context *ctx);
	void (*stop)(struct dpu_context *ctx);
	void (*disable_vsync)(struct dpu_context *ctx);
	void (*enable_vsync)(struct dpu_context *ctx);
	u32 (*isr)(struct dpu_context *ctx);
	void (*ifconfig)(struct dpu_context *ctx);
	void (*write_back)(struct dpu_context *ctx, u8 count, bool debug);
	void (*flip)(struct dpu_context *ctx,
		     struct sprd_dpu_layer layers[], u8 count);
	int (*capability)(struct dpu_context *ctx,
			struct dpu_capability *cap);
	void (*bg_color)(struct dpu_context *ctx, u32 color);
	int (*modeset)(struct dpu_context *ctx,
			struct drm_display_mode *mode);
};

struct dpu_clk_ops {
	int (*parse_dt)(struct dpu_context *ctx,
			struct device_node *np);
	int (*init)(struct dpu_context *ctx);
	int (*uinit)(struct dpu_context *ctx);
	int (*enable)(struct dpu_context *ctx);
	int (*disable)(struct dpu_context *ctx);
	int (*update)(struct dpu_context *ctx, int clk_id, int val);
};

struct dpu_glb_ops {
	int (*parse_dt)(struct dpu_context *ctx,
			struct device_node *np);
	void (*enable)(struct dpu_context *ctx);
	void (*disable)(struct dpu_context *ctx);
	void (*reset)(struct dpu_context *ctx);
	void (*power)(struct dpu_context *ctx, int enable);
};

struct dpu_context {
	unsigned long base;
	u32 base_offset[2];
	const char *version;
	u32 corner_size;
	int irq;
	u8 id;
	bool is_inited;
	bool is_stopped;
	bool disable_flip;
	struct videomode vm;
	struct semaphore refresh_lock;
	struct work_struct wb_work;
	u32 wb_addr_p;
	irqreturn_t (*dpu_isr)(int irq, void *data);
	bool bypass_mode;
	u32 hdr_static_metadata[9];
	bool static_metadata_changed;
};

struct sprd_dpu {
	struct device dev;
	struct drm_crtc crtc;
	struct dpu_context ctx;
	struct dpu_core_ops *core;
	struct dpu_clk_ops *clk;
	struct dpu_glb_ops *glb;
	struct drm_display_mode *mode;
	struct sprd_dpu_layer *layers;
	u8 pending_planes;
};

extern struct list_head dpu1_core_head;
extern struct list_head dpu1_clk_head;
extern struct list_head dpu1_glb_head;

static inline struct sprd_dpu *crtc_to_dpu(struct drm_crtc *crtc)
{
	return crtc ? container_of(crtc, struct sprd_dpu, crtc) : NULL;
}

#define dpu_core_ops_register(entry) \
	disp_ops_register(entry, &dpu1_core_head)
#define dpu_clk_ops_register(entry) \
	disp_ops_register(entry, &dpu1_clk_head)
#define dpu_glb_ops_register(entry) \
	disp_ops_register(entry, &dpu1_glb_head)

#define dpu_core_ops_attach(str) \
	disp_ops_attach(str, &dpu1_core_head)
#define dpu_clk_ops_attach(str) \
	disp_ops_attach(str, &dpu1_clk_head)
#define dpu_glb_ops_attach(str) \
	disp_ops_attach(str, &dpu1_glb_head)

int sprd_dpu1_run(struct sprd_dpu *dpu);
int sprd_dpu1_stop(struct sprd_dpu *dpu);

#endif
