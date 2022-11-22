/*
 * f_qc_acm.c -- USB CDC serial (ACM) function driver
 *
 * Copyright (C) 2003 Al Borchers (alborchers@steinerpoint.com)
 * Copyright (C) 2008 by David Brownell
 * Copyright (C) 2008 by Nokia Corporation
 * Copyright (C) 2009 by Samsung Electronics
 * Copyright (c) 2011 The Linux Foundation. All rights reserved.
 * Author: Michal Nazarewicz (mina86@mina86.com)
 *
 * This software is distributed under the terms of the GNU General
 * Public License ("GPL") as published by the Free Software Foundation,
 * either version 2 of that License or (at your option) any later version.
 */

/* #define VERBOSE_DEBUG */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <mach/usb_gadget_xport.h>

#include "u_serial.h"
#include "gadget_chips.h"
#ifdef CONFIG_USB_DUN_SUPPORT
extern void notify_control_line_state(u32 value);
extern int modem_register(void *data);
extern void modem_unregister(void);
#endif

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

struct f_serial {
	struct gserial			port;
	u8				ctrl_id, data_id;
	u8				port_num;
	enum transport_type		transport;

	u8				pending;

	/* lock is mostly for pending and notify_req ... they get accessed
	 * by callbacks both from tty (open/close/break) under its spinlock,
	 * and notify_req.complete() which can't use that lock.
	 */
	spinlock_t			lock;

	struct usb_ep			*notify;
	struct usb_request		*notify_req;

	struct usb_cdc_line_coding	port_line_coding;	/* 8-N-1 etc */

	/* SetControlLineState request -- CDC 1.1 section 6.2.14 (INPUT) */
	u16				port_handshake_bits;
#define ACM_CTRL_RTS	(1 << 1)	/* unused with full duplex */
#define ACM_CTRL_DTR	(1 << 0)	/* host is ready for data r/w */

	/* SerialState notification -- CDC 1.1 section 6.3.5 (OUTPUT) */
	u16				serial_state;
#define ACM_CTRL_OVERRUN	(1 << 6)
#define ACM_CTRL_PARITY		(1 << 5)
#define ACM_CTRL_FRAMING	(1 << 4)
#define ACM_CTRL_RI		(1 << 3)
#define ACM_CTRL_BRK		(1 << 2)
#define ACM_CTRL_DSR		(1 << 1)
#define ACM_CTRL_DCD		(1 << 0)
};

static unsigned int no_serial_tty_ports;
#if 0
static unsigned int no_serial_sdio_ports;
static unsigned int no_serial_smd_ports;
#endif
static unsigned int no_hsic_sports;
static unsigned int nr_serial_ports;
static unsigned int serial_next_free_port;

#define GSERIAL_NO_PORTS 4

static struct serial_port_info {
	enum transport_type	transport;
	unsigned		port_num;
	unsigned char		client_port_num;
} gserial_ports[GSERIAL_NO_PORTS];

static inline struct f_serial *func_to_serial(struct usb_function *f)
{
	return container_of(f, struct f_serial, port.func);
}

static inline struct f_serial *port_to_serial(struct gserial *p)
{
	return container_of(p, struct f_serial, port);
}

int serial_port_setup(struct usb_configuration *c)
{
	int ret = 0, i, port_idx;

	pr_debug("%s: no_serial_tty_ports:%u  nr_serial_ports:%u no_hsic_sports: %u\n",
			__func__, no_serial_tty_ports,
				nr_serial_ports, no_hsic_sports);

	if (no_serial_tty_ports) {
		for (i = 0; i < no_serial_tty_ports; i++) {
			ret = gserial_alloc_line(
					&gserial_ports[i].client_port_num);
			if (ret)
				return ret;
		}
	}
#if 0
	if (no_serial_sdio_ports)
		ret = gsdio_setup(c->cdev->gadget, no_serial_sdio_ports);
	if (no_serial_smd_ports)
		ret = gsmd_setup(c->cdev->gadget, no_serial_smd_ports);
#endif
	if (no_hsic_sports) {
		port_idx = ghsic_data_setup(no_hsic_sports, USB_GADGET_SERIAL);
		if (port_idx < 0)
			return port_idx;

		for (i = 0; i < nr_serial_ports; i++) {
			if (gserial_ports[i].transport ==
					USB_GADGET_XPORT_HSIC) {
				gserial_ports[i].client_port_num = port_idx;
				port_idx++;
			}
		}

		/*clinet port num is same for data setup and ctrl setup*/
		ret = ghsic_ctrl_setup(no_hsic_sports, USB_GADGET_SERIAL);
		if (ret < 0)
			return ret;
	}

	return ret;
}

