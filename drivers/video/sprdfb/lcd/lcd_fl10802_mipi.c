/* drivers/video/sc8825/lcd_fl10802_mipi.c
 *
 * Support for fl10802 mipi LCD device
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
//#define LCD_Delay(ms)  uDelay(ms*1000)

#define MAX_DATA   80//48

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
    {0x39,LCM_SEND(6), {4,0,0xb9,0xf1,0x08,0x01}},
    {0x39,LCM_SEND(20), {18,0,0xba,0x31,0x00,0x44,0x25,0xb1,0x0a,0x00,0x00,0xc1,0x00,0x00,0x00,0x0d,0x02,0x5d,0xb5,0xee}},
  
	{0x37,{LCM_SEND(2),{0x3,0}}},
};

static LCM_Force_Cmd_Code rd_prep_code_1[]={
    {0x39,LCM_SEND(6), {4,0,0xb9,0xf1,0x08,0x01}},
    {0x39,LCM_SEND(20), {18,0,0xba,0x31,0x00,0x44,0x25,0xb1,0x0a,0x00,0x00,0xc1,0x00,0x00,0x00,0x0d,0x02,0x5d,0xb5,0xee}},
  
	{0x37,{LCM_SEND(2),{0x1,0}}},
};


static LCM_Init_Code init_data[] = {


{LCM_SEND(6), {4,0,0xB9,0xF1,0x08,0x01}},

{LCM_SEND(7), {5,0,0xB1,0x22,0x1E,0x1E,0x87}},

{LCM_SEND(2), {0xB2,0x23}},

{LCM_SEND(11), {9,0,0xB3,0x00,0x00,0x06,0x06,0x20,0x20,0x30,0x30}},

{LCM_SEND(20), {18,0,0xBA,0x31,0x00,0x44,0x25,0x91,0x0A,0x00,0x00,0xC1,
	                   0x00,0x00,0x00,0x0D,0x02,0x4F,0xB9,0xEE}},

{LCM_SEND(8), {6,0,0xE3,0x09,0x09,0x03,0x03,0x00}},

{LCM_SEND(2), {0xB4,0x00}},

{LCM_SEND(5), {3,0,0xB5,0x09,0x09}},

{LCM_SEND(5), {3,0,0xB6,0x52,0x52}},//0x57

{LCM_SEND(5), {3,0,0xB8,0x64,0x28}},

{LCM_SEND(2), {0xCC,0x00}},

{LCM_SEND(2), {0xBC,0x47}},

{LCM_SEND(54), {52,0,0xE9,0x00,0x00,0x0F,0x03,0x36,0x0A,0x90,0x10,0x01,
			 0x00,0x37,0x13,0x0A,0x90,0x37,0x00,0x00,0x18,0x00,
                     0x00,0x00,0x25,0x09,0x80,0x40,0x00,0x42,0x60,0x00,
                     0x00,0x00,0x09,0x81,0x50,0x01,0x53,0x70,0x00,0x00,
                     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                     0x00,0x00}},

{LCM_SEND(25), {23,0,0xEA,0x94,0x00,0x00,0x00,0x08,0x95,0x10,0x07,0x35,
	                   0x10,0x00,0x00,0x00,0x08,0x94,0x00,0x06,0x24,0x00,
	                   0x00,0x00,0x00}},

{LCM_SEND(37), {35,0,0xE0,0x00,0x00,0x00,0x09,0x09,0x3F,0x17,0x2C,0x02,
                     0x0C,0x12,0x15,0x18,0x16,0x16,0x12,0x16,0x00,0x00,
                     0x00,0x09,0x09,0x3F,0x17,0x2C,0x02,0x0C,0x12,0x15,
                     0x18,0x16,0x16,0x12,0x16}},



{LCM_SEND(2), {0x11,0x00}},
{LCM_SLEEP(120),},

{LCM_SEND(2), {0x29,0x00}},
{LCM_SLEEP(50),},

};

//static LCM_Init_Code disp_on =  {LCM_SEND(1), {0x29}};

static LCM_Init_Code sleep_in[] =  {
{LCM_SEND(1), {0x28}},
{LCM_SLEEP(30)},
{LCM_SEND(1), {0x10}},
{LCM_SLEEP(120)},
};

static LCM_Init_Code sleep_out[] =  {
{LCM_SEND(1), {0x11}},
{LCM_SLEEP(120)},
{LCM_SEND(1), {0x29}},
{LCM_SLEEP(50)},
};

static int32_t fl10802_mipi_init(struct panel_spec *self)
{
	int32_t i;
	LCM_Init_Code *init = init_data;
	unsigned int tag;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_gen_write_t mipi_gen_write = self->info.mipi->ops->mipi_gen_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	pr_debug(KERN_DEBUG "fl10802_mipi_init\n");

	mipi_set_cmd_mode();
	mipi_eotp_set(1,0);

	for(i = 0; i < ARRAY_SIZE(init_data); i++){
		tag = (init->tag >>24);
		if(tag & LCM_TAG_SEND){
			mipi_gen_write(init->data, (init->tag & LCM_TAG_MASK));
			udelay(20);
		}else if(tag & LCM_TAG_SLEEP){
			msleep((init->tag & LCM_TAG_MASK));
		}
		init++;
	}
	mipi_eotp_set(1,1);
	return 0;
}

static uint32_t fl10802_readid(struct panel_spec *self)
{
		/*Jessica TODO: need read id*/
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
	
		printk("kernel lcd_fl10802_mipi read id!\n");
		#if 1
		//self->info.mipi->ops->mipi_set_lp_mode(); 
		mipi_set_cmd_mode(); 
		mipi_eotp_set(1,0); 
		for(j = 0; j < 4; j++){ 
			rd_prepare = rd_prep_code; 
			for(i = 0; i < ARRAY_SIZE(rd_prep_code); i++){ 
				tag = (rd_prepare->real_cmd_code.tag >> 24); 
				if(tag & LCM_TAG_SEND){ 
					mipi_force_write(rd_prepare->datatype, rd_prepare->real_cmd_code.data, (rd_prepare->real_cmd_code.tag & LCM_TAG_MASK)); 
					udelay(20);
				}else if(tag & LCM_TAG_SLEEP){ 
					udelay((rd_prepare->real_cmd_code.tag & LCM_TAG_MASK) * 1000); 
				} 
				rd_prepare++; 
			}
			read_rtn = mipi_force_read(0x04, 3,(uint8_t *)read_data); 
			printk("kernel lcd_fl10802_mipi read id 0xa1 value is 0x%x, 0x%x, 0x%x!\n", read_data[0], read_data[1], read_data[2]); 
			mipi_eotp_set(1,1); 
			if((0x10 == read_data[0])&&(0x80== read_data[1])&&(0x1A == read_data[2])){ 
				printk(" kernel lcd_fl10802_mipi read id success!\n"); 
				return 0x1080; 
			} 
		} 
	#endif
		printk("lcd_fl10802_mipi identify fail!\n");
		//self->info.mipi->ops->mipi_set_hs_mode();
		return 0x0;
}

