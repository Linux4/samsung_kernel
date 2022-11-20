/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>

#include "fimc-is-err.h"
#include "fimc-is-core.h"
#include "fimc-is-hw-control.h"

#include "fimc-is-api-isp.h"
#include "fimc-is-hw-3aa.h"
#include "fimc-is-hw-isp.h"
#include "fimc-is-hw-mcscaler.h"
#include "fimc-is-hw-fd.h"
#include "fimc-is-hw-dm.h"

#define INTERNAL_SHOT_EXIST	(1)
static bool enable_debug_info = false;

static int get_free_work_irq(struct fimc_is_work_list *this,
	struct fimc_is_work **work)
{
	int ret = 0;

	if (work) {
		spin_lock(&this->slock_free);

		if (this->work_free_cnt) {
			*work = container_of(this->work_free_head.next,
					struct fimc_is_work, list);
			list_del(&(*work)->list);
			this->work_free_cnt--;
		} else
			*work = NULL;

		spin_unlock(&this->slock_free);
	} else {
		ret = -EFAULT;
		err_hw("item is null ptr");
	}

	return ret;
}

static int set_req_work_irq(struct fimc_is_work_list *this,
	struct fimc_is_work *work)
{
	int ret = 0;

	if (work) {
		spin_lock(&this->slock_request);
		list_add_tail(&work->list, &this->work_request_head);
		this->work_request_cnt++;
#ifdef TRACE_WORK
		print_req_work_list(this);
#endif

		spin_unlock(&this->slock_request);
	} else {
		ret = -EFAULT;
		err_hw("item is null ptr");
	}

	return ret;
}

static inline void wq_func_schedule(struct fimc_is_interface *itf,
	struct work_struct *work_wq)
{
	if (itf->workqueue)
		queue_work(itf->workqueue, work_wq);
	else
		schedule_work(work_wq);
}

void print_hw_interrupt_time(struct fimc_is_hw_ip *hw_ip)
{
	int hw_slot = -1;
	struct fimc_is_hw_ip *_hw_ip = NULL;
	struct fimc_is_hardware *hardware;
	unsigned long usec_fs, usec_fe, usec_dma;

	BUG_ON(!hw_ip);

	hardware = hw_ip->hardware;

	for (hw_slot = 0; hw_slot < HW_SLOT_MAX; hw_slot++) {
		_hw_ip = &hardware->hw_ip[hw_slot];
		usec_fs  = do_div(_hw_ip->fs_time,  NSEC_PER_SEC);
		usec_fe  = do_div(_hw_ip->fe_time,  NSEC_PER_SEC);
		usec_dma = do_div(_hw_ip->dma_time, NSEC_PER_SEC);

		err_hw("time[ID:%d]fs[%5lu.%06lu], fe[%5lu.%06lu], dma[%5lu.%06lu]",
			_hw_ip->id,
			(unsigned long)_hw_ip->fs_time,  usec_fs  / NSEC_PER_USEC,
			(unsigned long)_hw_ip->fe_time,  usec_fe  / NSEC_PER_USEC,
			(unsigned long)_hw_ip->dma_time, usec_dma / NSEC_PER_USEC);
	}
}

void print_hw_frame_count(struct fimc_is_hw_ip *hw_ip)
{
	int hw_slot = -1;
	struct fimc_is_hw_ip *_hw_ip = NULL;
	struct fimc_is_hardware *hardware;

	BUG_ON(!hw_ip);

	hardware = hw_ip->hardware;

	for (hw_slot = 0; hw_slot < HW_SLOT_MAX; hw_slot++) {
		_hw_ip = &hardware->hw_ip[hw_slot];
		err_hw("count[ID:%d]fs(%d), fe(%d), cl(%d)",
			_hw_ip->id,
			atomic_read(&_hw_ip->fs_count),
			atomic_read(&_hw_ip->fe_count),
			atomic_read(&_hw_ip->cl_count));
	}
}

int fimc_is_hardware_probe(struct fimc_is_hardware *hardware,
	struct fimc_is_interface *itf,
	struct fimc_is_interface_ischain *itfc)
{
	int ret = 0;
	int i = 0;
	int hw_slot = -1;
	enum fimc_is_hardware_id hw_id = DEV_HW_END;

	BUG_ON(!hardware);
	BUG_ON(!itf);
	BUG_ON(!itfc);

	for (i = 0; i < HW_SLOT_MAX; i++)
		hardware->hw_ip[i].id = DEV_HW_END;

#if defined(ENABLE_3AAISP)
	hw_id = DEV_HW_3AA0;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_hw("invalid slot (%d,%d)", hw_id, hw_slot);
		ret = -EINVAL;
		goto p_err;
	}
	ret = fimc_is_hw_3aa_probe(&(hardware->hw_ip[hw_slot]), itf, itfc, hw_id);
	if (ret) {
		err_hw("probe fail (%d,%d)", hw_id, hw_slot);
		goto p_err;
	}
#endif

#if (defined(SOC_30C) && !defined(ENABLE_3AAISP))
	hw_id = DEV_HW_3AA0;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_hw("invalid slot (%d,%d)", hw_id, hw_slot);
		ret = -EINVAL;
		goto p_err;
	}
	ret = fimc_is_hw_3aa_probe(&(hardware->hw_ip[hw_slot]), itf, itfc, hw_id);
	if (ret) {
		err_hw("probe fail (%d,%d)", hw_id, hw_slot);
		goto p_err;
	}
#endif

#if (defined(SOC_31C) && !defined(ENABLE_3AAISP))
	hw_id = DEV_HW_3AA1;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_hw("invalid slot (%d,%d)", hw_id, hw_slot);
		ret = -EINVAL;
		goto p_err;
	}
	ret = fimc_is_hw_3aa_probe(&(hardware->hw_ip[hw_slot]), itf, itfc, hw_id);
	if (ret) {
		err_hw("probe fail (%d,%d)", hw_id, hw_slot);
		goto p_err;
	}
#endif

#if (defined(SOC_I0C) && !defined(ENABLE_3AAISP))
	hw_id = DEV_HW_ISP0;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_hw("invalid slot (%d,%d)", hw_id, hw_slot);
		ret = -EINVAL;
		goto p_err;
	}
	ret = fimc_is_hw_isp_probe(&(hardware->hw_ip[hw_slot]), itf, itfc, hw_id);
	if (ret) {
		err_hw("probe fail (%d,%d)", hw_id, hw_slot);
		goto p_err;
	}
#endif

#if (defined(SOC_I1C) && !defined(ENABLE_3AAISP))
	hw_id = DEV_HW_ISP1;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_hw("invalid slot (%d,%d)", hw_id, hw_slot);
		ret = -EINVAL;
		goto p_err;
	}
	ret = fimc_is_hw_isp_probe(&(hardware->hw_ip[hw_slot]), itf, itfc, hw_id);
	if (ret) {
		err_hw("probe fail (%d,%d)", hw_id, hw_slot);
		goto p_err;
	}
#endif

#ifdef SOC_SCP
	hw_id = DEV_HW_MCSC;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_hw("invalid slot (%d,%d)", hw_id, hw_slot);
		ret = -EINVAL;
		goto p_err;
	}
	ret = fimc_is_hw_mcsc_probe(&(hardware->hw_ip[hw_slot]), itf, itfc, hw_id);
	if (ret) {
		err_hw("probe fail (%d,%d)", hw_id, hw_slot);
		goto p_err;
	}
#endif

#ifdef SOC_VRA
	hw_id = DEV_HW_FD;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_hw("invalid slot (%d,%d)", hw_id, hw_slot);
		ret = -EINVAL;
		goto p_err;
	}
	ret = fimc_is_hw_fd_probe(&(hardware->hw_ip[hw_slot]), itf, itfc, hw_id);
	if (ret) {
		err_hw("probe fail (%d,%d)", hw_id, hw_slot);
		goto p_err;
	}
#endif

	for (i = 0; i < FIMC_IS_MAX_NODES; i++)
		hardware->hw_path[i] = 0;

	atomic_set(&hardware->stream_on, 0);

p_err:
	return ret;
}

int fimc_is_hardware_set_param(struct fimc_is_hardware *hardware,
	u32 instance,
	struct is_region *region,
	u32 lindex,
	u32 hindex,
	unsigned long hw_path)
{
	int ret = 0;
	int hw_slot = -1;
	struct fimc_is_hw_ip *hw_ip = NULL;

	BUG_ON(!hardware);
	BUG_ON(!region);

	for (hw_slot = 0; hw_slot < HW_SLOT_MAX; hw_slot++) {
		hw_ip = &hardware->hw_ip[hw_slot];
		ret = CALL_HW_OPS(hw_ip, set_param, region, lindex, hindex, instance, hw_path);
		if (ret) {
			err_hw("[%d]set_param fail (%d,%d)", instance, hw_ip->id, hw_slot);
			ret = -EINVAL;
			goto p_err;
		}
	}

	dbg_hw("[%d]set_param [P:0x%lx]\n", instance, hw_path);

p_err:
	return ret;
}

