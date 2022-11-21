/*
 * =================================================================
 *
 *       Filename:  smart_mtp_s6e3.c
 *
 *    Description:  Smart dimming algorithm implementation
 *
 *        Company:  Samsung Electronics
 *
 *			model:	K LTE
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

#include "smart_mtp_s6e3ha2x01.h"
#include "smart_dimming.h"

#include "smart_mtp_2p2_gamma.h"

static struct SMART_DIM smart_S6E3;
static struct smartdim_conf __S6E3__ ;

#if defined(CONFIG_LCD_HMT)
static struct SMART_DIM smart_S6E3_reverse_hmt_single;
static struct smartdim_conf_hmt __S6E3_REVERSE_HMT_S__;

typedef struct _PANEL_STATE_HMT_ {
	int panel_rev;
} PANEL_STATE_HMT;
#endif

/*#define SMART_DIMMING_DEBUG*/

static char max_lux_table[GAMMA_SET_MAX];
static char min_lux_table[GAMMA_SET_MAX];

/*
*	To support different center cell gamma setting
*/
static char V255_300CD_R_MSB;
static char V255_300CD_R_LSB;

static char V255_300CD_G_MSB;
static char V255_300CD_G_LSB;

static char V255_300CD_B_MSB;
static char V255_300CD_B_LSB;

static char V203_300CD_R;
static char V203_300CD_G;
static char V203_300CD_B;

static char V151_300CD_R;
static char V151_300CD_G;
static char V151_300CD_B;

static char V87_300CD_R;
static char V87_300CD_G;
static char V87_300CD_B;

static char V51_300CD_R;
static char V51_300CD_G;
static char V51_300CD_B;

static char V35_300CD_R;
static char V35_300CD_G;
static char V35_300CD_B;

static char V23_300CD_R;
static char V23_300CD_G;
static char V23_300CD_B;

static char V11_300CD_R;
static char V11_300CD_G;
static char V11_300CD_B;

static char V3_300CD_R;
static char V3_300CD_G;
static char V3_300CD_B;

static char V0_300CD_R;
static char V0_300CD_G;
static char V0_300CD_B;

static char VT_300CD_R;
static char VT_300CD_G;
static char VT_300CD_B;

static int char_to_int(char data1)
{
	int cal_data;

	if (data1 & 0x80) {
		cal_data = data1 & 0x7F;
		cal_data *= -1;
	} else
		cal_data = data1;

	return cal_data;
}

static int char_to_int_v255(char data1, char data2)
{
	int cal_data;

	if (data1)
		cal_data = data2 * -1;
	else
		cal_data = data2;

	return cal_data;
}

void print_RGB_offset(struct SMART_DIM *pSmart)
{
	pr_info("%s MTP Offset VT R:%d G:%d B:%d\n", __func__,
			char_to_int(pSmart->MTP.G_OFFSET.OFFSET_T & 0x0F),
			char_to_int(pSmart->MTP.G_OFFSET.OFFSET_T >> 4),
			char_to_int(pSmart->MTP.B_OFFSET.OFFSET_T));
	pr_info("%s MTP Offset V0 R:%d G:%d B:%d\n", __func__,
			char_to_int(pSmart->MTP.R_OFFSET.OFFSET_0),
			char_to_int(pSmart->MTP.G_OFFSET.OFFSET_0),
			char_to_int(pSmart->MTP.B_OFFSET.OFFSET_0));
	pr_info("%s MTP Offset V3 R:%d G:%d B:%d\n", __func__,
			char_to_int(pSmart->MTP.R_OFFSET.OFFSET_3),
			char_to_int(pSmart->MTP.G_OFFSET.OFFSET_3),
			char_to_int(pSmart->MTP.B_OFFSET.OFFSET_3));
	pr_info("%s MTP Offset V11 R:%d G:%d B:%d\n", __func__,
			char_to_int(pSmart->MTP.R_OFFSET.OFFSET_11),
			char_to_int(pSmart->MTP.G_OFFSET.OFFSET_11),
			char_to_int(pSmart->MTP.B_OFFSET.OFFSET_11));
	pr_info("%s MTP Offset V23 R:%d G:%d B:%d\n", __func__,
			char_to_int(pSmart->MTP.R_OFFSET.OFFSET_23),
			char_to_int(pSmart->MTP.G_OFFSET.OFFSET_23),
			char_to_int(pSmart->MTP.B_OFFSET.OFFSET_23));
	pr_info("%s MTP Offset V35 R:%d G:%d B:%d\n", __func__,
			char_to_int(pSmart->MTP.R_OFFSET.OFFSET_35),
			char_to_int(pSmart->MTP.G_OFFSET.OFFSET_35),
			char_to_int(pSmart->MTP.B_OFFSET.OFFSET_35));
	pr_info("%s MTP Offset V51 R:%d G:%d B:%d\n", __func__,
			char_to_int(pSmart->MTP.R_OFFSET.OFFSET_51),
			char_to_int(pSmart->MTP.G_OFFSET.OFFSET_51),
			char_to_int(pSmart->MTP.B_OFFSET.OFFSET_51));
	pr_info("%s MTP Offset V87 R:%d G:%d B:%d\n", __func__,
			char_to_int(pSmart->MTP.R_OFFSET.OFFSET_87),
			char_to_int(pSmart->MTP.G_OFFSET.OFFSET_87),
			char_to_int(pSmart->MTP.B_OFFSET.OFFSET_87));
	pr_info("%s MTP Offset V151 R:%d G:%d B:%d\n", __func__,
			char_to_int(pSmart->MTP.R_OFFSET.OFFSET_151),
			char_to_int(pSmart->MTP.G_OFFSET.OFFSET_151),
			char_to_int(pSmart->MTP.B_OFFSET.OFFSET_151));
	pr_info("%s MTP Offset V203 R:%d G:%d B:%d\n", __func__,
			char_to_int(pSmart->MTP.R_OFFSET.OFFSET_203),
			char_to_int(pSmart->MTP.G_OFFSET.OFFSET_203),
			char_to_int(pSmart->MTP.B_OFFSET.OFFSET_203));
	pr_info("%s MTP Offset V255 R:%d G:%d B:%d\n", __func__,
			char_to_int_v255(pSmart->MTP.R_OFFSET.OFFSET_255_MSB,
				pSmart->MTP.R_OFFSET.OFFSET_255_LSB),
			char_to_int_v255(pSmart->MTP.G_OFFSET.OFFSET_255_MSB,
				pSmart->MTP.G_OFFSET.OFFSET_255_LSB),
			char_to_int_v255(pSmart->MTP.B_OFFSET.OFFSET_255_MSB,
				pSmart->MTP.B_OFFSET.OFFSET_255_LSB));
}

void print_lux_table(struct SMART_DIM *psmart)
{
	int lux_loop;
	int cnt;
	char pBuffer[256];
	memset(pBuffer, 0x00, 256);

	for (lux_loop = 0; lux_loop < psmart->lux_table_max; lux_loop++) {
		for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %d",
				psmart->gen_table[lux_loop].gamma_setting[cnt]);

		pr_info("lux : %3d  %s", psmart->plux_table[lux_loop], pBuffer);
		memset(pBuffer, 0x00, 256);
	}
}

void print_aid_log(void)
{
	print_RGB_offset(&smart_S6E3);
	print_lux_table(&smart_S6E3);
}

#if defined(CONFIG_LCD_HMT)
void print_lux_table_hmt(struct SMART_DIM *psmart)
{
	int lux_loop;
	int cnt;
	char pBuffer[256];
	memset(pBuffer, 0x00, 256);

	for (lux_loop = 0; lux_loop < psmart->lux_table_max; lux_loop++) {
		for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %d",
				psmart->hmt_gen_table[lux_loop].gamma_setting[cnt]);

		pr_info("lux : %3d  %s\n", psmart->plux_table[lux_loop], pBuffer);
		memset(pBuffer, 0x00, 256);
	}
}

void print_aid_log_hmt_single(void)
{
	pr_info("== SINGLE SCAN ==\n");
	print_RGB_offset(&smart_S6E3_reverse_hmt_single);
	print_lux_table_hmt(&smart_S6E3_reverse_hmt_single);
	pr_info("\n");
}
#endif

#define v255_coefficient 72
#define v255_denominator 860
static int v255_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	unsigned long long add_mtp;
	int LSB;
	int v255_value;

	v255_value = (V255_300CD_R_MSB << 8) | (V255_300CD_R_LSB);
	LSB = char_to_int_v255(pSmart->MTP.R_OFFSET.OFFSET_255_MSB,
				pSmart->MTP.R_OFFSET.OFFSET_255_LSB);
	add_mtp = LSB + v255_value;
	result_1 = result_2 = (v255_coefficient+add_mtp) << BIT_SHIFT;
	do_div(result_2, v255_denominator);
	result_3 = (pSmart->vregout_voltage * result_2) >> BIT_SHIFT;
	result_4 = pSmart->vregout_voltage - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_255 = result_4;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_0 = pSmart->vregout_voltage;

	v255_value = (V255_300CD_G_MSB << 8) | (V255_300CD_G_LSB);
	LSB = char_to_int_v255(pSmart->MTP.G_OFFSET.OFFSET_255_MSB,
				pSmart->MTP.G_OFFSET.OFFSET_255_LSB);
	add_mtp = LSB + v255_value;
	result_1 = result_2 = (v255_coefficient+add_mtp) << BIT_SHIFT;
	do_div(result_2, v255_denominator);
	result_3 = (pSmart->vregout_voltage * result_2) >> BIT_SHIFT;
	result_4 = pSmart->vregout_voltage - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_255 = result_4;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_0 = pSmart->vregout_voltage;

	v255_value = (V255_300CD_B_MSB << 8) | (V255_300CD_B_LSB);
	LSB = char_to_int_v255(pSmart->MTP.B_OFFSET.OFFSET_255_MSB,
				pSmart->MTP.B_OFFSET.OFFSET_255_LSB);
	add_mtp = LSB + v255_value;
	result_1 = result_2 = (v255_coefficient+add_mtp) << BIT_SHIFT;
	do_div(result_2, v255_denominator);
	result_3 = (pSmart->vregout_voltage * result_2) >> BIT_SHIFT;
	result_4 = pSmart->vregout_voltage - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_255 = result_4;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_0 = pSmart->vregout_voltage;

#ifdef SMART_DIMMING_DEBUG
	pr_info("%s V255 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_255,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_255,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_255);
#endif

	return 0;
}

static void v255_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = pSmart->vregout_voltage -
		(pSmart->GRAY.TABLE[index[V255_INDEX]].R_Gray);
	result_2 = result_1 * v255_denominator;
	do_div(result_2, pSmart->vregout_voltage);
	result_3 = result_2  - v255_coefficient;
	str[0] = (result_3 & 0xff00) >> 8;
	str[1] = result_3 & 0xff;

	result_1 = pSmart->vregout_voltage -
		(pSmart->GRAY.TABLE[index[V255_INDEX]].G_Gray);
	result_2 = result_1 * v255_denominator;
	do_div(result_2, pSmart->vregout_voltage);
	result_3 = result_2  - v255_coefficient;
	str[2] = (result_3 & 0xff00) >> 8;
	str[3] = result_3 & 0xff;

	result_1 = pSmart->vregout_voltage -
			(pSmart->GRAY.TABLE[index[V255_INDEX]].B_Gray);
	result_2 = result_1 * v255_denominator;
	do_div(result_2, pSmart->vregout_voltage);
	result_3 = result_2  - v255_coefficient;
	str[4] = (result_3 & 0xff00) >> 8;
	str[5] = result_3 & 0xff;

}

static int vt_coefficient[] = {
	0, 12, 24, 36, 48,
	60, 72, 84, 96, 108,
	138, 148, 158, 168,
	178, 186,
};
#define vt_denominator 860
static int vt_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	unsigned long long add_mtp;
	int LSB;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_T & 0x0F);
	add_mtp = LSB + VT_300CD_R;
	result_1 = result_2 = vt_coefficient[LSB] << BIT_SHIFT;
	do_div(result_2, vt_denominator);
	result_3 = (pSmart->vregout_voltage * result_2) >> BIT_SHIFT;
	result_4 = pSmart->vregout_voltage - result_3;
	pSmart->GRAY.VT_TABLE.R_Gray = result_4;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_T >> 4);
	add_mtp = LSB + VT_300CD_G;
	result_1 = result_2 = vt_coefficient[LSB] << BIT_SHIFT;
	do_div(result_2, vt_denominator);
	result_3 = (pSmart->vregout_voltage * result_2) >> BIT_SHIFT;
	result_4 = pSmart->vregout_voltage - result_3;
	pSmart->GRAY.VT_TABLE.G_Gray = result_4;

	LSB = char_to_int(pSmart->MTP.B_OFFSET.OFFSET_T);
	add_mtp = LSB + VT_300CD_B;
	result_1 = result_2 = vt_coefficient[LSB] << BIT_SHIFT;
	do_div(result_2, vt_denominator);
	result_3 = (pSmart->vregout_voltage * result_2) >> BIT_SHIFT;
	result_4 = pSmart->vregout_voltage - result_3;
	pSmart->GRAY.VT_TABLE.B_Gray = result_4;

#ifdef SMART_DIMMING_DEBUG
	pr_info("%s VT RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->GRAY.VT_TABLE.R_Gray,
			pSmart->GRAY.VT_TABLE.G_Gray,
			pSmart->GRAY.VT_TABLE.B_Gray);
#endif

	return 0;

}

static void vt_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	str[33] = VT_300CD_G;
	str[34] = VT_300CD_B;
}

#define v203_coefficient 64
#define v203_denominator 320
static int v203_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	unsigned long long add_mtp;
	int LSB;

	LSB = char_to_int(pSmart->MTP.R_OFFSET.OFFSET_203);
	add_mtp = LSB + V203_300CD_R;
	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
				- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_255);
	result_2 = (v203_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v203_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.R_Gray) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_203 = result_4;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_203);
	add_mtp = LSB + V203_300CD_G;
	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
				- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_255);
	result_2 = (v203_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v203_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.G_Gray) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_203 = result_4;

	LSB = char_to_int(pSmart->MTP.B_OFFSET.OFFSET_203);
	add_mtp = LSB + V203_300CD_B;
	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
				- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_255);
	result_2 = (v203_coefficient+add_mtp) << BIT_SHIFT;
	do_div(result_2, v203_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.B_Gray) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_203 = result_4;

#ifdef SMART_DIMMING_DEBUG
	pr_info("%s V203 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_203,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_203,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_203);
#endif

	return 0;

}

static void v203_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V203_INDEX]].R_Gray);
	result_2 = result_1 * v203_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V255_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[6] = (result_2  - v203_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V203_INDEX]].G_Gray);
	result_2 = result_1 * v203_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V255_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[7] = (result_2  - v203_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V203_INDEX]].B_Gray);
	result_2 = result_1 * v203_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V255_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[8] = (result_2  - v203_coefficient) & 0xff;

}

#define v151_coefficient 64
#define v151_denominator 320
static int v151_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	unsigned long long add_mtp;
	int LSB;

	LSB = char_to_int(pSmart->MTP.R_OFFSET.OFFSET_151);
	add_mtp = LSB + V151_300CD_R;
	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_203);
	result_2 = (v151_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v151_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.R_Gray) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_151 = result_4;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_151);
	add_mtp = LSB + V151_300CD_G;
	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_203);
	result_2 = (v151_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v151_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.G_Gray) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_151 = result_4;

	LSB = char_to_int(pSmart->MTP.B_OFFSET.OFFSET_151);
	add_mtp = LSB + V151_300CD_B;
	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_203);
	result_2 = (v151_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v151_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.B_Gray) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_151 = result_4;

#ifdef SMART_DIMMING_DEBUG
	pr_info("%s V151 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_151,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_151,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_151);
#endif

	return 0;

}

static void v151_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V151_INDEX]].R_Gray);
	result_2 = result_1 * v151_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V203_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[9] = (result_2  - v151_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V151_INDEX]].G_Gray);
	result_2 = result_1 * v151_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V203_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[10] = (result_2  - v151_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V151_INDEX]].B_Gray);
	result_2 = result_1 * v151_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V203_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[11] = (result_2  - v151_coefficient) & 0xff;
}

#define v87_coefficient 64
#define v87_denominator 320
static int v87_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	unsigned long long add_mtp;
	int LSB;

	LSB = char_to_int(pSmart->MTP.R_OFFSET.OFFSET_87);
	add_mtp = LSB + V87_300CD_R;
	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_151);
	result_2 = (v87_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v87_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.R_Gray) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_87 = result_4;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_87);
	add_mtp = LSB + V87_300CD_G;
	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_151);
	result_2 = (v87_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v87_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.G_Gray) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_87 = result_4;

	LSB = char_to_int(pSmart->MTP.B_OFFSET.OFFSET_87);
	add_mtp = LSB + V87_300CD_B;
	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_151);
	result_2 = (v87_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v87_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.B_Gray) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_87 = result_4;

#ifdef SMART_DIMMING_DEBUG
	pr_info("%s V87 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_87,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_87,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_87);
#endif

	return 0;

}

static void v87_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V87_INDEX]].R_Gray);
	result_2 = result_1 * v87_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V151_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[12] = (result_2  - v87_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V87_INDEX]].G_Gray);
	result_2 = result_1 * v87_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V151_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[13] = (result_2  - v87_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V87_INDEX]].B_Gray);
	result_2 = result_1 * v87_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V151_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[14] = (result_2  - v87_coefficient) & 0xff;
}

#define v51_coefficient 64
#define v51_denominator 320
static int v51_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	unsigned long long add_mtp;
	int LSB;

	LSB = char_to_int(pSmart->MTP.R_OFFSET.OFFSET_51);
	add_mtp = LSB + V51_300CD_R;
	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_87);
	result_2 = (v51_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v51_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.R_Gray) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_51 = result_4;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_51);
	add_mtp = LSB + V51_300CD_G;
	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_87);
	result_2 = (v51_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v51_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.G_Gray) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_51 = result_4;

	LSB = char_to_int(pSmart->MTP.B_OFFSET.OFFSET_51);
	add_mtp = LSB + V51_300CD_B;
	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_87);
	result_2 = (v51_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v51_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.B_Gray) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_51 = result_4;

#ifdef SMART_DIMMING_DEBUG
	pr_info("%s V51 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_51,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_51,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_51);
#endif

	return 0;

}

static void v51_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V51_INDEX]].R_Gray);
	result_2 = result_1 * v51_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V87_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[15] = (result_2  - v51_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V51_INDEX]].G_Gray);
	result_2 = result_1 * v51_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V87_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[16] = (result_2  - v51_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V51_INDEX]].B_Gray);
	result_2 = result_1 * v51_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V87_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[17] = (result_2  - v51_coefficient) & 0xff;

}

#define v35_coefficient 64
#define v35_denominator 320
static int v35_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	unsigned long long add_mtp;
	int LSB;

	LSB = char_to_int(pSmart->MTP.R_OFFSET.OFFSET_35);
	add_mtp = LSB + V35_300CD_R;
	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_51);
	result_2 = (v35_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v35_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.R_Gray) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_35 = result_4;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_35);
	add_mtp = LSB + V35_300CD_G;
	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_51);
	result_2 = (v35_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v35_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.G_Gray) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_35 = result_4;

	LSB = char_to_int(pSmart->MTP.B_OFFSET.OFFSET_35);
	add_mtp = LSB + V35_300CD_B;
	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_51);
	result_2 = (v35_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v35_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.B_Gray) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_35 = result_4;

#ifdef SMART_DIMMING_DEBUG
	pr_info("%s V35 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_35,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_35,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_35);
#endif

	return 0;

}

static void v35_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V35_INDEX]].R_Gray);
	result_2 = result_1 * v35_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V51_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[18] = (result_2  - v35_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V35_INDEX]].G_Gray);
	result_2 = result_1 * v35_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V51_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[19] = (result_2  - v35_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V35_INDEX]].B_Gray);
	result_2 = result_1 * v35_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V51_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[20] = (result_2  - v35_coefficient) & 0xff;

}

#define v23_coefficient 64
#define v23_denominator 320
static int v23_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	unsigned long long add_mtp;
	int LSB;

	LSB = char_to_int(pSmart->MTP.R_OFFSET.OFFSET_23);
	add_mtp = LSB + V23_300CD_R;
	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_35);
	result_2 = (v23_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v23_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.R_Gray) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_23 = result_4;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_23);
	add_mtp = LSB + V23_300CD_G;
	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_35);
	result_2 = (v23_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v23_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.G_Gray) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_23 = result_4;

	LSB = char_to_int(pSmart->MTP.B_OFFSET.OFFSET_23);
	add_mtp = LSB + V23_300CD_B;
	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_35);
	result_2 = (v23_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v23_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.B_Gray) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_23 = result_4;

#ifdef SMART_DIMMING_DEBUG
	pr_info("%s V23 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_23,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_23,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_23);
#endif

	return 0;

}

static void v23_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V23_INDEX]].R_Gray);
	result_2 = result_1 * v23_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V35_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[21] = (result_2  - v23_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V23_INDEX]].G_Gray);
	result_2 = result_1 * v23_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V35_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[22] = (result_2  - v23_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V23_INDEX]].B_Gray);
	result_2 = result_1 * v23_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V35_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[23] = (result_2  - v23_coefficient) & 0xff;

}

#define v11_coefficient 64
#define v11_denominator 320
static int v11_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	unsigned long long add_mtp;
	int LSB;

	LSB = char_to_int(pSmart->MTP.R_OFFSET.OFFSET_11);
	add_mtp = LSB + V11_300CD_R;
	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_23);
	result_2 = (v11_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v11_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.R_Gray) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_11 = result_4;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_11);
	add_mtp = LSB + V11_300CD_G;
	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_23);
	result_2 = (v11_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v11_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.G_Gray) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_11 = result_4;

	LSB = char_to_int(pSmart->MTP.B_OFFSET.OFFSET_11);
	add_mtp = LSB + V11_300CD_B;
	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_23);
	result_2 = (v11_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v11_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.B_Gray) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_11 = result_4;

#ifdef SMART_DIMMING_DEBUG
	pr_info("%s V11 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_11,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_11,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_11);
#endif

	return 0;

}

static void v11_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V11_INDEX]].R_Gray);
	result_2 = result_1 * v11_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V23_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[24] = (result_2  - v11_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V11_INDEX]].G_Gray);
	result_2 = result_1 * v11_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V23_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[25] = (result_2  - v11_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V11_INDEX]].B_Gray);
	result_2 = result_1 * v11_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V23_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[26] = (result_2  - v11_coefficient) & 0xff;

}

#define v3_coefficient 64
#define v3_denominator 320
static int v3_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	unsigned long long add_mtp;
	int LSB;

	LSB = char_to_int(pSmart->MTP.R_OFFSET.OFFSET_3);
	add_mtp = LSB + V3_300CD_R;
	result_1 = (pSmart->vregout_voltage)
			- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_11);
	result_2 = (v3_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v3_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->vregout_voltage) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_3 = result_4;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_3);
	add_mtp = LSB + V3_300CD_G;
	result_1 = (pSmart->vregout_voltage)
			- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_11);
	result_2 = (v3_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v3_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->vregout_voltage) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_3 = result_4;

	LSB = char_to_int(pSmart->MTP.B_OFFSET.OFFSET_3);
	add_mtp = LSB + V3_300CD_B;
	result_1 = (pSmart->vregout_voltage)
			- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_11);
	result_2 = (v3_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v3_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->vregout_voltage) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_3 = result_4;

#ifdef SMART_DIMMING_DEBUG
	pr_info("%s V3 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_3,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_3,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_3);
#endif

	return 0;

}

static void v3_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = (pSmart->vregout_voltage)
			- (pSmart->GRAY.TABLE[index[V3_INDEX]].R_Gray);
	result_2 = result_1 * v3_denominator;
	result_3 = (pSmart->vregout_voltage)
			- (pSmart->GRAY.TABLE[index[V11_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[27] = (result_2  - v3_coefficient) & 0xff;

	result_1 = (pSmart->vregout_voltage)
			- (pSmart->GRAY.TABLE[index[V3_INDEX]].G_Gray);
	result_2 = result_1 * v3_denominator;
	result_3 = (pSmart->vregout_voltage)
			- (pSmart->GRAY.TABLE[index[V11_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[28] = (result_2  - v3_coefficient) & 0xff;

	result_1 = (pSmart->vregout_voltage)
			- (pSmart->GRAY.TABLE[index[V3_INDEX]].B_Gray);
	result_2 = result_1 * v3_denominator;
	result_3 = (pSmart->vregout_voltage)
			- (pSmart->GRAY.TABLE[index[V11_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[29] = (result_2  - v3_coefficient) & 0xff;

}

static void v0_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	str[30] = V0_300CD_R;
	str[31] = V0_300CD_G;
	str[32] = V0_300CD_B;
}

/*V0,V1,V3,V11,V23,V35,V51,V87,V151,V203,V255*/
static int S6E3_ARRAY[S6E3_MAX] = {0, 1, 3, 11, 23, 35, 51, 87, 151, 203, 255};

#define V0toV3_Coefficient 2
#define V0toV3_Multiple 1
#define V0toV3_denominator 3

#define V3toV11_Coefficient 7
#define V3toV11_Multiple 1
#define V3toV11_denominator 8

#define V11toV23_Coefficient 11
#define V11toV23_Multiple 1
#define V11toV23_denominator 12

#define V23toV35_Coefficient 11
#define V23toV35_Multiple 1
#define V23toV35_denominator 12

#define V35toV51_Coefficient 15
#define V35toV51_Multiple 1
#define V35toV51_denominator 16

#define V51toV87_Coefficient 35
#define V51toV87_Multiple 1
#define V51toV87_denominator 36

#define V87toV151_Coefficient 63
#define V87toV151_Multiple 1
#define V87toV151_denominator 64

#define V151toV203_Coefficient 51
#define V151toV203_Multiple 1
#define V151toV203_denominator 52

#define V203toV255_Coefficient 51
#define V203toV255_Multiple 1
#define V203toV255_denominator 52

static int cal_gray_scale_linear(int up, int low, int coeff,
int mul, int deno, int cnt)
{
	unsigned long long result_1, result_2, result_3, result_4;

	result_1 = up - low;
	result_2 = (result_1 * (coeff - (cnt * mul))) << BIT_SHIFT;
	do_div(result_2, deno);
	result_3 = result_2 >> BIT_SHIFT;
	result_4 = low + result_3;

	return (int)result_4;
}

static int generate_gray_scale(struct SMART_DIM *pSmart)
{
	int cnt = 0, cal_cnt = 0;
	int array_index = 0;
	struct GRAY_VOLTAGE *ptable = (struct GRAY_VOLTAGE *)
						(&(pSmart->GRAY.TABLE));

	for (cnt = 0; cnt < S6E3_MAX; cnt++) {
		pSmart->GRAY.TABLE[S6E3_ARRAY[cnt]].R_Gray =
			((int *)&(pSmart->RGB_OUTPUT.R_VOLTAGE))[cnt];

		pSmart->GRAY.TABLE[S6E3_ARRAY[cnt]].G_Gray =
			((int *)&(pSmart->RGB_OUTPUT.G_VOLTAGE))[cnt];

		pSmart->GRAY.TABLE[S6E3_ARRAY[cnt]].B_Gray =
			((int *)&(pSmart->RGB_OUTPUT.B_VOLTAGE))[cnt];
	}

	/*
		below codes use hard coded value.
		So it is possible to modify on each model.
		V0,V1,V3,V11,V23,V35,V51,V87,V151,V203,V255
	*/
	for (cnt = 0; cnt < S6E3_GRAY_SCALE_MAX; cnt++) {

		if (cnt == S6E3_ARRAY[0]) {
			/* 0 */
			array_index = 0;
			cal_cnt = 0;
		} else if ((cnt > S6E3_ARRAY[0]) &&
			(cnt < S6E3_ARRAY[2])) {
			/* 1 ~ 2 */
			array_index = 2;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-2]].R_Gray,
			ptable[S6E3_ARRAY[array_index]].R_Gray,
			V0toV3_Coefficient, V0toV3_Multiple,
			V0toV3_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-2]].G_Gray,
			ptable[S6E3_ARRAY[array_index]].G_Gray,
			V0toV3_Coefficient, V0toV3_Multiple,
			V0toV3_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-2]].B_Gray,
			ptable[S6E3_ARRAY[array_index]].B_Gray,
			V0toV3_Coefficient, V0toV3_Multiple,
			V0toV3_denominator , cal_cnt);

			cal_cnt++;
		} else if (cnt == S6E3_ARRAY[2]) {
			/* 3 */
			cal_cnt = 0;
		} else if ((cnt > S6E3_ARRAY[2]) &&
			(cnt < S6E3_ARRAY[3])) {
			/* 4 ~ 10 */
			array_index = 3;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].R_Gray,
			ptable[S6E3_ARRAY[array_index]].R_Gray,
			V3toV11_Coefficient, V3toV11_Multiple,
			V3toV11_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].G_Gray,
			ptable[S6E3_ARRAY[array_index]].G_Gray,
			V3toV11_Coefficient, V3toV11_Multiple,
			V3toV11_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].B_Gray,
			ptable[S6E3_ARRAY[array_index]].B_Gray,
			V3toV11_Coefficient, V3toV11_Multiple,
			V3toV11_denominator , cal_cnt);

			cal_cnt++;
		} else if (cnt == S6E3_ARRAY[3]) {
			/* 11 */
			cal_cnt = 0;
		} else if ((cnt > S6E3_ARRAY[3]) &&
			(cnt < S6E3_ARRAY[4])) {
			/* 12 ~ 22 */
			array_index = 4;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].R_Gray,
			ptable[S6E3_ARRAY[array_index]].R_Gray,
			V11toV23_Coefficient, V11toV23_Multiple,
			V11toV23_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].G_Gray,
			ptable[S6E3_ARRAY[array_index]].G_Gray,
			V11toV23_Coefficient, V11toV23_Multiple,
			V11toV23_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].B_Gray,
			ptable[S6E3_ARRAY[array_index]].B_Gray,
			V11toV23_Coefficient, V11toV23_Multiple,
			V11toV23_denominator , cal_cnt);

			cal_cnt++;
		}  else if (cnt == S6E3_ARRAY[4]) {
			/* 23 */
			cal_cnt = 0;
		} else if ((cnt > S6E3_ARRAY[4]) &&
			(cnt < S6E3_ARRAY[5])) {
			/* 24 ~ 34 */
			array_index = 5;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].R_Gray,
			ptable[S6E3_ARRAY[array_index]].R_Gray,
			V23toV35_Coefficient, V23toV35_Multiple,
			V23toV35_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].G_Gray,
			ptable[S6E3_ARRAY[array_index]].G_Gray,
			V23toV35_Coefficient, V23toV35_Multiple,
			V23toV35_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].B_Gray,
			ptable[S6E3_ARRAY[array_index]].B_Gray,
			V23toV35_Coefficient, V23toV35_Multiple,
			V23toV35_denominator , cal_cnt);

			cal_cnt++;
		} else if (cnt == S6E3_ARRAY[5]) {
			/* 35 */
			cal_cnt = 0;
		} else if ((cnt > S6E3_ARRAY[5]) &&
			(cnt < S6E3_ARRAY[6])) {
			/* 36 ~ 50 */
			array_index = 6;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].R_Gray,
			ptable[S6E3_ARRAY[array_index]].R_Gray,
			V35toV51_Coefficient, V35toV51_Multiple,
			V35toV51_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].G_Gray,
			ptable[S6E3_ARRAY[array_index]].G_Gray,
			V35toV51_Coefficient, V35toV51_Multiple,
			V35toV51_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].B_Gray,
			ptable[S6E3_ARRAY[array_index]].B_Gray,
			V35toV51_Coefficient, V35toV51_Multiple,
			V35toV51_denominator, cal_cnt);
			cal_cnt++;

		} else if (cnt == S6E3_ARRAY[6]) {
			/* 51 */
			cal_cnt = 0;
		} else if ((cnt > S6E3_ARRAY[6]) &&
			(cnt < S6E3_ARRAY[7])) {
			/* 52 ~ 86 */
			array_index = 7;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].R_Gray,
			ptable[S6E3_ARRAY[array_index]].R_Gray,
			V51toV87_Coefficient, V51toV87_Multiple,
			V51toV87_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].G_Gray,
			ptable[S6E3_ARRAY[array_index]].G_Gray,
			V51toV87_Coefficient, V51toV87_Multiple,
			V51toV87_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].B_Gray,
			ptable[S6E3_ARRAY[array_index]].B_Gray,
			V51toV87_Coefficient, V51toV87_Multiple,
			V51toV87_denominator, cal_cnt);
			cal_cnt++;

		} else if (cnt == S6E3_ARRAY[7]) {
			/* 87 */
			cal_cnt = 0;
		} else if ((cnt > S6E3_ARRAY[7]) &&
			(cnt < S6E3_ARRAY[8])) {
			/* 88 ~ 150 */
			array_index = 8;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].R_Gray,
			ptable[S6E3_ARRAY[array_index]].R_Gray,
			V87toV151_Coefficient, V87toV151_Multiple,
			V87toV151_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].G_Gray,
			ptable[S6E3_ARRAY[array_index]].G_Gray,
			V87toV151_Coefficient, V87toV151_Multiple,
			V87toV151_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].B_Gray,
			ptable[S6E3_ARRAY[array_index]].B_Gray,
			V87toV151_Coefficient, V87toV151_Multiple,
			V87toV151_denominator, cal_cnt);

			cal_cnt++;
		} else if (cnt == S6E3_ARRAY[8]) {
			/* 151 */
			cal_cnt = 0;
		} else if ((cnt > S6E3_ARRAY[8]) &&
			(cnt < S6E3_ARRAY[9])) {
			/* 152 ~ 202 */
			array_index = 9;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].R_Gray,
			ptable[S6E3_ARRAY[array_index]].R_Gray,
			V151toV203_Coefficient, V151toV203_Multiple,
			V151toV203_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].G_Gray,
			ptable[S6E3_ARRAY[array_index]].G_Gray,
			V151toV203_Coefficient, V151toV203_Multiple,
			V151toV203_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].B_Gray,
			ptable[S6E3_ARRAY[array_index]].B_Gray,
			V151toV203_Coefficient, V151toV203_Multiple,
			V151toV203_denominator, cal_cnt);

			cal_cnt++;
		} else if (cnt == S6E3_ARRAY[9]) {
			/* 203 */
			cal_cnt = 0;
		} else if ((cnt > S6E3_ARRAY[9]) &&
			(cnt < S6E3_ARRAY[10])) {
			/* 204 ~ 254 */
			array_index = 10;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].R_Gray,
			ptable[S6E3_ARRAY[array_index]].R_Gray,
			V203toV255_Coefficient, V203toV255_Multiple,
			V203toV255_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].G_Gray,
			ptable[S6E3_ARRAY[array_index]].G_Gray,
			V203toV255_Coefficient, V203toV255_Multiple,
			V203toV255_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
			ptable[S6E3_ARRAY[array_index-1]].B_Gray,
			ptable[S6E3_ARRAY[array_index]].B_Gray,
			V203toV255_Coefficient, V203toV255_Multiple,
			V203toV255_denominator, cal_cnt);

			cal_cnt++;
		 } else {
			if (cnt == S6E3_ARRAY[10]) {
				pr_debug("%s end\n", __func__);
			} else {
				pr_err("%s fail cnt:%d\n", __func__, cnt);
				return -EINVAL;
			}
		}

	}

