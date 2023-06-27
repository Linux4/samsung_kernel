/*
 * Battery driver for Marvell 88PM80x PMIC
 *
 * Copyright (c) 2012 Marvell International Ltd.
 * Author:	Yi Zhang <yizhang@marvell.com>
 *		Jett Zhou <jtzhou@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/mfd/88pm80x.h>
#include <linux/delay.h>

#define FG_INTERVAL			(HZ * 1)
#define MONITOR_INTERVAL		(HZ * 60)
#define LOW_BAT_INTERVAL		(HZ * 20)

/* bit definitions of PM800_RTC_MISC6 Register */
#define SLP_CNT_RD_LSB			(1 << 7)

/* bit definitions of PM800_POWER_DOWN_LOG2 Register */
#define HYB_DONE			(1 << 0)

/* bit definitions of GPADC Bias Current 1 Register */
#define GP1_BAIS_SET			(2)

/* GPADC1 Bias Current value in uA unit */
#define GP1_BAIS_SET_UA		((GP1_BAIS_SET) * 5 + 1)

/*
 * We define 3650mV is the low bat voltage;
 * define 3450mV is the power off voltage.
 */
#define LOW_BAT_THRESHOLD		(3650)
#define LOW_BAT_CAP			(15)
#define POWER_OFF_THRESHOLD		(3450)
/*
 * Battery temperature based on NTC resistor, defined
 * corresponding resistor value  -- Ohm / C degeree.
 */
#define TBAT_NEG_25D		127773	/* -25 */
#define TBAT_NEG_10D		54564	/* -10 */
#define TBAT_0D			32330	/* 0 */
#define TBAT_10D		19785	/* 10 */
#define TBAT_20D		12468	/* 20 */
#define TBAT_30D		8072	/* 30 */
#define TBAT_40D		5356	/* 40 */

static int who_is_chg;

struct pm80x_battery_params {
	int status;
	int present;
	int volt;	/* µV */
	int cap;	/* percents: 0~100% */
	int health;
	int tech;
	int temp;
};

struct pm80x_battery_info {
	struct pm80x_chip	*chip;
	struct device	*dev;
	struct pm80x_battery_params	bat_params;

	struct power_supply	battery;
	struct delayed_work	fg_work;
	struct delayed_work	monitor_work;
	struct delayed_work	charged_work;
	struct workqueue_struct *bat_wqueue;
	struct mutex		lock;

	unsigned	present:1;
	int		bat_ntc;
	int irq;
};

static char *pm80x_supply_to[] = {
	"ac",
	"usb",
};

/*
 * Battery parameters from battery vendor for fuel gauge.
 * For SS aruba battery paramters, the extraction was
 * performed at T=25C and with ICHG=IDIS=0.5A, they are:
 * Rs = 0.11 Ohm + 0.01 Ohm;
 * R1 = 0.04 Ohm;
 * R2 = 0.03 Ohm;
 * C1 = 1000F
 * C2 = 10000F
 * Csoc = 5476F (Qtot = 5476C)
 */

/* we use m-Ohm by *1000 for calculation */
static int r1 = 40;
static int r2 = 30;
static int r_s = 120;
static int rtot;
static int c1 = 1000;
static int c2 = 10000;
static int c_soc = 6286;

static int r_off = 240;
/*
 * v1, v2 is voltage, the unit of orignal formula design is "Volt",
 * since we can not do float calculation, we introduce the factor of
 * 1024 * 1024 to enlarge the resolution for LSB, the value of it is
 * "Volt with factor" in the driver.
 *
 * v1 and v2 are signded variable, normally, for charging case they
 * are negative value, for discharging case, they are positive value.
 *
 * v3 is SOC, original formular is 0~1, since we can not do float
 * calculation, for improve the resolution of it, the range value
 * is enlarged  by multiply 10000 in v3_res, but we report
 * v3 = v3_res/1000 to APP/Android level.
 */
static int v1, v2, v3, cap = 100;
static int v1_mv, v2_mv, v3_res;
static int vbat_mv;
/*
 * LUT of exp(-x) function, we need to calcolate exp(-x) with x=SLEEP_CNT/RC,
 * Since 0<SLEEP_CNT<1000 and RCmin=20 we should calculate exp function in
 * [0-50] Because exp(-5)= 0.0067 we can use following approximation:
 *	f(x)= exp(-x) = 0   (if x>5);
 * 20 points in [0-5] with linear interpolation between points.
 *  [-0.25, 0.7788] [-0.50, 0.6065] [-0.75, 0.4724] [-1.00, 0.3679]
 *  [-1.25, 0.2865] [-1.50, 0.2231] [-1.75, 0.1738] [-2.00, 0.1353]
 *  [-2.25, 0.1054] [-2.50, 0.0821] [-2.75, 0.0639] [-3.00, 0.0498]
 *  [-3.25, 0.0388] [-3.50, 0.0302] [-3.75, 0.0235] [-4.00, 0.0183]
 *  [-4.25, 0.0143] [-4.50, 0.0111] [-4.75, 0.0087] [-5.00, 0.0067]
 */
static int exp_data[] = {
	7788, 6065, 4724, 3679,
	2865, 2231, 1738, 1353,
	1054, 821,  639,  498,
	388,  302,  235,  183,
	143,  111,  87,   67 };

static int ocv_dischg[] = {
3429, 3510, 3540, 3558, 3580, 3609, 3622, 3637, 3649, 3657,
3661, 3665, 3670, 3675, 3683, 3692, 3698, 3703, 3709, 3716,
3720, 3726, 3728, 3732, 3734, 3740, 3744, 3749, 3750, 3754,
3760, 3760, 3763, 3764, 3768, 3769, 3771, 3772, 3774, 3776,
3777, 3780, 3782, 3784, 3785, 3789, 3792, 3796, 3798, 3801,
3804, 3808, 3810, 3815, 3818, 3824, 3829, 3833, 3840, 3843,
3849, 3854, 3862, 3868, 3874, 3880, 3887, 3892, 3898, 3904,
3910, 3916, 3922, 3929, 3934, 3941, 3946, 3954, 3962, 3968,
3976, 3982, 3987, 3995, 4006, 4013, 4019, 4028, 4035, 4044,
4053, 4060, 4070, 4081, 4088, 4102, 4107, 4122, 4136, 4154, 4170};

static int ocv_chg[] = {
3429, 3510, 3540, 3558, 3580, 3609, 3622, 3637, 3649, 3657,
3661, 3665, 3670, 3675, 3683, 3692, 3698, 3703, 3709, 3716,
3720, 3726, 3728, 3732, 3734, 3740, 3744, 3749, 3750, 3754,
3760, 3760, 3763, 3764, 3768, 3769, 3771, 3772, 3774, 3776,
3777, 3780, 3782, 3784, 3785, 3789, 3792, 3796, 3798, 3801,
3804, 3808, 3810, 3815, 3818, 3824, 3829, 3833, 3840, 3843,
3849, 3854, 3862, 3868, 3874, 3880, 3887, 3892, 3898, 3904,
3910, 3916, 3922, 3929, 3934, 3941, 3946, 3954, 3962, 3968,
3976, 3982, 3987, 3995, 4006, 4013, 4019, 4028, 4035, 4044,
4053, 4060, 4070, 4081, 4088, 4102, 4107, 4122, 4136, 4154, 4170};

/*
 * The rtot(Rtotal) of battery is much different in different temperature,
 * so we introduced data in different temperature, the typical sample point
 * of temperature are -5/10/25/40 C.
 * For charging case, we support the data of charging current of 800/1200 mA
 * in those temperatures, so we have 8 tables for it.
 * For discharging case, we have data 300/500/700/1000/1500 mA discharging
 * current case, then we have 20 tables for it.
 */
static int rtot_tm5_i0p3[] = {
4130, 4130, 4130, 4130, 4130, 4130, 4130, 3530, 2850, 2410,
2100, 1840, 1640, 1490, 1370, 1280, 1190, 1120, 1050, 993,
938, 894, 854, 818, 784, 751, 722, 695, 670, 649,
629, 612, 597, 585, 575, 567, 560, 554, 549, 547,
544, 541, 540, 539, 537, 536, 535, 534, 533, 532,
532, 533, 534, 538, 545, 554, 564, 577, 591, 603,
612, 620, 626, 630, 632, 634, 636, 637, 638, 639,
640, 641, 643, 643, 640, 635, 629, 621, 616, 612,
609, 605, 602, 595, 587, 580, 573, 568, 562, 555,
546, 537, 528, 518, 506, 505, 505, 505, 505, 505};

static int rtot_tm5_i0p5[] = {
2580, 2580, 2580, 2580, 2580, 2580, 2580, 2580, 2580, 2580,
2580, 2580, 2580, 2580, 2330, 2010, 1780, 1600, 1460, 1340,
1240, 1150, 1080, 1010, 946, 891, 843, 800, 761, 727,
697, 670, 646, 626, 609, 593, 580, 568, 558, 551,
544, 537, 533, 529, 526, 523, 521, 519, 518, 516,
516, 516, 516, 517, 519, 522, 527, 533, 541, 548,
555, 560, 564, 567, 569, 569, 570, 571, 570, 570,
571, 570, 570, 572, 571, 569, 567, 565, 562, 557,
552, 551, 546, 544, 541, 535, 530, 525, 518, 512,
506, 498, 492, 492, 492, 492, 492, 492, 492, 492};

