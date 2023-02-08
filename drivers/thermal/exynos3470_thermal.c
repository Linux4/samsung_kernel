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
#include <linux/delay.h>

#include <plat/cpu.h>

#include <mach/tmu.h>
#include <mach/smc.h>
#include <mach/map.h>
#include <mach/sec_debug.h>

/*Exynos generic registers*/
#define EXYNOS_TMU_REG_TRIMINFO		0x0
#define EXYNOS_TMU_REG_CONTROL		0x20
#define EXYNOS_TMU_REG_STATUS		0x28
#define EXYNOS_TMU_REG_CURRENT_TEMP	0x40
#define EXYNOS_TMU_REG_INTEN		0x70
#define EXYNOS_TMU_REG_INTSTAT		0x74
#define EXYNOS_TMU_REG_INTCLEAR		0x78

#define EXYNOS_TMU_TRIM_TEMP_MASK	0xff
#define EXYNOS_TMU_GAIN_SHIFT		8
#define EXYNOS_TMU_REF_VOLTAGE_SHIFT	24
#define EXYNOS_TMU_CORE_ON		3
#define EXYNOS_TMU_CORE_OFF		2
#define EXYNOS_TMU_DEF_CODE_TO_TEMP_OFFSET	50
#define EXYNOS_THERM_TRIP_EN		(1 << 12)
#define EXYNOS_TRIMINFO_RELOAD1		0x01
#define EXYNOS_TRIMINFO_RELOAD2		0x11
#define EXYNOS_TMU_TRIMINFO_CON1	0x10
#define EXYNOS_TMU_TRIMINFO_CON2	0x14

/*Exynos4 specific registers*/
#define EXYNOS4_TMU_REG_THRESHOLD_TEMP	0x44
#define EXYNOS4_TMU_REG_TRIG_LEVEL0	0x50
#define EXYNOS4_TMU_REG_TRIG_LEVEL1	0x54
#define EXYNOS4_TMU_REG_TRIG_LEVEL2	0x58
#define EXYNOS4_TMU_REG_TRIG_LEVEL3	0x5C
#define EXYNOS4_TMU_REG_PAST_TEMP0	0x60
#define EXYNOS4_TMU_REG_PAST_TEMP1	0x64
#define EXYNOS4_TMU_REG_PAST_TEMP2	0x68
#define EXYNOS4_TMU_REG_PAST_TEMP3	0x6C

#define EXYNOS4_TMU_TRIG_LEVEL0_MASK	0x1
#define EXYNOS4_TMU_TRIG_LEVEL1_MASK	0x10
#define EXYNOS4_TMU_TRIG_LEVEL2_MASK	0x100
#define EXYNOS4_TMU_TRIG_LEVEL3_MASK	0x1000
#define EXYNOS4_TMU_INTCLEAR_VAL	0x1111

/* Exynos4270 specific registers */
#define EXYNOS4270_TMU_THRESHOLD_TEMP_RISE	0x50
#define EXYNOS4270_TMU_THRESHOLD_TEMP_FALL	0x54
#define EXYNOS4270_TMU_SAMPLING_INTERVAL	0x002C
#define EXYNOS4270_TMU_CLEAR_RISE_INT		0x111
#define EXYNOS4270_TMU_CLEAR_FALL_INT		0x111000
#define EXYNOS4270_TMU_DISABLE_RISE0		0x1110110
#define EXYNOS4270_TMU_DISABLE_RISE1		0x1110101
#define EXYNOS4270_TMU_DISABLE_FALL0		0x1100111
#define EXYNOS4270_TMU_DISABLE_FALL1		0x1010111

/*Exynos5 specific registers*/
#define EXYNOS5_THD_TEMP_RISE		0x50
#define EXYNOS5_THD_TEMP_FALL		0x54
#define EXYNOS5_EMUL_CON		0x80

#define EXYNOS5_MUX_ADDR_VALUE		6
#define EXYNOS5_MUX_ADDR_SHIFT		20
#define EXYNOS5_TMU_TRIP_MODE_SHIFT	13


/*In-kernel thermal framework related macros & definations*/
#define SENSOR_NAME_LEN	16
#define MAX_TRIP_COUNT	8
#define MAX_COOLING_DEVICE 4

#define PASSIVE_INTERVAL 100
#define ACTIVE_INTERVAL 300
#define IDLE_INTERVAL 1000
#define MCELSIUS	1000

