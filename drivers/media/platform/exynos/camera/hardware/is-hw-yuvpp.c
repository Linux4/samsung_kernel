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

#include "is-hw-yuvpp.h"
#include "is-err.h"
#include "api/is-hw-api-yuvpp-v2_1.h"
#include "is-votfmgr.h"
#include "is-votf-id-table.h"
#include "pablo-fpsimd.h"

#define LIB_MODE USE_DRIVER
static spinlock_t ypp_out_slock;

static int debug_ypp;
module_param(debug_ypp, int, 0644);

static inline void __nocfi __is_hw_ypp_ddk_isr(struct is_interface_hwip *itf_hw, int handler_id)
{
	struct hwip_intr_handler *intr_hw = NULL;

	intr_hw = &itf_hw->handler[handler_id];

	if (intr_hw->valid) {
		is_fpsimd_get_isr();
		intr_hw->handler(intr_hw->id, intr_hw->ctx);
		is_fpsimd_put_isr();
	} else {
		err_itfc("[ID:%d](%d)- chain(%d) empty handler!!",
			itf_hw->id, handler_id, intr_hw->chain_id);
	}
}

static int is_hw_ypp_handle_interrupt(u32 id, void *context)
{
	struct is_hardware *hardware = NULL;
	struct is_hw_ip *hw_ip = NULL;
	struct is_hw_ypp *hw_ypp = NULL;
	struct is_interface_hwip *itf_hw = NULL;
	u32 status, instance, hw_fcount, strip_index, hl = 0, vl = 0;
	bool f_err = false;
	int hw_slot = -1;

	hw_ip = (struct is_hw_ip *)context;

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	strip_index = atomic_read(&hw_ypp->strip_index);

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hardware, get_hw_slot_id, hw_ip->id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return IRQ_NONE;
	}

	itf_hw = &hw_ip->itfc->itf_ip[hw_slot];

	if (hw_ip->lib_mode == USE_ONLY_DDK) {
		__is_hw_ypp_ddk_isr(itf_hw, INTR_HWIP1);
		return IRQ_HANDLED;
	}

	status = ypp_hw_g_int_state(hw_ip->regs[REG_SETA], true,
			(hw_ip->num_buffers & 0xffff), &hw_ypp->irq_state[YUVPP_INTR])
			& ypp_hw_g_int_mask(hw_ip->regs[REG_SETA]);

	msdbg_hw(2, "YUVPP interrupt status(0x%x)\n", instance, hw_ip, status);

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

	if (ypp_hw_is_occurred(status, INTR_FRAME_START) && ypp_hw_is_occurred(status, INTR_FRAME_END))
		mswarn_hw("start/end overlapped!! (0x%x)", instance, hw_ip, status);

	if (ypp_hw_is_occurred(status, INTR_FRAME_START)) {
		if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
			is_lib_isp_event_notifier(hw_ip, &hw_ypp->lib[instance],
					instance, hw_fcount, EVENT_FRAME_START,
					strip_index, NULL);
		} else {
			atomic_add(1, &hw_ip->count.fs);
			CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount,
					DEBUG_POINT_FRAME_START);
			if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
				msinfo_hw("[F:%d]F.S\n", instance, hw_ip,
						hw_fcount);

			CALL_HW_OPS(hw_ip, frame_start, hw_ip, instance);
		}
	}

	if (ypp_hw_is_occurred(status, INTR_FRAME_END)) {
		if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
			is_lib_isp_event_notifier(hw_ip, &hw_ypp->lib[instance],
					instance, hw_fcount, EVENT_FRAME_END,
					strip_index, NULL);
		} else {
			CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount,
					DEBUG_POINT_FRAME_END);
			atomic_add(hw_ip->num_buffers, &hw_ip->count.fe);

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

	if (ypp_hw_is_occurred(status, INTR_COREX_END_0)) {
		/* */
	}

	if (ypp_hw_is_occurred(status, INTR_COREX_END_1)) {
		/* */
	}

	f_err = ypp_hw_is_occurred(status, INTR_ERR);

	if (f_err) {
		msinfo_hw("[F:%d] Ocurred error interrupt (%d,%d) status(0x%x)\n",
			instance, hw_ip, hw_fcount, hl, vl, status);
		ypp_hw_dump(hw_ip->regs[REG_SETA]);
	}

	return 0;
}

static int __is_hw_ypp_s_reset(struct is_hw_ip *hw_ip, struct is_hw_ypp *hw_ypp)
{
	int i;

	for (i = 0; i < COREX_MAX; i++)
		hw_ip->cur_hw_iq_set[i].size = 0;

	return ypp_hw_s_reset(hw_ip->regs[REG_SETA]);
}

