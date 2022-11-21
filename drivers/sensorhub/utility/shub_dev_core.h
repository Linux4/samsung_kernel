#ifndef _SHUB_DEV_CORE_H__
#define _SHUB_DEV_CORE_H__

#include <linux/device.h>

int add_sensor_device_attr(struct device *dev, struct device_attribute *attributes[]);
int add_sensor_bin_attr(struct device *dev, struct bin_attribute *attributes[]);
void remove_sensor_device_attr(struct device *dev, struct device_attribute *attributes[]);
void remove_sensor_bin_attr(struct device *dev, struct bin_attribute *attributes[]);
int sensor_device_create(struct device **pdev, void *drvdata, char *name);
void sensor_device_destroy(struct device *dev);

#endif