/* CPU Zone information */
#define PANIC_ZONE      4
#define WARN_ZONE       3
#define MONITOR_ZONE    2
#define SAFE_ZONE       1

#define GET_ZONE(trip) (trip + 2)
#define GET_TRIP(zone) (zone - 2)

#define EXYNOS_ZONE_COUNT	3
#define EXYNOS_TMU_COUNT	1

#define THRESH_MEM_TEMP0	10
#define THRESH_MEM_TEMP1	95
#define THRESH_MEM_TEMP2	105

#define RISE_LEVEL1_SHIFT		4
#define RISE_LEVEL2_SHIFT		8
#define RISE_LEVEL3_SHIFT		12
#define FALL_LEVEL0_SHIFT		16
#define FALL_LEVEL1_SHIFT		20
#define FALL_LEVEL2_SHIFT		24

#define THRESH_LEVE1_SHIFT     8
#define THRESH_LEVE2_SHIFT     16
#define THRESH_LEVE3_SHIFT     24

#define TRIP_EN_COUNT		4
#define GAP_WITH_RISE          2
#define MAX_FREQ		1400
#define MIN_FREQ		400
#define MIN_TEMP		10
#define MAX_TEMP		125

#define DREX_TIMINGAREF		0x30
#define AREF_CRITICAL		0x17
#define AREF_HOT		0x2E
#define AREF_NORMAL		0x5D

#ifndef CONFIG_ARM_TRUSTZONE
static void __iomem *exynos4_base_drex0;
static void __iomem *exynos4_base_drex1;
#endif


static int old_cold;
static int old_hot;

static BLOCKING_NOTIFIER_HEAD(exynos_tmu_notifier);

static struct exynos_thermal_zone *th_zone;
static struct exynos_tmu_data *tmudata;
static void exynos_unregister_thermal(void);
static int exynos_register_thermal(struct thermal_sensor_conf *sensor_conf);
static void exynos_tmu_control(struct platform_device *pdev, int id, bool on);
static int temp_to_code(struct exynos_tmu_data *data, u8 temp);

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
	/* Find the level for which trip happened */
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

	th_zone->sensor_conf->cooling_data.freq_data[trip].freq_clip_max = freq * 1000;
	printk(KERN_INFO "Set frequency[%d] : %ld\n", trip, freq);

	return 0;
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
		writel(EXYNOS4270_TMU_CLEAR_RISE_INT|EXYNOS4270_TMU_CLEAR_FALL_INT,
				data->base[i] + EXYNOS_TMU_REG_INTCLEAR);

		/* Change the trigger levels  */
		rising_threshold = readl(data->base[i] + EXYNOS4270_TMU_THRESHOLD_TEMP_RISE);
		falling_threshold = readl(data->base[i] + EXYNOS4270_TMU_THRESHOLD_TEMP_FALL);
		threshold_code = temp_to_code(data, temp);

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

		writel(rising_threshold, data->base[i] + EXYNOS4270_TMU_THRESHOLD_TEMP_RISE);
		writel(falling_threshold, data->base[i] + EXYNOS4270_TMU_THRESHOLD_TEMP_FALL);

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
	th_zone->sensor_conf->cooling_data.freq_data[0].temp_level = temp0;
	th_zone->sensor_conf->cooling_data.freq_data[1].temp_level = temp1;
	th_zone->sensor_conf->cooling_data.freq_data[2].temp_level = temp2;

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
		if (cdev == th_zone->cool_dev[i])
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
};

