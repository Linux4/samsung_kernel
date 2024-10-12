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
#include "is-hw-yuvp-v1_1.h"
#include "is-err.h"
#include "api/is-hw-api-yuvp-v1_1.h"
#include "is-votfmgr.h"
#include "is-votf-id-table.h"
#include "is-stripe.h"

static struct is_yuvp_config config_default = {
	.clahe_bypass = 1,
	.vhist_grid_num = 0,
	.drc_bypass = 1,
	.drc_grid_w = 0,
	.drc_grid_h = 0,
	.biquad_scale_shift_adder = 0,
	.drc_grid_enabled_num = 1,
	.yuvnr_contents_aware_isp_en = 0,
	.ccm_contents_aware_isp_en = 0,
	.sharpen_contents_aware_isp_en = 0,
};

static int is_hw_yuvp_handle_interrupt0(u32 id, void *context)
{
	struct is_hardware *hardware;
	struct is_hw_ip *hw_ip;
	struct is_hw_yuvp *hw_yuvp;
	struct yuvp_param_set *param_set;
	u32 status, instance, hw_fcount, strip_index, strip_total_count;
	bool f_err = false;
	u32 cur_idx, batch_num;
	bool is_first_shot = true, is_last_shot = true;

	hw_ip = (struct is_hw_ip *)context;
	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	param_set = &hw_yuvp->param_set[instance];
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

	status = yuvp_hw_g_int_state0(hw_ip->regs[REG_SETA], true,
			(hw_ip->num_buffers & 0xffff), &hw_yuvp->irq_state[YUVP0_INTR])
			& yuvp_hw_g_int_mask0(hw_ip->regs[REG_SETA]);

	msdbg_hw(2, "YUVP0 interrupt status(0x%x)\n", instance, hw_ip, status);

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

	if (yuvp_hw_is_occurred0(status, INTR_SETTING_DONE))
		CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_SETTING_DONE);

	if (yuvp_hw_is_occurred0(status, INTR_FRAME_START) && yuvp_hw_is_occurred0(status, INTR_FRAME_END))
		mswarn_hw("start/end overlapped!! (0x%x)", instance, hw_ip, status);

	if (yuvp_hw_is_occurred0(status, INTR_FRAME_START)) {
		if (IS_ENABLED(YPP_DDK_LIB_CALL) && is_first_shot) {
			is_lib_isp_event_notifier(hw_ip, &hw_yuvp->lib[instance], instance, hw_fcount,
				YUVP_EVENT_FRAME_START, strip_index, NULL);
		} else {
			atomic_add((hw_ip->num_buffers & 0xffff), &hw_ip->count.fs);
			CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_FRAME_START);
			if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
				msinfo_hw("[F:%d]F.S\n", instance, hw_ip, hw_fcount);

			CALL_HW_OPS(hw_ip, frame_start, hw_ip, instance);
		}
	}

	if (yuvp_hw_is_occurred0(status, INTR_FRAME_END)) {
		if (IS_ENABLED(YPP_DDK_LIB_CALL) && is_last_shot) {
			is_lib_isp_event_notifier(hw_ip, &hw_yuvp->lib[instance], instance, hw_fcount,
				YUVP_EVENT_FRAME_END, strip_index, NULL);
		} else {
			CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_FRAME_END);
			atomic_add((hw_ip->num_buffers & 0xffff), &hw_ip->count.fe);

			CALL_HW_OPS(hw_ip, frame_done, hw_ip, NULL, -1, IS_HW_CORE_END,
					IS_SHOT_SUCCESS, true);

			if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
				msinfo_hw("[F:%d]F.E\n", instance, hw_ip, hw_fcount);

			if (atomic_read(&hw_ip->count.fs) < atomic_read(&hw_ip->count.fe)) {
				mserr_hw("fs(%d), fe(%d), dma(%d), status(0x%x)", instance, hw_ip,
						atomic_read(&hw_ip->count.fs),
						atomic_read(&hw_ip->count.fe),
						atomic_read(&hw_ip->count.dma), status);
			}
			wake_up(&hw_ip->status.wait_queue);
		}
		if (unlikely(is_get_debug_param(IS_DEBUG_PARAM_YUVP)))
			yuvp_hw_print_chain_debug_cnt(hw_ip->regs[REG_SETA]);
	}

	f_err = yuvp_hw_is_occurred0(status, INTR_ERR);
	if (f_err) {
		msinfo_hw("[YUVP0][F:%d] Ocurred error interrupt : status(0x%x)\n",
			instance, hw_ip, hw_fcount, status);
		is_debug_s2d(true, "YUVP0 HW Error");
	}

	return 0;
}

