/* drivers/video/sprdfb/lcd_nt35516_mipi.c
 *
 * Support for nt35516 mipi LCD device
 *
 * Copyright (C) 2015 Spreadtrum
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


#define MAX_DATA   10

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

#define LCM_TAG_SEND  (1<< 0)
#define LCM_TAG_SLEEP (1 << 1)

static LCM_Init_Code init_data[] = {

/*{LCM_SEND(2), {0xFF,0x05}},
{LCM_SEND(2), {0xFB,0x01}},
{LCM_SEND(2), {0xc5,0x01}},  //Turn On NT50198

{LCM_SLEEP(25)},
{LCM_SEND(2), {0xFF,0xEE}},
{LCM_SEND(2), {0xFB,0x01}},
{LCM_SEND(2), {0x1F,0x45}},
{LCM_SEND(2), {0x24,0x4F}},
{LCM_SEND(2), {0x38,0xC8}},
{LCM_SEND(2), {0x39,0x2C}},
{LCM_SEND(2), {0x1E,0xBB}},
{LCM_SEND(2), {0x1D,0x0F}},
{LCM_SEND(2), {0x7E,0xB1}},
{LCM_SEND(2), {0xFF,0x00}},
{LCM_SEND(2), {0xFB,0x01}},
{LCM_SEND(2), {0x35,0x01}},
{LCM_SEND(2), {0xFF,0x01}},
{LCM_SEND(2), {0xFB,0x01}},
{LCM_SEND(2), {0x00,0x01}},
{LCM_SEND(2), {0x01,0x55}},
{LCM_SEND(2), {0x02,0x40}},
{LCM_SEND(2), {0x05,0x40}},
{LCM_SEND(2), {0x06,0x4A}},
{LCM_SEND(2), {0x07,0x24}},
{LCM_SEND(2), {0x08,0x0C}},
{LCM_SEND(2), {0x0B,0x87}},
{LCM_SEND(2), {0x0C,0x87}},
{LCM_SEND(2), {0x0E,0xB0}},
{LCM_SEND(2), {0x0F,0xB3}},
{LCM_SEND(2), {0x11,0x10}},
{LCM_SEND(2), {0x12,0x10}},
{LCM_SEND(2), {0x13,0x05}},
{LCM_SEND(2), {0x14,0x4A}},
{LCM_SEND(2), {0x15,0x18}},
{LCM_SEND(2), {0x16,0x18}},
{LCM_SEND(2), {0x18,0x00}},
{LCM_SEND(2), {0x19,0x77}},
{LCM_SEND(2), {0x1A,0x55}},
{LCM_SEND(2), {0x1B,0x13}},
{LCM_SEND(2), {0x1C,0x00}},
{LCM_SEND(2), {0x1D,0x00}},
{LCM_SEND(2), {0x1E,0x13}},
{LCM_SEND(2), {0x1F,0x00}},
{LCM_SEND(2), {0x23,0x00}},
{LCM_SEND(2), {0x24,0x00}},
{LCM_SEND(2), {0x25,0x00}},
{LCM_SEND(2), {0x26,0x00}},
{LCM_SEND(2), {0x27,0x00}},
{LCM_SEND(2), {0x28,0x00}},
{LCM_SEND(2), {0x35,0x00}},
{LCM_SEND(2), {0x66,0x00}},
{LCM_SEND(2), {0x58,0x82}},
{LCM_SEND(2), {0x59,0x02}},
{LCM_SEND(2), {0x5a,0x02}},
{LCM_SEND(2), {0x5B,0x02}},
{LCM_SEND(2), {0x5C,0x82}},
{LCM_SEND(2), {0x5D,0x82}},
{LCM_SEND(2), {0x5E,0x02}},
{LCM_SEND(2), {0x5F,0x02}},
{LCM_SEND(2), {0x72,0x31}},
{LCM_SEND(2), {0xFF,0x05}},
{LCM_SEND(2), {0xFB,0x01}},
{LCM_SEND(2), {0x00,0x01}},
{LCM_SEND(2), {0x01,0x0B}},
{LCM_SEND(2), {0x02,0x0C}},
{LCM_SEND(2), {0x03,0x09}},
{LCM_SEND(2), {0x04,0x0a}},
{LCM_SEND(2), {0x05,0x00}},
{LCM_SEND(2), {0x06,0x0F}},
{LCM_SEND(2), {0x07,0x10}},
{LCM_SEND(2), {0x08,0x00}},
{LCM_SEND(2), {0x09,0x00}},
{LCM_SEND(2), {0x0A,0x00}},
{LCM_SEND(2), {0x0B,0x00}},
{LCM_SEND(2), {0x0C,0x00}},
{LCM_SEND(2), {0x0d,0x13}},
{LCM_SEND(2), {0x0e,0x15}},
{LCM_SEND(2), {0x0f,0x17}},
{LCM_SEND(2), {0x10,0x01}},
{LCM_SEND(2), {0x11,0x0b}},
{LCM_SEND(2), {0x12,0x0c}},
{LCM_SEND(2), {0x13,0x09}},
{LCM_SEND(2), {0x14,0x0a}},
{LCM_SEND(2), {0x15,0x00}},
{LCM_SEND(2), {0x16,0x0f}},
{LCM_SEND(2), {0x17,0x10}},
{LCM_SEND(2), {0x18,0x00}},
{LCM_SEND(2), {0x19,0x00}},
{LCM_SEND(2), {0x1a,0x00}},
{LCM_SEND(2), {0x1b,0x00}},
{LCM_SEND(2), {0x1c,0x00}},
{LCM_SEND(2), {0x1d,0x13}},
{LCM_SEND(2), {0x1e,0x15}},
{LCM_SEND(2), {0x1f,0x17}},
{LCM_SEND(2), {0x20,0x00}},
{LCM_SEND(2), {0x21,0x03}},
{LCM_SEND(2), {0x22,0x01}},
{LCM_SEND(2), {0x23,0x40}},
{LCM_SEND(2), {0x24,0x40}},
{LCM_SEND(2), {0x25,0xed}},
{LCM_SEND(2), {0x29,0x58}},
{LCM_SEND(2), {0x2a,0x12}},
{LCM_SEND(2), {0x2b,0x01}},
{LCM_SEND(2), {0x4b,0x06}},
{LCM_SEND(2), {0x4c,0x11}},
{LCM_SEND(2), {0x4d,0x20}},
{LCM_SEND(2), {0x4e,0x02}},
{LCM_SEND(2), {0x4f,0x02}},
{LCM_SEND(2), {0x50,0x20}},
{LCM_SEND(2), {0x51,0x61}},
{LCM_SEND(2), {0x52,0x01}},
{LCM_SEND(2), {0x53,0x63}},
{LCM_SEND(2), {0x54,0x77}},
{LCM_SEND(2), {0x55,0xed}},
{LCM_SEND(2), {0x5b,0x00}},
{LCM_SEND(2), {0x5c,0x00}},
{LCM_SEND(2), {0x5d,0x00}},
{LCM_SEND(2), {0x5e,0x00}},
{LCM_SEND(2), {0x5f,0x15}},
{LCM_SEND(2), {0x60,0x75}},
{LCM_SEND(2), {0x61,0x00}},
{LCM_SEND(2), {0x62,0x00}},
{LCM_SEND(2), {0x63,0x00}},
{LCM_SEND(2), {0x64,0x00}},
{LCM_SEND(2), {0x65,0x00}},
{LCM_SEND(2), {0x66,0x00}},
{LCM_SEND(2), {0x67,0x00}},
{LCM_SEND(2), {0x68,0x04}},
{LCM_SEND(2), {0x69,0x00}},
{LCM_SEND(2), {0x6a,0x00}},
{LCM_SEND(2), {0x6c,0x40}},
{LCM_SEND(2), {0x75,0x01}},
{LCM_SEND(2), {0x76,0x01}},
{LCM_SEND(2), {0x7a,0x80}},
{LCM_SEND(2), {0x7b,0xc5}},
{LCM_SEND(2), {0x7c,0xd8}},
{LCM_SEND(2), {0x7d,0x60}},
{LCM_SEND(2), {0x7f,0x15}},
{LCM_SEND(2), {0x80,0x81}},
{LCM_SEND(2), {0x83,0x05}},
{LCM_SEND(2), {0x93,0x08}},
{LCM_SEND(2), {0x94,0x10}},
{LCM_SEND(2), {0x8a,0x00}},
{LCM_SEND(2), {0x9b,0x0f}},
{LCM_SEND(2), {0xea,0xff}},
{LCM_SEND(2), {0xec,0x00}},

{LCM_SEND(2), {0xff,0x01}},  //GAMMA
{LCM_SEND(2), {0xfb,0x01}},
{LCM_SEND(2), {0x75,0x00}},
{LCM_SEND(2), {0x76,0x18}},
{LCM_SEND(2), {0x77,0x00}},
{LCM_SEND(2), {0x78,0x38}},
{LCM_SEND(2), {0x79,0x00}},
{LCM_SEND(2), {0x7a,0x65}},
{LCM_SEND(2), {0x7b,0x00}},
{LCM_SEND(2), {0x7c,0x84}},
{LCM_SEND(2), {0x7d,0x00}},
{LCM_SEND(2), {0x7e,0x9b}},
{LCM_SEND(2), {0x7f,0x00}},
{LCM_SEND(2), {0x80,0xaf}},
{LCM_SEND(2), {0x81,0x00}},
{LCM_SEND(2), {0x82,0xc1}},
{LCM_SEND(2), {0x83,0x00}},
{LCM_SEND(2), {0x84,0xd2}},
{LCM_SEND(2), {0x85,0x00}},
{LCM_SEND(2), {0x86,0xdf}},
{LCM_SEND(2), {0x87,0x01}},
{LCM_SEND(2), {0x88,0x11}},
{LCM_SEND(2), {0x89,0x01}},
{LCM_SEND(2), {0x8a,0x38}},
{LCM_SEND(2), {0x8b,0x01}},
{LCM_SEND(2), {0x8c,0x76}},
{LCM_SEND(2), {0x8d,0x01}},
{LCM_SEND(2), {0x8e,0xa7}},
{LCM_SEND(2), {0x8f,0x01}},
{LCM_SEND(2), {0x90,0xf3}},
{LCM_SEND(2), {0x91,0x02}},
{LCM_SEND(2), {0x92,0x2f}},
{LCM_SEND(2), {0x93,0x02}},
{LCM_SEND(2), {0x94,0x30}},
{LCM_SEND(2), {0x95,0x02}},
{LCM_SEND(2), {0x96,0x66}},
{LCM_SEND(2), {0x97,0x02}},
{LCM_SEND(2), {0x98,0xa0}},
{LCM_SEND(2), {0x99,0x02}},
{LCM_SEND(2), {0x9a,0xc5}},
{LCM_SEND(2), {0x9b,0x02}},
{LCM_SEND(2), {0x9c,0xf8}},
{LCM_SEND(2), {0x9d,0x03}},
{LCM_SEND(2), {0x9e,0x1b}},
{LCM_SEND(2), {0x9f,0x03}},
{LCM_SEND(2), {0xa0,0x46}},
{LCM_SEND(2), {0xa2,0x03}},
{LCM_SEND(2), {0xa3,0x52}},
{LCM_SEND(2), {0xa4,0x03}},
{LCM_SEND(2), {0xa5,0x62}},
{LCM_SEND(2), {0xa6,0x03}},
{LCM_SEND(2), {0xa7,0x71}},
{LCM_SEND(2), {0xa9,0x03}},
{LCM_SEND(2), {0xaa,0x83}},
{LCM_SEND(2), {0xab,0x03}},
{LCM_SEND(2), {0xac,0x94}},
{LCM_SEND(2), {0xad,0x03}},
{LCM_SEND(2), {0xae,0xa3}},
{LCM_SEND(2), {0xaf,0x03}},
{LCM_SEND(2), {0xb0,0xad}},
{LCM_SEND(2), {0xb1,0x03}},
{LCM_SEND(2), {0xb2,0xcc}},
{LCM_SEND(2), {0xb3,0x00}},
{LCM_SEND(2), {0xb4,0x18}},
{LCM_SEND(2), {0xb5,0x00}},
{LCM_SEND(2), {0xb6,0x38}},
{LCM_SEND(2), {0xb7,0x00}},
{LCM_SEND(2), {0xb8,0x65}},
{LCM_SEND(2), {0xb9,0x00}},
{LCM_SEND(2), {0xba,0x84}},
{LCM_SEND(2), {0xbb,0x00}},
{LCM_SEND(2), {0xbc,0x9b}},
{LCM_SEND(2), {0xbd,0x00}},
{LCM_SEND(2), {0xbe,0xaf}},
{LCM_SEND(2), {0xbf,0x00}},
{LCM_SEND(2), {0xc0,0xc1}},
{LCM_SEND(2), {0xc1,0x00}},
{LCM_SEND(2), {0xc2,0xd2}},
{LCM_SEND(2), {0xc3,0x00}},
{LCM_SEND(2), {0xc4,0xdf}},
{LCM_SEND(2), {0xc5,0x01}},
{LCM_SEND(2), {0xc6,0x11}},
{LCM_SEND(2), {0xc7,0x01}},
{LCM_SEND(2), {0xc8,0x38}},
{LCM_SEND(2), {0xc9,0x01}},
{LCM_SEND(2), {0xca,0x76}},
{LCM_SEND(2), {0xcb,0x01}},
{LCM_SEND(2), {0xcc,0xa7}},
{LCM_SEND(2), {0xcd,0x01}},
{LCM_SEND(2), {0xce,0xf3}},
{LCM_SEND(2), {0xcf,0x02}},
{LCM_SEND(2), {0xd0,0x2f}},
{LCM_SEND(2), {0xd1,0x02}},
{LCM_SEND(2), {0xd2,0x30}},
{LCM_SEND(2), {0xd3,0x02}},
{LCM_SEND(2), {0xd4,0x66}},
{LCM_SEND(2), {0xd5,0x02}},
{LCM_SEND(2), {0xd6,0xa0}},
{LCM_SEND(2), {0xd7,0x02}},
{LCM_SEND(2), {0xd8,0xc5}},
{LCM_SEND(2), {0xd9,0x02}},
{LCM_SEND(2), {0xda,0xf8}},
{LCM_SEND(2), {0xdb,0x03}},
{LCM_SEND(2), {0xdc,0x1b}},
{LCM_SEND(2), {0xdd,0x03}},
{LCM_SEND(2), {0xde,0x46}},
{LCM_SEND(2), {0xdf,0x03}},
{LCM_SEND(2), {0xe0,0x52}},
{LCM_SEND(2), {0xe1,0x03}},
{LCM_SEND(2), {0xe2,0x62}},
{LCM_SEND(2), {0xe3,0x03}},
{LCM_SEND(2), {0xe4,0x71}},
{LCM_SEND(2), {0xe5,0x03}},
{LCM_SEND(2), {0xe6,0x83}},
{LCM_SEND(2), {0xe7,0x03}},
{LCM_SEND(2), {0xe8,0x94}},
{LCM_SEND(2), {0xe9,0x03}},
{LCM_SEND(2), {0xea,0xa3}},
{LCM_SEND(2), {0xeb,0x03}},
{LCM_SEND(2), {0xec,0xad}},
{LCM_SEND(2), {0xed,0x03}},
{LCM_SEND(2), {0xee,0xcc}},
{LCM_SEND(2), {0xef,0x00}},
{LCM_SEND(2), {0xf0,0x18}},
{LCM_SEND(2), {0xf1,0x00}},
{LCM_SEND(2), {0xf2,0x38}},
{LCM_SEND(2), {0xf3,0x00}},
{LCM_SEND(2), {0xf4,0x65}},
{LCM_SEND(2), {0xf5,0x00}},
{LCM_SEND(2), {0xf6,0x84}},
{LCM_SEND(2), {0xf7,0x00}},
{LCM_SEND(2), {0xf8,0x9b}},
{LCM_SEND(2), {0xf9,0x00}},
{LCM_SEND(2), {0xfa,0xaf}},
{LCM_SEND(2), {0xff,0x02}},
{LCM_SEND(2), {0xfb,0x01}},
{LCM_SEND(2), {0x00,0x00}},
{LCM_SEND(2), {0x01,0xc1}},
{LCM_SEND(2), {0x02,0x00}},
{LCM_SEND(2), {0x03,0xd2}},
{LCM_SEND(2), {0x04,0x00}},
{LCM_SEND(2), {0x05,0xdf}},
{LCM_SEND(2), {0x06,0x01}},
{LCM_SEND(2), {0x07,0x11}},
{LCM_SEND(2), {0x08,0x01}},
{LCM_SEND(2), {0x09,0x38}},
{LCM_SEND(2), {0x0a,0x01}},
{LCM_SEND(2), {0x0b,0x76}},
{LCM_SEND(2), {0x0c,0x01}},
{LCM_SEND(2), {0x0d,0xa7}},
{LCM_SEND(2), {0x0e,0x01}},
{LCM_SEND(2), {0x0f,0xf3}},
{LCM_SEND(2), {0x10,0x02}},
{LCM_SEND(2), {0x11,0x2f}},
{LCM_SEND(2), {0x12,0x02}},
{LCM_SEND(2), {0x13,0x30}},
{LCM_SEND(2), {0x14,0x02}},
{LCM_SEND(2), {0x15,0x66}},
{LCM_SEND(2), {0x16,0x02}},
{LCM_SEND(2), {0x17,0xa0}},
{LCM_SEND(2), {0x18,0x02}},
{LCM_SEND(2), {0x19,0xc5}},
{LCM_SEND(2), {0x1a,0x02}},
{LCM_SEND(2), {0x1b,0xf8}},
{LCM_SEND(2), {0x1c,0x03}},
{LCM_SEND(2), {0x1d,0x1b}},
{LCM_SEND(2), {0x1e,0x03}},
{LCM_SEND(2), {0x1f,0x46}},
{LCM_SEND(2), {0x20,0x03}},
{LCM_SEND(2), {0x21,0x52}},
{LCM_SEND(2), {0x22,0x03}},
{LCM_SEND(2), {0x23,0x62}},
{LCM_SEND(2), {0x24,0x03}},
{LCM_SEND(2), {0x25,0x71}},
{LCM_SEND(2), {0x26,0x03}},
{LCM_SEND(2), {0x27,0x83}},
{LCM_SEND(2), {0x28,0x03}},
{LCM_SEND(2), {0x29,0x94}},
{LCM_SEND(2), {0x2a,0x03}},
{LCM_SEND(2), {0x2b,0xa3}},
{LCM_SEND(2), {0x2d,0x03}},
{LCM_SEND(2), {0x2f,0xad}},
{LCM_SEND(2), {0x30,0x03}},
{LCM_SEND(2), {0x31,0xcc}},
{LCM_SEND(2), {0x32,0x00}},
{LCM_SEND(2), {0x33,0x18}},
{LCM_SEND(2), {0x34,0x00}},
{LCM_SEND(2), {0x35,0x38}},
{LCM_SEND(2), {0x36,0x00}},
{LCM_SEND(2), {0x37,0x65}},
{LCM_SEND(2), {0x38,0x00}},
{LCM_SEND(2), {0x39,0x84}},
{LCM_SEND(2), {0x3a,0x00}},
{LCM_SEND(2), {0x3b,0x9b}},
{LCM_SEND(2), {0x3d,0x00}},
{LCM_SEND(2), {0x3f,0xaf}},
{LCM_SEND(2), {0x40,0x00}},
{LCM_SEND(2), {0x41,0xc1}},
{LCM_SEND(2), {0x42,0x00}},
{LCM_SEND(2), {0x43,0xd2}},
{LCM_SEND(2), {0x44,0x00}},
{LCM_SEND(2), {0x45,0xdf}},
{LCM_SEND(2), {0x46,0x01}},
{LCM_SEND(2), {0x47,0x11}},
{LCM_SEND(2), {0x48,0x01}},
{LCM_SEND(2), {0x49,0x38}},
{LCM_SEND(2), {0x4a,0x01}},
{LCM_SEND(2), {0x4b,0x76}},
{LCM_SEND(2), {0x4c,0x01}},
{LCM_SEND(2), {0x4d,0xa7}},
{LCM_SEND(2), {0x4e,0x01}},
{LCM_SEND(2), {0x4f,0xf3}},
{LCM_SEND(2), {0x50,0x02}},
{LCM_SEND(2), {0x51,0x2f}},
{LCM_SEND(2), {0x52,0x02}},
{LCM_SEND(2), {0x53,0x30}},
{LCM_SEND(2), {0x54,0x02}},
{LCM_SEND(2), {0x55,0x66}},
{LCM_SEND(2), {0x56,0x02}},
{LCM_SEND(2), {0x58,0xa0}},
{LCM_SEND(2), {0x59,0x02}},
{LCM_SEND(2), {0x5a,0xc5}},
{LCM_SEND(2), {0x5b,0x02}},
{LCM_SEND(2), {0x5c,0xf8}},
{LCM_SEND(2), {0x5d,0x03}},
{LCM_SEND(2), {0x5e,0x1b}},
{LCM_SEND(2), {0x5f,0x03}},
{LCM_SEND(2), {0x60,0x46}},
{LCM_SEND(2), {0x61,0x03}},
{LCM_SEND(2), {0x62,0x52}},
{LCM_SEND(2), {0x63,0x03}},
{LCM_SEND(2), {0x64,0x62}},
{LCM_SEND(2), {0x65,0x03}},
{LCM_SEND(2), {0x66,0x71}},
{LCM_SEND(2), {0x67,0x03}},
{LCM_SEND(2), {0x68,0x83}},
{LCM_SEND(2), {0x69,0x03}},
{LCM_SEND(2), {0x6a,0x94}},
{LCM_SEND(2), {0x6b,0x03}},
{LCM_SEND(2), {0x6c,0xa3}},
{LCM_SEND(2), {0x6d,0x03}},
{LCM_SEND(2), {0x6e,0xad}},
{LCM_SEND(2), {0x6f,0x03}},
{LCM_SEND(2), {0x70,0xcc}},
{LCM_SEND(2), {0x71,0x00}},
{LCM_SEND(2), {0x72,0x18}},
{LCM_SEND(2), {0x73,0x00}},
{LCM_SEND(2), {0x74,0x38}},
{LCM_SEND(2), {0x75,0x00}},
{LCM_SEND(2), {0x76,0x65}},
{LCM_SEND(2), {0x77,0x00}},
{LCM_SEND(2), {0x78,0x84}},
{LCM_SEND(2), {0x79,0x00}},
{LCM_SEND(2), {0x7a,0x9b}},
{LCM_SEND(2), {0x7b,0x00}},
{LCM_SEND(2), {0x7c,0xaf}},
{LCM_SEND(2), {0x7d,0x00}},
{LCM_SEND(2), {0x7e,0xc1}},
{LCM_SEND(2), {0x7f,0x00}},
{LCM_SEND(2), {0x80,0xd2}},
{LCM_SEND(2), {0x81,0x00}},
{LCM_SEND(2), {0x82,0xdf}},
{LCM_SEND(2), {0x83,0x01}},
{LCM_SEND(2), {0x84,0x11}},
{LCM_SEND(2), {0x85,0x01}},
{LCM_SEND(2), {0x86,0x38}},
{LCM_SEND(2), {0x87,0x01}},
{LCM_SEND(2), {0x88,0x76}},
{LCM_SEND(2), {0x89,0x01}},
{LCM_SEND(2), {0x8a,0xa7}},
{LCM_SEND(2), {0x8b,0x01}},
{LCM_SEND(2), {0x8c,0xf3}},
{LCM_SEND(2), {0x8d,0x02}},
{LCM_SEND(2), {0x8e,0x2f}},
{LCM_SEND(2), {0x8f,0x02}},
{LCM_SEND(2), {0x90,0x30}},
{LCM_SEND(2), {0x91,0x02}},
{LCM_SEND(2), {0x92,0x66}},
{LCM_SEND(2), {0x93,0x02}},
{LCM_SEND(2), {0x94,0xa0}},
{LCM_SEND(2), {0x95,0x02}},
{LCM_SEND(2), {0x96,0xc5}},
{LCM_SEND(2), {0x97,0x02}},
{LCM_SEND(2), {0x98,0xf8}},
{LCM_SEND(2), {0x99,0x03}},
{LCM_SEND(2), {0x9a,0x1b}},
{LCM_SEND(2), {0x9b,0x03}},
{LCM_SEND(2), {0x9c,0x46}},
{LCM_SEND(2), {0x9d,0x03}},
{LCM_SEND(2), {0x9e,0x52}},
{LCM_SEND(2), {0x9f,0x03}},
{LCM_SEND(2), {0xa0,0x62}},
{LCM_SEND(2), {0xa2,0x03}},
{LCM_SEND(2), {0xa3,0x71}},
{LCM_SEND(2), {0xa4,0x03}},
{LCM_SEND(2), {0xa5,0x83}},
{LCM_SEND(2), {0xa6,0x03}},
{LCM_SEND(2), {0xa7,0x94}},
{LCM_SEND(2), {0xa9,0x03}},
{LCM_SEND(2), {0xaa,0xa3}},
{LCM_SEND(2), {0xab,0x03}},
{LCM_SEND(2), {0xac,0xad}},
{LCM_SEND(2), {0xad,0x03}},
{LCM_SEND(2), {0xae,0xcc}},
{LCM_SEND(2), {0xaf,0x00}},
{LCM_SEND(2), {0xb0,0x18}},
{LCM_SEND(2), {0xb1,0x00}},
{LCM_SEND(2), {0xb2,0x38}},
{LCM_SEND(2), {0xb3,0x00}},
{LCM_SEND(2), {0xb4,0x65}},
{LCM_SEND(2), {0xb5,0x00}},
{LCM_SEND(2), {0xb6,0x84}},
{LCM_SEND(2), {0xb7,0x00}},
{LCM_SEND(2), {0xb8,0x9b}},
{LCM_SEND(2), {0xb9,0x00}},
{LCM_SEND(2), {0xba,0xaf}},
{LCM_SEND(2), {0xbb,0x00}},
{LCM_SEND(2), {0xbc,0xc1}},
{LCM_SEND(2), {0xbd,0x00}},
{LCM_SEND(2), {0xbe,0xd2}},
{LCM_SEND(2), {0xbf,0x00}},
{LCM_SEND(2), {0xc0,0xdf}},
{LCM_SEND(2), {0xc1,0x01}},
{LCM_SEND(2), {0xc2,0x11}},
{LCM_SEND(2), {0xc3,0x01}},
{LCM_SEND(2), {0xc4,0x38}},
{LCM_SEND(2), {0xc5,0x01}},
{LCM_SEND(2), {0xc6,0x76}},
{LCM_SEND(2), {0xc7,0x01}},
{LCM_SEND(2), {0xc8,0xa7}},
{LCM_SEND(2), {0xc9,0x01}},
{LCM_SEND(2), {0xca,0xf3}},
{LCM_SEND(2), {0xcb,0x02}},
{LCM_SEND(2), {0xcc,0x2f}},
{LCM_SEND(2), {0xcd,0x02}},
{LCM_SEND(2), {0xce,0x30}},
{LCM_SEND(2), {0xcf,0x02}},
{LCM_SEND(2), {0xd0,0x66}},
{LCM_SEND(2), {0xd1,0x02}},
{LCM_SEND(2), {0xd2,0xa0}},
{LCM_SEND(2), {0xd3,0x02}},
{LCM_SEND(2), {0xd4,0xc5}},
{LCM_SEND(2), {0xd6,0xf8}},
{LCM_SEND(2), {0xd7,0x03}},
{LCM_SEND(2), {0xd8,0x1b}},
{LCM_SEND(2), {0xd9,0x03}},
{LCM_SEND(2), {0xda,0x46}},
{LCM_SEND(2), {0xdb,0x03}},
{LCM_SEND(2), {0xdc,0x52}},
{LCM_SEND(2), {0xdd,0x03}},
{LCM_SEND(2), {0xde,0x62}},
{LCM_SEND(2), {0xdf,0x03}},
{LCM_SEND(2), {0xe0,0x71}},
{LCM_SEND(2), {0xe1,0x03}},
{LCM_SEND(2), {0xe2,0x83}},
{LCM_SEND(2), {0xe3,0x03}},
{LCM_SEND(2), {0xe4,0x94}},
{LCM_SEND(2), {0xe5,0x03}},
{LCM_SEND(2), {0xe6,0xa3}},
{LCM_SEND(2), {0xe7,0x03}},
{LCM_SEND(2), {0xe8,0xad}},
{LCM_SEND(2), {0xe9,0x03}},
{LCM_SEND(2), {0xea,0xcc}},  //GAMMA END

{LCM_SEND(2), {0xff,0x01}},
{LCM_SEND(2), {0xfb,0x01}},
{LCM_SEND(2), {0xff,0x02}},
{LCM_SEND(2), {0xfb,0x01}},
{LCM_SEND(2), {0xff,0x04}},
{LCM_SEND(2), {0xfb,0x01}},
{LCM_SEND(2), {0xff,0x00}},
//{LCM_SEND(2), {0xD3,0x05}},  //VBP VFP VSPW
{LCM_SEND(2), {0xD3,0x06}},  //VBP VFP VSPW
{LCM_SEND(2), {0xD4,0x04}},*/

