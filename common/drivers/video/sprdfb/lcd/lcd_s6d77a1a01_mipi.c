/* drivers/video/sprdfb/lcd_s6d77a1a01_mipi.c
 *
 * Support for s6d77a1a01 mipi LCD device
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
#include <mach/gpio.h>
#ifdef CONFIG_LCD_ESD_RECOVERY
#include "../esd_detect.h"
#endif
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
	{LCM_SEND(5), {3, 0, 0xF0,0x5A , 0x5A} },
	{LCM_SEND(5), {3, 0, 0xF1,0x5A , 0x5A} },
	{LCM_SEND(5), {3, 0, 0xFC,0xA5 , 0xA5} },
	{LCM_SEND(4), {2, 0, 0xE2,0xFD} },
	{LCM_SEND(10),{8, 0, 0xF2, 0x11,0x04,0x08,0x13,0x06,0x37,0x1E } },
	{LCM_SEND(8), {6, 0, 0xF3, 0x11,0x00,0x00,0x00,0x10  } },
	{LCM_SEND(46),{44, 0, 0xF4,0x01,0x02,0x23,0x24,0x25,0x25,0x26,0x26,0x29,0x29,0x2C,\
						  0x2B,0x2C,0x07,0x08,0x05,0x04,0x04,0x01,0x01,0x22,0x0A,0x17,\
						  0x0F,0x11,0x23,0x0D,0x0A,0x0B,0x02,0x19,0x13,0x1E,0x1F,0x23,\
						  0x20,0x05,0x16,0x1A,0x16,0x1E,0x24,0x1E  } },                   
	{LCM_SEND(31),{29, 0, 0xF5,0x28,0x28,0xB9,0x21,0x35,0x55,0x06,0x0A,0x00,0x39,0x00,\
						  0x04,0x09,0x49,0xC6,0xC9,0x49,0x0F,0x0F,0x1F,0x00,0x10,0xE0,\
						  0xE2,0x0E,0x34,0x34,0x03  } },
	
	{LCM_SEND(20), {18, 0, 0xFE,0x00,0x02,0x01,0x39,0x60,0x40,0x21,0x00,0x4B,0x00,0x00,\
						   0x00,0xF0,0x00,0x00,0x00,0x02 } },
	{LCM_SLEEP(10)},
	//Initializing Sequence 
	{LCM_SEND(13), {11, 0, 0xEE, 0x04,0xE3,0x04,0xE3,0x10,0x00,0x00,0x00,0x00,0x00 } },
	{LCM_SEND(23), {21, 0, 0xEF, 0x23,0x01,0x00,0x00,0x2A,0x49,0x08,0x27,0x21,0x40,0x10,\
						   0x24,0x02,0x21,0x21,0x03,0x03,0x40,0x00,0x30} },
	{LCM_SEND(33), {31, 0, 0xF7, 0x02,0x1A,0x1B,0x02,0x02,0x0D,0x0D,0x09,0x09,0x0C,0x0C,\
						   0x08,0x08,0x04,0x06,0x02,0x1A,0x1B,0x02,0x02,0x0F,0x0F,0x0B,\
						   0x0B,0x0E,0x0E,0x0A,0x0A,0x05,0x07 } },
	{LCM_SEND(9), {7, 0, 0xF6, 0x63,0x21,0x15,0x00,0x00,0x30 } },
	{LCM_SEND(2), {0x36, 0x10} },
/* gamma setting */
	{LCM_SEND(54), {52, 0, 0x11,0x33,0x10,0x15,0x0E,0x18,0x20,0x20,0x21,0x25,0x23,0x1A,\
						   0x17,0x12,0x0C,0x03,0x07,0x11,0x33,0x10,0x15,0x0E,0x18,0x20,\
						   0x20,0x21,0x25,0x23,0x1A,0x17,0x12,0x0C,0x03,0x07,0x11,0x33,\
						   0x10,0x15,0x0E,0x18,0x20,0x1E,0x1F,0x24,0x23,0x1A,0x17,0x12,
						   0x0C,0x03,0x07} },
	{LCM_SEND(54), {52, 0, 0xFB,0x11,0x33,0x0F,0x15,0x0F,0x18,0x20,0x20,0x22,0x26,0x22,\
						   0x1B,0x18,0x13,0x0C,0x04,0x07,0x11,0x33,0x0F,0x15,0x0F,0x18,\
						   0x20,0x20,0x22,0x26,0x22,0x1B,0x18,0x13,0x0C,0x04,0x07,0x11,\
						   0x33,0x0F,0x15,0x0F,0x18,0x20,0x1E,0x20,0x25,0x22,0x1B,0x18,\
						   0x13,0x0C,0x04,0x07 } },
	{LCM_SEND(1), {0x11} },
	{LCM_SLEEP(120)},	/*>120ms*/
	{LCM_SEND(1), {0x29} },
	{LCM_SLEEP(20)}

};


static LCM_Init_Code disp_on[] = {
	{LCM_SEND(1), {0x11} },
	{LCM_SLEEP(120)},	/*>120ms*/
	{LCM_SEND(1), {0x29} },
	{LCM_SLEEP(20)}
};

static LCM_Init_Code sleep_in[] = {
	{LCM_SEND(1), {0x28} },
	{LCM_SLEEP(10)},	/*>10ms*/
	{LCM_SEND(1), {0x10} },
	{LCM_SLEEP(120)}	/*>120ms*/
};

static LCM_Init_Code sleep_out[] = {
	{LCM_SEND(1), {0x11} },
	{LCM_SLEEP(120)},	/*>120ms*/
	{LCM_SEND(1), {0x29} },
	{LCM_SLEEP(50)}	/*>50ms*/
};

