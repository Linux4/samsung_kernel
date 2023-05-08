/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include<linux/string.h>
#include <asm/io.h>
#include <linux/thermal.h>
#include <linux/sprd_thm.h>
#include "thm.h"
#include "linux/delay.h"
#ifdef CONFIG_OF
#include <linux/slab.h>
#include <linux/of_device.h>
//#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

struct thm_handle_ops *sprd_thm_handle_ops[SPRD_MAX_SENSOR] = {NULL};
char sprd_thm_name[SPRD_MAX_SENSOR][THERMAL_NAME_LENGTH] = {{0},{0}};
extern struct thermal_zone_device *thermal_zone_get_zone_by_name(const char *name);
extern int thermal_zone_get_temp(struct thermal_zone_device *tz, unsigned long *temp);
int  sprd_thermal_temp_get(enum sprd_thm_sensor_id thermal_id,unsigned long *temp)
{
	char thermal_name[SPRD_MAX_SENSOR][THERMAL_NAME_LENGTH] = {
		"sprd_arm_thm",
		"sprd_gpu_thm",
		"sprd_pmic_thm"
	};
	struct thermal_zone_device * tz = thermal_zone_get_zone_by_name(thermal_name[thermal_id]);
	if(IS_ERR(tz) )
		return -1;
	return thermal_zone_get_temp(tz, temp);
}
EXPORT_SYMBOL(sprd_thermal_temp_get);
#if 0
static int sprd_thm_ops_init(struct sprd_thermal_zone *pzone)
{
	if((!sprd_thm_name[pzone->sensor_id]) ||(!sprd_thm_handle_ops[pzone->sensor_id])  )
		return  -1;
	strcpy(pzone->thermal_zone_name,sprd_thm_name[pzone->sensor_id]);
	pzone->ops = sprd_thm_handle_ops[pzone->sensor_id];
	return 0;
}
int sprd_thm_add(struct thm_handle_ops* ops, char *p,int id)
{
	if( (ops == NULL) ||( p == NULL) ||( id >=SPRD_MAX_SENSOR))
		return -1;
	sprd_thm_handle_ops[id] = ops;
	strcpy(sprd_thm_name[id],p);
	return 0;
}
void sprd_thm_delete(int id)
{
	sprd_thm_handle_ops[id] = NULL;
	strcpy(sprd_thm_name[id],"");
}
#endif
static int sprd_thermal_match_cdev(struct thermal_cooling_device *cdev,
				   struct sprd_trip_point *trip_point)
{
	int i;
	if (!strlen(cdev->type))
		return -EINVAL;
	for (i = 0; i < COOLING_DEV_MAX; i++) {
		if (!strcmp(trip_point->cdev_name[i], cdev->type))
			return 0;
	}
	return -ENODEV;
}

/* Callback to bind cooling device to thermal zone */
static int sprd_cdev_bind(struct thermal_zone_device *thermal,
			  struct thermal_cooling_device *cdev)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	struct sprd_thm_platform_data *ptrips = pzone->trip_tab;
	int i, ret = -EINVAL;

	printk(KERN_INFO "sprd_cdev_bind--%d \n", pzone->sensor_id);
	for (i = 0; i < ptrips->num_trips; i++) {
		if (sprd_thermal_match_cdev(cdev, &ptrips->trip_points[i]))
			continue;
		ret = thermal_zone_bind_cooling_device(thermal, i, cdev, THERMAL_NO_LIMIT, THERMAL_NO_LIMIT);
		dev_info(&cdev->device, "%s bind to %d: %d-%s\n", cdev->type,
			 i, ret, ret ? "fail" : "succeed");
	}
	return 0;
}

/* Callback to unbind cooling device from thermal zone */
static int sprd_cdev_unbind(struct thermal_zone_device *thermal,
			    struct thermal_cooling_device *cdev)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	struct sprd_thm_platform_data *ptrips = pzone->trip_tab;
	int i, ret = -EINVAL;

	for (i = 0; i < ptrips->num_trips; i++) {
		if (sprd_thermal_match_cdev(cdev, &ptrips->trip_points[i]))
			continue;
		ret = thermal_zone_unbind_cooling_device(thermal, i, cdev);
		dev_info(&cdev->device, "%s unbind from %d: %s\n",
			 cdev->type, i, ret ? "fail" : "succeed");
	}
	return ret;
}

