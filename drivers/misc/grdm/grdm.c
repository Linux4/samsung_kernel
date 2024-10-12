#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>

#define GPIO_BASE 329
#define GPIO_NUM (GPIO_BASE + 159)

struct grdm_driver_data {
	int grdm_is_open;
    struct regulator *grdm_ldo;
	struct clk *grdm_clk;
};

static struct grdm_driver_data grdm_data = {
	.grdm_is_open = 0,
	.grdm_clk = NULL
};

static ssize_t show_grdm_node(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	// pr_info("grdm_node_show, gpio159 is %d",
	// 				gpio_get_value(GPIO_NUM));

	if (grdm_data.grdm_is_open) {
		return sprintf(buf, "%s\n", "opened");
	} else {
		return sprintf(buf, "%s\n", "closed");
	}
}

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
        if (IS_ERR(grdm_data.grdm_ldo)) {
            ret = PTR_ERR(grdm_data.grdm_ldo);
            pr_err("failed to get grdm_ldo, ret is %d\n", ret);
            return ret;
        }
        regulator_enable(grdm_data.grdm_ldo);
		grdm_data.grdm_is_open = 1;
	} else if (buf[0] == '0' && grdm_data.grdm_is_open == 1) {
        if (IS_ERR(grdm_data.grdm_ldo)) {
            ret = PTR_ERR(grdm_data.grdm_ldo);
            pr_err("failed to get grdm_ldo, ret is %d\n", ret);
            return ret;
        }
        regulator_disable(grdm_data.grdm_ldo);
		grdm_data.grdm_is_open = 0;
	} else {
		pr_err("%s buf is %s, len is %d, grdm_is_open is %d\n",
						__func__, buf, len, grdm_data.grdm_is_open);
		return -1;
	}

	return len;
}

static DEVICE_ATTR(grdm_node_gpio, S_IWUSR|S_IRUSR,
				show_grdm_node, store_grdm_node);

static int grdm_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	pr_info("%s", __func__);

	if (IS_ERR(grdm_data.grdm_ldo)) {
		ret = PTR_ERR(grdm_data.grdm_ldo);
		pr_err("failed to get grdm_ldo, ret is %d\n", ret);
		return ret;
	}
// Tab A8 code for SR-AX6300-01-207 by zhumengnan at 20210918 start
	else
	{
		regulator_enable(grdm_data.grdm_ldo);
		grdm_data.grdm_is_open = 1;
	}
// Tab A8 code for SR-AX6300-01-207 by zhumengnan at 20210918 end
	return 0;
}


static int grdm_release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	pr_info("%s", __func__);


	if (IS_ERR(grdm_data.grdm_ldo)) {
		ret = PTR_ERR(grdm_data.grdm_ldo);
		pr_err("failed to get grdm_ldo, ret is %d\n", ret);
		return ret;
	}
// Tab A8 code for SR-AX6300-01-207 by zhumengnan at 20210918 start
	else
	{
		regulator_disable(grdm_data.grdm_ldo);
		grdm_data.grdm_is_open = 0;
	}
// Tab A8 code for SR-AX6300-01-207 by zhumengnan at 20210918 end
	return 0;
}

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

	if (p[0] == '0') {
        if (grdm_data.grdm_is_open == 0) {
            pr_err("grdm is closed yet");
            return -EPERM;
        }
		regulator_disable(grdm_data.grdm_ldo);
		grdm_data.grdm_is_open = 0;
	} else {
        if (grdm_data.grdm_is_open == 1) {
            pr_err("grdm is opened yet");
            return -EPERM;
        }
		regulator_enable(grdm_data.grdm_ldo);
		grdm_data.grdm_is_open = 1;
	}

	return count;
}

static ssize_t grdm_read(struct file *file, char __user *buf,
						size_t count, loff_t *ppos)
{
	pr_info("%s", __func__);
	if (count == 0 || buf == NULL) {
		return -1;
	}

	if (grdm_data.grdm_is_open == 1) {
		copy_to_user(buf, "1", 1);
	} else {
		copy_to_user(buf, "0", 1);
	}

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
	if (major < 0){
		pr_err("fail to register grdm chrdev,ret is %d\n", major);
		ret = major;
		goto err_register_chr;
	}

	grdm_node_class = class_create(THIS_MODULE, "grdm_node_class");
	if (IS_ERR(grdm_node_class)) {
		pr_err("fail to create grdm class,ret is %d\n",PTR_ERR(grdm_node_class));
		ret = PTR_ERR(grdm_node_class);
		goto err_class_create;
	}

	grdm_node_device = device_create(grdm_node_class, 0,
							MKDEV(major, 0), NULL,"grdm");
	if (IS_ERR(grdm_node_device)) {
		pr_err("fail to create grdm device,ret is %d\n",PTR_ERR(grdm_node_device));
		ret = PTR_ERR(grdm_node_device);
		goto err_device_create;
	}

	ret = sysfs_create_file(&(grdm_node_device->kobj),
					&dev_attr_grdm_node_gpio.attr);
	if (ret) {
		pr_err("fail to create grdm sys file, ret is %d\n", ret);
		goto err_create_sysfs;
	}

	grdm_data.grdm_ldo = regulator_get(&pdev->dev, "vddldo1");
	if (IS_ERR(grdm_data.grdm_ldo)) {
		ret = PTR_ERR(grdm_data.grdm_ldo);
		pr_err("failed to get grdm_ldo, ret is %d\n", ret);
		goto err_get_regulator;
	}

	return 0;

err_get_regulator:
    sysfs_remove_file(&(grdm_node_device->kobj),
					&dev_attr_grdm_node_gpio.attr);
err_create_sysfs:
	device_destroy(grdm_node_class, MKDEV(major, 0));
err_device_create:
    class_destroy(grdm_node_class);
err_class_create:
	unregister_chrdev(major, "grdm_node");
err_register_chr:
	return ret;
}

static int grdm_remove(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	device_destroy(grdm_node_class, MKDEV(major, 0));
	class_destroy(grdm_node_class);
	unregister_chrdev(major, "grdm_node");
	regulator_disable(grdm_data.grdm_ldo);
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

late_initcall(grdm_init);
module_exit(grdm_exit);
MODULE_LICENSE("GPL");