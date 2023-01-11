/*
 * f_acm.c -- USB CDC serial (ACM) function driver
 *
 * Copyright (C) 2003 Al Borchers (alborchers@steinerpoint.com)
 * Copyright (C) 2008 by David Brownell
 * Copyright (C) 2008 by Nokia Corporation
 * Copyright (C) 2009 by Samsung Electronics
 * Author: Michal Nazarewicz (mina86@mina86.com)
 *
 * This software is distributed under the terms of the GNU General
 * Public License ("GPL") as published by the Free Software Foundation,
 * either version 2 of that License or (at your option) any later version.
 */

/* #define VERBOSE_DEBUG */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/usb/composite.h>
#include <linux/workqueue.h>

#include "pxa910_u_serial.h"
#include "gadget_chips.h"

/*
 * This CDC ACM function support just wraps control functions and
 * notifications around the generic serial-over-usb code.
 *
 * Because CDC ACM is standardized by the USB-IF, many host operating
 * systems have drivers for it.  Accordingly, ACM is the preferred
 * interop solution for serial-port type connections.  The control
 * models are often not necessary, and in any case don't do much in
 * this bare-bones implementation.
 *
 * Note that even MS-Windows has some support for ACM.  However, that
 * support is somewhat broken because when you use ACM in a composite
 * device, having multiple interfaces confuses the poor OS.  It doesn't
 * seem to understand CDC Union descriptors.  The new "association"
 * descriptors (roughly equivalent to CDC Unions) may sometimes help.
 */
#define GS_DIAG_PORT_BASE 1

struct pxa910_f_diag {
	struct pxa910_gserial		port;
	u8				data_id;
	u8				port_num;
};

static inline struct pxa910_f_diag *pxa910_func_to_diag(struct usb_function *f)
{
	return container_of(f, struct pxa910_f_diag, port.func);
}

static inline struct
pxa910_f_diag *pxa910_port_to_diag(struct pxa910_gserial *p)
{
	return container_of(p, struct pxa910_f_diag, port);
}

/*-------------------------------------------------------------------------*/

/* interface descriptor: */
static struct usb_interface_descriptor pxa910_diag_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_CDC_DATA,
	.bInterfaceSubClass =	0,
	.bInterfaceProtocol =	0xff,
	/* .iInterface = DYNAMIC */
};

/* full speed support: */

static struct usb_endpoint_descriptor pxa910_diag_fs_in_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor pxa910_diag_fs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *pxa910_diag_fs_function[] = {
	(struct usb_descriptor_header *)&pxa910_diag_interface_desc,
	(struct usb_descriptor_header *)&pxa910_diag_fs_in_desc,
	(struct usb_descriptor_header *)&pxa910_diag_fs_out_desc,
	NULL,
};

/* high speed support: */

static struct usb_endpoint_descriptor pxa910_diag_hs_in_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = cpu_to_le16(512),
};

static struct usb_endpoint_descriptor pxa910_diag_hs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_descriptor_header *pxa910_diag_hs_function[] = {
	(struct usb_descriptor_header *)&pxa910_diag_interface_desc,
	(struct usb_descriptor_header *)&pxa910_diag_hs_in_desc,
	(struct usb_descriptor_header *)&pxa910_diag_hs_out_desc,
	NULL,
};

static struct usb_endpoint_descriptor pxa910_diag_ss_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor pxa910_diag_ss_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor pxa910_diag_ss_bulk_comp_desc = {
	.bLength =              sizeof(pxa910_diag_ss_bulk_comp_desc),
	.bDescriptorType =      USB_DT_SS_ENDPOINT_COMP,
};

static struct usb_descriptor_header *pxa910_diag_ss_function[] = {
	(struct usb_descriptor_header *) &pxa910_diag_interface_desc,
	(struct usb_descriptor_header *) &pxa910_diag_ss_in_desc,
	(struct usb_descriptor_header *) &pxa910_diag_ss_bulk_comp_desc,
	(struct usb_descriptor_header *) &pxa910_diag_ss_out_desc,
	(struct usb_descriptor_header *) &pxa910_diag_ss_bulk_comp_desc,
	NULL,
};

/* string descriptors: */

/* static strings, in UTF-8 */
static struct usb_string pxa910_diag_string_defs[] = {
	[0].s = "Marvell DIAG",
	{  /* ZEROES END LIST */ },
};

static struct usb_gadget_strings pxa910_diag_string_table = {
	.language =		0x0409,	/* en-us */
	.strings =		pxa910_diag_string_defs,
};

