#ifndef __EA8076_PARAM_H__
#define __EA8076_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>

#define LCD_TYPE_VENDOR		"SDC"

#define EXTEND_BRIGHTNESS	365
#define UI_MAX_BRIGHTNESS	255
#define UI_DEFAULT_BRIGHTNESS	128

#define NORMAL_TEMPERATURE	25	/* 25 degrees Celsius */

#define ACL_CMD_CNT				((u16)ARRAY_SIZE(SEQ_ACL_OFF))
#define ACL_DIM_CMD_CNT				((u16)ARRAY_SIZE(SEQ_ACL_DIM_OFF))
#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
#define FFC_CMD_CNT				((u16)ARRAY_SIZE(SEQ_FFC_SET_1177))
#endif
#define HBM_CMD_CNT				((u16)ARRAY_SIZE(SEQ_HBM_OFF))
#define ELVSS_CMD_CNT				((u16)ARRAY_SIZE(SEQ_ELVSS_SET))
#define ACL_DIM_FRAME_OFFSET		7

#define LDI_REG_BRIGHTNESS			0x51
#define LDI_REG_ID				0x04
#define LDI_REG_COORDINATE			0xEA
#define LDI_REG_DATE				LDI_REG_COORDINATE
#define LDI_REG_MANUFACTURE_INFO		LDI_REG_COORDINATE
#define LDI_REG_MANUFACTURE_INFO_CELL_ID	0xEF
#define LDI_REG_CHIP_ID				0xD1
#define LDI_REG_ELVSS				0xB7
#define LDI_REG_MCA_CHECK			0xC4

/* len is read length */
#define LDI_LEN_ID				3
#define LDI_LEN_COORDINATE			4
#define LDI_LEN_DATE				7
#define LDI_LEN_MANUFACTURE_INFO		4
#define LDI_LEN_MANUFACTURE_INFO_CELL_ID	16
#define LDI_LEN_CHIP_ID				6
#define LDI_LEN_ELVSS				(ELVSS_CMD_CNT - 1)
#define LDI_LEN_MCA_CHECK			33

/* offset is position including addr, not only para */
#define LDI_OFFSET_ACL		1			/* 55h 1st para */
#define LDI_OFFSET_HBM		1			/* 53h 1st para */
#define LDI_OFFSET_ELVSS_1	6			/* B7h 6th para */
#define LDI_OFFSET_ELVSS_2	46			/* B7h 46th para */

#define LDI_GPARA_COORDINATE			3	/* EAh 4th Para: x, y */
#define LDI_GPARA_DATE				7	/* EAh 8th Para: [D7:D4]: Year */
#define LDI_GPARA_MANUFACTURE_INFO		15	/* EAh 16th Para ~ 19th Para */
#define LDI_GPARA_MANUFACTURE_INFO_CELL_ID	2	/* EFh 3rd Para ~ 18th Para */
#define LDI_GPARA_CHIP_ID			55	/* D1h 56th Para */
#define LDI_GPARA_ELVSS_NORMAL			7	/* B7h 8th Para */
#define LDI_GPARA_ELVSS_HBM			8	/* B7h 9th Para */

struct bit_info {
	unsigned int reg;
	unsigned int len;
	char **print;
	unsigned int expect;
	unsigned int offset;
	unsigned int g_para;
	unsigned int invert;
	unsigned int mask;
	unsigned int result;
};

enum {
	LDI_BIT_ENUM_05,	LDI_BIT_ENUM_RDNUMED = LDI_BIT_ENUM_05,
	LDI_BIT_ENUM_0A,	LDI_BIT_ENUM_RDDPM = LDI_BIT_ENUM_0A,
	LDI_BIT_ENUM_0E,	LDI_BIT_ENUM_RDDSM = LDI_BIT_ENUM_0E,
	LDI_BIT_ENUM_0F,	LDI_BIT_ENUM_RDDSDR = LDI_BIT_ENUM_0F,
	LDI_BIT_ENUM_MAX
};

static char *LDI_BIT_DESC_05[BITS_PER_BYTE] = {
	[0 ... 6] = "number of corrupted packets",
	[7] = "overflow on number of corrupted packets",
};

static char *LDI_BIT_DESC_0A[BITS_PER_BYTE] = {
	[2] = "Display is Off",
	[7] = "Booster has a fault",
};

