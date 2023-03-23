// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include "sprd_dpu1.h"

#define DISPC_INT_FBC_PLD_ERR_MASK	BIT(8)
#define DISPC_INT_FBC_HDR_ERR_MASK	BIT(9)

#define DPU_NORMAL_MODE	0
#define DPU_BYPASS_MODE	1

#define XFBC8888_HEADER_SIZE(w, h) (ALIGN((ALIGN((w), 16)) * \
				(ALIGN((h), 16)) / 16, 128))
#define XFBC8888_PAYLOAD_SIZE(w, h) (ALIGN((w), 16) * ALIGN((h), 16) * 4)
#define XFBC8888_BUFFER_SIZE(w, h) (XFBC8888_HEADER_SIZE(w, h) \
				+ XFBC8888_PAYLOAD_SIZE(w, h))

struct layer_reg {
	u32 addr[4];
	u32 ctrl;
	u32 size;
	u32 pitch;
	u32 pos;
	u32 alpha;
	u32 ck;
	u32 pallete;
	u32 crop_start;
};

struct dpu_reg {
	u32 dpu_version;
	u32 dpu_ctrl;
	u32 dpu_cfg0;
	u32 dpu_cfg1;
	u32 dpu_mode;
	u32 dpu_secure;
	u32 reserved_0x0018_0x001C[2];
	u32 panel_size;
	u32 reserved_0x0028[2];
	u32 bg_color;
	struct layer_reg layers[8];
	u32 wb_base_addr;
	u32 wb_ctrl;
	u32 wb_cfg;
	u32 wb_pitch;
	u32 reserved_0x01C0_0x01DC[8];
	u32 dpu_int_en;
	u32 dpu_int_clr;
	u32 dpu_int_sts;
	u32 dpu_int_raw;
	u32 dpi_ctrl;
	u32 dpi_h_timing;
	u32 dpi_v_timing;
	u32 reserved_0x035c[89];
	u32 dpu_sts[18];
	u32 reserved_0x03DC_0x03FC[22];
	u32 cursor_en;
	u32 cursor_size;
	u32 cursor_pos;
	u32 cursor_lut_addr;
	u32 cursor_lut_wdata;
	u32 cursor_lut_rdata;
	u32 reserved_0x041c[2];
	u32 sdp_cfg;
	u32 sdp_lut_addr;
	u32 sdp_lut_wdata;
	u32 sdp_lut_rdata;
};

struct dpu_cfg1 {
	u8 arqos_low;
	u8 arqos_high;
	u8 awqos_low;
	u8 awqos_high;
};

static struct dpu_cfg1 qos_cfg = {
	.arqos_low = 0xA,
	.arqos_high = 0xC,
	.awqos_low = 0xA,
	.awqos_high = 0xC,
};

static DECLARE_WAIT_QUEUE_HEAD(wait_queue);
static bool evt_update;
static bool evt_stop;
static int wb_en;
static int max_vsync_count;
static int vsync_count;
//static struct sprd_dpu_layer wb_layer;
static int wb_xfbc_en = 1;
static struct device_node *g_np;
module_param(wb_xfbc_en, int, 0644);
module_param(max_vsync_count, int, 0644);

static void dpu_clean_all(struct dpu_context *ctx);
static void dpu_layer(struct dpu_context *ctx,
		    struct sprd_dpu_layer *hwlayer);
static u32 dpu_get_version(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	return reg->dpu_version;
}

static int dpu_parse_dt(struct dpu_context *ctx,
				struct device_node *np)
{
	int ret = 0;
	struct device_node *qos_np = NULL;

	g_np = np;

	qos_np = of_parse_phandle(np, "sprd,qos", 0);
	if (!qos_np)
		pr_warn("can't find dpu qos cfg node\n");

	ret = of_property_read_u8(qos_np, "arqos-low",
					&qos_cfg.arqos_low);
	if (ret)
		pr_warn("read arqos-low failed, use default\n");

	ret = of_property_read_u8(qos_np, "arqos-high",
					&qos_cfg.arqos_high);
	if (ret)
		pr_warn("read arqos-high failed, use default\n");

	ret = of_property_read_u8(qos_np, "awqos-low",
					&qos_cfg.awqos_low);
	if (ret)
		pr_warn("read awqos_low failed, use default\n");

