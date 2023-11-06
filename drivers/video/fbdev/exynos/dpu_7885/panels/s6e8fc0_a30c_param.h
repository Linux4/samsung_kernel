#ifndef __S6E8FC0_PARAM_H__
#define __S6E8FC0_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>

#define EXTEND_BRIGHTNESS	355
#define UI_MAX_BRIGHTNESS	255
#define UI_DEFAULT_BRIGHTNESS	128

#define NORMAL_TEMPERATURE	25	/* 25 degrees Celsius */

#define ACL_CMD_CNT			((u16)ARRAY_SIZE(SEQ_ACL_OFF))
#define OPR_CMD_CNT			((u16)ARRAY_SIZE(SEQ_ACL_OPR_OFF))
#define HBM_CMD_CNT				((u16)ARRAY_SIZE(SEQ_HBM_OFF))
#define ELVSS_CMD_CNT				((u16)ARRAY_SIZE(SEQ_ELVSS_SET))

#define LDI_REG_BRIGHTNESS			0x51
#define LDI_REG_ID				0x04
#define LDI_REG_COORDINATE			0xA1
#define LDI_REG_DATE				LDI_REG_COORDINATE
#define LDI_REG_MANUFACTURE_INFO		LDI_REG_COORDINATE
#define LDI_REG_CELL_ID		LDI_REG_COORDINATE
#define LDI_REG_CHIP_ID				0xD6
#define LDI_REG_ELVSS			0xBF

/* len is read length */
#define LDI_LEN_ID				3
#define LDI_LEN_COORDINATE			4
#define LDI_LEN_DATE				7
#define LDI_LEN_MANUFACTURE_INFO		20
#define LDI_LEN_CELL_ID		20
#define LDI_LEN_CHIP_ID				5
#define LDI_LEN_ELVSS				(ELVSS_CMD_CNT - 1)

/* offset is position including addr, not only para */
#define LDI_OFFSET_HBM		1
#define LDI_OFFSET_ELVSS_1	4		/* BFh 4th para: ELVSS	*/
#define LDI_OFFSET_ELVSS_2	1		/* BFh 1th para: TSET	*/

#define LDI_GPARA_COORDINATE			0	/* A1h 1st Para: x, y */
#define LDI_GPARA_DATE				4	/* A1h 5th Para: [D7:D4]: Year */
#define LDI_GPARA_MANUFACTURE_INFO		11	/* A1h 12th Para: [D7:D4]:Site */
#define LDI_GPARA_CELL_ID		15	/* A1h 16th Para ~ 31th para */

#define	LDI_REG_RDDPM		0x0A	/* Read Display Power Mode */
#define	LDI_LEN_RDDPM		1

#define	LDI_REG_RDDSM		0x0E	/* Read Display Signal Mode */
#define	LDI_LEN_RDDSM		1

#ifdef CONFIG_DISPLAY_USE_INFO
#define	LDI_REG_RDNUMPE		0x05		/* DPUI_KEY_PNDSIE: Read Number of the Errors on DSI */
#define	LDI_LEN_RDNUMPE		1
#define LDI_PNDSIE_MASK		(GENMASK(6, 0))

/*
 * ESD_ERROR[2] =  VLIN3 error is occurred by ESD.
 * ESD_ERROR[3] =  ELVDD error is occurred by ESD.
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
	0x00
};

static unsigned char SEQ_ERR_FG_SET[] = {
	0xED,
	0x00, 0x4C
};

static unsigned char SEQ_GPARA_CELL_ID[] = {
	0xB0,
	0x0F, LDI_REG_CELL_ID
};

static unsigned char SEQ_ELVSS_SET[] = {
	0xBF,
	0x19,	/* 1st para TSET	*/
	0x0D, 0x80,
	0xD0,	/* 4th para ELVSS	*/
	0x04		/* 5th para 4 frame dim speed */
};

static unsigned char SEQ_HBM_ON[] = {
	0x53,
	0xE0,
};

static unsigned char SEQ_HBM_OFF[] = {
	0x53,
	0x28,
};

static unsigned char SEQ_EDGE_DIM[] = {
	0xB2,
	0x0A, 0x0C, 0xD8, 0xD8, 0xBA, 0xBA	/* 5th~6th 0xBA, 0xBA - 50% */
};

static unsigned char SEQ_ACL_OPR_OFF[] = {
	0xC1,
	0x41,	/* 16 Frame Avg. at ACL Off */
	0x11, 0x12, 0x15, 0x55, 0x55, 0x55, 0x0F, 0x1B, 0x18, 0x47,
	0x02,
	0x61, 0x28,	/* 13th~14th para ACL 15% */
	0x4A,
	0x41, 0xFC,	/* 16th~17th para Start step 50% */
	0x00
};

static unsigned char SEQ_ACL_OPR_08P[] = {
	0xC1,
	0x41,	/* 16 Frame Avg. at ACL Off */
	0x11, 0x12, 0x15, 0x55, 0x55, 0x55, 0x0F, 0x1B, 0x18, 0x47,
	0x02,
	0x60, 0x98,	/* 13th~14th para ACL 8% */
	0x4A,
	0x42, 0x64,	/* 16th~17th para Start step 60% */
	0x00
};

