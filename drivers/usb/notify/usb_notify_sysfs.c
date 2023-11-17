/*
 *  drivers/usb/notify/usb_notify_sysfs.c
 *
 * Copyright (C) 2015-2017 Samsung, Inc.
 *
 */

 /* usb notify layer v3.1 */

#define pr_fmt(fmt) "usb_notify: " fmt

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/usb.h>
#include <linux/string.h>
#include <linux/usb_notify.h>
#include <linux/configfs.h>
#include <linux/usb/composite.h>
#include <linux/usb/gadget.h>
#include "usb_gadget_info.h"

#define MAJOR_DEV 0
#define MINOR_DEV 1


RAW_NOTIFIER_HEAD(usb_otg_notifier);
EXPORT_SYMBOL_GPL(usb_otg_notifier);

static int check_cmd_type(char *cur_cmd, char *pre_cmd)
{
	int notify_t = 0;

	pr_err("%s : current cmd=%s, previous cmd=%s\n",__func__, cur_cmd, pre_cmd);

	if (!strcmp(cur_cmd, "ON_ALL_SIM")) {
		notify_t = NOTIFY_BLOCK_ALL;
	} else if (!strcmp(cur_cmd, "ON_HOST_UPSM")) {
		notify_t = NOTIFY_BLOCK_HOST;
	} else if (!strcmp(cur_cmd, "ON_HOST_MDM")) {
		notify_t = NOTIFY_BLOCK_HOST;
	} else if (!strcmp(cur_cmd, "OFF")) {
		notify_t = NOTIFY_OFF;
	} else {
		notify_t = NOTIFY_INVALID;
	}

	if(!strcmp(cur_cmd, pre_cmd))
		notify_t = NOTIFY_INVALID;

	return notify_t;
}

ssize_t usb_notify_gadget_disable(struct gadget_info *gi)
{
	int ret = 0;

	mutex_lock(&gi->lock);

	if (!gi->composite.gadget_driver.udc_name) {
		ret = -ENODEV;
		goto err;
	}

	gi->unbinding = true;
	ret = usb_gadget_unregister_driver(&gi->composite.gadget_driver);
	if (ret)
		goto err;

	gi->unbinding = false;
	kfree(gi->composite.gadget_driver.udc_name);
	gi->composite.gadget_driver.udc_name = NULL;

	mutex_unlock(&gi->lock);
	return 0;

err:
	mutex_unlock(&gi->lock);
	return ret;
}

ssize_t usb_notify_gadget_disable_usb_data(struct gadget_info *gi)
{
	int ret = 0;

	mutex_lock(&gi->lock);

	if (!gi->composite.gadget_driver.udc_name) {
		ret = -ENODEV;
		goto err;
	}

	ret = usb_gadget_unregister_driver(&gi->composite.gadget_driver);
	if (ret)
		goto err;

	kfree(gi->composite.gadget_driver.udc_name);
	gi->composite.gadget_driver.udc_name = NULL;

	mutex_unlock(&gi->lock);
	return 0;

err:
	mutex_unlock(&gi->lock);
	return ret;
}

ssize_t usb_notify_gadget_enable(struct gadget_info *gi, char *name)
{
	int ret = 0;

	mutex_lock(&gi->lock);

	if (gi->composite.gadget_driver.udc_name) {
			ret = -EBUSY;
			goto err;
	}

	gi->composite.gadget_driver.udc_name = name;
	ret = usb_gadget_probe_driver(&gi->composite.gadget_driver);
	if (ret) {
		gi->composite.gadget_driver.udc_name = NULL;
		goto err;
	}

	mutex_unlock(&gi->lock);
	return 0;

err:
	kfree(name);
	mutex_unlock(&gi->lock);
	return ret;
}

void usb_notify_event(struct usb_notify* u_notify,bool block)
{
	char *usb_device_disable[2]    = { "USB_DEVICE_FUNC_STATE=disable",NULL };
	char *usb_device_enable[2]    = { "USB_DEVICE_FUNC_STATE=enable",NULL };

	if(block)
		kobject_uevent_env(&u_notify->notify_dev.dev->kobj,KOBJ_CHANGE, usb_device_disable);
	else
		kobject_uevent_env(&u_notify->notify_dev.dev->kobj,KOBJ_CHANGE, usb_device_enable);

	return;
}

