/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-hw-mcscaler.h"
#include "api/fimc-is-hw-api-mcscaler.h"
#include "../interface/fimc-is-interface-ischain.h"

static int fimc_is_hw_mcsc_handle_interrupt(void *data)
{
	struct fimc_is_hw_ip *hw_ip = NULL;
	u32 status, intr_mask, intr_status;
	bool err_intr_flag = false;
	int ret = 0;

	hw_ip = (struct fimc_is_hw_ip *)data;

	/* read interrupt status register (sc_intr_status) */
	intr_mask = fimc_is_scaler_get_intr_mask(hw_ip->base_addr);
	intr_status = fimc_is_scaler_get_intr_status(hw_ip->base_addr);
	status = (~intr_mask) & intr_status;

	if (status & (1 << INTR_MC_SCALER_OVERFLOW)) {
		err_hw("[MCSC]OverFlow!!");
		err_intr_flag = true;
	}

	if (status & (1 << INTR_MC_SCALER_OUTSTALL)) {
		err_hw("[MCSC]Output Block BLOCKING!!");
		err_intr_flag = true;
	}

	if (status & (1 << INTR_MC_SCALER_INPUT_VERTICAL_UNF)) {
		err_hw("[MCSC]Input OTF Vertical Underflow!! (0x%x)", status);
		err_intr_flag = true;
	}

	if (status & (1 << INTR_MC_SCALER_INPUT_VERTICAL_OVF)) {
		err_hw("[MCSC]Input OTF Vertical Overflow!! (0x%x)", status);
		err_intr_flag = true;
	}

	if (status & (1 << INTR_MC_SCALER_INPUT_HORIZONTAL_UNF)) {
		err_hw("[MCSC]Input OTF Horizontal Underflow!! (0x%x)", status);
		err_intr_flag = true;
	}

	if (status & (1 << INTR_MC_SCALER_INPUT_HORIZONTAL_OVF)) {
		err_hw("[MCSC]Input OTF Horizontal Overflow!! (0x%x)", status);
		err_intr_flag = true;
	}

	if (status & (1 << INTR_MC_SCALER_INPUT_PROTOCOL_ERR)) {
		err_hw("[MCSC]Input Protocol Error!! (0x%x)", status);
		err_intr_flag = true;
	}

	if (status & (1 << INTR_MC_SCALER_WDMA_FINISH)) {
		hw_ip->dma_time = cpu_clock(raw_smp_processor_id());
		if (!atomic_read(&hw_ip->hardware->stream_on))
			info_hw("[ID:%d][F:%d]F.E DMA\n", hw_ip->id, hw_ip->fcount);

		atomic_inc(&hw_ip->cl_count);
		fimc_is_hardware_frame_done(hw_ip, NULL, WORK_SCP_FDONE, ENTRY_SCP, 0, FRAME_DONE_NORMAL);
	}

	if (status & (1 << INTR_MC_SCALER_FRAME_START)) {
		hw_ip->fs_time = cpu_clock(raw_smp_processor_id());
		if (!atomic_read(&hw_ip->hardware->stream_on))
			info_hw("[ID:%d][F:%d]F.S\n", hw_ip->id, hw_ip->fcount);

		atomic_set(&hw_ip->Vvalid, V_VALID);
		clear_bit(HW_CONFIG, &hw_ip->state);
		atomic_inc(&hw_ip->fs_count);
	}

	if (status & (1 << INTR_MC_SCALER_FRAME_END)) {
		hw_ip->fe_time = cpu_clock(raw_smp_processor_id());
		if (!atomic_read(&hw_ip->hardware->stream_on))
			info_hw("[ID:%d][F:%d]F.E\n", hw_ip->id, hw_ip->fcount);

		atomic_set(&hw_ip->Vvalid, V_BLANK);
		if (atomic_read(&hw_ip->fs_count) == atomic_read(&hw_ip->fe_count)) {
			err_hw("[MCSC] fs(%d), fe(%d), cl(%d)\n",
				atomic_read(&hw_ip->fs_count),
				atomic_read(&hw_ip->fe_count),
				atomic_read(&hw_ip->cl_count));
		}
		atomic_inc(&hw_ip->fe_count);
		fimc_is_hardware_frame_done(hw_ip, NULL, -1, FIMC_IS_HW_CORE_END, 0, FRAME_DONE_NORMAL);
	}

	if (err_intr_flag) {
		u32 hl = 0, vl = 0;

		fimc_is_scaler_get_input_status(hw_ip->base_addr, &hl, &vl);
		info_hw("[MCSC][F:%d] Ocurred error interrupt (%d,%d)\n", hw_ip->fcount, hl, vl);
		fimc_is_hardware_size_dump(hw_ip);
	}

	fimc_is_scaler_clear_intr_src(hw_ip->base_addr, status);

	return ret;
}

const struct fimc_is_hw_ip_ops fimc_is_hw_mcsc_ops = {
	.open			= fimc_is_hw_mcsc_open,
	.init			= fimc_is_hw_mcsc_init,
	.close			= fimc_is_hw_mcsc_close,
	.enable			= fimc_is_hw_mcsc_enable,
	.disable		= fimc_is_hw_mcsc_disable,
	.shot			= fimc_is_hw_mcsc_shot,
	.set_param		= fimc_is_hw_mcsc_set_param,
	.frame_ndone		= fimc_is_hw_mcsc_frame_ndone,
	.load_setfile		= fimc_is_hw_mcsc_load_setfile,
	.apply_setfile		= fimc_is_hw_mcsc_apply_setfile,
	.delete_setfile		= fimc_is_hw_mcsc_delete_setfile,
	.size_dump		= fimc_is_hw_mcsc_size_dump
};

