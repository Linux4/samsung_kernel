// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/string.h>
#include <linux/slab.h>
#include <linux/moduleparam.h>

#include "punit-group-test.h"
#include "is-config.h"
#include "is-groupmgr.h"
#include "is-core.h"
#include "is-device-ischain.h"
#include "is-interface-wrap.h"
#include "punit-test-crta-mock.h"
#include "punit-test-criteria.h"
#include "punit-test-param.h"

static int set_punit_group_test(const char *val, const struct kernel_param *kp);
static int get_punit_group_test(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops param_ops_punit_group_test = {
	.set = set_punit_group_test,
	.get = get_punit_group_test,
};
module_param_cb(punit_group_test, &param_ops_punit_group_test, NULL, 0644);

static struct punit_group_test_static_info pgt_st_info[IS_STREAM_COUNT];
static struct punit_group_test_ctx *test_grp_ctx[IS_STREAM_COUNT];
static bool result;

/* TODO: move static data */
static struct is_fmt fmt = {
	.name		= "BAYER 10 bit packed",
	.pixelformat	= V4L2_PIX_FMT_SBGGR10P,
	.num_planes	= 1 + NUM_OF_META_PLANE,
	.bitsperpixel	= { 10 },
	.hw_format	= DMA_INOUT_FORMAT_BAYER_PACKED,
	.hw_order	= DMA_INOUT_ORDER_GB_BG,
	.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
	.hw_plane	= DMA_INOUT_PLANE_1,
};

static void pgt_set_capnode(u32 index, struct is_frame *frame, u32 vid,
				u32 pixelformat, u32 in[], u32 out[])
{
	struct camera2_node_group *node_group = &frame->shot_ext->node_group;
	struct camera2_node *cap_node = &node_group->capture[index];
	struct is_sub_frame *sframe;

	cap_node->request = 1;
	cap_node->vid = vid;
	cap_node->pixelformat = pixelformat;

	cap_node->input.cropRegion[0] = in[0];
	cap_node->input.cropRegion[1] = in[1];
	cap_node->input.cropRegion[2] = in[2];
	cap_node->input.cropRegion[3] = in[3];

	cap_node->output.cropRegion[0] = out[0];
	cap_node->output.cropRegion[1] = out[1];
	cap_node->output.cropRegion[2] = out[2];
	cap_node->output.cropRegion[3] = out[3];

	sframe = &frame->cap_node.sframe[index];
	sframe->id = cap_node->vid;
}

static void pgt_set_node_group(struct is_group *group, struct is_frame *frame)
{
	struct punit_group_test_static_info *st_info = &pgt_st_info[group->instance];
	struct camera2_node_group *node_group = &frame->shot_ext->node_group;
	struct camera2_node *ldr_node = &node_group->leader;
	u32 sensor_width = st_info->sensor_width;
	u32 sensor_height = st_info->sensor_height;
	u32 in[4], out[4];

	set_bit(IS_GROUP_STRGEN_INPUT, &group->state); /* TODO: remove */

	/*
	 * TODO: This is just for example.
	 * These parameters should be come from user setting.
	 */
	ldr_node->request = 1;
	ldr_node->vid = IS_VIDEO_BYRP;
	ldr_node->pixelformat = V4L2_PIX_FMT_SBGGR10P;
	ldr_node->width = sensor_width;
	ldr_node->height = sensor_height;

	ldr_node->input.cropRegion[0] = 0;
	ldr_node->input.cropRegion[1] = 0;
	ldr_node->input.cropRegion[2] = sensor_width;
	ldr_node->input.cropRegion[3] = sensor_height;

	ldr_node->output.cropRegion[0] = 0;
	ldr_node->output.cropRegion[1] = 0;
	ldr_node->output.cropRegion[2] = sensor_width;
	ldr_node->output.cropRegion[3] = sensor_height;

	CROP_IN(0, 0, sensor_width, sensor_height);
	CROP_OUT(0, 0, sensor_width, sensor_height);
	pgt_set_capnode(0, frame, IS_LVN_BYRP_BYR, V4L2_PIX_FMT_SBGGR10, in, out);
}

static int pgt_check_q_cnt(struct is_group *group)
{
	struct is_framemgr *framemgr = GET_HEAD_GROUP_FRAMEMGR(group);
	u32 r, p, c, q_cnt, try_cnt = 100;

	do {
		fsleep(PGT_DQ_WAIT_TIME);
		r = framemgr->queued_count[FS_REQUEST];
		p = framemgr->queued_count[FS_PROCESS];
		c = framemgr->queued_count[FS_COMPLETE];
		q_cnt = r + p + c;
		mgwarn("%s: [R:%d, P:%d, C:%d, try:%d]", group, group, __func__, r, p, c, try_cnt);
	} while (try_cnt-- && q_cnt);

	return (q_cnt > 0) ? -EBUSY : 0;
}

static int pgt_qbuf_group(struct is_group *head, u32 fcount)
{
	int ret;
	struct is_queue *queue = head->leader.vctx->queue;
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	framemgr = &queue->framemgr;
	frame = peek_frame(framemgr, FS_FREE);
	if (!frame) {
		mgerr("free frame is NULL", head, head);
		return -EINVAL;
	}

	frame->shot->dm.request.frameCount = fcount;
	pgt_set_node_group(head, frame);

	ret = CALL_GROUP_OPS(head, buffer_queue, queue, frame->index);
	if (ret) {
		mgerr("fail buffer_queue(%d)", head, head, ret);
		return ret;
	}

	mginfo("Q [I:%d]\n", head, head, frame->index);

	return 0;
}

static int pgt_dqbuf_group(struct is_group *head)
{
	int ret;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	u32 cur_fcount, ndone, com_cnt, i;
	struct punit_group_test_ctx *t_ctx = test_grp_ctx[head->instance];

	framemgr = GET_HEAD_GROUP_FRAMEMGR(head);

	com_cnt = framemgr->queued_count[FS_COMPLETE];
	if (!com_cnt) {
		mgerr("There is no complete frame.", head, head);
		return -EINVAL;
	}

	for (i = 0; i < com_cnt; i++) {
		frame = peek_frame(framemgr, FS_COMPLETE);

		cur_fcount = frame->fcount;
		ndone = frame->result;

		ret = CALL_GROUP_OPS(head, buffer_finish, frame->index);
		if (ret) {
			mgerr("fail buffer_finish(%d)", head, head, ret);
			continue;
		}

		mgrinfo("DQ [I:%d] [F:%d, R:%d, P:%d, C:%d]\n", head, head, frame, frame->index,
			framemgr->queued_count[FS_FREE], framemgr->queued_count[FS_REQUEST],
			framemgr->queued_count[FS_PROCESS], framemgr->queued_count[FS_COMPLETE]);

		if (head->slot == GROUP_SLOT_SENSOR && !t_ctx->stop)
			pgt_qbuf_group(head, 0);

		/* qbuf next group */
		if (head->gnext && !ndone)
			pgt_qbuf_group(head->gnext, cur_fcount);

		punit_check_dq_criteria(head, frame);
	}

	return 0;
}

static void pgt_dq_work_fn(struct kthread_work *work)
{
	struct punit_group_test_work_info *work_info =
			container_of(work, struct punit_group_test_work_info, work);
	struct punit_group_test_static_info *st_info = &pgt_st_info[work_info->instance];
	struct is_device_ischain *idi;
	struct is_device_sensor *ids;
	struct is_group *head;

	if (work_info->slot == GROUP_SLOT_SENSOR) {
		ids = is_get_sensor_device(st_info->sensor_id);
		head = &ids->group_sensor;
	} else {
		idi = is_get_ischain_device(work_info->instance);
		head = idi->group[work_info->slot];
	}

	pgt_dqbuf_group(head);
}

static int pgt_make_dq_thread(u32 instance, u32 slot)
{
	struct punit_group_test_ctx *t_ctx = test_grp_ctx[instance];

	kthread_init_worker(&t_ctx->worker[slot]);
	t_ctx->task_dq[slot] = kthread_run(kthread_worker_fn, &t_ctx->worker[slot], "punit-dq-gslot%d", slot);
	if (IS_ERR(t_ctx->task_dq[slot])) {
		err("failed to create kthread");
		t_ctx->task_dq[slot] = NULL;
		return -EINVAL;
	}

	t_ctx->work_info[slot].instance = instance;
	t_ctx->work_info[slot].slot = slot;
	kthread_init_work(&t_ctx->work_info[slot].work, pgt_dq_work_fn);

	return 0;
}

static int pgt_vops_done(struct is_video_ctx *vctx, u32 index, u32 state)
{
	u32 slot = vctx->group->slot;
	struct punit_group_test_ctx *t_ctx = test_grp_ctx[vctx->group->instance];

	kthread_queue_work(&t_ctx->worker[slot], &t_ctx->work_info[slot].work);

	return 0;
}

static void pgt_init_frame(struct is_frame *frame, struct is_group *group)
{
	struct camera2_shot_ext *shot_ext = kvzalloc(sizeof(struct camera2_shot_ext), GFP_KERNEL);

	frame->shot_ext = shot_ext;
	frame->shot = &shot_ext->shot;
	set_bit(FRAME_MEM_INIT, &frame->mem_state);
	frame->num_buffers = 1;

	CALL_GROUP_OPS(group, buffer_init, frame->index);
}

static void pgt_deinit_frame(struct is_frame *frame)
{
	kvfree(frame->shot_ext);
	frame->shot_ext = NULL;
	frame->shot = NULL;
}

static int pgt_init_vctx(struct is_video_ctx *vctx, struct is_group *group, struct is_video *video)
{
	struct is_queue *queue;

	queue = kvzalloc(sizeof(struct is_queue), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(queue)) {
		mgerr("is_queue alloc fail", group, group);
		return -ENOMEM;
	}

	vctx->queue = queue;
	vctx->group = group;
	vctx->video = video;
	vctx->vops.done = pgt_vops_done;

	frame_manager_probe(&queue->framemgr, queue->name);

	return 0;
}

static void pgt_deinit_vctx(struct is_video_ctx *vctx)
{
	int i;
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	framemgr = &vctx->queue->framemgr;
	for (i = 0; i < framemgr->num_frames; i++) {
		frame = &framemgr->frames[i];
		pgt_deinit_frame(frame);
	}

	frame_manager_close(framemgr);

	kvfree(vctx->queue);
	vctx->queue = NULL;
	vctx->group = NULL;
	vctx->video = NULL;
	vctx->vops.done = NULL;
}

static void pgt_init_sensor(u32 instance)
{
	struct punit_group_test_static_info *st_info = &pgt_st_info[instance];
	u32 sensor_id = st_info->sensor_id;
	u32 sensor_position = st_info->sensor_position;
	struct is_device_ischain *device = is_get_ischain_device(instance);
	struct is_device_sensor *ids;
	struct punit_group_test_ctx *t_ctx = test_grp_ctx[instance];
	struct is_video_ctx *vctx_ss = &t_ctx->vctx[GROUP_SLOT_SENSOR];
	struct is_queue *queue_ss;
	struct v4l2_streamparm param;
	int ret;

	ret = is_resource_open(&is_get_is_core()->resourcemgr, RESOURCE_TYPE_SENSOR0 + sensor_id, (void **)&ids);
	if (ret) {
		err("fail is_resource_open(%d)", ret);
		return;
	}

	pgt_init_vctx(vctx_ss, &ids->group_sensor, &ids->video);
	queue_ss = vctx_ss->queue;
	queue_ss->qops = is_get_sensor_device_qops();

	/* open */
	is_sensor_open(ids, vctx_ss);

	/* s_input */
	is_sensor_s_input(ids, sensor_position, SENSOR_SCENARIO_NORMAL, 1);

	/* s_framerate */
	param.parm.capture.timeperframe.denominator = 30;
	param.parm.capture.timeperframe.numerator = 1;
	param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	is_sensor_s_framerate(ids, &param);

	/* s_fmt */
	queue_ss->framecfg.format = &fmt;
	ids->sensor_width = st_info->sensor_width;
	ids->sensor_height = st_info->sensor_height;
	ids->ex_mode = EX_NONE;
	CALL_QOPS(queue_ss, s_fmt, ids, queue_ss);

	ids->ischain = device;
	device->sensor = ids;
}

static void pgt_start_sensor(u32 instance)
{
	struct punit_group_test_static_info *st_info = &pgt_st_info[instance];
	u32 sensor_id = st_info->sensor_id;
	struct is_device_sensor *ids = is_get_sensor_device(sensor_id);
	struct punit_group_test_ctx *t_ctx = test_grp_ctx[instance];
	struct is_video_ctx *vctx = &t_ctx->vctx[GROUP_SLOT_SENSOR];
	struct is_queue *queue = vctx->queue;
	struct is_frame *frame;
	int i, ret;

	CALL_QOPS(queue, start_streaming, ids, queue);

	/* reqbuf */
	queue->buf_maxcount = st_info->buf_maxcount;
	frame_manager_open(&queue->framemgr, queue->buf_maxcount, true);

	/* qbuf */
	for (i = 0; i < queue->buf_maxcount; i++) {
		frame = &queue->framemgr.frames[i];
		pgt_init_frame(frame, &ids->group_sensor);

		ret = pgt_qbuf_group(&ids->group_sensor, 0);
		if (ret)
			return;
	}

	ret = pgt_make_dq_thread(instance, GROUP_SLOT_SENSOR);
	if (ret)
		return;

	is_sensor_front_start(ids, 0, 1);
}

static int pgt_init_ischain(u32 instance)
{
	int ret;
	u32 slot, id, stream_type, leader;
	struct punit_group_test_static_info *st_info = &pgt_st_info[instance];
	u32 sensor_id = st_info->sensor_id;
	struct is_device_ischain *idi = is_get_ischain_device(instance);
	struct is_device_sensor *ids = is_get_sensor_device(sensor_id);
	struct is_group *group;
	IS_DECLARE_PMAP(pmap);
	struct punit_group_test_ctx *t_ctx = test_grp_ctx[instance];
	bool enable_ischain = false;
	bool end_of_sensor_otf = false;

	if (st_info->enable[GROUP_SLOT_SENSOR]) {
		stream_type = IS_PREVIEW_STREAM;
		clear_bit(IS_ISCHAIN_REPROCESSING, &idi->state); /* TODO: remove */
		leader = 0;
	} else {
		stream_type = IS_REPROCESSING_STREAM;
		set_bit(IS_ISCHAIN_REPROCESSING, &idi->state); /* TODO: remove */
		idi->sensor = ids;
		leader = 1;
	}

	for (slot = GROUP_SLOT_SENSOR + 1; slot < GROUP_SLOT_MAX; slot++) {
		if (!st_info->enable[slot])
			continue;

		enable_ischain = true;

		if (st_info->input[slot] == GROUP_INPUT_MEMORY)
			end_of_sensor_otf = true;
		else
			leader = 0;

		group = idi->group[slot];
		id = slot_to_gid[slot];
		id += end_of_sensor_otf ? 0 : st_info->lic_ch;

		pgt_init_vctx(&t_ctx->vctx[slot], group, NULL);
		ret = CALL_GROUP_OPS(group, open, id, &t_ctx->vctx[slot]);
		if (ret)
			return ret;

		ret = CALL_GROUP_OPS(group, init, st_info->input[slot], leader, stream_type);
		if (ret)
			return ret;
	}

	if (!enable_ischain)
		return 0;

	set_bit(instance, &idi->ginstance_map);
	set_bit(IS_ISCHAIN_OPEN, &idi->state);
	set_bit(IS_ISCHAIN_INIT, &idi->state);

	if (stream_type == IS_PREVIEW_STREAM)
		set_bit(IS_SENSOR_OTF_OUTPUT, &ids->state);

	ret = is_groupmgr_init(idi);
	if (ret)
		return ret;

	ret = is_itf_open_wrap(idi, stream_type);
	if (ret)
		return ret;

	IS_INIT_PMAP(pmap);
	ret = is_itf_s_param_wrap(idi, pmap);
	if (ret)
		return ret;

	return 0;
}

static int pgt_start_ischain(u32 instance)
{
	struct punit_group_test_static_info *st_info = &pgt_st_info[instance];
	int ret;
	u32 i, slot;
	struct is_group *gnext;
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	gnext = get_leader_group(instance);
	while (gnext) {
		slot = gnext->slot;

		if (slot == GROUP_SLOT_SENSOR) {
			gnext = gnext->gnext;
			continue;
		}

		framemgr = GET_HEAD_GROUP_FRAMEMGR(gnext);
		frame_manager_open(framemgr, st_info->buf_maxcount, true);

		for (i = 0; i < framemgr->num_frames; i++) {
			frame = &framemgr->frames[i];
			pgt_init_frame(frame, gnext);
		}

		ret = CALL_GROUP_OPS(gnext, start);
		if (ret)
			return ret;

		ret = pgt_make_dq_thread(instance, slot);
		if (ret)
			return ret;

		gnext = gnext->gnext;
	}

	return 0;
}

static int pgt_qbuf_ischain(u32 instance)
{
	int ret, i;
	struct is_device_ischain *idi = is_get_ischain_device(instance);
	struct is_group *head = get_leader_group(instance);
	struct is_framemgr *framemgr;

	ret = is_groupmgr_start(idi);
	if (ret)
		return ret;

	ret = is_itf_process_on_wrap(idi, head);
	if (ret)
		return ret;

	framemgr = GET_HEAD_GROUP_FRAMEMGR(head);
	for (i = 0; i < framemgr->num_frames; i++) {
		ret = pgt_qbuf_group(head, 0);
		if (ret)
			return ret;
	}

	return 0;
}

static int pgt_open(void)
{
	punit_init_criteria();

	return 0;
}

static int pgt_start(u32 instance)
{
	int ret;
	struct punit_group_test_ctx *t_ctx;
	struct punit_group_test_static_info *st_info = &pgt_st_info[instance];

	t_ctx = test_grp_ctx[instance] = kvzalloc(sizeof(struct punit_group_test_ctx), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(test_grp_ctx[instance])) {
		mierr("not enough memory for test_grp_ctx", instance);
		return -ENOMEM;
	}

	if (st_info->enable[GROUP_SLOT_SENSOR]) {
		t_ctx->stop = false;
		pgt_init_sensor(instance);
	}

	ret = pgt_init_ischain(instance);
	if (ret) {
		mierr("fail pgt_init_ischain(%d)", instance, ret);
		goto err;
	}

	ret = pgt_start_ischain(instance);
	if (ret) {
		mierr("fail pgt_start_ischain(%d)", instance, ret);
		goto err;
	}

	if (st_info->enable[GROUP_SLOT_SENSOR]) {
		pgt_start_sensor(instance);
	} else {
		ret = pgt_qbuf_ischain(instance);
		if (ret) {
			mierr("fail pgt_qbuf_ischain(%d)", instance, ret);
			goto err;
		}
	}

	punit_check_start_criteria(instance);

	return 0;

err:
	kvfree(test_grp_ctx[instance]);

	return ret;
}

static void pgt_stop(u32 instance)
{
	struct is_device_ischain *idi = is_get_ischain_device(instance);
	struct punit_group_test_static_info *st_info = &pgt_st_info[instance];
	struct punit_group_test_ctx *t_ctx = test_grp_ctx[instance];
	int slot, ret = 0;
	struct is_group *gnext, *leader = get_leader_group(instance);
	struct is_device_sensor *ids = is_get_sensor_device(st_info->sensor_id);

	if (st_info->enable[GROUP_SLOT_SENSOR]) {
		t_ctx->stop = true;
		ret |= pgt_check_q_cnt(leader);

		is_sensor_front_stop(ids, true);
	}

	gnext = leader;
	while (gnext) {
		ret |= pgt_check_q_cnt(gnext);
		kthread_stop(t_ctx->task_dq[gnext->slot]);
		gnext = gnext->gnext;
	}

	result = !ret ? PGT_PASS : PGT_FAIL;

	is_itf_close_wrap(idi);
	for (slot = 0; slot < GROUP_SLOT_MAX; slot++) {
		if (!st_info->enable[slot])
			continue;

		if (slot == GROUP_SLOT_SENSOR)
			is_sensor_close(ids);
		else
			CALL_GROUP_OPS(idi->group[slot], close);

		pgt_deinit_vctx(&t_ctx->vctx[slot]);
	}

	clear_bit(IS_ISCHAIN_INIT, &idi->state);
	clear_bit(IS_ISCHAIN_OPEN, &idi->state);

	punit_check_stop_criteria(instance);

	kvfree(test_grp_ctx[instance]);
}

static int pgt_close(void)
{
	bool criteria_result;

	criteria_result = punit_finish_criteria();
	result &= criteria_result;

	return 0;
}

static inline struct punit_group_test_map *__get_map(char *key)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(map_static_info); i++) {
		if (!strncmp(key, map_static_info[i].key, 15))
			return &map_static_info[i];
	}

	err("[PUNIT] invalid key(%s)", key);
	return NULL;
}

