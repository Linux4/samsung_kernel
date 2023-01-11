/*
 * Copyright (C) 2011 Marvell International Ltd.
 *		Jialing Fu <jlfu@marvell.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * TODO:
 * 1. clean the comment
 * 2. double check the lock mechanism
 * 3. remove unnecessary h file
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>

#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/ioctl.h>

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/spi/cmmb.h>

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/wait.h>

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#include "mxdchar.h"
#ifdef CONFIG_MXDCMMB_DEBUG
#define LOG_ERR(fmt, x...) pr_err(fmt, ##x)
#define LOG_INFO(fmt, x...) pr_info(fmt, ##x)
#define LOG_DEBUG(fmt, x...) pr_info(fmt, ##x)
#else
#define LOG_ERR(fmt, x...) pr_err(fmt, ##x)
#define LOG_INFO(fmt, x...)
#define LOG_DEBUG(fmt, x...)
#endif

#define DRIVER_NAME "cmmb_if"

/*Begin of SPI interface */
/*
 * Here is a spi transfer limitation:
 * some platform can't support very large spi burst transfer
 * example:the pxa910 spi driver code support 8191B maxinum
 *
 * the transfer can split to muti transfer in chip_spi_xfer
 * if the xfer len is larger then the limit
 */
#define PXA_SPI_MAX_SIZE		4096

/* some SPI definition*/
#define MXD_SPI_BITS_PER_WORD	(8)
#define MXD_SPI_WORK_SPEED_HZ	(13000000)
#define MXD_SPI_INIT_SPEED_HZ	(13000000)
/* Bit masks for spi_device.mode management.  Note that incorrect
 * settings for CS_HIGH and 3WIRE can cause *lots* of trouble for other
 * devices on a shared bus:  CS_HIGH, because this device will be
 * active when it shouldn't be;  3WIRE, because when active it won't
 * behave as it should.
 *
 * REVISIT should changing those two modes be privileged?
 */
#define SPI_MODE_MASK           (SPI_CPHA | SPI_CPOL | SPI_CS_HIGH \
					| SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP)
/*End of SPI interface */

/*
 * If the platform dosen't provide the callback functions to
 * control the POWER or RESET, or the driver want to control
 * the pin directlly,
 * define "DRIVER_HANDLE_GPIO" Here
 */
#define DRIVER_HANDLE_GPIO

/*
 * If the platform dosen't pass the pin definition,or the
 * driver want to set the pin num directlly,
 * define the "PIN_DEFINED_BY_DRIVE" Here.
 *
 * You can define the power/reset/spi_cs pin num in this file
 * directlly. By this way,the driver is easy to debug or port
 */
/*#define PIN_DEFINED_BY_DRIVE*/
#define POWER_PIN_NUM			150
#define RESET_PIN_NUM			151
#define SPI_CS_PIN_NUM			34

#define SPIDEV_MAJOR            0	/* assigned */
#define N_SPI_MINORS            32	/* ... up to 256 */
static unsigned long minors[N_SPI_MINORS / BITS_PER_LONG];
static int mxdspi_major = SPIDEV_MAJOR;

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

struct cmmbspi_dev {
	dev_t devt;
	spinlock_t spi_lock;
	struct spi_device *spi;
	struct list_head device_entry;

	/* Interrupt Register */
	int irq;
	unsigned long irq_flag;	/* irq flag */
	unsigned char irq_enable;
	wait_queue_head_t iowait;	/* use for user space polling */

	/* HW Subsystem structures */
	int gpio_power;		/* the pin to power on/off the module */
	int gpio_reset;		/* the pin to reset  the module */
	int gpio_cs;		/* spi cs pin num */
	int gpio_defined;	/* The platform has set the GPIO */

	/* DMA buffers */
	int mapcount;
	unsigned int dma_buf_size;	/* allocated size */
	int order;		/* Internal buffer addresses */

	void *dma_rx_bufs;	/* Internal Rx buffer addresses */
	dma_addr_t dma_rx_handles;	/* Buffer Rx bus addresses */
	void *dma_tx_bufs;	/* Internal Tx buffer addresses */
	dma_addr_t dma_tx_handles;	/* Tx Buffer bus addresses */
	int is_tx_clean;	/* whether dma_tx_bufs has be cleaned */

	/* buffer is NULL unless this device is open (users > 0) */
	struct mutex buf_lock;
	unsigned int users;
	unsigned char *buffer;
};

