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

#ifdef CONFIG_ARCH_SCX35
#define WDG_BASE		(ANA_WDG_BASE)
#define SPRD_ANA_BASE		(ANA_CTL_GLB_BASE)
#define AP_WDG_LOAD_LOW        (SPRD_WDG_BASE + 0x00)
#define AP_WDG_LOAD_HIGH       (SPRD_WDG_BASE + 0x04)
#define AP_WDG_CTRL            (SPRD_WDG_BASE + 0x08)
#define AP_WDG_INT_CLR         (SPRD_WDG_BASE + 0x0C)
#define AP_WDG_INT_RAW         (SPRD_WDG_BASE + 0x10)
#define AP_WDG_INT_MSK         (SPRD_WDG_BASE + 0x14)
#define AP_WDG_CNT_LOW         (SPRD_WDG_BASE + 0x18)
#define AP_WDG_CNT_HIGH        (SPRD_WDG_BASE + 0x1C)
#define AP_WDG_LOCK            (SPRD_WDG_BASE + 0x20)

#define CA7_WDG_LOAD_LOW        (SPRD_CA7WDG_BASE + 0x00)
#define CA7_WDG_LOAD_HIGH       (SPRD_CA7WDG_BASE + 0x04)
#define CA7_WDG_CTRL            (SPRD_CA7WDG_BASE + 0x08)
#define CA7_WDG_INT_CLR         (SPRD_CA7WDG_BASE + 0x0C)
#define CA7_WDG_INT_RAW         (SPRD_CA7WDG_BASE + 0x10)
#define CA7_WDG_INT_MSK         (SPRD_CA7WDG_BASE + 0x14)
#define CA7_WDG_CNT_LOW         (SPRD_CA7WDG_BASE + 0x18)
#define CA7_WDG_CNT_HIGH        (SPRD_CA7WDG_BASE + 0x1C)
#define CA7_WDG_LOCK            (SPRD_CA7WDG_BASE + 0x20)
#define CA7_WDG_IRQ_LOAD_LOW    (SPRD_CA7WDG_BASE + 0x2c)
#define CA7_WDG_IRQ_LOAD_HIGH   (SPRD_CA7WDG_BASE + 0x30)
#else
#define WDG_BASE		(SPRD_MISC_BASE + 0x40)
#define SPRD_ANA_BASE           (SPRD_MISC_BASE + 0x600)
#endif


#define WDG_LOAD_LOW        (WDG_BASE + 0x00)
#define WDG_LOAD_HIGH       (WDG_BASE + 0x04)
#define WDG_CTRL            (WDG_BASE + 0x08)
#define WDG_INT_CLR         (WDG_BASE + 0x0C)
#define WDG_INT_RAW         (WDG_BASE + 0x10)
#define WDG_INT_MSK         (WDG_BASE + 0x14)
#define WDG_CNT_LOW         (WDG_BASE + 0x18)
#define WDG_CNT_HIGH        (WDG_BASE + 0x1C)
#define WDG_LOCK            (WDG_BASE + 0x20)

#define WDG_INT_EN_BIT          BIT(0)
#define WDG_CNT_EN_BIT          BIT(1)
#define WDG_NEW_VER_EN		BIT(2)
#define WDG_INT_CLEAR_BIT       BIT(0)
#define WDG_RST_CLEAR_BIT       BIT(3)
#define WDG_LD_BUSY_BIT         BIT(4)
#define WDG_RST_EN_BIT		BIT(3)


#define ANA_REG_BASE            SPRD_ANA_BASE	/*  0x82000600 */


#ifdef CONFIG_ARCH_SCX35
#define WDG_NEW_VERSION
#define ANA_RST_STATUS          (ANA_REG_GLB_POR_RST_MONITOR)
#define ANA_AGEN                (ANA_REG_GLB_ARM_MODULE_EN)
#define ANA_RTC_CLK_EN		(ANA_REG_GLB_RTC_CLK_EN)

#define AGEN_WDG_EN             BIT(2)
#define AGEN_RTC_WDG_EN         BIT(2)

#else

#define ANA_RST_STATUS          (ANA_REG_BASE + 0X88)
#define ANA_AGEN                (ANA_REG_BASE + 0x00)

#define AGEN_WDG_EN             BIT(2)
#define AGEN_RTC_ARCH_EN        BIT(8)
#define AGEN_RTC_WDG_EN         BIT(10)

#endif

#define WDG_CLK                 32768
#define WDG_UNLOCK_KEY          0xE551

#define ANA_WDG_LOAD_TIMEOUT_NUM    (10000)

