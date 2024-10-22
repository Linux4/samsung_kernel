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

#include "mtk-mml-driver.h"
#include "tile_driver.h"
#include "mtk-mml-tile.h"
#include "tile_mdp_func.h"
#include <soc/mediatek/smi.h>

#undef pr_fmt
#define pr_fmt(fmt) "[mml_pq_hdr]" fmt

#define HDR_WAIT_TIMEOUT_MS (50)
#define HDR_REG_NUM (70)
#define HDR_HIST_CNT (57)
/* count of reserved cross page anchor label count
 * min count would be
 *	((HDR_CURVE_NUM * 2 + HDR_REG_NUM * 3) / CMDQ_NUM_CMD(CMDQ_CMD_BUFFER_SIZE) + 3)
 * but labels may be divid due to write w/ mask between write w/o mask
 */
#define HDR_LABEL_CNT_REG	6
#define HDR_LABEL_CNT_CURVE	7
#define HDR_LABEL_CNT		(HDR_LABEL_CNT_REG + HDR_LABEL_CNT_CURVE)
#define call_hw_op(_comp, op, ...) \
	(_comp->hw_ops->op ? _comp->hw_ops->op(_comp, ##__VA_ARGS__) : 0)
#define REG_NOT_SUPPORT 0xfff

enum mml_hdr_reg_index {
	HDR_TOP,
	HDR_RELAY,
	HDR_INTSTA,
	HDR_ENGSTA,
	HDR_SIZE_0,
	HDR_SIZE_1,
	HDR_SIZE_2,
	HDR_HIST_CTRL_0,
	HDR_HIST_CTRL_1,
	HDR_HIST_CTRL_2,
	HDR_HIST_ADDR,
	HDR_HIST_DATA,
	HDR_GAIN_TABLE_0,
	HDR_GAIN_TABLE_1,
	HDR_GAIN_TABLE_2,
	HDR_LBOX_DET_4,
	HDR_CURSOR_CTRL,
	HDR_CURSOR_POS,
	HDR_CURSOR_COLOR,
	HDR_TILE_POS,
	HDR_CURSOR_BUF0,
	HDR_CURSOR_BUF1,
	HDR_CURSOR_BUF2,
	HDR_TONE_MAP_TOP,
	HDR_3x3_COEF_00,
	HDR_B_CHANNEL_NR,
	HDR_A_LUMINANCE,
	HDR_REG_MAX_COUNT
};

static const u16 hdr_reg_table_mt6983[HDR_REG_MAX_COUNT] = {
	[HDR_TOP] = 0x000,
	[HDR_RELAY] = 0x004,
	[HDR_INTSTA] = 0x00c,
	[HDR_ENGSTA] = 0x010,
	[HDR_SIZE_0] = 0x014,
	[HDR_SIZE_1] = 0x018,
	[HDR_SIZE_2] = 0x01c,
	[HDR_HIST_CTRL_0] = 0x020,
	[HDR_HIST_CTRL_1] = 0x024,
	[HDR_HIST_CTRL_2] = 0x028,
	[HDR_3x3_COEF_00] = 0x038,
	[HDR_B_CHANNEL_NR] = 0x0D8,
	[HDR_HIST_ADDR] = 0x0dc,
	[HDR_HIST_DATA] = 0x0e0,
	[HDR_A_LUMINANCE] = 0x0e4,
	[HDR_GAIN_TABLE_0] = 0x0e8,
	[HDR_GAIN_TABLE_1] = 0x0ec,
	[HDR_GAIN_TABLE_2] = 0x0f0,
	[HDR_LBOX_DET_4] = 0x104,
	[HDR_CURSOR_CTRL] = 0x10c,
	[HDR_CURSOR_POS] = 0x110,
	[HDR_CURSOR_COLOR] = 0x114,
	[HDR_TILE_POS] = 0x118,
	[HDR_CURSOR_BUF0] = 0x11c,
	[HDR_CURSOR_BUF1] = 0x120,
	[HDR_CURSOR_BUF2] = 0x124,
	[HDR_TONE_MAP_TOP] = 0x1b0,
};

struct hdr_data {
	u32 min_tile_width;
	u16 cpr[MML_PIPE_CNT];
	u16 gpr[MML_PIPE_CNT];
	const u16 *reg_table;
	u8 tile_loss;
	bool vcp_readback;
	u8 rb_mode;
};

static const struct hdr_data mt6983_hdr_data = {
	.min_tile_width = 16,
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.vcp_readback = false,
	.reg_table = hdr_reg_table_mt6983,
	.tile_loss = 8,
	.rb_mode = RB_EOF_MODE,
};

static const struct hdr_data mt6895_hdr_data = {
	.min_tile_width = 16,
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.vcp_readback = true,
	.reg_table = hdr_reg_table_mt6983,
	.tile_loss = 8,
	.rb_mode = RB_EOF_MODE,
};

static const struct hdr_data mt6985_hdr_data = {
	.min_tile_width = 16,
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.vcp_readback = false,
	.reg_table = hdr_reg_table_mt6983,
	.rb_mode = RB_EOF_MODE,
};

struct mml_comp_hdr {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	struct mml_dev *mml;
	const struct hdr_data *data;
	bool ddp_bound;
	u8 pipe;
	u8 out_idx;
	u32 *gain_curve;
	u16 event_eof;
	u16 mutex_hint;
	u32 jobid;
	u32 cut_pos_x;
	u32 crop_width;
	u32 crop_height;
	struct mml_pq_task *pq_task;
	struct mml_pq_task *curve_pq_task;
	bool dual;
	bool dpc;
	struct mml_comp *mmlsys_comp;
	bool hist_cmd_done;
	struct mutex hist_cmd_lock;
	struct mml_pq_frame_data frame_data;
	struct mml_pq_readback_buffer *hdr_hist[MML_PIPE_CNT];
	struct cmdq_client *clts;
	struct cmdq_pkt *hist_pkts[MML_PIPE_CNT];
	struct workqueue_struct *hdr_curve_wq;
	struct work_struct hdr_curve_task;
	struct workqueue_struct *hdr_hist_wq;
	struct work_struct hdr_hist_task;
};

enum hdr_label_index {
	HDR_REUSE_LABEL = 0,
	HDR_POLLGPR_0 = HDR_LABEL_CNT,
	HDR_POLLGPR_1,
	HDR_LABEL_TOTAL
};

/* meta data for each different frame config */
struct hdr_frame_data {
	u32 out_hist_xs;
	u32 cut_pos_x;
	u32 begin_offset;
	u32 condi_offset;
	struct cmdq_poll_reuse polling_reuse;
	u16 labels[HDR_LABEL_TOTAL];
	struct mml_reuse_array reuse_reg;
	struct mml_reuse_array reuse_curve;
	struct mml_reuse_offset offs_reg[HDR_LABEL_CNT_REG];
	struct mml_reuse_offset offs_curve[HDR_LABEL_CNT_CURVE];
	bool is_hdr_need_readback;
	bool config_success;
};

static inline struct hdr_frame_data *hdr_frm_data(struct mml_comp_config *ccfg)
{
	return ccfg->data;
}

static inline struct mml_comp_hdr *comp_to_hdr(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_hdr, comp);
}

static s32 hdr_prepare(struct mml_comp *comp, struct mml_task *task,
		       struct mml_comp_config *ccfg)
{
	struct hdr_frame_data *hdr_frm;

	hdr_frm = kzalloc(sizeof(*hdr_frm), GFP_KERNEL);
	ccfg->data = hdr_frm;
	hdr_frm->reuse_reg.offs = hdr_frm->offs_reg;
	hdr_frm->reuse_reg.offs_size = ARRAY_SIZE(hdr_frm->offs_reg);
	hdr_frm->reuse_curve.offs = hdr_frm->offs_curve;
	hdr_frm->reuse_curve.offs_size = ARRAY_SIZE(hdr_frm->offs_curve);
	return 0;
}

static s32 hdr_buf_prepare(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	s32 ret = 0;

	mml_pq_msg("%s pipe_id[%d] engine_id[%d] en_hdr[%d]", __func__,
		   ccfg->pipe, comp->id,
		   dest->pq_config.en_hdr);

	mml_pq_trace_ex_begin("%s_%u", __func__, ccfg->pipe);
	if (dest->pq_config.en_hdr)
		ret = mml_pq_set_comp_config(task);
	mml_pq_trace_ex_end();

	return ret;
}

static s32 hdr_tile_prepare(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg,
			    struct tile_func_block *func,
			    union mml_tile_data *data)
{
	const struct mml_frame_config *cfg = task->config;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	const struct mml_comp_hdr *hdr = comp_to_hdr(comp);
	bool relay_mode = !dest->pq_config.en_hdr;

	func->for_func = tile_crop_for;
	func->enable_flag = dest->pq_config.en_hdr;

	func->full_size_x_in = cfg->frame_tile_sz.width;
	func->full_size_y_in = cfg->frame_tile_sz.height;
	func->full_size_x_out = func->full_size_x_in;
	func->full_size_y_out = func->full_size_y_in;

	func->in_tile_width = 8191;
	func->out_tile_width = 8191;
	func->in_tile_height = 65535;
	func->out_tile_height = 65535;

	if (!relay_mode || cfg->info.dest_cnt > 1) {
		func->type |= TILE_TYPE_CROP_EN;
		if (hdr->data->tile_loss) {
			func->l_tile_loss = hdr->data->tile_loss;
			func->r_tile_loss = hdr->data->tile_loss;
		}
		func->in_min_width = hdr->data->min_tile_width;
	}

	return 0;
}

static const struct mml_comp_tile_ops hdr_tile_ops = {
	.prepare = hdr_tile_prepare,
};

static u32 hdr_get_label_count(struct mml_comp *comp, struct mml_task *task,
			       struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];

	mml_pq_msg("%s pipe_id[%d] engine_id[%d] en_hdr[%d]", __func__,
		ccfg->pipe, comp->id, dest->pq_config.en_hdr);

	if (!dest->pq_config.en_hdr)
		return 0;

	return HDR_LABEL_TOTAL;
}

