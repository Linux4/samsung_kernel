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

struct pxa910_f_acm {
	struct pxa910_gserial		port;
	u8				ctrl_id, data_id;
	u8				port_num;

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
	u32 modem_state;
#define MODEM_DATA_MODE_OVER_PSD	0
#define MODEM_DATA_MODE_OVER_CSD	1
#define MODEM_CONTROL_MODE		2
	u64 time_stamp;
	u32 cid;
#define MODEM_INVALID_CID		(0xff)
};

static inline struct pxa910_f_acm *pxa910_func_to_acm(struct usb_function *f)
{
	return container_of(f, struct pxa910_f_acm, port.func);
}

static inline struct
pxa910_f_acm *pxa910_port_to_acm(struct pxa910_gserial *p)
{
	return container_of(p, struct pxa910_f_acm, port);
}

/*-------------------------------------------------------------------------*/

/* notification endpoint uses smallish and infrequent fixed-size messages */

#define GS_LOG2_NOTIFY_INTERVAL		5	/* 1 << 5 == 32 msec */
#define GS_NOTIFY_MAXPACKET		10	/* notification + 2 bytes */
#define GS_MODEM_PORT_BASE		0

/* interface and class descriptors: */

static struct usb_interface_assoc_descriptor
pxa910_acm_iad_descriptor = {
	.bLength =		sizeof(pxa910_acm_iad_descriptor),
	.bDescriptorType =	USB_DT_INTERFACE_ASSOCIATION,

	/* .bFirstInterface =	DYNAMIC, */
	.bInterfaceCount =	2,
	.bFunctionClass =	USB_CLASS_COMM,
	.bFunctionSubClass =	USB_CDC_SUBCLASS_ACM,
	.bFunctionProtocol =	USB_CDC_ACM_PROTO_AT_V25TER,
	/* .iFunction =		DYNAMIC */
};


static struct usb_interface_descriptor
pxa910_acm_control_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =	1,
	.bInterfaceClass =	USB_CLASS_COMM,
	.bInterfaceSubClass =	USB_CDC_SUBCLASS_ACM,
	.bInterfaceProtocol =	USB_CDC_ACM_PROTO_AT_V25TER,
	/* .iInterface = DYNAMIC */
};

static struct usb_interface_descriptor pxa910_acm_data_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_CDC_DATA,
	.bInterfaceSubClass =	0,
	.bInterfaceProtocol =	0,
	/* .iInterface = DYNAMIC */
};

static struct usb_cdc_header_desc pxa910_acm_header_desc = {
	.bLength =		sizeof(pxa910_acm_header_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_HEADER_TYPE,
	.bcdCDC =		cpu_to_le16(0x0110),
};

static struct usb_cdc_call_mgmt_descriptor
pxa910_acm_call_mgmt_descriptor = {
	.bLength =		sizeof(pxa910_acm_call_mgmt_descriptor),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_CALL_MANAGEMENT_TYPE,
	.bmCapabilities =	0,
	/* .bDataInterface = DYNAMIC */
};

static struct usb_cdc_acm_descriptor pxa910_acm_descriptor = {
	.bLength =		sizeof(pxa910_acm_descriptor),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_ACM_TYPE,
	.bmCapabilities =	USB_CDC_CAP_LINE,
};

static struct usb_cdc_union_desc pxa910_acm_union_desc = {
	.bLength =		sizeof(pxa910_acm_union_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_UNION_TYPE,
	/* .bMasterInterface0 =	DYNAMIC */
	/* .bSlaveInterface0 =	DYNAMIC */
};

/* full speed support: */

static struct usb_endpoint_descriptor pxa910_acm_fs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(GS_NOTIFY_MAXPACKET),
	.bInterval =		1 << GS_LOG2_NOTIFY_INTERVAL,
};

static struct usb_endpoint_descriptor pxa910_acm_fs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor pxa910_acm_fs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *pxa910_acm_fs_function[] = {
	(struct usb_descriptor_header *) &pxa910_acm_iad_descriptor,
	(struct usb_descriptor_header *) &pxa910_acm_control_interface_desc,
	(struct usb_descriptor_header *) &pxa910_acm_header_desc,
	(struct usb_descriptor_header *) &pxa910_acm_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &pxa910_acm_descriptor,
	(struct usb_descriptor_header *) &pxa910_acm_union_desc,
	(struct usb_descriptor_header *) &pxa910_acm_fs_notify_desc,
	(struct usb_descriptor_header *) &pxa910_acm_data_interface_desc,
	(struct usb_descriptor_header *) &pxa910_acm_fs_in_desc,
	(struct usb_descriptor_header *) &pxa910_acm_fs_out_desc,
	NULL,
};

/* high speed support: */

static struct usb_endpoint_descriptor pxa910_acm_hs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(GS_NOTIFY_MAXPACKET),
	.bInterval =		GS_LOG2_NOTIFY_INTERVAL+4,
};

