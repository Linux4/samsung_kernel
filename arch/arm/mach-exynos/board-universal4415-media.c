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

#include "board-universal4415.h"

#include <mach/exynos-mfc.h>
#include <mach/exynos-tv.h>
#include <media/s5p_fimc.h>
#include <media/v4l2-mediabus.h>

struct platform_device exynos4_fimc_md = {
	.name = "s5p-fimc-md",
	.id = 0,
};

static struct platform_device *universal4415_media_devices[] __initdata = {
#ifdef CONFIG_VIDEO_EXYNOS_MFC
	&s5p_device_mfc,
#endif
	&exynos4_fimc_md,
	&s5p_device_fimc0,
	&s5p_device_fimc1,
	&s5p_device_fimc2,
	&s5p_device_fimc3,
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
static struct s5p_mfc_qos universal4415_mfc_qos_table[] = {
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

static struct s5p_mfc_platdata universal4415_mfc_pd = {
	.ip_ver = IP_VER_MFC_4P_1,
	.clock_rate = 200 * MHZ,
#ifdef CONFIG_ARM_EXYNOS4415_BUS_DEVFREQ
	.num_qos_steps	= ARRAY_SIZE(universal4415_mfc_qos_table),
	.qos_table	= universal4415_mfc_qos_table,
#endif
};
#endif

void __init exynos4_universal4415_media_init(void)
{
	platform_add_devices(universal4415_media_devices,
			ARRAY_SIZE(universal4415_media_devices));

#ifdef CONFIG_VIDEO_EXYNOS_MFC
	s5p_mfc_set_platdata(&universal4415_mfc_pd);

	dev_set_name(&s5p_device_mfc.dev, "s5p-mfc");
	clk_add_alias("mfc", "s5p-mfc-v5", "mfc", &s5p_device_mfc.dev);
	clk_add_alias("sclk_mfc", "s5p-mfc-v5", "sclk_mfc",
						&s5p_device_mfc.dev);
	s5p_mfc_setname(&s5p_device_mfc, "s5p-mfc-v5");
#endif

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

}
