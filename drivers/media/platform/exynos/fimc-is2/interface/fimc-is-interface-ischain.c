/*
 * drivers/media/video/exynos/fimc-is-mc2/interface/fimc-is-interface-ishcain.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *       http://www.samsung.com
 *
 * The header file related to camera
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/workqueue.h>
#include <linux/bug.h>

#include "fimc-is-core.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-groupmgr.h"
#include "fimc-is-interface-ischain.h"
#include "fimc-is-interface.h"
#include "fimc-is-debug.h"
#include "../hardware/api/fimc-is-api-common.h"
#include "../include/fimc-is-hw.h"

u32 __iomem *notify_fcount_sen0;
u32 __iomem *notify_fcount_sen1;
u32 __iomem *notify_fcount_sen2;
u32 __iomem *last_fcount0;
u32 __iomem *last_fcount1;

#define init_request_barrier(itf) mutex_init(&itf->request_barrier)
#define enter_request_barrier(itf) mutex_lock(&itf->request_barrier);
#define exit_request_barrier(itf) mutex_unlock(&itf->request_barrier);
#define init_process_barrier(itf) spin_lock_init(&itf->process_barrier);
#define enter_process_barrier(itf) spin_lock_irq(&itf->process_barrier);
#define exit_process_barrier(itf) spin_unlock_irq(&itf->process_barrier);

extern struct fimc_is_lib_support gPtr_lib_support;

int fimc_is_interface_ischain_probe(struct fimc_is_interface_ischain *this,
	struct fimc_is_hardware *hardware,
	struct fimc_is_resourcemgr *resourcemgr,
	struct platform_device *pdev,
	ulong core_regs)
{
	int ret = 0;
	int hw_slot = -1;
	int intr_slot = -1;
	enum fimc_is_hardware_id hw_id = DEV_HW_END;

	BUG_ON(!this);
	BUG_ON(!resourcemgr);

	this->state = 0;

#if defined(ENABLE_3AAISP)
	hw_id = DEV_HW_3AA0;
	intr_slot = fimc_is_intr_slot_id(hw_id);
	if (!valid_3aaisp_intr_slot_id(intr_slot)) {
		err_itfc("invalid intr_slot (%d)", intr_slot);
		ret = -EINVAL;
		goto p_err;
	}

	this->taaisp[intr_slot].kvaddr = resourcemgr->minfo.kvaddr;
	this->taaisp[intr_slot].dvaddr = resourcemgr->minfo.dvaddr;
	this->taaisp[intr_slot].fw_cookie = resourcemgr->minfo.fw_cookie;
	this->taaisp[intr_slot].alloc_ctx = resourcemgr->mem.alloc_ctx;
	ret = fimc_is_interface_isp_probe(this, intr_slot, hw_id, pdev, core_regs);
	if (ret) {
		err_itfc("interface probe fail (%d,%d)", hw_id, intr_slot);
		ret = -EINVAL;
		goto p_err;
	}
#endif

#if (defined(SOC_30C) && !defined(ENABLE_3AAISP))
	hw_id = DEV_HW_3AA0;
	intr_slot = fimc_is_intr_slot_id(hw_id);
	if (!valid_3aaisp_intr_slot_id(intr_slot)) {
		err_itfc("invalid intr_slot (%d) ", intr_slot);
		ret = -EINVAL;
		goto p_err;
	}

	this->taaisp[intr_slot].kvaddr = resourcemgr->minfo.kvaddr;
	this->taaisp[intr_slot].dvaddr = resourcemgr->minfo.dvaddr;
	this->taaisp[intr_slot].fw_cookie = resourcemgr->minfo.fw_cookie;
	this->taaisp[intr_slot].alloc_ctx = resourcemgr->mem.alloc_ctx;
	ret = fimc_is_interface_isp_probe(this, intr_slot, hw_id, pdev, core_regs);
	if (ret) {
		err_itfc("interface probe fail (%d,%d)", hw_id, intr_slot);
		ret = -EINVAL;
		goto p_err;
	}
#endif

#if (defined(SOC_I0C) && !defined(ENABLE_3AAISP))
	hw_id = DEV_HW_ISP0;
	intr_slot = fimc_is_intr_slot_id(hw_id);
	if (!valid_3aaisp_intr_slot_id(intr_slot)) {
		err_itfc("invalid intr_slot (%d) ", intr_slot);
		ret = -EINVAL;
		goto p_err;
	}

	this->taaisp[intr_slot].kvaddr = resourcemgr->minfo.kvaddr;
	this->taaisp[intr_slot].dvaddr = resourcemgr->minfo.dvaddr;
	this->taaisp[intr_slot].fw_cookie = resourcemgr->minfo.fw_cookie;
	this->taaisp[intr_slot].alloc_ctx = resourcemgr->mem.alloc_ctx;
	ret = fimc_is_interface_isp_probe(this, intr_slot, hw_id, pdev, core_regs);
	if (ret) {
		err_itfc("interface probe fail (%d,%d)", hw_id, intr_slot);
		ret = -EINVAL;
		goto p_err;
	}
#endif

#ifdef SOC_SCP
	hw_id = DEV_HW_MCSC;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d)", hw_slot);
		ret = -EINVAL;
		goto p_err;
	}

	intr_slot = fimc_is_intr_slot_id(hw_id);
	if (!valid_subip_intr_slot_id(intr_slot)) {
		err_itfc("invalid intr_slot (%d) ", intr_slot);
		ret = -EINVAL;
		goto p_err;
	}

	this->subip[intr_slot].hw_ip = &(hardware->hw_ip[hw_slot]);

	ret = fimc_is_interface_scaler_probe(this, intr_slot, hw_id, pdev, core_regs);
	if (ret) {
		err_itfc("interface probe fail (%d,%d)", hw_id, intr_slot);
		ret = -EINVAL;
		goto p_err;
	}
#endif

#ifdef SOC_VRA
	hw_id = DEV_HW_FD;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d)", hw_slot);
		ret = -EINVAL;
		goto p_err;
	}

	intr_slot = fimc_is_intr_slot_id(hw_id);
	if (!valid_subip_intr_slot_id(intr_slot)) {
		err_itfc("invalid intr_slot (%d) ", intr_slot);
		ret = -EINVAL;
		goto p_err;
	}

	this->subip[intr_slot].hw_ip = &(hardware->hw_ip[hw_slot]);

	ret = fimc_is_interface_fd_probe(this, intr_slot, hw_id, pdev, core_regs);
	if (ret) {
		err_itfc("interface probe fail (%d,%d)", hw_id, intr_slot);
		ret = -EINVAL;
		goto p_err;
	}
#endif

	set_bit(IS_CHAIN_IF_STATE_INIT, &this->state);

p_err:
	if (ret)
		err_itfc("probe fail (%d,%d,%d)(%d)", hw_id, hw_slot, intr_slot, ret);

	return ret;
}

static irqreturn_t interface_isp_isr1(int irq, void *data)
{
	struct fimc_is_interface_taaisp *itf_isp = NULL;
	struct isp_intr_handler *intr_isp = NULL;

	itf_isp = (struct fimc_is_interface_taaisp *)data;
	intr_isp = &itf_isp->handler[INTR_3AAISP1];

	if (intr_isp->valid)
		intr_isp->handler(intr_isp->id, intr_isp->ctx);
	else
		err_itfc("[%d]isp(1) empty handler!!", itf_isp->id);

	return IRQ_HANDLED;
}

static irqreturn_t interface_isp_isr2(int irq, void *data)
{
	struct fimc_is_interface_taaisp *itf_isp = NULL;
	struct isp_intr_handler *intr_isp = NULL;

	itf_isp = (struct fimc_is_interface_taaisp *)data;
	intr_isp = &itf_isp->handler[INTR_3AAISP2];

	if (intr_isp->valid)
		intr_isp->handler(intr_isp->id, intr_isp->ctx);
	else
		err_itfc("[%d]isp(2) empty handler!!", itf_isp->id);

	return IRQ_HANDLED;
}

static irqreturn_t interface_isp_isr3(int irq, void *data)
{
	struct fimc_is_interface_taaisp *itf_isp = NULL;
	struct isp_intr_handler *intr_isp = NULL;

	itf_isp = (struct fimc_is_interface_taaisp *)data;
	intr_isp = &itf_isp->handler[INTR_3AAISP3];

	if (intr_isp->valid)
		intr_isp->handler(intr_isp->id, intr_isp->ctx);
	else
		err_itfc("[%d]isp(3) empty handler!!", itf_isp->id);

	return IRQ_HANDLED;
}

static irqreturn_t interface_scaler_isr(int irq, void *data)
{
	struct fimc_is_interface_subip *itf_subip = NULL;

	itf_subip = (struct fimc_is_interface_subip *)data;

	if (itf_subip->handler)
		itf_subip->handler((void *)itf_subip->hw_ip);
	else
		err_itfc("[%d]scaler empty handler!!", itf_subip->id);

	return IRQ_HANDLED;
}

static irqreturn_t interface_fd_isr(int irq, void *data)
{
	struct fimc_is_interface_subip *itf_subip = NULL;

	itf_subip = (struct fimc_is_interface_subip *)data;

	if (itf_subip->handler)
		itf_subip->handler((void *)itf_subip->hw_ip);
	else
		err_itfc("[%d]fd empty handler!!", itf_subip->id);

	return IRQ_HANDLED;
}

int fimc_is_interface_isp_probe(struct fimc_is_interface_ischain *itfc,
	int intr_slot,
	int hw_id,
	struct platform_device *pdev,
	ulong core_regs)
{
	int ret = 0;
	struct fimc_is_interface_taaisp *itf_isp = NULL;

	BUG_ON(!itfc);

	itf_isp = &itfc->taaisp[intr_slot];
	itf_isp->id = intr_slot;
	itf_isp->state = 0;
	itf_isp->regs = 0;
	itf_isp->regs_b = 0;

	ret = fimc_is_hw_get_address(itfc, core_regs, pdev, hw_id, intr_slot);
	if (ret) {
		err_itfc("get_address failed (%d,%d)(%d)", hw_id, intr_slot, ret);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_hw_get_irq(itfc, pdev, hw_id, intr_slot);
	if (ret) {
		err_itfc("get_irq failed (%d,%d)(%d)", hw_id, intr_slot, ret);
		ret = -EINVAL;
		goto p_err;
	}

	snprintf(itf_isp->irq_name[0], sizeof(itf_isp->irq_name[0]), "fimcisp%d-1", itf_isp->id);
	ret = request_irq(itf_isp->irq[0], interface_isp_isr1,
		IRQF_GIC_MULTI_TARGET,
		itf_isp->irq_name[0],
		itf_isp);
	if (ret) {
		err_itfc("request_irq [0] fail (%d,%d)", hw_id, intr_slot);
		ret = -EINVAL;
		goto p_err;
	}

	snprintf(itf_isp->irq_name[1], sizeof(itf_isp->irq_name[1]), "fimcisp%d-2", itf_isp->id);
	ret = request_irq(itf_isp->irq[1], interface_isp_isr2,
		IRQF_GIC_MULTI_TARGET,
		itf_isp->irq_name[1],
		itf_isp);
	if (ret) {
		err_itfc("request_irq [1] fail (%d,%d)", hw_id, intr_slot);
		ret = -EINVAL;
		goto p_err;
	}

	snprintf(itf_isp->irq_name[2], sizeof(itf_isp->irq_name[2]), "fimcisp%d-3", itf_isp->id);
	ret = request_irq(itf_isp->irq[2], interface_isp_isr3,
		IRQF_GIC_MULTI_TARGET,
		itf_isp->irq_name[2],
		itf_isp);
	if (ret) {
		err_itfc("request_irq [2] fail (%d,%d)", hw_id, intr_slot);
		ret = -EINVAL;
		goto p_err;
	}

	itf_isp->handler[INTR_3AAISP1].valid = false;
	itf_isp->handler[INTR_3AAISP2].valid = false;
	itf_isp->handler[INTR_3AAISP3].valid = false;

	set_bit(IS_CHAIN_IF_STATE_INIT, &itf_isp->state);

	/* library data settings */
	gPtr_lib_support.kvaddr    = itf_isp->kvaddr;
	gPtr_lib_support.dvaddr    = itf_isp->dvaddr;
	gPtr_lib_support.fw_cookie = itf_isp->fw_cookie;
	gPtr_lib_support.alloc_ctx = itf_isp->alloc_ctx;
	gPtr_lib_support.pdev = pdev;

	/* TODO: this is not cool */
	gPtr_lib_support.intr_handler_taaisp[itf_isp->id][INTR_3AAISP1]
		= (struct isp_intr_handler *)&itf_isp->handler[INTR_3AAISP1];
	gPtr_lib_support.intr_handler_taaisp[itf_isp->id][INTR_3AAISP2]
		= (struct isp_intr_handler *)&itf_isp->handler[INTR_3AAISP2];
	gPtr_lib_support.intr_handler_taaisp[itf_isp->id][INTR_3AAISP3]
		= (struct isp_intr_handler *)&itf_isp->handler[INTR_3AAISP3];

