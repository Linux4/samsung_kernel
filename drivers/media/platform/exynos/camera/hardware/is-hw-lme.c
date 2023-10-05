// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-hw-lme.h"
#include "is-err.h"
#include "api/is-hw-api-lme.h"

#define DDK_LIB_CALL
#ifndef DDK_LIB_CALL
IS_TIMER_FUNC(is_hw_frame_start_timer)
{
	struct is_hw_ip *hw_ip = from_timer(hw_ip, (struct timer_list *)data, lme_frame_start_timer);
	struct is_hardware *hardware;
	u32 instance, hw_fcount;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	hardware = hw_ip->hardware;

	msdbg_hw(2, "lme frame start timer", instance, hw_ip);
	atomic_add(1, &hw_ip->count.fs);
	if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
		msinfo_hw("[F:%d]F.S\n", instance, hw_ip, hw_fcount);

	CALL_HW_OPS(hw_ip, frame_start, hw_ip, instance);

	mod_timer(&hw_ip->lme_frame_end_timer, jiffies + msecs_to_jiffies(0));
}

IS_TIMER_FUNC(is_hw_frame_end_timer)
{
	struct is_hw_ip *hw_ip = from_timer(hw_ip, (struct timer_list *)data, lme_frame_end_timer);
	struct is_hardware *hardware;
	u32 instance, hw_fcount;

	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	hardware = hw_ip->hardware;

	msdbg_hw(2, "lme frame end timer", instance, hw_ip);
	atomic_add(hw_ip->num_buffers, &hw_ip->count.fe);
	CALL_HW_OPS(hw_ip, frame_done, hw_ip, NULL, -1, 0,
					IS_SHOT_SUCCESS, true);

	if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
		msinfo_hw("[F:%d]F.E\n", instance, hw_ip, hw_fcount);

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	if (atomic_read(&hw_ip->count.fs) < atomic_read(&hw_ip->count.fe)) {
		mserr_hw("fs(%d), fe(%d), dma(%d)", instance, hw_ip,
			atomic_read(&hw_ip->count.fs),
			atomic_read(&hw_ip->count.fe),
			atomic_read(&hw_ip->count.dma));
	}

	wake_up(&hw_ip->status.wait_queue);
}
#endif

static int __nocfi is_hw_lme_open(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_lme *hw_lme = NULL;

	FIMC_BUG(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HWLME");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw_ip->priv_info = vzalloc(sizeof(struct is_hw_lme));
	if (!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

#ifdef USE_ORBMCH_WA
	is_hw_lme_isr_clear_register(hw_ip->id, true);
#endif

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

	ret = CALL_HW_HELPER_OPS(hw_ip, open, instance, &hw_lme->lib[instance],
				LIB_FUNC_LME);
	if (ret)
		goto err_chain_create;

	set_bit(HW_OPEN, &hw_ip->state);
	msdbg_hw(2, "open:, framemgr[%s]", instance, hw_ip, hw_ip->framemgr->name);

	return 0;
err_chain_create:
	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_lme_init(struct is_hw_ip *hw_ip, u32 instance,
			bool flag, u32 f_type)
{
	int ret = 0;
	struct is_hw_lme *hw_lme;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	hw_lme->param_set[instance].reprocessing = flag;

	ret = CALL_HW_HELPER_OPS(hw_ip, init, instance, &hw_lme->lib[instance],
				(u32)flag, f_type, LIB_FUNC_LME);
	if (ret)
		return ret;

#ifndef DDK_LIB_CALL
	timer_setup(&hw_ip->lme_frame_start_timer, (void (*)(struct timer_list *))is_hw_frame_start_timer, 0);
	timer_setup(&hw_ip->lme_frame_end_timer, (void (*)(struct timer_list *))is_hw_frame_end_timer, 0);
#endif

	set_bit(HW_INIT, &hw_ip->state);
	return ret;
}

static int is_hw_lme_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_lme *hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, deinit, instance, &hw_lme->lib[instance]);
}

static int is_hw_lme_close(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_lme *hw_lme;

	FIMC_BUG(!hw_ip);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	FIMC_BUG(!hw_ip->priv_info);
	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

#ifdef USE_ORBMCH_WA
	is_hw_lme_isr_clear_register(hw_ip->id, false);
#endif

	CALL_HW_HELPER_OPS(hw_ip, close, instance, &hw_lme->lib[instance]);

#ifndef DDK_LIB_CALL
	del_timer(&hw_ip->lme_frame_start_timer);
	del_timer(&hw_ip->lme_frame_end_timer);
#endif

	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
	frame_manager_close(hw_ip->framemgr);

	clear_bit(HW_OPEN, &hw_ip->state);

	return ret;
}

static int is_hw_lme_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	set_bit(HW_RUN, &hw_ip->state);

	return ret;
}

