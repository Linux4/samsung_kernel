// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <mtk_drm_ddp_comp.h>

#include "mtk-mml-color.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-dle-adaptor.h"
#include "mtk-mml-pq-core.h"

#include "tile_driver.h"
#include "mtk-mml-tile.h"
#include "tile_mdp_func.h"

#undef pr_fmt
#define pr_fmt(fmt) "[mml_pq_tdshp]" fmt


#define TDSHP_WAIT_TIMEOUT_MS (50)
#define DS_REG_NUM (36)
#define REG_NOT_SUPPORT 0xfff
#define DS_CLARITY_REG_NUM (42)
#define VALID_CONTOUR_HIST_VALUE (0x07FFFFFF)
#define call_hw_op(_comp, op, ...) \
	(_comp->hw_ops->op ? _comp->hw_ops->op(_comp, ##__VA_ARGS__) : 0)


enum mml_tdshp_reg_index {
	TDSHP_00,
	HIST_CFG_00,
	HIST_CFG_01,
	TDSHP_CTRL,
	TDSHP_INTEN,
	TDSHP_INTSTA,
	TDSHP_STATUS,
	TDSHP_CFG,
	TDSHP_INPUT_COUNT,
	TDSHP_OUTPUT_COUNT,
	TDSHP_INPUT_SIZE,
	TDSHP_OUTPUT_OFFSET,
	TDSHP_OUTPUT_SIZE,
	TDSHP_BLANK_WIDTH,
	/* REGION_PQ_SIZE_PARAMETER_MODE_SEGMENTATION_LENGTH */
	TDSHP_REGION_PQ_PARAM,
	TDSHP_SHADOW_CTRL,
	CONTOUR_HIST_00,
	TDSHP_STATUS_00,
	TDHSP_C_BOOST_MAIN,
	TDSHP_REG_MAX_COUNT
};

static const u16 tdshp_reg_table_mt6983[TDSHP_REG_MAX_COUNT] = {
	[TDSHP_00] = 0x000,
	[HIST_CFG_00] = 0x064,
	[HIST_CFG_01] = 0x068,
	[TDSHP_CTRL] = 0x100,
	[TDSHP_INTEN] = 0x104,
	[TDSHP_INTSTA] = 0x108,
	[TDSHP_STATUS] = 0x10c,
	[TDSHP_CFG] = 0x110,
	[TDSHP_INPUT_COUNT] = 0x114,
	[TDSHP_OUTPUT_COUNT] = 0x11c,
	[TDSHP_INPUT_SIZE] = 0x120,
	[TDSHP_OUTPUT_OFFSET] = 0x124,
	[TDSHP_OUTPUT_SIZE] = 0x128,
	[TDSHP_BLANK_WIDTH] = 0x12c,
	[TDSHP_REGION_PQ_PARAM] = REG_NOT_SUPPORT,
	[TDSHP_SHADOW_CTRL] = 0x67c,
	[CONTOUR_HIST_00] = 0x3dc,
	[TDSHP_STATUS_00] = REG_NOT_SUPPORT,
	[TDHSP_C_BOOST_MAIN] = 0x0E0
};

static const u16 tdshp_reg_table_mt6985[TDSHP_REG_MAX_COUNT] = {
	[TDSHP_00] = 0x000,
	[HIST_CFG_00] = 0x064,
	[HIST_CFG_01] = 0x068,
	[TDSHP_CTRL] = 0x100,
	[TDSHP_INTEN] = 0x104,
	[TDSHP_INTSTA] = 0x108,
	[TDSHP_STATUS] = 0x10c,
	[TDSHP_CFG] = 0x110,
	[TDSHP_INPUT_COUNT] = 0x114,
	[TDSHP_OUTPUT_COUNT] = 0x11c,
	[TDSHP_INPUT_SIZE] = 0x120,
	[TDSHP_OUTPUT_OFFSET] = 0x124,
	[TDSHP_OUTPUT_SIZE] = 0x128,
	[TDSHP_BLANK_WIDTH] = 0x12c,
	[TDSHP_REGION_PQ_PARAM] = 0x680,
	[TDSHP_SHADOW_CTRL] = 0x724,
	[CONTOUR_HIST_00] = 0x3dc,
	[TDSHP_STATUS_00] = 0x644,
	[TDHSP_C_BOOST_MAIN] = 0x0E0
};

struct tdshp_data {
	u32 tile_width;
	/* u32 min_hfg_width; 9: HFG min + TDSHP crop */
	u16 gpr[MML_PIPE_CNT];
	u16 cpr[MML_PIPE_CNT];
	const u16 *reg_table;
	u8 rb_mode;
	bool wrot_pending;
};

static const struct tdshp_data mt6893_tdshp_data = {
	.tile_width = 528,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = tdshp_reg_table_mt6983,
	.rb_mode = RB_EOF_MODE,
};

static const struct tdshp_data mt6983_tdshp_data = {
	.tile_width = 1628,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = tdshp_reg_table_mt6983,
	.rb_mode = RB_EOF_MODE,
};

static const struct tdshp_data mt6879_tdshp_data = {
	.tile_width = 1344,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = tdshp_reg_table_mt6983,
	.rb_mode = RB_EOF_MODE,
};

static const struct tdshp_data mt6895_tdshp_data = {
	.tile_width = 1926,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = tdshp_reg_table_mt6983,
	.rb_mode = RB_EOF_MODE,
};

static const struct tdshp_data mt6985_tdshp_data = {
	.tile_width = 1666,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = tdshp_reg_table_mt6985,
	.rb_mode = RB_EOF_MODE,
};

static const struct tdshp_data mt6886_tdshp_data = {
	.tile_width = 1300,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = tdshp_reg_table_mt6983,
	.rb_mode = RB_EOF_MODE,
};

static const struct tdshp_data mt6989_tdshp_data = {
	.tile_width = 3332,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = tdshp_reg_table_mt6985,
	.rb_mode = RB_EOF_MODE,
	.wrot_pending = true,
};

static const struct tdshp_data mt6878_tdshp_data = {
	.tile_width = 528,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = tdshp_reg_table_mt6983,
	.rb_mode = RB_EOF_MODE,
};

struct mml_comp_tdshp {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	const struct tdshp_data *data;
	bool ddp_bound;
	u8 pipe;
	u8 out_idx;
	u16 event_eof;
	u32 jobid;
	struct mml_pq_task *pq_task;
	bool dual;
	bool hist_cmd_done;
	bool clarity_readback;
	bool dc_readback;
	bool dpc;
	struct mml_comp *mmlsys_comp;
	struct mutex hist_cmd_lock;
	struct mml_pq_readback_buffer *tdshp_hist[MML_PIPE_CNT];
	struct cmdq_client *clt;
	struct cmdq_pkt *hist_pkts[MML_PIPE_CNT];
	struct workqueue_struct *tdshp_hist_wq;
	struct work_struct tdshp_hist_task;
	struct mml_pq_config pq_config;
	struct mml_pq_frame_data frame_data;
	struct mml_dev *mml;
};

enum tdshp_label_index {
	TDSHP_REUSE_LABEL = 0,
	TDSHP_POLLGPR_0 = DS_REG_NUM+DS_CLARITY_REG_NUM,
	TDSHP_POLLGPR_1,
	TDSHP_LABEL_TOTAL
};

/* meta data for each different frame config */
struct tdshp_frame_data {
	u32 out_hist_xs;
	u32 out_hist_ys;
	u32 cut_pos_x;
	u16 labels[TDSHP_LABEL_TOTAL];
	bool is_clarity_need_readback;
	bool is_dc_need_readback;
	bool config_success;
};

#define tdshp_frm_data(i)	((struct tdshp_frame_data *)(i->data))

static inline struct mml_comp_tdshp *comp_to_tdshp(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_tdshp, comp);
}

static s32 tdshp_prepare(struct mml_comp *comp, struct mml_task *task,
			 struct mml_comp_config *ccfg)
{
	struct tdshp_frame_data *tdshp_frm = NULL;

	tdshp_frm = kzalloc(sizeof(*tdshp_frm), GFP_KERNEL);

	ccfg->data = tdshp_frm;
	return 0;
}

static s32 tdshp_buf_prepare(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	s32 ret = 0;

	if (!dest->pq_config.en_sharp && !dest->pq_config.en_dc)
		return ret;

	mml_pq_trace_ex_begin("%s", __func__);
	ret = mml_pq_set_comp_config(task);
	mml_pq_trace_ex_end();
	return ret;
}

static s32 tdshp_tile_prepare(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg,
			      struct tile_func_block *func,
			      union mml_tile_data *data)
{
	const struct mml_frame_config *cfg = task->config;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	const struct mml_frame_size *frame_out = &cfg->frame_out[ccfg->node->out_idx];
	const struct mml_comp_tdshp *tdshp = comp_to_tdshp(comp);
	const u8 rotate = cfg->out_rotate[ccfg->node->out_idx];

	data->tdshp.relay_mode = !dest->pq_config.en_sharp;
	data->tdshp.max_width = tdshp->data->tile_width;
	/* TDSHP support crop capability, if no HFG. */
	func->type = TILE_TYPE_CROP_EN;
	func->init_func = tile_tdshp_init;
	func->for_func = tile_crop_for;
	func->data = data;

	func->enable_flag = dest->pq_config.en_sharp ||
			    (cfg->info.mode == MML_MODE_DDP_ADDON ||
			    cfg->info.mode == MML_MODE_DIRECT_LINK);
	if (tdshp->data->wrot_pending) {
		func->full_size_x_in = frame_out->width;
		func->full_size_y_in = frame_out->height;
		func->full_size_x_out = frame_out->width;
		func->full_size_y_out = frame_out->height;
	} else if (rotate == MML_ROT_90 || rotate == MML_ROT_270) {
		func->full_size_x_in = dest->data.height;
		func->full_size_y_in = dest->data.width;
		func->full_size_x_out = dest->data.height;
		func->full_size_y_out = dest->data.width;
	} else {
		func->full_size_x_in = dest->data.width;
		func->full_size_y_in = dest->data.height;
		func->full_size_x_out = dest->data.width;
		func->full_size_y_out = dest->data.height;
	}

	return 0;
}

static const struct mml_comp_tile_ops tdshp_tile_ops = {
	.prepare = tdshp_tile_prepare,
};

static u32 tdshp_get_label_count(struct mml_comp *comp, struct mml_task *task,
			struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];

	mml_pq_msg("%s pipe_id[%d] engine_id[%d] en_sharp[%d]", __func__,
		ccfg->pipe, comp->id, dest->pq_config.en_sharp);

	if (!dest->pq_config.en_sharp && !dest->pq_config.en_dc)
		return 0;

	return TDSHP_LABEL_TOTAL;
}

