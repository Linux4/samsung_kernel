/*
 * Copyright (C) 2014 Samsung
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

#include <linux/kernel.h>
#include <linux/delay.h>
#include "../sprdfb_panel.h"
#ifdef CONFIG_LCD_ESD_RECOVERY
#include "../esd_detect.h"
#endif

#define HX8369b_SpiWriteCmd(cmd)\
{\
	spi_send_cmd((cmd & 0xFF));\
}

#define  HX8369b_SpiWriteData(data)\
{\
	spi_send_data((data & 0xFF));\
}
unsigned int panel_id = 0x55bd90;

static int32_t hx8369b_init_0_4T(struct panel_spec *self)
{
	uint32_t test_data[8] = {0};
	uint32_t left = 0;
	uint32_t top = 0;
	uint32_t right = 480;
	uint32_t bottom = 800;
	uint32_t data = 0;
	spi_send_cmd_t spi_send_cmd =
			self->info.rgb->bus_info.spi->ops->spi_send_cmd;
	spi_send_data_t spi_send_data =
			self->info.rgb->bus_info.spi->ops->spi_send_data;
	spi_read_t spi_read = self->info.rgb->bus_info.spi->ops->spi_read;

	pr_info("hx8369b_init\n");

	HX8369b_SpiWriteCmd(0xB9);	/*SET EXTC*/
	HX8369b_SpiWriteData(0xFF);
	HX8369b_SpiWriteData(0x83);
	HX8369b_SpiWriteData(0x69);

	/* Power Setting Sequence*/

	HX8369b_SpiWriteCmd(0xB1);	/* Set Power*/
	HX8369b_SpiWriteData(0x0B);
	HX8369b_SpiWriteData(0x83);
	HX8369b_SpiWriteData(0x77);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x11);
	HX8369b_SpiWriteData(0x11);
	HX8369b_SpiWriteData(0x08);
	HX8369b_SpiWriteData(0x08);
	HX8369b_SpiWriteData(0x0C);
	HX8369b_SpiWriteData(0x12);

	HX8369b_SpiWriteCmd(0xC6);	/* Set Source EQ*/
	HX8369b_SpiWriteData(0x41);
	HX8369b_SpiWriteData(0xFF);
	HX8369b_SpiWriteData(0x7A);
	HX8369b_SpiWriteData(0xFF);

	HX8369b_SpiWriteCmd(0xE3);	/*Set Source EQ*/
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);

	HX8369b_SpiWriteCmd(0xC0);	/*Set Source Option*/
	HX8369b_SpiWriteData(0x73);
	HX8369b_SpiWriteData(0x50);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x34);
	HX8369b_SpiWriteData(0xC4);
	HX8369b_SpiWriteData(0x00);

	/*Initializing Sequence*/

	HX8369b_SpiWriteCmd(0x3A);	/*Interface Pixel*/
	HX8369b_SpiWriteData(0x70);

	HX8369b_SpiWriteCmd(0xB3);	/*Set RGB Interface*/
	HX8369b_SpiWriteData(0x83);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x31);
	HX8369b_SpiWriteData(0x03);
	HX8369b_SpiWriteData(0x01);
	HX8369b_SpiWriteData(0x13);
	HX8369b_SpiWriteData(0x06);

	HX8369b_SpiWriteCmd(0xB4);	/*Set column inversion*/
	HX8369b_SpiWriteData(0x00);

	HX8369b_SpiWriteCmd(0xCC);	/*Set  DGC*/
	HX8369b_SpiWriteData(0x0C);

	HX8369b_SpiWriteCmd(0xEA);	/*Set Color*/
	HX8369b_SpiWriteData(0x72);

	HX8369b_SpiWriteCmd(0xB2);	/*Set Display*/
	HX8369b_SpiWriteData(0x03);

	HX8369b_SpiWriteCmd(0xD5);	/*GIP Timing SET*/
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x0D);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x12);
	HX8369b_SpiWriteData(0x40);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x01);
	HX8369b_SpiWriteData(0x60);
	HX8369b_SpiWriteData(0x37);
	HX8369b_SpiWriteData(0x00);

	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x0F);
	HX8369b_SpiWriteData(0x1E);
	HX8369b_SpiWriteData(0x1F);
	HX8369b_SpiWriteData(0x47);
	HX8369b_SpiWriteData(0x03);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x03);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x03);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x18);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x89);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x11);
	HX8369b_SpiWriteData(0x33);
	HX8369b_SpiWriteData(0x55);
	HX8369b_SpiWriteData(0x77);
	HX8369b_SpiWriteData(0x31);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x98);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x66);
	HX8369b_SpiWriteData(0x44);
	HX8369b_SpiWriteData(0x22);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x02);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x89);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x22);
	HX8369b_SpiWriteData(0x44);
	HX8369b_SpiWriteData(0x66);
	HX8369b_SpiWriteData(0x20);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x98);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x77);
	HX8369b_SpiWriteData(0x55);
	HX8369b_SpiWriteData(0x33);
	HX8369b_SpiWriteData(0x11);
	HX8369b_SpiWriteData(0x13);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x01);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x03);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0xCF);
	HX8369b_SpiWriteData(0xFF);
	HX8369b_SpiWriteData(0xFF);
	HX8369b_SpiWriteData(0x03);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0xCF);
	HX8369b_SpiWriteData(0xFF);
	HX8369b_SpiWriteData(0xFF);
	HX8369b_SpiWriteData(0x20);
	HX8369b_SpiWriteData(0x8C);
	HX8369b_SpiWriteData(0x5A);

	/*Gamam Setting Sequence*/

	HX8369b_SpiWriteCmd(0xE0);	/*Gamma*/
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x08);
	HX8369b_SpiWriteData(0x08);
	HX8369b_SpiWriteData(0x3F);
	HX8369b_SpiWriteData(0x18);
	HX8369b_SpiWriteData(0x2D);
	HX8369b_SpiWriteData(0x02);
	HX8369b_SpiWriteData(0x0E);
	HX8369b_SpiWriteData(0x0E);
	HX8369b_SpiWriteData(0x14);
	HX8369b_SpiWriteData(0x16);

	HX8369b_SpiWriteData(0x15);
	HX8369b_SpiWriteData(0x15);
	HX8369b_SpiWriteData(0x13);
	HX8369b_SpiWriteData(0x16);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x0A);
	HX8369b_SpiWriteData(0x0D);
	HX8369b_SpiWriteData(0x3F);
	HX8369b_SpiWriteData(0x18);
	HX8369b_SpiWriteData(0x2B);
	HX8369b_SpiWriteData(0x02);
	HX8369b_SpiWriteData(0x08);
	HX8369b_SpiWriteData(0x0E);
	HX8369b_SpiWriteData(0x13);
	HX8369b_SpiWriteData(0x16);
	HX8369b_SpiWriteData(0x14);
	HX8369b_SpiWriteData(0x15);
	HX8369b_SpiWriteData(0x12);
	HX8369b_SpiWriteData(0x16);
	HX8369b_SpiWriteData(0x01);

	HX8369b_SpiWriteCmd(0xC1);
	HX8369b_SpiWriteData(0x01);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x08);
	HX8369b_SpiWriteData(0x10);
	HX8369b_SpiWriteData(0x18);
	HX8369b_SpiWriteData(0x1F);
	HX8369b_SpiWriteData(0x27);
	HX8369b_SpiWriteData(0x2E);
	HX8369b_SpiWriteData(0x34);
	HX8369b_SpiWriteData(0x3E);

	HX8369b_SpiWriteData(0x48);
	HX8369b_SpiWriteData(0x50);
	HX8369b_SpiWriteData(0x58);
	HX8369b_SpiWriteData(0x60);
	HX8369b_SpiWriteData(0x68);
	HX8369b_SpiWriteData(0x70);
	HX8369b_SpiWriteData(0x78);
	HX8369b_SpiWriteData(0x80);
	HX8369b_SpiWriteData(0x88);
	HX8369b_SpiWriteData(0x90);

	HX8369b_SpiWriteData(0x98);
	HX8369b_SpiWriteData(0xA0);
	HX8369b_SpiWriteData(0xA8);
	HX8369b_SpiWriteData(0xB0);
	HX8369b_SpiWriteData(0xB8);
	HX8369b_SpiWriteData(0xC0);
	HX8369b_SpiWriteData(0xC8);
	HX8369b_SpiWriteData(0xD0);
	HX8369b_SpiWriteData(0xD8);
	HX8369b_SpiWriteData(0xE0);

	HX8369b_SpiWriteData(0xE8);
	HX8369b_SpiWriteData(0xF0);
	HX8369b_SpiWriteData(0xF7);
	HX8369b_SpiWriteData(0xFF);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);

	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x08);
	HX8369b_SpiWriteData(0x10);
	HX8369b_SpiWriteData(0x18);
	HX8369b_SpiWriteData(0x1F);
	HX8369b_SpiWriteData(0x27);
	HX8369b_SpiWriteData(0x2E);

	HX8369b_SpiWriteData(0x34);
	HX8369b_SpiWriteData(0x3E);
	HX8369b_SpiWriteData(0x48);
	HX8369b_SpiWriteData(0x50);
	HX8369b_SpiWriteData(0x58);
	HX8369b_SpiWriteData(0x60);
	HX8369b_SpiWriteData(0x68);
	HX8369b_SpiWriteData(0x70);
	HX8369b_SpiWriteData(0x78);
	HX8369b_SpiWriteData(0x80);

	HX8369b_SpiWriteData(0x88);
	HX8369b_SpiWriteData(0x90);
	HX8369b_SpiWriteData(0x98);
	HX8369b_SpiWriteData(0xA0);
	HX8369b_SpiWriteData(0xA8);
	HX8369b_SpiWriteData(0xB0);
	HX8369b_SpiWriteData(0xB8);
	HX8369b_SpiWriteData(0xC0);
	HX8369b_SpiWriteData(0xC8);
	HX8369b_SpiWriteData(0xD0);

	HX8369b_SpiWriteData(0xD8);
	HX8369b_SpiWriteData(0xE0);
	HX8369b_SpiWriteData(0xE8);
	HX8369b_SpiWriteData(0xF0);
	HX8369b_SpiWriteData(0xF7);
	HX8369b_SpiWriteData(0xFF);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);

	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x08);
	HX8369b_SpiWriteData(0x10);
	HX8369b_SpiWriteData(0x18);
	HX8369b_SpiWriteData(0x1F);

	HX8369b_SpiWriteData(0x27);
	HX8369b_SpiWriteData(0x2E);
	HX8369b_SpiWriteData(0x34);
	HX8369b_SpiWriteData(0x3E);
	HX8369b_SpiWriteData(0x48);
	HX8369b_SpiWriteData(0x50);
	HX8369b_SpiWriteData(0x58);
	HX8369b_SpiWriteData(0x60);
	HX8369b_SpiWriteData(0x68);
	HX8369b_SpiWriteData(0x70);

	HX8369b_SpiWriteData(0x78);
	HX8369b_SpiWriteData(0x80);
	HX8369b_SpiWriteData(0x88);
	HX8369b_SpiWriteData(0x90);
	HX8369b_SpiWriteData(0x98);
	HX8369b_SpiWriteData(0xA0);
	HX8369b_SpiWriteData(0xA8);
	HX8369b_SpiWriteData(0xB0);
	HX8369b_SpiWriteData(0xB8);
	HX8369b_SpiWriteData(0xC0);

	HX8369b_SpiWriteData(0xC8);
	HX8369b_SpiWriteData(0xD0);
	HX8369b_SpiWriteData(0xD8);
	HX8369b_SpiWriteData(0xE0);
	HX8369b_SpiWriteData(0xE8);
	HX8369b_SpiWriteData(0xF0);
	HX8369b_SpiWriteData(0xF7);
	HX8369b_SpiWriteData(0xFF);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);

	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);

	/* Dispaly on */
	HX8369b_SpiWriteCmd(0x11);
	msleep(120);
	HX8369b_SpiWriteCmd(0x29);
	msleep(50);

	return 0;
}