static int rtot_tm5_i0p7[] = {
1860, 1860, 1860, 1860, 1860, 1860, 1860, 1860, 1860, 1860,
1860, 1860, 1860, 1720, 1510, 1360, 1260, 1180, 1110, 1060,
1010, 964, 924, 886, 850, 817, 785, 757, 729, 705,
681, 661, 643, 625, 610, 596, 584, 573, 562, 553,
545, 538, 532, 527, 523, 518, 514, 512, 510, 507,
507, 506, 507, 508, 511, 515, 520, 525, 530, 535,
542, 546, 546, 546, 547, 548, 549, 549, 550, 550,
552, 553, 555, 557, 557, 558, 558, 558, 558, 556,
556, 554, 552, 551, 548, 546, 542, 538, 534, 534,
534, 534, 534, 534, 534, 534, 534, 534, 534, 534};

static int rtot_tm5_i1p0[] = {
1410, 1410, 1410, 1410, 1410, 1410, 1410, 1410, 1410, 1410,
1410, 1410, 1410, 1410, 1410, 1410, 1410, 1410, 1410, 1410,
1410, 1410, 1410, 1410, 1410, 1410, 1410, 1410, 1410, 1410,
1410, 1410, 1410, 1410, 1410, 1410, 1290, 1130, 1020, 928,
858, 802, 757, 719, 688, 659, 634, 611, 591, 575,
561, 549, 539, 531, 524, 520, 517, 516, 516, 518,
519, 521, 522, 522, 523, 524, 525, 526, 526, 527,
527, 527, 527, 528, 529, 529, 530, 530, 530, 530,
529, 529, 528, 527, 527, 527, 527, 527, 527, 527,
527, 527, 527, 527, 527, 527, 527, 527, 527, 527};

static int rtot_tm5_i1p5[] = {
1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
1070, 1070, 1070, 1070, 1070, 983, 849, 758, 697, 653,
621, 597, 580, 566, 556, 549, 543, 543, 543, 543,
543, 543, 543, 543, 543, 543, 543, 543, 543, 543,
543, 543, 543, 543, 543, 543, 543, 543, 543, 543};

static int rtot_tm5_i0p8c[] = {
374, 374, 374, 374, 374, 374, 374, 374, 374, 374,
374, 374, 374, 374, 374, 374, 374, 376, 380, 381,
381, 385, 390, 396, 401, 406, 410, 416, 423, 428,
435, 441, 445, 452, 458, 462, 468, 473, 478, 483,
488, 493, 498, 503, 508, 514, 520, 525, 527, 531,
535, 539, 541, 541, 537, 531, 528, 527, 528, 536,
541, 546, 550, 554, 558, 562, 565, 568, 571, 575,
578, 581, 584, 588, 593, 598, 605, 615, 625, 637,
651, 667, 687, 710, 737, 771, 815, 868, 936, 1030,
1130, 1230, 1320, 1390, 1430, 1430, 1430, 1430, 1430, 1430};

static int rtot_tm5_i1p2c[] = {
390, 390, 390, 390, 390, 390, 390, 390, 390, 390,
390, 390, 390, 390, 390, 390, 390, 390, 390, 390,
390, 390, 390, 390, 390, 390, 390, 390, 390, 390,
390, 390, 390, 390, 390, 390, 390, 390, 390, 390,
390, 390, 390, 390, 390, 390, 390, 390, 390, 390,
390, 390, 390, 390, 390, 390, 390, 390, 390, 390,
390, 390, 390, 390, 390, 390, 390, 390, 390, 390,
390, 390, 390, 390, 390, 390, 390, 390, 390, 390,
390, 399, 408, 415, 422, 428, 434, 441, 447, 456,
467, 481, 499, 522, 551, 595, 654, 745, 782, 782};

static int rtot_t10_i0p3[] = {
3620, 3620, 3620, 3620, 2430, 1670, 1310, 1110, 984, 892,
805, 720, 636, 568, 518, 475, 446, 424, 400, 375,
355, 342, 333, 325, 316, 308, 300, 293, 286, 280,
275, 271, 269, 267, 267, 267, 266, 266, 266, 266,
265, 265, 264, 263, 262, 262, 261, 260, 260, 260,
260, 262, 264, 268, 275, 284, 294, 307, 319, 330,
338, 343, 346, 347, 346, 344, 341, 337, 333, 328,
324, 319, 314, 309, 302, 297, 292, 288, 285, 282,
279, 276, 272, 269, 267, 264, 262, 258, 256, 253,
250, 247, 242, 238, 233, 233, 233, 233, 233, 233};

static int rtot_t10_i0p5[] = {
2390, 2390, 2390, 2390, 2390, 2390, 2390, 1860, 1330, 1060,
892, 777, 692, 628, 580, 544, 507, 473, 442, 413,
388, 369, 356, 345, 335, 326, 317, 308, 300, 293,
286, 281, 276, 272, 269, 267, 265, 264, 263, 262,
262, 261, 261, 260, 260, 259, 258, 257, 256, 256,
255, 254, 255, 256, 258, 261, 266, 272, 279, 287,
293, 298, 302, 304, 305, 305, 305, 304, 303, 302,
301, 300, 298, 296, 293, 290, 287, 283, 279, 277,
274, 271, 268, 264, 262, 259, 256, 252, 249, 246,
243, 240, 237, 237, 237, 237, 237, 237, 237, 237};

static int rtot_t10_i0p7[] = {
1760, 1760, 1760, 1760, 1760, 1760, 1710, 1310, 1060, 888,
768, 678, 610, 560, 522, 485, 452, 423, 396, 373,
356, 343, 334, 325, 318, 311, 304, 298, 292, 286,
281, 276, 272, 269, 266, 263, 261, 260, 258, 257,
256, 255, 254, 254, 253, 252, 251, 250, 249, 248,
248, 248, 248, 249, 252, 255, 259, 264, 269, 273,
277, 280, 281, 282, 283, 282, 282, 282, 282, 281,
280, 280, 279, 278, 276, 274, 272, 270, 268, 266,
265, 263, 260, 258, 255, 253, 250, 247, 245, 245,
245, 245, 245, 245, 245, 245, 245, 245, 245, 245};

static int rtot_t10_i1p0[] = {
1310, 1310, 1310, 1310, 1310, 1310, 1310, 1310, 1310, 1310,
1310, 1310, 1310, 1310, 1260, 1040, 882, 775, 695, 631,
579, 536, 501, 470, 444, 420, 399, 380, 365, 351,
338, 328, 319, 311, 303, 298, 293, 288, 284, 281,
278, 276, 274, 273, 271, 270, 269, 269, 268, 267,
267, 267, 267, 267, 268, 269, 271, 273, 276, 280,
282, 285, 286, 286, 286, 286, 286, 285, 285, 284,
283, 282, 281, 280, 279, 277, 276, 274, 272, 270,
268, 266, 264, 262, 261, 261, 261, 261, 261, 261,
261, 261, 261, 261, 261, 261, 261, 261, 261, 261};

static int rtot_t10_i1p5[] = {
950, 950, 950, 950, 950, 950, 950, 950, 950, 950,
950, 950, 950, 950, 950, 950, 950, 950, 950, 950,
950, 950, 950, 950, 950, 950, 950, 950, 950, 950,
950, 950, 834, 713, 624, 559, 509, 471, 439, 414,
393, 374, 359, 345, 334, 323, 314, 306, 300, 294,
290, 287, 284, 282, 281, 281, 282, 283, 284, 286,
288, 290, 291, 292, 292, 293, 293, 292, 292, 292,
291, 290, 290, 289, 288, 287, 287, 287, 287, 287,
287, 287, 287, 287, 287, 287, 287, 287, 287, 287,
287, 287, 287, 287, 287, 287, 287, 287, 287, 287};

static int rtot_t10_i0p8c[] = {
162, 162, 162, 162, 162, 162, 162, 162, 162, 162,
162, 162, 162, 162, 162, 162, 162, 162, 162, 162,
162, 162, 164, 172, 180, 187, 194, 200, 205, 209,
212, 215, 218, 220, 223, 224, 226, 227, 228, 229,
230, 230, 230, 231, 231, 231, 231, 230, 230, 229,
228, 226, 223, 219, 212, 203, 196, 192, 190, 189,
187, 187, 187, 187, 187, 187, 187, 187, 187, 188,
188, 188, 188, 189, 190, 191, 192, 194, 196, 198,
199, 201, 203, 205, 207, 209, 214, 214, 213, 211,
207, 204, 198, 192, 183, 172, 156, 137, 111, 77};

static int rtot_t10_i1p2c[] = {
224, 224, 224, 224, 224, 224, 224, 224, 224, 224,
224, 224, 224, 224, 224, 224, 224, 224, 224, 224,
224, 224, 224, 224, 224, 224, 224, 224, 224, 224,
224, 224, 224, 224, 224, 224, 224, 224, 224, 224,
224, 224, 224, 224, 224, 224, 224, 224, 224, 224,
224, 225, 227, 228, 228, 226, 223, 222, 222, 223,
225, 227, 229, 232, 234, 237, 240, 242, 245, 247,
250, 252, 254, 256, 263, 268, 271, 274, 276, 279,
281, 283, 285, 287, 290, 293, 296, 300, 305, 311,
318, 325, 335, 346, 359, 376, 399, 430, 479, 516};