{LCM_SEND(1), {0x11}},       //sleep out
{LCM_SLEEP(150)},

{LCM_SEND(2), {0xff,0x00}},  //Tearing Effect Line ON
{LCM_SEND(2), {0x35,0x00}},

{LCM_SEND(1), {0x29}},      //display on
{LCM_SLEEP(100)},

/*{LCM_SEND(2), {0x51,0x00}},  //CABC setting
{LCM_SEND(2), {0x5e,0x00}},
{LCM_SEND(2), {0x53,0x2c}},
{LCM_SEND(2), {0x55,0x00}},*/
};

//static LCM_Init_Code disp_on =  {LCM_SEND(1), {0x29}};

static LCM_Init_Code sleep_in[] =  {
{LCM_SEND(1), {0x28}},
{LCM_SLEEP(10)},
{LCM_SEND(1), {0x10}},
{LCM_SLEEP(120)},
//{LCM_SEND(2), {0x4f, 0x01}}, /* this code may cause not resume bug*/
};

static LCM_Init_Code sleep_out[] =  {
{LCM_SEND(1), {0x11}},
{LCM_SLEEP(120)},
{LCM_SEND(1), {0x29}},
{LCM_SLEEP(20)},
};

static LCM_Force_Cmd_Code rd_prep_code[]={
//	{0x39, {LCM_SEND(8), {0x6, 0, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01}}},
	{0x37, {LCM_SEND(2), {0x3, 0}}},
};

