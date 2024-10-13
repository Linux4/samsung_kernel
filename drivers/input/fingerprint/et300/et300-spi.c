/*
 * Simple synchronous userspace interface to SPI devices
 *
 * Copyright (C) 2006 SWAPP
 *	Andrea Paterniani <a.paterniani@swapp-eng.it>
 * Copyright (C) 2007 David Brownell (simplification, cleanup)
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

#include "et300_data_transfer.h"
#include "et300.h"
#include "et300_define_jadeOpc.h"
#include "sprd_fps_trigger.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <soc/sprd/gpio.h>


extern int et300_wake;
extern int _fIrq;
extern int et300_wake_lock_status;

static DECLARE_BITMAP(minors, N_SPI_MINORS);

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static unsigned bufsiz = 4096;
module_param(bufsiz, uint, S_IRUGO);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");

struct device *esfp0_dev;

static inline void FPS_reset(void)
{
	int gpio_reset = -1;
	int rv = 0;

	gpio_reset = sprd_et300_parse_dts_gpio(2);
	if(gpio_reset < 0){
		printk("sprd:%s,Failed to get gpio_reset gpio from dts: %d.\n",__func__,gpio_reset);
	}

	rv = gpio_request(gpio_reset,"gpio_et300_reset");
	if (rv < 0){
		printk("sprd:%s:gpio_request failed!\n",__func__);
	}

	rv = gpio_direction_output(gpio_reset, 1);
	if(rv){
		printk("sprd:%s,gpio_direction_output failed.\n",__func__);
	}

	gpio_set_value(gpio_reset,0);
	msleep(10);
	gpio_set_value(gpio_reset,1);
}

static inline void FPS_reset_low(void)
{
	int gpio_reset = -1;
	int rv = 0;
	gpio_reset = sprd_et300_parse_dts_gpio(2);
	if(gpio_reset < 0){
		printk("sprd:%s,Failed to get gpio_reset gpio from dts: %d.\n",__func__,gpio_reset);
	}
	rv = gpio_request(gpio_reset,"gpio_et300_reset");
	if (rv < 0){
		printk("sprd:%s:gpio_request failed!\n",__func__);
	}
	rv = gpio_direction_output(gpio_reset, 1);
	if(rv){
		printk("sprd:%s,gpio_direction_output failed.\n",__func__);
	}
	gpio_set_value(gpio_reset,0);
}

//For test
static ssize_t
et300_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	int i = 0;
	struct et300_data *et300;
	u8 result[2] = {0xFF,};
	int status;
	size_t size = 0;

	DEBUG_PRINT("%s", __func__);

	/* chipselect only toggles at start or end of operation */
	if (count > bufsiz)
		return -EMSGSIZE;

	et300 = filp->private_data;
	mutex_lock(&et300->buf_lock);

	status = et300_read_register(et300, 0x29, result);
	for(i=0;i<2;i++)
	{
		DEBUG_PRINT("%s result[%d] = 0x%x", __func__, i, result[i]);
	}
	if(status == 0)
	{
		unsigned long	missing;

		size = 1;
		missing = copy_to_user(buf, result+1, count);
		DEBUG_PRINT("et300_read mising = %lu", missing);
		if(missing != 0)
			size = -EFAULT;
	}
	else
		size = -EFAULT;
	mutex_unlock(&et300->buf_lock);
	return size;
}

/* Write-only message with current device setup */
//For test
static ssize_t
et300_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	int i=0;
	struct et300_data	*et300;
	ssize_t			status = 0;
	unsigned long		missing;

	DEBUG_PRINT("%s\n", __func__);

	/* chipselect only toggles at start or end of operation */
	if (count > bufsiz)
		return -EMSGSIZE;

	et300 = filp->private_data;

	mutex_lock(&et300->buf_lock);
	missing = copy_from_user(et300->buffer, buf, count);
/*
#ifdef ET300_SPI_DEBUG
	DEBUG_PRINT("et300_write mising = %lu", missing);
	for(i=0;i<count;i++)
		DEBUG_PRINT("et300_write buf = %x", et300->buffer[i]);
#endif
*/
	if (missing == 0)
		status = et300_write_register(et300, et300->buffer[0], et300->buffer[1]);
	else
		status = -EFAULT;

	mutex_unlock(&et300->buf_lock);