static int pgt_set_static_info(int argc, char **argv)
{
	int ret, arg_i = 1;
	char *key;
	u32 value, instance, idx, count;
	u32 *st_info;
	struct punit_group_test_map *map;

	if (argc < 4) {
		pr_err("%s: Not enough parameters. %d < 4\n", __func__, argc);
		return -EINVAL;
	}

	ret = kstrtouint(argv[arg_i++], 0, &instance);
	if (ret) {
		pr_err("%s: Invalid parameter[%d](ret %d)\n", __func__, arg_i, ret);
		return ret;
	}

	if (instance >= IS_STREAM_COUNT) {
		err("[PUNIT] invalid instance(%d)", instance);
		return -EINVAL;
	}

	st_info = (u32 *)&pgt_st_info[instance];

	key = argv[arg_i++];
	map = __get_map(key);
	if (!map)
		return -EINVAL;

	for (count = 0; count < map->count && arg_i < argc; count++, arg_i++) {
		ret = kstrtouint(argv[arg_i], 0, &value);
		if (ret) {
			pr_err("%s: Invalid parameter[%d](ret %d)\n", __func__, arg_i, ret);
			return ret;
		}

		idx = map->idx + count;

		st_info[idx] = value;
		pr_info("%s: [%d] %s(%d): %d\n", __func__, instance, key, idx, value);
	}

	return 0;
}

