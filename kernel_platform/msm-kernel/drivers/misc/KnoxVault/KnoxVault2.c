#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>

#define SECURITY_IC_POWER_SUPPLY_VIA_GPIO   // gpio control power supply
// #define SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK

#define GPIO_BASE 329
#define GPIO_NUM (GPIO_BASE + 36)

enum k250a_status {
	K250A_CLOSE = 0,
	K250A_OPEN = 1,
};

enum gpio_ctrl_power {
	GPIO_CTRL_POWER_CLOSE = 0,
	GPIO_CTRL_POWER_OPEN = 1,
};

struct k250a_driver_data {
	int k250a_is_open;
	struct regulator *k250a_ldo;
	struct clk *k250a_clk;
};

static struct k250a_driver_data k250a_data = {
	.k250a_is_open = K250A_CLOSE,
	.k250a_ldo = NULL,
	.k250a_clk = NULL,
};

#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
static int secrityic_en_gpio = -1;

static int k250a_parse_dt(struct device *dev)
{
    struct device_node *np = dev->of_node;

    pr_err("[%s]l=%d: parse gpio_power!\n", __FUNCTION__, __LINE__);
    secrityic_en_gpio = of_get_named_gpio(np, "sec,gpio_power", 0);
    if (secrityic_en_gpio < 0) {
        pr_err("get k250a,pvdd_gpio err!!!:%d\n", secrityic_en_gpio);
        return -1;
    }
    pr_err("k250a,pvdd_gpio=%d\n", secrityic_en_gpio);

    return 0;
}
#endif  // gpio control power supply

static ssize_t show_k250a_node(struct device *dev,
					struct device_attribute *attr, char *buf)
{
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
	pr_info("[%s]l=%d: gpio=%d\n",
		__FUNCTION__, __LINE__, gpio_get_value(secrityic_en_gpio));
#endif

	if (k250a_data.k250a_is_open) {
		return sprintf(buf, "%s\n", "opened");
	} else {
		return sprintf(buf, "%s\n", "closed");
	}
}

static ssize_t store_k250a_node(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
	int ret = 0;
#endif
#if defined(SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK) // CLK_IFRAO_SPI3 for TEE
    int ret = 0;
#endif
    pr_info("[%s]l=%d: open flag=%d\n",
            __FUNCTION__, __LINE__, k250a_data.k250a_is_open);
    if (len <= 0) {
        return -1;
    }

    if (buf[0] == '1' && k250a_data.k250a_is_open == K250A_CLOSE) {
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
#if defined(SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK) // CLK_IFRAO_SPI3 for TEE
        if (IS_ERR(k250a_data.k250a_clk)) {
            ret = PTR_ERR(k250a_data.k250a_clk);
            pr_err("k250a failed to get k250a_clk, ret is %d\n", ret);
            return -1;
        }
        clk_prepare_enable(k250a_data.k250a_clk);
#endif  // SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK
        gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_OPEN);
        pr_info("[%s]l=%d: gpio control power supply=>power on\n",
            __FUNCTION__, __LINE__);
#else // pmic ldo_vio18 power supply
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
        if (IS_ERR(k250a_data.k250a_ldo)) {
            ret = PTR_ERR(k250a_data.k250a_ldo);
            pr_err("failed to get k250a_ldo, ret is %d\n", ret);
            return ret;
        }
        ret = regulator_enable(k250a_data.k250a_ldo);
        if (ret) {
            pr_err("[%s]l=%d: failed to en ldo_vio18, ret=%d\n",
                __FUNCTION__, __LINE__, ret);
        }
#endif  // ldo control, otherwise always on
#endif  // gpio control power supply
        k250a_data.k250a_is_open = K250A_OPEN;
    } else if (buf[0] == '0' && k250a_data.k250a_is_open == K250A_OPEN) {
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
#if defined(SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK) // CLK_IFRAO_SPI3 for TEE
        if (IS_ERR(k250a_data.k250a_clk)) {
            ret = PTR_ERR(k250a_data.k250a_clk);
            pr_err("k250a failed to get k250a_clk, ret is %d\n", ret);
            return -1;
        }
        clk_disable_unprepare(k250a_data.k250a_clk);
#endif  // SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK
        gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_CLOSE);
        pr_info("[%s]l=%d: gpio control power supply=>power off\n",
            __FUNCTION__, __LINE__);
