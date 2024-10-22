// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <mtk_drm_ddp_comp.h>
#include <linux/of_irq.h>

#include "mtk-mml-color.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-dle-adaptor.h"
#include "mtk-mml-pq-core.h"

#include "tile_driver.h"
#include "mtk-mml-tile.h"
#include "tile_mdp_func.h"
#include "mtk-mml-mmp.h"
#include <soc/mediatek/smi.h>
#include <slbc_ops.h>

#undef pr_fmt
#define pr_fmt(fmt) "[mml_pq_aal]" fmt

#define AAL_WAIT_TIMEOUT_MS	(50)
#define AAL_POLL_SLEEP_TIME_US	(10)
#define AAL_MAX_POLL_TIME_US	(1000)
#define MAX_SRAM_BUF_NUM (2)

#define AAL_SRAM_STATUS_BIT	BIT(17)
#define AAL_HIST_MAX_SUM (300)
#define REG_NOT_SUPPORT 0xfff
#define BLK_WIDTH_DEFAULT (120)
#define BLK_HEIGH_DEFAULT (135)
#define MIN_HIST_CHECK_SRC_WIDTH (480)
#define MAX_HIST_CHECK_BLK_X_NUM (16)
#define MIN_HIST_CHECK_BLK_X_NUM (1)
#define MAX_HIST_CHECK_BLK_Y_NUM (8)
#define MIN_HIST_CHECK_BLK_Y_NUM (1)
#define MIN_HIST_CHECK_BLK_HEIGHT (80)

/* min of label count for aal curve
 *	(AAL_CURVE_NUM * 7 / CMDQ_NUM_CMD(CMDQ_CMD_BUFFER_SIZE) + 1)
 */
#define AAL_LABEL_CNT		10
#define AAL_IR_LABEL_CNT	4

#define call_hw_op(_comp, op, ...) \
	(_comp->hw_ops->op ? _comp->hw_ops->op(_comp, ##__VA_ARGS__) : 0)

enum mml_aal_reg_index {
	AAL_EN,
	AAL_INTEN,
	AAL_INTSTA,
	AAL_STATUS,
	AAL_CFG,
	AAL_INPUT_COUNT,
	AAL_OUTPUT_COUNT,
	AAL_SIZE,
	AAL_OUTPUT_SIZE,
	AAL_OUTPUT_OFFSET,
	AAL_SRAM_CFG,
	AAL_SRAM_STATUS,
	AAL_SRAM_RW_IF_0, /* sram curve addr */
	AAL_SRAM_RW_IF_1, /* sram curve value */
	AAL_SRAM_RW_IF_2,
	AAL_SRAM_RW_IF_3,
	AAL_SHADOW_CTRL,
	AAL_TILE_02,
	AAL_DRE_BLOCK_INFO_07,
	AAL_CFG_MAIN,
	AAL_WIN_X_MAIN,
	AAL_WIN_Y_MAIN,
	AAL_DRE_BLOCK_INFO_00,
	AAL_TILE_00,
	AAL_TILE_01,
	AAL_DUAL_PIPE_00,
	AAL_DUAL_PIPE_01,
	AAL_DUAL_PIPE_02,
	AAL_DUAL_PIPE_03,
	AAL_DUAL_PIPE_04,
	AAL_DUAL_PIPE_05,
	AAL_DUAL_PIPE_06,
	AAL_DUAL_PIPE_07,
	AAL_DRE_ROI_00,
	AAL_DRE_ROI_01,
	AAL_DUAL_PIPE_08,
	AAL_DUAL_PIPE_09,
	AAL_DUAL_PIPE_10,
	AAL_DUAL_PIPE_11,
	AAL_DUAL_PIPE_12,
	AAL_DUAL_PIPE_13,
	AAL_DUAL_PIPE_14,
	AAL_DUAL_PIPE_15,
	AAL_BILATERAL_STATUS_00,
	AAL_BILATERAL_STATUS_CTRL,
	AAL_REG_MAX_COUNT
};

static const u16 aal_reg_table_mt6983[AAL_REG_MAX_COUNT] = {
	[AAL_EN] = 0x000,
	[AAL_INTEN] = 0x008,
	[AAL_INTSTA] = 0x00c,
	[AAL_STATUS] = 0x010,
	[AAL_CFG] = 0x020,
	[AAL_INPUT_COUNT] = 0x024,
	[AAL_OUTPUT_COUNT] = 0x028,
	[AAL_SIZE] = 0x030,
	[AAL_OUTPUT_SIZE] = 0x034,
	[AAL_OUTPUT_OFFSET] = 0x038,
	[AAL_SRAM_CFG] = 0x0c4,
	[AAL_SRAM_STATUS] = 0x0c8,
	[AAL_SRAM_RW_IF_0] = 0x0cc,
	[AAL_SRAM_RW_IF_1] = 0x0d0,
	[AAL_SRAM_RW_IF_2] = 0x0d4,
	[AAL_SRAM_RW_IF_3] = 0x0d8,
	[AAL_SHADOW_CTRL] = 0x0f0,
	[AAL_TILE_02] = 0x0f4,
	[AAL_DRE_BLOCK_INFO_07] = 0x0f8,
	[AAL_CFG_MAIN] = 0x200,
	[AAL_WIN_X_MAIN] = 0x460,
	[AAL_WIN_Y_MAIN] = 0x464,
	[AAL_DRE_BLOCK_INFO_00] = 0x468,
	[AAL_TILE_00] = 0x4ec,
	[AAL_TILE_01] = 0x4f0,
	[AAL_DUAL_PIPE_00] = 0x500,
	[AAL_DUAL_PIPE_01] = 0x504,
	[AAL_DUAL_PIPE_02] = 0x508,
	[AAL_DUAL_PIPE_03] = 0x50c,
	[AAL_DUAL_PIPE_04] = 0x510,
	[AAL_DUAL_PIPE_05] = 0x514,
	[AAL_DUAL_PIPE_06] = 0x518,
	[AAL_DUAL_PIPE_07] = 0x51c,
	[AAL_DRE_ROI_00] = 0x520,
	[AAL_DRE_ROI_01] = 0x524,
	[AAL_DUAL_PIPE_08] = 0x544,
	[AAL_DUAL_PIPE_09] = 0x548,
	[AAL_DUAL_PIPE_10] = 0x54c,
	[AAL_DUAL_PIPE_11] = 0x550,
	[AAL_DUAL_PIPE_12] = 0x554,
	[AAL_DUAL_PIPE_13] = 0x558,
	[AAL_DUAL_PIPE_14] = 0x55c,
	[AAL_DUAL_PIPE_15] = 0x560,
	[AAL_BILATERAL_STATUS_00] = REG_NOT_SUPPORT,
	[AAL_BILATERAL_STATUS_CTRL] = REG_NOT_SUPPORT
};

static const u16 aal_reg_table_mt6985[AAL_REG_MAX_COUNT] = {
	[AAL_EN] = 0x000,
	[AAL_INTEN] = 0x008,
	[AAL_INTSTA] = 0x00c,
	[AAL_STATUS] = 0x010,
	[AAL_CFG] = 0x020,
	[AAL_INPUT_COUNT] = 0x024,
	[AAL_OUTPUT_COUNT] = 0x028,
	[AAL_SIZE] = 0x030,
	[AAL_OUTPUT_SIZE] = 0x034,
	[AAL_OUTPUT_OFFSET] = 0x038,
	[AAL_SRAM_CFG] = 0x0c4,
	[AAL_SRAM_STATUS] = 0x0c8,
	[AAL_SRAM_RW_IF_0] = 0x0cc,
	[AAL_SRAM_RW_IF_1] = 0x0d0,
	[AAL_SRAM_RW_IF_2] = 0x0d4,
	[AAL_SRAM_RW_IF_3] = 0x0d8,
	[AAL_SHADOW_CTRL] = 0x0f0,
	[AAL_TILE_02] = 0x0f4,
	[AAL_DRE_BLOCK_INFO_07] = 0x0f8,
	[AAL_CFG_MAIN] = 0x200,
	[AAL_WIN_X_MAIN] = 0x460,
	[AAL_WIN_Y_MAIN] = 0x464,
	[AAL_DRE_BLOCK_INFO_00] = 0x468,
	[AAL_TILE_00] = 0x4ec,
	[AAL_TILE_01] = 0x4f0,
	[AAL_DUAL_PIPE_00] = 0x500,
	[AAL_DUAL_PIPE_01] = 0x504,
	[AAL_DUAL_PIPE_02] = 0x508,
	[AAL_DUAL_PIPE_03] = 0x50c,
	[AAL_DUAL_PIPE_04] = 0x510,
	[AAL_DUAL_PIPE_05] = 0x514,
	[AAL_DUAL_PIPE_06] = 0x518,
	[AAL_DUAL_PIPE_07] = 0x51c,
	[AAL_DRE_ROI_00] = 0x520,
	[AAL_DRE_ROI_01] = 0x524,
	[AAL_DUAL_PIPE_08] = 0x544,
	[AAL_DUAL_PIPE_09] = 0x548,
	[AAL_DUAL_PIPE_10] = 0x54c,
	[AAL_DUAL_PIPE_11] = 0x550,
	[AAL_DUAL_PIPE_12] = 0x554,
	[AAL_DUAL_PIPE_13] = 0x558,
	[AAL_DUAL_PIPE_14] = 0x55c,
	[AAL_DUAL_PIPE_15] = 0x560,
	[AAL_BILATERAL_STATUS_00] = 0x588,
	[AAL_BILATERAL_STATUS_CTRL] = 0x5b8
};

static const u16 aal_reg_table_mt6897[AAL_REG_MAX_COUNT] = {
	[AAL_EN] = 0x000,
	[AAL_INTEN] = 0x008,
	[AAL_INTSTA] = 0x00c,
	[AAL_STATUS] = 0x010,
	[AAL_CFG] = 0x020,
	[AAL_INPUT_COUNT] = 0x024,
	[AAL_OUTPUT_COUNT] = 0x028,
	[AAL_SIZE] = 0x030,
	[AAL_OUTPUT_SIZE] = 0x034,
	[AAL_OUTPUT_OFFSET] = 0x038,
	[AAL_SRAM_CFG] = 0x0c4,
	[AAL_SRAM_STATUS] = 0x0c8,
	[AAL_SRAM_RW_IF_0] = 0x690,
	[AAL_SRAM_RW_IF_1] = 0x694,
	[AAL_SRAM_RW_IF_2] = 0x0d4,
	[AAL_SRAM_RW_IF_3] = 0x0d8,
	[AAL_SHADOW_CTRL] = 0x0f0,
	[AAL_TILE_02] = 0x0f4,
	[AAL_DRE_BLOCK_INFO_07] = 0x0f8,
	[AAL_CFG_MAIN] = 0x200,
	[AAL_WIN_X_MAIN] = 0x460,
	[AAL_WIN_Y_MAIN] = 0x464,
	[AAL_DRE_BLOCK_INFO_00] = 0x468,
	[AAL_TILE_00] = 0x4ec,
	[AAL_TILE_01] = 0x4f0,
	[AAL_DUAL_PIPE_00] = 0x500,
	[AAL_DUAL_PIPE_01] = 0x504,
	[AAL_DUAL_PIPE_02] = 0x508,
	[AAL_DUAL_PIPE_03] = 0x50c,
	[AAL_DUAL_PIPE_04] = 0x510,
	[AAL_DUAL_PIPE_05] = 0x514,
	[AAL_DUAL_PIPE_06] = 0x518,
	[AAL_DUAL_PIPE_07] = 0x51c,
	[AAL_DRE_ROI_00] = 0x520,
	[AAL_DRE_ROI_01] = 0x524,
	[AAL_DUAL_PIPE_08] = 0x544,
	[AAL_DUAL_PIPE_09] = 0x548,
	[AAL_DUAL_PIPE_10] = 0x54c,
	[AAL_DUAL_PIPE_11] = 0x550,
	[AAL_DUAL_PIPE_12] = 0x554,
	[AAL_DUAL_PIPE_13] = 0x558,
	[AAL_DUAL_PIPE_14] = 0x55c,
	[AAL_DUAL_PIPE_15] = 0x560,
	[AAL_BILATERAL_STATUS_00] = 0x588,
	[AAL_BILATERAL_STATUS_CTRL] = 0x5b8
};

struct aal_data {
	u32 min_tile_width;
	u32 tile_width;
	u32 min_hist_width;
	bool vcp_readback;
	u16 gpr[MML_PIPE_CNT];
	u16 cpr[MML_PIPE_CNT];
	const u16 *reg_table;
	bool crop;
	bool alpha_pq_r2y;
	u8 rb_mode;
	bool is_linear;
	u8 curve_ready_bit;
};

static const struct aal_data mt6893_aal_data = {
	.min_tile_width = 50,
	.tile_width = 560,
	.min_hist_width = 128,
	.vcp_readback = false,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = aal_reg_table_mt6983,
	.crop = true,
	.rb_mode = RB_EOF_MODE,
	.is_linear = false,
	.curve_ready_bit = 16,
};

static const struct aal_data mt6983_aal_data = {
	.min_tile_width = 50,
	.tile_width = 1652,
	.min_hist_width = 128,
	.vcp_readback = false,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = aal_reg_table_mt6983,
	.crop = true,
	.rb_mode = RB_EOF_MODE,
	.is_linear = false,
	.curve_ready_bit = 16,
};

static const struct aal_data mt6879_aal_data = {
	.min_tile_width = 50,
	.tile_width = 1376,
	.min_hist_width = 128,
	.vcp_readback = false,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = aal_reg_table_mt6983,
	.crop = true,
	.rb_mode = RB_EOF_MODE,
	.is_linear = false,
	.curve_ready_bit = 16,
};

static const struct aal_data mt6895_aal0_data = {
	.min_tile_width = 50,
	.tile_width = 1300,
	.min_hist_width = 128,
	.vcp_readback = true,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = aal_reg_table_mt6983,
	.crop = true,
	.rb_mode = RB_EOF_MODE,
	.is_linear = false,
	.curve_ready_bit = 16,
};

static const struct aal_data mt6895_aal1_data = {
	.min_tile_width = 50,
	.tile_width = 852,
	.min_hist_width = 128,
	.vcp_readback = true,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = aal_reg_table_mt6983,
	.crop = true,
	.rb_mode = RB_EOF_MODE,
	.is_linear = false,
	.curve_ready_bit = 16,
};

static const struct aal_data mt6985_aal_data = {
	.min_tile_width = 50,
	.tile_width = 1690,
	.min_hist_width = 128,
	.vcp_readback = false,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = aal_reg_table_mt6985,
	.crop = false,
	.rb_mode = RB_EOF_MODE,
	.is_linear = true,
	.curve_ready_bit = 16,
};

static const struct aal_data mt6886_aal_data = {
	.min_tile_width = 50,
	.tile_width = 1300,
	.min_hist_width = 128,
	.vcp_readback = false,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = aal_reg_table_mt6983,
	.crop = true,
	.rb_mode = RB_EOF_MODE,
	.is_linear = false,
	.curve_ready_bit = 16,
};

static const struct aal_data mt6897_aal_data = {
	.min_tile_width = 50,
	.tile_width = 1690,
	.min_hist_width = 128,
	.vcp_readback = false,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = aal_reg_table_mt6897,
	.crop = true,
	.rb_mode = RB_EOF_MODE,
	.is_linear = false,
	.curve_ready_bit = 18,
};

static const struct aal_data mt6989_aal_data = {
	.min_tile_width = 50,
	.tile_width = 3380,
	.min_hist_width = 128,
	.vcp_readback = false,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = aal_reg_table_mt6897,
	.crop = false,
	.alpha_pq_r2y = true,
	.rb_mode = RB_EOF_MODE,
	.is_linear = false,
	.curve_ready_bit = 18,
};

static const struct aal_data mt6878_aal_data = {
	.min_tile_width = 50,
	.tile_width = 560,
	.min_hist_width = 128,
	.vcp_readback = false,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = aal_reg_table_mt6983,
	.crop = true,
	.rb_mode = RB_EOF_MODE,
	.is_linear = false,
	.curve_ready_bit = 16,
};

struct mml_comp_aal {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	const struct aal_data *data;
	bool ddp_bound;
	bool dual;
	bool clk_on;
	bool dpc;
	struct mml_comp *mmlsys_comp;
	u32 jobid;
	u8 pipe;
	u8 out_idx;
	int irq;
	u32 *phist;
	u32 curve_sram_idx;
	u32 hist_sram_idx;
	u32 hist_read_idx;
	u16 event_eof;
	bool hist_cmd_done;
	struct mutex hist_cmd_lock;
	struct mml_pq_readback_buffer *clarity_hist[MML_PIPE_CNT];
	struct cmdq_client *clts[MML_PIPE_CNT];
	struct cmdq_pkt *hist_pkts[MML_PIPE_CNT];
	struct workqueue_struct *aal_readback_wq;
	struct work_struct aal_readback_task;
	struct workqueue_struct *clarity_hist_wq;
	struct work_struct clarity_hist_task;
	struct mml_pq_task *pq_task;
	struct mml_pq_frame_data frame_data;
	struct mml_dev *mml;
	struct mutex irq_wq_lock;
	struct mml_pq_config pq_config;

	u32 sram_curve_start;
	u32 sram_hist_start;
	s8 force_rb_mode;
	u32 dre_blk_width;
	u32 dre_blk_height;
	u32 cut_pos_x;
};

enum aal_label_index {
	AAL_REUSE_LABEL = 0,
	AAL_POLLGPR_0 = AAL_LABEL_CNT,
	AAL_POLLGPR_1,
	AAL_LABEL_TOTAL
};

/* meta data for each different frame config */
struct aal_frame_data {
	u32 out_hist_xs;
	u32 dre_blk_width;
	u32 dre_blk_height;
	u32 begin_offset;
	u32 condi_offset;
	u32 cut_pos_x;
	struct cmdq_poll_reuse polling_reuse;
	u16 labels[AAL_LABEL_TOTAL];
	struct mml_reuse_array reuse_curve;
	struct mml_reuse_offset offs_curve[AAL_LABEL_CNT];
	bool is_aal_need_readback;
	bool is_clarity_need_readback;
	bool config_success;
	bool relay_mode;
};

static inline struct aal_frame_data *aal_frm_data(struct mml_comp_config *ccfg)
{
	return ccfg->data;
}

static inline struct mml_comp_aal *comp_to_aal(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_aal, comp);
}

static s32 aal_prepare(struct mml_comp *comp, struct mml_task *task,
		       struct mml_comp_config *ccfg)
{
	const struct mml_frame_config *cfg = task->config;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	const struct mml_crop *crop = &cfg->frame_in_crop[ccfg->node->out_idx];
	struct aal_frame_data *aal_frm;
	struct mml_comp_aal *aal = comp_to_aal(comp);

	aal_frm = kzalloc(sizeof(*aal_frm), GFP_KERNEL);
	ccfg->data = aal_frm;
	aal_frm->reuse_curve.offs = aal_frm->offs_curve;
	aal_frm->reuse_curve.offs_size = ARRAY_SIZE(aal_frm->offs_curve);
	aal_frm->relay_mode = !dest->pq_config.en_dre ||
		crop->r.width < aal->data->min_tile_width;
	return 0;
}

static s32 aal_buf_prepare(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	const struct aal_frame_data *aal_frm = aal_frm_data(ccfg);
	s32 ret = 0;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s engine_id[%d] en_dre[%d]", __func__, comp->id,
		   !aal_frm->relay_mode);
	if (!aal_frm->relay_mode)
		ret = mml_pq_set_comp_config(task);
	mml_pq_trace_ex_end();
	return ret;
}

