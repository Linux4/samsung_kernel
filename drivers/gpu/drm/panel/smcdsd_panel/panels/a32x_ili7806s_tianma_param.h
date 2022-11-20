/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ILI7806S_PARAM_H__
#define __ILI7806S_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>

#define LCD_TYPE_VENDOR		"TMC"

#define EXTEND_BRIGHTNESS	306
#define UI_MAX_BRIGHTNESS	255
#define UI_DEFAULT_BRIGHTNESS	128

#define LDI_REG_BRIGHTNESS	0x51
#define LDI_REG_ID		0xDA
#define LDI_LEN_BRIGHTNESS	((u16)ARRAY_SIZE(SEQ_ILI7806S_BRIGHTNESS))

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
	TYPE_WRITE, 0x03, 0x2F,
	TYPE_WRITE, 0x11, 0x74,
	TYPE_WRITE, 0x04, 0x06,
	TYPE_WRITE, 0x05, 0xC0,
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

static unsigned char SEQ_ILI7806S_SLEEP_OUT[] = { 0x11 };
static unsigned char SEQ_ILI7806S_SLEEP_IN[] = { 0x10 };
static unsigned char SEQ_ILI7806S_DISPLAY_OFF[] = { 0x28 };
static unsigned char SEQ_ILI7806S_DISPLAY_ON[] = { 0x29 };

static unsigned char SEQ_ILI7806S_BRIGHTNESS[] = {
	0x51,
	0x00, 0x00,
};

static unsigned char SEQ_ILI7806S_BRIGHTNESS_MODE[] = {
	0x53,
	0x2C,
};

static unsigned char SEQ_ILI7806S_001[] = {
	0xFF,
	0x78, 0x07, 0x00,
};

static unsigned char SEQ_ILI7806S_002[] = {
	0x51,
	0x0F, 0xFF,
};

static unsigned char SEQ_ILI7806S_003[] = {
	0x53,
	0x2C,
};

static unsigned char SEQ_ILI7806S_004[] = {
	0x35,
	0x00,
};

static unsigned char SEQ_ILI7806S_005[] = {
	0x55,
	0x01,
};

static unsigned char SEQ_ILI7806S_006[] = {
	0xFF,
	0x78, 0x07, 0x03,
};

static unsigned char SEQ_ILI7806S_007[] = {
	0x80,
	0x36,
};

static unsigned char SEQ_ILI7806S_008[] = {
	0x83,
	0x60,
};

static unsigned char SEQ_ILI7806S_009[] = {
	0x84,
	0x00,
};

static unsigned char SEQ_ILI7806S_010[] = {
	0x88,
	0xD9,
};

static unsigned char SEQ_ILI7806S_011[] = {
	0x89,
	0xE1,
};

static unsigned char SEQ_ILI7806S_012[] = {
	0x8A,
	0xE8,
};

static unsigned char SEQ_ILI7806S_013[] = {
	0x8B,
	0xF0,
};

static unsigned char SEQ_ILI7806S_014[] = {
	0xC6,
	0x14,
};

static unsigned char SEQ_ILI7806S_015[] = {
	0xFF,
	0x78, 0x07, 0x06,
};

static unsigned char SEQ_ILI7806S_016[] = {
	0xDD,
	0x1F,
};

static unsigned char SEQ_ILI7806S_017[] = {
	0xCD,
	0x00,
};

static unsigned char SEQ_ILI7806S_018[] = {
	0xD6,
	0x00,
};

static unsigned char SEQ_ILI7806S_019[] = {
	0x48,
	0x03,
};

static unsigned char SEQ_ILI7806S_020[] = {
	0x3E,
	0xE2,
};

static unsigned char SEQ_ILI7806S_021[] = {
	0xFF,
	0x78, 0x07, 0x0E,
};

static unsigned char SEQ_ILI7806S_022[] = {
	0x0B,
	0x00,
};

static unsigned char SEQ_ILI7806S_023[] = {
	0x21,
	0x14,
};

static unsigned char SEQ_ILI7806S_024[] = {
	0xFF,
	0x78, 0x07, 0x1E,
};

static unsigned char SEQ_ILI7806S_025[] = {
	0xA5,
	0x96,
};

static unsigned char SEQ_ILI7806S_026[] = {
	0xA6,
	0x96,
};

static unsigned char SEQ_ILI7806S_027[] = {
	0xFF,
	0x78, 0x07, 0x00,
};


static struct lcd_seq_info LCD_SEQ_INIT_1[] = {
	{SEQ_ILI7806S_001, ARRAY_SIZE(SEQ_ILI7806S_001), 0, },
	{SEQ_ILI7806S_002, ARRAY_SIZE(SEQ_ILI7806S_002), 0, },
	{SEQ_ILI7806S_003, ARRAY_SIZE(SEQ_ILI7806S_003), 0, },
	{SEQ_ILI7806S_004, ARRAY_SIZE(SEQ_ILI7806S_004), 0, },
	{SEQ_ILI7806S_005, ARRAY_SIZE(SEQ_ILI7806S_005), 0, },
	{SEQ_ILI7806S_006, ARRAY_SIZE(SEQ_ILI7806S_006), 0, },
	{SEQ_ILI7806S_007, ARRAY_SIZE(SEQ_ILI7806S_007), 0, },
	{SEQ_ILI7806S_008, ARRAY_SIZE(SEQ_ILI7806S_008), 0, },
	{SEQ_ILI7806S_009, ARRAY_SIZE(SEQ_ILI7806S_009), 0, },
	{SEQ_ILI7806S_010, ARRAY_SIZE(SEQ_ILI7806S_010), 0, },
	{SEQ_ILI7806S_011, ARRAY_SIZE(SEQ_ILI7806S_011), 0, },
	{SEQ_ILI7806S_012, ARRAY_SIZE(SEQ_ILI7806S_012), 0, },
	{SEQ_ILI7806S_013, ARRAY_SIZE(SEQ_ILI7806S_013), 0, },
	{SEQ_ILI7806S_014, ARRAY_SIZE(SEQ_ILI7806S_014), 0, },
	{SEQ_ILI7806S_015, ARRAY_SIZE(SEQ_ILI7806S_015), 0, },
	{SEQ_ILI7806S_016, ARRAY_SIZE(SEQ_ILI7806S_016), 0, },
	{SEQ_ILI7806S_017, ARRAY_SIZE(SEQ_ILI7806S_017), 0, },
	{SEQ_ILI7806S_018, ARRAY_SIZE(SEQ_ILI7806S_018), 0, },
	{SEQ_ILI7806S_019, ARRAY_SIZE(SEQ_ILI7806S_019), 0, },
	{SEQ_ILI7806S_020, ARRAY_SIZE(SEQ_ILI7806S_020), 0, },
	{SEQ_ILI7806S_021, ARRAY_SIZE(SEQ_ILI7806S_021), 0, },
	{SEQ_ILI7806S_022, ARRAY_SIZE(SEQ_ILI7806S_022), 0, },
	{SEQ_ILI7806S_023, ARRAY_SIZE(SEQ_ILI7806S_023), 0, },
	{SEQ_ILI7806S_024, ARRAY_SIZE(SEQ_ILI7806S_024), 0, },
	{SEQ_ILI7806S_025, ARRAY_SIZE(SEQ_ILI7806S_025), 0, },
	{SEQ_ILI7806S_026, ARRAY_SIZE(SEQ_ILI7806S_026), 0, },
	{SEQ_ILI7806S_027, ARRAY_SIZE(SEQ_ILI7806S_027), 0, },
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

#endif /* __ILI7806S_PARAM_H__ */
