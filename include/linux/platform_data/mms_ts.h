/*
 * mms_ts.h - Platform data for Melfas MMS-series touch driver
 *
 * Copyright (C) 2011 Google Inc.
 * Author: Dima Zavin <dima@android.com>
 *
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#ifndef _LINUX_MMS_TOUCH_H
#define _LINUX_MMS_TOUCH_H

extern struct tsp_callbacks *charger_callbacks;
struct tsp_callbacks {
	void (*inform_charger)(struct tsp_callbacks *tsp_cb, bool mode);
};
struct mms_ts_platform_data {
	int	max_x;
	int	max_y;

	bool	invert_x;
	bool	invert_y;

	int	gpio_sda;
	int	gpio_scl;
	int	gpio_resetb;
	int	gpio_lcd_type;
	int	(*mux_fw_flash)(bool to_gpios);
	void (*vdd_on)(bool);
	int (*is_vdd_on)(void);
	void	(*register_cb)(struct tsp_callbacks *);
	const char	*fw_name;
	bool	use_touchkey;
	const u8	*touchkey_keycode;
	const u8	*config_fw_version;
	int	check_module_type;
};
extern struct class *sec_class;
extern int poweroff_charging;
extern unsigned char LCD_Get_Value(void);

#endif /* _LINUX_MMS_TOUCH_H */