static void hdr_init(struct mml_comp *comp, struct cmdq_pkt *pkt, const phys_addr_t base_pa)
{
	struct mml_comp_hdr *hdr = comp_to_hdr(comp);

	/* Enable engine and shadow */
	cmdq_pkt_write(pkt, NULL, base_pa + hdr->data->reg_table[HDR_TOP], 0x100001, 0x00308001);
}

static void hdr_relay(struct mml_comp *comp, struct cmdq_pkt *pkt, const phys_addr_t base_pa,
		      u32 relay)
{
	struct mml_comp_hdr *hdr = comp_to_hdr(comp);

	cmdq_pkt_write(pkt, NULL, base_pa + hdr->data->reg_table[HDR_RELAY], relay, U32_MAX);
}

static void hdr_disable_curve(struct mml_comp *comp, struct cmdq_pkt *pkt,
	const phys_addr_t base_pa)
{
	struct mml_comp_hdr *hdr = comp_to_hdr(comp);

	if (!hdr->data->tile_loss)
		hdr_relay(comp, pkt, base_pa, 0x1);
	else {
		cmdq_pkt_write(pkt, NULL,
			base_pa + hdr->data->reg_table[HDR_TONE_MAP_TOP], 0x0, 0x1);
		cmdq_pkt_write(pkt, NULL,
			base_pa + hdr->data->reg_table[HDR_TOP], 0x0, 0x08080000);
		cmdq_pkt_write(pkt, NULL,
			base_pa + hdr->data->reg_table[HDR_3x3_COEF_00], 0x0, 0x1);
		cmdq_pkt_write(pkt, NULL,
			base_pa + hdr->data->reg_table[HDR_B_CHANNEL_NR], 0x0, 0x1);
		cmdq_pkt_write(pkt, NULL,
			base_pa + hdr->data->reg_table[HDR_A_LUMINANCE], 0x0, 0x1);
	}
}

static s32 hdr_config_init(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	mml_pq_msg("%s pipe_id[%d] engine_id[%d]", __func__, ccfg->pipe, comp->id);

	hdr_init(comp, task->pkts[ccfg->pipe], comp->base_pa);

	return 0;
}

static struct mml_pq_comp_config_result *get_hdr_comp_config_result(
	struct mml_task *task)
{
	return task->pq_task->comp_config.result;
}

static s32 hdr_hist_ctrl(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg,
			    struct mml_pq_comp_config_result *result)
{
	struct mml_comp_hdr *hdr = comp_to_hdr(comp);
	struct mml_frame_config *cfg = task->config;

	if (IS_ERR_OR_NULL(task) || IS_ERR_OR_NULL(task->pq_task)) {
		mml_err("%s task or pq_task is NULL", __func__);
		return 0;
	}

	mml_pq_ir_log("%s jobid[%d] pipe_id[%d] engine_id[%d]",
		__func__, task->job.jobid, ccfg->pipe, comp->id);

	mml_pq_set_hdr_status(task->pq_task, ccfg->node->out_idx);

	if (result->is_hdr_need_readback &&
		task->pq_task->read_status.hdr_comp == MML_PQ_HIST_IDLE) {
		if (hdr->hist_pkts[ccfg->pipe]) {

			hdr->pq_task = task->pq_task;
			mml_pq_get_pq_task(hdr->pq_task);

			hdr->frame_data.size_info.out_rotate[ccfg->node->out_idx] =
				cfg->out_rotate[ccfg->node->out_idx];
			memcpy(&hdr->frame_data.pq_param, task->pq_param,
				MML_MAX_OUTPUTS * sizeof(struct mml_pq_param));
			memcpy(&hdr->frame_data.info, &task->config->info,
				sizeof(struct mml_frame_info));
			memcpy(&hdr->frame_data.frame_out, &task->config->frame_out,
				MML_MAX_OUTPUTS * sizeof(struct mml_frame_size));
			memcpy(&hdr->frame_data.size_info.frame_in_crop_s[0],
				&cfg->frame_in_crop[0],	MML_MAX_OUTPUTS * sizeof(struct mml_crop));

			hdr->frame_data.size_info.crop_size_s.width =
				cfg->frame_tile_sz.width;
			hdr->frame_data.size_info.crop_size_s.height=
				cfg->frame_tile_sz.height;
			hdr->frame_data.size_info.frame_size_s.width = cfg->frame_in.width;
			hdr->frame_data.size_info.frame_size_s.height = cfg->frame_in.height;
			hdr->jobid = task->job.jobid;

			if (task->config->dual)
				hdr->cut_pos_x = cfg->hist_div[ccfg->tile_eng_idx];
			else
				hdr->cut_pos_x = task->config->frame_tile_sz.width;

			hdr->crop_height = task->config->frame_tile_sz.height;
			hdr->crop_width = task->config->frame_tile_sz.width;
			hdr->pipe = ccfg->pipe;
			hdr->out_idx = ccfg->node->out_idx;

			if (!(hdr->hdr_hist[ccfg->pipe]))
				mml_pq_get_readback_buffer(task, ccfg->pipe,
					&(hdr->hdr_hist[ccfg->pipe]));
			hdr->hist_pkts[ccfg->pipe]->no_irq = task->config->dpc;

			if (hdr->data->rb_mode == RB_EOF_MODE) {
				mml_clock_lock(task->config->mml);
				call_hw_op(task->config->path[0]->mmlsys,
					mminfra_pw_enable);
				call_hw_op(task->config->path[0]->mmlsys,
					pw_enable);
				mml_msg_dpc("%s dpc exception flow on", __func__);
				mml_dpc_exc_keep(task->config->mml);
				call_hw_op(comp, clk_enable);
				mml_clock_unlock(task->config->mml);
				mml_lock_wake_lock(hdr->mml, true);
				hdr->dpc = task->config->dpc;
				hdr->mmlsys_comp =
					task->config->path[0]->mmlsys;
			}

			queue_work(hdr->hdr_hist_wq, &hdr->hdr_hist_task);
		}
	}
	mml_pq_ir_log("%s end jobid[%d] pipe_id[%d] engine_id[%d]",
		__func__, task->job.jobid, ccfg->pipe, comp->id);
	return 0;
}


