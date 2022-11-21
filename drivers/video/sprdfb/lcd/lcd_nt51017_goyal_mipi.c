/* drivers/video/sprdfb/lcd_nt51017_mipi.c
 *
 * Support for nt51017 mipi LCD device
 *
 * Copyright (C) 2010 Spreadtrum
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

#define LCM_TAG_SHIFT 24
#define LCM_TAG_MASK  ((1 << 24) -1)
#define LCM_SEND(len) ((1 << LCM_TAG_SHIFT)| len)
#define LCM_SLEEP(ms) ((2 << LCM_TAG_SHIFT)| ms)
//#define ARRAY_SIZE(array) ( sizeof(array) / sizeof(array[0]))

#define LCM_TAG_SEND  (1<< 0)
#define LCM_TAG_SLEEP (1 << 1)




static LCM_Init_Code init_data[] = {
 {LCM_SEND(2), {0x83, 0x96} },
 {LCM_SEND(2), {0x84, 0x69} },
 {LCM_SEND(2), {0x92, 0x19} },
 {LCM_SEND(2), {0x83, 0x00} },
 {LCM_SEND(2), {0x84, 0x00} },
 {LCM_SEND(2), {0x96, 0x00} },

};


static LCM_Init_Code sleep_in[] =  {

	{LCM_SEND(1), {0x11}},
	{LCM_SLEEP(170)},
};

static LCM_Init_Code sleep_out[] =  {
	{LCM_SEND(1), {0x10}},
	{LCM_SLEEP(170)},

};


static int32_t nt51017_mipi_init(struct panel_spec *self)
{

	if (gpio_request(232, "LED_EN0")) {
		printk("Failed ro request LCD_BL GPIO_%d \n",232);
		return -ENODEV;
	}
	gpio_direction_output(232, 1);
	gpio_set_value(232, 1);

	if (gpio_request(233, "LED_EN1")) {
		printk("Failed ro request LCD_BL GPIO_%d \n",233);
		return -ENODEV;
	}
	gpio_direction_output(233, 1);
	gpio_set_value(233, 1);

	mdelay(200);

	int32_t i;
	LCM_Init_Code *init = init_data;
	unsigned int tag;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_dcs_write_t mipi_dcs_write = self->info.mipi->ops->mipi_dcs_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	printk("lcd_nt51017_init\n");

	mipi_set_cmd_mode();
	mipi_eotp_set(0,0);

	for(i = 0; i < ARRAY_SIZE(init_data); i++){
		tag = (init->tag >>24);
		if(tag & LCM_TAG_SEND){
			mipi_dcs_write(init->data, (init->tag & LCM_TAG_MASK));
			udelay(20);
		}else if(tag & LCM_TAG_SLEEP){
			mdelay(init->tag & LCM_TAG_MASK);//udelay((init->tag & LCM_TAG_MASK) * 1000);
		}
		init++;
	}
	mipi_eotp_set(0,0);

	return 0;
}

static uint32_t nt51017_readid(struct panel_spec *self)
{
#if 0
	uint32 j =0;
	uint8_t read_data[4] = {0};
	int32_t read_rtn = 0;
	uint8_t param[2] = {0};
	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_force_write_t mipi_force_write = self->info.mipi->ops->mipi_force_write;
	mipi_force_read_t mipi_force_read = self->info.mipi->ops->mipi_force_read;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	printk("lcd_nt51017_mipi read id!\n");

	mipi_set_cmd_mode();
	mipi_eotp_set(0,0);

	for(j = 0; j < 4; j++){
		param[0] = 0x01;
		param[1] = 0x00;
		mipi_force_write(0x37, param, 2);
		read_rtn = mipi_force_read(0xda,1,&read_data[0]);
		printk("lcd_nt51017_mipi read id 0xda value is 0x%x!\n",read_data[0]);

		read_rtn = mipi_force_read(0xdb,1,&read_data[1]);
		printk("lcd_nt51017_mipi read id 0xdb value is 0x%x!\n",read_data[1]);

		read_rtn = mipi_force_read(0xdc,1,&read_data[2]);
		printk("lcd_nt51017_mipi read id 0xdc value is 0x%x!\n",read_data[2]);

		if((0x55 == read_data[0])&&(0xbf == read_data[1] || 0xc0 == read_data[1] || 0xbe == read_data[1])&&(0x90 == read_data[2])){
				printk("lcd_nt51017_mipi read id success!\n");
				return 0x8369;
		}
	}

	mipi_eotp_set(0,0);

	printk("lcd_s6d77a1_mipi read id failed!\n");
	return 0;
#endif
	return 0x51017;
}

static int32_t nt51017_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	int32_t i = 0;
	LCM_Init_Code *sleep_in_out = NULL;
	unsigned int tag;
	int32_t size = 0;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_dcs_write_t mipi_dcs_write = self->info.mipi->ops->mipi_dcs_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	printk("kernel nt51017_enter_sleep, is_sleep = %d\n", is_sleep);

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

static int32_t nt51017_before_resume(struct panel_spec *self, uint8_t is_sleep)
{
	gpio_set_value(152, 1);
	gpio_set_value(155, 1);
	gpio_set_value(232, 1);
	gpio_set_value(233, 1);
}

static int32_t nt51017_after_suspend(struct panel_spec *self, uint8_t is_sleep)
{
	gpio_set_value(152, 0);
	gpio_set_value(155, 0);
	gpio_set_value(232, 0);
	gpio_set_value(233, 0);
}

static struct panel_operations lcd_nt51017_mipi_operations = {
	.panel_init = nt51017_mipi_init,
	.panel_readid = nt51017_readid,
	.panel_enter_sleep = nt51017_enter_sleep,
	.panel_before_resume = nt51017_before_resume,
	.panel_after_suspend = nt51017_after_suspend,
};

static struct timing_rgb lcd_nt51017_mipi_timing = {

	.hfp = 80,  /* unit: pixel */
	.hbp = 80,
	.hsync = 1,
	.vfp = 18, /*unit: line*/
	.vbp = 23,
	.vsync = 1,

};


