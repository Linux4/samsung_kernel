/*
 * =================================================================
 *		File Name:			dynamic_aid_gamma.c
 *		Original Filename:  smart_mtp_s6e88a.h
 *
 *    Description:  Smart dimming algorithm implementation
 *
 *        Author: jb09.kim
 *        Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
   <one line to give the program's name and a brief idea of what it does.>
   Copyright (C) 2012, Samsung Electronics. All rights reserved.

 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include "dynamic_aid_gamma.h"
#include "dynamic_aid_gamma_curve.h"
#include "dynamic_aid.h"

//#define DYNAMIC_AID_DEBUG
static struct SMART_DIM sdim_oled;
static char max_lux_table[GAMMA_SET_MAX];
static char min_lux_table[GAMMA_SET_MAX];

static int REF_MTP[][3] = {
	{0x0, 0x0, 0x0},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x100, 0x100, 0x100},
};

static int MTP[][3] = {
	{0x0, 0x0, 0x0},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x100, 0x100, 0x100},
};

struct coeff_t v_coeff[] = {
	{0, 860},       /* IV_VT */
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{72, 860}       /* IV_255 */
};

static int vt_coefficient[] = {
	0, 12, 24, 36, 48,
	60, 72, 84, 96, 108,
	138, 148, 158, 168,
	178, 186,
};

static int char_to_int(char data1)
{
	int cal_data;

	if (data1 & 0x80) {
		cal_data = data1 & 0x7F;
		cal_data *= (-1);
	} else
		cal_data = data1;

	return cal_data;
}

static int char_to_int_v255(char data1, char data2)
{
	int cal_data;

	if (data1)
		cal_data = data2 * (-1);
	else
		cal_data = data2;

	return cal_data;
}

static void print_mtp(int (*mtp)[3])
{
	int volt, offset;
	for (volt = VOLT_VT; volt < VOLT_MAX; volt++)
		pr_info("%s, RED %d, GREED %d, BLUE %d\n",
				volt_name[volt],
				mtp[volt][R_OFFSET],
				mtp[volt][G_OFFSET],
				mtp[volt][B_OFFSET]);
}

static void print_candela(long long *candela_level)
{
	char buf[256];
	char *p = buf;
	int volt;

	p += sprintf(p, "\n#### [CANDELA LEVEL] ####\n");
	for (volt = VOLT_VT; volt < VOLT_MAX; volt++) {
		p += sprintf(p, "[%3d : %10llu]",
				VT_CURVE[volt], candela_level[volt]);
		if ((volt + 1) == VOLT_MAX)
			break;
		if (!((volt + 1) % 3))
			p += sprintf(p, "\n");
		else
			p += sprintf(p, "\t");
	}
	pr_info("%s\n", buf);
}

static void print_bl_index(int *bl_index)
{
	char buf[256];
	char *p = buf;
	int volt;

	p += sprintf(p, "\n#### [BL INDEX] ####\n");
	for (volt = VOLT_VT; volt < VOLT_MAX; volt++) {
		p += sprintf(p, "[%3d : %5d]", VT_CURVE[volt], bl_index[volt]);
		if ((volt + 1) == VOLT_MAX)
			break;
		if (!((volt + 1) % 3))
			p += sprintf(p, "\n");
		else
			p += sprintf(p, "\t");
	}
	pr_info("%s\n", buf);
}

