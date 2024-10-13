/* drivers/video/sprdfb/lcd_otm1283a_mipi.c
 *
 * Support for otm1283a mipi LCD device
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
//#define printk printf

#define  LCD_DEBUG
//#define THREE_LANE_SUPPORT

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

static LCM_Force_Cmd_Code rd_prep_code[]={
	{0x37, {LCM_SEND(2), {0x5, 0}}},
};

static LCM_Force_Cmd_Code rd_prep_code_1[]={
	{0x37, {LCM_SEND(2), {0x1, 0}}},
};

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

static LCM_Init_Code init_data[] = {
	{LCM_SEND(2),{0x00,0x00}},
	{LCM_SEND(6),{4,0,0xFF,0x12,0x83,0x01}},
	{LCM_SEND(2),{0x00,0x80}},

	{LCM_SEND(5),{3,0,0xFF,0x12,0x83}},

#ifdef THREE_LANE_SUPPORT
	{LCM_SEND(2),{0x00,0x92}},
	{LCM_SEND(5),{3,0,0xFF,0x20,0x02}},		//0x20:3lane   0x30:4lanes
#endif
	//---------------PANEL setting -----------//
	{LCM_SEND(2),{0x00,0x80}},
	{LCM_SEND(10),{8,0,0xC0,0x00,0x64,0x00,0x10,0x10,0x00,0x64}},
	{LCM_SEND(2),{0x00,0x87}},
	{LCM_SEND(5),{3,0,0xC0,0x10,0x10}},
	{LCM_SEND(2),{0x00,0x90}},
	{LCM_SEND(9),{7,0,0xC0,0x00,0x5C,0x00,0x01,0x00,0x04}},
	{LCM_SEND(2),{0x00,0xA4}},
	{LCM_SEND(2),{0xC0,0x22}},
	{LCM_SEND(2),{0x00,0xB3}},
	{LCM_SEND(5),{3,0,0xC0,0x00,0x50}},
	{LCM_SEND(2),{0x00,0xB4}},
	{LCM_SEND(2),{0xC0,0x55}},
	{LCM_SEND(2),{0x00,0x81}},
	//if(!TC358768_Gen_write_1A_1P(0xC1,0x55)) return -1;//xufei 2013-08-26 delete
	//or
	{LCM_SEND(2),{0xC1,0x66}},
	{LCM_SEND(2),{0x00,0x90}},
	{LCM_SEND(2),{0xC4,0x49}},
	{LCM_SEND(2),{0x00,0x00}},
	{LCM_SEND(2),{0x35,0x01}},
	{LCM_SEND(2),{0x00,0x00}},
	{LCM_SEND(2),{0x36,0x00}},
	//-----------------POWER SETTING-----------------
	{LCM_SEND(2),{0x00,0xA0}},
	{LCM_SEND(10),{8,0,0xC4,0x05,0x10,0x06,0x02,0x05,0x15,0x10}},
	{LCM_SEND(2),{0x00,0xA7}},
	{LCM_SEND(10),{8,0,0xC4,0x05,0x10,0x07,0x02,0x05,0x15,0x10}},
	{LCM_SEND(2),{0x00,0xB0}},
	{LCM_SEND(5),{3,0,0xC4,0x00,0x00}},
	{LCM_SEND(2),{0x00,0x91}},
	{LCM_SEND(5),{3,0,0xC5,0x46,0x40}},
	{LCM_SEND(2),{0x00,0x00}},
	{LCM_SEND(5),{3,0,0xD8,0xC7,0xC7}},
	{LCM_SEND(2),{0x00,0x00}},
	{LCM_SEND(2),{0xD9,0x6B}},
	{LCM_SEND(2),{0x00,0x81}},
	{LCM_SEND(2),{0xC4,0x82}},
	{LCM_SEND(2),{0x00,0xB0}},

	//if(!TC358768_Gen_write_1A_2P(0xC5,0x04,0xB8)) return -1;//xufei 2013-08-26 delete
	//or
	{LCM_SEND(5),{3,0,0xC5,0x04,0x58}},
	{LCM_SEND(2),{0x00,0xBB}},
	{LCM_SEND(2),{0xC5,0x80}},
	{LCM_SEND(2),{0x00,0x82}},
	{LCM_SEND(2),{0xC4,0x02}},
	{LCM_SEND(2),{0x00,0xC6}},
	{LCM_SEND(2),{0xB0,0x03}},
	{LCM_SEND(2),{0x00,0xB1}},
	{LCM_SEND(2),{0xC6,0x03}},
	//-----------------CONTROL SETTING---------------
	{LCM_SEND(2),{0x00,0x00}},
	{LCM_SEND(2),{0xD0,0x40}},
	{LCM_SEND(2),{0x00,0x00}},
	{LCM_SEND(5),{3,0,0xD1,0x00,0x00}},
	//-----------------PANEL TIMING STATE-------------------
	{LCM_SEND(2),{0x00,0x80}},
	{LCM_SEND(10),{8,0,0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0x87}},
	{LCM_SEND(6),{4,0,0xCB,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0x90}},
	{LCM_SEND(10),{8,0,0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0x97}},
	{LCM_SEND(10),{8,0,0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0x9E}},
	{LCM_SEND(2),{0xCB,0x00}},
	{LCM_SEND(2),{0x00,0xA0}},
	{LCM_SEND(10),{8,0,0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0xA7}},
	{LCM_SEND(10),{8,0,0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0xAE}},
	{LCM_SEND(2),{0xCB,0x00}},
	{LCM_SEND(2),{0x00,0xB0}},
	{LCM_SEND(10),{8,0,0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0xB7}},
	{LCM_SEND(10),{8,0,0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0xBE}},
	{LCM_SEND(2),{0xCB,0x00}},
	{LCM_SEND(2),{0x00,0xC0}},
	{LCM_SEND(10),{8,0,0xCB,0x05,0x05,0x05,0x05,0x05,0x05,0x00}},
	{LCM_SEND(2),{0x00,0xC7}},
	{LCM_SEND(10),{8,0,0xCB,0x00,0x00,0x00,0x05,0x05,0x00,0x05}},
	{LCM_SEND(2),{0x00,0xCE}},
	{LCM_SEND(2),{0xCB,0x05}},
	{LCM_SEND(2),{0x00,0xD0}},
	{LCM_SEND(10),{8,0,0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0xD7}},
	{LCM_SEND(10),{8,0,0xCB,0x05,0x05,0x05,0x05,0x05,0x05,0x00}},
	{LCM_SEND(2),{0x00,0xDE}},
	{LCM_SEND(2),{0xCB,0x00}},
	{LCM_SEND(2),{0x00,0xE0}},
	{LCM_SEND(10),{8,0,0xCB,0x00,0x00,0x05,0x05,0x00,0x05,0x05}},
	{LCM_SEND(2),{0x00,0xE7}},
	{LCM_SEND(10),{8,0,0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0xF0}},
	{LCM_SEND(10),{8,0,0xCB,0xff,0xff,0xff,0xff,0xff,0xff,0xff}},
	{LCM_SEND(2),{0x00,0xF7}},
	{LCM_SEND(6),{4,0,0xCB,0xff,0xff,0xff,0xff}},
	//--------------PANEL PAD MAPPING-------------
	{LCM_SEND(2),{0x00,0x80}},
	{LCM_SEND(10),{8,0,0xCC,0x0E,0x10,0x0A,0x0C,0x02,0x04,0x00}},
	{LCM_SEND(2),{0x00,0x87}},
	{LCM_SEND(10),{8,0,0xCC,0x00,0x00,0x00,0x2E,0x2D,0x00,0x29}},
	{LCM_SEND(2),{0x00,0x8E}},
	{LCM_SEND(2),{0xCC,0x2A}},
	{LCM_SEND(2),{0x00,0x90}},
	{LCM_SEND(10),{8,0,0xCC,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0x97}},
	{LCM_SEND(10),{8,0,0xCC,0x0D,0x0F,0x09,0x0B,0x01,0x03,0x00}},
	{LCM_SEND(2),{0x00,0x9E}},
	{LCM_SEND(2),{0xCC,0x00}},

	{LCM_SEND(2),{0x00,0xA0}},
	{LCM_SEND(10),{8,0,0xCC,0x00,0x00,0x2E,0x2D,0x00,0x29,0x2A}},
	{LCM_SEND(2),{0x00,0xA7}},
	{LCM_SEND(10),{8,0,0xCC,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0xB0}},
	{LCM_SEND(10),{8,0,0xCC,0x0B,0x09,0x0F,0x0D,0x03,0x01,0x00}},
	{LCM_SEND(2),{0x00,0xB7}},
	{LCM_SEND(10),{8,0,0xCC,0x00,0x00,0x00,0x2D,0x2E,0x00,0x29}},
	{LCM_SEND(2),{0x00,0xBE}},
	{LCM_SEND(2),{0xCC,0x2A}},
	{LCM_SEND(2),{0x00,0xC0}},
	{LCM_SEND(10),{8,0,0xCC,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0xC7}},
	{LCM_SEND(10),{8,0,0xCC,0x0C,0x0A,0x10,0x0E,0x04,0x02,0x00}},
	{LCM_SEND(2),{0x00,0xCE}},
	{LCM_SEND(2),{0xCC,0x00}},
	{LCM_SEND(2),{0x00,0xD0}},
	{LCM_SEND(10),{8,0,0xCC,0x00,0x00,0x2D,0x2E,0x00,0x29,0x2A}},
	{LCM_SEND(2),{0x00,0xD7}},
	{LCM_SEND(10),{8,0,0xCC,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	//-----------PANEL TIMING SETTING
	{LCM_SEND(2),{0x00,0x80}},
	{LCM_SEND(10),{8,0,0xCE,0x8B,0x03,0x18,0x8A,0x03,0x18,0x89}},
	{LCM_SEND(2),{0x00,0x87}},
	{LCM_SEND(8),{6,0,0xCE,0x03,0x18,0x88,0x03,0x18}},
	{LCM_SEND(2),{0x00,0x90}},
	{LCM_SEND(10),{8,0,0xCE,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0x97}},
	{LCM_SEND(10),{8,0,0xCE,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0xA0}},
	{LCM_SEND(10),{8,0,0xCE,0x38,0x07,0x05,0x00,0x00,0x48,0x00}},
	{LCM_SEND(2),{0x00,0xA7}},
	{LCM_SEND(10),{8,0,0xCE,0x38,0x06,0x05,0x01,0x00,0x48,0x00}},
	{LCM_SEND(2),{0x00,0xB0}},
	{LCM_SEND(10),{8,0,0xCE,0x38,0x05,0x05,0x02,0x00,0x48,0x00}},
	{LCM_SEND(2),{0x00,0xB7}},
	{LCM_SEND(10),{8,0,0xCE,0x38,0x04,0x05,0x03,0x00,0x48,0x00}},
	{LCM_SEND(2),{0x00,0xC0}},
	{LCM_SEND(10),{8,0,0xCE,0x38,0x03,0x05,0x04,0x00,0x48,0x00}},
	{LCM_SEND(2),{0x00,0xC7}},
	{LCM_SEND(10),{8,0,0xCE,0x38,0x02,0x05,0x05,0x00,0x48,0x00}},
	{LCM_SEND(2),{0x00,0xD0}},
	{LCM_SEND(10),{8,0,0xCE,0x38,0x01,0x05,0x06,0x00,0x48,0x00}},
	{LCM_SEND(2),{0x00,0xD7}},
	{LCM_SEND(10),{8,0,0xCE,0x38,0x00,0x05,0x07,0x00,0x48,0x00}},
	{LCM_SEND(2),{0x00,0x80}},
	{LCM_SEND(10),{8,0,0xCF,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0x87}},
	{LCM_SEND(10),{8,0,0xCF,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0x90}},
	{LCM_SEND(10),{8,0,0xCF,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0x97}},
	{LCM_SEND(10),{8,0,0xCF,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0xA0}},
	{LCM_SEND(10),{8,0,0xCF,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0xA7}},
	{LCM_SEND(10),{8,0,0xCF,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0xB0}},
	{LCM_SEND(10),{8,0,0xCF,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0xB7}},
	{LCM_SEND(10),{8,0,0xCF,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0x00,0xC0}},
	{LCM_SEND(10),{8,0,0xCF,0x02,0x01,0x20,0x20,0x00,0x00,0x01}},
	{LCM_SEND(2),{0x00,0xC7}},
	{LCM_SEND(6),{4,0,0xCF,0x81,0x00,0x03,0x08}},
	{LCM_SEND(2),{0x00,0xB5}},
	{LCM_SEND(9),{7,0,0xC5,0x00,0x6F,0xFF,0x00,0x6F,0xFF}},
	//-----------------FOR POWER IC--------------------------
	{LCM_SEND(2),{0x00,0x90}},
	{LCM_SEND(6),{4,0,0xF5,0x02,0x11,0x02,0x11}},
	{LCM_SEND(2),{0x00,0x90}},
	{LCM_SEND(2),{0xC5,0x50}},
	{LCM_SEND(2),{0x00,0x94}},
	{LCM_SEND(2),{0xC5,0x66}},
	//-----------------VGL01/02----------------------------------
	{LCM_SEND(2),{0x00,0xB2}},
	{LCM_SEND(5),{3,0,0xF5,0x00,0x00}},
	{LCM_SEND(2),{0x00,0xB4}},
	{LCM_SEND(5),{3,0,0xF5,0x00,0x00}},
	{LCM_SEND(2),{0x00,0xB6}},
	{LCM_SEND(5),{3,0,0xF5,0x00,0x00}},
	{LCM_SEND(2),{0x00,0xB8}},
	{LCM_SEND(5),{3,0,0xF5,0x00,0x00}},
	{LCM_SEND(2),{0x00,0x94}},
	{LCM_SEND(2),{0xF5,0x02}},
	{LCM_SEND(2),{0x00,0xBA}},
	{LCM_SEND(2),{0xF5,0x03}},
	{LCM_SEND(2),{0x00,0xB4}},
	{LCM_SEND(2),{0xC5,0xC0}},
	{LCM_SEND(2),{0x00,0xc6}},
	{LCM_SEND(2),{0xB0,0x03}},
	//spread frame range
	{LCM_SEND(2),{0x00,0xA0}},
	{LCM_SEND(2),{0xC1,0x02}},
	{LCM_SEND(2),{0x00,0xA3}},
	{LCM_SEND(2),{0xC1,0xF0}},
	//-----------------GAMMA------------------------------
	{LCM_SEND(2),{0x00,0x00}},
	{LCM_SEND(10),{8,0,0xE1,0x00,0x0d,0x14,0x0d,0x06,0x0e,0x0a}},
	{LCM_SEND(10),{8,0,0xE1,0x09,0x05,0x08,0x0f,0x08,0x0f,0x10}},
	{LCM_SEND(5),{3,0,0xE1,0x0a,0x05}},
	{LCM_SEND(2),{0x00,0x00}},
	{LCM_SEND(10),{8,0,0xE2,0x00,0x0d,0x14,0x0d,0x06,0x0e,0x0a}},
	{LCM_SEND(10),{8,0,0xE2,0x09,0x05,0x08,0x0f,0x08,0x0f,0x10}},
	{LCM_SEND(5),{3,0,0xE2,0x0a,0x05}},

	{LCM_SEND(1), {0x11}}, // sleep out
	{LCM_SLEEP(120)},
	{LCM_SEND(1), {0x29}}, // display on
	{LCM_SLEEP(50),}, //30, 100
	{LCM_SEND(1), {0x2C}}, // normal on
};

static int32_t otm1283a_mipi_init(struct panel_spec *self)
{
	int32_t i;
	LCM_Init_Code *init = init_data;
	unsigned int tag;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_gen_write_t mipi_gen_write = self->info.mipi->ops->mipi_gen_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	LCD_PRINT("otm1283a_mipi_init\n");

	mipi_set_cmd_mode();
	mipi_eotp_set(1,0);

	for(i = 0; i < ARRAY_SIZE(init_data); i++){
		tag = (init->tag >>24);
		if(tag & LCM_TAG_SEND){
			mipi_gen_write(init->data, (init->tag & LCM_TAG_MASK));
			udelay(20);
		}else if(tag & LCM_TAG_SLEEP){
			mdelay((init->tag & LCM_TAG_MASK));
		}
		init++;
	}
	mipi_eotp_set(1,1);
	return 0;
}

static uint32_t otm1283a_readid(struct panel_spec *self)
{
	int32_t i = 0;
	uint32_t j =0;
	LCM_Force_Cmd_Code * rd_prepare = rd_prep_code;
	uint8_t read_data[5] = {0};
	int32_t read_rtn = 0;
	unsigned int tag = 0;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_force_write_t mipi_force_write = self->info.mipi->ops->mipi_force_write;
	mipi_force_read_t mipi_force_read = self->info.mipi->ops->mipi_force_read;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	LCD_PRINT("lcd_otm1283a_mipi read id!\n");
	return 0x1283;

	mipi_set_cmd_mode();
	mipi_eotp_set(1,0);

	for(j = 0; j < 4; j++){
		rd_prepare = rd_prep_code;
		for(i = 0; i < ARRAY_SIZE(rd_prep_code); i++){
			tag = (rd_prepare->real_cmd_code.tag >> 24);
			if(tag & LCM_TAG_SEND){
				mipi_force_write(rd_prepare->datatype, rd_prepare->real_cmd_code.data, (rd_prepare->real_cmd_code.tag & LCM_TAG_MASK));
			}else if(tag & LCM_TAG_SLEEP){
				mdelay((rd_prepare->real_cmd_code.tag & LCM_TAG_MASK));
			}
			rd_prepare++;
		}

		read_rtn = mipi_force_read(0xa1, 5,(uint8_t *)read_data);
		LCD_PRINT("lcd_otm1283a_mipi read id 0xa1 value is 0x%x, 0x%x, 0x%x, 0x%x, 0x%x!\n",
			read_data[0], read_data[1], read_data[2], read_data[3], read_data[4]);

                mipi_eotp_set(1,1);

		if((0x01 == read_data[0])&&(0x8b == read_data[1])&&(0x12 == read_data[2])&&(0x83 == read_data[3])&&(0xff == read_data[4])){
			LCD_PRINT("lcd_otm1283a_mipi read id success!\n");
			return 0x1283;
		}
	}

	printk("lcd_otm1283a_mipi identify fail!\n");
	return 0;
}

static uint32_t otm1283a_readpowermode(struct panel_spec *self)
{
	int32_t i = 0;
	uint32_t j =0;
	LCM_Force_Cmd_Code * rd_prepare = rd_prep_code_1;
	uint8_t read_data[1] = {0};
	int32_t read_rtn = 0;
	unsigned int tag = 0;

	mipi_force_write_t mipi_force_write = self->info.mipi->ops->mipi_force_write;
	mipi_force_read_t mipi_force_read = self->info.mipi->ops->mipi_force_read;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	pr_debug("lcd_otm1283a_mipi read power mode!\n");
	mipi_eotp_set(0,1);
	for(j = 0; j < 4; j++){
		rd_prepare = rd_prep_code_1;
		for(i = 0; i < ARRAY_SIZE(rd_prep_code_1); i++){
			tag = (rd_prepare->real_cmd_code.tag >> 24);
			if(tag & LCM_TAG_SEND){
				mipi_force_write(rd_prepare->datatype, rd_prepare->real_cmd_code.data, (rd_prepare->real_cmd_code.tag & LCM_TAG_MASK));
			}else if(tag & LCM_TAG_SLEEP){
				msleep((rd_prepare->real_cmd_code.tag & LCM_TAG_MASK));
			}
			rd_prepare++;
		}
		read_rtn = mipi_force_read(0x0A, 1,(uint8_t *)read_data);
//		printk("lcd_otm1283a_mipi read power mode 0x0A value is 0x%x! , read result(%d)\n", read_data[0], read_rtn);
		if((0x9c == read_data[0])  && (0 == read_rtn)){
			pr_debug("lcd_otm1283a_mipi read power mode success!\n");
			mipi_eotp_set(1,1);
			return 0x9c;
		}
	}

	printk("lcd_otm1283a_mipi read power mode fail!0x0A value is 0x%x! , read result(%d)\n", read_data[0], read_rtn);
	mipi_eotp_set(1,1);
	return 0x0;
}

static int32_t otm1283a_check_esd(struct panel_spec *self)
{
	uint32_t power_mode;

	mipi_set_lp_mode_t mipi_set_data_lp_mode = self->info.mipi->ops->mipi_set_data_lp_mode;
	mipi_set_hs_mode_t mipi_set_data_hs_mode = self->info.mipi->ops->mipi_set_data_hs_mode;
	mipi_set_lp_mode_t mipi_set_lp_mode = self->info.mipi->ops->mipi_set_lp_mode;
	mipi_set_hs_mode_t mipi_set_hs_mode = self->info.mipi->ops->mipi_set_hs_mode;
	uint16_t work_mode = self->info.mipi->work_mode;

	pr_debug("otm1283a_check_esd!\n");
	if(SPRDFB_MIPI_MODE_CMD==work_mode){
		mipi_set_lp_mode();
	}else{
		mipi_set_data_lp_mode();
	}
	power_mode = otm1283a_readpowermode(self);
	//power_mode = 0x0;
	if(SPRDFB_MIPI_MODE_CMD==work_mode){
		mipi_set_hs_mode();
	}else{
		mipi_set_data_hs_mode();
	}
	if(power_mode == 0x9c){
		pr_debug("otm1283a_check_esd OK!\n");
		return 1;
	}else{
		printk("otm1283a_check_esd fail!(0x%x)\n", power_mode);
		return 0;
	}
}

static int32_t otm1283a_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	int32_t i;
	LCM_Init_Code *sleep_in_out = NULL;
	unsigned int tag;
	int32_t size = 0;

	mipi_gen_write_t mipi_gen_write = self->info.mipi->ops->mipi_gen_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	printk(KERN_DEBUG "otm1283a_enter_sleep, is_sleep = %d\n", is_sleep);

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
			msleep((sleep_in_out->tag & LCM_TAG_MASK));
		}
		sleep_in_out++;
	}
	mipi_eotp_set(1,1);

	return 0;
}

static struct panel_operations lcd_otm1283a_mipi_operations = {
	.panel_init = otm1283a_mipi_init,
	.panel_readid = otm1283a_readid,
	.panel_esd_check = otm1283a_check_esd,
	.panel_enter_sleep = otm1283a_enter_sleep,
};

static struct timing_rgb lcd_otm1283a_mipi_timing = {
	.hfp =32,
	.hbp = 4,
	.hsync =8,
	.vfp = 17,
	.vbp = 18,
	.vsync = 7,
};

static struct info_mipi lcd_otm1283a_mipi_info = {
	.work_mode				= SPRDFB_MIPI_MODE_VIDEO,
	.video_bus_width		= 24, /*18,16*/
