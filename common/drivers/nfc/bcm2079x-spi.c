/*
 * Copyright (C) 2011 Broadcom Corporation.
 *
 * Author: Kevin Park <spark@broadcom.com>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/spi/spi.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/bcm2079x.h>
#include <linux/poll.h>
#include <linux/version.h>

#ifndef U16_TO_STREAM
#define U16_TO_STREAM(p, var16) {*(p)++ = (u8)((var16)>>8);\
				*(p)++ = (u8)(var16&0xFF); }
#endif
#ifndef U8_TO_STREAM
#define U8_TO_STREAM(p, var8) {*(p)++ = (u8)(var8); }
#endif

#ifndef STREAM_TO_U16
#define STREAM_TO_U16(var16, p) {(var16) = ((u16)(*((u8 *)p+1)) + \
				((u8)(*((u8 *)p) << 8))); }
#endif

#define TRUE                1
#define FALSE               0
#define STATE_HIGH          1
#define STATE_LOW           0

#define NFC_REQ_ACTIVE_STATE    STATE_LOW
#define NFC_WAKE_ACTIVE_STATE   STATE_HIGH
#define NFC_REQ_INACTIVE_STATE  STATE_HIGH

#define INTERRUPT_TRIGGER_TYPE  IRQF_TRIGGER_FALLING

#define MAX_BUFFER_SIZE     780

struct bcm2079x_dev {
	wait_queue_head_t read_wq;
	struct mutex read_mutex;
	struct spi_device *spi;
	struct miscdevice bcm2079x_device;
	int wake_gpio;
	unsigned int en_gpio;
	unsigned int irq_gpio;
	bool irq_enabled;
	spinlock_t irq_enabled_lock;
	unsigned char cs_change;
	unsigned int packet_size;
	unsigned int wake_active_state;
	unsigned int read_multiple_packets;
};

static inline int spi_write_cs_hint(struct spi_device *spi, const void *buf,
				size_t len, char cs_hint)
{
	struct spi_transfer t = {
		.tx_buf = buf,
		.len = len,
		.cs_change = cs_hint,
	};
	struct spi_message m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spi_sync(spi, &m);
}

static inline size_t spi_read_espi(struct spi_device *spi, void *buf,
				  size_t size)
{
	unsigned char req[2] = { 0x02, 0x00 };
	unsigned char res[2] = { 0x00, };
	unsigned short rx_len = 0;
	int ret;

	struct spi_transfer t_c = {
		.tx_buf = req,
		.len = 2,
		.cs_change = 1,	/* For OMAP4, McSPI driver fix needed */
	};
	struct spi_transfer t_r = {
		.rx_buf = res,
		.len = 2,
		.cs_change = 1,	/* For OMAP4, McSPI driver fix needed */
	};
	struct spi_message m;

	spi_message_init(&m);
	spi_message_add_tail(&t_c, &m);
	spi_message_add_tail(&t_r, &m);
	ret = spi_sync(spi, &m);

	if (ret)
		return 0;

	STREAM_TO_U16(rx_len, res);
	if (rx_len == 0) {
		dev_info(&spi->dev, "-read try fail: %d ", rx_len);
		return 0;
	}

	if (size < rx_len)
		rx_len = size;

	dev_info(&spi->dev, "-read back %d bytes", rx_len);

	spi_message_init(&m);
	t_r.rx_buf = buf;
	t_r.len = rx_len;
	t_r.cs_change = 0;
	spi_message_add_tail(&t_r, &m);
	ret = spi_sync(spi, &m);

	if (ret)
		return 0;
	else
		return rx_len;
}

static void bcmspinfc_disable_irq(struct bcm2079x_dev *bcm2079x_dev)
{
	unsigned long flags;
	spin_lock_irqsave(&bcm2079x_dev->irq_enabled_lock, flags);
	if (bcm2079x_dev->irq_enabled) {
		disable_irq_nosync(bcm2079x_dev->spi->irq);
		bcm2079x_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&bcm2079x_dev->irq_enabled_lock, flags);
}

