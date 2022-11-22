/*
 * Gadget Driver for Android simmian
 *
 * Copyright (C) 2012 Samsung Electronics.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/usb/composite.h>
#include <linux/usb/gadget.h>
#include <linux/miscdevice.h>

#include "f_simmian.h"
#include "../configfs.h"

#define SIMMIAN_BULK_BUFFER_SIZE            (1<<18)//(4096)
#define MAX_INST_NAME_LEN          40

#define SIMMIAN_COMMAND1    1
#define SIMMIAN_COMMAND2    2

#define SIMMIAN_STRING_MANUFACTURER 1
#define SIMMIAN_STRING_PRODUCT      2

#define STATE_OFF_EP0       99
#define STATE_READY_EP0     0
#define STATE_IDLE_EP0      1

#define STATE_OFFLINE               0   /* initial state, disconnected */
#define STATE_READY                 1   /* ready for userspace calls */
#define STATE_BUSY                  2   /* processing userspace calls */
#define STATE_CANCELED              3   /* transaction canceled by host */
#define STATE_ERROR                 4   /* error from completion routine */

#define SIMMIAN_CTRL_MAXPACKET 0x8000
#define NUM_INTERNAL_CTRL       (6)

/* number of tx requests to allocate */

char gBufSimmian[255]={0,};
const u8 SIMMIAN_STRING1[]= "Samsung";
const u8 SIMMIAN_STRING2[]= "Capture Board";

/* number of tx and rx requests to allocate */
#define TX_BULK_REQ_MAX 8

//#define DEBUG

typedef struct
{
	u8 bRequestType;
	u8 bRequest;
	u16 wValue;
	u16 wIndex;
	u16 wLength;
}tSIMMIAN_CTRL_S;

tSIMMIAN_CTRL_S ep0_command;

static const char simmian_devname[] = "simmian_usb";
static const char simmian_ep0_devname[] = "simmian_usb_ep0";

struct usb_request_list{
	struct list_head list;
	struct usb_ctrlrequest ctrl;
};

struct simmian_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	spinlock_t lock;
	spinlock_t lock_request;
	spinlock_t lock_free;

	wait_queue_head_t ep0_read_wq;
	wait_queue_head_t ep0_write_wq;
	wait_queue_head_t write_wq;
	wait_queue_head_t read_wq;

	int state_read_ep0;
	int state_write_ep0;
	int state;

	struct usb_ep *ep_in;
	struct usb_ep *ep_out;

	atomic_t read_excl;
	atomic_t write_excl;
	atomic_t open_excl;

	struct list_head tx_idle;

	struct list_head req_list_head;
	struct list_head free_req_head;
	struct usb_request_list req_list[NUM_INTERNAL_CTRL];
	int readcount;
	int reqcount;
	int ctrl_free_cnt;
	int ctrl_request_cnt;
	u8 *databuf;

	struct usb_request *rx_req;
	int rx_done;
	struct usb_request *tx_req;
};

struct simmian_instance {
	const char *name;
	struct usb_function_instance func_inst;
	struct simmian_dev *dev;
};

static struct usb_interface_descriptor simmian_interface_desc = {
	.bLength                = USB_DT_INTERFACE_SIZE,
	.bDescriptorType        = USB_DT_INTERFACE,
	.bInterfaceNumber       = 0,
	.bAlternateSetting  = 0,
	.bNumEndpoints          = 2,
	.bInterfaceClass        = USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass     = 0x0,
	.bInterfaceProtocol     = 0,
	.iInterface     = 0,
};

