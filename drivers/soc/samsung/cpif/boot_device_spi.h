/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019, Samsung Electronics.
 *
 */

#ifndef __BOOT_DEVICE_SPI_H__
#define __BOOT_DEVICE_SPI_H__

#if IS_ENABLED(CONFIG_BOOT_DEVICE_SPI)
extern int cpboot_spi_load_cp_image(struct link_device *ld, struct io_device *iod,
				    unsigned long arg);
#else
static inline int cpboot_spi_load_cp_image(struct link_device *ld, struct io_device *iod,
					   unsigned long arg) { return -ENXIO; }
#endif

#endif /* __BOOT_DEVICE_SPI_H__ */
