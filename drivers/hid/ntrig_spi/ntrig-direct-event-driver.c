/*
 *
 *  N-Trig direct event character driver
 *
 *  Copyright (c) 2011 N-TRIG
 *
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#include "typedef-ntrig.h"
#include "ntrig-dispatcher.h"
#include "ntrig-common.h"

#define DEVICE_NAME_DIRECT_EVENTS "ntrig_direct_events"
#define NTRIG_DEV_NAME_DIRECT_EVENTS "ntrig_de"
/* File operation modes */
#define BLOCKING_MODE 0x01
#define POLLING_MODE 0x02
#define FILE_MODE(filep) \
	(((filep)->f_flags & O_NONBLOCK) ? POLLING_MODE : BLOCKING_MODE)

static struct cdev g_cdev;
static dev_t g_devno;
static struct class *g_class;
static struct mr_message_types_s g_data;		/* data buffer */

static int my_open(struct inode *inode, struct file *filep);
static int my_flush(struct file *filep, fl_owner_t id);
static ssize_t my_read(struct file *filep, char *buff, size_t count,
	loff_t *offp);
static unsigned int my_poll(struct file *filep,
	struct poll_table_struct *poll_table);
static void memory_lock(unsigned long *flag);
static void memory_unlock(unsigned long *flag);
static void wait_for_data(int file_mode);
static void notify_data_ready(void);
static int is_data_ready(void);
static void reset_data_ready(void);
static void init_synch_data(void);

/******************************************************************************/

static const struct file_operations g_fops = {
	.owner	= THIS_MODULE,
	.open	= my_open,
	.flush	= my_flush,
	.read	= my_read,
	.poll	= my_poll,
};

/******************************************************************************/
/* Flag to indicate data readiness */
static int g_data_ready;

/* Number of open handles on the character device */
static int g_num_open_files;

/* Working queues to block reading/polling operations, when data not ready */
static wait_queue_head_t g_buffer_ready;
static wait_queue_head_t g_poll_buffer_ready;

/* Threads protection on data */
static spinlock_t g_mem_lock;

static void memory_lock(unsigned long *flag)
{
	spin_lock_irqsave(&g_mem_lock, *flag);
}

static void memory_unlock(unsigned long *flag)
{
	spin_unlock_irqrestore(&g_mem_lock, *flag);
}

static int on_message(void *buf)
{
	unsigned long flag;
	int num_open_files;

	/* Ignore message if new character device is not in use */
	memory_lock(&flag);
	num_open_files = g_num_open_files;
	memory_unlock(&flag);
	if (num_open_files == 0)
		return 0;

	ntrig_dbg("ntrig-direct-event-driver inside %s\n", __func__);

	/* Copy data to temp buf */
	memory_lock(&flag);
	memcpy(&g_data, buf, sizeof(g_data));
	/* check if unblocking required */
	notify_data_ready();
	memory_unlock(&flag);
	return 0;
}

static void wait_for_data(int file_mode)
{
	if (file_mode == BLOCKING_MODE)
		interruptible_sleep_on(&g_buffer_ready);
}

static void notify_data_ready(void)
{
	ntrig_dbg("notify_data_ready\n");
	g_data_ready = 1;
	/* wake up queue for reading */
	wake_up_interruptible(&g_buffer_ready);
	/* wake up queue for polling */
	wake_up_interruptible(&g_poll_buffer_ready);
}

static int is_data_ready(void)
{
	return g_data_ready;
}

static void reset_data_ready(void)
{
	g_data_ready = 0;
}

static int my_open(struct inode *inode, struct file *filep)
{
	unsigned long flag;
	int num_open_files;
	int accept;
	memory_lock(&flag);
	accept = (g_num_open_files == 0);
	if (accept) {
		g_num_open_files++;
		num_open_files = g_num_open_files;
	}
	memory_unlock(&flag);
	if (!accept) {
		ntrig_err(
			"ntrig-direct-event-driver inside %s: concurrent "
			"sessions unsupported - rejecting\n", __func__);
		return -EBUSY;
	}
	ntrig_dbg(
		"ntrig-direct-event-driver inside %s, file mode=%d, "
		"num_open_files=%d\n", __func__, FILE_MODE(filep),
		num_open_files);
	return DTRG_NO_ERROR;
}

static int my_flush(struct file *filep, fl_owner_t id)
{
	unsigned long flag;
	ntrig_dbg("ntrig-direct-event-driver inside %s\n", __func__);
	memory_lock(&flag);
	g_num_open_files--;
	if (g_num_open_files == 0)	/* Wake up a blocked read */
			notify_data_ready();
	memory_unlock(&flag);
	return DTRG_NO_ERROR;
}

