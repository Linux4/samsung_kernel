#ifndef __DYNAMIC_AID_XXXX_H
#define __DYNAMIC_AID_XXXX_H __FILE__

#include "dynamic_aid.h"
#include "dynamic_aid_gamma_curve.h"

enum {
	IV_1,
	IV_15,
	IV_35,
	IV_59,
	IV_87,
	IV_171,
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
	IBRIGHTNESS_111NT,
	IBRIGHTNESS_119NT,
	IBRIGHTNESS_126NT,
	IBRIGHTNESS_134NT,
	IBRIGHTNESS_143NT,
	IBRIGHTNESS_152NT,
	IBRIGHTNESS_162NT,
	IBRIGHTNESS_172NT,
	IBRIGHTNESS_183NT,
	IBRIGHTNESS_195NT,
	IBRIGHTNESS_207NT,
	IBRIGHTNESS_220NT,
	IBRIGHTNESS_234NT,
	IBRIGHTNESS_249NT,
	IBRIGHTNESS_265NT,
	IBRIGHTNESS_282NT,
	IBRIGHTNESS_300NT,
	IBRIGHTNESS_HBM,
	IBRIGHTNESS_MAX
};

#define VREG_OUT_X1000		4713	/* VREG_OUT x 1000 */

static const int index_voltage_table[IBRIGHTNESS_MAX] = {
	1,		/* IV_1 */
	15,		/* IV_15 */
	35,		/* IV_35 */
	59,		/* IV_59 */
	87,		/* IV_87 */
	171,		/* IV_171 */
	255		/* IV_255 */
};

static const int table_v35[20] = {
	66,
	62,
	58,
	54,
	50,
	46,
	42,
	38,
	34,
	30,
	27,
	24,
	21,
	18,
	15,
	12,
	9,
	6,
	3,
	0
};
static const int *volt_table_v35 = table_v35;


static const int table_v15[14] = {
	47,
	42,
	37,
	32,
	27,
	23,
	19,
	15,
	12,
	9,
	6,
	4,
	2,
	0
};
static const int *volt_table_v15 = table_v15;

static const int aor[IBRIGHTNESS_MAX] = {
	0x9B,  /* IBRIGHTNESS_5NT */
	0x99,  /* IBRIGHTNESS_6NT */
	0x98,  /* IBRIGHTNESS_7NT */
	0x97,  /* IBRIGHTNESS_8NT */
	0x95,  /* IBRIGHTNESS_9NT */
	0x94,  /* IBRIGHTNESS_10NT */
	0x93,  /* IBRIGHTNESS_11NT */
	0x91,  /* IBRIGHTNESS_12NT */
	0x90,  /* IBRIGHTNESS_13NT */
	0x8F,  /* IBRIGHTNESS_14NT */
	0x8D,  /* IBRIGHTNESS_15NT */
	0x8A,  /* IBRIGHTNESS_16NT */
	0x8B,  /* IBRIGHTNESS_17NT */
	0x88,  /* IBRIGHTNESS_19NT */
	0x87,  /* IBRIGHTNESS_20NT */
	0x85,  /* IBRIGHTNESS_21NT */
	0x84,  /* IBRIGHTNESS_22NT */
	0x81,  /* IBRIGHTNESS_24NT */
	0x80,  /* IBRIGHTNESS_25NT */
	0x7D,  /* IBRIGHTNESS_27NT */
	0x7A,  /* IBRIGHTNESS_29NT */
	0x79,  /* IBRIGHTNESS_30NT */
	0x76,  /* IBRIGHTNESS_32NT */
	0x74,  /* IBRIGHTNESS_34NT */
	0x6F,  /* IBRIGHTNESS_37NT */
	0x6C,  /* IBRIGHTNESS_39NT */
	0x69,  /* IBRIGHTNESS_41NT */
	0x65,  /* IBRIGHTNESS_44NT */
	0x60,  /* IBRIGHTNESS_47NT */
	0x5C,  /* IBRIGHTNESS_50NT */
	0x58,  /* IBRIGHTNESS_53NT */
	0x53,  /* IBRIGHTNESS_56NT */
	0x4D,  /* IBRIGHTNESS_60NT */
	0x44,  /* IBRIGHTNESS_64NT */
	0x44,  /* IBRIGHTNESS_68NT */
	0x44,  /* IBRIGHTNESS_72NT */
	0x44,  /* IBRIGHTNESS_77NT */
	0x44,  /* IBRIGHTNESS_82NT */
	0x44,  /* IBRIGHTNESS_87NT */
	0x44,  /* IBRIGHTNESS_93NT */
	0x44,  /* IBRIGHTNESS_98NT */
	0x44,  /* IBRIGHTNESS_105NT */
	0x44,  /* IBRIGHTNESS_111NT */
	0x44,  /* IBRIGHTNESS_119NT */
	0x44,  /* IBRIGHTNESS_126NT */
	0x44,  /* IBRIGHTNESS_134NT */
	0x44,  /* IBRIGHTNESS_143NT */
	0x44,  /* IBRIGHTNESS_152NT */
	0x44,  /* IBRIGHTNESS_162NT */
	0x3B,  /* IBRIGHTNESS_172NT */
	0x33,  /* IBRIGHTNESS_183NT */
	0x2D,  /* IBRIGHTNESS_195NT */
	0x25,  /* IBRIGHTNESS_207NT */
	0x1B,  /* IBRIGHTNESS_220NT */
	0x11,  /* IBRIGHTNESS_234NT */
	0x04,  /* IBRIGHTNESS_249NT */
	0x04,  /* IBRIGHTNESS_265NT */
	0x04,  /* IBRIGHTNESS_282NT */
	0x04,  /* IBRIGHTNESS_300NT */
	0x04  /* IBRIGHTNESS_HBM */
};
static const int *aor_table = aor;