void serial_port_cleanup(void)
{
	int i;

	for (i = 0; i < no_serial_tty_ports; i++)
		gserial_free_line(gserial_ports[i].client_port_num);
}

static int serial_port_connect(struct f_serial *serial)
{
	unsigned port_num;
	int ret = 0;

	port_num = gserial_ports[serial->port_num].client_port_num;


	pr_debug("%s: transport:%s f_serial:%p gserial:%p port_num:%d cl_port_no:%d\n",
			__func__, xport_to_str(serial->transport),
			serial, &serial->port, serial->port_num, port_num);

	switch (serial->transport) {
	case USB_GADGET_XPORT_TTY:
		gserial_connect(&serial->port, port_num);
		break;
#if 0
	case USB_GADGET_XPORT_SDIO:
		gsdio_connect(&serial->port, port_num);
		break;
	case USB_GADGET_XPORT_SMD:
		gsmd_connect(&serial->port, port_num);
		break;
#endif
	case USB_GADGET_XPORT_HSIC:
		ret = ghsic_ctrl_connect(&serial->port, port_num);
		if (ret) {
			pr_err("%s: ghsic_ctrl_connect failed: err:%d\n",
					__func__, ret);
			return ret;
		}
		ret = ghsic_data_connect(&serial->port, port_num);
		if (ret) {
			pr_err("%s: ghsic_data_connect failed: err:%d\n",
					__func__, ret);
			ghsic_ctrl_disconnect(&serial->port, port_num);
			return ret;
		}
		break;
	default:
		pr_err("%s: Un-supported transport: %s\n", __func__,
				xport_to_str(serial->transport));
		return -ENODEV;
	}

	return 0;
}

static int serial_port_disconnect(struct f_serial *serial)
{
	unsigned port_num;

	port_num = gserial_ports[serial->port_num].client_port_num;

	pr_debug("%s: transport:%s f_serial:%p gserial:%p port_num:%d cl_pno:%d\n",
			__func__, xport_to_str(serial->transport),
			serial, &serial->port, serial->port_num, port_num);

	switch (serial->transport) {
	case USB_GADGET_XPORT_TTY:
		gserial_disconnect(&serial->port);
		break;
#if 0
	case USB_GADGET_XPORT_SDIO:
		gsdio_disconnect(&serial->port, port_num);
		break;
	case USB_GADGET_XPORT_SMD:
		gsmd_disconnect(&serial->port, port_num);
		break;
#endif
	case USB_GADGET_XPORT_HSIC:
		ghsic_ctrl_disconnect(&serial->port, port_num);
		ghsic_data_disconnect(&serial->port, port_num);
		break;
	default:
		pr_err("%s: Un-supported transport:%s\n", __func__,
				xport_to_str(serial->transport));
		return -ENODEV;
	}

	return 0;
}
/*-------------------------------------------------------------------------*/

/* notification endpoint uses smallish and infrequent fixed-size messages */

#define GS_NOTIFY_INTERVAL_MS		32
#define GS_NOTIFY_MAXPACKET		10	/* notification + 2 bytes */

/* interface and class descriptors: */

static struct usb_interface_assoc_descriptor
serial_iad_descriptor = {
	.bLength =		sizeof serial_iad_descriptor,
	.bDescriptorType =	USB_DT_INTERFACE_ASSOCIATION,

	/* .bFirstInterface =	DYNAMIC, */
	.bInterfaceCount = 	2,	// control + data
	.bFunctionClass =	USB_CLASS_COMM,
	.bFunctionSubClass =	USB_CDC_SUBCLASS_ACM,
	.bFunctionProtocol =	USB_CDC_ACM_PROTO_AT_V25TER,
	/* .iFunction =		DYNAMIC */
};


