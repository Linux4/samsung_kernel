/*
	$License:
	Copyright (C) 2011 InvenSense Corporation, All Rights Reserved.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
	$
 */
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/signal.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/pm.h>
#include <linux/mutex.h>
#include <linux/suspend.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/sensors_core.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/hrtimer.h>

#include "mpuirq.h"
#include "slaveirq.h"
#include "mlsl.h"
#include "mldl_cfg.h"
#include <linux/mpu_411.h>

#include "accel/mpu6050.h"
#include "mpu-dev.h"

#ifdef CONFIG_INPUT_YAS_MAGNETOMETER
#include "compass/yas530_ext.h"
#endif
#ifdef CONFIG_SENSORS_AK8975
#include "compass/ak89753.h"
#endif
#include <linux/akm8975.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define MAG_VENDOR	"AKM"
#define MAG_PART_ID	"AK8963C"
#define MPU_VENDOR	"INVENSENSE"
#define MPU_PART_ID	"MPU-6050"

#define MPU_EARLY_SUSPEND_IN_DRIVER 1

#define CALIBRATION_FILE_PATH	"/efs/calibration_data"
#define CALIBRATION_GYRO_FILE_PATH	"/efs/gyro_cal_data"
#define CALIBRATION_DATA_AMOUNT	100
#include "mpu6050_selftest.h"

struct acc_data cal_data = {0, 0, 0};

/* Platform data for the MPU */
struct mpu_private_data {
	struct miscdevice dev;
	struct i2c_client *client;

	/* mldl_cfg data */
	struct mldl_cfg mldl_cfg;
	struct mpu_ram		mpu_ram;
	struct mpu_gyro_cfg	mpu_gyro_cfg;
	struct mpu_offsets	mpu_offsets;
	struct mpu_chip_info	mpu_chip_info;
	struct inv_mpu_cfg	inv_mpu_cfg;
	struct inv_mpu_state	inv_mpu_state;

	struct mutex mutex;
	wait_queue_head_t mpu_event_wait;
	struct completion completion;
	struct timer_list timeout;
	struct notifier_block nb;
	struct mpuirq_data mpu_pm_event;
	int response_timeout;	/* In seconds */
	unsigned long event;
	int pid;
	struct module *slave_modules[EXT_SLAVE_NUM_TYPES];
	struct {
		atomic_t enable;
		unsigned char is_activated;
		unsigned char turned_by_mpu_accel;
	} mpu_accel;

	struct hrtimer activate_timer;
	int activate_timeout;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	int gyro_bias[3];
};

static struct i2c_client *this_client;

#define IDEAL_X 0
#define IDEAL_Y 0
#define IDEAL_Z 1024

struct mpu_private_data *mpu_private_data;

void mpu_accel_enable_set(int enable)
{

	struct mpu_private_data *mpu =
	    (struct mpu_private_data *) i2c_get_clientdata(this_client);
	struct i2c_client *client = mpu->client;
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	struct i2c_adapter *slave_adapter[EXT_SLAVE_NUM_TYPES];
	struct ext_slave_platform_data **pdata_slave = mldl_cfg->pdata_slave;
	int ii;

	for (ii = 0; ii < EXT_SLAVE_NUM_TYPES; ii++) {
		if (!pdata_slave[ii])
			slave_adapter[ii] = NULL;
		else
			slave_adapter[ii] =
				i2c_get_adapter(pdata_slave[ii]->adapt_num);
	}
	slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE] = client->adapter;

	if (enable) {
		mpu->mpu_accel.is_activated = !(mldl_cfg->inv_mpu_state->status
			& MPU_ACCEL_IS_SUSPENDED);

			(void)inv_mpu_resume(mldl_cfg,
					slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
					slave_adapter[EXT_SLAVE_TYPE_ACCEL],
					slave_adapter[EXT_SLAVE_TYPE_COMPASS],
					slave_adapter[EXT_SLAVE_TYPE_PRESSURE],
					INV_THREE_AXIS_ACCEL);

			mpu->mpu_accel.turned_by_mpu_accel
				= 1;
	} else {
		if (!mpu->mpu_accel.is_activated &&
			mpu->mpu_accel.turned_by_mpu_accel) {
				(void)inv_mpu_suspend(mldl_cfg,
				slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
				slave_adapter[EXT_SLAVE_TYPE_ACCEL],
				slave_adapter[EXT_SLAVE_TYPE_COMPASS],
				slave_adapter[EXT_SLAVE_TYPE_PRESSURE],
					INV_THREE_AXIS_ACCEL);
				mpu->mpu_accel.turned_by_mpu_accel = 0;
				}
		}
	atomic_set(&mpu->mpu_accel.enable, enable);
}

int read_accel_raw_xyz(struct acc_data *acc)
{
	s16 x, y, z;
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *) i2c_get_clientdata(this_client);
	struct i2c_client *client = mpu->client;
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	unsigned char data[6];

	struct i2c_adapter *slave_adapter[EXT_SLAVE_NUM_TYPES];
	struct ext_slave_platform_data **pdata_slave = mldl_cfg->pdata_slave;
	int cal_div = (mldl_cfg->mpu_chip_info->accel_sens_trim) / 1024;
	int ii;

	for (ii = 0; ii < EXT_SLAVE_NUM_TYPES; ii++) {
		if (!pdata_slave[ii])
			slave_adapter[ii] = NULL;
		else
			slave_adapter[ii] =
				i2c_get_adapter(pdata_slave[ii]->adapt_num);
	}
	slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE] = client->adapter;


	inv_serial_read(slave_adapter[EXT_SLAVE_TYPE_ACCEL],
		0x68, 0x3B, 6, data);

	x = (s16)((data[0] << 8) | data[1]) / cal_div;
	y = (s16)((data[2] << 8) | data[3]) / cal_div;
	z = (s16)((data[4] << 8) | data[5]) / cal_div;

	acc->x = x;
	acc->y = y;
	acc->z = z;

	return 0;
}

static int accel_open_calibration(void)
{
	struct file *cal_filp = NULL;
	int err = 0;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		return err;
	}

	err = cal_filp->f_op->read(cal_filp,
		(char *)&cal_data, 3 * sizeof(s16), &cal_filp->f_pos);
	if (err != 3 * sizeof(s16)) {
		pr_err("%s: Can't read the cal data from file\n", __func__);
		err = -EIO;
	}

	printk(KERN_INFO"%s: (%u,%u,%u)\n", __func__,
		cal_data.x, cal_data.y, cal_data.z);

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return err;
}

static int accel_do_calibrate(int enable)
{
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *) i2c_get_clientdata(this_client);
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	struct acc_data data = { 0, };
	struct file *cal_filp = NULL;

	int sum[3] = { 0, };
	int err = 0;
	int i;
	mm_segment_t old_fs;

	int cal_div = (mldl_cfg->mpu_chip_info->accel_sens_trim) / 1024;
	int ideal_z = IDEAL_Z * mldl_cfg->pdata->orientation[8];

	/*mutex_lock(&mpu->mutex);*/
	mpu_accel_enable_set(1);
	mdelay(1);

	for (i = 0; i < CALIBRATION_DATA_AMOUNT; i++) {
		err = read_accel_raw_xyz(&data);
		if (err < 0) {
			pr_err("%s: accel_read_accel_raw_xyz() "
				"failed in the %dth loop\n", __func__, i);
			return err;
		}

		sum[0] += data.x;
		sum[1] += data.y;
		sum[2] += data.z;
	}

	mpu_accel_enable_set(0);
	mdelay(1);
	/*mutex_unlock(&mpu->mutex);*/

	if (enable) {
		cal_data.x = ((sum[0] / CALIBRATION_DATA_AMOUNT) - IDEAL_X)
			* cal_div;
		cal_data.y = ((sum[1] / CALIBRATION_DATA_AMOUNT) - IDEAL_Y)
			* cal_div;
		cal_data.z = ((sum[2] / CALIBRATION_DATA_AMOUNT) - ideal_z)
			* cal_div;
	} else {
		cal_data.x = 0;
		cal_data.y = 0;
		cal_data.z = 0;
	}

	printk(KERN_INFO "%s: cal data (%d,%d,%d)\n", __func__,
			cal_data.x, cal_data.y, cal_data.z);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		return err;
	}

	err = cal_filp->f_op->write(cal_filp,
		(char *)&cal_data, 3 * sizeof(s16), &cal_filp->f_pos);
	if (err != 3 * sizeof(s16)) {
		pr_err("%s: Can't write the cal data to file\n", __func__);
		err = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return err;
}

static void mpu_pm_timeout(u_long data)
{
	struct mpu_private_data *mpu = (struct mpu_private_data *)data;
	struct i2c_client *client = mpu->client;
	dev_dbg(&client->adapter->dev, "%s\n", __func__);
	complete(&mpu->completion);
}


static int mpu_early_notifier_callback(struct mpu_private_data *mpu,
				    unsigned long event, void *unused)
{
	struct i2c_client *client = mpu->client;
	struct timeval event_time;
	dev_dbg(&client->adapter->dev, "%s: %ld\n", __func__, event);

	/* Prevent the file handle from being closed before we initialize
	   the completion event */
	printk(KERN_INFO"[%s] event = %lu\n", __func__, event);
	mutex_lock(&mpu->mutex);
	if (!(mpu->pid) ||
	    (event != PM_SUSPEND_PREPARE && event != PM_POST_SUSPEND)) {
		mutex_unlock(&mpu->mutex);
		return NOTIFY_OK;
	}

	if (event == PM_SUSPEND_PREPARE)
		mpu->event |= MPU_PM_EVENT_SUSPEND_PREPARE;
	if (event == PM_POST_SUSPEND)
		mpu->event |= MPU_PM_EVENT_POST_SUSPEND;

	do_gettimeofday(&event_time);
	mpu->mpu_pm_event.interruptcount++;
	mpu->mpu_pm_event.irqtime =
	    (((long long)event_time.tv_sec) << 32) + event_time.tv_usec;
	mpu->mpu_pm_event.data_type = MPUIRQ_DATA_TYPE_PM_EVENT;
	mpu->mpu_pm_event.data = mpu->event;

	if (mpu->response_timeout > 0) {
		mpu->timeout.expires = jiffies + mpu->response_timeout * HZ;
		add_timer(&mpu->timeout);
	}
	INIT_COMPLETION(mpu->completion);
	mutex_unlock(&mpu->mutex);

	wake_up_interruptible(&mpu->mpu_event_wait);
	wait_for_completion(&mpu->completion);
	del_timer_sync(&mpu->timeout);
	dev_dbg(&client->adapter->dev, "%s: %ld DONE\n", __func__, event);
	return NOTIFY_OK;
}

