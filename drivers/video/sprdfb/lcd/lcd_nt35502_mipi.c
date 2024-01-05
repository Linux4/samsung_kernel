#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include "../sprdfb_panel.h"
#include <mach/gpio.h>
#include <linux/platform_device.h>
#include <linux/err.h>

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


#define CONFIG_LDI_SUPPORT_MDNIE

#ifdef CONFIG_LDI_SUPPORT_MDNIE
#include "lcd_nt35502_mdnie_param.h"

static int isReadyTo_mDNIe = 1;
static struct mdnie_config mDNIe_cfg;
static void set_mDNIe_Mode(struct mdnie_config *mDNIeCfg);
#ifdef ENABLE_MDNIE_TUNING
#define TUNING_FILE_PATH "/sdcard/mdnie/"
#define MAX_TUNING_FILE_NAME 100
#define NR_MDNIE_DATA 115
static int tuning_enable;
static char tuning_filename[MAX_TUNING_FILE_NAME];
static unsigned char mDNIe_data[NR_MDNIE_DATA] = {113,0,REG_MDNIE, 0x00};
static LCM_Mdnie_Code video_display_mDNIe_cmds[] =
{
	{LCM_SEND(NR_MDNIE_DATA), mDNIe_data},
};
#endif	/* ENABLE_MDNIE_TUNING */
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */
static LCM_Init_Code auo_init_data[] = {
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

	//Display on
	{LCM_SEND(1),{0x11}},
	{LCM_SLEEP(120)},
	{LCM_SEND(1),{0x29}},
	{LCM_SLEEP(10)},
//	{LCM_SEND(8),{6,0,0xF0,0x55,0xAA,0x52,0x08,0x01}},  
//	{LCM_SEND(7),{5,0,0xFF,0xAA,0x55,0x25,0x00}}, 
	
};

static LCM_Init_Code auo_disp_on[] =  {  
	{LCM_SEND(1),{0x11}},
	{LCM_SLEEP(120)},
	{LCM_SEND(1),{0x29}},
	

};

static LCM_Init_Code auo_sleep_in[] =  {
	{LCM_SEND(1), {0x28}},
	
	{LCM_SEND(1), {0x10}},
	{LCM_SLEEP(70)},	//>150ms
	{LCM_SEND(5),{3,0, 0x4F,0x01}}, 
};
static LCM_Init_Code auo_sleep_out[] =  {
	{LCM_SEND(1), {0x11}},
	{LCM_SLEEP(120)},//>120ms
	{LCM_SEND(1), {0x29}},
	{LCM_SLEEP(20)}, //>20ms
};
static LCM_Force_Cmd_Code rd_prep_code_1[]={
	{0x37, {LCM_SEND(2), {0x1, 0}}},
};

static LCM_Force_Cmd_Code rd_prep_code_3[]={
	{0x37, {LCM_SEND(2), {0x3, 0}}},
};
static LCM_Force_Cmd_Code rd_prep_code_4[]={
	
	{0x37, {LCM_SEND(2), {0x4, 0}}},
};