	ret = of_property_read_u8(qos_np, "awqos-high",
					&qos_cfg.awqos_high);
	if (ret)
		pr_warn("read awqos-high failed, use default\n");

	return ret;
}

static u32 dpu_isr(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u32 reg_val, int_mask = 0;

	reg_val = reg->dpu_int_sts;
	reg->dpu_int_clr = reg_val;

	/* disable err interrupt */
	if (reg_val & DISPC_INT_ERR_MASK)
		int_mask |= DISPC_INT_ERR_MASK;

	/* dpu update done isr */
	if (reg_val & DISPC_INT_UPDATE_DONE_MASK) {
		evt_update = true;
		wake_up_interruptible_all(&wait_queue);
	}

	/* dpu vsync isr */
	if (reg_val & DISPC_INT_DPI_VSYNC_MASK) {
		/* write back feature */
		if ((vsync_count == max_vsync_count) && wb_en)
			schedule_work(&ctx->wb_work);

		vsync_count++;
	}

	/* dpu stop done isr */
	if (reg_val & DISPC_INT_DONE_MASK) {
		evt_stop = true;
		wake_up_interruptible_all(&wait_queue);
	}

	/* dpu write back done isr */
	if (reg_val & DISPC_INT_WB_DONE_MASK) {
		/*
		 * The write back is a time-consuming operation. If there is a
		 * flip occurs before write back done, the write back buffer is
		 * no need to display. Otherwise the new frame will be covered
		 * by the write back buffer, which is not what we wanted.
		 */
		if ((vsync_count > max_vsync_count) && wb_en) {
			wb_en = false;
			schedule_work(&ctx->wb_work);
		}

		pr_debug("wb done\n");
	}

	/* dpu write back error isr */
	if (reg_val & DISPC_INT_WB_FAIL_MASK) {
		pr_err("dpu write back fail\n");
		/*give a new chance to write back*/
		wb_en = true;
		vsync_count = 0;
	}

	/* dpu afbc payload error isr */
	if (reg_val & DISPC_INT_FBC_PLD_ERR_MASK) {
		int_mask |= DISPC_INT_FBC_PLD_ERR_MASK;
		pr_err("dpu afbc payload error\n");
	}

	/* dpu afbc header error isr */
	if (reg_val & DISPC_INT_FBC_HDR_ERR_MASK) {
		int_mask |= DISPC_INT_FBC_HDR_ERR_MASK;
		pr_err("dpu afbc header error\n");
	}

	reg->dpu_int_clr = reg_val;
	reg->dpu_int_en &= ~int_mask;

	return reg_val;
}

static int dpu_wait_stop_done(struct dpu_context *ctx)
{
	int rc;

	if (ctx->is_stopped)
		return 0;

	/* wait for stop done interrupt */
	rc = wait_event_interruptible_timeout(wait_queue, evt_stop,
					       msecs_to_jiffies(500));
	evt_stop = false;

	ctx->is_stopped = true;

	if (!rc) {
		/* time out */
		pr_err("dpu wait for stop done time out!\n");
		return -1;
	}

	return 0;
}

static int dpu_wait_update_done(struct dpu_context *ctx)
{
	int rc;

	/* clear the event flag before wait */
	evt_update = false;

	/* wait for reg update done interrupt */
	rc = wait_event_interruptible_timeout(wait_queue, evt_update,
					       msecs_to_jiffies(500));

	if (!rc) {
		/* time out */
		pr_err("dpu wait for reg update done time out!\n");
		return -1;
	}

	return 0;
}

static void dpu_stop(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->dpu_ctrl |= BIT(1);

	dpu_wait_stop_done(ctx);
	pr_info("dpu stop\n");
}

static void dpu_run(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->dpu_ctrl |= BIT(0);

	ctx->is_stopped = false;

	pr_info("dpu1 run\n");
}