static void bcmspinfc_enable_irq(struct bcm2079x_dev *bcm2079x_dev)
{
	unsigned long flags;
	spin_lock_irqsave(&bcm2079x_dev->irq_enabled_lock, flags);
	if (!bcm2079x_dev->irq_enabled) {
		bcm2079x_dev->irq_enabled = true;
		enable_irq(bcm2079x_dev->spi->irq);
	}
	spin_unlock_irqrestore(&bcm2079x_dev->irq_enabled_lock, flags);
}

static irqreturn_t bcm2079x_dev_irq_handler(int irq, void *dev_id)
{
	struct bcm2079x_dev *bcm2079x_dev = dev_id;
	if (gpio_get_value(bcm2079x_dev->irq_gpio) != NFC_REQ_ACTIVE_STATE)
		return IRQ_HANDLED;

	bcmspinfc_disable_irq(bcm2079x_dev);

	/* Wake up waiting readers */
	wake_up(&bcm2079x_dev->read_wq);

	return IRQ_HANDLED;
}

static unsigned int bcm2079x_dev_poll(struct file *filp, poll_table * wait)
{
	struct bcm2079x_dev *bcm2079x_dev = filp->private_data;
	unsigned int mask = 0;

	poll_wait(filp, &bcm2079x_dev->read_wq, wait);

	if (gpio_get_value(bcm2079x_dev->irq_gpio) == NFC_REQ_ACTIVE_STATE)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static ssize_t bcm2079x_dev_read(struct file *filp, char __user *buf,
				  size_t count, loff_t *offset)
{
	struct bcm2079x_dev *bcm2079x_dev = filp->private_data;
	char p_rcv[MAX_BUFFER_SIZE];
	int ret, len, packets;

	ret = 0;
	len = 1;
	packets = 0;
	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	dev_info(&bcm2079x_dev->spi->dev,
		 "bcm2079x_dev_read count %d\n", count);

	if (bcm2079x_dev->packet_size > 0) {
		count = bcm2079x_dev->packet_size;
		len = count;
	}

	mutex_lock(&bcm2079x_dev->read_mutex);

	ret = spi_read_espi(bcm2079x_dev->spi, p_rcv, MAX_BUFFER_SIZE);

	mutex_unlock(&bcm2079x_dev->read_mutex);

	if (copy_to_user(buf, &p_rcv, ret)) {
		dev_err(&bcm2079x_dev->spi->dev,
			"failed to copy to user space, rx_cnt = %d\n", ret);
		ret = -EFAULT;
	}

	bcmspinfc_enable_irq(bcm2079x_dev);
	return ret;
}

static ssize_t bcm2079x_dev_write(struct file *filp, const char __user *buf,
				   size_t count, loff_t *offset)
{
	struct bcm2079x_dev *bcm2079x_dev = filp->private_data;
	char txbuff[MAX_BUFFER_SIZE];
	char *p = txbuff;
	int ret;

	if (count > MAX_BUFFER_SIZE) {
		dev_err(&bcm2079x_dev->spi->dev, "out of memory\n");
		return -ENOMEM;
	}

	if (copy_from_user(&txbuff[4], buf, count)) {
		dev_err(&bcm2079x_dev->spi->dev,
			"failed to copy from user space\n");
		return -EFAULT;
	}

	dev_info(&bcm2079x_dev->spi->dev,
		 "bcmspinfc irq gpio= %d\n",
		 gpio_get_value(bcm2079x_dev->irq_gpio));

	bcmspinfc_disable_irq(bcm2079x_dev);
	U8_TO_STREAM(p, 0x01); /* Direct Write */
	U8_TO_STREAM(p, 0x00); /* Reserved */
	U16_TO_STREAM(p, count); /* Length */

	mutex_lock(&bcm2079x_dev->read_mutex);

	/* Write data */
	ret = spi_write_cs_hint(bcm2079x_dev->spi, txbuff, count + 4, 
				bcm2079x_dev->cs_change);

	/* Restore cs_change hit */
	bcm2079x_dev->cs_change=0;

	if (ret != 0) {
		dev_err(&bcm2079x_dev->spi->dev, "write %d\n", ret);
		ret = -EIO;
	} else {
		ret = count;
	}

	mutex_unlock(&bcm2079x_dev->read_mutex);
	bcmspinfc_enable_irq(bcm2079x_dev);

	return ret;
}

static int bcm2079x_dev_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct bcm2079x_dev *bcm2079x_dev = container_of(filp->private_data,
							   struct bcm2079x_dev,
							   bcm2079x_device);

	filp->private_data = bcm2079x_dev;
	bcm2079x_dev->wake_active_state = NFC_WAKE_ACTIVE_STATE;

	dev_info(&bcm2079x_dev->spi->dev,
		 "dev_open %d,%d\n", imajor(inode), iminor(inode));

	return ret;
}

