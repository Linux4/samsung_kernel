#ifndef __DYNAMIC_AID_XXXX_H
#define __DYNAMIC_AID_XXXX_H __FILE__

#include "dynamic_aid.h"
#include "dynamic_aid_gamma_curve.h"

enum {
	IV_VT,
	IV_3,
	IV_11,
	IV_23,
	IV_35,
	IV_51,
	IV_87,
	IV_151,
	IV_203,
	IV_255,
	IV_MAX
};

enum {
	IBRIGHTNESS_5NT,
	IBRIGHTNESS_6NT,
	IBRIGHTNESS_7NT,
	IBRIGHTNESS_8NT,
	IBRIGHTNESS_9NT,
	IBRIGHTNESS_10NT,
	IBRIGHTNESS_11NT,
	IBRIGHTNESS_12NT,
	IBRIGHTNESS_13NT,
	IBRIGHTNESS_14NT,
	IBRIGHTNESS_15NT,
	IBRIGHTNESS_16NT,
	IBRIGHTNESS_17NT,
	IBRIGHTNESS_19NT,
	IBRIGHTNESS_20NT,
	IBRIGHTNESS_21NT,
	IBRIGHTNESS_22NT,
	IBRIGHTNESS_24NT,
	IBRIGHTNESS_25NT,
	IBRIGHTNESS_27NT,
	IBRIGHTNESS_29NT,
	IBRIGHTNESS_30NT,
	IBRIGHTNESS_32NT,
	IBRIGHTNESS_34NT,
	IBRIGHTNESS_37NT,
	IBRIGHTNESS_39NT,
	IBRIGHTNESS_41NT,
	IBRIGHTNESS_44NT,
	IBRIGHTNESS_47NT,
	IBRIGHTNESS_50NT,
	IBRIGHTNESS_53NT,
	IBRIGHTNESS_56NT,
	IBRIGHTNESS_60NT,
	IBRIGHTNESS_64NT,
	IBRIGHTNESS_68NT,
	IBRIGHTNESS_72NT,
	IBRIGHTNESS_77NT,
	IBRIGHTNESS_82NT,
	IBRIGHTNESS_87NT,
	IBRIGHTNESS_93NT,
	IBRIGHTNESS_98NT,
	IBRIGHTNESS_105NT,
	IBRIGHTNESS_106NT,
	IBRIGHTNESS_107NT,
	IBRIGHTNESS_108NT,
	IBRIGHTNESS_109NT,
	IBRIGHTNESS_110NT,
	IBRIGHTNESS_111NT,
	IBRIGHTNESS_119NT,
	IBRIGHTNESS_126NT,
	IBRIGHTNESS_134NT,
	IBRIGHTNESS_143NT,
	IBRIGHTNESS_152NT,
	IBRIGHTNESS_162NT,
	IBRIGHTNESS_172NT,
	IBRIGHTNESS_173NT,
	IBRIGHTNESS_175NT,
	IBRIGHTNESS_177NT,
	IBRIGHTNESS_179NT,
	IBRIGHTNESS_181NT,
	IBRIGHTNESS_183NT,
	IBRIGHTNESS_195NT,
	IBRIGHTNESS_207NT,
	IBRIGHTNESS_220NT,
	IBRIGHTNESS_234NT,
	IBRIGHTNESS_249NT,
	IBRIGHTNESS_265NT,
	IBRIGHTNESS_282NT,
	IBRIGHTNESS_300NT,
	IBRIGHTNESS_316NT,
	IBRIGHTNESS_333NT,
	IBRIGHTNESS_350NT,
	IBRIGHTNESS_500NT,
	IBRIGHTNESS_MAX
};

#define VREG_OUT_X1000		5800	/* VREG_OUT x 1000 */

static const int index_voltage_table[IBRIGHTNESS_MAX] = {
	0,		/* IV_VT */
	3,		/* IV_3 */
	11,		/* IV_11 */
	23,		/* IV_23 */
	35,		/* IV_35 */
	51,		/* IV_51 */
	87,		/* IV_87 */
	151,		/* IV_151 */
	203,		/* IV_203 */
	255		/* IV_255 */
};

