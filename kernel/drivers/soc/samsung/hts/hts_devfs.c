/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "hts_devfs.h"
#include "hts_sysfs.h"
#include "hts_core.h"
#include "hts_data.h"
#include "hts_pmu.h"
#include "hts_vh.h"
#include "hts_var.h"
#include "hts_command.h"

#include <linux/bitops.h>
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/workqueue.h>
#include <linux/kernel_stat.h>
#include <linux/skbuff.h>

static struct hts_core_util __percpu *hts_core_utilization;

static inline void hts_signal_workqueue(struct hts_data *data);
static inline void hts_cleanup_workqueue(struct hts_data *data);
static inline void hts_modify_signal_tick(struct hts_data *data, unsigned long ms);
static inline void hts_enable_evaluation_tick(struct hts_data *data);
static inline void hts_disable_evaluation_tick(struct hts_data *data);
static int hts_add_cgroup(struct hts_data *data, char *cgroup_name);
static int hts_configure_event(struct hts_data *data, unsigned long arg);
static void hts_clear_event(struct hts_data *data, unsigned long arg);
static void hts_set_log_mask(struct hts_data *data, unsigned long arg);
static void hts_set_ref_mask(struct hts_data *data, unsigned long arg);
static int hts_add_io_mask(unsigned long arg);
static void hts_clear_io_mask(void);
static int hts_set_predef_value(unsigned long arg);
static void hts_add_predef_value(void);
static void hts_clear_predef_value(void);
static int hts_set_core_threshold(struct hts_data *data, unsigned long arg);
static int hts_set_total_threshold(struct hts_data *data, unsigned long arg);
static void hts_initialize_core_activity(void);
static int hts_calculate_core_activity(struct hts_data *data);

static int hts_fops_open(struct inode *inode, struct file *filp)
{
	struct hts_private_data *data = kzalloc(sizeof(struct hts_private_data), GFP_KERNEL);

	if (data == NULL)
		return -ENOMEM;

	data->cpu = -1;
	filp->private_data = data;

        return 0;
}

static ssize_t hts_fops_write(struct file *filp, const char __user *buf,
                size_t count, loff_t *f_pos)
{
	struct hts_private_data *data = filp->private_data;
	struct hts_pmu *pmu_handle = NULL;
	int core;

	if (buf == NULL ||
		data == NULL ||
		count < sizeof(int))
		return -EINVAL;

	if (copy_from_user(&core, buf, sizeof(int)))
		return -EFAULT;

	if (core < 0 ||
		core >= CONFIG_VENDOR_NR_CPUS)
		return -EINVAL;

	pmu_handle = hts_get_core_handle(core);
	if (pmu_handle == NULL)
		return -EINVAL;

	data->pmu = pmu_handle;
	data->cpu = core;

	return sizeof(int);
}

static ssize_t hts_fops_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

static __poll_t hts_fops_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct hts_data *data = container_of(filp->f_op, struct hts_data, devfs.fops);
	unsigned long flags;
	__poll_t mask = 0;

	poll_wait(filp, &data->wait_queue, wait);

	spin_lock_irqsave(&data->lock, flags);
	if (data->enable_wait) {
		mask = EPOLLIN | EPOLLRDNORM;
		data->enable_wait = 0;
	}
	spin_unlock_irqrestore(&data->lock, flags);

	return mask;
}

