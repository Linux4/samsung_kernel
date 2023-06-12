/* drivers/video/sc8810/lcd_ili9341.c
 *
 * Support for ili9341 LCD device
 *
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
#ifdef CONFIG_LCD_ESD_RECOVERY
#include "../esd_detect.h"
#endif

#define BOE_PANEL	0x61BCD1
#define TIANMA_PANEL	0x61A4D0

unsigned int panel_id = 0;

static int32_t boe_panel_init(struct panel_spec *self)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_data_t send_data = self->info.mcu->ops->send_data;

	printk("boe_panel_init\n");

	/*Power Setting*/
	send_cmd(0xCF);
	send_data(0x18);

	send_cmd(0xC1);
	send_data(0x10);

	send_cmd(0xC5);
	send_data(0x2D);
	send_data(0x4D);

	send_cmd(0xC7);
	send_data(0x00);

	send_cmd(0xB1);
	send_data(0x00);
	send_data(0x1A);

	send_cmd(0xBC);
	send_data(0x39);
	send_data(0x2c);
	send_data(0x00);
	send_data(0x34);
	send_data(0x02);

	msleep(50);

	/*Display parameter setting*/
	send_cmd(0xB4); /*02H*/
	send_data(0x02);

	send_cmd(0xB6); /*0AH  A2H 27H*/
	send_data(0x0A);
	send_data(0xA2);
	send_data(0x27);

	send_cmd(0xB7); /*06H*/
	send_data(0x06);

	send_cmd(0xF6); /*01H  30H 00H */
	send_data(0x01);
	send_data(0x30);
	send_data(0x00);

	send_cmd(0x3A); /*55H*/
	send_data(0x55);

	send_cmd(0x36); /*08H*/
	send_data(0xD8);

	send_cmd(0x35); /*00H*/
	send_data(0x00);

	send_cmd(0xCF); /*00H  FBH FFH */
	send_data(0x00);
	send_data(0xFB);
	send_data(0xFF);

	send_cmd(0xED); /*64H  03H 12H  81*/
	send_data(0x64);
	send_data(0x03);
	send_data(0x12);
	send_data(0x81);

	send_cmd(0xE8); /*85H  00H 60H */
	send_data(0x85);
	send_data(0x00);
	send_data(0x60);

	send_cmd(0xEA); /*00H  00H*/
	send_data(0x00);
	send_data(0x00);

	send_cmd(0x2A); /*00H  00H 00H  EF*/
	send_data(0x00);
	send_data(0x00);
	send_data(0x00);
	send_data(0xEF);

	send_cmd(0x2B); /*00H  00H 01H  3F*/
	send_data(0x00);
	send_data(0x00);
	send_data(0x01);
	send_data(0x3F);

	send_cmd(0xF2); /*02H*/
	send_data(0x02);

	msleep(50);

	/*Gamma Setting*/
	send_cmd(0xE0);
	send_data(0x0F);
	send_data(0x11);
	send_data(0x10);
	send_data(0x09);
	send_data(0x0f);
	send_data(0x06);
	send_data(0x41);
	send_data(0x84);
	send_data(0x32);
	send_data(0x06);
	send_data(0x0D);
	send_data(0x04);
	send_data(0x0E);
	send_data(0x10);
	send_data(0x0C);

	send_cmd(0xE1);
	send_data(0x00);
	send_data(0x10);
	send_data(0x14);
	send_data(0x02);
	send_data(0x10);
	send_data(0x04);
	send_data(0x2C);
	send_data(0x22);
	send_data(0x3f);
	send_data(0x04);
	send_data(0x10);
	send_data(0x0A);
	send_data(0x28);
	send_data(0x2E);
	send_data(0x03);
#if 1
	send_cmd(0x11);	/* (SLPOUT)*/
	msleep(150);	/* 100ms*/

	send_cmd(0x2C); /* (Memory write) and show pattern*/

	send_cmd(0x29); /* (DISPON)*/
	msleep(200);	/* 100ms*/

	printk("boe_panel_init: end\n");

	return 0;
#else
	if (1) { /*  for test the lcd*/
		int i;
		send_cmd(0x2C); /*Write data*/
		for (i = 0; i < 240*320/3; i++)
			send_data(0xff);
		for (i = 0; i < 240*320/3; i++)
			send_data(0xff00);
		for (i = 0; i < 24*320/3; i++)
			send_data(0xff0000);
	}
	send_cmd(0x11); /*Display On*/
	mdelay(10);	/*120ms*/
	send_cmd(0x29); /*Write data*/
	mdelay(10);	/*120ms*/
#endif
}

