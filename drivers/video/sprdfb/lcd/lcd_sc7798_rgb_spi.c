/* drivers/video/sc8810/lcd_hx8363_rgb_spi.c
 *
 *
 *
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




#define sc7798_SpiWriteCmd(cmd) \
{ \
	spi_send_cmd((cmd & 0xFF));\
}

#define  sc7798_SpiWriteData(data)\
{ \
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
	spi_send_cmd_t spi_send_cmd = self->info.rgb->bus_info.spi->ops->spi_send_cmd; 
	spi_send_data_t spi_send_data = self->info.rgb->bus_info.spi->ops->spi_send_data; 
	spi_read_t spi_read = self->info.rgb->bus_info.spi->ops->spi_read; 


	printk("sc7798_init\n");

	sc7798_SpiWriteCmd(0xB9);
	sc7798_SpiWriteData(0xF1);
	sc7798_SpiWriteData(0x08);
	sc7798_SpiWriteData(0x00);

	sc7798_SpiWriteCmd(0xBC); 
	sc7798_SpiWriteData(0x67);

	sc7798_SpiWriteCmd(0x3A); 
	sc7798_SpiWriteData(0x70);

	sc7798_SpiWriteCmd(0xB2); 
	sc7798_SpiWriteData(0x23);

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

#if 0
	sc7798_SpiWriteCmd(0xC1); 
	sc7798_SpiWriteData(0x01);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x03);
	sc7798_SpiWriteData(0x0B);
	sc7798_SpiWriteData(0x17);
	sc7798_SpiWriteData(0x20);
	sc7798_SpiWriteData(0x26); 
	sc7798_SpiWriteData(0x30);
	sc7798_SpiWriteData(0x36);
	sc7798_SpiWriteData(0x3F);
	sc7798_SpiWriteData(0x48);
	sc7798_SpiWriteData(0x50);
	sc7798_SpiWriteData(0x58);
	sc7798_SpiWriteData(0x5F);
	sc7798_SpiWriteData(0x68);
	sc7798_SpiWriteData(0x71);
	sc7798_SpiWriteData(0x79);
	sc7798_SpiWriteData(0x82);
	sc7798_SpiWriteData(0x89);
	sc7798_SpiWriteData(0x91);
	sc7798_SpiWriteData(0x99);
	sc7798_SpiWriteData(0xA1);
	sc7798_SpiWriteData(0xA9);
	sc7798_SpiWriteData(0xB0);
	sc7798_SpiWriteData(0xB8);
	sc7798_SpiWriteData(0xC0);
	sc7798_SpiWriteData(0xC8);
	sc7798_SpiWriteData(0xD0);
	sc7798_SpiWriteData(0xD6);
	sc7798_SpiWriteData(0xDC);
	sc7798_SpiWriteData(0xE4);
	sc7798_SpiWriteData(0xED);
	sc7798_SpiWriteData(0xF7);
	sc7798_SpiWriteData(0xFF);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0xC0);
	sc7798_SpiWriteData(0x0A);
	sc7798_SpiWriteData(0x80);
	sc7798_SpiWriteData(0x30);
	sc7798_SpiWriteData(0xE8);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x03);
	sc7798_SpiWriteData(0x0B);
	sc7798_SpiWriteData(0x17);
	sc7798_SpiWriteData(0x20);
	sc7798_SpiWriteData(0x27);
	sc7798_SpiWriteData(0x30);
	sc7798_SpiWriteData(0x37);
	sc7798_SpiWriteData(0x40);
	sc7798_SpiWriteData(0x48);
	sc7798_SpiWriteData(0x50);
	sc7798_SpiWriteData(0x58);
	sc7798_SpiWriteData(0x5F);
	sc7798_SpiWriteData(0x67);
	sc7798_SpiWriteData(0x70);
	sc7798_SpiWriteData(0x78);
	sc7798_SpiWriteData(0x80);
	sc7798_SpiWriteData(0x88);
	sc7798_SpiWriteData(0x90);
	sc7798_SpiWriteData(0x98);
	sc7798_SpiWriteData(0xA0);
	sc7798_SpiWriteData(0xA8);
	sc7798_SpiWriteData(0xAF);
	sc7798_SpiWriteData(0xB7);
	sc7798_SpiWriteData(0xBF);
	sc7798_SpiWriteData(0xC7);
	sc7798_SpiWriteData(0xCF);
	sc7798_SpiWriteData(0xD5);
	sc7798_SpiWriteData(0xDC);
	sc7798_SpiWriteData(0xE4);
	sc7798_SpiWriteData(0xED);
	sc7798_SpiWriteData(0xF7);
	sc7798_SpiWriteData(0xFF);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0xC0);
	sc7798_SpiWriteData(0x0A);
	sc7798_SpiWriteData(0x80);
	sc7798_SpiWriteData(0x30);
	sc7798_SpiWriteData(0xE8);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x03);
	sc7798_SpiWriteData(0x0D);
	sc7798_SpiWriteData(0x19);
	sc7798_SpiWriteData(0x23);
	sc7798_SpiWriteData(0x29);
	sc7798_SpiWriteData(0x31);
	sc7798_SpiWriteData(0x38);
	sc7798_SpiWriteData(0x41);
	sc7798_SpiWriteData(0x48);
	sc7798_SpiWriteData(0x4F);
	sc7798_SpiWriteData(0x57);
	sc7798_SpiWriteData(0x5D);
	sc7798_SpiWriteData(0x65);
	sc7798_SpiWriteData(0x6C);
	sc7798_SpiWriteData(0x74);
	sc7798_SpiWriteData(0x7B);
	sc7798_SpiWriteData(0x84);
	sc7798_SpiWriteData(0x8B);
	sc7798_SpiWriteData(0x94);
	sc7798_SpiWriteData(0x9B);
	sc7798_SpiWriteData(0xA4);
	sc7798_SpiWriteData(0xAA);
	sc7798_SpiWriteData(0xB4);
	sc7798_SpiWriteData(0xB9);
	sc7798_SpiWriteData(0xC3);
	sc7798_SpiWriteData(0xCC);
	sc7798_SpiWriteData(0xD6);
	sc7798_SpiWriteData(0xDC);
	sc7798_SpiWriteData(0xE4);
	sc7798_SpiWriteData(0xED);
	sc7798_SpiWriteData(0xF7);
	sc7798_SpiWriteData(0xFF);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0x00);
	sc7798_SpiWriteData(0xC0);
	sc7798_SpiWriteData(0x0A);
	sc7798_SpiWriteData(0x80);
	sc7798_SpiWriteData(0x30);
	sc7798_SpiWriteData(0xE8);
	sc7798_SpiWriteData(0x00);
#endif

	sc7798_SpiWriteCmd(0x11);
	mdelay(120);
	sc7798_SpiWriteCmd(0x29);
}


#if 1
static int32_t sc7798_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	spi_send_cmd_t spi_send_cmd = self->info.rgb->bus_info.spi->ops->spi_send_cmd; 
	spi_send_data_t spi_send_data = self->info.rgb->bus_info.spi->ops->spi_send_data; 
	
	if(is_sleep==1){
		//Sleep In
		sc7798_SpiWriteCmd(0x28);
		mdelay(120); 
		sc7798_SpiWriteCmd(0x10);
		mdelay(10); 
	}else{
		//Sleep Out
		sc7798_SpiWriteCmd(0x11);
		mdelay(120); 
		sc7798_SpiWriteCmd(0x29);
		mdelay(10); 
	}

	return 0;
}




static int32_t sc7798_set_window(struct panel_spec *self,
		uint16_t left, uint16_t top, uint16_t right, uint16_t bottom)
{
	uint32_t *test_data[4] = {0};
	spi_send_cmd_t spi_send_cmd = self->info.rgb->bus_info.spi->ops->spi_send_cmd; 
	spi_send_data_t spi_send_data = self->info.rgb->bus_info.spi->ops->spi_send_data; 
	spi_read_t spi_read = self->info.rgb->bus_info.spi->ops->spi_read;
#if 0

	printk("zxdbg add -hx8363_set_window: %d, %d, %d, %d\n",left, top, right, bottom);

	sc7798_SpiWriteCmd(0x2A00); 
	sc7798_SpiWriteData((left>>8));// set left address
	sc7798_SpiWriteData((left&0xff));
	sc7798_SpiWriteData((right>>8));// set right address
	sc7798_SpiWriteData((right&0xff));

	sc7798_SpiWriteCmd(0x2B00); 
	sc7798_SpiWriteData((top>>8));// set left address
	sc7798_SpiWriteData((top&0xff));
	sc7798_SpiWriteData((bottom>>8));// set bottom address
	sc7798_SpiWriteData((bottom&0xff));

//	HX8363_SpiWriteCmd(0x2C00);

	sc7798_SpiWriteCmd(0x2A00); 
	spi_read(test_data);
	spi_read(test_data+1);
	sc7798_SpiWriteCmd(0x2B00); 
	spi_read(test_data+2);
	spi_read(test_data+3);
#endif
//	printk("zxdbg add -hx8363_read read: %x, %x, %x, %x\n",test_data[0], test_data[1], test_data[2], test_data[3]);

	return 0;
}

static int32_t sc7798_invalidate(struct panel_spec *self)
{
//	printk("hx8363_invalidate\n");

	return self->ops->panel_set_window(self, 0, 0,
		self->width - 1, self->height - 1);
}



static int32_t sc7798_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom)
{
//	printk("hx8363_invalidate_rect \n");

	return self->ops->panel_set_window(self, left, top,
			right, bottom);
}


#endif


static int32_t sc7798_read_id(struct panel_spec *self)
{
#if 0
	int32_t id  = 0x62;
	spi_send_cmd_t spi_send_cmd = self->info.rgb->bus_info.spi->ops->spi_send_cmd;
	spi_send_data_t spi_send_data = self->info.rgb->bus_info.spi->ops->spi_send_data;
	spi_read_t spi_read = self->info.rgb->bus_info.spi->ops->spi_read;
/*
	printk("sprdfb hx8363_read_id \n");
	HX8363_SpiWriteCmd(0xB9); // SET password
	HX8363_SpiWriteData(0xFF); //
	HX8363_SpiWriteData(0x83); //
	HX8363_SpiWriteData(0x63); //

	HX8363_SpiWriteCmd(0xFE); // SET SPI READ INDEX
	HX8363_SpiWriteData(0xF4); // GETHXID
	HX8363_SpiWriteCmd(0xFF); // GET SPI READ

	spi_read(&id);
*/
	id &= 0xff;
	printk(" hx8363_read_id kernel id = %x\n",id);
//	return id;
#endif
	return 0x7798;
}

static struct panel_operations lcd_sc7798_rgb_spi_operations = {
	.panel_init = sc7798_init,
	.panel_set_window = sc7798_set_window,
	.panel_invalidate_rect= sc7798_invalidate_rect,
	.panel_invalidate = sc7798_invalidate,
	.panel_enter_sleep = sc7798_enter_sleep,
	.panel_readid = sc7798_read_id
};

static struct timing_rgb lcd_sc7798_rgb_timing = {
	.hfp = 90,
	.hbp = 70,
	.hsync = 60,
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
	.fps = 60,
	.type = LCD_MODE_RGB,
	.direction = LCD_DIRECT_NORMAL,
	.is_clean_lcd = true,
	.info = {
		.rgb = &lcd_sc7798_rgb_info
	},
	.ops = &lcd_sc7798_rgb_spi_operations,
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
	return sprdfb_panel_register(&lcd_sc7798_rgb_spi_spec);

}

subsys_initcall(lcd_sc7798_rgb_spi_init);
