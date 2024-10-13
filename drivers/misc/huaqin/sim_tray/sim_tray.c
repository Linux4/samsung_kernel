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
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

static unsigned int det_pin_no;

static int sim_tray_status_show(struct seq_file *m, void *v)
{
	seq_printf(m, "STATUS=%d\n", gpio_get_value(det_pin_no));
	return 0;
}

static int sim_tray_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, sim_tray_status_show, inode->i_private);
}

static const struct file_operations sim_tray_fops = {
	.open = sim_tray_file_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int sim_tray_det_pin(struct device *dev)
{
	struct device_node *np;

	if (!dev)
		return -ENODEV;

	np = dev->of_node;
	det_pin_no = of_get_named_gpio(np, "mediatek,sdcard-detect-pin-no", 0);

	if (!gpio_is_valid(det_pin_no)) {
		printk("Invalid GPIO, det_pin_no:%d",
			det_pin_no);
		return -EINVAL;
	}

	return 0;
}

static int det_pin_probe(struct platform_device *pdev)
{

	struct proc_dir_entry *ret;
	int rc = 0;

	pr_info("sim_tray driver probe...\n");

	ret = proc_mkdir("class", NULL);
	if(!ret) {
		pr_err("init proc/class failed!!!\n");
		return -ENOMEM;
	}

	ret = proc_mkdir("sd-hotplug", ret);
	if(!ret) {
		pr_err("init proc/class/sd-hotplug failed!!!\n");
		return -ENOMEM;
	}

	ret = proc_mkdir("status", ret);
	if(!ret) {
		pr_err("init proc/class/sd-hotplug/status failed!!!\n");
		return -ENOMEM;
	}

	ret = proc_create("hot-status", 0444, ret, &sim_tray_fops);

	if (pdev->dev.of_node) {
		rc = sim_tray_det_pin(&pdev->dev);
		if (rc < 0)
			return -EINVAL;
	}

	return 0;
}

static int det_pin_remove(struct platform_device *pdev)
{

	pr_info("sim_tray driver remove...\n");


	remove_proc_entry("class/sd-hotplug/status/hot-status",NULL);

	return 0;
}


static const struct of_device_id msm_detpin_dt_match[] = {
	{.compatible = "mediatek,sdcard-detect-pin"},
	{},
};
MODULE_DEVICE_TABLE(of, msm_detpin_dt_match);

static struct platform_driver sim_tray_driver = {
	.driver = {
		.name = "det-pin",
		.owner = THIS_MODULE,
		.of_match_table = msm_detpin_dt_match,
	},
	.probe = det_pin_probe,
	.remove = det_pin_remove,
};

static int __init sim_tray_init(void)
{
	int rc;

	pr_info("sim_tray driver init...\n");
	rc = platform_driver_register(&sim_tray_driver);
	if (rc) {
		pr_err("%s: Failed to register sim stray driver\n",
				__func__);
		return rc;
	}

	return 0;
}

static void __exit sim_tray_exit(void)
{
	pr_info("sim_tray driver exit...\n");
	platform_driver_unregister(&sim_tray_driver);
}

module_init(sim_tray_init);
module_exit(sim_tray_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Sim stray driver");
