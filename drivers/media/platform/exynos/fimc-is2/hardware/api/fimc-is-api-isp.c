/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-api-isp.h"
#include "fimc-is-hw-control.h"
#include "sfr/fimc-is-sfr-isp-v310.h"

static void fimc_is_lib_camera_callback(void *this,
	enum lib_isp_callback_event_type event_id,
	u32 instance_id, void *data)
{
	struct fimc_is_hw_ip *hw_ip;

	BUG_ON(!this);

	hw_ip = (struct fimc_is_hw_ip *)this;

	switch (event_id) {
	case LIB_ISP_EVENT_CONFIG_LOCK:
		atomic_inc(&hw_ip->cl_count);
		if (!atomic_read(&hw_ip->hardware->stream_on))
			info_hw("[%d][ID:%d][F:%d]C.L\n", instance_id, hw_ip->id, hw_ip->fcount);

		fimc_is_hardware_config_lock(hw_ip, instance_id, (u32)data);
		break;
	case LIB_ISP_EVENT_FRAME_START_ISR:
		hw_ip->fs_time = cpu_clock(raw_smp_processor_id());
		if (!atomic_read(&hw_ip->hardware->stream_on))
			info_hw("[%d][ID:%d][F:%d]F.S\n", instance_id, hw_ip->id, hw_ip->fcount);

		fimc_is_hardware_frame_start(hw_ip, instance_id);
		break;
	case LIB_ISP_EVENT_FRAME_END:
		hw_ip->fe_time = cpu_clock(raw_smp_processor_id());
		atomic_inc(&hw_ip->fe_count);
		if (!atomic_read(&hw_ip->hardware->stream_on))
			info_hw("[%d][ID:%d][F:%d]F.E\n", instance_id, hw_ip->id, hw_ip->fcount);

		fimc_is_hardware_frame_done(hw_ip, NULL, -1, FIMC_IS_HW_CORE_END, 0, FRAME_DONE_NORMAL);
		atomic_dec(&hw_ip->Vvalid);
		wake_up(&hw_ip->wait_queue);
		/* TODO: meta data copy */
		break;
	default:
		break;
	}

	return;
};

static void fimc_is_lib_io_callback(void *this,
	enum lib_isp_callback_event_type event_id,
	u32 instance_id)
{
	struct fimc_is_hw_ip *hw_ip;
	u32 status = 0; /* 0:FRAME_DONE, 1:FRAME_NDONE */
	int wq_id = 0;
	int output_id = 0;

	BUG_ON(!this);

	hw_ip = (struct fimc_is_hw_ip *)this;

	switch (event_id) {
	case LIB_ISP_EVENT_DMA_A_OUT_DONE: /* after BDS : VDMA2 */
		if (!atomic_read(&hw_ip->hardware->stream_on))
			info_hw("[%d][ID:%d][F:%d]DMA 2\n",
				instance_id, hw_ip->id, hw_ip->fcount);

#if defined(ENABLE_3AAISP)
		hw_ip->dma_time = cpu_clock(raw_smp_processor_id());
		wq_id = WORK_30C_FDONE;
		output_id = ENTRY_3AC;
#else
		if (hw_ip->id == DEV_HW_3AA0) {
			wq_id = WORK_30P_FDONE;
			output_id = ENTRY_3AP;
		} else {
			wq_id = WORK_31P_FDONE;
			output_id = ENTRY_3AP;
		}
#endif
		fimc_is_hardware_frame_done(hw_ip, NULL, wq_id, output_id, status, FRAME_DONE_NORMAL);
		break;
	case LIB_ISP_EVENT_DMA_B_OUT_DONE: /* before BDS : VDMA4 */
		if (!atomic_read(&hw_ip->hardware->stream_on))
			info_hw("[%d][ID:%d][F:%d]DMA 4\n",
				instance_id, hw_ip->id, hw_ip->fcount);

		if (hw_ip->id == DEV_HW_3AA0)
			wq_id = WORK_30C_FDONE;
		else
			wq_id = WORK_31C_FDONE;

		fimc_is_hardware_frame_done(hw_ip, NULL, wq_id, ENTRY_3AC, status, FRAME_DONE_NORMAL);
		break;
	default:
		break;
	}

	return;
};

