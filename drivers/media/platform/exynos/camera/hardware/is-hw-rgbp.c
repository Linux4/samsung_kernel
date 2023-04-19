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

#include "is-hw-rgbp.h"
#include "is-err.h"
#include "api/is-hw-api-rgbp-v1_0.h"
#include "is-votfmgr.h"
#include "is-votf-id-table.h"
#include "votf/pablo-votf.h"
#include "pablo-hw-helper.h"
#include "is-stripe.h"

static int debug_rgbp;
module_param(debug_rgbp, int, 0644);

static int is_hw_rgbp_handle_interrupt0(u32 id, void *context)
{
	struct is_hardware *hardware = NULL;
	struct is_hw_ip *hw_ip = NULL;
	struct is_hw_rgbp *hw_rgbp = NULL;
	struct rgbp_param_set *param_set;
	u32 status, instance, hw_fcount, strip_index, strip_total_count, hl = 0, vl = 0;
	bool f_err = false;
	u32 cur_idx, batch_num;
	bool is_first_shot = true, is_last_shot = true;

	hw_ip = (struct is_hw_ip *)context;

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	param_set = &hw_rgbp->param_set[instance];
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

	status = rgbp_hw_g_int0_state(hw_ip->regs[REG_SETA], true,
			(hw_ip->num_buffers & 0xffff), &hw_rgbp->irq_state[RGBP_INTR0])
			& rgbp_hw_g_int0_mask(hw_ip->regs[REG_SETA]);

	msdbg_hw(2, "RGBP interrupt status(0x%x)\n", instance, hw_ip, status);

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

	if (rgbp_hw_is_occurred0(status, INTR_SETTING_DONE))
		 CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_SETTING_DONE);

	if (rgbp_hw_is_occurred0(status, INTR_FRAME_START) && rgbp_hw_is_occurred0(status, INTR_FRAME_END))
		mswarn_hw("start/end overlapped!! (0x%x)", instance, hw_ip, status);

	if (rgbp_hw_is_occurred0(status, INTR_FRAME_START)) {
		if (IS_ENABLED(RGBP_DDK_LIB_CALL) && is_first_shot) {
			is_lib_isp_event_notifier(hw_ip, &hw_rgbp->lib[instance],
					instance, hw_fcount, EVENT_FRAME_START,
					strip_index, NULL);
		} else {
			atomic_add((hw_ip->num_buffers & 0xffff), &hw_ip->count.fs);
			CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount,
					DEBUG_POINT_FRAME_START);
			if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
				msdbg_hw(2, "[F:%d]F.S\n", instance, hw_ip, hw_fcount);

			CALL_HW_OPS(hw_ip, frame_start, hw_ip, instance);
		}
	}

	if (rgbp_hw_is_occurred0(status, INTR_FRAME_END)) {
		if (IS_ENABLED(RGBP_DDK_LIB_CALL) && is_last_shot) {
			is_lib_isp_event_notifier(hw_ip, &hw_rgbp->lib[instance],
					instance, hw_fcount, EVENT_FRAME_END,
					strip_index, NULL);
		} else {
			CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount,
					DEBUG_POINT_FRAME_END);
			atomic_add((hw_ip->num_buffers & 0xffff), &hw_ip->count.fe);

			CALL_HW_OPS(hw_ip, frame_done, hw_ip, NULL, -1, IS_HW_CORE_END,
					IS_SHOT_SUCCESS, true);

			if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
				msdbg_hw(2, "[F:%d]F.E\n", instance, hw_ip, hw_fcount);

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

	f_err = rgbp_hw_is_occurred0(status, INTR_ERR0);

	if (f_err) {
		msinfo_hw("[F:%d] Ocurred error interrupt (%d,%d) status(0x%x)\n",
			instance, hw_ip, hw_fcount, hl, vl, status);
		rgbp_hw_dump(hw_ip->regs[REG_SETA]);
	}

	return 0;

}

static int is_hw_rgbp_handle_interrupt1(u32 id, void *context)
{
	struct is_hardware *hardware = NULL;
	struct is_hw_ip *hw_ip = NULL;
	struct is_hw_rgbp *hw_rgbp = NULL;
	u32 status, instance, hw_fcount, strip_index, hl = 0, vl = 0;
	bool f_err = false;

	hw_ip = (struct is_hw_ip *)context;

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	strip_index = atomic_read(&hw_rgbp->strip_index);

	status = rgbp_hw_g_int1_state(hw_ip->regs[REG_SETA], true,
			(hw_ip->num_buffers & 0xffff), &hw_rgbp->irq_state[RGBP_INTR1])
			& rgbp_hw_g_int1_mask(hw_ip->regs[REG_SETA]);

	msdbg_hw(2, "RGBP interrupt status1(0x%x)\n", instance, hw_ip, status);

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

	f_err = rgbp_hw_is_occurred1(status, INTR_ERR1);

	if (f_err) {
		msinfo_hw("[F:%d] Ocurred error interrupt (%d,%d) status(0x%x)\n",
			instance, hw_ip, hw_fcount, hl, vl, status);
		rgbp_hw_dump(hw_ip->regs[REG_SETA]);
	}

	return 0;

}


static int __is_hw_rgbp_s_reset(struct is_hw_ip *hw_ip, struct is_hw_rgbp *hw_rgbp)
{
	int i;

	for (i = 0; i < COREX_MAX; i++)
		hw_ip->cur_hw_iq_set[i].size = 0;

	return rgbp_hw_s_reset(hw_ip->regs[REG_SETA]);
}

