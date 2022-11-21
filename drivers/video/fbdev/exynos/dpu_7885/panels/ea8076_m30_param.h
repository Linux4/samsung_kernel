#ifndef __EA8076_M30_PARAM_H__
#define __EA8076_M30_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>

#define EXTEND_BRIGHTNESS	365
#define UI_MAX_BRIGHTNESS	255
#define UI_DEFAULT_BRIGHTNESS	128

#define ACL_CMD_CNT				((u16)ARRAY_SIZE(SEQ_ACL_OFF))
#define HBM_CMD_CNT				((u16)ARRAY_SIZE(SEQ_HBM_OFF))

#define LDI_REG_BRIGHTNESS			0x51
#define LDI_REG_ID				0x04
#define LDI_REG_COORDINATE			0xEA
#define LDI_REG_DATE				LDI_REG_COORDINATE
#define LDI_REG_MANUFACTURE_INFO		LDI_REG_COORDINATE

/* len is read length */
#define LDI_LEN_ID				3
#define LDI_LEN_COORDINATE			4
#define LDI_LEN_DATE				7
#define LDI_LEN_MANUFACTURE_INFO		4

/* offset is position including addr, not only para */
#define LDI_OFFSET_ACL		1
#define LDI_OFFSET_HBM		1

#define LDI_GPARA_COORDINATE		3	/* EAh 4th Para: x, y */
#define LDI_GPARA_DATE			7	/* EAh 8th Para: [D7:D4]: Year */
#define LDI_GPARA_MANUFACTURE_INFO	15	/* EAh 16th Para: [D7:D4]:Site */

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
	0x11, 0x55, 0x98, 0x96, 0x80, 0xB2, 0x41, 0xC3, 0x00, 0x1A,
	0xB8		/* MIPI Speed 1.2Gbps */
};

static unsigned char SEQ_HBM_ON[] = {
	0x53,
	0xE8,
};

static unsigned char SEQ_HBM_OFF[] = {
	0x53,
	0x28,
};

static unsigned char SEQ_ACL_OFF[] = {
	0x55,
	0x00
};

static unsigned char SEQ_ACL_5P[] = {
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
	0x28
};
#endif

