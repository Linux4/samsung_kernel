/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-hw-3aa.h"

extern struct fimc_is_lib_support gPtr_lib_support;

const struct fimc_is_hw_ip_ops fimc_is_hw_3aa_ops = {
	.open			= fimc_is_hw_3aa_open,
	.init			= fimc_is_hw_3aa_init,
	.close			= fimc_is_hw_3aa_close,
	.enable			= fimc_is_hw_3aa_enable,
	.disable		= fimc_is_hw_3aa_disable,
	.shot			= fimc_is_hw_3aa_shot,
	.set_param		= fimc_is_hw_3aa_set_param,
	.get_meta		= fimc_is_hw_3aa_get_meta,
	.frame_ndone		= fimc_is_hw_3aa_frame_ndone,
	.load_setfile		= fimc_is_hw_3aa_load_setfile,
	.apply_setfile		= fimc_is_hw_3aa_apply_setfile,
	.delete_setfile		= fimc_is_hw_3aa_delete_setfile,
	.size_dump		= fimc_is_hw_3aa_size_dump
};

int fimc_is_hw_3aa_probe(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_interface *itf,
	struct fimc_is_interface_ischain *itfc,
	int id)
{
	int ret = 0;
	int intr_slot;

	BUG_ON(!hw_ip);
	BUG_ON(!itf);
	BUG_ON(!itfc);

	/* initialize device hardware */
	hw_ip->id   = id;
	hw_ip->ops  = &fimc_is_hw_3aa_ops;
	hw_ip->itf  = itf;
	hw_ip->itfc = itfc;
	hw_ip->fcount = 0;
	hw_ip->internal_fcount = 0;
	hw_ip->is_leader = true;
	atomic_set(&hw_ip->Vvalid, 0);
	atomic_set(&hw_ip->otf_start, 0);
	atomic_set(&hw_ip->ref_count, 0);
	init_waitqueue_head(&hw_ip->wait_queue);

	/* set 3aa sfr base address */
	switch (id) {
	case DEV_HW_3AA0:
	case DEV_HW_3AA1:
		intr_slot = fimc_is_intr_slot_id(id);
		if (!valid_3aaisp_intr_slot_id(intr_slot)) {
			err_hw("invalid intr_slot (%d,%d)", id, intr_slot);
			goto p_err;
		}
		hw_ip->base_addr   = itfc->taaisp[intr_slot].regs;
		hw_ip->base_addr_b = itfc->taaisp[intr_slot].regs_b;
		break;
	default:
		err_hw("invalid hw (%d)", id);
		goto p_err;
		break;
	}

	clear_bit(HW_OPEN, &hw_ip->state);
	clear_bit(HW_INIT, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);
	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_TUNESET, &hw_ip->state);

p_err:
	return ret;
}

