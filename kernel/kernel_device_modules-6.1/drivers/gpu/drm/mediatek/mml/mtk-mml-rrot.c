// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Dennis YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#include <linux/component.h>
#include <linux/dma-mapping.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/math64.h>
#include <soc/mediatek/smi.h>
#include <mtk-smmu-v3.h>

#include "mtk-mml-driver.h"
#include "mtk-mml-tile.h"
#include "mtk-mml-sys.h"
#include "mtk-mml-mmp.h"
#include "mtk-mml-dle-adaptor.h"
#include "mtk-mml-rrot-golden.h"
#include "tile_driver.h"
#include "tile_mdp_func.h"

#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#endif

/* RROT register offset */
#define RROT_EN				0x000
#define RROT_RESET			0x008
#define RROT_CON			0x020
#define RROT_SHADOW_CTRL		0x024
#define RROT_GMCIF_CON			0x028
#define RROT_SRC_CON			0x030
#define RROT_COMP_CON			0x038
#define RROT_MF_BKGD_SIZE_IN_BYTE	0x060
#define RROT_MF_BKGD_SIZE_IN_PXL	0x068
#define RROT_MF_SRC_SIZE		0x070
#define RROT_MF_CLIP_SIZE		0x078
#define RROT_MF_OFFSET_1		0x080
#define RROT_SF_BKGD_SIZE_IN_BYTE	0x090
#define RROT_MF_BKGD_H_SIZE_IN_PXL	0x098
#define RROT_AUTO_SLICE_0		0x0a0
#define RROT_AUTO_SLICE_1		0x0a8
#define RROT_BINNING			0x0b8
#define RROT_PREFETCH_CONTROL_0		0x0c8
#define RROT_PREFETCH_CONTROL_1		0x0d0
#define RROT_PREFETCH_CONTROL_2		0x0d8
#define RROT_SRC_BASE_ADD_0		0x100
#define RROT_SRC_BASE_ADD_1		0x108
#define RROT_SRC_BASE_ADD_2		0x110
#define RROT_SRC_OFFSET_0		0x118
#define RROT_SRC_OFFSET_1		0x120
#define RROT_SRC_OFFSET_2		0x128
#define RROT_SRC_OFFSET_WP		0x148
#define RROT_SRC_OFFSET_HP		0x150
#define RROT_SRC_BASE_ADD_0_MSB		0x160
#define RROT_SRC_BASE_ADD_1_MSB		0x168
#define RROT_SRC_BASE_ADD_2_MSB		0x170
#define RROT_STASH_URGENT_TH_CON_0	0x1a0
#define RROT_STASH_ULTRA_TH_CON_0	0x1a4
#define RROT_STASH_PREULTRA_TH_CON_0	0x1a8
#define RROT_STASH_URGENT_TH_CON_1	0x1ac
#define RROT_STASH_ULTRA_TH_CON_1	0x1b0
#define RROT_STASH_PREULTRA_TH_CON_1	0x1b4
#define RROT_STASH_URGENT_TH_CON_2	0x1b8
#define RROT_STASH_ULTRA_TH_CON_2	0x1bc
#define RROT_STASH_PREULTRA_TH_CON_2	0x1c0
#define RROT_STASH_URGENT_TH_CON_3	0x1c4
#define RROT_STASH_ULTRA_TH_CON_3	0x1c8
#define RROT_STASH_PREULTRA_TH_CON_3	0x1cc
#define RROT_TRANSFORM_0		0x200

#define RROT_DMABUF_CON_0		0x240
#define RROT_URGENT_TH_CON_0		0x244
#define RROT_ULTRA_TH_CON_0		0x248
#define RROT_PREULTRA_TH_CON_0		0x250
#define RROT_DMABUF_CON_1		0x254
#define RROT_URGENT_TH_CON_1		0x258
#define RROT_ULTRA_TH_CON_1		0x260
#define RROT_PREULTRA_TH_CON_1		0x264
#define RROT_DMABUF_CON_2		0x268
#define RROT_URGENT_TH_CON_2		0x270
#define RROT_ULTRA_TH_CON_2		0x274
#define RROT_PREULTRA_TH_CON_2		0x278
#define RROT_DMABUF_CON_3		0x280
#define RROT_URGENT_TH_CON_3		0x284
#define RROT_ULTRA_TH_CON_3		0x288
#define RROT_PREULTRA_TH_CON_3		0x290

#define RROT_DITHER_CON			0x2a0
#define RROT_CHKS_EXTR			0x300
#define RROT_DEBUG_CON			0x380
#define RROT_MON_STA_0			0x400
#define RROT_SRC_BASE_0			0xf00
#define RROT_SRC_BASE_1			0xf08
#define RROT_SRC_BASE_2			0xf10
#define RROT_UFO_DEC_LENGTH_BASE_Y	0xf20
#define RROT_UFO_DEC_LENGTH_BASE_C	0xf28
#define RROT_SRC_BASE_0_MSB		0xf30
#define RROT_SRC_BASE_1_MSB		0xf34
#define RROT_SRC_BASE_2_MSB		0xf38
#define RROT_UFO_DEC_LENGTH_BASE_Y_MSB	0xf3c
#define RROT_UFO_DEC_LENGTH_BASE_C_MSB	0xf40
#define RROT_SRC_OFFSET_0_MSB		0xf44
#define RROT_SRC_OFFSET_1_MSB		0xf48
#define RROT_SRC_OFFSET_2_MSB		0xf4c
#define RROT_AFBC_PAYLOAD_OST		0xf50

/* RROT debug monitor register count */
#define RROT_MON_COUNT 38
#define RROT_HYFBC_MON_COUNT 4
static u32 hyfbc_setting[RROT_HYFBC_MON_COUNT] = {0x6000, 0x18000, 0x3c000, 0x3e000};

/* SMI offset */
#define SMI_LARB_NON_SEC_CON		0x380

int mml_rrot_msg;
module_param(mml_rrot_msg, int, 0644);
/* HyFBC decode done checksum
 * 0x10 Y
 * 0x11 C
 */
int rrot_dbg_con = 0x10;
module_param(rrot_dbg_con, int, 0644);

