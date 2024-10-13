// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-hw-helper.h"
#include "is-hw-shrp.h"
#include "is-err.h"
#include "api/is-hw-api-shrp-v3_0.h"
#include "is-votfmgr.h"
#include "is-votf-id-table.h"
#include "is-stripe.h"

static inline void *get_base(struct is_hw_ip *hw_ip) {
	if (IS_ENABLED(SHRP_USE_MMIO))
		return hw_ip->regs[REG_SETA];
	return hw_ip->pmio;
}

static struct is_shrp_config config_default = {
	.sharpen_contents_aware_isp_en = 0,
};

static int is_hw_shrp_handle_interrupt0(u32 id, void *context)
{
	struct is_hardware *hardware;
	struct is_hw_ip *hw_ip;
	struct is_hw_shrp *hw_shrp;
	struct shrp_param_set *param_set;
	u32 status, instance, hw_fcount;
	bool f_err = false;

	hw_ip = (struct is_hw_ip *)context;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);

	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("invalid interrupt, hw_ip state(0x%lx)", instance, hw_ip, hw_ip->state);
		return 0;
	}

	hw_shrp = (struct is_hw_shrp *)hw_ip->priv_info;
	param_set = &hw_shrp->param_set[instance];

	status = shrp_hw_g_int_state0(get_base(hw_ip), true,
			(hw_ip->num_buffers & 0xffff), &hw_shrp->irq_state[SHRP0_INTR])
			& shrp_hw_g_int_mask0(get_base(hw_ip));

	msdbg_hw(2, "SHRP0 interrupt status(0x%x)\n", instance, hw_ip, status);

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

	if (shrp_hw_is_occurred0(status, INTR_SETTING_DONE))
		CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_SETTING_DONE);

	if (shrp_hw_is_occurred0(status, INTR_FRAME_START) && shrp_hw_is_occurred0(status, INTR_FRAME_END))
		mswarn_hw("start/end overlapped!! (0x%x)", instance, hw_ip, status);

	if (shrp_hw_is_occurred0(status, INTR_FRAME_START)) {
		atomic_add((hw_ip->num_buffers & 0xffff), &hw_ip->count.fs);
		CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_FRAME_START);
		if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
			msinfo_hw("[F:%d]F.S\n", instance, hw_ip, hw_fcount);

		CALL_HW_OPS(hw_ip, frame_start, hw_ip, instance);
	}

	if (shrp_hw_is_occurred0(status, INTR_FRAME_END)) {
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

		if (unlikely(is_get_debug_param(IS_DEBUG_PARAM_SHRP)))
			shrp_hw_print_chain_debug_cnt(get_base(hw_ip));
	}

	f_err = shrp_hw_is_occurred0(status, INTR_ERR);
	if (f_err) {
		msinfo_hw("[SHRP0][F:%d] Ocurred error interrupt : status(0x%x)\n",
			instance, hw_ip, hw_fcount, status);
		shrp_hw_dump(get_base(hw_ip));
	}

	return 0;
}

static int is_hw_shrp_handle_interrupt1(u32 id, void *context)
{
	struct is_hardware *hardware;
	struct is_hw_ip *hw_ip;
	struct is_hw_shrp *hw_shrp;
	u32 status, instance, hw_fcount;
	bool f_err;

	hw_ip = (struct is_hw_ip *)context;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);

	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("invalid interrupt, hw_ip state(0x%lx)", instance, hw_ip, hw_ip->state);
		return 0;
	}

	hw_shrp = (struct is_hw_shrp *)hw_ip->priv_info;

	status = shrp_hw_g_int_state1(get_base(hw_ip), true,
			(hw_ip->num_buffers & 0xffff), &hw_shrp->irq_state[SHRP1_INTR])
			& shrp_hw_g_int_mask1(get_base(hw_ip));

	msdbg_hw(2, "SHRP1 interrupt status(0x%x)\n", instance, hw_ip, status);

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

	f_err = shrp_hw_is_occurred1(status, INTR_ERR);
	if (f_err) {
		msinfo_hw("[SHRP1][F:%d] Ocurred error interrupt : status(0x%x)\n",
			instance, hw_ip, hw_fcount, status);
		shrp_hw_dump(get_base(hw_ip));
	}

	return 0;
}

