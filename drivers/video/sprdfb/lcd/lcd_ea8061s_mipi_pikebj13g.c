/* drivers/video/sprdfb/lcd_hx8379C_mipi.c
 *
 * Support for hx8379C mipi LCD device
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
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include "../sprdfb_panel.h"
#include <mach/gpio.h>



#define  LCD_DEBUG
#ifdef LCD_DEBUG
#define LCD_PRINT printk
#else
#define LCD_PRINT(...)
#endif

#define MAX_DATA   150
#define NEW_PANAL_BOE
#define LCD_NEW_PANEL

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

#if defined(CONFIG_MACH_PIKEB_J1_3G)
static int is_gpio_request2=0;
#endif

/* Dimming data*/
#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		100
#define GAMMA_CMD_CNT	36
#define DIMMING_COUNT	62
#define MTP_ADDR	0xC8
#define MTP_LEN	0x21
#define ELVSS_ADDR	0xB6
#define LDI_GAMMA	0xCA



static LCM_Init_Code init_data[] = {

	{LCM_SEND(5),{3,0,0xf0,0x5a,0x5a}},
	{LCM_SEND(4),{2,0,0xba,0x74}},
	{LCM_SEND(36),{34,0,0xca,0x01,0x00,0x01,0x00,0x01,0x00,\
					  0x80,0x80,0x80,\
					  0x80,0x80,0x80,\
					  0x80,0x80,0x80,\
					  0x80,0x80,0x80,\
					  0x80,0x80,0x80,\
					  0x80,0x80,0x80,\
					  0x80,0x80,0x80,\
					  0x80,0x80,0x80,\
					  0x00,0x00,0x00}},
	{LCM_SEND(7),{5,0,0xb2,0x00,0x00,0x00,0x0a}},
	{LCM_SEND(5),{3,0,0xb6,0xcb,0x94}},
	{LCM_SEND(4),{2,0,0xf7,0x03}},
	/*acl set*/
	{LCM_SEND(2),{0x55,0x02}},
	{LCM_SEND(5),{3,0,0xf0,0xa5,0xa5}},

	{LCM_SEND(1),{0x11}},
	{LCM_SLEEP(120)},
	{LCM_SEND(1),{0x29}},

{LCM_SEND(1), {0x11}},//0x11
{LCM_SLEEP(120)},
{LCM_SEND(1), {0x29}},//0x29
{LCM_SLEEP(100)},
};


static LCM_Init_Code disp_on =  {LCM_SEND(1), {0x29}};

static LCM_Init_Code sleep_in[] =  {
	{LCM_SEND(1), {0x28}},
	{LCM_SLEEP(150)}, 	//>150ms
	{LCM_SEND(1), {0x10}},
	{LCM_SLEEP(150)},	//>150ms
};

static LCM_Init_Code sleep_out[] =  {
	{LCM_SEND(1), {0x11}},
	{LCM_SLEEP(120)},//>120ms
	{LCM_SEND(1), {0x29}},
	{LCM_SLEEP(20)}, //>20ms
};

static LCM_Init_Code test_key_on[] =  {
	{LCM_SEND(5),		{3, 0x00, 0xF0, 0x5A, 0x5A} },
};

static LCM_Init_Code test_key_off[] =  {
	{LCM_SEND(5),		{3, 0x00, 0xF0, 0xA5, 0xA5} },
};


static LCM_Init_Code gamma_update[] =  {
	{LCM_SEND(2),		{0xf7, 0x03} },
};

static LCM_Init_Code aid_cmd[] = {

#if 1//spec                                              
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0xb7}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0xb0}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0xa8}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0xa1}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0x9a}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0x95}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0x8e}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0x88}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0x81}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0x7a}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0x76}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0x71}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0x6e}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0x67}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0x5f}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0x50}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0x48}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0x3f}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0x36}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0x24}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0x1a}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x03, 0x06}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x02, 0xf4}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x02, 0xea}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x02, 0xd9}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x02, 0xc8}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x02, 0xb0}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x02, 0x9f}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x02, 0x8f}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x02, 0x76}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x02, 0x5d}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x02, 0x45}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x02, 0x2b}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x02, 0x12}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x01, 0xf0}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x01, 0xce}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x01, 0xaf}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x01, 0x92}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x01, 0x66}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x01, 0x3e}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x01, 0x16}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x00, 0xe3}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x00, 0xb5}},
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x00, 0x7b }},                 
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x00, 0x47 }},      
{LCM_SEND(7),	{5, 0x00, 0xB2 , 0x40 , 0x08 ,0x00, 0x0a }},
                                                       
#endif 


};

static const unsigned int br_convert[DIMMING_COUNT] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
	11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 44, 46, 48, 50, 52, 54, 56, 58,
	60, 63, 66, 69, 73, 77, 81, 85, 89, 93,
	97, 100
};

