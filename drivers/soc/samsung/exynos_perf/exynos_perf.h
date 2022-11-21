/*
 * Exynos Performance
 * Author: Jungwook Kim, jwook1.kim@samsung.com
 * Created: 2014
 * Updated: 2015, 2019
 */

#ifndef EXYNOS_PERF_H
#define EXYNOS_PERF_H __FILE__

extern int exynos_perf_cpu_init(struct platform_device *pdev);
extern int exynos_perf_cpufreq_profile_init(struct platform_device *pdev);
extern int exynos_perf_cpuidle_profile_init(struct platform_device *pdev);
extern int exynos_perf_ncmemcpy_init(struct platform_device *pdev);
extern int exynos_perf_gmc_init(struct platform_device *pdev);

#endif
