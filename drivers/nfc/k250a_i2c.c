/*
 * Copyright (C) 2019 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program;
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/ioctl.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/pinctrl.h>
#ifdef FEATURE_ESE_WAKELOCK
#include <linux/pm_wakeup.h>
#endif
#include "k250a_i2c.h"

#define K250A_ERR_MSG(msg...) pr_err("[K250A] : " msg)
#define K250A_INFO_MSG(msg...) pr_info("[K250A] : " msg)

static DEFINE_MUTEX(device_list_lock);

/* Device specific macro and structure */
struct k250a_dev {
	struct mutex buffer_mutex; /* buffer mutex */
	struct i2c_client *client;  /* i2c device structure */
	struct miscdevice k250a_device; /* char device as misc driver */

	unsigned int users;

	bool device_opened;
#ifdef FEATURE_SNVM_WAKELOCK
	struct wakeup_source ese_lock;
#endif
	struct regulator *vdd;
	unsigned char *buf;

	struct pinctrl *pinctrl;
	struct pinctrl_state *nvm_on_pin;
	struct pinctrl_state *nvm_off_pin;

};

static int k250a_regulator_onoff(struct k250a_dev *kdev, int onoff)
{
	int ret = 0;

	K250A_INFO_MSG("%s - onoff = %d\n", __func__, onoff);

	if (onoff == K250A_ON) {
		ret = regulator_enable(kdev->vdd);
		if (ret) {
			K250A_ERR_MSG("%s - enable vdd failed, ret=%d\n",
				__func__, ret);
		}
	} else {
		ret = regulator_disable(kdev->vdd);
		if (ret) {
			K250A_ERR_MSG("%s - disable vdd failed, ret=%d\n",
				__func__, ret);
		}
	}
	return ret;
}

static int k250a_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct k250a_dev *k250a_dev = container_of(filp->private_data,
			struct k250a_dev, k250a_device);

	/* for defence MULTI-OPEN */
	if (k250a_dev->device_opened) {
		K250A_ERR_MSG("%s - ALREADY opened!\n", __func__);
		return -EBUSY;
	}

	mutex_lock(&device_list_lock);
	k250a_dev->device_opened = true;
	K250A_INFO_MSG("open\n");

#ifdef FEATURE_SNVM_WAKELOCK
	__pm_stay_awake(&k250a_dev->ese_lock);
	K250A_INFO_MSG("called to __pm_stay_awake");
#endif

	if (k250a_dev->nvm_on_pin) {
		if (pinctrl_select_state(k250a_dev->pinctrl, k250a_dev->nvm_on_pin))
			K250A_ERR_MSG("nvm on pinctrl set error\n");
		else
			K250A_INFO_MSG("nvm on pinctrl set\n");
	}

	ret = k250a_regulator_onoff(k250a_dev, K250A_ON);
	if (ret < 0)
		K250A_ERR_MSG(" %s : failed to turn on LDO()\n", __func__);

	filp->private_data = k250a_dev;

	k250a_dev->users++;
	mutex_unlock(&device_list_lock);

	return 0;
}

static int k250a_close(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct k250a_dev *k250a_dev = filp->private_data;

	if (!k250a_dev->device_opened) {
		K250A_ERR_MSG("%s - was NOT opened....\n", __func__);
		return 0;
	}

	mutex_lock(&device_list_lock);

#ifdef FEATURE_SNVM_WAKELOCK
	if (k250a_dev->ese_lock.active) {
		__pm_relax(&k250a_dev->ese_lock);
		K250A_INFO_MSG("called to __pm_relax");
	}
#endif

	filp->private_data = k250a_dev;

	k250a_dev->users--;

	if (!k250a_dev->users) {
		k250a_dev->device_opened = false;

		ret = k250a_regulator_onoff(k250a_dev, K250A_OFF);
		if (ret < 0)
			K250A_ERR_MSG("failed to turn off LDO()\n");
	}
	if (k250a_dev->nvm_off_pin) {
		if (pinctrl_select_state(k250a_dev->pinctrl, k250a_dev->nvm_off_pin))
			K250A_ERR_MSG("nvm off pinctrl set error\n");
		else
			K250A_INFO_MSG("nvm off pinctrl set\n");
	}

	mutex_unlock(&device_list_lock);

	K250A_INFO_MSG("%s, users:%d, Major Minor No:%d %d\n", __func__,
			k250a_dev->users, imajor(inode), iminor(inode));
	return 0;
}

