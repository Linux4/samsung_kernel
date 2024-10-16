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
#include <linux/videodev2.h>
#include <videodev2_exynos_camera.h>

#include "is-core.h"
#include "is-err.h"
#include "is-groupmgr.h"

#include "is-interface.h"
#include "pablo-debug.h"
#include "pablo-work.h"

#include "pablo-interface-irta.h"
#include "pablo-icpu-adapter.h"
#include "is-interface-ischain.h"
#include "is-interface-library.h"
#include "../include/is-hw.h"

static int is_interface_hw_ip_probe(struct is_interface_ischain *itfc,
	int hw_id, int handler_id, struct platform_device *pdev, struct is_hardware *hardware)
{
	struct is_interface_hwip *itf;
	int ret;
	int hw_slot;
	struct is_hw_ip *hw_ip;

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hardware, get_hw_slot_id, hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_itfc("invalid hw_slot (%d) ", hw_slot);
		return -EINVAL;
	}

	hw_ip = &(hardware->hw_ip[hw_slot]);
	itfc->itf_ip[hw_slot].hw_ip = hw_ip;

	itf = &itfc->itf_ip[hw_slot];
	itf->id = hw_id;

	ret = is_hw_get_address(itf, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_address failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = is_hw_get_irq(itf, pdev, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_get_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	ret = is_hw_request_irq(itf, hw_id);
	if (ret) {
		err_itfc("[ID:%2d] hw_request_irq failed (%d)", hw_id, ret);
		return -EINVAL;
	}

	if (handler_id >= 0)
		is_interface_set_irq_handler(hw_ip, handler_id, itf);

	dbg_itfc("[ID:%2d] probe done\n", hw_id);

	return ret;
}

int is_interface_ischain_probe(struct is_interface_ischain *this,
	struct is_hardware *hardware, struct is_resourcemgr *resourcemgr,
	struct platform_device *pdev)
{
	int ret, i;
	struct exynos_platform_is *pdata = pdev->dev.platform_data;
	enum is_hardware_id hw_id = DEV_HW_END;
	enum taaisp_chain_id handler_id;

	this->minfo = is_get_is_minfo();

	is_interface_set_lib_support(this, pdev);

	for (i = 0; i < pdata->num_of_ip.taa; i++) {
		hw_id = DEV_HW_3AA0 + i;
		handler_id = ID_3AA_0 + i;
		ret = is_interface_hw_ip_probe(this, hw_id, handler_id, pdev, hardware);
		if (ret) {
			err_itfc("interface probe fail (hw_id: %d)", hw_id);
			return -EINVAL;
		}
	}

	for (i = 0; i < pdata->num_of_ip.isp; i++) {
		hw_id = DEV_HW_ISP0 + i;
		handler_id = ID_ISP_0 + i;
		ret = is_interface_hw_ip_probe(this, hw_id, handler_id, pdev, hardware);
		if (ret) {
			err_itfc("interface probe fail (hw_id: %d)", hw_id);
			return -EINVAL;
		}
	}

	for (i = 0; i < pdata->num_of_ip.byrp; i++) {
		hw_id = DEV_HW_BYRP + i;
		handler_id = -1;
		ret = is_interface_hw_ip_probe(this, hw_id, handler_id, pdev, hardware);
		if (ret) {
			err_itfc("interface probe fail (hw_id: %d)", hw_id);
			return -EINVAL;
		}
	}

	for (i = 0; i < pdata->num_of_ip.rgbp; i++) {
		hw_id = DEV_HW_RGBP + i;
		handler_id = -1;
		ret = is_interface_hw_ip_probe(this, hw_id, handler_id, pdev, hardware);
		if (ret) {
			err_itfc("interface probe fail (hw_id: %d)", hw_id);
			return -EINVAL;
		}
	}

	for (i = 0; i < pdata->num_of_ip.mcsc; i++) {
		hw_id = DEV_HW_MCSC0 + i;
		handler_id = -1;
		ret = is_interface_hw_ip_probe(this, hw_id, handler_id, pdev, hardware);
		if (ret) {
			err_itfc("interface probe fail (hw_ip: %d)", hw_id);
			return -EINVAL;
		}
	}

	for (i = 0; i < pdata->num_of_ip.vra; i++) {
		hw_id = DEV_HW_VRA + i;
		handler_id = -1;
		ret = is_interface_hw_ip_probe(this, hw_id, handler_id, pdev, hardware);
		if (ret) {
			err_itfc("interface probe fail (hw_ip: %d)", hw_id);
			return -EINVAL;
		}
	}

	for (i = 0; i < pdata->num_of_ip.ypp; i++) {
		hw_id = DEV_HW_YPP + i;
		handler_id = -1;
		ret = is_interface_hw_ip_probe(this, hw_id, handler_id, pdev, hardware);
		if (ret) {
			err_itfc("interface probe fail (hw_ip: %d)", hw_id);
			return -EINVAL;
		}
	}

	for (i = 0; i < pdata->num_of_ip.mcfp; i++) {
		hw_id = DEV_HW_MCFP + i;
		handler_id = -1;
		ret = is_interface_hw_ip_probe(this, hw_id, handler_id, pdev, hardware);
		if (ret) {
			err_itfc("interface probe fail (hw_ip: %d)", hw_id);
			return -EINVAL;
		}
	}

	for (i = 0; i < pdata->num_of_ip.lme; i++) {
		hw_id = DEV_HW_LME0 + i;
		handler_id = ID_LME_0 + i;
		ret = is_interface_hw_ip_probe(this, hw_id, handler_id, pdev, hardware);
		if (ret) {
			err_itfc("interface probe fail (hw_ip: %d)", hw_id);
			return -EINVAL;
		}
	}

	for (i = 0; i < pdata->num_of_ip.orbmch; i++) {
		hw_id = DEV_HW_ORB0 + i;
		handler_id = ID_LME_0 + i;
		ret = is_interface_hw_ip_probe(this, hw_id, handler_id, pdev, hardware);
		if (ret) {
			err_itfc("interface probe fail (hw_ip: %d)", hw_id);
			return -EINVAL;
		}
	}

	dbg_itfc("interface ishchain probe done (hw_id: %d)", hw_id);

	return 0;
}

static void wq_func_group_xxx(struct is_groupmgr *groupmgr,
	struct is_group *group,
	struct is_framemgr *framemgr,
	struct is_frame *frame,
	struct is_video_ctx *vctx,
	u32 status)
{
	u32 done_state = VB2_BUF_STATE_DONE;

	FIMC_BUG_VOID(!vctx);
	FIMC_BUG_VOID(!framemgr);
	FIMC_BUG_VOID(!frame);

	/* Collecting sensor group NDONE status */
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state))
		status = status ? status : frame->result;

	if (status) {
		mgrinfo("[ERR] NDONE(%d, E%d)\n", group, group, frame, frame->index, status);
		done_state = VB2_BUF_STATE_ERROR;
		frame->result = status;

		if (status == IS_SHOT_OVERFLOW) {
#ifdef OVERFLOW_PANIC_ENABLE_ISCHAIN
			panic("G%d overflow", group->id);
#else
			err("G%d overflow", group->id);
#endif
		}
	} else {
		mgrdbgs(1, " DONE(%d)\n", group, group, frame, frame->index);
	}

	frame->repeat_info.idx++;
	if (frame->repeat_info.num) {
		if (frame->repeat_info.idx == frame->repeat_info.num) {
			frame->stripe_info.region_id++;
			frame->repeat_info.idx = 0;
		}
	} else {
		frame->stripe_info.region_id++;
	}
	mgrdbgs(5, "%s : repeat num %d, repeat idx %d, repeat scn %d\n",
		group, group, frame, __func__, frame->repeat_info.num,
		frame->repeat_info.idx, frame->repeat_info.scenario);
	mgrdbgs(5, "%s : strip num %d, strip idx %d\n",	group, group, frame, __func__,
		frame->stripe_info.region_num, frame->stripe_info.region_id);
	clear_bit(group->leader.id, &frame->out_flag);
	is_group_done(groupmgr, group, frame, done_state);

	/**
	 * Skip done when current frame is doing stripe process,
	 * and re-trigger the group shot for next stripe process.
	 */
	if (!status && frame->state == FS_REPEAT_PROCESS)
		return;

	frame->stripe_info.region_id = 0;
	frame->stripe_info.region_num  = 0;
	frame->repeat_info.idx = 0;
	frame->repeat_info.num = 0;

	trans_frame(framemgr, frame, FS_COMPLETE);
	CALL_VOPS(vctx, done, frame->index, done_state);
}