static int is_hw_shrp_reset(struct is_hw_ip *hw_ip, u32 instance)
{
	msinfo_hw("reset\n", instance, hw_ip);

	if (shrp_hw_s_reset(get_base(hw_ip))) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}

	return 0;
}

static void is_hw_shrp_prepare(struct is_hw_ip *hw_ip, u32 instance)
{
	u32 seed;

	shrp_hw_s_init(get_base(hw_ip));

	seed = is_get_debug_param(IS_DEBUG_PARAM_CRC_SEED);
	if (unlikely(seed))
		shrp_hw_s_crc(get_base(hw_ip), seed);
}

static int is_hw_shrp_finish(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;

	ret = CALL_HWIP_OPS(hw_ip, reset, instance);
	if (ret)
		return ret;

	ret = shrp_hw_wait_idle(get_base(hw_ip));
	if (ret)
		mserr_hw("failed to shrp_hw_wait_idle", instance, hw_ip);

	msinfo_hw("final finished shrp\n", instance, hw_ip);

	return ret;
}

static int is_hw_shrp_set_cmdq(struct is_hw_ip *hw_ip, u32 instance, u32 set_id,
			       u32 num_buffers, struct c_loader_buffer *clb)
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

	ret = shrp_hw_s_corex_update_type(get_base(hw_ip), set_id);
	if (ret)
		return ret;

	if (clb)
		shrp_hw_s_cmdq(get_base(hw_ip), set_id, num_buffers, clb->header_dva, clb->num_of_headers);
	else
		shrp_hw_s_cmdq(get_base(hw_ip), set_id, num_buffers, 0, 0);

	return ret;
}

static int __nocfi is_hw_shrp_open(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;
	struct is_hw_shrp *hw_shrp;
	u32 rdma_max_cnt;
	struct is_mem *mem;

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HWSHRP");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw_ip->priv_info = vzalloc(sizeof(struct is_hw_shrp));
	if (!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	hw_shrp = (struct is_hw_shrp *)hw_ip->priv_info;
	hw_shrp->instance = instance;

	ret = CALL_HW_HELPER_OPS(hw_ip, open, instance, &hw_shrp->lib[instance],
				LIB_FUNC_SHRP);
	if (ret)
		goto err_chain_create;

	ret = CALL_HW_HELPER_OPS(hw_ip, alloc_iqset, shrp_hw_g_reg_cnt());
	if (ret)
		goto err_iqset_alloc;

	rdma_max_cnt = shrp_hw_g_rdma_max_cnt();
	hw_shrp->rdma = vzalloc(sizeof(struct is_common_dma) * rdma_max_cnt);
	if (!hw_shrp->rdma) {
		mserr_hw("Failed to allocate rdma", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc_dma;
	}

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);

	mem = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_iommu_mem, GROUP_ID_SHRP);

	hw_shrp->pb_c_loader_payload = CALL_PTR_MEMOP(mem,
						alloc,
						mem->priv,
						0x16000,
						NULL,
						0);
	if (IS_ERR_OR_NULL(hw_shrp->pb_c_loader_payload)) {
		hw_shrp->pb_c_loader_payload = NULL;
		err("failed to allocate buffer for c-loader payload");
		ret = -ENOMEM;
		goto err_alloc_dma;
	}
	hw_shrp->kva_c_loader_payload = CALL_BUFOP(hw_shrp->pb_c_loader_payload,
					kvaddr, hw_shrp->pb_c_loader_payload);
	hw_shrp->dva_c_loader_payload = CALL_BUFOP(hw_shrp->pb_c_loader_payload,
					dvaddr, hw_shrp->pb_c_loader_payload);

	hw_shrp->pb_c_loader_header = CALL_PTR_MEMOP(mem, alloc,
					mem->priv, 0x4000,
					NULL, 0);
	if (IS_ERR_OR_NULL(hw_shrp->pb_c_loader_header)) {
		hw_shrp->pb_c_loader_header = NULL;
		err("failed to allocate buffer for c-loader header");
		ret = -ENOMEM;
		goto err_alloc_dma;
	}
	hw_shrp->kva_c_loader_header = CALL_BUFOP(hw_shrp->pb_c_loader_header,
					kvaddr, hw_shrp->pb_c_loader_header);
	hw_shrp->dva_c_loader_header = CALL_BUFOP(hw_shrp->pb_c_loader_header,
					dvaddr, hw_shrp->pb_c_loader_header);

	set_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "open: framemgr[%s]", instance, hw_ip, hw_ip->framemgr->name);

	return 0;