#ifdef WDG_NEW_VERSION
#define WDG_LOAD_TIMER_VALUE(value) \
        do{\
                    sci_adi_raw_write( WDG_LOAD_HIGH, (uint16_t)(((value) >> 16 ) & 0xffff));\
                    sci_adi_raw_write( WDG_LOAD_LOW , (uint16_t)((value)  & 0xffff) );\
        }while(0)

#define AP_WDG_LOAD_TIMER_VALUE(value) \
	do { \
		uint32_t   cnt          =  0;\
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

#include <linux/spinlock.h>
#include <linux/bitops.h>
#include <mach/adi.h>
#include <mach/sci_glb_regs.h>
#include <mach/sci.h>
#include <mach/hardware.h>

#ifdef CONFIG_SC_INTERNAL_WATCHDOG
static inline void FEED_ALL_WDG(int chip_margin, int ap_margin,
		int ca7_margin, int ca7_irq_margin)
{
	AP_WDG_LOAD_TIMER_VALUE(ap_margin * WDG_CLK);
	WDG_LOAD_TIMER_VALUE(chip_margin * WDG_CLK);
	CA7_WDG_LOAD_TIMER_VALUE(ca7_margin * WDG_CLK, (ca7_margin - ca7_irq_margin) * WDG_CLK);
}
#else
static inline void FEED_ALL_WDG(int chip_margin, int ap_margin,
		int ca7_margin, int ca7_irq_margin) { }
#endif

#define ENABLE_ALL_WDG() \
	do { \
		sci_adi_set(WDG_CTRL, WDG_CNT_EN_BIT | WDG_RST_EN_BIT); \
		sci_glb_set(AP_WDG_CTRL, WDG_CNT_EN_BIT | WDG_RST_EN_BIT); \
		sci_glb_set(CA7_WDG_CTRL, WDG_CNT_EN_BIT | WDG_RST_EN_BIT | WDG_INT_EN_BIT); \
	} while (0)

#define DISABLE_ALL_WDG() \
	do { \
		sci_adi_clr(WDG_CTRL, WDG_CNT_EN_BIT | WDG_RST_EN_BIT); \
		sci_glb_clr(AP_WDG_CTRL, WDG_CNT_EN_BIT | WDG_RST_EN_BIT); \
		sci_glb_clr(CA7_WDG_CTRL, WDG_CNT_EN_BIT | WDG_RST_EN_BIT | WDG_INT_EN_BIT); \
	} while (0)

#define WATCHDOG_THREAD_USER_BIT 	0
#define WATCHDOG_THREAD_KERNEL_BIT	1

#define WATCHDOG_FEED_BY_USER_ST_BIT 	(0)
#define WATCHDOG_FEED_BY_USER_BIT 			(1)
#define WATCHDOG_FEED_BY_KERNEL_BIT 		(2)
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
}

#else
static inline void watchdog_start(int chip_margin, int ap_margin,
		int ca7_margin, int ca7_irq_margin) { }
#endif
#else

#define WDG_LOAD_TIMER_VALUE(value) \
        do{\
                    uint32_t   cnt          =  0;\
                    sci_adi_raw_write( WDG_LOAD_HIGH, (uint16_t)(((value) >> 16 ) & 0xffff));\
                    sci_adi_raw_write( WDG_LOAD_LOW , (uint16_t)((value)  & 0xffff) );\
                    while((sci_adi_read(WDG_INT_RAW) & WDG_LD_BUSY_BIT) && ( cnt < ANA_WDG_LOAD_TIMEOUT_NUM )) cnt++;\
        }while(0)
#endif

#define HWRST_STATUS_RECOVERY (0x20)
#define HWRST_STATUS_NORMAL (0X40)
#define HWRST_STATUS_ALARM (0X50)
#define HWRST_STATUS_SLEEP (0X60)
#define HWRST_STATUS_FASTBOOT (0X30)
#define HWRST_STATUS_SPECIAL (0x70)
#define HWRST_STATUS_PANIC (0X80)
#define HWRST_STATUS_CFTREBOOT (0x90)
#define HWRST_STATUS_AUTODLOADER (0xa0)
#define HWRST_STATUS_IQMODE (0xb0)

#ifdef CONFIG_SC_INTERNAL_WATCHDOG
void sprd_set_reboot_mode(const char *cmd);
void sprd_turnon_watchdog(unsigned int ms);
void sprd_turnoff_watchdog(void);
#else
static inline void sprd_set_reboot_mode(const char *cmd) {}

static inline void sprd_turnon_watchdog(unsigned int ms) {}

static inline void sprd_turnoff_watchdog(void) {}
#endif
#endif