static const int index_brightness_table[IBRIGHTNESS_MAX] = {
	5,	/* IBRIGHTNESS_5NT */
	6,	/* IBRIGHTNESS_6NT */
	7,	/* IBRIGHTNESS_7NT */
	8,	/* IBRIGHTNESS_8NT */
	9,	/* IBRIGHTNESS_9NT */
	10,  /* IBRIGHTNESS_10NT */
	11,  /* IBRIGHTNESS_11NT */
	12,  /* IBRIGHTNESS_12NT */
	13,  /* IBRIGHTNESS_13NT */
	14,  /* IBRIGHTNESS_14NT */
	15,  /* IBRIGHTNESS_15NT */
	16,  /* IBRIGHTNESS_16NT */
	17,  /* IBRIGHTNESS_17NT */
	19,  /* IBRIGHTNESS_19NT */
	20,  /* IBRIGHTNESS_20NT */
	21,  /* IBRIGHTNESS_21NT */
	22,  /* IBRIGHTNESS_22NT */
	24,  /* IBRIGHTNESS_24NT */
	25,  /* IBRIGHTNESS_25NT */
	27,  /* IBRIGHTNESS_27NT */
	29,  /* IBRIGHTNESS_29NT */
	30,  /* IBRIGHTNESS_30NT */
	32,  /* IBRIGHTNESS_32NT */
	34,  /* IBRIGHTNESS_34NT */
	37,  /* IBRIGHTNESS_37NT */
	39,  /* IBRIGHTNESS_39NT */
	41,  /* IBRIGHTNESS_41NT */
	44,  /* IBRIGHTNESS_44NT */
	47,  /* IBRIGHTNESS_47NT */
	50,  /* IBRIGHTNESS_50NT */
	53,  /* IBRIGHTNESS_53NT */
	56,  /* IBRIGHTNESS_56NT */
	60,  /* IBRIGHTNESS_60NT */
	64,  /* IBRIGHTNESS_64NT */
	68,  /* IBRIGHTNESS_68NT */
	72,  /* IBRIGHTNESS_72NT */
	77,  /* IBRIGHTNESS_77NT */
	82,  /* IBRIGHTNESS_82NT */
	87,  /* IBRIGHTNESS_87NT */
	93,  /* IBRIGHTNESS_93NT */
	98,  /* IBRIGHTNESS_98NT */
	105,  /* IBRIGHTNESS_105NT */
	111,  /* IBRIGHTNESS_111NT */
	119,  /* IBRIGHTNESS_119NT */
	126,  /* IBRIGHTNESS_126NT */
	134,  /* IBRIGHTNESS_134NT */
	143,  /* IBRIGHTNESS_143NT */
	152,  /* IBRIGHTNESS_152NT */
	162,  /* IBRIGHTNESS_162NT */
	172,  /* IBRIGHTNESS_172NT */
	183,  /* IBRIGHTNESS_183NT */
	195,  /* IBRIGHTNESS_195NT */
	207,  /* IBRIGHTNESS_207NT */
	220,  /* IBRIGHTNESS_220NT */
	234,  /* IBRIGHTNESS_234NT */
	249,  /* IBRIGHTNESS_249NT */
	265,  /* IBRIGHTNESS_265NT */
	282,  /* IBRIGHTNESS_282NT */
	300  /* IBRIGHTNESS_300NT */
};

