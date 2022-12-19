/*
 * peelir.c -- simple synchronous userspace interface to SPI devices
 *
 * Copyright (C) 2014 Mistral Solutions Private Limited
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/gpio.h>

#include <linux/spi/spi.h>
#include <linux/sec_sysfs.h>
#include <linux/uaccess.h>

#ifdef CONFIG_OF
#include <linux/of_device.h>
#endif

#include "irled-common.h"
#include "peelir.h"

/* #define SUPPORT_READ_MSG */

#define N_SPI_MINORS	32	/* ... up to 256 */
static DECLARE_BITMAP(minors, N_SPI_MINORS);

#define SPI_MODE_MASK		(SPI_CPHA | SPI_CPOL | SPI_CS_HIGH \
				| SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP \
				| SPI_NO_CS | SPI_READY)

#define TRANS_BUFF	40960
#define NUM_BUFF	14
#define MAX_BUFF	(NUM_BUFF*TRANS_BUFF)

u8 t_buff[MAX_BUFF], rx_snd_buff[MAX_BUFF];

struct peelir_data {
	dev_t			devt;
	struct spi_device	*spi;
	struct device		*sec_device;
	struct list_head	device_entry;
	struct mutex		buf_lock;
	spinlock_t		spi_lock;
	unsigned		users;
	u8			*buffer;
	struct irled_power	irled_power;
};

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static int peelir_major;
static unsigned bufsiz = 170 * 1024;		/* Default buffer size */
static int mode = 0, bpw = 32, spi_clk_freq = 1520000;
static int prev_tx_status;			/* Status of previous transaction */
static unsigned int field;

#if defined(SUPPORT_READ_MSG)
static int code_rcv;
static int flag, byte_delay = 140;
#endif

static void peelir_complete(void *arg)
{
	complete(arg);
}

static ssize_t
peelir_sync(struct peelir_data *peelir, struct spi_message *message)
{
	DECLARE_COMPLETION_ONSTACK(done);
	int status;

	message->complete = peelir_complete;
	message->context = &done;

	spin_lock_irq(&peelir->spi_lock);
	if (peelir->spi == NULL)
		status = -ESHUTDOWN;
	else
		status = spi_async(peelir->spi, message);

	spin_unlock_irq(&peelir->spi_lock);

	if (status == 0) {
		wait_for_completion(&done);
		status = message->status;
		if (status == 0)
			status = message->actual_length;
	}
	return status;
}

static int peelir_write_message(struct peelir_data *peelir,
		struct spi_ioc_transfer *u_xfers)
{
	struct spi_message msg;
	struct spi_transfer *k_xfers;
	struct spi_transfer *k_tmp;
	struct spi_ioc_transfer *u_tmp;
	unsigned total;
	u8 *buf;
	int status = -EFAULT;

	pr_info("%s:%s\n", IRLED_DEV, __func__);

	spi_message_init(&msg);
	k_xfers = kzalloc(sizeof(*k_tmp), GFP_KERNEL);
	if (k_xfers == NULL)
		return -ENOMEM;

	buf = peelir->buffer;
	total = 0;

	k_tmp = k_xfers;
	u_tmp = u_xfers;
	k_tmp->len = u_tmp->len;

	total += k_tmp->len;
	if (total > bufsiz) {
		pr_info("%s:%s buf overflow\n", IRLED_DEV, __func__);
		status = -EMSGSIZE;
		goto done;
	}

	if (u_tmp->rx_buf) {
		k_tmp->rx_buf = buf;
		if (!access_ok(VERIFY_WRITE, (u8 __user *)
					(uintptr_t) u_tmp->rx_buf,
					u_tmp->len)) {
			pr_info("%s:%s rx buf error\n", IRLED_DEV, __func__);
			goto done;
		}
	}
	if (u_tmp->tx_buf) {
		k_tmp->tx_buf = buf;
		pr_info("%s:%s tx buf len(0x%x)\n", IRLED_DEV, __func__, u_tmp->len);
		if (copy_from_user(buf, (const u8 __user *)
					(uintptr_t) u_tmp->tx_buf,
					u_tmp->len)) {
			pr_info("%s:%s tx buf error\n", IRLED_DEV, __func__);
			goto done;
		}
	}

	k_tmp->cs_change = 0;
	k_tmp->bits_per_word = u_tmp->bits_per_word;
	k_tmp->speed_hz = u_tmp->speed_hz;

	spi_message_add_tail(k_tmp, &msg);

