/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/errno.h>
#include <linux/platform_device.h>
#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/fimc-core.h>
#include <plat/clock.h>
#include <plat/jpeg.h>

#include "board-xyref4415.h"

#include <media/s5p_fimc.h>
#include <media/v4l2-mediabus.h>

struct platform_device exynos4_fimc_md = {
	.name = "s5p-fimc-md",
	.id = 0,
};

static struct platform_device *xyref4415_media_devices[] __initdata = {
	&exynos4_fimc_md,
	&s5p_device_fimc0,
	&s5p_device_fimc1,
	&s5p_device_fimc2,
	&s5p_device_fimc3,
	&s5p_device_jpeg,
};

void __init exynos4_xyref4415_media_init(void)
{
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
}