#define rrot_msg(fmt, args...) \
do { \
	if (mml_rrot_msg) \
		_mml_log("[rrot]" fmt, ##args); \
	else \
		mml_msg("[rrot]" fmt, ##args); \
} while (0)

int rrot_binning = 1;
module_param(rrot_binning, int, 0644);

/* Debug parameter to force config stash cmd RROT_PREFETCH_CONTROL_1 and RROT_PREFETCH_CONTROL_2
 * Set 0 for disable, any value to enable.
 * con1[31:16]	PREFETCH_LINE_CNT_0
 * con1[15:00]	PREFETCH_LINE_CNT_1
 * con2[31:16]	PREFETCH_LINE_CNT_2
 * con2[15:00]	PREFETCH_LINE_CNT_3
 *
 */
int rrot_stash_con1;
module_param(rrot_stash_con1, int, 0644);
int rrot_stash_con2;
module_param(rrot_stash_con2, int, 0644);

enum rrot_label {
	RROT_LABEL_BASE_0 = 0,
	RROT_LABEL_BASE_0_MSB,
	RROT_LABEL_BASE_1,
	RROT_LABEL_BASE_1_MSB,
	RROT_LABEL_BASE_2,
	RROT_LABEL_BASE_2_MSB,
	RROT_LABEL_UFO_DEC_BASE_C,
	RROT_LABEL_UFO_DEC_BASE_C_MSB,
	RROT_LABEL_UFO_DEC_BASE_Y,
	RROT_LABEL_UFO_DEC_BASE_Y_MSB,
	RROT_LABEL_TOTAL
};

enum rrot_golden_fmt {
	GOLDEN_FMT_ARGB,
	GOLDEN_FMT_RGB,
	GOLDEN_FMT_YUV420,
	GOLDEN_FMT_YV12,
	GOLDEN_FMT_HYFBC,
	GOLDEN_FMT_AFBC,
	GOLDEN_FMT_TOTAL
};

static const u16 rrot_dmabuf[] = {
	RROT_DMABUF_CON_0,
	RROT_DMABUF_CON_1,
	RROT_DMABUF_CON_2,
	RROT_DMABUF_CON_3,
};

static const u16 rrot_urgent_th[] = {
	RROT_URGENT_TH_CON_0,
	RROT_URGENT_TH_CON_1,
	RROT_URGENT_TH_CON_2,
	RROT_URGENT_TH_CON_3,
};

static const u16 rrot_ultra_th[] = {
	RROT_ULTRA_TH_CON_0,
	RROT_ULTRA_TH_CON_1,
	RROT_ULTRA_TH_CON_2,
	RROT_ULTRA_TH_CON_3,
};

static const u16 rrot_preultra_th[] = {
	RROT_PREULTRA_TH_CON_0,
	RROT_PREULTRA_TH_CON_1,
	RROT_PREULTRA_TH_CON_2,
	RROT_PREULTRA_TH_CON_3,
};

struct rrot_data {
	u32 tile_width;
	bool alpha_pq_r2y;

	/* threshold golden setting for racing mode */
	struct rrot_golden golden[GOLDEN_FMT_TOTAL];
};

static const struct rrot_data mt6989_rrot_data = {
	.tile_width = 4096,
	.alpha_pq_r2y = true,
	.golden = {
		[GOLDEN_FMT_ARGB] = {
			.cnt = ARRAY_SIZE(th_argb_mt6989),
			.settings = th_argb_mt6989,
		},
		[GOLDEN_FMT_RGB] = {
			.cnt = ARRAY_SIZE(th_rgb_mt6989),
			.settings = th_rgb_mt6989,
		},
		[GOLDEN_FMT_YUV420] = {
			.cnt = ARRAY_SIZE(th_yuv420_mt6989),
			.settings = th_yuv420_mt6989,
		},
		[GOLDEN_FMT_YV12] = {
			.cnt = ARRAY_SIZE(th_yv12_mt6989),
			.settings = th_yv12_mt6989,
		},
		[GOLDEN_FMT_HYFBC] = {
			.cnt = ARRAY_SIZE(th_hyfbc_mt6989),
			.settings = th_hyfbc_mt6989,
		},
		[GOLDEN_FMT_AFBC] = {
			.cnt = ARRAY_SIZE(th_afbc_mt6989),
			.settings = th_afbc_mt6989,
		},
	},
};

struct mml_comp_rrot {
	struct mml_comp comp;
	const struct rrot_data *data;
	struct device *mmu_dev;	/* for dmabuf to iova */
	struct device *mmu_dev_sec; /* for secure dmabuf to secure iova */

	u16 event_eof;

	/* smi register to config sram/dram mode */
	phys_addr_t smi_larb_con;

	u8 pipe;	/* separate rrot and rrot_2nd */
};

struct rrot_offset {
	u64 y;
	u64 c;
	u64 v;
};

/* meta data for each different frame config */
struct rrot_frame_data {
	/* frame separate by rrot pipe (rrot/rrot_2nd) */
	struct mml_rect crop;

	/* tile config */
	u8 enable_ufo;
	u8 hw_fmt;
	u8 swap;
	u8 blk;
	u8 lb_2b_mode;
	u8 field;
	u8 blk_10bit;
	u8 blk_tile;
	u8 color_tran;
	u8 matrix_sel;
	u32 bits_per_pixel_y;
	u32 bits_per_pixel_uv;
	u32 hor_shift_uv;
	u32 ver_shift_uv;
	u32 vdo_blk_shift_w;
	u32 vdo_blk_height;
	u32 vdo_blk_shift_h;
	u32 mf_src_w;
	u32 mf_src_h;
	u32 datasize;		/* qos data size in bytes */
	u16 crop_off_l;		/* crop offset left */
	u16 crop_off_t;		/* crop offset top */
	u32 gmcif_con;
	u32 slice_pixel;
	bool ultra_off;
	bool binning;

	/* array of indices to one of entry in cache entry list,
	 * use in reuse command
	 */
	u16 labels[RROT_LABEL_TOTAL];
	u32 crc_inst_offset;
};

static s32 rrot_write_addr(struct cmdq_pkt *pkt,
			   dma_addr_t addr, dma_addr_t addr_high, u64 value,
			   struct mml_task_reuse *reuse,
			   struct mml_pipe_cache *cache,
			   u16 *label_idx)
{
	s32 ret;

	ret = mml_write(pkt, addr, value & GENMASK_ULL(31, 0), U32_MAX,
			reuse, cache, label_idx);
	if (ret)
		return ret;

	ret = mml_write(pkt, addr_high, value >> 32, U32_MAX,
			reuse, cache, label_idx + 1);
	return ret;
}

static void rrot_update_addr(struct mml_task_reuse *reuse,
			     u16 label, u16 label_high, u64 value)
{
	mml_update(reuse, label, value & GENMASK_ULL(31, 0));
	mml_update(reuse, label_high, value >> 32);
}

static s32 rrot_write64(struct cmdq_pkt *pkt, phys_addr_t pa, phys_addr_t pa_msb, u64 value)
{
	int ret = cmdq_pkt_write(pkt, NULL, pa, value & GENMASK_ULL(31, 0), U32_MAX);

	if (ret)
		return ret;
	return cmdq_pkt_write(pkt, NULL, pa_msb, value >> 32, U32_MAX);
}

static inline struct rrot_frame_data *rrot_frm_data(struct mml_comp_config *ccfg)
{
	return ccfg->data;
}

static inline struct mml_comp_rrot *comp_to_rrot(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_rrot, comp);
}

static u32 rrot_get_latency(struct mml_frame_config *cfg)
{
	const enum mml_color fmt = cfg->info.src.format;
	const u32 rot = cfg->info.dest[0].rotate;
	u32 line;

	if (MML_FMT_AFBC_ARGB(fmt)) {
		/* rotate 0 and 180, include flip */
		line = (rot == MML_ROT_0 || rot == MML_ROT_180) ? 4 : 32;
	} else if (MML_FMT_AFBC_YUV(fmt) || MML_FMT_HYFBC(fmt)) {
		line = 16;
	} else if (MML_FMT_UFO(fmt) || MML_FMT_BLOCK(fmt)) {
		line = (rot == MML_ROT_0 || rot == MML_ROT_180) ? 8 : 16;
	} else {
		/* other unexpect format, use max latency */
		line = 32;
	}

	return line;
}

static void calc_binning_crop(u32 *crop, u32 *frac)
{
	*frac = (*frac + (*crop & 0x1 ? (1 << MML_SUBPIXEL_BITS) : 0)) >> 1;
	*crop = *crop >> 1;
}

#define calc_tile_subpx(_start_sub, sz, sz_sub) \
	((_start_sub + (sz << MML_SUBPIXEL_BITS) + \
		sz_sub + (1 << MML_SUBPIXEL_BITS) - 1) >> MML_SUBPIXEL_BITS)

static void calc_binning_rot(struct mml_frame_config *cfg, struct mml_comp_config *ccfg)
{
	const struct mml_frame_data *src = &cfg->info.src;
	const struct mml_frame_dest *dest = &cfg->info.dest[0];
	u32 w = dest->crop.r.width, h = dest->crop.r.height, i;
	u32 outw = dest->data.width, outh = dest->data.height;
	struct mml_crop *crop;
	bool binning = rrot_binning && MML_FMT_YUV420(src->format);

	if (dest->rotate == MML_ROT_90 || dest->rotate == MML_ROT_270)
		swap(outw, outh);

	if (binning && (w >> 1) >= outw) {
		cfg->frame_in.width = (src->width + 1) >> 1;
		cfg->bin_x = 1;
		for (i = 0; i < cfg->info.dest_cnt; i++) {
			crop = &cfg->frame_in_crop[i];
			calc_binning_crop(&crop->r.width, &crop->w_sub_px);
			calc_binning_crop(&crop->r.left, &crop->x_sub_px);
		}
	}
	if (binning && (h >> 1) >= outh) {
		cfg->frame_in.height = (src->height + 1) >> 1;
		cfg->bin_y = 1;
		for (i = 0; i < cfg->info.dest_cnt; i++) {
			crop = &cfg->frame_in_crop[i];
			calc_binning_crop(&crop->r.height, &crop->h_sub_px);
			calc_binning_crop(&crop->r.top, &crop->y_sub_px);

		}
	}

	if ((cfg->info.dest_cnt == 1 ||
	     !memcmp(&cfg->info.dest[0].crop, &cfg->info.dest[1].crop,
		     sizeof(struct mml_crop))) &&
	    (dest->crop.r.width != src->width || dest->crop.r.height != src->height)) {
		crop = &cfg->frame_in_crop[0];
		/* calculate tile full size from rrot out to rsz in, with roundup sub pixel */
		cfg->frame_tile_sz.width =
			calc_tile_subpx(crop->x_sub_px, crop->r.width, crop->w_sub_px);
		cfg->frame_tile_sz.height =
			calc_tile_subpx(crop->y_sub_px, crop->r.height, crop->h_sub_px);
	} else {
		cfg->frame_tile_sz.width = cfg->frame_in.width;
		cfg->frame_tile_sz.height = cfg->frame_in.height;
	}

	if (dest->rotate == MML_ROT_90 || dest->rotate == MML_ROT_270) {
		swap(cfg->frame_in.width, cfg->frame_in.height);
		swap(cfg->frame_tile_sz.width, cfg->frame_tile_sz.height);
		for (i = 0; i < cfg->info.dest_cnt; i++) {
			crop = &cfg->frame_in_crop[i];
			swap(crop->r.left, crop->r.top);
			swap(crop->r.width, crop->r.height);
			swap(crop->x_sub_px, crop->y_sub_px);
			swap(crop->w_sub_px, crop->h_sub_px);
			swap(cfg->frame_out[i].width, cfg->frame_out[i].height);
			cfg->out_rotate[i] = 0;
			cfg->out_flip[i] = false;
		}
	} else {
		/* rotate 0 or 180, clear both rotation and flip value but keep others */
		for (i = 0; i < cfg->info.dest_cnt; i++) {
			cfg->out_rotate[i] = 0;
			cfg->out_flip[i] = false;
		}
	}

	if (cfg->bin_x || cfg->bin_y) {
		for (i = 0; i < cfg->info.dest_cnt; i++) {
			crop = &cfg->frame_in_crop[i];
			rrot_msg("crop%u %u.%u %u.%u %u.%u %u.%u",
				i, crop->r.left, crop->x_sub_px, crop->r.top, crop->y_sub_px,
				crop->r.width, crop->w_sub_px, crop->r.height, crop->h_sub_px);
		}
		rrot_msg("frame tile sz %u %u",
			cfg->frame_tile_sz.width, cfg->frame_tile_sz.height);
	}
}

static s32 rrot_prepare(struct mml_comp *comp, struct mml_task *task,
			struct mml_comp_config *ccfg)
{
	struct mml_comp_rrot *rrot = comp_to_rrot(comp);
	struct mml_frame_config *cfg = task->config;
	struct rrot_frame_data *rrot_frm;

	ccfg->data = kzalloc(sizeof(struct rrot_frame_data), GFP_KERNEL);
	if (!ccfg->data)
		return -ENOMEM;
	rrot_frm = rrot_frm_data(ccfg);

	/* calculate binning size and set to frame config */
	if (rrot->pipe == 0)
		calc_binning_rot(task->config, ccfg);
	if (cfg->bin_x || cfg->bin_y) {
		rrot_frm->binning = true;
		rrot_msg("%s rrot%s bin %u %u",
			__func__, rrot->pipe ? "_2nd" : "    ", cfg->bin_x, cfg->bin_y);
	}

	return 0;
}

static s32 rrot_buf_map(struct mml_comp *comp, struct mml_task *task,
			const struct mml_path_node *node)
{
	struct mml_comp_rrot *rrot = comp_to_rrot(comp);
	struct mml_frame_config *cfg = task->config;
	s32 ret = 0;

	mml_trace_ex_begin("%s_rrot%s", __func__, rrot->pipe ? "_2nd" : "");

	/* check iova, so rrot get iova first and rrot_2nd use same value */
	if (!task->buf.src.dma[0].iova) {
		mml_mmp(buf_map, MMPROFILE_FLAG_START,
			((u64)task->job.jobid << 16) | comp->id, 0);

		/* get iova */
		ret = mml_buf_iova_get(cfg->info.src.secure ? rrot->mmu_dev_sec : rrot->mmu_dev,
			&task->buf.src);
		if (ret < 0)
			mml_err("%s iova fail %d", __func__, ret);

		mml_mmp(buf_map, MMPROFILE_FLAG_END,
			((u64)task->job.jobid << 16) | comp->id,
			(unsigned long)task->buf.src.dma[0].iova);

		mml_msg("%s comp %u dma %p iova %#11llx (%u) %#11llx (%u) %#11llx (%u)",
			__func__, comp->id, task->buf.src.dma[0].dmabuf,
			task->buf.src.dma[0].iova,
			task->buf.src.size[0],
			task->buf.src.dma[1].iova,
			task->buf.src.size[1],
			task->buf.src.dma[2].iova,
			task->buf.src.size[2]);
	}

	mml_trace_ex_end();

	return 0;
}

s32 rrot_tile_prepare(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg,
	struct tile_func_block *func,
	union mml_tile_data *data)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_data *src = &cfg->info.src;
	struct mml_comp_rrot *rrot = comp_to_rrot(comp);
	struct rrot_frame_data *rrot_frm = rrot_frm_data(ccfg);

	mml_msg("%s rrot %u pipe %u", __func__, comp->id, rrot->pipe);

	data->rdma.src_fmt = src->format;
	data->rdma.blk_shift_w = MML_FMT_BLOCK(src->format) ? 4 : 0;
	data->rdma.blk_shift_h = MML_FMT_BLOCK(src->format) ? 5 : 0;
	data->rdma.max_width = rrot->data->tile_width;

	/* RDMA support crop capability */
	func->type = TILE_TYPE_RDMA | TILE_TYPE_CROP_EN;
	func->init_func = tile_rrot_init;
	func->for_func = tile_rdma_for;
	func->back_func = tile_rdma_back;
	func->data = data;
	data->rdma.read_rotate = cfg->info.dest[0].rotate;
	func->enable_flag = true;

	func->full_size_x_in = cfg->frame_in.width;
	func->full_size_y_in = cfg->frame_in.height;
	func->full_size_x_out = cfg->frame_tile_sz.width;
	func->full_size_y_out = cfg->frame_tile_sz.height;

	if (cfg->info.dest_cnt == 1 ||
	    !memcmp(&cfg->info.dest[0].crop, &cfg->info.dest[1].crop,
		    sizeof(struct mml_crop))) {
		data->rdma.crop.left = cfg->frame_in_crop[0].r.left;
		data->rdma.crop.top = cfg->frame_in_crop[0].r.top;
		data->rdma.crop.width = cfg->frame_tile_sz.width;
		data->rdma.crop.height = cfg->frame_tile_sz.height;

		rrot_frm->crop_off_l = data->rdma.crop.left;
		rrot_frm->crop_off_t = data->rdma.crop.top;
	} else {
		data->rdma.crop.left = 0;
		data->rdma.crop.top = 0;
		data->rdma.crop.width = cfg->frame_in.width;
		data->rdma.crop.height = cfg->frame_in.height;
	}

	rrot_msg("%s size in %u %u out %u %u crop off %u %u",
		__func__,
		func->full_size_x_in, func->full_size_y_in,
		func->full_size_x_out, func->full_size_y_out,
		rrot_frm->crop_off_l, rrot_frm->crop_off_t);
	return 0;
}

static const struct mml_comp_tile_ops rrot_tile_ops = {
	.prepare = rrot_tile_prepare,
};

static u32 rrot_get_label_count(struct mml_comp *comp, struct mml_task *task,
				struct mml_comp_config *ccfg)
{
	return RROT_LABEL_TOTAL;
}

static void rrot_color_fmt(struct mml_frame_config *cfg,
			   struct rrot_frame_data *rrot_frm)
{
	u32 fmt = cfg->info.src.format;
	u16 profile_in = cfg->info.src.profile;

	rrot_frm->color_tran = 0;
	rrot_frm->matrix_sel = 15;

	rrot_frm->enable_ufo = MML_FMT_UFO(fmt);
	rrot_frm->hw_fmt = MML_FMT_HW_FORMAT(fmt);
	rrot_frm->swap = MML_FMT_SWAP(fmt);
	rrot_frm->blk = MML_FMT_BLOCK(fmt);
	rrot_frm->lb_2b_mode = rrot_frm->blk ? 0 : 1;
	rrot_frm->field = MML_FMT_INTERLACED(fmt);
	rrot_frm->blk_10bit = MML_FMT_10BIT_PACKED(fmt);
	rrot_frm->blk_tile = MML_FMT_10BIT_TILE(fmt);

