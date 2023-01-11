/******************** (C) COPYRIGHT 2010 STMicroelectronics ********************
 *
 * File Name          : mxc400x.c
 * Description        : MXC400x accelerometer sensor API
 *
 *******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
 * OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
 * PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
 * AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
 * INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
 * CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
 * INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * THIS SOFTWARE IS SPECIFICALLY DESIGNED FOR EXCLUSIVE USE WITH ST PARTS.
 *

 ******************************************************************************/

#include	<linux/err.h>
#include	<linux/errno.h>
#include	<linux/delay.h>
#include	<linux/fs.h>
#include	<linux/i2c.h>
#include <linux/module.h>
#include	<linux/input.h>
#include	<linux/input-polldev.h>
#include	<linux/miscdevice.h>
#include	<linux/uaccess.h>
#include  <linux/slab.h>

#include	<linux/workqueue.h>
#include	<linux/irq.h>
#include	<linux/gpio.h>
#include	<linux/interrupt.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include        <linux/earlysuspend.h>
#endif

#include        "mxc400x.h"

#define	I2C_RETRY_DELAY		5
#define	I2C_RETRIES		5

#define DEVICE_INFO         "Memsic, MXC400X"
#define DEVICE_INFO_LEN     32

/* end RESUME STATE INDICES */

#define DEBUG    1
//#define MXC400X_DEBUG  0

#define	MAX_INTERVAL	50
#if 0
//#ifdef __KERNEL__
static struct mxc400x_acc_platform_data mxc400x_plat_data = {
    .poll_interval = 20,
    .min_interval = 10,
};
//#endif

//#ifdef I2C_BUS_NUM_STATIC_ALLOC
static struct i2c_board_info  mxc400x_i2c_boardinfo = {
        I2C_BOARD_INFO(MXC400X_ACC_DEV_NAME, MXC400X_ACC_I2C_ADDR),
//#ifdef __KERNEL__
        .platform_data = &mxc400x_plat_data
//#endif
};
//#endif
#endif

struct mxc400x_acc_data {
	struct i2c_client *client;
	struct mxc400x_acc_platform_data *pdata;

	struct mutex lock;

	int hw_initialized;
	/* hw_working=-1 means not tested yet */
	int hw_working;
	atomic_t enabled;
	int on_before_suspend;

#ifdef CONFIG_HAS_EARLYSUSPEND
        struct early_suspend early_suspend;
#endif
};

/*
 * Because misc devices can not carry a mxc400x from driver register to
 * open, we keep this global.  This limits the driver to a single instance.
 */
struct mxc400x_acc_data *mxc400x_acc_misc_data;
struct i2c_client      *mxc400x_i2c_client;

static int mxc400x_acc_i2c_read(struct mxc400x_acc_data *acc, u8 * buf, int len)
{
	int err;
	int tries = 0;

	struct i2c_msg	msgs[] = {
		{
			.addr = acc->client->addr,
			.flags = acc->client->flags & I2C_M_TEN,
			.len = 1,
			.buf = buf, },
		{
			.addr = acc->client->addr,
			.flags = (acc->client->flags & I2C_M_TEN) | I2C_M_RD,
			.len = len,
			.buf = buf, },
	};

	do {
		err = i2c_transfer(acc->client->adapter, msgs, 2);
		if (err != 2)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 2) && (++tries < I2C_RETRIES));

	if (err != 2) {
		dev_err(&acc->client->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}


	return err;
}