void wq_func_group(struct is_device_ischain *device,
	struct is_groupmgr *groupmgr,
	struct is_group *group,
	struct is_framemgr *framemgr,
	struct is_frame *frame,
	struct is_video_ctx *vctx,
	u32 status, u32 fcount)
{
	FIMC_BUG_VOID(!groupmgr);
	FIMC_BUG_VOID(!group);
	FIMC_BUG_VOID(!framemgr);
	FIMC_BUG_VOID(!frame);
	FIMC_BUG_VOID(!vctx);

	TIME_SHOT(TMS_SDONE);
	/*
	 * complete count should be lower than 3 when
	 * buffer is queued or overflow can be occurred
	 */
	if (framemgr->queued_count[FS_COMPLETE] >= DIV_ROUND_UP(framemgr->num_frames, 2))
		mgwarn(" complete bufs : %d", device, group, (framemgr->queued_count[FS_COMPLETE] + 1));

	if (unlikely(fcount != frame->fcount)) {
		if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			while (frame) {
				if (fcount == frame->fcount) {
					wq_func_group_xxx(groupmgr, group, framemgr,
							frame, vctx, status);
					break;
				} else if (fcount > frame->fcount) {
					wq_func_group_xxx(groupmgr, group, framemgr,
							frame, vctx, SHOT_ERR_MISMATCH);

					/* get next leader frame */
					frame = peek_frame(framemgr, FS_PROCESS);
				} else {
					warn("%d shot done is ignored", fcount);
					break;
				}
			}
		} else {
			wq_func_group_xxx(groupmgr, group, framemgr,
					frame, vctx, SHOT_ERR_MISMATCH);
		}
	} else {
		wq_func_group_xxx(groupmgr, group, framemgr, frame, vctx, status);
	}
}