static int __is_hw_rgbp_s_common_reg(struct is_hw_ip *hw_ip, u32 instance)
{
	int res = 0;
	struct is_hw_rgbp *hw_rgbp;

	FIMC_BUG(!hw_ip);

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;

	if (hw_ip) {
		msinfo_hw("reset\n", instance, hw_ip);

		res = __is_hw_rgbp_s_reset(hw_ip, hw_rgbp);
		if (res != 0) {
			mserr_hw("sw reset fail", instance, hw_ip);
			return -ENODEV;
		}
		/* TODO : enable corex */
		/* rgbp_hw_s_corex_init(hw_ip->regs[REG_SETA], 0); */
		rgbp_hw_s_init(hw_ip->regs[REG_SETA]);
	}

	msinfo_hw("clear interrupt\n", instance, hw_ip);

	return 0;
}

static int __is_hw_rgbp_clear_common(struct is_hw_ip *hw_ip, u32 instance)
{
	int res = 0;
	struct is_hw_rgbp *hw_rgbp;

	FIMC_BUG(!hw_ip);

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;

	res = __is_hw_rgbp_s_reset(hw_ip, hw_rgbp);
	if (res != 0) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}
	res = rgbp_hw_wait_idle(hw_ip->regs[REG_SETA]);

	if (res)
		mserr_hw("failed to rgbp_hw_wait_idle", instance, hw_ip);

	msinfo_hw("final finished rgbp\n", instance, hw_ip);

	return res;
}

static int is_hw_rgbp_set_cmdq(struct is_hw_ip *hw_ip, u32 instance, u32 set_id)
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

	/*
	 * TODO : enable corex
	ret = rgbp_hw_s_corex_update_type(hw_ip->regs[REG_SETA], set_id, COREX_COPY);
	if (ret)
		return ret;
	*/
	rgbp_hw_s_cmdq(hw_ip->regs[REG_SETA], set_id);

	return ret;
}

static int __nocfi is_hw_rgbp_open(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_rgbp *hw_rgbp = NULL;

	FIMC_BUG(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HWRGBP");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw_ip->priv_info = vzalloc(sizeof(struct is_hw_rgbp));
	if (!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	hw_rgbp->instance = instance;

	ret = CALL_HW_HELPER_OPS(hw_ip, open, instance, &hw_rgbp->lib[instance],
				LIB_FUNC_RGBP);
	if (ret)
		goto err_chain_create;

	ret = CALL_HW_HELPER_OPS(hw_ip, alloc_iqset, rgbp_hw_g_reg_cnt());
	if (ret)
		goto err_iqset_alloc;

	hw_rgbp->rdma_max_cnt = rgbp_hw_g_rdma_max_cnt();
	hw_rgbp->wdma_max_cnt = rgbp_hw_g_wdma_max_cnt();
	hw_rgbp->rdma = vzalloc(sizeof(struct is_common_dma) * hw_rgbp->rdma_max_cnt);
	hw_rgbp->wdma = vzalloc(sizeof(struct is_common_dma) * hw_rgbp->wdma_max_cnt);

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	set_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "open: framemgr[%s]", instance, hw_ip, hw_ip->framemgr->name);

	return 0;

err_iqset_alloc:
	CALL_HW_HELPER_OPS(hw_ip, close, instance, &hw_rgbp->lib[instance]);

err_chain_create:
	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_rgbp_init(struct is_hw_ip *hw_ip, u32 instance,
			bool flag, u32 f_type)
{
	int ret = 0;
	struct is_hw_rgbp *hw_rgbp = NULL;
	u32 idx;
	u32 rdma_max_cnt, wdma_max_cnt;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	if (!hw_rgbp) {
		mserr_hw("hw_rgbp is null ", instance, hw_ip);
		ret = -ENODATA;
		goto err;
	}

	ret = CALL_HW_HELPER_OPS(hw_ip, init, instance, &hw_rgbp->lib[instance],
				(u32)flag, f_type, LIB_FUNC_RGBP);
	if (ret)
		return ret;

	rdma_max_cnt = hw_rgbp->rdma_max_cnt;
	wdma_max_cnt = hw_rgbp->wdma_max_cnt;

	/* set RDMA */
	for (idx = 0; idx < rdma_max_cnt; idx++) {
		ret = rgbp_hw_rdma_create(&hw_rgbp->rdma[idx], hw_ip->regs[REG_SETA], idx);
		if (ret) {
			mserr_hw("rgbp_hw_rdma_create error[%d]", instance, hw_ip, idx);
			ret = -ENODATA;
			goto err;
		}
	}

	/* set WDMA */
	for (idx = 0; idx < wdma_max_cnt; idx++) {
		ret = rgbp_hw_wdma_create(&hw_rgbp->wdma[idx], hw_ip->regs[REG_SETA], idx);
		if (ret) {
			mserr_hw("rgbp_hw_wdma_create error[%d]", instance, hw_ip, idx);
			ret = -ENODATA;
			goto err;
		}
	}

	set_bit(HW_INIT, &hw_ip->state);
	return 0;

err:
	return ret;

}

static int is_hw_rgbp_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_rgbp *hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, deinit, instance, &hw_rgbp->lib[instance]);
}

