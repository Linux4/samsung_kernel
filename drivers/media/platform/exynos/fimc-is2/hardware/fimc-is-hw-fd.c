/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-hw-fd.h"
#include "../interface/fimc-is-interface-ischain.h"

int fimc_is_hw_fd_shadow_trigger(struct fimc_is_hw_ip *hw_ip,
				struct fimc_is_frame *frame)
{
	int ret = 0;
	int hw_slot;
	struct fimc_is_hw_ip *hw_ip_fd = NULL, *hw_ip_scaler = NULL;
	struct fimc_is_hardware *hardware = NULL;
	struct fimc_is_hw_fd *hw_fd = NULL;

	BUG_ON(!hw_ip);
	BUG_ON(!frame);

	if (!unlikely(hw_ip->hardware)) {
		err_hw("[ID:%d]hardware(null)!!", hw_ip->id);
		ret = -EINVAL;
		goto exit;
	}
	hardware = hw_ip->hardware;

	hw_slot = fimc_is_hw_slot_id(DEV_HW_FD);
	if (!valid_hw_slot_id(hw_slot)) {
		err_hw("invalid slot (%d,%d)", DEV_HW_FD, hw_slot);
		ret = -EINVAL;
		goto exit;
	}
	hw_ip_fd = &hardware->hw_ip[hw_slot];

	if (!unlikely(hw_ip_fd->priv_info)) {
		err_hw("[ID:%d] FD priv_info(null)!!", hw_ip_fd->id);
		ret = -ENOSPC;
		goto exit;
	}
	hw_fd = (struct fimc_is_hw_fd *)hw_ip_fd->priv_info;

	hw_slot = fimc_is_hw_slot_id(DEV_HW_MCSC);
	if (!valid_hw_slot_id(hw_slot)) {
		err_hw("invalid slot (%d,%d)", DEV_HW_MCSC, hw_slot);
		ret = -EINVAL;
		goto exit;
	}
	hw_ip_scaler = &hardware->hw_ip[hw_slot];

	if ((hw_fd->shadow_sw.cur_cmd == CONTROL_COMMAND_START) &&
			(hw_fd->shadow_sw.next_cmd == CONTROL_COMMAND_STOP)) {
		if (hw_ip->id != hw_ip_fd->id)
			goto exit;

		if (atomic_read(&hw_ip_fd->Vvalid)) {
			err_hw("[FD] Vvalid is not available region in FD");
			goto exit;
		}

		fimc_is_fd_enable(hw_ip->base_addr, false);
	} else if ((hw_fd->shadow_sw.cur_cmd == CONTROL_COMMAND_STOP) &&
			(hw_fd->shadow_sw.next_cmd == CONTROL_COMMAND_START)) {
		if (hw_ip->id != hw_ip_scaler->id)
			goto exit;

		if (atomic_read(&hw_ip_scaler->Vvalid)) {
			err_hw("[FD] Vvalid is not available region in scaler");
			goto exit;
		}

		fimc_is_fd_shadow_sw(hw_ip_fd->base_addr, true);
		fimc_is_fd_enable(hw_ip_fd->base_addr, true);
	}

exit:
	return ret;
}

