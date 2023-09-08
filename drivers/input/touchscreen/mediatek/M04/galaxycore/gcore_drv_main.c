/*
 * GalaxyCore touchscreen driver
 *
 * Copyright (C) 2021 GalaxyCore Incorporated
 *
 * Copyright (C) 2021 Neo Chen <neo_chen@gcoreinc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "gcore_drv_common.h"

#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/touchscreen_info.h>


#ifdef CONFIG_DRM
#include <drm/drm_panel.h>
struct drm_panel *gcore_active_panel;
#endif


#define GTP_TRANS_X(x, tpd_x_res, lcm_x_res)     (((x)*(lcm_x_res))/(tpd_x_res))
#define GTP_TRANS_Y(y, tpd_y_res, lcm_y_res)     (((y)*(lcm_y_res))/(tpd_y_res))

/*hs03s_NM code for SR-AL5625-01-624 by zhawei at 20220407 start*/
extern enum tp_module_used tp_is_used;
/*hs03s code for SR-AL5625-01-624 by zhawei at 20220407 end*/
/*hs03s_NM code for SR-AL5625-01-640 by yuli at 20220422 start*/
bool gcore_usb_detect_flag = 0;
/*hs03s_NM code for SR-AL5625-01-640 by yuli at 20220422 end*/
/*hs04 code for DEAL6398A-1521 by huangzhongjie at 20220903 start*/
#define PRESSURE_MAX 255
#define PRESSURE_DATA_ADDRESS 63
static u8 g_press = 0;
/*hs04 code for DEAL6398A-1521 by huangzhongjie at 20220903 end*/
/*hs03s_NM code for SR-AL5625-01-626  by zhawei at 20220407 start*/
#define HWINFO_NAME "tp_wake_switch"
static struct platform_device hwinfo_device = {
    .name = HWINFO_NAME,
    .id = -1,
};
/*hs03s_NM code for SR-AL5625-01-626  by zhawei at 20220407 end*/

struct gcore_exp_fn_data fn_data = {
    .initialized = false,
    .fn_inited = false,
};

struct gcore_exp_fn *p_fwu_fn = &fw_update_fn;
struct gcore_exp_fn *p_intf_fn = &fs_interf_fn;
struct gcore_exp_fn *p_mp_fn = &mp_test_fn;

#if GCORE_WDT_RECOVERY_ENABLE
int wdt_contin;
#endif

/*hs03s_NM code for SR-AL5625-01-626  by zhawei at 20220407 start*/
static ssize_t gc_ito_test_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    int count = 0;
    int ret = -1;

    GTP_DEBUG("Run ito test with LCM on\n");

    ret = gcore_start_mp_test();
    if (ret) {
        GTP_ERROR("selftest failed!");
        ret = 0;
    } else {
        GTP_DEBUG("selftest success!");
        ret = 1;
    }
    /*hs03s_NM code for SR-AL5625-01-640 by yuli at 20220428 start*/
    msleep(5);
    if (gcore_usb_detect_flag == true) {
        gcore_fw_event_notify(FW_CHARGER_PLUG);
        /*hs04 code for DEAL6398A-522 by huangzhongjie at 20220811 start*/
        fn_data.gdev->usb_detect_status = 1;
        /*hs04 code for DEAL6398A-522 by huangzhongjie at 20220811 end*/
    } else {
        gcore_fw_event_notify(FW_CHARGER_UNPLUG);
        /*hs04 code for DEAL6398A-522 by huangzhongjie at 20220811 start*/
        fn_data.gdev->usb_detect_status = 0;
        /*hs04 code for DEAL6398A-522 by huangzhongjie at 20220811 end*/
    }
    /*hs03s_NM code for SR-AL5625-01-640 by yuli at 20220428 end*/
    count = scnprintf(buf, PAGE_SIZE, "%d\n", ret);
    return count;
}

static ssize_t gc_ito_test_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
    return count;
}

static DEVICE_ATTR(factory_check, 0644, gc_ito_test_show, gc_ito_test_store);

static struct attribute *gc_ito_test_attributes[] = {
    &dev_attr_factory_check.attr,
    NULL,
};

static const struct attribute_group gc_ito_test_group = {
    .attrs = gc_ito_test_attributes,
};

static int gc_ito_test_node_init(struct platform_device *tp_device)
{
    int err = 0;
    err = platform_device_register(tp_device);
    if (err) {
        platform_device_unregister(tp_device);
        GTP_ERROR("platform device register failed!\n");
        return err;
            }
    err = sysfs_create_group(&tp_device->dev.kobj, &gc_ito_test_group);
    if (err != 0) {
        sysfs_remove_group(&tp_device->dev.kobj, &gc_ito_test_group);
        GTP_ERROR("sysfs creat group failed!\n");
        return err;
        }
    return 0;
}
/*hs03s_NM code for SR-AL5625-01-626 by zhawei at 20220407 end*/
/*hs03s_NM code for SR-AL5625-01-640 by yuli at 20220422 start*/
void gcore_input_enable_init(struct work_struct *work)
{
    int ret;

    ret = sysfs_create_link(&fn_data.gdev->sec.fac_dev->kobj, &fn_data.gdev->input_device->dev.kobj, "input");
    if (ret) {
        sysfs_delete_link(&fn_data.gdev->sec.fac_dev->kobj, &fn_data.gdev->input_device->dev.kobj, "input");
        GTP_ERROR("failed to creat sysfs link\n");
    }
}
/*hs03s_NM code for SR-AL5625-01-640 by yuli at 20220422 end*/

/*hs03s_NM code for SR-AL5625-01-640 by yuli at 20220422 start*/
static int gc_charger_notifier_callback(struct notifier_block *nb,
                                unsigned long val, void *v)
{
    int ret = 0;
    static int usb_connect = 0;
    struct power_supply *psy_usb = NULL;
    struct power_supply *psy_ac = NULL;
    union power_supply_propval prop_usb;
    union power_supply_propval prop_ac;

    psy_usb = power_supply_get_by_name("usb");
    if (!psy_usb) {
        GTP_ERROR("Couldn't get usb psy\n");
        return -EINVAL;
    }

    psy_ac = power_supply_get_by_name("ac");
    if (!psy_ac) {
        GTP_ERROR("Couldn't get ac psy\n");
        return -EINVAL;
    }

    if (!strcmp(psy_usb->desc->name, "usb")) {
        if (psy_usb && val == POWER_SUPPLY_PROP_STATUS) {
            ret = power_supply_get_property(psy_usb, POWER_SUPPLY_PROP_ONLINE, &prop_usb);
            if (ret < 0) {
                GTP_ERROR("Couldn't get POWER_SUPPLY_PROP_ONLINE rc=%d\n", ret);
                return ret;
            }
        }
    }

    if (!strcmp(psy_ac->desc->name, "ac")) {
        if (psy_ac && val == POWER_SUPPLY_PROP_STATUS) {
            ret = power_supply_get_property(psy_ac, POWER_SUPPLY_PROP_ONLINE, &prop_ac);
            if (ret < 0) {
                GTP_ERROR("Couldn't get POWER_SUPPLY_PROP_ONLINE rc=%d\n", ret);
                return ret;
            }
        }
    }

    if (prop_usb.intval || prop_ac.intval) {
        gcore_usb_detect_flag = 1;
    } else {
        gcore_usb_detect_flag = 0;
    }
    /*hs03s_NM code for SR-AL5625-01-640 by yuli at 20220428 start*/
    if (fn_data.gdev->ts_stat !=  TS_MPTEST) {
        if (gcore_usb_detect_flag != usb_connect) {
            GTP_DEBUG("prop_usb=%d, prop_ac=%d, usb_flag=%d \n", prop_usb.intval, prop_ac.intval, gcore_usb_detect_flag);
            usb_connect = gcore_usb_detect_flag;
            if (gcore_usb_detect_flag == true) {
                gcore_fw_event_notify(FW_CHARGER_PLUG);
                /*hs04 code for DEAL6398A-522 by huangzhongjie at 20220811 start*/
                fn_data.gdev->usb_detect_status = 1;
                /*hs04 code for DEAL6398A-522 by huangzhongjie at 20220811 end*/
            } else {
                gcore_fw_event_notify(FW_CHARGER_UNPLUG);
                /*hs04 code for DEAL6398A-522 by huangzhongjie at 20220811 start*/
                fn_data.gdev->usb_detect_status = 0;
                /*hs04 code for DEAL6398A-522 by huangzhongjie at 20220811 end*/
            }
        }
    }
    /*hs03s_NM code for SR-AL5625-01-640 by yuli at 20220428 end*/
    return 0;
}

