// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-hw-helper.h"
#include "is-hw-mcfp.h"
#include "is-err.h"
#include "api/is-hw-api-mcfp.h"
#include "is-stripe.h"

static int debug_mcfp;
module_param(debug_mcfp, int, 0644);

#define GET_MCFP_CAP_NODE_IDX(video_num) (video_num - IS_LVN_MCFP_PREV_YUV)

static int is_hw_mcfp_handle_interrupt(u32 id, void *context)
{
	struct is_hardware *hardware;
	struct is_hw_ip *hw_ip;
	struct is_hw_mcfp *hw_mcfp;
	struct mcfp_param_set *param_set;
	u32 status, instance, hw_fcount, strip_index, strip_total_count, hl = 0, vl = 0;
	u32 f_err;
	u32 cur_idx, batch_num;
	bool is_first_shot = true, is_last_shot = true;

	hw_ip = (struct is_hw_ip *)context;

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	param_set = &hw_mcfp->param_set[instance];
	strip_index = param_set->stripe_input.index;
	strip_total_count = param_set->stripe_input.total_count;

	cur_idx = (hw_ip->num_buffers >> CURR_INDEX_SHIFT) & 0xff;
	batch_num = (hw_ip->num_buffers >> SW_FRO_NUM_SHIFT) & 0xff;

	if (IS_ENABLED(SKIP_ISP_SHOT_FOR_MULTI_SHOT)) {
		/* SW batch mode */
		if (!pablo_is_first_shot(batch_num, cur_idx)) is_first_shot = false;
		if (!pablo_is_last_shot(batch_num, cur_idx)) is_last_shot = false;
		/* Stripe processing */
		if (!pablo_is_first_shot(strip_total_count, strip_index)) is_first_shot = false;
		if (!pablo_is_last_shot(strip_total_count, strip_index)) is_last_shot = false;
		/* Repeat processing */
		if (!pablo_is_first_shot(param_set->stripe_input.repeat_num,
			param_set->stripe_input.repeat_idx)) is_first_shot = false;
		if (!pablo_is_last_shot(param_set->stripe_input.repeat_num,
			param_set->stripe_input.repeat_idx)) is_last_shot = false;
	}

	status = mcfp_hw_g_int_state(hw_ip->regs[REG_SETA], true,
			(hw_ip->num_buffers & 0xffff),
			MCFP_INTR_0, &hw_mcfp->irq_state[MCFP_INTR_0])
			& mcfp_hw_g_int_mask(hw_ip->regs[REG_SETA], MCFP_INTR_0);

	msdbg_hw(2, "MCFP interrupt status(0x%x)\n", instance, hw_ip, status);

	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("invalid interrupt: 0x%x", instance, hw_ip, status);
		return 0;
	}

	if (test_bit(HW_OVERFLOW_RECOVERY, &hardware->hw_recovery_flag)) {
		mserr_hw("During recovery : invalid interrupt", instance, hw_ip);
		return 0;
	}

	if (!test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("HW disabled!! interrupt(0x%x)", instance, hw_ip, status);
		return 0;
	}

	if (mcfp_hw_is_occurred(status, INTR_SETTING_DONE))
		CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_SETTING_DONE);

	if (mcfp_hw_is_occurred(status, INTR_FRAME_START) && mcfp_hw_is_occurred(status, INTR_FRAME_END))
		mswarn_hw("start/end overlapped!! (0x%x)", instance, hw_ip, status);

	if (mcfp_hw_is_occurred(status, INTR_FRAME_START)) {
		if (IS_ENABLED(MCFP_DDK_LIB_CALL) && is_first_shot) {
			is_lib_isp_event_notifier(hw_ip, &hw_mcfp->lib[instance],
					instance, hw_fcount, EVENT_FRAME_START,
					strip_index, NULL);
		} else {
			atomic_add((hw_ip->num_buffers & 0xffff), &hw_ip->count.fs);
			CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount,
					DEBUG_POINT_FRAME_START);
			if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
				msinfo_hw("[F:%d]F.S\n", instance, hw_ip,
						hw_fcount);

			CALL_HW_OPS(hw_ip, frame_start, hw_ip, instance);
		}
	}

	if (mcfp_hw_is_occurred(status, INTR_FRAME_END)) {
		if (IS_ENABLED(MCFP_DDK_LIB_CALL) && is_last_shot) {
			is_lib_isp_event_notifier(hw_ip, &hw_mcfp->lib[instance],
					instance, hw_fcount, EVENT_FRAME_END,
					strip_index, NULL);
		} else {
			CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount,
					DEBUG_POINT_FRAME_END);
			atomic_add((hw_ip->num_buffers & 0xffff), &hw_ip->count.fe);

			CALL_HW_OPS(hw_ip, frame_done, hw_ip, NULL, -1, IS_HW_CORE_END,
					IS_SHOT_SUCCESS, true);

			if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
				msinfo_hw("[F:%d]F.E\n", instance, hw_ip,
						hw_fcount);

			atomic_set(&hw_ip->status.Vvalid, V_BLANK);
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
	}

	f_err = mcfp_hw_is_occurred(status, INTR_ERR);

	if (f_err) {
		msinfo_hw("[F:%d] Ocurred error interrupt (%d,%d) status(0x%x)\n",
			instance, hw_ip, hw_fcount, hl, vl, status);
		mcfp_hw_dump(hw_ip->regs[REG_SETA]);
	}

	return 0;
}

static int __is_hw_mcfp_s_reset(struct is_hw_ip *hw_ip, struct is_hw_mcfp *hw_mcfp)
{
	int i;

	for (i = 0; i < COREX_MAX; i++)
		hw_ip->cur_hw_iq_set[i].size = 0;

	return mcfp_hw_s_reset(hw_ip->regs[REG_SETA]);
}

static int __is_hw_mcfp_s_common_reg(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_mcfp *hw_mcfp;
	u32 seed;

	if (!hw_ip) {
		err("hw_ip is NULL");
		return -ENODEV;
	}

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	msinfo_hw("reset\n", instance, hw_ip);
	if (__is_hw_mcfp_s_reset(hw_ip, hw_mcfp)) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}

	mcfp_hw_s_cmdq_init(hw_ip->regs[REG_SETA]);
	mcfp_hw_s_init(hw_ip->regs[REG_SETA]);
	mcfp_hw_s_cmdq_enable(hw_ip->regs[REG_SETA], 1);

	seed = is_get_debug_param(IS_DEBUG_PARAM_CRC_SEED);
	if (unlikely(seed))
		mcfp_hw_s_crc(hw_ip->regs[REG_SETA], seed);

	msinfo_hw("clear interrupt\n", instance, hw_ip);

	return 0;
}

static int __is_hw_mcfp_clear_common(struct is_hw_ip *hw_ip, u32 instance)
{
	int res;
	struct is_hw_mcfp *hw_mcfp;

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	if (__is_hw_mcfp_s_reset(hw_ip, hw_mcfp)) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}

	res = mcfp_hw_wait_idle(hw_ip->regs[REG_SETA]);
	if (res)
		mserr_hw("failed to mcfp_hw_wait_idle", instance, hw_ip);

	msinfo_hw("final finished mcfp\n", instance, hw_ip);

	return res;
}

static int is_hw_mcfp_get_lossy_byte32num(u32 bit)
{
	if (bit == 8)
		return COMP_LOSSYBYTE32NUM_32X4_U8;
	else if (bit == 10)
		return COMP_LOSSYBYTE32NUM_32X4_U10;
	else
		return COMP_LOSSYBYTE32NUM_32X4_U12;
}

static int is_hw_mcfp_get_y_sbwc_type(u32 sbwc_type)
{
	if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
		return DMA_INPUT_SBWC_LOSSY_32B;
	else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
		return DMA_INPUT_SBWC_LOSSY_64B;
	else
		return sbwc_type;
}