static LCM_Force_Cmd_Code rd_prep_code_1[]={
	{0x37, {LCM_SEND(2), {0x1, 0}}},
};
static int32_t nt35596h_rdc_mipi_init(struct panel_spec *self)
{
	int32_t i = 0;
	LCM_Init_Code *init = init_data;
	unsigned int tag;

	mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
	mipi_gen_write_t mipi_gen_write = self->info.mipi->ops->mipi_gen_write;

	pr_debug(KERN_DEBUG "sprdfb:nt35596h_rdc_mipi_init\n");

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

static uint32_t nt35596h_rdc_readid(struct panel_spec *self)
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

	printk("sprdfb:lcd_nt35596h_rdc_mipi read id!\n");

	mipi_set_cmd_mode();
	mipi_eotp_set(0,1);

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
		read_rtn = mipi_force_read(0xDC, 1,(uint8_t *)read_data);
		printk("lcd_nt35596h_rdc_mipi read id 0xDC value is 0x%x, 0x%x, 0x%x!\n", read_data[0], read_data[1], read_data[2]);

		if(0x86 == read_data[0]){
			printk("lcd_nt35596h_rdc_mipi read id success!\n");
			mipi_eotp_set(1,1);
			return 0x86;
		}
	}
	mipi_eotp_set(1,1);
	return 0x0;
}