static uint32_t fl10802_readpowermode(struct panel_spec *self)
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

	pr_debug("lcd_fl10802_mipi read power mode!\n");
	mipi_eotp_set(0,1);
	for(j = 0; j < 4; j++){
		rd_prepare = rd_prep_code_1;
		for(i = 0; i < ARRAY_SIZE(rd_prep_code_1); i++){
			tag = (rd_prepare->real_cmd_code.tag >> 24);
			if(tag & LCM_TAG_SEND){
				mipi_force_write(rd_prepare->datatype, rd_prepare->real_cmd_code.data, (rd_prepare->real_cmd_code.tag & LCM_TAG_MASK));
				udelay(20);
			}else if(tag & LCM_TAG_SLEEP){
				msleep((rd_prepare->real_cmd_code.tag & LCM_TAG_MASK));
			}
			rd_prepare++;
		}
		read_rtn = mipi_force_read(0x0A, 1,(uint8_t *)read_data);
		printk("lcd_fl10802_mipi read power mode 0x0A value is 0x%x! , read result(%d)\n", read_data[0], read_rtn);
		if((0x9c == read_data[0])  && (0 == read_rtn)){
			pr_debug("lcd_fl10802a_mipi read power mode success!\n");
			mipi_eotp_set(1,1);
			return 0x9c;
		}
	}

	printk("lcd_fl10802_mipi read power mode fail!0x0A value is 0x%x! , read result(%d)\n", read_data[0], read_rtn);
	mipi_eotp_set(1,1);
	return 0x0;
}