int fimc_is_hw_mcsc_probe(struct fimc_is_hw_ip *hw_ip,
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
	hw_ip->ops  = &fimc_is_hw_mcsc_ops;
	hw_ip->itf  = itf;
	hw_ip->itfc = itfc;
	hw_ip->fcount = 0;
	hw_ip->internal_fcount = 0;
	hw_ip->is_leader = false;
	atomic_set(&hw_ip->Vvalid, 0);
	atomic_set(&hw_ip->ref_count, 0);
	init_waitqueue_head(&hw_ip->wait_queue);

	/* set mcsc sfr base address */
	intr_slot = fimc_is_intr_slot_id(id);
	if (!valid_subip_intr_slot_id(intr_slot)) {
		err_hw("invalid intr_slot (%d, %d)", id, intr_slot);
		goto p_err;
	}
	hw_ip->base_addr = itfc->subip[intr_slot].regs;

	/* set mcsc interrupt handler */
	itfc->subip[intr_slot].handler = &fimc_is_hw_mcsc_handle_interrupt;

	clear_bit(HW_OPEN, &hw_ip->state);
	clear_bit(HW_INIT, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);
	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_TUNESET, &hw_ip->state);

p_err:
	return ret;
}

int fimc_is_hw_mcsc_open(struct fimc_is_hw_ip *hw_ip, u32 instance, u32 *size)
{
	int ret = 0;
	u32 intr_mask = 0;

	BUG_ON(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		goto exit;

	*size = sizeof(struct fimc_is_hw_mcsc);

	ret = fimc_is_scaler_sw_reset(hw_ip->base_addr);
	if (ret != 0) {
		err_hw("MCSC sw reset fail");
		ret = -ENODEV;
		goto exit;
	}

	fimc_is_scaler_clear_intr_all(hw_ip->base_addr);
	fimc_is_scaler_disable_intr(hw_ip->base_addr);

	atomic_set(&hw_ip->Vvalid, V_BLANK);

	intr_mask = INTR_MASK;
	fimc_is_scaler_mask_intr(hw_ip->base_addr, intr_mask);

exit:
	return ret;
}

int fimc_is_hw_mcsc_init(struct fimc_is_hw_ip *hw_ip, struct fimc_is_group *group,
		bool flag, u32 module_id)
{
	int ret = 0;
	u32 instance = 0;

	BUG_ON(!hw_ip);

	instance = group->instance;
	hw_ip->group[instance] = group;

	dbg_hw("[%d][ID:%d] hw_mcsc init : Gr(%d)\n", instance, hw_ip->id, group->id);

	return ret;
}

int fimc_is_hw_mcsc_close(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;

	BUG_ON(!hw_ip);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		goto exit;

	info_hw("[%d]close (%d)(%d)\n", instance, hw_ip->id, atomic_read(&hw_ip->ref_count));

exit:
	return ret;
}

int fimc_is_hw_mcsc_enable(struct fimc_is_hw_ip *hw_ip, u32 instance, unsigned long hw_map)
{
	int ret = 0;

	BUG_ON(!hw_ip);

	dbg_hw("[%d][ID:%d] hw_mcsc enable\n", instance, hw_ip->id);

	if (!test_bit(hw_ip->id, &hw_map))
		goto exit;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("[%d]not initialized!! (%d)", instance, hw_ip->id);
		ret = -EINVAL;
		goto exit;
	}

	if (!test_bit(HW_RUN, &hw_ip->state))
		fimc_is_scaler_start(hw_ip->base_addr);

	set_bit(HW_RUN, &hw_ip->state);

exit:
	return ret;
}

int fimc_is_hw_mcsc_disable(struct fimc_is_hw_ip *hw_ip, u32 instance, unsigned long hw_map)
{
	int ret = 0;
	u32 intr_mask = 0;

	BUG_ON(!hw_ip);

	dbg_hw("[%d][ID:%d] hw_mcsc disable\n", instance, hw_ip->id);

	if (test_bit(HW_RUN, &hw_ip->state)) {
		/* disable MCSC */
		fimc_is_scaler_clear_dma_out_addr(hw_ip->base_addr);
		fimc_is_scaler_stop(hw_ip->base_addr);

		ret = fimc_is_scaler_sw_reset(hw_ip->base_addr);
		if (ret != 0) {
			err_hw("MCSC sw reset fail");
			ret = -ENODEV;
			goto exit;
		}

		intr_mask = INTR_MASK;
		fimc_is_scaler_clear_intr_all(hw_ip->base_addr);
		fimc_is_scaler_disable_intr(hw_ip->base_addr);
		fimc_is_scaler_mask_intr(hw_ip->base_addr, intr_mask);

		clear_bit(HW_RUN, &hw_ip->state);
	} else {
		dbg_hw("[%d][ID:%d] hw_mcsc already disabled\n", instance, hw_ip->id);
	}

exit:
	return ret;
}