static void tdshp_init(struct mml_comp *comp, struct cmdq_pkt *pkt, const phys_addr_t base_pa)
{
	struct mml_comp_tdshp *tdshp = comp_to_tdshp(comp);

	cmdq_pkt_write(pkt, NULL, base_pa + tdshp->data->reg_table[TDSHP_CTRL], 0x1, 0x00000001);
	cmdq_pkt_write(pkt, NULL, base_pa + tdshp->data->reg_table[TDSHP_CFG], 0x2, 0x00000002);
	/* Enable shadow */
	cmdq_pkt_write(pkt, NULL,
		base_pa + tdshp->data->reg_table[TDSHP_SHADOW_CTRL], 0x2, U32_MAX);
}

static void tdshp_relay(struct mml_comp *comp, struct cmdq_pkt *pkt, const phys_addr_t base_pa,
			u32 relay)
{
	struct mml_comp_tdshp *tdshp = comp_to_tdshp(comp);

	/* 17	ALPHA_EN
	 * 0	RELAY_MODE
	 */
	cmdq_pkt_write(pkt, NULL, base_pa + tdshp->data->reg_table[TDSHP_CFG], relay, 0x00020001);
}

static void tdshp_config_region_pq(struct mml_comp *comp, struct cmdq_pkt *pkt,
			const phys_addr_t base_pa, const struct mml_pq_config *cfg)
{
	struct mml_comp_tdshp *tdshp = comp_to_tdshp(comp);

	if (tdshp->data->reg_table[TDSHP_REGION_PQ_PARAM] != REG_NOT_SUPPORT) {

		mml_pq_msg("%s:en_region_pq[%d] en_color[%d]", __func__,
			cfg->en_region_pq, cfg->en_color);

		if (!cfg->en_region_pq)
			cmdq_pkt_write(pkt, NULL,
				base_pa + tdshp->data->reg_table[TDSHP_REGION_PQ_PARAM],
				0, U32_MAX);
		else if (cfg->en_region_pq && !cfg->en_color)
			cmdq_pkt_write(pkt, NULL,
				base_pa + tdshp->data->reg_table[TDSHP_REGION_PQ_PARAM],
				0x5C1B0, U32_MAX);
		else if (cfg->en_region_pq && cfg->en_color)
			cmdq_pkt_write(pkt, NULL,
				base_pa + tdshp->data->reg_table[TDSHP_REGION_PQ_PARAM],
				0xDC1B0, U32_MAX);
	}
}

static s32 tdshp_config_init(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	tdshp_init(comp, task->pkts[ccfg->pipe], comp->base_pa);
	return 0;
}

static struct mml_pq_comp_config_result *get_tdshp_comp_config_result(
	struct mml_task *task)
{
	return task->pq_task->comp_config.result;
}

static s32 tdshp_hist_ctrl(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg,
			    struct mml_pq_comp_config_result *result)
{
	struct mml_comp_tdshp *tdshp = comp_to_tdshp(comp);
	struct mml_frame_config *cfg = task->config;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct tdshp_frame_data *tdshp_frm = tdshp_frm_data(ccfg);

	if (!result->is_dc_need_readback && !result->is_clarity_need_readback)
		return 0;

	mml_pq_set_tdshp_status(task->pq_task, ccfg->node->out_idx);

