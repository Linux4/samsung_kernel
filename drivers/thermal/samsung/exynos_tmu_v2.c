/*
 * exynos_tmu.c - Samsung EXYNOS TMU (Thermal Management Unit)
 *
 *  Copyright (C) 2014 Samsung Electronics
 *  Bartlomiej Zolnierkiewicz <b.zolnierkie@samsung.com>
 *  Lukasz Majewski <l.majewski@samsung.com>
 *
 *  Copyright (C) 2011 Samsung Electronics
 *  Donggeun Kim <dg77.kim@samsung.com>
 *  Amit Daniel Kachhap <amit.kachhap@linaro.org>
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
 *
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/cpufreq.h>
#include <linux/suspend.h>
#include <linux/pm_qos.h>
#include <linux/threads.h>
#include <linux/thermal.h>
#include <linux/gpu_cooling.h>
#include <linux/isp_cooling.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/debug-snapshot.h>
#include <soc/samsung/tmu.h>
#include <soc/samsung/ect_parser.h>
#ifdef CONFIG_EXYNOS_MCINFO
#include <soc/samsung/exynos-mcinfo.h>
#endif
#include <soc/samsung/cal-if.h>

#include "exynos_tmu.h"
#include "../thermal_core.h"
#ifdef CONFIG_EXYNOS_ACPM_THERMAL
#include "exynos_acpm_tmu.h"
#include "soc/samsung/exynos-pmu.h"
#endif
#include <soc/samsung/exynos-cpuhp.h>

#define EXYNOS_GPU_THERMAL_ZONE_ID		(3)

#ifdef CONFIG_MCU_IPC
#include "linux/mcu_ipc.h"
#endif

#ifdef CONFIG_SEC_PM
#include <linux/sec_class.h>
#endif

#ifdef CONFIG_EXYNOS_ACPM_THERMAL
static struct acpm_tmu_cap cap;
static unsigned int num_of_devices, suspended_count;
static bool cp_call_mode;
static bool is_aud_on(void)
{
	unsigned int val;

	exynos_pmu_read(PMUREG_AUD_STATUS, &val);

	pr_info("%s AUD_STATUS %d\n", __func__, val);

	return ((val & PMUREG_AUD_STATUS_MASK) == PMUREG_AUD_STATUS_MASK);
}

#ifdef CONFIG_MCU_IPC
#define CP_MBOX_NUM		3
#define CP_MBOX_MASK		1
#define CP_MBOX_SHIFT		25
static bool is_cp_net_conn(void)
{
	unsigned int val;

	val = mbox_extract_value(MCU_CP, CP_MBOX_NUM, CP_MBOX_MASK, CP_MBOX_SHIFT);

	pr_info("%s CP mobx value %d\n", __func__, val);

	return !!val;
}
#else
static bool is_cp_net_conn(void)
{
	return false;
}
#endif
#else
static bool suspended;
static DEFINE_MUTEX (thermal_suspend_lock);
#endif
static bool is_cpu_hotplugged_out;

int exynos_build_static_power_table(struct device_node *np, int **var_table,
		unsigned int *var_volt_size, unsigned int *var_temp_size)
{
	int i, j, ret = -EINVAL;
	int ratio, asv_group, cal_id;

	void *gen_block;
	struct ect_gen_param_table *volt_temp_param = NULL, *asv_param = NULL;
	int cpu_ratio_table[16] = { 0, 18, 22, 27, 33, 40, 49, 60, 73, 89, 108, 131, 159, 194, 232, 250};
	int g3d_ratio_table[16] = { 0, 25, 29, 35, 41, 48, 57, 67, 79, 94, 110, 130, 151, 162, 162, 162};
	int *ratio_table, *var_coeff_table, *asv_coeff_table;
	struct thermal_zone_device *tz;

	if (of_property_read_u32(np, "cal-id", &cal_id)) {
		if (of_property_read_u32(np, "g3d_cmu_cal_id", &cal_id)) {
			pr_err("%s: Failed to get cal-id\n", __func__);
			return -EINVAL;
		}
	}

	ratio = cal_asv_get_ids_info(cal_id);
	asv_group = cal_asv_get_grp(cal_id);

	if (asv_group < 0 || asv_group > 15)
		asv_group = 0;

	gen_block = ect_get_block("GEN");
	if (gen_block == NULL) {
		pr_err("%s: Failed to get gen block from ECT\n", __func__);
		return ret;
	}

	tz = thermal_zone_get_zone_by_cool_np(np);

	if (!strcmp(tz->type, "MID")) {
		volt_temp_param = ect_gen_param_get_table(gen_block, "DTM_MID_VOLT_TEMP");
		asv_param = ect_gen_param_get_table(gen_block, "DTM_MID_ASV");
		ratio_table = cpu_ratio_table;
	}
	else if (!strcmp(tz->type, "BIG")) {
		volt_temp_param = ect_gen_param_get_table(gen_block, "DTM_BIG_VOLT_TEMP");
		asv_param = ect_gen_param_get_table(gen_block, "DTM_BIG_ASV");
		ratio_table = cpu_ratio_table;
	}
	else if (!strcmp(tz->type, "G3D")) {
		volt_temp_param = ect_gen_param_get_table(gen_block, "DTM_G3D_VOLT_TEMP");
		asv_param = ect_gen_param_get_table(gen_block, "DTM_G3D_ASV");
		ratio_table = g3d_ratio_table;
	}
	else {
		pr_err("%s: Thermal zone %s does not use PIDTM\n", __func__, tz->type);
		return -EINVAL;
	}

	if (!ratio)
		ratio = ratio_table[asv_group];

	if (volt_temp_param && asv_param) {
		*var_volt_size = volt_temp_param->num_of_row - 1;
		*var_temp_size = volt_temp_param->num_of_col - 1;

		var_coeff_table = kzalloc(sizeof(int) *
							volt_temp_param->num_of_row *
							volt_temp_param->num_of_col,
							GFP_KERNEL);
		if (!var_coeff_table)
			goto err_mem;

		asv_coeff_table = kzalloc(sizeof(int) *
							asv_param->num_of_row *
							asv_param->num_of_col,
							GFP_KERNEL);
		if (!asv_coeff_table)
			goto free_var_coeff;

		*var_table = kzalloc(sizeof(int) *
							volt_temp_param->num_of_row *
							volt_temp_param->num_of_col,
							GFP_KERNEL);
		if (!*var_table)
			goto free_asv_coeff;

		memcpy(var_coeff_table, volt_temp_param->parameter,
			sizeof(int) * volt_temp_param->num_of_row * volt_temp_param->num_of_col);
		memcpy(asv_coeff_table, asv_param->parameter,
			sizeof(int) * asv_param->num_of_row * asv_param->num_of_col);
		memcpy(*var_table, volt_temp_param->parameter,
			sizeof(int) * volt_temp_param->num_of_row * volt_temp_param->num_of_col);
	} else {
		pr_err("%s: Failed to get param table from ECT\n", __func__);
		return -EINVAL;
	}

	for (i = 1; i <= *var_volt_size; i++) {
		long asv_coeff = (long)asv_coeff_table[3 * i + 0] * asv_group * asv_group
				+ (long)asv_coeff_table[3 * i + 1] * asv_group
				+ (long)asv_coeff_table[3 * i + 2];
		asv_coeff = asv_coeff / 100;

		for (j = 1; j <= *var_temp_size; j++) {
			long var_coeff = (long)var_coeff_table[i * (*var_temp_size + 1) + j];
			var_coeff =  ratio * var_coeff * asv_coeff;
			var_coeff = var_coeff / 100000;
			(*var_table)[i * (*var_temp_size + 1) + j] = (int)var_coeff;
		}
	}

	ret = 0;

free_asv_coeff:
	kfree(asv_coeff_table);
free_var_coeff:
	kfree(var_coeff_table);
err_mem:
	return ret;
}

/* list of multiple instance for each thermal sensor */
static LIST_HEAD(dtm_dev_list);

