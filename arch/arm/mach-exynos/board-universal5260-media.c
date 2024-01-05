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

#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/fimg2d.h>
#include <plat/jpeg.h>
#include <media/exynos_gscaler.h>
#include <mach/exynos-mfc.h>
#include <mach/exynos-scaler.h>

#include <linux/spi/spi.h>
#include <plat/s3c64xx-spi.h>
#include <mach/spi-clocks.h>
#include "board-universal5260.h"

#ifdef CONFIG_ARM_EXYNOS5260_BUS_DEVFREQ
static struct s5p_mfc_qos universal5260_mfc_qos_table[] = {
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

static struct s5p_mfc_platdata universal5260_mfc_pd = {
	.ip_ver		= IP_VER_MFC_6R_0,
	.clock_rate	= 333 * MHZ,
	.min_rate	= 84000, /* in KHz */
#ifdef CONFIG_ARM_EXYNOS5260_BUS_DEVFREQ
	.num_qos_steps	= ARRAY_SIZE(universal5260_mfc_qos_table),
	.qos_table	= universal5260_mfc_qos_table,
#endif
};

static struct exynos_scaler_platdata universal5260_mscl_pd = {
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

static struct s3c64xx_spi_csinfo spi3_csi[] = {
	[0] = {
		.line           = EXYNOS5260_GPF1(1),
		.set_level      = gpio_set_value,
		.fb_delay       = 0x2,
	},
};

static struct spi_board_info spi3_board_info[] __initdata = {
	{
		.modalias               = "fimc_is_spi",
		.platform_data          = NULL,
		.max_speed_hz           = 50 * 1000 * 1000,
		.bus_num                = 3,
		.chip_select            = 0,
		.mode                   = SPI_MODE_0,
		.controller_data        = &spi3_csi[0],
	}
};

static struct platform_device *universal5260_media_devices[] __initdata = {
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
	&s3c64xx_device_spi3,
};

static struct fimg2d_platdata fimg2d_data __initdata = {
	.ip_ver		= IP_VER_G2D_5R,
	.hw_ver		= 0x43,
	.parent_clkname	= "sclk_mediatop_pll",
	.clkname	= "aclk_g2d_333",
	.gate_clkname	= "fimg2d",
	.clkrate	= 333 * MHZ,
	.cpu_min	= 400000, /* KFC 800MHz */
	.mif_min	= 667000,
	.int_min	= 333000,
};

#ifdef CONFIG_VIDEO_EXYNOS_JPEG_HX
static struct exynos_jpeg_platdata exynos5260_jpeg_hx_pd __initdata = {
	.ip_ver		= IP_VER_JPEG_HX_5R,
	.gateclk	= "jpeg-hx",
};
#endif

void __init exynos5_universal5260_media_init(void)
{
#ifdef CONFIG_VIDEO_EXYNOS_MFC
	s5p_mfc_set_platdata(&universal5260_mfc_pd);

	dev_set_name(&s5p_device_mfc.dev, "s3c-mfc");
	clk_add_alias("mfc", "s5p-mfc-v6", "mfc", &s5p_device_mfc.dev);
	s5p_mfc_setname(&s5p_device_mfc, "s5p-mfc-v6");
#endif
	platform_add_devices(universal5260_media_devices,
			ARRAY_SIZE(universal5260_media_devices));

	exynos_gsc_set_ip_ver(IP_VER_GSC_5A);
	exynos_gsc_set_pm_qos_val(160000, 111000);

	s3c_set_platdata(&exynos_gsc0_default_data,
		sizeof(exynos_gsc0_default_data), &exynos5_device_gsc0);
	s3c_set_platdata(&exynos_gsc1_default_data,
		sizeof(exynos_gsc1_default_data), &exynos5_device_gsc1);

	s5p_fimg2d_set_platdata(&fimg2d_data);
#ifdef CONFIG_VIDEO_EXYNOS_JPEG_HX
	s3c_set_platdata(&exynos5260_jpeg_hx_pd, sizeof(exynos5260_jpeg_hx_pd),
			&exynos5_device_jpeg_hx);
#endif

	s3c_set_platdata(&universal5260_mscl_pd, sizeof(universal5260_mscl_pd),
			&exynos5_device_scaler0);
	s3c_set_platdata(&universal5260_mscl_pd, sizeof(universal5260_mscl_pd),
			&exynos5_device_scaler1);
	if (!exynos_spi_cfg_cs(spi3_csi[0].line, 3)) {
		s3c64xx_spi3_set_platdata(&s3c64xx_spi3_pdata,
				EXYNOS_SPI_SRCCLK_SCLK, ARRAY_SIZE(spi3_csi));

		spi_register_board_info(spi3_board_info,
				ARRAY_SIZE(spi3_board_info));
	} else {
		pr_err("%s: Error requesting gpio for SPI-CH1 CS\n", __func__);
	}   
}
