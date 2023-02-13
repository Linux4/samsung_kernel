/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2020-01-20 File created.
 */

#if defined(CONFIG_FSM_SYSFS)
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/version.h>
#include "fsm_public.h"

static int g_fsm_class_inited = 0;

static ssize_t fsm_re25_show(struct class *class,
                struct class_attribute *attr, char *buf)
{
    struct fsm_cal_result *result = NULL;
    struct fsm_calib *info;
    int size;
    int dev;
    int ret;

    int temp;
    int imped_integer=0;
    int imped_decimals=0;


    size = sizeof(struct fsm_cal_result);
    result = (struct fsm_cal_result *)fsm_alloc_mem(size);
    if (!result) {
        pr_err("allocate memory failed");
        return -EINVAL;
    }
    memset(result, 0, size);

	result->ndev = fsm_dev_count();
	pr_info("ndev:%d", result->ndev);
    ret = fsm_test_result(result, size);
    size = 0;
    for (dev = 0; dev < result->ndev; dev++) {
        info = &result->info[dev];
        // [addr,re25,errcode]
        if((info->re25 >= 6400)&&(info->re25 < 8600))
            info->errcode=0;
        else
            info->errcode=1;
        temp=(info->re25*1);
        imped_integer=temp/1000;
        imped_decimals=temp%1000;

        size += scnprintf(buf + size, PAGE_SIZE, "[%02X,re=%d,temp_re=%d.%d,result=%d]",
                info->addr, info->re25, imped_integer, imped_decimals, info->errcode);
    }
    size += scnprintf(buf + size, PAGE_SIZE, "\n");
    fsm_free_mem((void **)&result);

    return size;
}

static ssize_t fsm_re25_store(struct class *class,
                struct class_attribute *attr, const char *buf, size_t len)
{
    return 0;
}

static ssize_t fsm_info_show(struct class *class,
                struct class_attribute *attr, char *buf)
{
    fsm_version_t version;
    struct preset_file *pfile;
    int dev_count;
    int len = 0;

    fsm_get_version(&version);
    len  = scnprintf(buf + len, PAGE_SIZE, "version: %s\n",
            version.code_version);
    len += scnprintf(buf + len, PAGE_SIZE, "branch : %s\n",
            version.git_branch);
    len += scnprintf(buf + len, PAGE_SIZE, "commit : %s\n",
            version.git_commit);
    len += scnprintf(buf + len, PAGE_SIZE, "date   : %s\n",
            version.code_date);
    pfile = fsm_get_presets();
    dev_count = (pfile ? pfile->hdr.ndev : 0);
    len += scnprintf(buf + len, PAGE_SIZE, "device : [%d, %d]\n",
            dev_count, fsm_dev_count());

    return len;
}

static ssize_t fsm_debug_store(struct class *class,
                struct class_attribute *attr, const char *buf, size_t len)
{
    fsm_config_t *cfg = fsm_get_config();
    int value = simple_strtoul(buf, NULL, 0);

    if (cfg) {
        cfg->i2c_debug = !!value;
    }
    pr_info("i2c debug: %s", (cfg->i2c_debug ? "ON" : "OFF"));

    return len;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
static struct class_attribute g_fsm_class_attrs[] = {
    __ATTR(fsm_re25, S_IRUGO|S_IWUSR, fsm_re25_show, fsm_re25_store),
    __ATTR(fsm_info, S_IRUGO, fsm_info_show, NULL),
    __ATTR(fsm_debug, S_IWUSR, NULL, fsm_debug_store),
    __ATTR_NULL
};

static struct class g_fsm_class = {
    .name = FSM_DRV_NAME,
    .class_attrs = g_fsm_class_attrs,
};

#else
static CLASS_ATTR_RW(fsm_re25);
static CLASS_ATTR_RO(fsm_info);
static CLASS_ATTR_WO(fsm_debug);

static struct attribute *fsm_class_attrs[] = {
    &class_attr_fsm_re25.attr,
    &class_attr_fsm_info.attr,
    &class_attr_fsm_debug.attr,
    NULL,
};
ATTRIBUTE_GROUPS(fsm_class);

/** Device model classes */
struct class g_fsm_class = {
    .name = FSM_DRV_NAME,
    .class_groups = fsm_class_groups,
};
#endif

int fsm_sysfs_init(struct device *dev)
{
    int ret;

    if (g_fsm_class_inited) {
        return MODULE_INITED;
    }
    // path: sys/class/$(FSM_DRV_NAME)
    ret = class_register(&g_fsm_class);
    if (!ret) {
        g_fsm_class_inited = 1;
    }

    return ret;
}

void fsm_sysfs_deinit(struct device *dev)
{
    class_unregister(&g_fsm_class);
    g_fsm_class_inited = 0;
}
#endif
