// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/string.h>

#include "sfr/pablo-sfr-common-ctrl-v1_3.h"
#include "pmio.h"
#include "is-hw-common-dma.h"
#include "pablo-hw-api-common-ctrl.h"

#define PCC_TRY_COUNT 20000 /* 20ms */
#define MAX_COTF_IN_NUM 8
#define MAX_COTF_OUT_NUM 6
#define INT_HIST_NUM 8
#define PCC_IRQ_HANDLER_NUM 32 /* 32 bits per 1 IRQ*/

/* PMIO MACRO */
#define SET_CR(base, R, val) PMIO_SET_R(base, R, val)
#define SET_CR_F(base, R, F, val) PMIO_SET_F(base, R, F, val)
#define SET_CR_V(base, reg_val, F, val) PMIO_SET_V(base, reg_val, F, val)

#define GET_CR(base, R) PMIO_GET_R(base, R)
#define GET_CR_F(base, R, F) PMIO_GET_F(base, R, F)
#define GET_CR_V(base, F, val) _get_pmio_field_val(base, F, val)

/* LOG MACRO */
#define pcc_err(fmt, pcc, args...)                                                                 \
	pr_err("[@][%s][PCC][ERR]%s:%d:" fmt "\n", pcc->name, __func__, __LINE__, ##args)
#define pcc_warn(fmt, pcc, args...)                                                                \
	pr_warn("[@][%s][PCC][WRN]%s:%d:" fmt "\n", pcc->name, __func__, __LINE__, ##args)
#define pcc_info(fmt, pcc, args...)                                                                \
	pr_info("[@][%s][PCC]%s:%d:" fmt "\n", pcc->name, __func__, __LINE__, ##args)
#define pcc_dump(fmt, pcc, args...) pr_info("[@][%s][PCC][DUMP] " fmt "\n", pcc->name, ##args)

struct cr_field_map {
	u32 cr_offset;
	u32 field_id;
};

struct int_cr_map {
	struct cr_field_map src;
	struct cr_field_map clr;
};

struct cmdq_queue_status {
	u32 fullness;
	u32 wptr;
	u32 rptr;
};

struct cmdq_frame_id {
	u32 cmd_id;
	u32 frame_id;
};

struct cmdq_dbg_info {
	u32 state;
	u32 cmd_cnt;
	struct cmdq_queue_status queue;
	struct cmdq_frame_id pre;
	struct cmdq_frame_id cur;
	struct cmdq_frame_id next;
	bool charged;
};

typedef void (*int_handler)(struct pablo_common_ctrl *pcc);
struct irq_handler {
	int_handler func;
};

/* HW specified constants */
static const struct cr_field_map cotf_in_cr[MAX_COTF_IN_NUM] = {
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_01,
		.field_id = CMN_CTRL_F_IP_USE_OTF_IN_FOR_PATH_0,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_01,
		.field_id = CMN_CTRL_F_IP_USE_OTF_IN_FOR_PATH_1,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_23,
		.field_id = CMN_CTRL_F_IP_USE_OTF_IN_FOR_PATH_2,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_23,
		.field_id = CMN_CTRL_F_IP_USE_OTF_IN_FOR_PATH_3,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_45,
		.field_id = CMN_CTRL_F_IP_USE_OTF_IN_FOR_PATH_4,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_45,
		.field_id = CMN_CTRL_F_IP_USE_OTF_IN_FOR_PATH_5,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_67,
		.field_id = CMN_CTRL_F_IP_USE_OTF_IN_FOR_PATH_6,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_67,
		.field_id = CMN_CTRL_F_IP_USE_OTF_IN_FOR_PATH_7,
	},
};

static const struct cr_field_map cotf_out_cr[MAX_COTF_OUT_NUM] = {
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_01,
		.field_id = CMN_CTRL_F_IP_USE_OTF_OUT_FOR_PATH_0,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_01,
		.field_id = CMN_CTRL_F_IP_USE_OTF_OUT_FOR_PATH_1,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_23,
		.field_id = CMN_CTRL_F_IP_USE_OTF_OUT_FOR_PATH_2,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_23,
		.field_id = CMN_CTRL_F_IP_USE_OTF_OUT_FOR_PATH_3,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_45,
		.field_id = CMN_CTRL_F_IP_USE_OTF_OUT_FOR_PATH_4,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_45,
		.field_id = CMN_CTRL_F_IP_USE_OTF_OUT_FOR_PATH_5,
	},
};

static const struct int_cr_map int_cr_maps[PCC_INT_ID_NUM] = {
	[PCC_INT_0] = {
		.src = {
			.cr_offset = CMN_CTRL_R_INT_REQ_INT0,
			.field_id = CMN_CTRL_F_INT_REQ_INT0,
		},
		.clr = {
			.cr_offset = CMN_CTRL_R_INT_REQ_INT0_CLEAR,
			.field_id = CMN_CTRL_F_INT_REQ_INT0_CLEAR,
		},
	},
	[PCC_INT_1] = {
		.src = {
			.cr_offset = CMN_CTRL_R_INT_REQ_INT1,
			.field_id = CMN_CTRL_F_INT_REQ_INT1,
		},
		.clr = {
			.cr_offset = CMN_CTRL_R_INT_REQ_INT1_CLEAR,
			.field_id = CMN_CTRL_F_INT_REQ_INT1_CLEAR,
		},
	},
	[PCC_CMDQ_INT] = {
		.src = {
			.cr_offset = CMN_CTRL_R_CMDQ_INT,
			.field_id = CMN_CTRL_F_CMDQ_INT,
		},
		.clr = {
			.cr_offset = CMN_CTRL_R_CMDQ_INT_CLEAR,
			.field_id = CMN_CTRL_F_CMDQ_INT_CLEAR,
		},
	},
	[PCC_COREX_INT] = {
		.src = {
			.cr_offset = CMN_CTRL_R_COREX_INT,
			.field_id = CMN_CTRL_F_COREX_INT,
		},
		.clr = {
			.cr_offset = CMN_CTRL_R_COREX_INT_CLEAR,
			.field_id = CMN_CTRL_F_COREX_INT_CLEAR,
		},
	},
};

#define CMDQ_STATE_STR_LENGTH 20
static const char cmdq_state_str[CMDQ_STATE_NUM][CMDQ_STATE_STR_LENGTH] = {
	"IDLE",
	"ACTIVE_CMD_POPPED",
	"PRE_CONFIG",
	"PRE_START",
	"FRAME",
	"POST_FRAME",
};

#define CMD_ID_STR_LENGTH 3
static const char cmd_id_str[][CMD_ID_STR_LENGTH] = {
	"NOR",
	"DUM",
};

/* PCC internal functions */
static inline u32 _get_pmio_field_val(struct pablo_mmio *pmio, u32 F, u32 val)
{
	struct pmio_field *field;

	field = pmio_get_field(pmio, F);
	if (!field)
		return 0;

	val &= field->mask;
	val >>= field->shift;

	return val;
}

static int _init_pmio(struct pablo_common_ctrl *pcc, struct pablo_mmio *hw_pmio)
{
	int ret;
	struct pmio_config pcfg;
	struct pablo_mmio *pcc_pmio;

	if (pcc->pmio) {
		pcc_err("It already has PMIO.", pcc);
		goto err_init;
	}

	memset(&pcfg, 0, sizeof(struct pmio_config));

	pcfg.name = pcc->name;
	pcfg.phys_base = hw_pmio->phys_base;
	pcfg.mmio_base = hw_pmio->mmio_base;
	pcfg.cache_type = PMIO_CACHE_NONE;

	pcfg.max_register = CMN_CTRL_R_FREEZE_CORRUPTED_ENABLE;
	pcfg.num_reg_defaults_raw = (pcfg.max_register / PMIO_REG_STRIDE) + 1;
	pcfg.fields = cmn_ctrl_field_descs;
	pcfg.num_fields = ARRAY_SIZE(cmn_ctrl_field_descs);

	pcc_pmio = pmio_init(NULL, NULL, &pcfg);
	if (IS_ERR(pcc_pmio)) {
		pcc_err("Failed to init PMIO. ret(%ld)", pcc, PTR_ERR(pcc_pmio));
		goto err_init;
	}

	ret = pmio_field_bulk_alloc(pcc_pmio, &pcc->fields, pcfg.fields, pcfg.num_fields);
	if (ret) {
		pcc_err("Failed to alloc PMIO field bulk. ret(%d)", pcc, ret);
		goto err_field_bulk_alloc;
	}

	pcc->pmio = pcc_pmio;

	return 0;

err_field_bulk_alloc:
	pmio_exit(pcc_pmio);
err_init:
	return -EINVAL;
}

static int _init_cloader(struct pablo_common_ctrl *pcc, struct is_mem *mem)
{
	struct pablo_internal_subdev *pis;
	int ret;
	u32 reg_num, reg_size, header_size;

	if (!mem) {
		pcc_err("mem is NULL.", pcc);
		return -EINVAL;
	}

	pis = &pcc->subdev_cloader;
	ret = pablo_internal_subdev_probe(pis, 0, mem, "CLOADER");
	if (ret) {
		pcc_err("Failed to probe internal sub-device for CLOADER. ret(%d)", pcc, ret);
		return ret;
	}

	reg_num = (pcc->pmio->max_register / PMIO_REG_STRIDE) + 1;
	reg_size = reg_num * PMIO_REG_STRIDE;
	header_size = ALIGN(reg_num, 16);

	pis->width = header_size + reg_size;
	pis->height = 1;
	pis->num_planes = 1;
	pis->num_batch = 1;
	pis->num_buffers = 1;
	pis->bits_per_pixel = BITS_PER_BYTE;
	pis->memory_bitwidth = BITS_PER_BYTE;
	pis->size[0] = ALIGN(DIV_ROUND_UP(pis->width * pis->memory_bitwidth, BITS_PER_BYTE), 32) *
		       pis->height;

	ret = CALL_I_SUBDEV_OPS(pis, alloc, pis);
	if (ret) {
		pcc_err("Failed to alloc internal sub-device for CLOADER. ret(%d)", pcc, ret);
		return ret;
	}

	return 0;
}

static int _wait_cmdq_idle(struct pablo_common_ctrl *pcc)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 retry = PCC_TRY_COUNT;
	u32 val;
	bool busy;

	do {
		val = GET_CR_F(pmio, CMN_CTRL_R_CMDQ_DEBUG_STATUS, CMN_CTRL_F_CMDQ_DEBUG_PROCESS);
		busy = val & CMDQ_BUSY_STATE;
		if (busy)
			udelay(1);
		else
			break;
	} while (--retry);

	if (busy) {
		pcc_warn("timeout(0x%x)", pcc, val);
		return -ETIME;
	}

	return 0;
}

