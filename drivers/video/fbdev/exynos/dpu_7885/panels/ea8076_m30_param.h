#ifndef __EA8076_PARAM_H__
#define __EA8076_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>

#define EXTEND_BRIGHTNESS	365
#define UI_MAX_BRIGHTNESS	255
#define UI_DEFAULT_BRIGHTNESS	128

#define NORMAL_TEMPERATURE	25	/* 25 degrees Celsius */

#define ACL_CMD_CNT				((u16)ARRAY_SIZE(SEQ_ACL_OFF))
#define HBM_CMD_CNT				((u16)ARRAY_SIZE(SEQ_HBM_OFF))
#define ELVSS_CMD_CNT				((u16)ARRAY_SIZE(SEQ_ELVSS_SET))

#define LDI_REG_BRIGHTNESS			0x51
#define LDI_REG_ID				0x04
#define LDI_REG_COORDINATE			0xEA
#define LDI_REG_DATE				LDI_REG_COORDINATE
#define LDI_REG_MANUFACTURE_INFO		LDI_REG_COORDINATE
#define LDI_REG_MANUFACTURE_INFO_CELL_ID	0xEF
#define LDI_REG_CHIP_ID				0xD1
#define LDI_REG_ELVSS				0xB7

/* len is read length */
#define LDI_LEN_ID				3
#define LDI_LEN_COORDINATE			4
#define LDI_LEN_DATE				7
#define LDI_LEN_MANUFACTURE_INFO		4
#define LDI_LEN_MANUFACTURE_INFO_CELL_ID	16
#define LDI_LEN_CHIP_ID				6
#define LDI_LEN_ELVSS				(ELVSS_CMD_CNT - 1)

/* offset is position including addr, not only para */
#define LDI_OFFSET_ACL		1
#define LDI_OFFSET_HBM		1
#define LDI_OFFSET_ELVSS_1	6
#define LDI_OFFSET_ELVSS_2	46

#define LDI_GPARA_COORDINATE			3	/* EAh 4th Para: x, y */
#define LDI_GPARA_DATE				7	/* EAh 8th Para: [D7:D4]: Year */
#define LDI_GPARA_MANUFACTURE_INFO		15	/* EAh 16th Para: [D7:D4]:Site */
#define LDI_GPARA_MANUFACTURE_INFO_CELL_ID	2	/* EFh 3rd Para ~ 18th Para */
#define LDI_GPARA_CHIP_ID			55	/* D1h 56th Para */
#define LDI_GPARA_ELVSS_NORMAL			7	/* B7h 8th Para */
#define LDI_GPARA_ELVSS_HBM			8	/* B7h 9th Para */

#define	LDI_REG_RDDPM		0x0A	/* Read Display Power Mode */
#define	LDI_LEN_RDDPM		1

#define	LDI_REG_RDDSM		0x0E	/* Read Display Signal Mode */
#define	LDI_LEN_RDDSM		1

#ifdef CONFIG_DISPLAY_USE_INFO
#define	LDI_REG_RDNUMPE		0x05		/* DPUI_KEY_PNDSIE: Read Number of the Errors on DSI */
#define	LDI_LEN_RDNUMPE		1
#define LDI_PNDSIE_MASK		(GENMASK(6, 0))

/*
 * ESD_ERROR[0] =  MIPI DSI error is occurred by ESD.
 * ESD_ERROR[1] =  HS CLK lane error is occurred by ESD.
 * ESD_ERROR[2] =  VLIN3 error is occurred by ESD.
 * ESD_ERROR[3] =  ELVDD error is occurred by ESD.
 * ESD_ERROR[4]  = CHECK_SUM error is occurred by ESD.
 * ESD_ERROR[5] =  Internal HSYNC error is occurred by ESD.
 * ESD_ERROR[6] =  VLIN1 error is occurred by ESD
 */
