#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/ioctl.h>

#define SECURITY_IC_POWER_SUPPLY_VIA_GPIO   // gpio control power supply
/* A06 code for SR-AL7160A-01-708 by gaochao at 20240416 start */
#define SECURITY_IC_ENABLE_I2C_VIA_I2C_CLK
/* A06 code for SR-AL7160A-01-708 by gaochao at 20240416 end */

#define STAR_MAGIC_CODE			'S'
#define STAR_RESET_INTERFACE	_IO(STAR_MAGIC_CODE, 3)

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
    struct mutex dev_ref_mutex;
    int k250a_is_open;
    struct regulator *k250a_ldo;
    /* A06 code for SR-AL7160A-01-06 by gaochao at 20240424 start */
    struct clk *k250a_clk_main;
    struct clk *k250a_clk_dma;
    /* A06 code for SR-AL7160A-01-06 by gaochao at 20240424 end */
};

static struct k250a_driver_data k250a_data = {
    .k250a_is_open = K250A_CLOSE,
    .k250a_ldo = NULL,
    .k250a_clk_main = NULL,
    .k250a_clk_dma = NULL,
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
#if defined(SECURITY_IC_ENABLE_I2C_VIA_I2C_CLK) // CLK_IFR_I2C6 for TEE
    int ret = 0;
#endif
    pr_info("[%s]l=%d: open flag=%d\n",
            __FUNCTION__, __LINE__, k250a_data.k250a_is_open);
    if (len <= 0) {
        pr_err("[%s]l=%d: failed to en ldo_vio18, len=%u\n",
            __FUNCTION__, __LINE__, len);
        return -1;
    }

    if (buf[0] == '1' && k250a_data.k250a_is_open == K250A_CLOSE) {
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
#if defined(SECURITY_IC_ENABLE_I2C_VIA_I2C_CLK) // CLK_IFR_I2C6 for TEE
        /* A06 code for AL7160A-1233 by gaochao at 20240625 start */
        do {
            if (IS_ERR(k250a_data.k250a_clk_dma)) {
                ret = PTR_ERR(k250a_data.k250a_clk_dma);
                pr_err("k250a failed to get k250a_clk_dma, ret is %d\n", ret);
                return -1;
            }
            ret = clk_prepare_enable(k250a_data.k250a_clk_dma);
            if (ret) {
                pr_err("k250a failed to enable k250a_clk_dma, ret is %d\n", ret);
                continue;
                // return ret;
            }

            if (IS_ERR(k250a_data.k250a_clk_main)) {
                ret = PTR_ERR(k250a_data.k250a_clk_main);
                pr_err("k250a failed to get k250a_clk_main, ret is %d\n", ret);
                clk_disable_unprepare(k250a_data.k250a_clk_dma);
                return -1;
            }
            ret = clk_prepare_enable(k250a_data.k250a_clk_main);
            if (ret) {
                pr_err("k250a failed to enable k250a_clk_main, ret is %d\n", ret);
                clk_disable_unprepare(k250a_data.k250a_clk_dma);
                continue;
                // return ret;
            }
        } while (ret);
        /* A06 code for AL7160A-1233 by gaochao at 20240625 end */
#endif  // SECURITY_IC_ENABLE_I2C_VIA_I2C_CLK
        /* A06 code for AL7160A-1233 by gaochao at 20240625 start */
        /* A06 code for SR-AL7160A-01-708 by gaochao at 20240508 start */
        ret = gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_CLOSE);
        if (ret != 0) {
            pr_err("[%s][%d]: failed to set gpio, ret=%d\n", __FUNCTION__, __LINE__, ret);
        }
        usleep_range(1000, 2000);
        ret = gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_OPEN);
        if (ret != 0) {
            pr_err("[%s][%d]: failed to set gpio, ret=%d\n", __FUNCTION__, __LINE__, ret);
        }
        usleep_range(15000, 20000);
        /* A06 code for SR-AL7160A-01-708 by gaochao at 20240508 end */
        /* A06 code for AL7160A-1233 by gaochao at 20240625 end */
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
#if defined(SECURITY_IC_ENABLE_I2C_VIA_I2C_CLK) // CLK_IFR_I2C6 for TEE
        /* A06 code for AL7160A-1233 by gaochao at 20240625 start */
        if (IS_ERR(k250a_data.k250a_clk_main)) {
            ret = PTR_ERR(k250a_data.k250a_clk_main);
            pr_err("k250a failed to get k250a_clk_main, ret is %d\n", ret);
            return -1;
        }
        clk_disable_unprepare(k250a_data.k250a_clk_main);

        if (IS_ERR(k250a_data.k250a_clk_dma)) {
            ret = PTR_ERR(k250a_data.k250a_clk_dma);
            pr_err("k250a failed to get k250a_clk_dma, ret is %d\n", ret);
            return -1;
        }
        clk_disable_unprepare(k250a_data.k250a_clk_dma);
        /* A06 code for AL7160A-1233 by gaochao at 20240625 end */
