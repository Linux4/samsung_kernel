#ifndef __S6E3HA2_PARAM_H__
#define __S6E3HA2_PARAM_H__

#define GAMMA_PARAM_SIZE	ARRAY_SIZE(SEQ_GAMMA_CONTROL_SET)
#define ACL_PARAM_SIZE	ARRAY_SIZE(SEQ_ACL_OFF)
#define OPR_PARAM_SIZE	ARRAY_SIZE(SEQ_ACL_OFF_OPR_AVR)
#define ELVSS_PARAM_SIZE	ARRAY_SIZE(SEQ_ELVSS_SET)
#define AID_PARAM_SIZE	ARRAY_SIZE(SEQ_AOR_CONTROL)
#define HBM_PARAM_SIZE	ARRAY_SIZE(SEQ_HBM_OFF)
#define VINT_PARAM_SIZE	ARRAY_SIZE(SEQ_VINT_SETTING)

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

static const unsigned char SEQ_TOUCH_HSYNC_ON_ID2_00[] = {
	0xBD,
	0x33, 0x11, 0x02, 0x16, 0x02, 0x16
};

static const unsigned char SEQ_TOUCH_HSYNC_ON[] = {
	0xBD,
	0x11, 0x11, 0x02, 0x16, 0x02, 0x16
};

static const unsigned char SEQ_PENTILE_CONTROL[] = {
	0xC0,
	0x00, 0x00, 0xD8, 0xD8
};

static const unsigned char SEQ_POC_GLOBAL[] = {
	0xB0,
	0x20
};
static const unsigned char SEQ_POC_SETTING[] = {
	0xFE,
	0x08
};

static const unsigned char SEQ_LTPS_TIMING[] = {
	0xCB,
	0x18, 0x11, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0xE1, 0x09,
	0x01, 0x00, 0x00, 0x00, 0x02, 0x05, 0x00, 0x15, 0x98, 0x15,
	0x98, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x12, 0x8C
};

static unsigned char SEQ_PARTIAL_AREA[] = {
	0x30,
	0x00, 0x00, 0x00, 0x00
};

static const unsigned char SEQ_NORMAL_MODE_ON[] = {
	0x13,
};

static const unsigned char SEQ_PARTIAL_MODE_ON[] = {
	0x12,
};

#if defined(CONFIG_LCD_PCD)
static const unsigned char SEQ_PCD_SET_DET_LOW[] = {
	0xCC,
	0x5C
};
#endif

static const unsigned char SEQ_PCK_DELAY_SETTING[] = {
	0xFF,
	0x00, 0x00, 0x00, 0x08
};

static const unsigned char SEQ_VINT_SETTING[] = {
	0xF4,
	0x8B, 0x21
};

static const unsigned char SEQ_ERR_FG_SETTING[] = {
	0xED,
	0x44
};

static const unsigned char SEQ_TE_START_SETTING[] = {
	0xB9,
	0x10, 0x09, 0xFF, 0x00, 0x09
};

static const unsigned char SEQ_FREQ_CALIBRATION_SETTING1[] = {
	0xFD,
	0x1C
};

static const unsigned char SEQ_FREQ_CALIBRATION_SETTING2[] = {
	0xFE,
	0x20, 0x39
};

static const unsigned char SEQ_FREQ_CALIBRATION_SETTING3[] = {
	0xFE,
	0xA0
};

static const unsigned char SEQ_FREQ_CALIBRATION_SETTING4[] = {
	0xFE,
	0x20
};

static const unsigned char SEQ_FREQ_CALIBRATION_SETTING5[] = {
	0xCE,
	0x03, 0x3B, 0x12, 0x62,
	0x40, 0x80, 0xC0, 0x28, 0x28, 0x28, 0x28, 0x39,
	0xC5
};

static struct lcd_seq_info SEQ_LDI_FPS_SET[] = {
	{(u8 *)SEQ_FREQ_CALIBRATION_SETTING1, ARRAY_SIZE(SEQ_FREQ_CALIBRATION_SETTING1), 0},
	{(u8 *)SEQ_FREQ_CALIBRATION_SETTING2, ARRAY_SIZE(SEQ_FREQ_CALIBRATION_SETTING2), 0},
	{(u8 *)SEQ_FREQ_CALIBRATION_SETTING3, ARRAY_SIZE(SEQ_FREQ_CALIBRATION_SETTING3), 0},
	{(u8 *)SEQ_FREQ_CALIBRATION_SETTING4, ARRAY_SIZE(SEQ_FREQ_CALIBRATION_SETTING4), 0},
	{(u8 *)SEQ_FREQ_CALIBRATION_SETTING5, ARRAY_SIZE(SEQ_FREQ_CALIBRATION_SETTING5), 0},
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
	ELVSS_STATUS_360,
	ELVSS_STATUS_HBM,
	ELVSS_STATUS_MAX
};

