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
#include <linux/delay.h>

#include <mach/exynos-fimc-is.h>
#include <mach/exynos-fimc-is-sensor.h>
#include <plat/devs.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/mipi_csis.h>
#include <media/exynos_camera.h>

#include "board-xyref5260.h"

#include <linux/i2c-gpio.h>
#include <plat/iic.h>

/* #define EXTERNAL_ISP */
/* #define GPIO_EMULATION */

#if defined(CONFIG_VIDEO_EXYNOS_FIMC_IS)
static struct exynos_platform_fimc_is exynos_fimc_is_data;

struct exynos_fimc_is_clk_gate_info gate_info = {
	.groups = {
		[FIMC_IS_GRP_3A0] = {
			.mask_clk_on_org	= 0,
			.mask_clk_on_mod	= 0,
			.mask_clk_off_self_org	= 0,
			.mask_clk_off_self_mod	= 0,
			.mask_clk_off_depend	= 0,
			.mask_cond_for_depend	= 0,
		},
		[FIMC_IS_GRP_3A1] = {
			.mask_clk_on_org	= 0,
			.mask_clk_on_mod	= 0,
			.mask_clk_off_self_org	= 0,
			.mask_clk_off_self_mod	= 0,
			.mask_clk_off_depend	= 0,
			.mask_cond_for_depend	= 0,
		},
		[FIMC_IS_GRP_ISP] = {
			.mask_clk_on_org	=
				(1 << FIMC_IS_GATE_ISP_IP) |
				(1 << FIMC_IS_GATE_DRC_IP) |
				(1 << FIMC_IS_GATE_SCC_IP) |
				(1 << FIMC_IS_GATE_SCP_IP) |
				(1 << FIMC_IS_GATE_FD_IP),
			.mask_clk_on_mod	= 0,
			.mask_clk_off_self_org	=
				(1 << FIMC_IS_GATE_ISP_IP) |
				(1 << FIMC_IS_GATE_DRC_IP) |
				(1 << FIMC_IS_GATE_SCC_IP) |
				(1 << FIMC_IS_GATE_SCP_IP) |
				(1 << FIMC_IS_GATE_FD_IP),
			.mask_clk_off_self_mod	= 0,
			.mask_clk_off_depend	= 0,
			.mask_cond_for_depend	= 0,
		},
		[FIMC_IS_GRP_DIS] = {
			.mask_clk_on_org	= 0,
			.mask_clk_on_mod	= 0,
			.mask_clk_off_self_org	= 0,
			.mask_clk_off_self_mod	= 0,
			.mask_clk_off_depend	= 0,
			.mask_cond_for_depend	= 0,
		},
	},
};

static struct exynos_fimc_is_subip_info subip_info = {
	._mcuctl = {
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 161,
		.base_addr	= 0,
	},
	._3a0 = {
		.valid		= 0,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0,
	},
	._3a1 = {
		.valid		= 0,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0,
	},
	._isp = {
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0,
	},
	._drc = {
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0,
	},
	._scc = {
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0,
	},
	._odc = {
		.valid		= 0,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0,
	},
	._dis = {
		.valid		= 0,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0,
	},
	._dnr = {
		.valid		= 0,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0,
	},
	._scp = {
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0,
	},
	._fd = {
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0,
	},
	._pwm = {
		.valid		= 1,
		.full_bypass	= 0,
		.version	= 0,
		.base_addr	= 0x13160000,
	},
};
#endif

#ifdef EXTERNAL_ISP
#ifdef GPIO_EMULATION
static struct i2c_gpio_platform_data i2c10_gpio_data = {
	.sda_pin		= EXYNOS5260_GPF0(0),
	.scl_pin		= EXYNOS5260_GPF0(1),
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0
};

static struct platform_device i2c10_gpio = {
	.name			= "i2c-gpio",
	.id			= 10,
	.dev			= {
		.platform_data	= &i2c10_gpio_data
	}
};
#endif

static struct i2c_board_info sensor0_board_info[] = {
	{
		I2C_BOARD_INFO("S5K4EC", 0xAC>>1)
	}
};

static struct exynos_platform_fimc_is_sensor sensor0 = {
	.scenario		= SENSOR_SCENARIO_EXTERNAL,
	.mclk_ch		= 0,
	.csi_ch			= CSI_ID_A,
	.flite_ch		= FLITE_ID_C,
	.i2c_ch			= SENSOR_CONTROL_I2C0,
	.i2c_addr		= 0x20,
	.is_bns			= 0,
	.flash_first_gpio	= 5,
	.flash_second_gpio	= 8,
};

