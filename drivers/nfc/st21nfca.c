/*
 * Copyright (C) 2010 Stollmann E+V GmbH
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/platform_data/st21nfca.h>

#define MAX_BUFFER_SIZE	256

/* define the active state of the WAKEUP pin */
#define ST21_IRQ_ACTIVE_HIGH 1
#define ST21_IRQ_ACTIVE_LOW 0


/* prototypes */
static irqreturn_t st21nfca_dev_irq_handler(int irq, void *dev_id);
/*
   The platform data member 'polarity_mode' defines how the wakeup
   pin is configured and handled, it can take the following values:
   IRQF_TRIGGER_RISING
   IRQF_TRIGGER_FALLING
   IRQF_TRIGGER_HIGH
   IRQF_TRIGGER_LOW
*/

struct st21nfca_dev {
	wait_queue_head_t	read_wq;
	struct mutex		read_mutex;
	struct i2c_client	*client;
	struct miscdevice	st21nfca_device;
	unsigned int	irq_gpio;
	unsigned int	reset_gpio;
	unsigned int	ena_gpio;
	unsigned int	polarity_mode;
	/* active_polarity: either 0 (low-active) or 1 (high-active) */
	unsigned int	active_polarity;
	bool			irq_enabled;
	spinlock_t		irq_enabled_lock;
};

static int st21nfca_loc_set_polaritymode(struct st21nfca_dev *st21nfca_dev,
				int mode)
{
	struct i2c_client *client = st21nfca_dev->client;
	static bool irqIsAttached;
	unsigned int irq_type;
	int ret;
	st21nfca_dev->polarity_mode = mode;
	/* setup irq_flags */
	switch (mode) {
	case IRQF_TRIGGER_RISING:
		irq_type = IRQ_TYPE_EDGE_RISING;
		st21nfca_dev->active_polarity = 1;
		break;
	case IRQF_TRIGGER_FALLING:
		irq_type = IRQ_TYPE_EDGE_FALLING;
		st21nfca_dev->active_polarity = 0;
		break;
	case IRQF_TRIGGER_HIGH:
		irq_type = IRQ_TYPE_LEVEL_HIGH;
		st21nfca_dev->active_polarity = 1;
		break;
	case IRQF_TRIGGER_LOW:
		irq_type = IRQ_TYPE_LEVEL_LOW;
		st21nfca_dev->active_polarity = 0;
		break;
	default:
		irq_type = IRQF_TRIGGER_FALLING;
		st21nfca_dev->active_polarity = 0;
		break;
	}
	if (irqIsAttached) {
		free_irq(client->irq, st21nfca_dev);
		irqIsAttached = false;
	}
	ret = irq_set_irq_type(client->irq, irq_type);
	if (ret) {
		pr_err("%s : set_irq_type failed\n", __FILE__);
		return -ENODEV;
	}

	/* request irq.  the irq is set whenever the chip has data available
	 * for reading.  it is cleared when all data has been read.
	 */
	pr_debug("%s : requesting IRQ %d\n", __func__, client->irq);
	st21nfca_dev->irq_enabled = true;

	ret = request_irq(client->irq, st21nfca_dev_irq_handler,
						st21nfca_dev->polarity_mode,
						client->name, st21nfca_dev);
	if (!ret)
		irqIsAttached = true;

	return ret;
}

static void st21nfca_disable_irq(struct st21nfca_dev *st21nfca_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&st21nfca_dev->irq_enabled_lock, flags);
	if (st21nfca_dev->irq_enabled) {
		disable_irq_nosync(st21nfca_dev->client->irq);
		st21nfca_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&st21nfca_dev->irq_enabled_lock, flags);
}

