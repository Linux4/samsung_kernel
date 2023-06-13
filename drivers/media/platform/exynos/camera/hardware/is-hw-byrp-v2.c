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

#include "is-hw-byrp-v2.h"
#include "is-err.h"
#include "is-interface-ddk.h"
#include "is-stripe.h"

static inline void *get_base(struct is_hw_ip *hw_ip) {
	if (IS_ENABLED(BYRP_USE_MMIO))
		return hw_ip->regs[REG_SETA];
	return hw_ip->pmio;
}

static int debug_byrp;
module_param(debug_byrp, int, 0644);

static int is_hw_byrp_handle_interrupt0(u32 id, void *context)
{
	struct is_hardware *hardware = NULL;
	struct is_hw_ip *hw_ip = NULL;
	struct is_hw_byrp *hw_byrp = NULL;
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

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;

	status = byrp_hw_g_int0_state(get_base(hw_ip), true,
			(hw_ip->num_buffers & 0xffff), &hw_byrp->irq_state[BYRP_INT0])
			& byrp_hw_g_int0_mask(get_base(hw_ip));

	msdbg_hw(2, "BYRP interrupt status(0x%x)\n", instance, hw_ip, status);

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

	if (byrp_hw_is_occurred(status, INTR_SETTING_DONE))
		CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_SETTING_DONE);

	if (byrp_hw_is_occurred(status, INTR_FRAME_START) && byrp_hw_is_occurred(status, INTR_FRAME_END))
		mswarn_hw("start/end overlapped!! (0x%x)", instance, hw_ip, status);

	if (byrp_hw_is_occurred(status, INTR_FRAME_START)) {
		msdbg_hw(2, "[F:%d]F.S\n", instance, hw_ip, hw_fcount);

		atomic_add((hw_ip->num_buffers & 0xffff), &hw_ip->count.fs);
		CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount,
				DEBUG_POINT_FRAME_START);
		if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
			msdbg_hw(2, "[F:%d]F.S\n", instance, hw_ip, hw_fcount);

		CALL_HW_OPS(hw_ip, frame_start, hw_ip, instance);

		/* byrp_hw_dump(get_base(hw_ip)); */
	}

	if (byrp_hw_is_occurred(status, INTR_FRAME_END)) {
		msdbg_hw(2, "[F:%d]F.E\n", instance, hw_ip, hw_fcount);

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

		/* byrp_hw_dump(get_base(hw_ip)); */
	}

	/* COREX Interrupt */
	/*
	 * if (byrp_hw_is_occurred(status, INTR_COREX_END_0)) {}
	 *
	 * if (byrp_hw_is_occurred(status, INTR_COREX_END_1)) {}
	 */

	f_err = byrp_hw_is_occurred(status, INTR_ERR0);
	if (f_err) {
		msinfo_hw("[F:%d] Ocurred error interrupt0 status(0x%x)\n",
			instance, hw_ip, hw_fcount, status);
		byrp_hw_dump(get_base(hw_ip));
		byrp_hw_int0_error_handle(get_base(hw_ip));
	}

	return 0;
}

static int is_hw_byrp_handle_interrupt1(u32 id, void *context)
{
	struct is_hw_ip *hw_ip;
	struct is_hw_byrp *hw_byrp;
	u32 status, instance, hw_fcount;
	bool f_err;

	hw_ip = (struct is_hw_ip *)context;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);

	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("invalid interrupt, hw_ip state(0x%lx)", instance, hw_ip, hw_ip->state);
		return 0;
	}

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;

	status = byrp_hw_g_int1_state(get_base(hw_ip), true,
			(hw_ip->num_buffers & 0xffff), &hw_byrp->irq_state[BYRP_INT1])
			& byrp_hw_g_int1_mask(get_base(hw_ip));

	f_err = byrp_hw_is_occurred(status, INTR_ERR1);

	if (f_err) {
		msinfo_hw("[F:%d] Ocurred error interrupt1 status(0x%x)\n",
			instance, hw_ip, hw_fcount, status);
		byrp_hw_dump(get_base(hw_ip));
	}

	return 0;
}

static int is_hw_byrp_reset(struct is_hw_ip *hw_ip, u32 instance)
{
	msinfo_hw("reset\n", instance, hw_ip);

	if (byrp_hw_s_reset(get_base(hw_ip))) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}

	return 0;
}

static int is_hw_byrp_wait_idle(struct is_hw_ip *hw_ip, u32 instance)
{
	return byrp_hw_wait_idle(get_base(hw_ip));
}

static void is_hw_byrp_prepare(struct is_hw_ip *hw_ip, u32 instance)
{
	byrp_hw_s_init(get_base(hw_ip), 0);
}

static int is_hw_byrp_finish(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;

	ret = CALL_HWIP_OPS(hw_ip, reset, instance);
	if (ret)
		return ret;

	ret = CALL_HWIP_OPS(hw_ip, wait_idle, instance);
	if (ret)
		mserr_hw("failed to byrp_hw_wait_idle", instance, hw_ip);

	msinfo_hw("final finished byrp\n", instance, hw_ip);

	return ret;
}

