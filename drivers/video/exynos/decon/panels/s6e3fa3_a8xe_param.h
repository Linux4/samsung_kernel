#ifndef __S6E3FA3_A8XE_PARAM_H__
#define __S6E3FA3_A8XE_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>

#define EXTEND_BRIGHTNESS	365
#define UI_MAX_BRIGHTNESS	255
#define UI_MIN_BRIGHTNESS	0
#define UI_DEFAULT_BRIGHTNESS	128
#define NORMAL_TEMPERATURE	25	/* 25 degrees Celsius */

#define S6E3FA3_AID_CMD_CNT 3
#define S6E3FA3_AID_REG_OFFSET 1
#define S6E3FA3_TSET_REG 0xB8    /* TSET: Global para 8th */
#define S6E3FA3_TSET_LEN 2


#define S6E3FA3_MAX_BRIGHTNESS 	420
#define S6E3FA3_HBM_BRIGHTNESS  600

#define S6E3FA3_COORDINATE_REG	0xA1
#define S6E3FA3_COORDINATE_LEN	4
#define S6E3FA3_ID_REG		0x04
#define S6E3FA3_ID_LEN		3
#define S6E3FA3_CHIP_ID_REG	0xD6
#define S6E3FA3_CHIP_ID_LEN	5
#define S6E3FA3_MTP_REG		0xC8
#define S6E3FA3_MTP_LEN		35
#define S6E3FA3_HBMGAMMA_REG		0xB4
#define S6E3FA3_HBMGAMMA_LEN		33
#define S6E3FA3_MTP_DATE_REG	S6E3FA3_MTP_REG
#define S6E3FA3_MTP_DATE_LEN	47
#define S6E3FA3_MTP_DATE_OFFSET	40
#define S6E3FA3_ELVSS_LEN 	23
#define S6E3FA3_ELVSS_REG 	0xB6
#define S6E3FA3_ELVSS_CMD_CNT 	3
#define S6E3FA3_ELVSS_22TH_CMD_CNT 	2
#define S6E3FA3_VINT_REG 	0xF4
#define S6E3FA3_VINT_1ST 	0x7B
#define S6E3FA3_VINT_LEN 	3
#define S6E3FA3_ACL_CMD_LEN	ARRAY_SIZE(SEQ_ACL_ON)
#define S6E3FA3_ACL_OPR_LEN	ARRAY_SIZE(SEQ_OPR_ACL_ON)
#define S6E3FA3_ACL_OPR_IDX_START	2
#define S6E3FA3_ACL_OPR_IDX_RATIO	4
#define S6E3FA3_IS_8MASK(id)	(((id >> 4) & 0x03) == 0x00)
#define S6E3FA3_IS_7MASK(id)	(((id >> 4) & 0x03) == 0x01)

#ifdef CONFIG_CHECK_OCTA_CHIP_ID
#define S6E3FA3_OCTAID_REG	0xC9
#define S6E3FA3_OCTAID_LEN	21
#endif

#define LEVEL_IS_HBM(brightness)	(brightness == EXTEND_BRIGHTNESS)

#ifdef CONFIG_LCD_DOZE_MODE
#define	ALPM_OFF			0
#define ALPM_ON_LOW			1
#define HLPM_ON_LOW			2
#define ALPM_ON_HIGH			3
#define HLPM_ON_HIGH			4
#endif

#if defined(CONFIG_LCD_ALPM) || defined(CONFIG_LCD_DOZE_MODE)
#define UNSUPPORT_ALPM					0
#define SUPPORT_30HZALPM				1
#define SUPPORT_LOWHZALPM				2
#endif

struct lcd_seq_info {
	unsigned char	*cmd;
	unsigned int	len;
	unsigned int	sleep;
};

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11
};

static const unsigned char SEQ_SLEEP_IN[] = {
	0x10
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29
};

static const unsigned char SEQ_DISPLAY_OFF[] = {
	0x28
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
	0x5A, 0x5A
};

static const unsigned char SEQ_TEST_KEY_OFF_FC[] = {
	0xFC,
	0xA5, 0xA5
};

static const unsigned char SEQ_TE_ON[] = {
	0x35,
	0x00
};

static const unsigned char SEQ_PCD_SET_DET_HIGH[] = {
	0xCC,
	0x4C
};

static const unsigned char SEQ_ERR_FG_SETTING[] = {
	0xED,
	0x44
};

static const unsigned char SEQ_AVC_SETTING_1[] = {
	0xB0,
	0x1E
};

static const unsigned char SEQ_AVC_SETTING_2[] = {
	0xFD,
	0x94
};

static const unsigned char SEQ_AVC_SETTING_2_7MASK[] = {
	0xFD,
	0xA8
};

static const unsigned char SEQ_TSHL_SETTING_1[] = {
	0xB0,
	0x10
};

static const unsigned char SEQ_TSHL_SETTING_2[] = {
	0xB6,
	0x7F
};

static const unsigned char SEQ_GAMMA_CONDITION_SET[] = {
	0xCA,
	0x01, 0x00, 0x01,
	0x00, 0x01, 0x00,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x00, 0x00, 0x00,
	0x00, 0x00
};

