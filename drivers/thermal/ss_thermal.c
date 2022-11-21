/*
 *  ss_thermal.c
 *  Samsung thermal mitigation through core wear levelling
 *
 *  Copyright (C) 2015 Samsung Electronics
 *
 */

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/msm_tsens.h>
#include <linux/workqueue.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/msm_tsens.h>
#include <linux/msm_thermal.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include <linux/thermal.h>
#include <linux/msm_thermal_ioctl.h>
#include <soc/qcom/rpm-smd.h>
#include <soc/qcom/scm.h>
#include <linux/sched/rt.h>

#include "ss_thermal.h"
#include <linux/sec_class.h>

static unsigned int step_count;
static unsigned int core_present_mitigation_temp;
static unsigned int num_max_sensors;
static unsigned int is_init_done = 0;
static unsigned int is_monitor_enabled = 1;
static unsigned int mitigation_temp_setting = CORE_MITIGATION_TEMP_VALUE;
static unsigned int temp_step_up_value_setting = TEMP_STEP_UP_VALUE;
static unsigned int temp_step_down_value_setting = TEMP_STEP_DOWN_VALUE;

struct tsens_id_to_core tsens_id_to_core_unplug[TSENS_NAME_MAX_LENGTH];
static struct core_mitigation_temp_unplug thermal_core_offline;
static struct delayed_work thermal_core_mitigate_work;


static int return_valid_core_to_offline(int sensor_id)
{
    int i, core_id = -1;

    pr_debug("****** the sensor id %d\n", sensor_id);
    for(i=0;i<num_max_sensors;i++)
    {
        if(tsens_id_to_core_unplug[i].sensor_id == sensor_id && !tsens_id_to_core_unplug[i].unplugged)
        {
            core_id = tsens_id_to_core_unplug[i].core_id;
            tsens_id_to_core_unplug[i].unplugged = true;
        }
    }
    return core_id;
}

static void plug_the_core(int core_id)
{
    int i;

    pr_debug("****** the core_id %d\n", core_id);
    for(i=0;i<num_max_sensors;i++)
    {
        if(tsens_id_to_core_unplug[i].core_id == core_id && tsens_id_to_core_unplug[i].unplugged)
            tsens_id_to_core_unplug[i].unplugged = false;
    }
}

static void unplug_online_cores(int present_step, int sensor_id)
{
    int ret_core;

    ret_core = return_valid_core_to_offline(sensor_id);
    if(ret_core == -1)
        return;
    cpu_hotplug_driver_lock();
    thermal_core_offline.core_to_unplug[present_step] = ret_core;
    ret_core = cpu_down(thermal_core_offline.core_to_unplug[present_step]);
    if(!ret_core)
        printk("****** offlined the cpu %d\n", thermal_core_offline.core_to_unplug[present_step]);
    else
        printk("****** Failed to offline the cpu %d\n", thermal_core_offline.core_to_unplug[present_step]);
    cpu_hotplug_driver_unlock();
}

static void plug_offline_cores(int present_step)
{
    int ret;

    cpu_hotplug_driver_lock();
    plug_the_core(thermal_core_offline.core_to_unplug[present_step]);
    ret = cpu_up(thermal_core_offline.core_to_unplug[present_step]);
    if(!ret)
        printk("****** onlined the cpu %d\n", thermal_core_offline.core_to_unplug[present_step]);
    else
        printk("****** Failed to online the cpu %d\n", thermal_core_offline.core_to_unplug[present_step]);
    cpu_hotplug_driver_unlock();
}

static bool sensor_id_core_down(int sensor_id)
{
    int i;
    bool all_cores_with_sensor=false;

    for(i=0;i<num_max_sensors;i++)
    {
        if(tsens_id_to_core_unplug[i].sensor_id == sensor_id && !tsens_id_to_core_unplug[i].unplugged && cpu_online(tsens_id_to_core_unplug[i].core_id))
            all_cores_with_sensor = true;
    }

    if(all_cores_with_sensor)
        return false;
    else
        return true;
}

