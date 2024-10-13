#ifndef _WT_PM_DEBUG_H
#define _WT_PM_DEBUG_H

#include <linux/types.h>

#ifdef CONFIG_WT_PM_DEBUG

void wt_pm_debug_wakeup_reason(int irq);
ktime_t wt_pm_debug_dpm_calc_start(void);
void wt_pm_debug_dpm_calc_end(struct device *dev, ktime_t starttime, char *pm_flag);

#else /* !CONFIG_WT_PM_DEBUG */

static inline void wt_pm_debug_wakeup_reason(int irq)
{
}

static inline ktime_t wt_pm_debug_dpm_calc_start(void)
{
	return 0;
}

static inline void wt_pm_debug_dpm_calc_end(struct device *dev, ktime_t starttime, char *pm_flag)
{
}

#endif  /* CONFIG_WT_PM_DEBUG */
#endif