static long k250a_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	int ret = 0;
	struct k250a_dev *k250a_dev = NULL;

	k250a_dev = filp->private_data;

	mutex_lock(&k250a_dev->buffer_mutex);

	switch (cmd) {

		case K250A_SET_TIMEOUT:
			K250A_INFO_MSG("%s timeout  %ld\n", __func__, arg);
			k250a_dev->client->adapter->timeout = msecs_to_jiffies(arg);
			break;

		case K250A_SET_RESET:
			K250A_INFO_MSG("%s requested to reset", __func__);
			ret = k250a_regulator_onoff(k250a_dev, K250A_OFF);
			if (ret < 0)
				K250A_ERR_MSG(" test: failed to turn off LDO()\n");

			usleep_range(2000, 2500);
			ret = k250a_regulator_onoff(k250a_dev, K250A_ON);
			if (ret < 0)
				K250A_ERR_MSG(" test: failed to turn off LDO()\n");
			break;

		case K250A_GET_PWR_STATUS:
			K250A_INFO_MSG("%s requested to get pwr status : %d", __func__,
					k250a_dev->device_opened);
			put_user(k250a_dev->device_opened, (int __user *)arg);
			break;

		default:
			K250A_INFO_MSG("%s no matching ioctl! 0x%X\n", __func__, cmd);
			ret = -EINVAL;
	}

	mutex_unlock(&k250a_dev->buffer_mutex);
	return ret;
}


static ssize_t k250a_write(struct file *filp, const char *buf, size_t count,
		loff_t *offset)
{
	int ret = -1;
	struct k250a_dev *k250a_dev;

	k250a_dev = filp->private_data;

	if (count > MAX_BUFFER_SIZE)
		return -EFAULT;

	mutex_lock(&k250a_dev->buffer_mutex);

	if (copy_from_user(k250a_dev->buf, &buf[0], count)) {
		K250A_ERR_MSG("%s: failed to copy from user space\n", __func__);
		mutex_unlock(&k250a_dev->buffer_mutex);
		return -EFAULT;
	}

	/* Write data */
	ret = i2c_master_send(k250a_dev->client, k250a_dev->buf, count);
	if (ret < 0) {
		K250A_ERR_MSG("i2c_master_send failed\n");
		ret = -EIO;
	}
	else
		ret = count;

	mutex_unlock(&k250a_dev->buffer_mutex);
	K250A_INFO_MSG("%s: count:%zu ret:%d\n", __func__, count, ret);

	return ret;
}

static ssize_t k250a_read(struct file *filp, char *buf, size_t count,
		loff_t *offset)
{
	int ret = -EIO;
	struct k250a_dev *k250a_dev = filp->private_data;

	if (count > MAX_BUFFER_SIZE)
		return -EFAULT;

	mutex_lock(&k250a_dev->buffer_mutex);

	/* Read the available data along with one byte LRC */
	ret = i2c_master_recv(k250a_dev->client, (void *)k250a_dev->buf, count);
	if (ret < 0) {
		K250A_ERR_MSG("i2c_master_recv failed\n");
		ret = -EIO;
		goto fail;
	}

	if (copy_to_user(buf, k250a_dev->buf, count)) {
		K250A_ERR_MSG("%s : failed to copy to user space\n", __func__);
		ret = -EFAULT;
		goto fail;
	}
	K250A_INFO_MSG("%s: count:%zu, ret:%d\n", __func__, count, ret);
	ret = count;

	mutex_unlock(&k250a_dev->buffer_mutex);

	return ret;

fail:
	K250A_ERR_MSG("Error %s ret %d Exit\n", __func__, ret);
	mutex_unlock(&k250a_dev->buffer_mutex);
	return ret;
}

static const struct file_operations k250a_dev_fops = {
	.owner = THIS_MODULE,
	.read = k250a_read,
	.write = k250a_write,
	.open = k250a_open,
	.release = k250a_close,
	.unlocked_ioctl = k250a_ioctl,
};

static int k250a_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = -1;
	struct k250a_dev *k250a_dev;
	struct device_node *i2c_device_node;
	struct platform_device *i2c_pdev;

	if (client->dev.of_node) {
		k250a_dev = devm_kzalloc(&client->dev,
				sizeof(struct k250a_dev), GFP_KERNEL);
		if (k250a_dev == NULL) {
			K250A_ERR_MSG("failed to allocate memory for module data\n");
			ret = -ENOMEM;
			return ret;
		}

		k250a_dev->vdd = devm_regulator_get(&client->dev, "1p8_pvdd");
		if (IS_ERR(k250a_dev->vdd)) {
			ret = PTR_ERR(k250a_dev->vdd);
			K250A_ERR_MSG("%s - Failed to get 1p8_pvdd\n", __func__);
			return ret;
		}
	} else {
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		K250A_ERR_MSG("need I2C_FUNC_I2C\n");
		return -ENODEV;
	}

	k250a_dev->client = client;
	k250a_dev->k250a_device.minor = MISC_DYNAMIC_MINOR;
	k250a_dev->k250a_device.name = "k250a";
	k250a_dev->k250a_device.fops = &k250a_dev_fops;

	/* init mutex */
	mutex_init(&k250a_dev->buffer_mutex);