	switch (fmt) {
	case MML_FMT_GREY:
		rrot_frm->bits_per_pixel_y = 8;
		rrot_frm->bits_per_pixel_uv = 0;
		rrot_frm->hor_shift_uv = 0;
		rrot_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_RGB565:
	case MML_FMT_BGR565:
		rrot_frm->bits_per_pixel_y = 16;
		rrot_frm->bits_per_pixel_uv = 0;
		rrot_frm->hor_shift_uv = 0;
		rrot_frm->ver_shift_uv = 0;
		rrot_frm->color_tran = 1;
		break;
	case MML_FMT_RGB888:
	case MML_FMT_BGR888:
		rrot_frm->bits_per_pixel_y = 24;
		rrot_frm->bits_per_pixel_uv = 0;
		rrot_frm->hor_shift_uv = 0;
		rrot_frm->ver_shift_uv = 0;
		rrot_frm->color_tran = 1;
		break;
	case MML_FMT_RGBA8888:
	case MML_FMT_BGRA8888:
	case MML_FMT_ARGB8888:
	case MML_FMT_ABGR8888:
	case MML_FMT_RGBA1010102:
	case MML_FMT_BGRA1010102:
	case MML_FMT_RGBA8888_AFBC:
	case MML_FMT_RGBA1010102_AFBC:
		rrot_frm->bits_per_pixel_y = 32;
		rrot_frm->bits_per_pixel_uv = 0;
		rrot_frm->hor_shift_uv = 0;
		rrot_frm->ver_shift_uv = 0;
		rrot_frm->color_tran = 1;
		break;
	case MML_FMT_UYVY:
	case MML_FMT_VYUY:
	case MML_FMT_YUYV:
	case MML_FMT_YVYU:
		rrot_frm->bits_per_pixel_y = 16;
		rrot_frm->bits_per_pixel_uv = 0;
		rrot_frm->hor_shift_uv = 0;
		rrot_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_I420:
	case MML_FMT_YV12:
		rrot_frm->bits_per_pixel_y = 8;
		rrot_frm->bits_per_pixel_uv = 8;
		rrot_frm->hor_shift_uv = 1;
		rrot_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_I422:
	case MML_FMT_YV16:
		rrot_frm->bits_per_pixel_y = 8;
		rrot_frm->bits_per_pixel_uv = 8;
		rrot_frm->hor_shift_uv = 1;
		rrot_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_I444:
	case MML_FMT_YV24:
		rrot_frm->bits_per_pixel_y = 8;
		rrot_frm->bits_per_pixel_uv = 8;
		rrot_frm->hor_shift_uv = 0;
		rrot_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_NV12:
	case MML_FMT_NV21:
		rrot_frm->bits_per_pixel_y = 8;
		rrot_frm->bits_per_pixel_uv = 16;
		rrot_frm->hor_shift_uv = 1;
		rrot_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_YUV420_AFBC:
	case MML_FMT_NV12_HYFBC:
		rrot_frm->bits_per_pixel_y = 12;
		rrot_frm->bits_per_pixel_uv = 0;
		rrot_frm->hor_shift_uv = 1;
		rrot_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_BLK_UFO:
	case MML_FMT_BLK_UFO_AUO:
	case MML_FMT_BLK:
		rrot_frm->vdo_blk_shift_w = 4;
		rrot_frm->vdo_blk_height = 32;
		rrot_frm->vdo_blk_shift_h = 5;
		rrot_frm->bits_per_pixel_y = 8;
		rrot_frm->bits_per_pixel_uv = 16;
		rrot_frm->hor_shift_uv = 1;
		rrot_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_NV16:
	case MML_FMT_NV61:
		rrot_frm->bits_per_pixel_y = 8;
		rrot_frm->bits_per_pixel_uv = 16;
		rrot_frm->hor_shift_uv = 1;
		rrot_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_NV24:
	case MML_FMT_NV42:
		rrot_frm->bits_per_pixel_y = 8;
		rrot_frm->bits_per_pixel_uv = 16;
		rrot_frm->hor_shift_uv = 0;
		rrot_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_NV12_10L:
	case MML_FMT_NV21_10L:
		rrot_frm->bits_per_pixel_y = 16;
		rrot_frm->bits_per_pixel_uv = 32;
		rrot_frm->hor_shift_uv = 1;
		rrot_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_YUVA8888:
	case MML_FMT_AYUV8888:
	case MML_FMT_YUVA1010102:
	case MML_FMT_UYV1010102:
		rrot_frm->bits_per_pixel_y = 32;
		rrot_frm->bits_per_pixel_uv = 0;
		rrot_frm->hor_shift_uv = 0;
		rrot_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_NV15:
	case MML_FMT_NV51:
		rrot_frm->bits_per_pixel_y = 10;
		rrot_frm->bits_per_pixel_uv = 20;
		rrot_frm->hor_shift_uv = 1;
		rrot_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_YUV420_10P_AFBC:
	case MML_FMT_P010_HYFBC:
		rrot_frm->bits_per_pixel_y = 16;
		rrot_frm->bits_per_pixel_uv = 0;
		rrot_frm->hor_shift_uv = 1;
		rrot_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_BLK_10H:
	case MML_FMT_BLK_10V:
	case MML_FMT_BLK_10HJ:
	case MML_FMT_BLK_10VJ:
	case MML_FMT_BLK_UFO_10H:
	case MML_FMT_BLK_UFO_10V:
	case MML_FMT_BLK_UFO_10HJ:
	case MML_FMT_BLK_UFO_10VJ:
		rrot_frm->vdo_blk_shift_w = 4;
		rrot_frm->vdo_blk_height = 32;
		rrot_frm->vdo_blk_shift_h = 5;
		rrot_frm->bits_per_pixel_y = 10;
		rrot_frm->bits_per_pixel_uv = 20;
		rrot_frm->hor_shift_uv = 1;
		rrot_frm->ver_shift_uv = 1;
		break;
	default:
		mml_err("[rrot] not support format %x", fmt);
		break;
	}

	if (profile_in == MML_YCBCR_PROFILE_BT2020 ||
	    profile_in == MML_YCBCR_PROFILE_FULL_BT709 ||
	    profile_in == MML_YCBCR_PROFILE_FULL_BT2020)
		profile_in = MML_YCBCR_PROFILE_BT709;

	if (rrot_frm->color_tran) {
		if (profile_in == MML_YCBCR_PROFILE_BT601)
			rrot_frm->matrix_sel = 2;
		else if (profile_in == MML_YCBCR_PROFILE_BT709)
			rrot_frm->matrix_sel = 3;
		else if (profile_in == MML_YCBCR_PROFILE_FULL_BT601)
			rrot_frm->matrix_sel = 0;
		else
			mml_err("[rrot] unknown color conversion %x",
				profile_in);
	}
}

static void calc_hyfbc(struct mml_file_buf *src_buf, struct mml_frame_data *src,
		       u64 *y_header_addr, u64 *y_data_addr,
		       u64 *c_header_addr, u64 *c_data_addr)
{
	u64 buf_addr = src_buf->dma[0].iova;
	u32 width = ((src->width + 63) >> 6) << 6;
	u32 height = ((src->height + 63) >> 6) << 6;
	u32 y_data_sz = width * height;
	u32 c_data_sz;
	u32 y_header_sz;
	u32 c_header_sz;
	u32 total_sz;

	if (MML_FMT_10BIT(src->format))
		y_data_sz = y_data_sz * 6 >> 2;

	c_data_sz = y_data_sz >> 1;
	y_header_sz = (width * height + 63) >> 6;
	c_header_sz = ((width * height >> 1) + 63) >> 6;

	*y_data_addr = (((buf_addr + y_header_sz + 4095) >> 12) << 12);
	*y_header_addr = *y_data_addr - y_header_sz;	/* should be 64 aligned */
	*c_data_addr = ((*y_data_addr + y_data_sz + c_header_sz + 4095) >> 12) << 12;
	*c_header_addr = ((*c_data_addr - c_header_sz) >> 6) << 6;

	total_sz = (u32)(*c_data_addr + c_data_sz - buf_addr);
	if (src_buf->size[0] != total_sz)
		mml_log("[rrot]warn %s hyfbc buf size %u calc size %u",
			__func__, src_buf->size[0], total_sz);
}

static void calc_ufo(struct mml_file_buf *src_buf, struct mml_frame_data *src,
		     u64 *ufo_dec_length_y, u64 *ufo_dec_length_c,
		     u32 *u4pic_size_bs, u32 *u4pic_size_y_bs)
{
	u32 u4pic_size_y = src->width * src->height;
	u32 u4ufo_len_size_y =
		((((u4pic_size_y + 255) >> 8) + 63 + (16*8)) >> 6) << 6;
	u32 u4pic_size_c_bs;

	if (MML_FMT_10BIT_PACKED(src->format)) {
		if (MML_FMT_10BIT_JUMP(src->format)) {
			*u4pic_size_y_bs =
				(((u4pic_size_y * 5 >> 2) + 511) >> 9) << 9;
			*u4pic_size_bs =
				((*u4pic_size_y_bs + u4ufo_len_size_y + 4095) >>
				12) << 12;
			u4pic_size_c_bs =
				(((u4pic_size_y * 5 >> 3) + 63) >> 6) << 6;
		} else {
			*u4pic_size_y_bs =
				(((u4pic_size_y * 5 >> 2) + 4095) >> 12) << 12;
			u4pic_size_c_bs = u4pic_size_y * 5 >> 3;
			*u4pic_size_bs =
				((*u4pic_size_y_bs + u4pic_size_c_bs + 511) >>
				9) << 9;
		}
	} else {
		if (MML_FMT_AUO(src->format)) {
			u4ufo_len_size_y = u4ufo_len_size_y << 1;
			*u4pic_size_y_bs = ((u4pic_size_y + 511) >> 9) << 9;
			*u4pic_size_bs =
				((*u4pic_size_y_bs + u4ufo_len_size_y + 4095) >>
				12) << 12;
			u4pic_size_c_bs =
				(((u4pic_size_y >> 1) + 63) >> 6) << 6;
		} else {
			*u4pic_size_y_bs = ((u4pic_size_y + 4095) >> 12) << 12;
			u4pic_size_c_bs = u4pic_size_y >> 1;
			*u4pic_size_bs =
				((*u4pic_size_y_bs + u4pic_size_c_bs + 511) >>
				9) << 9;
		}
	}

	if (MML_FMT_10BIT_JUMP(src->format) || MML_FMT_AUO(src->format)) {
		/* Y YL C CL*/
		*ufo_dec_length_y = src_buf->dma[0].iova + src->plane_offset[0] +
				   *u4pic_size_y_bs;
		*ufo_dec_length_c = src_buf->dma[1].iova + src->plane_offset[1] +
				   u4pic_size_c_bs;
	} else {
		/* Y C YL CL */
		*ufo_dec_length_y = src_buf->dma[0].iova + src->plane_offset[0] +
				   *u4pic_size_bs;
		*ufo_dec_length_c = src_buf->dma[0].iova + src->plane_offset[0] +
				   *u4pic_size_bs + u4ufo_len_size_y;
	}
}

static void rrot_reset_threshold(struct mml_comp_rrot *rrot,
	struct cmdq_pkt *pkt, const phys_addr_t base_pa)
{
	u32 i;

	/* line cnt 0/1/2/3 in con1/con2 */
	cmdq_pkt_write(pkt, NULL, base_pa + RROT_PREFETCH_CONTROL_1, 0, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + RROT_PREFETCH_CONTROL_2, 0, U32_MAX);

	/* clear threshold for all plane */
	for (i = 0; i < DMABUF_CON_CNT; i++) {
		cmdq_pkt_write(pkt, NULL, base_pa + rrot_dmabuf[i], 0x3, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + rrot_urgent_th[i], 0, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + rrot_ultra_th[i], 0, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + rrot_preultra_th[i], 0, U32_MAX);
	}
}

static void rrot_select_threshold_hrt(struct mml_comp_rrot *rrot,
	struct cmdq_pkt *pkt, const phys_addr_t base_pa,
	u32 format, u32 width, u32 height, u32 rot)
{
	const struct rrot_golden *golden;
	const struct golden_setting *golden_set;
	u32 pixel = width * height;
	u32 idx, i;
	u32 plane = MML_FMT_PLANE(format);

	rot = rot & 0x1; /* only hort and vert case*/

	if (MML_FMT_HYFBC(format)) {
		golden = &rrot->data->golden[GOLDEN_FMT_HYFBC];
	} else if (MML_FMT_AFBC(format)) {
		golden = &rrot->data->golden[GOLDEN_FMT_AFBC];
	} else if (plane == 1) {
		if (MML_FMT_BITS_PER_PIXEL(format) >= 32)
			golden = &rrot->data->golden[GOLDEN_FMT_ARGB];
		else
			golden = &rrot->data->golden[GOLDEN_FMT_RGB];
	} else if (plane == 2) {
		golden = &rrot->data->golden[GOLDEN_FMT_YUV420];
	} else if (plane == 3) {
		golden = &rrot->data->golden[GOLDEN_FMT_YV12];
	} else {
		golden = &rrot->data->golden[GOLDEN_FMT_ARGB];
	}

	for (idx = 0; idx < golden->cnt - 1; idx++)
		if (golden->settings[idx].pixel > pixel)
			break;
	golden_set = &golden->settings[idx];

	/* config threshold for all plane */
	for (i = 0; i < DMABUF_CON_CNT; i++) {
		cmdq_pkt_write(pkt, NULL, base_pa + rrot_urgent_th[i],
			golden_set->plane[i].urgent, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + rrot_ultra_th[i],
			golden_set->plane[i].ultra, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + rrot_preultra_th[i],
			golden_set->plane[i].preultra, U32_MAX);
	}

	cmdq_pkt_write(pkt, NULL, base_pa + RROT_PREFETCH_CONTROL_1,
		golden_set->pfrot[rot].prefetch_ctrl1, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + RROT_PREFETCH_CONTROL_2,
		golden_set->pfrot[rot].prefetch_ctrl2, U32_MAX);
}

