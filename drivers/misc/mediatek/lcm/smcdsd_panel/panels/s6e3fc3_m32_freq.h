#ifndef __S6E3FC3_FREQ_H__
#define __S6E3FC3_FREQ_H__

#include <linux/types.h>
#include <linux/kernel.h>

#define TBL_ARR_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define DEFINE_FREQ_RANGE(_min, _max, _idx)	\
{							\
	.min = _min,			\
	.max = _max,			\
	.freq_idx = _idx,		\
}

#define DEFINE_FREQ_SET(_array)	\
{								\
	.array = _array,			\
	.size = TBL_ARR_SIZE(_array),	\
}

struct dynamic_freq_range {
	int	min;
	int max;
	int freq_idx;
};

struct df_freq_tbl_info {
	struct dynamic_freq_range *array;
	unsigned int size;
};

struct __packed ril_noti_info {
	u8 rat;
	u32 band;
	u32 channel;
};

enum {
	FREQ_RANGE_GSM850 = 1,
	FREQ_RANGE_EGSM900 = 2,
	FREQ_RANGE_DCS1800 = 3,
	FREQ_RANGE_PCS1900 = 4,
	FREQ_RANGE_WB01 = 11,
	FREQ_RANGE_WB02 = 12,
	FREQ_RANGE_WB03 = 13,
	FREQ_RANGE_WB04 = 14,
	FREQ_RANGE_WB05 = 15,
	FREQ_RANGE_WB06 = 16,
	FREQ_RANGE_WB07 = 17,
	FREQ_RANGE_WB08 = 18,
	FREQ_RANGE_WB09 = 19,
	FREQ_RANGE_WB10 = 20,
	FREQ_RANGE_WB11 = 21,
	FREQ_RANGE_WB12 = 22,
	FREQ_RANGE_WB13 = 23,
	FREQ_RANGE_WB14 = 24,
	FREQ_RANGE_WB15 = 25,
	FREQ_RANGE_WB16 = 26,
	FREQ_RANGE_WB17 = 27,
	FREQ_RANGE_WB18 = 28,
	FREQ_RANGE_WB19 = 29,
	FREQ_RANGE_WB20 = 30,
	FREQ_RANGE_WB21 = 31,
	FREQ_RANGE_WB22 = 32,
	FREQ_RANGE_WB23 = 33,
	FREQ_RANGE_WB24 = 34,
	FREQ_RANGE_WB25 = 35,
	FREQ_RANGE_WB26 = 36,
	FREQ_RANGE_WB27 = 37,
	FREQ_RANGE_WB28 = 38,
	FREQ_RANGE_WB29 = 39,
	FREQ_RANGE_WB30 = 40,
	FREQ_RANGE_WB31 = 41,
	FREQ_RANGE_WB32 = 42,
	FREQ_RANGE_TD1 = 51,
	FREQ_RANGE_TD2 = 52,
	FREQ_RANGE_TD3 = 53,
	FREQ_RANGE_TD4 = 54,
	FREQ_RANGE_TD5 = 55,
	FREQ_RANGE_TD6 = 56,
	FREQ_RANGE_BC0 = 61,
	FREQ_RANGE_BC1 = 62,
	FREQ_RANGE_BC2 = 63,
	FREQ_RANGE_BC3 = 64,
	FREQ_RANGE_BC4 = 65,
	FREQ_RANGE_BC5 = 66,
	FREQ_RANGE_BC6 = 67,
	FREQ_RANGE_BC7 = 68,
	FREQ_RANGE_BC8 = 69,
	FREQ_RANGE_BC9 = 70,
	FREQ_RANGE_BC10 = 71,
	FREQ_RANGE_BC11 = 72,
	FREQ_RANGE_BC12 = 73,
	FREQ_RANGE_BC13 = 74,
	FREQ_RANGE_BC14 = 75,
	FREQ_RANGE_BC15 = 76,
	FREQ_RANGE_BC16 = 77,
	FREQ_RANGE_BC17 = 78,
	FREQ_RANGE_BC18 = 79,
	FREQ_RANGE_BC19 = 80,
	FREQ_RANGE_BC20 = 81,
	FREQ_RANGE_BC21 = 82,
	FREQ_RANGE_LB01 = 91,
	FREQ_RANGE_LB02 = 92,
	FREQ_RANGE_LB03 = 93,
	FREQ_RANGE_LB04 = 94,
	FREQ_RANGE_LB05 = 95,
	FREQ_RANGE_LB06 = 96,
	FREQ_RANGE_LB07 = 97,
	FREQ_RANGE_LB08 = 98,
	FREQ_RANGE_LB09 = 99,
	FREQ_RANGE_LB10 = 100,
	FREQ_RANGE_LB11 = 101,
	FREQ_RANGE_LB12 = 102,
	FREQ_RANGE_LB13 = 103,
	FREQ_RANGE_LB14 = 104,
	FREQ_RANGE_LB15 = 105,
	FREQ_RANGE_LB16 = 106,
	FREQ_RANGE_LB17 = 107,
	FREQ_RANGE_LB18 = 108,
	FREQ_RANGE_LB19 = 109,
	FREQ_RANGE_LB20 = 110,
	FREQ_RANGE_LB21 = 111,
	FREQ_RANGE_LB22 = 112,
	FREQ_RANGE_LB23 = 113,
	FREQ_RANGE_LB24 = 114,
	FREQ_RANGE_LB25 = 115,
	FREQ_RANGE_LB26 = 116,
	FREQ_RANGE_LB27 = 117,
	FREQ_RANGE_LB28 = 118,
	FREQ_RANGE_LB29 = 119,
	FREQ_RANGE_LB30 = 120,
	FREQ_RANGE_LB31 = 121,
	FREQ_RANGE_LB32 = 122,
	FREQ_RANGE_LB33 = 123,
	FREQ_RANGE_LB34 = 124,
	FREQ_RANGE_LB35 = 125,
	FREQ_RANGE_LB36 = 126,
	FREQ_RANGE_LB37 = 127,
	FREQ_RANGE_LB38 = 128,
	FREQ_RANGE_LB39 = 129,
	FREQ_RANGE_LB40 = 130,
	FREQ_RANGE_LB41 = 131,
	FREQ_RANGE_LB42 = 132,
	FREQ_RANGE_LB43 = 133,
	FREQ_RANGE_LB44 = 134,
	FREQ_RANGE_LB45 = 135,
	FREQ_RANGE_LB46 = 136,
	FREQ_RANGE_LB47 = 137,
	FREQ_RANGE_LB48 = 138,
	FREQ_RANGE_LB65 = 155,
	FREQ_RANGE_LB66 = 156,
	FREQ_RANGE_LB67 = 157,
	FREQ_RANGE_LB68 = 158,
	FREQ_RANGE_LB69 = 159,
	FREQ_RANGE_LB70 = 160,
	FREQ_RANGE_LB71 = 161,
	FREQ_RANGE_MAX = 162,
};