p_err:
	return ret;
}

int fimc_is_interface_scaler_probe(struct fimc_is_interface_ischain *itfc,
	int intr_slot,
	int hw_id,
	struct platform_device *pdev,
	ulong core_regs)
{
	int ret = 0;
	struct fimc_is_interface_subip *itf_subip = NULL;

	BUG_ON(!itfc);

	itf_subip = &itfc->subip[intr_slot];
	itf_subip->id = intr_slot;
	itf_subip->state = 0;
	itf_subip->handler = NULL;

	ret = fimc_is_hw_get_address(itfc, core_regs, pdev, hw_id, intr_slot);
	if (ret) {
		err_itfc("get_address failed (%d,%d)(%d)", hw_id, intr_slot, ret);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_hw_get_irq(itfc, pdev, hw_id, intr_slot);
	if (ret) {
		err_itfc("get_irq failed (%d,%d)(%d)", hw_id, intr_slot, ret);
		ret = -EINVAL;
		goto p_err;
	}

	snprintf(itf_subip->irq_name, sizeof(itf_subip->irq_name), "fimcsc");
	ret = request_irq(itf_subip->irq, interface_scaler_isr,
		IRQF_GIC_MULTI_TARGET,
		itf_subip->irq_name,
		itf_subip);
	if (ret) {
		err_itfc("request_irq fail (%d,%d)", hw_id, intr_slot);
		ret = -EINVAL;
		goto p_err;
	}

	set_bit(IS_CHAIN_IF_STATE_INIT, &itf_subip->state);

p_err:
	return ret;
}

int fimc_is_interface_fd_probe(struct fimc_is_interface_ischain *itfc,
	int intr_slot,
	int hw_id,
	struct platform_device *pdev,
	ulong core_regs)
{
	int ret = 0;
	struct fimc_is_interface_subip *itf_subip = NULL;

	BUG_ON(!itfc);

	itf_subip = &itfc->subip[intr_slot];
	itf_subip->id = intr_slot;
	itf_subip->state = 0;
	itf_subip->handler = NULL;

	ret = fimc_is_hw_get_address(itfc, core_regs, pdev, hw_id, intr_slot);
	if (ret) {
		err_itfc("get_address failed (%d,%d)(%d)", hw_id, intr_slot, ret);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_hw_get_irq(itfc, pdev, hw_id, intr_slot);
	if (ret) {
		err_itfc("get_irq failed (%d,%d)(%d)", hw_id, intr_slot, ret);
		ret = -EINVAL;
		goto p_err;
	}

	snprintf(itf_subip->irq_name, sizeof(itf_subip->irq_name), "fimcfd");
	ret = request_irq(itf_subip->irq, interface_fd_isr,
		IRQF_GIC_MULTI_TARGET,
		itf_subip->irq_name,
		itf_subip);
	if (ret) {
		err_itfc("request_irq fail (%d,%d)", hw_id, intr_slot);
		ret = -EINVAL;
		goto p_err;
	}

	set_bit(IS_CHAIN_IF_STATE_INIT, &itf_subip->state);
p_err:
	return ret;
}

int print_fre_work_list(struct fimc_is_work_list *this)
{
	struct list_head *temp;
	struct fimc_is_work *work;

	if (!(this->id & TRACE_WORK_ID_MASK))
		return 0;

	printk(KERN_ERR "[INF] fre(%02X, %02d) :",
		this->id, this->work_free_cnt);

	list_for_each(temp, &this->work_free_head) {
		work = list_entry(temp, struct fimc_is_work, list);
		printk(KERN_CONT "%X(%d)->", work->msg.command, work->fcount);
	}

	printk(KERN_CONT "X\n");

	return 0;
}

static int set_free_work(struct fimc_is_work_list *this,
	struct fimc_is_work *work)
{
	int ret = 0;
	unsigned long flags;

	if (work) {
		spin_lock_irqsave(&this->slock_free, flags);

		list_add_tail(&work->list, &this->work_free_head);
		this->work_free_cnt++;
#ifdef TRACE_WORK
		print_fre_work_list(this);
#endif

		spin_unlock_irqrestore(&this->slock_free, flags);
	} else {
		ret = -EFAULT;
		err_itfc("item is null ptr");
	}

	return ret;
}

int print_req_work_list(struct fimc_is_work_list *this)
{
	struct list_head *temp;
	struct fimc_is_work *work;

	if (!(this->id & TRACE_WORK_ID_MASK))
		return 0;

	printk(KERN_ERR "[INF] req(%02X, %02d) :",
		this->id, this->work_request_cnt);

	list_for_each(temp, &this->work_request_head) {
		work = list_entry(temp, struct fimc_is_work, list);
		printk(KERN_CONT "%X(%d)->", work->msg.command, work->fcount);
	}

	printk(KERN_CONT "X\n");

	return 0;
}

static int get_req_work(struct fimc_is_work_list *this,
	struct fimc_is_work **work)
{
	int ret = 0;
	unsigned long flags;

	if (work) {
		spin_lock_irqsave(&this->slock_request, flags);

		if (this->work_request_cnt) {
			*work = container_of(this->work_request_head.next,
					struct fimc_is_work, list);
			list_del(&(*work)->list);
			this->work_request_cnt--;
		} else
			*work = NULL;

		spin_unlock_irqrestore(&this->slock_request, flags);
	} else {
		ret = -EFAULT;
		err_itfc("item is null ptr");
	}

	return ret;
}

static void init_work_list(struct fimc_is_work_list *this, u32 id, u32 count)
{
	u32 i;

	this->id = id;
	this->work_free_cnt	= 0;
	this->work_request_cnt	= 0;
	INIT_LIST_HEAD(&this->work_free_head);
	INIT_LIST_HEAD(&this->work_request_head);
	spin_lock_init(&this->slock_free);
	spin_lock_init(&this->slock_request);
	for (i = 0; i < count; ++i)
		set_free_work(this, &this->work[i]);

	init_waitqueue_head(&this->wait_queue);
}

static inline void wq_func_schedule(struct fimc_is_interface *itf,
	struct work_struct *work_wq)
{
	if (itf->workqueue)
		queue_work(itf->workqueue, work_wq);
	else
		schedule_work(work_wq);
}

static void wq_func_subdev(struct fimc_is_subdev *leader,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *sub_frame,
	u32 fcount, u32 rcount, u32 status)
{
	u32 done_state = VB2_BUF_STATE_DONE;
	u32 findex, mindex;
	struct fimc_is_video_ctx *ldr_vctx, *sub_vctx;
	struct fimc_is_framemgr *ldr_framemgr, *sub_framemgr;
	struct fimc_is_frame *ldr_frame;
	struct camera2_node *capture;

	BUG_ON(!sub_frame);

	ldr_vctx = leader->vctx;
	sub_vctx = subdev->vctx;

	ldr_framemgr = GET_FRAMEMGR(ldr_vctx);
	sub_framemgr = GET_FRAMEMGR(sub_vctx);

	findex = sub_frame->stream->findex;
	mindex = ldr_vctx->queue.buf_maxcount;
	if (findex >= mindex) {
		mserr("findex(%d) is invalid(max %d)", subdev, subdev, findex, mindex);
		done_state = VB2_BUF_STATE_ERROR;
		sub_frame->stream->fvalid = 0;
		goto complete;
	}

	ldr_frame = &ldr_framemgr->frames[findex];
	if (ldr_frame->fcount != fcount) {
		mserr("frame mismatched(ldr%d, sub%d)", subdev, subdev, ldr_frame->fcount, fcount);
		done_state = VB2_BUF_STATE_ERROR;
		sub_frame->stream->fvalid = 0;
		goto complete;
	}

	if (status) {
		msrinfo("[ERR] NDONE(%d, E%X)\n", subdev, subdev, ldr_frame, sub_frame->index, status);
		done_state = VB2_BUF_STATE_ERROR;
		sub_frame->stream->fvalid = 0;
	} else {
#ifdef DBG_STREAMING
		msrinfo(" DONE(%d)\n", subdev, subdev, ldr_frame, sub_frame->index);
#endif
		sub_frame->stream->fvalid = 1;
	}

	capture = &ldr_frame->shot_ext->node_group.capture[subdev->cid];
	if (likely(capture->vid == subdev->vid)) {
		sub_frame->stream->input_crop_region[0] = capture->input.cropRegion[0];
		sub_frame->stream->input_crop_region[1] = capture->input.cropRegion[1];
		sub_frame->stream->input_crop_region[2] = capture->input.cropRegion[2];
		sub_frame->stream->input_crop_region[3] = capture->input.cropRegion[3];
		sub_frame->stream->output_crop_region[0] = capture->output.cropRegion[0];
		sub_frame->stream->output_crop_region[1] = capture->output.cropRegion[1];
		sub_frame->stream->output_crop_region[2] = capture->output.cropRegion[2];
		sub_frame->stream->output_crop_region[3] = capture->output.cropRegion[3];
	} else {
		mserr("capture vid is changed(%d != %d)", subdev, subdev, subdev->vid, capture->vid);
		sub_frame->stream->input_crop_region[0] = 0;
		sub_frame->stream->input_crop_region[1] = 0;
		sub_frame->stream->input_crop_region[2] = 0;
		sub_frame->stream->input_crop_region[3] = 0;
		sub_frame->stream->output_crop_region[0] = 0;
		sub_frame->stream->output_crop_region[1] = 0;
		sub_frame->stream->output_crop_region[2] = 0;
		sub_frame->stream->output_crop_region[3] = 0;
	}

	clear_bit(subdev->id, &ldr_frame->out_flag);

complete:
	clear_bit(REQ_FRAME, &sub_frame->req_flag);
	sub_frame->stream->fcount = fcount;
	sub_frame->stream->rcount = rcount;

	trans_frame(sub_framemgr, sub_frame, FS_COMPLETE);
	buffer_done(sub_vctx, sub_frame->index, done_state);
}

static void wq_func_frame(struct fimc_is_subdev *leader,
	struct fimc_is_subdev *subdev,
	u32 fcount, u32 rcount, u32 status)
{
	unsigned long flags;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;

	BUG_ON(!leader);
	BUG_ON(!subdev);
	BUG_ON(!leader->vctx);
	BUG_ON(!subdev->vctx);
	BUG_ON(!leader->vctx->video);
	BUG_ON(!subdev->vctx->video);

	framemgr = GET_FRAMEMGR(subdev->vctx);

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_4, flags);

	frame = peek_frame(framemgr, FS_PROCESS);
	if (frame && test_bit(REQ_FRAME, &frame->req_flag)) {
		if (!frame->stream) {
			mserr("stream is NULL", subdev, subdev);
			BUG();
		}

		if (unlikely(frame->stream->fcount != fcount)) {
			while (frame) {
				if (fcount == frame->stream->fcount) {
					wq_func_subdev(leader, subdev, frame, fcount, rcount, status);
					break;
				} else if (fcount > frame->stream->fcount) {
					wq_func_subdev(leader, subdev, frame, frame->stream->fcount, rcount, 0xF);

					/* get next subdev frame */
					frame = peek_frame(framemgr, FS_PROCESS);
				} else {
					warn("%d frame done is ignored", frame->stream->fcount);
					break;
				}
			}
		} else {
			wq_func_subdev(leader, subdev, frame, fcount, rcount, status);
		}
	} else {
		mserr("frame done(%p) is occured without request", subdev, subdev, frame);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_4, flags);
}

static void wq_func_30c(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface, work_wq[WORK_30C_FDONE]);

	get_req_work(&itf->work_list[WORK_30C_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= FIMC_IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = &device->txc;
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[WORK_30C_FDONE], work);
		get_req_work(&itf->work_list[WORK_30C_FDONE], &work);
	}
}