static struct usb_gadget_strings *pxa910_diag_strings[] = {
	&pxa910_diag_string_table,
	NULL,
};

/*-------------------------------------------------------------------------*/

static int
pxa910_diag_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct pxa910_f_diag *diag = pxa910_func_to_diag(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	/* we know alt == 0, so this is an activation or a reset */

	if (diag->port.in->driver_data) {
		DBG(cdev, "reset diag ttyGS%d\n", diag->port_num);
		pxa910_gserial_disconnect(&diag->port);
	} else {
		DBG(cdev, "activate generic ttyGS%d\n", diag->port_num);
	}
	if (!diag->port.in->desc || !diag->port.out->desc) {
		DBG(cdev, "activate diag ttyGS%d\n", diag->port_num);
		if (config_ep_by_speed(cdev->gadget, f, diag->port.in) ||
			config_ep_by_speed(cdev->gadget, f, diag->port.out)) {
			diag->port.in->desc = NULL;
			diag->port.out->desc = NULL;
			return -EINVAL;
		}
	}

	pxa910_gserial_connect(&diag->port, diag->port_num);

	return 0;
}

static void pxa910_diag_disable(struct usb_function *f)
{
	struct pxa910_f_diag	*diag = pxa910_func_to_diag(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	DBG(cdev, "diag ttyGS%d deactivated\n", diag->port_num);

	pxa910_gserial_disconnect(&diag->port);
}

/*-------------------------------------------------------------------------*/

/* DIAG function driver setup/binding */
static int
pxa910_diag_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct pxa910_f_diag	*diag = pxa910_func_to_diag(f);
	int			status;
	struct usb_ep		*ep;

	/* allocate instance-specific interface IDs, and patch descriptors */
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	diag->data_id = status;

	pxa910_diag_interface_desc.bInterfaceNumber = status;

	status = -ENODEV;

	/* allocate instance-specific endpoints */
	ep = usb_ep_autoconfig(cdev->gadget, &pxa910_diag_fs_in_desc);
	if (!ep)
		goto fail;
	diag->port.in = ep;
	ep->driver_data = cdev;	/* claim */

	ep = usb_ep_autoconfig(cdev->gadget, &pxa910_diag_fs_out_desc);
	if (!ep)
		goto fail;
	diag->port.out = ep;
	ep->driver_data = cdev;	/* claim */

	/* copy descriptors */
	f->fs_descriptors = usb_copy_descriptors(pxa910_diag_fs_function);
	if (!f->fs_descriptors)
		goto fail;

	/* support all relevant hardware speeds... we expect that when
	 * hardware is dual speed, all bulk-capable endpoints work at
	 * both speeds
	 */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		pxa910_diag_hs_in_desc.bEndpointAddress =
				pxa910_diag_fs_in_desc.bEndpointAddress;
		pxa910_diag_hs_out_desc.bEndpointAddress =
				pxa910_diag_fs_out_desc.bEndpointAddress;

		/* copy descriptors */
		f->hs_descriptors =
			usb_copy_descriptors(pxa910_diag_hs_function);
	}
	if (gadget_is_superspeed(c->cdev->gadget)) {
		pxa910_diag_ss_in_desc.bEndpointAddress =
			pxa910_diag_fs_in_desc.bEndpointAddress;
		pxa910_diag_ss_out_desc.bEndpointAddress =
			pxa910_diag_fs_out_desc.bEndpointAddress;

		/* copy descriptors, and track endpoint copies */
		f->ss_descriptors =
			usb_copy_descriptors(pxa910_diag_ss_function);
		if (!f->ss_descriptors)
			goto fail;
	}

	DBG(cdev, "diag ttyGS%d: %s speed IN/%s OUT/%s\n",
			diag->port_num,
			gadget_is_superspeed(c->cdev->gadget) ? "super" :
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			diag->port.in->name, diag->port.out->name);
	return 0;

fail:
	/* we might as well release our claims on endpoints */
	if (diag->port.out)
		diag->port.out->driver_data = NULL;
	if (diag->port.in)
		diag->port.in->driver_data = NULL;

	ERROR(cdev, "%s/%p: can't bind, err %d\n", f->name, f, status);

	return status;
}

static void
pxa910_diag_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct pxa910_f_diag	*diag = pxa910_func_to_diag(f);

	if (gadget_is_dualspeed(c->cdev->gadget))
		usb_free_descriptors(f->hs_descriptors);
	if (gadget_is_superspeed(c->cdev->gadget))
		usb_free_descriptors(f->ss_descriptors);
	usb_free_descriptors(f->fs_descriptors);
	kfree(diag);
}