static LCM_Init_Code dtc_init_data[] = {
    //Power setting Sequence
	{LCM_SEND(8),{6,0,0xF0,0x55,0xAA,0x52,0x08,0x01}},
	 {LCM_SEND(4),{2,0,0xB0,0x0C}},
	 {LCM_SEND(4),{2,0,0xB2,0x41}},
	 {LCM_SEND(4),{2,0,0xB3,0x1E}},
	 {LCM_SEND(4),{2,0,0xB4,0x19}},
	 {LCM_SEND(4),{2,0,0xB5,0x64}},
	 {LCM_SEND(4),{2,0,0xB9,0x36}},
	 {LCM_SEND(4),{2,0,0xBA,0x26}},
	 {LCM_SEND(5),{3,0,0xBC,0x66,0x00}},
	 {LCM_SEND(5),{3,0,0xBD,0x66,0x00}},
	 {LCM_SEND(4),{2,0,0x6F,0x02}},
	 {LCM_SEND(4),{2,0,0xC8,0xC0}},
	
	 //Gamma setting
	{LCM_SEND(19),{17,0,0xD1,0x00,0x00,0x00,0x08,0x00,0x0C,0x00,0x52,0x00,0x81,0x00,0x91,0x00,0xA2,0x01,0x12}},//R+ SETTING
	{LCM_SEND(19),{17,0,0xD2,0x01,0x44,0x01,0x8A,0x01,0xB5,0x01,0xF6,0x02,0x29,0x02,0x2A,0x02,0x51,0x02,0x78}},//R+ SETTING
	{LCM_SEND(19),{17,0,0xD3,0x02,0x8C,0x02,0xA2,0x02,0xB2,0x02,0xC2,0x02,0xC4,0x02,0xD8,0x02,0xD9,0x02,0xDC}},//R+ SETTING
	{LCM_SEND(7),{5,0,0xD4,0x02,0xF1,0x02,0xF4}},//R+ SETTING
	{LCM_SEND(19),{17,0,0xE0,0x00,0x00,0x00,0x08,0x00,0x0C,0x00,0x52,0x00,0x81,0x00,0x91,0x00,0xA2,0x01,0x12}},//R- SETTING
	{LCM_SEND(19),{17,0,0xE1,0x01,0x44,0x01,0x8A,0x01,0xB5,0x01,0xF6,0x02,0x29,0x02,0x2A,0x02,0x51,0x02,0x78}},//R- SETTING
	{LCM_SEND(19),{17,0,0xE2,0x02,0x8C,0x02,0xA2,0x02,0xB2,0x02,0xC2,0x02,0xC4,0x02,0xD8,0x02,0xD9,0x02,0xDC}},//R- SETTING
	{LCM_SEND(7),{5,0,0xE3,0x02,0xF1,0x02,0xF4}},//R- SETTING							
	{LCM_SEND(19),{17,0,0xD5,0x00,0x00,0x00,0x06,0x00,0x08,0x00,0x5E,0x00,0x7D,0x00,0x9D,0x00,0xED,0x01,0x19}},//G+ SETTING
	{LCM_SEND(19),{17,0,0xD6,0x01,0x57,0x01,0x96,0x01,0xBD,0x01,0xFC,0x02,0x2B,0x02,0x2D,0x02,0x53,0x02,0x78}},//G+ SETTING
	{LCM_SEND(19),{17,0,0xD7,0x02,0x8E,0x02,0xA5,0x02,0xB2,0x02,0xC4,0x02,0xCC,0x02,0xD8,0x02,0xD9,0x02,0xF0}},//G+ SETTING
	{LCM_SEND(7),{5,0,0xD8,0x02,0xF2,0x02,0xF6}},//G+ SETTING
	{LCM_SEND(19),{17,0,0xE4,0x00,0x00,0x00,0x06,0x00,0x08,0x00,0x5E,0x00,0x7D,0x00,0x9D,0x00,0xED,0x01,0x19}},//G- SETTING
	{LCM_SEND(19),{17,0,0xE5,0x01,0x57,0x01,0x96,0x01,0xBD,0x01,0xFC,0x02,0x2B,0x02,0x2D,0x02,0x53,0x02,0x78}},//G- SETTING
	{LCM_SEND(19),{17,0,0xE6,0x02,0x8E,0x02,0xA5,0x02,0xB2,0x02,0xC4,0x02,0xCC,0x02,0xD8,0x02,0xD9,0x02,0xF0}},//G- SETTING
	{LCM_SEND(7),{5,0,0xE7,0x02,0xF2,0x02,0xF6}},//G- SETTING
	{LCM_SEND(19),{17,0,0xD9,0x00,0x00,0x00,0x03,0x00,0x0A,0x00,0x50,0x00,0x7F,0x00,0x92,0x00,0xC8,0x01,0x19}},//B+ SETTING
	{LCM_SEND(19),{17,0,0xDD,0x01,0x57,0x01,0x99,0x01,0xC2,0x02,0x06,0x02,0x32,0x02,0x36,0x02,0x5D,0x02,0x86}},//B+ SETTING
	{LCM_SEND(19),{17,0,0xDE,0x02,0x98,0x02,0xAE,0x02,0xBC,0x02,0xCE,0x02,0xD6,0x02,0xE1,0x02,0xE5,0x02,0xFA}},//B+ SETTING
	{LCM_SEND(7),{5,0,0xDF,0x02,0xFC,0x02,0xFF}},//B+ SETTING
	{LCM_SEND(19),{17,0,0xE8,0x00,0x00,0x00,0x03,0x00,0x0A,0x00,0x50,0x00,0x7F,0x00,0x92,0x00,0xC8,0x01,0x19}},//B- SETTING
	{LCM_SEND(19),{17,0,0xE9,0x01,0x57,0x01,0x99,0x01,0xC2,0x02,0x06,0x02,0x32,0x02,0x36,0x02,0x5D,0x02,0x86}},//B- SETTING
	{LCM_SEND(19),{17,0,0xEA,0x02,0x98,0x02,0xAE,0x02,0xBC,0x02,0xCE,0x02,0xD6,0x02,0xE1,0x02,0xE5,0x02,0xFA}},//B- SETTING
	{LCM_SEND(7),{5,0,0xEB,0x02,0xFC,0x02,0xFF}},//B- SETTING
	
	//initializing sequence
	
	{LCM_SEND(8),{6,0,0xF0,0x55,0xAA,0x52,0x08,0x00}},//Enable CMD2 Page0
	{LCM_SEND(5),{3,0,0xB1,0x16,0x08}},//Backward/Forward
	{LCM_SEND(5),{3,0,0xB3,0x61,0x02}},//Error out for ESD recovery
	{LCM_SEND(5),{3,0,0xB4,0x09,0x50}},//Resolution control
	{LCM_SEND(4),{2,0,0xB6,0x03}},//Source output data hold time
	{LCM_SEND(5),{3,0,0xB7,0x00,0x00}},//Gate EQ
	{LCM_SEND(5),{3,0,0xB8,0x11,0x11}},//Source EQ
	{LCM_SEND(6),{4,0,0xBB,0xFF,0xF5,0x00}},//Source Driver control
	{LCM_SEND(4),{2,0,0xBC,0x00}},//Inversion
	{LCM_SEND(13),{11,0,0xC9,0xD2,0x00,0x00,0x9A,0x00,0x80,0xC7,0x4A,0x41,0x04}},//GOA Timing
	{LCM_SEND(39),{37,0,0xEC,0x10,0x10,0x10,0x10,0x04,0x0F,0x13,0x12,0x09,0x0A,0x0B,0x0C,0x05,0x06,0x07,0x08,0x0D,0x0E,0x03,0x04,0x09,0x0A,0x0B,0x0C,0x05,0x06,0x07,0x08,0x12,0x13,0x02,0x0D,0x10,0x10,0x10,0x10}},//GOA Timing
	{LCM_SEND(39),{37,0,0xED,0x10,0x10,0x10,0x10,0x04,0x0F,0x13,0x12,0x09,0x0A,0x0B,0x0C,0x05,0x06,0x07,0x08,0x0D,0x0E,0x03,0x04,0x09,0x0A,0x0B,0x0C,0x05,0x06,0x07,0x08,0x12,0x13,0x02,0x0D,0x10,0x10,0x10,0x10}},//GOA Timing
	{LCM_SEND(7),{5,0,0xFF,0xAA,0x55,0x25,0x01}},//Enable CMD2
	{LCM_SEND(4),{2,0,0x6F,0x13}},//Set Parameter Index in MIPI
	{LCM_SEND(4),{2,0,0xF2,0x16}},
	{LCM_SEND(7),{5,0,0xFF,0xAA,0x55,0x25,0x00}},
	{LCM_SEND(8),{6,0,0xF0,0x55,0xAA,0x52,0x00,0x00}},  
	//Display on
	{LCM_SEND(1),{0x11}},
	{LCM_SLEEP(120)},
	{LCM_SEND(1),{0x29}},
	{LCM_SLEEP(10)},
	
};
#if 0
static LCM_Init_Code dtc_disp_on[] =  {  
	{LCM_SEND(1),{0x11}},
	{LCM_SLEEP(120)},
	{LCM_SEND(1),{0x29}},
};
#endif
// Sleep in same as auo
static LCM_Init_Code dtc_sleep_in[] =  {
	{LCM_SEND(1), {0x28}},

	{LCM_SEND(1), {0x10}},
	{LCM_SLEEP(70)},	//>150ms
	{LCM_SEND(5),{3,0,0x4F,0x01}}, 
};
static LCM_Init_Code dtc_sleep_out[] =  {
	{LCM_SEND(1), {0x11}},
	{LCM_SLEEP(120)},//>120ms
	{LCM_SEND(1), {0x29}},
	{LCM_SLEEP(20)}, //>20ms
};

