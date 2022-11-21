#ifndef __S6E3HF2_PARAM_H__
#define __S6E3HF2_PARAM_H__

#define GAMMA_PARAM_SIZE	ARRAY_SIZE(SEQ_GAMMA_CONTROL_SET)
#define ACL_PARAM_SIZE	ARRAY_SIZE(SEQ_ACL_OFF)
#define OPR_PARAM_SIZE	ARRAY_SIZE(SEQ_ACL_OFF_OPR_AVR)
#define ELVSS_PARAM_SIZE	ARRAY_SIZE(SEQ_ELVSS_SET)
#define AID_PARAM_SIZE	ARRAY_SIZE(SEQ_AOR_CONTROL)
#define HBM_PARAM_SIZE	ARRAY_SIZE(SEQ_HBM_OFF)

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

static const unsigned char SEQ_GAMMA_CONTROL_SET[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x00, 0x00
};

static const unsigned char SEQ_AOR_CONTROL[] = {
	0xB2,
	0x03, 0x10
};

static const unsigned char SEQ_ELVSS_SET[] = {
	0xB6,
	0x9C,	/* MPS_CON: ACL OFF: CAPS ON + MPS_TEMP ON */
	0x0A,	/* ELVSS: MAX */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
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

static const unsigned char SEQ_HBM_OFF[] = {
	0x53,
	0x00
};

static const unsigned char SEQ_HBM_ON[] = {
	0x53,
	0xC0
};

#ifdef CONFIG_DECON_MIC
static const unsigned char SEQ_SINGLE_DSI_SET1[] = {
	0xF2,
	0x67
};

static const unsigned char SEQ_SINGLE_DSI_SET2[] = {
	0xF9,
	0x09,
};
#else
static const unsigned char SEQ_SINGLE_DSI_SET1[] = {
	0xF2,
	0x63,
};

static const unsigned char SEQ_SINGLE_DSI_SET2[] = {
	0xF9,
	0x0A,
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
	0x40
};

static const unsigned char SEQ_ACL_ON_OPR_AVR[] = {
	0xB5,
	0x50
};

static const unsigned char SEQ_TOUCH_HSYNC_ON[] = {
	0xBD,
	0x11, 0x11, 0x02, 0x16, 0x02, 0x16
};

static const unsigned char SEQ_PENTILE_CONTROL[] = {
	0xC0,
	0x00, 0x00, 0xD8, 0xD8
};

static const unsigned char SEQ_POC_SETTING1[] = {
	0xC3,
	0xC0, 0x00, 0x33
};
static const unsigned char SEQ_POC_SETTING2[] = {
	0xB0,
	0x20
};
static const unsigned char SEQ_POC_SETTING3[] = {
	0xFE,
	0x08
};

static const unsigned char SEQ_PCD_SET_OFF[] = {
	0xCC,
	0x40, 0x51
};

static const unsigned char SEQ_ERR_FG_SETTING[] = {
	0xED,
	0x44
};

static unsigned char SEQ_PARTIAL_AREA[] = {
	0x30,
	0x00, 0x00, 0x00, 0x00
};

static const unsigned char SEQ_PARTIAL_MODE_ON[] = {
	0x12,
};

static const unsigned char SEQ_TE_START_SETTING[] = {
	0xB9,
	0x10, 0x09, 0xFF, 0x00, 0x09
};


static const unsigned char SEQ_FREQ_MEMCLOCK_SETTING[] = {
	0xFE,
	0x20, 0x19, 0x05, 0x00, 0x00, 0x00, 0x00, 0x96, 0x2C, 0x01,
	0x00, 0x12, 0x22, 0x50, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00
};

static const unsigned char SEQ_FREQ_CALIBRATION_SETTING1[] = {
	0xFD,
	0x1C
};

static const unsigned char SEQ_FREQ_CALIBRATION_SETTING2[] = {
	0xF2,
	0x67, 0x40, 0xC5
};

static const unsigned char SEQ_FREQ_CALIBRATION_SETTING3[] = {
	0xFE,
	0x20, 0x39
};

static const unsigned char SEQ_FREQ_CALIBRATION_SETTING4[] = {
	0xFE,
	0xA0
};

static const unsigned char SEQ_FREQ_CALIBRATION_SETTING5[] = {
	0xFE,
	0x20
};

static const unsigned char SEQ_FREQ_CALIBRATION_SETTING6[] = {
	0xCE,
	0x03, 0x3B, 0x14, 0x6D,
	0x40, 0x80, 0xC0, 0x28, 0x28, 0x28, 0x28, 0x39,
	0xC5
};

static struct lcd_seq_info SEQ_LDI_FPS_SET[] = {
	{(u8 *)SEQ_FREQ_CALIBRATION_SETTING1, ARRAY_SIZE(SEQ_FREQ_CALIBRATION_SETTING1), 0},
	{(u8 *)SEQ_FREQ_CALIBRATION_SETTING2, ARRAY_SIZE(SEQ_FREQ_CALIBRATION_SETTING2), 0},
	{(u8 *)SEQ_FREQ_CALIBRATION_SETTING3, ARRAY_SIZE(SEQ_FREQ_CALIBRATION_SETTING3), 0},
	{(u8 *)SEQ_FREQ_CALIBRATION_SETTING4, ARRAY_SIZE(SEQ_FREQ_CALIBRATION_SETTING4), 0},
	{(u8 *)SEQ_FREQ_CALIBRATION_SETTING5, ARRAY_SIZE(SEQ_FREQ_CALIBRATION_SETTING5), 0},
	{(u8 *)SEQ_FREQ_CALIBRATION_SETTING6, ARRAY_SIZE(SEQ_FREQ_CALIBRATION_SETTING6), 0},
};

static struct lcd_seq_info SEQ_POC_SET[] = {
	{(u8 *)SEQ_POC_SETTING1, ARRAY_SIZE(SEQ_POC_SETTING1), 0},
	{(u8 *)SEQ_POC_SETTING2, ARRAY_SIZE(SEQ_POC_SETTING2), 0},
	{(u8 *)SEQ_POC_SETTING3, ARRAY_SIZE(SEQ_POC_SETTING3), 0},
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

enum {
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
	ELVSS_STATUS_316,
	ELVSS_STATUS_333,
	ELVSS_STATUS_350,
	ELVSS_STATUS_HBM,
	ELVSS_STATUS_MAX
};

static const unsigned int ELVSS_DIM_TABLE[ELVSS_STATUS_MAX] = {
	77, 82, 87, 93, 98, 105, 110, 119, 126, 134,
	143, 152, 162, 172, 183, 195, 207, 220, 234, 249,
	265, 282, 300, 316, 333, 350, 500
};

static const unsigned char ELVSS_TABLE[ELVSS_STATUS_MAX] = {
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x10, 0x10, 0x10, 0x0F,
	0x0F, 0x0F, 0x0E, 0x0D, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
	0x0C, 0x0B, 0x0B, 0x0A, 0x0A, 0x0A, 0x0A
};

#ifdef CONFIG_LCD_ALPM
/* ALPM Commands */

static const unsigned char SEQ_ALPM2NIT_MODE_ON[] = {
	0x53, 0x23
};

static const unsigned char SEQ_NORMAL_MODE_ON[] = {
	0x53, 0x00
};

static struct lcd_seq_info SEQ_ALPM_ON_SET[] = {
	{(u8 *)SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF), 17},
	{(u8 *)SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0), 0},
	{(u8 *)SEQ_ALPM2NIT_MODE_ON, ARRAY_SIZE(SEQ_ALPM2NIT_MODE_ON), 0},
	{(u8 *)SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE), 0},
	{(u8 *)SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(SEQ_GAMMA_UPDATE_L), 0},
	{(u8 *)SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0), 17},
	{(u8 *)SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON), 0},
};

