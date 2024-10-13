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

enum gdrm_status {
	GDRM_CLOSE = 0,
	GDRM_OPEN = 1,
};

enum gpio_ctrl_power {
	GPIO_CTRL_POWER_CLOSE = 0,
	GPIO_CTRL_POWER_OPEN = 1,
};

struct grdm_driver_data {
	int grdm_is_open;
	struct regulator *grdm_ldo;
	struct clk *grdm_clk;
};

static struct grdm_driver_data grdm_data = {
	.grdm_is_open = GDRM_CLOSE,
	.grdm_ldo = NULL,
	.grdm_clk = NULL,
};

#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
static int secrityic_en_gpio = -1;

static int grdm_parse_dt(struct device *dev)
{
    struct device_node *np = dev->of_node;

    pr_err("[%s]l=%d: parse gpio_power!\n", __FUNCTION__, __LINE__);
    secrityic_en_gpio = of_get_named_gpio(np, "sec,gpio_power", 0);
    if (secrityic_en_gpio < 0) {
        pr_err("get grdm,pvdd_gpio err!!!:%d\n", secrityic_en_gpio);
        return -1;
    }
    pr_err("grdm,pvdd_gpio=%d\n", secrityic_en_gpio);

    return 0;
}
#endif  // gpio control power supply

static ssize_t show_grdm_node(struct device *dev,
					struct device_attribute *attr, char *buf)
{
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
	pr_info("[%s]l=%d: gpio=%d\n",
		__FUNCTION__, __LINE__, gpio_get_value(secrityic_en_gpio));
#endif

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
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
	int ret = 0;
#endif
#if defined(SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK) // CLK_IFRAO_SPI3 for TEE
    int ret = 0;
#endif
    pr_info("[%s]l=%d: open flag=%d\n",
            __FUNCTION__, __LINE__, grdm_data.grdm_is_open);
    if (len <= 0) {
        return -1;
    }

    if (buf[0] == '1' && grdm_data.grdm_is_open == GDRM_CLOSE) {
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
#if defined(SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK) // CLK_IFRAO_SPI3 for TEE
        if (IS_ERR(grdm_data.grdm_clk)) {
            ret = PTR_ERR(grdm_data.grdm_clk);
            pr_err("grdm failed to get grdm_clk, ret is %d\n", ret);
            return -1;
        }
        clk_prepare_enable(grdm_data.grdm_clk);
#endif  // SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK
        gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_OPEN);
        pr_info("[%s]l=%d: gpio control power supply=>power on\n",
            __FUNCTION__, __LINE__);
#else // pmic ldo_vio18 power supply
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
        if (IS_ERR(grdm_data.grdm_ldo)) {
            ret = PTR_ERR(grdm_data.grdm_ldo);
            pr_err("failed to get grdm_ldo, ret is %d\n", ret);
            return ret;
        }
        ret = regulator_enable(grdm_data.grdm_ldo);
        if (ret) {
            pr_err("[%s]l=%d: failed to en ldo_vio18, ret=%d\n",
                __FUNCTION__, __LINE__, ret);
        }
#endif  // ldo control, otherwise always on
#endif  // gpio control power supply
        grdm_data.grdm_is_open = GDRM_OPEN;
    } else if (buf[0] == '0' && grdm_data.grdm_is_open == GDRM_OPEN) {
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
#if defined(SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK) // CLK_IFRAO_SPI3 for TEE
        if (IS_ERR(grdm_data.grdm_clk)) {
            ret = PTR_ERR(grdm_data.grdm_clk);
            pr_err("grdm failed to get grdm_clk, ret is %d\n", ret);
            return -1;
        }
        clk_disable_unprepare(grdm_data.grdm_clk);
#endif  // SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK
        gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_CLOSE);
        pr_info("[%s]l=%d: gpio control power supply=>power off\n",
            __FUNCTION__, __LINE__);
#else // pmic ldo_vio18 power supply
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
        if (IS_ERR(grdm_data.grdm_ldo)) {
            ret = PTR_ERR(grdm_data.grdm_ldo);
            pr_err("failed to get grdm_ldo, ret is %d\n", ret);
            return ret;
        }
        ret = regulator_disable(grdm_data.grdm_ldo);
        if (ret) {
            pr_err("[%s]l=%d: failed to dis ldo_vio18, ret=%d\n",
                __FUNCTION__, __LINE__, ret);
        }
