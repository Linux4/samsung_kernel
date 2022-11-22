/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>

#include <mach/exynos-fimc-is.h>
#include <mach/exynos-fimc-is-sensor.h>
#include <plat/devs.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/mipi_csis.h>
#include <media/exynos_camera.h>

#include "board-xyref5260.h"

#if defined(CONFIG_VIDEO_EXYNOS_FIMC_IS)
static struct exynos_platform_fimc_is exynos_fimc_is_data;

static struct exynos_fimc_is_subip_info subip_info = {
	._mcuctl = {
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 162,
	},
	._3a0 = {
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 0,
	},
	._3a1 = {
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 0,
	},
	._isp = {
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 0,
	},
	._drc = {
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 0,
	},
	._scc = {
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 0,
	},
	._odc = {
		.valid		= 0,
		.full_bypass	= 0,
		.version	= 0,
	},
	._dis = {
		.valid		= 0,
		.full_bypass	= 0,
		.version	= 0,
	},
	._dnr = {
		.valid		= 0,
		.full_bypass	= 0,
		.version	= 0,
	},
	._scp = {
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 0,
	},
	._fd = {
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 0,
	},
};

FIMC_IS_DECLARE_QOS_ENUM(INT) {
	FIMC_IS_MAKE_QOS_IDX_NM(FIMC_IS_INT_L0),
};

FIMC_IS_DECLARE_QOS_ENUM(MIF) {
	FIMC_IS_MAKE_QOS_IDX_NM(FIMC_IS_MIF_L0),
};

FIMC_IS_DECLARE_QOS_ENUM(I2C) {
	FIMC_IS_MAKE_QOS_IDX_NM(FIMC_IS_I2C_L0),
};

static u32 exynos_fimc_is_int_qos_table[] = {
	[FIMC_IS_MAKE_QOS_IDX_NM(FIMC_IS_INT_L0)]= 400000,
};

static u32 exynos_fimc_is_mif_qos_table[] = {
	[FIMC_IS_MAKE_QOS_IDX_NM(FIMC_IS_MIF_L0)]= 800000,
};

static u32 exynos_fimc_is_i2c_qos_table[] = {
	[FIMC_IS_MAKE_QOS_IDX_NM(FIMC_IS_I2C_L0)]= 108000000,
};

static int exynos_fimc_is_get_int_qos(int scenario_id)
{
	u32 qos_idx = -1;
	switch (scenario_id) {
	case FIMC_IS_SN_MAX:
	case FIMC_IS_SN_DEFAULT:
		/* L0 */
		qos_idx =  FIMC_IS_MAKE_QOS_IDX_NM(FIMC_IS_INT_L0);
		break;
	default:
		pr_err("%s : not matched any scenario(%d)\n", __func__, scenario_id);
		break;
	}

	if ((qos_idx < 0) ||
			(qos_idx >= ARRAY_SIZE(exynos_fimc_is_int_qos_table))) {
		pr_err("%s : qos_idx(%d)\n", __func__, qos_idx);
		qos_idx =  FIMC_IS_MAKE_QOS_IDX_NM(FIMC_IS_INT_L0);
	}

	return exynos_fimc_is_data.int_qos_table[qos_idx];
}

static int exynos_fimc_is_get_mif_qos(int scenario_id)
{
	u32 qos_idx = -1;
	switch (scenario_id) {
	case FIMC_IS_SN_MAX:
	case FIMC_IS_SN_DEFAULT:
		/* L0 */
		qos_idx =  FIMC_IS_MAKE_QOS_IDX_NM(FIMC_IS_MIF_L0);
		break;
	default:
		pr_err("%s : not matched any scenario(%d)\n", __func__, scenario_id);
		break;
	}

	if ((qos_idx < 0) ||
			(qos_idx >= ARRAY_SIZE(exynos_fimc_is_mif_qos_table))) {
		pr_err("%s : qos_idx(%d)\n", __func__, qos_idx);
		qos_idx =  FIMC_IS_MAKE_QOS_IDX_NM(FIMC_IS_MIF_L0);
	}

	return exynos_fimc_is_data.mif_qos_table[qos_idx];
}