static struct usb_endpoint_descriptor pxa910_acm_hs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor pxa910_acm_hs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_descriptor_header *pxa910_acm_hs_function[] = {
	(struct usb_descriptor_header *) &pxa910_acm_iad_descriptor,
	(struct usb_descriptor_header *) &pxa910_acm_control_interface_desc,
	(struct usb_descriptor_header *) &pxa910_acm_header_desc,
	(struct usb_descriptor_header *) &pxa910_acm_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &pxa910_acm_descriptor,
	(struct usb_descriptor_header *) &pxa910_acm_union_desc,
	(struct usb_descriptor_header *) &pxa910_acm_hs_notify_desc,
	(struct usb_descriptor_header *) &pxa910_acm_data_interface_desc,
	(struct usb_descriptor_header *) &pxa910_acm_hs_in_desc,
	(struct usb_descriptor_header *) &pxa910_acm_hs_out_desc,
	NULL,
};

static struct usb_endpoint_descriptor pxa910_acm_ss_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor pxa910_acm_ss_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor pxa910_acm_ss_bulk_comp_desc = {
	.bLength =              sizeof(pxa910_acm_ss_bulk_comp_desc),
	.bDescriptorType =      USB_DT_SS_ENDPOINT_COMP,
};

static struct usb_descriptor_header *pxa910_acm_ss_function[] = {
	(struct usb_descriptor_header *) &pxa910_acm_iad_descriptor,
	(struct usb_descriptor_header *) &pxa910_acm_control_interface_desc,
	(struct usb_descriptor_header *) &pxa910_acm_header_desc,
	(struct usb_descriptor_header *) &pxa910_acm_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &pxa910_acm_descriptor,
	(struct usb_descriptor_header *) &pxa910_acm_union_desc,
	(struct usb_descriptor_header *) &pxa910_acm_hs_notify_desc,
	(struct usb_descriptor_header *) &pxa910_acm_ss_bulk_comp_desc,
	(struct usb_descriptor_header *) &pxa910_acm_data_interface_desc,
	(struct usb_descriptor_header *) &pxa910_acm_ss_in_desc,
	(struct usb_descriptor_header *) &pxa910_acm_ss_bulk_comp_desc,
	(struct usb_descriptor_header *) &pxa910_acm_ss_out_desc,
	(struct usb_descriptor_header *) &pxa910_acm_ss_bulk_comp_desc,
	NULL,
};

/* string descriptors: */

#define ACM_CTRL_IDX	0
#define ACM_DATA_IDX	1
#define ACM_IAD_IDX	2

/* static strings, in UTF-8 */
static struct usb_string pxa910_acm_string_defs[] = {
	[ACM_CTRL_IDX].s = "CDC Abstract Control Model (ACM)",
	[ACM_DATA_IDX].s = "CDC ACM Data",
	[ACM_IAD_IDX].s = "CDC Serial",
	{  /* ZEROES END LIST */ },
};

static struct usb_gadget_strings pxa910_acm_string_table = {
	.language =		0x0409,	/* en-us */
	.strings =		pxa910_acm_string_defs,
};

static struct usb_gadget_strings *pxa910_acm_strings[] = {
	&pxa910_acm_string_table,
	NULL,
};

/*-------------------------------------------------------------------------*/

/* ACM control ... data handling is delegated to tty library code.
 * The main task of this function is to activate and deactivate
 * that code based on device state; track parameters like line
 * speed, handshake state, and so on; and issue notifications.
 */

static void pxa910_acm_complete_set_line_coding(struct usb_ep *ep,
		struct usb_request *req)
{
	struct pxa910_f_acm	*acm = ep->driver_data;
	struct usb_composite_dev *cdev = acm->port.func.config->cdev;

	if (req->status != 0) {
		DBG(cdev, "acm ttyGS%d completion, err %d\n",
				acm->port_num, req->status);
		return;
	}

	/* normal completion */
	if (req->actual != sizeof(acm->port_line_coding)) {
		DBG(cdev, "acm ttyGS%d short resp, len %d\n",
				acm->port_num, req->actual);
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
		acm->port_line_coding = *value;
	}
}

static int
pxa910_acm_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct pxa910_f_acm	*acm = pxa910_func_to_acm(f);
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
				|| w_index != acm->ctrl_id)
			goto invalid;

		value = w_length;
		cdev->gadget->ep0->driver_data = acm;
		req->complete = pxa910_acm_complete_set_line_coding;
		break;

	/* GET_LINE_CODING ... return what host sent, or initial value */
	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_REQ_GET_LINE_CODING:
		if (w_index != acm->ctrl_id)
			goto invalid;

		value = min_t(unsigned, w_length,
				sizeof(struct usb_cdc_line_coding));
		memcpy(req->buf, &acm->port_line_coding, value);
		break;

	/* SET_CONTROL_LINE_STATE ... save what the host sent */
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_REQ_SET_CONTROL_LINE_STATE:
		if (w_index != acm->ctrl_id)
			goto invalid;

		value = 0;

		/* FIXME we should not allow data to flow until the
		 * host sets the ACM_CTRL_DTR bit; and when it clears
		 * that bit, we should return to that no-flow state.
		 */
		acm->port_handshake_bits = w_value;
		break;

	default:
