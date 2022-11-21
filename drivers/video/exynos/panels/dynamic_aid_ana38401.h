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
	IV_MAX,
};

enum {
	IBRIGHTNESS_2NT,
	IBRIGHTNESS_3NT,
	IBRIGHTNESS_4NT,
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
	IBRIGHTNESS_316NT,
	IBRIGHTNESS_333NT,
	IBRIGHTNESS_360NT,
	IBRIGHTNESS_500NT,
	IBRIGHTNESS_MAX
};

/* brightness <-> nit <-> level table, 0~255 x 2 = 512 */
static unsigned int brightness_table[512] = {
	2,	IBRIGHTNESS_2NT,
	2,	IBRIGHTNESS_2NT,
	2,	IBRIGHTNESS_2NT,
	3,	IBRIGHTNESS_3NT,
	4,	IBRIGHTNESS_4NT,
	5,	IBRIGHTNESS_5NT,
	6,	IBRIGHTNESS_6NT,
	7,	IBRIGHTNESS_7NT,
	8,	IBRIGHTNESS_8NT,
	9,	IBRIGHTNESS_9NT,
	10,	IBRIGHTNESS_10NT,
	11,	IBRIGHTNESS_11NT,
	12,	IBRIGHTNESS_12NT,
	13,	IBRIGHTNESS_13NT,
	14,	IBRIGHTNESS_14NT,
	15,	IBRIGHTNESS_15NT,
	16,	IBRIGHTNESS_16NT,
	17,	IBRIGHTNESS_17NT,
	18,	IBRIGHTNESS_19NT,
	19,	IBRIGHTNESS_19NT,
	20,	IBRIGHTNESS_20NT,
	21,	IBRIGHTNESS_21NT,
	22,	IBRIGHTNESS_22NT,
	24,	IBRIGHTNESS_25NT,
	25,	IBRIGHTNESS_25NT,
	27,	IBRIGHTNESS_27NT,
	29,	IBRIGHTNESS_29NT,
	30,	IBRIGHTNESS_30NT,
	32,	IBRIGHTNESS_32NT,
	37,	IBRIGHTNESS_37NT,
	39,	IBRIGHTNESS_39NT,
	41,	IBRIGHTNESS_41NT,
	43,	IBRIGHTNESS_44NT,
	44,	IBRIGHTNESS_44NT,
	46,	IBRIGHTNESS_47NT,
	47,	IBRIGHTNESS_47NT,
	49,	IBRIGHTNESS_50NT,
	50,	IBRIGHTNESS_50NT,
	52,	IBRIGHTNESS_53NT,
	53,	IBRIGHTNESS_53NT,
	55,	IBRIGHTNESS_56NT,
	56,	IBRIGHTNESS_56NT,
	57,	IBRIGHTNESS_60NT,
	59,	IBRIGHTNESS_60NT,
	60,	IBRIGHTNESS_60NT,
	61,	IBRIGHTNESS_64NT,
	63,	IBRIGHTNESS_64NT,
	64,	IBRIGHTNESS_64NT,
	65,	IBRIGHTNESS_68NT,
	67,	IBRIGHTNESS_68NT,
	68,	IBRIGHTNESS_68NT,
	69,	IBRIGHTNESS_72NT,
	71,	IBRIGHTNESS_72NT,
	72,	IBRIGHTNESS_72NT,
	73,	IBRIGHTNESS_77NT,
	75,	IBRIGHTNESS_77NT,
	76,	IBRIGHTNESS_77NT,
	77,	IBRIGHTNESS_77NT,
	79,	IBRIGHTNESS_82NT,
	80,	IBRIGHTNESS_82NT,
	82,	IBRIGHTNESS_82NT,
	83,	IBRIGHTNESS_87NT,
	85,	IBRIGHTNESS_87NT,
	86,	IBRIGHTNESS_87NT,
	87,	IBRIGHTNESS_87NT,
	89,	IBRIGHTNESS_93NT,
	90,	IBRIGHTNESS_93NT,
	92,	IBRIGHTNESS_93NT,
	93,	IBRIGHTNESS_93NT,
	94,	IBRIGHTNESS_98NT,
	96,	IBRIGHTNESS_98NT,
	97,	IBRIGHTNESS_98NT,
	98,	IBRIGHTNESS_98NT,
	99,	IBRIGHTNESS_105NT,
	101,	IBRIGHTNESS_105NT,
	102,	IBRIGHTNESS_105NT,
	104,	IBRIGHTNESS_105NT,
	105,	IBRIGHTNESS_105NT,
	107,	IBRIGHTNESS_111NT,
	108,	IBRIGHTNESS_111NT,
	110,	IBRIGHTNESS_111NT,
	111,	IBRIGHTNESS_111NT,
	112,	IBRIGHTNESS_119NT,
	114,	IBRIGHTNESS_119NT,
	115,	IBRIGHTNESS_119NT,
	116,	IBRIGHTNESS_119NT,
	118,	IBRIGHTNESS_119NT,
	119,	IBRIGHTNESS_119NT,
	120,	IBRIGHTNESS_126NT,
	122,	IBRIGHTNESS_126NT,
	123,	IBRIGHTNESS_126NT,
	125,	IBRIGHTNESS_126NT,
	126,	IBRIGHTNESS_126NT,
	127,	IBRIGHTNESS_134NT,
	129,	IBRIGHTNESS_134NT,
	130,	IBRIGHTNESS_134NT,
	131,	IBRIGHTNESS_134NT,
	133,	IBRIGHTNESS_134NT,
	134,	IBRIGHTNESS_134NT,
	135,	IBRIGHTNESS_143NT,
	137,	IBRIGHTNESS_143NT,
	138,	IBRIGHTNESS_143NT,
	139,	IBRIGHTNESS_143NT,
	140,	IBRIGHTNESS_143NT,
	142,	IBRIGHTNESS_143NT,
	143,	IBRIGHTNESS_143NT,
	145,	IBRIGHTNESS_152NT,
	146,	IBRIGHTNESS_152NT,
	148,	IBRIGHTNESS_152NT,
	149,	IBRIGHTNESS_152NT,
	151,	IBRIGHTNESS_152NT,
	152,	IBRIGHTNESS_152NT,
	153,	IBRIGHTNESS_162NT,
	155,	IBRIGHTNESS_162NT,
	156,	IBRIGHTNESS_162NT,
	157,	IBRIGHTNESS_162NT,
	158,	IBRIGHTNESS_162NT,
	160,	IBRIGHTNESS_162NT,
	161,	IBRIGHTNESS_162NT,
	162,	IBRIGHTNESS_162NT,
	163,	IBRIGHTNESS_172NT,
	165,	IBRIGHTNESS_172NT,
	166,	IBRIGHTNESS_172NT,
	168,	IBRIGHTNESS_172NT,
	169,	IBRIGHTNESS_172NT,
	171,	IBRIGHTNESS_172NT,
	172,	IBRIGHTNESS_172NT,
	173,	IBRIGHTNESS_183NT,
	175,	IBRIGHTNESS_183NT,
	176,	IBRIGHTNESS_183NT,
	178,	IBRIGHTNESS_183NT,
	179,	IBRIGHTNESS_183NT,
	180,	IBRIGHTNESS_183NT,
	182,	IBRIGHTNESS_183NT,
	183,	IBRIGHTNESS_183NT,
	184,	IBRIGHTNESS_195NT,
	186,	IBRIGHTNESS_195NT,
	187,	IBRIGHTNESS_195NT,
	188,	IBRIGHTNESS_195NT,
	190,	IBRIGHTNESS_195NT,
	191,	IBRIGHTNESS_195NT,
	192,	IBRIGHTNESS_195NT,
	194,	IBRIGHTNESS_195NT,
	195,	IBRIGHTNESS_195NT,
	197,	IBRIGHTNESS_207NT,
	198,	IBRIGHTNESS_207NT,
	200,	IBRIGHTNESS_207NT,
	201,	IBRIGHTNESS_207NT,
	203,	IBRIGHTNESS_207NT,
	204,	IBRIGHTNESS_207NT,
	206,	IBRIGHTNESS_207NT,
	207,	IBRIGHTNESS_207NT,
	208,	IBRIGHTNESS_220NT,
	210,	IBRIGHTNESS_220NT,
	211,	IBRIGHTNESS_220NT,
	212,	IBRIGHTNESS_220NT,
	214,	IBRIGHTNESS_220NT,
	215,	IBRIGHTNESS_220NT,
	216,	IBRIGHTNESS_220NT,
	217,	IBRIGHTNESS_220NT,
	219,	IBRIGHTNESS_220NT,
	220,	IBRIGHTNESS_220NT,
	221,	IBRIGHTNESS_234NT,
	223,	IBRIGHTNESS_234NT,
	224,	IBRIGHTNESS_234NT,
	226,	IBRIGHTNESS_234NT,
	227,	IBRIGHTNESS_234NT,
	228,	IBRIGHTNESS_234NT,
	230,	IBRIGHTNESS_234NT,
	231,	IBRIGHTNESS_234NT,
	233,	IBRIGHTNESS_234NT,
	234,	IBRIGHTNESS_234NT,
	236,	IBRIGHTNESS_249NT,
	237,	IBRIGHTNESS_249NT,
	239,	IBRIGHTNESS_249NT,
	240,	IBRIGHTNESS_249NT,
	242,	IBRIGHTNESS_249NT,
	243,	IBRIGHTNESS_249NT,
	245,	IBRIGHTNESS_249NT,
	246,	IBRIGHTNESS_249NT,
	248,	IBRIGHTNESS_249NT,
	249,	IBRIGHTNESS_249NT,
	249,	IBRIGHTNESS_249NT,
	250,	IBRIGHTNESS_265NT,
	252,	IBRIGHTNESS_265NT,
	253,	IBRIGHTNESS_265NT,
	254,	IBRIGHTNESS_265NT,
	256,	IBRIGHTNESS_265NT,
	257,	IBRIGHTNESS_265NT,
	258,	IBRIGHTNESS_265NT,
	260,	IBRIGHTNESS_265NT,
	261,	IBRIGHTNESS_265NT,
	262,	IBRIGHTNESS_265NT,
	264,	IBRIGHTNESS_265NT,
	265,	IBRIGHTNESS_265NT,
	266,	IBRIGHTNESS_282NT,
	268,	IBRIGHTNESS_282NT,
	269,	IBRIGHTNESS_282NT,
	271,	IBRIGHTNESS_282NT,
	272,	IBRIGHTNESS_282NT,
	274,	IBRIGHTNESS_282NT,
	275,	IBRIGHTNESS_282NT,
	276,	IBRIGHTNESS_282NT,
	278,	IBRIGHTNESS_282NT,
	279,	IBRIGHTNESS_282NT,
	281,	IBRIGHTNESS_282NT,
	282,	IBRIGHTNESS_282NT,
	283,	IBRIGHTNESS_300NT,
	285,	IBRIGHTNESS_300NT,
	286,	IBRIGHTNESS_300NT,
	288,	IBRIGHTNESS_300NT,
	289,	IBRIGHTNESS_300NT,
	290,	IBRIGHTNESS_300NT,
	292,	IBRIGHTNESS_300NT,
	293,	IBRIGHTNESS_300NT,
	294,	IBRIGHTNESS_300NT,
	296,	IBRIGHTNESS_300NT,
	297,	IBRIGHTNESS_300NT,
	299,	IBRIGHTNESS_300NT,
	300,	IBRIGHTNESS_300NT,
	301,	IBRIGHTNESS_316NT,
	303,	IBRIGHTNESS_316NT,
	304,	IBRIGHTNESS_316NT,
	305,	IBRIGHTNESS_316NT,
	307,	IBRIGHTNESS_316NT,
	308,	IBRIGHTNESS_316NT,
	309,	IBRIGHTNESS_316NT,
	311,	IBRIGHTNESS_316NT,
	312,	IBRIGHTNESS_316NT,
	313,	IBRIGHTNESS_316NT,
	315,	IBRIGHTNESS_316NT,
	316,	IBRIGHTNESS_316NT,
	317,	IBRIGHTNESS_333NT,
	319,	IBRIGHTNESS_333NT,
	320,	IBRIGHTNESS_333NT,
	322,	IBRIGHTNESS_333NT,
	323,	IBRIGHTNESS_333NT,
	325,	IBRIGHTNESS_333NT,
	326,	IBRIGHTNESS_333NT,
	327,	IBRIGHTNESS_333NT,
	329,	IBRIGHTNESS_333NT,
	330,	IBRIGHTNESS_333NT,
	332,	IBRIGHTNESS_333NT,
	333,	IBRIGHTNESS_333NT,
	335,	IBRIGHTNESS_360NT,
	338,	IBRIGHTNESS_360NT,
	340,	IBRIGHTNESS_360NT,
	343,	IBRIGHTNESS_360NT,
	345,	IBRIGHTNESS_360NT,
	348,	IBRIGHTNESS_360NT,
	350,	IBRIGHTNESS_360NT,
	353,	IBRIGHTNESS_360NT,
	355,	IBRIGHTNESS_360NT,
	358,	IBRIGHTNESS_360NT,
	360,	IBRIGHTNESS_360NT,
	360,	IBRIGHTNESS_360NT
};