int fimc_is_hw_fd_otf_input(void __iomem *base_addr, struct vra_param *param)
{
	int ret = 0;
	u32 start_x = 0, start_y = 0;
	u32 crop_width = 0, crop_height = 0, crop_size = 0;
	u32 out_width = 0, out_height = 0, out_size = 0;
	u32 scale_x = 0, scale_y= 0, remainder_x = 0, remainder_y = 0;
	u32 ycc_format = 0;
	struct param_otf_input otf_input;
	struct param_fd_config config;

	BUG_ON(!base_addr);
	BUG_ON(!param);

	otf_input = param->otf_input;
	config = param->config;

	/* crop_size = input size */
	crop_width = otf_input.width - start_x;
	crop_height = otf_input.height - start_y;
	crop_size = crop_width * crop_height;

	/* ToDo: Add size error handle */
	out_width = config.map_width;
	out_height = config.map_height;
	out_size = out_width * out_height;

	if ((out_width < 32) || (out_width > 640) || (out_height < 16) || (out_height > 480)) {
		err_hw("OTF output size error. out_width:%d, out_height:%d\n", out_width, out_height);
		ret = -EINVAL;
		goto exit;
	}

	/* down-sampling x scaling */
	scale_x = (((crop_width - 1) * 256) / (out_width - 1));
	remainder_x = ((crop_width - 1) * 256) % (out_width - 1);
	if ((remainder_x == 0) & (scale_x  > 256))
		scale_x -= 1;

	/* down-sampling y scaling */
	scale_y = ((crop_height - 1) * 256) / (out_height - 1);
	remainder_y = ((crop_height - 1) * 256) % (out_height - 1);
	if ((remainder_y == 0) & (scale_y > 256))
		scale_y -= 1;

	/* LHFD max input size: 32 * 16 ~ 8190 * 8190 */
	if ((crop_width < 32) || (crop_width > 8190) || (scale_x < 256) || (scale_x > 4095)) {
		err_hw("OTF input widtrh error. crop_width:%d, scale_x:%d\n", crop_width, scale_x);
		ret = -EINVAL;
		goto exit;
	}
	if ((crop_height < 16) || (crop_height > 8190) || (scale_y < 256) || (scale_y > 4095)) {
		err_hw("OTF input height error. crop_height:%d, scale_y:%d\n", crop_height, scale_y);
		ret = -EINVAL;
		goto exit;
	}

	/* OTF format value = YUV444 */
	if (otf_input.format == OTF_INPUT_FORMAT_YUV444)
		ycc_format = 2;
	else if (otf_input.format == OTF_INPUT_FORMAT_YUV422)
		ycc_format = 0;
	else if (otf_input.format == OTF_INPUT_FORMAT_YUV420)
		ycc_format = 1;
	else {
		err_hw("not support LHFD format!!");
		ret = -EINVAL;
		goto exit;
	}

	if (otf_input.bitwidth != OTF_INPUT_BIT_WIDTH_8BIT) {
		err_hw("not support LHFD bit width!!");
		ret = -EINVAL;
		goto exit;
	}

	/* OTF mode = FD_PREVIEW */
	fimc_is_fd_input_mode(base_addr, FD_FORMAT_PREVIEW_MODE);
	fimc_is_fd_set_input_point(base_addr, start_x, start_y);
	fimc_is_fd_set_input_size(base_addr, crop_width, crop_height, crop_size);
	fimc_is_fd_set_down_sampling(base_addr, scale_x, scale_y);
	fimc_is_fd_set_output_size(base_addr, out_width, out_height, out_size);
	fimc_is_fd_ycc_format(base_addr, ycc_format);
	fimc_is_fd_set_cbcr_align(base_addr, FD_FORMAT_FIRST_CBCR);

exit:
	return ret;
}

int fimc_is_hw_fd_dma_input(void __iomem *base_addr, struct vra_param *param)
{
	int ret = 0;

	/* ToDo */

	return ret;
}

void fimc_is_hw_fd_set_map(void __iomem *base_addr, struct camera2_fd_uctl *uctl)
{
	u32 up = 0, coef_k = 0, shift = 0;
	u8 *ymap = NULL;
	struct hw_api_fd_map out_map;

	BUG_ON(!base_addr);
	BUG_ON(!uctl);

	out_map.dvaddr_0	= uctl->vendorSpecific[0];
	out_map.dvaddr_1	= uctl->vendorSpecific[1];
	out_map.dvaddr_2	= uctl->vendorSpecific[2];
	out_map.dvaddr_3	= uctl->vendorSpecific[3];
	out_map.dvaddr_4	= uctl->vendorSpecific[4];
	out_map.dvaddr_5	= uctl->vendorSpecific[5];
	out_map.dvaddr_6	= uctl->vendorSpecific[6];
	out_map.dvaddr_7	= uctl->vendorSpecific[7];
	ymap			= (u8 *)uctl->vendorSpecific[8];
	coef_k			= uctl->vendorSpecific[9];
	up			= uctl->vendorSpecific[10];
	shift			= uctl->vendorSpecific[11];

	fimc_is_fd_set_map_addr(base_addr, &out_map);
	fimc_is_fd_set_coefk(base_addr, coef_k);
	fimc_is_fd_set_shift(base_addr, shift);

	if (up)
		fimc_is_fd_set_ymap_addr(base_addr, ymap);
}