	status = peelir_sync(peelir, &msg);
	if (status < 0)
		goto done;

	status = total;

done:
	kfree(k_xfers);
	return status;
}

#if defined(SUPPORT_READ_MSG)
static u8 *process(u8 *buffer)
{
	int i = 0, byte_num = 0;

	pr_info("%s:%s\n", IRLED_DEV, __func__);

	flag = 0;
	while (1) {
		if (buffer[byte_num] == 0)
			flag = 1;
		else
			byte_num++;

		if (flag)
			break;
	}

	if (byte_num < (MAX_BUFF - TRANS_BUFF)) {
		code_rcv = byte_num;
		pr_info("%s:%s received(0x%02x)\n", IRLED_DEV, __func__, byte_num);
		for (i = 0; i < MAX_BUFF; i++, byte_num++)
			rx_snd_buff[i] = ~buffer[byte_num];
	} else {
		pr_info("%s:%s receive error\n", IRLED_DEV, __func__);
		flag = 0;
	}

	return rx_snd_buff;
}

static int peelir_read_message(struct peelir_data *peelir,
		struct spi_ioc_transfer *u_xfers)
{
	struct spi_message msg;
	struct spi_transfer *k_xfers;
	struct spi_transfer *k_tmp;
	struct spi_ioc_transfer *u_tmp;
	unsigned total;
	u8 *buf;
	u8 *rx_usr_buf;
	char *tx_data;
	int status = -EFAULT;
	int i = 0, m = 0, l = 0;

	pr_info("%s:%s\n", IRLED_DEV, __func__);

	spi_message_init(&msg);
	k_xfers = kzalloc(sizeof(*k_tmp), GFP_KERNEL);
	rx_usr_buf = kmalloc(MAX_BUFF, GFP_KERNEL);
	tx_data = kmalloc(TRANS_BUFF, GFP_KERNEL);
	if (k_xfers == NULL || rx_usr_buf == NULL || tx_data == NULL) {
		status = -ENOMEM;
		goto done;
	}

	/* Accept Pattern from the user and generate the SPI message */

	memset(tx_data, 0, TRANS_BUFF);	/* Transmit buffer */
	memset(t_buff, 0xff, MAX_BUFF);	/* Receive buffer */
	memset(rx_usr_buf, 0, MAX_BUFF);	/* Processed Receive Buffer */

	u_xfers->tx_buf = (unsigned long)tx_data;

	/* Transmit 40K buffer filled with zero for 3 second to keep the SPI clk active for receiving IR input */
	buf = peelir->buffer;
	total = 0;
	k_tmp = k_xfers;
	u_tmp = u_xfers;
	k_tmp->len = u_tmp->len;
	total += k_tmp->len;
	if (total > bufsiz) {
		pr_info("%s:%s buf overflow\n", IRLED_DEV, __func__);
		status = -EMSGSIZE;
		goto done;
	}

	if (u_tmp->rx_buf) {
		k_tmp->rx_buf = buf;
		if (!access_ok(VERIFY_WRITE, (u8 __user *)
					(uintptr_t) u_tmp->rx_buf,
					u_tmp->len)) {
			pr_info("%s:%s rx buf error\n", IRLED_DEV, __func__);
		}
	}
	if (u_tmp->tx_buf) {
		buf = tx_data;
		k_tmp->tx_buf = buf;
	}
	buf += k_tmp->len;

	k_tmp->cs_change = 0;
	k_tmp->bits_per_word = u_tmp->bits_per_word;
	k_tmp->speed_hz = u_tmp->speed_hz;

	spi_message_add_tail(k_tmp, &msg);

	/* Receiving IR input for 3 seconds */

	pr_info("%s:%s Waitint for IR data\n", IRLED_DEV, __func__);
	pr_info("%s:%s Press the Key\n", IRLED_DEV, __func__);
	for (i = 0; i < NUM_BUFF; i++) {
		status = peelir_sync(peelir, &msg);

		buf = peelir->buffer;
		m = 0;

		if (i == 0) {
			for (l = 0; l < byte_delay; l++)
				t_buff[l] = 0xff;
			m = byte_delay;

			for (l = byte_delay; l < TRANS_BUFF; l++) {
				t_buff[l] = buf[m];
				m++;
			}
		} else {
			for (l = (i*TRANS_BUFF); l < ((i+1)*TRANS_BUFF); l++) {
				t_buff[l] = buf[m];
				m++;
			}
		}
	}

	u_tmp = u_xfers;
	u_tmp->len = MAX_BUFF;
	buf = peelir->buffer;

