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
#include <plat/iic.h>
#include <mach/exynos-mfc.h>
#include <mach/exynos-scaler.h>

#include <linux/spi/spi.h>
#include <plat/s3c64xx-spi.h>
#include <mach/spi-clocks.h>
#include <mach/exynos-tv.h>

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

#if defined(CONFIG_MACH_M2ALTE_KOR_SKT) \
	|| defined(CONFIG_MACH_M2ALTE_KOR_KTT) || defined(CONFIG_MACH_M2ALTE_KOR_LGT)
static struct s3c64xx_spi_csinfo spi2_csi[] = {
	[0] = {
		.line           = EXYNOS5260_GPB1(1),
		.set_level      = gpio_set_value,
		.fb_delay       = 0x2,
	},
};

static struct spi_board_info spi2_board_info[] __initdata = {
	{
		.modalias               = "spi_modem_boot",
		.platform_data          = NULL,
		.max_speed_hz           = 2 * 1000 * 1000,
		.bus_num                = 2,
		.chip_select            = 0,
		.mode                   = SPI_MODE_0,
		.controller_data        = &spi2_csi[0],
	}
};
#endif

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
#if defined(CONFIG_MACH_M2ALTE_KOR_SKT) \
	|| defined(CONFIG_MACH_M2ALTE_KOR_KTT) || defined(CONFIG_MACH_M2ALTE_KOR_LGT)
	&s3c64xx_device_spi2,
#endif
	&s3c64xx_device_spi3,
#ifdef CONFIG_VIDEO_EXYNOS_TV
	&s3c_device_i2c2,
	&s5p_device_mixer,
	&s5p_device_hdmi,
#ifdef CONFIG_VIDEO_EXYNOS_HDMI_CEC
	&s5p_device_cec,
#endif
#endif
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
#ifdef CONFIG_VIDEO_EXYNOS_TV
#ifdef CONFIG_VIDEO_EXYNOS_HDMI_CEC
static struct s5p_platform_cec hdmi_cec_data __initdata = {
};
#endif
static struct s5p_mxr_platdata mxr_platdata __initdata = {
	.ip_ver = IP_VER_TV_5R,
};
static struct s5p_hdmi_platdata hdmi_platdata __initdata = {
	.ip_ver = IP_VER_TV_5R,
};
static struct i2c_board_info i2c_devs2[] __initdata = {
	{
		I2C_BOARD_INFO("exynos_hdcp", (0x74 >> 1)),
	},
	{
		I2C_BOARD_INFO("exynos_edid", (0xA0 >> 1)),
	},
};
static struct s3c2410_platform_i2c i2c2_data __initdata = {
	.frequency	= 100*1000,
	.sda_delay	= 100,
	.bus_num = 6,
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

	exynos5_gsc_set_ip_ver(IP_VER_GSC_5A);
	exynos5_gsc_set_pm_qos_val(160000, 111000);

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
#if defined(CONFIG_MACH_M2ALTE_KOR_SKT) \
	|| defined(CONFIG_MACH_M2ALTE_KOR_KTT) || defined(CONFIG_MACH_M2ALTE_KOR_LGT)
	pr_err("%s: Setup spi-CH2 CS\n", __func__);
	if (!exynos_spi_cfg_cs(spi2_csi[0].line, 2)) {
		pr_err("%s: spi config gpio\n", __func__);
		s3c64xx_spi2_set_platdata(&s3c64xx_spi2_pdata,
				EXYNOS_SPI_SRCCLK_SCLK, ARRAY_SIZE(spi2_csi));

		spi_register_board_info(spi2_board_info,
				ARRAY_SIZE(spi2_board_info));
	} else {
		pr_err("%s: Error requesting gpio for SPI-CH2 CS\n", __func__);
	}
#endif
	if (!exynos_spi_cfg_cs(spi3_csi[0].line, 3)) {
		s3c64xx_spi3_set_platdata(&s3c64xx_spi3_pdata,
				EXYNOS_SPI_SRCCLK_SCLK, ARRAY_SIZE(spi3_csi));

		spi_register_board_info(spi3_board_info,
				ARRAY_SIZE(spi3_board_info));
	} else {
		pr_err("%s: Error requesting gpio for SPI-CH1 CS\n", __func__);
	}
#ifdef CONFIG_VIDEO_EXYNOS_TV
	dev_set_name(&s5p_device_hdmi.dev, "exynos5-hdmi");
	s5p_tv_setup();
#ifdef CONFIG_VIDEO_EXYNOS_HDMI_CEC
	s5p_hdmi_cec_set_platdata(&hdmi_cec_data);
#endif
	s3c_set_platdata(&mxr_platdata, sizeof(mxr_platdata), &s5p_device_mixer);
	s5p_hdmi_set_platdata(&hdmi_platdata);
	s3c_i2c2_set_platdata(&i2c2_data);
	i2c_register_board_info(6, i2c_devs2, ARRAY_SIZE(i2c_devs2));
#endif
}