static int32_t nt35502_mipi_init_auo(struct panel_spec *self)
{
	int32_t i;
	LCM_Init_Code *init = auo_init_data;
	unsigned int tag;
#ifdef ENABLE_MDNIE_TUNING
	LCM_Init_Code *tuning = video_display_mDNIe_cmds;
#endif

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_dcs_write_t mipi_dcs_write = self->info.mipi->ops->mipi_dcs_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	printk("kernel nt35502_mipi_init_auo\n");

	mipi_set_cmd_mode();
	mipi_eotp_set(0,0);

	for(i = 0; i < ARRAY_SIZE(auo_init_data); i++){
		tag = (init->tag >>24);
		if(tag & LCM_TAG_SEND){
			mipi_dcs_write(init->data, (init->tag & LCM_TAG_MASK));
		}else if(tag & LCM_TAG_SLEEP){
			msleep(init->tag & LCM_TAG_MASK);
		}
		init++;
	}
#ifdef  CONFIG_LDI_SUPPORT_MDNIE
	isReadyTo_mDNIe = 1;
	set_mDNIe_Mode(&mDNIe_cfg);
#ifdef ENABLE_MDNIE_TUNING
	if (tuning_enable && mDNIe_data[3]) {
		printk("%s, set mDNIe for tuning\n", __func__);
    	for(i = 0; i < ARRAY_SIZE(video_display_mDNIe_cmds); i++){
    		tag = (tuning->tag >>24);
    		if(tag & LCM_TAG_SEND){
    			mipi_dcs_write(tuning->data, (tuning->tag & LCM_TAG_MASK));
    		}else if(tag & LCM_TAG_SLEEP){
    			msleep(tuning->tag & LCM_TAG_MASK);
    		}
    		tuning++;
    	}
	}
#endif	/* ENABLE_MDNIE_TUNING */
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */
	mipi_eotp_set(1,1);
	return 0;
}