static void exynos_report_trigger(struct exynos_tmu_data *p)
{
	struct thermal_zone_device *tz = p->tzd;

	if (!tz) {
		pr_err("No thermal zone device defined\n");
		return;
	}

	thermal_zone_device_update(tz, THERMAL_EVENT_UNSPECIFIED);
}

static int exynos_tmu_initialize(struct platform_device *pdev)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct thermal_zone_device *tz = data->tzd;
	enum thermal_trip_type type;
	int i, temp;
	unsigned char threshold[8] = {0, };
	unsigned char inten = 0;

	mutex_lock(&data->lock);

	for (i = (of_thermal_get_ntrips(tz) - 1); i >= 0; i--) {
		tz->ops->get_trip_type(tz, i, &type);

		if (type == THERMAL_TRIP_PASSIVE)
			continue;

		tz->ops->get_trip_temp(tz, i, &temp);

		threshold[i] = (unsigned char)(temp / MCELSIUS);
		inten |= (1 << i);
	}
	exynos_acpm_tmu_set_threshold(tz->id, threshold);
	exynos_acpm_tmu_set_interrupt_enable(tz->id, inten);

	mutex_unlock(&data->lock);

	return 0;
}

static void exynos_tmu_control(struct platform_device *pdev, bool on)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	mutex_lock(&data->lock);
	exynos_acpm_tmu_tz_control(data->tzd->id, on);
	data->enabled = on;
	mutex_unlock(&data->lock);
}


#define MCINFO_LOG_THRESHOLD	(4)