int fimc_is_hardware_shot(struct fimc_is_hardware *hardware,
	u32 instance,
	struct fimc_is_group *group,
	struct fimc_is_frame *frame,
	struct fimc_is_framemgr *framemgr,
	unsigned long hw_path,
	u32 framenum)
{
	int ret = 0;
	int hw_slot = -1;
	struct fimc_is_hw_ip *hw_ip = NULL;
	enum fimc_is_hardware_id hw_id = DEV_HW_END;
	struct fimc_is_group *child;
	unsigned long flags;

	BUG_ON(!hardware);
	BUG_ON(!frame);

	framemgr_e_barrier_irqs(framemgr, 0, flags);
	put_frame(framemgr, frame, FS_PROCESS);
	framemgr_x_barrier_irqr(framemgr, 0, flags);

	child = group;
	while (child->child)
		child = child->child;

	while (child) {
		switch (child->id) {
		case GROUP_ID_3AA0:
			hw_id = DEV_HW_3AA0;
			hw_slot = fimc_is_hw_slot_id(hw_id);
			if (valid_hw_slot_id(hw_slot)) {
				hw_ip = &(hardware->hw_ip[hw_slot]);
				hw_ip->fcount = framenum;
				ret = CALL_HW_OPS(hw_ip, shot, frame, hw_path);
				if (ret) {
					err_hw("[%d]shot fail (%d,%d)", instance, hw_id, hw_slot);
					ret = -EINVAL;
					goto p_err;
				}
			}
			break;
		case GROUP_ID_3AA1:
			hw_id = DEV_HW_3AA1;
			hw_slot = fimc_is_hw_slot_id(hw_id);
			if (valid_hw_slot_id(hw_slot)) {
				hw_ip = &(hardware->hw_ip[hw_slot]);
				hw_ip->fcount = framenum;
				ret = CALL_HW_OPS(hw_ip, shot, frame, hw_path);
				if (ret) {
					err_hw("[%d]shot fail (%d,%d)", instance, hw_id, hw_slot);
					ret = -EINVAL;
					goto p_err;
				}
			}
			break;
		case GROUP_ID_ISP0:
			hw_id = DEV_HW_FD;
			hw_slot = fimc_is_hw_slot_id(hw_id);
			if (valid_hw_slot_id(hw_slot)) {
				hw_ip = &(hardware->hw_ip[hw_slot]);
				hw_ip->fcount = framenum;
				ret = CALL_HW_OPS(hw_ip, shot, frame, hw_path);
				if (ret) {
					err_hw("[%d]shot fail (%d,%d)", instance, hw_id, hw_slot);
					ret = -EINVAL;
					goto p_err;
				}
			}

			hw_id = DEV_HW_MCSC;
			hw_slot = fimc_is_hw_slot_id(hw_id);
			if (valid_hw_slot_id(hw_slot)) {
				hw_ip = &(hardware->hw_ip[hw_slot]);
				hw_ip->fcount = framenum;
				ret = CALL_HW_OPS(hw_ip, shot, frame, hw_path);
				if (ret) {
					err_hw("[%d]shot fail (%d,%d)", instance, hw_id, hw_slot);
					ret = -EINVAL;
					goto p_err;
				}
			}

			hw_id = DEV_HW_ISP0;
			hw_slot = fimc_is_hw_slot_id(hw_id);
			if (valid_hw_slot_id(hw_slot)) {
				hw_ip = &(hardware->hw_ip[hw_slot]);
				hw_ip->fcount = framenum;
				ret = CALL_HW_OPS(hw_ip, shot, frame, hw_path);
				if (ret) {
					err_hw("[%d]shot fail (%d,%d)", instance, hw_id, hw_slot);
					ret = -EINVAL;
					goto p_err;
				}
			}
			break;
		default:
			err_hw("[%d]invalid group (%d)", instance, group->id);
			ret = -EINVAL;
			goto p_err;
			break;
		}

		child = child->parent;
	}

	/* save for SHOT_TYPE_INTERNAL */
	hardware->hw_path[instance] = hw_path;

	if (!atomic_read(&hardware->stream_on)
		&& (hw_ip && atomic_read(&hw_ip->otf_start)))
			info_hw("[%d]shot [F:%d][G:0x%x][R:0x%lx][O:0x%lx][C:0x%lx][HWF:%d]\n",
				instance, frame->fcount, GROUP_ID(group->id),
				frame->req_flag, frame->out_flag, frame->core_flag, framenum);

p_err:

	return ret;
}

int fimc_is_hardware_get_meta(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_frame *frame,
	u32 instance,
	unsigned long hw_path)
{
	int ret = 0;

	BUG_ON(!hw_ip);

	switch (hw_ip->id) {
	case DEV_HW_3AA0:
	case DEV_HW_3AA1:
	case DEV_HW_ISP0:
		copy_ctrl_to_dm(frame->shot);

		ret = CALL_HW_OPS(hw_ip, get_meta, frame, hw_path);
		if (ret) {
			err_hw("[%d]get_meta fail (%d)", instance, hw_ip->id);
			goto exit;
		}
		break;
	default:
		goto exit;
		break;
	}

	dbg_hw("[%d]get_meta [ID:%d][G:0x%x]\n",
		instance, hw_ip->id, GROUP_ID(hw_ip->group[instance]->id));

exit:
	return ret;
}

static int frame_fcount(struct fimc_is_frame *frame, void *data)
{
	return frame->fcount - (u32)data;
}

int check_shot_exist(struct fimc_is_framemgr *framemgr, u32 fcount)
{
	int ret = 0;
	struct fimc_is_frame *frame;

	if (framemgr->queued_count[FS_COMPLETE]) {
		if (enable_debug_info == true) {
			frame = peek_frame(framemgr, FS_COMPLETE);
			if (frame) {
				info_hw("check_shot_exist: FS_COMPLETE(%d)(%d)(%d)\n",
					framemgr->queued_count[FS_COMPLETE], fcount, frame->fcount);
			}
		}

		frame = find_frame(framemgr, FS_COMPLETE, frame_fcount,
					(void *)fcount);
		if (frame) {
			info_hw("[F:%d]is in complete_list\n", fcount);
			ret = INTERNAL_SHOT_EXIST;
			goto exit;
		}
	}

	if (framemgr->queued_count[FS_PROCESS]) {
		if (enable_debug_info == true) {
			frame = peek_frame(framemgr, FS_PROCESS);
			if (frame) {
				info_hw("check_shot_exist: FS_PROCESS(%d)(%d)(%d)\n",
					framemgr->queued_count[FS_PROCESS], fcount, frame->fcount);
			}
		}

		frame = find_frame(framemgr, FS_PROCESS, frame_fcount,
					(void *)fcount);
		if (frame) {
			info_hw("[F:%d]is in process_list\n", fcount);
			ret = INTERNAL_SHOT_EXIST;
			goto exit;
		}
	}
exit:
	return ret;
}

