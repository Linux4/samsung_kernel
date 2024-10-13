/* drivers/video/sc8825/lcd_ams549hq01_mipi.c
 *
 * Support for ams549hq01 mipi LCD device
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
#include"../sprdfb_dispc_reg.h"
//#include <mach/adc.h>
#include <soc/sprd/gpio.h>
//#define LCD_Delay(ms)  uDelay(ms*1000)

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
	{LCM_SEND(1), {0x11}},                 // Sleep-Out
	{LCM_SLEEP(60)},
	{LCM_SEND(36), {34, 0, 0xca,0x01,0x00,0x01,0x00,0x01,0x00,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,\
			0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x00,0x00,0x00}},
	{LCM_SEND(7), {5, 0, 0xb2,0x00,0x06,0x00,0x06}},
	{LCM_SEND(2), {0xf7, 0x03}},
	{LCM_SEND(2), {0x55, 0x01}},/*0x00 : ACL2 Off, 0x01 : ACL2 15%, 0x02 : ACL2 25%, 0x03 : ACL2 50%*/
	{LCM_SEND(2), {0x53, 0x20}},
	{LCM_SEND(2), {0x51, 0x00}},
	{LCM_SLEEP(10)},
	{LCM_SEND(1), {0x29}}                 // Display-on
};

static LCM_Init_Code sleep_in[] =  {
{LCM_SEND(1), {0x28}},
{LCM_SLEEP(20)},
{LCM_SEND(1), {0x10}},
{LCM_SLEEP(120)}
};

static LCM_Init_Code sleep_out[] =  {
{LCM_SEND(1), {0x11}},
{LCM_SLEEP(120)},
{LCM_SEND(1), {0x29}},
{LCM_SLEEP(20)}
};

static LCM_Force_Cmd_Code rd_prep_code[]={
	{0x39, {LCM_SEND(5), {0x3, 0, 0xf0, 0x5a, 0x5a}}},
	{0x39, {LCM_SEND(5), {0x3, 0, 0xfc, 0x5a, 0x5a}}},
	{0x37, {LCM_SEND(2), {0x3, 0}}}
};

static LCM_Force_Cmd_Code rd_prep_code_1[]={
	{0x39, {LCM_SEND(5), {0x3, 0, 0xfc, 0xa5, 0xa5}}},
	{0x39, {LCM_SEND(5), {0x3, 0, 0xf0, 0xa5, 0xa5}}}
};

static LCM_Force_Cmd_Code rd_prep_code_2[]={
	{0x37, {LCM_SEND(2), {0x1, 0}}}
};

static int32_t LDO33_power(uint8_t power)	/*0:power down, 1:power up*/
{
	static uint8_t is_requested = 0;
	if (gpio_request(16, "VDDPWR")){
		printk("kernel GPIO%d requeste failed!\n", 16);
		return 0;
	}
	gpio_direction_output(16, power);
	gpio_free(16);
	return 0;
}

static int32_t ams549hq01_mipi_init(struct panel_spec *self)
{
	int32_t i;
	LCM_Init_Code *init = init_data;
	unsigned int tag;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_gen_write_t mipi_gen_write = self->info.mipi->ops->mipi_gen_write;

	pr_debug(KERN_DEBUG "ams549hq01_mipi_init\n");

	LDO33_power(1);
	msleep(20);
	mipi_set_cmd_mode();

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
	return 0;
}

static int32_t ams549hq01_reset(struct panel_spec *self)
{
	dispc_write(0, DISPC_RSTN);
}

