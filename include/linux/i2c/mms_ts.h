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

#ifndef CONFIG_TOUCHSCREEN_MMS134
extern struct tsp_callbacks *charger_callbacks;
struct tsp_callbacks {
	void (*inform_charger)(struct tsp_callbacks *tsp_cb, bool mode);
};
#endif

struct mms_ts_platform_data {
	int	max_x;
	int	max_y;

	bool	invert_x;
	bool	invert_y;

	int	gpio_sda;
	int	gpio_scl;
	int    gpio_int;
	int	gpio_resetb;
#ifndef CONFIG_TOUCHSCREEN_MMS134
	int	gpio_vdd_en;
#else
	int	gpio_lcd_type;
#endif
	int	(*mux_fw_flash)(bool to_gpios);
	void (*vdd_on)(bool);
	int (*is_vdd_on)(void);
	void	(*register_cb)(struct tsp_callbacks *);
	const char	*fw_name;
	bool	use_touchkey;
	bool	use_surface_touch;
	const u8	*touchkey_keycode;
	int	check_module_type;
	void (*tkey_led_vdd_on)(bool);
	const char *tsp_ic_name;
};
#ifndef CONFIG_TOUCHSCREEN_MMS134
extern struct class *sec_class;
extern int touch_is_pressed;
/* SPRD remove this declaration due to a compile error */
//extern int system_rev;

extern int poweroff_charging;
/*
extern unsigned char LCD_Get_Value(void);
*/
#endif
#endif /* _LINUX_MMS_TOUCH_H */