#else // pmic ldo_vio18 power supply
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
        if (IS_ERR(k250a_data.k250a_ldo)) {
            ret = PTR_ERR(k250a_data.k250a_ldo);
            pr_err("failed to get k250a_ldo, ret is %d\n", ret);
            return ret;
        }
        ret = regulator_disable(k250a_data.k250a_ldo);
        if (ret) {
            pr_err("[%s]l=%d: failed to dis ldo_vio18, ret=%d\n",
                __FUNCTION__, __LINE__, ret);
        }
#endif
#endif
        k250a_data.k250a_is_open = K250A_CLOSE;
    } else {
        pr_err("[%s]l=%d: buf is %s, len is %d, k250a_is_open is %d\n",
                        __FUNCTION__, __LINE__, buf, len, k250a_data.k250a_is_open);
        return -1;
    }

    return len;
}
// /sys/class/k250a_node_class/k250a/k250a_node_gpio
static DEVICE_ATTR(k250a_node_gpio, S_IWUSR|S_IRUSR,
				show_k250a_node, store_k250a_node);

static int k250a_open(struct inode *inode, struct file *filp)
{
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
    int ret = 0;
#endif
#if defined(SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK) // CLK_IFRAO_SPI3 for TEE
    int ret = 0;
#endif

    pr_info("[%s]l=%d\n", __FUNCTION__, __LINE__);
    if (k250a_data.k250a_is_open == K250A_OPEN) {
        pr_err("k250a is opened yet\n");
        return -EBUSY;
    }
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
#if defined(SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK) // CLK_IFRAO_SPI3 for TEE
    if (IS_ERR(k250a_data.k250a_clk)) {
        ret = PTR_ERR(k250a_data.k250a_clk);
        pr_err("k250a failed to get k250a_clk, ret is %d\n", ret);
        return ret;
    }
    clk_prepare_enable(k250a_data.k250a_clk);
#endif  // SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK
    gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_OPEN);
    pr_info("[%s]l=%d: gpio control power supply=>power on\n",
        __FUNCTION__, __LINE__);
#else // pmic ldo_vio18 power supply
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
    if (IS_ERR(k250a_data.k250a_ldo)) {
        ret = PTR_ERR(k250a_data.k250a_ldo);
        pr_err("failed to get k250a_ldo, ret is %d\n", ret);
        return ret;
    }
#endif  // ldo control, otherwise always on
#endif  // CLK_IFRAO_SPI3 for TEE
	usleep_range(2000, 2001);
    k250a_data.k250a_is_open = K250A_OPEN;

    return 0;
}

static int k250a_release(struct inode *inode, struct file *filp)
{
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
    int ret = 0;
#endif
#if defined(SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK)
    int ret = 0;
#endif

    pr_info("[%s]l=%d\n", __FUNCTION__, __LINE__);
    if (k250a_data.k250a_is_open == K250A_CLOSE) {
        pr_err("k250a is closed yet\n");
        return -EPERM;
    }
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
#if defined(SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK) // CLK_IFRAO_SPI3 for TEE
    if (IS_ERR(k250a_data.k250a_clk)) {
        ret = PTR_ERR(k250a_data.k250a_clk);
        pr_err("k250a failed to get k250a_clk, ret is %d\n", ret);
        return ret;
    }
    clk_disable_unprepare(k250a_data.k250a_clk);
#endif  // SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK
    gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_CLOSE);
    pr_info("[%s]l=%d: gpio control power supply=>power off\n",
        __FUNCTION__, __LINE__);