	/* copy any rx data to user space */
	if (u_tmp->rx_buf) {
		rx_usr_buf = process(t_buff);
		u_tmp->len = MAX_BUFF - code_rcv;

		if (flag) {
			pr_info("%s:%s Copying data to user space\n", IRLED_DEV, __func__);
			if (__copy_to_user((u8 __user *)
					(uintptr_t) u_tmp->rx_buf, rx_usr_buf, u_tmp->len)) {
				pr_info("%s:%s Copy to user space failed !!!\n", IRLED_DEV, __func__);
				status = -EFAULT;
				goto done;
			}
		}
	}

	status = total;

done:
	kfree(k_xfers);
	kfree(tx_data);
	kfree(rx_usr_buf);

	return flag;
}
#endif

static long
peelir_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	int status;
	struct peelir_data *peelir;
	struct spi_device *spi;
	struct spi_ioc_transfer	*ioc;

	pr_info("%s:%s cmd(%02x)\n", IRLED_DEV, __func__, cmd);

	peelir = filp->private_data;

	spin_lock_irq(&peelir->spi_lock);

	spi = spi_dev_get(peelir->spi);

	spin_unlock_irq(&peelir->spi_lock);

	if (spi == NULL)
		return -ESHUTDOWN;

	mutex_lock(&peelir->buf_lock);

	spi->mode = mode;
	spi->bits_per_word = bpw;
	spi->max_speed_hz = spi_clk_freq;

	retval = spi_setup(spi);
	if (retval < 0) {
		pr_info("%s:%s error spi setup\n", IRLED_DEV, __func__);
		return retval;
	}

	switch (cmd) {
	/* read ioctls */
	case SPI_IOC_RD_MODE:
		pr_info("%s:%s rd mode\n", IRLED_DEV, __func__);
		retval = __put_user(spi->mode & SPI_MODE_MASK,
					(__u8 __user *)arg);
		break;

	case SPI_IOC_WR_MSG:
		pr_info("%s:%s wr msg\n", IRLED_DEV, __func__);
		/* copy into scratch area */
		ioc = kmalloc(sizeof(struct spi_ioc_transfer), GFP_KERNEL);
		if (!ioc) {
			retval = -ENOMEM;
			break;
		}
		if (__copy_from_user(ioc, (void __user *)arg, sizeof(struct spi_ioc_transfer))) {
			kfree(ioc);
			retval = -EFAULT;
			break;
		}

		status = irled_power_on(&peelir->irled_power);
		if (status) {
			pr_info("%s:%s power on error\n", IRLED_DEV, __func__);
			kfree(ioc);
			retval = -EFAULT;
			break;
		}

		retval = peelir_write_message(peelir, ioc);
		if (retval > 0)
			prev_tx_status = 1;
		else
			prev_tx_status = 0;
		status = irled_power_off(&peelir->irled_power);
		if (status) {
			pr_info("%s:%s power off error\n", IRLED_DEV, __func__);
			kfree(ioc);
			retval = -EFAULT;
			break;
		}

		kfree(ioc);
		break;

	case SPI_IOC_RD_MSG:
#if defined(SUPPORT_READ_MSG)
		pr_info("%s:%s rd msg\n", IRLED_DEV, __func__);
		/* copy into scratch area */
		ioc = kmalloc(sizeof(struct spi_ioc_transfer), GFP_KERNEL);
		if (!ioc) {
			retval = -ENOMEM;
			break;
		}
		if (__copy_from_user(ioc, (void __user *)arg, sizeof(struct spi_ioc_transfer))) {
			kfree(ioc);
			retval = -EFAULT;
			break;
		}

		status = irled_power_on(&peelir->irled_power);
		if (status) {
			pr_info("%s:%s power on error\n", IRLED_DEV, __func__);
			kfree(ioc);
			retval = -EFAULT;
			break;
		}
		retval = peelir_read_message(peelir, ioc);
		status = irled_power_off(&peelir->irled_power);
		if (status) {
			pr_info("%s:%s power off error\n", IRLED_DEV, __func__);
			kfree(ioc);
			retval = -EFAULT;
			break;
		}

		kfree(ioc);
		break;
#else
		retval = -EFAULT;
#endif
	}

	mutex_unlock(&peelir->buf_lock);
	spi_dev_put(spi);

	return retval;
}

