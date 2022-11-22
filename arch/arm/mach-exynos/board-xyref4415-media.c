/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/fimc-core.h>
#include <plat/clock.h>
#include <plat/jpeg.h>
#include <plat/fimg2d.h>
#include <plat/iic.h>
#include <plat/tv-core.h>

#include "board-xyref4415.h"

#include <mach/exynos-mfc.h>
#include <mach/exynos-tv.h>
#include <media/s5p_fimc.h>
#include <media/v4l2-mediabus.h>

struct platform_device exynos4_fimc_md = {
	.name = "s5p-fimc-md",
	.id = 0,
};

static struct fimg2d_platdata fimg2d_data __initdata = {
	.ip_ver = IP_VER_G2D_4H,
	.hw_ver = 0x43,
	.parent_clkname	= "mout_g2d_acp",
	.clkname	= "sclk_fimg2d",
	.gate_clkname = "fimg2d",
	.clkrate	= 200000000,
};

struct platform_device exynos_device_md0 = {
	.name = "exynos-mdev",
	.id = 0,
};

static struct platform_device *xyref4415_media_devices[] __initdata = {
#ifdef CONFIG_VIDEO_EXYNOS_MFC
	&s5p_device_mfc,
#endif
	&exynos4_fimc_md,
	&s5p_device_fimc0,
	&s5p_device_fimc1,
	&s5p_device_fimc2,
	&s5p_device_fimc3,
	&s5p_device_jpeg,
	&s5p_device_fimg2d,
	&exynos_device_md0,
	&s5p_device_mixer,
	&s5p_device_hdmi,
	&s3c_device_i2c7,
	&s5p_device_cec,
};

#ifdef CONFIG_VIDEO_EXYNOS_MFC
#ifdef CONFIG_ARM_EXYNOS4415_BUS_DEVFREQ
/*
 * thrd_mb means threadhold of macroblock(MB) count.
 * If current MB counts are higher than table's thrd_mb,
 * table is selected for INT/MIF/CPU locking.
 * Ex> Current MBs = 196000, table[3] is selected.
 *
 * thrd_mb is obtained by the ratio of INT clock for MFC.
 * Maximum MB count is based on 1080p@30fps on 200MHz
 *
 * +------------+---------------+---------------+---------------+
 * | INT Level	| INT clock(Hz)	|  MB count	|  Ratio	|
 * +------------+---------------+---------------+---------------+
 * |   LV0	|    200000	|    244800	|    1.0	|
 * |   LV1	|    160000	|    195840	|    0.8	|
 * |   LV2	|    133000	|    162792	|    0.665	|
 * |   LV3	|    100000	|    122400	|    0.5	|
 * +------------+---------------+---------------+---------------+
 */
static struct s5p_mfc_qos xyref4415_mfc_qos_table[] = {
	[0] = {
		.thrd_mb	= 0,
		.freq_int	= 100000,
		.freq_mif	= 200000,
		.freq_cpu	= 0,
	},
	[1] = {
		.thrd_mb	= 122400,
		.freq_int	= 133000,
		.freq_mif	= 200000,
		.freq_cpu	= 0,
	},
	[2] = {
		.thrd_mb	= 162792,
		.freq_int	= 160000,
		.freq_mif	= 400000,
		.freq_cpu	= 0,
	},
	[3] = {
		.thrd_mb	= 195840,
		.freq_int	= 200000,
		.freq_mif	= 400000,
		.freq_cpu	= 0,
	},
};
#endif

static struct s5p_mfc_platdata xyref4415_mfc_pd = {
	.ip_ver = IP_VER_MFC_4P_1,
	.clock_rate = 200 * MHZ,
#ifdef CONFIG_ARM_EXYNOS4415_BUS_DEVFREQ
	.num_qos_steps	= ARRAY_SIZE(xyref4415_mfc_qos_table),
	.qos_table	= xyref4415_mfc_qos_table,
#endif
};
#endif

static struct s5p_platform_cec hdmi_cec_data __initdata = {
};

static struct s5p_mxr_platdata mxr_platdata __initdata = {
	.ip_ver = IP_VER_TV_4H,
};

static struct s5p_hdmi_platdata hdmi_platdata __initdata = {
	.ip_ver = IP_VER_TV_4H,
};

static struct i2c_board_info i2c_devs7[] __initdata = {
	{
		I2C_BOARD_INFO("exynos_hdcp", (0x74 >> 1)),
	},
	{
		I2C_BOARD_INFO("exynos_edid", (0xA0 >> 1)),
	},
};

void __init exynos4_xyref4415_media_init(void)
{
#ifdef CONFIG_VIDEO_EXYNOS_MFC
	s5p_mfc_set_platdata(&xyref4415_mfc_pd);

	dev_set_name(&s5p_device_mfc.dev, "s5p-mfc");
	clk_add_alias("mfc", "s5p-mfc-v5", "mfc", &s5p_device_mfc.dev);
	clk_add_alias("sclk_mfc", "s5p-mfc-v5", "sclk_mfc",
			&s5p_device_mfc.dev);
	s5p_mfc_setname(&s5p_device_mfc, "s5p-mfc-v5");
#endif
	platform_add_devices(xyref4415_media_devices,
			ARRAY_SIZE(xyref4415_media_devices));

	dev_set_name(&s5p_device_fimc0.dev, "exynos4-fimc.0");
	dev_set_name(&s5p_device_fimc1.dev, "exynos4-fimc.1");
	dev_set_name(&s5p_device_fimc2.dev, "exynos4-fimc.2");
	dev_set_name(&s5p_device_fimc3.dev, "exynos4-fimc.3");

	s3c_set_platdata(&s5p_fimc_md_platdata,
			 sizeof(s5p_fimc_md_platdata), &exynos4_fimc_md);
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_fimc0.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_fimc1.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_fimc2.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_fimc3.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#endif

#ifdef CONFIG_VIDEO_EXYNOS_FIMG2D
	s5p_fimg2d_set_platdata(&fimg2d_data);
#endif

	dev_set_name(&s5p_device_hdmi.dev, "exynos4-hdmi");
	s5p_tv_setup();
	s5p_hdmi_cec_set_platdata(&hdmi_cec_data);
	s3c_set_platdata(&mxr_platdata, sizeof(mxr_platdata), &s5p_device_mixer);
	s5p_hdmi_set_platdata(&hdmi_platdata);
	s3c_i2c7_set_platdata(NULL);
	i2c_register_board_info(7, i2c_devs7, ARRAY_SIZE(i2c_devs7));
}