static int is_hw_mcfp_get_uv_sbwc_type(u32 sbwc_type)
{
	if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
		return DMA_INPUT_SBWC_LOSSYLESS_32B;
	else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
		return DMA_INPUT_SBWC_LOSSYLESS_64B;
	else
		return sbwc_type;
}

static int is_hw_mcfp_get_buf_info(enum is_hw_mcfp_subdev buf_id,
				struct pablo_internal_subdev *sd,
				struct mcfp_param_set *param_set,
				struct is_mcfp_config *config,
				u32 scenario)
{
	u32 width = param_set->dma_output_yuv.width;
	u32 height = param_set->dma_output_yuv.height;
	u32 sbwc_type = param_set->dma_output_yuv.sbwc_type;
	u32 i, sbwc_t, sbwc_en, comp_64b_align = 1, lossy_byte32num;
	u32 payload_size, header_size;
	bool header_en;

	switch (buf_id) {
	case MCFP_SUBDEV_PREV_YUV:
	case MCFP_SUBDEV_YUV:
		if (config->still_en)
			return -EINVAL;

		sd->width = width;
		sd->height = height;
		sd->num_planes = 2;
		sd->num_batch = 1;
		sd->num_buffers = 1;
		sd->bits_per_pixel = config->img_bit;
		sd->memory_bitwidth = config->img_bit;

		lossy_byte32num = is_hw_mcfp_get_lossy_byte32num(config->img_bit);

		for (i = 0; i < sd->num_planes; i++) {
			sbwc_t = (i == 0) ?
				is_hw_mcfp_get_y_sbwc_type(sbwc_type) :
				is_hw_mcfp_get_uv_sbwc_type(sbwc_type);

			sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_t, &comp_64b_align);
			if (sbwc_en == NONE) {
				sbwc_en = COMP;
				comp_64b_align = 1;
			}

			if (IS_ENABLED(CONFIG_COMMON_DMA_V1_0))
				header_en = (sbwc_en == COMP) ? true : false;
			else
				header_en = true;

			payload_size = is_hw_dma_get_payload_stride(
				sbwc_en, sd->bits_per_pixel, width,
				comp_64b_align, lossy_byte32num,
				MCFP_COMP_BLOCK_WIDTH, MCFP_COMP_BLOCK_HEIGHT)
				* DIV_ROUND_UP(height, MCFP_COMP_BLOCK_HEIGHT);

			header_size = header_en ?
				is_hw_dma_get_header_stride(width, MCFP_COMP_BLOCK_WIDTH, 16)
				* DIV_ROUND_UP(height, MCFP_COMP_BLOCK_HEIGHT) : 0;

			sd->size[i] = payload_size + header_size;
		}

		sd->secure = (scenario == IS_SCENARIO_SECURE) ? true : false;
		break;
	case MCFP_SUBDEV_PREV_W:
	case MCFP_SUBDEV_W:
		sd->width = width;
		sd->height = height;
		sd->num_planes = 1;
		sd->num_batch = 1;
		sd->num_buffers = 1;
		sd->bits_per_pixel = 8;
		sd->memory_bitwidth = 8;
		sd->size[0] = ALIGN(width / 2, 16) * ((height / 2) + 4);
		break;
	default:
		mserr("invalid buf_id(%d)", sd, sd, buf_id);
		return -EINVAL;
	}

	return 0;
}

static void __is_hw_mcfp_free_buffer(u32 instance, struct is_hw_ip *hw_ip)
{
	enum is_hw_mcfp_subdev buf_id;
	struct is_hw_mcfp *hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
	struct pablo_internal_subdev *sd;

	for (buf_id = 0; buf_id < MCFP_SUBDEV_END; buf_id++) {
		sd = &hw_mcfp->subdev[instance][buf_id];

		if (!test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
			continue;

		if (CALL_I_SUBDEV_OPS(sd, free, sd))
			mserr_hw("[%s] failed to free", instance, hw_ip, sd->name);
	}
}

static int __is_hw_mcfp_alloc_buffer(u32 instance, struct is_hw_ip *hw_ip,
					struct mcfp_param_set *param_set,
					struct is_mcfp_config *config)
{
	int ret;
	enum is_hw_mcfp_subdev buf_id;
	struct pablo_internal_subdev *sd;
	struct is_hw_mcfp *hw = (struct is_hw_mcfp *)hw_ip->priv_info;
	u32 scenario = hw_ip->region[instance]->parameter.sensor.config.scenario;

	for (buf_id = 0; buf_id < MCFP_SUBDEV_END; buf_id++) {
		sd = &hw->subdev[instance][buf_id];

		if (test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
			continue;

		if (is_hw_mcfp_get_buf_info(buf_id, sd, param_set, config, scenario))
			continue;

		ret = CALL_I_SUBDEV_OPS(sd, alloc, sd);
		if (ret) {
			mserr_hw("[%s] failed to alloc(%d)", instance, hw_ip, sd->name, ret);
			goto err_sd;
		}
	}

	return 0;

err_sd:
	while (buf_id-- > 0) {
		sd = &hw->subdev[instance][buf_id];
		if (CALL_I_SUBDEV_OPS(sd, free, sd))
			mserr_hw("[%s] failed to free", instance, hw_ip, sd->name);
	}

	return ret;
}

static int is_hw_mcfp_set_cmdq(struct is_hw_ip *hw_ip, u32 num_buffers)
{
	mcfp_hw_s_cmdq_queue(hw_ip->regs[REG_SETA], num_buffers, 0, 0);

	return 0;
}

static int __nocfi is_hw_mcfp_open(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;
	struct is_hw_mcfp *hw_mcfp;

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HWMCFP");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw_ip->priv_info = vzalloc(sizeof(struct is_hw_mcfp));
	if (!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
	hw_mcfp->instance = instance;

	ret = CALL_HW_HELPER_OPS(hw_ip, open, instance, &hw_mcfp->lib[instance],
				LIB_FUNC_MCFP);
	if (ret)
		goto err_chain_create;

	ret = CALL_HW_HELPER_OPS(hw_ip, alloc_iqset, mcfp_hw_g_reg_cnt());
	if (ret)
		goto err_iqset_alloc;

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	set_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "open: framemgr[%s]", instance, hw_ip, hw_ip->framemgr->name);

	return 0;

err_iqset_alloc:
	CALL_HW_HELPER_OPS(hw_ip, close, instance, &hw_mcfp->lib[instance]);

err_chain_create:
	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_mcfp_init(struct is_hw_ip *hw_ip, u32 instance,
			bool flag, u32 f_type)
{
	int ret;
	struct is_hw_mcfp *hw_mcfp;
	u32 input_id;
	enum is_hw_mcfp_subdev buf_id;
	struct is_mem *mem;

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
	if (!hw_mcfp) {
		mserr_hw("hw_mcfp is null ", instance, hw_ip);
		ret = -ENODATA;
		goto err;
	}

	ret = CALL_HW_HELPER_OPS(hw_ip, init, instance, &hw_mcfp->lib[instance],
				(u32)flag, f_type, LIB_FUNC_MCFP);
	if (ret)
		return ret;

	for (input_id = MCFP_RDMA_CUR_IN_Y; input_id < MCFP_RDMA_MAX; input_id++) {
		ret = mcfp_hw_rdma_create(&hw_mcfp->rdma[input_id], hw_ip->regs[REG_SETA], input_id);
		if (ret) {
			mserr_hw("mcfp_hw_rdma_create error[%d]", instance, hw_ip, input_id);
			ret = -ENODATA;
			goto err;
		}
	}

	for (input_id = MCFP_WDMA_PREV_OUT_Y; input_id < MCFP_WDMA_MAX; input_id++) {
		ret = mcfp_hw_wdma_create(&hw_mcfp->wdma[input_id], hw_ip->regs[REG_SETA], input_id);
		if (ret) {
			mserr_hw("mcfp_hw_wdma_create error[%d]", instance, hw_ip, input_id);
			ret = -ENODATA;
			goto err;
		}
	}

	mem = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_iommu_mem, GROUP_ID_MCFP);
	for (buf_id = 0; buf_id < MCFP_SUBDEV_END; buf_id++)
		pablo_internal_subdev_probe(&hw_mcfp->subdev[instance][buf_id],
					instance, ENTRY_INTERNAL, mem,
					mcfp_internal_buf_name[buf_id]);

	set_bit(HW_INIT, &hw_ip->state);
	return 0;

err:
	return ret;

}

static int is_hw_mcfp_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_mcfp *hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, deinit, instance, &hw_mcfp->lib[instance]);
}

