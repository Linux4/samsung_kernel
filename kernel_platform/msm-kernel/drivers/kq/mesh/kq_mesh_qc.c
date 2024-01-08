/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * KQ(Kernel Quality) MESH driver implementation
 *  : Jaecheol Kim <jc22.kim@samsung.com>
 *    ChangHwi Seo <c.seo@samsung.com>
 *    Jiseong Lee <jsjsjs.lee@samsung.com>
 */

#include <asm/cputype.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/platform_device.h>
#include <linux/sec_class.h>
#include <linux/kq/kq_mesh_qc.h>

#if IS_ENABLED(CONFIG_MESH_KERNEL_MEMTEST)
#include <linux/kq/kq_mesh_kernel_memtest.h>
#endif
/* module param to determine driver loading or not loading */
static unsigned int kq_mesh_drv_loading;
module_param(kq_mesh_drv_loading, uint, 0440);

#if IS_ENABLED(CONFIG_MESH_KERNEL_MEMTEST)
static void kq_mesh_kernel_memtest(struct kq_mesh_info *kminfo)
{
	unsigned int size;
	unsigned int num_of_testarea;
	int ret;

	ret = sscanf(kminfo->mesh_arg[1], "%d\n", &size);
	if (ret != 1) goto invaild_arg;

	ret = sscanf(kminfo->mesh_arg[2], "%d\n", &num_of_testarea);
	if (ret != 1) goto invaild_arg;

	kernel_mem_test(size, num_of_testarea);
	
	return;
invaild_arg:
	pr_err("%s parsing failed due to invalid arguement\n", __func__);
	return;
}
#endif

/* func list of /sys/class/sec/sec_kq_mesh/func */
struct kq_mesh_func_name kq_mesh_func_list[] = {
#if IS_ENABLED(CONFIG_MESH_KERNEL_MEMTEST)
	{ "kernel_memtest", KQ_MESH_FEATURE_KERNEL_MEMTEST, kq_mesh_kernel_memtest},
#endif
	{ NULL, KQ_MESH_FEATURE_END, NULL},
};

/* temporary set always enable driver loading */
static inline bool kq_mesh_is_drv_enable(void)
{
	if (kq_mesh_drv_loading == KQ_MESH_IS_DRV_LOADING)
		return true;
	return false;
}

static inline bool kq_mesh_is_var_null(void *var)
{
	if (!var)
		return true;
	return false;
}

static inline bool kq_mesh_is_valid_process(void)
{
#if defined(CONFIG_SEC_FACTORY)
	return true;
#else
	struct task_struct *tsk = current;

	/* only mesh process can run this fuction */
	if (!strncmp(tsk->comm, KQ_MESH_VALID_PROCESS_MESH, KQ_MESH_VALID_PROCESS_LEN) ||
			!strncmp(tsk->comm, KQ_MESH_VALID_PROCESS_BPS, KQ_MESH_VALID_PROCESS_LEN))
		return true;

	pr_info("%s [%s] is not valid process!\n", __func__, tsk->comm);
	return false;
#endif
}

static inline int kq_mesh_get_next_start_index(const char *buf, size_t count, int index)
{
	while (index < count) {
		index++;
		if (buf[index] == '_')
			break;
	}
	return index + 1;
}

static inline bool kq_mesh_is_valid_func(int func)
{
	if (func == KQ_MESH_FEATURE_INIT ||
		func >= KQ_MESH_FEATURE_END)
		return false;
	return true;
}

static int kq_mesh_check_support_func_list(struct kq_mesh_info *kminfo, char *buf)
{
	int len = 0;
	return len;
}

static struct device_attribute kq_mesh_attrs[] = {
	KQ_MESH_ATTR(func),
	KQ_MESH_ATTR(result),
	KQ_MESH_ATTR(panic),
	KQ_MESH_ATTR(support),
};

static ssize_t kq_mesh_show_func_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct kq_mesh_info *kminfo = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", kminfo->last_func);
}

static ssize_t kq_mesh_show_result_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct kq_mesh_info *kminfo = dev_get_drvdata(dev);

	if (!kq_mesh_is_valid_func(kminfo->last_func))
		return 0;
	return 0;
}

static ssize_t kq_mesh_show_support_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct kq_mesh_info *kminfo = dev_get_drvdata(dev);

	return kq_mesh_check_support_func_list(kminfo, buf);
}

static ssize_t kq_mesh_show_attrs(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	const ptrdiff_t offset = attr - kq_mesh_attrs;
	int i = 0;

	pr_info("%s tsk [%s]\n", __func__, current->comm);

	switch (offset) {
	case KQ_MESH_SYSFS_FUNC:
		i = kq_mesh_show_func_attr(dev, attr, buf);
		break;
	case KQ_MESH_SYSFS_RESULT:
		i = kq_mesh_show_result_attr(dev, attr, buf);
		break;
	case KQ_MESH_SYSFS_PANIC:
		break;
	case KQ_MESH_SYSFS_SUPPORT:
		i = kq_mesh_show_support_attr(dev, attr, buf);
		break;
	default:
		pr_err("%s kq mesh out of range\n", __func__);
		i = -EINVAL;
		break;
	}
	return i;
}


