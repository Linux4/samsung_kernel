/*
 * Copyright (C) 2020 Hynetek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __LINUX_HUSB311_H
#define __LINUX_HUSB311_H

#include "std_tcpci_v10.h"
#include "pd_dbg_info.h"

/*show debug message or not */
#define ENABLE_HUSB311_DBG	0

/* HUSB311 Private RegMap */

#define HUSB311_REG_CONFIG_GPIO0			(0x71)

#define HUSB311_REG_PHY_CTRL1				(0x80)

#define HUSB311_REG_CLK_CTRL2				(0x87)
#define HUSB311_REG_CLK_CTRL3				(0x88)

#define HUSB311_REG_PRL_FSM_RESET			(0x8D)

#define HUSB311_REG_BMC_CTRL				(0x90)
#define HUSB311_REG_BMCIO_RXDZSEL			(0x93)
#define HUSB311_REG_VCONN_CLIMITEN			(0x95)

#define HUSB311_REG_HUSB_STATUS				(0x97)
#define HUSB311_REG_HUSB_INT				(0x98)
#define HUSB311_REG_HUSB_MASK				(0x99)

#define HUSB311_REG_IDLE_CTRL				(0x9B)
#define HUSB311_REG_INTRST_CTRL				(0x9C)
#define HUSB311_REG_WATCHDOG_CTRL			(0x9D)
#define HUSB311_REG_I2CRST_CTRL				(0X9E)

#define HUSB311_REG_SWRESET					(0xA0)
#define HUSB311_REG_TTCPC_FILTER			(0xA1)
#define HUSB311_REG_DRP_TOGGLE_CYCLE		(0xA2)
#define HUSB311_REG_DRP_DUTY_CTRL			(0xA3)
#define HUSB311_REG_BMCIO_RXDZEN			(0xAF)

#define HUSB311_REG_UNLOCK_PW_2				(0xF0)
#define HUSB311_REG_UNLOCK_PW_1				(0xF1)
#define HUSB311_REG_EFUSE5					(0xF6)

/*HUSB311 REGMAP*/

#define HUSB311_TCPC_V10_REG_VID					(0x00)
#define HUSB311_TCPC_V10_REG_PID					(0x02)
#define HUSB311_TCPC_V10_REG_DID					(0x04)
#define HUSB311_TCPC_V10_REG_TYPEC_REV				(0x06)
#define HUSB311_TCPC_V10_REG_PD_REV					(0x08)
#define HUSB311_TCPC_V10_REG_PDIF_REV				(0x0A)
#define HUSB311_TCPC_V10_REG_ALERT					(0x10)
#define HUSB311_TCPC_V10_REG_ALERT_MASK				(0x12)
#define HUSB311_TCPC_V10_REG_POWER_STATUS_MASK		(0x14)
#define HUSB311_TCPC_V10_REG_FAULT_STATUS_MASK		(0x15)
#define HUSB311_TCPC_V10_REG_HUSB311_TCPC_CTRL		(0x19)
#define HUSB311_TCPC_V10_REG_ROLE_CTRL				(0x1A)
#define HUSB311_TCPC_V10_REG_FAULT_CTRL				(0x1B)
#define HUSB311_TCPC_V10_REG_POWER_CTRL				(0x1C)
#define HUSB311_TCPC_V10_REG_CC_STATUS				(0x1D)
#define HUSB311_TCPC_V10_REG_POWER_STATUS			(0x1E)
#define HUSB311_TCPC_V10_REG_FAULT_STATUS			(0x1F)
#define HUSB311_TCPC_V10_REG_COMMAND				(0x23)
#define HUSB311_TCPC_V10_REG_MSG_HDR_INFO			(0x2e)
#define HUSB311_TCPC_V10_REG_RX_DETECT				(0x2f)
#define HUSB311_TCPC_V10_REG_RX_BYTE_CNT			(0x30)
#define HUSB311_TCPC_V10_REG_RX_BUF_FRAME_TYPE		(0x31)
#define HUSB311_TCPC_V10_REG_RX_HDR					(0x32)
#define HUSB311_TCPC_V10_REG_RX_DATA				(0x34)
#define HUSB311_TCPC_V10_REG_TRANSMIT				(0x50)
#define HUSB311_TCPC_V10_REG_TX_BYTE_CNT			(0x51)
#define HUSB311_TCPC_V10_REG_TX_HDR					(0x52)
#define HUSB311_TCPC_V10_REG_TX_DATA				(0x54)

/*HUSB311 private*/
/* hs14 code for SR-AL6528A-945 by shanxinkai at 2022/11/21 start */
#define HUSB311_REG_CD					(0xCD)
#define HUSB311_CFG_MODE				(0xBF)
#define HUSB311_CFG_ENTRY				(0x33)
#define HUSB311_CFG_EXIT				(0x00)
#define HUSB311_CFG_START				(0xC0)
/* hs14 code for SR-AL6528A-945 by shanxinkai at 2022/11/21 end */
#define HUSB311_REG_CF					(0xcf)
/*
 * Device ID
 */
#define HUSB311_DID             0x0
/*
 * HUSB311_REG_PHY_CTRL1			(0x80)
 */

#define HUSB311_REG_PHY_CTRL1_SET( \
	retry_discard, toggle_cnt, bus_idle_cnt, rx_filter) \
	((retry_discard << 7) | (toggle_cnt << 4) | \
	(bus_idle_cnt << 2) | (rx_filter & 0x03))

/*
 * HUSB311_REG_CLK_CTRL2			(0x87)
 */

