#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/semaphore.h>
#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#include <linux/mutex.h>
#include <linux/of.h>

#include "wcn_core.h"

static struct platform_device *sprd_wcn_device;
static struct semaphore pa_enable_lock;
static DEFINE_MUTEX(bluetooth_ops_lock);
static volatile unsigned char pa_state = 0;
static volatile bool bt_state = 0;

void marlin_pa_enable(bool enable){
    int ret;
    const char *vdd_pa;
    static struct regulator *download_vdd = NULL;
    struct device_node *np;

    np = of_find_node_by_name(NULL, "sprd-marlin");
    if (!np) {
        printk(KERN_ERR"sprd-marlin not found");
	return;
    }

    ret = of_property_read_string(np, "vdd-pa", &vdd_pa);
    if (ret) {
        printk(KERN_ERR"get vdd-download failed");
	return;
    }

    down(&pa_enable_lock);

    if (enable) {
        if (pa_state == 0) {
           //TODO
        } else if (pa_state && pa_state < 0xFF) {
            pa_state++;
            printk(KERN_DEBUG "%s pa state: 0x%02x\n", __func__, pa_state);
            goto done;
        } else if (pa_state == 0xFF) {
            printk(KERN_ERR "%s error too many call invoke\n", __func__);
            goto done;
        }
    } else {
        if (pa_state == 1) {
            //TODO
        } else if (pa_state > 1) {
            pa_state--;
            printk(KERN_DEBUG "%s pa state: 0x%02x\n", __func__, pa_state);
            goto done;
        } else if (pa_state == 0) {
            printk(KERN_ERR "%s error pa is disable already\n", __func__);
            goto done;
        }
    }

    if (download_vdd == NULL) {
        download_vdd = regulator_get(NULL, vdd_pa);
        if (IS_ERR(download_vdd)) {
            printk(KERN_ERR "get regulator of vdd-pa  error!\n");
            goto done;
        }
        printk(KERN_DEBUG "get vdd-pa ok\n");
        if (enable) {
            printk(KERN_DEBUG "vdd-pa enable\n");
            regulator_set_voltage(download_vdd, 3300000, 3300000);
            ret = regulator_enable(download_vdd);
            pa_state++;
        } else if (regulator_is_enabled(download_vdd)) {
            printk(KERN_DEBUG "vdd-pa disable\n");
            ret = regulator_disable(download_vdd);
            pa_state--;
        }
    }

done:
    up(&pa_enable_lock);
}
EXPORT_SYMBOL(marlin_pa_enable);

static ssize_t get_pa_state(struct device *dev,
    struct device_attribute *attr, char *buf){
    return 0;
}

static ssize_t enable_pa(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count){
    int ret = count;

    if (count <= 0) {
        printk(KERN_ERR "%s parameters miss\n", __func__);
        goto done;
    }

    switch (buf[0]) {
        case COMMAND_PA_ENABLE:
            printk(KERN_INFO "%s pa enable\n", __func__);
            marlin_pa_enable(1);
            break;

        case COMMAND_PA_DISABLE:
            printk(KERN_INFO "%s pa disable\n", __func__);
            marlin_pa_enable(0);
            break;

        default:
            printk(KERN_ERR "%s unknow parameters: %c\n", __func__, buf[0]);
            break;
    }

done:
    return ret;
}

static DEVICE_ATTR(pa_enable, 0660,
    get_pa_state, enable_pa);


static struct attribute *wcn_pa_sysfs_entries[] = {
    &dev_attr_pa_enable.attr,
    NULL,
};

static struct attribute_group wcn_pa_attr_group = {
    .attrs = wcn_pa_sysfs_entries,
};

// bt
bool get_bt_state(void){
    bool state = 0;
    mutex_lock(&bluetooth_ops_lock);
    state = bt_state;
   // printk(KERN_INFO "%s get bt state: %d\n", __func__, state);
    mutex_unlock(&bluetooth_ops_lock);
    return state;
}
EXPORT_SYMBOL(get_bt_state);

static void marlin_bt_enable(bool enable){
    mutex_lock(&bluetooth_ops_lock);
    printk(KERN_INFO "%s enable bt: %d\n", __func__, enable);
    bt_state = enable;
    mutex_unlock(&bluetooth_ops_lock);
}

static ssize_t get_bluetooth_state(struct device *dev,
    struct device_attribute *attr, char *buf){
    return sprintf(buf, "%d\n", get_bt_state());
}

static ssize_t enable_bluetooth(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count){
    int ret = count;

    if (count <= 0) {
        printk(KERN_ERR "%s parameters miss\n", __func__);
        goto done;
    }

    switch (buf[0]) {
        case COMMAND_BT_ENABLE:
            printk(KERN_INFO "%s bt enable\n", __func__);
            marlin_bt_enable(1);
            break;

        case COMMAND_BT_DISABLE:
            printk(KERN_INFO "%s bt disable\n", __func__);
            marlin_bt_enable(0);
            break;

        default:
            printk(KERN_ERR "%s unknow parameters: %c\n", __func__, buf[0]);
            break;
    }

done:
    return ret;
}

static DEVICE_ATTR(bt_enable, 0660,
    get_bluetooth_state, enable_bluetooth);


static struct attribute *wcn_bluetooth_sysfs_entries[] = {
    &dev_attr_bt_enable.attr,
    NULL,
};

static struct attribute_group wcn_bluetooth_attr_group = {
    .attrs = wcn_bluetooth_sysfs_entries,
};

static int  sprd_wcn_probe(struct platform_device *pdev){
    int err = 0;

    sema_init(&pa_enable_lock, 1);
    err = sysfs_create_group(&pdev->dev.kobj,
    &wcn_pa_attr_group);
    if (err) {
        printk(KERN_ERR "%s create sysfs pa failed\n", __func__);
        goto error_remove_files;
    }

    err = sysfs_create_group(&pdev->dev.kobj,
    &wcn_bluetooth_attr_group);
    if (err) {
        printk(KERN_ERR "%s create sysfs bluetooth failed\n", __func__);
        goto error_remove_files;
    }

    return 0;

error_remove_files:
    sysfs_remove_group(&pdev->dev.kobj, &wcn_pa_attr_group);

    return err;
}

static int  sprd_wcn_remove(struct platform_device *pdev){
    sysfs_remove_group(&pdev->dev.kobj, &wcn_pa_attr_group);
    return 0;
}

static struct platform_driver sprd_wcn_driver = {
    .probe =   sprd_wcn_probe,
    .remove =  sprd_wcn_remove,
    .driver = {
        .owner = THIS_MODULE,
        .name = DEVICE_NAME,
    },
};

static int sprd_wcn_init(void){
    sprd_wcn_device = platform_device_register_simple(DEVICE_NAME, 0, NULL, 0);
    if (IS_ERR(sprd_wcn_device)) {
    platform_driver_unregister(&sprd_wcn_driver);
    return PTR_ERR(sprd_wcn_device);
    }
    return platform_driver_register(&sprd_wcn_driver);
}

static void sprd_wcn_exit(void){
    platform_driver_unregister(&sprd_wcn_driver);
    platform_device_unregister(sprd_wcn_device);
}

module_init(sprd_wcn_init);
module_exit(sprd_wcn_exit);
MODULE_DESCRIPTION("SPRD WCN Driver");
MODULE_AUTHOR("steven.chen");