static struct exynos_platform_fimc_is_sensor sensor1 = {
	.scenario		= SENSOR_SCENARIO_EXTERNAL,
	.mclk_ch		= 1,
	.csi_ch			= CSI_ID_B,
	.flite_ch		= FLITE_ID_B,
	.i2c_ch			= SENSOR_CONTROL_I2C1,
	.i2c_addr		= 0x20,
	.is_bns			= 0,
};
#else
static struct exynos_platform_fimc_is_sensor sensor0 = {
	.scenario		= SENSOR_SCENARIO_NORMAL,
	.mclk_ch		= 0,
	.csi_ch			= CSI_ID_A,
	.flite_ch		= FLITE_ID_A,
	.i2c_ch			= SENSOR_CONTROL_I2C0,
	.i2c_addr		= 0x20,
	.is_bns			= 0,
	.flash_first_gpio	= 5,
	.flash_second_gpio	= 8,
};

static struct exynos_platform_fimc_is_sensor sensor1 = {
	.scenario		= SENSOR_SCENARIO_NORMAL,
	.mclk_ch		= 1,
	.csi_ch			= CSI_ID_B,
	.flite_ch		= FLITE_ID_B,
	.i2c_ch			= SENSOR_CONTROL_I2C1,
	.i2c_addr		= 0x20,
	.is_bns			= 0,
};
#endif

static struct platform_device *camera_devices[] __initdata = {
#if defined(CONFIG_VIDEO_EXYNOS_FIMC_IS)
	&exynos_device_fimc_is_sensor0,
	&exynos_device_fimc_is_sensor1,
	&exynos5_device_fimc_is,
#endif
};

void __init exynos5_xyref5260_camera_init(void)
{
#if defined(CONFIG_VIDEO_EXYNOS_FIMC_IS)
	dev_set_name(&exynos5_device_fimc_is.dev, "s5p-mipi-csis.0");
	clk_add_alias("gscl_wrap0", FIMC_IS_DEV_NAME,
			"gscl_wrap0", &exynos5_device_fimc_is.dev);
	dev_set_name(&exynos5_device_fimc_is.dev, "s5p-mipi-csis.1");
	clk_add_alias("gscl_wrap1", FIMC_IS_DEV_NAME,
			"gscl_wrap1", &exynos5_device_fimc_is.dev);
	dev_set_name(&exynos5_device_fimc_is.dev, FIMC_IS_DEV_NAME);

	exynos_fimc_is_data.subip_info = &subip_info;

	/* DVFS sceanrio setting */
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_DEFAULT,
			400000, 667000, 75000000, 0, 333000, 2666);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_FRONT_PREVIEW,
			400000, 667000, 75000000, 0, 333000, 2666);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_FRONT_CAPTURE,
			400000, 667000, 75000000, 0, 333000, 2666);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_FRONT_CAMCORDING,
			400000, 667000, 75000000, 0, 333000, 2666);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_FRONT_VT1,
			400000, 667000, 75000000, 0, 333000, 2666);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_FRONT_VT2,
			400000, 667000, 75000000, 0, 333000, 2666);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_REAR_PREVIEW_FHD,
			400000, 667000, 75000000, 0, 333000, 2666);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_REAR_PREVIEW_WHD,
			400000, 667000, 75000000, 0, 333000, 2666);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_REAR_PREVIEW_UHD,
			400000, 667000, 75000000, 0, 333000, 2666);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_REAR_CAPTURE,
			533000, 667000, 100000000, 0, 333000, 2000);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_REAR_CAMCORDING_FHD,
			400000, 667000, 75000000, 0, 333000, 2666);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_REAR_CAMCORDING_UHD,
			400000, 667000, 75000000, 0, 333000, 2666);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_DUAL_PREVIEW,
			400000, 667000, 75000000, 0, 333000, 2666);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_DUAL_CAPTURE,
			400000, 667000, 75000000, 0, 333000, 2666);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_DUAL_CAMCORDING,
			400000, 667000, 75000000, 0, 333000, 2666);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_HIGH_SPEED_FPS,
			400000, 667000, 75000000, 0, 333000, 2666);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_DIS_ENABLE,
			400000, 667000, 75000000, 0, 333000, 2666);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_MAX,
			400000, 667000, 75000000, 0, 333000, 2666);

	exynos_fimc_is_data.gate_info = &gate_info;

	exynos_fimc_is_set_platdata(&exynos_fimc_is_data);

	/* sensor0: normal: on */
	SET_PIN(&sensor0, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0,
		EXYNOS5260_GPE1(2), (2 << 8), "GPE1.2 (CAM_MCLK)", 0, PIN_FUNCTION);
	SET_PIN(&sensor0, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1,
		EXYNOS5260_GPE0(5), (2 << 20), "GPE0.5 (CAM_FLASH_EN)", 0, PIN_FUNCTION);
	SET_PIN(&sensor0, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2,
		EXYNOS5260_GPF0(1), (2 << 4), "GPF0.1 (CAM_I2C0_SCL)", 0, PIN_FUNCTION);
	SET_PIN(&sensor0, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3,
		EXYNOS5260_GPF0(0), (2 << 0), "GPE0.0 (CAM_I2C0_SDA)", 0, PIN_FUNCTION);
	SET_PIN(&sensor0, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4,
		0, 0, "main_cam_io_1v8", 0, PIN_REGULATOR_ON);
	SET_PIN(&sensor0, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5,
		0, 0, "main_cam_sensor_a2v8", 0, PIN_REGULATOR_ON);
	SET_PIN(&sensor0, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6,
		0, 0, "main_cam_af_2v8", 0, PIN_REGULATOR_ON);
	SET_PIN(&sensor0, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 7,
		EXYNOS5260_GPE0(6), 1, "GPE0.6 (CAMCORE_EN)", 0, PIN_OUTPUT_HIGH);
	SET_PIN(&sensor0, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 8,
		EXYNOS5260_GPE0(1), 1, "GPE0.1 (MAIN_CAM_RST)", 0, PIN_RESET);