	if (task->pq_task->read_status.tdshp_comp == MML_PQ_HIST_IDLE) {

		if (!tdshp->clt)
			tdshp->clt = mml_get_cmdq_clt(cfg->mml,
				ccfg->pipe + GCE_THREAD_START);

		if ((!tdshp->hist_pkts[ccfg->pipe] && tdshp->clt))
			tdshp->hist_pkts[ccfg->pipe] =
				cmdq_pkt_create(tdshp->clt);

		if (!tdshp->tdshp_hist[ccfg->pipe])
			tdshp->tdshp_hist[ccfg->pipe] =
				kzalloc(sizeof(struct mml_pq_readback_buffer),
				GFP_KERNEL);

		if (tdshp->tdshp_hist[ccfg->pipe] &&
			!tdshp->tdshp_hist[ccfg->pipe]->va &&
			tdshp->clt)
			tdshp->tdshp_hist[ccfg->pipe]->va =
				(u32 *)cmdq_mbox_buf_alloc(tdshp->clt,
				&tdshp->tdshp_hist[ccfg->pipe]->pa);

		if (tdshp->hist_pkts[ccfg->pipe] && tdshp->tdshp_hist[ccfg->pipe]->va) {

			tdshp->pq_task = task->pq_task;
			mml_pq_get_pq_task(tdshp->pq_task);

			tdshp->pipe = ccfg->pipe;
			tdshp->dual = cfg->dual;
			tdshp->out_idx = ccfg->node->out_idx;
			tdshp->jobid = task->job.jobid;

			tdshp->frame_data.size_info.out_rotate[ccfg->node->out_idx] =
				cfg->out_rotate[ccfg->node->out_idx];
			memcpy(&tdshp->pq_config, &dest->pq_config,
				sizeof(struct mml_pq_config));
			tdshp->clarity_readback = tdshp_frm->is_clarity_need_readback;
			tdshp->dc_readback = tdshp_frm->is_dc_need_readback;
			memcpy(&tdshp->frame_data.pq_param, task->pq_param,
				MML_MAX_OUTPUTS * sizeof(struct mml_pq_param));
			memcpy(&tdshp->frame_data.info, &task->config->info,
				sizeof(struct mml_frame_info));
			memcpy(&tdshp->frame_data.frame_out, &task->config->frame_out,
				MML_MAX_OUTPUTS * sizeof(struct mml_frame_size));
			memcpy(&tdshp->frame_data.size_info.frame_in_crop_s[0],
				&cfg->frame_in_crop[0],	MML_MAX_OUTPUTS * sizeof(struct mml_crop));

			tdshp->frame_data.size_info.crop_size_s.width =
				cfg->frame_tile_sz.width;
			tdshp->frame_data.size_info.crop_size_s.height=
				cfg->frame_tile_sz.height;
			tdshp->frame_data.size_info.frame_size_s.width = cfg->frame_in.width;
			tdshp->frame_data.size_info.frame_size_s.height = cfg->frame_in.height;


			tdshp->hist_pkts[ccfg->pipe]->no_irq = task->config->dpc;

			if (tdshp->data->rb_mode == RB_EOF_MODE) {
				mml_clock_lock(task->config->mml);
				/* ccf power on */
				call_hw_op(task->config->path[0]->mmlsys,
					mminfra_pw_enable);
				call_hw_op(task->config->path[0]->mmlsys,
					pw_enable);
				/* dpc exception flow on */
				mml_msg_dpc("%s dpc exception flow on", __func__);
				mml_dpc_exc_keep(task->config->mml);
				call_hw_op(comp, clk_enable);
				mml_clock_unlock(task->config->mml);
				mml_lock_wake_lock(tdshp->mml, true);
				tdshp->dpc = task->config->dpc;
				tdshp->mmlsys_comp =
					task->config->path[0]->mmlsys;
			}

			queue_work(tdshp->tdshp_hist_wq, &tdshp->tdshp_hist_task);
		}
	}
	mml_pq_ir_log("%s end jobid[%d] pipe[%d] engine[%d]",
		__func__, task->job.jobid, ccfg->pipe, comp->id);

	return 0;
}


static s32 tdshp_config_frame(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	struct tdshp_frame_data *tdshp_frm = tdshp_frm_data(ccfg);
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];
	struct mml_comp_tdshp *tdshp = comp_to_tdshp(comp);
	u32 alpha = cfg->info.alpha ? 1 << 17 : 0;

	const phys_addr_t base_pa = comp->base_pa;
	struct mml_pq_comp_config_result *result;
	s32 ret;
	s32 i;
	struct mml_pq_reg *regs = NULL;
	s8 mode = task->config->info.mode;

	mml_pq_msg("%s pipe_id[%d] engine_id[%d] en_sharp[%d]", __func__,
		ccfg->pipe, comp->id, dest->pq_config.en_sharp);

	/* Enable 10-bit output by default
	 * cmdq_pkt_write(pkt, NULL, base_pa + tdshp->data->reg_table[TDSHP_CTRL], 0, 0x00000004);
	 */

	tdshp_config_region_pq(comp, pkt, base_pa, &dest->pq_config);

	if (!dest->pq_config.en_sharp && !dest->pq_config.en_dc) {
		/* relay mode */
		if (cfg->info.mode == MML_MODE_DDP_ADDON ||
			cfg->info.mode == MML_MODE_DIRECT_LINK) {
			/* enable to crop */
			tdshp_relay(comp, pkt, base_pa, alpha);
			cmdq_pkt_write(pkt, NULL,
				base_pa + tdshp->data->reg_table[TDSHP_00], 0, 1 << 31);
		} else {
			tdshp_relay(comp, pkt, base_pa, alpha | 0x1);
		}
		return 0;
	}

	tdshp_relay(comp, pkt, base_pa, alpha);

	do {
		ret = mml_pq_get_comp_config_result(task, TDSHP_WAIT_TIMEOUT_MS);
		if (ret) {
			mml_pq_comp_config_clear(task);
			tdshp_frm->config_success = false;
			if (dest->pq_config.en_region_pq) {
				// DON'T relay hardware, but disable sub-module
				cmdq_pkt_write(pkt, NULL,
					base_pa + tdshp->data->reg_table[TDSHP_00],
					1 << 31|1 << 28, 1 << 31|1 << 28);
				cmdq_pkt_write(pkt, NULL,
					base_pa + tdshp->data->reg_table[TDHSP_C_BOOST_MAIN],
					1 << 13, 1 << 13);
				// enable map input, disable map output
				cmdq_pkt_write(pkt, NULL,
					base_pa + tdshp->data->reg_table[TDSHP_REGION_PQ_PARAM],
					0x5C1B0, U32_MAX);
			} else
				tdshp_relay(comp, pkt, base_pa, alpha | 0x1);
			mml_pq_err("%s:get ds param timeout: %d in %dms", __func__,
				ret, TDSHP_WAIT_TIMEOUT_MS);
			ret = -ETIMEDOUT;
			goto exit;
		}

		result = get_tdshp_comp_config_result(task);
		if (!result) {
			tdshp_frm->config_success = false;
			if (dest->pq_config.en_region_pq) {
				// DON'T relay hardware, but disable sub-module
				cmdq_pkt_write(pkt, NULL,
					base_pa + tdshp->data->reg_table[TDSHP_00],
					1 << 31|1 << 28, 1 << 31|1 << 28);
				cmdq_pkt_write(pkt, NULL,
					base_pa + tdshp->data->reg_table[TDHSP_C_BOOST_MAIN],
					1 << 13, 1 << 13);
				// enable map input, disable map output
				cmdq_pkt_write(pkt, NULL,
					base_pa + tdshp->data->reg_table[TDSHP_REGION_PQ_PARAM],
					0x5C1B0, U32_MAX);
			} else
				tdshp_relay(comp, pkt, base_pa, alpha | 0x1);
			mml_pq_err("%s: not get result from user lib", __func__);
			ret = -EBUSY;
			goto exit;
		}
	} while ((mml_pq_debug_mode & MML_PQ_SET_TEST) && result->is_set_test);

	regs = result->ds_regs;

	/* TODO: use different regs */
	mml_pq_msg("%s:config ds regs, count: %d", __func__, result->ds_reg_cnt);
	tdshp_frm->config_success = true;
	for (i = 0; i < result->ds_reg_cnt; i++) {
		mml_write(pkt, base_pa + regs[i].offset, regs[i].value,
			regs[i].mask, reuse, cache,
			&tdshp_frm->labels[i]);

		mml_pq_msg("[ds][config][%x] = %#x mask(%#x)",
			regs[i].offset, regs[i].value, regs[i].mask);
	}

	tdshp_frm->is_clarity_need_readback = result->is_clarity_need_readback;
	tdshp_frm->is_dc_need_readback = result->is_dc_need_readback;

	if ((mode == MML_MODE_DDP_ADDON || mode == MML_MODE_DIRECT_LINK) &&
		((dest->pq_config.en_dre && dest->pq_config.en_sharp) ||
		dest->pq_config.en_dc))
		tdshp_hist_ctrl(comp, task, ccfg, result);

