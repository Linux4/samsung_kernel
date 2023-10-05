/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-hw-3aa.h"
#include "is-err.h"
#include "pablo-fpsimd.h"

static int __nocfi is_hw_3aa_open(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_3aa *hw_3aa = NULL;

	FIMC_BUG(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HW3AA");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw_ip->priv_info = vzalloc(sizeof(struct is_hw_3aa));
	if(!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	hw_3aa = (struct is_hw_3aa *)hw_ip->priv_info;
	hw_3aa->lib_support = is_get_lib_support();

	ret = CALL_HW_HELPER_OPS(hw_ip, open, instance, &hw_3aa->lib[instance],
				LIB_FUNC_3AA);
	if (ret)
		goto err_chain_create;

	set_bit(HW_OPEN, &hw_ip->state);
	msdbg_hw(2, "open: framemgr[%s]", instance, hw_ip, hw_ip->framemgr->name);

	return 0;

err_chain_create:
	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_3aa_init(struct is_hw_ip *hw_ip, u32 instance,
			bool flag, u32 f_type)
{
	int ret = 0;
	struct is_hw_3aa *hw_3aa;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	hw_3aa = (struct is_hw_3aa *)hw_ip->priv_info;

	hw_3aa->param_set[instance].reprocessing = flag;

	ret = CALL_HW_HELPER_OPS(hw_ip, init, instance, &hw_3aa->lib[instance],
				(u32)flag, f_type, LIB_FUNC_3AA);
	if (ret)
		return ret;

	hw_ip->changed_hw_ip[instance] = NULL;

	set_bit(HW_INIT, &hw_ip->state);
	return ret;
}

static int is_hw_3aa_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_3aa *hw_3aa = (struct is_hw_3aa *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, deinit, instance, &hw_3aa->lib[instance]);
}

static int is_hw_3aa_close(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_3aa *hw_3aa;

	FIMC_BUG(!hw_ip);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_3aa = (struct is_hw_3aa *)hw_ip->priv_info;

	CALL_HW_HELPER_OPS(hw_ip, close, instance, &hw_3aa->lib[instance]);
	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
	frame_manager_close(hw_ip->framemgr);

	clear_bit(HW_OPEN, &hw_ip->state);

	return ret;
}

static int is_hw_3aa_sensor_start(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_3aa *hw_3aa = (struct is_hw_3aa *)hw_ip->priv_info;
	struct is_frame *frame = NULL;
	struct is_framemgr *framemgr;
	struct camera2_shot *shot = NULL;
#ifdef ENABLE_MODECHANGE_CAPTURE
	struct is_device_sensor *sensor;
#endif

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("sensor_start: mode_change for sensor\n", instance, hw_ip);

	if (hw_3aa->param_set[instance].otf_input.source != OTF_INPUT_PAF_RDMA_PATH) {
		/* For sensor info initialize for mode change */
		framemgr = hw_ip->framemgr;
		FIMC_BUG(!framemgr);

		framemgr_e_barrier(framemgr, 0);
		frame = peek_frame(framemgr, FS_HW_CONFIGURE);
		framemgr_x_barrier(framemgr, 0);
		if (frame) {
			shot = frame->shot;
		} else {
			mswarn_hw("enable (frame:NULL)(%d)", instance, hw_ip,
				framemgr->queued_count[FS_HW_CONFIGURE]);
#ifdef ENABLE_MODECHANGE_CAPTURE
			sensor = hw_ip->group[instance]->device->sensor;
			if (sensor && sensor->mode_chg_frame) {
				frame = sensor->mode_chg_frame;
				shot = frame->shot;
				msinfo_hw("[F:%d]mode_chg_frame used for REMOSAIC\n",
					instance, hw_ip, frame->fcount);
			}
#endif
		}

		if (shot) {
			ret = is_lib_isp_sensor_info_mode_chg(&hw_3aa->lib[instance],
					instance, shot);
			if (ret < 0) {
				mserr_hw("is_lib_isp_sensor_info_mode_chg fail)",
					instance, hw_ip);
			}
		}
	}

	return ret;
}

static int is_hw_3aa_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
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

static int is_hw_3aa_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_3aa *hw_3aa;
	struct taa_param_set *param_set;
	u32 i;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("disable: Vvalid(%d)\n", instance, hw_ip,
		atomic_read(&hw_ip->status.Vvalid));

	FIMC_BUG(!hw_ip->priv_info);
	hw_3aa = (struct is_hw_3aa *)hw_ip->priv_info;
	param_set = &hw_3aa->param_set[instance];

	/*
	 * Vvalid state is only needed to be checked when HW works at same instance.
	 * If HW works at different instance, skip Vvalid checking.
	 */
	if (atomic_read(&hw_ip->instance) == instance) {
		timetowait = wait_event_timeout(hw_ip->status.wait_queue,
				!atomic_read(&hw_ip->status.Vvalid),
				IS_HW_STOP_TIMEOUT);
		if (!timetowait) {
			mserr_hw("wait FRAME_END timeout (%ld)", instance,
					hw_ip, timetowait);
			ret = -ETIME;
		}
	}

	param_set->fcount = 0;

	/* TODO: need to divide each task index */
	for (i = 0; i < TASK_INDEX_MAX; i++) {
		FIMC_BUG(!hw_3aa->lib_support);
		if (hw_3aa->lib_support->task_taaisp[i].task == NULL)
			serr_hw("task is null", hw_ip);
		else
			kthread_flush_worker(&hw_3aa->lib_support->task_taaisp[i].worker);
	}

	CALL_HW_HELPER_OPS(hw_ip, disable, instance, &hw_3aa->lib[instance]);

	if (hw_ip->run_rsc_state)
		return 0;

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

#if IS_ENABLED(ENABLE_3AA_LIC_OFFSET)
static int __is_hw_3aa_get_change_state(struct is_hw_ip *hw_ip, int instance, int hint)
{
	struct is_hw_3aa *hw_3aa = (struct is_hw_3aa *)hw_ip->priv_info;
	struct is_hardware *hw = NULL;
	int i, ref_cnt = 0, ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->hardware);

	hw = hw_ip->hardware;

	for (i = 0; i < SENSOR_POSITION_MAX; i++) {
		if (atomic_read(&hw->streaming[i]) & BIT(HW_ISCHAIN_STREAMING))
			ref_cnt++;
	}

	if (hw_3aa->param_set[instance].otf_input.source != OTF_INPUT_PAF_RDMA_PATH) {
		/* OTF or vOTF case */
		if (ref_cnt) {
			ret = SRAM_CFG_BLOCK;
		} else {
			/*
			 * If LIC SRAM_CFG_N already called before stream on,
			 * Skip for not call duplicate lic offset setting
			 */
			if (atomic_inc_return(&hw->lic_updated) == 1)
				ret = SRAM_CFG_N;
			else
				ret = SRAM_CFG_BLOCK;
		}
	} else {
		/* Reprocessing case */
		if (ref_cnt)
			ret = SRAM_CFG_BLOCK;
		else
			ret = SRAM_CFG_R;
	}

#if IS_ENABLED(TAA_SRAM_STATIC_CONFIG)
	if (hw->lic_offset_def[0] == 0) {
		mswarn_hw("lic_offset is 0 with STATIC_CONFIG, use dynamic mode", instance, hw_ip);
	} else {
		if (ret == SRAM_CFG_N)
			ret = SRAM_CFG_S;
		else
			ret = SRAM_CFG_BLOCK;
	}
#endif

	if (!atomic_read(&hw->streaming[hw->sensor_position[instance]]))
		msinfo_hw("LIC change state: %d (refcnt(streaming:%d/updated:%d))", instance, hw_ip,
			ret, ref_cnt, atomic_read(&hw->lic_updated));

	return ret;
}

static int __is_hw_3aa_change_sram_offset(struct is_hw_ip *hw_ip, int instance, int mode)
{
	struct is_hw_3aa *hw_3aa = NULL;
	struct is_hardware *hw = NULL;
	struct taa_param_set *param_set = NULL;
	u32 target_w;
	u32 offset[LIC_OFFSET_MAX] = {0, };
	u32 offsets = LIC_CHAIN_OFFSET_NUM / 2 - 1;
	u32 set_idx = offsets + 1;
	int ret = 0;
	int i, index_a, index_b;
	char *str_a, *str_b;
	int id, sum;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->hardware);
	FIMC_BUG(!hw_ip->priv_info);

	hw = hw_ip->hardware;
	hw_3aa = (struct is_hw_3aa *)hw_ip->priv_info;
	param_set = &hw_3aa->param_set[instance];

	/* check condition */
	switch (mode) {
	case SRAM_CFG_BLOCK:
		return ret;
	case SRAM_CFG_N:
		for (i = LIC_OFFSET_0; i < offsets; i++) {
			offset[i] = hw->lic_offset_def[i];
			if (!offset[i])
				offset[i] = (IS_MAX_HW_3AA_SRAM / offsets) * (1 + i);
		}

		target_w = param_set->otf_input.width;
		/* 3AA2 is not considered */
		/* TODO: set offset dynamically */
		if (hw_ip->id == DEV_HW_3AA0) {
			if (target_w > offset[LIC_OFFSET_0])
				offset[LIC_OFFSET_0] = target_w;
		} else if (hw_ip->id == DEV_HW_3AA1) {
			if (target_w > (IS_MAX_HW_3AA_SRAM - offset[LIC_OFFSET_0]))
				offset[LIC_OFFSET_0] = IS_MAX_HW_3AA_SRAM - target_w;
		}
		break;
	case SRAM_CFG_R:
	case SRAM_CFG_MODE_CHANGE_R:
		target_w = param_set->otf_input.width;
		if (param_set->dma_input.cmd == DMA_INPUT_COMMAND_ENABLE)
			target_w = MAX(param_set->dma_input.width, target_w);

		id = hw_ip->id - DEV_HW_3AA0;
		sum = 0;
		for (i = LIC_OFFSET_0; i < offsets; i++) {
			if (id == i)
				offset[i] = sum + target_w;
			else
				offset[i] = sum + 0;

			sum = offset[i];
		}
		break;
	case SRAM_CFG_S:
		for (i = LIC_OFFSET_0; i < offsets; i++)
			offset[i] = hw->lic_offset_def[i];
		break;
	default:
		mserr_hw("invalid mode (%d)", instance, hw_ip, mode);
		return ret;
	}

	index_a = COREX_SETA * set_idx; /* setA */
	index_b = COREX_SETB * set_idx; /* setB */

	for (i = LIC_OFFSET_0; i < offsets; i++) {
		hw->lic_offset[0][index_a + i] = offset[i];
		hw->lic_offset[0][index_b + i] = offset[i];
	}
	hw->lic_offset[0][index_a + offsets] = hw->lic_offset_def[index_a + offsets];
	hw->lic_offset[0][index_b + offsets] = hw->lic_offset_def[index_b + offsets];

	ret = is_lib_set_sram_offset(hw_ip, &hw_3aa->lib[instance], instance);
	if (ret) {
		mserr_hw("lib_set_sram_offset fail", instance, hw_ip);
		ret = -EINVAL;
	}

	if (in_atomic())
		goto exit_in_atomic;

	str_a = __getname();
	if (unlikely(!str_a)) {
		mserr_hw(" out of memory for str_a!", instance, hw_ip);
		goto err_alloc_str_a;
	}

	str_b = __getname();
	if (unlikely(!str_b)) {
		mserr_hw(" out of memory for str_b!", instance, hw_ip);
		goto err_alloc_str_b;
	}

	snprintf(str_a, PATH_MAX, "%d", hw->lic_offset[0][index_a + 0]);
	snprintf(str_b, PATH_MAX, "%d", hw->lic_offset[0][index_b + 0]);
	for (i = LIC_OFFSET_1; i <= offsets; i++) {
		snprintf(str_a, PATH_MAX, "%s, %d", str_a, hw->lic_offset[0][index_a + i]);
		snprintf(str_b, PATH_MAX, "%s, %d", str_b, hw->lic_offset[0][index_b + i]);
	}
	msinfo_hw("=> (%d) update 3AA SRAM offset: setA(%s), setB(%s)\n",
		instance, hw_ip, mode, str_a, str_b);

	__putname(str_b);
err_alloc_str_b:
	__putname(str_a);
err_alloc_str_a:
exit_in_atomic:

	return ret;
}
#endif