static int is_hw_yuvp_handle_interrupt1(u32 id, void *context)
{
	struct is_hardware *hardware;
	struct is_hw_ip *hw_ip;
	struct is_hw_yuvp *hw_yuvp;
	u32 status, instance, hw_fcount, strip_index;
	bool f_err;

	hw_ip = (struct is_hw_ip *)context;
	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	strip_index = atomic_read(&hw_yuvp->strip_index);

	status = yuvp_hw_g_int_state1(hw_ip->regs[REG_SETA], true,
			(hw_ip->num_buffers & 0xffff), &hw_yuvp->irq_state[YUVP1_INTR])
			& yuvp_hw_g_int_mask1(hw_ip->regs[REG_SETA]);

	msdbg_hw(2, "YUVP1 interrupt status(0x%x)\n", instance, hw_ip, status);

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

	f_err = yuvp_hw_is_occurred1(status, INTR_ERR);
	if (f_err) {
		msinfo_hw("[YUVP1][F:%d] Ocurred error interrupt : status(0x%x)\n",
			instance, hw_ip, hw_fcount, status);
		is_debug_s2d(true, "YUVP1 HW Error");
	}

	return 0;
}

static int __is_hw_yuvp_s_reset(struct is_hw_ip *hw_ip, struct is_hw_yuvp *hw_yuvp)
{
	int i;

	sdbg_hw(4, "[HW] %s\n", hw_ip, __func__);
	for (i = 0; i < COREX_MAX; i++)
		hw_ip->cur_hw_iq_set[i].size = 0;

	return yuvp_hw_s_reset(hw_ip->regs[REG_SETA]);
}

static int __is_hw_yuvp_s_common_reg(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_yuvp *hw_yuvp;
	u32 seed;

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;

	msinfo_hw("reset\n", instance, hw_ip);

	if (__is_hw_yuvp_s_reset(hw_ip, hw_yuvp)) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}

	yuvp_hw_s_init(hw_ip->regs[REG_SETA]);

	seed = is_get_debug_param(IS_DEBUG_PARAM_CRC_SEED);
	if (unlikely(seed))
		yuvp_hw_s_crc(hw_ip->regs[REG_SETA], seed);

	msinfo_hw("clear interrupt\n", instance, hw_ip);

	return 0;
}

static int __is_hw_yuvp_clear_common(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_yuvp *hw_yuvp;

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;

	if (__is_hw_yuvp_s_reset(hw_ip, hw_yuvp)) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}

	if (yuvp_hw_wait_idle(hw_ip->regs[REG_SETA]))
		mserr_hw("failed to yuvp_hw_wait_idle", instance, hw_ip);

	msinfo_hw("final finished yuvp\n", instance, hw_ip);

	return 0;
}

static int is_hw_yuvp_set_cmdq(struct is_hw_ip *hw_ip, u32 instance, u32 set_id)
{
	/*
	 * add to command queue
	 * 1. set corex_update_type_0_setX
	 * normal shot: SETA(wide), SETB(tele, front), SETC(uw)
	 * reprocessing shot: SETD
	 * 2. set cq_frm_start_type to 0. only at first time?
	 * 3. set ms_add_to_queue(set id, frame id).
	 */
	int ret;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	ret = yuvp_hw_s_corex_update_type(hw_ip->regs[REG_SETA], set_id);
	if (ret)
		return ret;

	yuvp_hw_s_cmdq(hw_ip->regs[REG_SETA], set_id);

	return ret;
}

static int __nocfi is_hw_yuvp_open(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;
	struct is_hw_yuvp *hw_yuvp;

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HWYUVP");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw_ip->priv_info = vzalloc(sizeof(struct is_hw_yuvp));
	if (!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	hw_yuvp->instance = instance;

	ret = CALL_HW_HELPER_OPS(hw_ip, open, instance, &hw_yuvp->lib[instance],
				LIB_FUNC_YUVP);
	if (ret)
		goto err_chain_create;

	ret = CALL_HW_HELPER_OPS(hw_ip, alloc_iqset, yuvp_hw_g_reg_cnt());
	if (ret)
		goto err_iqset_alloc;

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	set_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "open: framemgr[%s]", instance, hw_ip, hw_ip->framemgr->name);

	return 0;

err_iqset_alloc:
	CALL_HW_HELPER_OPS(hw_ip, close, instance, &hw_yuvp->lib[instance]);

err_chain_create:
	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_yuvp_init(struct is_hw_ip *hw_ip, u32 instance,
			bool flag, u32 f_type)
{
	int ret;
	struct is_hw_yuvp *hw_yuvp;
	u32 input_id;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	if (!hw_yuvp) {
		mserr_hw("hw_yuvp is null ", instance, hw_ip);
		ret = -ENODATA;
		goto err;
	}

	ret = CALL_HW_HELPER_OPS(hw_ip, init, instance, &hw_yuvp->lib[instance],
				(u32)flag, f_type, LIB_FUNC_YUVP);
	if (ret)
		return ret;

	for (input_id = YUVP_RDMA_Y; input_id < YUVP_DMA_MAX; input_id++) {
		ret = yuvp_hw_dma_create(&hw_yuvp->rdma[input_id], hw_ip->regs[REG_SETA], input_id);
		if (ret) {
			mserr_hw("yuvp_hw_dma_create error[%d]", instance, hw_ip, input_id);
			ret = -ENODATA;
			goto err;
		}
	}

	set_bit(HW_INIT, &hw_ip->state);
	return 0;

err:
	return ret;

}

static int is_hw_yuvp_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_yuvp *hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, deinit, instance, &hw_yuvp->lib[instance]);
}

