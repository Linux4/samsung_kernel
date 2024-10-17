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

#define pr_fmt(fmt) "[" KBUILD_MODNAME "] Pablo-V4L2-Test: " fmt

#include <linux/completion.h>
#include <linux/refcount.h>
#include <linux/delay.h>

#include "is-video.h"
#include "is-device-sensor.h"
#include "videodev2_exynos_camera.h"
#include "pablo-icpu-adapter.h"
#include "is-core.h"
#include "is-hw.h"
#include "pablo-mem.h"

#define USE_RESULT_VID 0

#define MAX_RESULT 32
#define WAIT_TIMEOUT 1000
#define TEST_WIDTH 4000
#define TEST_HEIGHT 3000
#define TEST_PLANES 2

struct pablo_v4l2_test_case {
	char *name;
	int (*run) (struct file *file);

	int id;
	char *result;
};

#define PABLO_V4L2_TEST_CASE(_name, _run) {	\
	.name = _name,				\
	.run = _run,				\
}

static DEFINE_MUTEX(pre_post_mutex);

static struct pablo_v4l2_test_case *latest_case;

static struct completion sensor_s_input_wait;
static struct completion ischain_s_input_wait;
static struct completion ischain_streamon_wait;

static struct pablo_icpu_adt pia_fake;
static struct pablo_icpu_adt *pia_orig;
static refcount_t pia_ref = REFCOUNT_INIT(0);

static struct is_priv_buf *pb;
static struct v4l2_plane vp[TEST_PLANES];

static int pvt_wait_for_completion(struct completion *x, ulong timeout)
{
	long t;

	t = wait_for_completion_interruptible_timeout(x,
					msecs_to_jiffies(timeout));
	if (t < 0)
		return (int)t;
	else if (t == 0)
		return -ETIME;

	return 0;
}

static int __pvt_s_input(struct file *file, u32 s)
{
	pr_debug("is_vidioc_s_input: 0x%x\n", s);

	return is_vidioc_s_input(file, NULL, s);
}

static int __pvt_s_ctrl(struct file *file, u32 id, s32 value)
{
	struct is_video *iv = video_drvdata(file);
	struct v4l2_control c;

	c.id = id;
	c.value = value;

	pr_debug("is_vidioc_s_ctrl: 0x%x, value=%d\n", id, value);

	if (iv->device_type == IS_DEVICE_SENSOR)
		return is_ssx_video_s_ctrl(file, NULL, &c);
	else
		return is_vidioc_s_ctrl(file, NULL, &c);
}

static int __pvt_s_parm(struct file *file, u32 framerate)
{
	struct is_video *iv = video_drvdata(file);
	struct v4l2_streamparm p;

	if (!iv->vd.ioctl_ops->vidioc_s_parm)
		return -EINVAL;

	p.parm.capture.timeperframe.denominator = framerate;
	p.parm.capture.timeperframe.numerator = 1;
	p.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	pr_debug("vidioc_s_parm: framerate=%d\n", framerate);

	return iv->vd.ioctl_ops->vidioc_s_parm(file, NULL, &p);
}

static int __pvt_g_parm(struct file *file, u32 framerate)
{
	struct is_video *iv = video_drvdata(file);
	struct v4l2_streamparm p;
	int ret;

	if (!iv->vd.ioctl_ops->vidioc_g_parm)
		return -EINVAL;

	p.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	pr_debug("vidioc_g_parm: framerate=%d\n", framerate);

	ret = iv->vd.ioctl_ops->vidioc_g_parm(file, NULL, &p);
	if (ret)
		return ret;

	if (p.parm.capture.timeperframe.denominator != framerate)
		return -EINVAL;

	return 0;
}

static int __pvt_s_fmt_mplane(struct file *file, u32 pixelformat, u32 width, u32 height)
{
	struct v4l2_format f;

	f.fmt.pix_mp.pixelformat = pixelformat;
	f.fmt.pix_mp.colorspace = V4L2_COLORSPACE_DEFAULT;
	f.fmt.pix_mp.quantization = V4L2_QUANTIZATION_DEFAULT;
	f.fmt.pix_mp.width = width;
	f.fmt.pix_mp.height = height;
	f.fmt.pix_mp.flags = CAMERA_PIXEL_SIZE_8BIT;

	pr_debug("vidioc_s_fmt_mplane: %p4cc, %uX%u\n", &pixelformat, width, height);

	return is_vidioc_s_fmt_mplane(file, NULL, &f);
}