static irqreturn_t st21nfca_dev_irq_handler(int irq, void *dev_id)
{
	struct st21nfca_dev *st21nfca_dev = dev_id;

	st21nfca_disable_irq(st21nfca_dev);

#if 0
	pr_info("%s ####$$$$$ H I T	 M E	 $$$$$$#####\n", __func__);
	gpio_set_value(st21nfca_dev->ena_gpio, 0);
	udelay(5);
	gpio_set_value(st21nfca_dev->ena_gpio, 1);
#endif

	/* Wake up waiting readers */
	wake_up(&st21nfca_dev->read_wq);
	return IRQ_HANDLED;
}

static ssize_t st21nfca_dev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	struct st21nfca_dev *st21nfca_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE];
	int ret;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	pr_debug("%s : reading %zu bytes.\n", __func__, count);

	mutex_lock(&st21nfca_dev->read_mutex);

	/* Read data */
	ret = i2c_master_recv(st21nfca_dev->client, tmp, count);
	mutex_unlock(&st21nfca_dev->read_mutex);

	if (ret < 0) {
		pr_err("%s: i2c_master_recv returned %d\n", __func__, ret);
		return ret;
	}
	if (ret > count) {
		pr_err("%s: received too many bytes from i2c (%d)\n",
				__func__, ret);
		return -EIO;
	}
	if (copy_to_user(buf, tmp, ret)) {
		pr_warn("%s : failed to copy to user space\n", __func__);
		return -EFAULT;
	}
	return ret;
}

static ssize_t st21nfca_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	struct st21nfca_dev  *st21nfca_dev;
	char tmp[MAX_BUFFER_SIZE];
	int ret = count;

	st21nfca_dev = filp->private_data;
	pr_debug("%s: st21nfca_dev ptr %p\n", __func__, st21nfca_dev);

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	if (copy_from_user(tmp, buf, count)) {
		pr_err("%s : failed to copy from user space\n", __func__);
		return -EFAULT;
	}

	pr_debug("%s : writing %zu bytes.\n", __func__, count);
	/* Write data */
	ret = i2c_master_send(st21nfca_dev->client, tmp, count);
	if (ret != count) {
		pr_err("%s : i2c_master_send returned %d\n", __func__, ret);
		ret = -EIO;
	}
	return ret;
}

static int st21nfca_dev_open(struct inode *inode, struct file *filp)
{
	struct st21nfca_dev *st21nfca_dev =
				container_of(filp->private_data,
						struct st21nfca_dev,
						st21nfca_device);

	filp->private_data = st21nfca_dev;
	pr_debug("%s : %d,%d\n", __func__, imajor(inode), iminor(inode));
	pr_debug("%s: st21nfca_dev ptr %p\n", __func__, st21nfca_dev);
	return 0;
}