#if 0
static void dpu_wb_trigger(struct dpu_context *ctx,
							u8 count, bool debug)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	int mode_width  = reg->panel_size & 0xFFFF;
	int mode_height = reg->panel_size >> 16;

	wb_layer.dst_w = mode_width;
	wb_layer.dst_h = mode_height;
	wb_layer.xfbc = wb_xfbc_en;
	wb_layer.pitch[0] = ALIGN(mode_width, 16) * 4;
	wb_layer.header_size_r = XFBC8888_HEADER_SIZE(mode_width,
					mode_height) / 128;

	reg->wb_pitch = ALIGN((mode_width), 16);

	wb_layer.xfbc = wb_xfbc_en;

	if (wb_xfbc_en && !debug)
		reg->wb_cfg = (wb_layer.header_size_r << 16) | BIT(0);
	else
		reg->wb_cfg = 0;

	reg->wb_base_addr = ctx->wb_addr_p;
}

static void dpu_wb_flip(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	dpu_clean_all(ctx);
	dpu_layer(ctx, &wb_layer);

	reg->dpu_ctrl |= BIT(2);
	dpu_wait_update_done(ctx);

	pr_debug("write back flip\n");
}

static void dpu_wb_work_func(struct work_struct *data)
{
	struct dpu_context *ctx =
		container_of(data, struct dpu_context, wb_work);

	down(&ctx->refresh_lock);

	if (!ctx->is_inited) {
		up(&ctx->refresh_lock);
		pr_err("dpu is not initialized\n");
		return;
	}

	if (ctx->disable_flip) {
		up(&ctx->refresh_lock);
		pr_warn("dpu flip is disabled\n");
		return;
	}

	if (wb_en && (vsync_count > max_vsync_count))
		dpu_wb_trigger(ctx, 1, false);
	else if (!wb_en)
		dpu_wb_flip(ctx);

	up(&ctx->refresh_lock);
}

static int dpu_wb_buf_alloc(struct sprd_dpu *dpu, size_t size,
			 u32 *paddr)
{
	struct device_node *node;
	u64 size64;
	struct resource r;

	node = of_parse_phandle(dpu->dev.of_node,
					"sprd,wb-memory", 0);
	if (!node) {
		pr_err("no sprd,wb-memory specified\n");
		return -EINVAL;
	}

	if (of_address_to_resource(node, 0, &r)) {
		pr_err("invalid wb reserved memory node!\n");
		return -EINVAL;
	}

	*paddr = r.start;
	size64 = resource_size(&r);

	if (size64 < size) {
		pr_err("unable to obtain enough wb memory\n");
		return -ENOMEM;
	}

	return 0;
}

static int dpu_write_back_config(struct dpu_context *ctx)
{
	int ret;
	static int need_config = 1;
	size_t wb_buf_size;
	struct sprd_dpu *dpu =
		(struct sprd_dpu *)container_of(ctx, struct sprd_dpu, ctx);

	if (!need_config) {
		pr_debug("write back has configed\n");
		return 0;
	}

	wb_buf_size = XFBC8888_BUFFER_SIZE(dpu->mode->hdisplay,
						dpu->mode->vdisplay);
	pr_info("use wb_reserved memory for writeback, size:0x%zx\n",
		wb_buf_size);
	ret = dpu_wb_buf_alloc(dpu,
		wb_buf_size, &ctx->wb_addr_p);
	if (ret) {
		max_vsync_count = 0;
		return -1;
	}

	wb_layer.index = 7;
	wb_layer.planes = 1;
	wb_layer.alpha = 0xff;
	wb_layer.format = DRM_FORMAT_ABGR8888;
	wb_layer.addr[0] = ctx->wb_addr_p;

	max_vsync_count = 4;
	need_config = 0;

	INIT_WORK(&ctx->wb_work, dpu_wb_work_func);

	return 0;
}
#endif

static void dpu_sdp_set(struct dpu_context *ctx,
			u32 size, u32 *data)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	int i;
	/* hardware SDP transfer num, 36 bytes unit */
	u32 num_sdp = size * 4 / 36;

	for (i = 0; i < size; i++) {
		reg->sdp_lut_addr = i;
		reg->sdp_lut_wdata = data[i];
	}

	reg->sdp_cfg = (num_sdp << 8) | BIT(0);
}

static void dpu_sdp_disable(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->sdp_cfg = 0;
}