struct dynamic_freq_range m32_freq_range_GSM850[] = {
	DEFINE_FREQ_RANGE(0, 0, 2),
};

struct dynamic_freq_range m32_freq_range_EGSM900[] = {
	DEFINE_FREQ_RANGE(0, 0, 2),
};

struct dynamic_freq_range m32_freq_range_DCS1800[] = {
	DEFINE_FREQ_RANGE(0, 0, 2),
};

struct dynamic_freq_range m32_freq_range_PCS1900[] = {
	DEFINE_FREQ_RANGE(0, 0, 1),
};

struct dynamic_freq_range m32_freq_range_WB01[] = {
	DEFINE_FREQ_RANGE(10562, 10658, 0),
	DEFINE_FREQ_RANGE(10659, 10728, 1),
	DEFINE_FREQ_RANGE(10729, 10838, 0),
};

struct dynamic_freq_range m32_freq_range_WB02[] = {
	DEFINE_FREQ_RANGE(9662, 9938, 0),
};

struct dynamic_freq_range m32_freq_range_WB03[] = {
	DEFINE_FREQ_RANGE(1162, 1357, 0),
	DEFINE_FREQ_RANGE(1358, 1427, 1),
	DEFINE_FREQ_RANGE(1428, 1513, 0),
};

struct dynamic_freq_range m32_freq_range_WB04[] = {
	DEFINE_FREQ_RANGE(1537, 1633, 0),
	DEFINE_FREQ_RANGE(1634, 1703, 1),
	DEFINE_FREQ_RANGE(1704, 1738, 0),
};