/* Callback to get current temperature */
static int sprd_sys_get_temp(struct thermal_zone_device *thermal,
			     unsigned long *temp)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	if(!pzone->ops->read_temp)
		return -1;
	*temp = pzone->ops->read_temp(pzone);
	return 0;
}

/* Callback to get thermal zone mode */
static int sprd_sys_get_mode(struct thermal_zone_device *thermal,
			     enum thermal_device_mode *mode)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	mutex_lock(&pzone->th_lock);
	*mode = pzone->mode;
	mutex_unlock(&pzone->th_lock);
	return 0;
}

/* Callback to set thermal zone mode */
static int sprd_sys_set_mode(struct thermal_zone_device *thermal,
			     enum thermal_device_mode mode)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	mutex_lock(&pzone->th_lock);
	pzone->mode = mode;
	if (mode == THERMAL_DEVICE_ENABLED)
		schedule_work(&pzone->therm_work);
	mutex_unlock(&pzone->th_lock);
	return 0;
}
#ifdef THM_TEST
static int sprd_sys_get_regs(struct thermal_zone_device *thermal,
			    unsigned  int* regs)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	if(!pzone->ops->reg_debug_get)
		return -1;
	return pzone->ops->reg_debug_get(pzone,regs);
}

static int sprd_sys_set_regs(struct thermal_zone_device *thermal,
			     unsigned int *regs)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	if(!pzone->ops->reg_debug_set)
		return -1;
	return pzone->ops->reg_debug_set(pzone,regs);
}

/* Callback to get thermal zone logtime */
static int sprd_sys_get_logtime(struct thermal_zone_device *thermal,
			     unsigned long *logtime)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	mutex_lock(&pzone->th_lock);
	*logtime = pzone->logtime;
	mutex_unlock(&pzone->th_lock);
	return 0;
}

/* Callback to set thermal zone logtime */
static int sprd_sys_set_logtime(struct thermal_zone_device *thermal,
			     unsigned long logtime)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	mutex_lock(&pzone->th_lock);
	pzone->logtime = logtime;
	mutex_unlock(&pzone->th_lock);
	return 0;
}

/* Callback to get thermal log switch status*/
static int sprd_sys_get_logswitch(struct thermal_zone_device *thermal,
			     enum thermal_log_switch *logswitch)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	mutex_lock(&pzone->th_lock);
	*logswitch = pzone->thmlog_switch;
	mutex_unlock(&pzone->th_lock);
	return 0;
}

/* Callback to set thermal log switch status */
static int sprd_sys_set_logswitch(struct thermal_zone_device *thermal,
			     enum thermal_log_switch logswitch)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	mutex_lock(&pzone->th_lock);
	pzone->thmlog_switch = logswitch;
	if (logswitch == THERMAL_LOG_ENABLED)
		schedule_delayed_work(&pzone->thm_logtime_work, (HZ * pzone->logtime));
	mutex_unlock(&pzone->th_lock);
	return 0;
}


static ssize_t
debug_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct thermal_zone_device *tz = container_of(dev, struct thermal_zone_device, device);
	u32 regs[4] = {0};
	sprd_sys_get_regs(tz,regs);
	return sprintf(buf, "DET_PERI=%08x,MON_CTL=%08x,MON_PERI=%08x,SENSOR_CTL=%08x\n",
		regs[0],regs[1],regs[2],regs[3]);
}

static ssize_t
debug_reg_store(struct device *dev, struct device_attribute *attr,
	   const char *buf, size_t count)
{
	struct thermal_zone_device *tz = container_of(dev, struct thermal_zone_device, device);
	unsigned int regs[4]={0};
	if (!sscanf(buf, "%x,%x,%x,%x", &regs[0],&regs[1],&regs[2],&regs[3]))
		return -EINVAL;

	printk("%x,%x,%x,%x\n", regs[0],regs[1],regs[2],regs[3]);
	sprd_sys_set_regs(tz,regs);
	return count;
}
static DEVICE_ATTR(debug_reg, 0644,debug_reg_show, debug_reg_store);

static int sprd_debug_get_trip_temp(struct thermal_zone_device *thermal,
				  int trip, unsigned long *temp,unsigned long *low_off)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	struct sprd_thm_platform_data *ptrips = pzone->trip_tab;
	if (trip >= ptrips->num_trips)
		return -EINVAL;
	*temp = ptrips->trip_points[trip].temp;
	*low_off = ptrips->trip_points[trip].lowoff;
	return 0;
}