static s32 aal_tile_prepare(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg,
			    struct tile_func_block *func,
			    union mml_tile_data *data)
{
	const struct aal_frame_data *aal_frm = aal_frm_data(ccfg);
	const struct mml_frame_config *cfg = task->config;
	const struct mml_comp_aal *aal = comp_to_aal(comp);

	func->for_func = tile_aal_for;
	func->enable_flag = !aal_frm->relay_mode;

	func->full_size_x_in = cfg->frame_tile_sz.width;
	func->full_size_y_in = cfg->frame_tile_sz.height;
	func->full_size_x_out = func->full_size_x_in;
	func->full_size_y_out = func->full_size_y_in;

	func->in_tile_width = aal->data->tile_width;
	func->out_tile_width = aal->data->tile_width;
	func->in_tile_height = 65535;
	func->out_tile_height = 65535;

	if (aal->data->crop) {
		func->l_tile_loss = 8;
		func->r_tile_loss = 8;
	} else {
		func->l_tile_loss = 0;
		func->r_tile_loss = 0;
	}
	func->in_min_width = max(min(aal->data->min_hist_width,
				     (u32)func->full_size_x_in),
				 aal->data->min_tile_width);

	return 0;
}

static const struct mml_comp_tile_ops aal_tile_ops = {
	.prepare = aal_tile_prepare,
};

static u32 aal_get_label_count(struct mml_comp *comp, struct mml_task *task,
			       struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	u32 mode = cfg->info.mode;

	mml_pq_msg("%s engine_id[%d] en_dre[%d]", __func__, comp->id,
			dest->pq_config.en_dre);

	if (!dest->pq_config.en_dre || mode == MML_MODE_DDP_ADDON)
		return 0;

	return AAL_LABEL_TOTAL;
}

static void aal_init(struct mml_comp *comp, struct cmdq_pkt *pkt, const phys_addr_t base_pa)
{
	struct mml_comp_aal *aal = comp_to_aal(comp);

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_EN], 0x1, U32_MAX);
	/* Enable shadow */
	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_SHADOW_CTRL], 0x2, U32_MAX);
}

static void aal_relay(struct mml_comp *comp, struct cmdq_pkt *pkt, const phys_addr_t base_pa,
		      u32 relay)
{
	struct mml_comp_aal *aal = comp_to_aal(comp);

	/* 8	alpha_en
	 * 0	RELAY_MODE
	 */
	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_CFG], relay, 0x00000101);
}

static s32 aal_config_init(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	aal_init(comp, task->pkts[ccfg->pipe], comp->base_pa);
	return 0;
}

static struct mml_pq_comp_config_result *get_aal_comp_config_result(
	struct mml_task *task)
{
	return task->pq_task->comp_config.result;
}

static bool aal_reg_poll(struct mml_comp *comp, u32 addr, u32 value, u32 mask)
{
	bool return_value = false;
	u32 reg_value = 0;
	u32 polling_time = 0;
	void __iomem *base = comp->base;

	do {
		reg_value = readl(base + addr);

		if ((reg_value & mask) == value) {
			return_value = true;
			break;
		}

		udelay(AAL_POLL_SLEEP_TIME_US);
		polling_time += AAL_POLL_SLEEP_TIME_US;
	} while (polling_time < AAL_MAX_POLL_TIME_US);

	return return_value;
}

static s8 aal_get_rb_mode(struct mml_comp_aal *aal)
{
	mml_pq_msg("%s force_rb_mode[%d] rb_mode[%d]",
		__func__, aal->force_rb_mode, aal->data->rb_mode);

	if (aal->force_rb_mode == -1) {
		return aal->data->rb_mode;
	} else {
		return aal->force_rb_mode;
	}
}

static void clarity_hist_ctrl(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg, bool readback)
{
	struct mml_comp_aal *aal = comp_to_aal(comp);
	struct mml_frame_config *cfg = task->config;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];

	if (!dest->pq_config.en_dre || !dest->pq_config.en_sharp)
		return;

	if (!aal->clts[ccfg->pipe])
		aal->clts[ccfg->pipe] = mml_get_cmdq_clt(cfg->mml,
			ccfg->pipe + GCE_THREAD_START);

	if ((!aal->hist_pkts[ccfg->pipe] && aal->clts[ccfg->pipe]))
		aal->hist_pkts[ccfg->pipe] =
			cmdq_pkt_create(aal->clts[ccfg->pipe]);

	if (!aal->clarity_hist[ccfg->pipe])
		aal->clarity_hist[ccfg->pipe] =
			kzalloc(sizeof(struct mml_pq_readback_buffer),
			GFP_KERNEL);

	if (aal->clarity_hist[ccfg->pipe] &&
		!aal->clarity_hist[ccfg->pipe]->va &&
		aal->clts[ccfg->pipe]) {

		aal->clarity_hist[ccfg->pipe]->va =
			(u32 *)cmdq_mbox_buf_alloc(aal->clts[ccfg->pipe],
			&aal->clarity_hist[ccfg->pipe]->pa);
	}

	if (aal->clarity_hist[ccfg->pipe] && aal->clarity_hist[ccfg->pipe]->va && readback) {
		mml_pq_get_pq_task(aal->pq_task);
		aal->hist_pkts[ccfg->pipe]->no_irq = task->config->dpc;

		if (aal_get_rb_mode(aal) == RB_EOF_MODE) {
			mml_clock_lock(task->config->mml);
			/* ccf power on */
			call_hw_op(task->config->path[0]->mmlsys, mminfra_pw_enable);
			call_hw_op(task->config->path[0]->mmlsys, pw_enable);
			/* dpc exception flow on */
			mml_msg_dpc("%s dpc exception flow on", __func__);
			mml_dpc_exc_keep(task->config->mml);
			call_hw_op(comp, clk_enable);
			mml_clock_unlock(task->config->mml);
			mml_lock_wake_lock(aal->mml, true);
		}

		queue_work(aal->clarity_hist_wq,
			&aal->clarity_hist_task);
	}
}