static int __is_hw_ypp_s_common_reg(struct is_hw_ip *hw_ip, u32 instance)
{
	int res = 0;
	struct is_hw_ypp *hw_ypp;

	FIMC_BUG(!hw_ip);

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;

	if (hw_ip) {
		msinfo_hw("reset\n", instance, hw_ip);

		res = __is_hw_ypp_s_reset(hw_ip, hw_ypp);
		if (res != 0) {
			mserr_hw("sw reset fail", instance, hw_ip);
			return -ENODEV;
		}
		/* temp corex disable*/
		ypp_hw_s_corex_init(hw_ip->regs[REG_SETA], 0);
		ypp_hw_s_init(hw_ip->regs[REG_SETA]);
	}

	msinfo_hw("clear interrupt\n", instance, hw_ip);

	return 0;
}

static int __is_hw_ypp_clear_common(struct is_hw_ip *hw_ip, u32 instance)
{
	int res = 0;
	struct is_hw_ypp *hw_ypp;

	FIMC_BUG(!hw_ip);

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;

	res = __is_hw_ypp_s_reset(hw_ip, hw_ypp);
	if (res != 0) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}
	res = ypp_hw_wait_idle(hw_ip->regs[REG_SETA]);

	if (res)
		mserr_hw("failed to ypp_hw_wait_idle", instance, hw_ip);

	msinfo_hw("final finished ypp\n", instance, hw_ip);

	return res;
}

static int is_hw_ypp_set_cmdq(struct is_hw_ip *hw_ip, u32 instance, u32 set_id)
{
	/*
	 * add to command queue
	 * 1. set corex_update_type_0_setX
	 * normal shot: SETA(wide), SETB(tele, front), SETC(uw)
	 * reprocessing shot: SETD
	 * 2. set cq_frm_start_type to 0. only at first time?
	 * 3. set ms_add_to_queue(set id, frame id).
	 */
	int ret = 0;

	FIMC_BUG(!hw_ip);

	ret = ypp_hw_s_corex_update_type(hw_ip->regs[REG_SETA], set_id);
	if (ret)
		return ret;
	ypp_hw_s_cmdq(hw_ip->regs[REG_SETA], set_id);

	return ret;
}

static int __is_hw_ypp_init_config(struct is_hw_ip *hw_ip, u32 instance, struct is_frame *frame, u32 set_id)
{
	struct is_hw_ypp *hw_ypp;
	struct is_hardware *hardware;
	struct ypp_param_set *param_set = NULL;
	int ret = 0;

	FIMC_BUG(!hw_ip);

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	if (!hw_ypp) {
		mserr_hw("failed to get ypp", instance, hw_ip);
		return -ENODEV;
	}

	hardware = hw_ip->hardware;
	if (!hardware) {
		mserr_hw("failed to get hardware", instance, hw_ip);
		return -EINVAL;
	}

	param_set = &hw_ypp->param_set[instance];

	/* ypp context setting */
	ypp_hw_s_core(hw_ip->regs[REG_SETA], frame->num_buffers, set_id);

	return ret;
}

static int __nocfi is_hw_ypp_open(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_ypp *hw_ypp = NULL;

	FIMC_BUG(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HWYPP");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw_ip->priv_info = vzalloc(sizeof(struct is_hw_ypp));
	if (!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	hw_ypp->instance = instance;

	ret = CALL_HW_HELPER_OPS(hw_ip, open, instance, &hw_ypp->lib[instance],
				LIB_FUNC_YUVPP);
	if (ret)
		goto err_chain_create;

	ret = CALL_HW_HELPER_OPS(hw_ip, alloc_iqset, YUVPP_REG_CNT);
	if (ret)
		goto err_iqset_alloc;

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	set_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "open: framemgr[%s]", instance, hw_ip, hw_ip->framemgr->name);

	return 0;

err_iqset_alloc:
	CALL_HW_HELPER_OPS(hw_ip, close, instance, &hw_ypp->lib[instance]);

err_chain_create:
	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_ypp_init(struct is_hw_ip *hw_ip, u32 instance,
			bool flag, u32 f_type)
{
	int ret = 0;
	struct is_hw_ypp *hw_ypp = NULL;
	u32 input_id;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	if (!hw_ypp) {
		mserr_hw("hw_ypp is null ", instance, hw_ip);
		ret = -ENODATA;
		goto err;
	}

	hw_ip->lib_mode = LIB_MODE;

	ret = CALL_HW_HELPER_OPS(hw_ip, init, instance, &hw_ypp->lib[instance],
				(u32)flag, f_type, LIB_FUNC_YUVPP);
	if (ret)
		return ret;

	if (hw_ip->lib_mode == USE_ONLY_DDK) {
		set_bit(HW_INIT, &hw_ip->state);
		return 0;
	}

	for (input_id = YUVPP_RDMA_L0_Y; input_id < YUVPP_RDMA_MAX; input_id++) {
		ret = ypp_hw_dma_create(&hw_ypp->rdma[input_id], hw_ip->regs[REG_SETA], input_id);
		if (ret) {
			mserr_hw("ypp_hw_dma_create error[%d]", instance, hw_ip, input_id);
			ret = -ENODATA;
			goto err;
		}
	}

	set_bit(HW_INIT, &hw_ip->state);
	return 0;

err:
	return ret;

}

static int is_hw_ypp_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_ypp *hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, deinit, instance, &hw_ypp->lib[instance]);
}