static void print_gamma_table(struct SMART_DIM *pSmart)
{
	int cnt, lux_loop;
	char pBuffer[256];
	int i;
	int ref_table[] = {
		27, 28, 29, 24, 25, 26,
		21, 22, 23, 18, 19, 20,
		15, 16, 17, 12, 13, 14,
		9, 10, 11, 6, 7, 8
	};
	memset(pBuffer, 0x00, 256);

	for (lux_loop = 0; lux_loop < pSmart->lux_table_max; lux_loop++) {
		for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %d",
					pSmart->gen_table[lux_loop].gamma_setting[cnt]);

		pr_info("lux : %d  %s\n", pSmart->plux_table[lux_loop], pBuffer);
		memset(pBuffer, 0x00, 256);
	}

	for (lux_loop = 0; lux_loop < pSmart->lux_table_max; lux_loop++) {
		for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256,
					" %02X",
					pSmart->gen_table[lux_loop].gamma_setting[cnt]);

		pr_info("lux : %d  %s\n", pSmart->plux_table[lux_loop], pBuffer);
		memset(pBuffer, 0x00, 256);
	}

	pr_info("################# PRINT DECIMAL #################\n");
	for (lux_loop = 0; lux_loop < pSmart->lux_table_max; lux_loop++) {

		for (cnt = 0; cnt < ARRAY_SIZE(ref_table); cnt++) {
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %3d",
					pSmart->gen_table[lux_loop].gamma_setting[ref_table[cnt]]);
		}

		i = pSmart->gen_table[lux_loop].gamma_setting[0]*256 + pSmart->gen_table[lux_loop].gamma_setting[1];
		snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %3d", i);
		i = pSmart->gen_table[lux_loop].gamma_setting[2]*256 + pSmart->gen_table[lux_loop].gamma_setting[3];
		snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %3d", i);
		i = pSmart->gen_table[lux_loop].gamma_setting[4]*256 + pSmart->gen_table[lux_loop].gamma_setting[5];
		snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %3d", i);

		for (cnt = 30; cnt < GAMMA_SET_MAX; cnt++) {
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %3d",
					pSmart->gen_table[lux_loop].gamma_setting[cnt]);
		}

		pr_info("lux : %3d  %s\n", pSmart->plux_table[lux_loop], pBuffer);
		memset(pBuffer, 0x00, 256);
	}
	pr_info("################# PRINT DECIMAL END #################\n");

	pr_info("################# PRINT HEX #################\n");
	for (lux_loop = 0; lux_loop < pSmart->lux_table_max; lux_loop++) {
		for (cnt = 0; cnt < ARRAY_SIZE(ref_table); cnt++) {
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02X",
					pSmart->gen_table[lux_loop].gamma_setting[ref_table[cnt]]);
		}
		i = pSmart->gen_table[lux_loop].gamma_setting[0]*256 +
			pSmart->gen_table[lux_loop].gamma_setting[1];
		snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02X", i);
		i = pSmart->gen_table[lux_loop].gamma_setting[2]*256 +
			pSmart->gen_table[lux_loop].gamma_setting[3];
		snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02X", i);
		i = pSmart->gen_table[lux_loop].gamma_setting[4]*256 +
			pSmart->gen_table[lux_loop].gamma_setting[5];
		snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02X", i);

		for (cnt = 30; cnt < GAMMA_SET_MAX; cnt++) {
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02X",
					pSmart->gen_table[lux_loop].gamma_setting[cnt]);
		}

		pr_info("lux : %3d  %s\n", pSmart->plux_table[lux_loop], pBuffer);
		memset(pBuffer, 0x00, 256);
	}
	pr_info("################# PRINT HEX END #################\n");
}

static void mtp_ctoi(int (*dst)[3], char *src)
{
	int volt, offset, cnt;

	for (volt = VOLT_V255, cnt = 0; volt >= VOLT_VT; volt--) {
		for (offset = R_OFFSET; offset <= B_OFFSET; offset++) {
			if (volt == VOLT_V255) {
				dst[volt][offset] =
					char_to_int_v255(src[cnt],
							src[cnt + 1]);
				cnt += 2;
			} else {
				dst[volt][offset] = src[cnt++];
			}
		}
	}
#ifdef DYNAMIC_AID_DEBUG
	print_mtp(dst);
#endif
}

static void mtp_itoc(char *dst, int (*src)[3])
{
	int volt, offset, cnt;

	for (cnt = 0, volt = VOLT_V255; volt >= VOLT_VT; volt--)
		for (offset = R_OFFSET; offset <= B_OFFSET; offset++) {
			if (volt == VOLT_V255) {
				dst[cnt++] = (src[volt][offset] & 0xFF00) >> 8;
				dst[cnt++] = src[volt][offset] & 0xFF;
			} else
				dst[cnt++] =
				((src[volt][offset] > 0xFF)? 0xFF : src[volt][offset] & 0xFF);
		}
}