static void wq_func_shot(struct work_struct *data)
{
	struct is_device_ischain *device;
	struct is_interface *itf;
	struct is_msg *msg;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct is_groupmgr *groupmgr;
	struct is_group *group;
	struct is_group *head;
	struct is_work_list *work_list;
	struct is_work *work;
	struct is_video_ctx *vctx;
	ulong flags;
	u32 fcount, status;
	int instance;
	u64 group_id;
	struct is_core *core;

	FIMC_BUG_VOID(!data);

	itf = container_of(data, struct is_interface, work_wq[WORK_SHOT_DONE]);
	work_list = &itf->work_list[WORK_SHOT_DONE];
	group  = NULL;
	vctx = NULL;
	framemgr = NULL;

	get_req_work(work_list, &work);
	while (work) {
		core = (struct is_core *)itf->core;
		instance = work->msg.instance;
		group_id = work->msg.group;
		device = &((struct is_core *)itf->core)->ischain[instance];
		groupmgr = device->groupmgr;

		msg = &work->msg;
		fcount = msg->param1;
		status = msg->param2;

		switch (group_id) {
		case GROUP_ID(GROUP_ID_PAF0):
		case GROUP_ID(GROUP_ID_PAF1):
		case GROUP_ID(GROUP_ID_PAF2):
		case GROUP_ID(GROUP_ID_PAF3):
			group = &device->group_paf;
			break;
		case GROUP_ID(GROUP_ID_3AA0):
		case GROUP_ID(GROUP_ID_3AA1):
		case GROUP_ID(GROUP_ID_3AA2):
			group = &device->group_3aa;
			break;
		case GROUP_ID(GROUP_ID_LME0):
		case GROUP_ID(GROUP_ID_LME1):
			group = &device->group_lme;
			break;
		case GROUP_ID(GROUP_ID_ORB0):
		case GROUP_ID(GROUP_ID_ORB1):
			group = &device->group_orb;
			break;
		case GROUP_ID(GROUP_ID_ISP0):
		case GROUP_ID(GROUP_ID_ISP1):
			group = &device->group_isp;
			break;
		case GROUP_ID(GROUP_ID_YPP):
			group = &device->group_ypp;
			break;
		case GROUP_ID(GROUP_ID_YUVP):
			group = &device->group_yuvp;
			break;
		case GROUP_ID(GROUP_ID_MCS0):
		case GROUP_ID(GROUP_ID_MCS1):
			group = &device->group_mcs;
			break;
		case GROUP_ID(GROUP_ID_VRA0):
			group = &device->group_vra;
			break;
		case GROUP_ID(GROUP_ID_BYRP):
			group = &device->group_byrp;
			break;
		case GROUP_ID(GROUP_ID_RGBP):
			group = &device->group_rgbp;
			break;
		case GROUP_ID(GROUP_ID_MCFP):
			group = &device->group_mcfp;
			break;
		default:
			merr("unresolved group (%s)", device,
				group_id_name[GROUP_ID(group_id)]);
			group = NULL;
			vctx = NULL;
			framemgr = NULL;
			goto remain;
		}

		head = group->head;
		if (!head) {
			merr("head is NULL", device);
			goto remain;
		}

		vctx = head->leader.vctx;
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

		frame = find_frame(framemgr, FS_REPEAT_PROCESS, frame_fcount, (void *)(ulong)fcount);
		if (!frame)
			frame = peek_frame(framemgr, FS_PROCESS);

		if (frame) {
			/* clear bit for child group */
			if (group != head)
				clear_bit(group->leader.id, &frame->out_flag);
#ifdef MEASURE_TIME
#ifdef EXTERNAL_TIME
			ktime_get_ts64(&frame->tzone[TM_SHOT_D]);
#endif
#endif

			wq_func_group(device, groupmgr, head, framemgr, frame,
				vctx, status, fcount);
		} else {
			mgerr("invalid shot done(%d)", device, head, fcount);
			frame_manager_print_queues(framemgr);
		}

		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_5, flags);

remain:
		set_free_work(work_list, work);
		get_req_work(work_list, &work);
	}
}