static const int index_brightness_table[IBRIGHTNESS_MAX] = {
	5,	/* IBRIGHTNESS_5NT */
	6,	/* IBRIGHTNESS_6NT */
	7,	/* IBRIGHTNESS_7NT */
	8,	/* IBRIGHTNESS_8NT */
	9,	/* IBRIGHTNESS_9NT */
	10,	/* IBRIGHTNESS_10NT */
	11,	/* IBRIGHTNESS_11NT */
	12,	/* IBRIGHTNESS_12NT */
	13,	/* IBRIGHTNESS_13NT */
	14,	/* IBRIGHTNESS_14NT */
	15,	/* IBRIGHTNESS_15NT */
	16,	/* IBRIGHTNESS_16NT */
	17,	/* IBRIGHTNESS_17NT */
	19,	/* IBRIGHTNESS_19NT */
	20,	/* IBRIGHTNESS_20NT */
	21,	/* IBRIGHTNESS_21NT */
	22,	/* IBRIGHTNESS_22NT */
	24,	/* IBRIGHTNESS_24NT */
	25,	/* IBRIGHTNESS_25NT */
	27,	/* IBRIGHTNESS_27NT */
	29,	/* IBRIGHTNESS_29NT */
	30,	/* IBRIGHTNESS_30NT */
	32,	/* IBRIGHTNESS_32NT */
	34,	/* IBRIGHTNESS_34NT */
	37,	/* IBRIGHTNESS_37NT */
	39,	/* IBRIGHTNESS_39NT */
	41,	/* IBRIGHTNESS_41NT */
	44,	/* IBRIGHTNESS_44NT */
	47,	/* IBRIGHTNESS_47NT */
	50,	/* IBRIGHTNESS_50NT */
	53,	/* IBRIGHTNESS_53NT */
	56,	/* IBRIGHTNESS_56NT */
	60,	/* IBRIGHTNESS_60NT */
	64,	/* IBRIGHTNESS_64NT */
	68,	/* IBRIGHTNESS_68NT */
	72,	/* IBRIGHTNESS_72NT */
	77,	/* IBRIGHTNESS_77NT */
	82,	/* IBRIGHTNESS_82NT */
	87,	/* IBRIGHTNESS_87NT */
	93,	/* IBRIGHTNESS_93NT */
	98,	/* IBRIGHTNESS_98NT */
	105,	/* IBRIGHTNESS_105NT */
	106,	/* IBRIGHTNESS_106NT */
	107,	/* IBRIGHTNESS_107NT */
	108,	/* IBRIGHTNESS_108NT */
	109,	/* IBRIGHTNESS_109NT */
	110,	/* IBRIGHTNESS_110NT */
	111,	/* IBRIGHTNESS_111NT */
	119,	/* IBRIGHTNESS_119NT */
	126,	/* IBRIGHTNESS_126NT */
	134,	/* IBRIGHTNESS_134NT */
	143,	/* IBRIGHTNESS_143NT */
	152,	/* IBRIGHTNESS_152NT */
	162,	/* IBRIGHTNESS_162NT */
	172,	/* IBRIGHTNESS_172NT */
	173,	/* IBRIGHTNESS_173NT */
	175,	/* IBRIGHTNESS_175NT */
	177,	/* IBRIGHTNESS_177NT */
	179,	/* IBRIGHTNESS_179NT */
	181,	/* IBRIGHTNESS_181NT */
	183,	/* IBRIGHTNESS_183NT */
	195,	/* IBRIGHTNESS_195NT */
	207,	/* IBRIGHTNESS_207NT */
	220,	/* IBRIGHTNESS_220NT */
	234,	/* IBRIGHTNESS_234NT */
	249,	/* IBRIGHTNESS_249NT */
	265,	/* IBRIGHTNESS_265NT */
	282,	/* IBRIGHTNESS_282NT */
	300,	/* IBRIGHTNESS_300NT */
	316,	/* IBRIGHTNESS_316NT */
	333,	/* IBRIGHTNESS_333NT */
	350,	/* IBRIGHTNESS_350NT */
	350	/* IBRIGHTNESS_500NT */
};

static const int gamma_default_0[IV_MAX*CI_MAX] = {
	0x00, 0x00, 0x00,	/* IV_VT */
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x100, 0x100, 0x100	/* IV_255 */
};

static const int *gamma_default = gamma_default_0;

static const struct formular_t gamma_formula[IV_MAX] = {
	{0, 860},	/* IV_VT */
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{46, 605}	/* IV_255 */
};

static const int vt_voltage_value[] = {
	0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 138, 148, 158, 168, 178, 186
};

static const int brightness_base_table[IBRIGHTNESS_MAX] = {
	110,	/* IBRIGHTNESS_5NT */
	110,	/* IBRIGHTNESS_6NT */
	110,	/* IBRIGHTNESS_7NT */
	110,	/* IBRIGHTNESS_8NT */
	110,	/* IBRIGHTNESS_9NT */
	110,	/* IBRIGHTNESS_10NT */
	110,	/* IBRIGHTNESS_11NT */
	110,	/* IBRIGHTNESS_12NT */
	110,	/* IBRIGHTNESS_13NT */
	110,	/* IBRIGHTNESS_14NT */
	110,	/* IBRIGHTNESS_15NT */
	110,	/* IBRIGHTNESS_16NT */
	110,	/* IBRIGHTNESS_17NT */
	110,	/* IBRIGHTNESS_19NT */
	110,	/* IBRIGHTNESS_20NT */
	110,	/* IBRIGHTNESS_21NT */
	110,	/* IBRIGHTNESS_22NT */
	110,	/* IBRIGHTNESS_24NT */
	110,	/* IBRIGHTNESS_25NT */
	110,	/* IBRIGHTNESS_27NT */
	110,	/* IBRIGHTNESS_29NT */
	110,	/* IBRIGHTNESS_30NT */
	110,	/* IBRIGHTNESS_32NT */
	110,	/* IBRIGHTNESS_34NT */
	110,	/* IBRIGHTNESS_37NT */
	110,	/* IBRIGHTNESS_39NT */
	110,	/* IBRIGHTNESS_41NT */
	110,	/* IBRIGHTNESS_44NT */
	110,	/* IBRIGHTNESS_47NT */
	110,	/* IBRIGHTNESS_50NT */
	110,	/* IBRIGHTNESS_53NT */
	110,	/* IBRIGHTNESS_56NT */
	110,	/* IBRIGHTNESS_60NT */
	110,	/* IBRIGHTNESS_64NT */
	110,	/* IBRIGHTNESS_68NT */
	110,	/* IBRIGHTNESS_72NT */
	110,	/* IBRIGHTNESS_77NT */
	110,	/* IBRIGHTNESS_82NT */
	110,	/* IBRIGHTNESS_87NT */
	110,	/* IBRIGHTNESS_93NT */
	110,	/* IBRIGHTNESS_98NT */
	110,	/* IBRIGHTNESS_105NT */
	122,	/* IBRIGHTNESS_106NT */
	134,	/* IBRIGHTNESS_107NT */
	147,	/* IBRIGHTNESS_108NT */
	159,	/* IBRIGHTNESS_109NT */
	171,	/* IBRIGHTNESS_110NT */
	185,	/* IBRIGHTNESS_111NT */
	198,	/* IBRIGHTNESS_119NT */
	209,	/* IBRIGHTNESS_126NT */
	221,	/* IBRIGHTNESS_134NT */
	233,	/* IBRIGHTNESS_143NT */
	248,	/* IBRIGHTNESS_152NT */
	264,	/* IBRIGHTNESS_162NT */
	274,	/* IBRIGHTNESS_172NT */
	258,	/* IBRIGHTNESS_173NT */
	243,	/* IBRIGHTNESS_175NT */
	228,	/* IBRIGHTNESS_177NT */
	213,	/* IBRIGHTNESS_179NT */
	198,	/* IBRIGHTNESS_181NT */
	183,	/* IBRIGHTNESS_183NT */
	195,	/* IBRIGHTNESS_195NT */
	207,	/* IBRIGHTNESS_207NT */
	220,	/* IBRIGHTNESS_220NT */
	234,	/* IBRIGHTNESS_234NT */
	249,	/* IBRIGHTNESS_249NT */
	265,	/* IBRIGHTNESS_265NT */
	282,	/* IBRIGHTNESS_282NT */
	300,	/* IBRIGHTNESS_300NT */
	316,	/* IBRIGHTNESS_316NT */
	333,	/* IBRIGHTNESS_333NT */
	350,	/* IBRIGHTNESS_350NT */
	350	/* IBRIGHTNESS_500NT */
};

