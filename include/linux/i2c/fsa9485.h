/*
 * Copyright (C) 2010 Samsung Electronics
 * Minkyu Kang <mk7.kang@samsung.com>
 * Wonguk Jeong <wonguk.jeong@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef _FSA9485_H_
#define _FSA9485_H_
#if defined(CONFIG_MACH_LT02)
#include <linux/mfd/pm8xxx/pm8921-charger.h>
#endif
enum {
	FSA9485_DETACHED,

	FSA9485_ATTACHED,
#if defined(CONFIG_MACH_LT02)
	TSU6721_ATTACHED,
#endif
};
#if defined(CONFIG_MACH_LT02)
enum {
	DISABLE,
	ENABLE
};
#endif
enum {
	FSA9485_DETACHED_DOCK = 0,
	FSA9485_ATTACHED_DESK_DOCK,
	FSA9485_ATTACHED_CAR_DOCK,
};

#define UART_SEL_SW	    58
#if defined(CONFIG_MACH_LT02)
enum {
	USB_CALL = 0,
	CDP_CALL,
	UART_CALL,
	CHARGER_CALL,
	OTG_CALL,
	JIG_CALL,
	DESKDOCK_CALL,
	CARDOCK_CALL,
	SMARTDOCK_CALL,
	AUDIODOCK_CALL,
	MHL_CALL,
	INCOMPATIBLE_CALL,
};
#endif
struct fsa9485_platform_data {
	void (*cfg_gpio) (void);
	void (*otg_cb) (bool attached);
	void (*usb_cb) (bool attached);
	void (*uart_cb) (bool attached);
	void (*charger_cb) (bool attached);
	void (*jig_cb) (bool attached);
	void (*mhl_cb) (bool attached);
	void (*reset_cb) (void);
	void (*set_init_flag) (void);
	void (*mhl_sel) (bool onoff);
	void (*dock_cb) (int attached);
	int	(*dock_init) (void);
	void (*usb_cdp_cb) (bool attached);
	void (*smartdock_cb) (bool attached);
	void (*audio_dock_cb) (bool attached);
#if defined(CONFIG_MACH_LT02)
    void (*callback)(enum cable_type_t cable_type, int attached);
	void (*oxp_callback)(int state);
#endif
};

enum {
	SWITCH_PORT_AUTO = 0,
	SWITCH_PORT_USB,
	SWITCH_PORT_AUDIO,
	SWITCH_PORT_UART,
	SWITCH_PORT_VAUDIO,
	SWITCH_PORT_USB_OPEN,
	SWITCH_PORT_ALL_OPEN,
};

extern void fsa9485_manual_switching(int path);
extern void fsa9485_otg_detach(void);
#if defined(CONFIG_MACH_AEGIS2)
extern void fsa9485_checkandhookaudiodockfornoise(int value);
#endif
#if defined(CONFIG_MACH_LT02)
extern int check_jig_state(void);
#endif
extern struct class *sec_class;

#endif /* _FSA9485_H_ */
