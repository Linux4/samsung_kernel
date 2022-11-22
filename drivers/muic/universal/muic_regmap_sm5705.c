/*
 * muic_regmap_sm5705.c
 *
 * Copyright (C) 2014 Samsung Electronics
 * Thomas Ryu <smilesr.ryu@samsung.com>
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

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/host_notify.h>
#include <linux/string.h>
#include <linux/power_supply.h>

#include <linux/ccic/s2mm005.h>
#include <linux/muic/muic.h>
#include <linux/muic/muic_afc.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#include "muic-internal.h"
#include "muic_i2c.h"
#include "muic_regmap.h"
#include "muic_vps.h"
#include "../../battery_v2/include/sec_charging_common.h"

#if defined(CONFIG_MUIC_UNIVERSAL_SM5705_AFC)
extern void muic_afc_delay_check_state(int state);
#endif

extern unsigned int system_rev;
extern muic_is_charger_ready(void);

#define ADC_DETECT_TIME_200MS (0x03)
#define KEY_PRESS_TIME_100MS  (0x00)

enum sm5705_muic_reg_init_value {
	REG_INTMASK1_VALUE	= (0xDC),
	REG_INTMASK2_VALUE	= (0x00),
	REG_INTMASK3AFC_VALUE	= (0x00),
	REG_TIMING1_VALUE	= (ADC_DETECT_TIME_200MS |
				KEY_PRESS_TIME_100MS),
	REG_AFC_HYSTERESIS_OPT	= (0x20),
#if defined(CONFIG_MUIC_SUPPORT_KEYBOARDDOCK)
	/* set the register to attatch RID when VBUS exists. */
	REG_RSVDID1_VALUE	= (0x80),
#endif
#if defined(CONFIG_MUIC_SUPPORT_CCIC)
	REG_RSVDID2_VALUE	= (0x26),
#else
	REG_RSVDID2_VALUE	= (0x24),
#endif
	REG_AFC_SSYNC_CHECK	= (0x40),
	REG_AFC_TXD_VALUE	= (0x46),
};

/* sm5705 I2C registers */
enum sm5705_muic_reg {
	REG_DEVID	= 0x01,
	REG_CTRL	= 0x02,
	REG_INT1	= 0x03,
	REG_INT2	= 0x04,
	REG_INT3	= 0x05,
	REG_INTMASK1	= 0x06,
	REG_INTMASK2	= 0x07,
	REG_INTMASK3	= 0x08,
	REG_ADC		= 0x09,
	REG_DEVT1	= 0x0a,
	REG_DEVT2	= 0x0b,
	REG_DEVT3	= 0x0c,
	REG_TIMING1	= 0x0d,
	REG_TIMING2	= 0x0e,
	/* 0x0f is reserved. */
	REG_BTN1	= 0x10,
	REG_BTN2	= 0x11,
	REG_CarKit	= 0x12,
	REG_MANSW1	= 0x13,
	REG_MANSW2	= 0x14,
	REG_RSVDID1	= 0x15,
	REG_RSVDID2	= 0x16,
	REG_CHGTYPE	= 0x17,

	/* 0x18 ~ 0x21 for AFC  */
	REG_AFCCNTL	= 0x18,
	REG_AFCTXD	= 0x19,
	REG_AFCSTAT	= 0x1a,
	REG_VBUSSTAT	= 0x1b,
	REG_AFCRXD1	= 0x1c,
	REG_AFCRXD2	= 0x1d,
	REG_AFCRXD3	= 0x1e,
	REG_AFCRXD4	= 0x1f,
	REG_AFCRXD5	= 0x20,
	REG_AFCRXD6	= 0x21,

	REG_RESET	= 0x22,
	REG_AFCCNTL2	= 0x32,
	REG_AFC_DMCNTL	= 0x37,
	REG_END,
};

#define REG_ITEM(addr, bitp, mask) ((bitp<<16) | (mask<<8) | addr)

/* Field */
enum sm5705_muic_reg_item {
	DEVID_QC20     = REG_ITEM(REG_DEVID, _BIT3, _MASK1),
	DEVID_VendorID = REG_ITEM(REG_DEVID, _BIT0, _MASK3),

	CTRL_SW_OPEN	= REG_ITEM(REG_CTRL, _BIT4, _MASK1),
	CTRL_RAWDATA	= REG_ITEM(REG_CTRL, _BIT3, _MASK1),
	CTRL_ManualSW	= REG_ITEM(REG_CTRL, _BIT2, _MASK1),
	CTRL_WAIT	= REG_ITEM(REG_CTRL, _BIT1, _MASK1),
	CTRL_MASK_INT	= REG_ITEM(REG_CTRL, _BIT0, _MASK1),

	INT1_DETACH	= REG_ITEM(REG_INT1, _BIT1, _MASK1),
	INT1_ATTACH	= REG_ITEM(REG_INT1, _BIT0, _MASK1),

	INT2_VBUSDET_ON  = REG_ITEM(REG_INT2, _BIT7, _MASK1),
	INT2_RID_CHARGER = REG_ITEM(REG_INT2, _BIT6, _MASK1),
	INT2_MHL         = REG_ITEM(REG_INT2, _BIT5, _MASK1),
	INT2_ADC_CHG     = REG_ITEM(REG_INT2, _BIT2, _MASK1),
	INT2_VBUS_OFF    = REG_ITEM(REG_INT2, _BIT0, _MASK1),

	INT3_AFC_ERROR         = REG_ITEM(REG_INT3, _BIT5, _MASK1),
	INT3_AFC_STA_CHG       = REG_ITEM(REG_INT3, _BIT4, _MASK1),
	INT3_MULTI_BYTE        = REG_ITEM(REG_INT3, _BIT3, _MASK1),
	INT3_VBUS_UPDATE       = REG_ITEM(REG_INT3, _BIT2, _MASK1),
	INT3_AFC_ACCEPTED      = REG_ITEM(REG_INT3, _BIT1, _MASK1),
	INT3_AFC_TA_ATTACHED   = REG_ITEM(REG_INT3, _BIT0, _MASK1),

	INTMASK1_DETACH_M    = REG_ITEM(REG_INTMASK1, _BIT1, _MASK1),
	INTMASK1_ATTACH_M    = REG_ITEM(REG_INTMASK1, _BIT0, _MASK1),

	INTMASK2_VBUSDET_ON_M = REG_ITEM(REG_INTMASK2, _BIT7, _MASK1),
	INTMASK2_RID_CHARGERM = REG_ITEM(REG_INTMASK2, _BIT6, _MASK1),
	INTMASK2_MHL_M        = REG_ITEM(REG_INTMASK2, _BIT5, _MASK1),
	INTMASK2_ADC_CHG_M    = REG_ITEM(REG_INTMASK2, _BIT2, _MASK1),
	INTMASK2_REV_ACCE_M   = REG_ITEM(REG_INTMASK2, _BIT1, _MASK1),
	INTMASK2_VBUS_OFF_M   = REG_ITEM(REG_INTMASK2, _BIT0, _MASK1),

	INT3_AFC_ERROR_M        = REG_ITEM(REG_INTMASK3, _BIT5, _MASK1),
	INT3_AFC_STA_CHG_M      = REG_ITEM(REG_INTMASK3, _BIT4, _MASK1),
	INT3_MULTI_BYTE_M       = REG_ITEM(REG_INTMASK3, _BIT3, _MASK1),
	INT3_VBUS_UPDATE_M      = REG_ITEM(REG_INTMASK3, _BIT2, _MASK1),
	INT3_AFC_ACCEPTED_M     = REG_ITEM(REG_INTMASK3, _BIT1, _MASK1),
	INT3_AFC_TA_ATTACHED_M  = REG_ITEM(REG_INTMASK3, _BIT0, _MASK1),

	ADC_ADC_VALUE  =  REG_ITEM(REG_ADC, _BIT0, _MASK5),

	DEVT1_USB_OTG         = REG_ITEM(REG_DEVT1, _BIT7, _MASK1),
	DEVT1_DEDICATED_CHG   = REG_ITEM(REG_DEVT1, _BIT6, _MASK1),
	DEVT1_USB_CHG         = REG_ITEM(REG_DEVT1, _BIT5, _MASK1),
	DEVT1_CAR_KIT_CHARGER = REG_ITEM(REG_DEVT1, _BIT4, _MASK1),
	DEVT1_UART            = REG_ITEM(REG_DEVT1, _BIT3, _MASK1),
	DEVT1_USB             = REG_ITEM(REG_DEVT1, _BIT2, _MASK1),
	DEVT1_AUDIO_TYPE2     = REG_ITEM(REG_DEVT1, _BIT1, _MASK1),
	DEVT1_AUDIO_TYPE1     = REG_ITEM(REG_DEVT1, _BIT0, _MASK1),