static int32_t nt35596h_rdc_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	int32_t i = 0;
	LCM_Init_Code *sleep_in_out = NULL;
	unsigned int tag;
	int32_t size = 0;

	mipi_gen_write_t mipi_gen_write = self->info.mipi->ops->mipi_gen_write;

	printk(KERN_DEBUG "nt35596h_rdc_enter_sleep, is_sleep = %d\n", is_sleep);

	if(is_sleep){
		sleep_in_out = sleep_in;
		size = ARRAY_SIZE(sleep_in);
	}else{
		sleep_in_out = sleep_out;
		size = ARRAY_SIZE(sleep_out);
	}

	for(i = 0; i <size ; i++){
		tag = (sleep_in_out->tag >>24);
		if(tag & LCM_TAG_SEND){
			mipi_gen_write(sleep_in_out->data, (sleep_in_out->tag & LCM_TAG_MASK));
		}else if(tag & LCM_TAG_SLEEP){
			msleep((sleep_in_out->tag & LCM_TAG_MASK));
		}
		sleep_in_out++;
	}
	return 0;
}

static uint32_t nt35596h_rdc_readpowermode(struct panel_spec *self)
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

	pr_debug("lcd_nt35596h_rdc_mipi read power mode!\n");
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
		read_rtn = mipi_force_read(0x0A, 1,(uint8_t *)read_data);
		//printk("lcd_nt35516 mipi read power mode 0x0A value is 0x%x! , read result(%d)\n", read_data[0], read_rtn);
		if(0x08 == read_data[0]){
			pr_debug("lcd_nt35596h_rdc_mipi read power mode success!\n");
			mipi_eotp_set(1,1);
			return 0x9c;
		}
	}

	printk("lcd_nt35596h_rdc mipi read power mode fail!0x0A value is 0x%x! , read result(%d)\n", read_data[0], read_rtn);
	mipi_eotp_set(1,1);
	return 0x0;
}

