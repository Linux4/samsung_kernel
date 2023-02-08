/*
 * exynos_thermal.c - Samsung EXYNOS TMU (Thermal Management Unit)
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

#include <linux/module.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/workqueue.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/platform_data/exynos_thermal.h>
#include <linux/thermal.h>
#include <linux/cpufreq.h>
#include <linux/cpu_cooling.h>
#include <linux/of.h>
#include <linux/reboot.h>
#include <linux/suspend.h>
#include <plat/cpu.h>
#include <mach/tmu.h>
#include <mach/cpufreq.h>
#include <mach/regs-pmu.h>
#include <mach/irqs.h>

/*****************************************************************************/
/*			  	   Variables	      	     		     */
/*****************************************************************************/
static int tmu_old_state;
static int gpu_old_state;
static int mif_old_state;
static bool is_suspending;
static struct exynos_thermal_zone *th_zone;
static struct platform_device *exynos_tmu_pdev;
int g_count;
unsigned int g_cam_err_count;
extern unsigned int g_cpufreq;
#ifdef CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ
extern unsigned int g_miffreq;
extern unsigned int g_intfreq;
extern unsigned int g_g3dfreq;
#endif
#ifdef CONFIG_ARM_EXYNOS5420_BUS_DEVFREQ
extern bool mif_is_probed;
#endif
/*****************************************************************************/
/* 	        	      Function prototypes 		             */
/*****************************************************************************/
#ifdef CONFIG_ARM_EXYNOS5420_BUS_DEVFREQ
static bool check_mif_probed(void);
#endif
static int exynos_get_mode(struct thermal_zone_device *, enum thermal_device_mode *);
static int exynos_set_mode(struct thermal_zone_device *, enum thermal_device_mode);
static void exynos_report_trigger(void);
static int exynos_get_trip_type(struct thermal_zone_device *, int, enum thermal_trip_type *);
static int exynos_get_trip_temp(struct thermal_zone_device *, int, unsigned long *);
static int exynos_set_trip_temp(struct thermal_zone_device *, int, unsigned long);
static int exynos_get_trip_temp_level(struct thermal_zone_device *, int, bool);
static int exynos_set_trip_temp_level(struct thermal_zone_device *, unsigned int,
		unsigned int, unsigned int);
static int exynos_get_trip_freq(struct thermal_zone_device *, unsigned int, unsigned long *);
static int exynos_set_trip_freq(struct thermal_zone_device *, unsigned int, unsigned long);
static int exynos_get_crit_temp(struct thermal_zone_device *, unsigned long *);
static int exynos_bind(struct thermal_zone_device *, struct thermal_cooling_device *);
static int exynos_unbind(struct thermal_zone_device *, struct thermal_cooling_device *);
static int exynos_get_temp(struct thermal_zone_device *, unsigned long *);
static int exynos_notify(struct thermal_zone_device *, int, enum thermal_trip_type);
static int exynos_get_throttling_count(struct thermal_zone_device *, enum thermal_trip_type);
#ifdef CONFIG_SOC_EXYNOS5260
static int exynos_set_polling_delay(struct thermal_zone_device *);
#endif
static int exynos_register_thermal(struct thermal_sensor_conf *);
static void exynos_unregister_thermal(void);
int exynos_tmu_add_notifier(struct notifier_block *);
void exynos_tmu_call_notifier(enum tmu_noti_state_t);
int exynos_gpu_add_notifier(struct notifier_block *);
void exynos_gpu_call_notifier(enum gpu_noti_state_t);
static int temp_to_code(struct exynos_tmu_data *, u8, int);
static int code_to_temp(struct exynos_tmu_data *, u8, int);
static int exynos_tmu_initialize(struct platform_device *, int);
static void exynos_tmu_get_efuse(struct platform_device *, int);
static void exynos_tmu_control(struct platform_device *, int, bool);
static int exynos_tmu_read(struct exynos_tmu_data *);
static void exynos_check_mif_noti_state(int);
static void exynos_check_tmu_noti_state(int, int);
static void exynos_check_gpu_noti_state(int);
static void exynos_tmu_work(struct work_struct *);
static irqreturn_t exynos_tmu_irq(int, void *);
static int exynos_pm_notifier(struct notifier_block *, unsigned long, void *);
static inline struct exynos_tmu_platform_data *exynos_get_driver_data(struct platform_device *);
static ssize_t exynos_thermal_sensor_temp(struct device *, struct device_attribute *, char *);
static void exynos_tmu_regdump(struct platform_device *, int);
static int __devinit exynos_tmu_probe(struct platform_device *);
static int __devexit exynos_tmu_remove(struct platform_device *);
#ifdef CONFIG_PM
static int exynos_tmu_suspend(struct platform_device *, pm_message_t);
static int exynos_tmu_resume(struct platform_device *);
#endif
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
static int exynos5_tmu_cpufreq_notifier(struct notifier_block *notifier, unsigned long event, void *v);
static struct notifier_block exynos_cpufreq_nb = {
	.notifier_call = exynos5_tmu_cpufreq_notifier,
};
#endif

/*****************************************************************************/
/*      	                Notifier Heads 		     	             */
/*****************************************************************************/
static BLOCKING_NOTIFIER_HEAD(exynos_tmu_notifier);
static BLOCKING_NOTIFIER_HEAD(exynos_gpu_notifier);

/*****************************************************************************/
/* 		    		Implementation     		      	     */
/*****************************************************************************/
#ifdef CONFIG_ARM_EXYNOS5420_BUS_DEVFREQ
static bool check_mif_probed(void)
{
	return mif_is_probed;
}
#endif

/* Get mode callback functions for thermal zone */
static int exynos_get_mode(struct thermal_zone_device *thermal,
			enum thermal_device_mode *mode)
{
	if (th_zone)
		*mode = th_zone->mode;
	return 0;
}

/* Set mode callback functions for thermal zone */
static int exynos_set_mode(struct thermal_zone_device *thermal,
			enum thermal_device_mode mode)
{
	bool on;
	int i;

	if (!th_zone->therm_dev) {
		pr_notice("thermal zone not registered\n");
		return 0;
	}

	mutex_lock(&th_zone->therm_dev->lock);

	if (mode == THERMAL_DEVICE_ENABLED) {
		th_zone->therm_dev->polling_delay = IDLE_INTERVAL;
		on = true;
	} else {
		th_zone->therm_dev->polling_delay = 0;
		on = false;
	}

	for (i = 0; i < EXYNOS_TMU_COUNT; i++)
		exynos_tmu_control(th_zone->exynos4_dev, i, on);

	mutex_unlock(&th_zone->therm_dev->lock);

	th_zone->mode = mode;
	thermal_zone_device_update(th_zone->therm_dev);
	pr_info("thermal polling set for duration=%d msec\n",
				th_zone->therm_dev->polling_delay);
	return 0;
}

/*
 * This function may be called from interrupt based temperature sensor
 * when threshold is changed.
 */
static void exynos_report_trigger(void)
{
	unsigned int i;
	char data[10];
	char *envp[] = { data, NULL };

	if (!th_zone || !th_zone->therm_dev)
		return;

	thermal_zone_device_update(th_zone->therm_dev);

	mutex_lock(&th_zone->therm_dev->lock);
	for (i = 0; i < th_zone->sensor_conf->trip_data.trip_count; i++) {
		if (th_zone->therm_dev->last_temperature <
			th_zone->sensor_conf->trip_data.trip_val[i] * MCELSIUS)
			break;
	}

	if (th_zone->mode == THERMAL_DEVICE_ENABLED) {
		if (i > 0)
			th_zone->therm_dev->polling_delay = ACTIVE_INTERVAL;
		else
			th_zone->therm_dev->polling_delay = IDLE_INTERVAL;
	}

	snprintf(data, sizeof(data), "%u", i);
	kobject_uevent_env(&th_zone->therm_dev->device.kobj, KOBJ_CHANGE, envp);
	mutex_unlock(&th_zone->therm_dev->lock);
}

