/*
 * drivers/video/sprdfb/lcd/lcd_ili6150_lvds.c
 *
 * Copyright (C) 2014 Spreadtrum Communications Inc.
 *
 * Author: Haibing.Yang <haibing.yang@spreadtrum.com>
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

#include <linux/delay.h>
#include <asm/io.h>
#include "sprdfb.h"

#define GPIO_BASE_ADDR 0x40280000
#define GPIO_LVDSCHIP_OFFSET 0x480

#ifndef BIT
#define BIT(x) (1<<x)
#endif

#define GPIO_SCK_BIT BIT(6)
#define GPIO_CS_BIT BIT(7)
#define GPIO_SDI_BIT BIT(8)

#define GPIO_SPI_SCK_PIN 6
#define GPIO_SPI_CS_PIN 7
#define GPIO_SPI_SDI_PIN 8

#define DELAY_CNT 10

static unsigned long gpio_base_addr;

struct gpio_ctrl_reg {
	volatile u32 data; /* bits data */
	volatile u32 msk; /* bits data mask */
	volatile u32 dir; /* bits data direction */
	volatile u32 is; /* bits interrupt sense */
	volatile u32 ibe; /* bits both edges interrupt */
	volatile u32 iev; /* bits interrupt event */
	volatile u32 ie; /* bits interrupt enable */
	volatile u32 ris; /* bits raw interrupt status */
	volatile u32 mis; /* bits masked interrupt status */
	volatile u32 inen; /* input enable */
};

static int gpio_spi_write(u32 index, u8 len_i, u32 data, u8 len_d)
{
	u32 i, reg_val;
	volatile struct gpio_ctrl_reg *regs =
			(volatile struct gpio_ctrl_reg *)
			(gpio_base_addr + GPIO_LVDSCHIP_OFFSET);

	regs->inen = 0;
	regs->msk = GPIO_SCK_BIT | GPIO_CS_BIT | GPIO_SDI_BIT;
	regs->dir = GPIO_SCK_BIT | GPIO_CS_BIT | GPIO_SDI_BIT;
	regs->data = GPIO_SCK_BIT | GPIO_CS_BIT;

	/* Bit[7] output 1, CS Enable */
	reg_val = GPIO_CS_BIT;
	regs->data = reg_val;
	udelay(DELAY_CNT);

	/* all output 0 */
	reg_val = 0;
	regs->data = reg_val;
	/* Write data */
	for (i = 0; i < len_d; ++i) {
		reg_val = 0;
		/* Get a bit value */
		reg_val = ((data >> (len_d - i -1)) << GPIO_SPI_SDI_PIN) & GPIO_SDI_BIT;
		regs->data = reg_val;
		udelay(DELAY_CNT);
		/* High clock to set bit */
		regs->data |= GPIO_SCK_BIT;
		udelay(DELAY_CNT);
	}

	/* Write index */
	for (i = 0; i < len_i; ++i) {
		reg_val = 0;
		/* Get a bit value */
		reg_val = ((index >> (len_i - i -1)) << GPIO_SPI_SDI_PIN) & GPIO_SDI_BIT;
		regs->data = reg_val;
		udelay(DELAY_CNT);
		/* High clock to set bit */
		regs->data |= GPIO_SCK_BIT;
		udelay(DELAY_CNT);
	}

	regs->data = 0;
	udelay(DELAY_CNT);
	regs->data |= GPIO_CS_BIT;
	udelay(DELAY_CNT);
	regs->data |= GPIO_SCK_BIT;
	udelay(DELAY_CNT * 20);

	return 0;
}

static int gpio_spi_read(u32 index, u8 len_i, u32 data, u8 len_d)
{
	u32 i, reg_val = 0, rd_val = 0;
	u32 rd_reg[16] = { 0 };
	u32 rd_reg1[16] = { 0 };
	u32 rd_reg2[16] = { 0 };
	volatile struct gpio_ctrl_reg *regs =
			(volatile struct gpio_ctrl_reg *)
			(gpio_base_addr + GPIO_LVDSCHIP_OFFSET);

	regs->inen = 0;
	regs->msk = GPIO_SCK_BIT | GPIO_CS_BIT | GPIO_SDI_BIT;
	regs->dir = GPIO_SCK_BIT | GPIO_CS_BIT | GPIO_SDI_BIT;
	regs->data = GPIO_SCK_BIT | GPIO_CS_BIT;

	/* Bit[7] output 1 */
	reg_val = GPIO_CS_BIT;
	regs->data = reg_val;
	udelay(DELAY_CNT);

	/* all output 0 */
	reg_val = 0;
	regs->data = reg_val;

	for (i = 0; i < len_d; ++i) {
		reg_val = 0;
		reg_val = ((data >> (len_d - i -1)) << GPIO_SPI_SDI_PIN) & GPIO_SDI_BIT;
		regs->data = reg_val;
		udelay(DELAY_CNT);

		regs->data |= GPIO_SCK_BIT;
		udelay(DELAY_CNT);
	}

	for (i = 0; i < len_i; ++i) {
		reg_val = 0;
		reg_val = ((index >> (len_i - i -1)) << GPIO_SPI_SDI_PIN) & GPIO_SDI_BIT;
		regs->data = reg_val;
		udelay(DELAY_CNT);

		regs->data |= GPIO_SCK_BIT;
		udelay(DELAY_CNT);
	}

	regs->data = 0;
	udelay(DELAY_CNT);
	regs->data |= GPIO_CS_BIT;
	udelay(DELAY_CNT);
	regs->inen |= GPIO_SDI_BIT;
	regs->dir &= ~GPIO_SDI_BIT;
	regs->data |= GPIO_SCK_BIT;
	udelay(DELAY_CNT);
	regs->data &= ~GPIO_SCK_BIT;

	for (i = 0; i < 16; ++i) {
		udelay(DELAY_CNT);
		regs->data |= GPIO_SCK_BIT;
		rd_reg[i] = regs->data & GPIO_SDI_BIT;
		rd_reg1[i] = rd_reg[i] >> 10;
		rd_reg2[i] = rd_reg1[i] << (15 - i);
		rd_val |= rd_reg2[i];
		udelay(DELAY_CNT);
		regs->data &= ~GPIO_SCK_BIT;
	}

	udelay(DELAY_CNT);
	regs->data |= GPIO_CS_BIT;
	udelay(DELAY_CNT);
	regs->data |= GPIO_SCK_BIT;
	regs->data |= GPIO_SDI_BIT;
	udelay(DELAY_CNT * 50);

	return rd_val;
}

int sprdchip_lvds_init(void)
{
	u32 i, rd_val1, rd_val2;
	gpio_base_addr = ioremap(GPIO_BASE_ADDR, 0x1000);
	for (i = 0; i < 50; ++i) {
		gpio_spi_write(27, 18, 0, 16);
		rd_val1 = gpio_spi_read(63, 18, 27, 16);
	}

	for (i = 0; i < 50; ++i) {
		gpio_spi_write(28, 18, 10240, 16);
		gpio_spi_write(27, 18, 16, 16);
		rd_val1 = gpio_spi_read(63, 18, 27, 16);
		rd_val2 = gpio_spi_read(63, 18, 28, 16);
	}
	return 0;
}
