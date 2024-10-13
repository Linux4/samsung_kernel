// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-hw.h"
#include "pablo-hw-api-dlfe.h"
#include "pablo-hw-dlfe.h"

#define GET_HW(hw_ip)				((struct pablo_hw_dlfe *)hw_ip->priv_info)
#define CALL_HW_DLFE_OPS(hw, op, args...)	((hw && hw->ops) ? hw->ops->op(args) : 0)

static struct pablo_mmio *_pablo_hw_dlfe_pmio_init(struct is_hw_ip *hw_ip)
{
	int ret;
	struct pablo_mmio *pmio;
	struct pmio_config *pcfg;

	pcfg = &hw_ip->pmio_config;

	pcfg->name = "DLFE";
	pcfg->mmio_base = hw_ip->mmio_base;
	pcfg->cache_type = PMIO_CACHE_NONE;

	dlfe_hw_g_pmio_cfg(pcfg);

	pmio = pmio_init(NULL, NULL, pcfg);
	if (IS_ERR(pmio))
		goto err_init;

	ret = pmio_field_bulk_alloc(pmio, &hw_ip->pmio_fields,
				pcfg->fields, pcfg->num_fields);
	if (ret) {
		serr_hw("Failed to alloc DLFE PMIO field_bulk. ret %d", hw_ip, ret);
		goto err_field_bulk_alloc;
	}

	return pmio;

err_field_bulk_alloc:
	pmio_exit(pmio);
	pmio = ERR_PTR(ret);
err_init:
	return pmio;
}

static int pablo_hw_dlfe_open(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_dlfe *hw;

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_ip->priv_info = vzalloc(sizeof(struct pablo_hw_dlfe));
	if (!hw_ip->priv_info) {
		mserr_hw("Failed to alloc pablo_hw_dlfe", instance, hw_ip);
		return -ENOMEM;
	}

	hw = GET_HW(hw_ip);
	hw->ops = dlfe_hw_g_ops();

	set_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "open\n", instance, hw_ip);

	return 0;
}

static int pablo_hw_dlfe_init(struct is_hw_ip *hw_ip, u32 instance, bool flag, u32 f_type)
{
	struct pablo_hw_dlfe *hw;

	hw = GET_HW(hw_ip);
	hw->config[instance].bypass = 1;

	set_bit(HW_INIT, &hw_ip->state);

	msdbg_hw(2, "init\n", instance, hw_ip);

	return 0;
}

static int pablo_hw_dlfe_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_dlfe *hw;
	int ret;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw = GET_HW(hw_ip);
	ret = CALL_HW_DLFE_OPS(hw, wait_idle, hw_ip->pmio);
	if (ret)
		mserr_hw("failed to wait_idle. ret %d", instance, hw_ip, ret);

	if (hw_ip->priv_info) {
		vfree(hw_ip->priv_info);
		hw_ip->priv_info = NULL;
	}

	clear_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "close\n", instance, hw_ip);

	return 0;
}

static int pablo_hw_dlfe_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct pablo_hw_dlfe *hw;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	} else if (test_bit(HW_RUN, &hw_ip->state)) {
		return 0;
	}

	msdbg_hw(2, "enable: start\n", instance, hw_ip);

	hw = GET_HW(hw_ip);
	CALL_HW_DLFE_OPS(hw, init, hw_ip->pmio);

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return 0;
}

static int pablo_hw_dlfe_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct pablo_hw_dlfe *hw;
	int ret = 0;
	long timetowait;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("disable: %s\n", instance, hw_ip,
			atomic_read(&hw_ip->status.Vvalid) == V_VALID ? "V_VALID" : "V_BLANK");

	hw = GET_HW(hw_ip);
	timetowait = wait_event_timeout(hw_ip->status.wait_queue,
			!atomic_read(&hw_ip->status.Vvalid),
			IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		mserr_hw("wait FRAME_END timeout. timetowait %uus", instance, hw_ip,
				jiffies_to_usecs(IS_HW_STOP_TIMEOUT));
		CALL_HW_DLFE_OPS(hw, dump, hw_ip->pmio, DLFE_HW_DUMP_DBG_STATE);
		ret = -ETIME;
	}

	if (hw_ip->run_rsc_state)
		return ret;

	CALL_HW_DLFE_OPS(hw, reset, hw_ip->pmio);

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static bool _pablo_hw_dlfe_update_param(struct is_hw_ip *hw_ip, struct is_frame *frame,
		struct pablo_dlfe_config *cfg, struct dlfe_param_set *p_set)
{
	struct is_param_region *p_region;
	struct mcfp_param *param;
	u32 width = 0, height = 0, dbg_lv = 2;