static long bcm2079x_dev_unlocked_ioctl(struct file *filp,
					 unsigned int cmd, unsigned long arg)
{
	struct bcm2079x_dev *bcm2079x_dev = filp->private_data;

	switch (cmd) {
	case BCMNFC_SET_WAKE_ACTIVE_STATE:
		dev_info(&bcm2079x_dev->spi->dev,
			 "%s, BCMNFC_SET_WAKE_ACTIVE_STATE (%x, %lx):\n",
			 __func__, cmd, arg);
		bcm2079x_dev->wake_active_state = arg;
		break;
	case BCMNFC_READ_FULL_PACKET:
		dev_info(&bcm2079x_dev->spi->dev,
			 "%s, BCMNFC_READ_FULL_PACKET (%x, %lx):\n", __func__,
			 cmd, arg);
		bcm2079x_dev->packet_size = arg;
		break;
	case BCMNFC_READ_MULTI_PACKETS:
		dev_info(&bcm2079x_dev->spi->dev,
			 "%s, BCMNFC_READ_MULTI_PACKETS (%x, %lx):\n", __func__,
			 cmd, arg);
		bcm2079x_dev->read_multiple_packets = arg;
		break;
	case BCMNFC_POWER_CTL:
		dev_info(&bcm2079x_dev->spi->dev,
			 "%s, BCMNFC_POWER_CTL GPIO[%d](%x, %lx):\n", __func__,
			 bcm2079x_dev->en_gpio, cmd, arg);
		gpio_set_value(bcm2079x_dev->en_gpio, arg);
		break;
	case BCMNFC_WAKE_CTL:
		dev_info(&bcm2079x_dev->spi->dev,
			 "%s, BCMNFC_WAKE_CTL GPIO[%d] (%x, %lx):\n", __func__,
			 bcm2079x_dev->wake_gpio, cmd, arg);

		if (bcm2079x_dev->wake_gpio<0)
		{
			bcm2079x_dev->cs_change=1;
			return 1;
		}

		gpio_set_value(bcm2079x_dev->wake_gpio, arg);
		break;
	default:
		dev_err(&bcm2079x_dev->spi->dev,
			"%s, unknown cmd (%x, %lx)\n", __func__, cmd, arg);
		return 0;
	}

	return 0;
}

static const struct file_operations bcm2079x_dev_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.poll = bcm2079x_dev_poll,
	.read = bcm2079x_dev_read,
	.write = bcm2079x_dev_write,
	.open = bcm2079x_dev_open,
	.unlocked_ioctl = bcm2079x_dev_unlocked_ioctl
};

