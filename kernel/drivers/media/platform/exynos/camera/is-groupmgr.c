/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is group manager functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>

#include "exynos-is.h"
#include "is-interface-wrap.h"
#include "is-votfmgr.h"
#include "is-type.h"
#include "is-core.h"
#include "is-err.h"
#include "is-video.h"
#include "pablo-framemgr.h"
#include "is-groupmgr.h"
#include "is-devicemgr.h"
#include "is-cmd.h"
#include "pablo-debug.h"
#include "pablo-dvfs.h"
#include "pablo-fpsimd.h"
#include "is-hw.h"
#include "is-hw-dvfs.h"
#include "is-vendor.h"
#if defined(CONFIG_CAMERA_PDP)
#include "is-interface-sensor.h"
#include "is-device-sensor-peri.h"
#endif
#include "pablo-sync-reprocessing.h"

static struct is_groupmgr grpmgr;

struct is_groupmgr *is_get_groupmgr(void)
{
	return &grpmgr;
}
KUNIT_EXPORT_SYMBOL(is_get_groupmgr);

static inline void smp_shot_init(struct is_group *group, u32 value)
{
	atomic_set(&group->smp_shot_count, value);
}

static inline int smp_shot_get(struct is_group *group)
{
	return atomic_read(&group->smp_shot_count);
}

static inline void smp_shot_inc(struct is_group *group)
{
	atomic_inc(&group->smp_shot_count);
}

static inline void smp_shot_dec(struct is_group *group)
{
	atomic_dec(&group->smp_shot_count);
}

int is_group_lock(struct is_group *group, enum is_device_type device_type, bool leader_lock,
	unsigned long *flags)
{
	u32 entry;
	struct is_subdev *subdev;
	struct is_framemgr *ldr_framemgr, *sub_framemgr;
	struct is_group *pos;

	FIMC_BUG(!group);
	FIMC_BUG(!flags);

	*flags = 0;

	if (leader_lock) {
		ldr_framemgr = GET_HEAD_GROUP_FRAMEMGR(group);
		if (!ldr_framemgr) {
			mgerr("ldr_framemgr is NULL", group, group);
			BUG();
		}
		framemgr_e_barrier_irqs(ldr_framemgr, FMGR_IDX_20, *flags);
	}

	for_each_group_child(pos, group) {
		/* skip the lock by device type */
		if (pos->device_type != device_type &&
				device_type != IS_DEVICE_MAX)
			break;

		for (entry = 0; entry < ENTRY_END; ++entry) {
			subdev = pos->subdev[entry];
			pos->locked_sub_framemgr[entry] = NULL;

			if (!subdev)
				continue;

			if (&pos->leader == subdev)
				continue;

			sub_framemgr = GET_SUBDEV_FRAMEMGR(subdev);
			if (!sub_framemgr)
				continue;

			if (!test_bit(IS_SUBDEV_START, &subdev->state))
				continue;

			framemgr_e_barrier(sub_framemgr, FMGR_IDX_19);
			pos->locked_sub_framemgr[entry] = sub_framemgr;
		}
	}

	return 0;
}

void is_group_unlock(struct is_group *group, unsigned long flags,
		enum is_device_type device_type,
		bool leader_lock)
{
	u32 entry;
	struct is_framemgr *ldr_framemgr;
	struct is_group *pos;

	FIMC_BUG_VOID(!group);

	if (leader_lock) {
		ldr_framemgr = GET_HEAD_GROUP_FRAMEMGR(group);
		if (!ldr_framemgr) {
			mgerr("ldr_framemgr is NULL", group, group);
			BUG();
		}
	}

	for_each_group_child(pos, group) {
		/* skip the unlock by device type */
		if (pos->device_type != device_type &&
				device_type != IS_DEVICE_MAX)
			break;

		for (entry = 0; entry < ENTRY_END; ++entry) {
			if (pos->locked_sub_framemgr[entry]) {
				framemgr_x_barrier(pos->locked_sub_framemgr[entry],
							FMGR_IDX_19);
				pos->locked_sub_framemgr[entry] = NULL;
			}
		}
	}

	if (leader_lock)
		framemgr_x_barrier_irqr(ldr_framemgr, FMGR_IDX_20, flags);
}

void is_group_subdev_cancel(struct is_group *group,
		struct is_frame *ldr_frame,
		enum is_device_type device_type,
		enum is_frame_state frame_state,
		bool flush)
{
	struct is_subdev *subdev;
	struct is_video_ctx *sub_vctx;
	struct is_framemgr *sub_framemgr;
	struct is_frame *sub_frame;
	struct is_group *pos;
	struct camera2_node *cap_node;
	u32 cid;

	for_each_group_child(pos, group) {
		/* skip the subdev device by device type */
		if (pos->device_type != device_type &&
				device_type != IS_DEVICE_MAX)
			break;

		list_for_each_entry(subdev, &pos->subdev_list, list) {
			sub_vctx = subdev->vctx;
			if (!sub_vctx)
				continue;

			sub_framemgr = GET_FRAMEMGR(sub_vctx);
			if (!sub_framemgr)
				continue;

			if (!test_bit(IS_SUBDEV_START, &subdev->state))
				continue;

			/* Find cid of matching node_group with this subdev */
			for (cid = 0; cid < CAPTURE_NODE_MAX; cid++) {
				cap_node = &ldr_frame->shot_ext->node_group.capture[cid];
				if (cap_node->vid == subdev->vid)
					break;
			}

			if (cid >= CAPTURE_NODE_MAX || !cap_node->request)
				continue;

			do {
				sub_frame = peek_frame(sub_framemgr, frame_state);
				if (sub_frame) {
					if (sub_frame->fcount > ldr_frame->fcount) {
						msrinfo("[WRN] SKIP CANCEL(SBUF:%d > LDRF:%d)(%d, %d)\n",
								pos, subdev, ldr_frame,
								sub_frame->fcount, ldr_frame->fcount,
								sub_frame->index, sub_frame->state);
						break;
					}
					if (sub_frame->stream) {
						sub_frame->stream->fvalid = 0;
						sub_frame->stream->fcount = ldr_frame->fcount;
						sub_frame->stream->rcount = ldr_frame->rcount;
					}
					clear_bit(subdev->id, &ldr_frame->out_flag);
					trans_frame(sub_framemgr, sub_frame, FS_COMPLETE);
					msrinfo("[ERR] CANCEL(%d, %d)\n", pos, subdev, ldr_frame,
								sub_frame->index, sub_frame->state);
					CALL_VOPS(sub_vctx, done, sub_frame->index, VB2_BUF_STATE_ERROR);
				} else {
					msrinfo("[ERR] There's no frame\n", pos, subdev, ldr_frame);
#ifdef DEBUG
					frame_manager_print_queues(sub_framemgr);
#endif
				}
			} while (sub_frame && flush);
		}
	}
}

static void is_group_cancel(struct is_group *group,
	struct is_frame *ldr_frame)
{
	u32 wait_count = 300;
	unsigned long flags;
	struct is_video_ctx *ldr_vctx;
	struct is_framemgr *ldr_framemgr;
	struct is_frame *prev_frame, *next_frame;

	ldr_vctx = group->head->leader.vctx;
	ldr_framemgr = GET_FRAMEMGR(ldr_vctx);
	if (!ldr_framemgr) {
		mgerr("ldr_framemgr is NULL", group, group);
		BUG();
	}

p_retry:
	if (!ldr_framemgr->num_frames) {
		mgerr("ldr_framemgr is already closed", group, group);
		return;
	}

	if (is_group_lock(group, IS_DEVICE_MAX, true, &flags))
		return;

	next_frame = peek_frame_tail(ldr_framemgr, FS_FREE);
	if (wait_count && next_frame && next_frame->out_flag) {
		mginfo("next frame(F%d) is on process1(%lX %lX), waiting...\n", group, group,
			next_frame->fcount, next_frame->bak_flag, next_frame->out_flag);
		is_group_unlock(group, flags, IS_DEVICE_MAX, true);
		usleep_range(1000, 1000);
		wait_count--;
		goto p_retry;
	}

	next_frame = peek_frame_tail(ldr_framemgr, FS_COMPLETE);
	if (wait_count && next_frame && next_frame->out_flag) {
		mginfo("next frame(F%d) is on process2(%lX %lX), waiting...\n", group, group,
			next_frame->fcount, next_frame->bak_flag, next_frame->out_flag);
		is_group_unlock(group, flags, IS_DEVICE_MAX, true);
		usleep_range(1000, 1000);
		wait_count--;
		goto p_retry;
	}

	prev_frame = peek_frame(ldr_framemgr, FS_PROCESS);
	if (wait_count && prev_frame && prev_frame->bak_flag != prev_frame->out_flag) {
		mginfo("prev frame(F%d) is on process(%lX %lX), waiting...\n", group, group,
			prev_frame->fcount, prev_frame->bak_flag, prev_frame->out_flag);
		is_group_unlock(group, flags, IS_DEVICE_MAX, true);
		usleep_range(1000, 1000);
		wait_count--;
		goto p_retry;
	}

	if (ldr_frame->state == FS_COMPLETE || ldr_frame->state == FS_FREE) {
		mgrwarn("Can't cancel this frame because state(%s) is invalid, skip buffer done",
			group, group, ldr_frame,
			frame_state_name[ldr_frame->state]);
		is_group_unlock(group, flags, IS_DEVICE_MAX, true);
		return;
	}

	is_group_subdev_cancel(group, ldr_frame, IS_DEVICE_MAX, FS_REQUEST, false);

	clear_bit(group->leader.id, &ldr_frame->out_flag);
	trans_frame(ldr_framemgr, ldr_frame, FS_COMPLETE);

	mgrinfo("[ERR] CANCEL(i%d)(R%d, P%d, C%d)\n", group, group, ldr_frame,
		ldr_frame->index,
		ldr_framemgr->queued_count[FS_REQUEST],
		ldr_framemgr->queued_count[FS_PROCESS],
		ldr_framemgr->queued_count[FS_COMPLETE]);

	CALL_VOPS(ldr_vctx, done, ldr_frame->index, VB2_BUF_STATE_ERROR);

	is_group_unlock(group, flags, IS_DEVICE_MAX, true);
}

static void is_group_s_leader(struct is_group *group,
	struct is_subdev *leader, bool force)
{
	struct is_subdev *subdev;

	FIMC_BUG_VOID(!group);
	FIMC_BUG_VOID(!leader);

	subdev = &group->leader;
	subdev->leader = leader;

	list_for_each_entry(subdev, &group->subdev_list, list) {
		/*
		 * TODO: Remove this error check logic.
		 * For MC-scaler group, this warning message could be printed
		 * because each capture node is shared by different output node.
		 */
		if (leader->vctx && subdev->vctx &&
			(leader->vctx->refcount < subdev->vctx->refcount)) {
			mgwarn("Invalid subdev instance (%s(%u) < %s(%u))",
				group, group,
				leader->name, leader->vctx->refcount,
				subdev->name, subdev->vctx->refcount);
		}

		if (force || test_bit(IS_SUBDEV_OPEN, &subdev->state))
			subdev->leader = leader;
	}
}

