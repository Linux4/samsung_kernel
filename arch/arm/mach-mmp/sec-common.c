#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

struct class *camera_class;
EXPORT_SYMBOL(camera_class);

u32 set_default_param;
EXPORT_SYMBOL(set_default_param);

static __init int setup_default_param(char *str)
{
	if (get_option(&str, &set_default_param) != 1)
		set_default_param = 0;

	return 0;
}

__setup("set_default_param=", setup_default_param);

void (*sec_set_param_value) (int idx, void *value) = NULL;
EXPORT_SYMBOL(sec_set_param_value);

void (*sec_get_param_value) (int idx, void *value) = NULL;
EXPORT_SYMBOL(sec_get_param_value);

static int __init rhea_class_create(void)
{
	sec_class = class_create(THIS_MODULE, "sec");
	if (IS_ERR(sec_class)) {
		pr_err("Failed to create class(sec)!\n");
		return PTR_ERR(sec_class);
	}

	return 0;
}

int __init camera_class_init(void)
{
	camera_class = class_create(THIS_MODULE, "camera");
	if (IS_ERR(camera_class))
		pr_err("Failed to create class(camera)!\n");

	return 0;
}

subsys_initcall(rhea_class_create);
subsys_initcall(camera_class_init);

