/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/notifier.h>
#include <linux/string.h>
#include <linux/kobject.h>
#include <linux/list.h>
#include <linux/notifier.h>
#include <linux/relay.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/mtd/super.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/types.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
#include <linux/sysdev.h>
#endif
#include <mach/hardware.h>
#include <mach/irqs.h>
#include <asm/atomic.h>
#include <asm/io.h>

#define COMM_TIMER_PHY_ADDR     0xD4080000

#define	TMR_WMER		0x64
#define	TMR_WMR			0x68
#define	TMR_WVR			0x6c
#define	TMR_WSR			0x70

#define	TMR_WICR		0x80
#define	TMR_CER			0x84
#define	TMR_CMR			0x88

#define	TMR_WCR			0x98
#define	TMR_WFAR		0x9c
#define	TMR_WSAR		0xa0

#define	WATCHDOG_1ST_ACCESS_KEY	0xbaba
#define	WATCHDOG_2ND_ACCESS_KEY	0xeb10

#define WATCHDOG_COUNT_MASK	0x1
#define WATCHDOG_RESET_MASK	0x2

#define IRQ_COMM_WDT_ID IRQ_PXA988_WDT

typedef enum
{
	SUCCESSFUL,
	ERROR
} WATCHDOG_HW_RETURN_CODE;
extern void watchDogCountStop(void);
WATCHDOG_HW_RETURN_CODE watchDogDeactive(void);