void  gcore_charger_usb_check_init(void)
{
    int ret = 0;
    struct power_supply *psy_usb = NULL;
    struct power_supply *psy_ac = NULL;
    union power_supply_propval prop_usb;
    union power_supply_propval prop_ac;

    fn_data.gdev->charger_notif.notifier_call = gc_charger_notifier_callback;
    ret = power_supply_reg_notifier(&fn_data.gdev->charger_notif);
    if (ret) {
        GTP_ERROR("Unable to register charger_notifier: %d\n",ret);
        return;
    }

    psy_usb = power_supply_get_by_name("usb");
    if (!psy_usb) {
        GTP_ERROR("Couldn't get usb psy\n");
        return;
    }

    psy_ac = power_supply_get_by_name("ac");
    if (!psy_ac) {
        GTP_ERROR("Couldn't get ac psy\n");
        return;
    }

    if (!strcmp(psy_usb->desc->name, "usb")) {
        ret = power_supply_get_property(psy_usb, POWER_SUPPLY_PROP_ONLINE, &prop_usb);
        if (ret < 0) {
            GTP_ERROR("Couldn't get POWER_SUPPLY_PROP_ONLINE rc=%d\n", ret);
            return;
        }
    }

    if (!strcmp(psy_ac->desc->name, "ac")) {
        ret = power_supply_get_property(psy_ac, POWER_SUPPLY_PROP_ONLINE, &prop_ac);
        if (ret < 0) {
            GTP_ERROR("Couldn't get POWER_SUPPLY_PROP_ONLINE rc=%d\n", ret);
            return;
        }
    }

    if (prop_usb.intval || prop_ac.intval) {
        gcore_usb_detect_flag = 1;
    } else {
        gcore_usb_detect_flag = 0;
    }
    if (gcore_usb_detect_flag == true) {
        gcore_fw_event_notify(FW_CHARGER_PLUG);
        /*hs04 code for DEAL6398A-522 by huangzhongjie at 20220811 start*/
        fn_data.gdev->usb_detect_status = 1;
        /*hs04 code for DEAL6398A-522 by huangzhongjie at 20220811 end*/
    } else {
        gcore_fw_event_notify(FW_CHARGER_UNPLUG);
        /*hs04 code for DEAL6398A-522 by huangzhongjie at 20220811 start*/
        fn_data.gdev->usb_detect_status = 0;
        /*hs04 code for DEAL6398A-522 by huangzhongjie at 20220811 end*/
    }
    GTP_ERROR("boot check usb_plug_status = %d\n", gcore_usb_detect_flag);
}
/*hs03s_NM code for SR-AL5625-01-640 by yuli at 20220422 end*/

/*hs04 code for DEAL6398A-522 by huangzhongjie at 20220808 start*/
/**
*Name: <gcore_earphone_notifier_callback>
*Author: <huangzhongjie>
*Date: <2022/08/11>
*Param: <struct notifier_block *nb, unsigned long event, void *ptr>
*Return: <earphone result>
*Purpose: <earphone notifier callback>
*/
static int gcore_earphone_notifier_callback(struct notifier_block *nb, unsigned long event, void *ptr)
{
    static int earphone_pre_status = 0;
    //int earphone_status = 0;
    if (event == EARPHONE_PLUGIN_STATE) {
        fn_data.gdev->earphone_status = 1;
        GTP_DEBUG("earphone in");
    } else if (event == EARPHONE_PLUGOUT_STATE) {
        fn_data.gdev->earphone_status = 0;
        GTP_DEBUG("earphone out");
    } else {
        return -EINVAL;
    }

    if(earphone_pre_status != fn_data.gdev->earphone_status) {
        gcore_fw_event_notify(FW_HEADSET_UNPLUG-fn_data.gdev->earphone_status);
        earphone_pre_status = fn_data.gdev->earphone_status;
    }

    return 0;
}
/**
*Name: <gcore_earphone_init>
*Author: <huangzhongjie>
*Date: <2022/08/11>
*Param: <NA>
*Return: <NA>
*Purpose: <earphone init>
*/
void gcore_earphone_init(void)
{
    int ret;
    fn_data.gdev->earphone_notif.notifier_call = gcore_earphone_notifier_callback;
    GTP_DEBUG("earphone blocking_notifier_chain_register");
    ret = headset_notifier_register(&fn_data.gdev->earphone_notif);
    if (ret) {
        GTP_ERROR("unable to register headset_notifier_register\n");
    }
}
/*hs04 code for DEAL6398A-522 by huangzhongjie at 20220811 end*/

void gcore_irq_enable(struct gcore_dev *gdev)
{
    unsigned long flags;

    spin_lock_irqsave(&gdev->irq_flag_lock, flags);

    if (gdev->irq_flag == 0) {
        gdev->irq_flag = 1;
        spin_unlock_irqrestore(&gdev->irq_flag_lock, flags);
        enable_irq(gdev->touch_irq);
    } else if (gdev->irq_flag == 1) {
        spin_unlock_irqrestore(&gdev->irq_flag_lock, flags);
        GTP_ERROR("Touch Eint already enabled!");
    } else {
        spin_unlock_irqrestore(&gdev->irq_flag_lock, flags);
        GTP_ERROR("Invalid irq_flag %d!", gdev->irq_flag);
    }
    /*GTP_DEBUG("Enable irq_flag=%d",g_touch.irq_flag); */

}

void gcore_irq_disable(struct gcore_dev *gdev)
{
    unsigned long flags;

    spin_lock_irqsave(&gdev->irq_flag_lock, flags);

    if (gdev->irq_flag == 0) {
        spin_unlock_irqrestore(&gdev->irq_flag_lock, flags);
        GTP_ERROR("Touch Eint already disable!");
        return;
    }

    gdev->irq_flag = 0;

    spin_unlock_irqrestore(&gdev->irq_flag_lock, flags);

    disable_irq(gdev->touch_irq);
/* disable_irq_nosync(gdev->touch_irq); */

}

#ifndef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
int gcore_get_gpio_info(struct gcore_dev *gdev, struct device_node *node)
{
    int flag = 0;

    gdev->irq_gpio = of_get_named_gpio_flags(node, "gcore,irq-gpio", 0, (enum of_gpio_flags *)&flag);
    if (!gpio_is_valid(gdev->irq_gpio)) {
        GTP_ERROR("invalid gcore,irq-gpio:%d", gdev->irq_gpio);
        return -EPERM;
    }

    if (gpio_request(gdev->irq_gpio, "gcore,irq-gpio")) {
        GTP_ERROR("gpio %d request failed!", gdev->irq_gpio);
        return -EPERM;
    }

    gdev->rst_gpio = of_get_named_gpio_flags(node, "gcore,rst-gpio", 0, (enum of_gpio_flags *)&flag);
    if (!gpio_is_valid(gdev->rst_gpio)) {
        GTP_ERROR("invalid gcore,reset-gpio:%d", gdev->rst_gpio);
        return -EPERM;
    }

    if (gpio_request(gdev->rst_gpio, "gcore,rst-gpio")) {
        GTP_ERROR("gpio %d request failed!", gdev->rst_gpio);
        return -EPERM;
    }

    return 0;

}