	DEVT2_AV           = REG_ITEM(REG_DEVT2, _BIT6, _MASK1),
	DEVT2_TTY          = REG_ITEM(REG_DEVT2, _BIT5, _MASK1),
	DEVT2_PPD          = REG_ITEM(REG_DEVT2, _BIT4, _MASK1),
	DEVT2_JIG_UART_OFF = REG_ITEM(REG_DEVT2, _BIT3, _MASK1),
	DEVT2_JIG_UART_ON  = REG_ITEM(REG_DEVT2, _BIT2, _MASK1),
	DEVT2_JIG_USB_OFF  = REG_ITEM(REG_DEVT2, _BIT1, _MASK1),
	DEVT2_JIG_USB_ON   = REG_ITEM(REG_DEVT2, _BIT0, _MASK1),

	DEVT3_AFC_TA        = REG_ITEM(REG_DEVT3, _BIT7, _MASK1),
	DEVT3_U200_CHG      = REG_ITEM(REG_DEVT3, _BIT6, _MASK1),
	DEVT3_LO_TA         = REG_ITEM(REG_DEVT3, _BIT5, _MASK1),
	DEVT3_AV_CABLE_VBUS = REG_ITEM(REG_DEVT3, _BIT4, _MASK1),
	DEVT3_DCD_OUT_SDP   = REG_ITEM(REG_DEVT3, _BIT2, _MASK1),
	DEVT3_QC20_TA       = REG_ITEM(REG_DEVT3, _BIT1, _MASK1),	
	DEVT3_MHL           = REG_ITEM(REG_DEVT3, _BIT0, _MASK1),

	TIMING1_KEY_PRESS_T = REG_ITEM(REG_TIMING1, _BIT4, _MASK4),
	TIMING1_ADC_DET_T   = REG_ITEM(REG_TIMING1, _BIT0, _MASK4),

	TIMING2_SW_WAIT_T  = REG_ITEM(REG_TIMING2, _BIT4, _MASK4),
	TIMING2_LONG_KEY_T = REG_ITEM(REG_TIMING2, _BIT0, _MASK4),

	MANSW1_DM_CON_SW    =  REG_ITEM(REG_MANSW1, _BIT5, _MASK3),
	MANSW1_DP_CON_SW    =  REG_ITEM(REG_MANSW1, _BIT2, _MASK3),

	MANSW2_BOOT_SW      =  REG_ITEM(REG_MANSW2, _BIT3, _MASK1),
	MANSW2_JIG_ON       =  REG_ITEM(REG_MANSW2, _BIT2, _MASK1),
	MANSW2_SINGLE_MODE  =  REG_ITEM(REG_MANSW2, _BIT1, _MASK1),
	MANSW2_ID_SW        =  REG_ITEM(REG_MANSW2, _BIT0, _MASK1),

	RSVDID1_VBUS_VALID    = REG_ITEM(REG_RSVDID1, _BIT0, _MASK1),
	RSVDID2_DCD_TIME_EN   = REG_ITEM(REG_RSVDID2, _BIT2, _MASK1),
	RSVDID2_BCD_RESCAN    = REG_ITEM(REG_RSVDID2, _BIT4, _MASK1),
	RSVDID2_CHGPUMP_nEN   = REG_ITEM(REG_RSVDID2, _BIT5, _MASK1),
	CHGTYPE_CHG_TYPE      = REG_ITEM(REG_CHGTYPE, _BIT0, _MASK5),

	AFCCNTL_ENQC20      = REG_ITEM(REG_AFCCNTL, _BIT6, _MASK1),
	AFCCNTL_DISAFC      = REG_ITEM(REG_AFCCNTL, _BIT5, _MASK1),
	AFCCNTL_VBUS_READ   = REG_ITEM(REG_AFCCNTL, _BIT3, _MASK1),
	AFCCNTL_DM_RESET    = REG_ITEM(REG_AFCCNTL, _BIT2, _MASK1),
	AFCCNTL_DP_RESET    = REG_ITEM(REG_AFCCNTL, _BIT1, _MASK1),
	AFCCNTL_ENAFC       = REG_ITEM(REG_AFCCNTL, _BIT0, _MASK1),

	AFCTXD_VOLTAGE    = REG_ITEM(REG_AFCTXD, _BIT4, _MASK4),
	AFCTXD_CURRENT    = REG_ITEM(REG_AFCTXD, _BIT0, _MASK4),
	AFCSTAT_STATUS    = REG_ITEM(REG_AFCSTAT, _BIT0, _MASK8),
	VBUSSTAT_STATUS   = REG_ITEM(REG_VBUSSTAT, _BIT0, _MASK4),

	AFCRXD1_DATA   = REG_ITEM(REG_AFCRXD1, _BIT0, _MASK8),
	AFCRXD2_DATA   = REG_ITEM(REG_AFCRXD2, _BIT0, _MASK8),
	AFCRXD3_DATA   = REG_ITEM(REG_AFCRXD3, _BIT0, _MASK8),
	AFCRXD4_DATA   = REG_ITEM(REG_AFCRXD4, _BIT0, _MASK8),
	AFCRXD5_DATA   = REG_ITEM(REG_AFCRXD5, _BIT0, _MASK8),
	AFCRXD6_DATA   = REG_ITEM(REG_AFCRXD6, _BIT0, _MASK8),

	RESET_RESET = REG_ITEM(REG_RESET, _BIT0, _MASK1),
};

/* sm5705 Control register */
#define CTRL_SWITCH_OPEN_SHIFT	4
#define CTRL_RAW_DATA_SHIFT		3
#define CTRL_MANUAL_SW_SHIFT		2
#define CTRL_WAIT_SHIFT		1
#define CTRL_INT_MASK_SHIFT		0

#define CTRL_SWITCH_OPEN_MASK	(0x1 << CTRL_SWITCH_OPEN_SHIFT)
#define CTRL_RAW_DATA_MASK		(0x1 << CTRL_RAW_DATA_SHIFT)
#define CTRL_MANUAL_SW_MASK		(0x1 << CTRL_MANUAL_SW_SHIFT)
#define CTRL_WAIT_MASK		(0x1 << CTRL_WAIT_SHIFT)
#define CTRL_INT_MASK_MASK		(0x1 << CTRL_INT_MASK_SHIFT)
#define CTRL_MASK		(CTRL_SWITCH_OPEN_MASK | CTRL_RAW_DATA_MASK | \
				/*CTRL_MANUAL_SW_MASK |*/ CTRL_WAIT_MASK | \
				CTRL_INT_MASK_MASK)
#define DEV_TYPE3_AFC_TA		(0x1 << 7)

#define SM5705_CHG_TYPE_NC		0x00
#define SM5705_CHG_TYPE_DCP		0x01
#define SM5705_CHG_TYPE_CDP		0x02
#define SM5705_CHG_TYPE_SDP		0x04
#define SM5705_CHG_TYPE_TIMEOUT_SDP	0x08
#define SM5705_CHG_TYPE_U200		0x10

#define SM5705_AFC_TA_ATTACHED		0x01
#define SM5705_DT1_DCP			0x40

struct reg_value_set {
	int value;
	char *alias;
};

/* ADC Scan Mode Values : b'1 */
static struct reg_value_set sm5705_adc_scanmode_tbl[] = {
	[ADC_SCANMODE_CONTINUOUS] = {0x01, "Periodic"},
	[ADC_SCANMODE_ONESHOT]    = {0x00, "Oneshot."},
	[ADC_SCANMODE_PULSE]      = {0x00, "Oneshot.."},
};

/*
 * Manual Switch
 * D- [7:5] / D+ [4:2] / Reserved [1:0]
 * 000: Open all / 001: USB / 010: AUDIO / 011: UART / 100: V_AUDIO
 * No Vbus switching in SM5705
 */
#define _D_OPEN	(0x0)
#define _D_USB		(0x1)
#define _D_AUDIO	(0x2)
#define _D_UART	(0x3)
//#define _V_OPEN	(0x0)
//#define _V_CHARGER	(0x1)
//#define _V_MIC		(0x2)

/* COM patch Values */
#define COM_VALUE(dm) ((dm<<5) | (dm<<2))

#define _COM_OPEN		COM_VALUE(_D_OPEN)
//#define _COM_OPEN_WITH_V_BUS	COM_VALUE(_D_OPEN, _V_CHARGER)
#define _COM_UART_AP		COM_VALUE(_D_UART)
#define _COM_UART_CP		_COM_UART_AP
#define _COM_USB_AP		COM_VALUE(_D_USB)
#define _COM_USB_CP		_COM_USB_AP
#define _COM_AUDIO		COM_VALUE(_D_AUDIO)

static int sm5705_com_value_tbl[] = {
	[COM_OPEN]		= _COM_OPEN,
//	[COM_OPEN_WITH_V_BUS]	= _COM_OPEN_WITH_V_BUS,
	[COM_UART_AP]		= _COM_UART_AP,
	[COM_UART_CP]		= _COM_UART_CP,
	[COM_USB_AP]		= _COM_USB_AP,
	[COM_USB_CP]		= _COM_USB_CP,
	[COM_AUDIO]		= _COM_AUDIO,
};

#define REG_CTRL_INITIAL (CTRL_MASK | CTRL_MANUAL_SW_MASK)