	p_region = frame->parameter;
	param = &p_region->mcfp;

	p_set->instance = frame->instance;
	p_set->fcount = frame->fcount;

	if (test_bit(PARAM_MCFP_OTF_INPUT, frame->pmap)) {
		width = param->otf_input.width;
		height = param->otf_input.height;
	}

	if (!cfg->bypass && (!width || !height)) {
		mserr_hw("[F%d] Invalid OTF input size. Force to disable.", p_set->instance, hw_ip, p_set->fcount);
		cfg->bypass = 1;
	}

	if (p_set->wgt_out.cmd && cfg->bypass) {
		p_set->curr_in.cmd = OTF_INPUT_COMMAND_DISABLE;
		p_set->prev_in.cmd = OTF_INPUT_COMMAND_DISABLE;
		p_set->wgt_out.cmd = OTF_OUTPUT_COMMAND_DISABLE;
		dbg_lv = 0;
	} else if (!p_set->wgt_out.cmd && !cfg->bypass) {
		p_set->wgt_out.width = width;
		p_set->wgt_out.height = height;
		p_set->wgt_out.cmd = OTF_OUTPUT_COMMAND_ENABLE;

		p_set->prev_in.width = width;
		p_set->prev_in.height = height;
		p_set->prev_in.cmd = OTF_INPUT_COMMAND_ENABLE;

		p_set->curr_in.width = width;
		p_set->curr_in.height = height;
		p_set->curr_in.cmd = OTF_INPUT_COMMAND_ENABLE;
		dbg_lv = 0;
	}

	msdbg_hw(dbg_lv, "[F%d] %s\n", p_set->instance, hw_ip, p_set->fcount, cfg->bypass ? "BYPASS" : "RUN");

	return !cfg->bypass;
}

static int pablo_hw_dlfe_shot(struct is_hw_ip *hw_ip, struct is_frame *frame, ulong hw_map)
{
	struct pablo_hw_dlfe *hw;
	struct pablo_mmio *pmio;
	struct pablo_dlfe_config *cfg;
	struct dlfe_param_set *p_set;
	u32 instance, fcount, cur_idx;
	bool en;

	instance = frame->instance;
	fcount = frame->fcount;
	cur_idx = frame->cur_buf_index;

	msdbgs_hw(2, "[F%d]shot(%d)\n", instance, hw_ip, fcount, cur_idx);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw = GET_HW(hw_ip);
	pmio = hw_ip->pmio;
	cfg = &hw->config[instance];
	p_set = &hw->param_set[instance];

	if (CALL_HW_HELPER_OPS(hw_ip, set_rta_regs, instance, COREX_DIRECT, false, frame, NULL))
		cfg->bypass = true;

	en = _pablo_hw_dlfe_update_param(hw_ip, frame, cfg, p_set);
	if (!en)
		clear_bit(hw_ip->id, &frame->core_flag);

	CALL_HW_DLFE_OPS(hw, s_core, pmio, p_set);
	CALL_HW_DLFE_OPS(hw, s_path, pmio, p_set);

	CALL_HW_DLFE_OPS(hw, q_cmdq, pmio);

	return 0;
}

static int pablo_hw_dlfe_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
		enum ShotErrorType done_type)
{
	if (test_bit(hw_ip->id, &frame->core_flag))
		return CALL_HW_OPS(hw_ip, frame_done, hw_ip, frame, -1,
				IS_HW_CORE_END, done_type, false);

	return 0;
}

static int pablo_hw_dlfe_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_dlfe *hw;
	u32 v_id;

	hw = GET_HW(hw_ip);
	v_id = atomic_read(&hw_ip->status.Vvalid);
	if (v_id == V_VALID)
		CALL_HW_DLFE_OPS(hw, dump, hw_ip->pmio, DLFE_HW_DUMP_DBG_STATE);

	return 0;
}

static int pablo_hw_dlfe_reset(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_dlfe *hw = GET_HW(hw_ip);

	msinfo_hw("reset\n", instance, hw_ip);

	return CALL_HW_DLFE_OPS(hw, reset, hw_ip->pmio);
}