static void wq_func_30p(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface, work_wq[WORK_30P_FDONE]);

	get_req_work(&itf->work_list[WORK_30P_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= FIMC_IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = &device->txp;
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[WORK_30P_FDONE], work);
		get_req_work(&itf->work_list[WORK_30P_FDONE], &work);
	}
}

static void wq_func_i0c(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface, work_wq[WORK_I0C_FDONE]);

	get_req_work(&itf->work_list[WORK_I0C_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= FIMC_IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = &device->ixc;
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[WORK_I0C_FDONE], work);
		get_req_work(&itf->work_list[WORK_I0C_FDONE], &work);
	}
}

static void wq_func_i0p(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface, work_wq[WORK_I0P_FDONE]);

	get_req_work(&itf->work_list[WORK_I0P_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= FIMC_IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = &device->ixp;
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[WORK_I0P_FDONE], work);
		get_req_work(&itf->work_list[WORK_I0P_FDONE], &work);
	}
}

static void wq_func_scc(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface, work_wq[WORK_SCC_FDONE]);

	get_req_work(&itf->work_list[WORK_SCC_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= FIMC_IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = &device->scc;
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[WORK_SCC_FDONE], work);
		get_req_work(&itf->work_list[WORK_SCC_FDONE], &work);
	}
}