static regmap_t sm5705_muic_regmap_table[] = {
	[REG_DEVID]	= {"DeviceID",	0x01, 0x00, INIT_NONE},
#if defined(CONFIG_MUIC_SUPPORT_CCIC)		
	[REG_CTRL]	= {"CONTROL",		0x1F, 0x00, INIT_NONE,},
#else
	[REG_CTRL]	= {"CONTROL",		0x1F, 0x00, REG_CTRL_INITIAL,},
#endif
	[REG_INT1]	= {"INT1",		0x00, 0x00, INIT_NONE,},
	[REG_INT2]	= {"INT2",		0x00, 0x00, INIT_NONE,},
	[REG_INT3]	= {"INT3_AFC",		0x00, 0x00, INIT_NONE,},
	[REG_INTMASK1]	= {"INTMASK1",	0x00, 0x00, REG_INTMASK1_VALUE,},
	[REG_INTMASK2]	= {"INTMASK2",	0x00, 0x00, REG_INTMASK2_VALUE,},
	[REG_INTMASK3]	= {"INTMASK3_AFC",	0x00, 0x00, REG_INTMASK3AFC_VALUE,},
	[REG_ADC]	= {"ADC",		0x1F, 0x00, INIT_NONE,},
	[REG_DEVT1]	= {"DEVICETYPE1",	0xFF, 0x00, INIT_NONE,},
	[REG_DEVT2]	= {"DEVICETYPE2",	0xFF, 0x00, INIT_NONE,},
	[REG_DEVT3]	= {"DEVICETYPE3",	0xFF, 0x00, INIT_NONE,},
	[REG_TIMING1]	= {"TimingSet1",	0x00, 0x00, REG_TIMING1_VALUE,},
	[REG_TIMING2]	= {"TimingSet2",	0x00, 0x00, INIT_NONE,},
	/* 0x0F: Reserved */
	[REG_BTN1]	= {"BUTTON1",		0xFF, 0x00, INIT_NONE,},
	[REG_BTN2]	= {"BUTTON2",		0xFF, 0x00, INIT_NONE,},
	[REG_CarKit]	= {"CarKitStatus",	0x00, 0x00, INIT_NONE,},
	[REG_MANSW1]	= {"ManualSW1",	0x00, 0x00, INIT_NONE,},
	[REG_MANSW2]	= {"ManualSW2",	0x00, 0x00, INIT_NONE,},
#if defined(CONFIG_MUIC_SUPPORT_KEYBOARDDOCK)
	[REG_RSVDID1]	= {"Reserved_ID1",	0xFF, 0x00, REG_RSVDID1_VALUE,},
#else
	[REG_RSVDID1]	= {"Reserved_ID1",	0xFF, 0x00, INIT_NONE,},
#endif
	[REG_RSVDID2]	= {"Reserved_ID2",	0x24, 0x00, REG_RSVDID2_VALUE,},
	[REG_CHGTYPE]	= {"REG_CHG_TYPE",	0xFF, 0x00, INIT_NONE,},
	/* 0x18 ~ 0x21: AFC */
	[REG_AFCCNTL]	= {"AFC_CNTL",	0x00, 0x00, INIT_NONE,},
	[REG_AFCTXD]	= {"AFC_TXD",	0x00, 0x00, REG_AFC_TXD_VALUE,}, // 9V , 1.65A
	[REG_AFCSTAT]	= {"AFC_STATUS",	0xFF, 0x00, INIT_NONE,},
	[REG_VBUSSTAT]	= {"VBUS_STATUS",	0x00, 0x00, INIT_NONE,},
	[REG_AFCRXD1]	= {"AFC_RXD1",	0xFF, 0x00, INIT_NONE,},
	[REG_AFCRXD2]	= {"AFC_RXD2",	0xFF, 0x00, INIT_NONE,},
	[REG_AFCRXD3]	= {"AFC_RXD3",	0xFF, 0x00, INIT_NONE,},
	[REG_AFCRXD4]	= {"AFC_RXD4",	0xFF, 0x00, INIT_NONE,},
	[REG_AFCRXD5]	= {"AFC_RXD5",	0xFF, 0x00, INIT_NONE,},
	[REG_AFCRXD6]	= {"AFC_RXD6",	0xFF, 0x00, INIT_NONE,},
	[REG_RESET]	= {"RESET",		0x00, 0x00, INIT_NONE,},
	[REG_AFCCNTL2]	= {"AFC_CNTL2",	0x00, 0x00, REG_AFC_SSYNC_CHECK,},
	[REG_AFC_DMCNTL]= {"AFC_DMCNTL",0x00, 0x00, REG_AFC_HYSTERESIS_OPT,},
	[REG_END]	= {NULL, 0, 0, INIT_NONE},
};

static int sm5705_muic_ioctl(struct regmap_desc *pdesc,
		int arg1, int *arg2, int *arg3)
{
	int ret = 0;

	switch (arg1) {
	case GET_COM_VAL:
		*arg2 = sm5705_com_value_tbl[*arg2];
		*arg3 = REG_MANSW1;
		break;
	case GET_CTLREG:
		*arg3 = REG_CTRL;
		break;

	case GET_ADC:
		*arg3 = ADC_ADC_VALUE;
		break;

	case GET_SWITCHING_MODE:
		*arg3 = CTRL_ManualSW;
		break;

	case GET_INT_MASK:
		*arg3 = CTRL_MASK_INT;
		break;

	case GET_REVISION:
		*arg3 = DEVID_VendorID;
		break;

	case GET_OTG_STATUS:
		*arg3 = INTMASK2_VBUS_OFF_M;
		break;

	case GET_CHGTYPE:
		*arg3 = CHGTYPE_CHG_TYPE;
		break;

	case GET_RESID3:
		*arg3 = RSVDID2_BCD_RESCAN;
		break;

	default:
		ret = -1;
		break;
	}

	if (pdesc->trace) {
		pr_info("%s: ret:%d arg1:%x,", __func__, ret, arg1);

		if (arg2)
			pr_info(" arg2:%x,", *arg2);

		if (arg3)
			pr_info(" arg3:%x - %s", *arg3,
				regmap_to_name(pdesc, _ATTR_ADDR(*arg3)));
		pr_info("\n");
	}

	return ret;
}

static int sm5705_run_chgdet(struct regmap_desc *pdesc, bool started)
{
#if !defined(CONFIG_MUIC_SUPPORT_KEYBOARDDOCK)
	int attr, value, ret;

	pr_info("%s: start. %s\n", __func__, started ? "enabled": "disabled");

	attr = MANSW1_DM_CON_SW;
	value = 0;
	ret = regmap_write_value(pdesc, attr, value);
	if (ret < 0)
		pr_err("%s Reset reg write fail.\n", __func__);
	else
		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

	pr_info("%s: reset [DP]\n", __func__);
	attr = MANSW1_DP_CON_SW;
	value = 0;
	ret = regmap_write_value(pdesc, attr, value);
	if (ret < 0)
		pr_err("%s Reset reg write fail.\n", __func__);
	else
		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

	pr_info("%s: reset [MannualSW]\n", __func__);
	attr = CTRL_ManualSW;
	value = 0;
	ret = regmap_write_value(pdesc, attr, value);
	if (ret < 0)
		pr_err("%s Reset reg write fail.\n", __func__);
	else
		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

	pr_info("%s: reset [RESET]\n", __func__);
	attr = RESET_RESET;
	value = started ? 1 : 0;
	ret = regmap_write_value_ex(pdesc, attr, value);
	if (ret < 0)
		pr_err("%s Reset reg write fail.\n", __func__);
	else
		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

	pr_info("%s: init all register\n", __func__);
	muic_reg_init(pdesc->muic);

	pr_info("%s: enable INT\n", __func__);
	set_int_mask(pdesc->muic, 0);
	return ret;
#else
	int attr = 0, ret = 0;

	if (started) {
		attr = RSVDID2_BCD_RESCAN;
		ret = regmap_write_value(pdesc, attr, 1);
		if (ret < 0)
			pr_err("%s BCD_RESCAN write fail.\n", __func__);
		else
			_REGMAP_TRACE(pdesc, 'w', ret, attr, 1);

		msleep(1);

		ret = regmap_write_value(pdesc, attr, 0);
		if (ret < 0)
			pr_err("%s BCD_RESCAN write fail.\n", __func__);
		else
			_REGMAP_TRACE(pdesc, 'w', ret, attr, 0);
	}
	return ret;
#endif
}

static int sm5705_attach_ta(struct regmap_desc *pdesc)
{
	pr_info("%s\n", __func__);
	return 0;
}

static int sm5705_detach_ta(struct regmap_desc *pdesc)
{
	pr_info("%s\n", __func__);
	return 0;
}