static int peelir_open(struct inode *inode, struct file *filp)
{
	struct peelir_data *peelir;
	int status = -ENXIO;

	mutex_lock(&device_list_lock);

	list_for_each_entry(peelir, &device_list, device_entry) {
		if (peelir->devt == inode->i_rdev) {
			status = 0;
			break;
		}
	}

	if (status == 0) {
		if (!peelir->buffer) {
			peelir->buffer = kmalloc(bufsiz, GFP_KERNEL);
			if (!peelir->buffer) {
				pr_info("%s:%s buffer nomem\n", IRLED_DEV, __func__);
				status = -ENOMEM;
			}
		}
		if (status == 0) {
			peelir->users++;
			filp->private_data = peelir;
			nonseekable_open(inode, filp);
		}
	} else
		pr_info("%s:%s nothing for minor %d\n", IRLED_DEV, __func__, iminor(inode));

	mutex_unlock(&device_list_lock);
	return status;
}

static int peelir_release(struct inode *inode, struct file *filp)
{
	struct peelir_data *peelir;
	int status = 0;

	mutex_lock(&device_list_lock);
	peelir = filp->private_data;
	filp->private_data = NULL;

	peelir->users--;
	if (!peelir->users) {
		int dofree;

		kfree(peelir->buffer);
		peelir->buffer = NULL;

		spin_lock_irq(&peelir->spi_lock);
		dofree = (peelir->spi == NULL);
		spin_unlock_irq(&peelir->spi_lock);

		if (dofree)
			kfree(peelir);
	}
	mutex_unlock(&device_list_lock);

	return status;
}

/*
 * sysfs layer
 */

static ssize_t txstat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", prev_tx_status);
}

static ssize_t field_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%x\n", field);
}

static ssize_t field_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%x", &field);
	return count;
}

static DEVICE_ATTR(txstat, S_IRUGO, txstat_show, NULL);
static DEVICE_ATTR(field, S_IRUGO | S_IWUSR, field_show, field_store);

static ssize_t ir_send_result_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", prev_tx_status);
}

static ssize_t ir_send_result_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(ir_send_result, S_IRUGO|S_IWUSR|S_IWGRP, ir_send_result_show, ir_send_result_store);

static struct attribute *sec_ir_attributes = {
	&dev_attr_ir_send_result.attr,
};

static struct attribute *peel_attributes[] = {
	&dev_attr_txstat.attr,
	&dev_attr_field.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = peel_attributes,
};

static const struct file_operations peelir_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = peelir_ioctl,
	.open = peelir_open,
	.release = peelir_release,
};

static struct class *peelir_class;

/*-------------------------------------------------------------------------*/

#if defined(CONFIG_OF)
static int of_irled_parse_dt(struct device *dev, struct peelir_data *peelir)
{
	struct device_node *np_irled = dev->of_node;

	if (!np_irled)
		return -EINVAL;

	pr_info("%s:%s\n", IRLED_DEV, __func__);

	return of_irled_power_parse_dt(dev, &peelir->irled_power);
}
#endif /* CONFIG_OF */