static s32 hdr_config_frame(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_comp_hdr *hdr = comp_to_hdr(comp);
	const phys_addr_t base_pa = comp->base_pa;
	struct mml_pq_comp_config_result *result;
	struct mml_pq_reg *regs;
	u32 *curve;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];
	s32 ret;
	u32 i;
	s8 mode = task->config->info.mode;

	mml_pq_msg("%s pipe_id[%d] engine_id[%d] en_hdr[%d] mode[%d] pkt[%p]", __func__,
		ccfg->pipe, comp->id, dest->pq_config.en_hdr, mode, pkt);

	/* Enable 10-bit output by default
	 * cmdq_pkt_write(pkt, NULL, base_pa + hdr->data->reg_table[HDR_TOP], 3 << 28, 0x30000000);
	 */

	if (!dest->pq_config.en_hdr) {
		/* relay mode */
		hdr_relay(comp, pkt, base_pa, 0x1);
		return 0;
	}
	hdr_relay(comp, pkt, base_pa, 0x0);

	do {
		ret = mml_pq_get_comp_config_result(task, HDR_WAIT_TIMEOUT_MS);
		if (ret) {
			mml_pq_comp_config_clear(task);
			hdr_frm->config_success = false;
			hdr_disable_curve(comp, pkt, base_pa);
			mml_pq_err("%s: get hdr param timeout: %d in %dms",
				 __func__, ret, HDR_WAIT_TIMEOUT_MS);
			ret = -ETIMEDOUT;
			goto exit;
		}

		result = get_hdr_comp_config_result(task);
		if (!result) {
			hdr_frm->config_success = false;
			hdr_disable_curve(comp, pkt, base_pa);
			mml_pq_err("%s: not get result from user lib", __func__);
			ret = -EBUSY;
			goto exit;
		}
	} while ((mml_pq_debug_mode & MML_PQ_SET_TEST) && result->is_set_test);

	regs = result->hdr_regs;
	curve = result->hdr_curve;

	/* TODO: use different regs */
	mml_pq_msg("%s:config hdr regs, count: %d", __func__, result->hdr_reg_cnt);
	for (i = 0; i < result->hdr_reg_cnt; i++) {
		mml_write_array(pkt, base_pa + regs[i].offset, regs[i].value,
			regs[i].mask, reuse, cache, &hdr_frm->reuse_reg);
		mml_pq_msg("[hdr][config][%x] = %#x mask(%#x)",
			regs[i].offset, regs[i].value, regs[i].mask);
	}

	if (mode == MML_MODE_MML_DECOUPLE) {
		for (i = 0; i < HDR_CURVE_NUM; i += 2) {
			mml_write_array(pkt, base_pa + hdr->data->reg_table[HDR_GAIN_TABLE_1],
				curve[i], U32_MAX, reuse, cache, &hdr_frm->reuse_curve);
			mml_write_array(pkt, base_pa + hdr->data->reg_table[HDR_GAIN_TABLE_2],
				curve[i + 1], U32_MAX, reuse, cache, &hdr_frm->reuse_curve);
		}
	} else if (mode == MML_MODE_DDP_ADDON || mode == MML_MODE_DIRECT_LINK) {
		hdr->curve_pq_task = task->pq_task;
		hdr->dual = cfg->dual;
		hdr->pipe = ccfg->pipe;
		memcpy(hdr->gain_curve, &curve[0], sizeof(u32)*HDR_CURVE_NUM);

		if (!hdr->clts) {
			hdr->clts = mml_get_cmdq_clt(cfg->mml,
				ccfg->pipe + GCE_THREAD_START);
			if (!hdr->clts)
				mml_pq_err("%s cmdq client get failed", __func__);
		}

		if ((!hdr->hist_pkts[ccfg->pipe] && hdr->clts))
			hdr->hist_pkts[ccfg->pipe] = cmdq_pkt_create(hdr->clts);

		if (!hdr->hist_pkts[ccfg->pipe] || !hdr->clts) {
			mml_pq_err("%s pkt or clits get failed", __func__);
			complete_all(&hdr->curve_pq_task->hdr_curve_ready[hdr->pipe]);
			goto jump_cmd;
		}

		if (!hdr->hist_pkts[ccfg->pipe]->dev) {
			mml_pq_err("%s pkt dev get failed", __func__);
			complete_all(&hdr->curve_pq_task->hdr_curve_ready[hdr->pipe]);
			goto jump_cmd;
		}

		queue_work(hdr->hdr_curve_wq, &hdr->hdr_curve_task);
		hdr_hist_ctrl(comp, task, ccfg, result);
	}

	cmdq_pkt_write(pkt, NULL,
		base_pa + hdr->data->reg_table[HDR_HIST_ADDR], 1 << 8, 1 << 8);
	cmdq_pkt_write(pkt, NULL,
		base_pa + hdr->data->reg_table[HDR_HIST_CTRL_2], 1 << 31, 1 << 31);

	if (mode == MML_MODE_MML_DECOUPLE)
		cmdq_pkt_write(pkt, NULL,
			base_pa + hdr->data->reg_table[HDR_GAIN_TABLE_0], 1 << 11, 1 << 11);

	mml_pq_msg("%s is_hdr_need_readback[%d] reuses count %u %u",
		__func__, result->is_hdr_need_readback,
		hdr_frm->reuse_reg.idx,
		hdr_frm->reuse_curve.idx);

jump_cmd:
	hdr_frm->is_hdr_need_readback = result->is_hdr_need_readback;
	hdr_frm->config_success = true;

	return 0;

exit:
	return ret;
}

static s32 hdr_config_tile(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;
	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);
	struct mml_comp_hdr *hdr = comp_to_hdr(comp);

	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);
	u16 tile_cnt = cfg->frame_tile[ccfg->pipe]->tile_cnt;
	u32 hdr_input_w;
	u32 hdr_input_h;
	u32 hdr_crop_xs;
	u32 hdr_crop_xe;
	u32 hdr_crop_ys;
	u32 hdr_crop_ye;
	u32 hdr_hist_left_start = 0;
	u32 hdr_hist_begin_x = 0;
	u32 hdr_hist_end_x = 0;
	u32 hdr_first_tile = 0;
	u32 hdr_last_tile = 0;

	mml_pq_msg("%s pipe_id[%d] engine_id[%d] en_hdr[%d]", __func__,
		ccfg->pipe, comp->id, dest->pq_config.en_hdr);

	hdr_input_w = tile->in.xe - tile->in.xs + 1;
	hdr_input_h = tile->in.ye - tile->in.ys + 1;
	hdr_crop_xs = tile->out.xs - tile->in.xs;
	hdr_crop_xe = tile->out.xe - tile->in.xs;
	hdr_crop_ys = tile->out.ys - tile->in.ys;
	hdr_crop_ye = tile->out.ye - tile->in.ys;

	cmdq_pkt_write(pkt, NULL, base_pa + hdr->data->reg_table[HDR_TILE_POS],
		(tile->out.ys << 16) + tile->out.xs, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + hdr->data->reg_table[HDR_SIZE_0],
		(hdr_input_h << 16) + hdr_input_w, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + hdr->data->reg_table[HDR_SIZE_1],
		(hdr_crop_xe << 16) + hdr_crop_xs, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + hdr->data->reg_table[HDR_SIZE_2],
		(hdr_crop_ye << 16) + hdr_crop_ys, U32_MAX);


	mml_pq_msg("%s %d: [input] jobid[%d] [xs, xe] = [%d, %d], [ys, ye] = [%d, %d]",
			__func__, idx, task->job.jobid, tile->in.xs, tile->in.xe,
			tile->in.ys, tile->in.ye);

	mml_pq_msg("%s %d: [output] [xs, xe] = [%d, %d], [ys, ye] = [%d, %d]",
			__func__, idx, tile->out.xs, tile->out.xe, tile->out.ys,
			tile->out.ye);

	mml_pq_msg("%s %d: jobid[%d] en_hdr[%d], hist_div[%d] out_hist_xs[%d] pipe[%d]",
			__func__, idx, task->job.jobid, dest->pq_config.en_hdr,
			cfg->hist_div[ccfg->tile_eng_idx], hdr_frm->out_hist_xs,
			ccfg->pipe);

	if (!dest->pq_config.en_hdr)
		return 0;

	if (!idx) {
		if (task->config->dual)
			hdr_frm->cut_pos_x = cfg->hist_div[ccfg->tile_eng_idx];
		else
			hdr_frm->cut_pos_x = task->config->frame_tile_sz.width;
		if (ccfg->pipe)
			hdr_frm->out_hist_xs = hdr_frm->cut_pos_x;
	}

	hdr_hist_left_start =
		(tile->out.xs > hdr_frm->out_hist_xs) ? tile->out.xs : hdr_frm->out_hist_xs;
	hdr_hist_begin_x = hdr_hist_left_start - tile->in.xs;

	if (task->config->dual && !ccfg->pipe && (idx + 1 >= tile_cnt))
		hdr_hist_end_x = hdr_frm->cut_pos_x - tile->in.xs - 1;
	else
		hdr_hist_end_x = tile->out.xe - tile->in.xs;

	cmdq_pkt_write(pkt, NULL,
		base_pa + hdr->data->reg_table[HDR_HIST_CTRL_0], hdr_hist_begin_x, 0x0000ffff);
	cmdq_pkt_write(pkt, NULL,
		base_pa + hdr->data->reg_table[HDR_HIST_CTRL_1], hdr_hist_end_x, 0x0000ffff);

	/* for n+1 tile*/
	if (tile_cnt == 1) {
		hdr_frm->out_hist_xs = 0;
		hdr_first_tile = 1;
		hdr_last_tile = 1;
	} else if (!idx) {
		if (task->config->dual && ccfg->pipe)
			hdr_frm->out_hist_xs = tile->in.xs + hdr_hist_end_x + 1;
		else
			hdr_frm->out_hist_xs = tile->out.xe + 1;
		hdr_first_tile = 1;
		hdr_last_tile = 0;
	} else if (idx + 1 >= tile_cnt) {
		hdr_frm->out_hist_xs = 0;
		hdr_first_tile = 0;
		hdr_last_tile = 1;
	} else {
		if (task->config->dual && ccfg->pipe)
			hdr_frm->out_hist_xs = tile->in.xs + hdr_hist_end_x + 1;
		else
			hdr_frm->out_hist_xs = tile->out.xe + 1;
		hdr_first_tile = 0;
		hdr_last_tile = 0;
	}

	task->pq_task->hdr_readback.readback_data.cut_pos_x =
		hdr_frm->cut_pos_x;

	cmdq_pkt_write(pkt, NULL, base_pa + hdr->data->reg_table[HDR_TOP],
		(hdr_first_tile << 5) | (hdr_last_tile << 6), 0x00000060);
	cmdq_pkt_write(pkt, NULL,
		base_pa + hdr->data->reg_table[HDR_HIST_ADDR], (hdr_first_tile << 9), 0x00000200);

	mml_pq_msg("%s %d: jobid[%d] hdr_hist_begin_x[%d] hdr_hist_end_x[%d] out_hist_xs[%d]",
		__func__, idx, task->job.jobid, hdr_hist_begin_x, hdr_hist_end_x,
		hdr_frm->out_hist_xs);
	mml_pq_msg("%s %d: jobid[%d] hdr_first_tile[%d] hdr_last_tile[%d]",
		__func__, idx, task->job.jobid, hdr_first_tile, hdr_last_tile);

	mml_pq_msg("%s %d: jobid[%d] out_hist_xs[%d] cut_pos_x[%d]",
		__func__, idx, task->job.jobid,	hdr_frm->out_hist_xs,
		hdr_frm->cut_pos_x);

	return 0;
}