static void __ref msm_therm_mitigate_core(struct work_struct *work)
{
    struct tsens_device tsens_dev;
    long temp = 0;
    int max_sens_temp = 0, max_sens_temp_unplug = 0;
    int max_temp_sens_id = 0;
    pr_debug("****** %s\n", __func__);
    pr_debug("****** %d\n", num_max_sensors);
	
    if(num_max_sensors)
    {
        int i;
        for (i = 5; i< num_max_sensors;i++)
        {
            tsens_dev.sensor_num = i;
            tsens_get_temp(&tsens_dev,&temp);
            if(max_sens_temp <= temp)
            {
                max_sens_temp_unplug = temp;
                if(!sensor_id_core_down(i))
                {
                    max_sens_temp = temp;
                    max_temp_sens_id = i;
                }
            }
        }
        pr_debug("****** max sens temp = %d, sensor = %d, present mitogation temp =%d\n",
                                        max_sens_temp, max_temp_sens_id, core_present_mitigation_temp);

        pr_debug("****** step count = %d\n", step_count);
        if(max_sens_temp > core_present_mitigation_temp )
	{
            pr_info("****** temp > core mitigation temp %d\n", core_present_mitigation_temp);
            unplug_online_cores(step_count,max_temp_sens_id);
            step_count += 1;
            core_present_mitigation_temp = thermal_core_offline.core_mitigation_temp_step[step_count];
        }
        else if(step_count && (max_sens_temp_unplug < (thermal_core_offline.core_mitigation_temp_step[step_count]-temp_step_down_value_setting)))
        {
            pr_info("****** temp < core mitigation temp %d\n", core_present_mitigation_temp);
            plug_offline_cores(step_count-1);
            core_present_mitigation_temp = thermal_core_offline.core_mitigation_temp_step[step_count-1];
            step_count -= 1;
        }
    }

    if(step_count) {
        schedule_delayed_work(&thermal_core_mitigate_work, HZ/2);
    }
    else {
	 if (is_monitor_enabled == 0) {
	 	cancel_delayed_work(&thermal_core_mitigate_work);
		is_init_done = 0;
	 }
	 else {
              schedule_delayed_work(&thermal_core_mitigate_work, HZ*2);
	 }
    }
}

static void msm_thermal_core_mitigation_temp_init(void)
{
    int i;

    for(i=0;i<MAX_CORE_WEAR_LEVEL;i++)
    {
        thermal_core_offline.core_mitigation_temp_step[i] = mitigation_temp_setting + (i*temp_step_up_value_setting);
    }

    i=0;
    tsens_id_to_core_unplug[i].sensor_id = SENSOR_ID_PERF1;
    tsens_id_to_core_unplug[i].core_id = CORE_ID_PERF1;
    i++;

    tsens_id_to_core_unplug[i].sensor_id = SENSOR_ID_PERF2;
    tsens_id_to_core_unplug[i].core_id = CORE_ID_PERF2;
    i++;

    tsens_id_to_core_unplug[i].sensor_id = SENSOR_ID_PERF3;
    tsens_id_to_core_unplug[i].core_id = CORE_ID_PERF3;
    i++;

    tsens_id_to_core_unplug[i].sensor_id = SENSOR_ID_POWER_ALL;
    tsens_id_to_core_unplug[i].core_id = CORE_ID_POWER1;
    i++;

    tsens_id_to_core_unplug[i].sensor_id = SENSOR_ID_POWER_ALL;
    tsens_id_to_core_unplug[i].core_id = CORE_ID_POWER2;
    i++;

    tsens_id_to_core_unplug[i].sensor_id = SENSOR_ID_POWER_ALL;
    tsens_id_to_core_unplug[i].core_id = CORE_ID_POWER3;
    i++;

    tsens_get_max_sensor_num(&num_max_sensors);
    core_present_mitigation_temp = thermal_core_offline.core_mitigation_temp_step[0];

    is_init_done = 1;

    return;
}

