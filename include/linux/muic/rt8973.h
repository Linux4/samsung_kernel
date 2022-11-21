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

#ifndef __RT8973_H__
#define __RT8973_H__

#include <linux/muic/muic.h>
#include <linux/wakelock.h>

#define MUIC_DEV_NAME	"muic-rt8973"

#define RT8973_IRQF_MODE	(IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND)

enum rt8973_muic_reg {
	RT8973_REG_CHIP_ID		= 0x01,
	RT8973_REG_CONTROL		= 0x02,
	RT8973_REG_INT1			= 0x03,
	RT8973_REG_INT2			= 0x04,
	RT8973_REG_INT_MASK1		= 0x05,
	RT8973_REG_INT_MASK2		= 0x06,
	RT8973_REG_ADC			= 0x23,
	RT8973_REG_DEVICE1		= 0x0A,
	RT8973_REG_DEVICE2		= 0x0B,
	RT8973_REG_MANUAL_SW1		= 0x13,
	RT8973_REG_MANUAL_SW2		= 0x14,
	RT8973_REG_RESET		= 0x1B,
};

#define DCD_T_RETRY			2

/* RT8973 Control register */
#define CTRL_ADC_EN_SHIFT		7
#define CTRL_USBCHDEN_SHIFT		6
#define CTRL_CHG_TYP_SHIFT		5
#define CTRL_SWITCH_OPEN_SHIFT		4
/* CTRL bit 3 is deprecated */
#define CTRL_AUTO_CONFIG_SHIFT		2
/* CTRL bit 1 is RSVD */
#define CTRL_INT_MASK_SHIFT		0
#define CTRL_ADC_EN_MASK		(0x1 << CTRL_ADC_EN_SHIFT)
#define CTRL_USBCHDEN_MASK		(0x1 << CTRL_USBCHDEN_SHIFT)
#define CTRL_CHG_TYP_MASK		(0x1 << CTRL_CHG_TYP_SHIFT)
#define CTRL_SWITCH_OPEN_MASK		(0x1 << CTRL_SWITCH_OPEN_SHIFT)
#define CTRL_AUTO_CONFIG_MASK		(0x1 << CTRL_AUTO_CONFIG_SHIFT)
#define CTRL_INT_MASK_MASK		(0x1 << CTRL_INT_MASK_SHIFT)
#define CTRL_MASK			(CTRL_INT_MASK_MASK | CTRL_AUTO_CONFIG_MASK |\
					CTRL_CHG_TYP_MASK | CTRL_USBCHDEN_MASK | CTRL_ADC_EN_MASK)

/* RT8973 Interrupt 1 register */
#define INT_OTP_SHIFT			7
#define INT_ADC_CHG_SHIFT		6
#define INT_CONNECT_SHIFT		5
#define INT_OVP_SHIFT			4
#define INT_DCD_T_SHIFT			3
#define INT_CHG_DET_SHIFT		2
#define INT_DETACH_SHIFT		1
#define INT_ATTACH_SHIFT		0
#define INT_OTP_MASK			(0x1 << INT_OTP_SHIFT)
#define INT_ADC_CHG_MASK		(0x1 << INT_ADC_CHG_SHIFT)
#define INT_CONNECT_MASK		(0x1 << INT_CONNECT_SHIFT)
#define INT_OVP_MASK			(0x1 << INT_OVP_SHIFT)
#define INT_DCD_T_MASK			(0x1 << INT_DCD_T_SHIFT)
#define INT_CHG_DET_MASK		(0x1 << INT_CHG_DET_SHIFT)
#define INT_DETACH_MASK			(0x1 << INT_DETACH_SHIFT)
#define INT_ATTACH_MASK			(0x1 << INT_ATTACH_SHIFT)

/* RT8973 Interrupt 2 register */
#define INT_OVP_OCP_SHIFT		7
#define INT_OCP_SHIFT			6
#define INT_OCP_LATCH_SHIFT		5
#define INT_OVP_FET_SHIFT		4
#define INT_OTP_FET_SHIFT		3
#define INT_POR_SHIFT			2
#define INT_UVLO_SHIFT			1
/* INT2 bit 0 is RSVD */
#define INT_OVP_OCP_MASK		(0x1 << INT_OVP_OCP_SHIFT)
#define INT_OCP_MASK			(0x1 << INT_OCP_SHIFT)
#define INT_OCP_LATCH_MASK		(0x1 << INT_OCP_LATCH_SHIFT)
#define INT_OVP_FET_MASK		(0x1 << INT_OVP_FET_SHIFT)
#define INT_OTP_FET_MASK		(0x1 << INT_OTP_FET_SHIFT)
#define INT_POR_MASK			(0x1 << INT_POR_SHIFT)
#define INT_UVLO_MASK			(0x1 << INT_UVLO_SHIFT)

/* RT8973 ADC register */
#define ADC_ADC_SHIFT			0
#define ADC_ADC_MASK			(0x1f << ADC_ADC_SHIFT)

