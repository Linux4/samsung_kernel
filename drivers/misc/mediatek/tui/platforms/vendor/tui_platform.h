/*
* Copyright (C) 2017 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/
#ifndef __TUI_PLATFORM_H__
#define __TUI_PLATFORM_H__

#include <linux/types.h>
/*fix extern error*/
#include "ion_drv.h"

extern struct stui_buf_info g_stui_buf_info;
extern struct ion_device *g_ion_device;

/*memory alloc/free*/
extern int tui_region_offline(phys_addr_t *pa, unsigned long *size);
extern int tui_region_online(void);

/*display*/
extern int display_enter_tui(void);
extern int display_exit_tui(void);

/*touch*/
extern int tpd_enter_tui(void);
extern int tpd_exit_tui(void);

/*I2C*/
extern int i2c_tui_enable_clock(int id);
extern int i2c_tui_disable_clock(int id);

#endif