static void hdr_readback_vcp(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_comp_hdr *hdr = comp_to_hdr(comp);
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	u8 pipe = ccfg->pipe;
	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);


	u32 gpr = hdr->data->gpr[ccfg->pipe];
	u32 engine = CMDQ_VCP_ENG_MML_HDR0 + pipe;

	mml_pq_rb_msg("%s hdr_hist[%p] pipe[%d]", __func__,
		task->pq_task->hdr_hist[pipe], pipe);

	if (!(task->pq_task->hdr_hist[pipe])) {
		task->pq_task->hdr_hist[pipe] =
			kzalloc(sizeof(struct mml_pq_readback_buffer), GFP_KERNEL);

		if (unlikely(!task->pq_task->hdr_hist[pipe])) {
			mml_pq_err("%s not enough mem for hdr_hist", __func__);
			return;
		}

		task->pq_task->hdr_hist[pipe]->va =
			cmdq_get_vcp_buf(engine, &(task->pq_task->hdr_hist[pipe]->pa));

		mml_pq_rb_msg("%s hdr_hist[%p] pipe[%d] hdr_hist_va[%p] hdr_hist_pa[%llx]",
			__func__, task->pq_task->hdr_hist[pipe], pipe,
			task->pq_task->hdr_hist[pipe]->va,
			task->pq_task->hdr_hist[pipe]->pa);
	}

	mml_pq_get_vcp_buf_offset(task, MML_PQ_HDR0+pipe, task->pq_task->hdr_hist[pipe]);

	cmdq_vcp_enable(true);

	cmdq_pkt_readback(pkt, engine, task->pq_task->hdr_hist[pipe]->va_offset,
		HDR_HIST_NUM, gpr,
		&reuse->labels[reuse->label_idx],
		&hdr_frm->polling_reuse);

	add_reuse_label(reuse, &hdr_frm->labels[HDR_POLLGPR_0],
		task->pq_task->hdr_hist[pipe]->va_offset);
}

static void hdr_readback_cmdq(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct mml_comp_hdr *hdr = comp_to_hdr(comp);
	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);
	const phys_addr_t base_pa = comp->base_pa;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_frame_config *cfg = task->config;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];

	u8 pipe = ccfg->pipe;
	dma_addr_t begin_pa = 0;
	dma_addr_t pa = 0;
	u32 *condi_inst = NULL;
	struct cmdq_operand lop, rop;

	const u16 idx_counter = CMDQ_THR_SPR_IDX1;
	const u16 idx_val = CMDQ_THR_SPR_IDX2;
	const u16 idx_out = hdr->data->cpr[ccfg->pipe];
	const u16 idx_out64 = CMDQ_CPR_TO_CPR64(idx_out);

	mml_pq_get_readback_buffer(task, pipe, &(task->pq_task->hdr_hist[pipe]));

	if (unlikely(!task->pq_task->hdr_hist[pipe])) {
		mml_pq_err("%s job_id[%d] hdr_hist is null", __func__,
			task->job.jobid);
		return;
	}

	pa = task->pq_task->hdr_hist[pipe]->pa;

	/* readback to this pa */
	mml_assign(pkt, idx_out, (u32)pa,
		reuse, cache, &hdr_frm->labels[HDR_POLLGPR_0]);
	mml_assign(pkt, idx_out + 1, (u32)(pa >> 32),
		reuse, cache, &hdr_frm->labels[HDR_POLLGPR_1]);

	cmdq_pkt_wfe(pkt, hdr->event_eof);

	/* counter init to 0 */
	cmdq_pkt_assign_command(pkt, idx_counter, 0);

	/* loop again here */
	hdr_frm->begin_offset = pkt->cmd_buf_size;
	begin_pa = cmdq_pkt_get_pa_by_offset(pkt, hdr_frm->begin_offset);

	/* read to value cpr */
	cmdq_pkt_read_addr(pkt, base_pa + hdr->data->reg_table[HDR_HIST_DATA], idx_val);
	/* write value spr to dst cpr */
	cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

	/* jump forward end if match, if spr1 >= 57 - 1	*/
	cmdq_pkt_assign_command(pkt, CMDQ_THR_SPR_IDX0, 0);
	hdr_frm->condi_offset = pkt->cmd_buf_size - CMDQ_INST_SIZE;

	/* inc counter */
	lop.reg = true;
	lop.idx = idx_counter;
	rop.reg = false;
	rop.value = 1;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_counter, &lop, &rop);
	/* inc outut pa */
	lop.reg = true;
	lop.idx = idx_out;
	rop.reg = false;
	rop.value = 4;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);


	lop.reg = true;
	lop.idx = idx_counter;
	rop.reg = false;
	rop.value =  HDR_HIST_CNT - 1;
	cmdq_pkt_cond_jump_abs(pkt, CMDQ_THR_SPR_IDX0, &lop, &rop,
		CMDQ_LESS_THAN_AND_EQUAL);
	condi_inst = (u32 *)cmdq_pkt_get_va_by_offset(pkt, hdr_frm->condi_offset);
	if (unlikely(!condi_inst))
		mml_pq_err("%s wrong offset %u\n", __func__, hdr_frm->condi_offset);

	*condi_inst = (u32)CMDQ_REG_SHIFT_ADDR(begin_pa);

	/* read to value cpr */
	cmdq_pkt_read_addr(pkt, base_pa + hdr->data->reg_table[HDR_LBOX_DET_4], idx_val);
	/* write value spr to dst cpr */
	cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

	if ((mml_pq_debug_mode & MML_PQ_HIST_CHECK)) {
		lop.reg = true;
		lop.idx = idx_out;
		rop.reg = false;
		rop.value = 4;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);

		cmdq_pkt_read_addr(pkt, base_pa + hdr->data->reg_table[HDR_HIST_CTRL_0], idx_val);
		/* write value spr to dst cpr */
		cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

		lop.reg = true;
		lop.idx = idx_out;
		rop.reg = false;
		rop.value = 4;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);

		cmdq_pkt_read_addr(pkt, base_pa + hdr->data->reg_table[HDR_HIST_CTRL_1], idx_val);
		/* write value spr to dst cpr */
		cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);
	}

	mml_pq_rb_msg("%s end job_id[%d] engine_id[%d] va[%p] pa[%pad] pkt[%p]",
		__func__, task->job.jobid, comp->id, task->pq_task->hdr_hist[pipe]->va,
		&task->pq_task->hdr_hist[pipe]->pa, pkt);

	mml_pq_rb_msg("%s end job_id[%d] condi:offset[%u] inst[%p], begin:offset[%u] pa[%pad]",
		__func__, task->job.jobid, hdr_frm->condi_offset, condi_inst,
		hdr_frm->begin_offset, &begin_pa);
}


static s32 hdr_config_post(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	struct mml_comp_hdr *hdr = comp_to_hdr(comp);
	struct mml_frame_dest *dest = &task->config->info.dest[ccfg->node->out_idx];
	bool vcp = hdr->data->vcp_readback;
	s8 mode = task->config->info.mode;

	if (!dest->pq_config.en_hdr)
		goto exit;

	if (mode != MML_MODE_MML_DECOUPLE)
		goto comp_config_put;

	if (vcp)
		hdr_readback_vcp(comp, task, ccfg);
	else
		hdr_readback_cmdq(comp, task, ccfg);

comp_config_put:
	mml_pq_put_comp_config_result(task);
exit:
	return 0;

}

static s32 hdr_reconfig_frame(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pq_comp_config_result *result;
	struct mml_pq_reg *regs;
	struct mml_comp_hdr *hdr = comp_to_hdr(comp);
	u32 *curve;
	u32 i, j, val_idx;
	s32 ret;
	s8 mode = task->config->info.mode;

	mml_pq_msg("%s pipe_id[%d] engine_id[%d] en_hdr[%d] config_success[%d]", __func__,
		ccfg->pipe, comp->id, dest->pq_config.en_hdr,
		hdr_frm->config_success);
	if (!dest->pq_config.en_hdr)
		return 0;

	do {
		ret = mml_pq_get_comp_config_result(task, HDR_WAIT_TIMEOUT_MS);
		if (ret) {
			mml_pq_comp_config_clear(task);
			mml_pq_err("get hdr param timeout: %d in %dms",
				ret, HDR_WAIT_TIMEOUT_MS);
			ret = -ETIMEDOUT;
			goto exit;
		}

		result = get_hdr_comp_config_result(task);
		if (!result || !hdr_frm->config_success || !result->hdr_reg_cnt) {
			mml_pq_err("%s: not get result from user lib", __func__);
			ret = -EBUSY;
			goto exit;
		}
	} while ((mml_pq_debug_mode & MML_PQ_SET_TEST) && result->is_set_test);

	regs = result->hdr_regs;
	curve = result->hdr_curve;
	if (mode == MML_MODE_MML_DECOUPLE) {
		val_idx = 0;
		for (i = 0; i < hdr_frm->reuse_reg.idx; i++)
			for (j = 0; j < hdr_frm->reuse_reg.offs[i].cnt; j++, val_idx++)
				mml_update_array(reuse, &hdr_frm->reuse_reg, i, j,
					regs[val_idx].value);

		val_idx = 0;
		for (i = 0; i < hdr_frm->reuse_curve.idx; i++)
			for (j = 0; j < hdr_frm->reuse_curve.offs[i].cnt; j++, val_idx++)
				mml_update_array(reuse, &hdr_frm->reuse_curve, i, j,
					curve[val_idx]);
	} else if (mode == MML_MODE_DIRECT_LINK) {
		hdr->curve_pq_task = task->pq_task;
		hdr->dual = cfg->dual;
		hdr->pipe = ccfg->pipe;
		memcpy(hdr->gain_curve, &curve[0], sizeof(u32)*HDR_CURVE_NUM);
		queue_work(hdr->hdr_curve_wq, &hdr->hdr_curve_task);
		hdr_hist_ctrl(comp, task, ccfg, result);
	}

	mml_pq_msg("%s is_hdr_need_readback[%d]",
		__func__, result->is_hdr_need_readback);
	hdr_frm->is_hdr_need_readback = result->is_hdr_need_readback;
	task->pq_task->hdr_readback.readback_data.cut_pos_x =
		hdr_frm->cut_pos_x;
	hdr->cut_pos_x = hdr_frm->cut_pos_x;

	return 0;

exit:
	return ret;
}

