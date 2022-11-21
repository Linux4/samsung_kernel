#ifndef SAMSUNG_SPI_COMMON_H
#define SAMSUNG_SPI_COMMON_H

#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include "ss_dsi_panel_common.h"

enum DDI_SPI_STATUS {
	DDI_SPI_SUSPEND = 0,
	DDI_SPI_RESUME,
};

struct samsung_display_driver_data;

int ss_spi_read(struct spi_device *spi, u8 *buf,
				int tx_bpw, int rx_bpw, int tx_size, int rx_size, u8 rx_addr);
int ss_spi_init(struct samsung_display_driver_data *vdd);

#endif
