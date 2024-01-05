#include <linux/device.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/string.h>

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

static int __init sec_class_create(void)
{
	sec_class = class_create(THIS_MODULE, "sec");
	if (IS_ERR(sec_class)) {
		pr_err("Failed to create class(sec)!\n");
		return PTR_ERR(sec_class);
	}

	return 0;
}

static int sec_class_match_device_by_name(struct device *dev, void *data)
{
	const char *name = data;

	return (0 == strcmp(dev->kobj.name, name));
}

struct device *sec_dev_get_by_name(char *name)
{
	return class_find_device(sec_class, NULL, name,
			sec_class_match_device_by_name);
}
EXPORT_SYMBOL(sec_dev_get_by_name);

subsys_initcall(sec_class_create);