static int voltage_adjustment(struct SMART_DIM *pSmart, int volt)
{
	unsigned long long result_1, result_2, result_3, result_4;
	int add_mtp, offset, LSB;

	if (volt == VOLT_V255) {
		for (offset = R_OFFSET; offset <= B_OFFSET; offset++) {
			add_mtp = REF_MTP[volt][offset] + MTP[volt][offset];
			result_1 = result_2 = (v_coeff[volt].numerator + add_mtp) << BIT_SHIFT;
			do_div(result_2, v_coeff[volt].denominator);
			result_3 = (pSmart->vregout_voltage * result_2) >> BIT_SHIFT;
			result_4 = pSmart->vregout_voltage - result_3;
			((int *)(&pSmart->RGB_OUTPUT))[offset * VOLT_MAX + volt] = result_4;
			((int *)(&pSmart->RGB_OUTPUT))[offset * VOLT_MAX + VOLT_VT] = pSmart->vregout_voltage;
		}
	} else if (volt == VOLT_VT) {
		for (offset = R_OFFSET; offset <= B_OFFSET; offset++) {
			LSB = MTP[volt][offset];
			add_mtp = REF_MTP[volt][offset] + MTP[volt][offset];
			result_1 = result_2 = vt_coefficient[LSB] << BIT_SHIFT;
			do_div(result_2, v_coeff[volt].denominator);
			result_3 = (pSmart->vregout_voltage * result_2) >> BIT_SHIFT;
			result_4 = pSmart->vregout_voltage - result_3;
			((int *)(&pSmart->GRAY.VT_TABLE))[offset] = result_4;
		}
	} else if (volt == VOLT_V3) {
		for (offset = R_OFFSET; offset <= B_OFFSET; offset++) {
			add_mtp = REF_MTP[volt][offset] + MTP[volt][offset];
			result_1 = (pSmart->vregout_voltage)
				- ((int *)(&pSmart->RGB_OUTPUT))[offset * VOLT_MAX + volt + 1];
			result_2 = (v_coeff[volt].numerator + add_mtp) << BIT_SHIFT;
			do_div(result_2, v_coeff[volt].denominator);

			result_3 = (result_1 * result_2) >> BIT_SHIFT;
			result_4 = (pSmart->vregout_voltage) - result_3;
			((int *)(&pSmart->RGB_OUTPUT))[offset * VOLT_MAX + volt] = result_4;
		}
	} else {
		for (offset = R_OFFSET; offset <= B_OFFSET; offset++) {
			add_mtp = REF_MTP[volt][offset] + MTP[volt][offset];
			result_1 = ((int *)(&pSmart->GRAY.VT_TABLE))[offset]
				- ((int *)(&pSmart->RGB_OUTPUT))[offset * VOLT_MAX + volt + 1];
			result_2 = (v_coeff[volt].numerator + add_mtp) << BIT_SHIFT;
			do_div(result_2, v_coeff[volt].denominator);
			result_3 = (result_1 * result_2) >> BIT_SHIFT;
			result_4 = ((int *)(&pSmart->GRAY.VT_TABLE))[offset] - result_3;
			((int *)(&pSmart->RGB_OUTPUT))[offset * VOLT_MAX + volt] = result_4;
		}
	}

#ifdef DYNAMIC_AID_DEBUG
	pr_info("%s %s RED:%d GREEN:%d BLUE:%d\n", __func__, volt_name[volt],
			((int *)(&pSmart->RGB_OUTPUT))[R_OFFSET * VOLT_MAX + volt],
			((int *)(&pSmart->RGB_OUTPUT))[G_OFFSET * VOLT_MAX + volt],
			((int *)(&pSmart->RGB_OUTPUT))[B_OFFSET * VOLT_MAX + volt]);
#endif

	return 0;
}