static int sm5705_set_rustproof(struct regmap_desc *pdesc, int op)
{
	int attr = 0, value = 0, ret = 0;

	pr_info("%s\n", __func__);

	do {
		attr = MANSW2_JIG_ON;
		value = op ? 1 : 0;
		ret = regmap_write_value(pdesc, attr, value);
		if (ret < 0) {
			pr_err("%s MANSW2_JIG_ON write fail.\n", __func__);
			break;
		}

		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

		attr = CTRL_ManualSW;
		value = op ? 0 : 1;
		ret = regmap_write_value(pdesc, attr, value);
		if (ret < 0) {
			pr_err("%s CTRL_ManualSW write fail.\n", __func__);
			break;
		}

		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

	} while (0);

	return ret;
}
#if defined(CONFIG_MUIC_SUPPORT_KEYBOARDDOCK)
static int chgtype_sm_to_maxim(int chgtyp)
{
	int ret = 0;

	switch (chgtyp) {
	case 0x01: // DCP
	case 0x10: // U200
	case 0x11: // AFC
	case 0x12: // LO TA
	case 0x13: // QC2.0
		ret = CHGTYP_DEDICATED_CHARGER;
		break;
	case 0x02: // CDP
		ret = CHGTYP_CDP;
		break;
	case 0x04: // SDP
	case 0x08: // Time out SDP
		ret = CHGTYP_USB;
		break;
	default:
		break;	
	}

	pr_info("%s: 0x%x -> 0x%x\n", __func__, chgtyp, ret);

	return ret;
}
#endif
static int sm5705_get_vps_data(struct regmap_desc *pdesc, void *pbuf)
{
	muic_data_t *pmuic = pdesc->muic;
	vps_data_t *pvps = (vps_data_t *)pbuf;
	int attr;
#if defined(CONFIG_MUIC_SUPPORT_KEYBOARDDOCK)
	int chgtyp;
#endif

	if (pdesc->trace)
		pr_info("%s\n", __func__);

	*(u8 *)&pvps->s.val1 = muic_i2c_read_byte(pmuic->i2c, REG_DEVT1);
	*(u8 *)&pvps->s.val2 = muic_i2c_read_byte(pmuic->i2c, REG_DEVT2);
	*(u8 *)&pvps->s.val3 = muic_i2c_read_byte(pmuic->i2c, REG_DEVT3);
	*(u8 *)&pvps->s.chgtyp = muic_i2c_read_byte(pmuic->i2c, REG_CHGTYPE);

	attr = RSVDID1_VBUS_VALID;
	*(u8 *)&pvps->s.vbvolt = regmap_read_value(pdesc, attr);

	attr = ADC_ADC_VALUE;
	*(u8 *)&pvps->s.adc = regmap_read_value(pdesc, attr);
#if defined(CONFIG_MUIC_SUPPORT_KEYBOARDDOCK)
	attr = CHGTYPE_CHG_TYPE;
	chgtyp = regmap_read_value(pdesc, attr);
	*(u8 *)&pvps->s.chgtyp = chgtype_sm_to_maxim(chgtyp);
#endif
	return 0;
}
/*
static int sm5705_muic_enable_accdet(struct regmap_desc *pdesc)
{
	int ret = 0;
	return ret;
}
static int sm5705_muic_disable_accdet(struct regmap_desc *pdesc)
{
	int ret = 0;
	return ret;
}
*/
static int sm5705_get_adc_scan_mode(struct regmap_desc *pdesc)
{
	struct reg_value_set *pvset;
	int attr, value, mode = 0;

	attr = MANSW2_SINGLE_MODE;
	value = regmap_read_value(pdesc, attr);

	for ( ; mode <ARRAY_SIZE(sm5705_adc_scanmode_tbl); mode++) {
		pvset = &sm5705_adc_scanmode_tbl[mode];
		if (pvset->value == value)
			break;
	}

	pr_info("%s: [%2d]=%02x,%02x\n", __func__, mode, value, pvset->value);
	pr_info("%s:       %s\n", __func__, pvset->alias);

	return mode;
}

static void sm5705_set_adc_scan_mode(struct regmap_desc *pdesc,
		const int mode)
{
	struct reg_value_set *pvset;
	int attr, ret, value;

	if (mode > ADC_SCANMODE_PULSE) {
		pr_err("%s Out of range(%d).\n", __func__, mode);
		return;
	}

	pvset = &sm5705_adc_scanmode_tbl[mode];
	pr_info("%s: [%2d] %s\n", __func__, mode, pvset->alias);

	do {
		value = pvset->value;
		attr = MANSW2_SINGLE_MODE;
		ret = regmap_write_value(pdesc, attr, value);
		if (ret < 0) {
			pr_err("%s MANSW2_SINGLE_MODE write fail.\n", __func__);
			break;
		}

		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

#define _ENABLE_PERIODIC_SCAN (0)
#define _DISABLE_PERIODIC_SCAN (1)

		attr = CTRL_RAWDATA;
		if (mode == ADC_SCANMODE_CONTINUOUS)
			value = _ENABLE_PERIODIC_SCAN;
		else
			value = _DISABLE_PERIODIC_SCAN;

		ret = regmap_write_value(pdesc, attr, value);
		if (ret < 0) {
			pr_err("%s CTRL_RAWDATA write fail.\n", __func__);
			break;
		}

		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

        } while (0);
}

enum switching_mode_val{
	_SWMODE_AUTO =1,
	_SWMODE_MANUAL =0,
};

static int sm5705_get_switching_mode(struct regmap_desc *pdesc)
{
	int attr, value, mode;

	pr_info("%s\n",__func__);

	attr = CTRL_ManualSW;
	value = regmap_read_value(pdesc, attr);

	mode = (value == _SWMODE_AUTO) ? SWMODE_AUTO : SWMODE_MANUAL;

	return mode;
}

static void sm5705_set_switching_mode(struct regmap_desc *pdesc, int mode)
{
        int attr, value;
	int ret = 0;

	pr_info("%s\n",__func__);

	value = (mode == SWMODE_AUTO) ? _SWMODE_AUTO : _SWMODE_MANUAL;
	attr = CTRL_ManualSW;
	ret = regmap_write_value(pdesc, attr, value);
	if (ret < 0)
		pr_err("%s REG_CTRL write fail.\n", __func__);
	else
		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);
}

static int sm5705_run_BCD_rescan(struct regmap_desc *pdesc, int value)
{
	int attr, ret;

	attr = RSVDID2_BCD_RESCAN;

	ret = regmap_write_value(pdesc, attr, value);
	if (ret < 0)
		pr_err("%s BCD_RESCAN write fail.\n", __func__);
	else
		_REGMAP_TRACE(pdesc, 'w', ret, attr, value);

	return ret;
}

#define VBUS_OFFSET		5
static int sm5705_get_vbus_value(struct regmap_desc *pdesc)
{
	muic_data_t *muic = pdesc->muic;
	u8 val = -1;
	int ret;
	int attr, value;
	int i, retry_cnt = 20;

	/* write '1' to INT3_VBUS_UPDATE_M: masking */
	attr = INT3_VBUS_UPDATE_M;
	value = 1;
	ret = regmap_write_value(pdesc, attr, value);
	if (ret < 0) {
		pr_err("%s: failed to set VBUS_UPDATE_M to 1\n", __func__);
		return -EINVAL;
	}


	/* write '1' -> '0' to VBUS_READ of AFC_CNTL */
	attr = AFCCNTL_VBUS_READ;
	value = 1;
	ret = regmap_write_value(pdesc, attr, value);
	if (ret < 0) {
		pr_err("%s: failed to set VBUS_READ to 1\n", __func__);
		return -EINVAL;
	}

	msleep(20);

	value = 0;
	ret = regmap_write_value(pdesc, attr, value);
	if (ret < 0) {
		pr_err("%s: failed to set VBUS_READ to 0\n", __func__);
		return -EINVAL;
	}


	/* check VBUS_UPDATE */
	attr = INT3_VBUS_UPDATE;
	for (i = 0 ; i < retry_cnt ; i++) {
		ret = regmap_read_value(pdesc, attr);
		if (ret > 0)
			break;
		mdelay(50);
	}

	/* read VBUS Value */
	if (ret && i<retry_cnt) {
		val = muic_i2c_read_byte(muic->i2c, REG_VBUSSTAT);
		val &= 0xF;
		pr_info("%s: vbus:%d\n", __func__, val);
	}

	/* write '0' to INT3_VBUS_UPDATE_M: unmasking */
	attr = INT3_VBUS_UPDATE_M;
	value = 0;
	ret = regmap_write_value(pdesc, attr, value);
	if (ret < 0) {
		pr_err("%s: failed to set VBUS_UPDATE_M to 0\n", __func__);
		return -EINVAL;
	}

	if (val > 0xF)
		return -EINVAL;

	/* H/W reqest: 8V return 9V */
	if (val == 3)
		val++;

	return val + VBUS_OFFSET;
}

#define DCD_OUT_SDP	(1 << 2)