static int fimc_is_hw_fd_handle_interrupt(void *data)
{
	int ret = 0;
	u32 intr_mask = 0, intr_status = 0, intr_result = 0, overflow = 0;
	struct fimc_is_hw_ip *hw_ip = NULL;

	BUG_ON(!data);

	hw_ip = (struct fimc_is_hw_ip *)data;

	/* read interrupt status */
	intr_mask = fimc_is_fd_get_intr_mask(hw_ip->base_addr);
	intr_status = fimc_is_fd_get_intr_status(hw_ip->base_addr);
	intr_result = (intr_mask) & intr_status;

	if (intr_result & (1 << INTR_FD_END_PROCESSING)) {
		hw_ip->fe_time = cpu_clock(raw_smp_processor_id());
		if (!atomic_read(&hw_ip->hardware->stream_on))
			info_hw("[ID:%d][F:%d]F.E\n", hw_ip->id, hw_ip->fcount);

		atomic_set(&hw_ip->Vvalid, V_BLANK);
		if (atomic_read(&hw_ip->fs_count) == atomic_read(&hw_ip->fe_count)) {
			err_hw("[FD] fs(%d), fe(%d), cl(%d)\n",
				atomic_read(&hw_ip->fs_count),
				atomic_read(&hw_ip->fe_count),
				atomic_read(&hw_ip->cl_count));
		}
		atomic_inc(&hw_ip->fe_count);
		fimc_is_fd_generate_end_intr(hw_ip->base_addr, false);
		fimc_is_hardware_frame_done(hw_ip, NULL, -1, FIMC_IS_HW_CORE_END, 0, FRAME_DONE_NORMAL);
	}

	if (intr_result & (1 << INTR_FD_AXI_ERROR)) {
		err_hw("This device status is INTR_FD_AXI_ERROR~!!\n");
		ret = -EINTR;
	}

	if (intr_result & (1 << INTR_FD_BUFFER_OVERFLOW)) {
		overflow = fimc_is_fd_get_buffer_status(hw_ip->base_addr);
		err_hw("This device status is INTR_FD_BUFFER_OVERFLOW(%d)!!\n", overflow);
		ret = -EINTR;
	}

	if (intr_result & (1 << INTR_FD_SW_RESET_COMPLETED)) {
		err_hw("This device status is INTR_FD_SW_RESET_COMPLETED!!\n");
		ret = -EINTR;
	}

	if (intr_result & (1 << INTR_FD_START_PROCESSING)) {
		hw_ip->fs_time = cpu_clock(raw_smp_processor_id());
		atomic_inc(&hw_ip->fs_count);
		if (!atomic_read(&hw_ip->hardware->stream_on))
			info_hw("[ID:%d][F:%d]F.S\n", hw_ip->id, hw_ip->fcount);

		atomic_set(&hw_ip->Vvalid, V_VALID);
		fimc_is_fd_shadow_sw(hw_ip->base_addr, true);
		fimc_is_fd_generate_end_intr(hw_ip->base_addr, true);
		clear_bit(HW_CONFIG, &hw_ip->state);
	}

	fimc_is_fd_clear_intr(hw_ip->base_addr, intr_result);

	return ret;
}

const struct fimc_is_hw_ip_ops fimc_is_hw_fd_ops = {
	.open			= fimc_is_hw_fd_open,
	.init			= fimc_is_hw_fd_init,
	.close			= fimc_is_hw_fd_close,
	.enable			= fimc_is_hw_fd_enable,
	.disable		= fimc_is_hw_fd_disable,
	.shot			= fimc_is_hw_fd_shot,
	.set_param		= fimc_is_hw_fd_set_param,
	.frame_ndone		= fimc_is_hw_fd_frame_ndone,
	.load_setfile		= fimc_is_hw_fd_load_setfile,
	.apply_setfile		= fimc_is_hw_fd_apply_setfile,
	.delete_setfile		= fimc_is_hw_fd_delete_setfile
};