static int is_hw_yuvp_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_yuvp *hw_yuvp;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;

	CALL_HW_HELPER_OPS(hw_ip, close, instance, &hw_yuvp->lib[instance]);

	__is_hw_yuvp_clear_common(hw_ip, instance);

	CALL_HW_HELPER_OPS(hw_ip, free_iqset);

	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	clear_bit(HW_OPEN, &hw_ip->state);

	return 0;
}

static int is_hw_yuvp_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_yuvp *hw_yuvp;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	if (test_bit(HW_RUN, &hw_ip->state))
		return 0;

	msdbg_hw(2, "enable: start\n", instance, hw_ip);

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;

	__is_hw_yuvp_s_common_reg(hw_ip, instance);

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return 0;
}

static int is_hw_yuvp_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_yuvp *hw_yuvp;
	struct yuvp_param_set *param_set;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("yuvp_disable: Vvalid(%d)\n", instance, hw_ip,
		atomic_read(&hw_ip->status.Vvalid));

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;

	timetowait = wait_event_timeout(hw_ip->status.wait_queue,
		!atomic_read(&hw_ip->status.Vvalid),
		IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		mserr_hw("wait FRAME_END timeout (%ld)", instance,
			hw_ip, timetowait);
		ret = -ETIME;
	}

	param_set = &hw_yuvp->param_set[instance];
	param_set->fcount = 0;

	CALL_HW_HELPER_OPS(hw_ip, disable, instance, &hw_yuvp->lib[instance]);

	if (hw_ip->run_rsc_state)
		return 0;

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static int __is_hw_yuvp_set_rdma_cmd(struct yuvp_param_set *param_set, u32 instance,
	struct is_yuvp_config *conf)
{
	mdbg_hw(4, "%s\n", instance, __func__);

	if (conf->clahe_bypass & 0x1)
		param_set->dma_input_clahe.cmd = DMA_INPUT_COMMAND_DISABLE;

	if (conf->drc_bypass & 0x1)
		param_set->dma_input_drc.cmd = DMA_INPUT_COMMAND_DISABLE;

	if (!conf->yuvnr_contents_aware_isp_en &&
		!conf->ccm_contents_aware_isp_en &&
		!conf->sharpen_contents_aware_isp_en)
		param_set->dma_input_seg.cmd = DMA_INPUT_COMMAND_DISABLE;

	return 0;
}

static int __is_hw_yuvp_get_input_dva(u32 id, u32 *cmd, pdma_addr_t **input_dva,
	struct yuvp_param_set *param_set, u32 grid_en)
{
	switch (id) {
	case YUVP_RDMA_Y:
	case YUVP_RDMA_UV:
		*input_dva = param_set->input_dva;
		*cmd = param_set->dma_input.cmd;
		break;
	case YUVP_RDMA_SEG:
		*input_dva = param_set->input_dva_seg;
		*cmd = param_set->dma_input_seg.cmd;
		break;
	case YUVP_RDMA_DRC0:
		*input_dva = param_set->input_dva_drc;
		*cmd = param_set->dma_input_drc.cmd;
		break;
	case YUVP_RDMA_DRC1:
		*input_dva = param_set->input_dva_drc;
		*cmd = (grid_en == 1) ? DMA_INPUT_COMMAND_DISABLE : param_set->dma_input_drc.cmd;
		break;
	case YUVP_RDMA_CLAHE:
		*input_dva = param_set->input_dva_clahe;
		*cmd = param_set->dma_input_clahe.cmd;
		break;
	case YUVP_WDMA_Y:
	case YUVP_WDMA_UV:
		*input_dva = param_set->output_dva_yuv;
		*cmd = param_set->dma_output_yuv.cmd;
		break;
	default:
		return -1;
	}
	return 0;
}

static int __is_hw_yuvp_get_cache_hint(u32 id, u32 *cache_hint)
{
	switch (id) {
		case YUVP_RDMA_Y:
		case YUVP_RDMA_UV:
		case YUVP_RDMA_SEG:
		case YUVP_RDMA_CLAHE:
		case YUVP_RDMA_DRC0:
		case YUVP_RDMA_DRC1:
		case YUVP_WDMA_Y:
		case YUVP_WDMA_UV:
			*cache_hint = 0x7;	/* 111: last-access-read */
			return 0;
	default:
		return -1;
	}
}

static int __is_hw_yuvp_set_rdma(struct is_hw_ip *hw_ip, struct is_hw_yuvp *hw_yuvp,
			struct yuvp_param_set *param_set, u32 instance, u32 id, u32 set_id)
{
	pdma_addr_t *input_dva = NULL;
	u32 cmd;
	u32 comp_sbwc_en, payload_size;
	u32 in_crop_size_x = 0;
	u32 cache_hint;
	int ret;