int fimc_is_hw_3aa_open(struct fimc_is_hw_ip *hw_ip, u32 instance, u32 *size)
{
	int ret = 0;

	BUG_ON(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		goto p_err;

	*size = sizeof(struct fimc_is_hw_3aa);

	frame_manager_probe(hw_ip->framemgr, FRAMEMGR_ID_HW | hw_ip->id);
	frame_manager_open(hw_ip->framemgr, FIMC_IS_MAX_HW_FRAME);

p_err:
	return ret;
}

int fimc_is_hw_3aa_init(struct fimc_is_hw_ip *hw_ip, struct fimc_is_group *group,
		bool flag, u32 module_id)
{
	int ret = 0;
	int i = 0;
	u32 instance = 0;
	struct fimc_is_hw_3aa *hw_3aa = NULL;

	BUG_ON(!hw_ip);
	BUG_ON(!hw_ip->priv_info);
	BUG_ON(!group);

	hw_3aa = (struct fimc_is_hw_3aa *)hw_ip->priv_info;
	instance = group->instance;
	hw_ip->group[instance] = group;

	if (hw_3aa->lib_func == NULL) {
		ret = get_lib_func(LIB_FUNC_3AA, (void **)&hw_3aa->lib_func);
		dbg_hw("lib_interface_func is set (%d)\n", hw_ip->id);
	}

	if (hw_3aa->lib_func == NULL) {
		err_hw("func_3aa(null) (%d)", hw_ip->id);
		fimc_is_load_clear();
		ret = -EINVAL;
		goto p_err;
	}

	hw_3aa->lib_support = &gPtr_lib_support;
	for (i = 0; i < FIMC_IS_MAX_NODES; i++) {
		hw_3aa->lib[i].object = NULL;
		hw_3aa->lib[i].func   = hw_3aa->lib_func;
	}

	hw_3aa->param_set[instance].reprocessing = flag;

	if (test_bit(HW_INIT, &hw_ip->state)) {
		dbg_hw("[%d]chain is already created (%d)\n", instance, hw_ip->id);
	} else {
		ret = fimc_is_lib_isp_chain_create(hw_ip, &hw_3aa->lib[instance], instance);
		if (ret) {
			err_hw("[%d]chain create fail (%d)", instance, hw_ip->id);
			ret = -EINVAL;
			goto p_err;
		}
	}

	if (hw_3aa->lib[instance].object != NULL) {
		dbg_hw("[%d]object is already created (%d)\n", instance, hw_ip->id);
	} else {
		ret = fimc_is_lib_isp_object_create(hw_ip, &hw_3aa->lib[instance],
				instance, (u32)flag, module_id);
		if (ret) {
			err_hw("[%d]object create fail (%d)", instance, hw_ip->id);
			ret = -EINVAL;
			goto p_err;
		}
	}

p_err:
	return ret;
}

int fimc_is_hw_3aa_object_close(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct fimc_is_hw_3aa *hw_3aa;

	BUG_ON(!hw_ip);

	hw_3aa = (struct fimc_is_hw_3aa *)hw_ip->priv_info;

	fimc_is_lib_isp_object_destroy(hw_ip, &hw_3aa->lib[instance], instance);
	hw_3aa->lib[instance].object = NULL;

	return ret;
}

int fimc_is_hw_3aa_close(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	int i = 0;
	struct fimc_is_hw_3aa *hw_3aa;

	BUG_ON(!hw_ip);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		goto exit;

	hw_3aa = (struct fimc_is_hw_3aa *)hw_ip->priv_info;

	for (i = 0; i < FIMC_IS_MAX_TASK_WORKER; i++) {
		if (hw_3aa->lib_support->lib_task[i].task == NULL) {
			err_hw("task is null");
		} else {
			ret = kthread_stop(hw_3aa->lib_support->lib_task[i].task);
			if (ret)
				err_hw("kthread_stop fail (%d)", ret);
		}
	}

	fimc_is_lib_isp_chain_destroy(hw_ip, &hw_3aa->lib[instance], instance);
	frame_manager_close(hw_ip->framemgr);

	info_hw("[%d]close (%d)(%d)\n", instance, hw_ip->id, atomic_read(&hw_ip->ref_count));

exit:
	return ret;
}

int fimc_is_hw_3aa_enable(struct fimc_is_hw_ip *hw_ip, u32 instance, unsigned long hw_map)
{
	int ret = 0;
	struct fimc_is_hw_3aa *hw_3aa;
	struct fimc_is_frame *frame = NULL;
	struct fimc_is_framemgr *framemgr;
	struct camera2_shot *shot = NULL;

	BUG_ON(!hw_ip);

	if (!test_bit(hw_ip->id, &hw_map))
		goto exit;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("[%d]not initialized!! (%d)", instance, hw_ip->id);
		ret = -EINVAL;
		goto exit;
	}

	/* For sensor info initialize for mode change */
	framemgr = hw_ip->framemgr;
	framemgr_e_barrier(framemgr, 0);
	frame = peek_frame(framemgr, FS_PROCESS);
	framemgr_x_barrier(framemgr, 0);
	if (frame) {
		shot = frame->shot;
	} else {
		warn_hw("[%d] 3aa_enable (frame:NULL)(%d)",
			instance, framemgr->queued_count[FS_PROCESS]);
	}

	hw_3aa = (struct fimc_is_hw_3aa *)hw_ip->priv_info;
	if (shot) {
		ret = fimc_is_lib_isp_sensor_info_mode_chg(&hw_3aa->lib[instance], instance, shot);
		if (ret < 0) {
			err_hw("[%d] fimc_is_lib_isp_sensor_info_mode_chg fail (%d)", instance, hw_ip->id);
		}
	}

	set_bit(HW_RUN, &hw_ip->state);
exit:
	return ret;
}