static void _cmdq_lock(struct pablo_common_ctrl *pcc)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 val = 0;

	spin_lock_irqsave(&pcc->cmdq_lock, pcc->irq_flags);

	val = SET_CR_V(pmio, val, CMN_CTRL_F_CMDQ_POP_LOCK, 1);
	val = SET_CR_V(pmio, val, CMN_CTRL_F_CMDQ_RELOAD_LOCK, 1);

	SET_CR(pmio, CMN_CTRL_R_CMDQ_LOCK, val);

	_wait_cmdq_idle(pcc);
}

static inline void _cmdq_unlock(struct pablo_common_ctrl *pcc)
{
	SET_CR(pcc->pmio, CMN_CTRL_R_CMDQ_LOCK, 0);

	spin_unlock_irqrestore(&pcc->cmdq_lock, pcc->irq_flags);
}

static int _sw_reset(struct pablo_common_ctrl *pcc)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 retry = PCC_TRY_COUNT;
	u32 val;

	_cmdq_lock(pcc);

	pcc_info("E", pcc);
	SET_CR(pmio, CMN_CTRL_R_SW_RESET, 1);

	do {
		val = GET_CR(pmio, CMN_CTRL_R_SW_RESET);
		if (val)
			udelay(1);
		else
			break;
	} while (--retry);

	_cmdq_unlock(pcc);

	if (val) {
		pcc_err("timeout(0x%x)", pcc, val);
		return -ETIME;
	}

	pcc_info("X", pcc);

	return 0;
}

