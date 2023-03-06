#ifndef __S6E3HA0_PARAM_H__
#define __S6E3HA0_PARAM_H__

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

static const unsigned char SEQ_GAMMA_CONTROL_SET[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x00, 0x00, 0x00,
};

static const unsigned char SEQ_AOR_CONTROL[] = {
	0xB2,
	0x00, 0x0A
};

static const unsigned char SEQ_ELVSS_SET[] = {
	0xB6,
	0x98,				/* ACL OFF, T > 0 degrees Celsius */
	0x04, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x03, 0x55, 0x54, 0x20,
	0x00, 0x0A, 0xAA, 0xAF, 0x0F,	/* Temp offset */
	0x01, 0x11, 0x11, 0x10,		/* CAPS offset */
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
static const unsigned char SEQ_MIPI_SINGLE_DSI_SET1[] = {
	0xF2,
	0x07
};

static const unsigned char SEQ_MIPI_SINGLE_DSI_SET2[] = {
	0xF9,
	0x09,
};
#else
static const unsigned char SEQ_MIPI_SINGLE_DSI_SET1[] = {
	0xF2,
	0x07,
};

static const unsigned char SEQ_MIPI_SINGLE_DSI_SET2[] = {
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
	0x41
};

static const unsigned char SEQ_ACL_ON_OPR_AVR[] = {
	0xB5,
	0x51
};

static const unsigned char SEQ_TOUCH_HSYNC_ON[] = {
	0xBD,
	0x85, 0x02, 0x16
};

static const unsigned char SEQ_TOUCH_VSYNC_ON[] = {
	0xFF,
	0x02
};

static const unsigned char SEQ_PENTILE_CONTROL[] = {
	0xC0,
	0x30, 0x00, 0xD8, 0xD8
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

static const unsigned char SEQ_PCD_SET_DET_LOW[] = {
	0xCC,
	0x5C, 0x51,
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
	77, 82, 87, 93, 98, 105, 111, 119, 126, 134,
	143, 152, 162, 172, 183, 195, 207, 220, 234, 249,
	265, 282, 300, 316, 333, 350, 500
};

static const unsigned char ELVSS_TABLE[ACL_STATUS_MAX][ELVSS_STATUS_MAX] = {
	{
		0x0F, 0x0F, 0x0E, 0x0E, 0x0E, 0x0D, 0x0D, 0x0C, 0x0C, 0x0B,
		0x0B, 0x0A, 0x09, 0x09, 0x09, 0x09, 0X09, 0x09, 0x09, 0x09,
		0x08, 0x07, 0x07, 0x06, 0x05, 0x04, 0x04
	}, {
		0x0F, 0x0F, 0x0E, 0x0E, 0x0E, 0x0D, 0x0D, 0x0C, 0x0C, 0x0B,
		0x0B, 0x0A, 0x09, 0x09, 0x09, 0x09, 0X09, 0x09, 0x09, 0x09,
		0x08, 0x07, 0x07, 0x06, 0x05, 0x04, 0x04
	}
};

enum {
	TEMP_ABOVE_MINUS_20_DEGREE,	/* T > -20 */
	TEMP_BELOW_MINUS_20_DEGREE,	/* T <= -20 */
	TEMP_MAX,
};

static const unsigned char MPS_TABLE[ACL_STATUS_MAX][TEMP_MAX] = {
	{0x98, 0x9C},
	{0x88, 0x8C}
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
static const unsigned char SEQ_HMT_DUAL1[] = {		/* G.Param */
	0xB0, 0x01
};

static const unsigned char SEQ_HMT_DUAL2[] = {		/* Display Line : 2560 -> 1264, porch : 16 -> 32 */
	0xF2, 0x00, 0x01, 0xA4, 0x05, 0x1B, 0x4F
};

static const unsigned char SEQ_HMT_DUAL3[] = {		/* GRAM Window Vertical Set : 2559(9FF) -> 1279(4FF) */
	0x2B, 0x00, 0x00, 0x04, 0xFF
};

static const unsigned char SEQ_HMT_DUAL4[] = {		/* G.Param */
	0xB0, 0x01
};

static const unsigned char SEQ_HMT_DUAL5[] = {		/* Display Line Setting Enable */
	0xF2, 0x04
};

static const unsigned char SEQ_HMT_SINGLE1[] = {	/* G.Param */
	0xB0, 0x01
};

static const unsigned char SEQ_HMT_SINGLE2[] = {	/* Display Line : 1264 -> 2560, porch : 32 -> 16 */
	0xF2, 0x00, 0x01, 0xA4, 0x05, 0x0B, 0xA0
};

static const unsigned char SEQ_HMT_SINGLE3[] = {	/* GRAM Window Vertical Set : 1279(4FF) -> 2559(9FF) */
	0x2B, 0x00, 0x00, 0x09, 0xFF
};

static const unsigned char SEQ_HMT_SINGLE4[] = {	/* G.Param */
	0xB0, 0x01
};

static const unsigned char SEQ_HMT_SINGLE5[] = {	/* Display Line Setting Enable */
	0xF2, 0x04
};

static struct lcd_seq_info SEQ_HMT_DUAL_SET[] = {
	{(u8 *)SEQ_HMT_DUAL1, ARRAY_SIZE(SEQ_HMT_DUAL1), 0},
	{(u8 *)SEQ_HMT_DUAL2, ARRAY_SIZE(SEQ_HMT_DUAL2), 0},
	{(u8 *)SEQ_HMT_DUAL3, ARRAY_SIZE(SEQ_HMT_DUAL3), 0},
	{(u8 *)SEQ_HMT_DUAL4, ARRAY_SIZE(SEQ_HMT_DUAL4), 0},
	{(u8 *)SEQ_HMT_DUAL5, ARRAY_SIZE(SEQ_HMT_DUAL5), 0},
};

static struct lcd_seq_info SEQ_HMT_SINGLE_SET[] = {
	{(u8 *)SEQ_HMT_SINGLE1, ARRAY_SIZE(SEQ_HMT_SINGLE1), 0},
	{(u8 *)SEQ_HMT_SINGLE2, ARRAY_SIZE(SEQ_HMT_SINGLE2), 0},
	{(u8 *)SEQ_HMT_SINGLE3, ARRAY_SIZE(SEQ_HMT_SINGLE3), 0},
	{(u8 *)SEQ_HMT_SINGLE4, ARRAY_SIZE(SEQ_HMT_SINGLE4), 0},
	{(u8 *)SEQ_HMT_SINGLE5, ARRAY_SIZE(SEQ_HMT_SINGLE5), 0},
};
#endif
#endif /* __S6E3HA0_PARAM_H__ */
