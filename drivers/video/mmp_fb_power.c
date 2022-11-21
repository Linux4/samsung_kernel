/* kernel/power/fbearlysuspend.c
 *
 * Copyright (C) 2005-2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/wait.h>
#include <linux/device.h>
#include <linux/utsname.h>

static wait_queue_head_t fb_state_wq;
static DEFINE_SPINLOCK(fb_state_lock);
static enum {
	FB_STATE_STOPPED_DRAWING,
	FB_STATE_REQUEST_STOP_DRAWING,
	FB_STATE_DRAWING_OK,
} fb_state;

/*
 * In old early suspend/late resume scenario, the two function are used
 * to change fb_state, it is called in the "early suspend thread context",
 * and  it is synchronized with "androdi surface flinger thread" by
 * wake_queue scheme.
 * Now, for kernel 3.4, there is no "early suspend thread " in kernel side,
 * then we will use pm_runtime interface to support "android level power
 * thread " to do the same things as old "early suspend thread", the
 * synchronization scheme is the same as before.
 * Currently we follow the same sequence as kernel 3.0, android_stop_drawing
 * will be called in pxa168fb_runtime_suspend first, android_start_drawing
 * will be called in pxa168fb_runtime_resume after.
 */

/* tell userspace to stop drawing, wait for it to stop */
void android_stop_drawing(void)
{
	int ret;
	unsigned long irq_flags;

	spin_lock_irqsave(&fb_state_lock, irq_flags);
	fb_state = FB_STATE_REQUEST_STOP_DRAWING;
	spin_unlock_irqrestore(&fb_state_lock, irq_flags);

	wake_up_all(&fb_state_wq);
	ret = wait_event_timeout(fb_state_wq,
				 fb_state == FB_STATE_STOPPED_DRAWING,
				 HZ);
	if (unlikely(fb_state != FB_STATE_STOPPED_DRAWING))
		pr_warning("stop_drawing_early_suspend: timeout waiting for "
			   "userspace to stop drawing\n");
}
EXPORT_SYMBOL_GPL(android_stop_drawing);

/* tell userspace to start drawing */
void android_start_drawing(void)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&fb_state_lock, irq_flags);
	fb_state = FB_STATE_DRAWING_OK;
	spin_unlock_irqrestore(&fb_state_lock, irq_flags);
	wake_up(&fb_state_wq);
}
EXPORT_SYMBOL_GPL(android_start_drawing);

static ssize_t wait_for_fb_sleep_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char *s = buf;
	int ret;

	ret = wait_event_interruptible(fb_state_wq,
				       fb_state != FB_STATE_DRAWING_OK);
	if (ret && fb_state == FB_STATE_DRAWING_OK)
		return ret;
	else
		s += sprintf(buf, "sleeping");
	return s - buf;
}
DEVICE_ATTR(wait_for_fb_sleep, S_IRUGO, wait_for_fb_sleep_show, NULL);

static ssize_t wait_for_fb_wake_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char *s = buf;
	int ret;
	unsigned long irq_flags;

	spin_lock_irqsave(&fb_state_lock, irq_flags);
	if (fb_state == FB_STATE_REQUEST_STOP_DRAWING) {
		fb_state = FB_STATE_STOPPED_DRAWING;
		wake_up(&fb_state_wq);
	}
	spin_unlock_irqrestore(&fb_state_lock, irq_flags);

	ret = wait_event_interruptible(fb_state_wq,
				       fb_state == FB_STATE_DRAWING_OK);
	if (ret && fb_state != FB_STATE_DRAWING_OK)
		return ret;
	else
		s += sprintf(buf, "awake");

	return s - buf;
}
DEVICE_ATTR(wait_for_fb_awake, S_IRUGO, wait_for_fb_wake_show, NULL);

static struct attribute *pxa_android_power_attrs[] = {
	&dev_attr_wait_for_fb_sleep.attr,
	&dev_attr_wait_for_fb_awake.attr,
	NULL
};

struct attribute_group pxa_android_power_sysfs_files = {
	.attrs	= pxa_android_power_attrs,
};

static int __init android_power_init(void)
{
	init_waitqueue_head(&fb_state_wq);
	fb_state = FB_STATE_DRAWING_OK;

	return 0;
}

static void  __exit android_power_exit(void)
{
}

module_init(android_power_init);
module_exit(android_power_exit);

