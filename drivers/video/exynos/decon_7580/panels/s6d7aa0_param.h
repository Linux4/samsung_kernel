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
	{ SKY82896_REG_BRT0, 0xAB},
	{ SKY82896_REG_BRT1, 0x02},
	{ SKY82896_REG_CTRL1, 0x7E},
};

static const struct SKY82896_rom_data SKY82896_eprom_drv_outdoor_on_arr[] = {
	{ SKY82896_REG_BRT0, 0x33},
	{ SKY82896_REG_BRT1, 0x03},
};

static const struct SKY82896_rom_data SKY82896_eprom_drv_outdoor_off_arr[] = {
	{ SKY82896_REG_BRT0, 0xAB},
	{ SKY82896_REG_BRT1, 0x02},
};


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

static const unsigned char SEQ_OTP_RELOAD[] = {
	0xD0,
	0x00, 0x10
};

static const unsigned char SEQ_BC_PARAM_MDNIE[] = {
	0xBC,
	0x01, 0x4E, 0x0A
};

static const unsigned char SEQ_FD_PARAM_MDNIE[] = {
	0xFD,
	0x16, 0x10, 0x11, 0x23, 0x09
};

static const unsigned char SEQ_FE_PARAM_MDNIE[] = {
	0xFE,
	0x00, 0x02, 0x03, 0x21, 0x80, 0x78
};

static const unsigned char SEQ_B3_PARAM[] = {
	0xB3,
	0x51, 0x00
};

static const unsigned char SEQ_BL_CTL[] = {
	0x53,
	0x24, 0x00
};

static const unsigned char SEQ_BL_DEFAULT[] = {
	0x51,
	0x5F, 0x00
};

static const unsigned char SEQ_PORCH_CTL[] = {
	0xF2,
	0x02, 0x08, 0x08
};

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11,
	0x00, 0x00
};

static const unsigned char SEQ_BL_ON_CTL[] = {
	0xC3,
	0xC7, 0x00, 0x29,
};

static const unsigned char SEQ_CABC_CTL[] = {
	0xC0,
	0x80, 0x80, 0x30,
};

static const unsigned char SEQ_CD_ON[] = {
	0xCD,
	0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,
	0x2E, 0x2E, 0x2E,
};

static const unsigned char SEQ_CD_OFF[] = {
	0xCD,
	0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,
	0x2E, 0x2E, 0x00,
};

static const unsigned char SEQ_CE_CTL[] = {
	0xCE,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00,
};

static const unsigned char SEQ_CABC_OFF[] = {
	0x55,
	0x00, 0x00
};

static const unsigned char SEQ_CABC_ON[] = {
	0x55,
	0x03, 0x00
};

static const unsigned char SEQ_BC_MODE[] = {
	0xC1,
	0x03, 0x00
};

static const unsigned char SEQ_TEON_CTL[] = {
	0x35,
	0x00, 0x00
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29,
	0x00, 0x00
};

static const unsigned char SEQ_TEST_KEY_ON_F0[] = {
	0xF0,
	0x5A, 0x5A
};

static const unsigned char SEQ_TEST_KEY_OFF_F0[] = {
	0xF0,
	0xA5, 0xA5
};

static const unsigned char SEQ_TEST_KEY_ON_FC[] = {
	0xFC,
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_OFF_FC[] = {
	0xFC,
	0xA5, 0xA5,
};

static const unsigned char SEQ_SLEEP_IN[] = {
	0x10,
	0x00, 0x00
};

static const unsigned char SEQ_DISPLAY_OFF[] = {
	0x28,
	0x00, 0x00
};

#endif /* __S6D7AA0_PARAM_H__ */