static int exynos_get_temp(void *p, int *temp)
{
	struct exynos_tmu_data *data = p;
#ifndef CONFIG_EXYNOS_ACPM_THERMAL
	struct thermal_cooling_device *cdev = NULL;
	struct thermal_zone_device *tz;
	struct thermal_instance *instance;
#endif
#ifdef CONFIG_EXYNOS_MCINFO
	unsigned int mcinfo_count;
	unsigned int mcinfo_result[4] = {0, 0, 0, 0};
	unsigned int mcinfo_logging = 0;
	unsigned int mcinfo_temp = 0;
	unsigned int i;
#endif
	int acpm_temp = 0, stat = 0;
	int acpm_data[2];
	unsigned long long dbginfo;
	unsigned int limited_max_freq = 0;

	if (!data || !data->enabled)
		return -EINVAL;

	mutex_lock(&data->lock);

	exynos_acpm_tmu_set_read_temp(data->tzd->id, &acpm_temp, &stat, acpm_data);

	*temp = acpm_temp * MCELSIUS;

	if (data->id == 0)
	    limited_max_freq = PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE;
	else if (data->id == 1)
	    limited_max_freq = PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE;

	if (data->limited_frequency_2) {
		if (data->limited == 0) {
			if (*temp >= data->limited_threshold_2) {
				pm_qos_update_request(&data->thermal_limit_request,
						data->limited_frequency_2);
				data->limited = 2;
			} else if (*temp >= data->limited_threshold) {
				pm_qos_update_request(&data->thermal_limit_request,
						data->limited_frequency);
				data->limited = 1;
			}
		} else if (data->limited == 1) {
			if (*temp >= data->limited_threshold_2) {
				pm_qos_update_request(&data->thermal_limit_request,
						data->limited_frequency_2);
				data->limited = 2;
			}
			else if (*temp < data->limited_threshold_release) {
				pm_qos_update_request(&data->thermal_limit_request,
						limited_max_freq);
				data->limited = 0;
			}
		} else if (data->limited == 2) {
			if (*temp < data->limited_threshold_release) {
				pm_qos_update_request(&data->thermal_limit_request,
						limited_max_freq);
				data->limited = 0;
			} else if (*temp < data->limited_threshold_release_2) {
				pm_qos_update_request(&data->thermal_limit_request,
						data->limited_frequency);
				data->limited = 1;
			} 
		}
	} else if (data->limited_frequency) {
		if (data->limited == 0) {
			if (*temp >= data->limited_threshold) {
				pm_qos_update_request(&data->thermal_limit_request,
						data->limited_frequency);
				data->limited = 1;
			}
		} else {
			if (*temp < data->limited_threshold_release) {
				pm_qos_update_request(&data->thermal_limit_request,
						limited_max_freq);
				data->limited = 0;
			}
		}
	}

	mutex_unlock(&data->lock);

#ifndef CONFIG_EXYNOS_ACPM_THERMAL
	tz = data->tzd;

	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		if (instance->cdev) {
			cdev = instance->cdev;
			break;
		}
	}

	if (!cdev)
		return 0;

	mutex_lock(&thermal_suspend_lock);

	if (cdev->ops->set_cur_temp && data->id != 1)
		cdev->ops->set_cur_temp(cdev, suspended, *temp / 1000);

	mutex_unlock(&thermal_suspend_lock);
#endif

	dbginfo = (((unsigned long long)acpm_data[0]) | (((unsigned long long)acpm_data[1]) << 32));
	dbg_snapshot_thermal(data, *temp / 1000, data->tmu_name, dbginfo);
#ifdef CONFIG_EXYNOS_MCINFO
	if (data->id == 0) {
		mcinfo_count = get_mcinfo_base_count();
		get_refresh_rate(mcinfo_result);

		for (i = 0; i < mcinfo_count; i++) {
			mcinfo_temp |= (mcinfo_result[i] & 0xf) << (8 * i);

			if (mcinfo_result[i] >= MCINFO_LOG_THRESHOLD)
				mcinfo_logging = 1;
		}

		if (mcinfo_logging == 1)
			dbg_snapshot_thermal(NULL, mcinfo_temp, "MCINFO", 0);
	}
#endif
	return 0;
}

#ifdef CONFIG_SEC_BOOTSTAT
void sec_bootstat_get_thermal(int *temp)
{
	struct exynos_tmu_data *data;

	list_for_each_entry(data, &dtm_dev_list, node) {
		if (!strncasecmp(data->tmu_name, "BIG", THERMAL_NAME_LENGTH)) {
			exynos_get_temp(data, &temp[0]);
			temp[0] /= 1000;
		} else if (!strncasecmp(data->tmu_name, "MID", THERMAL_NAME_LENGTH)) {
			exynos_get_temp(data, &temp[1]);
			temp[1] /= 1000;
		} else if (!strncasecmp(data->tmu_name, "LITTLE", THERMAL_NAME_LENGTH)) {
			exynos_get_temp(data, &temp[2]);
			temp[2] /= 1000;
		} else if (!strncasecmp(data->tmu_name, "G3D", THERMAL_NAME_LENGTH)) {
			exynos_get_temp(data, &temp[3]);
			temp[3] /= 1000;
		} else if (!strncasecmp(data->tmu_name, "ISP", THERMAL_NAME_LENGTH)) {
			exynos_get_temp(data, &temp[4]);
			temp[4] /= 1000;
		} else
			continue;
	}
}
#endif

static int exynos_get_trend(void *p, int trip, enum thermal_trend *trend)
{
	struct exynos_tmu_data *data = p;
	struct thermal_zone_device *tz = data->tzd;
	int trip_temp, ret = 0;

	ret = tz->ops->get_trip_temp(tz, trip, &trip_temp);
	if (ret < 0)
		return ret;

	if (tz->temperature >= trip_temp)
		*trend = THERMAL_TREND_RAISE_FULL;
	else
		*trend = THERMAL_TREND_DROP_FULL;

	return 0;
}

#ifdef CONFIG_THERMAL_EMULATION
static int exynos_tmu_set_emulation(void *drv_data, int temp)
{
	struct exynos_tmu_data *data = drv_data;
	int ret = -EINVAL;
	unsigned char emul_temp;

	if (temp && temp < MCELSIUS)
		goto out;

	mutex_lock(&data->lock);
	emul_temp = (unsigned char)(temp / MCELSIUS);
	exynos_acpm_tmu_set_emul_temp(data->tzd->id, emul_temp);
	mutex_unlock(&data->lock);
	return 0;
out:
	return ret;
}
#else
static int exynos_tmu_set_emulation(void *drv_data, int temp)
	{ return -EINVAL; }
#endif /* CONFIG_THERMAL_EMULATION */

static irqreturn_t exynos_tmu_threaded_irq(int irq, void *id)
{
	struct exynos_tmu_data *data = id;

	exynos_report_trigger(data);
	mutex_lock(&data->lock);

	exynos_acpm_tmu_clear_tz_irq(data->tzd->id);

	mutex_unlock(&data->lock);
	enable_irq(data->irq);

	return IRQ_HANDLED;
}