static int is_hw_3aa_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	int ret = 0;
	int i, cur_idx, batch_num;
	struct is_hw_3aa *hw_3aa;
	struct taa_param_set *param_set;
	u32 instance;
	u32 input_w, input_h, crop_x, crop_y, output_w = 0, output_h = 0;
	struct is_param_region *param_region;
#if IS_ENABLED(ENABLE_3AA_LIC_OFFSET)
	int mode;
#endif
	u32 cmd_before_bds, cmd_after_bds, cmd_efd, cmd_mrg;
	u32 cmd_drc_grid = 0;
#if defined(ENABLE_ORBDS)
	u32 cmd_orb_ds = 0;
#endif
#if defined(ENABLE_LMEDS)
	u32 cmd_lme_ds = 0;
#endif
#if IS_ENABLED(USE_HF_INTERFACE)
	u32 cmd_hf = 0;
#endif

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
	hw_3aa = (struct is_hw_3aa *)hw_ip->priv_info;
	param_set = &hw_3aa->param_set[instance];

	cur_idx = frame->cur_buf_index;

	param_region = frame->parameter;
	if (frame->type == SHOT_TYPE_INTERNAL) {
		/* OTF INPUT case */
		cmd_before_bds = param_set->dma_output_before_bds.cmd;
		param_set->dma_output_before_bds.cmd
			= DMA_OUTPUT_COMMAND_DISABLE;
		param_set->output_dva_before_bds[0] = 0x0;

		cmd_after_bds = param_set->dma_output_after_bds.cmd;
		param_set->dma_output_after_bds.cmd
			= DMA_OUTPUT_COMMAND_DISABLE;
		param_set->output_dva_after_bds[0] = 0x0;

		cmd_efd = param_set->dma_output_efd.cmd;
		param_set->dma_output_efd.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->output_dva_efd[0] = 0x0;

		cmd_mrg = param_set->dma_output_mrg.cmd;
		param_set->dma_output_mrg.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->output_dva_mrg[0] = 0x0;

#if IS_ENABLED(LOGICAL_VIDEO_NODE)
		cmd_drc_grid = param_set->dma_output_drcgrid.cmd;
		param_set->dma_output_drcgrid.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->output_dva_drcgrid[0] = 0;
#endif
#if defined(ENABLE_ORBDS)
		cmd_orb_ds = param_set->dma_output_orbds.cmd;
		param_set->dma_output_orbds.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->output_dva_orbds[0] = 0x0;
#endif
#if defined(ENABLE_LMEDS)
		cmd_lme_ds = param_set->dma_output_lmeds.cmd;
		param_set->dma_output_lmeds.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->output_dva_lmeds[0] = 0x0;
#endif
#if IS_ENABLED(USE_HF_INTERFACE)
		cmd_hf = param_set->dma_output_hf.cmd;
		param_set->dma_output_hf.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->output_dva_hf[0] = 0;
#endif

		param_set->output_kva_me[0] = 0;
#if IS_ENABLED(SOC_ORB0C)
		param_set->output_kva_orb[0] = 0;
#endif

		hw_ip->internal_fcount[instance] = frame->fcount;
		goto config;
	} else {
		FIMC_BUG(!frame->shot);

		if (hw_ip->internal_fcount[instance] != 0)
			hw_ip->internal_fcount[instance] = 0;
	}

	is_hw_3aa_update_param(hw_ip, param_region, param_set, frame->pmap, instance);

	/* DMA settings */
	CALL_HW_OPS(hw_ip, dma_cfg, "3xs",
			hw_ip, frame, cur_idx, frame->num_buffers,
			&param_set->dma_input.cmd,
			param_set->dma_input.plane,
			param_set->input_dva,
			frame->dvaddr_buffer);
	if (param_set->dma_input.cmd == DMA_INPUT_COMMAND_ENABLE) {
		input_w = param_set->dma_input.bayer_crop_width;
		input_h = param_set->dma_input.bayer_crop_height;
		crop_x = param_set->dma_input.bayer_crop_offset_x;
		crop_y = param_set->dma_input.bayer_crop_offset_y;
	} else {
		input_w = param_set->otf_input.bayer_crop_width;
		input_h = param_set->otf_input.bayer_crop_height;
		crop_x = param_set->otf_input.bayer_crop_offset_x;
		crop_y = param_set->otf_input.bayer_crop_offset_y;
	}

	cmd_before_bds = CALL_HW_OPS(hw_ip, dma_cfg, "3xc",
		hw_ip, frame, cur_idx, frame->num_buffers,
		&param_set->dma_output_before_bds.cmd,
		param_set->dma_output_before_bds.plane,
		param_set->output_dva_before_bds,
		frame->txcTargetAddress);

	cmd_after_bds = CALL_HW_OPS(hw_ip, dma_cfg, "3xp",
		hw_ip, frame, cur_idx, frame->num_buffers,
		&param_set->dma_output_after_bds.cmd,
		param_set->dma_output_after_bds.plane,
		param_set->output_dva_after_bds,
		frame->txpTargetAddress);
	if (cmd_after_bds == DMA_OUTPUT_COMMAND_ENABLE) {
		output_w = param_set->dma_output_after_bds.width;
		output_h = param_set->dma_output_after_bds.height;
	}

	cmd_efd = CALL_HW_OPS(hw_ip, dma_cfg, "efd",
		hw_ip, frame, cur_idx, frame->num_buffers,
		&param_set->dma_output_efd.cmd,
		param_set->dma_output_efd.plane,
		param_set->output_dva_efd,
		frame->efdTargetAddress);

	cmd_mrg = CALL_HW_OPS(hw_ip, dma_cfg, "mrg",
		hw_ip, frame, cur_idx, frame->num_buffers,
		&param_set->dma_output_mrg.cmd,
		param_set->dma_output_mrg.plane,
		param_set->output_dva_mrg,
		frame->mrgTargetAddress);