static int mxc400x_acc_i2c_write(struct mxc400x_acc_data *acc, u8 * buf, int len)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = { { .addr = acc->client->addr,
			.flags = acc->client->flags & I2C_M_TEN,
			.len = len + 1, .buf = buf, }, };
	do {
		err = i2c_transfer(acc->client->adapter, msgs, 1);
		if (err != 1)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		dev_err(&acc->client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int mxc400x_acc_hw_init(struct mxc400x_acc_data *acc)
{
	int err = -1;
	u8 buf[7] = {0};

	printk(KERN_INFO "%s: hw init start\n", MXC400X_ACC_DEV_NAME);

	buf[0] = WHO_AM_I;
	err = mxc400x_acc_i2c_read(acc, buf, 1);
	if (err < 0)
		goto error_firstread;
	else
		acc->hw_working = 1;
	if ((buf[0] & 0x3F) != WHOAMI_MXC400X_ACC) {
		err = -1; /* choose the right coded error */
		goto error_unknown_device;
	}

	acc->hw_initialized = 1;
	printk(KERN_INFO "%s: hw init done\n", MXC400X_ACC_DEV_NAME);
	return 0;

error_firstread:
	acc->hw_working = 0;
	dev_warn(&acc->client->dev, "Error reading WHO_AM_I: is device "
		"available/working?\n");
	goto error1;
error_unknown_device:
	dev_err(&acc->client->dev,
		"device unknown. Expected: 0x%x,"
		" Replies: 0x%x\n", WHOAMI_MXC400X_ACC, buf[0]);
error1:
	acc->hw_initialized = 0;
	dev_err(&acc->client->dev, "hw init error 0x%x,0x%x: %d\n", buf[0],
			buf[1], err);
	return err;
}

static void mxc400x_acc_device_power_off(struct mxc400x_acc_data *acc)
{
	int err;
	u8 buf[2] = { MXC400X_REG_CTRL, MXC400X_CTRL_PWRDN };

	err = mxc400x_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		dev_err(&acc->client->dev, "soft power off failed: %d\n", err);
}

static int mxc400x_acc_device_power_on(struct mxc400x_acc_data *acc)
{

	int err = -1;
	u8 buf[2] = {MXC400X_REG_FAC, MXC400X_PASSWORD};
	printk(KERN_INFO "%s: %s entry\n",MXC400X_ACC_DEV_NAME, __func__);
	//+/-2G
/*
	err = mxc400x_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		dev_err(&acc->client->dev, "soft factory password write failed: %d\n", err);

	buf[0] = MXC400X_REG_FSRE;
	buf[1] = MXC400X_RANGE_8G;
	err = mxc400x_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		dev_err(&acc->client->dev, "soft change to +/8g failed: %d\n", err);
*/
	buf[0] = MXC400X_REG_CTRL;
	buf[1] = MXC400X_CTRL_PWRON;
	err = mxc400x_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		dev_err(&acc->client->dev, "soft power on failed: %d\n", err);
	msleep(300);
	if (!acc->hw_initialized) {
		err = mxc400x_acc_hw_init(acc);
		if (acc->hw_working == 1 && err < 0) {
			mxc400x_acc_device_power_off(acc);
			return err;
		}
	}

	return 0;
}

static int mxc400x_acc_register_read(struct mxc400x_acc_data *acc, u8 *buf,
		u8 reg_address)
{

	int err = -1;
	buf[0] = (reg_address);
	err = mxc400x_acc_i2c_read(acc, buf, 1);
	return err;
}

/* */

static int mxc400x_acc_get_acceleration_data(struct mxc400x_acc_data *acc,
		int *xyz)
{
	int err = -1;
	/* Data bytes from hardware x, y,z */
	unsigned char acc_data[MXC400X_DATA_LENGTH] = {0, 0, 0, 0, 0, 0};

	acc_data[0] = MXC400X_REG_X;
	err = mxc400x_acc_i2c_read(acc, acc_data, MXC400X_DATA_LENGTH);
//	printk("acc[0] = %x, acc[1] = %x, acc[2] = %x, acc[3] = %x, acc[4] = %x, acc[5] = %x\n", acc_data[0], acc_data[1], acc_data[2], 																								acc_data[3], acc_data[4], acc_data[5]);

	if (err < 0)
    {
        #ifdef MXC400X_DEBUG
        printk(KERN_INFO "%s I2C read xy error %d\n", MXC400X_ACC_DEV_NAME, err);
        #endif
		return err;
    }


    xyz[0] = (signed short)(acc_data[0] << 8 | acc_data[1]) >> 4;
	xyz[1] = (signed short)(acc_data[2] << 8 | acc_data[3]) >> 4;
	xyz[2] = (signed short)(acc_data[4] << 8 | acc_data[5]) >> 4;
	#ifdef MXC400X_DEBUG
	printk(KERN_INFO "%s read x=%d, y=%d, z=%d\n",MXC400X_ACC_DEV_NAME, xyz[0], xyz[1], xyz[2]);
	#endif
	return err;
}


static int mxc400x_acc_enable(struct mxc400x_acc_data *acc)
{
	int err;
    //printk("mxc400x_acc_enable\n");
	if (!atomic_cmpxchg(&acc->enabled, 0, 1)) {
		err = mxc400x_acc_device_power_on(acc);

		if (err < 0) {
			atomic_set(&acc->enabled, 0);
			return err;
		}

	}

	return 0;
}

static int mxc400x_acc_disable(struct mxc400x_acc_data *acc)
{
	if (atomic_cmpxchg(&acc->enabled, 1, 0)) {
		mxc400x_acc_device_power_off(acc);
	}

	return 0;
}

static int mxc400x_acc_misc_open(struct inode *inode, struct file *file)
{
	int err;
	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;

	file->private_data = mxc400x_acc_misc_data;

	return 0;
}

static long mxc400x_acc_misc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;

	int err;
	int interval;
	int xyz[3] = {0,0,0};
	int reg;
	struct mxc400x_acc_data *acc = file->private_data;

//	printk(KERN_INFO "%s: %s call with cmd 0x%x and arg 0x%x\n",
//			MXC400X_ACC_DEV_NAME, __func__, cmd, (unsigned int)arg);
	mutex_lock(&acc->lock);
	switch (cmd) {
	case MXC400X_ACC_IOCTL_GET_DELAY:
		interval = acc->pdata->poll_interval;
		if (copy_to_user(argp, &interval, sizeof(interval))){
			mutex_unlock(&acc->lock);
			return -EFAULT;
		}
		break;

	case MXC400X_ACC_IOCTL_SET_DELAY:
		if (copy_from_user(&interval, argp, sizeof(interval))){
			mutex_unlock(&acc->lock);
			return -EFAULT;
		}
		if (interval < 0 || interval > 1000){
			mutex_unlock(&acc->lock);
			return -EINVAL;
		}
		if(interval > MAX_INTERVAL)
			interval = MAX_INTERVAL;
		acc->pdata->poll_interval = max(interval,
				acc->pdata->min_interval);
		break;

	case MXC400X_ACC_IOCTL_SET_ENABLE:
		if (copy_from_user(&interval, argp, sizeof(interval))){
			mutex_unlock(&acc->lock);
			return -EFAULT;
		}
		if (interval > 1){
			mutex_unlock(&acc->lock);
			return -EINVAL;
		}
		if (interval)
			err = mxc400x_acc_enable(acc);
		else
			err = mxc400x_acc_disable(acc);

		mutex_unlock(&acc->lock);
		return err;
		break;

	case MXC400X_ACC_IOCTL_GET_ENABLE:
		interval = atomic_read(&acc->enabled);
		if (copy_to_user(argp, &interval, sizeof(interval))){
			mutex_unlock(&acc->lock);
			return -EINVAL;
		}
		break;
	case MXC400X_ACC_IOCTL_GET_COOR_XYZ:
		err = mxc400x_acc_get_acceleration_data(acc, xyz);
		if (err < 0){
			mutex_unlock(&acc->lock);
			return err;
		}

		if (copy_to_user(argp, xyz, sizeof(xyz))) {
			printk(KERN_ERR " %s %d error in copy_to_user \n",
				__func__, __LINE__);
			mutex_unlock(&acc->lock);
			return -EINVAL;
		}
		break;
	case MXC400X_ACC_IOCTL_GET_TEMP:
	{
		u8  tempture = 0;

		err = mxc400x_acc_register_read(acc, &tempture,MXC400X_REG_TEMP);
		if (err < 0)
		{
			printk("%s, error read register MXC400X_REG_TEMP\n", __func__);
			mutex_unlock(&acc->lock);
			return err;
		}

		if (copy_to_user(argp, &tempture, sizeof(tempture))) {
			printk(KERN_ERR " %s %d error in copy_to_user \n",
				__func__, __LINE__);
			mutex_unlock(&acc->lock);
			return -EINVAL;
		}
	}
		break;
	case MXC400X_ACC_IOCTL_GET_CHIP_ID:
	{
		u8 devid = 0;
		err = mxc400x_acc_register_read(acc, &devid, WHO_AM_I);
		if (err < 0) {
			printk("%s, error read register WHO_AM_I\n", __func__);
			mutex_unlock(&acc->lock);
			return -EAGAIN;
		}

		if (copy_to_user(argp, &devid, sizeof(devid))) {
			printk("%s error in copy_to_user(IOCTL_GET_CHIP_ID)\n", __func__);
			mutex_unlock(&acc->lock);
			return -EINVAL;
		}
	}
            break;
	case MXC400X_ACC_IOCTL_READ_REG:
	{
		u8 content = 0;
		if (copy_from_user(&reg, argp, sizeof(reg))){
			mutex_unlock(&acc->lock);
			return -EFAULT;
		}
		err = mxc400x_acc_register_read(acc, &content, reg);
		if (err < 0) {
			printk("%s, error read register %x\n", __func__, reg);
			mutex_unlock(&acc->lock);
			return -EAGAIN;
		}
		if (copy_to_user(argp, &content, sizeof(content))) {
			printk("%s error in copy_to_user(IOCTL_GET_CHIP_ID)\n", __func__);
			mutex_unlock(&acc->lock);
			return -EINVAL;
		}

	}
	break;
	case MXC400X_ACC_IOCTL_WRITE_REG:
	{
		if (copy_from_user(&reg, argp, sizeof(reg))){
			mutex_unlock(&acc->lock);
			return -EFAULT;
		}
		err = mxc400x_acc_i2c_write(acc, (u8*)&reg, 1);
		if (err < 0) {
			printk("%s, error write register %x\n", __func__, reg);
			mutex_unlock(&acc->lock);
			return -EAGAIN;
		}
	}
	break;

	default:
	    printk("%s no cmd error\n", __func__);
	    mutex_unlock(&acc->lock);
		return -EINVAL;
	}

    mutex_unlock(&acc->lock);
	return 0;
}

static const struct file_operations mxc400x_acc_misc_fops = {
		.owner = THIS_MODULE,
		.open = mxc400x_acc_misc_open,
		.unlocked_ioctl = mxc400x_acc_misc_ioctl,
#ifdef CONFIG_COMPAT
		.compat_ioctl	= mxc400x_acc_misc_ioctl,
#endif
};

static struct miscdevice mxc400x_acc_misc_device = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = MXC400X_ACC_DEV_NAME,
		.fops = &mxc400x_acc_misc_fops,
};


