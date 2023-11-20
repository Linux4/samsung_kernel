/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MACH_WATCHDOG_H_
#define _MACH_WATCHDOG_H_
#include <linux/spinlock.h>
#include <linux/bitops.h>
#include <soc/sprd/adi.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/hardware.h>
#define WDG_BASE			(ANA_WDG_BASE)
#define SPRD_ANA_BASE		(ANA_CTL_GLB_BASE)

#define WDG_LOAD_LOW		(WDG_BASE + 0x00)
#define WDG_LOAD_HIGH		(WDG_BASE + 0x04)
#define WDG_CTRL			(WDG_BASE + 0x08)
#define WDG_INT_CLR			(WDG_BASE + 0x0C)
#define WDG_INT_RAW			(WDG_BASE + 0x10)
#define WDG_INT_MSK			(WDG_BASE + 0x14)
#define WDG_CNT_LOW			(WDG_BASE + 0x18)
#define WDG_CNT_HIGH		(WDG_BASE + 0x1C)
#define WDG_LOCK			(WDG_BASE + 0x20)

#define WDG_INT_EN_BIT		BIT(0)
#define WDG_CNT_EN_BIT		BIT(1)
#define WDG_NEW_VER_EN		BIT(2)
#define WDG_INT_CLEAR_BIT	BIT(0)
#define WDG_RST_CLEAR_BIT	BIT(3)
#define WDG_LD_BUSY_BIT		BIT(4)
#define WDG_RST_EN_BIT		BIT(3)

#define WDG_NEW_VERSION
#define ANA_RST_STATUS		ANA_REG_GLB_POR_RST_MONITOR
#define ANA_AGEN			ANA_REG_GLB_ARM_MODULE_EN
#define ANA_RTC_CLK_EN		ANA_REG_GLB_RTC_CLK_EN
#define AGEN_WDG_EN			BIT_ANA_WDG_EN
#define AGEN_RTC_WDG_EN		BIT_RTC_WDG_EN

#define WDG_CLK				32768
#define WDG_UNLOCK_KEY		0xE551

#define ANA_WDG_LOAD_TIMEOUT_NUM	(10000)

#ifdef WDG_NEW_VERSION
#define WDG_LOAD_TIMER_VALUE(value) \
	do{\
		sci_adi_raw_write( WDG_LOAD_HIGH, (uint16_t)(((value) >> 16 ) & 0xffff));\
		sci_adi_raw_write( WDG_LOAD_LOW , (uint16_t)((value)  & 0xffff) );\
	}while(0)
#else
#define WDG_LOAD_TIMER_VALUE(value) \
	do{\
		uint32_t   cnt  =  0;\
		sci_adi_raw_write( WDG_LOAD_HIGH, (uint16_t)(((value) >> 16 ) & 0xffff));\
        sci_adi_raw_write( WDG_LOAD_LOW , (uint16_t)((value)  & 0xffff) );\
		while((sci_adi_read(WDG_INT_RAW) & WDG_LD_BUSY_BIT) && ( cnt < ANA_WDG_LOAD_TIMEOUT_NUM )) cnt++;\
	}while(0)
#endif

#define HWRST_STATUS_RECOVERY		(0x20)
#define HWRST_STATUS_NORMAL			(0X40)
#define HWRST_STATUS_ALARM			(0X50)
#define HWRST_STATUS_SLEEP 			(0X60)
#define HWRST_STATUS_FASTBOOT		(0X30)
#define HWRST_STATUS_SPECIAL		(0x70)
#define HWRST_STATUS_PANIC			(0X80)
#define HWRST_STATUS_CFTREBOOT		(0x90)
#define HWRST_STATUS_AUTODLOADER	(0xa0)
#define HWRST_STATUS_IQMODE			(0xb0)

void sprd_set_reboot_mode(const char *cmd);
void sprd_turnon_watchdog(unsigned int ms);
void sprd_turnoff_watchdog(void);

#endif