#define PMU_BUFFER_PAGE_COUNT	1
static int hts_fops_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret = 0;
	struct hts_private_data *data = filp->private_data;
	unsigned long flags, requested_size = vma->vm_end - vma->vm_start;

	if (data == NULL ||
		data->pmu == NULL ||
		requested_size < PAGE_SIZE ||
		requested_size % DATA_UNIT_SIZE != 0)
		return -EINVAL;

	spin_lock_irqsave(&data->pmu->lock, flags);
	if (atomic_read(&data->pmu->ref_count) <= 0) {
		struct page *pages = __dev_alloc_pages(GFP_KERNEL | __GFP_ZERO,
						PMU_BUFFER_PAGE_COUNT);
		if (unlikely(!pages)) {
			ret = -ENOMEM;
			goto err_alloc;
		}

		data->pmu->pmu_pages = pages;
		data->pmu->pmu_buffer = page_to_virt(pages);
		data->pmu->buffer_size = requested_size / DATA_UNIT_SIZE - OFFSET_DEFAULT;
	} else {
		ret = -EINVAL;
		goto err_using_check;
	}
	atomic_inc(&data->pmu->ref_count);
	spin_unlock_irqrestore(&data->pmu->lock, flags);

	if (remap_pfn_range(vma,
			vma->vm_start,
			page_to_pfn(data->pmu->pmu_pages),
			requested_size,
			vma->vm_page_prot)) {
		ret = -EAGAIN;
		goto err_remap;
	}

	atomic_inc(&data->mmap_count);

	return 0;

err_remap:
	if (atomic_dec_and_test(&data->pmu->ref_count)) {
		spin_lock_irqsave(&data->pmu->lock, flags);
		__free_pages(data->pmu->pmu_pages, PMU_BUFFER_PAGE_COUNT);
		data->pmu->pmu_pages = NULL;
		data->pmu->pmu_buffer = NULL;
		data->pmu->buffer_size = 0;
		spin_unlock_irqrestore(&data->pmu->lock, flags);
	}

	return ret;

err_using_check:
err_alloc:
	spin_unlock_irqrestore(&data->pmu->lock, flags);

	return ret;
}