static int32_t nt35596h_rdc_check_esd(struct panel_spec *self)
{
	uint32_t power_mode;

#ifndef FB_CHECK_ESD_IN_VFP
	mipi_set_lp_mode_t mipi_set_data_lp_mode = self->info.mipi->ops->mipi_set_data_lp_mode;
	mipi_set_hs_mode_t mipi_set_data_hs_mode = self->info.mipi->ops->mipi_set_data_hs_mode;
	mipi_set_lp_mode_t mipi_set_lp_mode = self->info.mipi->ops->mipi_set_lp_mode;
	mipi_set_hs_mode_t mipi_set_hs_mode = self->info.mipi->ops->mipi_set_hs_mode;
	uint16_t work_mode = self->info.mipi->work_mode;
#endif

	pr_debug("nt35596h_rdc_check_esd!\n");
#ifndef FB_CHECK_ESD_IN_VFP
	if(SPRDFB_MIPI_MODE_CMD==work_mode){
		mipi_set_lp_mode();
	}else{
		mipi_set_data_lp_mode();
	}
#endif
	power_mode = nt35596h_rdc_readpowermode(self);
	//power_mode = 0x0;
#ifndef FB_CHECK_ESD_IN_VFP
	if(SPRDFB_MIPI_MODE_CMD==work_mode){
		mipi_set_hs_mode();
	}else{
		mipi_set_data_hs_mode();
	}
#endif
	if(power_mode == 0x9c){
		pr_debug("nt35596h_rdc_check_esd OK!\n");
		return 1;
	}else{
		printk("nt35596h_rdc_check_esd fail!(0x%x)\n", power_mode);
		return 0;
	}
}