int fimc_is_hardware_grp_shot(struct fimc_is_hardware *hardware,
	u32 instance,
	struct fimc_is_group *group,
	struct fimc_is_frame *frame,
	unsigned long hw_path)
{
	int ret = 0;
	int i = 0;
	int hw_slot = -1;
	struct fimc_is_hw_ip *hw_ip = NULL;
	enum fimc_is_hardware_id hw_id = DEV_HW_END;
	struct fimc_is_frame *hw_frame;
	struct fimc_is_framemgr *framemgr;
	unsigned long flags;

	BUG_ON(!hardware);
	BUG_ON(!frame);

	switch (group->id) {
	case GROUP_ID_3AA0:
		hw_id = DEV_HW_3AA0;
		break;
	case GROUP_ID_3AA1:
		hw_id = DEV_HW_3AA1;
		break;
	case GROUP_ID_ISP0:
		hw_id = DEV_HW_ISP0;
		break;
	default:
		hw_id = DEV_HW_END;
		err_hw("[%d]invalid group (%d)", instance, group->id);
		goto p_err;
		break;
	}

	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		err_hw("[%d]invalid slot (%d,%d)", instance, hw_id, hw_slot);
		goto p_err;
	}

	hw_ip = &hardware->hw_ip[hw_slot];
	if (hw_ip == NULL) {
		err_hw("[%d][G:0x%d]hw_ip(null) (%d,%d)", instance, GROUP_ID(group->id), hw_id, hw_slot);
		ret = -EINVAL;
		goto p_err;
	}

	if (!atomic_read(&hardware->stream_on) && atomic_read(&hw_ip->otf_start))
		info_hw("[%d]grp_shot [F:%d][G:0x%x][R:0x%lx][O:0x%lx][IN:0x%x]\n",
			instance, frame->fcount, GROUP_ID(group->id),
			frame->req_flag, frame->out_flag,
			frame->dvaddr_buffer[0]);

	framemgr = hw_ip->framemgr;
	framemgr_e_barrier_irqs(framemgr, 0, flags);
	ret = check_shot_exist(framemgr, frame->fcount);
	framemgr_x_barrier_irqr(framemgr, 0, flags);

	/* check late shot */
	if (hw_ip->internal_fcount >= frame->fcount || ret == INTERNAL_SHOT_EXIST) {
		info_hw("[%d]LATE_SHOT (%d)[F:%d][G:0x%x][R:0x%lx][O:0x%lx][C:0x%lx]\n",
			instance, hw_ip->internal_fcount, frame->fcount, GROUP_ID(group->id),
			frame->req_flag, frame->out_flag, frame->core_flag);
		frame->type = SHOT_TYPE_LATE;
		framemgr = &hardware->framemgr_late;
		if (framemgr->queued_count[FS_REQUEST] > 0) {
			warn_hw("[%d]LATE_SHOT REQ(%d) > 0, PRO(%d)",
				instance,
				framemgr->queued_count[FS_REQUEST],
				framemgr->queued_count[FS_PROCESS]);
		}
		ret = 0;
	} else {
		frame->type = SHOT_TYPE_EXTERNAL;
		enable_debug_info = false;
		framemgr = hw_ip->framemgr;
	}

	framemgr_e_barrier_irqs(framemgr, 0, flags);
	hw_frame = get_frame(framemgr, FS_FREE);
	framemgr_x_barrier_irqr(framemgr, 0, flags);
	if (hw_frame == NULL) {
		err_hw("[%d][G:0x%x]free_head(NULL)", instance, GROUP_ID(group->id));
		ret = -EINVAL;
		goto p_err;
	}

	hw_frame->groupmgr	= frame->groupmgr;
	hw_frame->group		= frame->group;
	hw_frame->shot		= frame->shot;
	hw_frame->shot_ext	= frame->shot_ext;
	hw_frame->kvaddr_shot	= frame->kvaddr_shot;
	hw_frame->dvaddr_shot	= frame->dvaddr_shot;
	hw_frame->shot_size	= frame->shot_size;
	hw_frame->fcount	= frame->fcount;
	hw_frame->rcount	= frame->rcount;
	hw_frame->req_flag	= frame->req_flag;
	hw_frame->out_flag	= frame->out_flag;
	hw_frame->ndone_flag	= 0;
	hw_frame->core_flag	= 0;

	for (i = 0; i < FIMC_IS_MAX_PLANES; i++) {
		hw_frame->kvaddr_buffer[i]	= frame->kvaddr_buffer[i];
		hw_frame->dvaddr_buffer[i]	= frame->dvaddr_buffer[i];
	}
	hw_frame->instance = instance;

	if (frame->type == SHOT_TYPE_LATE)
		hw_frame->type = SHOT_TYPE_LATE;
	else
		hw_frame->type = SHOT_TYPE_EXTERNAL;

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &hw_ip->group[instance]->state)) {
		if (!atomic_read(&hw_ip->otf_start)) {
			atomic_set(&hw_ip->otf_start, 1);
			info_hw("[%d]OTF start [F:%d][G:0x%x][R:0x%lx][O:0x%lx]\n",
				instance, frame->fcount, GROUP_ID(group->id),
				frame->req_flag, frame->out_flag);

			for (hw_slot = 0; hw_slot < HW_SLOT_MAX; hw_slot++) {
				hw_ip = &hardware->hw_ip[hw_slot];
				atomic_set(&hw_ip->fs_count, (frame->fcount - 1));
				atomic_set(&hw_ip->fe_count, (frame->fcount - 1));
				atomic_set(&hw_ip->cl_count, (frame->fcount - 1));
			}
		} else {
			framemgr_e_barrier_irqs(framemgr, 0, flags);
			put_frame(framemgr, hw_frame, FS_REQUEST);
			framemgr_x_barrier_irqr(framemgr, 0, flags);

			return ret;
		}
	}

	if (frame->type == SHOT_TYPE_LATE) {
		err_hw("[%d]grp_shot: LATE_SHOT", instance);
		goto p_err;
	}

	ret = fimc_is_hardware_shot(hardware,
			instance,
			group,
			hw_frame,
			framemgr,
			hw_path,
			frame->fcount);
	if (ret) {
		err_hw("hardware_shot fail [G:0x%x](%d)", GROUP_ID(group->id), hw_ip->id);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int make_internal_shot(struct fimc_is_hw_ip *hw_ip,
	u32 instance,
	u32 framenum,
	struct fimc_is_frame **in_frame,
	struct fimc_is_framemgr *framemgr)
{
	int ret = 0;
	int i = 0;
	struct fimc_is_frame *frame;

	BUG_ON(!hw_ip);
	BUG_ON(!framemgr);

	if (framemgr->queued_count[FS_FREE] < 3)
		warn_hw("[%d][ID:%d] Free frame is less than 3", instance, hw_ip->id);

	ret = check_shot_exist(framemgr, framenum);
	if (ret == INTERNAL_SHOT_EXIST)
		goto exit;

	frame = get_frame(framemgr, FS_FREE);
	if (frame == NULL) {
		err_hw("[%d]config_lock: frame(null)", instance);
		frame_manager_print_info_queues(framemgr);

		while (framemgr->queued_count[FS_PROCESS]) {
			frame = get_frame(hw_ip->framemgr, FS_PROCESS);
			warn_hw("[%d][ID:%d]config_lock: process_list " \
				"[F:%d][G:0x%x][R:0x%lx][C:0x%lx][O:0x%lx]\n",
				instance, hw_ip->id,
				frame->fcount, GROUP_ID(hw_ip->group[instance]->id),
				frame->req_flag, frame->core_flag, frame->out_flag);
		}

		while (framemgr->queued_count[FS_REQUEST]) {
			frame = get_frame(hw_ip->framemgr, FS_REQUEST);
			warn_hw("[%d][ID:%d]config_lock: request_list " \
				"[F:%d][G:0x%x][R:0x%lx][C:0x%lx][O:0x%lx]\n",
				instance, hw_ip->id,
				frame->fcount, GROUP_ID(hw_ip->group[instance]->id),
				frame->req_flag, frame->core_flag, frame->out_flag);
		}
		ret = -EINVAL;
		goto exit;
	}
	frame->groupmgr		= NULL;
	frame->group		= NULL;
	frame->shot		= NULL;
	frame->shot_ext		= NULL;
	frame->kvaddr_shot	= 0;
	frame->dvaddr_shot	= 0;
	frame->shot_size	= 0;
	frame->fcount		= framenum;
	frame->rcount		= 0;
	frame->req_flag		= 0;
	frame->out_flag		= 0;
	frame->core_flag	= 0;

	for (i = 0; i < FIMC_IS_MAX_PLANES; i++) {
		frame->kvaddr_buffer[i]	= 0;
		frame->dvaddr_buffer[i]	= 0;
	}
	frame->type = SHOT_TYPE_INTERNAL;
	frame->instance = instance;
	*in_frame = frame;

exit:
	return ret;
}

int fimc_is_hardware_config_lock(struct fimc_is_hw_ip *hw_ip, u32 instance, u32 framenum)
{
	int ret = 0;
	struct fimc_is_frame *frame;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_hardware *hardware;
	unsigned long flags;

	BUG_ON(!hw_ip);

	hardware = hw_ip->hardware;

	if (!test_bit(FIMC_IS_GROUP_OTF_INPUT, &hw_ip->group[instance]->state))
		return ret;

	dbg_hw("C.L [F:%d]\n", framenum);

	framemgr = hw_ip->framemgr;
	framemgr_e_barrier_irqs(framemgr, 0, flags);
	if (framemgr->queued_count[FS_REQUEST]) {
		frame = get_frame(framemgr, FS_REQUEST);
	} else {
		ret = make_internal_shot(hw_ip, instance, framenum + 1, &frame, framemgr);
		if (ret == INTERNAL_SHOT_EXIST) {
			framemgr_x_barrier_irqr(framemgr, 0, flags);
			goto p_err;
		}
		if (ret) {
			framemgr_x_barrier_irqr(framemgr, 0, flags);
			print_hw_interrupt_time(hw_ip);
			BUG_ON(1);
		}
		info_hw("config_lock: INTERNAL_SHOT [F:%d](%d)\n", (framenum + 1), frame->index);
	}
	frame->frame_info[INFO_CONFIG_LOCK].cpu = raw_smp_processor_id();
	frame->frame_info[INFO_CONFIG_LOCK].pid = current->pid;
	frame->frame_info[INFO_CONFIG_LOCK].when = cpu_clock(raw_smp_processor_id());

	framemgr_x_barrier_irqr(framemgr, 0, flags);

	ret = fimc_is_hardware_shot(hardware,
			instance,
			hw_ip->group[instance],
			frame,
			framemgr,
			hardware->hw_path[instance],
			framenum);
	if (ret) {
		err_hw("hardware_shot fail [G:0x%x](%d)", GROUP_ID(hw_ip->group[instance]->id), hw_ip->id);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

void check_late_shot(struct fimc_is_hw_ip *hw_ip)
{
	int ret = 0;
	struct fimc_is_frame *frame;
	struct fimc_is_framemgr *framemgr;
	unsigned long flags;

	/* check LATE_FRAME */
	framemgr = &hw_ip->hardware->framemgr_late;
	framemgr_e_barrier_irqs(framemgr, 0, flags);
	if (!framemgr->queued_count[FS_REQUEST]) {
		framemgr_x_barrier_irqr(framemgr, 0, flags);
		goto exit;
	}
	frame = get_frame(framemgr, FS_REQUEST);
	framemgr_x_barrier_irqr(framemgr, 0, flags);

	if (frame == NULL)
		goto exit;

	framemgr_e_barrier_irqs(framemgr, 0, flags);
	put_frame(framemgr, frame, FS_COMPLETE);
	framemgr_x_barrier_irqr(framemgr, 0, flags);

	ret = fimc_is_hardware_frame_ndone(hw_ip, frame, frame->instance, true);
	if (ret)
		err_hw("[%d]F_NDONE fail (%d)", frame->instance, hw_ip->id);

exit:
	dbg_hw("START TASK [ID:%d]\n", hw_ip->id);

	return;
}

void fimc_is_hardware_size_dump(struct fimc_is_hw_ip *hw_ip)
{
	struct fimc_is_hardware *hardware;
	int hw_slot = -1;

	BUG_ON(!hw_ip);
	BUG_ON(!hw_ip->hardware);

	hardware = hw_ip->hardware;

	for (hw_slot = 0; hw_slot < HW_SLOT_MAX; hw_slot++) {
		hw_ip = &hardware->hw_ip[hw_slot];
		if (hw_ip->ops->size_dump)
			hw_ip->ops->size_dump(hw_ip);;
	}

	return;
}

void fimc_is_hardware_frame_start(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	struct fimc_is_frame *frame;
	struct fimc_is_framemgr *framemgr;

	BUG_ON(!hw_ip);

	framemgr = hw_ip->framemgr;
	framemgr_e_barrier(framemgr, 0);
	atomic_inc(&hw_ip->fs_count);
	if (framemgr->queued_count[FS_PROCESS]) {
		frame = get_frame(framemgr, FS_PROCESS);
	} else {
		print_hw_frame_count(hw_ip);
		frame_manager_print_info_queues(framemgr);
		framemgr_x_barrier(framemgr, 0);
		err_hw("FSTART frame null [ID:%d](%d)", hw_ip->id, hw_ip->internal_fcount);
		goto exit;
	}

	if (frame->fcount != atomic_read(&hw_ip->fs_count)) {
		/* error handling */
		info_hw("frame_start_isr (%d, %d)\n", frame->fcount, atomic_read(&hw_ip->fs_count));
	}
	/* TODO: multi-instance */
	frame->frame_info[INFO_FRAME_START].cpu = raw_smp_processor_id();
	frame->frame_info[INFO_FRAME_START].pid = current->pid;
	frame->frame_info[INFO_FRAME_START].when = cpu_clock(raw_smp_processor_id());
	put_frame(framemgr, frame, FS_COMPLETE);
	clear_bit(HW_CONFIG, &hw_ip->state);
	atomic_inc(&hw_ip->Vvalid);
	framemgr_x_barrier(framemgr, 0);

	if (!test_bit(FIMC_IS_GROUP_OTF_INPUT, &hw_ip->group[instance]->state))
		goto exit;

	check_late_shot(hw_ip);

exit:
	return;
}

int fimc_is_hardware_stream_on(struct fimc_is_hardware *hardware,
	u32 instance,
	unsigned long hw_path)
{
	int ret = 0;
	int hw_slot = -1;
	struct fimc_is_hw_ip *hw_ip = NULL;

	BUG_ON(!hardware);

	for (hw_slot = 0; hw_slot < HW_SLOT_MAX; hw_slot++) {
		hw_ip = &hardware->hw_ip[hw_slot];
		ret = CALL_HW_OPS(hw_ip, enable, instance, hw_path);
		if (ret) {
			err_hw("[%d]enable fail (%d,%d)", instance, hw_ip->id, hw_slot);
			ret = -EINVAL;
			goto p_err;
		}
	}

	atomic_set(&hardware->stream_on, 1);
	info_hw("[%d]stream_on [P:0x%lx]\n", instance, hw_path);

p_err:
	return ret;
}

int fimc_is_hardware_stream_off(struct fimc_is_hardware *hardware,
	u32 instance,
	unsigned long hw_path)
{
	int ret = 0;
	int hw_slot = -1;
	struct fimc_is_hw_ip *hw_ip = NULL;

	BUG_ON(!hardware);

	atomic_set(&hardware->stream_on, 0);

	for (hw_slot = 0; hw_slot < HW_SLOT_MAX; hw_slot++) {
		hw_ip = &hardware->hw_ip[hw_slot];
		info_hw("[%d][ID:%d]stream_off \n", instance, hw_ip->id);
		ret = CALL_HW_OPS(hw_ip, disable, instance, hw_path);
		if (ret) {
			err_hw("[%d]disable fail (%d,%d)", instance, hw_ip->id, hw_slot);
			ret = -EINVAL;
			goto p_err;
		}
	}

	info_hw("[%d]stream_off [P:0x%lx]\n", instance, hw_path);

p_err:
	return ret;
}

int fimc_is_hardware_process_start(struct fimc_is_hardware *hardware,
	u32 instance, u32 group)
{
	int ret = 0;
	int hw_slot = -1;
	struct fimc_is_hw_ip *hw_ip = NULL;

	BUG_ON(!hardware);

	info_hw("[%d]process_start [G:0x%x]\n", instance, group);

	for (hw_slot = 0; hw_slot < HW_SLOT_MAX; hw_slot++) {
		hw_ip = &hardware->hw_ip[hw_slot];
		hw_ip->internal_fcount = 0;
	}

	return ret;
}

void fimc_is_hardware_force_stop(struct fimc_is_hardware *hardware,
	struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	int retry;
	int list_index;
	struct fimc_is_frame *frame;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_framemgr *framemgr_late;

	BUG_ON(!hw_ip);

	framemgr = hw_ip->framemgr;
	framemgr_late = &hw_ip->hardware->framemgr_late;

	pr_info("[@][HW][%d]complete_list (%d)(%d)(%d)\n",
		instance, framemgr->queued_count[FS_COMPLETE],
		framemgr->queued_count[FS_PROCESS],
		framemgr->queued_count[FS_REQUEST]);
	retry = 150;
	while (--retry && framemgr->queued_count[FS_COMPLETE]) {
		frame = peek_frame(framemgr, FS_COMPLETE);
		if (frame == NULL)
			break;

		info_hw("complete_list [F:%d][%d][O:0x%lx][C:0x%lx][(%d)",
			frame->fcount, frame->type, frame->out_flag, frame->core_flag,
			framemgr->queued_count[FS_COMPLETE]);
		ret = fimc_is_hardware_frame_ndone(hw_ip, frame, instance, false);
		if (ret) {
			err_hw("[%d]hardware_frame_ndone fail (%d)", instance, hw_ip->id);
			ret = -EINVAL;
			goto p_err;
		}
		warn_hw(" %d com waiting...", framemgr->queued_count[FS_COMPLETE]);
		msleep(1);
	}

	info_hw("[%d]process_list (%d)\n",
		instance, framemgr->queued_count[FS_PROCESS]);
	retry = 150;
	while (--retry && framemgr->queued_count[FS_PROCESS]) {
		frame = peek_frame(framemgr, FS_PROCESS);
		if (frame == NULL)
			break;

		info_hw("process_list [F:%d][%d][O:0x%lx][C:0x%lx][(%d)",
			frame->fcount, frame->type, frame->out_flag, frame->core_flag,
			framemgr->queued_count[FS_PROCESS]);
		ret = fimc_is_hardware_frame_ndone(hw_ip, frame, instance, false);
		if (ret) {
			err_hw("[%d]hardware_frame_ndone fail (%d)", instance, hw_ip->id);
			ret = -EINVAL;
			goto p_err;
		}
		warn_hw(" %d pro waiting...", framemgr->queued_count[FS_PROCESS]);
		msleep(1);
	}

	info_hw("[%d]request_list (%d)\n",
		instance, framemgr->queued_count[FS_REQUEST]);
	retry = 150;
	while (--retry && framemgr->queued_count[FS_REQUEST]) {
		frame = peek_frame(framemgr, FS_REQUEST);
		if (frame == NULL)
			break;

		set_bit(hw_ip->id, &frame->core_flag);

		info_hw("request_list [F:%d][%d][O:0x%lx][C:0x%lx][(%d)",
			frame->fcount, frame->type, frame->out_flag, frame->core_flag,
			framemgr->queued_count[FS_REQUEST]);
		ret = fimc_is_hardware_frame_ndone(hw_ip, frame, instance, false);
		if (ret) {
			err_hw("[%d]hardware_frame_ndone fail (%d)", instance, hw_ip->id);
			ret = -EINVAL;
			goto p_err;
		}
		warn_hw(" %d req waiting...", framemgr->queued_count[FS_REQUEST]);
		msleep(1);
	}

	pr_info("[@][HW][%d]late_list (%d)(%d)(%d)\n",
		instance, framemgr_late->queued_count[FS_COMPLETE],
		framemgr_late->queued_count[FS_PROCESS],
		framemgr_late->queued_count[FS_REQUEST]);
	for (list_index = FS_REQUEST; list_index < FS_INVALID; list_index++) {
		info_hw("[%d]late_list[%d] (%d)\n",
			instance, list_index, framemgr_late->queued_count[list_index]);
		retry = 150;
		while (--retry && framemgr_late->queued_count[list_index]) {
			frame = peek_frame(framemgr_late, list_index);
			if (frame == NULL)
				break;

			set_bit(hw_ip->id, &frame->core_flag);

			info_hw("late_list[%d] [F:%d][%d][O:0x%lx][C:0x%lx][(%d)",
				list_index, frame->fcount, frame->type,
				frame->out_flag, frame->core_flag,
				framemgr_late->queued_count[list_index]);
			ret = fimc_is_hardware_frame_ndone(hw_ip, frame, instance, true);
			if (ret) {
				err_hw("[%d]hardware_frame_ndone fail (%d)", instance, hw_ip->id);
				ret = -EINVAL;
				goto p_err;
			}
			warn_hw(" %d late waiting...", framemgr_late->queued_count[list_index]);
			msleep(1);
		}
	}

p_err:
	return;
}

int fimc_is_hardware_process_stop(struct fimc_is_hardware *hardware,
	u32 instance, u32 group, u32 mode)
{
	int ret = 0;
	int hw_slot = -1;
	struct fimc_is_hw_ip *hw_ip;
	enum fimc_is_hardware_id hw_id = DEV_HW_END;

	BUG_ON(!hardware);

	info_hw("[%d]process_stop [G:0x%x](%d)\n", instance, group, mode);

	if (mode == 0)
		goto exit;

	if ((group) & GROUP_ID(GROUP_ID_3AA0)) {
		hw_id = DEV_HW_3AA0;
		hw_slot = fimc_is_hw_slot_id(hw_id);
		if (valid_hw_slot_id(hw_slot)) {
			hw_ip = &hardware->hw_ip[hw_slot];
			fimc_is_hardware_force_stop(hardware, hw_ip, instance);
			atomic_set(&hw_ip->otf_start, 0);
		}
	}

	if ((group) & GROUP_ID(GROUP_ID_3AA1)) {
		hw_id = DEV_HW_3AA1;
		hw_slot = fimc_is_hw_slot_id(hw_id);
		if (valid_hw_slot_id(hw_slot)) {
			hw_ip = &hardware->hw_ip[hw_slot];
			fimc_is_hardware_force_stop(hardware, hw_ip, instance);
			atomic_set(&hw_ip->otf_start, 0);
		}
	}

	if ((group) & GROUP_ID(GROUP_ID_ISP0)) {
		hw_id = DEV_HW_ISP0;
		hw_slot = fimc_is_hw_slot_id(hw_id);
		if (valid_hw_slot_id(hw_slot)) {
			hw_ip = &hardware->hw_ip[hw_slot];
			fimc_is_hardware_force_stop(hardware, hw_ip, instance);
			atomic_set(&hw_ip->otf_start, 0);
		}
	}

exit:
	return ret;
}
int fimc_is_hardware_alloc_dma(struct fimc_is_hardware *hardware,
				struct fimc_is_minfo *mem_info,
				struct fimc_is_ishcain_mem *imem)
{
	ulong kvaddr = 0;
	u32 dvaddr = 0;
	u32 offset = 0;
	int ret, i;

	kvaddr = (ulong)fimc_is_alloc_dma_buffer(FIMC_IS_LHFD_MAP_SIZE);
	ret = fimc_is_translate_kva_to_dva(kvaddr, &dvaddr);
	for (i = 0; i < NUM_FD_INTERNAL_BUF; i++) {
		mem_info->kvaddr_fd[i] = kvaddr + offset;
		mem_info->dvaddr_fd[i] = dvaddr + offset;
		hardware->kvaddr_fd[i] = kvaddr + offset;
		offset += (SIZE_FD_INTERNEL_BUF);
	}
	imem->kvaddr_fd = mem_info->kvaddr_fd;
	imem->dvaddr_fd = mem_info->dvaddr_fd;

	return 0;
}

int fimc_is_hardware_free_dma(struct fimc_is_hardware *hardware)
{
	fimc_is_free_dma_buffer((void *)hardware->kvaddr_fd);

	return 0;
}

int fimc_is_hardware_open(struct fimc_is_hardware *hardware, u32 hw_id,
		struct fimc_is_group *group, u32 instance, bool rep_flag, u32 module_id)
{
	int ret = 0;
	int hw_slot = -1;
	struct fimc_is_hw_ip *hw_ip = NULL;
	struct fimc_is_group *parent;
	u32 size = 0;

	BUG_ON(!hardware);

	parent = group;
	while (parent->parent)
		parent = parent->parent;

	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot))
		goto p_err;

#ifndef ENABLE_RESERVED_INTERNAL_DMA
	if (hw_id == DEV_HW_FD)
		fimc_is_hardware_alloc_dma(hardware,
					&group->device->resourcemgr->minfo,
					&group->device->imemory);
#endif

	hw_ip = &(hardware->hw_ip[hw_slot]);
	hw_ip->hardware = hardware;
	hw_ip->framemgr = &hardware->framemgr[parent->id];
	ret = CALL_HW_OPS(hw_ip, open, instance, &size);
	if (ret) {
		err_hw("[%d]open fail (%d)", instance, hw_ip->id);
		goto p_err;
	}
	if (size) {
		hw_ip->priv_info = kzalloc(size, GFP_KERNEL);
		if(!hw_ip->priv_info) {
			err_hw("hw_ip->priv_info(null) (%d)", hw_ip->id);
			ret = -EINVAL;
			goto p_err;
		}
	} else {
		hw_ip->priv_info = NULL;
	}

	ret = CALL_HW_OPS(hw_ip, init, group, rep_flag, module_id);
	if (ret) {
		err_hw("[%d]init fail (%d)", instance, hw_ip->id);
		goto p_err;
	}

	hw_ip->fs_time = 0;
	hw_ip->fe_time = 0;
	hw_ip->dma_time = 0;
	set_bit(HW_OPEN, &hw_ip->state);
	set_bit(HW_INIT, &hw_ip->state);
	atomic_inc(&hw_ip->ref_count);
	atomic_set(&hw_ip->fs_count, 0);
	atomic_set(&hw_ip->fe_count, 0);
	atomic_set(&hw_ip->cl_count, 0);
	atomic_set(&hw_ip->Vvalid, 0);
	set_bit(hw_id, &hardware->hw_path[instance]);

	enable_debug_info = false;
p_err:
	return ret;
}

int fimc_is_hardware_close(struct fimc_is_hardware *hardware,
	u32 hw_id,
	u32 instance)
{
	int ret = 0;
	int hw_slot = -1;
	struct fimc_is_hw_ip *hw_ip = NULL;

	BUG_ON(!hardware);

	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (!valid_hw_slot_id(hw_slot))
		goto p_err;

	hw_ip = &(hardware->hw_ip[hw_slot]);

	switch (hw_id) {
	case DEV_HW_3AA0:
	case DEV_HW_3AA1:
		fimc_is_hw_3aa_object_close(hw_ip, instance);
		break;
	case DEV_HW_ISP0:
		fimc_is_hw_isp_object_close(hw_ip, instance);
		break;
	default:
		break;
	}

	atomic_dec(&hw_ip->ref_count);
	if (atomic_read(&hw_ip->ref_count)) {
		err_hw("[%d][ID:%d] ref_count(%d)\n", instance, hw_ip->id,
			atomic_read(&hw_ip->ref_count));
		goto p_err;
	}

	ret = CALL_HW_OPS(hw_ip, close, instance);
	if (ret) {
		err_hw("[%d]close fail (%d)", instance, hw_ip->id);
		goto p_err;
	}

	switch (hw_id) {
	case DEV_HW_FD:
#ifndef ENABLE_RESERVED_INTERNAL_DMA
		ret = fimc_is_hardware_free_dma(hardware);
#endif
		break;
	default:
		break;
	}

	kfree(hw_ip->priv_info);
	clear_bit(hw_id, &hardware->hw_path[instance]);

	hw_ip->fs_time = 0;
	hw_ip->fe_time = 0;
	hw_ip->dma_time = 0;
	clear_bit(HW_OPEN, &hw_ip->state);
	clear_bit(HW_INIT, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);
	clear_bit(HW_RUN, &hw_ip->state);
	atomic_set(&hw_ip->otf_start, 0);
	hw_ip->fcount = 0;
	hw_ip->internal_fcount = 0;

p_err:
	return ret;
}

int fimc_is_hardware_shadow_sw(struct fimc_is_hw_ip *hw_ip,
				struct fimc_is_frame *frame)
{
	int ret = 0;

	BUG_ON(!hw_ip);
	BUG_ON(!frame);

	if ((hw_ip->id != DEV_HW_MCSC) && (hw_ip->id != DEV_HW_FD))
		goto exit;

	ret = fimc_is_hw_fd_shadow_trigger(hw_ip, frame);
	if (ret) {
		err_hw("[ID:%d]FD shadow trigger fail!!", hw_ip->id);
		ret = -EINVAL;
		goto exit;
	}

exit:
	return ret;
}

int do_frame_done_work_func(struct fimc_is_interface *itf,
	int wq_id, u32 instance, u32 group_id, u32 fcount, u32 rcount, u32 status)
{
	int ret = 0;
	bool retry_flag = false;
	struct work_struct		*work_wq;
	struct fimc_is_work_list	*work_list;
	struct fimc_is_work		*work;

	work_wq   = &itf->work_wq[wq_id];
	work_list = &itf->work_list[wq_id];
retry:
	get_free_work_irq(work_list, &work);
	if (work) {
		work->msg.id		= 0;
		work->msg.command	= IHC_FRAME_DONE;
		work->msg.instance	= instance;
		work->msg.group		= GROUP_ID(group_id);
		work->msg.param1	= fcount;
		work->msg.param2	= rcount;
		work->msg.param3	= status; /* status: 0:FRAME_DONE, 1:FRAME_NDONE */
		work->msg.param4	= 0;

		work->fcount = work->msg.param1;
		set_req_work_irq(work_list, work);

		if (!work_pending(work_wq))
			wq_func_schedule(itf, work_wq);
	} else {
		err_hw("free work item is empty (%d)", (int)retry_flag);
		if (retry_flag == false) {
			retry_flag = true;
			goto retry;
		}
		ret = -EINVAL;
	}

	return ret;
}

int check_core_end(struct fimc_is_hw_ip *hw_ip,
	u32 hw_fcount,
	struct fimc_is_frame **in_frame,
	struct fimc_is_framemgr *framemgr,
	u32 output_id,
	enum fimc_is_frame_done_type done_type)
{
	int ret = 0;
	struct fimc_is_frame *frame = *in_frame;

	BUG_ON(!hw_ip);
	BUG_ON(!frame);
	BUG_ON(!framemgr);

	if (frame->fcount != hw_fcount) {
		if ((hw_ip->is_leader) && (hw_ip->fcount - frame->fcount >= 2)) {
			dbg_hw("[%d]LATE  CORE END [ID:%d][F:%d][0x%x][C:0x%lx][O:0x%lx][END:%d,%d]\n",
				frame->instance, hw_ip->id, frame->fcount,
				output_id, frame->core_flag, frame->out_flag,
				hw_ip->fcount, hw_fcount);

			info_hw("%d: force_done for LATE FRAME [ID:%d][F:%d]\n",
				__LINE__, hw_ip->id, frame->fcount);
			ret = fimc_is_hardware_frame_ndone(hw_ip, frame, frame->instance, false);
			if (ret) {
				err_hw("[%d]hardware_frame_ndone fail (%d)", frame->instance, hw_ip->id);
				ret = -EINVAL;
				goto exit;
			}
		}

		framemgr_e_barrier(framemgr, 0);
		*in_frame = find_frame(framemgr, FS_COMPLETE, frame_fcount,
					(void *)hw_fcount);
		framemgr_x_barrier(framemgr, 0);
		frame = *in_frame;

		if (frame == NULL) {
			err_hw("[ID:%d][F:%d]frame(null)!!(%d)", hw_ip->id, hw_fcount, done_type);
			framemgr_e_barrier(framemgr, 0);
			print_hw_frame_count(hw_ip);
			frame_manager_print_info_queues(framemgr);
			framemgr_x_barrier(framemgr, 0);
			ret = -EINVAL;
			goto exit;
		}

		if (!test_bit(hw_ip->id, &frame->core_flag)) {
			info_hw("[%d]invalid core_flag [ID:%d][F:%d][0x%x][C:0x%lx][O:0x%lx]",
				frame->instance, hw_ip->id, frame->fcount,
				output_id, frame->core_flag, frame->out_flag);
			ret = -EINVAL;
			goto exit;
		}
	} else {
		dbg_hw("[ID:%d][%d,F:%d]FRAME COUNT invalid",
			hw_ip->id, frame->fcount, hw_ip->fcount);
	}

exit:
	return ret;
}

int check_frame_end(struct fimc_is_hw_ip *hw_ip,
	u32 hw_fcount,
	struct fimc_is_frame **in_frame,
	struct fimc_is_framemgr *framemgr,
	u32 output_id,
	enum fimc_is_frame_done_type done_type)
{
	int ret = 0;
	struct fimc_is_frame *frame = *in_frame;

	BUG_ON(!hw_ip);
	BUG_ON(!frame);
	BUG_ON(!framemgr);

	if (frame->fcount != hw_fcount) {
		dbg_hw("[%d]LATE FRAME END [ID:%d][F:%d][0x%x][C:0x%lx][O:0x%lx][END:%d,%d]\n",
			frame->instance, hw_ip->id, frame->fcount,
			output_id, frame->core_flag, frame->out_flag,
			hw_ip->fcount, hw_fcount);

		framemgr_e_barrier(framemgr, 0);
		*in_frame = find_frame(framemgr, FS_COMPLETE, frame_fcount,
					(void *)hw_fcount);
		framemgr_x_barrier(framemgr, 0);
		frame = *in_frame;
		if (frame == NULL) {
			err_hw("[ID:%d][F:%d]frame(null)!!(%d)", hw_ip->id, hw_fcount, done_type);
			framemgr_e_barrier(framemgr, 0);
			print_hw_frame_count(hw_ip);
			frame_manager_print_info_queues(framemgr);
			framemgr_x_barrier(framemgr, 0);
			ret = -EINVAL;
			goto exit;
		}

		if (!test_bit(output_id, &frame->out_flag)) {
			info_hw("[%d]invalid output_id [ID:%d][F:%d][0x%x][C:0x%lx][O:0x%lx]",
				frame->instance, hw_ip->id, frame->fcount,
				output_id, frame->core_flag, frame->out_flag);
			ret = -EINVAL;
			goto exit;
		}
	}

exit:
	return ret;
}

int fimc_is_hardware_frame_done(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_frame *frame,
	int wq_id, u32 output_id, u32 status,
	enum fimc_is_frame_done_type done_type)
{
	int ret = 0;
	u32 fe_count = 0;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_group *parent;

	BUG_ON(!hw_ip);

	fe_count = atomic_read(&hw_ip->fe_count);

	switch(done_type) {
	case FRAME_DONE_NORMAL:
		framemgr = hw_ip->framemgr;
		if (frame == NULL) {
			framemgr_e_barrier(framemgr, 0);
			frame = peek_frame(framemgr, FS_COMPLETE);
			framemgr_x_barrier(framemgr, 0);
		} else {
			warn_hw("[ID:%d]frame NOT null!!(%d)", hw_ip->id, done_type);
		}
		break;
	case FRAME_DONE_FORCE:
		framemgr = hw_ip->framemgr;
		info_hw("FORCE_DONE start [ID:%d][0x%x][F:%d][C:0x%lx][O:0x%lx]\n",
			hw_ip->id, output_id, frame->fcount, frame->core_flag, frame->out_flag);
		break;
	case FRAME_DONE_LATE_SHOT:
		framemgr = &hw_ip->hardware->framemgr_late;
		if (frame == NULL) {
			warn_hw("[ID:%d]frame null!!(%d)", hw_ip->id, done_type);
			framemgr_e_barrier(framemgr, 0);
			frame = peek_frame(framemgr, FS_COMPLETE);
			framemgr_x_barrier(framemgr, 0);
		}

		if (frame!= NULL && frame->type != SHOT_TYPE_LATE) {
			warn_hw("invalid frame type");
			frame->type = SHOT_TYPE_LATE;
		}
		break;
	default:
		framemgr = hw_ip->framemgr;
		err_hw("[ID:%d]invalid done_type(%d)", hw_ip->id, done_type);
		ret = -EINVAL;
		goto exit;
		break;
	}

	if (frame == NULL) {
		err_hw("[ID:%d][F:%d]frame_done: frame(null)!!(%d)(0x%x)",
			hw_ip->id, hw_ip->fcount, done_type, output_id);
		framemgr_e_barrier(framemgr, 0);
		print_hw_frame_count(hw_ip);
		frame_manager_print_info_queues(framemgr);
		framemgr_x_barrier(framemgr, 0);
		ret = -EINVAL;
		goto exit;
	}

	parent = hw_ip->group[frame->instance];
	while (parent->parent)
		parent = parent->parent;

	dbg_hw("[%d][ID:%d][0x%x]frame_done [F:%d][G:0x%x][R:0x%lx][C:0x%lx][O:0x%lx]\n",
		frame->instance, hw_ip->id, output_id,
		frame->fcount, GROUP_ID(parent->id), frame->req_flag,
		frame->core_flag, frame->out_flag);

	/* check core_done */
	if (output_id == FIMC_IS_HW_CORE_END) {
		switch(done_type) {
		case FRAME_DONE_NORMAL:
			if (!test_bit(hw_ip->id, &frame->core_flag)) {
				ret = check_core_end(hw_ip, hw_ip->fcount, &frame,
					framemgr, output_id, done_type);
			}
			break;
		case FRAME_DONE_FORCE:
			goto shot_done;
			break;
		case FRAME_DONE_LATE_SHOT:
			goto shot_done;
			break;
		default:
			err_hw("[ID:%d]invalid done_type(%d)", hw_ip->id, done_type);
			break;
		}

		if (ret)
			goto exit;

		frame->frame_info[INFO_FRAME_END_PROC].cpu = raw_smp_processor_id();
		frame->frame_info[INFO_FRAME_END_PROC].pid = current->pid;
		frame->frame_info[INFO_FRAME_END_PROC].when = cpu_clock(raw_smp_processor_id());

		if (frame->shot)
			fimc_is_hardware_get_meta(hw_ip, frame,
				frame->instance, hw_ip->hardware->hw_path[frame->instance]);

	} else {
		if (frame->type == SHOT_TYPE_INTERNAL)
			goto shot_done;

		switch(done_type) {
		case FRAME_DONE_NORMAL:
			if (!test_bit(output_id, &frame->out_flag)) {
				ret = check_frame_end(hw_ip, hw_ip->fcount, &frame,
					framemgr, output_id, done_type);
				if (ret)
					goto exit;
			}
			break;
		case FRAME_DONE_FORCE:
			if (!test_bit(output_id, &frame->out_flag))
				goto shot_done;
			break;
		case FRAME_DONE_LATE_SHOT:
			break;
		default:
			err_hw("[ID:%d]invalid done_type(%d)", hw_ip->id, done_type);
			break;
		}

		if (status != 0) {
			set_bit(wq_id, &frame->ndone_flag);
			dbg_hw("[%d]FRAME NDONE (%d)\n", frame->instance, hw_ip->id);
		}

		ret = do_frame_done_work_func(hw_ip->itf,
				wq_id,
				frame->instance,
				parent->id,
				frame->fcount,
				frame->rcount,
				status);
		if (ret)
			BUG_ON(1);

		clear_bit(output_id, &frame->out_flag);
	}

shot_done:
	if (output_id == FIMC_IS_HW_CORE_END) {
		clear_bit(hw_ip->id, &frame->core_flag);

		if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &parent->state)
			&& done_type != FRAME_DONE_FORCE) {
			/* runtime on/off for HW FD */
			ret = fimc_is_hardware_shadow_sw(hw_ip, frame);
			if (ret) {
				err_hw("hardware shadowing is fail, (%d)\n", ret);
				ret = -EINVAL;
				goto exit;
			}
		}
	}

	if (done_type == FRAME_DONE_FORCE)
		info_hw("FORCE_DONE [ID:%d][0x%x][F:%d][C:0x%lx][O:0x%lx]\n",
			hw_ip->id, output_id, frame->fcount, frame->core_flag, frame->out_flag);

	if ((frame->out_flag == 0)
		&& (frame->core_flag == 0)
		&& !atomic_read(&frame->shot_done_flag)) {
		atomic_set(&frame->shot_done_flag, SHOT_DONE_SET);
		ret = fimc_is_hardware_shot_done(hw_ip, frame, framemgr, done_type);
	}

