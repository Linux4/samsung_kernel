/*
 * Copyright (C) 2016-2017 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * for ptn38003 driver
 */

#ifndef __LINUX_PTN38003_H__
#define __LINUX_PTN38003_H__

#include <linux/ccic/max77705.h>

#define Chip_ID				0x00
#define Chip_Rev			0x01
#define Mode_Control			0x04
#define Device_Control			0x05
#define DP_Link_Control			0x06
#define DP_Lane0_Control1		0x07
#define DP_Lane0_Control2		0x08
#define DP_Lane1_Control1		0x09
#define DP_Lane1_Control2		0x0a
#define DP_Lane2_Control1		0x0b
#define DP_Lane2_Control2		0x0c
#define DP_Lane3_Control1		0x0d
#define DP_Lane3_Control2		0x0e
#define LOS_Detector_Threshold		0x0f
#define USB_Downstream_RX_Control	0x10
#define USB_Upstream_TX_Control		0x11
#define USB_Upstream_RX_Control		0x12
#define USB_Downstream_TX_Control	0x13

typedef union{
	uint8_t	data;
	 struct {
		uint8_t	RxEq:4,
				Swing:2,
				DeEmpha:2;
	} BITS;
} usb_Control;

struct ptn38003_data {
	struct device *dev;
	struct i2c_client *i2c;
	struct mutex i2c_mutex;
	int combo_redriver_en;
	int redriver_en;
	int con_sel;
	usb_Control usbControl_US;
	usb_Control usbControl_DS;
};

enum config_type {
	INIT_MODE = 0,
	USB3_ONLY_MODE,
	DP4_LANE_MODE,
	DP2_LANE_USB3_MODE,
	SAFE_STATE,
};

extern void ptn38003_config(int config, int is_DFP);
extern int ptn38003_i2c_read(u8 command);

#endif