static int __pvt_reqbufs(struct file *file, u32 count)
{
	struct v4l2_requestbuffers b;

	b.count = count;
	b.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	b.memory = V4L2_MEMORY_DMABUF;

	return is_vidioc_reqbufs(file, NULL, &b);
}

static int __pvt_streamon(struct file *file)
{
	return is_vidioc_streamon(file, NULL,
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
}

static int __pvt_streamoff(struct file *file)
{
	return is_vidioc_streamoff(file, NULL,
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
}


static int __pvt_qbuf(struct file *file)
{
	struct v4l2_buffer b;
	struct camera2_shot_ext *shot_ext;
	int i;

	b.index = 0;
	b.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	b.bytesused = 0;
	b.memory = V4L2_MEMORY_DMABUF;
	b.m.planes = vp;
	b.length = TEST_PLANES;

	shot_ext = (struct camera2_shot_ext *)CALL_BUFOP(pb, kvaddr, pb);
	shot_ext->shot.magicNumber = SHOT_MAGIC_NUMBER;

	for (i = 0; i < TEST_PLANES; i++)
		vp[i].m.fd = dma_buf_fd(pb->dma_buf, 0);

	is_vidioc_qbuf(file, NULL, &b);

	for (i = 0; i < TEST_PLANES; i++)
		put_unused_fd(vp[i].m.fd);

	return -EINVAL;
}

static int __pvt_dqbuf(struct file *file)
{
	struct v4l2_buffer b;
	struct is_video *iv = video_drvdata(file);

	b.index = 0;
	b.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	b.bytesused = 0;
	b.memory = V4L2_MEMORY_DMABUF;
	b.m.planes = vp;
	b.length = TEST_PLANES;

	is_vidioc_dqbuf(file, NULL, &b);

	/* It needs unlock after .wait_finish callback in vb2_dqbuf(). */
	mutex_unlock(&iv->lock);

	return -EINVAL;
}

static int pvt_querycap(struct file *file)
{
	struct is_video *iv = video_drvdata(file);
	struct v4l2_capability c;
	int ret;

	c.device_caps = 0;
	c.capabilities = 0;
	ret = is_vidioc_querycap(file, NULL, &c);
	if (ret)
		return ret;

	if (iv->video_type == IS_VIDEO_TYPE_LEADER) {
		if (c.capabilities != (V4L2_CAP_STREAMING
					| V4L2_CAP_VIDEO_OUTPUT
					| V4L2_CAP_VIDEO_OUTPUT_MPLANE))
			ret = -EINVAL;
	} else {
		if (c.capabilities != (V4L2_CAP_STREAMING
					| V4L2_CAP_VIDEO_CAPTURE
					| V4L2_CAP_VIDEO_CAPTURE_MPLANE))
			ret = -EINVAL;
	}

	return ret;
}

static int pvt_sensor_s_input(struct file *file)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_device_sensor *ids;
	unsigned int s = 0;
	int ret;

	ids = (struct is_device_sensor *)ivc->device;
	ids->sensor_width = TEST_WIDTH;
	ids->sensor_height = TEST_HEIGHT;

	s |= (1 << INPUT_LEADER_SHIFT) & INPUT_LEADER_MASK;
	s |= (SENSOR_SCENARIO_NORMAL << SENSOR_SCN_SHIFT) & SENSOR_SCN_MASK;
	ret = __pvt_s_input(file, s);
	if (ret)
		return ret;

	complete(&sensor_s_input_wait);

	ret = pvt_wait_for_completion(&ischain_s_input_wait, WAIT_TIMEOUT);
	if (ret)
		return ret;

	return 0;
}

static int pvt_ischain_s_input(struct file *file)
{
	int ret;

	ret = __pvt_s_ctrl(file, V4L2_CID_IS_END_OF_STREAM, 0);
	if (ret)
		return ret;

	ret = pvt_wait_for_completion(&sensor_s_input_wait, WAIT_TIMEOUT);
	if (ret)
		return ret;

	ret = __pvt_s_input(file, 0);
	if (ret)
		return ret;

	complete(&ischain_s_input_wait);

	return 0;
}

static int pvt_s_input(struct file *file)
{
	struct is_video *iv = video_drvdata(file);

	if (iv->device_type == IS_DEVICE_SENSOR)
		return pvt_sensor_s_input(file);
	else
		return pvt_ischain_s_input(file);
}

static int pvt_s_parm(struct file *file)
{
	/* we don't care the result of 's_input' even timeout */
	pvt_s_input(file);

	return __pvt_s_parm(file, 30);
}

static int pvt_g_parm(struct file *file)
{
	pvt_s_parm(file);

	return __pvt_g_parm(file, 30);
}

static int pvt_sensor_s_fmt_mplane(struct file *file)
{
	pvt_s_parm(file);

	return __pvt_s_fmt_mplane(file, V4L2_PIX_FMT_SBGGR10P,
					TEST_WIDTH, TEST_HEIGHT);
}

static int pvt_ischain_s_fmt_mplane(struct file *file)
{
	pvt_s_parm(file);

	return __pvt_s_fmt_mplane(file, V4L2_PIX_FMT_YUYV,
					TEST_WIDTH, TEST_HEIGHT);
}

static int pvt_s_fmt_mplane(struct file *file)
{
	struct is_video *iv = video_drvdata(file);

	if (iv->device_type == IS_DEVICE_SENSOR)
		return pvt_sensor_s_fmt_mplane(file);
	else
		return pvt_ischain_s_fmt_mplane(file);
}

static int pvt_reqbufs(struct file *file)
{
	pvt_s_fmt_mplane(file);

	return __pvt_reqbufs(file, 8);
}

static int pvt_sensor_streamon(struct file *file)
{
	int ret;

	pvt_reqbufs(file);

	ret = pvt_wait_for_completion(&ischain_streamon_wait, WAIT_TIMEOUT);
	if (ret)
		return ret;

	return __pvt_streamon(file);
}

static int pvt_ischain_streamon(struct file *file)
{
	int ret;

	pvt_reqbufs(file);

	ret = __pvt_streamon(file);
	if (ret)
		return ret;

	complete(&ischain_streamon_wait);

	msleep_interruptible(WAIT_TIMEOUT);

	return 0;
}

static int pvt_streamon(struct file *file)
{
	struct is_video *iv = video_drvdata(file);

	if (iv->device_type == IS_DEVICE_SENSOR)
		return pvt_sensor_streamon(file);
	else
		return pvt_ischain_streamon(file);
}

static int pvt_streamoff(struct file *file)
{
	pvt_streamon(file);

	return __pvt_streamoff(file);
}

static int pvt_qbuf(struct file *file)
{
	pvt_streamon(file);

	return __pvt_qbuf(file);
}

static int pvt_dqbuf(struct file *file)
{
	pvt_qbuf(file);

	return __pvt_dqbuf(file);
}

/* Single API */
static int pvt_s_input_only(struct file *file)
{
	return is_vidioc_s_input(file, NULL, 0);
}

static int pvt_s_fmt_mplane_only(struct file *file)
{
	struct v4l2_format f;

	return is_vidioc_s_fmt_mplane(file, NULL, &f);
}

static int pvt_reqbuf_only(struct file *file)
{
	struct v4l2_requestbuffers b;

	return is_vidioc_reqbufs(file, NULL, &b);
}

static int pvt_qbuf_only(struct file *file)
{
	struct v4l2_buffer b;

	return is_vidioc_qbuf(file, NULL, &b);
}

static int pvt_dqbuf_only(struct file *file)
{
	struct v4l2_buffer b;

	return is_vidioc_dqbuf(file, NULL, &b);
}

static int pvt_streamon_only(struct file *file)
{
	return is_vidioc_streamon(file, NULL, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
}

static int pvt_streamoff_only(struct file *file)
{
	return is_vidioc_streamoff(file, NULL, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
}

static int pvt_sensor_s_ctrl(struct file *file)
{
	__pvt_s_ctrl(file, V4L2_CID_IS_S_STREAM, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_CHANGE_CHAIN_PATH, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_CHECK_CHAIN_STATE, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_MIN_TARGET_FPS, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_MAX_TARGET_FPS, 0);
	__pvt_s_ctrl(file, V4L2_CID_SCENEMODE, 0);
	__pvt_s_ctrl(file, V4L2_CID_SENSOR_SET_FRAME_RATE, 0);
	__pvt_s_ctrl(file, V4L2_CID_SENSOR_SET_AE_TARGET, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_S_SENSOR_SIZE, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_SECURE_MODE, 0);
	__pvt_s_ctrl(file, V4L2_CID_SENSOR_SET_GAIN, 0);
	__pvt_s_ctrl(file, V4L2_CID_SENSOR_SET_SHUTTER, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_S_BNS, 0);
	__pvt_s_ctrl(file, V4L2_CID_SENSOR_SET_EXTENDED_MODE, 0);
	__pvt_s_ctrl(file, V4L2_CID_SENSOR_SET_EXTENDED_MODE_EXTRA, 0);
	__pvt_s_ctrl(file, V4L2_CID_SENSOR_SET_EXTENDED_MODE_FORMAT, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_S_STANDBY_OFF, 0);
	__pvt_s_ctrl(file, V4L2_CID_SENSOR_SET_AE_TARGET, 0);

	return 0;
}

static int pvt_ischain_s_ctrl(struct file *file)
{
	__pvt_s_ctrl(file, V4L2_CID_HFLIP, 0);
	__pvt_s_ctrl(file, V4L2_CID_VFLIP, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_DVFS_LOCK, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_DVFS_UNLOCK, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_DVFS_CLUSTER0, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_DVFS_CLUSTER1, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_DVFS_CLUSTER1_HF, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_DVFS_CLUSTER2, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_FORCE_DONE, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_SET_SETFILE, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_END_OF_STREAM, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_S_DVFS_SCENARIO, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_S_DVFS_RECORDING_SIZE, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_OPENING_HINT, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_CLOSING_HINT, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_ICPU_PRELOAD, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_HF_MAX_SIZE, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_YUV_MAX_SIZE, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_DEBUG_DUMP, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_DEBUG_SYNC_LOG, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_S_LLC_CONFIG, 0);
 	__pvt_s_ctrl(file, V4L2_CID_IS_S_IRTA_RESULT_BUF, 0);
 	__pvt_s_ctrl(file, V4L2_CID_IS_S_IRTA_RESULT_IDX, 0);
 	__pvt_s_ctrl(file, V4L2_CID_IS_S_CRTA_START, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_FAST_CTL_LENS_POS, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_NOTIFY_HAL_INIT, 0);
	__pvt_s_ctrl(file, V4L2_CID_IS_INTENT, 0);

	return 0;
}

static int pvt_s_ctrl_only(struct file *file)
{
	struct is_video *iv = video_drvdata(file);

	if (iv->device_type == IS_DEVICE_SENSOR)
		return pvt_sensor_s_ctrl(file);
	else
		return pvt_ischain_s_ctrl(file);
}

static struct pablo_v4l2_test_case pablo_v4l2_test_cases[] = {
	PABLO_V4L2_TEST_CASE("querycap", pvt_querycap),
	PABLO_V4L2_TEST_CASE("s_input", pvt_s_input),
	PABLO_V4L2_TEST_CASE("s_parm", pvt_s_parm),
	PABLO_V4L2_TEST_CASE("g_parm", pvt_g_parm),
	PABLO_V4L2_TEST_CASE("s_fmt_mplane", pvt_s_fmt_mplane),
	PABLO_V4L2_TEST_CASE("reqbufs", pvt_reqbufs),
	PABLO_V4L2_TEST_CASE("streamon", pvt_streamon),
	PABLO_V4L2_TEST_CASE("streamoff", pvt_streamoff),
	PABLO_V4L2_TEST_CASE("qbuf", pvt_qbuf),
	PABLO_V4L2_TEST_CASE("dqbuf", pvt_dqbuf),

	/* Single API Test */
	PABLO_V4L2_TEST_CASE("s_input_only", pvt_s_input_only),
	PABLO_V4L2_TEST_CASE("s_fmt_mplane_only", pvt_s_fmt_mplane_only),
	PABLO_V4L2_TEST_CASE("reqbuf_only", pvt_reqbuf_only),
	PABLO_V4L2_TEST_CASE("qbuf_only", pvt_qbuf_only),
	PABLO_V4L2_TEST_CASE("dqbuf_only", pvt_dqbuf_only),
	PABLO_V4L2_TEST_CASE("streamon_only", pvt_streamon_only),
	PABLO_V4L2_TEST_CASE("streamoff_only", pvt_streamoff_only),
	PABLO_V4L2_TEST_CASE("s_ctrl_only", pvt_s_ctrl_only),
};

int pablo_v4l2_test_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pablo_v4l2_test_cases); i++) {
		pablo_v4l2_test_cases[i].result = kvzalloc(MAX_RESULT, GFP_KERNEL);
		if (ZERO_OR_NULL_PTR(pablo_v4l2_test_cases[i].result)) {
			while (i-- > 0)
				kvfree(pablo_v4l2_test_cases[i].result);

			return -ENOMEM;
		}
	}

	return 0;
}