static struct lcd_seq_info SEQ_ALPM_OFF_SET[] = {
	{(u8 *)SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF), 17},
	{(u8 *)SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0), 0},
	{(u8 *)SEQ_NORMAL_MODE_ON, ARRAY_SIZE(SEQ_NORMAL_MODE_ON), 0},
	{(u8 *)SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE), 0},
	{(u8 *)SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(SEQ_GAMMA_UPDATE_L), 0},
	{(u8 *)SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0), 17},
	{(u8 *)SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON), 0},
};

static const unsigned char SEQ_GLOBAL_PARAM_21[] = {
	0xB0,
	0x15
};

static const unsigned char VDDMRVDD_LUT[] = {
	72,
	72,
	71,
	70,
	69,
	68,
	67,
	66,
	65,
	64,
	1,
	2,
	3,
	4,
	5,
	6,
	7,
	8,
	9,
	10,
	11,
	12,
	13,
	14,
	15,
	16,
	17,
	18,
	19,
	20,
	21,
	22,
	23,
	24,
	25,
	26,
	27,
	28,
	29,
	30,
	31,
	32,
	33,
	34,
	35,
	36,
	37,
	38,
	39,
	40,
	41,
	42,
	43,
	44,
	45,
	46,
	47,
	48,
	49,
	50,
	51,
	52,
	53,
	54,
	73,
	74,
	75,
	76,
	77,
	78,
	79,
	80,
	81,
	82,
	83,
	84,
	85,
	86,
	87,
	88,
	89,
	90,
	91,
	92,
	93,
	94,
	95,
	96,
	97,
	98,
	99,
	100,
	101,
	102,
	103,
	104,
	105,
	106,
	107,
	108,
	109,
	110,
	111,
	112,
	113,
	114,
	115,
	116,
	117,
	118,
	119,
	120,
	121,
	122,
	123,
	124,
	125,
	126,
	127,
	127,
	127,
	127,
	127,
	127,
	127,
	127,
	127,
	127
};

