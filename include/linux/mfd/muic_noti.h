/*
 * include/linux/mfd/muic_noti.h
 *
 * SEC MUIC Notification header file
 *
 */

#ifndef LINUX_MFD_MUICNOTI_H
#define LINUX_MFD_MUICNOTI_H

enum {
    MUIC_CABLE_TYPE_NONE = 0,
    MUIC_CABLE_TYPE_UART,            //adc 0x16
    MUIC_CABLE_TYPE_USB,             //adc 0x1f (none id)
    MUIC_CABLE_TYPE_OTG,             //adc 0x00, regDev1&0x01
    /* TA Group */
    MUIC_CABLE_TYPE_REGULAR_TA,      //adc 0x1f (none id, D+ short to D-)
    MUIC_CABLE_TYPE_ATT_TA,          //adc 0x1f (none id, only VBUS)AT&T
    MUIC_CABLE_TYPE_0x15,            //adc 0x15
    MUIC_CABLE_TYPE_TYPE1_CHARGER,   //adc 0x17 (id : 200k)
    MUIC_CABLE_TYPE_0x1A,            //adc 0x1A
    /* JIG Group */
    MUIC_CABLE_TYPE_JIG_USB_OFF,     //adc 0x18
    MUIC_CABLE_TYPE_JIG_USB_ON,      //adc 0x19
    MUIC_CABLE_TYPE_JIG_UART_OFF,    //adc 0x1C
    MUIC_CABLE_TYPE_JIG_UART_ON,     //adc 0x1D
    // JIG type with VBUS
    MUIC_CABLE_TYPE_JIG_UART_OFF_WITH_VBUS,    //adc 0x1C
    MUIC_CABLE_TYPE_JIG_UART_ON_WITH_VBUS,     //adc 0x1D

    MUIC_CABLE_TYPE_CDP, // USB Charging downstream port, usually treated as SDP
    MUIC_CABLE_TYPE_L_USB, // L-USB cable
    MUIC_CABLE_TYPE_UNKNOWN,
    MUIC_CABLE_TYPE_INVALID, // Un-initialized

    MUIC_CABLE_TYPE_OTG_WITH_VBUS,
    MUIC_CABLE_TYPE_UNKNOWN_WITH_VBUS,
};

#ifdef CONFIG_MFD_SM5504
extern int unregister_muic_notifier(struct notifier_block *nb);
extern int register_muic_notifier(struct notifier_block *nb);
#else
#define register_muic_notifier register_adapter_notifier
#define unregister_muic_notifier unregister_adapter_notifier
extern int unregister_adapter_notifier(struct notifier_block *nb);
extern int register_adapter_notifier(struct notifier_block *nb);
#endif

#define MUIC_ATTACH_NOTI		0x0008
#define MUIC_DETACH_NOTI		0x0007	
#define MUIC_CHG_ATTACH_NOTI		0x0006
#define MUIC_CHG_DETACH_NOTI		0x0005
#define MUIC_OTG_DETACH_NOTI		0x0004
#define MUIC_OTG_ATTACH_NOTI		0x0003
#define MUIC_VBUS_NOTI			0x0002
#define MUIC_USB_ATTACH_NOTI		0x0001
#define MUIC_USB_DETACH_NOTI		0x0000

struct muic_notifier_param {
	uint32_t vbus_status;
	int cable_type;
};

#endif // LINUX_MFD_SM5504_H

