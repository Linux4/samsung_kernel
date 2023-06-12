/*
 *  kernel/driver/char/modem_interface/modem_spi_drv.c
 *
 *  Generic modem interface spi API.
 *
 *  Author:     Jiayong Yang(Jiayong.Yang@spreadtrum.com)
 *  Created:    Jul 27, 2012
 *  Copyright:  Spreadtrum Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/spi/spi.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include "modem_buffer.h"
#include "modem_interface_driver.h"

extern struct spi_device *sprd_spi_modem_device_register(int master_bus_num,struct spi_board_info *chip);

dma_addr_t spi2_rx_buf_dma_phys;
dma_addr_t spi2_tx_buf_dma_phys;
void *spi2_rx_buf_dma_virt;
void *spi2_tx_buf_dma_virt;

spinlock_t write_spinlock;
#define SPI_WRITE_DEBUG
#ifdef SPI_WRITE_DEBUG
spinlock_t log_spinlock;
#endif

static struct spi_device *spi_g=NULL;

static int modem_spi_open(void *para)
{
	return 0;
}
int modem_spi_read(char *buffer,int size)
{
	spi_read(spi_g,buffer,size);
}
int modem_spi_write(char *buffer,int size)
{
	unsigned short *data = buffer;
	spi_write(spi_g,buffer,size);
}

static int mux2spi_probe(struct spi_device *spi)
{
	int status;

	spi_g = spi;
	spi_g->mode = SPI_MODE_1;
	spi_g->max_speed_hz = 1000000;
	spi_g->bits_per_word = 32;

	spi2_rx_buf_dma_virt =dma_alloc_coherent(NULL, 4096,&spi2_rx_buf_dma_phys, GFP_KERNEL | GFP_DMA);

	if (spi2_rx_buf_dma_virt == NULL){
		printk(KERN_ERR "spi2_rx_buf_dma_virt alloc fail");
		return (-ENOMEM);
	}

	spi2_tx_buf_dma_virt =dma_alloc_coherent(NULL, 4096,&spi2_tx_buf_dma_phys, GFP_KERNEL | GFP_DMA);

	if (spi2_tx_buf_dma_virt == NULL){
		printk(KERN_ERR "spi2_tx_buf_dma_virt alloc fail");
		return (-ENOMEM);
	}

	spin_lock_init(&write_spinlock);
#ifdef SPI_WRITE_DEBUG
	spin_lock_init(&log_spinlock);
#endif
	status = spi_setup(spi_g);

  ret:
	return status;
}

static struct spi_driver mux_spi_drv = {
	.driver = {
		.name =         "spi_mux",
		.bus =          &spi_bus_type,
		.owner =        THIS_MODULE,
	},
	.probe =        mux2spi_probe,
};


static struct modem_device_operation	spi_device_op = {
	.dev  = MODEM_DEV_SPI,
	.open = modem_spi_open,
	.read = modem_spi_read,
	.write =modem_spi_write
};
struct modem_device_operation * modem_spi_drv_init(void)
{
	//sprd_spi_modem_device_register(0, NULL);
	spi_g = NULL;
	spi_register_driver(&mux_spi_drv);
	return &spi_device_op;
}