struct dynamic_freq_range m32_freq_range_WB05[] = {
	DEFINE_FREQ_RANGE(4357, 4458, 2),
};

struct dynamic_freq_range m32_freq_range_WB06[] = {
	DEFINE_FREQ_RANGE(4387, 4413, 2),
};

struct dynamic_freq_range m32_freq_range_WB07[] = {
	DEFINE_FREQ_RANGE(2237, 2278, 0),
	DEFINE_FREQ_RANGE(2279, 2348, 1),
	DEFINE_FREQ_RANGE(2349, 2563, 0),
};

struct dynamic_freq_range m32_freq_range_WB08[] = {
	DEFINE_FREQ_RANGE(2937, 3088, 2),
};

struct dynamic_freq_range m32_freq_range_WB19[] = {
	DEFINE_FREQ_RANGE(712, 763, 2),
};

struct dynamic_freq_range m32_freq_range_LB01[] = {
	DEFINE_FREQ_RANGE(0, 217, 0),
	DEFINE_FREQ_RANGE(218, 357, 1),
	DEFINE_FREQ_RANGE(358, 599, 0),
};

struct dynamic_freq_range m32_freq_range_LB02[] = {
	DEFINE_FREQ_RANGE(600, 618, 1),
	DEFINE_FREQ_RANGE(619, 1191, 0),
	DEFINE_FREQ_RANGE(1192, 1199, 1),
};

struct dynamic_freq_range m32_freq_range_LB03[] = {
	DEFINE_FREQ_RANGE(1200, 1615, 0),
	DEFINE_FREQ_RANGE(1616, 1755, 1),
	DEFINE_FREQ_RANGE(1756, 1949, 0),
};

struct dynamic_freq_range m32_freq_range_LB04[] = {
	DEFINE_FREQ_RANGE(1950, 2167, 0),
	DEFINE_FREQ_RANGE(2168, 2307, 1),
	DEFINE_FREQ_RANGE(2308, 2399, 0),
};

struct dynamic_freq_range m32_freq_range_LB05[] = {
	DEFINE_FREQ_RANGE(2400, 2469, 2),
	DEFINE_FREQ_RANGE(2470, 2649, 0),
};

struct dynamic_freq_range m32_freq_range_LB07[] = {
	DEFINE_FREQ_RANGE(2750, 2857, 0),
	DEFINE_FREQ_RANGE(2858, 2997, 1),
	DEFINE_FREQ_RANGE(2998, 3449, 0),
};

struct dynamic_freq_range m32_freq_range_LB08[] = {
	DEFINE_FREQ_RANGE(3450, 3500, 1),
	DEFINE_FREQ_RANGE(3501, 3799, 2),
};

struct dynamic_freq_range m32_freq_range_LB12[] = {
	DEFINE_FREQ_RANGE(5010, 5179, 0),
};

struct dynamic_freq_range m32_freq_range_LB13[] = {
	DEFINE_FREQ_RANGE(5180, 5279, 1),
};

struct dynamic_freq_range m32_freq_range_LB14[] = {
	DEFINE_FREQ_RANGE(5280, 5379, 0),
};

struct dynamic_freq_range m32_freq_range_LB17[] = {
	DEFINE_FREQ_RANGE(5730, 5849, 2),
};

struct dynamic_freq_range m32_freq_range_LB18[] = {
	DEFINE_FREQ_RANGE(5850, 5999, 2),
};

struct dynamic_freq_range m32_freq_range_LB19[] = {
	DEFINE_FREQ_RANGE(6000, 6149, 2),
};