static void _s_cotf(struct pablo_common_ctrl *pcc, struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 pos, val;

	/* COTF_IN */
	for (pos = 0; pos < MAX_COTF_IN_NUM; pos++) {
		val = (frame_cfg->cotf_in_en & BIT_MASK(pos)) ? 1 : 0;
		SET_CR_F(pmio, cotf_in_cr[pos].cr_offset, cotf_in_cr[pos].field_id, val);
	}

	/* COTF_OUT */
	for (pos = 0; pos < MAX_COTF_OUT_NUM; pos++) {
		val = (frame_cfg->cotf_out_en & BIT_MASK(pos)) ? 1 : 0;
		SET_CR_F(pmio, cotf_out_cr[pos].cr_offset, cotf_out_cr[pos].field_id, val);
	}
}

static void _s_post_frame_gap(struct pablo_common_ctrl *pcc, u32 delay)
{
	if (delay < MIN_POST_FRAME_GAP)
		delay = MIN_POST_FRAME_GAP;

	SET_CR_F(pcc->pmio, CMN_CTRL_R_IP_POST_FRAME_GAP, CMN_CTRL_F_IP_POST_FRAME_GAP, delay);
}

static void _flush_cmdq_queue(struct pablo_common_ctrl *pcc)
{
	/*
	 * This reset below CR values, too.
	 *  - cmd_h/m/l
	 *  - queue_wptr
	 *  - queue_rptr
	 *  - queue_fullness
	 */
	SET_CR_F(pcc->pmio, CMN_CTRL_R_CMDQ_FLUSH_QUEUE_0, CMN_CTRL_F_CMDQ_FLUSH_QUEUE_0, 1);
}

