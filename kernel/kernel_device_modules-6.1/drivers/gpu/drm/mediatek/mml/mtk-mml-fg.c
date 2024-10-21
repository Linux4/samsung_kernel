/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <mtk-smmu-v3.h>

#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-fg-fw.h"

#include "tile_driver.h"

#define REG_NOT_SUPPORT 0xfff

enum mml_fg_reg_index {
	FG_TRIGGER,
	FG_STATUS,
	FG_CTRL_0,
	FG_CK_EN,
	FG_SHADOW_CTRL,
	FG_BACK_DOOR_0,
	FG_CRC_TBL_0,
	FG_CRC_TBL_1,
	FG_PIC_INFO_0,
	FG_PIC_INFO_1,
	FG_TILE_INFO_0,
	FG_TILE_INFO_1,
	FG_DEBUG_0,
	FG_DEBUG_1,
	FG_DEBUG_2,
	FG_DEBUG_3,
	FG_DEBUG_4,
	FG_DEBUG_5,
	FG_DEBUG_6,
	FG_PPS_0,
	FG_PPS_1,
	FG_PPS_2,
	FG_PPS_3,
	FG_LUMA_TBL_BASE,
	FG_CB_TBL_BASE,
	FG_CR_TBL_BASE,
	FG_LUT_BASE,
	FG_LUMA_TBL_BASE_MSB,
	FG_CB_TBL_BASE_MSB,
	FG_CR_TBL_BASE_MSB,
	FG_LUT_BASE_MSB,
	FG_IRQ_CTRL,
	FG_IRQ_STATUS,
	FG_SRAM_CTRL,
	FG_SRAM_STATUS,
	FG_REG_MAX_COUNT
};

static const u16 fg_reg_table_mt6897[FG_REG_MAX_COUNT] = {
	[FG_TRIGGER] = 0x000,
	[FG_STATUS] = 0x004,
	[FG_CTRL_0] = 0x020,
	[FG_CK_EN] = 0x024,
	[FG_SHADOW_CTRL] = 0x028,
	[FG_BACK_DOOR_0] = 0x02c,
	[FG_CRC_TBL_0] = 0x030,
	[FG_CRC_TBL_1] = 0x034,
	[FG_PIC_INFO_0] = 0x400,
	[FG_PIC_INFO_1] = 0x404,
	[FG_TILE_INFO_0] = 0x418,
	[FG_TILE_INFO_1] = 0x41c,
	[FG_DEBUG_0] = 0x500,
	[FG_DEBUG_1] = 0x504,
	[FG_DEBUG_2] = 0x508,
	[FG_DEBUG_3] = 0x50c,
	[FG_DEBUG_4] = 0x510,
	[FG_DEBUG_5] = 0x514,
	[FG_DEBUG_6] = 0x518,
	[FG_PPS_0] = 0x408,
	[FG_PPS_1] = 0x40C,
	[FG_PPS_2] = 0x410,
	[FG_PPS_3] = 0x414,
	[FG_LUMA_TBL_BASE] = 0x008,
	[FG_CB_TBL_BASE] = 0x00c,
	[FG_CR_TBL_BASE] = 0x010,
	[FG_LUT_BASE] = 0x014,
	[FG_LUMA_TBL_BASE_MSB] = 0x05c,
	[FG_CB_TBL_BASE_MSB] = 0x060,
	[FG_CR_TBL_BASE_MSB] = 0x064,
	[FG_LUT_BASE_MSB] = 0x068,
	[FG_IRQ_CTRL] = 0x018,
	[FG_IRQ_STATUS] = 0x01c,
	[FG_SRAM_CTRL] = REG_NOT_SUPPORT,
	[FG_SRAM_STATUS] = REG_NOT_SUPPORT
};

