#ifndef __DEV_COOLING_H__
#define __DEV_COOLING_H__

#include <linux/of.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <soc/samsung/exynos-devfreq.h>

extern struct thermal_cooling_device *exynos_dev_cooling_register(struct device_node *np, struct exynos_devfreq_data *data);

#endif
