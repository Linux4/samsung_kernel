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

extern struct is_lib_support gPtr_lib_support;

IS_TIMER_FUNC(is_hw_frame_start_timer)
{
	struct is_hw_ip *hw_ip = from_timer(hw_ip, (struct timer_list *)data, lme_frame_start_timer);
	struct is_hardware *hardware;
	u32 instance, hw_fcount;

	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	hardware = hw_ip->hardware;

	atomic_add(1, &hw_ip->count.fs);
	if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
		msinfo_hw("[F:%d]F.S\n", instance, hw_ip, hw_fcount);

	atomic_add(hw_ip->num_buffers, &hw_ip->count.fs);
	is_hardware_frame_start(hw_ip, instance);

	mod_timer(&hw_ip->lme_frame_end_timer, jiffies + msecs_to_jiffies(3));
}

IS_TIMER_FUNC(is_hw_frame_end_timer)
{
	struct is_hw_ip *hw_ip = from_timer(hw_ip, (struct timer_list *)data, lme_frame_end_timer);
	struct is_hardware *hardware;
	u32 instance, hw_fcount;

	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	hardware = hw_ip->hardware;

	atomic_add(hw_ip->num_buffers, &hw_ip->count.fe);
	is_hardware_frame_done(hw_ip, NULL, -1, 0,
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

static int __nocfi is_hw_lme_open(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group)
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

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
/* TODO: enable DDK */
#if 0
#ifdef ENABLE_FPSIMD_FOR_USER
	fpsimd_get();
	ret = get_lib_func(LIB_FUNC_LME, (void **)&hw_lme->lib_func);
	fpsimd_put();
#else
	ret = get_lib_func(LIB_FUNC_LME, (void **)&hw_lme->lib_func);
#endif

	if (hw_lme->lib_func == NULL) {
		mserr_hw("hw_lme->lib_func(null)", instance, hw_ip);
		is_load_clear();
		ret = -EINVAL;
		goto err_lib_func;
	}
	msinfo_hw("get_lib_func is set\n", instance, hw_ip);

	hw_lme->lib_support = &gPtr_lib_support;
	hw_lme->lib[instance].func = hw_lme->lib_func;

	ret = is_lib_isp_chain_create(hw_ip, &hw_lme->lib[instance], instance);
	if (ret) {
		mserr_hw("chain create fail", instance, hw_ip);
		ret = -EINVAL;
		goto err_chain_create;
	}
#endif
	set_bit(HW_OPEN, &hw_ip->state);
	msdbg_hw(2, "open: [G:0x%x], framemgr[%s]", instance, hw_ip,
		GROUP_ID(group->id), hw_ip->framemgr->name);

	return 0;
#if 0
err_chain_create:
err_lib_func:
	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
#endif
err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_lme_init(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group, bool flag, u32 module_id)
{
	int ret = 0;
	struct is_hw_lme *hw_lme = NULL;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!group);

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

#if 0
>>>>>>> 41420869f... [COMMON] Pablo: hardware: fix: lme frame_done error
	hw_lme->lib[instance].object = NULL;
	hw_lme->lib[instance].func   = hw_lme->lib_func;

	if (hw_lme->lib[instance].object != NULL) {
		msdbg_hw(2, "object is already created\n", instance, hw_ip);
	} else {
		ret = is_lib_isp_object_create(hw_ip, &hw_lme->lib[instance],
				instance, (u32)flag, module_id);
		if (ret) {
			mserr_hw("object create fail", instance, hw_ip);
			return -EINVAL;
		}
	}
#endif

	timer_setup(&hw_ip->lme_frame_start_timer, (void (*)(struct timer_list *))is_hw_frame_start_timer, 0);
	timer_setup(&hw_ip->lme_frame_end_timer, (void (*)(struct timer_list *))is_hw_frame_end_timer, 0);

	set_bit(HW_INIT, &hw_ip->state);
	return ret;
}

static int is_hw_lme_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_lme *hw_lme;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
#if 0
	is_lib_isp_object_destroy(hw_ip, &hw_lme->lib[instance], instance);
	hw_lme->lib[instance].object = NULL;
#endif
	return ret;
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
#if 0
	FIMC_BUG(!hw_lme->lib_support);

	is_lib_isp_chain_destroy(hw_ip, &hw_lme->lib[instance], instance);
#endif
	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
	frame_manager_close(hw_ip->framemgr);

	del_timer(&hw_ip->lme_frame_start_timer);
	del_timer(&hw_ip->lme_frame_end_timer);

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

	atomic_inc(&hw_ip->run_rsccount);
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
	if (test_bit(HW_RUN, &hw_ip->state)) {
		/* TODO: need to kthread_flush when lme use task */
		/* is_lib_isp_stop(hw_ip, &hw_lme->lib[instance], instance); */
	} else {
		msdbg_hw(2, "already disabled\n", instance, hw_ip);
	}

	if (atomic_dec_return(&hw_ip->run_rsccount) > 0)
		return 0;

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static void is_hw_lme_update_param(struct is_hw_ip *hw_ip, struct is_region *region,
	struct lme_param_set *param_set, u32 lindex, u32 hindex, u32 instance)
{
	struct lme_param *param;

	FIMC_BUG_VOID(!region);
	FIMC_BUG_VOID(!param_set);

	param = &region->parameter.lme;
	param_set->instance_id = instance;

	/* check input */
	if ((lindex & LOWBIT_OF(PARAM_LME_DMA))
		|| (hindex & HIGHBIT_OF(PARAM_LME_DMA))) {
		memcpy(&param_set->dma, &param->dma,
			sizeof(struct param_lme_dma));
	}

}

static int is_hw_lme_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	int ret = 0;
	int i;
	struct is_hw_lme *hw_lme;
	struct lme_param_set *param_set;
	struct is_region *region;
	struct lme_param *param;
	u32 lindex, hindex, fcount, instance;
	u32 hw_plane = 0;
	bool frame_done = false;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	instance = frame->instance;
	msdbgs_hw(2, "[F:%d]shot\n", instance, hw_ip, frame->fcount);
	msinfo_hw("[F:%d]shot\n", instance, hw_ip, frame->fcount);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	is_hw_g_ctrl(hw_ip, hw_ip->id, HW_G_CTRL_FRM_DONE_WITH_DMA, (void *)&frame_done);
	if ((!frame_done)
		|| (!test_bit(ENTRY_LME, &frame->out_flag) && !test_bit(ENTRY_LMEC, &frame->out_flag)
			&& !test_bit(ENTRY_LMES, &frame->out_flag)))
		set_bit(hw_ip->id, &frame->core_flag);


	FIMC_BUG(!hw_ip->priv_info);
	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	param_set = &hw_lme->param_set[instance];
	region = hw_ip->region[instance];
	FIMC_BUG(!region);

	param = &region->parameter.lme;
	fcount = frame->fcount + frame->cur_buf_index;

	if (frame->type == SHOT_TYPE_INTERNAL) {
		param_set->dma.input_cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->input_dva[0] = 0x0;
		param_set->output_dva[0] = 0x0;
		hw_ip->internal_fcount[instance] = fcount;
		goto config;
	} else {
		FIMC_BUG(!frame->shot);
		/* per-frame control
		 * check & update size from region
		 */
		lindex = frame->shot->ctl.vendor_entry.lowIndexParam;
		hindex = frame->shot->ctl.vendor_entry.highIndexParam;

		if (hw_ip->internal_fcount[instance] != 0) {
			hw_ip->internal_fcount[instance] = 0;
			param_set->dma.output_cmd = param->dma.output_cmd;
		}
	}

	is_hw_lme_update_param(hw_ip, region, param_set, lindex, hindex, instance);

	/* DMA settings */
	if (param_set->dma.input_cmd != DMA_INPUT_COMMAND_DISABLE) {
		hw_plane = param_set->dma.input_plane;
		/* TODO : need to loop (frame->num_buffers) for FRO */
		for (i = 0; i < hw_plane; i++) {
			param_set->input_dva[i] = frame->dvaddr_buffer[frame->cur_buf_index + i];
			if (param_set->input_dva[i] == 0) {
				msinfo_hw("[F:%d]dvaddr_buffer[%d] is zero",
					instance, hw_ip, frame->fcount, i);
				FIMC_BUG(1);
			}
		}
	}

	if (param_set->dma.prev_input_cmd != DMA_INPUT_COMMAND_DISABLE) {
		hw_plane = param_set->dma.input_plane;
		/* TODO : need to loop (frame->num_buffers) for FRO */
		for (i = 0; i < hw_plane; i++) {
			param_set->prev_input_dva[i] = frame->lmesTargetAddress[frame->cur_buf_index + i];
			if (frame->lmesTargetAddress[i] == 0) {
				msdbg_hw(2, "[F:%d]lmesTargetAddress[%d] is zero",
					instance, hw_ip, frame->fcount, i);
				FIMC_BUG(1);
			}

		}
	}

	if (param_set->dma.output_cmd != DMA_OUTPUT_COMMAND_DISABLE) {
		hw_plane = param_set->dma.output_plane;
		/* TODO : need to loop (frame->num_buffers) for FRO */
		for (i = 0; i < hw_plane; i++) {
			param_set->output_dva[i] = frame->lmecTargetAddress[frame->cur_buf_index + i];
			if (frame->lmecTargetAddress[i] == 0) {
				msinfo_hw("[F:%d]lmecTargetAddress[%d] is zero",
					instance, hw_ip, frame->fcount, i);
				param_set->dma.output_cmd = DMA_OUTPUT_COMMAND_DISABLE;
			}
		}
	}

config:
	param_set->instance_id = instance;
	param_set->fcount = fcount;

	/* multi-buffer */
	if (frame->num_buffers)
		hw_ip->num_buffers = frame->num_buffers;

	if (frame->type == SHOT_TYPE_INTERNAL) {
		is_log_write("[@][DRV][%d]lme_shot [T:%d][F:%d][IN:0x%x] [%d][OUT:0x%x]\n",
			param_set->instance_id, frame->type,
			param_set->fcount, param_set->input_dva[0],
			param_set->dma.output_cmd, param_set->output_dva[0]);
	}
#if 0
	if (frame->shot) {
		ret = is_lib_isp_set_ctrl(hw_ip, &hw_lme->lib[instance], frame);
		if (ret)
			mserr_hw("set_ctrl fail", instance, hw_ip);
	}

	is_lib_isp_shot(hw_ip, &hw_lme->lib[instance], param_set, frame->shot);
#endif
	set_bit(HW_CONFIG, &hw_ip->state);

	mod_timer(&hw_ip->lme_frame_start_timer, jiffies + msecs_to_jiffies(3));

	return ret;
}