#ifdef SMART_DIMMING_DEBUG
		for (cnt = 0; cnt < S6E3_GRAY_SCALE_MAX; cnt++) {
			pr_info("%s %8d %8d %8d %d\n", __func__,
				pSmart->GRAY.TABLE[cnt].R_Gray,
				pSmart->GRAY.TABLE[cnt].G_Gray,
				pSmart->GRAY.TABLE[cnt].B_Gray, cnt);
		}
#endif
	return 0;
}

char offset_cal(int offset,  int value)
{
	if (value - offset < 0)
		return 0;
	else if (value - offset > 255)
		return 0xFF;
	else
		return value - offset;
}

void mtp_offset_substraction(struct SMART_DIM *pSmart, int *str)
{
	int level_255_temp = 0;
	int level_255_temp_MSB = 0;
	int MTP_V255;

	/*subtration MTP_OFFSET value from generated gamma table*/
	level_255_temp = (str[0] << 8) | str[1] ;
	MTP_V255 = char_to_int_v255(pSmart->MTP.R_OFFSET.OFFSET_255_MSB,
				pSmart->MTP.R_OFFSET.OFFSET_255_LSB);
	level_255_temp -=  MTP_V255;
	level_255_temp_MSB = level_255_temp / 256;
	str[0] = level_255_temp_MSB & 0xff;
	str[1] = level_255_temp & 0xff;

	level_255_temp = (str[2] << 8) | str[3] ;
	MTP_V255 = char_to_int_v255(pSmart->MTP.G_OFFSET.OFFSET_255_MSB,
				pSmart->MTP.G_OFFSET.OFFSET_255_LSB);
	level_255_temp -=  MTP_V255;
	level_255_temp_MSB = level_255_temp / 256;
	str[2] = level_255_temp_MSB & 0xff;
	str[3] = level_255_temp & 0xff;

	level_255_temp = (str[4] << 8) | str[5] ;
	MTP_V255 = char_to_int_v255(pSmart->MTP.B_OFFSET.OFFSET_255_MSB,
				pSmart->MTP.B_OFFSET.OFFSET_255_LSB);
	level_255_temp -=  MTP_V255;
	level_255_temp_MSB = level_255_temp / 256;
	str[4] = level_255_temp_MSB & 0xff;
	str[5] = level_255_temp & 0xff;

	str[6] = offset_cal(char_to_int(pSmart->MTP.R_OFFSET.OFFSET_203), str[6]);
	str[7] = offset_cal(char_to_int(pSmart->MTP.G_OFFSET.OFFSET_203), str[7]);
	str[8] = offset_cal(char_to_int(pSmart->MTP.B_OFFSET.OFFSET_203), str[8]);

	str[9] = offset_cal(char_to_int(pSmart->MTP.R_OFFSET.OFFSET_151), str[9]);
	str[10] = offset_cal(char_to_int(pSmart->MTP.G_OFFSET.OFFSET_151), str[10]);
	str[11] = offset_cal(char_to_int(pSmart->MTP.B_OFFSET.OFFSET_151), str[11]);

	str[12] = offset_cal(char_to_int(pSmart->MTP.R_OFFSET.OFFSET_87), str[12]);
	str[13] = offset_cal(char_to_int(pSmart->MTP.G_OFFSET.OFFSET_87), str[13]);
	str[14] = offset_cal(char_to_int(pSmart->MTP.B_OFFSET.OFFSET_87), str[14]);

	str[15] = offset_cal(char_to_int(pSmart->MTP.R_OFFSET.OFFSET_51), str[15]);
	str[16] = offset_cal(char_to_int(pSmart->MTP.G_OFFSET.OFFSET_51), str[16]);
	str[17] = offset_cal(char_to_int(pSmart->MTP.B_OFFSET.OFFSET_51), str[17]);

	str[18] = offset_cal(char_to_int(pSmart->MTP.R_OFFSET.OFFSET_35), str[18]);
	str[19] = offset_cal(char_to_int(pSmart->MTP.G_OFFSET.OFFSET_35), str[19]);
	str[20] = offset_cal(char_to_int(pSmart->MTP.B_OFFSET.OFFSET_35), str[20]);

	str[21] = offset_cal(char_to_int(pSmart->MTP.R_OFFSET.OFFSET_23), str[21]);
	str[22] = offset_cal(char_to_int(pSmart->MTP.G_OFFSET.OFFSET_23), str[22]);
	str[23] = offset_cal(char_to_int(pSmart->MTP.B_OFFSET.OFFSET_23), str[23]);

	str[24] = offset_cal(char_to_int(pSmart->MTP.R_OFFSET.OFFSET_11), str[24]);
	str[25] = offset_cal(char_to_int(pSmart->MTP.G_OFFSET.OFFSET_11), str[25]);
	str[26] = offset_cal(char_to_int(pSmart->MTP.B_OFFSET.OFFSET_11), str[26]);

	str[27] = offset_cal(char_to_int(pSmart->MTP.R_OFFSET.OFFSET_3), str[27]);
	str[28] = offset_cal(char_to_int(pSmart->MTP.G_OFFSET.OFFSET_3), str[28]);
	str[29] = offset_cal(char_to_int(pSmart->MTP.B_OFFSET.OFFSET_3), str[29]);
}

static int searching_function(long long candela, int *index, int gamma_curve)
{
	long long delta_1 = 0, delta_2 = 0;
	int cnt;

	/*
	*	This searching_functin should be changed with improved
		searcing algorithm to reduce searching time.
	*/
	*index = -1;

	for (cnt = 0; cnt < (S6E3_GRAY_SCALE_MAX-1); cnt++) {
		if (gamma_curve == GAMMA_CURVE_1P9) {
			delta_1 = candela - curve_1p9_350[cnt];
			delta_2 = candela - curve_1p9_350[cnt+1];
		} else if (gamma_curve == GAMMA_CURVE_2P15) {
			delta_1 = candela - curve_2p15_350[cnt];
			delta_2 = candela - curve_2p15_350[cnt+1];
		} else if (gamma_curve == GAMMA_CURVE_2P2) {
			delta_1 = candela - curve_2p2_350[cnt];
			delta_2 = candela - curve_2p2_350[cnt+1];
		} else {
			delta_1 = candela - curve_2p2_350[cnt];
			delta_2 = candela - curve_2p2_350[cnt+1];
		}

		if (delta_2 < 0) {
			*index = (delta_1 + delta_2) <= 0 ? cnt : cnt+1;
			break;
		}

		if (delta_1 == 0) {
			*index = cnt;
			break;
		}

		if (delta_2 == 0) {
			*index = cnt+1;
			break;
		}
	}

	if (*index == -1)
		return -EINVAL;
	else
		return 0;
}
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQHD_PT_PANEL) || defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQXGA_PT_PANEL)
static int searching_function_360(long long candela, int *index, int gamma_curve)
{
	long long delta_1 = 0, delta_2 = 0;
	int cnt;

	/*
	*	This searching_functin should be changed with improved
		searcing algorithm to reduce searching time.
	*/
	*index = -1;

	for (cnt = 0; cnt < (S6E3_GRAY_SCALE_MAX-1); cnt++) {
		if (gamma_curve == GAMMA_CURVE_1P9) {
			delta_1 = candela - curve_1p9_360[cnt];
			delta_2 = candela - curve_1p9_360[cnt+1];
		} else if (gamma_curve == GAMMA_CURVE_2P15) {
			delta_1 = candela - curve_2p15_360[cnt];
			delta_2 = candela - curve_2p15_360[cnt+1];
		} else if (gamma_curve == GAMMA_CURVE_2P2) {
			delta_1 = candela - curve_2p2_360[cnt];
			delta_2 = candela - curve_2p2_360[cnt+1];
		} else{
			delta_1 = candela - curve_2p2_360[cnt];
			delta_2 = candela - curve_2p2_360[cnt+1];
		}

		if (delta_2 < 0) {
			*index = (delta_1 + delta_2) <= 0 ? cnt : cnt+1;
			break;
		}

		if (delta_1 == 0) {
			*index = cnt;
			break;
		}

		if (delta_2 == 0) {
			*index = cnt+1;
			break;
		}
	}

	if (*index == -1)
		return -EINVAL;
	else
		return 0;
}
#endif
/* -1 means V1 */
#define S6E3_TABLE_MAX  (S6E3_MAX-1)
static void(*Make_hexa[S6E3_TABLE_MAX+1])(int*, struct SMART_DIM*, char*) = {
	v255_hexa,
	v203_hexa,
	v151_hexa,
	v87_hexa,
	v51_hexa,
	v35_hexa,
	v23_hexa,
	v11_hexa,
	v3_hexa,
	v0_hexa,
	vt_hexa,
};

#if defined(AID_OPERATION)
/*
*	Because of AID operation & display quality.
*
*	only smart dimmg range : 350CD ~ 183CD
*	AOR fix range : 172CD ~ 111CD  AOR 40%
*	AOR adjust range : 105CD ~ 10CD
*/
#define AOR_FIX_CD		172
#define AOR_ADJUST_CD	111
#define AOR_DIM_BASE_CD	116
#define AOR_DIM_BASE_CD_REVA	116
#define AOR_DIM_BASE_CD_REVA_WQXGA 110
#define AOR_DIM_BASE_CD_REVB_WQXGA 112
#define AOR_DIM_BASE_CD_REVD_WQXGA 114

#define AOR_DIM_BASE_CD_REVC 117
#define AOR_DIM_BASE_CD_REVF	119
#define AOR_DIM_BASE_CD_REVH	113


#define CCG6_MAX_TABLE 64
static int ccg6_candela_table[][2] = {
	{2, 0,},
	{3, 1,},
	{4, 2,},
	{5, 3,},
	{6, 4,},
	{7, 5,},
	{8, 6,},
	{9, 7,},
	{10, 8,},
	{11, 9,},
	{12, 10,},
	{13, 11,},
	{14, 12,},
	{15, 13,},
	{16, 14,},
	{17, 15,},
	{19, 16,},
	{20, 17,},
	{21, 18,},
	{22, 19,},
	{24, 20,},
	{25, 21,},
	{27, 22,},
	{29, 23,},
	{30, 24,},
	{32, 25,},
	{34, 26,},
	{37, 27,},
	{39, 28,},
	{41, 29,},
	{44, 30,},
	{47, 31,},
	{50, 32,},
	{53, 33,},
	{56, 34,},
	{60, 35,},
	{64, 36,},
	{68, 37,},
	{72, 38,},
	{77, 39,},
	{82, 40,},
	{87, 41,},
	{93, 42,},
	{98, 43,},
	{105, 44,},
	{111, 45,},
	{119, 46,},
	{126, 47,},
	{134, 48,},
	{143, 49,},
	{152, 50,},
	{162, 51,},
	{172, 52,},
	{183, 53,},
	{195, 54,},
	{207, 55,},
	{220, 56,},
	{234, 57,},
	{249, 58,},
	{265, 59,},
	{282, 60,},
	{300, 61,},
	{316, 62,},
	{333, 63,},
	{350, 64,},
};

static int find_cadela_table(int brightness)
{
	int loop;
	int err = -1;

	for(loop = 0; loop <= CCG6_MAX_TABLE; loop++)
		if (ccg6_candela_table[loop][0] == brightness)
			return ccg6_candela_table[loop][1];

	return err;
}