static bool sm5705_get_dcdtmr_irq(struct regmap_desc *pdesc)
{
	muic_data_t *muic = pdesc->muic;
	int ret;

	ret = muic_i2c_read_byte(muic->i2c, REG_DEVT3);
	if (ret < 0) {
		pr_err("%s: failed to read REG_DEVT3\n", __func__);
		return false;
	}

	pr_info("%s: REG_DEVT3: 0x%02x\n", __func__, ret);

	if (ret & DCD_OUT_SDP)
		return true;

	return false;
}

static void sm5705_get_fromatted_dump(struct regmap_desc *pdesc, char *mesg)
{
	muic_data_t *muic = pdesc->muic;
	int val;

	if (pdesc->trace)
		pr_info("%s\n", __func__);

	val = i2c_smbus_read_byte_data(muic->i2c, REG_CTRL);
	sprintf(mesg, "CT:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_INTMASK1);
	sprintf(mesg+strlen(mesg), "IM1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_INTMASK2);
	sprintf(mesg+strlen(mesg), "IM2:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_MANSW1);
	sprintf(mesg+strlen(mesg), "MS1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_MANSW2);
	sprintf(mesg+strlen(mesg), "MS2:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_ADC);
	sprintf(mesg+strlen(mesg), "ADC:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_DEVT1);
	sprintf(mesg+strlen(mesg), "DT1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_DEVT2);
	sprintf(mesg+strlen(mesg), "DT2:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_DEVT3);
	sprintf(mesg+strlen(mesg), "DT3:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_RSVDID1);
	sprintf(mesg+strlen(mesg), "RS1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, REG_RSVDID2);
	sprintf(mesg+strlen(mesg), "RS2:%x", val);
}

#ifdef CONFIG_MUIC_SM5705_SWITCH_CONTROL_GPIO
static int sm5705_prepare_switch(muic_data_t *pmuic, int port)
{
	int sm5705_switch_gpio = pmuic->sm5705_switch_gpio;
	int sm5705_switch_val;
	int ret;
	
	pr_info("%s: port = %d\n", __func__, port);

	ret = gpio_request(sm5705_switch_gpio, "SM5705_SWITCH_GPIO");
	if (ret) {
		pr_err("failed to gpio_request SM5705_SWITCH_GPIO\n");
		return 0;
	}

	sm5705_switch_val = gpio_get_value(sm5705_switch_gpio);

	gpio_free(sm5705_switch_gpio);

	pr_info("%s: SM5705_SWITCH_GPIO(%d)=%c\n", __func__, sm5705_switch_gpio,
			(sm5705_switch_val == 0 ? 'L' : 'H'));

	return 0;
}
#endif

static int sm5705_get_sizeof_regmap(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	return (int)ARRAY_SIZE(sm5705_muic_regmap_table);
}

static int sm5705_set_afc_ctrl_reg(struct regmap_desc *pdesc, int shift, bool on)
{
	muic_data_t *pmuic = pdesc->muic;
	struct i2c_client *i2c = pmuic->i2c;
	u8 reg_val;
	int ret = 0;

	pr_info("%s: Register[%d], set [%d]\n", __func__, shift, on);
	ret = muic_i2c_read_byte(i2c, REG_AFCCNTL);
	if (ret < 0)
		printk(KERN_ERR "[muic] %s(%d)\n", __func__, ret);
	if (on)
		reg_val = ret | (0x1 << shift);
	else
		reg_val = ret & ~(0x1 << shift);

	if (reg_val ^ ret) {
		printk(KERN_DEBUG "[muic] %s reg_val(0x%x)!=AFC_CTRL reg(0x%x), update reg\n",
			__func__, reg_val, ret);

		ret = muic_i2c_write_byte(i2c, REG_AFCCNTL,
				reg_val);
		if (ret < 0)
			printk(KERN_ERR "[muic] %s err write AFC_CTRL(%d)\n",
					__func__, ret);
	} else {
		printk(KERN_DEBUG "[muic] %s (0x%x), just return\n",
				__func__, ret);
		return 0;
	}

	ret = muic_i2c_read_byte(i2c, REG_AFCCNTL);
	if (ret < 0)
		printk(KERN_ERR "[muic] %s err read AFC_CTRL(%d)\n",
			__func__, ret);
	else
		printk(KERN_DEBUG "[muic] %s AFC_CTRL reg after change(0x%x)\n",
			__func__, ret);

	return ret;
}

static int afc_vbus_retry_count = 0;
static int afc_multibyte_retry_count = 0;

#ifdef CONFIG_MUIC_SM5705_AFC_18W_TA_SUPPORT
static void sm5705_reset_multibyte_retry_count(struct regmap_desc *pdesc)
{
	pr_info("%s:%s afc_multibyte_retry_count = 0 \n",MUIC_DEV_NAME, __func__);

	afc_multibyte_retry_count = 0;
}
#endif

static int sm5705_afc_ta_attach(struct regmap_desc *pdesc)
{
	muic_data_t *pmuic = pdesc->muic;
	struct i2c_client *i2c = pmuic->i2c;
	union power_supply_propval val;
	int ret, value;
	union power_supply_propval temp; 
	int retry = 0; 


	pr_info("%s:%s AFC_TA_ATTACHED \n",MUIC_DEV_NAME, __func__);

	if (pmuic->pdata->afc_disable) {
		pr_info("%s:%s AFC is disabled in settings menu\n",
						MUIC_DEV_NAME, __func__);
		pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC;
		muic_notifier_attach_attached_dev(ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC);
		return 0;
	}

	// read clear : AFC_STATUS
	value = muic_i2c_read_byte(i2c, REG_AFCSTAT);
	if (value < 0)
		printk(KERN_ERR "%s: err read AFC_STATUS %d\n", __func__, value);
	pr_info("%s:%s AFC_STATUS [0x%02x]\n",MUIC_DEV_NAME, __func__, value);

#if defined(CONFIG_MUIC_UNIVERSAL_SM5705_AFC)
	muic_afc_delay_check_state(0);
#endif

	if (pmuic->is_flash_on) {
		pmuic->afc_retry_count = 0;
		afc_vbus_retry_count = 0;
		
		pr_info("%s:%s FLASH On, Skip AFC\n",MUIC_DEV_NAME, __func__);
		pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_AFC_CHARGER_5V_MUIC;
		muic_notifier_attach_attached_dev(ATTACHED_DEV_AFC_CHARGER_5V_MUIC);
		return 0;
	}
	pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC;
	muic_notifier_attach_attached_dev(ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC);

	for (retry = 0; retry < 20; retry++) { 
		msleep(50); 
		psy_do_property("sm5705-charger", get, POWER_SUPPLY_PROP_CURRENT_MAX, temp);     
		if (temp.intval <= 500) { 
			pr_info("[%s:%s]PREPARE Success(%d mA)\n",MUIC_DEV_NAME, __func__, temp.intval); 
			break; 
		} else { 
			pr_info("[%s:%s]PREPARE fail(%d mA)\n",MUIC_DEV_NAME, __func__, temp.intval); 
		} 
	} 

	cancel_delayed_work(&pmuic->afc_retry_work);
	schedule_delayed_work(&pmuic->afc_retry_work, msecs_to_jiffies(5000)); // 5sec

#ifdef CONFIG_MUIC_SM5705_AFC_18W_TA_SUPPORT
    pr_info("%s:%s pmuic->is_18w_ta = %d \n",MUIC_DEV_NAME, __func__,pmuic->is_18w_ta);

    if (pmuic->is_18w_ta == 0) {
        // voltage(9.0V) + current(1.95A) setting : 0x48
        value = 0x48;
    } else {
        value = muic_i2c_read_byte(i2c, REG_AFCTXD);
        pr_info("%s:%s AFC_TXD read [0x%02x]\n",MUIC_DEV_NAME, __func__, value);
    }
#else
	// voltage(9.0V) + current(1.65A) setting : 0x46
	value = 0x46;
#endif
	psy_do_property("battery", get,
			POWER_SUPPLY_EXT_PROP_SUB_PBA_TEMP_REC, val);

	pr_info("%s: TUSB(%d)\n", __func__, val.intval); // val.intval == 1 ? Recovered : Not recovered

	if(!val.intval) {
		pr_info("%s:%s Working Tusb W/A\n", MUIC_DEV_NAME, __func__);
		if (muic_is_charger_ready()) {
			pmuic->pdata->afc_limit_voltage = true;
		} else {
			pr_info("%s:%s battery driver has not been initialized yet.\n", MUIC_DEV_NAME, __func__);
		}

		muic_dpreset_afc();
		pmuic->attached_dev = ATTACHED_DEV_NONE_MUIC;
		muic_notifier_detach_attached_dev(pmuic->attached_dev);
		msleep(500);
		pmuic->attached_dev = ATTACHED_DEV_TA_MUIC;
		muic_notifier_attach_attached_dev(pmuic->attached_dev);
		
		return 0;
	} else
		ret = muic_i2c_write_byte(i2c, REG_AFCTXD, value);

	if (ret < 0)
		printk(KERN_ERR "[muic] %s: err write AFC_TXD(%d)\n", __func__, ret);
	pr_info("%s:%s AFC_TXD [0x%02x]\n",MUIC_DEV_NAME, __func__, value);

	// ENAFC set '1'
	pr_info("%s: Register[%d]\n", __func__, AFCCTRL_ENAFC);
	sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_ENAFC, 1);
	pmuic->afc_retry_count = 0;
	afc_vbus_retry_count = 0;
	afc_multibyte_retry_count = 0;
	return 0;
}

static int sm5705_afc_ta_accept(struct regmap_desc *pdesc)
{
	muic_data_t *pmuic = pdesc->muic;
	struct i2c_client *i2c = pmuic->i2c;
	int dev3;
	int chg_type;

	pr_info("%s:%s AFC_ACCEPTED \n",MUIC_DEV_NAME, __func__);

	if (pmuic->is_flash_on) {
		pr_info("%s:%s FLASH On, AFC_ACCEPTED DP_RESET\n",MUIC_DEV_NAME, __func__);
		// ENAFC set '0'
		sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_ENAFC, 0);
		msleep(50); // 50ms delay

#if defined(CONFIG_MUIC_UNIVERSAL_SM5705_AFC)
		muic_afc_delay_check_state(0);
#endif

		sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_ENQC20, 0);

		// DP_RESET
		sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_DIS_AFC, 1);
		msleep(20);
		sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_DIS_AFC, 0);

		pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_AFC_CHARGER_5V_MUIC;
		muic_notifier_attach_attached_dev(ATTACHED_DEV_AFC_CHARGER_5V_MUIC);
		return 0;
	}

	cancel_delayed_work(&pmuic->afc_retry_work);

#ifdef CONFIG_MUIC_SM5705_AFC_18W_TA_SUPPORT
    pr_info("%s:%s pmuic->is_18w_ta = %d \n",MUIC_DEV_NAME, __func__,pmuic->is_18w_ta);

    // ENAFC set '0'
    sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_ENAFC, 0);
#else
	// ENAFC set '0'
	sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_ENAFC, 0);
#endif
	dev3 = muic_i2c_read_byte(i2c, REG_DEVT3);
	chg_type = muic_i2c_read_byte(i2c, REG_CHGTYPE);
	
	pr_info("%s: dev3 [0x%02x], chg_type [0x%02x]\n",MUIC_DEV_NAME, dev3, chg_type);
	if ( (dev3 & 0x80) || (chg_type == 0x11) || (chg_type == 0x13) ) { /*CHG_TYPE : 0x11(AFC_TA), 0x13(QC20 TA)*/
		// VBUS_READ
		sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_VBUS_READ, 1);
		pr_info("%s:%s VBUS READ start\n",MUIC_DEV_NAME, __func__);
		if (pmuic->attached_dev != ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC) {
			pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC;
			muic_notifier_attach_attached_dev(ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC);
		}
		afc_vbus_retry_count++;
	} else {
		// ENAFC set '0'
		sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_ENAFC, 0);
	}
	return 0;

}

static int sm5705_afc_vbus_update(struct regmap_desc *pdesc)
{
	muic_data_t *pmuic = pdesc->muic;
	struct i2c_client *i2c = pmuic->i2c;
	int value;
	int vbus_status;
	int qc20;
	int device_type3;
	int chg_type;

	pr_info("%s:%s AFC_VBUS_UPDATE \n",MUIC_DEV_NAME, __func__);

	if (pmuic->attached_dev == ATTACHED_DEV_NONE_MUIC) {
		pr_info("%s:%s Device type is None\n",MUIC_DEV_NAME, __func__);
		return 0;
	}

	vbus_status = muic_i2c_read_byte(i2c, REG_VBUSSTAT);
	pr_info("%s: vbus_status [0x%02x]\n",MUIC_DEV_NAME, vbus_status);

	vbus_status = (vbus_status&0x0F);
	value = muic_i2c_read_byte(i2c, REG_AFCTXD);
	pr_info("%s: AFC_TXD [0x%02x]\n",MUIC_DEV_NAME, value);
	value = (value&0xF0)>>4;
	pr_info("%s:%s AFC_VBUS_STATUS:0x%02x, AFC_TXD:0x%02x\n"
			,MUIC_DEV_NAME, __func__,vbus_status, value);

	if (pmuic->is_flash_on == -1) {
		pr_info("%s:%s Ready FLASH On, Skip AFC\n",MUIC_DEV_NAME, __func__);
		return 0;
	} else if (pmuic->is_flash_on) {
		pr_info("%s:%s FLASH On, Skip AFC\n",MUIC_DEV_NAME, __func__);

#ifdef CONFIG_MUIC_SM5705_AFC_18W_TA_SUPPORT
        if ((vbus_status == value) || ((vbus_status+1) == value) || ((vbus_status-1) == value)) {
#else
		if ((vbus_status == 3) || (vbus_status == 4) || (vbus_status == 5)) {
#endif            
			// ENAFC set '0'
			sm5705_set_afc_ctrl_reg(pdesc, AFCCNTL_ENAFC, 0);
			msleep(50);

#if defined(CONFIG_MUIC_UNIVERSAL_SM5705_AFC)
			muic_afc_delay_check_state(0);
#endif

			sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_ENQC20, 0);

			// DP_RESET
			sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_DIS_AFC, 1);
			msleep(20);
			sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_DIS_AFC, 0);
		}

		pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_AFC_CHARGER_5V_MUIC;
		muic_notifier_attach_attached_dev(ATTACHED_DEV_AFC_CHARGER_5V_MUIC);
		return 0;
	}

	qc20 = muic_i2c_read_byte(i2c, REG_DEVID);
	pr_info("%s:%s REG_DEVID [0x%02x]\n",MUIC_DEV_NAME, __func__, qc20);

	if (qc20 & 0x08) { // QC20 support
		device_type3 = muic_i2c_read_byte(i2c, REG_DEVT3);
		chg_type = muic_i2c_read_byte(i2c, REG_CHGTYPE);

		pr_info("%s:%s DEVICE_TYPE3 [0x%02x], chg_type [0x%02x]\n",MUIC_DEV_NAME, __func__, device_type3, chg_type);
		if ((device_type3 & 0x02) || (chg_type == 0x13) ) { // QC20_TA
 			if (device_type3 & DEV_TYPE3_AFC_TA) {
                            sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_DP_RESET,1);
                            msleep(60);
                            muic_dpreset_afc();
                            return 0;
                        }

			if ( (vbus_status == 3) ||(vbus_status == 4) ||(vbus_status == 5) ) {
				pmuic->is_afc_device = 1;
				afc_vbus_retry_count = 0;

				pr_info("%s:%s AFC(QC20) done \n",MUIC_DEV_NAME, __func__);

				pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_QC_CHARGER_9V_MUIC;
				muic_notifier_attach_attached_dev(ATTACHED_DEV_QC_CHARGER_9V_MUIC);

			} else {
				// VBUS_READ
				if ( afc_vbus_retry_count < 5 ) {
					sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_VBUS_READ, 1);
					sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_VBUS_READ, 0);
					pr_info("%s:%s VBUS READ retry = %d \n",MUIC_DEV_NAME, __func__,
						afc_vbus_retry_count);
					msleep(100);
					afc_vbus_retry_count++;
				} else {
					pr_info("%s:%s AFC 5V \n",MUIC_DEV_NAME, __func__);

					pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_AFC_CHARGER_5V_MUIC;
					muic_notifier_attach_attached_dev(ATTACHED_DEV_AFC_CHARGER_5V_MUIC);
					afc_vbus_retry_count = 0;
				}
			}
		} else { 
			if ((vbus_status == value) || ((vbus_status+1) == value) 
						|| ((vbus_status-1) == value)) { /* AFC DONE */
				pmuic->is_afc_device = 1;
				afc_vbus_retry_count = 0;

				// ENAFC set '0'
				sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_ENAFC, 0);
				pr_info("%s:%s AFC done \n",MUIC_DEV_NAME, __func__);

#ifdef CONFIG_MUIC_SM5705_AFC_18W_TA_SUPPORT
				value = muic_i2c_read_byte(i2c, REG_AFCTXD);
				if ((value == 0x08)||(value == 0x18)||(value == 0x28)||(value == 0x38)||(value == 0x48)) {
					pmuic->is_18w_ta = 1;
					pr_info("%s:%s pmuic->is_18w_ta = %d \n",MUIC_DEV_NAME, __func__,pmuic->is_18w_ta);

					pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_AFC_CHARGER_9V_18W_MUIC;
					muic_notifier_attach_attached_dev(ATTACHED_DEV_AFC_CHARGER_9V_18W_MUIC);
				}
				else {
					pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_AFC_CHARGER_9V_MUIC;
					muic_notifier_attach_attached_dev(ATTACHED_DEV_AFC_CHARGER_9V_MUIC);
				}
#else
				pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_AFC_CHARGER_9V_MUIC;
				muic_notifier_attach_attached_dev(ATTACHED_DEV_AFC_CHARGER_9V_MUIC);
#endif
			} else {
				// VBUS_READ
				if ( afc_vbus_retry_count < 5 ) {
					sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_VBUS_READ, 1);
					sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_VBUS_READ, 0);
					pr_info("%s:%s VBUS READ retry = %d \n",MUIC_DEV_NAME, __func__,
						afc_vbus_retry_count);
					msleep(100);
					afc_vbus_retry_count++;
				} else {
					// ENAFC set '0'
					sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_ENAFC, 0);
					pr_info("%s:%s AFC 5V \n",MUIC_DEV_NAME, __func__);

					pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_AFC_CHARGER_5V_MUIC;
					muic_notifier_attach_attached_dev(ATTACHED_DEV_AFC_CHARGER_5V_MUIC);
					afc_vbus_retry_count = 0;
				}
			}
		}

	} else { // AFC TA only
		if ((vbus_status == value) || ((vbus_status+1) == value) 
						|| ((vbus_status-1) == value)) { /* AFC DONE */
			pmuic->is_afc_device = 1;
			afc_vbus_retry_count = 0;

			// ENAFC set '0'
			sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_ENAFC, 0);
			pr_info("%s:%s AFC done \n",MUIC_DEV_NAME, __func__);
            
#ifdef CONFIG_MUIC_SM5705_AFC_18W_TA_SUPPORT
			value = muic_i2c_read_byte(i2c, REG_AFCTXD);
			if ((value == 0x08)||(value == 0x18)||(value == 0x28)||(value == 0x38)||(value == 0x48)) {
				pmuic->is_18w_ta = 1;
				pr_info("%s:%s pmuic->is_18w_ta = %d \n",MUIC_DEV_NAME, __func__,pmuic->is_18w_ta);

				pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_AFC_CHARGER_9V_18W_MUIC;
				muic_notifier_attach_attached_dev(ATTACHED_DEV_AFC_CHARGER_9V_18W_MUIC);
			}
			else {
				pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_AFC_CHARGER_9V_MUIC;
				muic_notifier_attach_attached_dev(ATTACHED_DEV_AFC_CHARGER_9V_MUIC);
			}
#else
			pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_AFC_CHARGER_9V_MUIC;
			muic_notifier_attach_attached_dev(ATTACHED_DEV_AFC_CHARGER_9V_MUIC);
#endif
		} else {
			// VBUS_READ
			if ( afc_vbus_retry_count < 5 ) {
				sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_VBUS_READ, 1);
				sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_VBUS_READ, 0);
				pr_info("%s:%s VBUS READ retry = %d \n",MUIC_DEV_NAME, __func__,
						afc_vbus_retry_count);
				msleep(100);
				afc_vbus_retry_count++;
			} else {
				// ENAFC set '0'
				sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_ENAFC, 0);
				pr_info("%s:%s AFC 5V \n",MUIC_DEV_NAME, __func__);

				pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_AFC_CHARGER_5V_MUIC;
				muic_notifier_attach_attached_dev(ATTACHED_DEV_AFC_CHARGER_5V_MUIC);
				afc_vbus_retry_count = 0;
			}
		}

	}
	return 0;
}