static int sprd_debug_set_trip_temp(struct thermal_zone_device *thermal,
				  int trip, unsigned long temp,unsigned long low_off)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	struct sprd_thm_platform_data *ptrips = pzone->trip_tab;
	if(!pzone->ops->trip_debug_set)
		return 0;
	ptrips->trip_points[trip].temp = temp;
	ptrips->trip_points[trip].lowoff = low_off;
	return pzone->ops->trip_debug_set(pzone,trip);
}
static ssize_t
trip_point_debug_temp_show(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct thermal_zone_device *tz = container_of(dev, struct thermal_zone_device, device);
	int trip, ret;
	long temperature,low_off;
	if (!sscanf(attr->attr.name, "trip_%d_temp", &trip))
		return -EINVAL;
	ret = sprd_debug_get_trip_temp(tz, trip, &temperature,&low_off);

	if (ret)
		return ret;
	return sprintf(buf, "tem = %ld,low_off=%ld\n", temperature,low_off);
}
static ssize_t
trip_point_debug_temp_store(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count)
{
	struct thermal_zone_device *tz = container_of(dev, struct thermal_zone_device, device);
	int trip, ret;
	unsigned long temperature,low_off;
	if (!sscanf(attr->attr.name, "trip_%d_temp", &trip))
		return -EINVAL;
	if(!sscanf(buf,"%ld,%ld",&temperature,&low_off))
		return -EINVAL;
	ret = sprd_debug_set_trip_temp(tz, trip, temperature, low_off);
	return ret ? ret : count;
}

static ssize_t
logtime_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	//struct thermal_zone_device *tz = to_thermal_zone(dev);
	struct thermal_zone_device *tz = container_of(dev, struct thermal_zone_device, device);
	unsigned long logtime;
	int result;
	result = sprd_sys_get_logtime(tz, &logtime);
	if (result)
		return result;

	return sprintf(buf, "%ld\n", logtime);

}
static ssize_t
logtime_store(struct device *dev, struct device_attribute *attr,
	   const char *buf, size_t count)
{
	//struct thermal_zone_device *tz = to_thermal_zone(dev);
	struct thermal_zone_device *tz = container_of(dev, struct thermal_zone_device, device);
	int result;
    unsigned long logtime;
    if (kstrtoul(buf, 10, &logtime))
		return -EINVAL;
		result = sprd_sys_set_logtime(tz, logtime);
	if (result)
		return result;

	return count;
}
static ssize_t
logswitch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	//struct thermal_zone_device *tz = to_thermal_zone(dev);
	struct thermal_zone_device *tz = container_of(dev, struct thermal_zone_device, device);
	enum thermal_log_switch logswtich;
	int result;

	result = sprd_sys_get_logswitch(tz, &logswtich);
	if (result)
		return result;

	return sprintf(buf, "%s\n", logswtich == THERMAL_LOG_ENABLED ? "enabled"
		       : "disabled");
}
static ssize_t
logswitch_store(struct device *dev, struct device_attribute *attr,
	   const char *buf, size_t count)
{
	struct thermal_zone_device *tz = container_of(dev, struct thermal_zone_device, device);
	int result;

	if (!strncmp(buf, "enabled", sizeof("enabled") - 1))
		result = sprd_sys_set_logswitch(tz, THERMAL_LOG_ENABLED);
	else if (!strncmp(buf, "disabled", sizeof("disabled") - 1))
		result = sprd_sys_set_logswitch(tz, THERMAL_LOG_DISABLED);
	else
		result = -EINVAL;

	if (result)
		return result;

	return count;
}

static DEVICE_ATTR(logtime, 0644, logtime_show, logtime_store);
static DEVICE_ATTR(logswitch, 0644, logswitch_show, logswitch_store);