#ifdef DEBUG_AA
static void is_group_debug_aa_shot(struct is_group *group,
	struct is_frame *ldr_frame)
{
	if (group->prev)
		return;

#ifdef DEBUG_FLASH
	if (ldr_frame->shot->ctl.aa.vendor_aeflashMode != group->flashmode) {
		group->flashmode = ldr_frame->shot->ctl.aa.vendor_aeflashMode;
		info("flash ctl : %d(%d)\n", group->flashmode, ldr_frame->fcount);
	}
#endif
}

static void is_group_debug_aa_done(struct is_group *group,
	struct is_frame *ldr_frame)
{
	if (group->prev)
		return;

#ifdef DEBUG_FLASH
	if (ldr_frame->shot->dm.flash.vendor_firingStable != group->flash.vendor_firingStable) {
		group->flash.vendor_firingStable = ldr_frame->shot->dm.flash.vendor_firingStable;
		info("flash stable : %d(%d)\n", group->flash.vendor_firingStable, ldr_frame->fcount);
	}

	if (ldr_frame->shot->dm.flash.vendor_flashReady!= group->flash.vendor_flashReady) {
		group->flash.vendor_flashReady = ldr_frame->shot->dm.flash.vendor_flashReady;
		info("flash ready : %d(%d)\n", group->flash.vendor_flashReady, ldr_frame->fcount);
	}

	if (ldr_frame->shot->dm.flash.vendor_flashOffReady!= group->flash.vendor_flashOffReady) {
		group->flash.vendor_flashOffReady = ldr_frame->shot->dm.flash.vendor_flashOffReady;
		info("flash off : %d(%d)\n", group->flash.vendor_flashOffReady, ldr_frame->fcount);
	}
#endif
}
#endif

static bool is_group_kthread_queue_work(struct is_group_task *gtask, struct is_group *group, struct is_frame *frame)
{
	TIME_SHOT(TMS_Q);
#ifdef VH_FPSIMD_API
	is_fpsimd_set_task_using(gtask->task, &gtask->fp_state);
#endif
	return kthread_queue_work(&gtask->worker, &frame->work);
}

static void is_group_start_trigger(struct is_groupmgr *groupmgr,
	struct is_group *group,
	struct is_frame *frame)
{
	struct is_group_task *gtask;
	bool do_sync;

	FIMC_BUG_VOID(!groupmgr);
	FIMC_BUG_VOID(!group);
	FIMC_BUG_VOID(!frame);
	FIMC_BUG_VOID(group->id >= GROUP_ID_MAX);

	atomic_inc(&group->rcount);

	gtask = &groupmgr->gtask[group->id];
	do_sync = CALL_SYNC_REPRO_OPS(gtask->sync_repro, start_trigger, gtask, group, frame);
	if (!do_sync)
		is_group_kthread_queue_work(gtask, group, frame);
}

static int is_group_task_probe(struct is_group_task *gtask,
	u32 id)
{
	gtask->id = id;
	atomic_set(&gtask->refcount, 0);
	clear_bit(IS_GTASK_START, &gtask->state);
	clear_bit(IS_GTASK_REQUEST_STOP, &gtask->state);
	spin_lock_init(&gtask->gtask_slock);

	return is_hw_group_task_cfg(gtask, id);
}

static int is_group_task_start(struct is_group_task *gtask, int slot)
{
	int ret = 0;
	struct sched_param param;

	if (test_bit(IS_GTASK_START, &gtask->state))
		goto p_work;

	sema_init(&gtask->smp_resource, 0);
	kthread_init_worker(&gtask->worker);
	gtask->task =
		kthread_run(kthread_worker_fn, &gtask->worker, "%s", group_id_name[gtask->id]);
	if (IS_ERR(gtask->task)) {
		err("failed to create group_task%d, err(%ld)\n",
			gtask->id, PTR_ERR(gtask->task));
		ret = PTR_ERR(gtask->task);
		goto p_err;
	}

	param.sched_priority = (slot == GROUP_SLOT_SENSOR) ?
		TASK_GRP_OTF_INPUT_PRIO : TASK_GRP_DMA_INPUT_PRIO;

	ret = sched_setscheduler_nocheck(gtask->task, SCHED_FIFO, &param);
	if (ret) {
		err("sched_setscheduler_nocheck is fail(%d)", ret);
		goto p_err;
	}

#ifndef VH_FPSIMD_API
	is_fpsimd_set_task_using(gtask->task, NULL);
#endif

	CALL_SYNC_REPRO_OPS(gtask->sync_repro, init, gtask);

	/* default gtask's smp_resource value is 1 for guerrentee m2m IP operation */
	sema_init(&gtask->smp_resource, 1);
	set_bit(IS_GTASK_START, &gtask->state);

p_work:
	atomic_inc(&gtask->refcount);

p_err:
	return ret;
}

static int is_group_task_stop(struct is_group_task *gtask)
{
	int ret = 0;
	u32 refcount;

	if (!test_bit(IS_GTASK_START, &gtask->state)) {
		err("gtask(%d) is not started", gtask->id);
		ret = -EINVAL;
		goto p_err;
	}

	if (IS_ERR_OR_NULL(gtask->task)) {
		err("task of gtask(%d) is invalid(%p)", gtask->id, gtask->task);
		ret = -EINVAL;
		goto p_err;
	}

	refcount = atomic_dec_return(&gtask->refcount);
	if (refcount > 0)
		goto p_err;

	set_bit(IS_GTASK_REQUEST_STOP, &gtask->state);

	if (!list_empty(&gtask->smp_resource.wait_list)) {
		warn("gtask(%d) is not empty", gtask->id);
#ifdef ENABLE_KERNEL_LOG_DUMP
		is_kernel_log_dump(&is_get_is_core()->resourcemgr, false);
#endif
		up(&gtask->smp_resource);
	}

	/*
	 * flush kthread wait until all work is complete
	 * it's dangerous if all is not finished
	 * so it's commented currently
	 * flush_kthread_worker(&groupmgr->group_worker[slot]);
	 */
	kthread_stop(gtask->task);

	clear_bit(IS_GTASK_REQUEST_STOP, &gtask->state);
	clear_bit(IS_GTASK_START, &gtask->state);

p_err:
	return ret;
}

struct is_group *get_ischain_leader_group(u32 instance)
{
	struct is_group *leader_group = grpmgr.leader[instance];

	if ((leader_group->device_type == IS_DEVICE_SENSOR)
		&& (leader_group->next)
		&& (!test_bit(IS_GROUP_OTF_INPUT, &leader_group->next->state))) {
		leader_group = leader_group->next;
	}

	return leader_group;
}

struct is_group *get_leader_group(u32 instance)
{
	return grpmgr.leader[instance];
}
PST_EXPORT_SYMBOL(get_leader_group);

struct is_group_task *get_group_task(u32 gid)
{
	return &grpmgr.gtask[gid];
}
KUNIT_EXPORT_SYMBOL(get_group_task);

int is_groupmgr_probe(void)
{
	int ret = 0;
	u32 stream, slot, id;

	for (stream = 0; stream < IS_STREAM_COUNT; ++stream) {
		grpmgr.leader[stream] = NULL;
		for (slot = 0; slot < GROUP_SLOT_MAX; ++slot)
			grpmgr.group[stream][slot] = NULL;
	}

	for (id = 0; id < GROUP_ID_MAX; ++id) {
		ret = is_group_task_probe(&grpmgr.gtask[id], id);
		if (ret) {
			err("is_group_task_probe is fail(%d)", ret);
			goto p_err;
		}
	}

p_err:
	return ret;
}

static int is_group_path_dmsg(struct is_group *leader_group)
{
	struct is_group *group, *next;
	struct is_subdev *subdev;

	if (test_bit(IS_GROUP_REPROCESSING, &leader_group->state)) {
		is_dmsg_init();
		is_dmsg_concate("STM(R) PH:");
	} else if (test_bit(IS_GROUP_USE_MULTI_CH, &leader_group->state)) {
		is_dmsg_concate("STM(N:2) PH:");
	} else {
		is_dmsg_init();
		is_dmsg_concate("STM(N) PH:");
	}

	/*
	 * The Meaning of Symbols.
	 *  1) -> : HAL(OTF), Driver(OTF)
	 *  2) => : HAL(M2M), Driver(M2M)
	 *  3) ~> : HAL(OTF), Driver(M2M)
	 *  4) >> : HAL(OTF), Driver(M2M)
	 *  5) *> : HAL(VOTF), Driver(VOTF)
	 *          It's Same with 3).
	 *          But HAL q/dq junction node between the groups.
	 *  6) ^> : HAL(OTF_2nd), Driver(OTF_2nd)
	 */
	if (test_bit(IS_GROUP_OTF_INPUT, &leader_group->state)) {
		if (test_bit(IS_GROUP_VOTF_INPUT, &leader_group->state))
			is_dmsg_concate(" *> ");
		else if (test_bit(IS_GROUP_USE_MULTI_CH, &leader_group->state))
			is_dmsg_concate(" ^> ");
		else
			is_dmsg_concate(" -> ");
	} else {
		is_dmsg_concate(" => ");
	}

	group = leader_group;
	while (group) {
		FIMC_BUG(group->slot >= GROUP_SLOT_MAX);

		next = group->next;
		if (next)
			FIMC_BUG(next->slot >= GROUP_SLOT_MAX);

		group->junction = &group->leader;

		is_dmsg_concate("GP%s ( ", group_id_name[group->id]);
		list_for_each_entry(subdev, &group->subdev_list, list)
			is_dmsg_concate("%02d ", subdev->id);
		is_dmsg_concate(")");

		if (next && !group->junction) {
			mgerr("junction subdev can NOT be found", group, group);
			return -EINVAL;
		}

		if (next) {
			if (test_bit(IS_GROUP_OTF_INPUT, &next->state)) {
				set_bit(IS_GROUP_OTF_OUTPUT, &group->state);

				/* In only sensor group, VOTF path is determined by sensor mode. */
				if (group->slot == GROUP_SLOT_SENSOR) {
					if (test_bit(IS_GROUP_VOTF_OUTPUT, &group->state))
						set_bit(IS_GROUP_VOTF_INPUT, &next->state);
					else
						clear_bit(IS_GROUP_VOTF_INPUT, &next->state);
				}

				if (test_bit(IS_GROUP_VOTF_INPUT, &next->state)) {
					set_bit(IS_GROUP_VOTF_OUTPUT, &group->state);
					is_dmsg_concate(" *> ");
				} else {
					is_dmsg_concate(" -> ");
				}
			} else {
				is_dmsg_concate(" => ");
			}
		}

		group = next;
	}
	is_dmsg_concate("\n");

	return 0;
}