static ssize_t disable_show(
	struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct usb_notify_dev *udev = (struct usb_notify_dev *)
		dev_get_drvdata(dev);

	pr_info("read cmd_state %s\n", udev->state_cmd);
	return sprintf(buf, "%s\n", udev->state_cmd);
}

static ssize_t disable_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct usb_notify_dev *udev = (struct usb_notify_dev *)dev_get_drvdata(dev);
	struct usb_notify* u_notify = container_of(udev, struct usb_notify, notify_dev);

	char *disable;
	int ret = 0;
	int notify_type = 0;

	if (size > CMD_STATE_LEN) {
		pr_err("%s size(%zu) is too long.\n", __func__, size);
		goto error;
	}

	disable = kzalloc(size+1, GFP_KERNEL);
	if (!disable)
		goto error;

	ret = sscanf(buf, "%s", disable);
	if (ret != 1)
		goto error1;

	notify_type = check_cmd_type(disable, udev->state_cmd);
	if(!notify_type)
		goto error1;

	if( (notify_type==NOTIFY_BLOCK_ALL) || (notify_type==NOTIFY_OFF) ) {
		u_notify->udc_name = kzalloc(UDC_CMD_LEN, GFP_KERNEL);
		if (!u_notify->udc_name)
			goto error1;
	}

	strncpy(udev->state_cmd,disable, sizeof(udev->state_cmd)-1);

	switch (notify_type) {

		case NOTIFY_BLOCK_ALL:
			u_notify->usb_host_flag = 1;
			u_notify->usb_device_flag = 1;

			//usb_notify_event(u_notify,true);
			ret = usb_notify_gadget_disable(u_notify->usb_gi);
			if(ret)
				pr_err("usb_notify_gadget_disable fail or already disable!\n");
			kfree(u_notify->udc_name);
			break;

		case NOTIFY_BLOCK_HOST:
			u_notify->usb_host_flag = 1;
			u_notify->usb_device_flag = 0;
			//usb_notify_event(u_notify,false);
			break;

		case NOTIFY_OFF:
			u_notify->usb_host_flag = 0;
			u_notify->usb_device_flag = 0;

			//usb_notify_event(u_notify,false);
			strcpy(u_notify->udc_name,u_notify->gi_name);
			ret = usb_notify_gadget_enable(u_notify->usb_gi,u_notify->udc_name);
			if(ret)
				pr_err("usb_notify_gadget_enable fail or already config!\n");
			break;

		default:
			goto error1;
	}

	raw_notifier_call_chain(&usb_otg_notifier,0,u_notify);

	kfree(disable);
	return size;

error1:
	kfree(disable);
error:
	return size;
}

static ssize_t usb_data_enabled_show(
    struct device *dev, struct device_attribute *attr, char *buf)
{
    struct usb_notify_dev *udev = (struct usb_notify_dev *)
        dev_get_drvdata(dev);

    pr_info("Read usb_data_enabled %lu\n", udev->usb_data_enabled);

    return sprintf(buf, "%lu\n", udev->usb_data_enabled);
}

static ssize_t usb_data_enabled_store(
        struct device *dev, struct device_attribute *attr,
        const char *buf, size_t size)
{
    struct usb_notify_dev *udev = (struct usb_notify_dev *)
        dev_get_drvdata(dev);

    struct usb_notify* u_notify = container_of(udev, struct usb_notify, notify_dev);
    size_t ret = -ENOMEM;
    int sret = -EINVAL;
    char *usb_data_enabled;

    if (size > PAGE_SIZE) {
        pr_err("%s: size(%zu) is too long.\n", __func__, size);

        goto error;
    }

    usb_data_enabled = kzalloc(size+1, GFP_KERNEL);

    if (!usb_data_enabled)
      goto error;

    sret = sscanf(buf, "%s", usb_data_enabled);

    if (sret != 1)
        goto error1;
    u_notify->udc_name = kzalloc(UDC_CMD_LEN, GFP_KERNEL);
    if (!u_notify->udc_name)
        goto error1;

    if (strcmp(usb_data_enabled, "0") == 0) {
        udev->usb_data_enabled = 0;
        u_notify->usb_host_flag = 1;
        u_notify->usb_device_flag = 1;

        ret = usb_notify_gadget_disable_usb_data(u_notify->usb_gi);
 
        if(ret)
            pr_err("usb_notify_gadget_disable fail or already disable!\n");

        kfree(u_notify->udc_name);
    } else if (strcmp(usb_data_enabled, "1") == 0) {
        udev->usb_data_enabled = 1;
        u_notify->usb_host_flag = 0;
        u_notify->usb_device_flag = 0;
        strcpy(u_notify->udc_name,u_notify->gi_name);

        ret = usb_notify_gadget_enable(u_notify->usb_gi,u_notify->udc_name);

        if(ret)
            pr_err("usb_notify_gadget_enable fail or already config!\n");
    } else {
        pr_err("%s: usb_data_enabled(%s) error.\n",
            __func__, usb_data_enabled);

        goto error1;
    }
    pr_info("%s: usb_data_enabled=%s\n",
        __func__, usb_data_enabled);

    raw_notifier_call_chain(&usb_otg_notifier,0,u_notify);
    ret = size;

error1:
    kfree(usb_data_enabled);

error:
    return ret;
}