exit:
	return ret;
}

static s32 tdshp_config_tile(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;
	struct tdshp_frame_data *tdshp_frm = tdshp_frm_data(ccfg);
	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);
	struct mml_comp_tdshp *tdshp = comp_to_tdshp(comp);
	u16 tile_cnt = cfg->frame_tile[ccfg->pipe]->tile_cnt;
	const struct mml_crop *crop = &cfg->frame_in_crop[ccfg->node->out_idx];

	u32 tdshp_input_w = 0;
	u32 tdshp_input_h = 0;
	u32 tdshp_output_w = 0;
	u32 tdshp_output_h = 0;
	u32 tdshp_crop_x_offset = 0;
	u32 tdshp_crop_y_offset = 0;
	u32 tdshp_hist_left_start = 0, tdshp_hist_top_start = 0;
	u32 hist_win_x_start = 0, hist_win_x_end = 0;
	u32 hist_win_y_start = 0, hist_win_y_end = 0;
	u32 hist_first_tile = 0, hist_last_tile = 0;

	mml_pq_msg("%s idx[%d] engine_id[%d]", __func__, idx, comp->id);

	tdshp_input_w = tile->in.xe - tile->in.xs + 1;
	tdshp_input_h = tile->in.ye - tile->in.ys + 1;
	tdshp_output_w = tile->out.xe - tile->out.xs + 1;
	tdshp_output_h = tile->out.ye - tile->out.ys + 1;
	tdshp_crop_x_offset = tile->out.xs - tile->in.xs;
	tdshp_crop_y_offset = tile->out.ys - tile->in.ys;

	if (!idx) {
		if (task->config->dual)
			tdshp_frm->cut_pos_x = crop->r.width / 2;
		else
			tdshp_frm->cut_pos_x = crop->r.width;
		if (ccfg->pipe)
			tdshp_frm->out_hist_xs = tdshp_frm->cut_pos_x;
	}

	tdshp_frm->out_hist_ys = idx ? (tile->out.ye + 1) : 0;

	tdshp_hist_left_start =
		(tile->out.xs > tdshp_frm->out_hist_xs) ? tile->out.xs : tdshp_frm->out_hist_xs;
	tdshp_hist_top_start =
		(tile->out.ys > tdshp_frm->out_hist_ys) ? tile->out.ys  : tdshp_frm->out_hist_ys;

	hist_win_x_start = tdshp_hist_left_start - tile->in.xs;
	if (task->config->dual && !ccfg->pipe && (idx + 1 >= tile_cnt))
		hist_win_x_end = tdshp_frm->cut_pos_x - tile->in.xs - 1;
	else
		hist_win_x_end = tile->out.xe - tile->in.xs;

	hist_win_y_start = tdshp_hist_top_start - tile->in.ys;
	hist_win_y_end = tile->out.xe - tile->in.xs;


	if (!idx) {
		if (task->config->dual && ccfg->pipe)
			tdshp_frm->out_hist_xs = tile->in.xs + hist_win_x_end + 1;
		else
			tdshp_frm->out_hist_xs = tile->out.xe + 1;
		hist_first_tile = 1;
		hist_last_tile = (tile_cnt == 1) ? 1 : 0;
	} else if (idx + 1 >= tile_cnt) {
		tdshp_frm->out_hist_xs = 0;
		hist_first_tile = 0;
		hist_last_tile = 1;
	} else {
		if (task->config->dual && ccfg->pipe)
			tdshp_frm->out_hist_xs = tile->in.xs + hist_win_x_end + 1;
		else
			tdshp_frm->out_hist_xs = tile->out.xe + 1;
		hist_first_tile = 0;
		hist_last_tile = 0;
	}

	cmdq_pkt_write(pkt, NULL, base_pa + tdshp->data->reg_table[TDSHP_INPUT_SIZE],
		(tdshp_input_w << 16) + tdshp_input_h, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + tdshp->data->reg_table[TDSHP_OUTPUT_OFFSET],
		(tdshp_crop_x_offset << 16) + tdshp_crop_y_offset, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + tdshp->data->reg_table[TDSHP_OUTPUT_SIZE],
		(tdshp_output_w << 16) + tdshp_output_h, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + tdshp->data->reg_table[HIST_CFG_00],
		(hist_win_x_end << 16) | (hist_win_x_start << 0), U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + tdshp->data->reg_table[HIST_CFG_01],
		(hist_win_y_end << 16) | (hist_win_y_start << 0), U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + tdshp->data->reg_table[TDSHP_CFG],
		(hist_last_tile << 14) | (hist_first_tile << 15), (1 << 14) | (1 << 15));

	return 0;
}

static void tdshp_readback_cmdq(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct mml_comp_tdshp *tdshp = comp_to_tdshp(comp);
	struct tdshp_frame_data *tdshp_frm = tdshp_frm_data(ccfg);
	const phys_addr_t base_pa = comp->base_pa;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_frame_config *cfg = task->config;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];

	u8 pipe = ccfg->pipe;
	dma_addr_t pa = 0;
	struct cmdq_operand lop, rop;
	u32 i = 0;

	const u16 idx_val = CMDQ_THR_SPR_IDX2;
	const u16 idx_out = tdshp->data->cpr[ccfg->pipe];
	const u16 idx_out64 = CMDQ_CPR_TO_CPR64(idx_out);

	mml_pq_get_readback_buffer(task, pipe, &(task->pq_task->tdshp_hist[pipe]));

	if (unlikely(!task->pq_task->tdshp_hist[pipe])) {
		mml_pq_err("%s job_id[%d] engine_id[%d] tdshp_hist is null",
			__func__, task->job.jobid, comp->id);
		return;
	}

	pa = task->pq_task->tdshp_hist[pipe]->pa;

	/* readback to this pa */
	mml_assign(pkt, idx_out, (u32)pa,
		reuse, cache, &tdshp_frm->labels[TDSHP_POLLGPR_0]);
	mml_assign(pkt, idx_out + 1, (u32)(pa >> 32),
		reuse, cache, &tdshp_frm->labels[TDSHP_POLLGPR_1]);

	/* read contour histogram status */
	for (i = 0; i < TDSHP_CONTOUR_HIST_NUM; i++) {
		cmdq_pkt_read_addr(pkt, base_pa +
			tdshp->data->reg_table[CONTOUR_HIST_00] + i * 4, idx_val);
		cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

		lop.reg = true;
		lop.idx = idx_out;
		rop.reg = false;
		rop.value = 4;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);
	}

	/* read tdshp clarity status */
	if (tdshp->data->reg_table[TDSHP_STATUS_00] != REG_NOT_SUPPORT) {
		for (i = 0; i < TDSHP_CLARITY_STATUS_NUM; i++) {
			cmdq_pkt_read_addr(pkt, base_pa +
				tdshp->data->reg_table[TDSHP_STATUS_00] + i * 4, idx_val);
			cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

			lop.reg = true;
			lop.idx = idx_out;
			rop.reg = false;
			rop.value = 4;
			cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);
		}
	}


	mml_pq_rb_msg("%s end job_id[%d] engine_id[%d] va[%p] pa[%pad] pkt[%p]",
		__func__, task->job.jobid, comp->id, task->pq_task->tdshp_hist[pipe]->va,
		&task->pq_task->tdshp_hist[pipe]->pa, pkt);
}


