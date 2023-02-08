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
#include <linux/regulator/fixed.h>

#include <media/exynos_flite.h>
#include <mach/exynos-fimc-is.h>
#include <media/s5p_fimc.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/mipi_csis.h>
#include <plat/iic.h>
#include <media/exynos_camera.h>
#include <media/s5k4ecgx.h>
#include <media/sr030pc50_platform.h>

#include "board-universal222ap.h"
#include "board-garda.h"

extern unsigned int system_rev;

#define GPIO_CAM_FLASH_EN	EXYNOS4_GPM3(4)
#define GPIO_CAM_FLASH_SET	EXYNOS4_GPM3(6)

#if defined(CONFIG_VIDEO_EXYNOS_FIMC_LITE)
#if defined(CONFIG_VIDEO_S5K4ECGX)

int s5k4ecgx_flash_onoff(int on)
{
	int ret = 0;

	ret = gpio_direction_output(GPIO_CAM_FLASH_EN, on ? true : false);
	if (ret)
		return ret;

	ret = gpio_direction_output(GPIO_CAM_FLASH_SET, on ? true : false);
	if (ret) {
		gpio_direction_output(GPIO_CAM_FLASH_EN, false);
		return ret;
	}

	return ret;

}

static struct s5k4ecgx_platform_data s5k4ecgx_platdata = {
	.gpio_rst = EXYNOS4_GPX1(2), /* ISP_RESET CAM A port : 2/ CAM B port : 0 */
	.enable_rst = true, /* positive reset */
	.flash_onoff = s5k4ecgx_flash_onoff,
};

static struct i2c_board_info s5k4ecgx_board_info = {
	I2C_BOARD_INFO("S5K4ECGX", 0xAC>>1),
	.platform_data = &s5k4ecgx_platdata,
};

static struct exynos_isp_info s5k4ecgx = {
	.board_info	= &s5k4ecgx_board_info,
	.cam_srclk_name	= "xusbxti",
	.clk_frequency  = 24000000UL,
	.bus_type	= CAM_TYPE_MIPI,
	.cam_clk_name	= "sclk_cam1",
	.camif_clk_name	= "camif",
	.i2c_bus_num	= 6,
	.cam_port	= CAM_PORT_A, /* A-Port : 0, B-Port : 1 */
	.flags		= CAM_CLK_INV_PCLK | CAM_CLK_INV_VSYNC,
	.csi_data_align = 32,
};

/* This is for platdata of fimc-lite */
static struct s3c_platform_camera flite_s5k4ecgx = {
	.type		= CAM_TYPE_MIPI,
	.use_isp	= true,
	.inv_pclk	= 1,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,
};
#endif
#if defined(CONFIG_VIDEO_SR030PC50)
static struct sr030pc50_platform_data sr030pc50_platdata = {
	.gpio_rst = EXYNOS4_GPX1(0), /* ISP_RESET CAM A port : 2/ CAM B port : 0 */
	.enable_rst = true, /* positive reset */
};

static struct i2c_board_info sr030pc50_board_info = {
	I2C_BOARD_INFO("SR030PC50", 0x60>>1),
	.platform_data = &sr030pc50_platdata,
};

static struct exynos_isp_info sr030pc50 = {
	.board_info	= &sr030pc50_board_info,
	.cam_srclk_name	= "xusbxti",
	.clk_frequency  = 24000000UL,
	.bus_type	= CAM_TYPE_MIPI,
	.cam_clk_name	= "sclk_cam1",
	.camif_clk_name	= "camif",
	.i2c_bus_num	= 6,
	.cam_port	= CAM_PORT_B, /* A-Port : 0, B-Port : 1 */
	.flags		= CAM_CLK_INV_PCLK | CAM_CLK_INV_VSYNC,
	.csi_data_align = 32,
};

/* This is for platdata of fimc-lite */
static struct s3c_platform_camera flite_sr030pc50 = {
	.type		= CAM_TYPE_MIPI,
	.use_isp	= true,
	.inv_pclk	= 1,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,
};
#endif

static void __init universal_garda_camera_config(void)
{
	/* MIPI CAM MCLK */
	s3c_gpio_cfgrange_nopull(EXYNOS4_GPM2(2), 1, S3C_GPIO_SFN(3));

	/* MIPI CAM EN */
	s3c_gpio_cfgpin(EXYNOS4_GPM4(4), S3C_GPIO_SFN(1));

	/* CAM FLASH */
	s3c_gpio_cfgpin(GPIO_CAM_FLASH_EN, S3C_GPIO_SFN(1));
	s3c_gpio_cfgpin(GPIO_CAM_FLASH_SET, S3C_GPIO_SFN(1));
}