struct lib_callback_func fimc_is_lib_cb_func = {
	.camera_callback	= fimc_is_lib_camera_callback,
	.io_callback		= fimc_is_lib_io_callback,
};

int fimc_is_lib_isp_chain_create(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this,
	u32 instance_id)
{
	int ret = 0;
	unsigned int chain_id;
	unsigned int set_b_offset;

	BUG_ON(!hw_ip);
	BUG_ON(!this->func);

	switch (hw_ip->id) {
	case DEV_HW_3AA0:
		chain_id = 0;
		break;
	case DEV_HW_3AA1:
		chain_id = 1;
		break;
	case DEV_HW_ISP0:
		chain_id = 0;
		break;
	case DEV_HW_ISP1:
		chain_id = 1;
		break;
	default:
		err_lib("invalid hw (%d)", hw_ip->id);
		ret = -EINVAL;
		goto exit;
		break;
	}

	set_b_offset = ((unsigned int)hw_ip->base_addr_b == 0) ? 0 : (unsigned int)hw_ip->base_addr_b - (unsigned int)hw_ip->base_addr;

	ret = this->func->chain_create(chain_id, (unsigned int)hw_ip->base_addr, set_b_offset, &fimc_is_lib_cb_func);
	if (ret) {
		err_lib("chain_create fail (%d)", hw_ip->id);
		ret = -EINVAL;
		goto exit;
	}

	info_lib("[%d]chain_create (%d)[reg_base:0x%x][b_offset:0x%x]\n",
		instance_id, hw_ip->id, (unsigned int)hw_ip->base_addr, set_b_offset);

exit:
	return ret;
}

int fimc_is_lib_isp_object_create(struct fimc_is_hw_ip *hw_ip,
		struct fimc_is_lib_isp *this, u32 instance_id, u32 rep_flag, u32 module_id)
{
	int ret = 0;
	unsigned int chain_id, input_type, obj_info = 0;
	struct fimc_is_group *parent;

	BUG_ON(!hw_ip);
	BUG_ON(!this->func);

	switch (hw_ip->id) {
	case DEV_HW_3AA0:
		chain_id = 0;
		break;
	case DEV_HW_3AA1:
		chain_id = 1;
		break;
	case DEV_HW_ISP0:
		chain_id = 0;
		break;
	case DEV_HW_ISP1:
		chain_id = 1;
		break;
	default:
		err_lib("invalid hw (%d)", hw_ip->id);
		ret = -EINVAL;
		goto exit;
		break;
	}

	parent = hw_ip->group[instance_id];
	while (parent->parent)
		parent = parent->parent;
	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &parent->state))
		input_type = 0; /* default */
	else
		input_type = 1;

	obj_info |= chain_id << CHAIN_ID_SHIFT;
	obj_info |= instance_id << INSTANCE_ID_SHIFT;
	obj_info |= rep_flag << REPROCESSING_FLAG_SHIFT;
	obj_info |= (input_type) << DMA_INPUT_ID_SHIFT;
	obj_info |= (module_id) << MODULE_ID_SHIFT;

	info_lib("obj_create: chain(%d), instance(%d), rep(%d), in_type(%d),"
			" obj_info(0x%08x), module_id(%d)\n",
			chain_id, instance_id, rep_flag, input_type, obj_info, module_id);
	ret = this->func->object_create(&this->object, obj_info, hw_ip);
	if (ret) {
		err_lib("object_create fail (%d)", hw_ip->id);
		ret = -EINVAL;
		goto exit;
	}

exit:
	return ret;
}

