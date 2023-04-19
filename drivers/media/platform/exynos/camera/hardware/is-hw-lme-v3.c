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

#include "is-hw-lme-v3.h"
#include "is-err.h"
#include "api/is-hw-api-lme-v3_0.h"

static ulong debug_lme;
module_param(debug_lme, ulong, 0644);

static int is_hw_lme_handle_interrupt(u32 id, void *context)
{
	struct is_hardware *hardware = NULL;
	struct is_hw_ip *hw_ip = NULL;
	struct is_hw_lme *hw_lme = NULL;
	u32 status, instance, hw_fcount, strip_index, set_id, hl = 0, vl = 0;
	int f_err;

	hw_ip = (struct is_hw_ip *)context;

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	strip_index = atomic_read(&hw_lme->strip_index);
	set_id = hw_lme->corex_set_id;

	status = lme_hw_g_int_state(hw_ip->regs[REG_SETA], true,
			(hw_ip->num_buffers & 0xffff),
			&hw_lme->irq_state[LME_INTR], set_id)
			& lme_hw_g_int_mask(hw_ip->regs[REG_SETA], set_id);

	msdbg_hw(2, "LME interrupt status(0x%x)\n", instance, hw_ip, status);

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

	if (lme_hw_is_occurred(status, INTR_FRAME_START) &&
			lme_hw_is_occurred(status, INTR_FRAME_END))
		mswarn_hw("start/end overlapped!! (0x%x)", instance, hw_ip, status);

	if (lme_hw_is_occurred(status, INTR_FRAME_START)) {
		atomic_add(1, &hw_ip->count.fs);
		CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount,
				DEBUG_POINT_FRAME_START);
		if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
			msinfo_hw("[F:%d]F.S\n", instance, hw_ip, hw_fcount);

		CALL_HW_OPS(hw_ip, frame_start, hw_ip, instance);

	}

	if (lme_hw_is_occurred(status, INTR_FRAME_END)) {
		lme_hw_s_clock(hw_ip->regs[REG_SETA], false);

		CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount,
				DEBUG_POINT_FRAME_END);
		atomic_add(hw_ip->num_buffers, &hw_ip->count.fe);

		CALL_HW_OPS(hw_ip, frame_done, hw_ip, NULL, -1, IS_HW_CORE_END,
				IS_SHOT_SUCCESS, true);

		if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
			msinfo_hw("[F:%d]F.E\n", instance, hw_ip, hw_fcount);

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

	f_err = lme_hw_is_occurred(status, INTR_ERR);

	if (f_err) {
		msinfo_hw("[F:%d] Ocurred error interrupt (%d,%d) status(0x%x)\n",
			instance, hw_ip, hw_fcount, hl, vl, status);
		lme_hw_dump(hw_ip->regs[REG_SETA], COREX_DIRECT);
	}

	return 0;
}

static int __is_hw_lme_s_reset(struct is_hw_ip *hw_ip, struct is_hw_lme *hw_lme)
{
	return lme_hw_s_reset(hw_ip->regs[REG_SETA]);
}

static int __is_hw_lme_s_common_reg(struct is_hw_ip *hw_ip, u32 instance)
{
	int res = 0;
	struct is_hw_lme *hw_lme;
	u32 seed;

	FIMC_BUG(!hw_ip);

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

	if (hw_ip) {
		msinfo_hw("reset\n", instance, hw_ip);

		res = __is_hw_lme_s_reset(hw_ip, hw_lme);
		if (res != 0) {
			mserr_hw("sw reset fail", instance, hw_ip);
			return -ENODEV;
		}

		lme_hw_s_corex_init(hw_ip->regs[REG_SETA], true);

		seed = is_get_debug_param(IS_DEBUG_PARAM_CRC_SEED);
		if (unlikely(seed))
			lme_hw_s_crc(hw_ip->regs[REG_SETA], seed);
	}

	msinfo_hw("clear interrupt\n", instance, hw_ip);

	return 0;
}

static int __is_hw_lme_clear_common(struct is_hw_ip *hw_ip, u32 instance)
{
	int res;
	struct is_hw_lme *hw_lme;

	FIMC_BUG(!hw_ip);

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

	res = __is_hw_lme_s_reset(hw_ip, hw_lme);
	if (res != 0) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}
	res = lme_hw_wait_idle(hw_ip->regs[REG_SETA], hw_lme->corex_set_id);

	if (res)
		mserr_hw("failed to lme_hw_wait_idle", instance, hw_ip);

	msinfo_hw("final finished lme\n", instance, hw_ip);

	return res;
}

