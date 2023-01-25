/*
 * cypress_touchkey.h - Platform data for cypress touchkey driver
 *
 * Copyright (C) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __LINUX_CYPRESS_TOUCHKEY_H
#define __LINUX_CYPRESS_TOUCHKEY_H
extern struct class *sec_class;
extern int ISSP_main(void);
extern int touch_is_pressed;
struct cypress_touchkey_platform_data {
	unsigned	gpio_int;
	unsigned	gpio_led_en;
	const u8	*touchkey_keycode;
	void	(*power_onoff) (int);
	bool	skip_fw_update;
	bool	touchkey_order;
};

#if defined(CONFIG_KEYBOARD_CYPRESS_TOUCH)
#if defined(CONFIG_MACH_EXPRESS)
#define CYPRESS_DIFF_MENU      0x0A
#define CYPRESS_DIFF_BACK      0x0C
#define CYPRESS_DIFF_HOME      0
#define CYPRESS_DIFF_RECENT    0
#define CYPRESS_RAW_DATA_MENU  0x0E
#define CYPRESS_RAW_DATA_BACK  0x10
#define CYPRESS_RAW_DATA_HOME  0
#define CYPRESS_RAW_DATA_RECENT    0
#define CYPRESS_IDAC_MENU 0x06
#define CYPRESS_IDAC_BACK 0x07

#elif defined(CONFIG_MACH_AEGIS2)
#define CYPRESS_DIFF_BACK      0x10
#define CYPRESS_DIFF_HOME      0x0E
#define CYPRESS_DIFF_RECENT    0x0C
#define CYPRESS_DIFF_MENU      0x0A

#define CYPRESS_RAW_DATA_BACK  0x18
#define CYPRESS_RAW_DATA_HOME  0x16
#define CYPRESS_RAW_DATA_RECENT 0x14
#define CYPRESS_RAW_DATA_MENU  0x12


#endif
#endif

#endif /* __LINUX_CYPRESS_TOUCHKEY_H */