enum {
	ACL_STATUS_OFF,
	ACL_STATUS_5P,
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
static unsigned char *ACL_TABLE[ACL_STATUS_MAX] = {SEQ_ACL_OFF, SEQ_ACL_5P, SEQ_ACL_15P};

/* platform brightness <-> acl opr and percent */
static unsigned int brightness_opr_table[ACL_STATUS_MAX][EXTEND_BRIGHTNESS + 1] = {
	{
		[0 ... UI_MAX_BRIGHTNESS - 1]			= ACL_STATUS_15P,
		[UI_MAX_BRIGHTNESS ... EXTEND_BRIGHTNESS]	= ACL_STATUS_OFF,
	}, {
		[0 ... UI_MAX_BRIGHTNESS]					= ACL_STATUS_15P,
		[UI_MAX_BRIGHTNESS + 1 ... EXTEND_BRIGHTNESS]			= ACL_STATUS_5P
	}
};

/* platform brightness <-> gamma level */
static unsigned int brightness_table[EXTEND_BRIGHTNESS + 1] = {
	0,
	3, 6, 9, 13, 16, 20, 23, 27, 30, 34, /* 1: 3 */
	37, 41, 44, 48, 51, 55, 58, 62, 65, 69,
	72, 76, 79, 83, 86, 90, 93, 96, 100, 103,
	107, 110, 114, 117, 121, 124, 128, 131, 135, 138,
	142, 145, 149, 152, 156, 159, 163, 166, 170, 173,
	177, 180, 183, 187, 190, 194, 197, 201, 204, 208,
	211, 215, 218, 222, 225, 229, 232, 236, 239, 243,
	246, 250, 253, 257, 260, 264, 267, 270, 274, 277,
	281, 284, 288, 291, 295, 298, 302, 305, 309, 312,
	316, 319, 323, 326, 330, 333, 337, 340, 344, 347,
	351, 354, 357, 361, 364, 368, 371, 375, 378, 382,
	385, 389, 392, 396, 399, 403, 406, 410, 413, 417,
	420, 424, 427, 431, 434, 438, 441, 445, 449, 454, /* 128: 445 */
	458, 463, 467, 472, 476, 481, 485, 490, 495, 499,
	504, 508, 513, 517, 522, 526, 531, 536, 540, 545,
	549, 554, 558, 563, 567, 572, 576, 581, 586, 590,
	595, 599, 604, 608, 613, 617, 622, 627, 631, 636,
	640, 645, 649, 654, 658, 663, 668, 672, 677, 681,
	686, 690, 695, 699, 704, 708, 713, 718, 722, 727,
	731, 736, 740, 745, 749, 754, 759, 763, 768, 772,
	777, 781, 786, 790, 795, 799, 804, 809, 813, 818,
	822, 827, 831, 836, 840, 845, 850, 854, 859, 863,
	868, 872, 877, 881, 886, 891, 895, 900, 904, 909,
	913, 918, 922, 927, 931, 936, 941, 945, 950, 954,
	959, 963, 968, 972, 977, 982, 986, 991, 995, 1000,
	1004, 1009, 1013, 1018, 1023, 1023, 1023, 1023, 1023, 1023, /* 255: 1023 */
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023,
};

#ifdef CONFIG_LCD_HMT
#define DEFAULT_HMT_BRIGHTNESS			162

static unsigned int hmt_brightness_table[EXTEND_BRIGHTNESS + 1] = {
	0,
	3, 6, 9, 13, 16, 20, 23, 27, 30, 34,
	37, 41, 44, 48, 51, 55, 58, 62, 65, 69,
	72, 76, 79, 83, 86, 90, 93, 96, 100, 103,
	107, 110, 114, 117, 121, 124, 128, 131, 135, 138,
	142, 145, 149, 152, 156, 159, 163, 166, 170, 173,
	177, 180, 183, 187, 190, 194, 197, 201, 204, 208,
	211, 215, 218, 222, 225, 229, 232, 236, 239, 243,
	246, 250, 253, 257, 260, 264, 267, 270, 274, 277,
	281, 284, 288, 291, 295, 298, 302, 305, 309, 312,
	316, 319, 323, 326, 330, 333, 337, 340, 344, 347,
	351, 354, 357, 361, 364, 368, 371, 375, 378, 382,
	385, 389, 392, 396, 399, 403, 406, 410, 413, 417,
	420, 424, 427, 431, 434, 438, 441, 445, 449, 454,
	458, 463, 467, 472, 476, 481, 485, 490, 495, 499,
	504, 508, 513, 517, 522, 526, 531, 536, 540, 545,
	549, 554, 558, 563, 567, 572, 576, 581, 586, 590,
	595, 599, 604, 608, 613, 617, 622, 627, 631, 636,
	640, 645, 649, 654, 658, 663, 668, 672, 677, 681,
	686, 690, 695, 699, 704, 708, 713, 718, 722, 727,
	731, 736, 740, 745, 749, 754, 759, 763, 768, 772,
	777, 781, 786, 790, 795, 799, 804, 809, 813, 818,
	822, 827, 831, 836, 840, 845, 850, 854, 859, 863,
	868, 872, 877, 881, 886, 891, 895, 900, 904, 909,
	913, 918, 922, 927, 931, 936, 941, 945, 950, 954,
	959, 963, 968, 972, 977, 982, 986, 991, 995, 1000,
	1004, 1009, 1013, 1018, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023,
};
#if 0

/* when get OP code will modify */
static unsigned char SEQ_HMT_ON[] = {
	0xB1,
	0x01, 0xC2, 0x10, 0x00
};

static unsigned char SEQ_HMT_OFF[] = {
	0xB1,
	0x01, 0xC2, 0x10, 0x80
};

static unsigned char SEQ_IRC_OFF[] = {
	0xB8,
	0x00, 0x00,	/* OTP_Value, OTP_Value */
	0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
	0x00
};

#define IRC_OFF_CNT				((u16)ARRAY_SIZE(SEQ_IRC_OFF))
#endif
#endif

#endif /* __EA8076_M30_PARAM_H__ */