static irqreturn_t exynos_tmu_irq(int irq, void *id)
{
	disable_irq_nosync(irq);

	return IRQ_WAKE_THREAD;
}

#ifndef CONFIG_EXYNOS_ACPM_THERMAL
static int exynos_pm_notifier(struct notifier_block *notifier,
			unsigned long event, void *v)
{
	struct exynos_tmu_data *devnode;
	struct thermal_cooling_device *cdev = NULL;
	struct thermal_zone_device *tz;
	struct thermal_instance *instance;

	switch (event) {
	case PM_SUSPEND_PREPARE:
		mutex_lock(&thermal_suspend_lock);
		suspended = true;
		list_for_each_entry(devnode, &dtm_dev_list, node) {
			tz = devnode->tzd;
			list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
				if (instance->cdev) {
					cdev = instance->cdev;
					break;
				}
			}

			if (cdev && cdev->ops->set_cur_temp && devnode->id != 1)
				cdev->ops->set_cur_temp(cdev, suspended, 0);
		}
		mutex_unlock(&thermal_suspend_lock);
		break;
	case PM_POST_SUSPEND:
		mutex_lock(&thermal_suspend_lock);
		suspended = false;
		list_for_each_entry(devnode, &dtm_dev_list, node) {
			cdev = devnode->cool_dev;
			tz = devnode->tzd;
			list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
				if (instance->cdev) {
					cdev = instance->cdev;
					break;
				}
			}

			if (cdev && cdev->ops->set_cur_temp && devnode->id != 1)
				cdev->ops->set_cur_temp(cdev, suspended, devnode->tzd->temperature / 1000);
		}
		mutex_unlock(&thermal_suspend_lock);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_tmu_pm_notifier = {
	.notifier_call = exynos_pm_notifier,
};
#endif

static const struct of_device_id exynos_tmu_match[] = {
	{ .compatible = "samsung,exynos-tmu-v2", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, exynos_tmu_match);

static int exynos_map_dt_data(struct platform_device *pdev)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct resource res;
	const char *tmu_name;

	if (!data || !pdev->dev.of_node)
		return -ENODEV;

	data->np = pdev->dev.of_node;

	if (of_property_read_u32(pdev->dev.of_node, "id", &data->id)) {
		dev_err(&pdev->dev, "failed to get TMU ID\n");
		return -ENODEV;
	}

	data->irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (data->irq <= 0) {
		dev_err(&pdev->dev, "failed to get IRQ\n");
		return -ENODEV;
	}

	if (of_address_to_resource(pdev->dev.of_node, 0, &res)) {
		dev_err(&pdev->dev, "failed to get Resource 0\n");
		return -ENODEV;
	}

	data->base = devm_ioremap(&pdev->dev, res.start, resource_size(&res));
	if (!data->base) {
		dev_err(&pdev->dev, "Failed to ioremap memory\n");
		return -EADDRNOTAVAIL;
	}

	if (of_property_read_string(pdev->dev.of_node, "tmu_name", &tmu_name)) {
		dev_err(&pdev->dev, "failed to get tmu_name\n");
	} else
		strncpy(data->tmu_name, tmu_name, THERMAL_NAME_LENGTH);

	data->hotplug_enable = of_property_read_bool(pdev->dev.of_node, "hotplug_enable");
	if (data->hotplug_enable) {
		dev_info(&pdev->dev, "thermal zone use hotplug function \n");
		of_property_read_u32(pdev->dev.of_node, "hotplug_in_threshold",
					&data->hotplug_in_threshold);
		if (!data->hotplug_in_threshold)
			dev_err(&pdev->dev, "No input hotplug_in_threshold \n");

		of_property_read_u32(pdev->dev.of_node, "hotplug_out_threshold",
					&data->hotplug_out_threshold);
		if (!data->hotplug_out_threshold)
			dev_err(&pdev->dev, "No input hotplug_out_threshold \n");
	}

	return 0;
}

static int exynos_throttle_cpu_hotplug(void *p, int temp)
{
	struct exynos_tmu_data *data = p;
	int ret = 0;
	struct cpumask mask;

	temp = temp / MCELSIUS;

	if (is_cpu_hotplugged_out) {
		if (temp < data->hotplug_in_threshold) {
			/*
			 * If current temperature is lower than low threshold,
			 * call cluster1_cores_hotplug(false) for hotplugged out cpus.
			 */
			exynos_cpuhp_request("DTM", *cpu_possible_mask, 0);
			is_cpu_hotplugged_out = false;
		}
	} else {
		if (temp >= data->hotplug_out_threshold) {
			/*
			 * If current temperature is higher than high threshold,
			 * call cluster1_cores_hotplug(true) to hold temperature down.
			 */
			is_cpu_hotplugged_out = true;
			cpumask_and(&mask, cpu_possible_mask, cpu_coregroup_mask(0));
			exynos_cpuhp_request("DTM", mask, 0);
		}
	}

	return ret;
}

static const struct thermal_zone_of_device_ops exynos_hotplug_sensor_ops = {
	.get_temp = exynos_get_temp,
	.set_emul_temp = exynos_tmu_set_emulation,
	.throttle_cpu_hotplug = exynos_throttle_cpu_hotplug,
};

static const struct thermal_zone_of_device_ops exynos_sensor_ops = {
	.get_temp = exynos_get_temp,
	.set_emul_temp = exynos_tmu_set_emulation,
	.get_trend = exynos_get_trend,
};


static ssize_t
hotplug_out_temp_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->hotplug_out_threshold);
}