#define RGB_COMPENSATION 24

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQHD_PT_PANEL)
static int gradation_offset_T_wqhd_revA[][9] = {
/*	V255 V203 V151 V87 V51 V35 V23 V11 V3 */
	{ 0, 7, 13, 19, 27, 31, 34, 32, 32 },
	{ 0, 6, 11, 15, 20, 25, 28, 32, 32 },
	{ 0, 5, 8, 12, 16, 20, 23, 27, 26 },
	{ 0, 5, 7, 11, 14, 17, 20, 25, 22 },
	{ 0, 4, 6, 9, 12, 15, 18, 22, 21 },
	{ 0, 4, 6, 8, 11, 13, 16, 20, 22 },
	{ 0, 3, 5, 8, 10, 12, 15, 19, 19 },
	{ 0, 3, 4, 7, 9, 11, 14, 18, 16 },
	{ 0, 3, 4, 6, 8, 10, 13, 16, 19 },
	{ 0, 3, 4, 6, 7, 9, 11, 15, 19 },
	{ 0, 3, 4, 6, 7, 9, 11, 15, 14 },
	{ 0, 3, 4, 5, 7, 8, 10, 14, 16 },
	{ 0, 3, 4, 5, 6, 8, 10, 14, 13 },
	{ 0, 3, 4, 5, 6, 7, 9, 13, 16 },
	{ 0, 3, 4, 4, 5, 7, 8, 13, 13 },
	{ 0, 3, 3, 5, 5, 7, 8, 13, 12 },
	{ 0, 3, 3, 4, 5, 6, 7, 12, 10 },
	{ 0, 3, 3, 4, 4, 6, 7, 12, 10 },
	{ 0, 3, 3, 4, 4, 6, 7, 11, 12 },
	{ 0, 3, 3, 4, 4, 6, 7, 11, 11 },
	{ 0, 2, 3, 4, 4, 5, 6, 10, 12 },
	{ 0, 3, 3, 4, 4, 5, 6, 10, 12 },
	{ 0, 3, 3, 3, 3, 4, 6, 9, 13 },
	{ 0, 2, 3, 3, 3, 4, 6, 9, 10 },
	{ 0, 2, 3, 3, 3, 4, 6, 9, 10 },
	{ 0, 2, 3, 3, 3, 4, 5, 8, 11 },
	{ 0, 2, 3, 3, 3, 3, 5, 8, 9 },
	{ 0, 2, 3, 3, 2, 3, 5, 8, 7 },
	{ 0, 2, 2, 2, 2, 3, 4, 7, 10 },
	{ 0, 2, 2, 2, 2, 3, 4, 7, 8 },
	{ 0, 2, 2, 2, 2, 3, 4, 6, 5 },
	{ 0, 2, 2, 2, 2, 2, 4, 6, 9 },
	{ 0, 2, 2, 2, 2, 2, 4, 6, 7 },
	{ 0, 2, 2, 2, 2, 2, 3, 6, 4 },
	{ 0, 2, 2, 2, 1, 2, 3, 5, 7 },
	{ 0, 2, 2, 2, 1, 1, 3, 5, 4 },
	{ 0, 2, 2, 2, 1, 1, 3, 4, 8 },
	{ 0, 2, 1, 2, 1, 1, 2, 4, 5 },
	{ 0, 2, 1, 1, 1, 1, 2, 4, 3 },
	{ 0, 3, 0, 1, 1, 1, 3, 4, 5 },
	{ 0, 2, 2, 2, 1, 1, 3, 3, 5 },
	{ 0, 1, 1, 2, 0, 1, 2, 3, 4 },
	{ 0, 1, 0, 1, 0, 0, 2, 3, 5 },
	{ 0, 2, 2, 2, 2, 1, 2, 3, 7 },
	{ 0, 2, 2, 2, 1, 1, 2, 3, 6 },
	{ 0, 2, 3, 2, 1, 1, 1, 2, 6 },
	{ 0, 1, 3, 3, 1, 1, 2, 3, 2 },
	{ 0, 2, 4, 2, 1, 1, 2, 3, 3 },
	{ 0, 2, 3, 2, 2, 1, 2, 3, 2 },
	{ 0, 2, 3, 2, 2, 1, 2, 3, 4 },
	{ 0, 3, 4, 2, 1, 1, 1, 2, 4 },
	{ 0, 2, 3, 2, 1, 1, 2, 2, 3 },
	{ 0, 3, 4, 3, 1, 1, 1, 3, 1 },
	{ 0, 2, 3, 2, 1, 0, 1, 1, 4 },
	{ 0, 2, 2, 2, 0, 0, 1, 1, 3 },
	{ 0, 2, 2, 1, 0, 0, 0, 1, 0 },
	{ 0, 1, 1, 1, 0, -1, 0, 1, 0 },
	{ 0, 1, 1, 1, 0, -1, 0, 1, 0 },
	{ 0, 1, 1, 0, 0, -1, 0, 0, 4 },
	{ 0, 1, 1, 0, 0, -1, -1, 0, 0 },
	{ 0, 1, 1, 0, 0, 0, 0, 0, 0 },
	{ 0, 1, 1, 0, -1, -1, -1, 0, 1 },
	{ 0, 2, 1, 0, -1, 0, 0, 0, 2 },
	{ 0, 0, 1, -2, -1, -1, -1, -1, 3 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

static int gradation_offset_T_wqhd_revC[][9] = {
	{0, 8, 14, 23, 32, 37, 40, 35, 37},
	{0, 7, 12, 19, 25, 28, 32, 35, 37},
	{0, 6, 10, 16, 21, 25, 28, 31, 32},
	{0, 6, 8, 14, 17, 21, 23, 27, 28},
	{0, 5, 7, 12, 15, 19, 21, 25, 25},
	{0, 5, 6, 11, 14, 16, 19, 23, 21},
	{0, 5, 6, 10, 12, 15, 17, 22, 20},
	{0, 4, 5, 9, 12, 14, 16, 20, 22},
	{0, 4, 5, 9, 11, 13, 16, 19, 20},
	{0, 3, 4, 8, 10, 11, 14, 18, 18},
	{0, 3, 4, 7, 9, 11, 13, 17, 17},
	{0, 3, 4, 7, 9, 10, 13, 16, 16},
	{0, 3, 4, 7, 8, 10, 12, 15, 18},
	{0, 3, 3, 6, 8, 9, 11, 15, 14},
	{0, 3, 3, 6, 7, 9, 11, 15, 13},
	{0, 3, 3, 6, 7, 8, 10, 14, 14},
	{0, 3, 3, 5, 6, 8, 9, 13, 15},
	{0, 3, 3, 5, 6, 8, 9, 13, 12},
	{0, 3, 3, 5, 6, 7, 9, 13, 11},
	{0, 3, 2, 5, 5, 7, 8, 12, 15},
	{0, 3, 2, 5, 5, 7, 8, 12, 9},
	{0, 3, 2, 5, 5, 6, 7, 11, 14},
	{0, 3, 2, 5, 5, 6, 7, 11, 9},
	{0, 3, 2, 5, 4, 6, 7, 10, 13},
	{0, 3, 2, 4, 4, 5, 6, 10, 11},
	{0, 3, 2, 4, 4, 5, 6, 10, 9},
	{0, 3, 2, 4, 3, 5, 6, 9, 11},
	{0, 3, 2, 3, 3, 4, 5, 9, 7},
	{0, 2, 2, 3, 3, 4, 5, 8, 10},
	{0, 2, 2, 3, 3, 4, 5, 8, 8},
	{0, 2, 2, 3, 3, 4, 5, 7, 10},
	{0, 2, 2, 3, 2, 3, 4, 7, 8},
	{0, 2, 1, 3, 2, 3, 4, 7, 6},
	{0, 2, 1, 3, 2, 3, 4, 6, 9},
	{0, 2, 1, 2, 2, 3, 4, 6, 6},
	{0, 2, 1, 2, 2, 2, 3, 5, 9},
	{0, 2, 1, 2, 2, 2, 3, 5, 7},
	{0, 2, 1, 2, 2, 2, 3, 5, 5},
	{0, 2, 1, 2, 1, 2, 3, 5, 4},
	{0, 2, 1, 2, 1, 2, 3, 4, 6},
	{0, 3, 1, 2, 1, 1, 3, 4, 7},
	{0, 2, 2, 2, 1, 1, 3, 3, 5},
	{0, 2, 3, 2, 1, 1, 2, 3, 6},
	{0, 1, 2, 2, 0, 1, 3, 3, 6},
	{0, 1, 2, 2, 1, 2, 2, 4, 4},
	{0, 1, 2, 2, 1, 1, 2, 3, 6},
	{0, 1, 3, 3, 1, 1, 2, 3, 3},
	{0, 1, 3, 2, 1, 1, 2, 3, 4},
	{0, 2, 3, 2, 0, 1, 1, 3, 3},
	{0, 2, 2, 2, 1, 1, 2, 3, 3},
	{0, 2, 3, 2, 1, 1, 1, 2, 4},
	{0, 2, 4, 2, 1, 1, 2, 2, 5},
	{0, 2, 4, 2, 1, 1, 1, 3, 1},
	{0, 3, 4, 3, 1, 0, 1, 2, 3},
	{0, 2, 3, 2, 1, 0, 1, 2, 0},
	{0, 2, 2, 2, 0, 0, 1, 1, 3},
	{0, 2, 2, 1, 0, 0, 0, 1, 1},
	{0, 1, 1, 1, 0, -1, 0, 1, 0},
	{0, 1, 1, 1, 0, -1, 0, 1, 0},
	{0, 2, 2, 1, 0, -1, 0, 1, 1},
	{0, 0, 1, -1, 0, -1, 0, 0, 2},
	{0, 1, 1, -1, -1, -1, -1, 0, 2},
	{0, 2, 2, 0, 0, 0, 0, 0, 3},
	{0, 1, 1, 0, 0, -1, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static int gradation_offset_T_wqhd_revF[][9] = {
/*	V255 V203 V151 V87 V51 V35 V23 V11 V3 */
	{0, 8, 13, 21, 27, 29, 32, 30, 23},
	{0, 8, 12, 16, 20, 24, 27, 30, 23},
	{0, 7, 10, 15, 18, 21, 23, 27, 25},
	{0, 6, 9, 13, 15, 17, 19, 23, 22},
	{0, 6, 8, 12, 14, 16, 18, 22, 23},
	{0, 5, 8, 11, 13, 15, 17, 21, 20},
	{0, 5, 7, 10, 11, 13, 16, 19, 21},
	{0, 5, 7, 9, 11, 13, 16, 19, 19},
	{0, 5, 6, 9, 11, 13, 15, 18, 21},
	{0, 3, 5, 7, 10, 12, 14, 17, 21},
	{0, 3, 4, 7, 9, 11, 13, 17, 17},
	{0, 3, 4, 6, 8, 10, 13, 16, 17},
	{0, 3, 4, 6, 8, 10, 12, 16, 15},
	{0, 4, 5, 6, 8, 10, 12, 15, 16},
	{0, 4, 4, 6, 7, 9, 11, 14, 17},
	{0, 3, 3, 5, 7, 9, 10, 14, 15},
	{0, 3, 3, 5, 6, 8, 9, 13, 15},
	{0, 3, 4, 5, 6, 8, 9, 13, 14},
	{0, 3, 3, 4, 6, 8, 9, 13, 13},
	{0, 3, 3, 4, 6, 7, 8, 12, 13},
	{0, 3, 3, 4, 5, 7, 8, 12, 12},
	{0, 3, 3, 4, 5, 7, 8, 11, 12},
	{0, 3, 3, 4, 5, 6, 7, 11, 11},
	{0, 3, 3, 4, 4, 6, 7, 10, 11},
	{0, 3, 3, 4, 4, 6, 7, 10, 10},
	{0, 3, 3, 3, 4, 5, 6, 9, 10},
	{0, 3, 3, 3, 4, 5, 6, 9, 9},
	{0, 3, 2, 3, 3, 5, 5, 8, 9},
	{0, 3, 2, 2, 3, 4, 5, 8, 9},
	{0, 3, 2, 2, 3, 4, 5, 8, 7},
	{0, 3, 2, 2, 3, 4, 5, 7, 9},
	{0, 3, 2, 2, 2, 3, 4, 7, 7},
	{0, 3, 2, 2, 2, 3, 4, 7, 6},
	{0, 3, 2, 2, 2, 3, 4, 6, 8},
	{0, 2, 2, 2, 2, 3, 4, 6, 6},
	{0, 2, 2, 2, 2, 3, 3, 5, 7},
	{0, 2, 2, 1, 2, 2, 3, 5, 6},
	{0, 2, 1, 1, 2, 2, 3, 5, 4},
	{0, 2, 1, 1, 1, 2, 3, 4, 6},
	{0, 2, 1, 1, 1, 2, 2, 4, 4},
	{0, 2, 2, 2, 1, 1, 3, 4, 4},
	{0, 2, 1, 1, 1, 2, 3, 4, 3},
	{0, 2, 3, 2, 1, 2, 2, 3, 5},
	{0, 1, 2, 2, 1, 1, 3, 3, 6},
	{0, 1, 2, 2, 1, 2, 2, 3, 6},
	{0, 1, 3, 2, 1, 1, 2, 3, 6},
	{0, 0, 3, 2, 1, 1, 2, 3, 2},
	{0, 1, 3, 2, 1, 1, 2, 3, 4},
	{0, 0, 2, 1, 1, 1, 1, 3, 3},
	{0, 1, 3, 2, 1, 1, 2, 3, 3},
	{0, 1, 3, 2, 1, 1, 1, 2, 5},
	{0, 1, 3, 2, 1, 1, 2, 2, 4},
	{0, 2, 2, 2, 1, 1, 1, 2, 5},
	{0, 2, 2, 1, 1, 1, 0, 1, 3},
	{0, 2, 2, 1, 1, 1, 0, 1, 2},
	{0, 1, 2, 1, 1, 0, 0, 1, 0},
	{0, 1, 2, 1, 1, 0, 0, 1, 0},
	{0, 1, 2, 1, 0, 1, 0, 1, 0},
	{0, 1, 1, 1, 0, 0, 0, 0, 4},
	{0, 1, 1, 0, 0, 0, 0, 1, 0},
	{0, 0, 0, 0, -1, 0, 0, 0, 0},
	{0, 0, 1, 0, -1, -1, 0, 0, 1},
	{0, 2, 0, -1, -1, -1, 0, 0, 1},
	{0, 0, 0, -1, 0, -1, -1, 0, 3},
	{0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static int gradation_offset_T_wqhd_revH[][9] = {
/*	V255 V203 V151 V87 V51 V35 V23 V11 V3 */
	{0, 9, 17, 22, 27, 33, 35, 27, 4},
	{0, 8, 12, 17, 17, 25, 29, 29, 4},
	{0, 7, 12, 16, 19, 21, 24, 26, 23},
	{0, 5, 9, 13, 15, 17, 20, 22, 21},
	{0, 6, 10, 12, 14, 16, 19, 21, 21},
	{0, 5, 9, 11, 13, 14, 18, 20, 21},
	{0, 4, 8, 10, 12, 13, 17, 19, 19},
	{0, 4, 7, 9, 12, 13, 16, 19, 18},
	{0, 4, 7, 9, 10, 12, 15, 18, 20},
	{0, 4, 7, 8, 10, 11, 15, 17, 16},
	{0, 4, 6, 8, 10, 11, 14, 16, 17},
	{0, 4, 6, 7, 10, 11, 14, 16, 17},
	{0, 4, 5, 6, 9, 10, 13, 15, 17},
	{0, 4, 6, 7, 9, 10, 12, 15, 15},
	{0, 4, 5, 6, 8, 9, 12, 14, 16},
	{0, 4, 5, 6, 8, 9, 11, 14, 13},
	{0, 4, 5, 6, 7, 8, 10, 13, 14},
	{0, 4, 5, 5, 7, 8, 10, 13, 11},
	{0, 4, 5, 5, 7, 7, 10, 13, 10},
	{0, 4, 5, 5, 6, 7, 9, 12, 11},
	{0, 4, 4, 5, 6, 6, 8, 11, 12},
	{0, 4, 5, 5, 6, 6, 8, 11, 11},
	{0, 4, 4, 4, 5, 6, 8, 10, 12},
	{0, 4, 4, 4, 5, 6, 8, 10, 9},
	{0, 3, 5, 4, 5, 5, 8, 10, 8},
	{0, 4, 4, 4, 5, 5, 7, 9, 10},
	{0, 4, 4, 4, 4, 5, 7, 9, 7},
	{0, 4, 4, 3, 4, 4, 6, 8, 8},
	{0, 3, 4, 4, 4, 5, 6, 8, 8},
	{0, 4, 4, 3, 4, 4, 6, 7, 10},
	{0, 3, 4, 3, 4, 4, 6, 7, 8},
	{0, 3, 4, 3, 3, 4, 6, 7, 5},
	{0, 3, 4, 3, 4, 3, 5, 6, 8},
	{0, 3, 4, 3, 3, 3, 5, 6, 6},
	{0, 3, 4, 3, 3, 3, 5, 5, 8},
	{0, 3, 4, 3, 3, 3, 5, 5, 6},
	{0, 3, 4, 2, 3, 2, 4, 5, 4},
	{0, 3, 3, 2, 3, 2, 4, 4, 6},
	{0, 3, 3, 2, 3, 2, 4, 4, 4},
	{0, 3, 3, 2, 3, 2, 3, 4, 2},
	{0, 2, 3, 2, 2, 3, 3, 4, 2},
	{0, 1, 2, 2, 2, 2, 3, 4, 1},
	{0, 2, 3, 3, 2, 2, 3, 4, 1},
	{0, 2, 4, 3, 2, 2, 3, 3, 5},
	{0, 1, 3, 2, 2, 2, 3, 3, 4},
	{0, 0, 2, 2, 1, 1, 3, 3, 2},
	{0, 0, 4, 3, 2, 2, 3, 3, 4},
	{0, -2, 3, 3, 2, 2, 3, 3, 6},
	{0, -1, 3, 2, 2, 1, 3, 3, 5},
	{0, 0, 2, 3, 2, 1, 2, 3, 0},
	{0, 0, 3, 3, 2, 2, 2, 3, 2},
	{0, 0, 3, 3, 2, 2, 2, 2, 0},
	{0, 0, 2, 2, 1, 2, 1, 2, 3},
	{0, 1, 3, 2, 1, 1, 1, 1, 2},
	{0, 0, 2, 2, 1, 1, 1, 1, 1},
	{0, -1, 1, 1, 1, 1, 0, 1, 0},
	{0, -1, 1, 1, 1, 1, 0, 1, 0},
	{0, -1, 1, 1, 0, 1, 0, 1, 2},
	{0, 0, 1, 1, 0, 0, 0, 0, 2},
	{0, 1, 1, 1, 0, 0, 1, 0, 3},
	{0, 1, 1, 1, 0, 0, 0, 0, 0},
	{0, 1, 1, 1, 0, 0, 0, 0, 0},
	{0, 1, 0, 1, 0, -1, 0, 0, 0},
	{0, 0, 0, 0, 0, -1, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0},
};


static int rgb_offset_T_wqhd_revA[][RGB_COMPENSATION] = {
/*	R255 G255 B255 R203 G203 B203 R151 G151 B151
	R87 G87 B87 R51 G51 B51 R35 G35 B35
	R23 G23 B23 R11 G11 B11
*/
	{-6, 0, -4, -4, 0, -3, -4, 0, -3, -14, 2, -6, -15, 5, -12, -6, 5, -10, -1, 5, -12, 0, 6, -12},
	{-4, 0, -3, -2, 0, -2, -4, 0, -3, -10, 1, -4, -16, 4, -10, -8, 4, -8, -1, 4, -12, 0, 6, -12},
	{-2, 0, -2, -3, 0, -2, -3, 0, -2, -7, 1, -3, -13, 4, -8, -10, 4, -9, -3, 4, -12, 0, 5, -12},
	{-2, 0, -2, -2, 0, -2, -3, 0, -2, -7, 1, -2, -13, 3, -8, -11, 4, -9, -4, 4, -10, 0, 6, -13},
	{-1, 0, -1, -2, 0, -2, -3, 0, -2, -7, 0, -2, -11, 2, -6, -11, 4, -9, -5, 4, -10, -1, 8, -16},
	{-1, 0, -1, -2, 0, -2, -1, 0, -1, -6, 1, -2, -10, 2, -5, -12, 4, -9, -6, 4, -12, 1, 8, -16},
	{0, 0, -1, -2, 0, -1, -2, 0, -2, -6, 0, -2, -8, 2, -5, -13, 4, -9, -6, 4, -10, -1, 9, -18},
	{0, 0, -1, -1, 0, -1, -2, 0, -1, -4, 0, -1, -9, 2, -5, -12, 3, -8, -6, 3, -10, -2, 8, -16},
	{0, 0, -1, -1, 0, -1, -2, 0, -1, -5, 0, -1, -7, 2, -5, -12, 3, -8, -6, 3, -10, -2, 10, -20},
	{0, 0, -1, -1, 0, -1, -2, 0, -1, -4, 0, 0, -7, 2, -4, -11, 3, -7, -7, 3, -12, -1, 9, -20},
	{0, 0, -1, -1, 0, -1, -2, 0, -1, -4, 0, 0, -7, 1, -4, -9, 3, -6, -8, 3, -11, -2, 8, -18},
	{0, 0, -1, -1, 0, -1, -1, 0, 0, -4, 0, -1, -6, 2, -4, -10, 3, -6, -9, 3, -11, 0, 10, -21},
	{0, 0, 0, -1, 0, -2, -1, 0, 0, -4, 0, -1, -5, 1, -4, -10, 3, -6, -8, 3, -10, -2, 9, -20},
	{0, 0, 0, -1, 0, -2, -1, 0, 0, -2, 0, 0, -6, 2, -4, -10, 2, -6, -8, 2, -11, -1, 10, -22},
	{0, 0, 0, -1, 0, -2, -1, 0, 0, -2, 0, 0, -5, 1, -3, -10, 2, -6, -9, 2, -11, 0, 9, -19},
	{0, 0, 0, 0, 0, -1, -2, 0, -2, -2, 0, 0, -5, 1, -3, -10, 2, -6, -9, 2, -11, -3, 8, -18},
	{0, 0, 0, 0, 0, -1, -2, 0, -2, -1, 0, 1, -3, 1, -2, -9, 2, -5, -9, 2, -12, -2, 8, -17},
	{0, 0, 0, 0, 0, -1, -2, 0, -2, -1, 0, 1, -3, 1, -2, -9, 2, -5, -9, 2, -12, -2, 7, -16},
	{0, 0, 0, 0, 0, -1, -1, 0, -1, -2, 0, 0, -3, 1, -2, -8, 2, -4, -7, 2, -10, -3, 10, -22},
	{0, 0, 0, 0, 0, -1, -1, 0, -1, -2, 0, 0, -3, 0, -2, -8, 2, -4, -7, 2, -10, -4, 10, -21},
	{0, 0, 0, 0, 0, -1, -1, 0, -1, -2, 0, 0, -2, 0, -1, -8, 2, -4, -7, 2, -11, -2, 11, -22},
	{0, 0, 0, 0, 0, -1, -1, 0, -2, -2, 0, 1, -2, 0, -1, -8, 1, -4, -7, 1, -11, -4, 10, -22},
	{0, 0, 0, 0, 0, 0, 0, 0, -2, -3, 0, 0, -2, 1, -2, -8, 2, -4, -5, 2, -9, -3, 12, -26},
	{0, 0, 0, 0, 0, 0, 0, 0, -2, -3, 0, 0, -2, 0, -2, -7, 2, -4, -5, 2, -8, -4, 11, -22},
	{0, 0, 0, 0, 0, 0, 0, 0, -2, -3, 0, 0, -1, 0, -2, -8, 1, -4, -5, 1, -8, -5, 10, -22},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -2, 0, 0, -1, 0, -2, -8, 1, -4, -4, 1, -10, -5, 12, -24},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -2, 0, 0, -1, 0, -2, -7, 2, -4, -3, 2, -8, -5, 10, -22},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -2, 0, 0, -2, 0, -2, -6, 1, -4, -3, 1, -8, -6, 10, -20},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -2, 0, 0, 0, 0, -1, -6, 1, -3, -2, 1, -10, -6, 10, -22},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -2, 0, 0, 0, 0, -1, -6, 1, -3, -2, 1, -10, -6, 10, -20},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -2, 0, 0, 0, 0, -1, -4, 1, -2, -1, 1, -9, -7, 12, -25},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -2, 0, 0, 0, 0, 0, -5, 1, -3, -1, 1, -9, -8, 11, -23},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, -5, 1, -3, -1, 1, -8, -9, 10, -22},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, -4, 1, -2, -2, 1, -10, -5, 8, -16},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, -4, 1, -3, 0, 1, -9, -6, 10, -20},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, -3, 1, -2, 0, 1, -8, -4, 9, -18},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, -3, 1, -2, 1, 1, -8, -6, 10, -22},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, -2, 1, -2, 2, 1, -9, -4, 9, -18},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, -2, 1, -2, 3, 1, -9, -5, 8, -16},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, -8, -4, 8, -18},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, -2, 0, -1, 0, 0, -8, -4, 8, -18},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, -2, 2, 0, -7, -5, 8, -16},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, -2, 0, -1, 1, 0, -7, -4, 7, -16},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, -6, -5, 8, -18},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, -6, -4, 8, -17},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -2, 0, -1, 0, 0, -6, -4, 7, -16},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, -5, -1, 6, -14},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, -5, -2, 6, -14},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1, 0, 0, -4, -1, 6, -13},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1, -2, 0, 0, 0, 0, -4, -2, 6, -14},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, -4, -1, 6, -13},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, -2, 0, 6, -13},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, -3, 0, 5, -12},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, -2, 0, 5, -11},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, -2, 0, 5, -11},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 2, 0, -2, 3, 4, -9},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 1, 0, -2, 4, 4, -9},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 1, 0, -2, 4, 4, -9},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static int rgb_offset_T_wqhd_revC[][RGB_COMPENSATION] = {
/*	R255 G255 B255 R203 G203 B203 R151 G151 B151
	R87 G87 B87 R51 G51 B51 R35 G35 B35
	R23 G23 B23 R11 G11 B11
*/
	{-7, 1, -5, -3, 0, -2, -5, 0, -4, -10, 3, -6, -9, 5, -12, -5, 3, -8, 0, 3, -8, 4, 5, -10},
	{-5, 0, -4, -3, 0, -2, -5, 0, -4, -8, 2, -4, -10, 5, -11, -7, 4, -9, 0, 4, -8, 4, 5, -10},
	{-4, 0, -3, -2, 0, -2, -4, 0, -3, -8, 1, -4, -10, 4, -9, -10, 4, -10, -2, 4, -10, 2, 6, -12},
	{-3, 0, -3, -2, 0, -1, -3, 0, -2, -7, 1, -4, -11, 4, -8, -8, 4, -9, -3, 4, -12, 0, 6, -13},
	{-3, 0, -3, -1, 0, 0, -3, 0, -3, -6, 1, -3, -10, 3, -8, -8, 4, -8, -4, 4, -12, 1, 7, -14},
	{-2, 0, -2, -2, 0, -1, -2, 0, -2, -6, 1, -2, -8, 3, -6, -9, 4, -9, -5, 4, -9, 0, 8, -16},
	{-2, 0, -2, -2, 0, -1, -1, 0, -1, -5, 1, -2, -8, 3, -6, -9, 3, -8, -6, 3, -12, 0, 6, -14},
	{-2, 0, -2, -1, 0, 0, -2, 0, -2, -6, 0, -2, -6, 2, -6, -10, 4, -9, -6, 4, -11, 1, 7, -16},
	{-2, 0, -2, -1, 0, 0, -1, 0, -1, -5, 0, -2, -8, 2, -6, -9, 3, -8, -7, 3, -10, -1, 9, -18},
	{0, 0, -1, -3, 0, -1, -1, 0, -1, -4, 1, -2, -6, 2, -5, -10, 3, -8, -6, 3, -10, 0, 7, -16},
	{0, 0, -1, -3, 0, -1, -1, 0, -1, -3, 0, -1, -6, 2, -5, -10, 3, -8, -7, 3, -11, 0, 7, -16},
	{0, 0, -1, -3, 0, -1, -1, 0, -1, -3, 0, -1, -5, 2, -4, -10, 3, -8, -6, 3, -8, -2, 9, -18},
	{0, 0, -1, -3, 0, -1, -1, 0, -1, -2, 0, 0, -5, 2, -5, -8, 3, -6, -8, 3, -12, -1, 10, -20},
	{0, 0, 0, -2, 0, -1, -1, 0, -1, -3, 0, 0, -5, 2, -4, -8, 3, -6, -8, 3, -11, -1, 8, -16},
	{0, 0, 0, -2, 0, -1, -1, 0, -1, -3, 0, 0, -6, 2, -5, -8, 2, -6, -8, 2, -10, -3, 7, -16},
	{0, 0, 0, 0, 0, -1, -3, 0, -1, -3, 0, 0, -5, 2, -4, -8, 2, -6, -7, 2, -10, -2, 9, -18},
	{0, 0, 0, 0, 0, -1, -3, 0, -1, -2, 0, 0, -5, 2, -4, -7, 2, -5, -8, 2, -11, 0, 9, -20},
	{0, 0, 0, 0, 0, -1, -3, 0, -1, -2, 0, 0, -5, 1, -4, -7, 2, -5, -8, 2, -10, -1, 9, -18},
	{0, 0, 0, 0, 0, -1, -3, 0, -1, -2, 0, 0, -5, 1, -4, -8, 2, -6, -7, 2, -9, -2, 9, -18},
	{0, 0, 0, 0, 0, -1, -2, 0, -1, -2, 0, 0, -4, 1, -4, -8, 2, -5, -7, 2, -10, -1, 10, -22},
	{0, 0, 0, 0, 0, -1, -2, 0, -1, -2, 0, 0, -3, 1, -3, -7, 2, -4, -5, 2, -8, -3, 8, -18},
	{0, 0, 0, 0, 0, -1, -2, 0, -1, -2, 0, 0, -3, 1, -3, -7, 2, -5, -6, 2, -11, -2, 10, -22},
	{0, 0, 0, 0, 0, -1, -2, 0, -1, -2, 0, 0, -2, 1, -2, -6, 2, -4, -6, 2, -10, -2, 8, -18},
	{0, 0, 0, 0, 0, 0, -2, 0, -2, -2, 0, 0, -2, 1, -3, -6, 1, -4, -5, 1, -9, -2, 11, -24},
	{0, 0, 0, 0, 0, 0, -2, 0, -2, -1, 0, 1, -2, 1, -2, -5, 1, -4, -5, 1, -10, -4, 9, -20},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -3, 0, 0, -2, 0, -2, -5, 1, -4, -5, 1, -10, -5, 8, -18},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -3, 0, 0, -1, 0, -2, -6, 1, -4, -5, 1, -9, -2, 11, -22},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -3, 0, 0, -1, 1, -2, -5, 1, -4, -4, 1, -10, -3, 8, -17},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -3, 0, 0, -1, 0, -2, -5, 2, -4, -3, 2, -9, -3, 10, -22},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -3, 0, 0, -1, 0, -2, -5, 1, -4, -3, 1, -9, -3, 10, -20},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -3, 0, 0, -1, 0, -2, -4, 1, -3, -2, 1, -8, -4, 12, -24},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -3, 0, 0, -1, 0, -1, -5, 1, -3, -2, 1, -8, -3, 10, -20},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -3, 0, 0, -1, 0, -1, -4, 1, -2, -2, 1, -8, -3, 9, -19},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -3, 0, 0, -1, 0, -1, -4, 0, -2, -2, 0, -8, -3, 11, -23},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, -1, -4, 1, -2, -1, 1, -7, -2, 10, -20},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, -1, -4, 1, -2, 0, 1, -9, -2, 11, -22},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, -1, -4, 0, -2, 1, 0, -8, -2, 10, -21},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, 0, -2, 1, -2, 0, 1, -8, -2, 9, -19},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, -1, -3, 0, -2, 0, 0, -8, -2, 8, -18},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, -1, -3, 0, -2, 1, 0, -8, -2, 9, -20},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1, 0, 0, 0, -4, 0, -2, 1, 0, -8, -1, 9, -20},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, 0, -1, 0, 0, 2, 0, -7, -2, 8, -18},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, -2, 0, -1, 1, 0, -7, -1, 9, -18},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1, 0, 0, 0, -2, 0, -1, 2, 0, -6, 0, 9, -18},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 1, -1, 0, 0, 1, 0, -6, -2, 7, -16},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -2, 0, 0, 2, 0, -5, 0, 8, -17},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, -5, 0, 7, -16},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, -5, 0, 7, -15},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, -4, 0, 7, -14},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, -3, 0, 7, -15},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, -3, 0, 7, -15},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1, 0, 0, -2, 0, 7, -14},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1, -1, 0, 0, -1, 0, -3, 2, 6, -12},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1, 0, 0, -2, 0, 5, -12},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, -1, 0, 1, 0, 0, -2, 2, 5, -11},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2, 0, 0, 0, 0, 0, -2, 0, 5, -12},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 1, 0, -2, 2, 5, -10},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 1, 1, 0, -2, 2, 4, -10},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static int rgb_offset_T_wqhd_revF[][RGB_COMPENSATION] = {
/*	R255 G255 B255 R203 G203 B203 R151 G151 B151
	R87 G87 B87 R51 G51 B51 R35 G35 B35
	R23 G23 B23 R11 G11 B11
*/
	{-5, 1, -4, -3, 0, -1, -5, 0, -3, -13, 1, -4, -17, 4, -8, -10, 3, -6, -6, 3, -8, 5, 1, -4},
	{-4, 0, -4, -2, 0, -1, -4, 0, -2, -11, 1, -4, -18, 3, -7, -13, 3, -7, -6, 3, -8, 5, 1, -4},
	{-3, 0, -3, -2, 0, -2, -3, 0, -2, -10, 1, -4, -17, 1, -4, -13, 3, -7, -9, 3, -9, 2, 2, -5},
	{-2, 0, -2, -1, 0, -2, -4, 0, -2, -7, 1, -3, -14, 1, -2, -17, 3, -7, -8, 3, -8, 1, 5, -10},
	{-1, 0, -2, -1, 0, -1, -4, 0, -2, -6, 1, -4, -14, 1, -3, -15, 2, -6, -9, 2, -10, 3, 4, -9},
	{0, 0, -1, -2, 0, -2, -3, 0, -1, -5, 1, -3, -11, 1, -3, -14, 2, -6, -9, 2, -10, 0, 5, -10},
	{0, 0, -1, -1, 0, -1, -3, 0, -2, -5, 1, -3, -11, 1, -4, -14, 2, -6, -9, 2, -9, 0, 7, -15},
	{0, 0, -1, -1, 0, -1, -2, 0, -1, -6, 1, -4, -9, 1, -4, -12, 3, -6, -8, 3, -8, 0, 8, -16},
	{0, 0, 0, -1, 0, -2, -2, 0, -1, -4, 1, -3, -9, 1, -4, -11, 2, -6, -9, 2, -10, 1, 8, -16},
	{0, 0, -1, -1, 0, 0, -3, 0, -2, -4, 1, -3, -9, 1, -4, -11, 2, -6, -8, 2, -10, 0, 9, -18},
	{0, 0, -1, -1, 0, 0, -2, 0, -1, -4, 1, -3, -7, 1, -4, -10, 2, -6, -8, 2, -10, -1, 7, -16},
	{0, 0, -1, -1, 0, 0, -2, 0, -1, -3, 1, -2, -7, 1, -4, -9, 2, -6, -7, 2, -8, -1, 9, -18},
	{0, 0, -1, -1, 0, 0, -2, 0, -1, -2, 1, -2, -7, 1, -4, -8, 2, -5, -6, 2, -10, -1, 7, -16},
	{0, 0, -1, -1, 0, 0, -2, 0, -1, -1, 1, -2, -7, 1, -4, -7, 1, -4, -7, 1, -10, -1, 9, -20},
	{0, 0, -1, 0, 0, 0, -2, 0, 0, -2, 1, -2, -7, 1, -3, -7, 2, -4, -7, 2, -8, 1, 11, -22},
	{0, 0, -1, 0, 0, 0, -1, 0, 0, -2, 1, -2, -8, 1, -4, -7, 1, -4, -8, 1, -9, 0, 10, -20},
	{0, 0, 0, 0, 0, -1, -1, 0, 0, -2, 1, -2, -6, 1, -3, -7, 2, -4, -9, 2, -10, 1, 10, -21},
	{0, 0, 0, 0, 0, -1, -2, 0, 0, -2, 0, -2, -7, 1, -4, -7, 1, -4, -9, 1, -9, 0, 10, -21},
	{0, 0, 0, 0, 0, -1, -1, 0, 0, -2, 0, -2, -6, 1, -2, -7, 1, -4, -8, 1, -8, -1, 8, -18},
	{0, 0, 0, 0, 0, -1, -1, 0, 0, -2, 0, -2, -5, 1, -2, -6, 2, -4, -8, 2, -8, 0, 11, -22},
	{0, 0, 0, 0, 0, -1, -1, 0, 0, -2, 0, -2, -5, 1, -2, -6, 1, -4, -7, 1, -6, -2, 9, -20},
	{0, 0, 0, 0, 0, -1, -1, 0, 0, -2, 0, -2, -4, 1, -2, -6, 1, -3, -7, 1, -6, -1, 13, -26},
	{0, 0, 0, 0, 0, -1, -1, 0, 0, -1, 0, -1, -4, 1, -2, -6, 1, -4, -8, 1, -8, -2, 10, -22},
	{0, 0, 0, 0, 0, -1, -1, 0, 0, -1, 0, -1, -4, 1, -2, -5, 1, -2, -7, 1, -7, -1, 12, -24},
	{0, 0, 0, 0, 0, -1, -1, 0, 0, -1, 0, -1, -4, 0, -2, -6, 1, -3, -6, 1, -6, -2, 11, -24},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -2, -4, 0, -2, -5, 1, -3, -6, 1, -7, -1, 12, -26},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, -1, -4, 0, -2, -3, 1, -2, -6, 1, -6, -1, 11, -22},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, -4, 0, -2, -4, 0, -2, -6, 0, -8, -1, 12, -26},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, -4, 0, -2, -4, 1, -2, -5, 1, -8, -2, 11, -24},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, -3, 0, -1, -4, 1, -2, -3, 1, -7, -3, 10, -22},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, -3, 0, -1, -3, 0, -2, -4, 0, -7, -3, 12, -26},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, -2, 0, -1, -4, 1, -2, -3, 1, -8, -3, 10, -21},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, -2, 0, -1, -4, 0, -2, -3, 0, -8, -4, 9, -20},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, -2, 0, -1, -2, 0, -1, -4, 0, -8, -3, 11, -24},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, -2, 0, -1, -2, 0, -1, -2, 0, -7, -5, 10, -22},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, -1, -2, 0, -1, -3, 0, -8, -3, 12, -24},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -2, 0, 0, -2, 0, -1, -1, 0, -8, -4, 11, -22},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -2, 0, 0, -1, 0, -1, -1, 0, -7, -4, 10, -20},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, -1, 0, -1, -1, 0, -7, -4, 11, -23},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, -8, -5, 9, -20},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0, -6, -4, 10, -20},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, -1, 0, 0, -6, -4, 9, -18},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -6, -4, 9, -19},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, -5, -5, 9, -20},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, -4, -5, 9, -20},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, 0, -1, 0, -5, -5, 8, -18},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, -1, 0, -4, -2, 7, -14},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, -1, 0, -5, -4, 7, -15},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 1, 0, 0, -4, -3, 6, -14},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, -3, -3, 7, -14},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, -3, -4, 6, -14},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, -1, 0, 0, 0, 0, -2, -4, 7, -14},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1, 0, 0, 1, 0, 0, -1, -3, 6, -14},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 0, -2, -1, 6, -12},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, -2, -2, 6, -12},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, -2, -1, 5, -10},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, -2, -1, 4, -10},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, 0, 2, 0, -1, -1, 4, -10},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static int rgb_offset_T_wqhd_revH[][RGB_COMPENSATION] = {
/*	R255 G255 B255 R203 G203 B203 R151 G151 B151
	R87 G87 B87 R51 G51 B51 R35 G35 B35
	R23 G23 B23 R11 G11 B11
*/
	{-3, 4, 0, -1, 1, 0, -5, 0, -3, -14, -1, -6, -22, 0, -14, -7, -1, -5, 2, 2, -8, 21, -2, 1},
	{-3, 3, -1, -2, -1, -1, -4, 1, -1, -10, 1, -4, -26, -1, -15, -4, -1, -3, -3, 1, -8, 21, -2, 1},
	{0, 4, 1, -2, -1, -1, -3, 1, -1, -9, 1, -5, -18, -1, -8, -10, 4, -4, -7, 2, -10, 10, -2, -6},
	{0, 3, 1, -1, 1, 0, -3, 1, -1, -7, 1, -4, -13, 1, -3, -12, 1, -6, -8, 2, -8, 10, 0, -6},
	{-2, 1, -2, -1, -1, -1, -3, 1, -1, -6, 1, -4, -11, 2, -2, -12, 2, -6, -7, 2, -8, 10, 2, -7},
	{-1, 1, -1, 0, 0, 0, -3, 1, -1, -6, 1, -4, -9, 2, -3, -11, 2, -6, -6, 2, -8, 4, 4, -9},
	{0, 0, -1, -2, 1, 0, -2, 1, 0, -5, 2, -3, -7, 2, -3, -12, 2, -7, -7, 1, -9, 4, 5, -11},
	{0, 0, -1, -1, 1, 0, -2, 1, 0, -5, 2, -2, -7, 3, -4, -10, 1, -7, -7, 4, -9, 0, 1, -14},
	{0, 0, -1, 0, 1, 1, -2, 1, 0, -5, 2, -2, -7, 3, -4, -9, 1, -7, -7, 3, -9, 0, 2, -14},
	{0, 0, -1, 0, 1, 1, -2, 1, 0, -3, 2, -2, -7, 2, -4, -9, 1, -7, -7, 2, -9, 1, 5, -14},
	{0, 0, -1, 0, 1, 1, -1, 1, 0, -3, 2, -2, -7, 2, -4, -7, 1, -6, -6, 3, -9, 2, 5, -13},
	{0, 0, -1, -1, 0, 0, -1, 1, 0, -2, 2, -2, -7, 2, -4, -7, 1, -6, -7, 2, -11, 1, 6, -14},
	{0, 0, 0, 0, 0, 0, -1, 1, 0, -2, 2, -2, -7, 1, -4, -5, 1, -4, -6, 5, -11, 5, 8, -13},
	{0, 0, 0, 0, 0, 0, -1, 1, 0, -2, 2, -2, -6, 1, -4, -5, 1, -4, -6, 5, -11, 0, 3, -14},
	{0, 0, 0, 0, 0, 0, -1, 1, 0, -2, 2, -2, -6, 1, -4, -5, 1, -4, -8, 2, -11, 6, 10, -13},
	{0, 0, 0, 0, 0, 0, -1, 0, -1, -2, 2, -2, -6, 1, -3, -5, 1, -4, -7, 3, -11, 3, 6, -13},
	{0, 0, 0, 0, 0, 0, -1, 0, -1, -2, 2, -2, -6, 1, -3, -5, 1, -5, -7, 3, -11, 8, 9, -13},
	{0, 0, 0, 0, 0, 0, -1, 0, -1, -2, 2, -1, -6, 1, -3, -6, 0, -6, -7, 4, -10, 6, 5, -16},
	{0, 0, 0, 0, 0, 0, -1, 0, -2, -2, 2, 0, -5, 1, -3, -5, 1, -5, -7, 3, -8, 1, 3, -17},
	{0, 0, 0, 0, 0, 0, -1, 0, -2, -2, 2, 0, -5, 1, -2, -5, 2, -5, -5, 3, -8, 5, 7, -15},
	{0, 0, 0, 0, 0, 0, -1, 0, -1, -2, 2, 0, -3, 1, -2, -5, 2, -4, -3, 5, -10, 6, 8, -15},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -2, 2, 0, -3, 1, -2, -5, 2, -4, -5, 4, -10, 5, 7, -15},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -2, 2, 0, -3, 1, -2, -5, 1, -4, -5, 2, -10, 11, 12, -15},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -2, 2, 0, -3, 0, -2, -5, 1, -5, -5, 2, -9, 8, 10, -16},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -2, 1, 0, -3, 2, -1, -2, 1, -4, -7, -1, -9, 7, 9, -16},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -2, 1, 0, -3, 2, 0, -3, 0, -5, -2, 2, -9, 11, 12, -14},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 1, 0, -2, 2, 1, -3, 0, -5, -4, 1, -9, 7, 8, -16},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 1, 0, -2, 2, 1, -3, 0, -5, 1, 5, -7, 7, 9, -16},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 1, 0, -2, 2, 1, -3, 0, -5, 0, 2, -8, 8, 9, -15},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, -2, 2, 1, -3, 0, -5, -1, 2, -9, 15, 16, -12},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, -2, 2, 1, -5, -2, -7, 0, 2, -8, 13, 14, -12},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, -2, 2, 1, -4, -2, -7, 1, 0, -9, 10, 12, -14},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, -2, 0, 1, -2, 1, -5, 3, 3, -8, 12, 14, -12},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, -2, 0, 0, -2, 1, -4, 3, 2, -8, 10, 12, -13},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, -2, 0, 0, -2, 1, -4, 3, 1, -8, 19, 18, -11},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, -2, 0, 0, -1, 0, -4, 2, 0, -9, 14, 15, -13},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, -2, 0, 0, 0, 2, -2, 7, 3, -8, 7, 8, -11},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, -2, 0, 0, 0, 1, -2, 7, 3, -8, 13, 12, -11},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, -2, 0, 0, 0, 1, -2, 6, 1, -9, 11, 11, -13},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, -2, 0, 0, 1, 2, -1, 11, 4, -8, 5, 3, -12},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -2, 0, 0, 1, 2, -1, 6, -1, -10, 6, 6, -15},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 1, 2, -1, 6, 0, -10, 5, 5, -12},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 1, 2, -1, 3, -4, -12, 4, 3, -10},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 1, 1, -1, 4, -2, -10, 13, 10, -7},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, -1, -3, -10, 17, 14, -3},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0, -3, -9, 16, 12, -4},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 4, 0, -6, 15, 11, -4},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 1, 1, 0, 0, -1, -1, -4, -9, 11, 8, -4},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 1, 1, 0, 0, -1, 0, -3, -7, 10, 6, -5},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 2, -1, -5, 10, 4, -5},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1, 0, 0, 0, -3, -4, -7, 15, 10, -1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -2, -5, 15, 9, -1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -5, 15, 9, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -4, 13, 9, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, -5, 13, 8, -1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 3, -1, 11, 1, -2},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 3, -1, 8, -3, -5},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 3, -1, 7, -4, -5},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},

};


