/*
 *  DFU driver interface for firmware upgrade
 *
 *  Copyright (c) 2010 N-TRIG
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/usb.h>

#include "typedef-ntrig.h"
#include "ntrig-common.h"
#include "ntrig-dispatcher.h"


#define DRIVER_VERSION "N-TRIG Bulk Driver Version 1.00"

#ifdef CONFIG_USB_DYNAMIC_MINORS
#define USB_NTRIG_MINOR_BASE 0
#else
/* FIXME 186 - is another driver's minor - apply for that */
#define USB_NTRIG_MINOR_BASE 186
#endif

#define N_TRIG_BOOT_LOADER_ID 0x02
#define BULK_IN_MAX_MSG_LEN 1024
#define IOCTL_GET_HARD_VERSION 1
#define IOCTL_GET_DRV_VERSION 2
/**
 * Ctrl Endpoint header declaration
 */
#define REQUEST_CMD 0
#define BULK_GROUP_ID 1
#define BULK_CODE 2
#define BULK_LEN_LSB 3
#define BULK_LEN_MSB 4
#define BULK_DATA 5

#define CMD_WRITE_MESSAGE 0x0F
#define CMD_GO2_BOOT_LOADER 0x0B

#define CTL_REQUEST_TYPE 0x40
#define NTRIG_INT_PACKET_LENGTH 10
#define MAX_TRANSMIT_FRAGMENT_SIZE 64
static struct usb_device_id id_table[] = {
	{ .idVendor = 0x1b96, .match_flags = USB_DEVICE_ID_MATCH_VENDOR, },
	{ },
};
MODULE_DEVICE_TABLE(usb, id_table);

static DEFINE_MUTEX(open_disc_mutex);

struct usb_ntrig {
	struct usb_device *udev;
	struct usb_interface *interface;
	unsigned char *bulk_in_buffer;
	size_t bulk_in_size;
	__u8 bulk_in_endpointAddr;
	__u8 bulk_out_endpointAddr;
	__u8 product_id;
	struct kref kref;
	struct usb_anchor submitted;
	__u8 sensor_id;
};
#define to_ntrig_dev(d) container_of(d, struct usb_ntrig, kref)

static struct usb_driver ntrig_driver;

static void ntrig_delete(struct kref *kref)
{
	struct usb_ntrig *dev = to_ntrig_dev(kref);
ntrig_dbg("N-trig bulk: %s\n", __func__);
	usb_put_dev(dev->udev);
	/* We don't want to enable Autosuspend while */
	/* Firmware upgrade procedure */
	kfree(dev->bulk_in_buffer);
	kfree(dev);
}


static int ntrig_open(struct inode *inode, struct file *file)
{
	struct usb_ntrig *dev;
	struct usb_interface *interface;
	int subminor, r;
	subminor = iminor(inode);
	ntrig_dbg("N-trig bulk: %s\n", __func__);
	interface = usb_find_interface(&ntrig_driver, subminor);
	if (!interface) {
		ntrig_err(
			"N-trig bulk: %s - error, can't find device for "
			"minor %d\n", __func__, subminor);
		return -ENODEV;
	}
	mutex_lock(&open_disc_mutex);
	dev = usb_get_intfdata(interface);
	if (!dev) {
		mutex_unlock(&open_disc_mutex);
		return -ENODEV;
	}
	/* increment our usage count for the device */
	kref_get(&dev->kref);
	mutex_unlock(&open_disc_mutex);
	/* grab a power reference */
	r = usb_autopm_get_interface(interface);
	if (r < 0) {
		ntrig_err("N-trig bulk: %s - error, grab a power reference\n",
			__func__);
		kref_put(&dev->kref, ntrig_delete);
		return r;
	}
	/* save our object in the file's private structure */
	file->private_data = dev;
	return 0;
}

static int ntrig_release(struct inode *inode, struct file *file)
{
	struct usb_ntrig *dev;
	dev = (struct usb_ntrig *)file->private_data;
	ntrig_dbg("N-trig bulk: %s\n", __func__);
	if (dev == NULL) {
		ntrig_dbg("N-trig bulk: %s: dev == NULL\n", __func__);
		return -ENODEV;
	}
	/*decrement the count on our device*/
	/*We Can't But there is still ntrig-dfu communicate with him*/
	/*We don't want to enable autosusppend*/
	/*becuase we don't when we finish work we driver we take it out*/
	kref_put(&dev->kref, ntrig_delete);
	return 0;
}

int NTRIGReadNCP(void *raw_dev, char *buf, size_t count)
{
	struct usb_ntrig *dev = (struct usb_ntrig *)raw_dev;
	int retval = 0;
	int bytes_read;

	ntrig_dbg("N-trig bulk: %s\n", __func__);
	/* do a blocking bulk read to get data from the device */
	retval = usb_bulk_msg(dev->udev,
		usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointAddr),
		dev->bulk_in_buffer,
		min(dev->bulk_in_size, count),
		&bytes_read, 10000);

	/* if the read was successful, copy the data to userspace */
	if (!retval) {
		/* allocated buffer too small */
		if (count < bytes_read) {
			ntrig_err(
				"N-trig bulk: %s - error, count (%d) < "
				"bytes_read (%d)\n",
				__func__, count, bytes_read);
			return DTRG_FAILED;
		}

		/* buf is safely kallocated by ncp char driver read function,
		 * therefore no need to call copy_to_user */
		memcpy(buf, dev->bulk_in_buffer, bytes_read);
		retval = bytes_read;
	}
	return retval;
}