static void wq_func_scp(struct work_struct *data)
{
	u32 instance, fcount, rcount, status;
	struct fimc_is_interface *itf;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader, *subdev;
	struct fimc_is_work *work;
	struct fimc_is_msg *msg;

	itf = container_of(data, struct fimc_is_interface, work_wq[WORK_SCP_FDONE]);

	get_req_work(&itf->work_list[WORK_SCP_FDONE], &work);
	while (work) {
		msg = &work->msg;
		instance = msg->instance;
		fcount = msg->param1;
		rcount = msg->param2;
		status = msg->param3;

		if (instance >= FIMC_IS_STREAM_COUNT) {
			err("instance is invalid(%d)", instance);
			goto p_err;
		}

		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		if (!device) {
			err("device is NULL");
			goto p_err;
		}

		if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
			merr("device is not open", device);
			goto p_err;
		}

		subdev = &device->scp;
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			merr("subdev is not start", device);
			goto p_err;
		}

		leader = subdev->leader;
		if (!leader) {
			merr("leader is NULL", device);
			goto p_err;
		}

		wq_func_frame(leader, subdev, fcount, rcount, status);

p_err:
		set_free_work(&itf->work_list[WORK_SCP_FDONE], work);
		get_req_work(&itf->work_list[WORK_SCP_FDONE], &work);
	}
}

