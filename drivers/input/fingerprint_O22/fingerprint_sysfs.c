/*
 * Copyright (C) 2017 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

/*
 *  fingerprint sysfs class
 */
/*hs14 code for AL6528A-947 by zhangziyi at 2022/11/21 start*/
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/pinctrl/consumer.h>
#include <linux/errno.h>
#include <linux/of_device.h>

int adm_flag[] = {0};

int finger_sysfs = 0;
EXPORT_SYMBOL_GPL(finger_sysfs);
int g_prox_ret = 0;
EXPORT_SYMBOL_GPL(g_prox_ret);

struct class *fingerprint_class = NULL;
struct device *adm_dev = NULL;
struct pinctrl *g_proxctrl = NULL;
struct pinctrl_state *g_ldo_enable = NULL;

/*hs14 code for AL6528ADEU-3630 by xiongxiaoliang at 2022/12/16 start*/
#define FPS_PCBAINFO_STR_LEN  20
static char fps_pcbainfo_cmdline[FPS_PCBAINFO_STR_LEN + 1];

static int __init fps_pcbainfo_setup(char *str)
{
    strlcpy(fps_pcbainfo_cmdline, str, ARRAY_SIZE(fps_pcbainfo_cmdline));
    return 1;
}

__setup("androidboot.pcbainfo=", fps_pcbainfo_setup);

static bool detect_machine(void)
{
    pr_info("%s command_line = %s\n", __func__, fps_pcbainfo_cmdline);

    if ((NULL != strstr(fps_pcbainfo_cmdline, "EVB")) || (NULL != strstr(fps_pcbainfo_cmdline, "EVT"))
        || (NULL != strstr(fps_pcbainfo_cmdline, "DVT")) ) {
        pr_info("%s detect EVB/EVT/DVT\n", __func__);
        return true;
    } else {
        pr_info("%s detect PVT/MP\n", __func__);
        return false;
    }
}
/*hs14 code for AL6528ADEU-3630 by xiongxiaoliang at 2022/12/16 end*/

/*
 * Create sysfs interface
 */
static ssize_t fingerprint_adm_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
    pr_info("%s: finger_sysfs = %d\n", __func__, finger_sysfs);

    if (finger_sysfs) {
        return snprintf(buf, PAGE_SIZE, "%d\n", 1);
    } else {
        return snprintf(buf, PAGE_SIZE, "%d\n", 0);
    }
}

static DEVICE_ATTR(adm, 0444, fingerprint_adm_show, NULL);

static struct device_attribute *fps_attrs[] = {
    &dev_attr_adm,
    NULL,
};

static void get_pinctrl(struct device *dev)
{
    int ret = 0;
    struct device_node *np = dev->of_node;
    if (!np) {
        pr_info("device node is null");
        return ;
    }

    g_prox_ret = detect_machine();
    if (!g_prox_ret) {
        g_proxctrl = devm_pinctrl_get(dev);
        pr_info("detect_machine in");
        if (IS_ERR_OR_NULL(g_proxctrl)) {
            ret = PTR_ERR(g_proxctrl);
            pr_info("Failed to get pinctrl, please check dts,ret=%d", ret);
            return ;
        }

        g_ldo_enable = pinctrl_lookup_state(g_proxctrl, "prox_enable");
        if (IS_ERR_OR_NULL(g_ldo_enable)) {
            ret = PTR_ERR(g_ldo_enable);
            pr_info("prox ldo lookup_state error = %d", ret);
            devm_pinctrl_put(g_proxctrl);
            return ;
        }

        ret = pinctrl_select_state(g_proxctrl, g_ldo_enable);
        if (ret) {
            pr_info("prox ldo select_state error =%d", ret);
            devm_pinctrl_put(g_proxctrl);
            return ;
        }
    }
}

static int prox_ldo__probe(struct platform_device *pdev)
{
    get_pinctrl(&pdev->dev);
    return 0;
}

static const struct of_device_id prox_ldo_of_match[] = {
    { .compatible = "mediatek, prox_ldo", },
    {}
};

static struct platform_driver prox_ldo_driver = {
    .driver      = {
        .name    = "prox_ldo_enable",
        .of_match_table = prox_ldo_of_match,
    },
    .probe       = prox_ldo__probe,
};

static int __init fingerprint_class_init(void)
{
    int i = 0;
    int ret = 0;

    pr_info("%s\n", __func__);

    ret = platform_driver_register(&prox_ldo_driver);
    if (ret < 0) {
        goto out;
    }

    fingerprint_class = class_create(THIS_MODULE, "fingerprint");

    if (IS_ERR(fingerprint_class)) {
        pr_err("%s: create fingerprint_class is failed.\n", __func__);
        return PTR_ERR(fingerprint_class);
    }

    fingerprint_class->dev_uevent = NULL;

    adm_dev = device_create(fingerprint_class, NULL, 0, NULL, "fingerprint");

    if (IS_ERR(adm_dev)) {
        pr_err("%s: device_create failed!\n", __func__);
        class_destroy(fingerprint_class);
        return PTR_ERR(adm_dev);
    }

    for (i = 0; fps_attrs[i] != NULL; i++) {
        adm_flag[i] = device_create_file(adm_dev, fps_attrs[i]);

        if (adm_flag[i]) {
            pr_err("%s: fail device_create_file (adm_dev, fps_attrs[%d])\n", __func__, i);
            device_destroy(fingerprint_class, 0);
            class_destroy(fingerprint_class);
            return adm_flag[i];
        }
    }

    out:
        return ret;

    return 0;
}

static void __exit fingerprint_class_exit(void)
{
    int i = 0;

    for (i = 0; fps_attrs[i] != NULL; i++) {
        if (!adm_flag[i]) {
            device_remove_file(adm_dev, &dev_attr_adm);
            device_destroy(fingerprint_class, 0);
            class_destroy(fingerprint_class);
        }
    }
    fingerprint_class = NULL;
}

subsys_initcall(fingerprint_class_init);
module_exit(fingerprint_class_exit);

MODULE_DESCRIPTION("fingerprint sysfs class");
MODULE_LICENSE("GPL");
/*hs14 code for AL6528A-947 by zhangziyi at 2022/11/21 end*/