static int32_t hx8369b_init(struct panel_spec *self)
{
	uint32_t test_data[8] = {0};
	uint32_t left = 0;
	uint32_t top = 0;
	uint32_t right = 480;
	uint32_t bottom = 800;
	uint32_t data = 0;
	spi_send_cmd_t spi_send_cmd = self->info.rgb->bus_info.spi->ops->spi_send_cmd;
	spi_send_data_t spi_send_data = self->info.rgb->bus_info.spi->ops->spi_send_data;
	spi_read_t spi_read = self->info.rgb->bus_info.spi->ops->spi_read;


	pr_info("hx8369b_init\n");

	HX8369b_SpiWriteCmd(0xB9);
	HX8369b_SpiWriteData(0xFF);
	HX8369b_SpiWriteData(0x83);
	HX8369b_SpiWriteData(0x69);

	/* Power Setting Sequence */

	HX8369b_SpiWriteCmd(0xB1);
	HX8369b_SpiWriteData(0x0B);
	HX8369b_SpiWriteData(0x83);
	HX8369b_SpiWriteData(0x77);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x11);
	HX8369b_SpiWriteData(0x11);
	HX8369b_SpiWriteData(0x08);
	HX8369b_SpiWriteData(0x08);
	HX8369b_SpiWriteData(0x0C);
	HX8369b_SpiWriteData(0x12);


	HX8369b_SpiWriteCmd(0xC6);
	HX8369b_SpiWriteData(0x41);
	HX8369b_SpiWriteData(0xFF);
	HX8369b_SpiWriteData(0x7A);
	HX8369b_SpiWriteData(0xFF);

	HX8369b_SpiWriteCmd(0xE3);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);

	HX8369b_SpiWriteCmd(0xC0);
	HX8369b_SpiWriteData(0x73);
	HX8369b_SpiWriteData(0x50);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x34);
	HX8369b_SpiWriteData(0xC4);
	HX8369b_SpiWriteData(0x00);

	/* Initializing Sequence */

	HX8369b_SpiWriteCmd(0x3A);
	HX8369b_SpiWriteData(0x70);

	HX8369b_SpiWriteCmd(0xB3);
	HX8369b_SpiWriteData(0x83);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x31);
	HX8369b_SpiWriteData(0x03);
	HX8369b_SpiWriteData(0x01);
	HX8369b_SpiWriteData(0x13);
	HX8369b_SpiWriteData(0x06);

	HX8369b_SpiWriteCmd(0xB4);
	HX8369b_SpiWriteData(0x00);

	HX8369b_SpiWriteCmd(0xCC);
	HX8369b_SpiWriteData(0x0C);

	HX8369b_SpiWriteCmd(0xEA);
	HX8369b_SpiWriteData(0x72);

	HX8369b_SpiWriteCmd(0xB2);
	HX8369b_SpiWriteData(0x03);

	HX8369b_SpiWriteCmd(0xD5);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x0D);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x12);
	HX8369b_SpiWriteData(0x40);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x01);
	HX8369b_SpiWriteData(0x60);
	HX8369b_SpiWriteData(0x37);
	HX8369b_SpiWriteData(0x00);

	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x0F);
	HX8369b_SpiWriteData(0x1E);
	HX8369b_SpiWriteData(0x1F);
	HX8369b_SpiWriteData(0x47);
	HX8369b_SpiWriteData(0x03);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x03);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x03);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x18);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x89);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x11);
	HX8369b_SpiWriteData(0x33);
	HX8369b_SpiWriteData(0x55);
	HX8369b_SpiWriteData(0x77);
	HX8369b_SpiWriteData(0x31);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x98);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x66);
	HX8369b_SpiWriteData(0x44);
	HX8369b_SpiWriteData(0x22);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x02);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x89);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x22);
	HX8369b_SpiWriteData(0x44);
	HX8369b_SpiWriteData(0x66);
	HX8369b_SpiWriteData(0x20);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x98);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x77);
	HX8369b_SpiWriteData(0x55);
	HX8369b_SpiWriteData(0x33);
	HX8369b_SpiWriteData(0x11);
	HX8369b_SpiWriteData(0x13);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x01);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x03);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0xCF);
	HX8369b_SpiWriteData(0xFF);
	HX8369b_SpiWriteData(0xFF);
	HX8369b_SpiWriteData(0x03);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0xCF);
	HX8369b_SpiWriteData(0xFF);
	HX8369b_SpiWriteData(0xFF);
	HX8369b_SpiWriteData(0x20);
	HX8369b_SpiWriteData(0x8C);
	HX8369b_SpiWriteData(0x5A);

	/* Gamam Setting Sequence */

	HX8369b_SpiWriteCmd(0xE0);	/*Gamma*/
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x09);
	HX8369b_SpiWriteData(0x08);
	HX8369b_SpiWriteData(0x3F);
	HX8369b_SpiWriteData(0x15);
	HX8369b_SpiWriteData(0x2B);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x0D);
	HX8369b_SpiWriteData(0x0E);
	HX8369b_SpiWriteData(0x14);
	HX8369b_SpiWriteData(0x16);

	HX8369b_SpiWriteData(0x16);
	HX8369b_SpiWriteData(0x15);
	HX8369b_SpiWriteData(0x12);
	HX8369b_SpiWriteData(0x15);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x0A);
	HX8369b_SpiWriteData(0x0D);
	HX8369b_SpiWriteData(0x3F);
	HX8369b_SpiWriteData(0x15);
	HX8369b_SpiWriteData(0x2B);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x06);
	HX8369b_SpiWriteData(0x0E);
	HX8369b_SpiWriteData(0x14);
	HX8369b_SpiWriteData(0x16);
	HX8369b_SpiWriteData(0x14);
	HX8369b_SpiWriteData(0x16);
	HX8369b_SpiWriteData(0x11);
	HX8369b_SpiWriteData(0x15);
	HX8369b_SpiWriteData(0x01);

	HX8369b_SpiWriteCmd(0xC1);	/*Gamma*/
	HX8369b_SpiWriteData(0x01);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x08);
	HX8369b_SpiWriteData(0x10);
	HX8369b_SpiWriteData(0x18);
	HX8369b_SpiWriteData(0x1F);
	HX8369b_SpiWriteData(0x27);
	HX8369b_SpiWriteData(0x2E);
	HX8369b_SpiWriteData(0x34);
	HX8369b_SpiWriteData(0x3E);

	HX8369b_SpiWriteData(0x48);
	HX8369b_SpiWriteData(0x50);
	HX8369b_SpiWriteData(0x58);
	HX8369b_SpiWriteData(0x60);
	HX8369b_SpiWriteData(0x68);
	HX8369b_SpiWriteData(0x70);
	HX8369b_SpiWriteData(0x78);
	HX8369b_SpiWriteData(0x80);
	HX8369b_SpiWriteData(0x88);
	HX8369b_SpiWriteData(0x90);

	HX8369b_SpiWriteData(0x98);
	HX8369b_SpiWriteData(0xA0);
	HX8369b_SpiWriteData(0xA8);
	HX8369b_SpiWriteData(0xB0);
	HX8369b_SpiWriteData(0xB8);
	HX8369b_SpiWriteData(0xC0);
	HX8369b_SpiWriteData(0xC8);
	HX8369b_SpiWriteData(0xD0);
	HX8369b_SpiWriteData(0xD8);
	HX8369b_SpiWriteData(0xE0);

	HX8369b_SpiWriteData(0xE8);
	HX8369b_SpiWriteData(0xF0);
	HX8369b_SpiWriteData(0xF7);
	HX8369b_SpiWriteData(0xFF);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);

	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x08);
	HX8369b_SpiWriteData(0x10);
	HX8369b_SpiWriteData(0x18);
	HX8369b_SpiWriteData(0x1F);
	HX8369b_SpiWriteData(0x27);
	HX8369b_SpiWriteData(0x2E);

	HX8369b_SpiWriteData(0x34);
	HX8369b_SpiWriteData(0x3E);
	HX8369b_SpiWriteData(0x48);
	HX8369b_SpiWriteData(0x50);
	HX8369b_SpiWriteData(0x58);
	HX8369b_SpiWriteData(0x60);
	HX8369b_SpiWriteData(0x68);
	HX8369b_SpiWriteData(0x70);
	HX8369b_SpiWriteData(0x78);
	HX8369b_SpiWriteData(0x80);

	HX8369b_SpiWriteData(0x88);
	HX8369b_SpiWriteData(0x90);
	HX8369b_SpiWriteData(0x98);
	HX8369b_SpiWriteData(0xA0);
	HX8369b_SpiWriteData(0xA8);
	HX8369b_SpiWriteData(0xB0);
	HX8369b_SpiWriteData(0xB8);
	HX8369b_SpiWriteData(0xC0);
	HX8369b_SpiWriteData(0xC8);
	HX8369b_SpiWriteData(0xD0);

	HX8369b_SpiWriteData(0xD8);
	HX8369b_SpiWriteData(0xE0);
	HX8369b_SpiWriteData(0xE8);
	HX8369b_SpiWriteData(0xF0);
	HX8369b_SpiWriteData(0xF7);
	HX8369b_SpiWriteData(0xFF);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);

	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x08);
	HX8369b_SpiWriteData(0x10);
	HX8369b_SpiWriteData(0x18);
	HX8369b_SpiWriteData(0x1F);

	HX8369b_SpiWriteData(0x27);
	HX8369b_SpiWriteData(0x2E);
	HX8369b_SpiWriteData(0x34);
	HX8369b_SpiWriteData(0x3E);
	HX8369b_SpiWriteData(0x48);
	HX8369b_SpiWriteData(0x50);
	HX8369b_SpiWriteData(0x58);
	HX8369b_SpiWriteData(0x60);
	HX8369b_SpiWriteData(0x68);
	HX8369b_SpiWriteData(0x70);

	HX8369b_SpiWriteData(0x78);
	HX8369b_SpiWriteData(0x80);
	HX8369b_SpiWriteData(0x88);
	HX8369b_SpiWriteData(0x90);
	HX8369b_SpiWriteData(0x98);
	HX8369b_SpiWriteData(0xA0);
	HX8369b_SpiWriteData(0xA8);
	HX8369b_SpiWriteData(0xB0);
	HX8369b_SpiWriteData(0xB8);
	HX8369b_SpiWriteData(0xC0);

	HX8369b_SpiWriteData(0xC8);
	HX8369b_SpiWriteData(0xD0);
	HX8369b_SpiWriteData(0xD8);
	HX8369b_SpiWriteData(0xE0);
	HX8369b_SpiWriteData(0xE8);
	HX8369b_SpiWriteData(0xF0);
	HX8369b_SpiWriteData(0xF7);
	HX8369b_SpiWriteData(0xFF);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);

	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);
	HX8369b_SpiWriteData(0x00);

	/*Dispaly on*/
	HX8369b_SpiWriteCmd(0x11);
	msleep(120);
	HX8369b_SpiWriteCmd(0x29);
	msleep(50);

	return 0;
}