static void wq_func_group_30s(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_framemgr *framemgr,
	struct fimc_is_frame *frame,
	struct fimc_is_video_ctx *vctx,
	u32 status, u32 lindex, u32 hindex)
{
	u32 done_state = VB2_BUF_STATE_DONE;

	BUG_ON(!vctx);
	BUG_ON(!framemgr);
	BUG_ON(!frame);

	if (status) {
		mgrinfo("[ERR] NDONE(%d, E%X(L%X H%X))\n", group, group, frame, frame->index, status,
			lindex, hindex);
		done_state = VB2_BUF_STATE_ERROR;

		/* specially force set is enabled when perframe control is fail */
		if (lindex || hindex)
			set_bit(FIMC_IS_SUBDEV_FORCE_SET, &group->leader.state);
	}

#ifdef DBG_STREAMING
	if (!status)
		mgrinfo(" DONE(%d)\n", group, group, frame, frame->index);
#endif

	/* 2. leader frame done */
	fimc_is_ischain_meta_invalid(frame);

	frame->result = status;
	trans_frame(framemgr, frame, FS_COMPLETE);
	fimc_is_group_done(groupmgr, group, frame, done_state);
	buffer_done(vctx, frame->index, done_state);
}

static void wq_func_group_31s(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_framemgr *framemgr,
	struct fimc_is_frame *frame,
	struct fimc_is_video_ctx *vctx,
	u32 status, u32 lindex, u32 hindex)
{
	u32 done_state = VB2_BUF_STATE_DONE;

	BUG_ON(!vctx);
	BUG_ON(!framemgr);
	BUG_ON(!frame);

	if (status) {
		mgrinfo("[ERR] NDONE(%d, E%X(L%X H%X))\n", group, group, frame, frame->index, status,
			lindex, hindex);
		done_state = VB2_BUF_STATE_ERROR;

		/* specially force set is enabled when perframe control is fail */
		if (lindex || hindex)
			set_bit(FIMC_IS_SUBDEV_FORCE_SET, &group->leader.state);
	}

#ifdef DBG_STREAMING
	if (!status)
		mgrinfo(" DONE(%d)\n", group, group, frame, frame->index);
#endif

	/* 2. leader frame done */
	fimc_is_ischain_meta_invalid(frame);

	frame->result = status;
	trans_frame(framemgr, frame, FS_COMPLETE);
	fimc_is_group_done(groupmgr, group, frame, done_state);
	buffer_done(vctx, frame->index, done_state);
}

