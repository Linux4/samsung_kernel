/* drivers/video/sprdfb/lcd/lcd_t8861_mipi.c
 *
 * Support for t8861 mipi LCD device
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
#include <linux/bug.h>
#include <linux/delay.h>
#include "../sprdfb_panel.h"

//#define  LCD_DEBUG
#ifdef LCD_DEBUG
#define LCD_PRINT printk
#else
#define LCD_PRINT(...)
#endif

#define MAX_DATA   48

typedef struct LCM_Init_Code_tag {
	unsigned int tag;
	unsigned char data[MAX_DATA];
}LCM_Init_Code;

typedef struct LCM_force_cmd_code_tag{
	unsigned int datatype;
	LCM_Init_Code real_cmd_code;
}LCM_Force_Cmd_Code;

#define LCM_TAG_SHIFT 24
#define LCM_TAG_MASK  ((1 << 24) -1)
#define LCM_SEND(len) ((1 << LCM_TAG_SHIFT)| len)
#define LCM_SLEEP(ms) ((2 << LCM_TAG_SHIFT)| ms)
//#define ARRAY_SIZE(array) ( sizeof(array) / sizeof(array[0]))

#define LCM_TAG_SEND  (1<< 0)
#define LCM_TAG_SLEEP (1 << 1)

static LCM_Init_Code init_data[] = {

	{LCM_SEND(8), {6, 0,0xFF,0xFF,0x98,0x06,0x04,0x01}},
	{LCM_SLEEP(5)},

	{LCM_SEND(2), {0x08,0x10}},

	{LCM_SEND(2), {0x21,0x01}},

	{LCM_SEND(2), {0x30,0x02}},
	{LCM_SEND(2), {0x31,0x00}}, //0x02

	{LCM_SEND(2), {0x60,0x07}},
	{LCM_SEND(2), {0x61,0x06}},
	{LCM_SEND(2), {0x62,0x06}},
	{LCM_SEND(2), {0x63,0x04}},

	{LCM_SEND(2), {0x40,0x14}},
	{LCM_SEND(2), {0x41,0x44}},//22
	{LCM_SEND(2), {0x42,0x01}},
	{LCM_SEND(2), {0x43,0x89}},
	{LCM_SEND(2), {0x44,0x89}},
	{LCM_SEND(2), {0x45,0x1b}},
	{LCM_SEND(2), {0x46,0x44}},
	{LCM_SEND(2), {0x47,0x44}},

	{LCM_SEND(2), {0x50,0x85}},
	{LCM_SEND(2), {0x51,0x85}},
	{LCM_SEND(2), {0x52,0x00}},
	{LCM_SEND(2), {0x53,0x64}},//90
	
	//{LCM_SEND(8), {6, 0,0xFF,0xFF,0x98,0x06,0x04,0x01}},

	{LCM_SEND(2), {0xA0,0x00}},
	{LCM_SEND(2), {0xA1,0x00}},//01
	{LCM_SEND(2), {0xA2,0x03}},//08
	{LCM_SEND(2), {0xA3,0x0e}},//0e
	{LCM_SEND(2), {0xA4,0x08}},//08
	{LCM_SEND(2), {0xA5,0x1f}},//19
	{LCM_SEND(2), {0xA6,0x0f}},//0b
	{LCM_SEND(2), {0xA7,0x0b}},//0a
	{LCM_SEND(2), {0xA8,0x03}},//02
	{LCM_SEND(2), {0xA9,0x06}},//08
	{LCM_SEND(2), {0xAA,0x05}},//04
	{LCM_SEND(2), {0xAB,0x02}},//03
	{LCM_SEND(2), {0xAC,0x0e}},//0a
	{LCM_SEND(2), {0xAD,0x25}},//2c
	{LCM_SEND(2), {0xAE,0x1d}},//28
	{LCM_SEND(2), {0xAF,0x00}},

	{LCM_SEND(2), {0xC0,0x00}},//00
	{LCM_SEND(2), {0xC1,0x04}},//02
	{LCM_SEND(2), {0xC2,0x0f}},//09
	{LCM_SEND(2), {0xC3,0x10}},//0e
	{LCM_SEND(2), {0xC4,0x0b}},//09
	{LCM_SEND(2), {0xC5,0x1e}},//19
	{LCM_SEND(2), {0xC6,0x09}},//0a
	{LCM_SEND(2), {0xC7,0x0a}},//0a
	{LCM_SEND(2), {0xC8,0x00}},//02
	{LCM_SEND(2), {0xC9,0x0a}},//07
	{LCM_SEND(2), {0xCA,0x01}},//05
	{LCM_SEND(2), {0xCB,0x06}},//04
	{LCM_SEND(2), {0xCC,0x09}},//0a
	{LCM_SEND(2), {0xCD,0x2a}},//2d
	{LCM_SEND(2), {0xCE,0x28}},//28
	{LCM_SEND(2), {0xCF,0x00}},//00
	
	{LCM_SEND(8), {6, 0,0xFF,0xFF,0x98,0x06,0x04,0x06}},
	{LCM_SEND(2), {0x00,0xa0}},
	{LCM_SEND(2), {0x01,0x05}},
	{LCM_SEND(2), {0x02,0x00}},
	{LCM_SEND(2), {0x03,0x00}},
	{LCM_SEND(2), {0x04,0x01}},
	{LCM_SEND(2), {0x05,0x01}},
	{LCM_SEND(2), {0x06,0x88}},
	{LCM_SEND(2), {0x07,0x04}},
	{LCM_SEND(2), {0x08,0x01}},
	{LCM_SEND(2), {0x09,0x90}},
	{LCM_SEND(2), {0x0A,0x04}},
	{LCM_SEND(2), {0x0B,0x01}},
	{LCM_SEND(2), {0x0C,0x01}},
	{LCM_SEND(2), {0x0D,0x01}},
	{LCM_SEND(2), {0x0E,0x00}},
	{LCM_SEND(2), {0x0F,0x00}},

	{LCM_SEND(2), {0x10,0x55}},
	{LCM_SEND(2), {0x11,0x50}},
	{LCM_SEND(2), {0x12,0x01}},
	{LCM_SEND(2), {0x13,0x85}},
	{LCM_SEND(2), {0x14,0x85}},
	{LCM_SEND(2), {0x15,0xc0}},
	{LCM_SEND(2), {0x16,0x0B}},
	{LCM_SEND(2), {0x17,0x00}},
	{LCM_SEND(2), {0x18,0x00}},
	{LCM_SEND(2), {0x19,0x00}},
	{LCM_SEND(2), {0x1A,0x00}},
	{LCM_SEND(2), {0x1B,0x00}},
	{LCM_SEND(2), {0x1C,0x00}},
	{LCM_SEND(2), {0x1D,0x00}},

	{LCM_SEND(2), {0x20,0x01}},
	{LCM_SEND(2), {0x21,0x23}},
	{LCM_SEND(2), {0x22,0x45}},
	{LCM_SEND(2), {0x23,0x67}},
	{LCM_SEND(2), {0x24,0x01}},
	{LCM_SEND(2), {0x25,0x23}},
	{LCM_SEND(2), {0x26,0x45}},
	{LCM_SEND(2), {0x27,0x67}},

	{LCM_SEND(2), {0x30,0x02}},
	{LCM_SEND(2), {0x31,0x22}},
	{LCM_SEND(2), {0x32,0x11}},
	{LCM_SEND(2), {0x33,0xaa}},
	{LCM_SEND(2), {0x34,0xbb}},
	{LCM_SEND(2), {0x35,0x66}},
	{LCM_SEND(2), {0x36,0x00}},
	{LCM_SEND(2), {0x37,0x22}},
	{LCM_SEND(2), {0x38,0x22}},
	{LCM_SEND(2), {0x39,0x22}},
	{LCM_SEND(2), {0x3A,0x22}},
	{LCM_SEND(2), {0x3B,0x22}},
	{LCM_SEND(2), {0x3C,0x22}},
	{LCM_SEND(2), {0x3D,0x22}},
	{LCM_SEND(2), {0x3E,0x22}},
	{LCM_SEND(2), {0x3F,0x22}},

	{LCM_SEND(2), {0x40,0x22}},
	{LCM_SEND(2), {0x53,0x1a}},
	
	{LCM_SEND(8), {6, 0,0xFF,0xFF,0x98,0x06,0x04,0x07}},
	{LCM_SEND(2), {0x18,0x1d}},
	{LCM_SEND(2), {0x17,0x12}},

	{LCM_SEND(2), {0x02,0x77}},
	
	{LCM_SEND(2), {0xe1,0x79}},
	
	{LCM_SEND(2), {0x06,0x13}},

	{LCM_SEND(8), {6, 0,0xFF,0xFF,0x98,0x06,0x04,0x00}},
	
//	{LCM_SEND(2), {0x35,0x00}},

	{LCM_SEND(1), {0x11}}, 	
	{LCM_SLEEP(120)},

	{LCM_SEND(1), {0x29}},
	{LCM_SLEEP(1)},
	
//	{LCM_SEND(8), {6, 0,0xFF,0xFF,0x98,0x06,0x04,0x08}},
	
};

static LCM_Init_Code disp_on =  {LCM_SEND(1), {0x29}};

static LCM_Init_Code sleep_in[] =  {
	{LCM_SEND(8), {6, 0,0xFF,0xFF,0x98,0x06,0x04,0x00}},
	{LCM_SLEEP(1)},

	
	{LCM_SEND(8), {6, 0,0xFF,0xFF,0x98,0x06,0x04,0x00}},
	{LCM_SLEEP(1)},

{LCM_SEND(1), {0x28}},
{LCM_SLEEP(120)},
{LCM_SEND(1), {0x10}},
{LCM_SLEEP(10)},
		{LCM_SEND(8), {6, 0,0xFF,0xFF,0x98,0x06,0x04,0x01}},
		{LCM_SLEEP(1)},
		
	{LCM_SEND(2), {0x58,0x91}},

};

static LCM_Init_Code sleep_out[] =  {
{LCM_SEND(1), {0x11}},
{LCM_SLEEP(120)},
{LCM_SEND(1), {0x29}},
{LCM_SLEEP(20)},
};

static LCM_Force_Cmd_Code rd_prep_code[]={
	{0x39, {LCM_SEND(8), {0x6, 0, 0xFF, 0xFF, 0x98, 0x06,0x04,0x01}}},
	{0x37, {LCM_SEND(2), {0x3, 0}}},
};

static LCM_Force_Cmd_Code rd_prep_code_1[]={
	{0x37, {LCM_SEND(2), {0x1, 0}}},
};
static int32_t t8861_mipi_init(struct panel_spec *self)
{
	int32_t i;
	LCM_Init_Code *init = init_data;
	unsigned int tag;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_gen_write_t mipi_gen_write = self->info.mipi->ops->mipi_gen_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	pr_debug(KERN_DEBUG "t8861_mipi_init\n");

	msleep(2);
	mipi_set_cmd_mode();
	mipi_eotp_set(1,0);

	for(i = 0; i < ARRAY_SIZE(init_data); i++){
		tag = (init->tag >>24);
		if(tag & LCM_TAG_SEND){
			mipi_gen_write(init->data, (init->tag & LCM_TAG_MASK));
			udelay(20);
		}else if(tag & LCM_TAG_SLEEP){
			//udelay((init->tag & LCM_TAG_MASK) * 1000);
				msleep(init->tag & LCM_TAG_MASK);
		}
		init++;
	}
	mipi_eotp_set(1,1);

	return 0;
}

static uint32_t t8861_readid(struct panel_spec *self)
{
	/*Jessica TODO: need read id*/
	int32_t i = 0;
	uint32_t j =0;
	LCM_Force_Cmd_Code * rd_prepare = rd_prep_code;
	uint8_t read_data[3] = {0};
	int32_t read_rtn = 0;
	unsigned int tag = 0;
	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_force_write_t mipi_force_write = self->info.mipi->ops->mipi_force_write;
	mipi_force_read_t mipi_force_read = self->info.mipi->ops->mipi_force_read;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;
  
	printk("lcd_t8861_mipi read id!\n");

	mipi_set_cmd_mode();
	mipi_eotp_set(1,0);

	for(j = 0; j < 4; j++){
		rd_prepare = rd_prep_code;
		for(i = 0; i < ARRAY_SIZE(rd_prep_code); i++){
			tag = (rd_prepare->real_cmd_code.tag >> 24);
			if(tag & LCM_TAG_SEND){
				mipi_force_write(rd_prepare->datatype, rd_prepare->real_cmd_code.data, (rd_prepare->real_cmd_code.tag & LCM_TAG_MASK));
			}else if(tag & LCM_TAG_SLEEP){
				udelay((rd_prepare->real_cmd_code.tag & LCM_TAG_MASK) * 1000);
			}
			rd_prepare++;
		}
		read_rtn = mipi_force_read(0x02, 3,(uint8_t *)read_data);
		printk("lcd_t8861_mipi read id 0xc5 value is 0x%x, 0x%x, 0x%x!\n", read_data[0], read_data[1], read_data[2]);

		if((0x04 == read_data[0])){
			printk("lcd_t8861_mipi read id success!\n");
			mipi_eotp_set(1,1);
			return 0x04;
		}
	}
	mipi_eotp_set(1,1);
	return 0x0;
}

