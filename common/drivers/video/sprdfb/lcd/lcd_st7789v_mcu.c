/*
 * Copyright (C) 2010 Spreadtrum
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

static int32_t st7789v_init(struct panel_spec *self)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_data_t send_data = self->info.mcu->ops->send_data;

	pr_info("st7789v_init\n");
	send_cmd(0x11);
	msleep(120);

	if (self->info.mcu->bus_width == 8) {
		/*Initializing Sequence*/
		send_cmd(0xDF);
		send_data(0x5A69);
		send_data(0x0201);

		send_cmd(0x36);
		send_data(0x0000);

		send_cmd(0x3A);
		send_data(0x0500);

		send_cmd(0x35);
		send_data(0x0000);

		send_cmd(0xD2);
		send_data(0x4C00);

		send_cmd(0xB2);
		send_data(0x0404);
		send_data(0x0033);
		send_data(0x3300);

		send_cmd(0xC6);
		send_data(0x0A00);

		send_cmd(0xB8);
		send_data(0x2A2B);
		send_data(0x0500);

		/*Power setting sequence*/
		send_cmd(0xB7);
		send_data(0x7500);

		send_cmd(0xBB);
		send_data(0x1B00);

		send_cmd(0xC2);
		send_data(0x0100);

		send_cmd(0xC3);
		send_data(0x0B00);

		send_cmd(0xC4);
		send_data(0x2000);

		send_cmd(0xD0);
		send_data(0xA4A1);

		send_cmd(0xE8);
		send_data(0x8300);

		send_cmd(0xE9);
		send_data(0x0909);
		send_data(0x0500);

		send_cmd(0xC0);
		send_data(0x2500);

		send_cmd(0xB0);
		send_data(0x00D0);
		/*Display Gamma Setting Sequence*/
		send_cmd(0xE0);
		send_data(0xD00E);
		send_data(0x1810);
		send_data(0x0E2A);
		send_data(0x3E54);
		send_data(0x4C2B);
		send_data(0x1716);
		send_data(0x1D21);

		send_cmd(0xE1);
		send_data(0xD00E);
		send_data(0x1710);
		send_data(0x0E29);
		send_data(0x3C44);
		send_data(0x4D2C);
		send_data(0x1615);
		send_data(0x1E22);

		/*Display on*/
		send_cmd(0x29);
		msleep(100);
	} else { /* 16/18/24bit*/
		send_cmd(0xDF);
		send_data(0x5A);
		send_data(0x69);
		send_data(0x02);
		send_data(0x01);

		send_cmd(0x36);
		send_data(0x00);

		send_cmd(0x3A);
		send_data(0x06);

		send_cmd(0x35);
		send_data(0x00);

		send_cmd(0xD2);
		send_data(0x4C);

		send_cmd(0xB2);
		send_data(0x04);
		send_data(0x04);
		send_data(0x00);
		send_data(0x33);
		send_data(0x33);

		send_cmd(0xC6);
		send_data(0x0A);

		send_cmd(0xC0);
		send_data(0x25);

		send_cmd(0xB8);
		send_data(0x2A);
		send_data(0x2B);
		send_data(0x05);
		send_data(0x75);

		send_cmd(0xBA);
		send_data(0x04);

		send_cmd(0xB7);
		send_data(0x75);
		send_cmd(0xBB);
		send_data(0x1B);
		send_cmd(0xC2);
		send_data(0x01);
		send_cmd(0xC3);
		send_data(0x0B);
		send_cmd(0xC4);
		send_data(0x20);
		send_cmd(0xD0);
		send_data(0xA4);
		send_data(0xA1);
		send_cmd(0xE8);
		send_data(0x83);
		send_cmd(0xE9);
		send_data(0x09);
		send_data(0x09);
		send_data(0x05);

		/*Gamma setting*/
		send_cmd(0xE0);
		send_data(0xD0);
		send_data(0x0A);
		send_data(0x12);
		send_data(0x0D);
		send_data(0x0D);
		send_data(0x28);
		send_data(0x3A);
		send_data(0x44);
		send_data(0x4B);
		send_data(0x2A);
		send_data(0x16);
		send_data(0x15);
		send_data(0x1D);
		send_data(0x20);

		send_cmd(0xE1);
		send_data(0xD0);
		send_data(0x0A);
		send_data(0x12);
		send_data(0x0D);
		send_data(0x0D);
		send_data(0x28);
		send_data(0x3A);
		send_data(0x43);
		send_data(0x4B);
		send_data(0x2B);
		send_data(0x16);
		send_data(0x15);
		send_data(0x1D);
		send_data(0x21);

		send_cmd(0xE2);
		send_data(0x00);
		send_data(0x04);
		send_data(0x08);
		send_data(0x0C);
		send_data(0x10);
		send_data(0x14);
		send_data(0x18);
		send_data(0x1C);
		send_data(0x20);
		send_data(0x24);
		send_data(0x28);
		send_data(0x2C);
		send_data(0x2F);
		send_data(0x34);
		send_data(0x38);

		send_data(0x3C);
		send_data(0x40);
		send_data(0x44);
		send_data(0x48);
		send_data(0x4C);
		send_data(0x50);
		send_data(0x54);
		send_data(0x58);
		send_data(0x5C);
		send_data(0x60);
		send_data(0x64);
		send_data(0x68);
		send_data(0x6C);
		send_data(0x70);
		send_data(0x74);

		send_data(0x78);
		send_data(0x7C);
		send_data(0x80);
		send_data(0x84);
		send_data(0x88);
		send_data(0x8C);
		send_data(0x90);
		send_data(0x94);
		send_data(0x98);
		send_data(0x9C);
		send_data(0xA0);
		send_data(0xA4);
		send_data(0xA8);
		send_data(0xAC);
		send_data(0xB0);

		send_data(0xB4);
		send_data(0xB8);
		send_data(0xBC);
		send_data(0xC0);
		send_data(0xC4);
		send_data(0xC8);
		send_data(0xCC);
		send_data(0xD0);
		send_data(0xD4);
		send_data(0xD8);
		send_data(0xDC);
		send_data(0xE0);
		send_data(0xE4);
		send_data(0xE8);
		send_data(0xEC);

		send_data(0xF0);
		send_data(0xF4);
		send_data(0xF8);
		send_data(0xFC);

		send_cmd(0xE3);
		send_data(0x00);
		send_data(0x04);
		send_data(0x08);
		send_data(0x0C);
		send_data(0x10);
		send_data(0x14);
		send_data(0x18);
		send_data(0x1C);
		send_data(0x20);
		send_data(0x24);
		send_data(0x28);
		send_data(0x2C);
		send_data(0x2F);
		send_data(0x34);
		send_data(0x38);

		send_data(0x3C);
		send_data(0x40);
		send_data(0x44);
		send_data(0x48);
		send_data(0x4C);
		send_data(0x50);
		send_data(0x54);
		send_data(0x58);
		send_data(0x5C);
		send_data(0x60);
		send_data(0x64);
		send_data(0x68);
		send_data(0x6C);
		send_data(0x70);
		send_data(0x74);

		send_data(0x78);
		send_data(0x7C);
		send_data(0x80);
		send_data(0x84);
		send_data(0x88);
		send_data(0x8C);
		send_data(0x90);
		send_data(0x94);
		send_data(0x98);
		send_data(0x9C);
		send_data(0xA0);
		send_data(0xA4);
		send_data(0xA8);
		send_data(0xAC);
		send_data(0xB0);

		send_data(0xB4);
		send_data(0xB8);
		send_data(0xBC);
		send_data(0xC0);
		send_data(0xC4);
		send_data(0xC8);
		send_data(0xCC);
		send_data(0xD0);
		send_data(0xD4);
		send_data(0xD8);
		send_data(0xDC);
		send_data(0xE0);
		send_data(0xE4);
		send_data(0xE8);
		send_data(0xEC);

		send_data(0xF0);
		send_data(0xF4);
		send_data(0xF8);
		send_data(0xFC);

		send_cmd(0x29);
		msleep(100);
	}

	return 0;
}