#if 0
static int mxc400x_acc_validate_pdata(struct mxc400x_acc_data *acc)
{
	acc->pdata->poll_interval = max(acc->pdata->poll_interval,
			acc->pdata->min_interval);

	/* Enforce minimum polling interval */
	if (acc->pdata->poll_interval < acc->pdata->min_interval) {
		dev_err(&acc->client->dev, "minimum poll interval violated\n");
		return -EINVAL;
	}

	return 0;
}
#endif


#ifdef CONFIG_HAS_EARLYSUSPEND
static void mxc400x_early_suspend (struct early_suspend* es);
static void mxc400x_early_resume (struct early_suspend* es);
#endif

static struct of_device_id memsic_match_table[] = {
        { .compatible = "memsic,mxc400x", },
        {}
};
static int mxc400x_acc_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{

	struct mxc400x_acc_data *acc;

	int err = -1;
	int tempvalue;

	printk("%s: probe start.\n", MXC400X_ACC_DEV_NAME);
/*
	if (client->dev.platform_data == NULL) {
		dev_err(&client->dev, "platform data is NULL. exiting.\n");
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}
*/
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "client not i2c capable\n");
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	if (!i2c_check_functionality(client->adapter,
					I2C_FUNC_SMBUS_BYTE |
					I2C_FUNC_SMBUS_BYTE_DATA |
					I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_err(&client->dev, "client not smb-i2c capable:2\n");
		err = -EIO;
		goto exit_check_functionality_failed;
	}


	if (!i2c_check_functionality(client->adapter,
						I2C_FUNC_SMBUS_I2C_BLOCK)){
		dev_err(&client->dev, "client not smb-i2c capable:3\n");
		err = -EIO;
		goto exit_check_functionality_failed;
	}
	/*
	 * OK. From now, we presume we have a valid client. We now create the
	 * client structure, even though we cannot fill it completely yet.
	 */

	acc = kzalloc(sizeof(struct mxc400x_acc_data), GFP_KERNEL);
	if (acc == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
				"failed to allocate memory for module data: "
					"%d\n", err);
		goto exit_alloc_data_failed;
	}

	mutex_init(&acc->lock);
	mutex_lock(&acc->lock);

	acc->client = client;
    mxc400x_i2c_client = client;
	i2c_set_clientdata(client, acc);

	/* read chip id */
	tempvalue = i2c_smbus_read_word_data(client, WHO_AM_I);

	if ((tempvalue & 0x003F) == WHOAMI_MXC400X_ACC) {
		printk(KERN_INFO "%s I2C driver registered!\n",
							MXC400X_ACC_DEV_NAME);
	} else {
		acc->client = NULL;
		printk(KERN_INFO "I2C driver not registered!"
				" Device unknown 0x%x\n", tempvalue);
		goto err_mutexunlockfreedata;
	}