/*
 * The porting layer, according to different host platform.
 *
 * The below SPI functions use the struct cmmbspi_dev,instead of
 * struct spi_device.
 * The purposes are
 *  1.make the interface simple
 *  2.easy to add other features in the further if need
 *
 * if the chip_xxx return 0, it is OK, other is not OK
 */

static int chip_power_reset(struct cmmbspi_dev *cmmb_dev)
{
#ifdef DRIVER_HANDLE_GPIO
	int chip_rst;
	int rst_loop;
	LOG_DEBUG("sms smschipreset\n");
	chip_rst = cmmb_dev->gpio_reset;
	if (gpio_request(chip_rst, "cmmb rst")) {
		LOG_ERR("failed to request GPIO for CMMB RST\n");
		return -EIO;
	}

	/* reset chip twice, it may reset fail sometimes */
	rst_loop = 1;		/* change to 1 for mxd device */
	while (rst_loop--) {
		/* reset cmmb, keep low for about 1ms */
		gpio_direction_output(chip_rst, 0);
		msleep(20);	/* 10ms enough */

		/* get cmmb go out of reset state */
		gpio_direction_output(chip_rst, 1);
		/* wait for at least 100ms after reset */
		msleep(20);	/* TODO:Check with HW Engineer */

	}

	gpio_free(chip_rst);
	return 0;
#else
	struct cmmb_platform_data *pdata;
	pdata = cmmb_dev->spi->dev.platform_data;
	if (!pdata || !pdata->power_reset)
		return -EIO;
	pdata->power_reset();
	return 0;
#endif
}

static int chip_power_on(struct cmmbspi_dev *cmmb_dev)
{
#ifdef DRIVER_HANDLE_GPIO
	int chip_en;

	chip_en = cmmb_dev->gpio_power;
	if (gpio_request(chip_en, "cmmb power")) {
		LOG_ERR("[ERROR] failed to request GPIO for CMMB POWER\n");
		return -EIO;
	}
	gpio_direction_output(chip_en, 0);
	msleep(20);		/* TODO:Check with HW Engineer */

	gpio_direction_output(chip_en, 1);
	msleep(20);		/* TODO:Check with HW Engineer */

	gpio_free(chip_en);

	return chip_power_reset(cmmb_dev);
#else
	struct cmmb_platform_data *pdata;

	pdata = cmmb_dev->spi->dev.platform_data;
	if (!pdata || !pdata->power_on)
		return -EIO;
	pdata->power_on();

	return 0;
#endif
}

static int chip_power_down(struct cmmbspi_dev *cmmb_dev)
{
#ifdef DRIVER_HANDLE_GPIO
	int chip_en;

	chip_en = cmmb_dev->gpio_power;
	if (gpio_request(chip_en, "cmmb power")) {
		LOG_ERR("failed to request GPIO for CMMB POWER\n");
		return -EIO;
	}

	gpio_direction_output(chip_en, 0);
	msleep(20);		/* TODO:Check with HW Engineer */
	gpio_free(chip_en);

	return 0;
#else
	struct cmmb_platform_data *pdata;

	pdata = cmmb_dev->spi->dev.platform_data;
	if (!pdata || !pdata->power_off)
		return -EIO;
	pdata->power_off();

	return 0;
#endif
}

#if 0
static void chip_cs_assert(struct cmmbspi_dev *cmmb_dev)
{
#ifdef DRIVER_HANDLE_GPIO
	int cs;
	cs = cmmb_dev->gpio_cs;
	gpio_direction_output(cs, 0);
#else
	struct cmmb_platform_data *pdata;

	pdata = cmmb_dev->spi->dev.platform_data;
	if (!pdata || !pdata->cs_assert)
		return;
	pdata->cs_assert();
#endif
}

static void chip_cs_deassert(struct cmmbspi_dev *cmmb_dev)
{
#ifdef DRIVER_HANDLE_GPIO
	int cs;
	cs = cmmb_dev->gpio_cs;
	gpio_direction_output(cs, 1);
#else
	struct cmmb_platform_data *pdata;

	pdata = cmmb_dev->spi->dev.platform_data;
	if (!pdata || !pdata->cs_deassert)
		return;
	pdata->cs_deassert();
#endif
}
#endif

/* -------------------------------------------------------------------- */
/*
 * DMA buffer management.  These functions need s_mutex held.
 */
