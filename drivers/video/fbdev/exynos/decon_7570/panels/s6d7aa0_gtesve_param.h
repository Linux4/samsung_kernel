#ifndef __S6D7AA0_PARAM_H__
#define __S6D7AA0_PARAM_H__
#include <linux/types.h>
#include <linux/kernel.h>

#define EXTEND_BRIGHTNESS	306
#define UI_MAX_BRIGHTNESS	255
#define UI_DEFAULT_BRIGHTNESS	128

#define S6D7AA0_ID_REG		0xDA
#define S6D7AA0_ID_LEN		3
#define BRIGHTNESS_REG		0x51

struct lcd_seq_info {
	unsigned char	*cmd;
	unsigned int	len;
	unsigned int	sleep;
};

struct i2c_rom_data {
	u8 addr;
	u8 val;
};

/* 2017.07.04 RE: RE: RE: RE: RE: RE: RE: LP8558  mail */
static const struct i2c_rom_data LP8558_eprom_drv_arr[] = {
	{0xA1, 0x6F},
	{0xA5, 0x34},
	{0xA6, 0x80},
	{0xA7, 0xF6},
	{0xA9, 0x80},
	{0x16, 0x07},
};

static const struct i2c_rom_data LP8558_eprom_drv_arr_off[] = {
	{0x00, 0x00},
};

//Date : 2017-08-02 10:47 (GMT+9)
//Title : FW: RE: ISL98608
static const struct i2c_rom_data ISL98608_eprom_drv_arr[] = {
	{0x04, 0x00},
	{0x05, 0x87},
	{0x06, 0x08},
	{0x08, 0x00},
	{0x09, 0x00},
	{0x0D, 0x34},
};

static const struct i2c_rom_data ISL98608_eprom_drv_arr_off[] = {
	{0x00, 0x00},
};

static const unsigned char SEQ_01_PARAM[] = {
	0x01,
	0x00, 0x00
};

static const unsigned char SEQ_B3_PARAM[] = {
	0xB3,
	0X51, 0X00
};

static const unsigned char SEQ_PASSWD1[] = {
	0xF0,
	0x5A, 0x5A
};

static const unsigned char SEQ_PASSWD2[] = {
	0xF1,
	0x5A, 0x5A
};

static const unsigned char SEQ_PASSWD3[] = {
	0xFC,
	0xA5, 0xA5
};

static const unsigned char SEQ_PASSWD1_LOCK[] = {
	0xF0,
	0xA5, 0xA5
};

static const unsigned char SEQ_PASSWD2_LOCK[] = {
	0xF1,
	0xA5, 0xA5
};

static const unsigned char SEQ_PASSWD3_LOCK[] = {
	0xFC,
	0x5A, 0x5A
};

static const unsigned char SEQ_OTP_RELOAD[] = {
	0xD0,
	0x00, 0x10
};

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11,
	0x00, 0x00
};

static const unsigned char SEQ_BL_ON_CTL[] = {
	0xC3,
	0xC7, 0x00, 0x29		// PMIC_ON, PWM freq 24 kHz
};

static const unsigned char SEQ_PWM_MANUAL[] = {
	0xC1,
	0x01, 0x00		// PWM manual mode
};

static const unsigned char SEQ_F2_PARAM[] = {
	0xF2,		//if change MIPI speed -> can modify
	0x02, 0x08, 0x08, 0x40, 0x10		// last 2~5th reg ->.vbp = 6+(vsw2), .vfp = 8, .hbp = 48+(hsw16), .hfp = 16,
};

static const unsigned char SEQ_PWM_DUTY_INIT[] = {
	0x51,
	0x00, 0x00
};

static const unsigned char SEQ_BACKLIGHT_CTL[] = {
	0x53,
	0x24, 0x00		// PWM enable, dimming enable
};

static const unsigned char SEQ_BACKLIGHT_OFF[] = {
	0x53,
	0x00, 0x00
};

static const unsigned char SEQ_SLEEP_IN[] = {
	0x10,
	0x00, 0x00
};

static const unsigned char SEQ_DISPLAY_OFF[] = {
	0x28,
	0x00, 0x00
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29,
	0x00, 0x00
};

/* platform brightness <-> bl reg */
static unsigned int brightness_table[EXTEND_BRIGHTNESS + 1] = {
	0,
	0,
	2,
	2,
	3,
	3,
	4,
	5,
	6,
	6,
	7,
	7,
	8,
	8,
	9,
	9,
	10,
	10,
	11,
	11,
	12,
	13,
	13,
	14,
	14,
	15,
	15,
	16,
	16,
	17,
	18,
	18,
	19,
	19,
	20,
	20,
	21,
	21,
	22,
	23,
	23,
	24,
	24,
	25,
	25,
	26,
	26,
	27,
	28,
	28,
	29,
	29,
	30,
	30,
	31,
	31,
	32,
	33,
	33,
	34,
	34,
	35,
	35,
	36,
	36,
	37,
	38,
	38,
	39,
	39,
	40,
	40,
	41,
	41,
	42,
	43,
	43,
	44,
	44,
	45,
	45,
	46,
	46,
	47,
	48,
	48,
	49,
	49,
	50,
	50,
	51,
	51,
	52,
	53,
	54,
	55,
	56,
	57,
	58,
	59,
	60,
	61,
	62,
	63,
	64,
	65,
	66,
	67,
	68,
	69,
	70,
	71,
	72,
	73,
	74,
	75,
	76,
	77,
	78,
	79,
	80,
	81,
	82,
	83,
	84,
	85,
	86,
	87,
	88,
	89,
	90,
	91,
	92,
	93,
	94,
	95,
	96,
	97,
	98,
	99,
	100,
	101,
	102,
	103,
	104,
	105,
	106,
	107,
	108,
	109,
	110,
	111,
	112,
	113,
	114,
	115,
	116,
	117,
	118,
	119,
	120,
	121,
	122,
	123,
	124,
	125,
	126,
	127,
	128,
	129,
	130,
	131,
	132,
	133,
	134,
	135,
	136,
	137,
	138,
	139,
	140,
	141,
	142,
	143,
	144,
	145,
	146,
	147,
	148,
	149,
	150,
	150,
	151,
	151,
	152,
	152,
	153,
	153,
	154,
	154,
	155,
	155,
	156,
	156,
	157,
	157,
	158,
	158,
	159,
	159,
	160,
	160,
	161,
	161,
	162,
	163,
	164,
	164,
	165,
	165,
	166,
	166,
	167,
	167,
	168,
	168,
	169,
	169,
	170,
	170,
	171,
	171,
	172,
	172,
	173,
	173,
	174,
	174,
	175,
	175,
	175,
	176,
	176,
	176,
	177,
	177,
	177,
	178,
	178,
	178,
	179,
	179,
	179,
	180,
	180,
	[UI_MAX_BRIGHTNESS ... EXTEND_BRIGHTNESS - 1] = 181,
	[EXTEND_BRIGHTNESS ... EXTEND_BRIGHTNESS] = 225
};

#endif /* __S6D7AA0_PARAM_H__ */
