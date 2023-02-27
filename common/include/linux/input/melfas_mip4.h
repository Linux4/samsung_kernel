/*
 * MELFAS MIP4 Touchscreen
 *
 * Copyright (C) 2015 MELFAS Inc.
 *
 *
 * melfas_mip4.h : Platform data
 *
 *
 * Default path : linux/input/melfas_mip4.h
 *
 */

#ifndef _LINUX_MIP_TOUCH_H
#define _LINUX_MIP_TOUCH_H

#ifdef CONFIG_OF
#define MIP_USE_DEVICETREE		1
#else
#define MIP_USE_DEVICETREE		0
#endif

#define MIP_USE_CALLBACK	0	// 0 or 1 : Callback for charger, display, power, etc...

#define MIP_DEVICE_NAME	"mip4_ts"

/**
* Platform Data
*/
struct melfas_platform_data {
	unsigned int max_x;
	unsigned int max_y;
	unsigned int x_num;
	unsigned int y_num;
	int gpio_intr;
	int gpio_vdd_en;
	int gpio_sda;
	int gpio_scl;
	const char *fw_name;
	const char *ext_fw_name;
	int (*gpio_mode)(bool);
#if MIP_USE_CALLBACK
	void (*register_callback) (void *);
#endif
};

#endif