static uint32_t ams549hq01_readid(struct panel_spec *self)
{
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
	mipi_set_lp_mode_t mipi_set_lp_mode = self->info.mipi->ops->mipi_set_lp_mode;
	mipi_set_hs_mode_t mipi_set_hs_mode = self->info.mipi->ops->mipi_set_hs_mode;

	printk("lcd_ams549hq01_mipi read id!\n");

	mipi_set_cmd_mode();
	mipi_eotp_set(0,0);
//	mipi_set_lp_mode();
	for(j = 0; j < 4; j++){
		rd_prepare = rd_prep_code;
		for(i = 0; i < ARRAY_SIZE(rd_prep_code); i++){
			tag = (rd_prepare->real_cmd_code.tag >> 24);
			if(tag & LCM_TAG_SEND){
				mipi_force_write(rd_prepare->datatype, rd_prepare->real_cmd_code.data, (rd_prepare->real_cmd_code.tag & LCM_TAG_MASK));
			}else if(tag & LCM_TAG_SLEEP){
				msleep((rd_prepare->real_cmd_code.tag & LCM_TAG_MASK));
			}
			rd_prepare++;
		}

		read_rtn = mipi_force_read(0xda, 1,(uint8_t *)&read_data[0]);
		read_rtn = mipi_force_read(0xdb, 1,(uint8_t *)&read_data[1]);
		read_rtn = mipi_force_read(0xdc, 1,(uint8_t *)&read_data[2]);

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

		printk("lcd_ams549hq01_mipi read id 0xda value is 0x%x, 0x%x, 0x%x!\n", read_data[0], read_data[1], read_data[2]);

		if((0x40 == read_data[0])&&(0x10 == read_data[1])&&(0xa2 == read_data[2]))
		{
			printk("lcd_ams549hq01_mipi read id success!\n");
			mipi_eotp_set(1,1);
			return 0x4010;
		}
	}

//	mipi_set_hs_mode();
	mipi_eotp_set(1,1);
	return 0;
}

static int32_t ams549hq01_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	int32_t i = 0;
	LCM_Init_Code *sleep_in_out = NULL;
	unsigned int tag;
	int32_t size = 0;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_gen_write_t mipi_gen_write = self->info.mipi->ops->mipi_gen_write;

	printk("ams549hq01_enter_sleep, is_sleep = %d\n", is_sleep);

	if(is_sleep){
		sleep_in_out = sleep_in;
		size = ARRAY_SIZE(sleep_in);

		mipi_set_cmd_mode();
		for(i = 0; i < size; i++)
		{
			tag = (sleep_in_out->tag >>24);
			if(tag & LCM_TAG_SEND)
			{
				mipi_gen_write(sleep_in_out->data, (sleep_in_out->tag & LCM_TAG_MASK));
				udelay(20);
			}
			else if(tag & LCM_TAG_SLEEP)
			{
				msleep((sleep_in_out->tag & LCM_TAG_MASK));
			}
			sleep_in_out++;
		}
		LDO33_power(0);
		msleep(20);
		//ams549hq01_reset(self);
		//self->ops->panel_reset(self);
	}else{
		//sleep_in_out = sleep_out;
		//size = ARRAY_SIZE(sleep_out);
		printk("ams549hq01_mipi_init out(20141218)\n");
		self->ops->panel_reset(self);
		ams549hq01_mipi_init(self);
	}

	return 0;
}

static uint32_t ams549hq01_readpowermode(struct panel_spec *self)
{
	int32_t i = 0;
	uint32_t j =0;
	LCM_Force_Cmd_Code * rd_prepare = rd_prep_code_2;
	uint8_t read_data[1] = {0};
	int32_t read_rtn = 0;
	unsigned int tag = 0;

	mipi_force_write_t mipi_force_write = self->info.mipi->ops->mipi_force_write;
	mipi_force_read_t mipi_force_read = self->info.mipi->ops->mipi_force_read;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	pr_debug("lcd_ams549hq01_mipi read power mode!\n");
	mipi_eotp_set(0,0);
	for(j = 0; j < 4; j++){
		rd_prepare = rd_prep_code_2;
		for(i = 0; i < ARRAY_SIZE(rd_prep_code_2); i++){
			tag = (rd_prepare->real_cmd_code.tag >> 24);
			if(tag & LCM_TAG_SEND){
				mipi_force_write(rd_prepare->datatype, rd_prepare->real_cmd_code.data, (rd_prepare->real_cmd_code.tag & LCM_TAG_MASK));
			}else if(tag & LCM_TAG_SLEEP){
				msleep((rd_prepare->real_cmd_code.tag & LCM_TAG_MASK));
			}
			rd_prepare++;
		}
		read_rtn = mipi_force_read(0x0A, 1,(uint8_t *)read_data);
		//printk("lcd_ams549hq01 mipi read power mode 0x0A value is 0x%x! , read result(%d)\n", read_data[0], read_rtn);
		if((0x9c == read_data[0])  && (0 == read_rtn)){
			pr_debug("lcd_ams549hq01_mipi read power mode success!\n");
			mipi_eotp_set(1,1);
			return 0x9c;
		}
	}

	printk("lcd_ams549hq01 mipi read power mode fail!0x0A value is 0x%x! , read result(%d)\n", read_data[0], read_rtn);
	mipi_eotp_set(1,1);
	return 0x0;
}