/* RT8973 DEVICE1 register */
#define RT8973_DEVICE1_OTG		0x01
#define RT8973_DEVICE1_SDP		(0x1 << 2)
#define RT8973_DEVICE1_UART		(0x1 << 3)
#define RT8973_DEVICE1_CDPORT		(0x1 << 5)
#define RT8973_DEVICE1_DCPORT		(0x1 << 6)
#define RT8973_USB_TYPES		(RT8973_DEVICE1_OTG | RT8973_DEVICE1_SDP | RT8973_DEVICE1_CDPORT)
#define RT8973_CHG_TYPES		(RT8973_DEVICE1_DCPORT | RT8973_DEVICE1_CDPORT)

/* RT8973 DEVICE2 register */
#define RT8973_DEVICE2_JIG_USB_ON	0x01
#define RT8973_DEVICE2_JIG_USB_OFF	(0x1 << 1)
#define RT8973_DEVICE2_JIG_UART_ON	(0x1 << 2)
#define RT8973_DEVICE2_JIG_UART_OFF	(0x1 << 3)
#define RT8973_DEVICE2_UNKNOWN		(0x1 << 7)
#define RT8973_JIG_USB_TYPES		(RT8973_DEVICE2_JIG_USB_ON | RT8973_DEVICE2_JIG_USB_OFF)
#define RT8973_JIG_UART_TYPES		(RT8973_DEVICE2_JIG_UART_OFF)

/*
 * Manual Switch
 * D- [7:5] / D+ [4:2]
 * 000: Open all / 001: USB / 011: UART
 * 00: Vbus to Open / 01: Vbus to Charger / 10: Vbus to MIC / 11: Vbus to VBout
 */
#define MANUAL_SW1_DN_SHIFT	5
#define MANUAL_SW1_DP_SHIFT	2
#define MANUAL_SW_DM_DP_MASK	0xFC
/* bit 1 and bit 0 are RSVD */
#define MANUAL_SW1_D_OPEN	(0x0)
#define MANUAL_SW1_D_USB	(0x1)
/* RT8973 does not have audio path */
#define MANUAL_SW1_D_UART	(0x3)

enum rt8973_switch_sel_val {
	SM5502_SWITCH_SEL_1st_BIT_USB	= (0x1 << 0),
	SM5502_SWITCH_SEL_2nd_BIT_UART	= (0x1 << 1),
};

enum rt8973_reg_manual_sw1_value {
	MANSW1_OPEN =	(MANUAL_SW1_D_OPEN << MANUAL_SW1_DN_SHIFT) | \
			(MANUAL_SW1_D_OPEN << MANUAL_SW1_DP_SHIFT),
	MANSW1_USB =	(MANUAL_SW1_D_USB << MANUAL_SW1_DN_SHIFT) | \
			(MANUAL_SW1_D_USB << MANUAL_SW1_DP_SHIFT),
	MANSW1_UART =	(MANUAL_SW1_D_UART << MANUAL_SW1_DN_SHIFT) | \
			(MANUAL_SW1_D_UART << MANUAL_SW1_DP_SHIFT),
};

typedef enum {
	JIG_USB_BOOT_OFF,
	JIG_USB_BOOT_ON,
	JIG_UART_BOOT_OFF,
	JIG_UART_BOOT_ON,
} jig_type_t;

struct rt8973_status {
	int cable_type;
	int id_adc;
	uint8_t irq_flags[2];
	uint8_t device_reg[2];
	/* Processed useful status
	 * Compare previous and current regs
	 * to get this information */
	union {
		struct {
			uint32_t vbus_status:1;
			uint32_t accessory_status:1;
			uint32_t ocp_status:1;
			uint32_t ovp_status:1;
			uint32_t otp_status:1;
			uint32_t adc_chg_status:1;
			uint32_t cable_chg_status:1;
			uint32_t otg_status:1;
			uint32_t dcdt_status:1;
			uint32_t usb_connect:1;
			uint32_t uart_connect:1;
			uint32_t jig_connect:1;
			uint32_t l200k_usb_connect:1;
			uint32_t dock_status:1;
		};
		uint32_t status;
	};
};

/* muic chip specific internal data structure
 * that setted at muic-xxxx.c file
 */
struct rt8973_muic_data {
	struct device *dev;
	struct i2c_client *i2c; /* i2c addr: 0x4A; MUIC */
	struct mutex muic_mutex;

	/* muic common callback driver internal data */
	struct sec_switch_data *switch_data;

	/* model dependant muic platform data */
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

	/* RT8973 specific*/
	struct delayed_work		dwork;
	struct wake_lock		muic_wake_lock;
	struct rt8973_status		prev_status;
	struct rt8973_status		curr_status;
	int dcdt_retry_count;

	int rev_id;
};

extern unsigned int system_rev;

#endif /* __RT8973_H__ */