static void voltage_hexa(int *index, struct SMART_DIM *pSmart, int *str, int volt)
{
	unsigned long long result_1, result_2, result_3;
	int offset;

	if (volt == VOLT_VT) {
		for (offset = R_OFFSET; offset <= B_OFFSET; offset++)
			str[offset] = REF_MTP[volt][offset];
	} else if (volt == VOLT_V255) {
		for (offset = R_OFFSET; offset <= B_OFFSET; offset++) {
			result_1 = pSmart->vregout_voltage -
				((int *)&pSmart->GRAY.TABLE[index[volt]])[offset];
			result_2 = result_1 * v_coeff[volt].denominator;
			do_div(result_2, pSmart->vregout_voltage);
			str[offset] = result_2  - v_coeff[volt].numerator;
		}
	} else if (volt == VOLT_V3) {
		for (offset = R_OFFSET; offset <= B_OFFSET; offset++) {
			result_1 = pSmart->vregout_voltage -
				((int *)&pSmart->GRAY.TABLE[index[volt]])[offset];
			result_2 = result_1 * v_coeff[volt].denominator;
			result_3 = (pSmart->vregout_voltage) -
				((int *)&pSmart->GRAY.TABLE[index[volt + 1]])[offset];
			do_div(result_2, result_3);
			str[offset] = result_2  - v_coeff[volt].numerator;
		}
	} else {
		for (offset = R_OFFSET; offset <= B_OFFSET; offset++) {
			result_1 = ((int *)(&pSmart->GRAY.VT_TABLE))[offset] -
				((int *)&pSmart->GRAY.TABLE[index[volt]])[offset];
			result_2 = result_1 * v_coeff[volt].denominator;
			result_3 = ((int *)(&pSmart->GRAY.VT_TABLE))[offset] -
				((int *)&pSmart->GRAY.TABLE[index[volt + 1]])[offset];
			do_div(result_2, result_3);
			str[offset] = result_2  - v_coeff[volt].numerator;
		}
	}
}

static int cal_gray_scale_linear(int up, int low, int coeff,
		int mul, int deno, int cnt)
{
	unsigned long long result_1, result_2, result_3, result_4;
	int sign;

	sign = (up < low) ? -1 : 1;
	result_1 = abs(up - low);
	result_2 = (result_1 * (coeff - (cnt * mul))) << BIT_SHIFT;
	do_div(result_2, deno);
	result_3 = result_2 >> BIT_SHIFT;
	result_4 = low + (sign * result_3);

	return (int)result_4;
}

static int generate_gray_scale(struct SMART_DIM *pSmart)
{
	int cnt = 0, cal_cnt = 0;
	int iv = 0, gap = 0;
	struct GRAY_VOLTAGE *ptable = (struct GRAY_VOLTAGE *)
		(&(pSmart->GRAY.TABLE));

	for (cnt = 0; cnt < VOLT_MAX; cnt++) {
		pSmart->GRAY.TABLE[VT_CURVE[cnt]].R_Gray =
			((int *)&(pSmart->RGB_OUTPUT.R_VOLTAGE))[cnt];

		pSmart->GRAY.TABLE[VT_CURVE[cnt]].G_Gray =
			((int *)&(pSmart->RGB_OUTPUT.G_VOLTAGE))[cnt];

		pSmart->GRAY.TABLE[VT_CURVE[cnt]].B_Gray =
			((int *)&(pSmart->RGB_OUTPUT.B_VOLTAGE))[cnt];
	}

	/*
	   below codes use hard coded value.
	   So it is possible to modify on each model.
	   V0,V3,V11,V23,V35,V51,V87,V151,V203,V255
	   */
	for (cnt = 0; cnt < GRAY_SCALE_MAX; cnt++) {
		if (cnt == VT_CURVE[iv]) {
			cal_cnt = 0;
			iv++;
		} else {
			gap = VT_CURVE[iv] - VT_CURVE[iv - 1];
			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
					ptable[VT_CURVE[iv - 1]].R_Gray,
					ptable[VT_CURVE[iv]].R_Gray,
					gap - 1, 1, gap, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
					ptable[VT_CURVE[iv - 1]].G_Gray,
					ptable[VT_CURVE[iv]].G_Gray,
					gap - 1, 1, gap, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
					ptable[VT_CURVE[iv - 1]].B_Gray,
					ptable[VT_CURVE[iv]].B_Gray,
					gap - 1, 1, gap, cal_cnt);
			cal_cnt++;
		}
	}