exit:
	return ret;
}

int fimc_is_hardware_shot_done(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_frame *frame,
	struct fimc_is_framemgr *framemgr,
	enum fimc_is_frame_done_type done_type)
{
	int ret = 0;
	struct work_struct *work_wq;
	struct fimc_is_work_list *work_list;
	struct fimc_is_work *work;
	struct fimc_is_group *parent;
	u32  req_id;

	u32 cmd = ISR_DONE;
	u32 err_type = IS_SHOT_SUCCESS;

	BUG_ON(!hw_ip);

	if (frame == NULL) {
		err_hw("[ID:%d]frame(null)!!", hw_ip->id);
		framemgr_e_barrier(framemgr, 0);
		frame_manager_print_info_queues(framemgr);
		while (framemgr->queued_count[FS_COMPLETE]) {
			frame = get_frame(framemgr, FS_COMPLETE);
			err_hw("[%d][ID:%d]shot_done: complete_list "
				"[F:%d][G:0x%x][R:0x%lx][C:0x%lx][O:0x%lx]\n",
				frame->instance, hw_ip->id,
				frame->fcount, GROUP_ID(hw_ip->group[frame->instance]->id),
				frame->req_flag, frame->core_flag, frame->out_flag);
		}

		while (framemgr->queued_count[FS_PROCESS]) {
			frame = get_frame(framemgr, FS_PROCESS);
			err_hw("[%d][ID:%d]shot_done: process_list "
				"[F:%d][G:0x%x][R:0x%lx][C:0x%lx][O:0x%lx]\n",
				frame->instance, hw_ip->id,
				frame->fcount, GROUP_ID(hw_ip->group[frame->instance]->id),
				frame->req_flag, frame->core_flag, frame->out_flag);
		}
		framemgr_x_barrier(framemgr, 0);

		BUG_ON(!frame);
	}

