// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <videodev2_exynos_camera.h>

#include "pablo-self-test-result.h"
#include "is-core.h"
#include "pablo-framemgr.h"
#include "is-resourcemgr.h"
#include "is-device-sensor.h"
#include "is-video.h"
#include "is-hw.h"
#include "pablo-phy-tune.h"

/*
 * @sensor self test
 */
static int pablo_test_get_sensor_run(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_sensor_run = {
	.set = pablo_test_set_sensor_run,
	.get = pablo_test_get_sensor_run,
};

module_param_cb(test_sensor_run, &pablo_param_ops_sensor_run, NULL, 0644);

enum pablo_type_sensor_run {
	PABLO_SENSOR_RUN_STOP = 0,
	PABLO_SENSOR_RUN_START,
};

struct pablo_info_sensor_run {
	u32 position;
	u32 width;
	u32 height;
	u32 fps;
	u32 ex_mode;
	u32 ex_mode_format;
};

static struct pablo_info_sensor_run sr_info;

static u32 sr_act;

static struct is_fmt fmt = {
	.name		= "BAYER 12bit packed single plane",
	.pixelformat	= V4L2_PIX_FMT_SRGB36P_SP,
	.num_planes	= 1 + NUM_OF_META_PLANE,
	.bitsperpixel	= { 12 },
	.hw_format	= DMA_INOUT_FORMAT_RGB,
	.hw_order	= DMA_INOUT_ORDER_NO,
	.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_12BIT,
	.hw_plane	= DMA_INOUT_PLANE_3,
};

struct is_device_sensor *sensor;
static int sensor_self_stop(void)
{
	int ret;
	struct is_device_csi *csi;
	struct is_queue *queue;

	if (!sensor) {
		err("sensor is NULL");
		return -ENODEV;
	}
	queue = sensor->group_sensor.leader.vctx->queue;

	csi = v4l2_get_subdevdata(sensor->subdev_csi);
	pst_result_csi_irq(csi);

	ret = is_sensor_close(sensor);
	if (ret) {
		err("fail is_sensor_close(%d)", ret);
		return ret;
	}

	/* HACK */
	ret = phy_power_off(csi->phy);
	if (ret)
		err("failed to csi%d power off", csi->otf_info.csi_ch);

	vfree(queue);

	sensor = NULL;

	return 0;
}

static int sensor_self_start(void)
{
	int ret;
	u32 position = sr_info.position + SP_REAR;
	u32 i, device_id, rsc_type, video_id;
	u32 scenario = SENSOR_SCENARIO_VISION;
	u32 stream_ldr = 1;
	static struct is_video_ctx vctx;
	struct v4l2_streamparm param;
	struct is_core *core;
	struct is_queue *queue;
	struct is_video *video;
	struct is_module_enum *module;
	struct is_device_csi *csi;

	core = is_get_is_core();

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		ret = is_search_sensor_module_with_position(&core->sensor[i],
							position, &module);
		if (!ret) {
			device_id = i;
			info("%s: pos(%d) dev(%d)", __func__, position, device_id);
			break;
		}
	}

	if (ret) {
		err("position(%d) is not supported.", position);
		return -ENODEV;
	}

	rsc_type = device_id + RESOURCE_TYPE_SENSOR0;
	video_id = device_id + IS_VIDEO_SS0_NUM;

	ret = is_resource_open(&core->resourcemgr, rsc_type, (void **)&sensor);
	if (ret) {
		err("fail is_resource_open(%d)", ret);
		return ret;
	}

	video = &sensor->video;
	vctx.instance = 0;
	vctx.device = &sensor;
	vctx.video = video;
	vctx.queue = vzalloc(sizeof(struct is_queue));
	if (!vctx.queue) {
		err("is_queue alloc fail");
		return -ENOMEM;
	}

	queue = vctx.queue;
	snprintf(queue->name, sizeof(queue->name), "%s", vn_name[video_id]);
	frame_manager_probe(&queue->framemgr, queue->name);

	ret = is_sensor_open(sensor, &vctx);
	if (ret) {
		err("fail is_sensor_open(%d)", ret);
		goto p_err;
	}

	ret = is_sensor_s_input(sensor, position, scenario, stream_ldr);
	if (ret) {
		err("fail is_sensor_s_input(%d)", ret);
		goto p_err;
	}

	param.parm.capture.timeperframe.denominator = sr_info.fps;
	param.parm.capture.timeperframe.numerator = 1;
	param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	ret = is_sensor_s_framerate(sensor, &param);
	if (ret) {
		err("fail is_sensor_s_framerate(%d)", ret);
		goto p_err;
	}

	queue->qops = is_get_sensor_device_qops();
	queue->framecfg.format = &fmt;
	sensor->sensor_width = sr_info.width;
	sensor->sensor_height = sr_info.height;
	sensor->ex_mode = sr_info.ex_mode;
	sensor->ex_mode_format = sr_info.ex_mode_format;

	ret = CALL_QOPS(queue, s_fmt, sensor, queue);
	if (ret) {
		err("fail sensor s_fmt(%d)", ret);
		goto p_err;
	}

	ret = CALL_QOPS(queue, start_streaming, sensor, queue);
	if (ret) {
		err("fail sensor start_streaming(%d)", ret);
		goto p_err;
	}

	ret = is_sensor_front_start(sensor, 0, 0);
	if (ret) {
		err("fail is_sensor_front_start(%d)", ret);
		goto p_err;
	}

	/* HACK */
	csi = v4l2_get_subdevdata(sensor->subdev_csi);
	ret = phy_power_on(csi->phy);
	if (ret) {
		err("failed to csi%d power on", csi->otf_info.csi_ch);
		goto p_err;
	}

	return ret;

p_err:
	vfree(vctx.queue);
	vctx.queue = NULL;

	return ret;
}