static int32_t t8861_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	int32_t i;
	LCM_Init_Code *sleep_in_out = NULL;
	unsigned int tag;
	int32_t size = 0;

	mipi_gen_write_t mipi_gen_write = self->info.mipi->ops->mipi_gen_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	printk(KERN_DEBUG "t8861_enter_sleep, is_sleep = %d\n", is_sleep);

	if(is_sleep){
		sleep_in_out = sleep_in;
		size = ARRAY_SIZE(sleep_in);
	}else{
		sleep_in_out = sleep_out;
		size = ARRAY_SIZE(sleep_out);
	}
	mipi_eotp_set(1,0);

	for(i = 0; i <size ; i++){
		tag = (sleep_in_out->tag >>24);
		if(tag & LCM_TAG_SEND){
			mipi_gen_write(sleep_in_out->data, (sleep_in_out->tag & LCM_TAG_MASK));
		}else if(tag & LCM_TAG_SLEEP){
			//udelay((sleep_in_out->tag & LCM_TAG_MASK) * 1000);
			msleep((sleep_in_out->tag & LCM_TAG_MASK));
		}
		sleep_in_out++;
	}
	return 0;
}

static uint32_t t8861_readpowermode(struct panel_spec *self)
{
	int32_t i = 0;
	uint32_t j =0;
	LCM_Force_Cmd_Code * rd_prepare = rd_prep_code_1;
	uint8_t read_data[1] = {0};
	int32_t read_rtn = 0;
	unsigned int tag = 0;
	uint32_t reg_val_1 = 0;
	uint32_t reg_val_2 = 0;

	mipi_force_write_t mipi_force_write = self->info.mipi->ops->mipi_force_write;
	mipi_force_read_t mipi_force_read = self->info.mipi->ops->mipi_force_read;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	pr_debug("lcd_t8861_mipi read power mode!\n");
	mipi_eotp_set(0,1);

	for(j = 0; j < 4; j++){
		rd_prepare = rd_prep_code_1;
		for(i = 0; i < ARRAY_SIZE(rd_prep_code_1); i++){
			tag = (rd_prepare->real_cmd_code.tag >> 24);
			if(tag & LCM_TAG_SEND){
				mipi_force_write(rd_prepare->datatype, rd_prepare->real_cmd_code.data, (rd_prepare->real_cmd_code.tag & LCM_TAG_MASK));
			}else if(tag & LCM_TAG_SLEEP){
				udelay((rd_prepare->real_cmd_code.tag & LCM_TAG_MASK) * 1000);
			}
			rd_prepare++;
		}

		read_rtn = mipi_force_read(0x0A, 1,(uint8_t *)read_data);
		//printk("lcd_t8861 mipi read power mode 0x0A value is 0x%x! , read result(%d)\n", read_data[0], read_rtn);

		if((0x9c == read_data[0])  && (0 == read_rtn)){
			pr_debug("lcd_t8861_mipi read power mode success!\n");
			mipi_eotp_set(1,1);
			return 0x9c;
		}
	}

	mipi_eotp_set(1,1);
	return 0x0;
}