static int mpu_dev_open(struct inode *inode, struct file *file)
{
	struct mpu_private_data *mpu =
	    container_of(file->private_data, struct mpu_private_data, dev);
	struct i2c_client *client = mpu->client;
	int result;
	int ii;
	dev_dbg(&client->adapter->dev, "%s\n", __func__);
	dev_dbg(&client->adapter->dev, "current->pid %d\n", current->pid);

	accel_open_calibration();

	result = mutex_lock_interruptible(&mpu->mutex);
	if (mpu->pid) {
		mutex_unlock(&mpu->mutex);
		return -EBUSY;
	}
	mpu->pid = current->pid;

	/* Reset the sensors to the default */
	if (result) {
		dev_err(&client->adapter->dev,
			"%s: mutex_lock_interruptible returned %d\n",
			__func__, result);
		return result;
	}

	for (ii = 0; ii < EXT_SLAVE_NUM_TYPES; ii++)
		__module_get(mpu->slave_modules[ii]);

	mutex_unlock(&mpu->mutex);
	return 0;
}

/* close function - called when the "file" /dev/mpu is closed in userspace   */
static int mpu_release(struct inode *inode, struct file *file)
{
	struct mpu_private_data *mpu =
	    container_of(file->private_data, struct mpu_private_data, dev);
	struct i2c_client *client = mpu->client;
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	int result = 0;
	int ii;
	struct i2c_adapter *slave_adapter[EXT_SLAVE_NUM_TYPES];
	struct ext_slave_platform_data **pdata_slave = mldl_cfg->pdata_slave;

	for (ii = 0; ii < EXT_SLAVE_NUM_TYPES; ii++) {
		if (!pdata_slave[ii])
			slave_adapter[ii] = NULL;
		else
			slave_adapter[ii] =
				i2c_get_adapter(pdata_slave[ii]->adapt_num);
	}
	slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE] = client->adapter;

	mutex_lock(&mpu->mutex);
	mldl_cfg->inv_mpu_cfg->requested_sensors = 0;
	result = inv_mpu_suspend(mldl_cfg,
				slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
				slave_adapter[EXT_SLAVE_TYPE_ACCEL],
				slave_adapter[EXT_SLAVE_TYPE_COMPASS],
				slave_adapter[EXT_SLAVE_TYPE_PRESSURE],
				INV_ALL_SENSORS);
	mpu->pid = 0;
	for (ii = 0; ii < EXT_SLAVE_NUM_TYPES; ii++)
		module_put(mpu->slave_modules[ii]);

	mutex_unlock(&mpu->mutex);
	complete(&mpu->completion);
	dev_dbg(&client->adapter->dev, "mpu_release\n");

	return result;
}

/* read function called when from /dev/mpu is read.  Read from the FIFO */
static ssize_t mpu_read(struct file *file,
			char __user *buf, size_t count, loff_t *offset)
{
	struct mpu_private_data *mpu =
	    container_of(file->private_data, struct mpu_private_data, dev);
	struct i2c_client *client = mpu->client;
	size_t len = sizeof(mpu->mpu_pm_event) + sizeof(unsigned long);
	int err;

	if (!mpu->event && (!(file->f_flags & O_NONBLOCK)))
		wait_event_interruptible(mpu->mpu_event_wait, mpu->event);

	if (!mpu->event || !buf
	    || count < sizeof(mpu->mpu_pm_event))
		return 0;

	err = copy_to_user(buf, &mpu->mpu_pm_event, sizeof(mpu->mpu_pm_event));
	if (err) {
		dev_err(&client->adapter->dev,
			"Copy to user returned %d\n", err);
		return -EFAULT;
	}
	mpu->event = 0;
	return len;
}

static unsigned int mpu_poll(struct file *file, struct poll_table_struct *poll)
{
	struct mpu_private_data *mpu =
	    container_of(file->private_data, struct mpu_private_data, dev);
	int mask = 0;

	poll_wait(file, &mpu->mpu_event_wait, poll);
	if (mpu->event)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

static int mpu_dev_ioctl_get_ext_slave_platform_data(
	struct i2c_client *client,
	struct ext_slave_platform_data __user *arg)
{
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *)i2c_get_clientdata(client);
	struct ext_slave_platform_data *pdata_slave;
	struct ext_slave_platform_data local_pdata_slave;

	if (copy_from_user(&local_pdata_slave, arg, sizeof(local_pdata_slave)))
		return -EFAULT;

	if (local_pdata_slave.type >= EXT_SLAVE_NUM_TYPES)
		return -EINVAL;

	pdata_slave = mpu->mldl_cfg.pdata_slave[local_pdata_slave.type];
	/* All but private data and irq_data */
	if (!pdata_slave)
		return -ENODEV;
	if (copy_to_user(arg, pdata_slave, sizeof(*pdata_slave)))
		return -EFAULT;
	return 0;
}

static int mpu_dev_ioctl_get_mpu_platform_data(
	struct i2c_client *client,
	struct mpu_platform_data __user *arg)
{
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *)i2c_get_clientdata(client);
	struct mpu_platform_data *pdata = mpu->mldl_cfg.pdata;

	if (copy_to_user(arg, pdata, sizeof(*pdata)))
		return -EFAULT;
	return 0;
}

static int mpu_dev_ioctl_get_ext_slave_descr(
	struct i2c_client *client,
	struct ext_slave_descr __user *arg)
{
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *)i2c_get_clientdata(client);
	struct ext_slave_descr *slave;
	struct ext_slave_descr local_slave;

	if (copy_from_user(&local_slave, arg, sizeof(local_slave)))
		return -EFAULT;

	if (local_slave.type >= EXT_SLAVE_NUM_TYPES)
		return -EINVAL;

	slave = mpu->mldl_cfg.slave[local_slave.type];
	/* All but private data and irq_data */
	if (!slave)
		return -ENODEV;
	if (copy_to_user(arg, slave, sizeof(*slave)))
		return -EFAULT;
	return 0;
}


/**
 * slave_config() - Pass a requested slave configuration to the slave sensor
 *
 * @adapter the adaptor to use to communicate with the slave
 * @mldl_cfg the mldl configuration structuer
 * @slave pointer to the slave descriptor
 * @usr_config The configuration to pass to the slave sensor
 *
 * returns 0 or non-zero error code
 */
static int inv_mpu_config(struct mldl_cfg *mldl_cfg,
			void *gyro_adapter,
			struct ext_slave_config __user *usr_config)
{
	int retval = 0;
	struct ext_slave_config config;

	retval = copy_from_user(&config, usr_config, sizeof(config));
	if (retval)
		return -EFAULT;

	if (config.len && config.data) {
		void *data;
		data = kmalloc(config.len, GFP_KERNEL);
		if (!data)
			return -ENOMEM;

		retval = copy_from_user(data,
					(void __user *)config.data, config.len);
		if (retval) {
			retval = -EFAULT;
			kfree(data);
			return retval;
		}
		config.data = data;
	}
	retval = gyro_config(gyro_adapter, mldl_cfg, &config);
	kfree(config.data);
	return retval;
}

static int inv_mpu_get_config(struct mldl_cfg *mldl_cfg,
			    void *gyro_adapter,
			    struct ext_slave_config __user *usr_config)
{
	int retval = 0;
	struct ext_slave_config config;
	void *user_data;

	retval = copy_from_user(&config, usr_config, sizeof(config));
	if (retval)
		return -EFAULT;

	user_data = config.data;
	if (config.len && config.data) {
		void *data;
		data = kmalloc(config.len, GFP_KERNEL);
		if (!data)
			return -ENOMEM;

		retval = copy_from_user(data,
					(void __user *)config.data, config.len);
		if (retval) {
			retval = -EFAULT;
			kfree(data);
			return retval;
		}
		config.data = data;
	}
	retval = gyro_get_config(gyro_adapter, mldl_cfg, &config);
	if (!retval)
		retval = copy_to_user((unsigned char __user *)user_data,
				config.data, config.len);
	kfree(config.data);
	return retval;
}

static int slave_config(struct mldl_cfg *mldl_cfg,
			void *gyro_adapter,
			void *slave_adapter,
			struct ext_slave_descr *slave,
			struct ext_slave_platform_data *pdata,
			struct ext_slave_config __user *usr_config)
{
	int retval = 0;
	struct ext_slave_config config;
	if ((!slave) || (!slave->config))
		return -ENODEV;

	retval = copy_from_user(&config, usr_config, sizeof(config));
	if (retval)
		return -EFAULT;

	if (config.len && config.data) {
		void *data;
		data = kmalloc(config.len, GFP_KERNEL);
		if (!data)
			return -ENOMEM;

		retval = copy_from_user(data,
					(void __user *)config.data, config.len);
		if (retval) {
			retval = -EFAULT;
			kfree(data);
			return retval;
		}
		config.data = data;
	}
	retval = inv_mpu_slave_config(mldl_cfg, gyro_adapter, slave_adapter,
				      &config, slave, pdata);
	kfree(config.data);
	return retval;
}

static int slave_get_config(struct mldl_cfg *mldl_cfg,
			    void *gyro_adapter,
			    void *slave_adapter,
			    struct ext_slave_descr *slave,
			    struct ext_slave_platform_data *pdata,
			    struct ext_slave_config __user *usr_config)
{
	int retval = 0;
	struct ext_slave_config config;
	void *user_data;
	if (!(slave) || !(slave->get_config))
		return -ENODEV;

	retval = copy_from_user(&config, usr_config, sizeof(config));
	if (retval)
		return -EFAULT;

	user_data = config.data;
	if (config.len && config.data) {
		void *data;
		data = kmalloc(config.len, GFP_KERNEL);
		if (!data)
			return -ENOMEM;

		retval = copy_from_user(data,
					(void __user *)config.data, config.len);
		if (retval) {
			retval = -EFAULT;
			kfree(data);
			return retval;
		}
		config.data = data;
	}
	retval = inv_mpu_get_slave_config(mldl_cfg, gyro_adapter,
					  slave_adapter, &config, slave, pdata);
	if (retval) {
		kfree(config.data);
		return retval;
	}
	retval = copy_to_user((unsigned char __user *)user_data,
			      config.data, config.len);
	kfree(config.data);
	return retval;
}

static int inv_slave_read(struct mldl_cfg *mldl_cfg,
			  void *gyro_adapter,
			  void *slave_adapter,
			  struct ext_slave_descr *slave,
			  struct ext_slave_platform_data *pdata,
			  void __user *usr_data)
{
	int retval;
	unsigned char *data;
	data = kzalloc(slave->read_len, GFP_KERNEL);
	if (!data)
		return -EFAULT;

	retval = inv_mpu_slave_read(mldl_cfg, gyro_adapter, slave_adapter,
				    slave, pdata, data);

	if ((!retval) &&
	    (copy_to_user((unsigned char __user *)usr_data,
			  data, slave->read_len)))
		retval = -EFAULT;

	kfree(data);
	return retval;
}

static int mpu_handle_mlsl(void *sl_handle,
			   unsigned char addr,
			   unsigned int cmd,
			   struct mpu_read_write __user *usr_msg)
{
	int retval = 0;
	struct mpu_read_write msg;
	unsigned char *user_data;
	retval = copy_from_user(&msg, usr_msg, sizeof(msg));
	if (retval)
		return -EFAULT;