struct dynamic_freq_range m32_freq_range_LB20[] = {
	DEFINE_FREQ_RANGE(6150, 6152, 2),
	DEFINE_FREQ_RANGE(6153, 6449, 1),
};

struct dynamic_freq_range m32_freq_range_LB21[] = {
	DEFINE_FREQ_RANGE(6450, 6532, 1),
	DEFINE_FREQ_RANGE(6533, 6599, 0),
};

struct dynamic_freq_range m32_freq_range_LB25[] = {
	DEFINE_FREQ_RANGE(8040, 8058, 1),
	DEFINE_FREQ_RANGE(8059, 8631, 0),
	DEFINE_FREQ_RANGE(8632, 8689, 1),
};

struct dynamic_freq_range m32_freq_range_LB26[] = {
	DEFINE_FREQ_RANGE(8690, 9039, 2),
};

struct dynamic_freq_range m32_freq_range_LB28[] = {
	DEFINE_FREQ_RANGE(9210, 9402, 0),
	DEFINE_FREQ_RANGE(9403, 9542, 2),
	DEFINE_FREQ_RANGE(9543, 9659, 0),
};

struct dynamic_freq_range m32_freq_range_LB29[] = {
	DEFINE_FREQ_RANGE(9660, 9689, 2),
	DEFINE_FREQ_RANGE(9690, 9769, 0),
};

struct dynamic_freq_range m32_freq_range_LB30[] = {
	DEFINE_FREQ_RANGE(9770, 9866, 1),
	DEFINE_FREQ_RANGE(9867, 9869, 0),
};

struct dynamic_freq_range m32_freq_range_LB32[] = {
	DEFINE_FREQ_RANGE(9920, 10301, 0),
	DEFINE_FREQ_RANGE(10302, 10359, 1),
};

struct dynamic_freq_range m32_freq_range_LB34[] = {
	DEFINE_FREQ_RANGE(36200, 36349, 0),
};

struct dynamic_freq_range m32_freq_range_LB38[] = {
	DEFINE_FREQ_RANGE(37750, 37784, 1),
	DEFINE_FREQ_RANGE(37785, 38249, 0),
};

struct dynamic_freq_range m32_freq_range_LB39[] = {
	DEFINE_FREQ_RANGE(38250, 38628, 0),
	DEFINE_FREQ_RANGE(38629, 38649, 1),
};

struct dynamic_freq_range m32_freq_range_LB40[] = {
	DEFINE_FREQ_RANGE(38650, 39106, 0),
	DEFINE_FREQ_RANGE(39107, 39246, 1),
	DEFINE_FREQ_RANGE(39247, 39649, 0),
};

struct dynamic_freq_range m32_freq_range_LB41[] = {
	DEFINE_FREQ_RANGE(39650, 39712, 1),
	DEFINE_FREQ_RANGE(39713, 40284, 0),
	DEFINE_FREQ_RANGE(40285, 40424, 1),
	DEFINE_FREQ_RANGE(40425, 40997, 0),
	DEFINE_FREQ_RANGE(40998, 41137, 1),
	DEFINE_FREQ_RANGE(41138, 41589, 0),
};

struct dynamic_freq_range m32_freq_range_LB42[] = {
	DEFINE_FREQ_RANGE(41590, 41740, 0),
	DEFINE_FREQ_RANGE(41741, 41880, 1),
	DEFINE_FREQ_RANGE(41881, 42452, 0),
	DEFINE_FREQ_RANGE(42453, 42592, 1),
	DEFINE_FREQ_RANGE(42593, 43165, 0),
	DEFINE_FREQ_RANGE(43166, 43305, 1),
	DEFINE_FREQ_RANGE(43306, 43589, 0),
};

struct dynamic_freq_range m32_freq_range_LB48[] = {
	DEFINE_FREQ_RANGE(55240, 55315, 0),
	DEFINE_FREQ_RANGE(55316, 55455, 1),
	DEFINE_FREQ_RANGE(55456, 56028, 0),
	DEFINE_FREQ_RANGE(56029, 56168, 1),
	DEFINE_FREQ_RANGE(56169, 56739, 0),
};

