/*
 * f_gnss.c - generic USB serial function driver
 * modified from f_serial.c and f_diag.c
 * ttyGS*
 *
 * Copyright (C) 2003 Al Borchers (alborchers@steinerpoint.com)
 * Copyright (C) 2008 by David Brownell
 * Copyright (C) 2008 by Nokia Corporation
 *
 * This software is distributed under the terms of the GNU General
 * Public License ("GPL") as published by the Free Software Foundation,
 * either version 2 of that License or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>

#include <linux/configfs.h>
#include <linux/usb/composite.h>

#include "../configfs.h"
#include "u_serial.h"

#define MAX_INST_NAME_LEN		40
/*  GNSS_PORT NUM : /dev/ttyGS* port number */
#define GNSS_PORT_NUM			4
/*
 * This function packages a simple "generic serial" port with no real
 * control mechanisms, just raw data transfer over two bulk endpoints.
 *
 * Because it's not standardized, this isn't as interoperable as the
 * CDC ACM driver.  However, for many purposes it's just as functional
 * if you can arrange appropriate host side drivers.
 */

struct gnss_descs {
	struct usb_endpoint_descriptor	*in;
	struct usb_endpoint_descriptor	*out;
};

struct f_gnss {
	struct gserial			port;
	u8				data_id;
	u8				port_num;

	struct gnss_descs			fs;
	struct gnss_descs			hs;
};

static inline struct f_gnss *func_to_gnss(struct usb_function *f)
{
	return container_of(f, struct f_gnss, port.func);
}

/*-------------------------------------------------------------------------*/

/* interface descriptor: */
static struct usb_interface_descriptor gnss_interface_desc  = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass =	0x67,
	.bInterfaceProtocol =	0x55,
	/* .iInterface = DYNAMIC */
};

/* full speed support: */

static struct usb_endpoint_descriptor gnss_fs_in_desc  = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor gnss_fs_out_desc  = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *gnss_fs_function[]  = {
	(struct usb_descriptor_header *) &gnss_interface_desc,
	(struct usb_descriptor_header *) &gnss_fs_in_desc,
	(struct usb_descriptor_header *) &gnss_fs_out_desc,
	NULL,
};

/* high speed support: */

static struct usb_endpoint_descriptor gnss_hs_in_desc  = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor gnss_hs_out_desc  = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16(512),
};
static struct usb_descriptor_header *gnss_hs_function[]  = {
	(struct usb_descriptor_header *) &gnss_interface_desc,
	(struct usb_descriptor_header *) &gnss_hs_in_desc,
	(struct usb_descriptor_header *) &gnss_hs_out_desc,
	NULL,
};

static struct usb_endpoint_descriptor gnss_ss_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor gnss_ss_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor gnss_ss_bulk_comp_desc = {
	.bLength =              sizeof gnss_ss_bulk_comp_desc,
	.bDescriptorType =      USB_DT_SS_ENDPOINT_COMP,
};

static struct usb_descriptor_header *gnss_ss_function[] = {
	(struct usb_descriptor_header *) &gnss_interface_desc,
	(struct usb_descriptor_header *) &gnss_ss_in_desc,
	(struct usb_descriptor_header *) &gnss_ss_bulk_comp_desc,
	(struct usb_descriptor_header *) &gnss_ss_out_desc,
	(struct usb_descriptor_header *) &gnss_ss_bulk_comp_desc,
	NULL,
};


/* string descriptors: */
#define F_GNSS_IDX	0

static struct usb_string gnss_string_defs[] = {
	[F_GNSS_IDX].s = "Samsung Android GNSS",
	{  /* ZEROES END LIST */ },
};

static struct usb_gadget_strings gnss_string_table = {
	.language =		0x0409,	/* en-us */
	.strings =		gnss_string_defs,
};

static struct usb_gadget_strings *gnss_strings[] = {
	&gnss_string_table,
	NULL,
};

struct gnss_instance {
	struct usb_function_instance func_inst;
	const char *name;
	struct f_gnss *gnss;
	u8 port_num;
};
/*-------------------------------------------------------------------------*/

static int gnss_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_gnss		*gnss = func_to_gnss(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	int status;

	/* we know alt == 0, so this is an activation or a reset */

	if (gnss->port.in->driver_data) {
		DBG(cdev, "reset generic ttyGS%d\n", gnss->port_num);
		gserial_disconnect(&gnss->port);
	} else {
		DBG(cdev, "activate generic ttyGS%d\n", gnss->port_num);
	}
	if (!gnss->port.in->desc || !gnss->port.out->desc) {
			DBG(cdev, "activate gnss ttyGS%d\n", gnss->port_num);
			if (config_ep_by_speed(cdev->gadget, f,
					       gnss->port.in) ||
			    config_ep_by_speed(cdev->gadget, f,
					       gnss->port.out)) {
				gnss->port.in->desc = NULL;
				gnss->port.out->desc = NULL;
				return -EINVAL;
			}
	}


	status = gserial_connect(&gnss->port, gnss->port_num);
	printk(KERN_DEBUG "usb: %s run generic_connect(%d)", __func__,
			gnss->port_num);

	if (status < 0) {
		printk(KERN_DEBUG "fail to activate generic ttyGS%d\n",
				gnss->port_num);

		return status;
	}

	return 0;
}