void gcore_free_gpio(struct gcore_dev *gdev)
{
    gpio_free(gdev->irq_gpio);
    gpio_free(gdev->rst_gpio);
}

#endif

#ifdef CONFIG_DRM
int gcore_ts_check_dt(struct device_node *np)
{
    int i;
    int count;
    struct device_node *node;
    struct drm_panel *panel;

    count = of_count_phandle_with_args(np, "panel", NULL);
    if (count <= 0)
        return 0;

    for (i = 0; i < count; i++) {
        node = of_parse_phandle(np, "panel", i);
        panel = of_drm_find_panel(node);
        of_node_put(node);
        if (!IS_ERR(panel)) {
            gcore_active_panel = panel;
            GTP_DEBUG("%s:find\n", __func__);
            return 0;
        }
    }

    GTP_ERROR("%s: not find\n", __func__);
    return -ENODEV;
}

int gcore_ts_check_default_tp(struct device_node *dt, const char *prop)
{
    const char **gcore_active_tp = NULL;
    int count = 0;
    int tmp = 0;
    int score = 0;
    const char *active;
    int ret = 0;
    int i = 0;

    count = of_property_count_strings(dt->parent, prop);
    if (count <= 0 || count > 3)
        return -ENODEV;

    gcore_active_tp = kcalloc(count, sizeof(char *), GFP_KERNEL);
    if (!gcore_active_tp) {
        GTP_ERROR("Goodix alloc failed\n");
        return -ENOMEM;
    }

    ret = of_property_read_string_array(dt->parent, prop, gcore_active_tp, count);
    if (ret < 0) {
        GTP_ERROR("fail to read %s %d\n", prop, ret);
        ret = -ENODEV;
        goto out;
    }

    for (i = 0; i < count; i++) {
        active = gcore_active_tp[i];
        if (active != NULL) {
            tmp = of_device_is_compatible(dt, active);
            if (tmp > 0)
                score++;
        }
    }

    if (score <= 0) {
        GTP_ERROR("not match this driver\n");
        ret = -ENODEV;
        goto out;
    }
    ret = 0;
out:
    kfree(gcore_active_tp);
    return ret;
}
#endif

void gcore_get_dts_info(struct gcore_dev *gdev)
{
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
    gdev->rst_gpio = GTP_RST_PORT;
    gdev->irq_gpio = GTP_INT_PORT;
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_RK_PLATFORM) \
    || defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) \
    || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
    struct device_node *node = NULL;
/*    struct device_node *node = client->dev.of_node;  */

    node = of_find_matching_node(node, tpd_of_match);

/* 20210114: remove customized parameter for dts to common.h */
/*
    if (node)
    {
        of_property_read_u32_array(node, "gcore,tp-resolution",
            gcore_dts_data.tpd_resolution, ARRAY_SIZE(gcore_dts_data.tpd_resolution));

        GTP_DEBUG("tp res x = %d y = %d", gcore_dts_data.tpd_resolution[0],
            gcore_dts_data.tpd_resolution[1]);

        of_property_read_u32(node, "gcore,max-touch-number",
            &gcore_dts_data.touch_max_num);

        GTP_DEBUG("max touch number = %d", gcore_dts_data.touch_max_num);

        of_property_read_u32_array(node, "gcore,lcd-resolution",
            gcore_dts_data.lcm_resolution, ARRAY_SIZE(gcore_dts_data.lcm_resolution));

        GTP_DEBUG("lcd res x = %d y = %d", gcore_dts_data.lcm_resolution[0],
            gcore_dts_data.lcm_resolution[1]);

    }
    else
    {
        GTP_ERROR("can't find touch compatible custom node.");
    }
*/

#ifdef CONFIG_DRM
    if (gcore_ts_check_dt(node)) {
        if (!gcore_ts_check_default_tp(node, "qcom,spi-touch-active")) {
            GTP_DEBUG("check default tp 1");
/* return -EPROBE_DEFER; */
        } else {
            GTP_DEBUG("check default tp 2");
/* return -ENODEV */
        }
    }
#endif

    if (gcore_get_gpio_info(gdev, node)) {
        GTP_ERROR("dts get gpio info fail!");
    }
#endif
}

int gcore_input_device_init(struct gcore_dev *gdev)
{
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
    int tpd_res_x = tpd_dts_data.tpd_resolution[0];
    int tpd_res_y = tpd_dts_data.tpd_resolution[1];

    gdev->input_device = tpd->dev;

    set_bit(EV_ABS, gdev->input_device->evbit);
    set_bit(EV_KEY, gdev->input_device->evbit);
    set_bit(EV_SYN, gdev->input_device->evbit);
    set_bit(INPUT_PROP_DIRECT, gdev->input_device->propbit);

    input_set_abs_params(gdev->input_device, ABS_MT_POSITION_X, 0, tpd_res_x, 0, 0);
    input_set_abs_params(gdev->input_device, ABS_MT_POSITION_Y, 0, tpd_res_y, 0, 0);
    /*hs04 code for DEAL6398A-1521 by huangzhongjie at 20220903 start*/
    input_set_abs_params(gdev->input_device, ABS_MT_PRESSURE, 0, PRESSURE_MAX, 0, 0);
    /*hs04 code for DEAL6398A-1521 by huangzhongjie at 20220903 end*/

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    set_bit(GESTURE_KEY, gdev->input_device->keybit);
#endif

#ifdef CONFIG_ENABLE_TYPE_B_PROCOTOL
    if (input_mt_init_slots(gdev->input_device, GTP_MAX_TOUCH, INPUT_MT_DIRECT)) {
        GTP_ERROR("mt slot init fail.");
    }
#else
    /* for single-touch device */
    set_bit(BTN_TOUCH, gdev->input_device->keybit);
#endif

    return 0;
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_RK_PLATFORM) \
    || defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) \
    || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
    int tpd_res_x = TOUCH_SCREEN_X_MAX;
    int tpd_res_y = TOUCH_SCREEN_Y_MAX;

    GTP_DEBUG("touch input device init.");

    gdev->input_device = input_allocate_device();
    if (gdev->input_device == NULL) {
        GTP_ERROR("fail to allocate touch input device.");
        return -EPERM;
    }

    gdev->input_device->name = GTP_DRIVER_NAME;
    set_bit(EV_ABS, gdev->input_device->evbit);
    set_bit(EV_KEY, gdev->input_device->evbit);
    set_bit(EV_SYN, gdev->input_device->evbit);
    set_bit(INPUT_PROP_DIRECT, gdev->input_device->propbit);

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    set_bit(GESTURE_KEY, gdev->input_device->keybit);
#endif

    input_set_abs_params(gdev->input_device, ABS_MT_POSITION_X, 0, tpd_res_x, 0, 0);
    input_set_abs_params(gdev->input_device, ABS_MT_POSITION_Y, 0, tpd_res_y, 0, 0);

#ifdef CONFIG_ENABLE_TYPE_B_PROCOTOL
    if (input_mt_init_slots(gdev->input_device, GTP_MAX_TOUCH, 0)) {
        GTP_ERROR("mt slot init fail.");
    }
#else
    /* for single-touch device */
    set_bit(BTN_TOUCH, gdev->input_device->keybit);
#endif

    input_set_drvdata(gdev->input_device, gdev);

    if (input_register_device(gdev->input_device)) {
        GTP_ERROR("input register device fail.");
        return -EPERM;
    }

    return 0;