#endif  // SECURITY_IC_ENABLE_I2C_VIA_I2C_CLK
        /* A06 code for AL7160A-1233 by gaochao at 20240625 start */
        /* A06 code for SR-AL7160A-01-708 by gaochao at 20240508 start */
        ret = gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_CLOSE);
        if (ret != 0) {
            pr_err("[%s][%d]: failed to set gpio, ret=%d\n", __FUNCTION__, __LINE__, ret);
        }
        usleep_range(15000, 20000);
        /* A06 code for SR-AL7160A-01-708 by gaochao at 20240508 end */
        /* A06 code for AL7160A-1233 by gaochao at 20240625 end */
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
#if defined(SECURITY_IC_ENABLE_I2C_VIA_I2C_CLK) // CLK_IFR_I2C6 for TEE
    int ret = 0;
#endif

    pr_info("[%s]l=%d\n", __FUNCTION__, __LINE__);
    mutex_lock(&k250a_data.dev_ref_mutex);

    k250a_data.k250a_is_open = K250A_OPEN;
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
#if defined(SECURITY_IC_ENABLE_I2C_VIA_I2C_CLK) // CLK_IFR_I2C6 for TEE
    /* A06 code for AL7160A-1233 by gaochao at 20240625 start */
    if (IS_ERR(k250a_data.k250a_clk_dma)) {
        ret = PTR_ERR(k250a_data.k250a_clk_dma);
        pr_err("k250a failed to get k250a_clk_dma, ret is %d\n", ret);
        mutex_unlock(&k250a_data.dev_ref_mutex);
        return ret;
    }
    clk_prepare_enable(k250a_data.k250a_clk_dma);

    if (IS_ERR(k250a_data.k250a_clk_main)) {
        ret = PTR_ERR(k250a_data.k250a_clk_main);
        pr_err("k250a failed to get k250a_clk_main, ret is %d\n", ret);
        mutex_unlock(&k250a_data.dev_ref_mutex);
        return ret;
    }
    clk_prepare_enable(k250a_data.k250a_clk_main);
    /* A06 code for AL7160A-1233 by gaochao at 20240625 end */
#endif  // SECURITY_IC_ENABLE_I2C_VIA_I2C_CLK
    /* A06 code for AL7160A-1233 by gaochao at 20240625 start */
    ret = gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_CLOSE);
    if (ret != 0) {
        pr_err("[%s][%d]: failed to set gpio, ret=%d\n", __FUNCTION__, __LINE__, ret);
    }
    usleep_range(1000, 2000);
    ret = gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_OPEN);
    if (ret != 0) {
        pr_err("[%s][%d]: failed to set gpio, ret=%d\n", __FUNCTION__, __LINE__, ret);
    }
    usleep_range(15000, 20000);
    /* A06 code for AL7160A-1233 by gaochao at 20240625 end */
    pr_info("[%s]l=%d: gpio control power supply=>power on\n",
        __FUNCTION__, __LINE__);