static int sm5705_afc_multi_byte(struct regmap_desc *pdesc)
{
	muic_data_t *pmuic = pdesc->muic;
	struct i2c_client *i2c = pmuic->i2c;
	int multi_byte[6] = {0,0,0,0,0,0};
	int i;
	int ret;
	int voltage_find;
#ifdef CONFIG_MUIC_SM5705_AFC_18W_TA_SUPPORT
	int value;
#endif

	pr_info("%s:%s AFC_MULTI_BYTE \n",MUIC_DEV_NAME, __func__);

#ifdef CONFIG_MUIC_SM5705_AFC_18W_TA_SUPPORT
    pr_info("%s:%s pmuic->is_18w_ta = %d \n",MUIC_DEV_NAME, __func__,pmuic->is_18w_ta);

    // read AFC_RXD1 ~ RXD6
    voltage_find = 0;
    for(i = 0 ; i < 6 ; i++ ){
        multi_byte[i] = muic_i2c_read_byte(i2c, REG_AFCRXD1 + i);
        if (multi_byte[i] < 0)
            printk(KERN_ERR "%s: err read AFC_RXD%d %d\n", __func__,i+1, multi_byte[i]);
        pr_info("%s:%s AFC_RXD%d [0x%02x]\n",MUIC_DEV_NAME, __func__,i+1, multi_byte[i]);
        
        if( i >= 1 ){   // voltate find
            if ( ((multi_byte[i]&0xF0)>>4) >= ((multi_byte[voltage_find]&0xF0)>>4) ) {
                voltage_find = i;
            }
        }        
    }
    pr_info("%s:%s AFC_TXD multi_byte[%d]=0x%02x \n", MUIC_DEV_NAME, __func__,
                                    voltage_find+1, multi_byte[voltage_find] );    
	if (afc_multibyte_retry_count < 5) {
		// voltage(9.0V) + current(1.65A) setting : 0x
		value = multi_byte[voltage_find];
		ret = muic_i2c_write_byte(i2c, REG_AFCTXD, value);
		if (ret < 0)
			printk(KERN_ERR "[muic] %s: err write AFC_TXD(%d)\n", __func__, ret);
		pr_info("%s:%s AFC_TXD [0x%02x], retry = %d\n",
				MUIC_DEV_NAME, __func__, value, afc_multibyte_retry_count);

		// ENAFC set '1'
		sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_ENAFC, 1);
		afc_multibyte_retry_count++;
	} else {
		pr_info("%s:%s AFC_TXD [0x%02x], retry end = %d\n",
				MUIC_DEV_NAME, __func__, value, afc_multibyte_retry_count);
		// ENAFC set '0'
		sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_ENAFC, 0);
		pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_AFC_CHARGER_5V_MUIC;
		muic_notifier_attach_attached_dev(ATTACHED_DEV_AFC_CHARGER_5V_MUIC);
	}