static void wq_func_group_i0s(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_framemgr *framemgr,
	struct fimc_is_frame *frame,
	struct fimc_is_video_ctx *vctx,
	u32 status, u32 lindex, u32 hindex)
{
	u32 done_state = VB2_BUF_STATE_DONE;

	BUG_ON(!framemgr);
	BUG_ON(!frame);

	if (status) {
		mgrinfo("[ERR] NDONE(%d, E%X(L%X H%X))\n", group, group, frame, frame->index, status,
			lindex, hindex);
		done_state = VB2_BUF_STATE_ERROR;

		/* specially force set is enabled when perframe control is fail */
		if (lindex || hindex)
			set_bit(FIMC_IS_SUBDEV_FORCE_SET, &group->leader.state);
	}

#ifdef DBG_STREAMING
	if (!status)
		mgrinfo(" DONE(%d)\n", group, group, frame, frame->index);
#endif

	/* Cache Invalidation */
	fimc_is_ischain_meta_invalid(frame);

	frame->result = status;
	trans_frame(framemgr, frame, FS_COMPLETE);
	fimc_is_group_done(groupmgr, group, frame, done_state);
	buffer_done(vctx, frame->index, done_state);
}

static void wq_func_group_i1s(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_framemgr *framemgr,
	struct fimc_is_frame *frame,
	struct fimc_is_video_ctx *vctx,
	u32 status, u32 lindex, u32 hindex)
{
	u32 done_state = VB2_BUF_STATE_DONE;

	BUG_ON(!framemgr);
	BUG_ON(!frame);

	if (status) {
		mgrinfo("[ERR] NDONE(%d, E%X(L%X H%X))\n", group, group, frame, frame->index, status,
			lindex, hindex);
		done_state = VB2_BUF_STATE_ERROR;

		/* specially force set is enabled when perframe control is fail */
		if (lindex || hindex)
			set_bit(FIMC_IS_SUBDEV_FORCE_SET, &group->leader.state);
	}

#ifdef DBG_STREAMING
	if (!status)
		mgrinfo(" DONE(%d)\n", group, group, frame, frame->index);
#endif

	/* Cache Invalidation */
	fimc_is_ischain_meta_invalid(frame);

	frame->result = status;
	trans_frame(framemgr, frame, FS_COMPLETE);
	fimc_is_group_done(groupmgr, group, frame, done_state);
	buffer_done(vctx, frame->index, done_state);
}

static void wq_func_group_dis(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_framemgr *framemgr,
	struct fimc_is_frame *frame,
	struct fimc_is_video_ctx *vctx,
	u32 status, u32 lindex, u32 hindex)
{
	u32 done_state = VB2_BUF_STATE_DONE;

	BUG_ON(!framemgr);
	BUG_ON(!frame);

	if (status) {
		mgrinfo("[ERR] NDONE(%d, E%X(L%X H%X))\n", group, group, frame, frame->index, status,
			lindex, hindex);
		done_state = VB2_BUF_STATE_ERROR;

		/* specially force set is enabled when perframe control is fail */
		if (lindex || hindex)
			set_bit(FIMC_IS_SUBDEV_FORCE_SET, &group->leader.state);
	}

#ifdef DBG_STREAMING
	if (!status)
		mgrinfo(" DONE(%d)\n", group, group, frame, frame->index);
#endif

	/* Cache Invalidation */
	fimc_is_ischain_meta_invalid(frame);

	trans_frame(framemgr, frame, FS_COMPLETE);
	fimc_is_group_done(groupmgr, group, frame, done_state);
	buffer_done(vctx, frame->index, done_state);
}

void wq_func_group(struct fimc_is_device_ischain *device,
	struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_framemgr *framemgr,
	struct fimc_is_frame *frame,
	struct fimc_is_video_ctx *vctx,
	u32 status, u32 fcount)
{
	u32 lindex = 0;
	u32 hindex = 0;
	void (*wq_func_group_xxx)(struct fimc_is_groupmgr *groupmgr,
		struct fimc_is_group *group,
		struct fimc_is_framemgr *framemgr,
		struct fimc_is_frame *frame,
		struct fimc_is_video_ctx *vctx,
		u32 status, u32 lindex, u32 hindex);

	BUG_ON(!groupmgr);
	BUG_ON(!group);
	BUG_ON(!framemgr);
	BUG_ON(!frame);
	BUG_ON(!vctx);

	/*
	 * complete count should be lower than 3 when
	 * buffer is queued or overflow can be occured
	 */
	if (framemgr->queued_count[FS_COMPLETE] >= DIV_ROUND_UP(framemgr->num_frames, 2))
		mgwarn(" complete bufs : %d", device, group, (framemgr->queued_count[FS_COMPLETE] + 1));

	switch (group->id) {
	case GROUP_ID_3AA0:
		wq_func_group_xxx = wq_func_group_30s;
		break;
	case GROUP_ID_3AA1:
		wq_func_group_xxx = wq_func_group_31s;
		break;
	case GROUP_ID_ISP0:
		wq_func_group_xxx = wq_func_group_i0s;
		break;
	case GROUP_ID_ISP1:
		wq_func_group_xxx = wq_func_group_i1s;
		break;
	case GROUP_ID_DIS0:
		wq_func_group_xxx = wq_func_group_dis;
		break;
	default:
		merr("unresolved group id %d", device, group->id);
		BUG();
	}

	if (status) {
#ifdef CONFIG_ENABLE_HAL3_2_META_INTERFACE
		lindex = frame->shot->ctl.vendor_entry.lowIndexParam;
		lindex &= ~frame->shot->dm.vendor_entry.lowIndexParam;
		hindex = frame->shot->ctl.vendor_entry.highIndexParam;
		hindex &= ~frame->shot->dm.vendor_entry.highIndexParam;
#else
		lindex = frame->shot->ctl.entry.lowIndexParam;
		lindex &= ~frame->shot->dm.entry.lowIndexParam;
		hindex = frame->shot->ctl.entry.highIndexParam;
		hindex &= ~frame->shot->dm.entry.highIndexParam;
#endif
	}

	if (unlikely(fcount != frame->fcount)) {
		if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
			while (frame) {
				if (fcount == frame->fcount) {
					wq_func_group_xxx(groupmgr, group, framemgr, frame, vctx,
						status, lindex, hindex);
					break;
				} else if (fcount > frame->fcount) {
					wq_func_group_xxx(groupmgr, group, framemgr, frame, vctx,
						SHOT_ERR_MISMATCH, lindex, hindex);

					/* get next leader frame */
					frame = peek_frame(framemgr, FS_PROCESS);
				} else {
					warn("%d shot done is ignored", fcount);
					break;
				}
			}
		} else {
			wq_func_group_xxx(groupmgr, group, framemgr, frame, vctx,
				SHOT_ERR_MISMATCH, lindex, hindex);
		}
	} else {
		wq_func_group_xxx(groupmgr, group, framemgr, frame, vctx,
			status, lindex, hindex);
	}
}