static int dpu_init(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u32 size;

	/* set bg color */
	reg->bg_color = 0;

	/* set dpu output size */
	size = (ctx->vm.vactive << 16) | ctx->vm.hactive;
	reg->panel_size = size;

	reg->dpu_cfg0 = 0;
	reg->dpu_cfg1 = (qos_cfg.awqos_high << 12) |
		(qos_cfg.awqos_low << 8) |
		(qos_cfg.arqos_high << 4) |
		(qos_cfg.arqos_low) | BIT(22) | BIT(23) | BIT(24);

	if (ctx->is_stopped)
		dpu_clean_all(ctx);

	reg->dpu_int_clr = 0xffff;

	dpu_sdp_disable(ctx);
	reg->cursor_en = 0;
	//dpu_write_back_config(ctx);

	ctx->base_offset[0] = 0x0;
	ctx->base_offset[1] = sizeof(struct dpu_reg) / 4;

	return 0;
}

static void dpu_uninit(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->dpu_int_en = 0;
	reg->dpu_int_clr = 0xff;
}

enum {
	DPU_LAYER_FORMAT_YUV422_2PLANE,
	DPU_LAYER_FORMAT_YUV420_2PLANE,
	DPU_LAYER_FORMAT_YUV420_3PLANE,
	DPU_LAYER_FORMAT_ARGB8888,
	DPU_LAYER_FORMAT_RGB565,
	DPU_LAYER_FORMAT_YUV420_10,
	DPU_LAYER_FORMAT_XFBC_ARGB8888 = 8,
	DPU_LAYER_FORMAT_XFBC_RGB565,
	DPU_LAYER_FORMAT_XFBC_YUV420,
	DPU_LAYER_FORMAT_XFBC_YUV420_10,
	DPU_LAYER_FORMAT_MAX_TYPES,
};

enum {
	DPU_LAYER_ROTATION_0,
	DPU_LAYER_ROTATION_90,
	DPU_LAYER_ROTATION_180,
	DPU_LAYER_ROTATION_270,
	DPU_LAYER_ROTATION_0_M,
	DPU_LAYER_ROTATION_90_M,
	DPU_LAYER_ROTATION_180_M,
	DPU_LAYER_ROTATION_270_M,
};

static u32 to_dpu_rotation(u32 angle)
{
	u32 rot = DPU_LAYER_ROTATION_0;

	switch (angle) {
	case 0:
	case DRM_MODE_ROTATE_0:
		rot = DPU_LAYER_ROTATION_0;
		break;
	case DRM_MODE_ROTATE_90:
		rot = DPU_LAYER_ROTATION_90;
		break;
	case DRM_MODE_ROTATE_180:
		rot = DPU_LAYER_ROTATION_180;
		break;
	case DRM_MODE_ROTATE_270:
		rot = DPU_LAYER_ROTATION_270;
		break;
	case (DRM_MODE_REFLECT_Y | DRM_MODE_ROTATE_0):
		rot = DPU_LAYER_ROTATION_180_M;
		break;
	case (DRM_MODE_REFLECT_Y | DRM_MODE_ROTATE_90):
		rot = DPU_LAYER_ROTATION_90_M;
		break;
	case (DRM_MODE_REFLECT_X | DRM_MODE_ROTATE_0):
		rot = DPU_LAYER_ROTATION_0_M;
		break;
	case (DRM_MODE_REFLECT_X | DRM_MODE_ROTATE_90):
		rot = DPU_LAYER_ROTATION_270_M;
		break;
	default:
		pr_err("rotation convert unsupport angle (drm)= 0x%x\n", angle);
		break;
	}

	return rot;
}