int fimc_is_hw_fd_probe(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_interface *itf,
	struct fimc_is_interface_ischain *itfc,
	int id)
{
	int ret = 0;
	int intr_slot;

	BUG_ON(!hw_ip);
	BUG_ON(!itf);
	BUG_ON(!itfc);

	dbg_hw("[ID:%d]%s\n", id, __func__);

	/* initialize device hardware */
	hw_ip->id   = id;
	hw_ip->ops  = &fimc_is_hw_fd_ops;
	hw_ip->itf  = itf;
	hw_ip->itfc = itfc;
	hw_ip->fcount = 0;
	hw_ip->internal_fcount = 0;
	hw_ip->is_leader = false;
	atomic_set(&hw_ip->Vvalid, 0);
	atomic_set(&hw_ip->ref_count, 0);
	init_waitqueue_head(&hw_ip->wait_queue);

	/* set fd sfr base address */
	intr_slot = fimc_is_intr_slot_id(id);
	if (!valid_subip_intr_slot_id(intr_slot)) {
		err_hw("invalid intr_slot (%d,%d)", id, intr_slot);
		goto exit;
	}
	hw_ip->base_addr = itfc->subip[intr_slot].regs;

	/* set fd interrupt handler */
	itfc->subip[intr_slot].handler = fimc_is_hw_fd_handle_interrupt;

	clear_bit(HW_OPEN, &hw_ip->state);
	clear_bit(HW_INIT, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);
	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_TUNESET, &hw_ip->state);

exit:
	return ret;
}

int fimc_is_hw_fd_open(struct fimc_is_hw_ip *hw_ip, u32 instance, u32 *size)
{
	int ret = 0;

	BUG_ON(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		goto exit;

	*size = sizeof(struct fimc_is_hw_fd);

exit:
	return ret;
}

int fimc_is_hw_fd_init(struct fimc_is_hw_ip *hw_ip, struct fimc_is_group *group,
		bool flag, u32 module_id)
{
	int ret = 0;
	u32 instance = 0;
	struct fimc_is_hw_fd *hw_fd = NULL;

	BUG_ON(!hw_ip);
	BUG_ON(!group);

	hw_fd = (struct fimc_is_hw_fd *)hw_ip->priv_info;
	instance = group->instance;
	hw_ip->group[instance] = group;

	hw_fd->setfile = NULL;
	hw_fd->mode = FD_FORMAT_MODE_END;
	hw_fd->shadow_sw.first_trigger = false;
	hw_fd->shadow_sw.cur_cmd  = CONTROL_COMMAND_STOP;
	hw_fd->shadow_sw.next_cmd = CONTROL_COMMAND_STOP;

	if (test_bit(HW_INIT, &hw_ip->state)) {
		dbg_hw("[%d]already init!! (%d)", instance, hw_ip->id);
		goto exit;
	}

	ret = fimc_is_fd_sw_reset(hw_ip->base_addr);
	if (ret) {
		err_hw("fd sw reset fail\n");
		ret = -ENODEV;
		goto exit;
	}

	info_hw("[%d]init (%d): LHFD ver = %#x\n",
		instance, hw_ip->id, fimc_is_fd_get_version(hw_ip->base_addr));
exit:
	return ret;
}

int fimc_is_hw_fd_close(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;

	BUG_ON(!hw_ip);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		goto exit;

	info_hw("[%d]close (%d)(%d)\n", instance, hw_ip->id, atomic_read(&hw_ip->ref_count));

exit:
	return ret;
}

int fimc_is_hw_fd_enable(struct fimc_is_hw_ip *hw_ip, u32 instance, unsigned long hw_map)
{
	int ret = 0;
	u32 intr_all = 0;

	BUG_ON(!hw_ip);

	if (!test_bit(hw_ip->id, &hw_map))
		goto exit;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("[%d]not initialized~!! (%d)", instance, hw_ip->id);
		ret = -EINVAL;
		goto exit;
	}

	intr_all = ((1 << INTR_FD_MAX_MAP) -1);
	fimc_is_fd_clear_intr(hw_ip->base_addr, intr_all);

	set_bit(HW_RUN, &hw_ip->state);

exit:
	return ret;
}

