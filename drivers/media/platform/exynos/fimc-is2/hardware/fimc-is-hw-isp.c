/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-hw-isp.h"

extern struct fimc_is_lib_support gPtr_lib_support;

const struct fimc_is_hw_ip_ops fimc_is_hw_isp_ops = {
	.open			= fimc_is_hw_isp_open,
	.init			= fimc_is_hw_isp_init,
	.close			= fimc_is_hw_isp_close,
	.enable			= fimc_is_hw_isp_enable,
	.disable		= fimc_is_hw_isp_disable,
	.shot			= fimc_is_hw_isp_shot,
	.set_param		= fimc_is_hw_isp_set_param,
	.frame_ndone		= fimc_is_hw_isp_frame_ndone,
	.load_setfile		= fimc_is_hw_isp_load_setfile,
	.apply_setfile		= fimc_is_hw_isp_apply_setfile,
	.delete_setfile		= fimc_is_hw_isp_delete_setfile
};

int fimc_is_hw_isp_probe(struct fimc_is_hw_ip *hw_ip,
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
	hw_ip->ops  = &fimc_is_hw_isp_ops;
	hw_ip->itf  = itf;
	hw_ip->itfc = itfc;
	hw_ip->fcount = 0;
	hw_ip->internal_fcount = 0;
	hw_ip->is_leader = true;
	atomic_set(&hw_ip->Vvalid, 0);
	atomic_set(&hw_ip->ref_count, 0);
	init_waitqueue_head(&hw_ip->wait_queue);

	/* set isp sfr base address */
	switch (id) {
	case DEV_HW_ISP0:
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

int fimc_is_hw_isp_open(struct fimc_is_hw_ip *hw_ip, u32 instance, u32 *size)
{
	int ret = 0;

	BUG_ON(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		goto p_err;

	*size = sizeof(struct fimc_is_hw_isp);

	frame_manager_probe(hw_ip->framemgr, FRAMEMGR_ID_HW | hw_ip->id);
	frame_manager_open(hw_ip->framemgr, FIMC_IS_MAX_HW_FRAME);

p_err:
	return ret;
}

int fimc_is_hw_isp_init(struct fimc_is_hw_ip *hw_ip, struct fimc_is_group *group,
		bool flag, u32 module_id)
{
	int ret = 0;
	int i = 0;
	u32 instance = 0;
	struct fimc_is_hw_isp *hw_isp = NULL;

	BUG_ON(!hw_ip);
	BUG_ON(!hw_ip->priv_info);
	BUG_ON(!group);

	hw_isp = (struct fimc_is_hw_isp *)hw_ip->priv_info;
	instance = group->instance;
	hw_ip->group[instance] = group;

	if (hw_isp->lib_func == NULL) {
		ret = get_lib_func(LIB_FUNC_ISP, (void **)&hw_isp->lib_func);
		dbg_hw("lib_interface_func is set (%d)\n", hw_ip->id);
	}

	if (hw_isp->lib_func == NULL) {
		err_hw("func_isp(null) (%d)", hw_ip->id);
		fimc_is_load_clear();
		ret = -EINVAL;
		goto p_err;
	}
	hw_isp->lib_support = &gPtr_lib_support;
	for (i = 0; i < FIMC_IS_MAX_NODES; i++) {
		hw_isp->lib[i].object = NULL;
		hw_isp->lib[i].func   = hw_isp->lib_func;
	}
	hw_isp->param_set[instance].reprocessing = flag;

	if (test_bit(HW_INIT, &hw_ip->state)) {
		dbg_hw("[%d]chain is already created (%d)\n", instance, hw_ip->id);
	} else {
		ret = fimc_is_lib_isp_chain_create(hw_ip, &hw_isp->lib[instance], instance);
		if (ret) {
			err_hw("[%d]chain create fail (%d)", instance, hw_ip->id);
			ret = -EINVAL;
			goto p_err;
		}
	}

	if (hw_isp->lib[instance].object != NULL) {
		dbg_hw("[%d]object is already created (%d)\n", instance, hw_ip->id);
	} else {
		ret = fimc_is_lib_isp_object_create(hw_ip, &hw_isp->lib[instance],
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

int fimc_is_hw_isp_object_close(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct fimc_is_hw_isp *hw_isp;

	BUG_ON(!hw_ip);

	hw_isp = (struct fimc_is_hw_isp *)hw_ip->priv_info;

	fimc_is_lib_isp_object_destroy(hw_ip, &hw_isp->lib[instance], instance);
        hw_isp->lib[instance].object = NULL;

	return ret;
}

int fimc_is_hw_isp_close(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	int i = 0;
	struct fimc_is_hw_isp *hw_isp;

	BUG_ON(!hw_ip);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		goto exit;

	hw_isp = (struct fimc_is_hw_isp *)hw_ip->priv_info;

	/* TODO: check 3aa */
	for (i = 0; i < FIMC_IS_MAX_TASK_WORKER; i++) {
		if (hw_isp->lib_support->lib_task[i].task == NULL) {
			err_hw("task is null");
		} else {
			ret = kthread_stop(hw_isp->lib_support->lib_task[i].task);
			if (ret)
				err_hw("kthread_stop fail (%d)", ret);
		}
	}

	fimc_is_lib_isp_chain_destroy(hw_ip, &hw_isp->lib[instance], instance);
	frame_manager_close(hw_ip->framemgr);

	info_hw("[%d]close (%d)(%d)\n", instance, hw_ip->id, atomic_read(&hw_ip->ref_count));
exit:
	return ret;
}

int fimc_is_hw_isp_enable(struct fimc_is_hw_ip *hw_ip, u32 instance, unsigned long hw_map)
{
	int ret = 0;

	BUG_ON(!hw_ip);

	if (!test_bit(hw_ip->id, &hw_map))
		goto exit;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("[%d]not initialized!! (%d)", instance, hw_ip->id);
		ret = -EINVAL;
		goto exit;
	}

	set_bit(HW_RUN, &hw_ip->state);

exit:
	return ret;
}

int fimc_is_hw_isp_disable(struct fimc_is_hw_ip *hw_ip, u32 instance, unsigned long hw_map)
{
	int ret = 0;
	u32 timetowait;
	struct fimc_is_hw_isp *hw_isp;
	struct isp_param_set *param_set;

	BUG_ON(!hw_ip);

	if (!test_bit(hw_ip->id, &hw_map))
		goto exit;

	hw_isp = (struct fimc_is_hw_isp *)hw_ip->priv_info;
	param_set = &hw_isp->param_set[instance];

	timetowait = wait_event_timeout(hw_ip->wait_queue,
		!atomic_read(&hw_ip->Vvalid),
		FIMC_IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		err_hw("[%d][ID:%d] wait FRAME_END timeout (%u)", instance, hw_ip->id, timetowait);
		ret = -ETIME;
	}

	param_set->fcount = 0;
	if (test_bit(HW_RUN, &hw_ip->state)) {
		fimc_is_lib_isp_stop(hw_ip, &hw_isp->lib[instance], instance);
		clear_bit(HW_RUN, &hw_ip->state);
	} else {
		dbg_hw("[%d]already disabled (%d)\n", instance, hw_ip->id);
	}

exit:
	return ret;
}

int fimc_is_hw_isp_shot(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_frame *frame,
	unsigned long hw_map)
{
	int ret = 0;
	struct fimc_is_hw_isp *hw_isp;
	struct isp_param_set *param_set;
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
	hw_isp = (struct fimc_is_hw_isp *)hw_ip->priv_info;
	param_set = &hw_isp->param_set[frame->instance];

	if (frame->type == SHOT_TYPE_INTERNAL) {
		/* OTF INPUT case */
		param_set->dma_input.cmd     = DMA_INPUT_COMMAND_DISABLE;
		param_set->input_buffer_addr = frame->dvaddr_buffer[0];
		param_set->vdma4_output.cmd  = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->output_buffer_addr_vdma4 = 0x0;
		param_set->vdma5_output.cmd  = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->output_buffer_addr_vdma5 = 0x0;
		goto config;
	}

	/* per-frame control
	 * check & update size from region */
	lindex = frame->shot->ctl.vendor_entry.lowIndexParam;
	hindex = frame->shot->ctl.vendor_entry.highIndexParam;

	fimc_is_hw_isp_update_param(hw_ip,
		region, param_set,
		lindex, hindex, frame->instance);

	/* DMA settings */
	param_set->input_buffer_addr = 0;
	param_set->output_buffer_addr_vdma4 = 0;
	param_set->output_buffer_addr_vdma5 = 0;

	if (param_set->dma_input.cmd != DMA_INPUT_COMMAND_DISABLE) {
		param_set->input_buffer_addr = frame->dvaddr_buffer[0];
		if (frame->dvaddr_buffer[0] == 0) {
			info_hw("[F:%d]dvaddr_buffer[0] is zero", frame->fcount);
			BUG_ON(1);
		}
	}

	if (param_set->vdma4_output.cmd != DMA_OUTPUT_COMMAND_DISABLE) {
		param_set->output_buffer_addr_vdma4 = frame->shot->uctl.scalerUd.ixcTargetAddress[0];
		if (frame->shot->uctl.scalerUd.ixcTargetAddress[0] == 0) {
			info_hw("[F:%d]ixcTargetAddress[0] is zero", frame->fcount);
			param_set->vdma4_output.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		}
	}

	if (param_set->vdma5_output.cmd != DMA_OUTPUT_COMMAND_DISABLE) {
		param_set->output_buffer_addr_vdma5 = frame->shot->uctl.scalerUd.ixpTargetAddress[0];
		if (frame->shot->uctl.scalerUd.ixpTargetAddress[0] == 0) {
			info_hw("[F:%d]ixpTargetAddress[0] is zero", frame->fcount);
			param_set->vdma5_output.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		}
	}

config:
	param_set->instance_id = frame->instance;
	param_set->fcount = frame->fcount;

	if (frame->type == SHOT_TYPE_INTERNAL) {
		fimc_is_log_write("[%d]isp_shot [T:%d][R:%d][F:%d][IN:0x%x] [%d][OUT:0x%x]\n",
				param_set->instance_id, frame->type, param_set->reprocessing, param_set->fcount,
				param_set->input_buffer_addr,
				param_set->vdma5_output.cmd, param_set->output_buffer_addr_vdma5);
	}

	if (frame->shot) {
		ret = fimc_is_lib_isp_set_ctrl(hw_ip, &hw_isp->lib[frame->instance], frame);
		if (ret)
			err_hw("[%d] set_ctrl fail", frame->instance);
	}

	fimc_is_lib_isp_shot(hw_ip, &hw_isp->lib[frame->instance], param_set, frame->shot);

	set_bit(HW_CONFIG, &hw_ip->state);
exit:
	return ret;
}

int fimc_is_hw_isp_set_param(struct fimc_is_hw_ip *hw_ip,
	struct is_region *region,
	u32 lindex,
	u32 hindex,
	u32 instance,
	unsigned long hw_map)
{
	int ret = 0;
	struct fimc_is_hw_isp *hw_isp;
	struct isp_param_set *param_set;

	BUG_ON(!hw_ip);
	BUG_ON(!region);

	hw_isp = (struct fimc_is_hw_isp *)hw_ip->priv_info;
	param_set = &hw_isp->param_set[instance];

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

	fimc_is_hw_isp_update_param(hw_ip,
		region, param_set,
		lindex, hindex, instance);

exit:
	return ret;
}

void fimc_is_hw_isp_update_param(struct fimc_is_hw_ip *hw_ip,
	struct is_region *region,
	struct isp_param_set *param_set,
	u32 lindex,
	u32 hindex,
	u32 instance)
{
	BUG_ON(!region);
	BUG_ON(!param_set);

	/* check input */
	if (lindex & LOWBIT_OF(PARAM_ISP_OTF_INPUT)) {
		memcpy(&param_set->otf_input,
			&region->parameter.isp.otf_input,
			sizeof(struct param_otf_input));
	}

	if (lindex & LOWBIT_OF(PARAM_ISP_VDMA1_INPUT)) {
		memcpy(&param_set->dma_input,
			&region->parameter.isp.vdma1_input,
			sizeof(struct param_dma_input));
	}

	/* check output*/
	if (lindex & LOWBIT_OF(PARAM_ISP_OTF_OUTPUT)) {
		memcpy(&param_set->otf_output,
			&region->parameter.isp.otf_output,
			sizeof(struct param_otf_output));
	}

	if (lindex & LOWBIT_OF(PARAM_ISP_VDMA4_OUTPUT)) {
		memcpy(&param_set->vdma4_output,
			&region->parameter.isp.vdma4_output,
			sizeof(struct param_dma_output));
	}

	if (lindex & LOWBIT_OF(PARAM_ISP_VDMA5_OUTPUT)) {
		memcpy(&param_set->vdma5_output,
			&region->parameter.isp.vdma5_output,
			sizeof(struct param_dma_output));
	}
}

int fimc_is_hw_isp_frame_ndone(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame, u32 instance, bool late_flag)
{
	int wq_id_ixc, wq_id_ixp;
	int output_id;
	enum fimc_is_frame_done_type done_type;
	int ret = 0;

	BUG_ON(!hw_ip);

	switch (hw_ip->id) {
	case DEV_HW_ISP0:
		wq_id_ixc = WORK_I0C_FDONE;
		wq_id_ixp = WORK_I0P_FDONE;
		break;
	case DEV_HW_ISP1:
		wq_id_ixc = WORK_I1C_FDONE;
		wq_id_ixp = WORK_I1P_FDONE;
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

	output_id = ENTRY_IXC;
	if (test_bit(output_id, &frame->out_flag))
		ret = fimc_is_hardware_frame_done(hw_ip, frame, wq_id_ixc, output_id, 1, done_type);

	output_id = ENTRY_IXP;
	if (test_bit(output_id, &frame->out_flag))
		ret = fimc_is_hardware_frame_done(hw_ip, frame, wq_id_ixp, output_id, 1, done_type);

	output_id = FIMC_IS_HW_CORE_END;
	if (test_bit(hw_ip->id, &frame->core_flag))
		ret = fimc_is_hardware_frame_done(hw_ip, frame, -1, output_id, 1, done_type);

p_err:
	return ret;
}

int fimc_is_hw_isp_load_setfile(struct fimc_is_hw_ip *hw_ip, int index, u32 instance, unsigned long hw_map)
{
	struct fimc_is_hw_isp *hw_isp = NULL;
	u32 addr, size;
	int flag = 0, ret = 0;

	BUG_ON(!hw_ip);

	/* skip */
	if ((hw_ip->id == DEV_HW_ISP0) && test_bit(DEV_HW_ISP1, &hw_map))
		goto exit;
	else if ((hw_ip->id == DEV_HW_ISP1) && test_bit(DEV_HW_ISP0, &hw_map))
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

	addr = hw_ip->setfile_info.table[index].addr;
	size = hw_ip->setfile_info.table[index].size;
	hw_isp = (struct fimc_is_hw_isp *)hw_ip->priv_info;
	ret = fimc_is_lib_isp_create_tune_set(&hw_isp->lib[instance],
		addr,
		size,
		(u32)index,
		flag,
		instance);
	set_bit(HW_TUNESET, &hw_ip->state);
exit:
	return ret;
}

int fimc_is_hw_isp_apply_setfile(struct fimc_is_hw_ip *hw_ip, int index, u32 instance, unsigned long hw_map)
{
	struct fimc_is_hw_isp *hw_isp = NULL;
	u32 setfile_index = 0;
	int ret = 0;

	BUG_ON(!hw_ip);

	/* skip */
	if ((hw_ip->id == DEV_HW_ISP0) && test_bit(DEV_HW_ISP1, &hw_map))
		goto exit;
	else if ((hw_ip->id == DEV_HW_ISP1) && test_bit(DEV_HW_ISP0, &hw_map))
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

	hw_isp = (struct fimc_is_hw_isp *)hw_ip->priv_info;
	ret = fimc_is_lib_isp_apply_tune_set(&hw_isp->lib[instance], setfile_index, instance);

exit:
	return ret;
}

int fimc_is_hw_isp_delete_setfile(struct fimc_is_hw_ip *hw_ip, u32 instance, unsigned long hw_map)
{
	struct fimc_is_hw_isp *hw_isp = NULL;
	int i, ret = 0;

	BUG_ON(!hw_ip);

	/* skip */
	if ((hw_ip->id == DEV_HW_ISP0) && test_bit(DEV_HW_ISP1, &hw_map))
		goto exit;
	else if ((hw_ip->id == DEV_HW_ISP1) && test_bit(DEV_HW_ISP0, &hw_map))
		goto exit;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		dbg_hw("[%d][ID:%d] Not initialized\n", instance, hw_ip->id);
		goto exit;
	}

	if (hw_ip->setfile_info.num == 0)
		goto exit;

	hw_isp = (struct fimc_is_hw_isp *)hw_ip->priv_info;
	for (i = 0; i < hw_ip->setfile_info.num; i++)
		ret = fimc_is_lib_isp_delete_tune_set(&hw_isp->lib[instance], (u32)i, instance);

exit:
	return ret;
}