	parent = hw_ip->group[frame->instance];
	while (parent->parent)
		parent = parent->parent;

	dbg_hw("[%d][ID:%d]shot_done [F:%d][G:0x%x][R:0x%lx][C:0x%lx][O:0x%lx]\n",
		frame->instance, hw_ip->id,
		frame->fcount, GROUP_ID(parent->id), frame->req_flag,
		frame->core_flag, frame->out_flag);

	if (frame->type == SHOT_TYPE_INTERNAL)
		goto free_frame;

	switch (parent->id) {
	case GROUP_ID_3AA0:
		req_id = REQ_3AA_SHOT;
		break;
	case GROUP_ID_ISP0:
		req_id = REQ_ISP_SHOT;
		break;
	case GROUP_ID_DIS0:
		req_id = REQ_DIS_SHOT;
		break;
	default:
		err_hw("invalid group (%d)", parent->id);
		goto exit;
		break;
	}

	if (!test_bit(req_id, &frame->req_flag)) {
		err_hw("[%d]invalid req_flag [ID:%d][F:%d][0x%x][R:0x%lx]",
			frame->instance, hw_ip->id, frame->fcount, req_id, frame->req_flag);
		goto free_frame;
	}

	if (frame->ndone_flag != 0) {
		cmd      = ISR_NDONE;
		err_type = IS_SHOT_UNKNOWN;
	} else {
		cmd      = ISR_DONE;
		err_type = IS_SHOT_SUCCESS;
	}