static int chip_alloc_dma_bufs(struct cmmbspi_dev *cmmb_dev)
{
	LOG_DEBUG("enter to chip_alloc_dma_bufs\n");

	cmmb_dev->dma_buf_size = PXA_SPI_MAX_SIZE;

	cmmb_dev->order = get_order(cmmb_dev->dma_buf_size);
	cmmb_dev->dma_rx_bufs =
	    (unsigned long *)__get_free_pages(GFP_KERNEL, cmmb_dev->order);
	if (cmmb_dev->dma_rx_bufs == NULL) {
		LOG_ERR("Failed to allocate Rx DMA buffer\n");
		return -ENOMEM;
	}
	cmmb_dev->dma_rx_handles = __pa(cmmb_dev->dma_rx_bufs);

	cmmb_dev->dma_tx_bufs =
	    (unsigned long *)__get_free_pages(GFP_KERNEL, cmmb_dev->order);
	if (cmmb_dev->dma_tx_bufs == NULL) {
		LOG_ERR("Failed to allocate Tx DMA buffer\n");
		free_pages((unsigned long)cmmb_dev->dma_rx_bufs,
			   cmmb_dev->order);
		cmmb_dev->dma_rx_bufs = NULL;
		return -ENOMEM;
	}
	cmmb_dev->dma_tx_handles = __pa(cmmb_dev->dma_tx_bufs);

	/* For debug, remove eventually */
	memset(cmmb_dev->dma_rx_bufs, 0xcc, cmmb_dev->dma_buf_size);
	memset(cmmb_dev->dma_tx_bufs, 0x00, cmmb_dev->dma_buf_size);

	return 0;
}

static void chip_free_dma_bufs(struct cmmbspi_dev *cmmb_dev)
{
	LOG_DEBUG("enter to chip free_dma_bufs\n");
	if (cmmb_dev->dma_rx_bufs) {
		free_pages((unsigned long)cmmb_dev->dma_rx_bufs,
			   cmmb_dev->order);
		cmmb_dev->dma_rx_bufs = NULL;
	}

	if (cmmb_dev->dma_tx_bufs) {
		free_pages((unsigned long)cmmb_dev->dma_tx_bufs,
			   cmmb_dev->order);
		cmmb_dev->dma_tx_bufs = NULL;
	}
}

static int spi_duplex_xfer(struct spi_device *spi,
			   const u8 *txbuffer, u8 *rxbuffer, int length)
{
	struct spi_transfer t = {
		.tx_buf = txbuffer,
		.rx_buf = rxbuffer,
		.len = length,
	};
	struct spi_message m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spi_sync(spi, &m);
}

static int chip_spi_xfer(struct cmmbspi_dev *cmmb_dev,
			 const u8 *txbuffer, u8 *rxbuffer, int length)
{
	struct spi_device *spi = cmmb_dev->spi;
	int offset = 0;
	int temp_size = 0;
	int ret;

	if (!txbuffer) {
		if (!cmmb_dev->is_tx_clean) {
			memset(cmmb_dev->dma_tx_bufs, 0x00, PXA_SPI_MAX_SIZE);
			cmmb_dev->is_tx_clean = 1;
		}
	}

	while (length) {
		temp_size =
		    (length >= PXA_SPI_MAX_SIZE) ? PXA_SPI_MAX_SIZE : length;

		if (txbuffer) {
			/* Spi Write has real data */
			cmmb_dev->is_tx_clean = 0;
			/* memcpy(cmmb_dev->dma_tx_bufs, txbuffer + offset,
			   temp_size); */
			ret = copy_from_user((u8 *) cmmb_dev->dma_tx_bufs,
					     (unsigned char *)txbuffer + offset,
					     temp_size);
			if (ret) {
				LOG_ERR("%s: copy_from_user error!", __func__);
				break;
			}

		}
		ret = spi_duplex_xfer(spi,
				      (const u8 *)cmmb_dev->dma_tx_bufs,
				      (u8 *) cmmb_dev->dma_rx_bufs, temp_size);

		if (ret) {
			LOG_ERR
			    ("%s: spi_write_read to cmmb device error! %d \r\n",
			     __func__, ret);
			break;
		}
		if (rxbuffer) {
			/* Spi Read has read data */
			/* memcpy(rxbuffer + offset, cmmb_dev->dma_rx_bufs,
			   temp_size); */
			ret = copy_to_user((unsigned char *)rxbuffer + offset,
				(u8 *)cmmb_dev->dma_rx_bufs, temp_size);
			if (ret) {
				LOG_ERR("%s: copy_to_user error!", __func__);
				break;
			}
		}
		offset += temp_size;
		length -= temp_size;
	}
	return length;
}

