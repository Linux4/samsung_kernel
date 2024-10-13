/*
 * MELFAS MIP4 Touchkey
 *
 * Copyright (C) 2015 MELFAS Inc.
 *
 *
 * melfas_mip4_tk.h : Platform data
 *
 *
 * Default path : linux/input/melfas_mip4_tk.h
 *
 */

#ifndef _LINUX_MIP4_TOUCHKEY_H
#define _LINUX_MIP4_TOUCHKEY_H

#ifdef CONFIG_OF
#define MIP_USE_DEVICETREE		1
#else
#define MIP_USE_DEVICETREE		0
#endif

#define MIP_USE_CALLBACK	0	// 0 or 1 : Callback for inform charger, display, power, etc...

#define MIP_DEVICE_NAME	"mip4_tk"

/**
* Platform Data
*/
struct mip4_tk_platform_data {
	int gpio_intr;			//Required (interrupt signal)
	int gpio_ce;			//Optional (chip enable/reset)

#if MIP_USE_CALLBACK
	void (*register_callback) (void *);
#endif
};

#endif

