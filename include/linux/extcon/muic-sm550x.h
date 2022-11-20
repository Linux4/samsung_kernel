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

#ifndef _SM550X_MUIC_H_
#define _SM550X_MUIC_H_

enum {
	CABLE_NONE_MUIC = 0,
	CABLE_SDP_MUIC,		//Max 500mA with Data
	CABLE_DCP_MUIC,		//Max 1.5A
	CABLE_CDP_MUIC,		//Max 1.5A with Data
	CABLE_OTG_MUIC,
	CABLE_CARKIT_T1_MUIC,	//Max 500mA with Data (Because of LG USB cable)
	CABLE_UART_MUIC,
	CABLE_JIG_UART_OFF_MUIC,
	CABLE_JIG_UART_OFF_VB_MUIC,
	CABLE_JIG_UART_ON_MUIC,
	CABLE_JIG_UART_ON_VB_MUIC,
	CABLE_JIG_USB_ON_MUIC,
	CABLE_JIG_USB_OFF_MUIC,
	CABLE_DESKTOP_DOCK_MUIC,
	CABLE_DESKTOP_DOCK_VB_MUIC,
	CABLE_UNKNOWN,
	CABLE_TYPE_NONE_MUIC,
	CABLE_TYPE1_USB_MUIC,
	CABLE_TYPE1_TA_MUIC,
	CABLE_TYPE1_OTG_MUIC,
	CABLE_TYPE1_CARKIT_T1OR2_MUIC,
	CABLE_TYPE2_JIG_UART_OFF_MUIC,
	CABLE_TYPE2_JIG_UART_OFF_VB_MUIC,
	CABLE_TYPE2_JIG_UART_ON_MUIC,
	CABLE_TYPE2_JIG_UART_ON_VB_MUIC,
	CABLE_TYPE2_JIG_USB_ON_MUIC,
	CABLE_TYPE2_JIG_USB_OFF_MUIC,
	CABLE_TYPE2_DESKDOCK_MUIC,
	CABLE_TYPE3_MHL_VB_MUIC,
	CABLE_TYPE3_MHL_MUIC,
	CABLE_TYPE3_DESKDOCK_VB_MUIC,
	CABLE_TYPE3_U200CHG_MUIC,
	CABLE_TYPE3_NONSTD_SDP_MUIC,
	CABLE_TYPE_AUDIODOCK_MUIC,
	CABLE_TYPE_SMARTDOCK_MUIC,
	CABLE_TYPE_SMARTDOCK_USB_MUIC,
	CABLE_TYPE_SMARTDOCK_TA_MUIC,
};

extern int usb_switch_register_notify(struct notifier_block *nb);
extern int usb_switch_unregister_notify(struct notifier_block *nb);

#endif