int fimc_is_hw_fd_disable(struct fimc_is_hw_ip *hw_ip, u32 instance, unsigned long hw_map)
{
	int ret = 0;

	BUG_ON(!hw_ip);

	if (!test_bit(hw_ip->id, &hw_map))
		goto exit;

	if (!test_bit(HW_RUN, &hw_ip->state))
		goto exit;
	else
		dbg_hw("[%d]already disabled (%d)\n", instance, hw_ip->id);

	fimc_is_fd_enable(hw_ip->base_addr, false);
	fimc_is_fd_generate_end_intr(hw_ip->base_addr, false);
	fimc_is_fd_sw_reset(hw_ip->base_addr);

	clear_bit(HW_RUN, &hw_ip->state);

exit:
	return ret;
}

int fimc_is_hw_fd_shot(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_frame *frame,
	unsigned long hw_map)
{
	int ret = 0;
	enum control_command cmd = CONTROL_COMMAND_STOP;
	struct vra_param *param = NULL;
	struct camera2_fd_uctl *uctl = NULL;
	struct fimc_is_hw_fd *hw_fd = NULL;
	struct fimc_is_group *parent;

	BUG_ON(!hw_ip);
	BUG_ON(!frame);

	dbg_hw("[%d][ID:%d][F:%d]\n", frame->instance, hw_ip->id, frame->fcount);

	if (!test_bit(hw_ip->id, &hw_map))
		goto exit;

	if (!unlikely(hw_ip->priv_info)) {
		err_hw("FD priv info is NULL");
		ret = -EINVAL;
		goto exit;
	}
	hw_fd = (struct fimc_is_hw_fd*)hw_ip->priv_info;

	if (frame->type == SHOT_TYPE_INTERNAL) {
		dbg_hw("[%d][ID:%d] request not exist\n", frame->instance, hw_ip->id);
		cmd = CONTROL_COMMAND_STOP;
		goto internal;
	}

	if (unlikely(!frame->shot_ext)) {
		err_hw("frame->shot_ext is NULL!!");
		ret = -EINVAL;
		goto check_trigger;
	}

	if (unlikely(!frame->shot)) {
		err_hw("frame->shot is NULL!!");
		ret = -EINVAL;
		goto check_trigger;
	}

	if (unlikely(!hw_ip->region)) {
		err_hw("hw_ip->reigion is NULL!!");
		ret = -EINVAL;
		goto check_trigger;
	}
	param = (struct vra_param *)&hw_ip->region[frame->instance]->parameter.vra;

	/* ToDo: Runtime on/off */
	if (param->control.cmd == CONTROL_COMMAND_STOP) {
		dbg_hw("FD CONTROL_COMMAND_STOP.\n");
		cmd = CONTROL_COMMAND_STOP;
		goto check_trigger;
	}

	fimc_is_fd_shadow_sw(hw_ip->base_addr, false);

	/* ToDo: Add DMA input control */
	if (param->otf_input.cmd == OTF_INPUT_COMMAND_ENABLE) {
		ret = fimc_is_hw_fd_otf_input(hw_ip->base_addr, param);
		if (ret) {
			err_hw("set param fail!!");
			ret = -EINVAL;
			goto check_trigger;
		}
	} else {
		err_hw("set param fail!!");
		ret = -EINVAL;
		goto check_trigger;
	}

	cmd = CONTROL_COMMAND_START;

	uctl = &frame->shot->uctl.fdUd;
	/* ToDo: Add error handler for outmap */
	fimc_is_hw_fd_set_map(hw_ip->base_addr, uctl);

	/* ToDo: Move meta update */
	frame->shot->udm.fd.vendorSpecific[0] = fimc_is_fd_get_sat(hw_ip->base_addr);

	set_bit(hw_ip->id, &frame->core_flag);

internal:
	set_bit(HW_CONFIG, &hw_ip->state);

check_trigger:
	parent = hw_ip->group[frame->instance];
	while (parent->parent)
		parent = parent->parent;

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &parent->state)) {
		if (!hw_fd->shadow_sw.first_trigger) {
			hw_fd->shadow_sw.cur_cmd = cmd;
			hw_fd->shadow_sw.next_cmd = cmd;
			hw_fd->shadow_sw.first_trigger= true;

			if (cmd == CONTROL_COMMAND_START) {
				fimc_is_fd_shadow_sw(hw_ip->base_addr, true);
				fimc_is_fd_enable(hw_ip->base_addr, true);
				set_bit(hw_ip->id, &frame->core_flag);
			}
		} else {
			hw_fd->shadow_sw.cur_cmd = hw_fd->shadow_sw.next_cmd;
			hw_fd->shadow_sw.next_cmd = cmd;
		}
	} else {
		if (cmd == CONTROL_COMMAND_START) {
			fimc_is_fd_shadow_sw(hw_ip->base_addr, true);
			fimc_is_fd_enable(hw_ip->base_addr, true);
		} else {
			fimc_is_fd_enable(hw_ip->base_addr, false);
		}
	}
