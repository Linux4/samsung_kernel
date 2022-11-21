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
	if( (ops == NULL) ||( p == NULL) ||( id > SPRD_MAX_SENSOR))
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

static int create_trip_attrs(struct thermal_zone_device *tz)
{
	int indx;
	int size = sizeof(struct thermal_attr) * tz->trips;

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
	for (indx = 0; indx < tz->trips; indx++) {
		device_remove_file(&tz->device,
				   &tz->trip_temp_attrs[indx].attr);
	}
	kfree(tz->trip_temp_attrs);
}
#endif
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
	.get_crit_temp = sprd_sys_get_crit_temp,
};

static irqreturn_t sprd_thm_irq_handler(int irq, void *irq_data)
{
	struct sprd_thermal_zone *pzone = irq_data;

	dev_dbg(&pzone->therm_dev->device, "sprd_thm_irq_handler\n");
	if (!pzone->ops->irq_handle(pzone)) {
		schedule_work(&pzone->therm_work);
	}
	return IRQ_HANDLED;
}

static void sprd_thermal_work(struct work_struct *work)
{
	enum thermal_device_mode cur_mode;
	struct sprd_thermal_zone *pzone;

	pzone = container_of(work, struct sprd_thermal_zone, therm_work);
	mutex_lock(&pzone->th_lock);
	cur_mode = pzone->mode;
	mutex_unlock(&pzone->th_lock);
	if (cur_mode == THERMAL_DEVICE_DISABLED)
		return;
	thermal_zone_device_update(pzone->therm_dev);
	dev_dbg(&pzone->therm_dev->device, "thermal work finished.\n");
}

static void sprd_thermal_resume_delay_work(struct work_struct *work)
{
	struct sprd_thermal_zone *pzone;

	pzone = container_of(work, struct sprd_thermal_zone, resume_delay_work.work);
	dev_dbg(&pzone->therm_dev->device, "thermal resume delay work Started.\n");
	pzone->ops->resume(pzone);
	dev_dbg(&pzone->therm_dev->device, "thermal resume delay work finished.\n");
}

