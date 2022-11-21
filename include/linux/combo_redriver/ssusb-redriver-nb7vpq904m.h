/*
 * Copyright (C) 2016-2017 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * for nb7vpq904m driver
 */
#ifndef __LINUX_NB7VPQ904M_H__
#define __LINUX_NB7VPQ904M_H__

/*
 * Three Modes of Operations:
 *  - One/Two ports of USB 3.1 Gen1/Gen2 (Default Mode)
 *  - Two lanes of DisplayPort 1.4 + One port of USB 3.1 Gen1/Gen2
 *  - Four lanes of DisplayPort 1.4
 */
enum operation_mode {
	OP_MODE_USB,	/* One/Two ports of USB */
	OP_MODE_DP,		/* DP 4 Lane and DP 2 Lane */
	OP_MODE_USB_AND_DP, /* One port of USB and DP 2 Lane */
};

extern void ssusb_redriver_config(int mode, bool on);

#endif