static int32_t ea8061s_mipi_init(struct panel_spec *self)
{
	int32_t i = 0;
	LCM_Init_Code *init = init_data;
	unsigned int tag;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_dcs_write_t mipi_dcs_write = self->info.mipi->ops->mipi_dcs_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	printk("kernel ea8061s_mipi_init\n");

	mipi_set_cmd_mode();
	mipi_eotp_set(0,0);

	for(i = 0; i < ARRAY_SIZE(init_data); i++){
		tag = (init->tag >>24);
		if(tag & LCM_TAG_SEND){
			mipi_dcs_write(init->data, (init->tag & LCM_TAG_MASK));
			udelay(20);
		}else if(tag & LCM_TAG_SLEEP){
			msleep(init->tag & LCM_TAG_MASK);
		}
		init++;
	}
	mipi_eotp_set(0,0);

	return 0;
}

static uint32_t ea8061s_readid(struct panel_spec *self)
{
#if 0
	int32_t j =0;
	uint8_t read_data[3] = {0};
	int32_t read_rtn = 0;
	uint8_t param[2] = {0};
	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_force_write_t mipi_force_write = self->info.mipi->ops->mipi_force_write;
	mipi_force_read_t mipi_force_read = self->info.mipi->ops->mipi_force_read;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	LCD_PRINT("kernel lcd_ea8061s_mipi read id!\n");

	mipi_set_cmd_mode();
	mipi_eotp_set(0,0);

	for(j = 0; j < 4; j++){
		param[0] = 0x01;
		param[1] = 0x00;
		mipi_force_write(0x37, param, 2);
		read_rtn = mipi_force_read(0xda,1,&read_data[0]);
		read_rtn = mipi_force_read(0xdb,1,&read_data[1]);
		read_rtn = mipi_force_read(0xdc,1,&read_data[2]);

		if((0x6 == read_data[1])&&(0x80 == read_data[2])){

			printk("lcd_ea8061s_mipi read id success!\n");
			return 0x8379;
		}
	}
	mipi_eotp_set(0,0);
	LCD_PRINT("lcd_ea8061s_mipi read id fail! 0xda,0xdb,0xdc is 0x%x,0x%x,0x%x!\n",read_data[0],read_data[1],read_data[2]);
#endif
	return 0xea8061;
}


static int32_t ea8061s_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	int32_t i = 0;
	LCM_Init_Code *sleep_in_out = NULL;
	unsigned int tag;
	int32_t size = 0;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_dcs_write_t mipi_dcs_write = self->info.mipi->ops->mipi_dcs_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	printk("kernel ea8061s_enter_sleep, is_sleep = %d\n", is_sleep);

	if(is_sleep){
		sleep_in_out = sleep_in;
		size = ARRAY_SIZE(sleep_in);
	}else{
		sleep_in_out = sleep_out;
		size = ARRAY_SIZE(sleep_out);
	}

	mipi_set_cmd_mode();
	mipi_eotp_set(0,0);

	for(i = 0; i <size ; i++){
		tag = (sleep_in_out->tag >>24);
		if(tag & LCM_TAG_SEND){
			mipi_dcs_write(sleep_in_out->data, (sleep_in_out->tag & LCM_TAG_MASK));
		}else if(tag & LCM_TAG_SLEEP){
			msleep(sleep_in_out->tag & LCM_TAG_MASK);
		}
		sleep_in_out++;
	}
	mipi_eotp_set(0,0);

	return 0;
}
static void s6e8aa5x01_test_key(struct panel_spec *self, bool enable)
{
	LCM_Init_Code *test_key = NULL;
	unsigned int tag;
	int32_t size = 0;
	uint8_t i = 0;
	mipi_dcs_write_t mipi_dcs_write =
			self->info.mipi->ops->mipi_dcs_write;

	if (enable) {
		test_key = test_key_on;
		size = ARRAY_SIZE(test_key_on);
	} else {
		test_key = test_key_off;
		size = ARRAY_SIZE(test_key_off);
	}

	for (i = 0; i < size; i++) {
		tag = (test_key->tag >> 24);
		if (tag & LCM_TAG_SEND)
			mipi_dcs_write(test_key->data,
				(test_key->tag & LCM_TAG_MASK));
		else if (tag & LCM_TAG_SLEEP)
			msleep(test_key->tag & LCM_TAG_MASK);
		test_key++;
	}
}