#if IS_ENABLED(LOGICAL_VIDEO_NODE)
	cmd_drc_grid = CALL_HW_OPS(hw_ip, dma_cfg, "txdrg",
		hw_ip, frame, cur_idx, frame->num_buffers,
		&param_set->dma_output_drcgrid.cmd,
		param_set->dma_output_drcgrid.plane,
		param_set->output_dva_drcgrid,
		frame->txdgrTargetAddress);
#endif
#if defined(ENABLE_ORBDS)
	cmd_orb_ds = CALL_HW_OPS(hw_ip, dma_cfg, "ods",
		hw_ip, frame, cur_idx, frame->num_buffers,
		&param_set->dma_output_orbds.cmd,
		param_set->dma_output_orbds.plane,
		param_set->output_dva_orbds,
		frame->txodsTargetAddress);
#endif
#if defined(ENABLE_LMEDS)
	cmd_lme_ds = CALL_HW_OPS(hw_ip, dma_cfg, "lds",
		hw_ip, frame, cur_idx, frame->num_buffers,
		&param_set->dma_output_lmeds.cmd,
		param_set->dma_output_lmeds.plane,
		param_set->output_dva_lmeds,
		frame->txldsTargetAddress);
#endif
#if IS_ENABLED(USE_HF_INTERFACE)
	cmd_hf = CALL_HW_OPS(hw_ip, dma_cfg, "thf",
		hw_ip, frame, cur_idx, frame->num_buffers,
		&param_set->dma_output_hf.cmd,
		param_set->dma_output_hf.plane,
		param_set->output_dva_hf,
		frame->txhfTargetAddress);