static ssize_t
hotplug_out_temp_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	int hotplug_out = 0;

	mutex_lock(&data->lock);

	if (kstrtos32(buf, 10, &hotplug_out)) {
		mutex_unlock(&data->lock);
		return -EINVAL;
	}

	data->hotplug_out_threshold = hotplug_out;

	mutex_unlock(&data->lock);

	return count;
}

static ssize_t
hotplug_in_temp_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->hotplug_in_threshold);
}

static ssize_t
hotplug_in_temp_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	int hotplug_in = 0;

	mutex_lock(&data->lock);

	if (kstrtos32(buf, 10, &hotplug_in)) {
		mutex_unlock(&data->lock);
		return -EINVAL;
	}

	data->hotplug_in_threshold = hotplug_in;

	mutex_unlock(&data->lock);

	return count;
}

static DEVICE_ATTR(hotplug_out_temp, S_IWUSR | S_IRUGO, hotplug_out_temp_show,
		hotplug_out_temp_store);

static DEVICE_ATTR(hotplug_in_temp, S_IWUSR | S_IRUGO, hotplug_in_temp_show,
		hotplug_in_temp_store);

static struct attribute *exynos_tmu_attrs[] = {
	&dev_attr_hotplug_out_temp.attr,
	&dev_attr_hotplug_in_temp.attr,
	NULL,
};

static const struct attribute_group exynos_tmu_attr_group = {
	.attrs = exynos_tmu_attrs,
};

#define PARAM_NAME_LENGTH	25
#define FRAC_BITS 10	/* FRAC_BITS should be same with power_allocator */

#if defined(CONFIG_ECT)
static int exynos_tmu_ect_get_param(struct ect_pidtm_block *pidtm_block, char *name)
{
	int i;
	int param_value = -1;

	for (i = 0; i < pidtm_block->num_of_parameter; i++) {
		if (!strncasecmp(pidtm_block->param_name_list[i], name, PARAM_NAME_LENGTH)) {
			param_value = pidtm_block->param_value_list[i];
			break;
		}
	}

	return param_value;
}