	if (__is_hw_yuvp_get_input_dva(id, &cmd, &input_dva, param_set,
		hw_yuvp->config.drc_grid_enabled_num) < 0) {
		merr_hw("invalid ID (%d)", instance,  id);
		return -EINVAL;
	}

	if (__is_hw_yuvp_get_cache_hint(id, &cache_hint) < 0) {
		merr_hw("invalid ID (%d)", instance,  id);
		return -EINVAL;
	}


	msdbg_hw(2, "%s: %d\n", instance, hw_ip, hw_yuvp->rdma[id].name, cmd);

	yuvp_hw_s_rdma_corex_id(&hw_yuvp->rdma[id], set_id);

	ret = yuvp_hw_s_rdma_init(&hw_yuvp->rdma[id], param_set, cmd,
			hw_yuvp->config.vhist_grid_num, hw_yuvp->config.drc_grid_w, hw_yuvp->config.drc_grid_h,
			in_crop_size_x, cache_hint, &comp_sbwc_en, &payload_size);
	if (ret) {
		mserr_hw("failed to initialize YUVPP_RDMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (cmd == DMA_INPUT_COMMAND_ENABLE) {
		ret = yuvp_hw_s_rdma_addr(&hw_yuvp->rdma[id], input_dva, 0,
			(hw_ip->num_buffers & 0xffff),
			0, comp_sbwc_en, payload_size);
		if (ret) {
			mserr_hw("failed to set YUVPP_RDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
	}

	return 0;
}

static void __is_hw_yuvp_check_size(struct is_hw_ip *hw_ip, struct yuvp_param_set *param_set, u32 instance)
{
	struct param_dma_input *input = &param_set->dma_input;
	struct param_otf_output *output = &param_set->otf_output;

	msdbgs_hw(2, "hw_yuvp_check_size >>>\n", instance, hw_ip);
	msdbgs_hw(2, "dma_input: format(%d),crop_size(%dx%d)\n", instance, hw_ip,
		input->format, input->dma_crop_width, input->dma_crop_height);
	msdbgs_hw(2, "otf_output: otf_cmd(%d),format(%d)\n", instance, hw_ip,
		output->cmd, output->format);
	msdbgs_hw(2, "otf_output: pos(%d,%d),crop%dx%d),size(%dx%d)\n", instance, hw_ip,
		output->crop_offset_x, output->crop_offset_y,
		output->crop_width, output->crop_height,
		output->width, output->height);
	msdbgs_hw(2, "<<< hw_yuvp_check_size <<<\n", instance, hw_ip);
}

static int __is_hw_yuvp_bypass(struct is_hw_ip *hw_ip, u32 set_id)
{
	sdbg_hw(4, "%s\n", hw_ip, __func__);

	yuvp_hw_s_block_bypass(hw_ip->regs[REG_SETA], set_id);

	return 0;
}


static int __is_hw_yuvp_update_block_reg(struct is_hw_ip *hw_ip, struct yuvp_param_set *param_set, u32 instance, u32 set_id)
{
	struct is_hw_yuvp *hw_yuvp;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;

	if (hw_yuvp->instance != instance) {
		msdbg_hw(2, "update_param: hw_ip->instance(%d)\n",
			instance, hw_ip, hw_yuvp->instance);
		hw_yuvp->instance = instance;
	}

	__is_hw_yuvp_check_size(hw_ip, param_set, instance);
	return __is_hw_yuvp_bypass(hw_ip, set_id);
}

static void __is_hw_yuvp_update_param(struct is_hw_ip *hw_ip, struct is_param_region *p_region,
					struct yuvp_param_set *param_set, IS_DECLARE_PMAP(pmap),
					u32 instance)
{
	struct yuvp_param *param;

	param = &p_region->yuvp;
	param_set->instance_id = instance;

	/* check input */
	if (test_bit(PARAM_YUVP_OTF_INPUT, pmap)) {
		memcpy(&param_set->otf_input, &param->otf_input,
			sizeof(struct param_otf_input));
		msdbg_hw(4, "%s : PARAM_YUVP_OTF_INPUT\n", instance, hw_ip, __func__);
	}

	if (test_bit(PARAM_YUVP_DMA_INPUT, pmap)) {
		memcpy(&param_set->dma_input, &param->dma_input,
			sizeof(struct param_dma_input));
		msdbg_hw(4, "%s : PARAM_YUVP_DMA_INPUT\n", instance, hw_ip, __func__);
	}

	if (test_bit(PARAM_YUVP_DRC, pmap)) {
		memcpy(&param_set->dma_input_drc, &param->drc,
			sizeof(struct param_dma_input));
		msdbg_hw(4, "%s : PARAM_YUVP_DRC\n", instance, hw_ip, __func__);
	}

	if (test_bit(PARAM_YUVP_CLAHE, pmap)) {
		memcpy(&param_set->dma_input_clahe, &param->clahe,
			sizeof(struct param_dma_input));
		msdbg_hw(4, "%s : PARAM_YUVP_CLAHE\n", instance, hw_ip, __func__);
	}

	if (test_bit(PARAM_YUVP_SEG, pmap)) {
		memcpy(&param_set->dma_input_seg, &param->seg,
			sizeof(struct param_dma_input));
		msdbg_hw(4, "%s : PARAM_YUVP_SEG_MAP\n", instance, hw_ip, __func__);
	}

	/* check output */
	if (test_bit(PARAM_YUVP_OTF_OUTPUT, pmap)) {
		memcpy(&param_set->otf_output, &param->otf_output,
			sizeof(struct param_otf_output));
		msdbg_hw(4, "%s : PARAM_YUVP_OTF_OUTPUT\n", instance, hw_ip, __func__);
	}

	if (test_bit(PARAM_YUVP_YUV, pmap)) {
		memcpy(&param_set->dma_output_yuv, &param->yuv,
			sizeof(struct param_dma_output));
		msdbg_hw(4, "%s : PARAM_YUVP_DMA_OUTPUT\n", instance, hw_ip, __func__);
	}

	if (test_bit(PARAM_YUVP_STRIPE_INPUT, pmap)) {
		memcpy(&param_set->stripe_input, &param->stripe_input,
			sizeof(struct param_stripe_input));
		msdbg_hw(4, "%s : PARAM_YUVP_STRIPE_INPUT\n", instance, hw_ip, __func__);
	}
}

static int is_hw_yuvp_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
		IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	struct is_hw_yuvp *hw_yuvp;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	hw_yuvp->instance = IS_STREAM_COUNT;

	return 0;
}

static int __is_hw_yuvp_set_size_regs(struct is_hw_ip *hw_ip, struct yuvp_param_set *param_set,
	u32 instance, u32 yuvpp_strip_start_pos, const struct is_frame *frame, u32 set_id,
	enum yuvp_input_path_type input_path, enum yuvp_chain_demux_type output_path)
{
	struct is_hw_yuvp *hw_yuvp;
	struct is_yuvp_config *config;
	int ret;
	u32 strip_enable;
	u32 width, height;
	u32 frame_width;
	u32 region_id;
	struct yuvp_radial_cfg radial_cfg;

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	config = &hw_yuvp->config;

	strip_enable = (param_set->stripe_input.total_count == 0) ? 0 : 1;

	region_id = param_set->stripe_input.index;

	if (input_path == YUVP_INPUT_RDMA) {
		width = param_set->dma_input.width;
		height = param_set->dma_input.height;
	} else {
		width = param_set->otf_input.width;
		height = param_set->otf_input.height;
	}
	msdbg_hw(4, "%s : path %d, (w x h) = (%d x %d)\n", instance, hw_ip, __func__, input_path, width, height);

	frame_width = (strip_enable) ? param_set->stripe_input.full_width : width;

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

	ret = yuvp_hw_s_yuvnr_size(hw_ip->regs[REG_SETA], set_id, yuvpp_strip_start_pos, frame_width,
		width, height, strip_enable, &radial_cfg, config->biquad_scale_shift_adder);
	if (ret) {
		mserr_hw("failed to set size regs for yuvnr", instance, hw_ip);
		goto err;
	}

	ret = yuvp_hw_s_drc_size(hw_ip->regs[REG_SETA], set_id, yuvpp_strip_start_pos, frame_width,
		width, height, strip_enable, &radial_cfg);
	if (ret) {
		mserr_hw("failed to set size regs for drc", instance, hw_ip);
		goto err;
	}

	ret = yuvp_hw_s_clahe_size(hw_ip->regs[REG_SETA], set_id, yuvpp_strip_start_pos, frame_width,
		width, height, strip_enable, &radial_cfg);
	if (ret) {
		mserr_hw("failed to set size regs for clahe", instance, hw_ip);
		goto err;
	}

	ret = yuvp_hw_s_sharpen_size(hw_ip->regs[REG_SETA], set_id, yuvpp_strip_start_pos, frame_width,
		width, height, strip_enable, &radial_cfg);
	if (ret) {
		mserr_hw("failed to set size regs for sharp adder", instance, hw_ip);
		goto err;
	}

	yuvp_hw_s_size(hw_ip->regs[REG_SETA], set_id, width, height, strip_enable);
err:
	return ret;
}

static void __is_hw_yuvp_config_path(struct is_hw_ip *hw_ip, struct yuvp_param_set *param_set,
	u32 set_id, enum yuvp_input_path_type *input_path, enum yuvp_chain_demux_type *output_path)
{
	struct is_hw_yuvp *hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;

	if (param_set->dma_input.cmd == DMA_INPUT_COMMAND_ENABLE) {
		*input_path = YUVP_INPUT_RDMA;
		yuvp_hw_s_mux_dtp(hw_ip->regs[REG_SETA], set_id, YUVP_MUX_DTP_RDMA_YUV);
	} else {
		*input_path = YUVP_INPUT_OTF;
		yuvp_hw_s_cinfifo(hw_ip->regs[REG_SETA], set_id);
		yuvp_hw_s_mux_dtp(hw_ip->regs[REG_SETA], set_id, YUVP_MUX_DTP_CINFIFO);
	}
	yuvp_hw_s_input_path(hw_ip->regs[REG_SETA], set_id, *input_path);

	*output_path = 0;

	if (param_set->dma_output_yuv.cmd == DMA_INPUT_COMMAND_ENABLE)
		*output_path |= YUVP_DEMUX_DITHER_WDMA;

	if (param_set->otf_output.cmd == DMA_INPUT_COMMAND_ENABLE) {
		*output_path |= YUVP_DEMUX_DITHER_COUTFIFO_0;
		yuvp_hw_s_coutfifo(hw_ip->regs[REG_SETA], set_id);
		yuvp_hw_s_output_path(hw_ip->regs[REG_SETA], set_id, 1);
	} else {
		yuvp_hw_s_output_path(hw_ip->regs[REG_SETA], set_id, 0);
	}

	*output_path |= YUVP_DEMUX_YUVNR_YUV422TO444;

	if (param_set->dma_input_seg.cmd == DMA_INPUT_COMMAND_ENABLE) {
		if (hw_yuvp->config.yuvnr_contents_aware_isp_en)
			*output_path |= YUVP_DEMUX_SEGCONF_YUVNR;
		if (hw_yuvp->config.ccm_contents_aware_isp_en)
			*output_path |= YUVP_DEMUX_SEGCONF_CCM;
		if (hw_yuvp->config.sharpen_contents_aware_isp_en)
			*output_path |= YUVP_DEMUX_SEGCONF_SHARPEN;
	}

	yuvp_hw_s_demux_enable(hw_ip->regs[REG_SETA], set_id, *output_path);
}

static void __is_hw_yuvp_config_dma(struct is_hw_ip *hw_ip, struct yuvp_param_set *param_set,
	struct is_frame *frame, u32 fcount, u32 instance)
{
	u32 cur_idx;
	u32 cmd_input, cmd_input_drc, cmd_input_hist, cmd_input_scmap;
	int batch_num;

	cur_idx = frame->cur_buf_index;

	cmd_input = CALL_HW_OPS(hw_ip, dma_cfg, "yuvp", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_input.cmd,
			param_set->dma_input.plane,
			param_set->input_dva,
			frame->dvaddr_buffer);

	cmd_input = CALL_HW_OPS(hw_ip, dma_cfg, "yuvpout", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_output_yuv.cmd,
			param_set->dma_output_yuv.plane,
			param_set->output_dva_yuv,
			frame->ypnrdsTargetAddress);

	cmd_input_drc = CALL_HW_OPS(hw_ip, dma_cfg, "ypdga", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_input_drc.cmd,
			param_set->dma_input_drc.plane,
			param_set->input_dva_drc,
			frame->ypdgaTargetAddress);

	/* only 1 CLAHE buffer is valid per 1 batch shot
	 * always use index 0 buffer for batch mode
	 */
	cmd_input_hist = CALL_HW_OPS(hw_ip, dma_cfg, "ypclahe", hw_ip,
			frame, 0, frame->num_buffers,
			&param_set->dma_input_clahe.cmd,
			param_set->dma_input_clahe.plane,
			param_set->input_dva_clahe,
			frame->ypclaheTargetAddress);

	cmd_input_scmap = CALL_HW_OPS(hw_ip, dma_cfg, "scmap", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_input_seg.cmd,
			param_set->dma_input_seg.plane,
			param_set->input_dva_seg,
			frame->ixscmapTargetAddress);

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

}

static int is_hw_yuvp_set_config(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount, void *config)
{
	struct is_hw_yuvp *hw_yuvp;
	struct is_yuvp_config *conf = (struct is_yuvp_config *)config;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);
	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!conf);

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	hw_yuvp->config = *conf;