static int rtot_t25_i0p3[] = {
3180, 2870, 1260, 925, 711, 587, 531, 500, 462, 411,
354, 305, 272, 264, 277, 283, 276, 265, 253, 241,
232, 229, 227, 226, 223, 220, 216, 212, 208, 204,
201, 198, 197, 195, 195, 194, 193, 192, 190, 189,
187, 185, 184, 183, 182, 181, 180, 178, 177, 177,
177, 178, 180, 183, 188, 196, 205, 215, 225, 232,
236, 236, 234, 229, 224, 220, 215, 211, 207, 204,
201, 198, 196, 194, 191, 189, 187, 185, 183, 182,
180, 179, 177, 176, 175, 174, 173, 171, 171, 170,
168, 167, 166, 164, 163, 163, 163, 163, 163, 163};

static int rtot_t25_i0p5[] = {
2240, 2240, 2240, 2240, 2240, 1720, 1080, 829, 696, 618,
561, 508, 455, 406, 369, 344, 320, 302, 287, 273,
258, 247, 240, 235, 231, 226, 221, 216, 212, 207,
203, 199, 196, 193, 192, 190, 190, 189, 189, 189,
188, 188, 187, 186, 185, 184, 183, 182, 181, 180,
180, 179, 180, 181, 183, 186, 191, 196, 203, 210,
216, 220, 223, 224, 224, 224, 222, 220, 218, 216,
213, 211, 209, 206, 204, 201, 199, 196, 193, 190,
188, 186, 183, 181, 179, 177, 175, 173, 171, 169,
167, 165, 163, 163, 163, 163, 163, 163, 163, 163};

static int rtot_t25_i0p7[] = {
1460, 1460, 1220, 712, 545, 454, 410, 387, 369, 349,
322, 293, 266, 250, 244, 240, 238, 236, 231, 225,
221, 218, 216, 214, 211, 208, 204, 201, 198, 195,
192, 189, 187, 185, 184, 183, 182, 181, 180, 180,
179, 178, 177, 175, 174, 173, 172, 171, 170, 169,
168, 168, 169, 170, 172, 175, 179, 183, 188, 192,
195, 197, 198, 198, 197, 196, 195, 194, 192, 191,
189, 188, 186, 185, 183, 181, 180, 178, 176, 174,
173, 171, 170, 168, 166, 165, 163, 162, 161, 161,
161, 161, 161, 161, 161, 161, 161, 161, 161, 161};

static int rtot_t25_i1p0[] = {
1250, 1250, 1250, 1250, 1250, 1250, 1250, 1250, 1120, 798,
632, 529, 461, 415, 384, 364, 344, 324, 304, 284,
266, 253, 245, 239, 234, 229, 224, 219, 214, 210,
206, 202, 199, 196, 193, 191, 189, 187, 186, 185,
184, 183, 183, 183, 182, 182, 181, 181, 180, 180,
179, 179, 178, 178, 179, 180, 182, 185, 188, 191,
194, 196, 197, 198, 198, 198, 197, 197, 196, 195,
194, 193, 192, 192, 191, 189, 188, 186, 185, 183,
181, 179, 178, 176, 175, 175, 175, 175, 175, 175,
175, 175, 175, 175, 175, 175, 175, 175, 175, 175};

static int rtot_t25_i1p5[] = {
877, 877, 877, 877, 877, 877, 877, 877, 877, 877,
877, 877, 856, 686, 576, 502, 447, 407, 377, 352,
331, 312, 296, 282, 270, 260, 251, 243, 236, 230,
225, 220, 215, 211, 208, 204, 201, 199, 196, 194,
192, 190, 189, 187, 186, 185, 184, 184, 184, 183,
183, 183, 183, 183, 183, 184, 185, 187, 189, 191,
193, 194, 195, 195, 195, 195, 194, 194, 193, 192,
191, 191, 190, 189, 188, 188, 187, 187, 187, 187,
187, 187, 187, 187, 187, 187, 187, 187, 187, 187,
187, 187, 187, 187, 187, 187, 187, 187, 187, 187};

static int rtot_t25_i0p8c[] = {
112, 112, 112, 112, 112, 112, 112, 112, 112, 112,
112, 112, 112, 112, 112, 112, 112, 112, 113, 113,
113, 118, 125, 133, 139, 144, 149, 153, 156, 158,
160, 162, 163, 165, 167, 168, 169, 169, 170, 170,
171, 171, 171, 172, 172, 172, 172, 171, 171, 170,
169, 168, 166, 162, 155, 146, 140, 137, 135, 134,
133, 133, 133, 133, 134, 134, 134, 134, 134, 134,
134, 134, 133, 133, 133, 134, 134, 135, 136, 136,
136, 137, 137, 138, 139, 140, 141, 142, 143, 144,
146, 146, 143, 138, 132, 124, 113, 99, 79, 55};

static int rtot_t25_i1p2c[] = {
152, 152, 152, 152, 152, 152, 152, 152, 152, 152,
152, 152, 152, 152, 152, 152, 152, 152, 152, 152,
152, 152, 152, 152, 152, 152, 152, 152, 152, 152,
152, 152, 155, 158, 160, 163, 165, 167, 168, 170,
171, 172, 172, 173, 173, 173, 174, 174, 174, 174,
173, 173, 172, 171, 169, 165, 160, 156, 154, 154,
154, 154, 155, 156, 157, 158, 159, 161, 162, 163,
165, 166, 168, 169, 170, 172, 173, 175, 177, 179,
181, 183, 187, 192, 194, 197, 200, 203, 206, 209,
213, 217, 222, 227, 233, 240, 250, 261, 278, 303};

static int rtot_t40_i0p3[] = {
3320, 3320, 1340, 938, 689, 525, 437, 383, 329, 283,
252, 230, 218, 227, 252, 270, 269, 260, 249, 236,
226, 223, 222, 219, 214, 207, 201, 195, 189, 184,
180, 177, 175, 174, 173, 171, 170, 168, 166, 165,
163, 161, 160, 159, 157, 156, 154, 153, 152, 152,
152, 153, 155, 158, 164, 171, 179, 187, 193, 195,
194, 191, 188, 184, 181, 179, 176, 174, 173, 172,
171, 170, 170, 169, 168, 167, 166, 164, 163, 162,
161, 160, 159, 158, 157, 156, 156, 155, 154, 154,
153, 152, 151, 151, 151, 151, 151, 151, 151, 151};

static int rtot_t40_i0p5[] = {
2280, 2280, 2280, 2280, 1890, 1000, 743, 606, 531, 484,
444, 397, 345, 300, 276, 266, 260, 256, 252, 241,
228, 218, 212, 208, 205, 201, 196, 192, 187, 183,
179, 175, 172, 170, 169, 168, 168, 167, 167, 166,
166, 165, 164, 163, 163, 162, 161, 159, 158, 158,
157, 157, 157, 158, 160, 163, 167, 172, 178, 183,
188, 190, 191, 192, 191, 190, 188, 187, 185, 183,
182, 180, 179, 178, 177, 176, 175, 173, 171, 170,
168, 167, 166, 164, 163, 161, 160, 159, 157, 156,
155, 154, 153, 153, 153, 153, 153, 153, 153, 153};

static int rtot_t40_i0p7[] = {
1490, 1490, 799, 557, 434, 358, 316, 291, 270, 250,
228, 208, 195, 192, 197, 201, 204, 202, 197, 191,
187, 185, 184, 182, 180, 177, 174, 171, 169, 166,
164, 162, 160, 159, 158, 157, 157, 156, 155, 154,
153, 152, 151, 150, 148, 147, 146, 145, 144, 143,
143, 143, 143, 144, 146, 149, 153, 158, 162, 166,
168, 169, 170, 169, 168, 167, 166, 164, 163, 162,
161, 160, 159, 158, 157, 156, 154, 153, 152, 151,
150, 149, 148, 147, 146, 145, 144, 143, 142, 142,
142, 142, 142, 142, 142, 142, 142, 142, 142, 142};

static int rtot_t40_i1p0[] = {
1190, 1190, 1190, 1190, 1190, 1190, 717, 535, 445, 391,
355, 326, 299, 276, 258, 245, 232, 222, 215, 208,
202, 197, 193, 190, 188, 185, 181, 178, 175, 172,
169, 167, 164, 162, 161, 159, 158, 157, 157, 156,
156, 156, 156, 155, 155, 154, 154, 153, 152, 152,
151, 150, 150, 150, 151, 152, 154, 157, 160, 163,
166, 168, 170, 171, 171, 171, 171, 170, 169, 168,
168, 167, 166, 165, 164, 163, 162, 161, 159, 158,
156, 155, 153, 152, 151, 151, 151, 151, 151, 151,
151, 151, 151, 151, 151, 151, 151, 151, 151, 151};

static int rtot_t40_i1p5[] = {
829, 829, 829, 829, 829, 829, 829, 829, 583, 455,
383, 338, 306, 284, 268, 257, 246, 234, 224, 214,
206, 201, 197, 195, 192, 189, 186, 183, 180, 177,
174, 171, 168, 166, 164, 162, 160, 159, 157, 156,
156, 155, 154, 154, 154, 154, 153, 153, 153, 152,
152, 152, 151, 151, 152, 153, 154, 155, 157, 160,
161, 163, 164, 164, 164, 164, 164, 163, 163, 162,
162, 161, 161, 160, 160, 159, 158, 158, 158, 158,
158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
158, 158, 158, 158, 158, 158, 158, 158, 158, 158};