#endif
#endif
        grdm_data.grdm_is_open = GDRM_CLOSE;
    } else {
        pr_err("[%s]l=%d: buf is %s, len is %d, grdm_is_open is %d\n",
                        __FUNCTION__, __LINE__, buf, len, grdm_data.grdm_is_open);
        return -1;
    }

    return len;
}
// /sys/class/grdm_node_class/grdm/grdm_node_gpio
static DEVICE_ATTR(grdm_node_gpio, S_IWUSR|S_IRUSR,
				show_grdm_node, store_grdm_node);

static int grdm_open(struct inode *inode, struct file *filp)
{
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
    int ret = 0;
#endif
#if defined(SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK) // CLK_IFRAO_SPI3 for TEE
    int ret = 0;
#endif

    pr_info("[%s]l=%d\n", __FUNCTION__, __LINE__);
    if (grdm_data.grdm_is_open == GDRM_OPEN) {
        pr_err("grdm is opened yet\n");
        return -EBUSY;
    }
    /* M55 code for SR-QN6887A-01-1023 by gaochao at 20240105 start */
    grdm_data.grdm_is_open = GDRM_OPEN;
    /* M55 code for SR-QN6887A-01-1023 by gaochao at 20240105 end */
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
#if defined(SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK) // CLK_IFRAO_SPI3 for TEE
    if (IS_ERR(grdm_data.grdm_clk)) {
        ret = PTR_ERR(grdm_data.grdm_clk);
        pr_err("grdm failed to get grdm_clk, ret is %d\n", ret);
        /* M55 code for SR-QN6887A-01-1023 by gaochao at 20240105 start */
        grdm_data.grdm_is_open = GDRM_CLOSE;
        /* M55 code for SR-QN6887A-01-1023 by gaochao at 20240105 end */
        return ret;
    }
    clk_prepare_enable(grdm_data.grdm_clk);
#endif  // SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK
    gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_OPEN);
    pr_info("[%s]l=%d: gpio control power supply=>power on\n",
        __FUNCTION__, __LINE__);
#else // pmic ldo_vio18 power supply
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
    if (IS_ERR(grdm_data.grdm_ldo)) {
        ret = PTR_ERR(grdm_data.grdm_ldo);
        pr_err("failed to get grdm_ldo, ret is %d\n", ret);
        /* M55 code for SR-QN6887A-01-1023 by gaochao at 20240105 start */
        grdm_data.grdm_is_open = GDRM_CLOSE;
        /* M55 code for SR-QN6887A-01-1023 by gaochao at 20240105 end */
        return ret;
    }
#endif  // ldo control, otherwise always on
#endif  // CLK_IFRAO_SPI3 for TEE
    /* M55 code for SR-QN6887A-01-1023 by gaochao at 20231212 start */
    // usleep_range(2000, 2001);
    // usleep_range(60000, 60001);
    msleep(60);
    /* M55 code for SR-QN6887A-01-1023 by gaochao at 20231212 end */

    return 0;
}

static int grdm_release(struct inode *inode, struct file *filp)
{
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
    int ret = 0;
#endif
#if defined(SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK)
    int ret = 0;
#endif

    pr_info("[%s]l=%d\n", __FUNCTION__, __LINE__);
    if (grdm_data.grdm_is_open == GDRM_CLOSE) {
        pr_err("grdm is closed yet\n");
        return -EPERM;
    }
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
#if defined(SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK) // CLK_IFRAO_SPI3 for TEE
    if (IS_ERR(grdm_data.grdm_clk)) {
        ret = PTR_ERR(grdm_data.grdm_clk);
        pr_err("grdm failed to get grdm_clk, ret is %d\n", ret);
        return ret;
    }
    clk_disable_unprepare(grdm_data.grdm_clk);
#endif  // SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK
    gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_CLOSE);
    pr_info("[%s]l=%d: gpio control power supply=>power off\n",
        __FUNCTION__, __LINE__);
#else // pmic ldo_vio18 power supply
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
    if (IS_ERR(grdm_data.grdm_ldo)) {
        ret = PTR_ERR(grdm_data.grdm_ldo);
        pr_err("failed to get grdm_ldo, ret is %d\n", ret);
        return ret;
    }
    ret = regulator_disable(grdm_data.grdm_ldo);
    if (ret) {
        pr_err("[%s]l=%d: failed to dis ldo_vio18, ret=%d\n",
            __FUNCTION__, __LINE__, ret);
    }