static int create_trip_attrs(struct thermal_zone_device *tz)
{
	int indx;
	int size = sizeof(struct thermal_attr) * tz->trips;
    int result;
	result = device_create_file(&tz->device, &dev_attr_logswitch);
	if (result)
		return result;
	result = device_create_file(&tz->device, &dev_attr_logtime);
	if (result)
	    return result;
	tz->trip_temp_attrs = kzalloc(size, GFP_KERNEL);
	if (!tz->trip_temp_attrs) {
		return -ENOMEM;
	}

	for (indx = 0; indx < tz->trips; indx++) {
		/* create trip temp attribute */
		snprintf(tz->trip_temp_attrs[indx].name, THERMAL_NAME_LENGTH,
			 "trip_%d_temp", indx);

		sysfs_attr_init(&tz->trip_temp_attrs[indx].attr.attr);
		tz->trip_temp_attrs[indx].attr.attr.name =
						tz->trip_temp_attrs[indx].name;
		tz->trip_temp_attrs[indx].attr.attr.mode = S_IRUGO;
		tz->trip_temp_attrs[indx].attr.show = trip_point_debug_temp_show;
		tz->trip_temp_attrs[indx].attr.attr.mode |= S_IWUSR;
		tz->trip_temp_attrs[indx].attr.store =trip_point_debug_temp_store;
		device_create_file(&tz->device,
				   &tz->trip_temp_attrs[indx].attr);
	}
	return 0;
}

static void remove_trip_attrs(struct thermal_zone_device *tz)
{
	int indx;
	device_remove_file(&tz->device, &dev_attr_logswitch);
	device_remove_file(&tz->device, &dev_attr_logtime);
	for (indx = 0; indx < tz->trips; indx++) {
		device_remove_file(&tz->device,
				   &tz->trip_temp_attrs[indx].attr);
	}
	kfree(tz->trip_temp_attrs);
}
#endif

void sprd_thermal_remove(struct sprd_thermal_zone *pzone)
{
#ifdef THM_TEST
	remove_trip_attrs(pzone->therm_dev);
	device_remove_file(&pzone->therm_dev->device, &dev_attr_debug_reg);
	cancel_delayed_work_sync(&pzone->thm_logtime_work);
#endif
	thermal_zone_device_unregister(pzone->therm_dev);
	cancel_delayed_work_sync(&pzone->thm_read_work);
	cancel_delayed_work_sync(&pzone->resume_delay_work);
	mutex_destroy(&pzone->th_lock);
}

static int sprd_sys_get_trend(struct thermal_zone_device *thermal,
		int trip, enum thermal_trend * ptrend)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	return pzone->ops->get_trend(pzone, trip, ptrend);
}

static int sprd_sys_get_hyst(struct thermal_zone_device *thermal,
		int trip, unsigned long *physt)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	return pzone->ops->get_hyst(pzone, trip, physt);
}


/* Callback to get trip point type */
static int sprd_sys_get_trip_type(struct thermal_zone_device *thermal,
				  int trip, enum thermal_trip_type *type)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	struct sprd_thm_platform_data *ptrips = pzone->trip_tab;
	if (trip >= ptrips->num_trips)
		return -EINVAL;
	*type = ptrips->trip_points[trip].type;
	return 0;
}

/* Callback to get trip point temperature */
static int sprd_sys_get_trip_temp(struct thermal_zone_device *thermal,
				  int trip, unsigned long *temp)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	struct sprd_thm_platform_data *ptrips = pzone->trip_tab;
	if (trip >= ptrips->num_trips)
		return -EINVAL;
	*temp = ptrips->trip_points[trip].temp;
	return 0;
}

/* Callback to get critical trip point temperature */
static int sprd_sys_get_crit_temp(struct thermal_zone_device *thermal,
				  unsigned long *temp)
{
	struct sprd_thermal_zone *pzone = thermal->devdata;
	struct sprd_thm_platform_data *ptrips = pzone->trip_tab;
	int i;
	for (i = ptrips->num_trips - 1; i > 0; i--) {
		if (ptrips->trip_points[i].type == THERMAL_TRIP_CRITICAL) {
			*temp = ptrips->trip_points[i].temp;
			return 0;
		}
	}
	return -EINVAL;
}

static struct thermal_zone_device_ops thdev_ops = {
	.bind = sprd_cdev_bind,
	.unbind = sprd_cdev_unbind,
	.get_temp = sprd_sys_get_temp,
	.get_mode = sprd_sys_get_mode,
	.set_mode = sprd_sys_set_mode,
	.get_trip_type = sprd_sys_get_trip_type,
	.get_trip_temp = sprd_sys_get_trip_temp,
	.get_trip_hyst = sprd_sys_get_hyst,
	.get_crit_temp = sprd_sys_get_crit_temp,
	.get_trend = sprd_sys_get_trend,
};
static void sprd_thermal_resume_delay_work(struct work_struct *work)
{
	struct sprd_thermal_zone *pzone;

	pzone = container_of(work, struct sprd_thermal_zone, resume_delay_work.work);
	dev_dbg(&pzone->therm_dev->device, "thermal resume delay work Started.\n");
	pzone->ops->resume(pzone);
	dev_dbg(&pzone->therm_dev->device, "thermal resume delay work finished.\n");
}
#ifdef THM_TEST
static void thm_logtime_work(struct work_struct *work)
{
	struct sprd_thermal_zone *pzone;
	unsigned long temp;
	pzone = container_of(work, struct sprd_thermal_zone, thm_logtime_work.work);
	temp = pzone->ops->read_temp(pzone);
	if(pzone->thmlog_switch == THERMAL_LOG_ENABLED ){
		schedule_delayed_work(&pzone->thm_logtime_work, (HZ * pzone->logtime));
	}
}
#endif