#ifdef THREE_LANE_SUPPORT
	.lan_number 			= 3,
	.phy_feq				= 550*1000,
#else
	.lan_number 			= 4,
	.phy_feq				= 500*1000,
#endif
	.h_sync_pol				= SPRDFB_POLARITY_POS,
	.v_sync_pol				= SPRDFB_POLARITY_POS,
	.de_pol					= SPRDFB_POLARITY_POS,
	.te_pol					= SPRDFB_POLARITY_POS,
	.color_mode_pol			= SPRDFB_POLARITY_NEG,
	.shut_down_pol			= SPRDFB_POLARITY_NEG,
	.timing					= &lcd_otm1283a_mipi_timing,
	.ops					= NULL,
};

struct panel_spec lcd_otm1283a_mipi_spec = {
	.width					= 720,
	.height					= 1280,
	.fps					= 60,
	.reset_timing = {5,40,20},
	.type					= LCD_MODE_DSI,
	.direction				= LCD_DIRECT_NORMAL,
//	.suspend_mode = SEND_SLEEP_CMD,
	.info = {
		.mipi				= &lcd_otm1283a_mipi_info
	},
	.ops					= &lcd_otm1283a_mipi_operations,
};

struct panel_cfg lcd_otm1283a_mipi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x1283,
	.lcd_name = "lcd_otm1283a_mipi",
	.panel = &lcd_otm1283a_mipi_spec,
};

static int __init lcd_otm1283_mipi_init(void)
{
	return sprdfb_panel_register(&lcd_otm1283a_mipi);
}

subsys_initcall(lcd_otm1283_mipi_init);