static int is_hw_ypp_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_ypp *hw_ypp = NULL;
	int ret = 0;
	ulong flag = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;

	CALL_HW_HELPER_OPS(hw_ip, close, instance, &hw_ypp->lib[instance]);

	if (hw_ip->lib_mode == USE_DRIVER) {
		spin_lock_irqsave(&ypp_out_slock, flag);
		__is_hw_ypp_clear_common(hw_ip, instance);
		spin_unlock_irqrestore(&ypp_out_slock, flag);
	}

	CALL_HW_HELPER_OPS(hw_ip, free_iqset);

	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	clear_bit(HW_OPEN, &hw_ip->state);


	return ret;
}

static int is_hw_ypp_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_ypp *hw_ypp = NULL;
	int ret = 0;
	ulong flag = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	if (test_bit(HW_RUN, &hw_ip->state))
		return 0;

	msdbg_hw(2, "enable: start\n", instance, hw_ip);

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	if (hw_ip->lib_mode == USE_ONLY_DDK) {
		set_bit(HW_RUN, &hw_ip->state);
		return ret;
	}

	spin_lock_irqsave(&ypp_out_slock, flag);
	__is_hw_ypp_s_common_reg(hw_ip, instance);
	spin_unlock_irqrestore(&ypp_out_slock, flag);

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return ret;
}

static int is_hw_ypp_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_ypp *hw_ypp;
	struct ypp_param_set *param_set;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("ypp_disable: Vvalid(%d)\n", instance, hw_ip,
		atomic_read(&hw_ip->status.Vvalid));

	FIMC_BUG(!hw_ip->priv_info);
	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;

	timetowait = wait_event_timeout(hw_ip->status.wait_queue,
		!atomic_read(&hw_ip->status.Vvalid),
		IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		mserr_hw("wait FRAME_END timeout (%ld)", instance,
			hw_ip, timetowait);
		ret = -ETIME;
	}

	param_set = &hw_ypp->param_set[instance];
	param_set->fcount = 0;

	CALL_HW_HELPER_OPS(hw_ip, disable, instance, &hw_ypp->lib[instance]);

	if (hw_ip->run_rsc_state)
		return 0;

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static int __is_hw_ypp_set_rdma_cmd(struct ypp_param_set *param_set, u32 instance, struct is_yuvpp_config *conf)
{
	FIMC_BUG(!param_set);
	FIMC_BUG(!conf);

	param_set->dma_input.cmd = DMA_INPUT_COMMAND_ENABLE;

	if (conf->yuvnr_bypass & 0x1) {
		param_set->dma_input_lv2.cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->dma_input_drc.cmd = DMA_INPUT_COMMAND_DISABLE;
	} else {
		param_set->dma_input_lv2.cmd = DMA_INPUT_COMMAND_ENABLE;
		param_set->dma_input_drc.cmd = DMA_INPUT_COMMAND_ENABLE;
	}

	if (conf->clahe_bypass & 0x1)
		param_set->dma_input_hist.cmd = DMA_INPUT_COMMAND_DISABLE;
	else
		param_set->dma_input_hist.cmd = DMA_INPUT_COMMAND_ENABLE;

	if (conf->nadd_bypass & 0x1)
		param_set->dma_input_noise.cmd = DMA_INPUT_COMMAND_DISABLE;
	else
		param_set->dma_input_noise.cmd = DMA_INPUT_COMMAND_ENABLE;

	return 0;
}

