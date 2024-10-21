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

static struct proc_dir_entry *wt_em_node;

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
			pr_err("wt_em: failed to get bootargs from dts.\n");
			return false;
		}

		str = strstr(bootparams, "wtcmdline.em_status=");
		if (str) {
			str += strlen("wtcmdline.em_status=");
			ret = get_option(&str, value);
			if (1 == ret) {
				pr_err("wt_em: wtcmdline.em_status=%d.\n", *value);
				return true;
			}
		}
	}

	pr_err("wt_em: parser paras return error, ret=%d.\n", ret);
	return false;
}

static int wt_em_proc_show(struct seq_file *m, void *v)
{
	bool ret = false;
	int wt_em_status = -1;

	ret = parser_paras_from_cmdline(&wt_em_status);
	if((wt_em_status == -1) || (ret != 1)) {
		pr_err("wt_em: wt_em status get error, wt_em_value=%d, ret=%d.\n", wt_em_status, ret);
		return 0;
	}

	seq_printf(m, "%d\n",wt_em_status);
	return 0;
}

static int wt_em_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, wt_em_proc_show, NULL);
}

static const struct proc_ops  wt_em_proc_fops = {
	.proc_open		= wt_em_proc_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release,
};

static int __init wt_em_status_init(void)
{
	int ret;
	wt_em_node = proc_create("wt_em", 0, NULL, &wt_em_proc_fops);
	ret = IS_ERR_OR_NULL(wt_em_node);
	if (ret) {
		pr_err("wt_em: failed to create proc entry.\n");
		return ret;
	}
	pr_notice("wt_em: creante node.\n");

	return 0;
}

static void __exit wt_em_status_exit(void)
{
	if (wt_em_node) {
		pr_notice("wt_em: remove node.\n");
		proc_remove(wt_em_node);
	}
}

module_init(wt_em_status_init);
module_exit(wt_em_status_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("WingTech wt em Driver");
MODULE_AUTHOR("WingTech Inc.");