static int is_hw_mcfp_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_mcfp *hw_mcfp = NULL;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	CALL_HW_HELPER_OPS(hw_ip, close, instance, &hw_mcfp->lib[instance]);

	__is_hw_mcfp_clear_common(hw_ip, instance);

	CALL_HW_HELPER_OPS(hw_ip, free_iqset);

	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	clear_bit(HW_OPEN, &hw_ip->state);


	return 0;
}

static int is_hw_mcfp_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_mcfp *hw_mcfp;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	if (test_bit(HW_RUN, &hw_ip->state))
		return 0;

	msdbg_hw(2, "enable: start\n", instance, hw_ip);

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	__is_hw_mcfp_s_common_reg(hw_ip, instance);

	memset(&hw_mcfp->config[instance], 0x0, sizeof(struct is_mcfp_config));

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return 0;
}

static int is_hw_mcfp_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_mcfp *hw_mcfp;
	struct mcfp_param_set *param_set;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("mcfp_disable: Vvalid(%d)\n", instance, hw_ip,
		atomic_read(&hw_ip->status.Vvalid));

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	timetowait = wait_event_timeout(hw_ip->status.wait_queue,
		!atomic_read(&hw_ip->status.Vvalid),
		IS_HW_STOP_TIMEOUT);

	if (!timetowait)
		mserr_hw("wait FRAME_END timeout (%ld)", instance,
			hw_ip, timetowait);

	param_set = &hw_mcfp->param_set[instance];
	param_set->fcount = 0;

	CALL_HW_HELPER_OPS(hw_ip, disable, instance, &hw_mcfp->lib[instance]);

	__is_hw_mcfp_free_buffer(instance, hw_ip);

	if (hw_ip->run_rsc_state)
		return 0;

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static void __is_hw_mcfp_set_dma_cmd(struct mcfp_param_set *param_set, u32 instance,
					struct is_mcfp_config *conf)
{
	if (conf->mixer_enable) {
		if (conf->still_en) {
			/* MF-Still */
			param_set->dma_input_sf.cmd = DMA_INPUT_COMMAND_ENABLE;
			param_set->dma_input_prev_w.cmd = DMA_INPUT_COMMAND_ENABLE;
			param_set->dma_output_w.cmd = DMA_OUTPUT_COMMAND_ENABLE;
		} else {
			/* Video TNR */
			param_set->dma_output_yuv.cmd = DMA_OUTPUT_COMMAND_ENABLE;
			switch (conf->mixer_mode) {
			case MCFP_TNR_MODE_PREPARE:
				param_set->dma_input_mv.cmd = DMA_INPUT_COMMAND_DISABLE;
				param_set->dma_input_prev_yuv.cmd = DMA_INPUT_COMMAND_DISABLE;
				param_set->dma_input_prev_w.cmd = DMA_INPUT_COMMAND_DISABLE;
				param_set->dma_output_w.cmd = DMA_OUTPUT_COMMAND_DISABLE;
				break;
			case MCFP_TNR_MODE_FIRST:
				param_set->dma_input_prev_yuv.cmd = DMA_INPUT_COMMAND_ENABLE;
				param_set->dma_input_prev_w.cmd = DMA_INPUT_COMMAND_DISABLE;
				param_set->dma_output_w.cmd = DMA_OUTPUT_COMMAND_ENABLE;
				break;
			case MCFP_TNR_MODE_NORMAL:
				param_set->dma_input_prev_yuv.cmd = DMA_INPUT_COMMAND_ENABLE;
				param_set->dma_input_prev_w.cmd = DMA_INPUT_COMMAND_ENABLE;
				param_set->dma_output_w.cmd = DMA_OUTPUT_COMMAND_ENABLE;
				break;
			default:
				break;
			}

			/* 1/2 dma */
			if (conf->skip_wdma && (conf->mixer_mode == MCFP_TNR_MODE_NORMAL)) {
				param_set->dma_output_yuv.cmd = DMA_OUTPUT_COMMAND_DISABLE;
				param_set->dma_output_w.cmd = DMA_OUTPUT_COMMAND_DISABLE;
			}
		}
	} else {
		param_set->dma_input_prev_yuv.cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->dma_input_prev_w.cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->dma_input_mv.cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->dma_output_yuv.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->dma_output_w.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	}
}

static int __is_hw_mcfp_set_rdma(struct is_hw_ip *hw_ip, struct is_hw_mcfp *hw_mcfp,
	struct mcfp_param_set *param_set, u32 instance, u32 id, u32 set_id)
{
	pdma_addr_t *input_dva;
	u32 comp_sbwc_en, payload_size, strip_offset = 0, header_offset = 0;
	struct param_dma_input *dma_input;
	u32 frame_width, frame_height;
	int ret;

	frame_width = param_set->dma_input.width;
	frame_height = param_set->dma_input.height;

	switch (id) {
	case MCFP_RDMA_CUR_IN_Y:
	case MCFP_RDMA_CUR_IN_UV:
		input_dva = param_set->input_dva;
		dma_input = &param_set->dma_input;
		break;
	case MCFP_RDMA_PRE_MV:
	case MCFP_RDMA_GEOMATCH_MV:
	case MCFP_RDMA_MIXER_MV:
		input_dva = param_set->input_dva_mv;
		dma_input = &param_set->dma_input_mv;
		break;
	case MCFP_RDMA_CUR_DRC:
		input_dva = param_set->input_dva_drc_cur_map;
		dma_input = &param_set->dma_input_drc_cur_map;
		break;
	case MCFP_RDMA_PREV_DRC:
		input_dva = param_set->input_dva_drc_prv_map;
		dma_input = &param_set->dma_input_drc_prv_map;
		break;
	case MCFP_RDMA_SAT_FLAG:
		input_dva = param_set->input_dva_sf;
		dma_input = &param_set->dma_input_sf;
		break;
	case MCFP_RDMA_PREV_IN0_Y:
	case MCFP_RDMA_PREV_IN0_UV:
	case MCFP_RDMA_PREV_IN1_Y:
	case MCFP_RDMA_PREV_IN1_UV:
		input_dva = param_set->input_dva_prv_yuv;
		dma_input = &param_set->dma_input_prev_yuv;
		break;
	case MCFP_RDMA_PREV_IN_WGT:
		input_dva = param_set->input_dva_prv_w;
		dma_input = &param_set->dma_input_prev_w;
		break;
	case MCFP_RDMA_CUR_IN_WGT:
		return 0;	/* does not support current weight RDMA */
	default:
		merr_hw("invalid ID (%d)", instance,  id);
		return -EINVAL;
	}

	msdbg_hw(2, "%s: %d\n", instance, hw_ip, hw_mcfp->rdma[id].name, dma_input->cmd);

	mcfp_hw_s_dma_corex_id(&hw_mcfp->rdma[id], set_id);