/*
	acc->pdata = kmalloc(sizeof(*acc->pdata), GFP_KERNEL);
	if (acc->pdata == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
				"failed to allocate memory for pdata: %d\n",
				err);
		goto exit_kfree_pdata;
	}

	memcpy(acc->pdata, client->dev.platform_data, sizeof(*acc->pdata));

	err = mxc400x_acc_validate_pdata(acc);
	if (err < 0) {
		dev_err(&client->dev, "failed to validate platform data\n");
		goto exit_kfree_pdata;
	}

	i2c_set_clientdata(client, acc);


	if (acc->pdata->init) {
		err = acc->pdata->init();
		if (err < 0) {
			dev_err(&client->dev, "init failed: %d\n", err);
			goto err2;
		}
	}
*/
	err = mxc400x_acc_device_power_on(acc);
	if (err < 0) {
		dev_err(&client->dev, "power on failed: %d\n", err);
		goto err2;
	}

	atomic_set(&acc->enabled, 1);

	mxc400x_acc_misc_data = acc;

	err = misc_register(&mxc400x_acc_misc_device);
	if (err < 0) {
		dev_err(&client->dev,
				"misc MXC400X_ACC_DEV_NAME register failed\n");
		goto exit_kfree_pdata;
	}

	mxc400x_acc_device_power_off(acc);

	/* As default, do not report information */
	atomic_set(&acc->enabled, 0);

    acc->on_before_suspend = 0;

 #ifdef CONFIG_HAS_EARLYSUSPEND
    acc->early_suspend.suspend = mxc400x_early_suspend;
    acc->early_suspend.resume  = mxc400x_early_resume;
    acc->early_suspend.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
    register_early_suspend(&acc->early_suspend);