static void is_group_set_hw_map(struct is_group *leader_group, struct is_hardware *hw, bool init)
{
	struct is_group *pos, *tmp, *head, *pos2;
	u32 instance;
	int hw_maxnum, hw_index, hw_list[GROUP_HW_MAX];
	u32 hw_id;
	struct is_hw_ip *hw_ip;
	unsigned long hw_slot_i = 0;

	for_each_group_child_double_path(pos, tmp, leader_group) {
		head = pos->head;
		if (head->instance != pos->instance)
			head = head->pnext;

		if (head == pos) {
			hw_slot_i = 0;
			memset(head->hw_slot_id, HW_SLOT_MAX, sizeof(head->hw_slot_id));

			if (test_bit(IS_GROUP_OTF_INPUT, &head->state))
				head->hw_grp_ops = is_hw_get_otf_group_ops();
			else
				head->hw_grp_ops = is_hw_get_m2m_group_ops();
		}

		if (pos->device_type != IS_DEVICE_ISCHAIN)
			continue;

		instance = pos->instance;
		hw_maxnum = is_get_hw_list(pos->id, hw_list);
		for (hw_index = 0; hw_index < hw_maxnum; hw_index++) {
			hw_id = hw_list[hw_index];
			if (init) {
				set_bit(hw_id, &hw->logical_hw_map[instance]);
				set_bit(hw_id, &hw->hw_map[instance]);
			}

			head->hw_slot_id[hw_slot_i++]
				= CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, hw_id);

			hw_ip = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_ip, hw_id);
			if (!hw_ip) {
				mgerr("invalid hw_id (%d)", pos, pos, hw_id);
				continue;
			}

			hw_ip->group[instance] = pos;
		}

		if (!pos->child) {
			mginfo("%s: hw_map[0x%lX]\n", pos, leader_group, __func__, hw->hw_map[instance]);

			for_each_group_child(pos2, head)
				memcpy(pos2->hw_slot_id, head->hw_slot_id, sizeof(head->hw_slot_id));
		}
	}

}

int is_groupmgr_init(struct is_device_ischain *device)
{
	int ret = 0;
	struct is_subdev *leader;
	struct is_group *group, *prev, *next, *sibling, *pnext, *gnext;
	struct is_group *leader_group;
	struct is_group *head_2nd = NULL;
	u32 slot;
	u32 instance;
	unsigned long i;

	instance = device->instance;

	leader_group = grpmgr.leader[instance];
	if (!leader_group) {
		mierr("stream leader is not selected", instance);
		return -EINVAL;
	}

	for_each_set_bit(i, &device->ginstance_map, IS_STREAM_COUNT) {
		prev = NULL;

		for (slot = 0; slot < GROUP_SLOT_MAX; ++slot) {
			group = grpmgr.group[i][slot];
			if (!group)
				continue;

			if (test_bit(IS_GROUP_USE_MULTI_CH, &group->state)
					&& (group->instance == device->instance))
				continue;

			/* A group should be initialized, only if the group was placed at the front of leader group */
			if (slot < leader_group->slot)
				continue;

			group->prev = NULL;
			group->next = NULL;
			group->gprev = NULL;
			group->gnext = NULL;
			group->parent = NULL;
			group->child = NULL;
			group->head = group;
			group->tail = group;
			group->junction = NULL;

			leader = &group->leader;
			/* Set force flag to initialize every leader in subdev. */
			is_group_s_leader(group, leader, true);

			if (prev) {
				group->prev = prev;
				prev->next = group;
			} else if (!prev && test_bit(IS_GROUP_USE_MULTI_CH, &group->state)) {
				struct is_group *group_main;

				group_main = grpmgr.group[instance][slot];
				if (!group_main) {
					err("not exist main group(%d)", group->id);
					continue;
				}

				group->prev = group_main->prev;
				group_main->prev->pnext = group;
				head_2nd = group;
				grpmgr.leader[i] = group;
			}

			prev = group;
		}
	}

	ret = is_group_path_dmsg(leader_group);
	if (ret) {
		mgerr("is_group_path_dmsg is failed(%d)", leader_group, leader_group, ret);
		return ret;
	}
	if (head_2nd) {
		is_group_path_dmsg(head_2nd);
		if (ret) {
			mgerr("is_group_path_dmsg is failed #2(%d)", head_2nd, head_2nd, ret);
			return ret;
		}
	}

	group = leader_group;
	sibling = leader_group;
	next = group->next;
	while (next) {
		if (test_bit(IS_GROUP_OTF_INPUT, &next->state)) {
			group->child = next;
			next->parent = next->prev;
			sibling->tail = next;
			next->head = sibling;
			leader = &sibling->leader;
			is_group_s_leader(next, leader, false);
		} else {
			sibling->gnext = next;
			next->gprev = sibling;
			sibling = next;
		}

		group = next;
		next = group->next;
	}

	/* each group tail setting */
	group = leader_group;
	sibling = leader_group;
	next = group->next;
	while (next) {
		if (test_bit(IS_GROUP_OTF_INPUT, &next->state))
			next->tail = sibling->tail;
		else
			sibling = next;

		group = next;
		next = group->next;
	}

	/* multi channel group connection setting */
	sibling = leader_group;
	group = leader_group;
	pnext = leader_group->pnext;
	if (pnext) {
		next = pnext;
		while (next) {
			if (test_bit(IS_GROUP_OTF_INPUT, &next->state)) {
				if (next != pnext)
					group->child = next;
				next->parent = next->prev;
				sibling->ptail = next;
				next->head = sibling;
				leader = &sibling->leader;

				is_group_s_leader(next, leader, false);
			} else {
				/* Not consider M2M connection if pnext use */
			}
			group = next;
			next = group->next;
		}
	}

	group = leader_group;
	sibling = leader_group;
	next = group->pnext;
	while (next) {
		if (test_bit(IS_GROUP_OTF_INPUT, &next->state))
			next->tail = sibling->ptail;
		else
			sibling = next;

		group = next;
		next = group->next;
	}

	gnext = leader_group;
	while (gnext) {
		is_group_set_hw_map(gnext, &is_get_is_core()->hardware, true);
		gnext = gnext->gnext;
	}

	minfo(" =STM CFG===============\n", leader_group);
	info(" %s", is_dmsg_print());
	info(" DEVICE GRAPH :");
	group = leader_group;
	while (group) {
		printk(KERN_CONT " %d(%s),", group->id, group_id_name[group->id]);
		group = group->next;
	}
	printk(KERN_CONT "X \n");
	info(" =======================\n");
	return ret;
}
PST_EXPORT_SYMBOL(is_groupmgr_init);