static void wq_func_shot(struct work_struct *data)
{
	struct fimc_is_device_ischain *device;
	struct fimc_is_interface *itf;
	struct fimc_is_msg *msg;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;
	struct fimc_is_work_list *work_list;
	struct fimc_is_work *work;
	struct fimc_is_video_ctx *vctx;
	unsigned long flags;
	u32 req_flag;
	u32 fcount, status;
	int instance;
	int group_id;
	struct fimc_is_core *core;

	BUG_ON(!data);

	itf = container_of(data, struct fimc_is_interface, work_wq[INTR_SHOT_DONE]);
	work_list = &itf->work_list[INTR_SHOT_DONE];
	group  = NULL;
	vctx = NULL;
	framemgr = NULL;

	get_req_work(work_list, &work);
	while (work) {
		core = (struct fimc_is_core *)itf->core;
		instance = work->msg.instance;
		group_id = work->msg.group;
		device = &((struct fimc_is_core *)itf->core)->ischain[instance];
		groupmgr = device->groupmgr;

		msg = &work->msg;
		fcount = msg->param1;
		status = msg->param2;

		switch (group_id) {
		case GROUP_ID(GROUP_ID_3AA0):
		case GROUP_ID(GROUP_ID_3AA1):
			req_flag = REQ_3AA_SHOT;
			group = &device->group_3aa;
			break;
		case GROUP_ID(GROUP_ID_ISP0):
		case GROUP_ID(GROUP_ID_ISP1):
			req_flag = REQ_ISP_SHOT;
			group = &device->group_isp;
			break;
		case GROUP_ID(GROUP_ID_DIS0):
			req_flag = REQ_DIS_SHOT;
			group = &device->group_dis;
			break;
		default:
			merr("unresolved group id %d", device, group_id);
			group = NULL;
			vctx = NULL;
			framemgr = NULL;
			goto remain;
		}

		if (!group) {
			merr("group is NULL", device);
			goto remain;
		}

		vctx = group->leader.vctx;
		if (!vctx) {
			merr("vctx is NULL", device);
			goto remain;
		}

		framemgr = GET_FRAMEMGR(vctx);
		if (!framemgr) {
			merr("framemgr is NULL", device);
			goto remain;
		}

		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_5, flags);

		frame = peek_frame(framemgr, FS_PROCESS);

		if (frame) {
			PROGRAM_COUNT(13);
#ifdef MEASURE_TIME
#ifdef EXTERNAL_TIME
			do_gettimeofday(&frame->tzone[TM_SHOT_D]);
#endif
#endif

			clear_bit(req_flag, &frame->req_flag);
			if (frame->req_flag)
				merr("group(%d) req flag is not clear all(%X)",
					device, group->id, (u32)frame->req_flag);

#ifdef ENABLE_CLOCK_GATE
			/* dynamic clock off */
			if (sysfs_debug.en_clk_gate &&
					sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)
				fimc_is_clk_gate_set(core, group->id, false, false, true);
#endif
			wq_func_group(device, groupmgr, group, framemgr, frame,
				vctx, status, fcount);

			PROGRAM_COUNT(14);
		} else {
			mgerr("invalid shot done(%d)", device, group, fcount);
			frame_manager_print_queues(framemgr);
		}

		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_5, flags);
#ifdef ENABLE_CLOCK_GATE
		if (fcount == 1 &&
				sysfs_debug.en_clk_gate &&
				sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)
			fimc_is_clk_gate_lock_set(core, instance, false);
#endif
remain:
		set_free_work(work_list, work);
		get_req_work(work_list, &work);
	}
}