#ifdef CONFIG_OF
static struct sprd_thm_platform_data *thermal_detect_parse_dt(
                         struct device *dev)
{
	struct sprd_thm_platform_data *pdata;
	struct device_node *np = dev->of_node;
	const char *cooling_names = "cooling-names";
	const char *point_arr[5];
	//char cool_name[COOLING_DEV_MAX][THERMAL_NAME_LENGTH];
	int ret;
	u32 trip_points_critical,trip_num,cool_num;
	u32 trip_temp[COOLING_DEV_MAX], trip_lowoff[COOLING_DEV_MAX];
	int i = 0;
	int j = 0;
	static int cool=1;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "could not allocate memory for platform data\n");
		return NULL;
	}
	ret = of_property_read_u32(np, "trip_points_critical", &trip_points_critical);
	if(ret){
		dev_err(dev, "fail to get trip_points_critical\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "trip_num", &trip_num);
	if(ret){
		dev_err(dev, "fail to get trip_num\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "cool_num", &cool_num);
	if(ret){
		cool=0;
	}
	for (i = 0; i < trip_num-1; i++) {
		ret = of_property_read_u32_index(np, "trip_points_active", i,
						 &trip_temp[i]);
		if(ret){
			dev_err(dev, "fail to get trip_points_active\n");
			goto fail;
		}
		ret = of_property_read_u32_index(np, "trip_points_lowoff", i,
						 &trip_lowoff[i]);
		if(ret){
			dev_err(dev, "fail to get trip_points_lowoff\n");
			goto fail;
		}
	}
	if(cool==1){
	    for (j = 0; j < cool_num; ++j){
			ret = of_property_read_string_index(np, cooling_names, j,
							  &point_arr[j]);
			if (ret) {
				printk("cooling_names: %s: missing %s in dt node\n", __func__, cooling_names);
				} else {
				printk("cooling_names: %s: %s is %s\n", __func__, cooling_names, point_arr[j]);
			}
	    }
	}
	pdata->num_trips = trip_num;
    
	for (i = 0; i < trip_num -1; ++i){
			pdata->trip_points[i].temp = trip_temp[i];
			pdata->trip_points[i].lowoff = trip_lowoff[i];
			pdata->trip_points[i].type = THERMAL_TRIP_ACTIVE;
			if(cool==1){
					for (j = 0; j < cool_num; ++j){
					memcpy(pdata->trip_points[i].cdev_name[j],
							point_arr[j], strlen(point_arr[j])+1);
					dev_info(dev,"cdev name: %s is %s\n", pdata->trip_points[i].cdev_name[j], point_arr[j]);
				}
			}
			else{
				memcpy(pdata->trip_points[i].cdev_name[0],
					"thermal-cpufreq-0", sizeof("thermal-cpufreq-0"));
				dev_info(dev, "def cdev name:is %s\n",  pdata->trip_points[i].cdev_name[0]);
				
			}
			dev_info(dev, "trip[%d] temp: %d lowoff: %d\n",
					i, trip_temp[i], trip_lowoff[i]);
				
	}
	pdata->trip_points[i].temp = trip_points_critical;
	pdata->trip_points[i].type = THERMAL_TRIP_CRITICAL;

	return pdata;

fail:
	kfree(pdata);
	return NULL;

}

#endif

#ifdef CONFIG_OF
extern int of_address_to_resource(struct device_node *dev, int index,
				  struct resource *r);
#endif

static int sprd_thermal_probe(struct platform_device *pdev)
{
	struct sprd_thermal_zone *pzone = NULL;
	struct sprd_thm_platform_data *ptrips = NULL;
	struct resource *regs;
	int thm_irq, ret = 0;
#ifdef CONFIG_OF
	struct device_node *np = pdev->dev.of_node;
#endif

	printk(KERN_INFO " sprd_thermal_probe id:%d\n", pdev->id);
#ifdef CONFIG_OF
	if (!np) {
		dev_err(&pdev->dev, "device node not found\n");
		return -EINVAL;
	}
	ptrips = thermal_detect_parse_dt(&pdev->dev);
#else
	ptrips = dev_get_platdata(&pdev->dev);
#endif
	if (!ptrips)
		return -EINVAL;
	pzone = devm_kzalloc(&pdev->dev, sizeof(*pzone), GFP_KERNEL);
	if (!pzone)
		return -ENOMEM;
	mutex_init(&pzone->th_lock);
	mutex_lock(&pzone->th_lock);
	pzone->mode = THERMAL_DEVICE_DISABLED;
	pzone->trip_tab = ptrips;
#ifdef CONFIG_OF
	ret = of_property_read_u32(np, "id", &pdev->id);
	if(ret){
		printk(KERN_INFO "sprd_thermal_probe No sensor ID \n");
		return -EINVAL;
	}
	pzone->sensor_id = pdev->id;
#else
	pzone->sensor_id = pdev->id;
#endif

	ret = sprd_thm_ops_init(pzone);
	if(ret){
		dev_err(&pdev->dev, " pzone->ops =NULL id =%d\n",pzone->sensor_id);
		return -ENODEV;
	}
	INIT_WORK(&pzone->therm_work, sprd_thermal_work);
	INIT_DELAYED_WORK(&pzone->resume_delay_work,sprd_thermal_resume_delay_work);
#ifdef CONFIG_OF
	regs = kzalloc(sizeof(*regs), GFP_KERNEL);
	ret = of_address_to_resource(np, 0, regs);
#else
	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
#endif
	if (!regs) {
		return -ENXIO;
	}
	ret = pzone->ops->get_reg_base(pzone , regs);
	if(ret<0){
		dev_err(&pdev->dev, "Failed to get_reg_base.\n");
		return ret;
	}
	printk(KERN_INFO "sprd_thermal_probe id:%d,base:0x%lx \n", pdev->id,
	       (unsigned long)pzone->reg_base);
#ifdef CONFIG_OF
	thm_irq = irq_of_parse_and_map(np, 0);
#else
	thm_irq = platform_get_irq(pdev, 0);
#endif
	if (thm_irq < 0) {
		dev_err(&pdev->dev, "Get IRQ_THM_INT failed.\n");
		return thm_irq;
	}
	ret =
	    devm_request_threaded_irq(&pdev->dev, thm_irq, NULL,
				      sprd_thm_irq_handler,
				      IRQF_NO_SUSPEND | IRQF_ONESHOT | IRQF_SHARED,
				      "sprd_thm_irq", pzone);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to allocate temp low irq.\n");
		return ret;
	}

	pzone->therm_dev =
	    thermal_zone_device_register(pzone->thermal_zone_name,
					 ptrips->num_trips, 0, pzone,
					 &thdev_ops, 0, 0, 0); 
	if (IS_ERR_OR_NULL(pzone->therm_dev)) {
		dev_err(&pdev->dev, "Register thermal zone device failed.\n");
		return PTR_ERR(pzone->therm_dev);
	}
	dev_info(&pdev->dev, "Thermal zone device registered.\n");

	pzone->ops->hw_init(pzone);
	platform_set_drvdata(pdev, pzone);
	pzone->mode = THERMAL_DEVICE_ENABLED;
	mutex_unlock(&pzone->th_lock);

#ifdef THM_TEST
	create_trip_attrs(pzone->therm_dev);	
	ret = device_create_file(&pzone->therm_dev->device, &dev_attr_debug_reg);
		if (ret)
			dev_err(&pdev->dev, "create regs debug fail\n");
#endif
	return 0;
}

static int sprd_thermal_remove(struct platform_device *pdev)
{
	struct sprd_thermal_zone *pzone = platform_get_drvdata(pdev);
#ifdef THM_TEST
	remove_trip_attrs(pzone->therm_dev);
	device_remove_file(&pzone->therm_dev->device, &dev_attr_debug_reg);
#endif
	thermal_zone_device_unregister(pzone->therm_dev);
	cancel_work_sync(&pzone->therm_work);
	mutex_destroy(&pzone->th_lock);
	return 0;
}

static int sprd_thermal_suspend(struct platform_device *pdev,
				pm_message_t state)
{
	struct sprd_thermal_zone *pzone = platform_get_drvdata(pdev);
	flush_work(&pzone->therm_work);
	flush_delayed_work(&pzone->resume_delay_work);
	pzone->ops->suspend(pzone);
	return 0;
}

static int sprd_thermal_resume(struct platform_device *pdev)
{
	struct sprd_thermal_zone *pzone = platform_get_drvdata(pdev);
	schedule_delayed_work(&pzone->resume_delay_work, (HZ * 1));
	//sprd_thm_hw_resume(pzone);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id thermal_of_match[] = {
	{ .compatible = "sprd,sprd-thermal", },
       {}
};
#endif

static struct platform_driver sprd_thermal_driver = {
	.probe = sprd_thermal_probe,
	.suspend = sprd_thermal_suspend,
	.resume = sprd_thermal_resume,
	.remove = sprd_thermal_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "sprd-thermal",
#ifdef CONFIG_OF
			.of_match_table = of_match_ptr(thermal_of_match),
#endif
		   },
};
static int __init sprd_thermal_init(void)
{
	return platform_driver_register(&sprd_thermal_driver);
}

static void __exit sprd_thermal_exit(void)
{
	platform_driver_unregister(&sprd_thermal_driver);
}

//module_init(sprd_thermal_init);
//module_exit(sprd_thermal_exit);

device_initcall_sync(sprd_thermal_init);
module_exit(sprd_thermal_exit);
// module_platform_driver(sprd_thermal_driver);
MODULE_AUTHOR("Mingwei Zhang <mingwei.zhang@spreadtrum.com>");
MODULE_DESCRIPTION("sprd thermal driver");
MODULE_LICENSE("GPL");
