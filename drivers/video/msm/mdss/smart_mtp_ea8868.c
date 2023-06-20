/*
 * =================================================================
 *
 *       Filename:  smart_mtp_EA8868.c
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

#include "smart_mtp_ea8868.h"
#include "smart_mtp_2p2_gamma.h"


static int char_to_int(char data1)
{
	int cal_data;

	if (data1 & 0x80)
		cal_data = 0xffffff00 | data1;
	else
		cal_data = data1;

	return cal_data;
}

#define v255_coefficient 100
#define v255_denominator 600
static int v255_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	int add_mtp;
	int LSB;

	LSB = char_to_int(pSmart->MTP.R_OFFSET.OFFSET_255_LSB);
	add_mtp = LSB + V255_300CD_R_LSB;
	result_1 = result_2 = (v255_coefficient+add_mtp) << BIT_SHIFT;
	do_div(result_2, v255_denominator);
	result_3 = (EA8868_VREG0_REF * result_2) >> BIT_SHIFT;
	result_4 = EA8868_VREG0_REF - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_255 = result_4;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_0 = EA8868_VREG0_REF;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_255_LSB);
	add_mtp = LSB + V255_300CD_G_LSB;
	result_1 = result_2 = (v255_coefficient+add_mtp) << BIT_SHIFT;
	do_div(result_2, v255_denominator);
	result_3 = (EA8868_VREG0_REF * result_2) >> BIT_SHIFT;
	result_4 = EA8868_VREG0_REF - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_255 = result_4;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_0 = EA8868_VREG0_REF;

	LSB = char_to_int(pSmart->MTP.B_OFFSET.OFFSET_255_LSB);
	add_mtp = LSB + V255_300CD_B_LSB;
	result_1 = result_2 = (v255_coefficient+add_mtp) << BIT_SHIFT;
	do_div(result_2, v255_denominator);
	result_3 = (EA8868_VREG0_REF * result_2) >> BIT_SHIFT;
	result_4 = EA8868_VREG0_REF - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_255 = result_4;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_0 = EA8868_VREG0_REF;

	#ifdef SMART_DIMMING_DEBUG
	printk("%s V255 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_255,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_255,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_255);
	#endif

	return 0;
}

static void v255_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = EA8868_VREG0_REF -
		(pSmart->GRAY.TABLE[index[V255_INDEX]].R_Gray);

	result_2 = result_1 * v255_denominator;
	do_div(result_2, EA8868_VREG0_REF);
	result_3 = result_2  - v255_coefficient;
	str[0] = (result_3 & 0xff00) >> 8;
	str[1] = result_3 & 0xff;

	result_1 = EA8868_VREG0_REF -
		(pSmart->GRAY.TABLE[index[V255_INDEX]].G_Gray);
	result_2 = result_1 * v255_denominator;
	do_div(result_2, EA8868_VREG0_REF);
	result_3 = result_2  - v255_coefficient;
	str[8] = (result_3 & 0xff00) >> 8;
	str[9] = result_3 & 0xff;

	result_1 = EA8868_VREG0_REF -
			(pSmart->GRAY.TABLE[index[V255_INDEX]].B_Gray);
	result_2 = result_1 * v255_denominator;
	do_div(result_2, EA8868_VREG0_REF);
	result_3 = result_2  - v255_coefficient;
	str[16] = (result_3 & 0xff00) >> 8;
	str[17] = result_3 & 0xff;

}

#define v1_coefficient 4
#define v1_denominator 600
static int v1_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	int add_mtp;
	int LSB;

	LSB = char_to_int(0);
	add_mtp = LSB + V1_300CD_R;
	result_1 = result_2 = (v1_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v1_denominator);
	result_3 = (EA8868_VREG0_REF * result_2) >> BIT_SHIFT;
	result_4 = EA8868_VREG0_REF - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_1 = result_4;

	LSB = char_to_int(0);
	add_mtp = LSB + V1_300CD_G;
	result_1 = result_2 = (v1_coefficient+add_mtp) << BIT_SHIFT;
	do_div(result_2, v1_denominator);
	result_3 = (EA8868_VREG0_REF * result_2) >> BIT_SHIFT;
	result_4 = EA8868_VREG0_REF - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_1 = result_4;

	LSB = char_to_int(0);
	add_mtp = LSB + V1_300CD_B;
	result_1 = result_2 = (v1_coefficient+add_mtp) << BIT_SHIFT;
	do_div(result_2, v1_denominator);
	result_3 = (EA8868_VREG0_REF * result_2) >> BIT_SHIFT;
	result_4 = EA8868_VREG0_REF - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_1 = result_4;

	#ifdef SMART_DIMMING_DEBUG
	printk("%s V1 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_1,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_1,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_1);
	#endif

	return 0;

}

static void v1_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	str[7] = V1_300CD_R;
	str[15] = V1_300CD_G;
	str[23] =  V1_300CD_B;
}

#define v171_coefficient 64
#define v171_denominator 320
static int v171_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	int add_mtp;
	int LSB;

	LSB = char_to_int(pSmart->MTP.R_OFFSET.OFFSET_171);
	add_mtp = LSB + V171_300CD_R;
	result_1 = (pSmart->RGB_OUTPUT.R_VOLTAGE.level_1)
				- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_255);
	result_2 = (v171_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v171_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.R_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_171 = result_4;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_171);
	add_mtp = LSB + V171_300CD_G;
	result_1 = (pSmart->RGB_OUTPUT.G_VOLTAGE.level_1)
				- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_255);
	result_2 = (v171_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v171_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.G_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_171 = result_4;

	LSB = char_to_int(pSmart->MTP.B_OFFSET.OFFSET_171);
	add_mtp = LSB + V171_300CD_B;
	result_1 = (pSmart->RGB_OUTPUT.B_VOLTAGE.level_1)
				- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_255);
	result_2 = (v171_coefficient+add_mtp) << BIT_SHIFT;
	do_div(result_2, v171_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.B_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_171 = result_4;

	#ifdef SMART_DIMMING_DEBUG
	printk("%s V171 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_171,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_171,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_171);
	#endif

	return 0;

}

static void v171_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = (pSmart->GRAY.TABLE[1].R_Gray)
			- (pSmart->GRAY.TABLE[index[V171_INDEX]].R_Gray);
	result_2 = result_1 * v171_denominator;
	result_3 = (pSmart->GRAY.TABLE[1].R_Gray)
			- (pSmart->GRAY.TABLE[index[V255_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[2] = (result_2  - v171_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.TABLE[1].G_Gray)
			- (pSmart->GRAY.TABLE[index[V171_INDEX]].G_Gray);
	result_2 = result_1 * v171_denominator;
	result_3 = (pSmart->GRAY.TABLE[1].G_Gray)
			- (pSmart->GRAY.TABLE[index[V255_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[10] = (result_2  - v171_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.TABLE[1].B_Gray)
			- (pSmart->GRAY.TABLE[index[V171_INDEX]].B_Gray);
	result_2 = result_1 * v171_denominator;
	result_3 = (pSmart->GRAY.TABLE[1].B_Gray)
			- (pSmart->GRAY.TABLE[index[V255_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[18] = (result_2  - v171_coefficient) & 0xff;

}

#define v87_coefficient 64
#define v87_denominator 320
static int v87_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	int add_mtp;
	int LSB;

	LSB = char_to_int(pSmart->MTP.R_OFFSET.OFFSET_87);
	add_mtp = LSB + V87_300CD_R;
	result_1 = (pSmart->RGB_OUTPUT.R_VOLTAGE.level_1)
			- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_171);
	result_2 = (v87_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v87_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.R_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_87 = result_4;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_87);
	add_mtp = LSB + V87_300CD_G;
	result_1 = (pSmart->RGB_OUTPUT.G_VOLTAGE.level_1)
			- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_171);
	result_2 = (v87_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v87_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.G_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_87 = result_4;

	LSB = char_to_int(pSmart->MTP.B_OFFSET.OFFSET_87);
	add_mtp = LSB + V87_300CD_B;
	result_1 = (pSmart->RGB_OUTPUT.B_VOLTAGE.level_1)
			- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_171);
	result_2 = (v87_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v87_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.B_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_87 = result_4;

	#ifdef SMART_DIMMING_DEBUG
	printk("%s V87 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_87,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_87,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_87);
	#endif

	return 0;

}

static void v87_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = (pSmart->GRAY.TABLE[1].R_Gray)
			- (pSmart->GRAY.TABLE[index[V87_INDEX]].R_Gray);
	result_2 = result_1 * v87_denominator;
	result_3 = (pSmart->GRAY.TABLE[1].R_Gray)
			- (pSmart->GRAY.TABLE[index[V171_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[3] = (result_2  - v87_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.TABLE[1].G_Gray)
			- (pSmart->GRAY.TABLE[index[V87_INDEX]].G_Gray);
	result_2 = result_1 * v87_denominator;
	result_3 = (pSmart->GRAY.TABLE[1].G_Gray)
			- (pSmart->GRAY.TABLE[index[V171_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[11] = (result_2  - v87_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.TABLE[1].B_Gray)
			- (pSmart->GRAY.TABLE[index[V87_INDEX]].B_Gray);
	result_2 = result_1 * v87_denominator;
	result_3 = (pSmart->GRAY.TABLE[1].B_Gray)
			- (pSmart->GRAY.TABLE[index[V171_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[19] = (result_2  - v87_coefficient) & 0xff;
}

#define v59_coefficient 64
#define v59_denominator 320
static int v59_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	int add_mtp;
	int LSB;

	LSB = char_to_int(pSmart->MTP.R_OFFSET.OFFSET_59);
	add_mtp = LSB + V59_300CD_R;
	result_1 = (pSmart->RGB_OUTPUT.R_VOLTAGE.level_1)
			- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_87);
	result_2 = (v59_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v59_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.R_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_59 = result_4;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_59);
	add_mtp = LSB + V59_300CD_G;
	result_1 = (pSmart->RGB_OUTPUT.G_VOLTAGE.level_1)
			- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_87);
	result_2 = (v59_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v59_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.G_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_59 = result_4;

	LSB = char_to_int(pSmart->MTP.B_OFFSET.OFFSET_59);
	add_mtp = LSB + V59_300CD_B;
	result_1 = (pSmart->RGB_OUTPUT.B_VOLTAGE.level_1)
			- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_87);
	result_2 = (v59_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v59_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.B_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_59 = result_4;

	#ifdef SMART_DIMMING_DEBUG
	printk("%s V59 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_59,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_59,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_59);
	#endif

	return 0;


}

static void v59_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = (pSmart->GRAY.TABLE[1].R_Gray)
			- (pSmart->GRAY.TABLE[index[V59_INDEX]].R_Gray);
	result_2 = result_1 * v59_denominator;
	result_3 = (pSmart->GRAY.TABLE[1].R_Gray)
			- (pSmart->GRAY.TABLE[index[V87_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[4] = (result_2  - v59_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.TABLE[1].G_Gray)
			- (pSmart->GRAY.TABLE[index[V59_INDEX]].G_Gray);
	result_2 = result_1 * v59_denominator;
	result_3 = (pSmart->GRAY.TABLE[1].G_Gray)
			- (pSmart->GRAY.TABLE[index[V87_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[12] = (result_2  - v59_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.TABLE[1].B_Gray)
			- (pSmart->GRAY.TABLE[index[V59_INDEX]].B_Gray);
	result_2 = result_1 * v59_denominator;
	result_3 = (pSmart->GRAY.TABLE[1].B_Gray)
			- (pSmart->GRAY.TABLE[index[V87_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[20] = (result_2  - v59_coefficient) & 0xff;

}

#define v35_coefficient 64
#define v35_denominator 320
static int v35_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	int add_mtp;
	int LSB;

	LSB = char_to_int(pSmart->MTP.R_OFFSET.OFFSET_35);
	add_mtp = LSB + V35_300CD_R;
	result_1 = (pSmart->RGB_OUTPUT.R_VOLTAGE.level_1)
			- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_59);
	result_2 = (v35_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v35_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.R_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_35 = result_4;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_35);
	add_mtp = LSB + V35_300CD_G;
	result_1 = (pSmart->RGB_OUTPUT.G_VOLTAGE.level_1)
			- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_59);
	result_2 = (v35_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v35_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.G_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_35 = result_4;

	LSB = char_to_int(pSmart->MTP.B_OFFSET.OFFSET_35);
	add_mtp = LSB + V35_300CD_B;
	result_1 = (pSmart->RGB_OUTPUT.B_VOLTAGE.level_1)
			- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_59);
	result_2 = (v35_coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v35_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.B_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_35 = result_4;

	#ifdef SMART_DIMMING_DEBUG
	printk("%s V35 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_35,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_35,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_35);
	#endif

	return 0;

}

static void v35_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = (pSmart->GRAY.TABLE[1].R_Gray)
			- (pSmart->GRAY.TABLE[index[V35_INDEX]].R_Gray);
	result_2 = result_1 * v35_denominator;
	result_3 = (pSmart->GRAY.TABLE[1].R_Gray)
			- (pSmart->GRAY.TABLE[index[V59_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[5] = (result_2 - v35_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.TABLE[1].G_Gray)
			- (pSmart->GRAY.TABLE[index[V35_INDEX]].G_Gray);
	result_2 = result_1 * v35_denominator;
	result_3 = (pSmart->GRAY.TABLE[1].G_Gray)
			- (pSmart->GRAY.TABLE[index[V59_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[13] = (result_2 - v35_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.TABLE[1].B_Gray)
			- (pSmart->GRAY.TABLE[index[V35_INDEX]].B_Gray);
	result_2 = result_1 * v35_denominator;
	result_3 = (pSmart->GRAY.TABLE[1].B_Gray)
			- (pSmart->GRAY.TABLE[index[V59_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[21] = (result_2 - v35_coefficient) & 0xff;

}

#define v15_Coefficient 20
#define v15_denominator 320
static int v15_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	int add_mtp;
	int LSB;

	LSB = char_to_int(pSmart->MTP.R_OFFSET.OFFSET_15);
	add_mtp = LSB + V15_300CD_R;
	result_1 = (pSmart->RGB_OUTPUT.R_VOLTAGE.level_1)
			- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_35);
	result_2 = (v15_Coefficient+add_mtp) << BIT_SHIFT;
	do_div(result_2, v15_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.R_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_15 = result_4;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_15);
	add_mtp = LSB + V15_300CD_G;
	result_1 = (pSmart->RGB_OUTPUT.G_VOLTAGE.level_1)
			- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_35);
	result_2 = (v15_Coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v15_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.G_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_15 = result_4;

	LSB = char_to_int(pSmart->MTP.B_OFFSET.OFFSET_15);
	add_mtp = LSB + V15_300CD_B;
	result_1 = (pSmart->RGB_OUTPUT.B_VOLTAGE.level_1)
			- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_35);
	result_2 = (v15_Coefficient + add_mtp) << BIT_SHIFT;
	do_div(result_2, v15_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.B_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_15 = result_4;

	#ifdef SMART_DIMMING_DEBUG
	printk("%s V15 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_15,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_15,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_15);
	#endif

	return 0;

}

static void v15_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = (pSmart->GRAY.TABLE[1].R_Gray)
			- (pSmart->GRAY.TABLE[index[V15_INDEX]].R_Gray);
	result_2 = result_1 * v15_denominator;
	result_3 = (pSmart->GRAY.TABLE[1].R_Gray)
			- (pSmart->GRAY.TABLE[index[V35_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[6] = (result_2  -  v15_Coefficient) & 0xff;

	result_1 = (pSmart->GRAY.TABLE[1].G_Gray)
			- (pSmart->GRAY.TABLE[index[V15_INDEX]].G_Gray);
	result_2 = result_1 * v15_denominator;
	result_3 = (pSmart->GRAY.TABLE[1].G_Gray)
			- (pSmart->GRAY.TABLE[index[V35_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[14] = (result_2  - v15_Coefficient) & 0xff;

	result_1 = (pSmart->GRAY.TABLE[1].B_Gray)
			- (pSmart->GRAY.TABLE[index[V15_INDEX]].B_Gray);
	result_2 = result_1 * v15_denominator;
	result_3 = (pSmart->GRAY.TABLE[1].B_Gray)
			- (pSmart->GRAY.TABLE[index[V35_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[22] = (result_2  - v15_Coefficient) & 0xff;
}

/*V0, V1,V15,V35,V59,V87,V171,V255*/
int EA8868_ARRAY[EA8868_MAX] = {0, 1, 15, 35, 59, 87, 171, 255};