static struct usb_interface_descriptor serial_control_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =	1,
	.bInterfaceClass =	USB_CLASS_COMM,
	.bInterfaceSubClass =	USB_CDC_SUBCLASS_ACM,
	.bInterfaceProtocol =	USB_CDC_ACM_PROTO_AT_V25TER,
	/* .iInterface = DYNAMIC */
};

static struct usb_interface_descriptor serial_data_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_CDC_DATA,
	.bInterfaceSubClass =	0,
	.bInterfaceProtocol =	0,
	/* .iInterface = DYNAMIC */
};

static struct usb_cdc_header_desc serial_header_desc = {
	.bLength =		sizeof(serial_header_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_HEADER_TYPE,
	.bcdCDC =		cpu_to_le16(0x0110),
};

static struct usb_cdc_call_mgmt_descriptor
serial_call_mgmt_descriptor = {
	.bLength =		sizeof(serial_call_mgmt_descriptor),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_CALL_MANAGEMENT_TYPE,
	.bmCapabilities =	0,
	/* .bDataInterface = DYNAMIC */
};

static struct usb_cdc_acm_descriptor serial_descriptor = {
	.bLength =		sizeof(serial_descriptor),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_ACM_TYPE,
	.bmCapabilities =	USB_CDC_CAP_LINE,
};

static struct usb_cdc_union_desc serial_union_desc = {
	.bLength =		sizeof(serial_union_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_UNION_TYPE,
	/* .bMasterInterface0 =	DYNAMIC */
	/* .bSlaveInterface0 =	DYNAMIC */
};

/* full speed support: */

static struct usb_endpoint_descriptor serial_fs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(GS_NOTIFY_MAXPACKET),
	.bInterval =		GS_NOTIFY_INTERVAL_MS,
};

static struct usb_endpoint_descriptor serial_fs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor serial_fs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *serial_fs_function[] = {
	(struct usb_descriptor_header *) &serial_iad_descriptor,
	(struct usb_descriptor_header *) &serial_control_interface_desc,
	(struct usb_descriptor_header *) &serial_header_desc,
	(struct usb_descriptor_header *) &serial_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &serial_descriptor,
	(struct usb_descriptor_header *) &serial_union_desc,
	(struct usb_descriptor_header *) &serial_fs_notify_desc,
	(struct usb_descriptor_header *) &serial_data_interface_desc,
	(struct usb_descriptor_header *) &serial_fs_in_desc,
	(struct usb_descriptor_header *) &serial_fs_out_desc,
	NULL,
};

/* high speed support: */
static struct usb_endpoint_descriptor serial_hs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(GS_NOTIFY_MAXPACKET),
	.bInterval =		USB_MS_TO_HS_INTERVAL(GS_NOTIFY_INTERVAL_MS),
};

static struct usb_endpoint_descriptor serial_hs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor serial_hs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_descriptor_header *serial_hs_function[] = {
	(struct usb_descriptor_header *) &serial_iad_descriptor,
	(struct usb_descriptor_header *) &serial_control_interface_desc,
	(struct usb_descriptor_header *) &serial_header_desc,
	(struct usb_descriptor_header *) &serial_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &serial_descriptor,
	(struct usb_descriptor_header *) &serial_union_desc,
	(struct usb_descriptor_header *) &serial_hs_notify_desc,
	(struct usb_descriptor_header *) &serial_data_interface_desc,
	(struct usb_descriptor_header *) &serial_hs_in_desc,
	(struct usb_descriptor_header *) &serial_hs_out_desc,
	NULL,
};

static struct usb_endpoint_descriptor serial_ss_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor serial_ss_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor serial_ss_bulk_comp_desc = {
	.bLength =              sizeof serial_ss_bulk_comp_desc,
	.bDescriptorType =      USB_DT_SS_ENDPOINT_COMP,
};

static struct usb_descriptor_header *serial_ss_function[] = {
	(struct usb_descriptor_header *) &serial_iad_descriptor,
	(struct usb_descriptor_header *) &serial_control_interface_desc,
	(struct usb_descriptor_header *) &serial_header_desc,
	(struct usb_descriptor_header *) &serial_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &serial_descriptor,
	(struct usb_descriptor_header *) &serial_union_desc,
	(struct usb_descriptor_header *) &serial_hs_notify_desc,
	(struct usb_descriptor_header *) &serial_ss_bulk_comp_desc,
	(struct usb_descriptor_header *) &serial_data_interface_desc,
	(struct usb_descriptor_header *) &serial_ss_in_desc,
	(struct usb_descriptor_header *) &serial_ss_bulk_comp_desc,
	(struct usb_descriptor_header *) &serial_ss_out_desc,
	(struct usb_descriptor_header *) &serial_ss_bulk_comp_desc,
	NULL,
};