static s32 tdshp_config_post(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	struct mml_frame_dest *dest = &task->config->info.dest[ccfg->node->out_idx];
	s8 mode = task->config->info.mode;

	/*Skip readback, need to add flow in IR//DL*/
	if (mode != MML_MODE_MML_DECOUPLE)
		goto put_comp_config;

	if ((dest->pq_config.en_sharp && dest->pq_config.en_dre) ||
		dest->pq_config.en_dc)
		tdshp_readback_cmdq(comp, task, ccfg);

put_comp_config:
	if (dest->pq_config.en_sharp || dest->pq_config.en_dc)
		mml_pq_put_comp_config_result(task);
	return 0;
}

static s32 tdshp_reconfig_frame(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;

	struct tdshp_frame_data *tdshp_frm = tdshp_frm_data(ccfg);
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];

	struct mml_pq_comp_config_result *result = NULL;
	s32 ret = 0;
	s32 i;
	struct mml_pq_reg *regs = NULL;
	s8 mode = task->config->info.mode;

	if (!dest->pq_config.en_sharp && !dest->pq_config.en_dc)
		return ret;

	do {
		ret = mml_pq_get_comp_config_result(task, TDSHP_WAIT_TIMEOUT_MS);
		if (ret) {
			mml_pq_comp_config_clear(task);
			mml_pq_err("get tdshp param timeout: %d in %dms",
				ret, TDSHP_WAIT_TIMEOUT_MS);
			ret = -ETIMEDOUT;
			goto exit;
		}

		result = get_tdshp_comp_config_result(task);
		if (!result || !tdshp_frm->config_success || !result->ds_reg_cnt) {
			mml_pq_err("%s: not get result from user lib", __func__);
			ret = -EBUSY;
			goto exit;
		}
	} while ((mml_pq_debug_mode & MML_PQ_SET_TEST) && result->is_set_test);

	regs = result->ds_regs;
	/* TODO: use different regs */
	mml_pq_msg("%s:config ds regs, count: %d is_set_test[%d]", __func__, result->ds_reg_cnt,
		result->is_set_test);
	for (i = 0; i < result->ds_reg_cnt; i++) {
		mml_update(reuse, tdshp_frm->labels[i], regs[i].value);
		mml_pq_msg("[ds][config][%x] = %#x mask(%#x)",
			regs[i].offset, regs[i].value, regs[i].mask);
	}
	tdshp_frm->is_clarity_need_readback = result->is_clarity_need_readback;
	tdshp_frm->is_dc_need_readback = result->is_dc_need_readback;

	if ((mode == MML_MODE_DDP_ADDON || mode == MML_MODE_DIRECT_LINK) &&
		((dest->pq_config.en_dre && dest->pq_config.en_sharp) ||
		dest->pq_config.en_dc))
		tdshp_hist_ctrl(comp, task, ccfg, result);
exit:
	return ret;
}

static s32 tdshp_config_repost(struct mml_comp *comp, struct mml_task *task,
			       struct mml_comp_config *ccfg)
{
	struct mml_frame_dest *dest = &task->config->info.dest[ccfg->node->out_idx];
	struct tdshp_frame_data *tdshp_frm = tdshp_frm_data(ccfg);
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	u8 pipe = ccfg->pipe;
	s8 mode = task->config->info.mode;

	if (mode != MML_MODE_MML_DECOUPLE)
		goto put_comp_config;

	if ((dest->pq_config.en_sharp && dest->pq_config.en_dre) ||
		dest->pq_config.en_dc) {

		mml_pq_get_readback_buffer(task, pipe, &(task->pq_task->tdshp_hist[pipe]));

		if (unlikely(!task->pq_task->tdshp_hist[pipe])) {
			mml_pq_err("%s job_id[%d] tdshp_hist is null", __func__,
				task->job.jobid);
			goto put_comp_config;
		}

		mml_update(reuse, tdshp_frm->labels[TDSHP_POLLGPR_0],
			(u32)task->pq_task->tdshp_hist[pipe]->pa);
		mml_update(reuse, tdshp_frm->labels[TDSHP_POLLGPR_1],
			(u32)(task->pq_task->tdshp_hist[pipe]->pa >> 32));
	}

put_comp_config:
	if (dest->pq_config.en_sharp || dest->pq_config.en_dc)
		mml_pq_put_comp_config_result(task);
	return 0;
}

static const struct mml_comp_config_ops tdshp_cfg_ops = {
	.prepare = tdshp_prepare,
	.buf_prepare = tdshp_buf_prepare,
	.get_label_count = tdshp_get_label_count,
	.init = tdshp_config_init,
	.frame = tdshp_config_frame,
	.tile = tdshp_config_tile,
	.post = tdshp_config_post,
	.reframe = tdshp_reconfig_frame,
	.repost = tdshp_config_repost,
};