static const unsigned char SEQ_AID_SETTING[] = {
	0xB2,
	0x00, 0xC2
};

static const unsigned char SEQ_AID_SETTING_7MASK[] = {
	0xB2,
	0x00, 0x0C
};

static const unsigned char SEQ_ELVSS_SET[] = {
	0xB6,
	0xBC,	/* B6h 1st Para: MPS_CON */
	0x0A	/* B6h 2nd Para: ELVSS_Dim_offset */
};

static const unsigned char SEQ_ELVSS_22TH_1[] = {
	0xB0,
	0x15
};

static const unsigned char SEQ_ELVSS_22TH_2[] = {
	0xB6,
	0x0D
};

static const unsigned char SEQ_VINT_SET[] = {
	0xF4,
	0x7B, 0x1E
};

static const unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7,
	0x03
};

static const unsigned char SEQ_HBM_OFF[] = {
	0x53,
	0x00
};

static const unsigned char SEQ_HBM_ON[] = {
	0x53,
	0xC0
};

static const unsigned char SEQ_OPR_ACL_OFF[] = {
	0xB5,
	0x40, 0x7F, 0x14, 0x14	/* 16 Frame Avg. at ACL Off, Start point 50%, CONST, ACL 15% */
};

static const unsigned char SEQ_OPR_ACL_ON[] = {
	0xB5,
	0x50, 0x7F, 0x14, 0x14	/* 32 Frame Avg. at ACL On, Start point 50%, CONST, ACL 15% */
};

static const unsigned char SEQ_OPR_ACL_ON_HBM[] = {
	0xB5,
	0x50, 0x99, 0x14, 0x0A	/* 32 Frame Avg. at ACL On, Start point 60%, CONST, ACL 8% */
};

static const unsigned char SEQ_ACL_OFF[] = {
	0x55,
	0x00
};

static const unsigned char SEQ_ACL_ON[] = {
	0x55,
	0x02	/* 0x02 : ACL 8% (Default) */
};

static const unsigned char SEQ_TSET_SETTING[] = {
	0xB8,
	0x19	/* (ex) 25 degree : 0x19 */
};

static const unsigned char SEQ_TE_TIMING[] = {
	0xB9,
	0x02,
	0x07, 0x82,
	0x00, 0x09
};

static const unsigned char SEQ_SELECT_HLPM[] = {
	0xBB,
	0x54, 0x00, 0x07, 0x30
};

static const unsigned char SEQ_SELECT_ALPM[] = {
	0xBB,
	0xC4, 0x00, 0x07, 0x30
};

static const unsigned char SEQ_ALPM_ON_60[] = {
	0x53,
	0x02
};

static const unsigned char SEQ_ALPM_ON_2[] = {
	0x53,
	0x03
};

static const unsigned char SEQ_ALPM_OFF[] = {
	0x53,
	0x00
};

static const unsigned char SEQ_SRC_LOAD_EN_HLPM[] = {
	0xFD,
	0xA1, 0x20, 0x08, 0x04, 0x44, 0x00,
	0x8C
};

static const unsigned char SEQ_SRC_LOAD_EN_ALPM[] = {
	0xFD,
	0xA1, 0x20, 0x08, 0x04, 0x44, 0x00,
	0x0C
};

static const unsigned char SEQ_LTPS_EQ_1[] = {
	0xB0,
	0x24
};

static const unsigned char SEQ_LTPS_EQ_2[] = {
	0xFD,
	0x01, 0x01, 0x01
};

static const unsigned char SEQ_PARTIAL_MODE_ON[] = {
	0x12
};

static const unsigned char SEQ_NORMAL_MODE_ON[] = {
	0x13
};

static const unsigned char SEQ_PARTIAL_AREA_SETTING[] = {
	0x30,
	0x07, 0x7E, 0x07, 0x7F
};

enum {
	ACL_STATUS_OFF,
	ACL_STATUS_ON,
	ACL_STATUS_HBM,
	ACL_STATUS_MAX
};

enum {
	OPR_STATUS_ACL_OFF,
	OPR_STATUS_ACL_ON,
	OPR_STATUS_ACL_HBM,
	OPR_STATUS_MAX
};

enum {
	CAPS_OFF,
	CAPS_ON,
	CAPS_MAX
};

enum {
	TEMP_ABOVE_MINUS_00_DEGREE,	/* T > 0 */
	TEMP_ABOVE_MINUS_20_DEGREE,	/* -20 < T <= 0 */
	TEMP_BELOW_MINUS_20_DEGREE,	/* T <= -20 */
	TEMP_MAX
};

enum {
	HBM_STATUS_OFF,
	HBM_STATUS_ON,
	HBM_STATUS_MAX
};

enum {
	HBM_INTER_OFF = 0,
	HBM_COLORBLIND_ON,
	HBM_GALLERY_ON,
};

static const unsigned int VINT_DIM_TABLE[] = {
	5, 6, 7, 8, 9,
	10, 11, 12
};

static const unsigned char VINT_TABLE[] = {
	0x17, 0x18, 0x19, 0x1A, 0x1B,
	0x1C, 0x1D, 0x1E
};


#endif /* __S6E3FA3_A8XE_PARAM_H__ */