#endif
}

void gcore_input_device_deinit(struct gcore_dev *gdev)
{
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
    return;
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_RK_PLATFORM) \
    || defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) \
    || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
    input_unregister_device(gdev->input_device);
#endif
}

void gcore_touch_down(struct input_dev *dev, s32 x, s32 y, u8 id)
{
#ifdef CONFIG_ENABLE_TYPE_B_PROCOTOL
    input_mt_slot(dev, id);
    input_mt_report_slot_state(dev, MT_TOOL_FINGER, true);
    input_report_abs(dev, ABS_MT_POSITION_X, x);
    input_report_abs(dev, ABS_MT_POSITION_Y, y);
/*hs04 code for DEAL6398A-1929 by suyurui at 20221111 start*/
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
    /*hs04 code for DEAL6398A-1521 by huangzhongjie at 20220903 start*/
    input_report_abs(dev, ABS_MT_PRESSURE, g_press);
    #ifdef HQ_FACTORY_BUILD
    GTP_REPORT("gcore_touch_down pressure data:%x",g_press);
    #endif
    /*hs04 code for DEAL6398A-1521 by huangzhongjie at 20220903 end*/
#endif
/*hs04 code for DEAL6398A-1929 by suyurui at 20221111 end*/

#else
    input_report_key(dev, BTN_TOUCH, 1);    /* for single-touch device */
    input_report_abs(dev, ABS_MT_POSITION_X, x);
    input_report_abs(dev, ABS_MT_POSITION_Y, y);
    input_mt_sync(dev);
#endif
}

void gcore_touch_up(struct input_dev *dev, u8 id)
{
#ifdef CONFIG_ENABLE_TYPE_B_PROCOTOL
    input_mt_slot(dev, id);
    input_mt_report_slot_state(dev, MT_TOOL_FINGER, false);
#else
    input_report_key(dev, BTN_TOUCH, 0);    /* for single-touch device */
    input_mt_sync(dev);
#endif
}

void gcore_touch_release_all_point(struct input_dev *dev)
{
    int i = 0;

    GTP_DEBUG("gcore touch release all point");

#ifdef CONFIG_ENABLE_TYPE_B_PROCOTOL
    for (i = 0; i < GTP_MAX_TOUCH; i++) {
        gcore_touch_up(dev, i);
    }

    input_report_key(dev, BTN_TOUCH, 0);
    input_report_key(dev, BTN_TOOL_FINGER, 0);
#else
    gcore_touch_up(dev, 0);
#endif

    input_sync(dev);
}

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
void gcore_gesture_event_handler(struct input_dev *dev, int id)
{

    if ((!id) || (id >= GESTURE_MAX)) {
        return;
    }

    GTP_DEBUG("gesture id = %d", id);

    switch (id) {
    case GESTURE_DOUBLE_CLICK:
        GTP_DEBUG("double click gesture event");
        input_report_key(dev, GESTURE_KEY, 1);
        input_sync(dev);
        input_report_key(dev, GESTURE_KEY, 0);
        input_sync(dev);
        break;

    case GESTURE_UP:
        break;

    case GESTURE_DOWN:
        break;

    case GESTURE_LEFT:
        break;

    case GESTURE_RIGHT:
        break;

    case GESTURE_C:
        GTP_DEBUG("double click gesture event");
        input_report_key(dev, KEY_F24, 1);
        input_sync(dev);
        input_report_key(dev, KEY_F24, 0);
        input_sync(dev);
        break;

    case GESTURE_E:
        break;

    case GESTURE_M:
        break;

    case GESTURE_N:
        break;

    case GESTURE_O:
        break;

    case GESTURE_S:
        break;

    case GESTURE_V:
        break;

    case GESTURE_W:
        break;

    case GESTURE_Z:
        break;

    case GESTURE_PALM:
        GTP_DEBUG("palm gesture event");
        break;

    default:
        break;
    }

}
#endif


static u8 Cal8bitsChecksum(u8 *array, u16 length)
{
    s32 s32Checksum = 0;
    u16 i;

    for (i = 0; i < length; i++) {
        s32Checksum += array[i];
    }

    return (u8) (((~s32Checksum) + 1) & 0xFF);
}

int gcore_hostdownload_esd_protect(struct gcore_dev *gdev)
{
    u8 *coor_data = &gdev->touch_data[0];
    int sum = 0;
    int i = 0;

    if (coor_data[0] == 0xB1) {
        /* santiy check coor data follow 0 */
        for (i = 1; i < 64; i++) {
            sum += coor_data[i];
        }

        if (!sum) {
            if (!gdev->ts_stat) {

                GTP_DEBUG("watchdog:B1, goto hostdownload!");
                queue_delayed_work(gdev->fwu_workqueue, &gdev->fwu_work,
                           msecs_to_jiffies(100));
            }

            return -EPERM;
        }
    }

    return 0;
}
/* hs03s_NM code added for SR-AL5625-01-590 by fengzhigang at 20220422 start */
#if GCORE_WDT_RECOVERY_ENABLE
static u8 wdt_valid_values[] = { 0xFF,0xFF,0x72,0x02,0x00,0x01,0xFF,0xFF };
#endif
/* hs03s_NM code added for SR-AL5625-01-590 by fengzhigang at 20220422 end */

s32 gcore_touch_event_handler(struct gcore_dev *gdev)
{
    u8 *coor_data = NULL;
    s32 i = 0;
    u8 id = 0;
    u8 status = 0;
    bool touch_end = true;
    u8 checksum = 0;
#ifdef CONFIG_ENABLE_TYPE_B_PROCOTOL
    static unsigned long prev_touch;
#endif
    unsigned long curr_touch = 0;
    s32 input_x = 0;
    s32 input_y = 0;
    int data_size = DEMO_DATA_SIZE;
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
    int tpd_res_x = tpd_dts_data.tpd_resolution[0];
    int tpd_res_y = tpd_dts_data.tpd_resolution[1];
    int lcm_res_x = tpd_dts_data.tpd_resolution[0];
    int lcm_res_y = tpd_dts_data.tpd_resolution[1];
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_RK_PLATFORM) \
    || defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) \
    || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
    int tpd_res_x = TOUCH_SCREEN_X_MAX;
    int tpd_res_y = TOUCH_SCREEN_Y_MAX;
    int lcm_res_x = TOUCH_SCREEN_X_MAX;
    int lcm_res_y = TOUCH_SCREEN_Y_MAX;
#endif

#ifdef CONFIG_ENABLE_FW_RAWDATA
    if (gdev->fw_mode == DEMO) {
        data_size = DEMO_DATA_SIZE;
    } else if (gdev->fw_mode == RAWDATA) {
        data_size = g_rawdata_row * g_rawdata_col * 2;
    } else if (gdev->fw_mode == DEMO_RAWDATA) {
#ifdef CONFIG_TOUCH_DRIVER_INTERFACE_SPI
        data_size = 2048;    /* mtk platform spi transfer len request */
#else
        data_size = DEMO_DATA_SIZE + (g_rawdata_row * g_rawdata_col * 2);
#endif
    } else {
        data_size = DEMO_DATA_SIZE + (g_rawdata_row * g_rawdata_col * 2);
    }
#endif
    if (data_size > DEMO_RAWDATA_MAX_SIZE) {
        GTP_ERROR("touch data read length exceed MAX_SIZE.");
        return -EPERM;
    }

    /*hs03s code for SR-AL5625-01-626  by zhawei at 20220413 start*/
    if (gdev->ts_stat == TS_MPTEST) {
        GTP_ERROR("mp test unknow int");
        return -EPERM;
    }
    /*hs03s code for SR-AL5625-01-626  by zhawei at 20220413 end*/

    if (gcore_bus_read(gdev->touch_data, data_size)) {
        GTP_ERROR("touch data read error.");
        return -EPERM;
    }
