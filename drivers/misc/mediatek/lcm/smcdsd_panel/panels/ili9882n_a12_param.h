/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ILI9882N_PARAM_H__
#define __ILI9882N_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>

#define EXTEND_BRIGHTNESS	306
#define UI_MAX_BRIGHTNESS	255
#define UI_DEFAULT_BRIGHTNESS	128

#define LDI_REG_BRIGHTNESS	0x51
#define LDI_REG_ID		0xDA
#define LDI_LEN_BRIGHTNESS	((u16)ARRAY_SIZE(SEQ_ILI9882N_BRIGHTNESS))

/* len is read length */
#define LDI_LEN_ID		3

#define TYPE_WRITE		I2C_SMBUS_WRITE
#define TYPE_DELAY		U8_MAX

static u8 LM36274_INIT[] = {
	TYPE_WRITE, 0x0C, 0x2C,
	TYPE_WRITE, 0x0D, 0x26,
	TYPE_WRITE, 0x0E, 0x26,
	TYPE_WRITE, 0x09, 0xBE,
	TYPE_WRITE, 0x02, 0x6B,
	TYPE_WRITE, 0x03, 0x0D,
	TYPE_WRITE, 0x11, 0x74,
	TYPE_WRITE, 0x04, 0x05,
	TYPE_WRITE, 0x05, 0xCC,
	TYPE_WRITE, 0x10, 0x67,
	TYPE_WRITE, 0x08, 0x13,
};

static u8 LM36274_EXIT[] = {
	TYPE_WRITE, 0x09, 0x18,
	TYPE_DELAY, 1, 0,
	TYPE_WRITE, 0x08, 0x00,
};

struct lcd_seq_info {
	unsigned char	*cmd;
	unsigned int	len;
	unsigned int	sleep;
};

static unsigned char SEQ_ILI9882N_SLEEP_OUT[] = { 0x11 };
static unsigned char SEQ_ILI9882N_SLEEP_IN[] = { 0x10 };
static unsigned char SEQ_ILI9882N_DISPLAY_OFF[] = { 0x28 };
static unsigned char SEQ_ILI9882N_DISPLAY_ON[] = { 0x29 };

static unsigned char SEQ_ILI9882N_BRIGHTNESS[] = {
	0x51,
	0x00, 0x00,
};

static unsigned char SEQ_ILI9882N_BRIGHTNESS_MODE[] = {
	0x53,
	0x2C,
};

static unsigned char SEQ_ILI9882N_01[] = {
	0xFF,
	0x98, 0x82, 0x00,
};

#if 0
static unsigned char SEQ_ILI9882N_02[] = {
	0x51,
	0x0F, 0xFF,
};

static unsigned char SEQ_ILI9882N_03[] = {
	0x53,
	0x2C,
};
#endif

static unsigned char SEQ_ILI9882N_04[] = {
	0x35,
	0x00,
};

static unsigned char SEQ_ILI9882N_05[] = {
	0x55,
	0x01,
};

static unsigned char SEQ_ILI9882N_06[] = {
	0xFF,
	0x98, 0x82, 0x03,
};

static unsigned char SEQ_ILI9882N_07[] = {
	0x80,
	0x36,
};

static unsigned char SEQ_ILI9882N_08[] = {
	0x83,
	0x60,
};

static unsigned char SEQ_ILI9882N_09[] = {
	0x84,
	0x00,
};

static unsigned char SEQ_ILI9882N_10[] = {
	0x88,
	0xE6,
};

static unsigned char SEQ_ILI9882N_11[] = {
	0x89,
	0xEE,
};

static unsigned char SEQ_ILI9882N_12[] = {
	0x8A,
	0xF6,
};

static unsigned char SEQ_ILI9882N_13[] = {
	0x8B,
	0xF6,
};

static unsigned char SEQ_ILI9882N_14[] = {
	0xC6,
	0x14,
};

static unsigned char SEQ_ILI9882N_15[] = {
	0xFF,
	0x98, 0x82, 0x06,
};

static unsigned char SEQ_ILI9882N_16[] = {
	0xD6,
	0x00,
};

static unsigned char SEQ_ILI9882N_17[] = {
	0xD7,
	0x11,
};

static unsigned char SEQ_ILI9882N_18[] = {
	0x48,
	0x35,
};

static unsigned char SEQ_ILI9882N_19[] = {
	0x08,
	0x21,
};

static unsigned char SEQ_ILI9882N_20[] = {
	0xD9,
	0x10,
};

static unsigned char SEQ_ILI9882N_21[] = {
	0x3E,
	0x60,
};

static unsigned char SEQ_ILI9882N_22[] = {
	0xFF,
	0x98, 0x82, 0x00,
};

