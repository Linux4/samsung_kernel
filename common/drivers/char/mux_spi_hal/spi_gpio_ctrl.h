/*
 *
 *
 *  Copyright (C) 2005-2008 Pierre Ossman, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */
#ifndef __SPI_GPIO_CTRL_H
#define __SPI_GPIO_CTRL_H

#include <linux/scatterlist.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/bitops.h>

#include "ipc_spi.h"

extern int cp2ap_sts(void);

extern int ap2cp_sts(void);

extern void ap2cp_enable(void);

extern void ap2cp_disable(void);

extern void ap2cp_wakeup(void);

extern void ap2cp_sleep(void);

extern int spi_hal_gpio_init(void);

extern int spi_hal_gpio_irq_init(struct ipc_spi_dev *dev);

extern void spi_hal_gpio_exit(void);

#endif /* __SPI_GPIO_CTRL_H */