static int rtot_t40_i0p8c[] = {
85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
85, 85, 85, 85, 85, 85, 85, 85, 88, 89,
89, 95, 102, 107, 111, 115, 118, 120, 122, 124,
126, 128, 129, 131, 132, 133, 134, 135, 136, 137,
138, 139, 140, 141, 142, 143, 145, 146, 147, 148,
148, 147, 146, 143, 137, 129, 124, 121, 120, 120,
120, 120, 121, 122, 123, 124, 125, 125, 126, 127,
128, 128, 129, 129, 130, 130, 131, 132, 134, 135,
136, 137, 138, 139, 140, 141, 142, 143, 145, 146,
148, 151, 152, 152, 151, 150, 147, 143, 134, 117};

static int rtot_t40_i1p2c[] = {
118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
118, 118, 118, 118, 118, 118, 118, 120, 122, 124,
126, 128, 130, 132, 133, 135, 136, 138, 139, 139,
140, 141, 141, 141, 141, 142, 141, 141, 141, 141,
141, 141, 140, 139, 137, 133, 128, 124, 122, 121,
121, 121, 122, 122, 123, 124, 125, 126, 127, 128,
129, 129, 130, 131, 132, 133, 134, 135, 136, 137,
139, 140, 141, 143, 144, 145, 148, 151, 152, 153,
154, 155, 156, 157, 156, 156, 155, 152, 147, 137};

static int *dis_chg_rtot[5][4] = {
	{rtot_tm5_i0p3, rtot_t10_i0p3, rtot_t25_i0p3, rtot_t40_i0p3},
	{rtot_tm5_i0p5, rtot_t10_i0p5, rtot_t25_i0p5, rtot_t40_i0p5},
	{rtot_tm5_i0p7, rtot_t10_i0p7, rtot_t25_i0p7, rtot_t40_i0p7},
	{rtot_tm5_i1p0, rtot_t10_i1p0, rtot_t25_i1p0, rtot_t40_i1p0},
	{rtot_tm5_i1p5, rtot_t10_i1p5, rtot_t25_i1p5, rtot_t40_i1p5}
};

static int dischg_ib[5] = {300, 500, 700, 1000, 1500};

static int *chg_rtot[2][4] = {
	{rtot_tm5_i0p8c, rtot_t10_i0p8c, rtot_t25_i0p8c, rtot_t40_i0p8c},
	{rtot_tm5_i1p2c, rtot_t10_i1p2c, rtot_t25_i1p2c, rtot_t40_i1p2c}
};

static int chg_ib[2] = {800, 1200};

/*
 * Calculate the rtot according to different ib and temp info, we think about
 * ib is y coordinate, temp is x coordinate to do interpolation.
 * eg. dischg case, ib are 300/500/700/1000/1500mA, temp are -5/10/25/40C.
 * We divide them into 3 kind of case to interpolate, they are 1/2/4 point
 * case, the refered point is top-right.
 *
 *        |-5C       |10C        |25C         |40C
 *        |          |           |            |    1 point
 * -------|----------|-----------|------------|------------- 1500mA
 *        |          |           | 4 point    |    2 point
 * -------|----------|-----------|------------|------------- 1000mA
 *        |          |           |            |
 * -------|----------|-----------|------------|------------- 700mA
 *        |          |           |            |
 * -------|----------|-----------|------------|------------- 500mA
 *        |          |           |            |
 * -------|----------|-----------|------------|------------- 300mA
 *        |          |           |            |
 *        |	     |           |            |
 */

static int calc_resistor(struct pm80x_battery_info *info, int temp, int ib)
{
	int x1, x2;
	int y1, y2;
	int p_num = 4;
	int i, j;

	dev_dbg(info->dev, "---> %s ib : %d temp: %d\n", __func__, ib, temp);
	if (info->bat_params.status == POWER_SUPPLY_STATUS_DISCHARGING) {
		for (i = 0; i < 5; i++) {
			if (ib <= dischg_ib[i])
				break;
		}
		if (i == 0 || i == 5) {
			p_num = 2;
			if (i == 5)
				i--;
		}
		for (j = 0; j < 4; j++) {
			if (temp <= -5 + j*15)
				break;
		}

		if ((j == 0 || j == 4)) {
			if (p_num == 2) {
				/* 1 point case */
				p_num = 1;
				if (j == 4)
					j--;
				rtot = *(dis_chg_rtot[i][j] + v3);
			} else {
				/* 2 point case , between two ib */
				p_num = 2;
				y1 = *(dis_chg_rtot[i-1][j] + v3);
				x1 = dischg_ib[i-1];
				y2 = *(dis_chg_rtot[i][j] + v3);
				x2 = dischg_ib[i];
				rtot = y1 + (y2 - y1) * (ib - x1)/(x2-x1);
			}
			dev_dbg(info->dev, "1 p_num: %d i: %d j: %d\n",
					p_num, i, j);
		} else if (p_num == 2) {
			/* 2 point case, between two temp */
			dev_dbg(info->dev, "2 p_num: %d i: %d j: %d\n",
					p_num, i, j);
			y1 = *(dis_chg_rtot[i][j-1] + v3);
			x1 = (j - 1) * 15 + (-5);
			y2 = *(dis_chg_rtot[i][j] + v3);
			x2 = j * 15 + (-5);
			rtot = y1 + (y2 - y1) * (temp - x1)/(x2-x1);
			dev_dbg(info->dev, "x1: %d, y1: %d, x2: %d, y2: %d\n",
					x1, y1, x2, y2);
		}

		if (p_num == 4) {
			int rtot1, rtot2;
			dev_dbg(info->dev, "3 p_num: %d i: %d j: %d\n",
					p_num, i, j);
			/* step1, interpolate two ib in low temp */
			y1 = *(dis_chg_rtot[i-1][j-1] + v3);
			x1 = dischg_ib[i-1];
			y2 = *(dis_chg_rtot[i][j-1] + v3);
			x2 = dischg_ib[i];
			dev_dbg(info->dev,
				"step1 x1: %d , y1, %d , x2 : %d y2: %d\n",
				x1, y1, x2, y2);
			rtot1 = y1 + (y2 - y1) * (ib - x1)/(x2-x1);

			/* step2, interpolate two ib in high temp */
			y1 = *(dis_chg_rtot[i-1][j] + v3);
			x1 = dischg_ib[i-1];
			y2 = *(dis_chg_rtot[i][j] + v3);
			x2 = dischg_ib[i];
			dev_dbg(info->dev,
				"step2 x1: %d , y1, %d , x2 : %d y2: %d\n",
				x1, y1, x2, y2);
			rtot2 = y1 + (y2 - y1) * (ib - x1)/(x2-x1);

			/* step3 */
			y1 = rtot1;
			x1 = (j - 1) * 15 + (-5);
			y2 = rtot2;
			x2 = j * 15 + (-5);
			rtot = y1 + (y2 - y1) * (temp - x1)/(x2-x1);
			dev_dbg(info->dev,
				"step3: rtot : %d, rtot1: %d, rtot2: %d\n",
				rtot, rtot1, rtot2);
		}
	} else if (info->bat_params.status == POWER_SUPPLY_STATUS_CHARGING) {
		for (i = 0; i < 2; i++) {
			if (ib <= chg_ib[i])
				break;
		}
		if (i == 0 || i == 2) {
			p_num = 2;
			if (i == 2)
				i--;
		}
		for (j = 0; j < 4; j++) {
			if (temp <= -5 + j*15)
				break;
		}

		if ((j == 0 || j == 4)) {
			if (p_num == 2) {
				/* 1 point case */
				p_num = 1;
				if (j == 4)
					j--;
				rtot = *(chg_rtot[i][j] + v3);
			} else {
				/* 2 point case , between two ib */
				p_num = 2;
				y1 = *(chg_rtot[i-1][j] + v3);
				x1 = chg_ib[i-1];
				y2 = *(chg_rtot[i][j] + v3);
				x2 = chg_ib[i];
				rtot = y1 + (y2 - y1) * (ib - x1)/(x2-x1);
			}
			dev_dbg(info->dev, "1 p_num: %d i: %d j: %d\n",
					p_num, i, j);
		} else if (p_num == 2) {
			/* 2 point case, between two temp */
			dev_dbg(info->dev, "2 p_num: %d i: %d j: %d\n",
					p_num, i, j);
			y1 = *(chg_rtot[i][j-1] + v3);
			x1 = (j - 1) * 15 + (-5);
			y2 = *(chg_rtot[i][j] + v3);
			x2 = j * 15 + (-5);
			rtot = y1 + (y2 - y1) * (temp - x1)/(x2-x1);
			dev_dbg(info->dev, "x1: %d, y1: %d, x2: %d, y2: %d\n",
					x1, y1, x2, y2);
		}

		if (p_num == 4) {
			int rtot1, rtot2;
			dev_dbg(info->dev, "3 p_num: %d i: %d j: %d\n",
					p_num, i, j);
			/* step1, interpolate two ib in low temp */
			y1 = *(chg_rtot[i-1][j-1] + v3);
			x1 = chg_ib[i-1];
			y2 = *(chg_rtot[i][j-1] + v3);
			x2 = chg_ib[i];
			dev_dbg(info->dev,
				"step1 x1: %d , y1, %d , x2 : %d y2: %d\n",
				x1, y1, x2, y2);
			rtot1 = y1 + (y2 - y1) * (ib - x1)/(x2-x1);

			/* step2, interpolate two ib in high temp */
			y1 = *(chg_rtot[i-1][j] + v3);
			x1 = chg_ib[i-1];
			y2 = *(chg_rtot[i][j] + v3);
			x2 = chg_ib[i];
			dev_dbg(info->dev,
				"step2 x1: %d , y1, %d , x2 : %d y2: %d\n",
				x1, y1, x2, y2);
			rtot2 = y1 + (y2 - y1) * (ib - x1)/(x2-x1);

			/* step3 */
			y1 = rtot1;
			x1 = (j - 1) * 15 + (-5);
			y2 = rtot2;
			x2 = j * 15 + (-5);
			rtot = y1 + (y2 - y1) * (temp - x1)/(x2-x1);
			dev_dbg(info->dev,
				"step3: rtot : %d, rtot1: %d, rtot2: %d\n",
				rtot, rtot1, rtot2);
		}
	}
	dev_dbg(info->dev, "<---%s r_tot : %d p_num: %d\n",
			__func__, rtot, p_num);
	return rtot;
}