static int is_hw_byrp_set_cmdq(struct is_hw_ip *hw_ip, u32 instance, u32 set_id, u32 num_buffers,
	enum cmdq_setting_mode setting_mode, struct c_loader_buffer *clb)
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
	 * ret = byrp_hw_s_corex_update_type(hw_ip->pmio, set_id, COREX_COPY);
	 * if (ret)
	 *	return ret;
	 */

	switch (setting_mode) {
	case CTRL_MODE_DMA_DIRECT:
		byrp_hw_s_cmdq(get_base(hw_ip), set_id, num_buffers, clb->header_dva, clb->num_of_headers);
		break;
	case CTRL_MODE_APB_DIRECT:
		byrp_hw_s_cmdq(get_base(hw_ip), set_id, num_buffers, 0, 0);
		break;
	default:
		merr_hw("unsupport setting mode (%d)", instance,  setting_mode);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int __nocfi is_hw_byrp_open(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_byrp *hw_byrp = NULL;
	struct is_mem *mem;

	FIMC_BUG(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HWBYRP");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw_ip->priv_info = vzalloc(sizeof(struct is_hw_byrp));
	if (!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	hw_byrp->instance = instance;

	hw_byrp->rdma_max_cnt = byrp_hw_g_rdma_max_cnt();
	hw_byrp->wdma_max_cnt = byrp_hw_g_wdma_max_cnt();
	hw_byrp->rdma_param_max_cnt = byrp_hw_g_rdma_cfg_max_cnt();
	hw_byrp->wdma_param_max_cnt = byrp_hw_g_wdma_cfg_max_cnt();
	hw_byrp->rdma = vzalloc(sizeof(struct is_common_dma) * hw_byrp->rdma_max_cnt);
	if (!hw_byrp->rdma) {
		mserr_hw("hw_ip->rdma(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_dma_alloc;
	}
	hw_byrp->wdma = vzalloc(sizeof(struct is_common_dma) * hw_byrp->wdma_max_cnt);
	if (!hw_byrp->wdma) {
		mserr_hw("hw_ip->wdma(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_dma_alloc;
	}

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);

	mem = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_iommu_mem, GROUP_ID_BYRP);

	hw_byrp->pb_c_loader_payload = CALL_PTR_MEMOP(mem,
						alloc,
						mem->priv,
						0x8000,
						NULL,
						0);
	if (IS_ERR_OR_NULL(hw_byrp->pb_c_loader_payload)) {
		err("failed to allocate buffer for c-loader payload");
		hw_byrp->pb_c_loader_payload = NULL;
		return -ENOMEM;
	}
	hw_byrp->kva_c_loader_payload = CALL_BUFOP(hw_byrp->pb_c_loader_payload,
					kvaddr, hw_byrp->pb_c_loader_payload);
	hw_byrp->dva_c_loader_payload = CALL_BUFOP(hw_byrp->pb_c_loader_payload,
					dvaddr, hw_byrp->pb_c_loader_payload);

	hw_byrp->pb_c_loader_header = CALL_PTR_MEMOP(mem, alloc,
					mem->priv, 0x2000,
					NULL, 0);
	if (IS_ERR_OR_NULL(hw_byrp->pb_c_loader_header)) {
		err("failed to allocate buffer for c-loader header");
		hw_byrp->pb_c_loader_header = NULL;
		return -ENOMEM;
	}
	hw_byrp->kva_c_loader_header = CALL_BUFOP(hw_byrp->pb_c_loader_header,
					kvaddr, hw_byrp->pb_c_loader_header);
	hw_byrp->dva_c_loader_header = CALL_BUFOP(hw_byrp->pb_c_loader_header,
					dvaddr, hw_byrp->pb_c_loader_header);

	set_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "open: framemgr[%s]", instance, hw_ip, hw_ip->framemgr->name);

	return 0;

err_dma_alloc:
	if (hw_byrp->rdma)
		vfree(hw_byrp->rdma);
	if (hw_byrp->wdma)
		vfree(hw_byrp->rdma);
	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_byrp_init(struct is_hw_ip *hw_ip, u32 instance,
			bool flag, u32 f_type)
{
	int ret = 0;
	struct is_hw_byrp *hw_byrp = NULL;
	u32 i;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;

	for (i = 0; i < hw_byrp->rdma_max_cnt; i++) {
		ret = byrp_hw_rdma_create(&hw_byrp->rdma[i], get_base(hw_ip), i);
		if (ret) {
			mserr_hw("byrp_hw_rdma_create error[%d]", instance, hw_ip, i);
			return -ENODATA;
		}
	}

	for (i = 0; i < hw_byrp->wdma_max_cnt; i++) {
		ret = byrp_hw_wdma_create(&hw_byrp->wdma[i], get_base(hw_ip), i);
		if (ret) {
			mserr_hw("byrp_hw_wdma_create error[%d]", instance, hw_ip, i);
			return -ENODATA;
		}
	}

	set_bit(HW_INIT, &hw_ip->state);

	return 0;
}

static int is_hw_byrp_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_byrp *hw_byrp = NULL;
	int ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;

	is_hw_byrp_finish(hw_ip, instance);

	vfree(hw_byrp->rdma);
	vfree(hw_byrp->wdma);

	CALL_BUFOP(hw_byrp->pb_c_loader_payload, free, hw_byrp->pb_c_loader_payload);
	CALL_BUFOP(hw_byrp->pb_c_loader_header, free, hw_byrp->pb_c_loader_header);

	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	clear_bit(HW_OPEN, &hw_ip->state);

	return ret;
}

static int is_hw_byrp_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_byrp *hw_byrp = NULL;
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

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;

	ret = CALL_HWIP_OPS(hw_ip, reset, instance);
	if (ret)
		return ret;

	if (!IS_ENABLED(BYRP_USE_MMIO)) {
		hw_ip->pmio_config.cache_type = PMIO_CACHE_FLAT;
		if (pmio_reinit_cache(hw_ip->pmio, &hw_ip->pmio_config)) {
			pmio_cache_set_bypass(hw_ip->pmio, true);
			err("failed to reinit PMIO cache, set bypass");
			return -EINVAL;
		}
	}

	is_hw_byrp_prepare(hw_ip, instance);

	if (!IS_ENABLED(BYRP_USE_MMIO))
		pmio_cache_set_only(hw_ip->pmio, true);

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return ret;
}

static int is_hw_byrp_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_byrp *hw_byrp;
	struct byrp_param_set *param_set;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("byrp_disable: Vvalid(%d)\n", instance, hw_ip,
		atomic_read(&hw_ip->status.Vvalid));

	FIMC_BUG(!hw_ip->priv_info);
	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;

	timetowait = wait_event_timeout(hw_ip->status.wait_queue,
		!atomic_read(&hw_ip->status.Vvalid),
		IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		mserr_hw("wait FRAME_END timeout (%ld)", instance,
			hw_ip, timetowait);
		ret = -ETIME;
	}

	param_set = &hw_byrp->param_set[instance];
	param_set->fcount = 0;

	if (hw_ip->run_rsc_state)
		return 0;

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static int byrp_hw_g_rdma_offset(u32 instance, u32 id, struct param_dma_input *dma_input,
	struct param_stripe_input *stripe_input,
	struct is_crop *dma_crop_cfg,
	u32 *img_offset, u32 *header_offset, struct is_byrp_config *config)
{
	u32 hwformat, memory_bitwidth, pixelsize, sbwc_type;
	u32 sbwc_en;
	u32 comp_64b_align, lossy_byte32num = 0;
	u32 strip_enable;
	u32 full_width;
	u32 start_pos_x;
	u32 image_stride_1p, header_stride_1p;
	u32 crop_x;
	u32 stride_align;

	hwformat = dma_input->format;
	sbwc_type = dma_input->sbwc_type;
	memory_bitwidth = dma_input->bitwidth;
	pixelsize = dma_input->msb + 1;

	*img_offset = 0;
	*header_offset = 0;

	strip_enable = (stripe_input->total_count == 0) ? 0 : 1;
	full_width = (strip_enable) ? stripe_input->full_width : dma_input->width;
	start_pos_x = (strip_enable) ? stripe_input->start_pos_x : 0;
	crop_x = dma_crop_cfg->x + start_pos_x;

	sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	stride_align = config->byrp_bypass ? 16 : 32;

	if (sbwc_en == NONE) {
		image_stride_1p = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize,
								hwformat, full_width,
								stride_align, true);
		*img_offset = (image_stride_1p * dma_crop_cfg->y) +
				is_hw_dma_get_img_stride(memory_bitwidth, pixelsize,
								hwformat, crop_x, 16, true);
	} else if (sbwc_en == COMP || sbwc_en == COMP_LOSS) {
		image_stride_1p = is_hw_dma_get_payload_stride(sbwc_en, pixelsize,
								full_width, comp_64b_align,
								lossy_byte32num,
								BYRP_COMP_BLOCK_WIDTH,
								BYRP_COMP_BLOCK_HEIGHT);
		*img_offset = (image_stride_1p * dma_crop_cfg->y) +
				is_hw_dma_get_payload_stride(sbwc_en, pixelsize, crop_x,
								comp_64b_align,
								lossy_byte32num,
								BYRP_COMP_BLOCK_WIDTH,
								BYRP_COMP_BLOCK_HEIGHT);
	} else {
		merr_hw("sbwc_en(%d) is not valid", instance, sbwc_en);
		return -1;
	}

	if (sbwc_en == COMP) {
		header_stride_1p = is_hw_dma_get_header_stride(full_width,
								BYRP_COMP_BLOCK_WIDTH,
								32);
		*header_offset = (header_stride_1p * dma_crop_cfg->y) +
				is_hw_dma_get_header_stride(crop_x, BYRP_COMP_BLOCK_WIDTH,
								0);
	}

	return 0;
}