static int32_t nt35502_mipi_init_dtc(struct panel_spec *self)
{
	int32_t i;
	LCM_Init_Code *init = dtc_init_data;
	unsigned int tag;
#ifdef ENABLE_MDNIE_TUNING
	LCM_Init_Code *tuning = video_display_mDNIe_cmds;
#endif

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_dcs_write_t mipi_dcs_write = self->info.mipi->ops->mipi_dcs_write;
	mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

	printk("kernel nt35502_mipi_init_dtc\n");

	mipi_set_cmd_mode();
	mipi_eotp_set(0,0);

	for(i = 0; i < ARRAY_SIZE(dtc_init_data); i++){
		tag = (init->tag >>24);
		if(tag & LCM_TAG_SEND){
			mipi_dcs_write(init->data, (init->tag & LCM_TAG_MASK));
		}else if(tag & LCM_TAG_SLEEP){
			msleep(init->tag & LCM_TAG_MASK);
		}
		init++;
	}
#ifdef  CONFIG_LDI_SUPPORT_MDNIE
	isReadyTo_mDNIe = 1;
	set_mDNIe_Mode(&mDNIe_cfg);
#ifdef ENABLE_MDNIE_TUNING
	if (tuning_enable && mDNIe_data[3]) {
		printk("%s, set mDNIe for tuning\n", __func__);
    	for(i = 0; i < ARRAY_SIZE(video_display_mDNIe_cmds); i++){
    		tag = (tuning->tag >>24);
    		if(tag & LCM_TAG_SEND){
    			mipi_dcs_write(tuning->data, (tuning->tag & LCM_TAG_MASK));
    		}else if(tag & LCM_TAG_SLEEP){
    			msleep(tuning->tag & LCM_TAG_MASK);
    		}
    		tuning++;
    	}
	}
#endif	/* ENABLE_MDNIE_TUNING */
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */
	mipi_eotp_set(1,1);
	return 0;
}

static uint32_t nt35502_readid(struct panel_spec *self)
	{
		int32_t j =0;
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

		for(j = 0; j < 4; j++){
			param[0] = 0x01;
			param[1] = 0x00;
			mipi_force_write(0x04, param, 2);
			read_rtn = mipi_force_read(0xda,1,&read_data[0]);
			LCD_PRINT("lcd_nt35502_mipi read id 0xda value is 0x%x!\n",read_data[0]);

			read_rtn = mipi_force_read(0xdb,1,&read_data[1]);
			LCD_PRINT("lcd_nt35502_mipi read id 0xdb value is 0x%x!\n",read_data[1]);

			read_rtn = mipi_force_read(0xdc,1,&read_data[2]);
			LCD_PRINT("lcd_nt35502_mipi read id 0xdc value is 0x%x!\n",read_data[2]);

			if((0x55 == read_data[0])&&(0x4c == read_data[1])&&(0xc0 == read_data[2])){
					printk("lcd_nt35502_mipi read id success!\n");
					return 0x8370;
				}
			}

		mipi_eotp_set(0,0);

		return 0;
}