int fimc_is_hw_3aa_disable(struct fimc_is_hw_ip *hw_ip, u32 instance, unsigned long hw_map)
{
	int ret = 0;
	int i = 0;
	u32 timetowait;
	struct fimc_is_hw_3aa *hw_3aa;
	struct taa_param_set *param_set;

	BUG_ON(!hw_ip);

	if (!test_bit(hw_ip->id, &hw_map))
		goto exit;

	hw_3aa = (struct fimc_is_hw_3aa *)hw_ip->priv_info;
	param_set = &hw_3aa->param_set[instance];

	timetowait = wait_event_timeout(hw_ip->wait_queue,
		!atomic_read(&hw_ip->Vvalid),
		FIMC_IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		err_hw("[%d][ID:%d] wait FRAME_END timeout (%u)", instance, hw_ip->id, timetowait);
		ret = -ETIME;
	}

	param_set->fcount = 0;
	if (test_bit(HW_RUN, &hw_ip->state)) {
		for (i = 0; i < FIMC_IS_MAX_TASK_WORKER; i++) {
			if (hw_3aa->lib_support->lib_task[i].task == NULL)
				err_hw("task is null");
			else
				flush_kthread_worker(&hw_3aa->lib_support->lib_task[i].worker);
		}
		fimc_is_lib_isp_stop(hw_ip, &hw_3aa->lib[instance], instance);
		clear_bit(HW_RUN, &hw_ip->state);
	} else {
		dbg_hw("[%d]already disabled (%d)\n", instance, hw_ip->id);
	}

	clear_bit(HW_CONFIG, &hw_ip->state);
exit:
	return ret;
}

