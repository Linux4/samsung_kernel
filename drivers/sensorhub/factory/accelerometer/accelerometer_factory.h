#ifndef __SHUB_ACCELEROMETER_FACTORY_H_
#define __SHUB_ACCELEROMETER_FACTORY_H_

#include <linux/device.h>
#include <linux/types.h>

struct device_attribute **get_accelerometer_icm42605m_dev_attrs(char *name);
struct device_attribute **get_accelerometer_lsm6dsl_dev_attrs(char *name);
struct device_attribute **get_accelerometer_lis2dlc12_dev_attrs(char *name);
struct device_attribute **get_accelerometer_lsm6dsotr_dev_attrs(char *name);
#endif
