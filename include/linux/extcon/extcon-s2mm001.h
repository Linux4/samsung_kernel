/*
 *
 * Copyright (c) Samsung Electronics Co, Ltd.
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

#ifndef _S2MM001_MUIC_H_
#define _S2MM001_MUIC_H_

#include <linux/types.h>

enum s2mm001_cable_type {
	S2MM001_CABLE_TYPE_NONE = 0,
	S2MM001_CABLE_TYPE1_AUDIO1,
	S2MM001_CABLE_TYPE1_AUDIO2,
	S2MM001_CABLE_TYPE1_USB,
	S2MM001_CABLE_TYPE1_UART,
	S2MM001_CABLE_TYPE1_CARKIT,
	S2MM001_CABLE_TYPE1_TA,
	S2MM001_CABLE_TYPE1_OTG,
	S2MM001_CABLE_TYPE2_JIG_USB_ON,
	S2MM001_CABLE_TYPE2_JIG_USB_OFF,
	S2MM001_CABLE_TYPE2_JIG_UART_ON,
	S2MM001_CABLE_TYPE2_JIG_UART_ON_VB,
	S2MM001_CABLE_TYPE2_JIG_UART_OFF,
	S2MM001_CABLE_TYPE2_JIG_UART_OFF_VB,
	S2MM001_CABLE_TYPE2_DESKDOCK,
	S2MM001_CABLE_TYPE3_MHL_VB,
	S2MM001_CABLE_TYPE3_MHL,
	S2MM001_CABLE_TYPE3_DESKDOCK_VB,
	S2MM001_CABLE_TYPE3_U200CHG,
	S2MM001_CABLE_TYPE3_NONSTD_SDP,
	S2MM001_CABLE_UNKNOWN,
};

struct s2mm001_platform_data {
	void (*charger_cb) (u8 attached);
	u32 qos_val;
};

extern int is_battery_connected(void);

#endif /* _S2MM001_MUIC_H_ */