#endif

#ifdef CONFIG_LCD_HMT
static const unsigned char SEQ_HMT_SINGLE1[] = {	/* G.Param */
	0xB0, 0x03
};

static const unsigned char SEQ_HMT_SINGLE2[] = { /* Display Setting. Porch Setting*/
	0xF2, 0x0A, 0x06
};

static const unsigned char SEQ_HMT_SINGLE3[] = {	/* G.Param */
	0xB0, 0x05
};

static const unsigned char SEQ_HMT_SINGLE4[] = {	/* AID Setting AID 1-Cycle */
	0xB2, 0x00
};

static const unsigned char SEQ_HMT_SINGLE5[] = {		/* G.Param */
	0xB0, 0x06
};

static const unsigned char SEQ_HMT_SINGLE6[] = {		/* Display Setting. Porch Data Setting*/
	0xF3, 0x10
};

static const unsigned char SEQ_HMT_SINGLE7[] = {		/* PASET Setting  */
	0x2B,
	0x00, 0x00, 0x09, 0xFF
};

static const unsigned char SEQ_HMT_OFF1[] = {	/* G.Param */
	0xB0, 0x03
};
static const unsigned char SEQ_HMT_OFF2[] = {	/* Display setting. porch setting */
	0xF2, 0x06, 0x0A
};
static const unsigned char SEQ_HMT_OFF3[] = {	/* G.Param */
	0xB0, 0x05
};
static const unsigned char SEQ_HMT_OFF4[] = {	/* AID Setting AID 4-Cycle */
	0xB2, 0x40
};

static const unsigned char SEQ_HMT_AID_FORWARD1[] = {	/* G.Param */
	0xB0, 0x09
};
static const unsigned char SEQ_HMT_AID_FORWARD2[] = {	/*LTPS Setting */
	0xCB, 0x09
};
static const unsigned char SEQ_HMT_AID_FORWARD3[] = {	/* G.Param */
	0xB0, 0x0F
};
static const unsigned char SEQ_HMT_AID_FORWARD4[] = {	/*LTPS Setting */
	0xCB, 0x05
};
static const unsigned char SEQ_HMT_AID_FORWARD5[] = {	/* G.Param */
	0xB0, 0x1D
};
static const unsigned char SEQ_HMT_AID_FORWARD6[] = {	/* LTPS Setting  */
	0xCB, 0x00
};
static const unsigned char SEQ_HMT_AID_FORWARD7[] = {	/* G.Param */
	0xB0, 0x33
};
static const unsigned char SEQ_HMT_AID_FORWARD8[] = {	/* LTPS Setting  */
	0xCB, 0xCB
};