int is_groupmgr_start(struct is_device_ischain *device)
{
	int ret = 0;
	u32 instance;
	u32 width, height;
	IS_DECLARE_PMAP(pmap);
	struct is_group *group, *prev, *pnext;
	struct is_subdev *leader, *subdev;

	FIMC_BUG(!device);

	width = 0;
	height = 0;
	IS_INIT_PMAP(pmap);
	instance = device->instance;
	group = grpmgr.leader[instance];
	if (!group) {
		merr("stream leader is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}
	pnext = group->pnext;

	minfo(" =GRP CFG===============\n", group);
	while(group) {
		leader = &group->leader;
		prev = group->prev;

		if (!test_bit(IS_GROUP_OTF_INPUT, &group->state) &&
			!test_bit(IS_GROUP_START, &group->state)) {
			mgerr(" NOT started", group, group);
			ret = -EINVAL;
			goto p_err;
		}

		if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			if (group->slot == GROUP_SLOT_SENSOR) {
				width = is_sensor_g_width(device->sensor);
				height = is_sensor_g_height(device->sensor);
				leader->input.width = width;
				leader->input.height = height;
			} else {
				if (prev && prev->junction) {
					leader->input.width = width;
					leader->input.height = height;
					prev->junction->output.width = width;
					prev->junction->output.height = height;
				} else {
					mgerr("previous group is NULL", group, group);
					BUG();
				}
			}
		} else {
			width = leader->input.width;
			height = leader->input.height;
			leader->input.canv.x = 0;
			leader->input.canv.y = 0;
			leader->input.canv.w = leader->input.width;
			leader->input.canv.h = leader->input.height;
		}

		mginfo(" SRC%02d:%04dx%04d\n", group, group, leader->vid,
			leader->input.width, leader->input.height);
		list_for_each_entry(subdev, &group->subdev_list, list) {
			if (subdev->vctx && test_bit(IS_SUBDEV_START, &subdev->state)) {
				mginfo(" CAP%2d:%04dx%04d\n", group, group, subdev->vid,
					subdev->output.width, subdev->output.height);

				if (!group->junction && (subdev != leader))
					group->junction = subdev;
			}
		}

		if (prev && !test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			if (!prev->junction) {
				mgerr("prev group is existed but junction is NULL", group, group);
				ret = -EINVAL;
				goto p_err;
			}
			/* skip size checking when using LVN */
			if (!IS_ENABLED(LOGICAL_VIDEO_NODE) &&
				((prev->junction->output.width != group->leader.input.width) ||
				(prev->junction->output.height != group->leader.input.height))) {
				mwarn("%s(%d x %d) != %s(%d x %d)", device,
					prev->junction->name,
					prev->junction->output.width,
					prev->junction->output.height,
					group->leader.name,
					group->leader.input.width,
					group->leader.input.height);
			}
		}

		group = group->next;
		if (!group && pnext) {
			group = pnext;
			pnext = NULL;
			width = group->leader.leader->input.width;
			height = group->leader.leader->input.height;
			minfo(" =GRP CFG2==============\n", group);
		}
	}
	info(" =======================\n");

	pablo_sync_repro_set_main(instance);

p_err:
	return ret;
}
PST_EXPORT_SYMBOL(is_groupmgr_start);

void is_groupmgr_stop(struct is_device_ischain *device)
{
	pablo_sync_repro_reset_main(device->instance);
}

static int is_group_open(struct is_group *group, u32 id, struct is_video_ctx *vctx)
{
	int ret = 0;
	int ret_err = 0;
	u32 stream, slot;
	struct is_group_task *gtask;

	FIMC_BUG(!group);
	FIMC_BUG(id >= GROUP_ID_MAX);

	if (test_bit(IS_GROUP_OPEN, &group->state)) {
		if (group->id == id) {
			mgerr("already open", group, group);
			ret = -EMFILE;
		} else {
			mgwarn("need to map other device's group", group, group);
			ret = -EAGAIN;
		}
		goto err_state;
	}

	group->logical_id = id;
	group->id = id;
	stream = group->instance;
	group->stm = pablo_lib_get_stream_prefix(stream);
	slot = group->slot;
	gtask = &grpmgr.gtask[id];

	/* 1. Init Group */
	clear_bit(IS_GROUP_INIT, &group->state);
	clear_bit(IS_GROUP_START, &group->state);
	clear_bit(IS_GROUP_SHOT, &group->state);
	clear_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
	clear_bit(IS_GROUP_FORCE_STOP, &group->state);
	clear_bit(IS_GROUP_OTF_INPUT, &group->state);
	clear_bit(IS_GROUP_OTF_OUTPUT, &group->state);
	clear_bit(IS_GROUP_STANDBY, &group->state);
	clear_bit(IS_GROUP_VOTF_CONN_LINK, &group->state);

	group->prev = NULL;
	group->next = NULL;
	group->gprev = NULL;
	group->gnext = NULL;
	group->parent = NULL;
	group->child = NULL;
	group->head = NULL;
	group->tail = NULL;
	group->junction = NULL;
	group->pnext = NULL;
	group->ptail = NULL;
	group->fcount = 0;
	group->pcount = 0;
	group->aeflashMode = 0; /* Flash Mode Control */
	group->remainIntentCount = 0;
	group->remainRemosaicCropIntentCount = 0;
	atomic_set(&group->scount, 0);
	atomic_set(&group->rcount, 0);
	atomic_set(&group->backup_fcount, 0);
	atomic_set(&group->sensor_fcount, 1);
	sema_init(&group->smp_trigger, 0);

#ifdef MEASURE_TIME
#ifdef MONITOR_TIME
	monitor_init(&group->time);
#endif
#endif

	/* 2. start kthread */
	ret = is_group_task_start(gtask, group->slot);
	if (ret) {
		mgerr("is_group_task_start is fail(%d)", group, group, ret);
		goto err_group_task_start;
	}

	/* 3. Subdev Init */
	ret = is_subdev_open(&group->leader, vctx, stream);
	if (ret) {
		mgerr("is_subdev_open is fail(%d)", group, group, ret);
		goto err_subdev_open;
	}

	set_bit(IS_SUBDEV_EXTERNAL_USE, &group->leader.state);

	/* 4. group hw Init */
	ret = is_hw_group_open(group);
	if (ret) {
		mgerr("is_hw_group_open is fail(%d)", group, group, ret);
		goto err_hw_group_open;
	}

	/* 5. Update Group Manager */
	grpmgr.group[stream][slot] = group;
	set_bit(IS_GROUP_OPEN, &group->state);

	mdbgd_group("%s(%d) E\n", group, __func__, ret);
	return 0;

err_hw_group_open:
	ret_err = is_subdev_close(&group->leader);
	if (ret_err)
		mgerr("is_subdev_close is fail(%d)", group, group, ret_err);

	clear_bit(IS_SUBDEV_EXTERNAL_USE, &group->leader.state);

err_subdev_open:
	ret_err = is_group_task_stop(gtask);
	if (ret_err)
		mgerr("is_group_task_stop is fail(%d)", group, group, ret_err);
err_group_task_start:
err_state:
	return ret;
}

static int is_group_close(struct is_group *group)
{
	int ret = 0;
	u32 stream, slot;
	struct is_group_task *gtask;
	int state_vlink;

	FIMC_BUG(!group);
	FIMC_BUG(group->instance >= IS_STREAM_COUNT);
	FIMC_BUG(group->id >= GROUP_ID_MAX);

	slot = group->slot;
	stream = group->instance;
	gtask = &grpmgr.gtask[group->id];

	if (!test_bit(IS_GROUP_OPEN, &group->state)) {
		mgerr("already close", group, group);
		return -EMFILE;
	}

	ret = is_group_task_stop(gtask);
	if (ret)
		mgerr("is_group_task_stop is fail(%d)", group, group, ret);

	mutex_lock(&group->mlock_votf);
	state_vlink = test_and_clear_bit(IS_GROUP_VOTF_CONN_LINK, &group->state);
	mutex_unlock(&group->mlock_votf);

	if (state_vlink) {
		mginfo("destroy votf_link forcely at group_close", group, group);

		if (group->head && group->head->device_type != IS_DEVICE_SENSOR) {
			ret = is_votf_destroy_link(group);
			if (ret)
				mgerr("is_votf_destroy_link is fail(%d)", group, group, ret);
		}
	}

	ret = is_subdev_close(&group->leader);
	if (ret)
		mgerr("is_subdev_close is fail(%d)", group, group, ret);

	clear_bit(IS_SUBDEV_EXTERNAL_USE, &group->leader.state);

	clear_bit(IS_GROUP_USE_MULTI_CH, &group->state);
	clear_bit(IS_GROUP_INIT, &group->state);
	clear_bit(IS_GROUP_OPEN, &group->state);
	grpmgr.group[stream][slot] = NULL;

	mdbgd_group("%s(ref %d, %d)", group, __func__, atomic_read(&gtask->refcount), ret);

	/* reset after using it */
	group->id = GROUP_ID_MAX;
	group->logical_id = GROUP_ID_MAX;

	return ret;
}

static void is_group_clear_bits(struct is_group *group)
{
	clear_bit(IS_GROUP_OTF_INPUT, &group->state);
	clear_bit(IS_GROUP_OTF_OUTPUT, &group->state);
	clear_bit(IS_GROUP_VOTF_INPUT, &group->state);
	clear_bit(IS_GROUP_VOTF_OUTPUT, &group->state);
	clear_bit(IS_GROUP_REPROCESSING, &group->state);
	clear_bit(IS_GROUP_STRGEN_INPUT, &group->state);
}

static int is_group_init(struct is_group *group, u32 input_type, u32 stream_leader, u32 stream_type)
{
	int ret = 0;
	u32 slot;

	FIMC_BUG(!group);
	FIMC_BUG(group->instance >= IS_STREAM_COUNT);
	FIMC_BUG(group->id >= GROUP_ID_MAX);

	if (!test_bit(IS_GROUP_OPEN, &group->state)) {
		mgerr("group is NOT open", group, group);
		ret = -EINVAL;
		goto err_open_already;
	}

	slot = group->slot;
	group->thrott_tick = KEEP_FRAME_TICK_THROTT;

	is_group_clear_bits(group);

	if (stream_leader)
		grpmgr.leader[group->instance] = group;

	if (input_type >= GROUP_INPUT_MAX) {
		mgerr("input type is invalid(%d)", group, group, input_type);
		ret = -EINVAL;
		goto err_input_type;
	}

	if (stream_type != IS_PREVIEW_STREAM)
		set_bit(IS_GROUP_REPROCESSING, &group->state);

	smp_shot_init(group, 1);
	group->asyn_shots = 0;
	group->skip_shots = 0;
	group->init_shots = 0;
	group->sync_shots = 1;
	group->input_type = input_type;

	if (input_type == GROUP_INPUT_OTF || input_type == GROUP_INPUT_VOTF) {
		set_bit(IS_GROUP_OTF_INPUT, &group->state);
		smp_shot_init(group, MIN_OF_SHOT_RSC);
		group->asyn_shots = MIN_OF_ASYNC_SHOTS;
		group->skip_shots = MIN_OF_ASYNC_SHOTS;
		group->init_shots = MIN_OF_ASYNC_SHOTS;
		group->sync_shots = MIN_OF_SYNC_SHOTS;
	}

	if (input_type == GROUP_INPUT_VOTF)
		set_bit(IS_GROUP_VOTF_INPUT, &group->state);
	else if (input_type == GROUP_INPUT_STRGEN)
		set_bit(IS_GROUP_STRGEN_INPUT, &group->state);

	set_bit(IS_GROUP_INIT, &group->state);

err_input_type:
err_open_already:
	mdbgd_group("%s(input : %d):%d\n", group, __func__, input_type, ret);

	return ret;
}

static void set_group_shots(struct is_group *group)
{
#ifdef REDUCE_COMMAND_DELAY
	group->asyn_shots = MIN_OF_ASYNC_SHOTS + 1;
	group->sync_shots = 0;
#else
	group->asyn_shots = MIN_OF_ASYNC_SHOTS + 0;
	group->sync_shots = MIN_OF_SYNC_SHOTS;
#endif
	group->init_shots = group->asyn_shots;
	group->skip_shots = group->asyn_shots;

	return;
}

static int is_group_start(struct is_group *group)
{
	int ret = 0;
	struct is_framemgr *framemgr = NULL;
	struct is_group_task *gtask;
	struct is_group *pos;
	u32 sensor_fcount;
	u32 width, height;

	FIMC_BUG(!group);
	FIMC_BUG(group->instance >= IS_STREAM_COUNT);
	FIMC_BUG(group->id >= GROUP_ID_MAX);

	if (!test_bit(IS_GROUP_INIT, &group->state)) {
		merr("group is NOT initialized", group);
		ret = -EINVAL;
		goto p_err;
	}

	if (test_bit(IS_GROUP_START, &group->state)) {
		warn("already group start");
		ret = -EINVAL;
		goto p_err;
	}

	gtask = &grpmgr.gtask[group->id];
	framemgr = GET_HEAD_GROUP_FRAMEMGR(group);
	if (!framemgr) {
		mgerr("framemgr is NULL", group, group);
		goto p_err;
	}

	atomic_set(&group->scount, 0);
	sema_init(&group->smp_trigger, 0);

	if (test_bit(IS_GROUP_REPROCESSING, &group->state)) {
		group->asyn_shots = 1;
		group->skip_shots = 0;
		group->init_shots = 0;
		group->sync_shots = 0;
		smp_shot_init(group, group->asyn_shots + group->sync_shots);
	} else {
		if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			set_group_shots(group);

			/* frame count */
			sensor_fcount = is_sensor_g_fcount(group->sensor) + 1;
			atomic_set(&group->sensor_fcount, sensor_fcount);
			atomic_set(&group->backup_fcount, sensor_fcount - 1);
			group->fcount = sensor_fcount - 1;

			memset(&group->intent_ctl, 0, sizeof(struct camera2_aa_ctl));

			/* shot resource */
			sema_init(&gtask->smp_resource, group->asyn_shots + group->sync_shots);
			smp_shot_init(group, group->asyn_shots + group->sync_shots);
		} else {
			group->asyn_shots = MIN_OF_ASYNC_SHOTS;
			group->skip_shots = 0;
			group->init_shots = 0;
			group->sync_shots = 0;
			smp_shot_init(group, group->asyn_shots + group->sync_shots);
		}
	}

	width = height = 0;

	for_each_group_child(pos, group) {
		if (test_bit(IS_GROUP_VOTF_INPUT, &pos->state)) {
			width = pos->head->leader.input.width;
			height = pos->head->leader.input.height;

			if (test_and_set_bit(IS_GROUP_VOTF_CONN_LINK, &pos->state)) {
				mgwarn("already connected votf link", group, group);
				continue;
			}

			ret = is_votf_create_link(pos, width, height);
			if (ret) {
				mgerr("is_votf_create_link is fail(%d)", pos, pos, ret);
				goto p_err;
			}
		}
	}

#if IS_ENABLED(ENABLE_RECURSIVE_NR)
	is_hw_restore_group_state_2nr(group);
#endif

	set_bit(IS_SUBDEV_START, &group->leader.state);
	set_bit(IS_GROUP_START, &group->state);

p_err:
	mginfo("bufs: %02d, init : %d, asyn: %d, skip: %d, sync : %d\n", group, group,
		framemgr ? framemgr->num_frames : 0, group->init_shots,
		group->asyn_shots, group->skip_shots, group->sync_shots);
	return ret;
}

static inline bool wait_req_frame_before_stop(struct is_group *head, struct is_group_task *gtask,
					      struct is_framemgr *framemgr)
{
	int retry = 150, retry_wait;
	struct is_device_sensor *sensor;
	unsigned long flags;