static int _s_cmd(struct pablo_common_ctrl *pcc, struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	struct pablo_mmio *pmio = pcc->pmio;
	struct pablo_common_ctrl_cmd *cmd = &frame_cfg->cmd;
	u32 set_mode = cmd->set_mode, header_num = 0;
	u32 base_addr, val, int_grp_en, fro_id;

	/* C-loader configuration */
	switch (set_mode) {
	case PCC_DMA_PRELOADING:
	case PCC_DMA_DIRECT:
		base_addr = DVA_36BIT_HIGH(cmd->base_addr);
		if (base_addr) {
			header_num = cmd->header_num;
		} else {
			pcc_warn("Missing base_addr. Force APB_DIRECT mode.", pcc);
			set_mode = PCC_APB_DIRECT;
		}
		break;
	case PCC_COREX:
	case PCC_APB_DIRECT:
		base_addr = 0;
		header_num = 0;
		break;
	default:
		pcc_err("Invalid CMDQ setting mode(%d)", pcc, set_mode);
		return -EINVAL;
	}

	/* TODO: Need to care about FRO */
	int_grp_en = cmd->int_grp_en;
	fro_id = 0;

	_cmdq_lock(pcc);

	if (!fro_id)
		_flush_cmdq_queue(pcc);

	SET_CR_F(pmio, CMN_CTRL_R_CMDQ_QUE_CMD_H, CMN_CTRL_F_CMDQ_QUE_CMD_BASE_ADDR, base_addr);

	val = 0;
	val = SET_CR_V(pmio, val, CMN_CTRL_F_CMDQ_QUE_CMD_HEADER_NUM, header_num);
	val = SET_CR_V(pmio, val, CMN_CTRL_F_CMDQ_QUE_CMD_SETTING_MODE, set_mode);
	val = SET_CR_V(pmio, val, CMN_CTRL_F_CMDQ_QUE_CMD_HOLD_MODE, 0); /* not used */
	val = SET_CR_V(pmio, val, CMN_CTRL_F_CMDQ_QUE_CMD_CMD_ID, cmd->cmd_id);
	val = SET_CR_V(pmio, val, CMN_CTRL_F_CMDQ_QUE_CMD_FRAME_ID, cmd->fcount);
	SET_CR(pmio, CMN_CTRL_R_CMDQ_QUE_CMD_M, val);

	val = 0;
	val = SET_CR_V(pmio, val, CMN_CTRL_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE, int_grp_en);
	val = SET_CR_V(pmio, val, CMN_CTRL_F_CMDQ_QUE_CMD_FRO_INDEX, fro_id);
	SET_CR(pmio, CMN_CTRL_R_CMDQ_QUE_CMD_L, val);

	/* This triggers HW frame processing. */
	SET_CR(pcc->pmio, CMN_CTRL_R_CMDQ_ADD_TO_QUEUE_0, 1);

	_cmdq_unlock(pcc);

	return 0;
}

static void _get_cmdq_queue_status(struct pablo_common_ctrl *pcc, struct cmdq_queue_status *status)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 val;

	val = GET_CR(pmio, CMN_CTRL_R_CMDQ_QUEUE_0_INFO);
	status->fullness = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_QUEUE_0_FULLNESS, val);
	status->wptr = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_QUEUE_0_WPTR, val);
	status->rptr = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_QUEUE_0_RPTR, val);
}

static void _get_cmdq_last_cmd(struct pablo_common_ctrl *pcc)
{
	struct pablo_mmio *pmio = pcc->pmio;
	struct pmio_field *field;
	struct pablo_common_ctrl_cmd *last_cmd;
	u32 last_rptr, val;

	/* Get CMDQ previous read pointer with the way of circular buffer. */
	field = pmio_get_field(pmio, CMN_CTRL_F_CMDQ_QUEUE_0_RPTR_FOR_DEBUG);

	last_rptr = GET_CR_F(pmio, CMN_CTRL_R_CMDQ_QUEUE_0_INFO, CMN_CTRL_F_CMDQ_QUEUE_0_RPTR);
	last_rptr = (last_rptr + (field->mask >> field->shift)) & field->mask;
	pcc->dbg.last_rptr = last_rptr;

	/* Move CMDQ debug read pointer to last read pointer */
	SET_CR_F(pmio, CMN_CTRL_R_CMDQ_QUEUE_0_RPTR_FOR_DEBUG,
		CMN_CTRL_F_CMDQ_QUEUE_0_RPTR_FOR_DEBUG, last_rptr);

	last_cmd = &pcc->dbg.last_cmd;
	memset(last_cmd, 0, sizeof(struct pablo_common_ctrl_cmd));

	val = GET_CR(pmio, CMN_CTRL_R_CMDQ_DEBUG_QUE_0_CMD_H);
	last_cmd->base_addr = (dma_addr_t)GET_CR_V(pmio, CMN_CTRL_F_CMDQ_QUE_CMD_BASE_ADDR, val)
			      << LSB_BIT;

	val = GET_CR(pmio, CMN_CTRL_R_CMDQ_DEBUG_QUE_0_CMD_M);
	last_cmd->header_num = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_QUE_CMD_HEADER_NUM, val);
	last_cmd->set_mode = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_QUE_CMD_SETTING_MODE, val);
	last_cmd->cmd_id = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_QUE_CMD_CMD_ID, val);
	last_cmd->fcount = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_QUE_CMD_FRAME_ID, val);

	val = GET_CR(pmio, CMN_CTRL_R_CMDQ_DEBUG_QUE_0_CMD_L);
	last_cmd->int_grp_en = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE, val);
	last_cmd->fro_id = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_QUE_CMD_FRO_INDEX, val);
}