static s32 aal_hist_ctrl(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg, bool is_config)
{
	struct mml_comp_aal *aal = comp_to_aal(comp);
	struct mml_frame_config *cfg = NULL;
	struct cmdq_pkt *pkt = NULL;
	const phys_addr_t base_pa = comp->base_pa;
	s8 mode = 0;
	struct mml_pq_task *pq_task = NULL;
	struct mml_task_reuse *reuse = NULL;
	struct mml_pipe_cache *cache = NULL;
	struct aal_frame_data *aal_frm = NULL;
	struct mml_frame_dest *dest = NULL;

	if (IS_ERR_OR_NULL(task) || IS_ERR_OR_NULL(task->pq_task)) {
		mml_err("%s task or pq_task is NULL", __func__);
		return 0;
	}

	cfg = task->config;
	pq_task = task->pq_task;
	mode = cfg->info.mode;
	pkt = task->pkts[ccfg->pipe];
	reuse = &task->reuse[ccfg->pipe];
	cache = &cfg->cache[ccfg->pipe];
	aal_frm = aal_frm_data(ccfg);
	dest = &cfg->info.dest[ccfg->node->out_idx];

	if (mode != MML_MODE_DDP_ADDON && mode != MML_MODE_DIRECT_LINK)
		return 0;

	mutex_lock(&aal->irq_wq_lock);

	mml_pq_set_aal_status(pq_task, ccfg->node->out_idx);


	/* MT6989 platform, RB_SOF_MODE will use DPC to power on.
	 * MT6985 platform, RB_SOF_MODE use CCF to power on.
	 */
	if (aal_get_rb_mode(aal) == RB_SOF_MODE) {
		mml_clock_lock(task->config->mml);
		/* ccf power on */
		call_hw_op(task->config->path[0]->mmlsys, mminfra_pw_enable);
		call_hw_op(task->config->path[0]->mmlsys, pw_enable);
		/* dpc exception flow on */
		mml_msg_dpc("%s dpc exception flow on", __func__);
		mml_dpc_exc_keep(task->config->mml);
		call_hw_op(comp, clk_enable);
		mml_clock_unlock(task->config->mml);
	}
	mml_pq_ir_log("%s: engine_id[%d] comp_jobid[%d] jobid[%d] pipe[%d] ",
		__func__, aal->comp.id, aal->jobid,
		task->job.jobid, ccfg->pipe);

	mml_pq_msg("%s: hist_sram_idx[%d] hist_read_idx[%d] aal_read_status[%d]",
		__func__, aal->hist_sram_idx, aal->hist_read_idx,
		task->pq_task->read_status.aal_comp);

	if (is_config)
		mml_write(pkt, base_pa + aal->data->reg_table[AAL_INTSTA],
			0x0, U32_MAX, reuse, cache,
			&aal_frm->labels[0]);
	else
		mml_update(reuse, aal_frm->labels[0], 0x0);

	if (task->pq_task->read_status.aal_comp == MML_PQ_HIST_IDLE) {

		aal->hist_sram_idx = (aal->hist_sram_idx) ? 0 : 1;
		aal->hist_read_idx = aal->hist_sram_idx;

		aal->dual = task->config->dual;
		aal->pipe = ccfg->pipe;
		aal->pq_task = task->pq_task;
		mml_pq_get_pq_task(aal->pq_task);
		aal->out_idx = ccfg->node->out_idx;

		aal->frame_data.size_info.out_rotate[ccfg->node->out_idx] =
			cfg->out_rotate[ccfg->node->out_idx];
		memcpy(&aal->frame_data.pq_param, task->pq_param,
			MML_MAX_OUTPUTS * sizeof(struct mml_pq_param));
		memcpy(&aal->frame_data.info, &task->config->info,
			sizeof(struct mml_frame_info));
		memcpy(&aal->frame_data.frame_out, &task->config->frame_out,
			MML_MAX_OUTPUTS * sizeof(struct mml_frame_size));
		memcpy(&aal->frame_data.size_info.frame_in_crop_s[0],
			&cfg->frame_in_crop[0],	MML_MAX_OUTPUTS * sizeof(struct mml_crop));

		aal->frame_data.size_info.crop_size_s.width =
			cfg->frame_tile_sz.width;
		aal->frame_data.size_info.crop_size_s.height=
			cfg->frame_tile_sz.height;
		aal->frame_data.size_info.frame_size_s.width = cfg->frame_in.width;
		aal->frame_data.size_info.frame_size_s.height = cfg->frame_in.height;
		aal->jobid = task->job.jobid;

		if (task->config->dual)
			aal->cut_pos_x = cfg->hist_div[ccfg->tile_eng_idx];
		else
			aal->cut_pos_x = task->config->frame_tile_sz.width;

		aal->dre_blk_width = aal_frm->dre_blk_width;
		aal->dre_blk_height = aal_frm->dre_blk_height;

		mutex_unlock(&aal->irq_wq_lock);

		if (aal_get_rb_mode(aal) == RB_EOF_MODE) {
			mml_clock_lock(task->config->mml);
			/* ccf power on */
			call_hw_op(task->config->path[0]->mmlsys, mminfra_pw_enable);
			call_hw_op(task->config->path[0]->mmlsys, pw_enable);
			/* dpc exception flow on */
			mml_msg_dpc("%s dpc exception flow on", __func__);
			mml_dpc_exc_keep(task->config->mml);
			call_hw_op(comp, clk_enable);
			mml_clock_unlock(task->config->mml);
			mml_lock_wake_lock(aal->mml, true);
			aal->dpc = task->config->dpc;
			aal->mmlsys_comp = task->config->path[0]->mmlsys;

			if (is_config)
				mml_write(pkt, base_pa + aal->data->reg_table[AAL_INTEN],
					0x2, U32_MAX, reuse, cache,
					&aal_frm->labels[1]);
			else
				mml_update(reuse, aal_frm->labels[1], 0x2);
		}
		if (is_config)
			mml_write(pkt, base_pa + aal->data->reg_table[AAL_SRAM_CFG],
				aal->hist_sram_idx << 6 | aal->hist_read_idx << 5 |
				1 << 4,	0x7 << 4, reuse, cache,
				&aal_frm->labels[2]);
		else
			mml_update(reuse, aal_frm->labels[2], aal->hist_sram_idx << 6 |
				aal->hist_read_idx << 5 | 1 << 4);

		mml_pq_msg("%s: hist_sram_idx write to [%d], hist_read_idx change to[%d]",
			__func__, aal->hist_sram_idx, aal->hist_read_idx);

		clarity_hist_ctrl(comp, task, ccfg, aal_frm->is_clarity_need_readback);
	} else {
		mml_pq_msg("%s: hist_sram_idx [%d] is reading, hist_read_idx [%d]",
			__func__, aal->hist_sram_idx, aal->hist_read_idx);

		if (aal->hist_sram_idx == aal->hist_read_idx)
			aal->hist_sram_idx = (!aal->hist_read_idx) ? 1 : 0;

		mutex_unlock(&aal->irq_wq_lock);

		if (aal_get_rb_mode(aal) == RB_EOF_MODE) {
			if (is_config)
				mml_write(pkt, base_pa + aal->data->reg_table[AAL_INTEN],
					0x0, U32_MAX, reuse, cache,
					&aal_frm->labels[1]);
			else
				mml_update(reuse, aal_frm->labels[1], 0x0);
		}
		if (is_config)
			mml_write(pkt, base_pa + aal->data->reg_table[AAL_SRAM_CFG],
				aal->hist_sram_idx << 6 | 1 << 4,
				0x5 << 4, reuse, cache, &aal_frm->labels[2]);
		else
			mml_update(reuse, aal_frm->labels[2], aal->hist_sram_idx << 6 | 1 << 4);
	}

	mml_pq_msg("%s: hist_sram_idx write to [%d], hist_read_idx[%d]",
		__func__, aal->hist_sram_idx, aal->hist_read_idx);

	return 0;
}

static void aal_write_curve(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg, u32 *curve, bool is_config)
{
	struct mml_comp_aal *aal = comp_to_aal(comp);
	void __iomem *base = comp->base;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];
	struct aal_frame_data *aal_frm = aal_frm_data(ccfg);
	u32 sram_status_reg = aal->data->reg_table[AAL_SRAM_STATUS];
	const phys_addr_t base_pa = comp->base_pa;
	u32 addr = aal->sram_curve_start;
	bool sram_staus = false;
	u32 i = 0;
	u32 sram_cfg_val = 0, temp_val = 0;

	mml_clock_lock(task->config->mml);
	call_hw_op(task->config->path[0]->mmlsys, mminfra_pw_enable);
	call_hw_op(task->config->path[0]->mmlsys, pw_enable);
	if (task->config->dpc) {
		/* dpc exception flow on */
		mml_msg("%s dpc exception flow on", __func__);
		mml_dpc_exc_keep(task->config->mml);
	}
	call_hw_op(comp, clk_enable);
	mml_clock_unlock(task->config->mml);
	mml_lock_wake_lock(aal->mml, true);

	do {
		temp_val = readl(base + aal->data->reg_table[AAL_SRAM_CFG]);
		sram_cfg_val = temp_val & 0xFFFFF8FF;

		if (aal->clk_on) {
			aal->curve_sram_idx = 1;
			sram_cfg_val = sram_cfg_val | aal->curve_sram_idx << 10 |
				aal->curve_sram_idx << 9 | 1 << 8;
			writel(sram_cfg_val, base + aal->data->reg_table[AAL_SRAM_CFG]);
			aal->curve_sram_idx = (aal->curve_sram_idx == 1) ? 0 : 1;
			aal->clk_on = false;
		} else
			aal->curve_sram_idx = (aal->curve_sram_idx == 1) ? 0 : 1;

		mml_pq_ir_log("%s: jobid[%d] sram_cfg[%08x %08x] curve_idx[%d] clk_on[%d]",
			__func__, task->job.jobid, temp_val, sram_cfg_val,
			aal->curve_sram_idx, aal->clk_on);

		writel(addr, base + aal->data->reg_table[AAL_SRAM_RW_IF_0]);
		if (!sram_staus && aal_reg_poll(comp, sram_status_reg,
			(0x1 << aal->data->curve_ready_bit),
			(0x1 << aal->data->curve_ready_bit)) != true) {
			mml_pq_log("%s idx[%d]", __func__, i);
			break;
		}
		sram_staus = true;
	} while (0);

	for (i = 0; i < AAL_CURVE_NUM; i++, addr += 4)
		if (sram_staus)
			writel(curve[i], base + aal->data->reg_table[AAL_SRAM_RW_IF_1]);

	mml_clock_lock(task->config->mml);
	call_hw_op(comp, clk_disable, task->config->dpc);
	if (task->config->dpc) {
		/* dpc exception flow off */
		mml_msg("%s dpc exception flow off", __func__);
		mml_dpc_exc_release(task->config->mml);
	}
	call_hw_op(task->config->path[0]->mmlsys, pw_disable);
	call_hw_op(task->config->path[0]->mmlsys, mminfra_pw_disable);

	mml_clock_unlock(task->config->mml);
	mml_lock_wake_lock(aal->mml, false);

	if (is_config)
		mml_write(pkt, base_pa + aal->data->reg_table[AAL_SRAM_CFG],
			!aal->curve_sram_idx << 10 | aal->curve_sram_idx << 9 | 1 << 8,
			0x7 << 8, reuse, cache, &aal_frm->labels[3]);
	else
		mml_update(reuse, aal_frm->labels[3], !aal->curve_sram_idx << 10 |
			aal->curve_sram_idx << 9 | 1 << 8);
}

static s32 aal_config_frame(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	struct mml_comp_aal *aal = comp_to_aal(comp);
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	struct aal_frame_data *aal_frm = aal_frm_data(ccfg);
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	const phys_addr_t base_pa = comp->base_pa;

	struct mml_pq_comp_config_result *result;
	struct mml_pq_reg *regs;
	u32 *curve;
	struct mml_pq_aal_config_param *tile_config_param;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];
	u32 addr = aal->sram_curve_start;
	u32 gpr = aal->data->gpr[ccfg->pipe];
	s8 mode = cfg->info.mode;
	u32 alpha = cfg->info.alpha ? 1 << 8 : 0;
	s32 ret = 0;
	u32 i;

	mml_pq_trace_ex_begin("%s %d", __func__, mode);
	mml_pq_msg("%s engine_id[%d] en_dre[%d]", __func__, comp->id, dest->pq_config.en_dre);

	if (aal_frm->relay_mode) {
		/* relay mode */
		aal_relay(comp, pkt, base_pa, alpha | 0x1);
		goto exit;
	}
	aal_relay(comp, pkt, base_pa, alpha);

	/* Enable 10-bit output by default
	 * cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_CFG_MAIN], 0, 0x00000080);
	 */

	do {
		ret = mml_pq_get_comp_config_result(task, AAL_WAIT_TIMEOUT_MS);
		if (ret) {
			mml_pq_comp_config_clear(task);
			aal_frm->config_success = false;
			aal_relay(comp, pkt, base_pa, alpha | 0x1);
			mml_pq_err("%s: get aal param timeout: %d in %dms",
				__func__, ret, AAL_WAIT_TIMEOUT_MS);
			ret = -ETIMEDOUT;
			goto exit;
		}

		result = get_aal_comp_config_result(task);
		if (!result) {
			mml_pq_err("%s: not get result from user lib", __func__);
			ret = -EBUSY;
			aal_frm->config_success = false;
			aal_relay(comp, pkt, base_pa, alpha | 0x1);
			goto exit;
		}
	} while ((mml_pq_debug_mode & MML_PQ_SET_TEST) && result->is_set_test);

	regs = result->aal_regs;
	curve = result->aal_curve;

	/* TODO: use different regs */
	mml_pq_msg("%s:config aal regs, count: %d", __func__, result->aal_reg_cnt);
	aal_frm->config_success = true;
	for (i = 0; i < result->aal_reg_cnt; i++) {
		cmdq_pkt_write(pkt, NULL, base_pa + regs[i].offset,
			regs[i].value, regs[i].mask);
		mml_pq_msg("[aal][config][%x] = %#x mask(%#x)",
			regs[i].offset, regs[i].value, regs[i].mask);
	}

	if (aal->data->is_linear)
		cmdq_pkt_write(pkt, NULL, base_pa + 0x3b4, 0x18, U32_MAX);

	aal_frm->is_aal_need_readback = result->is_aal_need_readback;
	aal_frm->is_clarity_need_readback = result->is_clarity_need_readback;

	tile_config_param = &(result->aal_param[ccfg->node->out_idx]);
	aal_frm->dre_blk_width = tile_config_param->dre_blk_width;
	aal_frm->dre_blk_height = tile_config_param->dre_blk_height;
	aal_hist_ctrl(comp, task, ccfg, true);

	if (mode == MML_MODE_MML_DECOUPLE) {
		cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_INTSTA],
			0x0, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_INTEN],
			0x0, U32_MAX);
		cmdq_pkt_write(pkt, NULL,
			base_pa + aal->data->reg_table[AAL_INTSTA], 0x0, U32_MAX);
		cmdq_pkt_write(pkt, NULL,
			base_pa + aal->data->reg_table[AAL_INTEN], 0x0, U32_MAX);
		cmdq_pkt_write(pkt, NULL,
			base_pa + aal->data->reg_table[AAL_SRAM_RW_IF_0], addr, U32_MAX);
		cmdq_pkt_poll(pkt, NULL, (0x1 << aal->data->curve_ready_bit),
			base_pa + aal->data->reg_table[AAL_SRAM_STATUS],
			(0x1 << aal->data->curve_ready_bit), gpr);
		for (i = 0; i < AAL_CURVE_NUM; i++, addr += 4)
			mml_write_array(pkt, base_pa + aal->data->reg_table[AAL_SRAM_RW_IF_1],
				curve[i], U32_MAX, reuse, cache, &aal_frm->reuse_curve);
	} else if ((mode == MML_MODE_DDP_ADDON || mode == MML_MODE_DIRECT_LINK) &&
		!aal->data->is_linear)
		aal_write_curve(comp, task, ccfg, curve, true);

	mml_pq_msg("%s is_aal_need_readback[%d] base_pa[%llx] reuses[%u]",
		__func__, result->is_aal_need_readback, base_pa,
		aal_frm->reuse_curve.idx);

	mml_pq_msg("%s: success dre_blk_width[%d], dre_blk_height[%d]",
		__func__, tile_config_param->dre_blk_width,
		tile_config_param->dre_blk_height);