#define LDI_REG_ESDERR		0xEE		/* DPUI_KEY_PNELVDE, DPUI_KEY_PNVLI1E, DPUI_KEY_PNVLO3E, DPUI_KEY_PNESDE */
#define LDI_LEN_ESDERR		1
#define LDI_PNELVDE_MASK	(BIT(3))	/* ELVDD error */
#define LDI_PNVLI1E_MASK	(BIT(6))	/* VLIN1 error */
#define LDI_PNVLO3E_MASK	(BIT(2))	/* VLIN3 error */
#define LDI_PNESDE_MASK		(BIT(2) | BIT(3) | BIT(6))

#define LDI_REG_RDDSDR		0x0F		/* DPUI_KEY_PNSDRE: Read Display Self-Diagnostic Result */
#define LDI_LEN_RDDSDR		1
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
	0x00, 0x00
};

static unsigned char SEQ_PAGE_ADDR_SET[] = {
	0x2B,
	0x00, 0x00, 0x09, 0x23
};

static unsigned char SEQ_FFC_SET[] = {
	0xE9,
	0x11, 0x55, 0xA6, 0x75, 0xA3, 0xB2, 0x41, 0xC3, 0x00, 0x1A,
	0xB8		/* MIPI Speed 1.1Gbps */
};

static unsigned char SEQ_ERR_FG_SET[] = {
	0xE1,
	0x00, 0x00, 0x02, 0x10, 0x10, 0x10, 0x00, 0x00, 0x20, 0x00,
	0x00, 0x01, 0x19
};

static unsigned char SEQ_VSYNC_SET[] = {
	0xE0,
	0x01		/* Vsync Enable */
};

static unsigned char SEQ_ASWIRE_OFF[] = {
	0xD5,
	0x83, 0xFF, 0x5C, 0x44, 0x89, 0x89, 0x00, 0x00, 0x00, 0x00,
	0x00
};

static unsigned char SEQ_CHARGE_PUMP_SET[] = {
	0xD4,
	0x8B, 0xE3
};

static unsigned char SEQ_ELVSS_SET[] = {
	0xB7,
	0x01, 0x53, 0x28, 0x4D, 0x00,
	0x00,	/* 6th: ELVSS return */
	0x04,	/* 7th para : Smooth transition 4-frame */
	0x00,	/* 8th: Normal ELVSS OTP */
	0x00,	/* 9th: HBM ELVSS OTP */
	0x00,
	0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x41, 0x41, 0x42, 0x42,
	0x42, 0x42, 0x83, 0xC3, 0x83, 0xC3, 0x83, 0xC3, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00,	/* 46th: TSET */
};

static unsigned char SEQ_HBM_ON[] = {
	0x53,
	0xE0,
};

static unsigned char SEQ_HBM_OFF[] = {
	0x53,
	0x28,
};

static unsigned char SEQ_ACL_SETTING_1[] = {
	0xB0,
	0xD7
};