/* Get trip type callback functions for thermal zone */
static int exynos_get_trip_type(struct thermal_zone_device *thermal, int trip,
				 enum thermal_trip_type *type)
{
	switch (GET_ZONE(trip)) {
	case MONITOR_ZONE:
		*type = THERMAL_TRIP_ACTIVE;
		break;
	case WARN_ZONE:
		*type = THERMAL_TRIP_PASSIVE;
		break;
	case PANIC_ZONE:
		*type = THERMAL_TRIP_CRITICAL;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/* Get trip temperature callback functions for thermal zone */
static int exynos_get_trip_temp(struct thermal_zone_device *thermal, int trip,
				unsigned long *temp)
{
	if (trip < GET_TRIP(MONITOR_ZONE) || trip > GET_TRIP(PANIC_ZONE))
		return -EINVAL;

	*temp = th_zone->sensor_conf->trip_data.trip_val[trip];
	/* convert the temperature into millicelsius */
	*temp = *temp * MCELSIUS;

	return 0;
}

static int exynos_set_trip_temp(struct thermal_zone_device *thermal, int trip,
				unsigned long temp)
{
	struct exynos_tmu_platform_data *pdata;
	struct exynos_tmu_data *data;
	unsigned int interrupt_en = 0, rising_threshold = 0, falling_threshold;
	int threshold_code, i, con;

	data = th_zone->sensor_conf->private_data;
	pdata = data->pdata;

	clk_enable(data->clk);

	th_zone->sensor_conf->trip_data.trip_val[trip] = temp;

	for (i = 0; i < EXYNOS_TMU_COUNT; i++) {
		/* TMU core disable  */
		if (i <= GET_TRIP(PANIC_ZONE))
			writel(0, data->base[i] + EXYNOS_TMU_REG_INTEN);

		con = readl(data->base[i] + EXYNOS_TMU_REG_CONTROL);
		con &= ~(0x3);
		con |= EXYNOS_TMU_CORE_OFF;
		writel(con, data->base[i] + EXYNOS_TMU_REG_CONTROL);

		/* Interrupt pending clear  */
		writel(EXYNOS5_TMU_CLEAR_RISE_INT|EXYNOS5_TMU_CLEAR_FALL_INT,
				data->base[i] + EXYNOS_TMU_REG_INTCLEAR);

		/* Change the trigger levels  */
		rising_threshold = readl(data->base[i] + EXYNOS5_THD_TEMP_RISE);
		falling_threshold = readl(data->base[i] + EXYNOS5_THD_TEMP_FALL);
		threshold_code = temp_to_code(data, temp, i);

		switch (trip) {
		case 0:
			rising_threshold &= ~0xff;
			falling_threshold &= ~0xff;
			rising_threshold |= threshold_code;
			falling_threshold |= threshold_code - GAP_WITH_RISE;
			interrupt_en |= pdata->trigger_level0_en;
			interrupt_en |= pdata->trigger_level0_en << FALL_LEVEL0_SHIFT;
			break;
		case 1:
			rising_threshold &= ~(0xff << THRESH_LEVE1_SHIFT);
			falling_threshold &= ~(0xff << THRESH_LEVE1_SHIFT);
			rising_threshold |= (threshold_code << THRESH_LEVE1_SHIFT);
			falling_threshold |= ((threshold_code - GAP_WITH_RISE) << THRESH_LEVE1_SHIFT);
			interrupt_en |= pdata->trigger_level1_en << RISE_LEVEL1_SHIFT;
			interrupt_en |= pdata->trigger_level1_en << FALL_LEVEL1_SHIFT;
			break;
		case 2:
			rising_threshold &= ~(0xff << THRESH_LEVE2_SHIFT);
			falling_threshold &= ~(0xff << THRESH_LEVE2_SHIFT);
			rising_threshold |= (threshold_code << THRESH_LEVE2_SHIFT);
			falling_threshold |= ((threshold_code - GAP_WITH_RISE) << THRESH_LEVE2_SHIFT);
			interrupt_en |= pdata->trigger_level2_en << RISE_LEVEL2_SHIFT;
			interrupt_en |= pdata->trigger_level2_en << FALL_LEVEL2_SHIFT;
			break;
		case 3:
			rising_threshold &= ~(0xff << THRESH_LEVE3_SHIFT);
			rising_threshold |= (threshold_code << THRESH_LEVE3_SHIFT);
			break;
		}

		writel(rising_threshold, data->base[i] + EXYNOS5_THD_TEMP_RISE);
		writel(falling_threshold, data->base[i] + EXYNOS5_THD_TEMP_FALL);

		/* TMU core enable */
		if (i <= GET_TRIP(PANIC_ZONE))
			writel(interrupt_en, data->base[i] + EXYNOS_TMU_REG_INTEN);

		con = readl(data->base[i] + EXYNOS_TMU_REG_CONTROL);
		con &= ~(0x3 | (0x1 << 12));
		con |= (EXYNOS_TMU_CORE_ON | EXYNOS_THERM_TRIP_EN);
		writel(con, data->base[i] + EXYNOS_TMU_REG_CONTROL);
	}

	clk_disable(data->clk);

	printk(KERN_INFO "sysfs : set trip temp[%d] : %d\n", trip,
			th_zone->sensor_conf->trip_data.trip_val[trip]);

	return 0;
}

static int exynos_get_trip_temp_level(struct thermal_zone_device *thermal, int trip,
				bool passive_type)
{
	if (trip < th_zone->sensor_conf->cooling_data.freq_clip_count) {
		if (passive_type)
			trip++;
		return th_zone->sensor_conf->cooling_data.freq_data[trip].temp_level * MCELSIUS;
	}
	return 0;
}

static int exynos_set_trip_temp_level(struct thermal_zone_device *thermal,
				unsigned int temp0, unsigned int temp1,
				unsigned int temp2)
{
	if (!th_zone->sensor_conf) {
		pr_info("Temperature sensor not initialised\n");
		return -EINVAL;
	}

	th_zone->sensor_conf->cooling_data.freq_data[0].temp_level = temp0;
	th_zone->sensor_conf->cooling_data.freq_data[1].temp_level = temp1;
	th_zone->sensor_conf->cooling_data.freq_data[2].temp_level = temp2;

	return 0;
}

static int exynos_get_trip_freq(struct thermal_zone_device *thermal, unsigned int trip,
				unsigned long *freq)
{
	if (trip > th_zone->sensor_conf->trip_data.trip_count)
		return -EINVAL;

	*freq = th_zone->sensor_conf->cooling_data.freq_data[trip].freq_clip_max;
	return 0;
}

static int exynos_set_trip_freq(struct thermal_zone_device *thermal, unsigned int trip,
		unsigned long freq)
{
	if (MAX_FREQ < freq || MIN_FREQ > freq) {
		pr_info("Input[%d] is out of limit frequency range[%d ~ %d]", trip, MAX_FREQ, MIN_FREQ);
		return 0;
	}

	th_zone->sensor_conf->cooling_data.freq_data[trip].freq_clip_max = freq * kHz;

	printk(KERN_INFO "Set frequency[%d] : %ld\n", trip, freq);

	return 0;
}

/* Get critical temperature callback functions for thermal zone */
static int exynos_get_crit_temp(struct thermal_zone_device *thermal,
				unsigned long *temp)
{
	int ret;
	/* Panic zone */
	ret = exynos_get_trip_temp(thermal, GET_TRIP(PANIC_ZONE), temp);
	return ret;
}

/* Bind callback functions for thermal zone */
static int exynos_bind(struct thermal_zone_device *thermal,
			struct thermal_cooling_device *cdev)
{
	int ret = 0, i;

	/* find the cooling device registered*/
	for (i = 0; i < th_zone->cool_dev_size; i++)
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
		if (cdev == th_zone->cool_dev[i] || cdev == th_zone->cool_dev_kfc[i])
#else
		if (cdev == th_zone->cool_dev[i])
#endif
			break;

	/*No matching cooling device*/
	if (i == th_zone->cool_dev_size)
		return 0;

	switch (GET_ZONE(i)) {
	case MONITOR_ZONE:
	case WARN_ZONE:
	case PANIC_ZONE:
		if (thermal_zone_bind_cooling_device(thermal, i, cdev)) {
			pr_err("error binding cooling dev inst 0\n");
			ret = -EINVAL;
		}
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

/* Unbind callback functions for thermal zone */
static int exynos_unbind(struct thermal_zone_device *thermal,
			struct thermal_cooling_device *cdev)
{
	int ret = 0, i;

	/* find the cooling device registered*/
	for (i = 0; i < th_zone->cool_dev_size; i++)
		if (cdev == th_zone->cool_dev[i])
			break;

	/*No matching cooling device*/
	if (i == th_zone->cool_dev_size)
		return 0;

	switch (GET_ZONE(i)) {
	case MONITOR_ZONE:
	case WARN_ZONE:
	case PANIC_ZONE:
		if (thermal_zone_unbind_cooling_device(thermal, i, cdev)) {
			pr_err("error unbinding cooling dev\n");
			ret = -EINVAL;
		}
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

/* Get temperature callback functions for thermal zone */
static int exynos_get_temp(struct thermal_zone_device *thermal,
			unsigned long *temp)
{
	void *data;

	if (!th_zone->sensor_conf) {
		pr_info("Temperature sensor not initialised\n");
		return -EINVAL;
	}
	data = th_zone->sensor_conf->private_data;
	*temp = th_zone->sensor_conf->read_temperature(data);
	/* convert the temperature into millicelsius */
	*temp = *temp * MCELSIUS;
	return 0;
}

static int exynos_notify(struct thermal_zone_device *dev,
			int count, enum thermal_trip_type type)
{
	char tmustate_string[20];
	char *envp[2];

	if (type == THERMAL_TRIP_CRITICAL) {
		snprintf(tmustate_string, sizeof(tmustate_string), "TMUSTATE=%d",
							THERMAL_TRIP_CRITICAL);
		envp[0] = tmustate_string;
		envp[1] = NULL;

		pr_crit("Try S/W tripping, send uevent %s\n", envp[0]);
		return kobject_uevent_env(&dev->device.kobj, KOBJ_CHANGE, envp);
	}

	return 0;
}

static int exynos_get_throttling_count(struct thermal_zone_device *thermal,
		enum thermal_trip_type type)
{
	struct exynos_tmu_platform_data *pdata;
	struct exynos_tmu_data *data;
	unsigned int throttling_count;

	if (!th_zone->sensor_conf->private_data)
		return -ENODEV;

	data = th_zone->sensor_conf->private_data;
	if (!data)
		return -ENODEV;

	pdata = data->pdata;
	if (!pdata)
		return -ENODEV;

	switch (type) {
	case THERMAL_TRIP_ACTIVE:
		throttling_count = pdata->size[THERMAL_TRIP_ACTIVE];
		break;
	case THERMAL_TRIP_PASSIVE:
		throttling_count = pdata->size[THERMAL_TRIP_PASSIVE];
		break;
	default:
		return -EINVAL;
	}
	return throttling_count;
}

#ifdef CONFIG_SOC_EXYNOS5260
static int exynos_set_polling_delay(struct thermal_zone_device *thermal)
{
	unsigned int i;

	if (!th_zone || !th_zone->sensor_conf)
		return -EINVAL;
	if (!th_zone->therm_dev)
		return -EINVAL;

	for (i = 0; i < th_zone->sensor_conf->trip_data.trip_count; i++) {
		if (th_zone->therm_dev->last_temperature <
			th_zone->sensor_conf->trip_data.trip_val[i] * MCELSIUS)
			break;
	}

	if (th_zone->mode == THERMAL_DEVICE_ENABLED) {
		if (i > 0)
			th_zone->therm_dev->polling_delay = ACTIVE_INTERVAL;
		else
			th_zone->therm_dev->polling_delay = IDLE_INTERVAL;
	}
	return 0;
}
#endif

static int exynos_get_num_uncooled_cluster(void)
{
	return NUM_UNCOOLED_CLUSTER;
}

/* Operation callback functions for thermal zone */
static struct thermal_zone_device_ops const exynos_dev_ops = {
	.bind = exynos_bind,
	.unbind = exynos_unbind,
	.get_temp = exynos_get_temp,
	.get_mode = exynos_get_mode,
	.set_mode = exynos_set_mode,
	.get_trip_type = exynos_get_trip_type,
	.get_trip_temp = exynos_get_trip_temp,
	.set_trip_temp = exynos_set_trip_temp,
	.get_trip_temp_level = exynos_get_trip_temp_level,
	.set_trip_temp_level = exynos_set_trip_temp_level,
	.get_trip_freq = exynos_get_trip_freq,
	.set_trip_freq = exynos_set_trip_freq,
	.get_crit_temp = exynos_get_crit_temp,
	.get_throttling_count = exynos_get_throttling_count,
	.notify = exynos_notify,
#ifdef CONFIG_SOC_EXYNOS5260
	.set_polling_delay = exynos_set_polling_delay,
#endif
	.get_num_uncooled_cluster = exynos_get_num_uncooled_cluster,
};

/* Register with the in-kernel thermal management */
static int exynos_register_thermal(struct thermal_sensor_conf *sensor_conf)
{
	int ret, count, tab_size, pos = 0;
	struct freq_clip_table *tab_ptr, *clip_data;
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	struct freq_clip_table *tab_ptr_kfc;
#endif

	if (!sensor_conf || !sensor_conf->read_temperature) {
		pr_err("Temperature sensor not initialised\n");
		return -EINVAL;
	}

	th_zone = kzalloc(sizeof(struct exynos_thermal_zone), GFP_KERNEL);
	if (!th_zone)
		return -ENOMEM;

	th_zone->sensor_conf = sensor_conf;

	tab_ptr = (struct freq_clip_table *)sensor_conf->cooling_data.freq_data;
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	tab_ptr_kfc = (struct freq_clip_table *)sensor_conf->cooling_data_kfc.freq_data;
#endif

	/* Register the cpufreq cooling device */
	for (count = 0; count < EXYNOS_ZONE_COUNT; count++) {
		tab_size = sensor_conf->cooling_data.size[count];
		if (tab_size == 0)
			continue;

		clip_data = (struct freq_clip_table *)&(tab_ptr[pos]);

#ifdef CONFIG_CPU_THERMAL
		th_zone->cool_dev[count] = cpufreq_cooling_register(
						clip_data, tab_size);
#endif

#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
		clip_data = (struct freq_clip_table *)&(tab_ptr_kfc[pos]);

#ifdef CONFIG_CPU_THERMAL
		th_zone->cool_dev_kfc[count] = cpufreq_cooling_register(
						clip_data, tab_size);
#endif
#endif
		pos += tab_size;

		if (IS_ERR(th_zone->cool_dev[count])) {
			pr_err("Failed to register cpufreq cooling device\n");
			ret = -EINVAL;
			th_zone->cool_dev_size = count;
			goto err_unregister;
		}
	}
	th_zone->cool_dev_size = count;

	th_zone->therm_dev = thermal_zone_device_register(sensor_conf->name,
			EXYNOS_ZONE_COUNT, 7, NULL, &exynos_dev_ops, 1, 1, PASSIVE_INTERVAL,
			IDLE_INTERVAL);

	if (IS_ERR(th_zone->therm_dev)) {
		pr_err("Failed to register thermal zone device\n");
		ret = -EINVAL;
		goto err_unregister;
	}
	th_zone->mode = THERMAL_DEVICE_ENABLED;

	pr_info("Exynos: Kernel Thermal management registered\n");

	return 0;

err_unregister:
	exynos_unregister_thermal();
	return ret;
}

/* Un-Register with the in-kernel thermal management */
static void exynos_unregister_thermal(void)
{
#ifdef CONFIG_CPU_THERMAL
	int i;

	for (i = 0; i < th_zone->cool_dev_size; i++) {
		if (th_zone && th_zone->cool_dev[i])
			cpufreq_cooling_unregister(th_zone->cool_dev[i]);
	}
#endif

	if (th_zone && th_zone->therm_dev)
		thermal_zone_device_unregister(th_zone->therm_dev);

	kfree(th_zone);

	pr_info("Exynos: Kernel Thermal management unregistered\n");
}

int exynos_tmu_add_notifier(struct notifier_block *n)
{
	return blocking_notifier_chain_register(&exynos_tmu_notifier, n);
}

void exynos_tmu_call_notifier(enum tmu_noti_state_t cur_state)
{
	if (is_suspending)
		cur_state = TMU_COLD;

	if (cur_state != tmu_old_state) {
		if ((cur_state == TMU_COLD) ||
				((cur_state == TMU_NORMAL) && (tmu_old_state == TMU_COLD)))
			blocking_notifier_call_chain(&exynos_tmu_notifier, TMU_COLD, &cur_state);
		else
			blocking_notifier_call_chain(&exynos_tmu_notifier, cur_state, &tmu_old_state);

		pr_info("tmu temperature state %d to %d\n", tmu_old_state, cur_state);
		tmu_old_state = cur_state;
	}
}

int exynos_gpu_add_notifier(struct notifier_block *n)
{
	return blocking_notifier_chain_register(&exynos_gpu_notifier, n);
}

void exynos_gpu_call_notifier(enum gpu_noti_state_t cur_state)
{
	if (is_suspending)
		cur_state = GPU_COLD;

	if (cur_state != gpu_old_state) {
		pr_info("gpu temperature state %d to %d\n", gpu_old_state, cur_state);
		blocking_notifier_call_chain(&exynos_gpu_notifier, cur_state, &cur_state);
		gpu_old_state = cur_state;
	}
}

/*
 * TMU treats temperature as a mapped temperature code.
 * The temperature is converted differently depending on the calibration type.
 */
static int temp_to_code(struct exynos_tmu_data *data, u8 temp, int id)
{
	struct exynos_tmu_platform_data *pdata = data->pdata;
	int temp_code;
	int fuse_id;

	if (data->soc == SOC_ARCH_EXYNOS4)
		/* temp should range between 25 and 125 */
		if (temp < 25 || temp > 125) {
			temp_code = -EINVAL;
			goto out;
		}

	if (soc_is_exynos5420()) {
		switch (id) {
		case 0:
			fuse_id = 0;
			break;
		case 1:
			fuse_id = 1;
			break;
		case 2:
			fuse_id = 3;
			break;
		case 3:
			fuse_id = 4;
			break;
		case 4:
			fuse_id = 2;
			break;
		default:
			pr_err("unknown sensor id on Exynos5420\n");
			BUG_ON(1);
			break;
		}
	} else {
		fuse_id = id;
	}

	switch (pdata->cal_type) {
	case TYPE_TWO_POINT_TRIMMING:
		temp_code = (temp - 25) *
		    (data->temp_error2[fuse_id] - data->temp_error1[fuse_id]) /
		    (85 - 25) + data->temp_error1[fuse_id];
		break;
	case TYPE_ONE_POINT_TRIMMING:
		temp_code = temp + data->temp_error1[fuse_id] - 25;
		break;
	default:
		temp_code = temp + EXYNOS_TMU_DEF_CODE_TO_TEMP_OFFSET;
		break;
	}
out:
	return temp_code;
}

/*
 * Calculate a temperature value from a temperature code.
 * The unit of the temperature is degree Celsius.
 */
static int code_to_temp(struct exynos_tmu_data *data, u8 temp_code, int id)
{
	struct exynos_tmu_platform_data *pdata = data->pdata;
	int min, max, temp;
	int fuse_id;

	if (data->soc == SOC_ARCH_EXYNOS4) {
		min = 75;
		max = 175;
	} else {
		min = 31;
		max = 146;
	}

	if (soc_is_exynos5420()) {
		switch (id) {
		case 0:
			fuse_id = 0;
			break;
		case 1:
			fuse_id = 1;
			break;
		case 2:
			fuse_id = 3;
			break;
		case 3:
			fuse_id = 4;
			break;
		case 4:
			fuse_id = 2;
			break;
		default:
			pr_err("unknown sensor id on Exynos5420\n");
			BUG_ON(1);
			break;
		}
	} else {
		fuse_id = id;
	}

	/* temp_code should range between min and max */
	if (temp_code < min || temp_code > max) {
		temp = -ENODATA;
		goto out;
	}

	switch (pdata->cal_type) {
	case TYPE_TWO_POINT_TRIMMING:
		temp = (temp_code - data->temp_error1[fuse_id]) * (85 - 25) /
		    (data->temp_error2[fuse_id] - data->temp_error1[fuse_id]) + 25;
		break;
	case TYPE_ONE_POINT_TRIMMING:
		temp = temp_code - data->temp_error1[fuse_id] + 25;
		break;
	default:
		temp = temp_code - EXYNOS_TMU_DEF_CODE_TO_TEMP_OFFSET;
		break;
	}
out:
	return temp;
}

static int exynos_tmu_initialize(struct platform_device *pdev, int id)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct exynos_tmu_platform_data *pdata = data->pdata;
	unsigned int status, rising_threshold, falling_threshold;
	int ret = 0, threshold_code;

	mutex_lock(&data->lock);
	clk_enable(data->clk);

	status = readb(data->base[id] + EXYNOS_TMU_REG_STATUS);
	if (!status) {
		ret = -EBUSY;
		goto out;
	}

	if (data->soc == SOC_ARCH_EXYNOS4) {
		/* Write temperature code for threshold */
		threshold_code = temp_to_code(data, pdata->threshold, id);
		if (threshold_code < 0) {
			ret = threshold_code;
			goto out;
		}
		writeb(threshold_code,
			data->base[id] + EXYNOS4_TMU_REG_THRESHOLD_TEMP);
		writeb(pdata->trigger_levels[0],
			data->base[id] + EXYNOS4_TMU_REG_TRIG_LEVEL0);
		writeb(pdata->trigger_levels[1],
			data->base[id] + EXYNOS4_TMU_REG_TRIG_LEVEL1);
		writeb(pdata->trigger_levels[2],
			data->base[id] + EXYNOS4_TMU_REG_TRIG_LEVEL2);
		writeb(pdata->trigger_levels[3],
			data->base[id] + EXYNOS4_TMU_REG_TRIG_LEVEL3);
		writel(EXYNOS4_TMU_INTCLEAR_VAL,
			data->base[id] + EXYNOS_TMU_REG_INTCLEAR);
	} else if (data->soc == SOC_ARCH_EXYNOS3) {
		/* Write temperature code for threshold */
		threshold_code = temp_to_code(data, pdata->trigger_levels[0], id);
		if (threshold_code < 0) {
			ret = threshold_code;
			goto out;
		}
		rising_threshold = threshold_code;
		falling_threshold = threshold_code - GAP_WITH_RISE;
		threshold_code = temp_to_code(data, pdata->trigger_levels[1], id);
		if (threshold_code < 0) {
			ret = threshold_code;
			goto out;
		}
		rising_threshold |= (threshold_code << THRESH_LEVE1_SHIFT);
		falling_threshold |= ((threshold_code - GAP_WITH_RISE) << THRESH_LEVE1_SHIFT);
		threshold_code = temp_to_code(data, pdata->trigger_levels[2], id);
		if (threshold_code < 0) {
			ret = threshold_code;
			goto out;
		}
		rising_threshold |= (threshold_code << THRESH_LEVE2_SHIFT);
		falling_threshold |= ((threshold_code - GAP_WITH_RISE) << THRESH_LEVE2_SHIFT);
		threshold_code = temp_to_code(data, pdata->trigger_levels[3], id);
		if (threshold_code < 0) {
			ret = threshold_code;
			goto out;
		}
		rising_threshold |= (threshold_code << THRESH_LEVE3_SHIFT);

		writel(rising_threshold, data->base[id] + EXYNOS3_THD_TEMP_RISE);
		writel(falling_threshold, data->base[id] + EXYNOS3_THD_TEMP_FALL);
		writel(EXYNOS3_TMU_CLEAR_RISE_INT|EXYNOS3_TMU_CLEAR_FALL_INT,
				data->base[id] + EXYNOS_TMU_REG_INTCLEAR);
	} else if (data->soc == SOC_ARCH_EXYNOS5) {
		/* Write temperature code for threshold */
		threshold_code = temp_to_code(data, pdata->trigger_levels[0], id);
		if (threshold_code < 0) {
			ret = threshold_code;
			goto out;
		}
		rising_threshold = threshold_code;
		falling_threshold = threshold_code - GAP_WITH_RISE;
		threshold_code = temp_to_code(data, pdata->trigger_levels[1], id);
		if (threshold_code < 0) {
			ret = threshold_code;
			goto out;
		}
		rising_threshold |= (threshold_code << THRESH_LEVE1_SHIFT);
		falling_threshold |= ((threshold_code - GAP_WITH_RISE) << THRESH_LEVE1_SHIFT);
		threshold_code = temp_to_code(data, pdata->trigger_levels[2], id);
		if (threshold_code < 0) {
			ret = threshold_code;
			goto out;
		}
		rising_threshold |= (threshold_code << THRESH_LEVE2_SHIFT);
		falling_threshold |= ((threshold_code - GAP_WITH_RISE) << THRESH_LEVE2_SHIFT);
		threshold_code = temp_to_code(data, pdata->trigger_levels[3], id);
		if (threshold_code < 0) {
			ret = threshold_code;
			goto out;
		}
		rising_threshold |= (threshold_code << THRESH_LEVE3_SHIFT);

		writel(rising_threshold, data->base[id] + EXYNOS5_THD_TEMP_RISE);
		writel(falling_threshold, data->base[id] + EXYNOS5_THD_TEMP_FALL);
		writel(EXYNOS5_TMU_CLEAR_RISE_INT|EXYNOS5_TMU_CLEAR_FALL_INT,
				data->base[id] + EXYNOS_TMU_REG_INTCLEAR);
	}
out:
	clk_disable(data->clk);
	mutex_unlock(&data->lock);

	return ret;
}

static void exynos_tmu_get_efuse(struct platform_device *pdev, int id)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct exynos_tmu_platform_data *pdata = data->pdata;
	unsigned int trim_info, trim_info_ctl;

	mutex_lock(&data->lock);
	clk_enable(data->clk);

	/* Reload trimming info before get the value */
	trim_info_ctl = readl(data->base[id] + EXYNOS_TMU_TRIMINFO_CON);
	trim_info_ctl |= EXYNOS5_TRIMINFO_RELOAD;
	writel(trim_info_ctl, data->base[id] + EXYNOS_TMU_TRIMINFO_CON);
	__raw_writel(EXYNOS5_TRIMINFO_RELOAD,
			data->base[id] + EXYNOS_TMU_TRIMINFO_CON);
	/* Save trimming info in order to perform calibration */
	trim_info = readl(data->base[id] + EXYNOS_TMU_REG_TRIMINFO);
	data->temp_error1[id] = trim_info & EXYNOS_TMU_TRIM_TEMP_MASK;
	data->temp_error2[id] = ((trim_info >> 8) & EXYNOS_TMU_TRIM_TEMP_MASK);
	if ((EFUSE_MIN_VALUE > data->temp_error1[id]) ||
			(data->temp_error1[id] > EFUSE_MAX_VALUE))
		data->temp_error1[id] = pdata->efuse_value;

	clk_disable(data->clk);
	mutex_unlock(&data->lock);
}

static void exynos_tmu_control(struct platform_device *pdev, int id, bool on)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct exynos_tmu_platform_data *pdata = data->pdata;
	unsigned int con = 0, interrupt_en = 0, interrupt_rise2_en = 0;
#ifdef CONFIG_SOC_EXYNOS5260
	unsigned char gain;

	if (id == 3)
		gain = 0x6;
	else if (id == 4)
		gain = 0x9;
	else
		gain = pdata->gain;
#endif

	mutex_lock(&data->lock);
	clk_enable(data->clk);
#ifdef CONFIG_SOC_EXYNOS5260
	con = pdata->reference_voltage << EXYNOS_TMU_REF_VOLTAGE_SHIFT |
		gain << EXYNOS_TMU_GAIN_SHIFT;
#else
	con = pdata->reference_voltage << EXYNOS_TMU_REF_VOLTAGE_SHIFT |
		pdata->gain << EXYNOS_TMU_GAIN_SHIFT;
#endif

	con |= pdata->noise_cancel_mode << EXYNOS_TMU_TRIP_MODE_SHIFT;
	con |= (EXYNOS_MUX_ADDR_VALUE << EXYNOS_MUX_ADDR_SHIFT);

	if (on) {
		con |= (EXYNOS_TMU_CORE_ON | EXYNOS_THERM_TRIP_EN);
		interrupt_en =
			pdata->trigger_level3_en << FALL_LEVEL3_SHIFT |
			pdata->trigger_level2_en << FALL_LEVEL2_SHIFT |
			pdata->trigger_level1_en << FALL_LEVEL1_SHIFT |
			pdata->trigger_level0_en << FALL_LEVEL0_SHIFT |
			pdata->trigger_level3_en << RISE_LEVEL3_SHIFT |
			pdata->trigger_level2_en << RISE_LEVEL2_SHIFT |
			pdata->trigger_level1_en << RISE_LEVEL1_SHIFT |
			pdata->trigger_level0_en;
		interrupt_rise2_en = 0x00000100;
	} else {
		con |= EXYNOS_TMU_CORE_OFF;
		interrupt_en = 0; /* Disable all interrupts */
		interrupt_rise2_en = 0;
	}
	con |= EXYNOS_THERM_TRIP_EN;
#ifdef CONFIG_SOC_EXYNOS5260
	writel(0xffffffff, data->base[id] + EXYNOS5_THD_TEMP_RISE);
#endif
	writel(interrupt_en, data->base[id] + EXYNOS_TMU_REG_INTEN);
#ifdef CONFIG_SOC_EXYNOS5260
	writel(interrupt_rise2_en, data->base[id] + EXYNOS_TMU_REG_RISE2_INTEN);
#endif
	writel(con, data->base[id] + EXYNOS_TMU_REG_CONTROL);
#ifdef CONFIG_SOC_EXYNOS5260
	writel(0, data->base[id] + EXYNOS5_TMU_REG_CONTROL1);
#endif
#ifdef CONFIG_SOC_EXYNOS5260
	writel(0x11110000, data->base[id] + EXYNOS_TMU_REG_INTCLEAR);
#endif
	clk_disable(data->clk);
	mutex_unlock(&data->lock);
#ifdef CONFIG_SOC_EXYNOS5260
	exynos_tmu_initialize(pdev, id);
#endif
}

static int exynos_tmu_read(struct exynos_tmu_data *data)
{
	u8 temp_code;
	int temp, i, max = INT_MIN, min = INT_MAX, gpu_temp = 0;

#ifdef CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ
	enum tmu_noti_state_t cur_state;
#endif
	int alltemp[EXYNOS_TMU_COUNT] = {0,};

	if (!th_zone || !th_zone->therm_dev)
		return -EPERM;

	mutex_lock(&data->lock);
	clk_enable(data->clk);

	for (i = 0; i < EXYNOS_TMU_COUNT; i++) {
		temp_code = readb(data->base[i] + EXYNOS_TMU_REG_CURRENT_TEMP);
		temp = code_to_temp(data, temp_code, i);
		if (temp < 0)
			temp = 0;
		alltemp[i] = temp;

		if (temp > max)
			max = temp;
		if (temp < min)
			min = temp;
		if (soc_is_exynos5420() && i == EXYNOS_GPU_NUMBER)
			gpu_temp = temp;
	}
#ifdef CONFIG_SOC_EXYNOS5260
	if (alltemp[EXYNOS_GPU0_NUMBER] > alltemp[EXYNOS_GPU1_NUMBER])
		gpu_temp = alltemp[EXYNOS_GPU0_NUMBER];
	else
		gpu_temp = alltemp[EXYNOS_GPU1_NUMBER];
#endif

	clk_disable(data->clk);
	mutex_unlock(&data->lock);

#ifdef CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ
	if (min <= COLD_TEMP)
		cur_state = TMU_COLD;

	if (max >= HOT_110)
		cur_state = TMU_110;
	else if (max > HOT_95 && max < HOT_110)
		cur_state = TMU_109;
	else if (max <= HOT_95)
		cur_state = TMU_95;

	if (cur_state >= HOT_95)
		pr_info("[TMU] POLLING: temp=%d, cpu=%d, int=%d, mif=%d, hot_event(for aref)=%d\n",
			max, g_cpufreq, g_intfreq, g_miffreq, cur_state);
	g_count++;

	/* to probe thermal run away caused by fimc-is */
	if ((max >= 112 && tmu_old_state == TMU_110)
			&& g_cam_err_count && g_intfreq == 800000) {
		cur_state = TMU_111;
	}
	exynos_tmu_call_notifier(cur_state);
#endif

	if (soc_is_exynos5420()) {
		/* check temperature state */
		exynos_check_tmu_noti_state(min, max);
		exynos_check_gpu_noti_state(gpu_temp);

#ifdef CONFIG_ARM_EXYNOS5420_BUS_DEVFREQ
		if (!check_mif_probed())
			goto out;
#endif
		exynos_check_mif_noti_state(max);
	}

	if (soc_is_exynos5260()) {
		exynos_check_tmu_noti_state(min, max);
		exynos_check_gpu_noti_state(gpu_temp);
		exynos_check_mif_noti_state(max);
	}

#ifdef CONFIG_ARM_EXYNOS5420_BUS_DEVFREQ
out:
#endif
	th_zone->therm_dev->last_temperature = max * MCELSIUS;
	DTM_DBG("[TMU] TMU0 = %d, TMU1 = %d, TMU2 = %d, TMU3 = %d, TMU4 = %d  -------  CPU : %d  ,  GPU : %d\n",
			alltemp[0], alltemp[1], alltemp[2], alltemp[3], alltemp[4], max, gpu_temp);

	return max;
}

static void exynos_check_mif_noti_state(int temp)
{
	enum mif_noti_state_t cur_state;

	/* check current temperature state */
	if (temp <= MEM_TH_TEMP1)
		cur_state = MEM_TH_LV1;
	else if (temp > MEM_TH_TEMP1 && temp < MEM_TH_TEMP2)
		cur_state = MEM_TH_LV2;
	else if (temp >= MEM_TH_TEMP2)
		cur_state = MEM_TH_LV3;

	if (cur_state != mif_old_state) {
		pr_info("mif temperature state %d to %d\n", mif_old_state, cur_state);
		blocking_notifier_call_chain(&exynos_tmu_notifier, cur_state, &mif_old_state);
		mif_old_state = cur_state;
	}
}

static void exynos_check_tmu_noti_state(int min_temp, int max_temp)
{
	enum tmu_noti_state_t cur_state;

	/* check current temperature state */
	if (max_temp > HOT_CRITICAL_TEMP)
		cur_state = TMU_CRITICAL;
	else if (max_temp > HOT_NORMAL_TEMP && max_temp <= HOT_CRITICAL_TEMP)
		cur_state = TMU_HOT;
	else if (max_temp > COLD_TEMP && max_temp <= HOT_NORMAL_TEMP)
		cur_state = TMU_NORMAL;
	else
		cur_state = TMU_COLD;

	if (min_temp <= COLD_TEMP)
		cur_state = TMU_COLD;

	exynos_tmu_call_notifier(cur_state);
}

static void exynos_check_gpu_noti_state(int temp)
{
	enum gpu_noti_state_t cur_state;

	/* check current temperature state */
	if (temp >= GPU_TH_TEMP5)
		cur_state = GPU_TRIPPING;
	else if (temp >= GPU_TH_TEMP4 && temp < GPU_TH_TEMP5)
		cur_state = GPU_THROTTLING4;
	else if (temp >= GPU_TH_TEMP3 && temp < GPU_TH_TEMP4)
		cur_state = GPU_THROTTLING3;
	else if (temp >= GPU_TH_TEMP2 && temp < GPU_TH_TEMP3)
		cur_state = GPU_THROTTLING2;
	else if (temp >= GPU_TH_TEMP1 && temp < GPU_TH_TEMP2)
		cur_state = GPU_THROTTLING1;
	else if (temp > COLD_TEMP && temp < GPU_TH_TEMP1)
		cur_state = GPU_NORMAL;
	else
		cur_state = GPU_COLD;

	exynos_gpu_call_notifier(cur_state);
}

static void exynos_tmu_work(struct work_struct *work)
{
	struct exynos_tmu_data *data = container_of(work,
			struct exynos_tmu_data, irq_work);
	int i;

	exynos_report_trigger();

	mutex_lock(&data->lock);
	clk_enable(data->clk);

	if (data->soc == SOC_ARCH_EXYNOS5) {
		for (i = 0; i < EXYNOS_TMU_COUNT; i++) {
			writel(EXYNOS5_TMU_CLEAR_RISE_INT|EXYNOS5_TMU_CLEAR_FALL_INT,
					data->base[i] + EXYNOS_TMU_REG_INTCLEAR);
		}
	} else {
		writel(EXYNOS4_TMU_INTCLEAR_VAL,
				data->base + EXYNOS_TMU_REG_INTCLEAR);
	}

	clk_disable(data->clk);
	mutex_unlock(&data->lock);

	for (i = 0; i < EXYNOS_TMU_COUNT; i++)
		enable_irq(data->irq[i]);
}

static irqreturn_t exynos_tmu_irq(int irq, void *id)
{
	struct exynos_tmu_data *data = id;
	int i;

	for (i = 0; i < EXYNOS_TMU_COUNT; i++)
		disable_irq_nosync(data->irq[i]);
	schedule_work(&data->irq_work);

	return IRQ_HANDLED;
}

static struct thermal_sensor_conf exynos_sensor_conf = {
	.name			= "exynos-therm",
	.read_temperature	= (int (*)(void *))exynos_tmu_read,
};

static int exynos_pm_notifier(struct notifier_block *notifier,
			unsigned long pm_event, void *v)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		is_suspending = true;
		exynos_tmu_call_notifier(TMU_COLD);
		exynos_tmu_call_notifier(tmu_old_state);
		exynos_check_mif_noti_state(MEM_TH_TEMP2 - 1);
		exynos_gpu_call_notifier(GPU_COLD);
		break;
	case PM_POST_SUSPEND:
		is_suspending = false;
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_pm_nb = {
	.notifier_call = exynos_pm_notifier,
};

static struct platform_device_id exynos_tmu_driver_ids[] = {
	{
		.name		= "exynos3-tmu",
		.driver_data    = (kernel_ulong_t)NULL,
	},
	{
		.name		= "exynos4-tmu",
		.driver_data    = (kernel_ulong_t)EXYNOS4_TMU_DRV_DATA,
	},
	{
		.name		= "exynos5-tmu",
		.driver_data    = (kernel_ulong_t)EXYNOS5_TMU_DRV_DATA,
	},
	{ },
};
MODULE_DEVICE_TABLE(platform, exynos_tmu_driver_ids);

static inline struct  exynos_tmu_platform_data *exynos_get_driver_data(
		struct platform_device *pdev)
{
	return (struct exynos_tmu_platform_data *)
		platform_get_device_id(pdev)->driver_data;
}

/* sysfs interface : /sys/devices/platform/exynos5-tmu/temp */
static ssize_t
exynos_thermal_sensor_temp(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct exynos_tmu_data *data = th_zone->sensor_conf->private_data;
	u8 temp_code;
	unsigned long temp[EXYNOS_TMU_COUNT] = {0,};
	int i, len = 0;

	mutex_lock(&data->lock);
	clk_enable(data->clk);

	for (i = 0; i < EXYNOS_TMU_COUNT; i++) {
		temp_code = readb(data->base[i] + EXYNOS_TMU_REG_CURRENT_TEMP);
		if (temp_code == 0xff)
			continue;
		temp[i] = code_to_temp(data, temp_code, i) * MCELSIUS;
	}

	clk_disable(data->clk);
	mutex_unlock(&data->lock);

	for (i = 0; i < EXYNOS_TMU_COUNT; i++)
		len += snprintf(&buf[len], PAGE_SIZE, "sensor%d : %ld\n", i, temp[i]);

	return len;
}

static DEVICE_ATTR(temp, S_IRUSR | S_IRGRP, exynos_thermal_sensor_temp, NULL);

#ifdef CONFIG_SEC_PM
/* sysfs interface : /sys/devices/platform/exynos5-tmu/curr_temp */
static ssize_t
exynos_thermal_curr_temp(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct exynos_tmu_data *data = th_zone->sensor_conf->private_data;
	unsigned long temp[EXYNOS_TMU_COUNT];
	int i, len = 0;

	if (!(soc_is_exynos5410() || soc_is_exynos5420() || soc_is_exynos5260()))
		return -EPERM;

	if (EXYNOS_TMU_COUNT < 4)
		return -EPERM;

	mutex_lock(&data->lock);
	clk_enable(data->clk);

	for (i = 0; i < EXYNOS_TMU_COUNT; i++) {
		u8 temp_code = readb(data->base[i] + EXYNOS_TMU_REG_CURRENT_TEMP);
		if (temp_code == 0xff)
			temp[i] = 0;
		else
			temp[i] = code_to_temp(data, temp_code, i) * 10;
	}

	clk_disable(data->clk);
	mutex_unlock(&data->lock);

	len += sprintf(&buf[len], "%ld,", temp[0]);
	len += sprintf(&buf[len], "%ld,", temp[1]);
	len += sprintf(&buf[len], "%ld,", temp[2]);
	len += sprintf(&buf[len], "%ld,", temp[3]);
	len += sprintf(&buf[len], "%ld\n", temp[4]);

	return len;
}

static DEVICE_ATTR(curr_temp, S_IRUGO, exynos_thermal_curr_temp, NULL);
#endif


static struct attribute *exynos_thermal_sensor_attributes[] = {
	&dev_attr_temp.attr,
#ifdef CONFIG_SEC_PM
	&dev_attr_curr_temp.attr,
#endif

	NULL
};

static const struct attribute_group exynos_thermal_sensor_attr_group = {
	.attrs = exynos_thermal_sensor_attributes,
};

static void exynos_tmu_regdump(struct platform_device *pdev, int id)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	unsigned int reg_data;

	mutex_lock(&data->lock);
	clk_enable(data->clk);

	DTM_DBG("====================[TMU register dump]=====================\n");
	reg_data = readl(data->base[id] + EXYNOS_TMU_REG_TRIMINFO);
	DTM_DBG("TRIMINFO[%d] = 0x%x\n", id, reg_data);
	reg_data = readl(data->base[id] + EXYNOS_TMU_REG_CONTROL);
	DTM_DBG("CONTROL[%d] = 0x%x\n", id, reg_data);
	reg_data = readl(data->base[id] + EXYNOS_TMU_REG_STATUS);
	DTM_DBG("STATUS[%d] = 0x%x\n", id, reg_data);
	reg_data = readl(data->base[id] + EXYNOS_TMU_REG_CURRENT_TEMP);
	DTM_DBG("CURRENT_TEMP[%d] = 0x%x\n", id, reg_data);
	if (data->soc == SOC_ARCH_EXYNOS4) {
		reg_data = readl(data->base[id] + EXYNOS4_TMU_REG_TRIG_LEVEL0);
		DTM_DBG("THRESHOLD0[%d] = 0x%x\n", id, reg_data);
		reg_data = readl(data->base[id] + EXYNOS4_TMU_REG_TRIG_LEVEL1);
		DTM_DBG("THRESHOLD1[%d] = 0x%x\n", id, reg_data);
		reg_data = readl(data->base[id] + EXYNOS4_TMU_REG_TRIG_LEVEL2);
		DTM_DBG("THRESHOLD2[%d] = 0x%x\n", id, reg_data);
		reg_data = readl(data->base[id] + EXYNOS4_TMU_REG_TRIG_LEVEL3);
		DTM_DBG("THRESHOLD3[%d] = 0x%x\n", id, reg_data);
	} else if (data->soc == SOC_ARCH_EXYNOS5) {
		reg_data = readl(data->base[id] + EXYNOS5_THD_TEMP_RISE);
		DTM_DBG("THRESHOLD_TEMP_RISE[%d] = 0x%x\n", id, reg_data);
		reg_data = readl(data->base[id] + EXYNOS5_THD_TEMP_FALL);
		DTM_DBG("THRESHOLD_TEMP_FALL[%d] = 0x%x\n", id, reg_data);
	}
	reg_data = readl(data->base[id] + EXYNOS_TMU_REG_INTEN);
	DTM_DBG("INTEN[%d] = 0x%x\n", id, reg_data);
#ifdef CONFIG_SOC_EXYNOS5260
	reg_data = readl(data->base[id] + EXYNOS_TMU_REG_RISE2_INTEN);
	DTM_DBG("INTEN_RISE2[%d] = 0x%x\n", id, reg_data);
#endif
	reg_data = readl(data->base[id] + EXYNOS_TMU_REG_INTSTAT);
	DTM_DBG("INTSTAT[%d] = 0x%x\n", id, reg_data);
	reg_data = readl(data->base[id] + EXYNOS_TMU_REG_INTCLEAR);
	DTM_DBG("INTCLEAR[%d] = 0x%x\n", id, reg_data);
	DTM_DBG("============================================================\n");

	clk_disable(data->clk);
	mutex_unlock(&data->lock);
}

#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
static int exynos5_tmu_cpufreq_notifier(struct notifier_block *notifier, unsigned long event, void *v)
{
	int ret = 0, i;
	struct exynos_tmu_data *data = (struct exynos_tmu_data *)platform_get_drvdata(exynos_tmu_pdev);

	switch(event) {
	case CPUFREQ_INIT_COMPLETE:
		ret = exynos_register_thermal(&exynos_sensor_conf);
		if (ret) {
			dev_err(&th_zone->exynos4_dev->dev, "Failed to register thermal interface\n");
			unregister_pm_notifier(&exynos_pm_nb);
			platform_set_drvdata(th_zone->exynos4_dev, NULL);
			clk_put(data->clk);
			for (i = 0; i < EXYNOS_TMU_COUNT; i++) {
				if (data->irq[i])
					free_irq(data->irq[i], data);
			}
			for (i = 0; i < EXYNOS_TMU_COUNT; i++) {
				if (data->base[i])
					iounmap(data->base[i]);
			}
			for (i = 0; i < EXYNOS_TMU_COUNT; i++) {
				if (data->mem[i])
					release_mem_region(data->mem[i]->start, resource_size(data->mem[i]));
			}
			kfree(data);
			exynos_cpufreq_init_unregister_notifier(&exynos_cpufreq_nb);
			return ret;
		}
		th_zone->exynos4_dev = exynos_tmu_pdev;
		ret = sysfs_create_group(&exynos_tmu_pdev->dev.kobj, &exynos_thermal_sensor_attr_group);
		if (ret)
			dev_err(&exynos_tmu_pdev->dev, "cannot create thermal sensor attributes\n");
		break;
	}
	return 0;
}
#endif

static int __devinit exynos_tmu_probe(struct platform_device *pdev)
{
	struct exynos_tmu_data *data;
	struct exynos_tmu_platform_data *pdata = pdev->dev.platform_data;
	int ret, i;

	is_suspending = false;

	if (!pdata)
		pdata = exynos_get_driver_data(pdev);

	if (!pdata) {
		dev_err(&pdev->dev, "No platform init data supplied.\n");
		return -ENODEV;
	}
	data = kzalloc(sizeof(struct exynos_tmu_data), GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "Failed to allocate driver structure\n");
		return -ENOMEM;
	}
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	exynos_cpufreq_init_register_notifier(&exynos_cpufreq_nb);
#endif
	INIT_WORK(&data->irq_work, exynos_tmu_work);

	for (i = 0; i < EXYNOS_TMU_COUNT; i++) {
		data->irq[i] = platform_get_irq(pdev, i);
		if (data->irq[i] < 0) {
			ret = data->irq[i];
			dev_err(&pdev->dev, "Failed to get platform irq\n");
			goto err_free;
		}

		data->mem[i] = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!data->mem[i]) {
			ret = -ENOENT;
			dev_err(&pdev->dev, "Failed to get platform resource\n");
			goto err_free;
		}

		data->mem[i] = request_mem_region(data->mem[i]->start,
				resource_size(data->mem[i]), pdev->name);
		if (!data->mem[i]) {
			ret = -ENODEV;
			dev_err(&pdev->dev, "Failed to request memory region\n");
			goto err_free;
		}

		data->base[i] = ioremap(data->mem[i]->start, resource_size(data->mem[i]));
		if (!data->base[i]) {
			ret = -ENODEV;
			dev_err(&pdev->dev, "Failed to ioremap memory\n");
			goto err_mem_region;
		}

		ret = request_irq(data->irq[i], exynos_tmu_irq,
				IRQF_TRIGGER_RISING, "exynos-tmu", data);
		if (ret) {
			dev_err(&pdev->dev, "Failed to request irq: %d\n", data->irq[i]);
			goto err_io_remap;
		}
	}

