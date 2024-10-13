// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"

#include "is-core.h"
#include "videodev2_exynos_camera.h"
#include "is-interface-ddk.h"
#include "exynos-is-module.h"
#include "is-device-sensor-peri.h"
#include "is-vendor-private.h"

/* Define the test cases. */
enum pablo_type_sensor_run {
	PABLO_SENSOR_RUN_STOP = 0,
	PABLO_SENSOR_RUN_START,
};

struct pablo_info_sensor_run {
	u32 act;
	u32 position;
	u32 width;
	u32 height;
	u32 fps;
};

static struct pablo_kunit_device_sensor_func *fp;

static struct test_ctx {
	struct is_device_sensor ids;
	struct is_device_ischain idi;
	struct v4l2_ext_controls ctrls;
	struct v4l2_control ctrl;
	struct v4l2_subdev subdev_module;
	struct is_module_enum module;
	struct exynos_platform_is_module pdata;
	struct is_device_sensor_peri sensor_peri;
	struct platform_device pdev;
	struct exynos_platform_is_sensor psensor;
	struct is_sensor_cfg cfg;
	struct is_core core_private_data;
	struct is_group group;
	struct is_actuator actuator;
	struct is_flash flash;
	struct is_ois ois;
	struct is_mcu mcu;
	struct is_cis cis;
	struct is_aperture aperture;
	struct is_eeprom eeprom;
	struct is_laser_af laser_af;
	struct is_video_ctx vctx;
	struct is_video video_node;
	struct v4l2_subdev subdev_csi;
	struct v4l2_subdev subdev_cis;
	struct is_device_csi csi;
	struct is_resourcemgr resourcemgr;
	struct is_framemgr framemgr;
	struct is_subdev subdev;
 	struct is_queue queue;
	struct camera2_stream stream;
	struct v4l2_streamparm param;
} test_ctx;

static void pablo_device_sensor_v2_clk_init(void)
{
	struct is_device_sensor *ids;
	struct is_module_enum *module;

	ids = &test_ctx.ids;
	ids->pdata = &test_ctx.psensor;
	ids->private_data = &test_ctx.core_private_data;
	ids->pdev = &test_ctx.pdev;
	module = &test_ctx.module;
	module->pdata = &test_ctx.pdata;
	ids->subdev_module = &test_ctx.subdev_module;
	v4l2_set_subdevdata(ids->subdev_module, module); /* connect subdev_module and is_module_enum */
	v4l2_set_subdev_hostdata(ids->subdev_module, ids); /* connect subdev_module and is_device_sensor */
	is_sensor_get_clk_ops(ids->pdata); /* get ops of sensor clk */
}

static int sensor_s_again_mock(struct v4l2_subdev *subdev, u64 exposure)
{
	return 0;
}

static int sensor_s_exposure_mock(struct v4l2_subdev *subdev, u64 exposure)
{
	return 0;
}

static int sensor_s_duration_mock(struct v4l2_subdev *subdev, u64 exposure)
{
	return 0;
}

static int sensor_s_shutterspeed_mock(struct v4l2_subdev *subdev, u64 exposure)
{
	return 0;
}

static struct is_sensor_ops module_ops_mock = {
	.s_again = sensor_s_again_mock,
	.s_exposure = sensor_s_exposure_mock,
	.s_duration = sensor_s_duration_mock,
	.s_shutterspeed = sensor_s_shutterspeed_mock,
};

static void pablo_device_sensor_v2_s_again_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;
	struct is_module_enum *module;
	int ret;
	u32 gain = 0x0;

	ids = &test_ctx.ids;

	/* Sub TC : !subdev_module */
	ids->subdev_module = NULL;
	ret = fp->s_again(ids, gain);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC : !module */
	ids->subdev_module = &test_ctx.subdev_module;
	module = NULL;
	ret = fp->s_again(ids, gain);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC : !gain <=0 */
	module = &test_ctx.module;
	v4l2_set_subdevdata(ids->subdev_module, module);
	ret = fp->s_again(ids, gain);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC : !module->ops */
	gain = 0x100;
	ret = fp->s_again(ids, gain);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* Sub TC : true */
	module->ops = &module_ops_mock;
	ret = fp->s_again(ids, gain);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_device_sensor_v2_get_frame_type_kunit_test(struct kunit *test)
{
	int frame_type;
	int instance = 0;
	struct is_device_ischain *idi;
	struct is_device_sensor *ids;

	frame_type = fp->get_frame_type(instance);
	KUNIT_EXPECT_EQ(test, frame_type, -ENODEV);

	idi = is_get_ischain_device(instance);
	ids = is_get_sensor_device(instance);
	idi->sensor = ids;

	frame_type = fp->get_frame_type(instance);
	KUNIT_EXPECT_EQ(test, frame_type, LIB_FRAME_HDR_SINGLE);

	set_bit(IS_ISCHAIN_MULTI_CH, &idi->state);
	frame_type = fp->get_frame_type(instance);
	KUNIT_EXPECT_EQ(test, frame_type, LIB_FRAME_HDR_SHORT);

	clear_bit(IS_ISCHAIN_MULTI_CH, &idi->state);
	idi->sensor = NULL;
}

static void pablo_device_sensor_v2_g_module_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_sensor *ids;
	struct is_module_enum *module, *g_module;

	/* Sub TC: !device */
	ids = NULL;
	ret = fp->g_module(ids, &g_module);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: !device->subdev_module */
	ids = &test_ctx.ids;
	ret = fp->g_module(ids, &g_module);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: !*module */
	ids->subdev_module = &test_ctx.subdev_module;
	ret = fp->g_module(ids, &g_module);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: !(*module)->pdata */
	module = &test_ctx.module;
	v4l2_set_subdevdata(ids->subdev_module, module);
	ret = fp->g_module(ids, &g_module);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: get module successfully */
	module->pdata = &test_ctx.pdata;
	ret = fp->g_module(ids, &g_module);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, (ulong)g_module, (ulong)module);
}

static void pablo_device_sensor_v2_get_sensor_interface_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;
	struct is_sensor_interface *itf;
	struct is_module_enum *module;

	/* Sub TC: !is_sensor_g_module() */
	ids = &test_ctx.ids;
	itf = fp->get_sensor_interface(ids);
	KUNIT_EXPECT_TRUE(test, itf == NULL);

	/* Sub TC: !sensor_peri */
	ids->subdev_module = &test_ctx.subdev_module;
	module = &test_ctx.module;
	module->pdata = &test_ctx.pdata;
	v4l2_set_subdevdata(ids->subdev_module, module); /* connect subdev_module and is_module_enum */
	itf = fp->get_sensor_interface(ids);
	KUNIT_EXPECT_TRUE(test, itf == NULL);

	/* Sub TC: get itf successfully */
	module->private_data = &test_ctx.sensor_peri;
	itf = fp->get_sensor_interface(ids);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, itf);
}

static void pablo_device_sensor_v2_g_device_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_sensor *ids, *g_device;

	/* Sub TC: !pdev */
	ids = &test_ctx.ids;
	ret = fp->g_device(ids->pdev, &g_device);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: !*device */
	ids->pdev = &test_ctx.pdev;
	ret = fp->g_device(ids->pdev, &g_device);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: !(*device)->pdata */
	platform_set_drvdata(ids->pdev, ids);
	ret = fp->g_device(ids->pdev, &g_device);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: get device successfully */
	ids->pdata = &test_ctx.psensor;
	ret = fp->g_device(ids->pdev, &g_device);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, (ulong)ids, (ulong)g_device);
}

