#ifndef __SPRD_WDT_FIQ_H__
#define __SPRD_WDT_FIQ_H__

#include <linux/watchdog.h>

int sprd_wdt_fiq_get_dev(struct watchdog_device **wdd);

#endif