static int __is_hw_ypp_set_rdma(struct is_hw_ip *hw_ip, struct is_hw_ypp *hw_ypp, struct ypp_param_set *param_set, u32 instance, u32 id, u32 set_id)
{
	u32 *input_dva = NULL;
	u32 cmd;
	u32 grid_x_num = 0, grid_y_num = 0;
	u32 comp_sbwc_en, payload_size;
	u32 in_crop_size_x = 0;
	u32 cache_hint = 0;
	int ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ypp);
	FIMC_BUG(!param_set);

	switch (id) {
	case YUVPP_RDMA_L0_Y:
	case YUVPP_RDMA_L0_UV:
		input_dva = param_set->input_dva;
		cmd = param_set->dma_input.cmd;
		break;
	case YUVPP_RDMA_L2_Y:
	case YUVPP_RDMA_L2_UV:
	case YUVPP_RDMA_L2_1_Y:
	case YUVPP_RDMA_L2_1_UV:
	case YUVPP_RDMA_L2_2_Y:
	case YUVPP_RDMA_L2_2_UV:
		cmd = param_set->dma_input_lv2.cmd;
		input_dva = param_set->input_dva_lv2;
		in_crop_size_x = ypp_hw_g_in_crop_size_x(hw_ip->regs[REG_SETA], set_id);
		break;
	case YUVPP_RDMA_NOISE:
		if (!ypp_hw_g_nadd_use_noise_rdma(hw_ip->regs[REG_SETA], set_id))
			param_set->dma_input_noise.cmd = DMA_INPUT_COMMAND_DISABLE;
		cmd = param_set->dma_input_noise.cmd;
		input_dva = param_set->input_dva_noise;
		break;
	case YUVPP_RDMA_DRCGAIN:
		cmd = param_set->dma_input_drc.cmd;
		input_dva = param_set->input_dva_drc;
		in_crop_size_x = ypp_hw_g_in_crop_size_x(hw_ip->regs[REG_SETA], set_id);
		break;
	case YUVPP_RDMA_HIST:
		cmd = param_set->dma_input_hist.cmd;
		input_dva = param_set->input_dva_hist;
		ypp_hw_g_hist_grid_num(hw_ip->regs[REG_SETA], set_id, &grid_x_num, &grid_y_num);
		break;
	default:
		merr_hw("invalid ID (%d)", instance,  id);
		return -EINVAL;
	}

	switch (id) {
	case YUVPP_RDMA_L0_Y:
	case YUVPP_RDMA_L0_UV:
	case YUVPP_RDMA_NOISE:
	case YUVPP_RDMA_DRCGAIN:
		cache_hint = 0x7;	// 111: last-access-read
		break;
	case YUVPP_RDMA_L2_Y:
	case YUVPP_RDMA_L2_UV:
	case YUVPP_RDMA_L2_1_Y:
	case YUVPP_RDMA_L2_1_UV:
	case YUVPP_RDMA_L2_2_Y:
	case YUVPP_RDMA_L2_2_UV:
	case YUVPP_RDMA_HIST:
		cache_hint = 0x3;	// 011: cache-noalloc-type
		break;
	default:
		merr_hw("invalid ID (%d)", instance,  id);
		return -EINVAL;
	}

	msdbg_hw(2, "%s: %d\n", instance, hw_ip, hw_ypp->rdma[id].name, cmd);

	ypp_hw_s_rdma_corex_id(&hw_ypp->rdma[id], set_id);

	ret = ypp_hw_s_rdma_init(&hw_ypp->rdma[id], param_set, cmd,
		grid_x_num, grid_y_num, in_crop_size_x, cache_hint,
		&comp_sbwc_en, &payload_size);
	if (ret) {
		mserr_hw("failed to initialize YUVPP_RDMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (cmd == DMA_INPUT_COMMAND_ENABLE) {
		ret = ypp_hw_s_rdma_addr(&hw_ypp->rdma[id], input_dva, 0,
			(hw_ip->num_buffers & 0xffff),
			0, comp_sbwc_en, payload_size);
		if (ret) {
			mserr_hw("failed to set YUVPP_RDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
	}

	return 0;
}

static void __is_hw_ypp_check_size(struct is_hw_ip *hw_ip, struct ypp_param_set *param_set, u32 instance)
{
	struct param_dma_input *input = &param_set->dma_input;
	struct param_otf_output *output = &param_set->otf_output;

	FIMC_BUG_VOID(!param_set);

	msdbgs_hw(2, "hw_ypp_check_size >>>\n", instance, hw_ip);
	msdbgs_hw(2, "dma_input: format(%d),crop_size(%dx%d)\n", instance, hw_ip,
		input->format, input->dma_crop_width, input->dma_crop_height);
	msdbgs_hw(2, "otf_output: otf_cmd(%d),format(%d)\n", instance, hw_ip,
		output->cmd, output->format);
	msdbgs_hw(2, "otf_output: pos(%d,%d),crop%dx%d),size(%dx%d)\n", instance, hw_ip,
		output->crop_offset_x, output->crop_offset_y,
		output->crop_width, output->crop_height,
		output->width, output->height);
	msdbgs_hw(2, "[%d]<<< hw_ypp_check_size <<<\n", instance, hw_ip);
}

static int __is_hw_ypp_bypass(struct is_hw_ip *hw_ip, u32 set_id)
{
	FIMC_BUG(!hw_ip);

	ypp_hw_s_block_bypass(hw_ip->regs[REG_SETA], set_id);

	return 0;
}


static int __is_hw_ypp_update_block_reg(struct is_hw_ip *hw_ip, struct ypp_param_set *param_set, u32 instance, u32 set_id)
{
	struct is_hw_ypp *hw_ypp;
	int ret = 0;
	FIMC_BUG(!hw_ip);
	FIMC_BUG(!param_set);

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;

	if (hw_ypp->instance != instance) {
		msdbg_hw(2, "update_param: hw_ip->instance(%d)\n",
			instance, hw_ip, hw_ypp->instance);
		hw_ypp->instance = instance;
	}

	__is_hw_ypp_check_size(hw_ip, param_set, instance);
	ret = __is_hw_ypp_bypass(hw_ip, set_id);

	return ret;
}

static void __is_hw_ypp_update_param(struct is_hw_ip *hw_ip, struct is_param_region *p_region,
	struct ypp_param_set *param_set, IS_DECLARE_PMAP(pmap), u32 instance)
{
	struct ypp_param *param;

	param = &p_region->ypp;
	param_set->instance_id = instance;

	/* check input */
	if (test_bit(PARAM_YPP_DMA_INPUT, pmap))
		memcpy(&param_set->dma_input, &param->dma_input,
				sizeof(struct param_dma_input));

	if (test_bit(PARAM_YPP_DMA_LV2_INPUT, pmap))
		memcpy(&param_set->dma_input_lv2, &param->dma_input_lv2,
				sizeof(struct param_dma_input));

	if (test_bit(PARAM_YPP_DMA_NOISE_INPUT, pmap))
		memcpy(&param_set->dma_input_noise, &param->dma_input_noise,
				sizeof(struct param_dma_input));

	if (test_bit(PARAM_YPP_DMA_DRC_INPUT, pmap))
		memcpy(&param_set->dma_input_drc, &param->dma_input_drc,
				sizeof(struct param_dma_input));

	if (test_bit(PARAM_YPP_DMA_HIST_INPUT, pmap))
		memcpy(&param_set->dma_input_hist, &param->dma_input_hist,
				sizeof(struct param_dma_input));

	/* check output*/
	if (test_bit(PARAM_YPP_OTF_OUTPUT, pmap))
		memcpy(&param_set->otf_output, &param->otf_output,
				sizeof(struct param_otf_output));

	if (test_bit(PARAM_YPP_STRIPE_INPUT, pmap))
		memcpy(&param_set->stripe_input, &param->stripe_input,
				sizeof(struct param_stripe_input));
}

static int is_hw_ypp_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
		IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	struct is_hw_ypp *hw_ypp;
	struct ypp_param_set *param_set;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!region);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	hw_ypp->instance = IS_STREAM_COUNT;

	param_set = &hw_ypp->param_set[instance];
	__is_hw_ypp_update_param(hw_ip, &region->parameter, param_set, pmap, instance);

	return 0;
}