static void rrot_config_slice(struct mml_comp *comp, struct mml_frame_config *cfg,
	struct mml_frame_data *src, struct mml_frame_dest *dest, struct rrot_frame_data *rrot_frm,
	struct cmdq_pkt *pkt)
{
	u32 slice_size = 0;
	u32 slice_num;
	u32 slice0 = 0, slice1 = 0, slice_px;
	u32 width;

	if (dest->rotate == MML_ROT_0 || dest->rotate == MML_ROT_180)
		goto done;

	if (MML_FMT_YUV420(src->format) && rrot_frm->blk)
		slice_size = 4 + cfg->bin_x;
	else if (MML_FMT_IS_RGB(src->format) || MML_FMT_YUV444(src->format))
		slice_size = 5;
	else if (MML_FMT_YUV422(src->format) || MML_FMT_YUV420(src->format))
		slice_size = 6 + cfg->bin_x;
	else
		mml_log("no slice for format %#x", src->format);

	width = dest->crop.r.width;
	slice_px = 1 << slice_size;	/* full slice size */
	slice_num = DIV_ROUND_UP(width, slice_px);
	slice0 = slice_size << 16 | slice_num << 3 | 1;
	slice1 = slice_px << 16 | (width - slice_px * (slice_num - 1));
	rrot_frm->slice_pixel = slice_px;

	mml_msg("width %u slice size %u num %u slice0 %#010x slice1 %#010x",
		width, slice_size, slice_num, slice0, slice1);
done:
	cmdq_pkt_write(pkt, NULL, comp->base_pa + RROT_AUTO_SLICE_0, slice0, U32_MAX);
	cmdq_pkt_write(pkt, NULL, comp->base_pa + RROT_AUTO_SLICE_1, slice1, U32_MAX);
}

static void rrot_config_stash(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	if (unlikely(rrot_stash_con1))
		cmdq_pkt_write(pkt, NULL, comp->base_pa + RROT_PREFETCH_CONTROL_1,
			rrot_stash_con1, U32_MAX);
	if (unlikely(rrot_stash_con2))
		cmdq_pkt_write(pkt, NULL, comp->base_pa + RROT_PREFETCH_CONTROL_2,
			rrot_stash_con2, U32_MAX);
}

static s32 rrot_config_frame(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	struct mml_comp_rrot *rrot = comp_to_rrot(comp);
	struct mml_frame_config *cfg = task->config;
	struct rrot_frame_data *rrot_frm = rrot_frm_data(ccfg);
	struct mml_file_buf *src_buf = &task->buf.src;
	struct mml_frame_data *src = &cfg->info.src;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_frame_dest *dest = &cfg->info.dest[0];
	struct mml_frame_size *frame_in = &cfg->frame_in;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];

	const phys_addr_t base_pa = comp->base_pa;
	const u32 dst_fmt = cfg->info.dest[ccfg->node->out_idx].data.format;
	u8 simple_mode = 1;
	u8 filter_mode;
	u8 loose = 0;
	u8 bit_number = 0;
	u8 ufo_auo = 0;
	u8 ufo_jump = 0;
	u8 afbc = 0;
	u8 afbc_y2r = 0;
	u8 hyfbc = 0;
	u8 ufbdc = 0;
	u8 output_10bit = 0;
	u32 width_in_pxl = 0;
	u32 height_in_pxl = 0;
	u64 iova[3];
	u64 ufo_dec_length_y = 0;
	u64 ufo_dec_length_c = 0;
	u32 u4pic_size_bs = 0;
	u32 u4pic_size_y_bs = 0;
	u32 gmcif_con;

	mml_msg("use config %p rrot %p", cfg, rrot);

#ifdef MML_FPGA
	/* clear event in fpga, to avoid cmdq init issue */
	cmdq_pkt_clear_event(pkt, rrot->event_eof);
#endif

	/* before everything start, make sure ddr enable */
	if (ccfg->pipe == 0)
		cfg->task_ops->ddren(task, pkt, true);

	/* Enable engine */
	cmdq_pkt_write(pkt, NULL, base_pa + RROT_EN, 0x1, 0x00000001);
	/* Enable or disable shadow */
	cmdq_pkt_write(pkt, NULL, base_pa + RROT_SHADOW_CTRL,
		((cfg->shadow ? 0 : BIT(1)) << 1) | 0x1, U32_MAX);

	if (mml_rdma_crc) {
		if (MML_FMT_COMPRESS(src->format))
			cmdq_pkt_write(pkt, NULL, base_pa + RROT_DEBUG_CON,
				(rrot_dbg_con << 13) + 0x1, U32_MAX);
		else
			cmdq_pkt_write(pkt, NULL, base_pa + RROT_DEBUG_CON,
				0x1, U32_MAX);
	}

	/* enable binning with horizontal and vertical level, 0 for disable */
	cmdq_pkt_write(pkt, NULL, comp->base_pa + RROT_BINNING,
		cfg->bin_y << 3 |	/* vertical level */
		cfg->bin_x << 1 |	/* horizontal level */
		rrot_frm->binning,
		U32_MAX);

	rrot_color_fmt(cfg, rrot_frm);
	rrot_config_slice(comp, cfg, src, dest, rrot_frm, pkt);

	/* Enable dither on output, not input */
	cmdq_pkt_write(pkt, NULL, base_pa + RROT_DITHER_CON, 0x0, U32_MAX);

	/* priority bits:
	 *	bit[17:16] PRE_ULTRA_EN
	 *	bit[15:14] URGENT_EN
	 *	bit[13:12] ULTRA_EN
	 * and settings:
	 *	0: disable
	 *	1: enable according to threshold
	 *	2: always enable
	 */
	gmcif_con = BIT(0) |		/* COMMAND_DIV */
		    GENMASK(7, 4) |	/* READ_REQUEST_TYPE */
		    GENMASK(9, 8) |	/* WRITE_REQUEST_TYPE */
		    BIT(16);		/* PRE_ULTRA_EN */
	/* racing case also enable urgent/ultra to not blocking disp */
	if (unlikely(mml_rdma_urgent)) {
		if (mml_rdma_urgent == 1)
			gmcif_con ^= BIT(16) | BIT(15);	/* URGENT_EN: always */
		else if (mml_rdma_urgent == 2)
			gmcif_con ^= BIT(16) | BIT(13);	/* ULTRA_EN: always */
		else if (mml_rdma_urgent == 2)
			gmcif_con ^= BIT(16) | BIT(15) | BIT(13); /* URGENT_EN/ULTRA_EN: always */
		else
			gmcif_con |= BIT(14) | BIT(12);	/* URGENT_EN */
		if (cfg->info.mode == MML_MODE_RACING || cfg->info.mode == MML_MODE_DIRECT_LINK)
			rrot_select_threshold_hrt(rrot, pkt, comp->base_pa, src->format,
				frame_in->width, frame_in->height, dest->rotate);
		else
			rrot_reset_threshold(rrot, pkt, base_pa);
	} else if (cfg->info.mode == MML_MODE_DIRECT_LINK) {
		gmcif_con |= BIT(14) | BIT(12);	/* URGENT_EN and ULTRA_EN */
		rrot_select_threshold_hrt(rrot, pkt, comp->base_pa, src->format,
			frame_in->width, frame_in->height, dest->rotate);
	} else {
		rrot_reset_threshold(rrot, pkt, comp->base_pa);
	}

	rrot_config_stash(comp, task, ccfg);

	cmdq_pkt_write(pkt, NULL, base_pa + RROT_GMCIF_CON, gmcif_con, U32_MAX);
	rrot_frm->gmcif_con = gmcif_con;

	if (cfg->alpharot)
		rrot_frm->color_tran = 0;
	else if (MML_FMT_10BIT(src->format))
		rrot_frm->color_tran = 1;

	mml_msg("%s alpha_pq_r2y:%d alpha:%d src:0x%08x dst:0x%08x hdr:%d mode:%d",
		__func__, rrot->data->alpha_pq_r2y, cfg->info.alpha,
		src->format, dst_fmt, cfg->info.dest[0].pq_config.en_hdr, cfg->info.mode);
	if (MML_FMT_IS_RGB(src->format) && cfg->info.dest[0].pq_config.en_hdr &&
	    cfg->info.dest_cnt == 1) {
		rrot_frm->color_tran = 0;
	} else if (rrot->data->alpha_pq_r2y && cfg->alpharsz && dst_fmt == MML_FMT_YUVA8888) {
		/* Fix alpha r2y to full BT601 when resize */
		rrot_frm->color_tran = 1;
		rrot_frm->matrix_sel = 0;
	}

	cmdq_pkt_write(pkt, NULL, base_pa + RROT_TRANSFORM_0,
		   (rrot_frm->matrix_sel << 23) +
		   (rrot_frm->color_tran << 16),
		   U32_MAX);

	if (MML_FMT_V_SUBSAMPLE(src->format) &&
	    !MML_FMT_V_SUBSAMPLE(dst_fmt) &&
	    !MML_FMT_BLOCK(src->format) &&
	    !MML_FMT_HYFBC(src->format))
		/* 420 to 422 interpolation solution */
		filter_mode = 2;
	else
		/* config.enRROTCrop ? 3 : 2 */
		/* RSZ uses YUV422, RROT could use V filter unless cropping */
		filter_mode = 3;

	if (MML_FMT_10BIT_LOOSE(src->format))
		loose = 1;
	if (MML_FMT_10BIT(src->format))
		bit_number = 1;

	cmdq_pkt_write(pkt, NULL, base_pa + RROT_SRC_CON,
		(rrot_frm->hw_fmt << 0) +
		(filter_mode << 9) +
		(loose << 11) +
		(rrot_frm->field << 12) +
		(rrot_frm->swap << 14) +
		(rrot_frm->blk << 15) +
		(1 << 17) +	/* UNIFORM_CONFIG */
		(bit_number << 18) +
		(rrot_frm->blk_tile << 23) +
		(0 << 24) +	/* RING_BUF_READ */
		((cfg->alpharot || cfg->alpharsz) << 25),
		U32_MAX);

	if (rrot_frm->blk_10bit)
		ufo_jump = MML_FMT_10BIT_JUMP(src->format);
	else
		ufo_auo = MML_FMT_AUO(src->format);

	if (MML_FMT_HYFBC(src->format)) {
		hyfbc = 1;
		ufbdc = 1;
		width_in_pxl = ((src->width + 31) >> 5) << 5;
		height_in_pxl = ((src->height + 15) >> 4) << 4;
	} else if (MML_FMT_AFBC(src->format)) {
		afbc = 1;
		if (MML_FMT_IS_RGB(src->format))
			afbc_y2r = 1;
		ufbdc = 1;
		if (MML_FMT_IS_YUV(src->format)) {
			width_in_pxl = ((src->width + 15) >> 4) << 4;
			height_in_pxl = ((src->height + 15) >> 4) << 4;
		} else {
			width_in_pxl = ((src->width + 31) >> 5) << 5;
			height_in_pxl = ((src->height + 7) >> 3) << 3;
		}
	} else if (rrot_frm->enable_ufo && rrot_frm->blk_10bit) {
		width_in_pxl = (src->y_stride << 2) / 5;
	}
	cmdq_pkt_write(pkt, NULL, base_pa + RROT_MF_BKGD_SIZE_IN_PXL,
		width_in_pxl, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + RROT_MF_BKGD_H_SIZE_IN_PXL,
		height_in_pxl, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + RROT_AFBC_PAYLOAD_OST, 0, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + RROT_COMP_CON,
		   (rrot_frm->enable_ufo << 31) +
		   (ufo_auo << 29) +
		   (ufo_jump << 28) +
		   (1 << 26) +	/* UFO_DATA_IN_NOT_REV */
		   (1 << 25) +	/* UFO_DATA_OUT_NOT_REV */
		   (0 << 24) +	/* ufo_dcp */
		   (0 << 23) +	/* ufo_dcp_10bit */
		   (afbc << 22) +
		   (afbc_y2r << 21) +
		   (0 << 20) +	/* pvric_en */
		   (1 << 19) +	/* SHORT_BURST */
		   (12 << 14) +	/* UFBDC_HG_DISABLE */
		   (hyfbc << 13) +
		   (ufbdc << 12) +
		   (1 << 11),	/* payload_ost */
		   U32_MAX);

	if (MML_FMT_HYFBC(src->format)) {
		/* ufo_dec_length_y: Y header addr
		 * ufo_dec_length_c: C header addr
		 * src->plane_offset[0]: offset from buf addr to Y data addr
		 * src->plane_offset[1]: offset from buf addr to C data addr
		 */
		calc_hyfbc(src_buf, src, &ufo_dec_length_y, &iova[0],
			&ufo_dec_length_c, &iova[1]);

		rrot_write_addr(pkt,
			base_pa + RROT_UFO_DEC_LENGTH_BASE_Y,
			base_pa + RROT_UFO_DEC_LENGTH_BASE_Y_MSB,
			ufo_dec_length_y,
			reuse, cache,
			&rrot_frm->labels[RROT_LABEL_UFO_DEC_BASE_Y]);

		rrot_write_addr(pkt,
			base_pa + RROT_UFO_DEC_LENGTH_BASE_C,
			base_pa + RROT_UFO_DEC_LENGTH_BASE_C_MSB,
			ufo_dec_length_c,
			reuse, cache,
			&rrot_frm->labels[RROT_LABEL_UFO_DEC_BASE_C]);
	} else if (rrot_frm->enable_ufo) {
		calc_ufo(src_buf, src, &ufo_dec_length_y, &ufo_dec_length_c,
			 &u4pic_size_bs, &u4pic_size_y_bs);

		rrot_write_addr(pkt,
			base_pa + RROT_UFO_DEC_LENGTH_BASE_Y,
			base_pa + RROT_UFO_DEC_LENGTH_BASE_Y_MSB,
			ufo_dec_length_y,
			reuse, cache,
			&rrot_frm->labels[RROT_LABEL_UFO_DEC_BASE_Y]);

		rrot_write_addr(pkt,
			base_pa + RROT_UFO_DEC_LENGTH_BASE_C,
			base_pa + RROT_UFO_DEC_LENGTH_BASE_C_MSB,
			ufo_dec_length_c,
			reuse, cache,
			&rrot_frm->labels[RROT_LABEL_UFO_DEC_BASE_C]);
	} else {
		rrot_write64(pkt,
			base_pa + RROT_UFO_DEC_LENGTH_BASE_Y,
			base_pa + RROT_UFO_DEC_LENGTH_BASE_Y_MSB,
			0);
		rrot_write64(pkt,
			base_pa + RROT_UFO_DEC_LENGTH_BASE_C,
			base_pa + RROT_UFO_DEC_LENGTH_BASE_C_MSB,
			0);
	}

	/* Enable 10-bit output */
	output_10bit = 1;
	cmdq_pkt_write(pkt, NULL, base_pa + RROT_CON,
		   (rrot_frm->lb_2b_mode << 12) |
		   (output_10bit << 5) |
		   (simple_mode << 4) |
		   dest->flip << 2 |
		   dest->rotate,
		   U32_MAX);

	/* Write frame base address */
	if (MML_FMT_HYFBC(src->format)) {
		/* clear since not use */
		iova[2] = 0;

		mml_msg("%s y %#011llx %#011llx c %#011llx %#011llx",
			__func__, ufo_dec_length_y, iova[0],
			ufo_dec_length_c, iova[1]);

	} else if (rrot_frm->enable_ufo) {
		if (MML_FMT_10BIT_JUMP(src->format) ||
			MML_FMT_AUO(src->format)) {
			iova[0] = src_buf->dma[0].iova + src->plane_offset[0];
			iova[1] = src_buf->dma[0].iova + src->plane_offset[0] +
				  u4pic_size_bs;
			iova[2] = src_buf->dma[2].iova + src->plane_offset[2];
		} else {
			iova[0] = src_buf->dma[0].iova + src->plane_offset[0];
			iova[1] = src_buf->dma[0].iova + src->plane_offset[0] +
				  u4pic_size_y_bs;
			iova[2] = src_buf->dma[2].iova + src->plane_offset[2];
		}

		mml_msg("%s src %#011llx %#011llx ufo %#011llx %#011llx",
			__func__, iova[0], iova[1],
			ufo_dec_length_y, ufo_dec_length_c);
	} else {
		iova[0] = src_buf->dma[0].iova + src->plane_offset[0];
		iova[1] = src_buf->dma[1].iova + src->plane_offset[1];
		iova[2] = src_buf->dma[2].iova + src->plane_offset[2];
		mml_msg("%s src %#011llx %#011llx %#011llx",
			__func__, iova[0], iova[1], iova[2]);
	}

	if (!mml_slt) {
		rrot_write_addr(pkt,
			base_pa + RROT_SRC_BASE_0,
			base_pa + RROT_SRC_BASE_0_MSB,
			iova[0],
			reuse, cache,
			&rrot_frm->labels[RROT_LABEL_BASE_0]);
		rrot_write_addr(pkt,
			base_pa + RROT_SRC_BASE_1,
			base_pa + RROT_SRC_BASE_1_MSB,
			iova[1],
			reuse, cache,
			&rrot_frm->labels[RROT_LABEL_BASE_1]);
		rrot_write_addr(pkt,
			base_pa + RROT_SRC_BASE_2,
			base_pa + RROT_SRC_BASE_2_MSB,
			iova[2],
			reuse, cache,
			&rrot_frm->labels[RROT_LABEL_BASE_2]);
	}

	cmdq_pkt_write(pkt, NULL, base_pa + RROT_MF_BKGD_SIZE_IN_BYTE,
		   src->y_stride, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + RROT_SF_BKGD_SIZE_IN_BYTE,
		   src->uv_stride, U32_MAX);

	return 0;
}

