/* drivers/video/sc8825/lcd_ili9881c_mipi.c
 *
 * Support for ili9881c mipi LCD device
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

#include <linux/delay.h>
#include "../sprdfb_panel.h"

//#define LCD_Delay(ms)  uDelay(ms*1000)
///#define printk printf

#define  LCD_DEBUG
#ifdef LCD_DEBUG
#define LCD_PRINT printk
#else
#define LCD_PRINT(...)
#endif

#define MAX_DATA   56

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
#if 1
{LCM_SEND(6), {4, 0,0xFF, 0x98, 0x81, 0x03}},
        {LCM_SEND(2),{0x01, 0x00}},
        {LCM_SEND(2),{0x02, 0x00}},
        {LCM_SEND(2),{0x03, 0x73}},
        {LCM_SEND(2),{0x04, 0x03}},
        {LCM_SEND(2),{0x05, 0x00}},
        {LCM_SEND(2),{0x06, 0x06}},
        {LCM_SEND(2),{0x07, 0x06}},
        {LCM_SEND(2),{0x08, 0x00}},
        {LCM_SEND(2),{0x09, 0x18}},
        {LCM_SEND(2),{0x0a, 0x04}},
        {LCM_SEND(2),{0x0b, 0x00}},
        {LCM_SEND(2),{0x0c, 0x02}},
        {LCM_SEND(2),{0x0d, 0x03}},
        {LCM_SEND(2),{0x0e, 0x00}},
        {LCM_SEND(2),{0x0f, 0x25}},
        {LCM_SEND(2),{0x10, 0x25}},
        {LCM_SEND(2),{0x11, 0x00}},
        {LCM_SEND(2),{0x12, 0x00}},
        {LCM_SEND(2),{0x13, 0x00}},
        {LCM_SEND(2),{0x14, 0x00}},
        {LCM_SEND(2),{0x15, 0x00}},
        {LCM_SEND(2),{0x16, 0x0C}},
        {LCM_SEND(2),{0x17, 0x00}},
        {LCM_SEND(2),{0x18, 0x00}},
        {LCM_SEND(2),{0x19, 0x00}},
        {LCM_SEND(2),{0x1a, 0x00}},
        {LCM_SEND(2),{0x1b, 0x00}},
        {LCM_SEND(2),{0x1c, 0x00}},
        {LCM_SEND(2),{0x1d, 0x00}},
        {LCM_SEND(2),{0x1e, 0xC0}},
        {LCM_SEND(2),{0x1f, 0x80}},
        {LCM_SEND(2),{0x20, 0x04}},
        {LCM_SEND(2),{0x21, 0x01}},
        {LCM_SEND(2),{0x22, 0x00}},
        {LCM_SEND(2),{0x23, 0x00}},
        {LCM_SEND(2),{0x24, 0x00}},
        {LCM_SEND(2),{0x25, 0x00}},
        {LCM_SEND(2),{0x26, 0x00}},
        {LCM_SEND(2),{0x27, 0x00}},
        {LCM_SEND(2),{0x28, 0x33}},
        {LCM_SEND(2),{0x29, 0x03}},
        {LCM_SEND(2),{0x2a, 0x00}},
        {LCM_SEND(2),{0x2b, 0x00}},
        {LCM_SEND(2),{0x2c, 0x00}},
        {LCM_SEND(2),{0x2d, 0x00}},
        {LCM_SEND(2),{0x2e, 0x00}},
        {LCM_SEND(2),{0x2f, 0x00}},
        {LCM_SEND(2),{0x30, 0x00}},
        {LCM_SEND(2),{0x31, 0x00}},
        {LCM_SEND(2),{0x32, 0x00}},
        {LCM_SEND(2),{0x33, 0x00}},
        {LCM_SEND(2),{0x34, 0x04}},
        {LCM_SEND(2),{0x35, 0x00}},
        {LCM_SEND(2),{0x36, 0x00}},
        {LCM_SEND(2),{0x37, 0x00}},
        {LCM_SEND(2),{0x38, 0x3C}},
        {LCM_SEND(2),{0x39, 0x00}},
        {LCM_SEND(2),{0x3a, 0x00}},
        {LCM_SEND(2),{0x3b, 0x00}},
        {LCM_SEND(2),{0x3c, 0x00}},
        {LCM_SEND(2),{0x3d, 0x00}},
        {LCM_SEND(2),{0x3e, 0x00}},
        {LCM_SEND(2),{0x3f, 0x00}},
        {LCM_SEND(2),{0x40, 0x00}},
        {LCM_SEND(2),{0x41, 0x00}},
        {LCM_SEND(2),{0x42, 0x00}},
        {LCM_SEND(2),{0x43, 0x00}},
        {LCM_SEND(2),{0x44, 0x00}},
        {LCM_SEND(2),{0x50, 0x01}},
        {LCM_SEND(2),{0x51, 0x23}},
        {LCM_SEND(2),{0x52, 0x45}},
        {LCM_SEND(2),{0x53, 0x67}},
        {LCM_SEND(2),{0x54, 0x89}},
        {LCM_SEND(2),{0x55, 0xab}},
        {LCM_SEND(2),{0x56, 0x01}},
        {LCM_SEND(2),{0x57, 0x23}},
        {LCM_SEND(2),{0x58, 0x45}},
        {LCM_SEND(2),{0x59, 0x67}},
        {LCM_SEND(2),{0x5a, 0x89}},
        {LCM_SEND(2),{0x5b, 0xab}},
        {LCM_SEND(2),{0x5c, 0xcd}},
        {LCM_SEND(2),{0x5d, 0xef}},
        {LCM_SEND(2),{0x5e, 0x11}},
        {LCM_SEND(2),{0x5f, 0x02}},
        {LCM_SEND(2),{0x60, 0x02}},
        {LCM_SEND(2),{0x61, 0x02}},
        {LCM_SEND(2),{0x62, 0x02}},
        {LCM_SEND(2),{0x63, 0x02}},
        {LCM_SEND(2),{0x64, 0x02}},
        {LCM_SEND(2),{0x65, 0x02}},
        {LCM_SEND(2),{0x66, 0x02}},
        {LCM_SEND(2),{0x67, 0x02}},
        {LCM_SEND(2),{0x68, 0x02}},
        {LCM_SEND(2),{0x69, 0x02}},
        {LCM_SEND(2),{0x6a, 0x0C}},
        {LCM_SEND(2),{0x6b, 0x02}},
        {LCM_SEND(2),{0x6c, 0x0F}},
        {LCM_SEND(2),{0x6d, 0x0E}},
        {LCM_SEND(2),{0x6e, 0x0D}},
        {LCM_SEND(2),{0x6f, 0x06}},
        {LCM_SEND(2),{0x70, 0x07}},
        {LCM_SEND(2),{0x71, 0x02}},
        {LCM_SEND(2),{0x72, 0x02}},
        {LCM_SEND(2),{0x73, 0x02}},
        {LCM_SEND(2),{0x74, 0x02}},
        {LCM_SEND(2),{0x75, 0x02}},
        {LCM_SEND(2),{0x76, 0x02}},
        {LCM_SEND(2),{0x77, 0x02}},
        {LCM_SEND(2),{0x78, 0x02}},
        {LCM_SEND(2),{0x79, 0x02}},
        {LCM_SEND(2),{0x7a, 0x02}},
        {LCM_SEND(2),{0x7b, 0x02}},
        {LCM_SEND(2),{0x7c, 0x02}},
        {LCM_SEND(2),{0x7d, 0x02}},
        {LCM_SEND(2),{0x7e, 0x02}},
        {LCM_SEND(2),{0x7f, 0x02}},
        {LCM_SEND(2),{0x80, 0x0C}},
        {LCM_SEND(2),{0x81, 0x02}},
        {LCM_SEND(2),{0x82, 0x0F}},
        {LCM_SEND(2),{0x83, 0x0E}},
        {LCM_SEND(2),{0x84, 0x0D}},
        {LCM_SEND(2),{0x85, 0x06}},
        {LCM_SEND(2),{0x86, 0x07}},
        {LCM_SEND(2),{0x87, 0x02}},
        {LCM_SEND(2),{0x88, 0x02}},
        {LCM_SEND(2),{0x89, 0x02}},
        {LCM_SEND(2),{0x8A, 0x02}},
        {LCM_SEND(6), {4, 0,0xFF, 0x98, 0x81, 0x04}},
        {LCM_SEND(2),{0x6C, 0x15}},
        {LCM_SEND(2),{0x6E, 0x22}},
        {LCM_SEND(2),{0x6F, 0x33}},
        {LCM_SEND(2),{0x3A, 0xA4}},
        {LCM_SEND(2),{0x8D, 0x0D}},
        {LCM_SEND(2),{0x87, 0xBA}},
        {LCM_SEND(2),{0x26, 0x76}},
        {LCM_SEND(2),{0xB2, 0xD1}},
        {LCM_SEND(6), {4, 0,0xFF, 0x98, 0x81, 0x01}},
        {LCM_SEND(2),{0x22, 0x0A}},
        {LCM_SEND(2),{0x53, 0xAC}},
        {LCM_SEND(2),{0x55, 0xA7}},
        {LCM_SEND(2),{0x50, 0x78}},
        {LCM_SEND(2),{0x51, 0x78}},
        {LCM_SEND(2),{0x31, 0x00}},
        {LCM_SEND(2),{0x60, 0x14}},
        {LCM_SEND(2),{0xA0, 0x2A}},
        {LCM_SEND(2),{0xA1, 0x39}},
        {LCM_SEND(2),{0xA2, 0x46}},
        {LCM_SEND(2),{0xA3, 0x0e}},
        {LCM_SEND(2),{0xA4, 0x12}},
        {LCM_SEND(2),{0xA5, 0x25}},
        {LCM_SEND(2),{0xA6, 0x19}},
        {LCM_SEND(2),{0xA7, 0x1d}},
        {LCM_SEND(2),{0xA8, 0xa6}},
        {LCM_SEND(2),{0xA9, 0x1C}},
        {LCM_SEND(2),{0xAA, 0x29}},
        {LCM_SEND(2),{0xAB, 0x85}},
        {LCM_SEND(2),{0xAC, 0x1C}},
        {LCM_SEND(2),{0xAD, 0x1B}},
        {LCM_SEND(2),{0xAE, 0x51}},
        {LCM_SEND(2),{0xAF, 0x22}},
        {LCM_SEND(2),{0xB0, 0x2d}},
        {LCM_SEND(2),{0xB1, 0x4f}},
        {LCM_SEND(2),{0xB2, 0x59}},
        {LCM_SEND(2),{0xB3, 0x3F}},
        {LCM_SEND(2),{0xC0, 0x2A}},
        {LCM_SEND(2),{0xC1, 0x3a}},
        {LCM_SEND(2),{0xC2, 0x45}},
        {LCM_SEND(2),{0xC3, 0x0e}},
        {LCM_SEND(2),{0xC4, 0x11}},
        {LCM_SEND(2),{0xC5, 0x24}},
        {LCM_SEND(2),{0xC6, 0x1a}},
        {LCM_SEND(2),{0xC7, 0x1c}},
        {LCM_SEND(2),{0xC8, 0xaa}},
        {LCM_SEND(2),{0xC9, 0x1C}},
        {LCM_SEND(2),{0xCA, 0x29}},
        {LCM_SEND(2),{0xCB, 0x96}},
        {LCM_SEND(2),{0xCC, 0x1C}},
        {LCM_SEND(2),{0xCD, 0x1B}},
        {LCM_SEND(2),{0xCE, 0x51}},
        {LCM_SEND(2),{0xCF, 0x22}},
        {LCM_SEND(2),{0xD0, 0x2b}},
        {LCM_SEND(2),{0xD1, 0x4b}},
        {LCM_SEND(2),{0xD2, 0x59}},
        {LCM_SEND(2),{0xD3, 0x3F}},
        {LCM_SEND(6), {4, 0,0xFF, 0x98, 0x81, 0x00}},
        {LCM_SEND(2),{0x35, 0x00}},
        {LCM_SEND(2),{0x11, 0x00}},
        {LCM_SLEEP(120)},
        {LCM_SEND(2),{0x29, 0x00}},
	{LCM_SLEEP(10)},
#else
	{LCM_SEND(6), {4,0,0xFF,0x98,0x81,0x03}},
	//GIP_1
	{LCM_SEND(2), {0x01,0x00}},
	{LCM_SEND(2), {0x02,0x00}},
	{LCM_SEND(2), {0x03,0x73}},
	{LCM_SEND(2), {0x04,0x73}},
	{LCM_SEND(2), {0x05,0x00}},
	{LCM_SEND(2), {0x06,0x06}},
	{LCM_SEND(2), {0x07,0x02}},   
	{LCM_SEND(2), {0x08,0x01}},
	{LCM_SEND(2), {0x09,0x01}},
	{LCM_SEND(2), {0x0a,0x01}},
	{LCM_SEND(2), {0x0b,0x01}},   
	{LCM_SEND(2), {0x0c,0x01}},
	{LCM_SEND(2), {0x0d,0x01}},
	{LCM_SEND(2), {0x0e,0x01}},   
	{LCM_SEND(2), {0x0f,0x01}},
	{LCM_SEND(2), {0x10,0x01}}, 
	{LCM_SEND(2), {0x11,0x00}},
	{LCM_SEND(2), {0x12,0x00}},
	{LCM_SEND(2), {0x13,0x01}},
	{LCM_SEND(2), {0x14,0x00}},
	{LCM_SEND(2), {0x15,0x00}},
	{LCM_SEND(2), {0x16,0x00}}, 
	{LCM_SEND(2), {0x17,0x03}}, 
	{LCM_SEND(2), {0x18,0x00}},
	{LCM_SEND(2), {0x19,0x00}},
	{LCM_SEND(2), {0x1a,0x00}},
	{LCM_SEND(2), {0x1b,0x00}},
	{LCM_SEND(2), {0x1c,0x00}},
	{LCM_SEND(2), {0x1d,0x00}},
	{LCM_SEND(2), {0x1e,0xC0}},
	{LCM_SEND(2), {0x1f,0x80}},
	{LCM_SEND(2), {0x20,0x04}},   
	{LCM_SEND(2), {0x21,0x03}},   
	{LCM_SEND(2), {0x22,0x00}},
	{LCM_SEND(2), {0x23,0x00}},
	{LCM_SEND(2), {0x24,0x00}},
	{LCM_SEND(2), {0x25,0x00}},
	{LCM_SEND(2), {0x26,0x00}},
	{LCM_SEND(2), {0x27,0x00}},
	{LCM_SEND(2), {0x28,0x33}},
	{LCM_SEND(2), {0x29,0x03}},   
	{LCM_SEND(2), {0x2a,0x00}},
	{LCM_SEND(2), {0x2b,0x00}},
	{LCM_SEND(2), {0x2c,0x00}},
	{LCM_SEND(2), {0x2d,0x00}},
	{LCM_SEND(2), {0x2e,0x00}},
	{LCM_SEND(2), {0x2f,0x00}},
	{LCM_SEND(2), {0x30,0x00}},
	{LCM_SEND(2), {0x31,0x00}},
	{LCM_SEND(2), {0x32,0x00}},
	{LCM_SEND(2), {0x33,0x00}},
	{LCM_SEND(2), {0x34,0x03}},   
	{LCM_SEND(2), {0x35,0x00}},
	{LCM_SEND(2), {0x36,0x03}},   
	{LCM_SEND(2), {0x37,0x00}},
	{LCM_SEND(2), {0x38,0x00}},
	{LCM_SEND(2), {0x39,0x00}},
	{LCM_SEND(2), {0x3a,0x00}},    
	{LCM_SEND(2), {0x3b,0x00}},   
	{LCM_SEND(2), {0x3c,0x00}},
	{LCM_SEND(2), {0x3d,0x00}},
	{LCM_SEND(2), {0x3e,0x00}},
	{LCM_SEND(2), {0x3f,0x00}},
	{LCM_SEND(2), {0x40,0x00}},
	{LCM_SEND(2), {0x41,0x00}},
	{LCM_SEND(2), {0x42,0x00}},
	{LCM_SEND(2), {0x43,0x00}},
	{LCM_SEND(2), {0x44,0x00}},
	
	//GIP_2
	{LCM_SEND(2), {0x50,0x01}},
	{LCM_SEND(2), {0x51,0x23}},
	{LCM_SEND(2), {0x52,0x45}},
	{LCM_SEND(2), {0x53,0x67}},
	{LCM_SEND(2), {0x54,0x89}},
	{LCM_SEND(2), {0x55,0xab}},
	{LCM_SEND(2), {0x56,0x01}},
	{LCM_SEND(2), {0x57,0x23}},
	{LCM_SEND(2), {0x58,0x45}},
	{LCM_SEND(2), {0x59,0x67}},
	{LCM_SEND(2), {0x5a,0x89}},
	{LCM_SEND(2), {0x5b,0xab}},
	{LCM_SEND(2), {0x5c,0xcd}},
	{LCM_SEND(2), {0x5d,0xef}},
	
	//GIP_3
	{LCM_SEND(2), {0x5e,0x10}},   
	{LCM_SEND(2), {0x5f,0x09}},
	{LCM_SEND(2), {0x60,0x08}},
	{LCM_SEND(2), {0x61,0x0F}},
	{LCM_SEND(2), {0x62,0x0E}},
	{LCM_SEND(2), {0x63,0x0D}},
	{LCM_SEND(2), {0x64,0x0C}},
	{LCM_SEND(2), {0x65,0x02}},
	{LCM_SEND(2), {0x66,0x02}},
	{LCM_SEND(2), {0x67,0x02}},
	{LCM_SEND(2), {0x68,0x02}},
	{LCM_SEND(2), {0x69,0x02}},
	{LCM_SEND(2), {0x6a,0x02}},
	{LCM_SEND(2), {0x6b,0x02}},
	{LCM_SEND(2), {0x6c,0x02}},
	{LCM_SEND(2), {0x6d,0x02}},
	{LCM_SEND(2), {0x6e,0x02}},
	{LCM_SEND(2), {0x6f,0x02}},
	{LCM_SEND(2), {0x70,0x02}},
	{LCM_SEND(2), {0x71,0x06}},
	{LCM_SEND(2), {0x72,0x07}},
	{LCM_SEND(2), {0x73,0x02}},
	{LCM_SEND(2), {0x74,0x02}},
	
	{LCM_SEND(2), {0x75,0x06}},
	{LCM_SEND(2), {0x76,0x07}},
	{LCM_SEND(2), {0x77,0x0E}},
	{LCM_SEND(2), {0x78,0x0F}},
	{LCM_SEND(2), {0x79,0x0C}},
	{LCM_SEND(2), {0x7a,0x0D}},
	{LCM_SEND(2), {0x7b,0x02}},
	{LCM_SEND(2), {0x7c,0x02}},
	{LCM_SEND(2), {0x7d,0x02}},
	{LCM_SEND(2), {0x7e,0x02}},
	{LCM_SEND(2), {0x7f,0x02}},
	{LCM_SEND(2), {0x80,0x02}},
	{LCM_SEND(2), {0x81,0x02}},
	{LCM_SEND(2), {0x82,0x02}},
	{LCM_SEND(2), {0x83,0x02}},
	{LCM_SEND(2), {0x84,0x02}},
	{LCM_SEND(2), {0x85,0x02}},
	{LCM_SEND(2), {0x86,0x02}},
	{LCM_SEND(2), {0x87,0x09}},
	{LCM_SEND(2), {0x88,0x08}},
	{LCM_SEND(2), {0x89,0x02}},
	{LCM_SEND(2), {0x8A,0x02}},
	
	//CMD_Page 4
	{LCM_SEND(6), {4,0,0xFF,0x98,0x81,0x04}},
	{LCM_SEND(2), {0x6C,0x15}},               //VCORE 1.5 
	{LCM_SEND(2), {0x6E,0x2A}},               //di_pwr_reg=0 VGH clamp 15V
	{LCM_SEND(2), {0x6F,0x34}},               //VGH:4x VGL:-3x VCL:REGULATOR
	{LCM_SEND(2), {0x8D,0x1A}},               //VGL clamp -11.13V
	
	{LCM_SEND(2), {0x3A,0xA4}},               //Power saving          
	{LCM_SEND(2), {0x87,0xBA}},               //ESD Enhance  
	{LCM_SEND(2), {0x26,0x76}},               //ESD Enhance             
	{LCM_SEND(2), {0xB2,0xD1}},               //ESD Enhance 
	
	
	//CMD_Page 1
	{LCM_SEND(6), {4,0,0xFF,0x98,0x81,0x01}},
	{LCM_SEND(2), {0x22,0x0A}},		//BGR,0x SS
	{LCM_SEND(2), {0x31,0x00}},		//column inversion
	{LCM_SEND(2), {0x40,0x33}},               // 3XVCI
	//{LCM_SEND(2), {0x41,0x34}},             // 3XVCI
	
	{LCM_SEND(2), {0x50,0xA7}},         	//VREG1OUT=4.702V
	{LCM_SEND(2), {0x51,0xA7}},         	//VREG2OUT=-4.702V
	{LCM_SEND(2), {0x53,0x47}},	    	//VCOM1  -1.2V
	{LCM_SEND(2), {0x55,0x50}},	    	//VCOM2
	
	{LCM_SEND(2), {0x60,0x00}},               //SDT
	{LCM_SEND(2), {0x61,0x05}},               //0A
	{LCM_SEND(2), {0x62,0x05}},               //0A
	{LCM_SEND(2), {0x63,0x05}},               //0A
	
	{LCM_SEND(2), {0xA0,0x00}},		//VP255	Gamma P
	{LCM_SEND(2), {0xA1,0x2E}},		//VP251
	{LCM_SEND(2), {0xA2,0x41}},		//VP247
	{LCM_SEND(2), {0xA3,0x19}},		//VP243
	{LCM_SEND(2), {0xA4,0x19}},               //VP239 
	{LCM_SEND(2), {0xA5,0x34}},               //VP231
	{LCM_SEND(2), {0xA6,0x30}},               //VP219
	{LCM_SEND(2), {0xA7,0x35}},               //VP203
	{LCM_SEND(2), {0xA8,0xB7}},               //VP175
	{LCM_SEND(2), {0xA9,0x29}},               //VP144
	{LCM_SEND(2), {0xAA,0x2D}},               //VP111
	{LCM_SEND(2), {0xAB,0xA9}},               //VP80
	{LCM_SEND(2), {0xAC,0x29}},               //VP52
	{LCM_SEND(2), {0xAD,0x26}},               //VP36
	{LCM_SEND(2), {0xAE,0x46}},               //VP24
	{LCM_SEND(2), {0xAF,0x1B}},               //VP16
	{LCM_SEND(2), {0xB0,0x26}},               //VP12
	{LCM_SEND(2), {0xB1,0x62}},               //VP8
	{LCM_SEND(2), {0xB2,0x71}},               //VP4
	{LCM_SEND(2), {0xB3,0x00}},               //VP0   越大越暗
	
	{LCM_SEND(2), {0xC0,0x00}},		//VN255 GAMMA N
	{LCM_SEND(2), {0xC1,0x30}},               //VN251     
	{LCM_SEND(2), {0xC2,0x43}},               //VN247     
	{LCM_SEND(2), {0xC3,0x1B}},               //VN243     
	{LCM_SEND(2), {0xC4,0x23}},               //VN239     
	{LCM_SEND(2), {0xC5,0x31}},               //VN231     
	{LCM_SEND(2), {0xC6,0x21}},               //VN219     
	{LCM_SEND(2), {0xC7,0x1B}},               //VN203     
	{LCM_SEND(2), {0xC8,0xBA}},               //VN175     
	{LCM_SEND(2), {0xC9,0x0D}},               //VN144     
	{LCM_SEND(2), {0xCA,0x1E}},               //VN111     
	{LCM_SEND(2), {0xCB,0x7C}},               //VN80      
	{LCM_SEND(2), {0xCC,0x07}},               //VN52      
	{LCM_SEND(2), {0xCD,0x07}},               //VN36      
	{LCM_SEND(2), {0xCE,0x52}},               //VN24      
	{LCM_SEND(2), {0xCF,0x26}},               //VN16      
	{LCM_SEND(2), {0xD0,0x2A}},               //VN12      
	{LCM_SEND(2), {0xD1,0x42}},               //VN8       
	{LCM_SEND(2), {0xD2,0x52}},               //VN4       
	{LCM_SEND(2), {0xD3,0x00}},               //VN0   越大越暗
	//CMD_Page 0
	{LCM_SEND(6), {4,0,0xFF,0x98,0x81,0x00}},
   {LCM_SEND(2), {0x11,0x00}},
   {LCM_SLEEP(120),},
   {LCM_SEND(2), {0x29,0x00}},
#endif
};

static LCM_Init_Code disp_on =  {LCM_SEND(1), {0x29}};

static LCM_Init_Code sleep_in =  {LCM_SEND(1), {0x10}};

static LCM_Init_Code sleep_out =  {LCM_SEND(1), {0x11}};

static int32_t ili9881c_mipi_init(struct panel_spec *self)
{
	int32_t i;
	LCM_Init_Code *init = init_data;
	unsigned int tag;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_gen_write_t mipi_gen_write = self->info.mipi->ops->mipi_gen_write;

	LCD_PRINT("ili9881c_init\n");

	mipi_set_cmd_mode();

	for(i = 0; i < ARRAY_SIZE(init_data); i++){
		tag = (init->tag >>24);
		if(tag & LCM_TAG_SEND){
			mipi_gen_write(init->data, (init->tag & LCM_TAG_MASK));
		}else if(tag & LCM_TAG_SLEEP){
			mdelay((init->tag & LCM_TAG_MASK));
		}
		init++;
	}
	return 0;
}

static LCM_Force_Cmd_Code rd_prep_code[]={
        {0x39, {LCM_SEND(6), {0x6, 0, 0xFF, 0x98, 0x81, 0x01}}},
        {0x37, {LCM_SEND(2), {0x3, 0}}},
};

static uint32_t ili9881c_readid(struct panel_spec *self)
{
        /*Jessica TODO: need read id*/
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
 

        LCD_PRINT("lcd_ili9881c_mipi read id!\n");
 

        mipi_set_cmd_mode();
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
                mipi_eotp_set(0,0);
                read_rtn = mipi_force_read(0x00, 1,(uint8_t *)&read_data[0]);
                read_rtn = mipi_force_read(0x01, 1,(uint8_t *)&read_data[1]);
                mipi_eotp_set(1,1);
                LCD_PRINT("lcd_ili9881c_mipi read id 0x00 value is 0x%x, 0x%x, 0x%x!\n", read_data[0], read_data[1], read_data[2]);
 

                if((0x98 == read_data[0])&&(0x81 == read_data[1])){
                        LCD_PRINT("lcd_ili9881c_mipi read id success!\n");
                        return 0x9881;
                }
        }
        
        return 0;
}