static uint32_t t8861_check_esd(struct panel_spec *self)
{
	uint32_t power_mode;

	pr_debug("t8861_check_esd!\n");
	mipi_set_lp_mode_t mipi_set_data_lp_mode = self->info.mipi->ops->mipi_set_data_lp_mode;
	mipi_set_hs_mode_t mipi_set_data_hs_mode = self->info.mipi->ops->mipi_set_data_hs_mode;

	mipi_set_lp_mode_t mipi_set_lp_mode = self->info.mipi->ops->mipi_set_lp_mode;
	mipi_set_hs_mode_t mipi_set_hs_mode = self->info.mipi->ops->mipi_set_hs_mode;
	uint16_t work_mode = self->info.mipi->work_mode;

	if(SPRDFB_MIPI_MODE_CMD==work_mode){
		mipi_set_lp_mode();
	}else{
		mipi_set_data_lp_mode();
	}
	power_mode = t8861_readpowermode(self);
	//power_mode = 0x0;
	if(SPRDFB_MIPI_MODE_CMD==work_mode){
		mipi_set_hs_mode();
	}else{
		mipi_set_data_hs_mode();
	}
	if(power_mode == 0x9c){
		pr_debug("t8861_check_esd OK!\n");
		return 1;
	}else{
		printk("t8861_check_esd fail!(0x%x)\n", power_mode);
		return 0;
	}
}


