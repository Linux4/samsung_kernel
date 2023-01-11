/*
 * Input touch boost feature support
 *
 * Copyright (C) 2014 Marvell Technology Group Ltd.
 *
 * Authors: Lu Cao <lucao@marvell.com>
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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/pm_qos.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DEFAULT_BOOST_TIME		200000
#define MAX_FREQ			0x7FFFFFFF
#define MIN_DDR_FREQ			416000

struct input_boost_config {
	unsigned long boost_cpu_freq;
	unsigned long boost_cpu_time;

/* above code is used for little cluster */
#ifdef CONFIG_ARM_MMP_BL_CPUFREQ
        unsigned long boost_big_cpu_freq;
        unsigned long boost_big_cpu_time;
#endif

	unsigned long boost_ddr_freq;
	unsigned long boost_ddr_time;

	unsigned long boost_gc3d_freq;
	unsigned long boost_gc3d_time;

	unsigned long boost_gc2d_freq;
	unsigned long boost_gc2d_time;

	unsigned long boost_gcsh_freq;
	unsigned long boost_gcsh_time;

	unsigned long boost_vpu_freq;
	unsigned long boost_vpu_time;
};

static struct input_boost_config boost_config = {
	MAX_FREQ, DEFAULT_BOOST_TIME,
#ifdef CONFIG_ARM_MMP_BL_CPUFREQ
	MAX_FREQ, DEFAULT_BOOST_TIME,
#endif
	MIN_DDR_FREQ, DEFAULT_BOOST_TIME,
	MAX_FREQ, DEFAULT_BOOST_TIME,
	MAX_FREQ, DEFAULT_BOOST_TIME,
	MAX_FREQ, DEFAULT_BOOST_TIME,
	MAX_FREQ, DEFAULT_BOOST_TIME,
};

static struct pm_qos_request inputboost_cpu_qos_min = {
	.name = "input_boost",
};

#ifdef CONFIG_ARM_MMP_BL_CPUFREQ
static struct pm_qos_request inputboost_big_cpu_qos_min = {
	.name = "input_boost",
};
#endif

#ifdef CONFIG_DDR_DEVFREQ
static struct pm_qos_request touchboost_ddr_qos_min = {
	.name = "input_boost",
};
#endif

#if 0
static struct pm_qos_request touchboost_gpu3d_qos_min = {
	.name = "input_boost",
};

static struct pm_qos_request touchboost_gpu2d_qos_min = {
	.name = "input_boost",
};

static struct pm_qos_request touchboost_gpush_qos_min = {
	.name = "input_boost",
};


#ifdef CONFIG_VPU_DEVFREQ
static struct pm_qos_request touchboost_vpu_qos_min = {
	.name = "input_boost",
};
#endif
#endif

static unsigned int boost_enabled;
static struct work_struct inputboost_wk;

/*
 * The reason why we use workqueue here is input event report handler
 * is called in irq disable content, but core freq-chg related function
 * cpufreq_notify_transition request irq is enabled.
 * Call chain as below:
 * input_event(irq disable here) -> input_handle_event-> input_pass_event->
 * handler->event(handle, type, code, value) = touchboost_event->
 * inputboost_work
 */
static void inputboost_work(struct work_struct *w)
{
#ifndef CONFIG_INPUT_BOOSTER
	/* boost cpu to max frequency 200ms by default */
	if (boost_config.boost_cpu_time)
		pm_qos_update_request_timeout(&inputboost_cpu_qos_min,
				boost_config.boost_cpu_freq, boost_config.boost_cpu_time);

#ifdef CONFIG_ARM_MMP_BL_CPUFREQ
	if (boost_config.boost_big_cpu_time)
		pm_qos_update_request_timeout(&inputboost_big_cpu_qos_min,
				boost_config.boost_big_cpu_freq, boost_config.boost_big_cpu_time);
#endif
#endif

#ifdef CONFIG_DDR_DEVFREQ
	/* boost ddr to a limit frequency */
	if (boost_config.boost_ddr_time)
		pm_qos_update_request_timeout(&touchboost_ddr_qos_min,
				boost_config.boost_ddr_freq, boost_config.boost_ddr_time);
#endif

#if 0
	/* boost gpu0(3D) to a limit frequency */
	if (boost_config.boost_gc3d_time)
		pm_qos_update_request_timeout(&touchboost_gpu3d_qos_min,
				boost_config.boost_gc3d_freq, boost_config.boost_gc3d_time);

	/* boost gpu1(2D) to a limit frequency */
	if (boost_config.boost_gc2d_time)
		pm_qos_update_request_timeout(&touchboost_gpu2d_qos_min,
				boost_config.boost_gc2d_freq, boost_config.boost_gc2d_time);

	/* boost shader to a limit frequency */
	if (boost_config.boost_gcsh_time)
		pm_qos_update_request_timeout(&touchboost_gpush_qos_min,
				boost_config.boost_gcsh_freq, boost_config.boost_gcsh_time);


#ifdef CONFIG_VPU_DEVFREQ
	/* boost vpu to a limit frequency */
	if (boost_config.boost_vpu_time)
		pm_qos_update_request_timeout(&touchboost_vpu_qos_min,
				boost_config.boost_vpu_freq, boost_config.boost_vpu_time);
#endif
#endif

}