static int32_t hx8369b_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	spi_send_cmd_t spi_send_cmd =
			self->info.rgb->bus_info.spi->ops->spi_send_cmd;
	spi_send_data_t spi_send_data =
			self->info.rgb->bus_info.spi->ops->spi_send_data;

	if (is_sleep) {
		/*Sleep In*/
		HX8369b_SpiWriteCmd(0x28);
		usleep_range(20000, 25000);
		HX8369b_SpiWriteCmd(0x10);
		msleep(120);
	} else {
		/*Sleep Out*/
		HX8369b_SpiWriteCmd(0x11);
		msleep(120);
		HX8369b_SpiWriteCmd(0x29);
		usleep_range(30000, 35000);
	}

	return 0;
}

static int32_t hx8369b_set_window(struct panel_spec *self,
		uint16_t left, uint16_t top, uint16_t right, uint16_t bottom)
{
	uint32_t *test_data[4] = {0};
	spi_send_cmd_t spi_send_cmd =
			self->info.rgb->bus_info.spi->ops->spi_send_cmd;
	spi_send_data_t spi_send_data =
			self->info.rgb->bus_info.spi->ops->spi_send_data;
	spi_read_t spi_read = self->info.rgb->bus_info.spi->ops->spi_read;

	return 0;
}
static int32_t hx8369b_invalidate(struct panel_spec *self)
{
	return self->ops->panel_set_window(self, 0, 0, self->width - 1,
							self->height - 1);
}