/*
 * This function provide an API for user space SPI full duplex.
 * txbuffer,rxbuffer is passed from user space.
 *
 */
static int cmmb_spi_xfer(struct cmmbspi_dev *cmmb_dev,
			 const u8 *txbuffer, int txlength,
			 u8 *rxbuffer, int rxlength)
{

	int xfer_len, un_xfer;
	xfer_len = (txlength >= rxlength) ? txlength : rxlength;

#if 0
	chip_cs_assert(cmmb_dev);
#endif
	un_xfer = chip_spi_xfer(cmmb_dev, txbuffer, rxbuffer, xfer_len);
#if 0
	chip_cs_deassert(cmmb_dev);
#endif
	return un_xfer;
}

/*
 * SPI has a character major number assigned.  We allocate minor numbers
 * dynamically using a bitmask.  You must use hotplug tools, such as udev
 * (or mdev with busybox) to create and destroy the /dev/mxdspidevB.C device
 * nodes, since there is no fixed association of minor numbers with any
 * particular SPI bus or device.
 */

/*
 * We can't use the standard synchronous wrappers for file I/O; we
 * need to protect against async removal of the underlying spi_device.
 */
/*-------------------------------------------------------------------------*/

/* Read-only message with current device setup */
static ssize_t
mxdspidev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct cmmbspi_dev *cmmb_dev = (struct cmmbspi_dev *)filp->private_data;
	int un_xfer;
	ssize_t xfer;

	LOG_DEBUG("read buf:0x%p,len:%zd\n", buf, count);
	un_xfer = cmmb_spi_xfer(cmmb_dev, NULL, 0, buf, count);
	xfer = count - un_xfer;

	return xfer;
}

/* Write-only message with current device setup */
static ssize_t
mxdspidev_write(struct file *filp, const char __user *buf,
	size_t count, loff_t *f_pos)
{
	struct cmmbspi_dev *cmmb_dev = (struct cmmbspi_dev *)filp->private_data;
	int un_xfer;
	ssize_t xfer;

	LOG_DEBUG("write buf:0x%p,len:%zd\n", buf, count);
	un_xfer = cmmb_spi_xfer(cmmb_dev, buf, count, NULL, 0);
	xfer = count - un_xfer;

	return xfer;
}

static long do_mxdspidev_ioctl(struct file *filp, unsigned int cmd,
					struct mxdchar_spi_t *spi_msg)
{
	int retval = -EIO;
	int un_xfer;
	struct cmmbspi_dev *cmmb_dev;

	cmmb_dev = filp->private_data;
	switch (cmd) {
	case MXDSPI_IOC_RW:
			LOG_DEBUG("MXDSPI_IOC_RW start\n");
			un_xfer = cmmb_spi_xfer(cmmb_dev,
					spi_msg->txbuffer, spi_msg->txlength,
					spi_msg->rxbuffer, spi_msg->rxlength);
			retval = un_xfer ? (-EBUSY) : 0;
		break;

	case MXDSPI_IOC_R:
			LOG_DEBUG("MXDSPI_IOC_R start\n");
			un_xfer = cmmb_spi_xfer(cmmb_dev, NULL, 0,
				spi_msg->rxbuffer, spi_msg->rxlength);
			retval = un_xfer ? (-EBUSY) : 0;
		break;

	case MXDSPI_IOC_W:
			LOG_DEBUG("MXDSPI_IOC_W start\n");
			un_xfer = cmmb_spi_xfer(cmmb_dev,
				spi_msg->txbuffer, spi_msg->txlength, NULL, 0);
			retval = un_xfer ? (-EBUSY) : 0;
		break;
	}

	return retval;
}