int fimc_is_hw_mcsc_shot(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_frame *frame,
	unsigned long hw_map)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct scp_param *param;
	u32 target_addr[4] = {0};

	BUG_ON(!hw_ip);
	BUG_ON(!frame);

	dbg_hw("[%d][ID:%d][F:%d]\n", frame->instance, hw_ip->id, frame->fcount);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		dbg_hw("[%d][ID:%d] hw_mcsc not initialized\n", frame->instance, hw_ip->id);
		ret = -EINVAL;
		goto exit;
	}

	if (!test_bit(hw_ip->id, &hw_map)) {
		goto exit;
	}
	set_bit(hw_ip->id, &frame->core_flag);

	param = (struct scp_param *)&hw_ip->region[frame->instance]->parameter.scalerp;

	if (frame->type == SHOT_TYPE_INTERNAL) {
		dbg_hw("[%d][ID:%d] request not exist\n", frame->instance, hw_ip->id);
		goto config;
	}

	fimc_is_hw_mcsc_update_param(param, hw_mcsc);
	fimc_is_hw_mcsc_update_register(frame->instance, hw_ip);

	/* set mcsc dma out addr */
	target_addr[0] = frame->shot->uctl.scalerUd.scpTargetAddress[0];
	target_addr[1] = frame->shot->uctl.scalerUd.scpTargetAddress[1];
	target_addr[2] = frame->shot->uctl.scalerUd.scpTargetAddress[2];

#if (defined(DEBUG) && defined(DBG_HW))
	fimc_is_log_write("[%d][MCSC][F:%d] target addr [0x%x]\n", frame->instance, frame->fcount, target_addr[0]);
#endif

config:
	if (param->dma_output.cmd != DMA_OUTPUT_COMMAND_DISABLE
		&& target_addr[0] != 0
		&& frame->type != SHOT_TYPE_INTERNAL) {
		fimc_is_scaler_set_dma_enable(hw_ip->base_addr, true);
		/* use only one buffer (per-frame) */
		fimc_is_scaler_set_dma_out_frame_seq(hw_ip->base_addr, 0x1 << USE_DMA_BUFFER_INDEX);
		fimc_is_scaler_set_dma_out_addr(hw_ip->base_addr,
			target_addr[0], target_addr[1], target_addr[2], USE_DMA_BUFFER_INDEX);
	} else {
		fimc_is_scaler_set_dma_enable(hw_ip->base_addr, false);
		fimc_is_scaler_clear_dma_out_addr(hw_ip->base_addr);
		dbg_hw("[%d][ID:%d] Disable dma out\n", frame->instance, hw_ip->id);
	}
	set_bit(HW_CONFIG, &hw_ip->state);

exit:
	return ret;
}

int fimc_is_hw_mcsc_set_param(struct fimc_is_hw_ip *hw_ip,
	struct is_region *region,
	u32 lindex,
	u32 hindex,
	u32 instance,
	unsigned long hw_map)
{
	int ret = 0;
	struct fimc_is_hw_mcsc	*hw_mcsc;
	struct scp_param	*param;

	BUG_ON(!hw_ip);
	BUG_ON(!region);

	if (!test_bit(hw_ip->id, &hw_map))
		goto exit;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("[%d]not initialized!!", instance);
		ret = -EINVAL;
		goto exit;
	}

	hw_ip->region[instance] = region;
	hw_ip->lindex[instance] = lindex;
	hw_ip->hindex[instance] = hindex;

	param = &region->parameter.scalerp;
	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	fimc_is_hw_mcsc_update_param(param, hw_mcsc);

exit:
	return ret;
}

void fimc_is_hw_mcsc_update_param(struct scp_param *param, struct fimc_is_hw_mcsc *hw_mcsc)
{
	/* TODO : check update index */

	if (param->otf_input.cmd == OTF_INPUT_COMMAND_ENABLE)
		hw_mcsc->param_change_flags[MCSC_PARAM_OTFINPUT_CHANGE] = 1;

	if (param->input_crop.cmd == SCALER_CROP_COMMAND_ENABLE)
		hw_mcsc->param_change_flags[MCSC_PARAM_INPUTCROP_CHANGE] = 1;

	if (param->output_crop.cmd == SCALER_CROP_COMMAND_ENABLE)
		hw_mcsc->param_change_flags[MCSC_PARAM_OUTPUTCROP_CHANGE] = 1;

	if (param->otf_output.cmd == OTF_OUTPUT_COMMAND_ENABLE)
		hw_mcsc->param_change_flags[MCSC_PARAM_OTFOUTPUT_CHANGE] = 1;

	if (param->dma_output.cmd == DMA_OUTPUT_COMMAND_ENABLE)
		hw_mcsc->param_change_flags[MCSC_PARAM_DMAOUTPUT_CHANGE] = 1;

	if (SCALER_FLIP_COMMAND_NORMAL <= param->flip.cmd && param->flip.cmd <= SCALER_FLIP_COMMAND_XY_MIRROR)
		hw_mcsc->param_change_flags[MCSC_PARAM_FLIP_CHANGE] = 1;

	if (param->effect.yuv_range >= SCALER_OUTPUT_YUV_RANGE_FULL)
		hw_mcsc->param_change_flags[MCSC_PARAM_YUVRANGE_CHANGE] = 1;
}

