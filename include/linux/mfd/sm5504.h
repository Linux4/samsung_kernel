/*
 * include/linux/mfd/SM5504.h
 *
 * Richtek SM5504 driver header file
 *
 * Copyright (C) 2013 Richtek Technology Corp.
 * Author: Patrick Chang <patrick_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef LINUX_MFD_SM5504_H
#define LINUX_MFD_SM5504_H

/* Marvell MMP arch*/
#ifdef CONFIG_ARCH_MMP
#define SAMSUNG_MVRL_MUIC_SM5504 1
#endif

enum {
    MUIC_SM5504_CABLE_TYPE_NONE = 0,
    MUIC_SM5504_CABLE_TYPE_UART,            //adc 0x16
    MUIC_SM5504_CABLE_TYPE_USB,             //adc 0x1f (none id)
    MUIC_SM5504_CABLE_TYPE_OTG,             //adc 0x00, regDev1&0x01
    /* TA Group */
    MUIC_SM5504_CABLE_TYPE_REGULAR_TA,      //adc 0x1f (none id, D+ short to D-)
    MUIC_SM5504_CABLE_TYPE_ATT_TA,          //adc 0x1f (none id, only VBUS)AT&T
    MUIC_SM5504_CABLE_TYPE_0x15,            //adc 0x15
    MUIC_SM5504_CABLE_TYPE_TYPE1_CHARGER,   //adc 0x17 (id : 200k)
    MUIC_SM5504_CABLE_TYPE_0x1A,            //adc 0x1A
    /* JIG Group */
    MUIC_SM5504_CABLE_TYPE_JIG_USB_OFF,     //adc 0x18
    MUIC_SM5504_CABLE_TYPE_JIG_USB_ON,      //adc 0x19
    MUIC_SM5504_CABLE_TYPE_JIG_UART_OFF,    //adc 0x1C
    MUIC_SM5504_CABLE_TYPE_JIG_UART_ON,     //adc 0x1D
    // JIG type with VBUS
    MUIC_SM5504_CABLE_TYPE_JIG_UART_OFF_WITH_VBUS,    //adc 0x1C
    MUIC_SM5504_CABLE_TYPE_JIG_UART_ON_WITH_VBUS,     //adc 0x1D

    MUIC_SM5504_CABLE_TYPE_CDP, // USB Charging downstream port, usually treated as SDP
    MUIC_SM5504_CABLE_TYPE_L_USB, // L-USB cable
    MUIC_SM5504_CABLE_TYPE_UNKNOWN,
    MUIC_SM5504_CABLE_TYPE_INVALID, // Un-initialized

    MUIC_SM5504_CABLE_TYPE_OTG_WITH_VBUS,
};

typedef enum {
    JIG_USB_BOOT_OFF,
    JIG_USB_BOOT_ON,
    JIG_UART_BOOT_OFF,
    JIG_UART_BOOT_ON,
} jig_type_t;

struct sm5504_platform_data {
    int irq_gpio;
    void (*cable_chg_callback)(int32_t cable_type);
    void (*ocp_callback)(void);
    void (*otp_callback)(void);
    void (*ovp_callback)(void);
    void (*usb_callback)(uint8_t attached);
    void (*uart_callback)(uint8_t attached);
    void (*otg_callback)(uint8_t attached);
    void (*jig_callback)(jig_type_t type, uint8_t attached);
    void (*vbus_callback)(uint8_t attached);

};

#endif // LINUX_MFD_SM5504_H

