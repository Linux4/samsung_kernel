/* Copyright (C) 2010 Spreadtrum
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


unsigned int panel_id = 0x5544F0;

#define sc7798_SpiWriteCmd(cmd)\
{\
	spi_send_cmd((cmd & 0xFF));\
}

#define  sc7798_SpiWriteData(data)\
{\
	spi_send_data((data & 0xFF));\
}
static int32_t sc7798_init(struct panel_spec *self)
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

	pr_info("sc7798_init\n");

	sc7798_SpiWriteCmd(0xB9);
	sc7798_SpiWriteData(0xF1);
	sc7798_SpiWriteData(0x08);
	sc7798_SpiWriteData(0x00);

	sc7798_SpiWriteCmd(0xBC);
	sc7798_SpiWriteData(0x67);

	sc7798_SpiWriteCmd(0x3A);
	sc7798_SpiWriteData(0x70);

	sc7798_SpiWriteCmd(0xB2);
	sc7798_SpiWriteData(0x03);

	sc7798_SpiWriteCmd(0xB4);
	sc7798_SpiWriteData(0x00);

	sc7798_SpiWriteCmd(0xB1);
	sc7798_SpiWriteData(0x22);
	sc7798_SpiWriteData(0x1B);
	sc7798_SpiWriteData(0x1B);
	sc7798_SpiWriteData(0xB7);
	sc7798_SpiWriteData(0x22);
	sc7798_SpiWriteData(0x02);
	sc7798_SpiWriteData(0xA8);

	sc7798_SpiWriteCmd(0xC6);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0xFF);

	sc7798_SpiWriteCmd(0xCC);
	sc7798_SpiWriteData(0x0C);

	sc7798_SpiWriteCmd(0xE3);
	sc7798_SpiWriteData(0x02);
	sc7798_SpiWriteData(0x02);
	sc7798_SpiWriteData(0x02);
	sc7798_SpiWriteData(0x02);

	sc7798_SpiWriteCmd(0xB8);
	sc7798_SpiWriteData(0x07);
	sc7798_SpiWriteData(0x22);

	sc7798_SpiWriteCmd(0xB5);
	sc7798_SpiWriteData(0x09);
	sc7798_SpiWriteData(0x09);

	sc7798_SpiWriteCmd(0xC0);
	sc7798_SpiWriteData(0x73);
	sc7798_SpiWriteData(0x50);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x08);
	sc7798_SpiWriteData(0x70);

	sc7798_SpiWriteCmd(0xB3);
	sc7798_SpiWriteData(0x01);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x06);
	sc7798_SpiWriteData(0x06);
	sc7798_SpiWriteData(0x10);
	sc7798_SpiWriteData(0x0A);
	sc7798_SpiWriteData(0x45);
	sc7798_SpiWriteData(0x40);

	sc7798_SpiWriteCmd(0xB9);
	sc7798_SpiWriteData(0xF1);
	sc7798_SpiWriteData(0x08);
	sc7798_SpiWriteData(0x00);

	sc7798_SpiWriteCmd(0xE9);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x08);
	sc7798_SpiWriteData(0x03);
	sc7798_SpiWriteData(0x2F);
	sc7798_SpiWriteData(0x89);
	sc7798_SpiWriteData(0x6A);
	sc7798_SpiWriteData(0x12);
	sc7798_SpiWriteData(0x31);
	sc7798_SpiWriteData(0x23);
	sc7798_SpiWriteData(0x48);
	sc7798_SpiWriteData(0x0C);
	sc7798_SpiWriteData(0x89);
	sc7798_SpiWriteData(0x6A);
	sc7798_SpiWriteData(0x47);
	sc7798_SpiWriteData(0x02);
	sc7798_SpiWriteData(0x04);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x20);
	sc7798_SpiWriteData(0x88);
	sc7798_SpiWriteData(0x88);
	sc7798_SpiWriteData(0x40);
	sc7798_SpiWriteData(0x28);
	sc7798_SpiWriteData(0x69);
	sc7798_SpiWriteData(0x48);
	sc7798_SpiWriteData(0x88);
	sc7798_SpiWriteData(0x88);
	sc7798_SpiWriteData(0x80);
	sc7798_SpiWriteData(0x88);
	sc7798_SpiWriteData(0x88);
	sc7798_SpiWriteData(0x51);
	sc7798_SpiWriteData(0x38);
	sc7798_SpiWriteData(0x79);
	sc7798_SpiWriteData(0x58);
	sc7798_SpiWriteData(0x88);
	sc7798_SpiWriteData(0x88);
	sc7798_SpiWriteData(0x81);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);

	sc7798_SpiWriteCmd(0xEA);
	sc7798_SpiWriteData(0x88);
	sc7798_SpiWriteData(0x88);
	sc7798_SpiWriteData(0x37);
	sc7798_SpiWriteData(0x59);
	sc7798_SpiWriteData(0x18);
	sc7798_SpiWriteData(0x18);
	sc7798_SpiWriteData(0x88);
	sc7798_SpiWriteData(0x88);
	sc7798_SpiWriteData(0x85);
	sc7798_SpiWriteData(0x88);
	sc7798_SpiWriteData(0x88);
	sc7798_SpiWriteData(0x26);
	sc7798_SpiWriteData(0x49);
	sc7798_SpiWriteData(0x08);
	sc7798_SpiWriteData(0x08);
	sc7798_SpiWriteData(0x88);
	sc7798_SpiWriteData(0x88);
	sc7798_SpiWriteData(0x84);
	sc7798_SpiWriteData(0x30);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0xFF);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x50);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);

	sc7798_SpiWriteCmd(0xE0);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x04);
	sc7798_SpiWriteData(0x04);
	sc7798_SpiWriteData(0x0A);
	sc7798_SpiWriteData(0x18);
	sc7798_SpiWriteData(0x2B);
	sc7798_SpiWriteData(0x05);
	sc7798_SpiWriteData(0x0C);
	sc7798_SpiWriteData(0x11);
	sc7798_SpiWriteData(0x16);
	sc7798_SpiWriteData(0x18);
	sc7798_SpiWriteData(0x16);
	sc7798_SpiWriteData(0x16);
	sc7798_SpiWriteData(0x15);
	sc7798_SpiWriteData(0x19);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x04);
	sc7798_SpiWriteData(0x04);
	sc7798_SpiWriteData(0x0A);
	sc7798_SpiWriteData(0x18);
	sc7798_SpiWriteData(0x2C);
	sc7798_SpiWriteData(0x05);
	sc7798_SpiWriteData(0x0C);
	sc7798_SpiWriteData(0x12);
	sc7798_SpiWriteData(0x16);
	sc7798_SpiWriteData(0x18);
	sc7798_SpiWriteData(0x16);
	sc7798_SpiWriteData(0x17);
	sc7798_SpiWriteData(0x16);
	sc7798_SpiWriteData(0x19);

	sc7798_SpiWriteCmd(0x11);
	msleep(120);
	sc7798_SpiWriteCmd(0x29);
	msleep(50);

	return 0;
}

static int32_t sc7798_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	spi_send_cmd_t spi_send_cmd =
			self->info.rgb->bus_info.spi->ops->spi_send_cmd;
	spi_send_data_t spi_send_data =
			self->info.rgb->bus_info.spi->ops->spi_send_data;

	if (is_sleep) {
		/*Sleep In*/
		sc7798_SpiWriteCmd(0x28);
		msleep(120);
		sc7798_SpiWriteCmd(0xB2);
		sc7798_SpiWriteData(0x23);
		sc7798_SpiWriteCmd(0x10);
		usleep_range(10000, 15000);
	} else {
		/*Sleep Out*/
		sc7798_SpiWriteCmd(0x11);
		msleep(120);
		sc7798_SpiWriteCmd(0x29);
		usleep_range(10000, 15000);
	}

	return 0;
}