static struct usb_endpoint_descriptor simmian_superspeed_in_desc = {
	.bLength        = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    = USB_DT_ENDPOINT,
	.bEndpointAddress   = USB_DIR_IN,
	.bmAttributes       = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize     = __constant_cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor simmian_superspeed_out_desc = {
	.bLength        = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    = USB_DT_ENDPOINT,
	.bEndpointAddress   = USB_DIR_OUT,
	.bmAttributes       = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize     = __constant_cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor simmian_superspeed_bulk_comp_desc = {
	.bLength        = USB_DT_SS_EP_COMP_SIZE,
	.bDescriptorType    = USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst      = 0xF,
	.bmAttributes       = 0,
};

static struct usb_endpoint_descriptor simmian_highspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor simmian_highspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor simmian_fullspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor simmian_fullspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *fs_simmian_descs[] = {
	(struct usb_descriptor_header *) &simmian_interface_desc,
	(struct usb_descriptor_header *) &simmian_fullspeed_in_desc,
	(struct usb_descriptor_header *) &simmian_fullspeed_out_desc,
	NULL,
};

static struct usb_descriptor_header *hs_simmian_descs[] = {
	(struct usb_descriptor_header *) &simmian_interface_desc,
	(struct usb_descriptor_header *) &simmian_highspeed_in_desc,
	(struct usb_descriptor_header *) &simmian_highspeed_out_desc,
	NULL,
};

static struct usb_descriptor_header *ss_simmian_descs[] = {
	(struct usb_descriptor_header *) &simmian_interface_desc,
	(struct usb_descriptor_header *) &simmian_superspeed_in_desc,
	(struct usb_descriptor_header *) &simmian_superspeed_bulk_comp_desc,
	(struct usb_descriptor_header *) &simmian_superspeed_out_desc,
	(struct usb_descriptor_header *) &simmian_superspeed_bulk_comp_desc,
	NULL,
};

/* temporary variable used between simmian_open() and simmian_gadget_bind() */
static struct simmian_dev *_simmian_dev = NULL;

static inline int simmian_lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1) {
		return 0;
	} else {
		atomic_dec(excl);
		return -1;
	}
}

static inline void simmian_unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

static inline struct simmian_dev *func_to_simmian(struct usb_function *f)
{
	return container_of(f, struct simmian_dev, function);
}


static struct usb_request *simmian_request_new(struct usb_ep *ep, int buffer_size)
{
	struct usb_request *req = usb_ep_alloc_request(ep, GFP_KERNEL);
	if (!req)
		return NULL;

	/* now allocate buffers for the requests */
	req->buf = kmalloc(buffer_size, GFP_KERNEL);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}

	return req;
}

static void simmian_request_free(struct usb_request *req, struct usb_ep *ep)
{
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

/* add a request to the tail of a list */
void simmian_req_put(struct simmian_dev *dev, struct list_head *head, struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&dev->lock, flags);
}

/* remove a request from the head of a list */
struct usb_request *simmian_req_get(struct simmian_dev *dev, struct list_head *head)
{
	unsigned long flags;
	struct usb_request *req;

	spin_lock_irqsave(&dev->lock, flags);