#endif  // ldo control, otherwise always on
#endif  // gpio control power supply
    /* M55 code for SR-QN6887A-01-1023 by gaochao at 20231212 start */
    // usleep_range(5000, 5001);
    // usleep_range(60000, 60001);
    msleep(60);
    /* M55 code for SR-QN6887A-01-1023 by gaochao at 20231212 end */
    grdm_data.grdm_is_open = GDRM_CLOSE;
    /* M55 code for SR-QN6887A-01-1023 by gaochao at 20240122 start */
    pr_info("[%s]l=%d: released\n", __FUNCTION__, __LINE__);
    /* M55 code for SR-QN6887A-01-1023 by gaochao at 20240122 end */

    return 0;
}

static ssize_t grdm_write(struct file *file, const char __user *buf,
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
        if (grdm_data.grdm_is_open == GDRM_CLOSE) {
            pr_err("grdm is closed yet\n");
            return -EPERM;
        }
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
#if defined(SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK) // CLK_IFRAO_SPI3 for TEE
        if (IS_ERR(grdm_data.grdm_clk)) {
            ret = PTR_ERR(grdm_data.grdm_clk);
            pr_err("grdm failed to get grdm_clk, ret is %d\n", ret);
            return ret;
        }
        clk_disable_unprepare(grdm_data.grdm_clk);
#endif  // SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK
        gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_CLOSE);
        pr_info("[%s]l=%d: gpio control power supply\n",
            __FUNCTION__, __LINE__);
#else // pmic ldo_vio18 power supply
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
        if (IS_ERR(grdm_data.grdm_ldo)) {
            ret = PTR_ERR(grdm_data.grdm_ldo);
            pr_err("failed to get grdm_ldo, ret is %d\n", ret);
            return ret;
        }
        ret = regulator_disable(grdm_data.grdm_ldo);
        if (ret) {
            pr_err("[%s]l=%d: failed to dis ldo_vio18, ret=%d\n",
                __FUNCTION__, __LINE__, ret);
        }
#endif  // ldo control, otherwise always on
#endif  // gpio control power supply
        grdm_data.grdm_is_open = GDRM_CLOSE;
    } else {
        if (grdm_data.grdm_is_open == GDRM_OPEN) {
            pr_err("grdm is opened yet\n");
            return -EPERM;
        }
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
#if defined(SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK) // CLK_IFRAO_SPI3 for TEE
        if (IS_ERR(grdm_data.grdm_clk)) {
            ret = PTR_ERR(grdm_data.grdm_clk);
            pr_err("grdm failed to get grdm_clk, ret is %d\n", ret);
            return ret;
        }
        clk_prepare_enable(grdm_data.grdm_clk);
#endif  // SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK
        gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_OPEN);
        pr_info("[%s]l=%d: gpio control power supply\n",
            __FUNCTION__, __LINE__);
#else // pmic ldo_vio18 power supply
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
        if (IS_ERR(grdm_data.grdm_ldo)) {
            ret = PTR_ERR(grdm_data.grdm_ldo);
            pr_err("failed to get grdm_ldo, ret is %d\n", ret);
            return ret;
        }
        ret = regulator_enable(grdm_data.grdm_ldo);
        if (ret) {
            pr_err("[%s]l=%d: failed to en ldo_vio18, ret=%d\n",
                __FUNCTION__, __LINE__, ret);
        }
#endif  // ldo control, otherwise always on
#endif  // gpio control power supply
        grdm_data.grdm_is_open = GDRM_OPEN;
    }

    return count;
}

static ssize_t grdm_read(struct file *file, char __user *buf,
						size_t count, loff_t *ppos)
{
	pr_info("[%s]l=%d\n", __FUNCTION__, __LINE__);

	if (count == 0 || buf == NULL) {
		return -1;
	}

	if (grdm_data.grdm_is_open == GDRM_OPEN) {
		copy_to_user(buf, "1", 1);
	} else {
		copy_to_user(buf, "0", 1);
	}

	return count;
}

// devFS-/dev/grdm
struct file_operations grdm_node_ops = {
	.owner  = THIS_MODULE,
	.open = grdm_open,
	.release = grdm_release,
	.read = grdm_read,
	.write = grdm_write,
};

static int major;
static struct class *grdm_node_class = NULL;

static const struct of_device_id grdm_match_table[] = {
	{ .compatible = "sec,guardianm",},
	{},
};