static char *LDI_BIT_DESC_0E[BITS_PER_BYTE] = {
	[0] = "Error on DSI",
};

static char *LDI_BIT_DESC_0F[BITS_PER_BYTE] = {
	[7] = "Register Loading Detection",
};

static struct bit_info ldi_bit_info_list[LDI_BIT_ENUM_MAX] = {
	[LDI_BIT_ENUM_05] = {0x05, 1, LDI_BIT_DESC_05, 0x00, },
	[LDI_BIT_ENUM_0A] = {0x0A, 1, LDI_BIT_DESC_0A, 0x9C, .invert = (BIT(2) | BIT(7)), },
	[LDI_BIT_ENUM_0E] = {0x0E, 1, LDI_BIT_DESC_0E, 0x80, },
	[LDI_BIT_ENUM_0F] = {0x0F, 1, LDI_BIT_DESC_0F, 0xC0, .invert = (BIT(7)), },
};

#if defined(CONFIG_SMCDSD_DPUI)
#define LDI_LEN_RDNUMED		1		/* DPUI_KEY_PNDSIE: Read Number of the Errors on DSI */
#define LDI_PNDSIE_MASK		(GENMASK(7, 0))

#define LDI_LEN_RDDSDR		1		/* DPUI_KEY_PNSDRE: Read Display Self-Diagnostic Result */
#define LDI_PNSDRE_MASK		(BIT(7))	/* D7: REG_DET: Register Loading Detection */
#endif

struct lcd_seq_info {
	unsigned char	*cmd;
	unsigned int	len;
	unsigned int	sleep;
};

static unsigned char SEQ_SLEEP_OUT[] = {
	0x11
};

static unsigned char SEQ_SLEEP_IN[] = {
	0x10
};

static unsigned char SEQ_DISPLAY_ON[] = {
	0x29
};

static unsigned char SEQ_DISPLAY_OFF[] = {
	0x28
};

static unsigned char SEQ_TEST_KEY_ON_F0[] = {
	0xF0,
	0x5A, 0x5A
};

static unsigned char SEQ_TEST_KEY_OFF_F0[] = {
	0xF0,
	0xA5, 0xA5
};

static unsigned char SEQ_TEST_KEY_ON_FC[] = {
	0xFC,
	0x5A, 0x5A
};

static unsigned char SEQ_TEST_KEY_OFF_FC[] = {
	0xFC,
	0xA5, 0xA5
};

static unsigned char SEQ_TE_ON[] = {
	0x35,
	0x00
};

static unsigned char SEQ_PAGE_ADDR_SET_2A[] = {
	0x2A,
	0x00, 0x00, 0x04, 0x37
};

static unsigned char SEQ_PAGE_ADDR_SET_2B[] = {
	0x2B,
	0x00, 0x00, 0x09, 0x5F
};

#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
static unsigned char SEQ_FFC_SET_1177[] = {
	0xE9,
	0x11, 0x65, 0x9B, 0x91, 0xD4, 0xA9, 0x12, 0x8C,
	0x00, 0x1A, 0xB8
};

static unsigned char SEQ_FFC_SET_1196[] = {
	0xE9,
	0x11, 0x65, 0x99, 0x19, 0x25, 0xA9, 0x12, 0x8C,
	0x00, 0x1A, 0xB8
};

static unsigned char SEQ_FFC_SET_1150[] = {
	0xE9,
	0x11, 0x65, 0x9F, 0x38, 0xDF, 0xA9, 0x12, 0x8C,
	0x00, 0x1A, 0xB8
};

static unsigned char SEQ_FFC_SET_1157[] = {
	0xE9,
	0x11, 0x65, 0x9E, 0x42, 0x43, 0xA9, 0x12, 0x8C,
	0x00, 0x1A, 0xB8
};

static unsigned char SEQ_FFC_SET_OFF[] = {
	0xE9,
	0x10,
};
#else
static unsigned char SEQ_FFC_SET[] = {
	0xE9,
	0x11, 0x65, 0x98, 0x96, 0x80, 0xA9, 0xEB, 0x71,
	0x00, 0x1A, 0xB8	/* 1.2 Gbps Setting ( 89.8 Mhz ) */
};
#endif