#ifdef CONFIG_ENABLE_FW_RAWDATA
    if (gdev->usr_read) {
        gdev->data_ready = true;
        wake_up_interruptible(&gdev->usr_wait);

        if (gdev->fw_mode == RAWDATA) {
            return 0;
        }
    }
#endif

    coor_data = &gdev->touch_data[0];

#if GCORE_WDT_RECOVERY_ENABLE
    if (!gdev->ts_stat) {

        if (!memcmp(coor_data, wdt_valid_values, sizeof(wdt_valid_values))) {
            //GTP_DEBUG("WDT interrupt detected");
            mod_delayed_work(gdev->fwu_workqueue, &gdev->wdt_work, \
                msecs_to_jiffies(GCORE_WDT_TIMEOUT_PERIOD));
            wdt_contin = 0;
            return 0; /* WDT interrupt detected */
        }
    }
#endif

#if defined(CONFIG_GCORE_AUTO_UPDATE_FW_HOSTDOWNLOAD) \
        && defined(CONFIG_GCORE_HOSTDOWNLOAD_ESD_PROTECT)
    /* firmware watchdog, rehostdownload */
    if (gcore_hostdownload_esd_protect(gdev)) {
        return 0;
    }
#endif

    checksum = Cal8bitsChecksum(coor_data, DEMO_DATA_SIZE - 1);
    if (checksum != coor_data[DEMO_DATA_SIZE - 1]) {
        GTP_ERROR("checksum error! read:%x cal:%x", \
            coor_data[DEMO_DATA_SIZE - 1], checksum);
#if 1
        for (i = 1; i <= 10; i++) {
            GTP_DEBUG("demo %d:%x %x %x %x %x %x", i, coor_data[6*i-6], \
                coor_data[6*i-5], coor_data[6*i-4], coor_data[6*i-3], \
                coor_data[6*i-2], coor_data[6*i-1]);
        }
        GTP_DEBUG("demo:%x %x %x %x", coor_data[60], coor_data[61], \
            coor_data[62], coor_data[63]);
#endif
        return -EPERM;
    }

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    gcore_gesture_event_handler(gdev->input_device, (coor_data[61] >> 3));
#endif
    /*hs04 code for DEAL6398A-1521 by huangzhongjie at 20220903 start*/
    g_press = coor_data[PRESSURE_DATA_ADDRESS];
    /*hs04 code for DEAL6398A-1521 by huangzhongjie at 20220903 end*/
    for (i = 1; i <= GTP_MAX_TOUCH; i++) {
        // GTP_REPORT("i:%d data:%x %x %x %x %x %x", i, coor_data[0], coor_data[1],
        //       coor_data[2], coor_data[3], coor_data[4], coor_data[5]);

#if GCORE_WDT_RECOVERY_ENABLE
        if (!gdev->ts_stat) {
            mod_delayed_work(gdev->fwu_workqueue, &gdev->wdt_work, \
              msecs_to_jiffies(GCORE_WDT_TIMEOUT_PERIOD));
            wdt_contin = 0;
        }
#endif
        if ((coor_data[0] == 0xFF) || (coor_data[0] == 0x00)) {
            coor_data += 6;
            continue;
        }

        touch_end = false;
        id = coor_data[0] >> 3;
        status = coor_data[0] & 0x03;
        input_x = ((coor_data[1] << 4) | (coor_data[3] >> 4));
        input_y = ((coor_data[2] << 4) | (coor_data[3] & 0x0F));

/*hs04 code for DEAL6398A-1929 by suyurui at 20221111 start*/
#ifdef HQ_FACTORY_BUILD
        GTP_REPORT("id:%d (x=%d,y=%d)", id, input_x, input_y);
        GTP_REPORT("tpd_x=%d tpd_y=%d lcm_x=%d lcm_y=%d", tpd_res_x, tpd_res_y, lcm_res_x, lcm_res_y);
#endif
/*hs04 code for DEAL6398A-1929 by suyurui at 20221111 end*/
        if (id > GTP_MAX_TOUCH) {
            coor_data += 6;
            continue;
        }

        input_x = GTP_TRANS_X(input_x, tpd_res_x, lcm_res_x);
        input_y = GTP_TRANS_Y(input_y, tpd_res_y, lcm_res_y);

        set_bit(id, &curr_touch);

        gcore_touch_down(gdev->input_device, input_x, input_y, id - 1);

        coor_data += 6;

    }

#ifdef CONFIG_ENABLE_TYPE_B_PROCOTOL
    for (i = 1; i <= GTP_MAX_TOUCH; i++) {
        if (!test_bit(i, &curr_touch) && test_bit(i, &prev_touch)) {
            gcore_touch_up(gdev->input_device, i - 1);
        }

        if (test_bit(i, &curr_touch)) {
            set_bit(i, &prev_touch);
        } else {
            clear_bit(i, &prev_touch);
        }

        clear_bit(i, &curr_touch);
    }
    /*hs03s_NM code for SR-AL5625-01-640 by yuli at 20220422 start*/
    if (tpd->tp_is_enabled) {
        input_report_key(gdev->input_device, BTN_TOUCH, !touch_end);
    } else {
        GTP_DEBUG("tp is enable off\n");
    }
    /*hs03s_NM code for SR-AL5625-01-640 by yuli at 20220422 end*/

#else
    if (touch_end) {
        gcore_touch_up(gdev->input_device, 0);
    }
#endif

    input_sync(gdev->input_device);

    return 0;
}

static irqreturn_t tpd_event_handler(int irq, void *dev_id)
{
    struct gcore_dev *gdev = (struct gcore_dev *)dev_id;

    if (mutex_is_locked(&gdev->transfer_lock)) {
        GTP_DEBUG("touch is locked, ignore");
        return IRQ_HANDLED;
    }

    mutex_lock(&gdev->transfer_lock);
    /* don't reset before "if (tpd_halt..."  */
    //GTP_REPORT("tpd_event_handler enter.");

    if (gcore_touch_event_handler(gdev)) {
        GTP_ERROR("touch event handler error.");
    }

    mutex_unlock(&gdev->transfer_lock);

    return IRQ_HANDLED;
}

static irqreturn_t tpd_eint_interrupt_handler(int irq, void *dev_id)
{
    struct gcore_dev *gdev = (struct gcore_dev *)dev_id;
    struct gcore_exp_fn *exp_fn = NULL;
    struct gcore_exp_fn *exp_fn_temp = NULL;
    u8 found = 0;

/* GTP_DEBUG("tpd_eint_interrupt_handler."); */

    if (!list_empty(&fn_data.list)) {
        list_for_each_entry_safe(exp_fn, exp_fn_temp, &fn_data.list, link) {
            if (exp_fn->wait_int == true) {
                exp_fn->event_flag = true;
                found = 1;
                break;
            }
        }
    }

    if (!found) {
        gdev->tpd_flag = 1;
        return IRQ_WAKE_THREAD;
    } else {
        wake_up_interruptible(&gdev->wait);
        return IRQ_HANDLED;
    }

}

static int tpd_irq_registration(struct gcore_dev *gdev)
{
    int ret = 0;
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
    struct device_node *node = NULL;
#endif

    GTP_DEBUG("Device Tree Tpd_irq_registration!");

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
    node = of_find_matching_node(node, touch_of_match);
    if (node) {
        gdev->touch_irq = irq_of_parse_and_map(node, 0);
        GTP_DEBUG("Device touch_irq = %d!", gdev->touch_irq);
    } else {
        GTP_ERROR("tpd request_irq can not find touch eint device node!.");
        ret = -1;
    }
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_RK_PLATFORM) \
    || defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) \
    || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
    gdev->touch_irq = gpio_to_irq(gdev->irq_gpio);

