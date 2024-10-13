#ifndef _MACH_USB_H
#define _MACH_USB_H

extern void udc_enable(void);
extern void udc_disable(void);
extern void udc_phy_down(void);
extern void udc_phy_up(void);

enum usb_cable_status {
	USB_CABLE_PLUG_IN,
	USB_CABLE_PLUG_OUT,
};

extern int register_usb_notifier(struct notifier_block *nb);
extern int unregister_usb_notifier(struct notifier_block *nb);

#endif
