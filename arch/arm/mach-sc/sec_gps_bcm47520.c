#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <asm/uaccess.h>
#include "devices.h"
#include <mach/board.h>
#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <mach/pinmap.h>
#include <linux/proc_fs.h>
#include <linux/irq.h>

static struct device *gps_dev;
extern struct class *sec_class;


static void gps_clk_init(void)
{
    struct clk *gps_clk;
    struct clk *clk_parent;
    const char *clk_32k= "clk_aux0";

    gps_clk = clk_get(NULL, clk_32k);
    if (IS_ERR(gps_clk)) {
        printk("clock: failed to get clk_aux0\n");
    }
    clk_parent = clk_get(NULL, "ext_32k");
    if (IS_ERR(clk_parent)) {
        printk("failed to get parent ext_32k\n");
    }

    clk_set_parent(gps_clk, clk_parent);
    clk_set_rate(gps_clk, 32000);
    if(!clk_prepare_enable(gps_clk))
        printk("%s: %s enabled!!\n", __func__, clk_32k);
    //clk_enable(gps_clk);
}

static int gps_regulator_on(void)
{
    int ret = 0;
    const char *gps_node = "broadcom,bcm47520";
    struct device_node *root_node = NULL;
    static struct regulator *gps_regulator = NULL;
    const char *reg = NULL;

    root_node = of_find_compatible_node(NULL, NULL, gps_node);
    if(!root_node) {
        WARN(1, "[GPS] failed to get device node of bcm47520\n");
        return -ENODEV;
    }
    /* regulator enable from dts */
    if(of_property_read_string(root_node, "gps-regulator", &reg)) {
        pr_err("%s: main(1.8v) regulator field is not exsit\n", __func__);
        ret = -ENODEV;
        goto err_find_node;
    } else {
        gps_regulator = regulator_get(NULL, reg);
        if (IS_ERR(gps_regulator)){
            gps_regulator = NULL;
            return -EIO;
        } else {
            regulator_set_voltage(gps_regulator, 1800000, 1800000);
            if(!regulator_enable(gps_regulator))
                printk("%s: (%s) main(1.8v) regulator turned ON!!\n", __func__, reg);
        }
    }
    return 0;

err_find_node:
    of_node_put(root_node);
    return ret;
}

static unsigned int gps_pwr_on = 0;

static int __init gps_bcm47520_init(void)
{
    pr_info("%s\n", __func__);
    gps_regulator_on();
    gps_clk_init();

    int ret = 0;
    const char *gps_node = "broadcom,bcm47520";
    const char *gps_pwr_en = "gps-pwr-en";
    struct device_node *root_node = NULL;

    BUG_ON(!sec_class);
    gps_dev = device_create(sec_class, NULL, 0, NULL, "gps");
    BUG_ON(!gps_dev);

    root_node = of_find_compatible_node(NULL, NULL, gps_node);
    if(!root_node) {
        WARN(1, "failed to get device node of bcm4752\n");
        ret = -ENODEV;
        goto err_sec_device_create;
    }

    gps_pwr_on = of_get_named_gpio(root_node, gps_pwr_en, 0);
    if(!gpio_is_valid(gps_pwr_on)) {
        WARN(1, "Invalid gpio pin : %d\n", gps_pwr_on);
        ret = -ENODEV;
        goto err_find_node;
    }
    if (gpio_request(gps_pwr_on, "GPS_PWR_EN")) {
        WARN(1, "fail to request gpio(GPS_PWR_EN)\n");
        ret = -ENODEV;
        goto err_find_node;
    }
    gpio_direction_output(gps_pwr_on, 0);
    gpio_export(gps_pwr_on, 1);
    gpio_export_link(gps_dev, "GPS_PWR_EN", gps_pwr_on);

    return 0;

err_find_node:
    of_node_put(root_node);
err_sec_device_create:
    device_destroy(gps_dev, gps_dev->devt);
    return ret;
}

device_initcall(gps_bcm47520_init);