	return 0;
}

static int is_hw_yuvp_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	struct is_hw_yuvp *hw_yuvp;
	struct is_param_region *param_region;
	struct yuvp_param_set *param_set;
	struct is_frame *dma_frame;
	u32 fcount, instance;
	u32 cur_idx, batch_num;
	u32 strip_start_pos;
	u32 set_id;
	int ret, i;
	u32 strip_index, strip_total_count;
	enum yuvp_input_path_type input_path;
	enum yuvp_chain_demux_type output_path;
	bool do_blk_cfg;
	u32 cmd, bypass;
	bool skip = false;

	instance = frame->instance;
	msdbgs_hw(2, "[F:%d]shot\n", instance, hw_ip, frame->fcount);

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

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	param_set = &hw_yuvp->param_set[instance];
	param_region = frame->parameter;

	atomic_set(&hw_yuvp->strip_index, frame->stripe_info.region_id);
	fcount = frame->fcount;
	dma_frame = frame;

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

	cur_idx = frame->cur_buf_index;

	__is_hw_yuvp_update_param(hw_ip, param_region, param_set, frame->pmap, instance);
	__is_hw_yuvp_config_dma(hw_ip, param_set, frame, fcount, instance);

	cur_idx = (hw_ip->num_buffers >> CURR_INDEX_SHIFT) & 0xff;
	batch_num = (hw_ip->num_buffers >> SW_FRO_NUM_SHIFT) & 0xff;

	strip_index = param_set->stripe_input.index;
	strip_total_count = param_set->stripe_input.total_count;
	strip_start_pos = (strip_index) ? (param_set->stripe_input.start_pos_x) : 0;

	if (IS_ENABLED(SKIP_ISP_SHOT_FOR_MULTI_SHOT)) {
		if (!pablo_is_first_shot(batch_num, cur_idx)) skip = true;
		if (!pablo_is_first_shot(strip_total_count, strip_index)) skip = true;
		if (!pablo_is_first_shot(param_set->stripe_input.repeat_num,
			param_set->stripe_input.repeat_idx)) skip = true;
	}

	/* temp direct set*/
	set_id = COREX_DIRECT;

	yuvp_hw_s_core(hw_ip->regs[REG_SETA], frame->num_buffers, set_id);

	ret = CALL_HW_HELPER_OPS(hw_ip, lib_shot, instance, skip,
				frame, &hw_yuvp->lib[instance], param_set);
	if (ret)
		return ret;

	do_blk_cfg = CALL_HW_HELPER_OPS(hw_ip, set_rta_regs, instance, set_id, skip, frame, NULL);
	if (unlikely(do_blk_cfg)) {
		is_hw_yuvp_set_config(hw_ip, 0, instance, fcount, &config_default);
		__is_hw_yuvp_update_block_reg(hw_ip, param_set, instance, set_id);
	}

	if (param_set->stripe_input.repeat_scenario == PABLO_REPEAT_YUVP_CLAHE) {
		if (!param_set->stripe_input.repeat_idx) {
			hw_yuvp->config.clahe_bypass = 1;
			yuvp_hw_s_clahe_bypass(hw_ip->regs[REG_SETA], set_id, 1);
		} else {
			hw_yuvp->config.clahe_bypass = 0;
			yuvp_hw_s_clahe_bypass(hw_ip->regs[REG_SETA], set_id, 0);
		}
	}

	__is_hw_yuvp_config_path(hw_ip, param_set, set_id, &input_path, &output_path);
	__is_hw_yuvp_set_rdma_cmd(param_set, instance, &hw_yuvp->config);

	ret = __is_hw_yuvp_set_size_regs(hw_ip, param_set, instance, strip_start_pos, frame,
					 set_id, input_path, output_path);
	if (ret) {
		mserr_hw("__is_hw_yuvp_set_size_regs is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	for (i = YUVP_RDMA_Y; i < YUVP_DMA_MAX; i++) {
		ret = __is_hw_yuvp_set_rdma(hw_ip, hw_yuvp, param_set, instance, i, set_id);
		if (ret) {
			mserr_hw("__is_hw_yuvp_set_rdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	cmd = param_set->dma_input_clahe.cmd;
	bypass = hw_yuvp->config.clahe_bypass;
	if ((cmd == DMA_INPUT_COMMAND_DISABLE) && !bypass && IS_ENABLED(YPP_DDK_LIB_CALL)) {
		mserr_hw("clahe rdma setting mismatched : rdma %d, bypass %d\n",
			instance, hw_ip, cmd, bypass);
		ret = -EINVAL;
		goto shot_fail_recovery;
	}

	cmd = param_set->dma_input_drc.cmd;
	bypass = hw_yuvp->config.drc_bypass;
	if ((cmd == DMA_INPUT_COMMAND_DISABLE) && !bypass && IS_ENABLED(YPP_DDK_LIB_CALL)) {
		mserr_hw("drc rdma setting mismatched : rdma %d, bypass %d\n",
			instance, hw_ip, cmd, bypass);
		ret = -EINVAL;
		goto shot_fail_recovery;
	}

	cmd = param_set->dma_input_seg.cmd;
	bypass = (!hw_yuvp->config.yuvnr_contents_aware_isp_en &&
			!hw_yuvp->config.ccm_contents_aware_isp_en &&
			!hw_yuvp->config.sharpen_contents_aware_isp_en);
	if ((cmd == DMA_INPUT_COMMAND_DISABLE) && !bypass && IS_ENABLED(YPP_DDK_LIB_CALL)) {
		mserr_hw("seg_conf rdma setting mismatched : rdma %d, enable %d\n",
			instance, hw_ip, cmd, bypass);
		ret = -EINVAL;
		goto shot_fail_recovery;
	}

	if (param_region->yuvp.control.strgen == CONTROL_COMMAND_START) {
		msdbg_hw(2, "[YUVP]STRGEN input\n", instance, hw_ip);
		yuvp_hw_s_strgen(hw_ip->regs[REG_SETA], set_id);
	}

	CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, fcount, DEBUG_POINT_ADD_TO_CMDQ);
	ret = is_hw_yuvp_set_cmdq(hw_ip, instance, set_id);
	if (ret) {
		mserr_hw("is_hw_yuvp_set_cmdq is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	if (unlikely(is_get_debug_param(IS_DEBUG_PARAM_YUVP))) {
		yuvp_hw_dump(hw_ip->regs[REG_SETA]);
	}
	set_bit(HW_CONFIG, &hw_ip->state);

	return 0;

shot_fail_recovery:
	CALL_HW_HELPER_OPS(hw_ip, restore, instance, &hw_yuvp->lib[instance]);

	return ret;
}

static int is_hw_yuvp_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	struct is_hw_yuvp *hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, get_meta, frame->instance,
				frame, &hw_yuvp->lib[frame->instance]);
}

static int is_hw_yuvp_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
		enum ShotErrorType done_type)
{
	int ret = 0;

