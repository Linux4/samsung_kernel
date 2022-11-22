#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

#if defined(CONFIG_MACH_LT02)
struct class *camera_class;
EXPORT_SYMBOL(camera_class);
#endif 

static int __init rhea_class_create(void)
{
	sec_class = class_create(THIS_MODULE, "sec");
	if (IS_ERR(sec_class)) {
		pr_err("Failed to create class(sec)!\n");
		return PTR_ERR(sec_class);
	}

	return 0;
}

#if defined(CONFIG_MACH_LT02)
int __init camera_class_init(void)
{
	printk("zc class_creat camera\r\n");
	camera_class = class_create(THIS_MODULE, "camera");
	if (IS_ERR(camera_class))
		pr_err("Failed to create class(camera)!\n");

	return 0;
}
subsys_initcall(camera_class_init);
#endif 

subsys_initcall(rhea_class_create);