//	DEBUG_PRINT("et300_write status = %u", status);

	return status;
}

static long
et300_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int			err = 0;
	int			retval = 0;
	struct et300_data	*et300;
	struct spi_device	*spi;
	u32			tmp;
	unsigned		n_ioc;
	struct egis_ioc_transfer	*ioc = NULL;
	u32	save=1000000;

	/* Check type and command number */
	if (_IOC_TYPE(cmd) != EGIS_IOC_MAGIC){
		printk(KERN_ERR "%s _IOC_TYPE(cmd) != EGIS_IOC_MAGIC", __func__);
		return -ENOTTY;
	}

	/* Check access direction once here; don't repeat below.
	 * IOC_DIR is from the user perspective, while access_ok is
	 * from the kernel perspective; so they look reversed.
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,
				(void __user *)arg, _IOC_SIZE(cmd));
	if (err == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ,
				(void __user *)arg, _IOC_SIZE(cmd));
	if (err){
		printk(KERN_ERR "%s err", __func__);
		return -EFAULT;
	}

	/* guard against device removal before, or while,
	 * we issue this ioctl.
	 */
	et300 = filp->private_data;
	spin_lock_irq(&et300->spi_lock);
	spi = spi_dev_get(et300->spi);
	spin_unlock_irq(&et300->spi_lock);

	if (spi == NULL){
		printk(KERN_ERR "%s spi == NULL", __func__);
		return -ESHUTDOWN;
	}

	mutex_lock(&et300->buf_lock);

	/* segmented and/or full-duplex I/O request */
	if (_IOC_NR(cmd) != _IOC_NR(EGIS_IOC_MESSAGE(0))
			|| _IOC_DIR(cmd) != _IOC_WRITE) 
	{
		retval = -ENOTTY;
		goto out;
	}

	tmp = _IOC_SIZE(cmd);
	if ((tmp % sizeof(struct egis_ioc_transfer)) != 0)
	{
		retval = -EINVAL;
		goto out;
	}
	n_ioc = tmp / sizeof(struct egis_ioc_transfer);
	if (n_ioc == 0)
		goto out;

	/* copy into scratch area */
	ioc = kmalloc(tmp, GFP_KERNEL);
	if (!ioc) {
		retval = -ENOMEM;
		goto out;
	}
	if (__copy_from_user(ioc, (void __user *)arg, tmp))
	{
		retval = -EFAULT;
		goto out;
	}

	//Set clock rate of SPI controller
	//Ues egis_ioc_transfer->len as speed
	if(ioc->opcode == 99)
	{
		printk(KERN_INFO "Modify speed = %d\n", ioc->len);
		save = spi->max_speed_hz;
		tmp = ioc->len;

		spi->max_speed_hz = tmp;
		retval = spi_setup(spi);
		if (retval < 0)
		{
			spi->max_speed_hz = save;
			printk(KERN_ERR "setup speed error");
			goto out;
		}
		else
		{
			DEBUG_PRINT("%d Hz (original)\n", save);
			DEBUG_PRINT("%d Hz (max)\n", tmp);
		}
	}

	/*
	 * Read registers
	 * tx_buf include all registers' address will be read
	 * structure of tx_buf is [ address1, address2 ...]
	 */
	if(ioc->opcode == JADE_REGISTER_MASSREAD)
	{
		u8* address = ioc->tx_buf;
		u8* result = ioc->rx_buf;
		DEBUG_PRINT("et300 JADE_REGISTER_MASSREAD\n");

		retval = et300_io_bulk_read(et300, address, result, ioc->len);
		if(retval < 0)
		{
			printk(KERN_ERR "%s JADE_REGISTER_MASSREAD call et300_io_bulk_read error retval = %d", __func__, retval);
			goto out;
		}
	}

	/*
	 * Write data to registers
	 * tx_buf includes address and value will be wrote
	 * structure of tx_buf is [{ address1, value1 }, {address2, value2} ...]
	 */
	if(ioc->opcode == JADE_REGISTER_MASSWRITE)
	{
		u8* buf = ioc->tx_buf;
		DEBUG_PRINT("et300 JADE_REGISTER_MASSWRITE\n");

		retval = et300_io_bulk_write(et300, buf, ioc->len);
		if(retval < 0)
		{
			printk(KERN_ERR "%s JADE_REGISTER_MASSWRITE call et300_io_bulk_write error retval = %d", __func__, retval);
			goto out;
		}
	}

	/*
	 * Get one frame (192*4) data from chip
	 */
	if(ioc->opcode == JADE_GET_ONE_IMG)
	{
		u8* buf = ioc->tx_buf;
		u8* image_buf = ioc->rx_buf;
		DEBUG_PRINT("et300 JADE_GET_ONE_IMG\n");

		retval = et300_get_one_image(et300, buf, image_buf);
		if(retval < 0)
		{
			printk(KERN_ERR "%s JADE_GET_ONE_IMG call et300_get_one_image error retval = %d", __func__, retval);
			goto out;
		}
	}


		//
		// GPIO Initial FPS Device
		//
	if(ioc->opcode == GPIO_INIT_FPS_DEVICE)
	{
/*
		printk("GPIO_INIT_FPS_DEVICE\n");
		retval = gpio_request(GPIO_FPS_POWER_ONOFF, "fpspower");
		if(retval)
		{
			printk("request GPIO for fpspower failed\n");
			return retval;
		}
		s3c_gpio_cfgpin(GPIO_FPS_POWER_ONOFF, S3C_GPIO_OUTPUT);
*/
	}
	//
	// release FPS GPIO handle
	//
	if(ioc->opcode == GPIO_FREE_FPS_DEVICE)
	{
/*
		gpio_free(GPIO_FPS_POWER_ONOFF);
*/
	}

	//
	// Power on fps device or trigger something.
	//
	if(ioc->opcode == POWER_ON_FPS_DEVICE)
	{
		FPS_reset();
	}

	//
	// Power off fps device or trigger something.
	//
	if(ioc->opcode == POWER_OFF_FPS_DEVICE)
	{
		FPS_reset_low();
	}

	//
	// Trigger inital routine
	//
	if(ioc->opcode == INT_TRIGGER_INIT)
	{
		DEBUG_PRINT("ET300 INT_TRIGGER_INIT\n");
		u8* trigger_buf = ioc->rx_buf;
		retval = Interrupt_Init(); 
		retval = fps_interrupt_read(filp,trigger_buf,1,NULL);
		printk(KERN_ERR "trigger init = %d\n",  retval);
	}
	//
	// trigger 
	//
	if(ioc->opcode == INT_TRIGGER_CLOSE)
	{
		DEBUG_PRINT("ET300 INT_TRIGGER_CLOSE\n");
		retval = Interrupt_Free();
		printk(KERN_ERR "trigger close = %d\n", retval);
	}

	//
	// Exit que
	//