static void tdshp_task_done_readback(struct mml_comp *comp, struct mml_task *task,
					 struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct tdshp_frame_data *tdshp_frm = tdshp_frm_data(ccfg);
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_comp_tdshp *tdshp = comp_to_tdshp(comp);
	u8 pipe = ccfg->pipe;
	u32 offset = 0, i = 0;

	mml_pq_trace_ex_begin("%s comp[%d]", __func__, comp->id);
	mml_pq_msg("%s clarity_readback[%d] id[%d] en_sharp[%d] tdshp_hist[%lx]", __func__,
			tdshp_frm->is_clarity_need_readback, comp->id,
			dest->pq_config.en_sharp, (unsigned long)&(task->pq_task->tdshp_hist[pipe]));

	if (((!dest->pq_config.en_sharp || !dest->pq_config.en_dre) &&
		!dest->pq_config.en_dc) || !task->pq_task->tdshp_hist[pipe])
		goto exit;

	mml_pq_rb_msg("%s job_id[%d] id[%d] pipe[%d] en_dc[%d] va[%p] pa[%llx] offset[%d]",
		__func__, task->job.jobid, comp->id, ccfg->pipe,
		dest->pq_config.en_dc, task->pq_task->tdshp_hist[pipe]->va,
		task->pq_task->tdshp_hist[pipe]->pa,
		task->pq_task->tdshp_hist[pipe]->va_offset);


	mml_pq_rb_msg("%s job_id[%d] hist[0~4]={%08x, %08x, %08x, %08x, %08x}",
		__func__, task->job.jobid,
		task->pq_task->tdshp_hist[pipe]->va[offset/4+0],
		task->pq_task->tdshp_hist[pipe]->va[offset/4+1],
		task->pq_task->tdshp_hist[pipe]->va[offset/4+2],
		task->pq_task->tdshp_hist[pipe]->va[offset/4+3],
		task->pq_task->tdshp_hist[pipe]->va[offset/4+4]);

	mml_pq_rb_msg("%s job_id[%d] hist[10~14]={%08x, %08x, %08x, %08x, %08x}",
		__func__, task->job.jobid,
		task->pq_task->tdshp_hist[pipe]->va[offset/4+10],
		task->pq_task->tdshp_hist[pipe]->va[offset/4+11],
		task->pq_task->tdshp_hist[pipe]->va[offset/4+12],
		task->pq_task->tdshp_hist[pipe]->va[offset/4+13],
		task->pq_task->tdshp_hist[pipe]->va[offset/4+14]);

	mml_pq_rb_msg("%s job_id[%d]",
		__func__, task->job.jobid);

	/*remain code for ping-pong in the feature*/
	if (((!dest->pq_config.en_sharp || !dest->pq_config.en_dre) &&
		!dest->pq_config.en_dc)) {
		void __iomem *base = comp->base;
		s32 i;
		u32 *phist = kmalloc((TDSHP_CONTOUR_HIST_NUM+
			TDSHP_CLARITY_STATUS_NUM)*sizeof(u32), GFP_KERNEL);

		for (i = 0; i < TDSHP_CONTOUR_HIST_NUM; i++)
			phist[i] = readl(base + tdshp->data->reg_table[CONTOUR_HIST_00]);

		for (i = 0; i < TDSHP_CLARITY_STATUS_NUM; i++)
			phist[i] = readl(base + tdshp->data->reg_table[CONTOUR_HIST_00]);

		if (tdshp_frm->is_dc_need_readback)
			mml_pq_dc_readback(task, ccfg->pipe, &phist[0]);
		if (tdshp_frm->is_clarity_need_readback)
			mml_pq_clarity_readback(task, ccfg->pipe,
				&phist[TDSHP_CONTOUR_HIST_NUM],
				TDSHP_CLARITY_HIST_START,
				TDSHP_CLARITY_STATUS_NUM);

	}

	if (tdshp_frm->is_dc_need_readback) {
		if (mml_pq_debug_mode & MML_PQ_HIST_CHECK) {
			for (i = 0; i < TDSHP_CONTOUR_HIST_NUM; i++)
				if (task->pq_task->tdshp_hist[pipe]->va[offset/4+i] >
					VALID_CONTOUR_HIST_VALUE)
					mml_pq_util_aee("MML_PQ_TDSHP Histogram Error",
						"CONTOUR Histogram error need to check jobid:%d",
						task->job.jobid);
		}
		mml_pq_dc_readback(task, ccfg->pipe,
			&(task->pq_task->tdshp_hist[pipe]->va[offset/4+0]));
	}

	if (tdshp_frm->is_clarity_need_readback) {

		mml_pq_rb_msg("%s job_id[%d] clarity_hist[0~4]={%08x, %08x, %08x, %08x, %08x}",
			__func__, task->job.jobid,
			task->pq_task->tdshp_hist[pipe]->va[offset/4+TDSHP_CONTOUR_HIST_NUM+0],
			task->pq_task->tdshp_hist[pipe]->va[offset/4+TDSHP_CONTOUR_HIST_NUM+1],
			task->pq_task->tdshp_hist[pipe]->va[offset/4+TDSHP_CONTOUR_HIST_NUM+2],
			task->pq_task->tdshp_hist[pipe]->va[offset/4+TDSHP_CONTOUR_HIST_NUM+3],
			task->pq_task->tdshp_hist[pipe]->va[offset/4+TDSHP_CONTOUR_HIST_NUM+4]);

		mml_pq_clarity_readback(task, ccfg->pipe,
			&(task->pq_task->tdshp_hist[pipe]->va[
			offset/4+TDSHP_CONTOUR_HIST_NUM]),
			TDSHP_CLARITY_HIST_START, TDSHP_CLARITY_STATUS_NUM);
	}

	mml_pq_put_readback_buffer(task, pipe, &(task->pq_task->tdshp_hist[pipe]));
exit:
	mml_pq_trace_ex_end();
}


static const struct mml_comp_hw_ops tdshp_hw_ops = {
	.clk_enable = &mml_comp_clk_enable,
	.clk_disable = &mml_comp_clk_disable,
	.task_done = tdshp_task_done_readback,
};

static u32 read_reg_value(struct mml_comp *comp, u16 reg)
{
	void __iomem *base = comp->base;

	if (reg == REG_NOT_SUPPORT) {
		mml_err("%s tdshp reg is not support", __func__);
		return 0xFFFFFFFF;
	}

	return readl(base + reg);
}

static void tdshp_debug_dump(struct mml_comp *comp)
{
	struct mml_comp_tdshp *tdshp = comp_to_tdshp(comp);
	void __iomem *base = comp->base;
	u32 value[12];
	u32 shadow_ctrl;

	mml_err("tdshp component %u dump:", comp->id);

	/* Enable shadow read working */
	shadow_ctrl = read_reg_value(comp, tdshp->data->reg_table[TDSHP_SHADOW_CTRL]);
	shadow_ctrl |= 0x4;
	writel(shadow_ctrl, base + tdshp->data->reg_table[TDSHP_SHADOW_CTRL]);

	value[0] = read_reg_value(comp, tdshp->data->reg_table[TDSHP_CTRL]);
	value[1] = read_reg_value(comp, tdshp->data->reg_table[TDSHP_INTEN]);
	value[2] = read_reg_value(comp, tdshp->data->reg_table[TDSHP_INTSTA]);
	value[3] = read_reg_value(comp, tdshp->data->reg_table[TDSHP_STATUS]);
	value[4] = read_reg_value(comp, tdshp->data->reg_table[TDSHP_CFG]);
	value[5] = read_reg_value(comp, tdshp->data->reg_table[TDSHP_INPUT_COUNT]);
	value[6] = read_reg_value(comp, tdshp->data->reg_table[TDSHP_OUTPUT_COUNT]);
	value[7] = read_reg_value(comp, tdshp->data->reg_table[TDSHP_INPUT_SIZE]);
	value[8] = read_reg_value(comp, tdshp->data->reg_table[TDSHP_OUTPUT_OFFSET]);
	value[9] = read_reg_value(comp, tdshp->data->reg_table[TDSHP_OUTPUT_SIZE]);
	value[10] = read_reg_value(comp, tdshp->data->reg_table[TDSHP_BLANK_WIDTH]);

	mml_err("TDSHP_CTRL %#010x TDSHP_INTEN %#010x TDSHP_INTSTA %#010x TDSHP_STATUS %#010x",
		value[0], value[1], value[2], value[3]);
	mml_err("TDSHP_CFG %#010x TDSHP_INPUT_COUNT %#010x TDSHP_OUTPUT_COUNT %#010x",
		value[4], value[5], value[6]);
	mml_err("TDSHP_INPUT_SIZE %#010x TDSHP_OUTPUT_OFFSET %#010x TDSHP_OUTPUT_SIZE %#010x",
		value[7], value[8], value[9]);
	mml_err("TDSHP_BLANK_WIDTH %#010x", value[10]);

	if (tdshp->data->reg_table[TDSHP_REGION_PQ_PARAM] != REG_NOT_SUPPORT) {
		value[11] = readl(base + tdshp->data->reg_table[TDSHP_REGION_PQ_PARAM]);
		mml_err("TDSHP_REGION_PQ_PARAM %#010x", value[11]);
	}
}

