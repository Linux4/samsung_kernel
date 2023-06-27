/*
 * mms_ts.h - Platform data for Melfas MMS-series touch driver
 *
 * Copyright (C) 2011 Google Inc.
 * Author: Dima Zavin <dima@android.com>
 *
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#if defined(CONFIG_MACH_ARUBA_TD) || defined(CONFIG_MACH_WARUBA) || defined(CONFIG_MACH_HARRISON)
#include <../arch/arm/mach-mmp/include/mach/mfp-pxa988-aruba.h>
#elif defined(CONFIG_MACH_HELANDELOS) 
#include <../arch/arm/mach-mmp/include/mach/mfp-pxa1088-delos.h>
#elif defined(CONFIG_MACH_CS02)
#include <../arch/arm/mach-mmp/include/mach/mfp-pxa986-cs02.h>
#elif defined(CONFIG_MACH_CS05)
#include <../arch/arm/mach-mmp/include/mach/mfp-pxa1088-cs05.h>
#endif

#if defined(CONFIG_MACH_ARUBA_TD) || defined(CONFIG_MACH_WARUBA) || defined(CONFIG_MACH_HARRISON) \
	|| defined(CONFIG_MACH_HELANDELOS)
#define TSP_SDA	mfp_to_gpio(GPIO017_GPIO_17)
#define TSP_SCL	mfp_to_gpio(GPIO016_GPIO_16)
#define TSP_INT	mfp_to_gpio(GPIO094_GPIO_94)
#define KEY_LED_GPIO mfp_to_gpio(GPIO096_GPIO_96)
#elif defined(CONFIG_MACH_CS02) || defined(CONFIG_MACH_CS05)
#define TSP_SDA	mfp_to_gpio(GPIO088_GPIO_88)
#define TSP_SCL	mfp_to_gpio(GPIO087_GPIO_87)
#define TSP_INT	mfp_to_gpio(GPIO094_GPIO_94)
#endif


#if defined(CONFIG_MACH_ARUBA_TD)
#define TSP_TYPE1	mfp_to_gpio(GPIO032_GPIO_32)
#define TSP_TYPE2	mfp_to_gpio(GPIO085_GPIO_85)
#define TSP_TYPE3	mfp_to_gpio(GPIO086_GPIO_86)
#endif

#ifndef _LINUX_MMS_TOUCH_H
#define _LINUX_MMS_TOUCH_H

struct mms_ts_platform_data {
	int	max_x;
	int	max_y;

	bool	invert_x;
	bool	invert_y;

	int	gpio_sda;
	int	gpio_scl;
	int	gpio_resetb;
	int	gpio_vdd_en;

	int	(*mux_fw_flash)(bool to_gpios);
	const char	*fw_name;
};

#endif /* _LINUX_MMS_TOUCH_H */