/* string descriptors: */

#define ACM_CTRL_IDX	0
#define ACM_DATA_IDX	1
#define ACM_IAD_IDX	2

/* static strings, in UTF-8 */
static struct usb_string serial_string_defs[] = {
	[ACM_CTRL_IDX].s = "CDC Abstract Control Model (ACM)",
	[ACM_DATA_IDX].s = "CDC ACM Data",
	[ACM_IAD_IDX ].s = "CDC Serial",
	{  } /* end of list */
};

static struct usb_gadget_strings serial_string_table = {
	.language =		0x0409,	/* en-us */
	.strings =		serial_string_defs,
};

static struct usb_gadget_strings *serial_strings[] = {
	&serial_string_table,
	NULL,
};

/*-------------------------------------------------------------------------*/

/* ACM control ... data handling is delegated to tty library code.
 * The main task of this function is to activate and deactivate
 * that code based on device state; track parameters like line
 * speed, handshake state, and so on; and issue notifications.
 */

static void serial_complete_set_line_coding(struct usb_ep *ep,
		struct usb_request *req)
{
	struct f_serial	*serial = ep->driver_data;
	struct usb_composite_dev *cdev = serial->port.func.config->cdev;

	if (req->status != 0) {
		DBG(cdev, "serial ttyGS%d completion, err %d\n",
				serial->port_num, req->status);
		return;
	}

	/* normal completion */
	if (req->actual != sizeof(serial->port_line_coding)) {
		DBG(cdev, "serial ttyGS%d short resp, len %d\n",
				serial->port_num, req->actual);
		usb_ep_set_halt(ep);
	} else {
		struct usb_cdc_line_coding	*value = req->buf;

		/* REVISIT:  we currently just remember this data.
		 * If we change that, (a) validate it first, then
		 * (b) update whatever hardware needs updating,
		 * (c) worry about locking.  This is information on
		 * the order of 9600-8-N-1 ... most of which means
		 * nothing unless we control a real RS232 line.
		 */
		serial->port_line_coding = *value;
	}
}