#ifdef DYNAMIC_AID_DEBUG
	for (cnt = 0; cnt < GRAY_SCALE_MAX; cnt++) {
		pr_info("%s %8d %8d %8d %d\n", __func__,
				pSmart->GRAY.TABLE[cnt].R_Gray,
				pSmart->GRAY.TABLE[cnt].G_Gray,
				pSmart->GRAY.TABLE[cnt].B_Gray, cnt);
	}
#endif
	return 0;
}

static int searching_function(long long candela, int *gamma_curve_table, int max_bl)
{
	int l = 0, r = GRAY_SCALE_MAX - 1, m;
	long long delta_l, delta_r;
	if (unlikely(!gamma_curve_table)) {
		pr_err("%s, gamma curve table not exist\n", __func__);
		return -EINVAL;
	}

	if ((gamma_curve_table[l] * max_bl > candela) ||
			(gamma_curve_table[r] * max_bl < candela)) {
		pr_err("%s, out of range(%d, %d) candela(%lld)\n",
				__func__, gamma_curve_table[l] * max_bl,
				gamma_curve_table[r] * max_bl, candela);
		return -EINVAL;
	}

	while (l <= r) {
		m = l + (r - l) / 2;
		if (gamma_curve_table[m] * max_bl == candela)
			return m;
		if (gamma_curve_table[m] * max_bl < candela)
			l = m + 1;
		else
			r = m - 1;
	}

	delta_l = candela - gamma_curve_table[r] * max_bl;
	delta_r = gamma_curve_table[l] * max_bl - candela;

	return delta_l <= delta_r ? r : l;
}

int find_candela_index(struct SMART_DIM *pSmart, int brightness)
{
	int idx;

	for (idx = 0; idx < pSmart->lux_table_max; idx++)
		if (brightness == pSmart->plux_table[idx])
			break;

	if (idx == pSmart->lux_table_max) {
		pr_warn("%s, not found %d\n",
				__func__, brightness);
		return -1;
	}

	return idx;
}

int *find_gamma_curve_table(int gamma_curve)
{
	int *gamma_curve_table = NULL;
	switch (gamma_curve) {
	case 160:
		gamma_curve_table = candela_coeff_1p6;
		break;
	case 165:
		gamma_curve_table = candela_coeff_1p65;
		break;
	case 170:
		gamma_curve_table = candela_coeff_1p7;
		break;
	case 175:
		gamma_curve_table = candela_coeff_1p75;
		break;
	case 180:
		gamma_curve_table = candela_coeff_1p8;
		break;
	case 185:
		gamma_curve_table = candela_coeff_1p85;
		break;
	case 190:
		gamma_curve_table = candela_coeff_1p9;
		break;
	case 195:
		gamma_curve_table = candela_coeff_1p95;
		break;
	case 200:
		gamma_curve_table = candela_coeff_2p0;
		break;
	case 205:
		gamma_curve_table = candela_coeff_2p05;
		break;
	case 210:
		gamma_curve_table = candela_coeff_2p1;
		break;
	case 212:
		gamma_curve_table = candela_coeff_2p12;
		break;
	case 213:
		gamma_curve_table = candela_coeff_2p13;
		break;
	case 215:
		gamma_curve_table = candela_coeff_2p15;
		break;
	case 220:
		gamma_curve_table = candela_coeff_2p2;
		break;
	case 225:
		gamma_curve_table = candela_coeff_2p25;
		break;
	default:
		gamma_curve_table = candela_coeff_2p0;
		pr_err("%s, (%d.%d) gamma not exist, use default 2.0 gamma\n",
				__func__, gamma_curve / 100,
				gamma_curve - (gamma_curve / 100) * 100);
		break;
	}

	return gamma_curve_table;
}

