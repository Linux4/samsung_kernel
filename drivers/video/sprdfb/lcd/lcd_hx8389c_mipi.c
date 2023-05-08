/* drivers/video/sprdfb/lcd_hx8389c_mipi.c
 *
 * Support for hx8389c mipi LCD device
 *
 * Copyright (C) 2010 Spreadtrum
 *
 */

#include <linux/kernel.h>
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


typedef struct LCM_Init_Code_tag {
	unsigned int tag;
	unsigned char data[MAX_DATA];
}LCM_Init_Code;

typedef struct LCM_force_cmd_code_tag{
	unsigned int datatype;
	LCM_Init_Code real_cmd_code;
}LCM_Force_Cmd_Code;

#ifdef CONFIG_MACH_GRANDPRIME3G_VE
static int is_gpio_request2=0;
#endif

#define LCM_TAG_SHIFT 24
#define LCM_TAG_MASK  ((1 << 24) -1)
#define LCM_SEND(len) ((1 << LCM_TAG_SHIFT)| len)
#define LCM_SLEEP(ms) ((2 << LCM_TAG_SHIFT)| ms)
//#define ARRAY_SIZE(array) ( sizeof(array) / sizeof(array[0]))

#define LCM_TAG_SEND  (1<< 0)
#define LCM_TAG_SLEEP (1 << 1)