static inline void print_framemgr_spinlock_usage(struct fimc_is_core *core)
{
	u32 i;
	struct fimc_is_device_ischain *ischain;
	struct fimc_is_device_sensor *sensor;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_subdev *subdev;

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; ++i) {
		sensor = &core->sensor[i];
		if (test_bit(FIMC_IS_SENSOR_OPEN, &sensor->state) && (framemgr = GET_SUBDEV_FRAMEMGR(sensor)))
			info("[@] framemgr(0x%08X) sindex : 0x%08lX\n", framemgr->id, framemgr->sindex);
	}

	for (i = 0; i < FIMC_IS_STREAM_COUNT; ++i) {
		ischain = &core->ischain[i];
		if (test_bit(FIMC_IS_ISCHAIN_OPEN, &ischain->state)) {
			/* 3AA GROUP */
			subdev = &ischain->group_3aa.leader;
			if (test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(0x%08X) sindex : 0x%08lX\n", framemgr->id, framemgr->sindex);

			subdev = &ischain->txc;
			if (test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(0x%08X) sindex : 0x%08lX\n", framemgr->id, framemgr->sindex);

			subdev = &ischain->txp;
			if (test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(0x%08X) sindex : 0x%08lX\n", framemgr->id, framemgr->sindex);

			/* ISP GROUP */
			subdev = &ischain->group_isp.leader;
			if (test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(0x%08X) sindex : 0x%08lX\n", framemgr->id, framemgr->sindex);

			subdev = &ischain->ixc;
			if (test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(0x%08X) sindex : 0x%08lX\n", framemgr->id, framemgr->sindex);

			subdev = &ischain->ixp;
			if (test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(0x%08X) sindex : 0x%08lX\n", framemgr->id, framemgr->sindex);

			/* DIS GROUP */
			subdev = &ischain->group_dis.leader;
			if (test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(0x%08X) sindex : 0x%08lX\n", framemgr->id, framemgr->sindex);

			subdev = &ischain->scc;
			if (test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(0x%08X) sindex : 0x%08lX\n", framemgr->id, framemgr->sindex);

			subdev = &ischain->scp;
			if (test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(0x%08X) sindex : 0x%08lX\n", framemgr->id, framemgr->sindex);
		}
	}
}

#define VERSION_OF_NO_NEED_IFLAG 221
int fimc_is_interface_probe(struct fimc_is_interface *this,
	struct fimc_is_minfo *minfo,
	ulong regs,
	u32 irq,
	void *core_data)
{
	int ret = 0;
	struct fimc_is_core *core = (struct fimc_is_core *)core_data;

	dbg_interface("%s\n", __func__);

	init_request_barrier(this);
	init_process_barrier(this);
	init_waitqueue_head(&this->lock_wait_queue);
	init_waitqueue_head(&this->init_wait_queue);
	init_waitqueue_head(&this->idle_wait_queue);
	spin_lock_init(&this->shot_check_lock);

	this->workqueue = alloc_workqueue("fimc-is/highpri", WQ_HIGHPRI, 0);
	if (!this->workqueue)
		probe_warn("failed to alloc own workqueue, will be use global one");

	INIT_WORK(&this->work_wq[WORK_SHOT_DONE], wq_func_shot);
	INIT_WORK(&this->work_wq[WORK_30C_FDONE], wq_func_30c);
	INIT_WORK(&this->work_wq[WORK_30P_FDONE], wq_func_30p);
	INIT_WORK(&this->work_wq[WORK_I0C_FDONE], wq_func_i0c);
	INIT_WORK(&this->work_wq[WORK_I0P_FDONE], wq_func_i0p);
	INIT_WORK(&this->work_wq[WORK_SCC_FDONE], wq_func_scc);
	INIT_WORK(&this->work_wq[WORK_SCP_FDONE], wq_func_scp);

	this->core = (void *)core;

	clear_bit(IS_IF_STATE_OPEN, &this->state);
	clear_bit(IS_IF_STATE_START, &this->state);
	clear_bit(IS_IF_STATE_BUSY, &this->state);
	clear_bit(IS_IF_STATE_READY, &this->state);
	clear_bit(IS_IF_STATE_LOGGING, &this->state);
	clear_bit(IS_IF_RESUME, &this->fw_boot);
	clear_bit(IS_IF_SUSPEND, &this->fw_boot);

	init_work_list(&this->work_list[WORK_SHOT_DONE], TRACE_WORK_ID_SHOT, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_30C_FDONE], TRACE_WORK_ID_30C, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_30P_FDONE], TRACE_WORK_ID_30P, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_I0C_FDONE], TRACE_WORK_ID_I0C, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_I0P_FDONE], TRACE_WORK_ID_I0P, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_SCC_FDONE], TRACE_WORK_ID_SCC, MAX_WORK_COUNT);
	init_work_list(&this->work_list[WORK_SCP_FDONE], TRACE_WORK_ID_SCP, MAX_WORK_COUNT);

	this->err_report_vendor = NULL;

	return ret;
}

int fimc_is_interface_open(struct fimc_is_interface *this)
{
	int i;
	int ret = 0;

	if (test_bit(IS_IF_STATE_OPEN, &this->state)) {
		err("already open");
		ret = -EMFILE;
		goto exit;
	}

	dbg_interface("%s\n", __func__);

	for (i = 0; i < FIMC_IS_STREAM_COUNT; i++) {
		this->streaming[i] = IS_IF_STREAMING_INIT;
		this->processing[i] = IS_IF_PROCESSING_INIT;
	}

	clear_bit(IS_IF_STATE_START, &this->state);
	clear_bit(IS_IF_STATE_BUSY, &this->state);
	clear_bit(IS_IF_STATE_READY, &this->state);
	clear_bit(IS_IF_STATE_LOGGING, &this->state);

	set_bit(IS_IF_STATE_OPEN, &this->state);

exit:
	return ret;
}

int fimc_is_interface_close(struct fimc_is_interface *this)
{
	int ret = 0;

	if (!test_bit(IS_IF_STATE_OPEN, &this->state)) {
		err("already close");
		ret = -EMFILE;
		goto exit;
	}

	dbg_interface("%s\n", __func__);

	clear_bit(IS_IF_STATE_OPEN, &this->state);

exit:
	return ret;
}

void fimc_is_interface_reset(struct fimc_is_interface *this)
{
	return;
}

int fimc_is_hw_logdump(struct fimc_is_interface *this)
{
	return 0;
}

int fimc_is_hw_regdump(struct fimc_is_interface *this)
{
	return 0;
}

int fimc_is_hw_memdump(struct fimc_is_interface *this,
	ulong start,
	ulong end)
{
	return 0;
}

int fimc_is_hw_i2c_lock(struct fimc_is_interface *this,
	u32 instance, int i2c_clk, bool lock)
{
	return 0;
}

void fimc_is_interface_lock(struct fimc_is_interface *this)
{
	return;
}

void fimc_is_interface_unlock(struct fimc_is_interface *this)
{
	return;
}

int fimc_is_hw_g_capability(struct fimc_is_interface *this,
	u32 instance, u32 address)
{
	return 0;
}

int fimc_is_hw_fault(struct fimc_is_interface *this)
{
	return 0;
}