static u32 dpu_img_ctrl(u32 format, u32 blending, u32 compression, u32 y2r_coef,
		u32 rotation)
{
	int reg_val = 0;

	/* layer enable */
	reg_val |= BIT(0);

	switch (format) {
	case DRM_FORMAT_BGRA8888:
		/* BGRA8888 -> ARGB8888 */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 8;
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= (DPU_LAYER_FORMAT_XFBC_ARGB8888 << 4);
		else
			reg_val |= (DPU_LAYER_FORMAT_ARGB8888 << 4);
		break;
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_RGBA8888:
		/* RGBA8888 -> ABGR8888 */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 8;
	case DRM_FORMAT_ABGR8888:
		/* rb switch */
		reg_val |= BIT(10);
	case DRM_FORMAT_ARGB8888:
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= (DPU_LAYER_FORMAT_XFBC_ARGB8888 << 4);
		else
			reg_val |= (DPU_LAYER_FORMAT_ARGB8888 << 4);
		break;
	case DRM_FORMAT_XBGR8888:
		/* rb switch */
		reg_val |= BIT(10);
	case DRM_FORMAT_XRGB8888:
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= (DPU_LAYER_FORMAT_XFBC_ARGB8888 << 4);
		else
			reg_val |= (DPU_LAYER_FORMAT_ARGB8888 << 4);
		break;
	case DRM_FORMAT_BGR565:
		/* rb switch */
		reg_val |= BIT(12);
	case DRM_FORMAT_RGB565:
		if (compression)
			/* XFBC-RGB565 */
			reg_val |= (DPU_LAYER_FORMAT_XFBC_RGB565 << 4);
		else
			reg_val |= (DPU_LAYER_FORMAT_RGB565 << 4);
		break;
	case DRM_FORMAT_NV12:
		if (compression)
			/* 2-Lane: Yuv420 */
			reg_val |= DPU_LAYER_FORMAT_XFBC_YUV420 << 4;
		else
			reg_val |= DPU_LAYER_FORMAT_YUV420_2PLANE << 4;
		/* Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/* UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 10;
		break;
	case DRM_FORMAT_NV21:
		if (compression)
			/* 2-Lane: Yuv420 */
			reg_val |= DPU_LAYER_FORMAT_XFBC_YUV420 << 4;
		else
			reg_val |= DPU_LAYER_FORMAT_YUV420_2PLANE << 4;
		/* Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/* UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 10;
		break;
	case DRM_FORMAT_NV16:
		/* 2-Lane: Yuv422 */
		reg_val |= DPU_LAYER_FORMAT_YUV422_2PLANE << 4;
		/* Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 8;
		/* UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 10;
		break;
	case DRM_FORMAT_NV61:
		/* 2-Lane: Yuv422 */
		reg_val |= DPU_LAYER_FORMAT_YUV422_2PLANE << 4;
		/* Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/* UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 10;
		break;
	case DRM_FORMAT_YUV420:
		reg_val |= DPU_LAYER_FORMAT_YUV420_3PLANE << 4;
		/* Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/* UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 10;
		break;
	case DRM_FORMAT_P010:
		if (compression)
			/* 2-Lane: Yuv420 10bit*/
			reg_val |= DPU_LAYER_FORMAT_XFBC_YUV420_10 << 4;
		else
			reg_val |= DPU_LAYER_FORMAT_YUV420_10 << 4;
		/* Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/* UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 10;
		break;
	default:
		pr_err("error: invalid format %c%c%c%c\n", format,
						format >> 8,
						format >> 16,
						format >> 24);
		break;
	}

	switch (blending) {
	case DRM_MODE_BLEND_PIXEL_NONE:
		/* don't do blending, maybe RGBX */
		/* alpha mode select - layer alpha */
		reg_val |= BIT(2);
		break;
	case DRM_MODE_BLEND_COVERAGE:
		/* alpha mode select - combo alpha */
		reg_val |= BIT(3);
		/* blending mode select - normal mode */
		reg_val &= (~BIT(16));
		break;
	case DRM_MODE_BLEND_PREMULTI:
		/* alpha mode select - combo alpha */
		reg_val |= BIT(3);
		/* blending mode select - pre-mult mode */
		reg_val |= BIT(16);
		break;
	default:
		/* alpha mode select - layer alpha */
		reg_val |= BIT(2);
		break;
	}

	reg_val |= y2r_coef << 28;
	rotation = to_dpu_rotation(rotation);
	reg_val |= (rotation & 0x7) << 20;

	return reg_val;
}

static void dpu_clean_all(struct dpu_context *ctx)
{
	int i;
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	dpu_sdp_disable(ctx);

	for (i = 0; i < 8; i++)
		reg->layers[i].ctrl = 0;
}

static void dpu_bgcolor(struct dpu_context *ctx, u32 color)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->bg_color = color;

	dpu_clean_all(ctx);

	if (!ctx->is_stopped) {
		reg->dpu_ctrl |= BIT(2);
		dpu_wait_update_done(ctx);
	}
}