static unsigned char SEQ_ACL_SETTING_2[] = {
	0xB9,
	0x02, 0xA1, 0x8C, 0x4B
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

#if defined(CONFIG_EXYNOS_SUPPORT_DOZE)
enum {
	ALPM_OFF,
	ALPM_ON_LOW,	/* ALPM 2 NIT */
	HLPM_ON_LOW,	/* HLPM 2 NIT */
	ALPM_ON_HIGH,	/* ALPM 60 NIT */
	HLPM_ON_HIGH,	/* HLPM 60 NIT */
	ALPM_MODE_MAX
};

static unsigned char SEQ_HLPM_ON_02[] = {
	0x53,
	0x23		/* 0x23 : HLPM 2nit On */
};

static unsigned char SEQ_HLPM_ON_60[] = {
	0x53,
	0x22		/* 0x22 : HLPM 60nit On */
};

static unsigned char SEQ_HLPM_OFF[] = {
	0x53,
	0x20
};

static unsigned char SEQ_VLOUT3_SET[] = {
	0xD4,
	0x8B
};

static unsigned char SEQ_GPARA_68[] = {
	0xB0,
	0x68
};

static unsigned char SEQ_HLPM_AOR_60[] = {
	0xB9,
	0x01, 0x48
};
#endif

#if defined(CONFIG_SEC_FACTORY)
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

static unsigned char SEQ_FD_ON[] = {
	0xD5,
	0x01
};

static unsigned char SEQ_FD_OFF[] = {
	0xD5,
	0x02
};

static unsigned char SEQ_GPARA_FD[] = {
	0xB0,
	0x09
};
#endif

enum {
	ACL_STATUS_OFF,
	ACL_STATUS_08P,
	ACL_STATUS_15P,
	ACL_STATUS_MAX,
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
static unsigned char *ACL_TABLE[ACL_STATUS_MAX] = {SEQ_ACL_OFF, SEQ_ACL_08P, SEQ_ACL_15P};

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
	6, 9, 12, 15, 18, 21, 24, 27, 30, 33,
	36, 39, 42, 45, 48, 53, 56, 60, 63, 67,
	70, 74, 77, 81, 84, 88, 91, 95, 98, 102,
	105, 109, 112, 116, 120, 123, 127, 130, 134, 137,
	141, 144, 148, 151, 155, 158, 162, 165, 169, 172,
	176, 179, 183, 186, 190, 193, 197, 200, 204, 207,
	211, 214, 218, 221, 225, 228, 232, 235, 239, 242,
	246, 249, 253, 257, 260, 264, 267, 271, 274, 278,
	281, 285, 288, 292, 295, 299, 302, 306, 309, 313,
	316, 320, 323, 327, 330, 334, 337, 341, 344, 348,
	351, 355, 358, 362, 365, 369, 372, 376, 379, 383,
	387, 390, 394, 397, 401, 404, 408, 411, 415, 418,
	422, 425, 429, 432, 436, 439, 443, 445, 451, 456,	/* 128: 445 */
	460, 465, 470, 474, 479, 483, 488, 492, 497, 500,
	506, 510, 515, 520, 524, 529, 533, 538, 542, 547,
	551, 556, 561, 565, 570, 574, 579, 583, 588, 592,
	597, 601, 606, 611, 615, 620, 624, 629, 633, 638,
	642, 647, 652, 656, 661, 665, 670, 674, 679, 683,
	688, 693, 697, 702, 706, 711, 715, 720, 724, 729,
	733, 738, 743, 747, 752, 756, 761, 765, 770, 774,
	779, 784, 788, 793, 797, 802, 806, 811, 815, 820,
	825, 829, 834, 838, 843, 847, 852, 856, 861, 865,
	870, 875, 879, 884, 888, 893, 897, 902, 906, 911,
	916, 920, 925, 929, 934, 938, 943, 947, 952, 956,
	961, 966, 970, 975, 979, 984, 988, 993, 997, 1002,
	1007, 1011, 1016, 1020, 1023, 5, 9, 12, 16, 20,	/* 225: 1023 */
	23, 27, 31, 34, 38, 42, 45, 49, 53, 55,
	60, 64, 67, 71, 75, 78, 82, 86, 89, 93,
	97, 100, 104, 108, 111, 115, 119, 123, 126, 130,
	134, 137, 141, 145, 148, 152, 156, 159, 163, 167,
	170, 174, 178, 181, 185, 189, 192, 196, 200, 203,
	207, 211, 214, 218, 222, 225, 229, 233, 236, 240,
	244, 247, 251, 255, 258, 262, 266, 269, 273, 277,
	280, 284, 288, 291, 295, 299, 302, 306, 310, 313,
	317, 321, 325, 328, 332, 336, 339, 343, 347, 350,
	354, 358, 361, 365, 369, 372, 376, 380, 383, 387,
	391, 394, 398, 402, 404,
};

static unsigned int elvss_table[EXTEND_BRIGHTNESS + 1] = {
	[0 ... 255] = 0x90,
	[256 ... 268] = 0x90,
	[269 ... 281] = 0x99,
	[282 ... 295] = 0x97,
	[296 ... 308] = 0x96,
	[309 ... 322] = 0x95,
	[323 ... 336] = 0x93,
	[337 ... 349] = 0x92,
	[350 ... EXTEND_BRIGHTNESS - 1] = 0x91,
	[EXTEND_BRIGHTNESS] = 0x90,
};

#endif /* __EA8076_PARAM_H__ */
