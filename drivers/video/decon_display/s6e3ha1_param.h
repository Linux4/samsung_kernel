#ifndef __S6E3HA1_PARAM_H__
#define __S6E3HA1_PARAM_H__

#define GAMMA_PARAM_SIZE	ARRAY_SIZE(SEQ_GAMMA_CONTROL_SET)
#define ACL_PARAM_SIZE	ARRAY_SIZE(SEQ_ACL_OFF)
#define OPR_PARAM_SIZE	ARRAY_SIZE(SEQ_ACL_OFF_OPR_AVR)
#define ELVSS_PARAM_SIZE	ARRAY_SIZE(SEQ_ELVSS_SET)
#define AID_PARAM_SIZE	ARRAY_SIZE(SEQ_AOR_CONTROL)

struct lcd_seq_info {
	unsigned char	*cmd;
	unsigned int	len;
	unsigned int	sleep;
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

static const unsigned char SEQ_TEST_KEY_ON_FC[] = {
	0xFC,
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_OFF_FC[] = {
	0xFC,
	0xA5, 0xA5,
};

static const unsigned char SEQ_REG_FF[] = {
	0xFF,
	0x00, 0x00, 0x00, 0x08
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
	0x00, 0x0A
};

static const unsigned char SEQ_ELVSS_SET[] = {
	0xB6,
	0x9C, 0x0F,
	/* Dummy for 21st HBM ELVSS Setting */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00
};

static const unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7,
	0x03,
};

static const unsigned char SEQ_GAMMA_UPDATE_L[] = {
	0xF7,
	0x00,
};

static const unsigned char SEQ_SLEEP_IN[] = {
	0x10,
};

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11,
};

static const unsigned char SEQ_TE_ON[] = {
	0x35,
	0x00
};

static const unsigned char SEQ_TE_OFF[] = {
	0x34,
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29,
};

static const unsigned char SEQ_DISPLAY_OFF[] = {
	0x28,
};

static const unsigned char SEQ_MIPI_SINGLE_DSI_SET1[] = {
	0xF2,
	0x07, 0x40
};

static const unsigned char SEQ_MIPI_SINGLE_DSI_SET2[] = {
	0xF9,
	0x09 /* 0x29 */
};

static const unsigned char SEQ_PSR_OFF[] = {
	0xB9,
	0x00
};

static const unsigned char SEQ_ACL_OFF[] = {
	0x55,
	0x00
};

static const unsigned char SEQ_ACL_15[] = {
	0x55,
	0x01,
};

static const unsigned char SEQ_ACL_OFF_OPR_AVR[] = {
	0xB5,
	0x41
};

static const unsigned char SEQ_ACL_ON_OPR_AVR[] = {
	0xB5,
	0x51
};

static const unsigned char SEQ_TOUCH_HSYNC_ON[] = {
	0xBD,
	0x85
};

static const unsigned char SEQ_TOUCH_VSYNC_ON[] = {
	0xFF,
	0x02
};

static const unsigned char SEQ_PENTILE_CONTROL[] = {
	0xC0,
	0x00, 0x0F, 0xD8, 0xD8
};

static const unsigned char SEQ_GLOBAL_PARA_33rd[] = {
	0xB0,
	0x20
};

static const unsigned char SEQ_POC_SETTING[] = {
	0xFE,
	0x08
};

static const unsigned char SEQ_SETUP_MARGIN[] = {
	0xFF,
	0x00, 0x00, 0x00, 0x08
};

static const unsigned char SEQ_COLUMN_ADDRESS[] = {
	0x2A,
	0x00, 0x00, 0x05, 0xFF
};

static const unsigned char SEQ_PAGE_ADDRESS[] = {
	0x2B,
	0x00, 0x00, 0x07, 0xFF
};

static const unsigned char SEQ_TSET_GLOBAL[] = {
	0xB0,
	0x05
};
static const unsigned char SEQ_AID_SET[] = {
	0xB2,
	0x00, 0x0A
};
static const unsigned char SEQ_ACL_OPR[] = {
	0xB5,
	0x51
};
static const unsigned char SEQ_TE_OUT[] = {
	0x35,
	0x00
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
	ELVSS_STATUS_072,
	ELVSS_STATUS_077,
	ELVSS_STATUS_082,
	ELVSS_STATUS_087,
	ELVSS_STATUS_093,
	ELVSS_STATUS_098,
	ELVSS_STATUS_105,
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
	ELVSS_STATUS_HBM,
	ELVSS_STATUS_MAX
};

static const unsigned int ELVSS_DIM_TABLE[ELVSS_STATUS_MAX] = {
	72,	77,	82,	87,	93,	98,	105,	111,	119,	126,
	134,	143,	152,	162,	172,	183,	195,	207,	220,	234,
	249,	265,	282,	300,	400
};

static const unsigned char ELVSS_TABLE[ACL_STATUS_MAX][ELVSS_STATUS_MAX] = {
	{
		0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
		0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
		0x12, 0x11, 0x10, 0x0F, 0x0F
	}, {
		0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
		0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
		0x13, 0x12, 0x11, 0x10, 0x0F
	}
};

enum {
	TSET_25_DEGREES,
	TSET_MINUS_0_DEGREES,
	TSET_MINUS_20_DEGREES,
	TSET_STATUS_MAX,
};

static const unsigned char TSET_TABLE[TSET_STATUS_MAX] = {
	0x19,	/* +25 degree */
	0x00,	/* -0 degree */
	0x94,	/* -20 degree */
};

static const unsigned char SEQ_GLOBAL_PARAM_TSET[] = {
	0xB0,
	0x05,
};

static const unsigned char SEQ_TSET[] = {
	0xB8,
	0x19,
};

enum {
	TEMP_ABOVE_MINUS_20_DEGREE,	/* T > -20 */
	TEMP_BELOW_MINUS_20_DEGREE,	/* T <= -20 */
	TEMP_MAX,
};

static const unsigned char MPS_TABLE[ACL_STATUS_MAX] = {
	0x9C,
	0x8C
};


#endif /* __S6E3HA1_PARAM_H__ */
