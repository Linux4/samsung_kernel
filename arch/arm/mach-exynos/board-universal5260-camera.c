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

#include "board-universal5260.h"
#include <plat/s3c64xx-spi.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <mach/spi-clocks.h>

#include <linux/i2c-gpio.h>
#include <plat/iic.h>
#include <media/m7mu_platform.h>

#include <mach/hs-iic.h>

#if defined(CONFIG_CAMERA_EXTERNAL)
#define EXTERNAL_ISP
#endif

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

#if defined(EXTERNAL_ISP)
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
			qos = 400000;
			break;
		case 5136 * 3424: /* 0x10C5600 */
			qos = 400000;
			break;
		case 5120 * 2880: /* 0xE10000 */
			qos = 400000;
			break;
		case 2880 * 2880: /* 0x7E9000 */
			qos = 400000;
			break;
		case 2582 * 1944: /* 0x4C9710 */
			qos = 267000;
			break;
		case 1920 * 1080: /* 0x1FA400 */
			qos = 267000;
			break;
		default:
			qos = 400000;
			pr_err("capture bandwidth(%X) is undefined(int %d)\n", bandwidth, qos);
			break;
		}
	} else {
		/* PREVIEW OR RECORDING */
		switch (bandwidth) {
		case 1920 * 1080 * 60: /* 0x76A7000 */
			qos = 400000;
			break;
		case 1280 * 720 * 120: /* 0x6978000 */
			qos = 400000;
			break;
		case 1920 * 1080 * 30: /* 0x3B53800 */
			qos = 267000;
			break;
		case 1280 * 720 * 60: /* 0x34BC000 */
			qos = 267000;
			break;
		case 960 * 720 * 60: /* 0x278D000 */
			qos = 160000;
			break;
		case 1280 * 720 * 30: /* 0x1A5E000 */
			qos = 160000;
			break;
		case 960 * 720 * 30: /* 0x13C6800 */
			qos = 133000;
			break;
		case 704 * 576 * 30: /* 0xB9A00 */
			qos = 133000;
			break;
		default:
			qos = 400000;
			pr_err("preview bandwidth(%X) is undefined(int %d)\n", bandwidth, qos);
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
			qos = 413000;
			break;
		case 5136 * 3424: /* 0x10C5600 */
			qos = 413000;
			break;
		case 5120 * 2880: /* 0xE10000 */
			qos = 413000;
			break;
		case 2880 * 2880: /* 0x7E9000 */
			qos = 413000;
			break;
		case 2582 * 1944: /* 0x4C9710 */
			qos = 267000;
			break;
		case 1920 * 1080: /* 0x1FA400 */
			qos = 267000;
			break;
		default:
			qos = 413000;
			pr_err("capture bandwidth(%X) is undefined(mif %d)\n", bandwidth, qos);
			break;
		}
	} else {
		/* PREVIEW OR RECORDING */
		switch (bandwidth) {
		case 1920 * 1080 * 60: /* 0x76A7000 */
			qos = 513000;
			break;
		case 1280 * 720 * 120: /* 0x6978000 */
			qos = 513000;
			break;
		case 1920 * 1080 * 30: /* 0x3B53800 */
			qos = 413000;
			break;
		case 1280 * 720 * 60: /* 0x34BC000 */
			qos = 413000;
			break;
		case 960 * 720 * 60: /* 0x278D000 */
			qos = 267000;
			break;
		case 1280 * 720 * 30: /* 0x1A5E000 */
			qos = 267000;
			break;
		case 960 * 720 * 30: /* 0x13C6800 */
			qos = 200000;
			break;
		case 704 * 576 * 30: /* 0xB9A00 */
			qos = 200000;
			break;
		default:
			qos = 413000;
			pr_err("preview bandwidth(%X) is undefined(mif %d)\n", bandwidth, qos);
			break;
		}
	}

	return qos;
}
#endif //EXTERNAL_ISP


#if defined(EXTERNAL_ISP)

static struct m7mu_platform_data m7mu_platdata = {
	.enable_rst = true, /* positive reset */
};

