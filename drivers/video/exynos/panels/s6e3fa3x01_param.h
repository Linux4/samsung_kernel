#ifndef __S6E3FA3X01_PARAM_H__
#define __S6E3FA3X01_PARAM_H__

#define GAMMA_PARAM_SIZE	34
#define ACL_PARAM_SIZE	ARRAY_SIZE(SEQ_ACL_OFF)
#define ELVSS_PARAM_SIZE	ARRAY_SIZE(SEQ_ELVSS_SET)
#define VINT_PARAM_SIZE	ARRAY_SIZE(SEQ_VINT_SETTING)
#define HBM_PARAM_SIZE	ARRAY_SIZE(SEQ_HBM_OFF)
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

static const unsigned char SEQ_REG_FF[] = {
	0xFF,
	0x00, 0x00, 0x20, 0x00,
};

static const unsigned char SEQ_GAMMA_CONTROL_SET_360CD[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char SEQ_AOR_CONTROL[] = {
	0xB2,
	0x00, 0xC2
};

static const unsigned char *pSEQ_AOR_CONTROL = SEQ_AOR_CONTROL;

static const unsigned char SEQ_ELVSS_SET[] = {
	0xB6,
	0xBC,				/* ACL OFF, T > 0 C */
	0x0A, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x03, 0x55, 0x54, 0x20,
	0x00, 0x0A, 0xAA, 0xAF, 0x0F,	/* Temp offset */
	0x02, 0x11, 0x11, 0x10,0x00,/* CAPS offset */
	0x00				/* Dummy for HBM ELVSS */
};

static const unsigned char SEQ_VINT_SETTING[] = {
	0xF4,
	0x7B, 0x1E,
};

static const unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7,
	0x03,
};

static const unsigned char SEQ_GAMMA_UPDATE_L[] = {
	0xF7,
	0x00,
};

static const unsigned char SEQ_MEM_ACCESS1[] = {
	0x2C,
	0x00,
};

static const unsigned char SEQ_MEM_ACCESS2[] = {
	0x3C,
	0x00,
};


static const unsigned char SEQ_SLEEP_IN[] = {
	0x10,
};

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11,
};

static const unsigned char SEQ_GPARAM_ELVSS[] = {
	0xB0,
	0x15
};

#ifdef CONFIG_SOC_EXYNOS5433
static const unsigned char SEQ_SET_TE_LINE[] = {
	0xB9,
	0x02, 0x07, 0x82, 0x00, 0x09
};
#else
static const unsigned char SEQ_SET_TE_LINE[] = {
	0xB9,
	0x02, 0x07, 0x7C, 0x07, 0x7E
};
#endif

static const unsigned char SEQ_TE_ON[] = {
	0x35,
	0x00
};

static const unsigned char SEQ_TE_OFF[] = {
	0x34,
};

static const unsigned char SEQ_GLOB_PARA[] = {
	0xB0,
	0x1E,
};

static const unsigned char SEQ_AVC_SETTING[] = {
	0xFD,
	0x94,
};

static const unsigned char SEQ_PCD_SETTING[] = {
	0xCC,
	0x5C,
};

static const unsigned char SEQ_ERR_FG_SETTING[] = {
	0xED,
	0x44,
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29,
};

static const unsigned char SEQ_DISPLAY_OFF[] = {
	0x28,
};

static const unsigned char SEQ_HBM_OFF[] = {
	0x53,
	0x00
};

static const unsigned char SEQ_HBM_ON[] = {
	0x53,
	0xC0
};

static const unsigned char SEQ_MIC_DISABLE[] = {
	0xF9,
	0x13,
};

static const unsigned char SEQ_ACL_OFF[] = {
	0x55,
	0x00
};

static const unsigned char SEQ_ACL_8[] = {
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

static unsigned char SEQ_PARTIAL_AREA[] = {
	0x30,
	0x00, 0x00, 0x00, 0x00
};

static unsigned char SEQ_WA_PRE_DISP_AREA[] = {
	0x30,
	0x07, 0x7E, 0x07, 0x7F,
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
	15, 16, 17, 19, 34, 72, 87, 105, 119, 134,
	152, 162, 207, 234, 265, 282, 316, 333, 360, 550
};

static const unsigned int *pELVSS_DIM_TABLE = ELVSS_DIM_TABLE;

static unsigned int ELVSS_STATUS_MAX = ARRAY_SIZE(ELVSS_DIM_TABLE);
#define ELVSS_STATUS_HBM	ELVSS_STATUS_MAX - 1
#define ELVSS_STATUS_360	ELVSS_STATUS_MAX - 2

enum {
	HBM_STATUS_OFF,
	HBM_STATUS_ON,
	HBM_STATUS_MAX,
};

static const unsigned char *HBM_TABLE[HBM_STATUS_MAX] = {
	SEQ_HBM_OFF,
	SEQ_HBM_ON,
};

static const unsigned char ELVSS_TABLE[][ELVSS_TABLE_NUM] = {
	{0x0E, 0x0E},
	{0x10, 0x10},
	{0x12, 0x12},
	{0x15, 0x15},
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
	{0x0E, 0x0E},
	{0x0D, 0x0D},
	{0x0C, 0x0C},
	{0x0B, 0x0B},
	{0x0A, 0x0A},
	{0x0A, 0x0A},
};

static const unsigned char (*pELVSS_TABLE)[ELVSS_TABLE_NUM] = ELVSS_TABLE;

enum {
	ACL_STATUS_0P,
	ACL_STATUS_8P,
	ACL_STATUS_MAX
};

static const unsigned char *ACL_CUTOFF_TABLE[ACL_STATUS_MAX] = {
	SEQ_ACL_OFF,
	SEQ_ACL_8,
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
	VINT_STATUS_550,
	VINT_STATUS_MAX
};

static const unsigned int VINT_DIM_TABLE[VINT_STATUS_MAX] = {
	5,	6,	7,	8,	9,
	10,	11,	12,	13,	550
};

static const unsigned char VINT_TABLE[VINT_STATUS_MAX] = {
	0x15, 0x16, 0x17, 0x18, 0x19,
	0x1A, 0x1B, 0x1C, 0x1D, 0x1E
};


#endif /* __S6E3FA3X01_PARAM_H__ */
