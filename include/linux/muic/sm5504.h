/*
 * Copyright (C) 2016 Samsung Electronics
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

#ifndef __SM5504_H__
#define __SM5504_H__

#include <linux/muic/muic.h>
#define MUIC_DEV_NAME	"muic-sm5504"

/* SM5504 MUIC  registers */
enum {
	SM5504_MUIC_REG_DEV_ID		= 0x01,
	SM5504_MUIC_REG_CTRL		= 0x02,
	SM5504_MUIC_REG_INT1		= 0x03,
	SM5504_MUIC_REG_INT2		= 0x04,
	SM5504_MUIC_REG_INT_MASK1	= 0x05,
	SM5504_MUIC_REG_INT_MASK2	= 0x06,
	SM5504_MUIC_REG_ADC		= 0x07,
	SM5504_MUIC_REG_DEV_TYPE1	= 0x0A,
	SM5504_MUIC_REG_DEV_TYPE2	= 0x0B,
	SM5504_MUIC_REG_MAN_SW1		= 0x13,
	SM5504_MUIC_REG_MAN_SW2		= 0x14,
	SM5504_MUIC_REG_RESET		= 0x1B,
	SM5504_MUIC_REG_CHG		= 0x24,
	SM5504_MUIC_REG_SET		= 0x20,
	SM5504_MUIC_REG_END,
};

/* CONTROL : REG_CTRL */
#define ADC_EN			(1 << 7)
#define USBCHDEN		(1 << 6)
#define CHGTYP			(1 << 5)
#define SWITCH_OPEN		(1 << 4)
#define MANUAL_SWITCH		(1 << 2)
#define MASK_INT		(1 << 0)
#define CTRL_INIT		(ADC_EN | USBCHDEN | CHGTYP | MANUAL_SWITCH)
#define RESET_DEFAULT		(ADC_EN | USBCHDEN | CHGTYP | MANUAL_SWITCH | MASK_INT)

/* INTERRUPT 1 : REG_INT1 */
#define ADC_CHG			(1 << 6)
#define CONNECT			(1 << 5)
#define OVP			(1 << 4)
#define DCD_OUT			(1 << 3)
#define CHGDET			(1 << 2)
#define DETACH			(1 << 1)
#define ATTACH			(1 << 0)

/* INTERRUPT 2 : REG_INT2 */
#define OVP_OCP			(1 << 7)
#define OCP			(1 << 6)
#define OCP_LATCH		(1 << 5)
#define OVP_FET			(1 << 4)
#define POR			(1 << 2)
#define UVLO			(1 << 1)	/* vbus */
#define RID_CHARGER		(1 << 0)

#define INT_UVLO_MASK 		UVLO

/* INTMASK 1 : REG_INT1_MASK */
#define ADC_CHG_M		(1 << 6)
#define CONNECT_M		(1 << 5)
#define OVP_M			(1 << 4)
#define DCD_OUT_M		(1 << 3)
#define CHGDET_M		(1 << 2)
#define DETACH_M		(1 << 1)
#define ATTACH_M		(1 << 0)
#define INTMASK1_INIT		(ADC_CHG_M | CONNECT_M | OVP_M | DCD_OUT_M | CHGDET_M)
#define INTMASK1_CHGDET		(CHGDET_M)

/* INTMASK 2 : REG_INT2_MASK */
#define OVP_OCP_M		(1 << 7)
#define OCP_M			(1 << 6)
#define OCP_LATCH_M		(1 << 5)
#define OVP_FET_M		(1 << 4)
#define POR_M			(1 << 2)
#define UVLO_M			(1 << 1)
#define RID_CHARGER_M		(1 << 0)
#define INTMASK2_INIT		(0x80)
//#define INTMASK2_INIT		(OVP_OCP_M | POR_M | !UVLO_M | RID_CHARGER_M)

/* ADC : REG_ADC */
#define ADC_MASK		(0x1F)

/* DEVICE TYPE 1 : REG_DEV_T1 */
#define DEV_DCP			(1 << 6)	/* Max 1.5A */
#define DEV_CDP			(1 << 5)	/* Max 1.5A with Data */
#define DEV_CARKIT_T1		(1 << 4)
#define DEV_UART		(1 << 3)
#define DEV_SDP			(1 << 2)	/* Max 500mA with Data */
#define DEV_OTG			(1 << 0)
#define DEV_CHARGER		(DEV_DEDICATED_CHG | DEV_USB_CHG)

