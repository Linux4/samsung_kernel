/*
 *  Copyright (C) 2020, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __SHUB_PANEL_H_
#define __SHUB_PANEL_H_

#define UID_FILE_PATH "/efs/FactoryApp/ubid"

void init_shub_panel(void);
void remove_shub_panel(void);

bool is_lcd_changed(void);
int save_panel_lcd_type(void);
#endif
