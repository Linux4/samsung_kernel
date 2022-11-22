#ifndef __SHUB_PROXIMITY_FACTORY_H_
#define __SHUB_PROXIMITY_FACTORY_H_

#include <linux/device.h>
#include <linux/types.h>

struct device_attribute **get_proximity_gp2ap110s_dev_attrs(char *name);
struct device_attribute **get_proximity_stk3x6x_dev_attrs(char *name);
struct device_attribute **get_proximity_stk3328_dev_attrs(char *name);
struct device_attribute **get_proximity_tmd4912_dev_attrs(char *name);

#endif
