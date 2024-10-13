#include <linux/timer.h>
#include <linux/wakeup_reason.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/notifier.h>
#include <linux/suspend.h>
#include "wt_pm_debug.h"

ktime_t wt_pm_debug_dpm_calc_start(void)
{
	return ktime_get();
}

void wt_pm_debug_dpm_calc_end(struct device *dev, ktime_t starttime, char *pm_flag)
{
	ktime_t calltime;
	u64 usecs64;
	int usecs;
	int result;

	calltime = ktime_get();
	usecs64 = ktime_to_ns(ktime_sub(calltime, starttime));
	do_div(usecs64, NSEC_PER_USEC);
	usecs = usecs64;
	if (usecs == 0)
		usecs = 1;
	result = usecs / USEC_PER_MSEC;
	if(result > 100){
		printk("PM: Device %s %s time cost %d ms\n", dev_name(dev), pm_flag, result);
	}
}