static int __is_hw_ypp_set_rta_regs(struct is_hw_ip *hw_ip, u32 set_id, u32 instance)
{
	void __iomem *base = hw_ip->regs[REG_SETA];
	struct is_hw_iq *iq_set = NULL;
	struct cr_set *regs;
	unsigned long flag;
	u32 regs_size;
	int i, ret = 0;

	iq_set = &hw_ip->iq_set[instance];

	spin_lock_irqsave(&iq_set->slock, flag);

	if (test_and_clear_bit(CR_SET_CONFIG, &iq_set->state)) {
		regs = iq_set->regs;
		regs_size = iq_set->size;
		if (hw_ip->cur_hw_iq_set[set_id].size != regs_size ||
			memcmp(hw_ip->cur_hw_iq_set[set_id].regs, regs, sizeof(struct cr_set) * regs_size) != 0) {

			for (i = 0; i < regs_size; i++)
				writel(regs[i].reg_data, base + GET_COREX_OFFSET(set_id) + regs[i].reg_addr);

			memcpy(hw_ip->cur_hw_iq_set[set_id].regs, regs, sizeof(struct cr_set) * regs_size);
			hw_ip->cur_hw_iq_set[set_id].size = regs_size;
		}

		set_bit(CR_SET_EMPTY, &iq_set->state);
	}

	spin_unlock_irqrestore(&iq_set->slock, flag);

	return ret;
}