exit:
	mml_msg("%s alpha_pq_r2y:%d alpha:%d format:0x%08x mode:%d pq:%d dre:%d",
		__func__, aal->data->alpha_pq_r2y, cfg->info.alpha,
		dest->data.format, mode, dest->pq_config.en, dest->pq_config.en_dre);
	/* Enable alpha r2y when resize but not RROT */
	if (aal->data->alpha_pq_r2y && cfg->alpharsz && dest->data.format == MML_FMT_YUVA8888 &&
	    mode != MML_MODE_DIRECT_LINK) {
		u32 r2y_00, r2y_01, r2y_02, r2y_03, r2y_04, r2y_05;

		/* 31-31 r2y_en,   24-16 r2y_post_add_1_s, 8-0 r2y_post_add_0_s */
		r2y_00 = (1 << 31) | (128 << 16) | (0 << 0);
		/* 26-16 r2y_c00_s,  8-0 r2y_post_add_2_s */
		r2y_01 = (306 << 16) | (128 << 0);
		/* 26-16 r2y_c02_s, 10-0 r2y_c01_s */
		r2y_02 = (117 << 16) | (601 << 0);
		/* 26-16 r2y_c11_s, 10-0 r2y_c10_s */
		r2y_03 = (1701 << 16) | (1872 << 0);
		/* 26-16 r2y_c20_s, 10-0 r2y_c12_s */
		r2y_04 = (523 << 16) | (523 << 0);
		/* 26-16 r2y_c22_s, 10-0 r2y_c21_s */
		r2y_05 = (1963 << 16) | (1610 << 0);

		/* relay_mode(bit 0) = 0, AAL_HIST_EN(bit 2) = 0 */
		/* BLK_HIST_EN(bit 5) = 0, alpha_en(bit 8) = 1 */
		cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_CFG], 0x102, 0x127);
		/* dre_map_bypass(bit 4) = 1 */
		cmdq_pkt_write(pkt, NULL, base_pa + 0x3b4, 0x18, U32_MAX);
		/* bilateral_flt_en(bit 1) = 0 */
		cmdq_pkt_write(pkt, NULL, base_pa + 0x53c, 0x0, 0x2);
		/* y2r_en(bit 31) = 0 */
		cmdq_pkt_write(pkt, NULL, base_pa + 0x4bc, 0x1800000, U32_MAX);

		cmdq_pkt_write(pkt, NULL, base_pa + 0x4d4, r2y_00, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + 0x4d8, r2y_01, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + 0x4dc, r2y_02, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + 0x4e0, r2y_03, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + 0x4e4, r2y_04, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + 0x4e8, r2y_05, U32_MAX);
	}

	mml_pq_trace_ex_end();
	return ret;
}

static s32 aal_config_tile(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;
	struct aal_frame_data *aal_frm = aal_frm_data(ccfg);
	struct mml_comp_aal *aal = comp_to_aal(comp);

	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);
	u16 tile_cnt = cfg->frame_tile[ccfg->pipe]->tile_cnt;

	u32 aal_input_w;
	u32 aal_input_h;
	u32 aal_output_w;
	u32 aal_output_h;
	u32 aal_crop_x_offset;
	u32 aal_crop_y_offset;
	u32 dre_blk_width = 0;
	u32 dre_blk_height = 0;

	u32 act_win_x_start = 0, act_win_x_end = 0;
	u32 act_win_y_start = 0, act_win_y_end = 0;
	u32 win_x_start = 0, win_x_end = 0;
	u32 blk_num_x_start = 0, blk_num_x_end = 0;
	u32 blk_num_y_start = 0, blk_num_y_end = 0;
	u32 blk_cnt_x_start = 0, blk_cnt_x_end = 0;
	u32 blk_cnt_y_start = 0, blk_cnt_y_end = 0;
	u32 roi_x_start = 0, roi_x_end = 0;
	u32 roi_y_start = 0, roi_y_end = 0;
	u32 win_y_start = 0, win_y_end = 0;

	u32 aal_hist_left_start = 0;
	u32 tile_pxl_x_start = 0, tile_pxl_x_end = 0;
	u32 tile_pxl_y_start = 0, tile_pxl_y_end = 0;
	u32 last_tile_x_flag = 0, last_tile_y_flag = 0;
	u32 save_first_blk_col_flag = 0;
	u32 save_last_blk_col_flag = 0;
	u32 hist_first_tile = 0, hist_last_tile = 0;
	s32 ret = 0;

	mml_pq_trace_ex_begin("%s", __func__);
	aal_input_w = tile->in.xe - tile->in.xs + 1;
	aal_input_h = tile->in.ye - tile->in.ys + 1;
	aal_output_w = tile->out.xe - tile->out.xs + 1;
	aal_output_h = tile->out.ye - tile->out.ys + 1;
	aal_crop_x_offset = tile->out.xs - tile->in.xs;
	aal_crop_y_offset = tile->out.ys - tile->in.ys;

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_SIZE],
		(aal_input_w << 16) + aal_input_h, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_OUTPUT_OFFSET],
		(aal_crop_x_offset << 16) + aal_crop_y_offset, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_OUTPUT_SIZE],
		(aal_output_w << 16) + aal_output_h, U32_MAX);

	if (aal_frm->relay_mode)
		goto exit;

	if (!idx) {
		if (task->config->dual)
			aal_frm->cut_pos_x = cfg->hist_div[ccfg->tile_eng_idx];
		else
			aal_frm->cut_pos_x = task->config->frame_tile_sz.width;
		if (ccfg->pipe)
			aal_frm->out_hist_xs = aal_frm->cut_pos_x;
	}

	dre_blk_width =
		(aal_frm->dre_blk_width) ? aal_frm->dre_blk_width : BLK_WIDTH_DEFAULT;
	dre_blk_height =
		(aal_frm->dre_blk_height) ? aal_frm->dre_blk_height : BLK_HEIGH_DEFAULT;

	aal_hist_left_start =
		(tile->out.xs > aal_frm->out_hist_xs) ? tile->out.xs : aal_frm->out_hist_xs;

	mml_pq_msg("%s jobid[%d] engine_id[%d] idx[%d] pipe[%d] pkt[%08lx]",
		__func__, task->job.jobid, comp->id, idx, ccfg->pipe,
		(unsigned long)pkt);

	mml_pq_msg("%s %d: %d: %d: [input] [xs, xe] = [%d, %d], [ys, ye] = [%d, %d]",
		__func__, task->job.jobid, comp->id, idx, tile->in.xs,
		tile->in.xe, tile->in.ys,
		tile->in.ye);
	mml_pq_msg("%s %d: %d: %d: [output] [xs, xe] = [%d, %d], [ys, ye] = [%d, %d]",
		__func__, task->job.jobid, comp->id, idx,
		tile->out.xs, tile->out.xe, tile->out.ys,
		tile->out.ye);
	mml_pq_msg("%s %d: %d: %d: [aal_crop_offset] [x, y] = [%d, %d], aal_hist_left_start[%d]",
		__func__, task->job.jobid, comp->id, idx,
		aal_crop_x_offset, aal_crop_y_offset,
		aal_hist_left_start);

	act_win_x_start = aal_hist_left_start - tile->in.xs;
	if (task->config->dual && !ccfg->pipe && (idx + 1 >= tile_cnt))
		act_win_x_end = aal_frm->cut_pos_x - tile->in.xs - 1;
	else
		act_win_x_end = tile->out.xe - tile->in.xs;
	tile_pxl_x_start = tile->in.xs;
	tile_pxl_x_end = tile->in.xe;

	last_tile_x_flag = (tile->in.xe+1 >= task->config->frame_tile_sz.width) ? 1:0;

	mml_pq_msg("%s %d: %d: %d: [tile_pxl] [xs, xe] = [%d, %d]",
		__func__, task->job.jobid, comp->id, idx, tile_pxl_x_start, tile_pxl_x_end);

	act_win_y_start = 0;
	act_win_y_end = tile->in.ye - tile->in.ys;
	tile_pxl_y_start = 0;
	tile_pxl_y_end = tile->in.ye - tile->in.ys;

	last_tile_y_flag = (tile_pxl_y_end+1 >= task->config->frame_tile_sz.height) ? 1:0;
	roi_x_start = 0;
	roi_x_end = tile->in.xe - tile->in.xs;
	roi_y_start = 0;
	roi_y_end = tile->in.ye - tile->in.ys;

	blk_num_x_start = (tile_pxl_x_start / dre_blk_width);
	blk_num_x_end = (tile_pxl_x_end / dre_blk_width);
	blk_cnt_x_start = tile_pxl_x_start - (blk_num_x_start * dre_blk_width);
	blk_cnt_x_end = tile_pxl_x_end - (blk_num_x_end * dre_blk_width);
	blk_num_y_start = (tile_pxl_y_start / dre_blk_height);
	blk_num_y_end = (tile_pxl_y_end / dre_blk_height);
	blk_cnt_y_start = tile_pxl_y_start - (blk_num_y_start * dre_blk_height);
	blk_cnt_y_end = tile_pxl_y_end - (blk_num_y_end * dre_blk_height);

	/* for n+1 tile*/
	if (!idx) {
		if (task->config->dual && ccfg->pipe)
			aal_frm->out_hist_xs = tile->in.xs + act_win_x_end + 1;
		else
			aal_frm->out_hist_xs = tile->out.xe + 1;
		hist_first_tile = 1;
		hist_last_tile = (tile_cnt == 1) ? 1 : 0;
		save_first_blk_col_flag = (ccfg->pipe) ? 1 : 0;
		if (idx + 1 >= tile_cnt) {
			save_last_blk_col_flag = (ccfg->pipe) ? 0 : 1;
			aal_frm->out_hist_xs = 0;
		}
		else
			save_last_blk_col_flag = 0;
	} else if (idx + 1 >= tile_cnt) {
		aal_frm->out_hist_xs = 0;
		save_first_blk_col_flag = 0;
		save_last_blk_col_flag = (ccfg->pipe) ? 0 : 1;
		hist_first_tile = 0;
		hist_last_tile = 1;
	} else {
		if (task->config->dual && ccfg->pipe)
			aal_frm->out_hist_xs = tile->in.xs + act_win_x_end + 1;
		else
			aal_frm->out_hist_xs = tile->out.xe + 1;
		save_first_blk_col_flag = 0;
		save_last_blk_col_flag = 0;
		hist_first_tile = 0;
		hist_last_tile = 0;
	}

	win_x_start = 0;
	win_x_end = tile_pxl_x_end - tile_pxl_x_start + 1;
	win_y_start = 0;
	win_y_end = tile_pxl_y_end - tile_pxl_y_start + 1;
	task->pq_task->aal_readback.readback_data.cut_pos_x =
		aal_frm->cut_pos_x;

	mml_pq_msg("%s %d: %d: %d:[act_win][xs, xe] = [%d, %d], [ys, ye] = [%d, %d]",
			__func__, task->job.jobid, comp->id, idx, act_win_x_start, act_win_x_end,
			act_win_y_start, act_win_y_end);
	mml_pq_msg("%s %d: %d: %d:[blk_num][xs, xe] = [%d, %d], [ys, ye] = [%d, %d]",
			__func__, task->job.jobid, comp->id, idx, blk_num_x_start, blk_num_x_end,
			blk_num_y_start, blk_num_y_end);
	mml_pq_msg("%s %d: %d: %d:[blk_cnt][xs, xe] = [%d, %d], [ys, ye] = [%d, %d]",
			__func__, task->job.jobid, comp->id, idx, blk_cnt_x_start, blk_cnt_x_end,
			blk_cnt_y_start, blk_cnt_y_end);
	mml_pq_msg("%s %d: %d: %d:[roi][xs, xe] = [%d, %d], [ys, ye] = [%d, %d]",
			__func__,  task->job.jobid, comp->id, idx, roi_x_start,
			roi_x_end, roi_y_start, roi_y_end);
	mml_pq_msg("%s %d: %d: %d:[win][xs, xe] = [%d, %d], [ys, ye] = [%d, %d]",
			__func__, task->job.jobid, comp->id, idx, win_x_start,
			win_x_end, win_y_start, win_y_end);
	mml_pq_msg("%s %d: %d: %d:save_first_blk_col_flag[%d], save_last_blk_col_flag[%d]",
			__func__, task->job.jobid, comp->id, idx,
			save_first_blk_col_flag,
			save_last_blk_col_flag);
	mml_pq_msg("%s %d: %d: %d:last_tile_x_flag[%d], last_tile_y_flag[%d]",
			__func__, task->job.jobid, comp->id, idx,
			last_tile_x_flag, last_tile_y_flag);

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_DRE_BLOCK_INFO_00],
		(act_win_x_end << 16) | act_win_x_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_DRE_BLOCK_INFO_07],
		(act_win_y_end << 16) | act_win_y_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_TILE_00],
		(save_first_blk_col_flag << 23) | (save_last_blk_col_flag << 22) |
		(last_tile_x_flag << 21) | (last_tile_y_flag << 20) |
		(blk_num_x_end << 15) | (blk_num_x_start << 10) |
		(blk_num_y_end << 5) | blk_num_y_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_TILE_01],
		(blk_cnt_x_end << 16) | blk_cnt_x_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_TILE_02],
		(blk_cnt_y_end << 16) | blk_cnt_y_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_DRE_ROI_00],
		(roi_x_end << 16) | roi_x_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_DRE_ROI_01],
		(roi_y_end << 16) | roi_y_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_WIN_X_MAIN],
		(win_x_end << 16) | win_x_start, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_WIN_Y_MAIN],
		(win_y_end << 16) | win_y_start, U32_MAX);

	if (aal->data->reg_table[AAL_BILATERAL_STATUS_CTRL] != REG_NOT_SUPPORT)
		cmdq_pkt_write(pkt, NULL, base_pa + aal->data->reg_table[AAL_BILATERAL_STATUS_CTRL],
			(hist_last_tile << 2) | (hist_first_tile << 1) | 1, U32_MAX);