static long st21nfca_dev_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	struct st21nfca_dev *st21nfca_dev = filp->private_data;

	int ret = 0;

	switch (cmd) {
	case ST21NFCA_SET_POLARITY_FALLING:
		pr_info(" ### ST21NFCA_SET_POLARITY_FALLING ###");
		st21nfca_loc_set_polaritymode(st21nfca_dev,
				IRQF_TRIGGER_FALLING);
		break;

	case ST21NFCA_SET_POLARITY_RISING:
		pr_info(" ### ST21NFCA_SET_POLARITY_RISING ###");
		st21nfca_loc_set_polaritymode(st21nfca_dev,
				IRQF_TRIGGER_RISING);
		break;

	case ST21NFCA_SET_POLARITY_LOW:
		pr_info(" ### ST21NFCA_SET_POLARITY_LOW ###");
		st21nfca_loc_set_polaritymode(st21nfca_dev,
				IRQF_TRIGGER_LOW);
		break;

	case ST21NFCA_SET_POLARITY_HIGH:
		pr_info(" ### ST21NFCA_SET_POLARITY_HIGH ###");
		st21nfca_loc_set_polaritymode(st21nfca_dev,
				IRQF_TRIGGER_HIGH);
		break;

	case ST21NFCA_PULSE_RESET:
		pr_info("%s Pulse Request\n", __func__);
		if (st21nfca_dev->reset_gpio != 0) {
			/* pulse low for 20 millisecs */
			gpio_set_value(st21nfca_dev->reset_gpio, 0);
			msleep(20);
			gpio_set_value(st21nfca_dev->reset_gpio, 1);
			pr_info("%s done Pulse Request\n", __func__);
		}
		break;

	case ST21NFCA_GET_WAKEUP:
		/* deliver state of Wake_up_pin as return value of ioctl */
		ret = gpio_get_value(st21nfca_dev->irq_gpio);
		if (ret >= 0)
			ret = (ret == st21nfca_dev->active_polarity);
		pr_debug("%s get gpio result %d\n", __func__, ret);
		break;
	default:
		pr_err("%s bad ioctl %u\n", __func__, cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static unsigned int st21nfca_poll(struct file *file, poll_table *wait)
{
	struct st21nfca_dev *st21nfca_dev = file->private_data;
	unsigned int mask = 0;

	/* wait for Wake_up_pin == high  */
	pr_debug("%s call poll_Wait\n", __func__);
	poll_wait(file, &st21nfca_dev->read_wq, wait);

	if (st21nfca_dev->active_polarity ==
		gpio_get_value(st21nfca_dev->irq_gpio)) {
		pr_debug("%s return ready\n", __func__);
		mask = POLLIN | POLLRDNORM; /* signal data avail */
		st21nfca_disable_irq(st21nfca_dev);
	} else {
		/* Wake_up_pin  is low. Activate ISR  */
		if (!st21nfca_dev->irq_enabled) {
			pr_debug("%s enable irq\n", __func__);
			st21nfca_dev->irq_enabled = true;
			enable_irq(st21nfca_dev->client->irq);
		} else {
			pr_debug("%s irq already enabled\n", __func__);
		}

	}
	return mask;
}

static const struct file_operations st21nfca_dev_fops = {
	.owner	= THIS_MODULE,
	.llseek	= no_llseek,
	.read	= st21nfca_dev_read,
	.write	= st21nfca_dev_write,
	.open	= st21nfca_dev_open,
	.poll	= st21nfca_poll,

	.unlocked_ioctl = st21nfca_dev_ioctl,
};

#ifdef CONFIG_OF
static int st21nfca_probe_dt(struct i2c_client *client)
{
	struct st21nfca_i2c_platform_data *platform_data;
	struct device_node *np = client->dev.of_node;

	platform_data = kzalloc(sizeof(*platform_data), GFP_KERNEL);
	if (platform_data == NULL) {
		pr_err("Alloc GFP_KERNEL memory failed.");
		return -ENOMEM;
	}

	client->dev.platform_data = platform_data;
	platform_data->irq_gpio = of_get_named_gpio(np, "irq-gpios", 0);
	if (platform_data->irq_gpio < 0) {
		pr_err("of_get_named_gpio irq failed.");
		return -EINVAL;
	}

	platform_data->ena_gpio = of_get_named_gpio(np, "ena-gpios", 0);
	if (platform_data->ena_gpio < 0)
		platform_data->ena_gpio = 0;

	platform_data->reset_gpio = of_get_named_gpio(np, "reset-gpios", 0);
	if (platform_data->reset_gpio < 0)
		platform_data->reset_gpio = 0;

	if (of_property_read_u32(np, "polarity_mode",
			 &platform_data->polarity_mode)) {
		pr_err("failed to get st21nfca,polarity_mode\n");
		return -EINVAL;
	}

	return 0;
}
#endif

static int st21nfca_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret;
	struct st21nfca_i2c_platform_data *platform_data;
	struct st21nfca_dev *st21nfca_dev;

	pr_debug("%s : clientptr %p\n", __func__, client);

#ifdef CONFIG_OF
	ret = st21nfca_probe_dt(client);
	if (ret == -EINVAL) {
		pr_err("Probe device tree data failed.");
		kfree(client->dev.platform_data);
		client->dev.platform_data = NULL;
		return ret;
	}
#endif

	platform_data = client->dev.platform_data;

	if (platform_data == NULL) {
		pr_err("%s : st21nfca probe fail\n", __func__);
		return  -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s : need I2C_FUNC_I2C\n", __func__);
		ret =  -ENODEV;
		goto err_exit;
	}

	client->adapter->timeout = msecs_to_jiffies(3 * 10); /* 30ms */
	client->adapter->retries = 0;

	st21nfca_dev = kzalloc(sizeof(*st21nfca_dev), GFP_KERNEL);
	if (st21nfca_dev == NULL) {
		dev_err(&client->dev,
				"failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}
	pr_debug("%s : dev_cb_addr %p\n", __func__, st21nfca_dev);

	/* store for later use */
	st21nfca_dev->irq_gpio		= platform_data->irq_gpio;
	st21nfca_dev->ena_gpio		= platform_data->ena_gpio;
	st21nfca_dev->reset_gpio	= platform_data->reset_gpio;
	st21nfca_dev->polarity_mode	= platform_data->polarity_mode;
	st21nfca_dev->client		= client;

	ret = gpio_request(platform_data->irq_gpio, "st21nfca");
	if (ret) {
		pr_err("%s : gpio_request failed\n", __FILE__);
		ret = -ENODEV;
		goto err_irq_gpio;
	}

	ret = gpio_direction_input(platform_data->irq_gpio);
	if (ret) {
		pr_err("%s : gpio_direction_input failed\n", __FILE__);
		ret = -ENODEV;
		goto err_irq_gpio;
	}

	/* handle optional RESET */
	if (platform_data->reset_gpio != 0) {
		ret = gpio_request(platform_data->reset_gpio,
				"st21nfca_reset");
		if (ret) {
			pr_err("%s : reset gpio_request failed\n",
					__FILE__);
			ret = -ENODEV;
			goto err_reset_gpio;
		}
		ret = gpio_direction_output(platform_data->reset_gpio, 1);
		if (ret) {
			pr_err("%s : reset gpio_direction_output failed\n",
					__FILE__);
			ret = -ENODEV;
			goto err_reset_gpio;
		}

		/* low active */
		gpio_set_value(st21nfca_dev->reset_gpio, 1);
	}

	if (platform_data->ena_gpio != 0) {
		ret = gpio_request(platform_data->ena_gpio, "st21nfca_ena");
		if (ret) {
			pr_err("%s : ena gpio_request failed\n", __FILE__);
			ret = -ENODEV;
			goto err_ena_gpio;
		}
		ret = gpio_direction_output(platform_data->ena_gpio, 1);
		if (ret) {
			pr_err("%s : ena gpio_direction_output failed\n",
					__FILE__);
			ret = -ENODEV;
			goto err_ena_gpio;
		}
	}
	client->irq = gpio_to_irq(platform_data->irq_gpio);

	enable_irq_wake(client->irq);
	/* init mutex and queues */
	init_waitqueue_head(&st21nfca_dev->read_wq);
	mutex_init(&st21nfca_dev->read_mutex);
	spin_lock_init(&st21nfca_dev->irq_enabled_lock);

	st21nfca_dev->st21nfca_device.minor	= MISC_DYNAMIC_MINOR;
	st21nfca_dev->st21nfca_device.name	= "st21nfca";
	st21nfca_dev->st21nfca_device.fops	= &st21nfca_dev_fops;


	ret = misc_register(&st21nfca_dev->st21nfca_device);
	if (ret) {
		pr_err("%s : misc_register failed\n", __FILE__);
		goto err_misc_register;
	}

#if 0
	ret = st21nfca_loc_set_polaritymode(st21nfca_dev,
			platform_data->polarity_mode);
	if (ret) {
		dev_err(&client->dev, "request_irq failed\n");
		goto err_request_irq_failed;
	}
#endif

	st21nfca_disable_irq(st21nfca_dev);
	i2c_set_clientdata(client, st21nfca_dev);
	return 0;

#if 0
err_request_irq_failed:
	misc_deregister(&st21nfca_dev->st21nfca_device);
#endif

err_misc_register:
	mutex_destroy(&st21nfca_dev->read_mutex);
err_ena_gpio:
	if (platform_data->ena_gpio != 0)
		gpio_free(platform_data->ena_gpio);
err_reset_gpio:
	if (platform_data->reset_gpio != 0)
		gpio_free(platform_data->reset_gpio);
err_irq_gpio:
	gpio_free(platform_data->irq_gpio);

	kfree(st21nfca_dev);
err_exit:
#ifdef CONFIG_OF
	kfree(platform_data);
#endif

	return ret;
}

static int st21nfca_remove(struct i2c_client *client)
{
	struct st21nfca_dev *st21nfca_dev;

	st21nfca_dev = i2c_get_clientdata(client);
	free_irq(client->irq, st21nfca_dev);
	misc_deregister(&st21nfca_dev->st21nfca_device);
	mutex_destroy(&st21nfca_dev->read_mutex);
	gpio_free(st21nfca_dev->irq_gpio);
	if (st21nfca_dev->ena_gpio != 0)
		gpio_free(st21nfca_dev->ena_gpio);
	kfree(st21nfca_dev);

	return 0;
}

static struct of_device_id st21nfca_ts_dt_ids[] = {
	{ .compatible = "st,st21nfca", },
	{}
};

static const struct i2c_device_id st21nfca_id[] = {
	{"st21nfca", 0},
	{ }
};

static int st21nfca_suspend(struct i2c_client *client,
							pm_message_t mesg)
{
	struct st21nfca_dev *st21nfca_dev;
	if (client) {
		st21nfca_dev = i2c_get_clientdata(client);
		if (st21nfca_dev && st21nfca_dev->ena_gpio != 0) {
			gpio_set_value(st21nfca_dev->ena_gpio, 1); /* ON */
			return 0;
		}
		pr_debug("%s : failing no st21 context %p !!!\n",
				 __func__, st21nfca_dev);
	}
	pr_debug("%s : failing no client context  %p!!!\n",
			 __func__, client);
	return 0; /* silent fail */
}

static int st21nfca_resume(struct i2c_client *client)
{
	struct st21nfca_dev *st21nfca_dev;
	if (client) {
		st21nfca_dev = i2c_get_clientdata(client);
		if (st21nfca_dev && st21nfca_dev->ena_gpio != 0) {
			gpio_set_value(st21nfca_dev->ena_gpio, 1); /* ON */
			return 0;
		}
		pr_debug("%s : failing no st21 context %p !!!\n",
				 __func__, st21nfca_dev);
	}
	pr_debug("%s : failing no context %p!!!\n", __func__, client);
	return 0; /* silent fail */
}

static struct i2c_driver st21nfca_driver = {
	.id_table	= st21nfca_id,
	.probe		= st21nfca_probe,
	.remove		= st21nfca_remove,
	.suspend	= st21nfca_suspend,
	.resume		= st21nfca_resume,
	.driver		= {
		.owner = THIS_MODULE,
		.name  = "st21nfca",
		.of_match_table = of_match_ptr(st21nfca_ts_dt_ids),
	},
};

/*
 * module load/unload record keeping
 */
static int __init st21nfca_dev_init(void)
{
	pr_debug("Loading st21nfca driver\n");
	return i2c_add_driver(&st21nfca_driver);
}

module_init(st21nfca_dev_init);

static void __exit st21nfca_dev_exit(void)
{
	pr_debug("Unloading st21nfca driver\n");
	i2c_del_driver(&st21nfca_driver);
}
module_exit(st21nfca_dev_exit);

MODULE_AUTHOR("Norbert Kawulski");
MODULE_DESCRIPTION("NFC ST21NFCA driver");
MODULE_LICENSE("GPL");