static const struct mml_comp_debug_ops tdshp_debug_ops = {
	.dump = &tdshp_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_tdshp *tdshp = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	s32 ret;

	if (!drm_dev) {
		ret = mml_register_comp(master, &tdshp->comp);
		if (ret)
			dev_err(dev, "Failed to register mml component %s: %d\n",
				dev->of_node->full_name, ret);
		tdshp->mml = dev_get_drvdata(master);
	} else {
		ret = mml_ddp_comp_register(drm_dev, &tdshp->ddp_comp);
		if (ret)
			dev_err(dev, "Failed to register ddp component %s: %d\n",
				dev->of_node->full_name, ret);
		else
			tdshp->ddp_bound = true;
	}
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_tdshp *tdshp = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	if (!drm_dev) {
		mml_unregister_comp(master, &tdshp->comp);
	} else {
		mml_ddp_comp_unregister(drm_dev, &tdshp->ddp_comp);
		tdshp->ddp_bound = false;
	}
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static const struct mtk_ddp_comp_funcs ddp_comp_funcs = {
};

static struct mml_comp_tdshp *dbg_probed_components[4];
static int dbg_probed_count;

static void tdshp_histdone_cb(struct cmdq_cb_data data)
{
	struct cmdq_pkt *pkt = (struct cmdq_pkt *)data.data;
	struct mml_comp_tdshp *tdshp = (struct mml_comp_tdshp *)pkt->user_data;
	struct mml_comp *comp = &tdshp->comp;
	u32 pipe;

	mml_pq_ir_log("%s jobid[%d] tdshp->hist_pkts[0] = [%p] hdr->hist_pkts[1] = [%p]",
		__func__, tdshp->jobid, tdshp->hist_pkts[0], tdshp->hist_pkts[1]);

	if (mml_pq_tdshp_hist_reading(tdshp->pq_task, tdshp->out_idx, tdshp->pipe))
		return;

	if (pkt == tdshp->hist_pkts[0]) {
		pipe = 0;
	} else if (pkt == tdshp->hist_pkts[1]) {
		pipe = 1;
	} else {
		mml_err("%s task %p pkt %p not match both pipe (%p and %p)",
			__func__, tdshp, pkt, tdshp->hist_pkts[0],
			tdshp->hist_pkts[1]);
		return;
	}

	mml_pq_ir_log("%s hist[0~4]={%08x, %08x, %08x, %08x, %08x}",
		__func__,
		tdshp->tdshp_hist[pipe]->va[0],
		tdshp->tdshp_hist[pipe]->va[1],
		tdshp->tdshp_hist[pipe]->va[2],
		tdshp->tdshp_hist[pipe]->va[3],
		tdshp->tdshp_hist[pipe]->va[4]);

	mml_pq_ir_log("%s hist[5~9]={%08x, %08x, %08x, %08x, %08x}",
		__func__,
		tdshp->tdshp_hist[pipe]->va[5],
		tdshp->tdshp_hist[pipe]->va[6],
		tdshp->tdshp_hist[pipe]->va[7],
		tdshp->tdshp_hist[pipe]->va[8],
		tdshp->tdshp_hist[pipe]->va[9]);

	mml_pq_ir_log("%s hist[10~14]={%08x, %08x, %08x, %08x, %08x}",
		__func__,
		tdshp->tdshp_hist[pipe]->va[10],
		tdshp->tdshp_hist[pipe]->va[11],
		tdshp->tdshp_hist[pipe]->va[12],
		tdshp->tdshp_hist[pipe]->va[13],
		tdshp->tdshp_hist[pipe]->va[14]);

	mml_pq_ir_log("%s hist[15~19]={%08x, %08x, %08x, %08x, %08x}",
		__func__,
		tdshp->tdshp_hist[pipe]->va[15],
		tdshp->tdshp_hist[pipe]->va[16],
		tdshp->tdshp_hist[pipe]->va[17],
		tdshp->tdshp_hist[pipe]->va[18],
		tdshp->tdshp_hist[pipe]->va[19]);

	mml_pq_ir_log("%s hist[20~24]={%08x, %08x, %08x, %08x, %08x}",
		__func__,
		tdshp->tdshp_hist[pipe]->va[20],
		tdshp->tdshp_hist[pipe]->va[21],
		tdshp->tdshp_hist[pipe]->va[22],
		tdshp->tdshp_hist[pipe]->va[23],
		tdshp->tdshp_hist[pipe]->va[24]);

	mml_pq_ir_log("%s hist[25~29]={%08x, %08x, %08x, %08x, %08x}",
		__func__,
		tdshp->tdshp_hist[pipe]->va[25],
		tdshp->tdshp_hist[pipe]->va[26],
		tdshp->tdshp_hist[pipe]->va[27],
		tdshp->tdshp_hist[pipe]->va[28],
		tdshp->tdshp_hist[pipe]->va[29]);

	if (tdshp->dc_readback)
		mml_pq_ir_dc_readback(tdshp->pq_task, tdshp->frame_data,
			tdshp->pipe, &(tdshp->tdshp_hist[pipe]->va[0]),
			tdshp->jobid, 0, tdshp->dual);

	if (tdshp->clarity_readback)
		mml_pq_ir_clarity_readback(tdshp->pq_task, tdshp->frame_data,
			tdshp->pipe, &(tdshp->tdshp_hist[pipe]->va[TDSHP_CONTOUR_HIST_NUM]),
			tdshp->jobid, TDSHP_CLARITY_STATUS_NUM, TDSHP_CLARITY_HIST_START,
			tdshp->dual);

	if (tdshp->data->rb_mode == RB_EOF_MODE) {
		mml_clock_lock(tdshp->mml);
		call_hw_op(comp, clk_disable, tdshp->dpc);
		/* dpc exception flow off */
		mml_msg_dpc("%s dpc exception flow off", __func__);
		mml_dpc_exc_release(tdshp->mml);
		/* ccf power off */
		call_hw_op(tdshp->mmlsys_comp, pw_disable);
		call_hw_op(tdshp->mmlsys_comp, mminfra_pw_enable);
		mml_clock_unlock(tdshp->mml);
		mml_lock_wake_lock(tdshp->mml, false);
	}

	mml_pq_put_pq_task(tdshp->pq_task);

	mml_pq_tdshp_flag_check(tdshp->dual, tdshp->out_idx);

	mml_pq_ir_log("%s end jobid[%d] pkt[%p] hdr[%p] pipe[%d]",
		__func__, tdshp->jobid, pkt, tdshp, pipe);
	mml_trace_end();
}

static void tdshp_err_dump_cb(struct cmdq_cb_data data)
{
	struct mml_comp_tdshp *tdshp = (struct mml_comp_tdshp *)data.data;

	if (!tdshp) {
		mml_pq_err("%s tdshp is null", __func__);
		return;
	}

	mml_pq_ir_log("%s jobid[%d] tdshp->hist_pkts[0] = [%p] hdr->hist_pkts[1] = [%p]",
		__func__, tdshp->jobid, tdshp->hist_pkts[0], tdshp->hist_pkts[1]);

}


static void tdshp_hist_work(struct work_struct *work_item)
{
	struct mml_comp_tdshp *tdshp = NULL;
	struct mml_comp *comp = NULL;
	struct cmdq_pkt *pkt = NULL;
	struct cmdq_operand lop, rop;
	struct mml_pq_task *pq_task = NULL;

	const u16 idx_val = CMDQ_THR_SPR_IDX2;
	u16 idx_out = 0;
	u16 idx_out64 = 0;

	u8 pipe = 0;
	phys_addr_t base_pa = 0;
	dma_addr_t pa = 0;
	u32 i = 0;

	tdshp = container_of(work_item, struct mml_comp_tdshp, tdshp_hist_task);

	if (!tdshp) {
		mml_pq_err("%s comp_tdshp is null", __func__);
		return;
	}

	pipe = tdshp->pipe;
	comp = &tdshp->comp;
	base_pa = comp->base_pa;
	pkt = tdshp->hist_pkts[pipe];
	idx_out = tdshp->data->cpr[tdshp->pipe];
	pq_task = tdshp->pq_task;


	mml_pq_ir_log("%s job_id[%d] eng_id[%d] cmd_buf_size[%zu] hist_cmd_done[%d]",
		__func__, tdshp->jobid, comp->id,
		pkt->cmd_buf_size, tdshp->hist_cmd_done);

	idx_out64 = CMDQ_CPR_TO_CPR64(idx_out);

	if (unlikely(!tdshp->tdshp_hist[pipe])) {
		mml_pq_err("%s job_id[%d] eng_id[%d] pipe[%d] pkt[%p] tdshp_hist is null",
			__func__, tdshp->jobid, comp->id, pipe,
			tdshp->hist_pkts[pipe]);
		return;
	}

	mutex_lock(&tdshp->hist_cmd_lock);
	if (tdshp->hist_cmd_done) {
		mutex_unlock(&tdshp->hist_cmd_lock);
		goto tdshp_hist_cmd_done;
	}

	cmdq_pkt_wfe(pkt, tdshp->event_eof);

	pa = tdshp->tdshp_hist[pipe]->pa;

	/* readback to this pa */
	cmdq_pkt_assign_command(pkt, idx_out, (u32)pa);
	cmdq_pkt_assign_command(pkt, idx_out + 1, (u32)(pa >> 32));

	/* read contour histogram status */
	for (i = 0; i < TDSHP_CONTOUR_HIST_NUM; i++) {
		cmdq_pkt_read_addr(pkt, base_pa +
			tdshp->data->reg_table[CONTOUR_HIST_00] + i * 4, idx_val);
		cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

		lop.reg = true;
		lop.idx = idx_out;
		rop.reg = false;
		rop.value = 4;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);
	}

	/* read tdshp clarity status */
	if (tdshp->data->reg_table[TDSHP_STATUS_00] != REG_NOT_SUPPORT) {
		for (i = 0; i < TDSHP_CLARITY_STATUS_NUM; i++) {
			cmdq_pkt_read_addr(pkt, base_pa +
				tdshp->data->reg_table[TDSHP_STATUS_00] + i * 4, idx_val);
			cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

			lop.reg = true;
			lop.idx = idx_out;
			rop.reg = false;
			rop.value = 4;
			cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);
		}
	}

	mml_pq_rb_msg("%s end job_id[%d] engine_id[%d] va[%p] pa[%llx] pkt[%p]",
		__func__, tdshp->jobid, comp->id, tdshp->tdshp_hist[pipe]->va,
		tdshp->tdshp_hist[pipe]->pa, pkt);

	tdshp->hist_cmd_done = true;

	mml_pq_rb_msg("%s end engine_id[%d] va[%p] pa[%llx] pkt[%p]",
		__func__, comp->id, tdshp->tdshp_hist[pipe]->va,
		tdshp->tdshp_hist[pipe]->pa, pkt);

	pkt->user_data = tdshp;
	pkt->err_cb.cb = tdshp_err_dump_cb;
	pkt->err_cb.data = (void *)tdshp;

	mutex_unlock(&tdshp->hist_cmd_lock);

