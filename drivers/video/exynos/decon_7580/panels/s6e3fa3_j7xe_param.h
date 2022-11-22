#ifndef __S6E3FA3_J7XE_PARAM_H__
#define __S6E3FA3_J7XE_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>
#include "dynamic_aid_s6e3fa3_j7xe.h"

#define EXTEND_BRIGHTNESS	355
#define UI_MAX_BRIGHTNESS	255
#define UI_MIN_BRIGHTNESS	0
#define UI_DEFAULT_BRIGHTNESS	134
#define NORMAL_TEMPERATURE	25	/* 25 degrees Celsius */

#define GAMMA_CMD_CNT		ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET)
#define ACL_CMD_CNT		ARRAY_SIZE(SEQ_ACL_OFF)
#define OPR_CMD_CNT		ARRAY_SIZE(SEQ_OPR_ACL_OFF)
#define ELVSS_CMD_CNT		ARRAY_SIZE(SEQ_ELVSS_SET)
#define AID_CMD_CNT		ARRAY_SIZE(SEQ_AID_SETTING)
#define HBM_CMD_CNT		ARRAY_SIZE(SEQ_HBM_OFF)
#define TSET_CMD_CNT		ARRAY_SIZE(SEQ_TSET_SETTING)

#define LDI_REG_ELVSS		0xB6
#define LDI_REG_COORDINATE	0xA1
#define LDI_REG_DATE		LDI_REG_MTP
#define LDI_REG_ID		0x04
#define LDI_REG_CHIP_ID		0xD6
#define LDI_REG_MTP		0xC8
#define LDI_REG_HBM		0xB4
#define LDI_REG_RDDPM		0x0A
#define LDI_REG_RDDSM		0x0E
#define LDI_REG_ESDERR		0xEE

/* len is read length */
#define LDI_LEN_ELVSS		(ELVSS_CMD_CNT - 1)
#define LDI_LEN_COORDINATE	4
#define LDI_LEN_DATE		7
#define LDI_LEN_ID		3
#define LDI_LEN_CHIP_ID		5
#define LDI_LEN_MTP		35
#define LDI_LEN_HBM		28
#define LDI_LEN_RDDPM		1
#define LDI_LEN_RDDSM		1
#define LDI_LEN_ESDERR		1

/* offset is position including addr, not only para */
#define LDI_OFFSET_AOR_1	1
#define LDI_OFFSET_AOR_2	2

#define LDI_OFFSET_ELVSS_1	1	/* B6h 1st Para: MPS_CON */
#define LDI_OFFSET_ELVSS_2	2	/* B6h 2nd Para: ELVSS_Dim_offset */
#define LDI_OFFSET_ELVSS_3	22	/* B6h 22nd Para: ELVSS Temp Compensation */

#define LDI_OFFSET_HBM		1
#define LDI_OFFSET_ACL		1
#define LDI_OFFSET_TSET		1

#define LDI_GPARA_DATE		40	/* 0xC8 41st Para */
#define LDI_GPARA_HBM_ELVSS	22	/* 0xB6 23th para */


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

static unsigned char SEQ_PCD_SET_DET_LOW[] = {
	0xCC,
	0x5C
};

static unsigned char SEQ_ERR_FG_SETTING[] = {
	0xED,
	0x44
};

static unsigned char SEQ_AVC_SETTING_1[] = {
	0xB0,
	0x1E
};

static unsigned char SEQ_AVC_SETTING_2[] = {
	0xFD,
	0xB2
};

static unsigned char SEQ_GAMMA_CONDITION_SET[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x00, 0x00, 0x00, 0x00, 0x00
};

static unsigned char SEQ_AID_SETTING[] = {
	0xB2,
	0x00, 0x10
};

static unsigned char SEQ_ELVSS_SET[] = {
	0xB6,
	0xBC,	/* B6h 1st Para: MPS_CON */
	0x04,	/* B6h 2nd Para: ELVSS_Dim_offset */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00,
	0x00,	/* B6h 22nd Para: ELVSS Temp Compensation */
	0x00	/* B6h 23rd Para: OTP for B6h 22nd Para of HBM Interpolation */
};

static unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7,
	0x03
};

static unsigned char SEQ_HBM_OFF[] = {
	0x53,
	0x00
};

static unsigned char SEQ_HBM_ON[] = {
	0x53,
	0xC0
};

static unsigned char SEQ_OPR_ACL_OFF[] = {
	0xB5,
	0x40	/* 16 Frame Avg. at ACL Off */
};