static void thm_read_work(struct work_struct *work)
{
	int ret;
	enum thermal_device_mode cur_mode;
	struct sprd_thermal_zone *pzone;
	pzone = container_of(work, struct sprd_thermal_zone, thm_read_work.work);
	mutex_lock(&pzone->th_lock);
	cur_mode = pzone->mode;
	mutex_unlock(&pzone->th_lock);
	if (cur_mode == THERMAL_DEVICE_DISABLED)
		return;
	if(pzone->temp_inteval == 0)
		return;
	ret = thermal_zone_get_temp(pzone->therm_dev, &pzone->cur_temp);
	if (ret) {
		dev_warn(&pzone->therm_dev->device, "failed to read out thermal zone %d\n",
			 pzone->therm_dev->id);
		return;
	}
	printk("thm id:%d, inteval:%d, last_temp:%ld,cur_temp:%ld\n", pzone->sensor_id,pzone->temp_inteval,pzone->last_temp,pzone->cur_temp);
    if(pzone->cur_temp > pzone->last_temp)
	{
		pzone->trend_val = THERMAL_TREND_RAISING;
		if (pzone->cur_temp >= pzone->trip_tab->trip_points[pzone->current_trip_num].temp ){
			if((pzone->trip_tab->num_trips-1) > pzone->current_trip_num )
			pzone->current_trip_num++;
			thermal_zone_device_update(pzone->therm_dev);
		}
	}else if(pzone->cur_temp < pzone->last_temp){
		pzone->trend_val = THERMAL_TREND_DROPPING;
			printk("pzone->trend_val = THERMAL_TREND_DROPPING\n");
		if (pzone->cur_temp < pzone->trip_tab->trip_points[pzone->current_trip_num].lowoff ){
			pzone->current_trip_num--;
			thermal_zone_device_update(pzone->therm_dev);
		}
	}else{
		printk("pzone->trend_val = THERMAL_TREND_STABLE\n");
		pzone->trend_val = THERMAL_TREND_STABLE;
	}
    pzone->last_temp = pzone->cur_temp;
	schedule_delayed_work(&pzone->thm_read_work, (HZ * pzone->temp_inteval));
}

int sprd_thermal_init(struct sprd_thermal_zone *pzone)
{
	int ret=0;
	pzone->mode = THERMAL_DEVICE_DISABLED;
	pzone->trend_val = THERMAL_TREND_STABLE;
	INIT_DELAYED_WORK(&pzone->thm_read_work, thm_read_work);
	INIT_DELAYED_WORK(&pzone->resume_delay_work,sprd_thermal_resume_delay_work);
	pzone->therm_dev =
	    thermal_zone_device_register(pzone->thermal_zone_name,
					 pzone->trip_tab->num_trips, 0, pzone,
					 &thdev_ops, 0, 0, 0);
	if (IS_ERR_OR_NULL(pzone->therm_dev)) {
		printk("Register thermal zone device failed.\n");
		return PTR_ERR(pzone->therm_dev);
	}
	pzone->mode = THERMAL_DEVICE_ENABLED;
	schedule_delayed_work(&pzone->thm_read_work, (HZ * pzone->temp_inteval));
#ifdef THM_TEST
	pzone->logtime= 2;
	pzone->thmlog_switch= THERMAL_LOG_DISABLED;
	INIT_DELAYED_WORK(&pzone->thm_logtime_work, thm_logtime_work);
	create_trip_attrs(pzone->therm_dev);
	ret = device_create_file(&pzone->therm_dev->device, &dev_attr_debug_reg);
#endif
   printk(" sprd_thermal_init end\n");
	return 0;
}