/* Register with the in-kernel thermal management */
static int exynos_register_thermal(struct thermal_sensor_conf *sensor_conf)
{
	int ret, count, tab_size, pos = 0;
	struct freq_clip_table *tab_ptr, *clip_data;

	if (!sensor_conf || !sensor_conf->read_temperature) {
		pr_err("Temperature sensor not initialised\n");
		return -EINVAL;
	}

	th_zone = kzalloc(sizeof(struct exynos_thermal_zone), GFP_KERNEL);
	if (!th_zone)
		return -ENOMEM;

	th_zone->sensor_conf = sensor_conf;

	tab_ptr = (struct freq_clip_table *)sensor_conf->cooling_data.freq_data;

	/* Register the cpufreq cooling device */
	for (count = 0; count < EXYNOS_ZONE_COUNT; count++) {
		tab_size = sensor_conf->cooling_data.size[count];
		if (tab_size == 0)
			continue;

		clip_data = (struct freq_clip_table *)&(tab_ptr[pos]);

		th_zone->cool_dev[count] = cpufreq_cooling_register(
						clip_data, tab_size);
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
	int i;

	for (i = 0; i < th_zone->cool_dev_size; i++) {
		if (th_zone && th_zone->cool_dev[i])
			cpufreq_cooling_unregister(th_zone->cool_dev[i]);
	}

	if (th_zone && th_zone->therm_dev)
		thermal_zone_device_unregister(th_zone->therm_dev);

	kfree(th_zone);

	pr_info("Exynos: Kernel Thermal management unregistered\n");
}

int exynos_tmu_add_notifier(struct notifier_block *n)
{
	return blocking_notifier_chain_register(&exynos_tmu_notifier, n);
}

static void exynos_memory_auto_refresh(unsigned long event, void *v)
{
	switch (event) {
#ifdef CONFIG_ARM_TRUSTZONE
		case TMU_THR_LV1:
			exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(EXYNOS4_PA_DMC0_4X12 + DREX_TIMINGAREF),
					AREF_NORMAL, 0);
			exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(EXYNOS4_PA_DMC1_4X12 + DREX_TIMINGAREF),
					AREF_NORMAL, 0);
			break;
		case TMU_THR_LV2:
			exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(EXYNOS4_PA_DMC0_4X12 + DREX_TIMINGAREF),
					AREF_HOT, 0);
			exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(EXYNOS4_PA_DMC1_4X12 + DREX_TIMINGAREF),
					AREF_HOT, 0);
			break;
		case TMU_THR_LV3:
			exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(EXYNOS4_PA_DMC0_4X12 + DREX_TIMINGAREF),
					AREF_CRITICAL, 0);
			exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(EXYNOS4_PA_DMC1_4X12 + DREX_TIMINGAREF),
					AREF_CRITICAL, 0);
			break;
#else
		case TMU_THR_LV1:
			__raw_writel(AREF_NORMAL,(exynos4_base_drex0 + DREX_TIMINGAREF));
			__raw_writel(AREF_NORMAL,(exynos4_base_drex1 + DREX_TIMINGAREF));
			break;
		case TMU_THR_LV2:
			__raw_writel(AREF_HOT,(exynos4_base_drex0 + DREX_TIMINGAREF));
			__raw_writel(AREF_HOT,(exynos4_base_drex1 + DREX_TIMINGAREF));
			break;
		case TMU_THR_LV3:
			__raw_writel(AREF_CRITICAL,(exynos4_base_drex0 + DREX_TIMINGAREF));
			__raw_writel(AREF_CRITICAL,(exynos4_base_drex1 + DREX_TIMINGAREF));
			break;
#endif
	}
}

void exynos_tmu_call_notifier(int cold, int hot)
{
	if (old_cold != cold) {
		blocking_notifier_call_chain(&exynos_tmu_notifier, TMU_COLD, &cold);
		old_cold = cold;
	}

	if (old_hot != hot) {
		blocking_notifier_call_chain(&exynos_tmu_notifier, hot, &old_hot);
		exynos_memory_auto_refresh(hot, &old_hot);
		old_hot = hot;
	}
}

/*
 * TMU treats temperature as a mapped temperature code.
 * The temperature is converted differently depending on the calibration type.
 */
