/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": %s: " fmt, __func__

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/leds-s2mu106.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/sec_batt.h>

#include "flashlight-core.h"
#include "flashlight-dt.h"

/* MUIC header file */
#include <linux/ssdev_in_mtk/muic/muic.h>
//#include <linux/ssdev_in_mtk/muic/s2mu106-muic.h>
#include <linux/ssdev_in_mtk/muic/s2mu106-muic-hv.h>
#include <linux/ssdev_in_mtk/usb/typec/slsi/usbpd_ext.h>

#define S2MU106_NAME "flashlights-s2mu106"

/* define level */
#define S2MU106_LEVEL_NUM 32 
#define S2MU106_LEVEL_TORCH 5
#define S2MU106_HW_TIMEOUT 992 /* ms */

#define VOLTAGE_DECREASE 0
#define VOLTAGE_INCREASE 1
#define VOLTAGE_NONE 2
#define VOLTAGE_PRE_ON_DONE 3

/* define mutex and work queue */
static DEFINE_MUTEX(s2mu106_mutex);
static struct work_struct s2mu106_work;
static struct work_struct s2mu106_voltage_work;

static int volt_direction = VOLTAGE_NONE;
/* define usage count */
static int use_count;
static int s2mu106_current_level;

/* platform data */
struct s2mu106_platform_data {
    int channel_num;
    struct flashlight_device_id *dev_id;
};


static int s2mu106_is_torch(int level)
{
    if (level > S2MU106_LEVEL_TORCH)
        return 0;

    return 1;
}

static int s2mu106_verify_level(int level)
{
    if (level < 0)
        level = 0;
    else if (level >= S2MU106_LEVEL_NUM)
        level = S2MU106_LEVEL_NUM - 1;

    return level;
}

/* set flashlight level */
static void s2mu106_set_level(int level)
{
    level = s2mu106_verify_level(level);
    s2mu106_current_level = level;
}

static void s2mu106_adjust_voltage()
{
    if(volt_direction == VOLTAGE_DECREASE) {
        muic_afc_set_voltage(5);
        pdo_ctrl_by_flash(1);
        pr_info("[%s]%d Down Voltage set On \n" ,__func__,__LINE__);
    }
    else if(volt_direction == VOLTAGE_INCREASE) {
        volt_direction = VOLTAGE_NONE;
        pdo_ctrl_by_flash(0);
        muic_afc_set_voltage(9);
        pr_info("[%s]%d Down Voltage set Clear \n" ,__func__,__LINE__);
    }
}
/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer s2mu106_timer;
static unsigned int s2mu106_timeout_ms;

static void s2mu106_work_disable(struct work_struct *data)
{
    pr_debug("work queue callback\n");
    s2mu106_enable_external(0, 0);
}