#define HUSB311_REG_CLK_DIV_600K_EN		(1<<7)
#define HUSB311_REG_CLK_BCLK2_EN		(1<<6)
#define HUSB311_REG_CLK_BCLK2_TG_EN		(1<<5)
#define HUSB311_REG_CLK_DIV_300K_EN		(1<<3)
#define HUSB311_REG_CLK_CK_300K_EN		(1<<2)
#define HUSB311_REG_CLK_BCLK_EN			(1<<1)
#define HUSB311_REG_CLK_BCLK_TH_EN		(1<<0)

/*
 * HUSB311_REG_CLK_CTRL3			(0x88)
 */

#define HUSB311_REG_CLK_OSCMUX_RG_EN	(1<<7)
#define HUSB311_REG_CLK_CK_24M_EN		(1<<6)
#define HUSB311_REG_CLK_OSC_RG_EN		(1<<5)
#define HUSB311_REG_CLK_DIV_2P4M_EN		(1<<4)
#define HUSB311_REG_CLK_CK_2P4M_EN		(1<<3)
#define HUSB311_REG_CLK_PCLK_EN			(1<<2)
#define HUSB311_REG_CLK_PCLK_RG_EN		(1<<1)
#define HUSB311_REG_CLK_PCLK_TG_EN		(1<<0)

/*
 * HUSB311_REG_BMC_CTRL				(0x90)
 */

#define HUSB311_REG_IDLE_EN				(1<<6)
#define HUSB311_REG_DISCHARGE_EN			(1<<5)
#define HUSB311_REG_BMCIO_LPRPRD			(1<<4)
#define HUSB311_REG_BMCIO_LPEN				(1<<3)
#define HUSB311_REG_BMCIO_BG_EN				(1<<2)
#define HUSB311_REG_VBUS_DET_EN				(1<<1)
#define HUSB311_REG_BMCIO_OSC_EN			(1<<0)

/*
 * HUSB311_REG_HUSB_STATUS				(0x97)
 */

#define HUSB311_REG_RA_DETACH				(1<<5)
#define HUSB311_REG_VBUS_80				(1<<1)

/*
 * HUSB311_REG_HUSB_INT				(0x98)
 */

#define HUSB311_REG_INT_RA_DETACH			(1<<5)
#define HUSB311_REG_INT_WATCHDOG			(1<<2)
#define HUSB311_REG_INT_VBUS_80				(1<<1)
#define HUSB311_REG_INT_WAKEUP				(1<<0)

/*
 * HUSB311_REG_HUSB_MASK				(0x99)
 */

#define HUSB311_REG_M_RA_DETACH				(1<<5)
#define HUSB311_REG_M_WATCHDOG				(1<<2)
#define HUSB311_REG_M_VBUS_80				(1<<1)
#define HUSB311_REG_M_WAKEUP				(1<<0)

/*
 * HUSB311_REG_IDLE_CTRL				(0x9B)
 */

#define HUSB311_REG_CK_300K_SEL				(1<<7)
#define HUSB311_REG_SHIPPING_OFF			(1<<5)
#define HUSB311_REG_ENEXTMSG				(1<<4)
#define HUSB311_REG_AUTOIDLE_EN				(1<<3)

/*
 * HUSB311_REG_EFUSE5					(0xF6)
 */

#define HUSB311_REG_M_VBUS_CAL				GENMASK(7, 5)
#define HUSB311_REG_S_VBUS_CAL				5
#define HUSB311_REG_MIN_VBUS_CAL			-4

/* timeout = (tout*2+1) * 6.4ms */

#ifdef CONFIG_USB_PD_REV30
#define HUSB311_REG_IDLE_SET(ck300, ship_dis, auto_idle, tout) \
	((ck300 << 7) | (ship_dis << 5) | (auto_idle << 3) \
	| (tout & 0x07) | HUSB311_REG_ENEXTMSG)
#else
#define HUSB311_REG_IDLE_SET(ck300, ship_dis, auto_idle, tout) \
	((ck300 << 7) | (ship_dis << 5) | (auto_idle << 3) | (tout & 0x07))
#endif

/*
 * HUSB311_REG_INTRST_CTRL			(0x9C)
 */

#define HUSB311_REG_INTRST_EN				(1<<7)

/* timeout = (tout+1) * 0.2sec */
#define HUSB311_REG_INTRST_SET(en, tout) \
	((en << 7) | (tout & 0x03))

/*
 * HUSB311_REG_WATCHDOG_CTRL		(0x9D)
 */

#define HUSB311_REG_WATCHDOG_EN				(1<<7)

/* timeout = (tout+1) * 0.4sec */
#define HUSB311_REG_WATCHDOG_CTRL_SET(en, tout)	\
	((en << 7) | (tout & 0x07))

/*
 * HUSB311_REG_I2CRST_CTRL		(0x9E)
 */

#define HUSB311_REG_I2CRST_EN				(1<<7)

/* timeout = (tout+1) * 12.5ms */
#define HUSB311_REG_I2CRST_SET(en, tout)	\
	((en << 7) | (tout & 0x0f))

#if ENABLE_HUSB311_DBG
#define HUSB311_INFO(format, args...) \
	pd_dbg_info("%s() line-%d: " format,\
	__func__, __LINE__, ##args)
#else
#define HUSB311_INFO(foramt, args...)
#endif

enum husb311_version {
	HUSB311_B,
	HUSB311_C,
};
#endif /* #ifndef __LINUX_HUSB311_H */