	data->clk = clk_get(NULL, pdata->clk_name);

	if (IS_ERR(data->clk)) {
		ret = PTR_ERR(data->clk);
		dev_err(&pdev->dev, "Failed to get clock\n");
		goto err_irq;
	}

	if (pdata->type == SOC_ARCH_EXYNOS5 ||
				pdata->type == SOC_ARCH_EXYNOS4||
				pdata->type == SOC_ARCH_EXYNOS3)
		data->soc = pdata->type;
	else {
		ret = -EINVAL;
		dev_err(&pdev->dev, "Platform not supported\n");
		goto err_clk;
	}

	data->pdata = pdata;
	platform_set_drvdata(pdev, data);
	mutex_init(&data->lock);

	for (i = 0; i < EXYNOS_TMU_COUNT; i++)
		exynos_tmu_get_efuse(pdev, i);

	/*TMU initialization*/
	for (i = 0; i < EXYNOS_TMU_COUNT; i++) {
		ret = exynos_tmu_initialize(pdev, i);
		if (ret) {
			dev_err(&pdev->dev, "Failed to initialize TMU[%d]\n", i);
			goto err_clk;
		}

		exynos_tmu_control(pdev, i, true);
		exynos_tmu_regdump(pdev, i);
	}

	/*Register the sensor with thermal management interface*/
	(&exynos_sensor_conf)->private_data = data;
	exynos_sensor_conf.trip_data.trip_count = pdata->trigger_level0_en +
		pdata->trigger_level1_en + pdata->trigger_level2_en + pdata->trigger_level3_en;

