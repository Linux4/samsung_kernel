// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo Image Subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-hw-control.h"
#include "pablo-obte.h"
#define GET_HEAD_GROUP_IN_DEVICE(type, group) ((group)->head)

static void is_hardware_frame_start(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_frame *frame, *check_frame;
	struct is_framemgr *framemgr = hw_ip->framemgr;

	framemgr_e_barrier(framemgr, 0);
	frame = peek_frame(framemgr, FS_HW_CONFIGURE);

	if (!frame) {
		check_frame = find_frame(framemgr, FS_HW_WAIT_DONE,
			frame_fcount, (void *)(ulong)atomic_read(&hw_ip->fcount));
		if (check_frame) {
			msdbgs_hw(2, "[F:%d] already processed to HW_WAIT_DONE state",
					instance, hw_ip, check_frame->fcount);

			framemgr_x_barrier(framemgr, 0);
		} else {
			/* error happened..print the frame info */
			frame_manager_print_info_queues(framemgr);
			print_all_hw_frame_count(hw_ip->hardware);
			framemgr_x_barrier(framemgr, 0);
			mserr_hw("FSTART frame null", instance, hw_ip);
		}
		atomic_set(&hw_ip->status.Vvalid, V_VALID);
		return;
	}

	frame->frame_info[INFO_FRAME_START].cpu = raw_smp_processor_id();
	frame->frame_info[INFO_FRAME_START].pid = current->pid;
	frame->frame_info[INFO_FRAME_START].when = local_clock();

	trans_frame(framemgr, frame, FS_HW_WAIT_DONE);
	framemgr_x_barrier(framemgr, 0);
}

static int is_hardware_frame_done(struct is_hw_ip *hw_ip, struct is_frame *frame,
	int wq_id, u32 output_id, enum ShotErrorType done_type, bool get_meta)
{
	struct is_framemgr *framemgr = hw_ip->framemgr;
	u32 hw_fe_cnt = atomic_read(&hw_ip->fcount);
	struct is_group *group, *head;
	ulong flags = 0;
	struct is_hw_ip *ldr_hw_ip;
	u32 instance;

	switch (done_type) {
	case IS_SHOT_SUCCESS:
		if (!frame) {
			framemgr_e_barrier_common(framemgr, 0, flags);
			frame = peek_frame(framemgr, FS_HW_WAIT_DONE);
			framemgr_x_barrier_common(framemgr, 0, flags);
		} else {
			sdbg_hw(2, "frame NOT null!!(%d)", hw_ip, done_type);
		}

		if (IS_RUNNING_TUNING_SYSTEM() && frame)
			pablo_obte_regdump(frame->instance, hw_ip->id,
					frame->stripe_info.region_id,
					frame->stripe_info.region_num);
		break;
	case IS_SHOT_UNPROCESSED:
	case IS_SHOT_TIMEOUT:
		break;
	default:
		serr_hw("invalid done_type(%d)", hw_ip, done_type);
		return -EINVAL;
	}

	if (!frame) {
		serr_hw("[F:%d]frame_done: frame(null)!!(%d)(0x%x)", hw_ip,
			hw_fe_cnt, done_type, output_id);
		framemgr_e_barrier_common(framemgr, 0, flags);
		frame_manager_print_info_queues(framemgr);
		print_all_hw_frame_count(hw_ip->hardware);
		framemgr_x_barrier_common(framemgr, 0, flags);
		return -EINVAL;
	}

	instance = frame->instance;
	group = hw_ip->group[instance];
	if (!group) {
		mserr_hw("group is null", instance, hw_ip);
		return -EINVAL;
	}

	head = group->head;

	msdbgs_hw(2, "[0x%x]frame_done [F:%d][HWF:%d][B:0x%lx][C:0x%lx]\n",
		instance, hw_ip, output_id, frame->fcount, hw_fe_cnt,
		frame->bak_flag, frame->core_flag);

	if (frame->shot)
		CALL_HWIP_OPS(hw_ip, get_meta, frame, hw_ip->hardware->hw_map[instance]);

	if (atomic_read(&hw_ip->status.Vvalid) == V_BLANK)
		msdbg_hw(1, "[F%d][HF%d] already in VBlank", frame->instance, hw_ip,
				frame->fcount, hw_fe_cnt);
	else
		atomic_set(&hw_ip->status.Vvalid, V_BLANK);

	framemgr_e_barrier_common(framemgr, 0, flags);
	clear_bit(hw_ip->id, &frame->core_flag);
	if (!frame->core_flag) {
		framemgr_x_barrier_common(framemgr, 0, flags);

		frame->frame_info[INFO_FRAME_END_PROC].cpu = raw_smp_processor_id();
		frame->frame_info[INFO_FRAME_END_PROC].pid = current->pid;
		frame->frame_info[INFO_FRAME_END_PROC].when = local_clock();

		ldr_hw_ip = is_get_hw_ip(head->id, hw_ip->hardware);
		if (ldr_hw_ip)
			del_timer(&ldr_hw_ip->shot_timer);

		return is_hardware_shot_done(hw_ip, frame, framemgr, done_type);
	}
	framemgr_x_barrier_common(framemgr, 0, flags);

	return 0;
}