static struct lcd_seq_info LCD_SEQ_INIT_1[] = {
	{SEQ_ILI9882N_01, ARRAY_SIZE(SEQ_ILI9882N_01), 0, },
#if 0
	{SEQ_ILI9882N_02, ARRAY_SIZE(SEQ_ILI9882N_02), 0, },
	{SEQ_ILI9882N_03, ARRAY_SIZE(SEQ_ILI9882N_03), 0, },
#endif
	{SEQ_ILI9882N_04, ARRAY_SIZE(SEQ_ILI9882N_04), 0, },
	{SEQ_ILI9882N_05, ARRAY_SIZE(SEQ_ILI9882N_05), 0, },
	{SEQ_ILI9882N_06, ARRAY_SIZE(SEQ_ILI9882N_06), 0, },
	{SEQ_ILI9882N_07, ARRAY_SIZE(SEQ_ILI9882N_07), 0, },
	{SEQ_ILI9882N_08, ARRAY_SIZE(SEQ_ILI9882N_08), 0, },
	{SEQ_ILI9882N_09, ARRAY_SIZE(SEQ_ILI9882N_09), 0, },
	{SEQ_ILI9882N_10, ARRAY_SIZE(SEQ_ILI9882N_10), 0, },
	{SEQ_ILI9882N_11, ARRAY_SIZE(SEQ_ILI9882N_11), 0, },
	{SEQ_ILI9882N_12, ARRAY_SIZE(SEQ_ILI9882N_12), 0, },
	{SEQ_ILI9882N_13, ARRAY_SIZE(SEQ_ILI9882N_13), 0, },
	{SEQ_ILI9882N_14, ARRAY_SIZE(SEQ_ILI9882N_14), 0, },
	{SEQ_ILI9882N_15, ARRAY_SIZE(SEQ_ILI9882N_15), 0, },
	{SEQ_ILI9882N_16, ARRAY_SIZE(SEQ_ILI9882N_16), 0, },
	{SEQ_ILI9882N_17, ARRAY_SIZE(SEQ_ILI9882N_17), 0, },
	{SEQ_ILI9882N_18, ARRAY_SIZE(SEQ_ILI9882N_18), 0, },
	{SEQ_ILI9882N_19, ARRAY_SIZE(SEQ_ILI9882N_19), 0, },
	{SEQ_ILI9882N_20, ARRAY_SIZE(SEQ_ILI9882N_20), 0, },
	{SEQ_ILI9882N_21, ARRAY_SIZE(SEQ_ILI9882N_21), 0, },
	{SEQ_ILI9882N_22, ARRAY_SIZE(SEQ_ILI9882N_22), 0, },
};

static unsigned int brightness_table[EXTEND_BRIGHTNESS + 1] = {
	0,
	1, 1, 2, 3, 3, 4, 5, 5, 6, 7, /* 1: 1 */
	7, 8, 9, 9, 10, 11, 11, 12, 13, 14,
	14, 15, 16, 16, 17, 18, 18, 19, 20, 20,
	21, 22, 22, 23, 24, 24, 25, 26, 27, 27,
	28, 29, 29, 30, 31, 31, 32, 33, 33, 34,
	35, 35, 36, 37, 37, 38, 39, 40, 40, 41,
	42, 42, 43, 44, 44, 45, 46, 46, 47, 48,
	48, 49, 50, 51, 51, 52, 53, 53, 54, 55,
	55, 56, 57, 57, 58, 59, 59, 60, 61, 61,
	62, 63, 64, 64, 65, 66, 66, 67, 68, 68,
	69, 70, 70, 71, 72, 72, 73, 74, 74, 75,
	76, 77, 77, 78, 79, 79, 80, 81, 81, 82,
	83, 83, 84, 85, 85, 86, 87, 88, 88, 89, /* 128: 88 */
	90, 91, 92, 93, 94, 95, 96, 97, 98, 99,
	100, 101, 102, 103, 104, 105, 106, 107, 108, 109,
	110, 111, 112, 113, 114, 115, 116, 117, 118, 118,
	119, 120, 121, 122, 123, 124, 125, 126, 127, 128,
	129, 130, 131, 132, 133, 134, 135, 136, 137, 138,
	139, 140, 141, 142, 143, 144, 145, 146, 147, 148,
	149, 149, 150, 151, 152, 153, 154, 155, 156, 157,
	158, 159, 160, 161, 162, 163, 164, 165, 166, 167,
	168, 169, 170, 171, 172, 173, 174, 175, 176, 177,
	178, 179, 180, 180, 181, 182, 183, 184, 185, 186,
	187, 188, 189, 190, 191, 192, 193, 194, 195, 196,
	197, 198, 199, 200, 201, 202, 203, 204, 205, 206,
	207, 208, 209, 210, 211, 211, 211, 211, 211, 211, /* 255: 211 */
	211, 211, 211, 211, 211, 211, 211, 211, 211, 211,
	211, 211, 211, 211, 211, 211, 211, 211, 211, 211,
	211, 211, 211, 211, 211, 211, 211, 211, 211, 211,
	211, 211, 211, 211, 211, 211, 211, 211, 211, 211,
	211, 211, 211, 211, 211, 254,
};

#endif /* __ILI9882N_PARAM_H__ */