extern uint32_t lcd_id_from_uboot;

static int32_t s6d77a1a01_mipi_init(struct panel_spec *self)
{
	int32_t i = 0;
	LCM_Init_Code *init;
	unsigned int tag;
    uint32_t init_cmd_len;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_dcs_write_t mipi_dcs_write = self->info.mipi->ops->mipi_dcs_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	printk("kernel s6d77a1a01_mipi_init\n");

	mipi_set_cmd_mode();
	mipi_eotp_set(0,0);

    if(lcd_id_from_uboot == 0x55BF10) {
        init = init_data;
        init_cmd_len = ARRAY_SIZE(init_data);
    } 
    

	for(i = 0; i < init_cmd_len; i++){
		tag = (init->tag >>24);
		if(tag & LCM_TAG_SEND){
			mipi_dcs_write(init->data, (init->tag & LCM_TAG_MASK));
		}else if(tag & LCM_TAG_SLEEP){
			msleep(init->tag & LCM_TAG_MASK);
		}
		init++;
	}
	mipi_eotp_set(0,0);

	return 0;
}

static uint32_t s6d77a1a01_readid(struct panel_spec *self)
	{
		int32_t j =0;
		uint8_t read_data[4] = {0};
		int32_t read_rtn = 0;
		uint8_t param[2] = {0};
		mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
		mipi_force_write_t mipi_force_write = self->info.mipi->ops->mipi_force_write;
		mipi_force_read_t mipi_force_read = self->info.mipi->ops->mipi_force_read;
		mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

		LCD_PRINT("kernel lcd_s6d77a1a01_mipi read id!\n");

		mipi_set_cmd_mode();
		mipi_eotp_set(0,0);

		for(j = 0; j < 4; j++){
			param[0] = 0x03;
			param[1] = 0x00;
			mipi_force_write(0x37, param, 2);
			mipi_force_read(0x04, 3, &read_data);
	
			if((0x55 == read_data[0])&&(0xBF == read_data[1])&&(0x10 == read_data[2])){
					LCD_PRINT("lcd_s6d77a1a01_mipi read id success!\n");
					return 0x55BF10;
				}
			}

		mipi_eotp_set(0,0);

		return 0;
}

static int32_t s6d77a1a01_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	int32_t i = 0;
	LCM_Init_Code *sleep_in_out = NULL;
	unsigned int tag;
	int32_t size = 0;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_dcs_write_t mipi_dcs_write = self->info.mipi->ops->mipi_dcs_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	printk("kernel s6d77a1a01_enter_sleep, is_sleep = %d\n", is_sleep);

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

#ifdef CONFIG_FB_BL_EVENT_CTRL
static int32_t s6d77a1a01_backlight_ctrl(struct panel_spec *self, uint16_t bl_on)
{
	if (bl_on)
		fb_bl_notifier_call_chain(FB_BL_EVENT_RESUME, NULL);
	else
		fb_bl_notifier_call_chain(FB_BL_EVENT_SUSPEND, NULL);

	return 0;
}
#endif

#ifdef CONFIG_LCD_ESD_RECOVERY
struct esd_det_info s6d77a1a01_esd_info_hf = {
	.name = "s6d77a1a01",
	.type = ESD_TRIGGER_RISING,
	.gpio = 236,
};
#endif

static struct panel_operations lcd_s6d77a1a01_mipi_operations = {
	.panel_init = s6d77a1a01_mipi_init,
	.panel_readid = s6d77a1a01_readid,
	.panel_enter_sleep = s6d77a1a01_enter_sleep,
#ifdef CONFIG_FB_BL_EVENT_CTRL
	.panel_set_brightness = s6d77a1a01_backlight_ctrl,
#endif
};

static struct timing_rgb lcd_s6d77a1a01_mipi_timing = {
	.hfp = 30,  /* unit: pixel */
	.hbp = 55,
	.hsync = 20,
	.vfp = 9, /*unit: line*/
	.vbp = 15,
	.vsync = 4,
};

static struct info_mipi lcd_s6d77a1a01_mipi_info = {
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
	.timing = &lcd_s6d77a1a01_mipi_timing,
	.ops = NULL,
};

struct panel_spec lcd_s6d77a1a01_mipi_spec = {
	.width = 480,
	.height = 800,
	.display_width = 52,
	.display_height = 87,
	.fps	= 60,
	.suspend_mode=SEND_SLEEP_CMD,
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		.mipi = &lcd_s6d77a1a01_mipi_info
	},
	.ops = &lcd_s6d77a1a01_mipi_operations,
	#ifdef CONFIG_LCD_ESD_RECOVERY
	.esd_info = &s6d77a1a01_esd_info_hf,
    #endif
};

struct panel_cfg lcd_s6d77a1a01_mipi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x55BF10,
	.lcd_name = "lcd_s6d77a1a01_mipi",
	.panel = &lcd_s6d77a1a01_mipi_spec,
};

//temp code
static int __init lcd_s6d77a1a01_mipi_init(void)
{
/*
	printk("s6d77a1a01_mipi_init: set lcd backlight.\n");

		 if (gpio_request(214, "LCD_BL")) {
			   printk("Failed ro request LCD_BL GPIO_%d \n",
					   214);
			   return -ENODEV;
		 }
		 gpio_direction_output(214, 1);
		 gpio_set_value(214, 1);
*/
  
	return sprdfb_panel_register(&lcd_s6d77a1a01_mipi);
}

subsys_initcall(lcd_s6d77a1a01_mipi_init);