static int temp_to_code(struct exynos_tmu_data *data, u8 temp)
{
	struct exynos_tmu_platform_data *pdata = data->pdata;
	int temp_code;

	if (temp > MAX_TEMP) {
		temp_code = MAX_TEMP;
		goto out;
	}
	else if (temp < MIN_TEMP) {
		temp_code = MIN_TEMP;
		goto out;
	}

	switch (pdata->cal_type) {
	case TYPE_TWO_POINT_TRIMMING:
		temp_code = (temp - 25) *
		    (data->temp_error2 - data->temp_error1) /
		    (85 - 25) + data->temp_error1;
		break;
	case TYPE_ONE_POINT_TRIMMING:
		temp_code = temp + data->temp_error1 - 25;
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
static int code_to_temp(struct exynos_tmu_data *data, u8 temp_code)
{
	struct exynos_tmu_platform_data *pdata = data->pdata;
	int temp;

	switch (pdata->cal_type) {
	case TYPE_TWO_POINT_TRIMMING:
		temp = (temp_code - data->temp_error1) * (85 - 25) /
			(data->temp_error2 - data->temp_error1) + 25;
		break;
	case TYPE_ONE_POINT_TRIMMING:
		temp = temp_code - data->temp_error1 + 25;
		break;
	default:
		temp = temp_code - EXYNOS_TMU_DEF_CODE_TO_TEMP_OFFSET;
		break;
	}

	/* temperature should range between min(10) and max(125) */
	if (temp > MAX_TEMP)
		temp = MAX_TEMP;
	else if (temp < MIN_TEMP)
		temp = MIN_TEMP;

	return temp;
}

static int exynos_tmu_initialize(struct platform_device *pdev, int id)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct exynos_tmu_platform_data *pdata = data->pdata;
	unsigned int status, trim_info, rising_threshold, falling_threshold;
	int ret = 0, timeout =5, threshold_code;

	mutex_lock(&data->lock);
	clk_enable(data->clk);

	status = readb(data->base[id] + EXYNOS_TMU_REG_STATUS);
	if (!status) {
		ret = -EBUSY;
		goto out;
	}

	__raw_writel(EXYNOS_TRIMINFO_RELOAD1,
			data->base[id] + EXYNOS_TMU_TRIMINFO_CON1);
	__raw_writel(EXYNOS_TRIMINFO_RELOAD2,
			data->base[id] + EXYNOS_TMU_TRIMINFO_CON2);

	while(readl(data->base[id] + EXYNOS_TMU_TRIMINFO_CON2) & EXYNOS_TRIMINFO_RELOAD1) {
		if(!timeout) {
			pr_err("Thermal TRIMINFO register reload failed\n");
			break;
		}
		timeout--;
		cpu_relax();
		usleep_range(5,10);
	}
	/* Save trimming info in order to perform calibration */
	trim_info = readl(data->base[id] + EXYNOS_TMU_REG_TRIMINFO);
	data->temp_error1 = trim_info & EXYNOS_TMU_TRIM_TEMP_MASK;
	data->temp_error2 = ((trim_info >> 8) & EXYNOS_TMU_TRIM_TEMP_MASK);

	if ((EFUSE_MIN_VALUE > data->temp_error1) ||
			(data->temp_error1 > EFUSE_MAX_VALUE) ||
			(data->temp_error2 != 0))
		data->temp_error1 = pdata->efuse_value;

	/* Write temperature code for threshold */
	threshold_code = temp_to_code(data, pdata->threshold +
			pdata->trigger_levels[0]);
	if (threshold_code < 0) {
		ret = threshold_code;
		goto out;
	}
	rising_threshold = threshold_code;
	falling_threshold = threshold_code - GAP_WITH_RISE;
	threshold_code = temp_to_code(data, pdata->threshold +
			pdata->trigger_levels[1]);
	if (threshold_code < 0) {
		ret = threshold_code;
		goto out;
	}
	rising_threshold |= (threshold_code << THRESH_LEVE1_SHIFT);
	falling_threshold |= ((threshold_code - GAP_WITH_RISE) << THRESH_LEVE1_SHIFT);
	threshold_code = temp_to_code(data, pdata->threshold +
			pdata->trigger_levels[2]);
	if (threshold_code < 0) {
		ret = threshold_code;
		goto out;
	}
	rising_threshold |= (threshold_code << THRESH_LEVE2_SHIFT);
	falling_threshold |= ((threshold_code - GAP_WITH_RISE) << THRESH_LEVE2_SHIFT);
	threshold_code = temp_to_code(data, pdata->threshold +
			pdata->trigger_levels[3]);
	if (threshold_code < 0) {
		ret = threshold_code;
		goto out;
	}
	rising_threshold |= (threshold_code << THRESH_LEVE3_SHIFT);

	writel(rising_threshold,
			data->base[0] + EXYNOS4270_TMU_THRESHOLD_TEMP_RISE);
	writel(falling_threshold, data->base[0] + EXYNOS4270_TMU_THRESHOLD_TEMP_FALL);
	writel(EXYNOS4270_TMU_CLEAR_RISE_INT | EXYNOS4270_TMU_CLEAR_FALL_INT,
			data->base[0] + EXYNOS_TMU_REG_INTCLEAR);

out:
	clk_disable(data->clk);
	mutex_unlock(&data->lock);

	return ret;
}

static void exynos_tmu_control(struct platform_device *pdev, int id, bool on)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct exynos_tmu_platform_data *pdata = data->pdata;
	unsigned int con, interrupt_en;

	mutex_lock(&data->lock);
	clk_enable(data->clk);

	con = pdata->reference_voltage << EXYNOS_TMU_REF_VOLTAGE_SHIFT |
		pdata->gain << EXYNOS_TMU_GAIN_SHIFT;

	if (data->soc == SOC_ARCH_EXYNOS5) {
		con |= pdata->noise_cancel_mode << EXYNOS5_TMU_TRIP_MODE_SHIFT;
		con |= (EXYNOS5_MUX_ADDR_VALUE << EXYNOS5_MUX_ADDR_SHIFT);
	}

	if (on) {
		con |= (EXYNOS_TMU_CORE_ON | EXYNOS_THERM_TRIP_EN);
		interrupt_en =
			pdata->trigger_level2_en << FALL_LEVEL2_SHIFT |
			pdata->trigger_level1_en << FALL_LEVEL1_SHIFT |
			pdata->trigger_level0_en << FALL_LEVEL0_SHIFT |
			pdata->trigger_level3_en << RISE_LEVEL3_SHIFT |
			pdata->trigger_level2_en << RISE_LEVEL2_SHIFT |
			pdata->trigger_level1_en << RISE_LEVEL1_SHIFT |
			pdata->trigger_level0_en;
	} else {
		con |= EXYNOS_TMU_CORE_OFF;
		interrupt_en = 0; /* Disable all interrupts */
	}
	con |= EXYNOS_THERM_TRIP_EN;
	writel(interrupt_en, data->base[id] + EXYNOS_TMU_REG_INTEN);
	writel(con, data->base[id] + EXYNOS_TMU_REG_CONTROL);

	clk_disable(data->clk);
	mutex_unlock(&data->lock);
}

