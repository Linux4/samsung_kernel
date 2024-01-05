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

#include "board-espresso3250.h"

#include <mach/exynos-mfc.h>
#include <mach/exynos-tv.h>
#include <media/exynos_gscaler.h>
#include <media/s5p_fimc.h>
#include <media/v4l2-mediabus.h>

struct platform_device exynos_device_md0 = {
	.name = "exynos-mdev",
	.id = 0,
};

struct platform_device exynos_device_md1 = {
	.name = "exynos-mdev",
	.id = 1,
};

static struct platform_device *espresso3250_media_devices[] __initdata = {
#ifdef CONFIG_VIDEO_EXYNOS_MFC
	&s5p_device_mfc,
#endif
	&exynos_device_md0,
	&exynos_device_md1,
#ifdef CONFIG_VIDEO_EXYNOS_GSCALER
	&exynos3_device_gsc0,
	&exynos3_device_gsc1,
#endif
	&exynos5_device_jpeg_hx,
};

#ifdef CONFIG_VIDEO_EXYNOS_MFC
#ifdef CONFIG_ARM_EXYNOS3250_BUS_DEVFREQ
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
static struct s5p_mfc_qos espresso3250_mfc_qos_table[] = {
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

#ifdef CONFIG_ARM_EXYNOS3472_BUS_DEVFREQ
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
static struct s5p_mfc_qos espresso3472_mfc_qos_table[] = {
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

#ifdef CONFIG_VIDEO_EXYNOS_JPEG_HX
static struct exynos_jpeg_platdata espresso3250_jpeg_hx_pd __initdata = {
	.ip_ver		= IP_VER_JPEG_HX_3P,
	.gateclk	= "jpeg-hx",
};
#endif

static struct s5p_mfc_platdata espresso3250_mfc_pd = {
	.ip_ver = IP_VER_MFC_6P_0,
	.clock_rate = 200 * MHZ,
#ifdef CONFIG_ARM_EXYNOS3250_BUS_DEVFREQ
	.num_qos_steps	= ARRAY_SIZE(espresso3250_mfc_qos_table),
	.qos_table	= espresso3250_mfc_qos_table,
#endif

#ifdef CONFIG_ARM_EXYNOS3472_BUS_DEVFREQ
	.num_qos_steps	= ARRAY_SIZE(espresso3472_mfc_qos_table),
	.qos_table	= espresso3472_mfc_qos_table,
#endif
};
#endif

void __init exynos3_espresso3250_media_init(void)
{
	platform_add_devices(espresso3250_media_devices,
			ARRAY_SIZE(espresso3250_media_devices));

#ifdef CONFIG_VIDEO_EXYNOS_MFC
	s5p_mfc_set_platdata(&espresso3250_mfc_pd);

	dev_set_name(&s5p_device_mfc.dev, "s5p-mfc");
	clk_add_alias("mfc", "s5p-mfc-v6", "mfc", &s5p_device_mfc.dev);
	clk_add_alias("sclk_mfc", "s5p-mfc-v6", "sclk_mfc",
						&s5p_device_mfc.dev);
	s5p_mfc_setname(&s5p_device_mfc, "s5p-mfc-v6");
#endif
#ifdef CONFIG_VIDEO_EXYNOS_JPEG_HX
	s3c_set_platdata(&espresso3250_jpeg_hx_pd, sizeof(espresso3250_jpeg_hx_pd),
			&exynos5_device_jpeg_hx);
#endif
#ifdef CONFIG_VIDEO_EXYNOS_GSCALER
	exynos_gsc_set_ip_ver(IP_VER_GSC_5A);
	exynos_gsc_set_pm_qos_val(160000, 111000);
	s3c_set_platdata(&exynos_gsc0_default_data,
		sizeof(exynos_gsc0_default_data), &exynos3_device_gsc0);
	s3c_set_platdata(&exynos_gsc1_default_data,
		sizeof(exynos_gsc1_default_data), &exynos3_device_gsc1);
#endif
}
