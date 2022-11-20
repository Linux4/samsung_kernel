/*
 * exynos_tmu.h - Samsung EXYNOS TMU (Thermal Management Unit)
 *
 *  Copyright (C) 2011 Samsung Electronics
 *  Donggeun Kim <dg77.kim@samsung.com>
 *  Amit Daniel Kachhap <amit.daniel@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _EXYNOS_TMU_H
#define _EXYNOS_TMU_H
#include <linux/cpu_cooling.h>
#include <linux/gpu_cooling.h>
#include <linux/isp_cooling.h>
#include <linux/pm_qos.h>

#define MCELSIUS        1000

/**
 * struct exynos_tmu_data : A structure to hold the private data of the TMU
	driver
 * @id: identifier of the one instance of the TMU controller.
 * @base: base address of the single instance of the TMU controller.
 * @irq: irq number of the TMU controller.
 * @soc: id of the SOC type.
 * @irq_work: pointer to the irq work structure.
 * @lock: lock to implement synchronization.
 * @regulator: pointer to the TMU regulator structure.
 * @reg_conf: pointer to structure to register with core thermal.
 * @ntrip: number of supported trip points.
 * @tmu_initialize: SoC specific TMU initialization method
 * @tmu_control: SoC specific TMU control method
 * @tmu_read: SoC specific TMU temperature read method
 * @tmu_set_emulation: SoC specific TMU emulation setting method
 * @tmu_clear_irqs: SoC specific TMU interrupts clearing method
 */
struct exynos_tmu_data {
	int id;
	/* Throttle hotplug related variables */
	bool hotplug_enable;
	int hotplug_in_threshold;
	int hotplug_out_threshold;
	int limited_frequency;
	int limited_threshold;
	int limited_threshold_release;
	int limited_frequency_2;
	int limited_threshold_2;
	int limited_threshold_release_2;
	struct pm_qos_request thermal_limit_request;
	unsigned int limited;
	void __iomem *base;
	int irq;
	struct work_struct irq_work;
	struct mutex lock;
	struct thermal_zone_device *tzd;
	unsigned int ntrip;
	bool enabled;
	struct thermal_cooling_device *cool_dev;
	struct list_head node;
	char tmu_name[THERMAL_NAME_LENGTH + 1];
	struct device_node *np;
};

extern int exynos_build_static_power_table(struct device_node *np, int **var_table,
		unsigned int *var_volt_size, unsigned int *var_temp_size);

#ifdef CONFIG_EXYNOS_ACPM_THERMAL
#if defined(CONFIG_SOC_EXYNOS9810)
#define PMUREG_AUD_STATUS           0x4004
#define PMUREG_AUD_STATUS_MASK          0xF
#elif defined(CONFIG_SOC_EXYNOS9820) || defined(CONFIG_SOC_EXYNOS9830)
#define PMUREG_AUD_STATUS           0x1904
#define PMUREG_AUD_STATUS_MASK          0x1
#endif
#endif

#endif /* _EXYNOS_TMU_H */