static void gnss_disable(struct usb_function *f)
{
	struct f_gnss	*gnss = func_to_gnss(f);

	printk(KERN_DEBUG "usb: %s generic ttyGS%d deactivated\n", __func__,
			gnss->port_num);
	gserial_disconnect(&gnss->port);
}

/*-------------------------------------------------------------------------*/

/* serial function driver setup/binding */

static int
gnss_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_gnss		*gnss = func_to_gnss(f);
	int			status;
	struct usb_ep		*ep;

	/* maybe allocate device-global string ID */
	if (gnss_string_defs[F_GNSS_IDX].id == 0) {
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		gnss_string_defs[F_GNSS_IDX].id = status;
	}
	/* allocate instance-specific interface IDs */
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	gnss->data_id = status;
	gnss_interface_desc.bInterfaceNumber = status;

	status = -ENODEV;

	/* allocate instance-specific endpoints */
	ep = usb_ep_autoconfig(cdev->gadget, &gnss_fs_in_desc);
	if (!ep)
		goto fail;
	gnss->port.in = ep;
	ep->driver_data = cdev;	/* claim */

	ep = usb_ep_autoconfig(cdev->gadget, &gnss_fs_out_desc);
	if (!ep)
		goto fail;
	gnss->port.out = ep;
	ep->driver_data = cdev;	/* claim */
	printk(KERN_INFO "[%s]   in =0x%p , out =0x%p\n", __func__,
				gnss->port.in, gnss->port.out);

	/* copy descriptors, and track endpoint copies */
	f->fs_descriptors = usb_copy_descriptors(gnss_fs_function);




	/* support all relevant hardware speeds... we expect that when
	 * hardware is dual speed, all bulk-capable endpoints work at
	 * both speeds
	 */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		gnss_hs_in_desc.bEndpointAddress =
				gnss_fs_in_desc.bEndpointAddress;
		gnss_hs_out_desc.bEndpointAddress =
				gnss_fs_out_desc.bEndpointAddress;

		/* copy descriptors, and track endpoint copies */
		f->hs_descriptors = usb_copy_descriptors(gnss_hs_function);
	}
	if (gadget_is_superspeed(c->cdev->gadget)) {
		gnss_ss_in_desc.bEndpointAddress =
			gnss_fs_in_desc.bEndpointAddress;
		gnss_ss_out_desc.bEndpointAddress =
			gnss_fs_out_desc.bEndpointAddress;

		/* copy descriptors, and track endpoint copies */
		f->ss_descriptors = usb_copy_descriptors(gnss_ss_function);
		if (!f->ss_descriptors)
			goto fail;
	}

	printk("usb: [%s] generic ttyGS%d: %s speed IN/%s OUT/%s\n",
			__func__,
			gnss->port_num,
			gadget_is_superspeed(c->cdev->gadget) ? "super" :
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			gnss->port.in->name, gnss->port.out->name);
	return 0;

fail:
	/* we might as well release our claims on endpoints */
	if (gnss->port.out)
		gnss->port.out->driver_data = NULL;
	if (gnss->port.in)
		gnss->port.in->driver_data = NULL;

	printk(KERN_ERR "%s: can't bind, err %d\n", f->name, status);

	return status;
}

static void
gnss_unbind(struct usb_configuration *c, struct usb_function *f)
{
	if (gadget_is_dualspeed(c->cdev->gadget))
		usb_free_descriptors(f->hs_descriptors);
	usb_free_descriptors(f->fs_descriptors);
	printk(KERN_DEBUG "usb: %s\n", __func__);
}

/*
 * gnss_bind_config - add a generic serial function to a configuration
 * @c: the configuration to support the serial instance
 * @port_num: /dev/ttyGS* port this interface will use
 * Context: single threaded during gadget setup
 *
 * Returns zero on success, else negative errno.
 *
 * Caller must have called @gserial_setup() with enough ports to
 * handle all the ones it binds.  Caller is also responsible
 * for calling @gserial_cleanup() before module unload.
 */