static void pablo_device_sensor_v2_cmp_mode_kunit_test(struct kunit *test)
{
	bool ret;
	struct is_sensor_cfg *cfg_table;
	struct is_sensor_mode mode;

	cfg_table = &test_ctx.cfg;

	/* Sub TC: true */
	mode.width = cfg_table->width;
	mode.height = cfg_table->height;
	mode.ex_mode = cfg_table->ex_mode;
	mode.ex_mode_extra = cfg_table->ex_mode_extra;
	mode.hw_format = cfg_table->input[CSI_VIRTUAL_CH_0].hwformat;
	ret = fp->cmp_mode(cfg_table, &mode);
	KUNIT_EXPECT_TRUE(test, ret);

	/* Sub TC: false (width) */
	mode.width = cfg_table->width + 128;
	ret = fp->cmp_mode(cfg_table, &mode);
	KUNIT_EXPECT_FALSE(test, ret);

	/* Sub TC: false (height) */
	mode.width = cfg_table->width;
	mode.height = cfg_table->height + 128;
	ret = fp->cmp_mode(cfg_table, &mode);
	KUNIT_EXPECT_FALSE(test, ret);

	/* Sub TC: false (ex_mode_extra) */
	mode.height = cfg_table->height;
	mode.ex_mode_extra = cfg_table->ex_mode_extra + 128;
	ret = fp->cmp_mode(cfg_table, &mode);
	KUNIT_EXPECT_FALSE(test, ret);

	/* Sub TC: false (ex_mode) */
	mode.ex_mode_extra = cfg_table->ex_mode_extra;
	mode.ex_mode = cfg_table->ex_mode + 128;

	ret = fp->cmp_mode(cfg_table, &mode);
	KUNIT_EXPECT_FALSE(test, ret);

	/* Sub TC: false (hw_format) */
	mode.ex_mode = cfg_table->ex_mode;
	mode.hw_format = HW_FORMAT_RAW10;
	cfg_table->input[CSI_VIRTUAL_CH_0].hwformat = HW_FORMAT_RAW12;
	ret = fp->cmp_mode(cfg_table, &mode);
	KUNIT_EXPECT_FALSE(test, ret);
}

static void pablo_device_sensor_v2_g_mode_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;
	struct is_sensor_cfg *select;
	struct is_module_enum *module;
	struct is_sensor_cfg *cfg_table;

	ids = &test_ctx.ids;
	module = &test_ctx.module;
	module->cfg = &test_ctx.cfg;
	module->cfgs = 2;
	module->private_data = &test_ctx.sensor_peri;
	ids->subdev_module = &test_ctx.subdev_module;
	v4l2_set_subdevdata(ids->subdev_module, module); /* connect subdev_module and is_module_enum */
	v4l2_set_subdev_hostdata(ids->subdev_module, ids); /* connect subdev_module and is_device_sensor */
	cfg_table = module->cfg;

	ids->image.window.width = cfg_table->width;
	ids->image.window.height = cfg_table->height;
	ids->image.framerate = cfg_table->framerate;
	ids->ex_mode = cfg_table->ex_mode;
	ids->ex_mode_extra = cfg_table->ex_mode_extra;
	cfg_table->input[CSI_VIRTUAL_CH_0].hwformat = HW_FORMAT_RAW10;

	/* Sub TC: get mode successfully */
	select = fp->g_mode(ids);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, select);

	/* Sub TC: get mode failed */
	ids->ex_mode_format = EX_FORMAT_12BIT;
	select = fp->g_mode(ids);
	KUNIT_EXPECT_TRUE(test, select == NULL);
}

static void pablo_device_sensor_v2_mclk_on_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_sensor *ids;
	struct is_module_enum *module;

	/* Sub TC: !device */
	ids = NULL;
	ret = fp->mclk_on(ids, 0, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: !device->pdev */
	ids = &test_ctx.ids;
	ret = fp->mclk_on(ids, 0, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: !device->pdata */
	ids->pdev = &test_ctx.pdev;
	ret = fp->mclk_on(ids, 0, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: SENSOR_SCENARIO_STANDBY */
	ids->pdata = &test_ctx.psensor;
	ret = fp->mclk_on(ids, SENSOR_SCENARIO_STANDBY, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_FALSE(test, test_bit(IS_SENSOR_MCLK_ON, &ids->state));

	/* Sub TC: already clk on */
	set_bit(IS_SENSOR_MCLK_ON, &ids->state);
	ret = fp->mclk_on(ids, 0, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);
	clear_bit(IS_SENSOR_MCLK_ON, &ids->state);

	/* Sub TC: !pdata->mclk_on */
	ids->pdata->mclk_on = NULL;
	ret = fp->mclk_on(ids, 0, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_FALSE(test, test_bit(IS_SENSOR_MCLK_ON, &ids->state));

	/* Sub TC: mclk_on successfully */
	module = &test_ctx.module;
	pablo_device_sensor_v2_clk_init();
	ret = fp->mclk_on(ids, SENSOR_SCENARIO_NORMAL, module->pdata->mclk_ch,
			  module->pdata->mclk_freq_khz);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(IS_SENSOR_MCLK_ON, &ids->state));
}

static void pablo_device_sensor_v2_mclk_off_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_sensor *ids;
	struct is_module_enum *module;

	/* Sub TC: !device */
	ids = NULL;
	ret = fp->mclk_off(ids, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: !device->pdev */
	ids = &test_ctx.ids;
	ids->pdev = NULL;
	ret = fp->mclk_off(ids, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: !device->pdata */
	ids->pdev = &test_ctx.pdev;
	ids->pdata = NULL;
	ret = fp->mclk_off(ids, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: already clk off */
	ids->pdata = &test_ctx.psensor;
	clear_bit(IS_SENSOR_MCLK_ON, &ids->state);
	ret = fp->mclk_off(ids, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);
	set_bit(IS_SENSOR_MCLK_ON, &ids->state);

	/* Sub TC: !pdata->mclk_off */
	ids->pdata->mclk_off = NULL;
	ret = fp->mclk_off(ids, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_TRUE(test, test_bit(IS_SENSOR_MCLK_ON, &ids->state));

	/* Sub TC: mclk_on successfully */
	module = &test_ctx.module;
	pablo_device_sensor_v2_clk_init();
	ret = fp->mclk_off(ids, SENSOR_SCENARIO_NORMAL, module->pdata->mclk_ch);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_FALSE(test, test_bit(IS_SENSOR_MCLK_ON, &ids->state));
}

static void pablo_device_sensor_v2_iclk_on_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_sensor *ids;

	/* Sub TC: !device */
	ids = NULL;
	ret = fp->iclk_on(ids);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: !device->pdev */
	ids = &test_ctx.ids;
	ids->pdev = NULL;
	ret = fp->iclk_on(ids);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: !device->pdata */
	ids->pdev = &test_ctx.pdev;
	ids->pdata = NULL;
	ret = fp->iclk_on(ids);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: !device->private_data */
	ids->pdata = &test_ctx.psensor;
	ids->private_data = NULL;
	ret = fp->iclk_on(ids);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: already clk on */
	ids->private_data = &test_ctx.core_private_data;
	set_bit(IS_SENSOR_ICLK_ON, &ids->state);
	ret = fp->iclk_on(ids);
	KUNIT_EXPECT_EQ(test, ret, 0);
	clear_bit(IS_SENSOR_ICLK_ON, &ids->state);

	/* Sub TC: !pdata->iclk_cfg */
	ids->pdata->iclk_cfg = NULL;
	ret = fp->iclk_on(ids);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_FALSE(test, test_bit(IS_SENSOR_ICLK_ON, &ids->state));

	/* Sub TC: !pdata->iclk_on */
	is_sensor_get_clk_ops(ids->pdata);
	ids->pdata->iclk_on = NULL;
	ret = fp->iclk_on(ids);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_FALSE(test, test_bit(IS_SENSOR_ICLK_ON, &ids->state));

	/* Sub TC: iclk_on successfully */
	ids->pdata->scenario = SENSOR_SCENARIO_NORMAL;
	pablo_device_sensor_v2_clk_init();
	ret = fp->iclk_on(ids);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(IS_SENSOR_ICLK_ON, &ids->state));
}

static void pablo_device_sensor_v2_iclk_off_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_sensor *ids;

	/* Sub TC: !device */
	ids = NULL;
	ret = fp->iclk_off(ids);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: !device->pdev */
	ids = &test_ctx.ids;
	ids->pdev = NULL;
	ret = fp->iclk_off(ids);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: !device->pdata */
	ids->pdev = &test_ctx.pdev;
	ids->pdata = NULL;
	ret = fp->iclk_off(ids);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: !device->private_data */
	ids->pdata = &test_ctx.psensor;
	ids->private_data = NULL;
	ret = fp->iclk_off(ids);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: already clk off */
	ids->private_data = &test_ctx.core_private_data;
	clear_bit(IS_SENSOR_ICLK_ON, &ids->state);
	ret = fp->iclk_off(ids);
	KUNIT_EXPECT_EQ(test, ret, 0);
	set_bit(IS_SENSOR_ICLK_ON, &ids->state);

	/* Sub TC: !pdata->iclk_off */
	ids->pdata->iclk_off = NULL;
	ret = fp->iclk_off(ids);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_TRUE(test, test_bit(IS_SENSOR_ICLK_ON, &ids->state));

	/* Sub TC: iclk_off successfully */
	ids->pdata->scenario = SENSOR_SCENARIO_NORMAL;
	pablo_device_sensor_v2_clk_init();
	ret = fp->iclk_off(ids);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_FALSE(test, test_bit(IS_SENSOR_ICLK_ON, &ids->state));
}

static void pablo_device_sensor_v2_gpio_on_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_sensor *ids;

	ids = &test_ctx.ids;
	ids->pdata = &test_ctx.psensor;
	clear_bit(IS_SENSOR_GPIO_ON, &ids->state);

	/* Sub TC: mclk_on successfully (SENSOR_SCENARIO_STANDBY) */
	ids->pdata->scenario = SENSOR_SCENARIO_STANDBY;
	ret = fp->gpio_on(ids);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(IS_SENSOR_GPIO_ON, &ids->state));
}

static void pablo_device_sensor_v2_gpio_off_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_sensor *ids;
	struct is_module_enum *module;

	ids = &test_ctx.ids;
	ids->pdata = &test_ctx.psensor;
	ids->pdev = &test_ctx.pdev;
	ids->private_data = &test_ctx.core_private_data;
	ids->subdev_module = &test_ctx.subdev_module;
	module = &test_ctx.module;
	module->pdata = &test_ctx.pdata;

	v4l2_set_subdevdata(ids->subdev_module, module); /* connect subdev_module and is_module_enum */
	v4l2_set_subdev_hostdata(ids->subdev_module, ids); /* connect subdev_module and is_device_sensor */

	/* Sub TC: gpio off successfully (!IS_MODULE_GPIO_ON) */
	clear_bit(IS_MODULE_GPIO_ON, &module->state);
	ret = fp->gpio_off(ids);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_FALSE(test, test_bit(IS_SENSOR_GPIO_ON, &ids->state));
}