static const unsigned int ELVSS_DIM_TABLE[ELVSS_STATUS_MAX] = {
	77, 82, 87, 93, 98, 105, 111, 119, 126, 134,
	143, 152, 162, 172, 183, 195, 207, 220, 234, 249,
	265, 282, 300, 316, 333, 360, 550
};

static const unsigned char ELVSS_TABLE[ELVSS_STATUS_MAX] = {
	0x16, 0x16, 0x15, 0x15, 0x15, 0x15, 0x14, 0x14, 0x14, 0x13,
	0x13, 0x13, 0x12, 0x12, 0x0F, 0x0F, 0x0E, 0x0E, 0x0E, 0x0D,
	0x0D, 0x0C, 0x0C, 0x0B, 0x0B, 0x0A, 0x0A
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
	0x18, 0x19, 0x1A, 0x1B, 0x1C,
	0x1D, 0x1E, 0x1F, 0x20, 0x21
};

#ifdef CONFIG_LCD_ALPM
/* ALPM Commands */
static const unsigned char SEQ_ALPM_30HZ_LTPS_SET[] = {
	0xCB,
	0x18, 0x41, 0x01, 0x00, 0x00, 0x01, 0x23, 0x00, 0x00, 0xD2,
	0x04, 0x00, 0xD2, 0x03, 0x00, 0x00, 0x00, 0x8F, 0x14, 0x8F,
	0x00, 0x00, 0x21, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xD2, 0x03, 0x00, 0x21, 0x82, 0x00, 0x00, 0xC0, 0x00,
	0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0xE0, 0xE0, 0xED, 0xE0,
	0x63, 0x02, 0x04, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x0F, 0x0E, 0x04, 0x05,
	0x00, 0xD3, 0x03, 0x01, 0xD3, 0x72, 0x48, 0x72, 0x48, 0x72,
	0x48, 0x03, 0x03, 0x03
};

static const unsigned char SEQ_NORMAL_LTPS_SET[] = {
	0xCB,
	0x18, 0x41, 0x01, 0x00, 0x00, 0x01, 0x23, 0x00, 0x00, 0xD2,
	0x04, 0x00, 0xD2, 0x03, 0x00, 0x00, 0x00, 0x8F, 0x14, 0x8F,
	0x00, 0x00, 0x21, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xD2, 0x03, 0x00, 0x21, 0x82, 0x00, 0x00, 0xC0, 0x00,
	0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0xE0, 0xE0, 0xE1, 0xE0,
	0x63, 0x02, 0x04, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x0F, 0x0E, 0x04, 0x05,
	0x00, 0xD3, 0x03, 0x01, 0xD3, 0x72, 0x48, 0x72, 0x48, 0x72,
	0x48, 0x03, 0x03, 0x03
};

static const unsigned char SEQ_ALPM_30HZ_FRAME_FREQ[] = {
	0xBB,
	0x00, 0x00
};

static const unsigned char SEQ_IDLE_MODE_ON[] = {
	0x39,
};

static const unsigned char SEQ_IDLE_MODE_OFF[] = {
	0x38,
};

static struct lcd_seq_info SEQ_ALPM_ON_SET[] = {
	{(u8 *)SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF), 20},
	{(u8 *)SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0), 0},
	{(u8 *)SEQ_ALPM_30HZ_LTPS_SET, ARRAY_SIZE(SEQ_ALPM_30HZ_LTPS_SET), 0},
	{(u8 *)SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE), 0},
	{(u8 *)SEQ_ALPM_30HZ_FRAME_FREQ, ARRAY_SIZE(SEQ_ALPM_30HZ_FRAME_FREQ), 0},
	{(u8 *)SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0), 0},
	{(u8 *)SEQ_PARTIAL_AREA, ARRAY_SIZE(SEQ_PARTIAL_AREA), 0},
	{(u8 *)SEQ_PARTIAL_MODE_ON, ARRAY_SIZE(SEQ_PARTIAL_MODE_ON), 0},
	{(u8 *)SEQ_IDLE_MODE_ON, ARRAY_SIZE(SEQ_IDLE_MODE_ON), 20},
	{(u8 *)SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON), 0},
};

static struct lcd_seq_info SEQ_ALPM_OFF_SET[] = {
	{(u8 *)SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF), 20},
	{(u8 *)SEQ_IDLE_MODE_OFF, ARRAY_SIZE(SEQ_IDLE_MODE_OFF), 0},
	{(u8 *)SEQ_NORMAL_MODE_ON, ARRAY_SIZE(SEQ_NORMAL_MODE_ON), 0},
	{(u8 *)SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0), 0},
	{(u8 *)SEQ_NORMAL_LTPS_SET, ARRAY_SIZE(SEQ_NORMAL_LTPS_SET), 0},
	{(u8 *)SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE), 0},
	{(u8 *)SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0), 0},
	{(u8 *)SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON), 0},
};
#endif