static void _prepare_dummy_cmd(struct pablo_common_ctrl *pcc, struct pablo_common_ctrl_cmd *cmd)
{
	struct pablo_common_ctrl_cmd *last_cmd;

	last_cmd = &pcc->dbg.last_cmd;

	cmd->set_mode = PCC_APB_DIRECT;
	cmd->cmd_id = CMD_DUMMY;
	cmd->fcount = last_cmd->fcount;
	cmd->int_grp_en = last_cmd->int_grp_en;
}

static void _get_cmdq_frame_id(
	struct pablo_common_ctrl *pcc, struct cmdq_frame_id *pre, struct cmdq_frame_id *cur)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 val;

	val = GET_CR(pmio, CMN_CTRL_R_CMDQ_FRAME_ID);

	pre->cmd_id = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_PRE_CMD_ID, val);
	pre->frame_id = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_PRE_FRAME_ID, val);
	cur->cmd_id = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_CURRENT_CMD_ID, val);
	cur->frame_id = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_CURRENT_FRAME_ID, val);
}

static void _frame_start_handler(struct pablo_common_ctrl *pcc)
{
	struct cmdq_dbg_info dbg;

	_get_cmdq_frame_id(pcc, &dbg.pre, &dbg.cur);

	pcc->dbg.last_fcount = dbg.cur.frame_id;
}

static void _setting_done_m2m_handler(struct pablo_common_ctrl *pcc)
{
	_get_cmdq_last_cmd(pcc);
}

static void _setting_done_otf_handler(struct pablo_common_ctrl *pcc)
{
	struct cmdq_queue_status status;
	struct pablo_common_ctrl_frame_cfg dummy_cfg;

	_get_cmdq_last_cmd(pcc);

	memset(&status, 0, sizeof(status));

	_get_cmdq_queue_status(pcc, &status);

	/* Since there is remaining cmd, there is nothing to do. */
	if (status.fullness)
		return;

	memset(&dummy_cfg, 0, sizeof(dummy_cfg));

	_prepare_dummy_cmd(pcc, &dummy_cfg.cmd);
	_s_cmd(pcc, &dummy_cfg);
}

static struct irq_handler irq_handlers[PCC_INT_ID_NUM][PCC_IRQ_HANDLER_NUM][PCC_MODE_NUM] = {
	[PCC_INT_0] = {
		[CMN_CTRL_INT0_FRAME_START_INT] = {
			[PCC_OTF] = {
				.func = _frame_start_handler,
			},
			[PCC_M2M] = {
				.func = _frame_start_handler,
			},
		},
		[CMN_CTRL_INT0_SETTING_DONE_INT] = {
			[PCC_OTF] = {
				.func = _setting_done_otf_handler,
			},
			[PCC_M2M] = {
				.func = _setting_done_m2m_handler,
			},
		},
	},
	[PCC_INT_1] = {
		/* Nothing to do */
	},
	[PCC_CMDQ_INT] = {
		/* Nothing to do */
	},
	[PCC_COREX_INT] = {
		/* Nothing to do */
	},
};

static u32 _get_int_status(struct pablo_common_ctrl *pcc, u32 src_cr, u32 clr_cr, bool clear)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 status;

	status = GET_CR(pmio, src_cr);

	if (clear)
		SET_CR(pmio, clr_cr, status);

	return status;
}

static void _dump_cotf(struct pablo_common_ctrl *pcc)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 i;

	pcc_dump("COTF IN **********", pcc);
	for (i = 0; i < MAX_COTF_IN_NUM; i++) {
		if (GET_CR_F(pmio, cotf_in_cr[i].cr_offset, cotf_in_cr[i].field_id))
			pcc_dump("[IN%d] ON", pcc, i);
	}

	pcc_dump("COTF OUT **********", pcc);
	for (i = 0; i < MAX_COTF_OUT_NUM; i++) {
		if (GET_CR_F(pmio, cotf_out_cr[i].cr_offset, cotf_out_cr[i].field_id))
			pcc_dump("[OUT%d] ON", pcc, i);
	}
}