static int32_t sc7798_set_window(struct panel_spec *self,
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

static int32_t sc7798_invalidate(struct panel_spec *self)
{
	return self->ops->panel_set_window(self, 0, 0, self->width - 1,
							self->height - 1);
}

static int32_t sc7798_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom)
{
	return self->ops->panel_set_window(self, left, top,
			right, bottom);
}

static int32_t sc7798_read_id(struct panel_spec *self)
{
#if 0
	int32_t id  = 0x62;
	spi_send_cmd_t spi_send_cmd =
			self->info.rgb->bus_info.spi->ops->spi_send_cmd;
	spi_send_data_t spi_send_data =
			self->info.rgb->bus_info.spi->ops->spi_send_data;
	spi_read_t spi_read = self->info.rgb->bus_info.spi->ops->spi_read;
/*
	pr_info("sprdfb hx8363_read_id\n");
	HX8363_SpiWriteCmd(0xB9);	/*SET password*/
	HX8363_SpiWriteData(0xFF);
	HX8363_SpiWriteData(0x83);
	HX8363_SpiWriteData(0x63);

	HX8363_SpiWriteCmd(0xFE);	/*SET SPI READ INDEX*/
	HX8363_SpiWriteData(0xF4);	/*GETHXID*/
	HX8363_SpiWriteCmd(0xFF);	/*GET SPI READ*/

	spi_read(&id);
*/
	id &= 0xff;
	pr_info(" hx8363_read_id kernel id = %x\n", id);
#endif
	return 0x7798;
}
#ifdef CONFIG_FB_BL_EVENT_CTRL
static int32_t sc7798_backlight_ctrl(struct panel_spec *self, uint16_t bl_on)
{
	if (bl_on)
		fb_bl_notifier_call_chain(FB_BL_EVENT_RESUME, NULL);
	else
		fb_bl_notifier_call_chain(FB_BL_EVENT_SUSPEND, NULL);

	return 0;
}
#endif
static struct panel_operations lcd_sc7798_rgb_spi_operations = {
	.panel_init = sc7798_init,
	.panel_set_window = sc7798_set_window,
	.panel_invalidate_rect = sc7798_invalidate_rect,
	.panel_invalidate = sc7798_invalidate,
	.panel_enter_sleep = sc7798_enter_sleep,
	.panel_readid = sc7798_read_id,
#ifdef CONFIG_FB_BL_EVENT_CTRL
	.panel_set_brightness = sc7798_backlight_ctrl,
#endif
};