/*
 * register 1 bit[7:0] -- bit[11:4] of measured value of voltage
 * register 0 bit[3:0] -- bit[3:0] of measured value of voltage
 */
static int get_batt_vol(struct pm80x_battery_info *info, int *data, int active)
{
	int ret;
	unsigned char buf[2];
	if (!data)
		return -EINVAL;

	if (active) {
		ret = regmap_bulk_read(info->chip->subchip->regmap_gpadc,
				       PM800_VBAT_AVG, buf, 2);
		if (ret < 0)
			return ret;
	} else {
		ret = regmap_bulk_read(info->chip->subchip->regmap_gpadc,
				       PM800_VBAT_SLP, buf, 2);
		if (ret < 0)
			return ret;
	}

	*data = ((buf[0] & 0xff) << 4) | (buf[1] & 0x0f);
	/* measure(mv) = value * 4 * 1.4 *1000/(2^12) */
	*data = ((*data & 0xfff) * 7 * 100) >> 9;
	/*
	 * Calibrate vbat measurement only for un-trimed PMIC
	 * VBATmeas = VBATreal * gain + offset, so
	 * VBATreal = (VBATmeas - offset)/ gain;
	 * According to our test of vbat_sleep in bootup, the calculated
	 * gain as below:
	 * For Aruba 0.1, offset is -13.4ma, gain is 1.008.
	 * For Aruba 0.2, offset = -0.0087V and gain=1.007.
	 */
	/* data = (*data + 9) * 1000/1007; */
	return 0;
}

/* Read gpadc1 for getting temperature */
static int get_batt_temp(struct pm80x_battery_info *info, int *data)
{
	int ret;
	int temp;
	unsigned char buf[2];

	ret = regmap_bulk_read(info->chip->subchip->regmap_gpadc,
			       PM800_GPADC1_MEAS1, buf, 2);
	if (ret < 0)
		return ret;
	*data = ((buf[0] & 0xff) << 4) | (buf[1] & 0x0f);
	/* measure(mv) = value * 1.4 *1000/(2^12) */
	*data = ((*data & 0xfff) * 7 * 25) >> 9;
	/* meausered Vtbat(mV) / Ibias_current(11uA)*/
	*data = (*data * 1000) / GP1_BAIS_SET_UA;

	if (*data > TBAT_NEG_25D)
		temp = -30;	/* over cold , suppose -30 roughly */
	else if (*data > TBAT_NEG_10D)
		temp = -15;	/* -15 degree, code */
	else if (*data > TBAT_0D)
		temp = -5;	/* -5 degree */
	else if (*data > TBAT_10D)
		temp = 5;	/* in range of (0, 10) */
	else if (*data > TBAT_20D)
		temp = 15;	/* in range of (10, 20) */
	else if (*data > TBAT_30D)
		temp = 25;	/* in range of (20, 30) */
	else if (*data > TBAT_40D)
		temp = 35;	/* in range of (30, 40) */
	else
		temp = 45;	/* over heat ,suppose 45 roughly */


	dev_dbg(info->dev, "temp_C:%d C, NTC Resistor: %d ohm\n", temp, *data);
	*data = temp;

	return 0;
}

/* PM800_RTC_MISC5:E7[1] and PM800_RTC_CONTROL:D0[3:2] can be used */
static void fg_store(struct pm80x_battery_info *info)
{
	int data;

	v3 &= 0x7F;
	regmap_write(info->chip->regmap, PM800_USER_DATA5, v3);

	/* v1 is Volt with factor(2^20) */
	v1_mv = v1 >> 10;
	/* v1_mv resolution is 3mV */
	v1_mv /= 3;
	v1_mv = (v1_mv >= 0) ? min(v1_mv, 31) : max(v1_mv, -32);
	v1_mv &= 0x3F;
	/* regmap_write(info->chip->regmap, PM800_USER_DATA6, v1_mv); */

	/* v2 is Volt with factor(2^20) */
	v2_mv = v2 >> 10;
	v2_mv /= 3;
	v2_mv = (v2_mv >= 0) ? min(v2_mv, 31) : max(v2_mv, -32);
	v2_mv &= 0x3F;

	regmap_write(info->chip->regmap, 0xDD, v1_mv);
	regmap_write(info->chip->regmap, 0xDE, v2_mv);
	regmap_read(info->chip->regmap, 0xDF, &data);
	regmap_write(info->chip->regmap, 0xDF, data);
	regmap_read(info->chip->regmap, 0xE0, &data);
	regmap_write(info->chip->regmap, 0xE0, data);
	dev_dbg(info->dev, "%s v1_mv: 0x%x , v2_mv : 0x%x\n",
		__func__, v1_mv, v2_mv);
	dev_dbg(info->dev, "%s v1_mv: %d\n",
		__func__, (v1_mv & 1 << 5) ? v1_mv | 0xFFFFFFC0 : v1_mv & 0x1F);
	dev_dbg(info->dev, "%s v2_mv: %d\n",
		__func__, (v2_mv & 1 << 5) ? v2_mv | 0xFFFFFFC0 : v2_mv & 0x1F);
}

/*
 * From OCV curves, extract OCV for a given v3 (SOC).
 * Ib= (OCV - Vbatt - v1 - v2) / Rs;
 * v1 = (1-1/(C1*R1))*v1+(1/C1)*Ib;
 * v2 = (1-1/(C2*R2))*v2+(1/C2)*Ib;
 * v3 = v3-(1/Csoc)*Ib;
 * Factor is for enlarge v1 and v2
 * eg: orignal v1= 0.001736 V
 * v1 with factor will be 0.001736 *(1024* 1024) =1820
 */