static int32_t st7789v_set_window(struct panel_spec *self,
				  uint16_t left, uint16_t top, uint16_t right,
				  uint16_t bottom)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_data_t send_data = self->info.mcu->ops->send_data;

	pr_debug("st7789v_set_window\n");

	if (self->info.mcu->bus_width == 8) {
		send_cmd(0x002A); /*col*/
		send_data(left);
		send_data(right);

		send_cmd(0x002B); /*row*/
		send_data(top);
		send_data(bottom);
		send_cmd(0x002C);
	} else { /*16/18/24bit*/
		send_cmd(0x2A);	/*col*/
		send_data((left >> 8));
		send_data((left & 0xFF));
		send_data((right >> 8));
		send_data((right & 0xFF));

		send_cmd(0x2B);	/*row*/
		send_data((top >> 8));
		send_data((top & 0xFF));
		send_data((bottom >> 8));
		send_data((bottom & 0xFF));
		send_cmd(0x2C);
	}

	return 0;
}

static int32_t st7789v_invalidate(struct panel_spec *self)
{
	pr_debug("st7789v_invalidate\n");

	return self->ops->panel_set_window(self, 0, 0,
					   self->width - 1, self->height - 1);
}

static int32_t st7789v_invalidate_rect(struct panel_spec *self,
				       uint16_t left, uint16_t top,
				       uint16_t right, uint16_t bottom)
{
	pr_debug("st7789v_invalidate_rect : (%d, %d, %d, %d)\n", left, top,
		 right, bottom);

	return self->ops->panel_set_window(self, left, top, right, bottom);
}