static unsigned char SEQ_ERR_FG_SET[] = {
	0xE1,
	0x00, 0x00, 0x02,
	0x10, 0x10, 0x10,	/* DET_POL_VGH, ESD_FG_VGH, DET_EN_VGH */
	0x00, 0x00, 0x20, 0x00,
	0x00, 0x01, 0x19
};

static unsigned char SEQ_VSYNC_SET[] = {
	0xE0,
	0x01
};

static unsigned char SEQ_ASWIRE_OFF[] = {
	0xD5,
	0x80, 0xFF, 0x5A, 0xCC, 0xBF, 0x89, 0x11
};

static unsigned char SEQ_ASWIRE_OFF_FD_OFF[] = {
	0xD5,
	0x80, 0xFF, 0x5A, 0xCC, 0xBF, 0x89, 0x21
};

static unsigned char SEQ_PCD_SET_DET_LOW[] = {
	0xE3,
	0x57, 0x12	/* 1st 0x57: default high, 2nd 0x12: Disable SW RESET */
};

static unsigned char SEQ_HBM_ON[] = {
	0x53,
	0xE8,
};

static unsigned char SEQ_HBM_OFF[] = {
	0x53,
	0x28,
};

static unsigned char SEQ_HBM_ON_DIMMING_OFF[] = {
	0x53,
	0xE0,
};

static unsigned char SEQ_HBM_OFF_DIMMING_OFF[] = {
	0x53,
	0x20,
};

static unsigned char SEQ_ACL_AVG_OFFSET[] = {
	0xB0,
	0xCC,
};

static unsigned char SEQ_ACL_AVG[] = {
	0xB9,
	0x55, 0x27, 0x65		/* 32Frame Avg */
};

static unsigned char SEQ_ACL_DIM_OFFSET[] = {
	0xB0,
	0xD7,
};

static unsigned char SEQ_ACL_DIM_OFF[] = {
	0xB9,
	0x02, 0x61, 0x24, 0x49, 0x41, 0xFF, 0x00	/* 0 frame */
};

static unsigned char SEQ_ACL_DIM_ON[] = {
	0xB9,
	0x02, 0x61, 0x24, 0x49, 0x41, 0xFF, 0x20	/* 32 frame */
};


static unsigned char SEQ_ACL_OFF[] = {
	0x55,
	0x00
};

static unsigned char SEQ_ACL_08P[] = {
	0x55,
	0x01
};

static unsigned char SEQ_ACL_15P[] = {
	0x55,
	0x03
};

static unsigned char SEQ_ELVSS_SET[] = {
	0xB7,
	0x01, 0x53, 0x28, 0x4D, 0x00,
	0x8A,	/* 6th: ELVSS return */
	0x04,	/* 7th para : Smooth transition 4-frame */
	0x00,	/* 8th: Normal ELVSS OTP */
	0x00,	/* 9th: HBM ELVSS OTP */
	0x00, 0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x42,
	0x43, 0x43, 0x43, 0x43, 0x43, 0x83, 0xC3, 0x83,
	0xC3, 0x83, 0xC3, 0x03, 0x03, 0x03, 0x03, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00,	/* 46th: TSET */
};

static struct lcd_seq_info LCD_SEQ_INIT_1[] = {
	{SEQ_PAGE_ADDR_SET_2A, ARRAY_SIZE(SEQ_PAGE_ADDR_SET_2A) },
	{SEQ_PAGE_ADDR_SET_2B, ARRAY_SIZE(SEQ_PAGE_ADDR_SET_2B) },
#if !defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
	{SEQ_FFC_SET, ARRAY_SIZE(SEQ_FFC_SET) },
#endif
};

static struct lcd_seq_info LCD_SEQ_INIT_2[] = {
	{SEQ_ERR_FG_SET, ARRAY_SIZE(SEQ_ERR_FG_SET) },
	{SEQ_VSYNC_SET, ARRAY_SIZE(SEQ_VSYNC_SET) },
	{SEQ_PCD_SET_DET_LOW, ARRAY_SIZE(SEQ_PCD_SET_DET_LOW) },
	{SEQ_ACL_AVG_OFFSET, ARRAY_SIZE(SEQ_ACL_AVG_OFFSET) },
	{SEQ_ACL_AVG, ARRAY_SIZE(SEQ_ACL_AVG) },
};