static int bcmspinfc_probe(struct spi_device *spi)
{
	int ret;
	struct bcm2079x_platform_data *platform_data;
	struct bcm2079x_dev *bcm2079x_dev;

	platform_data = spi->dev.platform_data;

	dev_info(&spi->dev, "%s, probing bcmspinfc driver\n", __func__);

	if (platform_data == NULL) {
		dev_err(&spi->dev, "nfc probe fail\n");
		return -ENODEV;
	}

	ret = gpio_request(platform_data->irq_gpio, "nfc_spi_int");
	if (ret)
		return -ENODEV;

	ret = gpio_request(platform_data->en_gpio, "nfc_en");
	if (ret)
		goto err_en;

	/* Wake pin can be joined with SPI_CSN */
	if (platform_data->wake_gpio > 0)
	{
		ret = gpio_request(platform_data->wake_gpio, "nfc_spi_cs");
		if (ret)
			goto err_firm;
	}

	gpio_direction_output(platform_data->en_gpio, 0);

	/* Wake pin can be joined with SPI_CSN */
	if (platform_data->wake_gpio > 0)
	{
		gpio_direction_output(platform_data->wake_gpio, 0);
	}
	gpio_direction_input(platform_data->irq_gpio);
	gpio_set_value(platform_data->en_gpio, 0);
	gpio_set_value(platform_data->wake_gpio, 0);

	bcm2079x_dev = kzalloc(sizeof(*bcm2079x_dev), GFP_KERNEL);
	if (bcm2079x_dev == NULL) {
		dev_err(&spi->dev,
			"failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	bcm2079x_dev->wake_gpio = platform_data->wake_gpio;
	bcm2079x_dev->irq_gpio = platform_data->irq_gpio;
	bcm2079x_dev->en_gpio = platform_data->en_gpio;
	bcm2079x_dev->spi = spi;
	/* init mutex and queues */
	init_waitqueue_head(&bcm2079x_dev->read_wq);
	mutex_init(&bcm2079x_dev->read_mutex);
	spin_lock_init(&bcm2079x_dev->irq_enabled_lock);

	bcm2079x_dev->bcm2079x_device.minor = MISC_DYNAMIC_MINOR;
	bcm2079x_dev->bcm2079x_device.name = "bcm2079x";
	bcm2079x_dev->bcm2079x_device.fops = &bcm2079x_dev_fops;

	ret = misc_register(&bcm2079x_dev->bcm2079x_device);
	if (ret) {
		dev_err(&spi->dev, "misc_register failed\n");
		goto err_misc_register;
	}

	/* request irq.  the irq is set whenever the chip has data available
	 * for reading.  it is cleared when all data has been read.
	 */
	dev_info(&spi->dev, "requesting IRQ %d\n", spi->irq);
	bcm2079x_dev->irq_enabled = true;
	ret = request_irq(spi->irq, bcm2079x_dev_irq_handler,
			  INTERRUPT_TRIGGER_TYPE, spi->modalias,
			  bcm2079x_dev);
	if (ret) {
		dev_err(&spi->dev, "request_irq failed\n");
		goto err_request_irq_failed;
	}
	bcmspinfc_disable_irq(bcm2079x_dev);
	spi_set_drvdata(spi, bcm2079x_dev);

	bcm2079x_dev->packet_size = 0;
	dev_info(&spi->dev,
		 "%s, probing bcmspinfc driver exited successfully\n",
		 __func__);
	return 0;

err_request_irq_failed:
	misc_deregister(&bcm2079x_dev->bcm2079x_device);
err_misc_register:
	mutex_destroy(&bcm2079x_dev->read_mutex);
	kfree(bcm2079x_dev);
err_exit:
	gpio_free(platform_data->wake_gpio);
err_firm:
	gpio_free(platform_data->en_gpio);
err_en:
	gpio_free(platform_data->irq_gpio);
	return ret;
}

static int bcmspinfc_remove(struct spi_device *spi)
{
	struct bcm2079x_dev *bcm2079x_dev;

	bcm2079x_dev = spi_get_drvdata(spi);
	free_irq(spi->irq, bcm2079x_dev);
	misc_deregister(&bcm2079x_dev->bcm2079x_device);
	mutex_destroy(&bcm2079x_dev->read_mutex);
	gpio_free(bcm2079x_dev->irq_gpio);
	gpio_free(bcm2079x_dev->en_gpio);
	kfree(bcm2079x_dev);

	return 0;
}

static struct spi_driver bcmspinfc_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "bcm2079x-spi",
		   },
	.probe = bcmspinfc_probe,
	.remove = bcmspinfc_remove,
};

/*
 * module load/unload record keeping
 */

static int __init bcm2079x_dev_init(void)
{
	pr_info("Loading bcmspinfc driver\n");
	return spi_register_driver(&bcmspinfc_driver);
}

module_init(bcm2079x_dev_init);

static void __exit bcm2079x_dev_exit(void)
{
	pr_info("Unloading bcmspinfc driver\n");
	spi_unregister_driver(&bcmspinfc_driver);
}

module_exit(bcm2079x_dev_exit);

MODULE_AUTHOR("Broadcom");
MODULE_DESCRIPTION("NFC bcmspinfc driver");
MODULE_LICENSE("GPL");