static const u16 fg_reg_table_mt6989[FG_REG_MAX_COUNT] = {
	[FG_TRIGGER] = 0x000,
	[FG_STATUS] = 0x004,
	[FG_CTRL_0] = 0x020,
	[FG_CK_EN] = 0x024,
	[FG_SHADOW_CTRL] = 0x028,
	[FG_BACK_DOOR_0] = 0x02c,
	[FG_CRC_TBL_0] = 0x030,
	[FG_CRC_TBL_1] = 0x034,
	[FG_PIC_INFO_0] = 0x400,
	[FG_PIC_INFO_1] = 0x404,
	[FG_TILE_INFO_0] = 0x418,
	[FG_TILE_INFO_1] = 0x41c,
	[FG_DEBUG_0] = 0x500,
	[FG_DEBUG_1] = 0x504,
	[FG_DEBUG_2] = 0x508,
	[FG_DEBUG_3] = 0x50c,
	[FG_DEBUG_4] = 0x510,
	[FG_DEBUG_5] = 0x514,
	[FG_DEBUG_6] = 0x518,
	[FG_PPS_0] = 0x408,
	[FG_PPS_1] = 0x40C,
	[FG_PPS_2] = 0x410,
	[FG_PPS_3] = 0x414,
	[FG_LUMA_TBL_BASE] = 0x008,
	[FG_CB_TBL_BASE] = 0x00c,
	[FG_CR_TBL_BASE] = 0x010,
	[FG_LUT_BASE] = 0x014,
	[FG_LUMA_TBL_BASE_MSB] = 0x05c,
	[FG_CB_TBL_BASE_MSB] = 0x060,
	[FG_CR_TBL_BASE_MSB] = 0x064,
	[FG_LUT_BASE_MSB] = 0x068,
	[FG_IRQ_CTRL] = 0x018,
	[FG_IRQ_STATUS] = 0x01c,
	[FG_SRAM_CTRL] = 0x070,
	[FG_SRAM_STATUS] = 0x074
};

struct fg_data {
	u16 gpr[MML_PIPE_CNT];
	const u16 *reg_table;
};

static const struct fg_data mt6893_fg_data = {
};

static const struct fg_data mt6897_fg_data = {
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.reg_table = fg_reg_table_mt6897,
};

static const struct fg_data mt6989_fg_data = {
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.reg_table = fg_reg_table_mt6989,
};

struct mml_comp_fg {
	struct mml_comp comp;
	const struct fg_data *data;
	struct device *mmu_dev;	/* for dmabuf to iova */
	struct device *mmu_dev_sec; /* for secure dmabuf to secure iova */
};

enum fg_label_index {
	FG_REUSE_LABEL = 0,
	FG_LUMA_TBL_BASE_LABEL,
	FG_CB_TBL_BASE_LABEL,
	FG_CR_TBL_BASE_LABEL,
	FG_LUT_BASE_LABEL,
	FG_LUMA_TBL_BASE_MSB_LABEL,
	FG_CB_TBL_BASE_MSB_LABEL,
	FG_CR_TBL_BASE_MSB_LABEL,
	FG_LUT_BASE_MSB_LABEL,
	FG_PPS_0_LABEL,
	FG_PPS_1_LABEL,
	FG_PPS_2_LABEL,
	FG_PPS_3_LABEL,
	FG_CTRL_0_LABEL,
	FG_CK_EN_LABEL,
	FG_TRIGGER_LABEL_0,
	FG_TRIGGER_LABEL_1,
	FG_LABEL_TOTAL
};

/* meta data for each different frame config */
struct fg_frame_data {
	u8 out_idx;
	u16 labels[FG_LABEL_TOTAL];
	bool config_success;
};

#define fg_frm_data(i)	((struct fg_frame_data *)(i->data))

static inline struct mml_comp_fg *comp_to_fg(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_fg, comp);
}

static s32 fg_prepare(struct mml_comp *comp, struct mml_task *task,
		      struct mml_comp_config *ccfg)
{
	struct fg_frame_data *fg_frm;

	fg_frm = kzalloc(sizeof(*fg_frm), GFP_KERNEL);
	ccfg->data = fg_frm;
	/* cache out index for easy use */
	fg_frm->out_idx = ccfg->node->out_idx;

	return 0;
}

static s32 fg_tile_prepare(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg,
			   struct tile_func_block *func,
			   union mml_tile_data *data)
{
	struct fg_frame_data *fg_frm = fg_frm_data(ccfg);
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_size *frame_in = &cfg->frame_in;
	struct mml_crop *crop = &cfg->frame_in_crop[fg_frm->out_idx];

	func->enable_flag = true;

	if (cfg->info.dest_cnt == 1 &&
	    (crop->r.width != frame_in->width ||
	    crop->r.height != frame_in->height)) {
		func->full_size_x_in = cfg->frame_tile_sz.width + crop->r.left;
		func->full_size_y_in = cfg->frame_tile_sz.height + crop->r.top;
	} else {
		func->full_size_x_in = frame_in->width;
		func->full_size_y_in = frame_in->height;
	}
	func->full_size_x_out = func->full_size_x_in;
	func->full_size_y_out = func->full_size_y_in;

	return 0;
}

static const struct mml_comp_tile_ops fg_tile_ops = {
	.prepare = fg_tile_prepare,
};

static u32 fg_get_label_count(struct mml_comp *comp, struct mml_task *task,
			       struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];

	mml_pq_msg("%s pipe_id[%d] engine_id[%d] en_fg[%d]", __func__,
		ccfg->pipe, comp->id, dest->pq_config.en_fg);

	if (!dest->pq_config.en_fg)
		return 0;

	return FG_LABEL_TOTAL;
}