static void pablo_device_sensor_v2_gpio_dbg_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_sensor *ids;
	struct is_module_enum *module;

	ids = &test_ctx.ids;
	ids->pdev = &test_ctx.pdev;
	ids->pdata = &test_ctx.psensor;
	ids->subdev_module = &test_ctx.subdev_module;
	module = &test_ctx.module;
	module->pdata = &test_ctx.pdata;
	v4l2_set_subdevdata(ids->subdev_module, module); /* connect subdev_module and is_module_enum */
	v4l2_set_subdev_hostdata(ids->subdev_module, ids); /* connect subdev_module and is_device_sensor */

	is_get_gpio_ops(((struct is_module_enum *)ids->subdev_module->dev_priv)->pdata); /* get gpio ops */
	ret = fp->gpio_dbg(ids);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_device_sensor_v2_g_max_size_kunit_test(struct kunit *test)
{
	u32 max_width = 0;
	u32 max_height = 0;
	u32 pixel_width, pixel_height;
	struct is_device_sensor *ids;
	struct is_core *core = is_get_is_core();

	ids = &core->sensor[0];
	pixel_width = ids->module_enum[0].pixel_width;
	pixel_height = ids->module_enum[0].pixel_height;

	ids->module_enum[0].pixel_width = 0xFFFF;
	ids->module_enum[0].pixel_height = 0xFFFF;
	fp->g_max_size(&max_width, &max_height);
	KUNIT_EXPECT_TRUE(test, (max_width == 0xFFFF && max_height == 0xFFFF));

	/* restore */
	ids->module_enum[0].pixel_width = pixel_width;
	ids->module_enum[0].pixel_height = pixel_height;
}

static void pablo_device_sensor_v2_g_aeb_mode_kunit_test(struct kunit *test)
{
	bool ret;
	struct is_device_sensor *ids;

	ids = &test_ctx.ids;
	ids->cfg = &test_ctx.cfg;

	/* Sub TC: true */
	ids->group_sensor.pnext = &test_ctx.group;
	ids->cfg->ex_mode = EX_AEB;
	ret = fp->g_aeb_mode(ids);
	KUNIT_EXPECT_TRUE(test, ret);

	/* Sub TC: false (!pnext) */
	ids->group_sensor.pnext = NULL;
	ret = fp->g_aeb_mode(ids);
	KUNIT_EXPECT_FALSE(test, ret);

	/* Sub TC: false (ex_mode) */
	ids->group_sensor.pnext = &test_ctx.group;
	ids->cfg->ex_mode = EX_NONE;
	ret = fp->g_aeb_mode(ids);
	KUNIT_EXPECT_FALSE(test, ret);
}

static void pablo_device_sensor_v2_get_mono_mode_kunit_test(struct kunit *test)
{
	int ret;
	struct is_module_enum *module;
	struct is_sensor_interface *sensor_itf;
	struct is_device_sensor_peri *sensor_peri;

	/* set struct is_module_enum module */
	module = &test_ctx.module;
	module->pdata = &test_ctx.pdata;
	module->cfg = &test_ctx.cfg;

	/* set sensor interface */
	sensor_itf = &test_ctx.sensor_peri.sensor_interface;
	sensor_itf->magic = SENSOR_INTERFACE_MAGIC;

	/* set sensor peri */
	sensor_peri = &test_ctx.sensor_peri;
	sensor_peri->module = module;
	module->private_data = sensor_peri;

	/* Sub TC: SENSOR_TYPE_MONO */
	module->pdata->sensor_module_type = SENSOR_TYPE_MONO;
	ret = fp->get_mono_mode(sensor_itf);
	KUNIT_EXPECT_EQ(test, ret, 1);

	/* Sub TC: SENSOR_TYPE_RGB */
	module->pdata->sensor_module_type = SENSOR_TYPE_RGB;
	ret = fp->get_mono_mode(sensor_itf);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_device_sensor_v2_g_ex_mode_kunit_test(struct kunit *test)
{
	int g_ex_mode = EX_NONE;
	struct is_device_sensor *ids;

	ids = &test_ctx.ids;
	ids->cfg = &test_ctx.cfg;

	/* Sub TC: true */
	ids->cfg->ex_mode = EX_DRAMTEST;
	g_ex_mode = is_sensor_g_ex_mode(ids);
	KUNIT_EXPECT_TRUE(test, g_ex_mode == EX_DRAMTEST);
}

static void pablo_device_sensor_v2_set_cis_data_kunit_test(struct kunit *test)
{
	u32 i2c_ch;
	struct is_device_sensor *ids;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri;
	struct is_core *core;

	ids = &test_ctx.ids;
	module = &test_ctx.module;
	sensor_peri = &test_ctx.sensor_peri;
	module->private_data = sensor_peri;
	module->pdata = &test_ctx.pdata;
	core = is_get_is_core();

	i2c_ch = module->pdata->sensor_i2c_ch;
	module->pdata->sensor_i2c_ch = SENSOR_CONTROL_I2C0;
	fp->set_cis_data(ids, module);
	KUNIT_EXPECT_TRUE(test, (sensor_peri->cis.ixc_lock == &core->ixc_lock[i2c_ch]));
}

static void pablo_device_sensor_v2_set_actuator_data_kunit_test(struct kunit *test)
{
	u32 i2c_ch;
	struct is_device_sensor *ids;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri;
	struct is_core *core;

	ids = &test_ctx.ids;
	module = &test_ctx.module;
	sensor_peri = &test_ctx.sensor_peri;
	module->private_data = sensor_peri;
	module->pdata = &test_ctx.pdata;

	core = is_get_is_core();
	ids->pdev = &test_ctx.pdev;
	ids->actuator[ids->pdev->id] = &test_ctx.actuator;

	i2c_ch = module->pdata->af_i2c_ch;
	module->pdata->af_product_name = ids->actuator[ids->pdev->id]->id = ACTUATOR_NAME_NOTHING;
	module->pdata->af_i2c_ch = SENSOR_CONTROL_I2C0;
	clear_bit(IS_SENSOR_ACTUATOR_AVAILABLE, &sensor_peri->peri_state);
	fp->set_actuator_data(ids, module);
	KUNIT_EXPECT_EQ(test, (ulong)sensor_peri->subdev_actuator, (ulong)ids->subdev_actuator[ids->pdev->id]);
	KUNIT_EXPECT_TRUE(test, (sensor_peri->actuator->ixc_lock == &core->ixc_lock[i2c_ch]));
	KUNIT_EXPECT_TRUE(test, test_bit(IS_SENSOR_ACTUATOR_AVAILABLE, &sensor_peri->peri_state));
}

static void pablo_device_sensor_v2_set_flash_data_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri;

	ids = &test_ctx.ids;
	module = &test_ctx.module;
	sensor_peri = &test_ctx.sensor_peri;
	module->private_data = sensor_peri;
	module->pdata = &test_ctx.pdata;
	ids->flash = &test_ctx.flash;

	clear_bit(IS_SENSOR_FLASH_AVAILABLE, &sensor_peri->peri_state);
	module->pdata->flash_product_name = ids->flash->id = FLADRV_NAME_NOTHING;
	fp->set_flash_data(ids, module);
	KUNIT_EXPECT_TRUE(test, test_bit(IS_SENSOR_FLASH_AVAILABLE, &sensor_peri->peri_state));
}