static long
mxdspidev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = -EIO;
	struct mxdchar_spi_t spi_msg;

	mutex_lock(&device_list_lock);	/* TODO:check wether the lock is OK */

	switch (cmd) {
	case MXDSPI_IOC_RW:
	case MXDSPI_IOC_R:
	case MXDSPI_IOC_W:
		retval = copy_from_user((unsigned char *)&spi_msg,
			(unsigned char *)arg, sizeof(struct mxdchar_spi_t));
		if (retval == 0)
			retval = do_mxdspidev_ioctl(filp, cmd, &spi_msg);
		break;

	default:
		LOG_INFO("MXDSPI_IOC cmd:%x not support\n", cmd);
		break;
	}

	mutex_unlock(&device_list_lock);	/* TODO: Check it */

	return retval;
}

static long
compat_mxdspidev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = -EIO;
	struct mxdchar_spi_t spi_msg;
	struct compat_mxdchar_spi_t compat_spi_msg;

	mutex_lock(&device_list_lock);	/* TODO:check wether the lock is OK */

	switch (cmd) {
	case COMPAT_MXDSPI_IOC_RW:
	case COMPAT_MXDSPI_IOC_R:
	case COMPAT_MXDSPI_IOC_W:
		retval = copy_from_user((unsigned char *)&compat_spi_msg,
		(unsigned char *)arg, sizeof(struct compat_mxdchar_spi_t));
		if (retval == 0) {
			spi_msg.txbuffer = (unsigned char *)(uintptr_t)
				(compat_spi_msg.txbuffer);
			spi_msg.rxbuffer = (unsigned char *)(uintptr_t)
				(compat_spi_msg.rxbuffer);
			spi_msg.txlength = compat_spi_msg.txlength;
			spi_msg.rxlength = compat_spi_msg.rxlength;
			cmd = MXDSPI_COMPAT_TO_NORM(cmd);
			retval = do_mxdspidev_ioctl(filp, cmd, &spi_msg);
		}
		break;

	default:
		LOG_INFO("MXDSPI_IOC cmd:%x not support\n", cmd);
		break;
	}

	mutex_unlock(&device_list_lock);	/* TODO: Check it */

	return retval;
}
static int mxdspidev_open(struct inode *inode, struct file *filp)
{
	struct cmmbspi_dev *cmmb_dev;
	int status = -ENXIO;

	mutex_lock(&device_list_lock);

	list_for_each_entry(cmmb_dev, &device_list, device_entry) {
		if (cmmb_dev->devt == inode->i_rdev) {
			status = 0;
			break;
		}
	}

	if (status == 0) {
		if (!cmmb_dev->users) {
			LOG_INFO("mxd chip power on\n");
			chip_power_on(cmmb_dev);
		}

		cmmb_dev->users++;
		filp->private_data = cmmb_dev;
		nonseekable_open(inode, filp);
	} else
		LOG_ERR("mxdspidev: nothing for minor %d\n", iminor(inode));

	mutex_unlock(&device_list_lock);

	return status;
}

static int mxdspidev_release(struct inode *inode, struct file *filp)
{
	struct cmmbspi_dev *cmmb_dev;
	int status = 0;

	cmmb_dev = filp->private_data;
	if (cmmb_dev == NULL) {
		LOG_ERR("release err:cmmb_dev is NULL\n");
		return status;
	}

	mutex_lock(&device_list_lock);
	filp->private_data = NULL;

	/* last close? */
	cmmb_dev->users--;
	if (!cmmb_dev->users) {
		chip_power_down(cmmb_dev);
		LOG_INFO("mxd chip power off\n");
	}
	mutex_unlock(&device_list_lock);

	return status;
}

/* ---------------------------------------------------------------------- */
/*
 * Interrupt handler stuff
 */
#if 0
static irqreturn_t mxdspidev_irq(int irq, void *data)
{
	struct cmmbspi_dev *cmmb_dev = data;
	int frame = 0;

	spin_lock(&cmmb_dev->spi_lock);
	/*
	 * Basic frame housekeeping.
	 */
	if (test_bit(frame, &cmmb_dev->irq_flag) && pr_info_ratelimited()) {
		/* LOG_INFO("Frame overrun on %d, frames lost\n", frame); */
		;
	}
	set_bit(frame, &cmmb_dev->irq_flag);

	spin_unlock(&cmmb_dev->spi_lock);

	wake_up_interruptible(&cmmb_dev->iowait);

	return IRQ_HANDLED;
}
#endif

static unsigned int mxdspidev_poll(struct file *filp,
				   struct poll_table_struct *pt)
{
	int frame = 0;
	int data_ready = 0;
	struct cmmbspi_dev *cmmb_dev;

	cmmb_dev = filp->private_data;

