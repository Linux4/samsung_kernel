/*
 * Copyright (C) 2016 Samsung Electronics, Inc.
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

#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/rtc.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include "tzdev.h"
#include "tzprofiler.h"
#include "tz_cdev.h"
#include "tz_iwcbuf.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eugene Mandrenko <i.mandrenko@samsung.com>");

enum {
	TZPROFILER_LIST1_NEED_CLEAN = 0,
	TZPROFILER_LIST2_NEED_CLEAN
};

static bool tzprofiler_pool_registered = false;
LIST_HEAD(profiler_buf_list1);
LIST_HEAD(profiler_buf_list2);
static int current_full_pool;

static atomic_t tzprofiler_use_flag = ATOMIC_INIT(0);
static atomic_t sk_is_active_count = ATOMIC_INIT(0);
static atomic_t tzprofiler_data_is_being_written = ATOMIC_INIT(0);
static atomic_t tzprofiler_last_passing = ATOMIC_INIT(0);

static DECLARE_WAIT_QUEUE_HEAD(sk_wait);
static DECLARE_WAIT_QUEUE_HEAD(tzprofiler_data_writing);
static DEFINE_MUTEX(tzprofiler_mutex);

void tzprofiler_wait_for_bufs(void)
{
	int ret;

	if (atomic_read(&tzprofiler_data_is_being_written) == 0)
		return;

	ret = wait_event_interruptible(tzprofiler_data_writing,
			(atomic_read(&tzprofiler_data_is_being_written) == 0) || (atomic_read(&tzprofiler_last_passing) == 0));
	if (ret < 0) {
		tzdev_print(0, "%s:wait_event_interruptible_timeout tzprofiler_data_writing\n", __func__);
		atomic_set(&tzprofiler_data_is_being_written, 0);
	}
}

void tzprofiler_enter_sw(void)
{
	atomic_inc(&sk_is_active_count);
	atomic_set(&tzprofiler_last_passing, 2);
	wake_up(&sk_wait);
}

void tzprofiler_exit_sw(void)
{
	atomic_set(&tzprofiler_last_passing, 2);
	wake_up(&sk_wait);
	atomic_dec(&sk_is_active_count);
}

static int tzprofiler_init_buf_pool(unsigned int bufs_cnt, unsigned int buf_pg_cnt)
{
	struct profiler_buf_entry *profiler_buf;
	unsigned int i;

	for (i = 0; i < bufs_cnt; ++i) {
		profiler_buf = kmalloc(sizeof(struct profiler_buf_entry), GFP_KERNEL);
		if (profiler_buf == NULL)
			return -ENOMEM;
		profiler_buf->tzio_buf = tzio_alloc_iw_channel(TZDEV_CONNECT_PROFILER, buf_pg_cnt);
		if (profiler_buf->tzio_buf == NULL) {
			kfree(profiler_buf);
			return 0;
		}
		list_add(&profiler_buf->list, &profiler_buf_list1);
	}

	tzprofiler_pool_registered = true;

	return 0;
}

int tzprofiler_initialize(void)
{
	return tzprofiler_init_buf_pool(CONFIG_TZPROFILER_BUFS_CNT, CONFIG_TZPROFILER_BUF_PG_CNT);
}

static ssize_t tzprofiler_write_buf(unsigned char* s_buf, ssize_t quantity,
		unsigned char* d_buf, ssize_t *saved_count, size_t count)
{
	ssize_t real_saved;

	if ((*saved_count + quantity) <= count)
		real_saved = quantity;
	else
		real_saved = count - *saved_count;

	if (copy_to_user(&d_buf[*saved_count], s_buf, real_saved))
		tzdev_print(0, "%s:can't copy to user\n", __func__);

	*saved_count += real_saved;

	return real_saved;
}

static ssize_t __read(char __user *buf, size_t count, struct list_head *head,
		struct list_head *cleaned_head)
{
	struct tzio_iw_channel *tzio_buf;
	struct profiler_buf_entry *profiler_buf, *tmp;
	ssize_t bytes, quantity, write_count, saved_count = 0;

	list_for_each_entry_safe(profiler_buf, tmp, head, list) {
		tzio_buf = profiler_buf->tzio_buf;
		if (tzio_buf->read_count == tzio_buf->write_count) {
			list_del(&profiler_buf->list);
			list_add(&profiler_buf->list, cleaned_head);
			continue;
		}

		write_count = tzio_buf->write_count;
		if (write_count < tzio_buf->read_count) {
			quantity = TZDEV_PROFILER_BUF_SIZE - tzio_buf->read_count;
			bytes = tzprofiler_write_buf(tzio_buf->buffer + tzio_buf->read_count,
				quantity, buf, &saved_count, count);
			if (bytes < quantity) {
				tzio_buf->read_count += bytes;
				atomic_set(&tzprofiler_data_is_being_written, 1);
				return saved_count;
			}
			tzio_buf->read_count = 0;
		}
		quantity = write_count - tzio_buf->read_count;
		bytes = tzprofiler_write_buf(tzio_buf->buffer + tzio_buf->read_count, quantity,
						buf, &saved_count, count);
		if (bytes < quantity) {
			tzio_buf->read_count += bytes;
			atomic_set(&tzprofiler_data_is_being_written, 1);
			return saved_count;
		}
		tzio_buf->read_count += quantity;

		list_del(&profiler_buf->list);
		list_add(&profiler_buf->list, cleaned_head);
	}
	return saved_count;
}

static ssize_t tzprofiler_read(struct file *filp, char __user *buf, size_t count,
		loff_t *f_pos)
{
	struct list_head *current_head, *cleaned_head;
	size_t saved_count;

	atomic_set(&tzprofiler_data_is_being_written, 0);

	while(1) {
		if ((atomic_read(&sk_is_active_count) == 0) && (atomic_read(&tzprofiler_last_passing) == 0)) {
			int ret;

			ret = wait_event_interruptible_timeout(sk_wait,
					atomic_read(&sk_is_active_count) != 0, msecs_to_jiffies(5000));
			if (ret < 0) {
				atomic_set(&tzprofiler_use_flag, 0);
				return (ssize_t)-EINTR;
			}
		}

		if (tzprofiler_pool_registered == false)
			continue;

		if (current_full_pool == TZPROFILER_LIST1_NEED_CLEAN) {
			current_head = &profiler_buf_list1;
			cleaned_head = &profiler_buf_list2;
		} else {
			current_head = &profiler_buf_list2;
			cleaned_head = &profiler_buf_list1;
		}
		saved_count = __read(buf, count, current_head, cleaned_head);
		if (saved_count) {
			atomic_set(&tzprofiler_data_is_being_written, 1);
			return saved_count;
		}

		wake_up(&tzprofiler_data_writing);
		if (atomic_read(&tzprofiler_last_passing) > 0)
			atomic_dec(&tzprofiler_last_passing);

		if (current_full_pool == TZPROFILER_LIST1_NEED_CLEAN)
			current_full_pool = TZPROFILER_LIST2_NEED_CLEAN;
		else
			current_full_pool = TZPROFILER_LIST1_NEED_CLEAN;
	}

	return 0;
}

static int tzprofiler_open(struct inode *inode, struct file *filp)
{
	if (atomic_cmpxchg(&tzprofiler_use_flag, 0, 1))
		return -EBUSY;

	return 0;
}

static int tzprofiler_release(struct inode *inode, struct file *filp)
{
	atomic_set(&tzprofiler_use_flag, 0);
	atomic_set(&tzprofiler_data_is_being_written, 0);
	wake_up(&tzprofiler_data_writing);

	return 0;
}

static const struct file_operations tzprofiler_fops = {
	.owner = THIS_MODULE,
	.read = tzprofiler_read,
	.open = tzprofiler_open,
	.release = tzprofiler_release,
};

static struct tz_cdev tzprofiler_cdev = {
	.name =TZPROFILER_NAME,
	.fops = &tzprofiler_fops,
	.owner = THIS_MODULE,
};

static int __init tzprofiler_init(void)
{
	int rc;

	rc = tz_cdev_register(&tzprofiler_cdev);
	if (rc)
		return rc;

	current_full_pool = TZPROFILER_LIST1_NEED_CLEAN;

	init_waitqueue_head(&sk_wait);
	init_waitqueue_head(&tzprofiler_data_writing);
	return 0;
}

static void __exit tzprofiler_exit(void)
{
	tz_cdev_unregister(&tzprofiler_cdev);
}

module_init(tzprofiler_init);
module_exit(tzprofiler_exit);
