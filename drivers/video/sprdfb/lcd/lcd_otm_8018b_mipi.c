/* drivers/video/sc8825/lcd_otm8018b_mipi.c
 *
 * Support for otm8018b mipi LCD device
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

//#define LCD_Delay(ms)  uDelay(ms*1000)

#define MAX_DATA   48

typedef struct LCM_Init_Code_tag {
	unsigned int tag;
	unsigned char data[MAX_DATA];
}LCM_Init_Code;

#define LCM_TAG_SHIFT 24
#define LCM_TAG_MASK  ((1 << 24) -1)
#define LCM_SEND(len) ((1 << LCM_TAG_SHIFT)| len)
#define LCM_SLEEP(ms) ((2 << LCM_TAG_SHIFT)| ms)
//#define ARRAY_SIZE(array) ( sizeof(array) / sizeof(array[0]))

#define LCM_TAG_SEND  (1<< 0)
#define LCM_TAG_SLEEP (1 << 1)

static LCM_Init_Code init_data[] = {
#if 1 
{LCM_SEND(6), {4, 0, 0xff,0x80,0x09,0x01}}, 

{LCM_SEND(2), {0x00,0x80}}, 
{LCM_SEND(5), {3, 0, 0xff,0x80,0x09}},

{LCM_SEND(2), {0x00,0x03}}, 
{LCM_SEND(2), {0xFF,0x01}}, 

{LCM_SEND(2), {0x00,0xB4}}, 
{LCM_SEND(2), {0xC0,0x10}}, 

{LCM_SEND(2), {0x00,0x82}}, 
{LCM_SEND(2), {0xC5,0xA3}}, 

{LCM_SEND(2), {0x00,0x90}}, 
{LCM_SEND(5), {3, 0, 0xC5,0xC6,0x76}},

{LCM_SEND(2), {0x00,0x00}}, 
{LCM_SEND(5), {3, 0, 0xD8,0x75,0x73}},

{LCM_SEND(2), {0x00,0x00}}, 
{LCM_SEND(2), {0xD9,0x4E}}, 

/*-------------Gamma+ ------*/
{LCM_SEND(2), {0x00,0x00}},
{LCM_SEND(19), {17, 0, 0xE1,0x0C,0x0D,0x11,0x10,0x0A,0x1B,0x0B,0x0B,0x00,0x03,0x03,0x05,0x0D,0x24,0x21,0x05}}, 

{LCM_SEND(2), {0x00,0x00}},
{LCM_SEND(19), {17, 0, 0xE2,0x0C,0x0D,0x11,0x12,0x0E,0x1D,0x0B,0x0B,0x00,0x03,0x03,0x05,0x0D,0x25,0x21,0x05}}, 

{LCM_SEND(2), {0x00,0x81}}, 
{LCM_SEND(2), {0xC1,0x66}}, 
{LCM_SEND(2), {0x00,0xA1}}, 
{LCM_SEND(2), {0xC1,0x08}}, 

{LCM_SEND(2), {0x00,0x89}}, 
{LCM_SEND(2), {0xC4,0x08}}, 

{LCM_SEND(2), {0x00,0xA2}}, 
{LCM_SEND(6), {4, 0, 0xC0,0x1B,0x00,0x02}},

{LCM_SEND(2), {0x00,0x81}}, 
{LCM_SEND(2), {0xC4,0x83}}, 

{LCM_SEND(2), {0x00,0x92}}, 
{LCM_SEND(2), {0xC5,0x01}}, 

{LCM_SEND(2), {0x00,0xB1}}, 
{LCM_SEND(2), {0xC5,0xA9}}, 

/*C09x : mck_shift1/mck_shift2/mck_shift3*/
{LCM_SEND(2), {0x00,0x90}}, 
{LCM_SEND(9), {7, 0, 0xC0,0x00,0x44,0x00,0x00,0x00,0x03}},

/*C1Ax : hs_shift/vs_shift*/
{LCM_SEND(2), {0x00,0xA6}}, 
{LCM_SEND(6), {4, 0, 0xC1,0x00,0x00,0x00}},