static int is_hw_lme_set_cmdq(struct is_hw_ip *hw_ip, u32 instance, u32 set_id)
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
	 * ret = lme_hw_s_corex_update_type(hw_ip->regs[REG_SETA], set_id, COREX_COPY);
	 * if (ret)
	 * return ret;
	 */
	lme_hw_s_cmdq(hw_ip->regs[REG_SETA], set_id);

	return ret;
}

static int __nocfi is_hw_lme_open(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;

	FIMC_BUG(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HWLME");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw_ip->priv_info = vzalloc(sizeof(struct is_hw_lme));
	if (!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	set_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "open: framemgr[%s]", instance, hw_ip, hw_ip->framemgr->name);

	return 0;

err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static void is_hw_lme_set_i_subdev_config(struct pablo_internal_subdev *sd)
{
	sd->width = LME_IMAGE_MAX_WIDTH / 16;
	sd->height = LME_IMAGE_MAX_HEIGHT / 16;
	sd->num_planes = 1;
	sd->num_batch = 1;
	sd->num_buffers = LME_TNR_MODE_MIN_BUFFER_NUM;
	sd->bits_per_pixel = DMA_INOUT_BIT_WIDTH_16BIT;
	sd->memory_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
	sd->size[0] = sd->height * ALIGN(DIV_ROUND_UP(sd->width * sd->memory_bitwidth, BITS_PER_BYTE), 32);
}

static int is_hw_lme_init(struct is_hw_ip *hw_ip, u32 instance, bool flag, u32 f_type)
{
	struct is_hw_lme *hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	struct pablo_internal_subdev *sd;
	struct is_mem *mem;
	int ret;
	int buf_id;
	char name[IS_STR_LEN];

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	hw_lme->param_set[instance].reprocessing = flag;

	mem = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_iommu_mem, GROUP_ID_LME0);

	for (buf_id = 0; buf_id < LME_INTERNAL_BUF_ID_MAX; buf_id++) {
		sd = &hw_lme->subdev[instance][buf_id];
		snprintf(name, sizeof(name), "LME_MBMV%d", buf_id);

		pablo_internal_subdev_probe(sd, instance, ENTRY_INTERNAL, mem, name);

		is_hw_lme_set_i_subdev_config(sd);

		ret = CALL_I_SUBDEV_OPS(sd, alloc, sd);
		if (ret) {
			mserr_hw("[%s] failed to alloc(%d)", instance, hw_ip, sd->name, ret);
			return ret;
		}
	}

	set_bit(HW_INIT, &hw_ip->state);

	return 0;
}

static int is_hw_lme_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_lme *hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	struct pablo_internal_subdev *sd;
	int buf_id;

	for (buf_id = 0; buf_id < LME_INTERNAL_BUF_ID_MAX; buf_id++) {
		sd = &hw_lme->subdev[instance][buf_id];

		if (CALL_I_SUBDEV_OPS(sd, free, sd))
			mserr_hw("[%s] failed to free", instance, hw_ip, sd->name);
	}

	return 0;
}

static int is_hw_lme_close(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	__is_hw_lme_clear_common(hw_ip, instance);

	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	clear_bit(HW_OPEN, &hw_ip->state);

	return ret;
}

static int is_hw_lme_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
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

	__is_hw_lme_s_common_reg(hw_ip, instance);

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return ret;
}

