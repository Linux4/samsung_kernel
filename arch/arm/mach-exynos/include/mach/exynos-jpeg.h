/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for exynos jpeg support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_JPEG_H
#define __EXYNOS_JPEG_H __FILE__

int __init exynos4_jpeg_setup_clock(struct device *dev,
					unsigned long clk_rate);
#endif