static int32_t tianma_panel_init(struct panel_spec *self)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_data_t send_data = self->info.mcu->ops->send_data;

	printk("tianma_panel_init\n");
		send_cmd(0x11); /* (SLPOUT)*/
		msleep(120); /* 100ms*/

		/*Power Setting*/
		send_cmd(0xCF);
		send_data(0x00);
		send_data(0xE1);
		send_data(0x30);

		send_cmd(0xED);
		send_data(0x64);
		send_data(0x03);
		send_data(0x12);
		send_data(0x81);

		send_cmd(0xCB);
		send_data(0x39);
		send_data(0x2C);
		send_data(0x00);
		send_data(0x34);
		send_data(0x02);

		send_cmd(0xF7);
		send_data(0x20);

		send_cmd(0xEA);
		send_data(0x00);
		send_data(0x00);

		send_cmd(0xC5);
		send_data(0x37);
		send_data(0x47);

		send_cmd(0xC7);
		send_data(0x00);

		send_cmd(0xC0);
		send_data(0x27);

		send_cmd(0xC1);
		send_data(0x11);

		/*Initializing Sequence*/
		send_cmd(0x35);
		send_data(0x00);

		send_cmd(0x36);
		send_data(0xD8);

		send_cmd(0x3A);
		send_data(0x05);

		send_cmd(0xF2);
		send_data(0x03);

		send_cmd(0xB6);
		send_data(0x01);
		send_data(0xA2);

		send_cmd(0xF6);
		send_data(0x01);
		send_data(0x30);
		send_data(0x00);

		send_cmd(0xB1);
		send_data(0x00);
		send_data(0x1A);

		send_cmd(0xB5);
		send_data(0x02);
		send_data(0x02);
		send_data(0x0A);
		send_data(0x14);

		send_cmd(0xE8);
		send_data(0x85);
		send_data(0x01);
		send_data(0x78);

		/*Gamma Setting*/
		send_cmd(0xE0);
		send_data(0x0F);
		send_data(0x1E);
		send_data(0x1A);
		send_data(0x0D);
		send_data(0x10);
		send_data(0x06);
		send_data(0x4E);
		send_data(0x66);
		send_data(0x41);
		send_data(0x07);
		send_data(0x11);
		send_data(0x04);
		send_data(0x1C);
		send_data(0x13);
		send_data(0x08);

		send_cmd(0xE1);
		send_data(0x00);
		send_data(0x20);
		send_data(0x22);
		send_data(0x05);
		send_data(0x10);
		send_data(0x04);
		send_data(0x36);
		send_data(0x24);
		send_data(0x47);
		send_data(0x03);
		send_data(0x0E);
		send_data(0x0B);
		send_data(0x30);
		send_data(0x31);
		send_data(0x09);

		send_cmd(0xE2);
		send_data(0x00);
		send_data(0x00);
		send_data(0x00);
		send_data(0x00);
		send_data(0x00);
		send_data(0x00);
		send_data(0x00);
		send_data(0x00);
		send_data(0x00);
		send_data(0x00);
		send_data(0x00);
		send_data(0x00);
		send_data(0x00);
		send_data(0x00);
		send_data(0x00);
		send_data(0x00);

		send_cmd(0xE3);
		send_data(0x00);
		send_data(0x00);
		send_data(0x00);
		send_data(0x00);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x11);
		send_data(0x11);
		send_data(0x11);
		send_data(0x22);
		send_data(0x22);

		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);

		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);

		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x33);
		send_data(0x03);
		send_data(0x03);
		send_data(0x03);
		send_data(0x03);
		send_data(0x03);
		send_data(0x03);
		send_data(0x03);
		send_data(0x03);
		send_data(0x03);
		send_data(0x03);

		send_data(0x03);
		send_data(0x02);
		send_data(0x01);
		send_data(0x00);
#if 1
		/*Display on*/
		send_cmd(0x29); /* (DISPON)*/
		usleep_range(10000,15000);
		pr_debug("tianma_panel_init: end\n");

		return 0;
#else
		if (1) { /*for test the lcd*/
			int i;
			send_cmd(0x2C); /*Write data*/
			for (i = 0; i < 240*320/3; i++)
				send_data(0xff);
			for (i = 0; i < 240*320/3; i++)
				send_data(0xff00);
			for (i = 0; i < 24*320/3; i++)
				send_data(0xff0000);
		}
		send_cmd(0x11);	/*Display On*/
		mdelay(10);	/*120ms*/
		send_cmd(0x29); /*Write data*/
		mdelay(10);	/*120ms*/
#endif

}

static int32_t ili9341_init(struct panel_spec *self)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_data_t send_data = self->info.mcu->ops->send_data;

	printk("ili9341_init\n");
	if (BOE_PANEL == panel_id )
		boe_panel_init(self);
	else if (TIANMA_PANEL == panel_id )
		tianma_panel_init(self);
	else {
		printk("No correct panel detected:by default set to BOE \n");
		boe_panel_init(self);
	}
	printk("ili9341_init: end\n");

	return 0;
}

static int32_t ili9341_set_window(struct panel_spec *self,
		uint16_t left, uint16_t top, uint16_t right, uint16_t bottom)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_data_t send_data = self->info.mcu->ops->send_data;

	/*printk("ili9341_set_window\n");*/

	send_cmd(0x2A); /* col*/
	send_data((left >> 8));
	send_data((left & 0xFF));
	send_data((right >> 8));
	send_data((right & 0xFF));

	send_cmd(0x2B); /* row*/
	send_data((top >> 8));
	send_data((top & 0xFF));
	send_data((bottom >> 8));
	send_data((bottom & 0xFF));

	send_cmd(0x2C);

	return 0;
}