/**
 * acm_bind_config - add a CDC ACM function to a configuration
 * @c: the configuration to support the CDC ACM instance
 * @port_num: /dev/ttyGS* port this interface will use
 * Context: single threaded during gadget setup
 *
 * Returns zero on success, else negative errno.
 *
 * Caller must have called @gserial_setup() with enough ports to
 * handle all the ones it binds.  Caller is also responsible
 * for calling @gserial_cleanup() before module unload.
 */
int pxa910_diag_bind_config(struct usb_configuration *c, u8 port_num)
{
	struct pxa910_f_diag	*diag;
	int		status;

	/* REVISIT might want instance-specific strings to help
	 * distinguish instances ...
	 */

	/* maybe allocate device-global string IDs, and patch descriptors */
	if (pxa910_diag_string_defs[0].id == 0) {
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;

		pxa910_diag_string_defs[0].id = status;

		pxa910_diag_interface_desc.iInterface = status;
	}

	/* allocate and initialize one new instance */
	diag = kzalloc(sizeof(*diag), GFP_KERNEL);
	if (!diag)
		return -ENOMEM;

	diag->port_num = port_num;

	diag->port.func.name = "diag";
	diag->port.func.strings = pxa910_diag_strings;
	/* descriptors are per-instance copies */
	diag->port.func.bind = pxa910_diag_bind;
	diag->port.func.unbind = pxa910_diag_unbind;
	diag->port.func.set_alt = pxa910_diag_set_alt;
	diag->port.func.disable = pxa910_diag_disable;

	init_waitqueue_head(&diag->port.port_send);
	status = usb_add_function(c, &diag->port.func);
	if (status)
		kfree(diag);
	return status;
}

static int gs_marvell_diag_write(struct tty_struct *tty,
				 const unsigned char *buf, int count)
{
	struct pxa910_gs_port *port = tty->driver_data;
	unsigned long flags;
	int status;

	pr_vdebug("gs_marvell_diag_write: ttyGS%d (%p) writing %d bytes\n",
		  port->port_num, tty, count);

	if (port == NULL)
		goto out;
	spin_lock_irqsave(&port->port_lock, flags);
	if (count)
		count = pxa910_gs_buf_put(&port->port_write_buf, buf, count);

	/* treat count == 0 as flush_chars() */
	if (port->port_usb)
		status = pxa910_gs_start_tx(port);
	spin_unlock_irqrestore(&port->port_lock, flags);
out:
	if (count == 0)
		return -EAGAIN;
	return count;
}

int gs_marvell_diag_send(const unsigned char *buf, int count)
{
	struct tty_struct *tty;
	struct pxa910_gs_port *port;
	int c, retval = 0;
	const unsigned char *b = buf;
	unsigned long flags;
	wait_queue_head_t *p_port_send;

	/* Assume that we have only one port */
	port = pxa910_ports[GS_DIAG_PORT_BASE].port;
	tty = port->port.tty;

	if (tty == NULL || tty->driver_data == NULL || port->port_usb == NULL)
		return count;
	while (count > 0) {
		c = gs_marvell_diag_write(tty, b, count);

		if (c == count) {
			b += c;
			break;	/* send all bytes successfully */
		} else if (c >= 0) {
			b += c;
			count -= c;
			continue;
		} else if (c < 0) {
			retval = c;
			if (c != -EAGAIN)
				goto break_out;
		}

		/* retry to wait enough buffer to send the rest bytes. */
		spin_lock_irqsave(&port->port_lock, flags);

		if (port->port_usb) {
			p_port_send = &port->port_usb->port_send;
		} else {
			p_port_send = NULL;
			pr_info("%s: usb port is released.\n",
					__func__);
		}

		spin_unlock_irqrestore(&port->port_lock, flags);

		if (p_port_send)
			interruptible_sleep_on_timeout(p_port_send,
						       10 * HZ / 1000);

		port = pxa910_ports[GS_DIAG_PORT_BASE].port;
		tty = port->port.tty;
		if (tty == NULL || tty->driver_data == NULL
		    || port->port_usb == NULL)
			return count;
	}
break_out:
	return (b - buf) ? b - buf : retval;
}
EXPORT_SYMBOL(gs_marvell_diag_send);

typedef int (*marvell_diag_rx_callback) (char *packet, int len);