static int is_hw_rgbp_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_rgbp *hw_rgbp = NULL;
	int ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;

	CALL_HW_HELPER_OPS(hw_ip, close, instance, &hw_rgbp->lib[instance]);

	__is_hw_rgbp_clear_common(hw_ip, instance);

	CALL_HW_HELPER_OPS(hw_ip, free_iqset);

	vfree(hw_rgbp->rdma);
	vfree(hw_rgbp->wdma);

	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	clear_bit(HW_OPEN, &hw_ip->state);

	return ret;
}

static int is_hw_rgbp_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;

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

	__is_hw_rgbp_s_common_reg(hw_ip, instance);

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return ret;
}

static int is_hw_rgbp_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_rgbp *hw_rgbp;
	struct rgbp_param_set *param_set;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("rgbp_disable: Vvalid(%d)\n", instance, hw_ip,
		atomic_read(&hw_ip->status.Vvalid));

	FIMC_BUG(!hw_ip->priv_info);
	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;

	timetowait = wait_event_timeout(hw_ip->status.wait_queue,
		!atomic_read(&hw_ip->status.Vvalid),
		IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		mserr_hw("wait FRAME_END timeout (%ld)", instance,
			hw_ip, timetowait);
		ret = -ETIME;
	}

	param_set = &hw_rgbp->param_set[instance];
	param_set->fcount = 0;

	CALL_HW_HELPER_OPS(hw_ip, disable, instance, &hw_rgbp->lib[instance]);

	if (hw_ip->run_rsc_state)
		return 0;

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static int __is_hw_rgbp_set_rdma(struct is_hw_ip *hw_ip, struct is_hw_rgbp *hw_rgbp, struct rgbp_param_set *param_set,
										u32 instance, u32 id, u32 set_id)
{
	u32 *input_dva = NULL;
	u32 cmd;
	u32 comp_sbwc_en, payload_size;
	u32 in_crop_size_x = 0;
	u32 cache_hint = 0x7;
	int ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_rgbp);
	FIMC_BUG(!param_set);

	input_dva = rgbp_hw_g_input_dva(param_set, instance, id, &cmd);

	msdbg_hw(2, "%s: %d\n", instance, hw_ip, hw_rgbp->rdma[id].name, cmd);

	rgbp_hw_s_rdma_corex_id(&hw_rgbp->rdma[id], set_id);

	ret = rgbp_hw_s_rdma_init(hw_ip, &hw_rgbp->rdma[id], param_set, cmd,
		in_crop_size_x, cache_hint,
		&comp_sbwc_en, &payload_size);
	if (ret) {
		mserr_hw("failed to initialize RGBP_DMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (cmd == DMA_INPUT_COMMAND_ENABLE) {
		ret = rgbp_hw_s_rdma_addr(&hw_rgbp->rdma[id], input_dva, 0, (hw_ip->num_buffers & 0xffff),
			0, comp_sbwc_en, payload_size);
		if (ret) {
			mserr_hw("failed to set RGBP_RDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
	}

	return 0;
}

static int __is_hw_rgbp_set_wdma(struct is_hw_ip *hw_ip, struct is_hw_rgbp *hw_rgbp, struct rgbp_param_set *param_set,
							u32 instance, u32 id, u32 set_id, struct is_frame *dma_frame)
{
	u32 *output_dva = NULL;
	u32 cmd;
	u32 comp_sbwc_en, payload_size;
	u32 out_crop_size_x = 0;
	u32 cache_hint = IS_LLC_CACHE_HINT_VOTF_TYPE;
	int ret = 0;
	struct is_param_region *param;
	bool sbwc_en;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_rgbp);
	FIMC_BUG(!param_set);

	param = &hw_ip->region[instance]->parameter;

	output_dva = rgbp_hw_g_output_dva(param_set, instance, id, &cmd);

	msdbg_hw(2, "%s: %d\n", instance, hw_ip, hw_rgbp->wdma[id].name, cmd);

	rgbp_hw_s_wdma_corex_id(&hw_rgbp->wdma[id], set_id);

	ret = rgbp_hw_s_wdma_init(hw_ip, &hw_rgbp->wdma[id], param_set, instance, cmd,
		out_crop_size_x, cache_hint,
		&comp_sbwc_en, &payload_size);
	if (ret) {
		mserr_hw("failed to initialize RGBP_DMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (cmd == DMA_OUTPUT_COMMAND_ENABLE) {
		ret = rgbp_hw_s_wdma_addr(&hw_rgbp->wdma[id], output_dva, 0, (hw_ip->num_buffers & 0xffff),
			0, comp_sbwc_en, payload_size);
		if (ret) {
			mserr_hw("failed to set RGBP_WDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
		sbwc_en = comp_sbwc_en ? true : false;
		rgbp_hw_s_sbwc(hw_ip->regs[REG_SETA], set_id, sbwc_en);
	}

	return 0;
}


static void __is_hw_rgbp_check_size(struct is_hw_ip *hw_ip, struct rgbp_param_set *param_set, u32 instance)
{
	struct param_dma_output *output_dma = &param_set->dma_output_yuv;
	struct param_otf_input *input = &param_set->otf_input;
	struct param_otf_output *output = &param_set->otf_output;

	FIMC_BUG_VOID(!param_set);

	msdbgs_hw(2, "hw_rgbp_check_size >>>\n", instance, hw_ip);
	msdbgs_hw(2, "otf_input: pos(%d,%d),crop%dx%d),size(%dx%d)\n", instance, hw_ip,
		input->bayer_crop_offset_x, input->bayer_crop_offset_y,
		input->bayer_crop_width, input->bayer_crop_height,
		input->width, input->height);
	msdbgs_hw(2, "otf_output: pos(%d,%d),crop%dx%d),size(%dx%d)\n", instance, hw_ip,
		output->crop_offset_x, output->crop_offset_y,
		output->crop_width, output->crop_height,
		output->width, output->height);
	msdbgs_hw(2, "dma_output: format(%d),crop_size(%dx%d)\n", instance, hw_ip,
		output_dma->format, output_dma->dma_crop_width, output_dma->dma_crop_height);
	msdbgs_hw(2, "<<< hw_rgbp_check_size <<<\n", instance, hw_ip);
}

static int __is_hw_rgbp_bypass(struct is_hw_ip *hw_ip, u32 set_id)
{
	FIMC_BUG(!hw_ip);

	rgbp_hw_s_block_bypass(hw_ip->regs[REG_SETA], set_id);

	return 0;
}


static int __is_hw_rgbp_update_block_reg(struct is_hw_ip *hw_ip, struct rgbp_param_set *param_set,
									u32 instance, u32 set_id)
{
	struct is_hw_rgbp *hw_rgbp;
	int ret = 0;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;

	if (hw_rgbp->instance != instance) {
		msdbg_hw(2, "update_param: hw_ip->instance(%d)\n",
			instance, hw_ip, hw_rgbp->instance);
		hw_rgbp->instance = instance;
	}

	__is_hw_rgbp_check_size(hw_ip, param_set, instance);
	ret = __is_hw_rgbp_bypass(hw_ip, set_id);

	return ret;
}

static void __is_hw_rgbp_update_param(struct is_hw_ip *hw_ip, struct is_param_region *p_region,
					struct rgbp_param_set *param_set, IS_DECLARE_PMAP(pmap),
					u32 instance)
{
	struct rgbp_param *param;

	FIMC_BUG_VOID(!hw_ip);
	FIMC_BUG_VOID(!param_set);

	param = &p_region->rgbp;
	param_set->instance_id = instance;
	param_set->byrp_param = &p_region->byrp;

	/* check input */
	if (test_bit(PARAM_RGBP_OTF_INPUT, pmap))
		memcpy(&param_set->otf_input, &param->otf_input,
				sizeof(struct param_otf_input));

	/* check output */
	if (test_bit(PARAM_RGBP_OTF_OUTPUT, pmap))
		memcpy(&param_set->otf_output, &param->otf_output,
				sizeof(struct param_otf_output));

	if (test_bit(PARAM_RGBP_YUV, pmap))
		memcpy(&param_set->dma_output_yuv, &param->yuv,
				sizeof(struct param_dma_output));

	if (test_bit(PARAM_RGBP_HF, pmap))
		memcpy(&param_set->dma_output_hf, &param->hf,
				sizeof(struct param_dma_output));

	if (test_bit(PARAM_RGBP_SF, pmap))
		memcpy(&param_set->dma_output_sf, &param->sf,
				sizeof(struct param_dma_output));

	if (test_bit(PARAM_RGBP_STRIPE_INPUT, pmap))
		memcpy(&param_set->stripe_input, &param->stripe_input,
				sizeof(struct param_stripe_input));

#if IS_ENABLED(CONFIG_RGBP_V1_1)
	if (test_bit(PARAM_RGBP_DMA_INPUT, pmap))
		memcpy(&param_set->dma_input, &param->dma_input,
				sizeof(struct param_dma_input));

	if (test_bit(PARAM_RGBP_RGB, pmap))
		memcpy(&param_set->dma_output_rgb, &param->rgb,
				sizeof(struct param_dma_output));
#endif

}

static int is_hw_rgbp_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
		IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	struct is_hw_rgbp *hw_rgbp;

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

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	hw_rgbp->instance = IS_STREAM_COUNT;

	return 0;
}

static void is_hw_rgbp_s_size_regs(struct is_hw_ip *hw_ip, struct rgbp_param_set *param_set,
	u32 instance, const struct is_frame *frame)
{
	struct is_hw_rgbp *hw;
	void __iomem *base;
	struct rgbp_grid_cfg grid_cfg;
	struct is_rgbp_config *cfg;
	u32 strip_enable, start_pos_x, start_pos_y;
	u32 sensor_full_width, sensor_full_height;
	u32 sensor_binning_x, sensor_binning_y;
	u32 sensor_crop_x, sensor_crop_y;
	u32 bns_binning_x, bns_binning_y;

	hw = (struct is_hw_rgbp *)hw_ip->priv_info;
	base = hw_ip->regs[REG_SETA];
	cfg = &hw->config;

	/* GRID configuration for LSC */
	strip_enable = (param_set->stripe_input.total_count == 0) ? 0 : 1;

	sensor_full_width = frame->shot->udm.frame_info.sensor_size[0];
	sensor_full_height = frame->shot->udm.frame_info.sensor_size[1];
	sensor_crop_x = frame->shot->udm.frame_info.sensor_crop_region[0];
	sensor_crop_y = frame->shot->udm.frame_info.sensor_crop_region[1];
	sensor_binning_x = frame->shot->udm.frame_info.sensor_binning[0];
	sensor_binning_y = frame->shot->udm.frame_info.sensor_binning[1];
	bns_binning_x = frame->shot->udm.frame_info.bns_binning[0];
	bns_binning_y = frame->shot->udm.frame_info.bns_binning[1];

	/* Center */
	if (cfg->sensor_center_x == 0 || cfg->sensor_center_y == 0) {
		cfg->sensor_center_x = sensor_full_width >> 1;
		cfg->sensor_center_y = sensor_full_height >> 1;
		msdbg_hw(2, "%s: cal_center(0,0) from DDK. Fix to (%d,%d)",
				instance, hw_ip, __func__,
				cfg->sensor_center_x, cfg->sensor_center_y);
	}

	/* Total_binning = sensor_binning * csis_bns_binning */
	grid_cfg.binning_x = (1024ULL * sensor_binning_x * bns_binning_x) / 1000 / 1000;
	grid_cfg.binning_y = (1024ULL * sensor_binning_y * bns_binning_y) / 1000 / 1000;
	if (grid_cfg.binning_x == 0 || grid_cfg.binning_y == 0) {
		grid_cfg.binning_x = 1024;
		grid_cfg.binning_y = 1024;
		msdbg_hw(2, "%s:[LSC] calculated binning(0,0). Fix to (%d,%d)",
				instance, hw_ip, __func__, 1024, 1024);
	}

	/* Step */
	grid_cfg.step_x = cfg->lsc_step_x;
	grid_cfg.step_y = cfg->lsc_step_y;

	/* Total_crop = unbinned(Stripe_start_position + BYRP_bcrop) + unbinned(sensor_crop) */
	sensor_crop_x = frame->shot->udm.frame_info.sensor_crop_region[0];
	sensor_crop_y = frame->shot->udm.frame_info.sensor_crop_region[1];
	start_pos_x = (strip_enable) ? param_set->stripe_input.start_pos_x : 0;
	start_pos_x = (((start_pos_x + param_set->byrp_param->dma_input.bayer_crop_offset_x) *
			grid_cfg.binning_x) / 1024) +
			(sensor_crop_x * sensor_binning_x) / 1000;
	start_pos_y = ((param_set->byrp_param->dma_input.bayer_crop_offset_y * grid_cfg.binning_y) / 1024) +
			(sensor_crop_y * sensor_binning_y) / 1000;
	grid_cfg.crop_x = start_pos_x * grid_cfg.step_x;
	grid_cfg.crop_y = start_pos_y * grid_cfg.step_y;
	grid_cfg.crop_radial_x = (u16)((-1) * (cfg->sensor_center_x - start_pos_x));
	grid_cfg.crop_radial_y = (u16)((-1) * (cfg->sensor_center_y - start_pos_y));

	rgbp_hw_s_grid_cfg(base, &grid_cfg);

	msdbg_hw(2, "%s:[LSC] stripe: enable(%d), pos_x(%d), full_size(%dx%d)\n",
			instance, hw_ip, __func__,
			strip_enable, param_set->stripe_input.start_pos_x,
			param_set->stripe_input.full_width,
			param_set->stripe_input.full_height);

	msdbg_hw(2, "%s:[LSC] dbg: calibrated_size(%dx%d), cal_center(%d,%d), sensor_crop(%d,%d), start_pos(%d,%d)\n",
			instance, hw_ip, __func__,
			sensor_full_width, sensor_full_height,
			cfg->sensor_center_x,
			cfg->sensor_center_y,
			sensor_crop_x, sensor_crop_y,
			start_pos_x, start_pos_y);

	msdbg_hw(2, "%s:[LSC] sfr: binning(%dx%d), step(%dx%d), crop(%d,%d), crop_radial(%d,%d)\n",
			instance, hw_ip, __func__,
			grid_cfg.binning_x,
			grid_cfg.binning_y,
			grid_cfg.step_x,
			grid_cfg.step_y,
			grid_cfg.crop_x,
			grid_cfg.crop_y,
			grid_cfg.crop_radial_x,
			grid_cfg.crop_radial_y);
}

static int __is_hw_rgbp_set_size_regs(struct is_hw_ip *hw_ip, struct rgbp_param_set *param_set,
	u32 instance, u32 rgbp_strip_start_pos, const struct is_frame *frame, u32 set_id)
{
	struct is_hw_rgbp *hw_rgbp;
	struct is_rgbp_config *config;
	struct is_hw_rgbp_sc_size yuvsc_size, upsc_size;
	struct rgbp_radial_cfg radial_cfg;
	int ret = 0;
	u32 strip_enable;
	u32 stripe_index;
	u32 strip_start_pos_x;
	u32 in_width, in_height;
	u32 out_width, out_height;
	u32 vratio, hratio;
	u32 yuvsc_enable, yuvsc_bypass;
	u32 upsc_enable, upsc_bypass;
	u32 decomp_enable, decomp_bypass;
	u32 gamma_enable, gamma_bypass;
	u32 y_ds_upsc_us;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!param_set);

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	config = &hw_rgbp->config;

	in_width = param_set->otf_input.width;
	in_height = param_set->otf_input.height;
	if (param_set->otf_output.cmd) {
		out_width = param_set->otf_output.width;
		out_height = param_set->otf_output.height;
	} else if (param_set->dma_output_yuv.cmd) {
		out_width = param_set->dma_output_yuv.width;
		out_height = param_set->dma_output_yuv.height;
	} else {
		out_width = in_width;
		out_height = in_height;
	}

	rgbp_hw_s_chain_src_size(hw_ip->regs[REG_SETA], set_id, in_width, in_height);
	rgbp_hw_s_chain_dst_size(hw_ip->regs[REG_SETA], set_id, out_width, out_height);

	rgbp_hw_s_dtp_output_size(hw_ip->regs[REG_SETA], set_id, in_width, in_height);

	strip_enable = (param_set->stripe_input.total_count < 2) ? 0 : 1;
	stripe_index = param_set->stripe_input.index;
	strip_start_pos_x = (stripe_index) ?
		(param_set->stripe_input.start_pos_x - param_set->stripe_input.left_margin) : 0;

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

	rgbp_hw_s_dns_size(hw_ip->regs[REG_SETA], set_id, in_width, in_height,
			strip_enable, strip_start_pos_x, &radial_cfg, config);

	yuvsc_size.input_h_size = in_width;
	yuvsc_size.input_v_size = in_height;
	yuvsc_size.dst_h_size = out_width;
	yuvsc_size.dst_v_size = out_height;

	yuvsc_enable = 0x1;
	yuvsc_bypass = 0x1;
	if ((in_width != out_width) || (in_height != out_height)) {
		msdbgs_hw(2, "[F:%d][%s] YUVSC enable incrop(%d, %d), otcrop(%d, %d)\n",
					instance, hw_ip, frame->fcount, __func__,
					in_width, in_height, out_width, out_height);
		yuvsc_bypass = 0x0;
	}

	rgbp_hw_s_cout_fifo(hw_ip->regs[REG_SETA], set_id, param_set->otf_output.cmd);

	rgbp_hw_s_yuvsc_enable(hw_ip->regs[REG_SETA], set_id, yuvsc_enable, yuvsc_bypass);

	rgbp_hw_s_yuvsc_input_size(hw_ip->regs[REG_SETA], set_id, yuvsc_size.input_h_size, yuvsc_size.input_v_size);
	rgbp_hw_s_yuvsc_src_size(hw_ip->regs[REG_SETA], set_id, yuvsc_size.input_h_size, yuvsc_size.input_v_size);
	rgbp_hw_s_yuvsc_dst_size(hw_ip->regs[REG_SETA], set_id, yuvsc_size.dst_h_size, yuvsc_size.dst_v_size);
	rgbp_hw_s_yuvsc_out_crop_size(hw_ip->regs[REG_SETA], set_id, yuvsc_size.dst_h_size, yuvsc_size.dst_v_size);

	hratio = ((u64)yuvsc_size.input_h_size << 20) / yuvsc_size.dst_h_size;
	vratio = ((u64)yuvsc_size.input_v_size << 20) / yuvsc_size.dst_v_size;

	rgbp_hw_s_yuvsc_scaling_ratio(hw_ip->regs[REG_SETA], set_id, hratio, vratio);

	/* set_crop */
	rgbp_hw_s_crop(hw_ip->regs[REG_SETA], in_width, in_height, out_width, out_height);

	upsc_enable = 0x0;
	upsc_bypass = 0x0;
	decomp_enable = 0x0;
	decomp_bypass = 0x0;
	y_ds_upsc_us = 0x0;
	gamma_enable = 0x0;
	gamma_bypass = 0x1;

	/* for HF */
	if (param_set->dma_output_hf.cmd == DMA_OUTPUT_VOTF_ENABLE) {
		if ((param_set->otf_input.width == param_set->otf_output.width) &&
			(param_set->otf_input.height == param_set->otf_output.height)) {
			mserr_hw("not support HF in the same otf in/output size\n", instance, hw_ip);
			goto hf_disable;
		}

		/* set UPSC */
		upsc_size.input_h_size = yuvsc_size.dst_h_size;
		upsc_size.input_v_size = yuvsc_size.dst_v_size;
		upsc_size.dst_h_size = yuvsc_size.input_h_size;
		upsc_size.dst_v_size = yuvsc_size.input_v_size;

		upsc_enable = 0x1;
		rgbp_hw_s_upsc_input_size(hw_ip->regs[REG_SETA], set_id, upsc_size.input_h_size,
									upsc_size.input_v_size);
		rgbp_hw_s_upsc_src_size(hw_ip->regs[REG_SETA], set_id, upsc_size.input_h_size, upsc_size.input_v_size);
		rgbp_hw_s_upsc_dst_size(hw_ip->regs[REG_SETA], set_id, upsc_size.dst_h_size, upsc_size.dst_v_size);
		rgbp_hw_s_upsc_out_crop_size(hw_ip->regs[REG_SETA], set_id, upsc_size.dst_h_size,
									upsc_size.dst_v_size);

		hratio = (upsc_size.input_h_size << 20) / upsc_size.dst_h_size;
		vratio = (upsc_size.input_v_size << 20) / upsc_size.dst_v_size;

		rgbp_hw_s_upsc_scaling_ratio(hw_ip->regs[REG_SETA], set_id, hratio, vratio);
		rgbp_hw_s_upsc_coef(hw_ip->regs[REG_SETA], set_id, hratio, vratio);

		gamma_enable = 0x1;
		gamma_bypass = 0x0;

		/* set Decomp */
		decomp_enable = 0x1;
		rgbp_hw_s_decomp_size(hw_ip->regs[REG_SETA], set_id, upsc_size.dst_h_size, upsc_size.dst_v_size);

		y_ds_upsc_us = 1;

		rgbp_hw_s_votf(hw_ip->regs[REG_SETA], set_id, 0x1, 0x1);
	} else
		rgbp_hw_s_votf(hw_ip->regs[REG_SETA], set_id, 0x0, 0x1);

hf_disable:
	rgbp_hw_s_upsc_enable(hw_ip->regs[REG_SETA], set_id, upsc_enable, upsc_bypass);
	rgbp_hw_s_decomp_enable(hw_ip->regs[REG_SETA], set_id, decomp_enable, decomp_bypass);
	rgbp_hw_s_ds_y_upsc_us(hw_ip->regs[REG_SETA], set_id, y_ds_upsc_us);

	if (!IS_ENABLED(RGBP_DDK_LIB_CALL))
		gamma_bypass = 0x1;

	/* set gamma */
	rgbp_hw_s_gamma_enable(hw_ip->regs[REG_SETA], set_id, gamma_enable, gamma_bypass);

	return ret;
}

static int is_hw_rgbp_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	struct is_hw_rgbp *hw_rgbp = NULL;
	struct is_param_region *param_region;
	struct rgbp_param_set *param_set = NULL;
	struct is_frame *dma_frame;
	u32 fcount, instance;
	u32 strip_start_pos = 0;
	u32 cur_idx;
	u32 set_id;
	int ret, i, batch_num;
	u32 cmd_input;
	u32 cmd_output_hf, cmd_output_sf, cmd_output_yuv, cmd_output_rgb;
	u32 strip_enable;
	u32 stripe_index, strip_num;
	u32 rdma_max_cnt, wdma_max_cnt;
	bool do_blk_cfg = true;
	bool skip = false;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

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

	FIMC_BUG(!hw_ip->priv_info);
	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	param_set = &hw_rgbp->param_set[instance];
	param_region = frame->parameter;

	atomic_set(&hw_rgbp->strip_index, frame->stripe_info.region_id);
	fcount = frame->fcount;
	dma_frame = frame;
	rdma_max_cnt = hw_rgbp->rdma_max_cnt;
	wdma_max_cnt = hw_rgbp->wdma_max_cnt;

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

	__is_hw_rgbp_update_param(hw_ip, param_region, param_set, frame->pmap, instance);

	/* DMA settings */
	cur_idx = frame->cur_buf_index;

	cmd_input = CALL_HW_OPS(hw_ip, dma_cfg, "rgbp", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_input.cmd,
			param_set->dma_input.plane,
			param_set->input_dva,
			dma_frame->dvaddr_buffer);

	cmd_output_hf = CALL_HW_OPS(hw_ip, dma_cfg, "rgbphf", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_output_hf.cmd,
			param_set->dma_output_hf.plane,
			param_set->output_dva_hf,
			dma_frame->dva_rgbp_hf);

	cmd_output_sf = CALL_HW_OPS(hw_ip, dma_cfg, "rgbpsf", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_output_sf.cmd,
			param_set->dma_output_sf.plane,
			param_set->output_dva_sf,
			dma_frame->dva_rgbp_sf);

	cmd_output_yuv = CALL_HW_OPS(hw_ip, dma_cfg, "rgbpyuv", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_output_yuv.cmd,
			param_set->dma_output_yuv.plane,
			param_set->output_dva_yuv,
			dma_frame->dva_rgbp_yuv);

	cmd_output_rgb = CALL_HW_OPS(hw_ip, dma_cfg, "rgbprgb", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_output_rgb.cmd,
			param_set->dma_output_rgb.plane,
			param_set->output_dva_rgb,
			dma_frame->dva_rgbp_rgb);

	msdbg_hw(2, "[F:%d][%s] hf:%d, sf:%d, yuv:%d, rgb:%d\n", instance, hw_ip, dma_frame->fcount, __func__,
							param_set->dma_output_hf.cmd,
							param_set->dma_output_sf.cmd,
							param_set->dma_output_yuv.cmd,
							param_set->dma_output_rgb.cmd);

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

	strip_num = param_set->stripe_input.total_count;
	strip_enable = (strip_num < 2) ? 0 : 1;
	stripe_index = param_set->stripe_input.index;
	strip_start_pos = (stripe_index) ?
		(param_set->stripe_input.start_pos_x - param_set->stripe_input.left_margin) : 0;

	if (IS_ENABLED(SKIP_ISP_SHOT_FOR_MULTI_SHOT)) {
		if (!pablo_is_first_shot(batch_num, cur_idx)) skip = true;
		if (!pablo_is_first_shot(strip_num, stripe_index)) skip = true;
		if (!pablo_is_first_shot(param_set->stripe_input.repeat_num,
			param_set->stripe_input.repeat_idx)) skip = true;
	}

	set_id = COREX_DIRECT;

	rgbp_hw_s_core(hw_ip->regs[REG_SETA], frame->num_buffers, set_id,
			&hw_rgbp->config, param_set);

	ret = CALL_HW_HELPER_OPS(hw_ip, lib_shot, instance, skip,
				frame, &hw_rgbp->lib[instance], param_set);
	if (ret)
		return ret;

	do_blk_cfg = CALL_HW_HELPER_OPS(hw_ip, set_rta_regs, instance, set_id, skip, frame, NULL);
	if (unlikely(do_blk_cfg))
		__is_hw_rgbp_update_block_reg(hw_ip, param_set, instance, set_id);

	is_hw_rgbp_s_size_regs(hw_ip, param_set, instance, frame);

	ret = __is_hw_rgbp_set_size_regs(hw_ip, param_set, instance,
			strip_start_pos, frame, set_id);
	if (ret) {
		mserr_hw("__is_hw_rgbp_set_size_regs is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	for (i = 0; i < rdma_max_cnt; i++) {
		ret = __is_hw_rgbp_set_rdma(hw_ip, hw_rgbp, param_set, instance,
				i, set_id);
		if (ret) {
			mserr_hw("__is_hw_rgbp_set_rdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	for (i = 0; i < wdma_max_cnt; i++) {
		ret = __is_hw_rgbp_set_wdma(hw_ip, hw_rgbp, param_set, instance,
				i, set_id, dma_frame);
		if (ret) {
			mserr_hw("__is_hw_rgbp_set_wdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, fcount, DEBUG_POINT_ADD_TO_CMDQ);
	ret = is_hw_rgbp_set_cmdq(hw_ip, instance, set_id);
	if (ret) {
		mserr_hw("is_hw_rgbp_set_cmdq is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	if (unlikely(debug_rgbp))
		rgbp_hw_dump(hw_ip->regs[REG_SETA]);

	set_bit(HW_CONFIG, &hw_ip->state);

	return 0;

shot_fail_recovery:
	CALL_HW_HELPER_OPS(hw_ip, restore, instance, &hw_rgbp->lib[instance]);

	return ret;
}

static int is_hw_rgbp_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	struct is_hw_rgbp *hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, get_meta, frame->instance,
				frame, &hw_rgbp->lib[frame->instance]);
}

static int is_hw_rgbp_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
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


static int is_hw_rgbp_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_rgbp *hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, load_setfile, instance, &hw_rgbp->lib[instance]);
}


static int is_hw_rgbp_apply_setfile(struct is_hw_ip *hw_ip, u32 scenario,
	u32 instance, ulong hw_map)
{
	struct is_hw_rgbp *hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, apply_setfile, instance, &hw_rgbp->lib[instance],
				scenario);
}

static int is_hw_rgbp_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_rgbp *hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, delete_setfile, instance, &hw_rgbp->lib[instance]);
}

int is_hw_rgbp_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_rgbp *hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, restore, instance, &hw_rgbp->lib[instance]);
}