static s32 hdr_config_repost(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	struct mml_comp_hdr *hdr = comp_to_hdr(comp);
	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);
	struct mml_frame_dest *dest = &task->config->info.dest[ccfg->node->out_idx];
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	bool vcp = hdr->data->vcp_readback;
	dma_addr_t begin_pa;
	u32 *condi_inst;
	u8 pipe = ccfg->pipe;
	u32 engine = CMDQ_VCP_ENG_MML_HDR0 + pipe;
	s8 mode = task->config->info.mode;

	if (!dest->pq_config.en_hdr)
		goto exit;

	if (mode != MML_MODE_MML_DECOUPLE)
		goto comp_config_put;

	if (vcp) {
		if (!(task->pq_task->hdr_hist[pipe])) {
			task->pq_task->hdr_hist[pipe] =
				kzalloc(sizeof(struct mml_pq_readback_buffer), GFP_KERNEL);

			if (unlikely(!task->pq_task->hdr_hist[pipe])) {
				mml_pq_err("%s not enough mem for hdr_hist", __func__);
				goto comp_config_put;
			}

			task->pq_task->hdr_hist[pipe]->va =
				cmdq_get_vcp_buf(engine, &(task->pq_task->hdr_hist[pipe]->pa));
		}
		cmdq_vcp_enable(true);
		mml_pq_get_vcp_buf_offset(task, MML_PQ_HDR0 + pipe,
			task->pq_task->hdr_hist[pipe]);
		mml_update(reuse, hdr_frm->labels[HDR_POLLGPR_0],
			cmdq_pkt_vcp_reuse_val(engine,
			task->pq_task->hdr_hist[pipe]->va_offset,
			HDR_HIST_NUM));

		cmdq_pkt_reuse_poll(pkt, &hdr_frm->polling_reuse);

	} else {
		mml_pq_get_readback_buffer(task, pipe, &(task->pq_task->hdr_hist[pipe]));

		if (unlikely(!task->pq_task->hdr_hist[pipe])) {
			mml_pq_err("%s job_id[%d] hdr_hist is null", __func__,
				task->job.jobid);
			goto comp_config_put;
		}

		mml_update(reuse, hdr_frm->labels[HDR_POLLGPR_0],
			(u32)task->pq_task->hdr_hist[pipe]->pa);
		mml_update(reuse, hdr_frm->labels[HDR_POLLGPR_1],
			(u32)(task->pq_task->hdr_hist[pipe]->pa >> 32));

		begin_pa = cmdq_pkt_get_pa_by_offset(pkt, hdr_frm->begin_offset);
		condi_inst = (u32 *)cmdq_pkt_get_va_by_offset(pkt, hdr_frm->condi_offset);
		if (unlikely(!condi_inst))
			mml_pq_err("%s wrong offset %u\n", __func__, hdr_frm->condi_offset);

		*condi_inst = (u32)CMDQ_REG_SHIFT_ADDR(begin_pa);

		mml_pq_rb_msg("%s end job_id[%d] engine_id[%d] va[%p] pa[%pad] pkt[%p] ",
			__func__, task->job.jobid, comp->id, task->pq_task->hdr_hist[pipe]->va,
			&task->pq_task->hdr_hist[pipe]->pa, pkt);

		mml_pq_rb_msg("%s end job_id[%d]condi:offset[%u]inst[%p],begin:offset[%u]pa[%pad]",
			__func__, task->job.jobid, hdr_frm->condi_offset, condi_inst,
			hdr_frm->begin_offset, &begin_pa);
	}

comp_config_put:
	mml_pq_put_comp_config_result(task);
exit:
	return 0;
}

static const struct mml_comp_config_ops hdr_cfg_ops = {
	.prepare = hdr_prepare,
	.buf_prepare = hdr_buf_prepare,
	.get_label_count = hdr_get_label_count,
	.init = hdr_config_init,
	.frame = hdr_config_frame,
	.tile = hdr_config_tile,
	.post = hdr_config_post,
	.reframe = hdr_reconfig_frame,
	.repost = hdr_config_repost,
};

static void hdr_histogram_check(struct mml_comp *comp, struct mml_task *task, u32 offset,
				struct mml_comp_config *ccfg)
{
	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);
	struct mml_comp_hdr *hdr = comp_to_hdr(comp);
	u8 pipe = ccfg->pipe;
	void __iomem *base = comp->base;

	u32 i = 0, sum = 0;
	u32 expect_value_letter = 0, expect_value_crop = 0, expect_value_hist = 0;
	u32 letter_up = 0, letter_down = 0, letter_height = 0;
	u32 crop_width =
		task->config->frame_tile_sz.width;
	u32 crop_height =
		task->config->frame_tile_sz.height;
	bool vcp = hdr->data->vcp_readback;
	u32 hist_up = 0, hist_down = 0, hist_height = 0;

	for (i = 0; i < HDR_HIST_CNT; i++)
		sum = sum + task->pq_task->hdr_hist[pipe]->va[offset/4+i];

	letter_up = ((task->pq_task->hdr_hist[pipe]->va[57] &
		0x0000FFFF) >> 0);
	letter_down = ((task->pq_task->hdr_hist[pipe]->va[57] &
		0xFFFF0000) >> 16);
	letter_height = letter_down - letter_up;

	if (vcp) {
		hist_up = ((readl(base + hdr->data->reg_table[HDR_HIST_CTRL_0]) &
			0xFFFF0000) >> 16);
		hist_down = ((readl(base + hdr->data->reg_table[HDR_HIST_CTRL_1]) &
			0xFFFF0000) >> 16);
	} else {
		hist_up = ((task->pq_task->hdr_hist[pipe]->va[58] &
			0xFFFF0000) >> 16);
		hist_down = ((task->pq_task->hdr_hist[pipe]->va[59] &
			0xFFFF0000) >> 16);
	}
	hist_height = hist_down - hist_up;

	if (task->config->dual) {
		if (pipe) {
			expect_value_hist = (hist_height+1) *
				(crop_width - hdr_frm->cut_pos_x) * 8;
			expect_value_letter = (letter_height+1) *
				(crop_width - hdr_frm->cut_pos_x) * 8;
			expect_value_crop = crop_height *
				(crop_width - hdr_frm->cut_pos_x) * 8;
		} else {
			expect_value_hist =
				(hist_height+1) * hdr_frm->cut_pos_x * 8;
			expect_value_letter =
				(letter_height+1) * hdr_frm->cut_pos_x * 8;
			expect_value_crop =
				crop_height * hdr_frm->cut_pos_x * 8;
		}
	} else {
		expect_value_letter = (letter_height+1) * crop_width * 8;
		expect_value_crop = crop_height * crop_width * 8;
	}

	mml_pq_rb_msg("%s sum[%u] expect_value[%u %u %u] job_id[%d] pipe[%d]",
		__func__, sum, expect_value_letter, expect_value_crop, expect_value_hist,
		task->job.jobid, pipe);
	mml_pq_rb_msg("%s job_id[%d] pipe[%d] letter_height[%d] down[%d] up[%d]",
		__func__, task->job.jobid, pipe, letter_height, letter_down,
		letter_up);

	if (sum != expect_value_letter &&
		sum != expect_value_crop &&
		sum != expect_value_hist) {
		mml_pq_err("%s sum[%u] expect_value_letter[%u] job_id[%d] pipe[%d]",
			__func__, sum, expect_value_letter, task->job.jobid, pipe);
		for (i = 0; i < HDR_HIST_CNT-1; i += 4)
			mml_pq_err("%s hist[%d - %d] = [%d, %d, %d, %d]",
				__func__, i, i+3,
				task->pq_task->hdr_hist[pipe]->va[offset/4+i],
				task->pq_task->hdr_hist[pipe]->va[offset/4+i+1],
				task->pq_task->hdr_hist[pipe]->va[offset/4+i+2],
				task->pq_task->hdr_hist[pipe]->va[offset/4+i+3]);

			mml_pq_err("%s hist[56-57] = [%d, %d]",
				__func__, task->pq_task->hdr_hist[pipe]->va[offset/4+56],
				task->pq_task->hdr_hist[pipe]->va[offset/4+i+57]);

			if (mml_pq_debug_mode & MML_PQ_HIST_CHECK)
				mml_pq_util_aee("MML_PQ_HDR_Histogram Error",
					"HDR Histogram error need to check jobid:%d",
					task->job.jobid);
	}
}