static int factor = 1024 * 1024;
static int factor2 = 1000;
unsigned long long old_ns, new_ns, delta_ns;
static int fg_active_mode(struct pm80x_battery_info *info)
{
	int ret, ib, tmp, delta_ms = 0;
	int ocv, vbat;
	int temp, ib_ma;
	int *ocv_table = ocv_dischg;

	dev_dbg(info->dev, "%s --->\n", __func__);
	mutex_lock(&info->lock);
	new_ns = sched_clock();
	delta_ns = new_ns - old_ns;
	delta_ms = delta_ns >> 20;
	old_ns = new_ns;
	if (info->bat_params.status == POWER_SUPPLY_STATUS_DISCHARGING ||
			info->bat_params.status == POWER_SUPPLY_STATUS_CHARGING)
		dev_dbg(info->dev, "delta_ms: %d\n", delta_ms);
	mutex_unlock(&info->lock);

	ret = get_batt_vol(info, &vbat, 1);
	if (ret)
		return -EINVAL;
	if (info->bat_params.status == POWER_SUPPLY_STATUS_DISCHARGING ||
			info->bat_params.status == POWER_SUPPLY_STATUS_CHARGING)
		dev_dbg(info->dev, "%s, vbat: %d, v3: %d\n",
				__func__, vbat, v3);
	vbat_mv = vbat;

	/* get OCV from SOC-OCV curves */
	if (info->bat_params.status == POWER_SUPPLY_STATUS_DISCHARGING) {
		ocv_table = ocv_dischg;
		dev_dbg(info->dev, "%s, chg_status: dis-chging...\n", __func__);
	} else if (info->bat_params.status == POWER_SUPPLY_STATUS_CHARGING) {
		ocv_table = ocv_chg;
		dev_dbg(info->dev, "%s, chg_status: chging ...\n", __func__);
	} else if (info->bat_params.status == POWER_SUPPLY_STATUS_FULL) {
		dev_dbg(info->dev, "%s, chg_status: %d\n",
				__func__, info->bat_params.status);
		/* chg done, keep v3 as 100 and no need calculation of v3 */
		goto out;
	} else {
		dev_dbg(info->dev, "%s, chg_status: %d\n",
				__func__, info->bat_params.status);
		/* other exception case, eg. POWER_SUPPLY_STATUS_UNKNOWN */
		goto out;
	}

	ocv = ocv_table[v3];
	dev_dbg(info->dev, "%s, ocv: %d , v1:%d, v2:%d, v3:%d\n",
			__func__, ocv, v1, v2, v3);

	/* (mV/m-Ohm) = A , so, the ib current unit is A with factor */
	/* ib = (ocv - vbat - v1 - v2) * factor/ r_s; */
	ib = (ocv - vbat) * factor - (v1 + v2) * 1000;
	dev_dbg(info->dev, "%s, voltage with factor : %d\n", __func__, ib);
	ib /= r_s;
	dev_dbg(info->dev, "%s, ib with factor : %d\n", __func__, ib);

	/* calculator the rtotal based on temperature and ib mA */
	/* temp >>= 20; */
	/*
	 * FIXME: for emei_dkb we can not do the mearsument of the battery
	 * because of isl9226 charger impact bias current of bat_ntc, so
	 * we assume the temperaure as 26 C here.
	 */
	temp = 26;
	dev_dbg(info->dev, "%s bat temperature : %d\n", __func__, temp);
	ib_ma = ib >> 10;
	calc_resistor(info, temp, ib_ma);
	/* added by M.C. to account for Rsense */
	if (rtot != 0)
		r1 = r2 = rtot / 3;
	r_s = (rtot / 3) + r_off;
	dev_dbg(info->dev, "%s, new r1=r2: %d\n", __func__, r1);
	dev_dbg(info->dev, "%s, new r_s: %d\n", __func__, r_s);

	/* v1 = (1-Ts/(C1*R1))*v1+(Ts/C1)*Ib, the v1 unit is V with factor */
	/* tmp = ((c1 * r1 - delta_ms)* v1/(c1 * r1)); */
	tmp = ((c1 * r1 - delta_ms)/r1) * v1 / c1;
	dev_dbg(info->dev, "%s, v1-1: %d\n", __func__, tmp);
	v1 = tmp + ib/c1 * delta_ms/1000;
	dev_dbg(info->dev, "%s, v1 with factor: %d\n", __func__, v1);

	/* v2 = (1-Ts/(C2*R2))*v2+(Ts/C2)*Ib, the v2 unit is V with factor */
	/* tmp = ((c2 * r2 - delta_ms)* v2/(c2 * r2)); */
	tmp = ((c2 * r2 - delta_ms)/r2) * v2 / c2;
	dev_dbg(info->dev, "%s, v2-1: %d\n", __func__, tmp);
	v2 = tmp + ib/c2 * delta_ms/1000;
	dev_dbg(info->dev, "%s, v2: with factor %d\n", __func__, v2);

	/* v3 = v3-(1/Csoc)*Ib , v3 is SOC */
	/* tmp = ib * delta_ms /(c_soc * 1000); */
	tmp = (ib / c_soc) * delta_ms / 1000;
	/* we change x3 from 0~1 to 0~100 , so mutiply 100 here */
	tmp = tmp * 100 * factor2;
	dev_dbg(info->dev, "%s, v3: tmp1: %d\n", __func__, tmp);
	/* remove  factor */
	tmp >>=  20;
	dev_dbg(info->dev, "%s, v3: tmp2: %d v3_res: %d\n",
			__func__, tmp, v3_res);
	v3_res = v3_res - tmp;
	dev_dbg(info->dev, "%s, v3: v3_res: %d\n", __func__, v3_res);

	v3 = v3_res / factor2;
	fg_store(info);

	dev_dbg(info->dev, "%s, End v3: %d\n", __func__, v3);
out:
	dev_dbg(info->dev, "%s <---\n", __func__);
	return 0;
}

static int fg_lookup_v3(struct pm80x_battery_info *info)
{
	int ret, i;
	int vbat_slp, count;
	int *ocv_table = ocv_dischg;

	if (info->bat_params.status == POWER_SUPPLY_STATUS_DISCHARGING) {
		ocv_table = ocv_dischg;
		dev_dbg(info->dev, "%s, chg_status: dis-chging...\n", __func__);
	} else if (info->bat_params.status == POWER_SUPPLY_STATUS_CHARGING) {
		ocv_table = ocv_chg;
		dev_dbg(info->dev, "%s, chg_status: chging...\n", __func__);
	} else if (info->bat_params.status == POWER_SUPPLY_STATUS_FULL) {
		dev_dbg(info->dev, "%s, chg_status: chg_full %d\n",
				__func__, info->bat_params.status);
	} else {
		dev_dbg(info->dev, "%s, chg_status: %d\n",
				__func__, info->bat_params.status);
	}

	ret = get_batt_vol(info, &vbat_slp, 0);
	if (ret)
		return -EINVAL;

	if (vbat_slp < ocv_table[0]) {
		v3 = 0;
		goto out;
	}

	count = 100;
	for (i = count - 1; i >= 0; i--) {
		if (vbat_slp >= ocv_table[i]) {
			v3 = i + 1;
			break;
		}
	}
out:
	v3_res = v3 * factor2;
	dev_dbg(info->dev, "%s, vbat_slp: %d , v3: %d , v3_res: %d\n",
			__func__, vbat_slp, v3, v3_res);
	return 0;
}
/*
 *  From OCV curves, extract v3=SOC for a given VBATT_SLP.
 *  v1 = v2 = 0;
 *  v3 = v3;
 */
static int fg_long_slp_mode(struct pm80x_battery_info *info)
{

	dev_dbg(info->dev, "%s --->\n", __func__);
	fg_lookup_v3(info);
	v1 = v2 = 0;
	fg_store(info);
	dev_dbg(info->dev, "%s <---\n", __func__);
	return 0;
}
/* exp(-x) function , x is enlarged by mutiply 100,
 * y is already enlarged by 10000 in the table.
 * y0=y1+(y2-y1)*(x0-x1)/(x2-x1)
 */
static int factor_exp = 10000;
static int calc_exp(struct pm80x_battery_info *info, int x)
{
	int y1, y2, y;
	int x1, x2;

	dev_dbg(info->dev, "%s --->\n", __func__);
	if (x > 500)
		return 0;

	x1 = (x/25 * 25);
	x2 = (x + 25)/25 * 25;

	y1 = exp_data[x/25];
	y2 = exp_data[(x + 25)/25];

	y = y1 + (y2 - y1) * (x - x1)/(x2 - x1);

	/*
	dev_dbg(info->dev, "%s, x1: %d, y1: %d\n", __func__, x1, y1);
	dev_dbg(info->dev, "%s, x2: %d, y2: %d\n", __func__, x2, y2);
	dev_dbg(info->dev, "%s, x: %d, y: %d\n", __func__, x, y);
	dev_dbg(info->dev, "%s <-------------\n", __func__);
	*/

	return y;
}
/*
 * LUT of exp(-x) function, we need to calcolate exp(-x) with x=SLEEP_CNT/RC,
 * Since 0<SLEEP_CNT<1000 and RCmin=20 we should calculate exp function in
 * [0-50]. Because exp(-5)= 0.0067 we can use following approximation:
 *	f(x)= exp(-x) = 0   (if x>5);
 *  v1 = v1 * exp (-SLEEP_CNT/(R1*C1));
 *  v2 = v2 * exp (-SLEEP_CNT/(R2*C2));
 *  v3 = v3;
 */
static int fg_short_slp_mode(struct pm80x_battery_info *info, int slp_cnt)
{
	int tmp;
	dev_dbg(info->dev, "%s, v1: %d, v2: %d, v3: %d, slp_cnt: %d\n",
			__func__, v1, v2, v3, slp_cnt);

	dev_dbg(info->dev, "%s --->\n", __func__);
	/* v1 calculation */
	tmp = r1 * c1 / 1000;
	/* enlarge by multiply 100 */
	tmp = slp_cnt * 100 / tmp;
	v1 = v1 * calc_exp(info, tmp);
	v1 = v1 / factor_exp;

	/* v2 calculation */
	tmp = r2 * c2 / 1000;
	/* enlarge by multiply 100 */
	tmp = slp_cnt * 100/tmp;
	v2 = v2 * calc_exp(info, tmp);
	v2 = v2 / factor_exp;
	dev_dbg(info->dev, "%s, v1: %d, v2: %d\n", __func__, v1, v2);

	fg_store(info);
	dev_dbg(info->dev, "%s <---\n", __func__);
	return 0;
}

static int is_charger_online(struct pm80x_battery_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int i, ret = 0;

	for (i = 0; i < info->battery.num_supplicants; i++) {
		psy = power_supply_get_by_name(info->battery.supplied_to[i]);
		if (!psy || !psy->get_property)
			continue;
		ret = psy->get_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret == 0) {
			if (val.intval) {
				who_is_chg = i;
				return val.intval;
			}
		}
	}

	return 0;
}