static void pablo_device_sensor_v2_set_ois_data_kunit_test(struct kunit *test)
{
	u32 i2c_ch;
	struct is_device_sensor *ids;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri;
	struct is_core *core;

	ids = &test_ctx.ids;
	module = &test_ctx.module;
	sensor_peri = &test_ctx.sensor_peri;
	module->private_data = sensor_peri;
	module->pdata = &test_ctx.pdata;

	core = is_get_is_core();
	ids->ois = &test_ctx.ois;

	i2c_ch = module->pdata->ois_i2c_ch;
	module->pdata->ois_product_name = ids->ois->id = OIS_NAME_NOTHING;
	module->pdata->ois_i2c_ch = SENSOR_CONTROL_I2C0;
	clear_bit(IS_SENSOR_OIS_AVAILABLE, &sensor_peri->peri_state);
	fp->set_ois_data(ids, module);
	KUNIT_EXPECT_TRUE(test, sensor_peri->ois->ixc_lock == &core->ixc_lock[i2c_ch]);
	KUNIT_EXPECT_TRUE(test, test_bit(IS_SENSOR_OIS_AVAILABLE, &sensor_peri->peri_state));
}

static void pablo_device_sensor_v2_set_mcu_data_kunit_test(struct kunit *test)
{
	u32 i2c_ch;
	struct is_device_sensor *ids;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri;
	struct is_core *core;

	ids = &test_ctx.ids;
	module = &test_ctx.module;
	sensor_peri = &test_ctx.sensor_peri;
	module->private_data = sensor_peri;
	module->pdata = &test_ctx.pdata;
	core = is_get_is_core();

	ids->mcu = &test_ctx.mcu;
	ids->mcu->actuator = &test_ctx.actuator;
	ids->mcu->ois = &test_ctx.ois;
	ids->mcu->aperture = &test_ctx.aperture;

	i2c_ch = module->pdata->mcu_i2c_ch;
	module->pdata->mcu_product_name = ids->mcu->name = MCU_NAME_NOTHING;
	clear_bit(IS_SENSOR_OIS_AVAILABLE, &sensor_peri->peri_state);
	fp->set_mcu_data(ids, module);
	KUNIT_EXPECT_EQ(test, (ulong)sensor_peri->mcu->ois->sensor_peri, (ulong)sensor_peri);
	KUNIT_EXPECT_EQ(test, (ulong)sensor_peri->mcu->aperture->sensor_peri, (ulong)sensor_peri);
	KUNIT_EXPECT_TRUE(test, (sensor_peri->mcu->ois->ixc_lock == &core->ixc_lock[i2c_ch]));
	KUNIT_EXPECT_TRUE(test, test_bit(IS_SENSOR_OIS_AVAILABLE, &sensor_peri->peri_state));
}

static void pablo_device_sensor_v2_set_eeprom_data_kunit_test(struct kunit *test)
{
	u32 i2c_ch;
	struct is_device_sensor *ids;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri;
	struct is_core *core;

	ids = &test_ctx.ids;
	module = &test_ctx.module;
	sensor_peri = &test_ctx.sensor_peri;
	module->private_data = sensor_peri;
	module->pdata = &test_ctx.pdata;
	core = is_get_is_core();
	ids->eeprom = &test_ctx.eeprom;

	i2c_ch = module->pdata->eeprom_i2c_ch;
	module->pdata->eeprom_product_name = ids->eeprom->id = EEPROM_NAME_NOTHING;
	module->pdata->eeprom_i2c_ch = SENSOR_CONTROL_I2C0;
	clear_bit(IS_SENSOR_EEPROM_AVAILABLE, &sensor_peri->peri_state);
	fp->set_eeprom_data(ids, module);
	KUNIT_EXPECT_TRUE(test, sensor_peri->eeprom->ixc_lock == &core->ixc_lock[i2c_ch]);
	KUNIT_EXPECT_TRUE(test, test_bit(IS_SENSOR_EEPROM_AVAILABLE, &sensor_peri->peri_state));
}

static void pablo_device_sensor_v2_set_laser_af_data_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;
	struct is_module_enum *module;

	ids = &test_ctx.ids;
	module = &test_ctx.module;
	module->private_data = &test_ctx.sensor_peri;
	module->pdata = &test_ctx.pdata;
	ids->laser_af = &test_ctx.laser_af;

	module->pdata->laser_af_product_name = ids->laser_af->id = LASER_AF_NAME_NOTHING;
	ids->laser_af->active = true;
	fp->set_laser_af_data(ids, module);
	KUNIT_EXPECT_FALSE(test, ids->laser_af->active);
}

static void pablo_device_sensor_v2_open_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_sensor *ids;
	struct is_video_ctx *vctx;

	ids = &test_ctx.ids;
	ids->subdev_csi = &test_ctx.subdev_csi;
	ids->private_data = &test_ctx.core_private_data;
	ids->resourcemgr = &test_ctx.resourcemgr;
	ids->resourcemgr->private_data = &test_ctx.core_private_data;
	v4l2_set_subdevdata(ids->subdev_csi, &test_ctx.csi); /* connect subdev_csi and is_device_csi*/
	test_ctx.csi.subdev = &ids->subdev_csi; /* get is_device_sensor through struct v4l2_subdev subdev_csi: container_of() */

	vctx = &test_ctx.vctx;
	vctx->video = &test_ctx.video_node;

	/* Sub TC: failed in is_resource_get() */
	ret = is_sensor_open(ids, vctx);
	KUNIT_EXPECT_EQ(test, ret, -EMFILE);
	KUNIT_EXPECT_FALSE(test, test_bit(IS_SENSOR_OPEN, &ids->state));
}

static void pablo_device_sensor_v2_close_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_sensor *ids;

	ids = &test_ctx.ids;
	ids->vctx = &test_ctx.vctx;
	ids->private_data = &test_ctx.core_private_data;

	/* Sub TC: failed (already close) */
	clear_bit(IS_SENSOR_OPEN, &ids->state);
	ret = is_sensor_close(ids);
	KUNIT_EXPECT_EQ(test, ret, -EMFILE);
}