	poll_wait(filp, &cmmb_dev->iowait, pt);

	spin_lock(&cmmb_dev->spi_lock);	/* TODO: not need? */
	data_ready = test_and_clear_bit(frame, &cmmb_dev->irq_flag);
	spin_unlock(&cmmb_dev->spi_lock);

	if (data_ready)
		return POLLIN | POLLRDNORM;
	else
		return 0;
}

const struct file_operations mxdspidev_fops = {
	.owner = THIS_MODULE,
	/* REVISIT switch to aio primitives, so that userspace
	 * gets more complete API coverage.  It'll simplify things
	 * too, except for the locking.
	 */
	.write = mxdspidev_write,
	.read = mxdspidev_read,
	.poll = mxdspidev_poll,
	.unlocked_ioctl = mxdspidev_ioctl,
	.compat_ioctl = compat_mxdspidev_ioctl,
	.open = mxdspidev_open,
	.release = mxdspidev_release,
};

/*-------------------------------------------------------------------------*/

/* The main reason to have this class is to make mdev/udev create the
 * /dev/mxdspidevB.C character device nodes exposing our userspace API.
 * It also simplifies memory management.
 */

static struct class *mxdspidev_class;

/*-------------------------------------------------------------------------*/

static int mxdspidev_probe(struct spi_device *spi)
{
	struct cmmbspi_dev *cmmb_dev;
	int status;
	unsigned long minor;
#ifndef CONFIG_OF
	struct cmmb_platform_data *pdata;
#endif
	int ret;

#ifdef CONFIG_OF
	if (of_property_read_u8(spi->dev.of_node, "bits_per_word",
			&spi->bits_per_word))
		spi->bits_per_word = MXD_SPI_BITS_PER_WORD;

	if (of_property_read_u8(spi->dev.of_node, "mode",
			&spi->mode))
		spi->mode = 0;
#else
	spi->bits_per_word = MXD_SPI_BITS_PER_WORD;
	pdata = spi->dev.platform_data;
	if (!pdata) {
		ret = -EINVAL;
		goto out;
	}
#endif
	ret = spi_setup(spi);
	if (ret < 0)
		goto out;
	LOG_DEBUG("Spi setup OK\n");

	/* Allocate driver data */
	cmmb_dev = kzalloc(sizeof(struct cmmbspi_dev), GFP_KERNEL);
	if (!cmmb_dev) {
		ret = -ENOMEM;
		goto out;
	}

	/* Initialize the driver data */
	cmmb_dev->spi = spi;

	spin_lock_init(&cmmb_dev->spi_lock);
	mutex_init(&cmmb_dev->buf_lock);
	INIT_LIST_HEAD(&cmmb_dev->device_entry);
	init_waitqueue_head(&cmmb_dev->iowait);

	/* Initialize chip control pins */
#if 0
#ifndef PIN_DEFINED_BY_DRIVE
	if (pdata->gpio_defined) {
		cmmb_dev->gpio_power = pdata->gpio_power;
		cmmb_dev->gpio_reset = pdata->gpio_reset;
		cmmb_dev->gpio_cs = pdata->gpio_cs;
	} else
#endif /* in case the pdata is not set outsize the dirver */
#endif
	{
		cmmb_dev->gpio_power = POWER_PIN_NUM;
		cmmb_dev->gpio_reset = RESET_PIN_NUM;
		cmmb_dev->gpio_cs = SPI_CS_PIN_NUM;
	}

	/* Alloc two dma buf for SPI tx and rx internally */
	ret = chip_alloc_dma_bufs(cmmb_dev);
	if (ret)
		goto out_alloc_dma;

	LOG_DEBUG("chip_alloc_dma_bufs OK\n");

	/* Initialize the interrupt pin for chip */
	cmmb_dev->irq = spi->irq;
#if 0
#if 1
	irq_set_irq_type(cmmb_dev->irq, IRQ_TYPE_EDGE_FALLING);
	/* IRQ_TYPE_EDGE_RISING*/

	ret = request_irq(cmmb_dev->irq, mxdspidev_irq,
			  IRQF_DISABLED, "CMMB Demodulator", cmmb_dev);
#else
	ret = request_irq(cmmb_dev->irq, mxdspidev_irq,
			  IRQF_TRIGGER_RISING, "CMMB Demodulator", cmmb_dev);
#endif
#endif
	if (ret) {
		LOG_ERR("CMMB request irq failed.\n");
		goto out_request_irq;
	}
	LOG_DEBUG("request_irq OK\n");

	/* If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *dev;

		cmmb_dev->devt = MKDEV(mxdspi_major, minor);
		dev = device_create(mxdspidev_class, &spi->dev, cmmb_dev->devt,
				    cmmb_dev, "mxdspidev1.%ld", minor);
		status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
	} else {
		dev_dbg(&spi->dev, "no minor number available!\n");
		status = -ENODEV;
	}

	if (status == 0) {
		set_bit(minor, minors);
		list_add(&cmmb_dev->device_entry, &device_list);
	}
	mutex_unlock(&device_list_lock);

	if (status == 0)
		spi_set_drvdata(spi, cmmb_dev);
	else
		goto out_creat_cdev;
	LOG_DEBUG("device_create OK\n");

	return status;

out_creat_cdev:
#if 0
	free_irq(spi->irq, cmmb_dev);
#endif
out_request_irq:
	chip_free_dma_bufs(cmmb_dev);
out_alloc_dma:
	kfree(cmmb_dev);
out:
	LOG_ERR("mxdspidev probe fail\n");
	return ret;
}

static int mxdspidev_remove(struct spi_device *spi)
{
	struct cmmbspi_dev *cmmb_dev = spi_get_drvdata(spi);

	/* make sure ops on existing fds can abort cleanly */
	spin_lock_irq(&cmmb_dev->spi_lock);
	cmmb_dev->spi = NULL;
	spi_set_drvdata(spi, NULL);
	spin_unlock_irq(&cmmb_dev->spi_lock);

	/* prevent new opens */
	mutex_lock(&device_list_lock);
	list_del(&cmmb_dev->device_entry);
	device_destroy(mxdspidev_class, cmmb_dev->devt);
	clear_bit(MINOR(cmmb_dev->devt), minors);
#if 0
	free_irq(cmmb_dev->irq, cmmb_dev);
#endif
	chip_free_dma_bufs(cmmb_dev);
	kfree(cmmb_dev);
	mutex_unlock(&device_list_lock);
	LOG_DEBUG("mxdspidev_remove\n");
	return 0;
}