static struct panel_operations lcd_t8861_mipi_operations = {
	.panel_init = t8861_mipi_init,
	.panel_readid = t8861_readid,
	.panel_enter_sleep = t8861_enter_sleep,
	.panel_esd_check = t8861_check_esd,
};

static struct timing_rgb lcd_t8861_mipi_timing = {
	.hfp = 60,  /* unit: pixel */// 100
	.hbp = 80,//80
	.hsync = 60,//6
	.vfp = 20, /*unit: line*/
	.vbp = 14,
	.vsync =6, //6,
};

static struct info_mipi lcd_t8861_mipi_info = {
	.work_mode  = SPRDFB_MIPI_MODE_VIDEO,
	.video_bus_width = 24, /*18,16*/
	.lan_number = 2,
	.phy_feq = 500*1000, //500->10MHz,400->8M in lp mode
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.te_pol = SPRDFB_POLARITY_POS,
	.color_mode_pol = SPRDFB_POLARITY_NEG,
	.shut_down_pol = SPRDFB_POLARITY_NEG,
	.timing = &lcd_t8861_mipi_timing,
	.ops = NULL,
};

struct panel_spec lcd_t8861_mipi_spec = {
	//.cap = PANEL_CAP_NOT_TEAR_SYNC,
	.width = 480,
	.height = 800,
	.fps = 60,
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
	.is_clean_lcd = true,
	.info = {
		.mipi = &lcd_t8861_mipi_info
	},
	.ops = &lcd_t8861_mipi_operations,
};

struct panel_cfg lcd_t8861_mipi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x04,
	.lcd_name = "lcd_t8861_mipi",
	.panel = &lcd_t8861_mipi_spec,
};

static int __init lcd_t8861_mipi_init(void)
{
	return sprdfb_panel_register(&lcd_t8861_mipi);
}

subsys_initcall(lcd_t8861_mipi_init);