static s32 fg_init(struct mml_comp *comp, struct mml_task *task,
		   struct mml_comp_config *ccfg)
{
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;
	struct mml_comp_fg *fg = comp_to_fg(comp);

	cmdq_pkt_write(pkt, NULL, base_pa + fg->data->reg_table[FG_TRIGGER], 0, U32_MAX);

	/* Enable shadow */
	cmdq_pkt_write(pkt, NULL, base_pa + fg->data->reg_table[FG_SHADOW_CTRL], 0x2, U32_MAX);

	return 0;
}

static s32 fg_config_frame(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;
	struct mml_frame_config *cfg = task->config;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct fg_frame_data *fg_frm = fg_frm_data(ccfg);
	struct mml_comp_fg *fg = comp_to_fg(comp);
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];
	const struct mml_crop *crop = &cfg->frame_in_crop[ccfg->node->out_idx];
	struct mml_pq_film_grain_params *fg_meta =
		&task->pq_param[ccfg->node->out_idx].video_param.fg_meta;
	struct device *dev_buf = smmu_v3_enabled()?
		fg->mmu_dev : task->config->path[ccfg->pipe]->clt->chan->mbox->dev;
	bool relay_mode = !dest->pq_config.en_fg;
	u32 gpr = fg->data->gpr[ccfg->pipe];
	s32 i, ret = 0;
	u8 bit_depth = 10;
	bool smi_sw_reset = 1;
	bool crc_cg_enable = 1;
	bool is_yuv_444 = true;
	bool buf_ready = true;
	dma_addr_t fg_table_pa[FG_BUF_NUM] = {0};

	mml_pq_trace_ex_begin("%s %d", __func__, cfg->info.mode);
	mml_pq_msg("%s engine_id[%d] en_fg[%d] width[%d] height[%d]", __func__, comp->id,
		dest->pq_config.en_fg, crop->r.width, crop->r.height);

	if (relay_mode) {
		cmdq_pkt_write(pkt, NULL, base_pa + fg->data->reg_table[FG_CTRL_0], 1, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + fg->data->reg_table[FG_CK_EN], 0x7, U32_MAX);
		/* bit_depth & is yuv420 or yuv444 */
		cmdq_pkt_write(pkt, NULL, base_pa + fg->data->reg_table[FG_PIC_INFO_0],
			!is_yuv_444 << 4 | bit_depth << 0, 1 << 4 | 0xF << 0);
		goto exit;
	}

	mml_pq_get_fg_buffer(task, ccfg->pipe, dev_buf);

	for (i = 0; i < FG_BUF_NUM; i++) {
		if (unlikely(!task->pq_task->fg_table[i]) ||
			unlikely(!task->pq_task->fg_table[i]->va)) {
			mml_pq_err("%s job_id[%d] fg_table[%d] is null",
				__func__, task->job.jobid, i);
			buf_ready = false;
			break;
		}

		fg_table_pa[i] = task->pq_task->fg_table[i]->pa;
		if ((task->pq_task->fg_table[i]->pa >> 34) > 0) {
			mml_pq_err("%s job_id[%d] fg[%d] pa addr exceed 34 bits [%llx]",
				__func__, task->job.jobid, i, fg_table_pa[i]);
			buf_ready = false;
			break;
		}
	}

	if (buf_ready) {
		mml_pq_fg_calc(task->pq_task->fg_table, fg_meta, is_yuv_444, bit_depth);
		/* sync dmabuf for lumn, cb, cr */
		for (int i = 0; i < FG_BUF_NUM-1; i++)
			dma_sync_single_range_for_device(
				dev_buf, fg_table_pa[i], 0, FG_BUF_GRAIN_SIZE, DMA_TO_DEVICE);
		/* sync dmabuf for scaling table */
		dma_sync_single_range_for_device(
			dev_buf, fg_table_pa[FG_BUF_NUM-1], 0, FG_BUF_SCALING_SIZE, DMA_TO_DEVICE);

		/* enable filmGrain */
		mml_write(pkt, base_pa + fg->data->reg_table[FG_CTRL_0], relay_mode << 0, 1 << 0,
			reuse, cache, &fg_frm->labels[FG_CTRL_0_LABEL]);
		mml_write(pkt, base_pa + fg->data->reg_table[FG_CK_EN], 0xF, 0xF,
			reuse, cache, &fg_frm->labels[FG_CK_EN_LABEL]);
	} else {
		/* relay filmGrain */
		mml_write(pkt, base_pa + fg->data->reg_table[FG_CTRL_0], 1, 1 << 0,
			reuse, cache, &fg_frm->labels[FG_CTRL_0_LABEL]);
		mml_write(pkt, base_pa + fg->data->reg_table[FG_CK_EN], 0x7, 0xF,
			reuse, cache, &fg_frm->labels[FG_CK_EN_LABEL]);
	}

	// smi sw reset
	cmdq_pkt_write(pkt, NULL, base_pa + fg->data->reg_table[FG_BACK_DOOR_0],
		smi_sw_reset << 0 | crc_cg_enable << 4, 1 << 0 | 1 << 4);

	/* set FilmGrain Table Address */
	mml_pq_msg("%s FG_LUMA_TBL_ADDR[%llx]", __func__, fg_table_pa[0]);
	mml_pq_msg("%s FG_CB_TBL_ADDR[%llx]", __func__, fg_table_pa[1]);
	mml_pq_msg("%s FG_CR_TBL_ADDR[%llx]", __func__, fg_table_pa[2]);
	mml_pq_msg("%s FG_LUT_TBL_ADDR[%llx]", __func__, fg_table_pa[3]);

	mml_write(pkt, base_pa + fg->data->reg_table[FG_LUMA_TBL_BASE],
		(u32)(fg_table_pa[0]),
		U32_MAX, reuse, cache, &fg_frm->labels[FG_LUMA_TBL_BASE_LABEL]);
	mml_write(pkt, base_pa + fg->data->reg_table[FG_CB_TBL_BASE],
		(u32)(fg_table_pa[1]),
		U32_MAX, reuse, cache, &fg_frm->labels[FG_CB_TBL_BASE_LABEL]);
	mml_write(pkt, base_pa + fg->data->reg_table[FG_CR_TBL_BASE],
		(u32)(fg_table_pa[2]),
		U32_MAX, reuse, cache, &fg_frm->labels[FG_CR_TBL_BASE_LABEL]);
	mml_write(pkt, base_pa + fg->data->reg_table[FG_LUT_BASE],
		(u32)(fg_table_pa[3]),
		U32_MAX, reuse, cache, &fg_frm->labels[FG_LUT_BASE_LABEL]);

	mml_write(pkt, base_pa + fg->data->reg_table[FG_LUMA_TBL_BASE_MSB],
		(fg_table_pa[0]) >> 32,
		U32_MAX, reuse, cache, &fg_frm->labels[FG_LUMA_TBL_BASE_MSB_LABEL]);
	mml_write(pkt, base_pa + fg->data->reg_table[FG_CB_TBL_BASE_MSB],
		(fg_table_pa[1]) >> 32,
		U32_MAX, reuse, cache, &fg_frm->labels[FG_CB_TBL_BASE_MSB_LABEL]);
	mml_write(pkt, base_pa + fg->data->reg_table[FG_CR_TBL_BASE_MSB],
		(fg_table_pa[2]) >> 32,
		U32_MAX, reuse, cache, &fg_frm->labels[FG_CR_TBL_BASE_MSB_LABEL]);
	mml_write(pkt, base_pa + fg->data->reg_table[FG_LUT_BASE_MSB],
		(fg_table_pa[3]) >> 32,
		U32_MAX, reuse, cache, &fg_frm->labels[FG_LUT_BASE_MSB_LABEL]);

	/* bit_depth & is yuv420 or yuv444 */
	cmdq_pkt_write(pkt, NULL, base_pa + fg->data->reg_table[FG_PIC_INFO_0],
		!is_yuv_444 << 4 | bit_depth << 0, 1 << 4 | 0xF << 0);

	/* picture widht & height */
	cmdq_pkt_write(pkt, NULL, base_pa + fg->data->reg_table[FG_PIC_INFO_1],
		crop->r.width << 0 | crop->r.height << 16, U32_MAX);

	/* config pps */
	mml_write(pkt, base_pa + fg->data->reg_table[FG_PPS_0], mml_pq_fg_get_pps0(fg_meta),
		0x1FFFFFFF, reuse, cache, &fg_frm->labels[FG_PPS_0_LABEL]);
	mml_write(pkt, base_pa + fg->data->reg_table[FG_PPS_1], mml_pq_fg_get_pps1(fg_meta),
		0x01FFFFFF, reuse, cache, &fg_frm->labels[FG_PPS_1_LABEL]);
	mml_write(pkt, base_pa + fg->data->reg_table[FG_PPS_2], mml_pq_fg_get_pps2(fg_meta),
		0x01FFFFFF, reuse, cache, &fg_frm->labels[FG_PPS_2_LABEL]);
	mml_write(pkt, base_pa + fg->data->reg_table[FG_PPS_3], mml_pq_fg_get_pps3(fg_meta),
		0x00FFFFFF, reuse, cache, &fg_frm->labels[FG_PPS_3_LABEL]);

	/* trigger FG load table */
	if (buf_ready) {
		mml_write(pkt, base_pa + fg->data->reg_table[FG_TRIGGER], 1 << 0, 1 << 0,
			reuse, cache, &fg_frm->labels[FG_TRIGGER_LABEL_0]);
		mml_write(pkt, base_pa + fg->data->reg_table[FG_TRIGGER], 0 << 0, 1 << 0,
			reuse, cache, &fg_frm->labels[FG_TRIGGER_LABEL_1]);
	} else {
		mml_write(pkt, base_pa + fg->data->reg_table[FG_TRIGGER], 0 << 0, 1 << 0,
			reuse, cache, &fg_frm->labels[FG_TRIGGER_LABEL_0]);
		mml_write(pkt, base_pa + fg->data->reg_table[FG_TRIGGER], 0 << 0, 1 << 0,
			reuse, cache, &fg_frm->labels[FG_TRIGGER_LABEL_1]);
	}

	if (fg->data->reg_table[FG_SRAM_CTRL] != REG_NOT_SUPPORT) {
		cmdq_pkt_write(pkt, NULL, base_pa + fg->data->reg_table[FG_SRAM_CTRL], 0x0, 0x3);
		cmdq_pkt_poll(pkt, NULL,
			0x1, base_pa + fg->data->reg_table[FG_SRAM_STATUS], 0x1, gpr);
	}