#define SM5504_DEV_OTG		(DEV_OTG)
#define SM5504_DEV_SDP		(DEV_SDP)
#define SM5504_DEV_UART		(DEV_UART)
#define SM5504_DEV_CDP		(DEV_CDP)
#define SM5504_DEV_DCP		(DEV_DCP)

/* DEVICE TYPE 2 : REG_DEV_T2 */
#define DEV_UNKNOWN		(1 << 7)
#define DEV_JIG_UART_OFF	(1 << 3)
#define DEV_JIG_UART_ON		(1 << 2)
#define DEV_JIG_USB_OFF		(1 << 1)
#define DEV_JIG_USB_ON		(1 << 0)
#define DEV_JIG_ALL		(DEV_JIG_UART_OFF | DEV_JIG_UART_ON | DEV_JIG_USB_OFF | DEV_JIG_USB_ON)
#define DEV_JIG_WAKEUP		(DEV_JIG_UART_OFF | DEV_JIG_UART_ON | DEV_JIG_USB_ON)

#define SM5504_DEV_JIG_USB_ON	(DEV_JIG_USB_ON)
#define SM5504_DEV_JIG_USB_OFF	(DEV_JIG_USB_OFF)
#define SM5504_DEV_JIG_UART_ON	(DEV_JIG_UART_ON)
#define SM5504_DEV_JIG_UART_OFF	(DEV_JIG_UART_OFF)
#define SM5504_DEV_JIG_UNKNOWN	(DEV_UNKNOWN)

/* MANUAL SWITCH 1 : REG_MANSW1
 * D- [7:5] / D+ [4:2]
 * 000: Open all / 001: USB / 011: UART
 */
#define DM_SHIFT		(5)
#define DP_SHIFT		(2)
#define COM_OPEM		(0 << DM_SHIFT) | (0 << DP_SHIFT)
#define COM_TO_USB		(1 << DM_SHIFT) | (1 << DP_SHIFT)	/* 0010 0100 */
#define COM_TO_AUDIO		(2 << DM_SHIFT) | (2 << DP_SHIFT)
#define COM_TO_UART		(3 << DM_SHIFT) | (3 << DP_SHIFT)	/* 0110 1100 */
#define MANUAL_SW1_MASK		(0xFC)

/* MANUAL SWITCH 2 : REG_MANSW2 */
#define BOOT_SW			(1 << 3)
#define JIG_ON			(1 << 2)
#define VBUS_FET_ONOFF		(1 << 0)

/* RESET : REG_RESET */
#define IC_RESET		(1 << 0)

/* Setting Register */
#define VBUS_300MS		(0x06)
#define VBUS_140MS		(0x0E)

/* muic chip specific internal data structure
 * that setted at muic-xxxx.c file
 */
struct sm5504_muic_data {
	struct device *dev;
	struct i2c_client *i2c; /* i2c addr: 0x4A; MUIC */
	struct mutex muic_mutex;

	/* muic common callback driver internal data */
	struct sec_switch_data *switch_data;

	/* model dependancy muic platform data */
	struct muic_platform_data *pdata;

	/* muic support vps list */
	bool muic_support_list[ATTACHED_DEV_NUM];

	/* muic current attached device */
	muic_attached_dev_t     attached_dev;

	/* muic Device ID */
	u8 muic_vendor;			/* Vendor ID */
	u8 muic_version;		/* Version ID */

	bool is_usb_ready;
	bool is_factory_start;
	bool is_rustproof;
	bool is_otg_test;
	bool vbus_ignore;

	/* W/A waiting for the charger ic */
	bool suspended;
	bool need_to_noti;

	struct wake_lock		muic_wake_lock;
	int rev_id;

	// OTG enable W/A for JAVA Rev03 board
	u8 otg_en;

	int vbvolt;
};

extern struct device *switch_device;
extern struct muic_platform_data muic_pdata;

#endif /* __SM5504_H__ */