static unsigned char SEQ_OPR_ACL_ON[] = {
	0xB5,
	0x50	/* 32 Frame Avg. at ACL On */
};

static unsigned char SEQ_ACL_OFF[] = {
	0x55,
	0x00
};

static unsigned char SEQ_ACL_ON[] = {
	0x55,
	0x01	/* 0x01 : ACL 15% (Default) */
};

static unsigned char SEQ_TSET_SETTING[] = {
	0xB8,
	0x19	/* (ex) 25 degree : 0x19 */
};

enum {
	ACL_STATUS_0P,
	ACL_STATUS_8P,
	ACL_STATUS_MAX
};

enum {
	OPR_STATUS_ACL_OFF,
	OPR_STATUS_ACL_ON,
	OPR_STATUS_MAX
};

enum {
	CAPS_OFF,
	CAPS_ON,
	CAPS_MAX
};

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

static unsigned char *HBM_TABLE[HBM_STATUS_MAX] = {SEQ_HBM_OFF, SEQ_HBM_ON};
static unsigned char *ACL_TABLE[ACL_STATUS_MAX] = {SEQ_ACL_OFF, SEQ_ACL_ON};
static unsigned char *OPR_TABLE[OPR_STATUS_MAX] = {SEQ_OPR_ACL_OFF, SEQ_OPR_ACL_ON};

static unsigned char elvss_mpscon_offset_data[IBRIGHTNESS_HBM_MAX][3] = {
	[IBRIGHTNESS_005NIT] = {0xB6, 0xAC, 0x07},
	[IBRIGHTNESS_006NIT] = {0xB6, 0xAC, 0x07},
	[IBRIGHTNESS_007NIT] = {0xB6, 0xAC, 0x07},
	[IBRIGHTNESS_008NIT] = {0xB6, 0xAC, 0x07},
	[IBRIGHTNESS_009NIT] = {0xB6, 0xAC, 0x07},
	[IBRIGHTNESS_010NIT] = {0xB6, 0xAC, 0x07},
	[IBRIGHTNESS_011NIT] = {0xB6, 0xAC, 0x07},
	[IBRIGHTNESS_012NIT] = {0xB6, 0xAC, 0x07},
	[IBRIGHTNESS_013NIT] = {0xB6, 0xAC, 0x07},
	[IBRIGHTNESS_014NIT] = {0xB6, 0xAC, 0x07},
	[IBRIGHTNESS_015NIT] = {0xB6, 0xAC, 0x07},
	[IBRIGHTNESS_016NIT] = {0xB6, 0xAC, 0x07},
	[IBRIGHTNESS_017NIT] = {0xB6, 0xAC, 0x07},
	[IBRIGHTNESS_019NIT] = {0xB6, 0xAC, 0x07},
	[IBRIGHTNESS_020NIT] = {0xB6, 0xAC, 0x07},
	[IBRIGHTNESS_021NIT] = {0xB6, 0xAC, 0x09},
	[IBRIGHTNESS_022NIT] = {0xB6, 0xAC, 0x0B},
	[IBRIGHTNESS_024NIT] = {0xB6, 0xAC, 0x0D},
	[IBRIGHTNESS_025NIT] = {0xB6, 0xAC, 0x0F},
	[IBRIGHTNESS_027NIT] = {0xB6, 0xAC, 0x11},
	[IBRIGHTNESS_029NIT] = {0xB6, 0xAC, 0x13},
	[IBRIGHTNESS_030NIT] = {0xB6, 0xAC, 0x15},
	[IBRIGHTNESS_032NIT] = {0xB6, 0xAC, 0x17},
	[IBRIGHTNESS_034NIT] = {0xB6, 0xAC, 0x1A},
	[IBRIGHTNESS_037NIT] = {0xB6, 0xAC, 0x1A},
	[IBRIGHTNESS_039NIT] = {0xB6, 0xAC, 0x1A},
	[IBRIGHTNESS_041NIT] = {0xB6, 0xBC, 0x1A},
	[IBRIGHTNESS_044NIT] = {0xB6, 0xBC, 0x1A},
	[IBRIGHTNESS_047NIT] = {0xB6, 0xBC, 0x1A},
	[IBRIGHTNESS_050NIT] = {0xB6, 0xBC, 0x1A},
	[IBRIGHTNESS_053NIT] = {0xB6, 0xBC, 0x1A},
	[IBRIGHTNESS_056NIT] = {0xB6, 0xBC, 0x1A},
	[IBRIGHTNESS_060NIT] = {0xB6, 0xBC, 0x1A},
	[IBRIGHTNESS_064NIT] = {0xB6, 0xBC, 0x1A},
	[IBRIGHTNESS_068NIT] = {0xB6, 0xBC, 0x1A},
	[IBRIGHTNESS_072NIT] = {0xB6, 0xBC, 0x1A},
	[IBRIGHTNESS_077NIT] = {0xB6, 0xBC, 0x1A},
	[IBRIGHTNESS_082NIT] = {0xB6, 0xBC, 0x19},
	[IBRIGHTNESS_087NIT] = {0xB6, 0xBC, 0x19},
	[IBRIGHTNESS_093NIT] = {0xB6, 0xBC, 0x19},
	[IBRIGHTNESS_098NIT] = {0xB6, 0xBC, 0x19},
	[IBRIGHTNESS_105NIT] = {0xB6, 0xBC, 0x18},
	[IBRIGHTNESS_111NIT] = {0xB6, 0xBC, 0x18},
	[IBRIGHTNESS_119NIT] = {0xB6, 0xBC, 0x17},
	[IBRIGHTNESS_126NIT] = {0xB6, 0xBC, 0x17},
	[IBRIGHTNESS_134NIT] = {0xB6, 0xBC, 0x17},
	[IBRIGHTNESS_143NIT] = {0xB6, 0xBC, 0x17},
	[IBRIGHTNESS_152NIT] = {0xB6, 0xBC, 0x16},
	[IBRIGHTNESS_162NIT] = {0xB6, 0xBC, 0x15},
	[IBRIGHTNESS_172NIT] = {0xB6, 0xBC, 0x14},
	[IBRIGHTNESS_183NIT] = {0xB6, 0xBC, 0x14},
	[IBRIGHTNESS_195NIT] = {0xB6, 0xBC, 0x13},
	[IBRIGHTNESS_207NIT] = {0xB6, 0xBC, 0x12},
	[IBRIGHTNESS_220NIT] = {0xB6, 0xBC, 0x11},
	[IBRIGHTNESS_234NIT] = {0xB6, 0xBC, 0x10},
	[IBRIGHTNESS_249NIT] = {0xB6, 0xBC, 0x0F},
	[IBRIGHTNESS_265NIT] = {0xB6, 0xBC, 0x0E},
	[IBRIGHTNESS_282NIT] = {0xB6, 0xBC, 0x0D},
	[IBRIGHTNESS_300NIT] = {0xB6, 0xBC, 0x0B},
	[IBRIGHTNESS_316NIT] = {0xB6, 0xBC, 0x0A},
	[IBRIGHTNESS_333NIT] = {0xB6, 0xBC, 0x09},
	[IBRIGHTNESS_360NIT] = {0xB6, 0xBC, 0x07},
	[IBRIGHTNESS_500NIT] = {0xB6, 0xBC, 0x07}
};