static int32_t hx8369b_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom)
{
	return self->ops->panel_set_window(self, left, top, right, bottom);
}

static int32_t hx8369b_read_id(struct panel_spec *self)
{
	int32_t id  = 0x62;
	spi_send_cmd_t spi_send_cmd =
			self->info.rgb->bus_info.spi->ops->spi_send_cmd;
	spi_send_data_t spi_send_data =
			self->info.rgb->bus_info.spi->ops->spi_send_data;
	spi_read_t spi_read = self->info.rgb->bus_info.spi->ops->spi_read;

	pr_info("sprdfb hx8369b_read_id\n");
	HX8369b_SpiWriteCmd(0xB9);	/*SET password*/
	HX8369b_SpiWriteData(0xFF);
	HX8369b_SpiWriteData(0x83);
	HX8369b_SpiWriteData(0x69);

	HX8369b_SpiWriteCmd(0xFE);	/*SET SPI READ INDEX*/
	HX8369b_SpiWriteData(0xF4);	/*GETHXID*/
	HX8369b_SpiWriteCmd(0xFF);	/* GET SPI READ*/

	spi_read(&id);
	id &= 0xff;
	pr_info(" hx8369b_read_id kernel id = %x\n", id);

	return 0x55bd90;
}

#ifdef CONFIG_LCD_ESD_RECOVERY
struct esd_det_info hx8396b_esd_info = {
	.name = "hx8396b",
	.mode = ESD_DET_INTERRUPT,
	.gpio = 86,
	.state = ESD_DET_NOT_INITIALIZED,
	.level = ESD_DET_HIGH,
};
#endif