#else
	// read AFC_RXD1 ~ RXD6
	voltage_find = 0;	
	for(i = 0 ; i < 6 ; i++ ){
		multi_byte[i] = muic_i2c_read_byte(i2c, REG_AFCRXD1 + i);
		if (multi_byte[i] < 0)
			printk(KERN_ERR "%s: err read AFC_RXD%d %d\n", __func__,i+1, multi_byte[i]);
		pr_info("%s:%s AFC_RXD%d [0x%02x]\n",MUIC_DEV_NAME, __func__,i+1, multi_byte[i]);

		if( i >= 1 ){   // voltate find
			if ( ((multi_byte[i]&0xF0)>>4) >= ((multi_byte[voltage_find]&0xF0)>>4) ) {
				voltage_find = i;
			}
		}     		
	}

	pr_info("%s:%s AFC_TXD multi_byte[%d]=0x%02x \n", MUIC_DEV_NAME, __func__,
								voltage_find+1, multi_byte[voltage_find] );

	if (afc_multibyte_retry_count < 5) {
		// voltage(9.0V) + current(1.65A) setting : 0x
		ret = muic_i2c_write_byte(i2c, REG_AFCTXD, multi_byte[voltage_find]);
		if (ret < 0)
			printk(KERN_ERR "[muic] %s: err write AFC_TXD(%d)\n", __func__, ret);
		pr_info("%s:%s AFC_TXD [0x%02x], retry = %d\n",
				MUIC_DEV_NAME, __func__, multi_byte[voltage_find], afc_multibyte_retry_count);

		// ENAFC set '1'
		sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_ENAFC, 1);
		afc_multibyte_retry_count++;
	} else {
		pr_info("%s:%s AFC_TXD [0x%02x], retry end = %d\n",
				MUIC_DEV_NAME, __func__, multi_byte[voltage_find], afc_multibyte_retry_count);
		// ENAFC set '0'
		sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_ENAFC, 0);
		pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_AFC_CHARGER_5V_MUIC;
		muic_notifier_attach_attached_dev(ATTACHED_DEV_AFC_CHARGER_5V_MUIC);
	}

#endif

	return 0;

}