exit:
	return ret;
}

int fimc_is_hw_fd_set_param(struct fimc_is_hw_ip *hw_ip,
	struct is_region *region,
	u32 lindex,
	u32 hindex,
	u32 instance,
	unsigned long hw_map)
{
	int ret = 0;
	struct vra_param *param = NULL;
	struct fimc_is_hw_fd *hw_fd = NULL;
	u32 intr_all = 0, intr_mask = 0;

	BUG_ON(!hw_ip);
	BUG_ON(!region);

	if (!test_bit(hw_ip->id, &hw_map))
		goto exit;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("[%d] not initialized!! (%d)", instance, hw_ip->id);
		ret = -EINVAL;
		goto exit;
	}

	if (!unlikely(hw_ip->priv_info)) {
		err_hw("FD private data(null) (%d)", hw_ip->id);
		ret = -EINVAL;
		goto exit;
	}
	hw_fd = (struct fimc_is_hw_fd *)hw_ip->priv_info;

	hw_ip->region[instance] = region;
	hw_ip->lindex[instance] = lindex;
	hw_ip->hindex[instance] = hindex;

	hw_fd = (struct fimc_is_hw_fd*)hw_ip->priv_info;
	hw_fd->shadow_sw.first_trigger = false;

	fimc_is_fd_config_by_setfile(hw_ip->base_addr, NULL);

	intr_all = ((1 << INTR_FD_MAX_MAP) -1);
	fimc_is_fd_clear_intr(hw_ip->base_addr, intr_all);

	intr_mask = ((1 << INTR_FD_END_PROCESSING) | (1 << INTR_FD_BUFFER_OVERFLOW)
			| (1 << INTR_FD_AXI_ERROR) | (1 << INTR_FD_START_PROCESSING));
	fimc_is_fd_enable_intr(hw_ip->base_addr, intr_mask);

	param = &region->parameter.vra;

	if (param->otf_input.cmd == OTF_INPUT_COMMAND_ENABLE) {
		hw_fd->mode = FD_FORMAT_PREVIEW_MODE;
		if (param->control.cmd == CONTROL_COMMAND_STOP) {
			dbg_hw("[FD] CONTROL_COMMAND_STOP");
			goto exit;
		}

		ret = fimc_is_hw_fd_otf_input(hw_ip->base_addr, param);
		if (ret) {
			err_hw("set param fail!!");
			ret = -EINVAL;
			goto exit;
		}
	} else if (param->dma_input.cmd == DMA_INPUT_COMMAND_ENABLE) {
		hw_fd->mode = FD_FORMAT_PLAYBACK_MODE;
		if (param->control.cmd == CONTROL_COMMAND_STOP) {
			dbg_hw("[FD] CONTROL_COMMAND_STOP");
			goto exit;
		}

		ret = fimc_is_hw_fd_dma_input(hw_ip->base_addr, param);
		if (ret) {
			err_hw("set param fail!!");
			ret = -EINVAL;
			goto exit;
		}
	} else {
		hw_fd->mode = FD_FORMAT_MODE_END;
		err_hw("set param fail!!");
		ret = -EINVAL;
		goto exit;
	}

exit:
	return ret;
}