static int peelir_probe(struct spi_device *spi)
{
	struct peelir_data *peelir;
	int status = 0;
	unsigned long minor;
	struct device *dev;

	pr_info("%s:%s\n", IRLED_DEV, __func__);

	/* Allocate driver data */
	peelir = kzalloc(sizeof(*peelir), GFP_KERNEL);
	if (!peelir)
		return -ENOMEM;

	if (spi->dev.of_node) {
		status = of_irled_parse_dt(&spi->dev, peelir);
		if (status) {
			pr_info("%s:%s failed to parse dt\n", IRLED_DEV, __func__);
			kfree(peelir);
			return status;
		}
	}

	/* Initialize the driver data */
	peelir->spi = spi;
	spin_lock_init(&peelir->spi_lock);
	mutex_init(&peelir->buf_lock);

	INIT_LIST_HEAD(&peelir->device_entry);

	/* If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		peelir->devt = MKDEV(peelir_major, minor);
		dev = device_create(peelir_class, &spi->dev, peelir->devt,
				    peelir, "peel_ir");
		if (IS_ERR(dev)) {
			pr_info("%s:%s device_create failed!\n", IRLED_DEV, __func__);
			status = -EFAULT;
		} else {
			set_bit(minor, minors);
			list_add(&peelir->device_entry, &device_list);
		}
	} else {
		pr_info("%s:%s no minor number available\n", IRLED_DEV, __func__);
		status = -ENODEV;
	}
	mutex_unlock(&device_list_lock);

	if (status)
		goto err;

	dev = sec_device_create(NULL, "sec_ir");
	if (IS_ERR(dev)) {
		pr_info("%s:%s create sec_ir error\n", IRLED_DEV, __func__);
		status = -EFAULT;
		goto err;
	}

	status = sysfs_create_file(&dev->kobj, sec_ir_attributes);
	if (status) {
		pr_info("%s:%s create sysfs file for samsung ir\n", IRLED_DEV, __func__);
		sec_device_destroy(dev->devt);
		goto err;
	}

	status = sysfs_create_group(&spi->dev.kobj, &attr_group);
	if (status) {
		pr_info("%s:%s create sysfs file for peel ir\n", IRLED_DEV, __func__);
		sysfs_remove_file(&dev->kobj, sec_ir_attributes);
		sec_device_destroy(dev->devt);
		goto err;
	}

	peelir->sec_device = dev;
	spi_set_drvdata(spi, peelir);

	return status;

err:
	pr_info("%s:%s error(%d)\n", IRLED_DEV, __func__, status);

	device_destroy(peelir_class, peelir->devt);
	kfree(peelir);
	return status;
}

static int peelir_remove(struct spi_device *spi)
{
	struct peelir_data *peelir = spi_get_drvdata(spi);

	sysfs_remove_group(&spi->dev.kobj, &attr_group);
	sysfs_remove_file(&peelir->sec_device->kobj, sec_ir_attributes);
	sec_device_destroy(peelir->sec_device->devt);

	/* make sure ops on existing fds can abort cleanly */
	spin_lock_irq(&peelir->spi_lock);
	peelir->spi = NULL;
	spi_set_drvdata(spi, NULL);
	spin_unlock_irq(&peelir->spi_lock);

	/* prevent opening a new instance of the device
	   during the removal of the device
	 */
	mutex_lock(&device_list_lock);
	list_del(&peelir->device_entry);
	device_destroy(peelir_class, peelir->devt);
	unregister_chrdev(peelir_major, "peel_ir");
	clear_bit(MINOR(peelir->devt), minors);
	if (peelir->users == 0)
		kfree(peelir);
	mutex_unlock(&device_list_lock);

	return 0;
}

#if defined(CONFIG_OF)
static struct of_device_id peel_spi_dt_ids[] = {
	{ .compatible = "nochip,irled" },
	{ },
};
#else
#define peel_spi_dt_ids	NULL
#endif /* CONFIG_OF */

MODULE_DEVICE_TABLE(of, peel_spi_dt_ids);

static struct spi_driver peelir_spi_driver = {
	.driver = {
		.name = "peelir",
		.owner = THIS_MODULE,
		.of_match_table = peel_spi_dt_ids,
	},
	.probe =	peelir_probe,
	.remove =	__devexit_p(peelir_remove),

	/* NOTE:  suspend/resume methods are not necessary here.
	 * We don't do anything except pass the requests to/from
	 * the underlying controller.  The refrigerator handles
	 * most issues; the controller driver handles the rest.
	 */
};

/*-------------------------------------------------------------------------*/

static int __init peelir_init(void)
{
	int status;

	pr_info("%s:%s\n", IRLED_DEV, __func__);

	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */
	BUILD_BUG_ON(N_SPI_MINORS > 256);
	peelir_major = register_chrdev(0, "peel_ir", &peelir_fops);
	if (peelir_major < 0) {
		pr_info("%s:%s chdev err(%d)\n", IRLED_DEV, __func__, peelir_major);
		return peelir_major;
	}

	peelir_class = class_create(THIS_MODULE, "peel_ir");
	if (IS_ERR(peelir_class)) {
		pr_info("%s:%s class err\n", IRLED_DEV, __func__);
		unregister_chrdev(peelir_major, peelir_spi_driver.driver.name);
		return PTR_ERR(peelir_class);
	}

	status = spi_register_driver(&peelir_spi_driver);
	if (status < 0) {
		pr_info("%s:%s spi err(%d)\n", IRLED_DEV, __func__, status);
		class_destroy(peelir_class);
		unregister_chrdev(peelir_major, peelir_spi_driver.driver.name);
	}
	pr_info("%s:%s end\n", IRLED_DEV, __func__);

	return status;
}
module_init(peelir_init);

static void __exit peelir_exit(void)
{
	spi_unregister_driver(&peelir_spi_driver);
	class_destroy(peelir_class);
	unregister_chrdev(peelir_major, peelir_spi_driver.driver.name);
}
module_exit(peelir_exit);

MODULE_DESCRIPTION("Peel IR SPI driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("PEEL:IR");