	ret = mcfp_hw_s_rdma_init(&hw_mcfp->rdma[id], dma_input,
		&param_set->stripe_input,
		frame_width, frame_height, &comp_sbwc_en, &payload_size,
		&strip_offset, &header_offset, &hw_mcfp->config[instance]);
	if (ret) {
		mserr_hw("failed to initialize MCFP_RDMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (dma_input->cmd == DMA_INPUT_COMMAND_ENABLE) {
		ret = mcfp_hw_s_rdma_addr(&hw_mcfp->rdma[id], input_dva, 0,
			(hw_ip->num_buffers & 0xffff),
			0, comp_sbwc_en, payload_size, strip_offset, header_offset);
		if (ret) {
			mserr_hw("failed to set MCFP_RDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
	}

	return 0;
}

static int __is_hw_mcfp_set_wdma(struct is_hw_ip *hw_ip, struct is_hw_mcfp *hw_mcfp,
	struct mcfp_param_set *param_set, u32 instance, u32 id, u32 set_id)
{
	u32 *output_dva;
	u32 comp_sbwc_en, payload_size, strip_offset = 0, header_offset = 0;
	struct param_dma_output *dma_output;
	u32 frame_width, frame_height;
	int ret;

	frame_width = param_set->dma_input.width;
	frame_height = param_set->dma_input.height;

	switch (id) {
	case MCFP_WDMA_PREV_OUT_Y:
	case MCFP_WDMA_PREV_OUT_UV:
		output_dva = param_set->output_dva_yuv;
		dma_output = &param_set->dma_output_yuv;
		break;
	case MCFP_WDMA_PREV_OUT_WGT:
		output_dva = param_set->output_dva_w;
		dma_output = &param_set->dma_output_w;
		break;
	default:
		merr_hw("invalid ID (%d)", instance,  id);
		return -EINVAL;
	}

	msdbg_hw(2, "%s: %d\n", instance, hw_ip, hw_mcfp->wdma[id].name, dma_output->cmd);

	mcfp_hw_s_dma_corex_id(&hw_mcfp->wdma[id], set_id);

	ret = mcfp_hw_s_wdma_init(&hw_mcfp->wdma[id], dma_output,
		&param_set->stripe_input,
		frame_width, frame_height,
		&comp_sbwc_en, &payload_size, &strip_offset, &header_offset,
		&hw_mcfp->config[instance]);
	if (ret) {
		mserr_hw("failed to initialize MCFP_WDMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (dma_output->cmd == DMA_INPUT_COMMAND_ENABLE) {
		ret = mcfp_hw_s_wdma_addr(&hw_mcfp->wdma[id], output_dva, 0,
			(hw_ip->num_buffers & 0xffff),
			0, comp_sbwc_en, payload_size, strip_offset, header_offset);
		if (ret) {
			mserr_hw("failed to set MCFP_WDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
	}

	return 0;
}

static int __is_hw_mcfp_bypass(struct is_hw_ip *hw_ip, u32 set_id)
{
	mcfp_hw_s_block_bypass(hw_ip->regs[REG_SETA], set_id);

	return 0;
}


static int __is_hw_mcfp_update_block_reg(struct is_hw_ip *hw_ip,
	struct mcfp_param_set *param_set, u32 instance, u32 set_id)
{
	struct is_hw_mcfp *hw_mcfp;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	if (hw_mcfp->instance != instance) {
		msdbg_hw(2, "update_param: hw_ip->instance(%d)\n",
			instance, hw_ip, hw_mcfp->instance);
		hw_mcfp->instance = instance;
	}

	param_set->dma_input_prev_yuv.cmd = DMA_INPUT_COMMAND_DISABLE;
	param_set->dma_input_prev_w.cmd = DMA_INPUT_COMMAND_DISABLE;
	param_set->dma_input_mv.cmd = DMA_INPUT_COMMAND_DISABLE;
	param_set->dma_output_yuv.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	param_set->dma_output_w.cmd = DMA_OUTPUT_COMMAND_DISABLE;

	return __is_hw_mcfp_bypass(hw_ip, set_id);
}

static void __is_hw_mcfp_update_param(struct is_hw_ip *hw_ip, struct is_param_region *p_region,
					struct mcfp_param_set *param_set, IS_DECLARE_PMAP(pmap),
					u32 instance)
{
	struct mcfp_param *param;

	param = &p_region->mcfp;
	param_set->instance_id = instance;

	if (test_bit(PARAM_MCFP_OTF_INPUT, pmap))
		memcpy(&param_set->otf_input, &param->otf_input,
				sizeof(struct param_otf_input));

	if (test_bit(PARAM_MCFP_OTF_OUTPUT, pmap))
		memcpy(&param_set->otf_output, &param->otf_output,
			sizeof(struct param_otf_output));

	if (test_bit(PARAM_MCFP_DMA_INPUT, pmap))
		memcpy(&param_set->dma_input, &param->dma_input,
				sizeof(struct param_dma_input));

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && test_bit(PARAM_MCFP_STRIPE_INPUT, pmap))
		memcpy(&param_set->stripe_input, &param->stripe_input,
				sizeof(struct param_stripe_input));

	if (test_bit(PARAM_MCFP_PREV_YUV, pmap))
		memcpy(&param_set->dma_input_prev_yuv, &param->prev_yuv,
				sizeof(struct param_dma_input));

	if (test_bit(PARAM_MCFP_PREV_W, pmap))
		memcpy(&param_set->dma_input_prev_w, &param->prev_w,
				sizeof(struct param_dma_input));

	if (test_bit(PARAM_MCFP_DRC, pmap))
		memcpy(&param_set->dma_input_drc_cur_map, &param->drc,
				sizeof(struct param_dma_input));

	if (test_bit(PARAM_MCFP_DP, pmap))
		memcpy(&param_set->dma_input_drc_prv_map, &param->dp,
				sizeof(struct param_dma_input));

	if (test_bit(PARAM_MCFP_MV, pmap))
		memcpy(&param_set->dma_input_mv, &param->mv,
				sizeof(struct param_dma_input));

	if (test_bit(PARAM_MCFP_SF, pmap))
		memcpy(&param_set->dma_input_sf, &param->sf,
				sizeof(struct param_dma_input));

	if (test_bit(PARAM_MCFP_W, pmap))
		memcpy(&param_set->dma_output_w, &param->w,
				sizeof(struct param_dma_output));

	if (test_bit(PARAM_MCFP_YUV, pmap))
		memcpy(&param_set->dma_output_yuv, &param->yuv,
				sizeof(struct param_dma_output));
}

static int is_hw_mcfp_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
		IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	struct is_hw_mcfp *hw_mcfp;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
	hw_mcfp->instance = IS_STREAM_COUNT;

	return 0;
}

static int __is_hw_mcfp_set_size_regs(struct is_hw_ip *hw_ip, struct mcfp_param_set *param_set,
	u32 instance, const struct is_frame *frame, u32 set_id)
{
	struct is_hw_mcfp *hw_mcfp;
	struct is_mcfp_config *mcfp_config;
	u32 strip_enable;
	u32 crop_img_width, crop_wgt_width;
	u32 crop_img_x, crop_wgt_x;
	u32 crop_img_start_x;
	u32 crop_margin_for_align;
	u32 dma_width, dma_height;
	u32 frame_width;
	u32 strip_start_pos;
	u32 stripe_index;
	u32 region_id;
	u32 img_shift_bit;
	u32 wgt_shift_bit;
	struct mcfp_radial_cfg radial_cfg;

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
	mcfp_config = &hw_mcfp->config[instance];

	strip_enable = (param_set->stripe_input.total_count < 2) ? 0 : 1;

	region_id = param_set->stripe_input.index;
	frame_width = param_set->stripe_input.full_width;
	dma_width = param_set->dma_input.width;
	dma_height = param_set->dma_input.height;
	frame_width = (strip_enable) ? param_set->stripe_input.full_width : dma_width;
	stripe_index = param_set->stripe_input.index;
	strip_start_pos = (stripe_index) ? (param_set->stripe_input.start_pos_x) : 0;

	radial_cfg.sensor_full_width = frame->shot->udm.frame_info.sensor_size[0];
	radial_cfg.sensor_full_height = frame->shot->udm.frame_info.sensor_size[1];
	radial_cfg.sensor_crop_x = frame->shot->udm.frame_info.sensor_crop_region[0];
	radial_cfg.sensor_crop_y = frame->shot->udm.frame_info.sensor_crop_region[1];
	radial_cfg.taa_crop_x = frame->shot->udm.frame_info.taa_in_crop_region[0];
	radial_cfg.taa_crop_y = frame->shot->udm.frame_info.taa_in_crop_region[1];
	radial_cfg.taa_crop_width = frame->shot->udm.frame_info.taa_in_crop_region[2];
	radial_cfg.taa_crop_height = frame->shot->udm.frame_info.taa_in_crop_region[3];
	radial_cfg.sensor_binning_x = frame->shot->udm.frame_info.sensor_binning[0];
	radial_cfg.sensor_binning_y = frame->shot->udm.frame_info.sensor_binning[1];
	radial_cfg.bns_binning_x = frame->shot->udm.frame_info.bns_binning[0];
	radial_cfg.bns_binning_y = frame->shot->udm.frame_info.bns_binning[1];

	mcfp_hw_s_otf_input(hw_ip->regs[REG_SETA], set_id, param_set->otf_input.cmd);
	mcfp_hw_s_nr_otf_input(hw_ip->regs[REG_SETA], set_id, 0);
	mcfp_hw_s_otf_output(hw_ip->regs[REG_SETA], set_id, param_set->otf_output.cmd);

	mcfp_hw_s_input_size(hw_ip->regs[REG_SETA], set_id, dma_width, dma_height);
	mcfp_hw_s_geomatch_size(hw_ip->regs[REG_SETA], set_id, frame_width,
				dma_width, dma_height,
				strip_enable, strip_start_pos,
				mcfp_config);
	mcfp_hw_s_mixer_size(hw_ip->regs[REG_SETA], set_id, frame_width,
		dma_width, dma_height, strip_enable, strip_start_pos,
		&radial_cfg, mcfp_config);

	mcfp_hw_s_crop_clean_img_otf(hw_ip->regs[REG_SETA], set_id, 0, dma_width);
	mcfp_hw_s_crop_wgt_otf(hw_ip->regs[REG_SETA], set_id, 0, dma_width);

	if (strip_enable) {
		crop_img_start_x = ALIGN_DOWN(strip_start_pos + param_set->stripe_input.left_margin,
						MCFP_COMP_BLOCK_WIDTH * 2);
		crop_margin_for_align = strip_start_pos + param_set->stripe_input.left_margin -
						 crop_img_start_x;
		crop_img_x = param_set->stripe_input.left_margin - crop_margin_for_align;
		crop_img_width = dma_width - param_set->stripe_input.left_margin -
				param_set->stripe_input.right_margin + crop_margin_for_align;

		crop_wgt_x = param_set->stripe_input.left_margin >> 1;
		crop_wgt_width = (dma_width - param_set->stripe_input.left_margin -
					param_set->stripe_input.right_margin) >> 1;
	} else {
		crop_img_start_x = 0;
		crop_img_x = 0;
		crop_img_width = dma_width;

		crop_wgt_x = 0;
		crop_wgt_width = crop_img_width >> 1;
	}
	param_set->dma_output_yuv.dma_crop_offset_x = crop_img_start_x;
	param_set->dma_output_yuv.dma_crop_width = crop_img_width;

	param_set->dma_input.dma_crop_offset = strip_start_pos;
	param_set->dma_input.dma_crop_width = crop_img_width + param_set->stripe_input.left_margin
		+ param_set->stripe_input.right_margin;

	msdbgs_hw(3, "in_crop_ofs %d, in_crop_width %d, out_crop_ofs %d, out_crop_width %d, \
		left_margin %d, right_margin %d\n",
		instance, hw_ip, strip_start_pos, param_set->dma_input.dma_crop_width,
		crop_img_start_x, crop_img_width,
		param_set->stripe_input.left_margin, param_set->stripe_input.right_margin);

	mcfp_hw_s_crop_clean_img_dma(hw_ip->regs[REG_SETA], set_id,
					crop_img_x, crop_img_width, dma_height,
					!strip_enable);
	mcfp_hw_s_crop_wgt_dma(hw_ip->regs[REG_SETA], set_id,
				crop_wgt_x, crop_wgt_width, dma_height >> 1,
				!strip_enable);

	img_shift_bit = DMA_INOUT_BIT_WIDTH_12BIT - mcfp_config->img_bit;
	wgt_shift_bit = DMA_INOUT_BIT_WIDTH_8BIT - mcfp_config->wgt_bit;
	mcfp_hw_s_img_bitshift(hw_ip->regs[REG_SETA], set_id, img_shift_bit);
	mcfp_hw_s_wgt_bitshift(hw_ip->regs[REG_SETA], set_id, wgt_shift_bit);

	return 0;
}

/*
 * HW Guide
 * The sat_flag input DMA have to be enable some HDR mode as a dummy.
 */
static void is_hw_mcfp_set_sf_i_buffer(int instance, struct is_hw_ip *hw_ip,
					struct is_hw_mcfp *hw_mcfp,
					struct is_frame *frame)
{
	u32 plane = 1;
	struct mcfp_param_set *param_set = &hw_mcfp->param_set[instance];

	/* HW FRO */
	SET_MCFP_MUTLI_BUFFER_ADDR(frame->num_buffers, plane, frame->dvaddr_buffer);

	CALL_HW_OPS(hw_ip, dma_cfg, "sf",
		hw_ip, frame, 0, frame->num_buffers,
		&param_set->dma_input_sf.cmd, plane,
		param_set->input_dva_sf, frame->dvaddr_buffer);
}

static void is_hw_mcfp_set_wgt_i_buffer(int instance, struct is_hw_ip *hw_ip,
					struct is_hw_mcfp *hw_mcfp,
					struct is_frame *frame,
					bool swap_frame, bool swap_fro)
{
	struct is_frame *i_frame_p, *i_frame_c;
	struct is_framemgr *i_fmgr_p, *i_fmgr_c;
	struct mcfp_param_set *param_set = &hw_mcfp->param_set[instance];

	i_fmgr_p = GET_SUBDEV_I_FRAMEMGR(&hw_mcfp->subdev[instance][MCFP_SUBDEV_PREV_W]);
	i_fmgr_c = GET_SUBDEV_I_FRAMEMGR(&hw_mcfp->subdev[instance][MCFP_SUBDEV_W]);

	i_frame_p = get_frame(i_fmgr_p, FS_FREE);
	i_frame_c = get_frame(i_fmgr_c, FS_FREE);
	if (!i_frame_p || !i_frame_c) {
		mswarn_hw("There is no FREE frames. pre(%d), cur(%d)\n", instance, hw_ip,
			i_fmgr_p->queued_count[FS_FREE],
			i_fmgr_c->queued_count[FS_FREE]);
		return;
	}

	if (swap_frame) {
		put_frame(i_fmgr_c, i_frame_p, FS_FREE);
		put_frame(i_fmgr_p, i_frame_c, FS_FREE);
	} else {
		put_frame(i_fmgr_p, i_frame_p, FS_FREE);
		put_frame(i_fmgr_c, i_frame_c, FS_FREE);
	}

	if (swap_fro) {
		/* HW FRO */
		SET_MCFP_MUTLI_BUFFER_ADDR_SWAP(frame->num_buffers, i_frame_p->planes,
						i_frame_p->dvaddr_buffer,
						i_frame_c->dvaddr_buffer);
	} else {
		SET_MCFP_MUTLI_BUFFER_ADDR(frame->num_buffers, i_frame_p->planes,
					i_frame_p->dvaddr_buffer);
		SET_MCFP_MUTLI_BUFFER_ADDR(frame->num_buffers, i_frame_c->planes,
					i_frame_c->dvaddr_buffer);
	}

	/* prev wgt plane */
	param_set->dma_input_prev_w.plane = i_frame_p->planes;
	CALL_HW_OPS(hw_ip, dma_cfg, "prev_wgt",
		hw_ip, frame, 0, frame->num_buffers,
		&param_set->dma_input_prev_w.cmd,
		param_set->dma_input_prev_w.plane,
		param_set->input_dva_prv_w,
		i_frame_p->dvaddr_buffer);

	/* out wgt */
	param_set->dma_output_w.plane = i_frame_c->planes;
	CALL_HW_OPS(hw_ip, dma_cfg, "wgt_out",
		hw_ip, frame, 0, frame->num_buffers,
		&param_set->dma_output_w.cmd,
		param_set->dma_output_w.plane,
		param_set->output_dva_w,
		i_frame_c->dvaddr_buffer);
}

static void is_hw_mcfp_set_yuv_i_buffer(int instance, struct is_hw_ip *hw_ip,
					struct is_hw_mcfp *hw_mcfp,
					struct is_frame *frame,
					bool swap_frame, bool swap_fro)
{
	struct is_frame *i_frame_p, *i_frame_c;
	struct is_framemgr *i_fmgr_p, *i_fmgr_c;
	struct mcfp_param_set *param_set = &hw_mcfp->param_set[instance];

	i_fmgr_p = GET_SUBDEV_I_FRAMEMGR(&hw_mcfp->subdev[instance][MCFP_SUBDEV_PREV_YUV]);
	i_fmgr_c = GET_SUBDEV_I_FRAMEMGR(&hw_mcfp->subdev[instance][MCFP_SUBDEV_YUV]);

	i_frame_p = get_frame(i_fmgr_p, FS_FREE);
	i_frame_c = get_frame(i_fmgr_c, FS_FREE);
	if (!i_frame_p || !i_frame_c) {
		mswarn_hw("There is no FREE frames. pre(%d), cur(%d)\n", instance, hw_ip,
			i_fmgr_p->queued_count[FS_FREE],
			i_fmgr_c->queued_count[FS_FREE]);
		return;
	}

	if (swap_frame) {
		put_frame(i_fmgr_c, i_frame_p, FS_FREE);
		put_frame(i_fmgr_p, i_frame_c, FS_FREE);
	} else {
		put_frame(i_fmgr_p, i_frame_p, FS_FREE);
		put_frame(i_fmgr_c, i_frame_c, FS_FREE);
	}

	if (swap_fro) {
		/* HW FRO */
		SET_MCFP_MUTLI_BUFFER_ADDR_SWAP(frame->num_buffers, i_frame_p->planes,
						i_frame_p->dvaddr_buffer,
						i_frame_c->dvaddr_buffer);
	} else {
		SET_MCFP_MUTLI_BUFFER_ADDR(frame->num_buffers, i_frame_p->planes,
					i_frame_p->dvaddr_buffer);
		SET_MCFP_MUTLI_BUFFER_ADDR(frame->num_buffers, i_frame_c->planes,
					i_frame_c->dvaddr_buffer);
	}

	param_set->dma_input_prev_yuv.sbwc_type = param_set->dma_output_yuv.sbwc_type;
	/* prev yuv */
	param_set->dma_input_prev_yuv.plane = i_frame_p->planes;
	CALL_HW_OPS(hw_ip, dma_cfg, "prev_yuv",
		hw_ip, frame, 0, frame->num_buffers,
		&param_set->dma_input_prev_yuv.cmd,
		param_set->dma_input_prev_yuv.plane,
		param_set->input_dva_prv_yuv,
		i_frame_p->dvaddr_buffer);

	/* output yuv */
	param_set->dma_output_yuv.plane = i_frame_c->planes;
	CALL_HW_OPS(hw_ip, dma_cfg, "yuv_out",
		hw_ip, frame, 0, frame->num_buffers,
		&param_set->dma_output_yuv.cmd,
		param_set->dma_output_yuv.plane,
		param_set->output_dva_yuv,
		i_frame_c->dvaddr_buffer);
}

static int is_hw_mcfp_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	struct is_hw_mcfp *hw_mcfp;
	struct is_param_region *param_region;
	struct mcfp_param_set *param_set;
	u32 fcount, instance;
	u32 cur_idx;
	u32 set_id;
	int ret, i, batch_num;
	u32 cmd_input, cmd_mv_in, cmd_drc_in, cmd_dp_in;
	u32 cmd_sf_in, cmd_prev_yuv_in, cmd_prev_wgt_in;
	u32 cmd_yuv_out, cmd_wgt_out;
	struct param_stripe_input *stripe_input;
	struct is_mcfp_config *config;
	bool do_blk_cfg;
	bool skip = false, swap_frame, swap_fro;

	instance = frame->instance;
	msdbgs_hw(2, "[F:%d]shot(%d)\n", instance, hw_ip, frame->fcount, frame->cur_buf_index);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	if (!hw_ip->hardware) {
		mserr_hw("failed to get hardware", instance, hw_ip);
		return -EINVAL;
	}

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
	param_set = &hw_mcfp->param_set[instance];
	stripe_input = &param_set->stripe_input;
	param_region = frame->parameter;

	atomic_set(&hw_mcfp->strip_index, frame->stripe_info.region_id);
	fcount = frame->fcount;

	if (hw_ip->internal_fcount[instance] != 0)
		hw_ip->internal_fcount[instance] = 0;

	if (frame->shot_ext) {
		if ((param_set->tnr_mode != frame->shot_ext->tnr_mode) &&
				!CHK_VIDEOHDR_MODE_CHANGE(param_set->tnr_mode, frame->shot_ext->tnr_mode))
			msinfo_hw("[F:%d] TNR mode is changed (%d -> %d)\n",
					instance, hw_ip, frame->fcount,
					param_set->tnr_mode, frame->shot_ext->tnr_mode);
		param_set->tnr_mode = frame->shot_ext->tnr_mode;
	} else {
		mswarn_hw("frame->shot_ext is null", instance, hw_ip);
		param_set->tnr_mode = TNR_PROCESSING_PREVIEW_POST_ON;
	}

	__is_hw_mcfp_update_param(hw_ip, param_region, param_set, frame->pmap, instance);

	/* DMA settings */
	cur_idx = frame->cur_buf_index;

	cmd_input = CALL_HW_OPS(hw_ip, dma_cfg, "mcfp",
			hw_ip, frame, cur_idx, frame->num_buffers,
			&param_set->dma_input.cmd,
			param_set->dma_input.plane,
			param_set->input_dva,
			frame->dvaddr_buffer);

	cmd_mv_in = CALL_HW_OPS(hw_ip, dma_cfg, "mv",
			hw_ip, frame, cur_idx, frame->num_buffers,
			&param_set->dma_input_mv.cmd,
			param_set->dma_input_mv.plane,
			param_set->input_dva_mv,
			frame->dva_mcfp_motion);

	cmd_drc_in = CALL_HW_OPS(hw_ip, dma_cfg, "drc",
			hw_ip, frame, cur_idx, frame->num_buffers,
			&param_set->dma_input_drc_cur_map.cmd,
			param_set->dma_input_drc_cur_map.plane,
			param_set->input_dva_drc_cur_map,
			frame->dva_mcfp_cur_drc);

	cmd_dp_in = CALL_HW_OPS(hw_ip, dma_cfg, "dp",
			hw_ip, frame, cur_idx, frame->num_buffers,
			&param_set->dma_input_drc_prv_map.cmd,
			param_set->dma_input_drc_prv_map.plane,
			param_set->input_dva_drc_prv_map,
			frame->dva_mcfp_prev_drc);

	cmd_sf_in = CALL_HW_OPS(hw_ip, dma_cfg, "sf",
			hw_ip, frame, cur_idx, frame->num_buffers,
			&param_set->dma_input_sf.cmd,
			param_set->dma_input_sf.plane,
			param_set->input_dva_sf,
			frame->dva_mcfp_sat_flag);

	cmd_prev_wgt_in = CALL_HW_OPS(hw_ip, dma_cfg, "prev_wgt",
			hw_ip, frame, cur_idx, frame->num_buffers,
			&param_set->dma_input_prev_w.cmd,
			param_set->dma_input_prev_w.plane,
			param_set->input_dva_prv_w,
			frame->dva_mcfp_prev_wgt);

	cmd_prev_yuv_in = param_set->dma_input_prev_yuv.cmd;
	cmd_yuv_out = param_set->dma_output_yuv.cmd;

	cmd_wgt_out = CALL_HW_OPS(hw_ip, dma_cfg, "wgt_out",
			hw_ip, frame, cur_idx, frame->num_buffers,
			&param_set->dma_output_w.cmd,
			param_set->dma_output_w.plane,
			param_set->output_dva_w,
			frame->dva_mcfp_wgt);

	param_set->instance_id = instance;
	param_set->fcount = fcount;
	/* multi-buffer */
	hw_ip->num_buffers = frame->num_buffers;
	batch_num = hw_ip->framemgr->batch_num;
	/* Update only for SW_FRO scenario */
	if (batch_num > 1 && hw_ip->num_buffers == 1) {
		hw_ip->num_buffers |= batch_num << SW_FRO_NUM_SHIFT;
		hw_ip->num_buffers |= cur_idx << CURR_INDEX_SHIFT;
	} else {
		batch_num = 0;
	}

	if (IS_ENABLED(SKIP_ISP_SHOT_FOR_MULTI_SHOT)) {
		if (!pablo_is_first_shot(batch_num, cur_idx) ||
		    !pablo_is_first_shot(stripe_input->total_count, stripe_input->index) ||
		    !pablo_is_first_shot(stripe_input->repeat_num, stripe_input->repeat_idx))
			skip = true;
	}

	/* temp direct set */
	set_id = COREX_DIRECT;

	mcfp_hw_s_core(hw_ip->regs[REG_SETA], set_id);

	ret = CALL_HW_HELPER_OPS(hw_ip, lib_shot, instance, skip,
				frame, &hw_mcfp->lib[instance], param_set);
	if (ret)
		return ret;

	config = &hw_mcfp->config[instance];

	do_blk_cfg = CALL_HW_HELPER_OPS(hw_ip, set_rta_regs, instance, set_id, skip, frame, NULL);
	if (unlikely(do_blk_cfg))
		__is_hw_mcfp_update_block_reg(hw_ip, param_set, instance, set_id);
	else
		__is_hw_mcfp_set_dma_cmd(param_set, instance, config);

	if (config->geomatch_en && !config->geomatch_bypass && !cmd_mv_in) {
		mswarn_hw("LMC is enabled, but mv buffer is disabled", instance, hw_ip);
		ret = -EINVAL;
		goto shot_fail_recovery;
	}

	ret = __is_hw_mcfp_set_size_regs(hw_ip, param_set, instance, frame, set_id);
	if (ret) {
		mserr_hw("__is_hw_mcfp_set_size_regs is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	if (config->mixer_enable) {
		ret = __is_hw_mcfp_alloc_buffer(instance, hw_ip, param_set, config);
		if (ret) {
			mserr_hw("__is_hw_mcfp_alloc_buffer is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}

		swap_frame = (SKIP_MIX(config) || PARTIAL(stripe_input) || EVEN_BATCH(frame))
				? false : true;
		swap_fro = (SKIP_MIX(config) || PARTIAL(stripe_input))
				? false : true;

		is_hw_mcfp_set_wgt_i_buffer(instance, hw_ip, hw_mcfp, frame, swap_frame, swap_fro);

		if (!config->still_en) {
			is_hw_mcfp_set_yuv_i_buffer(instance, hw_ip, hw_mcfp, frame, swap_frame, swap_fro);
		} else {
			if (!cmd_sf_in)
				is_hw_mcfp_set_sf_i_buffer(instance, hw_ip, hw_mcfp, frame);

			CALL_HW_OPS(hw_ip, dma_cfg, "prev_yuv",
						hw_ip, frame, cur_idx, frame->num_buffers,
						&param_set->dma_input_prev_yuv.cmd,
						param_set->dma_input_prev_yuv.plane,
						param_set->input_dva_prv_yuv,
						frame->dva_mcfp_prev_yuv);

			CALL_HW_OPS(hw_ip, dma_cfg, "yuv_out",
						hw_ip, frame, cur_idx, frame->num_buffers,
						&param_set->dma_output_yuv.cmd,
						param_set->dma_output_yuv.plane,
						param_set->output_dva_yuv,
						frame->dva_mcfp_yuv);
		}
	} else {
		__is_hw_mcfp_free_buffer(instance, hw_ip);
	}

	for (i = MCFP_RDMA_CUR_IN_Y; i < MCFP_RDMA_MAX; i++) {
		ret = __is_hw_mcfp_set_rdma(hw_ip, hw_mcfp, param_set, instance,
				i, set_id);
		if (ret) {
			mserr_hw("__is_hw_mcfp_set_rdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	for (i = MCFP_WDMA_PREV_OUT_Y; i < MCFP_WDMA_MAX; i++) {
		ret = __is_hw_mcfp_set_wdma(hw_ip, hw_mcfp, param_set, instance,
				i, set_id);
		if (ret) {
			mserr_hw("__is_hw_mcfp_set_wdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, fcount, DEBUG_POINT_ADD_TO_CMDQ);
	ret = is_hw_mcfp_set_cmdq(hw_ip, frame->num_buffers);
	if (ret) {
		mserr_hw("is_hw_mcfp_set_cmdq is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	/* Restore CMDs in param_set. */
	param_set->dma_input_prev_yuv.cmd = cmd_prev_yuv_in;
	param_set->dma_input_mv.cmd  = cmd_mv_in;
	param_set->dma_input_sf.cmd  = cmd_sf_in;
	param_set->dma_input_prev_w.cmd = cmd_prev_wgt_in;
	param_set->dma_output_yuv.cmd = cmd_yuv_out;
	param_set->dma_output_w.cmd = cmd_wgt_out;

	if (unlikely(debug_mcfp))
		mcfp_hw_dump(hw_ip->regs[REG_SETA]);

	set_bit(HW_CONFIG, &hw_ip->state);

	return 0;

shot_fail_recovery:
	CALL_HW_HELPER_OPS(hw_ip, restore, instance, &hw_mcfp->lib[instance]);

	return ret;
}

static int is_hw_mcfp_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	struct is_hw_mcfp *hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, get_meta, frame->instance,
				frame, &hw_mcfp->lib[frame->instance]);
}

static int is_hw_mcfp_get_cap_meta(struct is_hw_ip *hw_ip, ulong hw_map,
	u32 instance, u32 fcount, u32 size, ulong addr)
{
	struct is_hw_mcfp *hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, get_cap_meta, instance,
				&hw_mcfp->lib[instance], fcount, size, addr);
}

static int is_hw_mcfp_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
		enum ShotErrorType done_type)
{
	int ret = 0;
	int output_id;

	output_id = IS_HW_CORE_END;
	if (test_bit(hw_ip->id, &frame->core_flag)) {
		ret = CALL_HW_OPS(hw_ip, frame_done, hw_ip, frame, -1,
			output_id, done_type, false);
	}

	return ret;
}

static int is_hw_mcfp_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_mcfp *hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, load_setfile, instance, &hw_mcfp->lib[instance]);
}

static int is_hw_mcfp_apply_setfile(struct is_hw_ip *hw_ip, u32 scenario,
	u32 instance, ulong hw_map)
{
	struct is_hw_mcfp *hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, apply_setfile, instance, &hw_mcfp->lib[instance],
				scenario);
}

static int is_hw_mcfp_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_mcfp *hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, delete_setfile, instance, &hw_mcfp->lib[instance]);
}

int is_hw_mcfp_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_mcfp *hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	if (__is_hw_mcfp_s_common_reg(hw_ip, instance)) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}

	return CALL_HW_HELPER_OPS(hw_ip, restore, instance, &hw_mcfp->lib[instance]);
}

static int is_hw_mcfp_set_regs(struct is_hw_ip *hw_ip, u32 chain_id,
	u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size)
{
	return CALL_HW_HELPER_OPS(hw_ip, set_regs, instance, regs, regs_size);
}

int is_hw_mcfp_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size, enum is_reg_dump_type dump_type)
{
	void __iomem *base;
	struct is_common_dma *dma;
	struct is_hw_mcfp *hw_mcfp = NULL;
	u32 i;

	switch (dump_type) {
	case IS_REG_DUMP_TO_LOG:
		base = hw_ip->regs[REG_SETA];
		mcfp_hw_dump(base);
		break;
	case IS_REG_DUMP_DMA:
		for (i = MCFP_RDMA_CUR_IN_Y; i < MCFP_RDMA_MAX; i++) {
			hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
			dma = &hw_mcfp->rdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}

		for (i = MCFP_WDMA_PREV_OUT_Y; i < MCFP_WDMA_MAX; i++) {
			hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
			dma = &hw_mcfp->wdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int is_hw_mcfp_set_config(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount, void *conf)
{
	struct is_hw_mcfp *hw_mcfp;
	struct is_mcfp_config *mcfp_config = (struct is_mcfp_config *)conf;

	FIMC_BUG(!conf);

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	if (hw_mcfp->config[instance].geomatch_en != mcfp_config->geomatch_en)
		msinfo_hw("[F:%d] MCFP geomatch enable: %d -> %d", instance, hw_ip, fcount,
			hw_mcfp->config[instance].geomatch_en,
			mcfp_config->geomatch_en);

	if (hw_mcfp->config[instance].mixer_enable != mcfp_config->mixer_enable)
		msinfo_hw("[F:%d] MCFP mixer enable: %d -> %d", instance, hw_ip, fcount,
			hw_mcfp->config[instance].mixer_enable,
			mcfp_config->mixer_enable);

	if (hw_mcfp->config[instance].still_en != mcfp_config->still_en)
		msinfo_hw("[F:%d] MCFP still enable: %d -> %d", instance, hw_ip, fcount,
			hw_mcfp->config[instance].still_en,
			mcfp_config->still_en);

	if (hw_mcfp->config[instance].mixer_mode != mcfp_config->mixer_mode)
		msinfo_hw("[F:%d] MCFP mode changed: %d -> %d", instance, hw_ip, fcount,
			hw_mcfp->config[instance].mixer_mode,
			mcfp_config->mixer_mode);

	if (hw_mcfp->config[instance].geomatch_bypass != mcfp_config->geomatch_bypass)
		msinfo_hw("[F:%d] MCFP geomatch bypass: %d -> %d", instance, hw_ip, fcount,
			hw_mcfp->config[instance].geomatch_bypass,
			mcfp_config->geomatch_bypass);

	if (!mcfp_config->still_en) {
		if (hw_mcfp->config[instance].img_bit != mcfp_config->img_bit)
			msinfo_hw("[F:%d] MCFP img bit: %d -> %d", instance, hw_ip, fcount,
				hw_mcfp->config[instance].img_bit,
				mcfp_config->img_bit);
	} else {
		mcfp_config->img_bit = 12;
	}

	if (mcfp_config->img_bit != 8 && mcfp_config->img_bit != 10 && mcfp_config->img_bit != 12) {
		mswarn_hw("[F:%d] img_bit(%d) is not valid", instance, hw_ip, fcount,
			mcfp_config->img_bit);
		mcfp_config->img_bit = 12;
	}

	if (!mcfp_config->still_en) {
		if (hw_mcfp->config[instance].wgt_bit != mcfp_config->wgt_bit)
			msinfo_hw("[F:%d] MCFP wgt bit: %d -> %d", instance, hw_ip, fcount,
				hw_mcfp->config[instance].wgt_bit,
				mcfp_config->wgt_bit);
	} else {
		mcfp_config->wgt_bit = 8;
	}

	if (mcfp_config->wgt_bit != 4 && mcfp_config->wgt_bit != 8) {
		mswarn_hw("[F:%d] wgt_bit(%d) is not valid", instance, hw_ip, fcount,
			mcfp_config->wgt_bit);
		mcfp_config->wgt_bit = 8;
	}

#if (DDK_INTERFACE_VER == 0x1010)
	if (hw_mcfp->config[instance].motion_scale_x != mcfp_config->motion_scale_x ||
	    hw_mcfp->config[instance].motion_scale_y != mcfp_config->motion_scale_y)
		msinfo_hw("[F:%d] MCFP motion_scale: (%d x %d) -> (%d x %d)",
			instance, hw_ip, fcount,
			hw_mcfp->config[instance].motion_scale_x,
			hw_mcfp->config[instance].motion_scale_y,
			mcfp_config->motion_scale_x,
			mcfp_config->motion_scale_y);

	if (mcfp_config->motion_scale_x > 2) {
		mswarn_hw("[F:%d] motion_scale_x(%d) is not valid", instance, hw_ip, fcount,
			mcfp_config->motion_scale_x);
		mcfp_config->motion_scale_x = 0;
	}

	if (mcfp_config->motion_scale_y > 3) {
		mswarn_hw("[F:%d] motion_scale_y(%d) is not valid", instance, hw_ip, fcount,
			mcfp_config->motion_scale_y);
		mcfp_config->motion_scale_y = 0;
	}
#endif

	msdbg_hw(2, "[F:%d] skip_wdma(%d)\n", instance, hw_ip, fcount, mcfp_config->skip_wdma);

	memcpy(&hw_mcfp->config[instance], conf, sizeof(struct is_mcfp_config));

	return 0;
}

static int is_hw_mcfp_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_mcfp *hw_mcfp;

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
	if (!hw_mcfp) {
		mserr_hw("failed to get HW MCFP", instance, hw_ip);
		return -ENODEV;
	}

	mcfp_hw_dump(hw_ip->regs[REG_SETA]);

	return CALL_HW_HELPER_OPS(hw_ip, notify_timeout, instance, &hw_mcfp->lib[instance]);
}

const struct is_hw_ip_ops is_hw_mcfp_ops = {
	.open			= is_hw_mcfp_open,
	.init			= is_hw_mcfp_init,
	.deinit			= is_hw_mcfp_deinit,
	.close			= is_hw_mcfp_close,
	.enable			= is_hw_mcfp_enable,
	.disable		= is_hw_mcfp_disable,
	.shot			= is_hw_mcfp_shot,
	.set_param		= is_hw_mcfp_set_param,
	.get_meta		= is_hw_mcfp_get_meta,
	.get_cap_meta		= is_hw_mcfp_get_cap_meta,
	.frame_ndone		= is_hw_mcfp_frame_ndone,
	.load_setfile		= is_hw_mcfp_load_setfile,
	.apply_setfile		= is_hw_mcfp_apply_setfile,
	.delete_setfile		= is_hw_mcfp_delete_setfile,
	.restore		= is_hw_mcfp_restore,
	.set_regs		= is_hw_mcfp_set_regs,
	.dump_regs		= is_hw_mcfp_dump_regs,
	.set_config		= is_hw_mcfp_set_config,
	.notify_timeout		= is_hw_mcfp_notify_timeout,
};

int is_hw_mcfp_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int hw_slot;

	hw_ip->ops = &is_hw_mcfp_ops;

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &is_hw_mcfp_handle_interrupt;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].handler = &is_hw_mcfp_handle_interrupt;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].id = INTR_HWIP2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = true;

	return 0;
}
EXPORT_SYMBOL_GPL(is_hw_mcfp_probe);