static void gamma_init(struct SMART_DIM *pSmart, char *dest, int size)
{
	long long candela_level[VOLT_MAX] = {-1, };
	int bl_index[VOLT_MAX] = {-1, };
	int gamma[VOLT_MAX][3];
	int bl_level, cnt, volt, offset, brightness, candela_index, gamma_curve;
	int max_brightness, *max_gamma_curve_table;
	int *gamma_curve_table;

	if (unlikely(!pSmart)) {
		pr_err("%s, no smart dim\n", __func__);
		return;
	}

	if (unlikely(!dest)) {
		pr_err("%s, no dest of mtp\n", __func__);
		return;
	}

	brightness = pSmart->brightness_level;
	max_brightness = pSmart->plux_table[pSmart->lux_table_max - 1];
	candela_index = find_candela_index(pSmart, max_brightness);
	if (candela_index == -1) {
		pr_warn("%s, can't find candela\n", __func__);
		return;
	}
	gamma_curve = pSmart->gamma_curve_table[candela_index];
	max_gamma_curve_table = find_gamma_curve_table(gamma_curve);
	pr_debug("max_brightness %d, gamma (%d.%d)\n",
			max_brightness, gamma_curve / 100,
			gamma_curve - (gamma_curve / 100) * 100);

	candela_index =
		find_candela_index(pSmart, brightness);
	if (candela_index == -1) {
		pr_warn("%s, can't find candela\n", __func__);
		return;
	}
	bl_level = pSmart->base_lux_table[candela_index];
	gamma_curve = pSmart->gamma_curve_table[candela_index];
	gamma_curve_table = find_gamma_curve_table(gamma_curve);
	pr_debug("brightness %d, gamma (%d.%d)\n",
			brightness, gamma_curve / 100,
			gamma_curve - (gamma_curve / 100) * 100);
	if (unlikely(!gamma_curve_table)) {
		pr_warn("%s, can't find gamma curve table\n", __func__);
		return;
	}

	for (volt = VOLT_VT; volt < VOLT_MAX; volt++)
		candela_level[volt] =
			((long long)(gamma_curve_table[VT_CURVE[volt]])) *
			((long long)(bl_level));

#ifdef DYNAMIC_AID_DEBUG
	print_candela(candela_level);
#endif
	for (volt = VOLT_VT; volt < VOLT_MAX; volt++) {
		int index = searching_function(candela_level[volt],
				max_gamma_curve_table, max_brightness);
		if (unlikely(index < 0)) {
			pr_info("%s, failed to search (point : %d)\n",
					__func__, VT_CURVE[volt]);
			break;
		}
		bl_index[volt] = index;
	}

	/* Candela Compensation */
	for (cnt = VOLT_V3; cnt < VOLT_MAX; cnt++) {
		bl_index[VOLT_MAX - cnt] +=
			(pSmart->candela_offset)[candela_index * 9 + (cnt - 1)];
		if (bl_index[VOLT_MAX - cnt] == 0)
			bl_index[VOLT_MAX - cnt] = 1;
	}

#ifdef DYNAMIC_AID_DEBUG
	print_bl_index(bl_index);
#endif
	/*Generate Gamma table*/
	for (volt = VOLT_V255; volt >= VOLT_VT; volt--)
		voltage_hexa(bl_index, pSmart, gamma[volt], volt);

	/* RGB Color Shift Compensation */
	for (volt = VOLT_V255, cnt = 0; volt >= VOLT_V11; volt--)
		for (offset = R_OFFSET; offset <= B_OFFSET; offset++, cnt++)
			gamma[volt][offset] += (pSmart->rgb_offset)[candela_index * 24 + cnt];

	/* subtration MTP_OFFSET value from generated gamma table */
	for (volt = VOLT_V3; volt < VOLT_MAX; volt++)
		for (offset = R_OFFSET; offset <= B_OFFSET; offset++)
			gamma[volt][offset] -= MTP[volt][offset];

	/* copy to mtp buffer */
	mtp_itoc(dest, gamma);
}