static int serial_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct f_serial		*serial = func_to_serial(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	int			value = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u16			w_length = le16_to_cpu(ctrl->wLength);

	/* composite driver infrastructure handles everything except
	 * CDC class messages; interface activation uses set_alt().
	 *
	 * Note CDC spec table 4 lists the ACM request profile.  It requires
	 * encapsulated command support ... we don't handle any, and respond
	 * to them by stalling.  Options include get/set/clear comm features
	 * (not that useful) and SEND_BREAK.
	 */
	switch ((ctrl->bRequestType << 8) | ctrl->bRequest) {

	/* SET_LINE_CODING ... just read and save what the host sends */
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_REQ_SET_LINE_CODING:
		if (w_length != sizeof(struct usb_cdc_line_coding)
				|| w_index != serial->ctrl_id)
			goto invalid;

		value = w_length;
		cdev->gadget->ep0->driver_data = serial;
		req->complete = serial_complete_set_line_coding;
		break;

	/* GET_LINE_CODING ... return what host sent, or initial value */
	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_REQ_GET_LINE_CODING:
		if (w_index != serial->ctrl_id)
			goto invalid;

		value = min_t(unsigned, w_length,
				sizeof(struct usb_cdc_line_coding));
		memcpy(req->buf, &serial->port_line_coding, value);
		break;

	/* SET_CONTROL_LINE_STATE ... save what the host sent */
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_REQ_SET_CONTROL_LINE_STATE:
		if (w_index != serial->ctrl_id)
			goto invalid;

		value = 0;

		/* FIXME we should not allow data to flow until the
		 * host sets the ACM_CTRL_DTR bit; and when it clears
		 * that bit, we should return to that no-flow state.
		 */
#ifdef CONFIG_USB_DUN_SUPPORT
		notify_control_line_state((unsigned long)w_value);
#endif
		if (serial->port.notify_modem) {
			unsigned port_num =
				gserial_ports[serial->port_num].client_port_num;

			serial->port.notify_modem(&serial->port, port_num, w_value);
		}

		break;

	default:
invalid:
		VDBG(cdev, "invalid control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	/* respond with data transfer or status phase? */
	if (value >= 0) {
		DBG(cdev, "serial ttyGS%d req%02x.%02x v%04x i%04x l%d\n",
			serial->port_num, ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
		req->zero = 0;
		req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0)
			ERROR(cdev, "serial response on ttyGS%d, err %d\n",
					serial->port_num, value);
	}

	/* device either stalls (value < 0) or reports success */
	return value;
}

static int serial_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_serial		*serial = func_to_serial(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	/* we know alt == 0, so this is an activation or a reset */

	if (intf == serial->ctrl_id) {
		if (serial->notify->driver_data) {
			VDBG(cdev, "reset serial control interface %d\n", intf);
			usb_ep_disable(serial->notify);
		} else {
			VDBG(cdev, "init serial ctrl interface %d\n", intf);
			if (config_ep_by_speed(cdev->gadget, f, serial->notify))
				return -EINVAL;
		}
		usb_ep_enable(serial->notify);
		serial->notify->driver_data = serial;

	} else if (intf == serial->data_id) {
		if (serial->port.in->driver_data) {
			DBG(cdev, "reset serial ttyGS%d\n", serial->port_num);
			serial_port_disconnect(serial);
		}
		if (!serial->port.in->desc || !serial->port.out->desc) {
			DBG(cdev, "activate serial ttyGS%d\n", serial->port_num);
			if (config_ep_by_speed(cdev->gadget, f,
					       serial->port.in) ||
			    config_ep_by_speed(cdev->gadget, f,
					       serial->port.out)) {
				serial->port.in->desc = NULL;
				serial->port.out->desc = NULL;
				return -EINVAL;
			}
		}
		serial_port_connect(serial);

	} else
		return -EINVAL;

	return 0;
}

static void serial_disable(struct usb_function *f)
{
	struct f_serial	*serial = func_to_serial(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	DBG(cdev, "serial ttyGS%d deactivated\n", serial->port_num);
	serial_port_disconnect(serial);
	usb_ep_disable(serial->notify);
	serial->notify->driver_data = NULL;
}

/*-------------------------------------------------------------------------*/

/**
 * serial_cdc_notify - issue CDC notification to host
 * @serial: wraps host to be notified
 * @type: notification type
 * @value: Refer to cdc specs, wValue field.
 * @data: data to be sent
 * @length: size of data
 * Context: irqs blocked, serial->lock held, serial_notify_req non-null
 *
 * Returns zero on success or a negative errno.
 *
 * See section 6.3.5 of the CDC 1.1 specification for information
 * about the only notification we issue:  SerialState change.
 */
static int serial_cdc_notify(struct f_serial *serial, u8 type, u16 value,
		void *data, unsigned length)
{
	struct usb_ep			*ep = serial->notify;
	struct usb_request		*req;
	struct usb_cdc_notification	*notify;
	const unsigned			len = sizeof(*notify) + length;
	void				*buf;
	int				status;

	req = serial->notify_req;
	serial->notify_req = NULL;
	serial->pending = false;

	req->length = len;
	notify = req->buf;
	buf = notify + 1;

	notify->bmRequestType = USB_DIR_IN | USB_TYPE_CLASS
			| USB_RECIP_INTERFACE;
	notify->bNotificationType = type;
	notify->wValue = cpu_to_le16(value);
	notify->wIndex = cpu_to_le16(serial->ctrl_id);
	notify->wLength = cpu_to_le16(length);
	memcpy(buf, data, length);

	/* ep_queue() can complete immediately if it fills the fifo... */
	spin_unlock(&serial->lock);
	status = usb_ep_queue(ep, req, GFP_ATOMIC);
	spin_lock(&serial->lock);

	if (status < 0) {
		ERROR(serial->port.func.config->cdev,
				"serial ttyGS%d can't notify serial state, %d\n",
				serial->port_num, status);
		serial->notify_req = req;
	}

	return status;
}

static int serial_notify_serial_state(struct f_serial *serial)
{
	struct usb_composite_dev *cdev = serial->port.func.config->cdev;
	int			status;

	spin_lock(&serial->lock);
	if (serial->notify_req) {
		DBG(cdev, "serial ttyGS%d serial state %04x\n",
				serial->port_num, serial->serial_state);
		status = serial_cdc_notify(serial, USB_CDC_NOTIFY_SERIAL_STATE,
				0, &serial->serial_state, sizeof(serial->serial_state));
	} else {
		serial->pending = true;
		status = 0;
	}
	spin_unlock(&serial->lock);
	return status;
}

static void serial_cdc_notify_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_serial		*serial = req->context;
	u8			doit = false;

	/* on this call path we do NOT hold the port spinlock,
	 * which is why ACM needs its own spinlock
	 */
	spin_lock(&serial->lock);
	if (req->status != -ESHUTDOWN)
		doit = serial->pending;
	serial->notify_req = req;
	spin_unlock(&serial->lock);

	if (doit)
		serial_notify_serial_state(serial);
}

#ifdef CONFIG_USB_DUN_SUPPORT
void serial_notify(void *dev, u16 state)
{
	struct f_serial    *serial = (struct f_serial *)dev;

	if (serial) {
		serial->serial_state = state;
	serial_notify_serial_state(serial);
	}
}
EXPORT_SYMBOL(serial_notify);
#endif

/* connect == the TTY link is open */

static void serial_connect(struct gserial *port)
{
	struct f_serial		*serial = port_to_serial(port);

	serial->serial_state |= ACM_CTRL_DSR | ACM_CTRL_DCD;
	serial_notify_serial_state(serial);
}

static void serial_disconnect(struct gserial *port)
{
	struct f_serial		*serial = port_to_serial(port);

	serial->serial_state &= ~(ACM_CTRL_DSR | ACM_CTRL_DCD);
	serial_notify_serial_state(serial);
}

static int serial_send_break(struct gserial *port, int duration)
{
	struct f_serial		*serial = port_to_serial(port);
	u16			state;

	state = serial->serial_state;
	state &= ~ACM_CTRL_BRK;
	if (duration)
		state |= ACM_CTRL_BRK;

	serial->serial_state = state;
	return serial_notify_serial_state(serial);
}

static int serial_send_modem_ctrl_bits(struct gserial *port, int ctrl_bits)
{
	struct f_serial *serial = port_to_serial(port);

	serial->serial_state = ctrl_bits;

	return serial_notify_serial_state(serial);
}

/*-------------------------------------------------------------------------*/

/* ACM function driver setup/binding */
static int
serial_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_serial		*serial = func_to_serial(f);
	int			status;
	struct usb_ep		*ep;

	/* REVISIT might want instance-specific strings to help
	 * distinguish instances ...
	 */
	if (serial_string_defs[0].id == 0) {
		status = usb_string_ids_tab(c->cdev, serial_string_defs);
		if (status < 0)
			return status;
		serial_control_interface_desc.iInterface =
			serial_string_defs[ACM_CTRL_IDX].id;
		serial_data_interface_desc.iInterface =
			serial_string_defs[ACM_DATA_IDX].id;
		serial_iad_descriptor.iFunction = serial_string_defs[ACM_IAD_IDX].id;
	}

	/* allocate instance-specific interface IDs, and patch descriptors */
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	serial->ctrl_id = status;
	serial_iad_descriptor.bFirstInterface = status;

	serial_control_interface_desc.bInterfaceNumber = status;
	serial_union_desc .bMasterInterface0 = status;

	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	serial->data_id = status;

	serial_data_interface_desc.bInterfaceNumber = status;
	serial_union_desc.bSlaveInterface0 = status;
	serial_call_mgmt_descriptor.bDataInterface = status;

	status = -ENODEV;

	/* allocate instance-specific endpoints */
	ep = usb_ep_autoconfig(cdev->gadget, &serial_fs_in_desc);
	if (!ep)
		goto fail;
	serial->port.in = ep;
	ep->driver_data = cdev;	/* claim */

	ep = usb_ep_autoconfig(cdev->gadget, &serial_fs_out_desc);
	if (!ep)
		goto fail;
	serial->port.out = ep;
	ep->driver_data = cdev;	/* claim */

	ep = usb_ep_autoconfig(cdev->gadget, &serial_fs_notify_desc);
	if (!ep)
		goto fail;
	serial->notify = ep;
	ep->driver_data = cdev;	/* claim */

	/* allocate notification */
	serial->notify_req = gs_alloc_req(ep,
			sizeof(struct usb_cdc_notification) + 2,
			GFP_KERNEL);
	if (!serial->notify_req)
		goto fail;

	serial->notify_req->complete = serial_cdc_notify_complete;
	serial->notify_req->context = serial;

	/* support all relevant hardware speeds... we expect that when
	 * hardware is dual speed, all bulk-capable endpoints work at
	 * both speeds
	 */
	serial_hs_in_desc.bEndpointAddress = serial_fs_in_desc.bEndpointAddress;
	serial_hs_out_desc.bEndpointAddress = serial_fs_out_desc.bEndpointAddress;
	serial_hs_notify_desc.bEndpointAddress =
		serial_fs_notify_desc.bEndpointAddress;

	serial_ss_in_desc.bEndpointAddress = serial_fs_in_desc.bEndpointAddress;
	serial_ss_out_desc.bEndpointAddress = serial_fs_out_desc.bEndpointAddress;

	status = usb_assign_descriptors(f, serial_fs_function, serial_hs_function,
			serial_ss_function);
	if (status)
		goto fail;

	DBG(cdev, "serial ttyGS%d: %s speed IN/%s OUT/%s NOTIFY/%s\n",
			serial->port_num,
			gadget_is_superspeed(c->cdev->gadget) ? "super" :
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			serial->port.in->name, serial->port.out->name,
			serial->notify->name);
	/* To notify serial state by datarouter*/
	#ifdef CONFIG_USB_DUN_SUPPORT
	modem_register(serial);
	#endif
	return 0;

fail:
	if (serial->notify_req)
		gs_free_req(serial->notify, serial->notify_req);

	/* we might as well release our claims on endpoints */
	if (serial->notify)
		serial->notify->driver_data = NULL;
	if (serial->port.out)
		serial->port.out->driver_data = NULL;
	if (serial->port.in)
		serial->port.in->driver_data = NULL;

	ERROR(cdev, "%s/%p: can't bind, err %d\n", f->name, f, status);

	return status;
}

static void serial_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_serial		*serial = func_to_serial(f);

#ifdef CONFIG_USB_DUN_SUPPORT
        modem_unregister();
#endif
	serial_string_defs[0].id = 0;
	usb_free_all_descriptors(f);
	if (serial->notify_req)
		gs_free_req(serial->notify, serial->notify_req);
}