static struct i2c_board_info m7mu_i2c_info[] __initdata = {
	{
		I2C_BOARD_INFO("M7MU", 0x3F >> 1),
		.platform_data = &m7mu_platdata,
	}
};

static struct i2c_board_info s5k6b2_i2c_info[] __initdata = {
	{
		I2C_BOARD_INFO("S5K6B2", 0x6A>>1)
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
	.scenario		= SENSOR_SCENARIO_NORMAL,
	.mclk_ch		= 1,
	.csi_ch			= CSI_ID_B,
	.flite_ch		= FLITE_ID_B,
	.i2c_ch			= SENSOR_CONTROL_I2C1,
	.i2c_addr		= 0x6a,
	.is_bns			= 0,
};
#else //!EXTERNAL_ISP
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
#endif //EXTERNAL_ISP

#ifdef CONFIG_VIDEO_M7MU_SPI
static struct s3c64xx_spi_csinfo spi1_csi[] = {
	[0] = {
		.line = EXYNOS5260_GPA2(5),
		.set_level = gpio_set_value,
		.fb_delay = 0x02,
	},
};

static struct spi_board_info spi1_board_info[] __initdata = {
	{
		.modalias = "m7mu_spi",
		.platform_data = NULL,
		.max_speed_hz = 20000000,	//10000000,
		.bus_num = 1,
		.chip_select = 0,
		.mode = SPI_MODE_1,
		.controller_data = &spi1_csi[0],
	}
};
#endif //CONFIG_VIDEO_M7MU_SPI

static struct platform_device *camera_devices[] __initdata = {
#if defined(CONFIG_VIDEO_EXYNOS_FIMC_IS)
	&exynos_device_fimc_is_sensor0,
	&exynos_device_fimc_is_sensor1,
	&exynos5_device_fimc_is,
#endif
#if defined(CONFIG_MACH_HL3G) || defined(CONFIG_MACH_HLLTE)
	&exynos5_device_hs_i2c1,
#endif
};

extern unsigned int system_rev;

#if defined(EXTERNAL_ISP)
void __init exynos5_universal5260_camera_init(void)
{
#ifdef CONFIG_VIDEO_M7MU_SPI
	int ret;
#endif

#if defined(CONFIG_VIDEO_EXYNOS_FIMC_IS)
	dev_set_name(&exynos5_device_fimc_is.dev, "s5p-mipi-csis.0");
	clk_add_alias("gscl_wrap0", FIMC_IS_DEV_NAME,
			"gscl_wrap0", &exynos5_device_fimc_is.dev);
	dev_set_name(&exynos5_device_fimc_is.dev, "s5p-mipi-csis.1");
	clk_add_alias("gscl_wrap1", FIMC_IS_DEV_NAME,
			"gscl_wrap1", &exynos5_device_fimc_is.dev);
	dev_set_name(&exynos5_device_fimc_is.dev, FIMC_IS_DEV_NAME);

	exynos_fimc_is_data.subip_info = &subip_info;
	exynos_fimc_is_data.get_int_qos = get_int_qos;
	exynos_fimc_is_data.get_mif_qos = get_mif_qos;
	exynos_fimc_is_data.gate_info = &gate_info;

	exynos_fimc_is_set_platdata(&exynos_fimc_is_data);

	/* sensor1: normal: on */
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0,
		EXYNOS5260_GPD1(0), 1, "vt_cam_sensor_a2v8", PIN_OUTPUT_HIGH);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1,
		EXYNOS5260_GPF0(3), (2 << 12), "GPF0.3 (CAM_I2C1_SCL)", PIN_FUNCTION);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2,
		EXYNOS5260_GPF0(2), (2 << 8), "GPF0.2 (CAM_I2C1_SDA)", PIN_FUNCTION);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3,
		EXYNOS5260_GPE0(3), 1, "vt_cam_core_1v8", PIN_OUTPUT_HIGH);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4,
		0, 5, "delay (5ms)", PIN_DELAY);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5,
		EXYNOS5260_GPE0(4), 1, "GPE0.4 (CAM_VT_nRST)", PIN_OUTPUT_HIGH);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6,
		0, 1, "delay (1ms)", PIN_DELAY);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 7,
		EXYNOS5260_GPE1(3), (2 << 12), "GPE1.3 (CAM_MCLK)", PIN_FUNCTION);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 8,
		0, 0, "", PIN_END);

	/* sensor1: normal: off */
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0,
		EXYNOS5260_GPE1(3), (1 << 12), "GPE1.3 (CAM_MCLK)", PIN_FUNCTION);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1,
		EXYNOS5260_GPE1(3), 0, "GPE1.3 (CAM_MCLK)", PIN_OUTPUT_LOW);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2,
		0, 1, "delay (1ms)", PIN_DELAY);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3,
		EXYNOS5260_GPE0(4), 1, "GPE0.4 (CAM_VT_nRST)", PIN_OUTPUT_LOW);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4,
		EXYNOS5260_GPE0(3), 0, "vt_cam_core_1v8", PIN_OUTPUT_LOW);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5,
		EXYNOS5260_GPD1(0), 0, "vt_cam_sensor_a2v8", PIN_OUTPUT_LOW);
	SET_PIN(&sensor1, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6,
		0, 0, "", PIN_END);

	/* sensor1: vision: on */
	SET_PIN(&sensor1, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 0,
		EXYNOS5260_GPD1(0), 1, "vt_cam_sensor_a2v8", PIN_OUTPUT_HIGH);
	SET_PIN(&sensor1, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 1,
		EXYNOS5260_GPB4(7), (2 << 28), "GPB4.7 (CAM_I2C1_SCL)", PIN_FUNCTION);
	SET_PIN(&sensor1, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 2,
		EXYNOS5260_GPB4(6), (2 << 24), "GPB4.6 (CAM_I2C1_SDA)", PIN_FUNCTION);
	SET_PIN(&sensor1, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 3,
		EXYNOS5260_GPE0(3), 1, "vt_cam_core_1v8", PIN_OUTPUT_HIGH);
	SET_PIN(&sensor1, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 4,
		0, 5, "delay (5ms)", PIN_DELAY);
	SET_PIN(&sensor1, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 5,
		EXYNOS5260_GPE0(4), 1, "GPE0.4 (CAM_VT_nRST)", PIN_OUTPUT_LOW);
	SET_PIN(&sensor1, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 6,
		EXYNOS5260_GPE0(2), 1, "GPE0.4 (CAM_VT_STBY)", PIN_OUTPUT_HIGH);
	SET_PIN(&sensor1, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 7,
		0, 1, "delay (1ms)", PIN_DELAY);
	SET_PIN(&sensor1, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 8,
		EXYNOS5260_GPE1(3), (2 << 12), "GPE1.3 (CAM_MCLK)", PIN_FUNCTION);
	SET_PIN(&sensor1, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 9,
		0, 0, "", PIN_END);

	/* sensor1: vision: off */
	SET_PIN(&sensor1, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 0,
		EXYNOS5260_GPE0(4), 1, "GPE0.4 (CAM_VT_nRST)", PIN_OUTPUT_LOW);
	SET_PIN(&sensor1, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 1,
		EXYNOS5260_GPE1(3), (1 << 12), "GPE1.3 (CAM_MCLK)", PIN_FUNCTION);
	SET_PIN(&sensor1, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 2,
		EXYNOS5260_GPE1(3), 0, "GPE1.3 (CAM_MCLK)", PIN_OUTPUT_LOW);
	SET_PIN(&sensor1, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 3,
		EXYNOS5260_GPE0(2), 1, "GPE0.4 (CAM_VT_STBY)", PIN_OUTPUT_LOW);
	SET_PIN(&sensor1, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 4,
		EXYNOS5260_GPE0(3), 0, "vt_cam_core_1v8", PIN_OUTPUT_LOW);
	SET_PIN(&sensor1, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 5,
		EXYNOS5260_GPD1(0), 0, "vt_cam_sensor_a2v8", PIN_OUTPUT_LOW);
	SET_PIN(&sensor1, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 6,
		0, 0, "", PIN_END);

	/* sensor power on */
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 0,
			EXYNOS5260_GPB3(3), (2 << 12), "GPB3.3 (SCL)", PIN_FUNCTION);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 1,
			EXYNOS5260_GPB3(2), (2 << 8), "GPB3.2 (SDA)", PIN_FUNCTION);
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, 2,
			0, 0, "PIN (END)", PIN_END);

	/* sensor power off */
	SET_PIN(&sensor0, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, 0,
			0, 0, "PIN (END)", PIN_END);

	exynos5_hs_i2c1_set_platdata(NULL);
	i2c_register_board_info(1, m7mu_i2c_info, ARRAY_SIZE(m7mu_i2c_info));
	platform_device_register(&exynos5_device_hs_i2c1);

	s3c_i2c3_set_platdata(NULL);
	i2c_register_board_info(7, s5k6b2_i2c_info, ARRAY_SIZE(s5k6b2_i2c_info));
	platform_device_register(&s3c_device_i2c3);

	exynos_fimc_is_sensor_set_platdata(&sensor0, &exynos_device_fimc_is_sensor0);
	exynos_fimc_is_sensor_set_platdata(&sensor1, &exynos_device_fimc_is_sensor1);