/*
 *              width
 * *-------------------------------*
 * | inxs                     inxe |
 * | *---------------------------* |
 * | |  inxs+lumax   inxe+lumax  | |
 * | |  *---------------------*  | |
 * | |  |outxs           outxe|  | |
 * | |  |                     |  | |
 * | |  *---------------------*  | |
 * | *---------------------------* |
 * *-------------------------------*
 *   ^  ^                     ^  ^
 *   |--|                     |--|
 *   luma                    in_crop
 *
 * luma: crop offset
 * in_crop: input size crop to output size
 *
 * width >= inxe - inxs + 1 = outxe - outxs + luma + incrop
 */
static void rrot_config_left(struct mml_tile_engine *tile)
{
	u32 in_w, out_w;

	in_w = round_up(tile->in.xe - tile->in.xs + 1, 2);
	tile->in.xe = round_up(tile->in.xs + in_w / 2, 32) - 1;
	out_w = tile->in.xe - tile->in.xs + 1 - tile->luma.x;
	tile->out.xe = tile->out.xs + out_w - 1;
}

static void rrot_config_right(struct mml_tile_engine *tile)
{
	u32 in_right_crop, in_left_w, out_w;

	in_right_crop = (tile->in.xe - tile->in.xs) - (tile->out.xe - tile->out.xs) - tile->luma.x;
	in_left_w = round_up(tile->in.xe - tile->in.xs + 1, 2);
	tile->in.xs = round_up(tile->in.xs + in_left_w / 2, 32);
	out_w = tile->in.xe - tile->in.xs + 1 - in_right_crop;
	tile->out.xs = tile->out.xe + 1 - out_w;
	tile->luma.x = 0;
}

static void rrot_config_top(struct mml_tile_engine *tile)
{
	u32 in_h, out_h;

	in_h = round_up(tile->in.ye - tile->in.ys + 1, 2);
	tile->in.ye = round_up(tile->in.ys + in_h / 2, 16) - 1;
	out_h = tile->in.ye - tile->in.ys + 1 - tile->luma.y;
	tile->out.ye = tile->out.ys + out_h - 1;
}

static void rrot_config_bottom(struct mml_tile_engine *tile)
{
	u32 in_bottom_crop, in_top_h, out_h;

	in_bottom_crop = (tile->in.ye - tile->in.ys) - (tile->out.ye - tile->out.ys) - tile->luma.y;
	in_top_h = round_up(tile->in.ye - tile->in.ys + 1, 2);
	tile->in.ys = round_up(tile->in.ys + in_top_h / 2, 16);
	out_h = tile->in.ye - tile->in.ys + 1 - in_bottom_crop;
	tile->out.ys = tile->out.ye + 1 - out_h;
	tile->luma.y = 0;
}

static void rrot_calc_unbin(struct mml_frame_config *cfg, struct mml_tile_engine *tile)
{
	if (cfg->bin_x) {
		u32 in_w = (tile->in.xe - tile->in.xs + 1) << cfg->bin_x;
		u32 out_w = (tile->out.xe - tile->out.xs + 1) << cfg->bin_x;

		tile->in.xs = tile->in.xs << cfg->bin_x;
		tile->in.xe = tile->in.xs + in_w - 1;
		tile->out.xs = tile->out.xs << cfg->bin_x;
		tile->out.xe = tile->out.xs + out_w - 1;
		tile->luma.x = tile->luma.x << cfg->bin_x;
		tile->chroma.x = tile->chroma.x << cfg->bin_x;
	}

	if (cfg->bin_y) {
		u32 in_h = (tile->in.ye - tile->in.ys + 1) << cfg->bin_y;
		u32 out_h = (tile->out.ye - tile->out.ys + 1) << cfg->bin_y;

		tile->in.ys = tile->in.ys << cfg->bin_y;
		tile->in.ye = tile->in.ys + in_h - 1;
		tile->out.ys = tile->out.ys << cfg->bin_y;
		tile->out.ye = tile->out.ys + out_h - 1;
		tile->luma.y = tile->luma.y << cfg->bin_y;
		tile->chroma.y = tile->chroma.y << cfg->bin_y;
	}
}

static void coord_flip(u16 left, u16 *start, u16 *end, u32 size)
{
	u32 crop = *end - *start + 1;

	*start = size - (*end + 1) + left;
	*end = *start + crop - 1;
}

static struct mml_tile_engine rrot_config_dual(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg, struct mml_tile_engine *tile_merge)
{
	struct mml_comp_rrot *rrot = comp_to_rrot(comp);
	const struct mml_frame_dest *dest = &task->config->info.dest[0];
	const struct mml_frame_config *cfg = task->config;
	struct mml_tile_engine tile = *tile_merge;

	rrot_msg("%s frame in crop %u %u %u %u in %u %u %u %u out %u %u %u %u ofst %u %u",
		__func__,
		cfg->frame_in_crop[0].r.left, cfg->frame_in_crop[0].r.top,
		cfg->frame_in_crop[0].r.width, cfg->frame_in_crop[0].r.height,
		tile.in.xs, tile.in.xe, tile.in.ys, tile.in.ye,
		tile.out.xs, tile.out.xe, tile.out.ys, tile.out.ye,
		tile.luma.x, tile.luma.y);

	if ((dest->rotate == MML_ROT_0 && dest->flip) ||
		(dest->rotate == MML_ROT_180 && !dest->flip) ||
		(dest->rotate == MML_ROT_90 && !dest->flip) ||
		(dest->rotate == MML_ROT_270 && dest->flip)) {
		const enum mml_color format = cfg->info.src.format;
		u32 left = cfg->frame_in_crop[0].r.left;
		u32 tile_width = left + cfg->frame_tile_sz.width;

		if (MML_FMT_10BIT_PACKED(format) && !MML_FMT_COMPRESS(format) &&
			!MML_FMT_ALPHA(format) && !MML_FMT_BLOCK(format)) {
			/* 10-bit packed, not compress, not alpha 32-bit, not blk */
			tile_width = round_up(tile_width, 4);
			left = left & ~0x3;
		} else if (MML_FMT_H_SUBSAMPLE(format)) {
			/* YUV422 or YUV420 tile alignment constraints */
			tile_width = round_up(tile_width, 2);
			left = left & ~0x1;
		}

		rrot_msg("%s flip tile %u frame in crop %u %u %u %u inx %u %u outx %u %u",
			__func__, tile_width,
			cfg->frame_in_crop[0].r.left, cfg->frame_in_crop[0].r.top,
			cfg->frame_in_crop[0].r.width, cfg->frame_in_crop[0].r.height,
			tile.in.xs, tile.in.xe, tile.out.xs, tile.out.xe);
		coord_flip(left, &tile.in.xs, &tile.in.xe, tile_width);
		coord_flip(0, &tile.out.xs, &tile.out.xe, cfg->frame_tile_sz.width);
		rrot_msg("%s flip tile %u frame in crop %u %u %u %u inx %u %u outx %u %u (after)",
			__func__, tile_width,
			cfg->frame_in_crop[0].r.left, cfg->frame_in_crop[0].r.top,
			cfg->frame_in_crop[0].r.width, cfg->frame_in_crop[0].r.height,
			tile.in.xs, tile.in.xe, tile.out.xs, tile.out.xe);
	}