static const unsigned char SEQ_HMT_AID_REVERSE1[] = {	/* G.Param */
	0xB0, 0x09
};
static const unsigned char SEQ_HMT_AID_REVERSE2[] = {	/*LTPS Setting */
	0xCB, 0x0B
};
static const unsigned char SEQ_HMT_AID_REVERSE3[] = {	/* G.Param */
	0xB0, 0x0F
};
static const unsigned char SEQ_HMT_AID_REVERSE4[] = {	/*LTPS Setting */
	0xCB, 0x09
};
static const unsigned char SEQ_HMT_AID_REVERSE5[] = {	/* G.Param */
	0xB0, 0x1D
};
static const unsigned char SEQ_HMT_AID_REVERSE6[] = {	/*LTPS Setting */
	0xCB, 0x10
};
static const unsigned char SEQ_HMT_AID_REVERSE7[] = {	/* G.Param */
	0xB0, 0x33
};
static const unsigned char SEQ_HMT_AID_REVERSE8[] = {	/* LTPS Setting  */
	0xCB, 0xCC
};


static struct lcd_seq_info SEQ_HMT_AID_FORWARD_SET[] = {
	{(u8 *)SEQ_HMT_AID_FORWARD1, ARRAY_SIZE(SEQ_HMT_AID_FORWARD1), 0},
	{(u8 *)SEQ_HMT_AID_FORWARD2, ARRAY_SIZE(SEQ_HMT_AID_FORWARD2), 0},
	{(u8 *)SEQ_HMT_AID_FORWARD3, ARRAY_SIZE(SEQ_HMT_AID_FORWARD3), 0},
	{(u8 *)SEQ_HMT_AID_FORWARD4, ARRAY_SIZE(SEQ_HMT_AID_FORWARD4), 0},
	{(u8 *)SEQ_HMT_AID_FORWARD5, ARRAY_SIZE(SEQ_HMT_AID_FORWARD5), 0},
	{(u8 *)SEQ_HMT_AID_FORWARD6, ARRAY_SIZE(SEQ_HMT_AID_FORWARD6), 0},
	{(u8 *)SEQ_HMT_AID_FORWARD7, ARRAY_SIZE(SEQ_HMT_AID_FORWARD7), 0},
	{(u8 *)SEQ_HMT_AID_FORWARD8, ARRAY_SIZE(SEQ_HMT_AID_FORWARD8), 0},
};

static struct lcd_seq_info SEQ_HMT_REVERSE_SET[] = {
	{(u8 *)SEQ_HMT_AID_REVERSE1, ARRAY_SIZE(SEQ_HMT_AID_REVERSE1), 0},
	{(u8 *)SEQ_HMT_AID_REVERSE2, ARRAY_SIZE(SEQ_HMT_AID_REVERSE2), 0},
	{(u8 *)SEQ_HMT_AID_REVERSE3, ARRAY_SIZE(SEQ_HMT_AID_REVERSE3), 0},
	{(u8 *)SEQ_HMT_AID_REVERSE4, ARRAY_SIZE(SEQ_HMT_AID_REVERSE4), 0},
	{(u8 *)SEQ_HMT_AID_REVERSE5, ARRAY_SIZE(SEQ_HMT_AID_REVERSE5), 0},
	{(u8 *)SEQ_HMT_AID_REVERSE6, ARRAY_SIZE(SEQ_HMT_AID_REVERSE6), 0},
	{(u8 *)SEQ_HMT_AID_REVERSE7, ARRAY_SIZE(SEQ_HMT_AID_REVERSE7), 0},
	{(u8 *)SEQ_HMT_AID_REVERSE8, ARRAY_SIZE(SEQ_HMT_AID_REVERSE8), 0},
};