static unsigned char SEQ_ACL_OPR_15P[] = {
	0xC1,
	0x51,	/* 32 Frame Avg. at ACL On */
	0x11, 0x12, 0x15, 0x55, 0x55, 0x55, 0x0F, 0x1B, 0x18, 0x47,
	0x02,
	0x61, 0x28,	/* 13th~14th para ACL 15% */
	0x4A,
	0x41, 0xFC,	/* 16th~17th para Start step 50% */
	0x00
};

static unsigned char SEQ_ACL_OFF[] = {
	0x55,
	0x00
};

static unsigned char SEQ_ACL_ON[] = {
	0x55,
	0x02
};

enum {
	ACL_STATUS_OFF,
	ACL_STATUS_ON,
	ACL_STATUS_MAX
};

enum {
	OPR_STATUS_OFF,
	OPR_STATUS_08P,
	OPR_STATUS_15P,
	OPR_STATUS_MAX
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
static unsigned char *OPR_TABLE[OPR_STATUS_MAX] = {SEQ_ACL_OPR_OFF, SEQ_ACL_OPR_08P, SEQ_ACL_OPR_15P};

/* platform brightness <-> acl opr and percent */
static unsigned int brightness_opr_table[ACL_STATUS_MAX][EXTEND_BRIGHTNESS + 1] = {
	{
		[0 ... EXTEND_BRIGHTNESS]			= OPR_STATUS_OFF,
	}, {
		[0 ... UI_MAX_BRIGHTNESS]					= OPR_STATUS_15P,
		[UI_MAX_BRIGHTNESS + 1 ... EXTEND_BRIGHTNESS]			= OPR_STATUS_08P
	}
};

/* platform brightness <-> gamma level */
static unsigned int brightness_table[EXTEND_BRIGHTNESS + 1] = {
	15,
	18, 21, 23, 26, 29, 32, 35, 38, 40, 43,
	46, 49, 52, 54, 56, 63, 66, 71, 74, 80,
	83, 86, 91, 94, 100, 102, 108, 111, 117, 119,
	122, 128, 131, 136, 139, 145, 148, 153, 156, 159,
	164, 167, 173, 176, 181, 184, 190, 193, 195, 201,
	204, 210, 212, 218, 221, 227, 229, 232, 238, 241,
	246, 249, 255, 258, 263, 266, 269, 274, 277, 283,
	286, 291, 294, 300, 303, 305, 311, 314, 320, 322,
	328, 331, 337, 339, 342, 348, 351, 356, 359, 362,
	368, 373, 376, 379, 384, 387, 393, 396, 399, 404,
	410, 413, 415, 421, 424, 430, 432, 438, 441, 446,
	449, 452, 458, 461, 466, 469, 475, 478, 483, 486,
	492, 494, 497, 503, 506, 511, 514, 517, 523, 526,
	531, 534, 537, 543, 546, 551, 554, 557, 563, 566,
	571, 574, 577, 583, 586, 591, 594, 600, 603, 606,
	611, 614, 617, 623, 626, 628, 634, 637, 643, 646,
	649, 654, 657, 663, 666, 669, 674, 677, 683, 686,
	691, 694, 697, 703, 706, 711, 714, 717, 720, 726,
	729, 734, 737, 740, 746, 749, 754, 757, 763, 766,
	769, 774, 777, 783, 786, 789, 794, 797, 803, 806,
	809, 814, 817, 820, 826, 829, 831, 837, 840, 846,
	849, 854, 857, 860, 866, 869, 874, 877, 880, 886,
	889, 894, 897, 900, 906, 909, 914, 917, 920, 926,
	929, 932, 937, 940, 946, 949, 952, 957, 960, 966,
	969, 972, 977, 980, 986, 989, 992, 997, 1000, 1006,
	1009, 1012, 1017, 1020, 1023, 0, 1, 2, 3, 4,
	4, 5, 6, 7, 8, 9, 10, 10, 11, 12,
	13, 14, 15, 16, 17, 17, 18, 19, 20, 21,
	22, 23, 23, 24, 25, 26, 27, 28, 29, 29,
	30, 31, 32, 33, 34, 35, 36, 36, 37, 38,
	39, 40, 41, 42, 43, 43, 44, 45, 46, 47,
	48, 49, 49, 50, 51, 52, 53, 54, 55, 56,
	57, 57, 58, 59, 60, 61, 62, 63, 63, 64,
	65, 66, 67, 68, 69, 70, 70, 71, 72, 73,
	74, 75, 76, 76, 77, 78, 79, 80, 81, 82,
	83, 83, 84, 85, 86,
};

static u8 elvss_table[EXTEND_BRIGHTNESS + 1] = {
	[0 ... 255] = 0xD0,
	[256 ... 260] = 0xDD,
	[260 ... 265] = 0xDC,
	[266 ... 271] = 0xDB,
	[272 ... 277] = 0xDA,
	[278 ... 285] = 0xD9,
	[286 ... 295] = 0xD8,
	[296 ... 304] = 0xD7,
	[305 ... 311] = 0xD6,
	[312 ... 317] = 0xD5,
	[318 ... 325] = 0xD4,
	[326 ... 331] = 0xD3,
	[332 ... 337] = 0xD2,
	[338 ... 345] = 0xD1,
	[346 ... EXTEND_BRIGHTNESS] = 0xD0,
};
#endif /* __S6E8FC0_PARAM_H__ */