static long sensor_module_csi_ioctl_mock(struct v4l2_subdev *subdev, unsigned int cmd, void *arg)
{
	struct is_device_csi *csi = (struct is_device_csi *)v4l2_get_subdevdata(subdev);

	switch (cmd) {
	case SENSOR_IOCTL_DMA_CANCEL:
		set_bit(CSIS_DMA_DISABLING, &csi->state);
		break;
	case SENSOR_IOCTL_CSI_S_CTRL:
		return -EINVAL;
	default:
		break;
	}

	return 0;
}

static long sensor_subdev_module_ioctl_mock(struct v4l2_subdev *subdev, unsigned int cmd, void *arg)
{
	struct is_device_sensor *ids = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);

	switch (cmd) {
	case SENSOR_IOCTL_MOD_S_EXT_CTRL:
		set_bit(IS_SENSOR_FRONT_START, &ids->state);
		break;
	case SENSOR_IOCTL_MOD_G_EXT_CTRL:
		set_bit(IS_SENSOR_FRONT_START, &ids->state);
		break;
	case SENSOR_IOCTL_MOD_S_CTRL:
		set_bit(IS_SENSOR_FRONT_START, &ids->state);
		break;
	default:
		break;
	}

	return 0;
}

static int sensor_module_log_status_mock(struct v4l2_subdev *subdev)
{
	struct is_device_sensor *ids = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);

	set_bit(IS_SENSOR_FRONT_START, &ids->state);

	return 0;
}

static int sensor_module_csi_g_errorCode_mock(struct v4l2_subdev *subdev, u32 *errorCode)
{
	struct is_device_csi *csi = (struct is_device_csi *)v4l2_get_subdevdata(subdev);

	if (!csi)
		return -EINVAL;

	set_bit(CSIS_DMA_DISABLING, &csi->state);
	return 0;
}

static int sensor_module_csi_s_buffer_mock(struct v4l2_subdev *subdev, void *buf, unsigned int *vcp)
{
	struct is_device_csi *csi = (struct is_device_csi *)v4l2_get_subdevdata(subdev);

	if (!csi)
		return -EINVAL;

	set_bit(CSIS_DMA_DISABLING, &csi->state);
	return 0;
}

static const struct v4l2_subdev_video_ops video_ops = {
	.g_input_status = sensor_module_csi_g_errorCode_mock,
	.s_rx_buffer = sensor_module_csi_s_buffer_mock,
};

static const struct v4l2_subdev_core_ops core_ops = {
	.log_status = sensor_module_log_status_mock,
	.ioctl = sensor_subdev_module_ioctl_mock,
};

static const struct v4l2_subdev_core_ops csi_ops = {
	.ioctl = sensor_module_csi_ioctl_mock,
};

static const struct v4l2_subdev_ops subdev_csi_ops = {
	.core = &csi_ops,
	.video = &video_ops,
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
};

static void pablo_device_sensor_v2_dump_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;

	ids = &test_ctx.ids;
	/*Sub TC :  !subdev_module */
	ids->subdev_module = NULL;
	fp->dump_status(ids);
	KUNIT_EXPECT_FALSE(test, test_bit(IS_SENSOR_FRONT_START, &ids->state));

	/*Sub TC : success subdev_module */
	ids->subdev_module = &test_ctx.subdev_module;
	ids->private_data = &test_ctx.sensor_peri;
	v4l2_subdev_init(ids->subdev_module, &subdev_ops);
	v4l2_set_subdev_hostdata(ids->subdev_module, ids);
	fp->dump_status(ids);
	KUNIT_EXPECT_TRUE(test, test_bit(IS_SENSOR_FRONT_START, &ids->state));
}

static void pablo_device_sensor_v2_g_instance_kunit_test(struct kunit *test)
{
	int device_id = EX_NONE;
	struct is_device_sensor *ids;

	/* Sub TC: true */
	ids = &test_ctx.ids;
	ids->device_id = 0x1;
	device_id = fp->g_instance(ids);
	KUNIT_EXPECT_TRUE(test, device_id == 0x1);
}

static void pablo_device_sensor_v2_g_position_kunit_test(struct kunit *test)
{
	int position = EX_NONE;
	struct is_device_sensor *ids;

	/* Sub TC: true */
	ids = &test_ctx.ids;
	ids->position = SP_REAR;
	position = fp->g_position(ids);
	KUNIT_EXPECT_TRUE(test, position == SP_REAR);
}

static void pablo_device_sensor_v2_s_exposure_time_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;
	struct is_module_enum *module;
	u32 exposure_time = 0;
	int ret;

	ids = &test_ctx.ids;

	/* Sub TC: !subdev_module */
	ids->subdev_module = NULL;
	ret = fp->s_exposure_time(ids, exposure_time);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: !module */
	ids->subdev_module = &test_ctx.subdev_module;
	ret = fp->s_exposure_time(ids, exposure_time);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: expousre time <=0 */
	module = &test_ctx.module;
	v4l2_set_subdevdata(ids->subdev_module, module);
	ret = fp->s_exposure_time(ids, exposure_time);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: exposure_time > 0 */
	exposure_time = 0x100;
	module->ops = &module_ops_mock;
	ret = fp->s_exposure_time(ids, exposure_time);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, ids->exposure_time, (int)exposure_time);
}

static void pablo_device_sensor_v2_dma_cancel_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;
	struct is_device_csi *csi;
	int ret;

	ids = &test_ctx.ids;
	csi = &test_ctx.csi;

	/*Sub TC :  !subdev_csi */
	ids->subdev_csi = NULL;
	ret = fp->dma_cancel(ids);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/*Sub TC :  success subdev_csi */
	ids->subdev_csi = &test_ctx.subdev_csi;
	v4l2_subdev_init(ids->subdev_csi, &subdev_csi_ops);
	v4l2_set_subdevdata(ids->subdev_csi, csi);
	ret = fp->dma_cancel(ids);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(CSIS_DMA_DISABLING, &csi->state));
}

static void pablo_device_sensor_v2_g_csis_error_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;
	struct is_device_csi *csi;
	int ret;

	ids = &test_ctx.ids;
	csi = &test_ctx.csi;

	/*Sub TC :  !subdev_csi */
	ids->subdev_csi = NULL;
	ret = fp->g_csis_error(ids);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/*Sub TC :  fail v4l2_subdevcall() */
	ids->subdev_csi = &test_ctx.subdev_csi;
	v4l2_subdev_init(ids->subdev_csi, &subdev_csi_ops);
	ret = fp->g_csis_error(ids);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/*Sub TC :  success subdev_csi */
	v4l2_set_subdevdata(ids->subdev_csi, csi);
	ret = fp->g_csis_error(ids);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(CSIS_DMA_DISABLING, &csi->state));
}

static void pablo_device_sensor_v2_s_ext_ctrls_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;
	struct v4l2_ext_controls *ctrls;
	int ret;

	ids = &test_ctx.ids;
	ctrls = &test_ctx.ctrls;

	/*Sub TC :  !subdev_module */
	ids->subdev_module = NULL;
	ret = fp->s_ext_ctrls(ids, ctrls);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/*Sub TC :  success subdev_module */
	ids->subdev_module = &test_ctx.subdev_module;
	v4l2_subdev_init(ids->subdev_module, &subdev_ops);
	v4l2_set_subdev_hostdata(ids->subdev_module, ids);
	ret = fp->s_ext_ctrls(ids, ctrls);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(IS_SENSOR_FRONT_START, &ids->state));
}

static void pablo_device_sensor_v2_g_ext_ctrls_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;
	struct v4l2_ext_controls *ctrls;
	int ret;

	ids = &test_ctx.ids;
	ctrls = &test_ctx.ctrls;

	/*Sub TC :  !subdev_module */
	ids->subdev_module = NULL;
	ret = fp->g_ext_ctrls(ids, ctrls);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/*Sub TC :  success subdev_module */
	ids->subdev_module = &test_ctx.subdev_module;
	ids->subdev_csi = &test_ctx.subdev_csi;
	v4l2_subdev_init(ids->subdev_module, &subdev_ops);
	v4l2_set_subdev_hostdata(ids->subdev_module, ids);
	ret = fp->g_ext_ctrls(ids, ctrls);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(IS_SENSOR_FRONT_START, &ids->state));
}