/*
	if(ioc->opcode == INT_TRIGGER_EXIT)
	{
		printk("<<< ET300 Trigger function close\n");
		retval = Interrupt_Exit();
		printk(KERN_ERR "trigger close = %d\n", retval);
	}
*/

//
// read interrupt status
//
	if(ioc->opcode == INT_TRIGGER_READ)
	{
		u8* trigger_buf = ioc->rx_buf;
		//filp->f_flags |= O_NONBLOCK;
		retval = fps_interrupt_read(filp,trigger_buf,1,NULL);   // TBD, maybe we can more trigger line 
	}

  //
  //  polling interrupt status
  // 
	if(ioc->opcode == INT_TRIGGER_POLLING)
	{
//			poll_table_struct wait;
//			u8* trigger_buf = ioc->rx_buf;
//			retval = fps_interrupt_poll(filp,&wait);
	}

out:
	if(ioc)
		kfree(ioc);

	mutex_unlock(&et300->buf_lock);
	spi_dev_put(spi);
	if(retval < 0)
	{
		printk(KERN_ERR "%s retval = %d", __func__, retval);
		//retval = -1;
	}
	return retval;
}

#ifdef CONFIG_COMPAT
static long
et300_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return et300_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define et300_compat_ioctl NULL
#endif /* CONFIG_COMPAT */

