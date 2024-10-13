#ifndef _USB_HW_H
#define _USB_HW_H

extern int usb_alloc_vbus_irq(int gpio);
extern void usb_free_vbus_irq(int irq,int gpio);
extern int usb_get_vbus_irq(void);
extern int usb_get_vbus_state(void);
extern void charge_pump_set(int gpio,int state);
extern void udc_enable(void);
extern void udc_disable(void);
extern int usb_alloc_id_irq(int gpio);
extern void usb_free_id_irq(int irq,int gpio);
extern int usb_get_id_irq(void);
extern int usb_get_id_state(void);
extern void usb_phy_tune_host(void);
extern void usb_phy_tune_dev(void);
enum vbus_irq_type {
	        VBUS_PLUG_IN,
		        VBUS_PLUG_OUT
};

enum id_irq_type {
	        OTG_CABLE_PLUG_IN,
		        OTG_CABLE_PLUG_OUT
};
extern void usb_set_vbus_irq_type(int irq, int irq_type);
extern void usb_set_id_irq_type(int irq, int irq_type);
#endif
