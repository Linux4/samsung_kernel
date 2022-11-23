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
#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#include <linux/err.h>

#define ili9486_SpiWriteCmd(cmd)\
{\
	spi_send_cmd((cmd & 0xFF));\
}

#define  ili9486_SpiWriteData(data)\
{\
	spi_send_data((data & 0xFF));\
}

unsigned int panel_id = 0x9486;

static int32_t ili9486_init(struct panel_spec *self)
{
	uint32_t test_data[8] = {0};
	uint32_t left = 0;
	uint32_t top = 0;
	uint32_t right = 480;
	uint32_t bottom = 854;
	uint32_t data = 0;
	spi_send_cmd_t spi_send_cmd =
		self->info.rgb->bus_info.spi->ops->spi_send_cmd;
	spi_send_data_t spi_send_data =
		self->info.rgb->bus_info.spi->ops->spi_send_data;
	spi_read_t spi_read =
		self->info.rgb->bus_info.spi->ops->spi_read;

	pr_info("enter - ili9486_init\n");
	ili9486_SpiWriteCmd(0xC0);
	ili9486_SpiWriteData(0x0A);
	ili9486_SpiWriteData(0x0A);

	ili9486_SpiWriteCmd(0xC1);
	ili9486_SpiWriteData(0x45);

	ili9486_SpiWriteCmd(0xC2);
	ili9486_SpiWriteData(0x00);
	/*mdelay(150);*/
	/* Display parameter setting */
	ili9486_SpiWriteCmd(0x2A);
	ili9486_SpiWriteData(0x00);
	ili9486_SpiWriteData(0x00);
	ili9486_SpiWriteData(0x01);
	ili9486_SpiWriteData(0x3F);

	ili9486_SpiWriteCmd(0x2B);
	ili9486_SpiWriteData(0x00);
	ili9486_SpiWriteData(0x00);
	ili9486_SpiWriteData(0x01);
	ili9486_SpiWriteData(0xDF);

	ili9486_SpiWriteCmd(0x36);
	ili9486_SpiWriteData(0x08);

	ili9486_SpiWriteCmd(0x35);
	ili9486_SpiWriteData(0x00);

	ili9486_SpiWriteCmd(0x3A);
	ili9486_SpiWriteData(0x66);

	ili9486_SpiWriteCmd(0xB0);
	ili9486_SpiWriteData(0x8C);

	ili9486_SpiWriteCmd(0xB4);
	ili9486_SpiWriteData(0x02);

	ili9486_SpiWriteCmd(0xB6);
	ili9486_SpiWriteData(0x32);
	ili9486_SpiWriteData(0x42);
	ili9486_SpiWriteData(0x3B);

	ili9486_SpiWriteCmd(0xB7);
	ili9486_SpiWriteData(0xC6);

	ili9486_SpiWriteCmd(0xF9);
	ili9486_SpiWriteData(0x00);
	ili9486_SpiWriteData(0x08);

	ili9486_SpiWriteCmd(0xF8);
	ili9486_SpiWriteData(0x21);
	ili9486_SpiWriteData(0x07);
	ili9486_SpiWriteData(0x02);

	ili9486_SpiWriteCmd(0xF2);
	ili9486_SpiWriteData(0x18);
	ili9486_SpiWriteData(0xA3);
	ili9486_SpiWriteData(0x12);
	ili9486_SpiWriteData(0x02);
	ili9486_SpiWriteData(0xB2);
	ili9486_SpiWriteData(0x12);
	ili9486_SpiWriteData(0xFF);
	ili9486_SpiWriteData(0x10);
	ili9486_SpiWriteData(0x00);

	ili9486_SpiWriteCmd(0xF1);
	ili9486_SpiWriteData(0x36);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x00);
	ili9486_SpiWriteData(0x3C);
	ili9486_SpiWriteData(0x0F);
	ili9486_SpiWriteData(0x0F);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x02);

	ili9486_SpiWriteCmd(0xFC);
	ili9486_SpiWriteData(0x00);
	ili9486_SpiWriteData(0x0C);
	ili9486_SpiWriteData(0x80);

	usleep_range(10000, 15000);
	/* Gamma setting */
	ili9486_SpiWriteCmd(0xE0);
	ili9486_SpiWriteData(0x00);
	ili9486_SpiWriteData(0x15);
	ili9486_SpiWriteData(0x17);
	ili9486_SpiWriteData(0x0C);
	ili9486_SpiWriteData(0x0F);
	ili9486_SpiWriteData(0x07);
	ili9486_SpiWriteData(0x44);
	ili9486_SpiWriteData(0x65);
	ili9486_SpiWriteData(0x35);
	ili9486_SpiWriteData(0x07);
	ili9486_SpiWriteData(0x12);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x09);
	ili9486_SpiWriteData(0x06);
	ili9486_SpiWriteData(0x06);

	ili9486_SpiWriteCmd(0xE1);
	ili9486_SpiWriteData(0x0F);
	ili9486_SpiWriteData(0x38);
	ili9486_SpiWriteData(0x34);
	ili9486_SpiWriteData(0x0B);
	ili9486_SpiWriteData(0x0B);
	ili9486_SpiWriteData(0x03);
	ili9486_SpiWriteData(0x43);
	ili9486_SpiWriteData(0x22);
	ili9486_SpiWriteData(0x30);
	ili9486_SpiWriteData(0x03);
	ili9486_SpiWriteData(0x0E);
	ili9486_SpiWriteData(0x03);
	ili9486_SpiWriteData(0x21);
	ili9486_SpiWriteData(0x1B);
	ili9486_SpiWriteData(0x00);

	ili9486_SpiWriteCmd(0xE2);
	ili9486_SpiWriteData(0x00);
	ili9486_SpiWriteData(0x09);
	ili9486_SpiWriteData(0x09);
	ili9486_SpiWriteData(0x09);
	ili9486_SpiWriteData(0x09);
	ili9486_SpiWriteData(0x09);
	ili9486_SpiWriteData(0x09);
	ili9486_SpiWriteData(0x09);
	ili9486_SpiWriteData(0x09);
	ili9486_SpiWriteData(0x09);
	ili9486_SpiWriteData(0x09);
	ili9486_SpiWriteData(0x09);
	ili9486_SpiWriteData(0x09);
	ili9486_SpiWriteData(0x09);
	ili9486_SpiWriteData(0x09);
	ili9486_SpiWriteData(0x00);

	ili9486_SpiWriteCmd(0xE3);
	ili9486_SpiWriteData(0x00);
	ili9486_SpiWriteData(0x00);
	ili9486_SpiWriteData(0x00);
	ili9486_SpiWriteData(0x00);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x04);
	ili9486_SpiWriteData(0x00);

	/* Display on */
	ili9486_SpiWriteCmd(0x11);
	msleep(120);
	ili9486_SpiWriteCmd(0x29);
	msleep(200);

	return 0;
}