#ifdef FEATURE_SNVM_WAKELOCK
	wakeup_source_init(&k250a_dev->ese_lock, "snvm_wake_lock");
#endif

	k250a_dev->device_opened = false;

	ret = misc_register(&k250a_dev->k250a_device);
	if (ret < 0) {
		pr_info("misc_register failed! %d\n", ret);
		goto err_misc_regi;
	}

	k250a_dev->buf = devm_kzalloc(&client->dev,
			sizeof(unsigned char) * MAX_BUFFER_SIZE, GFP_KERNEL);
	if (k250a_dev->buf == NULL) {
		K250A_ERR_MSG("failed to allocate for i2c buf\n");
		ret = -ENOMEM;
		goto err_buffer_malloc;
	}

	i2c_device_node = of_parse_phandle(client->dev.of_node, "k250a-i2c_node", 0);
	if (!IS_ERR_OR_NULL(i2c_device_node)) {
		i2c_pdev = of_find_device_by_node(i2c_device_node);
		
		
		k250a_dev->pinctrl = devm_pinctrl_get(&i2c_pdev->dev);
		if (IS_ERR(k250a_dev->pinctrl))
			K250A_ERR_MSG("devm_pinctrl_get failed\n");
		
		k250a_dev->nvm_on_pin = pinctrl_lookup_state(k250a_dev->pinctrl, "default");
		if (IS_ERR(k250a_dev->nvm_on_pin)) {
			K250A_ERR_MSG("pinctrl_lookup_state failed-default\n");
			k250a_dev->nvm_on_pin = NULL;
		}
		k250a_dev->nvm_off_pin = pinctrl_lookup_state(k250a_dev->pinctrl, "nvm_off");
		if (IS_ERR(k250a_dev->nvm_off_pin)) {
			K250A_ERR_MSG("pinctrl_lookup_state failed-nvm_off\n");
			k250a_dev->nvm_off_pin = NULL;
		} else if (pinctrl_select_state(k250a_dev->pinctrl, k250a_dev->nvm_off_pin))
			K250A_ERR_MSG("nvm off pinctrl set error\n");
		else
			K250A_INFO_MSG("nvm off pinctrl set\n");
	} else {
		pr_info("%s failed to get k250a-i2c_node\n", __func__);
	}
	pr_info("%s finished...\n", __func__);
	return ret;

err_buffer_malloc:
	misc_deregister(&k250a_dev->k250a_device);
err_misc_regi:
#ifdef FEATURE_SNVM_WAKELOCK
	wakeup_source_trash(&k250a_dev->ese_lock);
#endif
	mutex_destroy(&k250a_dev->buffer_mutex);
	devm_kfree(&client->dev, k250a_dev->buf);
	devm_kfree(&client->dev, k250a_dev);
	pr_info("ERROR: Exit : %s ret %d\n", __func__, ret);
	return ret;
}

static int k250a_remove(struct i2c_client *client)
{
	struct k250a_dev *k250a_dev;

	K250A_INFO_MSG("Entry : %s\n", __func__);
	k250a_dev = i2c_get_clientdata(client);
	if (k250a_dev == NULL) {
		K250A_ERR_MSG("%s k250a_dev is null!\n", __func__);
		return 0;
	}

#ifdef FEATURE_SNVM_WAKELOCK
	wake_lock_destroy(&k250a_dev->ese_lock);
#endif
	mutex_destroy(&(k250a_dev->buffer_mutex));
	misc_deregister(&(k250a_dev->k250a_device));
	devm_kfree(&client->dev, k250a_dev->buf);
	devm_kfree(&client->dev, k250a_dev);
	K250A_INFO_MSG("Exit : %s\n", __func__);
	return 0;
}

static const struct i2c_device_id k250a_id[] = {
	{"k250a", 0},
	{}
};

static const struct of_device_id k250a_match_table[] = {
	{ .compatible = "snvm_k250a",},
	{},
};

static struct i2c_driver k250a_driver = {
	.id_table = k250a_id,
	.probe = k250a_probe,
	.remove = k250a_remove,
	.driver = {
		.name = "k250a",
		.owner = THIS_MODULE,
		.of_match_table = k250a_match_table,
	},
};

static int __init k250a_dev_init(void)
{
	K250A_INFO_MSG("Entry : %s\n", __func__);
	return i2c_add_driver(&k250a_driver);
}
module_init(k250a_dev_init);

static void __exit k250a_dev_exit(void)
{
	K250A_INFO_MSG("Entry : %s\n", __func__);
	i2c_del_driver(&k250a_driver);
}

module_exit(k250a_dev_exit);

MODULE_AUTHOR("Sec");
MODULE_DESCRIPTION("snvm i2c driver");
MODULE_LICENSE("GPL");
