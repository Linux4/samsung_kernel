#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
/* TabA7 Lite code for P210218-01007 by wangweiran at 20210223 start */
#include <linux/clk.h>

#define GPIO_BASE 329
#define GPIO_NUM (GPIO_BASE + 159)

struct grdm_driver_data {
	int grdm_is_open;
	struct clk *grdm_clk;
};

static struct grdm_driver_data grdm_data = {
	.grdm_is_open = 0,
	.grdm_clk = NULL
};
/* TabA7 Lite code for P210218-01007 by wangweiran at 20210223 end */

static ssize_t show_grdm_node(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	pr_info("grdm_node_show, gpio159 is %d",
					gpio_get_value(GPIO_NUM));

	if (grdm_data.grdm_is_open) {
		return sprintf(buf, "%s\n", "opened");
	} else {
		return sprintf(buf, "%s\n", "closed");
	}
}

/* TabA7 Lite code for P210218-01007 by wangweiran at 20210223 start */
static ssize_t store_grdm_node(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
	int ret = 0;
	pr_info("%s open flag = %d\n",
			__func__, grdm_data.grdm_is_open);
	if (len <= 0) {
		return -1;
	}

	if (buf[0] == '1' && grdm_data.grdm_is_open == 0) {
		pr_err("%s open\n", __func__);
		if (IS_ERR(grdm_data.grdm_clk)) {
			ret = PTR_ERR(grdm_data.grdm_clk);
			pr_err("grdm failed to get grdm_clk, ret is %d\n", ret);
			return -1;
		}
		clk_prepare_enable(grdm_data.grdm_clk);
		gpio_direction_output(GPIO_NUM, 1);
		grdm_data.grdm_is_open = 1;
	} else if (buf[0] == '0' && grdm_data.grdm_is_open == 1) {
		pr_err("%s close\n", __func__);
		if (IS_ERR(grdm_data.grdm_clk)) {
			ret = PTR_ERR(grdm_data.grdm_clk);
			pr_err("grdm failed to get grdm_clk, ret is %d\n", ret);
			return -1;
		}
		clk_disable_unprepare(grdm_data.grdm_clk);
		gpio_direction_output(GPIO_NUM, 0);
		grdm_data.grdm_is_open = 0;
	} else {
		pr_err("%s buf is %s, len is %d, grdm_is_open is %d\n",
						__func__, buf, len, grdm_data.grdm_is_open);
		return -1;
	}
	/* TabA7 Lite code for P210218-01007 by wangweiran at 20210223 start */

	return len;
}

static DEVICE_ATTR(grdm_node_gpio, S_IWUSR|S_IRUSR,
				show_grdm_node, store_grdm_node);

static int grdm_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	pr_info("%s", __func__);

	if (grdm_data.grdm_is_open == 1) {
		pr_err("grdm is opened yet");
		return -EBUSY;
	}

	if (IS_ERR(grdm_data.grdm_clk)) {
		ret = PTR_ERR(grdm_data.grdm_clk);
		pr_err("grdm failed to get grdm_clk, ret is %d\n", ret);
		return ret;
	}

	clk_prepare_enable(grdm_data.grdm_clk);
	gpio_direction_output(GPIO_NUM, 1);
	grdm_data.grdm_is_open = 1;

	return 0;
}

static int grdm_release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	pr_info("%s", __func__);

	if (grdm_data.grdm_is_open == 0) {
		pr_err("grdm is closed yet");
		return -EPERM;
	}

	if (IS_ERR(grdm_data.grdm_clk)) {
		ret = PTR_ERR(grdm_data.grdm_clk);
		pr_err("grdm failed to get grdm_clk, ret is %d\n", ret);
		return ret;
	}

	clk_disable_unprepare(grdm_data.grdm_clk);
	gpio_direction_output(GPIO_NUM, 0);
	grdm_data.grdm_is_open = 0;

	return 0;
}
/* TabA7 Lite code for P210218-01007 by wangweiran at 20210223 end */