static inline void print_framemgr_spinlock_usage(struct is_core *core)
{
	u32 i;
	struct is_device_ischain *ischain;
	struct is_device_sensor *sensor;
	struct is_framemgr *framemgr;
	struct is_subdev *subdev;

	for (i = 0; i < IS_SENSOR_COUNT; ++i) {
		sensor = &core->sensor[i];
		if (test_bit(IS_SENSOR_OPEN, &sensor->state) && (framemgr = &sensor->vctx->queue.framemgr))
			info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);
	}

	for (i = 0; i < IS_STREAM_COUNT; ++i) {
		ischain = &core->ischain[i];
		if (test_bit(IS_ISCHAIN_OPEN, &ischain->state)) {
			/* 3AA GROUP */
			subdev = &ischain->group_3aa.leader;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->txc;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->txp;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->txf;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->txg;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->txo;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->txl;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->orbxc;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			/* ISP GROUP */
			subdev = &ischain->group_isp.leader;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->ixc;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->ixp;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->ixt;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->ixg;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->ixv;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->ixw;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);

			subdev = &ischain->mexc;
			if (test_bit(IS_SUBDEV_OPEN, &subdev->state) && (framemgr = GET_SUBDEV_FRAMEMGR(subdev)))
				info("[@] framemgr(%s) sindex : 0x%08lX\n", framemgr->name, framemgr->sindex);
		}
	}
}

