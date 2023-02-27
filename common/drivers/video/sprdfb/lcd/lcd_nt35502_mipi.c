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

#define MAX_DATA   100

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
    //Power setting Sequence
	{LCM_SEND(8),{6,0,0xF0,0x55,0xAA,0x52,0x08,0x01}},
	 {LCM_SEND(4),{2,0,0xB0,0x08}},
	 {LCM_SEND(4),{2,0,0xB2,0x40}},
	 {LCM_SEND(4),{2,0,0xB3,0x28}},
	 {LCM_SEND(4),{2,0,0xB4,0x19}},
	 {LCM_SEND(4),{2,0,0xB5,0x54}},
	 {LCM_SEND(4),{2,0,0xB9,0x36}},
	 {LCM_SEND(4),{2,0,0xBA,0x26}},
	 {LCM_SEND(5),{3,0,0xBC,0xA0,0x00}},
	 {LCM_SEND(5),{3,0,0xBD,0xA0,0x00}},
	
	 //Gamma setting
	{LCM_SEND(19),{17,0,0xD1,0x00,0x65,0x00,0x88,0x00,0xC9,0x00,0xD7,0x00,0xEB,0x01,0x12,0x01,0x33,0x01,0x65}},//new modified(modified_2_3)
	{LCM_SEND(19),{17,0,0xD2,0x01,0x8B,0x01,0xC2,0x01,0xEA,0x02,0x21,0x02,0x4C,0x02,0x4D,0x02,0x72,0x02,0x96}},//new modified(modified_2_3)
	{LCM_SEND(19),{17,0,0xD3,0x02,0xAA,0x02,0xC1,0x02,0xCF,0x02,0xE3,0x02,0xEF,0x02,0xFF,0x03,0x0B,0x03,0x1B}},//new modified(modified_2_3)
	{LCM_SEND(7),{5,0,0xD4,0x03,0x3A,0x03,0x9A}},																//(modified)
	{LCM_SEND(19),{17,0,0xD5,0x00,0x65,0x00,0x88,0x00,0xC9,0x00,0xD7,0x00,0xEB,0x01,0x12,0x01,0x33,0x01,0x65}},//new modified(modified_2_3)
	{LCM_SEND(19),{17,0,0xD6,0x01,0x8B,0x01,0xC2,0x01,0xEA,0x02,0x21,0x02,0x4C,0x02,0x4D,0x02,0x72,0x02,0x96}},//new modified(modified_2_3)
	{LCM_SEND(19),{17,0,0xD7,0x02,0xAA,0x02,0xC1,0x02,0xCF,0x02,0xE3,0x02,0xEF,0x02,0xFF,0x03,0x0B,0x03,0x1B}},//new modified(modified_2_3)
	{LCM_SEND(7),{5,0,0xD8,0x03,0x3A,0x03,0x9A}},																//(modified)
	{LCM_SEND(19),{17,0,0xD9,0x00,0x65,0x00,0x88,0x00,0xC9,0x00,0xD7,0x00,0xEB,0x01,0x12,0x01,0x33,0x01,0x65}},//new modified(modified_2_3)
	{LCM_SEND(19),{17,0,0xDD,0x01,0x8B,0x01,0xC2,0x01,0xEA,0x02,0x21,0x02,0x4C,0x02,0x4D,0x02,0x72,0x02,0x96}},//new modified(modified_2_3)
	{LCM_SEND(19),{17,0,0xDE,0x02,0xAA,0x02,0xC1,0x02,0xCF,0x02,0xE3,0x02,0xEF,0x02,0xFF,0x03,0x0B,0x03,0x1B}},//new modified(modified_2_3)
	{LCM_SEND(7),{5,0,0xDF,0x03,0x3A,0x03,0x9A}},																//(modified)
	{LCM_SEND(19),{17,0,0xE0,0x00,0x3B,0x00,0x5E,0x00,0xC9,0x00,0xD7,0x00,0xEB,0x01,0x12,0x01,0x33,0x01,0x65}},//new modified(modified_2_3)
	{LCM_SEND(19),{17,0,0xE1,0x01,0x8B,0x01,0xC2,0x01,0xEA,0x02,0x21,0x02,0x4C,0x02,0x4D,0x02,0x72,0x02,0x96}},//new modified(modified_2_3)
	{LCM_SEND(19),{17,0,0xE2,0x02,0xAA,0x02,0xC1,0x02,0xCF,0x02,0xE3,0x02,0xEF,0x02,0xFF,0x03,0x0B,0x03,0x1B}},//new modified(modified_2_3)
	{LCM_SEND(7),{5,0,0xE3,0x03,0x3A,0x03,0x9A}},																//(modified)
	{LCM_SEND(19),{17,0,0xE4,0x00,0x3B,0x00,0x5E,0x00,0xC9,0x00,0xD7,0x00,0xEB,0x01,0x12,0x01,0x33,0x01,0x65}},//new modified(modified_2_3)
	{LCM_SEND(19),{17,0,0xE5,0x01,0x8B,0x01,0xC2,0x01,0xEA,0x02,0x21,0x02,0x4C,0x02,0x4D,0x02,0x72,0x02,0x96}},//new modified(modified_2_3)
	{LCM_SEND(19),{17,0,0xE6,0x02,0xAA,0x02,0xC1,0x02,0xCF,0x02,0xE3,0x02,0xEF,0x02,0xFF,0x03,0x0B,0x03,0x1B}},//new modified(modified_2_3)
	{LCM_SEND(7),{5,0,0xE7,0x03,0x3A,0x03,0x9A}},																//(modified)
	{LCM_SEND(19),{17,0,0xE8,0x00,0x3B,0x00,0x5E,0x00,0xC9,0x00,0xD7,0x00,0xEB,0x01,0x12,0x01,0x33,0x01,0x65}},//new modified(modified_2_3)
	{LCM_SEND(19),{17,0,0xE9,0x01,0x8B,0x01,0xC2,0x01,0xEA,0x02,0x21,0x02,0x4C,0x02,0x4D,0x02,0x72,0x02,0x96}},//new modified(modified_2_3)
	{LCM_SEND(19),{17,0,0xEA,0x02,0xAA,0x02,0xC1,0x02,0xCF,0x02,0xE3,0x02,0xEF,0x02,0xFF,0x03,0x0B,0x03,0x1B}},//new modified(modified_2_3)
	{LCM_SEND(7),{5,0,0xEB,0x03,0x3A,0x03,0x9A}},																//(modified)
	
	//initializing sequence
	
	{LCM_SEND(8),{6,0,0xF0,0x55,0xAA,0x52,0x08,0x00}},
	{LCM_SEND(5),{3,0,0xB1,0x16,0x00}},//{3,0,0xB1,0x16,0x08}},
 
	{LCM_SEND(5),{3,0,0xB4,0x08,0x50}},
	{LCM_SEND(4),{2,0,0xB6,0x03}},
	{LCM_SEND(5),{3,0,0xB7,0xF0,0xF0}},
	{LCM_SEND(5),{3,0,0xB8,0x01,0x07}},
	{LCM_SEND(4),{2,0,0xBC,0x02}},
	{LCM_SEND(6),{4,0,0xBB,0xFF,0xF5,0x00}},//new modified
	{LCM_SEND(21),{19,0,0xC8,0x80,0x00,0x46,0x64,0x46,0x64,0x46,0x64,0x46,0x64,0x64,0xFF,0xFF,0x64,0x64,0x64,0x64,0x64}},
	{LCM_SEND(4),{2,0,0xB3,0x20}},
	{LCM_SEND(8),{6,0,0xF0,0x55,0xAA,0x52,0x00,0x00}},
	{LCM_SEND(7),{5,0,0xFF,0xAA,0x55,0x25,0x01}},
	{LCM_SEND(4),{2,0,0x6F,0x02}},
	{LCM_SEND(4),{2,0,0xF4,0x57}},
	{LCM_SEND(4),{2,0,0x6F,0x13}},
	{LCM_SEND(4),{2,0,0xF2,0x16}},
	{LCM_SEND(7),{5,0,0xFF,0xAA,0x55,0x25,0x00}},
	{LCM_SEND(4),{2,0,0xB3,0x20}},
	{LCM_SEND(4),{2,0,0x36,0x00}},
	{LCM_SEND(4),{2,0,0x35,0x00}},
 
	{LCM_SEND(8),{6,0, 0xF0,0x55,0xAA,0x52,0x08,0x00}},
	{LCM_SEND(5),{3,0, 0xB3,0x60,0x02}},
	{LCM_SEND(8),{6,0, 0xF0,0x55,0xAA,0x52,0x08,0x01}},
	{LCM_SEND(4),{2,0, 0x6F,0x02}},
	{LCM_SEND(4),{2,0, 0xC8,0xC0}},

	{LCM_SEND(8),{6,0, 0xF0,0x55,0xAA,0x52,0x08,0x00}},
	{LCM_SEND(4),{2,0, 0xB3,0x11}},

	{LCM_SEND(2),{0x55,0x00}},