int non_linear_V1toV15[] = {
	49, 44, 39, 34, 29,
	24, 20, 16, 12, 8,
	6, 4, 2
};
#define V1toV15_denominator 54

int non_linear_V15toV35[] = {
	132, 124, 116, 108, 100,
	92, 84, 76, 69, 62,
	55, 48, 42, 36, 30,
	24, 18, 12, 6,
};
#define V15toV35_denominator 140

#define V35toV59_Coefficient 23
#define V35toV59_Multiple 1
#define V35toV59_denominator 24

#define V59toV87_Coefficient 27
#define V59toV87_Multiple 1
#define V59toV87_denominator 28

#define V87toV171_Coefficient 83
#define V87toV171_Multiple 1
#define V87toV171_denominator 84

#define V171toV255_Coefficient 83
#define V171toV255_Multiple 1
#define V171toV255_denominator 84

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

static int cal_gray_scale_non_linear(int up, int low,
int *table, int deno, int cnt)
{
	unsigned long long result_1, result_2, result_3, result_4;

	result_1 = up - low;
	result_2 = (result_1 * (table[cnt])) << BIT_SHIFT;
	do_div(result_2, deno);
	result_3 = result_2 >> BIT_SHIFT;
	result_4 = low + result_3;

	return (int)result_4;
}

