/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __LINUX_WUSB3812C_H
#define __LINUX_WUSB3812C_H

#include "std_tcpci_v10.h"
#include "pd_dbg_info.h"

/*show debug message or not */
#define ENABLE_WUSB3812_DBG	0

/* WUSB3812H Private RegMap */

#define WUSB3812C_REG_CONFIG_GPIO0			(0x71)

#define WUSB3812C_REG_PHY_CTRL1				(0x80)

#define WUSB3812C_REG_CLK_CTRL2				(0x87)
#define WUSB3812C_REG_CLK_CTRL3				(0x88)

#define WUSB3812C_REG_PRL_FSM_RESET			(0x8D)

#define WUSB3812C_REG_BMC_CTRL				(0x90)
#define WUSB3812C_REG_BMCIO_RXDZSEL			(0x93)
#define WUSB3812C_REG_VCONN_CLIMITEN			(0x93)

#define WUSB3812C_REG_RT_STATUS				(0x97)
#define WUSB3812C_REG_RT_INT				(0x98)
#define WUSB3812C_REG_RT_MASK				(0x99)

#define WUSB3812C_REG_IDLE_CTRL				(0x9B)
#define WUSB3812C_REG_INTRST_CTRL			(0x9C)
#define WUSB3812C_REG_WATCHDOG_CTRL			(0x9D)
#define WUSB3812C_REG_I2CRST_CTRL			(0X92)

#define WUSB3812C_REG_SWRESET				(0x92)
#define WUSB3812C_REG_TTCPC_FILTER			(0xA1)
#define WUSB3812C_REG_DRP_TOGGLE_CYCLE		(0xA2)
#define WUSB3812C_REG_DRP_DUTY_CTRL			(0xA3)
#define WUSB3812C_REG_BMCIO_RXDZEN			(0xAF)

#define WUSB3812C_REG_VSAFE0_ENABLE			(0xB0)

#define WUSB3812C_REG_UNLOCK_PW_2				(0xF0)
#define WUSB3812C_REG_UNLOCK_PW_1				(0xF1)
#define WUSB3812C_REG_EFUSE5				(0xF6)

/*
 * Device ID
 */

#define WUSB3812C_DID_A		0x0111
#define WUSB3812C_DID_B		0x0111
#define WUSB3812C_DID_C		0x0111
#define WUSB3812C_DID_D		0x0111

/*
 * WUSB3812C_REG_PHY_CTRL1			(0x80)
 */

#define WUSB3812C_REG_PHY_CTRL1_SET( \
	retry_discard, toggle_cnt, bus_idle_cnt, rx_filter) \
	((retry_discard << 7) | (toggle_cnt << 4) | \
	(bus_idle_cnt << 2) | (rx_filter & 0x03))

/*
 * WUSB3812C OTG Verify
 */
#define WUSB3812C_REG_ROLE_PRP    	(0x1D)
#define WUSB3812C_REG_ROLE_NRP    	(0x17)

#define WUSB3812C_REG_CLK_DIV_600K_EN		(1<<7)
#define WUSB3812C_REG_ROLE_CTRL_DRP_EN		(1<<6)
#define WUSB3812C_REG_LOOK4CONNECTION		(1<<5)
#define WUSB3812C_REG_CLK_DIV_300K_EN		(1<<3)
#define WUSB3812C_REG_CLK_CK_300K_EN		(1<<2)
#define WUSB3812C_REG_CLK_BCLK_EN			(1<<1)
#define WUSB3812C_REG_CLK_BCLK_TH_EN		(1<<0)

/*
 * WUSB3812C_REG_CLK_CTRL3			(0x88)
 */

#define WUSB3812C_REG_CLK_OSCMUX_RG_EN	(1<<7)
#define WUSB3812C_REG_CLK_CK_24M_EN		(1<<6)
#define WUSB3812C_REG_CLK_OSC_RG_EN		(1<<5)
#define WUSB3812C_REG_CLK_DIV_2P4M_EN		(1<<4)
#define WUSB3812C_REG_CLK_CK_2P4M_EN		(1<<3)
#define WUSB3812C_REG_CLK_PCLK_EN			(1<<2)
#define WUSB3812C_REG_CLK_PCLK_RG_EN		(1<<1)
#define WUSB3812C_REG_CLK_PCLK_TG_EN		(1<<0)