static void hdr_task_done_readback(struct mml_comp *comp, struct mml_task *task,
					 struct mml_comp_config *ccfg)
{
	struct mml_comp_hdr *hdr = comp_to_hdr(comp);
	struct mml_frame_config *cfg = task->config;
	struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	u8 pipe = ccfg->pipe;
	u32 engine = MML_PQ_HDR0 + pipe;
	bool vcp = hdr->data->vcp_readback;
	u32 offset = 0;
	s8 mode = cfg->info.mode;


	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s is_hdr_need_readback[%d] id[%d] en_hdr[%d] mode[%d]",
		__func__, hdr_frm->is_hdr_need_readback, comp->id,
		dest->pq_config.en_hdr, mode);

	if (!dest->pq_config.en_hdr || !task->pq_task->hdr_hist[pipe])
		goto exit;

	if (!dest->pq_config.en_hdr) {
		s32 i;
		u32 *phist = kmalloc(HDR_HIST_NUM*sizeof(u32), GFP_KERNEL);
		void __iomem *base = comp->base;

		for (i = 0; i < HDR_HIST_NUM; i++) {
			if (i == 57) {
				phist[i] = readl(base + hdr->data->reg_table[HDR_LBOX_DET_4]);
				continue;
			}
			phist[i] = readl(base + hdr->data->reg_table[HDR_HIST_DATA]);
		}
		mml_pq_dc_hdr_readback(task, ccfg->pipe, phist);
	}

	offset = vcp ? task->pq_task->hdr_hist[pipe]->va_offset : 0;

	mml_pq_msg("%s job_id[%d] id[%d] pipe[%d] en_hdr[%d] va[%p] pa[%pad]",
		__func__, task->job.jobid, comp->id, ccfg->pipe,
		dest->pq_config.en_hdr, task->pq_task->hdr_hist[pipe]->va,
		&task->pq_task->hdr_hist[pipe]->pa);

	mml_pq_ir_log("%s jobid[%d] hist[0~4]={%08x, %08x, %08x, %08x, %08x} hist[57]=[%08x]",
		__func__, task->job.jobid,
		task->pq_task->hdr_hist[pipe]->va[offset/4+0],
		task->pq_task->hdr_hist[pipe]->va[offset/4+1],
		task->pq_task->hdr_hist[pipe]->va[offset/4+2],
		task->pq_task->hdr_hist[pipe]->va[offset/4+3],
		task->pq_task->hdr_hist[pipe]->va[offset/4+4],
		task->pq_task->hdr_hist[pipe]->va[offset/4+57]);

	if (hdr_frm->is_hdr_need_readback) {
		mml_pq_dc_hdr_readback(task, ccfg->pipe,
			&(task->pq_task->hdr_hist[pipe]->va[offset/4]));
		if (mml_pq_debug_mode & MML_PQ_HIST_CHECK) {
			hdr_histogram_check(comp, task, offset, ccfg);
		}
	}

	if (vcp) {
		mml_pq_put_vcp_buf_offset(task, engine, task->pq_task->hdr_hist[pipe]);
		cmdq_vcp_enable(false);
	} else
		mml_pq_put_readback_buffer(task, pipe, &(task->pq_task->hdr_hist[pipe]));
exit:
	mml_pq_trace_ex_end();
}