static int generate_gray_scale(struct SMART_DIM *pSmart)
{
	int cnt = 0, cal_cnt = 0;
	int array_index = 0;
	struct GRAY_VOLTAGE *ptable =
		(struct GRAY_VOLTAGE *)&(pSmart->GRAY.TABLE);

	for (cnt = 0; cnt < EA8868_MAX; cnt++) {
		pSmart->GRAY.TABLE[EA8868_ARRAY[cnt]].R_Gray =
			((int *)&(pSmart->RGB_OUTPUT.R_VOLTAGE))[cnt];

		pSmart->GRAY.TABLE[EA8868_ARRAY[cnt]].G_Gray =
			((int *)&(pSmart->RGB_OUTPUT.G_VOLTAGE))[cnt];

		pSmart->GRAY.TABLE[EA8868_ARRAY[cnt]].B_Gray =
			((int *)&(pSmart->RGB_OUTPUT.B_VOLTAGE))[cnt];
	}
	/*
		below codes use hard coded value.
		So it is possible to modify on each model.
		V0, V1, V15, V35, V59, V87, V171, V255
	*/
	for (cnt = 0; cnt < EA8868_GRAY_SCALE_MAX; cnt++) {

		if (cnt == EA8868_ARRAY[0]) {
			/* 0 */
			array_index = 0;
		} else if (cnt == EA8868_ARRAY[1]) {
			/* 1 */
			cal_cnt = 0;
		} else if ((cnt > EA8868_ARRAY[1]) &&
			(cnt < EA8868_ARRAY[2])) {
			/* 2 ~ 14 */
			array_index = 2;

			pSmart->GRAY.TABLE[cnt].R_Gray =
			cal_gray_scale_non_linear(
			ptable[EA8868_ARRAY[array_index-1]].R_Gray,
			ptable[EA8868_ARRAY[array_index]].R_Gray,
			non_linear_V1toV15, V1toV15_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray =
			cal_gray_scale_non_linear(
			ptable[EA8868_ARRAY[array_index-1]].G_Gray,
			ptable[EA8868_ARRAY[array_index]].G_Gray,
			non_linear_V1toV15, V1toV15_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray =
			cal_gray_scale_non_linear(
			ptable[EA8868_ARRAY[array_index-1]].B_Gray,
			ptable[EA8868_ARRAY[array_index]].B_Gray,
			non_linear_V1toV15, V1toV15_denominator, cal_cnt);

			cal_cnt++;
		} else if (cnt == EA8868_ARRAY[2]) {
			/* 15 */
			cal_cnt = 0;
		} else if ((cnt > EA8868_ARRAY[2]) &&
			(cnt < EA8868_ARRAY[3])) {
			/* 16 ~ 34 */
			array_index = 3;

			pSmart->GRAY.TABLE[cnt].R_Gray =
			cal_gray_scale_non_linear(
			ptable[EA8868_ARRAY[array_index-1]].R_Gray,
			ptable[EA8868_ARRAY[array_index]].R_Gray,
			non_linear_V15toV35, V15toV35_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray =
			cal_gray_scale_non_linear(
			ptable[EA8868_ARRAY[array_index-1]].G_Gray,
			ptable[EA8868_ARRAY[array_index]].G_Gray,
			non_linear_V15toV35, V15toV35_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray =
			cal_gray_scale_non_linear(
			ptable[EA8868_ARRAY[array_index-1]].B_Gray,
			ptable[EA8868_ARRAY[array_index]].B_Gray,
			non_linear_V15toV35, V15toV35_denominator, cal_cnt);

			cal_cnt++;
		}  else if (cnt == EA8868_ARRAY[3]) {
			/* 35 */
			cal_cnt = 0;
		} else if ((cnt > EA8868_ARRAY[3]) &&
			(cnt < EA8868_ARRAY[4])) {
			/* 35 ~ 58 */
			array_index = 4;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
			ptable[EA8868_ARRAY[array_index-1]].R_Gray,
			ptable[EA8868_ARRAY[array_index]].R_Gray,
			V35toV59_Coefficient, V35toV59_Multiple,
			V35toV59_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
			ptable[EA8868_ARRAY[array_index-1]].G_Gray,
			ptable[EA8868_ARRAY[array_index]].G_Gray,
			V35toV59_Coefficient, V35toV59_Multiple,
			V35toV59_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
			ptable[EA8868_ARRAY[array_index-1]].B_Gray,
			ptable[EA8868_ARRAY[array_index]].B_Gray,
			V35toV59_Coefficient, V35toV59_Multiple,
			V35toV59_denominator , cal_cnt);

			cal_cnt++;
		} else if (cnt == EA8868_ARRAY[4]) {
			/* 59 */
			cal_cnt = 0;
		} else if ((cnt > EA8868_ARRAY[4]) &&
			(cnt < EA8868_ARRAY[5])) {
			/* 60 ~ 86 */
			array_index = 5;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
			ptable[EA8868_ARRAY[array_index-1]].R_Gray,
			ptable[EA8868_ARRAY[array_index]].R_Gray,
			V59toV87_Coefficient, V59toV87_Multiple,
			V59toV87_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
			ptable[EA8868_ARRAY[array_index-1]].G_Gray,
			ptable[EA8868_ARRAY[array_index]].G_Gray,
			V59toV87_Coefficient, V59toV87_Multiple,
			V59toV87_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
			ptable[EA8868_ARRAY[array_index-1]].B_Gray,
			ptable[EA8868_ARRAY[array_index]].B_Gray,
			V59toV87_Coefficient, V59toV87_Multiple,
			V59toV87_denominator, cal_cnt);
			cal_cnt++;

		} else if (cnt == EA8868_ARRAY[5]) {
			/* 87 */
			cal_cnt = 0;
		} else if ((cnt > EA8868_ARRAY[5]) &&
			(cnt < EA8868_ARRAY[6])) {
			/* 88 ~ 170 */
			array_index = 6;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
			ptable[EA8868_ARRAY[array_index-1]].R_Gray,
			ptable[EA8868_ARRAY[array_index]].R_Gray,
			V87toV171_Coefficient, V87toV171_Multiple,
			V87toV171_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
			ptable[EA8868_ARRAY[array_index-1]].G_Gray,
			ptable[EA8868_ARRAY[array_index]].G_Gray,
			V87toV171_Coefficient, V87toV171_Multiple,
			V87toV171_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
			ptable[EA8868_ARRAY[array_index-1]].B_Gray,
			ptable[EA8868_ARRAY[array_index]].B_Gray,
			V87toV171_Coefficient, V87toV171_Multiple,
			V87toV171_denominator, cal_cnt);
			cal_cnt++;

		} else if (cnt == EA8868_ARRAY[6]) {
			/* 171 */
			cal_cnt = 0;
		} else if ((cnt > EA8868_ARRAY[6]) &&
			(cnt < EA8868_ARRAY[7])) {
			/* 172 ~ 254 */
			array_index = 7;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
			ptable[EA8868_ARRAY[array_index-1]].R_Gray,
			ptable[EA8868_ARRAY[array_index]].R_Gray,
			V171toV255_Coefficient, V171toV255_Multiple,
			V171toV255_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
			ptable[EA8868_ARRAY[array_index-1]].G_Gray,
			ptable[EA8868_ARRAY[array_index]].G_Gray,
			V171toV255_Coefficient, V171toV255_Multiple,
			V171toV255_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
			ptable[EA8868_ARRAY[array_index-1]].B_Gray,
			ptable[EA8868_ARRAY[array_index]].B_Gray,
			V171toV255_Coefficient, V171toV255_Multiple,
			V171toV255_denominator, cal_cnt);

			cal_cnt++;
		} else {
			if (cnt == EA8868_ARRAY[7]) {
				pr_info("%s end\n", __func__);
			} else {
				pr_err("%s fail cnt:%d\n", __func__, cnt);
				return -1;
			}
		}

	}

	#ifdef SMART_DIMMING_DEBUG
		for (cnt = 0; cnt < EA8868_GRAY_SCALE_MAX; cnt++) {
			printk("%s %d R:%d G:%d B:%d\n", __func__, cnt,
				pSmart->GRAY.TABLE[cnt].R_Gray,
				pSmart->GRAY.TABLE[cnt].G_Gray,
				pSmart->GRAY.TABLE[cnt].B_Gray);
		}
	#endif

	return 0;
}