static const struct is_hardware_ops is_hardware_m2m_ops = {
	.frame_start	= is_hardware_frame_start,
	.frame_done	= is_hardware_frame_done,
	.frame_ndone	= is_hardware_frame_ndone,
	.flush_frame	= is_hardware_flush_frame,
	.dbg_trace	= _is_hw_frame_dbg_trace,
};

const struct is_hardware_ops *is_hw_get_m2m_ops(void)
{
	return &is_hardware_m2m_ops;
}

static inline int _is_hardware_shot(struct is_hardware *hardware,
	struct is_group *head, struct is_frame *frame,
	struct is_framemgr *framemgr, u32 framenum)
{
	int ret = 0;
	struct is_hw_ip *hw_ip = NULL, *ldr_hw_ip = NULL;
	enum is_hardware_id hw_id = DEV_HW_END;
	struct is_group *child = NULL;
	ulong flags = 0;
	int hw_list[GROUP_HW_MAX], hw_index, hw_slot;
	int hw_maxnum = 0;
	ulong hw_map;
	u32 instance;

	ldr_hw_ip = is_get_hw_ip(head->id, hardware);
	if (!ldr_hw_ip) {
		mgerr("invalid ldr_hw_ip", head, head);
		return -EINVAL;
	}

	child = head->tail;
	while (child && (child->device_type == IS_DEVICE_ISCHAIN)) {
		hw_maxnum = is_get_hw_list(child->id, hw_list);
		instance = child->instance;

		for (hw_index = hw_maxnum - 1; hw_index >= 0; hw_index--) {
			hw_id = hw_list[hw_index];
			hw_slot = is_hw_slot_id(hw_id);
			if (!valid_hw_slot_id(hw_slot)) {
				merr_hw("invalid slot (%d,%d)", instance,
					hw_id, hw_slot);
				ret = -EINVAL;
				goto shot_err_cancel;
			}
			hw_ip = &hardware->hw_ip[hw_slot];
			atomic_set(&hw_ip->fcount, framenum);
			atomic_set(&hw_ip->instance, instance);

			hw_ip->framemgr = &hardware->framemgr[ldr_hw_ip->id];

			hw_map = hardware->hw_map[instance];
			CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, frame->fcount, DEBUG_POINT_HW_SHOT_E);
			ret = CALL_HWIP_OPS(hw_ip, shot, frame, hw_map);
			CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, frame->fcount, DEBUG_POINT_HW_SHOT_X);
			if (ret) {
				mserr_hw("shot fail (%d)[F:%d]", instance, hw_ip,
					hw_slot, frame->fcount);
				goto shot_err_cancel;
			}
		}
		child = child->parent;
	}

	return ret;

shot_err_cancel:
	mswarn_hw("[F:%d] Canceled by hardware shot err", instance, hw_ip, frame->fcount);

	framemgr_e_barrier_common(framemgr, 0, flags);
	trans_frame(framemgr, frame, FS_HW_FREE);
	framemgr_x_barrier_common(framemgr, 0, flags);

	if (child && child->tail) {
		struct is_group *restore_grp = child->tail;

		while (restore_grp && (restore_grp->id != child->id)) {
			is_hardware_restore_by_group(hardware, restore_grp, restore_grp->instance);
			restore_grp = restore_grp->parent;
		}
	}

	return ret;
}

static int is_hardware_shot_prepare(struct is_hardware *hardware,
	struct is_group *group, struct is_frame *frame)
{
	u32 instance, fcount;
	struct is_group *head;
	struct is_hw_ip *head_hw_ip;

	instance = group->instance;
	fcount = frame->fcount;

	head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, group);
	head_hw_ip = is_get_hw_ip(head->id, hardware);
	if (!head_hw_ip) {
		mgerr("invalid head_hw_ip", head, head);
		return -EINVAL;
	}

	is_hw_update_prfi(hardware, group, frame);

	msdbg_hw(1, "[F%d][G:%s][S:%s]shot_prepare [C:0x%lx]\n", instance, head_hw_ip,
			frame->fcount, group_id_name[group->id],
			hw_frame_state_name[frame->state],
			frame->core_flag);

	return 0;
}