invalid:
		VDBG(cdev, "invalid control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	/* respond with data transfer or status phase? */
	if (value >= 0) {
		DBG(cdev, "acm ttyGS%d req%02x.%02x v%04x i%04x l%d\n",
			acm->port_num, ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
		req->zero = 0;
		req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0)
			ERROR(cdev, "acm response on ttyGS%d, err %d\n",
					acm->port_num, value);
	}

	/* device either stalls (value < 0) or reports success */
	return value;
}

static int
pxa910_acm_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct pxa910_f_acm	*acm = pxa910_func_to_acm(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	/* we know alt == 0, so this is an activation or a reset */

	if (intf == acm->ctrl_id) {
		if (acm->notify->driver_data) {
			VDBG(cdev, "reset acm control interface %d\n", intf);
			usb_ep_disable(acm->notify);
		} else {
			VDBG(cdev, "init acm ctrl interface %d\n", intf);
			if (config_ep_by_speed(cdev->gadget, f, acm->notify))
				return -EINVAL;
		}
		usb_ep_enable(acm->notify);
		acm->notify->driver_data = acm;

	} else if (intf == acm->data_id) {
		if (acm->port.in->driver_data) {
			DBG(cdev, "reset acm ttyGS%d\n", acm->port_num);
			pxa910_gserial_disconnect(&acm->port);
		}
		if (!acm->port.in->desc || !acm->port.out->desc) {
			DBG(cdev, "activate acm ttyGS%d\n", acm->port_num);
			if (config_ep_by_speed(cdev->gadget, f,
					       acm->port.in) ||
			    config_ep_by_speed(cdev->gadget, f,
					       acm->port.out)) {
				acm->port.in->desc = NULL;
				acm->port.out->desc = NULL;
				return -EINVAL;
			}
		}
		pxa910_gserial_connect(&acm->port, acm->port_num);

	} else
		return -EINVAL;

	return 0;
}

static void pxa910_acm_disable(struct usb_function *f)
{
	struct pxa910_f_acm	*acm = pxa910_func_to_acm(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	DBG(cdev, "acm ttyGS%d deactivated\n", acm->port_num);
	acm->modem_state = MODEM_CONTROL_MODE;

	pxa910_gserial_disconnect(&acm->port);
	usb_ep_disable(acm->notify);
	acm->notify->driver_data = NULL;
}

/*-------------------------------------------------------------------------*/

/**
 * acm_cdc_notify - issue CDC notification to host
 * @acm: wraps host to be notified
 * @type: notification type
 * @value: Refer to cdc specs, wValue field.
 * @data: data to be sent
 * @length: size of data
 * Context: irqs blocked, acm->lock held, acm_notify_req non-null
 *
 * Returns zero on success or a negative errno.
 *
 * See section 6.3.5 of the CDC 1.1 specification for information
 * about the only notification we issue:  SerialState change.
 */
static int pxa910_acm_cdc_notify(struct pxa910_f_acm *acm, u8 type, u16 value,
		void *data, unsigned length)
{
	struct usb_ep			*ep = acm->notify;
	struct usb_request		*req;
	struct usb_cdc_notification	*notify;
	const unsigned			len = sizeof(*notify) + length;
	void				*buf;
	int				status;

	req = acm->notify_req;
	acm->notify_req = NULL;
	acm->pending = false;

	req->length = len;
	notify = req->buf;
	buf = notify + 1;

	notify->bmRequestType = USB_DIR_IN | USB_TYPE_CLASS
			| USB_RECIP_INTERFACE;
	notify->bNotificationType = type;
	notify->wValue = cpu_to_le16(value);
	notify->wIndex = cpu_to_le16(acm->ctrl_id);
	notify->wLength = cpu_to_le16(length);
	memcpy(buf, data, length);

	/* ep_queue() can complete immediately if it fills the fifo... */
	spin_unlock(&acm->lock);
	status = usb_ep_queue(ep, req, GFP_ATOMIC);
	spin_lock(&acm->lock);

	if (status < 0) {
		ERROR(acm->port.func.config->cdev,
				"acm ttyGS%d can't notify serial state, %d\n",
				acm->port_num, status);
		acm->notify_req = req;
	}

	return status;
}

static int pxa910_acm_notify_serial_state(struct pxa910_f_acm *acm)
{
	struct usb_composite_dev *cdev = acm->port.func.config->cdev;
	int			status;

	spin_lock(&acm->lock);
	if (acm->notify_req) {
		DBG(cdev, "acm ttyGS%d serial state %04x\n",
				acm->port_num, acm->serial_state);
		status = pxa910_acm_cdc_notify(acm, USB_CDC_NOTIFY_SERIAL_STATE,
				0, &acm->serial_state,
				sizeof(acm->serial_state));
	} else {
		acm->pending = true;
		status = 0;
	}
	spin_unlock(&acm->lock);
	return status;
}

static void
pxa910_acm_cdc_notify_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct pxa910_f_acm	*acm = req->context;
	u8			doit = false;

	/* on this call path we do NOT hold the port spinlock,
	 * which is why ACM needs its own spinlock
	 */
	spin_lock(&acm->lock);
	if (req->status != -ESHUTDOWN)
		doit = acm->pending;
	acm->notify_req = req;
	spin_unlock(&acm->lock);

	if (doit)
		pxa910_acm_notify_serial_state(acm);
}