static int exynos_fimc_is_get_i2c_qos(int scenario_id)
{
	u32 qos_idx = -1;
	switch (scenario_id) {
	case FIMC_IS_SN_MAX:
	case FIMC_IS_SN_DEFAULT:
		/* L0 */
		qos_idx =  FIMC_IS_MAKE_QOS_IDX_NM(FIMC_IS_I2C_L0);
		break;
	default:
		pr_err("%s : not matched any scenario(%d)\n", __func__, scenario_id);
		break;
	}

	if ((qos_idx < 0) ||
			(qos_idx >= ARRAY_SIZE(exynos_fimc_is_i2c_qos_table))) {
		pr_err("%s : qos_idx(%d)\n", __func__, qos_idx);
		qos_idx =  FIMC_IS_MAKE_QOS_IDX_NM(FIMC_IS_I2C_L0);
	}

	return exynos_fimc_is_data.i2c_qos_table[qos_idx];
}

static struct exynos_platform_fimc_is_sensor s5k3h7 = {
	.scenario	= SENSOR_SCENARIO_NORMAL,
	.mclk_ch	= 0,
	.csi_ch		= CSI_ID_A,
	.flite_ch	= FLITE_ID_A,
	.i2c_ch		= SENSOR_CONTROL_I2C0,
	.i2c_addr	= 0x20,
	.is_bns		= 0,
	.flash_first_gpio	= 5,
	.flash_second_gpio	= 6,
};

static struct exynos_platform_fimc_is_sensor s5k6b2 = {
	.scenario	= SENSOR_SCENARIO_NORMAL,
	.mclk_ch	= 1,
	.csi_ch		= CSI_ID_B,
	.flite_ch	= FLITE_ID_B,
	.i2c_ch		= SENSOR_CONTROL_I2C1,
	.i2c_addr	= 0x20,
	.is_bns		= 0,
};
#endif

static struct platform_device *camera_devices[] __initdata = {
#if defined(CONFIG_VIDEO_EXYNOS_FIMC_IS)
	&exynos_device_fimc_is_sensor0,
	&exynos_device_fimc_is_sensor1,
	&exynos4_device_fimc_is,
#endif
};