exit:
	mml_pq_trace_ex_end();
	return ret;
}

static s32 fg_config_tile(struct mml_comp *comp, struct mml_task *task,
			  struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;
	struct mml_comp_fg *fg = comp_to_fg(comp);

	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);

	u32 width = tile->in.xe - tile->in.xs + 1;
	u32 height = tile->in.ye - tile->in.ys + 1;

	cmdq_pkt_write(pkt, NULL, base_pa + fg->data->reg_table[FG_TILE_INFO_0],
		(tile->in.xs & 0xffff) + ((width & 0xffff) << 16), U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + fg->data->reg_table[FG_TILE_INFO_1],
		(tile->in.ys & 0xffff) + ((height & 0xffff) << 16), U32_MAX);
	return 0;
}

static s32 fg_reconfig_frame(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct fg_frame_data *fg_frm = fg_frm_data(ccfg);
	struct mml_comp_fg *fg = comp_to_fg(comp);
	struct mml_pq_film_grain_params *fg_meta =
		&task->pq_param[ccfg->node->out_idx].video_param.fg_meta;
	struct device *dev_buf = smmu_v3_enabled()?
		fg->mmu_dev : task->config->path[ccfg->pipe]->clt->chan->mbox->dev;
	bool relay_mode = !dest->pq_config.en_fg;
	s32 i, ret = 0;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	u8 bit_depth = 10;
	bool is_yuv_444 = true;
	dma_addr_t fg_table_pa[FG_BUF_NUM];

	mml_pq_trace_ex_begin("%s %d", __func__, cfg->info.mode);

	mml_pq_msg("%s engine_id[%d] en_fg[%d]", __func__, comp->id,
		dest->pq_config.en_fg);

	if (!dest->pq_config.en_fg)
		goto exit;

	mml_pq_get_fg_buffer(task, ccfg->pipe, dev_buf);

	for (i = 0; i < FG_BUF_NUM; i++) {
		if (unlikely(!task->pq_task->fg_table[i]) ||
			unlikely(!task->pq_task->fg_table[i]->va)) {
			mml_pq_err("%s job_id[%d] fg_table[%d] is null",
				__func__, task->job.jobid, i);
			goto buf_err_exit;
		}

		fg_table_pa[i] = task->pq_task->fg_table[i]->pa;
		if ((task->pq_task->fg_table[i]->pa >> 34) > 0) {
			mml_pq_err("%s job_id[%d] fg[%d] pa addr exceed 34 bits [%llx]",
				__func__, task->job.jobid, i, fg_table_pa[i]);
			goto buf_err_exit;
		}
	}

	/* enable filmGrain */
	mml_update(reuse, fg_frm->labels[FG_CTRL_0_LABEL], relay_mode << 0);
	mml_update(reuse, fg_frm->labels[FG_CK_EN_LABEL], 0xF);

	mml_pq_fg_calc(task->pq_task->fg_table, fg_meta, is_yuv_444, bit_depth);

	/* sync dmabuf for lumn, cb, cr */
	for (int i = 0; i < FG_BUF_NUM-1; i++)
		dma_sync_single_range_for_device(
			dev_buf, fg_table_pa[i], 0, FG_BUF_GRAIN_SIZE, DMA_TO_DEVICE);
	/* sync dmabuf for scaling table */
	dma_sync_single_range_for_device(
		dev_buf, fg_table_pa[FG_BUF_NUM-1], 0, FG_BUF_SCALING_SIZE, DMA_TO_DEVICE);

	/* set FilmGrain Table Address */
	mml_pq_msg("%s FG_LUMA_TBL_ADDR[%llx]", __func__, fg_table_pa[0]);
	mml_pq_msg("%s FG_CB_TBL_ADDR[%llx]", __func__, fg_table_pa[1]);
	mml_pq_msg("%s FG_CR_TBL_ADDR[%llx]", __func__, fg_table_pa[2]);
	mml_pq_msg("%s FG_LUT_TBL_ADDR[%llx]", __func__, fg_table_pa[3]);

	mml_update(reuse, fg_frm->labels[FG_LUMA_TBL_BASE_LABEL],
		(u32)(fg_table_pa[0]));
	mml_update(reuse, fg_frm->labels[FG_CB_TBL_BASE_LABEL],
		(u32)(fg_table_pa[1]));
	mml_update(reuse, fg_frm->labels[FG_CR_TBL_BASE_LABEL],
		(u32)(fg_table_pa[2]));
	mml_update(reuse, fg_frm->labels[FG_LUT_BASE_LABEL],
		(u32)(fg_table_pa[3]));

	mml_update(reuse, fg_frm->labels[FG_LUMA_TBL_BASE_MSB_LABEL],
		(fg_table_pa[0]) >> 32);
	mml_update(reuse, fg_frm->labels[FG_CB_TBL_BASE_MSB_LABEL],
		(fg_table_pa[1]) >> 32);
	mml_update(reuse, fg_frm->labels[FG_CR_TBL_BASE_MSB_LABEL],
		(fg_table_pa[2]) >> 32);
	mml_update(reuse, fg_frm->labels[FG_LUT_BASE_MSB_LABEL],
		(fg_table_pa[3]) >> 32);

	/* trigger FG load table */
	mml_update(reuse, fg_frm->labels[FG_TRIGGER_LABEL_0], 0x1);
	mml_update(reuse, fg_frm->labels[FG_TRIGGER_LABEL_1], 0x0);

	/* config pps */
	mml_update(reuse, fg_frm->labels[FG_PPS_0_LABEL], mml_pq_fg_get_pps0(fg_meta));
	mml_update(reuse, fg_frm->labels[FG_PPS_1_LABEL], mml_pq_fg_get_pps1(fg_meta));
	mml_update(reuse, fg_frm->labels[FG_PPS_2_LABEL], mml_pq_fg_get_pps2(fg_meta));
	mml_update(reuse, fg_frm->labels[FG_PPS_3_LABEL], mml_pq_fg_get_pps3(fg_meta));

exit:
	mml_pq_trace_ex_end();
	return ret;

buf_err_exit:
	/* relay filmGrain */
	mml_update(reuse, fg_frm->labels[FG_CTRL_0_LABEL], 0x1);
	mml_update(reuse, fg_frm->labels[FG_CK_EN_LABEL], 0x7);
	/* don't trigger FG load table */
	mml_update(reuse, fg_frm->labels[FG_TRIGGER_LABEL_0], 0x0);
	mml_update(reuse, fg_frm->labels[FG_TRIGGER_LABEL_1], 0x0);
	mml_pq_trace_ex_end();
	return ret;
}

