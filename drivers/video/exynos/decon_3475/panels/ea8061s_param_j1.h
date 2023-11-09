#ifndef __S6E88A0_PARAM_H__
#define __S6E88A0_PARAM_H__

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

#define POWER_IS_ON(pwr)		(pwr <= FB_BLANK_NORMAL)
#define LEVEL_IS_HBM(level)		(level >= 6)
#define LEVEL_IS_CAPS_OFF(level)	(level <= 19)
#define UNDER_MINUS_20(temperature)	(temperature <= -20)
#define ACL_IS_ON(nit)			(nit < 350)

#define NORMAL_TEMPERATURE	25	/* 25 degrees Celsius */
#define UI_MAX_BRIGHTNESS	255
#define UI_MIN_BRIGHTNESS	0
#define UI_DEFAULT_BRIGHTNESS	134

#define S6E88A0_MTP_ADDR		0xC8
#define S6E88A0_MTP_SIZE		33
#define S6E88A0_MTP_DATE_SIZE		S6E88A0_MTP_SIZE
#define S6E88A0_COORDINATE_REG		0xB5
#define S6E88A0_COORDINATE_LEN		24
#define HBM_INDEX			62
#define S6E88A0_ID_REG			0x04
#define S6E88A0_ID_LEN			3
#define TSET_REG			0xB8
#define TSET_LEN			7
#define ELVSS_REG			0xB6
#define ELVSS_LEN			3
#define S6E88A0_DATE_REG		0xB5
#define S6E88A0_DATE_LEN		25

#define EA8061S_MTP_ADDR		0xC8
#define EA8061S_MTP_SIZE		33
#define EA8061S_MTP_DATE_SIZE		EA8061S_MTP_SIZE
#define EA8061S_ID_REG			0x04
#define EA8061S_ID_LEN			3
#define EA8061S_DATE_REG		0xA1
#define EA8061S_DATE_LEN		6
#define EA8061S_TSET_LEN		2

#define HBM_GAMMA_READ_ADD_EA8061S	0xB6
#define HBM_GAMMA_READ_SIZE_EA8061S	8

#define GAMMA_CMD_CNT			34
#define AID_CMD_CNT			6
#define AID_CMD_CNT_EA8061S		5
#define ELVSS_CMD_CNT			3

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