static void pablo_device_sensor_v2_s_frame_duration_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;
	struct is_module_enum *module;
	int ret;
	u32 framerate = 0xFFFFFFFF;
	u64 frame_duration = 0x0;

	ids = &test_ctx.ids;

	/* Sub TC : !subdev_module */
	ids->subdev_module = NULL;
	ret = fp->s_frame_duration(ids, framerate);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC : !module */
	ids->subdev_module = &test_ctx.subdev_module;
	module = NULL;
	ret = fp->s_frame_duration(ids, framerate);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC : !framerate <=0 */
	module = &test_ctx.module;
	v4l2_set_subdevdata(ids->subdev_module, module);
	ret = fp->s_frame_duration(ids, framerate);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC : true */
	framerate = 0x100;
	frame_duration = (1000 * 1000 * 1000) / framerate;
	module->ops = &module_ops_mock;
	ret = fp->s_frame_duration(ids, framerate);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, ids->frame_duration, frame_duration);
}

static void pablo_device_sensor_v2_s_shutterspeed_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;
	struct is_module_enum *module;
	int ret;
	u32 shutterspeed = 0;

	ids = &test_ctx.ids;

	/* Sub TC : !subdev_module */
	ids->subdev_module = NULL;
	ret = fp->s_shutterspeed(ids, shutterspeed);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC : !module */
	ids->subdev_module = &test_ctx.subdev_module;
	ret = fp->s_shutterspeed(ids, shutterspeed);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC : !shutterspeed <=0 */
	module = &test_ctx.module;
	v4l2_set_subdevdata(ids->subdev_module, module);
	ret = fp->s_shutterspeed(ids, shutterspeed);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC : true */
	shutterspeed = 0x100;
	module->ops = &module_ops_mock;
	ret = fp->s_shutterspeed(ids, shutterspeed);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_device_sensor_v2_s_ctrl_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;
	struct is_device_csi *csi;
	struct v4l2_control *ctrl;
	int ret;

	ids = &test_ctx.ids;
	csi = &test_ctx.csi;
	ctrl = &test_ctx.ctrl;

	ids->subdev_module = &test_ctx.subdev_module;
	ids->subdev_csi = &test_ctx.subdev_csi;
	v4l2_subdev_init(ids->subdev_csi, &subdev_csi_ops);
	v4l2_set_subdevdata(ids->subdev_csi, csi);

	/*Sub TC : !ctrl->id */
	ctrl->id = SENSOR_IOCTL_CSI_S_CTRL;
	ret = fp->s_ctrl(ids, ctrl);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/*Sub TC : true */
	v4l2_subdev_init(ids->subdev_module, &subdev_ops);
	v4l2_set_subdev_hostdata(ids->subdev_module, ids);
	ctrl->id = SENSOR_IOCTL_DMA_CANCEL;
	ret = fp->s_ctrl(ids, ctrl);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(IS_SENSOR_FRONT_START, &ids->state));
}

static int cis_get_frame_id_mock(struct v4l2_subdev *subdev, u8 *embedded_buf, u16 *frame_id)
{
	return 0;
}

static int cis_check_rev_on_init_mock(struct v4l2_subdev *subdev)
{
	struct is_cis *cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	if (!cis->client)
		return -EINVAL;

	return 0;
}

static int cis_init_mock(struct v4l2_subdev *subdev)
{
	return 0;
}

static int cis_set_global_setting_mock(struct v4l2_subdev *subdev)
{
	return 0;
}

static struct is_cis_ops cis_ops_mock = {
	.cis_get_frame_id = cis_get_frame_id_mock,
	.cis_check_rev_on_init = cis_check_rev_on_init_mock,
	.cis_init = cis_init_mock,
	.cis_set_global_setting = cis_set_global_setting_mock,
};

static void pablo_device_sensor_v2_notify_by_dma_end_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;
	struct is_device_csi *csi;
	struct is_module_enum *module;
	struct is_cis *cis;
	int ret;
	void *arg;
	unsigned int notification = CSIS_NOTIFY_DMA_END_VC_EMBEDDED;

	ids = &test_ctx.ids;
	csi = &test_ctx.csi;
	cis = &test_ctx.cis;
	module = &test_ctx.module;
	arg = kunit_kzalloc(test, sizeof(struct is_frame), 0);

#ifdef USE_CAMERA_EMBEDDED_HEADER
	/* Sub TC : !subdev_module */
	ret = fp->notify_by_dma_end(ids, arg, notification);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC : !sensor_peri */
	ids->subdev_module = &test_ctx.subdev_module;
	v4l2_set_subdevdata(ids->subdev_module, module);
	module->pdata = &test_ctx.pdata;
	ret = fp->notify_by_dma_end(ids, arg, notification);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC : !csi */
	module->private_data = &test_ctx.sensor_peri;
	ids->subdev_csi = &test_ctx.subdev_csi;
	ret = fp->notify_by_dma_end(ids, arg, notification);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC : true */
	cis->cis_ops = &cis_ops_mock;
	v4l2_set_subdevdata(ids->subdev_csi, csi);
	test_ctx.sensor_peri.subdev_cis = &test_ctx.subdev_cis;
	v4l2_set_subdevdata(test_ctx.sensor_peri.subdev_cis, cis);
	ret = fp->notify_by_dma_end(ids, arg, notification);
	KUNIT_EXPECT_EQ(test, ret, 0);
#endif

	/* Sub TC : CSIS_NOTIFY_DMA_END_VC_MIPISTAT */
	notification = CSIS_NOTIFY_DMA_END_VC_MIPISTAT;
	ret = fp->notify_by_dma_end(ids, arg, notification);
	KUNIT_EXPECT_EQ(test, ret, 0);
	kunit_kfree(test, arg);
}

static void pablo_device_sensor_v2_resume_kunit_test(struct kunit *test)
{
	struct device *dev;
	int ret;

	dev = is_get_is_dev();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);
	ret = fp->resume(dev);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_device_sensor_v2_standby_flush_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;
	int ret;

	ids = &test_ctx.ids;
	ids->group_sensor = test_ctx.group;
	ids->ischain = &test_ctx.idi;
	ret = fp->standby_flush(ids);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static int is_sensor_group_tag_mock(struct is_device_sensor *device, struct is_frame *frame,
				    struct camera2_node *ldr_node)
{
	set_bit(IS_SENSOR_BACK_START, &device->state);
	return 0;
}

static struct pablo_device_sensor_ops ops = {
	.group_tag = is_sensor_group_tag_mock,
};