int fimc_is_hw_mcsc_update_register(u32 instance, struct fimc_is_hw_ip *hw_ip)
{
	int ret = 0;
	struct fimc_is_hw_mcsc	*hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	BUG_ON(!hw_ip);

	if (hw_mcsc->param_change_flags[MCSC_PARAM_OTFINPUT_CHANGE] == 1)
		ret = fimc_is_hw_mcsc_otf_input(instance, hw_ip);

	if (hw_mcsc->param_change_flags[MCSC_PARAM_INPUTCROP_CHANGE] == 1)
		ret = fimc_is_hw_mcsc_input_crop(instance, hw_ip);

	if (hw_mcsc->param_change_flags[MCSC_PARAM_OUTPUTCROP_CHANGE] == 1)
		ret = fimc_is_hw_mcsc_output_crop(instance, hw_ip);

	if (hw_mcsc->param_change_flags[MCSC_PARAM_FLIP_CHANGE] == 1)
		ret = fimc_is_hw_mcsc_flip(instance, hw_ip);

	if (hw_mcsc->param_change_flags[MCSC_PARAM_OTFOUTPUT_CHANGE] == 1)
		ret = fimc_is_hw_mcsc_otf_output(instance, hw_ip);

	if (hw_mcsc->param_change_flags[MCSC_PARAM_DMAOUTPUT_CHANGE] == 1)
		ret = fimc_is_hw_mcsc_dma_output(instance, hw_ip);

	if (hw_mcsc->param_change_flags[MCSC_PARAM_YUVRANGE_CHANGE] == 1)
		ret = fimc_is_hw_mcsc_output_yuvrange(instance, hw_ip);

	return ret;
}

int fimc_is_hw_mcsc_reset(struct fimc_is_hw_ip *hw_ip)
{
	int ret = 0;

	BUG_ON(!hw_ip);

	ret = fimc_is_scaler_sw_reset(hw_ip->base_addr);
	if (ret != 0) {
		err_hw("MCSC sw reset fail");
		ret = -ENODEV;
	}

	return ret;
}

int fimc_is_hw_mcsc_load_setfile(struct fimc_is_hw_ip *hw_ip, int index, u32 instance, unsigned long hw_map)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc = NULL;
	u32 setfile_index = 0;

	BUG_ON(!hw_ip);

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("[%d] not initialized!!", instance);
		return -ESRCH;
	}

	if (!unlikely(hw_ip->priv_info)) {
		err_hw("MCSC priv info is NULL");
		return -EINVAL;
	}
	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	switch (hw_ip->setfile_info.version) {
	case SETFILE_V2:
		break;
	case SETFILE_V3:
		break;
	default:
		err_hw("[%d][ID:%d] invalid version (%d)", instance, hw_ip->id, hw_ip->setfile_info.version);
		return -EINVAL;
	}

	setfile_index = hw_ip->setfile_info.index[index];
	hw_mcsc->setfile = (struct hw_api_scaler_setfile *)hw_ip->setfile_info.table[setfile_index].addr;
	if (hw_mcsc->setfile->setfile_version != MCSC_SETFILE_VERSION) {
		err_hw("[%d][ID:%d] setfile version(0x%x) is incorrect\n",
				instance, hw_ip->id, hw_mcsc->setfile->setfile_version);
		return -EINVAL;
	}

	set_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

int fimc_is_hw_mcsc_apply_setfile(struct fimc_is_hw_ip *hw_ip, int index, u32 instance, unsigned long hw_map)
{
	int ret = 0;
	u32 setfile_index = 0;
	struct fimc_is_hw_mcsc *hw_mcsc = NULL;

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("[%d] not initialized!!", instance);
		return -ESRCH;
	}

	if (!unlikely(hw_ip->priv_info)) {
		err_hw("MCSC priv info is NULL");
		return -EINVAL;
	}

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	if (!hw_mcsc->setfile)
		return 0;

	setfile_index = hw_ip->setfile_info.index[index];
	info_hw("[%d][ID:%d] setfile (%d) scenario (%d)\n", instance, hw_ip->id, setfile_index, index);

	return ret;
}

int fimc_is_hw_mcsc_delete_setfile(struct fimc_is_hw_ip *hw_ip, u32 instance, unsigned long hw_map)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc = NULL;

	BUG_ON(!hw_ip);

	if (!test_bit(hw_ip->id, &hw_map))
		return 0;

	if (test_bit(HW_TUNESET, &hw_ip->state)) {
		warn_hw("[%d][ID:%d] setfile already deleted\n", instance, hw_ip->id);
		return 0;
	}

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	hw_mcsc->setfile = NULL;

	return ret;
}

int fimc_is_hw_mcsc_frame_ndone(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame, u32 instance, bool late_flag)
{
	int wq_id;
	int output_id;
	enum fimc_is_frame_done_type done_type;
	int ret = 0;

	if (late_flag == true)
		done_type = FRAME_DONE_LATE_SHOT;
	else
		done_type = FRAME_DONE_FORCE;

	wq_id     = WORK_SCP_FDONE;
	output_id = ENTRY_SCP;
	if (test_bit(output_id, &frame->out_flag))
		ret = fimc_is_hardware_frame_done(hw_ip, frame, wq_id, output_id, 1, done_type);

	wq_id     = -1;
	output_id = FIMC_IS_HW_CORE_END;
	if (test_bit(hw_ip->id, &frame->core_flag))
		ret = fimc_is_hardware_frame_done(hw_ip, frame, wq_id, output_id, 1, done_type);

	return ret;
}