static int __is_hw_ypp_set_size_regs(struct is_hw_ip *hw_ip, struct ypp_param_set *param_set,
	u32 instance, u32 yuvpp_strip_start_pos, const struct is_frame *frame, u32 set_id)
{
	struct is_hw_ypp *hw_ypp;
	struct is_yuvpp_config *config;
	unsigned long flag;
	int ret = 0;
	u32 strip_enable;
	u32 dma_width, dma_height;
	u32 frame_width;
	u32 region_id;
	u32 crop_x, crop_y;
	u32 sensor_full_width, sensor_full_height;
	u32 sensor_binning_x, sensor_binning_y;
	u32 sensor_crop_x, sensor_crop_y;
	u32 taa_crop_x, taa_crop_y;
	u32 taa_crop_width, taa_crop_height;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!param_set);

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	config = &hw_ypp->config;

	strip_enable = (param_set->stripe_input.total_count == 0) ? 0 : 1;

	region_id = param_set->stripe_input.index;
	frame_width = param_set->stripe_input.full_width;
	dma_width = param_set->dma_input.width;
	dma_height = param_set->dma_input.height;
	crop_x = param_set->dma_input.dma_crop_offset >> 16;
	crop_y = param_set->dma_input.dma_crop_offset & 0xFFFF;
	frame_width = (strip_enable) ? param_set->stripe_input.full_width : dma_width;

	sensor_full_width = frame->shot->udm.frame_info.sensor_size[0];
	sensor_full_height = frame->shot->udm.frame_info.sensor_size[1];
	sensor_crop_x = frame->shot->udm.frame_info.sensor_crop_region[0];
	sensor_crop_y = frame->shot->udm.frame_info.sensor_crop_region[1];
	taa_crop_x = frame->shot->udm.frame_info.taa_in_crop_region[0];
	taa_crop_y = frame->shot->udm.frame_info.taa_in_crop_region[1];
	taa_crop_width = frame->shot->udm.frame_info.taa_in_crop_region[2];
	taa_crop_height = frame->shot->udm.frame_info.taa_in_crop_region[3];
	sensor_binning_x = frame->shot->udm.frame_info.sensor_binning[0];
	sensor_binning_y = frame->shot->udm.frame_info.sensor_binning[1];

	spin_lock_irqsave(&ypp_out_slock, flag);
	ret = ypp_hw_s_yuvnr_incrop(hw_ip->regs[REG_SETA], set_id,
		strip_enable, region_id, param_set->stripe_input.total_count, dma_width);
	if (ret) {
		mserr_hw("failed to set size regs for yuvnr_in_crop", instance, hw_ip);
		goto err;

	}
	ret = ypp_hw_s_yuvnr_size(hw_ip->regs[REG_SETA], set_id, yuvpp_strip_start_pos, frame_width,
		dma_width, dma_height, crop_x, crop_y, strip_enable,
		sensor_full_width, sensor_full_height, sensor_binning_x, sensor_binning_y, sensor_crop_x, sensor_crop_y,
		taa_crop_x, taa_crop_y, taa_crop_width, taa_crop_height);
	if (ret) {
		mserr_hw("failed to set size regs for yuvnr", instance, hw_ip);
		goto err;
	}
	ret = ypp_hw_s_yuvnr_outcrop(hw_ip->regs[REG_SETA], set_id,
		strip_enable, region_id, param_set->stripe_input.total_count);
	if (ret) {
		mserr_hw("failed to set size regs for yuvnr_out_crop", instance, hw_ip);
		goto err;
	}

	ret = ypp_hw_s_clahe_size(hw_ip->regs[REG_SETA], set_id, frame_width, dma_height,
		yuvpp_strip_start_pos, strip_enable);
	if (ret) {
		mserr_hw("failed to set size regs for clahe", instance, hw_ip);
		goto err;
	}

	ypp_hw_s_nadd_size(hw_ip->regs[REG_SETA], set_id, yuvpp_strip_start_pos, strip_enable);

	ypp_hw_s_coutfifo_size(hw_ip->regs[REG_SETA], set_id, dma_width, dma_height, strip_enable);
err:
	spin_unlock_irqrestore(&ypp_out_slock, flag);
	return ret;
}