static enum hrtimer_restart s2mu106_timer_func(struct hrtimer *timer)
{
    schedule_work(&s2mu106_work);
    volt_direction = VOLTAGE_INCREASE;
    schedule_work(&s2mu106_voltage_work);
    return HRTIMER_NORESTART;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int s2mu106_ioctl(unsigned int cmd, unsigned long arg)
{
    struct flashlight_dev_arg *fl_arg;
    int channel;
    ktime_t ktime;
    unsigned int s;
    unsigned int ns;

    fl_arg = (struct flashlight_dev_arg *)arg;
    channel = fl_arg->channel;

    switch (cmd) {
    case FLASH_IOC_SET_TIME_OUT_TIME_MS:
        pr_debug("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
                channel, (int)fl_arg->arg);
        s2mu106_timeout_ms = fl_arg->arg;
        break;

    case FLASH_IOC_SET_DUTY:
        pr_debug("FLASH_IOC_SET_DUTY(%d): %d\n",
                channel, (int)fl_arg->arg);
        s2mu106_set_level(fl_arg->arg);
        break;

    case FLASH_IOC_SET_ONOFF:
        pr_debug("FLASH_IOC_SET_ONOFF(%d): %d\n",
                channel, (int)fl_arg->arg);
        if (fl_arg->arg == 1) {
            if (s2mu106_timeout_ms) {
                s = s2mu106_timeout_ms / 1000;
                ns = s2mu106_timeout_ms % 1000 * 1000000;
                ktime = ktime_set(s, ns);
                hrtimer_start(&s2mu106_timer, ktime,
                        HRTIMER_MODE_REL);
            }
            //0 based indexing for level
            if(s2mu106_is_torch(s2mu106_current_level)) {
                pr_debug("TORCH MODE");
#ifdef CONFIG_MTK_A41_JPN_PRJ
                if(s2mu106_current_level == 0) {
                    pr_debug("A41 JPN VIDEO TORCH MODE - SPECIAL CASE");
                    s2mu106_enable_external(1, 60);
                }
                else
#endif
               s2mu106_enable_external(1, (s2mu106_current_level + 1) * 50);
            }
            else {
                pr_debug("FLASH MODE");
               s2mu106_enable_external(2, (s2mu106_current_level + 1) * 50);
            }
        } else {
            s2mu106_enable_external(0, 0);
            hrtimer_cancel(&s2mu106_timer);
            if(volt_direction == VOLTAGE_PRE_ON_DONE) {
                volt_direction = VOLTAGE_INCREASE;
                schedule_work(&s2mu106_voltage_work);
            }
        }
        break;
    
    case FLASH_IOC_PRE_ON:
        pr_debug("FLASH_IOC_PRE_ON(%d)\n", channel);
	if(volt_direction == VOLTAGE_DECREASE) {
	    volt_direction = VOLTAGE_PRE_ON_DONE;
	}
        break;

    case FLASH_IOC_GET_DUTY_NUMBER:
        pr_debug("FLASH_IOC_GET_DUTY_NUMBER(%d)\n", channel);
        fl_arg->arg = S2MU106_LEVEL_NUM;
        break;

    case FLASH_IOC_GET_MAX_TORCH_DUTY:
        pr_debug("FLASH_IOC_GET_MAX_TORCH_DUTY(%d)\n", channel);
        fl_arg->arg = S2MU106_LEVEL_TORCH - 1;
        break;

    case FLASH_IOC_GET_DUTY_CURRENT:
        fl_arg->arg = s2mu106_verify_level(fl_arg->arg);
        pr_debug("FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n",
                channel, (int)fl_arg->arg);
        fl_arg->arg = ((fl_arg->arg)+1) * 50;
        break;

    case FLASH_IOC_GET_HW_TIMEOUT:
        pr_debug("FLASH_IOC_GET_HW_TIMEOUT(%d)\n", channel);
        fl_arg->arg = S2MU106_HW_TIMEOUT;
        break;

    case FLASH_IOC_SET_SCENARIO:
        pr_debug("FLASH_IOC_SET_SCENARIO(%d)\n", channel);
        if(volt_direction == VOLTAGE_DECREASE) {
            volt_direction = VOLTAGE_INCREASE;
            schedule_work(&s2mu106_voltage_work);
        }
        break;

    case FLASH_IOC_SET_VOLTAGE:
        pr_err("FLASH_IOC_SET_VOLTAGE(%d)\n", channel);
	volt_direction = VOLTAGE_DECREASE;
	schedule_work(&s2mu106_voltage_work);
        break;

    default:
        pr_info("No such command and arg(%d): (%d, %d)\n",
                channel, _IOC_NR(cmd), (int)fl_arg->arg);
        return -ENOTTY;
    }

    return 0;
}

static int s2mu106_open(void)
{
    /* Move to set driver for saving power */
    return 0;
}

static int s2mu106_release(void)
{
    /* Move to set driver for saving power */
    return 0;
}

static int s2mu106_set_driver(int set)
{
    /* set chip and usage count */
    mutex_lock(&s2mu106_mutex);
    if (set) {
        use_count++;
        pr_debug("Set driver: %d\n", use_count);
    } else {
        use_count--;
        if (!use_count)
            s2mu106_enable_external(0, 0);
        if (use_count < 0)
            use_count = 0;
        pr_debug("Unset driver: %d\n", use_count);
    }
    mutex_unlock(&s2mu106_mutex);

    return 0;
}

static ssize_t s2mu106_strobe_store(struct flashlight_arg arg)
{
    s2mu106_set_driver(1);
    s2mu106_set_level(arg.level);
    s2mu106_timeout_ms = 0;
    s2mu106_set_driver(0);

    return 0;
}

static struct flashlight_operations s2mu106_ops = {
    s2mu106_open,
    s2mu106_release,
    s2mu106_ioctl,
    s2mu106_strobe_store,
    s2mu106_set_driver
};

static int s2mu106_parse_dt(struct device *dev,
        struct s2mu106_platform_data *pdata)
{
    struct device_node *np, *cnp;
    u32 decouple = 0;
    int i = 0;

    if (!dev || !dev->of_node || !pdata)
        return -ENODEV;

    np = dev->of_node;

    pdata->channel_num = of_get_child_count(np);
    if (!pdata->channel_num) {
        pr_info("Parse no dt, node.\n");
        return 0;
    }
    pr_info("Channel number(%d).\n", pdata->channel_num);

    if (of_property_read_u32(np, "decouple", &decouple))
        pr_info("Parse no dt, decouple.\n");

    pdata->dev_id = devm_kzalloc(dev,
            pdata->channel_num *
            sizeof(struct flashlight_device_id),
            GFP_KERNEL);
    if (!pdata->dev_id)
        return -ENOMEM;