	if(list_empty(head))
	{
		req = 0;
	}
	else
	{
		req = list_first_entry(head, struct usb_request, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(&dev->lock, flags);
	return req;
}

/* Make bulk-out requests be divisible by the maxpacket size */
static void simmian_set_req_length(struct usb_request *req,struct usb_ep *ep)
{
	unsigned int    rem;

	rem = req->length % ep->maxpacket;
	if (rem > 0)
		req->length += ep->maxpacket - rem;
}

static void simmian_complete_ctrl(struct usb_ep *ep, struct usb_request *req)
{
	if(ep0_command.bRequest & 0x80)
		memcpy(_simmian_dev->databuf,(u8*)req->buf,ep0_command.wLength);
	pr_debug("simmian_complete_ctrl: 0x%x,0x%x,0x%x,0x%x\n",*(u8*)req->buf,*((u8*)req->buf+1),*((u8*)req->buf+2),*((u8*)req->buf+3));
}

static void simmian_complete_bulk_in(struct usb_ep *ep, struct usb_request *req)
{
	struct simmian_dev *dev = _simmian_dev;

	if (req->status != 0)
		dev->state = STATE_ERROR;

	simmian_req_put(dev, &dev->tx_idle, req);

	wake_up(&dev->write_wq);
}

static void simmian_complete_bulk_out(struct usb_ep *ep, struct usb_request *req)
{
	struct simmian_dev *dev = _simmian_dev;

	dev->rx_done = 1;
	if (req->status != 0)
		dev->state = STATE_ERROR;

	wake_up(&dev->read_wq);
}

static int simmian_create_bulk_endpoints(struct simmian_dev *dev,
		struct usb_endpoint_descriptor *in_desc,
		struct usb_endpoint_descriptor *out_desc)
{
	int i = 0;
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	struct usb_ep *ep;

	DBG(cdev, "create_bulk_endpoints dev: 0x%p\n", dev);

	ep = usb_ep_autoconfig(cdev->gadget, in_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_in failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for ep_in got %s\n", ep->name);
	ep->driver_data = dev;      /* claim the endpoint */
	dev->ep_in = ep;

	ep = usb_ep_autoconfig(cdev->gadget, out_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_out failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for simmian ep_out got %s\n", ep->name);
	ep->driver_data = dev;      /* claim the endpoint */
	dev->ep_out = ep;

	/* now allocate requests for our endpoints */
	req = simmian_request_new(dev->ep_out, SIMMIAN_BULK_BUFFER_SIZE);
	if (!req)
		goto fail;
	req->complete = simmian_complete_bulk_out;
	dev->rx_req = req;

	for (i = 0; i < TX_BULK_REQ_MAX; i++) {
		req = simmian_request_new(dev->ep_in, SIMMIAN_BULK_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = simmian_complete_bulk_in;
		simmian_req_put(dev, &dev->tx_idle, req);
	}

	return 0;

fail:
	printk(KERN_ERR "simmian_bind() could not allocate requests\n");
	return -1;
}

static ssize_t simmian_read(struct file *fp, char __user *buf,
		size_t count, loff_t *pos)
{
	struct simmian_dev *dev = fp->private_data;
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	ssize_t r = count;
	unsigned xfer;
	int ret = 0;

	DBG(cdev, "simmian_read(%zu)\n", count);

	if (count > SIMMIAN_BULK_BUFFER_SIZE)
		return -EINVAL;

	/* we will block until we're online */
	DBG(cdev, "simmian_read: waiting for online state\n");
	ret = wait_event_interruptible(dev->read_wq,
			dev->state != STATE_OFFLINE);
	if (ret < 0) {
		r = ret;
		goto done;
	}
	spin_lock_irq(&dev->lock);
	if (dev->state == STATE_CANCELED) {
		/* report cancelation to userspace */
		dev->state = STATE_READY;
		spin_unlock_irq(&dev->lock);
		return -ECANCELED;
	}
	dev->state = STATE_BUSY;
	spin_unlock_irq(&dev->lock);

requeue_req:
	/* queue a request */
	req = dev->rx_req;
	req->length = count;
	dev->rx_done = 0;
	simmian_set_req_length(req, dev->ep_out);
	ret = usb_ep_queue(dev->ep_out, req, GFP_KERNEL);
	if (ret < 0)
		pr_err("simmian_read: failed to queue req %p (%d)\n",   req, ret);
	else{
		DBG(cdev, "rx %p queue\n", req);
	}

	/* wait for a request to complete */
	ret = wait_event_interruptible(dev->read_wq, dev->rx_done);
	if (ret < 0) {
		r = ret;
		usb_ep_dequeue(dev->ep_out, req);
		goto done;
	}
	if (dev->state == STATE_BUSY) {
		/* If we got a 0-len packet, throw it back and try again. */
		if (req->actual == 0)
			goto requeue_req;

		DBG(cdev, "rx %p %d\n", req, req->actual);
		xfer = (req->actual < count) ? req->actual : count;
		r = xfer;
		if (copy_to_user(buf, req->buf, xfer))
			r = -EFAULT;
	} else
		r = -EIO;

done:
	spin_lock_irq(&dev->lock);
	if (dev->state == STATE_CANCELED)
		r = -ECANCELED;
	else if (dev->state != STATE_OFFLINE)
		dev->state = STATE_READY;
	spin_unlock_irq(&dev->lock);

	DBG(cdev, "simmian_read returning %zd\n", r);
	return r;
}

static ssize_t simmian_write(struct file *fp, const char __user *buf,
		size_t count, loff_t *pos)
{
	struct simmian_dev *dev = fp->private_data;
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req = 0;
	ssize_t r = count;
	unsigned xfer;
	int ret;

	DBG(cdev, "simmian_write(%zu)\n", count);

	spin_lock_irq(&dev->lock);
	if (dev->state == STATE_CANCELED) {
		/* report cancelation to userspace */
		dev->state = STATE_READY;
		spin_unlock_irq(&dev->lock);
		return -ECANCELED;
	}
	if (dev->state == STATE_OFFLINE) {
		spin_unlock_irq(&dev->lock);
		return -ENODEV;
	}
	dev->state = STATE_BUSY;
	spin_unlock_irq(&dev->lock);

	if (simmian_lock(&dev->write_excl))
		return -EBUSY;


	while (count > 0) {
		if (dev->state != STATE_BUSY) {
			DBG(cdev, "simmian_write dev->error\n");
			r = -EIO;
			break;
		}

		/* get an idle tx request to use */
		req = 0;
		ret = wait_event_interruptible(dev->write_wq,
				((req = simmian_req_get(dev, &dev->tx_idle)) || dev->state != STATE_BUSY));

		if (!req) {
			r = ret;
			break;
		}

		if (count > SIMMIAN_BULK_BUFFER_SIZE)
			xfer = SIMMIAN_BULK_BUFFER_SIZE;
		else
			xfer = count;
		if (copy_from_user(req->buf, buf, xfer)) {
			r = -EFAULT;
			break;
		}
		pr_debug( "simmian_write count:%zu\n",count);

		req->length = xfer;
		ret = usb_ep_queue(dev->ep_in, req, GFP_KERNEL);
		if (ret < 0) {
			DBG(cdev, "simmian_write: xfer error %d\n", ret);
			r = -EIO;
			break;
		}

		buf += xfer;
		count -= xfer;
		/* zero this so we don't try to free it on error exit */
		req = 0;
	}

	if (req)
		simmian_req_put(dev, &dev->tx_idle, req);

	simmian_unlock(&dev->write_excl);

	spin_lock_irq(&dev->lock);
	if (dev->state == STATE_CANCELED)
		r = -ECANCELED;
	else if (dev->state != STATE_OFFLINE)
		dev->state = STATE_READY;
	spin_unlock_irq(&dev->lock);

	DBG(cdev, "simmian_write returning %zd\n", r);
	return r;
}



int simmian_open(struct inode *ip, struct file *fp)
{
	pr_info("simmian_open\n");

	/* clear any error condition */
	if(_simmian_dev->state != STATE_OFFLINE)
		_simmian_dev->state = STATE_READY;

	fp->private_data = _simmian_dev;

	return 0;
}

static int simmian_release(struct inode *ip, struct file *fp)
{
	pr_info("simmian_release\n");

	return 0;
}

static u32 simmian_ep0_poll(struct file *file, poll_table *wait)
{
	struct simmian_dev  *dev = file->private_data;
	poll_wait(file, &dev->ep0_read_wq, wait);

	if (dev->state_read_ep0 == STATE_READY_EP0)
		return POLLIN | POLLRDNORM;


	return 0;
}
static int set_free_req(struct simmian_dev *dev ,struct usb_request_list *ctrl)
{
	int ret = 0;
	unsigned long flags;

	if (ctrl) {
		spin_lock_irqsave(&dev->lock_free, flags);
		list_add_tail(&ctrl->list, &dev->free_req_head);
		dev->ctrl_free_cnt++;
		spin_unlock_irqrestore(&dev->lock_free, flags);
	} else {
		ret = -EFAULT;
		printk("item is null ptr\n");
	}

	return ret;
}

static int get_free_req(struct simmian_dev *dev ,struct usb_request_list **ctrl)
{
	int ret = 0;
	unsigned long flags;

	if (ctrl) {
		spin_lock_irqsave(&dev->lock_free, flags);

		if(dev->ctrl_free_cnt){
			*ctrl = container_of(dev->free_req_head.next,
					struct usb_request_list, list);
			dev->ctrl_free_cnt--;
			list_del(&(*ctrl)->list);
		}else
			*ctrl = NULL;

		spin_unlock_irqrestore(&dev->lock_free, flags);
	} else {
		ret = -EFAULT;
		printk("item is null ptr\n");
	}

	return ret;
}

static int set_req_list(struct simmian_dev *dev ,struct usb_request_list *ctrl)
{
	int ret = 0;
	unsigned long flags;

	if (ctrl) {
		spin_lock_irqsave(&dev->lock_request, flags);
		list_add_tail(&ctrl->list, &dev->req_list_head);
		dev->reqcount++;
		dev->ctrl_request_cnt++;
		spin_unlock_irqrestore(&dev->lock_request, flags);
	} else {
		ret = -EFAULT;
		printk("item is null ptr\n");
	}

	return ret;
}

static int get_req_list(struct simmian_dev *dev ,struct usb_request_list **ctrl)
{
	int ret = 0;
	unsigned long flags;

	if (ctrl) {
		spin_lock_irqsave(&dev->lock_request, flags);

		if(dev->ctrl_request_cnt){
			*ctrl = container_of(dev->req_list_head.next,
					struct usb_request_list, list);
			list_del(&(*ctrl)->list);
			dev->ctrl_request_cnt--;
		}else
			*ctrl =NULL;

		spin_unlock_irqrestore(&dev->lock_request, flags);
	} else {
		ret = -EFAULT;
		printk("item is null ptr\n");
	}

	return ret;
}

static ssize_t simmian_driver_ep0_tx(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	struct simmian_dev  *dev = file->private_data;
	struct usb_composite_dev *cdev = dev->function.config->cdev;
	struct usb_request  *req = cdev->req;
	int ret;

	pr_debug("simmian_driver_ep0_tx\n");

	if (count > 0) {
		req->zero = 0;
		req->length = count;

		if (copy_from_user(req->buf, buf, count)) {
			return -EFAULT;
		}

		ret = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);

		if (ret < 0)
			printk("simmian command setup response failed\n");
	}

	return ret;
}

static ssize_t simmian_driver_ep0_rx(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	int ret;
	struct simmian_dev *dev = file->private_data;
	struct usb_request_list *request;
	pr_debug("simmian_driver_ep0_rx\n");

	dev->readcount++;
	ret = wait_event_interruptible(dev->ep0_read_wq,!list_empty(&dev->req_list_head));

	get_req_list(dev,&request);
	if(request){
		ep0_command.bRequestType =request->ctrl.bRequestType;
		ep0_command.bRequest = request->ctrl.bRequest;
		ep0_command.wValue = request->ctrl.wValue;
		ep0_command.wIndex = request->ctrl.wIndex;
		ep0_command.wLength = request->ctrl.wLength;
		ret = copy_to_user(buf, (char*)&ep0_command,sizeof(ep0_command));
		set_free_req(dev,request);
	}
	return ret;
}

struct ep0_rbuf buf;

long simmian_ep0_ioctl(struct file* filp, unsigned int cmd,unsigned long arg)
{
	struct simmian_dev  *dev = filp->private_data;
	struct usb_composite_dev *cdev = dev->function.config->cdev;
	int data;
	unsigned long fail_copy_size = 0;

	switch(cmd){
		case SIMMIAN_EP0_MAXPACKET:
			data = cdev->gadget->ep0->maxpacket;

			if(put_user(data,(u32 __user *) arg))
				return -EFAULT;

			break;
		case SIMMIAN_EP0_DATA_STAGE:
			//spin_lock_irq(&dev->lock);

			// Modified by payton (sehoon.kim) 20180703 - arg: user space memory, buf: kernel space memory
			fail_copy_size = copy_from_user(&buf, (u32 __user *)arg, sizeof(int));
			if(fail_copy_size != 0)
			{
				printk("[%s] sehoon fail 1 (buf.count = %d, fail_copy_size = %lu)\n", __func__, buf.count, fail_copy_size);
				return -EFAULT;
			}

			if(copy_to_user((u32 __user *)(arg+4), (char*)dev->databuf,buf.count))
			{
				printk("[%s] sehoon fail 2 (buf.count = %d)\n", __func__, buf.count);
				return -EFAULT;
			}
			//spin_unlock_irq(&dev->lock);
			break;
	}

	return 0;
}

#ifdef CONFIG_COMPAT
static long simmian_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return simmian_ep0_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

int simmian_ep0_open(struct inode *ip, struct file *fp)
{
	pr_info("simmian_ep0_open %p %p\n", _simmian_dev, fp);
	fp->private_data = _simmian_dev;

	return 0;
}

static int simmian_ep0_release(struct inode *ip, struct file *fp)
{
	int i;
	struct simmian_dev *dev = fp->private_data;
	struct usb_request_list *request;

	for (i = 0; i < NUM_INTERNAL_CTRL; i++) {
		get_req_list(dev,&request);
		if(request)
			set_free_req(dev,request);
	}
	dev->readcount = 0;
	dev->reqcount =0;
	pr_info("simmian_ep0_release\n");

	return 0;
}

/* file operations for /dev/simmian_usb */
static const struct file_operations simmian_fops = {
	.owner = THIS_MODULE,
	.open = simmian_open,
	.release = simmian_release,
	.write = simmian_write,
	.read = simmian_read,
};

static struct miscdevice simmian_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = simmian_devname,
	.fops = &simmian_fops,
};

/* file operations for /dev/simmian_usb_ep0 */
static const struct file_operations simmian_ep0_fops = {
	.owner = THIS_MODULE,
	.open = simmian_ep0_open,
	.release = simmian_ep0_release,
	.write = simmian_driver_ep0_tx,
	.read = simmian_driver_ep0_rx,
	.poll = simmian_ep0_poll,
	.unlocked_ioctl = simmian_ep0_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = simmian_compat_ioctl,
#endif
};

static struct miscdevice simmian_ep0_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = simmian_ep0_devname,
	.fops = &simmian_ep0_fops,
};

static int simmian_function_bind(struct usb_configuration *c,
		struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct simmian_dev  *dev = func_to_simmian(f);
	int         id;
	int         ret;

	printk("simmian, called %s()\n", __FUNCTION__);

	dev->cdev = cdev;
	DBG(cdev, "simmian_function_bind dev: %p\n", dev);

	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	simmian_interface_desc.bInterfaceNumber = id;

	/* allocate endpoints */
	ret = simmian_create_bulk_endpoints(dev, &simmian_fullspeed_in_desc,
			&simmian_fullspeed_out_desc);
	if (ret)
		return ret;

	/* support high speed hardware */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		simmian_highspeed_in_desc.bEndpointAddress =
			simmian_fullspeed_in_desc.bEndpointAddress;
		simmian_highspeed_out_desc.bEndpointAddress =
			simmian_fullspeed_out_desc.bEndpointAddress;
	}
	if (gadget_is_superspeed(c->cdev->gadget)) {
		simmian_superspeed_in_desc.bEndpointAddress =
			simmian_fullspeed_in_desc.bEndpointAddress;
		simmian_superspeed_out_desc.bEndpointAddress =
			simmian_fullspeed_out_desc.bEndpointAddress;
	}
	DBG(cdev, "%s speed %s: IN/%s, OUT/%s\n",
			gadget_is_superspeed(c->cdev->gadget) ? "super" :
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			f->name, dev->ep_in->name, dev->ep_out->name);
	return 0;
}

static void simmian_function_unbind(struct usb_configuration *c,
		struct usb_function *f)
{
	struct simmian_dev  *dev = func_to_simmian(f);
	struct usb_request *req;

