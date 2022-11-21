/*
 * DSPG DBMD2 codec driver character device interface
 *
 * Copyright (C) 2014 DSP Group
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

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/kfifo.h>
#include <linux/delay.h>

#include "dbmd2-interface.h"

static struct dbmd2_private *dbmd2_p;

/* Access to the audio buffer is controlled through "audio_owner". Either the
 * character device or the ALSA-capture device can be opened. */
static int dbmd2_record_open(struct inode *inode, struct file *file)
{
	file->private_data = dbmd2_p;

	if (!atomic_add_unless(&dbmd2_p->audio_owner, 1, 1))
		return -EBUSY;

	return 0;
}

static int dbmd2_record_release(struct inode *inode, struct file *file)
{
	dbmd2_p->lock(dbmd2_p);
	dbmd2_p->va_flags.buffering = 0;
	dbmd2_p->unlock(dbmd2_p);

	flush_work(&dbmd2_p->sv_work);

	atomic_dec(&dbmd2_p->audio_owner);

	return 0;
}

/* The write function is a hack to load the A-model on systems where the
 * firmware files are not accesible to the user. */
static ssize_t dbmd2_record_write(struct file *file, const char __user *buf,
				  size_t count_want, loff_t *f_pos)
{
	return count_want;
}

/*
 * read out of the kfifo as much as was requested or if requested more
 * as much as is in the FIFO
 */
static ssize_t dbmd2_record_read(struct file *file, char __user *buf,
				 size_t count_want, loff_t *f_pos)
{
	struct dbmd2_private *p = (struct dbmd2_private *)file->private_data;
	size_t not_copied;
	ssize_t to_copy = count_want;
	int avail;
	unsigned int copied;
	int ret;

	avail = kfifo_len(&p->pcm_kfifo);

	if (avail == 0)
		return 0;

	if (count_want > avail)
		to_copy = avail;

	ret = kfifo_to_user(&p->pcm_kfifo, buf, to_copy, &copied);
	if (ret)
		return -EIO;

	not_copied = count_want - copied;
	*f_pos = *f_pos + (count_want - not_copied);

	return count_want - not_copied;
}

static const struct file_operations dbmd2_cdev_fops = {
	.owner   = THIS_MODULE,
	.open    = dbmd2_record_open,
	.release = dbmd2_record_release,
	.read    = dbmd2_record_read,
	.write   = dbmd2_record_write,
};

/*
 * read out of the kfifo as much as was requested and block until all
 * data is available or a timeout occurs
 */
static ssize_t dbmd2_record_read_blocking(struct file *file, char __user *buf,
					  size_t count_want, loff_t *f_pos)
{
	struct dbmd2_private *p = (struct dbmd2_private *)file->private_data;

	size_t not_copied;
	ssize_t to_copy = count_want;
	int avail;
	unsigned int copied, total_copied = 0;
	int ret;
	unsigned long timeout = jiffies + msecs_to_jiffies(500);


	avail = kfifo_len(&p->pcm_kfifo);


	while ((total_copied < count_want) && time_before(jiffies, timeout)
		&& avail) {
		to_copy = avail;
		if (count_want - total_copied < avail)
			to_copy = count_want - total_copied;

		ret = kfifo_to_user(&p->pcm_kfifo, buf + total_copied,
							to_copy, &copied);
		if (ret)
			return -EIO;

		total_copied += copied;

		avail = kfifo_len(&p->pcm_kfifo);
		if (avail == 0 && p->va_flags.buffering)
			usleep_range(100000, 110000);
	}

	if (avail && (total_copied < count_want))
		dev_err(p->dev, "dbmd2: timeout during reading\n");

	not_copied = count_want - total_copied;
	*f_pos = *f_pos + (count_want - not_copied);

	return count_want - not_copied;

}

static const struct file_operations dbmd2_cdev_block_fops = {
	.owner   = THIS_MODULE,
	.open    = dbmd2_record_open,
	.release = dbmd2_record_release,
	.read    = dbmd2_record_read_blocking,
	.write   = dbmd2_record_write,
};

int dbmd2_register_cdev(struct dbmd2_private *p)
{
	int ret;

	dbmd2_p = p;

	ret = alloc_chrdev_region(&p->record_chrdev, 0, 2, "dbmd2");
	if (ret) {
		dev_err(p->dbmd2_dev, "failed to allocate character device\n");
		return -EINVAL;
	}

	cdev_init(&p->record_cdev, &dbmd2_cdev_fops);
	cdev_init(&p->record_cdev_block, &dbmd2_cdev_block_fops);

	p->record_cdev.owner = THIS_MODULE;
	p->record_cdev_block.owner = THIS_MODULE;

	ret = cdev_add(&p->record_cdev, p->record_chrdev, 1);
	if (ret) {
		dev_err(p->dbmd2_dev, "failed to add character device\n");
		unregister_chrdev_region(p->record_chrdev, 1);
		return -EINVAL;
	}

	ret = cdev_add(&p->record_cdev_block, p->record_chrdev, 1);
	if (ret) {
		dev_err(p->dbmd2_dev,
			"failed to add blocking character device\n");
		unregister_chrdev_region(p->record_chrdev, 1);
		return -EINVAL;
	}

	p->record_dev = device_create(p->ns_class, &platform_bus,
				      MKDEV(MAJOR(p->record_chrdev), 0),
				      p, "dbmd2-%d", 0);
	if (IS_ERR(p->record_dev)) {
		dev_err(p->dev, "could not create device\n");
		unregister_chrdev_region(p->record_chrdev, 1);
		cdev_del(&p->record_cdev);
		return -EINVAL;
	}

	p->record_dev_block = device_create(p->ns_class, &platform_bus,
					    MKDEV(MAJOR(p->record_chrdev), 1),
					    p, "dbmd2-%d", 1);
	if (IS_ERR(p->record_dev_block)) {
		dev_err(p->dev, "could not create device\n");
		unregister_chrdev_region(p->record_chrdev, 1);
		cdev_del(&p->record_cdev_block);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(dbmd2_register_cdev);

void dbmd2_deregister_cdev(struct dbmd2_private *p)
{
	if (p->record_dev) {
		device_unregister(p->record_dev);
		cdev_del(&p->record_cdev);
	}
	if (p->record_dev_block) {
		device_unregister(p->record_dev_block);
		cdev_del(&p->record_cdev_block);
	}
	unregister_chrdev_region(p->record_chrdev, 2);

	dbmd2_p = NULL;
}
EXPORT_SYMBOL(dbmd2_deregister_cdev);