static void dpu_layer(struct dpu_context *ctx,
		    struct sprd_dpu_layer *hwlayer)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	struct layer_reg *layer;
	u32 offset, wd;
	int i;

	layer = &reg->layers[hwlayer->index];
	offset = (hwlayer->dst_x & 0xffff) | ((hwlayer->dst_y) << 16);

	if (hwlayer->pallete_en) {
		layer->pos = offset;
		layer->size = (hwlayer->dst_w & 0xffff) | ((hwlayer->dst_h) << 16);
		layer->alpha = hwlayer->alpha;
		layer->pallete = hwlayer->pallete_color;

		/* pallete layer enable */
		layer->ctrl = 0x2005;

		pr_debug("dst_x = %d, dst_y = %d, dst_w = %d, dst_h = %d, pallete:%d\n",
			hwlayer->dst_x, hwlayer->dst_y,
			hwlayer->dst_w, hwlayer->dst_h, layer->pallete);
		return;
	}

	for (i = 0; i < hwlayer->planes; i++) {
		if (hwlayer->addr[i] % 16)
			pr_err("layer addr[%d] is not 16 bytes align, it's 0x%08x\n",
				i, hwlayer->addr[i]);
		layer->addr[i] = hwlayer->addr[i];
	}

	layer->pos = offset;
	layer->size = (hwlayer->src_w & 0xffff) | ((hwlayer->src_h) << 16);
	layer->crop_start = (hwlayer->src_y << 16) | hwlayer->src_x;
	layer->alpha = hwlayer->alpha;

	wd = drm_format_plane_cpp(hwlayer->format, 0);
	if (wd == 0) {
		pr_err("layer[%d] bytes per pixel is invalid\n",
			hwlayer->index);
		return;
	}

	if (hwlayer->planes == 3)
		/* UV pitch is 1/2 of Y pitch*/
		layer->pitch = (hwlayer->pitch[0] / wd) |
				(hwlayer->pitch[0] / wd << 15);
	else
		layer->pitch = hwlayer->pitch[0] / wd;

	layer->ctrl = dpu_img_ctrl(hwlayer->format, hwlayer->blending,
		hwlayer->xfbc, hwlayer->y2r_coef, hwlayer->rotation);

	pr_debug("dst_x = %d, dst_y = %d, dst_w = %d, dst_h = %d\n",
				hwlayer->dst_x, hwlayer->dst_y,
				hwlayer->dst_w, hwlayer->dst_h);
	pr_debug("start_x = %d, start_y = %d, start_w = %d, start_h = %d\n",
				hwlayer->src_x, hwlayer->src_y,
				hwlayer->src_w, hwlayer->src_h);
}

static void dpu_flip(struct dpu_context *ctx,
		     struct sprd_dpu_layer layers[], u8 count)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	int i;

	vsync_count = 0;
	if (max_vsync_count > 0 && count > 1)
		wb_en = true;

	/* reset the bgcolor to black */
	reg->bg_color = 0;

	/* disable all the layers */
	dpu_clean_all(ctx);

	/* bypass mode only use layer7 */
	if (ctx->bypass_mode) {
		if (count != 1)
			pr_err("bypass mode layer count error:%d", count);
		layers[0].index = 7;
		if (ctx->static_metadata_changed) {
			dpu_sdp_set(ctx, 9, ctx->hdr_static_metadata);
			ctx->static_metadata_changed = false;
		}
	}

	/* start configure dpu layers */
	for (i = 0; i < count; i++)
		dpu_layer(ctx, &layers[i]);

	/* update trigger and wait */
	if (!ctx->is_stopped) {
		reg->dpu_ctrl |= BIT(2);
		dpu_wait_update_done(ctx);
	}

	reg->dpu_int_en |= DISPC_INT_ERR_MASK;
	/*
	 * If the following interrupt was disabled in isr,
	 * re-enable it.
	 */
	reg->dpu_int_en |= DISPC_INT_FBC_PLD_ERR_MASK |
			   DISPC_INT_FBC_HDR_ERR_MASK;
}