#if defined(CONFIG_EXYNOS5260_XYREF_REV1)
	SET_PIN(&sensor0, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 9,
		EXYNOS5260_GPE1(0), (2 << 0), "GPE1.0 (CAM_FLASH_TORCH)", 0,
		PIN_FUNCTION);
	SET_PIN(&sensor0, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 10,
		0, 0, "", 0, PIN_END);
#else
	SET_PIN(&sensor0, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 9,
		0, 0, "", 0, PIN_END);
#endif

	/* sensor0: normal: off */
	SET_PIN(&sensor0, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0,
		EXYNOS5260_GPE0(1), 1, "GPE0.1 (MAIN_CAM_RST)", 0, PIN_RESET);
	SET_PIN(&sensor0, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1,
		EXYNOS5260_GPE0(1), 1, "GPE0.1 (MAIN_CAM_RST)", 0, PIN_INPUT);
	SET_PIN(&sensor0, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2,
		EXYNOS5260_GPE0(6), 1, "GPE0.6 (CAMCORE_EN)", 0, PIN_OUTPUT_LOW);
	SET_PIN(&sensor0, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3,
		0, 0, "main_cam_io_1v8", 0, PIN_REGULATOR_OFF);
	SET_PIN(&sensor0, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4,
		0, 0, "main_cam_sensor_a2v8", 0, PIN_REGULATOR_OFF);
	SET_PIN(&sensor0, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5,
		0, 0, "main_cam_af_2v8", 0, PIN_REGULATOR_OFF);
#if defined(CONFIG_EXYNOS5260_XYREF_REV1)
	SET_PIN(&sensor0, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6,
		EXYNOS5260_GPE1(0), 1, "GPE1.0 (CAM_FLASH_TORCH)", 0, PIN_INPUT);
	SET_PIN(&sensor0, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 7,
		0, 0, "", 0, PIN_END);
#else
	SET_PIN(&sensor0, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6,
		0, 0, "", 0, PIN_END);