static long hts_fops_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct hts_data *data = container_of(filp->f_op, struct hts_data, devfs.fops);
	long ret = 0;

	switch (cmd) {
	case IOCTL_MODIFY_TICK:
		hts_modify_signal_tick(data, arg);
		break;
	case IOCTL_START_TICK:
		hts_enable_evaluation_tick(data);
		register_hts_ctx_switch_vh(data);
		hts_signal_workqueue(data);
		break;
	case IOCTL_STOP_TICK:
		hts_disable_evaluation_tick(data);
		unregister_hts_ctx_switch_vh(data);
		hts_core_reset_default();
		break;
	case IOCTL_CGROUP_ADD:
		ret = hts_add_cgroup(data, (char *)arg);
		break;
	case IOCTL_CGROUP_CLEAR:
		hts_clear_target_cgroup();
		break;
	case IOCTL_CONFIGURE_EVENT:
		ret = hts_configure_event(data, arg);
		break;
	case IOCTL_CLEAR_EVENT:
		hts_clear_event(data, arg);
		break;
	case IOCTL_SET_LOG_MASK:
		hts_set_log_mask(data, arg);
		break;
	case IOCTL_SET_REF_MASK:
		hts_set_ref_mask(data, arg);
		break;
	case IOCTL_MASK_ADD:
		ret = hts_add_io_mask(arg);
		break;
	case IOCTL_MASK_CLEAR:
		hts_clear_io_mask();
		break;
	case IOCTL_PREDEFINED_SET:
		ret = hts_set_predef_value(arg);
		break;
	case IOCTL_PREDEFINED_ADD:
		hts_add_predef_value();
		break;
	case IOCTL_PREDEFINED_CLEAR:
		hts_clear_predef_value();
		break;
	case IOCTL_SET_CORE_THRE:
		ret = hts_set_core_threshold(data, arg);
		break;
	case IOCTL_SET_TOTAL_THRE:
		ret = hts_set_total_threshold(data, arg);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int hts_fops_release(struct inode *node, struct file *filp)
{
	struct hts_private_data *data = filp->private_data;
	unsigned long flags;
	int mmap_count = 0;

	if (data == NULL ||
		data->pmu == NULL)
		return 0;

	mmap_count = atomic_read(&data->mmap_count);

	spin_lock_irqsave(&data->pmu->lock, flags);
	if (data->pmu != NULL &&
		data->pmu->pmu_buffer != NULL) {
		if (atomic_sub_return(mmap_count, &data->pmu->ref_count) == 0) {
			__free_pages(data->pmu->pmu_pages,
					PMU_BUFFER_PAGE_COUNT);
			data->pmu->pmu_pages = NULL;
			data->pmu->pmu_buffer = NULL;
		}
	}
	spin_unlock_irqrestore(&data->pmu->lock, flags);

	kfree(data);
	filp->private_data = NULL;

	return 0;
}

static inline void hts_modify_signal_tick(struct hts_data *data, unsigned long ms)
{
	data->tick_ms = ms;
}

static inline void hts_signal_workqueue(struct hts_data *data)
{
	if (!data->enable_tick)
		return;

	queue_delayed_work(data->hts_wq, &data->work, msecs_to_jiffies(data->tick_ms));
}

static inline void hts_cleanup_workqueue(struct hts_data *data)
{
	if (!data->enable_tick)
		return;

	cancel_delayed_work_sync(&data->work);
}

static inline void hts_enable_evaluation_tick(struct hts_data *data)
{
	hts_register_all_core_event();

	data->enable_tick = true;
	data->enabled_count++;
}

static inline void hts_disable_evaluation_tick(struct hts_data *data)
{
	int cpu;

	for_each_online_cpu(cpu)
		hts_release_core_event(cpu);

	data->enable_tick = false;
}

static int hts_add_cgroup(struct hts_data *data, char *cgroup_name)
{
	char buf[PID_BUFFER_LENGTH + 1];

	if (copy_from_user(&buf, cgroup_name, PID_BUFFER_LENGTH))
		return -EINVAL;

	hts_add_target_cgroup(buf);

	return 0;
}

static int hts_configure_event(struct hts_data *data, unsigned long arg)
{
	int cpu, dataCount, signature, eventData[MAXIMUM_EVENT_COUNT + 1];
	int *dataPtr = (int *)arg;

	if (copy_from_user(&signature, dataPtr, sizeof(int)) ||
		copy_from_user(&cpu, dataPtr + 1, sizeof(int)) ||
		copy_from_user(&dataCount, dataPtr + 2, sizeof(int))) {
		pr_err("HTS : Couldn't copy data from user for configuring event");
                return -EINVAL;
	}

	if (signature != SIGNATURE_ARGUMENT_START) {
		pr_err("HTS : Configuring event starting data is invalid!");
		return -EINVAL;
	}

	if (dataCount > MAXIMUM_EVENT_COUNT)
		dataCount = MAXIMUM_EVENT_COUNT;

	if (copy_from_user(eventData, dataPtr + 3, sizeof(int) * (dataCount + 1)))
		return -EINVAL;

	if (eventData[dataCount] != SIGNATURE_ARGUMENT_END) {
		pr_err("HTS : Configuring event ending data is invalid!");
		return -EINVAL;
	}

	if (hts_configure_core_events(cpu, eventData, dataCount)) {
		pr_err("HTS : Couldn't set event");
		return -EINVAL;
	}

	return 0;
}

static void hts_clear_event(struct hts_data *data, unsigned long arg)
{
	hts_clear_core_event((int)arg);
}

static void hts_set_log_mask(struct hts_data *data, unsigned long arg)
{
	int bit;

	cpumask_clear(&data->log_mask);
	for_each_set_bit(bit, &arg, CONFIG_VENDOR_NR_CPUS)
		cpumask_set_cpu(bit, &data->log_mask);
}

static void hts_set_ref_mask(struct hts_data *data, unsigned long arg)
{
	int bit;

	cpumask_clear(&data->ref_mask);
	for_each_set_bit(bit, &arg, CONFIG_VENDOR_NR_CPUS)
		cpumask_set_cpu(bit, &data->ref_mask);
}

static int hts_add_io_mask(unsigned long arg)
{
	unsigned int mask_data, *dataPtr = (unsigned int *)arg;

	if (copy_from_user(&mask_data, dataPtr, sizeof(unsigned int))) {
		pr_err("HTS : Couldn't copy dta from user for adding mask");
		return -EINVAL;
	}

	hts_add_mask(mask_data);

	return 0;
}

static void hts_clear_io_mask(void)
{
	hts_clear_mask();
}

static int hts_set_predef_value(unsigned long arg)
{
	unsigned long signature, dataCount = 0, type;
	unsigned long predefinedValue[MAXIMUM_PREDEFINED_VALUE_COUNT + 1];
	unsigned long *dataPtr = (unsigned long *)arg;

	if (copy_from_user(&signature, dataPtr, sizeof(unsigned long)) ||
		copy_from_user(&type, dataPtr + 1, sizeof(unsigned long)) ||
		copy_from_user(&dataCount, dataPtr + 2, sizeof(unsigned long))) {
		pr_err("HTS : Couldn't copy data from user for predefined value");
                return -EINVAL;
	}

	if (signature != SIGNATURE_ARGUMENT_START) {
		pr_err("HTS : Predefined value starting data is invalid!");
		return -EINVAL;
	}

	if (dataCount > MAXIMUM_PREDEFINED_VALUE_COUNT)
		dataCount = MAXIMUM_PREDEFINED_VALUE_COUNT;

	if (copy_from_user(predefinedValue, dataPtr + 3, sizeof(unsigned long) * (dataCount + 1)))
		return -EINVAL;

	if (predefinedValue[dataCount] != SIGNATURE_ARGUMENT_END) {
		pr_err("HTS : Predefined value ending data is invalid!");
		return -EINVAL;
	}

	hts_set_predefined_value(type, predefinedValue, dataCount);

	return 0;
}

static void hts_add_predef_value(void)
{
	hts_increase_predefined_index();
}

static void hts_clear_predef_value(void)
{
	hts_clear_predefined_value();
}

static int hts_set_core_threshold(struct hts_data *data, unsigned long arg)
{
	data->core_threshold = arg;

	return 0;
}

static int hts_set_total_threshold(struct hts_data *data, unsigned long arg)
{
	data->total_threshold = arg;

	return 0;
}

static void hts_initialize_core_activity(void)
{
	struct hts_core_util *core_util = NULL;
	int cpu;

	hts_core_utilization = alloc_percpu(struct hts_core_util);

	for_each_possible_cpu(cpu) {
		core_util = per_cpu_ptr(hts_core_utilization, cpu);
		kcpustat_cpu_fetch(&core_util->stat, cpu);
	}
}

static int hts_calculate_core_activity(struct hts_data *data)
{
	struct kernel_cpustat timestat;
	int cpu, core_count = 0, ret = 0;
	unsigned long active_time, idle_time, ratio, total_ratio = 0;
	struct hts_core_util *core_util = NULL;

	for_each_online_cpu(cpu) {
		if (!cpumask_test_cpu(cpu, &data->log_mask))
			continue;

		core_util = per_cpu_ptr(hts_core_utilization, cpu);

		kcpustat_cpu_fetch(&timestat, cpu);

		active_time = (timestat.cpustat[CPUTIME_USER] - core_util->stat.cpustat[CPUTIME_USER]) +
			(timestat.cpustat[CPUTIME_NICE] - core_util->stat.cpustat[CPUTIME_NICE]) +
			(timestat.cpustat[CPUTIME_SYSTEM] - core_util->stat.cpustat[CPUTIME_SYSTEM]) +
			(timestat.cpustat[CPUTIME_SOFTIRQ] - core_util->stat.cpustat[CPUTIME_SOFTIRQ]) +
			(timestat.cpustat[CPUTIME_IRQ] - core_util->stat.cpustat[CPUTIME_IRQ]);

		idle_time = (timestat.cpustat[CPUTIME_IDLE] - core_util->stat.cpustat[CPUTIME_IDLE]) +
			(timestat.cpustat[CPUTIME_IOWAIT] - core_util->stat.cpustat[CPUTIME_IOWAIT]);

		ratio = active_time * 100 / (active_time + idle_time);
		if (ratio >= data->core_threshold)
			ret = true;

		total_ratio += ratio;
		core_count++;

		core_util->stat.cpustat[CPUTIME_USER]		=	timestat.cpustat[CPUTIME_USER];
		core_util->stat.cpustat[CPUTIME_NICE]		=	timestat.cpustat[CPUTIME_NICE];
		core_util->stat.cpustat[CPUTIME_SYSTEM]		=	timestat.cpustat[CPUTIME_SYSTEM];
		core_util->stat.cpustat[CPUTIME_SOFTIRQ]	=	timestat.cpustat[CPUTIME_SOFTIRQ];
		core_util->stat.cpustat[CPUTIME_IRQ]		=	timestat.cpustat[CPUTIME_IRQ];
		core_util->stat.cpustat[CPUTIME_IDLE]		=	timestat.cpustat[CPUTIME_IDLE];
		core_util->stat.cpustat[CPUTIME_IOWAIT]		=	timestat.cpustat[CPUTIME_IOWAIT];
	}

	if (core_count && (total_ratio / core_count >= data->total_threshold))
		ret = true;

	return ret;
}

static void hts_monitor(struct work_struct *work)
{
	int flag = false;
	unsigned long flags;
	struct hts_data *data = container_of(work,
					struct hts_data, work.work);

	if (atomic_read(&data->req_value) < 0 &&
		hts_calculate_core_activity(data) &&
		data->enable_emsmode)
		flag = true;

	spin_lock_irqsave(&data->lock, flags);
	data->enable_wait = flag;
	spin_unlock_irqrestore(&data->lock, flags);
	wake_up_interruptible(&data->wait_queue);

	hts_signal_workqueue(data);
}

static int initialize_hts_deferrable_workqueue(struct platform_device *pdev)
{
	struct hts_data *data = platform_get_drvdata(pdev);

	data->tick_ms = DEFAULT_EVAL_WORKQUEUE_TICK_MS;
	data->hts_wq = create_freezable_workqueue("hts_wq");
	if (data->hts_wq == NULL) {
		pr_err("HTS : Couldn't create workqueue");
		return -ENOENT;
	}

	INIT_DEFERRABLE_WORK(&data->work, hts_monitor);

	data->enable_wait = false;
	init_waitqueue_head(&data->wait_queue);

	return 0;
}

int initialize_hts_devfs(struct platform_device *pdev)
{
	struct hts_data *data = platform_get_drvdata(pdev);
	struct hts_devfs *devfs = NULL;
	int ret;

	if (data == NULL) {
		pr_err("HTS : Couldn't get device data");
		return -EINVAL;
	}

	if (initialize_hts_deferrable_workqueue(pdev))
		goto err_workqueue;

	devfs = &data->devfs;
	if (devfs == NULL) {
		pr_err("HTS : Couldn't get devfs");
		return -EINVAL;
	}

	devfs->miscdev.minor            = MISC_DYNAMIC_MINOR;
	devfs->miscdev.name             = "hts";
	devfs->miscdev.fops             = &devfs->fops;

	devfs->fops.owner               = THIS_MODULE;
	devfs->fops.llseek              = noop_llseek;
	devfs->fops.open		= hts_fops_open;
	devfs->fops.write        	= hts_fops_write;
	devfs->fops.read                = hts_fops_read;
	devfs->fops.poll                = hts_fops_poll;
	devfs->fops.mmap		= hts_fops_mmap;
	devfs->fops.unlocked_ioctl      = hts_fops_ioctl;
	devfs->fops.compat_ioctl        = hts_fops_ioctl;
	devfs->fops.release             = hts_fops_release;

	ret = misc_register(&devfs->miscdev);
	if (ret) {
		pr_err("HTS : Couldn't register misc dev");
		return ret;
	}

	hts_initialize_core_activity();

	pr_info("HTS : file system has been completed");
err_workqueue:
	return 0;
}