int32_t s6e8aa5x01_set_brightnees(struct panel_spec *self,
						uint16_t brightness_value)
{

#if 1
	int32_t i = 0;
	int32_t size = 0;
	int level = 0;
	unsigned int tag;
	LCM_Init_Code *update = NULL;
	LCM_Init_Code *aid;
	mipi_dcs_write_t mipi_dcs_write =
			self->info.mipi->ops->mipi_dcs_write;
	mipi_eotp_set_t mipi_eotp_set =
			self->info.mipi->ops->mipi_eotp_set;

	uint16_t brightness= brightness_value * (sizeof(aid_cmd)/sizeof(LCM_Init_Code)) /255;
	
	if (brightness < MIN_BRIGHTNESS ||
		brightness > MAX_BRIGHTNESS) {
		pr_err("lcd brightness should be %d to %d.\n",
			MIN_BRIGHTNESS, MAX_BRIGHTNESS);
		return -EINVAL;
	}

	level = self->br_map[brightness];
	if(level>sizeof(aid_cmd)/sizeof(LCM_Init_Code))level=sizeof(aid_cmd)/sizeof(LCM_Init_Code);
	mipi_eotp_set(0, 0);

	s6e8aa5x01_test_key(self, true);

	aid = &aid_cmd[level];
	//mipi_dcs_write(self->gamma_tbl[level], (LCM_SEND(36) & LCM_TAG_MASK));
	mipi_dcs_write(aid->data, (aid->tag & LCM_TAG_MASK));
	update = gamma_update;

	size = ARRAY_SIZE(gamma_update);
	
	for (i = 0; i < size; i++) {
		tag = (update->tag >> 24);
		if (tag & LCM_TAG_SEND) {
			mipi_dcs_write(update->data,
				(update->tag & LCM_TAG_MASK));
		} else if (tag & LCM_TAG_SLEEP) {
			msleep(update->tag & LCM_TAG_MASK);
		}
		update++;
	}

	s6e8aa5x01_test_key(self, false);

	mipi_eotp_set(0, 0);

	pr_info("%s:brightness[%d], level[%d]\n", __func__, brightness, level);

#endif

	return 0;
}

 static int32_t s6e8aa5x01_dimming_init(struct panel_spec *self,
						struct device *dev)
{
	int start = 0, end, i, offset = 0;
	int ret;


	self->br_map = devm_kzalloc(dev,
		sizeof(unsigned char) * (MAX_BRIGHTNESS + 1), GFP_KERNEL);
	if (!self->br_map) {
		//dev_err(dev, "failed to allocate br_map\n");
		ret = -ENOMEM;
		return ret;
	}

	for (i = 0; i < DIMMING_COUNT; i++) {
		end = br_convert[offset++];
		memset(&self->br_map[start], i, end - start + 1);
		start = end + 1;
	}

	return 0;

}

#if defined(CONFIG_MACH_PIKEB_J1_3G)
static uint32_t ea8061s_before_resume(struct panel_spec *self)
{
	if(is_gpio_request2==0){
		if (gpio_request(153, "LCD LDO")) {
			printk("Failed ro request  GPIO_%d \n",153);
			return 0;
		}
		gpio_direction_output(153, 0);
		is_gpio_request2=1;
	}
	gpio_set_value(153, 1);
	return 0;
}

static uint32_t ea8061s_after_suspend(struct panel_spec *self)
{
	if(is_gpio_request2==0){
		if (gpio_request(153, "LCD LDO")) {
			printk("Failed ro request  GPIO_%d \n",153);
			return 0;
		}
		gpio_direction_output(153, 0);
		is_gpio_request2=1;
	}
	gpio_set_value(153, 0);
	return 0;
}
#endif
static struct panel_operations lcd_ea8061s_mipi_operations = {
	.panel_init = ea8061s_mipi_init,
	.panel_readid = ea8061s_readid,
	.panel_enter_sleep = ea8061s_enter_sleep,
	.panel_set_brightness = s6e8aa5x01_set_brightnees,
	.panel_dimming_init = s6e8aa5x01_dimming_init,
#if defined(CONFIG_MACH_PIKEB_J1_3G)
	.panel_after_suspend=ea8061s_after_suspend,
	.panel_before_resume=ea8061s_before_resume,
#endif
};

static struct timing_rgb lcd_ea8061s_mipi_timing = {
	.hfp = 74,  /* unit: pixel */
	.hbp = 88,
	.hsync = 26,
	.vfp = 16, /*unit: line*/
	.vbp = 4,
	.vsync = 4,
};


static struct info_mipi lcd_ea8061s_mipi_info = {
	.work_mode  = SPRDFB_MIPI_MODE_VIDEO,
	.video_bus_width = 24, /*18,16*/
	.lan_number = 	2,
	.phy_feq =481*1000,
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.te_pol = SPRDFB_POLARITY_POS,
	.color_mode_pol = SPRDFB_POLARITY_NEG,
	.shut_down_pol = SPRDFB_POLARITY_NEG,
	.timing = &lcd_ea8061s_mipi_timing,
	.ops = NULL,
};

struct panel_spec lcd_ea8061s_mipi_spec = {
	.width = 480,
	.height = 800,
	.fps	= 60,
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		.mipi = &lcd_ea8061s_mipi_info
	},
	.ops = &lcd_ea8061s_mipi_operations,
};
struct panel_cfg lcd_ea8061s_mipi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0xea8061,
	.lcd_name = "lcd_ea8061s_mipi",
	.panel = &lcd_ea8061s_mipi_spec,
};

//temp code
static int __init lcd_ea8061s_mipi_init(void)
{
/*
	printk("hx8379C_mipi_init: set lcd backlight.\n");

		 if (gpio_request(214, "LCD_BL")) {
			   printk("Failed ro request LCD_BL GPIO_%d \n",
					   214);
			   return -ENODEV;
		 }
		 gpio_direction_output(214, 1);
		 gpio_set_value(214, 1);
*/

	return sprdfb_panel_register(&lcd_ea8061s_mipi);
}

subsys_initcall(lcd_ea8061s_mipi_init);

