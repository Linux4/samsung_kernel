/*
 * Exynos Performance
 * Author: Jungwook Kim, jwook1.kim@samsung.com
 * Created: 2014
 * Updated: 2016, 2019, 2020
 */

#include <linux/of.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "exynos_perf.h"

static int exynos_perf_probe(struct platform_device *pdev)
{
	exynos_perf_cpu_init(pdev);

	exynos_perf_cpufreq_profile_init(pdev);

	exynos_perf_cpuidle_profile_init(pdev);

	exynos_perf_ncmemcpy_init(pdev);

	exynos_perf_gmc_init(pdev);

	return 0;
}

static const struct of_device_id of_exynos_perf_match[] = {
	{ .compatible = "samsung,exynos-perf", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_exynos_perf_match);

static struct platform_driver exynos_perf_driver = {
	.driver = {
		.name	= "exynos-perf",
		.owner = THIS_MODULE,
		.of_match_table = of_exynos_perf_match,
	},
	.probe		= exynos_perf_probe,
};

module_platform_driver(exynos_perf_driver);

MODULE_AUTHOR("Jungwook Kim");
MODULE_DESCRIPTION("Exynos Perf driver");
MODULE_LICENSE("GPL");
