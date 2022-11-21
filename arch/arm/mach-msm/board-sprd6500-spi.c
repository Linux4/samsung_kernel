/* arch/arm/mach-capri/board-baffin-spi.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/gpio.h>


#include <linux/irq.h>
#include <linux/spi/spi.h>
#include <linux/platform_device.h>
#include <mach/gpiomux.h>

/* These gpios are defined in apq8084-mif-trlte-ctc-r00.dtsi */
/*
#define GPIO_IPC_MRDY		16
#define GPIO_IPC_SUB_MRDY   141
#define GPIO_IPC_SRDY		124
#define GPIO_IPC_SUB_SRDY	136
#define GPIO_CP_DUMP_INT	123
*/

struct ipc_spi_platform_data {
	const char *name;
	unsigned gpio_ipc_mrdy;
	unsigned gpio_ipc_srdy;
	unsigned gpio_ipc_sub_mrdy;
	unsigned gpio_ipc_sub_srdy;
	unsigned gpio_cp_dump_int;

	void (*cfg_gpio)(void);
};

static struct ipc_spi_platform_data spi_modem_data;


static struct platform_device spi_modem = {
	.name = "if_spi_platform_driver",
	.id = -1,
	.dev = {
		.platform_data = &spi_modem_data,
	},
};

static int __init init_spi(void)
{
	pr_info("[SPI] %s\n", __func__);
	platform_device_register(&spi_modem);
/*
	spi_register_board_info(modem_if_spi_device,
		ARRAY_SIZE(modem_if_spi_device));
*/
	return 0;
}

module_init(init_spi);