#if defined(CONFIG_SMCDSD_DOZE)
enum {
	ALPM_OFF,
	ALPM_ON_LOW,	/* ALPM 2 NIT */
	HLPM_ON_LOW,	/* HLPM 2 NIT */
	ALPM_ON_HIGH,	/* ALPM 60 NIT */
	HLPM_ON_HIGH,	/* HLPM 60 NIT */
	ALPM_MODE_MAX
};

enum {
	AOD_MODE_OFF,
	AOD_MODE_ALPM,
	AOD_MODE_HLPM,
	AOD_MODE_MAX
};

enum {
	AOD_HLPM_OFF,
	AOD_HLPM_02_NIT,
	AOD_HLPM_10_NIT,
	AOD_HLPM_30_NIT,
	AOD_HLPM_60_NIT,
	AOD_HLPM_STATE_MAX
};

static const char *AOD_HLPM_STATE_NAME[AOD_HLPM_STATE_MAX] = {
	"HLPM_OFF",
	"HLPM_02_NIT",
	"HLPM_10_NIT",
	"HLPM_30_NIT",
	"HLPM_60_NIT",
};

static unsigned int lpm_old_table[ALPM_MODE_MAX] = {
	AOD_HLPM_OFF,
	AOD_HLPM_02_NIT,
	AOD_HLPM_02_NIT,
	AOD_HLPM_60_NIT,
	AOD_HLPM_60_NIT,
};

static unsigned int lpm_brightness_table[EXTEND_BRIGHTNESS + 1] = {
	[0 ... 39]			= AOD_HLPM_02_NIT,
	[40 ... 70]			= AOD_HLPM_10_NIT,
	[71 ... 93]			= AOD_HLPM_30_NIT,
	[94 ... EXTEND_BRIGHTNESS]	= AOD_HLPM_60_NIT,
};

static unsigned char SEQ_HLPM_SELECT_1[] = {
	0xB0,
	0xA3
};

static unsigned char SEQ_HLPM_SELECT_2[] = {
	0xC7,
	0x00
};

static unsigned char SEQ_HLPM_AOR_1[] = {
	0xB0,
	0x68
};

static unsigned char SEQ_HLPM_AOR_2_60[] = {
	0xB9,
	0x01, 0x89
};

static unsigned char SEQ_HLPM_AOR_2_30[] = {
	0xB9,
	0x57, 0x89
};

static unsigned char SEQ_HLPM_AOR_2_10[] = {
	0xB9,
	0x8A, 0x49
};

static unsigned char SEQ_HLPM_AOR_2_02[] = {
	0xB9,
	0x9D, 0xC9
};

static unsigned char SEQ_HLPM_SETTING_1[] = {
	0xB0,
	0x5C
};

static unsigned char SEQ_HLPM_SETTING_2[] = {
	0xC0,
	0x66, 0x29, 0x00, 0x00, 0x03, 0x3A, 0x40
};

static unsigned char SEQ_HLPM_UPDATE[] = {
	0xC0,
	0x01
};

static unsigned char SEQ_HLPM_ON[] = {
	0x53,
	0x22
};

static unsigned char SEQ_HLPM_OFF[] = {
	0x53,
	0x20
};

static struct lcd_seq_info LCD_SEQ_HLPM_60_NIT[] = {
	{SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0) },
	{SEQ_HLPM_SELECT_1, ARRAY_SIZE(SEQ_HLPM_SELECT_1) },
	{SEQ_HLPM_SELECT_2, ARRAY_SIZE(SEQ_HLPM_SELECT_2) },
	{SEQ_HLPM_AOR_1, ARRAY_SIZE(SEQ_HLPM_AOR_1) },
	{SEQ_HLPM_AOR_2_60, ARRAY_SIZE(SEQ_HLPM_AOR_2_60) },
	{SEQ_HLPM_SETTING_1, ARRAY_SIZE(SEQ_HLPM_SETTING_1) },
	{SEQ_HLPM_SETTING_2, ARRAY_SIZE(SEQ_HLPM_SETTING_2) },
	{SEQ_HLPM_UPDATE, ARRAY_SIZE(SEQ_HLPM_UPDATE) },
	{SEQ_HLPM_ON, ARRAY_SIZE(SEQ_HLPM_ON), 1},
	{SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0) },
};