static const int gamma_default_0[IV_MAX*CI_MAX] = {
	0x00, 0x00, 0x00,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x100, 0x100, 0x100	/* IV_255 */
};

static const int *gamma_default = gamma_default_0;

static const struct formular_t gamma_formula[IV_MAX] = {
	{5, 600},
	{20, 320},
	{65, 320},
	{65, 320},
	{65, 320},
	{65, 320},
	{100, 600}	/* IV_255 */
};

static const int vt_voltage_value[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
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
	114,	/* IBRIGHTNESS_68NT */
	120,	/* IBRIGHTNESS_72NT */
	127,	/* IBRIGHTNESS_77NT */
	135,	/* IBRIGHTNESS_82NT */
	142,	/* IBRIGHTNESS_87NT */
	151,	/* IBRIGHTNESS_93NT */
	160,	/* IBRIGHTNESS_98NT */
	170,	/* IBRIGHTNESS_105NT */
	179,	/* IBRIGHTNESS_111NT */
	191,	/* IBRIGHTNESS_119NT */
	201,	/* IBRIGHTNESS_126NT */
	213,	/* IBRIGHTNESS_134NT */
	227,	/* IBRIGHTNESS_143NT */
	240,	/* IBRIGHTNESS_152NT */
	254,	/* IBRIGHTNESS_162NT */
	254,	/* IBRIGHTNESS_172NT */
	254,	/* IBRIGHTNESS_183NT */
	254,	/* IBRIGHTNESS_195NT */
	254,	/* IBRIGHTNESS_207NT */
	254,	/* IBRIGHTNESS_220NT */
	254,	/* IBRIGHTNESS_234NT */
	254,	/* IBRIGHTNESS_249NT */
	267,	/* IBRIGHTNESS_265NT */
	282,	/* IBRIGHTNESS_282NT */
	300		/* IBRIGHTNESS_300NT */
};

static const int *gamma_curve_tables[IBRIGHTNESS_MAX] = {
	gamma_curve_2p00_table, /* IBRIGHTNESS_5NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_6NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_7NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_8NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_9NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_10NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_11NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_12NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_13NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_14NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_15NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_16NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_17NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_19NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_20NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_21NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_22NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_24NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_25NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_27NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_29NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_30NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_32NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_34NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_37NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_39NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_41NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_44NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_47NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_50NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_53NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_56NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_60NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_64NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_68NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_72NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_77NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_82NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_87NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_93NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_98NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_105NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_111NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_119NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_126NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_134NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_143NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_152NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_162NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_172NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_183NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_195NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_207NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_220NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_234NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_249NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_265NT */
	gamma_curve_2p00_table, /* IBRIGHTNESS_282NT */
	gamma_curve_2p00_table  /* IBRIGHTNESS_300NT */
};

static const int *gamma_curve_lut = gamma_curve_2p00_table;