#define VREG_OUT_X1000		6800	/* VREG_OUT x 1000 */

static const int index_voltage_table[IBRIGHTNESS_MAX] = {
	0,	/* IV_VT */
	3,	/* IV_3 */
	11,	/* IV_11 */
	23,	/* IV_23 */
	35,	/* IV_35 */
	51,	/* IV_51 */
	87,	/* IV_87 */
	151,	/* IV_151 */
	203,	/* IV_203 */
	255	/* IV_255 */
};

static int index_brightness_table[IBRIGHTNESS_MAX] = {
	2,	/* IBRIGHTNESS_2NT */
	3,	/* IBRIGHTNESS_3NT */
	4,	/* IBRIGHTNESS_4NT */
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
	111,	/* IBRIGHTNESS_111NT */
	119,	/* IBRIGHTNESS_119NT */
	126,	/* IBRIGHTNESS_126NT */
	134,	/* IBRIGHTNESS_134NT */
	143,	/* IBRIGHTNESS_143NT */
	152,	/* IBRIGHTNESS_152NT */
	162,	/* IBRIGHTNESS_162NT */
	172,	/* IBRIGHTNESS_172NT */
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
	360,	/* IBRIGHTNESS_360NT */
	360	/* IBRIGHTNESS_500NT */
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
	0x100, 0x100, 0x100,	/* IV_255 */
};