static void serial_free_func(struct usb_function *f)
{
	struct f_serial		*serial = func_to_serial(f);

	kfree(serial);
	serial_next_free_port--;
}

static struct usb_function *serial_alloc_func(struct usb_function_instance *fi)
{
	struct f_serial_opts *opts;
	struct f_serial *serial;

	serial = kzalloc(sizeof(*serial), GFP_KERNEL);
	if (!serial)
		return ERR_PTR(-ENOMEM);

	opts = container_of(fi, struct f_serial_opts, func_inst);

	spin_lock_init(&serial->lock);

	if (nr_serial_ports)
		opts->port_num = serial_next_free_port++;

	serial->transport = gserial_ports[opts->port_num].transport;
	serial->port.connect = serial_connect;
	serial->port.disconnect = serial_disconnect;
	serial->port.send_break = serial_send_break;
	serial->port.send_modem_ctrl_bits = serial_send_modem_ctrl_bits;

#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
	serial->port.func.name = kasprintf(GFP_KERNEL, "serial%u", opts->port_num);
#else
	//serial->port.func.name = kasprintf(GFP_KERNEL, "serial%u", opts->port_num + 1);
	serial->port.func.name = "serial";
#endif
	
	serial->port.func.strings = serial_strings;
	/* descriptors are per-instance copies */
	serial->port.func.bind = serial_bind;
	serial->port.func.set_alt = serial_set_alt;
	serial->port.func.setup = serial_setup;
	serial->port.func.disable = serial_disable;