static struct lcd_seq_info LCD_SEQ_HLPM_30_NIT[] = {
	{SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0) },
	{SEQ_HLPM_SELECT_1, ARRAY_SIZE(SEQ_HLPM_SELECT_1) },
	{SEQ_HLPM_SELECT_2, ARRAY_SIZE(SEQ_HLPM_SELECT_2) },
	{SEQ_HLPM_AOR_1, ARRAY_SIZE(SEQ_HLPM_AOR_1) },
	{SEQ_HLPM_AOR_2_30, ARRAY_SIZE(SEQ_HLPM_AOR_2_30) },
	{SEQ_HLPM_SETTING_1, ARRAY_SIZE(SEQ_HLPM_SETTING_1) },
	{SEQ_HLPM_SETTING_2, ARRAY_SIZE(SEQ_HLPM_SETTING_2) },
	{SEQ_HLPM_UPDATE, ARRAY_SIZE(SEQ_HLPM_UPDATE) },
	{SEQ_HLPM_ON, ARRAY_SIZE(SEQ_HLPM_ON), 1},
	{SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0) },
};

static struct lcd_seq_info LCD_SEQ_HLPM_10_NIT[] = {
	{SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0) },
	{SEQ_HLPM_SELECT_1, ARRAY_SIZE(SEQ_HLPM_SELECT_1) },
	{SEQ_HLPM_SELECT_2, ARRAY_SIZE(SEQ_HLPM_SELECT_2) },
	{SEQ_HLPM_AOR_1, ARRAY_SIZE(SEQ_HLPM_AOR_1) },
	{SEQ_HLPM_AOR_2_10, ARRAY_SIZE(SEQ_HLPM_AOR_2_10) },
	{SEQ_HLPM_SETTING_1, ARRAY_SIZE(SEQ_HLPM_SETTING_1) },
	{SEQ_HLPM_SETTING_2, ARRAY_SIZE(SEQ_HLPM_SETTING_2) },
	{SEQ_HLPM_UPDATE, ARRAY_SIZE(SEQ_HLPM_UPDATE) },
	{SEQ_HLPM_ON, ARRAY_SIZE(SEQ_HLPM_ON), 1},
	{SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0) },
};

static struct lcd_seq_info LCD_SEQ_HLPM_02_NIT[] = {
	{SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0) },
	{SEQ_HLPM_SELECT_1, ARRAY_SIZE(SEQ_HLPM_SELECT_1) },
	{SEQ_HLPM_SELECT_2, ARRAY_SIZE(SEQ_HLPM_SELECT_2) },
	{SEQ_HLPM_AOR_1, ARRAY_SIZE(SEQ_HLPM_AOR_1) },
	{SEQ_HLPM_AOR_2_02, ARRAY_SIZE(SEQ_HLPM_AOR_2_02) },
	{SEQ_HLPM_SETTING_1, ARRAY_SIZE(SEQ_HLPM_SETTING_1) },
	{SEQ_HLPM_SETTING_2, ARRAY_SIZE(SEQ_HLPM_SETTING_2) },
	{SEQ_HLPM_UPDATE, ARRAY_SIZE(SEQ_HLPM_UPDATE) },
	{SEQ_HLPM_ON, ARRAY_SIZE(SEQ_HLPM_ON), 1},
	{SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0) },
};

static struct lcd_seq_info LCD_SEQ_HLPM_OFF[] = {
	{SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0) },
	{SEQ_HLPM_OFF, ARRAY_SIZE(SEQ_HLPM_OFF), 1},
	{SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0) },
};
#endif

static unsigned char SEQ_XTALK_B0[] = {
	0xB0,
	0x1C
};

static unsigned char SEQ_XTALK_ON[] = {
	0xD9,
	0x60
};

static unsigned char SEQ_XTALK_OFF[] = {
	0xD9,
	0xC0
};

enum {
	ACL_STATUS_OFF,
	ACL_STATUS_08P,
	ACL_STATUS_15P,
	ACL_STATUS_MAX,
};

#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
enum {
	FFC_OFF,
	FFC_UPDATE,
};