static int32_t nt35502_enter_sleep_auo(struct panel_spec *self, uint8_t is_sleep)
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
		sleep_in_out = auo_sleep_in;
		size = ARRAY_SIZE(auo_sleep_in);
		isReadyTo_mDNIe=0;
	}else{
		sleep_in_out = auo_sleep_out;
		size = ARRAY_SIZE(auo_sleep_out);
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
static int32_t nt35502_enter_sleep_dtc(struct panel_spec *self, uint8_t is_sleep)
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
		sleep_in_out = dtc_sleep_in;
		size = ARRAY_SIZE(dtc_sleep_in);
		isReadyTo_mDNIe=0;
	}else{
		sleep_in_out = dtc_sleep_out;
		size = ARRAY_SIZE(dtc_sleep_out);
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
static uint32_t nt35502_readpowermode(struct panel_spec *self)
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

	mipi_eotp_set(0,0);
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
		//printk("<0>lcd_nt35502 mipi read power mode 0x0A value is 0x%x! , read result(%d) kanas=%d\n", read_data[0], read_rtn,j);

		if((0x9c == read_data[0])  && (0 == read_rtn)){
			//printk("<0>lcd_nt35502_mipi read power mode success! kanas=%d\n",j);
			mipi_eotp_set(1,1);
			return 0x9c;
		}
	}
	mipi_eotp_set(1,1);
	return 0x0;
}

static int32_t nt35502_check_esd(struct panel_spec *self)
{
	uint32_t power_mode;
	uint16_t work_mode = self->info.mipi->work_mode;

	mipi_set_lp_mode_t mipi_set_data_lp_mode = self->info.mipi->ops->mipi_set_data_lp_mode;
	mipi_set_hs_mode_t mipi_set_data_hs_mode = self->info.mipi->ops->mipi_set_data_hs_mode;
	mipi_set_lp_mode_t mipi_set_lp_mode = self->info.mipi->ops->mipi_set_lp_mode;
	mipi_set_hs_mode_t mipi_set_hs_mode = self->info.mipi->ops->mipi_set_hs_mode;

	//printk(KERN_ERR"<0> nt35502_check_esd!\n");
	
	if(SPRDFB_MIPI_MODE_CMD==work_mode){
		mipi_set_lp_mode();
	}else{
		mipi_set_data_lp_mode();
	}
	power_mode = nt35502_readpowermode(self);
	//power_mode = 0x0;
	if(SPRDFB_MIPI_MODE_CMD==work_mode){
		mipi_set_hs_mode();
	}else{
		mipi_set_data_hs_mode();
	}
	if (power_mode == 0x9c) {
		//printk(KERN_ERR"<0>nt35502_check_esd OK!\n");
		return 1;
	} else {
		//printk(KERN_ERR"<0> nt35502_check_esd fail!(0x%x)\n", power_mode);
		return 0;
	}
	
}

static int32_t nt35502_after_suspend(struct panel_spec *self)

{
   return 0;
}

#ifdef CONFIG_LCD_ESD_RECOVERY
extern void rt4502_backlight_on(void);
extern void rt4502_backlight_off(void);

struct esd_det_info nt35502_esd_info = {
	.name = "nt35502",
	.mode = ESD_DET_INTERRUPT,
	.gpio = 105,
	.state = ESD_DET_NOT_INITIALIZED,
	.level = ESD_DET_HIGH,
	.backlight_on = rt4502_backlight_on,
	.backlight_off = rt4502_backlight_off,
};
#endif
static struct panel_operations lcd_nt35502_mipi_operations_auo = {
	.panel_init = nt35502_mipi_init_auo,
	.panel_readid = nt35502_readid,
	.panel_enter_sleep = nt35502_enter_sleep_auo,
	.panel_esd_check = nt35502_check_esd,
	.panel_after_suspend = nt35502_after_suspend,
};
static struct panel_operations lcd_nt35502_mipi_operations_dtc = {
	.panel_init = nt35502_mipi_init_dtc,
	.panel_readid = nt35502_readid,
	.panel_enter_sleep = nt35502_enter_sleep_dtc,
	.panel_esd_check = nt35502_check_esd,
	.panel_after_suspend = nt35502_after_suspend,
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

struct panel_spec lcd_nt35502_mipi_spec_auo = {
	.width = 480,
	.height = 800,
	.fps	= 60,
	.suspend_mode=SEND_SLEEP_CMD,
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
	.is_clean_lcd = true,
	.info = {
		.mipi = &lcd_nt35502_mipi_info
	},
	.ops = &lcd_nt35502_mipi_operations_auo,
#ifdef CONFIG_LCD_ESD_RECOVERY
	.esd_info = &nt35502_esd_info,
#endif
};
struct panel_spec lcd_nt35502_mipi_spec_dtc = {
	.width = 480,
	.height = 800,
	.fps	= 60,
	.suspend_mode=SEND_SLEEP_CMD,
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
	.is_clean_lcd = true,
	.info = {
		.mipi = &lcd_nt35502_mipi_info
	},
	.ops = &lcd_nt35502_mipi_operations_dtc,
#ifdef CONFIG_LCD_ESD_RECOVERY
	.esd_info = &nt35502_esd_info,
#endif
};
struct panel_cfg lcd_nt35502_mipi_auo = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x554cc0,
	.lcd_name = "lcd_nt35502_mipi_auo",
	.panel = &lcd_nt35502_mipi_spec_auo,
};
struct panel_cfg lcd_nt35502_mipi_dtc = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x55c0c0,
	.lcd_name = "lcd_nt35502_mipi_dtc",
	.panel = &lcd_nt35502_mipi_spec_dtc,
};