static int32_t fl10802_check_esd(struct panel_spec *self)
{
	uint32_t power_mode;

	mipi_set_lp_mode_t mipi_set_data_lp_mode = self->info.mipi->ops->mipi_set_data_lp_mode;
	mipi_set_hs_mode_t mipi_set_data_hs_mode = self->info.mipi->ops->mipi_set_data_hs_mode;
	mipi_set_lp_mode_t mipi_set_lp_mode = self->info.mipi->ops->mipi_set_lp_mode;
	mipi_set_hs_mode_t mipi_set_hs_mode = self->info.mipi->ops->mipi_set_hs_mode;
	uint16_t work_mode = self->info.mipi->work_mode;

	pr_debug("fl10802_check_esd!\n");
	power_mode = fl10802_readpowermode(self);
	//power_mode = 0x0;
	if(power_mode == 0x9c){
		pr_debug("fl10802_check_esd OK!\n");
		return 1;
	}else{
		printk("fl10802_check_esd fail!(0x%x)\n", power_mode);
		return 0;
	}
}


static int32_t fl10802_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	int32_t i;
	LCM_Init_Code *sleep_in_out = NULL;
	unsigned int tag;
	int32_t size = 0;

	mipi_dcs_write_t mipi_dcs_write = self->info.mipi->ops->mipi_dcs_write;
	mipi_gen_write_t mipi_gen_write = self->info.mipi->ops->mipi_gen_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	printk(KERN_DEBUG "fl10802_enter_sleep, is_sleep = %d\n", is_sleep);

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
			udelay(20);
		}else if(tag & LCM_TAG_SLEEP){
			msleep((sleep_in_out->tag & LCM_TAG_MASK));
		}
		sleep_in_out++;
	}
	mipi_eotp_set(1,1);
	return 0;
}

static struct panel_operations lcd_fl10802_mipi_operations = {
	.panel_init = fl10802_mipi_init,
	.panel_readid = fl10802_readid,
	.panel_esd_check = fl10802_check_esd,
	.panel_enter_sleep = fl10802_enter_sleep,
};

static struct timing_rgb lcd_fl10802_mipi_timing = {
	.hfp = 80,  /* unit: pixel */
	.hbp = 80,
	.hsync = 10,
	.vfp = 18, /*unit: line*/
	.vbp = 18,
	.vsync = 4,
};

static struct info_mipi lcd_fl10802_mipi_info = {
	.work_mode  = SPRDFB_MIPI_MODE_VIDEO,
	.video_bus_width = 24, /*18,16*/
	.lan_number = 2,
	.phy_feq = 500*1000,
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	//.te_pol = SPRDFB_POLARITY_POS,
	.color_mode_pol = SPRDFB_POLARITY_NEG,
	.shut_down_pol = SPRDFB_POLARITY_NEG,
	.timing = &lcd_fl10802_mipi_timing,
	.ops = NULL,
};

struct panel_spec lcd_fl10802_mipi_spec = {
	.width = 480,
	.height = 800,
	.fps	= 60,
	.reset_timing = {5,40,20},
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		.mipi = &lcd_fl10802_mipi_info
	},
	.ops = &lcd_fl10802_mipi_operations,
};

struct panel_cfg lcd_fl10802_mipi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x1080,
	.lcd_name = "lcd_fl10802_mipi",
	.panel = &lcd_fl10802_mipi_spec,
};

static int __init lcd_fl10802_mipi_init(void)
{
	return sprdfb_panel_register(&lcd_fl10802_mipi);
}

subsys_initcall(lcd_fl10802_mipi_init);