//	{LCM_SLEEP(5)},
	{LCM_SEND(2),{0x51,0x00}},
	{LCM_SEND(2),{0x53,0x2C}},

	//Display on
	{LCM_SEND(1),{0x11}},
	{LCM_SLEEP(120)},
	{LCM_SEND(1),{0x29}},
	{LCM_SLEEP(100)},
	
};

static LCM_Init_Code disp_on[] =  {
	{LCM_SEND(1),{0x11}},
	{LCM_SLEEP(120)},
	{LCM_SEND(1),{0x29}},
	{LCM_SLEEP(100)},

};

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
static int32_t nt35502_mipi_init(struct panel_spec *self)
{
	int32_t i;
	LCM_Init_Code *init = init_data;
	unsigned int tag;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_dcs_write_t mipi_dcs_write = self->info.mipi->ops->mipi_dcs_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	printk("kernel nt35502_mipi_init\n");

	mipi_set_cmd_mode();
	mipi_eotp_set(0,0);

	for(i = 0; i < ARRAY_SIZE(init_data); i++){
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

static uint32_t nt35502_readid(struct panel_spec *self)
{
//	int32_t j =0;
	uint8_t read_data[4] = {0};
	int32_t read_rtn = 0;
	uint8_t param[2] = {0};
	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_force_write_t mipi_force_write = self->info.mipi->ops->mipi_force_write;
	mipi_force_read_t mipi_force_read = self->info.mipi->ops->mipi_force_read;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	LCD_PRINT("kernel lcd_nt35502_mipi read id!\n");

	mipi_set_cmd_mode();
	mipi_eotp_set(0,0);

//	for(j = 0; j < 4; j++){
		param[0] = 0x01;
		param[1] = 0x00;
		mipi_force_write(0x04, param, 2);
		read_rtn = mipi_force_read(0xda,1,&read_data[0]);
		LCD_PRINT("lcd_nt35502_mipi read id 0xda value is 0x%x!\n",read_data[0]);
		read_rtn = mipi_force_read(0xdb,1,&read_data[1]);
		LCD_PRINT("lcd_nt35502_mipi read id 0xdb value is 0x%x!\n",read_data[1]);
		read_rtn = mipi_force_read(0xdc,1,&read_data[2]);
		LCD_PRINT("lcd_nt35502_mipi read id 0xdc value is 0x%x!\n",read_data[2]);

	//	if((0x55 == read_data[0])&&(0xbc == read_data[1])&&(0x90 == read_data[2])){
		printk("lcd_nt35502_mipi read id success!\n");
		return 0x8370;
	//		}
//	}

//	mipi_eotp_set(0,0);

//	return 0;
}


static int32_t nt35502_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	int32_t i;
	LCM_Init_Code *sleep_in_out = NULL;
	unsigned int tag;
	int32_t size = 0;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_dcs_write_t mipi_dcs_write = self->info.mipi->ops->mipi_dcs_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	printk("kernel nt35502_enter_sleep, is_sleep = %d\n", is_sleep);

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

static struct panel_operations lcd_nt35502_mipi_operations = {
	.panel_init = nt35502_mipi_init,
	.panel_readid = nt35502_readid,
	.panel_enter_sleep = nt35502_enter_sleep,
};

static struct timing_rgb lcd_nt35502_mipi_timing = {
	.hfp = 110,  /* unit: pixel */
	.hbp = 110,
	.hsync = 4,
	.vfp = 10, /*unit: line*/
	.vbp = 12,
	.vsync = 4,
};


static struct info_mipi lcd_nt35502_mipi_info = {
	.work_mode  = SPRDFB_MIPI_MODE_VIDEO,
	.video_bus_width = 24, /*18,16*/
	.lan_number = 	2,
	.phy_feq =499*1000,
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.te_pol = SPRDFB_POLARITY_POS,
	.color_mode_pol = SPRDFB_POLARITY_NEG,
	.shut_down_pol = SPRDFB_POLARITY_NEG,
	.timing = &lcd_nt35502_mipi_timing,
	.ops = NULL,
};

struct panel_spec lcd_nt35502_mipi_spec = {
	.width = 480,
	.height = 800,
	.fps = 60,
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		.mipi = &lcd_nt35502_mipi_info
	},
	.ops = &lcd_nt35502_mipi_operations,
};
struct panel_cfg lcd_nt35502_mipi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x554CC0,
	.lcd_name = "lcd_nt35502_mipi",
	.panel = &lcd_nt35502_mipi_spec,
};

//temp code
static int __init lcd_nt35502_mipi_init(void)
{
/*
	printk("nt35502_mipi_init: set lcd backlight.\n");

		 if (gpio_request(214, "LCD_BL")) {
			   printk("Failed ro request LCD_BL GPIO_%d \n",
					   214);
			   return -ENODEV;
		 }
		 gpio_direction_output(214, 1);
		 gpio_set_value(214, 1);
*/

	return sprdfb_panel_register(&lcd_nt35502_mipi);
}

subsys_initcall(lcd_nt35502_mipi_init);