#endif

	/* sensor1: normal: on */
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0,
		EXYNOS5260_GPE0(4), 1, "GPE0.4 (CAM_VT_nRST)", 0, PIN_RESET);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1,
		EXYNOS5260_GPE1(3), (2 << 12), "GPE1.3 (CAM_MCLK)", 0, PIN_FUNCTION);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2,
		EXYNOS5260_GPF0(3), (2 << 12), "GPE0.3 (CAM_I2C1_SCL)", 0, PIN_FUNCTION);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3,
		EXYNOS5260_GPF0(2), (2 << 8), "GPE0.2 (CAM_I2C1_SDA)", 0, PIN_FUNCTION);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4,
		0, 0, "vt_cam_core_1v8", 0, PIN_REGULATOR_ON);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5,
		0, 0, "vt_cam_sensor_a2v8", 0, PIN_REGULATOR_ON);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6,
		0, 0, "", 0, PIN_END);

	/* sensor1: normal: off */
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0,
		EXYNOS5260_GPE0(4), 1, "GPE0.4 (CAM_VT_nRST)", 0, PIN_RESET);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1,
		EXYNOS5260_GPE0(4), 1, "GPE0.4 (CAM_VT_nRST)", 0, PIN_INPUT);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2,
		0, 0, "vt_cam_core_1v8", 0, PIN_REGULATOR_OFF);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3,
		0, 0, "vt_cam_sensor_a2v8", 0, PIN_REGULATOR_OFF);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4,
		0, 0, "", 0, PIN_END);

#if defined(EXTERNAL_ISP)
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 0,
		EXYNOS5260_GPB4(0), (2 << 0), "GPB4.0 (CAM_SDA)", 0, PIN_FUNCTION);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 1,
		EXYNOS5260_GPB4(1), (2 << 4), "GPB4.1 (CAM_SCL)", 0, PIN_FUNCTION);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 2,
		EXYNOS5260_GPE0(1), 1, "GPE0.1 (MAIN_RST_HIGH)", 0, PIN_OUTPUT);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 3,
		EXYNOS5260_GPE0(2), 1, "GPE0.2 (CAM_EN)", 0, PIN_OUTPUT);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 4,
		EXYNOS5260_GPE1(2), (2 << 8), "GPE1.2 (CAM_MCLK)", 0, PIN_FUNCTION);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 5,
		EXYNOS5260_GPE0(5), (2 << 20), "GPE0.5 (CAM_FLASH_EN)", 0, PIN_FUNCTION);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 6,
		0, 1, "main_cam_io_1v8", 0, PIN_REGULATOR);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 7,
		0, 5, "delay (5ms)", 0, PIN_DELAY);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 8,
		0, 1, "main_cam_sensor_a2v8", 0, PIN_REGULATOR);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 9,
		0, 5, "delay (5ms)", 0, PIN_DELAY);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 10,
		0, 1, "main_cam_af_2v8", 0, PIN_REGULATOR);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 11,
		0, 5, "delay (5ms)", 0, PIN_DELAY);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 12,
		EXYNOS5260_GPE0(6), 1, "GPE0.6 (CAMCORE_EN)", 0, PIN_OUTPUT);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 13,
		0, 200, "delay (200ms)", 0, PIN_DELAY);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 14,
		EXYNOS5260_GPE0(1), 1, "GPE0.1 (MAIN_CAM_RST)", 0, PIN_RESET);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 15,
		0, 0, "PIN (END)", 0, PIN_END);

	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, 0,
		EXYNOS5260_GPE0(1), 1, "GPE0.1 (MAIN_CAM_RST)", 0, PIN_RESET);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, 1,
		EXYNOS5260_GPE0(1), 1, "GPE0.1 (MAIN_RST_HIGH)", 0, PIN_OUTPUT);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, 2,
		EXYNOS5260_GPE0(2), 0, "GPE0.2 (CAM_EN)", 0, PIN_OUTPUT);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, 3,
		0, 0, "main_cam_io_1v8", 0, PIN_REGULATOR);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, 4,
		0, 0, "main_cam_sensor_a2v8", 0, PIN_REGULATOR);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, 5,
		0, 0, "main_cam_af_2v8", 0, PIN_REGULATOR);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, 6,
		EXYNOS5260_GPE0(6), 0, "GPE0.6 (CAMCORE_EN)", 0, PIN_OUTPUT);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, 7,
		0, 0, "", 0, PIN_END);

#ifdef GPIO_EMULATION
	i2c_register_board_info(10, sensor0_board_info, ARRAY_SIZE(sensor0_board_info));
	platform_device_register(&i2c10_gpio);
#else
	s3c_i2c0_set_platdata(NULL);
	i2c_register_board_info(4, sensor0_board_info, ARRAY_SIZE(sensor0_board_info));
	platform_device_register(&s3c_device_i2c0);
#endif
#endif

	exynos_fimc_is_sensor_set_platdata(&sensor0, &exynos_device_fimc_is_sensor0);
	exynos_fimc_is_sensor_set_platdata(&sensor1, &exynos_device_fimc_is_sensor1);
#endif

	platform_add_devices(camera_devices, ARRAY_SIZE(camera_devices));
}