int get_class_index(int ch9_class_num)
{
	int internal_class_index;

	switch (ch9_class_num) {
	case USB_CLASS_PER_INTERFACE:
		internal_class_index = 1;
		break;
	case USB_CLASS_AUDIO:
		internal_class_index = 2;
		break;
	case USB_CLASS_COMM:
		internal_class_index = 3;
		break;
	case USB_CLASS_HID:
		internal_class_index = 4;
		break;
	case USB_CLASS_PHYSICAL:
		internal_class_index = 5;
		break;
	case USB_CLASS_STILL_IMAGE:
		internal_class_index = 6;
		break;
	case USB_CLASS_PRINTER:
		internal_class_index = 7;
		break;
	case USB_CLASS_MASS_STORAGE:
		internal_class_index = 8;
		break;
	case USB_CLASS_HUB:
		internal_class_index = 9;
		break;
	case USB_CLASS_CDC_DATA:
		internal_class_index = 10;
		break;
	case USB_CLASS_CSCID:
		internal_class_index = 11;
		break;
	case USB_CLASS_CONTENT_SEC:
		internal_class_index = 12;
		break;
	case USB_CLASS_VIDEO:
		internal_class_index = 13;
		break;
	case USB_CLASS_WIRELESS_CONTROLLER:
		internal_class_index = 14;
		break;
	case USB_CLASS_MISC:
		internal_class_index = 15;
		break;
	case USB_CLASS_APP_SPEC:
		internal_class_index = 16;
		break;
	case USB_CLASS_VENDOR_SPEC:
		internal_class_index = 17;
		break;
	default:
		internal_class_index = 0;
		break;
	}
	return internal_class_index;
}

static bool usb_match_any_interface_for_mdm(struct usb_device *udev,
				    int *whitelist_array)
{
	unsigned int i;

	for (i = 0; i < udev->descriptor.bNumConfigurations; ++i) {
		struct usb_host_config *cfg = &udev->config[i];
		unsigned int j;

		for (j = 0; j < cfg->desc.bNumInterfaces; ++j) {
			struct usb_interface_cache *cache;
			struct usb_host_interface *intf;
			int intf_class;

			cache = cfg->intf_cache[j];
			if (cache->num_altsetting == 0)
				continue;

			intf = &cache->altsetting[0];
			intf_class = intf->desc.bInterfaceClass;
			if (!whitelist_array[get_class_index(intf_class)]) {
				pr_info("%s : FAIL,%x interface, it's not in whitelist\n",
					__func__, intf_class);
				return false;
			}
			pr_info("%s : SUCCESS,%x interface, it's in whitelist\n",
				__func__, intf_class);
		}
	}
	return true;
}

int usb_check_whitelist_for_mdm(struct usb_device *dev)
{
	int *whitelist_array;

	if (u_notify == NULL) {
		pr_err("u_notify is NULL\n");
		return 1;
	}

	if (u_notify->sec_whitelist_enable) {
		whitelist_array = u_notify->notify_dev.whitelist_array_for_mdm;
		if (usb_match_any_interface_for_mdm(dev, whitelist_array)) {
			dev_info(&dev->dev, "the device is matched with whitelist!\n");
			return 1;
		}
		return 0;
	}
	return 1;
}
EXPORT_SYMBOL_GPL(usb_check_whitelist_for_mdm);