static int is_hw_rgbp_set_regs(struct is_hw_ip *hw_ip, u32 chain_id,
	u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size)
{
	return CALL_HW_HELPER_OPS(hw_ip, set_regs, instance, regs, regs_size);
}

int is_hw_rgbp_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size, enum is_reg_dump_type dump_type)
{
	void __iomem *base;
	struct is_common_dma *dma;
	struct is_hw_rgbp *hw_rgbp = NULL;
	u32 i, rdma_max_cnt, wdma_max_cnt;

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	rdma_max_cnt = hw_rgbp->rdma_max_cnt;
	wdma_max_cnt = hw_rgbp->wdma_max_cnt;

	switch (dump_type) {
	case IS_REG_DUMP_TO_LOG:
		base = hw_ip->regs[REG_SETA];
		rgbp_hw_dump(base);
		break;
	case IS_REG_DUMP_DMA:
		for (i = 0; i < rdma_max_cnt; i++) {
			dma = &hw_rgbp->rdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}

		for (i = 0; i < wdma_max_cnt; i++) {
			dma = &hw_rgbp->wdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int is_hw_rgbp_set_config(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount, void *conf)
{
	struct is_hw_rgbp *hw_rgbp = NULL;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!conf);

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	hw_rgbp->config = *(struct is_rgbp_config *)conf;

	return 0;
}

static int is_hw_rgbp_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_rgbp *hw_rgbp;

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	if (!hw_rgbp) {
		mserr_hw("failed to get HW RGBP", instance, hw_ip);
		return -ENODEV;
	}
	/* DEBUG */
	rgbp_hw_dump(hw_ip->regs[REG_SETA]);

	return CALL_HW_HELPER_OPS(hw_ip, notify_timeout, instance, &hw_rgbp->lib[instance]);
}

const struct is_hw_ip_ops is_hw_rgbp_ops = {
	.open			= is_hw_rgbp_open,
	.init			= is_hw_rgbp_init,
	.deinit			= is_hw_rgbp_deinit,
	.close			= is_hw_rgbp_close,
	.enable			= is_hw_rgbp_enable,
	.disable		= is_hw_rgbp_disable,
	.shot			= is_hw_rgbp_shot,
	.set_param		= is_hw_rgbp_set_param,
	.get_meta		= is_hw_rgbp_get_meta,
	.frame_ndone		= is_hw_rgbp_frame_ndone,
	.load_setfile		= is_hw_rgbp_load_setfile,
	.apply_setfile		= is_hw_rgbp_apply_setfile,
	.delete_setfile		= is_hw_rgbp_delete_setfile,
	.restore		= is_hw_rgbp_restore,
	.set_regs		= is_hw_rgbp_set_regs,
	.dump_regs		= is_hw_rgbp_dump_regs,
	.set_config		= is_hw_rgbp_set_config,
	.notify_timeout		= is_hw_rgbp_notify_timeout,
};

int is_hw_rgbp_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int hw_slot;

	hw_ip->ops  = &is_hw_rgbp_ops;

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &is_hw_rgbp_handle_interrupt0;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].handler = &is_hw_rgbp_handle_interrupt1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].id = INTR_HWIP2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = true;

	return 0;
}