err_alloc_dma:
	if (hw_shrp->rdma)
		vfree(hw_shrp->rdma);

err_iqset_alloc:
	CALL_HW_HELPER_OPS(hw_ip, close, instance, &hw_shrp->lib[instance]);

err_chain_create:
	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_shrp_init(struct is_hw_ip *hw_ip, u32 instance,
			bool flag, u32 f_type)
{
	int ret;
	struct is_hw_shrp *hw_shrp;
	u32 rmda_max_cnt;
	u32 dma_id;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	hw_shrp = (struct is_hw_shrp *)hw_ip->priv_info;
	if (!hw_shrp) {
		mserr_hw("hw_shrp is null ", instance, hw_ip);
		ret = -ENODATA;
		goto err;
	}

	ret = CALL_HW_HELPER_OPS(hw_ip, init, instance, &hw_shrp->lib[instance],
				(u32)flag, f_type, LIB_FUNC_SHRP);
	if (ret)
		return ret;

	rmda_max_cnt = shrp_hw_g_rdma_max_cnt();
	for (dma_id = 0; dma_id < rmda_max_cnt; dma_id++) {
		ret = shrp_hw_rdma_create(&hw_shrp->rdma[dma_id], get_base(hw_ip), dma_id);
		if (ret) {
			mserr_hw("shrp_hw_rdma_create error[%d]", instance, hw_ip, dma_id);
			ret = -ENODATA;
			goto err;
		}
	}

	set_bit(HW_INIT, &hw_ip->state);
	return 0;

err:
	return ret;

}

static int is_hw_shrp_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_shrp *hw_shrp = (struct is_hw_shrp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, deinit, instance, &hw_shrp->lib[instance]);
}

static int is_hw_shrp_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_shrp *hw_shrp;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_shrp = (struct is_hw_shrp *)hw_ip->priv_info;

	CALL_HW_HELPER_OPS(hw_ip, close, instance, &hw_shrp->lib[instance]);

	is_hw_shrp_finish(hw_ip, instance);

	CALL_BUFOP(hw_shrp->pb_c_loader_payload, free, hw_shrp->pb_c_loader_payload);
	CALL_BUFOP(hw_shrp->pb_c_loader_header, free, hw_shrp->pb_c_loader_header);

	CALL_HW_HELPER_OPS(hw_ip, free_iqset);

	vfree(hw_shrp->rdma);

	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	clear_bit(HW_OPEN, &hw_ip->state);

	return 0;
}

static int is_hw_shrp_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_shrp *hw_shrp;
	int ret;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	if (test_bit(HW_RUN, &hw_ip->state))
		return 0;

	msdbg_hw(2, "enable: start\n", instance, hw_ip);

	hw_shrp = (struct is_hw_shrp *)hw_ip->priv_info;

	ret = CALL_HWIP_OPS(hw_ip, reset, instance);
	if (ret)
		return ret;

	if (!IS_ENABLED(SHRP_USE_MMIO)) {
		hw_ip->pmio_config.cache_type = PMIO_CACHE_FLAT;
		if (pmio_reinit_cache(hw_ip->pmio, &hw_ip->pmio_config)) {
			pmio_cache_set_bypass(hw_ip->pmio, true);
			err("failed to reinit PMIO cache, set bypass");
			return -EINVAL;
		}
	}

	is_hw_shrp_prepare(hw_ip, instance);

	if (!IS_ENABLED(SHRP_USE_MMIO))
		pmio_cache_set_only(hw_ip->pmio, true);

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return 0;
}