/*
 * WUSB3812C_REG_BMC_CTRL				(0x90)
 */

#define WUSB3812C_REG_IDLE_EN				(1<<6)
#define WUSB3812C_REG_DISCHARGE_EN			(1<<4)
#define WUSB3812C_REG_BMCIO_LPRPRD			(1<<4)
#define WUSB3812C_REG_BMCIO_LPEN				(1<<3)
#define WUSB3812C_REG_BMCIO_BG_EN				(1<<2)
#define WUSB3812C_REG_VBUS_DET_EN				(1<<1)
#define WUSB3812C_REG_BMCIO_OSC_EN			(1<<0)

/*
 * WUSB3812C_REG_RT_STATUS				(0x97)
 */

#define WUSB3812C_REG_RA_DETACH				(1<<5)
#define WUSB3812C_REG_VBUS_80				(1<<1)

/*
 * WUSB3812C_REG_RT_INT				(0x98)
 */

#define WUSB3812C_REG_INT_RA_DETACH			(1<<5)
#define WUSB3812C_REG_INT_WATCHDOG			(1<<2)
#define WUSB3812C_REG_INT_VBUS_80				(1<<1)
#define WUSB3812C_REG_INT_WAKEUP				(1<<0)

/*
 * WUSB3812C_REG_RT_MASK				(0x99)
 */

#define WUSB3812C_REG_M_RA_DETACH				(1<<5)
#define WUSB3812C_REG_M_WATCHDOG				(1<<2)
#define WUSB3812C_REG_M_VBUS_80				(1<<1)
#define WUSB3812C_REG_M_WAKEUP				(1<<0)

/*
 * WUSB3812C_REG_IDLE_CTRL				(0x9B)
 */

#define WUSB3812C_REG_CK_300K_SEL				(1<<7)
#define WUSB3812C_REG_SHIPPING_OFF			(1<<5)
#define WUSB3812C_REG_ENEXTMSG				(1<<4)
#define WUSB3812C_REG_AUTOIDLE_EN				(1<<3)

/*
 * WUSB3812C_REG_EFUSE5					(0xF6)
 */

#define WUSB3812C_REG_M_VBUS_CAL				GENMASK(7, 5)
#define WUSB3812C_REG_S_VBUS_CAL				5
#define WUSB3812C_REG_MIN_VBUS_CAL			-4

/* timeout = (tout*2+1) * 6.4ms */

#ifdef CONFIG_USB_PD_REV30
#define WUSB3812C_REG_IDLE_SET(ck300, ship_dis, auto_idle, tout) \
	((ck300 << 7) | (ship_dis << 5) | (auto_idle << 3) \
	| (tout & 0x07) | WUSB3812C_REG_ENEXTMSG)
#else
#define WUSB3812C_REG_IDLE_SET(ck300, ship_dis, auto_idle, tout) \
	((ck300 << 7) | (ship_dis << 5) | (auto_idle << 3) | (tout & 0x07))
#endif

/*
 * WUSB3812C_REG_INTRST_CTRL			(0x9C)
 */

#define WUSB3812C_REG_INTRST_EN				(1<<7)

/* timeout = (tout+1) * 0.2sec */
#define WUSB3812C_REG_INTRST_SET(en, tout) \
	((en << 7) | (tout & 0x03))

/*
 * WUSB3812C_REG_WATCHDOG_CTRL		(0x9D)
 */

#define WUSB3812C_REG_WATCHDOG_EN				(1<<7)

/* timeout = (tout+1) * 0.4sec */
#define WUSB3812C_REG_WATCHDOG_CTRL_SET(en, tout)	\
	((en << 7) | (tout & 0x07))

/*
 * WUSB3812C_REG_I2CRST_CTRL		(0x9E)
 */

#define WUSB3812C_REG_I2CRST_EN				(1<<7)

/* timeout = (tout+1) * 12.5ms */
#define WUSB3812C_REG_I2CRST_SET(en, tout)	\
	(en | (tout & 0x03))

#if ENABLE_WUSB3812_DBG
#define WUSB3812C_INFO(format, args...) \
	pd_dbg_info("%s() line-%d: " format,\
	__func__, __LINE__, ##args)
#else
#define WUSB3812C_INFO(foramt, args...)
#endif

#endif /* #ifndef __LINUX_WUSB3812C_H */