	for (i = 0; i < exynos_sensor_conf.trip_data.trip_count; i++)
		exynos_sensor_conf.trip_data.trip_val[i] =
			pdata->threshold + pdata->trigger_levels[i];
	exynos_sensor_conf.cooling_data.freq_clip_count = pdata->freq_tab_count;

	for (i = 0; i < pdata->freq_tab_count; i++) {
		exynos_sensor_conf.cooling_data.freq_data[i].freq_clip_max =
			pdata->freq_tab[i].freq_clip_max;
		exynos_sensor_conf.cooling_data.freq_data[i].temp_level =
			pdata->freq_tab[i].temp_level;
		if (pdata->freq_tab[i].mask_val)
			exynos_sensor_conf.cooling_data.freq_data[i].mask_val =
				pdata->freq_tab[i].mask_val;
		else
			exynos_sensor_conf.cooling_data.freq_data[i].mask_val =
				cpu_all_mask;

	}

	for (i = 0; i <= THERMAL_TRIP_CRITICAL; i++)
		exynos_sensor_conf.cooling_data.size[i] = pdata->size[i];

#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	exynos_sensor_conf.cooling_data_kfc.freq_clip_count = pdata->freq_tab_count;
	for (i = 0; i < pdata->freq_tab_count; i++) {
		exynos_sensor_conf.cooling_data_kfc.freq_data[i].freq_clip_max =
			pdata->freq_tab_kfc[i].freq_clip_max;
		exynos_sensor_conf.cooling_data_kfc.freq_data[i].temp_level =
			pdata->freq_tab_kfc[i].temp_level;
		if (pdata->freq_tab_kfc[i].mask_val)
			exynos_sensor_conf.cooling_data_kfc.freq_data[i].mask_val =
				pdata->freq_tab_kfc[i].mask_val;
		else
			exynos_sensor_conf.cooling_data_kfc.freq_data[i].mask_val =
				cpu_all_mask;
	}