static const int *gamma_curve_tables[IBRIGHTNESS_MAX] = {
	gamma_curve_2p15_table,	/* IBRIGHTNESS_5NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_6NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_7NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_8NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_9NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_10NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_11NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_12NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_13NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_14NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_15NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_16NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_17NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_19NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_20NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_21NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_22NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_24NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_25NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_27NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_29NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_30NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_32NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_34NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_37NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_39NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_41NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_44NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_47NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_50NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_53NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_56NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_60NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_64NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_68NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_72NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_77NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_82NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_87NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_93NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_98NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_105NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_106NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_107NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_108NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_109NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_110NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_111NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_119NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_126NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_134NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_143NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_152NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_162NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_172NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_173NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_175NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_177NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_179NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_181NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_183NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_195NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_207NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_220NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_234NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_249NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_265NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_282NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_300NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_316NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_333NT */
	gamma_curve_2p20_table,	/* IBRIGHTNESS_350NT */
	gamma_curve_2p20_table	/* IBRIGHTNESS_500NT */
};

static const int *gamma_curve_lut = gamma_curve_2p20_table;

static const unsigned char aor_cmd[IBRIGHTNESS_MAX][2] = {
	{0x04, 0xD5},		/* IBRIGHTNESS_5NT */
	{0x04, 0xC8},		/* IBRIGHTNESS_6NT */
	{0x04, 0xC3},		/* IBRIGHTNESS_7NT */
	{0x04, 0xB5},		/* IBRIGHTNESS_8NT */
	{0x04, 0xA7},		/* IBRIGHTNESS_9NT */
	{0x04, 0xA1},		/* IBRIGHTNESS_10NT */
	{0x04, 0x94},		/* IBRIGHTNESS_11NT */
	{0x04, 0x87},		/* IBRIGHTNESS_12NT */
	{0x04, 0x81},		/* IBRIGHTNESS_13NT */
	{0x04, 0x73},		/* IBRIGHTNESS_14NT */
	{0x04, 0x65},		/* IBRIGHTNESS_15NT */
	{0x04, 0x57},		/* IBRIGHTNESS_16NT */
	{0x04, 0x51},		/* IBRIGHTNESS_17NT */
	{0x04, 0x35},		/* IBRIGHTNESS_19NT */
	{0x04, 0x29},		/* IBRIGHTNESS_20NT */
	{0x04, 0x22},		/* IBRIGHTNESS_21NT */
	{0x04, 0x15},		/* IBRIGHTNESS_22NT */
	{0x04, 0x00},		/* IBRIGHTNESS_24NT */
	{0x03, 0xF2},		/* IBRIGHTNESS_25NT */
	{0x03, 0xD6},		/* IBRIGHTNESS_27NT */
	{0x03, 0xC3},		/* IBRIGHTNESS_29NT */
	{0x03, 0xB5},		/* IBRIGHTNESS_30NT */
	{0x03, 0xA1},		/* IBRIGHTNESS_32NT */
	{0x03, 0x85},		/* IBRIGHTNESS_34NT */
	{0x03, 0x63},		/* IBRIGHTNESS_37NT */
	{0x03, 0x46},		/* IBRIGHTNESS_39NT */
	{0x03, 0x32},		/* IBRIGHTNESS_41NT */
	{0x03, 0x08},		/* IBRIGHTNESS_44NT */
	{0x02, 0xE6},		/* IBRIGHTNESS_47NT */
	{0x02, 0xC3},		/* IBRIGHTNESS_50NT */
	{0x02, 0xA0},		/* IBRIGHTNESS_53NT */
	{0x02, 0x76},		/* IBRIGHTNESS_56NT */
	{0x02, 0x45},		/* IBRIGHTNESS_60NT */
	{0x02, 0x13},		/* IBRIGHTNESS_64NT */
	{0x01, 0xE2},		/* IBRIGHTNESS_68NT */
	{0x01, 0xA8},		/* IBRIGHTNESS_72NT */
	{0x01, 0x67},		/* IBRIGHTNESS_77NT */
	{0x01, 0x27},		/* IBRIGHTNESS_82NT */
	{0x00, 0xE6},		/* IBRIGHTNESS_87NT */
	{0x00, 0x96},		/* IBRIGHTNESS_93NT */
	{0x00, 0x55},		/* IBRIGHTNESS_98NT */
	{0x00, 0x06},		/* IBRIGHTNESS_105NT */
	{0x00, 0x71},		/* IBRIGHTNESS_106NT */
	{0x00, 0xE5},		/* IBRIGHTNESS_107NT */
	{0x01, 0x51},		/* IBRIGHTNESS_108NT */
	{0x01, 0x93},		/* IBRIGHTNESS_109NT */
	{0x01, 0xD3},		/* IBRIGHTNESS_110NT */
	{0x02, 0x06},		/* IBRIGHTNESS_111NT */
	{0x02, 0x06},		/* IBRIGHTNESS_119NT */
	{0x02, 0x06},		/* IBRIGHTNESS_126NT */
	{0x02, 0x06},		/* IBRIGHTNESS_134NT */
	{0x02, 0x06},		/* IBRIGHTNESS_143NT */
	{0x02, 0x06},		/* IBRIGHTNESS_152NT */
	{0x02, 0x06},		/* IBRIGHTNESS_162NT */
	{0x02, 0x06},		/* IBRIGHTNESS_172NT */
	{0x01, 0xD1},		/* IBRIGHTNESS_173NT */
	{0x01, 0x72},		/* IBRIGHTNESS_175NT */
	{0x01, 0x17},		/* IBRIGHTNESS_177NT */
	{0x00, 0xA3},		/* IBRIGHTNESS_179NT */
	{0x00, 0x44},		/* IBRIGHTNESS_181NT */
	{0x00, 0x06},		/* IBRIGHTNESS_183NT */
	{0x00, 0x06},		/* IBRIGHTNESS_195NT */
	{0x00, 0x06},		/* IBRIGHTNESS_207NT */
	{0x00, 0x06},		/* IBRIGHTNESS_220NT */
	{0x00, 0x06},		/* IBRIGHTNESS_234NT */
	{0x00, 0x06},		/* IBRIGHTNESS_249NT */
	{0x00, 0x06},		/* IBRIGHTNESS_265NT */
	{0x00, 0x06},		/* IBRIGHTNESS_282NT */
	{0x00, 0x06},		/* IBRIGHTNESS_300NT */
	{0x00, 0x06},		/* IBRIGHTNESS_316NT */
	{0x00, 0x06},		/* IBRIGHTNESS_333NT */
	{0x00, 0x06},		/* IBRIGHTNESS_350NT */
	{0x00, 0x06}		/* IBRIGHTNESS_500NT */

};