static void keyboost_event(struct input_handle *handle, unsigned int type,
		unsigned int code, int value)
{
	if (boost_enabled && type == EV_KEY && value == 1)
		schedule_work(&inputboost_wk);
}

static void touchboost_event(struct input_handle *handle, unsigned int type,
		unsigned int code, int value)
{
	if (boost_enabled && code == ABS_MT_TRACKING_ID && value != 0xffffffff)
		schedule_work(&inputboost_wk);
}

static int touchboost_connect(struct input_handler *handler,
				  struct input_dev *dev,
				  const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	pr_info("%s: connect to %s\n", __func__, dev->name);
	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "input_boost";

	error = input_register_handle(handle);
	if (error) {
		pr_err("Failed to register input boost handler, error %d\n",
		       error);
		goto err;
	}

	error = input_open_device(handle);
	if (error) {
		pr_err("Failed to open input boost device, error %d\n", error);
		input_unregister_handle(handle);
		goto err;
	}

	return 0;
err:
	kfree(handle);
	return error;
}

static void touchboost_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id touchboost_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
		    INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
		       BIT_MASK(ABS_MT_POSITION_X) |
		       BIT_MASK(ABS_MT_POSITION_Y) },
	}, /* multi-touch touchscreen */
	{
		.flags = INPUT_DEVICE_ID_MATCH_KEYBIT |
		    INPUT_DEVICE_ID_MATCH_ABSBIT,
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		.absbit = { [BIT_WORD(ABS_X)] =
		       BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) },
	}, /* touchpad */
	{ },
};

static const struct input_device_id keyboost_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_KEYBIT |
		    INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_KEY)},
	}, /* keyboard */
};

#ifdef CONFIG_DEBUG_FS
#define INPUT_BOOST_CONFIG_OPS(name)					\
static ssize_t name##_read(struct file *file, char __user *user_buf,	\
			       size_t count, loff_t *ppos)		\
{									\
	char val[20];					\
	sprintf(val, "%d\n", (int)boost_config.boost_##name);		\
	return simple_read_from_buffer(user_buf, count, ppos, val, strlen(val));	\
}									\
static ssize_t name##_write(struct file *filp,			\
		const char __user *buffer, size_t count, loff_t *ppos)	\
{									\
	unsigned long data;						\
	int ret = 0;							\
	char buf[20] = { 0 };						\
												\
	if (count >= sizeof(buf))					\
		return -EINVAL;							\
	if (copy_from_user(buf, buffer, count))				\
		return -EFAULT;						\
											\
	buf[count] = 0;							\
	ret = kstrtoul(buf, 10, &data);			\
	if (ret < 0) {							\
		pr_err("%s: input <ERR> wrong parameter!\n", __func__);	\
		return ret;					\
	}									\
	boost_config.boost_##name = data;				\
									\
	*ppos += count;					\
	return count;					\
}										\
										\
static const struct file_operations name##_ops = {			\
	.read		= name##_read,					\
	.write		= name##_write,				\
};									\

INPUT_BOOST_CONFIG_OPS(ddr_freq);
INPUT_BOOST_CONFIG_OPS(ddr_time);

INPUT_BOOST_CONFIG_OPS(cpu_freq);
INPUT_BOOST_CONFIG_OPS(cpu_time);

#ifdef CONFIG_ARM_MMP_BL_CPUFREQ
INPUT_BOOST_CONFIG_OPS(big_cpu_freq);
INPUT_BOOST_CONFIG_OPS(big_cpu_time);
#endif

INPUT_BOOST_CONFIG_OPS(gc2d_freq);
INPUT_BOOST_CONFIG_OPS(gc2d_time);

INPUT_BOOST_CONFIG_OPS(gc3d_freq);
INPUT_BOOST_CONFIG_OPS(gc3d_time);