	for (i = 0; i <= THERMAL_TRIP_CRITICAL; i++)
		exynos_sensor_conf.cooling_data_kfc.size[i] = pdata->size[i];

#endif

	register_pm_notifier(&exynos_pm_nb);

	exynos_tmu_pdev = pdev;

#ifndef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	ret = exynos_register_thermal(&exynos_sensor_conf);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register thermal interface\n");
		goto err_clk;
	}

	th_zone->exynos4_dev = pdev;
	ret = sysfs_create_group(&pdev->dev.kobj, &exynos_thermal_sensor_attr_group);
	if (ret)
		dev_err(&pdev->dev, "cannot create thermal sensor attributes\n");
#endif

	/* For low temperature compensation when boot time */
	exynos_tmu_call_notifier(TMU_COLD);
	exynos_gpu_call_notifier(GPU_COLD);

	return 0;
err_clk:
	platform_set_drvdata(pdev, NULL);
	clk_put(data->clk);
err_irq:
	for (i = 0; i < EXYNOS_TMU_COUNT; i++) {
		if (data->irq[i])
			free_irq(data->irq[i], data);
	}
err_io_remap:
	for (i = 0; i < EXYNOS_TMU_COUNT; i++) {
		if (data->base[i])
			iounmap(data->base[i]);
	}
