#ifndef _EXYNOS_CORESIGHT_H_
#define _EXYNOS_CORESIGHT_H_

#ifdef CONFIG_EXYNOS_CORESIGHT
unsigned long exynos_cs_read_pc(int cpu);
#else
static inline unsigned long exynos_cs_read_pc(int cpu)
{
	return 0;
}
#endif

#endif