static struct info_mipi lcd_nt51017_mipi_info = {
	.work_mode  = SPRDFB_MIPI_MODE_VIDEO,
	.video_bus_width = 24, /*18,16*/
	.lan_number = 	4,
	.phy_feq =481*1000,
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.te_pol = SPRDFB_POLARITY_POS,
	.color_mode_pol = SPRDFB_POLARITY_NEG,
	.shut_down_pol = SPRDFB_POLARITY_NEG,
	.timing = &lcd_nt51017_mipi_timing,
	.ops = NULL,
};

struct panel_spec lcd_nt51017_mipi_spec = {
	.width = 800,
	.height =1280,
	.fps = 60,
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
	.suspend_mode = SEND_SLEEP_CMD,
	.info = {
		.mipi = &lcd_nt51017_mipi_info
	},
	.ops = &lcd_nt51017_mipi_operations,
};
struct panel_cfg lcd_nt51017_mipi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x51017,
	.lcd_name = "lcd_nt51017_mipi",
	.panel = &lcd_nt51017_mipi_spec,
};


static int __init lcd_nt51017_mipi_init(void)
{
	if (gpio_request(152, "LCD_3V3")) {
		printk("Failed ro request LCD_3V3 GPIO_%d \n",152);
		return -ENODEV;
	}
	if (gpio_request(155, "BL_EN")) {
		printk("Failed ro request BL_EN GPIO_%d \n",155);
		return -ENODEV;
	}
	if (gpio_request(232, "CABC_EN0")) {
		printk("Failed ro request CABC_EN0 GPIO_%d \n",232);
		return -ENODEV;
	}
	if (gpio_request(233, "CABC_EN1")) {
		printk("Failed ro request CABC_EN1 GPIO_%d \n",233);
		return -ENODEV;
	}
	gpio_direction_output(152, 1);
	gpio_direction_output(155, 1);
	gpio_direction_output(232, 1);
	gpio_direction_output(233, 1);

	return sprdfb_panel_register(&lcd_nt51017_mipi);
}

subsys_initcall(lcd_nt51017_mipi_init);