    for_each_child_of_node(np, cnp) {
        if (of_property_read_u32(cnp, "type", &pdata->dev_id[i].type))
            goto err_node_put;
        if (of_property_read_u32(cnp, "ct", &pdata->dev_id[i].ct))
            goto err_node_put;
        if (of_property_read_u32(cnp, "part", &pdata->dev_id[i].part))
            goto err_node_put;
        snprintf(pdata->dev_id[i].name, FLASHLIGHT_NAME_SIZE,
                S2MU106_NAME);
        pdata->dev_id[i].channel = i + 1;
        pdata->dev_id[i].decouple = decouple;

        pr_info("Parse dt (type,ct,part,name,channel,decouple)=(%d,%d,%d,%s,%d,%d).\n",
                pdata->dev_id[i].type, pdata->dev_id[i].ct,
                pdata->dev_id[i].part, pdata->dev_id[i].name,
                pdata->dev_id[i].channel,
                pdata->dev_id[i].decouple);
        i++;
    }

    return 0;

err_node_put:
    of_node_put(cnp);
    return -EINVAL;
}

/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int s2mu106_probe(struct platform_device *pdev)
{
    struct s2mu106_platform_data *pdata = dev_get_platdata(&pdev->dev);
    int err;
    int i;

    pr_debug("Probe start.\n");

    if (!pdata) {
        pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
        if (!pdata) {
            err = -ENOMEM;
            goto err_free;
        }
        pdev->dev.platform_data = pdata;
        err = s2mu106_parse_dt(&pdev->dev, pdata);
        if (err)
            goto err_free;
    }

    /* init work queue */
    INIT_WORK(&s2mu106_work, s2mu106_work_disable);
    INIT_WORK(&s2mu106_voltage_work, s2mu106_adjust_voltage);

    /* init timer */
    hrtimer_init(&s2mu106_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    s2mu106_timer.function = s2mu106_timer_func;
    s2mu106_timeout_ms = S2MU106_HW_TIMEOUT;

    /* clear usage count */
    use_count = 0;

    /* register flashlight device */
    if (pdata->channel_num) {
        for (i = 0; i < pdata->channel_num; i++)
            if (flashlight_dev_register_by_device_id(
                        &pdata->dev_id[i],
                        &s2mu106_ops)) {
                err = -EFAULT;
                goto err_free;
            }
    } else {
        if (flashlight_dev_register(S2MU106_NAME, &s2mu106_ops)) {
            err = -EFAULT;
            goto err_free;
        }
    }

    pr_debug("Probe done.\n");
    return 0;
err_free:
    return err;
}

static int s2mu106_remove(struct platform_device *pdev)
{
    struct s2mu106_platform_data *pdata = dev_get_platdata(&pdev->dev);
    int i;

    pr_debug("Remove start.\n");

    /* unregister flashlight device */
    if (pdata && pdata->channel_num)
        for (i = 0; i < pdata->channel_num; i++)
            flashlight_dev_unregister_by_device_id(
                    &pdata->dev_id[i]);
    else
        flashlight_dev_unregister(S2MU106_NAME);

    /* flush work queue */
    flush_work(&s2mu106_work);
    flush_work(&s2mu106_voltage_work);

    pr_debug("Remove done.\n");

    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id s2mu106_of_match[] = {
    {.compatible = S2MU106_DTNAME},
    {},
};
MODULE_DEVICE_TABLE(of, s2mu106_of_match);
#else
static struct platform_device s2mu106_platform_device[] = {
    {
        .name = S2MU106_NAME,
        .id = 0,
        .dev = {}
    },
    {}
};
MODULE_DEVICE_TABLE(platform, s2mu106_platform_device);
#endif

static struct platform_driver s2mu106_platform_driver = {
    .probe = s2mu106_probe,
    .remove = s2mu106_remove,
    .driver = {
        .name = S2MU106_NAME,
        .owner = THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = s2mu106_of_match,
#endif
    },
};

static int __init flashlight_s2mu106_init(void)
{
    int ret;

    pr_debug("Init start.\n");

#ifndef CONFIG_OF
    ret = platform_device_register(&s2mu106_platform_device);
    if (ret) {
        pr_info("Failed to register platform device\n");
        return ret;
    }
#else
    ret = platform_driver_register(&s2mu106_platform_driver);
    if (ret) {
        pr_info("Failed to register platform driver\n");
        return ret;
    }
#endif

    pr_debug("Init done.\n");

    return 0;
}

static void __exit flashlight_s2mu106_exit(void)
{
    pr_debug("Exit start.\n");

    platform_driver_unregister(&s2mu106_platform_driver);

    pr_debug("Exit done.\n");
}

module_init(flashlight_s2mu106_init);
module_exit(flashlight_s2mu106_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Civashritt A B <ab.civa@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG S2MU106 FlashLight Driver");

