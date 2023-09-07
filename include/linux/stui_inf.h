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

#ifndef __LINUX_STUI_INF_H
#define __LINUX_STUI_INF_H

#define STUI_MODE_OFF			0x00
#define STUI_MODE_ALL			0x07
#define STUI_MODE_TUI_SESSION		0x01
#define STUI_MODE_DISPLAY_SEC		0x02
#define STUI_MODE_TOUCH_SEC		0x04

int  stui_inc_blank_ref(void);
int  stui_dec_blank_ref(void);
int  stui_get_blank_ref(void);
void stui_set_blank_ref(int ref);

int  stui_get_mode(void);
void stui_set_mode(int mode);

int  stui_set_mask(int mask);
int  stui_clear_mask(int mask);

#endif /* __LINUX_STUI_INF_H */