	if (dest->rotate == MML_ROT_90 || dest->rotate == MML_ROT_270) {
		swap(tile.in.xs, tile.in.ys);
		swap(tile.in.xe, tile.in.ye);
		swap(tile.out.xs, tile.out.ys);
		swap(tile.out.xe, tile.out.ye);
		swap(tile.luma.x, tile.luma.y);
		swap(tile.luma.x_sub, tile.luma.y_sub);
	}

	if (MML_FMT_COMPRESS(cfg->info.src.format)) {
		/* align 2 for rrot compress format constraint */
		u32 in_xs = round_down(tile.in.xs, 2);
		u32 in_ys = round_down(tile.in.ys, 2);

		/* align (xs, ys) to block position and put crop value into luma,
		 * to make rrot_config_X more easy to calculate position.
		 */
		tile.luma.x += tile.in.xs - in_xs;
		tile.luma.y += tile.in.ys - in_ys;
		tile.in.xs = in_xs;
		tile.in.ys = in_ys;
	}

	if (cfg->rrot_dual) {
		if (rrot->pipe == 0) {
			if ((dest->rotate == MML_ROT_90 && !dest->flip) ||
			    (dest->rotate == MML_ROT_270 && dest->flip))
				rrot_config_bottom(&tile);
			else if ((dest->rotate == MML_ROT_90 && dest->flip) ||
				 (dest->rotate == MML_ROT_270 && !dest->flip))
				rrot_config_top(&tile);
			else if ((dest->rotate == MML_ROT_0 && !dest->flip) ||
				 (dest->rotate == MML_ROT_180 && dest->flip))
				rrot_config_left(&tile);
			else if ((dest->rotate == MML_ROT_0 && dest->flip) ||
				 (dest->rotate == MML_ROT_180 && !dest->flip))
				rrot_config_right(&tile);
		} else {
			if ((dest->rotate == MML_ROT_90 && !dest->flip) ||
			    (dest->rotate == MML_ROT_270 && dest->flip))
				rrot_config_top(&tile);
			else if ((dest->rotate == MML_ROT_90 && dest->flip) ||
				 (dest->rotate == MML_ROT_270 && !dest->flip))
				rrot_config_bottom(&tile);
			else if ((dest->rotate == MML_ROT_0 && !dest->flip) ||
				 (dest->rotate == MML_ROT_180 && dest->flip))
				rrot_config_right(&tile);
			else if ((dest->rotate == MML_ROT_0 && dest->flip) ||
				 (dest->rotate == MML_ROT_180 && !dest->flip))
				rrot_config_left(&tile);
		}
	}

	rrot_calc_unbin(task->config, &tile);

	return tile;
}

#define __target(_offx, _offy, _stride, _bpp) \
	((_offy) * (_stride) + ((_offx) * (_bpp) >> 3))

#define __target_uv(_offx, _offy, _stride, _bpp, _hor_sh, _ver_sh) \
	(((_offy) >> (_ver_sh)) * (_stride) + (((_offx) >> (_hor_sh)) * (_bpp) >> 3))

#define _target(_offx, _offy, _stride, _frm) \
	__target(_offx, _offy, _stride, _frm->bits_per_pixel_y)

#define _target_uv(_offx, _offy, _stride, _frm) \
	__target_uv(_offx, _offy, _stride, _frm->bits_per_pixel_uv, \
		    _frm->hor_shift_uv, _frm->ver_shift_uv)

#define _target_blk(_offx, _offy, _stride, _frm) \
	_target((_offx) * (_frm->vdo_blk_height << _frm->field), \
		(_offy) >> _frm->vdo_blk_shift_h, \
		_stride, _frm)

#define _target_uv_blk(_offx, _offy, _stride, _frm) \
	(((_offy) >> _frm->vdo_blk_shift_h * _stride) + \
	 (((_offx) >> _frm->hor_shift_uv) * \
	 ((_frm->vdo_blk_height >> _frm->ver_shift_uv) << _frm->field) * \
	 _frm->bits_per_pixel_uv >> 3))

#define target_y(_offx, _offy, _stride, _frm) \
	(_frm->blk ? \
		(_target_blk(_offx, _offy, _stride, _frm)) : \
		(_target(_offx, _offy, _stride, _frm)))

#define target_uv(_offx, _offy, _stride, _frm) \
	(_frm->blk ? \
		(_target_uv_blk(_offx, _offy, _stride, _frm)) : \
		(_target_uv(_offx, _offy, _stride, _frm)))

static void rrot_calc_offset(struct mml_frame_data *src, const struct mml_frame_dest *dest,
	struct rrot_frame_data *rrot_frm, struct mml_tile_engine *tile,
	struct rrot_offset *ofst, bool buf_offset)
{
	u64 xs, ys;
	const u64 xe = tile->in.xe - tile->in.xs;
	const u64 ye = tile->in.ye - tile->in.ys;
	const char *msg = NULL;

	/* Calculate offset from begin of buffer to RROT or RROT_2ND */
	if (buf_offset) {
		xs = tile->in.xs;
		ys = tile->in.ys;

		/* Target Y offset */
		ofst->y = target_y(xs, ys, src->y_stride, rrot_frm);

		/* Target U offset */
		ofst->c = target_uv(xs, ys, src->uv_stride, rrot_frm);

		/* Target V offset */
		ofst->v = target_uv(xs, ys, src->uv_stride, rrot_frm);

		msg = "Buffer offset w/o rotation and flip";
		goto done;
	}

	/* Calculate the add_offset,
	 * which is offset by different rotation and flip,
	 * from addr+buffer_offset to the byte that RROT start read.
	 */
	xs = 0;
	ys = 0;
	if ((dest->rotate == MML_ROT_0 && !dest->flip)) {
		ofst->y = target_y(xs, ys, src->y_stride, rrot_frm);
		ofst->c = target_uv(xs, ys, src->uv_stride, rrot_frm);
		ofst->v = target_uv(xs, ys, src->uv_stride, rrot_frm);

		msg = "No flip and no rotation";
	} else if (dest->rotate == MML_ROT_0 && dest->flip) {
		ofst->y = target_y(xs, ys, src->y_stride, rrot_frm);
		ofst->c = target_uv(xs, ys, src->uv_stride, rrot_frm);
		ofst->v = target_uv(xs, ys, src->uv_stride, rrot_frm);

		msg = "Flip without rotation";
	} else if (dest->rotate == MML_ROT_90 && !dest->flip) {
		ofst->y = target_y(xs, ye, src->y_stride, rrot_frm);
		ofst->c = target_uv(xs, ye, src->uv_stride, rrot_frm);
		ofst->v = target_uv(xs, ye, src->uv_stride, rrot_frm);

		msg = "Rotate 90 degree only";
	} else if (dest->rotate == MML_ROT_90 && dest->flip) {
		ofst->y = target_y(xs, ys, src->y_stride, rrot_frm);
		ofst->c = target_uv(xs, ys, src->uv_stride, rrot_frm);
		ofst->v = target_uv(xs, ys, src->uv_stride, rrot_frm);

		msg = "Flip and Rotate 90 degree";
	} else if (dest->rotate == MML_ROT_180 && !dest->flip) {
		ofst->y = target_y(xs, ye, src->y_stride, rrot_frm);
		ofst->c = target_uv(xs, ye, src->uv_stride, rrot_frm);
		ofst->v = target_uv(xs, ye, src->uv_stride, rrot_frm);

		msg = "Rotate 180 degree only";
	} else if (dest->rotate == MML_ROT_180 && dest->flip) {
		ofst->y = target_y(xs, ye, src->y_stride, rrot_frm);
		ofst->c = target_uv(xs, ye, src->uv_stride, rrot_frm);
		ofst->v = target_uv(xs, ye, src->uv_stride, rrot_frm);

		msg = "Flip and Rotate 180 degree";
	} else if (dest->rotate == MML_ROT_270 && !dest->flip) {
		u32 w = xe - xs + 1;
		u32 slice = w & (rrot_frm->slice_pixel - 1);

		if (!slice)
			slice = rrot_frm->slice_pixel;

		ofst->y = target_y(xe + 1 - slice, ys, src->y_stride, rrot_frm);
		ofst->c = target_uv(xe + 1 - slice, ys, src->uv_stride, rrot_frm);
		ofst->v = target_uv(xe + 1 - slice, ys, src->uv_stride, rrot_frm);

		msg = "Rotate 270 degree only";
	} else if (dest->rotate == MML_ROT_270 && dest->flip) {
		u32 w = xe - xs + 1;
		u32 slice = w & (rrot_frm->slice_pixel - 1);

		if (!slice)
			slice = rrot_frm->slice_pixel;

		ofst->y = target_y(xe + 1 - slice, ye, src->y_stride, rrot_frm);
		ofst->c = target_uv(xe + 1 - slice, ye, src->uv_stride, rrot_frm);
		ofst->v = target_uv(xe + 1 - slice, ye, src->uv_stride, rrot_frm);

		msg = "Flip and Rotate 270 degree";
	}

done:
	mml_msg("%s %s: offset Y:%#010llx U:%#010llx V:%#010llx slice %u",
		__func__, msg, ofst->y, ofst->c, ofst->v, rrot_frm->slice_pixel);
}