	user_data = msg.data;
	if (msg.length && msg.data) {
		unsigned char *data;
		data = kmalloc(msg.length, GFP_KERNEL);
		if (!data)
			return -ENOMEM;

		retval = copy_from_user(data,
					(void __user *)msg.data, msg.length);
		if (retval) {
			retval = -EFAULT;
			kfree(data);
			return retval;
		}
		msg.data = data;
	} else {
		return -EPERM;
	}

	switch (cmd) {
	case MPU_READ:
		retval = inv_serial_read(sl_handle, addr,
			(unsigned char)msg.address, msg.length, msg.data);
		break;
	case MPU_WRITE:
		retval = inv_serial_write(sl_handle, addr,
					  msg.length, msg.data);
		break;
	case MPU_READ_MEM:
		retval = inv_serial_read_mem(sl_handle, addr,
					     msg.address, msg.length, msg.data);
		break;
	case MPU_WRITE_MEM:
		retval = inv_serial_write_mem(sl_handle, addr,
					      msg.address, msg.length,
					      msg.data);
		break;
	case MPU_READ_FIFO:
		retval = inv_serial_read_fifo(sl_handle, addr,
					      msg.length, msg.data);
		break;
	case MPU_WRITE_FIFO:
		retval = inv_serial_write_fifo(sl_handle, addr,
					       msg.length, msg.data);
		break;

	};
	if (retval) {
		dev_err(&((struct i2c_adapter *)sl_handle)->dev,
			"%s: i2c %d error %d\n",
			__func__, cmd, retval);
		kfree(msg.data);
		return retval;
	}
	retval = copy_to_user((unsigned char __user *)user_data,
			      msg.data, msg.length);
	kfree(msg.data);
	return retval;
}

static enum hrtimer_restart mpu_actiavte_sensors_callback(struct hrtimer *timer)
{
	struct mpu_private_data *mpu = mpu_private_data;

	struct i2c_client *client = mpu->client;
	struct timeval event_time;

	dev_dbg(&client->adapter->dev, "%s\n", __func__);

	/* Prevent the file handle from being closed before we initialize
	   the completion event */

	mpu->event |= MPU_KN_EVENT_ENABLE_SENSORS;

	do_gettimeofday(&event_time);
	mpu->mpu_pm_event.interruptcount++;
	mpu->mpu_pm_event.irqtime =
	    (((long long)event_time.tv_sec) << 32) + event_time.tv_usec;
	mpu->mpu_pm_event.data_type =  MPUIRQ_DATA_TYPE_PM_EVENT;
	mpu->mpu_pm_event.data = mpu->event;

	wake_up_interruptible(&mpu->mpu_event_wait);

	return HRTIMER_NORESTART;
}

static long mpu_dev_ioctl_activate_sensors(struct mpu_private_data *mpu)
{
	ktime_t ktime;
	struct i2c_client *client = mpu->client;

	dev_dbg(&client->adapter->dev, "%s\n", __func__);

	hrtimer_cancel(&mpu->activate_timer);

	ktime = ktime_set(mpu->activate_timeout / 1000,
			  (mpu->activate_timeout % 1000) * 1000000);
	return hrtimer_start(&mpu->activate_timer, ktime, HRTIMER_MODE_REL);
}

/* ioctl - I/O control */
static long mpu_dev_ioctl(struct file *file,
			  unsigned int cmd, unsigned long arg)
{
	struct mpu_private_data *mpu =
	    container_of(file->private_data, struct mpu_private_data, dev);
	struct i2c_client *client = mpu->client;
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	int retval = 0;
	struct i2c_adapter *slave_adapter[EXT_SLAVE_NUM_TYPES];
	struct ext_slave_descr **slave = mldl_cfg->slave;
	struct ext_slave_platform_data **pdata_slave = mldl_cfg->pdata_slave;
	int ii;

	for (ii = 0; ii < EXT_SLAVE_NUM_TYPES; ii++) {
		if (!pdata_slave[ii])
			slave_adapter[ii] = NULL;
		else
			slave_adapter[ii] =
				i2c_get_adapter(pdata_slave[ii]->adapt_num);
	}
	slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE] = client->adapter;

	retval = mutex_lock_interruptible(&mpu->mutex);
	if (retval) {
		dev_err(&client->adapter->dev,
			"%s: mutex_lock_interruptible returned %d\n",
			__func__, retval);
		return retval;
	}

	switch (cmd) {
	case MPU_GET_EXT_SLAVE_PLATFORM_DATA:
		retval = mpu_dev_ioctl_get_ext_slave_platform_data(
			client,
			(struct ext_slave_platform_data __user *)arg);
		break;
	case MPU_GET_MPU_PLATFORM_DATA:
		retval = mpu_dev_ioctl_get_mpu_platform_data(
			client,
			(struct mpu_platform_data __user *)arg);
		break;
	case MPU_GET_EXT_SLAVE_DESCR:
		retval = mpu_dev_ioctl_get_ext_slave_descr(
			client,
			(struct ext_slave_descr __user *)arg);
		break;
	case MPU_READ:
	case MPU_WRITE:
	case MPU_READ_MEM:
	case MPU_WRITE_MEM:
	case MPU_READ_FIFO:
	case MPU_WRITE_FIFO:
		retval = mpu_handle_mlsl(
			slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
			mldl_cfg->mpu_chip_info->addr, cmd,
			(struct mpu_read_write __user *)arg);
		break;
	case MPU_CONFIG_GYRO:
		retval = inv_mpu_config(
			mldl_cfg,
			slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
			(struct ext_slave_config __user *)arg);
		break;
	case MPU_CONFIG_ACCEL:
		retval = slave_config(
			mldl_cfg,
			slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
			slave_adapter[EXT_SLAVE_TYPE_ACCEL],
			slave[EXT_SLAVE_TYPE_ACCEL],
			pdata_slave[EXT_SLAVE_TYPE_ACCEL],
			(struct ext_slave_config __user *)arg);
		break;
	case MPU_CONFIG_COMPASS:
		retval = slave_config(
			mldl_cfg,
			slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
			slave_adapter[EXT_SLAVE_TYPE_COMPASS],
			slave[EXT_SLAVE_TYPE_COMPASS],
			pdata_slave[EXT_SLAVE_TYPE_COMPASS],
			(struct ext_slave_config __user *)arg);
		break;
	case MPU_CONFIG_PRESSURE:
		retval = slave_config(
			mldl_cfg,
			slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
			slave_adapter[EXT_SLAVE_TYPE_PRESSURE],
			slave[EXT_SLAVE_TYPE_PRESSURE],
			pdata_slave[EXT_SLAVE_TYPE_PRESSURE],
			(struct ext_slave_config __user *)arg);
		break;
	case MPU_GET_CONFIG_GYRO:
		retval = inv_mpu_get_config(
			mldl_cfg,
			slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
			(struct ext_slave_config __user *)arg);
		break;
	case MPU_GET_CONFIG_ACCEL:
		retval = slave_get_config(
			mldl_cfg,
			slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
			slave_adapter[EXT_SLAVE_TYPE_ACCEL],
			slave[EXT_SLAVE_TYPE_ACCEL],
			pdata_slave[EXT_SLAVE_TYPE_ACCEL],
			(struct ext_slave_config __user *)arg);
		break;
	case MPU_GET_CONFIG_COMPASS:
		retval = slave_get_config(
			mldl_cfg,
			slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
			slave_adapter[EXT_SLAVE_TYPE_COMPASS],
			slave[EXT_SLAVE_TYPE_COMPASS],
			pdata_slave[EXT_SLAVE_TYPE_COMPASS],
			(struct ext_slave_config __user *)arg);
		break;
	case MPU_GET_CONFIG_PRESSURE:
		retval = slave_get_config(
			mldl_cfg,
			slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
			slave_adapter[EXT_SLAVE_TYPE_PRESSURE],
			slave[EXT_SLAVE_TYPE_PRESSURE],
			pdata_slave[EXT_SLAVE_TYPE_PRESSURE],
			(struct ext_slave_config __user *)arg);
		break;
	case MPU_SUSPEND:
		retval = inv_mpu_suspend(
				mldl_cfg,
				slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
				slave_adapter[EXT_SLAVE_TYPE_ACCEL],
				slave_adapter[EXT_SLAVE_TYPE_COMPASS],
				slave_adapter[EXT_SLAVE_TYPE_PRESSURE],
				arg);
		break;
	case MPU_RESUME:
		retval = inv_mpu_resume(
			mldl_cfg,
			slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
			slave_adapter[EXT_SLAVE_TYPE_ACCEL],
			slave_adapter[EXT_SLAVE_TYPE_COMPASS],
			slave_adapter[EXT_SLAVE_TYPE_PRESSURE],
			arg);
		break;
	case MPU_PM_EVENT_HANDLED:
		dev_dbg(&client->adapter->dev, "%s: %d\n", __func__, cmd);
		complete(&mpu->completion);
		break;
	case MPU_READ_ACCEL:
		retval = inv_slave_read(
			mldl_cfg,
			slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
			slave_adapter[EXT_SLAVE_TYPE_ACCEL],
			slave[EXT_SLAVE_TYPE_ACCEL],
			pdata_slave[EXT_SLAVE_TYPE_ACCEL],
			(unsigned char __user *)arg);
		break;
	case MPU_READ_COMPASS:
		retval = inv_slave_read(
			mldl_cfg,
			slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
			slave_adapter[EXT_SLAVE_TYPE_COMPASS],
			slave[EXT_SLAVE_TYPE_COMPASS],
			pdata_slave[EXT_SLAVE_TYPE_COMPASS],
			(unsigned char __user *)arg);
		break;
	case MPU_READ_PRESSURE:
		retval = inv_slave_read(
			mldl_cfg,
			slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
			slave_adapter[EXT_SLAVE_TYPE_PRESSURE],
			slave[EXT_SLAVE_TYPE_PRESSURE],
			pdata_slave[EXT_SLAVE_TYPE_PRESSURE],
			(unsigned char __user *)arg);
		break;
	case MPU_GET_REQUESTED_SENSORS:
		if (copy_to_user(
			   (__u32 __user *)arg,
			   &mldl_cfg->inv_mpu_cfg->requested_sensors,
			   sizeof(mldl_cfg->inv_mpu_cfg->requested_sensors)))
			retval = -EFAULT;
		break;
	case MPU_SET_REQUESTED_SENSORS:
		mldl_cfg->inv_mpu_cfg->requested_sensors = arg;
		break;
	case MPU_GET_IGNORE_SYSTEM_SUSPEND:
		if (copy_to_user(
			(unsigned char __user *)arg,
			&mldl_cfg->inv_mpu_cfg->ignore_system_suspend,
			sizeof(mldl_cfg->inv_mpu_cfg->ignore_system_suspend)))
			retval = -EFAULT;
		break;
	case MPU_SET_IGNORE_SYSTEM_SUSPEND:
		mldl_cfg->inv_mpu_cfg->ignore_system_suspend = arg;
		break;
	case MPU_GET_MLDL_STATUS:
		if (copy_to_user(
			(unsigned char __user *)arg,
			&mldl_cfg->inv_mpu_state->status,
			sizeof(mldl_cfg->inv_mpu_state->status)))
			retval = -EFAULT;
		break;
	case MPU_GET_I2C_SLAVES_ENABLED:
		if (copy_to_user(
			(unsigned char __user *)arg,
			&mldl_cfg->inv_mpu_state->i2c_slaves_enabled,
			sizeof(mldl_cfg->inv_mpu_state->i2c_slaves_enabled)))
			retval = -EFAULT;
		break;
	case MPU_READ_ACCEL_OFFSET:
		{
			retval = copy_to_user((signed short __user *)arg,
				&cal_data, sizeof(cal_data));
			if (INV_SUCCESS != retval)
				dev_err(&client->adapter->dev,
				"%s: cmd %x, arg %lu\n", __func__, cmd, arg);
		}
		break;
	case MPU_ACTIVATE_SENSORS:
		mpu_dev_ioctl_activate_sensors(mpu);
		break;
	default:
		dev_err(&client->adapter->dev,
			"%s: Unknown cmd %x, arg %lu\n",
			__func__, cmd, arg);
		retval = -EINVAL;
	};

	mutex_unlock(&mpu->mutex);
	dev_dbg(&client->adapter->dev, "%s: %08x, %08lx, %d\n",
		__func__, cmd, arg, retval);

	if (retval > 0)
		retval = -retval;

	return retval;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