	work_wq   = &hw_ip->itf->work_wq[INTR_SHOT_DONE];
	work_list = &hw_ip->itf->work_list[INTR_SHOT_DONE];

	get_free_work_irq(work_list, &work);
	if (work) {
		work->msg.id		= 0;
		work->msg.command	= IHC_FRAME_DONE;
		work->msg.instance	= frame->instance;
		work->msg.group		= GROUP_ID(parent->id);
		work->msg.param1	= frame->fcount;
		work->msg.param2	= err_type; /* status */
		work->msg.param3	= 0;
		work->msg.param4	= 0;

		work->fcount = work->msg.param1;
		set_req_work_irq(work_list, work);

		if (!work_pending(work_wq))
			wq_func_schedule(hw_ip->itf, work_wq);
	} else {
		err_hw("free work item is empty\n");
	}
	clear_bit(req_id, &frame->req_flag);

free_frame:
	if (done_type == FRAME_DONE_LATE_SHOT) {
		info_hw("[%d]LATE_SHOT_DONE [ID:%d][F:%d][G:0x%x]\n",
			frame->instance, hw_ip->id, frame->fcount, GROUP_ID(parent->id));
		goto exit;
	}

	if (done_type == FRAME_DONE_FORCE) {
		info_hw("[%d]SHOT_DONE_FORCE [ID:%d][F:%d][G:0x%x]\n",
			frame->instance, hw_ip->id, frame->fcount, GROUP_ID(parent->id));
		goto exit;
	}

	if (cmd == ISR_NDONE) {
		warn_hw("[%d]shot_NDONE [ID:%d][F:%d][G:0x%x]\n",
			frame->instance, hw_ip->id, frame->fcount, GROUP_ID(parent->id));
		goto exit;
	}

	if (frame->type == SHOT_TYPE_INTERNAL) {
		dbg_hw("[%d]INTERNAL_SHOT_DONE [ID:%d][F:%d][G:0x%x]\n",
			frame->instance, hw_ip->id, frame->fcount, GROUP_ID(parent->id));
	} else {
		dbg_hw("[%d]shot_ DONE [ID:%d][F:%d][G:0x%x]\n",
			frame->instance, hw_ip->id, frame->fcount, GROUP_ID(parent->id));
	}

exit:
	framemgr_e_barrier(framemgr, 0);
	trans_frame(framemgr, frame, FS_FREE);
	framemgr_x_barrier(framemgr, 0);
	atomic_set(&frame->shot_done_flag, SHOT_DONE_CLEAR);

	return ret;
}

