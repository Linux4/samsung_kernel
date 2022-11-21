#ifndef ___SENSORS_CORE_H_INCLUDED
#define ___SENSORS_CORE_H_INCLUDED
#include <linux/device.h>

struct class *sensors_class;
EXPORT_SYMBOL_GPL(sensors_class);

int sensors_register(struct device **dev, void *drvdata,
	struct device_attribute *attributes[],
	char *name);
void sensors_unregister(struct device *dev,
	struct device_attribute *attributes[]);

#endif