static int exynos_tmu_read(struct exynos_tmu_data *data)
{
	u8 temp_code;
	int temp, i;
	int cold_event = old_cold;
	int hot_event = old_hot;
	int alltemp[EXYNOS_TMU_COUNT] = {0,};

	if (!th_zone || !th_zone->therm_dev)
		return -EPERM;

	mutex_lock(&data->lock);
	clk_enable(data->clk);

	for (i = 0; i < EXYNOS_TMU_COUNT; i++) {
		temp_code = readb(data->base[i] + EXYNOS_TMU_REG_CURRENT_TEMP);
		temp = code_to_temp(data, temp_code);
		alltemp[i] = temp;
	}

	clk_disable(data->clk);
	mutex_unlock(&data->lock);

	if (temp <= THRESH_MEM_TEMP0)
		cold_event = TMU_COLD;
	else
		cold_event = TMU_NORMAL;

	if (old_hot != TMU_THR_LV3 && temp >= THRESH_MEM_TEMP2)
		hot_event = TMU_THR_LV3;
	else if (old_hot != TMU_THR_LV2 && (temp >= THRESH_MEM_TEMP1 && temp < THRESH_MEM_TEMP2))
		hot_event = TMU_THR_LV2;
	else if (old_hot != TMU_THR_LV1 && (temp >= THRESH_MEM_TEMP0 && temp < THRESH_MEM_TEMP1))
		hot_event = TMU_THR_LV1;

	sec_debug_aux_log(SEC_DEBUG_AUXLOG_THERMAL_CHANGE, "[TMU] %d", temp);

	th_zone->therm_dev->last_temperature = temp;
	exynos_tmu_call_notifier(cold_event, hot_event);
	sec_debug_aux_log(SEC_DEBUG_AUXLOG_THERMAL_CHANGE, "[TMU] alltemp[0]: %d", alltemp[0]);

	return temp;
}

static void exynos_tmu_work(struct work_struct *work)
{
	struct exynos_tmu_data *data = container_of(work,
			struct exynos_tmu_data, irq_work);

	int i;

	mutex_lock(&data->lock);
	clk_enable(data->clk);


	if (data->soc == SOC_ARCH_EXYNOS5) {
		for (i = 0; i < EXYNOS_TMU_COUNT; i++) {
			writel(EXYNOS5_TMU_CLEAR_RISE_INT,
					data->base[i] + EXYNOS_TMU_REG_INTCLEAR);
		}
	}

	clk_disable(data->clk);
	mutex_unlock(&data->lock);
	exynos_report_trigger();

	if (data->soc == SOC_ARCH_EXYNOS5) {
		for (i = 0; i < EXYNOS_TMU_COUNT; i++)
			enable_irq(data->irq[i]);
	}
}

