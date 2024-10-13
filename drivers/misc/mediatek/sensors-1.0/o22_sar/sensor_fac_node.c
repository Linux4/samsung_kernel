//#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include "sensor_fac_node.h"

//sar create node
struct class *sensor_factory = NULL;
struct device *sar_node = NULL;
int sensor_flag[] = {0};

//sar point
struct sar_driver sar_drv;

//get sar name
static ssize_t sar_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    if (sar_drv.sar_name_drv) {
        pr_err("%s: sar_name in node = %s\n", __func__, sar_drv.sar_name_drv);
        return snprintf(buf, PAGE_SIZE, "%s\n", sar_drv.sar_name_drv);
    }

    pr_err("sar name error\n");

    return 0;
}
static DEVICE_ATTR(sar_name, 0664, sar_name_show, NULL);

//sar cali
static ssize_t sar_cali_store(struct device *dev,
                        struct device_attribute *attr,
                        const char *buf, size_t count)
{
    if (sar_drv.get_cali) {
        sar_drv.get_cali();
    } else {
        pr_err("%s: sar_name :%s cali fail\n", __func__, sar_drv.sar_name_drv);
    }

    return count;
}
static DEVICE_ATTR(sar_cali, 0664, NULL, sar_cali_store);

//sar enable
static ssize_t sar_enable_store(struct device *dev,
                        struct device_attribute *attr,
                        const char *buf, size_t count)
{
    if (sar_drv.set_enable) {
        sar_drv.set_enable(buf, count);
    } else {
        pr_err("%s: sar_name :%s enable fail\n", __func__, sar_drv.sar_name_drv);
    }

    return count;
}

ssize_t sar_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    int len = 0;

    if (sar_drv.get_enable) {
        len = sar_drv.get_enable(buf);
    }

    pr_err("%s: sar_name :%s cat enable stauts\n", __func__, sar_drv.sar_name_drv);

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
    sensor_factory = class_create(THIS_MODULE, "sensor_factory");
    if (IS_ERR(sensor_factory)) {
        pr_err("%s: create sensor_factory is failed.\n", __func__);
        return PTR_ERR(sensor_factory);
    }

    //sys/class/sensor_factory/sar_node/
    sar_node = device_create(sensor_factory, NULL, 0, NULL, "sar_node");
    if (IS_ERR(sar_node)) {
        pr_err("%s: device_create failed!\n", __func__);
        device_destroy(sensor_factory, 0);
        class_destroy(sensor_factory);
        return PTR_ERR(sar_node);
    }

    for (i = 0; sensor_attrs[i] != NULL; i++) {
        sensor_flag[i] = device_create_file(sar_node, sensor_attrs[i]);

        if (sensor_flag[i]) {
            pr_err("%s: fail device_create_file (sensor_flag, sensor_attrs[%d])\n", __func__, i);
            device_destroy(sensor_factory, 0);
            class_destroy(sensor_factory);
            return sensor_flag[i];
        }
    }

    return 0;
}

static void sensor_node_exit(void)
{
    int i = 0;

    for (i = 0; sensor_attrs[i] != NULL; i++) {
        if (!sensor_flag[i]) {
            device_remove_file(sar_node, sensor_attrs[i]);
            device_destroy(sensor_factory, 0);
            class_destroy(sensor_factory);
        }
    }

    sensor_factory = NULL;
}

module_init(sensor_node_init);
module_exit(sensor_node_exit);

MODULE_DESCRIPTION("sensor factory test node");
MODULE_AUTHOR("zhangziyi");
MODULE_LICENSE("GPL");