exit:
	mml_pq_trace_ex_end();
	return ret;
}

static void aal_readback_cmdq(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct mml_comp_aal *aal = comp_to_aal(comp);
	struct aal_frame_data *aal_frm = aal_frm_data(ccfg);

	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_frame_config *cfg = task->config;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];

	const phys_addr_t base_pa = comp->base_pa;
	u32 addr = aal->sram_hist_start;
	u32 dre30_hist_sram_start = aal->sram_hist_start;
	s32 i = 0;
	u8 pipe = ccfg->pipe;
	dma_addr_t begin_pa = 0;
	u32 *condi_inst = NULL;
	dma_addr_t pa = 0;
	struct cmdq_operand lop, rop;

	const u16 idx_addr = CMDQ_THR_SPR_IDX1;
	const u16 idx_val = CMDQ_THR_SPR_IDX2;
	const u16 poll_gpr = aal->data->gpr[ccfg->pipe];
	const u16 idx_out = aal->data->cpr[ccfg->pipe];
	const u16 idx_out64 = CMDQ_CPR_TO_CPR64(idx_out);

	mml_pq_msg("%s start engine_id[%d] addr[%d]", __func__, comp->id,  addr);

	mml_pq_get_readback_buffer(task, pipe, &(task->pq_task->aal_hist[pipe]));

	if (unlikely(!task->pq_task->aal_hist[pipe])) {
		mml_pq_err("%s job_id[%d] aal_hist is null", __func__,
			task->job.jobid);
		return;
	}

	pa = task->pq_task->aal_hist[pipe]->pa;

	/* init sprs
	 * spr1 = AAL_SRAM_START
	 * cpr64 = out_pa
	 */
	cmdq_pkt_assign_command(pkt, idx_addr, dre30_hist_sram_start);

	mml_assign(pkt, idx_out, (u32)pa,
		reuse, cache, &aal_frm->labels[AAL_POLLGPR_0]);
	mml_assign(pkt, idx_out + 1, (u32)(pa >> 32),
		reuse, cache, &aal_frm->labels[AAL_POLLGPR_1]);


	/* config aal sram addr and poll */
	cmdq_pkt_write_reg_addr(pkt, base_pa + aal->data->reg_table[AAL_SRAM_RW_IF_2],
		idx_addr, U32_MAX);
	/* use gpr low as poll gpr */
	cmdq_pkt_poll_addr(pkt, AAL_SRAM_STATUS_BIT,
		base_pa + aal->data->reg_table[AAL_SRAM_STATUS],
		AAL_SRAM_STATUS_BIT, poll_gpr);

	/* loop again here */
	aal_frm->begin_offset = pkt->cmd_buf_size;
	begin_pa = cmdq_pkt_get_pa_by_offset(pkt, aal_frm->begin_offset);

	/* read to value spr */
	cmdq_pkt_read_addr(pkt, base_pa + aal->data->reg_table[AAL_SRAM_RW_IF_3], idx_val);
	/* write value spr to dst cpr64 */
	cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

	/* jump forward end if sram is last one, if spr1 >= 4096 + 4 * 767 */

	cmdq_pkt_assign_command(pkt, CMDQ_THR_SPR_IDX0, begin_pa);
	aal_frm->condi_offset = pkt->cmd_buf_size - CMDQ_INST_SIZE;

	/* inc src addr */
	lop.reg = true;
	lop.idx = idx_addr;
	rop.reg = false;
	rop.value = 4;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_addr, &lop, &rop);
	/* inc outut pa */
	lop.reg = true;
	lop.idx = idx_out;
	rop.reg = false;
	rop.value = 4;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);

	lop.reg = true;
	lop.idx = idx_addr;
	rop.reg = false;
	rop.value = dre30_hist_sram_start + 4 * (AAL_HIST_NUM - 1);
	cmdq_pkt_cond_jump_abs(pkt, CMDQ_THR_SPR_IDX0, &lop, &rop,
		CMDQ_LESS_THAN_AND_EQUAL);

	condi_inst = (u32 *)cmdq_pkt_get_va_by_offset(pkt, aal_frm->condi_offset);
	if (unlikely(!condi_inst))
		mml_pq_err("%s wrong offset %u\n", __func__, aal_frm->condi_offset);

	*condi_inst = (u32)CMDQ_REG_SHIFT_ADDR(begin_pa);

	for (i = 0; i < 8; i++) {
		cmdq_pkt_read_addr(pkt, base_pa + aal->data->reg_table[AAL_DUAL_PIPE_00] + i * 4,
			idx_val);
		cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

		lop.reg = true;
		lop.idx = idx_out;
		rop.reg = false;
		rop.value = 4;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);
	}

	for (i = 0; i < 8; i++) {
		cmdq_pkt_read_addr(pkt, base_pa + aal->data->reg_table[AAL_DUAL_PIPE_08] + i * 4,
			idx_val);
		cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

		lop.reg = true;
		lop.idx = idx_out;
		rop.reg = false;
		rop.value = 4;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);
	}

	if (aal->data->reg_table[AAL_BILATERAL_STATUS_00] != REG_NOT_SUPPORT) {
		for (i = 0; i < AAL_CLARITY_STATUS_NUM; i++) {
			cmdq_pkt_read_addr(pkt,
				base_pa + aal->data->reg_table[AAL_BILATERAL_STATUS_00] + i * 4,
				idx_val);
			cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

			lop.reg = true;
			lop.idx = idx_out;
			rop.reg = false;
			rop.value = 4;
			cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);
		}
	}

	mml_pq_rb_msg("%s end job_id[%d] engine_id[%d] va[%p] pa[%llx] pkt[%p]",
		__func__, task->job.jobid, comp->id, task->pq_task->aal_hist[pipe]->va,
		task->pq_task->aal_hist[pipe]->pa, pkt);

	mml_pq_rb_msg("%s end job_id[%d] condi:offset[%u] inst[%p], begin:offset[%u] pa[%llx]",
		__func__, task->job.jobid, aal_frm->condi_offset, condi_inst,
		aal_frm->begin_offset, begin_pa);
}

static void aal_readback_vcp(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_comp_aal *aal = comp_to_aal(comp);
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	u8 pipe = ccfg->pipe;
	struct aal_frame_data *aal_frm = aal_frm_data(ccfg);


	u32 gpr = aal->data->gpr[ccfg->pipe];
	u32 engine = CMDQ_VCP_ENG_MML_AAL0 + pipe;

	mml_pq_rb_msg("%s aal_hist[%p] pipe[%d]", __func__,
		task->pq_task->aal_hist[pipe], pipe);

	if (!(task->pq_task->aal_hist[pipe])) {
		task->pq_task->aal_hist[pipe] =
			kzalloc(sizeof(struct mml_pq_readback_buffer), GFP_KERNEL);

		if (unlikely(!task->pq_task->aal_hist[pipe])) {
			mml_pq_err("%s not enough mem for aal_hist", __func__);
			return;
		}

		task->pq_task->aal_hist[pipe]->va =
			cmdq_get_vcp_buf(engine, &(task->pq_task->aal_hist[pipe]->pa));
	}

	mml_pq_get_vcp_buf_offset(task, MML_PQ_AAL0+pipe, task->pq_task->aal_hist[pipe]);

	cmdq_vcp_enable(true);

	cmdq_pkt_readback(pkt, engine, task->pq_task->aal_hist[pipe]->va_offset,
		 AAL_HIST_NUM+AAL_DUAL_INFO_NUM, gpr,
		&reuse->labels[reuse->label_idx],
		&aal_frm->polling_reuse);

	add_reuse_label(reuse, &aal_frm->labels[AAL_POLLGPR_0],
		task->pq_task->aal_hist[pipe]->va_offset);

	mml_pq_rb_msg("%s end job_id[%d] engine_id[%d] va[%p] pa[%llx] pkt[%p] offset[%d]",
			__func__, task->job.jobid, comp->id, task->pq_task->aal_hist[pipe]->va,
			task->pq_task->aal_hist[pipe]->pa, pkt,
			task->pq_task->aal_hist[pipe]->va_offset);
}

static s32 aal_config_post(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	struct mml_frame_dest *dest = &task->config->info.dest[ccfg->node->out_idx];
	struct mml_comp_aal *aal = comp_to_aal(comp);
	struct aal_frame_data *aal_frm = aal_frm_data(ccfg);
	bool vcp = aal->data->vcp_readback;
	u32 mode = task->config->info.mode;


	mml_pq_msg("%s start engine_id[%d] en_dre[%d] mode[%d]", __func__, comp->id,
			dest->pq_config.en_dre, mode);

	if (aal_frm->relay_mode)
		goto exit;

	if (mode == MML_MODE_DDP_ADDON || mode == MML_MODE_DIRECT_LINK)
		goto put_comp_config;

	if (vcp)
		aal_readback_vcp(comp, task, ccfg);
	else
		aal_readback_cmdq(comp, task, ccfg);

put_comp_config:
	mml_pq_put_comp_config_result(task);
exit:
	return 0;
}

static s32 aal_reconfig_frame(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct mml_comp_aal *aal = comp_to_aal(comp);
	struct aal_frame_data *aal_frm = aal_frm_data(ccfg);
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pq_comp_config_result *result;
	u32 *curve, i = 0, j, idx;
	s32 ret = 0;
	s8 mode = cfg->info.mode;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s engine_id[%d] en_dre[%d] config_success[%d]", __func__, comp->id,
			dest->pq_config.en_dre, aal_frm->config_success);

	if (aal_frm->relay_mode)
		goto exit;

	do {
		ret = mml_pq_get_comp_config_result(task, AAL_WAIT_TIMEOUT_MS);
		if (ret) {
			mml_pq_comp_config_clear(task);
			mml_pq_err("get aal param timeout: %d in %dms",
				ret, AAL_WAIT_TIMEOUT_MS);
			ret = -ETIMEDOUT;
			goto exit;
		}

		result = get_aal_comp_config_result(task);
		if (!result || !aal_frm->config_success || !result->aal_reg_cnt) {
			mml_pq_err("%s: not get result from user lib", __func__);
			ret = -EBUSY;
			goto exit;
		}
	} while ((mml_pq_debug_mode & MML_PQ_SET_TEST) && result->is_set_test);

	curve = result->aal_curve;
	aal_frm->is_aal_need_readback = result->is_aal_need_readback;
	aal_frm->is_clarity_need_readback = result->is_clarity_need_readback;
	task->pq_task->aal_readback.readback_data.cut_pos_x =
		aal_frm->cut_pos_x;
	aal_hist_ctrl(comp, task, ccfg, false);

	idx = 0;
	if (mode == MML_MODE_MML_DECOUPLE) {
		for (i = 0; i < aal_frm->reuse_curve.idx; i++)
			for (j = 0; j < aal_frm->reuse_curve.offs[i].cnt; j++, idx++)
				mml_update_array(reuse, &aal_frm->reuse_curve, i, j, curve[idx]);
	} else if (mode == MML_MODE_DIRECT_LINK && !aal->data->is_linear)
		aal_write_curve(comp, task, ccfg, curve, false);


	mml_pq_msg("%s is_aal_need_readback[%d] is_clarity_need_readback[%d]",
		__func__, result->is_aal_need_readback,
		result->is_clarity_need_readback);
exit:
	mml_pq_trace_ex_end();
	return ret;
}

