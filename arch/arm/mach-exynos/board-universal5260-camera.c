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
#include <mach/hs-iic.h>

#include <plat/devs.h>
#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/mipi_csis.h>
#include <media/exynos_camera.h>

#include "board-universal5260.h"

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

static struct exynos_platform_fimc_is_sensor imx175 = {
	.scenario       = SENSOR_SCENARIO_NORMAL,
	.mclk_ch        = 0,
	.csi_ch         = CSI_ID_A,
	.flite_ch       = FLITE_ID_A,
	.i2c_ch         = SENSOR_CONTROL_I2C0,
	.i2c_addr       = 0x18,
	.is_bns         = 0,
	.flash_first_gpio	= 0,
	.flash_second_gpio	= 1,
};

static struct exynos_platform_fimc_is_sensor s5k6b2 = {
	.scenario       = SENSOR_SCENARIO_NORMAL,
	.mclk_ch        = 1,
	.csi_ch         = CSI_ID_B,
	.flite_ch       = FLITE_ID_B,
	.i2c_ch         = SENSOR_CONTROL_I2C1,
	.i2c_addr       = 0x6A,
	.is_bns         = 0,
};

static struct i2c_board_info hs_i2c1_devices[] __initdata = {
	{
		I2C_BOARD_INFO("S5K6B2", 0x6A >> 1),
		.platform_data = NULL,
	},
};

static struct platform_device *camera_devices[] __initdata = {
#if defined(CONFIG_VIDEO_EXYNOS_FIMC_IS)
	&exynos_device_fimc_is_sensor0,
	&exynos_device_fimc_is_sensor1,
	&exynos5_device_fimc_is,

	&exynos5_device_hs_i2c1,
#endif
};

extern unsigned int system_rev;

