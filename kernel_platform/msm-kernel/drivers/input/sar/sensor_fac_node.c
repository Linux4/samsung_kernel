//#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include "sensor_fac_node.h"

//sar point
sar_driver_t g_facnode;
EXPORT_SYMBOL(g_facnode);

//sar create node
#define FAC_NODE_NUM  4
struct class *g_sensor_factory;
struct device *g_sar_node;
static int g_sensor_flag[FAC_NODE_NUM] = {0};
static bool g_sar_cali_flag = false;

//get sar name
static ssize_t sar_name_show(struct device *dev,
                        struct device_attribute *attr, char *buf)
{
    if (g_facnode.sar_name_drv) {
        pr_err("[SENS_SAR] " "%s: sar_name in node = %s\n", __func__, g_facnode.sar_name_drv);
        return snprintf(buf, PAGE_SIZE, "%s\n", g_facnode.sar_name_drv);
    }

    pr_err("[SENS_SAR] " "sar name error\n");

    return 0;
}
static DEVICE_ATTR(sar_name, 0444, sar_name_show, NULL);

//sar cali
ssize_t sar_cali_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", g_sar_cali_flag);
}

static ssize_t sar_cali_store(struct device *dev,
                        struct device_attribute *attr,
                        const char *buf, size_t count)
{
    if (g_facnode.get_cali) {
        g_facnode.get_cali();
        g_sar_cali_flag = true;
    } else {
        pr_err("[SENS_SAR] " "%s: sar cali fail\n", __func__);
    }

    return count;
}
static DEVICE_ATTR(sar_cali, 0664, sar_cali_show, sar_cali_store);

//sar enable
static ssize_t sar_enable_store(struct device *dev,
                        struct device_attribute *attr,
                        const char *buf, size_t count)
{
    if (g_facnode.set_enable) {
        g_facnode.set_enable(buf, count);
    } else {
        pr_err("[SENS_SAR] " "%s: sar enable fail\n", __func__);
    }

    return count;
}

ssize_t sar_enable_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    int len = 0;

    if (g_facnode.get_enable) {
        len = g_facnode.get_enable(buf);
    } else {

        pr_err("[SENS_SAR] " "%s: cat enable fail\n", __func__);
    }

    return len;
}
static DEVICE_ATTR(sar_enable, 0664, sar_enable_show, sar_enable_store);

static struct device_attribute *sensor_attrs[] = {
    &dev_attr_sar_name,
    &dev_attr_sar_cali,
    &dev_attr_sar_enable,
    NULL,
};

static int sensor_node_init(void)
{
    int i = 0;
    //sys/class/sensor_factory/
    g_sensor_factory = class_create(THIS_MODULE, "sensor_factory");
    if (IS_ERR(g_sensor_factory)) {
        pr_err("[SENS_SAR] " "%s: create sensor_factory is failed.\n", __func__);
        return PTR_ERR(g_sensor_factory);
    }

    //sys/class/sensor_factory/sar_node/
    g_sar_node = device_create(g_sensor_factory, NULL, 0, NULL, "sar_node");
    if (IS_ERR(g_sar_node)) {
        pr_err("[SENS_SAR] " "%s: device_create failed!\n", __func__);
        device_destroy(g_sensor_factory, 0);
        class_destroy(g_sensor_factory);
        return PTR_ERR(g_sar_node);
    }

    for (i = 0; sensor_attrs[i] != NULL; i++) {
        g_sensor_flag[i] = device_create_file(g_sar_node, sensor_attrs[i]);

        if (g_sensor_flag[i]) {
            pr_err("[SENS_SAR] " "%s: fail device_create_file, sensor_attrs[%d])\n", __func__, i);
            device_destroy(g_sensor_factory, 0);
            class_destroy(g_sensor_factory);
            return g_sensor_flag[i];
        }
    }

    pr_err("[SENS_SAR] " "%s: creat sar sensor factory test nodes success.\n", __func__);

    return 0;
}

static void sensor_node_exit(void)
{
    int i = 0;

    for (i = 0; sensor_attrs[i] != NULL; i++) {
        if (!g_sensor_flag[i]) {
            device_remove_file(g_sar_node, sensor_attrs[i]);
            device_destroy(g_sensor_factory, 0);
            class_destroy(g_sensor_factory);
        }
    }

    g_sensor_factory = NULL;
}

module_init(sensor_node_init);
module_exit(sensor_node_exit);

MODULE_DESCRIPTION("sensor factory test node");
MODULE_AUTHOR("sar sensor");
MODULE_LICENSE("GPL");
