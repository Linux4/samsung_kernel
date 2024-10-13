//#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include "sensor_user_node.h"

#define USER_TEST_NODE 10
//sar create node
struct class *sensor_usertest = NULL;
struct device *sar_node = NULL;
int sensor_flag[USER_TEST_NODE] = {0};

//sar point
struct sar_driver_func sar_func;

//sar onoff
static ssize_t onoff_store(struct device *dev,
                        struct device_attribute *attr,
                        const char *buf, size_t count)
{
    if (sar_func.set_onoff) {
        sar_func.set_onoff(buf, count);
    } else {
        pr_err("%s: sar_name :%s set_onoff fail\n", __func__);
    }

    return count;
}

ssize_t onoff_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    int len = 0;

    if (sar_func.get_onoff) {
        len = sar_func.get_onoff(buf);
    }

    pr_err("%s: sar_name :%s get_onoff stauts\n", __func__);

    return len;
}
static DEVICE_ATTR(onoff, 0664, onoff_show, onoff_store);

static struct device_attribute *sensor_attrs[] = {
    &dev_attr_onoff,
    NULL,
};

static int sensor_node_init(void)
{
    int i = 0;
    //sys/class/sensor_usertest/
    sensor_usertest = class_create(THIS_MODULE, "sensor_usertest");
    if (IS_ERR(sensor_usertest)) {
        pr_err("%s: create sensor_usertest is failed.\n", __func__);
        return PTR_ERR(sensor_usertest);
    }

    //sys/class/sensor_usertest/sar_node/
    sar_node = device_create(sensor_usertest, NULL, 0, NULL, "sar_node");
    if (IS_ERR(sar_node)) {
        pr_err("%s: device_create failed!\n", __func__);
        device_destroy(sensor_usertest, 0);
        class_destroy(sensor_usertest);
        return PTR_ERR(sar_node);
    }

    for (i = 0; sensor_attrs[i] != NULL; i++) {
        sensor_flag[i] = device_create_file(sar_node, sensor_attrs[i]);

        if (sensor_flag[i]) {
            pr_err("%s: fail device_create_file (sensor_flag, sensor_attrs[%d])\n", __func__, i);
            device_destroy(sensor_usertest, 0);
            class_destroy(sensor_usertest);
            return sensor_flag[i];
        }
    }

    return 0;
}

static void sensor_node_exit(void)
{
    int i = 0;

    for (i = 0; sensor_attrs[i] != NULL; i++) {
        device_remove_file(sar_node, sensor_attrs[i]);
    }
    device_destroy(sensor_usertest, 0);
    class_destroy(sensor_usertest);
    sensor_usertest = NULL;
}

module_init(sensor_node_init);
module_exit(sensor_node_exit);

MODULE_DESCRIPTION("sensor user test node");
MODULE_AUTHOR("sar sensor");
MODULE_LICENSE("GPL");