#else // pmic ldo_vio18 power supply
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
    if (IS_ERR(k250a_data.k250a_ldo)) {
        ret = PTR_ERR(k250a_data.k250a_ldo);
        pr_err("failed to get k250a_ldo, ret is %d\n", ret);
        return ret;
    }
    ret = regulator_disable(k250a_data.k250a_ldo);
    if (ret) {
        pr_err("[%s]l=%d: failed to dis ldo_vio18, ret=%d\n",
            __FUNCTION__, __LINE__, ret);
    }
#endif  // ldo control, otherwise always on
#endif  // gpio control power supply
    usleep_range(5000, 5001);
    k250a_data.k250a_is_open = K250A_CLOSE;

    return 0;
}

static ssize_t k250a_write(struct file *file, const char __user *buf,
						size_t count, loff_t *ppos)
{
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
    int ret = 0;
#endif

    char p[1] = {0x00};

    pr_info("[%s]l=%d\n", __FUNCTION__, __LINE__);
    if (count == 0 || buf == NULL) {
        return -1;
    }

    if (copy_from_user(p, buf, 1)) {
        return -1;
    }

    if (p[0] == '0') {
        if (k250a_data.k250a_is_open == K250A_CLOSE) {
            pr_err("k250a is closed yet\n");
            return -EPERM;
        }
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
#if defined(SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK) // CLK_IFRAO_SPI3 for TEE
        if (IS_ERR(k250a_data.k250a_clk)) {
            ret = PTR_ERR(k250a_data.k250a_clk);
            pr_err("k250a failed to get k250a_clk, ret is %d\n", ret);
            return ret;
        }
        clk_disable_unprepare(k250a_data.k250a_clk);
#endif  // SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK
        gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_CLOSE);
        pr_info("[%s]l=%d: gpio control power supply\n",
            __FUNCTION__, __LINE__);
#else // pmic ldo_vio18 power supply
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
        if (IS_ERR(k250a_data.k250a_ldo)) {
            ret = PTR_ERR(k250a_data.k250a_ldo);
            pr_err("failed to get k250a_ldo, ret is %d\n", ret);
            return ret;
        }
        ret = regulator_disable(k250a_data.k250a_ldo);
        if (ret) {
            pr_err("[%s]l=%d: failed to dis ldo_vio18, ret=%d\n",
                __FUNCTION__, __LINE__, ret);
        }
#endif  // ldo control, otherwise always on
#endif  // gpio control power supply
        k250a_data.k250a_is_open = K250A_CLOSE;
    } else {
        if (k250a_data.k250a_is_open == K250A_OPEN) {
            pr_err("k250a is opened yet\n");
            return -EPERM;
        }
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
#if defined(SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK) // CLK_IFRAO_SPI3 for TEE
        if (IS_ERR(k250a_data.k250a_clk)) {
            ret = PTR_ERR(k250a_data.k250a_clk);
            pr_err("k250a failed to get k250a_clk, ret is %d\n", ret);
            return ret;
        }
        clk_prepare_enable(k250a_data.k250a_clk);
#endif  // SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK
        gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_OPEN);
        pr_info("[%s]l=%d: gpio control power supply\n",
            __FUNCTION__, __LINE__);
#else // pmic ldo_vio18 power supply
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
        if (IS_ERR(k250a_data.k250a_ldo)) {
            ret = PTR_ERR(k250a_data.k250a_ldo);
            pr_err("failed to get k250a_ldo, ret is %d\n", ret);
            return ret;
        }
        ret = regulator_enable(k250a_data.k250a_ldo);
        if (ret) {
            pr_err("[%s]l=%d: failed to en ldo_vio18, ret=%d\n",
                __FUNCTION__, __LINE__, ret);
        }
#endif  // ldo control, otherwise always on
#endif  // gpio control power supply
        k250a_data.k250a_is_open = K250A_OPEN;
    }

    return count;
}