static void gamma_init_T_revA(struct SMART_DIM *pSmart, char *str, int size)
{
	long long candela_level[S6E3_TABLE_MAX] = {-1, };
	int bl_index[S6E3_TABLE_MAX] = {-1, };
	int gamma_setting[GAMMA_SET_MAX];

	long long temp_cal_data = 0;
	int bl_level = 0;

	int level_255_temp_MSB = 0;
	int level_V255 = 0;

	int point_index;
	int cnt;
	int table_index;

	pr_debug("%s : start !!\n",__func__);

	/*calculate candela level */
	if ((pSmart->brightness_level <= 350) &&
		(pSmart->brightness_level >= 265)) {
		/* 265CD ~ 350CD */
		if (pSmart->brightness_level == 350)
			bl_level = 350;
		else if (pSmart->brightness_level == 333)
			bl_level = 337;
		else if (pSmart->brightness_level == 316)
			bl_level = 321;
		else if (pSmart->brightness_level == 300)
			bl_level = 303;
		else if (pSmart->brightness_level == 282)
			bl_level = 287;
		else if (pSmart->brightness_level == 265)
			bl_level = 271;
	} else if ((pSmart->brightness_level < 265) &&
				(pSmart->brightness_level >= 172)) {
		/* 172CD ~ 265CD */
		bl_level = 256;
	} else if ((pSmart->brightness_level < 172) &&
				(pSmart->brightness_level >= 77)) {
		/* 77CD ~ 162CD */
		if (pSmart->brightness_level == 77)
			bl_level = 122;
		else if (pSmart->brightness_level == 82)
			bl_level = 130;
		else if (pSmart->brightness_level == 87)
			bl_level = 137;
		else if (pSmart->brightness_level == 93)
			bl_level = 146;
		else if (pSmart->brightness_level == 98)
			bl_level = 153;
		else if (pSmart->brightness_level == 105)
			bl_level = 162;
		else if (pSmart->brightness_level == 111)
			bl_level = 171;
		else if (pSmart->brightness_level == 119)
			bl_level = 183;
		else if (pSmart->brightness_level == 126)
			bl_level = 192;
		else if (pSmart->brightness_level == 134)
			bl_level = 205;
		else if (pSmart->brightness_level == 143)
			bl_level = 216;
		else if (pSmart->brightness_level == 152)
			bl_level = 229;
		else if (pSmart->brightness_level == 162)
			bl_level = 241;
	} else {
		/* 72CD ~ 2CD */
		bl_level = AOR_DIM_BASE_CD_REVA;
	}

	if (pSmart->brightness_level < 350) {
		for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
			point_index = S6E3_ARRAY[cnt+1];
			temp_cal_data =
			((long long)(candela_coeff_2p15[point_index])) *
			((long long)(bl_level));
			candela_level[cnt] = temp_cal_data;
		}

	} else {
		for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
			point_index = S6E3_ARRAY[cnt+1];
			temp_cal_data =
			((long long)(candela_coeff_2p2[point_index])) *
			((long long)(bl_level));
			candela_level[cnt] = temp_cal_data;
		}

	}

#ifdef SMART_DIMMING_DEBUG
	printk(KERN_INFO "\n candela_1:%llu  candela_3:%llu  candela_11:%llu ",
		candela_level[0], candela_level[1], candela_level[2]);
	printk(KERN_INFO "candela_23:%llu  candela_35:%llu  candela_51:%llu ",
		candela_level[3], candela_level[4], candela_level[5]);
	printk(KERN_INFO "candela_87:%llu  candela_151:%llu  candela_203:%llu ",
		candela_level[6], candela_level[7], candela_level[8]);
	printk(KERN_INFO "candela_255:%llu brightness_level %d\n", candela_level[9], pSmart->brightness_level);
#endif

	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
		if (searching_function(candela_level[cnt],
			&(bl_index[cnt]), GAMMA_CURVE_2P2)) {
			pr_info("%s searching functioin error cnt:%d\n",
			__func__, cnt);
		}
	}

	/*
	*	Candela compensation
	*/
	for (cnt = 1; cnt < S6E3_TABLE_MAX; cnt++) {
		table_index = find_cadela_table(pSmart->brightness_level);

		if (table_index == -1) {
			table_index = CCG6_MAX_TABLE;
			pr_info("%s fail candela table_index cnt : %d brightness %d",
				__func__, cnt, pSmart->brightness_level);
		}

		bl_index[S6E3_TABLE_MAX - cnt] +=
			gradation_offset_T_wqhd_revA[table_index][cnt - 1];

		/* THERE IS M-GRAY0 target */
		if (bl_index[S6E3_TABLE_MAX - cnt] == 0)
			bl_index[S6E3_TABLE_MAX - cnt] = 1;
	}

#ifdef SMART_DIMMING_DEBUG
	printk(KERN_INFO "\n bl_index_1:%d  bl_index_3:%d  bl_index_11:%d",
		bl_index[0], bl_index[1], bl_index[2]);
	printk(KERN_INFO "bl_index_23:%d bl_index_35:%d  bl_index_51:%d",
		bl_index[3], bl_index[4], bl_index[5]);
	printk(KERN_INFO "bl_index_87:%d  bl_index_151:%d bl_index_203:%d",
		bl_index[6], bl_index[7], bl_index[8]);
	printk(KERN_INFO "bl_index_255:%d\n", bl_index[9]);
#endif
	/*Generate Gamma table*/
	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++)
		(void)Make_hexa[cnt](bl_index , pSmart, str);

	/* To avoid overflow */
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
		gamma_setting[cnt] = str[cnt];

	/*
	*	RGB compensation
	*/
	for (cnt = 0; cnt < RGB_COMPENSATION; cnt++) {
		table_index = find_cadela_table(pSmart->brightness_level);

		if (table_index == -1) {
			table_index = CCG6_MAX_TABLE;
			pr_info("%s fail RGB table_index cnt : %d brightness %d",
				__func__, cnt, pSmart->brightness_level);
		}

		if (cnt < 3) {
			level_V255 = gamma_setting[cnt * 2] << 8 | gamma_setting[(cnt * 2) + 1];
			level_V255 +=
				rgb_offset_T_wqhd_revA[table_index][cnt];
			level_255_temp_MSB = level_V255 / 256;

			gamma_setting[cnt * 2] = level_255_temp_MSB & 0xff;
			gamma_setting[(cnt * 2) + 1] = level_V255 & 0xff;
		} else {
			gamma_setting[cnt+3] += rgb_offset_T_wqhd_revA[table_index][cnt];
		}
	}
	/*subtration MTP_OFFSET value from generated gamma table*/
	mtp_offset_substraction(pSmart, gamma_setting);

	/* To avoid overflow */
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
		str[cnt] = gamma_setting[cnt];
}

static void gamma_init_T_revC(struct SMART_DIM *pSmart, char *str, int size)
{
	long long candela_level[S6E3_TABLE_MAX] = {-1, };
	int bl_index[S6E3_TABLE_MAX] = {-1, };
	int gamma_setting[GAMMA_SET_MAX];

	long long temp_cal_data = 0;
	int bl_level = 0;

	int level_255_temp_MSB = 0;
	int level_V255 = 0;

	int point_index;
	int cnt;
	int table_index;

	pr_debug("%s : start !!\n",__func__);

	/*calculate candela level */
	if ((pSmart->brightness_level <= 350) &&
		(pSmart->brightness_level >= 265)) {
		/* 265CD ~ 350CD */
		if (pSmart->brightness_level == 350)
			bl_level = 350;
		else if (pSmart->brightness_level == 333)
			bl_level = 334;
		else if (pSmart->brightness_level == 316)
			bl_level = 317;
		else if (pSmart->brightness_level == 300)
			bl_level = 304;
		else if (pSmart->brightness_level == 282)
			bl_level = 288;
		else if (pSmart->brightness_level == 265)
			bl_level = 270;
	} else if ((pSmart->brightness_level < 265) &&
				(pSmart->brightness_level >= 183)) {
		/* 172CD ~ 265CD */
		bl_level = 256;
	} else if ((pSmart->brightness_level < 183) &&
				(pSmart->brightness_level >= 82)) {
		/* 77CD ~ 162CD */
		if (pSmart->brightness_level == 82)
			bl_level = 123;
		else if (pSmart->brightness_level == 87)
			bl_level = 130;
		else if (pSmart->brightness_level == 93)
			bl_level = 139;
		else if (pSmart->brightness_level == 98)
			bl_level = 148;
		else if (pSmart->brightness_level == 105)
			bl_level = 157;
		else if (pSmart->brightness_level == 111)
			bl_level = 164;
		else if (pSmart->brightness_level == 119)
			bl_level = 177;
		else if (pSmart->brightness_level == 126)
			bl_level = 185;
		else if (pSmart->brightness_level == 134)
			bl_level = 195;
		else if (pSmart->brightness_level == 143)
			bl_level = 208;
		else if (pSmart->brightness_level == 152)
			bl_level = 220;
		else if (pSmart->brightness_level == 162)
			bl_level = 231;
		else if (pSmart->brightness_level == 172)
			bl_level = 246;
	} else {
		/* 77CD ~ 2CD */
		bl_level = AOR_DIM_BASE_CD_REVC;
	}

	if (pSmart->brightness_level < 350) {
		for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
			point_index = S6E3_ARRAY[cnt+1];
			temp_cal_data =
			((long long)(candela_coeff_2p15[point_index])) *
			((long long)(bl_level));
			candela_level[cnt] = temp_cal_data;
		}

	} else {
		for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
			point_index = S6E3_ARRAY[cnt+1];
			temp_cal_data =
			((long long)(candela_coeff_2p2[point_index])) *
			((long long)(bl_level));
			candela_level[cnt] = temp_cal_data;
		}

	}

#ifdef SMART_DIMMING_DEBUG
	printk(KERN_INFO "\n candela_1:%llu  candela_3:%llu  candela_11:%llu ",
		candela_level[0], candela_level[1], candela_level[2]);
	printk(KERN_INFO "candela_23:%llu  candela_35:%llu  candela_51:%llu ",
		candela_level[3], candela_level[4], candela_level[5]);
	printk(KERN_INFO "candela_87:%llu  candela_151:%llu  candela_203:%llu ",
		candela_level[6], candela_level[7], candela_level[8]);
	printk(KERN_INFO "candela_255:%llu brightness_level %d\n", candela_level[9], pSmart->brightness_level);
#endif

	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
		if (searching_function(candela_level[cnt],
			&(bl_index[cnt]), GAMMA_CURVE_2P2)) {
			pr_info("%s searching functioin error cnt:%d\n",
			__func__, cnt);
		}
	}

	/*
	*	Candela compensation
	*/
	for (cnt = 1; cnt < S6E3_TABLE_MAX; cnt++) {
		table_index = find_cadela_table(pSmart->brightness_level);

		if (table_index == -1) {
			table_index = CCG6_MAX_TABLE;
			pr_info("%s fail candela table_index cnt : %d brightness %d",
				__func__, cnt, pSmart->brightness_level);
		}

		bl_index[S6E3_TABLE_MAX - cnt] +=
			gradation_offset_T_wqhd_revC[table_index][cnt - 1];

		/* THERE IS M-GRAY0 target */
		if (bl_index[S6E3_TABLE_MAX - cnt] == 0)
			bl_index[S6E3_TABLE_MAX - cnt] = 1;
	}

#ifdef SMART_DIMMING_DEBUG
	printk(KERN_INFO "\n bl_index_1:%d  bl_index_3:%d  bl_index_11:%d",
		bl_index[0], bl_index[1], bl_index[2]);
	printk(KERN_INFO "bl_index_23:%d bl_index_35:%d  bl_index_51:%d",
		bl_index[3], bl_index[4], bl_index[5]);
	printk(KERN_INFO "bl_index_87:%d  bl_index_151:%d bl_index_203:%d",
		bl_index[6], bl_index[7], bl_index[8]);
	printk(KERN_INFO "bl_index_255:%d\n", bl_index[9]);
#endif
	/*Generate Gamma table*/
	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++)
		(void)Make_hexa[cnt](bl_index , pSmart, str);

	/* To avoid overflow */
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
		gamma_setting[cnt] = str[cnt];

	/*
	*	RGB compensation
	*/
	for (cnt = 0; cnt < RGB_COMPENSATION; cnt++) {
		table_index = find_cadela_table(pSmart->brightness_level);

		if (table_index == -1) {
			table_index = CCG6_MAX_TABLE;
			pr_info("%s fail RGB table_index cnt : %d brightness %d",
				__func__, cnt, pSmart->brightness_level);
		}

		if (cnt < 3) {
			level_V255 = gamma_setting[cnt * 2] << 8 | gamma_setting[(cnt * 2) + 1];
			level_V255 +=
				rgb_offset_T_wqhd_revC[table_index][cnt];
			level_255_temp_MSB = level_V255 / 256;

			gamma_setting[cnt * 2] = level_255_temp_MSB & 0xff;
			gamma_setting[(cnt * 2) + 1] = level_V255 & 0xff;
		} else {
			gamma_setting[cnt+3] += rgb_offset_T_wqhd_revC[table_index][cnt];
		}
	}
	/*subtration MTP_OFFSET value from generated gamma table*/
	mtp_offset_substraction(pSmart, gamma_setting);

	/* To avoid overflow */
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
		str[cnt] = gamma_setting[cnt];
}

static void gamma_init_T_revF(struct SMART_DIM *pSmart, char *str, int size)
{
	long long candela_level[S6E3_TABLE_MAX] = {-1, };
	int bl_index[S6E3_TABLE_MAX] = {-1, };
	int gamma_setting[GAMMA_SET_MAX];

	long long temp_cal_data = 0;
	int bl_level = 0;

	int level_255_temp_MSB = 0;
	int level_V255 = 0;

	int point_index;
	int cnt;
	int table_index;

	pr_debug("%s : start !!\n",__func__);

	/*calculate candela level */
	if ((pSmart->brightness_level <= 350) &&
		(pSmart->brightness_level >= 265)) {
		/* 265CD ~ 350CD */
		if (pSmart->brightness_level == 350)
			bl_level = 350;
		else if (pSmart->brightness_level == 333)
			bl_level = 339;
		else if (pSmart->brightness_level == 316)
			bl_level = 320;
		else if (pSmart->brightness_level == 300)
			bl_level = 303;
		else if (pSmart->brightness_level == 282)
			bl_level = 286;
		else if (pSmart->brightness_level == 265)
			bl_level = 270;
	} else if ((pSmart->brightness_level < 265) &&
				(pSmart->brightness_level >= 172)) {
		/* 172CD ~ 265CD */
		bl_level = 255;
	} else if ((pSmart->brightness_level < 172) &&
				(pSmart->brightness_level > 77)) {
		/* 82CD ~ 162CD */
		if (pSmart->brightness_level == 82)
			bl_level = 125;
		else if (pSmart->brightness_level == 87)
			bl_level = 132;
		else if (pSmart->brightness_level == 93)
			bl_level = 143;
		else if (pSmart->brightness_level == 98)
			bl_level = 152;
		else if (pSmart->brightness_level == 105)
			bl_level = 161;
		else if (pSmart->brightness_level == 111)
			bl_level = 169;
		else if (pSmart->brightness_level == 119)
			bl_level = 179;
		else if (pSmart->brightness_level == 126)
			bl_level = 192;
		else if (pSmart->brightness_level == 134)
			bl_level = 202;
		else if (pSmart->brightness_level == 143)
			bl_level = 216;
		else if (pSmart->brightness_level == 152)
			bl_level = 227;
		else if (pSmart->brightness_level == 162)
			bl_level = 241;
	} else {
		/* 77CD ~ 2CD */
		bl_level = AOR_DIM_BASE_CD_REVF;
	}

	if (pSmart->brightness_level < 350) {
		for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
			point_index = S6E3_ARRAY[cnt+1];
			temp_cal_data =
			((long long)(candela_coeff_2p15[point_index])) *
			((long long)(bl_level));
			candela_level[cnt] = temp_cal_data;
		}

	} else {
		for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
			point_index = S6E3_ARRAY[cnt+1];
			temp_cal_data =
			((long long)(candela_coeff_2p2[point_index])) *
			((long long)(bl_level));
			candela_level[cnt] = temp_cal_data;
		}
	}

#ifdef SMART_DIMMING_DEBUG
	printk(KERN_INFO "\n candela_1:%llu  candela_3:%llu  candela_11:%llu ",
		candela_level[0], candela_level[1], candela_level[2]);
	printk(KERN_INFO "candela_23:%llu  candela_35:%llu  candela_51:%llu ",
		candela_level[3], candela_level[4], candela_level[5]);
	printk(KERN_INFO "candela_87:%llu  candela_151:%llu  candela_203:%llu ",
		candela_level[6], candela_level[7], candela_level[8]);
	printk(KERN_INFO "candela_255:%llu brightness_level %d\n", candela_level[9], pSmart->brightness_level);
#endif

	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
		if (searching_function_360(candela_level[cnt],
			&(bl_index[cnt]), GAMMA_CURVE_2P2)) {
			pr_info("%s searching functioin error cnt:%d\n",
			__func__, cnt);
		}
	}

	/*
	*	Candela compensation
	*/
	for (cnt = 1; cnt < S6E3_TABLE_MAX; cnt++) {
		table_index = find_cadela_table(pSmart->brightness_level);

		if (table_index == -1) {
			table_index = CCG6_MAX_TABLE;
			pr_info("%s fail candela table_index cnt : %d brightness %d",
				__func__, cnt, pSmart->brightness_level);
		}

		bl_index[S6E3_TABLE_MAX - cnt] +=
			gradation_offset_T_wqhd_revF[table_index][cnt - 1];

		/* THERE IS M-GRAY0 target */
		if (bl_index[S6E3_TABLE_MAX - cnt] == 0)
			bl_index[S6E3_TABLE_MAX - cnt] = 1;
	}

#ifdef SMART_DIMMING_DEBUG
	printk(KERN_INFO "\n bl_index_1:%d  bl_index_3:%d  bl_index_11:%d",
		bl_index[0], bl_index[1], bl_index[2]);
	printk(KERN_INFO "bl_index_23:%d bl_index_35:%d  bl_index_51:%d",
		bl_index[3], bl_index[4], bl_index[5]);
	printk(KERN_INFO "bl_index_87:%d  bl_index_151:%d bl_index_203:%d",
		bl_index[6], bl_index[7], bl_index[8]);
	printk(KERN_INFO "bl_index_255:%d\n", bl_index[9]);
#endif
	/*Generate Gamma table*/
	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++)
		(void)Make_hexa[cnt](bl_index , pSmart, str);

	/* To avoid overflow */
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
		gamma_setting[cnt] = str[cnt];

	/*
	*	RGB compensation
	*/
	for (cnt = 0; cnt < RGB_COMPENSATION; cnt++) {
		table_index = find_cadela_table(pSmart->brightness_level);

		if (table_index == -1) {
			table_index = CCG6_MAX_TABLE;
			pr_info("%s fail RGB table_index cnt : %d brightness %d",
				__func__, cnt, pSmart->brightness_level);
		}

		if (cnt < 3) {
			level_V255 = gamma_setting[cnt * 2] << 8 | gamma_setting[(cnt * 2) + 1];
			level_V255 +=
				rgb_offset_T_wqhd_revF[table_index][cnt];
			level_255_temp_MSB = level_V255 / 256;

			gamma_setting[cnt * 2] = level_255_temp_MSB & 0xff;
			gamma_setting[(cnt * 2) + 1] = level_V255 & 0xff;
		} else {
			gamma_setting[cnt+3] += rgb_offset_T_wqhd_revF[table_index][cnt];
		}
	}
	/*subtration MTP_OFFSET value from generated gamma table*/
	mtp_offset_substraction(pSmart, gamma_setting);

	/* To avoid overflow */
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
		str[cnt] = gamma_setting[cnt];
}

static void gamma_init_T_revH(struct SMART_DIM *pSmart, char *str, int size)
{
	long long candela_level[S6E3_TABLE_MAX] = {-1, };
	int bl_index[S6E3_TABLE_MAX] = {-1, };
	int gamma_setting[GAMMA_SET_MAX];

	long long temp_cal_data = 0;
	int bl_level = 0;

	int level_255_temp_MSB = 0;
	int level_V255 = 0;

	int point_index;
	int cnt;
	int table_index;

	pr_debug("%s : start !!\n",__func__);

	/*calculate candela level */
	if ((pSmart->brightness_level <= 350) &&
		(pSmart->brightness_level >= 265)) {
		/* 265CD ~ 350CD */
		if (pSmart->brightness_level == 350)
			bl_level = 350;
		else if (pSmart->brightness_level == 333)
			bl_level = 337;
		else if (pSmart->brightness_level == 316)
			bl_level = 319;
		else if (pSmart->brightness_level == 300)
			bl_level = 300;
		else if (pSmart->brightness_level == 282)
			bl_level = 283;
		else if (pSmart->brightness_level == 265)
			bl_level = 268;
	} else if ((pSmart->brightness_level < 265) &&
				(pSmart->brightness_level >= 172)) {
		/* 172CD ~ 265CD */
		bl_level = 253;
	} else if ((pSmart->brightness_level < 172) &&
				(pSmart->brightness_level > 77)) {
		/* 82CD ~ 162CD */
		if (pSmart->brightness_level == 82)
			bl_level = 122;
		else if (pSmart->brightness_level == 87)
			bl_level = 128;
		else if (pSmart->brightness_level == 93)
			bl_level = 136;
		else if (pSmart->brightness_level == 98)
			bl_level = 145;
		else if (pSmart->brightness_level == 105)
			bl_level = 155;
		else if (pSmart->brightness_level == 111)
			bl_level = 164;
		else if (pSmart->brightness_level == 119)
			bl_level = 175;
		else if (pSmart->brightness_level == 126)
			bl_level = 187;
		else if (pSmart->brightness_level == 134)
			bl_level = 196;
		else if (pSmart->brightness_level == 143)
			bl_level = 208;
		else if (pSmart->brightness_level == 152)
			bl_level = 223;
		else if (pSmart->brightness_level == 162)
			bl_level = 234;
	} else {
		/* 77CD ~ 2CD */
		bl_level = AOR_DIM_BASE_CD_REVH;
	}

	if (pSmart->brightness_level < 350) {
		for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
			point_index = S6E3_ARRAY[cnt+1];
			temp_cal_data =
			((long long)(candela_coeff_2p15[point_index])) *
			((long long)(bl_level));
			candela_level[cnt] = temp_cal_data;
		}

	} else {
		for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
			point_index = S6E3_ARRAY[cnt+1];
			temp_cal_data =
			((long long)(candela_coeff_2p2[point_index])) *
			((long long)(bl_level));
			candela_level[cnt] = temp_cal_data;
		}
	}

#ifdef SMART_DIMMING_DEBUG
	printk(KERN_INFO "\n candela_1:%llu  candela_3:%llu  candela_11:%llu ",
		candela_level[0], candela_level[1], candela_level[2]);
	printk(KERN_INFO "candela_23:%llu  candela_35:%llu  candela_51:%llu ",
		candela_level[3], candela_level[4], candela_level[5]);
	printk(KERN_INFO "candela_87:%llu  candela_151:%llu  candela_203:%llu ",
		candela_level[6], candela_level[7], candela_level[8]);
	printk(KERN_INFO "candela_255:%llu brightness_level %d\n", candela_level[9], pSmart->brightness_level);
#endif

	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
		if (searching_function_360(candela_level[cnt],
			&(bl_index[cnt]), GAMMA_CURVE_2P2)) {
			pr_info("%s searching functioin error cnt:%d\n",
			__func__, cnt);
		}
	}

	/*
	*	Candela compensation
	*/
	for (cnt = 1; cnt < S6E3_TABLE_MAX; cnt++) {
		table_index = find_cadela_table(pSmart->brightness_level);

		if (table_index == -1) {
			table_index = CCG6_MAX_TABLE;
			pr_info("%s fail candela table_index cnt : %d brightness %d",
				__func__, cnt, pSmart->brightness_level);
		}

		bl_index[S6E3_TABLE_MAX - cnt] +=
			gradation_offset_T_wqhd_revH[table_index][cnt - 1];

		/* THERE IS M-GRAY0 target */
		if (bl_index[S6E3_TABLE_MAX - cnt] == 0)
			bl_index[S6E3_TABLE_MAX - cnt] = 1;
	}

#ifdef SMART_DIMMING_DEBUG
	printk(KERN_INFO "\n bl_index_1:%d  bl_index_3:%d  bl_index_11:%d",
		bl_index[0], bl_index[1], bl_index[2]);
	printk(KERN_INFO "bl_index_23:%d bl_index_35:%d  bl_index_51:%d",
		bl_index[3], bl_index[4], bl_index[5]);
	printk(KERN_INFO "bl_index_87:%d  bl_index_151:%d bl_index_203:%d",
		bl_index[6], bl_index[7], bl_index[8]);
	printk(KERN_INFO "bl_index_255:%d\n", bl_index[9]);
#endif
	/*Generate Gamma table*/
	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++)
		(void)Make_hexa[cnt](bl_index , pSmart, str);

	/* To avoid overflow */
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
		gamma_setting[cnt] = str[cnt];

	/*
	*	RGB compensation
	*/
	for (cnt = 0; cnt < RGB_COMPENSATION; cnt++) {
		table_index = find_cadela_table(pSmart->brightness_level);

		if (table_index == -1) {
			table_index = CCG6_MAX_TABLE;
			pr_info("%s fail RGB table_index cnt : %d brightness %d",
				__func__, cnt, pSmart->brightness_level);
		}

		if (cnt < 3) {
			level_V255 = gamma_setting[cnt * 2] << 8 | gamma_setting[(cnt * 2) + 1];
			level_V255 +=
				rgb_offset_T_wqhd_revH[table_index][cnt];
			level_255_temp_MSB = level_V255 / 256;

			gamma_setting[cnt * 2] = level_255_temp_MSB & 0xff;
			gamma_setting[(cnt * 2) + 1] = level_V255 & 0xff;
		} else {
			gamma_setting[cnt+3] += rgb_offset_T_wqhd_revH[table_index][cnt];
		}
	}
	/*subtration MTP_OFFSET value from generated gamma table*/
	mtp_offset_substraction(pSmart, gamma_setting);

	/* To avoid overflow */
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
		str[cnt] = gamma_setting[cnt];
}


