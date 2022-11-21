/****************************************************************

Zoro Software.
N-Trig Digitizer modules files
Copyright (C) 2010, Dmitry Kuzminov

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

 This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

****************************************************************/

#ifndef _NTRIG_DISPATCHER_SYSFS_H
#define _NTRIG_DISPATCHER_SYSFS_H


/**
 * External Interface APIs
 */
int ntrig_dispatcher_sysfs_init(void);
void ntrig_dispathcer_sysfs_exit(void);

/**
 * External Virtual Keys APIs
 */
int get_virt_keys_scan_code(int i);
bool virtual_keys_supported(void);
int get_virtual_keys_num(void);
int get_touch_screen_border_left(void);
int get_touch_screen_border_right(void);
int get_touch_screen_border_down(void);
int get_touch_screen_border_up(void);
int get_touch_screen_border_pen_left(void);
int get_touch_screen_border_pen_right(void);
int get_touch_screen_border_pen_down(void);
int get_touch_screen_border_pen_up(void);


#endif /* _NTRIG_DISPATCHER_SYSFS_H */
