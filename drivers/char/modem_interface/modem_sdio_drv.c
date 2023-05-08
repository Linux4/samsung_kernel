/*
 *  kernel/driver/char/modem_interface/modem_sdio_drv.c
 *
 *  Generic modem interface sdio API
 *
 *  Author:     Jiayong Yang(Jiayong.Yang@spreadtrum.com)
 *  Created:    Jul 27, 2012
 *  Copyright:  Spreadtrum Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */


#include <linux/mutex.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include "modem_buffer.h"
#include "modem_interface_driver.h"
extern struct spi_device *sprd_spi_modem_device_register(int master_bus_num,struct spi_board_info *chip);

extern int sprd_sdio_channel_open(void);
extern void sprd_sdio_channel_close(void);
extern int sprd_sdio_channel_rx(char *buf, int len, unsigned int addr);
extern int sprd_sdio_channel_tx(const char *buf, unsigned int len, unsigned int addr);

static int modem_sdio_open(void *para)
{
	return sprd_sdio_channel_open();
}
int modem_sdio_read(char *buffer,int size)
{
    return sprd_sdio_channel_rx(buffer,size, 0);
}
int modem_sdio_write(char *buffer,int size)
{
    return sprd_sdio_channel_tx((const char *)buffer,size, 0);
}

static struct modem_device_operation	sdio_device_op = {
	.dev = MODEM_DEV_SDIO,
	.open = modem_sdio_open,
	.read = modem_sdio_read,
	.write =modem_sdio_write
};
struct modem_device_operation * modem_sdio_drv_init(void)
{
	return &sdio_device_op;
}
