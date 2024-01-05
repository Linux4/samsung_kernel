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

#include "board-universal3472.h"

#include <linux/i2c-gpio.h>
#include <plat/iic.h>

#define EXTERNAL_ISP
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
	.sda_pin		= EXYNOS3_GPM4(1),
	.scl_pin		= EXYNOS3_GPM4(0),
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

static struct i2c_board_info sensor_board_info[] = {
	{
		I2C_BOARD_INFO("SR352", 0x40>>1)
	}, {
		I2C_BOARD_INFO("SR030", 0x50>>1)
	}
};

static struct exynos_platform_fimc_is_sensor sensor0 = {
	.scenario		= SENSOR_SCENARIO_EXTERNAL,
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
	&exynos3_device_fimc_is_sensor0,
	&exynos3_device_fimc_is_sensor1,
	&exynos3_device_fimc_is,
#endif
};

static int get_int_qos(int scenario)
{
	u32 qos = 0;
	u32 bandwidth;
	u32 mode;

	bandwidth = scenario;
	mode = bandwidth & 0xF0000000;
	bandwidth &= 0x0FFFFFFF;

	if (mode) {
		/* CAPTURE */
		switch (bandwidth) {
		case 5152 * 3864: /* 0x12FB3E8 */
		case 5136 * 3424: /* 0x10C5600 */
		case 5120 * 2880: /* 0xE10000 */
		case 2880 * 2880: /* 0x7E9000 */
		case 2582 * 1944: /* 0x4C9710 */
		case 1920 * 1080: /* 0x1FA400 */
		default:
			qos = 400000;
			break;
		}
	} else {
		/* PREVIEW OR RECORDING */
		switch (bandwidth) {
		case 1920 * 1080 * 60: /* 0x76A7000 */
		case 1280 * 720 * 120: /* 0x6978000 */
		case 1920 * 1080 * 30: /* 0x3B53800 */
		case 1280 * 720 * 60: /* 0x34BC000 */
		case 960 * 720 * 60: /* 0x278D000 */
		case 1280 * 720 * 30: /* 0x1A5E000 */
		case 960 * 720 * 30: /* 0x13C6800 */
		case 704 * 576 * 30: /* 0xB9A00 */
		default:
			qos = 400000;
			break;
		}
	}

	return qos;
}

static int get_mif_qos(int scenario)
{
	u32 qos = 0;
	u32 bandwidth;
	u32 mode;

	bandwidth = scenario;
	mode = bandwidth & 0xF0000000;
	bandwidth &= 0x0FFFFFFF;

	if (mode) {
		/* CAPTURE */
		switch (bandwidth) {
		case 5152 * 3864: /* 0x12FB3E8 */
		case 5136 * 3424: /* 0x10C5600 */
		case 5120 * 2880: /* 0xE10000 */
		case 2880 * 2880: /* 0x7E9000 */
		case 2582 * 1944: /* 0x4C9710 */
		case 1920 * 1080: /* 0x1FA400 */
		default:
			qos = 533000;
			break;
		}
	} else {
		/* PREVIEW OR RECORDING */
		switch (bandwidth) {
		case 1920 * 1080 * 60: /* 0x76A7000 */
		case 1280 * 720 * 120: /* 0x6978000 */
		case 1920 * 1080 * 30: /* 0x3B53800 */
		case 1280 * 720 * 60: /* 0x34BC000 */
		case 960 * 720 * 60: /* 0x278D000 */
		case 1280 * 720 * 30: /* 0x1A5E000 */
		case 960 * 720 * 30: /* 0x13C6800 */
		case 704 * 576 * 30: /* 0xB9A00 */
		default:
			qos = 533000;
			break;
		}
	}

	return qos;
}