int fimc_is_hardware_frame_ndone(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_frame *frame,
	u32 instance,
	bool late_flag)
{
	int ret = 0;
	int hw_slot = -1;
	struct fimc_is_hw_ip *hw_subip = NULL;
	enum fimc_is_hardware_id hw_id = DEV_HW_END;

	if (late_flag == true && frame != NULL)
		info_hw("frame_ndone [F:%d](%d)\n", frame->fcount, late_flag);

	switch (hw_ip->id) {
	case DEV_HW_3AA0:
	case DEV_HW_3AA1:
		ret = CALL_HW_OPS(hw_ip, frame_ndone, frame, instance, late_flag);
		if (ret) {
			err_hw("[%d]frame_ndone fail (%d,%d)", instance, hw_ip->id, hw_slot);
			ret = -EINVAL;
			goto p_err;
		}
#if defined(ENABLE_3AAISP)
		hw_id = DEV_HW_MCSC;
		hw_slot = fimc_is_hw_slot_id(hw_id);
		if (valid_hw_slot_id(hw_slot)) {
			hw_subip = &(hw_ip->hardware->hw_ip[hw_slot]);
			ret = CALL_HW_OPS(hw_subip, frame_ndone, frame, instance, late_flag);
			if (ret) {
				err_hw("[%d]frame_ndone fail (%d,%d)", instance, hw_id, hw_slot);
				ret = -EINVAL;
				goto p_err;
			}
		}

		hw_id = DEV_HW_FD;
		hw_slot = fimc_is_hw_slot_id(hw_id);
		if (valid_hw_slot_id(hw_slot)) {
			hw_subip = &(hw_ip->hardware->hw_ip[hw_slot]);
			ret = CALL_HW_OPS(hw_subip, frame_ndone, frame, instance, late_flag);
			if (ret) {
				err_hw("[%d]frame_ndone fail (%d,%d)", instance, hw_id, hw_slot);
				ret = -EINVAL;
				goto p_err;
			}
		}
#endif
		break;
	case DEV_HW_ISP0:
		ret = CALL_HW_OPS(hw_ip, frame_ndone, frame, instance, late_flag);
		if (ret) {
			err_hw("[%d]frame_ndone fail (%d,%d)", instance, hw_ip->id, hw_slot);
			ret = -EINVAL;
			goto p_err;
		}

		hw_id = DEV_HW_MCSC;
		hw_slot = fimc_is_hw_slot_id(hw_id);
		if (valid_hw_slot_id(hw_slot)) {
			hw_subip = &(hw_ip->hardware->hw_ip[hw_slot]);
			ret = CALL_HW_OPS(hw_subip, frame_ndone, frame, instance, late_flag);
			if (ret) {
				err_hw("[%d]frame_ndone fail (%d,%d)", instance, hw_id, hw_slot);
				ret = -EINVAL;
				goto p_err;
			}
		}
		break;
	default:
		err_hw("[%d]invalid hw (%d)", instance, hw_ip->id);
		break;
	}

p_err:
	return ret;
}

int _set_setfile_number(struct fimc_is_hardware *hardware, u32 hw_id, u32 num)
{
	int ret = 0;
	int hw_slot = -1;

	BUG_ON(!hardware);

	switch (hw_id) {
	case DEV_HW_3AA0:
		hw_slot = fimc_is_hw_slot_id(hw_id);
		if (valid_hw_slot_id(hw_slot))
			hardware->hw_ip[hw_slot].setfile_info.num = num;

		hw_slot = fimc_is_hw_slot_id(DEV_HW_3AA1);
		if (valid_hw_slot_id(hw_slot))
			hardware->hw_ip[hw_slot].setfile_info.num = num;

		hw_slot = fimc_is_hw_slot_id(DEV_HW_ISP0);
		if (valid_hw_slot_id(hw_slot))
			hardware->hw_ip[hw_slot].setfile_info.num = num;
		break;
	case DEV_HW_DRC:
	case DEV_HW_DIS:
	case DEV_HW_3DNR:
	case DEV_HW_MCSC:
	case DEV_HW_FD:
		hw_slot = fimc_is_hw_slot_id(hw_id);
		if (valid_hw_slot_id(hw_slot))
			hardware->hw_ip[hw_slot].setfile_info.num = num;
		break;
	default:
		err_hw("invalid hw (%d)", hw_id);
		ret = -EINVAL;
		goto p_err;
		break;
	}

	if (valid_hw_slot_id(hw_slot)) {
		dbg_hw("[ID:%d] setfile number(%d)\n",
			hw_id, hardware->hw_ip[hw_slot].setfile_info.num);
	}

p_err:
	return ret;
}

int _set_setfile_table(struct fimc_is_hardware *hardware,
	u32 hw_id, ulong addr, u32 size, int index)
{
	int ret = 0;
	int hw_slot = -1;

	BUG_ON(!hardware);

	switch (hw_id) {
	case DEV_HW_3AA0:
		hw_slot = fimc_is_hw_slot_id(hw_id);
		if (valid_hw_slot_id(hw_slot)) {
			hardware->hw_ip[hw_slot].setfile_info.table[index].addr = addr;
			hardware->hw_ip[hw_slot].setfile_info.table[index].size = size;
		}

		hw_slot = fimc_is_hw_slot_id(DEV_HW_3AA1);
		if (valid_hw_slot_id(hw_slot)) {
			hardware->hw_ip[hw_slot].setfile_info.table[index].addr = addr;
			hardware->hw_ip[hw_slot].setfile_info.table[index].size = size;
		}

		hw_slot = fimc_is_hw_slot_id(DEV_HW_ISP0);
		if (valid_hw_slot_id(hw_slot)) {
			hardware->hw_ip[hw_slot].setfile_info.table[index].addr = addr;
			hardware->hw_ip[hw_slot].setfile_info.table[index].size = size;
		}
		break;
	case DEV_HW_DRC:
	case DEV_HW_DIS:
	case DEV_HW_3DNR:
	case DEV_HW_MCSC:
	case DEV_HW_FD:
		hw_slot = fimc_is_hw_slot_id(hw_id);
		if (valid_hw_slot_id(hw_slot)) {
			hardware->hw_ip[hw_slot].setfile_info.table[index].addr = addr;
			hardware->hw_ip[hw_slot].setfile_info.table[index].size = size;
		}
		break;
	default:
		err_hw("invalid hw (%d)",hw_id);
		ret = -EINVAL;
		goto p_err;
		break;
	}

	if (valid_hw_slot_id(hw_slot)) {
		dbg_hw("[ID:%d][index:%d] setfile[base:0x%08x][size:%d]\n",
			hw_id, index,
			hardware->hw_ip[hw_slot].setfile_info.table[index].addr,
			hardware->hw_ip[hw_slot].setfile_info.table[index].size);
	}

p_err:
	return ret;
}