#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQXGA_PT_PANEL)
static int gradation_offset_T_wqxga_revA[][9] = {
/*	V255 V203 V151 V87 V51 V35 V23 V11 V3 */
	{0, 10, 15, 23, 31, 35, 38, 34, 33},
	{0, 7, 12, 18, 22, 27, 30, 34, 33},
	{0, 6, 10, 14, 18, 22, 26, 29, 29},
	{0, 5, 9, 12, 15, 19, 22, 26, 28},
	{0, 5, 7, 11, 14, 17, 19, 24, 20},
	{0, 4, 7, 10, 12, 15, 18, 22, 21},
	{0, 4, 6, 9, 11, 13, 16, 20, 21},
	{0, 4, 6, 8, 10, 12, 15, 18, 20},
	{0, 4, 5, 7, 9, 11, 14, 17, 19},
	{0, 4, 5, 7, 9, 11, 13, 17, 17},
	{0, 3, 4, 6, 7, 10, 12, 15, 19},
	{0, 3, 4, 6, 7, 9, 11, 15, 14},
	{0, 3, 4, 6, 7, 9, 10, 14, 17},
	{0, 3, 4, 5, 7, 8, 10, 14, 16},
	{0, 3, 4, 5, 6, 8, 9, 13, 16},
	{0, 3, 4, 5, 6, 8, 9, 13, 15},
	{0, 3, 3, 4, 5, 7, 8, 12, 13},
	{0, 3, 3, 4, 5, 6, 8, 12, 9},
	{0, 3, 3, 4, 5, 6, 7, 11, 14},
	{0, 3, 3, 4, 4, 6, 7, 11, 11},
	{0, 3, 3, 4, 4, 5, 7, 10, 13},
	{0, 3, 3, 4, 4, 5, 7, 10, 12},
	{0, 3, 3, 3, 4, 5, 6, 10, 9},
	{0, 3, 3, 3, 3, 5, 6, 9, 10},
	{0, 3, 3, 3, 3, 5, 6, 9, 9},
	{0, 3, 3, 3, 3, 4, 5, 8, 12},
	{0, 3, 3, 3, 3, 4, 5, 8, 9},
	{0, 3, 3, 3, 3, 3, 4, 7, 9},
	{0, 3, 2, 2, 2, 3, 4, 7, 7},
	{0, 3, 2, 2, 2, 3, 4, 7, 5},
	{0, 3, 2, 2, 2, 3, 4, 6, 8},
	{0, 2, 2, 2, 2, 2, 3, 6, 5},
	{0, 3, 2, 2, 2, 2, 3, 5, 7},
	{0, 2, 2, 2, 1, 2, 3, 5, 4},
	{0, 2, 2, 2, 1, 2, 3, 4, 7},
	{0, 2, 2, 2, 1, 1, 2, 4, 5},
	{0, 2, 2, 1, 1, 1, 2, 4, 3},
	{0, 2, 1, 1, 1, 1, 2, 3, 7},
	{0, 2, 1, 1, 1, 1, 2, 3, 4},
	{0, 2, 1, 1, 1, 1, 2, 3, 2},
	{0, 2, 2, 2, 1, 1, 2, 3, 3},
	{0, 2, 3, 2, 1, 1, 2, 3, 5},
	{0, 2, 3, 2, 1, 1, 2, 2, 4},
	{0, 2, 3, 3, 2, 2, 2, 2, 6},
	{0, 2, 3, 3, 2, 2, 2, 2, 7},
	{0, 2, 3, 3, 2, 1, 2, 2, 5},
	{0, 1, 3, 2, 2, 1, 1, 2, 1},
	{0, 2, 3, 3, 2, 2, 2, 2, 3},
	{0, 1, 4, 3, 2, 1, 2, 2, 2},
	{0, 2, 4, 3, 2, 1, 2, 2, 3},
	{0, 1, 4, 3, 2, 2, 2, 2, 3},
	{0, 2, 3, 3, 1, 1, 1, 1, 2},
	{0, 2, 4, 3, 1, 1, 2, 1, 3},
	{0, 2, 4, 3, 1, 1, 1, 1, 2},
	{0, 2, 3, 2, 1, 0, 1, 1, 0},
	{0, 2, 3, 2, 0, 0, 0, 0, 4},
	{0, 2, 2, 2, 0, 0, 0, 0, 3},
	{0, 1, 2, 2, 0, 0, 0, 0, 1},
	{0, 1, 2, 1, 0, 0, 0, 0, 0},
	{0, 1, 2, 1, 0, 0, 0, 0, 1},
	{0, 1, 1, 1, 0, 0, 0, -1, 0},
	{0, 1, 1, 0, 0, 0, -1, -1, 2},
	{0, 0, -1, 0, -1, 0, 0, 0, 0},
	{0, 0, 1, 0, 0, 0, 0, -1, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static int gradation_offset_T_wqxga_revD[][9] = {
/*	V255 V203 V151 V87 V51 V35 V23 V11 V3 */
	{0, 10, 20, 26, 34, 36, 37, 39, 42},
	{0, 5, 10, 15, 19, 22, 26, 29, 32},
	{0, 7, 10, 14, 17, 20, 24, 28, 31},
	{0, 4, 8, 12, 15, 17, 20, 24, 27},
	{0, 5, 6, 10, 12, 13, 16, 20, 23},
	{0, 4, 5, 8, 11, 13, 16, 19, 23},
	{0, 5, 7, 12, 12, 13, 16, 18, 21},
	{0, 4, 1, 6, 7, 9, 12, 15, 18},
	{0, 4, 5, 7, 9, 10, 13, 15, 19},
	{0, 2, 3, 5, 7, 8, 11, 14, 18},
	{0, 3, 4, 6, 8, 8, 13, 15, 19},
	{0, 5, 8, 7, 8, 9, 11, 14, 17},
	{0, 2, 4, 6, 7, 8, 10, 13, 16},
	{0, 3, 4, 6, 6, 7, 9, 12, 16},
	{0, 5, 4, 6, 6, 7, 9, 12, 15},
	{0, 5, 4, 5, 5, 6, 8, 11, 14},
	{0, 3, 3, 4, 5, 6, 8, 11, 14},
	{0, 4, 5, 6, 5, 5, 7, 10, 13},
	{0, 3, 4, 6, 6, 6, 8, 11, 13},
	{0, 4, 5, 5, 5, 5, 7, 10, 13},
	{0, 4, 3, 4, 4, 4, 6, 9, 12},
	{0, 2, 4, 4, 4, 4, 6, 9, 12},
	{0, 4, 5, 4, 3, 3, 5, 8, 10},
	{0, 3, 4, 4, 3, 3, 5, 8, 10},
	{0, 4, 5, 4, 4, 4, 6, 8, 10},
	{0, 4, 4, 3, 3, 3, 5, 7, 8},
	{0, 2, 3, 4, 2, 3, 5, 7, 9},
	{0, 4, 3, 3, 3, 3, 5, 7, 10},
	{0, 3, 4, 4, 3, 2, 4, 6, 8},
	{0, 3, 2, 3, 2, 2, 4, 5, 7},
	{0, 2, 2, 3, 2, 2, 4, 5, 7},
	{0, 2, 2, 2, 2, 2, 4, 5, 8},
	{0, 3, 4, 3, 2, 1, 3, 4, 7},
	{0, 3, 5, 3, 2, 1, 3, 4, 7},
	{0, 3, 2, 3, 2, 1, 3, 4, 6},
	{0, 1, 0, 2, 2, 1, 3, 4, 6},
	{0, 3, 3, 2, 2, 1, 3, 3, 5},
	{0, 3, 3, 2, 1, 1, 3, 3, 5},
	{0, 3, 3, 1, 1, 0, 2, 2, 4},
	{0, 3, 3, 2, 1, 1, 2, 2, 3},
	{0, 3, 3, 3, 1, 1, 2, 3, 4},
	{0, 2, 2, 2, 1, 1, 2, 3, 5},
	{0, 2, 4, 3, 1, 1, 1, 1, 3},
	{0, 3, 4, 3, 1, 1, 2, 2, 4},
	{0, 3, 4, 4, 0, 1, 2, 2, 4},
	{0, 2, 5, 3, 0, 1, 1, 2, 4},
	{0, 2, 4, 3, 1, 1, 2, 2, 4},
	{0, 0, 4, 3, 1, 1, 1, 1, 4},
	{0, 2, 6, 4, 1, 1, 1, 1, 4},
	{0, 2, 6, 2, 1, 1, 1, 2, 4},
	{0, 3, 6, 4, 1, 1, 1, 2, 3},
	{0, 2, 4, 3, 1, 1, 1, 1, 2},
	{0, 1, 3, 3, 0, 1, 1, 1, 2},
	{0, 2, 2, 2, 1, 1, 0, 0, 0},
	{0, 2, 4, 2, 1, 1, 0, 0, 0},
	{0, 2, 4, 3, 1, 1, 0, 0, 2},
	{0, 2, 3, 2, 0, 0, 0, 0, 0},
	{0, 2, 4, 3, 1, 1, 0, 0, 0},
	{0, 2, 3, 1, 0, 0, -1, 0, 0},
	{0, 3, 3, 1, 1, 0, 1, 0, 0},
	{0, 1, 3, 1, 1, 0, 0, -1, 0},
	{0, 1, 2, 2, 0, 0, 0, -1, 0},
	{0, 1, 2, 1, -1, 0, -1, -1, 0},
	{0, 0, 0, -1, -1, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static int gradation_offset_T_wqxga_revE[][9] = {
/*	V255 V203 V151 V87 V51 V35 V23 V11 V3 */
	{0, 8, 13, 19, 25, 29, 34, 30, 29},
	{0, 7, 10, 15, 18, 22, 27, 30, 29},
	{0, 6, 9, 13, 15, 19, 22, 26, 29},
	{0, 6, 7, 11, 13, 15, 19, 23, 25},
	{0, 5, 7, 10, 11, 13, 17, 21, 20},
	{0, 5, 6, 9, 10, 12, 16, 19, 21},
	{0, 5, 5, 8, 9, 11, 14, 17, 21},
	{0, 4, 5, 8, 8, 10, 13, 16, 20},
	{0, 4, 5, 7, 7, 9, 12, 15, 14},
	{0, 4, 4, 6, 6, 8, 11, 14, 13},
	{0, 4, 4, 6, 6, 8, 10, 14, 16},
	{0, 4, 4, 6, 5, 7, 9, 13, 16},
	{0, 4, 4, 5, 5, 7, 9, 13, 14},
	{0, 4, 4, 5, 5, 6, 9, 12, 16},
	{0, 4, 4, 5, 5, 6, 8, 12, 12},
	{0, 4, 4, 5, 4, 5, 8, 11, 15},
	{0, 4, 4, 5, 4, 5, 7, 10, 9},
	{0, 4, 4, 4, 4, 5, 7, 10, 9},
	{0, 4, 3, 5, 4, 5, 7, 10, 11},
	{0, 4, 3, 5, 4, 4, 7, 10, 11},
	{0, 4, 3, 4, 4, 4, 7, 9, 8},
	{0, 4, 3, 4, 3, 4, 6, 9, 11},
	{0, 4, 3, 4, 3, 3, 6, 8, 12},
	{0, 4, 3, 4, 3, 3, 5, 8, 9},
	{0, 4, 3, 4, 3, 3, 5, 8, 8},
	{0, 4, 3, 4, 3, 3, 5, 7, 10},
	{0, 3, 3, 4, 3, 3, 5, 7, 8},
	{0, 3, 3, 4, 2, 2, 5, 6, 10},
	{0, 3, 3, 3, 2, 2, 5, 6, 8},
	{0, 3, 3, 3, 2, 2, 4, 6, 6},
	{0, 3, 3, 3, 2, 2, 4, 5, 9},
	{0, 3, 2, 3, 2, 2, 4, 5, 6},
	{0, 3, 2, 3, 2, 1, 4, 4, 3},
	{0, 3, 2, 3, 2, 1, 3, 4, 7},
	{0, 3, 2, 3, 2, 1, 3, 4, 5},
	{0, 3, 2, 3, 2, 1, 3, 3, 2},
	{0, 3, 2, 2, 1, 1, 3, 3, 6},
	{0, 3, 2, 2, 1, 1, 3, 3, 4},
	{0, 3, 2, 2, 1, 1, 2, 3, 1},
	{0, 3, 2, 2, 1, 0, 2, 2, 5},
	{0, 2, 1, 2, 1, 1, 2, 2, 6},
	{0, 2, 2, 2, 1, 1, 2, 3, 1},
	{0, 1, 3, 3, 1, 0, 2, 2, 2},
	{0, 2, 3, 3, 0, 1, 2, 2, 2},
	{0, 1, 3, 2, 1, 1, 1, 2, 3},
	{0, 2, 3, 2, 0, 1, 1, 2, 3},
	{0, 2, 3, 2, 1, 1, 2, 2, 5},
	{0, 1, 3, 3, 1, 1, 1, 1, 6},
	{0, 1, 4, 3, 1, 1, 2, 1, 5},
	{0, 1, 3, 2, 1, 1, 1, 2, 1},
	{0, 2, 4, 3, 1, 1, 2, 2, 1},
	{0, 1, 4, 3, 1, 1, 1, 1, 0},
	{0, 3, 3, 3, 1, 1, 1, 1, 0},
	{0, 2, 3, 2, 1, 1, 0, 1, 0},
	{0, 2, 3, 2, 1, 1, 0, 1, 0},
	{0, 0, 2, 1, 0, 0, 0, 0, 2},
	{0, 0, 2, 1, 0, 0, 0, 0, 0},
	{0, 0, 2, 1, 0, 0, 0, 0, 0},
	{0, 0, 1, 1, 0, 0, 0, 0, 0},
	{0, 0, 2, 1, 0, 0, 0, 0, 2},
	{0, 1, 1, 0, -1, 0, 0, -1, 2},
	{0, 1, 0, 0, -1, -1, 0, 0, 0},
	{0, 0, 0, 0, -1, 0, -1, 0, 0},
	{0, 0, 0, 0, -1, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0},
};


static int gradation_offset_T_wqxga_ym4[][9] = {
	{0, 13, 24, 34, 39, 41, 44, 36, 56},
	{0, 10, 20, 28, 32, 34, 35, 36, 56},
	{0, 9, 17, 25, 28, 29, 31, 31, 40},
	{0, 8, 15, 22, 24, 25, 27, 27, 42},
	{0, 8, 15, 21, 22, 24, 25, 25, 23},
	{0, 7, 13, 19, 20, 21, 22, 23, 21},
	{0, 7, 13, 18, 18, 19, 20, 21, 20},
	{0, 6, 12, 17, 17, 18, 19, 19, 28},
	{0, 6, 11, 16, 16, 17, 18, 19, 19},
	{0, 5, 10, 14, 15, 15, 17, 17, 17},
	{0, 6, 11, 14, 14, 14, 16, 16, 15},
	{0, 5, 9, 13, 14, 14, 16, 15, 22},
	{0, 5, 8, 13, 13, 13, 15, 15, 14},
	{0, 5, 9, 12, 12, 12, 14, 14, 15},
	{0, 5, 8, 11, 11, 12, 13, 14, 13},
	{0, 5, 8, 11, 11, 12, 13, 13, 14},
	{0, 4, 8, 10, 10, 10, 12, 12, 13},
	{0, 4, 8, 10, 10, 10, 11, 12, 12},
	{0, 4, 7, 10, 9, 10, 11, 11, 12},
	{0, 4, 7, 9, 9, 9, 11, 11, 12},
	{0, 4, 7, 9, 8, 9, 10, 10, 11},
	{0, 4, 7, 9, 8, 9, 10, 10, 11},
	{0, 4, 7, 8, 7, 8, 9, 9, 10},
	{0, 4, 6, 8, 7, 8, 8, 9, 8},
	{0, 4, 6, 8, 7, 8, 8, 9, 7},
	{0, 4, 6, 8, 7, 7, 8, 8, 10},
	{0, 4, 6, 7, 6, 7, 8, 8, 8},
	{0, 3, 5, 7, 6, 6, 7, 7, 9},
	{0, 3, 5, 6, 5, 6, 7, 7, 7},
	{0, 3, 5, 6, 5, 6, 7, 7, 5},
	{0, 3, 5, 6, 5, 5, 6, 6, 7},
	{0, 3, 5, 5, 4, 5, 6, 6, 5},
	{0, 3, 5, 5, 4, 5, 6, 5, 9},
	{0, 3, 4, 5, 3, 4, 5, 5, 4},
	{0, 3, 4, 5, 3, 4, 5, 4, 6},
	{0, 3, 4, 4, 3, 3, 5, 4, 3},
	{0, 3, 4, 4, 2, 3, 4, 4, 1},
	{0, 2, 4, 4, 2, 3, 4, 3, 5},
	{0, 2, 3, 3, 2, 2, 4, 3, 2},
	{0, 2, 3, 3, 1, 2, 3, 3, 1},
	{0, 3, 4, 3, 2, 2, 3, 3, 1},
	{0, 2, 3, 3, 2, 2, 3, 3, 1},
	{0, 2, 4, 3, 2, 2, 2, 2, 1},
	{0, 2, 4, 4, 2, 2, 3, 2, 2},
	{0, 2, 4, 3, 2, 2, 2, 2, 5},
	{0, 2, 4, 4, 2, 2, 2, 2, 5},
	{0, 1, 4, 4, 2, 2, 2, 2, 1},
	{0, 0, 3, 3, 2, 2, 2, 2, 1},
	{0, 1, 4, 4, 2, 2, 2, 2, 1},
	{0, 0, 3, 3, 2, 2, 2, 2, 1},
	{0, 0, 4, 4, 2, 2, 2, 2, 3},
	{0, 1, 3, 3, 2, 1, 1, 1, 2},
	{0, 1, 4, 3, 2, 1, 2, 1, 4},
	{0, 1, 3, 3, 2, 1, 1, 2, 1},
	{0, 1, 3, 3, 1, 1, 1, 2, 0},
	{0, 1, 2, 2, 1, 0, 1, 1, 1},
	{0, 1, 2, 2, 0, 0, 0, 1, 0},
	{0, 0, 1, 1, 0, 0, 0, 1, 0},
	{0, 0, 1, 1, 0, -1, 0, 0, 2},
	{0, 0, 0, 0, 0, 0, -1, 0, 3},
	{0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 1, 0, -1, -1, -1, 0, 0},
	{0, -1, 0, 0, -1, -1, 0, 0, 0},
	{0, -1, -1, -1, -1, -2, -1, -1, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static int rgb_offset_T_wqxga_revA[][RGB_COMPENSATION] = {
/*	R255 G255 B255 R203 G203 B203 R151 G151 B151
	R87 G87 B87 R51 G51 B51 R35 G35 B35
	R23 G23 B23 R11 G11 B11
*/
	{-8, 0, -4, -5, 0, -3, -7, 0, -3, -10, 2, -4, -10, 5, -10, -4, 4, -9, -3, 4, -9, 4, 5, -12},
	{-5, 0, -3, -4, 0, -2, -4, 0, -2, -8, 1, -4, -13, 3, -8, -5, 3, -8, -3, 3, -9, 4, 5, -12},
	{-3, 0, -2, -3, 0, -1, -4, 0, -2, -8, 1, -4, -11, 2, -6, -8, 4, -10, -3, 4, -6, 4, 6, -14},
	{-2, 0, -1, -3, 0, -2, -2, 0, -1, -7, 1, -3, -11, 2, -6, -8, 4, -10, -4, 4, -8, 2, 6, -14},
	{-1, 0, -1, -2, 0, -1, -4, 0, -2, -6, 1, -3, -8, 2, -4, -8, 3, -8, -6, 3, -11, 1, 5, -10},
	{-1, 0, -1, -2, 0, -1, -2, 0, -1, -6, 1, -3, -10, 1, -4, -8, 4, -8, -5, 4, -8, 1, 6, -14},
	{-1, 0, -1, -2, 0, -1, -2, 0, -1, -5, 1, -2, -8, 1, -4, -7, 3, -7, -7, 3, -10, 0, 7, -15},
	{0, 0, -1, -2, 0, 0, -2, 0, -1, -3, 1, -2, -8, 1, -4, -8, 3, -6, -5, 3, -9, 2, 8, -17},
	{0, 0, 0, -2, 0, -1, -2, 0, -1, -4, 1, -2, -7, 1, -3, -7, 3, -6, -5, 3, -8, 0, 8, -17},
	{0, 0, 0, -2, 0, -1, -1, 0, -1, -5, 0, -2, -6, 1, -3, -8, 2, -6, -6, 2, -10, 0, 6, -14},
	{0, 0, 0, -1, 0, -1, -1, 0, 0, -3, 1, -2, -7, 1, -3, -7, 2, -5, -7, 2, -10, 0, 9, -19},
	{0, 0, 0, -1, 0, -1, -1, 0, 0, -3, 1, -2, -6, 1, -2, -7, 2, -6, -5, 2, -9, -1, 6, -14},
	{0, 0, 0, -1, 0, -1, -1, 0, 0, -3, 0, -2, -6, 1, -3, -6, 2, -4, -7, 2, -11, 0, 7, -16},
	{0, 0, 0, -1, 0, -1, -1, 0, 0, -4, 0, -2, -5, 1, -2, -8, 2, -6, -6, 2, -10, -1, 7, -16},
	{0, 0, 0, -1, 0, -1, -1, 0, 0, -4, 0, -2, -5, 1, -2, -6, 2, -4, -7, 2, -10, 0, 9, -18},
	{0, 0, 0, 0, 0, 0, -2, 0, -1, -4, 0, -2, -5, 1, -2, -6, 1, -4, -7, 1, -10, 0, 7, -16},
	{0, 0, 0, 0, 0, 0, -2, 0, -1, -1, 0, -1, -6, 1, -2, -5, 1, -4, -8, 1, -9, -1, 7, -16},
	{0, 0, 0, 0, 0, 0, -2, 0, -1, -1, 0, -1, -6, 1, -2, -5, 2, -4, -6, 2, -6, -2, 7, -15},
	{0, 0, 0, 0, 0, 0, -2, 0, -1, -1, 0, -1, -5, 0, -2, -6, 2, -4, -6, 2, -9, -2, 9, -18},
	{0, 0, 0, 0, 0, 0, -2, 0, -1, -1, 0, -1, -4, 0, -2, -6, 1, -4, -6, 1, -8, -2, 8, -16},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -2, 0, -2, -3, 0, -1, -6, 2, -4, -6, 2, -6, -3, 9, -20},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, -1, -4, 0, -2, -6, 1, -4, -6, 1, -6, -4, 9, -20},
	{0, 0, 0, 0, 0, 0, -1, 0, -1, -1, 0, 0, -5, 0, -2, -5, 1, -4, -7, 1, -8, -3, 7, -15},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, -1, -4, 0, -2, -4, 1, -3, -5, 1, -6, -4, 9, -18},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, -1, -3, 0, -1, -5, 1, -4, -5, 1, -6, -5, 8, -18},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, -1, -2, 0, -1, -5, 1, -3, -6, 1, -7, -4, 9, -20},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, -3, 0, -1, -5, 1, -3, -6, 1, -6, -3, 9, -18},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, -2, 0, 0, -4, 1, -3, -6, 1, -6, -4, 8, -18},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -3, 0, 0, -3, 1, -2, -6, 1, -6, -4, 7, -16},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -3, 0, 0, -3, 0, -2, -6, 0, -6, -4, 7, -14},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -3, 0, 0, -3, 0, -2, -4, 0, -4, -5, 8, -18},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1, -3, 0, 0, -3, 0, -2, -4, 0, -5, -4, 7, -14},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -2, 0, 1, -3, 0, -2, -4, 0, -5, -4, 8, -17},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1, -1, 0, 1, -2, 0, -2, -4, 0, -4, -4, 7, -15},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1, -1, 0, 1, -2, 0, -2, -3, 0, -4, -6, 9, -18},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1, 0, 0, 1, -2, 0, -2, -3, 0, -4, -5, 7, -15},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, -1, -3, 0, -4, -5, 6, -14},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, -1, -3, 0, -4, -4, 7, -15},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, -1, -2, 0, -3, -2, 6, -12},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, -1, -2, 0, -2, -2, 4, -10},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, -1, -3, 0, -3, -2, 5, -10},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -2, 0, -1, -2, 0, -3, -2, 4, -10},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, -2, 0, -2, -2, 4, -10},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, -1, -2, 0, -2, -2, 5, -10},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, 0, -2, 0, -2, -3, 5, -12},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -2, 0, -2, -1, 4, -9},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, -2, 0, -2, -4, 3, -8},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -2, 0, -2, -3, 4, -9},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, -1, -4, 3, -8},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, -1, -2, 0, 0, -2, 0, -1, -4, 3, -8},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -4, 3, -8},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -2, 0, 0, 0, 0, 0, -4, 2, -6},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, 0, -4, 2, -6},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, -4, 2, -4},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, -3, 1, -3},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 1, -2},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -2},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0},
	};

static int rgb_offset_T_wqxga_revD[][RGB_COMPENSATION] = {
/*	R255 G255 B255 R203 G203 B203 R151 G151 B151
	R87 G87 B87 R51 G51 B51 R35 G35 B35
	R23 G23 B23 R11 G11 B11
*/
	{-7, 1, -2, -5, -1, -4, -7, -1, -4, -14, 1, -5, -27, -2, -14, -11, 5, -7, -12, 5, -6, -8, 10, -7},
	{-3, 1, 0, -1, 3, 0, -5, 1, -2, -7, 4, -1, -20, 1, -7, -12, 6, -7, -18, 1, -12, -1, 7, -11},
	{-1, 2, 1, -2, 0, -1, -5, 1, -2, -7, 3, -4, -15, 1, -3, -17, 4, -10, -22, -1, -13, -10, 2, -19},
	{0, 2, 2, 0, 2, 0, -4, 0, -2, -7, 2, -4, -17, -2, -6, -13, 6, -7, -21, 2, -12, -8, 5, -18},
	{-1, 0, 0, -2, 1, -1, 1, 4, 2, -6, 1, -4, -12, 1, -2, -13, 6, -6, -16, 6, -9, -9, 5, -19},
	{0, 1, 1, 1, 2, 0, -1, 2, 1, -5, 3, -3, -12, -1, -2, -18, 2, -9, -18, 4, -11, -6, 9, -17},
	{0, 1, 1, -2, 1, -1, -3, 0, -1, -9, -3, -8, -9, 2, -1, -15, 2, -7, -20, 3, -12, -3, 13, -14},
	{-1, 1, 0, 1, 2, 1, 4, 6, 5, -7, -1, -5, -5, 4, 2, -13, 3, -6, -19, 4, -12, -8, 11, -18},
	{1, 1, 1, -1, 2, 0, 0, 0, 0, -4, 1, -3, -9, 0, -1, -14, 2, -6, -22, 1, -14, -2, 14, -14},
	{2, 2, 2, 2, 3, 2, -2, 0, -1, -3, 2, -3, -9, 0, -1, -10, 5, -4, -19, 2, -13, -8, 12, -19},
	{1, 1, 1, 1, 2, 1, -1, 0, -1, -4, 2, -2, -10, -3, -3, -6, 7, 0, -31, -9, -23, -11, 12, -19},
	{0, 0, 0, -3, 0, -2, -2, 0, 0, -1, 2, -1, -8, -1, -3, -12, 0, -5, -22, 4, -14, -9, 11, -19},
	{1, 1, 1, 2, 3, 2, -3, -1, -2, -3, 0, -3, -7, 0, -1, -12, 1, -6, -17, 4, -10, -11, 12, -22},
	{1, 1, 1, 1, 2, 1, -1, 0, -1, -4, 0, -3, -5, 1, 1, -10, 2, -5, -18, 4, -11, -9, 14, -22},
	{1, 1, 1, -2, 0, -1, 1, 2, 1, -4, -1, -3, -4, 2, 1, -12, 0, -6, -18, 4, -11, -11, 13, -22},
	{1, 1, 1, -2, 0, -1, 2, 2, 2, -3, 1, -3, -3, 2, 2, -8, 2, -5, -18, 3, -12, -11, 14, -23},
	{1, 1, 1, 1, 2, 1, 0, 0, 0, -3, 1, -2, -4, -1, 0, -8, 1, -5, -19, 2, -12, -13, 12, -24},
	{0, 0, 0, 0, 1, 0, 0, 0, 0, -3, 0, -3, -3, 2, 2, -4, 5, 0, -17, 3, -12, -15, 13, -24},
	{-5, 2, 0, 7, 0, 2, -2, -1, -2, -4, -1, -3, -5, -1, -1, -5, 4, -2, -20, -1, -13, -17, 11, -26},
	{0, 0, 0, 0, 1, 1, 0, 0, -1, -2, 1, -2, -4, 0, 0, -6, 3, -2, -15, 3, -11, -20, 10, -29},
	{0, 0, 0, 0, 0, 0, 1, 2, 1, -2, 0, -2, -4, 0, 1, -2, 5, 0, -13, 5, -10, -20, 10, -29},
	{0, 0, 0, 2, 2, 2, -2, -1, -2, -1, 1, -1, -3, 0, 2, -4, 4, -2, -12, 4, -10, -23, 10, -30},
	{0, 0, 0, -1, 0, -1, 0, 0, 0, 1, 2, 0, 0, 3, 4, -4, 3, -2, -8, 7, -8, -23, 9, -30},
	{0, 0, 0, 2, 1, 1, -2, 0, -1, -1, 1, -1, -1, 2, 3, -3, 3, -2, -9, 5, -9, -24, 8, -30},
	{0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 1, -1, -3, -2, 1, -3, 4, -1, -14, 1, -12, -17, 15, -26},
	{0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, -2, 0, 1, -3, 3, -2, -6, 5, -7, -18, 14, -26},
	{0, 0, 0, 1, 1, 1, 0, 1, 0, -3, -2, -3, 2, 5, 6, -6, -1, -5, -10, 3, -10, -20, 12, -28},
	{0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 2, -4, 1, -3, -11, 1, -12, -24, 9, -31},
	{0, 0, 0, 1, 1, 1, -1, 0, -1, -1, 0, -1, 0, 1, 3, -1, 4, 0, -6, 4, -9, -22, 11, -28},
	{0, 0, 0, 1, 1, 1, 0, 1, 1, -1, 1, -1, 2, 2, 4, -4, 2, -2, -6, 2, -10, -14, 19, -23},
	{0, 0, 0, 2, 2, 2, -1, 1, 0, -2, -1, -2, 3, 3, 5, -6, -1, -4, -6, 3, -10, -15, 18, -24},
	{0, 0, 0, 2, 2, 2, 0, 2, 1, 0, 1, -1, 0, 0, 2, -3, 1, -2, -8, 0, -12, -19, 13, -27},
	{0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 1, 0, 1, 0, 3, 0, 4, 0, -3, 4, -10, -16, 14, -25},
	{0, 0, 0, 0, 0, 0, -2, -1, -2, 1, 2, 1, 2, 1, 4, -1, 3, -1, -3, 3, -10, -19, 11, -27},
	{0, 0, 0, 0, 0, 0, 0, 1, 1, -2, 0, -2, 2, 1, 4, -1, 2, -1, -3, 3, -11, -19, 10, -27},
	{0, 0, 0, 2, 2, 2, 1, 1, 1, -1, 0, -2, 0, -1, 1, -1, 2, -1, -5, 1, -12, -20, 7, -28},
	{0, 0, 0, 0, 0, 0, -1, -1, -1, 1, 2, 0, 1, 0, 2, -2, 1, -2, -3, 2, -11, -6, 13, -18},
	{0, 0, 0, 0, 0, 0, -2, -1, -2, 2, 2, 2, 3, 2, 4, -4, -2, -5, -5, 0, -13, -6, 12, -17},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 1, 2, 0, 3, 3, 5, 1, -2, 2, -12, 1, 10, -11},
	{0, 0, 0, 0, 0, 0, -1, -1, -1, 0, 1, 0, 4, 2, 4, -5, -2, -5, 4, 7, -7, 2, 9, -10},
	{0, 0, 0, 1, 1, 1, -1, -1, -1, -1, 0, -1, 3, 2, 4, -1, 1, -2, -4, -1, -12, -9, 5, -20},
	{0, 0, 0, 2, 2, 2, 1, 0, 0, -2, 0, -1, 0, 0, 2, 0, 1, -1, 0, 3, -9, -11, 3, -21},
	{0, 0, 0, 1, 1, 1, -1, 0, -1, 0, 0, -1, 1, -1, 2, -1, 0, -2, 4, 7, -5, 9, 12, -6},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, 1, 1, -4, -1, -11, -1, 9, -14},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 1, 0, 2, 2, 4, 0, 0, -1, 0, 3, -7, -3, 6, -16},
	{0, 0, 0, 0, 0, 0, -1, -1, -1, 0, 1, 0, 3, 2, 4, 0, 1, 0, 0, 3, -7, -5, 3, -16},
	{0, 0, 0, 1, 1, 1, 0, 0, 0, -1, -1, -1, 0, 1, 1, 4, 4, 2, -4, -1, -9, 2, 9, -11},
	{0, 0, 0, 2, 2, 2, 0, 0, 0, -1, 0, -1, 0, 0, 1, 0, 0, 0, 0, 3, -6, 2, 7, -11},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, 1, 3, 3, 2, 2, 2, 2, 3, -5, 0, 5, -12},
	{0, 0, 0, 0, 0, 0, -1, -1, -1, 2, 3, 2, 0, 0, 1, 2, 2, 2, -1, 1, -5, -9, 0, -21},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 2, 1, 0, 0, 1, 4, -3, -8, 1, -20},
	{0, 0, 0, 1, 1, 1, -1, 1, 0, 1, 1, 0, 0, -1, 1, -2, -3, -2, -2, 1, -4, -3, 6, -15},
	{0, 0, 0, 2, 1, 1, 0, 1, 0, 0, 0, -1, 2, 1, 3, 1, 1, 1, -2, 0, -4, -3, 3, -16},
	{1, 0, 1, 1, 1, 1, 1, 2, 1, -2, -2, -1, -1, 0, 0, 1, 0, 1, 2, 4, -2, 10, 8, -4},
	{0, 0, 0, 0, 0, 0, 1, 1, 1, -1, 0, -1, 0, 0, 1, 0, 0, 0, 2, 3, -2, 9, 6, -5},
	{0, 0, 0, 0, 0, 0, 1, 1, 1, -2, 0, -1, 1, 0, 2, 0, 0, 0, 3, 4, -1, 9, 4, -6},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, 1, 0, 2, 2, 2, 2, -1, 0, -4, 9, 3, -7},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -2, -2, -2, -2, 0, 1, 2, 1, 1, 2, 3, -2, 8, 1, -8},
	{0, 0, 0, -1, -1, -1, 1, 1, 1, -1, 1, 0, 2, 0, 1, 0, 1, 1, 5, 6, 0, 1, -5, -13},
	{0, 0, 0, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, -4, 12, 5, -4},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, 0, -1, 0, -2, 0, -3, 13, 5, -4},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -3, -2, -2, 0, 0, 0, 1, 0, 0, 1, 3, 1, 15, 6, -3},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, -2, -2, -2, 2, 2, 2, 17, 6, -2},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 0, 0, 0, 0, 0, 0, -1, -1, -1, 5, 1, -11},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	};