marvell_diag_rx_callback gs_marvell_diag_rx_callback =
		(marvell_diag_rx_callback) NULL;
EXPORT_SYMBOL(gs_marvell_diag_rx_callback);

typedef int (*marvell_diag_ioctl) (unsigned int cmd, unsigned long arg);

marvell_diag_ioctl gs_marvell_diag_ioctl = (marvell_diag_ioctl) NULL;
EXPORT_SYMBOL(gs_marvell_diag_ioctl);

/*
 * RX tasklet takes data out of the RX queue and hands it up to the TTY
 * layer until it refuses to take any more data (or is throttled back).
 * Then it issues reads for any further data.
 *
 * If the RX queue becomes full enough that no usb_request is queued,
 * the OUT endpoint may begin NAKing as soon as its FIFO fills up.
 * So QUEUE_SIZE packets plus however many the FIFO holds (usually two)
 * can be buffered before the TTY layer's buffers (currently 64 KB).
 */
static void gs_marvell_diag_rx_push(unsigned long _port)
{
	struct pxa910_gs_port *port = (void *)_port;
	struct tty_struct *tty;
	struct list_head *queue = &port->read_queue;
	bool disconnect = false;
	bool do_push = false;
	struct timespec now;

	/* hand any queued data to the tty */
	spin_lock_irq(&port->port_lock);
	tty = port->port.tty;
	while (!list_empty(queue)) {
		struct usb_request *req;

		now = CURRENT_TIME;
		req = list_first_entry(queue, struct usb_request, list);

		/* discard data if tty was closed */
		if (!tty)
			goto recycle;

		/* leave data queued if tty was rx throttled */
		if (test_bit(TTY_THROTTLED, &tty->flags))
			break;

		switch (req->status) {
		case -ESHUTDOWN:
			disconnect = true;
			pr_vdebug(PREFIX "%d: shutdown\n", port->port_num);
			break;

		default:
			/* presumably a transient fault */
			pr_warn("%d: unexpected RX status %d\n",
				   port->port_num, req->status);
			/* FALLTHROUGH */
		case 0:
			/* normal completion */
			break;
		}

		/* push data to (open) tty */
		if (req->actual) {
			char *packet = req->buf;
			unsigned size = req->actual;
			unsigned n;
			int count;

			/* we may have pushed part of this packet already... */
			n = port->n_read;
			if (n) {
				packet += n;
				size -= n;
			}

			if (gs_marvell_diag_rx_callback !=
			    (marvell_diag_rx_callback) NULL) {
				int filtered;
				filtered =
				    gs_marvell_diag_rx_callback(packet, size);
				if (filtered)
					goto recycle;
			}

			count = tty_insert_flip_string(tty->port, packet, size);
			if (count)
				do_push = true;
			if (count != size) {
				/* stop pushing; TTY layer can't handle more */
				port->n_read += count;
				pr_vdebug(PREFIX "%d: rx block %d/%d\n",
					  port->port_num, count, req->actual);
				break;
			}
			port->n_read = 0;
		}
recycle:
		list_move(&req->list, &port->read_pool);
		port->read_started--;
	}

	/* Push from tty to ldisc; this is immediate with low_latency, and
	 * may trigger callbacks to this driver ... so drop the spinlock.
	 */
	if (tty && do_push) {
		spin_unlock_irq(&port->port_lock);
		tty_flip_buffer_push(tty->port);
		wake_up_interruptible(&tty->read_wait);
		spin_lock_irq(&port->port_lock);

		/* tty may have been closed */
		tty = port->port.tty;
	}

	/* We want our data queue to become empty ASAP, keeping data
	 * in the tty and ldisc (not here).  If we couldn't push any
	 * this time around, there may be trouble unless there's an
	 * implicit tty_unthrottle() call on its way...
	 *
	 * REVISIT we should probably add a timer to keep the tasklet
	 * from starving ... but it's not clear that case ever happens.
	 */
	if (!list_empty(queue) && tty) {
		if (!test_bit(TTY_THROTTLED, &tty->flags)) {
			if (do_push)
				tasklet_schedule(&port->push);
			else
				pr_warn("%d: RX not scheduled?\n",
					   port->port_num);
		}
	}

	/* If we're still connected, refill the USB RX queue. */
	if (!disconnect && port->port_usb)
		pxa910_gs_start_rx(port);

	spin_unlock_irq(&port->port_lock);
}

static int gs_marvell_diag_port_alloc(unsigned port_num,
			   struct usb_cdc_line_coding *coding)
{
	struct pxa910_gs_port *port;