static ssize_t my_read(struct file *filep, char *buff, size_t count,
	loff_t *offp)
{
	int ret_count = 0;
	unsigned long flag;
	struct mr_message_types_s data;
	int ready;
	int interrupted = 0;

	ntrig_dbg("ntrig-direct-event-driver inside %s\n", __func__);
	/* Verify user buffer size */
	if (count < sizeof(struct mr_message_types_s))	{
		ntrig_err("ntrig-direct-event-driver inside %s: output buffer "
			"too small\n", __func__);
		return -EINVAL;
	}
	/* In blocking mode, wait until data is available. In polling mode, do
	 * not wait; return data only if available.
	 * If the file is closed while waiting for data to become available, the
	 * 'flush' callback will wake the 'read', which should return -EINTR
	 * (interrupted).
	 */
	wait_for_data(FILE_MODE(filep));
	memory_lock(&flag);
	if (g_num_open_files > 0) {
		ready = is_data_ready();
		if (ready)
			memcpy(&data, &g_data, sizeof(data));
	} else { /* aborted by flush callback */
		interrupted = 1;
		reset_data_ready();
	}
	memory_unlock(&flag);
	if (interrupted) {
		ntrig_dbg(
			"ntrig-direct-event-driver inside %s: read "
			"interrupted by release callback (file was closed)\n",
			__func__);
		return -EINTR;
	}
	if (ready) {
		if (copy_to_user(buff, &data, sizeof(data)) != 0) {
			ntrig_err(
				"ntrig-direct-event-driver: kernel -> "
				"userspace copy failed!\n");
			ret_count = -EFAULT;
		} else
			ret_count = sizeof(data);
		memory_lock(&flag);
		reset_data_ready();
		memory_unlock(&flag);
	}
	return ret_count;
}

static unsigned int my_poll(struct file *filep,
	struct poll_table_struct *poll_table)
{
	unsigned int mask = 0;
	unsigned int data_ready = 0;
	unsigned long flag;
	memory_lock(&flag);
	data_ready = is_data_ready();
	memory_unlock(&flag);
	if (data_ready)
		mask |= POLLIN | POLLRDNORM;
	poll_wait(filep, &g_poll_buffer_ready, poll_table);
	return mask;
}

static void init_synch_data(void)
{
	reset_data_ready();

	/* schedule syncronization */
	init_waitqueue_head(&g_buffer_ready);
	init_waitqueue_head(&g_poll_buffer_ready);

	/* resources protection */
	spin_lock_init(&g_mem_lock);
}

int bus_init_direct_events(void)
{
	int res;

	ntrig_dbg_lvl(NTRIG_DEBUG_LEVEL_ONCE,
		"ntrig-direct-event-driver inside %s\n", __func__);

	if (setup_direct_events(on_message)) {
		ntrig_err(
			"ntrig-direct-event-driver inside %s cannot start "
			"dispatcher\n", __func__);
		setup_direct_events(NULL);
		return DTRG_FAILED;
	}
	res = alloc_chrdev_region(&g_devno, 0, 1, DEVICE_NAME_DIRECT_EVENTS);
	if (res < 0) {
		printk(KERN_ERR
			"%s: Failed to allocate chrdev region, error %d",
			__func__, res);
		return res;
	}

	cdev_init(&g_cdev, &g_fops);
	g_cdev.owner = THIS_MODULE;
	res = cdev_add(&g_cdev, g_devno, 1);
	if (res < 0) {
		printk(KERN_ERR
			"%s: Failed to add direct-event device, error %d",
			__func__, res);
		goto unregister_region;
	}

	/* Prepare synchronization variables */
	init_synch_data();
	g_num_open_files = 0;

	/* Create /dev/ntrig_de node */
	g_class = class_create(THIS_MODULE, DEVICE_NAME_DIRECT_EVENTS);
	if (!g_class) {
		printk(KERN_ERR "%s: Could not create driver class\n",
			__func__);
		goto remove_device;
	}
	device_create(g_class, NULL, g_devno, "%s",
		NTRIG_DEV_NAME_DIRECT_EVENTS);

	return DTRG_NO_ERROR;

remove_device:
	cdev_del(&g_cdev);
unregister_region:
	unregister_chrdev_region(g_devno, 1);
	return res;
}
EXPORT_SYMBOL_GPL(bus_init_direct_events);

void bus_exit_direct_events(void)
{
	ntrig_dbg_lvl(NTRIG_DEBUG_LEVEL_ONCE,
		"ntrig-direct-event-driver inside %s\n", __func__);
	device_destroy(g_class, g_devno);
	class_destroy(g_class);
	cdev_del(&g_cdev);
	unregister_chrdev_region(g_devno, 1);
}
EXPORT_SYMBOL_GPL(bus_exit_direct_events);