static int is_hw_shrp_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_shrp *hw_shrp;
	struct shrp_param_set *param_set;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("shrp_disable: Vvalid(%d)\n", instance, hw_ip,
		atomic_read(&hw_ip->status.Vvalid));

	hw_shrp = (struct is_hw_shrp *)hw_ip->priv_info;

	timetowait = wait_event_timeout(hw_ip->status.wait_queue,
		!atomic_read(&hw_ip->status.Vvalid),
		IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		mserr_hw("wait FRAME_END timeout (%ld)", instance,
			hw_ip, timetowait);
		ret = -ETIME;
	}

	param_set = &hw_shrp->param_set[instance];
	param_set->fcount = 0;

	CALL_HW_HELPER_OPS(hw_ip, disable, instance, &hw_shrp->lib[instance]);

	if (hw_ip->run_rsc_state)
		return 0;

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static int __is_hw_shrp_set_rdma_cmd(struct shrp_param_set *param_set, u32 instance,
	struct is_shrp_config *conf)
{
	mdbg_hw(4, "%s\n", instance, __func__);

	if (!conf->sharpen_contents_aware_isp_en)
		param_set->dma_input_seg.cmd = DMA_INPUT_COMMAND_DISABLE;

	return 0;
}

static int __is_hw_shrp_set_rdma(struct is_hw_ip *hw_ip, struct is_hw_shrp *hw_shrp,
			struct shrp_param_set *param_set, u32 instance, u32 id, u32 set_id)
{
	msdbg_hw(2, "%s:\n", instance, hw_ip, __func__);

	return 0;
}

static void __is_hw_shrp_check_size(struct is_hw_ip *hw_ip, struct shrp_param_set *param_set, u32 instance)
{
	struct param_dma_input *input = &param_set->dma_input;
	struct param_otf_output *output = &param_set->otf_output;

	msdbgs_hw(2, "hw_shrp_check_size >>>\n", instance, hw_ip);
	msdbgs_hw(2, "dma_input: format(%d),crop_size(%dx%d)\n", instance, hw_ip,
		input->format, input->dma_crop_width, input->dma_crop_height);
	msdbgs_hw(2, "otf_output: otf_cmd(%d),format(%d)\n", instance, hw_ip,
		output->cmd, output->format);
	msdbgs_hw(2, "otf_output: pos(%d,%d),crop%dx%d),size(%dx%d)\n", instance, hw_ip,
		output->crop_offset_x, output->crop_offset_y,
		output->crop_width, output->crop_height,
		output->width, output->height);
	msdbgs_hw(2, "<<< hw_shrp_check_size <<<\n", instance, hw_ip);
}

static int __is_hw_shrp_bypass(struct is_hw_ip *hw_ip, u32 set_id)
{
	sdbg_hw(4, "%s\n", hw_ip, __func__);

	shrp_hw_s_block_bypass(get_base(hw_ip), set_id);

	return 0;
}


static int __is_hw_shrp_update_block_reg(struct is_hw_ip *hw_ip, struct shrp_param_set *param_set, u32 instance, u32 set_id)
{
	struct is_hw_shrp *hw_shrp;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	hw_shrp = (struct is_hw_shrp *)hw_ip->priv_info;

	if (hw_shrp->instance != instance) {
		msdbg_hw(2, "update_param: hw_ip->instance(%d)\n",
			instance, hw_ip, hw_shrp->instance);
		hw_shrp->instance = instance;
	}

	__is_hw_shrp_check_size(hw_ip, param_set, instance);
	return __is_hw_shrp_bypass(hw_ip, set_id);
}

static void __is_hw_shrp_update_param(struct is_hw_ip *hw_ip, struct is_param_region *p_region,
					struct shrp_param_set *param_set, IS_DECLARE_PMAP(pmap),
					u32 instance)
{
	struct shrp_param *param;

	param = &p_region->shrp;
	param_set->instance_id = instance;

	param_set->mono_mode = hw_ip->region[instance]->parameter.sensor.config.mono_mode;

	/* check input */
	if (test_bit(PARAM_SHRP_OTF_INPUT, pmap)) {
		memcpy(&param_set->otf_input, &param->otf_input,
			sizeof(struct param_otf_input));
		msdbg_hw(4, "%s : PARAM_SHRP_OTF_INPUT\n", instance, hw_ip, __func__);
	}

	/* check output */
	if (test_bit(PARAM_SHRP_OTF_OUTPUT, pmap)) {
		memcpy(&param_set->otf_output, &param->otf_output,
			sizeof(struct param_otf_output));
		msdbg_hw(4, "%s : PARAM_SHRP_OTF_OUTPUT\n", instance, hw_ip, __func__);
	}

	if (test_bit(PARAM_SHRP_STRIPE_INPUT, pmap)) {
		memcpy(&param_set->stripe_input, &param->stripe_input,
			sizeof(struct param_stripe_input));
		msdbg_hw(4, "%s : PARAM_SHRP_STRIPE_INPUT\n", instance, hw_ip, __func__);
	}
}