static int is_hw_lme_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_lme *hw_lme;
	struct lme_param_set *param_set;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("lme_disable: Vvalid(%d)\n", instance, hw_ip,
		atomic_read(&hw_ip->status.Vvalid));

	FIMC_BUG(!hw_ip->priv_info);
	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	param_set = &hw_lme->param_set[instance];

	timetowait = wait_event_timeout(hw_ip->status.wait_queue,
		!atomic_read(&hw_ip->status.Vvalid),
		IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		mserr_hw("wait FRAME_END timeout (%ld)", instance,
			hw_ip, timetowait);
		ret = -ETIME;
	}

	param_set->fcount = 0;

	CALL_HW_HELPER_OPS(hw_ip, disable, instance, &hw_lme->lib[instance]);

	if (hw_ip->run_rsc_state)
		return 0;

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static void is_hw_lme_update_param(struct is_hw_ip *hw_ip, struct is_param_region *p_region,
	struct lme_param_set *param_set, IS_DECLARE_PMAP(pmap), u32 instance)
{
	struct lme_param *param;

	param = &p_region->lme;
	param_set->instance_id = instance;

	/* check input */
	if (test_bit(PARAM_LME_DMA, pmap))
		memcpy(&param_set->dma, &param->dma,
				sizeof(struct param_lme_dma));
}

static int is_hw_lme_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	int ret = 0;
	int cur_idx, batch_num;
	struct is_hw_lme *hw_lme;
	struct lme_param_set *param_set;
	struct is_param_region *p_region;
	struct lme_param *param;
	u32 fcount, instance;
	bool is_reprocessing;
	u32 cmd_cur_input, cmd_prev_input, cmd_output;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	instance = frame->instance;
	msdbgs_hw(2, "[F:%d]shot\n", instance, hw_ip, frame->fcount);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	FIMC_BUG(!hw_ip->priv_info);
	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	param_set = &hw_lme->param_set[instance];
	p_region = frame->parameter;

	param = &p_region->lme;
	cur_idx = frame->cur_buf_index;
	fcount = frame->fcount;
#ifdef USE_ORBMCH_FOR_ME
	is_reprocessing = false;
#else
	is_reprocessing = param_set->reprocessing;