	printk("simmian, called %s()\n", __FUNCTION__);
	wake_up(&dev->read_wq);

	simmian_request_free(dev->rx_req, dev->ep_out);
	while((req = simmian_req_get(dev, &dev->tx_idle)))
		simmian_request_free(dev->tx_req, dev->ep_in);

	spin_lock_irq(&dev->lock);
	dev->state = STATE_OFFLINE;
	spin_unlock_irq(&dev->lock);
}

static int simmian_function_set_alt(struct usb_function *f,
		unsigned intf, unsigned alt)
{
	struct simmian_dev  *dev = func_to_simmian(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	int ret;

	printk("simmian, called %s()\n", __FUNCTION__);

	pr_debug("simmian_function_set_alt intf: %d alt: %d\n", intf, alt);
	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_in);
	if (ret)
		return ret;

	ret = usb_ep_enable(dev->ep_in);
	if (ret)
		return ret;

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_out);
	if (ret)
		return ret;

	ret = usb_ep_enable(dev->ep_out);
	if (ret) {
		usb_ep_disable(dev->ep_in);
		return ret;
	}

	dev->state = STATE_READY;

	return 0;
}

static int simmian_function_setup(struct usb_function *f,
		const struct usb_ctrlrequest *ctrl)
{
	int value = -EOPNOTSUPP;

	struct simmian_dev  *dev = func_to_simmian(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request  *req = cdev->req;
	struct usb_request_list *request;

	printk("simmian, called %s()\n", __FUNCTION__);
	printk("simmian, bRequestType=0x%X, bRequest=0x%X, wValue=0x%X, wIndex=0x%X, wLength=0x%X\n", 
		ctrl->bRequestType, ctrl->bRequest, ctrl->wValue, ctrl->wIndex, ctrl->wLength);

	req->complete = simmian_complete_ctrl;
	value = get_free_req(dev,&request);
	if(request){
		request->ctrl = *ctrl;
		set_req_list(dev,request);
	}
	if(dev->reqcount == dev->readcount)
		wake_up(&dev->ep0_read_wq);

	return value;
}

static void simmian_function_disable(struct usb_function *f)
{
	struct simmian_dev  *dev = func_to_simmian(f);
	struct usb_composite_dev    *cdev = dev->cdev;

	printk("simmian, called %s()\n", __FUNCTION__);

	pr_debug("simmian_function_disable cdev %p\n", cdev);

	dev->state = STATE_OFFLINE;
	usb_ep_disable(dev->ep_in);
	usb_ep_disable(dev->ep_out);

	pr_debug("%s disabled\n", dev->function.name);
}

static struct usb_string        simmian_strings[] = {
	{SIMMIAN_STRING_MANUFACTURER,   SIMMIAN_STRING1},
	{SIMMIAN_STRING_PRODUCT ,   SIMMIAN_STRING2},
	{}
};

static struct usb_gadget_strings    simmian_stringtab = {
	.language   = 0x0409,       /* en-us */
	.strings    = simmian_strings,
};

static struct usb_gadget_strings *simmian_strings_array[] = {
	&simmian_stringtab,
	NULL,
};

#ifndef CONFIG_USB_CONFIGFS_UEVENT
static int simmian_bind_config(struct usb_configuration *c)
{
	struct simmian_dev *dev = _simmian_dev;

	printk("simmian, called %s()\n", __FUNCTION__);

	pr_debug( "simmian_bind_config\n");

	dev->cdev = c->cdev;
	dev->function.name = "simmian";
	dev->function.fs_descriptors = fs_simmian_descs;
	dev->function.hs_descriptors = hs_simmian_descs;
	dev->function.ss_descriptors = ss_simmian_descs;
	dev->function.strings = simmian_strings_array;
	dev->function.bind = simmian_function_bind;
	dev->function.unbind = simmian_function_unbind;
	dev->function.set_alt = simmian_function_set_alt;
	dev->function.setup = simmian_function_setup;
	dev->function.disable = simmian_function_disable;

	return usb_add_function(c, &dev->function);
}
#endif

static int __simmian_init(struct simmian_instance *fi_simmian)
{
	int ret;
	int i;
	struct simmian_dev *dev;

	printk("simmian, called %s()\n", __FUNCTION__);

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	if (fi_simmian != NULL)
	{
		fi_simmian->dev = dev;
	}

	spin_lock_init(&dev->lock);
	spin_lock_init(&dev->lock_request);
	spin_lock_init(&dev->lock_free);

	init_waitqueue_head(&dev->ep0_read_wq);
	init_waitqueue_head(&dev->ep0_write_wq);
	init_waitqueue_head(&dev->write_wq);
	init_waitqueue_head(&dev->read_wq);

	dev->state_read_ep0 = STATE_IDLE_EP0;
	dev->state_write_ep0 = STATE_IDLE_EP0;

	atomic_set(&dev->open_excl, 0);
	atomic_set(&dev->read_excl, 0);
	atomic_set(&dev->write_excl, 0);

	INIT_LIST_HEAD(&dev->tx_idle);
	INIT_LIST_HEAD(&dev->req_list_head);
	INIT_LIST_HEAD(&dev->free_req_head);
	dev->readcount = 0;
	dev->reqcount =0;
	dev->ctrl_free_cnt=0;
	dev->ctrl_request_cnt = 0;

	for(i=0;i<NUM_INTERNAL_CTRL;i++)
		set_free_req(dev,&dev->req_list[i]);

	dev->databuf =kzalloc(SIMMIAN_CTRL_MAXPACKET,GFP_KERNEL);
	if (!dev->databuf)
		return -ENOMEM;
	_simmian_dev = dev;

	ret = misc_register(&simmian_device);
	if (ret)
		goto err;

	ret = misc_register(&simmian_ep0_device);
	if (ret)
		goto err;

	return 0;

err:
	kfree(dev);
	printk(KERN_ERR "simmian gadget driver failed to initialize\n");
	return ret;
}

static void simmian_cleanup(void)
{
	printk("simmian, called %s()\n", __FUNCTION__);

	kfree(_simmian_dev->databuf);
	kfree(_simmian_dev);
	_simmian_dev = NULL;
	misc_deregister(&simmian_device);
	misc_deregister(&simmian_ep0_device);

}

static int simmian_setup_configfs(struct simmian_instance *fi_simmian)
{
	printk("simmian, called %s()\n", __FUNCTION__);

	return __simmian_init(fi_simmian);
}

static struct simmian_instance *to_simmian_instance(struct config_item *item)
{
	return container_of(to_config_group(item), struct simmian_instance,
							func_inst.group);
}

static void simmian_attr_release(struct config_item *item)
{
	struct simmian_instance *fi_simmian = to_simmian_instance(item);

	printk("simmian, called %s()\n", __FUNCTION__);
	usb_put_function_instance(&fi_simmian->func_inst);
}

static struct configfs_item_operations simmian_item_ops = {
	.release	= simmian_attr_release,
};

static struct config_item_type simmian_func_type = {
	.ct_item_ops	= &simmian_item_ops,
	.ct_owner	= THIS_MODULE,
};

static struct simmian_instance *to_fi_simmian(struct usb_function_instance *fi)
{
	return container_of(fi, struct simmian_instance, func_inst);
}

static int simmian_set_inst_name(struct usb_function_instance *fi,
						const char *name)
{
	struct simmian_instance *fi_simmian;
	char *ptr;
	int name_len;
	printk("simmian, called %s()\n", __FUNCTION__);

	name_len = strlen(name) + 1;
	if (name_len > MAX_INST_NAME_LEN)
		return -ENAMETOOLONG;

	ptr = kstrndup(name, name_len, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	fi_simmian = to_fi_simmian(fi);
	fi_simmian->name = ptr;

	return 0;
}

static void simmian_free_inst(struct usb_function_instance *fi)
{
	struct simmian_instance *fi_simmian;
	printk("simmian, called %s()\n", __FUNCTION__);

	fi_simmian = to_fi_simmian(fi);
	kfree(fi_simmian->name);
	simmian_cleanup();
	kfree(fi_simmian);
}

struct usb_function_instance *alloc_inst_simmian(bool simmian_config)
{
	struct simmian_instance *fi_simmian;
	int ret = 0;
	printk("simmian, called %s()\n", __FUNCTION__);

	fi_simmian = kzalloc(sizeof(*fi_simmian), GFP_KERNEL);
	if (!fi_simmian)
		return ERR_PTR(-ENOMEM);
	fi_simmian->func_inst.set_inst_name = simmian_set_inst_name;
	fi_simmian->func_inst.free_func_inst = simmian_free_inst;

	if (simmian_config) {
		ret = simmian_setup_configfs(fi_simmian);
		if (ret) {
			kfree(fi_simmian);
			pr_err("Error setting simmian\n");
			return ERR_PTR(ret);
		}
	} else
		fi_simmian->dev = _simmian_dev;

	config_group_init_type_name(&fi_simmian->func_inst.group, "",
						&simmian_func_type);

	return  &fi_simmian->func_inst;
}
EXPORT_SYMBOL_GPL(alloc_inst_simmian);

static struct usb_function_instance *simmian_alloc_inst(void)
{
	printk("simmian, called %s()\n", __FUNCTION__);

	return alloc_inst_simmian(true);
}

static void simmian_free(struct usb_function *f)
{
	printk("simmian, called %s()\n", __FUNCTION__);
	/*NO-OP: no function specific resource allocation in simmian_alloc*/
}

struct usb_function *function_alloc_simmian(struct usb_function_instance *fi,
					bool simmian_config)
{
	struct simmian_instance *fi_simmian = to_fi_simmian(fi);
	struct simmian_dev *dev = fi_simmian->dev;
	printk("simmian, called %s()\n", __FUNCTION__);

	dev->function.name = "simmian";

	dev->function.fs_descriptors = fs_simmian_descs;
	dev->function.hs_descriptors = hs_simmian_descs;
	dev->function.ss_descriptors = ss_simmian_descs;
	dev->function.strings = simmian_strings_array;

	dev->function.bind = simmian_function_bind;
	dev->function.unbind = simmian_function_unbind;
	dev->function.set_alt = simmian_function_set_alt;
	dev->function.disable = simmian_function_disable;
	dev->function.setup = simmian_function_setup;
	dev->function.free_func = simmian_free;

	return &dev->function;
}
EXPORT_SYMBOL_GPL(function_alloc_simmian);

static struct usb_function *simmian_alloc(struct usb_function_instance *fi)
{
	return function_alloc_simmian(fi, true);
}

DECLARE_USB_FUNCTION_INIT(simmian, simmian_alloc_inst, simmian_alloc);
MODULE_LICENSE("GPL");