/* connect == the TTY link is open */

static void pxa910_acm_connect(struct pxa910_gserial *port)
{
	struct pxa910_f_acm	*acm = pxa910_port_to_acm(port);

	acm->serial_state |= ACM_CTRL_DSR | ACM_CTRL_DCD;
	pxa910_acm_notify_serial_state(acm);
}

static void pxa910_acm_disconnect(struct pxa910_gserial *port)
{
	struct pxa910_f_acm	*acm = pxa910_port_to_acm(port);

	acm->serial_state &= ~(ACM_CTRL_DSR | ACM_CTRL_DCD);
	pxa910_acm_notify_serial_state(acm);
}

static int pxa910_acm_send_break(struct pxa910_gserial *port, int duration)
{
	struct pxa910_f_acm	*acm = pxa910_port_to_acm(port);
	u16			state;

	state = acm->serial_state;
	state &= ~ACM_CTRL_BRK;
	if (duration)
		state |= ACM_CTRL_BRK;

	acm->serial_state = state;
	return pxa910_acm_notify_serial_state(acm);
}

/*-------------------------------------------------------------------------*/

/* ACM function driver setup/binding */
static int
pxa910_acm_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct pxa910_f_acm	*acm = pxa910_func_to_acm(f);
	int			status;
	struct usb_ep		*ep;

	/* allocate instance-specific interface IDs, and patch descriptors */
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	acm->ctrl_id = status;
	pxa910_acm_iad_descriptor.bFirstInterface = status;

	pxa910_acm_control_interface_desc.bInterfaceNumber = status;
	pxa910_acm_union_desc.bMasterInterface0 = status;

	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	acm->data_id = status;

	pxa910_acm_data_interface_desc.bInterfaceNumber = status;
	pxa910_acm_union_desc.bSlaveInterface0 = status;
	pxa910_acm_call_mgmt_descriptor.bDataInterface = status;

	status = -ENODEV;

	/* allocate instance-specific endpoints */
	ep = usb_ep_autoconfig(cdev->gadget, &pxa910_acm_fs_in_desc);
	if (!ep)
		goto fail;
	acm->port.in = ep;
	ep->driver_data = cdev;	/* claim */

	ep = usb_ep_autoconfig(cdev->gadget, &pxa910_acm_fs_out_desc);
	if (!ep)
		goto fail;
	acm->port.out = ep;
	ep->driver_data = cdev;	/* claim */

	ep = usb_ep_autoconfig(cdev->gadget, &pxa910_acm_fs_notify_desc);
	if (!ep)
		goto fail;
	acm->notify = ep;
	ep->driver_data = cdev;	/* claim */

	/* allocate notification */
	acm->notify_req = pxa910_gs_alloc_req(ep,
			sizeof(struct usb_cdc_notification) + 2,
			GFP_KERNEL);
	if (!acm->notify_req)
		goto fail;

	acm->notify_req->complete = pxa910_acm_cdc_notify_complete;
	acm->notify_req->context = acm;

	/* copy descriptors */
	f->fs_descriptors = usb_copy_descriptors(pxa910_acm_fs_function);
	if (!f->fs_descriptors)
		goto fail;

	/* support all relevant hardware speeds... we expect that when
	 * hardware is dual speed, all bulk-capable endpoints work at
	 * both speeds
	 */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		pxa910_acm_hs_in_desc.bEndpointAddress =
				pxa910_acm_fs_in_desc.bEndpointAddress;
		pxa910_acm_hs_out_desc.bEndpointAddress =
				pxa910_acm_fs_out_desc.bEndpointAddress;
		pxa910_acm_hs_notify_desc.bEndpointAddress =
				pxa910_acm_fs_notify_desc.bEndpointAddress;

		/* copy descriptors */
		f->hs_descriptors =
			usb_copy_descriptors(pxa910_acm_hs_function);
	}
	if (gadget_is_superspeed(c->cdev->gadget)) {
		pxa910_acm_ss_in_desc.bEndpointAddress =
			pxa910_acm_fs_in_desc.bEndpointAddress;
		pxa910_acm_ss_out_desc.bEndpointAddress =
			pxa910_acm_fs_out_desc.bEndpointAddress;

		/* copy descriptors, and track endpoint copies */
		f->ss_descriptors =
			usb_copy_descriptors(pxa910_acm_ss_function);
		if (!f->ss_descriptors)
			goto fail;
	}

	DBG(cdev, "acm ttyGS%d: %s speed IN/%s OUT/%s NOTIFY/%s\n",
			acm->port_num,
			gadget_is_superspeed(c->cdev->gadget) ? "super" :
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			acm->port.in->name, acm->port.out->name,
			acm->notify->name);
	return 0;