static int32_t st7789v_set_direction(struct panel_spec *self,
				     uint16_t direction)
{
	pr_debug("st7789v_set_direction\n");
	return 0;
}

static int32_t st7789v_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;

	if (is_sleep) {
		send_cmd(0x10);
		msleep(120);
	} else
		msleep(120);

	return 0;
}

unsigned int panel_id = 0x51a0f0;
#define LCD_MTP_ID 0x51a0f0
static uint32_t st7789v_read_id(struct panel_spec *self)
{
	int32_t read_value = 0;

	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	read_data_t read_data = self->info.mcu->ops->read_data;

	send_cmd(0x04);

	if (self->info.mcu->bus_width == 8) {
		read_value += (read_data() & 0xff) << 16;
		read_value += read_data();
	} else {
		read_data();
		read_value += read_data() << 16;
		read_value += read_data() << 8;
		read_value += read_data();
	}
	pr_debug("st7789v_read_id=%x\n", read_value);

	if (read_value == LCD_MTP_ID)
		read_value = 0x7789;

	return read_value;
}
#ifdef CONFIG_FB_BL_EVENT_CTRL
static int32_t st7789v_backlight_ctrl(struct panel_spec *self, uint16_t bl_on)
{
	if (bl_on)
		fb_bl_notifier_call_chain(FB_BL_EVENT_RESUME, NULL);
	else
		fb_bl_notifier_call_chain(FB_BL_EVENT_SUSPEND, NULL);

	return 0;
}
#endif
static struct panel_operations lcd_st7789v_operations = {
	.panel_init = st7789v_init,
	.panel_set_window = st7789v_set_window,
	.panel_invalidate = st7789v_invalidate,
	.panel_set_direction = st7789v_set_direction,
	.panel_enter_sleep = st7789v_enter_sleep,
	.panel_readid = st7789v_read_id,
#ifdef CONFIG_FB_BL_EVENT_CTRL
	.panel_set_brightness = st7789v_backlight_ctrl,
#endif
};

static struct timing_mcu lcd_st7789v_timing[] = {
	[MCU_LCD_REGISTER_TIMING] = {
				     .rcss = 45,
				     .rlpw = 60,
				     .rhpw = 100,
				     .wcss = 10,
				     .wlpw = 28,
				     .whpw = 28,
				     },
	[MCU_LCD_GRAM_TIMING] = {
				 .rcss = 25,	/* 25 ns */
				 .rlpw = 70,
				 .rhpw = 70,
				 .wcss = 10,
				 .wlpw = 28,
				 .whpw = 35,
				 }
};

#if defined(CONFIG_LCD_18BIT_INTERFACE)
static struct info_mcu lcd_st7789v_info = {
	.bus_mode = LCD_BUS_8080,
	.bus_width = 18,
	.bpp = 18,
	.timing = &lcd_st7789v_timing,
	.ops = NULL,
};
#else
static struct info_mcu lcd_st7789v_info = {
	.bus_mode = LCD_BUS_8080,
	.bus_width = 8,
	.bpp = 16,
	.timing = &lcd_st7789v_timing,
	.ops = NULL,
};
#endif

struct panel_spec lcd_panel_st7789v = {
	.width = 240,
	.height = 320,
	.width_hw = 46,
	.height_hw = 65,
	.fps = 60,
	.suspend_mode = SEND_SLEEP_CMD,
	.is_clean_lcd = true,
	.type = LCD_MODE_MCU,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		 .mcu = &lcd_st7789v_info},
	.ops = &lcd_st7789v_operations,
};

struct panel_cfg lcd_st7789v = {
	/* this panel may on both CS0/1 */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x51a0f0,
	.lcd_name = "lcd_st7789v",
	.panel = &lcd_panel_st7789v,
};

static int __init lcd_st7789v_init(void)
{
	return sprdfb_panel_register(&lcd_st7789v);
}

subsys_initcall(lcd_st7789v_init);