static int et300_open(struct inode *inode, struct file *filp)
{
	struct et300_data	*et300;
	int			status = -ENXIO;

	DEBUG_PRINT("%s", __func__);

	mutex_lock(&device_list_lock);

	list_for_each_entry(et300, &device_list, device_entry)
	{
		if (et300->devt == inode->i_rdev)
		{
			status = 0;
			break;
		}
	}
	if (status == 0)
	{
		if (!et300->buffer)
		{
			et300->buffer = kmalloc(bufsiz, GFP_KERNEL);
			if (!et300->buffer)
			{
				DEBUG_PRINT("%s.open/ENOMEM\n",__func__);
				status = -ENOMEM;
			}
		}
		if (status == 0)
		{
			et300->users++;
			filp->private_data = et300;
			nonseekable_open(inode, filp);
		}
	} 
	else
		pr_debug("et300: nothing for minor %d\n", iminor(inode));

	mutex_unlock(&device_list_lock);
	return status;
}

static int et300_release(struct inode *inode, struct file *filp)
{
	struct et300_data	*et300;
	int			status = 0;


	DEBUG_PRINT("%s", __func__);
	mutex_lock(&device_list_lock);
	et300 = filp->private_data;
	filp->private_data = NULL;

	/* last close? */
	et300->users--;
	if (!et300->users)
	{
		int		dofree;

		kfree(et300->buffer);
		et300->buffer = NULL;

		/* ... after we unbound from the underlying device? */
		spin_lock_irq(&et300->spi_lock);
		dofree = (et300->spi == NULL);
		spin_unlock_irq(&et300->spi_lock);

		if (dofree){
			DEBUG_PRINT("%s,dofree = %x.free et300 \n",__func__,dofree);
			kfree(et300->work_buf);
			kfree(et300->read_data);
			kfree(et300);
		}
	}
	mutex_unlock(&device_list_lock);
//	mt_set_gpio_out(GPIO_FPS_POWER_PIN, GPIO_OUT_ZERO);

	return status;
}

static const struct file_operations et300_fops =
{
	.owner =	THIS_MODULE,
	.write =	et300_write,
	.read =		et300_read,
	.unlocked_ioctl = et300_ioctl,
	.compat_ioctl = et300_compat_ioctl,
	.open =		et300_open,
	.release =	et300_release,
	.llseek =	no_llseek,
};

/*-------------------------------------------------------------------------*/

static struct class *et300_class;

/*-------------------------------------------------------------------------*/

static int et300_probe(struct spi_device *spi)
{

	struct et300_data	*et300;
	int			status;
	unsigned long		minor;
	int error = 0;
	struct device *dev;

	DEBUG_PRINT("%s", __func__);

	error = sprd_et300_3v3ldo_enable(1);
	if(error){
		printk("sprd:%s,3v3ldo enable failed.error = %d.\n",__func__,error);
		return error;
	}
	/* Allocate driver data */
	et300 = kzalloc(sizeof(*et300), GFP_KERNEL);
	if (!et300)
		return -ENOMEM;

	et300->read_data = (u8 *) kzalloc(ET300_READ_BUF_SZ + 1, GFP_KERNEL);
	if(et300->read_data == NULL){
		DEBUG_PRINT("%s,%d,kzalloc buff for spi read_data failed.\n",__func__,__LINE__);
		kfree(et300);
		return -ENOMEM;
	}
	et300->work_buf= (uint8_t *) kzalloc(ET300_READ_BUF_SZ, GFP_KERNEL);
	if(et300->work_buf == NULL){
		DEBUG_PRINT("%s,%d,kzalloc buff for spi read_data failed.\n",__func__,__LINE__);
		kfree(et300->read_data);
		kfree(et300);
		return -ENOMEM;
	}

	/* Initialize the driver data */
	et300->spi = spi;

	spi->max_speed_hz = 12 * 1000 * 1000;
	spi_setup(spi);
	spin_lock_init(&et300->spi_lock);
	mutex_init(&et300->buf_lock);

	INIT_LIST_HEAD(&et300->device_entry);

	/* If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS)
	{

		et300->devt = MKDEV(ET300_MAJOR, minor);
//		dev = device_create(et300_class, &spi->dev, et300->devt,
                dev = device_create(et300_class, NULL, et300->devt,
//				    spidev, "spidev%d.%d",
//				    spi->master->bus_num, spi->chip_select);
				    et300, "esfp0");
		status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
		if(status)
			goto dev_error;
	}
	else
	{
		dev_dbg(&spi->dev, "et300 no minor number available!\n");
		status = -ENODEV;
		goto dev_error;
	}
	if (status == 0)
	{
		set_bit(minor, minors);
		list_add(&et300->device_entry, &device_list);
	}
	mutex_unlock(&device_list_lock);
	spi_set_drvdata(spi, et300);

	esfp0_dev = dev;

	FPS_reset();    // reset FPS device

	msleep(100);
	Interrupt_Init();

	return status;

dev_error:
	kfree(et300->work_buf);
	kfree(et300->read_data);
	kfree(et300);
	return status;
}

void et300_device_change()
{
    kobject_uevent(&esfp0_dev->kobj, KOBJ_CHANGE);

}

static int et300_remove(struct spi_device *spi)
{
	struct et300_data	*et300 = spi_get_drvdata(spi);
	DEBUG_PRINT("%s", __func__);

	/* make sure ops on existing fds can abort cleanly */
	spin_lock_irq(&et300->spi_lock);
	et300->spi = NULL;
	spi_set_drvdata(spi, NULL);
	spin_unlock_irq(&et300->spi_lock);

	/* prevent new opens */
	mutex_lock(&device_list_lock);
	list_del(&et300->device_entry);
	device_destroy(et300_class, et300->devt);
	clear_bit(MINOR(et300->devt), minors);
	if (et300->users == 0)
	{
		kfree(et300->work_buf);
		kfree(et300->read_data);
		kfree(et300);
	}
	mutex_unlock(&device_list_lock);

	return 0;
}