static const int *gamma_default = gamma_default_0;

static const struct formular_t gamma_formula[IV_MAX] = {
	{0, 600},	/* IV_VT */
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{129, 640},	/* IV_255 */
};

static const int vt_voltage_value[] = {
	0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 138, 148, 158, 168, 178, 186
};

static const int brightness_base_table[IBRIGHTNESS_MAX] = {
	112,	/* IBRIGHTNESS_2NT */
	112,	/* IBRIGHTNESS_3NT */
	112,	/* IBRIGHTNESS_4NT */
	112,	/* IBRIGHTNESS_5NT */
	112,	/* IBRIGHTNESS_6NT */
	112,	/* IBRIGHTNESS_7NT */
	112,	/* IBRIGHTNESS_8NT */
	112,	/* IBRIGHTNESS_9NT */
	112,	/* IBRIGHTNESS_10NT */
	112,	/* IBRIGHTNESS_11NT */
	112,	/* IBRIGHTNESS_12NT */
	112,	/* IBRIGHTNESS_13NT */
	112,	/* IBRIGHTNESS_14NT */
	112,	/* IBRIGHTNESS_15NT */
	112,	/* IBRIGHTNESS_16NT */
	112,	/* IBRIGHTNESS_17NT */
	112,	/* IBRIGHTNESS_19NT */
	112,	/* IBRIGHTNESS_20NT */
	112,	/* IBRIGHTNESS_21NT */
	112,	/* IBRIGHTNESS_22NT */
	112,	/* IBRIGHTNESS_24NT */
	112,	/* IBRIGHTNESS_25NT */
	112,	/* IBRIGHTNESS_27NT */
	112,	/* IBRIGHTNESS_29NT */
	112,	/* IBRIGHTNESS_30NT */
	112,	/* IBRIGHTNESS_32NT */
	112,	/* IBRIGHTNESS_34NT */
	112,	/* IBRIGHTNESS_37NT */
	112,	/* IBRIGHTNESS_39NT */
	112,	/* IBRIGHTNESS_41NT */
	112,	/* IBRIGHTNESS_44NT */
	112,	/* IBRIGHTNESS_47NT */
	112,	/* IBRIGHTNESS_50NT */
	112,	/* IBRIGHTNESS_53NT */
	112,	/* IBRIGHTNESS_56NT */
	112,	/* IBRIGHTNESS_60NT */
	112,	/* IBRIGHTNESS_64NT */
	112,	/* IBRIGHTNESS_68NT */
	112,	/* IBRIGHTNESS_72NT */
	120,	/* IBRIGHTNESS_77NT */
	127,	/* IBRIGHTNESS_82NT */
	135,	/* IBRIGHTNESS_87NT */
	143,	/* IBRIGHTNESS_93NT */
	152,	/* IBRIGHTNESS_98NT */
	162,	/* IBRIGHTNESS_105NT */
	170,	/* IBRIGHTNESS_111NT */
	181,	/* IBRIGHTNESS_119NT */
	192,	/* IBRIGHTNESS_126NT */
	203,	/* IBRIGHTNESS_134NT */
	216,	/* IBRIGHTNESS_143NT */
	227,	/* IBRIGHTNESS_152NT */
	240,	/* IBRIGHTNESS_162NT */
	254,	/* IBRIGHTNESS_172NT */
	254,	/* IBRIGHTNESS_183NT */
	254,	/* IBRIGHTNESS_195NT */
	254,	/* IBRIGHTNESS_207NT */
	254,	/* IBRIGHTNESS_220NT */
	254,	/* IBRIGHTNESS_234NT */
	254,	/* IBRIGHTNESS_249NT */
	272,	/* IBRIGHTNESS_265NT */
	288,	/* IBRIGHTNESS_282NT */
	307,	/* IBRIGHTNESS_300NT */
	321,	/* IBRIGHTNESS_316NT */
	336,	/* IBRIGHTNESS_333NT */
	360,	/* IBRIGHTNESS_360NT */
	360	/* IBRIGHTNESS_500NT */
};