	sdbg_hw(4, "%s\n", hw_ip, __func__);

	if (test_bit(hw_ip->id, &frame->core_flag)) {
		ret = CALL_HW_OPS(hw_ip, frame_done, hw_ip, frame, -1,
			IS_HW_CORE_END, done_type, false);
	}

	return ret;
}


static int is_hw_yuvp_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_yuvp *hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, load_setfile, instance, &hw_yuvp->lib[instance]);
}

static int is_hw_yuvp_apply_setfile(struct is_hw_ip *hw_ip, u32 scenario,
	u32 instance, ulong hw_map)
{
	struct is_hw_yuvp *hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, apply_setfile, instance, &hw_yuvp->lib[instance],
				scenario);
}

static int is_hw_yuvp_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_yuvp *hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, delete_setfile, instance, &hw_yuvp->lib[instance]);
}

int is_hw_yuvp_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_yuvp *hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	if (__is_hw_yuvp_s_common_reg(hw_ip, instance)) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}

	return CALL_HW_HELPER_OPS(hw_ip, restore, instance, &hw_yuvp->lib[instance]);
}

static int is_hw_yuvp_set_regs(struct is_hw_ip *hw_ip, u32 chain_id,
	u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size)
{
	return CALL_HW_HELPER_OPS(hw_ip, set_regs, instance, regs, regs_size);
}