#endif

    ret = devm_request_threaded_irq(&gdev->bus_device->dev, gdev->touch_irq,
                    tpd_eint_interrupt_handler,
                    tpd_event_handler,
                    IRQF_TRIGGER_RISING | IRQF_ONESHOT, "TOUCH_PANEL-eint", gdev);
    if (ret > 0) {
        ret = -1;
        GTP_ERROR("tpd request_irq IRQ LINE NOT AVAILABLE!.");
    }

    return ret;
}

void gcore_gpio_rst_output(int rst, int level)
{

    GTP_DEBUG("gpio rst = %d output level = %d", rst, level);

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
    if (level) {
        tpd_gpio_output(rst, 1);
    } else {
        tpd_gpio_output(rst, 0);
    }
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_RK_PLATFORM) \
    || defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) \
    || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
    if (level) {
        gpio_direction_output(rst, 1);
    } else {
        gpio_direction_output(rst, 0);
    }

#endif

}

int gcore_probe_device(void)
{
#if defined(CONFIG_ENABLE_CHIP_TYPE_GC7371)
    u16 chip_type = 0x7371;
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7271)
    u16 chip_type = 0x7271;
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7202)
    u16 chip_type = 0x3202;
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7372)
    u16 chip_type = 0x7372;
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7302)
    u16 chip_type = 0x7302;
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7202H)
    u16 chip_type = 0x7203;
#endif
    u8 buf[2] = { 0 };
    u16 id = 0;
    int retries = 0;
    int ret = 0;

    while (retries++ < 3) {
        gcore_idm_read_id(buf, sizeof(buf));

        id = (buf[0] << 8) | buf[1];

        if (id == chip_type) {
            GTP_DEBUG("GC%04x chip id match success", id);
            ret = 0;
            break;
        } else {
            GTP_ERROR("GC%04x chip id match fail! retries:%d", id, retries);
            /*hs04 code for DEAL6398A-1659 by huangzhongjie at 20220909 start*/
            ret = 0;
            /*hs04 code for DEAL6398A-1659 by huangzhongjie at 20220909 end*/
        }
    }

    return ret;
}

#if GCORE_WDT_RECOVERY_ENABLE
void gcore_wdt_recovery_works(struct work_struct *work)
{
    struct gcore_dev *gdev = fn_data.gdev;

    wdt_contin++;
    /*hs04 code for DEAL6398A-1923 by suyurui at 20221108 start*/
    mp_wait_int_set_fail();
    /*hs04 code for DEAL6398A-1923 by suyurui at 20221108 end*/

    GTP_ERROR("WDT timeout recovery ts:%d,wdtime:%d", gdev->ts_stat, wdt_contin);

    if (!gdev->ts_stat) {
        if (wdt_contin < 2) {
            gcore_request_firmware_update_work(NULL);
        } else {
            gcore_trigger_esd_recovery();
            wdt_contin = 0;
        }
    }
}
#endif

s32 gcore_init(struct gcore_dev *gdev)
{
    int data_size = DEMO_DATA_SIZE;

    GTP_DEBUG("gcore touch init.");

    gdev->touch_irq = 0;
    gdev->irq_flag = 1;

    spin_lock_init(&gdev->irq_flag_lock);
    mutex_init(&gdev->transfer_lock);
    init_waitqueue_head(&gdev->wait);

    fn_data.gdev = gdev;
    fn_data.fn_inited = true;
    if (!fn_data.initialized) {
        INIT_LIST_HEAD(&fn_data.list);
        fn_data.initialized = true;
    }

    gdev->rst_output = gcore_gpio_rst_output;
    gdev->irq_enable = gcore_irq_enable;
    gdev->irq_disable = gcore_irq_disable;

#ifdef CONFIG_ENABLE_FW_RAWDATA
    gdev->fw_mode = DEMO;
    init_waitqueue_head(&gdev->usr_wait);
    gdev->usr_read = false;
    gdev->data_ready = false;

    data_size = DEMO_RAWDATA_MAX_SIZE;
#endif

    gdev->touch_data = kzalloc(data_size, GFP_KERNEL);
    if (IS_ERR_OR_NULL(gdev->touch_data)) {
        GTP_ERROR("Allocate touch data mem fail!");
        return -ENOMEM;
    }
/* gcore_gpio_rst_output(gdev->rst_gpio, 1); */

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
    tpd_load_status = 1;
#endif

    gdev->fwu_workqueue = create_singlethread_workqueue("fwu_workqueue");
    INIT_DELAYED_WORK(&gdev->fwu_work, gcore_request_firmware_update_work);
#if GCORE_WDT_RECOVERY_ENABLE
    INIT_DELAYED_WORK(&gdev->wdt_work, gcore_wdt_recovery_works);
#endif


    gdev->ts_stat = TS_NORMAL;

    /*hs03s_NM code for SR-AL5625-01-640 by yuli at 20220422 start*/
    fn_data.gdev->input_enable_workqueue = create_singlethread_workqueue("input_enable");
    if (!fn_data.gdev->input_enable_workqueue) {
        GTP_ERROR("allocate input_enable_workqueue fail!");
        return -ENOMEM;
    } else {
        INIT_DELAYED_WORK(&fn_data.gdev->input_enable, gcore_input_enable_init);
        queue_delayed_work(fn_data.gdev->input_enable_workqueue, &fn_data.gdev->input_enable, msecs_to_jiffies(3000));
    }
    /*hs03s_NM code for SR-AL5625-01-640 by yuli at 20220422 end*/

    return 0;
}


void gcore_deinit(struct gcore_dev *gdev)
{
    struct gcore_exp_fn *exp_fn = NULL;
    struct gcore_exp_fn *exp_fn_temp = NULL;

    if (!list_empty(&fn_data.list)) {
        list_for_each_entry_safe(exp_fn, exp_fn_temp, &fn_data.list, link) {
            if (exp_fn->remove != NULL) {
                exp_fn->remove(gdev);
            }
        }
    }
#ifdef CONFIG_GCORE_AUTO_UPDATE_FW_HOSTDOWNLOAD
    destroy_workqueue(gdev->fwu_workqueue);
#endif

    /*hs03s_NM code for SR-AL5625-01-640 by yuli at 20220422 start*/
    cancel_delayed_work_sync(&fn_data.gdev->input_enable);
    destroy_workqueue(fn_data.gdev->input_enable_workqueue);
    /*hs03s_NM code for SR-AL5625-01-640 by yuli at 20220422 end*/

    kfree(gdev->touch_data);

#ifndef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
    gcore_free_gpio(gdev);
#endif

    gcore_input_device_deinit(gdev);

    kfree(gdev);

}

