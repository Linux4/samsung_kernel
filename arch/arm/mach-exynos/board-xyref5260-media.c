/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/clkdev.h>
#include <linux/i2c.h>

#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/fimg2d.h>
#include <plat/jpeg.h>
#include <plat/tv-core.h>
#include <media/exynos_gscaler.h>
#include <mach/hs-iic.h>
#include <mach/exynos-mfc.h>
#include <mach/exynos-scaler.h>
#include <mach/exynos-tv.h>

#include "board-xyref5260.h"

#ifdef CONFIG_ARM_EXYNOS5260_BUS_DEVFREQ
static struct s5p_mfc_qos xyref5260_mfc_qos_table[] = {
	[0] = {
		.thrd_mb	= 0,
		.freq_mfc	= 134000,
		.freq_int	= 100000,
		.freq_mif	= 206000,
#ifdef CONFIG_ARM_EXYNOS_IKS_CPUFREQ
		.freq_cpu	= 0,
#else
		.freq_cpu	= 0,
		.freq_kfc	= 0,
#endif
	},
	[1] = {
		.thrd_mb	= 196420,
		.freq_mfc	= 167000,
		.freq_int	= 160000,
		.freq_mif	= 275000,
#ifdef CONFIG_ARM_EXYNOS_IKS_CPUFREQ
		.freq_cpu	= 0,
#else
		.freq_cpu	= 0,
		.freq_kfc	= 0,
#endif
	},
	[2] = {
		.thrd_mb	= 244800,
		.freq_mfc	= 223000,
		.freq_int	= 266000,
		.freq_mif	= 275000,
#ifdef CONFIG_ARM_EXYNOS_IKS_CPUFREQ
		.freq_cpu	= 400000,
#else
		.freq_cpu	= 0,
		.freq_kfc	= 800000,
#endif
	},
	[3] = {
		.thrd_mb	= 326880,
		.freq_mfc	= 334000,
		.freq_int	= 333000,
		.freq_mif	= 413000,
#ifdef CONFIG_ARM_EXYNOS_IKS_CPUFREQ
		.freq_cpu	= 400000,
#else
		.freq_cpu	= 0,
		.freq_kfc	= 800000,
#endif
	},
};
#endif

static struct s5p_mfc_platdata xyref5260_mfc_pd = {
	.ip_ver		= IP_VER_MFC_6R_0,
	.clock_rate	= 333 * MHZ,
	.min_rate	= 84000, /* in KHz */
#ifdef CONFIG_ARM_EXYNOS5260_BUS_DEVFREQ
	.num_qos_steps	= ARRAY_SIZE(xyref5260_mfc_qos_table),
	.qos_table	= xyref5260_mfc_qos_table,
#endif
};

static struct exynos_scaler_platdata xyref5260_mscl_pd = {
	.platid		= SCID_RH,
	.clk_rate	= 400 * MHZ,
	.gate_clk	= "mscl",
	.clk		= { "aclk_m2m_400", },
	.clksrc		= { "aclk_gscl_400", },
};

struct platform_device exynos_device_md0 = {
	.name = "exynos-mdev",
	.id = 0,
};

struct platform_device exynos_device_md1 = {
	.name = "exynos-mdev",
	.id = 1,
};

static struct platform_device *xyref5260_media_devices[] __initdata = {
	&exynos_device_md0,
	&exynos_device_md1,
	&exynos5_device_gsc0,
	&exynos5_device_gsc1,
	&s5p_device_fimg2d,
#ifdef CONFIG_VIDEO_EXYNOS_MFC
	&s5p_device_mfc,
#endif
	&exynos5_device_jpeg_hx,
	&exynos5_device_scaler0,
	&exynos5_device_scaler1,
	&exynos5_device_hs_i2c2,
	&s5p_device_mixer,
	&s5p_device_hdmi,
	&s5p_device_cec,
};