INPUT_BOOST_CONFIG_OPS(gcsh_freq);
INPUT_BOOST_CONFIG_OPS(gcsh_time);

INPUT_BOOST_CONFIG_OPS(vpu_freq);
INPUT_BOOST_CONFIG_OPS(vpu_time);

#endif

static struct input_handler touchboost_handler = {
	.event =	touchboost_event,
	.connect =	touchboost_connect,
	.disconnect =	touchboost_disconnect,
	.name =		"touch_boost",
	.id_table =	touchboost_ids,
};

static struct input_handler keyboost_handler = {
	.event =	keyboost_event,
	.connect =	touchboost_connect,
	.disconnect =	touchboost_disconnect,
	.name =		"key_boost",
	.id_table =	keyboost_ids,
};

static int __init boost_init(void)
{
	int ret1, ret2;
	struct dentry *boost_node = NULL;
	struct dentry *cpu_freq = NULL, *cpu_time = NULL;
#ifdef CONFIG_ARM_MMP_BL_CPUFREQ
	struct dentry *big_cpu_freq = NULL, *big_cpu_time = NULL;
#endif
	struct dentry *ddr_freq = NULL, *ddr_time = NULL;
	struct dentry *gc2d_freq = NULL, *gc2d_time = NULL;
	struct dentry *gc3d_freq = NULL, *gc3d_time = NULL;
	struct dentry *gcsh_freq = NULL, *gcsh_time = NULL;
	struct dentry *vpu_freq = NULL, *vpu_time = NULL;

#ifndef CONFIG_ARM_MMP_BL_CPUFREQ
	pm_qos_add_request(&inputboost_cpu_qos_min,
		PM_QOS_CPUFREQ_MIN, PM_QOS_DEFAULT_VALUE);
#else
	pm_qos_add_request(&inputboost_cpu_qos_min,
		PM_QOS_CPUFREQ_L_MIN, PM_QOS_DEFAULT_VALUE);

	pm_qos_add_request(&inputboost_big_cpu_qos_min,
		PM_QOS_CPUFREQ_B_MIN, PM_QOS_DEFAULT_VALUE);
#endif

#ifdef CONFIG_DDR_DEVFREQ
	pm_qos_add_request(&touchboost_ddr_qos_min,
		   PM_QOS_DDR_DEVFREQ_MIN, PM_QOS_DEFAULT_VALUE);
#endif

#if 0
	pm_qos_add_request(&touchboost_gpu3d_qos_min,
		PM_QOS_GPUFREQ_3D_MIN, PM_QOS_DEFAULT_VALUE);

	pm_qos_add_request(&touchboost_gpu2d_qos_min,
		PM_QOS_GPUFREQ_2D_MIN, PM_QOS_DEFAULT_VALUE);

	pm_qos_add_request(&touchboost_gpush_qos_min,
		PM_QOS_GPUFREQ_SH_MIN, PM_QOS_DEFAULT_VALUE);


#ifdef CONFIG_VPU_DEVFREQ
	pm_qos_add_request(&touchboost_vpu_qos_min,
		PM_QOS_VPU_DEVFREQ_MIN, PM_QOS_DEFAULT_VALUE);
#endif
#endif

	INIT_WORK(&inputboost_wk, inputboost_work);

	ret1 = input_register_handler(&touchboost_handler);
	if (ret1)
		pr_err("touchboost_handler register failed!\n");
	ret2 = input_register_handler(&keyboost_handler);
	if (ret2)
		pr_err("keyboost_handler register failed\n");

	if (ret1 && ret2)
		return -ENOENT;

#ifdef CONFIG_DEBUG_FS
	debugfs_create_u32("inputbst_enable", 0644, NULL,
		&boost_enabled);

	boost_node = debugfs_create_dir("inputbst_config", NULL);
	if (!boost_node) {
		pr_err("debugfs inputbst_config created failed in %s\n", __func__);
		return -ENOENT;
	}

#ifdef CONFIG_ARM_MMP_BL_CPUFREQ
	big_cpu_freq = debugfs_create_file("big_cpu_freq",
			S_IRUGO | S_IWUSR | S_IWGRP, boost_node, NULL, &big_cpu_freq_ops);
	if (!big_cpu_freq)
		goto err_big_cpu_freq;

	big_cpu_time = debugfs_create_file("big_cpu_time",
			S_IRUGO | S_IWUSR | S_IWGRP, boost_node, NULL, &big_cpu_time_ops);
	if (!big_cpu_time)
		goto err_big_cpu_time;
#endif

	cpu_freq = debugfs_create_file("cpu_freq",
			S_IRUGO | S_IWUSR | S_IWGRP, boost_node, NULL, &cpu_freq_ops);
	if (!cpu_freq)
		goto err_cpu_freq;

	ddr_freq = debugfs_create_file("ddr_freq",
			S_IRUGO | S_IWUSR | S_IWGRP, boost_node, NULL, &ddr_freq_ops);
	if (!ddr_freq)
		goto err_ddr_freq;

	gc2d_freq = debugfs_create_file("gc2d_freq",
			S_IRUGO | S_IWUSR | S_IWGRP, boost_node, NULL, &gc2d_freq_ops);
	if (!gc2d_freq)
		goto err_gc2d_freq;

	gc3d_freq = debugfs_create_file("gc3d_freq",
			S_IRUGO | S_IWUSR | S_IWGRP, boost_node, NULL, &gc3d_freq_ops);
	if (!gc3d_freq)
		goto err_gc3d_freq;

	gcsh_freq = debugfs_create_file("gcsh_freq",
			S_IRUGO | S_IWUSR | S_IWGRP, boost_node, NULL, &gcsh_freq_ops);
	if (!gcsh_freq)
		goto err_gcsh_freq;

	vpu_freq = debugfs_create_file("vpu_freq",
			S_IRUGO | S_IWUSR | S_IWGRP, boost_node, NULL, &vpu_freq_ops);
	if (!vpu_freq)
		goto err_vpu_freq;

	cpu_time = debugfs_create_file("cpu_time",
			S_IRUGO | S_IWUSR | S_IWGRP, boost_node, NULL, &cpu_time_ops);
	if (!cpu_time)
		goto err_cpu_time;

	ddr_time = debugfs_create_file("ddr_time",
			S_IRUGO | S_IWUSR | S_IWGRP, boost_node, NULL, &ddr_time_ops);
	if (!ddr_time)
		goto err_ddr_time;

	gc2d_time = debugfs_create_file("gc2d_time",
			S_IRUGO | S_IWUSR | S_IWGRP, boost_node, NULL, &gc2d_time_ops);
	if (!gc2d_time)
		goto err_gc2d_time;

	gc3d_time = debugfs_create_file("gc3d_time",
			S_IRUGO | S_IWUSR | S_IWGRP, boost_node, NULL, &gc3d_time_ops);
	if (!gc3d_time)
		goto err_gc3d_time;

	gcsh_time = debugfs_create_file("gcsh_time",
			S_IRUGO | S_IWUSR | S_IWGRP, boost_node, NULL, &gcsh_time_ops);
	if (!gcsh_time)
		goto err_gcsh_time;

	vpu_time = debugfs_create_file("vpu_time",
			S_IRUGO | S_IWUSR | S_IWGRP, boost_node, NULL, &vpu_time_ops);
	if (!vpu_time)
		goto err_vpu_time;
#endif

	return 0;

#ifdef CONFIG_DEBUG_FS
err_vpu_time:
	debugfs_remove(gcsh_time);
err_gcsh_time:
	debugfs_remove(gc3d_time);
err_gc3d_time:
	debugfs_remove(gc2d_time);
err_gc2d_time:
	debugfs_remove(ddr_time);
err_ddr_time:
	debugfs_remove(cpu_time);
err_cpu_time:
	debugfs_remove(vpu_freq);
err_vpu_freq:
	debugfs_remove(gcsh_freq);
err_gcsh_freq:
	debugfs_remove(gc3d_freq);
err_gc3d_freq:
	debugfs_remove(gc2d_freq);
err_gc2d_freq:
	debugfs_remove(ddr_freq);
err_ddr_freq:
	debugfs_remove(cpu_freq);
err_cpu_freq:
#ifdef CONFIG_ARM_MMP_BL_CPUFREQ
	debugfs_remove(big_cpu_time);
err_big_cpu_time:
	debugfs_remove(big_cpu_freq);
err_big_cpu_freq:
#endif
	debugfs_remove(boost_node);
	pr_err("debugfs entry created failed in %s\n", __func__);
	return -ENOENT;
#endif

}

static void __exit boost_exit(void)
{
	flush_work(&inputboost_wk);
	input_unregister_handler(&touchboost_handler);
	pm_qos_remove_request(&inputboost_cpu_qos_min);
}

module_init(boost_init);
module_exit(boost_exit);

MODULE_DESCRIPTION("Input touch boost feature support for Marvell MMP");
MODULE_LICENSE("GPL");
