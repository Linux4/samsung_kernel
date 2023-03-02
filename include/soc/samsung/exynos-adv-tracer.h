#ifndef __EXYNOS_ADV_TRACER_H__
#define __EXYNOS_ADV_TRACER_H__

#include <linux/types.h>

struct adv_tracer_info {
	unsigned int plugin_num;
	struct device *dev;
	unsigned int enter_wfi;
	atomic_t in_arraydump;
};

extern void *adv_tracer_memcpy_align_4(void *dest, const void *src, unsigned int n);
#ifdef CONFIG_EXYNOS_ADV_TRACER
extern void adv_tracer_wait_ipi(int cpu);
extern int adv_tracer_arraydump(void);
extern int adv_tracer_get_arraydump_state(void);
#else
#define adv_tracer_wait_ipi(a) do { } while (0)
#define adv_tracer_arraydump() do { } while (0)
#define adv_tracer_get_arraydump_state() do { } while(0)
#endif
#endif