static const int offset_gradation[IBRIGHTNESS_MAX][IV_MAX] = {	/* V0 ~ V255 */
	{0, 28, 28, 28, 24, 22, 17, 10, 5, 0},   /* IBRIGHTNESS_5NT */
	{0, 27, 27, 27, 23, 21, 16, 10, 5, 0},   /* IBRIGHTNESS_6NT */
	{0, 26, 26, 26, 22, 20, 15, 9, 5, 0},    /* IBRIGHTNESS_7NT */
	{0, 23, 23, 21, 18, 17, 13, 8, 4, 0},    /* IBRIGHTNESS_8NT */
	{0, 22, 22, 20, 17, 16, 12, 7, 4, 0},    /* IBRIGHTNESS_9NT */
	{0, 20, 20, 18, 15, 14, 11, 6, 4, 0},    /* IBRIGHTNESS_10NT */
	{0, 20, 20, 18, 15, 14, 11, 6, 4, 0},    /* IBRIGHTNESS_11NT */
	{0, 19, 19, 17, 14, 13, 10, 5, 4, 0},    /* IBRIGHTNESS_12NT */
	{0, 18, 18, 16, 13, 12, 9, 5, 3, 0},     /* IBRIGHTNESS_13NT */
	{0, 16, 16, 14, 12, 11, 8, 4, 3, 0},     /* IBRIGHTNESS_14NT */
	{0, 15, 15, 13, 11, 10, 7, 4, 3, 0},     /* IBRIGHTNESS_15NT */
	{0, 15, 15, 13, 11, 10, 7, 4, 3, 0},     /* IBRIGHTNESS_16NT */
	{0, 15, 15, 13, 11, 10, 7, 4, 3, 0},     /* IBRIGHTNESS_17NT */
	{0, 14, 14, 12, 10, 9, 6, 4, 3, 0},      /* IBRIGHTNESS_19NT */
	{0, 14, 14, 12, 10, 9, 6, 3, 3, 0},      /* IBRIGHTNESS_20NT */
	{0, 12, 12, 10, 8, 8, 5, 3, 2, 0},       /* IBRIGHTNESS_21NT */
	{0, 12, 12, 10, 8, 8, 5, 2, 2, 0},       /* IBRIGHTNESS_22NT */
	{0, 11, 11, 9, 7, 7, 4, 2, 2, 0},        /* IBRIGHTNESS_24NT */
	{0, 11, 11, 9, 7, 7, 4, 2, 2, 0},        /* IBRIGHTNESS_25NT */
	{0, 10, 10, 8, 7, 7, 4, 2, 2, 0},        /* IBRIGHTNESS_27NT */
	{0, 9, 9, 7, 7, 6, 4, 2, 2, 0},          /* IBRIGHTNESS_29NT */
	{0, 9, 9, 7, 7, 6, 4, 2, 2, 0},          /* IBRIGHTNESS_30NT */
	{0, 8, 8, 7, 6, 5, 3, 2, 2, 0},          /* IBRIGHTNESS_32NT */
	{0, 8, 8, 7, 6, 5, 3, 2, 2, 0},          /* IBRIGHTNESS_34NT */
	{0, 7, 7, 6, 5, 4, 3, 2, 2, 0},          /* IBRIGHTNESS_37NT */
	{0, 7, 7, 6, 5, 4, 3, 2, 2, 0},          /* IBRIGHTNESS_39NT */
	{0, 2, 2, 2, 0, 1, 1, 2, 2, 0},          /* IBRIGHTNESS_41NT */
	{0, 2, 2, 2, 0, 1, 1, 2, 2, 0},          /* IBRIGHTNESS_44NT */
	{0, 2, 2, 2, 0, 1, 1, 2, 2, 0},          /* IBRIGHTNESS_47NT */
	{0, 2, 2, 2, 0, 1, 1, 2, 2, 0},          /* IBRIGHTNESS_50NT */
	{0, 1, 1, 1, 0, 1, 0, 1, 1, 0},          /* IBRIGHTNESS_53NT */
	{0, 1, 1, 1, 0, 1, 0, 1, 1, 0},          /* IBRIGHTNESS_56NT */
	{0, 1, 1, 1, 0, 1, 0, 1, 1, 0},          /* IBRIGHTNESS_60NT */
	{0, 1, 1, 1, 0, 1, 0, 1, 1, 0},          /* IBRIGHTNESS_64NT */
	{0, 1, 1, 1, 0, 1, 0, 1, 1, 0},          /* IBRIGHTNESS_68NT */
	{0, 1, 1, 1, 0, 1, 0, 1, 1, 0},          /* IBRIGHTNESS_72NT */
	{0, 1, 1, 1, 0, 1, 0, 1, 1, 0},          /* IBRIGHTNESS_77NT */
	{0, 1, 1, 1, 0, 1, 0, 1, 1, 0},          /* IBRIGHTNESS_82NT */
	{0, 1, 1, 1, 0, 1, 0, 1, 1, 0},          /* IBRIGHTNESS_87NT */
	{0, 0, 0, 0, 0, 1, 0, 1, 1, 0},          /* IBRIGHTNESS_93NT */
	{0, 0, 0, 0, 0, 1, 0, 1, 1, 0},          /* IBRIGHTNESS_98NT */
	{0, 1, 1, 1, 0, 1, 0, 1, 1, 0},          /* IBRIGHTNESS_105NT */
	{0, 4, 2, 1, 1, 1, 1, 1, 1, 0},          /* IBRIGHTNESS_106NT */
	{0, 5, 3, 2, 2, 1, 1, 1, 1, 0},          /* IBRIGHTNESS_107NT */
	{0, 5, 3, 3, 2, 1, 1, 1, 1, 0},          /* IBRIGHTNESS_108NT */
	{0, 7, 4, 3, 2, 1, 1, 1, 0, 0},          /* IBRIGHTNESS_109NT */
	{0, 7, 4, 3, 3, 2, 2, 2, 1, 0},          /* IBRIGHTNESS_110NT */
	{0, 1, 1, 1, 0, 1, 0, 1, 1, 0},          /* IBRIGHTNESS_111NT */
	{0, 1, 1, 2, 1, 1, 1, 2, 1, 0},          /* IBRIGHTNESS_119NT */
	{0, 1, 1, 2, 2, 1, 1, 2, 1, 0},          /* IBRIGHTNESS_126NT */
	{0, 1, 1, 2, 2, 1, 2, 2, 1, 0},          /* IBRIGHTNESS_134NT */
	{0, 1, 1, 2, 2, 2, 2, 3, 1, 0},          /* IBRIGHTNESS_143NT */
	{0, 1, 1, 2, 2, 2, 2, 3, 1, 0},          /* IBRIGHTNESS_152NT */
	{0, 1, 1, 2, 2, 2, 2, 3, 1, 0},          /* IBRIGHTNESS_162NT */
	{0, 1, 1, 2, 2, 2, 2, 3, 1, 0},          /* IBRIGHTNESS_172NT */
	{0, 2, 1, 2, 2, 2, 2, 2, 1, 0},          /* IBRIGHTNESS_173NT */
	{0, 2, 1, 2, 2, 2, 2, 2, 1, 0},          /* IBRIGHTNESS_175NT */
	{0, 2, 1, 2, 2, 2, 2, 2, 1, 0},          /* IBRIGHTNESS_177NT */
	{0, 2, 1, 1, 1, 1, 2, 2, 1, 0},          /* IBRIGHTNESS_179NT */
	{0, 2, 1, 1, 1, 2, 2, 2, 1, 0},          /* IBRIGHTNESS_181NT */
	{0, 1, 1, 1, 2, 2, 3, 4, 4, 3},          /* IBRIGHTNESS_183NT */
	{0, 1, 1, 1, 2, 2, 3, 4, 4, 3},          /* IBRIGHTNESS_195NT */
	{0, 1, 1, 1, 2, 2, 3, 4, 4, 3},          /* IBRIGHTNESS_207NT */
	{0, 1, 1, 1, 2, 2, 3, 3, 3, 2},          /* IBRIGHTNESS_220NT */
	{0, 1, 1, 1, 2, 2, 3, 3, 3, 2},          /* IBRIGHTNESS_234NT */
	{0, 1, 1, 1, 2, 2, 2, 2, 3, 2},          /* IBRIGHTNESS_249NT */
	{0, 1, 1, 1, 1, 1, 1, 2, 2, 1},          /* IBRIGHTNESS_265NT */
	{0, 1, 1, 0, 1, 1, 1, 2, 2, 1},          /* IBRIGHTNESS_282NT */
	{0, 1, 1, 0, 1, 1, 1, 1, 1, 1},          /* IBRIGHTNESS_300NT */
	{0, 1, 1, 0, 0, 0, 0, 1, 1, 1},          /* IBRIGHTNESS_316NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1},          /* IBRIGHTNESS_333NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 0},          /* IBRIGHTNESS_350NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}           /* IBRIGHTNESS_500NT */
};