void __init exynos5_universal5260_camera_init(void)
{
#if defined(CONFIG_VIDEO_EXYNOS_FIMC_IS)
	dev_set_name(&exynos5_device_fimc_is.dev, "s5p-mipi-csis.0");
	clk_add_alias("gscl_wrap0", FIMC_IS_DEV_NAME,
			"gscl_wrap0", &exynos5_device_fimc_is.dev);
	dev_set_name(&exynos5_device_fimc_is.dev, "s5p-mipi-csis.1");
	clk_add_alias("gscl_wrap1", FIMC_IS_DEV_NAME,
			"gscl_wrap1", &exynos5_device_fimc_is.dev);
	dev_set_name(&exynos5_device_fimc_is.dev, FIMC_IS_DEV_NAME);

	/* subip information */
	exynos_fimc_is_data.subip_info = &subip_info;

	/* DVFS sceanrio setting */
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_DEFAULT,
			400000, 667000, 75000000, 0, 333000, 2000);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_FRONT_PREVIEW,
			400000, 667000, 75000000, 0, 333000, 2000);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_FRONT_CAPTURE,
			400000, 667000, 75000000, 0, 333000, 2000);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_FRONT_CAMCORDING,
			400000, 667000, 75000000, 0, 333000, 2000);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_FRONT_VT1,
			400000, 667000, 75000000, 0, 333000, 2000);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_FRONT_VT2,
			400000, 667000, 75000000, 0, 333000, 2000);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_REAR_PREVIEW_FHD,
			400000, 667000, 75000000, 0, 333000, 2000);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_REAR_PREVIEW_WHD,
			400000, 667000, 75000000, 0, 333000, 2000);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_REAR_PREVIEW_UHD,
			400000, 667000, 75000000, 0, 333000, 2000);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_REAR_CAPTURE,
			533000, 667000, 100000000, 0, 333000, 2666);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_REAR_CAMCORDING_FHD,
			400000, 667000, 75000000, 0, 333000, 2000);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_REAR_CAMCORDING_UHD,
			400000, 667000, 75000000, 0, 333000, 2000);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_DUAL_PREVIEW,
			400000, 667000, 75000000, 0, 333000, 2000);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_DUAL_CAPTURE,
			400000, 667000, 75000000, 0, 333000, 2000);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_DUAL_CAMCORDING,
			400000, 667000, 75000000, 0, 333000, 2000);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_HIGH_SPEED_FPS,
			400000, 667000, 75000000, 0, 333000, 2000);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_DIS_ENABLE,
			400000, 667000, 75000000, 0, 333000, 2000);
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_MAX,
			400000, 667000, 75000000, 0, 333000, 2000);

	exynos_fimc_is_data.gate_info = &gate_info;

	exynos_fimc_is_set_platdata(&exynos_fimc_is_data);

	/* imx175: normal: on */
	/*
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0,
			EXYNOS5260_GPE0(5), 0, "GPE0.5 (MAIN_CAM_RST)", PIN_FUNCTION);
	*/
	/* for af power sequence */
	/* 1. SCL & SDA Low */
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0,
			EXYNOS5260_GPF0(1), (2 << 4), "GPF0.1 (CAM_I2C0_SCL)", 0, PIN_OUTPUT_LOW);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1,
			EXYNOS5260_GPF0(0), (2 << 0), "GPF0.0 (CAM_I2C0_SDA)", 0, PIN_OUTPUT_LOW);
	/* 2. AF 2.8V ON */
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2,
			0, 0, "main_cam_sensor_a2v8", 0, PIN_REGULATOR_ON);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3,
			0, 0, "main_cam_core_1v2", 0, PIN_REGULATOR_ON);
	if (system_rev >= 8) {
		SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4,
				EXYNOS5260_GPE0(3), 1, "GPE0.3 (MAIN_IO_EN)", 0, PIN_OUTPUT_HIGH);
	} else {
		SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4,
				0, 0, "main_cam_af_2v8", 0, PIN_REGULATOR_ON);
	}
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5,
			EXYNOS5260_GPE1(2), (2 << 8), "GPE1.2 (CAM_MCLK)", 0, PIN_FUNCTION);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6,
			EXYNOS5260_GPF0(1), (2 << 4), "GPF0.1 (CAM_I2C0_SCL)", 0, PIN_FUNCTION);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 7,
			EXYNOS5260_GPF0(0), (2 << 0), "GPF0.0 (CAM_I2C0_SDA)", 0, PIN_FUNCTION);
	if (system_rev >= 8) {
		SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 8,
				0, 0, "main_cam_af_2v8", 0, PIN_REGULATOR_ON);
	} else {
		SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 8,
				EXYNOS5260_GPE0(3), 1, "GPE0.3 (MAIN_IO_EN)", 0, PIN_OUTPUT_HIGH);
	}
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 9,
			EXYNOS5260_GPE0(5), 1, "GPE0.5 (MAIN_CAM_RST)", 0, PIN_OUTPUT_HIGH);

	/* for flash gpio control */
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 10,
			EXYNOS5260_GPE0(0), (2 << 0), "GPE0.0 (CAM_FLASH_EN)", 0, PIN_FUNCTION);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 11,
			EXYNOS5260_GPE0(1), (2 << 4), "GPE0.0 (CAM_FLASH_SET)", 0, PIN_FUNCTION);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 12,
			EXYNOS5260_GPF1(0), (2 << 0), "GPIO_CAM_SPI0_SCLK", 0, PIN_FUNCTION);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 13,
			EXYNOS5260_GPF1(1), (1 << 4), "GPIO_CAM_SPI0_SSN", 0, PIN_FUNCTION);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 14,
			EXYNOS5260_GPF1(2), (2 << 8), "GPIO_CAM_SPI0_MISP", 0, PIN_FUNCTION);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 15,
			EXYNOS5260_GPF1(3), (2 << 12), "GPIO_CAM_SPI0_MOSI", 0, PIN_FUNCTION);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 16,
			0, 0, "", 0, PIN_END);

	/* imx175: normal: off */
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0,
			EXYNOS5260_GPE0(5), (1 << 20), "GPE0.5 (MAIN_CAM_RST)", 0, PIN_OUTPUT_LOW);
	if (system_rev >= 8) {
		SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1,
				EXYNOS5260_GPE0(3), 1, "GPE0.3 (MAIN_IO_EN)", 0, PIN_OUTPUT_LOW);
	} else {
		SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1,
				0, 0, "main_cam_af_2v8", 0, PIN_REGULATOR_OFF);
	}
	if (system_rev >= 8) {
		SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2,
				0, 0, "main_cam_af_2v8", 0, PIN_REGULATOR_OFF);
	} else {
		SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2,
				EXYNOS5260_GPE0(3), 1, "GPE0.3 (MAIN_IO_EN)", 0, PIN_OUTPUT_LOW);
	}
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3,
			EXYNOS5260_GPF0(1), (2 << 4), "GPF0.1 (CAM_I2C0_SCL)", 0, PIN_OUTPUT_LOW);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4,
			EXYNOS5260_GPF0(0), (2 << 0), "GPF0.0 (CAM_I2C0_SDA)", 0, PIN_OUTPUT_LOW);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5,
			0, 0, "main_cam_core_1v2", 0, PIN_REGULATOR_OFF);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6,
			0, 0, "main_cam_sensor_a2v8", 0, PIN_REGULATOR_OFF);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 7,
			EXYNOS5260_GPE1(2), (2 << 8), "GPE1.2 (CAM_MCLK)", 0, PIN_OUTPUT_LOW);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 8,
			0, 0, "", 0, PIN_END);

	/* s5k6b2: normal: on */
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0,
			EXYNOS5260_GPD1(0), 1, "GPD1.0 (MAIN_A2.8)", 0, PIN_OUTPUT_HIGH);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1,
			EXYNOS5260_GPF0(3), (2 << 12), "GPF0.3 (CAM_I2C1_SCL)", 0, PIN_FUNCTION);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2,
			EXYNOS5260_GPF0(2), (2 << 8), "GPF0.2 (CAM_I2C1_SDA)", 0, PIN_FUNCTION);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3,
			0, 0, "vt_cam_core_1v8", 0, PIN_REGULATOR_ON);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4,
			EXYNOS5260_GPE0(4), 1, "GPE0.4 (CAM_VT_nRST)", 0, PIN_OUTPUT_HIGH);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5,
			EXYNOS5260_GPE1(3), (2 << 12), "GPE1.3 (CAM_MCLK)", 0, PIN_FUNCTION);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6,
			0, 0, "", 0, PIN_END);

	/* s5k6b2: normal: off */
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0,
			EXYNOS5260_GPE1(3), (2 << 12), "GPE1.3 (CAM_MCLK)", 0, PIN_OUTPUT_LOW);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1,
			EXYNOS5260_GPE0(4), 0, "GPE0.4 (CAM_VT_nRST)", 0, PIN_OUTPUT_LOW);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2,
			0, 0, "vt_cam_core_1v8", 0, PIN_REGULATOR_OFF);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3,
			EXYNOS5260_GPF0(3), (2 << 12), "GPF0.3 (CAM_I2C1_SCL)", 0, PIN_INPUT);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4,
			EXYNOS5260_GPF0(2), (2 << 8), "GPF0.2 (CAM_I2C1_SDA)", 0, PIN_INPUT);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5,
			EXYNOS5260_GPD1(0), 0, "GPD1.0 (MAIN_A2.8)", 0, PIN_OUTPUT_LOW);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6,
			0, 0, "", 0, PIN_END);

	/* s5k6b2: vision: on */
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 0,
			EXYNOS5260_GPD1(0), 1, "GPD1.0 (MAIN_A2.8)", 0, PIN_OUTPUT_HIGH);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 1,
			EXYNOS5260_GPB3(3), (2 << 12), "GPB3.3 (HS_I2C1_SCL)", 0, PIN_FUNCTION);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 2,
			EXYNOS5260_GPB3(2), (2 << 8), "GPB3.2 (HS_I2C1_SDA)", 0, PIN_FUNCTION);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 3,
			0, 0, "vt_cam_core_1v8", 0, PIN_REGULATOR_ON);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 4,
			EXYNOS5260_GPE0(2), 1, "GPE0.2 (CAM_VT_STBY)", 0, PIN_OUTPUT_HIGH);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 5,
			EXYNOS5260_GPE1(3), (2 << 12), "GPE1.3 (CAM_MCLK)", 0, PIN_FUNCTION);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 6,
			0, 0, "", 0, PIN_END);

	/* s5k6b2: vision: off */
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 0,
			EXYNOS5260_GPE1(3), (2 << 12), "GPE1.3 (CAM_MCLK)", 0, PIN_OUTPUT_LOW);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 1,
			EXYNOS5260_GPE0(2), 0, "GPE0.2 (CAM_VT_STBY)", 0, PIN_OUTPUT_LOW);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 2,
			0, 0, "vt_cam_core_1v8", 0, PIN_REGULATOR_OFF);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 3,
			EXYNOS5260_GPB3(3), (2 << 12), "GPB3.3 (HS_I2C1_SCL)", 0, PIN_INPUT);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 4,
			EXYNOS5260_GPB3(2), (2 << 8), "GPB3.2 (HS_I2C1_SDA)", 0, PIN_INPUT);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 5,
			EXYNOS5260_GPD1(0), 0, "GPD1.0 (MAIN_A2.8)", 0, PIN_OUTPUT_LOW);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 6,
			0, 0, "", 0, PIN_END);

	exynos_fimc_is_sensor_set_platdata(&imx175, &exynos_device_fimc_is_sensor0);
	exynos_fimc_is_sensor_set_platdata(&s5k6b2, &exynos_device_fimc_is_sensor1);

	exynos5_hs_i2c1_set_platdata(NULL);
	i2c_register_board_info(1, hs_i2c1_devices, ARRAY_SIZE(hs_i2c1_devices));
#endif

	platform_add_devices(camera_devices, ARRAY_SIZE(camera_devices));
}