#ifdef CONFIG_LCD_HMT

static const unsigned char SEQ_HMT_SINGLE1[] = { /* Display Setting. Porch Setting*/
	0xF2,
	0x67, 0x41, 0xC5, 0x0A, 0x06
};

static const unsigned char SEQ_HMT_SINGLE2[] = {	/* AID Setting AID 1-Cycle */
	0xB2,
	0x03, 0x10, 0x00, 0x0A, 0x0A, 0x00
};

static const unsigned char SEQ_HMT_SINGLE3[] = {		/* Display Setting. Porch Data Setting*/
	0xF3,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
};

static const unsigned char SEQ_HMT_SINGLE4[] = {		/* PASET Setting  */
	0x2B,
	0x00, 0x00, 0x09, 0xFF
};


static const unsigned char SEQ_HMT_OFF1[] = {	/* Display setting. porch setting */
	0xF2,
	0x67, 0x41, 0xC5, 0x06, 0x0A
};
static const unsigned char SEQ_HMT_OFF2[] = {	/* AID Setting AID 4-Cycle */
	0xB2,
	0x03, 0x10, 0x00, 0x0A, 0x0A, 0x40
};


static const unsigned char SEQ_HMT_AID_FORWARD1[] = {	/* LTPS */
	0xCB,
	0x18,0x11,0x01,0x00,0x00,0x24,0x00,0x00,0xe1,0x09,
	0x01,0x00,0x00,0x00,0x02,0x05,0x00,0x15,0x98,0x15,
	0x98,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x12,0x8c,0x00,0x00,0xca,0x0a,0x0a,0x0a,0xca,
	0x8a,0xca,0x0a,0x0a,0x0a,0xc0,0xc1,0xc4,0xc5,0x42,
	0xc3,0xca,0xca,0x0a,0x0a,0x0a,0x0a,0x0a,0x0a,0x0a,
	0x0a,0x0b,
};


static const unsigned char SEQ_HMT_AID_REVERSE1[] = {	/* LTPS */
	0xCB,
	0x18,0x11,0x01,0x00,0x00,0x24,0x00,0x00,0xe1,0x0B,
	0x01,0x00,0x00,0x00,0x02,0x09,0x00,0x15,0x98,0x15,
	0x98,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,
	0x00,0x12,0x8c,0x00,0x00,0xca,0x0a,0x0a,0x0a,0xca,
	0x8a,0xca,0x0a,0x0a,0x0a,0xc0,0xc1,0xc4,0xc5,0x42,
	0xc3,0xca,0xca,0x0a,0x0a,0x0a,0x0a,0x0a,0x0a,0x0a,
	0x0a,0x0c,
};

static struct lcd_seq_info SEQ_HMT_AID_FORWARD_SET[] = {
	{(u8 *)SEQ_HMT_AID_FORWARD1, ARRAY_SIZE(SEQ_HMT_AID_FORWARD1), 0},
};

static struct lcd_seq_info SEQ_HMT_REVERSE_SET[] = {
	{(u8 *)SEQ_HMT_AID_REVERSE1, ARRAY_SIZE(SEQ_HMT_AID_REVERSE1), 0},
};

static struct lcd_seq_info SEQ_HMT_SINGLE_SET[] = {
	{(u8 *)SEQ_HMT_SINGLE1, ARRAY_SIZE(SEQ_HMT_SINGLE1), 0},
	{(u8 *)SEQ_HMT_SINGLE2, ARRAY_SIZE(SEQ_HMT_SINGLE2), 0},
	{(u8 *)SEQ_HMT_SINGLE3, ARRAY_SIZE(SEQ_HMT_SINGLE3), 0},
	{(u8 *)SEQ_HMT_SINGLE4, ARRAY_SIZE(SEQ_HMT_SINGLE4), 0},
};

static struct lcd_seq_info SEQ_HMT_OFF_SET[] = {
	{(u8 *)SEQ_HMT_OFF1, ARRAY_SIZE(SEQ_HMT_OFF1), 0},
	{(u8 *)SEQ_HMT_OFF2, ARRAY_SIZE(SEQ_HMT_OFF2), 0},
};

static const unsigned char HMT_ELVSS_TABLE[ACL_STATUS_MAX][3] = {
	{0xB6, 0x8C, 0x0A},
	{0xB6, 0x9C, 0x0A}
};

#endif
#endif /* __S6E3HA2_PARAM_H__ */