char interface_class_name[USB_CLASS_VENDOR_SPEC][4] = {
	{"PER"},
	{"AUD"},
	{"COM"},
	{"HID"},
	{"PHY"},
	{"STI"},
	{"PRI"},
	{"MAS"},
	{"HUB"},
	{"CDC"},
	{"CSC"},
	{"CON"},
	{"VID"},
	{"WIR"},
	{"MIS"},
	{"APP"},
	{"VEN"}
};

void init_usb_whitelist_array(int *whitelist_array)
{
	int i;

	for (i = 1; i <= MAX_CLASS_TYPE_NUM; i++)
		whitelist_array[i] = 0;
}

int set_usb_whitelist_array(const char *buf, int *whitelist_array)
{
	int valid_class_count = 0;
	char *ptr = NULL;
	int i;
	char *source;

	source = (char *)buf;
	while ((ptr = strsep(&source, ":")) != NULL) {
		pr_info("%s token = %c%c%c!\n", __func__,
			ptr[0], ptr[1], ptr[2]);
		for (i = 1; i <= USB_CLASS_VENDOR_SPEC; i++) {
			if (!strncmp(ptr, interface_class_name[i-1], 3))
				whitelist_array[i] = 1;
		}
	}

	for (i = 1; i <= U_CLASS_VENDOR_SPEC; i++) {
		if (whitelist_array[i])
			valid_class_count++;
	}
	pr_info("%s valid_class_count = %d!\n", __func__, valid_class_count);
	return valid_class_count;
}

static ssize_t whitelist_for_mdm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct usb_notify_dev *udev = (struct usb_notify_dev *)
		dev_get_drvdata(dev);

	if (udev == NULL) {
		pr_err("udev is NULL\n");
		return -EINVAL;
	}
	pr_info("%s read whitelist_classes %s\n",
		__func__, udev->whitelist_str);
	return sprintf(buf, "%s\n", udev->whitelist_str);
}

static ssize_t whitelist_for_mdm_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct usb_notify_dev *udev = (struct usb_notify_dev *)
		dev_get_drvdata(dev);
	struct usb_notify* u_notify = container_of(udev, struct usb_notify, notify_dev);
	char *disable;
	int sret;
	size_t ret = -ENOMEM;
	int mdm_disable;
	int valid_whilelist_count;

	if (udev == NULL) {
		pr_err("udev is NULL\n");
		ret = -EINVAL;
		goto error;
	}

	if (size > MAX_WHITELIST_STR_LEN) {
		pr_err("%s size(%zu) is too long.\n", __func__, size);
		goto error;
	}

	if (size < strlen(buf))
		goto error;

	disable = kzalloc(size+1, GFP_KERNEL);
	if (!disable)
		goto error;

	sret = sscanf(buf, "%s", disable);
	if (sret != 1)
		goto error1;
	pr_info("%s buf=%s\n", __func__, disable);

	init_usb_whitelist_array(udev->whitelist_array_for_mdm);
	/* To active displayport, hub class must be enabled */
	if (!strncmp(buf, "ABL", 3)) {
		udev->whitelist_array_for_mdm[USB_CLASS_HUB] = 1;
		mdm_disable = NOTIFY_MDM_TYPE_ON;
		u_notify->sec_whitelist_enable = 1;
	} else if (!strncmp(buf, "OFF", 3)) {
		mdm_disable = NOTIFY_MDM_TYPE_OFF;
		u_notify->sec_whitelist_enable = 0;
	} else {
		valid_whilelist_count =	set_usb_whitelist_array
			(buf, udev->whitelist_array_for_mdm);
		if (valid_whilelist_count > 0) {
			udev->whitelist_array_for_mdm[USB_CLASS_HUB] = 1;
			mdm_disable = NOTIFY_MDM_TYPE_ON;
			u_notify->sec_whitelist_enable = 1;
		} else {
			mdm_disable = NOTIFY_MDM_TYPE_OFF;
			u_notify->sec_whitelist_enable = 0;
		}
	}

	strncpy(udev->whitelist_str,
		disable, sizeof(udev->whitelist_str)-1);

	ret = size;
error1:
	kfree(disable);
error:
	return ret;
}

static DEVICE_ATTR(usb_data_enabled, 0664, usb_data_enabled_show, usb_data_enabled_store);
static DEVICE_ATTR(disable, 0664, disable_show, disable_store);
static DEVICE_ATTR(whitelist_for_mdm, 0664,
	whitelist_for_mdm_show, whitelist_for_mdm_store);

