/*
 * MELFAS MIP4 Touchscreen
 *
 * Copyright (C) 2016 MELFAS Inc.
 *
 *
 * melfas_mip4_ts.h : Platform data
 *
 * Version : 2016.09.06
 *
 * Default path : linux/input/melfas_mip4_ts.h
 */

#ifndef _LINUX_MELFAS_MIP4_TS_H
#define _LINUX_MELFAS_MIP4_TS_H

#define MIP4_TS_DEVICE_NAME "mip4_ts"

/*
* Platform Data
*/
struct mip4_ts_platform_data {
	//Interrupt : Required
	int gpio_intr;

	//Chip enable(Reset) : Optional
	int gpio_ce;

	//Power control GPIO : Optional
	int gpio_vd33_en;

	//ISP GPIO : Required for MCS6000
	int gpio_sda;
	int gpio_scl;
	int (*gpio_mode)(bool);
};
#endif