	while (--retry && framemgr->queued_count[FS_REQUEST]) {
		if (test_bit(IS_GROUP_OTF_INPUT, &head->state) &&
		    !list_empty(&head->smp_trigger.wait_list)) {
			sensor = head->sensor;
			if (!sensor) {
				mwarn(" sensor is NULL, forcely trigger(pc %d)", head,
				      head->pcount);
				set_bit(IS_GROUP_FORCE_STOP, &head->state);
				up(&head->smp_trigger);
			} else if (!test_bit(IS_SENSOR_OPEN, &head->state)) {
				mwarn(" sensor is closed, forcely trigger(pc %d)", head,
				      head->pcount);
				set_bit(IS_GROUP_FORCE_STOP, &head->state);
				up(&head->smp_trigger);
			} else if (!test_bit(IS_SENSOR_FRONT_START, &sensor->state)) {
				mwarn(" front is stopped, forcely trigger(pc %d)", head,
				      head->pcount);
				set_bit(IS_GROUP_FORCE_STOP, &head->state);
				up(&head->smp_trigger);
			} else if (!test_bit(IS_SENSOR_BACK_START, &sensor->state)) {
				mwarn(" back is stopped, forcely trigger(pc %d)", head,
				      head->pcount);
				set_bit(IS_GROUP_FORCE_STOP, &head->state);
				up(&head->smp_trigger);
			} else if (retry < 100) {
				merr(" sensor is working but no trigger(pc %d)", head,
				     head->pcount);
				set_bit(IS_GROUP_FORCE_STOP, &head->state);
				up(&head->smp_trigger);
			} else {
				mwarn(" wating for sensor trigger(pc %d)", head, head->pcount);
			}
		} else if (!test_bit(IS_GROUP_OTF_INPUT, &head->state)) {
			CALL_SYNC_REPRO_OPS(gtask->sync_repro, flush_task, gtask, head);
		}

		mgwarn(" %d reqs waiting1...(pc %d) smp_resource(%d)", head, head,
		       framemgr->queued_count[FS_REQUEST], head->pcount,
		       list_empty(&gtask->smp_resource.wait_list));

		retry_wait = 20;
		while (--retry_wait && framemgr->queued_count[FS_REQUEST]) {
			usleep_range(1000, 1000);
		}
	}

	if (!retry)
		mgerr(" waiting(until request empty) is fail(pc %d)", head, head, head->pcount);

	/* ensure that request cancel work is complete fully */
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_21, flags);
	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_21, flags);

	return !retry;
}

static inline bool wait_shot_thread(struct is_group *head)
{
	int retry = 150;

	while (--retry && test_bit(IS_GROUP_SHOT, &head->state)) {
		mgwarn(" thread stop waiting...(pc %d)", head, head, head->pcount);
		msleep(20);
	}

	if (!retry)
		mgerr(" waiting(until thread stop) is fail(pc %d)", head, head, head->pcount);

	return !retry;
}

static inline bool wait_pro_frame(struct is_group *head, struct is_framemgr *framemgr)
{
	int retry = 3000;

	while (--retry && framemgr->queued_count[FS_PROCESS]) {
		if (retry % 100 == 0)
			mgwarn(" %d pros waiting...(pc %d)", head, head,
			       framemgr->queued_count[FS_PROCESS], head->pcount);
		usleep_range(1000, 1000);
	}

	if (!retry)
		mgerr(" waiting(until process empty) is fail(pc %d)", head, head, head->pcount);

	return !retry;
}

static inline bool wait_req_frame_after_stop(struct is_group *head, struct is_framemgr *framemgr)
{
	int retry = 150;
	struct is_frame *frame;
	struct is_video_ctx *vctx;
	unsigned long flags;

	/* After stopped, wait until remained req_list frame is flushed by group shot cancel */
	while (--retry && framemgr->queued_count[FS_REQUEST]) {
		mgwarn(" %d req waiting2...(pc %d)", head, head, framemgr->queued_count[FS_REQUEST],
		       head->pcount);
		usleep_range(1000, 1001);
	}

	if (!retry) {
		mgerr(" waiting(until request empty) is fail(pc %d)", head, head, head->pcount);

		vctx = head->leader.vctx;

		framemgr_e_barrier_irqs(framemgr, 0, flags);
		frame = peek_frame(framemgr, FS_REQUEST);
		while (frame) {
			mgrinfo("[ERR] NDONE(%d, E%X)\n", head, head, frame, frame->index,
				IS_SHOT_UNPROCESSED);

			clear_bit(head->leader.id, &frame->out_flag);
			CALL_GROUP_OPS(head, done, frame, VB2_BUF_STATE_ERROR);
			trans_frame(framemgr, frame, FS_COMPLETE);
			CALL_VOPS(vctx, done, frame->index, VB2_BUF_STATE_ERROR);

			frame = peek_frame(framemgr, FS_REQUEST);
		}

		framemgr_x_barrier_irqr(framemgr, 0, flags);
	}

	return !retry;
}

static inline bool check_remain_req_count(struct is_group *head, struct is_group_task *gtask)
{
	unsigned long flags;
	u32 rcount = atomic_read(&head->rcount);

	if (!rcount)
		return false;

	mgerr(" request is NOT empty(%d) (pc %d)", head, head, rcount, head->pcount);

	/*
	 * Extinctionize pending works in worker to avoid the work_list corruption.
	 * When user calls 'vb2_stop_streaming()' that calls 'group_stop()',
	 * 'v4l2_reqbufs()' can be called for another stream
	 * and it means every work in frame is going to be initialized.
	 */
	spin_lock_irqsave(&gtask->gtask_slock, flags);
	INIT_LIST_HEAD(&gtask->worker.work_list);
	INIT_LIST_HEAD(&gtask->worker.delayed_work_list);
	spin_unlock_irqrestore(&gtask->gtask_slock, flags);

	/* the count of request should be clear for next streaming */
	atomic_set(&head->rcount, 0);

	return true;
}

static int is_group_stop(struct is_group *group)
{
	int ret = 0;
	int errcnt = 0;
	u32 fstop;
	struct is_framemgr *framemgr;
	struct is_device_ischain *device;
	struct is_group *head;
	struct is_group *pos, *tmp;
	struct is_group_task *gtask;
	int state_vlink;

	FIMC_BUG(!group->device);
	FIMC_BUG(!group->leader.vctx);
	FIMC_BUG(group->instance >= IS_STREAM_COUNT);
	FIMC_BUG(group->id >= GROUP_ID_MAX);

	head = group->head;
	if (!head) {
		mgwarn("head group is NULL", group, group);
		return -EPERM;
	}

	if (!test_bit(IS_GROUP_START, &group->state) &&
	    !test_bit(IS_GROUP_START, &head->state)) {
		mgwarn("already group stop", group, group);
		return -EPERM;
	}

	framemgr = GET_HEAD_GROUP_FRAMEMGR(group);
	if (!framemgr) {
		mgerr("framemgr is NULL", group, group);
		goto p_err;
	}

	/* force stop set if only HEAD group OTF input */
	if (test_bit(IS_GROUP_OTF_INPUT, &head->state)) {
		if (test_bit(IS_GROUP_REQUEST_FSTOP, &group->state))
			set_bit(IS_GROUP_FORCE_STOP, &group->state);
	}
	clear_bit(IS_GROUP_REQUEST_FSTOP, &group->state);

	gtask = &grpmgr.gtask[head->id];
	device = group->device;

	errcnt += wait_req_frame_before_stop(head, gtask, framemgr);
	errcnt += wait_shot_thread(head);

	fstop = test_bit(IS_GROUP_FORCE_STOP, &group->state);
	ret = is_itf_process_stop(device, head, fstop);
	if (ret) {
		mgerr(" is_itf_process_stop is fail(%d)", head, head, ret);
		errcnt++;
	}

	/* Stop sub ischain instance for MULTI_CH scenario */
	for_each_group_child_double_path(pos, tmp, head) {
		if (test_bit(IS_GROUP_VOTF_INPUT, &pos->state)) {
			mutex_lock(&pos->mlock_votf);
			state_vlink = test_and_clear_bit(IS_GROUP_VOTF_CONN_LINK, &pos->state);
			mutex_unlock(&pos->mlock_votf);

			if (!state_vlink) {
				mgwarn("already destroy votf link", pos, pos);
				continue;
			}

			ret = is_votf_destroy_link(pos);
			if (ret)
				mgerr("is_votf_destroy_link is fail(%d)", pos, pos, ret);
		}
	}

	errcnt += wait_pro_frame(head, framemgr);
	errcnt += wait_req_frame_after_stop(head, framemgr);
	errcnt += check_remain_req_count(head, gtask);

	if (test_bit(IS_GROUP_OTF_INPUT, &head->state))
		mginfo(" sensor fcount: %d, fcount: %d\n", head, head,
			atomic_read(&head->sensor_fcount), head->fcount);

	clear_bit(IS_GROUP_FORCE_STOP, &group->state);
	clear_bit(IS_SUBDEV_START, &group->leader.state);
	clear_bit(IS_GROUP_START, &group->state);
	clear_bit(IS_SUBDEV_RUN, &group->leader.state);
	clear_bit(IS_GROUP_VOTF_CONN_LINK, &group->state);

p_err:
	return -errcnt;
}

static void is_group_override_meta(struct is_group *group,
				struct is_frame *frame,
				struct is_resourcemgr *resourcemgr,
				struct is_device_sensor *sensor)
{
	if (IS_ENABLED(FIXED_FPS_DEBUG)) {
		frame->shot->ctl.aa.aeTargetFpsRange[0] = FIXED_MAX_FPS_VALUE;
		frame->shot->ctl.aa.aeTargetFpsRange[1] = FIXED_MAX_FPS_VALUE;
		frame->shot->ctl.sensor.frameDuration = 1000000000 / FIXED_MAX_FPS_VALUE;
	}

	if (resourcemgr->limited_fps) {
		frame->shot->ctl.aa.aeTargetFpsRange[0] = resourcemgr->limited_fps;
		frame->shot->ctl.aa.aeTargetFpsRange[1] = resourcemgr->limited_fps;
		frame->shot->ctl.sensor.frameDuration = 1000000000/resourcemgr->limited_fps;
	}