#ifdef CONFIG_LDI_SUPPORT_MDNIE
#ifdef ENABLE_MDNIE_TUNING
static int parse_text(char *src, int len)
{
	int i;
	int data=0, value=0, count=3, comment=0;
	char *cur_position;

	count++;
	cur_position = src;
	for(i=0; i<len; i++, cur_position++) {
		char a = *cur_position;
		switch(a) {
			case '\r':
			case '\n':
				comment = 0;
				data = 0;
				break;
			case '/':
				comment++;
				data = 0;
				break;
			case '0'...'9':
				if(comment>1)
					break;
				if(data==0 && a=='0')
					data=1;
				else if(data==2){
					data=3;
					value = (a-'0')*16;
				}
				else if(data==3){
					value += (a-'0');
					mDNIe_data[count]=value;
					printk("Tuning value[%d]=0x%02X\n", count, value);
					count++;
					data=0;
				}
				break;
			case 'a'...'f':
			case 'A'...'F':
				if(comment>1)
					break;
				if(data==2){
					data=3;
					if(a<'a') value = (a-'A'+10)*16;
					else value = (a-'a'+10)*16;
				}
				else if(data==3){
					if(a<'a') value += (a-'A'+10);
					else value += (a-'a'+10);
					mDNIe_data[count]=value;
					printk("Tuning value[%d]=0x%02X\n", count, value);
					count++;
					data=0;
				}
				break;
			case 'x':
			case 'X':
				if(data==1)
					data=2;
				break;
			default:
				if(comment==1)
					comment = 0;
				data = 0;
				break;
		}
	}

	return count;
}

static int load_tuning_data(char *filename)
{
	struct file *filp;
	char	*dp;
	long	l ;
	loff_t  pos;
	int     ret, num;
	mm_segment_t fs;

	printk("[INFO]:%s called loading file name : [%s]\n",__func__,filename);

	fs = get_fs();
	set_fs(get_ds());

	filp = filp_open(filename, O_RDONLY, 0);
	if(IS_ERR(filp))
	{
		printk(KERN_ERR "[ERROR]:File open failed %d\n", IS_ERR(filp));
		return -1;
	}

	l = filp->f_path.dentry->d_inode->i_size;
	printk("[INFO]: Loading File Size : %ld(bytes)", l);

	dp = kmalloc(l+10, GFP_KERNEL);
	if(dp == NULL){
		printk("[ERROR]:Can't not alloc memory for tuning file load\n");
		//	filp_close(filp, current->files);
		filp_close(filp, NULL);
		return -1;
	}
	pos = 0;
	memset(dp, 0, l);
	printk("[INFO] : before vfs_read()\n");
	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	printk("[INFO] : after vfs_read()\n");

	if(ret != l) {
		printk("[ERROR] : vfs_read() filed ret : %d\n",ret);
		kfree(dp);
	//	filp_close(filp, current->files);
		filp_close(filp, NULL);
		return -1;
	}

	//	filp_close(filp, current->files);
		filp_close(filp, NULL);

	set_fs(fs);
	num = parse_text(dp, l);

	if(!num) {
		printk("[ERROR]:Nothing to parse\n");
		kfree(dp);
		return -1;
	}

	printk("[INFO] : Loading Tuning Value's Count : %d", num);

	kfree(dp);
	return num;
}

ssize_t mDNIeTuning_show(struct device *dev,
		struct device_attribute *attr, char *buf)

{
	int ret = 0;
	ret = sprintf(buf,"Tunned File Name : %s\n",tuning_filename);

	return ret;
}