static const struct mml_comp_config_ops fg_cfg_ops = {
	.prepare = fg_prepare,
	.get_label_count = fg_get_label_count,
	.init = fg_init,
	.frame = fg_config_frame,
	.tile = fg_config_tile,
	.reframe = fg_reconfig_frame,
};

static void fg_task_done(struct mml_comp *comp, struct mml_task *task,
				  struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_comp_fg *fg = comp_to_fg(comp);
	struct device *dev_buf = smmu_v3_enabled()?
		fg->mmu_dev : task->config->path[ccfg->pipe]->clt->chan->mbox->dev;

	mml_pq_trace_ex_begin("%s %d", __func__, cfg->info.mode);
	mml_pq_msg("%s engine_id[%d] en_fg[%d]", __func__, comp->id,
		dest->pq_config.en_fg);
	if (!dest->pq_config.en_fg)
		goto exit;

	mml_pq_put_fg_buffer(task, ccfg->pipe, dev_buf);

exit:
	mml_pq_trace_ex_end();
}

static const struct mml_comp_hw_ops fg_hw_ops = {
	.clk_enable = &mml_comp_clk_enable,
	.clk_disable = &mml_comp_clk_disable,
	.task_done = fg_task_done,
};

static void fg_debug_dump(struct mml_comp *comp)
{
	struct mml_comp_fg *fg = comp_to_fg(comp);
	void __iomem *base = comp->base;
	u32 value[33];
	u32 shadow_ctrl;

	mml_err("fg component %u dump:", comp->id);

	/* Enable shadow read working */
	shadow_ctrl = readl(base + fg->data->reg_table[FG_SHADOW_CTRL]);
	shadow_ctrl |= 0x4;
	writel(shadow_ctrl, base + fg->data->reg_table[FG_SHADOW_CTRL]);

	value[0] = readl(base + fg->data->reg_table[FG_TRIGGER]);
	value[1] = readl(base + fg->data->reg_table[FG_STATUS]);
	value[2] = readl(base + fg->data->reg_table[FG_CTRL_0]);
	value[3] = readl(base + fg->data->reg_table[FG_CK_EN]);
	value[4] = readl(base + fg->data->reg_table[FG_BACK_DOOR_0]);
	value[5] = readl(base + fg->data->reg_table[FG_PIC_INFO_0]);
	value[6] = readl(base + fg->data->reg_table[FG_PIC_INFO_1]);
	value[7] = readl(base + fg->data->reg_table[FG_TILE_INFO_0]);
	value[8] = readl(base + fg->data->reg_table[FG_TILE_INFO_1]);
	value[9] = readl(base + fg->data->reg_table[FG_DEBUG_0]);
	value[10] = readl(base + fg->data->reg_table[FG_DEBUG_1]);
	value[11] = readl(base + fg->data->reg_table[FG_DEBUG_2]);
	value[12] = readl(base + fg->data->reg_table[FG_DEBUG_3]);
	value[13] = readl(base + fg->data->reg_table[FG_DEBUG_4]);
	value[14] = readl(base + fg->data->reg_table[FG_DEBUG_5]);
	value[15] = readl(base + fg->data->reg_table[FG_DEBUG_6]);
	value[16] = readl(base + fg->data->reg_table[FG_LUMA_TBL_BASE]);
	value[17] = readl(base + fg->data->reg_table[FG_CB_TBL_BASE]);
	value[18] = readl(base + fg->data->reg_table[FG_CR_TBL_BASE]);
	value[19] = readl(base + fg->data->reg_table[FG_LUT_BASE]);
	value[20] = readl(base + fg->data->reg_table[FG_LUMA_TBL_BASE_MSB]);
	value[21] = readl(base + fg->data->reg_table[FG_CB_TBL_BASE_MSB]);
	value[22] = readl(base + fg->data->reg_table[FG_CR_TBL_BASE_MSB]);
	value[23] = readl(base + fg->data->reg_table[FG_LUT_BASE_MSB]);
	value[24] = readl(base + fg->data->reg_table[FG_PPS_0]);
	value[25] = readl(base + fg->data->reg_table[FG_PPS_1]);
	value[26] = readl(base + fg->data->reg_table[FG_PPS_2]);
	value[27] = readl(base + fg->data->reg_table[FG_PPS_3]);
	value[28] = readl(base + fg->data->reg_table[FG_IRQ_STATUS]);

	if (fg->data->reg_table[FG_SRAM_CTRL] != REG_NOT_SUPPORT) {
		value[29] = readl(base + fg->data->reg_table[FG_SRAM_CTRL]);
		value[30] = readl(base + fg->data->reg_table[FG_SRAM_STATUS]);
	}
	value[31] = readl(base + fg->data->reg_table[FG_CRC_TBL_0]);
	value[32] = readl(base + fg->data->reg_table[FG_CRC_TBL_1]);

	mml_err("FG_TRIGGER %#010x FG_STATUS %#010x",
		value[0], value[1]);
	mml_err("FG_CTRL_0 %#010x FG_CK_EN %#010x",
		value[2], value[3]);
	if (fg->data->reg_table[FG_SRAM_CTRL] != REG_NOT_SUPPORT) {
		mml_err("FG_SRAM_CTRL %#010x FG_SRAM_STATUS %#010x",
			value[29], value[30]);
	}
	mml_err("FG_IRQ_STATUS %#010x", value[28]);
	mml_err("FG_BACK_DOOR_0 %#010x FG_PIC_INFO_0 %#010x FG_PIC_INFO_1 %#010x",
		value[4], value[5], value[6]);
	mml_err("FG_TILE_INFO_0 %#010x FG_TILE_INFO_1 %#010x",
		value[7], value[8]);
	mml_err("FG_LUMA_TBL_BASE %#010x", value[16]);
	mml_err("FG_CB_TBL_BASE %#010x", value[17]);
	mml_err("FG_CR_TBL_BASE %#010x", value[18]);
	mml_err("FG_LUT_BASE %#010x", value[19]);
	mml_err("FG_LUMA_TBL_BASE_MSB %#010x", value[20]);
	mml_err("FG_CB_TBL_BASE_MSB %#010x", value[21]);
	mml_err("FG_CR_TBL_BASE_MSB %#010x", value[22]);
	mml_err("FG_LUT_BASE_MSB %#010x", value[23]);
	mml_err("FG_PPS_0 %#010x FG_PPS_1 %#010x FG_PPS_2 %#010x FG_PPS_3 %#010x",
		value[24], value[25], value[26], value[27]);
	mml_err("FG_DEBUG_0 %#010x FG_DEBUG_1 %#010x FG_DEBUG_2 %#010x",
		value[9], value[10], value[11]);
	mml_err("FG_DEBUG_3 %#010x FG_DEBUG_4 %#010x FG_DEBUG_5 %#010x",
		value[12], value[13], value[14]);
	mml_err("FG_DEBUG_6 %#010x ", value[15]);
	mml_err("FG_CRC_TBL_0 %#010x FG_CRC_TBL_1 %#010x",
		value[31], value[32]);
}