IS_TIMER_FUNC(interface_timer)
{
	u32 shot_count, scount_3ax, scount_isp;
	u32 fcount, i;
	unsigned long flags;
	struct is_interface *itf = from_timer(itf, (struct timer_list *)data, timer);

	struct is_core *core;
	struct is_device_ischain *device;
	struct is_device_sensor *sensor;
	struct is_framemgr *framemgr;
	struct is_work_list *work_list;

	FIMC_BUG_VOID(!itf->core);

	if (!test_bit(IS_IF_STATE_OPEN, &itf->state)) {
		is_info("shot timer is terminated\n");
		return;
	}

	core = itf->core;

	for (i = 0; i < IS_STREAM_COUNT; ++i) {
		device = &core->ischain[i];
		shot_count = 0;
		scount_3ax = 0;
		scount_isp = 0;

		sensor = device->sensor;
		if (!sensor)
			continue;

		if (!test_bit(IS_SENSOR_FRONT_START, &sensor->state))
			continue;

		if (test_bit(IS_ISCHAIN_OPEN_STREAM, &device->state)) {
			spin_lock_irqsave(&itf->shot_check_lock, flags);
			if (atomic_read(&itf->shot_check[i])) {
				atomic_set(&itf->shot_check[i], 0);
				atomic_set(&itf->shot_timeout[i], 0);
				spin_unlock_irqrestore(&itf->shot_check_lock, flags);
				continue;
			}
			spin_unlock_irqrestore(&itf->shot_check_lock, flags);

			if (test_bit(IS_GROUP_START, &device->group_3aa.state)) {
				framemgr = GET_HEAD_GROUP_FRAMEMGR(&device->group_3aa);
				if (framemgr) {
					framemgr_e_barrier_irqs(framemgr, FMGR_IDX_6, flags);
					scount_3ax = framemgr->queued_count[FS_PROCESS];
					shot_count += scount_3ax;
					framemgr_x_barrier_irqr(framemgr, FMGR_IDX_6, flags);
				} else {
					minfo("\n### group_3aa framemgr is null ###\n", device);
				}
			}

			if (test_bit(IS_GROUP_START, &device->group_isp.state)) {
				framemgr = GET_HEAD_GROUP_FRAMEMGR(&device->group_isp);
				if (framemgr) {
					framemgr_e_barrier_irqs(framemgr, FMGR_IDX_7, flags);
					scount_isp = framemgr->queued_count[FS_PROCESS];
					shot_count += scount_isp;
					framemgr_x_barrier_irqr(framemgr, FMGR_IDX_7, flags);
				} else {
					minfo("\n### group_isp framemgr is null ###\n", device);
				}
			}

			if (test_bit(IS_GROUP_START, &device->group_vra.state)) {
				framemgr = GET_HEAD_GROUP_FRAMEMGR(&device->group_vra);
				if (framemgr) {
					framemgr_e_barrier_irqs(framemgr, FMGR_IDX_31, flags);
					shot_count += framemgr->queued_count[FS_PROCESS];
					framemgr_x_barrier_irqr(framemgr, FMGR_IDX_31, flags);
				} else {
					minfo("\n### group_vra framemgr is null ###\n", device);
				}
			}
		}

		if (shot_count) {
			atomic_inc(&itf->shot_timeout[i]);
			minfo("shot timer[%d] is increased to %d\n", device,
				i, atomic_read(&itf->shot_timeout[i]));
		}

		if (atomic_read(&itf->shot_timeout[i]) > TRY_TIMEOUT_COUNT) {
			merr("shot command is timeout(%d, %d(%d+%d))", device,
				atomic_read(&itf->shot_timeout[i]),
				shot_count, scount_3ax, scount_isp);

			minfo("\n### 3ax framemgr info ###\n", device);
			if (scount_3ax) {
				framemgr = GET_HEAD_GROUP_FRAMEMGR(&device->group_3aa);
				if (framemgr) {
					framemgr_e_barrier_irqs(framemgr, 0, flags);
					frame_manager_print_queues(framemgr);
					framemgr_x_barrier_irqr(framemgr, 0, flags);
				} else {
					minfo("\n### 3ax framemgr is null ###\n", device);
				}
			}

			minfo("\n### isp framemgr info ###\n", device);
			if (scount_isp) {
				framemgr = GET_HEAD_GROUP_FRAMEMGR(&device->group_isp);
				if (framemgr) {
					framemgr_e_barrier_irqs(framemgr, 0, flags);
					frame_manager_print_queues(framemgr);
					framemgr_x_barrier_irqr(framemgr, 0, flags);
				} else {
					minfo("\n### isp framemgr is null ###\n", device);
				}
			}

			minfo("\n### work list info ###\n", device);
			work_list = &itf->work_list[WORK_SHOT_DONE];
			print_fre_work_list(work_list);
			print_req_work_list(work_list);

			/* framemgr spinlock check */
			print_framemgr_spinlock_usage(core);

			merr("[@] camera firmware panic!!!", device);
			is_debug_s2d(true, "IS_SHOT_CMD_TIMEOUT");

			return;
		}
	}

	for (i = 0; i < IS_SENSOR_COUNT; ++i) {
		sensor = &core->sensor[i];

		if (!test_bit(IS_SENSOR_BACK_START, &sensor->state)
			|| !test_bit(IS_SENSOR_FRONT_START, &sensor->state)) {
			atomic_set(&itf->sensor_check[i], 0);
			continue;
		}

		fcount = is_sensor_g_fcount(sensor);
		if (fcount == atomic_read(&itf->sensor_check[i])) {
			atomic_inc(&itf->sensor_timeout[i]);
			pr_err ("sensor timer[%d] is increased to %d(fcount : %d)\n", i,
				atomic_read(&itf->sensor_timeout[i]), fcount);
			is_sensor_dump(sensor);
		} else {
			atomic_set(&itf->sensor_timeout[i], 0);
			atomic_set(&itf->sensor_check[i], fcount);
		}

		if (atomic_read(&itf->sensor_timeout[i]) > SENSOR_TIMEOUT_COUNT) {
			merr("sensor is timeout(%d, %d)", sensor,
				atomic_read(&itf->sensor_timeout[i]),
				atomic_read(&itf->sensor_check[i]));

			/* framemgr spinlock check */
			print_framemgr_spinlock_usage(core);

#ifdef SENSOR_PANIC_ENABLE
			/* if panic happened, fw log dump should be happened by panic handler */
			mdelay(2000);
			panic("[@] camera sensor panic!!!");
#else
			if (IS_ENABLED(CLOGGING))
				is_resource_cdump();
			else
				is_resource_dump();
#endif
			return;
		}
	}

	mod_timer(&itf->timer, jiffies + (IS_COMMAND_TIMEOUT/TRY_TIMEOUT_COUNT));
}