static int byrp_hw_g_wdma_offset(u32 instance, u32 id, struct param_dma_output *dma_output,
	struct param_stripe_input *stripe_input,
	struct is_crop *dma_crop_cfg,
	u32 *img_offset, u32 *header_offset)
{
	u32 hwformat, memory_bitwidth, pixelsize;
	u32 strip_enable;
	u32 full_width;
	u32 start_pos_x;
	u32 image_stride_1p;
	u32 crop_x;

	hwformat = dma_output->format;
	memory_bitwidth = dma_output->bitwidth;
	pixelsize = dma_output->msb + 1;

	*img_offset = 0;
	*header_offset = 0;

	strip_enable = (stripe_input->total_count == 0) ? 0 : 1;
	full_width = (strip_enable) ? stripe_input->full_width : dma_output->width;
	start_pos_x = (strip_enable) ? stripe_input->start_pos_x : 0;
	crop_x = dma_crop_cfg->x + start_pos_x + stripe_input->left_margin;

	image_stride_1p = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize,
			hwformat, full_width,
			32, true);
	*img_offset = (image_stride_1p * dma_crop_cfg->y) +
		is_hw_dma_get_img_stride(memory_bitwidth, pixelsize,
				hwformat, crop_x, 16, true);

	return 0;
}