struct elvss_otp_info {
	unsigned int	nit;
	int not_otp[TEMP_MAX];
};

struct elvss_otp_info elvss_otp_data[IBRIGHTNESS_MAX] = {
	[IBRIGHTNESS_005NIT] = {5,	{-1, 0x1B, 0x1F}},
	[IBRIGHTNESS_006NIT] = {6,	{-1, 0x1B, 0x1F}},
	[IBRIGHTNESS_007NIT] = {7,	{-1, 0x1B, 0x1F}},
	[IBRIGHTNESS_008NIT] = {8,	{-1, 0x1B, 0x1F}},
	[IBRIGHTNESS_009NIT] = {9,	{-1, 0x1B, 0x1F}},
	[IBRIGHTNESS_010NIT] = {10,	{-1, 0x1B, 0x1F}},
	[IBRIGHTNESS_011NIT] = {11,	{-1, 0x1B, 0x1F}},
	[IBRIGHTNESS_012NIT] = {12,	{-1, 0x1B, 0x1F}},
	[IBRIGHTNESS_013NIT] = {13,	{-1, 0x1B, 0x1F}},
	[IBRIGHTNESS_014NIT] = {14,	{-1, 0x1B, 0x1F}},
	[IBRIGHTNESS_015NIT] = {15,	{-1, 0x1B, 0x1F}},
	[IBRIGHTNESS_016NIT] = {16,	{-1, 0x1B, 0x1F}},
	[IBRIGHTNESS_017NIT] = {17,	{-1, 0x1B, 0x1F}},
	[IBRIGHTNESS_019NIT] = {19,	{-1, 0x1B, 0x1F}},
	[IBRIGHTNESS_020NIT] = {20,	{-1, 0x1B, 0x1F}},
	[IBRIGHTNESS_021NIT] = {21,	{-1, 0x19, 0x1E}},
	[IBRIGHTNESS_022NIT] = {22,	{-1, 0x18, 0x1D}},
	[IBRIGHTNESS_024NIT] = {24,	{-1, 0x17, 0x1C}},
	[IBRIGHTNESS_025NIT] = {25,	{-1, 0x16, 0x1A}},
	[IBRIGHTNESS_027NIT] = {27,	{-1, 0x15, 0x18}},
	[IBRIGHTNESS_029NIT] = {29,	{-1, 0x14, 0x16}},
	[IBRIGHTNESS_030NIT] = {30,	{-1, 0x13, 0x14}},
};