int _set_senario_setfile_index(struct fimc_is_hardware *hardware,
	u32 hw_id, u32 scenario, u32 index)
{
	int ret = 0;
	int hw_slot = -1;

	BUG_ON(!hardware);

	switch (hw_id) {
	case DEV_HW_3AA0:
		hw_slot = fimc_is_hw_slot_id(hw_id);
		if (valid_hw_slot_id(hw_slot))
			hardware->hw_ip[hw_slot].setfile_info.index[scenario] = index;

		hw_slot = fimc_is_hw_slot_id(DEV_HW_3AA1);
		if (valid_hw_slot_id(hw_slot))
			hardware->hw_ip[hw_slot].setfile_info.index[scenario] = index;

		hw_slot = fimc_is_hw_slot_id(DEV_HW_ISP0);
		if (valid_hw_slot_id(hw_slot))
			hardware->hw_ip[hw_slot].setfile_info.index[scenario] = index;
		break;
	case DEV_HW_DRC:
	case DEV_HW_DIS:
	case DEV_HW_3DNR:
	case DEV_HW_MCSC:
	case DEV_HW_FD:
		hw_slot = fimc_is_hw_slot_id(hw_id);
		if (valid_hw_slot_id(hw_slot))
			hardware->hw_ip[hw_slot].setfile_info.index[scenario] = index;
		break;
	default:
		err_hw("invalid hw (%d)", hw_id);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int _get_hw_id_by_settfile(u32 id, u32 num, u32 *designed_bit)
{
	int ret = -1;
	u32 d_bit = *designed_bit;

	info_hw("%s: designed_bit(0x%x)\n", __func__, d_bit);

	if (d_bit & SETFILE_DESIGN_BIT_3AA_ISP) {
		ret = DEV_HW_3AA0;
		d_bit &= ~SETFILE_DESIGN_BIT_3AA_ISP;
		goto exit;
	}

	if (d_bit & SETFILE_DESIGN_BIT_DRC) {
		ret = DEV_HW_DRC;
		d_bit &= ~SETFILE_DESIGN_BIT_DRC;
		goto exit;
	}

	if (d_bit & SETFILE_DESIGN_BIT_SCC) {
		/* TODO */
		d_bit &= ~SETFILE_DESIGN_BIT_SCC;
		goto exit;
	}

	if (d_bit & SETFILE_DESIGN_BIT_ODC) {
		/* TODO */
		d_bit &= ~SETFILE_DESIGN_BIT_ODC;
		goto exit;
	}

	if (d_bit & SETFILE_DESIGN_BIT_VDIS) {
		ret = DEV_HW_DIS;
		d_bit &= ~SETFILE_DESIGN_BIT_VDIS;
		goto exit;
	}

	if (d_bit & SETFILE_DESIGN_BIT_TDNR) {
		ret = DEV_HW_3DNR;
		d_bit &= ~SETFILE_DESIGN_BIT_TDNR;
		goto exit;
	}

	if (d_bit & SETFILE_DESIGN_BIT_SCX_MCSC) {
		ret = DEV_HW_MCSC;
		d_bit &= ~SETFILE_DESIGN_BIT_SCX_MCSC;
		goto exit;
	}

	if (d_bit & SETFILE_DESIGN_BIT_FD_VRA) {
		ret = DEV_HW_FD;
		d_bit &= ~SETFILE_DESIGN_BIT_FD_VRA;
		goto exit;
	}

exit:
	info_hw("%s: designed_bit(0x%x)\n", __func__, d_bit);
	*designed_bit = d_bit;

	return ret;
}

void _set_setfile_version(struct fimc_is_hardware *hardware, int version)
{
	int hw_slot = -1;
	enum fimc_is_hardware_id hw_id = DEV_HW_END;

	BUG_ON(!hardware);

	hw_id = DEV_HW_3AA0;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (valid_hw_slot_id(hw_slot))
		hardware->hw_ip[hw_slot].setfile_info.version = version;

	hw_id = DEV_HW_3AA1;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (valid_hw_slot_id(hw_slot))
		hardware->hw_ip[hw_slot].setfile_info.version = version;

	hw_id = DEV_HW_ISP0;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (valid_hw_slot_id(hw_slot))
		hardware->hw_ip[hw_slot].setfile_info.version = version;

	hw_id = DEV_HW_MCSC;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (valid_hw_slot_id(hw_slot))
		hardware->hw_ip[hw_slot].setfile_info.version = version;

	hw_id = DEV_HW_FD;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (valid_hw_slot_id(hw_slot))
		hardware->hw_ip[hw_slot].setfile_info.version = version;
}

int _load_setfile(struct fimc_is_hardware *hardware,
	u32 hw_id, int index, u32 instance, unsigned long hw_path)
{
	int ret = 0;
	int hw_slot = -1;
	struct fimc_is_hw_ip *hw_ip = NULL;

	BUG_ON(!hardware);

	switch (hw_id) {
	case DEV_HW_3AA0:
		/* 3A0, 3A1 and ISP's tuneset is set at the sametime */
		hw_slot = fimc_is_hw_slot_id(hw_id);
		if (valid_hw_slot_id(hw_slot)) {
			hw_ip = &hardware->hw_ip[hw_slot];
			ret = CALL_HW_OPS(hw_ip, load_setfile, index, instance, hw_path);
		}
		hw_slot = fimc_is_hw_slot_id(DEV_HW_3AA1);
		if (valid_hw_slot_id(hw_slot)) {
			hw_ip = &hardware->hw_ip[hw_slot];
			ret = CALL_HW_OPS(hw_ip, load_setfile, index, instance, hw_path);
		}

		hw_slot = fimc_is_hw_slot_id(DEV_HW_ISP0);
		if (valid_hw_slot_id(hw_slot)) {
			hw_ip = &hardware->hw_ip[hw_slot];
			ret = CALL_HW_OPS(hw_ip, load_setfile, index, instance, hw_path);
		}
		break;
	case DEV_HW_MCSC:
	case DEV_HW_FD:
		hw_slot = fimc_is_hw_slot_id(hw_id);
		if (valid_hw_slot_id(hw_slot)) {
			hw_ip = &hardware->hw_ip[hw_slot];
			ret = CALL_HW_OPS(hw_ip, load_setfile, index, instance, hw_path);
		}
		break;
	default:
		break;
	}

	return ret;
}

int fimc_is_hardware_load_setfile(struct fimc_is_hardware *hardware, ulong addr,
	u32 instance, ulong hw_map)
{
	struct fimc_is_setfile_header_v3 *header_v3 = NULL;
	struct fimc_is_setfile_header_v2 *header_v2 = NULL;
	struct fimc_is_setfile_element *setfile_index_table = NULL;
	ulong scenario_table = 0;
	ulong setfile_table = 0;
	ulong setfile_base = 0;
	u32 setfile_num, setfile_index;
	u32 scenario_num, subip_num, setfile_offset;
	int ver = 0, ret = 0;
	int hw_id = 0, i, j;
	u32 designed_bit;

	BUG_ON(!hardware);
	BUG_ON(!addr);

	info_hw("[I:%d]%s: [hw path0x%lx], [addr:0x%lx]\n", instance, __func__,
		hw_map, addr);
	/* 1. decision setfile version */
	/* 2. load header information */
	header_v3 = (struct fimc_is_setfile_header_v3 *)addr;
	if (header_v3->magic_number < SET_FILE_MAGIC_NUMBER) {
		header_v2 = (struct fimc_is_setfile_header_v2 *)addr;
		if (header_v2->magic_number != (SET_FILE_MAGIC_NUMBER - 1)) {
			err_hw("invalid magic number[0x%08x]", header_v2->magic_number);
			return -EINVAL;
		}
		scenario_num = header_v2->scenario_num;
		subip_num = header_v2->subip_num;
		setfile_offset = header_v2->setfile_offset;
		scenario_table = addr + sizeof(struct fimc_is_setfile_header_v2);
		ver = SETFILE_V2;
	} else {
		scenario_num = header_v3->scenario_num;
		subip_num = header_v3->subip_num;
		setfile_offset = header_v3->setfile_offset;
		scenario_table = addr + sizeof(struct fimc_is_setfile_header_v3);

		ver = SETFILE_V3;
		dbg_hw("%s: designed bit: 0x%08x\n", __func__, header_v3->designed_bit);
		dbg_hw("%s: version code: %s\n", __func__, header_v3->version_code);
		dbg_hw("%s: revision code: %s\n", __func__, header_v3->revision_code);
	}
	setfile_base = addr + setfile_offset;
	setfile_table = scenario_table + subip_num * scenario_num * sizeof(u32);
	setfile_index_table = (struct fimc_is_setfile_element *)(setfile_table + \
				(ulong)(subip_num * sizeof(u32)));

	dbg_hw("%s: version: %d\n", __func__, ver);
	dbg_hw("%s: scenario number: %d\n", __func__, scenario_num);
	dbg_hw("%s: subip number: %d\n", __func__, subip_num);
	dbg_hw("%s: offset: 0x%08x\n", __func__, setfile_offset);
	dbg_hw("%s: scenario_table: 0x%lx\n", __func__, scenario_table);
	dbg_hw("%s: setfile base: 0x%lx\n", __func__, setfile_base);
	dbg_hw("%s: setfile_table: 0x%lx\n", __func__, setfile_table);
	dbg_hw("%s: setfile_index_table: 0x%p addr(0x%x) size(0x%x)\n",
		__func__, setfile_index_table,
		setfile_index_table->addr, setfile_index_table->size);

	/* 3. set version */
	_set_setfile_version(hardware, ver);

	designed_bit = header_v3->designed_bit;
	/* 4. set scenaio index, setfile address and size */
	for (i = 0; i < subip_num; i++) {
		hw_id = _get_hw_id_by_settfile(i, subip_num, &designed_bit);
		if (hw_id < 0) {
			err_hw("invalid hw (%d)", hw_id);
			return -EINVAL;
		}

		/* set what setfile index is used at each scenario */
		for (j = 0; j < scenario_num; j++) {
			setfile_index = (u32)*(ulong *)scenario_table;
			if (valid_hw_slot_id(fimc_is_hw_slot_id(hw_id))) {
				ret = _set_senario_setfile_index(hardware, hw_id, j, setfile_index);
				if (ret) {
					err_hw("setting scenario index failed: [ID:%d]", hw_id);
					return -EINVAL;
				}
			}
			scenario_table += sizeof(u32);
		}

		/* set the number of setfile at each sub IP */
		setfile_num = (u32)*(ulong *)setfile_table;
		if (valid_hw_slot_id(fimc_is_hw_slot_id(hw_id)))
			_set_setfile_number(hardware, hw_id, setfile_num);
		setfile_table += sizeof(u32);

		/* set each setfile address and size */
		for (j = 0; j < setfile_num; j++) {
			if (valid_hw_slot_id(fimc_is_hw_slot_id(hw_id))) {
				_set_setfile_table(hardware, hw_id,
					(ulong)(setfile_base + setfile_index_table->addr),
					setfile_index_table->size, j);
				/* load setfile */
				_load_setfile(hardware, hw_id, j, instance, hw_map);
			}
			setfile_index_table++;;
		}
	}

	return ret;
}

int fimc_is_hardware_apply_setfile(struct fimc_is_hardware *hardware,
	u32 instance, u32 group, u32 sub_mode, unsigned long hw_path)
{
	int ret = 0;
	int hw_slot = -1;
	struct fimc_is_hw_ip *hw_ip = NULL;
	enum fimc_is_hardware_id hw_id = DEV_HW_END;

	BUG_ON(!hardware);

	hw_id = DEV_HW_3AA0;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (valid_hw_slot_id(hw_slot)) {
		hw_ip = &hardware->hw_ip[hw_slot];
		ret = CALL_HW_OPS(hw_ip, apply_setfile, sub_mode, instance, hw_path);
		if (ret) {
			err_hw("[%d]apply_setfile fail (%d)", instance, hw_id);
			ret = -EINVAL;
			goto exit;
		}
	}

	hw_id = DEV_HW_ISP0;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (valid_hw_slot_id(hw_slot)) {
		hw_ip = &hardware->hw_ip[hw_slot];
		ret = CALL_HW_OPS(hw_ip, apply_setfile, sub_mode, instance, hw_path);
		if (ret) {
			err_hw("[%d]apply_setfile fail (%d)", instance, hw_id);
			ret = -EINVAL;
			goto exit;
		}
	}

	hw_id = DEV_HW_MCSC;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (valid_hw_slot_id(hw_slot)) {
		hw_ip = &hardware->hw_ip[hw_slot];
		ret = CALL_HW_OPS(hw_ip, apply_setfile, sub_mode, instance, hw_path);
		if (ret) {
			err_hw("[%d]apply_setfile fail (%d)", instance, hw_id);
			ret = -EINVAL;
			goto exit;
		}
	}

	hw_id = DEV_HW_FD;
	hw_slot = fimc_is_hw_slot_id(hw_id);
	if (valid_hw_slot_id(hw_slot)) {
		hw_ip = &hardware->hw_ip[hw_slot];
		ret = CALL_HW_OPS(hw_ip, apply_setfile, sub_mode, instance, hw_path);
		if (ret) {
			err_hw("[%d]apply_setfile fail (%d)", instance, hw_id);
			ret = -EINVAL;
			goto exit;
		}
	}
exit:
	return ret;
}

int fimc_is_hardware_delete_setfile(struct fimc_is_hardware *hardware,
	u32 instance, unsigned long hw_path)
{
	int ret = 0;
	int hw_slot = -1;
	struct fimc_is_hw_ip *hw_ip = NULL;
	enum fimc_is_hardware_id hw_id = DEV_HW_END;

	BUG_ON(!hardware);

	info_hw("delete_setfile: hw_path (0x%lx)\n", hw_path);
	for (hw_slot = 0; hw_slot < HW_SLOT_MAX; hw_slot++) {
		hw_ip = &hardware->hw_ip[hw_slot];
		ret = CALL_HW_OPS(hw_ip, delete_setfile, instance, hw_path);
		if (ret) {
			err_hw("[%d]delete_setfile fail (%d)", instance, hw_id);
			ret = -EINVAL;
			goto exit;
		}
		test_and_clear_bit(HW_TUNESET, &hw_ip->state);
	}

exit:
	return ret;
}