void __init exynos4_xyref4415_camera_init(void)
{
#if defined(CONFIG_VIDEO_EXYNOS_FIMC_IS)
	dev_set_name(&exynos4_device_fimc_is.dev, "s5p-mipi-csis.0");
	clk_add_alias("gscl_wrap0", FIMC_IS_DEV_NAME, "gscl_wrap0", &exynos4_device_fimc_is.dev);
	dev_set_name(&exynos4_device_fimc_is.dev, "s5p-mipi-csis.1");
	clk_add_alias("gscl_wrap1", FIMC_IS_DEV_NAME, "gscl_wrap1", &exynos4_device_fimc_is.dev);
	dev_set_name(&exynos4_device_fimc_is.dev, FIMC_IS_DEV_NAME);

	exynos_fimc_is_data.subip_info = &subip_info;
	exynos_fimc_is_data.int_qos_table =  exynos_fimc_is_int_qos_table;
	exynos_fimc_is_data.mif_qos_table =  exynos_fimc_is_mif_qos_table;
	exynos_fimc_is_data.i2c_qos_table =  exynos_fimc_is_i2c_qos_table;
	exynos_fimc_is_data.get_int_qos = exynos_fimc_is_get_int_qos;
	exynos_fimc_is_data.get_mif_qos = exynos_fimc_is_get_mif_qos;
	exynos_fimc_is_data.get_i2c_qos = exynos_fimc_is_get_i2c_qos;

	exynos_fimc_is_set_platdata(&exynos_fimc_is_data);

	/* s5k3h7: normal: on */
	SET_PIN(&s5k3h7, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0,
		EXYNOS4_GPM2(2), (3 << 8), "GPM2.2 (CAM_MCLK)", PIN_FUNCTION);
	SET_PIN(&s5k3h7, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1,
		EXYNOS4_GPM3(3), (2 << 12), "GPM3.3 (CAM_FLASH_EN)", PIN_FUNCTION);
	SET_PIN(&s5k3h7, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2,
		EXYNOS4_GPM3(4), (2 << 16), "GPM3.4 (CAM_FLASH_TORCH)", PIN_FUNCTION);
	SET_PIN(&s5k3h7, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3,
		EXYNOS4_GPM4(0), (2 << 0), "GPM4.0 (CAM_I2C0_SCL)", PIN_FUNCTION);
	SET_PIN(&s5k3h7, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4,
		EXYNOS4_GPM4(1), (2 << 4), "GPM4.1 (CAM_I2C0_SDA)", PIN_FUNCTION);
	SET_PIN(&s5k3h7, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5,
		0, 0, "main_cam_core_1v2", PIN_REGULATOR_ON);
	SET_PIN(&s5k3h7, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6,
		0, 0, "main_cam_io_1v8", PIN_REGULATOR_ON);
	SET_PIN(&s5k3h7, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 7,
		0, 0, "main_cam_sensor_a2v8", PIN_REGULATOR_ON);
	SET_PIN(&s5k3h7, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 8,
		0, 0, "main_cam_af_2v8", PIN_REGULATOR_ON);
	SET_PIN(&s5k3h7, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 9,
		EXYNOS4_GPM2(4), 1, "GPM2.4 (MAIN_CAM_RST)", PIN_RESET);
	SET_PIN(&s5k3h7, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 11,
		0, 0, "", PIN_END);

	/* s5k3h7: normal: off */
	SET_PIN(&s5k3h7, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0,
		EXYNOS4_GPM2(4), 1, "GPM2.4 (MAIN_CAM_RST)", PIN_RESET);
	SET_PIN(&s5k3h7, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1,
		EXYNOS4_GPM2(2), 1, "GPM2.2 (CAM_MCLK)", PIN_INPUT);
	SET_PIN(&s5k3h7, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3,
		0, 0, "main_cam_core_1v2", PIN_REGULATOR_OFF);
	SET_PIN(&s5k3h7, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3,
		0, 0, "main_cam_io_1v8", PIN_REGULATOR_OFF);
	SET_PIN(&s5k3h7, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4,
		0, 0, "main_cam_sensor_a2v8", PIN_REGULATOR_OFF);
	SET_PIN(&s5k3h7, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5,
		0, 0, "main_cam_af_2v8", PIN_REGULATOR_OFF);
	SET_PIN(&s5k3h7, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6,
		0, 0, "", PIN_END);

	/* s5k6b2: normal: on */
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0,
		EXYNOS4_GPM3(2), 1, "GPM3.2 (CAM_VT_RST)", PIN_RESET);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1,
		EXYNOS4_GPM2(2), (3 << 8), "GPM2.2 (CAM_MCLK)", PIN_FUNCTION);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2,
		EXYNOS4_GPM4(3), (2 << 12), "GPM4.3 (CAM_I2C1_SCL)", PIN_FUNCTION);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3,
		EXYNOS4_GPM4(2), (2 << 8), "GPM4.2 (CAM_I2C1_SDA)", PIN_FUNCTION);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4,
		EXYNOS4_GPA1(3), (0 << 12), "GPA1.3 (HOST_I2C1_SCL)", PIN_FUNCTION);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5,
		EXYNOS4_GPA1(2), (0 << 8), "GAP1.2 (HOST_I2C1_SDA)", PIN_FUNCTION);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6,
		EXYNOS4_GPY6(5), 1, "GPY6.5 (VTCAM_1V8_EN)", PIN_OUTPUT_HIGH);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 7,
		0, 0, "vt_cam_sensor_a2v8", PIN_REGULATOR_ON);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 8,
		0, 0, "", PIN_END);

	/* s5k6b2: normal: off */
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0,
		EXYNOS4_GPM3(2), 1, "GPM3.2 (CAM_VT_RST)", PIN_RESET);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1,
		EXYNOS4_GPM2(2), 1, "GPM2.2 (CAM_MCLK)", PIN_INPUT);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2,
		EXYNOS4_GPY6(5), 1, "GPY6.5 (VTCAM_1V8_EN)", PIN_OUTPUT_LOW);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3,
		0, 0, "vt_cam_sensor_a2v8", PIN_REGULATOR_OFF);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4,
		0, 0, "", PIN_END);

	exynos_fimc_is_sensor_set_platdata(&s5k3h7, &exynos_device_fimc_is_sensor0);
	exynos_fimc_is_sensor_set_platdata(&s5k6b2, &exynos_device_fimc_is_sensor1);
#endif

	platform_add_devices(camera_devices, ARRAY_SIZE(camera_devices));
}