#else // pmic ldo_vio18 power supply
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
    if (IS_ERR(k250a_data.k250a_ldo)) {
        ret = PTR_ERR(k250a_data.k250a_ldo);
        pr_err("failed to get k250a_ldo, ret is %d\n", ret);
        mutex_unlock(&k250a_data.dev_ref_mutex);
        return ret;
    }
    ret = regulator_enable(k250a_data.k250a_ldo);
    if (ret) {
        pr_err("[%s]l=%d: failed to en ldo_vio18, ret=%d\n",
            __FUNCTION__, __LINE__, ret);
    }
#endif  // ldo control, otherwise always on
#endif  // CLK_IFRAO_SPI3 for TEE

    mutex_unlock(&k250a_data.dev_ref_mutex);

    return 0;
}

static int k250a_release(struct inode *inode, struct file *filp)
{
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
    int ret = 0;
#endif
#if defined(SECURITY_IC_ENABLE_I2C_VIA_I2C_CLK)
    int ret = 0;
#endif

    pr_info("[%s]l=%d\n", __FUNCTION__, __LINE__);
    mutex_lock(&k250a_data.dev_ref_mutex);

    k250a_data.k250a_is_open = K250A_CLOSE;
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
#if defined(SECURITY_IC_ENABLE_I2C_VIA_I2C_CLK) // CLK_IFR_I2C6 for TEE
    /* A06 code for AL7160A-1233 by gaochao at 20240625 start */
    if (IS_ERR(k250a_data.k250a_clk_main)) {
        ret = PTR_ERR(k250a_data.k250a_clk_main);
        pr_err("k250a failed to get k250a_clk_main, ret is %d\n", ret);
        mutex_unlock(&k250a_data.dev_ref_mutex);
        return ret;
    }
    clk_disable_unprepare(k250a_data.k250a_clk_main);

    if (IS_ERR(k250a_data.k250a_clk_dma)) {
        ret = PTR_ERR(k250a_data.k250a_clk_dma);
        pr_err("k250a failed to get k250a_clk_dma, ret is %d\n", ret);
        mutex_unlock(&k250a_data.dev_ref_mutex);
        return ret;
    }
    clk_disable_unprepare(k250a_data.k250a_clk_dma);
    /* A06 code for AL7160A-1233 by gaochao at 20240625 end */
#endif  // SECURITY_IC_ENABLE_I2C_VIA_I2C_CLK
    /* A06 code for AL7160A-1233 by gaochao at 20240625 start */
    ret = gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_CLOSE);
    if (ret != 0) {
        pr_err("[%s][%d]: failed to set gpio, ret=%d\n", __FUNCTION__, __LINE__, ret);
    }
    usleep_range(15000, 20000);
    /* A06 code for AL7160A-1233 by gaochao at 20240625 end */
    pr_info("[%s]l=%d: gpio control power supply=>power off\n",
        __FUNCTION__, __LINE__);
#else // pmic ldo_vio18 power supply
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
    if (IS_ERR(k250a_data.k250a_ldo)) {
        ret = PTR_ERR(k250a_data.k250a_ldo);
        pr_err("failed to get k250a_ldo, ret is %d\n", ret);
        mutex_unlock(&k250a_data.dev_ref_mutex);
        return ret;
    }
    ret = regulator_disable(k250a_data.k250a_ldo);
    if (ret) {
        pr_err("[%s]l=%d: failed to dis ldo_vio18, ret=%d\n",
            __FUNCTION__, __LINE__, ret);
    }
#endif  // ldo control, otherwise always on
#endif  // gpio control power supply

    mutex_unlock(&k250a_data.dev_ref_mutex);

    return 0;
}