void fimc_is_lib_isp_chain_destroy(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this,
	u32 instance_id)
{
	int ret = 0;
	unsigned int chain_id;

	BUG_ON(!hw_ip);
	BUG_ON(!this->func);

	switch (hw_ip->id) {
	case DEV_HW_3AA0:
		chain_id = 0;
		break;
	case DEV_HW_3AA1:
		chain_id = 1;
		break;
	case DEV_HW_ISP0:
		chain_id = 0;
		break;
	case DEV_HW_ISP1:
		chain_id = 1;
		break;
	default:
		err_lib("invalid hw (%d)", hw_ip->id);
		ret = -EINVAL;
		goto exit;
		break;
	}

	ret = this->func->chain_destroy(chain_id);
	if (ret) {
		err_lib("chain_destroy fail (%d)", hw_ip->id);
		ret = -EINVAL;
		goto exit;
	}
	info_lib("[%d]chain_destroy (%d)\n", instance_id, hw_ip->id);

exit:
	return;
}

void fimc_is_lib_isp_object_destroy(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this,
	u32 instance_id)
{
	int ret = 0;

	BUG_ON(!this);

	ret = this->func->object_destroy(this->object, instance_id);
	if (ret) {
		err_lib("object_destroy fail (%d)", hw_ip->id);
		ret = -EINVAL;
		goto exit;
	}
	info_lib("[%d]object_destroy (%d)\n", instance_id, hw_ip->id);

exit:
	return;
}

int fimc_is_lib_isp_set_param(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this,
	void *param)
{
	int ret;

	switch (hw_ip->id) {
	case DEV_HW_3AA0:
	case DEV_HW_3AA1:
		ret = this->func->set_param(this->object, param);
		if (ret)
			err_lib("3aa set_param fail (%d)", hw_ip->id);
		break;
	case DEV_HW_ISP0:
	case DEV_HW_ISP1:
		ret = this->func->set_param(this->object, param);
		if (ret)
			err_lib("isp set_param fail (%d)", hw_ip->id);
		break;
	default:
		ret = -EINVAL;
		err_lib("invalid hw (%d)", hw_ip->id);
		break;
	}

	return ret;
}

int fimc_is_lib_isp_set_ctrl(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this,
	struct fimc_is_frame *frame)
{
	int ret = 0;

	BUG_ON(!this);

	switch (hw_ip->id) {
	case DEV_HW_3AA0:
	case DEV_HW_3AA1:
		ret = this->func->set_ctrl(this->object, frame->instance, frame->fcount, frame->shot);
		if (ret)
			err_lib("3aa set_ctrl fail (%d)", hw_ip->id);
		break;
	case DEV_HW_ISP0:
	case DEV_HW_ISP1:
		ret = this->func->set_ctrl(this->object, frame->instance, frame->fcount, frame->shot);
		if (ret)
			err_lib("isp set_ctrl fail (%d)", hw_ip->id);
		break;
	default:
		err_lib("invalid hw (%d)", hw_ip->id);
		break;
	}

	return 0;
}

void fimc_is_lib_isp_shot(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this,
	void *param_set,
	struct camera2_shot *shot)
{
	int ret = 0;

	BUG_ON(!this);

	switch (hw_ip->id) {
	case DEV_HW_3AA0:
	case DEV_HW_3AA1:
		ret = this->func->shot(this->object, (struct taa_param_set *)param_set, shot);
		if (ret)
			err_lib("3aa shot fail (%d)", hw_ip->id);
		break;
	case DEV_HW_ISP0:
	case DEV_HW_ISP1:
		ret = this->func->shot(this->object, (struct isp_param_set *)param_set, shot);
		if (ret)
			err_lib("isp shot fail (%d)", hw_ip->id);
		break;
	default:
		err_lib("invalid hw (%d)", hw_ip->id);
		break;
	}
}

int fimc_is_lib_isp_get_meta(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this,
	struct fimc_is_frame *frame)

{
	int ret = 0;

	BUG_ON(!this);