fail:
	if (acm->notify_req)
		pxa910_gs_free_req(acm->notify, acm->notify_req);

	/* we might as well release our claims on endpoints */
	if (acm->notify)
		acm->notify->driver_data = NULL;
	if (acm->port.out)
		acm->port.out->driver_data = NULL;
	if (acm->port.in)
		acm->port.in->driver_data = NULL;

	ERROR(cdev, "%s/%p: can't bind, err %d\n", f->name, f, status);

	return status;
}

static void
pxa910_acm_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct pxa910_f_acm	*acm = pxa910_func_to_acm(f);

	if (gadget_is_dualspeed(c->cdev->gadget))
		usb_free_descriptors(f->hs_descriptors);
	if (gadget_is_superspeed(c->cdev->gadget))
		usb_free_descriptors(f->ss_descriptors);
	usb_free_descriptors(f->fs_descriptors);
	pxa910_gs_free_req(acm->notify, acm->notify_req);
	kfree(acm);
}

/* Some controllers can't support CDC ACM ... */
static inline bool pxa910_can_support_cdc(struct usb_configuration *c)
{
	/* everything else is *probably* fine ... */
	return true;
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
int pxa910_acm_bind_config(struct usb_configuration *c, u8 port_num)
{
	struct pxa910_f_acm	*acm;
	int		status;

	if (!pxa910_can_support_cdc(c))
		return -EINVAL;

	/* REVISIT might want instance-specific strings to help
	 * distinguish instances ...
	 */

	/* maybe allocate device-global string IDs, and patch descriptors */
	if (pxa910_acm_string_defs[ACM_CTRL_IDX].id == 0) {
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		pxa910_acm_string_defs[ACM_CTRL_IDX].id = status;

		pxa910_acm_control_interface_desc.iInterface = status;

		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		pxa910_acm_string_defs[ACM_DATA_IDX].id = status;

		pxa910_acm_data_interface_desc.iInterface = status;

		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		pxa910_acm_string_defs[ACM_IAD_IDX].id = status;

		pxa910_acm_iad_descriptor.iFunction = status;
	}

	/* allocate and initialize one new instance */
	acm = kzalloc(sizeof(*acm), GFP_KERNEL);
	if (!acm)
		return -ENOMEM;

	spin_lock_init(&acm->lock);

	acm->modem_state = MODEM_CONTROL_MODE;
	acm->cid = MODEM_INVALID_CID;
	acm->port_num = port_num;

	acm->port.connect = pxa910_acm_connect;
	acm->port.disconnect = pxa910_acm_disconnect;
	acm->port.send_break = pxa910_acm_send_break;

	acm->port.func.name = "acm";
	acm->port.func.strings = pxa910_acm_strings;
	/* descriptors are per-instance copies */
	acm->port.func.bind = pxa910_acm_bind;
	acm->port.func.unbind = pxa910_acm_unbind;
	acm->port.func.set_alt = pxa910_acm_set_alt;
	acm->port.func.setup = pxa910_acm_setup;
	acm->port.func.disable = pxa910_acm_disable;

	init_waitqueue_head(&acm->port.port_send);
	status = usb_add_function(c, &acm->port.func);
	if (status)
		kfree(acm);
	return status;
}

int gs_marvell_modem_send(const unsigned char *buf,
				int count, unsigned char cid)
{
	int buf_avail = 0;
	struct pxa910_gs_port *port = NULL;
	struct tty_struct *tty;
	struct pxa910_f_acm *acm;
	unsigned long flags;
	wait_queue_head_t *p_port_send;
	u32 modem_state;

	/* Assume that we have only one port */
	port = pxa910_ports[GS_MODEM_PORT_BASE].port;
	if (NULL == port)
		return count;

	spin_lock_irqsave(&port->port_lock, flags);

	tty = port->port.tty;

	if (tty == NULL || tty->driver_data == NULL || port->port_usb == NULL) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		return count;
	}

	spin_unlock_irqrestore(&port->port_lock, flags);

	/* sync with gs_ioctl, get the same f_acm!*/
	acm = pxa910_port_to_acm(port->port_usb);

	spin_lock_irqsave(&port->port_lock, flags);
	modem_state = acm->modem_state;
	spin_unlock_irqrestore(&port->port_lock, flags);

	if (modem_state != MODEM_CONTROL_MODE) {
		int c, retval = 0;
		const unsigned char *b = buf;
		unsigned long start, end;
		int log_count = 0;

		start = jiffies;

		while (count > 0) {
			c = pxa910_gs_write(tty, b, count);

			if (c == count) {
				b += c;
				count -= c;
				break; /* send all bytes successfully */
			} else if (c >= 0) {
				b += c;
				count -= c;
				continue;
			} else if (c < 0) {
				retval = c;
				if (c != -EAGAIN)
					goto break_out;
			}

			/* retry to wait enough buffer to send the rest bytes.
			 * timeout 100msec
			 */
			spin_lock_irqsave(&port->port_lock, flags);

			if (port->port_usb) {
				p_port_send = &port->port_usb->port_send;
			} else {
				p_port_send = NULL;
				pr_info("%s: usb port is released.\n",
						__func__);
				spin_unlock_irqrestore(&port->port_lock, flags);
				break;
			}

			spin_unlock_irqrestore(&port->port_lock, flags);

			usleep_range(5000, 6000);
			log_count++;

			/* from enter sleep to wake up,
			   usb connection could be changed */
			if (!port->port_usb) {
				pr_info("%s: usb port is released.\n",
						__func__);
				break;
			}
			if (log_count == 100 && port) {
				buf_avail = pxa910_gs_buf_space_avail(
						&port->port_write_buf);

				pr_info("%s: cannot send the packet over 10s! (%d)\n",
						 __func__, buf_avail);
				log_count = 0;
			}
		}

break_out:
		end = jiffies;

		return (b - buf) ? b - buf : retval;
	} else
		pr_err("Dropped data packet\n");
	return 0;
}
EXPORT_SYMBOL(gs_marvell_modem_send);