#endif //CONFIG_VIDEO_EXYNOS_FIMC_IS

#ifdef CONFIG_VIDEO_M7MU_SPI
	exynos_spi_clock_setup(&s3c64xx_device_spi1.dev, 1);

	ret = exynos_spi_cfg_cs(spi1_csi[0].line, 1);
	if (!ret) {
		s3c64xx_spi1_set_platdata(&s3c64xx_spi1_pdata,
			EXYNOS_SPI_SRCCLK_SCLK, ARRAY_SIZE(spi1_csi));

	} else
		pr_err(KERN_ERR "Error requesting gpio for SPI-CH1 CS\n");

	ret = spi_register_board_info(spi1_board_info,
						ARRAY_SIZE(spi1_board_info));
	if (ret)
		pr_err("ERR! spi_register_board_info fail (err %d)\n", ret);
	platform_device_register(&s3c64xx_device_spi1);
#endif //CONFIG_VIDEO_M7MU_SPI
	platform_add_devices(camera_devices, ARRAY_SIZE(camera_devices));
}
#else //!EXTERNAL_ISP
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
	SET_QOS(exynos_fimc_is_data.dvfs_data, FIMC_IS_SN_REAR_CAMCORDING,
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
			EXYNOS5260_GPF0(1), (2 << 4), "GPF0.1 (CAM_I2C0_SCL)", PIN_OUTPUT_LOW);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1,
			EXYNOS5260_GPF0(0), (2 << 0), "GPF0.0 (CAM_I2C0_SDA)", PIN_OUTPUT_LOW);
	/* 2. AF 2.8V ON */
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2,
			0, 0, "main_cam_sensor_a2v8", PIN_REGULATOR_ON);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3,
			0, 0, "main_cam_core_1v2", PIN_REGULATOR_ON);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4,
			EXYNOS5260_GPE1(2), (2 << 8), "GPE1.2 (CAM_MCLK)", PIN_FUNCTION);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5,
			EXYNOS5260_GPF0(1), (2 << 4), "GPF0.1 (CAM_I2C0_SCL)", PIN_FUNCTION);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6,
			EXYNOS5260_GPF0(0), (2 << 0), "GPF0.0 (CAM_I2C0_SDA)", PIN_FUNCTION);
	if (system_rev >= 8) {
		SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 7,
				EXYNOS5260_GPE0(3), 1, "GPE0.3 (MAIN_IO_EN)", PIN_OUTPUT_HIGH);
	} else {
		SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 7,
				0, 0, "main_cam_af_2v8", PIN_REGULATOR_ON);
	}
	if (system_rev >= 8) {
		SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 8,
				0, 0, "main_cam_af_2v8", PIN_REGULATOR_ON);
	} else {
		SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 8,
				EXYNOS5260_GPE0(3), 1, "GPE0.3 (MAIN_IO_EN)", PIN_OUTPUT_HIGH);
	}
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 9,
			EXYNOS5260_GPE0(5), 1, "GPE0.5 (MAIN_CAM_RST)", PIN_OUTPUT_HIGH);

	/* for flash gpio control */
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 10,
			EXYNOS5260_GPE0(0), (2 << 0), "GPE0.0 (CAM_FLASH_EN)", PIN_FUNCTION);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 11,
			EXYNOS5260_GPE0(1), (2 << 4), "GPE0.0 (CAM_FLASH_SET)", PIN_FUNCTION);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 12,
			EXYNOS5260_GPF1(0), (2 << 0), "GPIO_CAM_SPI0_SCLK", PIN_FUNCTION);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 13,
			EXYNOS5260_GPF1(1), (1 << 4), "GPIO_CAM_SPI0_SSN", PIN_FUNCTION);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 14,
			EXYNOS5260_GPF1(2), (2 << 8), "GPIO_CAM_SPI0_MISP", PIN_FUNCTION);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 15,
			EXYNOS5260_GPF1(3), (2 << 12), "GPIO_CAM_SPI0_MOSI", PIN_FUNCTION);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 16,
			0, 0, "", PIN_END);

	/* imx175: normal: off */
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0,
			EXYNOS5260_GPE0(5), (1 << 20), "GPE0.5 (MAIN_CAM_RST)", PIN_OUTPUT_LOW);
	if (system_rev >= 8) {
		SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1,
				EXYNOS5260_GPE0(3), 1, "GPE0.3 (MAIN_IO_EN)", PIN_OUTPUT_LOW);
	} else {
		SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1,
				0, 0, "main_cam_af_2v8", PIN_REGULATOR_OFF);
	}
	if (system_rev >= 8) {
		SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2,
				0, 0, "main_cam_af_2v8", PIN_REGULATOR_OFF);
	} else {
		SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2,
				EXYNOS5260_GPE0(3), 1, "GPE0.3 (MAIN_IO_EN)", PIN_OUTPUT_LOW);
	}
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3,
			EXYNOS5260_GPF0(1), (2 << 4), "GPF0.1 (CAM_I2C0_SCL)", PIN_OUTPUT_LOW);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4,
			EXYNOS5260_GPF0(0), (2 << 0), "GPF0.0 (CAM_I2C0_SDA)", PIN_OUTPUT_LOW);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5,
			0, 0, "main_cam_core_1v2", PIN_REGULATOR_OFF);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6,
			0, 0, "main_cam_sensor_a2v8", PIN_REGULATOR_OFF);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 7,
			EXYNOS5260_GPE1(2), (2 << 8), "GPE1.2 (CAM_MCLK)", PIN_OUTPUT_LOW);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 8,
			EXYNOS5260_GPF1(0), (2 << 0), "GPIO_CAM_SPI0_SCLK", PIN_OUTPUT_LOW);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 9,
			EXYNOS5260_GPF1(1), (1 << 4), "GPIO_CAM_SPI0_SSN", PIN_OUTPUT_LOW);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 10,
			EXYNOS5260_GPF1(2), (2 << 8), "GPIO_CAM_SPI0_MISP", PIN_OUTPUT_LOW);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 11,
			EXYNOS5260_GPF1(3), (2 << 12), "GPIO_CAM_SPI0_MOSI", PIN_OUTPUT_LOW);
	SET_PIN(&imx175, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 12,
			0, 0, "", PIN_END);

	/* s5k6b2: normal: on */
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0,
			EXYNOS5260_GPD1(0), 1, "GPD1.0 (MAIN_A2.8)", PIN_OUTPUT_HIGH);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1,
			EXYNOS5260_GPF0(3), (2 << 12), "GPF0.3 (CAM_I2C1_SCL)", PIN_FUNCTION);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2,
			EXYNOS5260_GPF0(2), (2 << 8), "GPF0.2 (CAM_I2C1_SDA)", PIN_FUNCTION);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3,
			0, 0, "vt_cam_core_1v8", PIN_REGULATOR_ON);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4,
			EXYNOS5260_GPE0(4), 1, "GPE0.4 (CAM_VT_nRST)", PIN_OUTPUT_HIGH);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5,
			EXYNOS5260_GPE1(3), (2 << 12), "GPE1.3 (CAM_MCLK)", PIN_FUNCTION);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6,
			0, 0, "", PIN_END);

	/* s5k6b2: normal: off */
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0,
			EXYNOS5260_GPE1(3), (2 << 12), "GPE1.3 (CAM_MCLK)", PIN_OUTPUT_LOW);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1,
			EXYNOS5260_GPE0(4), 0, "GPE0.4 (CAM_VT_nRST)", PIN_OUTPUT_LOW);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2,
			0, 0, "vt_cam_core_1v8", PIN_REGULATOR_OFF);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3,
			EXYNOS5260_GPF0(3), (2 << 12), "GPF0.3 (CAM_I2C1_SCL)", PIN_INPUT);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4,
			EXYNOS5260_GPF0(2), (2 << 8), "GPF0.2 (CAM_I2C1_SDA)", PIN_INPUT);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5,
			EXYNOS5260_GPD1(0), 0, "GPD1.0 (MAIN_A2.8)", PIN_OUTPUT_LOW);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6,
			0, 0, "", PIN_END);

	/* s5k6b2: vision: on */
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 0,
			EXYNOS5260_GPD1(0), 1, "GPD1.0 (MAIN_A2.8)", PIN_OUTPUT_HIGH);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 1,
			EXYNOS5260_GPB3(3), (2 << 12), "GPB3.3 (HS_I2C1_SCL)", PIN_FUNCTION);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 2,
			EXYNOS5260_GPB3(2), (2 << 8), "GPB3.2 (HS_I2C1_SDA)", PIN_FUNCTION);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 3,
			0, 0, "vt_cam_core_1v8", PIN_REGULATOR_ON);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 4,
			EXYNOS5260_GPE0(2), 1, "GPE0.2 (CAM_VT_STBY)", PIN_OUTPUT_HIGH);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 5,
			EXYNOS5260_GPE1(3), (2 << 12), "GPE1.3 (CAM_MCLK)", PIN_FUNCTION);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 6,
			0, 0, "", PIN_END);

	/* s5k6b2: vision: off */
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 0,
			EXYNOS5260_GPE1(3), (2 << 12), "GPE1.3 (CAM_MCLK)", PIN_OUTPUT_LOW);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 1,
			EXYNOS5260_GPE0(2), 0, "GPE0.2 (CAM_VT_STBY)", PIN_OUTPUT_LOW);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 2,
			0, 0, "vt_cam_core_1v8", PIN_REGULATOR_OFF);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 3,
			EXYNOS5260_GPB3(3), (2 << 12), "GPB3.3 (HS_I2C1_SCL)", PIN_INPUT);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 4,
			EXYNOS5260_GPB3(2), (2 << 8), "GPB3.2 (HS_I2C1_SDA)", PIN_INPUT);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 5,
			EXYNOS5260_GPD1(0), 0, "GPD1.0 (MAIN_A2.8)", PIN_OUTPUT_LOW);
	SET_PIN(&s5k6b2, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 6,
			0, 0, "", PIN_END);

	exynos_fimc_is_sensor_set_platdata(&imx175, &exynos_device_fimc_is_sensor0);
	exynos_fimc_is_sensor_set_platdata(&s5k6b2, &exynos_device_fimc_is_sensor1);

	exynos5_hs_i2c1_set_platdata(NULL);
	i2c_register_board_info(1, hs_i2c1_devices, ARRAY_SIZE(hs_i2c1_devices));
#endif //CONFIG_VIDEO_EXYNOS_FIMC_IS

	platform_add_devices(camera_devices, ARRAY_SIZE(camera_devices));
}
#endif //EXTERNAL_ISP

