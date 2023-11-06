#ifndef __NT36672A_PARAM_H__
#define __NT36672A_PARAM_H__
#include <linux/types.h>
#include <linux/kernel.h>

#define EXTEND_BRIGHTNESS	306
#define UI_MAX_BRIGHTNESS	255
#define UI_DEFAULT_BRIGHTNESS	128

struct lcd_seq_info {
	unsigned char	*cmd;
	unsigned int	len;
	unsigned int	sleep;
};

struct i2c_rom_data {
	u8 addr;
	u8 val;
};

static const struct i2c_rom_data LM3632_INIT[] = {
	{0x09, 0x41},
	{0x02, 0x70},
	{0x03, 0x8D},
	{0x04, 0x07},
	{0x05, 0xFF},
	{0x0A, 0x19},
	{0x0D, 0x1C},
	{0x0E, 0x1E},
	{0x0F, 0x1E},
	{0x0C, 0x1F},
};

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11,
};

static const unsigned char SEQ_NT36672A_FF[] = {
	0xFF,
	0x10,
};

static const unsigned char SEQ_TEON_CTL[] = {
	0x35,
	0x00,
};

static const unsigned char SEQ_NT36672A_BL[] = {
	0x51,
	0xFF,
};

static const unsigned char SEQ_NT36672A_BLON[] = {
	0x53,
	0x24,
};

static const unsigned char SEQ_NT36672A_55[] = {
	0x55,
	0x00,
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29,
	0x00, 0x00
};


static const unsigned char SEQ_DISPLAY_OFF[] = {
	0x28,
	0x00, 0x00
};

static const unsigned char SEQ_SLEEP_IN[] = {
	0x10,
	0x00, 0x00
};

/* platform brightness <-> bl reg */
static unsigned int brightness_table[EXTEND_BRIGHTNESS + 1] = {
	0,
	0, 0, 2, 2, 3, 3, 4, 4, 5, 5,
	6, 6, 7, 7, 7, 8, 9, 10, 11, 12,
	12, 13, 13, 14, 15, 15, 16, 16, 17, 17,
	18, 19, 19, 20, 20, 21, 22, 22, 23, 23,
	24, 25, 25, 26, 26, 27, 28, 28, 29, 29,
	30, 31, 31, 32, 32, 33, 33, 34, 35, 35,
	36, 36, 37, 38, 38, 39, 39, 40, 41, 41,
	42, 42, 43, 44, 44, 45, 45, 46, 46, 47,
	48, 48, 49, 49, 50, 51, 51, 52, 52, 53,
	54, 54, 55, 55, 56, 57, 57, 58, 58, 59,
	60, 60, 61, 61, 62, 62, 63, 64, 64, 65,
	65, 66, 67, 67, 68, 68, 69, 70, 70, 71,
	71, 72, 73, 73, 74, 74, 75, 76, 76, 77,
	78, 79, 80, 80, 81, 82, 83, 84, 85, 85,
	86, 87, 88, 89, 89, 90, 91, 92, 93, 94,
	94, 95, 96, 97, 98, 98, 99, 100, 101, 102,
	103, 103, 104, 105, 106, 107, 107, 108, 109, 110,
	111, 112, 112, 113, 114, 115, 116, 116, 117, 118,
	119, 120, 121, 121, 122, 123, 124, 125, 125, 126,
	127, 128, 129, 130, 130, 131, 132, 133, 134, 134,
	135, 136, 137, 138, 139, 139, 140, 141, 142, 143,
	143, 144, 145, 146, 147, 148, 148, 149, 150, 151,
	152, 152, 153, 154, 155, 156, 157, 157, 158, 159,
	160, 161, 161, 162, 163, 164, 165, 166, 166, 167,
	168, 169, 170, 170, 171, 172, 173, 174, 175, 175,
	176, 177, 178, 179, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
	180, 180, 180, 180, 180, 242,
};

#endif /* __NT36672A_PARAM_H__ */