#endif

	for (i = 0; i < frame->planes; i++) {
		param_set->output_kva_me[i] = frame->mexcTargetAddress[i + cur_idx];
		if (param_set->output_kva_me[i] == 0) {
			msdbg_hw(2, "[F:%d]mexcTargetAddress[%d] is zero",
					instance, hw_ip, frame->fcount, i);
		}

#if IS_ENABLED(SOC_ORB0C)
		param_set->output_kva_orb[i] = frame->orbxcKTargetAddress[i + cur_idx];
		if (param_set->output_kva_orb[i] == 0) {
			msdbg_hw(2, "[F:%d]orbxcKTargetAddress[%d] is zero",
					instance, hw_ip, frame->fcount, i);
		}
#endif
	}

	if (frame->shot_ext) {
		frame->shot_ext->binning_ratio_x = (u16)param_set->sensor_config.sensor_binning_ratio_x;
		frame->shot_ext->binning_ratio_y = (u16)param_set->sensor_config.sensor_binning_ratio_y;
		frame->shot_ext->crop_taa_x = crop_x;
		frame->shot_ext->crop_taa_y = crop_y;
		if (output_w && output_h) {
			frame->shot_ext->bds_ratio_x = (input_w / output_w);
			frame->shot_ext->bds_ratio_y = (input_h / output_h);
		} else {
			frame->shot_ext->bds_ratio_x = 1;
			frame->shot_ext->bds_ratio_y = 1;
		}
	}