static int pablo_hw_dlfe_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;

	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("restore: Not opened", instance, hw_ip);
		return -EINVAL;
	}

	ret = CALL_HWIP_OPS(hw_ip, reset, instance);
	if (ret)
		return ret;

	return 0;
}

static int pablo_hw_dlfe_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount,
		struct cr_set *regs, u32 regs_size, enum is_reg_dump_type dump_type)
{
	struct pablo_hw_dlfe *hw = GET_HW(hw_ip);

	switch (dump_type) {
	case IS_REG_DUMP_TO_LOG:
		CALL_HW_DLFE_OPS(hw, dump, hw_ip->pmio, DLFE_HW_DUMP_CR);
		break;
	default:
		/* Do nothing */
		break;
	}

	return 0;
}

static int pablo_hw_dlfe_set_config(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount,
		void *conf)
{
	struct pablo_hw_dlfe *hw = GET_HW(hw_ip);
	struct pablo_dlfe_config *org_cfg = &hw->config[instance];
	struct pablo_dlfe_config *new_cfg = (struct pablo_dlfe_config *)conf;

	FIMC_BUG(!new_cfg);

	if (org_cfg->bypass != new_cfg->bypass)
		msinfo_hw("[F%d]set_config:%s\n", instance, hw_ip, fcount,
				new_cfg->bypass ? "BYPASS" : "RUN");

	memcpy(org_cfg, new_cfg, sizeof(struct pablo_dlfe_config));

	return 0;
}

const struct is_hw_ip_ops pablo_hw_dlfe_ops = {
	.open			= pablo_hw_dlfe_open,
	.init			= pablo_hw_dlfe_init,
	.close			= pablo_hw_dlfe_close,
	.enable			= pablo_hw_dlfe_enable,
	.disable		= pablo_hw_dlfe_disable,
	.shot			= pablo_hw_dlfe_shot,
	.frame_ndone		= pablo_hw_dlfe_frame_ndone,
	.notify_timeout		= pablo_hw_dlfe_notify_timeout,
	.reset			= pablo_hw_dlfe_reset,
	.restore		= pablo_hw_dlfe_restore,
	.dump_regs		= pablo_hw_dlfe_dump_regs,
	.set_config		= pablo_hw_dlfe_set_config,
};

static void fs_fe_handler(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, u32 status)
{
	mswarn_hw("[F%d][INT0]FS/FE overlapped!! status 0x%08x", instance, hw_ip, fcount, status);
}

static void fs_handler(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, u32 status)
{
	struct is_hardware *hardware = hw_ip->hardware;
	u32 dbg_hw_lv = atomic_read(&hardware->streaming[hardware->sensor_position[instance]]) ? 2 : 0;

	msdbg_hw(dbg_hw_lv, "[F%d]FS\n", instance, hw_ip, fcount);

	atomic_set(&hw_ip->status.Vvalid, V_VALID);
	CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, fcount, DEBUG_POINT_FRAME_START);
	CALL_HW_OPS(hw_ip, frame_start, hw_ip, instance);
}

static void fe_handler(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, u32 status)
{
	struct is_hardware *hardware = hw_ip->hardware;
	u32 dbg_hw_lv = atomic_read(&hardware->streaming[hardware->sensor_position[instance]]) ? 2 : 0;

	msdbg_hw(dbg_hw_lv, "[F%d]FE\n", instance, hw_ip, fcount);

	CALL_HW_OPS(hw_ip, frame_done, hw_ip, NULL, -1, IS_HW_CORE_END, IS_SHOT_SUCCESS, true);

	if (atomic_read(&hw_ip->count.fs) < atomic_read(&hw_ip->count.fe)) {
		mserr_hw("fs(%d), fe(%d), dma(%d), status(0x%x)",
				instance, hw_ip,
				atomic_read(&hw_ip->count.fs),
				atomic_read(&hw_ip->count.fe),
				atomic_read(&hw_ip->count.dma),
				status);
	}

	wake_up(&hw_ip->status.wait_queue);
}

