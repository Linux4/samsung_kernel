
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/err.h>
#include "sensors_core.h"

struct class *sensors_class;
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

struct device *sensors_classdev_register(char *sensors_name)
{
	struct device *dev;
	int retval = -ENODEV;

	dev = device_create(sensors_class, NULL, 0,
					NULL, "%s", sensors_name);
	if (IS_ERR(dev))
		return ERR_PTR(retval);

	printk(KERN_INFO "Registered sensors device: %s\n", sensors_name);
	return dev;
}
EXPORT_SYMBOL_GPL(sensors_classdev_register);

/**
* sensors_classdev_unregister - unregisters a object of sensor device.
*
*/
void sensors_classdev_unregister(struct device *dev)
{
	device_unregister(dev);
}
EXPORT_SYMBOL_GPL(sensors_classdev_unregister);

int sensors_register(struct device **dev, void * drvdata,
	struct device_attribute *attributes[], char *name)
{
	int ret = 0;


	if (!sensors_class) {
		sensors_class = class_create(THIS_MODULE, "sensors");
		if (IS_ERR(sensors_class))
			return PTR_ERR(sensors_class);
	}


	*dev = device_create(sensors_class, NULL, 0, drvdata, "%s", name);

	if (IS_ERR(*dev)) {
		ret = PTR_ERR(*dev);
		pr_err("[SENSORS CORE] device_create failed [%d]\n", ret);
		return ret;
	}

	ret = set_sensor_attr(*dev, attributes);

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
	sensors_class = class_create(THIS_MODULE, "sensors");

	if (IS_ERR(sensors_class))
		return PTR_ERR(sensors_class);

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