static int set_punit_group_test(const char *val, const struct kernel_param *kp)
{
	int ret, argc, arg_i = 0;
	char **argv;
	enum punit_group_test_act act;
	u32 instance;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		pr_err("No argument!\n");
		return -EINVAL;
	}

	if (argc < 2) {
		pr_err("Not enough parameters. %d < 2\n", argc);
		ret = -EINVAL;
		goto func_exit;
	}

	ret = kstrtouint(argv[arg_i++], 0, &act);
	if (ret) {
		pr_err("Invalid act %d ret %d\n", act, ret);
		goto func_exit;
	}

	ret = kstrtouint(argv[arg_i++], 0, &instance);
	if (ret) {
		pr_err("Invalid instance %d ret %d\n", instance, ret);
		goto func_exit;
	}

	switch (act) {
	case PUNIT_GROUP_TEST_ACT_STOP:
		pgt_stop(instance);
		pst_deinit_crta_mock();
		break;
	case PUNIT_GROUP_TEST_ACT_START:
		pst_init_crta_mock();
		ret = pgt_start(instance);
		if (ret) {
			mierr("fail pgt_start(%d)", instance, ret);
			goto func_exit;
		}
		break;
	case PUNIT_GROUP_TEST_ACT_STATIC_INFO:
		ret = pgt_set_static_info(argc, argv);
		if (ret) {
			mierr("fail pgt_set_static_info(%d)", instance, ret);
			goto func_exit;
		}
		break;
	case PUNIT_GROUP_TEST_ACT_OPEN:
		pgt_open();
		break;
	case PUNIT_GROUP_TEST_ACT_CLOSE:
		pgt_close();
		break;
	case PUNIT_GROUP_TEST_ACT_CRITERIA_INFO:
		ret = punit_set_test_criteria_info(argc, argv);
		if (ret) {
			mierr("fail punit_set_test_criteria_info(%d)", instance, ret);
			goto func_exit;
		}
		break;
	case PUNIT_GROUP_TEST_ACT_SET_PARAM:
		ret = punit_set_test_param_info(argc, argv);
		if (ret) {
			mierr("fail punit_set_test_param_info(%d)", instance, ret);
			goto func_exit;
		}
		break;
	default:
		mierr("NOT supported act %u", instance, act);
		goto func_exit;
	}

	pr_info("[%d] %s\n", instance, group_act[act]);

func_exit:
	argv_free(argv);
	return ret;
}

static int get_punit_group_test(char *buffer, const struct kernel_param *kp)
{
	int ret;
	size_t num_of_tc = 1;

	if (punit_has_remained_criteria_result_msg()) {
		char result_msg[1024];

		ret = punit_get_remained_criteria_result_msg(result_msg);
		if (ret > 0)
			ret = sprintf(buffer, "%d %s", punit_get_criteria_count(), result_msg);

		pr_info("%s: CRITERIA_RES(%d) %s\n", __func__, ret, result_msg);
		return ret;
	}

	pr_info("%s: TC#(%lu)\n", __func__, num_of_tc);
	ret = sprintf(buffer, "%zu ", num_of_tc);
	ret += sprintf(buffer + ret, "%x ", result);
	result = 0;

	return ret;
}