static ssize_t k250a_write(struct file *file, const char __user *buf,
						size_t count, loff_t *ppos)
{
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
    int ret = 0;
#endif
/* A06 code for SR-AL7160A-01-708 by gaochao at 20240416 start */
#if defined(SECURITY_IC_ENABLE_I2C_VIA_I2C_CLK) // CLK_IFR_I2C6 for TEE
    int ret = 0;
#endif
/* A06 code for SR-AL7160A-01-708 by gaochao at 20240416 end */

    char p[1] = {0x00};

    pr_info("[%s]l=%d\n", __FUNCTION__, __LINE__);
    if (count == 0 || buf == NULL) {
        pr_err("[%s]l=%d: buf is NULL\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (copy_from_user(p, buf, 1)) {
        pr_err("[%s]l=%d: copy_from_user fail\n", __FUNCTION__, __LINE__);
        return -1;
    }

    mutex_lock(&k250a_data.dev_ref_mutex);

    if (p[0] == '0') {
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
#if defined(SECURITY_IC_ENABLE_I2C_VIA_I2C_CLK) // CLK_IFR_I2C6 for TEE
        /* A06 code for AL7160A-1233 by gaochao at 20240625 start */
        if (IS_ERR(k250a_data.k250a_clk_main)) {
            ret = PTR_ERR(k250a_data.k250a_clk_main);
            pr_err("k250a failed to get k250a_clk_main, ret is %d\n", ret);
            mutex_unlock(&k250a_data.dev_ref_mutex);
            return ret;
        }
        clk_disable_unprepare(k250a_data.k250a_clk_main);

        if (IS_ERR(k250a_data.k250a_clk_dma)) {
            ret = PTR_ERR(k250a_data.k250a_clk_dma);
            pr_err("k250a failed to get k250a_clk_dma, ret is %d\n", ret);
            mutex_unlock(&k250a_data.dev_ref_mutex);
            return ret;
        }
        clk_disable_unprepare(k250a_data.k250a_clk_dma);
        /* A06 code for AL7160A-1233 by gaochao at 20240625 end */
#endif  // SECURITY_IC_ENABLE_I2C_VIA_I2C_CLK
        /* A06 code for AL7160A-1233 by gaochao at 20240625 start */
        ret = gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_CLOSE);
        if (ret != 0) {
            pr_err("[%s][%d]: failed to set gpio, ret=%d\n", __FUNCTION__, __LINE__, ret);
        }
        pr_info("[%s]l=%d: gpio control power supply=>power off\n",
            __FUNCTION__, __LINE__);
        /* A06 code for AL7160A-1233 by gaochao at 20240625 end */
#else // pmic ldo_vio18 power supply
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
        if (IS_ERR(k250a_data.k250a_ldo)) {
            ret = PTR_ERR(k250a_data.k250a_ldo);
            pr_err("failed to get k250a_ldo, ret is %d\n", ret);
            mutex_unlock(&k250a_data.dev_ref_mutex);
            return ret;
        }
        ret = regulator_disable(k250a_data.k250a_ldo);
        if (ret) {
            pr_err("[%s]l=%d: failed to dis ldo_vio18, ret=%d\n",
                __FUNCTION__, __LINE__, ret);
        }
#endif  // ldo control, otherwise always on
#endif  // gpio control power supply
    } else {
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
#if defined(SECURITY_IC_ENABLE_I2C_VIA_I2C_CLK) // CLK_IFR_I2C6 for TEE
        /* A06 code for AL7160A-1233 by gaochao at 20240625 start */
        if (IS_ERR(k250a_data.k250a_clk_dma)) {
            ret = PTR_ERR(k250a_data.k250a_clk_dma);
            pr_err("k250a failed to get k250a_clk_dma, ret is %d\n", ret);
            mutex_unlock(&k250a_data.dev_ref_mutex);
            return ret;
        }
        clk_prepare_enable(k250a_data.k250a_clk_dma);

        if (IS_ERR(k250a_data.k250a_clk_main)) {
            ret = PTR_ERR(k250a_data.k250a_clk_main);
            pr_err("k250a failed to get k250a_clk_dmain, ret is %d\n", ret);
            mutex_unlock(&k250a_data.dev_ref_mutex);
            return ret;
        }
        clk_prepare_enable(k250a_data.k250a_clk_main);
        /* A06 code for SR-AL7160A-01-06 by gaochao at 20240424 end */
#endif  // SECURITY_IC_ENABLE_I2C_VIA_I2C_CLK
        /* A06 code for AL7160A-1233 by gaochao at 20240625 start */
        ret = gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_OPEN);
        if (ret != 0) {
            pr_err("[%s][%d]: failed to set gpio, ret=%d\n", __FUNCTION__, __LINE__, ret);
        }
        pr_info("[%s]l=%d: gpio control power supply=>power on\n",
            __FUNCTION__, __LINE__);
        /* A06 code for AL7160A-1233 by gaochao at 20240625 end */
#else // pmic ldo_vio18 power supply
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_LDO_CTL) // ldo control, otherwise always on
        if (IS_ERR(k250a_data.k250a_ldo)) {
            ret = PTR_ERR(k250a_data.k250a_ldo);
            pr_err("failed to get k250a_ldo, ret is %d\n", ret);
            mutex_unlock(&k250a_data.dev_ref_mutex);
            return ret;
        }
        ret = regulator_enable(k250a_data.k250a_ldo);
        if (ret) {
            pr_err("[%s]l=%d: failed to en ldo_vio18, ret=%d\n",
                __FUNCTION__, __LINE__, ret);
        }