static const struct mml_comp_hw_ops hdr_hw_ops = {
	.clk_enable = &mml_comp_clk_enable,
	.clk_disable = &mml_comp_clk_disable,
	.task_done = hdr_task_done_readback,
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

static void hdr_debug_dump(struct mml_comp *comp)
{
	struct mml_comp_hdr *hdr = comp_to_hdr(comp);
	void __iomem *base = comp->base;
	u32 value[16];
	u32 hdr_top;

	mml_err("hdr component %u dump:", comp->id);

	/* Enable shadow read working */
	hdr_top = read_reg_value(comp, hdr->data->reg_table[HDR_TOP]);
	hdr_top |= 0x8000;
	writel(hdr_top, base + hdr->data->reg_table[HDR_TOP]);

	value[0] = read_reg_value(comp, hdr->data->reg_table[HDR_TOP]);
	value[1] = read_reg_value(comp, hdr->data->reg_table[HDR_RELAY]);
	value[2] = read_reg_value(comp, hdr->data->reg_table[HDR_INTSTA]);
	value[3] = read_reg_value(comp, hdr->data->reg_table[HDR_ENGSTA]);
	value[4] = read_reg_value(comp, hdr->data->reg_table[HDR_SIZE_0]);
	value[5] = read_reg_value(comp, hdr->data->reg_table[HDR_SIZE_1]);
	value[6] = read_reg_value(comp, hdr->data->reg_table[HDR_SIZE_2]);
	value[7] = read_reg_value(comp, hdr->data->reg_table[HDR_HIST_CTRL_0]);
	value[8] = read_reg_value(comp, hdr->data->reg_table[HDR_HIST_CTRL_1]);
	value[9] = read_reg_value(comp, hdr->data->reg_table[HDR_CURSOR_CTRL]);
	value[10] = read_reg_value(comp, hdr->data->reg_table[HDR_CURSOR_POS]);
	value[11] = read_reg_value(comp, hdr->data->reg_table[HDR_CURSOR_COLOR]);
	value[12] = read_reg_value(comp, hdr->data->reg_table[HDR_TILE_POS]);
	value[13] = read_reg_value(comp, hdr->data->reg_table[HDR_CURSOR_BUF0]);
	value[14] = read_reg_value(comp, hdr->data->reg_table[HDR_CURSOR_BUF1]);
	value[15] = read_reg_value(comp, hdr->data->reg_table[HDR_CURSOR_BUF2]);

	mml_err("HDR_TOP %#010x HDR_RELAY %#010x HDR_INTSTA %#010x HDR_ENGSTA %#010x",
		value[0], value[1], value[2], value[3]);
	mml_err("HDR_SIZE_0 %#010x HDR_SIZE_1 %#010x HDR_SIZE_2 %#010x",
		value[4], value[5], value[6]);
	mml_err("HDR_HIST_CTRL_0 %#010x HDR_HIST_CTRL_1 %#010x",
		value[7], value[8]);
	mml_err("HDR_CURSOR_CTRL %#010x HDR_CURSOR_POS %#010x HDR_CURSOR_COLOR %#010x",
		value[9], value[10], value[11]);
	mml_err("HDR_TILE_POS %#010x", value[12]);
	mml_err("HDR_CURSOR_BUF0 %#010x HDR_CURSOR_BUF1 %#010x HDR_CURSOR_BUF2 %#010x",
		value[13], value[14], value[15]);
}

static const struct mml_comp_debug_ops hdr_debug_ops = {
	.dump = &hdr_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_hdr *hdr = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	s32 ret;

	if (!drm_dev) {
		ret = mml_register_comp(master, &hdr->comp);
		if (ret)
			dev_err(dev, "Failed to register mml component %s: %d\n",
				dev->of_node->full_name, ret);
		hdr->mml = dev_get_drvdata(master);
	} else {
		ret = mml_ddp_comp_register(drm_dev, &hdr->ddp_comp);
		if (ret)
			dev_err(dev, "Failed to register ddp component %s: %d\n",
				dev->of_node->full_name, ret);
		else
			hdr->ddp_bound = true;
	}
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_hdr *hdr = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	if (!drm_dev) {
		mml_unregister_comp(master, &hdr->comp);
	} else {
		mml_ddp_comp_unregister(drm_dev, &hdr->ddp_comp);
		hdr->ddp_bound = false;
	}
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static const struct mtk_ddp_comp_funcs ddp_comp_funcs = {
};

static struct mml_comp_hdr *dbg_probed_components[2];
static int dbg_probed_count;

static void hdr_curve_work(struct work_struct *work_item)
{
	struct mml_comp_hdr *hdr = NULL;
	struct mml_comp *comp = NULL;
	void __iomem *base = NULL;
	u32 *curve = NULL;
	struct cmdq_pkt *pkt = NULL;
	phys_addr_t base_pa = 0;
	u32 i = 0;
	u32 gpr = 0;

	hdr = container_of(work_item, struct mml_comp_hdr, hdr_curve_task);

	if (IS_ERR_OR_NULL(hdr) || IS_ERR_OR_NULL(&hdr->comp) || IS_ERR_OR_NULL(&hdr->comp.base)
		|| IS_ERR_OR_NULL(hdr->gain_curve)) {
		mml_pq_log("%s Has NULL hdr[%p] comp[%p] base[%p] curve[%p]",
			__func__, hdr, &hdr->comp, &hdr->comp.base, hdr->gain_curve);
		complete_all(&hdr->curve_pq_task->hdr_curve_ready[hdr->pipe]);
		return;
	}

	comp = &hdr->comp;
	base = comp->base;
	curve = hdr->gain_curve;
	pkt = cmdq_pkt_create(hdr->clts);
	base_pa = comp->base_pa;
	gpr = hdr->data->gpr[hdr->pipe];

	cmdq_pkt_poll(pkt, NULL, (0x0 << 11),
		base_pa + hdr->data->reg_table[HDR_GAIN_TABLE_0],
		(0x0 << 11), gpr);

	for (i = 0; i < HDR_CURVE_NUM; i += 2) {
		cmdq_pkt_write(pkt, NULL, base_pa + hdr->data->reg_table[HDR_GAIN_TABLE_1],
			curve[i], U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + hdr->data->reg_table[HDR_GAIN_TABLE_2],
			curve[i+1], U32_MAX);
	}

	cmdq_pkt_write(pkt, NULL,
		base_pa + hdr->data->reg_table[HDR_GAIN_TABLE_0], 1 << 11, 1 << 11);

	cmdq_pkt_flush(pkt);
	cmdq_pkt_destroy(pkt);

	complete_all(&hdr->curve_pq_task->hdr_curve_ready[hdr->pipe]);
	mml_pq_ir_log("%s end jobid[%d] hdr[%p] comp[%p] base[%p] curve[%p] pipe[%d]",
			__func__, hdr->jobid, hdr, comp, base, curve, hdr->pipe);
}

static void hdr_ir_histogram_check(struct mml_comp_hdr *hdr)
{
	u8 pipe = hdr->pipe;

	u32 i = 0, sum = 0;
	u32 expect_value_letter = 0, expect_value_crop = 0, expect_value_hist = 0;
	u32 letter_up = 0, letter_down = 0, letter_height = 0;
	u32 crop_width = hdr->crop_width;
	u32 crop_height = hdr->crop_height;
	u32 hist_up = 0, hist_down = 0, hist_height = 0;

	for (i = 0; i < HDR_HIST_CNT; i++)
		sum = sum + hdr->hdr_hist[pipe]->va[i];

	letter_up = ((hdr->hdr_hist[pipe]->va[57] &
		0x0000FFFF) >> 0);
	letter_down = ((hdr->hdr_hist[pipe]->va[57] &
		0xFFFF0000) >> 16);
	letter_height = letter_down - letter_up;

	hist_up = ((hdr->hdr_hist[pipe]->va[58] &
		0xFFFF0000) >> 16);
	hist_down = ((hdr->hdr_hist[pipe]->va[59] &
		0xFFFF0000) >> 16);

	hist_height = hist_down - hist_up;

	if (hdr->dual) {
		if (pipe) {
			expect_value_hist = (hist_height+1) *
				(crop_width - hdr->cut_pos_x) * 8;
			expect_value_letter = (letter_height+1) *
				(crop_width - hdr->cut_pos_x) * 8;
			expect_value_crop = crop_height *
				(crop_width - hdr->cut_pos_x) * 8;
		} else {
			expect_value_hist =
				(hist_height+1) * hdr->cut_pos_x * 8;
			expect_value_letter =
				(letter_height+1) * hdr->cut_pos_x * 8;
			expect_value_crop =
				crop_height * hdr->cut_pos_x * 8;
		}
	} else {
		expect_value_letter =
			(letter_height+1) * crop_width * 8;
		expect_value_crop = crop_height * crop_width * 8;
	}

	mml_pq_rb_msg("%s job_id[%d] pipe[%d] sum[%u] expect_value[%u %u %u]",
		__func__, hdr->jobid, pipe, sum, expect_value_letter,
		expect_value_crop, expect_value_hist);
	mml_pq_rb_msg("%s job_id[%d] pipe[%d] letter_height[%d] down[%d] up[%d]",
		__func__, hdr->jobid, pipe, letter_height, letter_down,
		letter_up);

	if (sum != expect_value_letter &&
		sum != expect_value_crop &&
		sum != expect_value_hist) {
		mml_pq_err("%s sum[%u] expect_value_letter[%u] job_id[%d] pipe[%d] cut_pos_x[%d]",
			__func__, sum, expect_value_letter, hdr->jobid, pipe,
			hdr->cut_pos_x);
		for (i = 0; i < HDR_HIST_CNT-1; i += 4)
			mml_pq_err("%s hist[%d - %d] = [%d, %d, %d, %d]",
				__func__, i, i+3,
				hdr->hdr_hist[pipe]->va[i],
				hdr->hdr_hist[pipe]->va[i+1],
				hdr->hdr_hist[pipe]->va[i+2],
				hdr->hdr_hist[pipe]->va[i+3]);

			mml_pq_err("%s hist[56-57] = [%d, %x]",
				__func__, hdr->hdr_hist[pipe]->va[56],
				hdr->hdr_hist[pipe]->va[57]);

			if (mml_pq_debug_mode & MML_PQ_HIST_CHECK)
				mml_pq_util_aee("MML_PQ_HDR_Histogram Error",
					"HDR Histogram error need to check jobid:%d",
					hdr->jobid);
	}
}


static void hdr_histdone_cb(struct cmdq_cb_data data)
{
	struct cmdq_pkt *pkt = (struct cmdq_pkt *)data.data;
	struct mml_comp_hdr *hdr = (struct mml_comp_hdr *)pkt->user_data;
	struct mml_comp *comp = &hdr->comp;
	u32 pipe;
	u32 offset = 0;
	u32 sum = 0;
	u32 i = 0;


	mml_pq_ir_log("%s jobid[%d] hdr->hist_pkts[0] = [%p] hdr->hist_pkts[1] = [%p]",
		__func__, hdr->jobid, hdr->hist_pkts[0], hdr->hist_pkts[1]);

	if (mml_pq_hdr_hist_reading(hdr->pq_task, hdr->out_idx, hdr->pipe))
		return;

	if (pkt == hdr->hist_pkts[0]) {
		pipe = 0;
	} else if (pkt == hdr->hist_pkts[1]) {
		pipe = 1;
	} else {
		mml_err("%s task %p pkt %p not match both pipe (%p and %p)",
			__func__, hdr, pkt, hdr->hist_pkts[0], hdr->hist_pkts[1]);
		return;
	}

	for (i = 0; i < HDR_HIST_CNT; i++)
		sum = sum + hdr->hdr_hist[pipe]->va[offset/4+i];

	mml_pq_ir_log("%s hist[0~4]={%08x, %08x, %08x, %08x, %08x} hist[57]=[%08x]",
		__func__,
		hdr->hdr_hist[pipe]->va[offset/4+0],
		hdr->hdr_hist[pipe]->va[offset/4+1],
		hdr->hdr_hist[pipe]->va[offset/4+2],
		hdr->hdr_hist[pipe]->va[offset/4+3],
		hdr->hdr_hist[pipe]->va[offset/4+4],
		hdr->hdr_hist[pipe]->va[offset/4+57]);

	mml_pq_ir_log("%s hist[10~14]={%08x, %08x, %08x, %08x, %08x} sum =[%d]",
		__func__,
		hdr->hdr_hist[pipe]->va[offset/4+10],
		hdr->hdr_hist[pipe]->va[offset/4+11],
		hdr->hdr_hist[pipe]->va[offset/4+12],
		hdr->hdr_hist[pipe]->va[offset/4+13],
		hdr->hdr_hist[pipe]->va[offset/4+14],
		sum);


	mml_pq_ir_log("%s jobid[%d] task[%p] pq_task[%p] pq_ref[%d] pipe[%d]", __func__,
		hdr->jobid, hdr->pq_task->task, hdr->pq_task,
		kref_read(&hdr->pq_task->ref),
		hdr->pipe);

	if ((mml_pq_debug_mode & MML_PQ_HIST_CHECK))
		hdr_ir_histogram_check(hdr);

	mml_pq_ir_hdr_readback(hdr->pq_task, hdr->frame_data, hdr->pipe,
			&(hdr->hdr_hist[pipe]->va[offset/4]), hdr->jobid,
			hdr->dual);

	if (hdr->jobid == 1) {
		mml_pq_err("%s sum[%u] job_id[%d] pipe[%d] cut_pos_x[%d]",
			__func__, sum, hdr->jobid, pipe,
			hdr->cut_pos_x);
		for (i = 0; i < HDR_HIST_CNT-1; i += 4)
			mml_pq_err("%s hist[%d - %d] = [%d, %d, %d, %d]",
				__func__, i, i+3,
				hdr->hdr_hist[pipe]->va[i],
				hdr->hdr_hist[pipe]->va[i+1],
				hdr->hdr_hist[pipe]->va[i+2],
				hdr->hdr_hist[pipe]->va[i+3]);

			mml_pq_err("%s hist[56-57] = [%d, %x]",
				__func__, hdr->hdr_hist[pipe]->va[56],
				hdr->hdr_hist[pipe]->va[57]);
	}

	if (hdr->data->rb_mode == RB_EOF_MODE) {
		mml_clock_lock(hdr->mml);
		call_hw_op(comp, clk_disable, hdr->dpc);
		/* dpc exception flow off */
		mml_msg_dpc("%s dpc exception flow off", __func__);
		mml_dpc_exc_release(hdr->mml);
		/* ccf power off */
		call_hw_op(hdr->mmlsys_comp, pw_disable);
		call_hw_op(hdr->mmlsys_comp, mminfra_pw_disable);
		mml_clock_unlock(hdr->mml);
		mml_lock_wake_lock(hdr->mml, false);
	}

	mml_pq_put_pq_task(hdr->pq_task);

	mml_pq_hdr_flag_check(hdr->dual, hdr->out_idx);


	mml_pq_ir_log("%s end jobid[%d] pkt[%p] hdr[%p] pipe[%d] pa[%llx] va[%p]",
		__func__, hdr->jobid, pkt, hdr, pipe,
		hdr->hdr_hist[pipe]->pa,
		hdr->hdr_hist[pipe]->va);

	mml_trace_end();
}


static void hdr_hist_work(struct work_struct *work_item)
{
	struct mml_comp_hdr *hdr = NULL;
	struct mml_comp *comp = NULL;
	struct cmdq_pkt *pkt = NULL;
	struct cmdq_operand lop, rop;

	const u16 idx_counter = CMDQ_THR_SPR_IDX1;
	const u16 idx_val = CMDQ_THR_SPR_IDX2;
	u16 idx_out = 0;
	u16 idx_out64 = 0;

	u32 begin_offset = 0;
	u32 condi_offset = 0;
	u8 pipe = 0;
	phys_addr_t base_pa = 0;
	u32 *condi_inst = NULL;
	dma_addr_t begin_pa = 0;
	dma_addr_t pa = 0;

	hdr = container_of(work_item, struct mml_comp_hdr, hdr_hist_task);

	if (!hdr) {
		mml_pq_err("%s comp_hdr is null", __func__);
		return;
	}

	pipe = hdr->pipe;
	comp = &hdr->comp;
	base_pa = comp->base_pa;
	pkt = hdr->hist_pkts[pipe];
	idx_out = hdr->data->cpr[hdr->pipe];

	mml_pq_ir_log("%s jobid[%d] pkt[%p] hdr[%p] pipe[%d] pa[%llx] va[%p]",
		__func__, hdr->jobid, pkt, hdr, pipe, hdr->hdr_hist[pipe]->pa,
		hdr->hdr_hist[pipe]->va);

	mml_pq_ir_log("%s job_id[%d] eng_id[%d] cmd_buf_size[%zu] hist_cmd_done[%d] pipe[%d]",
		__func__, hdr->jobid, comp->id,
		pkt->cmd_buf_size, hdr->hist_cmd_done,
		pipe);

	idx_out64 = CMDQ_CPR_TO_CPR64(idx_out);

	if (unlikely(!hdr->hdr_hist[pipe])) {
		mml_pq_err("%s job_id[%d] eng_id[%d] pipe[%d] pkt[%p] hdr_hist is null",
			__func__, hdr->jobid, comp->id, pipe,
			hdr->hist_pkts[pipe]);
		return;
	}

	mutex_lock(&hdr->hist_cmd_lock);
	if (hdr->hist_cmd_done) {
		mutex_unlock(&hdr->hist_cmd_lock);
		goto hdr_hist_cmd_done;
	}

	cmdq_pkt_wfe(pkt, hdr->event_eof);

	pa = hdr->hdr_hist[pipe]->pa;

	/* readback to this pa */
	cmdq_pkt_assign_command(pkt, idx_out, (u32)pa);
	cmdq_pkt_assign_command(pkt, idx_out + 1, (u32)(pa >> 32));

	/* counter init to 0 */
	cmdq_pkt_assign_command(pkt, idx_counter, 0);

	/* loop again here */
	begin_offset = pkt->cmd_buf_size;
	begin_pa = cmdq_pkt_get_pa_by_offset(pkt, begin_offset);

	/* read to value cpr */
	cmdq_pkt_read_addr(pkt, base_pa + hdr->data->reg_table[HDR_HIST_DATA], idx_val);
	/* write value spr to dst cpr */
	cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

	/* jump forward end if match, if spr1 >= 57 - 1	*/
	cmdq_pkt_assign_command(pkt, CMDQ_THR_SPR_IDX0, 0);
	condi_offset = pkt->cmd_buf_size - CMDQ_INST_SIZE;

	/* inc counter */
	lop.reg = true;
	lop.idx = idx_counter;
	rop.reg = false;
	rop.value = 1;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_counter, &lop, &rop);
	/* inc outut pa */
	lop.reg = true;
	lop.idx = idx_out;
	rop.reg = false;
	rop.value = 4;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);


	lop.reg = true;
	lop.idx = idx_counter;
	rop.reg = false;
	rop.value =  HDR_HIST_CNT - 1;
	cmdq_pkt_cond_jump_abs(pkt, CMDQ_THR_SPR_IDX0, &lop, &rop,
		CMDQ_LESS_THAN_AND_EQUAL);
	condi_inst = (u32 *)cmdq_pkt_get_va_by_offset(pkt, condi_offset);
	if (unlikely(!condi_inst))
		mml_pq_err("%s wrong offset %u\n", __func__, condi_offset);

	*condi_inst = (u32)CMDQ_REG_SHIFT_ADDR(begin_pa);

	/* read to value cpr */
	cmdq_pkt_read_addr(pkt, base_pa + hdr->data->reg_table[HDR_LBOX_DET_4], idx_val);
	/* write value spr to dst cpr */
	cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

	if ((mml_pq_debug_mode & MML_PQ_HIST_CHECK)) {
		lop.reg = true;
		lop.idx = idx_out;
		rop.reg = false;
		rop.value = 4;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);

		cmdq_pkt_read_addr(pkt, base_pa + hdr->data->reg_table[HDR_HIST_CTRL_0], idx_val);
		/* write value spr to dst cpr */
		cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

		lop.reg = true;
		lop.idx = idx_out;
		rop.reg = false;
		rop.value = 4;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);

		cmdq_pkt_read_addr(pkt, base_pa + hdr->data->reg_table[HDR_HIST_CTRL_1], idx_val);
		/* write value spr to dst cpr */
		cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);
	}
	hdr->hist_cmd_done = true;

	mml_pq_rb_msg("%s end engine_id[%d] va[%p] pa[%llx] pkt[%p]",
		__func__, comp->id, hdr->hdr_hist[pipe]->va,
		hdr->hdr_hist[pipe]->pa, pkt);

	mml_pq_rb_msg("%s end condi:offset[%u] inst[%p], begin:offset[%u] pa[%llx]",
		__func__, condi_offset, condi_inst,
		begin_offset, begin_pa);

	pkt->user_data = hdr;

	mutex_unlock(&hdr->hist_cmd_lock);

