#ifndef __S6E3HA2_PARAM_H__
#define __S6E3HA2_PARAM_H__

#define GAMMA_PARAM_SIZE	ARRAY_SIZE(SEQ_GAMMA_CONTROL_SET)
#define ACL_PARAM_SIZE		ARRAY_SIZE(SEQ_ACL_OFF)
#define OPR_PARAM_SIZE		ARRAY_SIZE(SEQ_ACL_OFF_OPR)
#define ELVSS_PARAM_SIZE	ARRAY_SIZE(SEQ_ELVSS_SET)
#define AID_PARAM_SIZE		ARRAY_SIZE(SEQ_AID_SET)
#define HBM_PARAM_SIZE		ARRAY_SIZE(SEQ_HBM_OFF)
#define VINT_PARAM_SIZE		ARRAY_SIZE(SEQ_VINT_SET)

struct lcd_seq_info {
	unsigned char	*cmd;
	unsigned int	len;
	unsigned int	sleep;
};

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11
};

static const unsigned char SEQ_SLEEP_IN[] = {
	0x10,
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29
};

static const unsigned char SEQ_DISPLAY_OFF[] = {
	0x28,
};

static const unsigned char SEQ_TEST_KEY_ON_F0[] = {
	0xF0,
	0x5A, 0x5A
};

static const unsigned char SEQ_TEST_KEY_OFF_F0[] = {
	0xF0,
	0xA5, 0xA5
};

static const unsigned char SEQ_TEST_KEY_ON_F1[] = {
	0xF1,
	0x5A, 0x5A
};

