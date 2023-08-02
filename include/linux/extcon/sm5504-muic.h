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

#ifndef _SM5504_MUIC_H_
#define _SM5504_MUIC_H_

#include <linux/pm_qos.h>
#include <linux/types.h>

enum sm5504_cable_type {
	SM5504_CABLE_NONE_MUIC = 0,
	SM5504_CABLE_SDP_MUIC,		//Max 500mA with Data
	SM5504_CABLE_DCP_MUIC,		//Max 1.5A
	SM5504_CABLE_CDP_MUIC,		//Max 1.5A with Data
	SM5504_CABLE_OTG_MUIC,
	SM5504_CABLE_CARKIT_T1_MUIC,	//Max 500mA with Data (Because of LG USB cable)
	SM5504_CABLE_UART_MUIC,
	SM5504_CABLE_JIG_UART_OFF_MUIC,
	SM5504_CABLE_JIG_UART_OFF_VB_MUIC,
	SM5504_CABLE_JIG_UART_ON_MUIC,
	SM5504_CABLE_JIG_UART_ON_VB_MUIC,
	SM5504_CABLE_JIG_USB_ON_MUIC,
	SM5504_CABLE_JIG_USB_OFF_MUIC,
	SM5504_CABLE_DESKTOP_DOCK_MUIC,
	SM5504_CABLE_DESKTOP_DOCK_VB_MUIC,
	SM5504_CABLE_TYPE_NOTIFY_VBUSPOWER_ON,
	SM5504_CABLE_TYPE_NOTIFY_VBUSPOWER_OFF,
	SM5504_CABLE_PPD_MUIC,
	SM5504_CABLE_UNKNOWN,
};

struct sm5504_platform_data {
	void (*charger_cb) (u8 attached);
	u32 qos_val;
	char *vbat_check_name;
};

#endif /* _SM5504_MUIC_H_ */