err_mem_region:
	for (i = 0; i < EXYNOS_TMU_COUNT; i++) {
		if (data->mem[i])
			release_mem_region(data->mem[i]->start, resource_size(data->mem[i]));
	}
err_free:
	kfree(data);
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	exynos_cpufreq_init_unregister_notifier(&exynos_cpufreq_nb);
#endif

	return ret;
}

static int __devexit exynos_tmu_remove(struct platform_device *pdev)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	int i;

	exynos_unregister_thermal();

	unregister_pm_notifier(&exynos_pm_nb);

	for (i = 0; i < EXYNOS_TMU_COUNT; i++)
		exynos_tmu_control(pdev, i, false);

	platform_set_drvdata(pdev, NULL);
	clk_put(data->clk);

	for (i = 0; i < EXYNOS_TMU_COUNT; i++)
		free_irq(data->irq[i], data);
	for (i = 0; i < EXYNOS_TMU_COUNT; i++)
		iounmap(data->base[i]);
	for (i = 0; i < EXYNOS_TMU_COUNT; i++)
		release_mem_region(data->mem[i]->start, resource_size(data->mem[i]));

	kfree(data);

	return 0;
}

#ifdef CONFIG_PM
static int exynos_tmu_suspend(struct platform_device *pdev, pm_message_t state)
{
	int i;
	for (i = 0; i < EXYNOS_TMU_COUNT; i++)
		exynos_tmu_control(pdev, i, false);

	return 0;
}

static int exynos_tmu_resume(struct platform_device *pdev)
{
	int i;
	bool on = false;

	if (th_zone->mode == THERMAL_DEVICE_ENABLED)
		on = true;

	for (i = 0; i < EXYNOS_TMU_COUNT; i++) {
		exynos_tmu_initialize(pdev, i);
		exynos_tmu_control(pdev, i, on);
	}

	return 0;
}
#else
#define exynos_tmu_suspend NULL
#define exynos_tmu_resume NULL
#endif

static struct platform_driver exynos_tmu_driver = {
	.driver = {
		.name   = "exynos-tmu",
		.owner  = THIS_MODULE,
	},
	.probe = exynos_tmu_probe,
	.remove	= __devexit_p(exynos_tmu_remove),
	.suspend = exynos_tmu_suspend,
	.resume = exynos_tmu_resume,
	.id_table = exynos_tmu_driver_ids,
};

module_platform_driver(exynos_tmu_driver);

MODULE_DESCRIPTION("EXYNOS TMU Driver");
MODULE_AUTHOR("Donggeun Kim <dg77.kim@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:exynos-tmu");