static struct lcd_seq_info SEQ_HMT_SINGLE_SET[] = {
	{(u8 *)SEQ_HMT_SINGLE1, ARRAY_SIZE(SEQ_HMT_SINGLE1), 0},
	{(u8 *)SEQ_HMT_SINGLE2, ARRAY_SIZE(SEQ_HMT_SINGLE2), 0},
	{(u8 *)SEQ_HMT_SINGLE3, ARRAY_SIZE(SEQ_HMT_SINGLE3), 0},
	{(u8 *)SEQ_HMT_SINGLE4, ARRAY_SIZE(SEQ_HMT_SINGLE4), 0},
	{(u8 *)SEQ_HMT_SINGLE5, ARRAY_SIZE(SEQ_HMT_SINGLE5), 0},
	{(u8 *)SEQ_HMT_SINGLE6, ARRAY_SIZE(SEQ_HMT_SINGLE6), 0},
	{(u8 *)SEQ_HMT_SINGLE7, ARRAY_SIZE(SEQ_HMT_SINGLE7), 0},
};

static struct lcd_seq_info SEQ_HMT_OFF_SET[] = {
	{(u8 *)SEQ_HMT_OFF1, ARRAY_SIZE(SEQ_HMT_OFF1), 0},
	{(u8 *)SEQ_HMT_OFF2, ARRAY_SIZE(SEQ_HMT_OFF2), 0},
	{(u8 *)SEQ_HMT_OFF3, ARRAY_SIZE(SEQ_HMT_OFF3), 0},
	{(u8 *)SEQ_HMT_OFF4, ARRAY_SIZE(SEQ_HMT_OFF4), 0},
};

static const unsigned char HMT_ELVSS_TABLE[ACL_STATUS_MAX][3] = {
	{0xB6, 0x8C, 0x0A},
	{0xB6, 0x9C, 0x0A}
};

#endif

static const unsigned char SEQ_MCD_ON1[] = {
	0xB0, 0x03
};

static const unsigned char SEQ_MCD_ON2[] = {
	0xBB, 0x30
};

static const unsigned char SEQ_MCD_ON3[] = {
	0xB0, 0x37
};

static const unsigned char SEQ_MCD_ON4[] = {
	0xCB, 0x04
};

static const unsigned char SEQ_MCD_ON5[] = {
	0xB0, 0x27
};

static const unsigned char SEQ_MCD_ON6[] = {
	0xCB, 0x44
};

static const unsigned char SEQ_MCD_ON7[] = {
	0xB0, 0x38
};

static const unsigned char SEQ_MCD_ON8[] = {
	0xCB, 0x05
};

static const unsigned char SEQ_MCD_ON9[] = {
	0xB0, 0x28
};

static const unsigned char SEQ_MCD_ON10[] = {
	0xCB, 0x45
};

static const unsigned char SEQ_MCD_ON11[] = {
	0xB0, 0x39
};

static const unsigned char SEQ_MCD_ON12[] = {
	0xCB, 0x12
};

static const unsigned char SEQ_MCD_ON13[] = {
	0xB0, 0x29
};

static const unsigned char SEQ_MCD_ON14[] = {
	0xCB, 0xD2
};

static const unsigned char SEQ_MCD_ON15[] = {
	0xB0, 0x11
};

static const unsigned char SEQ_MCD_ON16[] = {
	0xCB, 0x00
};

static const unsigned char SEQ_MCD_ON17[] = {
	0xB0, 0x12
};

static const unsigned char SEQ_MCD_ON18[] = {
	0xCB, 0x19
};

static const unsigned char SEQ_MCD_ON19[] = {
	0xB0, 0x13
};

static const unsigned char SEQ_MCD_ON20[] = {
	0xCB, 0xBF
};

static const unsigned char SEQ_MCD_ON21[] = {
	0xB0, 0x21
};

static const unsigned char SEQ_MCD_ON22[] = {
	0xCB, 0x3D
};

static const unsigned char SEQ_MCD_ON23[] = {
	0xB0, 0x22
};

static const unsigned char SEQ_MCD_ON24[] = {
	0xCB, 0xA2
};

static const unsigned char SEQ_MCD_ON25[] = {
	0xB0, 0x04
};

static const unsigned char SEQ_MCD_ON26[] = {
	0xCB, 0x80
};

static const unsigned char SEQ_MCD_ON27[] = {
	0xB0, 0x14
};

