#ifndef __SHUB_MAGNETOMETER_FACTORY_H_
#define __SHUB_MAGNETOMETER_FACTORY_H_

#include <linux/device.h>
#include <linux/types.h>

struct device_attribute **get_magnetometer_ak09918c_dev_attrs(char *name);
int check_ak09918c_adc_data_spec(int type);

struct device_attribute **get_magnetometer_mmc5633_dev_attrs(char *name);
int check_mmc5633_adc_data_spec(int type);

struct device_attribute **get_magnetometer_yas539_dev_attrs(char *name);
int check_yas539_adc_data_spec(int type);

#endif