static ssize_t k250a_read(struct file *file, char __user *buf,
						size_t count, loff_t *ppos)
{
	pr_info("[%s]l=%d\n", __FUNCTION__, __LINE__);

	if (count == 0 || buf == NULL) {
		return -1;
	}

	if (k250a_data.k250a_is_open == K250A_OPEN) {
		copy_to_user(buf, "1", 1);
	} else {
		copy_to_user(buf, "0", 1);
	}

	return count;
}

// devFS-/dev/k250a
struct file_operations k250a_node_ops = {
	.owner  = THIS_MODULE,
	.open = k250a_open,
	.release = k250a_release,
	.read = k250a_read,
	.write = k250a_write,
};

static int major;
static struct class *k250a_node_class = NULL;

static const struct of_device_id k250a_match_table[] = {
/* M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 start */
#if defined(CONFIG_CUSTOM_FACTORY_BUILD)
	{ .compatible = "sec,k250a",},
#endif
	{},
/* M55 code for SR-QN6887A-01-1023 by gaochao at 20231115 end */
};

#define EVB0    "EVB0"
#define EVB1    "EVB1"
bool k250a_get_cmdline(const char *name)
{
    const char *bootparams = NULL;
    bool result = false;
    struct device_node *np = NULL;

    if (!name) {
        pr_err("[%s]l=%d: name is NULL\n", __FUNCTION__, __LINE__);
        return false;
    }

    np = of_find_node_by_path("/chosen");
    if (!np) {
        pr_err("[%s]l=%d: failed to find node\n",
            __FUNCTION__, __LINE__);
        return false;
    }

    if (of_property_read_string(np, "bootargs", &bootparams)) {
        pr_err("[%s]l=%d: failed to get bootargs property\n",
            __FUNCTION__, __LINE__);
        of_node_put(np); // Free the node reference
        return false;
    }

    result = strnstr(bootparams, name, strlen(bootparams)) ? true : false;
    if (result) {
        pr_err("[%s]l=%d: Successfully matched '%s' in cmdline\n",
            __FUNCTION__, __LINE__, name);
    } else {
        pr_err("[%s]l=%d: No '%s' in cmdline\n",
            __FUNCTION__, __LINE__, name);
    }

    of_node_put(np); // Free the node reference

    return result;
}