static int rgb_offset_T_wqxga_revE[][RGB_COMPENSATION] = {
	/*	R255 G255 B255 R203 G203 B203 R151 G151 B151
		R87 G87 B87 R51 G51 B51 R35 G35 B35
		R23 G23 B23 R11 G11 B11
	*/
	{-7, 0, -2, -4, 0, -2, -4, 0, -2, -14, 1, -4, -23, 5, -11, -12, 4, -9, -9, 4, -9, -3, 7, -16},
	{-4, 0, -1, -3, 0, -1, -5, 0, -2, -9, 1, -3, -20, 4, -9, -15, 4, -10, -9, 4, -9, -3, 7, -16},
	{-2, 0, 0, -4, 0, -2, -3, 0, -1, -9, 2, -4, -17, 2, -5, -14, 4, -9, -13, 4, -12, -4, 7, -16},
	{-2, 0, 0, -2, 0, -1, -2, 0, -1, -8, 2, -4, -13, 1, -3, -17, 5, -10, -14, 5, -12, -4, 7, -16},
	{-1, 0, 0, -1, 0, -1, -2, 0, -1, -7, 1, -4, -13, 1, -2, -14, 4, -8, -16, 4, -12, -3, 7, -16},
	{-1, 0, 0, -1, 0, -1, -1, 0, 0, -5, 1, -3, -12, 0, -2, -14, 3, -8, -16, 3, -12, -7, 9, -19},
	{-1, 0, 0, 0, 0, 0, -2, 0, -1, -5, 1, -3, -10, 0, -2, -13, 3, -6, -18, 3, -14, -6, 10, -20},
	{0, 0, 0, -1, 0, 0, -1, 0, -1, -5, 1, -3, -10, 0, -2, -13, 2, -6, -17, 2, -13, -8, 9, -20},
	{0, 0, 0, -1, 0, 0, 0, 0, 0, -5, 1, -3, -9, 0, -2, -13, 2, -6, -19, 2, -14, -6, 11, -24},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -3, 1, -3, -8, 0, -1, -13, 2, -6, -18, 2, -14, -8, 12, -25},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -3, 1, -3, -8, 0, -1, -11, 2, -5, -18, 2, -14, -9, 9, -19},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -2, 1, -2, -6, 0, 0, -10, 2, -5, -19, 2, -14, -8, 11, -22},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -4, 1, -2, -6, 0, -1, -8, 2, -4, -18, 2, -13, -11, 9, -20},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 1, -2, -6, 0, -1, -9, 2, -5, -14, 2, -10, -11, 12, -26},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 1, -2, -4, 0, 0, -8, 2, -4, -16, 2, -13, -11, 9, -20},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 1, -2, -4, 0, 0, -8, 1, -4, -15, 1, -12, -12, 13, -27},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, -2, -4, 0, 0, -7, 1, -4, -14, 1, -13, -13, 13, -28},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, -2, -3, 0, 0, -7, 2, -4, -14, 2, -12, -14, 12, -26},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -3, 0, -2, -3, 0, 0, -6, 1, -3, -12, 1, -10, -14, 12, -25},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -2, 0, -2, -3, 0, 1, -8, 1, -4, -11, 1, -10, -15, 11, -24},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -2, 0, -2, -3, 0, 0, -7, 1, -3, -10, 1, -9, -17, 14, -30},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -2, 0, -2, -3, 0, 1, -5, 1, -3, -12, 1, -12, -15, 12, -25},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -2, 0, -2, -1, 0, 2, -5, 1, -3, -9, 1, -10, -18, 14, -29},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, -1, -2, 0, 1, -4, 1, -3, -11, 1, -12, -14, 12, -24},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, -1, -2, 0, 1, -3, 1, -2, -10, 1, -12, -15, 11, -23},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, -1, -1, 0, 1, -3, 1, -2, -8, 1, -10, -16, 13, -28},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, -1, -1, 0, 1, -3, 0, -2, -8, 0, -10, -15, 12, -26},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, -1, 0, 0, 1, -4, 1, -2, -5, 1, -9, -17, 14, -30},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, -2, -1, 0, 0, -4, 0, -2, -5, 0, -9, -16, 14, -28},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, -2, -1, 0, 0, -4, 0, -2, -5, 0, -11, -14, 12, -24},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, -2, -1, 0, 0, -3, 0, -2, -4, 0, -10, -17, 14, -30},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, -2, 0, 0, 1, -3, 0, -2, -3, 0, -10, -15, 12, -26},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, -2, 0, 0, 1, -2, 1, -2, -3, 1, -9, -15, 14, -30},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, -2, 0, 0, 1, -2, 1, -2, -2, 1, -11, -14, 12, -26},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, -2, 0, 0, 1, -3, 0, -2, -2, 0, -11, -12, 12, -24},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, -2, 0, 0, 1, -2, 0, -2, -1, 0, -10, -11, 12, -26},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -2, 0, 0, -10, -8, 11, -23},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, -1, 0, -10, -5, 9, -20},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -12, -2, 8, -16},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, -2, 1, 1, -10, 1, 8, -16},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, -1, 1, 0, 2, 0, 0, -2, 0, 0, -9, 0, 8, -18},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, -2, 0, 0, 2, 0, 0, 0, 1, 0, -10, 0, 7, -16},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, -1, 1, 0, 1, 0, 0, -2, 0, 0, -8, 0, 8, -17},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2, 0, 0, -1, 0, 0, -8, 0, 8, -17},
	{0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 2, 0, 0, 0, 1, 0, -7, 0, 8, -17},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 1, 1, 0, 0, 0, 0, -7, 1, 7, -15},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, -7, 0, 7, -16},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -6, 0, 7, -16},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, -1, -1, 0, -5, 1, 7, -15},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, -5, 1, 7, -15},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, -4, 0, 7, -16},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, -3, 1, 7, -14},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 1, 6, -14},
	{0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 0, -4, 1, 6, -13},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, -4, 3, 5, -12},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 8, 4, -9},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 10, 4, -8},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 10, 3, -8},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	};

static int rgb_offset_T_wqxga_ym4[][RGB_COMPENSATION] = {
/*	R255 G255 B255 R203 G203 B203 R151 G151 B151
	R87 G87 B87 R51 G51 B51 R35 G35 B35
	R23 G23 B23 R11 G11 B11
*/
	{-14, 2, -12, -9, 2, -8, -14, 2, -11, -23, 7, -14, -33, 9, -18, -28, 5, -12, -36, 5, -12, -27, 5, -10},
	{-9, 1, -8, -7, 1, -6, -11, 2, -9, -21, 6, -14, -31, 8, -18, -24, 6, -13, -36, 6, -12, -27, 5, -10},
	{-7, 1, -6, -5, 1, -5, -9, 1, -8, -19, 6, -14, -28, 7, -16, -25, 6, -14, -36, 6, -13, -37, 6, -14},
	{-4, 1, -4, -5, 1, -4, -9, 1, -7, -16, 5, -12, -26, 7, -16, -26, 7, -15, -35, 7, -14, -44, 7, -16},
	{-4, 0, -4, -5, 0, -4, -7, 1, -6, -15, 5, -12, -25, 7, -16, -24, 6, -13, -32, 6, -14, -41, 9, -18},
	{-4, 0, -4, -4, 0, -3, -8, 1, -7, -13, 5, -10, -22, 6, -14, -24, 7, -14, -34, 7, -16, -40, 9, -19},
	{-3, 0, -3, -3, 0, -3, -8, 1, -6, -12, 5, -10, -21, 7, -14, -24, 6, -14, -34, 6, -16, -40, 10, -22},
	{-3, 0, -3, -3, 0, -3, -6, 1, -5, -11, 4, -9, -21, 6, -14, -23, 6, -12, -31, 6, -16, -46, 11, -23},
	{-1, 0, -2, -4, 0, -3, -5, 1, -5, -11, 4, -9, -21, 6, -14, -21, 5, -12, -33, 5, -18, -46, 10, -22},
	{-2, 0, -3, -2, 0, -2, -4, 1, -4, -11, 4, -9, -18, 6, -12, -23, 6, -13, -28, 6, -16, -44, 12, -24},
	{-2, 0, -2, -2, 0, -2, -4, 1, -4, -10, 4, -8, -17, 6, -12, -22, 5, -12, -28, 5, -16, -44, 12, -24},
	{-2, 0, -2, -1, 0, -2, -5, 0, -4, -10, 3, -8, -17, 5, -12, -22, 5, -12, -26, 5, -15, -47, 13, -28},
	{-2, 0, -2, 0, 0, -1, -5, 0, -4, -10, 3, -8, -17, 5, -12, -20, 5, -11, -28, 5, -17, -43, 12, -24},
	{-1, 0, -2, -1, 0, -1, -4, 0, -3, -8, 3, -7, -17, 5, -12, -21, 5, -12, -26, 5, -16, -46, 12, -26},
	{-1, 0, -2, -1, 0, -1, -4, 0, -3, -8, 3, -7, -17, 6, -12, -17, 4, -10, -30, 4, -18, -43, 11, -24},
	{-1, 0, -2, -3, 0, -3, -2, 0, -1, -7, 3, -6, -17, 5, -12, -16, 4, -10, -27, 4, -16, -49, 13, -27},
	{-1, 0, -1, -3, 0, -3, -1, 0, 0, -7, 3, -6, -14, 5, -10, -16, 5, -10, -24, 5, -14, -50, 13, -28},
	{-1, 0, -1, -2, 0, -3, -2, 0, 0, -7, 2, -6, -14, 4, -10, -17, 5, -11, -27, 5, -16, -46, 12, -26},
	{0, 0, -1, -3, 0, -3, -1, 0, 0, -6, 2, -5, -14, 5, -10, -15, 4, -10, -26, 4, -15, -48, 13, -28},
	{0, 0, -1, -2, 0, -2, -1, 0, 0, -6, 2, -5, -14, 4, -10, -16, 4, -10, -23, 4, -13, -48, 13, -28},
	{0, 0, -1, -2, 0, -2, -1, 0, 0, -5, 2, -4, -14, 4, -10, -14, 4, -9, -25, 4, -14, -47, 14, -29},
	{0, 0, -1, -2, 0, -2, -1, 0, 0, -5, 1, -4, -14, 4, -10, -14, 4, -9, -24, 4, -14, -46, 13, -28},
	{0, 0, -1, -1, 0, -2, -2, 0, 0, -4, 2, -4, -12, 4, -9, -12, 4, -8, -23, 4, -13, -47, 13, -28},
	{0, 0, 0, -1, 0, -2, -2, 0, -1, -4, 2, -4, -11, 4, -8, -12, 4, -8, -24, 4, -14, -43, 12, -26},
	{0, 0, 0, -1, 0, -2, -2, 0, -1, -4, 2, -4, -11, 3, -8, -12, 4, -8, -24, 4, -14, -38, 12, -24},
	{0, 0, 0, -1, 0, -2, -2, 0, -3, -4, 1, -2, -11, 3, -8, -12, 4, -8, -23, 4, -12, -43, 13, -27},
	{0, 0, 0, -1, 0, -1, -2, 0, -3, -4, 1, -3, -10, 3, -8, -12, 3, -8, -21, 3, -11, -42, 12, -26},
	{0, 0, 0, 0, 0, -1, -2, 0, -3, -5, 1, -2, -9, 3, -7, -11, 3, -8, -19, 3, -11, -42, 12, -26},
	{0, 0, 0, 0, 0, -1, -2, 0, -3, -5, 0, -2, -9, 3, -7, -11, 3, -8, -17, 3, -10, -42, 12, -25},
	{0, 0, 0, 0, 0, -1, -2, 0, -3, -4, 0, -1, -9, 3, -7, -9, 3, -7, -18, 3, -10, -38, 11, -24},
	{0, 0, 0, 0, 0, -1, -2, 0, -2, -4, 0, -2, -7, 3, -6, -9, 3, -6, -18, 3, -10, -40, 12, -25},
	{0, 0, 0, 0, 0, -1, -1, 0, -2, -5, 0, -2, -8, 2, -6, -8, 2, -6, -17, 2, -9, -37, 11, -24},
	{0, 0, 0, 0, 0, -1, -1, 0, -2, -4, 0, -1, -7, 2, -6, -9, 2, -6, -16, 2, -8, -40, 12, -25},
	{0, 0, 0, 0, 0, -1, -1, 0, -2, -4, 0, 0, -6, 2, -5, -7, 2, -6, -16, 2, -8, -34, 11, -24},
	{0, 0, 0, 0, 0, -1, -1, 0, -2, -3, 0, 0, -6, 2, -4, -7, 2, -6, -14, 2, -7, -35, 11, -24},
	{0, 0, 0, 0, 0, -1, -1, 0, -2, -3, 0, 0, -5, 2, -4, -6, 2, -5, -13, 2, -6, -33, 10, -22},
	{0, 0, 0, 0, 0, -1, 0, 0, -1, -4, 0, -1, -5, 2, -4, -5, 1, -4, -13, 1, -7, -31, 9, -20},
	{0, 0, 0, 0, 0, -1, -1, 0, -1, -2, 0, 0, -5, 2, -4, -4, 1, -4, -12, 1, -6, -27, 11, -24},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, -5, 1, -4, -4, 1, -3, -10, 1, -4, -23, 10, -22},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, -5, 1, -4, -3, 1, -3, -8, 1, -4, -18, 9, -19},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, -4, 1, -3, -4, 1, -4, -9, 1, -4, -20, 10, -20},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, -4, 1, -3, -3, 1, -3, -9, 1, -4, -21, 9, -20},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -2, 0, 0, -4, 1, -2, -2, 1, -3, -9, 1, -3, -19, 8, -18},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -2, 0, 0, -4, 1, -3, -2, 1, -3, -8, 1, -2, -20, 9, -18},
	{0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, -2, -4, 0, -2, -3, 1, -3, -9, 1, -3, -21, 9, -18},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, -2, -3, 0, -1, -1, 1, -3, -8, 1, -2, -20, 8, -17},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, -1, 1, -2, -2, 1, -3, -7, 1, -1, -19, 6, -13},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, -3, 0, 0, -1, 1, -2, -8, 1, -1, -18, 6, -13},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, -3, 0, -2, -1, 1, -2, -8, 1, -2, -19, 6, -12},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, -2, 0, -1, -2, 1, -2, -6, 1, -2, -19, 5, -12},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, -3, 0, -1, -1, 1, -2, -6, 1, -2, -19, 5, -12},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, -2, 0, 0, -1, 1, -2, -5, 1, -1, -19, 5, -11},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, -1, -2, 1, -2, -5, 1, -1, -18, 5, -10},
	{1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, -2, -3, 0, -2, -6, 0, -2, -17, 4, -10},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, -1, -2, 0, -2, -5, 0, -2, -13, 3, -8},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 0, -1, 0, 0, -1, -4, 0, -1, -13, 3, -8},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, -1, -3, 0, 0, -11, 3, -7},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, -1, -3, 0, 0, -8, 2, -6},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static void gamma_init_T_revA(struct SMART_DIM *pSmart, char *str, int size)
{
	long long candela_level[S6E3_TABLE_MAX] = {-1, };
	int bl_index[S6E3_TABLE_MAX] = {-1, };
	int gamma_setting[GAMMA_SET_MAX];

	long long temp_cal_data = 0;
	int bl_level = 0;

	int level_255_temp_MSB = 0;
	int level_V255 = 0;

	int point_index;
	int cnt;
	int table_index;

	pr_debug("%s : start !!\n",__func__);

	/*calculate candela level */
	if ((pSmart->brightness_level <= 350) &&
		(pSmart->brightness_level >= 249)) {
		/* 265CD ~ 350CD */
		if (pSmart->brightness_level == 350)
			bl_level = 350;
		else if (pSmart->brightness_level == 333)
			bl_level = 337;
		else if (pSmart->brightness_level == 316)
			bl_level = 320;
		else if (pSmart->brightness_level == 300)
			bl_level = 303;
		else if (pSmart->brightness_level == 282)
			bl_level = 286;
		else if (pSmart->brightness_level == 265)
			bl_level = 269;
		else if (pSmart->brightness_level == 249)
			bl_level = 252;
	} else if ((pSmart->brightness_level < 249) &&
				(pSmart->brightness_level >= 183)) {
		/* 183CD ~ 249CD */
		bl_level = 249;
	} else if ((pSmart->brightness_level < 183) &&
				(pSmart->brightness_level >= 72)) {
		/* 77CD ~ 172CD */
		if (pSmart->brightness_level == 72)
			bl_level = 108;
		else if (pSmart->brightness_level == 77)
			bl_level = 116;
		else if (pSmart->brightness_level == 82)
			bl_level = 122;
		else if (pSmart->brightness_level == 87)
			bl_level = 130;
		else if (pSmart->brightness_level == 93)
			bl_level = 138;
		else if (pSmart->brightness_level == 98)
			bl_level = 144;
		else if (pSmart->brightness_level == 105)
			bl_level = 153;
		else if (pSmart->brightness_level == 111)
			bl_level = 160;
		else if (pSmart->brightness_level == 119)
			bl_level = 173;
		else if (pSmart->brightness_level == 126)
			bl_level = 181;
		else if (pSmart->brightness_level == 134)
			bl_level = 191;
		else if (pSmart->brightness_level == 143)
			bl_level = 202;
		else if (pSmart->brightness_level == 152)
			bl_level = 214;
		else if (pSmart->brightness_level == 162)
			bl_level = 227;
		else if (pSmart->brightness_level == 172)
			bl_level = 240;
	} else {
		/* 68CD ~ 2CD */
		bl_level = AOR_DIM_BASE_CD_REVA_WQXGA;
	}

	if (pSmart->brightness_level < 350) {
		for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
			point_index = S6E3_ARRAY[cnt+1];
			temp_cal_data =
			((long long)(candela_coeff_2p15[point_index])) *
			((long long)(bl_level));
			candela_level[cnt] = temp_cal_data;
		}

	} else {
		for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
			point_index = S6E3_ARRAY[cnt+1];
			temp_cal_data =
			((long long)(candela_coeff_2p2[point_index])) *
			((long long)(bl_level));
			candela_level[cnt] = temp_cal_data;
		}

	}

#if 1 //def SMART_DIMMING_DEBUG
	printk(KERN_INFO "\n candela_1:%llu  candela_3:%llu  candela_11:%llu ",
		candela_level[0], candela_level[1], candela_level[2]);
	printk(KERN_INFO "candela_23:%llu  candela_35:%llu  candela_51:%llu ",
		candela_level[3], candela_level[4], candela_level[5]);
	printk(KERN_INFO "candela_87:%llu  candela_151:%llu  candela_203:%llu ",
		candela_level[6], candela_level[7], candela_level[8]);
	printk(KERN_INFO "candela_255:%llu brightness_level %d\n", candela_level[9], pSmart->brightness_level);
#endif

	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
		if (searching_function(candela_level[cnt],
			&(bl_index[cnt]), GAMMA_CURVE_2P2)) {
			pr_info("%s searching functioin error cnt:%d\n",
			__func__, cnt);
		}
	}

	/*
	*	Candela compensation
	*/
	for (cnt = 1; cnt < S6E3_TABLE_MAX; cnt++) {
		table_index = find_cadela_table(pSmart->brightness_level);

		if (table_index == -1) {
			table_index = CCG6_MAX_TABLE;
			pr_info("%s fail candela table_index cnt : %d brightness %d",
				__func__, cnt, pSmart->brightness_level);
		}

		bl_index[S6E3_TABLE_MAX - cnt] +=
			gradation_offset_T_wqxga_revA[table_index][cnt - 1];

		/* THERE IS M-GRAY0 target */
		if (bl_index[S6E3_TABLE_MAX - cnt] == 0)
			bl_index[S6E3_TABLE_MAX - cnt] = 1;
	}

#ifdef SMART_DIMMING_DEBUG
	printk(KERN_INFO "\n bl_index_1:%d  bl_index_3:%d  bl_index_11:%d",
		bl_index[0], bl_index[1], bl_index[2]);
	printk(KERN_INFO "bl_index_23:%d bl_index_35:%d  bl_index_51:%d",
		bl_index[3], bl_index[4], bl_index[5]);
	printk(KERN_INFO "bl_index_87:%d  bl_index_151:%d bl_index_203:%d",
		bl_index[6], bl_index[7], bl_index[8]);
	printk(KERN_INFO "bl_index_255:%d\n", bl_index[9]);
#endif
	/*Generate Gamma table*/
	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++)
		(void)Make_hexa[cnt](bl_index , pSmart, str);

	/* To avoid overflow */
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
		gamma_setting[cnt] = str[cnt];

	/*
	*	RGB compensation
	*/
	for (cnt = 0; cnt < RGB_COMPENSATION; cnt++) {
		table_index = find_cadela_table(pSmart->brightness_level);

		if (table_index == -1) {
			table_index = CCG6_MAX_TABLE;
			pr_info("%s fail RGB table_index cnt : %d brightness %d",
				__func__, cnt, pSmart->brightness_level);
		}

		if (cnt < 3) {
			level_V255 = gamma_setting[cnt * 2] << 8 | gamma_setting[(cnt * 2) + 1];
			level_V255 +=
				rgb_offset_T_wqxga_revA[table_index][cnt];
			level_255_temp_MSB = level_V255 / 256;

			gamma_setting[cnt * 2] = level_255_temp_MSB & 0xff;
			gamma_setting[(cnt * 2) + 1] = level_V255 & 0xff;
		} else {
			gamma_setting[cnt+3] += rgb_offset_T_wqxga_revA[table_index][cnt];
		}
	}
	/*subtration MTP_OFFSET value from generated gamma table*/
	mtp_offset_substraction(pSmart, gamma_setting);

	/* To avoid overflow */
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
		str[cnt] = gamma_setting[cnt];
}

static void gamma_init_T_revD(struct SMART_DIM *pSmart, char *str, int size)
{
	long long candela_level[S6E3_TABLE_MAX] = {-1, };
	int bl_index[S6E3_TABLE_MAX] = {-1, };
	int gamma_setting[GAMMA_SET_MAX];

	long long temp_cal_data = 0;
	int bl_level = 0;

	int level_255_temp_MSB = 0;
	int level_V255 = 0;

	int point_index;
	int cnt;
	int table_index;

	pr_debug("%s : start !!\n",__func__);

	/*calculate candela level */
	if ((pSmart->brightness_level <= 350) &&
		(pSmart->brightness_level >= 265)) {
		/* 265CD ~ 350CD */
		if (pSmart->brightness_level == 350)
			bl_level = 350;
		else if (pSmart->brightness_level == 333)
			bl_level = 334;
		else if (pSmart->brightness_level == 316)
			bl_level = 315;
		else if (pSmart->brightness_level == 300)
			bl_level = 300;
		else if (pSmart->brightness_level == 282)
			bl_level = 283;
		else if (pSmart->brightness_level == 265)
			bl_level = 268;
		else if (pSmart->brightness_level == 249)
			bl_level = 252;
	} else if ((pSmart->brightness_level < 265) &&
		(pSmart->brightness_level >= 183)) {
		/* 183CD ~ 265CD */
		bl_level = 252;
	} else if ((pSmart->brightness_level < 183) &&
				(pSmart->brightness_level > 77)) {
		/* 82CD ~ 172CD */
		if (pSmart->brightness_level == 82)
			bl_level = 120;
		else if (pSmart->brightness_level == 87)
			bl_level = 129;
		else if (pSmart->brightness_level == 93)
			bl_level = 137;
		else if (pSmart->brightness_level == 98)
			bl_level = 143;
		else if (pSmart->brightness_level == 105)
			bl_level = 153;
		else if (pSmart->brightness_level == 111)
			bl_level = 162;
		else if (pSmart->brightness_level == 119)
			bl_level = 171;
		else if (pSmart->brightness_level == 126)
			bl_level = 179;
		else if (pSmart->brightness_level == 134)
			bl_level = 190;
		else if (pSmart->brightness_level == 143)
			bl_level = 202;
		else if (pSmart->brightness_level == 152)
			bl_level = 213;
		else if (pSmart->brightness_level == 162)
			bl_level = 228;
		else if (pSmart->brightness_level == 172)
			bl_level = 238;
	} else {
		/* 77CD ~ 2CD */
		bl_level = AOR_DIM_BASE_CD_REVD_WQXGA;
	}

	if (pSmart->brightness_level < 350) {
		for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
			point_index = S6E3_ARRAY[cnt+1];
			temp_cal_data =
			((long long)(candela_coeff_2p15[point_index])) *
			((long long)(bl_level));
			candela_level[cnt] = temp_cal_data;
		}

	} else {
		for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
			point_index = S6E3_ARRAY[cnt+1];
			temp_cal_data =
			((long long)(candela_coeff_2p2[point_index])) *
			((long long)(bl_level));
			candela_level[cnt] = temp_cal_data;
		}

	}

#if 1//def SMART_DIMMING_DEBUG
	printk(KERN_INFO "\n candela_1:%llu  candela_3:%llu  candela_11:%llu ",
		candela_level[0], candela_level[1], candela_level[2]);
	printk(KERN_INFO "candela_23:%llu  candela_35:%llu  candela_51:%llu ",
		candela_level[3], candela_level[4], candela_level[5]);
	printk(KERN_INFO "candela_87:%llu  candela_151:%llu  candela_203:%llu ",
		candela_level[6], candela_level[7], candela_level[8]);
	printk(KERN_INFO "candela_255:%llu brightness_level %d\n", candela_level[9], pSmart->brightness_level);
#endif

	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
		if (searching_function_360(candela_level[cnt],
//			if (searching_function(candela_level[cnt],
			&(bl_index[cnt]), GAMMA_CURVE_2P2)) {
			pr_info("%s searching functioin error cnt:%d\n",
			__func__, cnt);
		}
	}

	/*
	*	Candela compensation
	*/
	for (cnt = 1; cnt < S6E3_TABLE_MAX; cnt++) {
		table_index = find_cadela_table(pSmart->brightness_level);

		if (table_index == -1) {
			table_index = CCG6_MAX_TABLE;
			pr_info("%s fail candela table_index cnt : %d brightness %d",
				__func__, cnt, pSmart->brightness_level);
		}

		bl_index[S6E3_TABLE_MAX - cnt] +=
			gradation_offset_T_wqxga_revD[table_index][cnt - 1];

		/* THERE IS M-GRAY0 target */
		if (bl_index[S6E3_TABLE_MAX - cnt] == 0)
			bl_index[S6E3_TABLE_MAX - cnt] = 1;
	}

#if 1//def SMART_DIMMING_DEBUG
	printk(KERN_INFO "\n bl_index_1:%d  bl_index_3:%d  bl_index_11:%d",
		bl_index[0], bl_index[1], bl_index[2]);
	printk(KERN_INFO "bl_index_23:%d bl_index_35:%d  bl_index_51:%d",
		bl_index[3], bl_index[4], bl_index[5]);
	printk(KERN_INFO "bl_index_87:%d  bl_index_151:%d bl_index_203:%d",
		bl_index[6], bl_index[7], bl_index[8]);
	printk(KERN_INFO "bl_index_255:%d\n", bl_index[9]);
#endif
	/*Generate Gamma table*/
	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++)
		(void)Make_hexa[cnt](bl_index , pSmart, str);

	/* To avoid overflow */
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
		gamma_setting[cnt] = str[cnt];

	/*
	*	RGB compensation
	*/
	for (cnt = 0; cnt < RGB_COMPENSATION; cnt++) {
		table_index = find_cadela_table(pSmart->brightness_level);

		if (table_index == -1) {
			table_index = CCG6_MAX_TABLE;
			pr_info("%s fail RGB table_index cnt : %d brightness %d",
				__func__, cnt, pSmart->brightness_level);
		}

		if (cnt < 3) {
			level_V255 = gamma_setting[cnt * 2] << 8 | gamma_setting[(cnt * 2) + 1];
			level_V255 +=
				rgb_offset_T_wqxga_revD[table_index][cnt];
			level_255_temp_MSB = level_V255 / 256;

			gamma_setting[cnt * 2] = level_255_temp_MSB & 0xff;
			gamma_setting[(cnt * 2) + 1] = level_V255 & 0xff;
		} else {
			gamma_setting[cnt+3] += rgb_offset_T_wqxga_revD[table_index][cnt];
		}
	}
	/*subtration MTP_OFFSET value from generated gamma table*/
	mtp_offset_substraction(pSmart, gamma_setting);

	/* To avoid overflow */
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
		str[cnt] = gamma_setting[cnt];
}