int gnss_bind_config(struct usb_configuration *c, u8 port_num)
{
	struct f_gnss	*gnss;
	int		status;

	/* REVISIT might want instance-specific strings to help
	 * distinguish instances ...
	 */

	/* maybe allocate device-global string ID */
	if (gnss_string_defs[F_GNSS_IDX].id == 0) {
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		gnss_string_defs[F_GNSS_IDX].id = status;
	}

	/* allocate and initialize one new instance */
	gnss = kzalloc(sizeof *gnss, GFP_KERNEL);
	if (!gnss)
		return -ENOMEM;

	gnss->port_num = GNSS_PORT_NUM;

	gnss->port.func.name = "gnss";
	gnss->port.func.strings = gnss_strings;
	gnss->port.func.bind = gnss_bind;
	gnss->port.func.unbind = gnss_unbind;
	gnss->port.func.set_alt = gnss_set_alt;
	gnss->port.func.disable = gnss_disable;

	status = usb_add_function(c, &gnss->port.func);
	if (status)
		kfree(gnss);
	return status;
}
static struct gnss_instance *to_gnss_instance(struct config_item *item)
{
	return container_of(to_config_group(item), struct gnss_instance,
		func_inst.group);
}
static void gnss_attr_release(struct config_item *item)
{
	struct gnss_instance *fi_gnss = to_gnss_instance(item);
	usb_put_function_instance(&fi_gnss->func_inst);
}

static struct configfs_item_operations gnss_item_ops = {
	.release        = gnss_attr_release,
};

static struct config_item_type gnss_func_type = {
	.ct_item_ops    = &gnss_item_ops,
	.ct_owner       = THIS_MODULE,
};


static struct gnss_instance *to_fi_gnss(struct usb_function_instance *fi)
{
	return container_of(fi, struct gnss_instance, func_inst);
}

static int gnss_set_inst_name(struct usb_function_instance *fi, const char *name)
{
	struct gnss_instance *fi_gnss;
	char *ptr;
	int name_len;

	name_len = strlen(name) + 1;
	if (name_len > MAX_INST_NAME_LEN)
		return -ENAMETOOLONG;

	ptr = kstrndup(name, name_len, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	fi_gnss = to_fi_gnss(fi);
	fi_gnss->name = ptr;

	return 0;
}

static void gnss_free_inst(struct usb_function_instance *fi)
{
	struct gnss_instance *fi_gnss;

	fi_gnss = to_fi_gnss(fi);
	kfree(fi_gnss->name);
	kfree(fi_gnss);
}

struct usb_function_instance *alloc_inst_gnss(bool gnss_config)
{
	struct gnss_instance *fi_gnss;
	int ret;

	fi_gnss = kzalloc(sizeof(*fi_gnss), GFP_KERNEL);
	if (!fi_gnss)
		return ERR_PTR(-ENOMEM);
	fi_gnss->func_inst.set_inst_name = gnss_set_inst_name;
	fi_gnss->func_inst.free_func_inst = gnss_free_inst;

	ret = gserial_alloc_line(&fi_gnss->port_num);
	if (ret) {
		kfree(fi_gnss);
		return ERR_PTR(ret);
	}

	config_group_init_type_name(&fi_gnss->func_inst.group,
					"", &gnss_func_type);

	return  &fi_gnss->func_inst;
}
EXPORT_SYMBOL_GPL(alloc_inst_gnss);

static struct usb_function_instance *gnss_alloc_inst(void)
{
		return alloc_inst_gnss(true);
}

static void gnss_free(struct usb_function *f)
{
	struct f_gnss	*gnss = func_to_gnss(f);

	kfree(gnss);
}

struct usb_function *function_alloc_gnss(struct usb_function_instance *fi, bool gnss_config)
{

	struct gnss_instance *fi_gnss = to_fi_gnss(fi);
	struct f_gnss	*gnss;
	//int		ret;

	/* REVISIT might want instance-specific strings to help
	 * distinguish instances ...
	 */

	/* allocate and initialize one new instance */
	gnss = kzalloc(sizeof *gnss, GFP_KERNEL);
	if (!gnss)
		return ERR_PTR(-ENOMEM);


	gnss->port_num = fi_gnss->port_num;

	gnss->port.func.name = "gnss";
	gnss->port.func.strings = gnss_strings;
	gnss->port.func.bind = gnss_bind;
	gnss->port.func.unbind = gnss_unbind;
	gnss->port.func.set_alt = gnss_set_alt;
	gnss->port.func.disable = gnss_disable;
	gnss->port.func.free_func = gnss_free;

	fi_gnss->gnss = gnss;

	return &gnss->port.func;
}
EXPORT_SYMBOL_GPL(function_alloc_gnss);

static struct usb_function *gnss_alloc(struct usb_function_instance *fi)
{
	return function_alloc_gnss(fi, true);
}

DECLARE_USB_FUNCTION_INIT(gnss, gnss_alloc_inst, gnss_alloc);
MODULE_LICENSE("GPL");