static void _get_cmdq_state_str(const u32 state, char *str)
{
	u32 i;
	const ulong state_bits = state;

	if (!state) {
		strncpy(str, cmdq_state_str[0], CMDQ_STATE_STR_LENGTH);
		str += CMDQ_STATE_STR_LENGTH;
	}

	for_each_set_bit(i, (const ulong *)&state_bits, CMDQ_STATE_NUM - 1) {
		strncpy(str, cmdq_state_str[i + 1], CMDQ_STATE_STR_LENGTH);
		str += CMDQ_STATE_STR_LENGTH;
	}

	*str = '\0';
}

static void _dump_cmdq(struct pablo_common_ctrl *pcc)
{
	struct pablo_mmio *pmio = pcc->pmio;
	struct cmdq_dbg_info dbg;
	struct pablo_common_ctrl_cmd *last_cmd;
	char *str;
	u32 val;

	_cmdq_lock(pcc);

	dbg.state = GET_CR_F(pmio, CMN_CTRL_R_CMDQ_DEBUG_STATUS, CMN_CTRL_F_CMDQ_DEBUG_PROCESS);
	dbg.cmd_cnt = GET_CR_F(pmio, CMN_CTRL_R_CMDQ_FRAME_COUNTER, CMN_CTRL_F_CMDQ_FRAME_COUNTER);

	_get_cmdq_queue_status(pcc, &dbg.queue);
	_get_cmdq_frame_id(pcc, &dbg.pre, &dbg.cur);

	val = GET_CR(pmio, CMN_CTRL_R_CMDQ_DEBUG_STATUS_PRE_LOAD);
	dbg.next.cmd_id = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_CHARGED_CMD_ID, val);
	dbg.next.frame_id = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_CHARGED_FRAME_ID, val);
	dbg.charged = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_CHARGED_FOR_NEXT_FRAME, val);

	last_cmd = &pcc->dbg.last_cmd;

	_cmdq_unlock(pcc);

	str = __getname();
	if (!str)
		return;

	_get_cmdq_state_str(dbg.state, str);

	pcc_dump("CMDQ **********", pcc);
	pcc_dump("STATE: %s", pcc, str);
	pcc_dump("CMD_CNT: %d", pcc, dbg.cmd_cnt);
	pcc_dump("QUEUE: fullness %d last_rptr %d rptr %d wptr %d", pcc, dbg.queue.fullness,
		pcc->dbg.last_rptr, dbg.queue.rptr, dbg.queue.wptr);
	pcc_dump("FRAME_ID: PRE[%.3s][F%d] -> CUR[%.3s][F%d] -> NEXT[%.3s][F%d]", pcc,
		cmd_id_str[dbg.pre.cmd_id], dbg.pre.frame_id, cmd_id_str[dbg.cur.cmd_id],
		dbg.cur.frame_id, dbg.charged ? cmd_id_str[dbg.next.cmd_id] : "",
		dbg.charged ? dbg.next.frame_id : -1);
	pcc_dump("LAST_CMD_H: dva 0x%llx", pcc, last_cmd->base_addr);
	pcc_dump("LAST_CMD_M: header_num %d set_mode %d cmd_id %d fcount %d", pcc,
		last_cmd->header_num, last_cmd->set_mode, last_cmd->cmd_id, last_cmd->fcount);
	pcc_dump("LAST_CMD_L: int_grp_en 0x%x fro_id %d", pcc, last_cmd->int_grp_en,
		last_cmd->fro_id);
	pcc_dump("POST_FRAME_GAP: %u", pcc,
		GET_CR_F(pmio, CMN_CTRL_R_IP_POST_FRAME_GAP, CMN_CTRL_F_IP_POST_FRAME_GAP));

	__putname(str);
}

static void _dump_int_hist(struct pablo_common_ctrl *pcc)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 i, cr_offset, val;

	pcc_dump("INT_HIST **********", pcc);
	pcc_dump("[CUR] 0x%08x 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_INT_HIST_CURINT0),
		 GET_CR(pmio, CMN_CTRL_R_INT_HIST_CURINT1));

	for (i = 0, cr_offset = 0; i < INT_HIST_NUM; i++, cr_offset += (PMIO_REG_STRIDE * 3)) {
		val = GET_CR(pmio, CMN_CTRL_R_INT_HIST_00_FRAME_ID + cr_offset);

		pcc_dump("[%.3s][F%d] 0x%08x 0x%08x", pcc,
			cmd_id_str[GET_CR_V(pmio, CMN_CTRL_F_INT_HIST_00_CMD_ID, val)],
			GET_CR_V(pmio, CMN_CTRL_F_INT_HIST_00_FRAME_ID, val),
			GET_CR(pmio, CMN_CTRL_R_INT_HIST_00_INT0 + cr_offset),
			GET_CR(pmio, CMN_CTRL_R_INT_HIST_00_INT1 + cr_offset));
	}
}

