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

#ifndef __STUI_HAL_H_
#define __STUI_HAL_H_

int stui_i2c_protect(bool is_protect);
int stui_prepare_tui(void);
void stui_finish_tui(void);
void stui_free_video_space(void);
unsigned long stui_alloc_video_space(uint32_t allocsize, uint32_t count);

#endif /* __STUI_HAL_H_ */