int pablo_test_set_sensor_run(const char *val, const struct kernel_param *kp)
{
	int ret, argc;
	char **argv;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("No argument!");
		return -EINVAL;
	}

	ret = kstrtouint(argv[0], 0, &sr_act);
	if (ret) {
		err("Invalid act %d ret %d", sr_act, ret);
		goto func_exit;
	}

	switch (sr_act) {
	case PABLO_SENSOR_RUN_STOP:
		if (argc < 1) {
			err("Not enough parameters. %d < 1", argc);
			goto func_exit;
		}

		sensor_self_stop();

		info("stop sensor\n");
		break;
	case PABLO_SENSOR_RUN_START:
		if (argc < 5) {
			err("Not enough parameters. %d < 5", argc);
			err("ex) echo 1 0 4032 3024 30");
			goto func_exit;
		}

		memset(&sr_info, 0x00, sizeof(sr_info));
		ret = kstrtouint(argv[1], 0, &sr_info.position);
		ret |= kstrtouint(argv[2], 0, &sr_info.width);
		ret |= kstrtouint(argv[3], 0, &sr_info.height);
		ret |= kstrtouint(argv[4], 0, &sr_info.fps);
		if (argc > 5)
			ret |= kstrtouint(argv[5], 0, &sr_info.ex_mode);
		if (argc > 6)
			ret |= kstrtouint(argv[6], 0, &sr_info.ex_mode_format);

		if (ret) {
			err("Invalid parameters(ret %d)", ret);
			goto func_exit;
		}

		info("start sensor(position %d: %d x %d @ %d fps, ex(%d) ex_format(%d))\n",
			sr_info.position, sr_info.width, sr_info.height,
			sr_info.fps, sr_info.ex_mode, sr_info.ex_mode_format);

		fst_phy_check_step = true;
		sensor_self_start();
		break;
	default:
		err("NOT supported act %u", sr_act);
		goto func_exit;
	}

func_exit:
	argv_free(argv);
	return ret;
}

static int pablo_test_get_sensor_run(char *buffer, const struct kernel_param *kp)
{
	int ret;

	ret = sprintf(buffer, "> Set\n");
	ret += sprintf(buffer + ret, "# echo <act> <position> <width> <height> <fps> > sensor_run\n");
	ret += sprintf(buffer + ret, " - act: 0=PABLO_SENSOR_RUN_STOP, 1=PABLO_SENSOR_RUN_START\n");
	ret += sprintf(buffer + ret, " - position: 0=REAR 1=FRONT 2=REAR2 3=FRONT2 ...\n");
	ret += sprintf(buffer + ret, "act %d position %d width %d height %d fps %d ex %d\n",
			sr_act,
			sr_info.position,
			sr_info.width,
			sr_info.height,
			sr_info.fps,
			sr_info.ex_mode);

	return ret;
}