int fimc_is_hw_fd_frame_ndone(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame, u32 instance, bool late_flag)
{
	int wq_id = 0;
	int output_id = 0;
	enum fimc_is_frame_done_type done_type;
	int ret = 0;
	u32 intr_all = 0;

	BUG_ON(!hw_ip);
	BUG_ON(!frame);

	if (late_flag == true)
		done_type = FRAME_DONE_LATE_SHOT;
	else
		done_type = FRAME_DONE_FORCE;

	wq_id     = -1;
	output_id = FIMC_IS_HW_CORE_END;
	if (test_bit(hw_ip->id, &frame->core_flag))
		ret = fimc_is_hardware_frame_done(hw_ip, frame, wq_id, output_id, 1, done_type);

	intr_all = ((1 << INTR_FD_MAX_MAP) -1);
	fimc_is_fd_clear_intr(hw_ip->base_addr, intr_all);

	return ret;
}

int fimc_is_hw_fd_load_setfile(struct fimc_is_hw_ip *hw_ip, int index, u32 instance, unsigned long hw_map)
{
	int ret = 0;
	struct fimc_is_hw_fd *hw_fd = NULL;

	BUG_ON(!hw_ip);

	if (!test_bit(hw_ip->id, &hw_map))
		goto exit;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("[%d] not initialized!!", instance);
		ret = -ESRCH;
		goto exit;
	}

	if (!unlikely(hw_ip->priv_info)) {
		err_hw("FD priv info is NULL");
		ret = -EINVAL;
		goto exit;
	}
	hw_fd = (struct fimc_is_hw_fd *)hw_ip->priv_info;

	switch (hw_ip->setfile_info.version) {
	case SETFILE_V2:
		break;
	case SETFILE_V3:
		break;
	default:
		err_hw("[%d][ID:%d] invalid version (%d)", instance, hw_ip->id, hw_ip->setfile_info.version);
		ret = -EINVAL;
		goto exit;
	}

	set_bit(HW_TUNESET, &hw_ip->state);

exit:
	return ret;
}

int fimc_is_hw_fd_apply_setfile(struct fimc_is_hw_ip *hw_ip, int index, u32 instance, unsigned long hw_map)
{
	int ret = 0;
	u32 setfile_index = 0;
	struct fimc_is_hw_fd *hw_fd = NULL;

	if (!test_bit(hw_ip->id, &hw_map))
		goto exit;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		err_hw("[%d] not initialized!!", instance);
		ret = -ESRCH;
		goto exit;
	}

	if (!unlikely(hw_ip->priv_info)) {
		err_hw("FD priv info is NULL");
		ret = -EINVAL;
		goto exit;
	}
	hw_fd = (struct fimc_is_hw_fd *)hw_ip->priv_info;

	setfile_index = hw_ip->setfile_info.index[index];
	hw_fd->setfile = (struct hw_api_fd_setfile *)hw_ip->setfile_info.table[setfile_index].addr;

	if (!hw_fd->setfile)
		goto exit;

	info_hw("[%d][ID:%d] setfile (%d) scenario (%d)\n", instance, hw_ip->id, setfile_index, index);

	if (hw_fd->mode == FD_FORMAT_PREVIEW_MODE) {
		fimc_is_fd_config_by_setfile(hw_ip->base_addr, &hw_fd->setfile->preveiw);
	} else if (hw_fd->mode == FD_FORMAT_PREVIEW_MODE) {
		fimc_is_fd_config_by_setfile(hw_ip->base_addr, &hw_fd->setfile->playback);
	} else {
		err_hw("FD format mode is not available");
		ret = -EINVAL;
		goto exit;
	}

exit:
	return ret;
}

int fimc_is_hw_fd_delete_setfile(struct fimc_is_hw_ip *hw_ip, u32 instance, unsigned long hw_map)
{
	int ret = 0;

	if (!test_bit(hw_ip->id, &hw_map))
		goto exit;

	if (!test_bit(HW_TUNESET, &hw_ip->state)) {
		dbg_hw("[%d][ID:%d] Tuneset is not loaded: skip\n", instance, hw_ip->id);
		goto exit;
	}

	if (hw_ip->setfile_info.num == 0)
		goto exit;
exit:
	return ret;
}