#endif

	mutex_unlock(&acc->lock);

	dev_info(&client->dev, "%s: probed\n", MXC400X_ACC_DEV_NAME);

	return 0;


err2:
	if (acc->pdata->exit) acc->pdata->exit();
exit_kfree_pdata:
	kfree(acc->pdata);
err_mutexunlockfreedata:
	mutex_unlock(&acc->lock);
	i2c_set_clientdata(client, NULL);
	mxc400x_acc_misc_data = NULL;
	kfree(acc);
exit_alloc_data_failed:
exit_check_functionality_failed:
	printk(KERN_ERR "%s: Driver Init failed\n", MXC400X_ACC_DEV_NAME);
	return err;
}

static int mxc400x_acc_remove(struct i2c_client *client)
{
	/* TODO: revisit ordering here once _probe order is finalized */
	struct mxc400x_acc_data *acc = i2c_get_clientdata(client);

	misc_deregister(&mxc400x_acc_misc_device);
	mxc400x_acc_device_power_off(acc);
	if (acc->pdata->exit)
		acc->pdata->exit();
	kfree(acc->pdata);
	kfree(acc);

	return 0;
}

static int mxc400x_acc_resume(struct i2c_client *client)
{
	struct mxc400x_acc_data *acc = i2c_get_clientdata(client);
#ifdef MXC400X_DEBUG
    printk("%s.\n", __func__);
#endif

	if (acc != NULL && acc->on_before_suspend) {
        acc->on_before_suspend = 0;
		return mxc400x_acc_enable(acc);
    }

	return 0;
}