static void _dump_dbg_status(struct pablo_common_ctrl *pcc)
{
	struct pablo_mmio *pmio = pcc->pmio;

	pcc_dump("DBG_STATUS **********", pcc);
	pcc_dump("STATUS: qch 0x%x chain_idle %d idle %d", pcc,
		 GET_CR_F(pmio, CMN_CTRL_R_QCH_STATUS, CMN_CTRL_F_QCH_STATUS),
		 GET_CR_F(pmio, CMN_CTRL_R_IDLENESS_STATUS, CMN_CTRL_F_CHAIN_IDLENESS_STATUS),
		 GET_CR_F(pmio, CMN_CTRL_R_IDLENESS_STATUS, CMN_CTRL_F_IDLENESS_STATUS));
	pcc_dump("BUSY_MON[0] 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_IP_BUSY_MONITOR_0));
	pcc_dump("BUSY_MON[1] 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_IP_BUSY_MONITOR_1));
	pcc_dump("BUSY_MON[2] 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_IP_BUSY_MONITOR_2));
	pcc_dump("BUSY_MON[3] 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_IP_BUSY_MONITOR_3));
	pcc_dump("STAL_OUT[0] 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_IP_STALL_OUT_STATUS_0));
	pcc_dump("STAL_OUT[1] 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_IP_STALL_OUT_STATUS_1));
	pcc_dump("STAL_OUT[2] 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_IP_STALL_OUT_STATUS_2));
	pcc_dump("STAL_OUT[3] 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_IP_STALL_OUT_STATUS_3));
}

/* PCC Common APIs */
static int pcc_enable(struct pablo_common_ctrl *pcc, const struct pablo_common_ctrl_cfg *cfg)
{
	struct pablo_mmio *pmio = pcc->pmio;

	/* Enable QCH to get IP clock. */
	SET_CR(pmio, CMN_CTRL_R_IP_PROCESSING, 1);

	/* Assert FS event when there is actual VValid signal input. */
	SET_CR(pmio, CMN_CTRL_R_IP_USE_CINFIFO_NEW_FRAME_IN, cfg->fs_mode);

	/**
	 * When HW is in clock-gating state,
	 * send input stall signal for the VHD valid signal from input.
	 */
	SET_CR_F(pmio, CMN_CTRL_R_CMDQ_VHD_CONTROL, CMN_CTRL_F_CMDQ_VHD_STALL_ON_QSTOP_ENABLE, 1);

	/* Enable C_Loader. The actual C_Loader operation follows the CMDQ setting mode. */
	SET_CR(pmio, CMN_CTRL_R_C_LOADER_ENABLE, 1);

	/* Enable interrupts */
	SET_CR(pmio, CMN_CTRL_R_INT_REQ_INT0_ENABLE, cfg->int_en[PCC_INT_0]);
	SET_CR(pmio, CMN_CTRL_R_INT_REQ_INT1_ENABLE, cfg->int_en[PCC_INT_1]);
	SET_CR(pmio, CMN_CTRL_R_CMDQ_INT_ENABLE, cfg->int_en[PCC_CMDQ_INT]);
	SET_CR(pmio, CMN_CTRL_R_COREX_INT_ENABLE, cfg->int_en[PCC_COREX_INT]);
	SET_CR(pmio, CMN_CTRL_R_INT_HIST_CURINT0_ENABLE, 0xffffffff);
	SET_CR(pmio, CMN_CTRL_R_INT_HIST_CURINT1_ENABLE, 0xffffffff);

	/* Enable CMDQ */
	SET_CR(pmio, CMN_CTRL_R_CMDQ_ENABLE, 1);

	pcc_info("fs_mode(%d) int0(0x%08x) int1(0x%08x) cmdq_int(0x%08x) corex_int(0x%08x)", pcc,
		 cfg->fs_mode, cfg->int_en[PCC_INT_0], cfg->int_en[PCC_INT_1],
		 cfg->int_en[PCC_CMDQ_INT], cfg->int_en[PCC_COREX_INT]);

	return 0;
}

static int pcc_reset(struct pablo_common_ctrl *pcc)
{
	int ret;

	ret = _sw_reset(pcc);
	if (ret)
		pcc_err("sw_reset is failed", pcc);

	return ret;
}

static int pcc_shot(struct pablo_common_ctrl *pcc, struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	frame_cfg->cmd.cmd_id = CMD_NORMAL;

	_s_cotf(pcc, frame_cfg);
	_s_post_frame_gap(pcc, frame_cfg->delay);

	return _s_cmd(pcc, frame_cfg);
}

static u32 pcc_get_int_status(struct pablo_common_ctrl *pcc, u32 int_id, bool clear)
{
	u32 int_cr, clr_cr, i;
	ulong status;
	struct irq_handler *handler;

	if (int_id >= PCC_INT_ID_NUM) {
		pcc_err("Invalid INT_ID(%d)", pcc, int_id);
		return 0;
	}

	int_cr = int_cr_maps[int_id].src.cr_offset;
	clr_cr = int_cr_maps[int_id].clr.cr_offset;

	status = _get_int_status(pcc, int_cr, clr_cr, clear);

	for_each_set_bit(i, &status, PCC_IRQ_HANDLER_NUM) {
		handler = &irq_handlers[int_id][i][pcc->mode];

		if (handler->func)
			handler->func(pcc);
	}

	return status;
}

static int pcc_cmp_fcount(struct pablo_common_ctrl *pcc, const u32 fcount)
{
	struct pmio_field *field;
	u32 bitmask;

	field = pmio_get_field(pcc->pmio, CMN_CTRL_F_CMDQ_CURRENT_FRAME_ID);
	bitmask = field->mask >> field->shift;

	return (fcount & bitmask) - pcc->dbg.last_fcount;
}

static void pcc_dump_dbg(struct pablo_common_ctrl *pcc)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 version;

	version = GET_CR(pmio, CMN_CTRL_R_COMMON_CTRL_VERSION);

	pcc_dump("v%02x.%02x.%02x ====================", pcc,
		 GET_CR_V(pmio, CMN_CTRL_F_CTRL_MAJOR, version),
		 GET_CR_V(pmio, CMN_CTRL_F_CTRL_MINOR, version),
		 GET_CR_V(pmio, CMN_CTRL_F_CTRL_MICRO, version));

	_dump_cotf(pcc);
	_dump_cmdq(pcc);
	_dump_int_hist(pcc);
	_dump_dbg_status(pcc);

	pcc_dump("==============================", pcc);
}

static const struct pablo_common_ctrl_ops pcc_ops[PCC_MODE_NUM] = {
	/* PCC_OTF */
	{
		.enable = pcc_enable,
		.reset = pcc_reset,
		.shot = pcc_shot,
		.get_int_status = pcc_get_int_status,
		.cmp_fcount = pcc_cmp_fcount,
		.dump = pcc_dump_dbg,
	},
	/* PCC_M2M */
	{
		.enable = pcc_enable,
		.reset = pcc_reset,
		.shot = pcc_shot,
		.get_int_status = pcc_get_int_status,
		.cmp_fcount = pcc_cmp_fcount,
		.dump = pcc_dump_dbg,
	},
};

int pablo_common_ctrl_init(struct pablo_common_ctrl *pcc, struct pablo_mmio *pmio, const char *name,
	enum pablo_common_ctrl_mode mode, struct is_mem *mem)
{
	if (!pcc) {
		pr_err("[@][PCC][ERR]%s:%d:pcc is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!pmio) {
		pcc_err("pmio is NULL", pcc);
		return -EINVAL;
	}

	if (!name) {
		pcc_err("name is NULL", pcc);
		return -EINVAL;
	}

	if (mode >= PCC_MODE_NUM) {
		pcc_err("mode(%d) is out-of-range", pcc, mode);
		return -EINVAL;
	}

	strncpy(pcc->name, name, (sizeof(pcc->name) - 1));
	pcc->mode = mode;
	pcc->ops = &pcc_ops[mode];

	spin_lock_init(&pcc->cmdq_lock);

	if (_init_pmio(pcc, pmio))
		return -EINVAL;

	switch (mode) {
	case PCC_OTF:
		if (_init_cloader(pcc, mem)) {
			pablo_common_ctrl_deinit(pcc);
			return -EINVAL;
		}
		break;
	default:
		/* Nothing to do */
		break;
	};

	pcc_info("mode(%d)", pcc, mode);

	return 0;
}
EXPORT_SYMBOL_GPL(pablo_common_ctrl_init);

void pablo_common_ctrl_deinit(struct pablo_common_ctrl *pcc)
{
	struct pablo_internal_subdev *pis;

	if (!pcc)
		return;

	pcc_info("E", pcc);

	pis = &pcc->subdev_cloader;
	if (test_bit(PABLO_SUBDEV_ALLOC, &pis->state))
		CALL_I_SUBDEV_OPS(pis, free, pis);

	if (pcc->fields) {
		pmio_field_bulk_free(pcc->pmio, pcc->fields);
		pcc->fields = NULL;
	}

	if (pcc->pmio) {
		pmio_exit(pcc->pmio);
		pcc->pmio = NULL;
	}
}
EXPORT_SYMBOL_GPL(pablo_common_ctrl_deinit);
