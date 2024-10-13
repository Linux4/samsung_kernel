#ifndef _USB_HW_H
#define _USB_HW_H

#define GPIO_INVALID (0xFFFFFFFF)

#if defined(CONFIG_ARCH_SCX30G)
#define BIT_USB_UTMI_SOFT_RSET	BIT_USB_UTMI_SOFT_RST
#define BIT_USB_PHY_SOFT_RSET	BIT_USB_PHY_SOFT_RST
#define BIT_USB_SOFT_RSET		BIT_USB_SOFT_RST
#define BIT_USB_DMPULLUP		BIT_DMPULLUP
#define REG_AP_USB_PHY_CTRL		REG_AP_APB_USB_PHY_CTRL
#if defined(CONFIG_ARCH_SCX20)
#define REG_AP_USB_PHY_TUNE		REG_AP_APB_USB_CTRL0
#define REG_AP_USB_PHY_TUNE1	REG_AP_APB_USB_CTRL1
#else
#define REG_AP_USB_PHY_TUNE		REG_AP_APB_USB_PHY_TUNE
#endif
#else
#define BIT_USB_PHY_SOFT_RSET	BIT_OTG_PHY_SOFT_RST
#define BIT_USB_UTMI_SOFT_RSET	BIT_OTG_UTMI_SOFT_RST
#define BIT_USB_SOFT_RSET		BIT_OTG_SOFT_RST
#define BIT_USB_DMPULLUP		BIT_OTG_DMPULLUP
#define REG_AP_USB_PHY_CTRL		REG_AP_AHB_OTG_PHY_CTRL
#define REG_AP_USB_PHY_TUNE		REG_AP_AHB_OTG_PHY_TUNE
#endif

#define GUSBCFG_OFFSET  (0x0C)
#define BIT_PHY_INTERFACE  BIT(3)

/*special reg for sprd phy*/
#define AP_TOP_USB_PHY_RST SPRD_PIN_BASE
#define BIT_PHY_UTMI_PORN	BIT(0)

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