tdshp_hist_cmd_done:
	if (tdshp->pq_config.en_hdr)
		wait_for_completion(&tdshp->pq_task->hdr_curve_ready[tdshp->pipe]);

	cmdq_pkt_refinalize(pkt);
	cmdq_pkt_flush_threaded(pkt, tdshp_histdone_cb, (void *)tdshp->hist_pkts[pipe]);

	mml_pq_ir_log("%s end job_id[%d] pkts[%p %p] id[%d] size[%zu] cmd_done[%d] irq[%d]",
		__func__, tdshp->jobid,
		tdshp->hist_pkts[0], tdshp->hist_pkts[1], comp->id,
		pkt->cmd_buf_size, tdshp->hist_cmd_done,
		pkt->no_irq);

}


static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_tdshp *priv;
	s32 ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = of_device_get_match_data(dev);

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}

	if (of_property_read_u16(dev->of_node, "event-frame-done",
				 &priv->event_eof))
		dev_err(dev, "read event frame_done fail\n");

	priv->tdshp_hist_wq = create_singlethread_workqueue("tdshp_hist_read");
	INIT_WORK(&priv->tdshp_hist_task, tdshp_hist_work);

	mutex_init(&priv->hist_cmd_lock);

	/* assign ops */
	priv->comp.tile_ops = &tdshp_tile_ops;
	priv->comp.config_ops = &tdshp_cfg_ops;
	priv->comp.hw_ops = &tdshp_hw_ops;
	priv->comp.debug_ops = &tdshp_debug_ops;

	dbg_probed_components[dbg_probed_count++] = priv;

	ret = component_add(dev, &mml_comp_ops);
	if (ret)
		dev_err(dev, "Failed to add component: %d\n", ret);

	return ret;
}

static int remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mml_comp_ops);
	component_del(&pdev->dev, &mml_comp_ops);
	return 0;
}

const struct of_device_id mml_tdshp_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6983-mml_tdshp",
		.data = &mt6983_tdshp_data,
	},
	{
		.compatible = "mediatek,mt6893-mml_tdshp",
		.data = &mt6893_tdshp_data
	},
	{
		.compatible = "mediatek,mt6879-mml_tdshp",
		.data = &mt6879_tdshp_data
	},
	{
		.compatible = "mediatek,mt6895-mml_tdshp",
		.data = &mt6895_tdshp_data
	},
	{
		.compatible = "mediatek,mt6985-mml_tdshp",
		.data = &mt6985_tdshp_data,
	},
	{
		.compatible = "mediatek,mt6886-mml_tdshp",
		.data = &mt6886_tdshp_data
	},
	{
		.compatible = "mediatek,mt6897-mml_tdshp",
		.data = &mt6985_tdshp_data
	},
	{
		.compatible = "mediatek,mt6989-mml_tdshp",
		.data = &mt6989_tdshp_data,
	},
	{
		.compatible = "mediatek,mt6878-mml_tdshp",
		.data = &mt6878_tdshp_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_tdshp_driver_dt_match);

struct platform_driver mml_tdshp_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-tdshp",
		.owner = THIS_MODULE,
		.of_match_table = mml_tdshp_driver_dt_match,
	},
};

//module_platform_driver(mml_tdshp_driver);

static s32 dbg_case;
static s32 dbg_set(const char *val, const struct kernel_param *kp)
{
	s32 result;

	result = kstrtos32(val, 0, &dbg_case);
	mml_log("%s: debug_case=%d", __func__, dbg_case);

	switch (dbg_case) {
	case 0:
		mml_log("use read to dump component status");
		break;
	default:
		mml_err("invalid debug_case: %d", dbg_case);
		break;
	}
	return result;
}

static s32 dbg_get(char *buf, const struct kernel_param *kp)
{
	s32 length = 0;
	u32 i;

	switch (dbg_case) {
	case 0:
		length += snprintf(buf + length, PAGE_SIZE - length,
			"[%d] probed count: %d\n", dbg_case, dbg_probed_count);
		for (i = 0; i < dbg_probed_count; i++) {
			struct mml_comp *comp = &dbg_probed_components[i]->comp;

			length += snprintf(buf + length, PAGE_SIZE - length,
				"  - [%d] mml comp_id: %d.%d @%pa name: %s bound: %d\n", i,
				comp->id, comp->sub_idx, &comp->base_pa,
				comp->name ? comp->name : "(null)", comp->bound);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -         larb_port: %d @%pa pw: %d clk: %d\n",
				comp->larb_port, &comp->larb_base,
				comp->pw_cnt, comp->clk_cnt);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -     ddp comp_id: %d bound: %d\n",
				dbg_probed_components[i]->ddp_comp.id,
				dbg_probed_components[i]->ddp_bound);
		}
		break;
	default:
		mml_err("not support read for debug_case: %d", dbg_case);
		break;
	}
	buf[length] = '\0';

	return length;
}

static const struct kernel_param_ops dbg_param_ops = {
	.set = dbg_set,
	.get = dbg_get,
};
module_param_cb(tdshp_debug, &dbg_param_ops, NULL, 0644);
MODULE_PARM_DESC(tdshp_debug, "mml tdshp debug case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML TDSHP driver");
MODULE_LICENSE("GPL v2");