static s32 aal_config_repost(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	struct aal_frame_data *aal_frm = aal_frm_data(ccfg);
	struct mml_frame_dest *dest = &task->config->info.dest[ccfg->node->out_idx];
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	u8 pipe = ccfg->pipe;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	dma_addr_t begin_pa = 0;
	u32 *condi_inst = NULL;
	struct mml_comp_aal *aal = comp_to_aal(comp);
	bool vcp = aal->data->vcp_readback;
	u32 engine = CMDQ_VCP_ENG_MML_AAL0 + pipe;
	struct mml_frame_config *cfg = task->config;
	s8 mode = cfg->info.mode;

	mml_pq_msg("%s engine_id[%d] en_dre[%d]", __func__, comp->id,
			dest->pq_config.en_dre);

	if (aal_frm->relay_mode)
		goto exit;

	if (mode != MML_MODE_MML_DECOUPLE)
		goto comp_config_put;

	if (vcp) {
		if (!(task->pq_task->aal_hist[pipe])) {
			task->pq_task->aal_hist[pipe] =
				kzalloc(sizeof(struct mml_pq_readback_buffer), GFP_KERNEL);

			if (unlikely(!task->pq_task->aal_hist[pipe])) {
				mml_pq_err("%s not enough mem for aal_hist", __func__);
				goto comp_config_put;
			}
			mml_pq_rb_msg("%s aal_hist[%p] pipe[%d]", __func__,
				task->pq_task->aal_hist[pipe], pipe);

			task->pq_task->aal_hist[pipe]->va =
				cmdq_get_vcp_buf(engine, &(task->pq_task->aal_hist[pipe]->pa));
		}

		cmdq_vcp_enable(true);
		mml_pq_get_vcp_buf_offset(task, MML_PQ_AAL0+pipe,
			task->pq_task->aal_hist[pipe]);

		mml_update(reuse, aal_frm->labels[AAL_POLLGPR_0],
			cmdq_pkt_vcp_reuse_val(engine,
			task->pq_task->aal_hist[pipe]->va_offset,
			AAL_HIST_NUM + AAL_DUAL_INFO_NUM));

		cmdq_pkt_reuse_poll(pkt, &aal_frm->polling_reuse);

		mml_pq_rb_msg("%s end job_id[%d] engine_id[%d] va[%p] pa[%llx] pkt[%p] offset[%d]",
			__func__, task->job.jobid, comp->id, task->pq_task->aal_hist[pipe]->va,
			task->pq_task->aal_hist[pipe]->pa, pkt,
			task->pq_task->aal_hist[pipe]->va_offset);

	} else {
		mml_pq_get_readback_buffer(task, pipe, &(task->pq_task->aal_hist[pipe]));

		if (unlikely(!task->pq_task->aal_hist[pipe])) {
			mml_pq_err("%s job_id[%d] aal_hist is null", __func__,
				task->job.jobid);
			goto comp_config_put;
		}

		mml_update(reuse, aal_frm->labels[AAL_POLLGPR_0],
			(u32)task->pq_task->aal_hist[pipe]->pa);
		mml_update(reuse, aal_frm->labels[AAL_POLLGPR_1],
			(u32)(task->pq_task->aal_hist[pipe]->pa >> 32));

		begin_pa = cmdq_pkt_get_pa_by_offset(pkt, aal_frm->begin_offset);
		condi_inst = (u32 *)cmdq_pkt_get_va_by_offset(pkt, aal_frm->condi_offset);
		if (unlikely(!condi_inst))
			mml_pq_err("%s wrong offset %u\n", __func__, aal_frm->condi_offset);

		*condi_inst = (u32)CMDQ_REG_SHIFT_ADDR(begin_pa);

		mml_pq_rb_msg("%s end job_id[%d] engine_id[%d] va[%p] pa[%llx] pkt[%p]",
			__func__, task->job.jobid, comp->id, task->pq_task->aal_hist[pipe]->va,
			task->pq_task->aal_hist[pipe]->pa, pkt);

		mml_pq_rb_msg("%s end job_id[%d]condi:offset[%u]inst[%p],begin:offset[%u]pa[%llx]",
			__func__, task->job.jobid, aal_frm->condi_offset, condi_inst,
			aal_frm->begin_offset, begin_pa);
	}

comp_config_put:
	mml_pq_put_comp_config_result(task);
exit:

	return 0;

}


static const struct mml_comp_config_ops aal_cfg_ops = {
	.prepare = aal_prepare,
	.buf_prepare = aal_buf_prepare,
	.get_label_count = aal_get_label_count,
	.init = aal_config_init,
	.frame = aal_config_frame,
	.tile = aal_config_tile,
	.post = aal_config_post,
	.reframe = aal_reconfig_frame,
	.repost = aal_config_repost,
};

static bool get_dre_block(u32 *phist, const int block_x, const int block_y,
						  const int dre_blk_x_num)
{
	u32 read_value;
	u32 block_offset = 6 * (block_y * dre_blk_x_num + block_x);
	u32 sum = 0, i = 0;
	u32 aal_hist[AAL_HIST_BIN] = {0};
	u32 error_sum = AAL_HIST_MAX_SUM;


	if (block_x < 0 || block_y < 0) {
		mml_pq_err("Error block num block_y = %d, block_x = %d", block_y, block_x);
		return false;
	}

	do {
		if (block_offset >= AAL_HIST_NUM)
			break;
		read_value = phist[block_offset++];
		aal_hist[0] = read_value & 0xff;
		aal_hist[1] = (read_value>>8) & 0xff;
		aal_hist[2] = (read_value>>16) & 0xff;
		aal_hist[3] = (read_value>>24) & 0xff;

		if (block_offset >= AAL_HIST_NUM)
			break;
		read_value = phist[block_offset++];
		aal_hist[4] = read_value & 0xff;
		aal_hist[5] = (read_value>>8) & 0xff;
		aal_hist[6] = (read_value>>16) & 0xff;
		aal_hist[7] = (read_value>>24) & 0xff;

		if (block_offset >= AAL_HIST_NUM)
			break;
		read_value = phist[block_offset++];
		aal_hist[8] = read_value & 0xff;
		aal_hist[9] = (read_value>>8) & 0xff;
		aal_hist[10] = (read_value>>16) & 0xff;
		aal_hist[11] = (read_value>>24) & 0xff;

		if (block_offset >= AAL_HIST_NUM)
			break;
		read_value = phist[block_offset++];
		aal_hist[12] = read_value & 0xff;
		aal_hist[13] = (read_value>>8) & 0xff;
		aal_hist[14] = (read_value>>16) & 0xff;
		aal_hist[15] = (read_value>>24) & 0xff;

		if (block_offset >= AAL_HIST_NUM)
			break;
		read_value = phist[block_offset++];
		aal_hist[16] = read_value & 0xff;
	} while (0);

	for (i = 0; i < AAL_HIST_BIN; i++)
		sum += aal_hist[i];

	mml_pq_rb_msg("%s block_x[%u] block_y[%u] dre_blk_x_num[%u] sum[%u]",
		__func__, block_x, block_y, dre_blk_x_num, sum);

	if (sum >= error_sum) {
		mml_pq_err("block num block_y = %d, block_x = %d histogram over threshold",
			block_y, block_x);
		mml_pq_err("hist[0-8] = (%d %d %d %d %d %d %d %d %d)",
			aal_hist[0], aal_hist[1], aal_hist[2], aal_hist[3],
			aal_hist[4], aal_hist[5], aal_hist[6], aal_hist[7],
			aal_hist[8]);
		mml_pq_err("hist[9-16] = (%d %d %d %d %d %d %d %d)",
			aal_hist[9], aal_hist[10], aal_hist[11], aal_hist[12],
			aal_hist[13], aal_hist[14], aal_hist[15], aal_hist[16]);
		return false;
	} else
		return true;
}

static void aal_hist_blk_calc(struct mml_comp_aal *aal, u32 *dre_blk_x_num,
			u32 *dre_blk_y_num)
{
	u32 src_width = aal->frame_data.size_info.frame_size_s.width;
	u32 src_height = aal->frame_data.size_info.frame_size_s.height;
	u32 blk_height = 1;
	u32 j = MAX_HIST_CHECK_BLK_Y_NUM;

	mml_pq_rb_msg("%s jobid[%d] src_width[%u] src_height[%u]", __func__,
				aal->jobid, src_width, src_height);

	if (src_width < MIN_HIST_CHECK_SRC_WIDTH) {
		*dre_blk_x_num = MIN_HIST_CHECK_BLK_X_NUM;
	} else {
		*dre_blk_x_num = src_width / BLK_WIDTH_DEFAULT;
		*dre_blk_x_num =
			*dre_blk_x_num > MAX_HIST_CHECK_BLK_X_NUM ?
			MAX_HIST_CHECK_BLK_X_NUM : *dre_blk_x_num;
	}
	if (*dre_blk_x_num == MIN_HIST_CHECK_BLK_X_NUM) {
		*dre_blk_y_num = MIN_HIST_CHECK_BLK_Y_NUM;
	} else {
		*dre_blk_y_num = MAX_HIST_CHECK_BLK_Y_NUM;
		for (j = MAX_HIST_CHECK_BLK_Y_NUM; j > 0; j--) {
			*dre_blk_y_num = j;
			blk_height = src_height / (*dre_blk_y_num);
			if (blk_height < MIN_HIST_CHECK_BLK_HEIGHT)
				continue;
			else
				break;
		}
	}
}
static bool aal_ir_hist_check(struct mml_comp_aal *aal)
{
	u32 blk_x_num = 0, blk_y_num = 0, blk_x_start = 0, blk_x_cut = 0, blk_x_comp = 0,
		blk_x = 0, blk_y = 0;
	u32 blk_width = aal->dre_blk_width;
	u32 blk_cut_pos_x = aal->cut_pos_x;
	bool is_cut_on_line = false;
	bool dual = aal->dual;
	u8 pipe = aal->pipe;
	u32 *phist = aal->phist;

	aal_hist_blk_calc(aal, &blk_x_num, &blk_y_num);
	blk_x_cut = (blk_cut_pos_x + 1) / blk_width;
	blk_x_cut = min(blk_x_cut, (blk_x_num - 1));
	is_cut_on_line = ((blk_cut_pos_x + 1) % blk_width) == 0 ? true : false;
	if (dual) {
		if (pipe == 1) {
			blk_x_start = is_cut_on_line ? blk_x_cut : blk_x_cut + 1;
			blk_x_comp = blk_x_num - 1;
		} else {
			blk_x_start = 0;
			blk_x_comp = blk_x_cut - 1;
		}
	} else {
		blk_x_comp = blk_x_num - 1;
	}

	mml_pq_rb_msg("%s dual[%d] pipe[%d] cut_pos_x[%u] blk_width[%u]",
		__func__, dual, pipe, blk_cut_pos_x, blk_width);

	mml_pq_rb_msg("%s blk_x_num[%u] blk_y_num[%u] blk_x_start[%u] blk_x_comp[%u]",
		__func__, blk_x_num, blk_y_num, blk_x_start, blk_x_comp);

	for (blk_y = 0; blk_y < blk_y_num; blk_y++) {
		for (blk_x = blk_x_start; blk_x <= blk_x_comp; blk_x++) {
			if (!get_dre_block(phist, blk_x, blk_y, blk_x_num))
				return false;
		}
	}
	return true;
}

static bool aal_hist_check(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg, u32 *phist)
{
	struct mml_frame_config *cfg = NULL;
	u32 blk_x = 0, blk_y = 0;
	u8 pipe = 0;
	bool dual = false;
	struct aal_frame_data *aal_frm = NULL;
	u32 crop_width = 0;
	u32 crop_height = 0;
	u32 cut_pos_x = 0;
	u32 dre_blk_width = 0;
	u32 dre_blk_height = 0;
	u32 blk_x_start = 0;
	u32 dre_blk_y_num = 0, dre_blk_x_num = 0;

	if (IS_ERR_OR_NULL(task) || IS_ERR_OR_NULL(ccfg))
		return false;

	dual = task->config->dual;
	cfg = task->config;
	pipe = ccfg->pipe;
	aal_frm = aal_frm_data(ccfg);

	if (IS_ERR_OR_NULL(cfg) || IS_ERR_OR_NULL(aal_frm))
		return false;

	crop_width = cfg->frame_tile_sz.width;
	crop_height = cfg->frame_tile_sz.height;
	cut_pos_x = aal_frm->cut_pos_x;
	dre_blk_width = aal_frm->dre_blk_width;
	dre_blk_height = aal_frm->dre_blk_height;

	if (dual) {
		if (pipe == 1)
			dre_blk_x_num = (crop_width - cut_pos_x) / dre_blk_width;
		if (!pipe)
			dre_blk_x_num = cut_pos_x / dre_blk_width;

		blk_x_start = cut_pos_x / dre_blk_width + 1;
	} else {
		dre_blk_x_num =	crop_width / dre_blk_width;
		blk_x_start = 0;
	}

	dre_blk_y_num =	crop_height / dre_blk_height;

	mml_pq_msg("%s pipe[%d] crop_width[%u] crop_height[%u] cut_pos_x[%u]",
		__func__, pipe, crop_width, crop_height, cut_pos_x);
	mml_pq_msg("%s dre_blk_x_num[%u] dre_blk_y_num[%u] blk_x_start[%u]",
		__func__, dre_blk_x_num, dre_blk_y_num, blk_x_start);

	for (blk_y = 0; blk_y < dre_blk_y_num; blk_y++)
		for (blk_x = blk_x_start; blk_x < dre_blk_x_num; blk_x++)
			if (!get_dre_block(phist, blk_x, blk_y, dre_blk_x_num))
				return false;
	return true;
}