hdr_hist_cmd_done:

	wait_for_completion(&hdr->pq_task->hdr_curve_ready[hdr->pipe]);

	cmdq_pkt_refinalize(pkt);
	cmdq_pkt_flush_threaded(pkt, hdr_histdone_cb, (void *)hdr->hist_pkts[pipe]);

	mml_pq_ir_log("%s job_id[%d] hist_pkts[%p %p] id[%d] buf_size[%zu] hist_cmd_done[%d]",
		__func__, hdr->jobid,
		hdr->hist_pkts[0], hdr->hist_pkts[1], comp->id,
		pkt->cmd_buf_size, hdr->hist_cmd_done);

}


static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_hdr *priv;
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

	/* assign ops */
	priv->comp.tile_ops = &hdr_tile_ops;
	priv->comp.config_ops = &hdr_cfg_ops;
	priv->comp.hw_ops = &hdr_hw_ops;
	priv->comp.debug_ops = &hdr_debug_ops;

	priv->hdr_curve_wq = create_singlethread_workqueue("hdr_curve_write");
	INIT_WORK(&priv->hdr_curve_task, hdr_curve_work);

	priv->hdr_hist_wq = create_singlethread_workqueue("hdr_hist_read");
	INIT_WORK(&priv->hdr_hist_task, hdr_hist_work);

	priv->gain_curve = kmalloc_array(HDR_CURVE_NUM, sizeof(u32),
				  GFP_KERNEL);

	mutex_init(&priv->hist_cmd_lock);

	if (of_property_read_u16(dev->of_node, "event-frame-done",
				 &priv->event_eof))
		dev_err(dev, "read event frame_done fail\n");

	if (of_property_read_u16(dev->of_node, "event-mutex-hint",
				 &priv->mutex_hint))
		dev_err(dev, "read event mutex_hint fail\n");

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

const struct of_device_id mml_hdr_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6983-mml_hdr",
		.data = &mt6983_hdr_data,
	},
	{
		.compatible = "mediatek,mt6893-mml_hdr",
		.data = &mt6983_hdr_data,
	},
	{
		.compatible = "mediatek,mt6879-mml_hdr",
		.data = &mt6983_hdr_data,
	},
	{
		.compatible = "mediatek,mt6895-mml_hdr",
		.data = &mt6895_hdr_data,
	},
	{
		.compatible = "mediatek,mt6985-mml_hdr",
		.data = &mt6985_hdr_data,
	},
	{
		.compatible = "mediatek,mt6886-mml_hdr",
		.data = &mt6983_hdr_data,
	},
	{
		.compatible = "mediatek,mt6897-mml_hdr",
		.data = &mt6985_hdr_data,
	},
	{
		.compatible = "mediatek,mt6989-mml_hdr",
		.data = &mt6985_hdr_data,
	},
	{
		.compatible = "mediatek,mt6878-mml_hdr",
		.data = &mt6983_hdr_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_hdr_driver_dt_match);

struct platform_driver mml_hdr_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-hdr",
		.owner = THIS_MODULE,
		.of_match_table = mml_hdr_driver_dt_match,
	},
};

//module_platform_driver(mml_hdr_driver);

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
module_param_cb(hdr_debug, &dbg_param_ops, NULL, 0644);
MODULE_PARM_DESC(hdr_debug, "mml hdr debug case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML HDR driver");
MODULE_LICENSE("GPL v2");