int fimc_is_hw_3aa_shot(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_frame *frame,
	unsigned long hw_map)
{
	int ret = 0;
	struct fimc_is_hw_3aa *hw_3aa;
	struct taa_param_set *param_set;
	struct is_region *region;
	u32 lindex, hindex;

	BUG_ON(!hw_ip);
	BUG_ON(!frame);

	dbg_hw("[%d][ID:%d][F:%d]\n", frame->instance, hw_ip->id, frame->fcount);

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("not initialized!! (%d)\n", hw_ip->id);
		ret = -EINVAL;
		goto exit;
	}

	if (!test_bit(hw_ip->id, &hw_map))
		goto exit;

	set_bit(hw_ip->id, &frame->core_flag);

	region = hw_ip->region[frame->instance];
	hw_3aa = (struct fimc_is_hw_3aa *)hw_ip->priv_info;
	param_set = &hw_3aa->param_set[frame->instance];

	if (frame->type == SHOT_TYPE_INTERNAL) {
		/* OTF INPUT case */
		param_set->dma_input.cmd     = DMA_INPUT_COMMAND_DISABLE;
		param_set->input_buffer_addr = frame->dvaddr_buffer[0];
		param_set->vdma4_output.cmd  = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->output_buffer_addr_vdma4 = 0x0;
		param_set->vdma2_output.cmd  = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->output_buffer_addr_vdma2 = 0x0;
		hw_ip->internal_fcount = frame->fcount;
		goto config;
	} else {
		/* per-frame control
		 * check & update size from region */
		lindex = frame->shot->ctl.vendor_entry.lowIndexParam;
		hindex = frame->shot->ctl.vendor_entry.highIndexParam;

		if (hw_ip->internal_fcount != 0) {
			hw_ip->internal_fcount = 0;

			if (region->parameter.taa.otf_input.cmd != param_set->otf_input.cmd)
				lindex |= LOWBIT_OF(PARAM_3AA_OTF_INPUT);
			if (region->parameter.taa.vdma1_input.cmd != param_set->dma_input.cmd)
				lindex |= LOWBIT_OF(PARAM_3AA_VDMA1_INPUT);
			if (region->parameter.taa.otf_output.cmd != param_set->otf_output.cmd)
				lindex |= LOWBIT_OF(PARAM_3AA_OTF_OUTPUT);
			if (region->parameter.taa.vdma4_output.cmd != param_set->vdma4_output.cmd)
				lindex |= LOWBIT_OF(PARAM_3AA_VDMA4_OUTPUT);
			if (region->parameter.taa.vdma2_output.cmd != param_set->vdma2_output.cmd)
				lindex |= LOWBIT_OF(PARAM_3AA_VDMA2_OUTPUT);
		}
	}

	fimc_is_hw_3aa_update_param(hw_ip,
		region, param_set,
		lindex, hindex, frame->instance);

	/* DMA settings */
	param_set->input_buffer_addr = 0;
	param_set->output_buffer_addr_vdma4 = 0;
	param_set->output_buffer_addr_vdma2 = 0;

	if (param_set->dma_input.cmd != DMA_INPUT_COMMAND_DISABLE) {
		param_set->input_buffer_addr = frame->dvaddr_buffer[0];
		if (frame->dvaddr_buffer[0] == 0) {
			info_hw("[F:%d]dvaddr_buffer[0] is zero", frame->fcount);
			BUG_ON(1);
		}
	}

	if (param_set->vdma4_output.cmd != DMA_OUTPUT_COMMAND_DISABLE) {
		param_set->output_buffer_addr_vdma4 = frame->shot->uctl.scalerUd.txcTargetAddress[0];
		if (frame->shot->uctl.scalerUd.txcTargetAddress[0] == 0) {
			info_hw("[F:%d]txcTargetAddress[0] is zero", frame->fcount);
			param_set->vdma4_output.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		}
	}

	if (param_set->vdma2_output.cmd != DMA_OUTPUT_COMMAND_DISABLE) {
#if defined(ENABLE_3AAISP)
		/* handle vdma2 as 3ac node */
		param_set->output_buffer_addr_vdma2 = frame->shot->uctl.scalerUd.txcTargetAddress[0];
		if (frame->shot->uctl.scalerUd.txcTargetAddress[0] == 0) {
			info_hw("[F:%d]txcTargetAddress[0] is zero", frame->fcount);
			param_set->vdma2_output.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		}
#else
		param_set->output_buffer_addr_vdma2 = frame->shot->uctl.scalerUd.txpTargetAddress[0];
		if (frame->shot->uctl.scalerUd.txpTargetAddress[0] == 0) {
			info_hw("[F:%d]txpTargetAddress[0] is zero", frame->fcount);
			param_set->vdma2_output.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		}
#endif
	}