	/* preview HW running fps */
	if (IS_ENABLED(HW_RUNNING_FPS)) {
		u32 ex_mode = is_sensor_g_ex_mode(sensor);

		if (ex_mode == EX_DUALFPS_960 || ex_mode == EX_DUALFPS_480) {
			frame->shot->ctl.aa.ispHwTargetFpsRange[0] = 60;
			frame->shot->ctl.aa.ispHwTargetFpsRange[1] = 60;
		} else {
			if (frame->shot->ctl.aa.vendor_fpsHintResult == 0) {
				frame->shot->ctl.aa.ispHwTargetFpsRange[0] =
					frame->shot->ctl.aa.aeTargetFpsRange[0];
				frame->shot->ctl.aa.ispHwTargetFpsRange[1] =
					frame->shot->ctl.aa.aeTargetFpsRange[1];
			}  else {
				frame->shot->ctl.aa.ispHwTargetFpsRange[0] =
					frame->shot->ctl.aa.vendor_fpsHintResult;
				frame->shot->ctl.aa.ispHwTargetFpsRange[1] =
					frame->shot->ctl.aa.vendor_fpsHintResult;
			}
		}
	}

#if IS_ENABLED(CONFIG_CAMERA_PDP)
	if (!test_bit(IS_GROUP_REPROCESSING, &group->state)) {
		/* PAF */
		if (sensor->cfg->pd_mode == PD_NONE)
			frame->shot->uctl.isModeUd.paf_mode = CAMERA_PAF_OFF;
		else
			frame->shot->uctl.isModeUd.paf_mode = CAMERA_PAF_ON;
	}
#endif
}

static void is_group_override_sensor_req(struct is_framemgr *framemgr,
					struct is_frame *frame)
{
	int req_cnt = 0;
	struct is_frame *prev;

	list_for_each_entry_reverse(prev, &framemgr->queued_list[FS_REQUEST], list) {
		if (++req_cnt > SENSOR_REQUEST_DELAY)
			break;

		if (frame->shot->ctl.aa.aeMode == AA_AEMODE_OFF
		    || frame->shot->ctl.aa.mode == AA_CONTROL_OFF) {
			prev->shot->ctl.sensor.exposureTime	= frame->shot->ctl.sensor.exposureTime;
			prev->shot->ctl.sensor.frameDuration	= frame->shot->ctl.sensor.frameDuration;
			prev->shot->ctl.sensor.sensitivity	= frame->shot->ctl.sensor.sensitivity;
			prev->shot->ctl.aa.vendor_isoValue	= frame->shot->ctl.aa.vendor_isoValue;
			prev->shot->ctl.aa.vendor_isoMode	= frame->shot->ctl.aa.vendor_isoMode;
			prev->shot->ctl.aa.aeMode		= frame->shot->ctl.aa.aeMode;
		}

		/*
		 * Flash capture has 2 Frame delays due to DDK constraints.
		 * N + 1: The DDK uploads the best shot and streams off.
		 * N + 2: HAL used the buffer of the next of best shot as a flash image.
		 */
		if (frame->shot->ctl.aa.vendor_aeflashMode == AA_FLASHMODE_CAPTURE)
			prev->shot->ctl.aa.vendor_aeflashMode	= frame->shot->ctl.aa.vendor_aeflashMode;

		prev->shot->ctl.aa.aeExpCompensation		= frame->shot->ctl.aa.aeExpCompensation;
		prev->shot->ctl.aa.aeLock			= frame->shot->ctl.aa.aeLock;
		prev->shot->ctl.lens.opticalStabilizationMode	= frame->shot->ctl.lens.opticalStabilizationMode;

		if (frame->shot->ctl.aa.sceneMode == AA_SCENE_MODE_PRO_MODE
		    && frame->shot->ctl.aa.captureIntent == AA_CAPTURE_INTENT_STILL_CAPTURE_EXPOSURE_DYNAMIC_SHOT) {
			prev->shot->ctl.aa.sceneMode = frame->shot->ctl.aa.sceneMode;
			prev->shot->ctl.aa.captureIntent = frame->shot->ctl.aa.captureIntent;
			prev->shot->ctl.aa.vendor_captureExposureTime = frame->shot->ctl.aa.vendor_captureExposureTime;
			prev->shot->ctl.aa.vendor_captureCount = frame->shot->ctl.aa.vendor_captureCount;
		}
	}
}

static void is_group_update_stripe(struct is_group *group,
				struct is_frame *frame,
				struct is_resourcemgr *resourcemgr)
{
	struct camera2_stream *stream = (struct camera2_stream *)frame->shot_ext;
	struct is_group *pos;
	int dev_region_num;
	int max_region_num;

	if (!IS_ENABLED(CHAIN_STRIPE_PROCESSING))
		return;

	/* Trigger stripe processing for remosaic capture request. */
	if (IS_ENABLED(CHAIN_USE_STRIPE_REGION_NUM_META) && stream->stripe_region_num) {
		frame->stripe_info.region_num = stream->stripe_region_num;
	} else {
		max_region_num = 0;
		dev_region_num = 0;

		for_each_group_child (pos, group) {
			CALL_SOPS(&pos->leader, get, frame, PSGT_REGION_NUM, &dev_region_num);

			if (max_region_num < dev_region_num)
				max_region_num = dev_region_num;
		}
		frame->stripe_info.region_num = max_region_num;
	}
	mgrdbgs(3, "set stripe_region_num %d\n", group, group, frame,
			frame->stripe_info.region_num);

	return;
}

static inline bool __group_has_child(struct is_group *grp, u32 id)
{
	bool match_id = false;
	struct is_group *pos;

	for_each_group_child(pos, grp->child) {
		if (pos->id == id) {
			match_id = true;
			break;
		}
	}

	return match_id;
}

static void is_group_update_repeat(struct is_group *group,
				struct is_frame *frame)
{
	bool repeat_clahe = false;
	u32 target_gid = GROUP_ID_MAX;

#if IS_ENABLED(ENABLE_YUVP_CLAHE_ITERATION)
	repeat_clahe = frame->shot_ext->node_group.leader.iterationType == NODE_ITERATION_TYPE_SVHIST;
	target_gid = GROUP_ID_YUVP;
#endif

	/* for test w/o HAL */
	if (is_get_debug_param(IS_DEBUG_PARAM_STREAM) >= 6)
		repeat_clahe = true;

	frame->repeat_info.scenario = PABLO_REPEAT_NO;
	frame->repeat_info.num = 0;

	if (repeat_clahe && __group_has_child(group, target_gid)) {
		frame->repeat_info.scenario = PABLO_REPEAT_YUVP_CLAHE;
		frame->repeat_info.num = 2;
	}
}

static int pablo_group_buffer_init(struct is_group *ig, u32 index)
{
	unsigned long flags;
	struct is_framemgr *framemgr = GET_HEAD_GROUP_FRAMEMGR(ig);
	struct is_frame *frame;

	if (!framemgr) {
		mgerr("framemgr is NULL", ig, ig);
		return -EINVAL;
	}

	if (index >= framemgr->num_frames) {
		mgerr("out of range (%d >= %d)", ig, ig, index, framemgr->num_frames);
		return -EINVAL;
	}

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_23, flags);

	frame = &framemgr->frames[index];
	frame->group = ig;

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_23, flags);

	return 0;
}

static int is_group_buffer_queue(struct is_group *group, struct is_queue *queue, u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct is_resourcemgr *resourcemgr;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct is_device_sensor *sensor = NULL;
	struct is_sysfs_debug *sysfs_debug;

	FIMC_BUG(!group);
	FIMC_BUG(group->instance >= IS_STREAM_COUNT);
	FIMC_BUG(group->id >= GROUP_ID_MAX);
	FIMC_BUG(!queue);

	resourcemgr = &is_get_is_core()->resourcemgr;
	framemgr = &queue->framemgr;

	FIMC_BUG(index >= framemgr->num_frames);

	/* 1. check frame validation */
	frame = &framemgr->frames[index];

	if (unlikely(!test_bit(FRAME_MEM_INIT, &frame->mem_state))) {
		err("frame %d is NOT init", index);
		ret = EINVAL;
		goto p_err;
	}

	if (group->device_type == IS_DEVICE_SENSOR)
		sensor = group->sensor;

	if (sensor)
		is_sensor_s_batch_mode(sensor, frame);

	/* 2. update frame manager */
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_22, flags);

	if (frame->state == FS_FREE) {
		if (unlikely(frame->out_flag)) {
			mgwarn("output(0x%lX) is NOT completed", group, group, frame->out_flag);
			frame->out_flag = 0;
		}

		if (!test_bit(IS_GROUP_OTF_INPUT, &group->state) &&
			(framemgr->queued_count[FS_REQUEST] >= DIV_ROUND_UP(framemgr->num_frames, 2))) {
				mgwarn(" request bufs : %d", group, group, framemgr->queued_count[FS_REQUEST]);
				frame_manager_print_queues(framemgr);
				sysfs_debug = is_get_sysfs_debug();
				if (test_bit(IS_HAL_DEBUG_PILE_REQ_BUF, &sysfs_debug->hal_debug_mode)) {
					mdelay(sysfs_debug->hal_debug_delay);
					panic("HAL panic for request bufs");
				}
		}

		IS_INIT_PMAP(frame->pmap);
		frame->result = 0;
		frame->fcount = frame->shot->dm.request.frameCount;
		frame->rcount = frame->shot->ctl.request.frameCount;

		if (sensor) {
			is_group_override_meta(group, frame, resourcemgr, sensor);
			is_group_override_sensor_req(framemgr, frame);
		}

#ifdef ENABLE_MODECHANGE_CAPTURE
		if (sensor && !test_bit(IS_SENSOR_FRONT_START, &sensor->state)) {
			sensor->mode_chg_frame = NULL;

			if (is_vendor_check_remosaic_mode_change(frame)) {
				clear_bit(IS_SENSOR_OTF_OUTPUT, &sensor->state);
				sensor->mode_chg_frame = frame;
			} else {
				if (group->child)
					set_bit(IS_SENSOR_OTF_OUTPUT, &sensor->state);
			}
		}
#endif

		trans_frame(framemgr, frame, FS_REQUEST);
	} else {
		err("frame(%d) is invalid state(%d)\n", index, frame->state);
		frame_manager_print_queues(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_22, flags);

	is_group_update_stripe(group, frame, resourcemgr);
	is_group_update_repeat(group, frame);

	is_group_start_trigger(&grpmgr, group, frame);

p_err:
	mgrdbgs(5, "%s\n", group, group, frame, __func__);

	return ret;
}

static int is_group_buffer_finish(struct is_group *group, u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	FIMC_BUG(!group);
	FIMC_BUG(group->instance >= IS_STREAM_COUNT);

	if (unlikely(!test_bit(IS_GROUP_OPEN, &group->state))) {
		warn("group was closed..(%d)", index);
		return ret;
	}

	if (unlikely(group->id >= GROUP_ID_MAX)) {
		mgerr("group id is invalid(%d)", group, group, index);
		ret = -EINVAL;
		return ret;
	}

	framemgr = GET_HEAD_GROUP_FRAMEMGR(group);
	FIMC_BUG(index >= (framemgr ? framemgr->num_frames : 0));

	frame = &framemgr->frames[index];
	/* 2. update frame manager */
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_23, flags);

	if (frame->state == FS_COMPLETE) {
		trans_frame(framemgr, frame, FS_FREE);

		frame->shot_ext->free_cnt = framemgr->queued_count[FS_FREE];
		frame->shot_ext->request_cnt = framemgr->queued_count[FS_REQUEST];
		frame->shot_ext->process_cnt = framemgr->queued_count[FS_PROCESS];
		frame->shot_ext->complete_cnt = framemgr->queued_count[FS_COMPLETE];
	} else {
		mgerr("frame(%d) is not com state(%d)", group, group, index, frame->state);
		frame_manager_print_queues(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_23, flags);

	PROGRAM_COUNT(15);
	TIME_SHOT(TMS_DQ);

	mgrdbgs(5, "%s\n", group, group, frame, __func__);

	return ret;
}