static int is_hw_lme_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
	u32 lindex, u32 hindex, u32 instance, ulong hw_map)
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
	hw_ip->lindex[instance] = lindex;
	hw_ip->hindex[instance] = hindex;

	is_hw_lme_update_param(hw_ip, region, param_set, lindex, hindex, instance);

	return ret;
}

static int is_hw_lme_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	int ret = 0;
	struct is_hw_lme *hw_lme;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	FIMC_BUG(!hw_ip->priv_info);
	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

	/* ret = is_lib_isp_get_meta(hw_ip, &hw_lme->lib[frame->instance], frame); */
	if (ret)
		mserr_hw("get_meta fail", frame->instance, hw_ip);

	return ret;
}

static int is_hw_lme_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
	u32 instance, enum ShotErrorType done_type)
{
	int ret = 0;
	int output_id = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	if (test_bit(hw_ip->id, &frame->core_flag))
		output_id = IS_HW_CORE_END;

	ret = is_hardware_frame_done(hw_ip, frame, -1,
			output_id, done_type, false);

	return ret;
}

static int is_hw_lme_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		msdbg_hw(2, "%s: hw_map(0x%lx)\n", instance, hw_ip, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -ESRCH;
	}

	set_bit(HW_TUNESET, &hw_ip->state);

	return 0;
}

