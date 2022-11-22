/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * DPP HDR LUT(Look Up Table)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _HDR_LUT_
#define _HDR_LUT_

#include <linux/types.h>

#if defined(CONFIG_SOC_EXYNOS9630)
#define MAX_OETF	(33)
#define MAX_EOTF	(129)
#endif

#define MAX_GM		(9)
#define MAX_TM		(33)


#endif /* _HDR_LUT_ */