static int exynos_tmu_parse_ect(struct exynos_tmu_data *data)
{
	struct thermal_zone_device *tz = data->tzd;
	struct __thermal_zone *__tz;

	if (!tz)
		return -EINVAL;

	__tz = (struct __thermal_zone *)tz->devdata;

	if (strncasecmp(tz->tzp->governor_name, "power_allocator",
						THERMAL_NAME_LENGTH)) {
		/* if governor is not power_allocator */
		void *thermal_block;
		struct ect_ap_thermal_function *function;
		int i, temperature;
		int hotplug_threshold_temp = 0, hotplug_flag = 0;
		unsigned int freq;

		thermal_block = ect_get_block(BLOCK_AP_THERMAL);
		if (thermal_block == NULL) {
			pr_err("Failed to get thermal block");
			return -EINVAL;
		}

		pr_info("%s %d thermal zone_name = %s\n", __func__, __LINE__, tz->type);

		function = ect_ap_thermal_get_function(thermal_block, tz->type);
		if (function == NULL) {
			pr_err("Failed to get thermal block %s", tz->type);
			return -EINVAL;
		}

		__tz->ntrips = __tz->num_tbps = function->num_of_range;
		pr_info("Trip count parsed from ECT : %d, zone : %s", function->num_of_range, tz->type);

		for (i = 0; i < function->num_of_range; ++i) {
			temperature = function->range_list[i].lower_bound_temperature;
			freq = function->range_list[i].max_frequency;
			__tz->trips[i].temperature = temperature  * MCELSIUS;
			__tz->tbps[i].value = freq;

			pr_info("Parsed From ECT : [%d] Temperature : %d, frequency : %u\n",
					i, temperature, freq);

			if (function->range_list[i].flag != hotplug_flag) {
				if (function->range_list[i].flag != hotplug_flag) {
					hotplug_threshold_temp = temperature;
					hotplug_flag = function->range_list[i].flag;
					data->hotplug_out_threshold = temperature;

					if (i)
						data->hotplug_in_threshold = function->range_list[i-1].lower_bound_temperature;

					pr_info("[ECT]hotplug_threshold : %d\n", hotplug_threshold_temp);
					pr_info("[ECT]hotplug_in_threshold : %d\n", data->hotplug_in_threshold);
					pr_info("[ECT]hotplug_out_threshold : %d\n", data->hotplug_out_threshold);
				}
			}

			if (hotplug_threshold_temp != 0)
				data->hotplug_enable = true;
			else
				data->hotplug_enable = false;

		}
	} else {
		void *block;
		struct ect_pidtm_block *pidtm_block;
		int i, temperature, value;
		int hotplug_out_threshold = 0, hotplug_in_threshold = 0, limited_frequency = 0;
		int limited_threshold = 0, limited_threshold_release = 0;

		block = ect_get_block(BLOCK_PIDTM);
		if (block == NULL) {
			pr_err("Failed to get PIDTM block");
			return -EINVAL;
		}

		pr_info("%s %d thermal zone_name = %s\n", __func__, __LINE__, tz->type);

		pidtm_block = ect_pidtm_get_block(block, tz->type);
		if (pidtm_block == NULL) {
			pr_err("Failed to get PIDTM block %s", tz->type);
			return -EINVAL;
		}

		__tz->ntrips = pidtm_block->num_of_temperature;

		for (i = 0; i < pidtm_block->num_of_temperature; ++i) {
			temperature = pidtm_block->temperature_list[i];
			__tz->trips[i].temperature = temperature * MCELSIUS;

			pr_info("Parsed From ECT : [%d] Temperature : %d\n", i, temperature);
		}

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "k_po")) != -1) {
			pr_info("Parse from ECT k_po: %d\n", value);
			tz->tzp->k_po = value << FRAC_BITS;
		} else
			pr_err("Fail to parse k_po parameter\n");

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "k_pu")) != -1) {
			pr_info("Parse from ECT k_pu: %d\n", value);
			tz->tzp->k_pu = value << FRAC_BITS;
		} else
			pr_err("Fail to parse k_pu parameter\n");

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "k_i")) != -1) {
			pr_info("Parse from ECT k_i: %d\n", value);
			tz->tzp->k_i = value << FRAC_BITS;
		} else
			pr_err("Fail to parse k_i parameter\n");

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "i_max")) != -1) {
			pr_info("Parse from ECT i_max: %d\n", value);
			tz->tzp->integral_max = value;
		} else
			pr_err("Fail to parse i_max parameter\n");

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "integral_cutoff")) != -1) {
			pr_info("Parse from ECT integral_cutoff: %d\n", value);
			tz->tzp->integral_cutoff = value;
		} else
			pr_err("Fail to parse integral_cutoff parameter\n");

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "p_control_t")) != -1) {
			pr_info("Parse from ECT p_control_t: %d\n", value);
			tz->tzp->sustainable_power = value;
		} else
			pr_err("Fail to parse p_control_t parameter\n");

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "hotplug_out_threshold")) != -1) {
			pr_info("Parse from ECT hotplug_out_threshold: %d\n", value);
			hotplug_out_threshold = value;
		}

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "hotplug_in_threshold")) != -1) {
			pr_info("Parse from ECT hotplug_in_threshold: %d\n", value);
			hotplug_in_threshold = value;
		}

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "limited_frequency")) != -1) {
			pr_info("Parse from ECT limited_frequency: %d\n", value);
			limited_frequency = value;
			data->limited_frequency = limited_frequency;
			data->limited = false;
		}

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "limited_threshold")) != -1) {
			pr_info("Parse from ECT limited_threshold: %d\n", value);
			limited_threshold = value * MCELSIUS;
			__tz->trips[3].temperature = limited_threshold;
			data->limited_threshold = limited_threshold;
		}

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "limited_threshold_release")) != -1) {
			pr_info("Parse from ECT limited_threshold_release: %d\n", value);
			limited_threshold_release = value * MCELSIUS;
			data->limited_threshold_release = limited_threshold_release;
		}

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "limited_frequency_1")) != -1) {
			pr_info("Parse from ECT limited_frequency: %d\n", value);
			limited_frequency = value;
			data->limited_frequency = limited_frequency;
			data->limited = false;
		}

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "limited_threshold_1")) != -1) {
			pr_info("Parse from ECT limited_threshold: %d\n", value);
			limited_threshold = value * MCELSIUS;
			__tz->trips[3].temperature = limited_threshold;
			data->limited_threshold = limited_threshold;
		}

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "limited_threshold_release_1")) != -1) {
			pr_info("Parse from ECT limited_threshold_release: %d\n", value);
			limited_threshold_release = value * MCELSIUS;
			data->limited_threshold_release = limited_threshold_release;
		}


		if ((value = exynos_tmu_ect_get_param(pidtm_block, "limited_frequency_2")) != -1) {
			pr_info("Parse from ECT limited_frequency: %d\n", value);
			limited_frequency = value;
			data->limited_frequency_2 = limited_frequency;
			data->limited = false;
		}

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "limited_threshold_2")) != -1) {
			pr_info("Parse from ECT limited_threshold: %d\n", value);
			limited_threshold = value * MCELSIUS;
			__tz->trips[4].temperature = limited_threshold;
			data->limited_threshold_2 = limited_threshold;
		}

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "limited_threshold_release_2")) != -1) {
			pr_info("Parse from ECT limited_threshold_release: %d\n", value);
			limited_threshold_release = value * MCELSIUS;
			data->limited_threshold_release_2 = limited_threshold_release;
		}


		if (hotplug_out_threshold != 0 && hotplug_in_threshold != 0) {
			data->hotplug_out_threshold = hotplug_out_threshold;
			data->hotplug_in_threshold = hotplug_in_threshold;
			data->hotplug_enable = true;
		} else
			data->hotplug_enable = false;
	}
	return 0;
};
#endif

#ifdef CONFIG_MALI_DEBUG_KERNEL_SYSFS
struct exynos_tmu_data *gpu_thermal_data;
#endif