static int is_hw_lme_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_lme *hw_lme;
	struct lme_param_set *param_set;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("lme_disable: Vvalid(%d)\n", instance, hw_ip,
		atomic_read(&hw_ip->status.Vvalid));

	FIMC_BUG(!hw_ip->priv_info);
	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

	timetowait = wait_event_timeout(hw_ip->status.wait_queue,
		!atomic_read(&hw_ip->status.Vvalid),
		IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		mserr_hw("wait FRAME_END timeout (%ld)", instance,
			hw_ip, timetowait);
		ret = -ETIME;
	}

	param_set = &hw_lme->param_set[instance];
	param_set->fcount = 0;

	if (hw_ip->run_rsc_state)
		return 0;

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static int __is_hw_lme_set_rdma(struct is_hw_ip *hw_ip, struct is_hw_lme *hw_lme,
	struct lme_param_set *param_set, u32 instance, u32 is_reprocessing, u32 id, u32 set_id)
{
	dma_addr_t *input_dva = NULL;
	u32 cmd;
	int ret = 0;
	dma_addr_t mbmv_dva[2] = {0, 0};
	u32 total_width, line_count;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_lme);
	FIMC_BUG(!param_set);

	switch (id) {
	case LME_RDMA_CACHE_IN_0:
		if (is_reprocessing) {
			input_dva = param_set->cur_input_dva;
			cmd = param_set->dma.cur_input_cmd;
			sdbg_hw(4, "[HW][%s] MF STILL CUR cmd: %d, input_dva[0]: 0x%llx\n",
			hw_ip, __func__, cmd, input_dva[0]);
		} else {
			input_dva = param_set->prev_input_dva;
			cmd = param_set->dma.prev_input_cmd;
		}
		break;
	case LME_RDMA_CACHE_IN_1:
		if (is_reprocessing) {
			input_dva = param_set->prev_input_dva;
			cmd = param_set->dma.prev_input_cmd;
			sdbg_hw(4, "[HW][%s] MF STILL REF cmd: %d, input_dva[0]: 0x%llx\n",
				hw_ip, __func__, cmd, input_dva[0]);
		} else {
			input_dva = param_set->cur_input_dva;
			cmd = param_set->dma.cur_input_cmd;
		}
		break;
	case LME_RDMA_MBMV_IN:
		input_dva = mbmv_dva;
		if (hw_lme->lme_mode == LME_MODE_TNR) {
			total_width = 2 * DIV_ROUND_UP(param_set->dma.output_width, 16);
			line_count = DIV_ROUND_UP(param_set->dma.output_height, 16);

			input_dva[0] = param_set->mbmv1_dva[0] + total_width * (line_count - 1);
			input_dva[1] = param_set->mbmv0_dva[0];
		}
		cmd = param_set->dma.cur_input_cmd;
		break;
	default:
		merr_hw("invalid ID (%d)", instance, id);
		return -EINVAL;
	}

	sdbg_hw(4, "[HW][%s] RDMA id: %d, cmd: %d, input_dva[0]: 0x%llx, input_dva[1]: 0x%llx\n",
		hw_ip, __func__, id, cmd, input_dva[0], input_dva[1]);

	ret = lme_hw_s_rdma_init(hw_ip->regs[REG_SETA], param_set, cmd, id, set_id);

	if (ret) {
		mserr_hw("failed to initialize LME_RDMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (cmd == DMA_INPUT_COMMAND_ENABLE) {
		ret = lme_hw_s_rdma_addr(hw_ip->regs[REG_SETA], input_dva, id, set_id);
		if (ret) {
			mserr_hw("failed to set LME_RDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
	}

	return 0;
}

static int __is_hw_lme_set_wdma(struct is_hw_ip *hw_ip, struct is_hw_lme *hw_lme,
	struct lme_param_set *param_set, u32 instance, u32 id, u32 set_id)
{
	dma_addr_t output_dva[2] = {0, 0};
	u32 total_width, line_count;
	u32 cmd = 0;
	int ret;
	struct is_lme_config *config;
	enum lme_sps_out_mode sps_mode;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_lme);
	FIMC_BUG(!param_set);

	config = &hw_lme->config[instance];
	/* fix-me : get sps_mode from config */
	sps_mode = LME_OUTPUT_MODE_8_4;

	switch (id) {
	case LME_WDMA_MV_OUT:
		total_width = 4 * DIV_ROUND_UP(param_set->dma.cur_input_width, 8);
		line_count = DIV_ROUND_UP(param_set->dma.cur_input_height, 4);

		output_dva[0] = param_set->output_dva[0] + total_width * (line_count - 1);
		cmd = param_set->dma.output_cmd;
		break;
	case LME_WDMA_MBMV_OUT:
		if (hw_lme->lme_mode == LME_MODE_TNR) {
			total_width = 2 * DIV_ROUND_UP(param_set->dma.output_width, 16);
			line_count = DIV_ROUND_UP(param_set->dma.output_height, 16);

			output_dva[0] = param_set->mbmv1_dva[0];
			output_dva[1] = param_set->mbmv0_dva[0] + total_width * (line_count - 1);
		}
		cmd = param_set->dma.output_cmd;
		break;
	case LME_WDMA_SAD_OUT:
		total_width = LME_SAD_DATA_SIZE * DIV_ROUND_UP(param_set->dma.cur_input_width, 8);
		line_count = DIV_ROUND_UP(param_set->dma.cur_input_height, 4);

		output_dva[0] = param_set->output_dva_sad[0] + total_width * (line_count - 1);
		cmd = param_set->dma.sad_output_cmd;
		break;
	default:
		merr_hw("invalid ID (%d)", instance, id);
		return -EINVAL;
	}

	sdbg_hw(4, "[HW][%s] WDMA id: %d, cmd: %d, output_dva[0]: 0x%llx, output_dva[1]: 0x%llx\n",
		hw_ip, __func__, id, cmd, output_dva[0], output_dva[1]);

	ret = lme_hw_s_wdma_init(hw_ip->regs[REG_SETA], param_set, cmd, id, set_id, sps_mode);

	if (ret) {
		mserr_hw("failed to initializ LME_WDMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (cmd == DMA_OUTPUT_COMMAND_ENABLE) {
		ret = lme_hw_s_wdma_addr(hw_ip->regs[REG_SETA], output_dva, id, set_id);
		if (ret) {
			mserr_hw("failed to set LME_WDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
	}

	return 0;
}

static int __is_hw_lme_bypass(struct is_hw_ip *hw_ip, u32 set_id, struct lme_param_set *param_set)
{
	u32 cur_width, cur_height;

	FIMC_BUG(!hw_ip);

	cur_width = param_set->dma.cur_input_width;
	cur_height = param_set->dma.cur_input_height;
	lme_hw_s_block_bypass(hw_ip->regs[REG_SETA], set_id);
	return 0;
}

static int __is_hw_lme_update_block_reg(struct is_hw_ip *hw_ip,
	struct lme_param_set *param_set, u32 instance, u32 set_id)
{
	struct is_hw_lme *hw_lme;
	int ret = 0;
	u32 cache_en;
	u32 prev_width, cur_width, cur_height;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

	if (hw_lme->instance != instance) {
		msdbg_hw(2, "update_param: hw_ip->instance(%d)\n",
			instance, hw_ip, hw_lme->instance);
		hw_lme->instance = instance;
	}

	cache_en = 1;
	prev_width = param_set->dma.prev_input_width;
	cur_width = param_set->dma.cur_input_width;
	cur_height = param_set->dma.cur_input_height;

	/* Block Config */
	lme_hw_s_cache(hw_ip->regs[REG_SETA], set_id, cache_en, prev_width, cur_width);
	ret = __is_hw_lme_bypass(hw_ip, set_id, param_set);

	return ret;
}

static void __is_hw_lme_update_param_internal_buf(struct is_hw_ip *hw_ip, struct is_frame *frame,
						u32 instance, struct lme_param_set *param_set)
{
	struct is_framemgr *i_framemgr;
	struct is_frame *i_frame;
	struct pablo_internal_subdev *sd;
	struct is_hw_lme *hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	int buf_id;
	dma_addr_t *dst_dva;
	char name[IS_STR_LEN];

	for (buf_id = 0; buf_id < LME_INTERNAL_BUF_ID_MAX; buf_id++) {
		sd = &hw_lme->subdev[instance][buf_id];

		i_framemgr = GET_SUBDEV_I_FRAMEMGR(sd);
		if (!i_framemgr) {
			mserr_hw("internal framemgr is NULL", instance, hw_ip);
			return;
		}

		i_frame = &i_framemgr->frames[0];
		if (!i_frame) {
			mserr_hw("internal frame is NULL", instance, hw_ip);
			return;
		}

		if (buf_id == LME_INTERNAL_BUF_MBMV_IN)
			dst_dva = param_set->mbmv0_dva;
		else if (buf_id == LME_INTERNAL_BUF_MBMV_OUT)
			dst_dva = param_set->mbmv1_dva;

		snprintf(name, sizeof(name), "LME_MBMV%d", buf_id);

		CALL_HW_OPS(hw_ip, dma_cfg, name, hw_ip,
			frame, frame->cur_buf_index,
			frame->num_buffers,
			&param_set->dma.cur_input_cmd,
			param_set->dma.input_plane,
			dst_dva, i_frame->dvaddr_buffer);

		msdbg_hw(4, "[MBMV%d]cmd:%d, dva[0]:%pad, frame_dva[0]:%pad\n",
			instance, hw_ip, buf_id, param_set->dma.cur_input_cmd,
			&dst_dva[0], &i_frame->dvaddr_buffer[0]);
	}
}

static void __is_hw_lme_update_param(struct is_hw_ip *hw_ip, struct is_param_region *p_region,
					struct lme_param_set *param_set, IS_DECLARE_PMAP(pmap),
					u32 instance)
{
	struct lme_param *param;

	FIMC_BUG_VOID(!hw_ip);
	FIMC_BUG_VOID(!param_set);

	param = &p_region->lme;
	param_set->instance_id = instance;

	/* check input/output */
	if (test_bit(PARAM_LME_DMA, pmap)) {
		memcpy(&param_set->dma, &param->dma,
			sizeof(struct param_lme_dma));
	}

	sdbg_hw(4, "[DBG][%s] param->cur_input_cmd: %d, prev_input_cmd: %d, output_cmd: %d\n",
		hw_ip, __func__, param->dma.cur_input_cmd,
		param->dma.prev_input_cmd, param->dma.output_cmd);

	sdbg_hw(4, "[DBG][%s] param_set->cur_input_cmd: %d, prev_input_cmd: %d, output_cmd: %d\n",
		hw_ip, __func__, param_set->dma.cur_input_cmd,
		param_set->dma.prev_input_cmd, param_set->dma.output_cmd);
}

static int is_hw_lme_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
		IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	int ret = 0;
	struct is_hw_lme *hw_lme;
	struct lme_param_set *param_set;

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

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info,
	hw_lme->instance = IS_STREAM_COUNT;

	param_set = &hw_lme->param_set[instance];

	__is_hw_lme_update_param(hw_ip, &region->parameter, param_set, pmap, instance);

	return ret;
}

static int __is_hw_lme_set_size_regs(struct is_hw_ip *hw_ip, struct lme_param_set *param_set,
	u32 instance, u32 set_id)
{
	struct is_hw_lme *hw_lme;
	struct is_lme_config *config;
	int ret = 0;
	u32 prev_width, prev_height;
	u32 cur_width, cur_height;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!param_set);

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	config = &hw_lme->config[instance];

	if (config->lme_in_w == 0 || config->lme_in_h == 0)
		return -EINVAL;

	param_set->dma.prev_input_width = config->lme_in_w;
	param_set->dma.prev_input_height = config->lme_in_h;
	param_set->dma.cur_input_width = config->lme_in_w;
	param_set->dma.cur_input_height = config->lme_in_h;
	param_set->dma.output_width = config->lme_in_w;
	param_set->dma.output_height = config->lme_in_h;

	prev_width = param_set->dma.prev_input_width;
	prev_height = param_set->dma.prev_input_height;
	cur_width = param_set->dma.cur_input_width;
	cur_height = param_set->dma.cur_input_height;

	/* TODO: Block size configuration */
	lme_hw_s_cache_size(hw_ip->regs[REG_SETA], set_id, prev_width, prev_height,
			cur_width, cur_height);
	lme_hw_s_mvct_size(hw_ip->regs[REG_SETA], set_id, cur_width, cur_height);

	/* FIX ME */
#if (DDK_INTERFACE_VER == 0x1010)
	lme_hw_s_first_frame(hw_ip->regs[REG_SETA], set_id, config->first_frame);
#endif
	lme_hw_s_sps_out_mode(hw_ip->regs[REG_SETA], set_id, LME_OUTPUT_MODE_8_4);

	return ret;
}

static int is_hw_lme_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	struct is_hw_lme *hw_lme;
	struct lme_param_set *param_set;
	struct is_param_region *param_region;
	struct lme_param *param;
	u32 fcount, instance;
	bool is_reprocessing;
	bool do_blk_cfg;
	u32 cur_idx;
	u32 set_id;
	int ret = 0, i = 0, batch_num;
	u32 cmd_cur_input, cmd_prev_input, cmd_output;

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

	if (!hw_ip->hardware) {
		mserr_hw("failed to get hardware", instance, hw_ip);
		return -EINVAL;
	}

	FIMC_BUG(!hw_ip->priv_info);
	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	param_set = &hw_lme->param_set[instance];
	param_region = frame->parameter;

	param = &param_region->lme;
	cur_idx = frame->cur_buf_index;
	fcount = frame->fcount;

	is_reprocessing = param_set->reprocessing;

	if (frame->type == SHOT_TYPE_INTERNAL) {
		cmd_cur_input = param_set->dma.cur_input_cmd;
		param_set->dma.cur_input_cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->cur_input_dva[0] = 0x0;

		cmd_prev_input = param_set->dma.prev_input_cmd;
		param_set->dma.prev_input_cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->prev_input_dva[0] = 0x0;

		cmd_output = param_set->dma.output_cmd;
		param_set->dma.output_cmd = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->output_dva[0] = 0x0;

		param_set->tnr_mode = TNR_PROCESSING_PREVIEW_POST_ON;
		hw_ip->internal_fcount[instance] = fcount;
		goto config;
	} else {
		FIMC_BUG(!frame->shot);
		/* per-frame control
		 * check & update size from region
		 */
		if (hw_ip->internal_fcount[instance] != 0) {
			hw_ip->internal_fcount[instance] = 0;
			param_set->dma.output_cmd = param->dma.output_cmd;
		}

		/*set TNR operation mode */
		if (frame->shot_ext) {
			param_set->tnr_mode = frame->shot_ext->tnr_mode;
		} else {
			mswarn_hw("frame->shot_ext is null", instance, hw_ip);
			param_set->tnr_mode = TNR_PROCESSING_PREVIEW_POST_ON;
		}
	}

	__is_hw_lme_update_param(hw_ip, param_region, param_set, frame->pmap, instance);

	/* DMA settings */

	/* Only support TNR mode (video mode) */
	hw_lme->lme_mode = LME_MODE_TNR;

	__is_hw_lme_update_param_internal_buf(hw_ip, frame, instance, param_set);

	cmd_cur_input = CALL_HW_OPS(hw_ip, dma_cfg, "lme_cur", hw_ip,
				frame, cur_idx, frame->num_buffers,
				&param_set->dma.cur_input_cmd,
				param_set->dma.input_plane,
				param_set->cur_input_dva,
				frame->dvaddr_buffer);

	sdbg_hw(4, "[HW][%s][CUR_IN] cmd: %d, input_dva: 0x%llx, dvaddr_buffer[0]: %pad\n",
		hw_ip, __func__, param_set->dma.cur_input_cmd,
		param_set->cur_input_dva[0], &frame->dvaddr_buffer[0]);

	if (is_reprocessing && !param_set->dma.prev_input_cmd) {
		/* MF STILL after 1st frame. Keep ref addr */
		cmd_prev_input = param_set->dma.prev_input_cmd = 1;
	} else {
		cmd_prev_input = CALL_HW_OPS(hw_ip, dma_cfg, "lme_prev", hw_ip,
					frame, cur_idx, frame->num_buffers,
					&param_set->dma.prev_input_cmd,
					param_set->dma.input_plane,
					param_set->prev_input_dva,
					frame->lmesTargetAddress);
	}

	/* For 1st frame, use current buffer as prev */
	if (!is_reprocessing && !param_set->dma.prev_input_cmd) {
		param_set->dma.prev_input_cmd = param_set->dma.cur_input_cmd;
		param_set->prev_input_dva[0] = param_set->cur_input_dva[0];
		cmd_prev_input = param_set->dma.prev_input_cmd;

		sdbg_hw(4, "[HW][%s] VIDEO 1st frame prev_input_dva = cur_input_dva = 0x%llx\n",
			hw_ip, __func__, param_set->cur_input_dva[0]);
	}

	sdbg_hw(4, "[HW][%s][PREV_IN] cmd: %d, input_dva: 0x%llx, lemdsTargetAddress[0]: %pad\n",
		hw_ip, __func__, param_set->dma.prev_input_cmd,
		param_set->prev_input_dva[0], &frame->lmesTargetAddress[0]);


	cmd_output = CALL_HW_OPS(hw_ip, dma_cfg, "lme_pure", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma.output_cmd,
			param_set->dma.output_plane,
			param_set->output_dva,
			frame->lmecTargetAddress);

	sdbg_hw(4, "[HW][MV_OUT] cmd: %d, output_dva: 0x%llx, lmecTargetAddress[0]: %pad\n",
		hw_ip, param_set->dma.output_cmd, param_set->output_dva[0], &frame->lmecTargetAddress[0]);

	CALL_HW_OPS(hw_ip, dma_cfg_kva64, "lme_pure", hw_ip,
			frame, cur_idx,
			&param_set->dma.output_cmd,
			param_set->dma.output_plane,
			param_set->output_kva_motion_pure,
			frame->lmecKTargetAddress);

	sdbg_hw(4, "[HW][MV_OUT_KVA] output_kva_motion_pure[0]: 0x%lx, lmecKTargetAddr[0]: 0x%lx\n",
		hw_ip, param_set->output_kva_motion_pure[0], frame->lmecKTargetAddress[0]);

	CALL_HW_OPS(hw_ip, dma_cfg, "lme_sad", hw_ip,
		frame, cur_idx, frame->num_buffers,
		&param_set->dma.sad_output_cmd,
		param_set->dma.output_plane,
		param_set->output_dva_sad,
		frame->lmesadTargetAddress);

	sdbg_hw(4, "[HW][SAD_OUT] cmd: %d, output_dva_sad[0]: 0x%llx, lmesadTargetAddr[0]:%pad,\n",
		hw_ip, param_set->dma.sad_output_cmd, param_set->output_dva_sad[0],
		&frame->lmesadTargetAddress[0]);

	CALL_HW_OPS(hw_ip, dma_cfg_kva64, "lme_proc", hw_ip,
			frame, cur_idx,
			&param_set->dma.processed_output_cmd,
			param_set->dma.output_plane,
			param_set->output_kva_motion_processed,
			frame->lmemKTargetAddress);

	sdbg_hw(4, "[HW][MV_SW_OUT_KVA] kva_motion_processed[0]:0x%lx, lmemKTargetAddr[0]:0x%lx\n",
		hw_ip, param_set->output_kva_motion_processed[0], frame->lmemKTargetAddress[0]);

config:
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

	/* Use only setA */
	hw_lme->corex_set_id = COREX_SET_A;
	set_id = hw_lme->corex_set_id;

	sdbg_hw(4, "[HW][%s] COREX_SET : %d\n", hw_ip, __func__, set_id);

	lme_hw_s_clock(hw_ip->regs[REG_SETA], true);

	lme_hw_s_core(hw_ip->regs[REG_SETA], set_id);

	__is_hw_lme_update_block_reg(hw_ip, param_set, instance, set_id);

	do_blk_cfg = CALL_HW_HELPER_OPS(hw_ip, set_rta_regs, instance, set_id, false, frame, NULL);

	if (!IS_ENABLED(IRTA_CALL) || unlikely(do_blk_cfg)) {
		hw_lme->config[instance].lme_in_w = param_set->dma.cur_input_width;
		hw_lme->config[instance].lme_in_h = param_set->dma.cur_input_height;

		lme_hw_s_mvct(hw_ip->regs[REG_SETA], set_id);
	}

	ret = __is_hw_lme_set_size_regs(hw_ip, param_set, instance, set_id);
	if (ret) {
		mserr_hw("__is_hw_lme_set_size_regs is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	for (i = LME_RDMA_CACHE_IN_0; i < LME_RDMA_MAX; i++) {
		ret = __is_hw_lme_set_rdma(hw_ip, hw_lme, param_set, instance,
			is_reprocessing, i, set_id);
		if (ret) {
			mserr_hw("__is_hw_lme_set_rdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	for (i = LME_WDMA_MV_OUT; i < LME_WDMA_MAX; i++) {
		ret = __is_hw_lme_set_wdma(hw_ip, hw_lme, param_set, instance,
				i, set_id);
		if (ret) {
			mserr_hw("__is_hw_lme_set_wdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	};

	if (unlikely(test_bit(LME_DEBUG_SFR, &debug_lme)))
		lme_hw_dump(hw_ip->regs[REG_SETA], COREX_SET_A);

	ret = is_hw_lme_set_cmdq(hw_ip, instance, set_id);
	if (ret < 0) {
		mserr_hw("is_hw_lme_set_cmdq is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	set_bit(HW_CONFIG, &hw_ip->state);

	return ret;

shot_fail_recovery:

	return ret;
}

static int is_hw_lme_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	return 0;
}

static int is_hw_lme_get_cap_meta(struct is_hw_ip *hw_ip, ulong hw_map,
	u32 instance, u32 fcount, u32 size, ulong addr)
{
	return 0;
}

static int is_hw_lme_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
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

static int is_hw_lme_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	return 0;
}

static int is_hw_lme_apply_setfile(struct is_hw_ip *hw_ip, u32 scenario,
	u32 instance, ulong hw_map)
{
	return 0;
}

static int is_hw_lme_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	return 0;
}

static int is_hw_lme_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	return 0;
}

static int is_hw_lme_set_regs(struct is_hw_ip *hw_ip, u32 chain_id,
	u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size)
{
	return 0;
}

static int is_hw_lme_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size, enum is_reg_dump_type dump_type)
{
	lme_hw_dump(hw_ip->regs[REG_SETA], COREX_DIRECT);

	return 0;
}

static int is_hw_lme_set_config(struct is_hw_ip *hw_ip, u32 chain_id,
	u32 instance, u32 fcount, void *config)
{
	struct is_hw_lme *hw_lme = NULL;
	struct is_lme_config *lme_config = (struct is_lme_config *)config;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!config);

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

	if ((hw_lme->config[instance].lme_in_w != lme_config->lme_in_w)
		|| (hw_lme->config[instance].lme_in_h != lme_config->lme_in_h))
		msinfo_hw("[F:%d] LME in size changed: %dx%d  -> %dx%d", instance, hw_ip, fcount,
			hw_lme->config[instance].lme_in_w, hw_lme->config[instance].lme_in_h,
			lme_config->lme_in_w, lme_config->lme_in_h);

#if (DDK_INTERFACE_VER == 0x1010)
	if (hw_lme->config[instance].first_frame != lme_config->first_frame)
		msinfo_hw("[F:%d] LME first_frame changed: %d -> %d", instance, hw_ip, fcount,
			hw_lme->config[instance].first_frame, lme_config->first_frame);
#endif

	memcpy(&hw_lme->config[instance], config, sizeof(struct is_lme_config));

	return 0;
}

static int is_hw_lme_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	if (test_bit(HW_END, &hw_ip->state))
		msinfo_hw("HW end interrupt occur but no end callback", instance, hw_ip);

	/* DEBUG */
	lme_hw_dump(hw_ip->regs[REG_SETA], COREX_DIRECT);

	return 0;
}

const struct is_hw_ip_ops is_hw_lme_ops = {
	.open			= is_hw_lme_open,
	.init			= is_hw_lme_init,
	.deinit			= is_hw_lme_deinit,
	.close			= is_hw_lme_close,
	.enable			= is_hw_lme_enable,
	.disable		= is_hw_lme_disable,
	.shot			= is_hw_lme_shot,
	.set_param		= is_hw_lme_set_param,
	.get_meta		= is_hw_lme_get_meta,
	.get_cap_meta		= is_hw_lme_get_cap_meta,
	.frame_ndone		= is_hw_lme_frame_ndone,
	.load_setfile		= is_hw_lme_load_setfile,
	.apply_setfile		= is_hw_lme_apply_setfile,
	.delete_setfile		= is_hw_lme_delete_setfile,
	.restore		= is_hw_lme_restore,
	.set_regs		= is_hw_lme_set_regs,
	.dump_regs		= is_hw_lme_dump_regs,
	.set_config		= is_hw_lme_set_config,
	.notify_timeout		= is_hw_lme_notify_timeout,
};

int is_hw_lme_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int hw_slot;

	hw_ip->ops  = &is_hw_lme_ops;
	hw_ip->dump_for_each_reg = lme_hw_get_reg_struct();
	hw_ip->dump_reg_list_size = lme_hw_get_reg_cnt();

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &is_hw_lme_handle_interrupt;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	return 0;
}
EXPORT_SYMBOL_GPL(is_hw_lme_probe);