static unsigned char AOR_TABLE[IBRIGHTNESS_HBM_MAX][AID_CMD_CNT] = {
	{0xB2, 0x70, 0x4C},/* 5nit */
	{0xB2, 0x70, 0x42},
	{0xB2, 0x70, 0x33},
	{0xB2, 0x70, 0x1E},
	{0xB2, 0x70, 0x14},
	{0xB2, 0x70, 0x05},/* 10nit */
	{0xB2, 0x60, 0xF6},/* 11nit */
	{0xB2, 0x60, 0xE7},/* 12nit */
	{0xB2, 0x60, 0xD6},
	{0xB2, 0x60, 0xC8},
	{0xB2, 0x60, 0xBA},
	{0xB2, 0x60, 0xAB},
	{0xB2, 0x60, 0xA1},
	{0xB2, 0x60, 0x82},
	{0xB2, 0x60, 0x6B},
	{0xB2, 0x60, 0x57},
	{0xB2, 0x60, 0x42},
	{0xB2, 0x60, 0x0D},/*24nit*/
	{0xB2, 0x50, 0xF8},/* 25nit */
	{0xB2, 0x50, 0xCE},
	{0xB2, 0x50, 0x98},
	{0xB2, 0x50, 0x7A},
	{0xB2, 0x50, 0x44},
	{0xB2, 0x50, 0x22},/* 34nit */
	{0xB2, 0x40, 0xE4},/* 37nit */
	{0xB2, 0x40, 0xBE},
	{0xB2, 0x40, 0x95},
	{0xB2, 0x40, 0x56},
	{0xB2, 0x40, 0x21},/* 47nit */
	{0xB2, 0x30, 0xE3},/* 50nit */
	{0xB2, 0x30, 0xA5},
	{0xB2, 0x30, 0x64},
	{0xB2, 0x30, 0x12},/* 60nit */
	{0xB2, 0x20, 0xD5},/* 64nit */
	{0xB2, 0x20, 0xC5},
	{0xB2, 0x20, 0xC5},
	{0xB2, 0x20, 0xC5},
	{0xB2, 0x20, 0xC5},
	{0xB2, 0x20, 0xC5},
	{0xB2, 0x20, 0xC5},
	{0xB2, 0x20, 0xC5},
	{0xB2, 0x20, 0xC5},
	{0xB2, 0x20, 0xC5},
	{0xB2, 0x20, 0xC5},
	{0xB2, 0x20, 0xC5},
	{0xB2, 0x20, 0xC5},
	{0xB2, 0x20, 0xC5},
	{0xB2, 0x20, 0xC5},
	{0xB2, 0x20, 0xC5},/* 162nit */
	{0xB2, 0x20, 0x78},
	{0xB2, 0x20, 0x15},
	{0xB2, 0x10, 0xB6},/* 195nit */
	{0xB2, 0x10, 0x59},
	{0xB2, 0x00, 0xF5},
	{0xB2, 0x00, 0x84},/* 234nit */
	{0xB2, 0x00, 0x10},
	{0xB2, 0x00, 0x10},
	{0xB2, 0x00, 0x10},
	{0xB2, 0x00, 0x10},
	{0xB2, 0x00, 0x10},
	{0xB2, 0x00, 0x10},
	{0xB2, 0x00, 0x10},/* 360nit */
	{0xB2, 0x00, 0x10}	/* 500nit */
};