static int is_hw_lme_apply_setfile(struct is_hw_ip *hw_ip, u32 scenario,
	u32 instance, ulong hw_map)
{
	int ret = 0;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		msdbg_hw(2, "%s: hw_map(0x%lx)\n", instance, hw_ip, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -ESRCH;
	}

	return ret;
}

static int is_hw_lme_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		msdbg_hw(2, "%s: hw_map(0x%lx)\n", instance, hw_ip, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		msdbg_hw(2, "Not initialized\n", instance, hw_ip);
		return 0;
	}

	clear_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

int is_hw_lme_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;

	BUG_ON(!hw_ip);
	BUG_ON(!hw_ip->priv_info);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	return ret;
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
	.restore		= is_hw_lme_restore
};

int is_hw_lme_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!itf);
	FIMC_BUG(!itfc);

	/* initialize device hardware */
	hw_ip->id   = id;
	snprintf(hw_ip->name, sizeof(hw_ip->name), "%s", name);
	hw_ip->ops  = &is_hw_lme_ops;
	hw_ip->itf  = itf;
	hw_ip->itfc = itfc;
	atomic_set(&hw_ip->fcount, 0);
	hw_ip->is_leader = true;
	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	atomic_set(&hw_ip->rsccount, 0);
	atomic_set(&hw_ip->run_rsccount, 0);
	init_waitqueue_head(&hw_ip->status.wait_queue);

	clear_bit(HW_OPEN, &hw_ip->state);
	clear_bit(HW_INIT, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);
	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_TUNESET, &hw_ip->state);

	sinfo_hw("probe done\n", hw_ip);

	return ret;
}