static void __set_flite_camera_config(struct exynos_platform_flite *data,
					u32 active_index, u32 max_cam)
{
	data->active_cam_index = active_index;
	data->num_clients = max_cam;
}

#if defined(CONFIG_VIDEO_EXYNOS_FIMC_LITE)
static void __init garda_set_camera_platdata(void)
{
#if defined(CONFIG_VIDEO_S5K4ECGX)
	int flite0_cam_index = 0;
#endif
#if defined(CONFIG_VIDEO_SR030PC50)
	int flite1_cam_index = 0;
#endif

#if defined(CONFIG_VIDEO_S5K4ECGX)
	exynos_flite0_default_data.cam[flite0_cam_index] = &flite_s5k4ecgx;
	exynos_flite0_default_data.isp_info[flite0_cam_index] = &s5k4ecgx;
	flite0_cam_index++;
	/* flite platdata register */
	__set_flite_camera_config(&exynos_flite0_default_data, 0, flite0_cam_index);
#endif
#if defined(CONFIG_VIDEO_SR030PC50)
	exynos_flite1_default_data.cam[flite1_cam_index] = &flite_sr030pc50;
	exynos_flite1_default_data.isp_info[flite1_cam_index] = &sr030pc50;
	flite1_cam_index++;
	/* flite platdata register */
	__set_flite_camera_config(&exynos_flite1_default_data, 0, flite1_cam_index);
#endif
}
#endif /* CONFIG_VIDEO_EXYNOS_FIMC_LITE */

struct platform_device exynos_device_md1 = {
	.name = "exynos-mdev",
	.id = 1,
};
#endif

static struct platform_device *camera_devices[] __initdata = {
#if defined(CONFIG_VIDEO_EXYNOS_FIMC_LITE)
	&exynos_device_md1,
	&exynos_device_flite0,
	&exynos_device_flite1,

	&s5p_device_mipi_csis0,
	&s5p_device_mipi_csis1,
#endif /* CONFIG_VIDEO_EXYNOS_FIMC_LITE */
};

void __init exynos4_universal222ap_camera_init(void)
{
	platform_add_devices(camera_devices, ARRAY_SIZE(camera_devices));

	if (system_rev < GARDA_REV_06) {
#if defined(CONFIG_VIDEO_S5K4ECGX)
		s5k4ecgx.i2c_bus_num = 5;
#endif
#if defined(CONFIG_VIDEO_SR030PC50)
		sr030pc50.i2c_bus_num = 5;
#endif
	} else {
		s3c_i2c6_set_platdata(NULL);
		platform_device_register(&s3c_device_i2c6);
	}
#if defined(CONFIG_VIDEO_EXYNOS_FIMC_LITE)
	universal_garda_camera_config();
#if defined(CONFIG_EXYNOS_DEV_PD)
	s5p_device_mipi_csis0.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_mipi_csis1.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#endif
	dev_set_name(&s5p_device_mipi_csis0.dev, "s5p-mipi-csis.0");
	dev_set_name(&s5p_device_mipi_csis1.dev, "s5p-mipi-csis.1");

	s5p_mipi_csis0_default_data.fixed_phy_vdd = true;
	s5p_mipi_csis0_default_data.clk_rate = 266000000;
	s3c_set_platdata(&s5p_mipi_csis0_default_data,
			sizeof(s5p_mipi_csis0_default_data),
			&s5p_device_mipi_csis0);
	s5p_mipi_csis1_default_data.fixed_phy_vdd = true;
	s5p_mipi_csis1_default_data.clk_rate = 266000000;
	s3c_set_platdata(&s5p_mipi_csis1_default_data,
			sizeof(s5p_mipi_csis1_default_data),
			&s5p_device_mipi_csis1);

#if defined(CONFIG_EXYNOS_DEV_PD)
	exynos_device_flite0.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	exynos_device_flite1.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#endif
	garda_set_camera_platdata();
	s3c_set_platdata(&exynos_flite0_default_data,
			sizeof(exynos_flite0_default_data), &exynos_device_flite0);
	s3c_set_platdata(&exynos_flite1_default_data,
			sizeof(exynos_flite1_default_data), &exynos_device_flite1);

	dev_set_name(&exynos_device_flite0.dev, "exynos4-fimc.0");
	clk_add_alias("fimc", "exynos-fimc-lite.0", "fimc",
			&exynos_device_flite0.dev);
	dev_set_name(&exynos_device_flite0.dev, "exynos-fimc-lite.0");

	dev_set_name(&exynos_device_flite1.dev, "exynos4-fimc.0");
	clk_add_alias("fimc", "exynos-fimc-lite.1", "fimc",
			&exynos_device_flite1.dev);
	dev_set_name(&exynos_device_flite1.dev, "exynos-fimc-lite.1");
#endif
}
