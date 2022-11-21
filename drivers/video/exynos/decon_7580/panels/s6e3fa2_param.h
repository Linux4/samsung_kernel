#ifndef __S6E3FA2_PARAM_H__
#define __S6E3FA2_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>


struct lcd_seq_info {
	unsigned char	*cmd;
	unsigned int	len;
	unsigned int	sleep;
};

enum {
	HBM_STATUS_OFF,
	HBM_STATUS_ON,
	HBM_STATUS_MAX,
};

enum {
	ACL_STATUS_0P,
	ACL_STATUS_15P,
	ACL_STATUS_MAX
};

enum {
	ACL_OPR_16_FRAME,
	ACL_OPR_32_FRAME,
	ACL_OPR_MAX
};

#define POWER_IS_ON(pwr)			(pwr <= FB_BLANK_NORMAL)
#define LEVEL_IS_HBM(level)			(level >= 6)
#define LEVEL_IS_CAPS_OFF(level)	(level <= 19)
#define UNDER_MINUS_20(temperature)	(temperature <= -20)
#define ACL_IS_ON(nit) 				(nit <= 234)

#define NORMAL_TEMPERATURE			25	/* 25 degrees Celsius */
#define UI_MAX_BRIGHTNESS 	255
#define UI_MIN_BRIGHTNESS 	0
#define UI_DEFAULT_BRIGHTNESS 134

#define S6E3FA2_MTP_ADDR 			0xC8
#define S6E3FA2_MTP_SIZE 			35
#define S6E3FA2_MTP_DATE_SIZE 		S6E3FA2_MTP_SIZE + 9
#define S6E3FA2_COORDINATE_REG		0xA1
#define S6E3FA2_COORDINATE_LEN		4
#define S6E3FA2_HBMGAMMA_REG		0xB4
#define S6E3FA2_HBMGAMMA_LEN		31
#define HBM_INDEX					65
#define S6E3FA2_ID_REG				0x04
#define S6E3FA2_ID_LEN				3
#define S6E3FA2_CODE_REG			0xD6
#define S6E3FA2_CODE_LEN			5
#define TSET_REG			0xB8
#define TSET_LEN			3
#define TSET_MINUS_OFFSET			0x03
#define ELVSS_REG			0xB6
#define ELVSS_LEN			4   /* elvss: Global para 4th */

#define GAMMA_CMD_CNT		36
#define AID_CMD_CNT			5
#define ELVSS_CMD_CNT		3

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

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11
};

static const unsigned char SEQ_SOURCE_CONTROL[] = {
	0xBA,
	0x32, 0x30, 0x01
};

static const unsigned char SEQ_GAMMA_CONDITION_SET[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x00, 0x00, 0x00
};

static const unsigned char SEQ_AID_SET[] = {
	0xB2,
	0x00, 0x0E, 0x00, 0x0E,
};

static const unsigned char SEQ_AID_SET_RevF[] = {
	0xB2,
	0x00, 0x06, 0x00, 0x06,
};

static const unsigned char SEQ_ELVSS_SET[] = {
	0xB6,
	0x98, 0x0A,
};

static const unsigned char SEQ_CAPS_ELVSS_SET[] = {
	0xB6,
	0x98, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x55, 0x54,
	0x20, 0x00, 0x08, 0x88, 0x8F, 0x0F, 0x02, 0x11, 0x11, 0x10
};

static const unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7,
	0x03
};

static const unsigned char SEQ_GAMMA_UPDATE_L[] = {
	0xF7,
	0x00
};

static const unsigned char SEQ_GAMMA_UPDATE_EA8064G[] = {
	0xF7,
	0x01
};

static const unsigned char SEQ_HBM_OFF[] = {
	0x53,
	0x00
};

static const unsigned char SEQ_HBM_ON[] = {
	0x53,
	0xD0
};

static const unsigned char SEQ_ACL_SET[] = {
	0x55,
	0x02
};

static const unsigned char SEQ_ACL_OFF[] = {
	0x55,
	0x00
};

static const unsigned char SEQ_ACL_15[] = {
	0x55,
	0x02,
};


static const unsigned char SEQ_ACL_OFF_OPR_AVR[] = {
	0xB5,
	0x41
};

static const unsigned char SEQ_ACL_ON_OPR_AVR[] = {
	0xB5,
	0x51
};

static const unsigned char SEQ_ACL_OFF_OPR_AVR_EA8064G[] = {
	0xB5,
	0x21
};

static const unsigned char SEQ_ACL_ON_OPR_AVR_EA8064G[] = {
	0xB5,
	0x29
};

static const unsigned char SEQ_TSET_GLOBAL[] = {
	0xB0,
	0x05
};

static const unsigned char SEQ_TSET[] = {
	0xB8,
	0x19
};

static const unsigned char SEQ_TE_OUT[] = {
	0x35,
	0x00
};

static const unsigned char SEQ_GPARAM_TE[] = {
	0xB0,
	0x02
};

static const unsigned char SEQ_TE_ON_SET1[] = {
	0xFD,
	0x0A
};

static const unsigned char SEQ_TE_ON_SET2[] = {
	0xFE,
	0x80
};

static const unsigned char SEQ_TE_ON_SET3[] = {
	0xFE,
	0x00
};

static const unsigned char SEQ_ERR_FG[] = {
	0xED,
	0x01, 0x00
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29
};

static const unsigned char SEQ_DISPLAY_OFF[] = {
	0x28,
	0x00, 0x00
};

static const unsigned char SEQ_SLEEP_IN[] = {
	0x10,
	0x00, 0x00
};

static const unsigned char SEQ_TOUCH_HSYNC_ON_RevG[] = {
	0xBD,
	0x05, 0x02, 0x02
};

static const unsigned char SEQ_TOUCH_HSYNC_ON[] = {
	0xBD,
	0x05, 0x02, 0x0C
};

#endif /* __S6E3FA2_PARAM_H__ */