static const struct mml_comp_debug_ops fg_debug_ops = {
	.dump = &fg_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_fg *fg = dev_get_drvdata(dev);
	s32 ret;

	ret = mml_register_comp(master, &fg->comp);
	if (ret)
		dev_err(dev, "Failed to register mml component %s: %d\n",
			dev->of_node->full_name, ret);
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_fg *fg = dev_get_drvdata(dev);

	mml_unregister_comp(master, &fg->comp);
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mml_comp_fg *dbg_probed_components[2];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_fg *priv;
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
			mml_err("fail to config fg dma mask %d", ret);
	}

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}

	/* assign ops */
	priv->comp.tile_ops = &fg_tile_ops;
	priv->comp.config_ops = &fg_cfg_ops;
	priv->comp.hw_ops = &fg_hw_ops;
	priv->comp.debug_ops = &fg_debug_ops;

	dbg_probed_components[dbg_probed_count++] = priv;

	ret = component_add(dev, &mml_comp_ops);
	if (ret)
		dev_err(dev, "Failed to add component: %d\n", ret);

	return ret;
}

static int remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mml_comp_ops);
	return 0;
}

const struct of_device_id mml_fg_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6893-mml_fg",
		.data = &mt6893_fg_data
	},
	{
		.compatible = "mediatek,mt6897-mml_fg",
		.data = &mt6897_fg_data
	},
	{
		.compatible = "mediatek,mt6989-mml_fg",
		.data = &mt6989_fg_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_fg_driver_dt_match);