void pablo_v4l2_test_pre(struct file *file)
{
	struct is_video *iv = video_drvdata(file);
	struct is_mem *mem = iv->mem;
	struct is_core *core;

	if (strncmp(current->comm, "sh", TASK_COMM_LEN))
		return;

	mutex_lock(&pre_post_mutex);
	if (refcount_read(&pia_ref) == 0) {
		refcount_set(&pia_ref, 1);
		pia_orig = pablo_get_icpu_adt();

		init_completion(&sensor_s_input_wait);
		init_completion(&ischain_s_input_wait);
		init_completion(&ischain_streamon_wait);

		/* FIXME: sensor video doesn't have valid mem yet */
		if (!mem) {
			core = is_get_is_core();
			mem = &core->resourcemgr.mem;
		}

		pb = CALL_PTR_MEMOP(mem, alloc, mem->priv,
				TEST_WIDTH * TEST_HEIGHT * 2, NULL, 0);
		if (IS_ERR(pb))
			pb = NULL;
	} else {
		refcount_inc(&pia_ref);
	}
	mutex_unlock(&pre_post_mutex);
}

void pablo_v4l2_test_post(struct file *file)
{
	struct is_core *core = is_get_is_core();
	struct is_hardware *hw = &core->hardware;

	if (strncmp(current->comm, "sh", TASK_COMM_LEN))
		return;

	mutex_lock(&pre_post_mutex);
	if (refcount_dec_and_test(&pia_ref)) {
		pablo_set_icpu_adt(pia_orig);
		hw->icpu_adt = pia_orig;

		if (pb) {
			CALL_VOID_BUFOP(pb, free, pb);
			pb = NULL;
		}
	}
	mutex_unlock(&pre_post_mutex);
}