static struct panel_operations lcd_ili9881c_mipi_operations = {
	.panel_init = ili9881c_mipi_init,
	.panel_readid = ili9881c_readid,
};

static struct timing_rgb lcd_ili9881c_mipi_timing = {
	.hfp = 60,  /* unit: pixel */
	.hbp = 60,
	.hsync = 20,
	.vfp = 20, /*unit: line*/
	.vbp = 20,
	.vsync = 6,
};

static struct info_mipi lcd_ili9881c_mipi_info = {
	.work_mode  = SPRDFB_MIPI_MODE_VIDEO,
	.video_bus_width = 24, /*18,16*/
	.lan_number = 4,
	.phy_feq = 500*1000,
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.te_pol = SPRDFB_POLARITY_POS,
	.color_mode_pol = SPRDFB_POLARITY_NEG,
	.shut_down_pol = SPRDFB_POLARITY_NEG,
	.timing = &lcd_ili9881c_mipi_timing,
	.ops = NULL,
};

struct panel_spec lcd_ili9881c_mipi_spec = {
	.width = 720,
	.height = 1280,
	.fps = 60,
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		.mipi = &lcd_ili9881c_mipi_info
	},
	.ops = &lcd_ili9881c_mipi_operations,
};

struct panel_cfg lcd_ili9881c_mipi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x9881,
	.lcd_name = "lcd_ili9881c_mipi",
	.panel = &lcd_ili9881c_mipi_spec,
};

static int __init lcd_ili9881c_mipi_init(void)
{
	return sprdfb_panel_register(&lcd_ili9881c_mipi);
}

subsys_initcall(lcd_ili9881c_mipi_init);