#endif
	if (frame->type == SHOT_TYPE_INTERNAL) {
		cmd_cur_input = param_set->dma.cur_input_cmd;
		param_set->dma.cur_input_cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->cur_input_dva[0] = 0x0;

		cmd_prev_input = param_set->dma.prev_input_cmd;
		param_set->dma.prev_input_cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->prev_input_dva[0] = 0x0;

		cmd_output = param_set->dma.output_cmd;
		param_set->dma.output_cmd = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->output_dva[0] = 0x0;

		param_set->tnr_mode = TNR_PROCESSING_PREVIEW_POST_ON;
		hw_ip->internal_fcount[instance] = fcount;
		goto config;
	} else {
		FIMC_BUG(!frame->shot);

		if (hw_ip->internal_fcount[instance] != 0) {
			hw_ip->internal_fcount[instance] = 0;
			param_set->dma.output_cmd = param->dma.output_cmd;
		}

		/*set TNR operation mode */
		if (frame->shot_ext) {
			/*
			if ((frame->shot_ext->tnr_mode >= TNR_PROCESSING_CAPTURE_FIRST) &&
					!CHK_VIDEOHDR_MODE_CHANGE(param_set->tnr_mode, frame->shot_ext->tnr_mode))
				msinfo_hw("[F:%d] TNR mode is changed (%d -> %d)\n",
					instance, hw_ip, frame->fcount,
					param_set->tnr_mode, frame->shot_ext->tnr_mode);
			*/
			param_set->tnr_mode = frame->shot_ext->tnr_mode;
		} else {
			mswarn_hw("frame->shot_ext is null", instance, hw_ip);
			param_set->tnr_mode = TNR_PROCESSING_PREVIEW_POST_ON;
		}
	}

	is_hw_lme_update_param(hw_ip, p_region, param_set, frame->pmap, instance);

	/* DMA settings */
	if (is_reprocessing) {
		SWAP(param_set->dma.cur_input_cmd, param_set->dma.prev_input_cmd, u32);
		SWAP(param_set->dma.cur_input_width, param_set->dma.prev_input_width, u32);
		SWAP(param_set->dma.cur_input_height, param_set->dma.prev_input_height, u32);

		cmd_cur_input = CALL_HW_OPS(hw_ip, dma_cfg, "lems", hw_ip,
				frame, cur_idx, frame->num_buffers,
				&param_set->dma.cur_input_cmd,
				param_set->dma.input_plane,
				param_set->cur_input_dva,
				frame->lmesTargetAddress);

		cmd_prev_input = CALL_HW_OPS(hw_ip, dma_cfg, "lmes_prev", hw_ip,
				frame, cur_idx, frame->num_buffers,
				&param_set->dma.prev_input_cmd,
				param_set->dma.input_plane,
				param_set->prev_input_dva,
				frame->dvaddr_buffer);
	} else {
		cmd_cur_input = CALL_HW_OPS(hw_ip, dma_cfg, "lems", hw_ip,
				frame, cur_idx, frame->num_buffers,
				&param_set->dma.cur_input_cmd,
				param_set->dma.input_plane,
				param_set->cur_input_dva,
				frame->dvaddr_buffer);

		cmd_prev_input = CALL_HW_OPS(hw_ip, dma_cfg, "lmes_prev", hw_ip,
				frame, cur_idx, frame->num_buffers,
				&param_set->dma.prev_input_cmd,
				param_set->dma.input_plane,
				param_set->prev_input_dva,
				frame->lmesTargetAddress);
	}

#if IS_ENABLED(ENABLE_LME_PREV_KVA)
		CALL_HW_OPS(hw_ip, dma_cfg_kva64, "lmes_prev", hw_ip,
			frame, cur_idx,
			&param_set->dma.prev_input_cmd,
			param_set->dma.input_plane,
			param_set->prev_input_kva,
			frame->lmesKTargetAddress);
#endif

	cmd_output = CALL_HW_OPS(hw_ip, dma_cfg, "lmec", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma.output_cmd,
			param_set->dma.output_plane,
			param_set->output_dva,
			frame->lmecTargetAddress);

	CALL_HW_OPS(hw_ip, dma_cfg_kva64, "lmec", hw_ip,
			frame, cur_idx,
			&param_set->dma.output_cmd,
			param_set->dma.output_plane,
			param_set->output_kva_motion_pure,
			frame->lmecKTargetAddress);

	CALL_HW_OPS(hw_ip, dma_cfg_kva64, "lmem", hw_ip,
			frame, cur_idx,
			&param_set->dma.output_cmd,
			param_set->dma.output_plane,
			param_set->output_kva_motion_processed,
			frame->lmemKTargetAddress);