static int is_hw_m2m_grp_shot(struct is_hardware *hardware, u32 instance,
	struct is_group *group, struct is_frame *frame, ulong hw_map)
{
	int ret = 0;
	struct is_hw_ip *hw_ip = NULL;
	struct is_frame *hw_frame;
	struct is_framemgr *framemgr;
	struct is_group *head;
	ulong flags = 0;
	int num_buffers;
	bool reset;
	int i;
	u32 shot_timeout;

	FIMC_BUG(!hardware);
	FIMC_BUG(!frame);
	FIMC_BUG(instance >= IS_STREAM_COUNT);


	head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, group);

	hw_ip = is_get_hw_ip(head->id, hardware);
	if (!hw_ip) {
		mgerr("invalid hw_ip", head, head);
		return -EINVAL;
	}

	if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
		msinfo_hw("grp_shot [F:%d][G:0x%lx][B:0x%lx][O:0x%lx][dva:%pad]\n",
			instance, hw_ip,
			frame->fcount, GROUP_ID(head->id),
			frame->bak_flag, frame->out_flag, &frame->dvaddr_buffer[0]);

	hw_ip->framemgr = &hardware->framemgr[hw_ip->id];
	framemgr = hw_ip->framemgr;

	framemgr_e_barrier_irqs(framemgr, 0, flags);

	hw_frame = get_frame(framemgr, FS_HW_FREE);
	if (hw_frame == NULL) {
		framemgr_x_barrier_irqr(framemgr, 0, flags);
		merr_hw("[G:0x%lx]free_head(NULL)", instance, GROUP_ID(head->id));
		return -EINVAL;
	}

	num_buffers = frame->num_buffers;
	reset = (num_buffers > 1) ? 0 : 1;
	is_hardware_fill_frame_info(instance, hw_frame, frame, head, hardware, reset);
	frame->type = SHOT_TYPE_EXTERNAL;
	hw_frame->type = frame->type;

	/* multi-buffer */
	hw_frame->planes	= frame->planes;
	hw_frame->num_buffers	= num_buffers;
	hw_frame->cur_buf_index	= 0;
	framemgr->batch_num = num_buffers;

	/* for NI (noise index) */
	hw_frame->noise_idx = frame->noise_idx;

	put_frame(framemgr, hw_frame, FS_HW_REQUEST);

	if (num_buffers > 1) {
		if (SUPPORT_HW_FRO(head->id)) {
			hw_ip->hw_fro_en = true;
		} else {
			hw_ip->hw_fro_en = false;
			hw_frame->type = SHOT_TYPE_MULTI;
			hw_frame->planes = 1;
			hw_frame->num_buffers = 1;
			hw_frame->cur_buf_index = 0;

			for (i = 1; i < num_buffers; i++) {
				hw_frame = get_frame(framemgr, FS_HW_FREE);
				if (hw_frame == NULL) {
					framemgr_x_barrier_irqr(framemgr, 0, flags);
					err_hw("[F%d]free_head(NULL)", frame->fcount);
					return -EINVAL;
				}

				reset = (i < (num_buffers - 1)) ? 0 : 1;
				is_hardware_fill_frame_info(instance, hw_frame, frame,
								head, hardware, reset);
				hw_frame->type = SHOT_TYPE_MULTI;
				hw_frame->planes = 1;
				hw_frame->num_buffers = 1;
				hw_frame->cur_buf_index = i;

				put_frame(framemgr, hw_frame, FS_HW_REQUEST);
			}
			hw_frame->type = frame->type; /* last buffer */
		}
	} else {
		hw_ip->hw_fro_en = false;
	}
	msdbg_hw(2, "ischain batch_num(%d), HW FRO(%d)\n", instance, hw_ip,
		num_buffers, hw_ip->hw_fro_en);
	/* set shot timer in DMA operation */
	shot_timeout = head->device->resourcemgr->shot_timeout;
	mod_timer(&hw_ip->shot_timer, jiffies + msecs_to_jiffies(shot_timeout));

	hw_frame = peek_frame(framemgr, FS_HW_REQUEST);
	trans_frame(framemgr, hw_frame, FS_HW_CONFIGURE);

	framemgr_x_barrier_irqr(framemgr, 0, flags);

	is_set_hw_count(hardware, head, instance, frame->fcount, num_buffers, hw_map);
	is_hardware_shot_prepare(hardware, group, frame);
	ret = _is_hardware_shot(hardware, head, hw_frame, framemgr, frame->fcount);
	if (ret) {
		mserr_hw("hardware_shot fail [G:0x%lx]", instance, hw_ip, GROUP_ID(head->id));
		return -EINVAL;
	}

	return ret;
}

static const struct is_hw_group_ops is_hw_group_ops_m2m = {
	.shot = is_hw_m2m_grp_shot,
};

const struct is_hw_group_ops *is_hw_get_m2m_group_ops(void)
{
	return &is_hw_group_ops_m2m;
}
EXPORT_SYMBOL_GPL(is_hw_get_m2m_group_ops);