#endif  // ldo control, otherwise always on
#endif  // gpio control power supply
    }

    mutex_unlock(&k250a_data.dev_ref_mutex);

    return count;
}

static ssize_t k250a_read(struct file *file, char __user *buf,
						size_t count, loff_t *ppos)
{
    pr_info("[%s]l=%d\n", __FUNCTION__, __LINE__);

    if (count == 0 || buf == NULL) {
        pr_err("[%s]l=%d: buf is NULL\n", __FUNCTION__, __LINE__);
        return -1;
    }

    mutex_lock(&k250a_data.dev_ref_mutex);

    if (k250a_data.k250a_is_open == K250A_OPEN) {
        copy_to_user(buf, "1", 1);
    } else {
        copy_to_user(buf, "0", 1);
    }

    mutex_unlock(&k250a_data.dev_ref_mutex);

    return count;
}

static int k250a_reset(void)
{
    pr_info("[%s]l=%d: before gpio=%d\n", __FUNCTION__, __LINE__,
        gpio_get_value(secrityic_en_gpio));
    gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_CLOSE);
    usleep_range(1000, 2000);
    gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_OPEN);
    pr_info("[%s]l=%d: after gpio=%d\n", __FUNCTION__, __LINE__,
        gpio_get_value(secrityic_en_gpio));
    usleep_range(15000, 20000);

    return 0;
}

static long k250a_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
    if (filp == NULL) {
        pr_err("[%s]l=%d: filp is NULL\n", __FUNCTION__, __LINE__);
        return -EINVAL;
    }

    if (_IOC_TYPE(cmd) != STAR_MAGIC_CODE) {
        pr_err("[%s]l=%d: invalid magic. cmd=0x%X Received=0x%X Expected=0x%X\n",
            __FUNCTION__, __LINE__, cmd, _IOC_TYPE(cmd), STAR_MAGIC_CODE);
        return -ENOTTY;
    }

    mutex_lock(&k250a_data.dev_ref_mutex);

    switch (cmd) {
    case STAR_RESET_INTERFACE:
        pr_err("[%s]l=%d: reset interface\n", __FUNCTION__, __LINE__);
        k250a_reset();
        break;
    default:
        pr_err("[%s]l=%d: no matching ioctl! 0x%X\n", __FUNCTION__, __LINE__, cmd);
        mutex_unlock(&k250a_data.dev_ref_mutex);
        return -EINVAL;
    }

    mutex_unlock(&k250a_data.dev_ref_mutex);

    return 0;
}