typedef void (*marvell_modem_rx_callback) (unsigned char cid, char *packet,
					   int len);

marvell_modem_rx_callback gs_marvell_modem_rx_psd_callback =
	(marvell_modem_rx_callback) NULL;
EXPORT_SYMBOL(gs_marvell_modem_rx_psd_callback);

marvell_modem_rx_callback gs_marvell_modem_rx_csd_callback =
	(marvell_modem_rx_callback) NULL;
EXPORT_SYMBOL(gs_marvell_modem_rx_csd_callback);


/*
 * The following callback is used for notifying a client on the IOCTL
 * event that was received by the modem.
 * This notification is needed by the AP implementation of the
 * PPP module.
 */
typedef int (*marvell_modem_ioctl_notify_callback)
		(struct tty_struct *tty, unsigned int cmd, unsigned long arg);

marvell_modem_ioctl_notify_callback
	gs_marvell_modem_ioctl_notify_callback =
	(marvell_modem_ioctl_notify_callback) NULL;
EXPORT_SYMBOL(gs_marvell_modem_ioctl_notify_callback);

/*
 * gs_marvell_modem_send_to_atcmdsrv
 *
 * This function allows for direct connection between
 * the PPP server and the AT Command Server.
 * This functionality is needed because during PPP handshake,
 * it needs to send AT commands to the AT Command Server
 * (mimicking the path from PC to AT Command Server)
 */
int gs_marvell_modem_send_to_atcmdsrv(char *buf, int count)
{
	struct pxa910_gs_port *port = NULL;
	struct tty_struct *tty;
	struct pxa910_f_acm *acm;
	unsigned long flags, modem_state;

	/* Assume that we have only one port */
	port = pxa910_ports[GS_MODEM_PORT_BASE].port;

	spin_lock_irqsave(&port->port_lock, flags);

	tty = port->port.tty;

	if (tty == NULL || tty->driver_data == NULL || port->port_usb == NULL) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		return count;
	}

	spin_unlock_irqrestore(&port->port_lock, flags);

	/* sync with gs_ioctl, get the same f_acm!*/
	acm = pxa910_port_to_acm(port->port_usb);

	spin_lock_irqsave(&port->port_lock, flags);
	modem_state = acm->modem_state;
	spin_unlock_irqrestore(&port->port_lock, flags);

	if (modem_state != MODEM_CONTROL_MODE) {

		count = tty_insert_flip_string(tty->port, buf, count);

		if (count) {

			tty_flip_buffer_push(tty->port);

			wake_up_interruptible(&tty->read_wait);

		}
	} else
		pr_err("%s: Message from TTY dropped\n", __func__);

	return count;
}
EXPORT_SYMBOL(gs_marvell_modem_send_to_atcmdsrv);