	serial->port_num = opts->port_num;
	serial->port.func.unbind = serial_unbind;
	serial->port.func.free_func = serial_free_func;

	return &serial->port.func;
}

static inline struct f_serial_opts *to_f_serial_opts(struct config_item *item)
{
	return container_of(to_config_group(item), struct f_serial_opts,
			func_inst.group);
}

CONFIGFS_ATTR_STRUCT(f_serial_opts);
static ssize_t f_serial_attr_show(struct config_item *item,
				 struct configfs_attribute *attr,
				 char *page)
{
	struct f_serial_opts *opts = to_f_serial_opts(item);
	struct f_serial_opts_attribute *f_serial_opts_attr =
		container_of(attr, struct f_serial_opts_attribute, attr);
	ssize_t ret = 0;

	if (f_serial_opts_attr->show)
		ret = f_serial_opts_attr->show(opts, page);
	return ret;
}

static void serial_attr_release(struct config_item *item)
{
	struct f_serial_opts *opts = to_f_serial_opts(item);

	usb_put_function_instance(&opts->func_inst);
}

static struct configfs_item_operations serial_item_ops = {
	.release                = serial_attr_release,
	.show_attribute		= f_serial_attr_show,
};

static ssize_t f_serial_port_num_show(struct f_serial_opts *opts, char *page)
{
	return sprintf(page, "%u\n", opts->port_num);
}