static int et300_suspend(struct spi_device *spi, pm_message_t mesg)
{
	DEBUG_PRINT("%s\n", __func__);
	et300_wake = 0;
	return 0;
}

static int et300_resume(struct spi_device *spi)
{
	DEBUG_PRINT("%s\n", __func__);
	et300_wake = 1;
	if(et300_wake_lock_status){
		et300_wake_lock_status = 0;
	}
	return 0;
}

static struct spi_driver et300_spi_driver =
{
	.driver =
	{

		.name =		"et300",
		.owner =	THIS_MODULE,
	},
	.probe =		et300_probe,
	.remove =	et300_remove,
	.suspend =	et300_suspend,
	.resume =	et300_resume,
};

/*-------------------------------------------------------------------------*/

static struct spi_board_info et300_spi[] __initdata = {
        [0] ={
		.modalias			= "et300",
		.bus_num		= 1,
		.chip_select		= 0,
		.max_speed_hz	= 15000000,
		.mode			= SPI_MODE_3,
	}
};


static int __init et300_init(void)
{
	int status;
	int retval = 0;
	DEBUG_PRINT("%s", __func__);

	printk(KERN_ERR "driver loading message %s", __func__);

	retval = spi_register_board_info(et300_spi, ARRAY_SIZE(et300_spi));


	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */
	BUILD_BUG_ON(N_SPI_MINORS > 256);

	status = register_chrdev(ET300_MAJOR, "spi", &et300_fops);
	if (status < 0)
		return status;

	et300_class = class_create(THIS_MODULE, "et300");
	if (IS_ERR(et300_class))
	{
		unregister_chrdev(ET300_MAJOR, et300_spi_driver.driver.name);
		return PTR_ERR(et300_class);
	}

	status = spi_register_driver(&et300_spi_driver);
	if (status < 0)
	{
		class_destroy(et300_class);
		unregister_chrdev(ET300_MAJOR, et300_spi_driver.driver.name);
	}
	return status;
}


module_init(et300_init);

static void __exit et300_exit(void)
{
	DEBUG_PRINT("%s", __func__);
//	gpio_free(GPIO_FPS_POWER_ONOFF);
	spi_unregister_driver(&et300_spi_driver);
	class_destroy(et300_class);
	unregister_chrdev(ET300_MAJOR, et300_spi_driver.driver.name);
}
module_exit(et300_exit);

MODULE_AUTHOR("Wang YuWei, <robert.wang@egistec.com>");
MODULE_DESCRIPTION("SPI Interface for ET300");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:et300");
