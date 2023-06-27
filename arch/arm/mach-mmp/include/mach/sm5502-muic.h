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

#ifndef _SM5502_MUIC_H_
#define _SM5502_MUIC_H_

#include <linux/types.h>

/* I2C Register */
#define REG_DEVID		0x01
#define REG_CTRL		0x02
#define REG_INT1		0x03
#define REG_INT2		0x04
#define REG_INT1_MASK		0x05
#define REG_INT2_MASK		0x06
#define REG_ADC			0x07
#define REG_TIMING1		0x08
#define REG_TIMING2		0x09
#define REG_DEV_T1		0x0A
#define REG_DEV_T2		0x0B
#define REG_BTN1		0x0C
#define REG_BTN2		0x0D
#define REG_CK_STAT		0x0E
#define REG_MANSW1		0x13
#define REG_MANSW2		0x14
#define REG_DEV_T3		0x15
#define REG_RESET		0x1B
#define REG_RSV_ID1		0x1D
#define REG_RSV_ID2		0x20
#define REG_OCP			0x22
#define REG_RSV_ID3		0x3A

/* Control */
#define SWITCH_OPEN		(1 << 4)
#define RAW_DATA		(1 << 3)
#define MANUAL_SWITCH		(1 << 2)
#define WAIT			(1 << 1)
#define MASK_INT		(1 << 0)
#define CTRL_INIT		(SWITCH_OPEN | RAW_DATA | MANUAL_SWITCH | WAIT)

/* Device Type 1*/
#define DEV_USB_OTG		(1 << 7)
#define DEV_DEDICATED_CHG	(1 << 6)
#define DEV_USB_CHG		(1 << 5)
#define DEV_CARKIT_CHG		(1 << 4)
#define DEV_UART		(1 << 3)
#define DEV_USB			(1 << 2)
#define DEV_AUDIO_2		(1 << 1)
#define DEV_AUDIO_1		(1 << 0)
#define DEV_CHARGER		(DEV_DEDICATED_CHG | DEV_USB_CHG)

/* Device Type 2*/
#define DEV_RESERVED		(1 << 7)
#define DEV_AV			(1 << 6)
#define DEV_TTY			(1 << 5)
#define DEV_PPD			(1 << 4)
#define DEV_JIG_UART_OFF	(1 << 3)
#define DEV_JIG_UART_ON		(1 << 2)
#define DEV_JIG_USB_OFF		(1 << 1)
#define DEV_JIG_USB_ON		(1 << 0)
#define DEV_JIG_ALL		(DEV_JIG_UART_OFF | DEV_JIG_UART_ON | DEV_JIG_USB_OFF | DEV_JIG_USB_ON)

/* Device Type 3 */
#define DEV_U200_CHG		(1 << 6)
#define DEV_AV_VBUS		(1 << 4)
#define DEV_DCD_OUT_SDP		(1 << 2)
#define DEV_MHL			(1 << 0)

/* INT1 */
#define DETACHED		(1 << 1)
#define ATTACHED		(1 << 0)

/* INT2 */
#define VBUSOUT_ON		(1 << 7)
#define REV_ACCE		(1 << 1)
#define VBUSOUT_OFF		(1 << 0)

/* INT Mask 1 */
#define OCP_EVENT_M		(1 << 6)
#define LKR_M			(1 << 4)
#define LKP_M			(1 << 3)
#define KP_M			(1 << 2)
#define INTMASK1_INIT		(OCP_EVENT_M | LKR_M | LKP_M | KP_M)

/* INT Mask 2 */
#define VBUSOUT_ON_M		(1 << 7)
#define MHL_M			(1 << 5)
#define VBUSOUT_OFF_M		(1 << 0)
#define INTMASK2_INIT		(VBUSOUT_ON_M | MHL_M | VBUSOUT_OFF_M)

/* Manual Switch 1 */
#define CON_TO_USB		0x27
#define CON_TO_AUDIO		0x4B
#define AUTO_SWITCH		0x00

/* Reset */
#define IC_RESET		(1 << 0)
#define MAX_RESET_TRIAL		(2)

/* Reserved ID 1 */
#define VBUSIN_VALID		(1 << 1)

/* Reserved ID 3 */
#define CHGPUMP_DIS		(1 << 0)

/* Timing */
#define ADC_DET_T200		0x03

struct sm5502_platform_data {
	int intb_gpio;
	void (*usb_cb) (u8 attached);
	void (*uart_cb) (u8 attached);
	void (*charger_cb) (u8 attached);
	void (*jig_cb) (u8 attached);
	void (*reset_cb) (void);
	void (*mhl_cb) (u8 attached);
	void (*cardock_cb) (bool attached);
	void (*deskdock_cb) (bool attached);
	void (*id_open_cb) (void);
};

enum {
	CABLE_TYPE_NONE_MUIC = 0,
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

/* Control for Analog Audio Dock device */
extern int usb_switch_register_notify(struct notifier_block *nb);
extern int usb_switch_unregister_notify(struct notifier_block *nb);
extern void sm5502_dock_audiopath_ctrl(int on);
extern void sm5502_chgpump_ctrl(int enable);
extern int sm5502_ic_reset(void);

#endif /* _SM5502_MUIC_H_ */