static struct attribute *usb_notify_attrs[] = {
	&dev_attr_usb_data_enabled.attr,
	&dev_attr_disable.attr,
	&dev_attr_whitelist_for_mdm.attr,
	NULL,
};


static struct attribute_group usb_notify_attr_group = {
	.attrs = usb_notify_attrs,
};

int usb_notify_dev_register(struct usb_notify_dev *udev)
{
	int ret;

	udev->dev = device_create(u_notify->usb_notify_class, NULL,
		MKDEV(MAJOR_DEV, MINOR_DEV), NULL, udev->name);
	if (IS_ERR(udev->dev))
		return PTR_ERR(udev->dev);

	ret = sysfs_create_group(&udev->dev->kobj, &usb_notify_attr_group);
	if (ret < 0) {
		device_destroy(u_notify->usb_notify_class,
				MKDEV(MAJOR_DEV, MINOR_DEV));
		return ret;
	}

	dev_set_drvdata(udev->dev, udev);
	return 0;
}
EXPORT_SYMBOL_GPL(usb_notify_dev_register);

void usb_notify_dev_unregister(struct usb_notify_dev *udev)
{
	sysfs_remove_group(&udev->dev->kobj, &usb_notify_attr_group);
	device_destroy(u_notify->usb_notify_class, MKDEV(MAJOR_DEV, MINOR_DEV));
	dev_set_drvdata(udev->dev, NULL);
}
EXPORT_SYMBOL_GPL(usb_notify_dev_unregister);

int usb_device_dev_register(struct usb_device_dev *udev)
{
	int ret;
	dev_t devid;

    ret = alloc_chrdev_region(&devid,2,1,"usb_device");
	if(ret < 0)
		pr_err("alloc_chrdev_region fail!\n");

	udev->dev = device_create(u_notify->usb_device_class, NULL,
		devid, NULL, udev->name);
	if (IS_ERR(udev->dev))
		return PTR_ERR(udev->dev);

	dev_set_drvdata(udev->dev, udev);
	return 0;
}
EXPORT_SYMBOL_GPL(usb_device_dev_register);

void usb_device_dev_unregister(struct usb_device_dev *udev)
{
	device_destroy(u_notify->usb_device_class, MKDEV(MAJOR_DEV, MINOR_DEV));
	dev_set_drvdata(udev->dev, NULL);
}
EXPORT_SYMBOL_GPL(usb_device_dev_unregister);

static int create_usb_notify_class(struct usb_notify* u_notify)
{
	if (!u_notify->usb_notify_class) {
		u_notify->usb_notify_class = class_create(THIS_MODULE, "usb_notify");
		if (IS_ERR(u_notify->usb_notify_class))
			return PTR_ERR(u_notify->usb_notify_class);
	}

	if (!u_notify->usb_device_class) {
		u_notify->usb_device_class = class_create(THIS_MODULE, "usb_device");
		if (IS_ERR(u_notify->usb_device_class))
			return PTR_ERR(u_notify->usb_device_class);
	}

	return 0;
}

int usb_notify_class_init(struct usb_notify* u_notify)
{
	return create_usb_notify_class(u_notify);
}
EXPORT_SYMBOL_GPL(usb_notify_class_init);

void usb_notify_class_exit(struct usb_notify* u_notify)
{
	class_destroy(u_notify->usb_notify_class);
}
EXPORT_SYMBOL_GPL(usb_notify_class_exit);


int usb_otg_notifier_register(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&usb_otg_notifier, nb);
}
EXPORT_SYMBOL_GPL(usb_otg_notifier_register);


void usb_otg_notifier_unregister(struct notifier_block *nb)
{
	raw_notifier_chain_unregister(&usb_otg_notifier, nb);
}
EXPORT_SYMBOL_GPL(usb_otg_notifier_unregister);


struct usb_notify * usb_get_notify(struct gadget_info *gi, char *name ,int len)
{

	if(u_notify != NULL) {
		if(gi)
			u_notify->usb_gi = gi;

		if( (len > 0) && (len <= UDC_CMD_LEN) ) {
			strncpy(u_notify->gi_name, name, len);
			u_notify->gi_name[len] = '\0';
		}
		else
			pr_err("usb notify gadget len err\n");
	}

	return u_notify;
}
EXPORT_SYMBOL_GPL(usb_get_notify);