static int is_hw_shrp_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
		IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	struct is_hw_shrp *hw_shrp;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;

	hw_shrp = (struct is_hw_shrp *)hw_ip->priv_info;
	hw_shrp->instance = IS_STREAM_COUNT;

	return 0;
}

static void __is_hw_shrp_set_strip_regs(struct is_hw_ip *hw_ip,
	struct shrp_param_set *param_set, u32 enable, u32 start_pos, u32 set_id)
{
	u32 strip_index, strip_total_count;
	enum shrp_strip_type strip_type = SHRP_STRIP_TYPE_NONE;

	if (enable) {
		strip_index = param_set->stripe_input.index;
		strip_total_count = param_set->stripe_input.total_count;

		if (!strip_index)
			strip_type = SHRP_STRIP_TYPE_FIRST;
		else if (strip_index == strip_total_count - 1)
			strip_type = SHRP_STRIP_TYPE_LAST;
		else
			strip_type = SHRP_STRIP_TYPE_MIDDLE;
	}
	shrp_hw_s_strip(get_base(hw_ip), set_id, enable,
		start_pos, strip_type, param_set->stripe_input.left_margin,
		param_set->stripe_input.right_margin,
		param_set->stripe_input.full_width);
}

static int __is_hw_shrp_set_size_regs(struct is_hw_ip *hw_ip, struct shrp_param_set *param_set,
	u32 instance, u32 strip_start_pos, const struct is_frame *frame, u32 set_id,
	enum shrp_input_path_type input_path, enum shrp_chain_demux_type output_path)
{
	struct is_hw_shrp *hw_shrp;
	struct is_shrp_config *config;
	int ret;
	u32 strip_enable;
	u32 width, height;
	u32 frame_width;
	u32 region_id;
	struct shrp_radial_cfg radial_cfg;
	bool mono_mode_en;

	hw_shrp = (struct is_hw_shrp *)hw_ip->priv_info;
	config = &hw_shrp->config;

	strip_enable = (param_set->stripe_input.total_count == 0) ? 0 : 1;
	mono_mode_en = param_set->mono_mode;

	region_id = param_set->stripe_input.index;