config:
	param_set->instance_id = instance;
	param_set->fcount = frame->fcount;

	/* multi-buffer */
	hw_ip->num_buffers = frame->num_buffers;
	batch_num = hw_ip->framemgr->batch_num;
	/* Update only for SW_FRO scenario */
	if (batch_num > 1 && hw_ip->num_buffers == 1) {
		hw_ip->num_buffers |= batch_num << SW_FRO_NUM_SHIFT;
		hw_ip->num_buffers |= cur_idx << CURR_INDEX_SHIFT;
	}

	if (frame->shot) {
		param_set->sensor_config.min_target_fps = frame->shot->ctl.aa.aeTargetFpsRange[0];
		param_set->sensor_config.max_target_fps = frame->shot->ctl.aa.aeTargetFpsRange[1];
		param_set->sensor_config.frametime = 1000000 / param_set->sensor_config.min_target_fps;
		dbg_hw(2, "3aa_shot: min_fps(%d), max_fps(%d), frametime(%d)\n",
			param_set->sensor_config.min_target_fps,
			param_set->sensor_config.max_target_fps,
			param_set->sensor_config.frametime);

#if IS_ENABLED(ENABLE_VRA)
		ret = is_lib_isp_convert_face_map(hw_ip->hardware, param_set, frame);
		if (ret)
			mserr_hw("Convert face size is fail : %d", instance, hw_ip, ret);
#endif

		ret = is_lib_isp_set_ctrl(hw_ip, &hw_3aa->lib[instance], frame);
		if (ret)
			mserr_hw("set_ctrl fail", instance, hw_ip);
	}
	if (is_lib_isp_sensor_update_control(&hw_3aa->lib[instance],
			instance, frame->fcount, frame->shot) < 0) {
		mserr_hw("is_lib_isp_sensor_update_control fail",
			instance, hw_ip);
	}

#if IS_ENABLED(ENABLE_3AA_LIC_OFFSET)
	if (frame->type != SHOT_TYPE_INTERNAL) {
		mode = __is_hw_3aa_get_change_state(hw_ip, instance,
			CHK_MODECHANGE_SCN(frame->shot->ctl.aa.captureIntent));
		ret = __is_hw_3aa_change_sram_offset(hw_ip, instance, mode);
	}
#endif
	ret = is_lib_isp_shot(hw_ip, &hw_3aa->lib[instance], param_set, frame->shot);

	/* Restore the CMDs in param_set. */
	param_set->dma_output_before_bds.cmd = cmd_before_bds;
	param_set->dma_output_after_bds.cmd = cmd_after_bds;
	param_set->dma_output_efd.cmd = cmd_efd;
	param_set->dma_output_mrg.cmd = cmd_mrg;