#if IS_ENABLED(ENABLE_DVFS)
static bool is_group_throttling_tick_check(struct is_device_ischain *device,
	struct is_group *group, struct is_frame *frame)
{
	struct is_resourcemgr *resourcemgr = device->resourcemgr;
	struct is_dvfs_scenario_ctrl *static_ctrl = resourcemgr->dvfs_ctrl.static_ctrl;
	bool ret = false;
	int current_fps;

	if (!resourcemgr->limited_fps) {
		/* throttling release event */
		mgrinfo("[DVFS] throttling restore scenario(%d)-[%s]\n", group, group, frame,
			static_ctrl->cur_scenario_id,
			static_ctrl->scenario_nm);
		return true;
	}

	if (!group->gnext) {
		current_fps = frame->shot->ctl.aa.aeTargetFpsRange[1];

		if ((group->thrott_tick > 0) &&
			(current_fps == 3 || current_fps == 5 || current_fps == 15))
			group->thrott_tick--;

		if (!group->thrott_tick) {
			set_bit(IS_DVFS_TICK_THROTT, &resourcemgr->dvfs_ctrl.state);
			ret = true;
			mgrinfo("[DVFS] throttling scenario(%d)-[%s]\n", group, group, frame,
				static_ctrl->cur_scenario_id,
				static_ctrl->scenario_nm);
		}

		mgrdbgs(3, "limited_fps(%d), current_fps(%d) tick(%d)\n",
			group, group, frame,
			resourcemgr->limited_fps, current_fps, group->thrott_tick);
	}

	return ret;
}

static void is_group_update_dvfs(struct is_device_ischain *device,
				struct is_group *group,
				struct is_frame *frame)
{
	struct is_resourcemgr *resourcemgr = device->resourcemgr;
	struct is_sysfs_debug *sysfs_debug;
	struct is_dvfs_ctrl *dvfs_ctrl = &(resourcemgr->dvfs_ctrl);
	struct is_dvfs_scenario_ctrl *dynamic_ctrl = dvfs_ctrl->dynamic_ctrl;
	struct is_dvfs_scenario_ctrl *static_ctrl = dvfs_ctrl->static_ctrl;
	struct is_dvfs_iteration_mode *iter = dvfs_ctrl->iter_mode;
	int scenario_id, update_dvfs = 0;
	int ret;
	bool reprocessing_mode;

	/* Skip dynamic DVFS update when it's sensor group to minimize LATE_SHOT */
	if (group->device_type == IS_DEVICE_SENSOR && group->tail->next)
		return;

	mutex_lock(&dvfs_ctrl->lock);
	sysfs_debug = is_get_sysfs_debug();

	if ((!is_pm_qos_request_active(&dvfs_ctrl->user_qos)) && (sysfs_debug->en_dvfs)) {
		/* try to find dynamic scenario to apply */
		reprocessing_mode = test_bit(IS_GROUP_REPROCESSING, &group->state);
		/* when thrott_ctrl is not OFF, the value of thrott_ctrl is thrott scenario id */
		dvfs_ctrl->thrott_ctrl = (device->resourcemgr->limited_fps > 0) ?
						 IS_DVFS_SN_THROTTLING :
						 IS_DVFS_THROTT_CTRL_OFF;
		is_hw_check_iteration_state(frame, dvfs_ctrl, group);
		ret = is_dvfs_sel_dynamic(dvfs_ctrl, reprocessing_mode);
		if (ret == 0) {
			scenario_id = is_hw_dvfs_get_scenario(device, reprocessing_mode);
			if (scenario_id < 0)
				scenario_id = IS_DVFS_SN_MAX;
			if (!is_dvfs_update_dynamic(dvfs_ctrl, scenario_id))
				scenario_id = -1;
		} else {
			scenario_id = -1;
		}

		if (scenario_id > 0) {
			mgrinfo("[DVFS] dynamic scenario(%d)-[%s]\n", group, group, frame,
				scenario_id, dynamic_ctrl->scenario_nm);
			update_dvfs = 1;
		}

		if ((scenario_id < 0) && is_hw_dvfs_restore_static(device)) {
			mgrinfo("[DVFS] restore scenario(%d)-[%s]\n", group, group, frame,
				static_ctrl->cur_scenario_id,
				static_ctrl->scenario_nm);
			update_dvfs = 1;
			scenario_id = static_ctrl->cur_scenario_id;
		}

		if (test_bit(IS_DVFS_TMU_THROTT, &dvfs_ctrl->state)) {
			if (is_group_throttling_tick_check(device, group, frame)) {
				clear_bit(IS_DVFS_TMU_THROTT, &dvfs_ctrl->state);
				update_dvfs = 1;
				if (scenario_id <= 0)
					scenario_id = static_ctrl->cur_scenario_id;
			}
		} else
			group->thrott_tick = KEEP_FRAME_TICK_THROTT;

		if (IS_ENABLED(ENABLE_YUVP_CLAHE_ITERATION) && iter->iter_svhist) {
			dynamic_ctrl->cur_frame_tick = KEEP_FRAME_TICK_ITERATION;
			dynamic_ctrl->cur_scenario_id = -1; /* clear current dynamic scenario */
			if (iter->changed) {
				update_dvfs = 1;
				if (scenario_id <= 0)
					scenario_id = static_ctrl->cur_scenario_id;
			}
		}

		if (update_dvfs)
			is_set_dvfs_m2m(dvfs_ctrl, scenario_id, reprocessing_mode);
	}
	mutex_unlock(&dvfs_ctrl->lock);
}
#else
#define is_group_update_dvfs(device, group, frame)	do { } while (0)
#endif

static inline void is_group_shot_recovery(struct is_groupmgr *groupmgr,
					struct is_group *group,
					struct is_group_task *gtask,
					bool try_sdown, bool try_rdown,
					bool try_gdown[])
{
	struct is_group *pos;
	struct is_group_task *gtask_child;

	for_each_group_child(pos, group->child) {
		if (test_bit(IS_GROUP_OTF_INPUT, &group->state))
			break;

		if (!test_bit(IS_GROUP_OPEN, &pos->state))
			break;

		gtask_child = &groupmgr->gtask[pos->id];
		if (try_gdown[pos->id])
			up(&gtask_child->smp_resource);
	}

	if (try_sdown)
		smp_shot_inc(group);

	if (try_rdown)
		up(&gtask->smp_resource);

	clear_bit(IS_GROUP_SHOT, &group->state);
}

static int is_group_shot(struct is_group *group, struct is_frame *frame)
{
	int ret = 0;
	struct is_device_ischain *device;
	struct is_group_task *gtask;
	struct is_group *pos;
	struct is_group_task *gtask_child;
	bool try_sdown = false;
	bool try_rdown = false;
	bool try_gdown[GROUP_ID_MAX];
	u32 gtask_child_id;
	bool init_shot = false;
	int scenario_id;
	bool reprocessing_mode;

	FIMC_BUG(!group);
	FIMC_BUG(!group->shot_callback);
	FIMC_BUG(!group->device);
	FIMC_BUG(!frame);
	FIMC_BUG(group->instance >= IS_STREAM_COUNT);
	FIMC_BUG(group->id >= GROUP_ID_MAX);

	set_bit(IS_GROUP_SHOT, &group->state);
	memset(try_gdown, 0, sizeof(try_gdown));
	device = group->device;
	gtask = &grpmgr.gtask[group->id];

	if (unlikely(test_bit(IS_GROUP_STANDBY, &group->head->state))) {
		mgwarn(" cancel by group standby", group, group);
		ret = -EINVAL;
		goto p_err_cancel;
	}

	if (unlikely(test_bit(IS_GROUP_FORCE_STOP, &group->state))) {
		mgwarn(" cancel by fstop1", group, group);
		ret = -EINVAL;
		goto p_err_cancel;
	}

	if (unlikely(test_bit(IS_GTASK_REQUEST_STOP, &gtask->state))) {
		mgerr(" cancel by gstop1", group, group);
		ret = -EINVAL;
		goto p_err_ignore;
	}

	PROGRAM_COUNT(2);
	smp_shot_dec(group);
	try_sdown = true;

	PROGRAM_COUNT(3);
	ret = down_interruptible(&gtask->smp_resource);
	if (ret) {
		mgerr(" down fail(%d) #2", group, group, ret);
		goto p_err_ignore;
	}
	try_rdown = true;

	/* check for group stop */
	if (unlikely(test_bit(IS_GROUP_FORCE_STOP, &group->state))) {
		mgwarn(" cancel by fstop2", group, group);
		ret = -EINVAL;
		goto p_err_cancel;
	}

	if (unlikely(test_bit(IS_GTASK_REQUEST_STOP, &gtask->state))) {
		mgerr(" cancel by gstop2", group, group);
		ret = -EINVAL;
		goto p_err_ignore;
	}

	for_each_group_child(pos, group->child) {
		if (test_bit(IS_GROUP_OTF_INPUT, &group->state))
			break;

		gtask_child = &grpmgr.gtask[pos->id];
		gtask_child_id = pos->id;
		if (!test_bit(IS_GTASK_START, &gtask_child->state))
			continue;

		ret = down_interruptible(&gtask_child->smp_resource);
		if (ret) {
			mgerr(" down fail(%d) #2", group, group, ret);
			goto p_err_ignore;
		}
		try_gdown[gtask_child_id] = true;
	}

#if IS_ENABLED(ENABLE_RECURSIVE_NR)
	is_hw_update_group_state_2nr(group, frame);
#endif

	/*
	 * this statement is execued only at initial.
	 * automatic increase the frame count of sensor
	 * for next shot without real frame start
	 */
	if (group->sensor && !test_bit(IS_SENSOR_FRONT_START, &group->sensor->state) &&
	    (group->init_shots > atomic_read(&group->scount))) {
		frame->fcount = atomic_read(&group->sensor_fcount);
		atomic_set(&group->backup_fcount, frame->fcount);
		atomic_inc(&group->sensor_fcount);
		init_shot = true;

		goto p_skip_sync;
	}

	if (group->sync_shots) {
#if defined(SYNC_SHOT_ALWAYS)
		PROGRAM_COUNT(4);
		ret = down_interruptible(&group->smp_trigger);
		if (ret) {
			mgerr(" down fail(%d) #4", group, group, ret);
			goto p_err_ignore;
		}
#else
		bool try_sync_shot = false;

		if (group->asyn_shots == 0) {
			try_sync_shot = true;
		} else {
			if ((smp_shot_get(group) < MIN_OF_SYNC_SHOTS))
				try_sync_shot = true;
			else
				if (atomic_read(&group->backup_fcount) >=
					atomic_read(&group->sensor_fcount))
					try_sync_shot = true;
		}

		if (try_sync_shot) {
			PROGRAM_COUNT(4);
			ret = down_interruptible(&group->smp_trigger);
			if (ret) {
				mgerr(" down fail(%d) #4", group, group, ret);
				goto p_err_ignore;
			}
		}

#endif

		/* check for group stop */
		if (unlikely(test_bit(IS_GROUP_FORCE_STOP, &group->state))) {
			mgwarn(" cancel by fstop3", group, group);
			ret = -EINVAL;
			goto p_err_cancel;
		}

		if (unlikely(test_bit(IS_GTASK_REQUEST_STOP, &gtask->state))) {
			mgerr(" cancel by gstop3", group, group);
			ret = -EINVAL;
			goto p_err_ignore;
		}

		frame->fcount = atomic_read(&group->sensor_fcount);
		atomic_set(&group->backup_fcount, frame->fcount);

#if defined(SYNC_SHOT_ALWAYS)
		/* Nothing */
#else
		/* real automatic increase */
		if (!try_sync_shot && (smp_shot_get(group) > MIN_OF_SYNC_SHOTS)) {
			atomic_inc(&group->sensor_fcount);
		}
#endif
	} else {
		if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			frame->fcount = atomic_read(&group->sensor_fcount);
			atomic_set(&group->backup_fcount, frame->fcount);
		}
	}

