
#ifndef USB__GADGET__INFO__H
#define USB__GADGET__INFO__H

#define OS_STRING_QW_SIGN_LEN		14
#define MAX_USB_STRING_LANGS 2
struct gadget_info {
	struct config_group group;
	struct config_group functions_group;
	struct config_group configs_group;
	struct config_group strings_group;
	struct config_group os_desc_group;

	struct mutex lock;
	struct usb_gadget_strings *gstrings[MAX_USB_STRING_LANGS + 1];
	struct list_head string_list;
	struct list_head available_func;

	struct usb_composite_driver composite;
	struct usb_composite_dev cdev;
	bool use_os_desc;
	bool unbinding;
	char b_vendor_code;
	char qw_sign[OS_STRING_QW_SIGN_LEN];
#ifdef CONFIG_USB_CONFIGFS_UEVENT
	bool connected;
	bool sw_connected;
	struct work_struct work;
	struct device *dev;
#endif
};

#endif
