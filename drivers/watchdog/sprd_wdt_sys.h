
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

#ifndef _SPRD_WDT_SYS_H_
#define _SPRD_WDT_SYS_H_
#include <linux/spinlock.h>
#include <linux/bitops.h>
#include <soc/sprd/adi.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/hardware.h>
#ifdef CONFIG_SEC_DEBUG
#include <asm/sec/sec_debug.h>
#endif

unsigned long ap_wdt_addr;
unsigned long a7_wdt_addr;

#define AP_WDG_LOAD_LOW			(ap_wdt_addr + 0x00)
#define AP_WDG_LOAD_HIGH		(ap_wdt_addr + 0x04)
#define AP_WDG_CTRL				(ap_wdt_addr + 0x08)
#define AP_WDG_INT_CLR			(ap_wdt_addr + 0x0C)
#define AP_WDG_INT_RAW			(ap_wdt_addr + 0x10)
#define AP_WDG_INT_MSK			(ap_wdt_addr + 0x14)
#define AP_WDG_CNT_LOW			(ap_wdt_addr + 0x18)
#define AP_WDG_CNT_HIGH			(ap_wdt_addr + 0x1C)
#define AP_WDG_LOCK				(ap_wdt_addr + 0x20)

#define CA7_WDG_LOAD_LOW		(a7_wdt_addr + 0x00)
#define CA7_WDG_LOAD_HIGH		(a7_wdt_addr + 0x04)
#define CA7_WDG_CTRL			(a7_wdt_addr + 0x08)
#define CA7_WDG_INT_CLR			(a7_wdt_addr + 0x0C)
#define CA7_WDG_INT_RAW			(a7_wdt_addr + 0x10)
#define CA7_WDG_INT_MSK			(a7_wdt_addr + 0x14)
#define CA7_WDG_CNT_LOW			(a7_wdt_addr + 0x18)
#define CA7_WDG_CNT_HIGH		(a7_wdt_addr + 0x1C)
#define CA7_WDG_LOCK			(a7_wdt_addr + 0x20)
#define CA7_WDG_IRQ_LOAD_LOW	(a7_wdt_addr + 0x2c)
#define CA7_WDG_IRQ_LOAD_HIGH	(a7_wdt_addr + 0x30)

#define AP_WDG_LOAD_TIMER_VALUE(value) \
	do { \
		uint32_t   cnt =  0;\
		sci_glb_write( AP_WDG_LOAD_HIGH, (uint16_t)(((value) >> 16 ) & 0xffff), -1UL); \
		sci_glb_write( AP_WDG_LOAD_LOW , (uint16_t)((value)  & 0xffff), -1UL ); \
		while((sci_glb_read(AP_WDG_INT_RAW, -1UL) & WDG_LD_BUSY_BIT) && ( cnt < ANA_WDG_LOAD_TIMEOUT_NUM )) cnt++; \
	} while (0)

#define CA7_WDG_LOAD_TIMER_VALUE(margin, irq) \
	do { \
		uint32_t   cnt          =  0; \
		sci_glb_write( CA7_WDG_LOAD_HIGH, (uint16_t)(((margin) >> 16 ) & 0xffff), -1UL); \
		sci_glb_write( CA7_WDG_LOAD_LOW , (uint16_t)((margin)  & 0xffff), -1UL ); \
		sci_glb_write( CA7_WDG_IRQ_LOAD_HIGH, (uint16_t)(((irq) >> 16 ) & 0xffff), -1UL); \
		sci_glb_write( CA7_WDG_IRQ_LOAD_LOW , (uint16_t)((irq)  & 0xffff), -1UL ); \
		while((sci_glb_read(CA7_WDG_INT_RAW, -1UL) & WDG_LD_BUSY_BIT) && ( cnt < ANA_WDG_LOAD_TIMEOUT_NUM )) cnt++; \
	} while (0)


#ifdef CONFIG_SC_INTERNAL_WATCHDOG
static inline void FEED_ALL_WDG(int chip_margin, int ap_margin,
		int ca7_margin, int ca7_irq_margin)
{
	if(!sec_debug_level.en.kernel_fault) {
		AP_WDG_LOAD_TIMER_VALUE(ap_margin * WDG_CLK);
		WDG_LOAD_TIMER_VALUE(chip_margin * WDG_CLK);
		CA7_WDG_LOAD_TIMER_VALUE(ca7_margin * WDG_CLK, (ca7_margin - ca7_irq_margin) * WDG_CLK);
	} else {
		CA7_WDG_LOAD_TIMER_VALUE(ca7_margin * WDG_CLK, (ca7_margin - ca7_irq_margin) * WDG_CLK);
	}
}
#else
static inline void FEED_ALL_WDG(int chip_margin, int ap_margin,
		int ca7_margin, int ca7_irq_margin) { }
#endif

#define ENABLE_ALL_WDG() \
	do { \
		if(!sec_debug_level.en.kernel_fault) { \
			sci_adi_set(WDG_CTRL, WDG_CNT_EN_BIT | WDG_RST_EN_BIT); \
			sci_glb_set(AP_WDG_CTRL, WDG_CNT_EN_BIT | WDG_RST_EN_BIT); \
			sci_glb_set(CA7_WDG_CTRL, WDG_CNT_EN_BIT | WDG_RST_EN_BIT | WDG_INT_EN_BIT); \
		} else { \
			sci_glb_set(CA7_WDG_CTRL, WDG_CNT_EN_BIT | WDG_INT_EN_BIT); \
		} \
	} while (0)