struct platform_driver mml_fg_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-fg",
		.owner = THIS_MODULE,
		.of_match_table = mml_fg_driver_dt_match,
	},
};

//module_platform_driver(mml_fg_driver);

static s32 ut_case;
static s32 ut_set(const char *val, const struct kernel_param *kp)
{
	s32 result;

	result = sscanf(val, "%d", &ut_case);
	if (result != 1) {
		mml_err("invalid input: %s, result(%d)", val, result);
		return -EINVAL;
	}
	mml_log("%s: case_id=%d", __func__, ut_case);

	switch (ut_case) {
	case 0:
		mml_log("use read to dump current pwm setting");
		break;
	default:
		mml_err("invalid case_id: %d", ut_case);
		break;
	}

	mml_log("%s END", __func__);
	return 0;
}

static s32 ut_get(char *buf, const struct kernel_param *kp)
{
	s32 length = 0;
	u32 i;

	switch (ut_case) {
	case 0:
		length += snprintf(buf + length, PAGE_SIZE - length,
			"[%d] probed count: %d\n", ut_case, dbg_probed_count);
		for(i = 0; i < dbg_probed_count; i++) {
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  - [%d] mml_comp_id: %d\n", i,
				dbg_probed_components[i]->comp.id);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -      mml_bound: %d\n",
				dbg_probed_components[i]->comp.bound);
		}
		break;
	default:
		mml_err("not support read for case_id: %d", ut_case);
		break;
	}
	buf[length] = '\0';

	return length;
}

static struct kernel_param_ops up_param_ops = {
	.set = ut_set,
	.get = ut_get,
};
module_param_cb(fg_ut_case, &up_param_ops, NULL, 0644);
MODULE_PARM_DESC(fg_ut_case, "mml fg UT test case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML FG driver");
MODULE_LICENSE("GPL v2");
