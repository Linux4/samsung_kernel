/*
 * Copyright (C) 2022 ConvenientPower. Co., Ltd.
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

#ifndef __LINUX_CPS8851_H
#define __LINUX_CPS8851_H

#include "std_tcpci_v10.h"
#include "pd_dbg_info.h"

/*show debug message or not */
#define ENABLE_CPS8851_DBG	0
//#define CONFIG_RT_REGMAP

/* CPS8851 Private RegMap */

#define CPS8851_REG_VBUS_VOLTAGE			(0x71)
#define CPS8851_REG_VDDIO_SEL				(0x7C)
#define CPS8851_REG_VCONNOCP				(0x93)

#define CPS8851_REG_CP_STATUS					(0x97)
#define CPS8851_REG_CP_INT					(0x98)
#define CPS8851_REG_CP_MASK					(0x99)

#define CPS8851_REG_IDLE_CTRL				(0x9B)
#define CPS8851_REG_INTRST_CTRL				(0x9C)

#define CPS8851_REG_SWRESET					(0xA0)
/*
 * The period a DRP will complete a Source to Sink and back advertisement. 
 * (Period = TDRP * 6.4 + 51.2ms)
 * 0000 : 51.2ms
 * 0001 : 57.6ms
 * 0010 : 64ms
 * 0011 : 70.4ms (default)
 * …
 * 1110 : 140.8ms
 * 1111 : 147.2ms
 */
#define CPS8851_REG_TDRP					(0xA2)
/*
 * The percent of time that a DRP will advertise Source during tDRP.
 * (DUTY = (DCSRCDRP[9:0] + 1) / 1024)
 */
#define CPS8851_REG_DRP_DUTY_CTRL			(0xA3)

#define CPS8851_REG_CP_OTSD					(0xA5)
#define CPS8851_REG_CP_OTSD_MASK			(0xA6)
#define CPS8851_REG_PASSWORD				(0xB0)
#define CPS8851_REG_PD_SDAC					(0xB5)
#define CPS8851_REG_PD_TCPC					(0xB8)

/*
 * Device ID
 */

#define CPS8851_DID_A		0x2173

/*
 * CPS8851_REG_VBUS_VOLTAGE			(0x71)
 */

#define CPS8851_REG_VBUS_VSAFE0V	(0x00)
#define CPS8851_REG_VBUS_PRESENT	(0x01)
#define CPS8851_REG_VBUS_9V			(0x02)
#define CPS8851_REG_VBUS_12V		(0x03)
#define CPS8851_REG_VBUS_15V		(0x04)
#define CPS8851_REG_VBUS_20V		(0x05)

/*
 * CPS8851_REG_VDDIO_SEL			(0x7C)
 */

#define CPS8851_REG_VDDIO_1P2V		(0x00)
#define CPS8851_REG_VDDIO_1P8V		(0x01)

/*
 * CPS8851_REG_VCONNOCP				(0x93)
 */

#define CPS8851_VCONNOCP_LEVEL_200MA	(0x00)
#define CPS8851_VCONNOCP_LEVEL_300MA	(0x01)
#define CPS8851_VCONNOCP_LEVEL_400MA	(0x02)
#define CPS8851_VCONNOCP_LEVEL_500MA	(0x03)
#define CPS8851_VCONNOCP_LEVEL_600MA	(0x04) /* default */
#define CPS8851_REG_VCONNOCP_SHIFT		(0x05)
#define CPS8851_REG_VCONNOCP_SET(level)	\
	((level) << CPS8851_REG_VCONNOCP_SHIFT)

/*
 * CPS8851_REG_CP_STATUS				(0x97)
 */

#define CPS8851_REG_VBUS_80				(1<<1)

/*
 * CPS8851_REG_CP_INT					(0x98)
 */

#define CPS8851_REG_INT_RA_DETACH			(1<<5)
#define CPS8851_REG_INT_VBUS_80				(1<<1)

/*
 * CPS8851_REG_CP_MASK					(0x99)
 */

#define CPS8851_REG_M_RA_DETACH				(1<<5)
#define CPS8851_REG_M_VBUS_80				(1<<1)

/*
 * CPS8851_REG_IDLE_CTRL				(0x9B)
 */

#define CPS8851_REG_SHIPPING_OFF			(1<<5)
#define CPS8851_REG_AUTOIDLE_EN				(1<<3)

/* timeout = (tout*2+1) * 6.4ms */

#define CPS8851_REG_IDLE_SET(dummy, ship_dis, auto_idle, tout) \
	((ship_dis << 5) | (auto_idle << 3) | (tout & 0x07))

/*
 * CPS8851_REG_INTRST_CTRL			(0x9C)
 */

#define CPS8851_REG_INTRST_EN				(1<<7)

/* timeout = (tout+1) * 0.2sec */
#define CPS8851_REG_INTRST_SET(en, tout) \
	((en << 7) | (tout & 0x03))

/*
 * CPS8851_REG_CP_OTSD					(0xA5)
 */

#define CPS8851_REG_OTSD_STATE			(1<<2)

/*
 * CPS8851_REG_CP_OTSD_MASK			(0xA6)
 */

#define CPS8851_REG_M_OTSD_STATE			(1<<2)

#if ENABLE_CPS8851_DBG
#define CPS8851_INFO(format, args...) \
	pd_dbg_info("%s() line-%d: " format,\
	__func__, __LINE__, ##args)
#else
#define CPS8851_INFO(foramt, args...)
#endif

#endif /* #ifndef __LINUX_CPS8851_H */
