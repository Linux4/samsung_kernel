/*
 *  linux/arch/arm/mach-sprd/modem_interface.h
 *
 *  Generic modem interface structure defination.
 *
 *  Author:     Jiayong.Yang(Jiayong.Yang@spreadtrum.com)
 *  Created:    Jul 27, 2012
 *  Copyright:  Spreadtrum Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#ifndef _MODEM_INTF_H
#define _MODEM_INTF_H

enum MODEM_Mode_type {
	MODEM_MODE_SHUTDOWN,
	MODEM_MODE_BOOT,
	MODEM_MODE_BOOTCOMP,
	MODEM_MODE_NORMAL,
	MODEM_MODE_DUMP,
	MODEM_MODE_RESET,
	MODEM_MODE_UNKNOWN
};

enum MODEM_device_type {
	MODEM_DEV_UART,
	MODEM_DEV_SPI,
	MODEM_DEV_SDIO,
	MODEM_DEV_VPIP
};

enum {
	MODEM_STATUS_REBOOT,
	MODEM_STATUS_ALVIE,
	MODEM_STATUS_ASSERT,
	MODEM_STATUS_POWERON
};

enum {
	MODEMSTS_LEVEL_MUX = 50,
	MODEMSTS_LEVEL_SDIO = 100,
	MODEMSTS_LEVEL_SPI = 150,
};

struct modemsts_chg {
	struct list_head link;
	int level;
	void *data;
	void (*modemsts_notifier)(struct modemsts_chg *h, unsigned int state);
};

struct modem_intf_platform_data{
    enum MODEM_device_type	dev_type;
    void             *modem_dev_parameter;
    int	        	modem_power_gpio;
    int              	modem_boot_gpio;
    int             modem_watchdog_gpio;
    int              	modem_alive_gpio;
    int              	modem_crash_gpio;
    int              modem_reset_gpio;
};
extern int modem_intf_open(enum MODEM_Mode_type mode,int index);
extern int modem_intf_read(char *buffer, int size,int index);
extern int modem_intf_write(char *buffer, int size,int index);
extern void modem_intf_set_mode(enum MODEM_Mode_type mode,int index);

extern int modemsts_notifier_register(struct modemsts_chg *handler);

extern int modemsts_notifier_unregister(struct modemsts_chg *handler);

extern void modemsts_change_notification(unsigned int state);
#endif