int is_interface_probe(struct is_interface *this,
	void *core_data)
{
	int ret;

	is_load_ctrl_init();
	dbg_interface(1, "%s\n", __func__);

	spin_lock_init(&this->shot_check_lock);

	this->workqueue = alloc_workqueue("is/[H/U]", WQ_HIGHPRI | WQ_UNBOUND, 0);
	if (!this->workqueue)
		probe_warn("failed to alloc own workqueue, will be use global one");

	INIT_WORK(&this->work_wq[WORK_SHOT_DONE], wq_func_shot);
	init_work_list(&this->work_list[WORK_SHOT_DONE], WORK_SHOT_DONE, MAX_WORK_COUNT);

	this->core = core_data;

	clear_bit(IS_IF_STATE_OPEN, &this->state);
	clear_bit(IS_IF_STATE_START, &this->state);
	clear_bit(IS_IF_STATE_BUSY, &this->state);
	clear_bit(IS_IF_STATE_READY, &this->state);
	clear_bit(IS_IF_STATE_LOGGING, &this->state);

	ret = pablo_interface_irta_probe();
	if (ret) {
		probe_err("pablo_interface_irta_probe is fail(%d)", ret);
		return ret;
	}

	pablo_icpu_adt_probe();

	return ret;
}

int is_interface_open(struct is_interface *this)
{
	int i;
	int ret = 0;

	if (test_bit(IS_IF_STATE_OPEN, &this->state)) {
		err("already open");
		ret = -EMFILE;
		goto exit;
	}

	dbg_interface(1, "%s\n", __func__);

	for (i = 0; i < IS_STREAM_COUNT; i++) {
		this->streaming[i] = IS_IF_STREAMING_INIT;
		this->processing[i] = IS_IF_PROCESSING_INIT;
		atomic_set(&this->shot_check[i], 0);
		atomic_set(&this->shot_timeout[i], 0);
	}
	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		atomic_set(&this->sensor_check[i], 0);
		atomic_set(&this->sensor_timeout[i], 0);
	}

	clear_bit(IS_IF_STATE_START, &this->state);
	clear_bit(IS_IF_STATE_BUSY, &this->state);
	clear_bit(IS_IF_STATE_READY, &this->state);
	clear_bit(IS_IF_STATE_LOGGING, &this->state);

	timer_setup(&this->timer, (void (*)(struct timer_list *))interface_timer, 0);
	this->timer.expires = jiffies + (IS_COMMAND_TIMEOUT/TRY_TIMEOUT_COUNT);
	add_timer(&this->timer);

	set_bit(IS_IF_STATE_OPEN, &this->state);

exit:
	return ret;
}

int is_interface_close(struct is_interface *this)
{
	int ret = 0;

	if (!test_bit(IS_IF_STATE_OPEN, &this->state)) {
		err("already close");
		ret = -EMFILE;
		goto exit;
	}

	del_timer_sync(&this->timer);

	dbg_interface(1, "%s\n", __func__);

	clear_bit(IS_IF_STATE_OPEN, &this->state);

exit:
	return ret;
}