static struct panel_operations lcd_nt35596h_rdc_mipi_operations = {
	.panel_init = nt35596h_rdc_mipi_init,
	.panel_readid = nt35596h_rdc_readid,
	.panel_enter_sleep = nt35596h_rdc_enter_sleep,
	.panel_esd_check = nt35596h_rdc_check_esd,
};

static struct timing_rgb lcd_nt35596h_rdc_mipi_timing = {
	.hfp = 30,  /* unit: pixel */
	.hbp = 30,
	.hsync = 30,//4,
	.vfp = 8, /*unit: line*/
	.vbp = 3,
	.vsync = 3,
};

static struct info_mipi lcd_nt35596h_rdc_mipi_info = {
	.work_mode  = SPRDFB_MIPI_MODE_VIDEO,
	.video_bus_width = 24, /*18,16*/
	.lan_number = 4,
	.phy_feq = 850*1000,
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.te_pol = SPRDFB_POLARITY_POS,
	.color_mode_pol = SPRDFB_POLARITY_NEG,
	.shut_down_pol = SPRDFB_POLARITY_NEG,
	.timing = &lcd_nt35596h_rdc_mipi_timing,
	.ops = NULL,
};

struct panel_spec lcd_nt35596h_rdc_mipi_spec = {
	.width = 1080,
	.height = 1920,
	.fps = 45,
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
	.is_clean_lcd = true,
	.info = {
		.mipi = &lcd_nt35596h_rdc_mipi_info
	},
	.ops = &lcd_nt35596h_rdc_mipi_operations,
};

struct panel_cfg lcd_nt35596h_rdc_mipi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x86,
	.lcd_name = "lcd_nt35596h_rdc_mipi",
	.panel = &lcd_nt35596h_rdc_mipi_spec,
};

static int __init lcd_nt35596h_rdc_mipi_init(void)
{
	return sprdfb_panel_register(&lcd_nt35596h_rdc_mipi);
}

subsys_initcall(lcd_nt35596h_rdc_mipi_init);
