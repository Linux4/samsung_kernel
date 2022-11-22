#ifndef __EA8064G_PARAM_H__
#define __EA8064G_PARAM_H__

#define GAMMA_PARAM_SIZE	ARRAY_SIZE(SEQ_GAMMA_CONTROL_SET)
#define ACL_PARAM_SIZE	ARRAY_SIZE(SEQ_ACL_OFF)
#define OPR_PARAM_SIZE	ARRAY_SIZE(SEQ_ACL_OFF_OPR_AVR)
#define ELVSS_PARAM_SIZE	ARRAY_SIZE(SEQ_ELVSS_SET)
#define AID_PARAM_SIZE	ARRAY_SIZE(SEQ_AOR_CONTROL)
#define TSET_PARAM_SIZE	ARRAY_SIZE(SEQ_TSET)
#define HBM_PARAM_SIZE	ARRAY_SIZE(SEQ_HBM_OFF)

struct lcd_seq_info {
	unsigned char	*cmd;
	unsigned int	len;
	unsigned int	sleep;
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
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_OFF_FC[] = {
	0xFC,
	0xA5, 0xA5,
};

static const unsigned char SEQ_SLEEP_IN[] = {
	0x10,
};

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11,
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29,
};

static const unsigned char SEQ_DISPLAY_OFF[] = {
	0x28,
};

static const unsigned char SEQ_GAMMA_CONTROL_SET[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x00, 0x00, 0x00
};

static const unsigned char SEQ_AOR_CONTROL[] = {
	0xB2,
	0x00, 0x08, 0x00, 0x08
};

static const unsigned char SEQ_ELVSS_SET[] = {
	0xB6,
	0x5C,	/* MPS: ACL OFF */
	0x84	/* ELVSS: MAX */
};

static const unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7,
	0x01, 0x00
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
	0x21	/* 16Frame Averaging. at ACL OFF */
};

static const unsigned char SEQ_ACL_ON_OPR_AVR[] = {
	0xB5,
	0x29	/* 32Frame Averaging. at ACL ON */
};

static const unsigned char SEQ_TSET[] = {
	0xB8,
	0x19,	/* 25 degree */
	0x04	/* DCDC1 Set */
};

static const unsigned char SEQ_TE_ON[] = {
	0x35,
	0x00, 0x00
};

static const unsigned char SEQ_TE_OFF[] = {
	0x34,
};

static const unsigned char SEQ_COLUMN_ADDRESS[] = {
	0x2A,
	0x00, 0x00, 0x02, 0xCF
};

static const unsigned char SEQ_PAGE_ADDRESS[] = {
	0x2B,
	0x00, 0x00, 0x04, 0xFF
};

static const unsigned char SEQ_TOUCH_HSYNC[] = {
	0xB9,
	0x03, 0x15, 0x1B
};

static const unsigned char SEQ_SOURCE_CONTROL[] = {
	0xBA,
	0x32, 0x30, 0x01
};

static const unsigned char SEQ_HBM_OFF[] = {
	0x53,
	0x00
};

static const unsigned char SEQ_HBM_ON[] = {
	0x53,
	0xD0
};

static const unsigned char SEQ_PCD_SET_DET_LOW[] = {
	0xCC,
	0x57, 0x3A,
};

static const unsigned char SEQ_ESD_FG_ENABLE[] = {
	0xCB,
	0x56
};

static const unsigned char SEQ_GPARA_ESD_FG_POLARITY[] = {
	0xB0,
	0x05
};

static const unsigned char SEQ_ESD_FG_POLARITY[] = {
	0xFF,
	0x20
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
	SEQ_ACL_OFF_OPR_AVR,
	SEQ_ACL_ON_OPR_AVR,
};

enum {
	ELVSS_STATUS_003,
	ELVSS_STATUS_005,
	ELVSS_STATUS_007,
	ELVSS_STATUS_009,
	ELVSS_STATUS_011,
	ELVSS_STATUS_013,
	ELVSS_STATUS_015,
	ELVSS_STATUS_017,
	ELVSS_STATUS_020,
	ELVSS_STATUS_072,
	ELVSS_STATUS_093,
	ELVSS_STATUS_111,
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
	ELVSS_STATUS_HBM,
	ELVSS_STATUS_MAX
};

static const unsigned int ELVSS_DIM_TABLE[ELVSS_STATUS_MAX] = {
	3,	5,	7,	9 ,	11,	13,	15,	17,	20,	72,
	93,	111,	119,	126,	134,	143,	152,	162,	172,	183,
	195,	207,	220,	234,	249,	265,	282,	300,	316,	333,
	360,	500
};

static const unsigned char ELVSS_TABLE[ACL_STATUS_MAX][ELVSS_STATUS_MAX] = {
	{
		0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D,
		0x8C, 0x8B, 0x8A, 0x8A, 0x89, 0x89, 0x88, 0x88, 0x88, 0x88,
		0x88, 0x88, 0x87, 0x87, 0x87, 0x86, 0x86, 0x85, 0x85, 0x84,
		0x84, 0x84
	}, {
		0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D,
		0x8C, 0x8B, 0x8A, 0x8A, 0x89, 0x89, 0x88, 0x88, 0x88, 0x88,
		0x88, 0x88, 0x87, 0x87, 0x87, 0x86, 0x86, 0x85, 0x85, 0x84,
		0x84, 0x84
	}
};

static const unsigned char MPS_TABLE[ACL_STATUS_MAX] = {0x5C, 0x4C};

#endif /* __EA8064G_PARAM_H__ */