#if IS_ENABLED(LOGICAL_VIDEO_NODE)
	param_set->dma_output_drcgrid.cmd = cmd_drc_grid;
#endif
#if defined(ENABLE_ORBDS)
	param_set->dma_output_orbds.cmd = cmd_orb_ds;
#endif
#if defined(ENABLE_LMEDS)
	param_set->dma_output_lmeds.cmd = cmd_lme_ds;
#endif
#if IS_ENABLED(USE_HF_INTERFACE)
	param_set->dma_output_hf.cmd = cmd_hf;
#endif
	set_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static int is_hw_3aa_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
		IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	int ret;
	struct is_hw_3aa *hw_3aa;
	struct taa_param_set *param_set;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	FIMC_BUG(!hw_ip->priv_info);

	hw_3aa = (struct is_hw_3aa *)hw_ip->priv_info;
	param_set = &hw_3aa->param_set[instance];

	hw_ip->region[instance] = region;

	is_hw_3aa_update_param(hw_ip, &region->parameter, param_set, pmap, instance);

	ret = is_lib_isp_set_param(hw_ip, &hw_3aa->lib[instance], param_set);
	if (ret)
		mserr_hw("set_param fail", instance, hw_ip);

	return ret;
}

void is_hw_3aa_update_param(struct is_hw_ip *hw_ip, struct is_param_region *param_region,
	struct taa_param_set *param_set, IS_DECLARE_PMAP(pmap), u32 instance)
{
	struct taa_param *param;

	FIMC_BUG_VOID(!param_region);
	FIMC_BUG_VOID(!param_set);

	param = &param_region->taa;
	param_set->instance_id = instance;

	if (test_bit(PARAM_SENSOR_CONFIG, pmap))
		memcpy(&param_set->sensor_config, &param_region->sensor.config,
			sizeof(struct param_sensor_config));

	/* check input */
	if (test_bit(PARAM_3AA_OTF_INPUT, pmap))
		memcpy(&param_set->otf_input, &param->otf_input,
			sizeof(struct param_otf_input));

	if (test_bit(PARAM_3AA_VDMA1_INPUT, pmap))
		memcpy(&param_set->dma_input, &param->vdma1_input,
			sizeof(struct param_dma_input));

	/* check output*/
	if (test_bit(PARAM_3AA_OTF_OUTPUT, pmap))
		memcpy(&param_set->otf_output, &param->otf_output,
			sizeof(struct param_otf_output));

	if (test_bit(PARAM_3AA_VDMA4_OUTPUT, pmap))
		memcpy(&param_set->dma_output_before_bds, &param->vdma4_output,
			sizeof(struct param_dma_output));

	if (test_bit(PARAM_3AA_VDMA2_OUTPUT, pmap))
		memcpy(&param_set->dma_output_after_bds, &param->vdma2_output,
			sizeof(struct param_dma_output));

	if (test_bit(PARAM_3AA_FDDMA_OUTPUT, pmap))
		memcpy(&param_set->dma_output_efd, &param->efd_output,
			sizeof(struct param_dma_output));

	if (test_bit(PARAM_3AA_MRGDMA_OUTPUT, pmap))
		memcpy(&param_set->dma_output_mrg, &param->mrg_output,
			sizeof(struct param_dma_output));

#if defined(ENABLE_ORBDS)
	if (test_bit(PARAM_3AA_ORBDS_OUTPUT, pmap))
		memcpy(&param_set->dma_output_orbds, &param->ods_output,
			sizeof(struct param_dma_output));
#endif

#if defined(ENABLE_LMEDS)
	if (test_bit(PARAM_3AA_LMEDS_OUTPUT, pmap))
		memcpy(&param_set->dma_output_lmeds, &param->lds_output,
			sizeof(struct param_dma_output));
#endif

#if defined(ENABLE_HF)
	if (test_bit(PARAM_3AA_DRCG_OUTPUT, pmap))
		memcpy(&param_set->dma_output_drcgrid, &param->drc_output,
			sizeof(struct param_dma_output));

	if (test_bit(PARAM_3AA_ORBDS_OUTPUT, pmap))
		memcpy(&param_set->dma_output_orbds, &param->ods_output,
			sizeof(struct param_dma_output));

	if (test_bit(PARAM_3AA_LMEDS_OUTPUT, pmap))
		memcpy(&param_set->dma_output_lmeds, &param->lds_output,
			sizeof(struct param_dma_output));