#define DISABLE_ALL_WDG() \
	do { \
		sci_adi_clr(WDG_CTRL, WDG_CNT_EN_BIT | WDG_RST_EN_BIT); \
		sci_glb_clr(AP_WDG_CTRL, WDG_CNT_EN_BIT | WDG_RST_EN_BIT); \
		sci_glb_clr(CA7_WDG_CTRL, WDG_CNT_EN_BIT | WDG_RST_EN_BIT | WDG_INT_EN_BIT); \
	} while (0)

#define WATCHDOG_THREAD_USER_BIT			0
#define WATCHDOG_THREAD_KERNEL_BIT			1

#define WATCHDOG_FEED_BY_USER_ST_BIT		(0)
#define WATCHDOG_FEED_BY_USER_BIT 			(1)
#define WATCHDOG_FEED_BY_KERNEL_BIT			(2)
#define WATCHDOG_FEED_ENABLE_BIT			(30)
#define WATCHDOG_FEED_START_BIT				(31)

#define WATCHDOG_FEED_BY_KERNEL_THREAD ( \
		(1 << WATCHDOG_FEED_BY_USER_BIT) | \
		(1 << WATCHDOG_FEED_BY_KERNEL_BIT) \
		)

#define WATCHDOG_FEED_BY_USER_THREAD ( \
		(1 << WATCHDOG_FEED_BY_USER_ST_BIT) \
		)

#define WATCHDOG_FEED_BY_THREAD \
		(WATCHDOG_FEED_BY_KERNEL_THREAD | WATCHDOG_FEED_BY_USER_THREAD)

#ifdef CONFIG_SC_INTERNAL_WATCHDOG
static inline void watchdog_start(int chip_margin, int ap_margin,
				  int ca7_margin, int ca7_irq_margin)
{
	if(!sec_debug_level.en.kernel_fault) {
		/* start the chip watchdog */
		sci_adi_set(ANA_AGEN, AGEN_WDG_EN);
		sci_adi_set(ANA_RTC_CLK_EN, AGEN_RTC_WDG_EN);
		sci_adi_raw_write(WDG_LOCK, WDG_UNLOCK_KEY);
		sci_adi_set(WDG_CTRL, WDG_NEW_VER_EN);
		WDG_LOAD_TIMER_VALUE(chip_margin * WDG_CLK);
		sci_adi_set(WDG_CTRL, WDG_CNT_EN_BIT | WDG_RST_EN_BIT);

		/* start ap watchdog */
		sci_glb_set(REG_AON_APB_APB_EB0, BIT_AP_WDG_EB);
		sci_glb_set(REG_AON_APB_APB_RTC_EB, BIT_AP_WDG_RTC_EB);
		sci_glb_write(AP_WDG_LOCK, WDG_UNLOCK_KEY, -1UL);
		sci_glb_set(AP_WDG_CTRL, WDG_NEW_VER_EN);
		AP_WDG_LOAD_TIMER_VALUE(ap_margin * WDG_CLK);
		sci_glb_set(AP_WDG_CTRL, WDG_CNT_EN_BIT | WDG_RST_EN_BIT);

		/* start ca7 watchdog */
		sci_glb_set(REG_AON_APB_APB_EB1, BIT_CA7_WDG_EB);
		sci_glb_set(REG_AON_APB_APB_RTC_EB, BIT_CA7_WDG_RTC_EB);
		sci_glb_write(CA7_WDG_LOCK, WDG_UNLOCK_KEY, -1UL);
		sci_glb_set(CA7_WDG_CTRL, WDG_NEW_VER_EN);
		CA7_WDG_LOAD_TIMER_VALUE(ca7_margin * WDG_CLK, (ca7_margin - ca7_irq_margin) * WDG_CLK);
		sci_glb_set(CA7_WDG_CTRL, WDG_CNT_EN_BIT | WDG_RST_EN_BIT | WDG_INT_EN_BIT);

	} else {
		/* start ca7 watchdog irq mode only */
		sci_glb_set(REG_AON_APB_APB_EB1, BIT_CA7_WDG_EB);
		sci_glb_set(REG_AON_APB_APB_RTC_EB, BIT_CA7_WDG_RTC_EB);
		sci_glb_write(CA7_WDG_LOCK, WDG_UNLOCK_KEY, -1UL);
		sci_glb_set(CA7_WDG_CTRL, WDG_NEW_VER_EN);
		CA7_WDG_LOAD_TIMER_VALUE(ca7_margin * WDG_CLK, (ca7_margin - ca7_irq_margin) * WDG_CLK);
		sci_glb_set(CA7_WDG_CTRL, WDG_CNT_EN_BIT | WDG_INT_EN_BIT);
	}
}

#else
static inline void watchdog_start(int chip_margin, int ap_margin,
		int ca7_margin, int ca7_irq_margin) { }
#endif
#endif