static const int offset_gradation[IBRIGHTNESS_MAX][IV_MAX] = {	/* V0 ~ V255 */
	{0, 1, 9, 9, 6, 6, 1}, /* IBRIGHTNESS_5NT */
	{0, 1, 8, 8, 5, 6, 0}, /* IBRIGHTNESS_6NT */
	{0, 1, 8, 8, 5, 5, 0}, /* IBRIGHTNESS_7NT */
	{0, 1, 7, 7, 4, 5, 0}, /* IBRIGHTNESS_8NT */
	{0, 1, 7, 6, 4, 4, 0}, /* IBRIGHTNESS_9NT */
	{0, 1, 6, 5, 3, 4, 0}, /* IBRIGHTNESS_10NT */
	{0, 1, 6, 5, 3, 4, 0}, /* IBRIGHTNESS_11NT */
	{0, 1, 6, 5, 3, 4, 0}, /* IBRIGHTNESS_12NT */
	{0, 1, 5, 5, 3, 4, 0}, /* IBRIGHTNESS_13NT */
	{0, 1, 5, 4, 3, 4, 0}, /* IBRIGHTNESS_14NT */
	{0, 1, 5, 4, 2, 3, 0}, /* IBRIGHTNESS_15NT */
	{0, 1, 4, 4, 2, 3, 0}, /* IBRIGHTNESS_16NT */
	{0, 1, 4, 3, 2, 3, 0}, /* IBRIGHTNESS_17NT */
	{0, 1, 3, 3, 2, 3, 0}, /* IBRIGHTNESS_19NT */
	{0, 1, 3, 3, 2, 3, 0}, /* IBRIGHTNESS_20NT */
	{0, 1, 3, 3, 2, 3, 0}, /* IBRIGHTNESS_21NT */
	{0, 1, 3, 3, 2, 3, 0}, /* IBRIGHTNESS_22NT */
	{0, 1, 3, 3, 2, 3, 0}, /* IBRIGHTNESS_24NT */
	{0, 1, 3, 3, 2, 3, 0}, /* IBRIGHTNESS_25NT */
	{0, 1, 3, 3, 2, 3, 0}, /* IBRIGHTNESS_27NT */
	{0, 1, 3, 3, 2, 3, 0}, /* IBRIGHTNESS_29NT */
	{0, 1, 3, 3, 2, 3, 0}, /* IBRIGHTNESS_30NT */
	{0, 1, 3, 3, 2, 3, 0}, /* IBRIGHTNESS_32NT */
	{0, 1, 3, 3, 2, 3, 0}, /* IBRIGHTNESS_34NT */
	{0, 1, 3, 3, 2, 3, 0}, /* IBRIGHTNESS_37NT */
	{0, 2, 3, 3, 2, 3, 0}, /* IBRIGHTNESS_39NT */
	{0, 2, 3, 3, 2, 3, 0}, /* IBRIGHTNESS_41NT */
	{0, 2, 2, 2, 1, 2, 0}, /* IBRIGHTNESS_44NT */
	{0, 2, 2, 2, 1, 2, 0}, /* IBRIGHTNESS_47NT */
	{0, 3, 2, 2, 1, 2, 0}, /* IBRIGHTNESS_50NT */
	{0, 3, 2, 2, 1, 2, 0}, /* IBRIGHTNESS_53NT */
	{0, 3, 2, 2, 1, 2, 0}, /* IBRIGHTNESS_56NT */
	{0, 3, 2, 2, 1, 2, 0}, /* IBRIGHTNESS_60NT */
	{0, 2, 2, 2, 1, 2, 0}, /* IBRIGHTNESS_64NT */
	{0, 2, 2, 2, 1, 2, 0}, /* IBRIGHTNESS_68NT */
	{0, 2, 2, 2, 1, 3, 0}, /* IBRIGHTNESS_72NT */
	{0, 2, 2, 2, 1, 3, 0}, /* IBRIGHTNESS_77NT */
	{0, 2, 2, 3, 2, 3, 0}, /* IBRIGHTNESS_82NT */
	{0, 2, 2, 3, 2, 3, 0}, /* IBRIGHTNESS_87NT */
	{0, 2, 2, 3, 2, 4, 0}, /* IBRIGHTNESS_93NT */
	{0, 2, 2, 3, 2, 4, 0}, /* IBRIGHTNESS_98NT */
	{0, 2, 2, 3, 2, 4, 0}, /* IBRIGHTNESS_105NT */
	{0, 2, 2, 3, 2, 4, 0}, /* IBRIGHTNESS_111NT */
	{0, 2, 2, 3, 2, 4, 0}, /* IBRIGHTNESS_119NT */
	{0, 1, 2, 3, 3, 4, 0}, /* IBRIGHTNESS_126NT */
	{0, 1, 3, 3, 3, 4, 0}, /* IBRIGHTNESS_134NT */
	{0, 1, 3, 3, 3, 4, 0}, /* IBRIGHTNESS_143NT */
	{0, 1, 3, 3, 3, 4, 0}, /* IBRIGHTNESS_152NT */
	{0, 1, 3, 3, 4, 4, 2}, /* IBRIGHTNESS_162NT */
	{0, 1, 3, 3, 4, 4, 0}, /* IBRIGHTNESS_172NT */
	{0, 1, 3, 3, 3, 3, 0}, /* IBRIGHTNESS_183NT */
	{0, 1, 3, 3, 3, 3, 0}, /* IBRIGHTNESS_195NT */
	{0, 1, 2, 2, 3, 3, 0}, /* IBRIGHTNESS_207NT */
	{0, 1, 2, 2, 2, 2, 0}, /* IBRIGHTNESS_220NT */
	{0, 1, 2, 2, 2, 2, 0}, /* IBRIGHTNESS_234NT */
	{0, 0, 1, 1, 1, 2, 0}, /* IBRIGHTNESS_249NT */
	{0, 0, 1, 1, 1, 1, 0}, /* IBRIGHTNESS_265NT */
	{0, 0, 1, 1, 1, 1, 0}, /* IBRIGHTNESS_282NT */
	{0, 0, 0, 0, 0, 0, 0}, /* IBRIGHTNESS_300NT */
};