int gcore_touch_probe(struct gcore_dev *gdev)
{
#if 0
    struct gcore_exp_fn *exp_fn = NULL;
    struct gcore_exp_fn *exp_fn_temp = NULL;
#endif

    int ret = 0;

    GTP_DEBUG("tpd_registration start.");

    gcore_get_dts_info(gdev);

    gcore_input_device_init(gdev);

    if (gcore_init(gdev)) {
        GTP_ERROR("gcore init fail!");
    }
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
    tpd_gpio_as_int(GTP_INT_PORT);
#endif

    msleep(50);

    /* EINT device tree, default EINT enable */
    tpd_irq_registration(gdev);

    list_add_tail(&p_fwu_fn->link, &fn_data.list);
    if (p_fwu_fn->init != NULL) {
        p_fwu_fn->init(fn_data.gdev);
    }

    list_add_tail(&p_intf_fn->link, &fn_data.list);
    if (p_intf_fn->init != NULL) {
        p_intf_fn->init(fn_data.gdev);
    }

    list_add_tail(&p_mp_fn->link, &fn_data.list);
    if (p_mp_fn->init != NULL) {
        p_mp_fn->init(fn_data.gdev);
    }

/*hs03s_NM code for SR-AL5625-01-626  by zhawei at 20220407 start*/

    ret = gc_ito_test_node_init(&hwinfo_device);
    if (ret != 0){
        GTP_ERROR("init ito test node failed!\n");
        return -EIO;
    } else {
        GTP_DEBUG("init ito test node PASS!\n");
    }
/*hs03s_NM code for SR-AL5625-01-626  by zhawei at 20220407 end*/
/*hs03s_NM code for SR-AL5625-01-624 by zhawei at 20220407 start*/
    tp_is_used=GALAXYCORE;
/*hs03s_NM code for SR-AL5625-01-624 by zhawei at 20220407 end*/

#if 0
    if (!list_empty(&fn_data.list)) {
        list_for_each_entry_safe(exp_fn, exp_fn_temp, &fn_data.list, link) {
            if (exp_fn->init != NULL) {
                exp_fn->init(gdev);
            }
        }
    }
#endif

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM
    if (gcore_sysfs_add_device(&gdev->bus_device->dev)) {
        GTP_ERROR("gcore sysfs add device fail!");
    }
#endif

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM
#ifdef CONFIG_DRM
    GTP_DEBUG("Init notifier_drm struct");
    gdev->drm_notifier.notifier_call = gcore_ts_drm_notifier_callback;

    if (gcore_active_panel) {
        if (drm_panel_notifier_register(gcore_active_panel, &gdev->drm_notifier) < 0)
            GTP_ERROR("register notifier failed!");
    }
#endif
#endif

    /*hs03s_NM code for SR-AL5625-01-640 by yuli at 20220420 start*/
#if defined(HQ_D85_BUILD) || defined(FTY_TP_GESTURE)
    gcore_enable_gesture_wakeup();
    GTP_DEBUG("%s:Double click gesture default on in D85 version\n", __func__);
#endif

    gcore_sec_fn_init(gdev);
    /*hs03s_NM code for SR-AL5625-01-640 by yuli at 20220420 end*/
    /*hs03s_NM code for SR-AL5625-01-640 by yuli at 20220422 start*/
    gcore_charger_usb_check_init();
    /*hs03s_NM code for SR-AL5625-01-640 by yuli at 20220422 end*/
    /*hs04 code for DEAL6398A-522 by huangzhongjie at 20220811 start*/
    gcore_earphone_init();
    /*hs04 code for DEAL6398A-522 by huangzhongjie at 20220811 end*/
    return 0;
}

/*-------------------------------------------------*/
/*  Touch Driver I2C and SPI Function              */
/*-------------------------------------------------*/
#if defined(CONFIG_TOUCH_DRIVER_INTERFACE_I2C)

/*
 * I2C address
 * GC7202:0x26(0x78 unuse)
 * GC7271:0x78(0x26 unuse)
 */

struct i2c_client *gcore_i2c_client;

#define GTP_I2C_ADDRESS      0xF0    /* slave address 0x78 << 1 */
static const struct i2c_device_id tpd_i2c_id[] = { {GTP_DRIVER_NAME, 0}, {} };
static unsigned short force[] = { 0, GTP_I2C_ADDRESS, I2C_CLIENT_END, I2C_CLIENT_END };
static const unsigned short *const forces[] = { force, NULL };

s32 gcore_i2c_read(u8 *buffer, s32 len)
{
    s32 data_length = len;
    s32 ret = 0;

    struct i2c_msg msgs[1] = {
        {
         .addr = gcore_i2c_client->addr,
         .flags = I2C_M_RD,
         .buf = buffer,
         .len = data_length,
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_RK_PLATFORM
         .scl_rate = 400000,
#endif
         },
    };

/*
* According to i2c-mtk.c __mt_i2c_transfer(),mtk i2c always use DMA mode.
* Mtk i2c will copy the buffer to DMA memory automatic. so our buffer needn't
* allocate DMA memory.i2c max transfer size is 4K.
*/
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
    if (len > 4096) {
        GTP_ERROR("mtk i2c transfer size limit 4k!");
        return -EPERM;
    }
#endif

    ret = i2c_transfer(gcore_i2c_client->adapter, msgs, 1);
    if (ret != 1) {
        GTP_ERROR("I2C Transfer error!");
        return -EPERM;
    }

    return 0;

}

s32 gcore_i2c_write(u8 *buffer, s32 len)
{
    s32 ret = 0;

    struct i2c_msg msgs = {
        .flags = 0,
        .addr = gcore_i2c_client->addr,
        .len = len,
        .buf = buffer,
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_RK_PLATFORM
        .scl_rate = 100000,
#endif
    };

/*
* According to i2c-mtk.c __mt_i2c_transfer(),mtk i2c always use DMA mode.
* Mtk i2c will copy the buffer to DMA memory automatic. so our buffer needn't
* allocate DMA memory.i2c max transfer size is 4K.
*/
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
    if (len > 4096) {
        GTP_ERROR("mtk i2c transfer size limit 4k!");
        return -EPERM;
    }
#endif

    ret = i2c_transfer(gcore_i2c_client->adapter, &msgs, 1);
    if (ret != 1) {
        GTP_ERROR("I2C Transfer error!");
        return -EPERM;
    }

    return 0;

}

static s32 gcore_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct gcore_dev *touch_dev = NULL;

    GTP_DEBUG("tpd_i2c_probe start.");

    gcore_i2c_client = client;

    /* GC7371 use rst pin and need init before use */
#ifndef CONFIG_ENABLE_CHIP_TYPE_GC7371
    if (gcore_probe_device()) {
        GTP_ERROR("gcore probe device fail!");
        return -EPERM;
    }
#endif

    touch_dev = kzalloc(sizeof(struct gcore_dev), GFP_KERNEL);
    if (IS_ERR_OR_NULL(touch_dev)) {
        GTP_ERROR("allocate touch_dev mem fail!");
        return -ENOMEM;
    }

    touch_dev->bus_device = client;

    i2c_set_clientdata(client, touch_dev);

    if (gcore_touch_probe(touch_dev)) {
        GTP_ERROR("touch registration fail!");
        return -EPERM;
    }

    return 0;
}

static int gcore_i2c_remove(struct i2c_client *client)
{
    struct gcore_dev *gdev = i2c_get_clientdata(client);

    gcore_deinit(gdev);

    return 0;
}

static int gcore_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
    strlcpy(info->type, "mtk-tpd", sizeof(info->type));
    return 0;
}

static struct i2c_driver tpd_i2c_driver = {
    .probe = gcore_i2c_probe,
    .remove = gcore_i2c_remove,
    .detect = gcore_i2c_detect,
    .driver.name = GTP_DRIVER_NAME,
    .driver = {
           .name = GTP_DRIVER_NAME,
           .of_match_table = tpd_of_match,
           },
    .id_table = tpd_i2c_id,
    .address_list = (const unsigned short *)forces,
};

#elif defined(CONFIG_TOUCH_DRIVER_INTERFACE_SPI)

struct spi_device *gcore_spi_slave;