void mpu_dev_early_suspend(struct early_suspend *h)
{
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *)i2c_get_clientdata(this_client);
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	struct i2c_adapter *slave_adapter[EXT_SLAVE_NUM_TYPES];
	struct ext_slave_platform_data **pdata_slave = mldl_cfg->pdata_slave;
	int ii;
	printk(KERN_INFO"[@@@@@%s@@@@@]\n", __func__);

	mpu_early_notifier_callback(mpu, PM_SUSPEND_PREPARE, NULL);

	for (ii = 0; ii < EXT_SLAVE_NUM_TYPES; ii++) {
		if (!pdata_slave[ii])
			slave_adapter[ii] = NULL;
		else
			slave_adapter[ii] =
				i2c_get_adapter(pdata_slave[ii]->adapt_num);
	}
	slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE] = this_client->adapter;
	mutex_lock(&mpu->mutex);
	if (!mldl_cfg->inv_mpu_cfg->ignore_system_suspend) {
		(void)inv_mpu_suspend(mldl_cfg,
			slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
			slave_adapter[EXT_SLAVE_TYPE_ACCEL],
			slave_adapter[EXT_SLAVE_TYPE_COMPASS],
			slave_adapter[EXT_SLAVE_TYPE_PRESSURE],
			INV_ALL_SENSORS);
		}
	mutex_unlock(&mpu->mutex);
}

void mpu_dev_early_resume(struct early_suspend *h)
{
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *)i2c_get_clientdata(this_client);
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	struct i2c_adapter *slave_adapter[EXT_SLAVE_NUM_TYPES];
	struct ext_slave_platform_data **pdata_slave = mldl_cfg->pdata_slave;
	int ii;
	printk(KERN_INFO"[@@@@@%s@@@@@]\n", __func__);
	for (ii = 0; ii < EXT_SLAVE_NUM_TYPES; ii++) {
		if (!pdata_slave[ii])
			slave_adapter[ii] = NULL;
		else
			slave_adapter[ii] =
				i2c_get_adapter(pdata_slave[ii]->adapt_num);
	}
	slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE] = this_client->adapter;

	mutex_lock(&mpu->mutex);
	if (mpu->pid && !mldl_cfg->inv_mpu_cfg->ignore_system_suspend) {
		(void)inv_mpu_resume(mldl_cfg,
				slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
				slave_adapter[EXT_SLAVE_TYPE_ACCEL],
				slave_adapter[EXT_SLAVE_TYPE_COMPASS],
				slave_adapter[EXT_SLAVE_TYPE_PRESSURE],
				mldl_cfg->inv_mpu_cfg->requested_sensors);
	}
	mutex_unlock(&mpu->mutex);
	mpu_early_notifier_callback(mpu, PM_POST_SUSPEND, NULL);
}
#endif


void mpu_shutdown(struct i2c_client *client)
{
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *)i2c_get_clientdata(client);
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	struct i2c_adapter *slave_adapter[EXT_SLAVE_NUM_TYPES];
	struct ext_slave_platform_data **pdata_slave = mldl_cfg->pdata_slave;
	int ii;

	for (ii = 0; ii < EXT_SLAVE_NUM_TYPES; ii++) {
		if (!pdata_slave[ii])
			slave_adapter[ii] = NULL;
		else
			slave_adapter[ii] =
				i2c_get_adapter(pdata_slave[ii]->adapt_num);
	}
	slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE] = client->adapter;

	mutex_lock(&mpu->mutex);
	(void)inv_mpu_suspend(mldl_cfg,
			slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
			slave_adapter[EXT_SLAVE_TYPE_ACCEL],
			slave_adapter[EXT_SLAVE_TYPE_COMPASS],
			slave_adapter[EXT_SLAVE_TYPE_PRESSURE],
			INV_ALL_SENSORS);
	mutex_unlock(&mpu->mutex);
	dev_dbg(&client->adapter->dev, "%s\n", __func__);
}

int mpu_dev_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *)i2c_get_clientdata(client);
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	struct i2c_adapter *slave_adapter[EXT_SLAVE_NUM_TYPES];
	struct ext_slave_platform_data **pdata_slave = mldl_cfg->pdata_slave;
	int ii;
	printk(KERN_INFO"@@@@@%s@@@@@\n", __func__);
	for (ii = 0; ii < EXT_SLAVE_NUM_TYPES; ii++) {
		if (!pdata_slave[ii])
			slave_adapter[ii] = NULL;
		else
			slave_adapter[ii] =
				i2c_get_adapter(pdata_slave[ii]->adapt_num);
	}
	slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE] = client->adapter;

	mutex_lock(&mpu->mutex);
	if (!mldl_cfg->inv_mpu_cfg->ignore_system_suspend) {
		dev_dbg(&client->adapter->dev,
			"%s: suspending on event %d\n", __func__, mesg.event);
		(void)inv_mpu_suspend(mldl_cfg,
			slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
			slave_adapter[EXT_SLAVE_TYPE_ACCEL],
			slave_adapter[EXT_SLAVE_TYPE_COMPASS],
			slave_adapter[EXT_SLAVE_TYPE_PRESSURE],
			INV_ALL_SENSORS);
	} else {
		dev_dbg(&client->adapter->dev,
			"%s: Already suspended %d\n", __func__, mesg.event);
	}
	mutex_unlock(&mpu->mutex);

	if (mldl_cfg->pdata->poweron)
		mldl_cfg->pdata->poweron(0);
	return 0;
}

int mpu_dev_resume(struct i2c_client *client)
{
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *)i2c_get_clientdata(client);
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	struct i2c_adapter *slave_adapter[EXT_SLAVE_NUM_TYPES];
	struct ext_slave_platform_data **pdata_slave = mldl_cfg->pdata_slave;
	int ii;
	printk(KERN_INFO"@@@@@%s@@@@@\n", __func__);

	if (mldl_cfg->pdata->poweron)
		mldl_cfg->pdata->poweron(1);
	for (ii = 0; ii < EXT_SLAVE_NUM_TYPES; ii++) {
		if (!pdata_slave[ii])
			slave_adapter[ii] = NULL;
		else
			slave_adapter[ii] =
				i2c_get_adapter(pdata_slave[ii]->adapt_num);
	}
	slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE] = client->adapter;

	mutex_lock(&mpu->mutex);
	if (mpu->pid && !mldl_cfg->inv_mpu_cfg->ignore_system_suspend) {
		(void)inv_mpu_resume(mldl_cfg,
				slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
				slave_adapter[EXT_SLAVE_TYPE_ACCEL],
				slave_adapter[EXT_SLAVE_TYPE_COMPASS],
				slave_adapter[EXT_SLAVE_TYPE_PRESSURE],
				mldl_cfg->inv_mpu_cfg->requested_sensors);
		dev_dbg(&client->adapter->dev,
			"%s for pid %d\n", __func__, mpu->pid);
	}
	mutex_unlock(&mpu->mutex);
	return 0;
}

/* define which file operations are supported */
static const struct file_operations mpu_fops = {
	.owner = THIS_MODULE,
	.read = mpu_read,
	.poll = mpu_poll,
	.unlocked_ioctl = mpu_dev_ioctl,
	.open = mpu_dev_open,
	.release = mpu_release,
};

int inv_mpu_register_slave(struct module *slave_module,
			struct i2c_client *slave_client,
			struct ext_slave_platform_data *slave_pdata,
			struct ext_slave_descr *(*get_slave_descr)(void))
{
	struct mpu_private_data *mpu = mpu_private_data;
	struct mldl_cfg *mldl_cfg;
	struct ext_slave_descr *slave_descr;
	struct ext_slave_platform_data **pdata_slave;
	char *irq_name = NULL;
	int result = 0;

	if (!slave_client || !slave_pdata || !get_slave_descr)
		return -EINVAL;

	if (!mpu) {
		dev_err(&slave_client->adapter->dev,
			"%s: Null mpu_private_data\n", __func__);
		return -EINVAL;
	}
	mldl_cfg    = &mpu->mldl_cfg;
	pdata_slave = mldl_cfg->pdata_slave;
	slave_descr = get_slave_descr();