static const unsigned char SEQ_MCD_ON28[] = {
	0xCB, 0x19
};

static const unsigned char SEQ_MCD_ON29[] = {
	0xB0, 0x05
};

static const unsigned char SEQ_MCD_ON30[] = {
	0xF6, 0x3A
};

static struct lcd_seq_info SEQ_MCD_ON_SET[] = {
	{(u8 *)SEQ_MCD_ON1, ARRAY_SIZE(SEQ_MCD_ON1), 0},
	{(u8 *)SEQ_MCD_ON2, ARRAY_SIZE(SEQ_MCD_ON2), 0},
	{(u8 *)SEQ_MCD_ON3, ARRAY_SIZE(SEQ_MCD_ON3), 0},
	{(u8 *)SEQ_MCD_ON4, ARRAY_SIZE(SEQ_MCD_ON4), 0},
	{(u8 *)SEQ_MCD_ON5, ARRAY_SIZE(SEQ_MCD_ON5), 0},
	{(u8 *)SEQ_MCD_ON6, ARRAY_SIZE(SEQ_MCD_ON6), 0},
	{(u8 *)SEQ_MCD_ON7, ARRAY_SIZE(SEQ_MCD_ON7), 0},
	{(u8 *)SEQ_MCD_ON8, ARRAY_SIZE(SEQ_MCD_ON8), 0},
	{(u8 *)SEQ_MCD_ON9, ARRAY_SIZE(SEQ_MCD_ON9), 0},
	{(u8 *)SEQ_MCD_ON10, ARRAY_SIZE(SEQ_MCD_ON10), 0},
	{(u8 *)SEQ_MCD_ON11, ARRAY_SIZE(SEQ_MCD_ON11), 0},
	{(u8 *)SEQ_MCD_ON12, ARRAY_SIZE(SEQ_MCD_ON12), 0},
	{(u8 *)SEQ_MCD_ON13, ARRAY_SIZE(SEQ_MCD_ON13), 0},
	{(u8 *)SEQ_MCD_ON14, ARRAY_SIZE(SEQ_MCD_ON14), 0},
	{(u8 *)SEQ_MCD_ON15, ARRAY_SIZE(SEQ_MCD_ON15), 0},
	{(u8 *)SEQ_MCD_ON16, ARRAY_SIZE(SEQ_MCD_ON16), 0},
	{(u8 *)SEQ_MCD_ON17, ARRAY_SIZE(SEQ_MCD_ON17), 0},
	{(u8 *)SEQ_MCD_ON18, ARRAY_SIZE(SEQ_MCD_ON18), 0},
	{(u8 *)SEQ_MCD_ON19, ARRAY_SIZE(SEQ_MCD_ON19), 0},
	{(u8 *)SEQ_MCD_ON20, ARRAY_SIZE(SEQ_MCD_ON20), 0},
	{(u8 *)SEQ_MCD_ON21, ARRAY_SIZE(SEQ_MCD_ON21), 0},
	{(u8 *)SEQ_MCD_ON22, ARRAY_SIZE(SEQ_MCD_ON22), 0},
	{(u8 *)SEQ_MCD_ON23, ARRAY_SIZE(SEQ_MCD_ON23), 0},
	{(u8 *)SEQ_MCD_ON24, ARRAY_SIZE(SEQ_MCD_ON24), 0},
	{(u8 *)SEQ_MCD_ON25, ARRAY_SIZE(SEQ_MCD_ON25), 0},
	{(u8 *)SEQ_MCD_ON26, ARRAY_SIZE(SEQ_MCD_ON26), 0},
	{(u8 *)SEQ_MCD_ON27, ARRAY_SIZE(SEQ_MCD_ON27), 0},
	{(u8 *)SEQ_MCD_ON28, ARRAY_SIZE(SEQ_MCD_ON28), 0},
	{(u8 *)SEQ_MCD_ON29, ARRAY_SIZE(SEQ_MCD_ON29), 0},
	{(u8 *)SEQ_MCD_ON30, ARRAY_SIZE(SEQ_MCD_ON30), 0},
};

#endif /* __S6E3HF2_PARAM_H__ */