static int pm80x_is_bat_present(struct pm80x_battery_info *info)
{
	u8 chg_online;
	int ret, data;
	struct power_supply *psy;
	union power_supply_propval val;

	get_batt_vol(info, &vbat_mv, 1);
	if (vbat_mv < 3000) {
		info->present = 0;
		goto out;
	}

	chg_online = is_charger_online(info);
	if (chg_online) {
		psy = power_supply_get_by_name(
			info->battery.supplied_to[who_is_chg]);
		if (!psy || !psy->get_property) {
			info->present = 1;
			return info->present;
		}
		ret = psy->get_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
		if (ret == 0) {
			switch (val.intval) {
			case POWER_SUPPLY_STATUS_CHARGING:
			case POWER_SUPPLY_STATUS_FULL:
				info->present = 1;
				info->bat_params.status = val.intval;
				break;
			case POWER_SUPPLY_STATUS_NOT_CHARGING:
				break;
			default:
				pr_info("%s: charger status: %d\n",
				       __func__, val.intval);
				info->present = 0;
				break;
			}
		}
	} else {
		if (info->bat_ntc) {
			/* detect one-time is enough for ntc */
			if (info->present)
				goto out;

			ret = regmap_read(info->chip->regmap,
				    PM800_STATUS_1, &data);
			if (ret < 0)
				goto out;

			if (PM800_BAT_STS1 & data)
				info->present = 1;
			else
				info->present = 0;
		} else
				info->present = 1;
	}
	dev_dbg(info->dev, "%s chg_online %d, present: %d\n",
			__func__, chg_online, info->present);
out:
	return info->present;
}

static int pm80x_calc_soc(struct pm80x_battery_info *info)
{
	if (info->bat_params.status == POWER_SUPPLY_STATUS_DISCHARGING) {
		/* cap decrease only in dis charge case */
		if (v3 < cap) {
			if (cap - v3 > 1)
				cap--;
			else
				cap = v3;
		}

		/* vbat < LOW_BAT_THRESHOLD */
		if (vbat_mv <= LOW_BAT_THRESHOLD) {
			if (cap > 1)
				cap--;
			if (cap == LOW_BAT_CAP) {
				v3 = cap;
				v3_res = 15 * factor2 + factor2 / 2;
			}
		}

		/* vbat < POWER_OFF_THRESHOLD, cap = 0; */
		if (vbat_mv <= POWER_OFF_THRESHOLD)
			cap = 0;
		else if (cap == 0)
			cap = 1;
	} else if (info->bat_params.status == POWER_SUPPLY_STATUS_CHARGING) {
		if (v3 >= 100)
			cap = 99;
		/* cap increase only in charge case */
		else if (v3 > cap)
			cap = v3;
		/* In charging case, cap should not be 100 */
		if (cap == 100)
			cap = v3;
	} else if (info->bat_params.status == POWER_SUPPLY_STATUS_FULL) {
		if (v3 == 100)
			cap = v3;
		else
			cap = 100;
		v3_res = 100 * factor2 + factor2 / 2;
		dev_dbg(info->dev, "%s, chg_status: %d\n",
				__func__, info->bat_params.status);
	} else {
		dev_dbg(info->dev, "%s, chg_status: %d\n",
				__func__, info->bat_params.status);
	}
	dev_dbg(info->dev, "%s, vbat_mv: %d, Android soc: %d\n",
			__func__, vbat_mv, cap);
	return 0;
}

/* Update battery status */
static void pm80x_bat_update_status(struct pm80x_battery_info *info)
{
	int data = 0;
	/* NOTE: hardcode battery type[Lion] and health[Good] */
	info->bat_params.health = POWER_SUPPLY_HEALTH_GOOD;
	info->bat_params.tech = POWER_SUPPLY_TECHNOLOGY_LION;

	if (info->bat_ntc) {
		get_batt_temp(info, &data);
		info->bat_params.temp = data * 10;
	} else {
		info->bat_params.temp = 260;
	}

	/* Battery presence state, update charging status if chg online */
	info->bat_params.present = pm80x_is_bat_present(info);
	info->bat_params.volt = vbat_mv;

	/* Charging status */
	if (info->bat_params.present) {
		/* Report charger online timely */
		if (!power_supply_am_i_supplied(&info->battery))
			info->bat_params.status =
				POWER_SUPPLY_STATUS_DISCHARGING;
		/* calculate the capacity for android */
		pm80x_calc_soc(info);
		/* Report soc of the capacity */
		info->bat_params.cap = cap;
	} else
		info->bat_params.status = POWER_SUPPLY_STATUS_UNKNOWN;
}

static void pm80x_fg_work(struct work_struct *work)
{
	struct pm80x_battery_info *info =
		container_of(work, struct pm80x_battery_info,
			     fg_work.work);

	fg_active_mode(info);
	queue_delayed_work(info->bat_wqueue, &info->fg_work,
			   FG_INTERVAL);
}

static void pm80x_battery_work(struct work_struct *work)
{
	struct pm80x_battery_info *info =
		container_of(work, struct pm80x_battery_info,
			     monitor_work.work);

	power_supply_changed(&info->battery);
	if (info->bat_params.cap <= LOW_BAT_CAP)
		queue_delayed_work(info->bat_wqueue, &info->monitor_work,
			   LOW_BAT_INTERVAL);
	else
		queue_delayed_work(info->bat_wqueue, &info->monitor_work,
			   MONITOR_INTERVAL);
}

static void pm80x_charged_work(struct work_struct *work)
{
	struct pm80x_battery_info *info =
		container_of(work, struct pm80x_battery_info,
			     charged_work.work);

	pm80x_bat_update_status(info);
	power_supply_changed(&info->battery);
	/* NO need to be scheduled again */
	return;
}

static irqreturn_t pm80x_bat_handler(int irq, void *data)
{
	struct pm80x_battery_info *info = data;
	dev_dbg(info->dev,
		"%s is triggered!\n", __func__);
	/*
	 * report the battery status
	 * when the voltage is abnormal
	 */
	pm80x_bat_update_status(info);
	return IRQ_HANDLED;
}

static void pm80x_init_battery(struct pm80x_battery_info *info)
{
	int data, mask;
	int previous_v3 = 0;
	int slp_cnt;
	/*
	 * enable VBAT
	 * VBAT is used to measure voltage
	 */
	data = mask = PM800_MEAS_EN1_VBAT;
	regmap_update_bits(info->chip->subchip->regmap_gpadc,
			PM800_GPADC_MEAS_EN1, mask, data);

	if (info->bat_ntc) {
		data = mask = PM800_MEAS_GP1_EN;
		regmap_update_bits(info->chip->subchip->regmap_gpadc,
				PM800_GPADC_MEAS_EN2, mask, data);

		data = mask = GP1_BAIS_SET;
		regmap_update_bits(info->chip->subchip->regmap_gpadc,
				PM800_GPADC_BIAS2, mask, data);

		data = mask = PM800_GPADC_GP_BIAS_EN1 | PM800_GPADC_GP_BIAS_OUT1;
		regmap_update_bits(info->chip->subchip->regmap_gpadc,
				PM800_GP_BIAS_ENA1, mask, data);

		data = mask = PM800_BD_GP1_EN | PM800_BD_EN;
		regmap_update_bits(info->chip->subchip->regmap_gpadc,
				PM800_GPADC_MEASURE_OFF2, mask, data);
	}
	/* set VBAT low threshold as 3.5V */
	regmap_write(info->chip->subchip->regmap_gpadc,
		     PM800_VBAT_LOW_TH, 0xa0);

	/* sleep cnt */
	regmap_update_bits(info->chip->regmap, PM800_RTC_MISC6,
			SLP_CNT_RD_LSB, SLP_CNT_RD_LSB);
	regmap_read(info->chip->regmap,
		    PM800_RTC_MISC7, &data);
	slp_cnt = data & 0xFF;
	dev_dbg(info->dev, "%s, LSB: data: %d 0x%x\n", __func__, data, data);

	regmap_update_bits(info->chip->regmap, PM800_RTC_MISC6,
			SLP_CNT_RD_LSB, 0);
	regmap_read(info->chip->regmap,
		    PM800_RTC_MISC7, &data);
	slp_cnt |= (data & 0xE0) << 3;
	dev_dbg(info->dev, "%s, MSB: data: %d 0x%x\n", __func__, data, data);
	dev_dbg(info->dev, "%s, slp_cnt: %d 0x%x\n",
			__func__, slp_cnt, slp_cnt);

	regmap_read(info->chip->regmap, PM800_POWER_UP_LOG, &data);
	dev_dbg(info->dev, "%s, power up flag: 0x%x\n", __func__, data);

	/* check whether battery present */
	info->bat_params.present = pm80x_is_bat_present(info);
	pr_info("%s: 88pm80x battery present: %d\n", __func__,
		info->bat_params.present);


	if (slp_cnt < 1000) {
		regmap_read(info->chip->regmap, PM800_USER_DATA5, &data);
		dev_dbg(info->dev, "%s read back: 0xEE: 0x%x\n",
				__func__, data);
		data &= 0x7F;
		dev_dbg(info->dev, "%s, data(v3): %d\n", __func__, data);
		previous_v3 = data;

		/* Only look up ocv table to avoid big error of v3 */
		fg_lookup_v3(info);
		if (max(data, v3) - min(data, v3) < 15) {
			v3 = data;
			v3_res = data * factor2 + factor2 / 2;
		}
		dev_dbg(info->dev, "%s, v3: %d, v3_res: %d\n",
				__func__, v3, v3_res);

		if (max(previous_v3, v3) - min(previous_v3, v3) < 15) {

			regmap_read(info->chip->regmap, 0xDD, &data);
			dev_dbg(info->dev, "%s read back: 0xDD: 0x%x\n",
					__func__, data);
			/* extract v1_mv */
			v1_mv = (data & 0x3F);
			v1_mv = (v1_mv & 1 << 5) ? (v1_mv | 0xFFFFFFC0)
				: (v1_mv & 0x1F);

			regmap_read(info->chip->regmap, 0xDE, &data);
			dev_dbg(info->dev, "%s read back: 0xDD: 0x%x\n",
					__func__, data);
			/* extract v2_mv */
			v2_mv = data;
			v2_mv = (v2_mv & 0x3F);
			v2_mv = (v2_mv & 1 << 5) ? (v2_mv | 0xFFFFFFC0)
				: (v2_mv & 0x1F);
			dev_dbg(info->dev, "%s v1_mv: 0x%x , v2_mv : 0x%x\n",
					__func__, v1_mv, v2_mv);
			dev_dbg(info->dev, "%s v1_mv: %d , v2_mv : %d\n",
					__func__, v1_mv, v2_mv);
			v1_mv *= 3;
			v2_mv *= 3;
			v1 = v1_mv << 10;
			v2 = v2_mv << 10;
			dev_dbg(info->dev, "%s v1: %d, v2 : %d\n",
				__func__, v1, v2);
		} else {
			v1 = 0;
			v2 = 0;
		}

		fg_short_slp_mode(info, slp_cnt);
		if (previous_v3 <= LOW_BAT_CAP && v3 <= LOW_BAT_CAP) {
			v3 = previous_v3;
			v3_res = v3 * factor2 + factor2 / 2;
		}
	} else
		fg_long_slp_mode(info);

	if (v3 > 100) {
		v3_res = 100 * factor2 + factor2 / 2;
		v3 = 100;
	}
	cap = v3;
}