static int is_hw_ypp_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	struct is_hw_ypp *hw_ypp = NULL;
	struct is_param_region *p_region = NULL;
	struct ypp_param_set *param_set = NULL;
	struct is_frame *dma_frame;
	struct is_group *group;
	u32 en_votf = 0;
	u32 fcount, instance;
	u32 strip_start_pos = 0;
	ulong flag = 0;
	u32 cur_idx;
	u32 set_id;
	int ret, i, batch_num;
	u32 cmd_input, cmd_input_lv2, cmd_input_noise;
	u32 cmd_input_drc, cmd_input_hist;
	u32 stripe_index;
	u32 max_dma_id;

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
	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	param_set = &hw_ypp->param_set[instance];
	p_region = frame->parameter;

	atomic_set(&hw_ypp->strip_index, frame->stripe_info.region_id);
	fcount = frame->fcount;
	dma_frame = frame;

	FIMC_BUG(!frame->shot);

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

	__is_hw_ypp_update_param(hw_ip, p_region, param_set, frame->pmap, instance);

	/* DMA settings */
	cur_idx = frame->cur_buf_index;

	en_votf = param_set->dma_input.v_otf_enable;
	if (en_votf == OTF_INPUT_COMMAND_ENABLE) {
		group = hw_ip->group[instance];
		dma_frame = is_votf_get_frame(group, TRS, ENTRY_YPP, 0);
	}

	cmd_input = CALL_HW_OPS(hw_ip, dma_cfg, "ypp", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_input.cmd,
			param_set->dma_input.plane,
			param_set->input_dva,
			dma_frame->dvaddr_buffer);

	cmd_input_lv2 = CALL_HW_OPS(hw_ip, dma_cfg, "ypnrds", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_input_lv2.cmd,
			param_set->dma_input_lv2.plane,
			param_set->input_dva_lv2,
			dma_frame->ypnrdsTargetAddress);

	cmd_input_noise = CALL_HW_OPS(hw_ip, dma_cfg, "ypnoi", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_input_noise.cmd,
			param_set->dma_input_noise.plane,
			param_set->input_dva_noise,
			dma_frame->ypnoiTargetAddress);

	cmd_input_drc = CALL_HW_OPS(hw_ip, dma_cfg, "ypdga", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_input_drc.cmd,
			param_set->dma_input_drc.plane,
			param_set->input_dva_drc,
			dma_frame->ypdgaTargetAddress);

	cmd_input_hist = CALL_HW_OPS(hw_ip, dma_cfg, "ypsvhist", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_input_hist.cmd,
			param_set->dma_input_hist.plane,
			param_set->input_dva_hist,
			dma_frame->ypsvhistTargetAddress);

	param_set->instance_id = instance;
	param_set->fcount = fcount;
	/* multi-buffer */
	hw_ip->num_buffers = dma_frame->num_buffers;
	batch_num = hw_ip->framemgr->batch_num;
	/* Update only for SW_FRO scenario */
	if (batch_num > 1 && hw_ip->num_buffers == 1) {
		hw_ip->num_buffers |= batch_num << SW_FRO_NUM_SHIFT;
		hw_ip->num_buffers |= cur_idx << CURR_INDEX_SHIFT;
	}

	if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
		if (frame->shot) {
			ret = is_lib_isp_set_ctrl(hw_ip, &hw_ypp->lib[instance],
					frame);
			if (ret)
				mserr_hw("set_ctrl fail", instance, hw_ip);
		}

		if (hw_ip->lib_mode == USE_ONLY_DDK) {
			ret = is_lib_isp_shot(hw_ip, &hw_ypp->lib[instance],
					param_set, frame->shot);

			param_set->dma_input.cmd = cmd_input;
			set_bit(HW_CONFIG, &hw_ip->state);

			return ret;
		}
	}

	/* temp direct set*/
	set_id = COREX_DIRECT;

	ret = __is_hw_ypp_init_config(hw_ip, instance, frame, set_id);
	if (ret) {
		mserr_hw("is_hw_ypp_init_config is fail", instance, hw_ip);
		return ret;
	}

	if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
		ret = is_lib_isp_shot(hw_ip, &hw_ypp->lib[instance], param_set,
				frame->shot);
		if (ret)
			return ret;

		_is_hw_frame_dbg_trace(hw_ip, fcount, DEBUG_POINT_RTA_REGS_E);
		ret = __is_hw_ypp_set_rta_regs(hw_ip, set_id, instance);
		_is_hw_frame_dbg_trace(hw_ip, fcount, DEBUG_POINT_RTA_REGS_X);

		if (ret) {
			mserr_hw("__is_hw_ypp_set_rta_regs is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}

		stripe_index = param_set->stripe_input.index;
		strip_start_pos = (stripe_index) ?
			(param_set->stripe_input.stripe_roi_start_pos_x[stripe_index] - STRIPE_MARGIN_WIDTH) : 0;

		ret = __is_hw_ypp_set_size_regs(hw_ip, param_set, instance,
				strip_start_pos, frame, set_id);
		if (ret) {
			mserr_hw("__is_hw_ypp_set_size_regs is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	} else {
		spin_lock_irqsave(&ypp_out_slock, flag);

		__is_hw_ypp_update_block_reg(hw_ip, param_set, instance, set_id);

		spin_unlock_irqrestore(&ypp_out_slock, flag);

		ypp_hw_s_coutfifo_size(hw_ip->regs[REG_SETA], set_id,
			param_set->otf_output.width,
			param_set->otf_output.height, 0);
	}

	if (IS_ENABLED(YPP_DDK_LIB_CALL))
		max_dma_id = YUVPP_RDMA_MAX;
	else
		max_dma_id = YUVPP_RDMA_L2_Y;


	for (i = YUVPP_RDMA_L0_Y; i < max_dma_id; i++) {
		ret = __is_hw_ypp_set_rdma(hw_ip, hw_ypp, param_set, instance,
				i, set_id);
		if (ret) {
			mserr_hw("__is_hw_ypp_set_rdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	if (en_votf == OTF_INPUT_COMMAND_ENABLE) {
		ret = is_hw_ypp_set_votf_size_config(param_set->dma_input.width, param_set->dma_input.height);
		if (ret) {
			mserr_hw("is_hw_ypp_set_votf_size_config is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	ret = is_hw_ypp_set_cmdq(hw_ip, instance, set_id);
	if (ret) {
		mserr_hw("is_hw_ypp_set_cmdq is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	if (unlikely(debug_ypp))
		ypp_hw_dump(hw_ip->regs[REG_SETA]);

	set_bit(HW_CONFIG, &hw_ip->state);

	return 0;

shot_fail_recovery:
	CALL_HW_HELPER_OPS(hw_ip, restore, instance, &hw_ypp->lib[instance]);

	return ret;
}

static int is_hw_ypp_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	struct is_hw_ypp *hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, get_meta, frame->instance,
				frame, &hw_ypp->lib[frame->instance]);
}

static int is_hw_ypp_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
		enum ShotErrorType done_type)
{
	int ret = 0;
	int output_id;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	output_id = IS_HW_CORE_END;
	if (test_bit(hw_ip->id, &frame->core_flag)) {
		ret = CALL_HW_OPS(hw_ip, frame_done, hw_ip, frame, -1,
			output_id, done_type, false);
	}

	return ret;
}


static int is_hw_ypp_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_ypp *hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, load_setfile, instance, &hw_ypp->lib[instance]);
}

static int is_hw_ypp_apply_setfile(struct is_hw_ip *hw_ip, u32 scenario,
	u32 instance, ulong hw_map)
{
	struct is_hw_ypp *hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, apply_setfile, instance, &hw_ypp->lib[instance],
				scenario);
}

static int is_hw_ypp_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_ypp *hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, delete_setfile, instance, &hw_ypp->lib[instance]);
}

int is_hw_ypp_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_ypp *hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	if (hw_ip->lib_mode == USE_DRIVER) {
		msinfo_hw("hw_ypp_reset\n", instance, hw_ip);

		ret = is_hw_ypp_reset_votf();
		if (ret) {
			mserr_hw("votf reset fail", instance, hw_ip);
			return ret;
		}

		if (__is_hw_ypp_s_common_reg(hw_ip, instance)) {
			mserr_hw("sw reset fail", instance, hw_ip);
			return -ENODEV;
		}
	}

	return CALL_HW_HELPER_OPS(hw_ip, restore, instance, &hw_ypp->lib[instance]);
}