static int exynos_tmu_probe(struct platform_device *pdev)
{
	struct exynos_tmu_data *data;
	int ret;

	data = devm_kzalloc(&pdev->dev, sizeof(struct exynos_tmu_data),
					GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	platform_set_drvdata(pdev, data);
	mutex_init(&data->lock);

	ret = exynos_map_dt_data(pdev);
	if (ret)
		goto err_sensor;

	/*
	 * data->tzd must be registered before calling exynos_tmu_initialize(),
	 * requesting irq and calling exynos_tmu_control().
	 */
	if (data->hotplug_enable) {
		exynos_cpuhp_register("DTM", *cpu_online_mask, 0);
	}

	data->tzd = thermal_zone_of_sensor_register(&pdev->dev, 0, data,
						    data->hotplug_enable ?
						    &exynos_hotplug_sensor_ops :
						    &exynos_sensor_ops);
	if (IS_ERR(data->tzd)) {
		ret = PTR_ERR(data->tzd);
		dev_err(&pdev->dev, "Failed to register sensor: %d\n", ret);
		goto err_sensor;
	}

#if defined(CONFIG_ECT)
	exynos_tmu_parse_ect(data);

	if (data->limited_frequency) {
	    if (data->id == 0) {
		pm_qos_add_request(&data->thermal_limit_request,
					PM_QOS_CLUSTER2_FREQ_MAX,
					PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE);
	    } else if (data->id == 1) {
		pm_qos_add_request(&data->thermal_limit_request,
					PM_QOS_CLUSTER1_FREQ_MAX,
					PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);
	    }
	}
#endif

	ret = exynos_tmu_initialize(pdev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to initialize TMU\n");
		goto err_thermal;
	}

	ret = devm_request_threaded_irq(&pdev->dev, data->irq, exynos_tmu_irq, exynos_tmu_threaded_irq,
				IRQF_SHARED | IRQF_GIC_MULTI_TARGET, dev_name(&pdev->dev), data);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq: %d\n", data->irq);
		goto err_thermal;
	}

	exynos_tmu_control(pdev, true);

	ret = sysfs_create_group(&pdev->dev.kobj, &exynos_tmu_attr_group);
	if (ret)
		dev_err(&pdev->dev, "cannot create exynos tmu attr group");

	mutex_lock(&data->lock);
	list_add_tail(&data->node, &dtm_dev_list);
	num_of_devices++;
	mutex_unlock(&data->lock);

	if (list_is_singular(&dtm_dev_list))
#ifdef CONFIG_EXYNOS_ACPM_THERMAL
		exynos_acpm_tmu_set_init(&cap);
#else
		register_pm_notifier(&exynos_tmu_pm_notifier);
#endif

	if (!IS_ERR(data->tzd))
		data->tzd->ops->set_mode(data->tzd, THERMAL_DEVICE_ENABLED);

#ifdef CONFIG_MALI_DEBUG_KERNEL_SYSFS
	if (data->id == EXYNOS_GPU_THERMAL_ZONE_ID)
		gpu_thermal_data = data;
#endif

	return 0;

err_thermal:
	thermal_zone_of_sensor_unregister(&pdev->dev, data->tzd);
err_sensor:
	return ret;
}

static int exynos_tmu_remove(struct platform_device *pdev)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct thermal_zone_device *tzd = data->tzd;
	struct exynos_tmu_data *devnode;

#ifndef CONFIG_EXYNOS_ACPM_THERMAL
	if (list_is_singular(&dtm_dev_list))
		unregister_pm_notifier(&exynos_tmu_pm_notifier);
#endif

	thermal_zone_of_sensor_unregister(&pdev->dev, tzd);
	exynos_tmu_control(pdev, false);

	mutex_lock(&data->lock);
	list_for_each_entry(devnode, &dtm_dev_list, node) {
		if (devnode->id == data->id) {
			list_del(&devnode->node);
			num_of_devices--;
			break;
		}
	}
	mutex_unlock(&data->lock);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int exynos_tmu_suspend(struct device *dev)
{
#ifdef CONFIG_EXYNOS_ACPM_THERMAL
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	int cp_state;

	suspended_count++;
	disable_irq(data->irq);

	cp_call_mode = is_aud_on() && cap.acpm_irq;
	if (cp_call_mode) {
		if (suspended_count == num_of_devices) {
			exynos_acpm_tmu_set_cp_call();
			pr_info("%s: TMU suspend w/ AUD-on\n", __func__);
		}
	} else {
		exynos_tmu_control(pdev, false);
		if (suspended_count == num_of_devices) {
			cp_state = is_cp_net_conn();
			exynos_acpm_tmu_set_suspend(cp_state);
			pr_info("%s: TMU suspend w/ cp_state %d\n", __func__, cp_state);
		}
	}
#else
	exynos_tmu_control(to_platform_device(dev), false);
#endif

	return 0;
}

static int exynos_tmu_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
#ifdef CONFIG_EXYNOS_ACPM_THERMAL
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	int temp, stat;

	if (suspended_count == num_of_devices)
		exynos_acpm_tmu_set_resume();

	if (!cp_call_mode) {
		exynos_tmu_control(pdev, true);
	}

	exynos_acpm_tmu_set_read_temp(data->tzd->id, &temp, &stat, NULL);

	pr_info("%s: thermal zone %d temp %d stat %d\n",
			__func__, data->tzd->id, temp, stat);

	enable_irq(data->irq);
	suspended_count--;

	if (!suspended_count)
		pr_info("%s: TMU resume complete\n", __func__);
#else
	exynos_tmu_control(pdev, true);
#endif

	return 0;
}

static SIMPLE_DEV_PM_OPS(exynos_tmu_pm,
			 exynos_tmu_suspend, exynos_tmu_resume);
#define EXYNOS_TMU_PM	(&exynos_tmu_pm)
#else
#define EXYNOS_TMU_PM	NULL
#endif

static struct platform_driver exynos_tmu_driver = {
	.driver = {
		.name   = "exynos-tmu",
		.pm     = EXYNOS_TMU_PM,
		.of_match_table = exynos_tmu_match,
		.suppress_bind_attrs = true,
	},
	.probe = exynos_tmu_probe,
	.remove	= exynos_tmu_remove,
};

module_platform_driver(exynos_tmu_driver);

#ifdef CONFIG_EXYNOS_ACPM_THERMAL
static void exynos_acpm_tmu_test_cp_call(bool mode)
{
	struct exynos_tmu_data *devnode;

	if (mode) {
		list_for_each_entry(devnode, &dtm_dev_list, node) {
			disable_irq(devnode->irq);
		}
		exynos_acpm_tmu_set_cp_call();
	} else {
		exynos_acpm_tmu_set_resume();
		list_for_each_entry(devnode, &dtm_dev_list, node) {
			enable_irq(devnode->irq);
		}
	}
}

