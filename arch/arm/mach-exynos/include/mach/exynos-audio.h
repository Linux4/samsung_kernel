/* linux/arch/arm/mach-exynos/include/mach/exynos-audio.h
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _EXYNOS_AUDIO_H
#define _EXYNOS_AUDIO_H __FILE__

#include <mach/regs-pmu.h>

#ifdef CONFIG_SOC_EXYNOS3475
extern void aud_mixer_MCLKO_enable(void);
extern void aud_mixer_MCLKO_disable(void);
#endif

static inline void exynos5_audio_set_mclk(bool enable, bool forced)
{
	static unsigned int mclk_usecount = 0;

	pr_debug("%s: %s forced %d, use_cnt %d\n", __func__,
				enable ? "on" : "off", forced, mclk_usecount);

	/* forced disable */
	if (forced && !enable) {
		pr_info("%s: mclk disbled\n", __func__);
		mclk_usecount = 0;
#ifdef CONFIG_SOC_EXYNOS3475
		aud_mixer_MCLKO_disable();
#else
		writel(0x1001, EXYNOS_PMU_PMU_DEBUG);
#endif
		return;
	}

	if (enable) {
		if (mclk_usecount == 0) {
			pr_info("%s: mclk enabled\n", __func__);
#ifdef CONFIG_SOC_EXYNOS3475
			aud_mixer_MCLKO_enable();
#else
			writel(0x1000, EXYNOS_PMU_PMU_DEBUG);
#endif
		}
		mclk_usecount++;
	} else {
		if (--mclk_usecount > 0)
			return;
#ifdef CONFIG_SOC_EXYNOS3475
		aud_mixer_MCLKO_disable();
#else
		writel(0x1001, EXYNOS_PMU_PMU_DEBUG);
#endif
		pr_info("%s: mclk disabled\n", __func__);
	}
}


#endif /* _EXYNOS_AUDIO_H */