p_skip_sync:
	PROGRAM_COUNT(6);

#ifdef DEBUG_AA
	is_group_debug_aa_shot(group, frame);
#endif

	ret = group->shot_callback(device, group, frame);
	if (unlikely(ret)) {
		mgerr(" shot_callback is fail(%d)", group, group, ret);
		goto p_err_cancel;
	}

	is_group_update_dvfs(device, group, frame);

	PROGRAM_COUNT(7);

	ret = is_itf_grp_shot(device, group, frame);
	if (unlikely(ret)) {
		mgerr(" is_itf_grp_shot is fail(%d)", group, group, ret);
		goto p_err_cancel;
	}

	if (init_shot) {
		set_bit(IS_SENSOR_START, &group->sensor->state);
		scenario_id = is_hw_dvfs_get_scenario(device, IS_DVFS_STATIC);
		reprocessing_mode = test_bit(IS_GROUP_REPROCESSING, &group->state);
		pablo_set_static_dvfs(&device->resourcemgr->dvfs_ctrl, "", scenario_id,
					      IS_DVFS_PATH_OTF, reprocessing_mode);
	}

	atomic_inc(&group->scount);

	clear_bit(IS_GROUP_SHOT, &group->state);
	PROGRAM_COUNT(12);
	TIME_SHOT(TMS_SHOT1);

	mgrdbgs(5, "%s\n", group, group, frame, __func__);

	return 0;

p_err_cancel:
	is_group_cancel(group, frame);

p_err_ignore:
	is_group_shot_recovery(&grpmgr, group, gtask, try_sdown, try_rdown, try_gdown);

	PROGRAM_COUNT(12);

	mgrdbgs(5, "%s: %d\n", group, group, frame, __func__, ret);

	return ret;
}

static int is_group_done(struct is_group *group, struct is_frame *frame, u32 done_state)
{
	int ret = 0;
	struct is_group_task *gtask;
	struct is_group *pos;
	struct is_group_task *gtask_child;
	struct is_sysfs_debug *sysfs_debug;

	FIMC_BUG(!group);
	FIMC_BUG(!frame);
	FIMC_BUG(group->instance >= IS_STREAM_COUNT);
	FIMC_BUG(group->id >= GROUP_ID_MAX);

	/* check shot & resource count validation */
	gtask = &grpmgr.gtask[group->id];
	sysfs_debug = is_get_sysfs_debug();

	if (unlikely(test_bit(IS_GROUP_REPROCESSING, &group->state) &&
		(done_state != VB2_BUF_STATE_DONE))) {
		merr("G%d NOT DONE(reprocessing)\n", group, group->id);
		if (test_bit(IS_HAL_DEBUG_NDONE_REPROCESSING, &sysfs_debug->hal_debug_mode)) {
			mdelay(sysfs_debug->hal_debug_delay);
			panic("HAL panic for NDONE reprocessing");
		}
	}

#ifdef DEBUG_AA
	is_group_debug_aa_done(group, frame);
#endif

	/* sensor tagging */
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state))
		is_sensor_dm_tag(group->sensor, frame);

	for_each_group_child(pos, group->child) {
		if (test_bit(IS_GROUP_OTF_INPUT, &group->state))
			break;

		gtask_child = &grpmgr.gtask[pos->id];
		if (!test_bit(IS_GTASK_START, &gtask_child->state))
			continue;

		up(&gtask_child->smp_resource);
	}

	smp_shot_inc(group);
	up(&gtask->smp_resource);

	if (unlikely(frame->result)) {
		ret = is_devicemgr_shot_done(group, frame, frame->result);
		if (ret) {
			mgerr("is_devicemgr_shot_done is fail(%d)", group, group, ret);
			return ret;
		}
	}

#ifdef ENABLE_STRIPE_SYNC_PROCESSING
	/* Re-trigger the group shot for next stripe processing. */
	if (is_vendor_check_remosaic_mode_change(frame)
			&& (frame->state == FS_REPEAT_PROCESS))
		is_group_start_trigger(&grpmgr, group, frame);
#endif

	mgrdbgs(5, "%s\n", group, group, frame, __func__);

	return ret;
}

static int is_group_change_chain(struct is_group *group, u32 next_id)
{
	int ret = 0;
	int curr_id;
	struct is_group_task *curr_gtask;
	struct is_group_task *next_gtask;
	struct is_group *pos;
	u32 base_id;

	if (group->slot != GROUP_SLOT_SENSOR) {
		mgerr("The group is not a sensor group.", group, group);
		return -EINVAL;
	}

	if (!test_bit(IS_GROUP_OTF_OUTPUT, &group->state)) {
		mgerr("The group output is not a OTF.", group, group);
		return -EINVAL;
	}

	ret = is_itf_change_chain_wrap(group, next_id);
	if (ret) {
		mgerr("is_itf_change_chain_wrap is fail (next_id: %d)", group, group, next_id);
		goto p_err;
	}

	for_each_group_child(pos, group->child) {
		if (pos->slot == GROUP_SLOT_PAF) {
			base_id = GROUP_ID_PAF0;
			curr_id = pos->id - base_id;
		} else if (pos->slot == GROUP_SLOT_3AA) {
			base_id = GROUP_ID_3AA0;
			curr_id = pos->id - base_id;
		} else {
			mgerr("A child group is invalid.", group, group);
			goto p_err;
		}

		if (curr_id == next_id)
			continue;

		curr_gtask = &grpmgr.gtask[pos->id];
		ret = is_group_task_stop(curr_gtask);
		if (ret) {
			mgerr("is_group_task_stop is fail(%d)", pos, pos, ret);
			goto p_err;
		}

		next_gtask = &grpmgr.gtask[base_id + next_id];
		ret = is_group_task_start(next_gtask, pos->slot);
		if (ret) {
			mgerr("is_group_task_start is fail(%d)", pos, pos, ret);
			goto p_err;
		}

		mginfo("%s: done (%d --> %d)\n", pos, pos, __func__, curr_id, next_id);

		pos->id = base_id + next_id;
	}

	is_group_set_hw_map(group->head, &is_get_is_core()->hardware, false);

p_err:
	clear_bit(IS_GROUP_STANDBY, &group->state);

	return ret;
}

static const struct is_group_ops ig_ops = {
	.lock = is_group_lock,
	.unlock = is_group_unlock,
	.subdev_cancel = is_group_subdev_cancel,
	.open = is_group_open,
	.close = is_group_close,
	.init = is_group_init,
	.start = is_group_start,
	.stop = is_group_stop,
	.buffer_init = pablo_group_buffer_init,
	.buffer_queue = is_group_buffer_queue,
	.buffer_finish = is_group_buffer_finish,
	.shot = is_group_shot,
	.done = is_group_done,
	.change_chain = is_group_change_chain,
};

void is_group_release(struct is_device_ischain *device, u32 slot)
{
	if (slot >= GROUP_SLOT_MAX)
		return;

	if (device && device->group[slot]) {
		kvfree(device->group[slot]);
		device->group[slot] = NULL;
	}
}

int is_group_probe(struct is_device_sensor *sensor,
	struct is_device_ischain *device,
	is_shot_callback shot_callback,
	u32 slot)
{
	int i;
	struct is_group *group;
	const struct is_subdev_ops *sops;
	const char *name;
	u32 stream, id;
	enum is_device_type device_type;

	if (slot >= GROUP_SLOT_MAX)
		return -EINVAL;

	if (device) {
		stream = device->instance;
		device_type = IS_DEVICE_ISCHAIN;
		group = kvzalloc(sizeof(struct is_group), GFP_KERNEL);
		if (ZERO_OR_NULL_PTR(group)) {
			minfo("failed to alloc group slot(%d)\n", device, slot);
			return -ENOMEM;
		}
		device->group[slot] = group;
	} else if (sensor) {
		stream = IS_STREAM_COUNT;
		device_type = IS_DEVICE_SENSOR;
		group = &sensor->group_sensor;
	} else {
		err("device and sensor are NULL");
		return -EINVAL;
	}

	is_hw_get_subdev_info(slot, &id, &name, &sops);
	is_subdev_probe(&group->leader, stream, id, name, sops);

	group->id = GROUP_ID_MAX;
	group->slot = slot;
	group->shot_callback = shot_callback;
	group->device = device;
	group->sensor = sensor;
	group->instance = stream;
	group->device_type = device_type;
	mutex_init(&group->mlock_votf);
	spin_lock_init(&group->slock_s_ctrl);
	group->ops = &ig_ops;
	group->state = 0UL;
	INIT_LIST_HEAD(&group->subdev_list);

	for (i = ENTRY_SENSOR; i < ENTRY_END; i++)
		group->subdev[i] = NULL;

	return 0;
}

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
static struct pablo_kunit_groupmgr_func groupmgr_func = {
	.group_lock = is_group_lock,
	.group_unlock = is_group_unlock,
	.group_subdev_cancel = is_group_subdev_cancel,
	.group_cancel = is_group_cancel,
	.group_task_start = is_group_task_start,
	.group_task_stop = is_group_task_stop,
	.wait_req_frame_before_stop = wait_req_frame_before_stop,
	.wait_shot_thread = wait_shot_thread,
	.wait_pro_frame = wait_pro_frame,
	.wait_req_frame_after_stop = wait_req_frame_after_stop,
	.group_override_meta = is_group_override_meta,
	.group_override_sensor_req = is_group_override_sensor_req,
	.check_remain_req_count = check_remain_req_count,
	.group_has_child = __group_has_child,
	.group_throttling_tick_check = is_group_throttling_tick_check,
	.group_shot_recovery = is_group_shot_recovery,
	.group_release = is_group_release,
	.group_probe = is_group_probe,
};

struct pablo_kunit_groupmgr_func *pablo_kunit_get_groupmgr_func(void)
{
	return &groupmgr_func;
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_get_groupmgr_func);
#endif