	if (input_path == SHRP_INPUT_RDMA) {
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

	ret = shrp_hw_s_sharpen_size(get_base(hw_ip), set_id, strip_start_pos, frame_width,
		width, height, strip_enable, &radial_cfg);
	if (ret) {
		mserr_hw("failed to set size regs for sharp adder", instance, hw_ip);
		goto err;
	}

	__is_hw_shrp_set_strip_regs(hw_ip, param_set, strip_enable, strip_start_pos, set_id);

	shrp_hw_s_size(get_base(hw_ip), set_id, width, height, strip_enable);
err:
	return ret;
}

static void __is_hw_shrp_config_path(struct is_hw_ip *hw_ip, struct shrp_param_set *param_set,
	u32 set_id, enum shrp_input_path_type *input_path, enum shrp_chain_demux_type *output_path)
{
	struct is_hw_shrp *hw_shrp = (struct is_hw_shrp *)hw_ip->priv_info;

	if (param_set->dma_input.cmd == DMA_INPUT_COMMAND_ENABLE) {
		*input_path = SHRP_INPUT_RDMA;
		shrp_hw_s_mux_dtp(get_base(hw_ip), set_id, SHRP_MUX_DTP_RDMA_YUV);
	} else {
		*input_path = SHRP_INPUT_OTF;
		shrp_hw_s_cinfifo(get_base(hw_ip), set_id);
		shrp_hw_s_mux_dtp(get_base(hw_ip), set_id, SHRP_MUX_DTP_CINFIFO);
	}
	shrp_hw_s_input_path(get_base(hw_ip), set_id, *input_path);

	*output_path = 0;

	if (param_set->otf_output.cmd == DMA_INPUT_COMMAND_ENABLE) {
		*output_path |= SHRP_DEMUX_DITHER_COUTFIFO_0;
		shrp_hw_s_coutfifo(get_base(hw_ip), set_id);
		shrp_hw_s_output_path(get_base(hw_ip), set_id, 1);
	} else {
		shrp_hw_s_output_path(get_base(hw_ip), set_id, 0);
	}

	*output_path |= SHRP_DEMUX_YUVNR_YUV422TO444;

	if (param_set->dma_input_seg.cmd == DMA_INPUT_COMMAND_ENABLE) {
		if (hw_shrp->config.sharpen_contents_aware_isp_en)
			*output_path |= SHRP_DEMUX_SEGCONF_SHARPEN;
	}
}

static void __is_hw_shrp_config_dma(struct is_hw_ip *hw_ip, struct shrp_param_set *param_set,
	struct is_frame *frame, u32 fcount, u32 instance)
{

}

static int is_hw_shrp_set_config(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount, void *config)
{
	struct is_hw_shrp *hw_shrp;
	struct is_shrp_config *conf = (struct is_shrp_config *)config;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);
	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!conf);

	hw_shrp = (struct is_hw_shrp *)hw_ip->priv_info;
	hw_shrp->config = *conf;

	msdbg_hw(4, "sharpen_contents_aware_isp_en %d\n", instance, hw_ip, hw_shrp->config.sharpen_contents_aware_isp_en);

	return 0;
}