enum {
	DYNAMIC_MIPI_INDEX_0,
	DYNAMIC_MIPI_INDEX_1,
	DYNAMIC_MIPI_INDEX_2,
	DYNAMIC_MIPI_INDEX_3,
	DYNAMIC_MIPI_INDEX_MAX,
};
#endif

enum {
	TEMP_ABOVE_MINUS_00_DEGREE,	/* T > 0 */
	TEMP_ABOVE_MINUS_15_DEGREE,	/* -15 < T <= 0 */
	TEMP_BELOW_MINUS_15_DEGREE,	/* T <= -15 */
	TEMP_MAX
};

enum {
	HBM_STATUS_OFF,
	HBM_STATUS_ON,
	HBM_STATUS_MAX
};

enum {
	TRANS_DIMMING_OFF,
	TRANS_DIMMING_ON,
	TRANS_DIMMING_MAX
};

enum {
	ACL_DIMMING_OFF,
	ACL_DIMMING_ON,
	ACL_DIMMING_MAX
};

static unsigned char *HBM_TABLE[TRANS_DIMMING_MAX][HBM_STATUS_MAX] = {
	{SEQ_HBM_OFF_DIMMING_OFF, SEQ_HBM_ON_DIMMING_OFF},
	{SEQ_HBM_OFF, SEQ_HBM_ON}
};

static unsigned char *ACL_TABLE[ACL_STATUS_MAX] = {SEQ_ACL_OFF, SEQ_ACL_08P, SEQ_ACL_15P};
static unsigned char *ACL_DIM_TABLE[ACL_DIMMING_MAX] = {SEQ_ACL_DIM_OFF, SEQ_ACL_DIM_ON};

#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
static unsigned char *FFC_TABLE[DYNAMIC_MIPI_INDEX_MAX] = {
	SEQ_FFC_SET_1177,
	SEQ_FFC_SET_1196,
	SEQ_FFC_SET_1150,
	SEQ_FFC_SET_1157,
};
#endif

/* platform brightness <-> acl opr and percent */
static unsigned int brightness_opr_table[ACL_STATUS_MAX][EXTEND_BRIGHTNESS + 1] = {
	{
		[0 ... EXTEND_BRIGHTNESS]			= ACL_STATUS_OFF,
	}, {
		[0 ... UI_MAX_BRIGHTNESS]			= ACL_STATUS_15P,
		[UI_MAX_BRIGHTNESS + 1 ... EXTEND_BRIGHTNESS]	= ACL_STATUS_08P
	}
};

/* platform brightness <-> gamma level */
static unsigned int brightness_table[EXTEND_BRIGHTNESS + 1] = {
	3,
	4, 5, 6, 7, 8, 9, 10, 10, 11, 12,
	13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
	23, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 49,
	50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
	60, 61, 62, 62, 63, 64, 65, 66, 67, 68,
	69, 70, 71, 72, 73, 74, 75, 76, 76, 77,
	78, 79, 80, 81, 82, 83, 84, 85, 86, 87,
	88, 89, 89, 90, 91, 92, 93, 94, 95, 96,
	97, 98, 99, 100, 101, 102, 102, 103, 104, 105,
	106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
	115, 116, 117, 118, 119, 120, 121, 122, 129, 136, /* 128: 122 */
	143, 150, 157, 165, 172, 179, 186, 193, 200, 207,
	214, 221, 228, 235, 243, 250, 257, 264, 271, 278,
	285, 292, 299, 306, 314, 321, 328, 335, 342, 349,
	356, 363, 370, 377, 384, 392, 399, 406, 413, 420,
	427, 434, 441, 448, 455, 463, 470, 477, 484, 491,
	498, 505, 512, 519, 526, 533, 541, 548, 555, 562,
	569, 576, 583, 590, 597, 604, 611, 619, 626, 633,
	640, 647, 654, 661, 668, 675, 682, 690, 697, 704,
	711, 718, 725, 732, 739, 746, 753, 760, 768, 775,
	782, 789, 796, 803, 810, 817, 824, 831, 839, 846,
	853, 860, 867, 874, 881, 888, 895, 902, 909, 917,
	924, 931, 938, 945, 952, 959, 966, 973, 980, 987,
	995, 1002, 1009, 1016, 1023, 4, 7, 11, 15, 18, /* 255: 1023 */
	22, 26, 29, 33, 37, 40, 44, 48, 51, 55,
	59, 62, 66, 70, 73, 77, 81, 84, 88, 92,
	95, 99, 103, 107, 110, 114, 118, 121, 125, 129,
	132, 136, 140, 143, 147, 151, 154, 158, 162, 165,
	169, 173, 176, 180, 184, 187, 191, 195, 198, 202,
	206, 209, 213, 217, 220, 224, 228, 231, 235, 239,
	242, 246, 250, 253, 257, 261, 264, 268, 272, 275,
	279, 283, 286, 290, 294, 297, 301, 305, 309, 312,
	316, 320, 323, 327, 331, 334, 338, 342, 345, 349,
	353, 356, 360, 364, 367, 371, 375, 378, 382, 386,
	389, 393, 397, 400, 404,
};