struct dynamic_freq_range m32_freq_range_LB66[] = {
	DEFINE_FREQ_RANGE(66436, 66653, 0),
	DEFINE_FREQ_RANGE(66654, 66793, 1),
	DEFINE_FREQ_RANGE(66794, 67335, 0),
};

struct dynamic_freq_range m32_freq_range_LB71[] = {
	DEFINE_FREQ_RANGE(68586, 68828, 1),
	DEFINE_FREQ_RANGE(68829, 68935, 2),
};

struct dynamic_freq_range m32_freq_range_TD1[] = {
	DEFINE_FREQ_RANGE(0, 0, 0),
};

struct dynamic_freq_range m32_freq_range_TD2[] = {
	DEFINE_FREQ_RANGE(0, 0, 0),
};

struct dynamic_freq_range m32_freq_range_TD3[] = {
	DEFINE_FREQ_RANGE(0, 0, 1),
};

struct dynamic_freq_range m32_freq_range_TD4[] = {
	DEFINE_FREQ_RANGE(0, 0, 0),
};

struct dynamic_freq_range m32_freq_range_TD5[] = {
	DEFINE_FREQ_RANGE(0, 0, 2),
};

struct dynamic_freq_range m32_freq_range_TD6[] = {
	DEFINE_FREQ_RANGE(0, 0, 1),
};

struct dynamic_freq_range m32_freq_range_BC0[] = {
	DEFINE_FREQ_RANGE(0, 0, 2),
};

struct dynamic_freq_range m32_freq_range_BC1[] = {
	DEFINE_FREQ_RANGE(0, 0, 0),
};

struct dynamic_freq_range m32_freq_range_BC10[] = {
	DEFINE_FREQ_RANGE(0, 0, 2),
};