static int32_t ili9486_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	spi_send_cmd_t spi_send_cmd =
		self->info.rgb->bus_info.spi->ops->spi_send_cmd;
	spi_send_data_t spi_send_data =
		self->info.rgb->bus_info.spi->ops->spi_send_data;

	if (is_sleep) {
		ili9486_SpiWriteCmd(0x28);
		usleep_range(20000, 25000);
		ili9486_SpiWriteCmd(0x10);
		msleep(120);
	} else {
		ili9486_SpiWriteCmd(0x11);
		msleep(120);
		ili9486_SpiWriteCmd(0x29);
		msleep(200);
	}

	return 0;
}




static int32_t ili9486_set_window(struct panel_spec *self,
		uint16_t left, uint16_t top, uint16_t right, uint16_t bottom)
{
	uint32_t *test_data[4] = {0};
	spi_send_cmd_t spi_send_cmd =
		self->info.rgb->bus_info.spi->ops->spi_send_cmd;
	spi_send_data_t spi_send_data =
		self->info.rgb->bus_info.spi->ops->spi_send_data;
	spi_read_t spi_read =
		self->info.rgb->bus_info.spi->ops->spi_read;
	return 0;
}
static int32_t ili9486_invalidate(struct panel_spec *self)
{
	return self->ops->panel_set_window(self, 0, 0,
			self->width - 1, self->height - 1);
}

static int32_t ili9486_invalidate_rect(struct panel_spec *self,
		uint16_t left, uint16_t top,
		uint16_t right, uint16_t bottom)
{
	return self->ops->panel_set_window(self, left, top,
			right, bottom);
}