config:
	param_set->instance_id = instance;
	param_set->fcount = fcount;

	/* multi-buffer */
	hw_ip->num_buffers = frame->num_buffers;
	batch_num = hw_ip->framemgr->batch_num;
	/* Update only for SW_FRO scenario */
	if (batch_num > 1 && hw_ip->num_buffers == 1) {
		hw_ip->num_buffers |= batch_num << SW_FRO_NUM_SHIFT;
		hw_ip->num_buffers |= cur_idx << CURR_INDEX_SHIFT;
	}

	ret = CALL_HW_HELPER_OPS(hw_ip, lib_shot, instance, false,
				frame, &hw_lme->lib[instance], param_set);
	if (ret)
		return ret;

	param_set->dma.cur_input_cmd = cmd_cur_input;
	param_set->dma.prev_input_cmd = cmd_prev_input;
	param_set->dma.output_cmd = cmd_output;

	set_bit(HW_CONFIG, &hw_ip->state);
#ifndef DDK_LIB_CALL
	mod_timer(&hw_ip->lme_frame_start_timer, jiffies + msecs_to_jiffies(0));
#endif
	return ret;
}

static int is_hw_lme_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
		IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	int ret = 0;
	struct is_hw_lme *hw_lme;
	struct lme_param_set *param_set;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	FIMC_BUG(!hw_ip->priv_info);
	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	param_set = &hw_lme->param_set[instance];

	hw_ip->region[instance] = region;

	is_hw_lme_update_param(hw_ip, &region->parameter, param_set, pmap, instance);
#ifdef DDK_LIB_CALL
	ret = is_lib_isp_set_param(hw_ip, &hw_lme->lib[instance], param_set);
	if (ret)
		mserr_hw("set_param fail", instance, hw_ip);
#endif
	return ret;
}

static int is_hw_lme_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	struct is_hw_lme *hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, get_meta, frame->instance,
				frame, &hw_lme->lib[frame->instance]);
}

static int is_hw_lme_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
		enum ShotErrorType done_type)
{
	int ret = 0;
	int output_id = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	if (test_bit(hw_ip->id, &frame->core_flag))
		output_id = IS_HW_CORE_END;

	ret = CALL_HW_OPS(hw_ip, frame_done, hw_ip, frame, -1,
			output_id, done_type, false);

	return ret;
}

static int is_hw_lme_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	return 0;
}

static int is_hw_lme_apply_setfile(struct is_hw_ip *hw_ip, u32 scenario,
	u32 instance, ulong hw_map)
{
	return 0;
}

static int is_hw_lme_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	return 0;
}

int is_hw_lme_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_lme *hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, restore, instance, &hw_lme->lib[instance]);
}

static int is_hw_lme_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_lme *hw_lme;

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	if (!hw_lme) {
		mserr_hw("failed to get HW LME", instance, hw_ip);
		return -ENODEV;
	}

	return CALL_HW_HELPER_OPS(hw_ip, notify_timeout, instance, &hw_lme->lib[instance]);
}

const struct is_hw_ip_ops is_hw_lme_ops = {
	.open			= is_hw_lme_open,
	.init			= is_hw_lme_init,
	.deinit			= is_hw_lme_deinit,
	.close			= is_hw_lme_close,
	.enable			= is_hw_lme_enable,
	.disable		= is_hw_lme_disable,
	.shot			= is_hw_lme_shot,
	.set_param		= is_hw_lme_set_param,
	.get_meta		= is_hw_lme_get_meta,
	.frame_ndone		= is_hw_lme_frame_ndone,
	.load_setfile		= is_hw_lme_load_setfile,
	.apply_setfile		= is_hw_lme_apply_setfile,
	.delete_setfile		= is_hw_lme_delete_setfile,
	.restore		= is_hw_lme_restore,
	.notify_timeout		= is_hw_lme_notify_timeout,
};

int is_hw_lme_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int ret = 0;

	hw_ip->ops  = &is_hw_lme_ops;
	hw_ip->dump_for_each_reg = lme_hw_get_reg_struct();
	hw_ip->dump_reg_list_size = lme_hw_get_reg_cnt();

	return ret;
}