	if (!slave_descr) {
		dev_err(&slave_client->adapter->dev,
			"%s: Null ext_slave_descr\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&mpu->mutex);
	if (mpu->pid) {
		mutex_unlock(&mpu->mutex);
		return -EBUSY;
	}

	if (pdata_slave[slave_descr->type]) {
		result = -EBUSY;
		goto out_unlock_mutex;
	}

	slave_pdata->address	= slave_client->addr;
	slave_pdata->irq	= slave_client->irq;
	slave_pdata->adapt_num	= i2c_adapter_id(slave_client->adapter);

	dev_info(&slave_client->adapter->dev,
		"%s: +%s Type %d: Addr: %2x IRQ: %2d, Adapt: %2d\n",
		__func__,
		slave_descr->name,
		slave_descr->type,
		slave_pdata->address,
		slave_pdata->irq,
		slave_pdata->adapt_num);

	switch (slave_descr->type) {
	case EXT_SLAVE_TYPE_ACCEL:
		irq_name = "accelirq";
		break;
	case EXT_SLAVE_TYPE_COMPASS:
		irq_name = "compassirq";
		break;
	case EXT_SLAVE_TYPE_PRESSURE:
		irq_name = "pressureirq";
		break;
	default:
		irq_name = "none";
	};
	if (slave_descr->type == EXT_SLAVE_TYPE_COMPASS
		&& slave_descr->init) {
		int retry_cnt = 3;
#if defined(CONFIG_MPU_SENSORS_AK8975_411)
		unsigned int reset = mldl_cfg->pdata->reset;
#endif
		do {
			retry_cnt--;

			result = slave_descr->init(slave_client->adapter,
				slave_descr, slave_pdata);
			if (result) {
				dev_err(&slave_client->adapter->dev,
					"%s init failed %d, cnt %d\n",
					slave_descr->name, result, retry_cnt);
#if defined(CONFIG_MPU_SENSORS_AK8975_411)
				gpio_direction_output(reset, 0);
				usleep_range(30, 30);
				gpio_set_value_cansleep(reset, 1);
				usleep_range(30, 30);
#endif
			} else {
				break;
			}
		} while (retry_cnt);
		if (result) {
			dev_err(&slave_client->adapter->dev,
				"%s init failed %d, cnt %d\n",
				slave_descr->name, result, retry_cnt);
				goto out_unlock_mutex;
		}

	} else {
		if (slave_descr->init) {
			result = slave_descr->init(slave_client->adapter,
					slave_descr,
					slave_pdata);
			if (result) {
				dev_err(&slave_client->adapter->dev,
					"%s init failed %d\n",
					slave_descr->name, result);
				goto out_unlock_mutex;
			}
		}
	}

	if (slave_descr->type == EXT_SLAVE_TYPE_ACCEL &&
	    slave_descr->id == ACCEL_ID_MPU6050 &&
	    slave_descr->config) {
		/* pass a reference to the mldl_cfg data
		   structure to the mpu6050 accel "class" */
		struct ext_slave_config config;
		config.key = MPU_SLAVE_CONFIG_INTERNAL_REFERENCE;
		config.len = sizeof(struct mldl_cfg *);
		config.apply = true;
		config.data = mldl_cfg;
		result = slave_descr->config(
			slave_client->adapter, slave_descr,
			slave_pdata, &config);
		if (result) {
			LOG_RESULT_LOCATION(result);
			goto out_slavedescr_exit;
		}
	}
	pdata_slave[slave_descr->type] = slave_pdata;
	mpu->slave_modules[slave_descr->type] = slave_module;
	mldl_cfg->slave[slave_descr->type] = slave_descr;

	goto out_unlock_mutex;

out_slavedescr_exit:
	if (slave_descr->exit)
		slave_descr->exit(slave_client->adapter,
				  slave_descr, slave_pdata);
out_unlock_mutex:
	mutex_unlock(&mpu->mutex);

	if (!result && irq_name && (slave_pdata->irq > 0)) {
		int warn_result;
		dev_info(&slave_client->adapter->dev,
			"Installing %s irq using %d\n",
			irq_name,
			slave_pdata->irq);
		warn_result = slaveirq_init(slave_client->adapter,
					slave_pdata, irq_name);
		if (warn_result)
			dev_warn(&slave_client->adapter->dev,
				"%s irq assigned error: %d\n",
				slave_descr->name, warn_result);
	} else {
		dev_warn(&slave_client->adapter->dev,
			"%s irq not assigned: %d %d %d\n",
			slave_descr->name,
			result, (int)irq_name, slave_pdata->irq);
	}

	return result;
}
EXPORT_SYMBOL(inv_mpu_register_slave);

void inv_mpu_unregister_slave(struct i2c_client *slave_client,
			struct ext_slave_platform_data *slave_pdata,
			struct ext_slave_descr *(*get_slave_descr)(void))
{
	struct mpu_private_data *mpu = mpu_private_data;
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	struct ext_slave_descr *slave_descr;
	int result;

	if (!slave_client || !slave_pdata || !get_slave_descr)
		return;

	dev_info(&slave_client->adapter->dev, "%s\n", __func__);

	if (slave_pdata->irq)
		slaveirq_exit(slave_pdata);

	slave_descr = get_slave_descr();
	if (!slave_descr)
		return;

	mutex_lock(&mpu->mutex);

	if (slave_descr->exit) {
		result = slave_descr->exit(slave_client->adapter,
					slave_descr,
					slave_pdata);
		if (result)
			dev_err(&slave_client->adapter->dev,
				"Accel exit failed %d\n", result);
	}
	mldl_cfg->slave[slave_descr->type] = NULL;
	mldl_cfg->pdata_slave[slave_descr->type] = NULL;
	mpu->slave_modules[slave_descr->type] = NULL;

	mutex_unlock(&mpu->mutex);

}
EXPORT_SYMBOL(inv_mpu_unregister_slave);

static unsigned short normal_i2c[] = { I2C_CLIENT_END };

static const struct i2c_device_id mpu_id[] = {
	{"mpu3050", 0},
	{"mpu6050", 0},
	{"mpu6050_no_accel", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, mpu_id);

static int mpu3050_factory_on(struct i2c_client *client)
{
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *)i2c_get_clientdata(this_client);
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	int prev_gyro_suspended = 0;

	struct i2c_adapter *slave_adapter[EXT_SLAVE_NUM_TYPES];
	struct ext_slave_platform_data **pdata_slave = mldl_cfg->pdata_slave;
	int ii;

	printk(KERN_INFO"@@@@@ %s : %d @@@@@\n", __func__, __LINE__);

	for (ii = 0; ii < EXT_SLAVE_NUM_TYPES; ii++) {
		if (!pdata_slave[ii])
			slave_adapter[ii] = NULL;
		else
			slave_adapter[ii] =
				i2c_get_adapter(pdata_slave[ii]->adapt_num);
	}
	slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE] = client->adapter;

	mutex_lock(&mpu->mutex);
	if (1) {
		(void)inv_mpu_resume(mldl_cfg,
				slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
				slave_adapter[EXT_SLAVE_TYPE_ACCEL],
				slave_adapter[EXT_SLAVE_TYPE_COMPASS],
				slave_adapter[EXT_SLAVE_TYPE_PRESSURE],
				mldl_cfg->inv_mpu_cfg->requested_sensors);
	}
	mutex_unlock(&mpu->mutex);
	return prev_gyro_suspended;
}

static ssize_t mpu3050_power_on(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int count = 0;

	dev_dbg(dev, "this_client = %d\n", (int)this_client);
	count = sprintf(buf, "%d\n", (this_client != NULL ? 1 : 0));

	return count;
}

static ssize_t mpu3050_get_temp(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int count = 0;
	short int temperature = 0;
	unsigned char data[2];
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *)i2c_get_clientdata(this_client);
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;

	struct i2c_adapter *slave_adapter[EXT_SLAVE_NUM_TYPES];
	struct ext_slave_platform_data **pdata_slave = mldl_cfg->pdata_slave;
	int ii;

	for (ii = 0; ii < EXT_SLAVE_NUM_TYPES; ii++) {
		if (!pdata_slave[ii])
			slave_adapter[ii] = NULL;
		else
			slave_adapter[ii] =
				i2c_get_adapter(pdata_slave[ii]->adapt_num);
	}
	slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE] = this_client->adapter;

	mpu3050_factory_on(this_client);

	/*MPUREG_TEMP_OUT_H,	27 0x1b */
	/*MPUREG_TEMP_OUT_L,	28 0x1c */
	/* TEMP_OUT_H/L: 16-bit temperature data (2's complement data format) */
	inv_serial_read(slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
	DEFAULT_MPU_SLAVEADDR, MPUREG_TEMP_OUT_H, 2, data);
	temperature = (short) (((data[0]) << 8) | data[1]);
	temperature = (((temperature + 521) / 340) + 35);
	printk(KERN_INFO"read temperature = %d\n", temperature);

	count = sprintf(buf, "%d\n", temperature);

	return count;
}

static int gyro_do_calibrate(void)
{
	struct file *cal_filp;
	int err;
	mm_segment_t old_fs = {0};
		struct mpu_private_data *mpu =
	    (struct mpu_private_data *) i2c_get_clientdata(this_client);
	pr_info("%s: cal data (%d,%d,%d)\n", __func__,
			mpu->gyro_bias[0], mpu->gyro_bias[1],
				mpu->gyro_bias[2]);

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	cal_filp = filp_open(CALIBRATION_GYRO_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY,
			S_IRUGO | S_IWUSR | S_IWGRP);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		goto done;
	}
	err = cal_filp->f_op->write(cal_filp,
		(char *)&mpu->gyro_bias, 3 * sizeof(int),
			&cal_filp->f_pos);
	if (err != 3 * sizeof(int)) {
		pr_err("%s: Can't write the cal data to file\n", __func__);
		err = -EIO;
	}

	filp_close(cal_filp, current->files);
done:
	set_fs(old_fs);
	return err;
}

static ssize_t mpu6050_input_gyro_selftest_show(struct device *dev,
					struct device_attribute *attr,
						char *buf)
{
	int scaled_gyro_bias[3] = {0};
	int scaled_gyro_rms[3] = {0};
	int packet_count[3] = {0};
	int ratio[3] = {0};
	int result;
	int hw_result;
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *) i2c_get_clientdata(this_client);
	hw_result = mpu6050_gyro_hw_self_check(mpu->client,
					ratio);

	result = mpu6050_selftest_run(mpu->client,
					packet_count,
					scaled_gyro_bias,
					scaled_gyro_rms,
					mpu->gyro_bias);
	if (!result) {
			gyro_do_calibrate();
	} else {
		mpu->gyro_bias[0] = 0;
		mpu->gyro_bias[1] = 0;
		mpu->gyro_bias[2] = 0;
		result = -1;
	}

	return sprintf(buf, "%d,"
		       "%d.%03d,%d.%03d,%d.%03d,"
		       "%d.%03d,%d.%03d,%d.%03d,"
			"%d.%01d,%d.%01d,%d.%01d,"
		       "%d,%d,%d\n",
		       result | hw_result,
		       (int)abs(scaled_gyro_bias[0] / 1000),
		       (int)abs(scaled_gyro_bias[0]) % 1000,
		       (int)abs(scaled_gyro_bias[1] / 1000),
		       (int)abs(scaled_gyro_bias[1]) % 1000,
		       (int)abs(scaled_gyro_bias[2] / 1000),
		       (int)abs(scaled_gyro_bias[2]) % 1000,
		       scaled_gyro_rms[0] / 1000,
		       (int)abs(scaled_gyro_rms[0]) % 1000,
		       scaled_gyro_rms[1] / 1000,
		       (int)abs(scaled_gyro_rms[1]) % 1000,
		       scaled_gyro_rms[2] / 1000,
		       (int)abs(scaled_gyro_rms[2]) % 1000,
		       (int)abs(ratio[0]/10),
		       (int)abs(ratio[0])%10,
		       (int)abs(ratio[1]/10),
		       (int)abs(ratio[1])%10,
		       (int)abs(ratio[2]/10),
		       (int)abs(ratio[2])%10,
		       packet_count[0], packet_count[1], packet_count[2]);
}
static ssize_t mpu3050_acc_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	s16 x, y, z, temp;
	int count = 0;
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *) i2c_get_clientdata(this_client);
	struct i2c_client *client = mpu->client;
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	unsigned char data[6];

	struct i2c_adapter *slave_adapter[EXT_SLAVE_NUM_TYPES];
	struct ext_slave_platform_data **pdata_slave = mldl_cfg->pdata_slave;
	int ii;

	for (ii = 0; ii < EXT_SLAVE_NUM_TYPES; ii++) {
		if (!pdata_slave[ii])
			slave_adapter[ii] = NULL;
		else
			slave_adapter[ii] =
				i2c_get_adapter(pdata_slave[ii]->adapt_num);
	}
	slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE] = client->adapter;


	inv_serial_read(slave_adapter[EXT_SLAVE_TYPE_ACCEL],
		0x68, 0x3B, 6, data);

	x = (s16)(((data[0] << 8) | data[1]) - cal_data.x);/*CAL_DIV;*/
	y = (s16)(((data[2] << 8) | data[3]) - cal_data.y);/*CAL_DIV;*/
	z = (s16)(((data[4] << 8) | data[5]) - cal_data.z);/*CAL_DIV;*/


	if (mldl_cfg->pdata->orientation[0]) {
		x *= mldl_cfg->pdata->orientation[0];
		y *= mldl_cfg->pdata->orientation[4];
	} else {
		temp = x*mldl_cfg->pdata->orientation[1];
		x = y*mldl_cfg->pdata->orientation[3];
		y = temp;
	}
	z *= mldl_cfg->pdata->orientation[8];


	count = sprintf(buf, "%d, %d, %d\n", x, y, z);

	return count;

}