int fimc_is_hw_mcsc_otf_input(u32 instance, struct fimc_is_hw_ip *hw_ip)
{
	int ret = 0;
	u32 width = 0;
	u32 height = 0;
	u32 format = 0;
	u32 bit_width = 0;
	u32 enable = 0;

	struct fimc_is_hw_mcsc *hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	struct scp_param *param = (struct scp_param *)&hw_ip->region[instance]->parameter.scalerp;
	struct param_otf_input *otf_input = &(param->otf_input);

	width = otf_input->width;
	height = otf_input->height;
	format = otf_input->format;
	bit_width = otf_input->bitwidth;

	if (hw_mcsc->param_change_flags[MCSC_PARAM_OTFINPUT_CHANGE] == 1) {
		enable = 1;
		hw_mcsc->param_change_flags[MCSC_PARAM_OTFINPUT_CHANGE] = 0;
	} else {
		enable = 0;
	}

	/* check otf input param */
	if (width < 16 || width > 8192) {
		err_hw("[%d]Invalid MCSC input width(%d)", instance, width);
		goto p_err;
	}

	if (height < 16 || height > 8192) {
		err_hw("[%d]Invalid MCSC input height(%d)", instance, width);
		goto p_err;
	}

	if (format != OTF_INPUT_FORMAT_YUV422) {
		err_hw("[%d]Invalid MCSC input format(%d)", instance, format);
		goto p_err;
	}

	if (bit_width != OTF_INPUT_BIT_WIDTH_12BIT) {
		err_hw("[%d]Invalid MCSC input bit_width(%d)", instance, bit_width);
		goto p_err;
	}

	fimc_is_scaler_set_poly_scaler_enable(hw_ip->base_addr, enable);
	fimc_is_scaler_set_poly_img_size(hw_ip->base_addr, width, height);
	fimc_is_scaler_set_dither(hw_ip->base_addr, enable);

p_err:
	return ret;
}

int fimc_is_hw_mcsc_effect(u32 instance, struct fimc_is_hw_ip *hw_ip)
{
	int ret = 0;

	struct fimc_is_hw_mcsc *hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	hw_mcsc->param_change_flags[MCSC_PARAM_IMAGEEFFECT_CHANGE] = 0;

	return ret;
}

int fimc_is_hw_mcsc_input_crop(u32 instance, struct fimc_is_hw_ip *hw_ip)
{
	int ret = 0;
	u32 img_width, img_height = 0;
	u32 src_pos_x, src_pos_y = 0;
	u32 src_width, src_height = 0;
	u32 out_width, out_height = 0;
	u32 post_img_width, post_img_height = 0;

	struct fimc_is_hw_mcsc *hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	struct scp_param *param = (struct scp_param *)&hw_ip->region[instance]->parameter.scalerp;
	struct param_scaler_input_crop *input_crop = &(param->input_crop);

	img_width = input_crop->in_width;
	img_height = input_crop->in_height;
	src_pos_x = input_crop->pos_x;
	src_pos_y = input_crop->pos_y;
	src_width = input_crop->crop_width;
	src_height = input_crop->crop_height;
	out_width = input_crop->out_width;
	out_height = input_crop->out_height;

	hw_mcsc->param_change_flags[MCSC_PARAM_INPUTCROP_CHANGE] = 0;

	fimc_is_scaler_set_poly_img_size(hw_ip->base_addr, img_width, img_height);
	fimc_is_scaler_set_poly_src_size(hw_ip->base_addr, src_pos_x, src_pos_y, src_width, src_height);

	if (out_width < (src_width / 4)) {
		post_img_width = src_width / 4;
		if (out_height < (src_height / 4))
			post_img_height = src_height / 4;
		else
			post_img_height = out_height;
		fimc_is_scaler_set_post_scaler_enable(hw_ip->base_addr, 1);
	} else {
		post_img_width = out_width;
		if (out_height < (src_height / 4)) {
			post_img_height = src_height / 4;
			fimc_is_scaler_set_post_scaler_enable(hw_ip->base_addr, 1);
		} else {
			post_img_height = out_height;
			fimc_is_scaler_set_post_scaler_enable(hw_ip->base_addr, 0);
		}
	}

	fimc_is_scaler_set_poly_dst_size(hw_ip->base_addr, post_img_width, post_img_height);
	fimc_is_scaler_set_post_img_size(hw_ip->base_addr, post_img_width, post_img_height);

	return ret;
}

int fimc_is_hw_mcsc_output_crop(u32 instance, struct fimc_is_hw_ip *hw_ip)
{
	int ret = 0;
	u32 position_x, position_y = 0;
	u32 crop_width, crop_height = 0;
	u32 format, plane = 0;
	bool conv420_flag = false;
	u32 y_stride, uv_stride = 0;

	struct fimc_is_hw_mcsc *hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	struct scp_param *param = (struct scp_param *)&hw_ip->region[instance]->parameter.scalerp;
	struct param_scaler_output_crop *output_crop = &(param->output_crop);

	position_x = output_crop->pos_x;
	position_y = output_crop->pos_y;
	crop_width = output_crop->crop_width;
	crop_height = output_crop->crop_height;
	format = output_crop->format;
	plane = param->dma_output.plane;

	if ((enum scaler_crop_command)output_crop->cmd != SCALER_CROP_COMMAND_ENABLE) {
		dbg_hw("[%d][ID:%d] Skip output crop\n", instance, hw_ip->id);
		goto exit;
	}

	hw_mcsc->param_change_flags[MCSC_PARAM_OUTPUTCROP_CHANGE] = 0;

	if (format == DMA_OUTPUT_FORMAT_YUV420)
		conv420_flag = true;

	fimc_is_hw_mcsc_adjust_stride(crop_width, plane, conv420_flag, &y_stride, &uv_stride);
	fimc_is_scaler_set_stride_size(hw_ip->base_addr, y_stride, uv_stride);

exit:
	return ret;
}

