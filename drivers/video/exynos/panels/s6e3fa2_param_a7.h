#ifndef __S6E3FA2_PARAM_H__
#define __S6E3FA2_PARAM_H__

#define GAMMA_PARAM_SIZE	34
#define ACL_PARAM_SIZE	ARRAY_SIZE(SEQ_ACL_OFF)
#define ELVSS_PARAM_SIZE	ARRAY_SIZE(SEQ_ELVSS_SET)

#define AID_PARAM_SIZE	ARRAY_SIZE(SEQ_AOR_CONTROL)
#define ELVSS_TABLE_NUM 2

struct lcd_seq_info {
	unsigned char	*cmd;
	unsigned int	len;
	unsigned int	sleep;
};

enum {
	GAMMA_2CD,
	GAMMA_3CD,
	GAMMA_4CD,
	GAMMA_5CD,
	GAMMA_6CD,
	GAMMA_7CD,
	GAMMA_8CD,
	GAMMA_9CD,
	GAMMA_10CD,
	GAMMA_11CD,
	GAMMA_12CD,
	GAMMA_13CD,
	GAMMA_14CD,
	GAMMA_15CD,
	GAMMA_16CD,
	GAMMA_17CD,
	GAMMA_19CD,
	GAMMA_20CD,
	GAMMA_21CD,
	GAMMA_22CD,
	GAMMA_24CD,
	GAMMA_25CD,
	GAMMA_27CD,
	GAMMA_29CD,
	GAMMA_30CD,
	GAMMA_32CD,
	GAMMA_34CD,
	GAMMA_37CD,
	GAMMA_39CD,
	GAMMA_41CD,
	GAMMA_44CD,
	GAMMA_47CD,
	GAMMA_50CD,
	GAMMA_53CD,
	GAMMA_56CD,
	GAMMA_60CD,
	GAMMA_64CD,
	GAMMA_68CD,
	GAMMA_72CD,
	GAMMA_77CD,
	GAMMA_82CD,
	GAMMA_87CD,
	GAMMA_93CD,
	GAMMA_98CD,
	GAMMA_105CD,
	GAMMA_111CD,
	GAMMA_119CD,
	GAMMA_126CD,
	GAMMA_134CD,
	GAMMA_143CD,
	GAMMA_152CD,
	GAMMA_162CD,
	GAMMA_172CD,
	GAMMA_183CD,
	GAMMA_195CD,
	GAMMA_207CD,
	GAMMA_220CD,
	GAMMA_234CD,
	GAMMA_249CD,
	GAMMA_265CD,
	GAMMA_282CD,
	GAMMA_300CD,
	GAMMA_316CD,
	GAMMA_333CD,
	GAMMA_360CD,
	GAMMA_HBM,
	GAMMA_MAX
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
	0x00, 0x00, 0x20, 0x00,
};

static const unsigned char SEQ_GAMMA_CONTROL_SET_360CD[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x00, 0x00, 0x00,
};

static const unsigned char SEQ_AOR_CONTROL[] = {
	0xB2,
	0x00, 0x0E, 0x00, 0x0E,
};

static const unsigned char *pSEQ_AOR_CONTROL = SEQ_AOR_CONTROL;

static const unsigned char SEQ_ELVSS_SET[] = {
	0xB6,
	0x9C,				/* ACL OFF, T > 0 C */
	0x0B, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x03, 0x55, 0x54, 0x20,
	0x00, 0x0A, 0xAA, 0xAF, 0x0F,	/* Temp offset */
	0x02, 0x11, 0x11, 0x10,		/* CAPS offset */
	0x00				/* Dummy for HBM ELVSS */
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

static const unsigned char SEQ_GPARAM_TSET[] = {
	0xB0,
	0x05
};

static const unsigned char SEQ_TSET[] = {
	0xB8,
	0x19,
};

static const unsigned char SEQ_GPARAM_ELVSS[] = {
	0xB0,
	0x14
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

#ifdef CONFIG_DECON_MIC
static const unsigned char SEQ_MIC_ENABLE[] = {
	0xF9,
	0x0B,
};
#endif

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

static unsigned char SEQ_PARTIAL_AREA[] = {
	0x30,
	0x00, 0x00, 0x00, 0x00
};

static const unsigned char SEQ_NORMAL_DISP[] = {
	0x13,
};

static const unsigned char SEQ_PARTIAL_DISP[] = {
	0x12,
};

static const unsigned char SEQ_GPARAM_HBM_ELVSS[] = {
	0xB0,
	0x15
};

static const unsigned char SEQ_IDLE_MODE[] = {
	0x39,
};

static const unsigned char SEQ_NORMAL_MODE[] = {
	0x38,
};

static const unsigned char SEQ_PCD_SET_DET_LOW[] = {
	0xCC,
	0x5C, 0x51,
};

static const unsigned int DIM_TABLE[GAMMA_MAX] = {
	2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
	12, 13, 14, 15, 16, 17, 19, 20, 21, 22,
	24, 25, 27, 29, 30, 32, 34, 37, 39, 41,
	44, 47, 50, 53, 56, 60, 64, 68, 72, 77,
	82, 87, 93, 98, 105, 111, 119, 126, 134, 143,
	152, 162, 172, 183, 195, 207, 220, 234, 249, 265,
	282, 300, 316, 333, 360, 550,
};

static const unsigned int ELVSS_DIM_TABLE[] = {
	20, 21, 22, 24, 25, 27, 29, 34, 50, 64,
	82, 98, 110, 134, 143, 162, 195, 220, 234, 249,
	265, 300, 316, 333, 360, 550,
};

static const unsigned int *pELVSS_DIM_TABLE = ELVSS_DIM_TABLE;

static unsigned int ELVSS_STATUS_MAX = ARRAY_SIZE(ELVSS_DIM_TABLE);
#define ELVSS_STATUS_HBM	ELVSS_STATUS_MAX - 1
#define ELVSS_STATUS_360	ELVSS_STATUS_MAX - 2

static const unsigned char ELVSS_TABLE[][ELVSS_TABLE_NUM] = {
	{0x07, 0x07},
	{0x09, 0x09},
	{0x0B, 0x0B},
	{0x0D, 0x0D},
	{0x0F, 0x0F},
	{0x11, 0x11},
	{0x14, 0x14},
	{0x1A, 0x1A},
	{0x19, 0x19},
	{0x18, 0x18},
	{0x17, 0x17},
	{0x16, 0x16},
	{0x15, 0x15},
	{0x14, 0x14},
	{0x13, 0x13},
	{0x12, 0x12},
	{0x11, 0x11},
	{0x10, 0x10},
	{0x0F, 0x0F},
	{0x10, 0x10},
	{0x0F, 0x0F},
	{0x0E, 0x0E},
	{0x0D, 0x0D},
	{0x0C, 0x0C},
	{0x0B, 0x0B},
	{0x0B, 0x0B},
};

static const unsigned char (*pELVSS_TABLE)[ELVSS_TABLE_NUM] = ELVSS_TABLE;

enum {
	ACL_STATUS_0P,
	ACL_STATUS_15P,
	ACL_STATUS_MAX
};

static const unsigned char *ACL_CUTOFF_TABLE[ACL_STATUS_MAX] = {
	SEQ_ACL_OFF,
	SEQ_ACL_15,
};

#endif /* __S6E3FA2_PARAM_H__ */
