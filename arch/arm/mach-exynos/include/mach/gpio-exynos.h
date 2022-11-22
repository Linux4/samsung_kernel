/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_GPIO_EXYNOS_H
#define __ASM_ARCH_GPIO_EXYNOS_H __FILE__

#include <mach/gpio.h>

#if defined(CONFIG_MACH_UNIVERSAL5260)

#if defined(CONFIG_MACH_HL3G)
#include "gpio-hl3g-rev00.h"
#elif defined(CONFIG_MACH_HLLTE)
#include "gpio-hllte-rev00.h"
#endif

#endif

#if defined(CONFIG_SOC_EXYNOS3470)
#include "gpio-shannon222ap.h"
#endif

extern void (*exynos_config_sleep_gpio)(void);
#endif