ssize_t mDNIeTuning_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *pt;
	char a;
	int32_t i;
	unsigned long tunning_mode=0;
	LCM_Init_Code *tuning = video_display_mDNIe_cmds;
	a = *buf;
	unsigned int tag;

	if(a=='1') {
		tuning_enable = 1;
		printk("%s:Tuning_enable\n",__func__);
	} else if(a=='0') {
		tuning_enable = 0;
		printk("%s:Tuning_disable\n",__func__);
	} else {
		memset(tuning_filename,0,sizeof(tuning_filename));
		sprintf(tuning_filename,"%s%s",TUNING_FILE_PATH,buf);

		pt = tuning_filename;
		while(*pt) {
			if(*pt =='\r'|| *pt =='\n')
			{
				*pt = 0;
				break;
			}
			pt++;
		}
		printk("%s:%s\n",__func__,tuning_filename);

		if (load_tuning_data(tuning_filename) <= 0) {
			printk("[ERROR]:load_tunig_data() failed\n");
			return size;
		}

		if (tuning_enable && mDNIe_data[3]) {
			printk("========================mDNIe!!!!!!!\n");
		for(i = 0; i < ARRAY_SIZE(video_display_mDNIe_cmds); i++){
		tag = (tuning->tag >>24);
		if(tag & LCM_TAG_SEND){
			mdnie_mipi_dcs_write(tuning->data, (tuning->tag & LCM_TAG_MASK));
		}else if(tag & LCM_TAG_SLEEP){
			msleep(tuning->tag & LCM_TAG_MASK);
		}
		tuning++;
	}
		}
	}
	return size;
}
#endif	/* ENABLE_MDNIE_TUNING */

extern uint32_t lcd_id_from_uboot;
static void set_mDNIe_Mode(struct mdnie_config *mDNIeCfg)
{
	int value;
	int32_t i;
	unsigned int tag;
	mipi_dcs_write_t mdnie_mipi_dcs_write;
	LCM_Init_Code *tuning_scenario;
	LCM_Init_Code *tuning_negative = (LCM_Init_Code*)video_display_mDNIe_scenario_cmds[SCENARIO_MAX];
    
	printk("%s:[mDNIe] LCD ID=%x, negative=%d , isReadyTo_mDNIe= %d\n", __func__, lcd_id_from_uboot, mDNIe_cfg.negative , isReadyTo_mDNIe);
	if(!lcd_id_from_uboot){
    	printk("%s:[mDNIe] LCD ID=00, retrun!!\n", __func__);
		return;
	}
	if(lcd_id_from_uboot==0x554cc0)
	mdnie_mipi_dcs_write = lcd_nt35502_mipi_spec_auo.info.mipi->ops->mipi_dcs_write;
	else
	mdnie_mipi_dcs_write = lcd_nt35502_mipi_spec_dtc.info.mipi->ops->mipi_dcs_write;
	if(!isReadyTo_mDNIe)
		return;
	if (mDNIe_cfg.negative)  {
		printk("%s : apply negative color\n", __func__);
	    	for(i = 0; i < ARRAY_SIZE(mDNie_Negative_Mode); i++) {
		    	tag = (tuning_negative->tag >>24);
		    	if (tag & LCM_TAG_SEND) {
				/*msleep(20);*/
				mdnie_mipi_dcs_write(tuning_negative->data, \
					(tuning_negative->tag & LCM_TAG_MASK));
				/*msleep(20);*/
		    	} else if (tag & LCM_TAG_SLEEP) {
		    		printk("%s :DONT apply negative color\n", __func__);
		    		msleep(tuning_negative->tag & LCM_TAG_MASK);
		    	}
			tuning_negative++;
		}
		/* wait more than 5 frames */
		msleep(84);
	} else {
		tuning_scenario = (LCM_Init_Code*)video_display_mDNIe_scenario_cmds[mDNIeCfg->scenario];
		printk("%s:[mDNIe] mDNIeCfg->scenario=%d\n", __func__, mDNIeCfg->scenario);

		for (i = 0; i < ARRAY_SIZE(mDNie_UI_Mode); i++) {
			tag = (tuning_scenario->tag >>24);
			if (tag & LCM_TAG_SEND) {
				/*msleep(20);*/
				mdnie_mipi_dcs_write(tuning_scenario->data, \
					(tuning_scenario->tag & LCM_TAG_MASK));
				/*msleep(20);*/
			} else if (tag & LCM_TAG_SLEEP) {
				msleep(tuning_scenario->tag & LCM_TAG_MASK);
			}

			tuning_scenario++;
		}
		/* wait more than 2 frames */	
		msleep(34);
	}
	
	return;
}