static int is_hw_ypp_set_regs(struct is_hw_ip *hw_ip, u32 chain_id,
	u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size)
{
	return CALL_HW_HELPER_OPS(hw_ip, set_regs, instance, regs, regs_size);
}

int is_hw_ypp_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size,
	enum is_reg_dump_type dump_type)
{
	void __iomem *base;
	int i;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!regs);

	base = hw_ip->regs[REG_SETA];
	for (i = 0; i < regs_size; i++) {
		regs[i].reg_data = readl(base + regs[i].reg_addr);
		msinfo_hw("<0x%x, 0x%x>\n", instance, hw_ip,
				regs[i].reg_addr, regs[i].reg_data);
	}

	return 0;
}

static int is_hw_ypp_set_config(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount, void *conf)
{
	struct is_hw_ypp *hw_ypp = NULL;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!conf);

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	memcpy(&hw_ypp->config, conf, sizeof(hw_ypp->config));

	return __is_hw_ypp_set_rdma_cmd(&hw_ypp->param_set[instance], instance,
		&hw_ypp->config);
}

static int is_hw_ypp_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_ypp *hw_ypp;

	hw_ypp = (struct is_hw_ypp *)hw_ip->priv_info;
	if (!hw_ypp) {
		mserr_hw("failed to get HW YPP", instance, hw_ip);
		return -ENODEV;
	}

	return CALL_HW_HELPER_OPS(hw_ip, notify_timeout, instance, &hw_ypp->lib[instance]);
}

const struct is_hw_ip_ops is_hw_ypp_ops = {
	.open			= is_hw_ypp_open,
	.init			= is_hw_ypp_init,
	.deinit			= is_hw_ypp_deinit,
	.close			= is_hw_ypp_close,
	.enable			= is_hw_ypp_enable,
	.disable		= is_hw_ypp_disable,
	.shot			= is_hw_ypp_shot,
	.set_param		= is_hw_ypp_set_param,
	.get_meta		= is_hw_ypp_get_meta,
	.frame_ndone		= is_hw_ypp_frame_ndone,
	.load_setfile		= is_hw_ypp_load_setfile,
	.apply_setfile		= is_hw_ypp_apply_setfile,
	.delete_setfile		= is_hw_ypp_delete_setfile,
	.restore		= is_hw_ypp_restore,
	.set_regs		= is_hw_ypp_set_regs,
	.dump_regs		= is_hw_ypp_dump_regs,
	.set_config		= is_hw_ypp_set_config,
	.notify_timeout		= is_hw_ypp_notify_timeout,
};

int is_hw_ypp_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int ret = 0;
	int hw_slot;

	hw_ip->ops  = &is_hw_ypp_ops;

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);

	itfc->itf_ip[hw_slot].handler[INTR_HWIP5].handler = &is_hw_ypp_handle_interrupt;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP5].id = INTR_HWIP5;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP5].valid = true;

	spin_lock_init(&ypp_out_slock);

	return ret;
}
