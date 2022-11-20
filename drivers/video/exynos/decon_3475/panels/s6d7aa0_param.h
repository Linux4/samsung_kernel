#ifndef __S6D7AA0_PARAM_H__
#define __S6D7AA0_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>


struct lcd_seq_info {
	unsigned char	*cmd;
	unsigned int	len;
	unsigned int	sleep;
};

#define POWER_IS_ON(pwr)			(pwr <= FB_BLANK_NORMAL)
#define UNDER_MINUS_20(temperature)	(temperature <= -20)

#define NORMAL_TEMPERATURE			25	/* 25 degrees Celsius */
#define UI_MAX_BRIGHTNESS 	255
#define UI_MIN_BRIGHTNESS 	0
#define UI_DEFAULT_BRIGHTNESS 134

#define S6D7AA0_ID_REG				0xD8
#define S6D7AA0_ID_LEN				3

#define SKY82896_REG_CTRL1		0x00
#define SKY82896_REG_CTRL2		0x01
#define SKY82896_REG_BRT0			0x02
#define SKY82896_REG_BRT1			0x03

struct SKY82896_rom_data {
	u8 addr;
	u8 val;
};

static const struct SKY82896_rom_data SKY82896_eprom_drv_arr[] = {
	{ SKY82896_REG_CTRL1, 0x70},
	{ SKY82896_REG_CTRL2, 0x09},
	{ SKY82896_REG_BRT0, 0xF4},
	{ SKY82896_REG_BRT1, 0x02},
	{ SKY82896_REG_CTRL1, 0x7E},
};

static const struct SKY82896_rom_data SKY82896_eprom_drv_arr_off[] = {
	{ SKY82896_REG_BRT0, 0x00},
	{ SKY82896_REG_BRT1, 0x00},
	{ SKY82896_REG_CTRL1, 0x00},
};

static const struct SKY82896_rom_data LP8557_eprom_drv_arr[] = {
	{ 0x00, 0x00},
	{ 0x03, 0xE0},
	{ 0x04, 0xB4},
	{ 0x10, 0x06},
	{ 0x11, 0x03},
	{ 0x12, 0x2F},
	{ 0x13, 0x03},
	{ 0x14, 0xBF},
	{ 0x15, 0xC3},
	{ 0x00, 0x07},
};

static const struct SKY82896_rom_data LP8557_eprom_drv_arr_outdoor_on[] = {
	{ 0x03, 0xD0},
	{ 0x04, 0xDD},
};

static const struct SKY82896_rom_data LP8557_eprom_drv_arr_outdoor_off[] = {
	{ 0x03, 0xE0},
	{ 0x04, 0xB4},
};

static const struct SKY82896_rom_data SKY82896_eprom_drv_arr_outdoor_on[] = {
	{ SKY82896_REG_BRT0, 0x93},
	{ SKY82896_REG_BRT1, 0x03},
};

static const struct SKY82896_rom_data SKY82896_eprom_drv_arr_outdoor_off[] = {
	{ SKY82896_REG_BRT0, 0xF4},
	{ SKY82896_REG_BRT1, 0x02},
};

static const struct SKY82896_rom_data LP8557_eprom_drv_arr_off[] = {
	{ 0x00, 0x00},
};

int sky82896_array_write(const struct SKY82896_rom_data * eprom_ptr, int eprom_size);

/* S6D7AA0 */
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

static const unsigned char SEQ_INIT_1_OLD[] = {
	0xD0,
	0x00, 0x10
};

//wait 1ms

static const unsigned char SEQ_INIT_2_OLD[] = {
	0xB6,
	0x10,
};

static const unsigned char SEQ_INIT_3_OLD[] = {
	0xC3,
	0xC7, 0x00, 0x29
};

static const unsigned char SEQ_INIT_3_1_OLD[] = {
	0xF2,
	0x02, 0x08, 0x08, 0x40, 0x10
};

static const unsigned char SEQ_INIT_4_OLD[] = {
	0xBC,
	0x00, 0x4E, 0xA2
};

static const unsigned char SEQ_INIT_5_OLD[] = {
	0xFD,
	0x16, 0x10, 0x11, 0x23
};

static const unsigned char SEQ_INIT_6_OLD[] = {
	0xFE,
	0x00, 0x02, 0x03, 0x21, 0x00, 0x70
};

static const unsigned char SEQ_INIT_7_OLD[] = {
	0x53,
	0x2C,
};

static const unsigned char SEQ_INIT_8_OLD[] = {
	0x51,
	0x5F,
};

//wait 5ms

static const unsigned char SEQ_INIT_9_OLD[] = {
	0x36,
	0x04,
};

static const unsigned char SEQ_INIT_10_OLD[] = {
	0x36,
	0x00,
};

static const unsigned char SEQ_INIT_1[] = {
	0xD0,
	0x00, 0x10
};

static const unsigned char SEQ_INIT_2[] = {
	0xC3,
	0xC7, 0x00, 0x2A
};

static const unsigned char SEQ_INIT_3[] = {
	0xB3,
	0x51,
};

static const unsigned char SEQ_INIT_4[] = {
	0x53,
	0x24,
};

static const unsigned char SEQ_INIT_5[] = {
	0x51,
	0x00,
};


static const unsigned char SEQ_INIT_6[] = {
	0xC1,
	0x01,
};


static const unsigned char SEQ_INIT_7[] = {
	0xF2,
	0x02, 0x08, 0x08, 0x40, 0x10
};


static const unsigned char SEQ_INIT_8[] = {
	0xBC,
	0x01, 0x4E, 0x0B
};


static const unsigned char SEQ_INIT_9[] = {
	0xFD,
	0x16, 0x10, 0x11, 0x23, 0x09
};

static const unsigned char SEQ_INIT_10[] = {
	0xFE,
	0x00, 0x02, 0x03, 0x21, 0x80, 0x68
};


static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11,
	0x00,
};

//wait 120ms

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29,
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

static const unsigned char SEQ_TEST_KEY_ON_F0[] = {
	0xF0,
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_OFF_F0[] = {
	0xF0,
	0xA5, 0xA5,
};

static const unsigned char SEQ_TEST_KEY_ON_F1[] = {
	0xF1,
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_OFF_F1[] = {
	0xF1,
	0xA5, 0xA5,
};

static const unsigned char SEQ_TEST_KEY_ON_FC[] = {
	0xFC,
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_OFF_FC[] = {
	0xFC,
	0xA5, 0xA5,
};

#endif /* __S6D7AA0_PARAM_H__ */