int is_hw_yuvp_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size, enum is_reg_dump_type dump_type)
{
	void __iomem *base;
	struct is_common_dma *dma;
	struct is_hw_yuvp *hw_yuvp = NULL;
	u32 i;

	switch (dump_type) {
	case IS_REG_DUMP_TO_LOG:
		base = hw_ip->regs[REG_SETA];
		yuvp_hw_dump(base);
		break;
	case IS_REG_DUMP_DMA:
		for (i = YUVP_RDMA_Y; i <= YUVP_RDMA_CLAHE; i++) {
			hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
			dma = &hw_yuvp->rdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}

		for (i = YUVP_WDMA_Y; i < YUVP_DMA_MAX; i++) {
			hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
			dma = &hw_yuvp->rdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int is_hw_yuvp_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_yuvp *hw_yuvp;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);
	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	if (!hw_yuvp) {
		mserr_hw("failed to get HW YUVP", instance, hw_ip);
		return -ENODEV;
	}

	yuvp_hw_print_chain_debug_cnt(hw_ip->regs[REG_SETA]);
	yuvp_hw_dump(hw_ip->regs[REG_SETA]);

	return CALL_HW_HELPER_OPS(hw_ip, notify_timeout, instance, &hw_yuvp->lib[instance]);
}

const struct is_hw_ip_ops is_hw_yuvp_ops = {
	.open			= is_hw_yuvp_open,
	.init			= is_hw_yuvp_init,
	.deinit			= is_hw_yuvp_deinit,
	.close			= is_hw_yuvp_close,
	.enable			= is_hw_yuvp_enable,
	.disable		= is_hw_yuvp_disable,
	.shot			= is_hw_yuvp_shot,
	.set_param		= is_hw_yuvp_set_param,
	.get_meta		= is_hw_yuvp_get_meta,
	.frame_ndone		= is_hw_yuvp_frame_ndone,
	.load_setfile		= is_hw_yuvp_load_setfile,
	.apply_setfile		= is_hw_yuvp_apply_setfile,
	.delete_setfile		= is_hw_yuvp_delete_setfile,
	.restore		= is_hw_yuvp_restore,
	.set_regs		= is_hw_yuvp_set_regs,
	.dump_regs		= is_hw_yuvp_dump_regs,
	.set_config		= is_hw_yuvp_set_config,
	.notify_timeout		= is_hw_yuvp_notify_timeout,
};

int is_hw_yuvp_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int hw_slot;

	hw_ip->ops  = &is_hw_yuvp_ops;

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &is_hw_yuvp_handle_interrupt0;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].handler = &is_hw_yuvp_handle_interrupt1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].id = INTR_HWIP2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = true;

	return 0;
}