static void pablo_device_sensor_v2_notify_by_line_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;
	unsigned int notification = FLITE_NOTIFY_FSTART;
	struct is_frame *frame;
	struct is_framemgr *framemgr = &test_ctx.queue.framemgr;
	int ret;

	ids = &test_ctx.ids;
	ids->line_fcount = 123;
	ids->group_sensor.head = &test_ctx.group;
	ids->group_sensor.head->leader.vctx = &test_ctx.vctx;
	ids->group_sensor.head->leader.vctx->queue = &test_ctx.queue;
	ids->group_sensor.leader.id = ENTRY_SENSOR;

	/* framemgr probe*/
	frame_manager_probe(framemgr, "KUT-SEN");
	frame_manager_open(framemgr, 8, false);

	/* Sub TC: !frame */
	frame = peek_frame(framemgr, FS_FREE);
	frame->fcount = 0;
	set_bit(ids->group_sensor.leader.id, &frame->out_flag);
	trans_frame(framemgr, frame, FS_PROCESS);
	ret = fp->notify_by_line(ids, notification);
	KUNIT_EXPECT_EQ(test, ret, 0);
	trans_frame(framemgr, frame, FS_FREE);
	clear_bit(ids->group_sensor.leader.id, &frame->out_flag);

	/* Sub TC: !test_bit(group->leader.id, &frame->out_flag) */
	frame = peek_frame(framemgr, FS_FREE);
	frame->fcount = ids->line_fcount + 1;
	clear_bit(ids->group_sensor.leader.id, &frame->out_flag);
	trans_frame(framemgr, frame, FS_PROCESS);
	ret = fp->notify_by_line(ids, notification);
	KUNIT_EXPECT_EQ(test, ret, 0);
	trans_frame(framemgr, frame, FS_FREE);

	/* Sub TC: !atomic_read(&group->scount) */
	frame = peek_frame(framemgr, FS_FREE);
	frame->fcount = ids->line_fcount + 1;
	set_bit(ids->group_sensor.leader.id, &frame->out_flag);
	trans_frame(framemgr, frame, FS_PROCESS);
	atomic_set(&ids->group_sensor.scount, 0);
	ret = fp->notify_by_line(ids, notification);
	KUNIT_EXPECT_EQ(test, ret, 0);
	trans_frame(framemgr, frame, FS_FREE);
	clear_bit(ids->group_sensor.leader.id, &frame->out_flag);

	/* Sub TC: success */
	frame = peek_frame(framemgr, FS_FREE);
	frame->fcount = ids->line_fcount + 1;
	set_bit(ids->group_sensor.leader.id, &frame->out_flag);
	trans_frame(framemgr, frame, FS_PROCESS);
	ids->group_sensor.device = &test_ctx.idi;
	ids->group_sensor.device->sensor = &test_ctx.ids;
	ids->ops = &ops;
	atomic_set(&ids->group_sensor.scount, 1);
	ret = fp->notify_by_line(ids, notification);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(IS_SENSOR_BACK_START, &ids->state));
	trans_frame(framemgr, frame, FS_FREE);

	/* Restore */
	frame_manager_close(framemgr);
}

static void pablo_device_sensor_v2_buf_skip_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;
	struct is_frame *ldr_frame, *sub_frame;
	struct v4l2_subdev *subdev_module;
	struct is_subdev *subdev;
	struct is_framemgr *ldr_framemgr, *sub_framemgr;
	struct is_video_ctx *ldr_vctx;
	struct is_queue *ldr_queue;
	int ret;

	ldr_vctx = kunit_kzalloc(test, sizeof(struct is_video_ctx), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ldr_vctx);
	ldr_queue = kunit_kzalloc(test, sizeof(struct is_queue), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ldr_queue);

	ids = &test_ctx.ids;

	ids->group_sensor.leader.vctx = ldr_vctx;
	ids->group_sensor.leader.vctx->queue = ldr_queue;
	ldr_framemgr = &ldr_queue->framemgr;
	subdev_module = &test_ctx.subdev_module;
	subdev = &test_ctx.subdev;
	subdev->vctx = &test_ctx.vctx;
	subdev->vctx->queue = &test_ctx.queue;
	sub_framemgr = &test_ctx.queue.framemgr;

	/* framemgr probe */
	frame_manager_probe(ldr_framemgr, "KUT-SEN_LDR");
	frame_manager_open(ldr_framemgr, 8, false);
	frame_manager_probe(sub_framemgr, "KUT-SEN_SUB");
	frame_manager_open(sub_framemgr, 8, false);

	/* Sub TC : stream is NULL */
	ldr_frame = peek_frame(ldr_framemgr, FS_FREE);
	sub_frame = peek_frame(sub_framemgr, FS_FREE);
	set_bit(subdev->id, &ldr_frame->out_flag);
	trans_frame(sub_framemgr, sub_frame, FS_REQUEST);
	ret = fp->buf_skip(ids, subdev, subdev_module, ldr_frame);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_EQ(test, test_bit(subdev->id, &ldr_frame->out_flag), 1);
	trans_frame(sub_framemgr, sub_frame, FS_FREE);
	clear_bit(subdev->id, &ldr_frame->out_flag);

	/* Sub TC : !frame */
	sub_frame = peek_frame(sub_framemgr, FS_FREE);
	set_bit(subdev->id, &ldr_frame->out_flag);
	sub_frame->stream = &test_ctx.stream;
	ret = fp->buf_skip(ids, subdev, subdev_module, ldr_frame);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_EQ(test, test_bit(subdev->id, &ldr_frame->out_flag), 1);
	trans_frame(sub_framemgr, sub_frame, FS_FREE);
	clear_bit(subdev->id, &ldr_frame->out_flag);

	/* Sub TC : FS_PROCESS success */
	sub_frame = peek_frame(sub_framemgr, FS_FREE);
	set_bit(subdev->id, &ldr_frame->out_flag);
	trans_frame(sub_framemgr, sub_frame, FS_REQUEST);
	sub_frame->stream = &test_ctx.stream;
	ret = fp->buf_skip(ids, subdev, subdev_module, ldr_frame);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, test_bit(subdev->id, &ldr_frame->out_flag), 0);
	trans_frame(sub_framemgr, sub_frame, FS_FREE);
	clear_bit(subdev->id, &ldr_frame->out_flag);

	/* Restore */
	frame_manager_close(ldr_framemgr);
	frame_manager_close(sub_framemgr);
	kunit_kfree(test, ldr_vctx);
	kunit_kfree(test, ldr_queue);
}

static void pablo_device_sensor_v2_buf_tag_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;
	struct is_device_csi *csi;
	struct is_frame *ldr_frame, *sub_frame;
	struct v4l2_subdev *subdev_csi;
	struct is_subdev *subdev;
	struct is_framemgr *sub_framemgr, *ldr_framemgr;
	struct is_queue *ldr_queue;
	int ret;

	ldr_queue = kunit_kzalloc(test, sizeof(struct is_queue), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ldr_queue);

	ids = &test_ctx.ids;
	csi = &test_ctx.csi;
	ldr_framemgr = &ldr_queue->framemgr;
	sub_framemgr = &test_ctx.queue.framemgr;
	subdev = &test_ctx.subdev;
	subdev_csi = &test_ctx.subdev_csi;
	subdev->vctx = &test_ctx.vctx;
	subdev->vctx->queue = &test_ctx.queue;

	/* framemgr probe */
	frame_manager_probe(sub_framemgr, "KUT-SEN-SUB");
	frame_manager_open(sub_framemgr, 8, false);
	frame_manager_probe(ldr_framemgr, "KUT-SEN-LDR");
	frame_manager_open(ldr_framemgr, 8, false);

	/* Sub TC : !frame */
	ldr_frame = peek_frame(ldr_framemgr, FS_FREE);
	sub_frame = peek_frame(sub_framemgr, FS_FREE);
	ret = fp->buf_tag(ids, subdev, subdev_csi, ldr_frame);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_FALSE(test, test_bit(CSIS_DMA_DISABLING, &csi->state));

	/* Sub TC : !subdev_csi */
	trans_frame(sub_framemgr, sub_frame, FS_REQUEST);
	sub_frame->stream = &test_ctx.stream;
	v4l2_subdev_init(subdev_csi, &subdev_csi_ops);
	ret = fp->buf_tag(ids, subdev, subdev_csi, sub_frame);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_FALSE(test, test_bit(CSIS_DMA_DISABLING, &csi->state));
	trans_frame(sub_framemgr, sub_frame, FS_FREE);

	/* Sub TC : success */
	ldr_frame->fcount = 123;
	ldr_frame->index = 1;
	sub_frame = peek_frame(sub_framemgr, FS_FREE);
	trans_frame(sub_framemgr, sub_frame, FS_REQUEST);
	sub_frame->stream = &test_ctx.stream;
	v4l2_set_subdevdata(subdev_csi, csi);
	ret = fp->buf_tag(ids, subdev, subdev_csi, ldr_frame);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(CSIS_DMA_DISABLING, &csi->state));
	KUNIT_EXPECT_EQ(test, sub_frame->fcount, ldr_frame->fcount);
	KUNIT_EXPECT_EQ(test, sub_frame->stream->findex, ldr_frame->index);
	KUNIT_EXPECT_EQ(test, sub_frame->stream->fcount, ldr_frame->fcount);
	trans_frame(sub_framemgr, sub_frame, FS_FREE);

	/* Restore */
	frame_manager_close(ldr_framemgr);
	frame_manager_close(sub_framemgr);
	kunit_kfree(test, ldr_queue);
}

