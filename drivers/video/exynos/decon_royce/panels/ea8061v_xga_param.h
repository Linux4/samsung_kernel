#ifndef __EA8061V_XGA_PARAM_H__
#define __EA8061V_XGA_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>

#define cmd_err_check(c, r, e) \
do { \
	if (r < 0) { \
		dsim_err("%s : fail to write %s\n", __func__,#c); \
		goto e; \
	} \
} while (0)

struct lcd_seq_info {
	unsigned char	*cmd;
	unsigned int	len;
	unsigned int	sleep;
};
/* FC register is not used in EA8061V, but it is for lcd_sysfs.c */
static const unsigned char SEQ_TEST_KEY_ON_FC[] = {
	0xFC,
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_OFF_FC[] = {
	0xFC,
	0xA5, 0xA5,
};

/************ Initial sequence begin ************/

static const unsigned char EA8061V_SEQ_LEVEL2_KEY_UNLOCK_F0[] = {
	0xF0,
	0x5A, 0x5A
};

static const unsigned char EA8061V_SEQ_LEVEL2_KEY_LOCK_F0[] = {
	0xF0,
	0xA5, 0xA5
};

static const unsigned char EA8061V_SEQ_MTP_KEY_UNLOCK_F1[] = {
	0xF1,
	0x5A, 0x5A
};

static const unsigned char EA8061V_SEQ_MTP_KEY_LOCK_F1[] = {
	0xF1,
	0xA5, 0xA5
};

static const unsigned char EA8061V_SEQ_COMMON_B8[] = {
	0xB8,
	0x00, 0x10 /* TSET:0`C, Vlin:7.6V*/
};
static const unsigned char EA8061V_SEQ_COMMON_BA[] = {
	0xBA,
	0x57
};

static const unsigned char EA8061V_SEQ_HSYNC_B9[] = {
	0xB9,
	0x03
};

static const unsigned char EA8061V_SEQ_GAMMA_CONDITION_CA[] = {
	0xCA, 0x01, 0x00, 0x01,
	0x00, 0x01, 0x00, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x00,
	0x00, 0x00
};

static const unsigned char EA8061V_BL_CTRL_B2[] = {
	0xB2,
	0x00, 0x06, 0x00, 0x0B
};

static const unsigned char EA8061V_BL_CTRL_B6[] = {
	0xB6,
	0x58, 0x8A
};

static const unsigned char EA8061V_SEQ_GAMMA_UPDATE_F7[] = {
	0xF7,
	0x01
};

/************ Initial sequence end ************/
static const unsigned char EA8061V_SEQ_SLEEP_OUT[] = {
	0x11
};

static const unsigned char EA8061V_SEQ_SLEEP_IN[] = {
	0x10
};

static const unsigned char EA8061V_SEQ_DISPLAY_ON[] = {
	0x29
};

static const unsigned char EA8061V_SEQ_DISPLAY_OFF[] = {
	0x28
};

static const unsigned char EA8061V_SEQ_ACL_OFF[] = {
	0x55,
	0x00
};

static const unsigned char EA8061V_SEQ_ACL_15[] = {
	0x55,
	0x01,
};

static const unsigned char EA8061V_SEQ_ACL_OFF_OPR[] = {
	0xB5,
	0x21
};

static const unsigned char EA8061V_SEQ_ACL_ON_OPR[] = {
	0xB5,
	0x29
};

static const unsigned char EA8061V_SEQ_TSET[] = {
	0xB8,
	0x19	/* Global para(8th) + 25 degrees  : 0x19 */
};

static const unsigned char EA8061V_SEQ_HBM_PARA_SKIP[] = {
	0xB0,
	0x03 /* skip 3 para.*/
};



#define POWER_IS_ON(pwr)			(pwr <= FB_BLANK_NORMAL)
#define LEVEL_IS_HBM(brightness)		(brightness == EXTEND_BRIGHTNESS)
#define UNDER_MINUS_20(temperature)	(temperature <= -20)
#define UNDER_0(temperature)	(temperature <= 0)

#define ACL_IS_ON(nit) 				(nit != 360)
#define CAPS_IS_ON(level)	(level >= 41)

#define NORMAL_TEMPERATURE			25	/* 25 degrees Celsius */
#define EXTEND_BRIGHTNESS			355
#define UI_MAX_BRIGHTNESS 			255
#define UI_MIN_BRIGHTNESS 			0
#define UI_DEFAULT_BRIGHTNESS 		134

#define EA8061V_MTP_ADDR		0xC8
#define EA8061V_MTP_SIZE		57

#define EA8061V_ID_REG			0x04
#define EA8061V_ID_LEN			3


#define EA8061V_RDDPM_ADDR		0x0A
#define EA8061V_RDDSM_ADDR		0x0E
#define EA8061V_MTP_DATE_SIZE	EA8061V_MTP_SIZE

#define EA8061V_COORDINATE_LEN	4
#define EA8061V_COORDINATE_REG	0xA1

#define EA8061V_CODE_REG		0xD6
#define EA8061V_CODE_LEN		5


#define EA8061V_HBMGAMMA_REG		0xCA
#define EA8061V_HBMGAMMA_LEN		33


#define AID_CMD_CNT					5
#define ELVSS_CMD_CNT				3
#define GAMMA_CMD_CNT				34
#define ELVSS_REG					0xB6
#define ELVSS_LEN					23	/* elvss: Global para 22th */

#define TSET_REG					0xB8 /* TSET: Global para 8th */
#define TSET_LEN					2

#define SEQ_TEST_KEY_ON_F0			EA8061V_SEQ_LEVEL2_KEY_UNLOCK_F0
#define SEQ_TEST_KEY_OFF_F0			EA8061V_SEQ_LEVEL2_KEY_LOCK_F0
#define SEQ_TEST_KEY_ON_F1			EA8061V_SEQ_MTP_KEY_UNLOCK_F1
#define SEQ_TEST_KEY_OFF_F1			EA8061V_SEQ_MTP_KEY_LOCK_F1
#define SEQ_GAMMA_UPDATE			EA8061V_SEQ_GAMMA_UPDATE_F7

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

#endif /* __EA8061V_XGA_PARAM_H__ */