	switch (hw_ip->id) {
	case DEV_HW_3AA0:
	case DEV_HW_3AA1:
		ret = this->func->get_meta(this->object, frame->instance, frame->fcount, frame->shot);
		if (ret)
			err_lib("3aa get_meta fail (%d)", hw_ip->id);
		break;
	case DEV_HW_ISP0:
	case DEV_HW_ISP1:
		ret = this->func->get_meta(this->object, frame->instance, frame->fcount, frame->shot);
		if (ret)
			err_lib("isp get_meta fail (%d)", hw_ip->id);
		break;
	default:
		err_lib("invalid hw (%d)", hw_ip->id);
		break;
	}

	return ret;
}

void fimc_is_lib_isp_stop(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_lib_isp *this,
	u32 instance_id)
{
	int ret = 0;

	BUG_ON(!this->func);

	ret = this->func->stop(this->object, instance_id);
	if (ret) {
		err_lib("object_suspend fail (%d)", hw_ip->id);
		goto exit;
	}
	info_lib("[%d]object_suspend (%d)\n", instance_id, hw_ip->id);

exit:
	return;
}

int fimc_is_lib_isp_create_tune_set(struct fimc_is_lib_isp *this,
	u32 addr, u32 size, u32 index, int flag, unsigned int instance_id)
{
	struct lib_tune_set tune_set;
	int ret = 0;

	tune_set.index = index;
	tune_set.addr = addr;
	tune_set.size = size;
	tune_set.decrypt_flag = flag;
	ret = this->func->create_tune_set(this->object, instance_id, &tune_set);
	if (ret) {
		err_lib("create_tune_set fail");
		goto exit;
	}
exit:
	return ret;
}

int fimc_is_lib_isp_apply_tune_set(struct fimc_is_lib_isp *this,
	unsigned int index, unsigned int instance_id)
{
	int ret = 0;

	ret = this->func->apply_tune_set(this->object, instance_id, index);
	if (ret) {
		err_lib("apply_tune_set fail");
		goto exit;
	}
exit:
	return ret;
}

int fimc_is_lib_isp_delete_tune_set(struct fimc_is_lib_isp *this,
	unsigned int index, unsigned int instance_id)
{
	int ret = 0;

	ret = this->func->delete_tune_set(this->object, instance_id, index);
	if (ret) {
		err_lib("delete_tune_set fail");
		goto exit;
	}
exit:
	return ret;
}

int fimc_is_lib_isp_load_cal_data(struct fimc_is_lib_isp *this,
	unsigned int instance_id, unsigned int addr)
{
	char version[20];
	int ret = 0;

	info_lib("CAL data addr 0x%08x\n", addr);
	memcpy(version, (void *)(addr + 0x20), 16);
	version[16] = '\0';
	info_lib("CAL version: %s\n", version);

	ret = this->func->load_cal_data(this->object, instance_id, addr);
	if (ret) {
		err_lib("apply_tune_set fail");
		goto exit;
	}
exit:
	return ret;
}

int fimc_is_lib_isp_get_cal_data(struct fimc_is_lib_isp *this,
	unsigned int instance_id, struct cal_info *data, int type)
{
	int ret = 0;

	ret = this->func->get_cal_data(this->object, instance_id, data, type);
	if (ret) {
		err_lib("apply_tune_set fail");
		goto exit;
	}
exit:
	return ret;
}

int fimc_is_lib_isp_sensor_info_mode_chg(struct fimc_is_lib_isp *this,
	unsigned int instance_id, struct camera2_shot *shot)
{
	int ret = 0;

	ret = this->func->sensor_info_mode_chg(this->object, instance_id, shot);
	if (ret) {
		err_lib("sensor_info_mode_chg fail");
		goto exit;
	}
exit:
	return ret;
}

int fimc_is_lib_isp_sensor_update_control(struct fimc_is_lib_isp *this,
	unsigned int instance_id, u32 frame_count, struct camera2_shot *shot)
{
	int ret = 0;

	ret = this->func->sensor_update_ctl(this->object, instance_id, frame_count, shot);
	if (ret) {
		err_lib("sensor_update_ctl fail");
		goto exit;
	}
exit:
	return ret;
}