static const unsigned char SEQ_TEST_KEY_OFF_F1[] = {
	0xF1,
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

static const unsigned char SEQ_SINGLE_DSI_1[] = {
	0xF2,
	0x67
};

static const unsigned char SEQ_SINGLE_DSI_2[] = {
	0xF9,
	0x09
};

static const unsigned char SEQ_TE_ON[] = {
	0x35,
	0x00
};

static const unsigned char SEQ_TSP_HSYNC_ON[] = {
	0xBD,
	0x11, 0x01, 0x02, 0x16, 0x02, 0x16
};

static const unsigned char SEQ_PENTILE_SETTING[] = {
	0xC0,
	0x00, 0x0F, 0xD8, 0xD8
};

static const unsigned char SEQ_POC_SETTING_1[] = {
	0xB0,
	0x20	/* Global para(33rd) */
};

static const unsigned char SEQ_POC_SETTING_2[] = {
	0xFE,
	0x08
};

static const unsigned char SEQ_ERR_FG_SETTING[] = {
	0xED,
	0x44
};

static const unsigned char SEQ_COLUMN_ADDRESS[] = {
	0x2A,
	0x00, 0x00, 0x05, 0xFF
};

static const unsigned char SEQ_PAGE_ADDRESS[] = {
	0x2B,
	0x00, 0x00, 0x07, 0xFF
};

static const unsigned char SEQ_GAMMA_CONTROL_SET[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x00, 0x00, 0x00, 0x00, 0x00
};

static const unsigned char SEQ_AID_SET[] = {
	0xB2,
	0x0A, 0x00
};

static const unsigned char SEQ_ELVSS_SET[] = {
	0xB6,
	0x9C,	/* MPS_CON: ACL OFF: CAPS ON + MPS_TEMP ON */
	0x0D,	/* ELVSS: MAX */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const unsigned char SEQ_VINT_SET[] = {
	0xF4,
	0xAB, 0x21
};

static const unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7,
	0x03
};

static const unsigned char SEQ_GAMMA_UPDATE_L[] = {
	0xF7,
	0x00
};

static const unsigned char SEQ_HBM_OFF[] = {
	0x53,
	0x00
};

static const unsigned char SEQ_HBM_ON[] = {
	0x53,
	0xC0
};

static const unsigned char SEQ_ACL_OFF_OPR[] = {
	0xB5,
	0x40	/* 16 Frame Avg */
};

static const unsigned char SEQ_ACL_ON_OPR[] = {
	0xB5,
	0x50	/* 32 Frame Avg */
};

static const unsigned char SEQ_ACL_OFF[] = {
	0x55,
	0x00
};

static const unsigned char SEQ_ACL_15[] = {
	0x55,
	0x02
};

static const unsigned char SEQ_PCD_SET_DET_LOW[] = {
	0xCC,
	0x5C
};

static const unsigned char SEQ_TE_START_SETTING[] = {
	0xB9,
	0x10, 0x07, 0xFF, 0x00, 0x09
};

enum {
	HBM_STATUS_OFF,
	HBM_STATUS_ON,
	HBM_STATUS_MAX,
};

static const unsigned char *HBM_TABLE[HBM_STATUS_MAX] = {
	SEQ_HBM_OFF,
	SEQ_HBM_ON,
};

enum {
	ACL_STATUS_0P,
	ACL_STATUS_15P,
	ACL_STATUS_MAX
};

static const unsigned char *ACL_CUTOFF_TABLE[ACL_STATUS_MAX] = {
	SEQ_ACL_OFF,
	SEQ_ACL_15,
};

enum {
	ACL_OPR_16_FRAME,
	ACL_OPR_32_FRAME,
	ACL_OPR_MAX
};

static const unsigned char *ACL_OPR_TABLE[ACL_OPR_MAX] = {
	SEQ_ACL_OFF_OPR,
	SEQ_ACL_ON_OPR,
};

enum {
	TEMP_ABOVE_MINUS_20_DEGREE,	/* T > -20 */
	TEMP_BELOW_MINUS_20_DEGREE,	/* T <= -20 */
	TEMP_MAX,
};

enum {
	CAPS_OFF,
	CAPS_ON,
	CAPS_MAX
};

/* 0x9C : CAPS ON + MPS_TEMP ON
0x8C : CAPS OFF + MPS_TEMP ON */
static const unsigned char MPS_TABLE[CAPS_MAX] = {0x8C, 0x9C};

/* write big value in order
ex1: 4nit~2nit = 4
ex2: 25nit = 25 */

enum {
	ELVSS_STATUS_004,
	ELVSS_STATUS_009,
	ELVSS_STATUS_014,
	ELVSS_STATUS_019,
	ELVSS_STATUS_024,
	ELVSS_STATUS_025,
	ELVSS_STATUS_027,
	ELVSS_STATUS_029,
	ELVSS_STATUS_030,
	ELVSS_STATUS_032,
	ELVSS_STATUS_034,
	ELVSS_STATUS_037,
	ELVSS_STATUS_039,
	ELVSS_STATUS_041,
	ELVSS_STATUS_044,
	ELVSS_STATUS_047,
	ELVSS_STATUS_050,
	ELVSS_STATUS_053,
	ELVSS_STATUS_056,
	ELVSS_STATUS_119,
	ELVSS_STATUS_126,
	ELVSS_STATUS_134,
	ELVSS_STATUS_143,
	ELVSS_STATUS_152,
	ELVSS_STATUS_162,
	ELVSS_STATUS_172,
	ELVSS_STATUS_183,
	ELVSS_STATUS_195,
	ELVSS_STATUS_207,
	ELVSS_STATUS_220,
	ELVSS_STATUS_234,
	ELVSS_STATUS_249,
	ELVSS_STATUS_265,
	ELVSS_STATUS_282,
	ELVSS_STATUS_300,
	ELVSS_STATUS_316,
	ELVSS_STATUS_333,
	ELVSS_STATUS_360,
	ELVSS_STATUS_MAX
};

static const unsigned int ELVSS_DIM_TABLE[ELVSS_STATUS_MAX] = {
	4,	9,	14,	19,	24,	25,	27,	29,	30,	32,
	34,	37,	39,	41,	44,	47,	50,	53,	56,	119,
	126,	134,	143,	152,	162,	172,	183,	195,	207,	220,
	234,	249,	265,	282,	300,	316,	333,	360
};

static const unsigned char ELVSS_TABLE[ELVSS_STATUS_MAX] = {
	0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x12, 0x12, 0x13, 0x13,
	0x13, 0x14, 0x14, 0x15, 0x15, 0x16, 0x17, 0x17, 0x18, 0x19,
	0x18, 0x18, 0x17, 0x17, 0x16, 0x15, 0x14, 0x14, 0x14, 0x14,
	0x13, 0x13, 0x12, 0x10, 0x10, 0x0F, 0x0E, 0x0D
};

enum {
	VINT_STATUS_005,
	VINT_STATUS_006,
	VINT_STATUS_007,
	VINT_STATUS_008,
	VINT_STATUS_009,
	VINT_STATUS_010,
	VINT_STATUS_011,
	VINT_STATUS_012,
	VINT_STATUS_013,
	VINT_STATUS_014,
	VINT_STATUS_MAX
};

static const unsigned int VINT_DIM_TABLE[VINT_STATUS_MAX] = {
	5,	6,	7,	8,	9,
	10,	11,	12,	13,	14
};

static const unsigned char VINT_TABLE[VINT_STATUS_MAX] = {
	0x21, 0x21, 0x21, 0x21, 0x21,
	0x21, 0x21, 0x21, 0x21, 0x21
};

#endif /* __S6E3HA2_PARAM_H__ */
