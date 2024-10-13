#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/sort.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/soc/qcom/smem.h>
#include "sensor_node.h"

static void *smem_info_addr = NULL;
static sensor_info *sensor_info_addr = NULL;
static struct class *sensor_class = NULL;
static struct device *light_device_class = NULL;
static struct device *prox_device_class = NULL;

#define print_log(fmt, ...) printk(KERN_ERR "LC_Sensor" fmt, ##__VA_ARGS__)

static ssize_t light_rawdata_show(struct device *dev,struct device_attribute *attr,char *buf)
{
	int ret;
	if(sensor_info_addr == NULL)
	{
		print_log("sensor_info_addr is NULL\n");
		return -1;
	}
	ret = sprintf(buf, "%d %d\n", sensor_info_addr->light.raw_ch0, sensor_info_addr->light.raw_ch1);
	return ret;
}
static ssize_t prox_rawdata_show(struct device *dev,struct device_attribute *attr,char *buf)
{
	int ret;
	if(sensor_info_addr == NULL)
	{
		print_log("sensor_info_addr is NULL\n");
		return -1;
	}
	ret = sprintf(buf, "%d %d\n", sensor_info_addr->prox.raw_data, sensor_info_addr->prox.poffset);
	return ret;
}

static DEVICE_ATTR_RO(light_rawdata);
static DEVICE_ATTR_RO(prox_rawdata);

static struct attribute *light_sensor_attrs[] = {
	&dev_attr_light_rawdata.attr,
	NULL,
};
static struct attribute *prox_sensor_attrs[] = {
	&dev_attr_prox_rawdata.attr,
	NULL,
};
static const struct attribute_group light_sensor_attr_group = {
	.attrs	= light_sensor_attrs,
};
static const struct attribute_group *light_sensor_attr_groups[] = {
	&light_sensor_attr_group,
	NULL,
};
static const struct attribute_group prox_sensor_attr_group = {
	.attrs	= prox_sensor_attrs,
};
static const struct attribute_group *prox_sensor_attr_groups[] = {
	&prox_sensor_attr_group,
	NULL,
};

static int lc_sensor_probe()
{
	int rc = 0;
	ssize_t size_tmp = 0;
	rc = qcom_smem_alloc(QCOM_SMEM_HOST_ANY, SMEM_SENSOR_INFO, sizeof(sensor_info));
	if(rc < 0 && rc != -EEXIST)
	{
		print_log("qcom_smem_alloc fail\n");
		return rc;
	}
	smem_info_addr = qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_SENSOR_INFO, &size_tmp);
	if(smem_info_addr == NULL || size_tmp != sizeof(sensor_info))
	{
		print_log("smem_info_addr is NULL\n");
		return -EEXIST;
	}
	memset(smem_info_addr, 0, sizeof(sensor_info));
	sensor_info_addr = (sensor_info *)smem_info_addr;
	sensor_class = class_create(THIS_MODULE, "sensor_info");
	if (!sensor_class)
	{
		print_log("Failed to class_create\n");
		return -EEXIST;
	}
	light_device_class = kzalloc(sizeof(struct device), GFP_KERNEL);
	prox_device_class = kzalloc(sizeof(struct device), GFP_KERNEL);
	if(!light_device_class || !prox_device_class)
	{
		print_log("Failed to device alloc fail\n");
		return -EEXIST;
	}
	//light
	light_device_class->class = sensor_class;
	light_device_class->groups = light_sensor_attr_groups;
	dev_set_name(light_device_class, "light_sensor");	
	rc = device_register(light_device_class);
	if(rc)
	{
		print_log("Failed to light device_register\n");
		goto init_fail;
	}
	//prox
	prox_device_class->class = sensor_class;
	prox_device_class->groups = prox_sensor_attr_groups;
	dev_set_name(prox_device_class, "prox_sensor");
	rc = device_register(prox_device_class);
	if(rc)
	{
		print_log("Failed to prox device_register\n");
		goto init_fail;
	}
	print_log("lc sensor probe success\n");
	return rc;
init_fail:
	device_unregister(light_device_class);
	device_unregister(prox_device_class);
        return -EEXIST;
}

static int __init lc_sensor_init(void)
{
	return lc_sensor_probe();
}
 
static void __exit lc_sensor_exit(void)
{
	class_unregister(sensor_class);
}

module_init(lc_sensor_init);
module_exit(lc_sensor_exit);

MODULE_AUTHOR("Lc-Sensor");
MODULE_DESCRIPTION("Qcom Ap Sensor get/send info from/to ADSP");
MODULE_LICENSE("GPL");
MODULE_VERSION("1");