#ifdef CONFIG_FB_BL_EVENT_CTRL
static int32_t hx8369b_backlight_ctrl(struct panel_spec *self, uint16_t bl_on)
{
	if (bl_on)
		fb_bl_notifier_call_chain(FB_BL_EVENT_RESUME, NULL);
	else
		fb_bl_notifier_call_chain(FB_BL_EVENT_SUSPEND, NULL);

	return 0;
}
#endif

static struct panel_operations lcd_hx8369b_rgb_spi_operations = {
	.panel_init = hx8369b_init,
	.panel_set_window = hx8369b_set_window,
	.panel_invalidate_rect = hx8369b_invalidate_rect,
	.panel_invalidate = hx8369b_invalidate,
	.panel_enter_sleep = hx8369b_enter_sleep,
	.panel_readid = hx8369b_read_id,
#ifdef CONFIG_FB_BL_EVENT_CTRL
	.panel_set_brightness = hx8369b_backlight_ctrl,
#endif
};

static struct timing_rgb lcd_hx8369b_rgb_timing = {
	.hfp = 30,
	.hbp = 20,
	.hsync = 20,
	.vfp = 6,
	.vbp = 19,
	.vsync = 4,
};

static struct spi_info lcd_hx8369b_rgb_spi_info = {
	.ops = NULL,
};