static struct fimg2d_platdata fimg2d_data __initdata = {
	.ip_ver		= IP_VER_G2D_5R,
	.hw_ver		= 0x43,
	.parent_clkname	= "sclk_mediatop_pll",
	.clkname	= "aclk_g2d_333",
	.gate_clkname	= "fimg2d",
	.clkrate	= 333 * MHZ,
	.cpu_min	= 800000, /* KFC 800MHz */
	.mif_min	= 667000,
	.int_min	= 333000,
};

#ifdef CONFIG_VIDEO_EXYNOS_JPEG_HX
static struct exynos_jpeg_platdata exynos5260_jpeg_hx_pd __initdata = {
	.ip_ver		= IP_VER_JPEG_HX_5R,
	.gateclk	= "jpeg-hx",
};
#endif

static struct s5p_platform_cec hdmi_cec_data __initdata = {
};

static struct s5p_mxr_platdata mxr_platdata __initdata = {
	.ip_ver = IP_VER_TV_5R,
};

static struct s5p_hdmi_platdata hdmi_platdata __initdata = {
	.ip_ver = IP_VER_TV_5R,
};

static struct i2c_board_info hs_i2c_devs2[] __initdata = {
	{
		I2C_BOARD_INFO("exynos_hdcp", (0x74 >> 1)),
	},
	{
		I2C_BOARD_INFO("exynos_edid", (0xA0 >> 1)),
	},
};

struct exynos5_platform_i2c hs_i2c2_data __initdata = {
	.bus_number = 2,
	.operation_mode = HSI2C_INTERRUPT,
	.speed_mode = HSI2C_FAST_SPD,
	.fast_speed = 100000,
	.high_speed = 2500000,
	.cfg_gpio = NULL,
};

void __init exynos5_xyref5260_media_init(void)
{
#ifdef CONFIG_VIDEO_EXYNOS_MFC
	s5p_mfc_set_platdata(&xyref5260_mfc_pd);

	dev_set_name(&s5p_device_mfc.dev, "s3c-mfc");
	clk_add_alias("mfc", "s5p-mfc-v6", "mfc", &s5p_device_mfc.dev);
	s5p_mfc_setname(&s5p_device_mfc, "s5p-mfc-v6");
#endif
	platform_add_devices(xyref5260_media_devices,
			ARRAY_SIZE(xyref5260_media_devices));

	exynos_gsc_set_ip_ver(IP_VER_GSC_5A);
	exynos_gsc_set_pm_qos_val(206000, 100000);

	s3c_set_platdata(&exynos_gsc0_default_data,
		sizeof(exynos_gsc0_default_data), &exynos5_device_gsc0);
	s3c_set_platdata(&exynos_gsc1_default_data,
		sizeof(exynos_gsc1_default_data), &exynos5_device_gsc1);

	s5p_fimg2d_set_platdata(&fimg2d_data);
#ifdef CONFIG_VIDEO_EXYNOS_JPEG_HX
	s3c_set_platdata(&exynos5260_jpeg_hx_pd, sizeof(exynos5260_jpeg_hx_pd),
			&exynos5_device_jpeg_hx);
#endif

	s3c_set_platdata(&xyref5260_mscl_pd, sizeof(xyref5260_mscl_pd),
			&exynos5_device_scaler0);
	s3c_set_platdata(&xyref5260_mscl_pd, sizeof(xyref5260_mscl_pd),
			&exynos5_device_scaler1);

	dev_set_name(&s5p_device_hdmi.dev, "exynos5-hdmi");
	s5p_tv_setup();
	s5p_hdmi_cec_set_platdata(&hdmi_cec_data);
	s3c_set_platdata(&mxr_platdata, sizeof(mxr_platdata), &s5p_device_mixer);
	s5p_hdmi_set_platdata(&hdmi_platdata);
	exynos5_hs_i2c2_set_platdata(&hs_i2c2_data);
	i2c_register_board_info(2, hs_i2c_devs2, ARRAY_SIZE(hs_i2c_devs2));
}