config:
	param_set->instance_id = frame->instance;
	param_set->fcount = frame->fcount;

	if (frame->type == SHOT_TYPE_INTERNAL) {
		fimc_is_log_write("[%d]3aa_shot [T:%d][R:%d][F:%d][IN:0x%x] [%d][C:0x%x] [%d][P:0x%x]\n",
				param_set->instance_id, frame->type, param_set->reprocessing, param_set->fcount,
				param_set->input_buffer_addr,
				param_set->vdma4_output.cmd, param_set->output_buffer_addr_vdma4,
				param_set->vdma2_output.cmd, param_set->output_buffer_addr_vdma2);
	}

	if (frame->shot) {
		param_set->sensor_config.min_target_fps = frame->shot->ctl.aa.aeTargetFpsRange[0];
		param_set->sensor_config.max_target_fps = frame->shot->ctl.aa.aeTargetFpsRange[1];
		param_set->sensor_config.frametime = 1000000 / param_set->sensor_config.min_target_fps;
		dbg_hw("3aa_shot: min_fps(%d), max_fps(%d), frametime(%d)\n",
			param_set->sensor_config.min_target_fps,
			param_set->sensor_config.max_target_fps,
			param_set->sensor_config.frametime);

		ret = fimc_is_lib_isp_convert_face_map(hw_ip->hardware, param_set, frame);
		if (ret)
			err_hw("[%d] Convert face size is fail : %d\n", frame->instance, ret);

		ret = fimc_is_lib_isp_set_ctrl(hw_ip, &hw_3aa->lib[frame->instance], frame);
		if (ret)
			err_hw("[%d] set_ctrl fail", frame->instance);
	}
	if (fimc_is_lib_isp_sensor_update_control(&hw_3aa->lib[frame->instance], frame->instance, frame->fcount, frame->shot) < 0) {
		err_hw("[%d] fimc_is_lib_isp_sensor_update_control fail (%d)", frame->instance, hw_ip->id);
	}

	fimc_is_lib_isp_shot(hw_ip, &hw_3aa->lib[frame->instance], param_set, frame->shot);

	set_bit(HW_CONFIG, &hw_ip->state);
exit:
	return ret;
}

int fimc_is_hw_3aa_set_param(struct fimc_is_hw_ip *hw_ip,
	struct is_region *region,
	u32 lindex,
	u32 hindex,
	u32 instance,
	unsigned long hw_map)
{
	int ret = 0;
	struct fimc_is_hw_3aa *hw_3aa;
	struct taa_param_set *param_set;

	BUG_ON(!hw_ip);
	BUG_ON(!region);

	hw_3aa = (struct fimc_is_hw_3aa *)hw_ip->priv_info;
	param_set = &hw_3aa->param_set[instance];

	if (!test_bit(hw_ip->id, &hw_map))
		goto exit;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("not initialized!! (%d)", hw_ip->id);
		ret = -EINVAL;
		goto exit;
	}

	hw_ip->region[instance] = region;
	hw_ip->lindex[instance] = lindex;
	hw_ip->hindex[instance] = hindex;

	fimc_is_hw_3aa_update_param(hw_ip,
		region, param_set,
		lindex, hindex, instance);

	param_set->instance_id = instance;
	ret = fimc_is_lib_isp_set_param(hw_ip, &hw_3aa->lib[instance], param_set);
	if (ret)
		err_hw("[%d] set_param fail", instance);

exit:
	return ret;
}

void fimc_is_hw_3aa_update_param(struct fimc_is_hw_ip *hw_ip,
	struct is_region *region,
	struct taa_param_set *param_set,
	u32 lindex,
	u32 hindex,
	u32 instance)
{
	BUG_ON(!region);
	BUG_ON(!param_set);

	if (lindex & LOWBIT_OF(PARAM_SENSOR_CONFIG)) {
		memcpy(&param_set->sensor_config,
			&region->parameter.sensor.config,
			sizeof(struct param_sensor_config));
	}

	/* check input */
	if (lindex & LOWBIT_OF(PARAM_3AA_OTF_INPUT)) {
		memcpy(&param_set->otf_input,
			&region->parameter.taa.otf_input,
			sizeof(struct param_otf_input));
	}

	if (lindex & LOWBIT_OF(PARAM_3AA_VDMA1_INPUT)) {
		memcpy(&param_set->dma_input,
			&region->parameter.taa.vdma1_input,
			sizeof(struct param_dma_input));
	}

	/* check output*/
	if (lindex & LOWBIT_OF(PARAM_3AA_OTF_OUTPUT)) {
		memcpy(&param_set->otf_output,
			&region->parameter.taa.otf_output,
			sizeof(struct param_otf_output));
	}

	if (lindex & LOWBIT_OF(PARAM_3AA_VDMA4_OUTPUT)) {
		memcpy(&param_set->vdma4_output,
			&region->parameter.taa.vdma4_output,
			sizeof(struct param_dma_output));
	}

	if (lindex & LOWBIT_OF(PARAM_3AA_VDMA2_OUTPUT)) {
		memcpy(&param_set->vdma2_output,
			&region->parameter.taa.vdma2_output,
			sizeof(struct param_dma_output));
	}
}

