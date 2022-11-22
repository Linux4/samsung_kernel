/*
 *	w1_ds278x.c - w1 family 27 (DS2780/DS2781) driver
 *
 * Copyright (c) Marvell 2007 Paul Shen <bo.a.shen@marvell.com>
 * Copyright (c) Intel 2006 Stanley Cai <stanley.cai@intel.com>
 *
 * Modified from w1_ds2433 driver
 *
 * Copyright (c) 2005 Ben Gardner <bgardner@wabtec.com>
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2. See the file COPYING for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>

#include "../w1.h"
#include "../w1_int.h"
#include "../w1_family.h"

#define W1_REG_SIZE		256

#define W1_F27_READ_REG		0x69
#define W1_F27_WRITE_REG	0x6C
#define W1_F27_COPY_REG		0x48
#define W1_F27_RECALL_REG	0xB8
#define W1_F27_LOCK_REG		0x6A

struct w1_f27_data {
	u8 memory[W1_REG_SIZE];
	u32 validcrc;
};

/**
 * Check the file size bounds and adjusts count as needed.
 * This would not be needed if the file size didn't reset to 0 after a write.
 */
static inline size_t w1_f27_fix_count(loff_t off, size_t count,
				      size_t size)
{
	if (off > size)
		return 0;

	if ((off + count) > size)
		return size - off;

	return count;
}

static ssize_t w1_f27_read_bin(struct file *fp, struct kobject *kobj,
			       struct bin_attribute *bin_attr,
			       char *buf, loff_t off, size_t count)
{
	struct w1_slave *sl = kobj_to_w1_slave(kobj);
	u8 wrbuf[2];

	count = w1_f27_fix_count(off, count, W1_REG_SIZE);
	if (count == 0)
		return 0;

	atomic_inc(&sl->refcnt);
	mutex_lock(&sl->master->mutex);

	/* read directly from the REG */
	if (w1_reset_select_slave(sl)) {
		count = -EIO;
		goto out_mutex_unlock;
	}

	wrbuf[0] = W1_F27_READ_REG;
	wrbuf[1] = off & 0xff;
	w1_write_block(sl->master, wrbuf, 2);
	w1_read_block(sl->master, buf, count);

out_mutex_unlock:
	mutex_unlock(&sl->master->mutex);
	atomic_dec(&sl->refcnt);

	return count;
}

static ssize_t w1_f27_write_bin(struct file *fp, struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	struct w1_slave *sl = kobj_to_w1_slave(kobj);
	char wrbuf[2];

	count = w1_f27_fix_count(off, count, W1_REG_SIZE);
	if (count == 0)
		return 0;

	atomic_inc(&sl->refcnt);
	mutex_lock(&sl->master->mutex);

	if (w1_reset_select_slave(sl)) {
		count = -EIO;
		goto out_mutex_unlock;
	}

	wrbuf[0] = W1_F27_WRITE_REG;
	wrbuf[1] = off & 0xff;
	w1_write_block(sl->master, wrbuf, 2);
	w1_write_block(sl->master, buf, count);

out_mutex_unlock:
	mutex_unlock(&sl->master->mutex);
	atomic_dec(&sl->refcnt);

	return count;
}

static struct bin_attribute w1_f27_bin_attr = {
	.attr = {
		 .name = "registers",
		 .mode = S_IRUGO | S_IWUSR,
		 },
	.size = W1_REG_SIZE,
	.read = w1_f27_read_bin,
	.write = w1_f27_write_bin,
};

static int w1_f27_add_slave(struct w1_slave *sl)
{
	int err;

	err = sysfs_create_bin_file(&sl->dev.kobj, &w1_f27_bin_attr);

	return err;
}

static void w1_f27_remove_slave(struct w1_slave *sl)
{
	sysfs_remove_bin_file(&sl->dev.kobj, &w1_f27_bin_attr);
}

static struct w1_family_ops w1_f27_fops = {
	.add_slave = w1_f27_add_slave,
	.remove_slave = w1_f27_remove_slave,
};

static struct w1_family w1_family_2781 = {
	.fid = W1_FAMILY_DS2781,
	.fops = &w1_f27_fops,
};

static struct w1_family w1_family_2783 = {
	.fid = W1_FAMILY_DS2783,
	.fops = &w1_f27_fops,
};


static int __init w1_f27_init(void)
{
	int ret;

	printk(KERN_INFO
	       "1-Wire driver for the DS278x battery monitor...\n");
	ret = w1_register_family(&w1_family_2781);
	if (ret < 0)
		goto out;
	ret = w1_register_family(&w1_family_2783);
	if (ret < 0)
		goto out;
out:
	return ret;
}

static void __exit w1_f27_fini(void)
{
	w1_unregister_family(&w1_family_2781);
	w1_unregister_family(&w1_family_2783);
}

module_init(w1_f27_init);
module_exit(w1_f27_fini);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Paul Shen <bo.a.shen@marvell.com>");
MODULE_DESCRIPTION("w1 family 27 driver for DS2780 & DS2781,"
		   " Stand-Alone Fuel Gauge IC");