static int __is_hw_byrp_set_rdma(struct is_hw_ip *hw_ip, struct is_hw_byrp *hw_byrp,
	struct byrp_param_set *param_set,
	u32 instance, u32 id, struct is_crop *dma_crop_cfg, u32 set_id)
{
	struct param_dma_input *dma_input;
	dma_addr_t *input_dva;
	u32 cmd;
	u32 comp_sbwc_en, payload_size;
	u32 image_addr_offset;
	u32 header_addr_offset;
	int ret;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_byrp);
	FIMC_BUG(!param_set);

	byrp_hw_g_input_param(param_set, instance, id, &dma_input, &input_dva, &hw_byrp->config);
	if (!input_dva) {
		mserr_hw("failed to get %s address", instance, hw_ip, hw_byrp->rdma[id].name);
		return -EINVAL;
	}
	if (!dma_input) {
		mserr_hw("failed to get %s param", instance, hw_ip, hw_byrp->wdma[id].name);
		return -EINVAL;
	}

	cmd = dma_input->cmd;

	msdbg_hw(2, "[%s] %s cmd: %d\n", instance, hw_ip,
		__func__, hw_byrp->rdma[id].name, cmd);

	byrp_hw_s_rdma_corex_id(&hw_byrp->rdma[id], set_id);

	ret = byrp_hw_s_rdma_init(&hw_byrp->rdma[id],
					dma_input, &param_set->stripe_input,
					cmd, dma_crop_cfg,
					&comp_sbwc_en, &payload_size, &hw_byrp->config);

	if (ret) {
		mserr_hw("failed to initialize BYRP_RDMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (cmd == DMA_INPUT_COMMAND_ENABLE) {
		byrp_hw_g_rdma_offset(instance, id, dma_input, &param_set->stripe_input,
			dma_crop_cfg, &image_addr_offset, &header_addr_offset, &hw_byrp->config);

		ret = byrp_hw_s_rdma_addr(&hw_byrp->rdma[id], input_dva, 0,
						(hw_ip->num_buffers & 0xffff),
						0, comp_sbwc_en, payload_size,
						image_addr_offset,
						header_addr_offset);

		if (ret) {
			mserr_hw("failed to set BYRP_RDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
	}

	return 0;
}

static int __is_hw_byrp_set_wdma(struct is_hw_ip *hw_ip, struct is_hw_byrp *hw_byrp,
	struct byrp_param_set *param_set, u32 instance, u32 id, u32 set_id)
{
	struct param_dma_output *dma_output = NULL;
	dma_addr_t *output_dva = NULL;
	u32 cmd;
	u32 comp_sbwc_en, payload_size;
	u32 out_crop_size_x = 0;
	u32 image_addr_offset;
	u32 header_addr_offset;
	int ret;
	struct is_crop dma_crop_cfg;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_byrp);
	FIMC_BUG(!param_set);

	byrp_hw_g_output_param(param_set, instance, id, &dma_output, &output_dva);
	if (!output_dva) {
		mserr_hw("failed to get %s address", instance, hw_ip, hw_byrp->wdma[id].name);
		return -EINVAL;
	}
	if (!dma_output) {
		mserr_hw("failed to get %s param", instance, hw_ip, hw_byrp->wdma[id].name);
		return -EINVAL;
	}

	cmd = dma_output->cmd;

	msdbg_hw(2, "[%s] %s cmd: %d\n", instance, hw_ip,
		__func__, hw_byrp->wdma[id].name, cmd);

	byrp_hw_s_wdma_corex_id(&hw_byrp->wdma[id], set_id);

	ret = byrp_hw_s_wdma_init(&hw_byrp->wdma[id], param_set,
					get_base(hw_ip), set_id,
					cmd, out_crop_size_x,
					&comp_sbwc_en, &payload_size);

	if (ret) {
		mserr_hw("failed to initialize BYRP_WDMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (cmd == DMA_OUTPUT_COMMAND_ENABLE) {
		dma_crop_cfg.x = 0;
		dma_crop_cfg.y = 0;
		dma_crop_cfg.w = param_set->dma_input.width;
		dma_crop_cfg.h = param_set->dma_input.height;

		byrp_hw_g_wdma_offset(instance, id, dma_output, &param_set->stripe_input,
					&dma_crop_cfg, &image_addr_offset, &header_addr_offset);

		ret = byrp_hw_s_wdma_addr(&hw_byrp->wdma[id], output_dva, 0,
			(hw_ip->num_buffers & 0xffff),
			0, comp_sbwc_en, payload_size, image_addr_offset, header_addr_offset);
		if (ret) {
			mserr_hw("failed to set BYRP_WDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
	}

	return 0;
}

static void __is_hw_byrp_check_size(struct is_hw_ip *hw_ip,
	struct byrp_param_set *param_set, u32 instance)
{
	struct param_dma_input *input = &param_set->dma_input;
	struct param_otf_output *output = &param_set->otf_output;

	FIMC_BUG_VOID(!param_set);

	msdbgs_hw(2, "hw_byrp_check_size >>>\n", instance, hw_ip);
	msdbgs_hw(2, "dma_input: format(%d),crop_size(%dx%d)\n", instance, hw_ip,
		input->format, input->dma_crop_width, input->dma_crop_height);
	msdbgs_hw(2, "otf_output: otf_cmd(%d),format(%d)\n", instance, hw_ip,
		output->cmd, output->format);
	msdbgs_hw(2, "otf_output: pos(%d,%d),crop%dx%d),size(%dx%d)\n", instance, hw_ip,
		output->crop_offset_x, output->crop_offset_y,
		output->crop_width, output->crop_height,
		output->width, output->height);
	msdbgs_hw(2, "<<< hw_byrp_check_size\n", instance, hw_ip);
}

static int __is_hw_byrp_bypass(struct is_hw_ip *hw_ip, u32 set_id)
{
	FIMC_BUG(!hw_ip);

	byrp_hw_s_block_bypass(get_base(hw_ip), set_id);

	return 0;
}

static int __is_hw_byrp_update_block_reg(struct is_hw_ip *hw_ip,
	struct byrp_param_set *param_set, u32 instance, u32 set_id)
{
	struct is_hw_byrp *hw_byrp;
	int ret = 0;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;

	if (hw_byrp->instance != instance) {
		msdbg_hw(2, "update_param: hw_ip->instance(%d)\n",
			instance, hw_ip, hw_byrp->instance);
		hw_byrp->instance = instance;
	}

	__is_hw_byrp_check_size(hw_ip, param_set, instance);

	ret = __is_hw_byrp_bypass(hw_ip, set_id);

	return ret;
}

static void __is_hw_byrp_update_param(struct is_hw_ip *hw_ip, struct is_param_region *p_region,
					struct byrp_param_set *param_set, IS_DECLARE_PMAP(pmap),
					u32 instance)
{
	struct byrp_param *param;

	FIMC_BUG_VOID(!hw_ip);
	FIMC_BUG_VOID(!param_set);

	param = &p_region->byrp;
	param_set->instance_id = instance;
	param_set->mono_mode = hw_ip->region[instance]->parameter.sensor.config.mono_mode;

	byrp_hw_update_param(param, pmap, param_set);
}

static int is_hw_byrp_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
		IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	struct is_hw_byrp *hw_byrp;

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

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	hw_byrp->instance = IS_STREAM_COUNT;

	return 0;
}

static void is_hw_byrp_s_size_regs(struct is_hw_ip *hw_ip, struct byrp_param_set *param_set,
	u32 instance, const struct is_frame *frame,
	struct is_crop *dma_crop_cfg, struct is_crop *bcrop_cfg)
{
	struct is_hw_byrp *hw;
	struct pablo_mmio *base;
	bool use_bcrop_rgb = false;
	struct byrp_grid_cfg grid_cfg;
	struct is_byrp_config *cfg;
	u32 strip_enable, start_pos_x, start_pos_y;
	u32 sensor_full_width, sensor_full_height;
	u32 sensor_binning_x, sensor_binning_y;
	u32 sensor_crop_x, sensor_crop_y;
	u32 byrp_crop_offset_x, byrp_crop_offset_y;
	u32 bns_binning_x, bns_binning_y;

	hw = (struct is_hw_byrp *)hw_ip->priv_info;
	base = get_base(hw_ip);
	cfg = &hw->config;
	if (param_set->dma_output_byr.cmd || param_set->dma_output_byr_processed.cmd)
		use_bcrop_rgb = true;

	if (use_bcrop_rgb) {
		/* In DNG scenario, use BcropRgb in the end of chain */
		dma_crop_cfg->x = 0;
		dma_crop_cfg->y = 0;
		dma_crop_cfg->w = param_set->dma_input.width;
		dma_crop_cfg->h = param_set->dma_input.height;
		bcrop_cfg->x = param_set->dma_input.bayer_crop_offset_x;
		bcrop_cfg->y = param_set->dma_input.bayer_crop_offset_y;
		bcrop_cfg->w = param_set->dma_input.bayer_crop_width;
		bcrop_cfg->h = param_set->dma_input.bayer_crop_height;
	} else {
		/* In Normal scenario, use RDMA crop + BcropByr */
		bcrop_cfg->x = param_set->dma_input.bayer_crop_offset_x % 512;
		bcrop_cfg->y = 0;
		bcrop_cfg->w = param_set->dma_input.bayer_crop_width;
		bcrop_cfg->h = param_set->dma_input.bayer_crop_height;

		dma_crop_cfg->x = param_set->dma_input.bayer_crop_offset_x - bcrop_cfg->x;
		dma_crop_cfg->y = param_set->dma_input.bayer_crop_offset_y;
		dma_crop_cfg->w = ALIGN(param_set->dma_input.bayer_crop_width + bcrop_cfg->x, 512);
		/* when dma crop size is larger than dma input size */
		if (dma_crop_cfg->w > param_set->dma_input.width - dma_crop_cfg->x)
			dma_crop_cfg->w = param_set->dma_input.width - dma_crop_cfg->x;
		dma_crop_cfg->h = param_set->dma_input.bayer_crop_height;
	}

	msdbg_hw(2, "[%s] rdma info w: %d, h: %d (%d, %d, %dx%d)\n",
		instance, hw_ip, __func__,
		param_set->dma_input.width, param_set->dma_input.height,
		param_set->dma_input.bayer_crop_offset_x,
		param_set->dma_input.bayer_crop_offset_y,
		param_set->dma_input.bayer_crop_width,
		param_set->dma_input.bayer_crop_height);
	msdbg_hw(2, "[%s] rdma crop (%d %d, %dx%d), bcrop(%s, %d %d, %dx%d)\n",
		instance, hw_ip, __func__,
		dma_crop_cfg->x, dma_crop_cfg->y, dma_crop_cfg->w, dma_crop_cfg->h,
		use_bcrop_rgb ? "BCROP_RGB" : "BCROP_BYR",
		bcrop_cfg->x, bcrop_cfg->y, bcrop_cfg->w, bcrop_cfg->h);

	/* GRID configuration for CGRAS */
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
		msdbg_hw(2, "%s:[CGR] calculated binning(0,0). Fix to (%d,%d)",
				instance, hw_ip, __func__, 1024, 1024);
	}

	/* Step */
	grid_cfg.step_x = cfg->lsc_step_x;
	grid_cfg.step_y = cfg->lsc_step_y;

	/* Total_crop = unbinned(Stripe_start_position + BYRP_bcrop) + unbinned(sensor_crop) */
	start_pos_x = (strip_enable) ? param_set->stripe_input.start_pos_x : 0;

	if (use_bcrop_rgb) { /* No crop */
		byrp_crop_offset_x = 0;
		byrp_crop_offset_y = 0;
	} else { /* DMA crop + Bayer crop */
		byrp_crop_offset_x = param_set->dma_input.bayer_crop_offset_x;
		byrp_crop_offset_y = param_set->dma_input.bayer_crop_offset_y;
	}

	start_pos_x = (((start_pos_x + byrp_crop_offset_x) * grid_cfg.binning_x) / 1024) +
			(sensor_crop_x * sensor_binning_x) / 1000;
	start_pos_y = ((byrp_crop_offset_y * grid_cfg.binning_y) / 1024) +
			(sensor_crop_y * sensor_binning_y) / 1000;
	grid_cfg.crop_x = start_pos_x * grid_cfg.step_x;
	grid_cfg.crop_y = start_pos_y * grid_cfg.step_y;
	grid_cfg.crop_radial_x = (u16)((-1) * (cfg->sensor_center_x - start_pos_x));
	grid_cfg.crop_radial_y = (u16)((-1) * (cfg->sensor_center_y - start_pos_y));

	byrp_hw_s_grid_cfg(base, &grid_cfg);

	msdbg_hw(2, "%s:[CGR] stripe: enable(%d), pos_x(%d), full_size(%dx%d)\n",
			instance, hw_ip, __func__,
			strip_enable, param_set->stripe_input.start_pos_x,
			param_set->stripe_input.full_width,
			param_set->stripe_input.full_height);

	msdbg_hw(2, "%s:[CGR] dbg: calibrated_size(%dx%d), cal_center(%d,%d), sensor_crop(%d,%d), start_pos(%d,%d)\n",
			instance, hw_ip, __func__,
			sensor_full_width, sensor_full_height,
			cfg->sensor_center_x,
			cfg->sensor_center_y,
			sensor_crop_x, sensor_crop_y,
			start_pos_x, start_pos_y);

	msdbg_hw(2, "%s:[CGR] sfr: binning(%dx%d), step(%dx%d), crop(%d,%d), crop_radial(%d,%d)\n",
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

static int __is_hw_byrp_set_size_regs(struct is_hw_ip *hw_ip, struct byrp_param_set *param_set,
	u32 instance, const struct is_frame *frame, struct is_crop *bcrop_cfg, u32 set_id)
{
	struct is_hw_byrp *hw_byrp;
	struct is_hw_size_config size_config;
	int ret = 0;
	bool use_bcrop_rgb = false;
	bool hdr_en;
	u32 strip_enable;
	u32 rdma_width, rdma_height;
	u32 frame_width;
	u32 region_id;
	u32 bcrop_x, bcrop_y, bcrop_w, bcrop_h;
	u32 pipe_w, pipe_h;
	u32 crop_x, crop_y, crop_w, crop_h;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!param_set);

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;

	strip_enable = (param_set->stripe_input.total_count == 0) ? 0 : 1;

	region_id = param_set->stripe_input.index;
	rdma_width = param_set->dma_input.width;
	rdma_height = param_set->dma_input.height;

	/* Bayer crop, belows are configured for detail cropping */
	if (param_set->dma_output_byr.cmd || param_set->dma_output_byr_processed.cmd)
		use_bcrop_rgb = true;
	hdr_en = param_set->dma_input_hdr.cmd;
	bcrop_x = bcrop_cfg->x;
	bcrop_y = bcrop_cfg->y;
	bcrop_w = bcrop_cfg->w;
	bcrop_h = bcrop_cfg->h;
	frame_width = (strip_enable) ? param_set->stripe_input.full_width : rdma_width;

	/* Belows are for block configuration */
	size_config.sensor_calibrated_width = frame->shot->udm.frame_info.sensor_size[0];
	size_config.sensor_calibrated_height = frame->shot->udm.frame_info.sensor_size[1];
	size_config.sensor_binning_x = frame->shot->udm.frame_info.sensor_binning[0];
	size_config.sensor_binning_y = frame->shot->udm.frame_info.sensor_binning[1];
	size_config.sensor_crop_x = frame->shot->udm.frame_info.sensor_crop_region[0];
	size_config.sensor_crop_y = frame->shot->udm.frame_info.sensor_crop_region[1];
	size_config.sensor_crop_width = frame->shot->udm.frame_info.sensor_crop_region[2];
	size_config.sensor_crop_height = frame->shot->udm.frame_info.sensor_crop_region[3];
	size_config.bcrop_x = bcrop_x;
	size_config.bcrop_y = bcrop_y;
	size_config.bcrop_width = bcrop_w;
	size_config.bcrop_height = bcrop_h;

	if (use_bcrop_rgb) {
		pipe_w = rdma_width;
		pipe_h = rdma_height;
		byrp_hw_s_mpc_size(get_base(hw_ip), set_id, rdma_width, rdma_height);
		byrp_hw_s_bcrop_size(get_base(hw_ip), set_id, BYRP_BCROP_BYR,
					0, 0, rdma_width, rdma_height);
		byrp_hw_s_bcrop_size(get_base(hw_ip), set_id, BYRP_BCROP_RGB,
					bcrop_x, bcrop_y, bcrop_w, bcrop_h);

		if (strip_enable) {
			crop_x = param_set->stripe_input.left_margin;
			crop_y = 0;
			crop_w = rdma_width - param_set->stripe_input.left_margin -
						param_set->stripe_input.right_margin;
			crop_h = rdma_height;

			byrp_hw_s_bcrop_size(get_base(hw_ip), set_id, BYRP_BCROP_ZSL,
					crop_x, crop_y, crop_w, crop_h);
		} else
			byrp_hw_s_bcrop_size(get_base(hw_ip), set_id, BYRP_BCROP_ZSL,
					0, 0, pipe_w, pipe_h);
	} else {
		pipe_w = bcrop_w;
		pipe_h = bcrop_h;
		byrp_hw_s_mpc_size(get_base(hw_ip), set_id, bcrop_w + bcrop_x * 2, pipe_h + bcrop_h * 2);
		byrp_hw_s_bcrop_size(get_base(hw_ip), set_id, BYRP_BCROP_BYR,
					bcrop_x, bcrop_y, bcrop_w, bcrop_h);
		byrp_hw_s_bcrop_size(get_base(hw_ip), set_id, BYRP_BCROP_RGB,
					0, 0, bcrop_w, bcrop_h);
		byrp_hw_s_bcrop_size(get_base(hw_ip), set_id, BYRP_BCROP_ZSL, 0, 0, pipe_w, pipe_h);
	}

	byrp_hw_s_chain_size(get_base(hw_ip), set_id, pipe_w, pipe_h);
	byrp_hw_s_disparity_size(get_base(hw_ip), set_id, &size_config);

	return ret;
}

static int __is_hw_byrp_pmio_config(struct is_hw_ip *hw_ip, u32 instance,
	enum cmdq_setting_mode setting_mode, struct c_loader_buffer* clb)
{
	struct is_hw_byrp *hw_byrp = NULL;

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;

	switch (setting_mode) {
	case CTRL_MODE_DMA_DIRECT:
		pmio_cache_fsync(hw_ip->pmio, (void *)clb, PMIO_FORMATTER_PAIR);

		if (clb->num_of_pairs > 0)
			clb->num_of_headers++;
		/*
		pr_info("header dva: 0x%08x\n", ((dma_addr_t)clb->header_dva) >> 4);
		pr_info("number of headers: %d\n", clb->num_of_headers);
		pr_info("number of pairs: %d\n", clb->num_of_pairs);

		print_hex_dump(KERN_INFO, "header  ", DUMP_PREFIX_OFFSET,
				16, 4, clb->clh, clb->num_of_headers * 16, false);
		print_hex_dump(KERN_INFO, "payload ", DUMP_PREFIX_OFFSET,
				16, 4, clb->clp, clb->num_of_headers * 64, false);
		*/
		break;
	case CTRL_MODE_APB_DIRECT:
		pmio_cache_sync(hw_ip->pmio);
		break;
	default:
		merr_hw("unsupport setting mode (%d)", instance,  setting_mode);
		return -EINVAL;
		break;
	}

	CALL_BUFOP(hw_byrp->pb_c_loader_payload, sync_for_device, hw_byrp->pb_c_loader_payload,
		0, hw_byrp->pb_c_loader_payload->size, DMA_TO_DEVICE);

	CALL_BUFOP(hw_byrp->pb_c_loader_header, sync_for_device, hw_byrp->pb_c_loader_header,
		0, hw_byrp->pb_c_loader_header->size, DMA_TO_DEVICE);

	return 0;
}

static int is_hw_byrp_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	struct is_hw_byrp *hw_byrp = NULL;
	struct is_param_region *param_region;
	struct byrp_param_set *param_set = NULL;
	struct is_frame *dma_frame;
	struct param_dma_input *pdi;
	struct param_dma_output *pdo;
	u32 fcount, instance;
	u32 cur_idx;
	u32 set_id;
	int ret = 0, i = 0, batch_num = 0;
	struct is_crop dma_crop_cfg, bcrop_cfg;
	bool do_blk_cfg;
	u32 strip_index, strip_total_count;
	struct c_loader_buffer clb;
	enum cmdq_setting_mode setting_mode;
	bool skip = false;
	pdma_addr_t *param_set_dva;
	dma_addr_t *dma_frame_dva;
	char *name;

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
	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	param_set = &hw_byrp->param_set[instance];
	param_region = frame->parameter;

	atomic_set(&hw_byrp->strip_index, frame->stripe_info.region_id);
	fcount = frame->fcount;
	dma_frame = frame;

	FIMC_BUG(!frame->shot);

	if (hw_ip->internal_fcount[instance] != 0)
		hw_ip->internal_fcount[instance] = 0;

	__is_hw_byrp_update_param(hw_ip, param_region, param_set, frame->pmap, instance);

	/* DMA settings */
	cur_idx = frame->cur_buf_index;
	setting_mode = CTRL_MODE_DMA_DIRECT;
	name = __getname();
	if (!name) {
		mserr_hw("Failed to get name buffer", instance, hw_ip);
		return -ENOMEM;
	}

	for (i = 0; i < hw_byrp->rdma_param_max_cnt ; i++) {
		if (byrp_hw_g_rdma_param_ptr(i, dma_frame, param_set,
			name, &dma_frame_dva, &pdi, &param_set_dva))
			continue;

		CALL_HW_OPS(hw_ip, dma_cfg, name, hw_ip,
			frame, cur_idx, frame->num_buffers,
			&pdi->cmd, pdi->plane, param_set_dva, dma_frame_dva);
	}

	for (i = 0; i < hw_byrp->wdma_param_max_cnt ; i++) {
		if (byrp_hw_g_wdma_param_ptr(i, dma_frame, param_set,
			name, &dma_frame_dva, &pdo, &param_set_dva))
			continue;

		CALL_HW_OPS(hw_ip, dma_cfg, name, hw_ip,
			frame, cur_idx, frame->num_buffers,
			&pdo->cmd, pdo->plane, param_set_dva, dma_frame_dva);
	}
	__putname(name);

	param_set->instance_id = instance;
	param_set->fcount = fcount;
	/* multi-buffer */
	hw_ip->num_buffers = dma_frame->num_buffers;
	batch_num = hw_ip->framemgr->batch_num;
	/* Update only for SW_FRO scenario */
	if (batch_num > 1 && hw_ip->num_buffers == 1) {
		hw_ip->num_buffers |= batch_num << SW_FRO_NUM_SHIFT;
		hw_ip->num_buffers |= cur_idx << CURR_INDEX_SHIFT;
	} else {
		batch_num = 0;
	}

	strip_index = param_set->stripe_input.index;
	strip_total_count = param_set->stripe_input.total_count;

	if (IS_ENABLED(SKIP_ISP_SHOT_FOR_MULTI_SHOT)) {
		if (!pablo_is_first_shot(batch_num, cur_idx)) skip = true;
		if (!pablo_is_first_shot(strip_total_count, strip_index)) skip = true;
		if (!pablo_is_first_shot(param_set->stripe_input.repeat_num,
			param_set->stripe_input.repeat_idx)) skip = true;
	}

	/* TODO: change to use COREX setA/B */
	set_id = COREX_DIRECT;

	/* reset CLD buffer */
	clb.num_of_headers = 0;
	clb.num_of_values = 0;
	clb.num_of_pairs = 0;

	clb.header_dva = hw_byrp->dva_c_loader_header;
	clb.payload_dva = hw_byrp->dva_c_loader_payload;

	clb.clh = (struct c_loader_header *)hw_byrp->kva_c_loader_header;
	clb.clp = (struct c_loader_payload *)hw_byrp->kva_c_loader_payload;

	byrp_hw_s_clock(get_base(hw_ip), true);

	byrp_hw_s_core(get_base(hw_ip), frame->num_buffers, set_id, param_set);

	do_blk_cfg = CALL_HW_HELPER_OPS(hw_ip, set_rta_regs, instance, set_id, skip, frame, (void *)&clb);
	if (unlikely(do_blk_cfg))
		__is_hw_byrp_update_block_reg(hw_ip, param_set, instance, set_id);

	byrp_hw_s_path(get_base(hw_ip), set_id, param_set, &hw_byrp->config);

	is_hw_byrp_s_size_regs(hw_ip, param_set, instance, frame, &dma_crop_cfg, &bcrop_cfg);

	__is_hw_byrp_set_size_regs(hw_ip, param_set, instance, frame, &bcrop_cfg, set_id);

	for (i = 0; i < hw_byrp->rdma_max_cnt; i++) {
		ret = __is_hw_byrp_set_rdma(hw_ip, hw_byrp, param_set, instance,
						i, &dma_crop_cfg, set_id);
		if (ret) {
			mserr_hw("__is_hw_byrp_set_rdma is fail", instance, hw_ip);
			return ret;
		}
	};

	for (i = 0; i < hw_byrp->wdma_max_cnt; i++) {
		ret = __is_hw_byrp_set_wdma(hw_ip, hw_byrp, param_set, instance,
						i, set_id);
		if (ret) {
			mserr_hw("__is_hw_byrp_set_wdma is fail", instance, hw_ip);
			return ret;
		}
	};

	if (param_region->byrp.control.strgen == CONTROL_COMMAND_START) {
		msdbg_hw(2, "STRGEN input\n", instance, hw_ip);
		byrp_hw_s_strgen(get_base(hw_ip), set_id);
	}

	CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, fcount, DEBUG_POINT_ADD_TO_CMDQ);
	if (!IS_ENABLED(BYRP_USE_MMIO))
		__is_hw_byrp_pmio_config(hw_ip, instance, setting_mode, &clb);

	ret = is_hw_byrp_set_cmdq(hw_ip, instance, set_id, frame->num_buffers,
		setting_mode, &clb);
	if (ret < 0) {
		mserr_hw("is_hw_byrp_set_cmdq is fail", instance, hw_ip);
		return ret;
	}

	byrp_hw_s_clock(get_base(hw_ip), false);

	if (unlikely(debug_byrp))
		byrp_hw_dump(get_base(hw_ip));

	set_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static int is_hw_byrp_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
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

int is_hw_byrp_restore(struct is_hw_ip *hw_ip, u32 instance)
{
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

	is_hw_byrp_prepare(hw_ip, instance);

	return 0;
}

int is_hw_byrp_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size, enum is_reg_dump_type dump_type)
{
	struct is_common_dma *dma;
	struct is_hw_byrp *hw_byrp = NULL;
	u32 i;

	switch (dump_type) {
	case IS_REG_DUMP_TO_LOG:
		byrp_hw_dump(get_base(hw_ip));
		break;
	case IS_REG_DUMP_DMA:
		hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
		for (i = 0; i < hw_byrp->rdma_max_cnt; i++) {
			dma = &hw_byrp->rdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}

		for (i = 0; i < hw_byrp->wdma_max_cnt; i++) {
			dma = &hw_byrp->wdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int is_hw_byrp_set_config(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount, void *config)
{
	struct is_hw_byrp *hw_byrp;
	struct is_byrp_config *conf = (struct is_byrp_config *)config;

	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!conf);

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	hw_byrp->config = *conf;

	if (hw_byrp->config.byrp_bypass)
		msinfo_hw("[F:%d] byrp_bypass is on", instance, hw_ip, fcount);

	return 0;
}

static int is_hw_byrp_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_byrp *hw_byrp;

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	if (!hw_byrp) {
		mserr_hw("failed to get HW BYRP", instance, hw_ip);
		return -ENODEV;
	}
	/* DEBUG */
	byrp_hw_dump(get_base(hw_ip));

	return 0;
}

const struct is_hw_ip_ops is_hw_byrp_ops = {
	.open			= is_hw_byrp_open,
	.init			= is_hw_byrp_init,
	.close			= is_hw_byrp_close,
	.enable			= is_hw_byrp_enable,
	.disable		= is_hw_byrp_disable,
	.shot			= is_hw_byrp_shot,
	.set_param		= is_hw_byrp_set_param,
	.frame_ndone		= is_hw_byrp_frame_ndone,
	.restore		= is_hw_byrp_restore,
	.dump_regs		= is_hw_byrp_dump_regs,
	.set_config		= is_hw_byrp_set_config,
	.notify_timeout		= is_hw_byrp_notify_timeout,
	.reset			= is_hw_byrp_reset,
	.wait_idle		= is_hw_byrp_wait_idle,
};

int is_hw_byrp_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int hw_slot;
	int ret = 0;

	hw_ip->ops = &is_hw_byrp_ops;

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &is_hw_byrp_handle_interrupt0;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].handler = &is_hw_byrp_handle_interrupt1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].id = INTR_HWIP2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = true;

	if (IS_ENABLED(BYRP_USE_MMIO))
		return ret;

	hw_ip->mmio_base = hw_ip->regs[REG_SETA];

	hw_ip->pmio_config.name = "byrp";

	hw_ip->pmio_config.mmio_base = hw_ip->regs[REG_SETA];

	hw_ip->pmio_config.cache_type = PMIO_CACHE_NONE;

	byrp_hw_init_pmio_config(&hw_ip->pmio_config);

	hw_ip->pmio = pmio_init(NULL, NULL, &hw_ip->pmio_config);
	if (IS_ERR(hw_ip->pmio)) {
		err("failed to init byrp PMIO: %ld", PTR_ERR(hw_ip->pmio));
		return -ENOMEM;
	}

	ret = pmio_field_bulk_alloc(hw_ip->pmio, &hw_ip->pmio_fields,
				    hw_ip->pmio_config.fields,
				    hw_ip->pmio_config.num_fields);
	if (ret) {
		err("failed to alloc byrp PMIO fields: %d", ret);
		pmio_exit(hw_ip->pmio);
		return ret;

	}

	hw_ip->pmio_reg_seqs = vzalloc(sizeof(struct pmio_reg_seq) * byrp_hw_g_reg_cnt());
	if (!hw_ip->pmio_reg_seqs) {
		err("failed to alloc PMIO multiple write buffer");
		pmio_field_bulk_free(hw_ip->pmio, hw_ip->pmio_fields);
		pmio_exit(hw_ip->pmio);
		return -ENOMEM;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(is_hw_byrp_probe);