int fimc_is_hw_3aa_get_meta(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_frame *frame,
	unsigned long hw_map)
{
	int ret = 0;
	struct fimc_is_hw_3aa *hw_3aa;

	if (!test_bit(hw_ip->id, &hw_map))
		goto exit;

	hw_3aa = (struct fimc_is_hw_3aa *)hw_ip->priv_info;

	ret = fimc_is_lib_isp_get_meta(hw_ip, &hw_3aa->lib[frame->instance], frame);
	if (ret)
		err_hw("[%d] get_meta fail", frame->instance);

exit:
	return ret;
}

int fimc_is_hw_3aa_frame_ndone(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame, u32 instance, bool late_flag)
{
	int wq_id_3xc, wq_id_3xp;
	int output_id;
	enum fimc_is_frame_done_type done_type;
	int ret = 0;

	BUG_ON(!hw_ip);

	switch (hw_ip->id) {
	case DEV_HW_3AA0:
		wq_id_3xc = WORK_30C_FDONE;
		wq_id_3xp = WORK_30P_FDONE;
		break;
	case DEV_HW_3AA1:
		wq_id_3xc = WORK_31C_FDONE;
		wq_id_3xp = WORK_31P_FDONE;
		break;
	default:
		err_hw("invalid hw (%d)", hw_ip->id);
		goto p_err;
		break;
	}

	if (late_flag == true)
		done_type = FRAME_DONE_LATE_SHOT;
	else
		done_type = FRAME_DONE_FORCE;

	output_id = ENTRY_3AC;
	if (test_bit(output_id, &frame->out_flag))
		ret = fimc_is_hardware_frame_done(hw_ip, frame, wq_id_3xc, output_id, 1, done_type);

	output_id = ENTRY_3AP;
	if (test_bit(output_id, &frame->out_flag))
		ret = fimc_is_hardware_frame_done(hw_ip, frame, wq_id_3xp, output_id, 1, done_type);

	output_id = FIMC_IS_HW_CORE_END;
	if (test_bit(hw_ip->id, &frame->core_flag))
		ret = fimc_is_hardware_frame_done(hw_ip, frame, -1, output_id, 1, done_type);

p_err:
	return ret;
}

int fimc_is_hw_3aa_load_setfile(struct fimc_is_hw_ip *hw_ip, int index, u32 instance, unsigned long hw_map)
{
	struct fimc_is_hw_3aa *hw_3aa = NULL;
	u32 addr, size;
	int flag = 0, ret = 0;

	BUG_ON(!hw_ip);

	/* skip */
	if ((hw_ip->id == DEV_HW_3AA0) && test_bit(DEV_HW_3AA1, &hw_map))
		goto exit;
	else if ((hw_ip->id == DEV_HW_3AA1) && test_bit(DEV_HW_3AA0, &hw_map))
		goto exit;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("[%d] not initialized!!", instance);
		ret = -ESRCH;
		goto exit;
	}

	switch (hw_ip->setfile_info.version) {
	case SETFILE_V2:
		flag = false;
		break;
	case SETFILE_V3:
		flag = true;
		break;
	default:
		err_hw("[%d][ID:%d] invalid version (%d)", instance, hw_ip->id, hw_ip->setfile_info.version);
		ret = -EINVAL;
		goto exit;
		break;
	}

	hw_3aa = (struct fimc_is_hw_3aa *)hw_ip->priv_info;
	addr = hw_ip->setfile_info.table[index].addr;
	size = hw_ip->setfile_info.table[index].size;
	ret = fimc_is_lib_isp_create_tune_set(&hw_3aa->lib[instance],
		addr, size, (u32)index, flag, instance);

	set_bit(HW_TUNESET, &hw_ip->state);