static int is_hw_shrp_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	struct is_hw_shrp *hw_shrp;
	struct is_param_region *param_region;
	struct shrp_param_set *param_set;
	struct is_frame *dma_frame;
	u32 fcount, instance;
	u32 cur_idx, batch_num;
	u32 strip_start_pos;
	u32 set_id;
	int ret, i;
	u32 strip_index, strip_total_count;
	enum shrp_input_path_type input_path;
	enum shrp_chain_demux_type output_path;
	bool do_blk_cfg;
	bool skip = false;
	u32 rdma_max_cnt;
	struct c_loader_buffer clb;

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

	hw_shrp = (struct is_hw_shrp *)hw_ip->priv_info;
	param_set = &hw_shrp->param_set[instance];
	param_region = frame->parameter;

	atomic_set(&hw_shrp->strip_index, frame->stripe_info.region_id);
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

	__is_hw_shrp_update_param(hw_ip, param_region, param_set, frame->pmap, instance);
	__is_hw_shrp_config_dma(hw_ip, param_set, frame, fcount, instance);

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

	/* reset CLD buffer */
	clb.num_of_headers = 0;
	clb.num_of_values = 0;
	clb.num_of_pairs = 0;

	clb.header_dva = hw_shrp->dva_c_loader_header;
	clb.payload_dva = hw_shrp->dva_c_loader_payload;

	clb.clh = (struct c_loader_header *)hw_shrp->kva_c_loader_header;
	clb.clp = (struct c_loader_payload *)hw_shrp->kva_c_loader_payload;

	shrp_hw_s_clock(get_base(hw_ip), true);

	shrp_hw_s_core(get_base(hw_ip), frame->num_buffers, set_id);

	hw_ip->num_buffers = frame->num_buffers;

	ret = CALL_HW_HELPER_OPS(hw_ip, lib_shot, instance, skip,
				frame, &hw_shrp->lib[instance], param_set);
	if (ret)
		return ret;

	do_blk_cfg = CALL_HW_HELPER_OPS(hw_ip, set_rta_regs, instance, set_id, skip, frame, (void *)&clb);
	if (unlikely(do_blk_cfg)) {
		is_hw_shrp_set_config(hw_ip, 0, instance, fcount, &config_default);
		__is_hw_shrp_update_block_reg(hw_ip, param_set, instance, set_id);
	}

	__is_hw_shrp_config_path(hw_ip, param_set, set_id, &input_path, &output_path);
	__is_hw_shrp_set_rdma_cmd(param_set, instance, &hw_shrp->config);

	ret = __is_hw_shrp_set_size_regs(hw_ip, param_set, instance, strip_start_pos, frame,
					 set_id, input_path, output_path);
	if (ret) {
		mserr_hw("__is_hw_shrp_set_size_regs is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	rdma_max_cnt = shrp_hw_g_rdma_max_cnt();
	for (i = 0; i < rdma_max_cnt; i++) {
		ret = __is_hw_shrp_set_rdma(hw_ip, hw_shrp, param_set, instance, i, set_id);
		if (ret) {
			mserr_hw("__is_hw_shrp_set_rdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	if (param_region->shrp.control.strgen == CONTROL_COMMAND_START) {
		msdbg_hw(2, "[SHRP]STRGEN input\n", instance, hw_ip);
		shrp_hw_s_strgen(get_base(hw_ip), set_id);
	}

	if (!IS_ENABLED(SHRP_USE_MMIO)) {
		pmio_cache_fsync(hw_ip->pmio, (void *)&clb, PMIO_FORMATTER_PAIR);
		if (clb.num_of_pairs > 0)
			clb.num_of_headers++;

		CALL_BUFOP(hw_shrp->pb_c_loader_payload, sync_for_device, hw_shrp->pb_c_loader_payload,
			0, hw_shrp->pb_c_loader_payload->size, DMA_TO_DEVICE);
		CALL_BUFOP(hw_shrp->pb_c_loader_header, sync_for_device, hw_shrp->pb_c_loader_header,
			0, hw_shrp->pb_c_loader_header->size, DMA_TO_DEVICE);
	}

	CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, fcount, DEBUG_POINT_ADD_TO_CMDQ);
	ret = is_hw_shrp_set_cmdq(hw_ip, instance, set_id, frame->num_buffers, &clb);
	if (ret) {
		mserr_hw("is_hw_shrp_set_cmdq is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	shrp_hw_s_clock(get_base(hw_ip), false);

	if (unlikely(is_get_debug_param(IS_DEBUG_PARAM_SHRP)))
		shrp_hw_dump(get_base(hw_ip));

	set_bit(HW_CONFIG, &hw_ip->state);

	return 0;

shot_fail_recovery:
	CALL_HW_HELPER_OPS(hw_ip, restore, instance, &hw_shrp->lib[instance]);

	return ret;
}

static int is_hw_shrp_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	struct is_hw_shrp *hw_shrp = (struct is_hw_shrp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, get_meta, frame->instance,
				frame, &hw_shrp->lib[frame->instance]);
}

static int is_hw_shrp_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
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


static int is_hw_shrp_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_shrp *hw_shrp = (struct is_hw_shrp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, load_setfile, instance, &hw_shrp->lib[instance]);
}

static int is_hw_shrp_apply_setfile(struct is_hw_ip *hw_ip, u32 scenario,
	u32 instance, ulong hw_map)
{
	struct is_hw_shrp *hw_shrp = (struct is_hw_shrp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, apply_setfile, instance, &hw_shrp->lib[instance],
				scenario);
}

static int is_hw_shrp_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_shrp *hw_shrp = (struct is_hw_shrp *)hw_ip->priv_info;

	return CALL_HW_HELPER_OPS(hw_ip, delete_setfile, instance, &hw_shrp->lib[instance]);
}

int is_hw_shrp_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_shrp *hw_shrp = (struct is_hw_shrp *)hw_ip->priv_info;
	int ret;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	ret = CALL_HWIP_OPS(hw_ip, reset, instance);
	if (ret)
		return ret;

	if (pmio_reinit_cache(hw_ip->pmio, &hw_ip->pmio_config)) {
		pmio_cache_set_bypass(hw_ip->pmio, true);
		err("failed to reinit PMIO cache, set bypass");
		return -EINVAL;
	}

	is_hw_shrp_prepare(hw_ip, instance);

	return CALL_HW_HELPER_OPS(hw_ip, restore, instance, &hw_shrp->lib[instance]);
}

static int is_hw_shrp_set_regs(struct is_hw_ip *hw_ip, u32 chain_id,
	u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size)
{
	return CALL_HW_HELPER_OPS(hw_ip, set_regs, instance, regs, regs_size);
}