static void gamma_init_T_revE(struct SMART_DIM *pSmart, char *str, int size)
{
	long long candela_level[S6E3_TABLE_MAX] = {-1, };
	int bl_index[S6E3_TABLE_MAX] = {-1, };
	int gamma_setting[GAMMA_SET_MAX];

	long long temp_cal_data = 0;
	int bl_level = 0;

	int level_255_temp_MSB = 0;
	int level_V255 = 0;

	int point_index;
	int cnt;
	int table_index;

	pr_debug("%s : start !!\n",__func__);

	/*calculate candela level */
	if ((pSmart->brightness_level <= 350) &&
		(pSmart->brightness_level >= 265)) {
		/* 265CD ~ 350CD */
		if (pSmart->brightness_level == 350)
			bl_level = 350;
		else if (pSmart->brightness_level == 333)
			bl_level = 334;
		else if (pSmart->brightness_level == 316)
			bl_level = 315;
		else if (pSmart->brightness_level == 300)
			bl_level = 300;
		else if (pSmart->brightness_level == 282)
			bl_level = 283;
		else if (pSmart->brightness_level == 265)
			bl_level = 268;
		else if (pSmart->brightness_level == 249)
			bl_level = 252;
	} else if ((pSmart->brightness_level < 265) &&
		(pSmart->brightness_level >= 183)) {
		/* 183CD ~ 265CD */
		bl_level = 252;
	} else if ((pSmart->brightness_level < 183) &&
				(pSmart->brightness_level > 77)) {
		/* 82CD ~ 172CD */
		if (pSmart->brightness_level == 82)
			bl_level = 120;
		else if (pSmart->brightness_level == 87)
			bl_level = 129;
		else if (pSmart->brightness_level == 93)
			bl_level = 137;
		else if (pSmart->brightness_level == 98)
			bl_level = 143;
		else if (pSmart->brightness_level == 105)
			bl_level = 153;
		else if (pSmart->brightness_level == 111)
			bl_level = 162;
		else if (pSmart->brightness_level == 119)
			bl_level = 171;
		else if (pSmart->brightness_level == 126)
			bl_level = 179;
		else if (pSmart->brightness_level == 134)
			bl_level = 190;
		else if (pSmart->brightness_level == 143)
			bl_level = 202;
		else if (pSmart->brightness_level == 152)
			bl_level = 213;
		else if (pSmart->brightness_level == 162)
			bl_level = 228;
		else if (pSmart->brightness_level == 172)
			bl_level = 238;
	} else {
		/* 77CD ~ 2CD */
		bl_level = AOR_DIM_BASE_CD_REVD_WQXGA;
	}

	if (pSmart->brightness_level < 350) {
		for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
			point_index = S6E3_ARRAY[cnt+1];
			temp_cal_data =
			((long long)(candela_coeff_2p15[point_index])) *
			((long long)(bl_level));
			candela_level[cnt] = temp_cal_data;
		}

	} else {
		for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
			point_index = S6E3_ARRAY[cnt+1];
			temp_cal_data =
			((long long)(candela_coeff_2p2[point_index])) *
			((long long)(bl_level));
			candela_level[cnt] = temp_cal_data;
		}

	}

#if 1//def SMART_DIMMING_DEBUG
	printk(KERN_INFO "\n candela_1:%llu  candela_3:%llu  candela_11:%llu ",
		candela_level[0], candela_level[1], candela_level[2]);
	printk(KERN_INFO "candela_23:%llu  candela_35:%llu  candela_51:%llu ",
		candela_level[3], candela_level[4], candela_level[5]);
	printk(KERN_INFO "candela_87:%llu  candela_151:%llu  candela_203:%llu ",
		candela_level[6], candela_level[7], candela_level[8]);
	printk(KERN_INFO "candela_255:%llu brightness_level %d\n", candela_level[9], pSmart->brightness_level);
#endif

	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
		if (searching_function_360(candela_level[cnt],
//			if (searching_function(candela_level[cnt],
			&(bl_index[cnt]), GAMMA_CURVE_2P2)) {
			pr_info("%s searching functioin error cnt:%d\n",
			__func__, cnt);
		}
	}

	/*
	*	Candela compensation
	*/
	for (cnt = 1; cnt < S6E3_TABLE_MAX; cnt++) {
		table_index = find_cadela_table(pSmart->brightness_level);

		if (table_index == -1) {
			table_index = CCG6_MAX_TABLE;
			pr_info("%s fail candela table_index cnt : %d brightness %d",
				__func__, cnt, pSmart->brightness_level);
		}

		bl_index[S6E3_TABLE_MAX - cnt] +=
			gradation_offset_T_wqxga_revE[table_index][cnt - 1];

		/* THERE IS M-GRAY0 target */
		if (bl_index[S6E3_TABLE_MAX - cnt] == 0)
			bl_index[S6E3_TABLE_MAX - cnt] = 1;
	}

#if 1//def SMART_DIMMING_DEBUG
	printk(KERN_INFO "\n bl_index_1:%d  bl_index_3:%d  bl_index_11:%d",
		bl_index[0], bl_index[1], bl_index[2]);
	printk(KERN_INFO "bl_index_23:%d bl_index_35:%d  bl_index_51:%d",
		bl_index[3], bl_index[4], bl_index[5]);
	printk(KERN_INFO "bl_index_87:%d  bl_index_151:%d bl_index_203:%d",
		bl_index[6], bl_index[7], bl_index[8]);
	printk(KERN_INFO "bl_index_255:%d\n", bl_index[9]);
#endif
	/*Generate Gamma table*/
	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++)
		(void)Make_hexa[cnt](bl_index , pSmart, str);

	/* To avoid overflow */
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
		gamma_setting[cnt] = str[cnt];

	/*
	*	RGB compensation
	*/
	for (cnt = 0; cnt < RGB_COMPENSATION; cnt++) {
		table_index = find_cadela_table(pSmart->brightness_level);

		if (table_index == -1) {
			table_index = CCG6_MAX_TABLE;
			pr_info("%s fail RGB table_index cnt : %d brightness %d",
				__func__, cnt, pSmart->brightness_level);
		}

		if (cnt < 3) {
			level_V255 = gamma_setting[cnt * 2] << 8 | gamma_setting[(cnt * 2) + 1];
			level_V255 +=
				rgb_offset_T_wqxga_revE[table_index][cnt];
			level_255_temp_MSB = level_V255 / 256;

			gamma_setting[cnt * 2] = level_255_temp_MSB & 0xff;
			gamma_setting[(cnt * 2) + 1] = level_V255 & 0xff;
		} else {
			gamma_setting[cnt+3] += rgb_offset_T_wqxga_revE[table_index][cnt];
		}
	}
	/*subtration MTP_OFFSET value from generated gamma table*/
	mtp_offset_substraction(pSmart, gamma_setting);

	/* To avoid overflow */
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
		str[cnt] = gamma_setting[cnt];
}

static void gamma_init_T_YM4(struct SMART_DIM *pSmart, char *str, int size)
{
	long long candela_level[S6E3_TABLE_MAX] = {-1, };
	int bl_index[S6E3_TABLE_MAX] = {-1, };
	int gamma_setting[GAMMA_SET_MAX];

	long long temp_cal_data = 0;
	int bl_level = 0;

	int level_255_temp_MSB = 0;
	int level_V255 = 0;

	int point_index;
	int cnt;
	int table_index;

	pr_debug("%s : start !!\n",__func__);

	/*calculate candela level */
	if ((pSmart->brightness_level <= 350) &&
		(pSmart->brightness_level >= 265)) {
		/* 265CD ~ 350CD */
		if (pSmart->brightness_level == 350)
			bl_level = 350;
		else if (pSmart->brightness_level == 333)
			bl_level = 336;
		else if (pSmart->brightness_level == 316)
			bl_level = 318;
		else if (pSmart->brightness_level == 300)
			bl_level = 304;
		else if (pSmart->brightness_level == 282)
			bl_level = 283;
		else if (pSmart->brightness_level == 265)
			bl_level = 269;
	} else if ((pSmart->brightness_level < 265) &&
				(pSmart->brightness_level >= 183)) {
		/* 183CD ~ 265CD */
		bl_level = 252;
	} else if ((pSmart->brightness_level < 183) &&
				(pSmart->brightness_level >= 82)) {
		/* 82CD ~ 183CD */
		if (pSmart->brightness_level == 82)
			bl_level = 120;
		else if (pSmart->brightness_level == 87)
			bl_level = 127;
		else if (pSmart->brightness_level == 93)
			bl_level = 135;
		else if (pSmart->brightness_level == 98)
			bl_level = 142;
		else if (pSmart->brightness_level == 105)
			bl_level = 151;
		else if (pSmart->brightness_level == 111)
			bl_level = 160;
		else if (pSmart->brightness_level == 119)
			bl_level = 171;
		else if (pSmart->brightness_level == 126)
			bl_level = 182;
		else if (pSmart->brightness_level == 134)
			bl_level = 189;
		else if (pSmart->brightness_level == 143)
			bl_level = 201;
		else if (pSmart->brightness_level == 152)
			bl_level = 215;
		else if (pSmart->brightness_level == 162)
			bl_level = 224;
		else if (pSmart->brightness_level == 172)
			bl_level = 239;
	} else {
		/* 77CD ~ 2CD */
		bl_level = AOR_DIM_BASE_CD_REVB_WQXGA;
	}

	if (pSmart->brightness_level < 350) {
		for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
			point_index = S6E3_ARRAY[cnt+1];
			temp_cal_data =
			((long long)(candela_coeff_2p15[point_index])) *
			((long long)(bl_level));
			candela_level[cnt] = temp_cal_data;
		}

	} else {
		for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
			point_index = S6E3_ARRAY[cnt+1];
			temp_cal_data =
			((long long)(candela_coeff_2p2[point_index])) *
			((long long)(bl_level));
			candela_level[cnt] = temp_cal_data;
		}

	}

#if 1 //def SMART_DIMMING_DEBUG
	printk(KERN_INFO "\n candela_1:%llu  candela_3:%llu  candela_11:%llu ",
		candela_level[0], candela_level[1], candela_level[2]);
	printk(KERN_INFO "candela_23:%llu  candela_35:%llu  candela_51:%llu ",
		candela_level[3], candela_level[4], candela_level[5]);
	printk(KERN_INFO "candela_87:%llu  candela_151:%llu  candela_203:%llu ",
		candela_level[6], candela_level[7], candela_level[8]);
	printk(KERN_INFO "candela_255:%llu brightness_level %d\n", candela_level[9], pSmart->brightness_level);
#endif

	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
		if (searching_function(candela_level[cnt],
			&(bl_index[cnt]), GAMMA_CURVE_2P2)) {
			pr_info("%s searching functioin error cnt:%d\n",
			__func__, cnt);
		}
	}

	/*
	*	Candela compensation
	*/
	for (cnt = 1; cnt < S6E3_TABLE_MAX; cnt++) {
		table_index = find_cadela_table(pSmart->brightness_level);

		if (table_index == -1) {
			table_index = CCG6_MAX_TABLE;
			pr_info("%s fail candela table_index cnt : %d brightness %d",
				__func__, cnt, pSmart->brightness_level);
		}

		bl_index[S6E3_TABLE_MAX - cnt] +=
			gradation_offset_T_wqxga_ym4[table_index][cnt - 1];

		/* THERE IS M-GRAY0 target */
		if (bl_index[S6E3_TABLE_MAX - cnt] == 0)
			bl_index[S6E3_TABLE_MAX - cnt] = 1;
	}

#ifdef SMART_DIMMING_DEBUG
	printk(KERN_INFO "\n bl_index_1:%d  bl_index_3:%d  bl_index_11:%d",
		bl_index[0], bl_index[1], bl_index[2]);
	printk(KERN_INFO "bl_index_23:%d bl_index_35:%d  bl_index_51:%d",
		bl_index[3], bl_index[4], bl_index[5]);
	printk(KERN_INFO "bl_index_87:%d  bl_index_151:%d bl_index_203:%d",
		bl_index[6], bl_index[7], bl_index[8]);
	printk(KERN_INFO "bl_index_255:%d\n", bl_index[9]);
#endif
	/*Generate Gamma table*/
	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++)
		(void)Make_hexa[cnt](bl_index , pSmart, str);

	/* To avoid overflow */
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
		gamma_setting[cnt] = str[cnt];

	/*
	*	RGB compensation
	*/
	for (cnt = 0; cnt < RGB_COMPENSATION; cnt++) {
		table_index = find_cadela_table(pSmart->brightness_level);

		if (table_index == -1) {
			table_index = CCG6_MAX_TABLE;
			pr_info("%s fail RGB table_index cnt : %d brightness %d",
				__func__, cnt, pSmart->brightness_level);
		}

		if (cnt < 3) {
			level_V255 = gamma_setting[cnt * 2] << 8 | gamma_setting[(cnt * 2) + 1];
			level_V255 +=
				rgb_offset_T_wqxga_ym4[table_index][cnt];
			level_255_temp_MSB = level_V255 / 256;

			gamma_setting[cnt * 2] = level_255_temp_MSB & 0xff;
			gamma_setting[(cnt * 2) + 1] = level_V255 & 0xff;
		} else {
			gamma_setting[cnt+3] += rgb_offset_T_wqxga_ym4[table_index][cnt];
		}
	}
	/*subtration MTP_OFFSET value from generated gamma table*/
	mtp_offset_substraction(pSmart, gamma_setting);

	/* To avoid overflow */
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
		str[cnt] = gamma_setting[cnt];
}


#endif
#endif

static void pure_gamma_init(struct SMART_DIM *pSmart, char *str, int size)
{
	long long candela_level[S6E3_TABLE_MAX] = {-1, };
	int bl_index[S6E3_TABLE_MAX] = {-1, };
	int gamma_setting[GAMMA_SET_MAX];

	long long temp_cal_data = 0;
	int bl_level, cnt;
	int point_index;

	bl_level = pSmart->brightness_level;

	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
			point_index = S6E3_ARRAY[cnt+1];
			temp_cal_data =
			((long long)(candela_coeff_2p2[point_index])) *
			((long long)(bl_level));
			candela_level[cnt] = temp_cal_data;
	}

#ifdef SMART_DIMMING_DEBUG
	printk(KERN_INFO "\n candela_1:%llu  candela_3:%llu  candela_11:%llu ",
		candela_level[0], candela_level[1], candela_level[2]);
	printk(KERN_INFO "candela_23:%llu  candela_35:%llu  candela_51:%llu ",
		candela_level[3], candela_level[4], candela_level[5]);
	printk(KERN_INFO "candela_87:%llu  candela_151:%llu  candela_203:%llu ",
		candela_level[6], candela_level[7], candela_level[8]);
	printk(KERN_INFO "candela_255:%llu\n", candela_level[9]);
#endif

	/*calculate brightness level*/
	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
			if (searching_function(candela_level[cnt],
				&(bl_index[cnt]), GAMMA_CURVE_2P2)) {
				pr_info("%s searching functioin error cnt:%d\n",
					__func__, cnt);
			}
	}

	/*
	*	210CD ~ 190CD compensation
	*	V3 level + 1
	*/
	if ((bl_level >= 190) && (bl_level <= 210))
		bl_index[1] += 1;

#ifdef SMART_DIMMING_DEBUG
	printk(KERN_INFO "\n bl_index_1:%d  bl_index_3:%d  bl_index_11:%d",
	bl_index[0], bl_index[1], bl_index[2]);
	printk(KERN_INFO "bl_index_23:%d bl_index_35:%d  bl_index_51:%d",
		bl_index[3], bl_index[4], bl_index[5]);
	printk(KERN_INFO "bl_index_87:%d  bl_index_151:%d bl_index_203:%d",
		bl_index[5], bl_index[7], bl_index[8]);
	printk(KERN_INFO "bl_index_255:%d\n", bl_index[9]);
#endif

	/*Generate Gamma table*/
	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++)
		(void)Make_hexa[cnt](bl_index , pSmart, str);

	/* To avoid overflow */
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
		gamma_setting[cnt] = str[cnt];

	mtp_offset_substraction(pSmart, gamma_setting);

	/* To avoid overflow */
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
		str[cnt] = gamma_setting[cnt];

}


static void set_max_lux_table(void)
{
	max_lux_table[0] = V255_300CD_R_MSB;
	max_lux_table[1] = V255_300CD_R_LSB;

	max_lux_table[2] = V255_300CD_G_MSB;
	max_lux_table[3] = V255_300CD_G_LSB;

	max_lux_table[4] = V255_300CD_B_MSB;
	max_lux_table[5] = V255_300CD_B_LSB;

	max_lux_table[6] = V203_300CD_R;
	max_lux_table[7] = V203_300CD_G;
	max_lux_table[8] = V203_300CD_B;

	max_lux_table[9] = V151_300CD_R;
	max_lux_table[10] = V151_300CD_G;
	max_lux_table[11] = V151_300CD_B;

	max_lux_table[12] = V87_300CD_R;
	max_lux_table[13] = V87_300CD_G;
	max_lux_table[14] = V87_300CD_B;

	max_lux_table[15] = V51_300CD_R;
	max_lux_table[16] = V51_300CD_G;
	max_lux_table[17] = V51_300CD_B;

	max_lux_table[18] = V35_300CD_R;
	max_lux_table[19] = V35_300CD_G;
	max_lux_table[20] = V35_300CD_B;

	max_lux_table[21] = V23_300CD_R;
	max_lux_table[22] = V23_300CD_G;
	max_lux_table[23] = V23_300CD_B;

	max_lux_table[24] = V11_300CD_R;
	max_lux_table[25] = V11_300CD_G;
	max_lux_table[26] = V11_300CD_B;

	max_lux_table[27] = V3_300CD_R;
	max_lux_table[28] = V3_300CD_G;
	max_lux_table[29] = V3_300CD_B;

	max_lux_table[30] = V0_300CD_R;
	max_lux_table[31] = V0_300CD_G;
	max_lux_table[32] = V0_300CD_B;

	max_lux_table[33] = (VT_300CD_G << 4) | VT_300CD_R;
	max_lux_table[34] = VT_300CD_B;


}


static void set_min_lux_table(struct SMART_DIM *psmart)
{
	psmart->brightness_level = MIN_CANDELA;
	pure_gamma_init(psmart, min_lux_table, GAMMA_SET_MAX);
}

static void get_min_lux_table(char *str, int size)
{
	memcpy(str, min_lux_table, size);
}

static void generate_gamma(struct SMART_DIM *psmart, char *str, int size)
{
	int lux_loop;
	struct illuminance_table *ptable = (struct illuminance_table *)
						(&(psmart->gen_table));

	/* searching already generated gamma table */
	for (lux_loop = 0; lux_loop < psmart->lux_table_max; lux_loop++) {
		if (ptable[lux_loop].lux == psmart->brightness_level) {
			memcpy(str, &(ptable[lux_loop].gamma_setting), size);
			break;
		}
	}

	/* searching fail... Setting 300CD value on gamma table */
	if (lux_loop == psmart->lux_table_max) {
		pr_info("%s searching fail lux : %d\n", __func__,
				psmart->brightness_level);
		memcpy(str, max_lux_table, size);
	}

#ifdef SMART_DIMMING_DEBUG
	if (lux_loop != psmart->lux_table_max)
		pr_info("%s searching ok index : %d lux : %d", __func__,
			lux_loop, ptable[lux_loop].lux);
#endif
}
static void gamma_cell_determine(int ldi_revision)
{
	pr_info("%s ldi_revision: 0x%x", __func__, ldi_revision);

	V255_300CD_R_MSB = V255_300CD_R_MSB_20;
	V255_300CD_R_LSB = V255_300CD_R_LSB_20;

	V255_300CD_G_MSB = V255_300CD_G_MSB_20;
	V255_300CD_G_LSB = V255_300CD_G_LSB_20;

	V255_300CD_B_MSB = V255_300CD_B_MSB_20;
	V255_300CD_B_LSB = V255_300CD_B_LSB_20;

	V203_300CD_R = V203_300CD_R_20;
	V203_300CD_G = V203_300CD_G_20;
	V203_300CD_B = V203_300CD_B_20;

	V151_300CD_R = V151_300CD_R_20;
	V151_300CD_G = V151_300CD_G_20;
	V151_300CD_B = V151_300CD_B_20;

	V87_300CD_R = V87_300CD_R_20;
	V87_300CD_G = V87_300CD_G_20;
	V87_300CD_B = V87_300CD_B_20;

	V51_300CD_R = V51_300CD_R_20;
	V51_300CD_G = V51_300CD_G_20;
	V51_300CD_B = V51_300CD_B_20;

	V35_300CD_R = V35_300CD_R_20;
	V35_300CD_G = V35_300CD_G_20;
	V35_300CD_B = V35_300CD_B_20;

	V23_300CD_R = V23_300CD_R_20;
	V23_300CD_G = V23_300CD_G_20;
	V23_300CD_B = V23_300CD_B_20;

	V11_300CD_R = V11_300CD_R_20;
	V11_300CD_G = V11_300CD_G_20;
	V11_300CD_B = V11_300CD_B_20;

	V3_300CD_R = V3_300CD_R_20;
	V3_300CD_G = V3_300CD_G_20;
	V3_300CD_B = V3_300CD_B_20;

	V0_300CD_R = V0_300CD_R_20;
	V0_300CD_G = V0_300CD_G_20;
	V0_300CD_B = V0_300CD_B_20;

	VT_300CD_R = VT_300CD_R_20;
	VT_300CD_G = VT_300CD_G_20;
	VT_300CD_B = VT_300CD_B_20;
}

static void mtp_sorting(struct SMART_DIM *psmart)
{
	int sorting[GAMMA_SET_MAX] = {
		0, 1, 6, 9, 12, 15, 18, 21, 24, 27, 30, /* R*/
		2, 3, 7, 10, 13, 16, 19, 22, 25, 28, 31,33, /* G */
		4, 5, 8, 11, 14, 17, 20, 23, 26, 29, 32, 34,/* B */
	};
	int loop;
	char *pfrom, *pdest;

	pfrom = (char *)&(psmart->MTP_ORIGN);
	pdest = (char *)&(psmart->MTP);

	for (loop = 0; loop < GAMMA_SET_MAX; loop++)
		pdest[loop] = pfrom[sorting[loop]];

}

static int smart_dimming_init(struct SMART_DIM *psmart)
{
	int lux_loop;
	int id1, id2, id3;
#ifdef SMART_DIMMING_DEBUG
	int cnt;
	char pBuffer[256];
	memset(pBuffer, 0x00, 256);
#endif
	id1 = (psmart->ldi_revision & 0x00FF0000) >> 16;
	id2 = (psmart->ldi_revision & 0x0000FF00) >> 8;
	id3 = psmart->ldi_revision & 0xFF;

	pr_debug("%s : ++\n",__func__);

	mtp_sorting(psmart);
	gamma_cell_determine(psmart->ldi_revision);
	set_max_lux_table();

#ifdef SMART_DIMMING_DEBUG
	print_RGB_offset(psmart);
#endif

	psmart->vregout_voltage = S6E3_VREG1_REF_6P4;

	v255_adjustment(psmart);
	vt_adjustment(psmart);
	v203_adjustment(psmart);
	v151_adjustment(psmart);
	v87_adjustment(psmart);
	v51_adjustment(psmart);
	v35_adjustment(psmart);
	v23_adjustment(psmart);
	v11_adjustment(psmart);
	v3_adjustment(psmart);


	if (generate_gray_scale(psmart)) {
		pr_info(KERN_ERR "lcd smart dimming fail generate_gray_scale\n");
		return -EINVAL;
	}

	/*Generating lux_table*/
	for (lux_loop = 0; lux_loop < psmart->lux_table_max; lux_loop++) {
		/* To set brightness value */
		psmart->brightness_level = psmart->plux_table[lux_loop];
		/* To make lux table index*/
		psmart->gen_table[lux_loop].lux = psmart->plux_table[lux_loop];

#if defined(AID_OPERATION)
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQHD_PT_PANEL)
		if(id3 == EVT0_T_wqhd_REVC || id3 == EVT0_T_wqhd_REVD){
			gamma_init_T_revC(psmart,
			(char *)(&(psmart->gen_table[lux_loop].gamma_setting)),
			GAMMA_SET_MAX);
		}else if (id3 == EVT0_T_wqhd_REVF || id3 == EVT0_T_wqhd_REVG){
			gamma_init_T_revF(psmart,
			(char *)(&(psmart->gen_table[lux_loop].gamma_setting)),
			GAMMA_SET_MAX);
		}else if (id3 == EVT0_T_wqhd_REVH || id3 == EVT0_T_wqhd_REVI || id3 == EVT0_T_wqhd_REVJ){
			gamma_init_T_revH(psmart,
			(char *)(&(psmart->gen_table[lux_loop].gamma_setting)),
			GAMMA_SET_MAX);
		} else {
			gamma_init_T_revA(psmart,
			(char *)(&(psmart->gen_table[lux_loop].gamma_setting)),
			GAMMA_SET_MAX);
		}
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQXGA_PT_PANEL)
	if(id2 == YM4_PANEL){
		gamma_init_T_YM4(psmart,
			(char *)(&(psmart->gen_table[lux_loop].gamma_setting)),
			GAMMA_SET_MAX);
	}else if (id3 == EVT0_T_wqxga_REVB || id3 == EVT0_T_wqxga_REVD){
		gamma_init_T_revD(psmart,
		(char *)(&(psmart->gen_table[lux_loop].gamma_setting)),
		GAMMA_SET_MAX);
	}else if (id3 == EVT0_T_wqxga_REVE){
		gamma_init_T_revE(psmart,
		(char *)(&(psmart->gen_table[lux_loop].gamma_setting)),
		GAMMA_SET_MAX);
	}else{
		gamma_init_T_revA(psmart,
			(char *)(&(psmart->gen_table[lux_loop].gamma_setting)),
			GAMMA_SET_MAX);
	}
#endif
#else
		pure_gamma_init(psmart,
			(char *)(&(psmart->gen_table[lux_loop].gamma_setting)),
			GAMMA_SET_MAX);
#endif
	}

	/* set 300CD max gamma table */
	memcpy(&(psmart->gen_table[lux_loop-1].gamma_setting),
			max_lux_table, GAMMA_SET_MAX);

	set_min_lux_table(psmart);

#ifdef SMART_DIMMING_DEBUG
	for (lux_loop = 0; lux_loop < psmart->lux_table_max; lux_loop++) {
		for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %d",
				psmart->gen_table[lux_loop].gamma_setting[cnt]);

		pr_info("lux : %3d  %s", psmart->plux_table[lux_loop], pBuffer);
		memset(pBuffer, 0x00, 256);
	}

	for (lux_loop = 0; lux_loop < psmart->lux_table_max; lux_loop++) {
		for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256,
				" %02X",
				psmart->gen_table[lux_loop].gamma_setting[cnt]);

		pr_info("lux : %3d  %s", psmart->plux_table[lux_loop], pBuffer);
		memset(pBuffer, 0x00, 256);
	}
#endif

	pr_info("%s done\n",__func__);

	return 0;
}

/* ----------------------------------------------------------------------------
 * Wrapper functions for smart dimming to work with 8974 generic code
 * ----------------------------------------------------------------------------
 */
static void wrap_generate_gamma(int cd, char *cmd_str) {
	smart_S6E3.brightness_level = cd;
	generate_gamma(&smart_S6E3, cmd_str, GAMMA_SET_MAX);
}

static void wrap_smart_dimming_init(void) {
	smart_S6E3.plux_table = __S6E3__.lux_tab;
	smart_S6E3.lux_table_max = __S6E3__.lux_tabsize;
	smart_S6E3.ldi_revision = __S6E3__.man_id;
	smart_dimming_init(&smart_S6E3);
}

struct smartdim_conf *smart_S6E3_get_conf(void) {
	__S6E3__.generate_gamma = wrap_generate_gamma;
	__S6E3__.init = wrap_smart_dimming_init;
	__S6E3__.get_min_lux_table = get_min_lux_table;
	__S6E3__.mtp_buffer = (char *)(&smart_S6E3.MTP_ORIGN);
	__S6E3__.print_aid_log = print_aid_log;
	return (struct smartdim_conf *)&__S6E3__;
}

#if defined(CONFIG_LCD_HMT)
static int ccg6_candela_table_reverse_hmt[][2] = {
	{20,	0},
	{21,	1},
	{22,	2},
	{23,	3},
	{25,	4},
	{27,	5},
	{29,	6},
	{31,	7},
	{33,	8},
	{35,	9},
	{37,	10},
	{39,	11},
	{41,	12},
	{44,	13},
	{47,	14},
	{50,	15},
	{53,	16},
	{56,	17},
	{60,	18},
	{64,	19},
	{68,	20},
	{72,	21},
	{77,	22},
	{82,	23},
	{87,	24},
	{93,	25},
	{99,	26},
	{105,	27},
	{150,	28},
};

static int get_ccg6_max_table_hmt(PANEL_STATE_HMT *panel_state) {
	return MAX_TABLE_HMT;
}

static int get_ccg6_candela_table_hmt(PANEL_STATE_HMT *panel_state, int table_index, int index) {
	return ccg6_candela_table_reverse_hmt[table_index][index];
}

static int find_candela_table_hmt(int brightness, PANEL_STATE_HMT *panel_state) {
	int loop;
	int err = -1;
	int size = get_ccg6_max_table_hmt(panel_state);

	for(loop = 0; loop < size; loop++)
		if (get_ccg6_candela_table_hmt(panel_state, loop, 0) == brightness)
			return get_ccg6_candela_table_hmt(panel_state, loop, 1);

	return err;
}

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQHD_PT_PANEL)

static int gradation_offset_reverse_hmt_single[][9] = {
/*	V255 V203 V151 V87 V51 V35 V23 V11 V3 */
	{0,	3,	3,	6,	6,	7,	8,	8,	9},
	{0,	3,	4,	6,	5,	6,	7,	8,	9},
	{0,	4,	5,	6,	6,	7,	7,	8,	13},
	{0,	4,	6,	6,	6,	7,	7,	8,	11},
	{0,	5,	6,	8,	6,	7,	7,	8,	14},
	{0,	3,	6,	8,	6,	7,	7,	9,	10},
	{0,	4,	6,	8,	6,	7,	8,	8,	12},
	{0,	4,	7,	8,	6,	7,	7,	8,	19},
	{0,	3,	7,	8,	6,	7,	8,	9,	8},
	{0,	3,	7,	8,	7,	7,	7,	8,	19},
	{0,	2,	8,	8,	7,	7,	7,	9,	9},
	{0,	3,	7,	9,	7,	7,	8,	9,	10},
	{0,	3,	8,	9,	7,	7,	7,	8,	12},
	{0,	3,	7,	9,	7,	6,	7,	8,	12},
	{0,	4,	8,	9,	8,	8,	8,	9,	7},
	{0,	5,	10,	9,	8,	8,	8,	9,	9},
	{0,	4,	9,	10,	8,	8,	8,	8,	12},
	{0,	6,	10,	10,	9,	8,	8,	8,	13},
	{0,	3,	8,	9,	8,	8,	7,	8,	13},
	{0,	6,	9,	10,	10,	8,	8,	9,	8},
	{0,	5,	9,	10,	10,	8,	8,	8,	11},
	{0,	6,	9,	11,	11,	8,	8,	8,	11},
	{0,	4,	8,	8,	6,	5,	5,	5,	8},
	{0,	3,	8,	8,	6,	6,	5,	5,	8},
	{0,	5,	8,	9,	7,	6,	5,	5,	9},
	{0,	4,	9,	9,	8,	6,	6,	6,	6},
	{0,	5,	8,	9,	8,	6,	6,	5,	7},
	{0,	5,	7,	10,	8,	6,	6,	5,	7},

	/* for 150 cd */
	{0, 0,	0,	0,	0,	0,	0,	0,	0},
};