exit:
	return ret;
}

int fimc_is_hw_3aa_apply_setfile(struct fimc_is_hw_ip *hw_ip, int index, u32 instance, unsigned long hw_map)
{
	struct fimc_is_hw_3aa *hw_3aa = NULL;
	u32 cal_addr, setfile_index = 0;
	int ret = 0;

	BUG_ON(!hw_ip);

	/* skip */
	if ((hw_ip->id == DEV_HW_3AA0) && test_bit(DEV_HW_3AA1, &hw_map))
		goto exit;
	else if ((hw_ip->id == DEV_HW_3AA1) && test_bit(DEV_HW_3AA0, &hw_map))
		goto exit;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("[%d] not initialized!!", instance);
		ret = -ESRCH;
		goto exit;
	}

	if (hw_ip->setfile_info.num == 0)
		goto exit;

	setfile_index = hw_ip->setfile_info.index[index];
	info_hw("[%d][ID:%d] setfile (%d) scenario (%d)\n", instance, hw_ip->id, setfile_index, index);

	hw_3aa = (struct fimc_is_hw_3aa *)hw_ip->priv_info;
	ret = fimc_is_lib_isp_apply_tune_set(&hw_3aa->lib[instance], setfile_index, instance);

	if (hw_ip->sensor_position == SENSOR_POSITION_REAR)
		cal_addr = hw_3aa->lib_support->kvaddr + FIMC_IS_REAR_CALDATA_OFFSET;
	else if (hw_ip->sensor_position == SENSOR_POSITION_FRONT)
		cal_addr = hw_3aa->lib_support->kvaddr + FIMC_IS_FRONT_CALDATA_OFFSET;
	else
		goto exit;

	ret = fimc_is_lib_isp_load_cal_data(&hw_3aa->lib[instance], instance, cal_addr);

exit:
	return ret;
}

int fimc_is_hw_3aa_delete_setfile(struct fimc_is_hw_ip *hw_ip, u32 instance, unsigned long hw_map)
{
	struct fimc_is_hw_3aa *hw_3aa = NULL;
	int i, ret = 0;

	BUG_ON(!hw_ip);

	/* skip */
	if ((hw_ip->id == DEV_HW_3AA0) && test_bit(DEV_HW_3AA1, &hw_map))
		goto exit;
	else if ((hw_ip->id == DEV_HW_3AA1) && test_bit(DEV_HW_3AA0, &hw_map))
		goto exit;

	if (!test_bit(HW_TUNESET, &hw_ip->state)) {
		dbg_hw("[%d][ID:%d] Tuneset is not loaded: skip\n", instance, hw_ip->id);
		goto exit;
	}

	if (hw_ip->setfile_info.num == 0)
		goto exit;

	hw_3aa = (struct fimc_is_hw_3aa *)hw_ip->priv_info;
	for (i = 0; i < hw_ip->setfile_info.num; i++)
		ret = fimc_is_lib_isp_delete_tune_set(&hw_3aa->lib[instance], (u32)i, instance);

exit:
	return ret;
}

void fimc_is_hw_3aa_size_dump(struct fimc_is_hw_ip *hw_ip)
{
	u32 bcrop_w = 0, bcrop_h = 0;

	fimc_is_isp_get_bcrop1_size(hw_ip->base_addr, &bcrop_w, &bcrop_h);

	info_hw("[3AA]=SIZE=====================================\n");
	info_hw("[BCROP1]w(%d), h(%d)\n", bcrop_w, bcrop_h);
	info_hw("[3AA]==========================================\n");
}

void fimc_is_hw_3aa_dump(void)
{
	int ret = 0;

	ret = fimc_is_lib_logdump();
}