//CE8x : vst1, vst2, vst3, vst4
{LCM_SEND(2), {0x00,0x80}}, 
{LCM_SEND(15), {13, 0, 0xCE,0x87,0x03,0x00,0x85,0x03,0x00,0x86,0x03,0x00,0x84,0x03,0x00}},

//CEAx : clka1, clka2
{LCM_SEND(2), {0x00,0xA0}},
{LCM_SEND(17), {15, 0, 0xCE,0x38,0x03,0x03,0x58,0x00,0x00,0x00,0x38,0x02,0x03,0x59,0x00,0x00,0x00}},

//CEBx : clka3, clka4
{LCM_SEND(2), {0x00,0xB0}},
{LCM_SEND(17), {15, 0, 0xCE,0x38,0x01,0x03,0x5A,0x00,0x00,0x00,0x38,0x00,0x03,0x5B,0x00,0x00,0x00}},

//CECx : clkb1, clkb2
{LCM_SEND(2), {0x00,0xC0}},
{LCM_SEND(17), {15, 0, 0xCE,0x30,0x00,0x03,0x5C,0x00,0x00,0x00,0x30,0x01,0x03,0x5D,0x00,0x00,0x00}},

//CEDx : clkb3, clkb4
{LCM_SEND(2), {0x00,0xD0}},
{LCM_SEND(17), {15, 0, 0xCE,0x30,0x02,0x03,0x5E,0x00,0x00,0x00,0x30,0x03,0x03,0x5F,0x00,0x00,0x00}},

{LCM_SEND(2), {0x00, 0xC7}}, 
{LCM_SEND(2), {0xCF, 0x00}}, 

{LCM_SEND(2), {0x00, 0xC9}}, 
{LCM_SEND(2), {0xCF, 0x00}}, 

{LCM_SEND(2), {0x00, 0xC4}}, 
{LCM_SEND(9), {7, 0, 0xCB,0x04,0x04,0x04,0x04,0x04,0x04}},

{LCM_SEND(2), {0x00, 0xD9}}, 
{LCM_SEND(9), {7, 0, 0xCB,0x04,0x04,0x04,0x04,0x04,0x04}},

{LCM_SEND(2), {0x00, 0x84}}, 
{LCM_SEND(9), {7, 0, 0xCC,0x0C,0x0A,0x10,0x0E,0x03,0x04}},

{LCM_SEND(2), {0x00,0x9E}}, 
{LCM_SEND(2), {0xCC,0x0B}}, 

{LCM_SEND(2), {0x00,0xA0}}, 
{LCM_SEND(8), {6, 0, 0xCC,0x09,0x0F,0x0D,0x01,0x02}},

{LCM_SEND(2), {0x00,0xB4}}, 
{LCM_SEND(9), {7, 0, 0xCC,0x0D,0x0F,0x09,0x0B,0x02,0x01}},

{LCM_SEND(2), {0x00,0xCE}}, 
{LCM_SEND(2), {0xCC,0x0E}}, 

{LCM_SEND(2), {0x00,0xD0}}, 
{LCM_SEND(8), {6, 0, 0xCC,0x10,0x0A,0x0C,0x04,0x03}},

{LCM_SEND(2), {0x00,0x00}}, 
{LCM_SEND(2), {0x3A,0x77}}, 

{LCM_SEND(2), {0x11, 0x00}}, // sleep out 
{LCM_SLEEP(200),},
{LCM_SEND(2), {0x29, 0x00}}, // display on 

{LCM_SEND(1), {0x2C}}, // normal on 

#endif

};

//static LCM_Init_Code disp_on =  {LCM_SEND(1), {0x29}};

static LCM_Init_Code sleep_in[] =  {
{LCM_SEND(1), {0x28}},
{LCM_SLEEP(10)},
{LCM_SEND(1), {0x10}},
{LCM_SLEEP(120)},
};