static LCM_Init_Code init_data[] = {

	{LCM_SEND(6),{4,0,0xB9,0xFF,0x83,0x89}},
	{LCM_SEND(23),{21,0,0xB1,0x7F,0x10,0x10,0xD2,0x32,0x80,0x10,0xF0,0x56,0x80,0x20,0x20,0xF8,0xAA,0xAA,0xA1,0x00,0x80,0x30,0x00}},
	{LCM_SEND(13),{11,0,0xB2,0x82,0x50,0x05,0x07,0xF0,0x38,0x11,0x64,0x5D,0x09}},
	{LCM_SEND(14),{12,0,0xB4,0x66,0x66,0x66,0x70,0x00,0x00,0x18,0x76,0x28,0x76,0xA8}},
	{LCM_SEND(2),{0xD2,0x33}},
	{LCM_SEND(7),{5,0,0xC0,0x30,0x17,0x00,0x03}},
	{LCM_SEND(7),{5,0,0xC7,0x00,0x80,0x00,0xC0}},
	{LCM_SEND(7),{5,0,0xBF,0x05,0x50,0x00,0x3E}},
	{LCM_SLEEP(10)},
	//{LCM_SEND(6),{4,0,0xB9,0xFF,0x83,0x89}},
	{LCM_SEND(2),{0xCC,0x0E}},
	{LCM_SEND(38),{36,0,0xD3,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x32,0x10,0x00,0x00,0x00,0x03,0xC6,0x03,0xC6,0x00,0x00,0x00,0x00 \
	,0x35,0x33,0x04,0x04,0x37,0x00,0x00,0x00,0x05,0x08,0x00,0x00,0x0A,0x00,0x01}},
	{LCM_SEND(41),{39,0,0xD5,0x18,0x18,0x18,0x18,0x19,0x19,0x18,0x18,0x20,0x21,0x24,0x25,0x18,0x18,0x18,0x18,0x00,0x01,0x04,0x05 \
	,0x02,0x03,0x06,0x07,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18}},
	{LCM_SEND(41),{39,0,0xD6,0x18,0x18,0x18,0x18,0x18,0x18,0x19,0x19,0x25,0x24,0x21,0x20,0x18,0x18 \
	,0x18,0x18,0x07,0x06,0x03,0x02,0x05,0x04,0x01,0x00,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18 \
	,0x18,0x18,0x18,0x18,0x18,0x18}},
	{LCM_SEND(6),{4,0,0xB7,0x20,0x80,0x00}},

//gamma
	{LCM_SEND(45),{43,0,0xE0,0x00,0x02,0x06,0x38,0x3C,0x3F,0x1B,0x46,0x06,0x09,0x0C,0x17 \
	,0x10,0x13,0x16,0x13,0x14,0x08,0x13,0x15,0x19,0x00,0x02,0x06,0x37,0x3C,0x3F \
	,0x1A,0x45,0x05,0x09,0x0B,0x16,0x0F,0x13,0x15,0x13,0x14,0x07,0x12,0x14,0x18}},

	{LCM_SEND(2),{0xBD,0x00}},
	{LCM_SEND(46),{44,0,0xC1,0x00,0x00,0x08,0x10,0x18,0x20,0x28,0x30,0x38,0x40,0x48,0x50 \
	,0x58,0x60,0x68,0x70,0x78,0x80,0x88,0x90,0x98,0xA0,0xA8,0xB0,0xB8,0xC0,0xC8,0xD0,0xD8 \
	,0xE0,0xE8,0xF0,0xF8,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0xBD,0x01}},
	{LCM_SEND(45),{43,0,0xC1,0x00,0x08,0x10,0x18,0x20,0x28,0x30,0x38,0x40,0x48,0x50,0x58,0x60,0x68,0x70,0x78 \
	,0x80,0x88,0x90,0x98,0xA0,0xA8,0xB0,0xB8,0xC0,0xC8,0xD0,0xD8,0xE0,0xE8,0xF0,0xF8,0xFF,0x00,0x00,0x00 \
	,0x00,0x00,0x00,0x00,0x00,0x00}},
	{LCM_SEND(2),{0xBD,0x02}},
	{LCM_SEND(45),{43,0,0xC1,0x00,0x08,0x10,0x18,0x20,0x28,0x30,0x38,0x40,0x48,0x50,0x58,0x60,0x68,0x70,0x78,0x80,0x88,0x90 \
	,0x98,0xA0,0xA8,0xB0,0xB8,0xC0,0xC8,0xD0,0xD8,0xE0,0xE8,0xF0,0xF8,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
}},

//CABC
	{LCM_SEND(2),{0x51,0x80}},
	{LCM_SLEEP(5)},

	{LCM_SEND(16),{14,0,0xC9,0x1F,0x2E,0x0A,0x1E,0x81,0x1E,0x00,0x00,0x01,0x19,0x00,0x00,0x20}},
	{LCM_SLEEP(5)},
	{LCM_SEND(2),{0x55,0x00}},
	{LCM_SLEEP(5)},
	{LCM_SEND(2),{0x53,0x24}},
	{LCM_SLEEP(5)},

//display on
	{LCM_SEND(1),{0x11}},
	{LCM_SLEEP(180)},
	{LCM_SEND(1),{0x29}},
	{LCM_SLEEP(10)}, 

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


static int32_t hx8389c_mipi_init(struct panel_spec *self)
{
	int32_t i = 0;
	LCM_Init_Code *init = init_data;
	unsigned int tag;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_dcs_write_t mipi_dcs_write = self->info.mipi->ops->mipi_dcs_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

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

#ifdef CONFIG_MACH_GRANDPRIME3G_VE
static uint32_t hx8389c_before_resume(struct panel_spec *self)
{
	if(is_gpio_request2==0){
		if (gpio_request(167, "LCD LDO")) {
			printk("Failed ro request  GPIO_%d \n",167);
			return 0;
		}
		gpio_direction_output(167, 0);
		is_gpio_request2=1;
	}
	gpio_set_value(167, 1);
	return 0;
}

static uint32_t hx8389c_after_suspend(struct panel_spec *self)
{
	if(is_gpio_request2==0){
		if (gpio_request(167, "LCD LDO")) {
			printk("Failed ro request  GPIO_%d \n",167);
			return 0;
		}
		gpio_direction_output(167, 0);
		is_gpio_request2=1;
	}
	gpio_set_value(167, 0);
	return 0;
}
#endif

static uint32_t hx8389c_readid(struct panel_spec *self)
{
	int32_t j =0;
	uint8_t read_data[3] = {0};
	int32_t read_rtn = 0;
	uint8_t param[2] = {0};
	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_force_write_t mipi_force_write = self->info.mipi->ops->mipi_force_write;
	mipi_force_read_t mipi_force_read = self->info.mipi->ops->mipi_force_read;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	LCD_PRINT("kernel lcd_hx8389c_mipi_J3 read id!\n");

	mipi_set_cmd_mode();
	mipi_eotp_set(0,0);

	for(j = 0; j < 4; j++){
		param[0] = 0x01;
		param[1] = 0x00;
		mipi_force_write(0x37, param, 2);
		read_rtn = mipi_force_read(0xda,1,&read_data[0]);
		read_rtn = mipi_force_read(0xdb,1,&read_data[1]);
		read_rtn = mipi_force_read(0xdc,1,&read_data[2]);


		if((0x55 == read_data[0])&&(0xc0 == read_data[1])&&(0x90 == read_data[2]))
		{
			printk("lcd_hx8389c_mipi read id success!\n");
			return 0x8389;
		}
	}
	mipi_eotp_set(0,0);
	LCD_PRINT("lcd_hx8389c_mipi read id fail! 0xda,0xdb,0xdc is 0x%x,0x%x,0x%x!\n",read_data[0],read_data[1],read_data[2]);
	return 0;
}


static int32_t hx8389c_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	int32_t i = 0;
	LCM_Init_Code *sleep_in_out = NULL;
	unsigned int tag;
	int32_t size = 0;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_dcs_write_t mipi_dcs_write = self->info.mipi->ops->mipi_dcs_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	printk("kernel hx8389c_enter_sleep, is_sleep = %d\n", is_sleep);

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

static struct panel_operations lcd_hx8389c_mipi_operations = {
	.panel_init = hx8389c_mipi_init,
	.panel_readid = hx8389c_readid,
	.panel_enter_sleep = hx8389c_enter_sleep,
#ifdef CONFIG_MACH_GRANDPRIME3G_VE
	.panel_after_suspend=hx8389c_after_suspend,
	.panel_before_resume=hx8389c_before_resume,
#endif
};

static struct timing_rgb lcd_hx8389c_mipi_timing = {
	.hfp = 80,  /* unit: pixel */
	.hbp = 80,
	.hsync = 20,
	.vfp = 18, /*unit: line*/
	.vbp = 13,
	.vsync = 2,
};


static struct info_mipi lcd_hx8389c_mipi_info = {
	.work_mode  = SPRDFB_MIPI_MODE_VIDEO,
	.video_bus_width = 24, /*18,16*/
	.lan_number = 	2,
	.phy_feq =500*1000,
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.te_pol = SPRDFB_POLARITY_POS,
	.color_mode_pol = SPRDFB_POLARITY_NEG,
	.shut_down_pol = SPRDFB_POLARITY_NEG,
	.timing = &lcd_hx8389c_mipi_timing,
	.ops = NULL,
};

struct panel_spec lcd_hx8389c_mipi_spec = {
	.width = 540,
	.height = 960,
	.fps = 60,
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		.mipi = &lcd_hx8389c_mipi_info
	},
	.ops = &lcd_hx8389c_mipi_operations,
};
struct panel_cfg lcd_hx8389c_mipi = {

	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x8389,
	.lcd_name = "lcd_hx8389c_mipi",
	.panel = &lcd_hx8389c_mipi_spec,
};

//temp code
static int __init lcd_hx8389c_mipi_init(void)
{
	return sprdfb_panel_register(&lcd_hx8389c_mipi);
}

subsys_initcall(lcd_hx8389c_mipi_init);