static int32_t ili9486_read_id(struct panel_spec *self)
{
	int32_t id  = 0x62;
	spi_send_cmd_t spi_send_cmd =
		self->info.rgb->bus_info.spi->ops->spi_send_cmd;
	spi_send_data_t spi_send_data =
		self->info.rgb->bus_info.spi->ops->spi_send_data;
	spi_read_t spi_read =
		self->info.rgb->bus_info.spi->ops->spi_read;

	pr_info("sprdfb ili9486_read_id\n");
	ili9486_SpiWriteCmd(0xB9);
	ili9486_SpiWriteData(0xFF);
	ili9486_SpiWriteData(0x83);
	ili9486_SpiWriteData(0x63);

	ili9486_SpiWriteCmd(0xFE);
	ili9486_SpiWriteData(0xF4);
	ili9486_SpiWriteCmd(0xFF);

	spi_read(&id);
	id &= 0xff;
	pr_info("ili9486_read_id kernel id = %x\n", id);

	return 0x9486;
}
#ifdef CONFIG_FB_BL_EVENT_CTRL
static int32_t ili9486_backlight_ctrl(struct panel_spec *self, uint16_t bl_on)
{
	if (bl_on)
		fb_bl_notifier_call_chain(FB_BL_EVENT_RESUME, NULL);
	else
		fb_bl_notifier_call_chain(FB_BL_EVENT_SUSPEND, NULL);

	return 0;
}
#endif
static int32_t ili9486_pwr_ctrl(struct panel_spec *self, int on_off)
{
	static struct regulator *vdd_2p8;

	vdd_2p8 = regulator_get(NULL, "vddcammot");

	pr_info("lcd vdd 2.8v boosting = [%d]\n", on_off);

	if (IS_ERR(vdd_2p8)) {
		vdd_2p8 = NULL;
		pr_err("lcd vdd 2.8v boosting fail\n");
		return -EIO;
	} else {
		regulator_set_voltage(vdd_2p8, 2800000, 2800000);
	}

	if (on_off)
		regulator_enable(vdd_2p8);
	else {
		regulator_disable(vdd_2p8);
		regulator_put(vdd_2p8);
		vdd_2p8 = NULL;
	}

	return 0;
}

static struct panel_operations lcd_ili9486_rgb_spi_operations = {
	.panel_init = ili9486_init,
	.panel_set_window = ili9486_set_window,
	.panel_invalidate_rect = ili9486_invalidate_rect,
	.panel_invalidate = ili9486_invalidate,
	.panel_enter_sleep = ili9486_enter_sleep,
	.panel_readid = ili9486_read_id,
#ifdef CONFIG_FB_BL_EVENT_CTRL
	.panel_set_brightness = ili9486_backlight_ctrl,
#endif
	.panel_pwr_ctrl = ili9486_pwr_ctrl,
};

static struct timing_rgb lcd_ili9486_rgb_timing = {
	.hfp = 30,
	.hbp = 26,
	.hsync = 4,
	.vfp = 12,
	.vbp = 8,
	.vsync = 2,
};

static struct spi_info lcd_ili9486_rgb_spi_info = {
	.ops = NULL,
};

static struct info_rgb lcd_ili9486_rgb_info = {
	.cmd_bus_mode  = SPRDFB_RGB_BUS_TYPE_SPI,
	.video_bus_width = 18, /*18,16*/
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.timing = &lcd_ili9486_rgb_timing,
	.bus_info = {
		.spi = &lcd_ili9486_rgb_spi_info,
	}
};

struct panel_spec lcd_panel_ili9486_rgb_spi = {
	.width = 320,
	.height = 480,
	.width_hw = 49,
	.height_hw = 74,
	.fps = 61,
	.suspend_mode = SEND_SLEEP_CMD,
	.type = LCD_MODE_RGB,
	.direction = LCD_DIRECT_NORMAL,
	.is_clean_lcd = false,
	.info = {
		.rgb = &lcd_ili9486_rgb_info
	},
	.ops = &lcd_ili9486_rgb_spi_operations,
};
struct panel_cfg lcd_ili9486_rgb_spi_main = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x5bbcd1,
	.lcd_name = "lcd_ili9486_rgb_spi",
	.panel = &lcd_panel_ili9486_rgb_spi,
};

struct panel_cfg lcd_ili9486_rgb_spi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x9486,
	.lcd_name = "lcd_ili9486_rgb_spi",
	.panel = &lcd_panel_ili9486_rgb_spi,
};
static int __init lcd_ili9486_rgb_spi_init(void)
{
	sprdfb_panel_register(&lcd_ili9486_rgb_spi_main);
	sprdfb_panel_register(&lcd_ili9486_rgb_spi);
	return 0;
}

subsys_initcall(lcd_ili9486_rgb_spi_init);