int fimc_is_hw_mcsc_flip(u32 instance, struct fimc_is_hw_ip *hw_ip)
{
	int ret = 0;

	struct fimc_is_hw_mcsc *hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	struct scp_param *param = (struct scp_param *)&hw_ip->region[instance]->parameter.scalerp;
	struct param_scaler_flip *flip = &(param->flip);

	if (flip->cmd > SCALER_FLIP_COMMAND_XY_MIRROR) {
		err_hw("[%d]Invalid MCSC flip cmd(%d)", instance, flip->cmd);
		goto exit;
	}

	hw_mcsc->param_change_flags[MCSC_PARAM_FLIP_CHANGE] = 0;

	fimc_is_scaler_set_flip_mode(hw_ip->base_addr, flip->cmd);

exit:
	return ret;
}

int fimc_is_hw_mcsc_otf_output(u32 instance, struct fimc_is_hw_ip *hw_ip)
{
	int ret = 0;
	u32 input_width, input_height = 0;
	u32 crop_width, crop_height = 0;
	u32 output_width, output_height = 0;
	u32 dst_width, dst_height = 0;
	u32 format = 0;
	u32 bit_witdh = 0;
	u32 post_hratio, post_vratio = 0;
	u32 hratio, vratio = 0;

	struct fimc_is_hw_mcsc *hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	struct scp_param *param = (struct scp_param *)&hw_ip->region[instance]->parameter.scalerp;
	struct param_otf_output *otf_output = &(param->otf_output);

	dst_width = otf_output->width;
	dst_height = otf_output->height;
	format = otf_output->format;
	bit_witdh = otf_output->bitwidth;

	hw_mcsc->param_change_flags[MCSC_PARAM_OTFOUTPUT_CHANGE] = 0;

	/* check otf output param */
	if (dst_width < 16 || dst_width > 8192) {
		err_hw("[%d]Invalid MCSC output width(%d)", instance, dst_width);
		goto p_err;
	}

	if (dst_height < 16 || dst_height > 8192) {
		err_hw("[%d]Invalid MCSC output height(%d)", instance, dst_height);
		goto p_err;

	}

	if (format != OTF_OUTPUT_FORMAT_YUV422) {
		err_hw("[%d]Invalid MCSC output format(%d)", instance, dst_height);
		goto p_err;
	}

	if (bit_witdh != OTF_OUTPUT_BIT_WIDTH_8BIT) {
		err_hw("[%d]Invalid MCSC output bit width(%d)", instance, dst_height);
		goto p_err;
	}

	fimc_is_scaler_set_post_dst_size(hw_ip->base_addr, dst_width, dst_height);

	if (param->otf_input.cmd == OTF_INPUT_COMMAND_ENABLE) {
		fimc_is_scaler_get_poly_img_size(hw_ip->base_addr, &input_width, &input_height);
		fimc_is_scaler_get_poly_src_size(hw_ip->base_addr, &crop_width, &crop_height);
		fimc_is_scaler_get_post_img_size(hw_ip->base_addr, &output_width, &output_height);

		hratio = (crop_width << 20) / output_width;
		vratio = (crop_height << 20) / output_height;

		post_hratio = (output_width << 20) / dst_width;
		post_vratio = (output_height << 20) / dst_height;

		fimc_is_scaler_set_poly_scaling_ratio(hw_ip->base_addr, hratio, vratio);
		fimc_is_scaler_set_post_scaling_ratio(hw_ip->base_addr, post_hratio, post_vratio);
		fimc_is_scaler_set_poly_scaler_coef(hw_ip->base_addr, hratio, vratio);
	}

	if ((enum otf_output_command)otf_output->cmd == OTF_OUTPUT_COMMAND_ENABLE) {
		/* set direct out path : DIRECT_FORMAT : 00 :  CbCr first */
		fimc_is_scaler_set_direct_out_path(hw_ip->base_addr, 0, 1);
	} else {
		fimc_is_scaler_set_direct_out_path(hw_ip->base_addr, 0, 0);
	}

p_err:
	return ret;
}

