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
#include <linux/string.h>

static struct proc_dir_entry *serialnum_node;

static bool parser_paras_from_cmdline(char *value)
{
	int ret = 0;
	char *str;
	const char *bootparams = NULL;
	struct device_node *node;

	node = of_find_node_by_path("/chosen");
	if(node) {
		ret = of_property_read_string(node, "bootargs", &bootparams);
		if ((!bootparams) || (ret < 0)) {
			pr_err("serialnum: failed to get bootargs from dts.\n");
			return false;
		}

		str = strstr(bootparams, "wtcmdline.cpuid_info=");
		if (str) {
			str += strlen("wtcmdline.cpuid_info=");
			strncpy(value , str, 40);
			if ( 40 == strlen(value) ) {
				pr_err("serialnum: wtcmdline.cpuid_info=%s.\n", value);
				return true;
			}
		}
	}

	pr_err("serialnum: parser paras return error, ret=%d.\n", ret);
	return false;
}

static int serialnum_proc_show(struct seq_file *m, void *v)
{
	bool ret = false;
	char serial_num[64] = {0};

	ret = parser_paras_from_cmdline(serial_num);
	if( ret != 1 ) {
		pr_err("serialnum: serial num get error, ret=%d.\n", ret);
		return -1;
	}

	seq_printf(m, "0x%s\n",serial_num);
	return 0;
}

static int serialnum_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, serialnum_proc_show, NULL);
}

static const struct proc_ops  serialnum_proc_fops = {
	.proc_open		= serialnum_proc_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release,
};

static int __init serialnum_status_init(void)
{
	int ret;
	serialnum_node = proc_create("serial_num", 0, NULL, &serialnum_proc_fops);
	ret = IS_ERR_OR_NULL(serialnum_node);
	if (ret) {
		pr_err("serialnum: failed to create proc entry.\n");
		return ret;
	}
	pr_notice("serialnum: creante node.\n");

	return 0;
}

static void __exit serialnum_status_exit(void)
{
	if (serialnum_node) {
		pr_notice("serialnum: remove node.\n");
		proc_remove(serialnum_node);
	}
}

module_init(serialnum_status_init);
module_exit(serialnum_status_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("WingTech serialnum Driver");
MODULE_AUTHOR("WingTech Inc.");
