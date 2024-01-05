/* Copyright (C) 2013 Samsung Electronics Co, Ltd.
 *
 * Based on mach-sc\board-huskytd\board-huskytd-serial.c
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


#include <linux/platform_device.h>

#include <mach/serial_sprd.h>
#include <mach/hardware.h>
#include <mach/irqs.h>

#include "board-huskytd.h"

static struct resource sprd_serial_resources0[] = {
	[0] = {
		.start = SPRD_UART0_BASE,
		.end = SPRD_UART0_BASE + SPRD_UART0_SIZE-1,
		.name = "serial_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SER0_INT,
		.end = IRQ_SER0_INT,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device sprd_serial_device0 = {
	.name           = "serial_sprd",
	.id             =  0,
	.num_resources  = ARRAY_SIZE(sprd_serial_resources0),
	.resource       = sprd_serial_resources0,
};

static struct resource sprd_serial_resources1[] = {
	[0] = {
		.start = SPRD_UART1_BASE,
		.end = SPRD_UART1_BASE + SPRD_UART1_SIZE-1,
		.name = "serial_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SER1_INT,
		.end = IRQ_SER1_INT,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device sprd_serial_device1 = {
	.name           = "serial_sprd",
	.id             =  1,
	.num_resources  = ARRAY_SIZE(sprd_serial_resources1),
	.resource       = sprd_serial_resources1,
};

static struct resource sprd_serial_resources2[] = {
	[0] = {
		.start = SPRD_UART2_BASE,
		.end = SPRD_UART2_BASE + SPRD_UART2_SIZE - 1,
		.name = "serial_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SER2_INT,
		.end = IRQ_SER2_INT,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device sprd_serial_device2 = {
	.name           = "serial_sprd",
	.id             =  2,
	.num_resources  = ARRAY_SIZE(sprd_serial_resources2),
	.resource       = sprd_serial_resources2,
};

static struct resource sprd_serial_resources3[] = {
	[0] = {
		.start = SPRD_UART3_BASE,
		.end = SPRD_UART3_BASE + SPRD_UART3_SIZE - 1,
		.name = "serial_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SER3_INT,
		.end = IRQ_SER3_INT,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device sprd_serial_device3 = {
	.name           = "serial_sprd",
	.id             =  3,
	.num_resources  = ARRAY_SIZE(sprd_serial_resources3),
	.resource       = sprd_serial_resources3,
};

static struct serial_data plat_data0 = {
	.wakeup_type = BT_RTS_HIGH_WHEN_SLEEP,
	.clk = 48000000,
};

static struct serial_data plat_data1 = {
	.wakeup_type = BT_RTS_HIGH_WHEN_SLEEP,
	.clk = 26000000,
};

static struct serial_data plat_data2 = {
	.wakeup_type = BT_RTS_HIGH_WHEN_SLEEP,
	.clk = 26000000,
};

static void huskytd_uart_init(void)
{
	platform_device_add_data(&sprd_serial_device0, \
			(const void *)&plat_data0, sizeof(plat_data0));
	platform_device_add_data(&sprd_serial_device1, \
			(const void *)&plat_data1, sizeof(plat_data1));
	platform_device_add_data(&sprd_serial_device2,\
			(const void *)&plat_data2, sizeof(plat_data2));

	platform_device_register(&sprd_serial_device0);
	platform_device_register(&sprd_serial_device1);
	platform_device_register(&sprd_serial_device2);
}

static struct resource spi0_resources[] = {
	[0] = {
		.start = SPRD_SPI0_BASE,
		.end = SPRD_SPI0_BASE + SPRD_SPI0_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SPI0_INT,
		.end = IRQ_SPI0_INT,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource spi1_resources[] = {
	[0] = {
		.start = SPRD_SPI1_BASE,
		.end = SPRD_SPI1_BASE + SPRD_SPI1_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SPI1_INT,
		.end = IRQ_SPI1_INT,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource spi2_resources[] = {
	[0] =	{
			.start = SPRD_SPI2_BASE,
			.end = SPRD_SPI2_BASE + SPRD_SPI2_SIZE - 1,
			.flags = IORESOURCE_MEM,
			},
	[1] =	{
			.start = IRQ_SPI2_INT,
			.end = IRQ_SPI2_INT,
			.flags = IORESOURCE_IRQ,
			},
};

static struct platform_device sprd_spi0_device = {
	.name = "sprd_spi",
	.id = 0,
	.resource = spi0_resources,
	.num_resources = ARRAY_SIZE(spi0_resources),
};

static struct platform_device sprd_spi1_device = {
	.name = "sprd_spi",
	.id = 1,
	.resource = spi1_resources,
	.num_resources = ARRAY_SIZE(spi1_resources),
};

static struct platform_device sprd_spi2_device = {
	.name = "sprd_spi",
	.id = 2,
	.resource = spi2_resources,
	.num_resources = ARRAY_SIZE(spi2_resources),
};

static void huskytd_spi_init(void)
{
	platform_device_register(&sprd_spi0_device);
	platform_device_register(&sprd_spi1_device);
	platform_device_register(&sprd_spi2_device);
}

static struct resource sprd_i2c0_resources[] = {
	[0] = {
		.start = SPRD_I2C0_BASE,
		.end   = SPRD_I2C0_BASE + SPRD_I2C0_SIZE - 1,
		.name  = "i2c0_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_I2C0_INT,
		.end   = IRQ_I2C0_INT,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device sprd_i2c0_device = {
	.name           = "sprd-i2c",
	.id             =  0,
	.num_resources  = ARRAY_SIZE(sprd_i2c0_resources),
	.resource       = sprd_i2c0_resources,
};

static struct resource sprd_i2c1_resources[] = {
	[0] = {
		.start = SPRD_I2C1_BASE,
		.end   = SPRD_I2C1_BASE + SZ_4K - 1,
		.name  = "i2c1_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_I2C1_INT,
		.end   = IRQ_I2C1_INT,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device sprd_i2c1_device = {
	.name           = "sprd-i2c",
	.id             =  1,
	.num_resources  = ARRAY_SIZE(sprd_i2c1_resources),
	.resource       = sprd_i2c1_resources,
};


static struct resource sprd_i2c2_resources[] = {
	[0] = {
		.start = SPRD_I2C2_BASE,
		.end   = SPRD_I2C2_BASE + SZ_4K - 1,
		.name  = "i2c2_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_I2C2_INT,
		.end   = IRQ_I2C2_INT,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device sprd_i2c2_device = {
	.name           = "sprd-i2c",
	.id             =  2,
	.num_resources  = ARRAY_SIZE(sprd_i2c2_resources),
	.resource       = sprd_i2c2_resources,
};

static struct resource sprd_i2c3_resources[] = {
	[0] = {
		.start = SPRD_I2C3_BASE,
		.end   = SPRD_I2C3_BASE + SZ_4K - 1,
		.name  = "i2c3_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_I2C3_INT,
		.end   = IRQ_I2C3_INT,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device sprd_i2c3_device = {
	.name           = "sprd-i2c",
	.id             =  3,
	.num_resources  = ARRAY_SIZE(sprd_i2c3_resources),
	.resource       = sprd_i2c3_resources,
};

static void huskytd_i2c_init(void)
{
	platform_device_register(&sprd_i2c0_device);
	platform_device_register(&sprd_i2c1_device);
	platform_device_register(&sprd_i2c2_device);
	platform_device_register(&sprd_i2c3_device);
}

void huskytd_serial_init(void)
{
	huskytd_uart_init();
	huskytd_i2c_init();
	huskytd_spi_init();
}
