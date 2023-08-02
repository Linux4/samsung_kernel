/*
 *  linux/drivers/mmc/host/sdio_hal.h - Secure Digital Host Controller Interface driver
 *
 *  Copyright (C) 2005-2008 Pierre Ossman, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */
#ifndef __SDHCI_HAL_H
#define __SDHCI_HAL_H

#include <linux/scatterlist.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/bitops.h>

#define   SDHCI_TRANSFER_DIRECT_WRITE         0
#define   SDHCI_TRANSFER_DIRECT_READ          1

#define   SDHCI_TRANSFER_OK                   0
#define   SDHCI_TRANSFER_ERROR                1
#define   SDHCI_TRANSFER_TIMEOUT              2

#define  SDHCI_SUCCESS     0
#define  SDHCI_ERROR       1
#define  SDHCI_TIMEOUT     2

int  sdhci_hal_gpio_init(void);
int  sdhci_hal_gpio_irq_init(void);
void sdhci_hal_gpio_exit(void);

int  cp2ap_req(void);
void set_ap_status(void);
void clear_ap_status(void);
int  cp2ap_rts(void);
void ap2cp_req_enable(void);
void ap2cp_req_disable(void);
void ap2cp_rdy_enable(void);
void ap2cp_rdy_disable(void);
u32  sdhci_connect(u32 direction);
u32  sdhci_disconnect(u32  status);
u32  sdhci_resetconnect(u32 status);
void check_cp2ap_rdy(void);

#endif /* __SDHCI_H */