static ssize_t grdm_write(struct file *file, const char __user *buf,
						size_t count, loff_t *ppos)
{
	char p[1] = {0x00};

	pr_info("%s", __func__);
	if (count == 0 || buf == NULL) {
		return -1;
	}

	if (copy_from_user(p, buf, 1)) {
		return -1;
	}

	/* TabA7 Lite code for P210218-01007 by wangweiran at 20210223 start */
	if (p[0] == '0') {
		gpio_direction_output(GPIO_NUM, 0);
		grdm_data.grdm_is_open = 0;
	} else {
		gpio_direction_output(GPIO_NUM, 1);
		grdm_data.grdm_is_open = 1;
	}
	/* TabA7 Lite code for P210218-01007 by wangweiran at 20210223 end */

	return count;
}

static ssize_t grdm_read(struct file *file, char __user *buf,
						size_t count, loff_t *ppos)
{
	pr_info("%s", __func__);
	if (count == 0 || buf == NULL) {
		return -1;
	}

/* TabA7 Lite code for P210218-01007 by wangweiran at 20210223 start */
	if (grdm_data.grdm_is_open == 1) {
		copy_to_user(buf, "1", 1);
	} else {
		copy_to_user(buf, "0", 1);
	}
/* TabA7 Lite code for P210218-01007 by wangweiran at 20210223 end */

	return count;
}

struct file_operations grdm_node_ops = {
	.owner  = THIS_MODULE,
	.open = grdm_open,
	.release = grdm_release,
	.read = grdm_read,
	.write = grdm_write,
};

static int major;
static struct class *grdm_node_class;

static const struct of_device_id grdm_match_table[] = {
	{ .compatible = "sec,guardianm",},
	{},
};

static int grdm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device* grdm_node_device;
	pr_info("%s\n", __func__);

	major = register_chrdev(0, "grdm_node", &grdm_node_ops);
	grdm_node_class = class_create(THIS_MODULE, "grdm_node_class");
	grdm_node_device = device_create(grdm_node_class, 0,
							MKDEV(major, 0), NULL,"grdm");

	if (sysfs_create_file(&(grdm_node_device->kobj),
					&dev_attr_grdm_node_gpio.attr)) {
		return -1;
	}

	/* TabA7 Lite code for P210218-01007 by wangweiran at 20210223 start */
	ret = gpio_request(GPIO_NUM, "grdm_pvdd_en");
	if (ret != 0) {
		pr_warn("failed to request grdm gpio!\n");
	}

	grdm_data.grdm_clk = devm_clk_get(&pdev->dev, "grdm-clk");
	if (IS_ERR(grdm_data.grdm_clk)) {
		ret = PTR_ERR(grdm_data.grdm_clk);
		pr_err("failed to get grdm_clk, ret is %d\n", ret);
		return ret;
	}
	/* TabA7 Lite code for P210218-01007 by wangweiran at 20210223 end */

	return 0;
}

static int grdm_remove(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	device_destroy(grdm_node_class, MKDEV(major, 0));
	class_destroy(grdm_node_class);
	unregister_chrdev(major, "grdm_node");
	gpio_direction_output(GPIO_NUM, 0);
	grdm_data.grdm_is_open = 0;

	return 0;
}

static struct platform_driver grdm_driver = {
	.driver = {
		.name = "grdm",
		.owner = THIS_MODULE,
		.of_match_table = grdm_match_table,
	},
	.probe =  grdm_probe,
	.remove = grdm_remove,
};

static int __init grdm_init(void)
{
	pr_info("%s\n", __func__);
	return platform_driver_register(&grdm_driver);
}

static void __exit grdm_exit(void)
{
	pr_info("%s\n", __func__);
}

module_init(grdm_init);
module_exit(grdm_exit);
MODULE_LICENSE("GPL");