static s32 rrot_config_tile(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_comp_rrot *rrot = comp_to_rrot(comp);
	struct mml_frame_config *cfg = task->config;
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];
	struct rrot_frame_data *rrot_frm = rrot_frm_data(ccfg);
	struct mml_frame_data *src = &cfg->info.src;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	u32 plane;
	const phys_addr_t base_pa = comp->base_pa;
	struct mml_tile_engine *tile_merge = config_get_tile(cfg, ccfg, idx);
	const struct mml_frame_dest *dest = &cfg->info.dest[0];
	struct rrot_offset ofst;

	u32 src_offset_wp = 0;
	u32 src_offset_hp = 0;
	u32 mf_src_w;
	u32 mf_src_h;
	u32 mf_clip_w;
	u32 mf_clip_h;
	u32 mf_offset_w_1;
	u32 mf_offset_h_1;
	u32 tput_w, tput_h;

	/* Following data retrieve from tile calc result */
	struct mml_tile_engine tile = rrot_config_dual(comp, task, ccfg, tile_merge);
	u64 in_xs = tile.in.xs;
	const u32 in_xe = tile.in.xe;
	u64 in_ys = tile.in.ys;
	const u32 in_ye = tile.in.ye;
	const u32 out_xs = tile.out.xs;
	const u32 out_xe = tile.out.xe;
	const u64 out_ys = tile.out.ys;
	const u32 out_ye = tile.out.ye;
	const u32 crop_ofst_x = tile.luma.x;
	const u32 crop_ofst_y = tile.luma.y;

	rrot_msg("%s rrot %u pipe %i tile %p in %llu %u %llu %u out %u %u %llu %u ofst %u %u",
		__func__, comp->id, rrot->pipe, tile_merge, in_xs, in_xe, in_ys, in_ye,
		out_xs, out_xe, out_ys, out_ye,
		crop_ofst_x, crop_ofst_y);

	if (rrot_frm->blk) {
		/* Alignment X left in block boundary */
		in_xs = ((in_xs >> rrot_frm->vdo_blk_shift_w) <<
			rrot_frm->vdo_blk_shift_w);
		/* Alignment Y top in block boundary */
		in_ys = ((in_ys >> rrot_frm->vdo_blk_shift_h) <<
			rrot_frm->vdo_blk_shift_h);
	}

	if (MML_FMT_AFBC(src->format) || MML_FMT_HYFBC(src->format)) {
		src_offset_wp = in_xs;
		src_offset_hp = in_ys;

		if ((src_offset_wp & 0x1) || (src_offset_hp & 0x1))
			mml_err("%s pipe %u w %u h %u",
				__func__, rrot->pipe, src_offset_wp, src_offset_hp);
	}

	/* config source buffer offset */
	rrot_calc_offset(src, dest, rrot_frm, &tile, &ofst, true);
	rrot_write64(pkt,
		base_pa + RROT_SRC_OFFSET_0,
		base_pa + RROT_SRC_OFFSET_0_MSB,
		ofst.y);
	rrot_write64(pkt,
		base_pa + RROT_SRC_OFFSET_1,
		base_pa + RROT_SRC_OFFSET_1_MSB,
		ofst.c);
	rrot_write64(pkt,
		base_pa + RROT_SRC_OFFSET_2,
		base_pa + RROT_SRC_OFFSET_2_MSB,
		ofst.v);

	rrot_calc_offset(src, dest, rrot_frm, &tile, &ofst, false);
	if (!rrot_frm->blk) {
		/* Set source size */
		mf_src_w = in_xe - in_xs + 1;
		mf_src_h = in_ye - in_ys + 1;

		/* Set target size */
		mf_clip_w = out_xe - out_xs + 1;
		mf_clip_h = out_ye - out_ys + 1;

		/* Set crop offset */
		mf_offset_w_1 = crop_ofst_x;
		mf_offset_h_1 = crop_ofst_y;
	} else {
		/* Set 10bit UFO mode */
		if (MML_FMT_10BIT_PACKED(src->format) && rrot_frm->enable_ufo)
			src_offset_wp = (ofst.y << 2) / 5;

		/* Set source size */
		mf_src_w = in_xe - in_xs + 1;
		mf_src_h = (in_ye - in_ys + 1) << rrot_frm->field;

		/* Set target size */
		mf_clip_w = out_xe - out_xs + 1;
		mf_clip_h = (out_ye - out_ys + 1) << rrot_frm->field;

		/* Set crop offset */
		mf_offset_w_1 = out_xs + rrot_frm->crop_off_l - in_xs;
		mf_offset_h_1 = (out_ys + rrot_frm->crop_off_t - in_ys) << rrot_frm->field;
	}

	rrot_write64(pkt,
		base_pa + RROT_SRC_BASE_ADD_0,
		base_pa + RROT_SRC_BASE_ADD_0_MSB,
		ofst.y);
	rrot_write64(pkt,
		base_pa + RROT_SRC_BASE_ADD_1,
		base_pa + RROT_SRC_BASE_ADD_1_MSB,
		ofst.c);
	rrot_write64(pkt,
		base_pa + RROT_SRC_BASE_ADD_2,
		base_pa + RROT_SRC_BASE_ADD_2_MSB,
		ofst.v);

	cmdq_pkt_write(pkt, NULL, base_pa + RROT_SRC_OFFSET_WP, src_offset_wp, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + RROT_SRC_OFFSET_HP, src_offset_hp, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + RROT_MF_SRC_SIZE,
		(mf_src_h << 16) + mf_src_w, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + RROT_MF_CLIP_SIZE,
		(mf_clip_h << 16) + mf_clip_w, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + RROT_MF_OFFSET_1,
		(mf_offset_h_1 << 16) + mf_offset_w_1, U32_MAX);

	if (dest->rotate == MML_ROT_0 || dest->rotate == MML_ROT_180) {
		cfg->rrot_out[rrot->pipe].width = mf_clip_w >> cfg->bin_x;
		cfg->rrot_out[rrot->pipe].height = mf_clip_h >> cfg->bin_y;
	} else {
		cfg->rrot_out[rrot->pipe].width = mf_clip_h >> cfg->bin_y;
		cfg->rrot_out[rrot->pipe].height = mf_clip_w >> cfg->bin_x;
	}

	/* qos accumulate tile pixel */
	if (MML_FMT_AFBC(src->format) || MML_FMT_HYFBC(src->format)) {
		/* align block 32x16 */
		tput_w = round_up(in_xe + 1, 32) - round_down(in_xs, 32);
		tput_h = round_up(in_ye + 1, 16) - round_down(in_ys, 16);
	} else {
		/* no block align */
		tput_w = mf_src_w;
		tput_h = mf_src_h;
		cache->line_bubble = 0;
	}

	if (rrot->pipe == 0) {
		cache->line_bubble = tput_w - mf_src_w;
		cache->max_size.width = tput_w >> cfg->bin_x;
		cache->max_size.height = tput_h >> cfg->bin_y;
		if (dest->rotate == MML_ROT_90 || dest->rotate == MML_ROT_270)
			swap(cache->max_size.width, cache->max_size.height);
	} else {
		cache->line_bubble = max(cache->line_bubble, tput_w - mf_src_w);

		tput_w = tput_w >> cfg->bin_x;
		tput_h = tput_h >> cfg->bin_y;
		if (dest->rotate == MML_ROT_90 || dest->rotate == MML_ROT_270)
			swap(tput_w, tput_h);
		cache_max_sz(cache, tput_w, tput_h);
	}

	/* calculate qos for later use */
	plane = MML_FMT_PLANE(src->format);
	rrot_frm->datasize += mml_color_get_min_y_size(src->format, mf_src_w, mf_src_h);
	if (plane > 1)
		rrot_frm->datasize += mml_color_get_min_uv_size(src->format, mf_src_w, mf_src_h);
	if (plane > 2)
		rrot_frm->datasize += mml_color_get_min_uv_size(src->format, mf_src_w, mf_src_h);

	rrot_msg("rrot%s whp %u %u src %u %u clip off %u %u clip %u %u pixel %ux%u bubble %u data %u rotate %u",
		rrot->pipe == 1 ? "_2nd" : "    ",
		src_offset_wp, src_offset_hp,
		mf_src_w, mf_src_h,
		mf_offset_w_1, mf_offset_h_1,
		mf_clip_w, mf_clip_h,
		cache->max_size.width, cache->max_size.height,
		cache->line_bubble, rrot_frm->datasize,
		dest->rotate);

	return 0;
}

static s32 rrot_wait(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *ccfg, u32 idx)
{
	if (unlikely(mml_wrot_bkgd_en))
		return 0;

	/* wait rdma frame done */
	cmdq_pkt_wfe(task->pkts[ccfg->pipe], comp_to_rrot(comp)->event_eof);
	return 0;
}

static void rrot_backup_crc(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
	struct rrot_frame_data *rrot_frm = rrot_frm_data(ccfg);
	const phys_addr_t crc_reg = MML_FMT_COMPRESS(task->config->info.src.format) ?
		(comp->base_pa + RROT_MON_STA_0 + 16 * 8) : (comp->base_pa + RROT_CHKS_EXTR);
	const u32 rrot_idx = comp_to_rrot(comp)->pipe;

	if (likely(!mml_rdma_crc))
		return;

	rrot_frm->crc_inst_offset = mml_backup_crc(task, ccfg,
		crc_reg, &task->rdma_crc_idx[rrot_idx]);
	if (!rrot_frm->crc_inst_offset) {
		mml_err("%s fail to backup CRC", __func__);
		mml_rdma_crc = 0;
	}
#endif
}

static void rrot_backup_crc_update(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
	struct rrot_frame_data *rrot_frm = rrot_frm_data(ccfg);
	const u32 rrot_idx = comp_to_rrot(comp)->pipe;

	if (!mml_rdma_crc || !rrot_frm->crc_inst_offset)
		return;

	mml_backup_crc_update(task, ccfg, rrot_frm->crc_inst_offset,
		&task->rdma_crc_idx[rrot_idx]);
#endif
}

static s32 rrot_post(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct rrot_frame_data *rrot_frm = rrot_frm_data(ccfg);
	struct mml_pipe_cache *cache = &task->config->cache[ccfg->pipe];
	u32 pipe = comp_to_rrot(comp)->pipe;

	/* ufo case */
	if (MML_FMT_UFO(cfg->info.src.format))
		rrot_frm->datasize = (u32)div_u64((u64)rrot_frm->datasize * 7, 10);

	/* Data size add to task,
	 * it is ok for rdma to directly assign and accumulate in wrot.
	 */
	if (pipe == 0)
		cache->total_datasize = rrot_frm->datasize;
	else
		cache->total_datasize += rrot_frm->datasize;

	mml_msg("%s task %p pipe %u data %u", __func__, task, pipe, rrot_frm->datasize);

	rrot_backup_crc(comp, task, ccfg);

	/* after rdma stops read, call ddren to sleep */
	if (ccfg->pipe == 0)
		task->config->task_ops->ddren(task, task->pkts[0], false);

	return 0;
}

static s32 rrot_reconfig_frame(struct mml_comp *comp, struct mml_task *task,
			       struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct rrot_frame_data *rrot_frm = rrot_frm_data(ccfg);
	struct mml_file_buf *src_buf = &task->buf.src;
	struct mml_frame_data *src = &cfg->info.src;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];

	u64 iova[3];
	u64 ufo_dec_length_y = 0;
	u64 ufo_dec_length_c = 0;
	u32 u4pic_size_bs = 0;
	u32 u4pic_size_y_bs = 0;

	if (MML_FMT_HYFBC(src->format)) {
		calc_hyfbc(src_buf, src, &ufo_dec_length_y, &iova[0],
			&ufo_dec_length_c, &iova[1]);

		rrot_update_addr(reuse,
				 rrot_frm->labels[RROT_LABEL_UFO_DEC_BASE_Y],
				 rrot_frm->labels[RROT_LABEL_UFO_DEC_BASE_Y_MSB],
				 ufo_dec_length_y);
		rrot_update_addr(reuse,
				 rrot_frm->labels[RROT_LABEL_UFO_DEC_BASE_C],
				 rrot_frm->labels[RROT_LABEL_UFO_DEC_BASE_C_MSB],
				 ufo_dec_length_c);
	} else if (rrot_frm->enable_ufo) {
		calc_ufo(src_buf, src, &ufo_dec_length_y, &ufo_dec_length_c,
			 &u4pic_size_bs, &u4pic_size_y_bs);

		rrot_update_addr(reuse,
				 rrot_frm->labels[RROT_LABEL_UFO_DEC_BASE_Y],
				 rrot_frm->labels[RROT_LABEL_UFO_DEC_BASE_Y_MSB],
				 ufo_dec_length_y);
		rrot_update_addr(reuse,
				 rrot_frm->labels[RROT_LABEL_UFO_DEC_BASE_C],
				 rrot_frm->labels[RROT_LABEL_UFO_DEC_BASE_C_MSB],
				 ufo_dec_length_c);
	}

	/* Write frame base address */
	if (MML_FMT_HYFBC(src->format)) {
		/* clear since not use */
		iova[2] = 0;
	} else if (rrot_frm->enable_ufo) {
		if (MML_FMT_10BIT_JUMP(src->format) ||
			MML_FMT_AUO(src->format)) {
			iova[0] = src_buf->dma[0].iova + src->plane_offset[0];
			iova[1] = src_buf->dma[0].iova + src->plane_offset[0] +
				  u4pic_size_bs;
			iova[2] = src_buf->dma[2].iova + src->plane_offset[2];
		} else {
			iova[0] = src_buf->dma[0].iova + src->plane_offset[0];
			iova[1] = src_buf->dma[0].iova + src->plane_offset[0] +
				  u4pic_size_y_bs;
			iova[2] = src_buf->dma[2].iova + src->plane_offset[2];
		}
	} else {
		iova[0] = src_buf->dma[0].iova + src->plane_offset[0];
		iova[1] = src_buf->dma[1].iova + src->plane_offset[1];
		iova[2] = src_buf->dma[2].iova + src->plane_offset[2];
	}

	rrot_update_addr(reuse,
			 rrot_frm->labels[RROT_LABEL_BASE_0],
			 rrot_frm->labels[RROT_LABEL_BASE_0_MSB],
			 iova[0]);
	rrot_update_addr(reuse,
			 rrot_frm->labels[RROT_LABEL_BASE_1],
			 rrot_frm->labels[RROT_LABEL_BASE_1_MSB],
			 iova[1]);
	rrot_update_addr(reuse,
			 rrot_frm->labels[RROT_LABEL_BASE_2],
			 rrot_frm->labels[RROT_LABEL_BASE_2_MSB],
			 iova[2]);

	rrot_backup_crc_update(comp, task, ccfg);

	return 0;
}

static const struct mml_comp_config_ops rrot_cfg_ops = {
	.prepare = rrot_prepare,
	.buf_map = rrot_buf_map,
	.get_label_count = rrot_get_label_count,
	.frame = rrot_config_frame,
	.tile = rrot_config_tile,
	.wait = rrot_wait,
	.post = rrot_post,
	.reframe = rrot_reconfig_frame,
};

static u32 rrot_datasize_get(struct mml_task *task, struct mml_comp_config *ccfg)
{
	struct rrot_frame_data *rrot_frm = rrot_frm_data(ccfg);

	mml_msg("[rrot]latency:%u", rrot_get_latency(task->config));

	return rrot_frm->datasize;
}