static int emul_call_get(void *data, unsigned long long *val)
{
	*val = exynos_acpm_tmu_is_test_mode();

	return 0;
}

static int emul_call_set(void *data, unsigned long long val)
{
	int status = exynos_acpm_tmu_is_test_mode();

	if ((val == 0 || val == 1) && (val != status)) {
		exynos_acpm_tmu_set_test_mode(val);
		exynos_acpm_tmu_test_cp_call(val);
	}

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(emul_call_fops, emul_call_get, emul_call_set, "%llu\n");

static int log_print_set(void *data, unsigned long long val)
{
	if (val == 0 || val == 1)
		exynos_acpm_tmu_log(val);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(log_print_fops, NULL, log_print_set, "%llu\n");

static ssize_t ipc_dump1_read(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	union {
		unsigned int dump[2];
		unsigned char val[8];
	} data;
	char buf[48];
	ssize_t ret;

	exynos_acpm_tmu_ipc_dump(0, data.dump);

	ret = snprintf(buf, sizeof(buf), "%3d %3d %3d %3d %3d %3d %3d\n",
			data.val[1], data.val[2], data.val[3],
			data.val[4], data.val[5], data.val[6], data.val[7]);
	if (ret < 0)
		return ret;

	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);
}

static ssize_t ipc_dump2_read(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	union {
		unsigned int dump[2];
		unsigned char val[8];
	} data;
	char buf[48];
	ssize_t ret;

	exynos_acpm_tmu_ipc_dump(EXYNOS_GPU_THERMAL_ZONE_ID, data.dump);

	ret = snprintf(buf, sizeof(buf), "%3d %3d %3d %3d %3d %3d %3d\n",
			data.val[1], data.val[2], data.val[3],
			data.val[4], data.val[5], data.val[6], data.val[7]);
	if (ret < 0)
		return ret;

	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);

}

static const struct file_operations ipc_dump1_fops = {
	.open = simple_open,
	.read = ipc_dump1_read,
	.llseek = default_llseek,
};

static const struct file_operations ipc_dump2_fops = {
	.open = simple_open,
	.read = ipc_dump2_read,
	.llseek = default_llseek,
};

#endif

static struct dentry *debugfs_root;

static int exynos_thermal_create_debugfs(void)
{
	debugfs_root = debugfs_create_dir("exynos-thermal", NULL);
	if (!debugfs_root) {
		pr_err("Failed to create exynos thermal debugfs\n");
		return 0;
	}

#ifdef CONFIG_EXYNOS_ACPM_THERMAL
	debugfs_create_file("emul_call", 0644, debugfs_root, NULL, &emul_call_fops);
	debugfs_create_file("log_print", 0644, debugfs_root, NULL, &log_print_fops);
	debugfs_create_file("ipc_dump1", 0644, debugfs_root, NULL, &ipc_dump1_fops);
	debugfs_create_file("ipc_dump2", 0644, debugfs_root, NULL, &ipc_dump2_fops);
#endif
	return 0;
}
arch_initcall(exynos_thermal_create_debugfs);

#ifdef CONFIG_SEC_PM

#define NR_THERMAL_SENSOR_MAX	10

static ssize_t exynos_tmu_curr_temp(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct exynos_tmu_data *data;
	int temp[NR_THERMAL_SENSOR_MAX] = {0, };
	int i, id_max = 0;
	ssize_t ret = 0;

	list_for_each_entry(data, &dtm_dev_list, node) {
		if (data->id < NR_THERMAL_SENSOR_MAX) {
			exynos_get_temp(data, &temp[data->id]);
			temp[data->id] /= 1000;

			if (id_max < data->id)
				id_max = data->id;
		} else {
			pr_err("%s: id:%d %s\n", __func__, data->id,
					data->tmu_name);
			continue;
		}
	}

	for (i = 0; i <= id_max; i++)
		ret += sprintf(buf + ret, "%d,", temp[i]);

	sprintf(buf + ret - 1, "\n");

	return ret;
}

static DEVICE_ATTR(curr_temp, 0444, exynos_tmu_curr_temp, NULL);

static struct attribute *exynos_tmu_sec_pm_attributes[] = {
	&dev_attr_curr_temp.attr,
	NULL
};

static const struct attribute_group exynos_tmu_sec_pm_attr_grp = {
	.attrs = exynos_tmu_sec_pm_attributes,
};

static int __init exynos_tmu_sec_pm_init(void)
{
	int ret = 0;
	struct device *dev;

	dev = sec_device_create(NULL, "exynos_tmu");

	if (IS_ERR(dev)) {
		pr_err("%s: failed to create device\n", __func__);
		return PTR_ERR(dev);
	}

	ret = sysfs_create_group(&dev->kobj, &exynos_tmu_sec_pm_attr_grp);
	if (ret) {
		pr_err("%s: failed to create sysfs group(%d)\n", __func__, ret);
		goto err_create_sysfs;
	}

	return ret;

err_create_sysfs:
	sec_device_destroy(dev->devt);

	return ret;
}

late_initcall(exynos_tmu_sec_pm_init);
#endif /* CONFIG_SEC_PM */

MODULE_DESCRIPTION("EXYNOS TMU Driver");
MODULE_AUTHOR("Donggeun Kim <dg77.kim@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:exynos-tmu");