static struct timing_rgb lcd_sc7798_rgb_timing = {
	.hfp = 20,
	.hbp = 40,
	.hsync = 20,
	.vfp = 8,
	.vbp = 12,
	.vsync = 4,

};

static struct spi_info lcd_sc7798_rgb_spi_info = {
	.ops = NULL,
};

static struct info_rgb lcd_sc7798_rgb_info = {
	.cmd_bus_mode  = SPRDFB_RGB_BUS_TYPE_SPI,
	.video_bus_width = 24, /*18,16*/
	.h_sync_pol = SPRDFB_POLARITY_NEG,
	.v_sync_pol = SPRDFB_POLARITY_NEG,
	.de_pol = SPRDFB_POLARITY_POS,
	.timing = &lcd_sc7798_rgb_timing,
	.bus_info = {
		.spi = &lcd_sc7798_rgb_spi_info,
	}
};

struct panel_spec lcd_panel_sc7798_rgb_spi_spec = {

	.width = 480,
	.height = 800,
	.width_hw = 56,
	.height_hw = 94,
	.fps = 61,
	.type = LCD_MODE_RGB,
	.suspend_mode = SEND_SLEEP_CMD,
	.direction = LCD_DIRECT_NORMAL,
	.is_clean_lcd = false,
	.info = {
		.rgb = &lcd_sc7798_rgb_info
	},
	.ops = &lcd_sc7798_rgb_spi_operations,
};

struct panel_cfg lcd_sc7798_rgb_spi_spec_main = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x5544f0,
	.lcd_name = "lcd_sc7798_rgb_spi",
	.panel = &lcd_panel_sc7798_rgb_spi_spec,
};

struct panel_cfg lcd_sc7798_rgb_spi_spec = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x7798,
	.lcd_name = "lcd_sc7798_rgb_spi",
	.panel = &lcd_panel_sc7798_rgb_spi_spec,
};

static int __init lcd_sc7798_rgb_spi_init(void)
{
	sprdfb_panel_register(&lcd_sc7798_rgb_spi_spec_main);
	sprdfb_panel_register(&lcd_sc7798_rgb_spi_spec);
	return 0;

}

subsys_initcall(lcd_sc7798_rgb_spi_init);