static bool aal_hist_read(struct mml_comp_aal *aal)
{
	struct mml_comp *comp = &aal->comp;
	u32 addr = aal->sram_hist_start;
	void __iomem *base = comp->base;
	s32 i;
	s32 dual_info_start = AAL_HIST_NUM;
	u32 poll_sram_status_addr = aal->data->reg_table[AAL_SRAM_STATUS];
	u32 *phist = aal->phist;

	for (i = 0; i < AAL_HIST_NUM; i++) {
		do {
			writel(addr,
				base + aal->data->reg_table[AAL_SRAM_RW_IF_2]);

			if (aal_reg_poll(comp, poll_sram_status_addr,
				(0x1 << 17), (0x1 << 17)) != true) {
				mml_pq_log("%s idx[%d]", __func__, i);
				break;
			}
			phist[i] = readl(base +
				aal->data->reg_table[AAL_SRAM_RW_IF_3]);
			} while (0);
		addr = addr + 4;
	}

	if (aal->dual) {
		phist[dual_info_start++] =
			readl(base + aal->data->reg_table[AAL_DUAL_PIPE_00]);
		phist[dual_info_start++] =
			readl(base + aal->data->reg_table[AAL_DUAL_PIPE_01]);
		phist[dual_info_start++] =
			readl(base + aal->data->reg_table[AAL_DUAL_PIPE_02]);
		phist[dual_info_start++] =
			readl(base + aal->data->reg_table[AAL_DUAL_PIPE_03]);
		phist[dual_info_start++] =
			readl(base + aal->data->reg_table[AAL_DUAL_PIPE_04]);
		phist[dual_info_start++] =
			readl(base + aal->data->reg_table[AAL_DUAL_PIPE_05]);
		phist[dual_info_start++] =
			readl(base + aal->data->reg_table[AAL_DUAL_PIPE_06]);
		phist[dual_info_start++] =
			readl(base + aal->data->reg_table[AAL_DUAL_PIPE_07]);
		phist[dual_info_start++] =
			readl(base + aal->data->reg_table[AAL_DUAL_PIPE_08]);
		phist[dual_info_start++] =
			readl(base + aal->data->reg_table[AAL_DUAL_PIPE_09]);
		phist[dual_info_start++] =
			readl(base + aal->data->reg_table[AAL_DUAL_PIPE_10]);
		phist[dual_info_start++] =
			readl(base + aal->data->reg_table[AAL_DUAL_PIPE_11]);
		phist[dual_info_start++] =
			readl(base + aal->data->reg_table[AAL_DUAL_PIPE_12]);
		phist[dual_info_start++] =
			readl(base + aal->data->reg_table[AAL_DUAL_PIPE_13]);
		phist[dual_info_start++] =
			readl(base + aal->data->reg_table[AAL_DUAL_PIPE_14]);
		phist[dual_info_start++] =
			readl(base + aal->data->reg_table[AAL_DUAL_PIPE_15]);
	}
	mml_pq_ir_aal_readback(aal->pq_task, aal->frame_data, aal->pipe, aal->phist,
		aal->jobid, aal->dual);

	if (mml_pq_debug_mode & MML_PQ_HIST_CHECK) {
		if (!aal_ir_hist_check(aal)) {
			mml_pq_err("%s hist error", __func__);
			mml_pq_util_aee("MML_PQ_AAL_Histogram Error",
				"AAL Histogram error need to check jobid:%d",
				aal->jobid);
		}
	}

	return true;
}

static void aal_task_done_readback(struct mml_comp *comp, struct mml_task *task,
					 struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct aal_frame_data *aal_frm = aal_frm_data(ccfg);
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_comp_aal *aal = comp_to_aal(comp);
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	u8 pipe = ccfg->pipe;
	bool vcp = aal->data->vcp_readback;
	u32 engine = MML_PQ_AAL0 + pipe;
	u32 offset = 0;
	s8 mode = cfg->info.mode;


	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s is_aal_need_readback[%d] id[%d] en_dre[%d]", __func__,
			aal_frm->is_aal_need_readback, comp->id, dest->pq_config.en_dre);

	mml_pq_msg("%s job_id[%d] pkt %p size %zu", __func__, task->job.jobid,
		pkt, pkt->cmd_buf_size);

	if (!dest->pq_config.en_dre)
		goto exit;

	if (aal_get_rb_mode(aal) == RB_SOF_MODE &&
		mode != MML_MODE_MML_DECOUPLE &&
		aal_frm->is_aal_need_readback) {

		mutex_lock(&aal->irq_wq_lock);

		mml_pq_msg("%s:aal jobid[%d] task jobid[%d]",
			__func__, aal->jobid, task->job.jobid);

		if (aal->jobid == task->job.jobid) {
			mutex_unlock(&aal->irq_wq_lock);
			queue_work(aal->aal_readback_wq, &aal->aal_readback_task);
		} else {
			mutex_unlock(&aal->irq_wq_lock);
		}

		goto exit;
	}

	if (!task->pq_task->aal_hist[pipe])
		goto exit;

	offset = vcp ? task->pq_task->aal_hist[pipe]->va_offset/4 : 0;

	mml_pq_rb_msg("%s job_id[%d] id[%d] pipe[%d] en_dre[%d] va[%p] pa[%llx] offset[%d]",
		__func__, task->job.jobid, comp->id, ccfg->pipe,
		dest->pq_config.en_dre, task->pq_task->aal_hist[pipe]->va,
		task->pq_task->aal_hist[pipe]->pa,
		task->pq_task->aal_hist[pipe]->va_offset);


	if (!pipe) {
		mml_pq_rb_msg("%s job_id[%d] hist[0~4]={%08x, %08x, %08x, %08x, %08x}",
			__func__, task->job.jobid,
			task->pq_task->aal_hist[pipe]->va[offset+0],
			task->pq_task->aal_hist[pipe]->va[offset+1],
			task->pq_task->aal_hist[pipe]->va[offset+2],
			task->pq_task->aal_hist[pipe]->va[offset+3],
			task->pq_task->aal_hist[pipe]->va[offset+4]);

		mml_pq_rb_msg("%s job_id[%d] hist[10~14]={%08x, %08x, %08x, %08x, %08x}",
			__func__, task->job.jobid,
			task->pq_task->aal_hist[pipe]->va[offset+10],
			task->pq_task->aal_hist[pipe]->va[offset+11],
			task->pq_task->aal_hist[pipe]->va[offset+12],
			task->pq_task->aal_hist[pipe]->va[offset+13],
			task->pq_task->aal_hist[pipe]->va[offset+14]);
	} else {
		mml_pq_rb_msg("%s job_id[%d] hist[600~604]={%08x, %08x, %08x, %08x, %08x",
			 __func__, task->job.jobid,
			task->pq_task->aal_hist[pipe]->va[offset+600],
			task->pq_task->aal_hist[pipe]->va[offset+601],
			task->pq_task->aal_hist[pipe]->va[offset+602],
			task->pq_task->aal_hist[pipe]->va[offset+603],
			task->pq_task->aal_hist[pipe]->va[offset+604]);

		mml_pq_rb_msg("%s job_id[%d] hist[610~614]={%08x, %08x, %08x, %08x, %08x",
			__func__, task->job.jobid,
			task->pq_task->aal_hist[pipe]->va[offset+610],
			task->pq_task->aal_hist[pipe]->va[offset+611],
			task->pq_task->aal_hist[pipe]->va[offset+612],
			task->pq_task->aal_hist[pipe]->va[offset+613],
			task->pq_task->aal_hist[pipe]->va[offset+614]);
	}


	/*remain code for ping-pong in the feature*/
	if (!dest->pq_config.en_dre)
		aal_hist_read(aal);

	if (aal_frm->is_aal_need_readback) {
		mml_pq_dc_aal_readback(task, ccfg->pipe,
			&(task->pq_task->aal_hist[pipe]->va[offset]));

		if (mml_pq_debug_mode & MML_PQ_HIST_CHECK) {
			if (!aal_hist_check(comp, task, ccfg,
				&(task->pq_task->aal_hist[pipe]->va[offset]))) {
				mml_pq_err("%s hist error", __func__);
				mml_pq_util_aee("MML_PQ_AAL_Histogram Error",
					"AAL Histogram error need to check jobid:%d",
					task->job.jobid);
			}
		}
	}

	if (aal_frm->is_clarity_need_readback) {

		mml_pq_rb_msg("%s job_id[%d] calrity_hist[0~4]={%08x, %08x, %08x, %08x, %08x",
			__func__, task->job.jobid,
			task->pq_task->aal_hist[pipe]->va[offset+AAL_HIST_NUM+AAL_DUAL_INFO_NUM+0],
			task->pq_task->aal_hist[pipe]->va[offset+AAL_HIST_NUM+AAL_DUAL_INFO_NUM+1],
			task->pq_task->aal_hist[pipe]->va[offset+AAL_HIST_NUM+AAL_DUAL_INFO_NUM+2],
			task->pq_task->aal_hist[pipe]->va[offset+AAL_HIST_NUM+AAL_DUAL_INFO_NUM+3],
			task->pq_task->aal_hist[pipe]->va[offset+AAL_HIST_NUM+AAL_DUAL_INFO_NUM+4]);


		mml_pq_clarity_readback(task, ccfg->pipe,
			&(task->pq_task->aal_hist[pipe]->va[
			offset+AAL_HIST_NUM+AAL_DUAL_INFO_NUM]),
			AAL_CLARITY_HIST_START, AAL_CLARITY_STATUS_NUM);
	}

	if (vcp) {
		mml_pq_put_vcp_buf_offset(task, engine, task->pq_task->aal_hist[pipe]);
		cmdq_vcp_enable(false);
	} else
		mml_pq_put_readback_buffer(task, pipe, &(task->pq_task->aal_hist[pipe]));
exit:
	mml_pq_trace_ex_end();
}

s32 aal_clk_enable(struct mml_comp *comp)
{
	u32 i;
	int ret;
	struct mml_comp_aal *aal = comp_to_aal(comp);

	comp->clk_cnt++;
	if (comp->clk_cnt > 1)
		return 0;
	if (comp->clk_cnt <= 0) {
		mml_err("%s comp %u %s cnt %d",
			__func__, comp->id, comp->name, comp->clk_cnt);
		return -EINVAL;
	}

	mml_mmp(clk_enable, MMPROFILE_FLAG_START, comp->id, 0);

	for (i = 0; i < ARRAY_SIZE(comp->clks); i++) {
		if (IS_ERR_OR_NULL(comp->clks[i]))
			break;
		ret = clk_prepare_enable(comp->clks[i]);
		if (ret)
			mml_err("%s clk_prepare_enable fail %d", __func__, ret);
		else
			aal->clk_on = true;
	}
	mml_mmp(clk_enable, MMPROFILE_FLAG_END, comp->id, 0);

	return 0;
}

static const struct mml_comp_hw_ops aal_hw_ops = {
	.clk_enable = &aal_clk_enable,
	.clk_disable = &mml_comp_clk_disable,
	.qos_clear = &mml_comp_qos_clear,
	.task_done = aal_task_done_readback,
};

static u32 read_reg_value(struct mml_comp *comp, u16 reg)
{
	void __iomem *base = comp->base;

	if (reg == REG_NOT_SUPPORT) {
		mml_err("%s aal reg is not support", __func__);
		return 0xFFFFFFFF;
	}

	return readl(base + reg);
}

static void aal_debug_dump(struct mml_comp *comp)
{
	struct mml_comp_aal *aal = comp_to_aal(comp);
	void __iomem *base = comp->base;
	u32 value[11];
	u32 shadow_ctrl;

	mml_err("aal component %u dump:", comp->id);

	/* Enable shadow read working */
	shadow_ctrl = read_reg_value(comp, aal->data->reg_table[AAL_SHADOW_CTRL]);
	shadow_ctrl |= 0x4;
	writel(shadow_ctrl, base + aal->data->reg_table[AAL_SHADOW_CTRL]);

	value[0] = readl(base + aal->data->reg_table[AAL_CFG]);
	mml_err("AAL_CFG %#010x", value[0]);

	value[0] = read_reg_value(comp, aal->data->reg_table[AAL_INTSTA]);
	value[1] = read_reg_value(comp, aal->data->reg_table[AAL_STATUS]);
	value[2] = read_reg_value(comp, aal->data->reg_table[AAL_INPUT_COUNT]);
	value[3] = read_reg_value(comp, aal->data->reg_table[AAL_OUTPUT_COUNT]);
	value[4] = read_reg_value(comp, aal->data->reg_table[AAL_SIZE]);
	value[5] = read_reg_value(comp, aal->data->reg_table[AAL_OUTPUT_SIZE]);
	value[6] = read_reg_value(comp, aal->data->reg_table[AAL_OUTPUT_OFFSET]);
	value[7] = read_reg_value(comp, aal->data->reg_table[AAL_TILE_00]);
	value[8] = read_reg_value(comp, aal->data->reg_table[AAL_TILE_01]);
	value[9] = read_reg_value(comp, aal->data->reg_table[AAL_TILE_02]);
	value[10] = read_reg_value(comp, aal->data->reg_table[AAL_CFG_MAIN]);

	mml_err("AAL_INTSTA %#010x AAL_STATUS %#010x AAL_INPUT_COUNT %#010x AAL_OUTPUT_COUNT %#010x",
		value[0], value[1], value[2], value[3]);
	mml_err("AAL_SIZE %#010x AAL_OUTPUT_SIZE %#010x AAL_OUTPUT_OFFSET %#010x",
		value[4], value[5], value[6]);
	mml_err("AAL_TILE_00 %#010x AAL_TILE_01 %#010x AAL_TILE_02 %#010x",
		value[7], value[8], value[9]);
	mml_err("AAL_CFG_MAIN %#010x",
		value[10]);
}

