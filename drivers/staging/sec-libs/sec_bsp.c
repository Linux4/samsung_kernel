/* drivers/staging/sec-libs/sec_bsp.c
 *
 * Copyright (C) 2014-2015 Samsung Electronics Co, Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/smp.h>
#include <linux/sched.h>

#include <asm/sec/sec_bsp.h>
#include <asm/sec/sec_debug.h>

#define BOOT_EVT_PREFIX			"!@Boot"
#define BOOT_EVT_PREFIX_PLATFORM	": "
#define BOOT_EVT_PREFIX_RIL		"_SVC : "

#define TIME_DIV			1000000

static const char *boot_prefix[16] = {
	BOOT_EVT_PREFIX BOOT_EVT_PREFIX_PLATFORM,
	BOOT_EVT_PREFIX BOOT_EVT_PREFIX_RIL
};

struct boot_event {
	unsigned int type;
	unsigned int prefix;
	const char *string;
	unsigned int time;
};

enum boot_events_prefix {
	EVT_PLATFORM,
	EVT_RIL,
};

enum boot_events_type {
	SYSTEM_START_INIT_PROCESS,
	PLATFORM_START_PRELOAD_RESOURCES,
	PLATFORM_START_PRELOAD_CLASSES,
	PLATFORM_END_PRELOAD_CLASSES,
	PLATFORM_END_PRELOAD_RESOURCES,
	PLATFORM_START_INIT_AND_LOOP,
	PLATFORM_START_PACKAGEMANAGERSERVICE,
	PLATFORM_END_PACKAGEMANAGERSERVICE,
	PLATFORM_END_INIT_AND_LOOP,
	PLATFORM_ENABLE_SCREEN,
	PLATFORM_BOOT_COMPLETE,
	PLATFORM_VOICE_SVC,
	PLATFORM_DATA_SVC,
	RIL_UNSOL_RIL_CONNECTED,
	RIL_SETRADIOPOWER_ON,
	RIL_SETUICCSUBSCRIPTION,
	RIL_SIM_RECORDSLOADED,
	RIL_RUIM_RECORDSLOADED,
	RIL_SETUPDATACALL,
};

static struct boot_event boot_events[] = {
	{SYSTEM_START_INIT_PROCESS, EVT_PLATFORM,
			"start init process", 0},
	{PLATFORM_START_PRELOAD_RESOURCES, EVT_PLATFORM,
			"beginofpreloadResources()", 0},
	{PLATFORM_START_PRELOAD_CLASSES, EVT_PLATFORM,
			"beginofpreloadClasses()", 0},
	{PLATFORM_END_PRELOAD_CLASSES, EVT_PLATFORM,
			"EndofpreloadClasses()", 0},
	{PLATFORM_END_PRELOAD_RESOURCES, EVT_PLATFORM,
			"End of preloadResources()", 0},
	{PLATFORM_START_INIT_AND_LOOP, EVT_PLATFORM,
			"Entered the Android system server!", 0},
	{PLATFORM_START_PACKAGEMANAGERSERVICE, EVT_PLATFORM,
			"Start PackageManagerService", 0},
	{PLATFORM_END_PACKAGEMANAGERSERVICE, EVT_PLATFORM,
			"End PackageManagerService", 0},
	{PLATFORM_END_INIT_AND_LOOP, EVT_PLATFORM,
			"Loop forever", 0},
	{PLATFORM_ENABLE_SCREEN, EVT_PLATFORM,
			"Enabling Screen!", 0},
	{PLATFORM_BOOT_COMPLETE, EVT_PLATFORM,
			"bootcomplete", 0},
	{PLATFORM_VOICE_SVC, EVT_PLATFORM,
			"Voice SVC is acquired", 0},
	{PLATFORM_DATA_SVC, EVT_PLATFORM,
			"Data SVC is acquired", 0},
	{RIL_UNSOL_RIL_CONNECTED, EVT_RIL,
			"RIL_UNSOL_RIL_CONNECTED", 0},
	{RIL_SETRADIOPOWER_ON, EVT_RIL,
			"setRadioPower on", 0},
	{RIL_SETUICCSUBSCRIPTION, EVT_RIL,
			"setUiccSubscription", 0},
	{RIL_SIM_RECORDSLOADED, EVT_RIL,
			"SIM onAllRecordsLoaded", 0},
	{RIL_RUIM_RECORDSLOADED, EVT_RIL,
			"RUIM onAllRecordsLoaded", 0},
	{RIL_SETUPDATACALL, EVT_RIL,
			"setupDataCall", 0},
	{0, 0, NULL, 0},
};

static int sec_boot_stat_proc_show(struct seq_file *m, void *v)
{
	size_t i;
	unsigned int delta = 0;

	seq_printf(m, "%-32s%s%13s\n", "boot event", "time (ms)", "delta");
	seq_puts(m, "-----------------------------------------------------\n");

	for (i = 0; boot_events[i].string; i++) {
		char boot_string[256];

		sprintf(boot_string, "%s%s",
				boot_prefix[boot_events[i].prefix],
				boot_events[i].string);

		seq_printf(m, "%-35s : %5u    %5d\n",
				boot_string,
				boot_events[i].time, delta);
		delta = boot_events[i + 1].time - boot_events[i].time;
	}

	return 0;
}

static int sec_boot_stat_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, sec_boot_stat_proc_show, NULL);
}

static const struct file_operations sec_boot_stat_proc_fops = {
	.open    = sec_boot_stat_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

void sec_boot_stat_add(const char * c)
{
	size_t i = 0;
	unsigned int prefix;
	char *android_log;
	unsigned long long t;

	if (strncmp(c, BOOT_EVT_PREFIX, 6))
		return;

	android_log = (char *)(c + 6);
	if (!strncmp(android_log, BOOT_EVT_PREFIX_PLATFORM, 2)) {
		prefix = EVT_PLATFORM;
		android_log = (char *)(android_log + 2);
	} else if (!strncmp(android_log, BOOT_EVT_PREFIX_RIL, 7)) {
		prefix = EVT_RIL;
		android_log = (char *)(android_log + 7);
	} else
		return;

	for (i = 0; boot_events[i].string; i++) {
		if (!strcmp(android_log, boot_events[i].string)) {
			t = local_clock();
			do_div(t, TIME_DIV);
			boot_events[i].time = (unsigned int)t;
			break;
		}
	}
}
EXPORT_SYMBOL(sec_boot_stat_add);

static struct device *sec_bsp_dev;
static struct proc_dir_entry *entry;

static ssize_t store_boot_stat(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long long t;

	if(!strncmp(buf, "!@Boot: start init process", 26)) {
		t = local_clock();
		do_div(t, TIME_DIV);
		boot_events[SYSTEM_START_INIT_PROCESS].time = (unsigned int)t;
	}

	return count;
}

static DEVICE_ATTR(boot_stat, S_IWUSR | S_IWGRP, NULL, store_boot_stat);

static int __init sec_bsp_init(void)
{
	int ret = -ENOMEM;

	entry = proc_create("boot_stat", S_IRUGO, NULL,
				&sec_boot_stat_proc_fops);
	if (unlikely(!entry))
		goto out;

	BUG_ON(!sec_class);

	sec_bsp_dev = device_create(sec_class, NULL, 0, NULL, "bsp");
	if (unlikely(IS_ERR(sec_bsp_dev))) {
		pr_err("(%s): Failed to create sec_bsp_dev!\n", __func__);
		ret = PTR_ERR(sec_bsp_dev);
		goto err_dev_create;
	}

	ret = device_create_file(sec_bsp_dev, &dev_attr_boot_stat);
	if (unlikely(ret < 0)) {
		pr_err("(%s): Failed to create sec_device file!\n",
						__func__);
		goto err_dev_create_file;
	}

	return 0;

err_dev_create_file:
	device_destroy(sec_class, 0);
err_dev_create:
	proc_remove(entry);
out:
	return ret;
}

static void __exit sec_bsp_exit(void)
{
	device_remove_file(sec_bsp_dev, &dev_attr_boot_stat);
	device_destroy(sec_class, 0);
	proc_remove(entry);
}

module_init(sec_bsp_init);
module_exit(sec_bsp_exit);