static ssize_t mDNIeScenario_show(struct device *dev,
		struct device_attribute *attr, char *buf)

{
	int ret;
	ret = sprintf(buf,"mDNIeScenario_show : %d\n", mDNIe_cfg.scenario);
	return ret;
}

static ssize_t mDNIeScenario_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int value;

	if (strict_strtoul(buf, 0, (unsigned long *)&value))
		return -EINVAL;

	printk("%s:value=%d\n", __func__, value);

	switch(value) {
	case UI_MODE:
	case VIDEO_MODE:
	case VIDEO_WARM_MODE:
	case VIDEO_COLD_MODE:
	case CAMERA_MODE:
	case GALLERY_MODE:
    case NAVI_MODE:
    case VT_MODE:
    case BROWSER_MODE:
    case EBOOK_MODE:
    case EMAIL_MODE:
		break;
	default:
		value = UI_MODE;
		break;
	};

	mDNIe_cfg.scenario = value;
	set_mDNIe_Mode(&mDNIe_cfg);
	return size;
}

static ssize_t mDNIeOutdoor_show(struct device *dev,
		struct device_attribute *attr, char *buf)

{
	int ret;
	ret = sprintf(buf,"mDNIeOutdoor_show : %d\n", mDNIe_cfg.outdoor);

	return ret;
}

static ssize_t mDNIeOutdoor_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int value;

	if (strict_strtoul(buf, 0, (unsigned long *)&value))
		return -EINVAL;

	printk("%s:value=%d\n", __func__, value);
	mDNIe_cfg.outdoor = value;
	set_mDNIe_Mode(&mDNIe_cfg);
	return size;
}

static ssize_t mDNIeNegative_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf,"mDNIeNegative_show : %d\n", mDNIe_cfg.negative);
	return ret;
}

static ssize_t mDNIeNegative_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int value;

	if (strict_strtoul(buf, 0, (unsigned long *)&value))
		return -EINVAL;

	printk("%s:value=%d\n", __func__, value);

	if(value == 1) {
		mDNIe_cfg.negative = 1;
	} else {
		mDNIe_cfg.negative = 0;
	}

	set_mDNIe_Mode(&mDNIe_cfg);
	return size;
}
#ifdef ENABLE_MDNIE_TUNING
static DEVICE_ATTR(tuning, 0664, mDNIeTuning_show, mDNIeTuning_store);
#endif	/* ENABLE_MDNIE_TUNING */
static DEVICE_ATTR(outdoor, 0664, mDNIeOutdoor_show, mDNIeOutdoor_store);
static DEVICE_ATTR(scenario, 0664, mDNIeScenario_show, mDNIeScenario_store);
static DEVICE_ATTR(negative, 0664, mDNIeNegative_show, mDNIeNegative_store);
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */

static int __init lcd_nt35502_mipi_init(void)
{
	if(lcd_id_from_uboot == 0x554cc0 || lcd_id_from_uboot == 0x55c0c0) {
#ifdef CONFIG_LDI_SUPPORT_MDNIE
    struct class *lcd_mDNIe_class;
	struct device *dev_t;

	lcd_mDNIe_class = class_create(THIS_MODULE, "mdnie");

	if (IS_ERR(lcd_mDNIe_class)) {
		printk("Failed to create mdnie!\n");
		return PTR_ERR( lcd_mDNIe_class );
	}

	dev_t = device_create( lcd_mDNIe_class, NULL, 0, "%s", "mdnie");

	if (device_create_file(dev_t, &dev_attr_outdoor) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_outdoor.attr.name);
	if (device_create_file(dev_t, &dev_attr_scenario) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_scenario.attr.name);
	if (device_create_file(dev_t, &dev_attr_negative) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_negative.attr.name);
#ifdef  ENABLE_MDNIE_TUNING
	if (device_create_file(dev_t, &dev_attr_tuning) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_tuning.attr.name);
#endif	/* ENABLE_MDNIE_TUNING */
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */
	}
	printk(KERN_INFO "sprdfb: [%s]:  0x%x!\n", __FUNCTION__,lcd_nt35502_mipi_auo.lcd_id);
	sprdfb_panel_register(&lcd_nt35502_mipi_auo);
	printk(KERN_INFO "sprdfb: [%s]:  0x%x!\n", __FUNCTION__,lcd_nt35502_mipi_dtc.lcd_id);
	return sprdfb_panel_register(&lcd_nt35502_mipi_dtc);
}

subsys_initcall(lcd_nt35502_mipi_init);

