/*
 * linux/arch/arm/mach-mmp/fake-sysoff.c
 *
 * Author:      Hong Feng <hongfeng@marvell.com>
 * Copyright:   (C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#define SYSOFF	"sysoff"
#define SYSPREPARE "sysprepare"
#define SYSON "syson"
#define MONITOR_WORK_EXPIRE (10 * HZ)

static void system_poweroff(struct work_struct *ignored);
static DECLARE_DELAYED_WORK(sysoff_work, system_poweroff);

static enum {
	SYSOFF_STATE_NONE = 0,
	SYSOFF_STATE_PREPARE,
	SYSOFF_STATE_ENTER,
} sys_state;

static atomic_t systemoff_work_status;
static unsigned int g_block_onkey;

unsigned int fake_sysoff_status_query(void)
{
	if (SYSOFF_STATE_ENTER == sys_state)
		return 1;
	else
		return 0;
}

/* block on_key between "sysprepare" and 10s timer exist */
unsigned int fake_sysoff_block_onkey(void)
{
	return g_block_onkey;
}

void fake_sysoff_work_cancel(void)
{
	if (atomic_cmpxchg(&systemoff_work_status, 1, 0))
		cancel_delayed_work_sync(&sysoff_work);
}

void fake_sysoff_set_block_onkey(int block)
{
	g_block_onkey = block;
}

static void system_poweroff(struct work_struct *ignored)
{
	printk(KERN_ALERT "Sysoff failed after %ds!\n", MONITOR_WORK_EXPIRE/HZ);
	atomic_set(&systemoff_work_status, 0);
	kernel_power_off();
}

static void start_sysoff_work(void)
{
	if (!atomic_cmpxchg(&systemoff_work_status, 0, 1))
		schedule_delayed_work(&sysoff_work, MONITOR_WORK_EXPIRE);
}

static ssize_t sysoff_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	char *s = buf;

	if (SYSOFF_STATE_NONE == sys_state)
		s += sprintf(buf, SYSON"\n");
	else if (SYSOFF_STATE_ENTER == sys_state)
		s += sprintf(buf, SYSOFF"\n");
	else if (SYSOFF_STATE_PREPARE == sys_state)
		s += sprintf(buf, SYSPREPARE"\n");
	return s - buf;
}

static ssize_t sysoff_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	if (strncmp(buf, SYSPREPARE, strlen(SYSPREPARE)) &&
		strncmp(buf, SYSON, strlen(SYSON)) &&
		strncmp(buf, SYSOFF, strlen(SYSOFF))) {

		printk(KERN_ERR "Wrong status:");
		printk(KERN_ERR "pls use sysprepare, sysoff, syson\n");
		return count;
	}
	switch (sys_state) {
	case SYSOFF_STATE_NONE:
		if (!strncmp(buf, SYSPREPARE, strlen(SYSPREPARE))) {
			sys_state = SYSOFF_STATE_PREPARE;
			fake_sysoff_set_block_onkey(1);
		} else {
			/* nothing */
		}
		break;
	case SYSOFF_STATE_PREPARE:
		if (!strncmp(buf, SYSON, strlen(SYSON))) {
			sys_state = SYSOFF_STATE_NONE;
			fake_sysoff_set_block_onkey(0);
		} else if (!strncmp(buf, SYSOFF, strlen(SYSOFF))) {
			sys_state = SYSOFF_STATE_ENTER;
			/*
			 * Kernel start MONITOR_WORK_EXPIRE timer to monitor
			 * the sequence, if timeout, power off the system
			 * directly; if within MONITOR_WORK_EXPIRE, code run
			 * to platform->enter, cancel timer there
			 */
			start_sysoff_work();
		} else {
			/* nothing */
		}
		break;
	case SYSOFF_STATE_ENTER:
		if (!strncmp(buf, SYSON, strlen(SYSON))) {
			sys_state = SYSOFF_STATE_NONE;
			fake_sysoff_set_block_onkey(0);
			fake_sysoff_work_cancel();
		} else {
			/* nothing */
		}
		break;
	default:
		printk(KERN_ERR "invalid sysoff status\n");
	}

	printk(KERN_INFO "set Current sys_state = %u\n", sys_state);

	return count;
}

static struct kobj_attribute system_off_attr = {
	.attr	= {
		.name = SYSOFF,
		.mode = 0644,
	},
	.show	= sysoff_show,
	.store	= sysoff_store,
};

static struct attribute *g[] = {
	&system_off_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

static int __init android_sysoff_init(void)
{
	int ret;

	sys_state = SYSOFF_STATE_NONE;
	atomic_set(&systemoff_work_status, 0);

	ret = sysfs_create_group(power_kobj, &attr_group);
	if (ret) {
		printk(KERN_ERR "%s: sysfs_create_group failed\n", __func__);
		return ret;
	}

	return 0;
}

static void  __exit android_sysoff_exit(void)
{
	sysfs_remove_group(power_kobj, &attr_group);
}

module_init(android_sysoff_init);
module_exit(android_sysoff_exit);