static irqreturn_t exynos_tmu_irq(int irq, void *id)
{
	struct exynos_tmu_data *data = id;
	unsigned int status = 0;

	pr_debug("[TMUIRQ] irq = %d\n", irq);
	sec_debug_aux_log(SEC_DEBUG_AUXLOG_THERMAL_CHANGE, "[IRQ] %d", irq);

	if (data->soc == SOC_ARCH_EXYNOS4) {
		status = readl(data->base[0] + EXYNOS_TMU_REG_INTSTAT);
		writel(EXYNOS4270_TMU_CLEAR_RISE_INT|EXYNOS4270_TMU_CLEAR_FALL_INT,
				data->base[0] + EXYNOS_TMU_REG_INTCLEAR);

		if (status & (1 << FALL_LEVEL1_SHIFT))
			writel(EXYNOS4270_TMU_DISABLE_FALL1, data->base[0] + EXYNOS_TMU_REG_INTEN);
		else if (status & (1 << FALL_LEVEL0_SHIFT))
			writel(EXYNOS4270_TMU_DISABLE_FALL0, data->base[0] + EXYNOS_TMU_REG_INTEN);
		else if (status & (1 << RISE_LEVEL1_SHIFT))
			writel(EXYNOS4270_TMU_DISABLE_RISE1, data->base[0] + EXYNOS_TMU_REG_INTEN);
		else if (status & 1)
			writel(EXYNOS4270_TMU_DISABLE_RISE0, data->base[0] + EXYNOS_TMU_REG_INTEN);
	}
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
		exynos_tmu_call_notifier(1, old_hot);
		break;
	case PM_POST_SUSPEND:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_pm_nb = {
	.notifier_call = exynos_pm_notifier,
};

#if defined(CONFIG_CPU_EXYNOS4210)
static struct exynos_tmu_platform_data const exynos4_default_tmu_data = {
	.threshold = 80,
	.trigger_levels[0] = 5,
	.trigger_levels[1] = 20,
	.trigger_levels[2] = 30,
	.trigger_level0_en = 1,
	.trigger_level1_en = 1,
	.trigger_level2_en = 1,
	.trigger_level3_en = 0,
	.gain = 15,
	.reference_voltage = 7,
	.cal_type = TYPE_ONE_POINT_TRIMMING,
	.freq_tab[0] = {
		.freq_clip_max = 800 * 1000,
		.temp_level = 85,
	},
	.freq_tab[1] = {
		.freq_clip_max = 200 * 1000,
		.temp_level = 100,
	},
	.freq_tab_count = 2,
	.type = SOC_ARCH_EXYNOS4,
};
#define EXYNOS4_TMU_DRV_DATA (&exynos4_default_tmu_data)
#else
#define EXYNOS4_TMU_DRV_DATA (NULL)
#endif

#if defined(CONFIG_SOC_EXYNOS5250)
static struct exynos_tmu_platform_data const exynos5_default_tmu_data = {
	.trigger_levels[0] = 70,
	.trigger_levels[1] = 75,
	.trigger_levels[2] = 110,
	.trigger_level0_en = 1,
	.trigger_level1_en = 1,
	.trigger_level2_en = 1,
	.trigger_level3_en = 0,
	.gain = 8,
	.reference_voltage = 16,
	.noise_cancel_mode = 4,
	.cal_type = TYPE_ONE_POINT_TRIMMING,
	.efuse_value = 55,
	.freq_tab[0] = {
		.freq_clip_max = 1600 * 1000,
		.temp_level = 70,
	},
	.freq_tab[1] = {
		.freq_clip_max = 1400 * 1000,
		.temp_level = 75,
	},
	.freq_tab[2] = {
		.freq_clip_max = 1200 * 1000,
		.temp_level = 80,
	},
	.freq_tab[3] = {
		.freq_clip_max = 800 * 1000,
		.temp_level = 85,
	},
	.freq_tab[4] = {
		.freq_clip_max = 400 * 1000,
		.temp_level = 100,
	},
	.size[THERMAL_TRIP_ACTIVE] = 1,
	.size[THERMAL_TRIP_PASSIVE] = 3,
	.size[THERMAL_TRIP_HOT] = 1,
	.freq_tab_count = 5,
	.type = SOC_ARCH_EXYNOS5,
};
#define EXYNOS5_TMU_DRV_DATA (&exynos5_default_tmu_data)
#else
#define EXYNOS5_TMU_DRV_DATA (NULL)
#endif

#ifdef CONFIG_OF
static const struct of_device_id exynos_tmu_match[] = {
	{
		.compatible = "samsung,exynos4-tmu",
		.data = (void *)EXYNOS4_TMU_DRV_DATA,
	},
	{
		.compatible = "samsung,exynos5-tmu",
		.data = (void *)EXYNOS5_TMU_DRV_DATA,
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_tmu_match);
#else
#define  exynos_tmu_match NULL
#endif

static struct platform_device_id exynos_tmu_driver_ids[] = {
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
#ifdef CONFIG_OF
	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		match = of_match_node(exynos_tmu_match, pdev->dev.of_node);
		if (!match)
			return NULL;
		return (struct exynos_tmu_platform_data *) match->data;
	}
#endif
	return (struct exynos_tmu_platform_data *)
		platform_get_device_id(pdev)->driver_data;
}

static int __devinit exynos_tmu_probe(struct platform_device *pdev)
{
	struct exynos_tmu_platform_data *pdata = pdev->dev.platform_data;
	int ret, i, count = 0;
	int trigger_level_en[TRIP_EN_COUNT];

	if (!pdata)
		pdata = exynos_get_driver_data(pdev);

	if (!pdata) {
		dev_err(&pdev->dev, "No platform init data supplied.\n");
		return -ENODEV;
	}
	tmudata = kzalloc(sizeof(struct exynos_tmu_data), GFP_KERNEL);
	if (!tmudata) {
		dev_err(&pdev->dev, "Failed to allocate driver structure\n");
		return -ENOMEM;
	}

	INIT_WORK(&tmudata->irq_work, exynos_tmu_work);

#ifndef CONFIG_ARM_TRUSTZONE
	/* ioremap for drex base address */
	exynos4_base_drex0 = ioremap(EXYNOS4_PA_DMC0_4X12, SZ_64K);
	exynos4_base_drex1 = ioremap(EXYNOS4_PA_DMC1_4X12, SZ_64K);
	if(!exynos4_base_drex0 || !exynos4_base_drex1) {
		ret = -ENOMEM;
		goto err_drex_ioremap;
	}
#endif

	for (i = 0; i < EXYNOS_TMU_COUNT; i++) {
		tmudata->irq[i] = platform_get_irq(pdev, i);
		if (tmudata->irq[i] < 0) {
			ret = tmudata->irq[i];
			dev_err(&pdev->dev, "Failed to get platform irq\n");
			goto err_get_resource;
		}

		tmudata->mem[i] = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!tmudata->mem[i]) {
			ret = -ENOENT;
			dev_err(&pdev->dev, "Failed to get platform resource\n");
			goto err_get_resource;
		}

		tmudata->mem[i] = request_mem_region(tmudata->mem[i]->start,
				resource_size(tmudata->mem[i]), pdev->name);
		if (!tmudata->mem[i]) {
			ret = -ENODEV;
			dev_err(&pdev->dev, "Failed to request memory region\n");
			goto err_mem_region;
		}

		tmudata->base[i] = ioremap(tmudata->mem[i]->start, resource_size(tmudata->mem[i]));
		if (!tmudata->base[i]) {
			ret = -ENODEV;
			dev_err(&pdev->dev, "Failed to ioremap memory\n");
			goto err_io_remap;
		}

		ret = request_irq(tmudata->irq[i], exynos_tmu_irq,
				IRQF_TRIGGER_RISING, "exynos-tmu", tmudata);
		if (ret) {
			dev_err(&pdev->dev, "Failed to request irq: %d\n", tmudata->irq[i]);
			goto err_irq;
		}
	}

	tmudata->clk = clk_get(NULL, "tmu");
	if (IS_ERR(tmudata->clk)) {
		ret = PTR_ERR(tmudata->clk);
		dev_err(&pdev->dev, "Failed to get clock\n");
		goto err_clk;
	}

	if (pdata->type == SOC_ARCH_EXYNOS5 ||
				pdata->type == SOC_ARCH_EXYNOS4)
		tmudata->soc = pdata->type;
	else {
		ret = -EINVAL;
		dev_err(&pdev->dev, "Platform not supported\n");
		goto err_soc_type;
	}

	tmudata->pdata = pdata;
	platform_set_drvdata(pdev, tmudata);
	mutex_init(&tmudata->lock);

	for (i = 0; i < EXYNOS_TMU_COUNT; i++) {
		ret = exynos_tmu_initialize(pdev, i);
		if (ret) {
			dev_err(&pdev->dev, "Failed to initialize TMU[%d]\n", i);
			goto err_tmu;
		}

		exynos_tmu_control(pdev, i, true);
	}

	/*Register the sensor with thermal management interface*/
	(&exynos_sensor_conf)->private_data = tmudata;
	exynos_sensor_conf.trip_data.trip_count = pdata->trigger_level0_en +
			pdata->trigger_level1_en + pdata->trigger_level2_en +
			pdata->trigger_level3_en;

	trigger_level_en[0] = pdata->trigger_level0_en;
	trigger_level_en[1] = pdata->trigger_level1_en;
	trigger_level_en[2] = pdata->trigger_level2_en;
	trigger_level_en[3] = pdata->trigger_level3_en;

	for (i = 0; i < TRIP_EN_COUNT; i++) {
		if (trigger_level_en[i]) {
			exynos_sensor_conf.trip_data.trip_val[count] =
				pdata->threshold + pdata->trigger_levels[i];
			count++;
		}
	}

	exynos_sensor_conf.cooling_data.freq_clip_count =
						pdata->freq_tab_count;

	for (i = 0; i <= THERMAL_TRIP_CRITICAL; i++)
		exynos_sensor_conf.cooling_data.size[i] = pdata->size[i];

	for (i = 0; i < pdata->freq_tab_count; i++) {
		exynos_sensor_conf.cooling_data.freq_data[i].freq_clip_max =
					pdata->freq_tab[i].freq_clip_max;
		exynos_sensor_conf.cooling_data.freq_data[i].temp_level =
					pdata->freq_tab[i].temp_level;
		exynos_sensor_conf.cooling_data.freq_data[i].mask_val = cpu_all_mask;
	}

	register_pm_notifier(&exynos_pm_nb);

	ret = exynos_register_thermal(&exynos_sensor_conf);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register thermal interface\n");
		goto err_register;
	}

	th_zone->exynos4_dev = pdev;

	/* For low temperature compensation when boot time */
	exynos_tmu_call_notifier(1, 0);

	return 0;