void generate_gamma(struct SMART_DIM *pSmart, char *dest, int size)
{
	int lux_loop;
	struct illuminance_table *ptable = (struct illuminance_table *)
		(&(pSmart->gen_table));

	/* searching already generated gamma table */
	for (lux_loop = 0; lux_loop < pSmart->lux_table_max; lux_loop++) {
		if (ptable[lux_loop].lux == pSmart->brightness_level) {
			memcpy(dest, &(ptable[lux_loop].gamma_setting), size);
			break;
		}
	}

	/* searching fail... Setting 300CD value on gamma table */
	if (lux_loop == pSmart->lux_table_max) {
		pr_info("%s searching fail lux : %d\n", __func__,
				pSmart->brightness_level);
		memcpy(dest, max_lux_table, size);
	}

#ifdef DYNAMIC_AID_DEBUG
	if (lux_loop != pSmart->lux_table_max)
		pr_info("%s searching ok index : %d lux : %d\n", __func__,
				lux_loop, ptable[lux_loop].lux);
#endif
}

int smart_dimming_init(struct SMART_DIM *pSmart)
{
	int index, volt;

	mtp_ctoi(MTP, (char *)&(pSmart->MTP_ORIGN));
	mtp_itoc(max_lux_table, REF_MTP);

	/* voltage adjustment */
	voltage_adjustment(pSmart, VOLT_V255);
	voltage_adjustment(pSmart, VOLT_VT);
	for (volt = VOLT_V203; volt >= VOLT_V3; volt--)
		voltage_adjustment(pSmart, volt);

	if (generate_gray_scale(pSmart)) {
		pr_err("%s, failed to generate gray scale\n", __func__);
		return -EINVAL;
	}

	/* Generating lux_table */
	for (index = 0; index < pSmart->lux_table_max; index++) {
		pSmart->brightness_level = pSmart->plux_table[index];
		pSmart->gen_table[index].lux = pSmart->plux_table[index];
		gamma_init(pSmart,
				(char *)&pSmart->gen_table[index].gamma_setting,
				GAMMA_SET_MAX);
	}

	/* set max gamma table */
	memcpy(&(pSmart->gen_table[index - 1].gamma_setting),
			max_lux_table, GAMMA_SET_MAX);

#ifdef DYNAMIC_AID_DEBUG
	print_gamma_table(pSmart);
#endif

	return 0;
}

/* ----------------------------------------------------------------------------
 * Wrapper functions for smart dimming to work with 8974 generic code
 * ----------------------------------------------------------------------------
 */

static struct smartdim_conf sdim_conf;

static void wrap_generate_gamma(int cd, char *cmd_str)
{
	sdim_oled.brightness_level = cd;
	generate_gamma(&sdim_oled, cmd_str, GAMMA_SET_MAX);
}

static void wrap_smart_dimming_init(void)
{
	sdim_oled.plux_table = sdim_conf.lux_tab;
	sdim_oled.lux_table_max = sdim_conf.lux_tabsize;
	sdim_oled.rgb_offset = sdim_conf.rgb_offset;
	sdim_oled.candela_offset = sdim_conf.candela_offset;
	sdim_oled.base_lux_table = sdim_conf.base_lux_table;
	sdim_oled.gamma_curve_table = sdim_conf.gamma_curve_table;
	sdim_oled.vregout_voltage = sdim_conf.vregout_voltage;
	sdim_oled.ldi_revision = sdim_conf.man_id;
	smart_dimming_init(&sdim_oled);
}

struct smartdim_conf *sdim_get_conf(void)
{
	sdim_conf.generate_gamma = wrap_generate_gamma;
	sdim_conf.init = wrap_smart_dimming_init;
	sdim_conf.mtp_buffer = (char *)(&sdim_oled.MTP_ORIGN);
	return (struct smartdim_conf *)&sdim_conf;
}

/* ----------------------------------------------------------------------------
 * END - Wrapper
 * ----------------------------------------------------------------------------
 */