void __init exynos3_universal3472_camera_init(void)
{
#if defined(CONFIG_VIDEO_EXYNOS_FIMC_IS)
	dev_set_name(&exynos3_device_fimc_is.dev, "s5p-mipi-csis.0");
	clk_add_alias("gscl_wrap0", FIMC_IS_DEV_NAME,
			"gscl_wrap0", &exynos3_device_fimc_is.dev);
	dev_set_name(&exynos3_device_fimc_is.dev, "s5p-mipi-csis.1");
	clk_add_alias("gscl_wrap1", FIMC_IS_DEV_NAME,
			"gscl_wrap1", &exynos3_device_fimc_is.dev);
	dev_set_name(&exynos3_device_fimc_is.dev, FIMC_IS_DEV_NAME);

	exynos_fimc_is_data.subip_info = &subip_info;

	/* DVFS sceanrio setting */
	exynos_fimc_is_data.get_int_qos = get_int_qos;
	exynos_fimc_is_data.get_mif_qos = get_mif_qos;

	exynos_fimc_is_data.gate_info = &gate_info;

	exynos_fimc_is_set_platdata(&exynos_fimc_is_data);

	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 0,
		EXYNOS3_GPD0(2), (3 << 8), "GPD0.2 (CAM_SDA)", 0, PIN_FUNCTION);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 1,
		EXYNOS3_GPD0(3), (3 << 12), "GPD0.3 (CAM_SCL)", 0, PIN_FUNCTION);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 2,
		EXYNOS3_GPM2(2), (0 << 8), "GPM2.2 (CAM_MCLK_OFF)", 0, PIN_FUNCTION);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 3,
		EXYNOS3_GPE1(5), 0, "GPE1.5 (MAIN_RST_LOW)", 0, PIN_OUTPUT);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 4,
		EXYNOS3_GPE1(4), 0, "GPE1.4 (STANDBY_LOW)", 0, PIN_OUTPUT);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 5,
		0, 1, "cam_io_1v8", 0, PIN_REGULATOR);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 6,
		0, 1, "delay (1ms)", 0, PIN_DELAY);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 7,
		0, 1, "cam_avdd_2v8", 0, PIN_REGULATOR);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 8,
		0, 1, "delay (1ms)", 0, PIN_DELAY);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 9,
		0, 1, "3m_core_1v2", 0, PIN_REGULATOR);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 10,
		0, 5, "delay (5ms)", 0, PIN_DELAY);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 11,
		EXYNOS3_GPM2(2), (3 << 8), "GPM2.2 (CAM_MCLK_ON)", 0, PIN_FUNCTION);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 12,
		0, 2, "delay (2ms)", 0, PIN_DELAY);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 13,
		EXYNOS3_GPE1(4), 1, "GPE1.4 (STANDBY_HIGH)", 0, PIN_OUTPUT);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 14,
		0, 10, "delay (10ms)", 0, PIN_DELAY);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 15,
		EXYNOS3_GPE1(5), 1, "GPE1.5 (MAIN_RST_HIGH)", 0, PIN_OUTPUT);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 16,
		0, 0, "PIN (END)", 0, PIN_END);

	/* sr030 sensor on */
	SET_PIN(&sensor1, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 0,
		EXYNOS3_GPD0(2), (3 << 8), "GPD0.2 (CAM_SDA)", 0, PIN_FUNCTION);
	SET_PIN(&sensor1, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 1,
		EXYNOS3_GPD0(3), (3 << 12), "GPD0.3 (CAM_SCL)", 0, PIN_FUNCTION);
	SET_PIN(&sensor1, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 2,
		EXYNOS3_GPM2(2), 0, "GPM2.2 (CAM_MCLK_OFF)", 0, PIN_OUTPUT);
	SET_PIN(&sensor1, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 3,
		EXYNOS3_GPE1(2), 0, "GPE1.2 (VT_RST_LOW)", 0, PIN_OUTPUT);
	SET_PIN(&sensor1, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 4,
		EXYNOS3_GPE1(3), 0, "GPE1.3 (STANDBY_LOW)", 0, PIN_OUTPUT);
	SET_PIN(&sensor1, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 5,
		0, 1, "cam_io_1v8", 0, PIN_REGULATOR);
	SET_PIN(&sensor1, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 6,
		0, 1, "cam_avdd_2v8", 0, PIN_REGULATOR);
	SET_PIN(&sensor1, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 7,
		0, 1, "vt_cam_1v8", 0, PIN_REGULATOR);
	SET_PIN(&sensor1, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 8,
		0, 5, "delay (5ms)", 0, PIN_DELAY);
	SET_PIN(&sensor1, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 9,
		EXYNOS3_GPM2(2), (3 << 8), "GPM2.2 (CAM_MCLK_ON)", 0, PIN_FUNCTION);
	SET_PIN(&sensor1, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 10,
		0, 2, "delay (2ms)", 0, PIN_DELAY);
	SET_PIN(&sensor1, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 11,
		EXYNOS3_GPE1(3), 1, "GPE1.3 (STANDBY_HIGH)", 0, PIN_OUTPUT);
	SET_PIN(&sensor1, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 12,
		0, 30, "delay (30ms)", 0, PIN_DELAY);
	SET_PIN(&sensor1, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 13,
		EXYNOS3_GPE1(2), 1, "GPE1.2 (VT_RST_HIGH)", 0, PIN_OUTPUT);
	SET_PIN(&sensor1, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 14,
		0, 0, "PIN (END)", 0, PIN_END);


	/* sr352 sensor off */
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, 0,
		EXYNOS3_GPE1(5), 0, "GPE1.5 (MAIN_CAM_RST)", 0, PIN_OUTPUT);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, 1,
		EXYNOS3_GPE1(5), 1, "GPM1.5 (MAIN_RST_HIGH)", 0, PIN_OUTPUT);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, 2,
		0, 0, "cam_io_1v8", 0, PIN_REGULATOR);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, 3,
		0, 0, "cam_avdd_2v8", 0, PIN_REGULATOR);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, 4,
		0, 0, "3m_core_1v2", 0, PIN_REGULATOR);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, 5,
		0, 0, "PIN (END)", 0, PIN_END);

	/* sr030 sensor off */
	SET_PIN(&sensor1, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, 0,
		EXYNOS3_GPE1(2), 0, "GPE1.2 (VT_RST_LOW)", 0, PIN_OUTPUT);
	SET_PIN(&sensor1, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, 1,
		EXYNOS3_GPE1(3), 0, "GPE1.3 (STANDBY_LOW)", 0, PIN_OUTPUT);
	SET_PIN(&sensor1, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, 2,
		0, 0, "vt_cam_1v8", 0, PIN_REGULATOR);
	SET_PIN(&sensor1, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, 3,
		0, 0, "cam_avdd_2v8", 0, PIN_REGULATOR);
	SET_PIN(&sensor1, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, 4,
		0, 0, "cam_io_1v8", 0, PIN_REGULATOR);
	SET_PIN(&sensor1, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, 5,
		0, 0, "PIN (END)", 0, PIN_END);

#ifdef GPIO_EMULATION
	i2c_register_board_info(10, sensor0_board_info, ARRAY_SIZE(sensor0_board_info));
	platform_device_register(&i2c10_gpio);
#else
	s3c_i2c7_set_platdata(NULL);
	i2c_register_board_info(7, sensor_board_info, ARRAY_SIZE(sensor_board_info));
	platform_device_register(&s3c_device_i2c7);
#endif

	exynos_fimc_is_sensor_set_platdata(&sensor0, &exynos3_device_fimc_is_sensor0);
	exynos_fimc_is_sensor_set_platdata(&sensor1, &exynos3_device_fimc_is_sensor1);
#endif

	platform_add_devices(camera_devices, ARRAY_SIZE(camera_devices));
}