static const int *gamma_curve_tables[IBRIGHTNESS_MAX] = {
	gamma_curve_2p15_table,	/* IBRIGHTNESS_2NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_3NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_4NT */
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
	gamma_curve_2p15_table,	/* IBRIGHTNESS_111NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_119NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_126NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_134NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_143NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_152NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_162NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_172NT */
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
	gamma_curve_2p20_table,	/* IBRIGHTNESS_360NT */
	gamma_curve_2p20_table	/* IBRIGHTNESS_500NT */
};

static const int *gamma_curve_lut = gamma_curve_2p20_table;

static const unsigned char inter_aor_cmd[256][2] = {
	{0x01, 0x7F},
	{0x01, 0x7F},
	{0x01, 0x7F},
	{0x01, 0x7D},
	{0x01, 0x7A},
	{0x01, 0x78},
	{0x01, 0x75},
	{0x01, 0x72},
	{0x01, 0x6F},
	{0x01, 0x6C},
	{0x01, 0x69},
	{0x01, 0x67},
	{0x01, 0x64},
	{0x01, 0x61},
	{0x01, 0x5E},
	{0x01, 0x5A},
	{0x01, 0x58},
	{0x01, 0x56},
	{0x01, 0x53},
	{0x01, 0x50},
	{0x01, 0x4D},
	{0x01, 0x48},
	{0x01, 0x45},
	{0x01, 0x40},
	{0x01, 0x3C},
	{0x01, 0x36},
	{0x01, 0x30},
	{0x01, 0x2C},
	{0x01, 0x26},
	{0x01, 0x16},
	{0x01, 0x0F},
	{0x01, 0x09},
	{0x01, 0x04},
	{0x00, 0xFF},
	{0x00, 0xFA},
	{0x00, 0xF5},
	{0x00, 0xF0},
	{0x00, 0xEB},
	{0x00, 0xE6},
	{0x00, 0xE1},
	{0x00, 0xDC},
	{0x00, 0xD7},
	{0x00, 0xD2},
	{0x00, 0xCE},
	{0x00, 0xC9},
	{0x00, 0xC4},
	{0x00, 0xC0},
	{0x00, 0xBB},
	{0x00, 0xB6},
	{0x00, 0xB2},
	{0x00, 0xAD},
	{0x00, 0xA6},
	{0x00, 0xA0},
	{0x00, 0x99},
	{0x00, 0x99},
	{0x00, 0x99},
	{0x00, 0x99},
	{0x00, 0x99},
	{0x00, 0xA2},
	{0x00, 0x9F},
	{0x00, 0x99},
	{0x00, 0xA4},
	{0x00, 0x9E},
	{0x00, 0x9C},
	{0x00, 0x99},
	{0x00, 0xA3},
	{0x00, 0xA1},
	{0x00, 0x9C},
	{0x00, 0x99},
	{0x00, 0xA3},
	{0x00, 0x9E},
	{0x00, 0x9B},
	{0x00, 0x99},
	{0x00, 0xA6},
	{0x00, 0xA2},
	{0x00, 0xA0},
	{0x00, 0x9B},
	{0x00, 0x99},
	{0x00, 0xA1},
	{0x00, 0x9F},
	{0x00, 0x9B},
	{0x00, 0x99},
	{0x00, 0xA7},
	{0x00, 0xA3},
	{0x00, 0xA1},
	{0x00, 0x9F},
	{0x00, 0x9B},
	{0x00, 0x99},
	{0x00, 0xA4},
	{0x00, 0xA0},
	{0x00, 0x9F},
	{0x00, 0x9B},
	{0x00, 0x99},
	{0x00, 0xA5},
	{0x00, 0xA2},
	{0x00, 0xA0},
	{0x00, 0x9E},
	{0x00, 0x9B},
	{0x00, 0x99},
	{0x00, 0xA6},
	{0x00, 0xA3},
	{0x00, 0xA1},
	{0x00, 0xA0},
	{0x00, 0x9E},
	{0x00, 0x9B},
	{0x00, 0x99},
	{0x00, 0xA4},
	{0x00, 0xA2},
	{0x00, 0x9F},
	{0x00, 0x9E},
	{0x00, 0x9B},
	{0x00, 0x99},
	{0x00, 0xA6},
	{0x00, 0xA3},
	{0x00, 0xA2},
	{0x00, 0xA0},
	{0x00, 0x9F},
	{0x00, 0x9C},
	{0x00, 0x9A},
	{0x00, 0x99},
	{0x00, 0xA5},
	{0x00, 0xA3},
	{0x00, 0xA1},
	{0x00, 0x9E},
	{0x00, 0x9D},
	{0x00, 0x9A},
	{0x00, 0x99},
	{0x00, 0x97},
	{0x00, 0x94},
	{0x00, 0x92},
	{0x00, 0x90},
	{0x00, 0x8D},
	{0x00, 0x8B},
	{0x00, 0x88},
	{0x00, 0x86},
	{0x00, 0x84},
	{0x00, 0x81},
	{0x00, 0x7F},
	{0x00, 0x7D},
	{0x00, 0x7A},
	{0x00, 0x78},
	{0x00, 0x76},
	{0x00, 0x73},
	{0x00, 0x71},
	{0x00, 0x6E},
	{0x00, 0x6C},
	{0x00, 0x69},
	{0x00, 0x67},
	{0x00, 0x64},
	{0x00, 0x61},
	{0x00, 0x5F},
	{0x00, 0x5C},
	{0x00, 0x5A},
	{0x00, 0x57},
	{0x00, 0x55},
	{0x00, 0x53},
	{0x00, 0x51},
	{0x00, 0x4E},
	{0x00, 0x4C},
	{0x00, 0x4A},
	{0x00, 0x47},
	{0x00, 0x45},
	{0x00, 0x42},
	{0x00, 0x40},
	{0x00, 0x3D},
	{0x00, 0x3B},
	{0x00, 0x38},
	{0x00, 0x35},
	{0x00, 0x33},
	{0x00, 0x30},
	{0x00, 0x2E},
	{0x00, 0x2B},
	{0x00, 0x28},
	{0x00, 0x26},
	{0x00, 0x23},
	{0x00, 0x20},
	{0x00, 0x1E},
	{0x00, 0x1B},
	{0x00, 0x19},
	{0x00, 0x16},
	{0x00, 0x13},
	{0x00, 0x11},
	{0x00, 0x0E},
	{0x00, 0x23},
	{0x00, 0x20},
	{0x00, 0x1F},
	{0x00, 0x1E},
	{0x00, 0x1B},
	{0x00, 0x19},
	{0x00, 0x18},
	{0x00, 0x15},
	{0x00, 0x14},
	{0x00, 0x12},
	{0x00, 0x0F},
	{0x00, 0x0E},
	{0x00, 0x23},
	{0x00, 0x21},
	{0x00, 0x1F},
	{0x00, 0x1D},
	{0x00, 0x1B},
	{0x00, 0x19},
	{0x00, 0x17},
	{0x00, 0x16},
	{0x00, 0x13},
	{0x00, 0x12},
	{0x00, 0x0F},
	{0x00, 0x0E},
	{0x00, 0x23},
	{0x00, 0x21},
	{0x00, 0x1F},
	{0x00, 0x1D},
	{0x00, 0x1C},
	{0x00, 0x1A},
	{0x00, 0x18},
	{0x00, 0x17},
	{0x00, 0x15},
	{0x00, 0x13},
	{0x00, 0x12},
	{0x00, 0x0F},
	{0x00, 0x0E},
	{0x00, 0x20},
	{0x00, 0x1D},
	{0x00, 0x1C},
	{0x00, 0x1B},
	{0x00, 0x19},
	{0x00, 0x17},
	{0x00, 0x16},
	{0x00, 0x14},
	{0x00, 0x13},
	{0x00, 0x12},
	{0x00, 0x0F},
	{0x00, 0x0E},
	{0x00, 0x20},
	{0x00, 0x1E},
	{0x00, 0x1D},
	{0x00, 0x1A},
	{0x00, 0x19},
	{0x00, 0x17},
	{0x00, 0x16},
	{0x00, 0x15},
	{0x00, 0x12},
	{0x00, 0x11},
	{0x00, 0x0F},
	{0x00, 0x0E},
	{0x00, 0x28},
	{0x00, 0x25},
	{0x00, 0x23},
	{0x00, 0x20},
	{0x00, 0x1E},
	{0x00, 0x1A},
	{0x00, 0x18},
	{0x00, 0x15},
	{0x00, 0x13},
	{0x00, 0x10},
	{0x00, 0x0E},
	{0x00, 0x0E}
};