static int mxc400x_acc_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct mxc400x_acc_data *acc = i2c_get_clientdata(client);
#ifdef MXC400X_DEBUG
    printk("%s.\n", __func__);
#endif
    if (acc != NULL) {
        if (atomic_read(&acc->enabled)) {
            acc->on_before_suspend = 1;
            return mxc400x_acc_disable(acc);
        }
    }
    return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND

static void mxc400x_early_suspend (struct early_suspend* es)
{
#ifdef MXC400X_DEBUG
    printk("%s.\n", __func__);
#endif
    mxc400x_acc_suspend(mxc400x_i2c_client,
         (pm_message_t){.event=0});
}

static void mxc400x_early_resume (struct early_suspend* es)
{
#ifdef MXC400X_DEBUG
    printk("%s.\n", __func__);
#endif
    mxc400x_acc_resume(mxc400x_i2c_client);
}

#endif /* CONFIG_HAS_EARLYSUSPEND */

static const struct i2c_device_id mxc400x_acc_id[]
				= { { MXC400X_ACC_DEV_NAME, 0 }, { }, };

MODULE_DEVICE_TABLE(i2c, mxc400x_acc_id);

static struct i2c_driver mxc400x_acc_driver = {
	.driver = {
			//.name = MXC400X_ACC_I2C_NAME,
			.name = "memsic,mxc400x",
			.of_match_table = memsic_match_table,
		  },
	.probe = mxc400x_acc_probe,
	.remove = mxc400x_acc_remove,
	.resume = mxc400x_acc_resume,
	.suspend = mxc400x_acc_suspend,
	.id_table = mxc400x_acc_id,
};


#ifdef I2C_BUS_NUM_STATIC_ALLOC

int i2c_static_add_device(struct i2c_board_info *info)
{
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	int err;

	adapter = i2c_get_adapter(I2C_STATIC_BUS_NUM);
	if (!adapter) {
		pr_err("%s: can't get i2c adapter\n", __FUNCTION__);
		err = -ENODEV;
		goto i2c_err;
	}

	client = i2c_new_device(adapter, info);
	if (!client) {
		pr_err("%s:  can't add i2c device at 0x%x\n",
			__FUNCTION__, (unsigned int)info->addr);
		err = -ENODEV;
		goto i2c_err;
	}

	i2c_put_adapter(adapter);

	return 0;

i2c_err:
	return err;
}

#endif /*I2C_BUS_NUM_STATIC_ALLOC*/

static int __init mxc400x_acc_init(void)
{
        int  ret = 0;

	printk(KERN_INFO "%s accelerometer driver: init\n",
						MXC400X_ACC_I2C_NAME);
#ifdef I2C_BUS_NUM_STATIC_ALLOC
        ret = i2c_static_add_device(&mxc400x_i2c_boardinfo);
        if (ret < 0) {
            pr_err("%s: add i2c device error %d\n", __FUNCTION__, ret);
            goto init_err;
        }
#endif
        ret = i2c_add_driver(&mxc400x_acc_driver);
        printk(KERN_INFO "%s add driver: %d\n",
						MXC400X_ACC_I2C_NAME,ret);
        return ret;
#ifdef I2C_BUS_NUM_STATIC_ALLOC
init_err:
        return ret;
#endif
}

static void __exit mxc400x_acc_exit(void)
{
	printk(KERN_INFO "%s accelerometer driver exit\n", MXC400X_ACC_DEV_NAME);

	#ifdef I2C_BUS_NUM_STATIC_ALLOC
        i2c_unregister_device(mxc400x_i2c_client);
	#endif

	i2c_del_driver(&mxc400x_acc_driver);
	return;
}

module_init(mxc400x_acc_init);
module_exit(mxc400x_acc_exit);


MODULE_AUTHOR("Robbie Cao<hjcao@memsic.com>");
MODULE_DESCRIPTION("MEMSIC MXC400X (DTOS) Accelerometer Sensor Driver");
MODULE_LICENSE("GPL");