static ssize_t kq_mesh_store_func_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct kq_mesh_info *kminfo = dev_get_drvdata(dev);
	struct kq_mesh_func_name *info;
	char temp_string[MESH_ARGSTR_LEN];
	char *tmp;
	char *ptr = NULL;
	int idx = 0;

	strlcpy(temp_string, buf, MESH_ARGSTR_LEN);
	tmp = temp_string;
	while((ptr = strsep(&tmp, ",")) != NULL && idx < MESH_ARG_NUM) {
		strlcpy(kminfo->mesh_arg[idx++], ptr, MESH_ARG_SIZE);
	}

	for (info = kq_mesh_func_list; info->func; info++) {
		if (!strncmp(kminfo->mesh_arg[0], info->name, sizeof((const char **)&info->name))) {
			pr_info("%s run %s func!\n", __func__, info->name);

			kminfo->last_func = info->type;
			info->func(kminfo);
		}
	}
	return count;
}

static ssize_t kq_mesh_store_result_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t kq_mesh_store_panic_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	panic("%s\n", buf);
	return count;
}

static ssize_t kq_mesh_store_support_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t kq_mesh_store_attrs(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	const ptrdiff_t offset = attr - kq_mesh_attrs;

	pr_info("%s tsk [%s]\n", __func__, current->comm);

	/* Only mesh can do these operations */
	if (!kq_mesh_is_valid_process())
		return count;

	switch (offset) {
	case KQ_MESH_SYSFS_FUNC:
		count = kq_mesh_store_func_attr(dev, attr, buf, count);
		break;
	case KQ_MESH_SYSFS_RESULT:
		count = kq_mesh_store_result_attr(dev, attr, buf, count);
		break;
	case KQ_MESH_SYSFS_PANIC:
		count = kq_mesh_store_panic_attr(dev, attr, buf, count);
		break;
	case KQ_MESH_SYSFS_SUPPORT:
		count = kq_mesh_store_support_attr(dev, attr, buf, count);
		break;
	default:
		pr_info("%s store kq mesh out of range\n", __func__);
		break;
	}
	return count;
}

static int kq_mesh_create_attr(struct device *dev)
{
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(kq_mesh_attrs); i++) {
		ret = device_create_file(dev, &kq_mesh_attrs[i]);
		if (ret)
			goto create_mesh_attr_fail;
	}
	return 0;

create_mesh_attr_fail:
	while (i--) {
		pr_err("%s remove %dth file\n", __func__, i);
		device_remove_file(dev, &kq_mesh_attrs[i]);
	}
	return -EINVAL;
}

static int kq_mesh_create_sysfs(struct platform_device *pdev,
	struct kq_mesh_info *kminfo)
{
	int ret;

	kminfo->sdev = sec_device_create(kminfo, "sec_kq_mesh");
	if (IS_ERR(kminfo->sdev)) {
		pr_err("%s sysfs create fail\n", __func__);
		return PTR_ERR(kminfo->sdev);
	}

	ret = kq_mesh_create_attr(kminfo->sdev);
	if (ret) {
		pr_err("%s attr sysfs fail\n", __func__);
		goto error_create_mesh_sysfs;
	}
	return 0;

error_create_mesh_sysfs:
	sec_device_destroy(kminfo->sdev->devt);
	return -EINVAL;
}

static struct kq_mesh_info *kq_mesh_alloc_init_info(struct platform_device *pdev)
{
	struct kq_mesh_info *kminfo;

	kminfo = devm_kzalloc(&pdev->dev,
				sizeof(struct kq_mesh_info), GFP_KERNEL);

	if (unlikely(kq_mesh_is_var_null(kminfo)))
		return NULL;

	kminfo->last_func = KQ_MESH_FEATURE_INIT;
	kminfo->support = KQ_MESH_SUPPORT_INIT;

	return kminfo;
}
static int kq_mesh_probe(struct platform_device *pdev)
{
	struct kq_mesh_info *kminfo = NULL;
	int ret = 0;

	if (!kq_mesh_is_drv_enable())
		return -EINVAL;

	pr_info("%s mesh drv loading..\n", __func__);

	kminfo = kq_mesh_alloc_init_info(pdev);
	if (unlikely(kq_mesh_is_var_null(kminfo))) {
		pr_err("%s failed to allocate mesh info\n", __func__);
		return ret;
	}

	ret = kq_mesh_create_sysfs(pdev, kminfo);
	if (ret) {
		pr_err("%s failed to create mesh sysfs\n", __func__);
		goto free_mesh_info;
	}

	platform_set_drvdata(pdev, kminfo);
	return ret;

free_mesh_info:
	devm_kfree(&pdev->dev, kminfo);
	return -EINVAL;
}

static int kq_mesh_remove(struct platform_device *pdev)
{
	return 0;
}
#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id of_kq_mesh_match[] = {
	{ .compatible = "samsung,kq-mesh", },
	{ },
};
#endif
MODULE_DEVICE_TABLE(of, of_kq_mesh_match);

static struct platform_driver kq_mesh_driver = {
	.probe = kq_mesh_probe,
	.remove = kq_mesh_remove,
	.driver = {
		.name = "kq-mesh",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = of_kq_mesh_match,
#endif
	},
};

static int __init kq_mesh_init(void)
{
	pr_info("%s platform_driver_register\n", __func__);
	return platform_driver_register(&kq_mesh_driver);
}

static void __exit kq_mesh_exit(void)
{
	return platform_driver_unregister(&kq_mesh_driver);
}

module_init(kq_mesh_init);
module_exit(kq_mesh_exit);

MODULE_DESCRIPTION("kernel quality mesh driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