int smart_dimming_init(struct SMART_DIM *smart_dim)
{

	v255_adjustment(smart_dim);
	v1_adjustment(smart_dim);
	v171_adjustment(smart_dim);
	v87_adjustment(smart_dim);
	v59_adjustment(smart_dim);
	v35_adjustment(smart_dim);
	v15_adjustment(smart_dim);

	if (generate_gray_scale(smart_dim)) {
		printk(KERN_ERR "lcd smart dimming fail generate_gray_scale\n");
		return -1;
	}

	return 0;
}

static int searching_function(long long candela, int *index)
{
	long long delta_1 = 0, delta_2 = 0;
	int cnt;

	/*
	*	This searching_functin should be changed with improved
		searcing algorithm to reduce searching time.
	*/
	*index = -1;

	for (cnt = 0; cnt < (EA8868_GRAY_SCALE_MAX-1); cnt++) {
		delta_1 = candela - curve_2p2[cnt];
		delta_2 = candela - curve_2p2[cnt+1];

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
		return -1;
	else
		return 0;
}

/* -1 means V1 */
#define EA8868_TABLE_MAX  (EA8868_MAX-1)
void(*Make_hexa[EA8868_TABLE_MAX])(int*, struct SMART_DIM*, char*) = {
	v255_hexa,
	v171_hexa,
	v87_hexa,
	v59_hexa,
	v35_hexa,
	v15_hexa,
	v1_hexa
};

void generate_gamma(struct SMART_DIM *pSmart, char *str, int size)
{
	long long candela_level[EA8868_TABLE_MAX] = {-1, };
	int bl_index[EA8868_TABLE_MAX] = {-1, };

	unsigned long long temp_cal_data = 0;
	int bl_level, cnt;

	bl_level = pSmart->brightness_level;

	/*calculate candela level */
	for (cnt = 0; cnt < EA8868_TABLE_MAX; cnt++) {
		temp_cal_data =
		((long long)(candela_coeff_2p2[EA8868_ARRAY[cnt+1]])) *
		((long long)(bl_level));
		candela_level[cnt] = temp_cal_data;
	}

	#ifdef SMART_DIMMING_DEBUG
	printk("%s candela_1:%llu candela_15:%llu  candela_35:%llu  "
		"candela_59:%llu  candela_171:%llu  candela_87:%llu "
		"candela_255:%llu\n", __func__, candela_level[0],
		candela_level[1], candela_level[2], candela_level[3],
		candela_level[4], candela_level[5], candela_level[6]);
	#endif

	/*calculate brightness level*/
	for (cnt = 0; cnt < EA8868_TABLE_MAX; cnt++) {
		if (searching_function(candela_level[cnt], &(bl_index[cnt]))) {
			pr_debug("%s searching functioin error cnt:%d\n",
				__func__, cnt);
		}
	}

	#ifdef SMART_DIMMING_DEBUG
	printk("%s bl_index_1:%d bl_index_15:%d bl_index_35:%d bl_index_59:%d "
		"bl_index_87:%d bl_index_171:%d bl_index_255:%d\n", __func__,
		bl_index[0], bl_index[1], bl_index[2], bl_index[3],
		bl_index[4], bl_index[5], bl_index[6]);
	#endif

	/*Generate Gamma table*/
	for (cnt = 0; cnt < EA8868_TABLE_MAX; cnt++){
		(void)Make_hexa[cnt](bl_index , pSmart, str);
	}

	/*subtration MTP_OFFSET value from generated gamma table*/
	str[1] -= pSmart->MTP.R_OFFSET.OFFSET_255_LSB;
	str[2] -= pSmart->MTP.R_OFFSET.OFFSET_171;
	str[3] -= pSmart->MTP.R_OFFSET.OFFSET_87;
	str[4] -= pSmart->MTP.R_OFFSET.OFFSET_59;
	str[5] -= pSmart->MTP.R_OFFSET.OFFSET_35;
	str[6] -= pSmart->MTP.R_OFFSET.OFFSET_15;

	str[9] -= pSmart->MTP.G_OFFSET.OFFSET_255_LSB;
	str[10] -= pSmart->MTP.G_OFFSET.OFFSET_171;
	str[11] -= pSmart->MTP.G_OFFSET.OFFSET_87;
	str[12] -= pSmart->MTP.G_OFFSET.OFFSET_59;
	str[13] -= pSmart->MTP.G_OFFSET.OFFSET_35;
	str[14] -= pSmart->MTP.G_OFFSET.OFFSET_15;

	str[17] -= pSmart->MTP.B_OFFSET.OFFSET_255_LSB;
	str[18] -= pSmart->MTP.B_OFFSET.OFFSET_171;
	str[19] -= pSmart->MTP.B_OFFSET.OFFSET_87;
	str[20] -= pSmart->MTP.B_OFFSET.OFFSET_59;
	str[21] -= pSmart->MTP.B_OFFSET.OFFSET_35;
	str[22] -= pSmart->MTP.B_OFFSET.OFFSET_15;

}