static const struct mml_comp_debug_ops aal_debug_ops = {
	.dump = &aal_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_aal *aal = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	s32 ret;

	if (!drm_dev) {
		ret = mml_register_comp(master, &aal->comp);
		if (ret)
			dev_err(dev, "Failed to register mml component %s: %d\n",
				dev->of_node->full_name, ret);
		aal->mml = dev_get_drvdata(master);
	} else {
		ret = mml_ddp_comp_register(drm_dev, &aal->ddp_comp);
		if (ret)
			dev_err(dev, "Failed to register ddp component %s: %d\n",
				dev->of_node->full_name, ret);
		else
			aal->ddp_bound = true;
	}
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_aal *aal = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	if (!drm_dev) {
		mml_unregister_comp(master, &aal->comp);
	} else {
		mml_ddp_comp_unregister(drm_dev, &aal->ddp_comp);
		aal->ddp_bound = false;
	}
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static const struct mtk_ddp_comp_funcs ddp_comp_funcs = {
};

static struct mml_comp_aal *dbg_probed_components[4];
static int dbg_probed_count;

static void aal_readback_work(struct work_struct *work_item)
{
	struct mml_comp_aal *aal = NULL;
	u32 *phist0 = NULL;
	struct mml_comp *comp = NULL;
	void __iomem *base = NULL;

	aal = container_of(work_item, struct mml_comp_aal, aal_readback_task);
	phist0 = aal->phist;
	comp = &aal->comp;
	base = comp->base;

	/* MT6989 platform, RB_SOF_MODE will use DPC to power on. */

	mutex_lock(&aal->irq_wq_lock);
	if (mml_pq_aal_hist_reading(aal->pq_task, aal->out_idx, aal->pipe)) {
		mutex_unlock(&aal->irq_wq_lock);
		return;
	}
	writel(0x0, base + aal->data->reg_table[AAL_INTEN]);
	mutex_unlock(&aal->irq_wq_lock);

	aal_hist_read(aal);

	mml_pq_put_pq_task(aal->pq_task);

	mml_pq_aal_flag_check(aal->dual, aal->out_idx);

	/* MT6989 platform, RB_SOF_MODE will use DPC to power off.
	 * MT6985 platform, RB_SOF_MODE use CCF to power off.
	 */
	if (aal_get_rb_mode(aal) == RB_EOF_MODE ||
		aal_get_rb_mode(aal) == RB_SOF_MODE) {
		mml_clock_lock(aal->mml);
		call_hw_op(comp, clk_disable, aal->dpc);
		/* dpc exception flow off */
		mml_msg_dpc("%s dpc exception flow off", __func__);
		mml_dpc_exc_release(aal->mml);
		/* ccf power off */
		call_hw_op(aal->mmlsys_comp, pw_disable);
		call_hw_op(aal->mmlsys_comp, mminfra_pw_disable);
		mml_clock_unlock(aal->mml);
		mml_lock_wake_lock(aal->mml, false);
	}

}

static void clarity_histdone_cb(struct cmdq_cb_data data)
{
	struct cmdq_pkt *pkt = (struct cmdq_pkt *)data.data;
	struct mml_comp_aal *aal = (struct mml_comp_aal *)pkt->user_data;
	struct mml_comp *comp = &aal->comp;
	u32 pipe;

	mml_pq_ir_log("%s jobid[%d] hist_pkts[0] = [%p] hdr->hist_pkts[1] = [%p]",
		__func__, aal->jobid, aal->hist_pkts[0], aal->hist_pkts[1]);

	if (pkt == aal->hist_pkts[0]) {
		pipe = 0;
	} else if (pkt == aal->hist_pkts[1]) {
		pipe = 1;
	} else {
		mml_err("%s task %p pkt %p not match both pipe (%p and %p)",
			__func__, aal, pkt, aal->hist_pkts[0], aal->hist_pkts[1]);
		return;
	}

	mml_pq_ir_log("%s hist[0~7]={%08x, %08x, %08x, %08x, %08x, %08x, %08x, %08x}",
		__func__,
		aal->clarity_hist[pipe]->va[0],
		aal->clarity_hist[pipe]->va[1],
		aal->clarity_hist[pipe]->va[2],
		aal->clarity_hist[pipe]->va[3],
		aal->clarity_hist[pipe]->va[4],
		aal->clarity_hist[pipe]->va[5],
		aal->clarity_hist[pipe]->va[6],
		aal->clarity_hist[pipe]->va[7]);

	mml_pq_ir_clarity_readback(aal->pq_task, aal->frame_data, aal->pipe,
		&(aal->clarity_hist[pipe]->va[0]), aal->jobid, AAL_CLARITY_STATUS_NUM,
		AAL_CLARITY_HIST_START, aal->dual);

	if (aal_get_rb_mode(aal) == RB_EOF_MODE) {
		mml_clock_lock(aal->mml);
		call_hw_op(comp, clk_disable, aal->dpc);
		/* dpc exception flow off */
		mml_msg_dpc("%s dpc exception flow off", __func__);
		mml_dpc_exc_release(aal->mml);
		/* ccf power off */
		call_hw_op(aal->mmlsys_comp, pw_disable);
		call_hw_op(aal->mmlsys_comp, mminfra_pw_disable);
		mml_clock_unlock(aal->mml);
		mml_lock_wake_lock(aal->mml, false);
	}

	mml_pq_put_pq_task(aal->pq_task);

	mml_pq_aal_flag_check(aal->dual, aal->out_idx);

	mml_pq_ir_log("%s end jobid[%d] pkt[%p] hdr[%p] pipe[%d]",
		__func__, aal->jobid, pkt, aal, pipe);
	mml_trace_end();
}


static void clarity_hist_work(struct work_struct *work_item)
{
	struct mml_comp_aal *aal = NULL;
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

	aal = container_of(work_item, struct mml_comp_aal, clarity_hist_task);

	if (!aal) {
		mml_pq_err("%s comp_hdr is null", __func__);
		return;
	}

	pipe = aal->pipe;
	comp = &aal->comp;
	base_pa = comp->base_pa;
	pkt = aal->hist_pkts[pipe];
	idx_out = aal->data->cpr[aal->pipe];
	pq_task = aal->pq_task;


	mml_pq_ir_log("%s job_id[%d] eng_id[%d] cmd_buf_size[%zu] hist_cmd_done[%d]",
		__func__, aal->jobid, comp->id,
		pkt->cmd_buf_size, aal->hist_cmd_done);

	idx_out64 = CMDQ_CPR_TO_CPR64(idx_out);

	if (unlikely(!aal->clarity_hist[pipe])) {
		mml_pq_err("%s job_id[%d] eng_id[%d] pipe[%d] pkt[%p] clarity_hist is null",
			__func__, aal->jobid, comp->id, pipe,
			aal->hist_pkts[pipe]);
		return;
	}

	mutex_lock(&aal->hist_cmd_lock);
	if (aal->hist_cmd_done) {
		mutex_unlock(&aal->hist_cmd_lock);
		goto aal_hist_cmd_done;
	}

	cmdq_pkt_wfe(pkt, aal->event_eof);

	pa = aal->clarity_hist[pipe]->pa;

	/* readback to this pa */
	cmdq_pkt_assign_command(pkt, idx_out, (u32)pa);
	cmdq_pkt_assign_command(pkt, idx_out + 1, (u32)(pa >> 32));

	if (aal->data->reg_table[AAL_BILATERAL_STATUS_00] != REG_NOT_SUPPORT) {
		for (i = 0; i < AAL_CLARITY_STATUS_NUM; i++) {
			cmdq_pkt_read_addr(pkt,
				base_pa + aal->data->reg_table[AAL_BILATERAL_STATUS_00] + i * 4,
				idx_val);
			cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

			lop.reg = true;
			lop.idx = idx_out;
			rop.reg = false;
			rop.value = 4;
			cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);
		}
	}

	mml_pq_rb_msg("%s end job_id[%d] engine_id[%d] va[%p] pa[%llx] pkt[%p]",
		__func__, aal->jobid, comp->id, aal->clarity_hist[pipe]->va,
		aal->clarity_hist[pipe]->pa, pkt);

	aal->hist_cmd_done = true;

	mml_pq_rb_msg("%s end engine_id[%d] va[%p] pa[%llx] pkt[%p]",
		__func__, comp->id, aal->clarity_hist[pipe]->va,
		aal->clarity_hist[pipe]->pa, pkt);

	pkt->user_data = aal;
	mutex_unlock(&aal->hist_cmd_lock);

aal_hist_cmd_done:
	if (aal->frame_data.info.dest[0].pq_config.en_hdr)
		wait_for_completion(&aal->pq_task->hdr_curve_ready[aal->pipe]);

	cmdq_pkt_refinalize(pkt);
	cmdq_pkt_flush_threaded(pkt, clarity_histdone_cb, (void *)aal->hist_pkts[pipe]);

	mml_pq_ir_log("%s job_id[%d] hist_pkts[%p %p] id[%d] buf_size[%zu] hist_cmd_done[%d]",
		__func__, aal->jobid,
		aal->hist_pkts[0], aal->hist_pkts[1], comp->id,
		pkt->cmd_buf_size, aal->hist_cmd_done);

}


static irqreturn_t mml_aal_irq_handler(int irq, void *dev_id)
{
	struct mml_comp_aal *priv = dev_id;
	struct mml_comp *comp = &priv->comp;
	irqreturn_t ret = IRQ_NONE;
	void __iomem *base = comp->base;

	if (readl(base + priv->data->reg_table[AAL_INTEN]) &&
		readl(base + priv->data->reg_table[AAL_INTSTA])) {
		writel(0x0, base + priv->data->reg_table[AAL_INTSTA]);
		queue_work(priv->aal_readback_wq, &priv->aal_readback_task);
		ret = IRQ_HANDLED;
	}

	mml_mmp(dle_aal_irq_done, MMPROFILE_FLAG_PULSE, comp->id, 0);

	return ret;
}

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_aal *priv;
	s32 ret;
	int irq = -1;

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

	if (of_property_read_u32(dev->of_node, "sram-curve-base", &priv->sram_curve_start))
		dev_err(dev, "read curve base fail\n");

	if (of_property_read_u32(dev->of_node, "sram-his-base", &priv->sram_hist_start))
		dev_err(dev, "read his base fail\n");

	if (of_property_read_u8(dev->of_node, "hist-read-mode", &priv->force_rb_mode))
		priv->force_rb_mode = -1;

	/* assign ops */
	priv->comp.tile_ops = &aal_tile_ops;
	priv->comp.config_ops = &aal_cfg_ops;
	priv->comp.hw_ops = &aal_hw_ops;
	priv->comp.debug_ops = &aal_debug_ops;

	if (aal_get_rb_mode(priv) == RB_EOF_MODE) {
		irq = platform_get_irq(pdev, 0);
		if (irq < 0)
			dev_info(dev, "Failed to get AAL irq: %d\n", irq);
		else {
			ret = devm_request_irq(dev, irq, mml_aal_irq_handler,
				IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev), priv);
			if (ret)
				dev_info(dev, "register irq fail: %d irg:%d\n", ret, irq);
			else
				dev_info(dev, "register irq success: %d irg:%d\n", ret, irq);
		}
	}
	priv->irq = irq;

	mutex_init(&priv->irq_wq_lock);
	dev_info(dev, "devm_request_irq success: %d irg:%d, %d\n",
		ret, irq, priv->irq);

	priv->phist = kzalloc((AAL_HIST_NUM+AAL_DUAL_INFO_NUM)*sizeof(u32),
			GFP_KERNEL);

	priv->aal_readback_wq = create_singlethread_workqueue("aal_readback");
	INIT_WORK(&priv->aal_readback_task, aal_readback_work);

	priv->hist_read_idx = 1;
	priv->hist_sram_idx = 1;

	if (of_property_read_u16(dev->of_node, "event-frame-done",
				 &priv->event_eof))
		dev_err(dev, "read event frame_done fail\n");

	priv->clarity_hist_wq = create_singlethread_workqueue("clarity_hist_read");
	INIT_WORK(&priv->clarity_hist_task, clarity_hist_work);

	mutex_init(&priv->hist_cmd_lock);

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

const struct of_device_id mml_aal_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6983-mml_aal",
		.data = &mt6983_aal_data,
	},
	{
		.compatible = "mediatek,mt6893-mml_aal",
		.data = &mt6893_aal_data
	},
	{
		.compatible = "mediatek,mt6879-mml_aal",
		.data = &mt6879_aal_data
	},
	{
		.compatible = "mediatek,mt6895-mml_aal0",
		.data = &mt6895_aal0_data
	},
	{
		.compatible = "mediatek,mt6895-mml_aal1",
		.data = &mt6895_aal1_data
	},
	{
		.compatible = "mediatek,mt6985-mml_aal",
		.data = &mt6985_aal_data
	},
	{
		.compatible = "mediatek,mt6886-mml_aal",
		.data = &mt6886_aal_data
	},
	{
		.compatible = "mediatek,mt6897-mml_aal",
		.data = &mt6897_aal_data
	},
	{
		.compatible = "mediatek,mt6989-mml_aal",
		.data = &mt6989_aal_data
	},
	{
		.compatible = "mediatek,mt6878-mml_aal",
		.data = &mt6878_aal_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_aal_driver_dt_match);

struct platform_driver mml_aal_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-aal",
		.owner = THIS_MODULE,
		.of_match_table = mml_aal_driver_dt_match,
	},
};

//module_platform_driver(mml_aal_driver);

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
				"  - [%d] mml comp_id: %d.%d @%llx name: %s bound: %d\n", i,
				comp->id, comp->sub_idx, comp->base_pa,
				comp->name ? comp->name : "(null)", comp->bound);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -         larb_port: %d @%llx pw: %d clk: %d\n",
				comp->larb_port, comp->larb_base,
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
module_param_cb(aal_debug, &dbg_param_ops, NULL, 0644);
MODULE_PARM_DESC(aal_debug, "mml aal debug case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML AAL driver");
MODULE_LICENSE("GPL v2");