static void int0_err_handler(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, u32 status)
{
	struct pablo_hw_dlfe *hw;

	mserr_hw("[F%d][INT0] HW Error!! status 0x%08x", instance, hw_ip, fcount, status);

	hw = GET_HW(hw_ip);
	CALL_HW_DLFE_OPS(hw, dump, hw_ip->pmio, DLFE_HW_DUMP_DBG_STATE);
	CALL_HW_DLFE_OPS(hw, dump, hw_ip->pmio, DLFE_HW_DUMP_CR);
}

static void int1_err_handler(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, u32 status)
{
	struct pablo_hw_dlfe *hw;

	mserr_hw("[F%d][INT1] HW Error!! status 0x%08x", instance, hw_ip, fcount, status);

	hw = GET_HW(hw_ip);
	CALL_HW_DLFE_OPS(hw, dump, hw_ip->pmio, DLFE_HW_DUMP_DBG_STATE);
	CALL_HW_DLFE_OPS(hw, dump, hw_ip->pmio, DLFE_HW_DUMP_CR);
}

typedef void (*int_handler)(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, u32 status);
struct dlfe_irq_handler {
	bool		valid;
	ulong		bitmask;
	int_handler	func;
};

/**
 * Array of IRQ handler for each interrupt bitmask.
 * It will call each IRQ handler as the order of declaration.
 */

#define NUM_DLFE_IRQ_HANDLER	32
static struct dlfe_irq_handler irq_handler[][NUM_DLFE_IRQ_HANDLER] = {
	/* INT0 */
	{
		{
			.valid = true,
			.bitmask = (BIT_MASK(INT_FRAME_START) | BIT_MASK(INT_FRAME_END)),
			.func = fs_fe_handler,
		},
		{
			.valid = true,
			.bitmask = BIT_MASK(INT_FRAME_START),
			.func = fs_handler,
		},
		{
			.valid = true,
			.bitmask = BIT_MASK(INT_FRAME_END),
			.func = fe_handler,
		},
		{
			.valid = true,
			.bitmask = BIT_MASK(INT_ERR0),
			.func = int0_err_handler,
		},
	},
	/* INT1 */
	{
		{
			.valid = true,
			.bitmask = BIT_MASK(INT_ERR1),
			.func = int1_err_handler,
		}
	},
};

static int pablo_hw_dlfe_handler_int(u32 id, void *ctx)
{
	struct is_hw_ip *hw_ip;
	struct pablo_hw_dlfe *hw;
	struct dlfe_irq_handler* handler;
	u32 instance, hw_fcount, status, i;
	bool occur;

	hw_ip = (struct is_hw_ip *)ctx;
	instance = atomic_read(&hw_ip->instance);
	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("Invalid INT%d. hw_state 0x%lx", instance, hw_ip, id, hw_ip->state);
		return 0;
	}

	hw = GET_HW(hw_ip);
	hw_fcount = atomic_read(&hw_ip->fcount);
	handler = irq_handler[id];
	status = CALL_HW_DLFE_OPS(hw, g_int_state, hw_ip->pmio, id);

	if (!test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("Invalid INT%d. int_status 0x%08x hw_state 0x%lx", instance, hw_ip, id,
				status, hw_ip->state);
		return 0;
	}

	msdbg_hw(2, "[F%d][INT%d] status 0x%08x\n", instance, hw_ip, hw_fcount, id, status);

	for (i = 0; i < NUM_DLFE_IRQ_HANDLER; i++) {
		occur = CALL_HW_DLFE_OPS(hw, is_occurred, status, handler[i].bitmask);
		if (handler[i].valid && occur)
			handler[i].func(hw_ip, instance, hw_fcount, status);
	}

	return 0;
}

int pablo_hw_dlfe_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
		struct is_interface_ischain *itfc, int id, const char *name)
{
	struct pablo_mmio *pmio;
	int hw_slot;

	hw_ip->ops = &pablo_hw_dlfe_ops;

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("Invalid hw_slot %d", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &pablo_hw_dlfe_handler_int;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].handler = &pablo_hw_dlfe_handler_int;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].id = INTR_HWIP2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = true;

	hw_ip->mmio_base = hw_ip->regs[REG_SETA];

	pmio = _pablo_hw_dlfe_pmio_init(hw_ip);
	if (IS_ERR(pmio)) {
		serr_hw("Failed to DLFE pmio_init.", hw_ip);
		return -EINVAL;
	}

	hw_ip->pmio = pmio;

	return 0;
}
EXPORT_SYMBOL_GPL(pablo_hw_dlfe_probe);