static ssize_t accel_calibration_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{

	int count = 0;
	int ret = 1;

	printk(buf, "%d %d %d\n",
		cal_data.x, cal_data.y, cal_data.z);

	if (!cal_data.x && !cal_data.y && !cal_data.z)
		ret = -1;

	count = sprintf(buf, "%d %d %d %d\n", ret, cal_data.x,
		cal_data.y, cal_data.z);

	return count;
}

static ssize_t accel_calibration_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	int err;
	int enable = 0;

	err = kstrtoint(buf, 10, &enable);

	if (err) {
		pr_err("ERROR: %s got bad char\n", __func__);
		return -EINVAL;
	}

	err = accel_do_calibrate(enable);
	if (err < 0) {
		pr_err("%s: accel_do_calibrate() failed\n", __func__);
		return err;
	}

	return size;
}

static int mpu_alert_factory_on(struct i2c_client *client)
{
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *)i2c_get_clientdata(this_client);
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	int prev_gyro_suspended = 0;

	struct i2c_adapter *slave_adapter[EXT_SLAVE_NUM_TYPES];
	struct ext_slave_platform_data **pdata_slave = mldl_cfg->pdata_slave;
	int ii;

	printk(KERN_INFO"@@@@@ %s : %d @@@@@\n", __func__, __LINE__);

	for (ii = 0; ii < EXT_SLAVE_NUM_TYPES; ii++) {
		if (!pdata_slave[ii])
			slave_adapter[ii] = NULL;
		else
			slave_adapter[ii] =
				i2c_get_adapter(pdata_slave[ii]->adapt_num);
	}
	slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE] = client->adapter;

	mutex_lock(&mpu->mutex);
	inv_mpu_resume(mldl_cfg,
				slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
				slave_adapter[EXT_SLAVE_TYPE_ACCEL],
				slave_adapter[EXT_SLAVE_TYPE_COMPASS],
				slave_adapter[EXT_SLAVE_TYPE_PRESSURE],
				INV_THREE_AXIS_ACCEL);
	mutex_unlock(&mpu->mutex);

	return prev_gyro_suspended;
}

static ssize_t accel_reactive_alert_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int err = 0;
	bool onoff = false, factory_test = false;
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *) i2c_get_clientdata(this_client);
	struct i2c_client *client = mpu->client;
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	struct i2c_adapter *slave_adapter;
	struct ext_slave_platform_data **pdata_slave = mldl_cfg->pdata_slave;

	unsigned char reg_data = 0;

	slave_adapter =
		i2c_get_adapter(pdata_slave[EXT_SLAVE_TYPE_ACCEL]->adapt_num);


	if (sysfs_streq(buf, "1")) {
		onoff = true;
	} else if (sysfs_streq(buf, "0")) {
		onoff = false;
	} else if (sysfs_streq(buf, "2")) {
		onoff = true;
		factory_test = true;

		err = inv_serial_read(slave_adapter, 0x68,
			MPUREG_INT_ENABLE, sizeof(reg_data), &reg_data);
		if (err)
			pr_err("%s: read INT failed\n", __func__);
		reg_data |= BIT_RAW_RDY_EN;
		err = inv_serial_single_write(slave_adapter, 0x68,
				 MPUREG_INT_ENABLE,
				 reg_data);
		if (err)
			pr_err("%s: i2c write INT reg failed\n", __func__);
		err = inv_serial_read(slave_adapter, 0x68,
				MPUREG_INT_ENABLE, sizeof(reg_data), &reg_data);
		if (err)
			pr_err("%s: read INT failed\n", __func__);
		mpu_alert_factory_on(this_client);
		err = inv_serial_single_write(slave_adapter, 0x68,
			MPUREG_SMPLRT_DIV,
			(unsigned char)19);
		if (err)
			pr_err("%s: set_DIV\n", __func__);
	} else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	if (onoff  && !mldl_cfg->inv_mpu_state->accel_reactive) {
		pr_info("reactive alert is on.\n");
		enable_irq_wake(client->irq);

	} else if (!onoff  && mldl_cfg->inv_mpu_state->accel_reactive) {
		pr_info("reactive alert is off.\n");
		disable_irq_wake(client->irq);
		err = inv_serial_read(slave_adapter, 0x68,
				MPUREG_INT_ENABLE, sizeof(reg_data), &reg_data);
		if (err)
			pr_err("%s: read INT failed\n", __func__);
		reg_data &= ~BIT_MOT_EN;
		err = inv_serial_single_write(slave_adapter, 0x68,
				 MPUREG_INT_ENABLE,
				 reg_data);
		if (err)
			pr_err("%s: i2c write INT reg failed\n", __func__);
	}

	mldl_cfg->inv_mpu_state->use_accel_reactive = onoff;
	mldl_cfg->inv_mpu_state->accel_reactive = onoff;
	mldl_cfg->inv_mpu_state->reactive_factory = factory_test;
	return count;
}


static ssize_t accel_reactive_alert_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *) i2c_get_clientdata(this_client);
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;

	if (mldl_cfg->inv_mpu_state->use_accel_reactive && 
		!mldl_cfg->inv_mpu_state->accel_reactive)
		return sprintf(buf, "%d\n", 1);
	else
		return sprintf(buf, "%d\n", 0);
}

static ssize_t mpu_vendor_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", MPU_VENDOR);
}

static ssize_t mpu_name_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", MPU_PART_ID);
}

#if defined(CONFIG_MPU_SENSORS_AK8975_411)
static int akm8975_wait_for_data_ready(struct i2c_adapter *sl_adapter)
{
	int err;
	u8 buf;
	int count = 10;

	while (1) {
		msleep(20);
		err = inv_serial_read(sl_adapter, 0x0C,
				AK8975_REG_ST1, sizeof(buf), &buf);
		if (err) {
			pr_err("%s: read data over i2c failed\n", __func__);
			return -EIO;
		}

		if (buf&0x1)
			break;

		count--;
		if (!count)
			break;
	}
	return 0;

}

static ssize_t ak8975_adc(struct device *dev,
		struct device_attribute *attr, char *strbuf)
{
	struct mpu_private_data *mpu =
		(struct mpu_private_data *) i2c_get_clientdata(this_client);
	struct i2c_client *client = mpu->client;
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;


	u8 buf[8];
	s16 x, y, z;
	int err, success;
	struct i2c_adapter *slave_adapter[EXT_SLAVE_NUM_TYPES];
	struct ext_slave_platform_data **pdata_slave = mldl_cfg->pdata_slave;
	int ii;

	if (pdata_slave[EXT_SLAVE_TYPE_COMPASS] == NULL)
		return snprintf(strbuf, PAGE_SIZE, "%s, %d, %d, %d\n",
			"NG", 0, 0, 0);

	for (ii = 0; ii < EXT_SLAVE_NUM_TYPES; ii++) {
		if (!pdata_slave[ii])
			slave_adapter[ii] = NULL;
		else
			slave_adapter[ii] =
				i2c_get_adapter(pdata_slave[ii]->adapt_num);
	}
	slave_adapter[EXT_SLAVE_TYPE_COMPASS] = client->adapter;

	mutex_lock(&mpu->mutex);

	/* start ADC conversion */
	err = inv_serial_single_write(slave_adapter[EXT_SLAVE_TYPE_COMPASS],
	0x0C, AK8975_REG_CNTL,
	AK8975_MODE_SNG_MEASURE);

	if (err)
		pr_err("ak8975_adc write err:%d\n", err);

	/* wait for ADC conversion to complete */

	err = akm8975_wait_for_data_ready
		(slave_adapter[EXT_SLAVE_TYPE_COMPASS]);
	if (err) {
		pr_err("%s: wait for data ready failed\n", __func__);
		return err;
	}

	msleep(20);/*msleep(10);*/
	/* get the value and report it */
	err = inv_serial_read(slave_adapter[EXT_SLAVE_TYPE_COMPASS], 0x0C,
					AK8975_REG_ST1, sizeof(buf), buf);

	if (err) {
		pr_err("%s: read data over i2c failed %d\n", __func__, err);
		mutex_unlock(&mpu->mutex);
		return -EIO;
	}
	mutex_unlock(&mpu->mutex);

	/* buf[0] is status1, buf[7] is status2 */
	if ((buf[0] == 0) | (buf[7] == 1))
		success = 0;
	else
		success = 1;

	x = buf[1] | (buf[2] << 8);
	y = buf[3] | (buf[4] << 8);
	z = buf[5] | (buf[6] << 8);

	pr_err("%s: raw x = %d, y = %d, z = %d\n", __func__, x, y, z);

	return snprintf(strbuf, PAGE_SIZE, "%s,%d,%d,%d\n",
		(success ? "OK" : "NG"), x, y, z);
}

static ssize_t ak8975_check_cntl(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *) i2c_get_clientdata(this_client);
	struct i2c_client *client = mpu->client;
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	struct i2c_adapter *slave_adapter[EXT_SLAVE_NUM_TYPES];
	struct ext_slave_platform_data **pdata_slave = mldl_cfg->pdata_slave;

	int ii, err;
	u8 data;

	if (pdata_slave[EXT_SLAVE_TYPE_COMPASS] == NULL)
		return snprintf(buf, PAGE_SIZE, "%s\n", "NG");

	for (ii = 0; ii < EXT_SLAVE_NUM_TYPES; ii++) {
		if (!pdata_slave[ii])
			slave_adapter[ii] = NULL;
		else
			slave_adapter[ii] =
				i2c_get_adapter(pdata_slave[ii]->adapt_num);
	}
	slave_adapter[EXT_SLAVE_TYPE_COMPASS] = client->adapter;
	mutex_lock(&mpu->mutex);
	err = inv_serial_single_write(slave_adapter[EXT_SLAVE_TYPE_COMPASS],
		0x0C, AK8975_REG_CNTL,
		AK8975_MODE_POWER_DOWN);

	if (err) {
		pr_err("ak8975_adc write err:%d\n", err);
		mutex_unlock(&mpu->mutex);
		return -EIO;
	}
	err = inv_serial_read(slave_adapter[EXT_SLAVE_TYPE_COMPASS], 0x0C,
				AK8975_REG_CNTL, sizeof(data), &data);
	if (err) {
		pr_err("%s: read data over i2c failed %d\n", __func__, err);
		mutex_unlock(&mpu->mutex);
		return -EIO;
	}
	mutex_unlock(&mpu->mutex);

	return snprintf(buf, PAGE_SIZE, "%s\n",
		data == AK8975_MODE_POWER_DOWN ? "OK" : "NG");

}

