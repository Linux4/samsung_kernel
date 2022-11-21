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
	IBRIGHTNESS_MAX
};

#define VREG_OUT_X1000		6300	/* VREG_OUT x 1000 */

static const int index_voltage_table[IBRIGHTNESS_MAX] =
{
	0,		/* IV_VT */
	3,		/* IV_3 */
	11,		/* IV_11 */
	23,		/* IV_23 */
	35,		/* IV_35 */
	51,		/* IV_51 */
	87,		/* IV_87 */
	151,	/* IV_151 */
	203,	/* IV_203 */
	255		/* IV_255 */
};


static const int index_brightness_table[IBRIGHTNESS_MAX] =
{
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
};

static const int gamma_default_0[IV_MAX*CI_MAX] =
{
	0x00, 0x00, 0x00,		/* IV_VT */
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

static const struct formular_t gamma_formula[IV_MAX] =
{
	{0, 860},	/* IV_VT */
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{72, 860},	/* IV_255 */
};

static const int vt_voltage_value[] =
{0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 138, 148, 158, 168, 178, 186};

static const int brightness_base_table[IBRIGHTNESS_MAX] =
{
	111,	/* IBRIGHTNESS_2NT */
	111,	/* IBRIGHTNESS_3NT */
	111,	/* IBRIGHTNESS_4NT */
	111,	/* IBRIGHTNESS_5NT */
	111,	/* IBRIGHTNESS_6NT */
	111,	/* IBRIGHTNESS_7NT */
	111,	/* IBRIGHTNESS_8NT */
	111,	/* IBRIGHTNESS_9NT */
	111,	/* IBRIGHTNESS_10NT */
	111,	/* IBRIGHTNESS_11NT */
	111,	/* IBRIGHTNESS_12NT */
	111,	/* IBRIGHTNESS_13NT */
	111,	/* IBRIGHTNESS_14NT */
	111,	/* IBRIGHTNESS_15NT */
	111,	/* IBRIGHTNESS_16NT */
	111,	/* IBRIGHTNESS_17NT */
	111,	/* IBRIGHTNESS_19NT */
	111,	/* IBRIGHTNESS_20NT */
	111,	/* IBRIGHTNESS_21NT */
	111,	/* IBRIGHTNESS_22NT */
	111,	/* IBRIGHTNESS_24NT */
	111,	/* IBRIGHTNESS_25NT */
	111,	/* IBRIGHTNESS_27NT */
	111,	/* IBRIGHTNESS_29NT */
	111,	/* IBRIGHTNESS_30NT */
	111,	/* IBRIGHTNESS_32NT */
	111,	/* IBRIGHTNESS_34NT */
	111,	/* IBRIGHTNESS_37NT */
	111,	/* IBRIGHTNESS_39NT */
	111,	/* IBRIGHTNESS_41NT */
	111,	/* IBRIGHTNESS_44NT */
	111,	/* IBRIGHTNESS_47NT */
	111,	/* IBRIGHTNESS_50NT */
	111,	/* IBRIGHTNESS_53NT */
	111,	/* IBRIGHTNESS_56NT */
	111,	/* IBRIGHTNESS_60NT */
	111,	/* IBRIGHTNESS_64NT */
	117,	/* IBRIGHTNESS_68NT */
	124,	/* IBRIGHTNESS_72NT */
	133,	/* IBRIGHTNESS_77NT */
	140,	/* IBRIGHTNESS_82NT */
	148,	/* IBRIGHTNESS_87NT */
	158,	/* IBRIGHTNESS_93NT */
	165,	/* IBRIGHTNESS_98NT */
	175,	/* IBRIGHTNESS_105NT */
	184,	/* IBRIGHTNESS_111NT */
	194,	/* IBRIGHTNESS_119NT */
	206,	/* IBRIGHTNESS_126NT */
	217,	/* IBRIGHTNESS_134NT */
	235,	/* IBRIGHTNESS_143NT */
	248,	/* IBRIGHTNESS_152NT */
	262,	/* IBRIGHTNESS_162NT */
	262,	/* IBRIGHTNESS_172NT */
	262,	/* IBRIGHTNESS_183NT */
	262,	/* IBRIGHTNESS_195NT */
	262,	/* IBRIGHTNESS_207NT */
	262,	/* IBRIGHTNESS_220NT */
	262,	/* IBRIGHTNESS_234NT */
	262,	/* IBRIGHTNESS_249NT */
	277,	/* IBRIGHTNESS_265NT */
	293,	/* IBRIGHTNESS_282NT */
	316,	/* IBRIGHTNESS_300NT */
	328,	/* IBRIGHTNESS_316NT */
	348,	/* IBRIGHTNESS_333NT */
	360,	/* IBRIGHTNESS_360NT */
};

static const int *gamma_curve_tables[IBRIGHTNESS_MAX] =
{
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
	gamma_curve_2p15_table, /* IBRIGHTNESS_316NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_333NT */
	gamma_curve_2p20_table, /* IBRIGHTNESS_350NT */
};

static const int *gamma_curve_lut = gamma_curve_2p20_table;

static const unsigned char aor_cmd[IBRIGHTNESS_MAX][2] =
{
	{0x07 ,0x6C}, /* IBRIGHTNESS_2NT */
	{0x07 ,0x58}, /* IBRIGHTNESS_3NT */
	{0x07 ,0x47}, /* IBRIGHTNESS_4NT */
	{0x07 ,0x36}, /* IBRIGHTNESS_5NT */
	{0x07 ,0x24}, /* IBRIGHTNESS_6NT */
	{0x07 ,0x0F}, /* IBRIGHTNESS_7NT */
	{0x07 ,0x04}, /* IBRIGHTNESS_8NT */
	{0x06 ,0xF3}, /* IBRIGHTNESS_9NT */
	{0x06 ,0xE3}, /* IBRIGHTNESS_10NT */
	{0x06 ,0xD2}, /* IBRIGHTNESS_11NT */
	{0x06 ,0xB9}, /* IBRIGHTNESS_12NT */
	{0x06 ,0xA9}, /* IBRIGHTNESS_13NT */
	{0x06 ,0x96}, /* IBRIGHTNESS_14NT */
	{0x06 ,0x86}, /* IBRIGHTNESS_15NT */
	{0x06 ,0x6E}, /* IBRIGHTNESS_16NT */
	{0x06 ,0x64}, /* IBRIGHTNESS_17NT */
	{0x06 ,0x38}, /* IBRIGHTNESS_19NT */
	{0x06 ,0x2E}, /* IBRIGHTNESS_20NT */
	{0x06 ,0x1A}, /* IBRIGHTNESS_21NT */
	{0x06 ,0x06}, /* IBRIGHTNESS_22NT */
	{0x05 ,0xE7}, /* IBRIGHTNESS_24NT */
	{0x05 ,0xD5}, /* IBRIGHTNESS_25NT */
	{0x05 ,0xB2}, /* IBRIGHTNESS_27NT */
	{0x05 ,0x88}, /* IBRIGHTNESS_29NT */
	{0x05 ,0x77}, /* IBRIGHTNESS_30NT */
	{0x05 ,0x55}, /* IBRIGHTNESS_32NT */
	{0x05 ,0x32}, /* IBRIGHTNESS_34NT */
	{0x04 ,0xF6}, /* IBRIGHTNESS_37NT */
	{0x04 ,0xD5}, /* IBRIGHTNESS_39NT */
	{0x04 ,0xB0}, /* IBRIGHTNESS_41NT */
	{0x04 ,0x74}, /* IBRIGHTNESS_44NT */
	{0x04 ,0x41}, /* IBRIGHTNESS_47NT */
	{0x04 ,0x05}, /* IBRIGHTNESS_50NT */
	{0x03 ,0xC9}, /* IBRIGHTNESS_53NT */
	{0x03 ,0x95}, /* IBRIGHTNESS_56NT */
	{0x03 ,0x3E}, /* IBRIGHTNESS_60NT */
	{0x02 ,0xF8}, /* IBRIGHTNESS_64NT */
	{0x02 ,0xF8}, /* IBRIGHTNESS_68NT */
	{0x02 ,0xF8}, /* IBRIGHTNESS_72NT */
	{0x02 ,0xF8}, /* IBRIGHTNESS_77NT */
	{0x02 ,0xF8}, /* IBRIGHTNESS_82NT */
	{0x02 ,0xF8}, /* IBRIGHTNESS_87NT */
	{0x02 ,0xF8}, /* IBRIGHTNESS_93NT */
	{0x02 ,0xF8}, /* IBRIGHTNESS_98NT */
	{0x02 ,0xF8}, /* IBRIGHTNESS_105NT */
	{0x02 ,0xF8}, /* IBRIGHTNESS_111NT */
	{0x02 ,0xF8}, /* IBRIGHTNESS_119NT */
	{0x02 ,0xF8}, /* IBRIGHTNESS_126NT */
	{0x02 ,0xF8}, /* IBRIGHTNESS_134NT */
	{0x02 ,0xF8}, /* IBRIGHTNESS_143NT */
	{0x02 ,0xF8}, /* IBRIGHTNESS_152NT */
	{0x02 ,0xF8}, /* IBRIGHTNESS_162NT */
	{0x02 ,0xB2}, /* IBRIGHTNESS_172NT */
	{0x02 ,0x55}, /* IBRIGHTNESS_183NT */
	{0x01 ,0xEE}, /* IBRIGHTNESS_195NT */
	{0x01 ,0x92}, /* IBRIGHTNESS_207NT */
	{0x01 ,0x22}, /* IBRIGHTNESS_220NT */
	{0x00 ,0xA7}, /* IBRIGHTNESS_234NT */
	{0x00 ,0x0E}, /* IBRIGHTNESS_249NT */
	{0x00 ,0x0E}, /* IBRIGHTNESS_265NT */
	{0x00 ,0x0E}, /* IBRIGHTNESS_282NT */
	{0x00 ,0x0E}, /* IBRIGHTNESS_300NT */
	{0x00 ,0x0E}, /* IBRIGHTNESS_316NT */
	{0x00 ,0x0E}, /* IBRIGHTNESS_333NT */
	{0x00 ,0x0E}, /* IBRIGHTNESS_360NT */
};

static const unsigned char (*paor_cmd)[2] = aor_cmd;

static const int offset_gradation[IBRIGHTNESS_MAX][IV_MAX] =
{	/* VT ~ V255 */
	{0 ,45 ,43 ,40 ,36 ,32 ,24 ,15 ,9 ,0},		/* IBRIGHTNESS_2NT */
	{0 ,41 ,38 ,34 ,31 ,27 ,22 ,14 ,8 ,0},		/* IBRIGHTNESS_3NT */
	{0 ,35 ,32 ,29 ,25 ,20 ,16 ,10 ,6 ,0},		/* IBRIGHTNESS_4NT */
	{0 ,31 ,28 ,25 ,22 ,18 ,14 ,9 ,5 ,0},		/* IBRIGHTNESS_5NT */
	{0 ,26 ,25 ,22 ,20 ,16 ,13 ,8 ,5 ,0},		/* IBRIGHTNESS_6NT */
	{0 ,23 ,22 ,20 ,17 ,13 ,10 ,6 ,4 ,0},		/* IBRIGHTNESS_7NT */
	{0 ,21 ,20 ,19 ,16 ,14 ,10 ,6 ,4 ,0},		/* IBRIGHTNESS_8NT */
	{0 ,20 ,19 ,17 ,15 ,12 ,9 ,4 ,3 ,0},		/* IBRIGHTNESS_9NT */
	{0 ,19 ,18 ,16 ,14 ,11 ,9 ,4 ,3 ,0},		/* IBRIGHTNESS_10NT */
	{0 ,18 ,18 ,15 ,12 ,10 ,8 ,3 ,3 ,0},		/* IBRIGHTNESS_11NT */
	{0 ,18 ,17 ,14 ,12 ,10 ,7 ,3 ,3 ,0},		/* IBRIGHTNESS_12NT */
	{0 ,18 ,16 ,13 ,11 ,9 ,7 ,3 ,3 ,0},		/* IBRIGHTNESS_13NT */
	{0 ,17 ,15 ,13 ,11 ,8 ,7 ,3 ,3 ,0},		/* IBRIGHTNESS_14NT */
	{0 ,16 ,14 ,12 ,10 ,8 ,6 ,2 ,3 ,0},		/* IBRIGHTNESS_15NT */
	{0 ,15 ,14 ,11 ,9 ,7 ,6 ,2 ,3 ,0},		/* IBRIGHTNESS_16NT */
	{0 ,15 ,13 ,11 ,9 ,7 ,6 ,2 ,2 ,0},		/* IBRIGHTNESS_17NT */
	{0 ,15 ,12 ,10 ,9 ,6 ,5 ,2 ,2 ,0},		/* IBRIGHTNESS_19NT */
	{0 ,12 ,12 ,9 ,8 ,6 ,5 ,2 ,2 ,0},		/* IBRIGHTNESS_20NT */
	{0 ,12 ,12 ,9 ,8 ,6 ,5 ,2 ,2 ,0},		/* IBRIGHTNESS_21NT */
	{0 ,12 ,11 ,9 ,8 ,5 ,5 ,2 ,2 ,0},		/* IBRIGHTNESS_22NT */
	{0 ,11 ,11 ,8 ,7 ,5 ,5 ,2 ,2 ,0},		/* IBRIGHTNESS_24NT */
	{0 ,11 ,11 ,8 ,7 ,5 ,4 ,2 ,2 ,0},		/* IBRIGHTNESS_25NT */
	{0 ,11 ,10 ,8 ,7 ,4 ,4 ,1 ,2 ,0},		/* IBRIGHTNESS_27NT */
	{0 ,10 ,9 ,7 ,6 ,4 ,4 ,1 ,2 ,0},		/* IBRIGHTNESS_29NT */
	{0 ,9 ,9 ,7 ,6 ,4 ,4 ,1 ,2 ,0},		/* IBRIGHTNESS_30NT */
	{0 ,9 ,8 ,6 ,6 ,4 ,4 ,1 ,2 ,0},		/* IBRIGHTNESS_32NT */
	{0 ,8 ,8 ,6 ,5 ,3 ,3 ,1 ,2 ,0},		/* IBRIGHTNESS_34NT */
	{0 ,8 ,8 ,6 ,5 ,3 ,3 ,1 ,2 ,0},		/* IBRIGHTNESS_37NT */
	{0 ,8 ,7 ,5 ,5 ,3 ,3 ,1 ,2 ,0},		/* IBRIGHTNESS_39NT */
	{0 ,7 ,7 ,5 ,4 ,3 ,3 ,1 ,2 ,0},		/* IBRIGHTNESS_41NT */
	{0 ,7 ,6 ,5 ,4 ,3 ,3 ,1 ,2 ,0},		/* IBRIGHTNESS_44NT */
	{0 ,6 ,6 ,5 ,4 ,3 ,3 ,1 ,2 ,0},		/* IBRIGHTNESS_47NT */
	{0 ,6 ,5 ,4 ,3 ,2 ,3 ,0 ,2 ,0},		/* IBRIGHTNESS_50NT */
	{0 ,5 ,5 ,4 ,3 ,2 ,3 ,0 ,2 ,0},		/* IBRIGHTNESS_53NT */
	{0 ,4 ,5 ,4 ,3 ,2 ,2 ,0 ,2 ,0},		/* IBRIGHTNESS_56NT */
	{0 ,5 ,4 ,3 ,3 ,2 ,2 ,0 ,1 ,0},		/* IBRIGHTNESS_60NT */
	{0 ,4 ,4 ,3 ,3 ,2 ,2 ,0 ,1 ,0},		/* IBRIGHTNESS_64NT */
	{0 ,4 ,4 ,2 ,2 ,2 ,3 ,1 ,3 ,0},		/* IBRIGHTNESS_68NT */
	{0 ,6 ,4 ,3 ,2 ,2 ,3 ,3 ,3 ,0},		/* IBRIGHTNESS_72NT */
	{0 ,5 ,4 ,3 ,2 ,1 ,2 ,2 ,2 ,0},		/* IBRIGHTNESS_77NT */
	{0 ,3 ,4 ,3 ,2 ,2 ,3 ,3 ,2 ,0},		/* IBRIGHTNESS_82NT */
	{0 ,3 ,4 ,3 ,3 ,3 ,3 ,3 ,3 ,0},		/* IBRIGHTNESS_87NT */
	{0 ,4 ,4 ,3 ,3 ,3 ,3 ,3 ,3 ,0},		/* IBRIGHTNESS_93NT */
	{0 ,4 ,4 ,3 ,2 ,3 ,3 ,4 ,2 ,0},		/* IBRIGHTNESS_98NT */
	{0 ,4 ,4 ,3 ,3 ,2 ,3 ,3 ,2 ,0},		/* IBRIGHTNESS_105NT */
	{0 ,4 ,3 ,2 ,2 ,2 ,3 ,3 ,2 ,0},		/* IBRIGHTNESS_111NT */
	{0 ,3 ,3 ,3 ,2 ,2 ,4 ,4 ,3 ,0},		/* IBRIGHTNESS_119NT */
	{0 ,4 ,3 ,2 ,3 ,2 ,4 ,4 ,3 ,0},		/* IBRIGHTNESS_126NT */
	{0 ,1 ,4 ,3 ,3 ,3 ,4 ,5 ,3 ,0},		/* IBRIGHTNESS_134NT */
	{0 ,0 ,3 ,2 ,2 ,2 ,4 ,3 ,3 ,0},		/* IBRIGHTNESS_143NT */
	{0 ,0 ,3 ,2 ,2 ,2 ,3 ,4 ,2 ,0},		/* IBRIGHTNESS_152NT */
	{0 ,1 ,3 ,2 ,2 ,2 ,3 ,4 ,2 ,0},		/* IBRIGHTNESS_162NT */
	{0 ,3 ,2 ,1 ,1 ,1 ,3 ,3 ,1 ,0},		/* IBRIGHTNESS_172NT */
	{0 ,1 ,2 ,1 ,1 ,1 ,2 ,3 ,1 ,0},		/* IBRIGHTNESS_183NT */
	{0 ,0 ,2 ,1 ,1 ,1 ,2 ,3 ,1 ,0},		/* IBRIGHTNESS_195NT */
	{0 ,1 ,1 ,1 ,0 ,1 ,2 ,2 ,1 ,0},		/* IBRIGHTNESS_207NT */
	{0 ,0 ,1 ,1 ,0 ,0 ,2 ,2 ,0 ,0},		/* IBRIGHTNESS_220NT */
	{0 ,0 ,1 ,0 ,0 ,0 ,1 ,2 ,0 ,0},		/* IBRIGHTNESS_234NT */
	{0 ,2 ,1 ,1 ,0 ,0 ,1 ,2 ,0 ,0},		/* IBRIGHTNESS_249NT */
	{0 ,2 ,1 ,0 ,0 ,0 ,0 ,0 ,0 ,0},		/* IBRIGHTNESS_265NT */
	{0 ,3 ,0 ,0 ,0 ,0 ,0 ,0 ,-1 ,0},		/* IBRIGHTNESS_282NT */
	{0 ,4 ,0 ,0 ,-1 ,-1 ,-1 ,-1 ,-1 ,0},		/* IBRIGHTNESS_300NT */
	{0 ,0 ,1 ,0 ,0 ,-1 ,-1 ,-1 ,-2 ,0},		/* IBRIGHTNESS_316NT */
	{0 ,1 ,0 ,0 ,0 ,0 ,-1 ,0 ,-2 ,0},		/* IBRIGHTNESS_333NT */
	{0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0},		/* IBRIGHTNESS_360NT */
};

static const struct rgb_t offset_color[IBRIGHTNESS_MAX][IV_MAX] =
{	/* VT ~ V255 */
	{{{0, 0, 0}},{{0, 0, 0}},{{-63, -34, -18}},{{-16, -12, -9}},{{-12, -8, -9}},{{-36, -24, -20}},{{-19, -10, -8}},{{-9, -6, -5}},{{-5, -2, -4}},{{-10, -7, -7}}},		/* IBRIGHTNESS_2NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-45, -33, -26}},{{-5, -3, -2}},{{-22, -16, -15}},{{-27, -16, -13}},{{-14, -10, -6}},{{-14, -8, -11}},{{-4, -3, -4}},{{-7, -5, -4}}},		/* IBRIGHTNESS_3NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-39, -18, -16}},{{-15, -10, -11}},{{-23, -17, -15}},{{-27, -17, -14}},{{-14, -8, -8}},{{-5, -3, -4}},{{-3, -1, -2}},{{-7, -6, -5}}},		/* IBRIGHTNESS_4NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-33, -17, -17}},{{-18, -11, -12}},{{-16, -10, -9}},{{-27, -18, -15}},{{-13, -7, -7}},{{-5, -4, -4}},{{-1, -1, -1}},{{-6, -5, -4}}},		/* IBRIGHTNESS_5NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-38, -22, -14}},{{-20, -14, -14}},{{-13, -7, -8}},{{-20, -11, -9}},{{-9, -6, -6}},{{-3, 0, -1}},{{-1, 0, -1}},{{-8, -8, -7}}},		/* IBRIGHTNESS_6NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-42, -22, -22}},{{-21, -15, -15}},{{-13, -7, -8}},{{-20, -12, -9}},{{-10, -6, -6}},{{-2, 0, -1}},{{-1, 0, -1}},{{-8, -8, -7}}},		/* IBRIGHTNESS_7NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-25, -9, -15}},{{-20, -13, -12}},{{-13, -8, -7}},{{-20, -13, -11}},{{-10, -6, -6}},{{-2, -1, -2}},{{-1, 0, 0}},{{-5, -5, -4}}},		/* IBRIGHTNESS_8NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-32, -17, -22}},{{-8, -3, -2}},{{-19, -13, -13}},{{-17, -10, -8}},{{-10, -6, -7}},{{-2, 0, -1}},{{1, 1, 1}},{{-6, -6, -5}}},		/* IBRIGHTNESS_9NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-25, -10, -17}},{{-17, -12, -10}},{{-15, -8, -9}},{{-15, -10, -7}},{{-10, -7, -8}},{{-1, 0, -1}},{{0, 1, 1}},{{-5, -5, -4}}},		/* IBRIGHTNESS_10NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-36, -20, -26}},{{-17, -13, -11}},{{-10, -4, -4}},{{-16, -11, -9}},{{-7, -4, -4}},{{-2, 0, -1}},{{1, 1, 1}},{{-5, -5, -4}}},		/* IBRIGHTNESS_11NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-35, -18, -24}},{{-14, -9, -8}},{{-18, -11, -11}},{{-14, -9, -6}},{{-7, -4, -5}},{{0, 1, 0}},{{0, 0, 0}},{{-5, -5, -4}}},		/* IBRIGHTNESS_12NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-32, -17, -25}},{{-14, -9, -8}},{{-14, -8, -9}},{{-13, -8, -6}},{{-8, -5, -6}},{{0, 1, 0}},{{1, 1, 1}},{{-5, -5, -4}}},		/* IBRIGHTNESS_13NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-25, -12, -19}},{{-17, -11, -11}},{{-14, -8, -8}},{{-11, -7, -5}},{{-8, -5, -6}},{{0, 1, 0}},{{0, 0, 0}},{{-5, -5, -4}}},		/* IBRIGHTNESS_14NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-26, -12, -21}},{{-15, -10, -9}},{{-10, -5, -5}},{{-13, -8, -6}},{{-9, -6, -6}},{{2, 3, 2}},{{0, 0, 0}},{{-5, -5, -4}}},		/* IBRIGHTNESS_15NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-31, -18, -26}},{{-16, -10, -10}},{{-12, -6, -7}},{{-10, -7, -5}},{{-7, -5, -4}},{{0, 2, 0}},{{0, 0, 0}},{{-6, -6, -5}}},		/* IBRIGHTNESS_16NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-27, -17, -25}},{{-13, -8, -8}},{{-10, -4, -6}},{{-9, -7, -4}},{{-8, -5, -5}},{{-1, 0, -1}},{{2, 2, 2}},{{-5, -5, -4}}},		/* IBRIGHTNESS_17NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-22, -12, -21}},{{-12, -6, -7}},{{-13, -7, -9}},{{-6, -4, -1}},{{-7, -5, -6}},{{1, 2, 1}},{{0, 0, 0}},{{-5, -5, -4}}},		/* IBRIGHTNESS_19NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-25, -15, -23}},{{-12, -8, -8}},{{-10, -4, -6}},{{-9, -7, -4}},{{-5, -3, -4}},{{-1, 0, -1}},{{2, 2, 2}},{{-5, -5, -4}}},		/* IBRIGHTNESS_20NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-22, -12, -25}},{{-11, -8, -5}},{{-9, -3, -10}},{{-6, -4, -2}},{{-6, -4, -5}},{{1, 2, 1}},{{0, 0, 0}},{{-4, -4, -3}}},		/* IBRIGHTNESS_21NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 0, -15}},{{-9, -5, -9}},{{-14, -8, -14}},{{-2, 0, 5}},{{-6, -4, -5}},{{0, 2, 0}},{{1, 1, 1}},{{-4, -4, -3}}},		/* IBRIGHTNESS_22NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-11, -4, -21}},{{-6, -4, -8}},{{-8, -3, -7}},{{-3, 0, 1}},{{-4, -3, -4}},{{-1, 1, -1}},{{1, 1, 1}},{{-3, -3, -2}}},		/* IBRIGHTNESS_24NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-12, -5, -24}},{{-3, -1, -6}},{{-6, -1, -5}},{{-4, -2, -2}},{{-3, -1, -2}},{{0, 1, 0}},{{1, 2, 1}},{{-3, -3, -2}}},		/* IBRIGHTNESS_25NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 3, -17}},{{-4, -2, -8}},{{-7, -3, -8}},{{0, 2, 2}},{{-4, -2, -3}},{{0, 2, 0}},{{1, 1, 1}},{{-2, -2, -1}}},		/* IBRIGHTNESS_27NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 4, -17}},{{-1, 1, -6}},{{-4, 2, -4}},{{1, 2, 1}},{{-3, -1, -2}},{{0, 2, 0}},{{1, 1, 1}},{{-1, -1, 0}}},		/* IBRIGHTNESS_29NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 8, -18}},{{-2, 2, -6}},{{-3, 1, -4}},{{-4, 0, -1}},{{0, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 2}}},		/* IBRIGHTNESS_30NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 8, -17}},{{0, 3, -6}},{{-3, 2, -4}},{{-3, 0, 0}},{{0, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 2}}},		/* IBRIGHTNESS_32NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 7, -16}},{{-1, 3, -6}},{{-3, 2, -4}},{{-2, 0, 0}},{{0, 1, -2}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 2}}},		/* IBRIGHTNESS_34NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-1, 7, -15}},{{-2, 2, -6}},{{-3, 2, -4}},{{-2, 0, 0}},{{0, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 2}}},		/* IBRIGHTNESS_37NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 7, -15}},{{1, 3, -6}},{{-4, 2, -4}},{{-2, 0, 0}},{{0, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 2}}},		/* IBRIGHTNESS_39NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{1, 7, -14}},{{1, 2, -6}},{{-4, 2, -5}},{{-2, 0, 0}},{{0, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 2}}},		/* IBRIGHTNESS_41NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{1, 9, -18}},{{1, 2, -5}},{{-3, 2, -4}},{{-2, 0, 0}},{{0, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 2}}},		/* IBRIGHTNESS_44NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 8, -16}},{{1, 2, -5}},{{-3, 1, -4}},{{-2, 0, 0}},{{0, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 2}}},		/* IBRIGHTNESS_47NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 8, -17}},{{2, 2, -5}},{{-3, 1, -4}},{{-2, 0, 0}},{{0, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 2}}},		/* IBRIGHTNESS_50NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 7, -16}},{{2, 2, -5}},{{-3, 1, -4}},{{-1, 0, 1}},{{0, 0, -2}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 2}}},		/* IBRIGHTNESS_53NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 7, -14}},{{2, 2, -5}},{{-3, 1, -4}},{{-2, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 2}}},		/* IBRIGHTNESS_56NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{1, 8, -16}},{{2, 3, -6}},{{-2, 1, -3}},{{-2, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, -1}},{{1, 0, 2}}},		/* IBRIGHTNESS_60NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 7, -16}},{{3, 2, -6}},{{-2, 1, -4}},{{-2, 0, 1}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, -1}},{{1, 0, 2}}},		/* IBRIGHTNESS_64NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 7, -16}},{{2, 2, -6}},{{-1, 1, -3}},{{-2, 0, 1}},{{0, 0, -1}},{{1, 0, 0}},{{0, 0, 0}},{{0, 0, 1}}},		/* IBRIGHTNESS_68NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 8, -17}},{{2, 2, -4}},{{-2, 1, -3}},{{-2, 0, 1}},{{0, 0, 0}},{{1, 0, -1}},{{0, 0, 0}},{{0, 0, 1}}},		/* IBRIGHTNESS_72NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{1, 8, -16}},{{1, 2, -4}},{{-1, 1, -3}},{{0, 0, 1}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{1, 0, 2}}},		/* IBRIGHTNESS_77NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 7, -14}},{{1, 1, -4}},{{-1, 1, -2}},{{0, 0, 1}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 1}}},		/* IBRIGHTNESS_82NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{1, 6, -13}},{{0, 2, -4}},{{-2, 0, -2}},{{-1, 0, 1}},{{0, 0, -1}},{{1, 0, -1}},{{0, 0, 0}},{{0, 0, 2}}},		/* IBRIGHTNESS_87NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{1, 7, -14}},{{0, 1, -4}},{{-2, 0, -1}},{{-1, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 1}}},		/* IBRIGHTNESS_93NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 6, -14}},{{1, 1, -3}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 1}}},		/* IBRIGHTNESS_98NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 6, -14}},{{0, 1, -4}},{{-1, 0, 0}},{{-1, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 1}}},		/* IBRIGHTNESS_105NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 6, -13}},{{0, 1, -4}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 1}},{{0, 0, 1}}},		/* IBRIGHTNESS_111NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{1, 6, -13}},{{1, 1, -2}},{{-1, 0, 0}},{{-1, 0, 0}},{{1, 0, 0}},{{0, 0, 0}},{{0, 0, 1}},{{0, 0, 1}}},		/* IBRIGHTNESS_119NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{1, 6, -13}},{{0, 1, -2}},{{-1, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 0}}},		/* IBRIGHTNESS_126NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{1, 6, -12}},{{0, 0, -1}},{{-2, 0, 0}},{{-1, 0, 0}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 0}}},		/* IBRIGHTNESS_134NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{1, 5, -12}},{{0, 0, -1}},{{0, 0, 1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 1}}},		/* IBRIGHTNESS_143NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{2, 5, -11}},{{-1, 0, -2}},{{0, 0, 1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 1}}},		/* IBRIGHTNESS_152NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{2, 5, -11}},{{-1, 0, 0}},{{0, 0, 1}},{{0, 0, 0}},{{0, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 1}}},		/* IBRIGHTNESS_162NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{2, 5, -11}},{{1, 0, -1}},{{0, 0, 1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 1}}},		/* IBRIGHTNESS_172NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{2, 5, -11}},{{0, 0, -1}},{{0, 0, 1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 1}}},		/* IBRIGHTNESS_183NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{3, 5, -10}},{{0, 0, -1}},{{0, 0, 1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 1}}},		/* IBRIGHTNESS_195NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{3, 5, -11}},{{0, 0, 0}},{{0, 0, 1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 1}}},		/* IBRIGHTNESS_207NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{3, 5, -10}},{{1, 0, -1}},{{0, 0, 2}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}},		/* IBRIGHTNESS_220NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{3, 4, -9}},{{1, 0, -2}},{{0, 0, 2}},{{0, 0, 0}},{{1, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}},		/* IBRIGHTNESS_234NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}},		/* IBRIGHTNESS_249NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}},		/* IBRIGHTNESS_265NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}},		/* IBRIGHTNESS_282NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}},		/* IBRIGHTNESS_300NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}},		/* IBRIGHTNESS_316NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}},		/* IBRIGHTNESS_333NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}},		/* IBRIGHTNESS_360NT */
};

#ifdef CONFIG_LCD_HMT

enum {
	HMT_SCAN_SINGLE,
	HMT_SCAN_DUAL,
	HMT_SCAN_MAX
};

enum {
	HMT_IBRIGHTNESS_80NT,
	HMT_IBRIGHTNESS_95NT,
	HMT_IBRIGHTNESS_115NT,
	HMT_IBRIGHTNESS_130NT,
	HMT_IBRIGHTNESS_MAX
};

static const int hmt_index_brightness_table[HMT_SCAN_MAX][HMT_IBRIGHTNESS_MAX] =
{
	{
		80,		/* IBRIGHTNESS_80NT */
		95,		/* IBRIGHTNESS_95NT */
		115,	/* IBRIGHTNESS_115NT */
		360,	/* IBRIGHTNESS_130NT */	/*should be fixed for finding max brightness */
	},{
		80,		/* IBRIGHTNESS_80NT */
		95,		/* IBRIGHTNESS_95NT */
		115,	/* IBRIGHTNESS_115NT */
		360,	/* IBRIGHTNESS_130NT */	/*should be fixed for finding max brightness */
	}
};

static const int hmt_brightness_base_table[HMT_SCAN_MAX][HMT_IBRIGHTNESS_MAX] =
{
	{
		250,	/* IBRIGHTNESS_80NT */
		270,	/* IBRIGHTNESS_95NT */
		260,	/* IBRIGHTNESS_115NT */
		288,	/* IBRIGHTNESS_130NT */
	},{
		250,	/* IBRIGHTNESS_80NT */
		270,	/* IBRIGHTNESS_95NT */
		260,	/* IBRIGHTNESS_115NT */
		288,	/* IBRIGHTNESS_130NT */
	}
};

static const int *hmt_gamma_curve_tables[HMT_SCAN_MAX][HMT_IBRIGHTNESS_MAX] =
{
	{
		gamma_curve_2p20_table,	/* IBRIGHTNESS_80NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_95NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_115NT */
		gamma_curve_2p20_table,	/* IBRIGHTNESS_130NT */
	},{
		gamma_curve_2p15_table,	/* IBRIGHTNESS_80NT */
		gamma_curve_2p15_table,	/* IBRIGHTNESS_95NT */
		gamma_curve_2p15_table,	/* IBRIGHTNESS_115NT */
		gamma_curve_2p15_table,	/* IBRIGHTNESS_130NT */
	}
};

static const unsigned char hmt_aor_cmd[HMT_SCAN_MAX][HMT_IBRIGHTNESS_MAX][3] =
{
	{
		{0x05, 0x4C, 0x08}, /* IBRIGHTNESS_80NT */
		{0x05, 0x4C, 0x08}, /* IBRIGHTNESS_95NT */
		{0x04, 0x8A, 0x08}, /* IBRIGHTNESS_115NT */
		{0x04, 0x8A, 0x08}, /* IBRIGHTNESS_130NT */
	},{
		{0x02, 0xC2, 0x08}, /* IBRIGHTNESS_80NT */
		{0x02, 0xC2, 0x08}, /* IBRIGHTNESS_95NT */
		{0x02, 0x5C, 0x08}, /* IBRIGHTNESS_115NT */
		{0x02, 0x5C, 0x08}, /* IBRIGHTNESS_130NT */
	}
};

static const int hmt_offset_gradation[HMT_SCAN_MAX][IBRIGHTNESS_MAX][IV_MAX] =
{	/* V0 ~ V255 */
	{
		{0, 9, 10, 10, 9, 10, 8, 8, 3, 0},
		{0, 7, 9, 7, 8, 8, 7, 7, 3, 0},
		{0, 7, 8, 7, 7, 5, 5, 6, 3, 0},
		{0, 7, 8, 5, 3, 2, 2, 5, 3, 0},
	},{

		{0, 14, 18, 16, 14, 13, 9, 11, 5, 0},
		{0, 13, 15, 11, 12, 11, 7, 9, 5, 0},
		{0, 12, 13, 11, 9, 9, 7, 9, 5, 0},
		{0, 13, 13, 11, 8, 10, 8, 5, 3, 0},
	}
};

static const struct rgb_t hmt_offset_color[HMT_SCAN_MAX][IBRIGHTNESS_MAX][IV_MAX] =
{	/* V0 ~ V255 */
	{
		{{{0, 0, 0}},{{0, 0, 0}},{{6, 10, -6}},{{-5, -1, 1}},{{-1, 1, -3}},{{1, 1, -1}},{{-3, -1, -3}},{{0, 0, 0}},{{1, 0, 0}},{{-3, -2, -3}}},
		{{{0, 0, 0}},{{0, 0, 0}},{{2, 6, -7}},{{2, 3, 4}},{{-2, 1, -3}},{{-1, 1, -3}},{{-1, 0, -1}},{{0, 0, 0}},{{2, 1, 1}},{{0, 1, 2}}},
		{{{0, 0, 0}},{{0, 0, 0}},{{1, 4, -6}},{{1, 2, 4}},{{-4, -2, -6}},{{0, 1, -1}},{{0, 1, 0}},{{0, 1, 0}},{{1, 0, 1}},{{-1, -1, 0}}},
		{{{0, 0, 0}},{{0, 0, 0}},{{-9, -3, -12}},{{0, 1, 1}},{{-2, -1, -5}},{{4, 4, 3}},{{2, 4, 3}},{{2, 1, 0}},{{0, 0, 0}},{{0, 0, 1}}},
	},{

		{{{0, 0, 0}},{{0, 0, 0}},{{3, 6, -11}},{{-5, -3, 3}},{{-2, 1, -6}},{{-2, 0, -5}},{{-1, 1, -1}},{{0, 1, -1}},{{0, 0, 0}},{{-3, -2, -3}}},
		{{{0, 0, 0}},{{0, 0, 0}},{{1, 5, -12}},{{2, 4, 7}},{{-3, 0, -7}},{{-2, 0, -4}},{{0, 3, 0}},{{0, 0, -1}},{{1, 0, 1}},{{0, 1, 2}}},
		{{{0, 0, 0}},{{0, 0, 0}},{{0, 4, -14}},{{-2, 0, 4}},{{-2, 1, -4}},{{0, 1, -2}},{{0, 2, 0}},{{0, 1, -1}},{{0, 0, 0}},{{-1, -1, -1}}},
		{{{0, 0, 0}},{{0, 0, 0}},{{-2, 3, -13}},{{-3, -2, 3}},{{1, 3, -2}},{{1, 2, -1}},{{-1, 0, -1}},{{1, 1, -1}},{{1, 0, 1}},{{-3, -1, -2}}},
	}
};
#endif

#endif /* __DYNAMIC_AID_XXXX_H */