/*
* mtk spi use DMA mode when transfer length over 32 bytes.
* out buffer should allocate DMA memory over 32 bytes.
* TODO:put the allocate memory in init or probe will be better.
*/
static void *tx = NULL;
static void *rx = NULL;
static s32 gcore_spi_setup_xfer(struct spi_transfer *xfer, u32 len)
{

    if (IS_ERR_OR_NULL(tx)) {
        tx = kzalloc(FW_SIZE, GFP_KERNEL | GFP_DMA);
    }
    if (tx == NULL) {
        GTP_ERROR("tx kzalloc fail!");
        return -ENOMEM;
    }

    if (IS_ERR_OR_NULL(rx)) {
        rx = kzalloc(FW_SIZE, GFP_KERNEL | GFP_DMA);
    }
    if (rx == NULL) {
        GTP_ERROR("rx kzalloc fail!");
        kfree(tx);
        return -ENOMEM;
    }

    xfer->len = len;
    xfer->tx_buf = tx;
    xfer->rx_buf = rx;

    return 0;

}

s32 gcore_spi_read(u8 *buffer, s32 len)
{
    struct spi_transfer transfer = { 0, };
    struct spi_message msg;
    int ret = 0;

/* GTP_DEBUG("spi read. len = %d", len); */

    spi_message_init(&msg);
    ret = gcore_spi_setup_xfer(&transfer, len);
    if (ret) {
        GTP_ERROR("gcore spi setup xfer fail!");
        return -EPERM;
    }

    memset((void *)transfer.tx_buf,0xff,len);

    spi_message_add_tail(&transfer, &msg);
    ret = spi_sync(gcore_spi_slave, &msg);
    if (ret < 0) {
        GTP_ERROR("spi message transfer error!");
    }

    memcpy(buffer, transfer.rx_buf, len);

    return 0;
}

s32 gcore_spi_write(const u8 *buffer, s32 len)
{
    struct spi_transfer transfer = { 0, };
    struct spi_message msg;
    int ret = 0;

    spi_message_init(&msg);
    ret = gcore_spi_setup_xfer(&transfer, len);
    if (ret) {
        GTP_ERROR("gcore spi setup xfer fail!");
        return -EPERM;
    }

    memcpy((u8 *) transfer.tx_buf, buffer, len);

    spi_message_add_tail(&transfer, &msg);
    ret = spi_sync(gcore_spi_slave, &msg);
    if (ret < 0) {
        GTP_ERROR("spi message transfer error!");
    }

    return 0;
}

#define SPI_DUMMY  2

s32 gcore_spi_write_then_read(u8 *txbuf, s32 n_tx, u8 *rxbuf, s32 n_rx)
{
    struct spi_transfer transfer = { 0, };
    struct spi_message msg;
    int ret = 0;
    int total = n_tx + n_rx + SPI_DUMMY;

    spi_message_init(&msg);
    ret = gcore_spi_setup_xfer(&transfer, total);
    if (ret) {
        GTP_ERROR("gcore spi setup xfer fail!");
        return -EPERM;
    }

    memcpy((u8 *) transfer.tx_buf, txbuf, n_tx);

    spi_message_add_tail(&transfer, &msg);
    ret = spi_sync(gcore_spi_slave, &msg);
    if (ret < 0) {
        GTP_ERROR("spi message transfer error!");
    }

    memcpy(rxbuf, transfer.rx_buf + n_tx + SPI_DUMMY, n_rx);

    return 0;
}

void gcore_spi_config(void)
{
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_MTK_LEGACY_PLATFORM
    struct mt_chip_conf *spi_config;

    spi_config = (struct mt_chip_conf *)gcore_spi_slave->controller_data;
    if (!spi_config) {
        pr_err("spi config is error!!!");
    }

    spi_config->cpol = 1;
    spi_config->cpha = 1;
    spi_config->holdtime = 5000;
    spi_config->setuptime = 5000;
    spi_config->high_time = 40;
    spi_config->low_time = 40;

    return;
#endif
#endif

    gcore_spi_slave->mode = SPI_MODE_3;
    gcore_spi_slave->bits_per_word = 8;
    gcore_spi_slave->max_speed_hz = 6000000;
    gcore_spi_slave->chip_select = 0;
    spi_setup(gcore_spi_slave);

}

static s32 gcore_spi_probe(struct spi_device *slave)
{
    struct gcore_dev *touch_dev = NULL;

    GTP_DEBUG("tpd_spi_probe start.");

    gcore_spi_slave = slave;
    gcore_spi_config();

    /* GC7371 use rst pin and need init before use */
#ifndef CONFIG_ENABLE_CHIP_TYPE_GC7371
    if (gcore_probe_device()) {
        GTP_ERROR("gcore probe device fail!");
        return -EPERM;
    }
#endif

    touch_dev = kzalloc(sizeof(struct gcore_dev), GFP_KERNEL);
    if (IS_ERR_OR_NULL(touch_dev)) {
        GTP_ERROR("allocate touch_dev mem fail!");
        return -ENOMEM;
    }

    touch_dev->bus_device = slave;

    spi_set_drvdata(slave, touch_dev);

    if (gcore_touch_probe(touch_dev)) {
        GTP_ERROR("touch registration fail!");
        return -EPERM;
    }

    return 0;
}

static int gcore_spi_remove(struct spi_device *slave)
{
    struct gcore_dev *gdev = spi_get_drvdata(slave);

    gcore_deinit(gdev);

    return 0;
}

#if 0
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_MTK_LEGACY_PLATFORM
static struct spi_board_info spi_board_devs[] __initdata = {
    [0] = {
           .modalias = GTP_DRIVER_NAME,
           .bus_num = 1,
           .chip_select = 1,
           .mode = SPI_MODE_3,
           },
};
#endif
#endif
#endif

static struct spi_device_id tpd_spi_id = { GTP_DRIVER_NAME, 0 };

static struct spi_driver tpd_spi_driver = {
    .probe = gcore_spi_probe,
    .id_table = &tpd_spi_id,
    .driver = {
           .name = GTP_DRIVER_NAME,
           .owner = THIS_MODULE,
           .of_match_table = tpd_of_match,
           },
    .remove = gcore_spi_remove,
};

#endif /* CONFIG_TOUCH_DRIVER_INTERFACE_I2C */

s32 gcore_bus_read(u8 *buffer, s32 len)
{
#if defined(CONFIG_TOUCH_DRIVER_INTERFACE_I2C)
    return gcore_i2c_read(buffer, len);
#elif defined(CONFIG_TOUCH_DRIVER_INTERFACE_SPI)
    return gcore_spi_read(buffer, len);
#endif

}

EXPORT_SYMBOL(gcore_bus_read);

s32 gcore_bus_write(const u8 *buffer, s32 len)
{
#if defined(CONFIG_TOUCH_DRIVER_INTERFACE_I2C)
    return gcore_i2c_write((u8 *) buffer, len);
#elif defined(CONFIG_TOUCH_DRIVER_INTERFACE_SPI)
    return gcore_spi_write(buffer, len);
#endif

}

EXPORT_SYMBOL(gcore_bus_write);

int gcore_touch_bus_init(void)
{
    GTP_DEBUG("touch_bus_init start.");
    if(tp_is_used != UNKNOWN_TP) {
        GTP_DEBUG("it is not gcore tp\n");
        return -ENOMEM;
    }

#if defined(CONFIG_TOUCH_DRIVER_INTERFACE_I2C)
    return i2c_add_driver(&tpd_i2c_driver);
#elif defined(CONFIG_TOUCH_DRIVER_INTERFACE_SPI)

#if 0
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_MTK_LEGACY_PLATFORM
    spi_register_board_info(spi_board_devs, ARRAY_SIZE(spi_board_devs));
#endif
#endif
#endif

    return spi_register_driver(&tpd_spi_driver);
#endif

}

void gcore_touch_bus_exit(void)
{
#if defined(CONFIG_TOUCH_DRIVER_INTERFACE_I2C)
    i2c_del_driver(&tpd_i2c_driver);
#elif defined(CONFIG_TOUCH_DRIVER_INTERFACE_SPI)
    spi_unregister_driver(&tpd_spi_driver);
#endif

}