static void pablo_device_sensor_v2_s_framerate_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;
	struct is_module_enum *module;
	struct v4l2_streamparm *param;
	int ret;
	u32 fps = 10, numerator = 100;

	ids = &test_ctx.ids;
	module = &test_ctx.module;
	param = &test_ctx.param;

	/* Sub TC : numerator == 0 */
	ids->subdev_module = &test_ctx.subdev_module;
	param->parm.capture.timeperframe.numerator = 0;
	ret = fp->s_framerate(ids, param);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC : framerate == 0 */
	param->parm.capture.timeperframe.denominator = 70;
	param->parm.capture.timeperframe.numerator = numerator;
	ret = fp->s_framerate(ids, param);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* Sub TC : !subdev_module */
	param->parm.capture.timeperframe.denominator = numerator * fps;
	param->parm.capture.timeperframe.numerator = numerator;
	ret = fp->s_framerate(ids, param);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC : IS_SENSOR_FRONT_START */
	v4l2_set_subdevdata(ids->subdev_module, module);
	set_bit(IS_SENSOR_FRONT_START, &ids->state);
	ret = fp->s_framerate(ids, param);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC : type != V4L2_BUF_TYPE_VIDEOE_CAPTURE_MPLANE */
	clear_bit(IS_SENSOR_FRONT_START, &ids->state);
	param->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	ret = fp->s_framerate(ids, param);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC : framerate>max_framerate */
	param->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	module->max_framerate = 9;
	ret = fp->s_framerate(ids, param);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC : success */
	module->max_framerate = 100;
	ret = fp->s_framerate(ids, param);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, ids->image.framerate, fps);
	KUNIT_EXPECT_EQ(test, ids->max_target_fps, fps);
}

static void pablo_device_sensor_v2_prepare_retention_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;
	struct is_cis *cis;
	int position;
	struct is_module_enum *module;
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_vendor_private *priv = core->vendor.private_data;

	position = 0;
	ids = &test_ctx.ids;
	cis = &test_ctx.sensor_peri.cis;
	module = &ids->module_enum[0];
	module->pdata = &test_ctx.pdata;
	module->sensor_id = priv->sensor_id[0];
	module->sensor_name = (char *)priv->sensor_name[0];

	/* init gpio ops */
	module->subdev = &test_ctx.subdev_module;
	v4l2_set_subdev_hostdata(module->subdev, ids);
	ids->resourcemgr = &test_ctx.resourcemgr;
	is_get_gpio_ops(module->pdata);
	module->pdata->pinctrl_index[SENSOR_SCENARIO_NORMAL][GPIO_SCENARIO_ON] = 1;
	module->pdata->pinctrl_index[SENSOR_SCENARIO_NORMAL][GPIO_SCENARIO_SENSOR_RETENTION_ON] = 1;
	module->pdata->pinctrl_index[SENSOR_SCENARIO_NORMAL][GPIO_SCENARIO_OFF] = 1;

	/* Sub TC : retention_mode == SENSOR_RETENTION_UNSUPPORTED*/
	atomic_set(&ids->module_count, 1);
	ret = fp->prepare_retention(ids, position);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC : !cis->client */
	module->ext.use_retention_mode = SENSOR_RETENTION_ACTIVATED;
	module->private_data = &test_ctx.sensor_peri;
	test_ctx.sensor_peri.subdev_cis = &test_ctx.subdev_cis;
	v4l2_set_subdevdata(test_ctx.sensor_peri.subdev_cis, cis);
	cis->cis_ops = &cis_ops_mock;
	cis->client = NULL;
	ret = fp->prepare_retention(ids, position);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC : success */
	cis->client = &test_ctx.sensor_peri.cis.client;
	ret = fp->prepare_retention(ids, position);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_device_sensor_v2_snr_kunit_test(struct kunit *test)
{
	struct is_device_sensor *ids;
	struct is_device_csi *csi;
	unsigned long force_stop = 0;

	/* Sub TC : device->snr_check == true */
	ids = &test_ctx.ids;
	ids->snr_check = true;
	fp->snr(&ids->snr_timer);
	KUNIT_EXPECT_TRUE(test, ids->snr_check);
	KUNIT_EXPECT_EQ(test, ids->force_stop, force_stop);
	KUNIT_EXPECT_FALSE(test, test_bit(IS_SENSOR_FRONT_SNR_STOP, &ids->state));

	/* Sub TC : test_bit(IS_SENSOR_FRONT_START, &device->state) == true */
	set_bit(IS_SENSOR_FRONT_START, &ids->state);
	csi = &test_ctx.csi;
	ids->subdev_csi = &test_ctx.subdev_csi;
	fp->snr(&ids->snr_timer);
	KUNIT_EXPECT_FALSE(test, ids->snr_check);
	KUNIT_EXPECT_EQ(test, ids->force_stop, force_stop);
	KUNIT_EXPECT_TRUE(test, test_bit(IS_SENSOR_FRONT_SNR_STOP, &ids->state));
	clear_bit(IS_SENSOR_FRONT_SNR_STOP, &ids->state);

	/* Sub TC : device->force_stop = 1*/
	ids->force_stop = 1;
	fp->snr(&ids->snr_timer);
	KUNIT_EXPECT_FALSE(test, ids->snr_check);
	KUNIT_EXPECT_EQ(test, ids->force_stop, force_stop);
	KUNIT_EXPECT_TRUE(test, test_bit(IS_SENSOR_FRONT_SNR_STOP, &ids->state));
}

static void pablo_device_sensor_v2_suspend_kunit_test(struct kunit *test)
{
	struct device *dev;
	struct is_device_sensor *device;
	struct platform_device *pdev;
	int ret;

	/* Sub TC : success */
	dev = &test_ctx.pdev.dev;
	pdev = to_platform_device(dev);
	device = &test_ctx.ids;
	device->pdata = &test_ctx.psensor;
	is_sensor_get_clk_ops(device->pdata);
	platform_set_drvdata(pdev, device);
	ret = fp->suspend(dev);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static int pablo_device_sensor_v2_kunit_test_init(struct kunit *test)
{
	fp = pablo_kunit_get_device_sensor_func();
	memset(&test_ctx, 0x00, sizeof(struct test_ctx));

	return 0;
}

static void pablo_device_sensor_v2_kunit_test_exit(struct kunit *test)
{
}

static struct kunit_case pablo_device_sensor_v2_kunit_test_cases[] = {
	KUNIT_CASE(pablo_device_sensor_v2_s_again_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_get_frame_type_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_g_module_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_get_sensor_interface_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_g_device_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_cmp_mode_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_g_mode_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_mclk_on_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_mclk_off_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_iclk_on_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_iclk_off_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_gpio_on_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_gpio_off_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_gpio_dbg_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_g_max_size_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_g_aeb_mode_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_get_mono_mode_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_g_ex_mode_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_set_cis_data_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_set_actuator_data_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_set_flash_data_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_set_ois_data_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_set_mcu_data_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_set_eeprom_data_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_set_laser_af_data_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_open_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_close_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_dump_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_g_instance_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_g_position_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_s_exposure_time_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_dma_cancel_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_g_csis_error_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_s_ext_ctrls_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_g_ext_ctrls_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_s_frame_duration_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_s_shutterspeed_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_s_ctrl_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_suspend_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_notify_by_dma_end_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_resume_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_standby_flush_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_notify_by_line_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_buf_skip_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_buf_tag_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_s_framerate_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_snr_kunit_test),
	KUNIT_CASE(pablo_device_sensor_v2_prepare_retention_kunit_test),
	{},
};

struct kunit_suite pablo_device_sensor_v2_kunit_test_suite = {
	.name = "pablo-device-sensor-v2-kunit-test",
	.init = pablo_device_sensor_v2_kunit_test_init,
	.exit = pablo_device_sensor_v2_kunit_test_exit,
	.test_cases = pablo_device_sensor_v2_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_device_sensor_v2_kunit_test_suite);

MODULE_LICENSE("GPL");