static struct info_rgb lcd_hx8369b_rgb_info = {
	.cmd_bus_mode  = SPRDFB_RGB_BUS_TYPE_SPI,
	.video_bus_width = 24, /*18,16*/
	.h_sync_pol = SPRDFB_POLARITY_NEG,
	.v_sync_pol = SPRDFB_POLARITY_NEG,
	.de_pol = SPRDFB_POLARITY_POS,
	.timing = &lcd_hx8369b_rgb_timing,
	.bus_info = {
		.spi = &lcd_hx8369b_rgb_spi_info,
	}
};

struct panel_spec lcd_panel_hx8369b_rgb_spi_spec = {
	.width = 480,
	.height = 800,
	.width_hw = 52,
	.height_hw = 87,
	.fps = 60,
	.suspend_mode = SEND_SLEEP_CMD,
	.type = LCD_MODE_RGB,
	.direction = LCD_DIRECT_NORMAL,
	.is_clean_lcd = false,
	.info = {
		.rgb = &lcd_hx8369b_rgb_info
	},
	.ops = &lcd_hx8369b_rgb_spi_operations,
#ifdef CONFIG_LCD_ESD_RECOVERY
	.esd_info = &hx8396b_esd_info,
#endif

};

static struct panel_operations lcd_hx8369b_rgb_spi_operations_0_4T = {
	.panel_init = hx8369b_init_0_4T,
	.panel_set_window = hx8369b_set_window,
	.panel_invalidate_rect = hx8369b_invalidate_rect,
	.panel_invalidate = hx8369b_invalidate,
	.panel_enter_sleep = hx8369b_enter_sleep,
	.panel_readid = hx8369b_read_id,
#ifdef CONFIG_FB_BL_EVENT_CTRL
	.panel_set_brightness = hx8369b_backlight_ctrl,
#endif
};

