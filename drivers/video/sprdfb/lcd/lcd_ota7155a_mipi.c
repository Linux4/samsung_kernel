/* drivers/video/sprdfb/lcd_ota7155a_mipi.c
 *
 * Support for ota7155a mipi LCD device
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
#include <linux/regulator/consumer.h>
#include <linux/err.h>

#define  LCD_DEBUG
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

#define LCD_DATA_EN 160
#define LCD_PWR_EN 161
extern int gpio_direction_output(unsigned gpio, int value);
extern int gpio_request(unsigned gpio, const char *label);
extern void gpio_free(unsigned gpio);

static LCM_Init_Code init_data[] = {
 {LCM_SLEEP(10)},
};

//static LCM_Init_Code disp_on =  {LCM_SEND(1), {0x29}};

//static LCM_Init_Code sleep_in =  {LCM_SEND(1), {0x10}};

//static LCM_Init_Code sleep_out =  {LCM_SEND(1), {0x11}};

static LCM_Init_Code sleep_in[] =  {
{LCM_SLEEP(10)},
};

static LCM_Init_Code sleep_out[] =  {
{LCM_SLEEP(10)},
};

static LCM_Force_Cmd_Code rd_prep_code_1[]={
	{0x37, {LCM_SEND(2), {0x3, 0}}},
};

static int32_t ota7155a_mipi_init(struct panel_spec *self)
{
	int32_t i = 0;
	LCM_Init_Code *init = init_data;
	unsigned int tag;
#if 0
	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_gen_write_t mipi_gen_write = self->info.mipi->ops->mipi_gen_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	printk("lcd_ota7155a_init\n");

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
#endif
/*
struct regulator *lvds_3v3;
	lvds_3v3 = regulator_get(NULL,"vddsim1");
	regulator_disable(lvds_3v3);
	regulator_put(lvds_3v3);
	lvds_3v3 = regulator_get(NULL,"vddsim1");
	if (IS_ERR(lvds_3v3)) {
		lvds_3v3 = NULL;
		printk("%s regulator 3v3 get failed2!!\n", "vddsim1");
	} else {
		regulator_set_voltage(lvds_3v3, 1800000, 1800000);
		regulator_enable(lvds_3v3);
		printk("%s regulator 3v3 set here2!!\n", "vddsim1");
	}
*/
	gpio_request(LCD_PWR_EN, "LCDPWR");
	gpio_direction_output(LCD_PWR_EN, 1);
	msleep(20);
	gpio_request(LCD_DATA_EN, "LCDDATA");
	gpio_direction_output(LCD_DATA_EN, 1);
	msleep(50);
	printk("lcd_ota7155a_init\n");
	return 0;
}

static uint32_t ota7155a_readid(struct panel_spec *self)
{
	int32_t j =0;
	uint8_t read_data[10] = {0};
	int32_t read_rtn = 0;
	uint8_t param[2] = {0};
	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_force_write_t mipi_force_write = self->info.mipi->ops->mipi_force_write;
	mipi_force_read_t mipi_force_read = self->info.mipi->ops->mipi_force_read;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;
#if 0
	printk("lcd_ota7155a_mipi read id!\n");
	mipi_set_cmd_mode();
	mipi_eotp_set(1,0);

	for(j = 0; j < 4; j++){
		param[0] = 0x0a;
		param[1] = 0x00;
		mipi_force_write(0x37, param, 2);
		read_rtn = mipi_force_read(0xA1,10,(uint8_t *)read_data);
		printk("lcd_ota7155a_mipi read id 0xA1 value is 0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x!\n", read_data[0], read_data[1], \
		read_data[2], read_data[3],read_data[4], read_data[5],read_data[6], read_data[7],read_data[8], read_data[9]);

		if((0x20 == read_data[0])&&(0x75 == read_data[1])){
			printk("lcd_ota7155a_mipi read id success!\n");
			mipi_eotp_set(1,1);
			return 0x2075;
		}
	}
	mipi_eotp_set(1,1);
#endif
	return 0;
}

static int32_t ota7155a_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	int32_t i = 0;
	LCM_Init_Code *sleep_in_out = NULL;
	unsigned int tag;
	int32_t size = 0;

	mipi_gen_write_t mipi_gen_write = self->info.mipi->ops->mipi_gen_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	printk("ota7155a_enter_sleep, is_sleep = %d\n", is_sleep);
#if 0
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
#endif
	if(is_sleep){

	gpio_direction_output(LCD_DATA_EN, 0);
	gpio_direction_output(LCD_PWR_EN, 0);
	gpio_free(LCD_DATA_EN);
	gpio_free(LCD_PWR_EN);
	}else{
	gpio_request(LCD_PWR_EN, "LCDPWR");
	gpio_direction_output(LCD_PWR_EN, 1);
	msleep(5);
	gpio_request(LCD_DATA_EN, "LCDDATA");
	gpio_direction_output(LCD_DATA_EN, 1);
	msleep(5);
	}
	return 0;
}