static int k250a_probe(struct platform_device *pdev)
{
    int ret = 0;
    struct device *k250a_node_device = NULL;

    pr_err("[%s]l=%d: start\n", __FUNCTION__, __LINE__);

    if (k250a_get_cmdline(EVB0) || k250a_get_cmdline(EVB1)) {
        pr_err("[%s]l=%d: EVB0, EVB1 don't SMT k250a\n", __FUNCTION__, __LINE__);
        return ret;
    } else {
        pr_err("[%s]l=%d: SMT k250a\n", __FUNCTION__, __LINE__);
    }

    major = register_chrdev(0, "k250a_node", &k250a_node_ops);
    if (major < 0){
        pr_err("fail to register k250a chrdev,ret is %d\n", major);
        ret = major;
        goto err_register_chr;
    }
    // sysFS-/sys/class/k250a_node_class/
    k250a_node_class = class_create(THIS_MODULE, "k250a_node_class");
    if (IS_ERR(k250a_node_class)) {
        pr_err("fail to create k250a class,ret is %d\n",PTR_ERR(k250a_node_class));
        ret = PTR_ERR(k250a_node_class);
        goto err_class_create;
    }
    // devFS-/dev/k250a
    k250a_node_device = device_create(k250a_node_class, 0,
                            MKDEV(major, 0), NULL,"k250a");
    if (IS_ERR(k250a_node_device)) {
        pr_err("fail to create k250a device,ret is %d\n",PTR_ERR(k250a_node_device));
        ret = PTR_ERR(k250a_node_device);
        goto err_device_create;
    }
    // sysFS-/sys/class/k250a_node_class/k250a/k250a_node_gpio
    ret = sysfs_create_file(&(k250a_node_device->kobj),
                    &dev_attr_k250a_node_gpio.attr);
    if (ret) {
        pr_err("fail to create k250a sys file, ret is %d\n", ret);
        goto err_create_sysfs;
    }

#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
    ret = k250a_parse_dt(&pdev->dev);
    if (ret < 0) {
        return -1;
    }
    pr_info("k250a_parse_dt ret is  :%d\n", ret);
    ret = gpio_request(secrityic_en_gpio, "k250a_pvdd_en");
    if (ret != 0) {
        pr_err("failed to request k250a gpio!\n");
    }
#if defined(SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK) // CLK_IFRAO_SPI3 for TEE
    k250a_data.k250a_clk = devm_clk_get(&pdev->dev, "k250a-clk");
    if (IS_ERR(k250a_data.k250a_clk)) {
        ret = PTR_ERR(k250a_data.k250a_clk);
        pr_err("failed to get k250a_clk, ret is %d\n", ret);
        return ret;
    }
    // clk_prepare_enable(k250a_data.k250a_clk);
#endif  // SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK
#else // pmic ldo_vio18 power supply
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
    k250a_data.k250a_ldo = regulator_get(&pdev->dev, "ldo_vio18");
    if (IS_ERR(k250a_data.k250a_ldo)) {
        ret = PTR_ERR(k250a_data.k250a_ldo);
        pr_err("failed to get k250a_ldo, ret is %d\n", ret);
        goto err_get_regulator;
    }
#endif
#endif

    pr_err("[%s]l=%d: success\n", __FUNCTION__, __LINE__);

	return 0;
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
err_get_regulator:
	sysfs_remove_file(&(k250a_node_device->kobj),
					&dev_attr_k250a_node_gpio.attr);
#endif  // ldo control, otherwise always on
err_create_sysfs:
	device_destroy(k250a_node_class, MKDEV(major, 0));
err_device_create:
	class_destroy(k250a_node_class);
err_class_create:
	unregister_chrdev(major, "k250a_node");
err_register_chr:
	return ret;
}

static int k250a_remove(struct platform_device *pdev)
{
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
    int ret = 0;
#endif
    pr_info("[%s]l=%d\n", __FUNCTION__, __LINE__);

    device_destroy(k250a_node_class, MKDEV(major, 0));
    class_destroy(k250a_node_class);
    unregister_chrdev(major, "k250a_node");
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
    gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_CLOSE);
    pr_info("[%s]l=%d: gpio control power supply\n",
        __FUNCTION__, __LINE__);
#else // pmic ldo_vio18 power supply
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
    if (IS_ERR(k250a_data.k250a_ldo)) {
        ret = PTR_ERR(k250a_data.k250a_ldo);
        pr_err("failed to get k250a_ldo, ret is %d\n", ret);
        return ret;
    }
    ret = regulator_disable(k250a_data.k250a_ldo);
    if (ret) {
        pr_err("[%s]l=%d: failed to dis ldo_vio18, ret=%d\n",
            __FUNCTION__, __LINE__, ret);
    }
#endif  // ldo control, otherwise always on
#endif  // gpio control power supply
	k250a_data.k250a_is_open = K250A_CLOSE;

	return 0;
}

static struct platform_driver k250a_driver = {
	.driver = {
		.name = "k250a",
		.owner = THIS_MODULE,
		.of_match_table = k250a_match_table,
	},
	.probe =  k250a_probe,
	.remove = k250a_remove,
};

static int __init k250a_init(void)
{
	pr_err("%s\n", __func__);
	return platform_driver_register(&k250a_driver);
}

static void __exit k250a_exit(void)
{
	pr_err("%s\n", __func__);
}

late_initcall(k250a_init);
module_exit(k250a_exit);
MODULE_LICENSE("GPL");