int fimc_is_lib_isp_convert_face_map(struct fimc_is_hardware *hardware,
	struct taa_param_set *param_set, struct fimc_is_frame *frame)
{
	int ret = 0;
	int i;
	u32 preview_width, preview_height;
	u32 bayer_crop_width, bayer_crop_height;
	struct fimc_is_group *group = NULL;
	struct camera2_shot *shot = NULL;
	struct fimc_is_device_ischain *device = NULL;

	BUG_ON(!hardware);
	BUG_ON(!param_set);
	BUG_ON(!frame);

	shot = frame->shot;
	BUG_ON(!shot);

	group = (struct fimc_is_group *)frame->group;
	BUG_ON(!group);

	device = group->device;
	BUG_ON(!device);

	if (shot->uctl.fdUd.faceDetectMode == FACEDETECT_MODE_OFF)
		goto exit;

	/*
	 * The face size which an algorithm uses is determined
	 * by the input bayer crop size of 3aa.
	 */
	if (param_set->otf_input.cmd == OTF_INPUT_COMMAND_ENABLE) {
		bayer_crop_width = param_set->otf_input.bayer_crop_width;
		bayer_crop_height = param_set->otf_input.bayer_crop_height;
	} else if (param_set->dma_input.cmd == DMA_INPUT_COMMAND_ENABLE) {
		bayer_crop_width = param_set->dma_input.bayer_crop_width;
		bayer_crop_height = param_set->dma_input.bayer_crop_height;
	} else {
		err_hw("invalid hw input!!\n");
		ret = -EINVAL;
		goto exit;
	}

	if (bayer_crop_width == 0 || bayer_crop_height == 0) {
		warn_hw("%s: invalid crop size (%d * %d)!!\n",
			__func__, bayer_crop_width, bayer_crop_height);
		goto exit;
	}

	/* The face size is determined by the preview size in FD uctl */
	preview_width = device->scp.output.width;
	preview_height = device->scp.output.height;
	if (preview_width == 0 || preview_height == 0) {
		warn_hw("%s: invalid preview size (%d * %d)!!\n",
			__func__, preview_width, preview_height);
		goto exit;
	}

	/* Convert face size */
	for (i = 0; i < CAMERA2_MAX_FACES; i++) {
		if (shot->uctl.fdUd.faceScores[i] == 0)
			continue;

		shot->uctl.fdUd.faceRectangles[i][0] =
				CONVRES(shot->uctl.fdUd.faceRectangles[i][0],
				preview_width, bayer_crop_width);
		shot->uctl.fdUd.faceRectangles[i][1] =
				CONVRES(shot->uctl.fdUd.faceRectangles[i][1],
				preview_height, bayer_crop_height);
		shot->uctl.fdUd.faceRectangles[i][2] =
				CONVRES(shot->uctl.fdUd.faceRectangles[i][2],
				preview_width, bayer_crop_width);
		shot->uctl.fdUd.faceRectangles[i][3] =
				CONVRES(shot->uctl.fdUd.faceRectangles[i][3],
				preview_height, bayer_crop_height);

		dbg_lib("%s: ID(%d), x_min(%d), y_min(%d), x_max(%d), y_max(%d)\n",
				__func__, shot->uctl.fdUd.faceIds[i],
				shot->uctl.fdUd.faceRectangles[i][0],
				shot->uctl.fdUd.faceRectangles[i][1],
				shot->uctl.fdUd.faceRectangles[i][2],
				shot->uctl.fdUd.faceRectangles[i][3]);
	}

exit:
	return ret;
}

void fimc_is_isp_get_bcrop1_size(void __iomem *base_addr, u32 *width, u32 *height)
{
	*width  = fimc_is_hw_get_field(base_addr, &isp_regs[ISP_R_BCROP1_SIZE_X], &isp_fields[ISP_F_BCROP1_SIZE_X]);
	*height = fimc_is_hw_get_field(base_addr, &isp_regs[ISP_R_BCROP1_SIZE_Y], &isp_fields[ISP_F_BCROP1_SIZE_Y]);
}