int fimc_is_hw_mcsc_dma_output(u32 instance, struct fimc_is_hw_ip *hw_ip)
{
	int ret = 0;
	u32 input_width = 0, input_height = 0;
	u32 crop_width = 0, crop_height = 0;
	u32 output_width = 0, output_height = 0;
	u32 scaled_width = 0, scaled_height = 0;
	u32 dst_width = 0, dst_height = 0;
	u32 post_hratio = 0, post_vratio = 0;
	u32 hratio = 0, vratio = 0;
	u32 format, plane, order = 0;
	bool conv420_en = false;
	u32 y_stride = 0, uv_stride = 0;
	u32 img_format;

	struct fimc_is_hw_mcsc *hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	struct scp_param *param = (struct scp_param *)&hw_ip->region[instance]->parameter.scalerp;
	struct param_dma_output *dma_output = &(param->dma_output);

	dst_width = dma_output->width;
	dst_height = dma_output->height;
	format = dma_output->format;
	plane = dma_output->plane;
	order = dma_output->order;

	hw_mcsc->param_change_flags[MCSC_PARAM_DMAOUTPUT_CHANGE] = 0;

	if ((enum dma_output_command)dma_output->cmd == DMA_OUTPUT_UPDATE_MASK_BITS) {
		/* TODO : set DMA output frame buffer sequence */

	} else if (dma_output->cmd != DMA_OUTPUT_COMMAND_ENABLE) {
		dbg_hw("[%d][ID:%d] Skip dma out\n", instance, hw_ip->id);
		fimc_is_scaler_set_dma_out_path(hw_ip->base_addr, 0, 0);
		goto p_err;
	} else if (dma_output->cmd == DMA_OUTPUT_COMMAND_ENABLE) {
		ret = fimc_is_hw_mcsc_adjust_img_fmt(format, plane, order, &img_format, &conv420_en);
		if (ret < 0) {
			warn_hw("[%d][ID:%d] Invalid dma image format\n", instance, hw_ip->id);
			img_format = hw_mcsc->img_format;
			conv420_en = hw_mcsc->conv420_en;
		} else {
			hw_mcsc->img_format = img_format;
			hw_mcsc->conv420_en = conv420_en;
		}

		fimc_is_scaler_set_dma_out_path(hw_ip->base_addr, img_format, 1);
		fimc_is_scaler_set_420_conversion(hw_ip->base_addr, 0, conv420_en);

		if (fimc_is_scaler_get_otf_out_enable(hw_ip->base_addr)) {
			fimc_is_scaler_get_post_dst_size(hw_ip->base_addr, &scaled_width, &scaled_height);

			if ((scaled_width != dst_width) || (scaled_height != dst_height)) {
				dbg_hw("[%d][ID:%d] Invalid scaled size (%d/%d)(%d/%d)\n",
					instance, hw_ip->id, scaled_width, scaled_height, dst_width, dst_height);
				goto p_err;
			}
		} else {
			fimc_is_scaler_get_poly_src_size(hw_ip->base_addr, &crop_width, &crop_height);
			fimc_is_scaler_get_post_img_size(hw_ip->base_addr, &output_width, &output_height);

			hratio = (crop_width << 20) / output_width;
			vratio = (crop_height << 20) / output_height;

			post_hratio = (output_width << 20) / dst_width;
			post_vratio = (output_height << 20) / dst_height;

			fimc_is_scaler_set_poly_scaling_ratio(hw_ip->base_addr, hratio, vratio);
			fimc_is_scaler_set_post_scaling_ratio(hw_ip->base_addr, post_hratio, post_vratio);
			fimc_is_scaler_set_poly_scaler_coef(hw_ip->base_addr, hratio, vratio);
		}

		if ((enum scaler_crop_command)param->output_crop.cmd != SCALER_CROP_COMMAND_ENABLE) {
			if (dma_output->reserved[0] == SCALER_DMA_OUT_UNSCALED) {
				fimc_is_scaler_get_poly_img_size(hw_ip->base_addr, &input_width, &input_height);
				fimc_is_hw_mcsc_adjust_stride(input_width, plane, conv420_en, &y_stride, &uv_stride);
				fimc_is_scaler_set_stride_size(hw_ip->base_addr, y_stride, uv_stride);
			} else {
				fimc_is_hw_mcsc_adjust_stride(dst_width, plane, conv420_en, &y_stride, &uv_stride);
				fimc_is_scaler_set_stride_size(hw_ip->base_addr, y_stride, uv_stride);
			}
		}
	}

	fimc_is_scaler_set_dma_size(hw_ip->base_addr, dst_width, dst_height);

p_err:
	return ret;
}

int fimc_is_hw_mcsc_output_yuvrange(u32 instance, struct fimc_is_hw_ip *hw_ip)
{
	int ret = 0;
	int yuv_range = 0;
	struct fimc_is_hw_mcsc *hw_mcsc = NULL;
	scaler_setfile_contents contents;

	struct scp_param *param = (struct scp_param *)&hw_ip->region[instance]->parameter.scalerp;
	struct param_scaler_imageeffect *image_effect = &(param->effect);

	yuv_range = image_effect->yuv_range;

	fimc_is_scaler_set_bchs_enable(hw_ip->base_addr, 1);

	if (test_bit(HW_TUNESET, &hw_ip->state)) {
		hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

		/* set yuv range config value by scaler_param yuv_range mode */
		contents = hw_mcsc->setfile->contents[yuv_range];

		fimc_is_scaler_set_b_c(hw_ip->base_addr, contents.y_offset, contents.y_gain);
		fimc_is_scaler_set_h_s(hw_ip->base_addr,
			contents.c_gain00, contents.c_gain01, contents.c_gain10, contents.c_gain11);
		dbg_hw("[%d][ID:%d] set YUV range(%d) by setfile parameter\n", instance, hw_ip->id, yuv_range);
	} else {
		if (yuv_range == SCALER_OUTPUT_YUV_RANGE_FULL) {
			/* Y range - [0:255], U/V range - [0:255] */
			fimc_is_scaler_set_b_c(hw_ip->base_addr, 0, 256);
			fimc_is_scaler_set_h_s(hw_ip->base_addr, 256, 0, 0, 256);
		} else if (yuv_range == SCALER_OUTPUT_YUV_RANGE_NARROW) {
			/* Y range - [16:235], U/V range - [16:239] */
			fimc_is_scaler_set_b_c(hw_ip->base_addr, 16, 220);
			fimc_is_scaler_set_h_s(hw_ip->base_addr, 224, 0, 0, 224);
		}

		dbg_hw("[%d][ID:%d] YUV range set default settings\n", instance, hw_ip->id);
	}
	return ret;
}