static int rgb_offset_reverse_hmt_single[][RGB_COMPENSATION] = {
/*	R255 G255 B255 R203 G203 B203 R151 G151 B151
	R87 G87 B87 R51 G51 B51 R35 G35 B35
	R23 G23 B23 R11 G11 B11
*/
	{13, -3,	16, 0,	0,	0,	0,	0,	1,	1,	0,	0,	-1, -1, 3,	-2, 0,	0,	-7, 0,	-3, -13,	5,	-10},
	{13, -3,	16, 1,	0,	1,	0,	0,	0,	1,	0,	2,	-1, 0,	2,	-2, 0,	0,	-6, 0,	-3, -12,	4,	-9},
	{12, -3,	16, 0,	0,	0,	1,	0,	2,	0,	0,	0,	0,	-1, 2,	-2, 0,	0,	-9, 0,	-4, -19,	5,	-11},
	{13, -3,	16, 1,	0,	1,	0,	0,	1,	1,	0,	2,	0,	0,	2,	-2, 0,	0,	-7, 0,	-3, -15,	5,	-10},
	{12, -3,	15, 0,	0,	1,	0,	0,	0,	1,	0,	1,	0,	-1, 2,	-3, 0,	0,	-6, 0,	-2, -18,	5,	-12},
	{13, -3,	16, 0,	0,	1,	0,	0,	0,	0,	0,	1,	0,	-1, 2,	-2, 0,	0,	-8, 0,	-2, -12,	4,	-9},
	{13, -3,	16, 0,	0,	0,	1,	0,	1,	1,	0,	2,	0,	0,	1,	0,	0,	1,	-7, 0,	-1, -15,	4,	-10},
	{14, -3,	16, 0,	0,	0,	0,	0,	0,	1,	0,	1,	0,	-1, 2,	-2, 0,	0,	-7, 0,	-1, -15,	4,	-10},
	{14, -3,	16, 0,	0,	1,	0,	0,	0,	1,	0,	1,	0,	-1, 2,	-2, 0,	0,	-6, 0,	-1, -12,	3,	-8},
	{14, -3,	16, 0,	0,	0,	0,	0,	1,	2,	0,	2,	0,	-1, 2,	0,	0,	2,	-7, 0,	-2, -14,	4,	-10},
	{13, -3,	16, 0,	0,	1,	0,	0,	0,	2,	0,	2,	0,	-1, 2,	-2, 0,	1,	-6, 0,	-1, -11,	4,	-8},
	{13, -3,	16, 1,	0,	1,	0,	0,	0,	0,	0,	1,	1,	0,	2,	0,	0,	2,	-5, 0,	0,	-12,	3,	-8},
	{14, -3,	16, 0,	0,	1,	1,	0,	0,	1,	0,	2,	1,	-1, 2,	-1, 0,	2,	-4, 0,	-1, -12,	3,	-8},
	{14, -3,	16, 1,	0,	1,	0,	0,	0,	2,	0,	2,	1,	0,	2,	-1, -1, 2,	-4, -1, -1, -12,	3,	-8},
	{14, -3,	16, 1,	0,	1,	0,	0,	0,	0,	0,	2,	0,	0,	1,	-1, 0,	2,	-2, 0,	0,	-10,	3,	-6},
	{13, -4,	16, 0,	0,	0,	1,	0,	1,	1,	0,	1,	0,	-1, 2,	0,	-1, 2,	-3, -1, 0,	-10,	3,	-6},
	{13, -4,	16, 0,	0,	0,	0,	0,	1,	1,	0,	1,	1,	0,	1,	-1, -1, 2,	-3, -1, 0,	-12,	3,	-6},
	{12, -4,	16, 0,	0,	0,	1,	0,	1,	2,	0,	2,	1,	0,	2,	-1, 0,	2,	-2, 0,	0,	-12,	2,	-6},
	{13, -3,	16, 1,	0,	1,	1,	0,	0,	1,	0,	2,	1,	-1, 2,	0,	0,	2,	-4, 0,	-1, -12,	3,	-6},
	{13, -4,	16, 0,	0,	0,	0,	0,	1,	2,	0,	1,	1,	0,	2,	0,	-1, 3,	-2, -1, 0,	-10,	2,	-5},
	{13, -3,	16, 0,	0,	0,	1,	0,	1,	1,	0,	2,	1,	0,	1,	0,	-1, 3,	-1, -1, 0,	-10,	2,	-5},
	{13, -4,	16, 1,	0,	0,	0,	0,	1,	2,	0,	1,	2,	0,	2,	0,	0,	2,	-2, 0,	0,	-9, 2,	-5},
	{14, -3,	16, 1,	0,	1,	0,	0,	1,	1,	-1, 2,	1,	-1, 2,	0,	-1, 2,	0,	-1, 0,	-7, 2,	-6},
	{13, -3,	16, 1,	0,	1,	1,	0,	1,	1,	-1, 2,	1,	0,	1,	1,	-1, 4,	-1, -1, 0,	-6, 2,	-6},
	{13, -4,	16, 1,	0,	1,	0,	0,	1,	2,	0,	2,	2,	0,	2,	0,	-1, 2,	0,	-1, 1,	-6, 2,	-6},
	{13, -4,	16, 0,	0,	0,	0,	0,	1,	1,	0,	2,	2,	0,	2,	1,	-1, 3,	0,	-1, 1,	-5, 2,	-5},
	{13, -4,	16, 1,	0,	1,	1,	0,	1,	0,	0,	2,	2,	0,	2,	1,	-1, 3,	0,	-1, 1,	-5, 2,	-4},
	{13, -4,	16, 1,	0,	1,	0,	0,	1,	2,	0,	2,	2,	0,	2,	0,	-1, 3,	0,	-1, 1,	-4, 2,	-4},

	/* for 150 cd */
	{-12,	-22,	-8,	-2,	-2,	-3,	6,	5,	6,	10,	8,	9,	6,	5,	7,	0,	0,	3,	5,	3,	6,	13,	17,	10},
};

static int base_luminance_reverse_hmt_single[][2] = {
	{20, 98},
	{21, 101},
	{22, 105},
	{23, 108},
	{25, 117},
	{27, 126},
	{29, 134},
	{31, 141},
	{33, 151},
	{35, 157},
	{37, 166},
	{39, 173},
	{41, 181},
	{44, 192},
	{47, 206},
	{50, 215},
	{53, 228},
	{56, 239},
	{60, 252},
	{64, 268},
	{68, 284},
	{72, 297},
	{77, 224},
	{82, 237},
	{87, 252},
	{93, 265},
	{99, 281},
	{105, 295},
	{150, 500},
};

#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQXGA_PT_PANEL)

static int gradation_offset_reverse_hmt_single_TB[][9] = {
/*	V255 V203 V151 V87 V51 V35 V23 V11 V3 */
	{0, 4, 3, 5, 6, 6, 7, 9, 11 },
	{0, 6, 3, 6, 5, 7, 7, 9, 15 },
	{0, 4, 3, 5, 6, 7, 8, 8, 16 },
	{0, 4, 4, 6, 5, 6, 7, 8, 15 },
	{0, 6, 7, 8, 7, 7, 7, 8, 13 },
	{0, 6, 9, 8, 7, 7, 7, 9, 9 },
	{0, 7, 9, 7, 7, 7, 7, 9, 9 },
	{0, 5, 9, 7, 7, 7, 7, 9, 17 },
	{0, 4, 9, 8, 7, 7, 7, 8, 16 },
	{0, 5, 9, 8, 7, 7, 7, 8, 15 },
	{0, 3, 7, 8, 7, 7, 6, 8, 16 },
	{0, 5, 11, 9, 8, 8, 7, 9, 12 },
	{0, 5, 9, 10, 8, 7, 7, 9, 10 },
	{0, 7, 11, 10, 8, 8, 8, 9, 17 },
	{0, 6, 10, 9, 8, 8, 7, 8, 16 },
	{0, 6, 12, 10, 8, 7, 8, 9, 8 },
	{0, 5, 10, 10, 8, 8, 8, 9, 10 },
	{0, 7, 11, 9, 8, 8, 8, 9, 10 },
	{0, 4, 11, 10, 8, 9, 7, 9, 12 },
	{0, 6, 12, 10, 9, 9, 8, 8, 15 },
	{0, 9, 13, 11, 9, 9, 9, 9, 8 },
	{0, 6, 14, 12, 10, 9, 8, 9, 10 },
	{0, 8, 10, 9, 7, 6, 5, 6, 5 },
	{0, 5, 10, 8, 6, 6, 6, 6, 6 },
	{0, 7, 10, 8, 7, 6, 5, 5, 7 },
	{0, 5, 9, 8, 6, 6, 6, 5, 9 },
	{0, 9, 12, 11, 8, 8, 6, 6, 6 },
	{0, 5, 11, 10, 8, 7, 6, 6, 8 },

	/* for 150 cd */
	{0, 0,	0,	0,	0,	0,	0,	0,	0},
};

static int rgb_offset_reverse_hmt_single_TB[][RGB_COMPENSATION] = {
/*	R255 G255 B255 R203 G203 B203 R151 G151 B151
	R87 G87 B87 R51 G51 B51 R35 G35 B35
	R23 G23 B23 R11 G11 B11
*/
	{13, -4, 17, 0, 0, 1, 6, -1, 5, 0, 0, 0, -4, 0, 2, -3, 1, -2, -11, 1, -7, -40, 6, -14},
	{12, -4, 17, 1, 0, 2, 6, -1, 5, 0, 0, 0, -5, 0, 0, -4, 0, -2, -11, 0, -6, -42, 6, -14},
	{15, -4, 20, -1, 0, 0, 4, -1, 4, 0, 0, 1, -4, 0, 1, -3, 1, -2, -9, 1, -5, -44, 7, -16},
	{15, -4, 20, -1, 0, 0, 3, 0, 4, 2, 0, 2, -4, 0, 1, -3, 0, -2, -10, 0, -6, -42, 7, -14},
	{14, -4, 18, 0, 0, 1, 1, 0, 3, 4, -1, 3, -3, 0, 1, -3, 0, -2, -11, 0, -6, -44, 7, -15},
	{13, -4, 17, 1, 0, 2, -1, 0, 0, 5, -1, 4, -4, 0, 0, -2, 0, -1, -11, 0, -6, -29, 5, -11},
	{15, -4, 19, 0, 0, 0, 0, 0, 2, 5, -1, 4, -4, 0, 0, -2, 0, -1, -7, 0, -4, -30, 6, -13},
	{15, -4, 19, 0, 0, 0, 0, 0, 0, 2, -2, 4, -1, 0, 2, -4, 0, -1, -10, 0, -5, -34, 6, -14},
	{17, -4, 19, -1, 0, 1, -1, 0, 0, 5, -1, 4, -4, 0, 0, -2, 0, 0, -8, 0, -4, -32, 6, -14},
	{16, -4, 19, 0, 0, 0, 0, 0, 2, 5, -1, 4, -4, 0, 0, -3, 0, 0, -7, 0, -4, -29, 6, -13},
	{17, -4, 20, 0, 0, 0, -1, 0, 1, 3, -1, 3, -1, -1, 2, -3, 0, 0, -8, 0, -4, -32, 6, -14},
	{16, -5, 20, 0, 0, 0, -1, 0, 0, 1, -1, 2, -2, 0, 1, -2, 0, 0, -8, 0, -4, -26, 6, -12},
	{17, -4, 20, 2, 0, 2, -1, 0, 0, 2, -1, 3, 0, 0, 1, -2, 0, 0, -8, 0, -4, -24, 5, -12},
	{17, -5, 21, 0, 0, 0, -1, 0, 1, 2, -1, 3, -2, 0, 1, -2, 0, 0, -7, 0, -4, -27, 5, -12},
	{17, -5, 20, 0, 0, 0, 0, 0, 2, 1, -1, 2, -1, 0, 1, -3, 0, 0, -6, 0, -4, -24, 5, -11},
	{17, -5, 21, 0, 0, 0, 0, 0, 1, 3, -1, 4, -1, 0, 1, -4, 0, 0, -5, 0, -2, -19, 5, -10},
	{20, -5, 23, 0, 0, 0, -1, 0, 0, 2, -1, 3, -1, 0, 0, -4, 0, 0, -4, 0, -1, -20, 4, -10},
	{19, -5, 23, 0, 0, 0, -1, 0, 0, 2, -1, 3, 0, 0, 1, -3, 0, 0, -5, 0, -2, -20, 4, -10},
	{22, -5, 24, -1, 0, 0, -1, 0, 0, 2, -1, 3, 0, 0, 1, -4, 0, 0, -6, 0, -2, -20, 4, -10},
	{19, -5, 22, 2, 0, 2, -1, 0, 0, 0, -1, 2, 0, 0, 1, -4, 0, 0, -5, 0, -2, -20, 4, -10},
	{19, -5, 23, 0, 0, 0, 0, 0, 1, 2, -1, 2, 0, 0, 1, -3, 0, 0, -5, 0, -2, -15, 4, -8},
	{21, -6, 24, 0, 0, 1, -1, 0, -1, 1, -1, 2, 0, 0, 1, -2, 0, 0, -5, 0, -2, -16, 3, -8},
	{15, -5, 20, 0, 0, 0, 2, 0, 3, 1, -1, 2, 1, -1, 2, 0, 0, 2, -2, 0, 0, -11, 4, -8},
	{17, -4, 20, 0, 0, 1, 0, 0, 1, 3, -1, 3, 0, -1, 2, -1, 0, 2, -1, 0, -1, -11, 3, -8},
	{16, -5, 21, 0, 0, 0, 0, 0, 1, 2, -1, 3, 0, -1, 2, 0, 0, 2, -1, 0, 0, -10, 4, -8},
	{19, -5, 22, -1, 0, 0, 0, 0, 1, 0, 0, 1, 3, -1, 4, 0, 0, 2, -2, 0, 0, -11, 3, -8},
	{16, -5, 21, 0, 0, 1, 0, 0, 0, 0, 0, 1, 3, -2, 4, 0, 0, 2, -2, 0, -1, -9, 3, -6},
	{20, -5, 23, 0, 0, 1, 0, 0, -1, -1, 0, 1, 1, -2, 4, 0, 0, 1, -2, 0, -1, -8, 2, -6},

	/* for 150 cd */
	{-13, -32, -9, -4, -4, -5, 5, 4, 5, 1, 0, -2, -4, -10, -1, -8, -10, -6, -11, -10, -7, -34, -24, -13},
};

static int base_luminance_reverse_hmt_single_TB[][2] = {
	{20,	92},
	{21,	94},
	{22,	99},
	{23,	104},
	{25,	107},
	{27,	113},
	{29,	122},
	{31,	133},
	{33,	138},
	{35,	145},
	{37,	154},
	{39,	160},
	{41,	163},
	{44,	173},
	{47,	184},
	{50,	193},
	{53,	206},
	{56,	215},
	{60,	223},
	{64,	237},
	{68,	247},
	{72,	262},
	{77,	206},
	{82,	217},
	{87,	229},
	{93,	248},
	{99,	256},
	{105,	265},
	{150, 	500},
};

#endif

static int get_candela_level_hmt(int brightness_level, PANEL_STATE_HMT *panel_state) {

	int cnt;
	int base_luminance[MAX_TABLE_HMT][2];

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQHD_PT_PANEL)
	memcpy(base_luminance, base_luminance_reverse_hmt_single, sizeof(base_luminance_reverse_hmt_single));
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQXGA_PT_PANEL)
	memcpy(base_luminance, base_luminance_reverse_hmt_single_TB, sizeof(base_luminance_reverse_hmt_single_TB));
#endif

	for (cnt = 0; cnt < MAX_TABLE_HMT; cnt++)
		if (base_luminance[cnt][0] == brightness_level)
			return base_luminance[cnt][1];

	return -1;
}

static int get_gradation_offset_hmt(PANEL_STATE_HMT *panel_state, int table_index, int index) {
	int gradation_offset = 0;

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQHD_PT_PANEL)
	gradation_offset = gradation_offset_reverse_hmt_single[table_index][index];
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQXGA_PT_PANEL)
	gradation_offset = gradation_offset_reverse_hmt_single_TB[table_index][index];
#endif
	return gradation_offset;
}

static int get_rgb_offset_hmt(PANEL_STATE_HMT *panel_state, int table_index, int index) {
	int rgb_offset = 0;

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQHD_PT_PANEL)
	rgb_offset = rgb_offset_reverse_hmt_single[table_index][index];
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQXGA_PT_PANEL)
	rgb_offset = rgb_offset_reverse_hmt_single_TB[table_index][index];
#endif
	return rgb_offset;
}

static int get_gamma_curve(PANEL_STATE_HMT *panel_state)
{
	return GAMMA_CURVE_2P15;
}

static void gamma_init_hmt_150cd(struct SMART_DIM *pSmart, char *str, int size, PANEL_STATE_HMT *panel_state)
{
	int cnt;
	int table_index;
	int level_255_temp_MSB = 0;
	int level_V255 = 0;
	int gamma_setting[GAMMA_SET_MAX];

	pr_debug("%s : setting for %d cd\n", __func__, pSmart->brightness_level);

	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
			gamma_setting[cnt] = str[cnt];

	for (cnt = 0; cnt < RGB_COMPENSATION - 3; cnt++) {
		table_index = find_candela_table_hmt(pSmart->brightness_level, panel_state);

		if (table_index == -1) {
			table_index = get_ccg6_max_table_hmt(panel_state) - 1;
			pr_info("%s fail RGB table_index cnt : %d brightness %d",
				__func__, cnt, pSmart->brightness_level);
		}

		if (cnt < 3) {
			level_V255 = gamma_setting[cnt * 2] << 8 | gamma_setting[(cnt * 2) + 1];
			level_V255 +=
				get_rgb_offset_hmt(panel_state, table_index, cnt);
			level_255_temp_MSB = level_V255 / 256;

			gamma_setting[cnt * 2] = level_255_temp_MSB & 0xff;
			gamma_setting[(cnt * 2) + 1] = level_V255 & 0xff;
			pr_debug(" 0x%x 0x%x\n", gamma_setting[cnt * 2], gamma_setting[(cnt * 2) + 1]);
		} else {
			gamma_setting[cnt+3] += get_rgb_offset_hmt(panel_state, table_index, cnt);
			pr_debug(" 0x%x\n", gamma_setting[cnt+3]);
		}
	}

	/* To avoid overflow */
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
		str[cnt] = gamma_setting[cnt];

	pr_debug("%s -- \n", __func__);
}

static void gamma_init_hmt(struct SMART_DIM *pSmart, char *str, int size, PANEL_STATE_HMT *panel_state) {
	long long candela_level[S6E3_TABLE_MAX] = {-1, };
	int bl_index[S6E3_TABLE_MAX] = {-1, };
	int gamma_setting[GAMMA_SET_MAX];

	long long temp_cal_data = 0;
	int bl_level;

	int level_255_temp_MSB = 0;
	int level_V255 = 0;

	int point_index;
	int cnt;
	int table_index;

	pr_info_once("%s : start !!\n",__func__);

	/*calculate candela level */
	bl_level = get_candela_level_hmt(pSmart->brightness_level, panel_state);

	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
		point_index = S6E3_ARRAY[cnt+1];
		temp_cal_data =
		((long long)(candela_coeff_2p15[point_index])) *
		((long long)(bl_level));
		candela_level[cnt] = temp_cal_data;
	}

#ifdef SMART_DIMMING_DEBUG
	printk(KERN_INFO "\n candela_1:%llu  candela_3:%llu  candela_11:%llu ",
		candela_level[0], candela_level[1], candela_level[2]);
	printk(KERN_INFO "candela_23:%llu  candela_35:%llu  candela_51:%llu ",
		candela_level[3], candela_level[4], candela_level[5]);
	printk(KERN_INFO "candela_87:%llu  candela_151:%llu  candela_203:%llu ",
		candela_level[6], candela_level[7], candela_level[8]);
	printk(KERN_INFO "candela_255:%llu brightness_level %d\n", candela_level[9], pSmart->brightness_level);
#endif

	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++) {
		if (searching_function(candela_level[cnt],
			&(bl_index[cnt]), get_gamma_curve(panel_state))) {
			pr_info("%s searching functioin error cnt:%d\n",
			__func__, cnt);
		}
	}

	/*
	*	Candela compensation
	*/

	for (cnt = 1; cnt < S6E3_TABLE_MAX; cnt++) {
		table_index = find_candela_table_hmt(pSmart->brightness_level, panel_state);

		if (table_index == -1) {
			table_index = get_ccg6_max_table_hmt(panel_state) - 1;
			pr_info("%s fail candela table_index cnt : %d brightness %d\n",
				__func__, cnt, pSmart->brightness_level);
		}

		bl_index[S6E3_TABLE_MAX - cnt] +=
			get_gradation_offset_hmt(panel_state, table_index, cnt - 1);

		/* THERE IS M-GRAY0 target */
		if (bl_index[S6E3_TABLE_MAX - cnt] == 0)
			bl_index[S6E3_TABLE_MAX - cnt] = 1;
	}

#ifdef SMART_DIMMING_DEBUG
	printk(KERN_INFO "\n bl_index_1:%d  bl_index_3:%d  bl_index_11:%d",
		bl_index[0], bl_index[1], bl_index[2]);
	printk(KERN_INFO "bl_index_23:%d bl_index_35:%d  bl_index_51:%d",
		bl_index[3], bl_index[4], bl_index[5]);
	printk(KERN_INFO "bl_index_87:%d  bl_index_151:%d bl_index_203:%d",
		bl_index[6], bl_index[7], bl_index[8]);
	printk(KERN_INFO "bl_index_255:%d\n", bl_index[9]);
#endif
	/*Generate Gamma table*/
	for (cnt = 0; cnt < S6E3_TABLE_MAX; cnt++)
		(void)Make_hexa[cnt](bl_index , pSmart, str);

	/* To avoid overflow */
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
		gamma_setting[cnt] = str[cnt];

	/*
	*	RGB compensation
	*/
	for (cnt = 0; cnt < RGB_COMPENSATION; cnt++) {
		table_index = find_candela_table_hmt(pSmart->brightness_level, panel_state);

		if (table_index == -1) {
			table_index = get_ccg6_max_table_hmt(panel_state) - 1;
			pr_info("%s fail RGB table_index cnt : %d brightness %d",
				__func__, cnt, pSmart->brightness_level);
		}

		if (cnt < 3) {
			level_V255 = gamma_setting[cnt * 2] << 8 | gamma_setting[(cnt * 2) + 1];
			level_V255 +=
				get_rgb_offset_hmt(panel_state, table_index, cnt);
			level_255_temp_MSB = level_V255 / 256;

			gamma_setting[cnt * 2] = level_255_temp_MSB & 0xff;
			gamma_setting[(cnt * 2) + 1] = level_V255 & 0xff;
		} else {
			gamma_setting[cnt+3] += get_rgb_offset_hmt(panel_state, table_index, cnt);
		}
	}
	/*subtration MTP_OFFSET value from generated gamma table*/
	mtp_offset_substraction(pSmart, gamma_setting);

	/* To avoid overflow */
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
		str[cnt] = gamma_setting[cnt];
}

static void generate_gamma_hmt(struct SMART_DIM *psmart, char *str, int size)
{
	int lux_loop;
	struct illuminance_table *ptable = (struct illuminance_table *)
						(&(psmart->hmt_gen_table));

	/* force copy for 150cd */
	if (psmart->brightness_level == 150) {
		pr_info("%s : force copy gamma str for 150cd !!\n", __func__);
		memcpy(str, &(ptable[MAX_TABLE_HMT-1].gamma_setting), size);
		return;
	}

	/* searching already generated gamma table */
	for (lux_loop = 0; lux_loop < psmart->lux_table_max; lux_loop++) {
		if (ptable[lux_loop].lux == psmart->brightness_level) {
			memcpy(str, &(ptable[lux_loop].gamma_setting), size);
			break;
		}
	}

	/* searching fail... Setting 150CD value on gamma table */
	if (lux_loop == psmart->lux_table_max) {
		pr_info("%s searching fail lux : %d\n", __func__,
				psmart->brightness_level);
		memcpy(str, max_lux_table, size);
	}

#ifdef SMART_DIMMING_DEBUG
	if (lux_loop != psmart->lux_table_max)
		pr_info("%s searching ok index : %d lux : %d", __func__,
			lux_loop, ptable[lux_loop].lux);
#endif
}

static char hmt_150cd_lux_table[GAMMA_SET_MAX] = {0,};

void set_150cd_lux_table(char *para, int size)
{
	int i;

	for (i=0; i<size; i++) {
		pr_debug("%s : B4[%d] = 0x%x\n", __func__, i, para[i]);
		hmt_150cd_lux_table[i] = para[i];
	}
}

static int smart_dimming_init_hmt(struct SMART_DIM *psmart)
{
	PANEL_STATE_HMT panel_state;
	int lux_loop;
	int id1, id2, id3;
#ifdef SMART_DIMMING_DEBUG
	int cnt;
	char pBuffer[256];
	memset(pBuffer, 0x00, 256);
#endif
	id1 = (psmart->ldi_revision & 0x00FF0000) >> 16;
	id2 = (psmart->ldi_revision & 0x0000FF00) >> 8;
	id3 = psmart->ldi_revision & 0xFF;

	pr_info_once("[HMT] %s : ++\n",__func__);

	panel_state.panel_rev = id3;

	mtp_sorting(psmart);
	gamma_cell_determine(psmart->ldi_revision);

#ifdef SMART_DIMMING_DEBUG
	print_RGB_offset(psmart);
#endif
	psmart->vregout_voltage = S6E3_VREG1_REF_6P4;

	v255_adjustment(psmart);
	vt_adjustment(psmart);
	v203_adjustment(psmart);
	v151_adjustment(psmart);
	v87_adjustment(psmart);
	v51_adjustment(psmart);
	v35_adjustment(psmart);
	v23_adjustment(psmart);
	v11_adjustment(psmart);
	v3_adjustment(psmart);

	if (generate_gray_scale(psmart)) {
		pr_info(KERN_ERR "lcd smart dimming fail generate_gray_scale\n");
		return -EINVAL;
	}

	/*Generating lux_table*/
	for (lux_loop = 0; lux_loop < psmart->lux_table_max; lux_loop++) {
		/* To set brightness value */
		psmart->brightness_level = psmart->plux_table[lux_loop];
		/* To make lux table index*/
		psmart->hmt_gen_table[lux_loop].lux = psmart->plux_table[lux_loop];

		pr_debug("%s : lux_loop(%d) lux_table_max (%d), lux (%d)\n",
			__func__, lux_loop, psmart->lux_table_max, psmart->hmt_gen_table[lux_loop].lux);

#if defined(AID_OPERATION)
		gamma_init_hmt(psmart,
			(char *)(&(psmart->hmt_gen_table[lux_loop].gamma_setting)),
			GAMMA_SET_MAX, &panel_state);
#else
		pure_gamma_init(psmart,
			(char *)(&(psmart->hmt_gen_table[lux_loop].gamma_setting)),
			GAMMA_SET_MAX);
#endif
	}

	/* gamma gen for 150cd */
	/* set 150CD gamma table */
	memcpy(&(psmart->hmt_gen_table[lux_loop].gamma_setting),
			hmt_150cd_lux_table, GAMMA_SET_MAX);

	psmart->brightness_level = 150;
	psmart->hmt_gen_table[lux_loop].lux = 150;

	gamma_init_hmt_150cd(psmart,
			(char *)(&(psmart->hmt_gen_table[lux_loop].gamma_setting)),
			GAMMA_SET_MAX, &panel_state);

#ifdef SMART_DIMMING_DEBUG
	for (lux_loop = 0; lux_loop < psmart->lux_table_max; lux_loop++) {
		for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %d",
				psmart->hmt_gen_table[lux_loop].gamma_setting[cnt]);

		pr_info("lux : %3d  %s", psmart->plux_table[lux_loop], pBuffer);
		memset(pBuffer, 0x00, 256);
	}

	for (lux_loop = 0; lux_loop < psmart->lux_table_max; lux_loop++) {
		for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256,
				" %02X",
				psmart->hmt_gen_table[lux_loop].gamma_setting[cnt]);

		pr_info("lux : %3d  %s", psmart->plux_table[lux_loop], pBuffer);
		memset(pBuffer, 0x00, 256);
	}
#endif

	pr_info("[HMT] %s --\n",__func__);

	return 0;
}

static void wrap_generate_gamma_hmt(int cd, char *cmd_str) {
	smart_S6E3_reverse_hmt_single.brightness_level = cd;
	generate_gamma_hmt(&smart_S6E3_reverse_hmt_single, cmd_str, GAMMA_SET_MAX);
}

static void wrap_smart_dimming_init_hmt(void) {
	smart_S6E3_reverse_hmt_single.plux_table = __S6E3_REVERSE_HMT_S__.lux_tab;
	smart_S6E3_reverse_hmt_single.lux_table_max = __S6E3_REVERSE_HMT_S__.lux_tabsize;
	smart_S6E3_reverse_hmt_single.ldi_revision = __S6E3_REVERSE_HMT_S__.man_id;
	smart_dimming_init_hmt(&smart_S6E3_reverse_hmt_single);
}

struct smartdim_conf_hmt *smart_S6E3_get_conf_hmt(void) {
	__S6E3_REVERSE_HMT_S__.generate_gamma = wrap_generate_gamma_hmt;
	__S6E3_REVERSE_HMT_S__.init = wrap_smart_dimming_init_hmt;
	__S6E3_REVERSE_HMT_S__.get_min_lux_table = get_min_lux_table;
	__S6E3_REVERSE_HMT_S__.mtp_buffer = (char *)(&smart_S6E3_reverse_hmt_single.MTP_ORIGN);
	__S6E3_REVERSE_HMT_S__.print_aid_log = print_aid_log_hmt_single;
	__S6E3_REVERSE_HMT_S__.set_para_for_150cd = set_150cd_lux_table;
	return (struct smartdim_conf_hmt *)&__S6E3_REVERSE_HMT_S__;
}
#endif