	if (test_bit(PARAM_3AA_HF_OUTPUT, pmap))
		memcpy(&param_set->dma_output_hf, &param->hf_output,
			sizeof(struct param_dma_output));
#endif

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && test_bit(PARAM_3AA_STRIPE_INPUT, pmap))
		memcpy(&param_set->stripe_input, &param->stripe_input,
				sizeof(struct param_stripe_input));
}

static int is_hw_3aa_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	int ret;
	struct is_hw_3aa *hw_3aa = (struct is_hw_3aa *)hw_ip->priv_info;

	copy_ctrl_to_dm(frame->shot);

	ret = CALL_HW_HELPER_OPS(hw_ip, get_meta, frame->instance,
				frame, &hw_3aa->lib[frame->instance]);
	if (frame->shot && frame->shot_ext) {
		msdbg_hw(2, "get_meta[F:%d]: ni(%d,%d,%d) binning_r(%d,%d), crop_taa(%d,%d), bds(%d,%d)\n",
			frame->instance, hw_ip, frame->fcount,
			frame->shot->udm.ni.currentFrameNoiseIndex,
			frame->shot->udm.ni.nextFrameNoiseIndex,
			frame->shot->udm.ni.nextNextFrameNoiseIndex,
			frame->shot_ext->binning_ratio_x, frame->shot_ext->binning_ratio_y,
			frame->shot_ext->crop_taa_x, frame->shot_ext->crop_taa_y,
			frame->shot_ext->bds_ratio_x, frame->shot_ext->bds_ratio_y);
	}

	return ret;
}

static int is_hw_3aa_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
		enum ShotErrorType done_type)
{
	int output_id = 0;
	int ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	if (test_bit(hw_ip->id, &frame->core_flag))
		output_id = IS_HW_CORE_END;

	ret = CALL_HW_OPS(hw_ip, frame_done, hw_ip, frame, -1,
			output_id, done_type, false);

	return ret;
}

static int is_hw_3aa_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_3aa *hw_3aa = (struct is_hw_3aa *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, load_setfile, instance, &hw_3aa->lib[instance]);
}

static int is_hw_3aa_apply_setfile(struct is_hw_ip *hw_ip, u32 scenario,
	u32 instance, ulong hw_map)
{
	int ret;
	struct is_hw_3aa *hw_3aa = (struct is_hw_3aa *)hw_ip->priv_info;

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		msdbg_hw(2, "%s: hw_map(0x%lx)\n", instance, hw_ip, __func__, hw_map);
		return 0;
	}

	ret = CALL_HW_HELPER_OPS(hw_ip, apply_setfile, instance, &hw_3aa->lib[instance], scenario);
	if (ret)
		return ret;

	ret = CALL_HW_HELPER_OPS(hw_ip, load_cal_data, instance, &hw_3aa->lib[instance]);
	if (ret)
		return ret;

	return 0;
}

static int is_hw_3aa_delete_setfile(struct is_hw_ip *hw_ip, u32 instance,
	ulong hw_map)
{
	struct is_hw_3aa *hw_3aa = (struct is_hw_3aa *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, delete_setfile, instance, &hw_3aa->lib[instance]);
}

int is_hw_3aa_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_3aa *hw_3aa = (struct is_hw_3aa *)hw_ip->priv_info;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	return CALL_HW_HELPER_OPS(hw_ip, restore, instance, &hw_3aa->lib[instance]);
}