static u32 rrot_format_get(struct mml_task *task, struct mml_comp_config *ccfg)
{
	return task->config->info.dest[ccfg->node->out_idx].data.format;
}

static void rrot_store_crc(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
	struct mml_comp_rrot *rrot = comp_to_rrot(comp);

	if (!mml_rdma_crc)
		return;

	task->src_crc[rrot->pipe] = mml_backup_crc_get(task, ccfg, task->rdma_crc_idx[rrot->pipe]);
	mml_msg("%s rrot0%s component %u job %u pipe %u crc %#010x idx %u",
		__func__, rrot->pipe == 0 ? "    " : "_2nd", comp->id, task->job.jobid,
		ccfg->pipe, task->src_crc[rrot->pipe], task->rdma_crc_idx[rrot->pipe]);
#endif
}

static void rrot_task_done(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	rrot_store_crc(comp, task, ccfg);
}

static const struct mml_comp_hw_ops rrot_hw_ops = {
	.clk_enable = &mml_comp_clk_enable,
	.clk_disable = &mml_comp_clk_disable,
	.qos_datasize_get = &rrot_datasize_get,
	.qos_format_get = &rrot_format_get,
	.qos_set = &mml_comp_qos_set,
	.qos_clear = &mml_comp_qos_clear,
	.task_done = rrot_task_done,
};

static const char *rrot_state(u32 state)
{
	switch (state) {
	case 0x1:
		return "idle";
	case 0x2:
		return "wait sof";
	case 0x4:
		return "reg update";
	case 0x8:
		return "clear0";
	case 0x10:
		return "clear1";
	case 0x20:
		return "int0";
	case 0x40:
		return "int1";
	case 0x80:
		return "data running";
	case 0x100:
		return "wait done";
	case 0x200:
		return "warm reset";
	case 0x400:
		return "wait reset";
	default:
		return "";
	}
}

static void rrot_debug_dump(struct mml_comp *comp)
{
	void __iomem *base = comp->base;
	u32 shadow_ctrl, value[RROT_MON_COUNT], state, greq, i;
	u32 hyfbc_value[RROT_HYFBC_MON_COUNT];

	mml_err("rrot component %u %s dump:", comp->id, comp->name ? comp->name : "");

	/* Enable shadow read working */
	shadow_ctrl = readl(base + RROT_SHADOW_CTRL);
	shadow_ctrl |= 0x4;
	writel(shadow_ctrl, base + RROT_SHADOW_CTRL);

	value[0] = readl(base + RROT_EN);
	value[1] = readl(base + RROT_RESET);
	value[2] = readl(base + RROT_CON);
	value[3] = readl(base + RROT_BINNING);
	value[4] = readl(base + RROT_PREFETCH_CONTROL_0);
	value[5] = readl(base + RROT_PREFETCH_CONTROL_1);
	value[6] = readl(base + RROT_PREFETCH_CONTROL_2);
	value[7] = readl(base + RROT_AUTO_SLICE_0);
	value[8] = readl(base + RROT_AUTO_SLICE_1);

	mml_err("RROT_EN %#010x RROT_RESET %#010x RROT_CON %#010x RROT_BINNING %#010x",
		value[0], value[1], value[2], value[3]);
	mml_err("RROT_PREFETCH_CONTROL 0 %#010x 1 %#010x 2 %#010x",
		value[4], value[5], value[6]);
	mml_err("RROT_AUTO_SLICE_0 %#010x RROT_AUTO_SLICE_1 %#010x",
		value[7], value[8]);

	value[2] = readl(base + RROT_SRC_CON);
	value[3] = readl(base + RROT_COMP_CON);
	value[4] = readl(base + RROT_TRANSFORM_0);
	mml_err("RROT_SRC_CON %#010x RROT_COMP_CON %#010x RROT_TRANSFORM_0 %#010x",
		value[2], value[3], value[4]);

	value[4] = readl(base + RROT_MF_BKGD_SIZE_IN_BYTE);
	value[5] = readl(base + RROT_SF_BKGD_SIZE_IN_BYTE);
	value[6] = readl(base + RROT_MF_BKGD_SIZE_IN_PXL);
	value[7] = readl(base + RROT_MF_BKGD_H_SIZE_IN_PXL);
	value[8] = readl(base + RROT_SRC_OFFSET_WP);
	value[9] = readl(base + RROT_SRC_OFFSET_HP);
	value[10] = readl(base + RROT_MF_SRC_SIZE);
	value[11] = readl(base + RROT_MF_OFFSET_1);
	value[12] = readl(base + RROT_MF_CLIP_SIZE);

	mml_err("RROT_MF_BKGD_SIZE_IN_BYTE %#010x RROT_SF_BKGD_SIZE_IN_BYTE %#010x",
		value[4], value[5]);

	/* following log dump by order out to in: bkgd(frame), src(tile), clip(crop) */
	mml_err("RROT_MF_BKGD_SIZE_IN_PXL %#010x RROT_MF_BKGD_H_SIZE_IN_PXL %#010x",
		value[6], value[7]);
	mml_err("RROT_SRC_OFFSET_WP %#010x RROT_SRC_OFFSET_HP %#010x RROT_MF_SRC_SIZE %#010x",
		value[8], value[9], value[10]);
	mml_err("RROT_MF_OFFSET_1 %#010x RROT_MF_CLIP_SIZE %#010x",
		value[11], value[12]);

	value[13] = readl(base + RROT_SRC_BASE_0_MSB);
	value[14] = readl(base + RROT_SRC_BASE_0);
	value[15] = readl(base + RROT_SRC_BASE_1_MSB);
	value[16] = readl(base + RROT_SRC_BASE_1);
	value[17] = readl(base + RROT_SRC_BASE_2_MSB);
	value[18] = readl(base + RROT_SRC_BASE_2);

	mml_err("RROT_SRC     BASE_0 %#03x%08x     BASE_1 %#03x%08x     BASE_2 %#03x%08x",
		value[13], value[14],
		value[15], value[16],
		value[17], value[18]);

	value[13] = readl(base + RROT_SRC_OFFSET_0);
	value[14] = readl(base + RROT_SRC_OFFSET_1);
	value[15] = readl(base + RROT_SRC_OFFSET_2);
	value[16] = readl(base + RROT_SRC_BASE_ADD_0);
	value[17] = readl(base + RROT_SRC_BASE_ADD_1);
	value[18] = readl(base + RROT_SRC_BASE_ADD_2);

	mml_err("RROT_SRC   OFFSET_0 %#011x   OFFSET_1 %#011x   OFFSET_2 %#011x",
		value[13], value[14], value[15]);
	mml_err("RROT_SRC BASE_ADD_0 %#011x BASE_ADD_1 %#011x BASE_ADD_2 %#011x",
		value[16], value[17], value[18]);

	value[25] = readl(base + RROT_UFO_DEC_LENGTH_BASE_Y_MSB);
	value[26] = readl(base + RROT_UFO_DEC_LENGTH_BASE_Y);
	value[27] = readl(base + RROT_UFO_DEC_LENGTH_BASE_C_MSB);
	value[28] = readl(base + RROT_UFO_DEC_LENGTH_BASE_C);
	value[29] = readl(base + RROT_AFBC_PAYLOAD_OST);
	value[30] = readl(base + RROT_GMCIF_CON);

	mml_err("RROT_UFO_DEC_LENGTH BASE_Y %#03x%08x BASE_C %#03x%08x",
		value[25], value[26], value[27], value[28]);
	mml_err("RROT_AFBC_PAYLOAD_OST %#010x RROT_GMCIF_CON %#010x",
		value[29], value[30]);

	if (mml_rdma_crc) {
		value[31] = readl(base + RROT_CHKS_EXTR);
		value[32] = readl(base + RROT_DEBUG_CON);
		mml_err("RROT_CHKS_EXTR %#010x RROT_DEBUG_CON %#010x",
			value[31], value[32]);
	}

	/* mon sta from 0 ~ 37 */
	for (i = 0; i < RROT_MON_COUNT; i++)
		value[i] = readl(base + RROT_MON_STA_0 + i * 8);

	for (i = 0; i < RROT_MON_COUNT / 3; i++) {
		mml_err("RROT_MON_STA_%-2u %#010x RROT_MON_STA_%-2u %#010x RROT_MON_STA_%-2u %#010x",
			i * 3, value[i * 3],
			i * 3 + 1, value[i * 3 + 1],
			i * 3 + 2, value[i * 3 + 2]);
	}
	mml_err("RROT_MON_STA_36 %#010x RROT_MON_STA_37 %#010x",
		value[36], value[37]);

	for (i = 0; i < RROT_HYFBC_MON_COUNT; i++) {
		writel(hyfbc_setting[i], base + RROT_DEBUG_CON);
		hyfbc_value[i] = readl(base + RROT_MON_STA_0 + 16 * 8);
	}

	mml_err("RROT_DEBUG_CON: RROT_MON_STA_16 %#06x: %#010x, %#06x: %#010x, %#06x: %#010x, %#06x: %#010x",
		hyfbc_setting[0], hyfbc_value[0], hyfbc_setting[1], hyfbc_value[1],
		hyfbc_setting[2], hyfbc_value[2], hyfbc_setting[3], hyfbc_value[3]);

	/* parse state */
	mml_err("RROT ack:%u req:%d ufo:%u",
		(value[0] >> 11) & 0x1, (value[0] >> 10) & 0x1,
		(value[0] >> 25) & 0x1);
	state = (value[1] >> 8) & 0x7ff;
	greq = (value[0] >> 21) & 0x1;
	mml_err("RROT state: %#x (%s)", state, rrot_state(state));
	mml_err("RROT horz_cnt %u vert_cnt %u",
		value[26] & 0xffff, (value[26] >> 16) & 0xffff);
	mml_err("RROT greq:%u => suggest to ask SMI help:%u", greq, greq);
}

static const struct mml_comp_debug_ops rrot_debug_ops = {
	.dump = &rrot_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_rrot *rrot = dev_get_drvdata(dev);
	s32 ret;

	ret = mml_register_comp(master, &rrot->comp);
	if (ret)
		dev_err(dev, "Failed to register mml component %s: %d\n",
			dev->of_node->full_name, ret);
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_rrot *rrot = dev_get_drvdata(dev);

	mml_unregister_comp(master, &rrot->comp);
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mml_comp_rrot *dbg_probed_components[4];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_rrot *priv;
	s32 ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = of_device_get_match_data(dev);

	if (smmu_v3_enabled()) {
		/* shared smmu device, setup 34bit in dts */
		priv->mmu_dev = mml_smmu_get_shared_device(dev, "mtk,smmu-shared");
		priv->mmu_dev_sec = mml_smmu_get_shared_device(dev, "mtk,smmu-shared-sec");
	} else {
		priv->mmu_dev = dev;
		priv->mmu_dev_sec = dev;
		ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));
		if (ret)
			mml_err("fail to config rrot dma mask %d", ret);
	}

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}

	/* init larb for smi and mtcmos */
	ret = mml_comp_init_larb(&priv->comp, dev);
	if (ret) {
		if (ret == -EPROBE_DEFER)
			return ret;
		dev_err(dev, "fail to init component %u larb ret %d\n",
			priv->comp.id, ret);
	}

	/* get pipe of rrot */
	if (of_property_read_u8(dev->of_node, "pipe", &priv->pipe))
		mml_err("read pipe fail");

	if (of_property_read_u16(dev->of_node, "event-frame-done", &priv->event_eof))
		mml_err("read event-frame-done fail");

	/* assign ops */
	priv->comp.tile_ops = &rrot_tile_ops;
	priv->comp.config_ops = &rrot_cfg_ops;
	priv->comp.hw_ops = &rrot_hw_ops;
	priv->comp.debug_ops = &rrot_debug_ops;

	dbg_probed_components[dbg_probed_count++] = priv;

	ret = component_add(dev, &mml_comp_ops);
	if (ret)
		dev_err(dev, "Failed to add component: %d\n", ret);

	mml_log("rrot%d (%s %u) pipe %d eof %u",
		priv->pipe, priv->comp.name ? priv->comp.name : "",
		priv->comp.id, priv->pipe, priv->event_eof);

	return 0;
}

static int remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mml_comp_ops);
	return 0;
}

const struct of_device_id mml_rrot_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6989-mml_rrot",
		.data = &mt6989_rrot_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_rrot_driver_dt_match);

struct platform_driver mml_rrot_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-rrot",
		.owner = THIS_MODULE,
		.of_match_table = mml_rrot_driver_dt_match,
	},
};

//module_platform_driver(mml_rrot_driver);

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML RROT driver");
MODULE_LICENSE("GPL");
