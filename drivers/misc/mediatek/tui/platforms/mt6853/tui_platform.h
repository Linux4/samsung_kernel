/*
 * Copyright (c) 2015-2018 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __TUI_PLATFORM_H__
#define __TUI_PLATFORM_H__

/*display*/
extern int display_enter_tui(void);
extern int display_exit_tui(void);

extern unsigned int DISP_GetScreenWidth(void);
extern unsigned int DISP_GetScreenHeight(void);
#endif