static struct of_device_id mxdspi_pxa_dt_ids[] = {
	{ .compatible = "maxscend,cmmb", NULL },
	{}
};
MODULE_DEVICE_TABLE(of, mxdspi_pxa_dt_ids);

static struct spi_driver mxdspidev_spi = {
	.driver = {
		   .name = DRIVER_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = mxdspi_pxa_dt_ids,
		   },
	.probe = mxdspidev_probe,
	.remove = mxdspidev_remove,

	/* NOTE:  suspend/resume methods are not necessary here.
	 * We don't do anything except pass the requests to/from
	 * the underlying controller.  The refrigerator handles
	 * most issues; the controller driver handles the rest.
	 */
};

/*-------------------------------------------------------------------------*/

static int __init mxdspidev_init(void)
{
	int status;

	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */
	BUILD_BUG_ON(N_SPI_MINORS > 256);

	LOG_DEBUG("mxdspidev_init\n");

	status = register_chrdev(mxdspi_major, "spi", &mxdspidev_fops);
	if (status < 0)
		return status;
	else if (mxdspi_major == 0)
		mxdspi_major = status;
	LOG_DEBUG("mxdspi major is %d\n", mxdspi_major);

	mxdspidev_class = class_create(THIS_MODULE, "mxdspidev");
	if (IS_ERR(mxdspidev_class)) {
		unregister_chrdev(mxdspi_major, mxdspidev_spi.driver.name);
		return PTR_ERR(mxdspidev_class);
	}

	status = spi_register_driver(&mxdspidev_spi);
	if (status < 0) {
		class_destroy(mxdspidev_class);
		unregister_chrdev(mxdspi_major, mxdspidev_spi.driver.name);
	}
	return status;
}
module_init(mxdspidev_init);

static void __exit mxdspidev_exit(void)
{
	spi_unregister_driver(&mxdspidev_spi);
	class_destroy(mxdspidev_class);
	unregister_chrdev(mxdspi_major, mxdspidev_spi.driver.name);
}
module_exit(mxdspidev_exit);

MODULE_AUTHOR("Jialing Fu");
MODULE_DESCRIPTION("SPI IF for Maxscend CMMB module");
MODULE_LICENSE("GPL");