static int is_hw_3aa_change_chain(struct is_hw_ip *hw_ip, u32 instance,
	u32 next_id, struct is_hardware *hardware)
{
	int ret = 0;
	struct is_hw_3aa *hw_3aa;
	u32 curr_id;
	u32 next_hw_id = DEV_HW_3AA0 + next_id;
	struct is_hw_ip *next_hw_ip;
	enum exynos_sensor_position sensor_position;

	curr_id = hw_ip->id - DEV_HW_3AA0;
	if (curr_id == next_id) {
		mswarn_hw("Same chain (curr:%d, next:%d)", instance, hw_ip,
			curr_id, next_id);
		goto p_err;
	}

	hw_3aa = (struct is_hw_3aa *)hw_ip->priv_info;
	if (!hw_3aa) {
		mserr_hw("failed to get HW 3AA", instance, hw_ip);
		return -ENODEV;
	}

	/* Call DDK */
	ret = is_lib_isp_change_chain(hw_ip, &hw_3aa->lib[instance], instance, next_id);
	if (ret) {
		mserr_hw("is_lib_isp_change_chain", instance, hw_ip);
		return ret;
	}

	next_hw_ip = CALL_HW_CHAIN_INFO_OPS(hardware, get_hw_ip, next_hw_id);
	if (!next_hw_ip) {
		merr_hw("[ID:%d]invalid next next_hw_id", instance, next_hw_id);
		return -EINVAL;
	}

	if (!test_and_clear_bit(instance, &hw_ip->run_rsc_state))
		mswarn_hw("try to disable disabled instance", instance, hw_ip);

	ret = is_hw_3aa_disable(hw_ip, instance, hardware->hw_map[instance]);
	if (ret) {
		msinfo_hw("is_hw_3aa_disable is fail", instance, hw_ip);
		return -EINVAL;
	}

	/*
	 * Copy instance infromation.
	 * But do not clear current hw_ip,
	 * because logical(initial) HW must be refered at close time.
	 */
	((struct is_hw_3aa *)(next_hw_ip->priv_info))->lib[instance]
		= hw_3aa->lib[instance];
	((struct is_hw_3aa *)(next_hw_ip->priv_info))->param_set[instance]
		= hw_3aa->param_set[instance];

	next_hw_ip->group[instance] = hw_ip->group[instance];
	next_hw_ip->region[instance] = hw_ip->region[instance];

	sensor_position = hardware->sensor_position[instance];
	next_hw_ip->setfile[sensor_position] = hw_ip->setfile[sensor_position];

	/* set & clear physical HW */
	set_bit(next_hw_id, &hardware->hw_map[instance]);
	clear_bit(hw_ip->id, &hardware->hw_map[instance]);

	if (test_and_set_bit(instance, &next_hw_ip->run_rsc_state))
		mswarn_hw("try to enable enabled instance", instance, next_hw_ip);

	ret = is_hw_3aa_enable(next_hw_ip, instance, hardware->hw_map[instance]);
	if (ret) {
		msinfo_hw("is_hw_3aa_enable is fail", instance, next_hw_ip);
		return -EINVAL;
	}

	/*
	 * There is no change about rsccount when change_chain processed
	 * because there is no open/close operation.
	 * But if it isn't increased, abnormal situation can be occurred
         * according to hw close order among instances.
	 */
	if (!test_bit(hw_ip->id, &hardware->logical_hw_map[instance])) {
		atomic_dec(&hw_ip->rsccount);
		msinfo_hw("decrease hw_ip rsccount(%d)", instance, hw_ip, atomic_read(&hw_ip->rsccount));
	}

	if (!test_bit(next_hw_ip->id, &hardware->logical_hw_map[instance])) {
		atomic_inc(&next_hw_ip->rsccount);
		msinfo_hw("increase next_hw_ip rsccount(%d)", instance, next_hw_ip, atomic_read(&next_hw_ip->rsccount));
	}

	/* HACK: for DDK */
	hw_ip->changed_hw_ip[instance] = next_hw_ip;

	msinfo_hw("change_chain done (state: curr(0x%lx) next(0x%lx))", instance, hw_ip,
		hw_ip->state, next_hw_ip->state);
p_err:
	return ret;
}

static int is_hw_3aa_get_offline_data(struct is_hw_ip *hw_ip, u32 instance,
	void *data_desc, int fcount)
{
	int ret = 0;
	struct is_hw_3aa *hw_3aa;

	hw_3aa = (struct is_hw_3aa *)hw_ip->priv_info;
	if (!hw_3aa) {
		mserr_hw("failed to get HW 3AA", instance, hw_ip);
		return -ENODEV;
	}

	ret = is_lib_get_offline_data(&hw_3aa->lib[instance], instance, data_desc,
			fcount);

	return ret;
}

static int is_hw_3aa_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_3aa *hw_3aa = NULL;

	hw_3aa = (struct is_hw_3aa *)hw_ip->priv_info;
	if (!hw_3aa) {
		mserr_hw("failed to get HW 3AA", instance, hw_ip);
		return -ENODEV;
	}

	return CALL_HW_HELPER_OPS(hw_ip, notify_timeout, instance, &hw_3aa->lib[instance]);
}

const struct is_hw_ip_ops is_hw_3aa_ops = {
	.open			= is_hw_3aa_open,
	.init			= is_hw_3aa_init,
	.deinit			= is_hw_3aa_deinit,
	.close			= is_hw_3aa_close,
	.enable			= is_hw_3aa_enable,
	.disable		= is_hw_3aa_disable,
	.shot			= is_hw_3aa_shot,
	.set_param		= is_hw_3aa_set_param,
	.get_meta		= is_hw_3aa_get_meta,
	.frame_ndone		= is_hw_3aa_frame_ndone,
	.load_setfile		= is_hw_3aa_load_setfile,
	.apply_setfile		= is_hw_3aa_apply_setfile,
	.delete_setfile		= is_hw_3aa_delete_setfile,
	.restore		= is_hw_3aa_restore,
	.sensor_start		= is_hw_3aa_sensor_start,
	.change_chain		= is_hw_3aa_change_chain,
	.get_offline_data	= is_hw_3aa_get_offline_data,
	.notify_timeout		= is_hw_3aa_notify_timeout,
};

int is_hw_3aa_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int ret = 0;

	hw_ip->ops  = &is_hw_3aa_ops;

	return ret;
}