static ssize_t akm8975_rawdata_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *) i2c_get_clientdata(this_client);
	struct i2c_client *client = mpu->client;
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;

	short x = 0, y = 0, z = 0;
	int err;
	u8 data[8];

	struct i2c_adapter *slave_adapter[EXT_SLAVE_NUM_TYPES];
	struct ext_slave_platform_data **pdata_slave = mldl_cfg->pdata_slave;
	int ii;

	for (ii = 0; ii < EXT_SLAVE_NUM_TYPES; ii++) {
		if (!pdata_slave[ii])
			slave_adapter[ii] = NULL;
		else
			slave_adapter[ii] =
				i2c_get_adapter(pdata_slave[ii]->adapt_num);
	}
	slave_adapter[EXT_SLAVE_TYPE_COMPASS] = client->adapter;


	mutex_lock(&mpu->mutex);
	err = inv_serial_single_write(slave_adapter[EXT_SLAVE_TYPE_COMPASS],
		0x0C, AK8975_REG_CNTL,
		AK8975_MODE_SNG_MEASURE);

	if (err) {
		pr_err("ak8975_adc write err:%d\n", err);
		mutex_unlock(&mpu->mutex);
		goto done;

	}

	err = akm8975_wait_for_data_ready
		(slave_adapter[EXT_SLAVE_TYPE_COMPASS]);
	if (err) {
		mutex_unlock(&mpu->mutex);
		goto done;
	}

	/* get the value and report it */
	err = inv_serial_read(slave_adapter[EXT_SLAVE_TYPE_COMPASS], 0x0C,
					AK8975_REG_ST1, sizeof(data), data);

	if (err) {
		pr_err("%s: read data over i2c failed %d\n", __func__, err);
		mutex_unlock(&mpu->mutex);
		return -EIO;
	}

	mutex_unlock(&mpu->mutex);

	if (err) {
		pr_err("%s: failed to read %d bytes of mag data\n",
		       __func__, sizeof(data));
		goto done;
	}

	if (data[0] & 0x01) {
		x = (data[2] << 8) + data[1];
		y = (data[4] << 8) + data[3];
		z = (data[6] << 8) + data[5];
	} else
		pr_err("%s: invalid raw data(st1 = %d)\n",
					__func__, data[0] & 0x01);

done:
	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", x, y, z);
}

struct ak8975_config {
	char asa[COMPASS_NUM_AXES];	/* axis sensitivity adjustment */
};

struct ak8975_private_data {
	struct ak8975_config init;
};
static ssize_t ak8975c_get_status(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *) i2c_get_clientdata(this_client);
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	struct ext_slave_platform_data **pdata_slave = mldl_cfg->pdata_slave;
	int success;

	struct ak8975_private_data *private_data;
	if (pdata_slave[EXT_SLAVE_TYPE_COMPASS] == NULL)
		return snprintf(buf, PAGE_SIZE, "%s\n", "NG");

	private_data =
		(struct ak8975_private_data *)
		pdata_slave[EXT_SLAVE_TYPE_COMPASS]->private_data;
	if ((private_data->init.asa[0] == 0) |
		(private_data->init.asa[0] == 0xff) |
		(private_data->init.asa[1] == 0) |
		(private_data->init.asa[1] == 0xff) |
		(private_data->init.asa[2] == 0) |
		(private_data->init.asa[2] == 0xff))
		success = 0;
	else
		success = 1;

	return snprintf(buf, PAGE_SIZE, "%s\n", (success ? "OK" : "NG"));

}

int ak8975c_selftest(struct i2c_adapter *slave_adapter,
	struct ak8975_private_data *private_data, int *sf)
{
	u8 data;
	u8 buf[6];
	int count = 20;
	s16 x, y, z;

	/* set ATSC self test bit to 1 */
	inv_serial_single_write(slave_adapter, 0x0C,
	AK8975_REG_ASTC, 0x40);

	/* start self test */
	inv_serial_single_write(slave_adapter, 0x0C,
	AK8975_REG_CNTL, AK8975_MODE_SELF_TEST);

	/* wait for data ready */
	while (1) {
		msleep(20);
		inv_serial_read(slave_adapter, 0x0C,
			AK8975_REG_ST1, sizeof(data), &data);

		if (data == 1)
			break;
		count--;
		if (!count)
			break;
	}
	inv_serial_read(slave_adapter, 0x0C,
					AK8975_REG_HXL, sizeof(buf), buf);

		/* set ATSC self test bit to 0 */
	inv_serial_single_write(slave_adapter, 0x0C,
					AK8975_REG_ASTC, 0x00);

	x = buf[0] | (buf[1] << 8);
	y = buf[2] | (buf[3] << 8);
	z = buf[4] | (buf[5] << 8);

	/* Hadj = (H*(Asa+128))/256 */
	x = (x*(private_data->init.asa[0] + 128)) >> 8;
	y = (y*(private_data->init.asa[1] + 128)) >> 8;
	z = (z*(private_data->init.asa[2] + 128)) >> 8;

	pr_info("%s: self test x = %d, y = %d, z = %d\n",
		__func__, x, y, z);
	if ((x >= -200) && (x <= 200))
		pr_info("%s: x passed self test, expect -200<=x<=200\n",
			__func__);
	else
		pr_info("%s: x failed self test, expect -200<=x<=200\n",
			__func__);
	if ((y >= -200) && (y <= 200))
		pr_info("%s: y passed self test, expect -200<=y<=200\n",
			__func__);
	else
		pr_info("%s: y failed self test, expect -200<=y<=200\n",
			__func__);
	if ((z >= -3200) && (z <= -800))
		pr_info("%s: z passed self test, expect -3200<=z<=-800\n",
			__func__);
	else
		pr_info("%s: z failed self test, expect -3200<=z<=-800\n",
			__func__);

	sf[0] = x;
	sf[1] = y;
	sf[2] = z;

	if (((x >= -200) && (x <= 200)) &&
		((y >= -200) && (y <= 200)) &&
		((z >= -3200) && (z <= -800)))
		return 1;
	else
		return 0;


}

static ssize_t ak8975c_get_selftest(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mpu_private_data *mpu =
	    (struct mpu_private_data *) i2c_get_clientdata(this_client);
	struct i2c_client *client = mpu->client;
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;

	struct i2c_adapter *slave_adapter[EXT_SLAVE_NUM_TYPES];
	struct ext_slave_platform_data **pdata_slave = mldl_cfg->pdata_slave;
	struct ak8975_private_data *private_data;

	int ii, success;
	int sf[3] = {0,};
	int retry = 3;

	if (pdata_slave[EXT_SLAVE_TYPE_COMPASS] == NULL)
		return snprintf(buf, PAGE_SIZE, "%d, %d, %d, %d\n",
		0, 0, 0, 0);

	private_data =
		(struct ak8975_private_data *)
		pdata_slave[EXT_SLAVE_TYPE_COMPASS]->private_data;

	for (ii = 0; ii < EXT_SLAVE_NUM_TYPES; ii++) {
		if (!pdata_slave[ii])
			slave_adapter[ii] = NULL;
		else
			slave_adapter[ii] =
				i2c_get_adapter(pdata_slave[ii]->adapt_num);
	}
	slave_adapter[EXT_SLAVE_TYPE_COMPASS] = client->adapter;
	do {
		retry--;
		success = ak8975c_selftest(
			slave_adapter[EXT_SLAVE_TYPE_COMPASS],
			private_data, sf);
		if (success)
			break;
	} while (retry > 0);

	return snprintf(buf, PAGE_SIZE, "%d, %d, %d, %d\n",
		success, sf[0], sf[1], sf[2]);
}

static ssize_t akm_vendor_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", MAG_VENDOR);
}

static ssize_t akm_name_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", MAG_PART_ID);
}
#endif

static DEVICE_ATTR(power_on, S_IRUGO, mpu3050_power_on, NULL);

static DEVICE_ATTR(temperature, S_IRUGO,	mpu3050_get_temp, NULL);
static struct device_attribute dev_attr_gyro_selftest =
	__ATTR(selftest, S_IRUGO, mpu6050_input_gyro_selftest_show, NULL);
static DEVICE_ATTR(calibration, S_IRUGO|S_IWUSR|S_IWGRP,
	accel_calibration_show, accel_calibration_store);

static DEVICE_ATTR(raw_data, S_IRUGO, mpu3050_acc_read, NULL);

static DEVICE_ATTR(reactive_alert, S_IRUGO|S_IWUSR|S_IWGRP,
	accel_reactive_alert_show,
	accel_reactive_alert_store);

static DEVICE_ATTR(vendor, S_IRUGO, mpu_vendor_show, NULL);
static DEVICE_ATTR(name, S_IRUGO, mpu_name_show, NULL);


#if defined(CONFIG_MPU_SENSORS_AK8975_411)
static DEVICE_ATTR(adc, S_IRUGO, ak8975_adc, NULL);

static DEVICE_ATTR(dac, S_IRUGO,
		ak8975_check_cntl, NULL);
static DEVICE_ATTR(status, S_IRUGO,
		ak8975c_get_status, NULL);
static DEVICE_ATTR(selftest, S_IRUGO,
		ak8975c_get_selftest, NULL);

static struct device_attribute dev_attr_mag_rawdata =
	__ATTR(raw_data, S_IRUGO,
	akm8975_rawdata_show, NULL);

static struct device_attribute dev_attr_mag_vendor =
	__ATTR(vendor, S_IRUGO,
	akm_vendor_show, NULL);

static struct device_attribute dev_attr_mag_name =
	__ATTR(name, S_IRUGO,
	akm_name_show, NULL);
#endif

static struct device_attribute *gyro_sensor_attrs[] = {
	&dev_attr_power_on,
	&dev_attr_temperature,
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_gyro_selftest,
	NULL,
};

static struct device_attribute *accel_sensor_attrs[] = {
	&dev_attr_raw_data,
	&dev_attr_calibration,
	&dev_attr_reactive_alert,
	&dev_attr_vendor,
	&dev_attr_name,
	NULL,
};

#if defined(CONFIG_MPU_SENSORS_AK8975_411)
static struct device_attribute *magnetic_sensor_attrs[] = {
	&dev_attr_adc,
	&dev_attr_mag_rawdata,
	&dev_attr_dac,
	&dev_attr_status,
	&dev_attr_selftest,
	&dev_attr_mag_vendor,
	&dev_attr_mag_name,
	NULL,
};
#endif

static struct device *gsensorcal;


static struct device *gyro_sensor_device;
static struct device *accel_sensor_device;

#if defined(CONFIG_MPU_SENSORS_AK8975_411)
static struct device *magnetic_sensor_device;
#endif

