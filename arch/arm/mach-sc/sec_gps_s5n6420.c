#include <linux/init.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <mach/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
//#include <mach/regulator.h>
#include <linux/clk.h>
#include <linux/gpio.h>

#include <asm/sec/sec_debug.h>

static struct device *gps_dev;

static int gps_clk_init(void)
{
    struct clk *gps_clk;
    struct clk *clk_parent;
    const char *clk_32k= "clk_aux0";

    gps_clk = clk_get(NULL, clk_32k);
    if (IS_ERR(gps_clk)) {
        printk("%s: failed to get clk_aux0.\n", __func__);
        return -1;
    }
    clk_parent = clk_get(NULL, "ext_32k");
    if (IS_ERR(clk_parent)) {
        printk("%s: failed to get parent ext_32k.\n", __func__);
        return -1;
    } else {
        clk_set_parent(gps_clk, clk_parent);
        clk_set_rate(gps_clk, 32000);
        if(!clk_prepare_enable(gps_clk))
            pr_info("%s: %s enabled!!\n", __func__, clk_32k);
        //clk_enable(gps_clk);
    }
    return 0;
}

static int gps_regulator_on(struct device_node *root_node)
{
    static struct regulator *gps_regulator = NULL;
    static struct regulator *tcxo_regulator = NULL;
    static unsigned int gps_regulator_volt = 1800000;
    static unsigned int tcxo_regulator_volt = 0;
    const char *reg = NULL;
    const char *reg_tcxo = NULL;

    /* regulator enable from dts */
    if(of_property_read_string(root_node, "gps-regulator", &reg)) {
        printk("%s: Failed to get main-regulator from dts.\n", __func__);
        return -1;
    }

    if(gps_regulator == NULL) {
        gps_regulator = regulator_get(NULL, reg);
        if (IS_ERR(gps_regulator)) {
            gps_regulator = NULL;
            return -1;
        }
    }

     regulator_set_voltage(gps_regulator, gps_regulator_volt, gps_regulator_volt);
     if(!regulator_enable(gps_regulator))
        pr_info("%s: (%s) main(%d uV) regulator turned ON!!\n", __func__, reg, gps_regulator_volt);

     if(of_property_read_string(root_node, "tcxo-regulator", &reg_tcxo)) {
        printk("%s: tcxo-regulator field is empty from dts.\n", __func__);
        return 0;
    }

    if(tcxo_regulator == NULL) {
        tcxo_regulator = regulator_get(NULL, reg_tcxo);
        if(IS_ERR(tcxo_regulator)) {
            tcxo_regulator = NULL;
            return 0;
        }
    }

    if(of_property_read_u32(root_node, "tcxo-regulator-volt", &tcxo_regulator_volt)) {
        printk("%s: tcxo-requlator-volt field is empty from dts.\n", __func__);
        return 0;
    }
    regulator_set_voltage(tcxo_regulator, tcxo_regulator_volt, tcxo_regulator_volt);
    if(!regulator_enable(tcxo_regulator))
        pr_info("%s: (%s) tcxo(%d uV) regulator turned ON!!\n", __func__, reg_tcxo, tcxo_regulator_volt);

    return 0;
}

static unsigned int gps_pwr_on = 0;
static unsigned int gps_reset = 0;

static int __init gps_s5n6420_init(void)
{
    const char *gps_node = "lsi,s5n6420";
    const char *gps_pwr_en = "gps-pwr-en";
    const char *gps_nRst = "gps-reset";
    struct device_node *root_node = NULL;
    int ret = 0;

    pr_info("%s\n", __func__);

    BUG_ON(!sec_class);
    gps_dev = device_create(sec_class, NULL, 0, NULL, "gps");
    BUG_ON(!gps_dev);

    root_node = of_find_compatible_node(NULL, NULL, gps_node);
    if(!root_node) {
        printk("failed to get device node of %s\n", gps_node);
        ret = -ENODEV;
        goto err_sec_device_create;
    }
    if(gps_regulator_on(root_node)) {
        printk("gps_regualtor_on() failed\n");
        ret = -ENODEV;
        goto err_find_node;
    }
    if(gps_clk_init()) {
        printk("gps_clk_init() failed\n");
        ret = -ENODEV;
        goto err_find_node;
    }
    gps_pwr_on = of_get_named_gpio(root_node, gps_pwr_en, 0);
    if(!gpio_is_valid(gps_pwr_on)) {
        printk("%s: Invalid gpio pin : %d\n", __func__, gps_pwr_on);
        ret = -ENODEV;
        goto err_find_node;
    }
    if (gpio_request(gps_pwr_on, "GPS_PWR_EN")) {
        printk("fail to request gpio(GPS_PWR_EN)\n");
        ret = -ENODEV;
        goto err_find_node;
    }

	gps_reset = of_get_named_gpio(root_node, gps_nRst, 0);
    if(!gpio_is_valid(gps_reset)) {
        printk("%s: Invalid gpio pin : %d\n", __func__, gps_nRst);
        ret = -ENODEV;
        goto err_find_node;
    }
    if (gpio_request(gps_reset, "GPS_RESET")) {
        printk("fail to request gpio(GPS_RESET)\n");
        ret = -ENODEV;
        goto err_find_node;
    }

    gpio_direction_output(gps_pwr_on, 0);
    gpio_export(gps_pwr_on, 1);
    gpio_export_link(gps_dev, "GPS_PWR_EN", gps_pwr_on);

    gpio_direction_output(gps_reset, 1);
    gpio_export(gps_reset, 1);
    gpio_export_link(gps_dev, "GPS_RESET", gps_reset);

    return 0;

err_find_node:
    of_node_put(root_node);
err_sec_device_create:
    device_destroy(sec_class, gps_dev->devt);
    return ret;
}

device_initcall(gps_s5n6420_init);