static void pm80x_external_power_changed(struct power_supply *psy)
{
	struct pm80x_battery_info *info;

	info = container_of(psy, struct pm80x_battery_info, battery);
	queue_delayed_work(info->bat_wqueue,
			   &info->charged_work, HZ);
	return;
}

static int pm80x_batt_get_prop(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct pm80x_battery_info *info = dev_get_drvdata(psy->dev->parent);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		pm80x_bat_update_status(info);
		val->intval = info->bat_params.status;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = info->bat_params.present;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		/* report fake capacity without battery */
		if (!info->bat_params.present)
			info->bat_params.cap = 80;
		val->intval = info->bat_params.cap;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = info->bat_params.tech;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = info->bat_params.volt;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (!info->bat_params.present)
			info->bat_params.temp = 240;
		val->intval = info->bat_params.temp;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = info->bat_params.health;
		break;
	default:
		return -ENODEV;
	}
	return 0;
}

static enum power_supply_property pm80x_batt_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_HEALTH,
};

static __devinit int pm80x_battery_probe(struct platform_device *pdev)
{
	struct pm80x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm80x_battery_info *info;
	struct pm80x_bat_pdata *pdata = NULL;
	int irq, ret;

	info = kzalloc(sizeof(struct pm80x_battery_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	pdata = pdev->dev.platform_data;
	if (pdata) {
		if (pdata->capacity)
			c_soc = pdata->capacity * 36 / 10;
		if (pdata->r_int)
			rtot = pdata->r_int;
		if (pdata->bat_ntc)
			info->bat_ntc = pdata->bat_ntc;
	}

	info->chip = chip;
	info->dev = &pdev->dev;
	info->bat_params.status = POWER_SUPPLY_STATUS_UNKNOWN;

	platform_set_drvdata(pdev, info);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "No IRQ resource!\n");
		ret = -EINVAL;
		goto out;
	}

	info->irq = irq + chip->irq_base;

	mutex_init(&info->lock);
	pm80x_init_battery(info);

	info->battery.name = "battery";
	info->battery.type = POWER_SUPPLY_TYPE_BATTERY;
	info->battery.properties = pm80x_batt_props;
	info->battery.num_properties = ARRAY_SIZE(pm80x_batt_props);
	info->battery.get_property = pm80x_batt_get_prop;
	info->battery.external_power_changed = pm80x_external_power_changed;
	info->battery.supplied_to = pm80x_supply_to;
	info->battery.num_supplicants = ARRAY_SIZE(pm80x_supply_to);

	ret = power_supply_register(&pdev->dev, &info->battery);
	if (ret)
		goto out;
	info->battery.dev->parent = &pdev->dev;
	pm80x_bat_update_status(info);

	ret = pm80x_request_irq(info->chip, info->irq, pm80x_bat_handler,
				IRQF_ONESHOT, "battery", info);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to request IRQ: #%d: %d\n",
			info->irq, ret);
		goto out;
	}

	info->bat_wqueue = create_singlethread_workqueue("bat-88pm800");
	if (!info->bat_wqueue) {
		dev_info(chip->dev,
			"[%s]Failed to create bat_wqueue\n", __func__);
		ret = -ESRCH;
		goto out;
	}

	INIT_DELAYED_WORK_DEFERRABLE(&info->fg_work, pm80x_fg_work);
	INIT_DELAYED_WORK_DEFERRABLE(&info->monitor_work, pm80x_battery_work);
	INIT_DELAYED_WORK(&info->charged_work, pm80x_charged_work);
	queue_delayed_work(info->bat_wqueue, &info->fg_work, FG_INTERVAL);
	queue_delayed_work(info->bat_wqueue, &info->monitor_work,
			   MONITOR_INTERVAL);

	device_init_wakeup(&pdev->dev, 1);
	return 0;

out:
	power_supply_unregister(&info->battery);
	kfree(info);
	return ret;
}

static int __devexit pm80x_battery_remove(struct platform_device *pdev)
{
	struct pm80x_battery_info *info = platform_get_drvdata(pdev);

	cancel_delayed_work(&info->fg_work);
	cancel_delayed_work(&info->monitor_work);
	cancel_delayed_work(&info->charged_work);
	flush_workqueue(info->bat_wqueue);
	power_supply_unregister(&info->battery);
	kfree(info);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static void pm80x_battery_shutdown(struct platform_device *pdev)
{
	struct pm80x_battery_info *info = platform_get_drvdata(pdev);

	cancel_delayed_work(&info->fg_work);
	cancel_delayed_work(&info->monitor_work);
	flush_workqueue(info->bat_wqueue);
}

#ifdef CONFIG_PM
static int pm80x_battery_suspend(struct device *dev)
{
	struct pm80x_battery_info *info = dev_get_drvdata(dev);
	cancel_delayed_work_sync(&info->fg_work);
	cancel_delayed_work_sync(&info->monitor_work);
	return pm80x_dev_suspend(dev);
}

static int pm80x_battery_resume(struct device *dev)
{
	struct pm80x_battery_info *info = dev_get_drvdata(dev);
	int data;
	int slp_cnt;

	mutex_lock(&info->lock);
	old_ns = sched_clock();
	dev_dbg(info->dev, "%s old_ns: %llu\n", __func__, old_ns);
	mutex_unlock(&info->lock);
	/* sleep cnt */
	regmap_update_bits(info->chip->regmap, PM800_RTC_MISC6,
			SLP_CNT_RD_LSB, SLP_CNT_RD_LSB);
	regmap_read(info->chip->regmap,
		    PM800_RTC_MISC7, &data);
	slp_cnt = data & 0xFF;
	dev_dbg(info->dev, "%s, LSB: data: %d 0x%x\n", __func__, data, data);

	regmap_update_bits(info->chip->regmap, PM800_RTC_MISC6,
			SLP_CNT_RD_LSB, 0);
	regmap_read(info->chip->regmap,
		    PM800_RTC_MISC7, &data);
	slp_cnt |= (data & 0xE0) << 3;
	dev_dbg(info->dev, "%s, MSB: data: %d 0x%x\n", __func__, data, data);
	dev_dbg(info->dev, "%s, slp_cnt: %d 0x%x\n",
			__func__, slp_cnt, slp_cnt);

	regmap_read(info->chip->regmap,
		    PM800_POWER_DOWN_LOG2, &data);
	if (data & HYB_DONE) {
		regmap_update_bits(info->chip->regmap, PM800_POWER_DOWN_LOG2,
				HYB_DONE, HYB_DONE);
		dev_info(info->dev, "%s HYB_DONE, slp_cnt: %d\n",
				__func__, slp_cnt);
		if (slp_cnt < 1000)
			fg_short_slp_mode(info, slp_cnt);
		else if (slp_cnt >= 1000)
			fg_long_slp_mode(info);
	}
	queue_delayed_work(info->bat_wqueue, &info->monitor_work, HZ);
	queue_delayed_work(info->bat_wqueue, &info->fg_work, FG_INTERVAL);
	return pm80x_dev_resume(dev);
}

static const struct dev_pm_ops pm80x_battery_pm_ops = {
	.suspend	= pm80x_battery_suspend,
	.resume		= pm80x_battery_resume,
};
#endif

static struct platform_driver pm80x_battery_driver = {
	.driver		= {
		.name	= "88pm80x-bat",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &pm80x_battery_pm_ops,
#endif
	},
	.probe		= pm80x_battery_probe,
	.remove		= __devexit_p(pm80x_battery_remove),
	.shutdown	= &pm80x_battery_shutdown,
};

static int __init pm80x_battery_init(void)
{
	return platform_driver_register(&pm80x_battery_driver);
}
module_init(pm80x_battery_init);

static void __exit pm80x_battery_exit(void)
{
	platform_driver_unregister(&pm80x_battery_driver);
}
module_exit(pm80x_battery_exit);

MODULE_DESCRIPTION("Marvell 88PM80x Battery driver");
MODULE_LICENSE("GPL");