int mpu_probe(struct i2c_client *client, const struct i2c_device_id *devid)
{
	struct mpu_platform_data *pdata;
	struct mpu_private_data *mpu;
	struct mldl_cfg *mldl_cfg;
	int res = 0;
	int ii = 0;

	dev_info(&client->adapter->dev, "%s: %d\n", __func__, ii++);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		res = -ENODEV;
		goto out_check_functionality_failed;
	}

	mpu = kzalloc(sizeof(struct mpu_private_data), GFP_KERNEL);
	if (!mpu) {
		res = -ENOMEM;
		goto out_alloc_data_failed;
	}
	mldl_cfg = &mpu->mldl_cfg;
	mldl_cfg->mpu_ram	= &mpu->mpu_ram;
	mldl_cfg->mpu_gyro_cfg	= &mpu->mpu_gyro_cfg;
	mldl_cfg->mpu_offsets	= &mpu->mpu_offsets;
	mldl_cfg->mpu_chip_info	= &mpu->mpu_chip_info;
	mldl_cfg->inv_mpu_cfg	= &mpu->inv_mpu_cfg;
	mldl_cfg->inv_mpu_state	= &mpu->inv_mpu_state;

	mldl_cfg->mpu_ram->length = MPU_MEM_NUM_RAM_BANKS * MPU_MEM_BANK_SIZE;
	mldl_cfg->mpu_ram->ram = kzalloc(mldl_cfg->mpu_ram->length, GFP_KERNEL);
	if (!mldl_cfg->mpu_ram->ram) {
		res = -ENOMEM;
		goto out_alloc_ram_failed;
	}
	mpu_private_data = mpu;
	i2c_set_clientdata(client, mpu);
	this_client = client;
	mpu->client = client;

	init_waitqueue_head(&mpu->mpu_event_wait);
	mutex_init(&mpu->mutex);
	init_completion(&mpu->completion);

	mpu->response_timeout = 1;	/* Seconds */
	mpu->timeout.function = mpu_pm_timeout;
	mpu->timeout.data = (u_long) mpu;
	init_timer(&mpu->timeout);

	mpu->activate_timeout = 10;
	hrtimer_init(&mpu->activate_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	mpu->activate_timer.function = mpu_actiavte_sensors_callback;

	pdata = (struct mpu_platform_data *)client->dev.platform_data;
	if (!pdata) {
		dev_err(&client->adapter->dev,
			 "Missing platform data for mpu\n");
		goto out_whoami_failed;
	}
	mldl_cfg->pdata = pdata;
	if (mldl_cfg->pdata->poweron)
		mldl_cfg->pdata->poweron(1);

	mldl_cfg->mpu_chip_info->addr = client->addr;
	res = inv_mpu_open(&mpu->mldl_cfg, client->adapter, NULL, NULL, NULL);

	if (res) {
		dev_err(&client->adapter->dev,
			"Unable to open %s %d\n", MPU_NAME, res);
		res = -ENODEV;
		goto out_whoami_failed;
	}

	mpu->dev.minor = MISC_DYNAMIC_MINOR;
	mpu->dev.name = "mpu";
	mpu->dev.fops = &mpu_fops;
	res = misc_register(&mpu->dev);
	if (res < 0) {
		dev_err(&client->adapter->dev,
			"ERROR: misc_register returned %d\n", res);
		goto out_misc_register_failed;
	}

	if (client->irq) {
		dev_info(&client->adapter->dev,
			 "Installing irq using %d\n", client->irq);
		res = mpuirq_init(client, mldl_cfg);
		if (res)
			goto out_mpuirq_failed;
	} else {
		dev_warn(&client->adapter->dev,
			 "Missing %s IRQ\n", MPU_NAME);
	}
	if (!strcmp(mpu_id[1].name, devid->name)) {
		/* Special case to re-use the inv_mpu_register_slave */
		struct ext_slave_platform_data *slave_pdata;
		slave_pdata = kzalloc(sizeof(*slave_pdata), GFP_KERNEL);
		if (!slave_pdata) {
			res = -ENOMEM;
			goto out_slave_pdata_kzalloc_failed;
		}
		slave_pdata->bus = EXT_SLAVE_BUS_PRIMARY;
		for (ii = 0; ii < 9; ii++)
			slave_pdata->orientation[ii] = pdata->orientation[ii];
		res = inv_mpu_register_slave(
			NULL, client,
			slave_pdata,
			mpu6050_get_slave_descr);
		if (res) {
			/* if inv_mpu_register_slave fails there are no pointer
			   references to the memory allocated to slave_pdata */
			kfree(slave_pdata);
			goto out_slave_pdata_kzalloc_failed;
		}
	}

#ifdef CONFIG_INPUT_YAS_MAGNETOMETER
	{
	__s8 orientation[9] = {
		1, 0, 0,
		0, 1, 0,
		0, 0, 1
	};
		/* Special case to re-use the inv_mpu_register_slave */
		struct ext_slave_platform_data *slave_pdata;
		slave_pdata = kzalloc(sizeof(*slave_pdata), GFP_KERNEL);
		if (!slave_pdata) {
			res = -ENOMEM;
			goto out_slave_pdata_kzalloc_failed;
		}
		slave_pdata->bus = EXT_SLAVE_BUS_PRIMARY;
		for (ii = 0; ii < 9; ii++)
			slave_pdata->orientation[ii] = orientation[ii];
		res = inv_mpu_register_slave(
			NULL, client,
			slave_pdata,
			yas530_ext_get_slave_descr);
		if (res) {
			/* if inv_mpu_register_slave fails there are no pointer
			   references to the memory allocated to slave_pdata */
			kfree(slave_pdata);
			goto out_slave_pdata_kzalloc_failed;
		}
	}
#endif /*CONFIG_INPUT_YAS_MAGNETOMETER*/


	res = sensors_register(gyro_sensor_device, NULL,
	gyro_sensor_attrs, "gyro_sensor");
	if (res) {
		printk(KERN_ERR
			"%s: cound not register gyro sensor device(%d).\n",
			__func__, res);
	}

	res = sensors_register(accel_sensor_device, NULL,
		accel_sensor_attrs, "accelerometer_sensor");
	if (res) {
		printk(KERN_ERR
			"%s: cound not register accelerometer sensor device(%d).\n",
			__func__, res);
		goto out_sensor_register_failed;
	}

#if defined(CONFIG_MPU_SENSORS_AK8975_411)
	res = sensors_register(magnetic_sensor_device, NULL,
		magnetic_sensor_attrs, "magnetic_sensor");
	if (res) {
		printk(KERN_ERR
			"%s: cound not register magnetic sensor device(%d).\n",
			__func__, res);
		goto out_sensor_register_failed;
	}
#endif

	gsensorcal = device_create(sec_class, NULL, 0, mpu, "gsensorcal");
	if (IS_ERR(gsensorcal))
		printk(KERN_ERR "Failed to create device!");

	if (device_create_file(gsensorcal, &dev_attr_calibration) < 0) {
		printk(KERN_ERR "Failed to create device file(%s)!\n",
			dev_attr_calibration.attr.name);
		goto out_gsensorcal_failed;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
		mpu->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
		mpu->early_suspend.suspend = mpu_dev_early_suspend;
		mpu->early_suspend.resume = mpu_dev_early_resume;
		register_early_suspend(&mpu->early_suspend);
#endif

	return res;

out_gsensorcal_failed:
out_sensor_register_failed:
out_slave_pdata_kzalloc_failed:
	if (client->irq)
		mpuirq_exit();
out_mpuirq_failed:
	misc_deregister(&mpu->dev);
out_misc_register_failed:
	inv_mpu_close(&mpu->mldl_cfg, client->adapter, NULL, NULL, NULL);
out_whoami_failed:
	kfree(mldl_cfg->mpu_ram->ram);
	mpu_private_data = NULL;
out_alloc_ram_failed:
	kfree(mpu);
out_alloc_data_failed:
out_check_functionality_failed:
	dev_err(&client->adapter->dev, "%s failed %d\n", __func__, res);
	return res;

}

static int mpu_remove(struct i2c_client *client)
{
	struct mpu_private_data *mpu = i2c_get_clientdata(client);
	struct i2c_adapter *slave_adapter[EXT_SLAVE_NUM_TYPES];
	struct mldl_cfg *mldl_cfg = &mpu->mldl_cfg;
	struct ext_slave_platform_data **pdata_slave = mldl_cfg->pdata_slave;
	int ii;

	for (ii = 0; ii < EXT_SLAVE_NUM_TYPES; ii++) {
		if (!pdata_slave[ii])
			slave_adapter[ii] = NULL;
		else
			slave_adapter[ii] =
				i2c_get_adapter(pdata_slave[ii]->adapt_num);
	}

	slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE] = client->adapter;
	dev_dbg(&client->adapter->dev, "%s\n", __func__);

	inv_mpu_close(mldl_cfg,
		slave_adapter[EXT_SLAVE_TYPE_GYROSCOPE],
		slave_adapter[EXT_SLAVE_TYPE_ACCEL],
		slave_adapter[EXT_SLAVE_TYPE_COMPASS],
		slave_adapter[EXT_SLAVE_TYPE_PRESSURE]);

	if (mldl_cfg->slave[EXT_SLAVE_TYPE_ACCEL] &&
		(mldl_cfg->slave[EXT_SLAVE_TYPE_ACCEL]->id ==
			ACCEL_ID_MPU6050)) {
		struct ext_slave_platform_data *slave_pdata =
			mldl_cfg->pdata_slave[EXT_SLAVE_TYPE_ACCEL];
		inv_mpu_unregister_slave(
			client,
			mldl_cfg->pdata_slave[EXT_SLAVE_TYPE_ACCEL],
			mpu6050_get_slave_descr);
		kfree(slave_pdata);
	}

	if (client->irq)
		mpuirq_exit();

	misc_deregister(&mpu->dev);


	kfree(mpu->mldl_cfg.mpu_ram->ram);
	kfree(mpu);

	return 0;
}

static struct i2c_driver mpu_driver = {
	.class = I2C_CLASS_HWMON,
	.probe = mpu_probe,
	.remove = mpu_remove,
	.id_table = mpu_id,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = MPU_NAME,
		   },
	.address_list = normal_i2c,
	.shutdown = mpu_shutdown,	/* optional */
	.suspend = mpu_dev_suspend,	/* optional */
	.resume = mpu_dev_resume,	/* optional */

};

static int __init mpu_init(void)
{
	int res = i2c_add_driver(&mpu_driver);
	pr_info("%s: Probe name %s\n", __func__, MPU_NAME);
	if (res)
		pr_err("%s failed\n", __func__);
	return res;
}

static void __exit mpu_exit(void)
{
	pr_info("%s\n", __func__);
	i2c_del_driver(&mpu_driver);
}

module_init(mpu_init);
module_exit(mpu_exit);

MODULE_AUTHOR("Invensense Corporation");
MODULE_DESCRIPTION("User space character device interface for MPU");
MODULE_LICENSE("GPL");
MODULE_ALIAS(MPU_NAME);