static int grdm_probe(struct platform_device *pdev)
{
    int ret = 0;
    struct device *grdm_node_device = NULL;
    pr_err("[%s]l=%d: start\n", __FUNCTION__, __LINE__);

    major = register_chrdev(0, "grdm_node", &grdm_node_ops);
    if (major < 0){
        pr_err("fail to register grdm chrdev,ret is %d\n", major);
        ret = major;
        goto err_register_chr;
    }
    // sysFS-/sys/class/grdm_node_class/
    grdm_node_class = class_create(THIS_MODULE, "grdm_node_class");
    if (IS_ERR(grdm_node_class)) {
        pr_err("fail to create grdm class,ret is %d\n",PTR_ERR(grdm_node_class));
        ret = PTR_ERR(grdm_node_class);
        goto err_class_create;
    }
    // devFS-/dev/grdm
    grdm_node_device = device_create(grdm_node_class, 0,
                            MKDEV(major, 0), NULL,"grdm");
    if (IS_ERR(grdm_node_device)) {
        pr_err("fail to create grdm device,ret is %d\n",PTR_ERR(grdm_node_device));
        ret = PTR_ERR(grdm_node_device);
        goto err_device_create;
    }
    // sysFS-/sys/class/grdm_node_class/grdm/grdm_node_gpio
    ret = sysfs_create_file(&(grdm_node_device->kobj),
                    &dev_attr_grdm_node_gpio.attr);
    if (ret) {
        pr_err("fail to create grdm sys file, ret is %d\n", ret);
        goto err_create_sysfs;
    }

#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
    ret = grdm_parse_dt(&pdev->dev);
    if (ret < 0) {
        return -1;
    }
    pr_info("grdm_parse_dt ret is  :%d\n", ret);
    ret = gpio_request(secrityic_en_gpio, "grdm_pvdd_en");
    if (ret != 0) {
        pr_err("failed to request grdm gpio!\n");
    }
#if defined(SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK) // CLK_IFRAO_SPI3 for TEE
    grdm_data.grdm_clk = devm_clk_get(&pdev->dev, "grdm-clk");
    if (IS_ERR(grdm_data.grdm_clk)) {
        ret = PTR_ERR(grdm_data.grdm_clk);
        pr_err("failed to get grdm_clk, ret is %d\n", ret);
        return ret;
    }
    // clk_prepare_enable(grdm_data.grdm_clk);
#endif  // SECURITY_IC_ENABLE_SPI_VIA_SPI_CLK
#else // pmic ldo_vio18 power supply
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
    grdm_data.grdm_ldo = regulator_get(&pdev->dev, "ldo_vio18");
    if (IS_ERR(grdm_data.grdm_ldo)) {
        ret = PTR_ERR(grdm_data.grdm_ldo);
        pr_err("failed to get grdm_ldo, ret is %d\n", ret);
        goto err_get_regulator;
    }
#endif
#endif

    pr_err("[%s]l=%d: success\n", __FUNCTION__, __LINE__);

	return 0;
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
err_get_regulator:
	sysfs_remove_file(&(grdm_node_device->kobj),
					&dev_attr_grdm_node_gpio.attr);
#endif  // ldo control, otherwise always on
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
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
    int ret = 0;
#endif
    pr_info("[%s]l=%d\n", __FUNCTION__, __LINE__);

    device_destroy(grdm_node_class, MKDEV(major, 0));
    class_destroy(grdm_node_class);
    unregister_chrdev(major, "grdm_node");
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
    gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_CLOSE);
    pr_info("[%s]l=%d: gpio control power supply\n",
        __FUNCTION__, __LINE__);
#else // pmic ldo_vio18 power supply
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
    if (IS_ERR(grdm_data.grdm_ldo)) {
        ret = PTR_ERR(grdm_data.grdm_ldo);
        pr_err("failed to get grdm_ldo, ret is %d\n", ret);
        return ret;
    }
    ret = regulator_disable(grdm_data.grdm_ldo);
    if (ret) {
        pr_err("[%s]l=%d: failed to dis ldo_vio18, ret=%d\n",
            __FUNCTION__, __LINE__, ret);
    }
#endif  // ldo control, otherwise always on
#endif  // gpio control power supply
	grdm_data.grdm_is_open = GDRM_CLOSE;

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
	pr_err("%s\n", __func__);
	return platform_driver_register(&grdm_driver);
}

static void __exit grdm_exit(void)
{
	pr_err("%s\n", __func__);
}

late_initcall(grdm_init);
module_exit(grdm_exit);
MODULE_LICENSE("GPL");