static struct f_serial_opts_attribute f_serial_port_num =
	__CONFIGFS_ATTR_RO(port_num, f_serial_port_num_show);


static struct configfs_attribute *serial_attrs[] = {
	&f_serial_port_num.attr,
	NULL,
};

static struct config_item_type serial_func_type = {
	.ct_item_ops    = &serial_item_ops,
	.ct_attrs	= serial_attrs,
	.ct_owner       = THIS_MODULE,
};

static void serial_free_instance(struct usb_function_instance *fi)
{
	struct f_serial_opts *opts;

	opts = container_of(fi, struct f_serial_opts, func_inst);
	if (!nr_serial_ports)
		gserial_free_line(opts->port_num);

	kfree(opts);
}

static struct usb_function_instance *serial_alloc_instance(void)
{
	struct f_serial_opts *opts;
	int ret;

	opts = kzalloc(sizeof(*opts), GFP_KERNEL);
	if (!opts)
		return ERR_PTR(-ENOMEM);
	opts->func_inst.free_func_inst = serial_free_instance;
	if (!nr_serial_ports) {
		ret = gserial_alloc_line(&opts->port_num);
		if (ret) {
			kfree(opts);
			return ERR_PTR(ret);
		}
	}
	config_group_init_type_name(&opts->func_inst.group, "",
			&serial_func_type);
	return &opts->func_inst;
}
DECLARE_USB_FUNCTION_INIT(serial, serial_alloc_instance, serial_alloc_func);
MODULE_LICENSE("GPL");

/**
 * serial_init_port - bind a serial_port to its transport
 */
int serial_init_port(int port_num, const char *name)
{
	enum transport_type transport;

	if (port_num >= GSERIAL_NO_PORTS)
		return -ENODEV;

	transport = str_to_xport(name);
	pr_debug("%s, port:%d, transport:%s\n", __func__,
			port_num, xport_to_str(transport));

	gserial_ports[port_num].transport = transport;
	gserial_ports[port_num].port_num = port_num;

	switch (transport) {
	case USB_GADGET_XPORT_TTY:
		no_serial_tty_ports++;
		break;
#if 0
	case USB_GADGET_XPORT_SDIO:
		gserial_ports[port_num].client_port_num = no_serial_sdio_ports;
		no_serial_sdio_ports++;
		break;
	case USB_GADGET_XPORT_SMD:
		gserial_ports[port_num].client_port_num = no_serial_smd_ports;
		no_serial_smd_ports++;
		break;
#endif
	case USB_GADGET_XPORT_HSIC:
		ghsic_ctrl_set_port_name("serial_hsic", name);
		ghsic_data_set_port_name("serial_hsic", name);

		/*client port number will be updated in gport_setup*/
		no_hsic_sports++;
		break;
	default:
		pr_err("%s: Un-supported transport transport: %u\n",
				__func__, gserial_ports[port_num].transport);
		return -ENODEV;
	}

	nr_serial_ports++;

	return 0;
}
