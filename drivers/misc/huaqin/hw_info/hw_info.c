/*
 * file create by liufurong for show tp module info, 2019/11/23
 *
 */

#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <asm/uaccess.h>
#include <linux/printk.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/hw_info.h>

struct platform_device hw_info_device= {
	.name = "hw_info",
	.id = -1,
};

static int __init hw_info_init(void)
{
	int rc = 0;
	rc = platform_device_register(&hw_info_device);
	if (rc) {
		pr_err("%s: Failed to register hw_info_device\n",__func__);
		return rc;
	}

	return 0;
}

static void __exit hw_info_exit(void)
{
	pr_info("hw_info_device exit...\n");
	platform_device_unregister(&hw_info_device);
}
static char tp_module_name[32] = "unkonwn_tp_module_name";
void set_tp_module_name(char* tp_info)
{
	if(NULL != tp_info)
		strcpy(tp_module_name,tp_info);
}
module_param_string(tp_module_name, tp_module_name,sizeof(tp_module_name), 0644);

EXPORT_SYMBOL_GPL(set_tp_module_name);

module_init(hw_info_init);
module_exit(hw_info_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("hw info device");