static void ntrig_write_bulk_callback(struct urb *urb)
{
	struct usb_ntrig *dev;
	int status = urb->status;
	dev = urb->context;
ntrig_err("N-trig bulk: %s\n", __func__);
	/* sync/async unlink faults aren't errors */
	if (status &&
		!(status == -ENOENT || status == -ECONNRESET ||
			status == -ESHUTDOWN)) {
		ntrig_dbg(
			"N-trig bulk: %s - nonzero write bulk status received: "
			"%d\n", __func__, status);
	}
	/* free up our allocated buffer */
	usb_free_coherent(urb->dev, urb->transfer_buffer_length,
		urb->transfer_buffer, urb->transfer_dma);
}

int NTRIGWriteNCP(void *raw_dev, const char __user *user_buffer, short count)
{
	struct usb_ntrig *dev = (struct usb_ntrig *)raw_dev;
	int retval = DTRG_NO_ERROR;
	struct urb *urb = NULL;
	char *buf = NULL;

	printk("N-trig bulk driver, %s\n", __func__);
	ntrig_err("N-trig bulk: %s, count = %d\n", __func__, count);
	/* verify that we actually have some data to write */
	if (count == 0)
		goto exit;

	ntrig_err(
		"N-trig bulk: %s, N_TRIG_BOOT_LOADER_I = %d, dev->product_id = "
		"%d\n", __func__, N_TRIG_BOOT_LOADER_ID, dev->product_id);
	/* create a urb, and a buffer for it, and copy the data to the urb */
	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		retval = -ENOMEM;
		ntrig_err("N-trig bulk: %s,err_no_buf\n", __func__);
		goto err_no_buf;
	}
	buf = usb_alloc_coherent(dev->udev, count, GFP_KERNEL,
		&urb->transfer_dma);
	if (!buf) {
		retval = -ENOMEM;
		ntrig_err("N-trig bulk: %s,ENOMEM\n", __func__);
		goto error;
	}

	/* No need to copy_from_user - user_buffer is allocated and copied from
	 * user in ncp_driver */
	memcpy(buf, user_buffer, count);

	/* initialize the urb properly */
	usb_fill_bulk_urb(urb, dev->udev,
		usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
		buf, count, ntrig_write_bulk_callback, dev);
	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	usb_anchor_urb(urb, &dev->submitted);
	/* send the data out the bulk port */
	retval = usb_submit_urb(urb, GFP_KERNEL);
	if (retval) {
		ntrig_err(
			"N-trig bulk: %s() - failed submitting write urb, "
			"error %d\n", __func__, retval);
		goto error_unanchor;
	}
	/*Release our reference to this urb*/
	/*USB core will eventually free it entirely */
	usb_free_urb(urb);
exit:
	ntrig_err("N-trig bulk: %s, returned count = %d\n", __func__, retval);
	return retval;

error_unanchor:
	ntrig_err("N-trig bulk: %s, on error_unanchor\n", __func__);
	usb_unanchor_urb(urb);
error:
	ntrig_err("N-trig bulk: %s,on error\n", __func__);
	usb_free_coherent(dev->udev, count, buf, urb->transfer_dma);
	usb_free_urb(urb);
err_no_buf:
	ntrig_err("N-trig bulk: %s, retval = %d, on err_no_buf\n", __func__,
		retval);
	return retval;
}

static const struct file_operations ntrig_fops = {
	.owner = THIS_MODULE,
	.open = ntrig_open,
		/* We need an "open" function in order to get proper minor
		 * number for the device to enable its registration
		 * (in usb_register_dev) */
	.release = ntrig_release,
		/* We need a release function to undo whatever "open" did... */
};

/*
 * usb class driver info in order to get a minor number from the usb core,
 * and to have the device registered with the driver core
 */
static struct usb_class_driver ntrig_class = {
	.name = "ntrig%d",
	.fops = &ntrig_fops,
		/* if fops == NULL, device regisration (usb_register_dev) is
		 * aborted */
	.minor_base = USB_NTRIG_MINOR_BASE,
};