static ssize_t enable_core_wear_levelling_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 4, "%d\n", is_monitor_enabled);
}

static ssize_t enable_core_wear_levelling_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	sscanf(buf, "%1d", &is_monitor_enabled);

	if (is_monitor_enabled == 1) {
	 	msm_thermal_core_mitigation_temp_init();
		INIT_DELAYED_WORK(&thermal_core_mitigate_work, msm_therm_mitigate_core);
		schedule_delayed_work(&thermal_core_mitigate_work,HZ*2);
		is_init_done = 1;
	}
	else {
		cancel_delayed_work(&thermal_core_mitigate_work);
	}

	return size;
}

static ssize_t mitigation_temp_value_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 4, "%d\n", mitigation_temp_setting);
}

static ssize_t mitigation_temp_value_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	sscanf(buf, "%d", &mitigation_temp_setting);
	if (step_count == 0 && is_init_done == 1)
		msm_thermal_core_mitigation_temp_init();
	else
		return -EINVAL;

	return size;
}

static ssize_t temp_step_up_value_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 4, "%d\n", temp_step_up_value_setting);
}

static ssize_t temp_step_up_value_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	sscanf(buf, "%d", &temp_step_up_value_setting);
	if (step_count == 0 && is_init_done == 1)
		msm_thermal_core_mitigation_temp_init();
	else
		return -EINVAL;

	return size;
}

static ssize_t temp_step_down_value_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 4, "%d\n", temp_step_down_value_setting);
}

static ssize_t temp_step_down_value_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	sscanf(buf, "%d", &temp_step_down_value_setting);
	if (step_count == 0 && is_init_done == 1)
		msm_thermal_core_mitigation_temp_init();
	else
		return -EINVAL;

	return size;
}

static DEVICE_ATTR(enable_core_wear_levelling, 0664, enable_core_wear_levelling_show, enable_core_wear_levelling_store);
static DEVICE_ATTR(mitigation_temp_value, 0664, mitigation_temp_value_show, mitigation_temp_value_store);
static DEVICE_ATTR(temp_step_up_value, 0664, temp_step_up_value_show, temp_step_up_value_store);
static DEVICE_ATTR(temp_step_down_value, 0664, temp_step_down_value_show, temp_step_down_value_store);

static struct attribute *sec_core_wear_levelling_attributes[] = {
	&dev_attr_enable_core_wear_levelling.attr,
	&dev_attr_mitigation_temp_value.attr,
	&dev_attr_temp_step_up_value.attr,
	&dev_attr_temp_step_down_value.attr,
	NULL
};

static const struct attribute_group sec_core_wear_levelling_attr_group = {
	.attrs = sec_core_wear_levelling_attributes,
};

int __init ss_thermal_late_init(void)
{

	int ret = 0;
	struct device *dev;

    	pr_info("%s\n", __func__);
	dev = device_create(sec_class, NULL, 0, NULL, "core_wear_levelling");
	if (IS_ERR(dev)) {
		dev_err(dev, "%s: fail to create sec_dev\n", __func__);
		return PTR_ERR(dev);
	}

	ret = sysfs_create_group(&dev->kobj, &sec_core_wear_levelling_attr_group);
	if (ret) {
		dev_err(dev, "failed to create sysfs group\n");
	}
	
	if (is_monitor_enabled == 1) {
	 	msm_thermal_core_mitigation_temp_init();
		INIT_DELAYED_WORK(&thermal_core_mitigate_work, msm_therm_mitigate_core);
		schedule_delayed_work(&thermal_core_mitigate_work,HZ*2);
		is_init_done = 1;
	}

 	return 0;
}
late_initcall(ss_thermal_late_init);