static const int offset_color[IBRIGHTNESS_MAX][CI_MAX * IV_MAX] = {	/* V0 ~ V255 */
	{0, 0, 0, 0, 0, 0, -10, 5, -11, 8, 3, -9, -10, 2, -11, -11, 9, -13, -11, 5, -12, -2, 2, -5, -2, 0, -3, -2, 0, -2},   /* IBRIGHTNESS_5NT */
	{0, 0, 0, 0, 0, 0, -9, 5, -11, 7, 3, -9, -9, 3, -11, -11, 9, -13, -10, 5, -11, -2, 2, -5, -2, 0, -3, -2, 0, -2},     /* IBRIGHTNESS_6NT */
	{0, 0, 0, 0, 0, 0, -8, 5, -11, 7, 3, -10, -7, 4, -10, -12, 8, -13, -9, 4, -10, -2, 2, -5, -2, 0, -3, -2, 0, -2},     /* IBRIGHTNESS_7NT */
	{0, 0, 0, 0, 0, 0, -6, 6, -10, -1, 5, -11, -6, 5, -9, -12, 8, -12, -8, 4, -9, -1, 1, -4, -1, 0, -2, -1, 0, -1},      /* IBRIGHTNESS_8NT */
	{0, 0, 0, 0, 0, 0, -4, 6, -10, -1, 5, -11, -5, 6, -8, -13, 7, -12, -7, 3, -8, -1, 1, -3, -1, 0, -2, -1, 0, -1},      /* IBRIGHTNESS_9NT */
	{0, 0, 0, 0, 0, 0, -3, 6, -10, -2, 5, -12, -4, 7, -7, -13, 7, -12, -5, 3, -7, -1, 1, -3, -1, 0, -2, -1, 0, -1},      /* IBRIGHTNESS_10NT */
	{0, 0, 0, 0, 0, 0, -3, 6, -11, -3, 5, -12, -5, 7, -7, -13, 7, -12, -5, 3, -7, -1, 1, -3, -1, 0, -2, -1, 0, -1},      /* IBRIGHTNESS_11NT */
	{0, 0, 0, 0, 0, 0, -4, 6, -13, -4, 5, -12, -6, 6, -7, -12, 7, -12, -5, 3, -7, -1, 1, -3, -1, 0, -2, -1, 0, -1},      /* IBRIGHTNESS_12NT */
	{0, 0, 0, 0, 0, 0, -4, 6, -13, -5, 6, -13, -6, 6, -7, -11, 6, -11, -5, 3, -7, 0, 1, -2, -1, 0, -2, -1, 0, -1},       /* IBRIGHTNESS_13NT */
	{0, 0, 0, 0, 0, 0, -5, 7, -15, -6, 6, -13, -7, 5, -6, -10, 6, -11, -4, 2, -8, 0, 1, -2, 0, 0, -1, 0, 0, 0},          /* IBRIGHTNESS_14NT */
	{0, 0, 0, 0, 0, 0, -6, 8, -17, -6, 6, -13, -8, 4, -6, -9, 6, -11, -4, 2, -8, 0, 1, -2, 0, 0, -1, 0, 0, 0},           /* IBRIGHTNESS_15NT */
	{0, 0, 0, 0, 0, 0, -7, 8, -19, -7, 6, -13, -8, 4, -6, -9, 6, -11, -4, 2, -8, 0, 1, -2, 0, 0, -1, 0, 0, 0},           /* IBRIGHTNESS_16NT */
	{0, 0, 0, 0, 0, 0, -5, 8, -19, -7, 6, -13, -8, 4, -6, -9, 6, -11, -4, 2, -8, 0, 1, -2, 0, 0, -1, 0, 0, 0},           /* IBRIGHTNESS_17NT */
	{0, 0, 0, 0, 0, 0, -4, 8, -19, -7, 6, -12, -8, 4, -6, -9, 6, -11, -3, 2, -7, 0, 1, -2, 0, 0, -1, 0, 0, 0},           /* IBRIGHTNESS_19NT */
	{0, 0, 0, 0, 0, 0, -3, 7, -19, -7, 6, -12, -9, 4, -5, -9, 6, -11, -3, 2, -7, 0, 1, -1, 0, 0, -1, 0, 0, 0},           /* IBRIGHTNESS_20NT */
	{0, 0, 0, 0, 0, 0, -2, 7, -19, -6, 5, -11, -9, 3, -5, -8, 5, -12, -2, 2, -6, 1, 1, -1, -1, 0, -1, 0, 0, 0},          /* IBRIGHTNESS_21NT */
	{0, 0, 0, 0, 0, 0, -1, 7, -19, -6, 5, -11, -10, 3, -4, -8, 5, -12, -2, 2, -5, 1, 1, 0, -1, 0, -1, 0, 0, 0},          /* IBRIGHTNESS_22NT */
	{0, 0, 0, 0, 0, 0, 0, 7, -19, -6, 5, -10, -10, 3, -4, -8, 5, -12, -1, 2, -4, 1, 1, 0, -1, 0, -1, 0, 0, 0},           /* IBRIGHTNESS_24NT */
	{0, 0, 0, 0, 0, 0, 0, 7, -19, -6, 5, -10, -10, 3, -4, -8, 5, -12, -1, 2, -4, 1, 1, 0, -1, 0, -1, 0, 0, 0},           /* IBRIGHTNESS_25NT */
	{0, 0, 0, 0, 0, 0, 0, 7, -19, -6, 5, -9, -10, 3, -4, -8, 5, -12, -1, 2, -4, 1, 1, 0, -1, 0, -1, 0, 0, 0},            /* IBRIGHTNESS_27NT */
	{0, 0, 0, 0, 0, 0, -1, 7, -20, -5, 5, -9, -9, 3, -4, -8, 5, -11, 0, 2, -4, 1, 1, 0, -1, 0, -1, 0, 0, 0},             /* IBRIGHTNESS_29NT */
	{0, 0, 0, 0, 0, 0, -1, 7, -20, -4, 5, -8, -8, 3, -4, -8, 5, -10, 0, 2, -3, 1, 1, 0, -1, 0, -1, 0, 0, 0},             /* IBRIGHTNESS_30NT */
	{0, 0, 0, 0, 0, 0, -2, 6, -21, -3, 4, -8, -7, 3, -3, -7, 4, -9, 1, 1, -3, 1, 1, 0, 0, 0, 0, 0, 0, 0},                /* IBRIGHTNESS_32NT */
	{0, 0, 0, 0, 0, 0, -2, 6, -21, -3, 4, -7, -6, 3, -3, -7, 4, -8, 1, 1, -3, 1, 1, 0, 0, 0, 0, 0, 0, 0},                /* IBRIGHTNESS_34NT */
	{0, 0, 0, 0, 0, 0, -3, 6, -22, -2, 4, -7, -5, 3, -3, -7, 4, -7, 2, 1, -2, 1, 1, 0, 0, 0, 0, 0, 0, 0},                /* IBRIGHTNESS_37NT */
	{0, 0, 0, 0, 0, 0, -3, 6, -22, -2, 4, -7, -5, 3, -3, -7, 4, -7, 2, 1, -2, 1, 1, 0, 0, 0, 0, 0, 0, 0},                /* IBRIGHTNESS_39NT */
	{0, 0, 0, 0, 0, 0, -18, 6, -3, -2, 0, 0, -7, 1, -2, 4, 3, -7, -4, 0, -1, -2, 0, -1, 0, 0, 0, 0, 0, 0},               /* IBRIGHTNESS_41NT */
	{0, 0, 0, 0, 0, 0, -17, 6, -4, -1, 0, 0, -7, 1, -1, 5, 3, -7, -4, 0, -1, -2, 0, -1, 0, 0, 0, 0, 0, 0},               /* IBRIGHTNESS_44NT */
	{0, 0, 0, 0, 0, 0, -16, 5, -5, 0, 0, 0, -7, 1, -1, 5, 3, -7, -4, 0, -1, -2, 0, -1, 0, 0, 0, 0, 0, 0},                /* IBRIGHTNESS_47NT */
	{0, 0, 0, 0, 0, 0, -14, 5, -5, 0, 0, 0, -6, 1, -1, 5, 3, -7, -4, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, 0},                /* IBRIGHTNESS_50NT */
	{0, 0, 0, 0, 0, 0, -12, 4, -4, 0, 0, 1, -6, 0, -1, 4, 2, -6, -3, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0},                 /* IBRIGHTNESS_53NT */
	{0, 0, 0, 0, 0, 0, -10, 4, -4, 0, 0, 1, -5, 0, -1, 4, 2, -6, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                   /* IBRIGHTNESS_56NT */
	{0, 0, 0, 0, 0, 0, -8, 4, -4, 0, 0, 1, -5, 0, -1, 4, 2, -6, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                    /* IBRIGHTNESS_60NT */
	{0, 0, 0, 0, 0, 0, -6, 3, -4, 0, 0, 1, -4, 0, -1, 4, 2, -6, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                    /* IBRIGHTNESS_64NT */
	{0, 0, 0, 0, 0, 0, -4, 3, -4, 1, 0, 2, -4, 0, 0, 4, 2, -6, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                     /* IBRIGHTNESS_68NT */
	{0, 0, 0, 0, 0, 0, 1, 2, -5, 3, 0, 4, -3, 0, 1, 4, 2, -6, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                      /* IBRIGHTNESS_72NT */
	{0, 0, 0, 0, 0, 0, 2, 1, -5, 3, 0, 4, -3, 0, 1, 4, 2, -6, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                      /* IBRIGHTNESS_77NT */
	{0, 0, 0, 0, 0, 0, 4, -1, -6, 4, 0, 5, -2, 0, 2, 5, 2, -5, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                     /* IBRIGHTNESS_82NT */
	{0, 0, 0, 0, 0, 0, 5, -1, -6, 4, 0, 5, -2, 0, 2, 5, 2, -5, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                     /* IBRIGHTNESS_87NT */
	{0, 0, 0, 0, 0, 0, 7, -1, -5, 5, 0, 4, -1, 0, 2, 4, 2, -4, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                     /* IBRIGHTNESS_93NT */
	{0, 0, 0, 0, 0, 0, 9, -1, -4, 5, 0, 4, -1, 0, 2, 4, 2, -4, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                     /* IBRIGHTNESS_98NT */
	{0, 0, 0, 0, 0, 0, 8, -2, -5, 5, -1, 5, -1, 0, 2, 5, 2, -4, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                    /* IBRIGHTNESS_105NT */
	{0, 0, 0, 0, 0, 0, -1, 1, -4, 0, 2, -2, -2, 0, -1, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},                   /* IBRIGHTNESS_106NT */
	{0, 0, 0, 0, 0, 0, -1, 1, -4, -1, 2, -4, -2, 0, -1, -1, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},                /* IBRIGHTNESS_107NT */
	{0, 0, 0, 0, 0, 0, -2, 2, -5, -2, 2, -5, -2, 1, -1, -1, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},                /* IBRIGHTNESS_108NT */
	{0, 0, 0, 0, 0, 0, -2, 2, -5, -3, 2, -5, -2, 1, -1, -1, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},                /* IBRIGHTNESS_109NT */
	{0, 0, 0, 0, 0, 0, -2, 2, -5, -4, 2, -5, -1, 1, -1, -2, 0, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},                /* IBRIGHTNESS_110NT */
	{0, 0, 0, 0, 0, 0, 6, 0, -5, 4, 0, 5, 0, 0, 2, 4, 2, -4, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                       /* IBRIGHTNESS_111NT */
	{0, 0, 0, 0, 0, 0, 5, 0, -5, 3, 0, 4, 0, 0, 1, 4, 0, -4, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                       /* IBRIGHTNESS_119NT */
	{0, 0, 0, 0, 0, 0, 4, 1, -4, 2, 1, 4, 1, 1, 0, 3, 0, -3, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                       /* IBRIGHTNESS_126NT */
	{0, 0, 0, 0, 0, 0, 3, 1, -4, 1, 1, 3, 1, 1, -1, 2, 0, -3, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                      /* IBRIGHTNESS_134NT */
	{0, 0, 0, 0, 0, 0, 1, 2, -4, 0, 2, 3, 2, 1, -2, 1, 0, -3, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                      /* IBRIGHTNESS_143NT */
	{0, 0, 0, 0, 0, 0, -4, 2, -5, -1, 2, 3, 2, 1, -3, 1, 0, -4, -2, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0},                   /* IBRIGHTNESS_152NT */
	{0, 0, 0, 0, 0, 0, -4, 2, -5, -1, 2, 2, 2, 1, -3, 1, 0, -3, -1, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0},                   /* IBRIGHTNESS_162NT */
	{0, 0, 0, 0, 0, 0, -4, 2, -5, -2, 2, 2, 3, 1, -3, 1, 0, -3, -1, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0},                   /* IBRIGHTNESS_172NT */
	{0, 0, 0, 0, 0, 0, -4, 2, -4, -2, 2, 1, 3, 1, -3, 0, 0, -2, -1, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0},                   /* IBRIGHTNESS_173NT */
	{0, 0, 0, 0, 0, 0, -4, 2, -4, -3, 2, 1, 4, 1, -3, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                     /* IBRIGHTNESS_175NT */
	{0, 0, 0, 0, 0, 0, -4, 2, -4, -3, 2, 0, 4, 1, -3, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                     /* IBRIGHTNESS_177NT */
	{0, 0, 0, 0, 0, 0, -4, 2, -4, -3, 1, 0, 5, 1, -3, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                     /* IBRIGHTNESS_179NT */
	{0, 0, 0, 0, 0, 0, -4, 2, -3, -4, 1, -1, 5, 1, -3, -2, 0, -1, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0},                 /* IBRIGHTNESS_181NT */
	{0, 0, 0, 0, 0, 0, 4, 0, -3, 1, 0, 5, 2, 0, -1, 1, 0, -3, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},                      /* IBRIGHTNESS_183NT */
	{0, 0, 0, 0, 0, 0, 4, 0, -3, 1, 0, 5, 2, 0, -1, 1, 0, -3, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},                      /* IBRIGHTNESS_195NT */
	{0, 0, 0, 0, 0, 0, 3, 0, -2, 0, 0, 4, 3, 0, -2, 1, 0, -2, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},                      /* IBRIGHTNESS_207NT */
	{0, 0, 0, 0, 0, 0, 3, 0, -1, 0, 0, 4, 3, 0, -3, 1, 0, -2, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},                      /* IBRIGHTNESS_220NT */
	{0, 0, 0, 0, 0, 0, 3, 0, 0, -1, 0, 3, 4, 0, -3, 1, 0, -2, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0},                      /* IBRIGHTNESS_234NT */
	{0, 0, 0, 0, 0, 0, 2, 0, 0, -1, 0, 3, 4, 0, -4, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                       /* IBRIGHTNESS_249NT */
	{0, 0, 0, 0, 0, 0, 2, 0, 0, -1, 0, 2, 5, 0, -4, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                       /* IBRIGHTNESS_265NT */
	{0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 5, 1, -3, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                        /* IBRIGHTNESS_282NT */
	{0, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 1, 5, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                         /* IBRIGHTNESS_300NT */
	{0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 6, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                          /* IBRIGHTNESS_316NT */
	{0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 6, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                          /* IBRIGHTNESS_333NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                          /* IBRIGHTNESS_350NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}                           /* IBRIGHTNESS_500NT */
};

#endif /* __DYNAMIC_AID_XXXX_H */