static const int offset_color[IBRIGHTNESS_MAX][CI_MAX * IV_MAX] = {	/* V0 ~ V255 */
	{0, 0, 0, 0, 0, 0, -25, -21, 5, -4, -3, 1, 0, -1, 1, -1, 0, -1, -1, 0, 1}, /* IBRIGHTNESS_5NT */
	{0, 0, 0, 0, 0, 0, -20, -17, 3, -4, -3, 1, 0, -1, 0, -1, 0, -1, -1, 0, 1}, /* IBRIGHTNESS_6NT */
	{0, 0, 0, 0, 0, 0, -17, -14, 2, -4, -3, 0, 0, -1, 0, -1, 0, -1, -1, 0, 1}, /* IBRIGHTNESS_7NT */
	{0, 0, 0, 0, 0, 0, -14, -12, 2, -4, -3, 0, 0, -1, 0, -1, 0, -1, -1, 0, 1}, /* IBRIGHTNESS_8NT */
	{0, 0, 0, 0, 0, 0, -11, -9, 2, -4, -2, 0, 0, -1, 0, -1, 0, -1, -1, 0, 1}, /* IBRIGHTNESS_9NT */
	{0, 0, 0, 0, 0, 0, -8, -7, 2, -4, -2, 0, 0, -1, 0, -1, 0, -1, -1, 0, 1}, /* IBRIGHTNESS_10NT */
	{0, 0, 0, 0, 0, 0, -7, -7, 2, -4, -2, 0, 0, -1, 0, -1, 0, -1, -1, 0, 1}, /* IBRIGHTNESS_11NT */
	{0, 0, 0, 0, 0, 0, -5, -5, 1, -3, -1, 0, 0, -1, 0, -1, 0, -1, -1, 0, 1}, /* IBRIGHTNESS_12NT */
	{0, 0, 0, 0, 0, 0, -5, -5, 1, -3, -1, 0, 0, -1, 0, -1, 0, -1, -1, 0, 1}, /* IBRIGHTNESS_13NT */
	{0, 0, 0, 0, 0, 0, -5, -5, 1, -3, -1, 0, 0, -1, 0, 0, 0, -1, -1, 0, 1}, /* IBRIGHTNESS_14NT */
	{0, 0, 0, 0, 0, 0, -4, -4, 1, -2, -1, 0, 0, 0, 0, 0, 1, 0, -1, 0, 1}, /* IBRIGHTNESS_15NT */
	{0, 0, 0, 0, 0, 0, -3, -3, 1, -2, -1, 0, 0, 0, 0, 0, 1, 0, -1, 0, 1}, /* IBRIGHTNESS_16NT */
	{0, 0, 0, 0, 0, 0, -3, -3, 1, -1, -1, 0, 0, 0, 0, 1, 1, 0, -1, 0, 1}, /* IBRIGHTNESS_17NT */
	{0, 0, 0, 0, 0, 0, -3, -3, 1, -1, -1, 0, 0, 0, 0, 1, 1, 0, -1, 0, 1}, /* IBRIGHTNESS_19NT */
	{0, 0, 0, 0, 0, 0, -3, -3, 1, -1, -1, 0, 0, 0, 0, 1, 1, 0, -1, 0, 1}, /* IBRIGHTNESS_20NT */
	{0, 0, 0, 0, 0, 0, -3, -3, 1, -1, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_21NT */
	{0, 0, 0, 0, 0, 0, -3, -3, 1, -1, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_22NT */
	{0, 0, 0, 0, 0, 0, -2, -2, 1, -1, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_24NT */
	{0, 0, 0, 0, 0, 0, -2, -2, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_25NT */
	{0, 0, 0, 0, 0, 0, -2, -2, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_27NT */
	{0, 0, 0, 0, 0, 0, -1, -1, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_29NT */
	{0, 0, 0, 0, 0, 0, -1, -1, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_30NT */
	{0, 0, 0, 0, 0, 0, -1, -1, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_32NT */
	{0, 0, 0, -1, -5, 0, -1, -1, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_34NT */
	{0, 0, 0, -2, -7, 0, -1, -1, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_37NT */
	{0, 0, 0, -3, -7, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_39NT */
	{0, 0, 0, -4, -9, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_41NT */
	{0, 0, 0, -5, -9, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_44NT */
	{0, 0, 0, -6, -11, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_47NT */
	{0, 0, 0, -10, -15, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_50NT */
	{0, 0, 0, -10, -15, 4, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_53NT */
	{0, 0, 0, -10, -15, 4, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_56NT */
	{0, 0, 0, -10, -15, 4, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_60NT */
	{0, 0, 0, -10, -15, 4, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_64NT */
	{0, 0, 0, -10, -15, 4, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_68NT */
	{0, 0, 0, -10, -15, 4, 1, 2, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, -1, 0, 0}, /* IBRIGHTNESS_72NT */
	{0, 0, 0, -9, -14, 3, 1, 2, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, -1, 0, 0}, /* IBRIGHTNESS_77NT */
	{0, 0, 0, -9, -14, 3, 0, 1, 0, -1, -1, 0, 1, 0, 0, 0, 1, 0, 0, -1, 0}, /* IBRIGHTNESS_82NT */
	{0, 0, 0, -8, -13, 3, 0, 1, 0, -1, -1, 0, 1, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_87NT */
	{0, 0, 0, -7, -9, 2, 0, 1, 0, -1, -1, 0, 1, 0, 0, 0, 1, 0, -1, 0, 0}, /* IBRIGHTNESS_93NT */
	{0, 0, 0, -7, -9, 2, 0, 1, 0, -1, -1, 0, 1, 0, 0, 0, 1, 0, -1, 0, 0}, /* IBRIGHTNESS_98NT */
	{0, 0, 0, -6, -8, 2, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, -1, 0, 0}, /* IBRIGHTNESS_105NT */
	{0, 0, 0, -5, -7, 2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_111NT */
	{0, 0, 0, -5, -7, 2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, -1, -1, 0}, /* IBRIGHTNESS_119NT */
	{0, 0, 0, -5, -7, 2, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_126NT */
	{0, 0, 0, -4, -6, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, -1, -1, 0}, /* IBRIGHTNESS_134NT */
	{0, 0, 0, -3, -6, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, -1, 0, 0}, /* IBRIGHTNESS_143NT */
	{0, 0, 0, -3, -5, 1, -1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, -1, -1, 0}, /* IBRIGHTNESS_152NT */
	{0, 0, 0, -2, -5, 1, -1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, -1, -1, 0}, /* IBRIGHTNESS_162NT */
	{0, 0, 0, -2, -5, 1, -1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, -1, -1, 0}, /* IBRIGHTNESS_172NT */
	{0, 0, 0, -2, -4, 1, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, -1, -1, 0}, /* IBRIGHTNESS_183NT */
	{0, 0, 0, -1, -4, 1, -1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, -1, -1, 0}, /* IBRIGHTNESS_195NT */
	{0, 0, 0, -1, -3, 1, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, -1, 0, 0, 0, 0}, /* IBRIGHTNESS_207NT */
	{0, 0, 0, -1, -3, 1, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, -1, 0, 0, 0, 0}, /* IBRIGHTNESS_220NT */
	{0, 0, 0, 0, -2, 1, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}, /* IBRIGHTNESS_234NT */
	{0, 0, 0, -1, -2, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0}, /* IBRIGHTNESS_249NT */
	{0, 0, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* IBRIGHTNESS_265NT */
	{0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* IBRIGHTNESS_282NT */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* IBRIGHTNESS_300NT */
};

#endif /* __DYNAMIC_AID_XXXX_H */