static const unsigned char SEQ_ELVSS_GLOBAL_EA8061S[] = {
	0xB0,
	0x07,
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
	0xC0,
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

static const unsigned char SEQ_SLEEP_IN[] = {
	0x10,
	0x00, 0x00
};

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11,
	0x00, 0x00
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29,
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

static const unsigned char SEQ_MDNIE_1[] = {
	/* start */
	0xEC,
	0x00, /* roi ctrl */
	0x00, /* roi0 x start */
	0x00,
	0x00, /* roi0 x end */
	0x00,
	0x00, /* roi0 y start */
	0x00,
	0x00, /* roi0 y end */
	0x00,
	0x00, /* roi1 x strat */
	0x00,
	0x00, /* roi1 x end */
	0x00,
	0x00, /* roi1 y start */
	0x00,
	0x00, /* roi1 y end */
	0x00,
	0x00, /* scr Cr Yb */
	0xff, /* scr Rr Bb */
	0xff, /* scr Cg Yg */
	0x14, /* scr Rg Bg */
	0xff, /* scr Cb Yr */
	0x14, /* scr Rb Br */
	0xff, /* scr Mr Mb */
	0x00, /* scr Gr Gb */
	0x00, /* scr Mg Mg */
	0xff, /* scr Gg Gg */
	0xff, /* scr Mb Mr */
	0x00, /* scr Gb Gr */
	0xff, /* scr Yr Cb */
	0x00, /* scr Br Rb */
	0xff, /* scr Yg Cg */
	0x00, /* scr Bg Rg */
	0x00, /* scr Yb Cr */
	0xff, /* scr Bb Rr */
	0xff, /* scr Wr Wb */
	0x00, /* scr Kr Kb */
	0xff, /* scr Wg Wg */
	0x00, /* scr Kg Kg */
	0xff, /* scr Wb Wr */
	0x00, /* scr Kb Kr */
	0x00, /* curve 1 b */
	0x20, /* curve 1 a */
	0x00, /* curve 2 b */
	0x20, /* curve 2 a */
	0x00, /* curve 3 b */
	0x20, /* curve 3 a */
	0x00, /* curve 4 b */
	0x20, /* curve 4 a */
	0x00, /* curve 5 b */
	0x20, /* curve 5 a */
	0x00, /* curve 6 b */
	0x20, /* curve 6 a */
	0x00, /* curve 7 b */
	0x20, /* curve 7 a */
	0x00, /* curve 8 b */
	0x20, /* curve 8 a */
	0x00, /* curve 9 b */
	0x20, /* curve 9 a */
	0x00, /* curve10 b */
	0x20, /* curve10 a */
	0x00, /* curve11 b */
	0x20, /* curve11 a */
	0x00, /* curve12 b */
	0x20, /* curve12 a */
	0x00, /* curve13 b */
	0x20, /* curve13 a */
	0x00, /* curve14 b */
	0x20, /* curve14 a */
	0x00, /* curve15 b */
	0x20, /* curve15 a */
	0x00, /* curve16 b */
	0x20, /* curve16 a */
	0x00, /* curve17 b */
	0x20, /* curve17 a */
	0x00, /* curve18 b */
	0x20, /* curve18 a */
	0x00, /* curve19 b */
	0x20, /* curve19 a */
	0x00, /* curve20 b */
	0x20, /* curve20 a */
	0x00, /* curve21 b */
	0x20, /* curve21 a */
	0x00, /* curve22 b */
	0x20, /* curve22 a */
	0x00, /* curve23 b */
	0x20, /* curve23 a */
	0x00, /* curve24 b */
	0xFF, /* curve24 a */
	0x04, /* cc r1 x */
	0x00,
	0x00, /* cc r2 */
	0x00,
	0x00, /* cc r3 */
	0x00,
	0x00, /* cc g1 */
	0x00,
	0x04, /* cc g2 */
	0x00,
	0x00, /* cc g3 */
	0x00,
	0x00, /* cc b1 */
	0x00,
	0x00, /* cc b2 */
	0x00,
	0x04, /* cc b3 */
	0x00,
};

static const unsigned char SEQ_MDNIE_2[] = {
	0xEB,
	0x01, /* mdnie_en */
	0x00, /* data_width mask 00 000 */
	0x30, /* scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0 */
	0x00, /* sharpen cc gamma 00 0 0 */
	/* end */
};

/* ea8061s */

static const unsigned char SEQ_SOURCE_SLEW_EA8061S[] = {
	0xBA,
	0x74,
};

static const unsigned char SEQ_S_WIRE_EA8061S[] = {
	0xB8,
	0x19, 0x00,
};

static const unsigned char SEQ_AID_SET_EA8061S[] = {
	0xB2,
	0x00, 0x00, 0x00, 0x0A,
};

static const unsigned char SEQ_ELVSS_SET_EA8061S[] = {
	0xB6,
	0xCC, 0x0B,
};

/* ACL OFFF */
static const unsigned char SEQ_ACL_SET_EA8061S[] = {
	0x55,
	0x00,
};

/* Tset 25 degree */
static const unsigned char SEQ_TSET_EA8061S[] = {
	0xB8,
	0x19,
};

static const unsigned char SEQ_MDNIE_1_EA8061S[] = {
	/* start */
	0xEC,
	0x00, /* de cs cc 000 */
	0x00, /* de_gain 10 */
	0x00,
	0x07, /* de_maxplus 11 */
	0xff,
	0x07, /* de_maxminus 11 */
	0xff,
	0x01, /* CS Gain */
	0x83,
	0x00, /* curve 1 b */
	0x20, /* curve 1 a */
	0x00, /* curve 2 b */
	0x20, /* curve 2 a */
	0x00, /* curve 3 b */
	0x20, /* curve 3 a */
	0x00, /* curve 4 b */
	0x20, /* curve 4 a */
	0x00, /* curve 5 b */
	0x20, /* curve 5 a */
	0x00, /* curve 6 b */
	0x20, /* curve 6 a */
	0x00, /* curve 7 b */
	0x20, /* curve 7 a */
	0x00, /* curve 8 b */
	0x20, /* curve 8 a */
	0x00, /* curve 9 b */
	0x20, /* curve 9 a */
	0x00, /* curve10 b */
	0x20, /* curve10 a */
	0x00, /* curve11 b */
	0x20, /* curve11 a */
	0x00, /* curve12 b */
	0x20, /* curve12 a */
	0x00, /* curve13 b */
	0x20, /* curve13 a */
	0x00, /* curve14 b */
	0x20, /* curve14 a */
	0x00, /* curve15 b */
	0x20, /* curve15 a */
	0x00, /* curve16 b */
	0x20, /* curve16 a */
	0x00, /* curve17 b */
	0x20, /* curve17 a */
	0x00, /* curve18 b */
	0x20, /* curve18 a */
	0x00, /* curve19 b */
	0x20, /* curve19 a */
	0x00, /* curve20 b */
	0x20, /* curve20 a */
	0x00, /* curve21 b */
	0x20, /* curve21 a */
	0x00, /* curve22 b */
	0x20, /* curve22 a */
	0x00, /* curve23 b */
	0x20, /* curve23 a */
	0x00, /* curve24 b */
	0xFF, /* curve24 a */
	0x00, /* ascr_skin_on strength 0 00000 */
	0x67, /* ascr_skin_cb */
	0xa9, /* ascr_skin_cr */
	0x0c, /* ascr_dist_up */
	0x0c, /* ascr_dist_down */
	0x0c, /* ascr_dist_right */
	0x0c, /* ascr_dist_left */
	0x00, /* ascr_div_up */
	0xaa,
	0xab,
	0x00, /* ascr_div_down */
	0xaa,
	0xab,
	0x00, /* ascr_div_right */
	0xaa,
	0xab,
	0x00, /* ascr_div_left */
	0xaa,
	0xab,
	0xd5, /* ascr_skin_Rr */
	0x2c, /* ascr_skin_Rg */
	0x2a, /* ascr_skin_Rb */
	0xff, /* ascr_skin_Yr */
	0xf5, /* ascr_skin_Yg */
	0x63, /* ascr_skin_Yb */
	0xfe, /* ascr_skin_Mr */
	0x4a, /* ascr_skin_Mg */
	0xff, /* ascr_skin_Mb */
	0xff, /* ascr_skin_Wr */
	0xf9, /* ascr_skin_Wg */
	0xf8, /* ascr_skin_Wb */
	0x00, /* ascr_Cr */
	0xff, /* ascr_Rr */
	0xff, /* ascr_Cg */
	0x14, /* ascr_Rg */
	0xff, /* ascr_Cb */
	0x14, /* ascr_Rb */
	0xff, /* ascr_Mr */
	0x00, /* ascr_Gr */
	0x00, /* ascr_Mg */
	0xff, /* ascr_Gg */
	0xff, /* ascr_Mb */
	0x00, /* ascr_Gb */
	0xff, /* ascr_Yr */
	0x00, /* ascr_Br */
	0xff, /* ascr_Yg */
	0x00, /* ascr_Bg */
	0x00, /* ascr_Yb */
	0xff, /* ascr_Bb */
	0xff, /* ascr_Wr */
	0x00, /* ascr_Kr */
	0xff, /* ascr_Wg */
	0x00, /* ascr_Kg */
	0xff, /* ascr_Wb */
	0x00, /* ascr_Kb */
};

static const unsigned char SEQ_MDNIE_2_EA8061S[] = {
	0xEB,
	0x01, /* mdnie_en */
	0x00, /* RGB_IF_Type mask 00 000 */
	0x30, /* scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0 */

	/* end */
};

#endif /* __S6E88A0_PARAM_H__ */

