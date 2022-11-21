#ifndef _MACH_USB_H
#define _MACH_USB_H

extern void udc_enable(void);
extern void udc_disable(void);
extern void udc_phy_down(void);
extern void udc_phy_up(void);

struct usb_hotplug_callback {
	int (*plugin)(int usb_cable, void *data);
	int (*plugout)(int usb_cable, void *data);
	void *data;
};

extern int usb_register_hotplug_callback(struct usb_hotplug_callback *cb);

#endif