static LCM_Init_Code sleep_out[] =  {
{LCM_SEND(1), {0x11}},
{LCM_SLEEP(120)},
{LCM_SEND(1), {0x29}},
{LCM_SLEEP(20)},
};

static int32_t otm8018b_mipi_init(struct panel_spec *self)
{
	int32_t i = 0;
	LCM_Init_Code *init = init_data;
	unsigned int tag;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_gen_write_t mipi_gen_write = self->info.mipi->ops->mipi_gen_write;

	pr_debug(KERN_DEBUG "otm8018b_mipi_init\n");

	mipi_set_cmd_mode();

	for(i = 0; i < ARRAY_SIZE(init_data); i++){
		tag = (init->tag >>24);
		if(tag & LCM_TAG_SEND){
			mipi_gen_write(init->data, (init->tag & LCM_TAG_MASK));
			udelay(10);
		}else if(tag & LCM_TAG_SLEEP){
			msleep((init->tag & LCM_TAG_MASK));
		}
		init++;
	}
	return 0;
}

static uint32_t otm8018b_readid(struct panel_spec *self)
{
	/*Jessica TODO: need read id*/
	return 0x18;
}

static int32_t otm8018b_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	int32_t i = 0;
	LCM_Init_Code *sleep_in_out = NULL;
	unsigned int tag;
	int32_t size = 0;

	mipi_dcs_write_t mipi_dcs_write = self->info.mipi->ops->mipi_dcs_write;
	mipi_gen_write_t mipi_gen_write = self->info.mipi->ops->mipi_gen_write;

	printk(KERN_DEBUG "otm8018b_enter_sleep, is_sleep = %d\n", is_sleep);

	if(is_sleep){
		sleep_in_out = sleep_in;
		size = ARRAY_SIZE(sleep_in);
	}else{
		sleep_in_out = sleep_out;
		size = ARRAY_SIZE(sleep_out);
	}

	for(i = 0; i <size ; i++){
		tag = (sleep_in_out->tag >>24);
		if(tag & LCM_TAG_SEND){
			mipi_gen_write(sleep_in_out->data, (sleep_in_out->tag & LCM_TAG_MASK));
		}else if(tag & LCM_TAG_SLEEP){
			msleep((sleep_in_out->tag & LCM_TAG_MASK));
		}
		sleep_in_out++;
	}
	return 0;
}

static struct panel_operations lcd_otm8018b_mipi_operations = {
	.panel_init = otm8018b_mipi_init,
	.panel_readid = otm8018b_readid,
	.panel_enter_sleep = otm8018b_enter_sleep,
};

static struct timing_rgb lcd_otm8018b_mipi_timing = {
	.hfp = 80,  /* unit: pixel */
	.hbp = 80,
	.hsync = 6,
	.vfp = 10, /*unit: line*/
	.vbp = 10,
	.vsync = 6,
};

static struct info_mipi lcd_otm8018b_mipi_info = {
	.work_mode  = SPRDFB_MIPI_MODE_VIDEO,
	.video_bus_width = 24, /*18,16*/
	.lan_number = 2,
	.phy_feq = 500*1000,
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.te_pol = SPRDFB_POLARITY_POS,
	.color_mode_pol = SPRDFB_POLARITY_POS,
	.shut_down_pol = SPRDFB_POLARITY_POS,
	.timing = &lcd_otm8018b_mipi_timing,
	.ops = NULL,
};

struct panel_spec lcd_otm8018b_mipi_spec = {
	.width = 480,
	.height = 800,
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		.mipi = &lcd_otm8018b_mipi_info
	},
	.ops = &lcd_otm8018b_mipi_operations,
};

struct panel_cfg lcd_otm8018b_mipi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x18,
	.lcd_name = "lcd_otm8018b_mipi",
	.panel = &lcd_otm8018b_mipi_spec,
};

static int __init lcd_otm8018b_mipi_init(void)
{
	return sprdfb_panel_register(&lcd_otm8018b_mipi);
}

subsys_initcall(lcd_otm8018b_mipi_init);