int pablo_v4l2_test_get_result(struct file *file, __user char *buf, size_t count)
{
	struct is_video *iv = video_drvdata(file);
	int ret;

	mutex_lock(&iv->lock);
	if (!latest_case)
		goto exit;

	if (!latest_case->result)
		goto exit;

	if (IS_ENABLED(USE_RESULT_VID) && latest_case->id != iv->id)
		goto exit;

	ret = copy_to_user(buf, latest_case->result, MAX_RESULT);
	if (ret) {
		pr_err("failed to copy test result: %d", ret);
		goto exit;
	}

	kvfree(latest_case->result);
	latest_case->result = NULL;
	latest_case->id = -ENOENT;
	latest_case = NULL;
	mutex_unlock(&iv->lock);

	return MAX_RESULT;

exit:
	mutex_unlock(&iv->lock);
	return 0;
}

int pablo_v4l2_test_run_test(struct file *file, const char __user *buf, size_t count)
{
	struct is_core *core = is_get_is_core();
	struct is_hardware *hw = &core->hardware;
	struct is_video *iv = video_drvdata(file);
	char *arg;
	int i, ret;

	arg = kvzalloc(count, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(arg)) {
		pr_err("failed to alloc arg buffer\n");
		return -ENOMEM;
	}

	ret = copy_from_user(arg, buf, count);
	if (ret) {
		pr_err("failed to copy test argument: %d", ret);
		goto exit;
	}

	mutex_lock(&pre_post_mutex);
	if (refcount_read(&pia_ref) > 0) {
		pablo_set_icpu_adt(&pia_fake);
		hw->icpu_adt = &pia_fake;
	}
	mutex_unlock(&pre_post_mutex);

	for (i = 0; i < ARRAY_SIZE(pablo_v4l2_test_cases); i++) {
		if (!strncmp(arg, pablo_v4l2_test_cases[i].name, count - 1)) {
			pr_info("run ""%s"" test case", pablo_v4l2_test_cases[i].name);

			ret = pablo_v4l2_test_cases[i].run(file);

			mutex_lock(&iv->lock);
			if (!pablo_v4l2_test_cases[i].result) {
				pablo_v4l2_test_cases[i].result =
					kvzalloc(MAX_RESULT, GFP_KERNEL);
				if (ZERO_OR_NULL_PTR(pablo_v4l2_test_cases[i].result)) {
					pr_err("failed to alloc result buffer\n");
					mutex_unlock(&iv->lock);
					goto exit;
				}
			}

			if (IS_ENABLED(USE_RESULT_VID))
				pablo_v4l2_test_cases[i].id = iv->id;

			snprintf(pablo_v4l2_test_cases[i].result,
					MAX_RESULT, "%s %d %s",
					ret ? "not ok" : "ok" , i + 1,
					pablo_v4l2_test_cases[i].name);

			latest_case = &pablo_v4l2_test_cases[i];
			mutex_unlock(&iv->lock);

			goto exit;
		}
	}

exit:
	kvfree(arg);

	return count;
}