static void dpu_dpi_init(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u32 int_mask = 0;

	/* use dpi as interface */
	reg->dpu_cfg0 &= ~BIT(0);

	/* set dpi timing */
	if (!(ctx->vm.flags & DISPLAY_FLAGS_HSYNC_LOW))
		reg->dpi_ctrl |= BIT(0);
	if (!(ctx->vm.flags & DISPLAY_FLAGS_VSYNC_LOW))
		reg->dpi_ctrl |= BIT(1);

	reg->dpi_h_timing = (ctx->vm.hsync_len << 0) |
			    (ctx->vm.hback_porch << 8) |
			    (ctx->vm.hfront_porch << 20);
	reg->dpi_v_timing = (ctx->vm.vsync_len << 0) |
			    (ctx->vm.vback_porch << 8) |
			    (ctx->vm.vfront_porch << 20);
	if (ctx->vm.vsync_len + ctx->vm.vback_porch < 32)
		pr_warn("Warning: (vsync + vbp) < 32, "
			"underflow risk!\n");

	if (ctx->bypass_mode)
		reg->dpu_mode = DPU_BYPASS_MODE;
	else
		reg->dpu_mode = DPU_NORMAL_MODE;

	/* enable dpu update done INT */
	int_mask |= DISPC_INT_UPDATE_DONE_MASK;
	/* enable dpu DONE  INT */
	int_mask |= DISPC_INT_DONE_MASK;
	/* enable dpu dpi vsync */
	int_mask |= DISPC_INT_DPI_VSYNC_MASK;
	/* enable underflow err INT */
	int_mask |= DISPC_INT_ERR_MASK;
	/* enable write back done INT */
	int_mask |= DISPC_INT_WB_DONE_MASK;
	/* enable write back fail INT */
	int_mask |= DISPC_INT_WB_FAIL_MASK;


	/* enable ifbc payload error INT */
	int_mask |= DISPC_INT_FBC_PLD_ERR_MASK;
	/* enable ifbc header error INT */
	int_mask |= DISPC_INT_FBC_HDR_ERR_MASK;

	reg->dpu_int_en = int_mask;
}

static void enable_vsync(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->dpu_int_en |= DISPC_INT_DPI_VSYNC_MASK;
}

static void disable_vsync(struct dpu_context *ctx)
{
	//struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	//reg->dpu_int_en &= ~DISPC_INT_DPI_VSYNC_MASK;
}

static int dpu_modeset(struct dpu_context *ctx,
		struct drm_display_mode *mode)
{
	drm_display_mode_to_videomode(mode, &ctx->vm);

	/* only 3840x2160 use bypass mode */
	if (ctx->vm.hactive == 3840 &&
		ctx->vm.vactive == 2160) {
		ctx->bypass_mode = true;
		pr_info("bypass_mode\n");
	} else {
		ctx->bypass_mode = false;
		pr_info("normal mode\n");
	}

	pr_info("switch to %u x %u\n", mode->hdisplay, mode->vdisplay);

	return 0;
}

static const u32 primary_fmts[] = {
	DRM_FORMAT_XRGB8888, DRM_FORMAT_XBGR8888,
	DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888,
	DRM_FORMAT_RGBA8888, DRM_FORMAT_BGRA8888,
	DRM_FORMAT_RGBX8888, DRM_FORMAT_BGRX8888,
	DRM_FORMAT_RGB565, DRM_FORMAT_BGR565,
	DRM_FORMAT_NV12, DRM_FORMAT_NV21,
	DRM_FORMAT_NV16, DRM_FORMAT_NV61,
	DRM_FORMAT_YUV420, DRM_FORMAT_P010,
};

static int dpu_capability(struct dpu_context *ctx,
			struct dpu_capability *cap)
{
	if (!cap)
		return -EINVAL;

	cap->max_layers = 4;
	cap->fmts_ptr = primary_fmts;
	cap->fmts_cnt = ARRAY_SIZE(primary_fmts);

	return 0;
}

static struct dpu_core_ops dpu_lite_r3p0_ops = {
	.parse_dt = dpu_parse_dt,
	.version = dpu_get_version,
	.init = dpu_init,
	.uninit = dpu_uninit,
	.run = dpu_run,
	.stop = dpu_stop,
	.isr = dpu_isr,
	.ifconfig = dpu_dpi_init,
	.capability = dpu_capability,
	.flip = dpu_flip,
	.bg_color = dpu_bgcolor,
	.enable_vsync = enable_vsync,
	.disable_vsync = disable_vsync,
	.modeset = dpu_modeset,
	.write_back = NULL,
};

static struct ops_entry entry = {
	.ver = "dpu-lite-r3p0",
	.ops = &dpu_lite_r3p0_ops,
};

static int __init dpu_core_register(void)
{
	return dpu_core_ops_register(&entry);
}

subsys_initcall(dpu_core_register);