static uint32_t ota7155a_readpowermode(struct panel_spec *self)
{
	int32_t i = 0;
	uint32_t j =0;
	LCM_Force_Cmd_Code * rd_prepare = rd_prep_code_1;
	uint8_t read_data[3] = {0};
	int32_t read_rtn = 0;
	unsigned int tag = 0;

	mipi_force_write_t mipi_force_write = self->info.mipi->ops->mipi_force_write;
	mipi_force_read_t mipi_force_read = self->info.mipi->ops->mipi_force_read;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

//	pr_debug("lcd_ota7155a_mipi read power mode!\n");

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
		read_rtn = mipi_force_read(0x0A, 1, (uint8_t *)read_data);
//		printk("lcd_ota7155a mipi read power mode 0xF5 value r[0]=0x%x,r[1]=0x%x,r[2]=0x%x! , read result(%d)\n", read_data[0],read_data[1],read_data[2],read_rtn);
		if ((0x9c == read_data[0])  && (0 == read_rtn)) {
			mipi_eotp_set(1,1);
//			pr_debug("lcd_ota7155a_mipi read power mode success!\n");
			return 0x9c;
		}
	}
	mipi_eotp_set(1,1);
	printk("lcd_ota7155a ota7155a_readpowermode fail! 0xF5 value r[0]=0x%x,r[1]=0x%x,r[2]=0x%x! , read result(%d)\n", read_data[0],\
		read_data[1],read_data[2],read_rtn);
	return 0x0;
}

static int32_t ota7155a_check_esd(struct panel_spec *self)
{
	uint32_t power_mode;

	mipi_set_lp_mode_t mipi_set_data_lp_mode = self->info.mipi->ops->mipi_set_data_lp_mode;
	mipi_set_hs_mode_t mipi_set_data_hs_mode = self->info.mipi->ops->mipi_set_data_hs_mode;
	mipi_set_lp_mode_t mipi_set_lp_mode = self->info.mipi->ops->mipi_set_lp_mode;
	mipi_set_hs_mode_t mipi_set_hs_mode = self->info.mipi->ops->mipi_set_hs_mode;
	uint16_t work_mode = self->info.mipi->work_mode;

//	pr_debug("ota7155a_check_esd!\n");
#ifndef FB_CHECK_ESD_IN_VFP
	if(SPRDFB_MIPI_MODE_CMD==work_mode){
		mipi_set_lp_mode();
	}else{
		mipi_set_data_lp_mode();
#endif
	power_mode = ota7155a_readpowermode(self);
	//power_mode = 0x0;
#ifndef FB_CHECK_ESD_IN_VFP
	if(SPRDFB_MIPI_MODE_CMD==work_mode){
		mipi_set_hs_mode();
	}else{
		mipi_set_data_hs_mode();
#endif
	if(power_mode == 0x9c){
//		pr_debug("ota7155a_check_esd OK!\n");
		return 1;
	}else{
		printk("ota7155a_check_esd fail!(0x%x)\n", power_mode);
		return 0;
	}
}

static struct panel_operations lcd_ota7155a_mipi_operations = {
	.panel_init = ota7155a_mipi_init,
	.panel_readid = ota7155a_readid,
	.panel_enter_sleep = ota7155a_enter_sleep,
//	.panel_esd_check = ota7155a_check_esd,
};

static struct timing_rgb lcd_ota7155a_mipi_timing = {
	.hfp =60,
	.hbp = 56,
	.hsync =64,
	.vfp = 36,
	.vbp = 30,
	.vsync = 50,
};


static struct info_mipi lcd_ota7155a_mipi_info = {
	.work_mode  = SPRDFB_MIPI_MODE_VIDEO,
	.video_bus_width = 24, /*18,16*/
	.lan_number = 	4,
	.phy_feq = 500*1000,
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.te_pol = SPRDFB_POLARITY_POS,
	.color_mode_pol = SPRDFB_POLARITY_NEG,
	.shut_down_pol = SPRDFB_POLARITY_NEG,
	.timing = &lcd_ota7155a_mipi_timing,
	.ops = NULL,
};

struct panel_spec lcd_ota7155a_mipi_spec = {
	.width = 768,
	.height = 1024,
	.fps = 60,
	.reset_timing = {20,20,120},
	.suspend_mode = SEND_SLEEP_CMD,
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		.mipi = &lcd_ota7155a_mipi_info
	},
	.ops = &lcd_ota7155a_mipi_operations,
};
struct panel_cfg lcd_ota7155a_mipi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x7155,
	.lcd_name = "lcd_ota7155a_mipi",
	.panel = &lcd_ota7155a_mipi_spec,
};

static int __init lcd_ota7155a_mipi_init(void)
{
	return sprdfb_panel_register(&lcd_ota7155a_mipi);
}

subsys_initcall(lcd_ota7155a_mipi_init);