static int sm5705_afc_error(struct regmap_desc *pdesc)
{
	muic_data_t *pmuic = pdesc->muic;
	struct i2c_client *i2c = pmuic->i2c;
	int value;
	int device_type3;
	int chg_type;

	pr_info("%s:%s AFC_ERROR \n",MUIC_DEV_NAME, __func__);

	// read AFC_STATUS
	value = muic_i2c_read_byte(i2c, REG_AFCSTAT);
	if (value < 0)
		printk(KERN_ERR "%s: err read AFC_STATUS %d\n", __func__, value);
	pr_info("%s:%s AFC_STATUS [0x%02x]\n",MUIC_DEV_NAME, __func__, value);

	if (pmuic->afc_retry_count < 5) {
		pr_info("%s:%s	ENAFC retry = %d \n",MUIC_DEV_NAME, __func__, pmuic->afc_retry_count);

		value = muic_i2c_read_byte(i2c, REG_DEVID);
		pr_info("%s:%s REG_DEVID [0x%02x]\n",MUIC_DEV_NAME, __func__, value);
		if (value & 0x08){ // QC20 support
			device_type3 = muic_i2c_read_byte(i2c, REG_DEVT3);
			chg_type = muic_i2c_read_byte(i2c, REG_CHGTYPE);

			pr_info("%s:%s DEVICE_TYPE3 [0x%02x], chg_type [0x%02x]\n",MUIC_DEV_NAME, __func__, device_type3, chg_type);
			if ((device_type3 & 0x02) || (chg_type == 0x13)){ // QC20_TA
				sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_ENAFC, 0);
				sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_ENQC20, 1); // QC20 enable 9V

				// VBUS_READ
				sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_VBUS_READ, 1);
				pr_info("%s:%s VBUS READ start\n",MUIC_DEV_NAME, __func__); 			   
				// reset retry_count
				pmuic->afc_retry_count = 0;
			} else {
				msleep(100); // 100ms delay
				// ENAFC set '1'
				sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_ENAFC, 1);
				pmuic->afc_retry_count++;
			}
		} else {
			msleep(100); // 100ms delay
			// ENAFC set '1'
			sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_ENAFC, 1);
			pmuic->afc_retry_count++;
		}		 

	} else {
		pr_info("%s:%s	ENAFC end = %d \n",MUIC_DEV_NAME, __func__, pmuic->afc_retry_count);
		// ENAFC set '0'
		sm5705_set_afc_ctrl_reg(pdesc, AFCCTRL_ENAFC, 0);
		pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC;
		muic_notifier_attach_attached_dev(pmuic->attached_dev);
	}
	return 0;
}

static int sm5705_afc_init_check(struct regmap_desc *pdesc)
{
	muic_data_t *pmuic = pdesc->muic;
	struct afc_ops *afcops = pmuic->regmapdesc->afcops;
	int ret;

	pr_info("%s:%s AFC_INIT_CHECK\n",MUIC_DEV_NAME, __func__);

	pr_info("%s:%s pmuic->vps.s.val1 [0x%02x], pmuic->vps.s.chgtyp [0x%02x]\n",MUIC_DEV_NAME, __func__, pmuic->vps.s.val1, pmuic->vps.s.chgtyp);

	/* check afc interrupt state */
	ret = muic_i2c_read_byte(pmuic->i2c, 0x0F);
	pr_info("%s:%s address 0x0F:0x%02x\n", MUIC_DEV_NAME, __func__, ret);

	if (ret & 0x01) {
		ret = muic_i2c_write_byte(pmuic->i2c, 0x0F, 0x00);
		if (ret < 0)
			printk(KERN_ERR "[muic] %s: err write 0x0F(%d)\n", __func__, ret);
	} else {
		pr_info("%s:%s Not AFC devices Skip afc init\n",
				MUIC_DEV_NAME, __func__);
		return 0;
	}


	if ( !((pmuic->vps.s.val1 == SM5705_DT1_DCP) || (pmuic->vps.s.chgtyp == CHGTYP_DEDICATED_CHARGER)) ) {
		pr_info("%s:%s pmuic->vps.s.val1 != DCP charger return \n",
						MUIC_DEV_NAME, __func__);
		return 0;
	}

	pr_info("%s:%s pmuic->intr.intr3[0x%02x], is_ccic_attach(%d), ccic_rp(%d)\n",MUIC_DEV_NAME, __func__,
		pmuic->intr.intr3, pmuic->is_ccic_attach, pmuic->ccic_rp);
	if ((pmuic->intr.intr3 & SM5705_AFC_TA_ATTACHED) ||
				(pmuic->vps.s.val1 == SM5705_DT1_DCP) ||
				(pmuic->vps.s.chgtyp == CHGTYP_DEDICATED_CHARGER)) {
#if defined(CONFIG_MUIC_SUPPORT_CCIC) && !defined(CONFIG_SEC_FACTORY)
		if (pmuic->is_ccic_attach) {
			if (pmuic->ccic_rp == Rp_56K)
				afcops->afc_ta_attach(pmuic->regmapdesc);
			else
				pr_info("%s: Rp isn't 56K, but is (%d)K\n", __func__, pmuic->ccic_rp);
		}
		else {
			pmuic->retry_afc = true;
			pr_info("%s: Need to restart AFC for late ccic_attach\n", __func__);
		}
#else
		afcops->afc_ta_attach(pmuic->regmapdesc);

#endif
	}


	return 0;
}

#if defined(CONFIG_SEC_DEBUG) || defined(CONFIG_SEC_QUEST)
static int sm5705_usb_to_ta(struct regmap_desc *pdesc, int mode)
{
	int ret = 0;
	muic_data_t *pmuic = pdesc->muic;
	vps_data_t *pmsr = &pmuic->vps;

	switch(mode) {
	case USB2TA_DISABLE:
		pr_info("%s, Disable USB to TA\n", __func__);
		if (pmuic->attached_dev == ATTACHED_DEV_TA_MUIC && pmuic->usb_to_ta_state) {
			switch (pmsr->s.chgtyp) {
			case SM5705_CHG_TYPE_CDP:
				pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_CDP_MUIC;
				break;
			case SM5705_CHG_TYPE_SDP:
				pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_USB_MUIC;
				break;
			default:
				pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_USB_MUIC;
				break;
			}
			muic_notifier_detach_attached_dev(ATTACHED_DEV_TA_MUIC);
			muic_notifier_attach_attached_dev(pmuic->attached_dev);
			pmuic->usb_to_ta_state = false;
		}
		break;
	case USB2TA_ENABLE:
		pr_info("%s, Enable USB to TA attached_dev %d\n",
				__func__, pdesc->muic->attached_dev);
		if ((pdesc->muic->attached_dev == ATTACHED_DEV_CDP_MUIC ||
				pdesc->muic->attached_dev == ATTACHED_DEV_USB_MUIC)
				&& !pmuic->usb_to_ta_state) {
			muic_notifier_detach_attached_dev(pdesc->muic->attached_dev);
			muic_notifier_attach_attached_dev(ATTACHED_DEV_TA_MUIC);
			pmuic->legacy_dev = pmuic->attached_dev = ATTACHED_DEV_TA_MUIC;
			pmuic->usb_to_ta_state = true;
		}
		break;
	case USB2TA_READ:
		pr_info("%s, USB to TA %s\n", __func__,
				pmuic->usb_to_ta_state ? "Enabled" : "Disabled");
		ret = pmuic->usb_to_ta_state;
		break;
	default:
		pr_err("%s, Unknown CMD\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}
#endif

static struct regmap_ops sm5705_muic_regmap_ops = {
	.get_size = sm5705_get_sizeof_regmap,
	.ioctl = sm5705_muic_ioctl,
	.get_formatted_dump = sm5705_get_fromatted_dump,
};

static struct vendor_ops sm5705_muic_vendor_ops = {
	.attach_ta = sm5705_attach_ta,
	.detach_ta = sm5705_detach_ta,
	.get_switch = sm5705_get_switching_mode,
	.set_switch = sm5705_set_switching_mode,
	.set_adc_scan_mode = sm5705_set_adc_scan_mode,
	.get_adc_scan_mode = sm5705_get_adc_scan_mode,
	.set_rustproof = sm5705_set_rustproof,
	.get_vps_data = sm5705_get_vps_data,
	.rescan = sm5705_run_BCD_rescan,
	.get_vbus_value = sm5705_get_vbus_value,
#ifdef CONFIG_MUIC_SM5705_SWITCH_CONTROL_GPIO	
	.prepare_switch = sm5705_prepare_switch,
#endif
	.run_chgdet = sm5705_run_chgdet,
	.get_dcdtmr_irq = sm5705_get_dcdtmr_irq,
#if defined(CONFIG_SEC_DEBUG) || defined(CONFIG_SEC_QUEST)
	.usb_to_ta = sm5705_usb_to_ta,
#endif
};

static struct afc_ops sm5705_muic_afc_ops = {
	.afc_ta_attach = sm5705_afc_ta_attach,
	.afc_ta_accept = sm5705_afc_ta_accept,
	.afc_vbus_update = sm5705_afc_vbus_update,
	.afc_multi_byte = sm5705_afc_multi_byte,
	.afc_error = sm5705_afc_error,
	.afc_ctrl_reg = sm5705_set_afc_ctrl_reg,
	.afc_init_check = sm5705_afc_init_check,
#ifdef CONFIG_MUIC_SM5705_AFC_18W_TA_SUPPORT
	.afc_reset_multibyte_retry_count = sm5705_reset_multibyte_retry_count,
#endif
};

static struct regmap_desc sm5705_muic_regmap_desc = {
	.name = "sm5705-MUIC",
	.regmap = sm5705_muic_regmap_table,
	.size = REG_END,
	.regmapops = &sm5705_muic_regmap_ops,
	.vendorops = &sm5705_muic_vendor_ops,
	.afcops = &sm5705_muic_afc_ops,
};

void muic_register_sm5705_regmap_desc(struct regmap_desc **pdesc)
{
	*pdesc = &sm5705_muic_regmap_desc;
}
