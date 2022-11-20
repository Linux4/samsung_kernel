#ifndef __S6E88A0_PARAM_H__
#define __S6E88A0_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>

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
#define ACL_IS_ON(nit) 				(nit < 350)

#define NORMAL_TEMPERATURE			25	/* 25 degrees Celsius */
#define UI_MAX_BRIGHTNESS 	255
#define UI_MIN_BRIGHTNESS 	0
#define UI_DEFAULT_BRIGHTNESS 134

#define S6E88A0_MTP_ADDR 			0xC8 // ok
#define S6E88A0_MTP_SIZE 			33// ok
#define S6E88A0_MTP_DATE_SIZE 		S6E88A0_MTP_SIZE // ok
#define S6E88A0_COORDINATE_REG		0xB5
#define S6E88A0_COORDINATE_LEN		24
#define HBM_INDEX					62
#define S6E88A0_ID_REG				0x04
#define S6E88A0_ID_LEN				3
#define TSET_REG			0xB8
#define TSET_LEN			7
#define ELVSS_REG			0xB6
#define ELVSS_LEN			3
#define S6E88A0_DATE_REG		0xB5
#define S6E88A0_DATE_LEN		25

#define GAMMA_CMD_CNT		34
#define AID_CMD_CNT			6
#define ELVSS_CMD_CNT		3

#define HBM_GAMMA_READ_ADD_1		0xB5
#define HBM_GAMMA_READ_SIZE_1		28
#define HBM_GAMMA_READ_ADD_2		0xB6
#define HBM_GAMMA_READ_SIZE_2		29


static const unsigned char SEQ_S_WIRE[] = {
	0xB8,
	0x38, 0x0B, 0x40, 0x00, 0xA8,
};

static const unsigned char SEQ_ACL_MTP[] = {
	0xB5,
	0x03,
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
	0x40, 0x0A, 0x17, 0x00, 0x0A,
};

static const unsigned char SEQ_ELVSS_SET[] = {
	0xB6,
	0x2C, 0x0B,
};

static const unsigned char SEQ_ELVSS_GLOBAL[] = {
	0xB0,
	0x10,
};

static const unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7,
	0x03,
};

static const unsigned char SEQ_HBM_OFF[] = {
	0x53,
	0x00,
};

static const unsigned char SEQ_HBM_ON[] = {
	0x53,
	0x11,
};

static const unsigned char SEQ_ACL_SET[] = {
	0x55,
	0x01,
};

static const unsigned char SEQ_ACL_OFF[] = {
	0x55,
	0x00,
};

static const unsigned char SEQ_ACL_15[] = {
	0x55,
	0x02,
};

static const unsigned char SEQ_ACL_OFF_OPR_AVR[] = {
	0xB5,
	0x40
};

static const unsigned char SEQ_ACL_ON_OPR_AVR[] = {
	0xB5,
	0x50
};

static const unsigned char SEQ_TSET_GLOBAL[] = {
	0xB0,
	0x05
};

/* Tset 25 degree */
static const unsigned char SEQ_TSET[] = {
	0xB8,
	0x19
};

static const unsigned char SEQ_ELVSS_SET_S6E88A0[] = {
	0xB6,
	0x2C, 0x0B,
};

static const unsigned char SEQ_GAMMA_UPDATE_S6E88A0[] = {
	0xF7,
	0x03,
};

/* ACL on 15% */
static const unsigned char SEQ_ACL_SET_S6E88A0[] = {
	0x55,
	0x01,
};

static const unsigned char SEQ_TSET_GP[] = {
	0xB0,
	0x05,
};



#endif /* __S6E88A0_PARAM_H__ */