static int32_t ams549hq01_check_esd(struct panel_spec *self)
{
	uint32_t power_mode;

	mipi_set_lp_mode_t mipi_set_data_lp_mode = self->info.mipi->ops->mipi_set_data_lp_mode;
	mipi_set_hs_mode_t mipi_set_data_hs_mode = self->info.mipi->ops->mipi_set_data_hs_mode;
	mipi_set_lp_mode_t mipi_set_lp_mode = self->info.mipi->ops->mipi_set_lp_mode;
	mipi_set_hs_mode_t mipi_set_hs_mode = self->info.mipi->ops->mipi_set_hs_mode;
	uint16_t work_mode = self->info.mipi->work_mode;

	return 1;

	pr_debug("ams549hq01_check_esd!\n");
#ifndef FB_CHECK_ESD_IN_VFP
	if(SPRDFB_MIPI_MODE_CMD==work_mode){
		mipi_set_lp_mode();
	}else{
		mipi_set_data_lp_mode();
	}
#endif
	power_mode = ams549hq01_readpowermode(self);
	//power_mode = 0x0;
#ifndef FB_CHECK_ESD_IN_VFP
	if(SPRDFB_MIPI_MODE_CMD==work_mode){
		mipi_set_hs_mode();
	}else{
		mipi_set_data_hs_mode();
	}
#endif
	if(power_mode == 0x9c){
		pr_debug("ams549hq01_check_esd OK!\n");
		return 1;
	}else{
		printk("ams549hq01_check_esd fail!(0x%x)\n", power_mode);
		return 0;
	}
}

static int32_t ams549hq01_after_suspend(struct panel_spec *self)
{
	printk("%s\r\n", __func__);
	return 0;
}

static struct panel_operations lcd_ams549hq01_mipi_operations = {
	.panel_init = ams549hq01_mipi_init,
	.panel_readid = ams549hq01_readid,
	.panel_enter_sleep = ams549hq01_enter_sleep,
	.panel_esd_check = ams549hq01_check_esd,
//	.panel_reset = ams549hq01_reset,
//	.panel_after_suspend = ams549hq01_after_suspend,
};

static struct timing_rgb lcd_ams549hq01_mipi_timing = {
	.hfp = 174,  /* unit: pixel */
	.hbp = 86,
	.hsync = 1,
	.vfp = 9, /*unit: line*/
	.vbp = 5,
	.vsync =2,
};

static struct info_mipi lcd_ams549hq01_mipi_info = {
	.work_mode  = SPRDFB_MIPI_MODE_VIDEO,
	.video_bus_width = 24, /*18,16*/
	.lan_number = 4,
	.phy_feq = 500000,
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.te_pol = SPRDFB_POLARITY_POS,
	.color_mode_pol = SPRDFB_POLARITY_NEG,
	.shut_down_pol = SPRDFB_POLARITY_NEG,
	.timing = &lcd_ams549hq01_mipi_timing,
	.ops = NULL,
};

struct panel_spec lcd_ams549hq01_mipi_spec = {
	.width = 720,
	.height = 1280,
	.fps = 60,
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
//	.is_clean_lcd = true,
	.info = {
		.mipi = &lcd_ams549hq01_mipi_info
	},
	.ops = &lcd_ams549hq01_mipi_operations,
	.suspend_mode = SEND_SLEEP_CMD,
};

struct panel_cfg lcd_ams549hq01_mipi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x4010,
	.lcd_name = "lcd_ams549hq01_mipi",
	.panel = &lcd_ams549hq01_mipi_spec,
};

static int __init lcd_ams549hq01_mipi_init(void)
{
	return sprdfb_panel_register(&lcd_ams549hq01_mipi);
}

subsys_initcall(lcd_ams549hq01_mipi_init);
