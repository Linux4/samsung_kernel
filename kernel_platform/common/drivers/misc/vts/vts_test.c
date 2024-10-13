// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2014-2021 Huaqin Electronics Co., Ltd. All Right Reserved.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/err.h>

static struct class *vts_class;

static void __exit vts_class_exit(void)
{
    class_destroy(vts_class);
}

static int __init vts_class_create(void)
{
    vts_class = class_create(THIS_MODULE, "vts_camera");
    if (IS_ERR(vts_class)) {
        pr_err("Failed to create class(vts_camera) %ld\n", PTR_ERR(vts_class));
        return PTR_ERR(vts_class);
    }

    return 0;
}

module_init(vts_class_create);
module_exit(vts_class_exit);

MODULE_AUTHOR("Huaqin Electronics");
MODULE_DESCRIPTION("vts_camera-class");
MODULE_LICENSE("GPL v2");
