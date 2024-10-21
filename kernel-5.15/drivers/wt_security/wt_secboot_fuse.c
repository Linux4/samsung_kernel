#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/moduleparam.h>

static struct proc_dir_entry *secureboot_node;

static bool parser_paras_from_cmdline(int *value)
{
	int ret = 0;
	struct device_node *node;
	const char *bootparams = NULL;
	char *str;

	node = of_find_node_by_path("/chosen");
	if(node) {
		ret = of_property_read_string(node, "bootargs", &bootparams);
		if ((!bootparams) || (ret < 0)) {
			pr_err("secureboot: failed to get bootargs from dts.\n");
			return false;
		}

		str = strstr(bootparams, "wtcmdline.efuse_status=");
		if (str) {
			str += strlen("wtcmdline.efuse_status=");
			ret = get_option(&str, value);
			if (1 == ret) {
				pr_err("secureboot: wtcmdline.efuse_status=%d.\n", *value);
				return true;
			}
		}
	}

	pr_err("secureboot: parser paras return error, ret=%d.\n", ret);
	return false;
}

static int secboot_proc_show(struct seq_file *m, void *v)
{
	bool ret = false;
	int efuse_status = -1;

	ret = parser_paras_from_cmdline(&efuse_status);
	if((efuse_status == -1) || (ret != 1)) {
		pr_err("secureboot: efuse value get error, efuse_value=%d, ret=%d.\n", efuse_status, ret);
		return 0;
	}

	seq_printf(m, "0x%x\n",efuse_status);
	return 0;
}

static int secboot_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, secboot_proc_show, NULL);
}

static const struct proc_ops  secboot_proc_fops = {
	.proc_open		= secboot_proc_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release,
};

static int __init secureboot_status_init(void)
{
	int ret;
	secureboot_node = proc_create("secboot_fuse_reg", 0, NULL, &secboot_proc_fops);
	ret = IS_ERR_OR_NULL(secureboot_node);
	if (ret) {
		pr_err("secureboot: failed to create proc entry.\n");
		return ret;
	}
	pr_notice("secureboot: creante node.\n");

	return 0;
}

static void __exit secureboot_status_exit(void)
{
	if (secureboot_node) {
		pr_notice("secureboot: remove node.\n");
		proc_remove(secureboot_node);
	}
}

module_init(secureboot_status_init);
module_exit(secureboot_status_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("WingTech SecureBoot Driver");
MODULE_AUTHOR("WingTech Inc.");
