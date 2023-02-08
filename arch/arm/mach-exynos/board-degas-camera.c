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
#include <media/sr352.h>
#include <media/sr130pc20_platform.h>

#include "board-universal222ap.h"
#include "board-degas.h"

extern unsigned int system_rev;

#if defined(CONFIG_VIDEO_EXYNOS_FIMC_LITE)
#if defined(CONFIG_VIDEO_SR352)

int sr352_flash_onoff(int on)
{
	int ret = 0;

	return ret;
}

static struct sr352_platform_data sr352_platdata = {
	.gpio_rst = EXYNOS4_GPX1(2), /* ISP_RESET CAM A port : 2/ CAM B port : 0 */
	.enable_rst = true, /* positive reset */
	.flash_onoff = sr352_flash_onoff,
};

static struct i2c_board_info sr352_board_info = {
	I2C_BOARD_INFO("SR352", 0x40>>1),
	.platform_data = &sr352_platdata,
};

static struct exynos_isp_info sr352 = {
	.board_info	= &sr352_board_info,
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
static struct s3c_platform_camera flite_sr352 = {
	.type		= CAM_TYPE_MIPI,
	.use_isp	= true,
	.inv_pclk	= 1,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,
};
#endif
#if defined(CONFIG_VIDEO_SR130PC20)
static struct sr130pc20_platform_data sr130pc20_platdata = {
	.gpio_rst = EXYNOS4_GPM1(2),
	.enable_rst = true, /* positive reset */
};

static struct i2c_board_info sr130pc20_board_info = {
	I2C_BOARD_INFO("SR130PC20", 0x50>>1),
	.platform_data = &sr130pc20_platdata,
};

static struct exynos_isp_info sr130pc20 = {
	.board_info	= &sr130pc20_board_info,
	.cam_srclk_name	= "xusbxti",
	.clk_frequency  = 24000000UL,
	.bus_type	= CAM_TYPE_MIPI,
	.cam_clk_name	= "sclk_cam1",
	.camif_clk_name	= "camif",
	.i2c_bus_num	= 4,
	.cam_port	= CAM_PORT_B, /* A-Port : 0, B-Port : 1 */
	.flags		= CAM_CLK_INV_PCLK | CAM_CLK_INV_VSYNC,
	.csi_data_align = 32,
};

/* This is for platdata of fimc-lite */
static struct s3c_platform_camera flite_sr130pc20 = {
	.type		= CAM_TYPE_MIPI,
	.use_isp	= true,
	.inv_pclk	= 1,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,
};
#endif

static void __init universal_degas_camera_config(void)
{
	/* MIPI CAM MCLK */
	s3c_gpio_cfgrange_nopull(EXYNOS4_GPM2(2), 1, S3C_GPIO_SFN(3));

	/* MIPI CAM EN */
	s3c_gpio_cfgpin(EXYNOS4_GPM3(4), S3C_GPIO_SFN(1));

}

static void __set_flite_camera_config(struct exynos_platform_flite *data,
					u32 active_index, u32 max_cam)
{
	data->active_cam_index = active_index;
	data->num_clients = max_cam;
}

#if defined(CONFIG_VIDEO_EXYNOS_FIMC_LITE)
static void __init degas_set_camera_platdata(void)
{
#if defined(CONFIG_VIDEO_SR352)
	int flite0_cam_index = 0;
#endif
#if defined(CONFIG_VIDEO_SR130PC20)
	int flite1_cam_index = 0;
#endif

#if defined(CONFIG_VIDEO_SR352)
	exynos_flite0_default_data.cam[flite0_cam_index] = &flite_sr352;
	exynos_flite0_default_data.isp_info[flite0_cam_index] = &sr352;
	flite0_cam_index++;
	/* flite platdata register */
	__set_flite_camera_config(&exynos_flite0_default_data, 0, flite0_cam_index);
#endif
#if defined(CONFIG_VIDEO_SR130PC20)
	exynos_flite1_default_data.cam[flite1_cam_index] = &flite_sr130pc20;
	exynos_flite1_default_data.isp_info[flite1_cam_index] = &sr130pc20;
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

	s3c_i2c6_set_platdata(NULL);
	platform_device_register(&s3c_device_i2c6);

	s3c_i2c4_set_platdata(NULL);
	platform_device_register(&s3c_device_i2c4);

#if defined(CONFIG_VIDEO_EXYNOS_FIMC_LITE)
	universal_degas_camera_config();
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
	degas_set_camera_platdata();
	s3c_set_platdata(&exynos_flite0_default_data,
			sizeof(exynos_flite0_default_data), &exynos_device_flite0);
	s3c_set_platdata(&exynos_flite1_default_data,
			sizeof(exynos_flite1_default_data), &exynos_device_flite1);

	dev_set_name(&exynos_device_flite0.dev, "exynos4-fimc.0");
	clk_add_alias("fimc", "exynos-fimc-lite.0", "fimc",
			&exynos_device_flite0.dev);
	dev_set_name(&exynos_device_flite0.dev, "exynos-fimc-lite.0");

	dev_set_name(&exynos_device_flite1.dev, "exynos4-fimc.1");
	clk_add_alias("fimc", "exynos-fimc-lite.1", "fimc",
			&exynos_device_flite1.dev);
	dev_set_name(&exynos_device_flite1.dev, "exynos-fimc-lite.1");
#endif
}