static unsigned int elvss_table[EXTEND_BRIGHTNESS + 1] = {
	[0 ... 255] = 0x8A,
	[256 ... 268] = 0x91,
	[269 ... 282] = 0x90,
	[283 ... 295] = 0x8F,
	[296 ... 309] = 0x8E,
	[310 ... 323] = 0x8D,
	[324 ... 336] = 0x8C,
	[337 ... 350] = 0x8B,
	[351 ... EXTEND_BRIGHTNESS] = 0x8A,
};

static unsigned int nit_table[EXTEND_BRIGHTNESS + 1] = {
	2,
	2, 3, 3, 4, 4, 4, 5, 5, 5, 6,
	6, 6, 7, 7, 8, 8, 8, 9, 9, 10,
	10, 10, 10, 11, 11, 12, 12, 13, 13, 13,
	14, 14, 15, 15, 15, 15, 16, 16, 17, 17,
	17, 18, 18, 19, 19, 19, 20, 20, 21, 21,
	21, 21, 22, 22, 23, 23, 23, 24, 24, 25,
	25, 25, 26, 26, 26, 27, 27, 27, 28, 28,
	29, 29, 29, 30, 30, 31, 31, 31, 31, 32,
	32, 33, 33, 34, 34, 34, 35, 35, 36, 36,
	36, 37, 37, 37, 38, 38, 38, 39, 39, 40,
	40, 40, 41, 41, 42, 42, 42, 42, 43, 43,
	44, 44, 44, 45, 45, 46, 46, 46, 47, 47,
	47, 48, 48, 48, 49, 49, 50, 50, 53, 56,
	59, 61, 64, 67, 70, 73, 76, 79, 82, 84,
	87, 90, 93, 96, 99, 102, 105, 107, 110, 113,
	116, 119, 122, 124, 128, 130, 133, 136, 139, 142,
	145, 147, 150, 153, 156, 159, 162, 165, 168, 170,
	173, 176, 179, 182, 185, 188, 191, 194, 197, 200,
	202, 205, 208, 211, 214, 217, 220, 223, 226, 229,
	232, 235, 238, 241, 243, 246, 249, 253, 256, 258,
	261, 264, 267, 270, 273, 276, 279, 282, 285, 288,
	291, 294, 297, 299, 302, 305, 308, 311, 314, 317,
	320, 323, 326, 329, 332, 335, 338, 340, 344, 347,
	350, 352, 355, 358, 361, 364, 367, 370, 373, 376,
	379, 382, 385, 388, 391, 393, 396, 399, 402, 405,
	408, 411, 414, 417, 420, 422, 423, 425, 427, 428,
	430, 432, 433, 435, 436, 438, 440, 441, 443, 444,
	446, 448, 449, 451, 452, 454, 456, 457, 459, 461,
	462, 464, 466, 468, 469, 471, 473, 474, 476, 477,
	479, 481, 482, 484, 485, 487, 489, 490, 492, 493,
	495, 497, 498, 500, 502, 503, 505, 507, 508, 510,
	512, 513, 515, 517, 518, 520, 522, 523, 525, 526,
	528, 530, 531, 533, 534, 536, 538, 539, 541, 543,
	544, 546, 547, 549, 551, 552, 554, 556, 558, 559,
	561, 563, 564, 566, 567, 569, 571, 572, 574, 575,
	577, 579, 580, 582, 584, 585, 587, 588, 590, 592,
	593, 595, 597, 598, 600,
};
#endif /* __EA8076_PARAM_H__ */
