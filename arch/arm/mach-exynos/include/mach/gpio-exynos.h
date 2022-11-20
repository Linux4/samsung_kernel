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
#elif defined(CONFIG_MACH_M2ALTE)
#include "gpio-m2alte-rev00.h"
#elif defined(CONFIG_MACH_M2A3G)
#include "gpio-m2a3g-rev00.h"
#elif defined(CONFIG_MACH_MEGA2ELTE)
#include "gpio-mega2elte-rev00.h"
#endif

#endif
extern void (*exynos_config_sleep_gpio)(void);
#endif