static int gs_marvell_modem_write(struct tty_struct *tty,
				  const unsigned char *buf, int count)
{
	struct pxa910_gs_port *port = tty->driver_data;
	struct pxa910_f_acm *acm = pxa910_port_to_acm(port->port_usb);
	unsigned long flags;

	spin_lock_irqsave(&port->port_lock, flags);
	if (acm && acm->modem_state != MODEM_CONTROL_MODE) {
		if (acm->modem_state == MODEM_DATA_MODE_OVER_PSD) {
			if (gs_marvell_modem_rx_psd_callback !=
				    (marvell_modem_rx_callback) NULL) {
					gs_marvell_modem_rx_psd_callback(0xFF,
								(char *)buf,
								count);
				}
		}
		spin_unlock_irqrestore(&port->port_lock, flags);
		return count;
	}
	spin_unlock_irqrestore(&port->port_lock, flags);

	return pxa910_gs_write(tty, buf, count);
}

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
static void gs_marvell_modem_rx_push(unsigned long _port)
{
	struct pxa910_gs_port *port = (void *)_port;
	struct pxa910_f_acm *acm = pxa910_port_to_acm(port->port_usb);
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
			/* for modem data filter */
			if (size >= 3) {
				if ((packet[0] == '+') && (packet[1] == '+')
				    && (packet[2] == '+')) {
					pr_err("received escape charaters +++\n");
					if (acm->time_stamp -
					    (now.tv_sec * 1000000000L +
					     now.tv_nsec) >= 1000000000L) {
						acm->modem_state =
						    MODEM_CONTROL_MODE;
						/* when receive software hangup,
						     drop CD asap.
						*/
						spin_unlock_irq(
							&port->port_lock);
						pxa910_acm_disconnect(
							&acm->port);
						spin_lock_irq(&port->port_lock);
					}
				}
			}
			if (acm->modem_state == MODEM_DATA_MODE_OVER_PSD) {
				if (gs_marvell_modem_rx_psd_callback !=
				    (marvell_modem_rx_callback) NULL) {
					gs_marvell_modem_rx_psd_callback(acm->
									 cid,
									 packet,
									 size);
					acm->time_stamp =
					    now.tv_sec * 1000000000L +
					    now.tv_nsec;
				}
				goto recycle;
			} else if (acm->modem_state ==
					MODEM_DATA_MODE_OVER_CSD) {
				if (gs_marvell_modem_rx_csd_callback !=
				    (marvell_modem_rx_callback) NULL) {
					gs_marvell_modem_rx_csd_callback(acm->
									 cid,
									 packet,
									 size);
					acm->time_stamp =
					    now.tv_sec * 1000000000L +
					    now.tv_nsec;
				}
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

static int gs_marvell_modem_port_alloc(unsigned port_num,
			    struct usb_cdc_line_coding *coding)
{
	struct pxa910_gs_port *port;

	port = kzalloc(sizeof(struct pxa910_gs_port), GFP_KERNEL);
	if (port == NULL)
		return -ENOMEM;

	tty_port_init(&port->port);
	spin_lock_init(&port->port_lock);
	init_waitqueue_head(&port->drain_wait);

	tasklet_init(&port->push, gs_marvell_modem_rx_push,
		     (unsigned long)port);

	INIT_LIST_HEAD(&port->read_pool);
	INIT_LIST_HEAD(&port->read_queue);
	INIT_LIST_HEAD(&port->write_pool);

	port->port_num = port_num;
	port->port_line_coding = *coding;
	port->tx_wq = create_singlethread_workqueue("modem_gadget_tx");
	INIT_WORK(&port->tx_work, pxa910_gs_tx_worker);
	pxa910_ports[port_num].port = port;

	return 0;
}

#define TIOENABLE       _IOW('T', 206, int)	/* enable data */
#define TIODISABLE      _IOW('T', 207, int)	/* disable data */
#define TIOPPPON        _IOW('T', 208, int)
#define TIOPPPOFF       _IOW('T', 209, int)
#define TIOPPPONCSD     _IOW('T', 211, int)
/*
 * gs_ioctl
 */
static int pxa910_gs_ioctl(struct tty_struct *tty, unsigned int cmd,
		    unsigned long arg)
{
	struct pxa910_gs_port *port;
	struct pxa910_f_acm *acm;
	int    rc = 0;

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
	acm = pxa910_port_to_acm(port->port_usb);
	if (acm == NULL) {
		pr_err("%s cmd %u. call from context [%d:%d]: acm is NULL pointer\n",
			   __func__, cmd, current->pid, current->tgid);
		return -EIO;
	}

	switch (cmd) {
	case TIOENABLE:
		acm->cid = (unsigned char)arg;
		break;
	case TIODISABLE:
		acm->cid = (unsigned char)arg;
		break;
	case TIOPPPON:
		acm->cid = (unsigned char)arg;
		acm->modem_state = MODEM_DATA_MODE_OVER_PSD;
		acm->serial_state |= ACM_CTRL_DSR | ACM_CTRL_DCD;
		pxa910_acm_notify_serial_state(acm);
		break;
	case TIOPPPONCSD:
		acm->cid = (unsigned char)arg;
		acm->modem_state = MODEM_DATA_MODE_OVER_CSD;
		break;
	case TIOPPPOFF:
		acm->cid = MODEM_INVALID_CID;
		acm->modem_state = MODEM_CONTROL_MODE;
		acm->serial_state &= ~(ACM_CTRL_DSR | ACM_CTRL_DCD);
		pxa910_acm_notify_serial_state(acm);
		break;
	default:
		pr_err("%s cmd %u. call from context [%d:%d]: Command not implemented\n",
			   __func__, cmd, current->pid, current->tgid);
		/* could not handle ioctl */
		rc = -ENOIOCTLCMD;

	}

	/*
	 * Check if notification callback is registered, and if so
	 * call it.
	 */
	if (gs_marvell_modem_ioctl_notify_callback != NULL)
		rc = gs_marvell_modem_ioctl_notify_callback(tty, cmd, arg);

	return rc;
}

static const struct tty_operations gs_marvell_modem_tty_ops = {
	.open = pxa910_gs_open,
	.close = pxa910_gs_close,
	.write = gs_marvell_modem_write,
	.put_char = pxa910_gs_put_char,
	.flush_chars = pxa910_gs_flush_chars,
	.write_room = pxa910_gs_write_room,
	.chars_in_buffer = pxa910_gs_chars_in_buffer,
	.ioctl = pxa910_gs_ioctl,
	.unthrottle = pxa910_gs_unthrottle,
	.break_ctl = pxa910_gs_break_ctl,
};

int pxa910_modem_gserial_setup(struct usb_gadget *g, unsigned count)
{

#define GS_MARVELL_MODEM_MAJOR			126
#define GS_MARVELL_MODEM_MINOR_START	32

	unsigned i;
	struct usb_cdc_line_coding coding;
	int status;

	if (count == 0 || count > PXA910_N_PORTS)
		return -EINVAL;

	gs_modem_tty_driver = alloc_tty_driver(count);
	if (!gs_modem_tty_driver)
		return -ENOMEM;


	gs_modem_tty_driver->owner = THIS_MODULE;
	gs_modem_tty_driver->driver_name = "g_serial_modem";
	gs_modem_tty_driver->name = "ttymodem";
	/* uses dynamically assigned dev_t values */

	gs_modem_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	gs_modem_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	gs_modem_tty_driver->flags =
		TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	gs_modem_tty_driver->init_termios = tty_std_termios;

	/* 9600-8-N-1 ... matches defaults expected by "usbser.sys" on
	 * MS-Windows.  Otherwise, most of these flags shouldn't affect
	 * anything unless we were to actually hook up to a serial line.
	 */
	gs_modem_tty_driver->init_termios.c_cflag =
	    B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	gs_modem_tty_driver->init_termios.c_ispeed = 9600;
	gs_modem_tty_driver->init_termios.c_ospeed = 9600;

	gs_modem_tty_driver->major = GS_MARVELL_MODEM_MAJOR;
	gs_modem_tty_driver->minor_start = GS_MARVELL_MODEM_MINOR_START;

	coding.dwDTERate = cpu_to_le32(9600);
	coding.bCharFormat = 8;
	coding.bParityType = USB_CDC_NO_PARITY;
	coding.bDataBits = USB_CDC_1_STOP_BITS;

	tty_set_operations(gs_modem_tty_driver, &gs_marvell_modem_tty_ops);

	/* make devices be openable */
	for (i = 0; i < count; i++) {
		mutex_init(&pxa910_ports[GS_MODEM_PORT_BASE + i].lock);
		status =
		  gs_marvell_modem_port_alloc(GS_MODEM_PORT_BASE + i, &coding);
		if (status) {
			count = i;
			goto fail;
		}
	}
	n_modem_ports = count;

	/* export the driver ... */
	status = tty_register_driver(gs_modem_tty_driver);
	if (status) {
		pr_err("%s: cannot register, err %d\n", __func__, status);
		goto fail;
	}

	/* ... and sysfs class devices, so mdev/udev make /dev/ttyGS* */
	for (i = 0; i < count; i++) {
		struct device *tty_dev;
		struct pxa910_gs_port *port =
			pxa910_ports[GS_MODEM_PORT_BASE + i].port;

		tty_dev = tty_port_register_device(&port->port,
			gs_modem_tty_driver, i, &g->dev);
		if (IS_ERR(tty_dev))
			pr_warn("%s: no classdev for port %d, err %ld\n",
				   __func__, i, PTR_ERR(tty_dev));
	}

	pr_debug("%s: registered %d ttyGS* device%s\n", __func__,
		 count, (count == 1) ? "" : "s");

	return status;
fail:
	while (count--)
		kfree(pxa910_ports[GS_MODEM_PORT_BASE + count].port);
	put_tty_driver(gs_modem_tty_driver);
	gs_modem_tty_driver = NULL;
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
void pxa910_modem_gserial_cleanup(void)
{
	unsigned	i;
	struct pxa910_gs_port *port;

	if (!gs_modem_tty_driver)
		return;

	for (i = 0; i < n_modem_ports; i++) {
		/* prevent new opens */
		mutex_lock(&pxa910_ports[GS_MODEM_PORT_BASE + i].lock);
		port = pxa910_ports[GS_MODEM_PORT_BASE + i].port;
		pxa910_ports[GS_MODEM_PORT_BASE + i].port = NULL;
		mutex_unlock(&pxa910_ports[GS_MODEM_PORT_BASE + i].lock);

		tasklet_kill(&port->push);
		destroy_workqueue(port->tx_wq);
		/* wait for old opens to finish */
		wait_event(port->port.close_wait, pxa910_gs_closed(port));

		WARN_ON(port->port_usb != NULL);

		tty_port_destroy(&port->port);
		kfree(port);

		tty_unregister_device(gs_modem_tty_driver, i);
	}
	n_modem_ports = 0;

	tty_unregister_driver(gs_modem_tty_driver);
	put_tty_driver(gs_modem_tty_driver);
	gs_modem_tty_driver = NULL;

	pr_debug("%s: cleaned up ttyGS* support\n", __func__);
}