static int ntrig_probe(struct usb_interface *interface,
	const struct usb_device_id *id)
{
	struct usb_ntrig *dev = NULL;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int i;
	int retval = -ENOMEM;
	struct _ntrig_dev_ncp_func *ncp_func = NULL;

	/* allocate memory for our device state and initialize it */
	ntrig_dbg("N-trig bulk: %s\n", __func__);
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (dev == NULL) {
		ntrig_err("%s() Out of memory\n", __func__);
		goto error;
	}
	kref_init(&dev->kref);
	init_usb_anchor(&dev->submitted);
	dev->udev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = interface;
	dev->product_id = le16_to_cpu(dev->udev->descriptor.idProduct);
	/* set up the endpoint information */
	/* use only the first bulk-in and bulk-out endpoints */
	iface_desc = interface->cur_altsetting;
	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i].desc;
		if (!dev->bulk_in_endpointAddr &&
			usb_endpoint_is_bulk_in(endpoint)) {
			/* we found a bulk in endpoint*/
			/*We ignore buffer size from descriptor*/
			dev->bulk_in_size = BULK_IN_MAX_MSG_LEN;
			dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
			dev->bulk_in_buffer = kmalloc(dev->bulk_in_size,
				GFP_KERNEL);
			if (!dev->bulk_in_buffer) {
				ntrig_err(
					"%s() Could not allocate "
					"bulk_in_buffer\n", __func__);
				goto error;
			}
		}
		if (!dev->bulk_out_endpointAddr &&
			usb_endpoint_is_bulk_out(endpoint)) {
			/* we found a bulk out endpoint */
			dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
		}
	}
	/* save our data pointer in this interface device */
	usb_set_intfdata(interface, dev);
	/* we can register the device now, as it is ready */
	retval = usb_register_dev(interface, &ntrig_class);
	if (retval) {
		/* something prevented us from registering this driver */
		ntrig_err("%s() Not able to get a minor for this device.\n",
			__func__);
		usb_set_intfdata(interface, NULL);
		goto error;
	}

	ncp_func = kmalloc(sizeof(struct _ntrig_dev_ncp_func), GFP_KERNEL);
	ncp_func->dev = (void *) dev;
	ncp_func->read = NTRIGReadNCP;
	ncp_func->write = NTRIGWriteNCP;
	dev->sensor_id = RegNtrigDispatcher(TYPE_BUS_USB, dev->udev->serial,
		ncp_func);
	kfree(ncp_func);
	ncp_func = NULL;

	if (dev->sensor_id) {
		ntrig_err("%s: Cannot register device to dispatcher\n",
			__func__);
		retval = DTRG_FAILED;
		goto error;
	}

	i = le16_to_cpu(dev->udev->descriptor.bcdDevice);
	dev_info(&interface->dev,
		"N-trig bulk Version %1d%1d.%1d%1d found at address %d\n",
		(i & 0xF000)>>12, (i & 0xF00)>>8,
		(i & 0xF0)>>4, (i & 0xF), dev->udev->devnum);
	/* let the user know what node this device is now attached to */
	dev_info(&interface->dev,
		"USB N-trig device now attached to N-trig bulk-%d\n",
		interface->minor);
	return 0;

error:
	if (dev)
		kref_put(&dev->kref, ntrig_delete);
	return retval;
}

static void ntrig_draw_down(struct usb_ntrig *dev)
{
	int time;
	ntrig_dbg("N-trig bulk: %s\n", __func__);
	time = usb_wait_anchor_empty_timeout(&dev->submitted, 1000);
	if (!time)
		usb_kill_anchored_urbs(&dev->submitted);
}

static int ntrig_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct usb_ntrig *dev = usb_get_intfdata(intf);
	ntrig_dbg("N-trig bulk: %s\n", __func__);
	if (!dev)
		return 0;
	ntrig_draw_down(dev);
	return 0;
}

static int ntrig_resume(struct usb_interface *intf)
{
	return 0;
}

static void ntrig_disconnect(struct usb_interface *interface)
{
	struct usb_ntrig *dev;
	int minor = interface->minor;
	ntrig_dbg("N-trig bulk: %s, minor = %d\n", __func__, minor);
	mutex_lock(&open_disc_mutex);
	dev = usb_get_intfdata(interface);
	UnregNtrigDispatcher((void *) dev, dev->sensor_id, TYPE_BUS_USB,
		dev->udev->serial/*itoa(slot_id) ??*/);
	usb_set_intfdata(interface, NULL);
	mutex_unlock(&open_disc_mutex);
	/* give back our minor */
	usb_deregister_dev(interface, &ntrig_class);
	/* decrement our usage count */
	kref_put(&dev->kref, ntrig_delete);
	dev_info(&interface->dev, "USB N-trig #%d now disconnected\n", minor);
	/* unregister device in dispatcher */
}

static struct usb_driver ntrig_driver = {
	.name = "usbntrig",
	.probe = ntrig_probe,
	.disconnect = ntrig_disconnect,
	.suspend = ntrig_suspend,
	.resume = ntrig_resume,
	.id_table = id_table,
	.supports_autosuspend = 1,
};

static int __init usb_ntrig_init(void)
{
	int result;
	ntrig_dbg("%s\n", __func__);
	result = usb_register(&ntrig_driver);
	if (result)
		ntrig_err("%s() usb_register failed. Error number %d\n",
			__func__, result);
	return result;
}


static void __exit usb_ntrig_exit(void)
{
	usb_deregister(&ntrig_driver);
}

module_init(usb_ntrig_init);
module_exit(usb_ntrig_exit);

MODULE_AUTHOR("Micki Balanga <micki@n-trig.com>");
MODULE_DESCRIPTION(DRIVER_VERSION);
MODULE_LICENSE("GPL");