struct panel_spec lcd_panel_hx8369b_rgb_spi_spec_0_4T = {
	.width = 480,
	.height = 800,
	.width_hw = 52,
	.height_hw = 87,
	.fps = 60,
	.suspend_mode = SEND_SLEEP_CMD,
	.type = LCD_MODE_RGB,
	.direction = LCD_DIRECT_NORMAL,
	.is_clean_lcd = false,
	.info = {
		.rgb = &lcd_hx8369b_rgb_info
	},
	.ops = &lcd_hx8369b_rgb_spi_operations_0_4T,
#ifdef CONFIG_LCD_ESD_RECOVERY
	.esd_info = &hx8396b_esd_info,
#endif

};

struct panel_cfg lcd_hx8369b_rgb_spi_0_4T = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x55bf90,
	.lcd_name = "lcd_hx8369b_rgb_spi",
	.panel = &lcd_panel_hx8369b_rgb_spi_spec_0_4T,
};
struct panel_cfg lcd_hx8369b_rgb_spi_main = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x55bc90,
	.lcd_name = "lcd_hx8369b_rgb_spi",
	.panel = &lcd_panel_hx8369b_rgb_spi_spec,
};
struct panel_cfg lcd_hx8369b_rgb_spi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x55bd90,
	.lcd_name = "lcd_hx8369b_rgb_spi",
	.panel = &lcd_panel_hx8369b_rgb_spi_spec,
};
static int __init lcd_hx8369b_rgb_spi_init(void)
{
	sprdfb_panel_register(&lcd_hx8369b_rgb_spi_0_4T);
	sprdfb_panel_register(&lcd_hx8369b_rgb_spi_main);
	sprdfb_panel_register(&lcd_hx8369b_rgb_spi);
	return 0;
}

subsys_initcall(lcd_hx8369b_rgb_spi_init);