/* platform brightness <-> gamma level */
static unsigned int brightness_table[EXTEND_BRIGHTNESS + 1] = {
	[0 ... 5] =		IBRIGHTNESS_005NIT,
	[6 ... 6] =		IBRIGHTNESS_006NIT,
	[7 ... 7] =		IBRIGHTNESS_007NIT,
	[8 ... 8] =		IBRIGHTNESS_008NIT,
	[9 ... 9] =		IBRIGHTNESS_009NIT,
	[10 ... 10] =		IBRIGHTNESS_010NIT,
	[11 ... 11] =		IBRIGHTNESS_011NIT,
	[12 ... 12] =		IBRIGHTNESS_012NIT,
	[13 ... 13] =		IBRIGHTNESS_013NIT,
	[14 ... 14] =		IBRIGHTNESS_014NIT,
	[15 ... 15] =		IBRIGHTNESS_015NIT,
	[16 ... 16] =		IBRIGHTNESS_016NIT,
	[17 ... 17] =		IBRIGHTNESS_017NIT,
	[18 ... 18] =		IBRIGHTNESS_019NIT,
	[19 ... 19] =		IBRIGHTNESS_020NIT,
	[20 ... 20] =		IBRIGHTNESS_021NIT,
	[21 ... 21] =		IBRIGHTNESS_022NIT,
	[22 ... 22] =		IBRIGHTNESS_024NIT,
	[23 ... 23] =		IBRIGHTNESS_025NIT,
	[24 ... 24] =		IBRIGHTNESS_027NIT,
	[25 ... 25] =		IBRIGHTNESS_029NIT,
	[26 ... 26] =		IBRIGHTNESS_030NIT,
	[27 ... 27] =		IBRIGHTNESS_032NIT,
	[28 ... 38] =		IBRIGHTNESS_034NIT,
	[29 ... 29] =		IBRIGHTNESS_037NIT,
	[30 ... 34] =		IBRIGHTNESS_039NIT,
	[31 ... 32] =		IBRIGHTNESS_041NIT,
	[33 ... 34] =		IBRIGHTNESS_044NIT,
	[35 ... 36] =		IBRIGHTNESS_047NIT,
	[37 ... 38] =		IBRIGHTNESS_050NIT,
	[39 ... 40] =		IBRIGHTNESS_053NIT,
	[41 ... 43] =		IBRIGHTNESS_056NIT,
	[44 ... 45] =		IBRIGHTNESS_060NIT,
	[46 ... 48] =		IBRIGHTNESS_064NIT,
	[49 ... 51] =		IBRIGHTNESS_068NIT,
	[52 ... 55] =		IBRIGHTNESS_072NIT,
	[56 ... 59] =		IBRIGHTNESS_077NIT,
	[60 ... 63] =		IBRIGHTNESS_082NIT,
	[64 ... 67] =		IBRIGHTNESS_087NIT,
	[68 ... 71] =		IBRIGHTNESS_093NIT,
	[71 ... 74] =		IBRIGHTNESS_098NIT,
	[75 ... 79] =		IBRIGHTNESS_105NIT,
	[80 ... 85] =		IBRIGHTNESS_111NIT,
	[86 ... 91] =		IBRIGHTNESS_119NIT,
	[92 ... 96] =		IBRIGHTNESS_126NIT,
	[97 ... 103] =		IBRIGHTNESS_134NIT,
	[104 ... 110] =		IBRIGHTNESS_143NIT,
	[111 ... 117] =		IBRIGHTNESS_152NIT,
	[118 ... 125] =		IBRIGHTNESS_162NIT,
	[126 ... 133] =		IBRIGHTNESS_172NIT,
	[134 ... 142] =		IBRIGHTNESS_183NIT,
	[143 ... 151] =		IBRIGHTNESS_195NIT,
	[152 ... 161] =		IBRIGHTNESS_207NIT,
	[162 ... 171] =		IBRIGHTNESS_220NIT,
	[172 ... 182] =		IBRIGHTNESS_234NIT,
	[183 ... 194] =		IBRIGHTNESS_249NIT,
	[195 ... 206] =		IBRIGHTNESS_265NIT,
	[207 ... 218] =		IBRIGHTNESS_282NIT,
	[219 ... 230] =		IBRIGHTNESS_300NIT,
	[231 ... 242] =		IBRIGHTNESS_316NIT,
	[243 ... 253] =		IBRIGHTNESS_333NIT,
	[254 ... 354] =		IBRIGHTNESS_360NIT,
	[EXTEND_BRIGHTNESS ... EXTEND_BRIGHTNESS] =		IBRIGHTNESS_500NIT
};

#endif /* __S6E3FA3_J7XE_PARAM_H__ */
