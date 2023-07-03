#ifndef __LTM184HL01_PARAM_H__
#define __LTM184HL01_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>


struct lcd_seq_info {
	unsigned char	*cmd;
	unsigned int	len;
	unsigned int	sleep;
};

#define POWER_IS_ON(pwr)			(pwr <= FB_BLANK_NORMAL)
#define UNDER_MINUS_20(temperature)	(temperature <= -20)
#define LEVEL_IS_HBM(brightness)		(brightness == EXTEND_BRIGHTNESS)

#define EXTEND_BRIGHTNESS	306
#define UI_MIN_BRIGHTNESS 	0
#define UI_DEFAULT_BRIGHTNESS 134

struct LP8558_rom_data {
	u8 addr;
	u8 val;
};

static const struct LP8558_rom_data LP8558_eprom_drv_arr_init[] = {
	{0x01, 0x83},/*for Brightness&PWM control*/
	{0xA5, 0x04},/*channel setting (for 6ch)*/
	{0xA3, 0x5E},/*Slope setting */
	{0xA0, 0x65},/* Change LED Current for 27mA*/
	{0xA1,	0x6E},/*Change LED Current for 27mA*/
	{0xA6,	0x80},/*frequency setting for 1.25MHz*/
	{0xA9,	0xC0},/*Boost MAX setting for max 39V */
	{0x00,	0xCB},/*Apply BRT setting for normal for 21.5mA*/
};


/*to fix build break*/
static const unsigned char SEQ_TEST_KEY_ON_F0[] = {

};

static const unsigned char SEQ_TEST_KEY_ON_FC[] = {

};

static const unsigned char SEQ_TEST_KEY_OFF_F0[] = {

};

static const unsigned char SEQ_TEST_KEY_OFF_FC[] = {

};

#endif /* __LTM184HL01_PARAM_H__ */