static const unsigned char aor_cmd[IBRIGHTNESS_MAX][2] = {
	{0x01, 0x7F},	/* IBRIGHTNESS_2NT */
	{0x01, 0x7D},	/* IBRIGHTNESS_3NT */
	{0x01, 0x7A},	/* IBRIGHTNESS_4NT */
	{0x01, 0x78},	/* IBRIGHTNESS_5NT */
	{0x01, 0x75},	/* IBRIGHTNESS_6NT */
	{0x01, 0x72},	/* IBRIGHTNESS_7NT */
	{0x01, 0x6F},	/* IBRIGHTNESS_8NT */
	{0x01, 0x6C},	/* IBRIGHTNESS_9NT */
	{0x01, 0x69},	/* IBRIGHTNESS_10NT */
	{0x01, 0x67},	/* IBRIGHTNESS_11NT */
	{0x01, 0x64},	/* IBRIGHTNESS_12NT */
	{0x01, 0x61},	/* IBRIGHTNESS_13NT */
	{0x01, 0x5E},	/* IBRIGHTNESS_14NT */
	{0x01, 0x5A},	/* IBRIGHTNESS_15NT */
	{0x01, 0x58},	/* IBRIGHTNESS_16NT */
	{0x01, 0x56},	/* IBRIGHTNESS_17NT */
	{0x01, 0x50},	/* IBRIGHTNESS_19NT */
	{0x01, 0x4D},	/* IBRIGHTNESS_20NT */
	{0x01, 0x48},	/* IBRIGHTNESS_21NT */
	{0x01, 0x45},	/* IBRIGHTNESS_22NT */
	{0x01, 0x3F},	/* IBRIGHTNESS_24NT */
	{0x01, 0x3C},	/* IBRIGHTNESS_25NT */
	{0x01, 0x36},	/* IBRIGHTNESS_27NT */
	{0x01, 0x30},	/* IBRIGHTNESS_29NT */
	{0x01, 0x2C},	/* IBRIGHTNESS_30NT */
	{0x01, 0x26},	/* IBRIGHTNESS_32NT */
	{0x01, 0x1F},	/* IBRIGHTNESS_34NT */
	{0x01, 0x16},	/* IBRIGHTNESS_37NT */
	{0x01, 0x0F},	/* IBRIGHTNESS_39NT */
	{0x01, 0x09},	/* IBRIGHTNESS_41NT */
	{0x00, 0xFF},	/* IBRIGHTNESS_44NT */
	{0x00, 0xF5},	/* IBRIGHTNESS_47NT */
	{0x00, 0xEB},	/* IBRIGHTNESS_50NT */
	{0x00, 0xE1},	/* IBRIGHTNESS_53NT */
	{0x00, 0xD7},	/* IBRIGHTNESS_56NT */
	{0x00, 0xC9},	/* IBRIGHTNESS_60NT */
	{0x00, 0xBB},	/* IBRIGHTNESS_64NT */
	{0x00, 0xAD},	/* IBRIGHTNESS_68NT */
	{0x00, 0x99},	/* IBRIGHTNESS_72NT */
	{0x00, 0x99},	/* IBRIGHTNESS_77NT */
	{0x00, 0x99},	/* IBRIGHTNESS_82NT */
	{0x00, 0x99},	/* IBRIGHTNESS_87NT */
	{0x00, 0x99},	/* IBRIGHTNESS_93NT */
	{0x00, 0x99},	/* IBRIGHTNESS_98NT */
	{0x00, 0x99},	/* IBRIGHTNESS_105NT */
	{0x00, 0x99},	/* IBRIGHTNESS_111NT */
	{0x00, 0x99},	/* IBRIGHTNESS_119NT */
	{0x00, 0x99},	/* IBRIGHTNESS_126NT */
	{0x00, 0x99},	/* IBRIGHTNESS_134NT */
	{0x00, 0x99},	/* IBRIGHTNESS_143NT */
	{0x00, 0x99},	/* IBRIGHTNESS_152NT */
	{0x00, 0x99},	/* IBRIGHTNESS_162NT */
	{0x00, 0x99},	/* IBRIGHTNESS_172NT */
	{0x00, 0x86},	/* IBRIGHTNESS_183NT */
	{0x00, 0x71},	/* IBRIGHTNESS_195NT */
	{0x00, 0x5C},	/* IBRIGHTNESS_207NT */
	{0x00, 0x45},	/* IBRIGHTNESS_220NT */
	{0x00, 0x2B},	/* IBRIGHTNESS_234NT */
	{0x00, 0x0E},	/* IBRIGHTNESS_249NT */
	{0x00, 0x0E},	/* IBRIGHTNESS_265NT */
	{0x00, 0x0E},	/* IBRIGHTNESS_282NT */
	{0x00, 0x0E},	/* IBRIGHTNESS_300NT */
	{0x00, 0x0E},	/* IBRIGHTNESS_316NT */
	{0x00, 0x0E},	/* IBRIGHTNESS_333NT */
	{0x00, 0x0E},	/* IBRIGHTNESS_360NT */
	{0x00, 0x0E},	/* IBRIGHTNESS_500NT */
};

