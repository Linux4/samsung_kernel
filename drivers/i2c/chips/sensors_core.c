
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
/*#include <linux/device.h>*/
#include <linux/fs.h>
#include <linux/err.h>
/*#include <linux/sensors_core.h>*/
#include "sensors_core.h"

#define DEBUG_HAN 1

/*struct class *sensors_class;*/
static atomic_t sensor_count;
static DEFINE_MUTEX(sensors_mutex);

/**
 * Create sysfs interface
 */
static int set_sensor_attr(struct device *dev,
	struct device_attribute *attributes[])
{
	int i;

	for (i = 0 ; attributes[i] != NULL ; i++) {
		if ((device_create_file(dev, attributes[i])) < 0) {
			pr_err("Create_dev_file fail(attributes[%d] )\n", i);
			break;
		}
	}

	if (attributes[i] == NULL)
		return 0;

	while (i-- > 0)
		device_remove_file(dev, attributes[i]);

	return -1;
}

int sensors_register(struct device **dev, void * drvdata,
	struct device_attribute *attributes[], char *name)
{
	int ret = 0;
#ifdef DEBUG_HAN
	pr_info("Thanks to into sensors_register\n");
#endif

	if (!sensors_class) {
		sensors_class = class_create(THIS_MODULE, "sensors");
		if (IS_ERR(sensors_class))
			return PTR_ERR(sensors_class);
	}
#ifdef DEBUG_HAN
	pr_info("The Class is created\n");
#endif
	mutex_lock(&sensors_mutex);

	*dev = device_create(sensors_class, NULL, 0, drvdata, "%s", name);

	if (IS_ERR(*dev)) {
		ret = PTR_ERR(*dev);
		pr_err("[SENSORS CORE] device_create failed [%d]\n", ret);
		return ret;
	}

	ret = set_sensor_attr(*dev, attributes);

	atomic_inc(&sensor_count);

	mutex_unlock(&sensors_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(sensors_register);

void sensors_unregister(struct device *dev,
	struct device_attribute *attributes[])
{
	int i;

	if (sensors_class != NULL) {
		for (i = 0 ; attributes[i] != NULL ; i++)
			device_remove_file(dev, attributes[i]);
	}
}

static int __init sensors_class_init(void)
{
	pr_info("[SENSORS CORE] sensors_class_init\n");
	sensors_class = class_create(THIS_MODULE, "sensors");

	if (IS_ERR(sensors_class))
		return PTR_ERR(sensors_class);

	atomic_set(&sensor_count, 0);
	sensors_class->dev_uevent = NULL;

	return 0;
}

static void __exit sensors_class_exit(void)
{
	class_destroy(sensors_class);
}

subsys_initcall(sensors_class_init);
module_exit(sensors_class_exit);

MODULE_DESCRIPTION("Universal sensors core class");
MODULE_AUTHOR("Ryunkyun Park <ryun.park@samsung.com>");
MODULE_LICENSE("GPL");