// devFS-/dev/k250a
struct file_operations k250a_node_ops = {
    .owner  = THIS_MODULE,
    .open = k250a_open,
    .release = k250a_release,
    .read = k250a_read,
    .write = k250a_write,
    .unlocked_ioctl = k250a_ioctl,
};

static int major;
static struct class *k250a_node_class = NULL;

static const struct of_device_id k250a_match_table[] = {
    { .compatible = "sec,k250a",},
    {},
};

static int k250a_probe(struct platform_device *pdev)
{
    int ret = 0;
    struct device *k250a_node_device = NULL;

    pr_err("[%s]l=%d: start\n", __FUNCTION__, __LINE__);

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

    mutex_init(&k250a_data.dev_ref_mutex);

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
#if defined(SECURITY_IC_ENABLE_I2C_VIA_I2C_CLK) // CLK_IFR_I2C6 for TEE
    /* A06 code for SR-AL7160A-01-06 by gaochao at 20240424 start */
    k250a_data.k250a_clk_main = devm_clk_get(&pdev->dev, "k250a-clk-main");
    if (IS_ERR(k250a_data.k250a_clk_main)) {
        ret = PTR_ERR(k250a_data.k250a_clk_main);
        pr_err("failed to get k250a_clk_main, ret is %d\n", ret);
        return ret;
    }

    k250a_data.k250a_clk_dma = devm_clk_get(&pdev->dev, "k250a-clk-dma");
    if (IS_ERR(k250a_data.k250a_clk_dma)) {
        ret = PTR_ERR(k250a_data.k250a_clk_dma);
        pr_err("failed to get k250a_clk_dma, ret is %d\n", ret);
        return ret;
    }

    // clk_prepare_enable(k250a_data.k250a_clk);
    /* A06 code for SR-AL7160A-01-06 by gaochao at 20240424 end */
#endif  // SECURITY_IC_ENABLE_I2C_VIA_I2C_CLK
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
    /* A06 code for AL7160A-1233 by gaochao at 20240625 start */
    int ret = 0;
    /* A06 code for AL7160A-1233 by gaochao at 20240625 end */
    pr_info("[%s]l=%d\n", __FUNCTION__, __LINE__);

    mutex_destroy(&k250a_data.dev_ref_mutex);
    device_destroy(k250a_node_class, MKDEV(major, 0));
    class_destroy(k250a_node_class);
    unregister_chrdev(major, "k250a_node");
#if defined(SECURITY_IC_POWER_SUPPLY_VIA_GPIO) // gpio control power supply
    /* A06 code for AL7160A-1233 by gaochao at 20240625 start */
    ret = gpio_direction_output(secrityic_en_gpio, GPIO_CTRL_POWER_CLOSE);
    if (ret != 0) {
        pr_err("[%s][%d]: failed to set gpio, ret=%d\n", __FUNCTION__, __LINE__, ret);
    }
    pr_info("[%s]l=%d: gpio control power supply==>power off\n",
        __FUNCTION__, __LINE__);
    /* A06 code for AL7160A-1233 by gaochao at 20240625 end */
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
    int rc = 0;

    pr_err("[%s]l=%d\n", __FUNCTION__, __LINE__);
    rc = platform_driver_register(&k250a_driver);
    if (rc) {
        pr_err("[%s]l=%d: Failed to register k250a driver\n",
            __FUNCTION__, __LINE__);
        return rc;
    }

    return 0;
}

static void __exit k250a_exit(void)
{
    pr_err("[%s]l=%d\n", __FUNCTION__, __LINE__);

    return platform_driver_unregister(&k250a_driver);
}

late_initcall(k250a_init);
module_exit(k250a_exit);
MODULE_LICENSE("GPL");