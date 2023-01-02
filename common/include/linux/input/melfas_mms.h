/*
 * MELFAS MMS Touchscreen Driver - Platform Data
 *
 * Copyright (C) 2014 MELFAS Inc.
 *
 * Default path : linux/input/melfas_mms.h
 *
 */

#ifndef _LINUX_MMS_TOUCH_H
#define _LINUX_MMS_TOUCH_H

#ifdef CONFIG_OF
#define MMS_USE_DEVICETREE		1
#else
#define MMS_USE_DEVICETREE		0
#endif

#define MMS_USE_CALLBACK	0	// 0 or 1 : Callback for inform charger, display, power, etc...

#define MMS_DEVICE_NAME	"mms_ts"

/**
* Platform Data
*/
struct mms_platform_data {
	unsigned int max_x;
	unsigned int max_y;

	int gpio_intr;			//Required (interrupt signal)

	int gpio_vdd_en;		//Optional (power control)	
	//int gpio_reset;		//Optional
	//int gpio_sda;			//Optional
	//int gpio_scl;			//Optional

#if MMS_USE_CALLBACK
	void (*register_callback) (void *);
#endif
};

#endif