static int32_t ili9341_invalidate(struct panel_spec *self)
{
	/*printk("ili9341_invalidate\n");*/

	return self->ops->panel_set_window(self, 0, 0,
			self->width-1, self->height-1);
}

static int32_t ili9341_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom)
{

	printk("ili9341_invalidate_rect : (%d, %d, %d, %d)\n", \
						left, top, right, bottom);

	return self->ops->panel_set_window(self, left, top, right, bottom);
}

static int32_t ili9341_set_direction(struct panel_spec *self, uint16_t direction)
{

	printk("ili9341_set_direction\n");
	return 0;
}

static int32_t ili9341_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;

	if (is_sleep) {
		send_cmd(0x10);
		msleep(120);
	} else {
		/*send_cmd(0x11);*/
		msleep(120);
	}
	return 0;
}

static uint32_t ili9341_read_id(struct panel_spec *self)
{
	int32_t read_value = 0,temp = 0;
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	read_data_t read_data = self->info.mcu->ops->read_data;

	send_cmd(0x04);

	 /*read_data();*/
	 read_data();
	 read_value += read_data()<< 16;
	 read_value += read_data()<< 8;
	 read_value += read_data();

	printk("<0> ili9341_read_id=%x\n", read_value);

	panel_id = read_value;

	if (BOE_PANEL == panel_id )
		printk("sprdfb: panel is BOE with ID:%d \n", panel_id);
	else if (TIANMA_PANEL == panel_id )
		printk("sprdfb: panel is TIANMA with ID:%d \n", panel_id);
	else {
		panel_id = TIANMA_PANEL;
		printk("No correct panel detected:by default set to TIANMA \n");
	}

	return panel_id;
}

#ifdef CONFIG_LCD_ESD_RECOVERY
struct esd_det_info ili9341_esd_info = {
	.name = "ili9341",
	.mode = ESD_DET_INTERRUPT,
	.gpio = 111,
	.state = ESD_DET_NOT_INITIALIZED,
	.level = ESD_DET_LOW,
};
#endif

static int32_t ili9341_backlight_ctrl(struct panel_spec *self, uint16_t bl_on)
{
	if (bl_on)
		fb_bl_notifier_call_chain(FB_BL_EVENT_RESUME, NULL);
	else
		fb_bl_notifier_call_chain(FB_BL_EVENT_SUSPEND, NULL);

	return 0;
}

static struct panel_operations lcd_ili9341_operations = {
	.panel_init		= ili9341_init,
	.panel_set_window	= ili9341_set_window,
	.panel_invalidate	= ili9341_invalidate,
	/*.lcd_invalidate_rect = ili9341_invalidate_rect,*/
	.panel_set_direction	= ili9341_set_direction,
	.panel_enter_sleep	= ili9341_enter_sleep,
	.panel_readid		= ili9341_read_id,
	.panel_set_brightness	= ili9341_backlight_ctrl,
};

static struct timing_mcu lcd_ili9341_timing[] = {
    [0] = {
        .rcss = 10,
        .rlpw = 80,   /*  rlpw + rhpw = Trc >= 160*/
        .rhpw = 120,
        .wcss = 10,
        .wlpw = 32,   /*  wlpw + whpw = Twc >= 66*/
        .whpw = 36,
    },
    [1] = {
        .rcss = 25,
        .rlpw = 45,
        .rhpw = 90,
        .wcss = 30,
        .wlpw = 50,
        .whpw = 17,
    }
};
static struct info_mcu lcd_ili9341_info = {
	.bus_mode = LCD_BUS_8080,
	.bus_width = 16,
	.bpp = 16, /*RGB88*/
	.timing = &lcd_ili9341_timing,
	.ops = NULL,
};

struct panel_spec lcd_panel_ili9341 = {
	.width	= 240,
	.height	= 320,
	.width_hw = 46,
	.height_hw = 61,
	.fps	= 60,
	.suspend_mode = SEND_SLEEP_CMD,
	.type = LCD_MODE_MCU,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
                 .mcu = &lcd_ili9341_info
                },
	.ops = &lcd_ili9341_operations,
#ifdef CONFIG_LCD_ESD_RECOVERY
	.esd_info = &ili9341_esd_info,
#endif
};

struct panel_cfg lcd_tianma = {
	/* this panel may on both CS0/1 */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x61A4D0,
	.lcd_name = "lcd_tianma",
	.panel = &lcd_panel_ili9341,
};

struct panel_cfg lcd_boe = {
	/* this panel may on both CS0/1 */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x61BCD1,
	.lcd_name = "lcd_boe",
	.panel = &lcd_panel_ili9341,
};

static int __init lcd_ili9341_init(void)
{
	printk(KERN_INFO "sprdfb_111: [%s]:  0x%x!\n", \
						__FUNCTION__,lcd_tianma.lcd_id);
	return sprdfb_panel_register(&lcd_tianma);
}

subsys_initcall(lcd_ili9341_init);