	port = kzalloc(sizeof(struct pxa910_gs_port), GFP_KERNEL);
	if (port == NULL)
		return -ENOMEM;

	tty_port_init(&port->port);
	spin_lock_init(&port->port_lock);
	init_waitqueue_head(&port->drain_wait);

	tasklet_init(&port->push, gs_marvell_diag_rx_push, (unsigned long)port);

	INIT_LIST_HEAD(&port->read_pool);
	INIT_LIST_HEAD(&port->read_queue);
	INIT_LIST_HEAD(&port->write_pool);

	port->port_num = port_num;
	port->port_line_coding = *coding;
	port->tx_wq = create_singlethread_workqueue("diag_gadget_tx");
	INIT_WORK(&port->tx_work, pxa910_gs_tx_worker);
	pxa910_ports[port_num].port = port;

	return 0;
}

#ifdef DIAG_USB_TEST
static void diagUsbTest3(void)
{
	int i;
	int ret;
	unsigned long start, end, timeuse;

	memset(testbuf, 'b', sizeof(testbuf));
	pr_info("diag usb test start...\n");
	msleep_interruptible(1000);

	start = jiffies;
	for (i = 0; i < lpcunt; i++) {
		ret = gs_usb_write(gs_diag_tty, testbuf, sizeof(testbuf));
		if (ret != sizeof(testbuf))
			pr_info("usb write error!ret=%d\n", ret);

		while (!writecomplete) {
			if (wait_event_interruptible(wcwaitQ,
						writecomplete == 1))
				continue;
		}
		writecomplete = 0;
	}
	end = jiffies;
	timeuse = (end - start) * 1000 / HZ;
	pr_info("sending %d Kbytes to use take %d ms\n",
	       lpcunt * sizeof(testbuf) / 1024, timeuse);
}

static void diagUsbTest(int caseNo)
{

	init_waitqueue_head(&wcwaitQ);
	switch (caseNo) {
	case 1:
		/* diagUsbTest1(); */
		break;
	case 2:
		/* diagUsbTest2(); */
		break;
	case 3:
		diagUsbTest3();
		break;
	}
}
#endif

/*
 * gs_ioctl
 */
static int pxa910_diag_ioctl(struct tty_struct *tty, unsigned int cmd,
		    unsigned long arg)
{
	struct pxa910_gs_port *port;
	struct pxa910_f_diag *diag;
	int    rc = -ENOIOCTLCMD;

	if (tty == NULL) {
		pr_err("%s cmd %u. call from context [%d:%d]: tty is NULL pointer\n",
			   __func__, cmd, current->pid, current->tgid);
		return -EIO;
	}
	port = tty->driver_data;
	if (port == NULL) {
		pr_err("%s cmd %u. call from context [%d:%d]: tty->driver_data is NULL pointer\n",
			   __func__, cmd, current->pid, current->tgid);
		return -EIO;
	}
	diag = pxa910_port_to_diag(port->port_usb);
	if (diag == NULL) {
		pr_err("%s cmd %u. call from context [%d:%d]: diag is NULL pointer\n",
			   __func__, cmd, current->pid, current->tgid);
		return -EIO;
	}

	/* handle ioctls */
	if (gs_marvell_diag_ioctl)
		rc = gs_marvell_diag_ioctl(cmd, arg);

	return rc;
}

static const struct tty_operations gs_marvell_diag_tty_ops = {
	.open = pxa910_gs_open,
	.close = pxa910_gs_close,
	.write = gs_marvell_diag_write,
	.put_char = pxa910_gs_put_char,
	.flush_chars = pxa910_gs_flush_chars,
	.write_room = pxa910_gs_write_room,
	.chars_in_buffer = pxa910_gs_chars_in_buffer,
	.ioctl = pxa910_diag_ioctl,
	.unthrottle = pxa910_gs_unthrottle,
	.break_ctl = pxa910_gs_break_ctl,
};