int fimc_is_hw_mcsc_adjust_img_fmt(u32 format, u32 plane, u32 order, u32 *img_format, bool *conv420_flag)
{
	int ret = 0;

	switch (format) {
	case DMA_OUTPUT_FORMAT_YUV420:
		*conv420_flag = true;
		switch (plane) {
		case 2:
			switch (order) {
			case DMA_OUTPUT_ORDER_CbCr:
				*img_format = MCSC_YUV420_2P_UFIRST;
				break;
			case DMA_OUTPUT_ORDER_CrCb:
				* img_format = MCSC_YUV420_2P_VFIRST;
				break;
			default:
				err_hw("output order error - (%d/%d/%d)", format, order, plane);
				ret = -EINVAL;
				break;
			}
			break;
		case 3:
			*img_format = MCSC_YUV420_3P;
			break;
		default:
			err_hw("output plane error - (%d/%d/%d)", format, order, plane);
			ret = -EINVAL;
			break;
		}
		break;
	case DMA_OUTPUT_FORMAT_YUV422:
		*conv420_flag = false;
		switch (plane) {
		case 1:
			switch (order) {
			case DMA_OUTPUT_ORDER_CrYCbY:
				*img_format = MCSC_YUV422_1P_VYUY;
				break;
			case DMA_OUTPUT_ORDER_CbYCrY:
				*img_format = MCSC_YUV422_1P_UYVY;
				break;
			case DMA_OUTPUT_ORDER_YCrYCb:
				*img_format = MCSC_YUV422_1P_YVYU;
				break;
			case DMA_OUTPUT_ORDER_YCbYCr:
				*img_format = MCSC_YUV422_1P_YUYV;
				break;
			default:
				err_hw("output order error - (%d/%d/%d)", format, order, plane);
				ret = -EINVAL;
				break;
			}
			break;
		case 2:
			switch (order) {
			case DMA_OUTPUT_ORDER_CbCr:
				*img_format = MCSC_YUV422_2P_UFIRST;
				break;
			case DMA_OUTPUT_ORDER_CrCb:
				*img_format = MCSC_YUV422_2P_VFIRST;
				break;
			default:
				err_hw("output order error - (%d/%d/%d)", format, order, plane);
				ret = -EINVAL;
				break;
			}
			break;
		case 3:
			*img_format = MCSC_YUV422_3P;
			break;
		default:
			err_hw("output plane error - (%d/%d/%d)", format, order, plane);
			ret = -EINVAL;
			break;
		}
		break;
	default:
		err_hw("output format error - (%d/%d/%d)", format, order, plane);
		ret = -EINVAL;
		break;
	}

	return ret;
}

void fimc_is_hw_mcsc_adjust_stride(u32 width, u32 plane, bool conv420_flag, u32 *y_stride, u32 *uv_stride)
{
	if ((conv420_flag == false) && (plane == 1)) {
		*y_stride = width * 2;
		*uv_stride = width;
	} else {
		*y_stride = width;
		if (plane == 3)
			*uv_stride = width / 2;
		else
			*uv_stride = width;
	}

	*y_stride = 2 * ((*y_stride / 2) + ((*y_stride % 2) > 0));
	*uv_stride = 2 * ((*uv_stride / 2) + ((*uv_stride % 2) > 0));
}

void fimc_is_hw_mcsc_size_dump(struct fimc_is_hw_ip *hw_ip)
{
	u32 otf_path, dma_path = 0;
	u32 poly_in_w, poly_in_h = 0;
	u32 poly_crop_w, poly_crop_h = 0;
	u32 poly_out_w, poly_out_h = 0;
	u32 post_in_w, post_in_h = 0;
	u32 post_out_w, post_out_h = 0;
	u32 dma_y_stride, dma_uv_stride = 0;

	otf_path = fimc_is_scaler_get_otf_out_enable(hw_ip->base_addr);
	dma_path = fimc_is_scaler_get_dma_enable(hw_ip->base_addr);

	fimc_is_scaler_get_poly_img_size(hw_ip->base_addr, &poly_in_w, &poly_in_h);
	fimc_is_scaler_get_poly_src_size(hw_ip->base_addr, &poly_crop_w, &poly_crop_h);
	fimc_is_scaler_get_poly_dst_size(hw_ip->base_addr, &poly_out_w, &poly_out_h);
	fimc_is_scaler_get_post_img_size(hw_ip->base_addr, &post_in_w, &post_in_h);
	fimc_is_scaler_get_post_dst_size(hw_ip->base_addr, &post_out_w, &post_out_h);
	fimc_is_scaler_get_stride_size(hw_ip->base_addr, &dma_y_stride, &dma_uv_stride);

	info_hw("[MCSC]=SIZE=====================================\n");
	info_hw("[OTF:%d], [DMA:%d]\n", otf_path, dma_path);
	info_hw("[POLY] IN:%dx%d, CROP:%dx%d, OUT:%dx%d\n",
		poly_in_w, poly_in_h, poly_crop_w, poly_crop_h, poly_out_w, poly_out_h);
	info_hw("[POST] IN:%dx%d, OUT:%dx%d\n", post_in_w, post_in_h, post_out_w, post_out_h);
	info_hw("[DMA_STRIDE] Y:%d, UV:%d\n", dma_y_stride, dma_uv_stride);
	info_hw("[MCSC]==========================================\n");

	return;
}

/*
void fimc_is_hw_scp_set_dma_mo8_sel(u32 base_addr, u32 width, u32 hoffset, u32 plane)
{
	bool dmaMo8Flag = false;
	u32 i = 0;
	u32 temp = 0;

	if (plane == 3) {
		for (i = 8; i >= 1; i--) {
			temp = (width + hoffset - (2 * i)) % 16;
			if (temp != 0)
				continue;

			temp = ((width + hoffset - (2 * i)) / 16) & 2;
			if (temp != 0) {
				dmaMo8Flag = true;
				break;
			}
		}
	} else {
		dmaMo8Flag = true;
	}

	if (dmaMo8Flag == true)
		fimc_is_scaler_set_dma_mo8_sel(base_addr, 0);
	else
		fimc_is_scaler_set_dma_mo8_sel(base_addr, 1);
}
*/