int is_hw_shrp_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size, enum is_reg_dump_type dump_type)
{
	struct is_common_dma *dma;
	struct is_hw_shrp *hw_shrp = NULL;
	u32 rdma_max_cnt;
	u32 i;

	switch (dump_type) {
	case IS_REG_DUMP_TO_LOG:
		shrp_hw_dump(get_base(hw_ip));
		break;
	case IS_REG_DUMP_DMA:
		rdma_max_cnt = shrp_hw_g_rdma_max_cnt();
		for (i = 0; i < rdma_max_cnt; i++) {
			hw_shrp = (struct is_hw_shrp *)hw_ip->priv_info;
			dma = &hw_shrp->rdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int is_hw_shrp_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_shrp *hw_shrp;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);
	hw_shrp = (struct is_hw_shrp *)hw_ip->priv_info;
	if (!hw_shrp) {
		mserr_hw("failed to get HW SHRP", instance, hw_ip);
		return -ENODEV;
	}

	shrp_hw_print_chain_debug_cnt(get_base(hw_ip));
	shrp_hw_dump(get_base(hw_ip));

	return CALL_HW_HELPER_OPS(hw_ip, notify_timeout, instance, &hw_shrp->lib[instance]);
}

const struct is_hw_ip_ops is_hw_shrp_ops = {
	.open			= is_hw_shrp_open,
	.init			= is_hw_shrp_init,
	.deinit			= is_hw_shrp_deinit,
	.close			= is_hw_shrp_close,
	.enable			= is_hw_shrp_enable,
	.disable		= is_hw_shrp_disable,
	.shot			= is_hw_shrp_shot,
	.set_param		= is_hw_shrp_set_param,
	.get_meta		= is_hw_shrp_get_meta,
	.frame_ndone		= is_hw_shrp_frame_ndone,
	.load_setfile		= is_hw_shrp_load_setfile,
	.apply_setfile		= is_hw_shrp_apply_setfile,
	.delete_setfile		= is_hw_shrp_delete_setfile,
	.restore		= is_hw_shrp_restore,
	.set_regs		= is_hw_shrp_set_regs,
	.dump_regs		= is_hw_shrp_dump_regs,
	.set_config		= is_hw_shrp_set_config,
	.notify_timeout		= is_hw_shrp_notify_timeout,
	.reset			= is_hw_shrp_reset,
};

int is_hw_shrp_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int hw_slot;
	int ret = 0;

	hw_ip->ops  = &is_hw_shrp_ops;

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &is_hw_shrp_handle_interrupt0;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].handler = &is_hw_shrp_handle_interrupt1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].id = INTR_HWIP2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = true;

	if (IS_ENABLED(SHRP_USE_MMIO))
		return ret;

	hw_ip->mmio_base = hw_ip->regs[REG_SETA];

	hw_ip->pmio_config.name = "shrp";

	hw_ip->pmio_config.mmio_base = hw_ip->regs[REG_SETA];

	hw_ip->pmio_config.cache_type = PMIO_CACHE_NONE;

	shrp_hw_init_pmio_config(&hw_ip->pmio_config);

	hw_ip->pmio = pmio_init(NULL, NULL, &hw_ip->pmio_config);
	if (IS_ERR(hw_ip->pmio)) {
		err("failed to init byrp PMIO: %ld", PTR_ERR(hw_ip->pmio));
		return -ENOMEM;
	}

	ret = pmio_field_bulk_alloc(hw_ip->pmio, &hw_ip->pmio_fields,
				    hw_ip->pmio_config.fields,
				    hw_ip->pmio_config.num_fields);
	if (ret) {
		err("failed to alloc shrp PMIO fields: %d", ret);
		pmio_exit(hw_ip->pmio);
		return ret;

	}

	hw_ip->pmio_reg_seqs = vzalloc(sizeof(struct pmio_reg_seq) * shrp_hw_g_reg_cnt());
	if (!hw_ip->pmio_reg_seqs) {
		err("failed to alloc PMIO multiple write buffer");
		pmio_field_bulk_free(hw_ip->pmio, hw_ip->pmio_fields);
		pmio_exit(hw_ip->pmio);
		return -ENOMEM;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(is_hw_shrp_probe);