int pxa910_diag_gserial_setup(struct usb_gadget *g, unsigned count)
{

#define GS_MARVELL_DIAG_MAJOR		126
#define GS_MARVELL_DIAG_MINOR_START	16

	unsigned i;
	struct usb_cdc_line_coding coding;
	int status;

	if (count == 0 || (GS_DIAG_PORT_BASE+count) > PXA910_N_PORTS)
		return -EINVAL;

	gs_diag_tty_driver = alloc_tty_driver(count);
	if (!gs_diag_tty_driver)
		return -ENOMEM;

	gs_diag_tty_driver->owner = THIS_MODULE;
	gs_diag_tty_driver->driver_name = "g_serial_diag";
	gs_diag_tty_driver->name = "ttydiag";
	/* uses dynamically assigned dev_t values */

	gs_diag_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	gs_diag_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	gs_diag_tty_driver->flags =
			TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	gs_diag_tty_driver->init_termios = tty_std_termios;

	/* 9600-8-N-1 ... matches defaults expected by "usbser.sys" on
	 * MS-Windows.  Otherwise, most of these flags shouldn't affect
	 * anything unless we were to actually hook up to a serial line.
	 */
	gs_diag_tty_driver->init_termios.c_cflag =
	    B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	gs_diag_tty_driver->init_termios.c_ispeed = 9600;
	gs_diag_tty_driver->init_termios.c_ospeed = 9600;

	gs_diag_tty_driver->major = GS_MARVELL_DIAG_MAJOR;
	gs_diag_tty_driver->minor_start = GS_MARVELL_DIAG_MINOR_START;

	coding.dwDTERate = cpu_to_le32(9600);
	coding.bCharFormat = 8;
	coding.bParityType = USB_CDC_NO_PARITY;
	coding.bDataBits = USB_CDC_1_STOP_BITS;

	tty_set_operations(gs_diag_tty_driver, &gs_marvell_diag_tty_ops);

	/* make devices be openable */
	for (i = 0; i < count; i++) {
		mutex_init(&pxa910_ports[GS_DIAG_PORT_BASE + i].lock);
		status =
		  gs_marvell_diag_port_alloc(GS_DIAG_PORT_BASE + i, &coding);
		if (status) {
			count = i;
			goto fail;
		}
	}
	n_diag_ports = count;

	/* export the driver ... */
	status = tty_register_driver(gs_diag_tty_driver);
	if (status) {
		pr_err("%s: cannot register, err %d\n", __func__, status);
		goto fail;
	}

	/* ... and sysfs class devices, so mdev/udev make /dev/ttyGS* */
	for (i = 0; i < count; i++) {
		struct device *tty_dev;
		struct pxa910_gs_port *port =
			pxa910_ports[GS_DIAG_PORT_BASE + i].port;

		tty_dev = tty_port_register_device(&port->port,
			gs_diag_tty_driver, i, &g->dev);
		if (IS_ERR(tty_dev))
			pr_warn("%s: no classdev for port %d, err %ld\n",
				   __func__, i, PTR_ERR(tty_dev));
	}

	pr_debug("%s: registered %d ttyGS* device%s\n", __func__,
		 count, (count == 1) ? "" : "s");

	return status;
fail:
	while (count--)
		kfree(pxa910_ports[GS_DIAG_PORT_BASE + count].port);
	put_tty_driver(gs_diag_tty_driver);
	gs_diag_tty_driver = NULL;
	return status;
}

/**
 * gserial_cleanup - remove TTY-over-USB driver and devices
 * Context: may sleep
 *
 * This is called to free all resources allocated by @gserial_setup().
 * Accordingly, it may need to wait until some open /dev/ files have
 * closed.
 *
 * The caller must have issued @gserial_disconnect() for any ports
 * that had previously been connected, so that there is never any
 * I/O pending when it's called.
 */
void pxa910_diag_gserial_cleanup(void)
{
	unsigned	i;
	struct pxa910_gs_port *port;

	if (!gs_diag_tty_driver)
		return;

	for (i = 0; i < n_diag_ports; i++) {
		/* prevent new opens */
		mutex_lock(&pxa910_ports[GS_DIAG_PORT_BASE + i].lock);
		port = pxa910_ports[GS_DIAG_PORT_BASE + i].port;
		pxa910_ports[GS_DIAG_PORT_BASE + i].port = NULL;
		mutex_unlock(&pxa910_ports[GS_DIAG_PORT_BASE + i].lock);

		tasklet_kill(&port->push);
		destroy_workqueue(port->tx_wq);
		/* wait for old opens to finish */
		wait_event(port->port.close_wait, pxa910_gs_closed(port));

		WARN_ON(port->port_usb != NULL);

		tty_port_destroy(&port->port);
		kfree(port);

		tty_unregister_device(gs_diag_tty_driver, i);
	}
	n_diag_ports = 0;

	tty_unregister_driver(gs_diag_tty_driver);
	put_tty_driver(gs_diag_tty_driver);
	gs_diag_tty_driver = NULL;

	pr_debug("%s: cleaned up ttyGS* support\n", __func__);
}


