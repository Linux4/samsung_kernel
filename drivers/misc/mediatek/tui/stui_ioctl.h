/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __STUI_IOCTL_H_
#define __STUI_IOCTL_H_

/* Commands and Structures for TUI */
#define STUI_HW_IOCTL_START_TUI		0x10
#define STUI_HW_IOCTL_FINISH_TUI	0x11

struct tui_hw_buffer {
	unsigned long fb_physical;
	unsigned long wb_physical;
	unsigned long disp_physical;
	unsigned long touch_physical;
	unsigned long fb_size;
	unsigned long wb_size;
	unsigned long disp_size;
	unsigned long touch_size;
};

#define STUI_RET_OK			0x00030000
#define STUI_RET_ERR_INTERNAL_ERROR	0x00030003

#endif /* __STUI_IOCTL_H_ */