err_register:
	unregister_pm_notifier(&exynos_pm_nb);
err_tmu:
	platform_set_drvdata(pdev, NULL);
err_soc_type:
	clk_put(tmudata->clk);
err_clk:
	for (i = 0; i < EXYNOS_TMU_COUNT; i++) {
		if (tmudata->irq[i])
			free_irq(tmudata->irq[i], tmudata);
	}
err_irq:
	for (i = 0; i < EXYNOS_TMU_COUNT; i++) {
		if (tmudata->base[i])
			iounmap(tmudata->base[i]);
	}
err_io_remap:
	for (i = 0; i < EXYNOS_TMU_COUNT; i++) {
		if (tmudata->mem[i])
			release_mem_region(tmudata->mem[i]->start, resource_size(tmudata->mem[i]));
	}
err_mem_region:
err_get_resource:
#ifndef  CONFIG_ARM_TRUSTZONE
	iounmap(exynos4_base_drex0);
	iounmap(exynos4_base_drex1);
err_drex_ioremap:
	kfree(tmudata);
#endif

	return ret;
}

static int __devexit exynos_tmu_remove(struct platform_device *pdev)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < EXYNOS_TMU_COUNT; i++)
		exynos_tmu_control(pdev, i, false);

	exynos_unregister_thermal();

	clk_put(data->clk);

	for (i = 0; i < EXYNOS_TMU_COUNT; i++)
		free_irq(data->irq[i], data);

	for (i = 0; i < EXYNOS_TMU_COUNT; i++)
		iounmap(data->base[i]);
	for (i = 0; i < EXYNOS_TMU_COUNT; i++)
		release_mem_region(data->mem[i]->start, resource_size(data->mem[i]));

#ifndef CONFIG_ARM_TRUSTZONE
	iounmap(exynos4_base_drex0);
	iounmap(exynos4_base_drex1);
#endif
	platform_set_drvdata(pdev, NULL);

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

	exynos_memory_auto_refresh(old_hot, &old_hot);

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
		.of_match_table = exynos_tmu_match,
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