static const int offset_gradation[IBRIGHTNESS_MAX][IV_MAX] = {
	/* VT ~ V255 */
	{0, 49, 47, 45, 41, 35, 26, 16, 8, 0},
	{0, 42, 40, 36, 30, 28, 21, 14, 7, 0},
	{0, 34, 31, 27, 22, 18, 16, 10, 6, 0},
	{0, 31, 29, 25, 20, 17, 14, 9, 5, 0},
	{0, 28, 27, 23, 19, 16, 13, 9, 5, 0},
	{0, 26, 24, 19, 15, 13, 11, 8, 5, 0},
	{0, 23, 21, 17, 14, 11, 10, 7, 5, 0},
	{0, 22, 19, 15, 11, 10, 9, 7, 5, 0},
	{0, 19, 17, 13, 10, 9, 8, 6, 4, 0},
	{0, 18, 17, 13, 10, 9, 7, 6, 4, 0},
	{0, 17, 17, 13, 10, 8, 7, 6, 4, 0},
	{0, 16, 16, 13, 10, 8, 7, 6, 4, 0},
	{0, 16, 15, 12, 9, 7, 6, 5, 4, 0},
	{0, 15, 14, 11, 8, 6, 6, 5, 4, 0},
	{0, 14, 14, 10, 7, 6, 6, 5, 4, 0},
	{0, 14, 14, 10, 7, 6, 6, 5, 4, 0},
	{0, 13, 13, 10, 7, 6, 6, 5, 4, 0},
	{0, 12, 12, 9, 6, 5, 5, 4, 4, 0},
	{0, 12, 12, 9, 6, 5, 5, 4, 4, 0},
	{0, 12, 11, 8, 6, 5, 5, 4, 4, 0},
	{0, 12, 11, 8, 5, 4, 4, 4, 4, 0},
	{0, 12, 11, 8, 5, 4, 4, 4, 4, 0},
	{0, 12, 10, 7, 5, 4, 4, 4, 4, 0},
	{0, 11, 10, 7, 5, 4, 4, 4, 4, 0},
	{0, 11, 9, 7, 4, 4, 4, 4, 4, 0},
	{0, 11, 9, 7, 4, 4, 4, 4, 3, 0},
	{0, 11, 8, 6, 4, 4, 4, 4, 3, 0},
	{0, 11, 8, 6, 4, 4, 4, 4, 3, 0},
	{0, 10, 8, 6, 4, 3, 4, 4, 3, 0},
	{0, 9, 8, 5, 3, 3, 4, 4, 3, 0},
	{0, 9, 7, 5, 3, 3, 4, 3, 3, 0},
	{0, 8, 7, 5, 3, 3, 3, 3, 3, 0},
	{0, 8, 7, 5, 3, 3, 3, 3, 3, 0},
	{0, 8, 6, 5, 2, 3, 3, 3, 3, 0},
	{0, 7, 6, 5, 2, 3, 3, 3, 3, 0},
	{0, 6, 6, 4, 2, 3, 3, 3, 3, 0},
	{0, 6, 6, 4, 2, 2, 3, 3, 3, 0},
	{0, 6, 5, 4, 2, 2, 3, 2, 2, 0},
	{0, 5, 5, 4, 2, 2, 3, 2, 2, 0},
	{0, 5, 5, 3, 2, 2, 3, 3, 3, 0},
	{0, 5, 5, 3, 2, 2, 2, 4, 3, 0},
	{0, 5, 4, 3, 3, 2, 3, 3, 1, 0},
	{0, 5, 4, 3, 2, 2, 3, 4, 3, 0},
	{0, 4, 4, 3, 2, 2, 3, 4, 1, 0},
	{0, 4, 5, 3, 3, 2, 3, 3, 1, 0},
	{0, 4, 4, 3, 2, 1, 3, 3, 1, 0},
	{0, 2, 4, 3, 2, 2, 4, 4, 2, 0},
	{0, 2, 4, 3, 2, 2, 4, 4, 2, 0},
	{0, 2, 4, 2, 2, 2, 3, 4, 2, 0},
	{0, 2, 4, 3, 2, 2, 3, 3, 1, 0},
	{0, 2, 3, 2, 2, 2, 4, 4, 2, 0},
	{0, 2, 3, 2, 2, 2, 3, 3, 1, 0},
	{0, 1, 3, 1, 2, 2, 3, 4, 2, 0},
	{0, 1, 3, 1, 2, 2, 3, 4, 2, 0},
	{0, 1, 2, 1, 1, 1, 2, 3, 1, 0},
	{0, 1, 2, 1, 1, 1, 2, 3, 1, 0},
	{0, 1, 1, 0, 1, 1, 2, 2, 1, 0},
	{0, 0, 1, 0, 1, 1, 1, 2, 1, 0},
	{0, 0, 1, 0, 0, 0, 1, 1, 0, 0},
	{0, 0, 1, 0, 0, -1, 0, -1, -1, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, -1, -1, 0, 0, 0, -1, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

static const int offset_color[IBRIGHTNESS_MAX][CI_MAX * IV_MAX] = {
	/* VT ~ V255 */
	{0, 0, 0, 0, 0, 0, -3, 0, -7, 0, 0, -4, -3, 0, -5, -5, 2, -7, -6, 3, -5, -7, 0, -4, -3, 0, -3, -5, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 0, -6, -2, 0, -4, -3, 2, -4, -5, 3, -7, -6, 2, -5, -6, 0, -3, -2, 0, -1, -3, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 1, -6, -2, 1, -5, -4, 2, -4, -5, 3, -6, -6, 2, -4, -5, 0, -3, -1, 0, 0, -1, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 1, -6, -2, 1, -4, -3, 2, -3, -5, 2, -6, -5, 2, -3, -5, 0, -3, -1, 0, 0, -1, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 0, -6, -2, 2, -4, -3, 2, -4, -6, 3, -6, -5, 1, -4, -4, 0, -2, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 0, -6, 0, 2, -5, -4, 2, -5, -6, 3, -5, -5, 1, -4, -3, 0, -1, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -2, 0, -4, -1, 2, -3, -4, 2, -4, -6, 4, -6, -5, 1, -4, -1, 0, -1, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -2, 0, -5, -1, 2, -3, -4, 2, -3, -6, 4, -6, -4, 1, -3, -2, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -2, 0, -4, -1, 2, -3, -4, 2, -3, -7, 4, -6, -2, 1, -3, -2, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -2, 0, -4, -1, 2, -3, -4, 2, -3, -7, 3, -6, -2, 1, -3, -2, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -2, 0, -4, -1, 1, -3, -4, 2, -3, -7, 3, -6, -2, 1, -3, -2, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -2, 0, -4, -1, 2, -3, -5, 2, -3, -6, 2, -5, -2, 1, -3, -2, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -2, 1, -4, -1, 2, -3, -5, 2, -3, -6, 3, -6, -2, 1, -2, -2, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -2, 2, -4, -1, 2, -3, -5, 2, -3, -6, 2, -5, -3, 1, -2, -1, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -2, 2, -4, -1, 2, -2, -5, 1, -3, -6, 2, -5, -3, 1, -2, -1, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 2, -4, 0, 2, -2, -5, 1, -4, -6, 2, -5, -1, 1, -2, -1, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 3, -5, 0, 2, -2, -5, 1, -4, -6, 2, -4, -2, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 4, -5, 0, 2, -2, -5, 1, -4, -5, 2, -4, -1, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 4, -5, 0, 2, -2, -5, 1, -4, -5, 1, -4, -1, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 4, -5, 0, 2, -2, -3, 1, -3, -5, 1, -4, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 4, -5, 0, 2, -2, -3, 1, -3, -5, 1, -4, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 3, -5, -1, 2, -2, -3, 1, -3, -5, 1, -4, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 4, -6, -1, 2, -3, -3, 1, -3, -4, 1, -4, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 4, -6, -1, 2, -3, -3, 1, -2, -3, 1, -3, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 4, -6, -1, 2, -3, -3, 0, -2, -3, 1, -3, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 4, -7, -1, 1, -4, -3, 0, -2, -3, 1, -3, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 4, -7, -2, 1, -4, -2, 0, -2, -3, 1, -3, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 4, -7, -2, 1, -4, -2, 0, -1, -3, 1, -3, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -1, 4, -7, -2, 1, -4, -2, 0, -1, -3, 1, -3, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -2, 4, -8, -2, 1, -4, -2, 0, -1, -2, 1, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -2, 4, -8, -2, 1, -4, -3, 0, -1, -1, 1, -2, 0, 0, -1, -1, 0, 0, 0, 0, 0, 1, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 4, -8, -2, 1, -4, -3, 0, -1, -2, 1, -3, 0, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 3, -9, -2, 1, -4, -3, 0, -1, -2, 1, -3, 0, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 3, -9, -2, 0, -4, -4, 0, -1, -1, 1, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 3, -9, -2, 0, -4, -4, 0, -1, -1, 1, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 1},
	{0, 0, 0, 0, 0, 0, -5, 3, -9, -2, 0, -4, -4, 0, -2, 0, 1, -2, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 3, -9, -2, 0, -4, -4, 0, -2, -1, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 5, -11, -2, 1, -4, -4, 1, -3, -1, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 5, -10, -2, 1, -4, -3, 1, -2, -1, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 5, -10, -2, 1, -3, -2, 1, -2, 0, 1, -2, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 5, -10, -3, 1, -4, -4, 1, -3, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 5, -11, -3, 1, -3, -3, 1, -2, -1, 0, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 5, -10, -4, 1, -4, -3, 1, -2, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 5, -10, -3, 1, -3, -2, 1, -2, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 4, -9, -3, 1, -4, -4, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 4, -10, -2, 1, -3, -2, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -3, 4, -8, -3, 1, -2, -2, 0, -2, -1, 0, -2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 4, -8, -3, 1, -2, -2, 0, -2, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 4, -8, -3, 1, -3, -2, 0, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 3, -8, -3, 1, -3, -2, 0, -2, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 3, -8, -4, 1, -2, -1, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -5, 3, -8, -3, 0, -2, -1, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, -4, 3, -7, -4, 1, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 2, -6, -4, 1, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 2, -5, -2, 0, -2, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 1, -4, -2, 0, -2, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -3, 1, -4, -1, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, -4, 1, -4, 0, 0, -1, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

#endif /* __DYNAMIC_AID_XXXX_H */