struct df_freq_tbl_info m32_dynamic_freq_set[FREQ_RANGE_MAX] = {
	[FREQ_RANGE_GSM850] = DEFINE_FREQ_SET(m32_freq_range_GSM850),
	[FREQ_RANGE_EGSM900] = DEFINE_FREQ_SET(m32_freq_range_EGSM900),
	[FREQ_RANGE_DCS1800] = DEFINE_FREQ_SET(m32_freq_range_DCS1800),
	[FREQ_RANGE_PCS1900] = DEFINE_FREQ_SET(m32_freq_range_PCS1900),
	[FREQ_RANGE_WB01] = DEFINE_FREQ_SET(m32_freq_range_WB01),
	[FREQ_RANGE_WB02] = DEFINE_FREQ_SET(m32_freq_range_WB02),
	[FREQ_RANGE_WB03] = DEFINE_FREQ_SET(m32_freq_range_WB03),
	[FREQ_RANGE_WB04] = DEFINE_FREQ_SET(m32_freq_range_WB04),
	[FREQ_RANGE_WB05] = DEFINE_FREQ_SET(m32_freq_range_WB05),
	[FREQ_RANGE_WB06] = DEFINE_FREQ_SET(m32_freq_range_WB06),
	[FREQ_RANGE_WB07] = DEFINE_FREQ_SET(m32_freq_range_WB07),
	[FREQ_RANGE_WB08] = DEFINE_FREQ_SET(m32_freq_range_WB08),
	[FREQ_RANGE_WB19] = DEFINE_FREQ_SET(m32_freq_range_WB19),
	[FREQ_RANGE_LB01] = DEFINE_FREQ_SET(m32_freq_range_LB01),
	[FREQ_RANGE_LB02] = DEFINE_FREQ_SET(m32_freq_range_LB02),
	[FREQ_RANGE_LB03] = DEFINE_FREQ_SET(m32_freq_range_LB03),
	[FREQ_RANGE_LB04] = DEFINE_FREQ_SET(m32_freq_range_LB04),
	[FREQ_RANGE_LB05] = DEFINE_FREQ_SET(m32_freq_range_LB05),
	[FREQ_RANGE_LB07] = DEFINE_FREQ_SET(m32_freq_range_LB07),
	[FREQ_RANGE_LB08] = DEFINE_FREQ_SET(m32_freq_range_LB08),
	[FREQ_RANGE_LB12] = DEFINE_FREQ_SET(m32_freq_range_LB12),
	[FREQ_RANGE_LB13] = DEFINE_FREQ_SET(m32_freq_range_LB13),
	[FREQ_RANGE_LB14] = DEFINE_FREQ_SET(m32_freq_range_LB14),
	[FREQ_RANGE_LB17] = DEFINE_FREQ_SET(m32_freq_range_LB17),
	[FREQ_RANGE_LB18] = DEFINE_FREQ_SET(m32_freq_range_LB18),
	[FREQ_RANGE_LB19] = DEFINE_FREQ_SET(m32_freq_range_LB19),
	[FREQ_RANGE_LB20] = DEFINE_FREQ_SET(m32_freq_range_LB20),
	[FREQ_RANGE_LB21] = DEFINE_FREQ_SET(m32_freq_range_LB21),
	[FREQ_RANGE_LB25] = DEFINE_FREQ_SET(m32_freq_range_LB25),
	[FREQ_RANGE_LB26] = DEFINE_FREQ_SET(m32_freq_range_LB26),
	[FREQ_RANGE_LB28] = DEFINE_FREQ_SET(m32_freq_range_LB28),
	[FREQ_RANGE_LB29] = DEFINE_FREQ_SET(m32_freq_range_LB29),
	[FREQ_RANGE_LB30] = DEFINE_FREQ_SET(m32_freq_range_LB30),
	[FREQ_RANGE_LB32] = DEFINE_FREQ_SET(m32_freq_range_LB32),
	[FREQ_RANGE_LB32] = DEFINE_FREQ_SET(m32_freq_range_LB32),
	[FREQ_RANGE_LB34] = DEFINE_FREQ_SET(m32_freq_range_LB34),
	[FREQ_RANGE_LB38] = DEFINE_FREQ_SET(m32_freq_range_LB38),
	[FREQ_RANGE_LB39] = DEFINE_FREQ_SET(m32_freq_range_LB39),
	[FREQ_RANGE_LB39] = DEFINE_FREQ_SET(m32_freq_range_LB39),
	[FREQ_RANGE_LB40] = DEFINE_FREQ_SET(m32_freq_range_LB40),
	[FREQ_RANGE_LB41] = DEFINE_FREQ_SET(m32_freq_range_LB41),
	[FREQ_RANGE_LB42] = DEFINE_FREQ_SET(m32_freq_range_LB42),
	[FREQ_RANGE_LB48] = DEFINE_FREQ_SET(m32_freq_range_LB48),
	[FREQ_RANGE_LB66] = DEFINE_FREQ_SET(m32_freq_range_LB66),
	[FREQ_RANGE_LB71] = DEFINE_FREQ_SET(m32_freq_range_LB71),
	[FREQ_RANGE_TD1] = DEFINE_FREQ_SET(m32_freq_range_TD1),
	[FREQ_RANGE_TD2] = DEFINE_FREQ_SET(m32_freq_range_TD2),
	[FREQ_RANGE_TD3] = DEFINE_FREQ_SET(m32_freq_range_TD3),
	[FREQ_RANGE_TD4] = DEFINE_FREQ_SET(m32_freq_range_TD4),
	[FREQ_RANGE_TD5] = DEFINE_FREQ_SET(m32_freq_range_TD5),
	[FREQ_RANGE_TD6] = DEFINE_FREQ_SET(m32_freq_range_TD6),
	[FREQ_RANGE_BC0] = DEFINE_FREQ_SET(m32_freq_range_BC0),
	[FREQ_RANGE_BC1] = DEFINE_FREQ_SET(m32_freq_range_BC1),
	[FREQ_RANGE_BC10] = DEFINE_FREQ_SET(m32_freq_range_BC10),
};

#endif /* __S6E3FC3_FREQ_H__ */
