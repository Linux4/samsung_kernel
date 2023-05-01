/*
 * =================================================================
 *
 *       Filename:  smart_mtp_s6e63m0.c
 *
 *    Description:  Smart dimming algorithm implementation
 *
 *        Author: D Debadas
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
#include "smart_mtp_s6e63m0-8930.h"
#include "smart_mtp_2p2_gamma_s6e63m0-8930.h"

/*#if defined(CONFIG_MACH_AEGIS2) 	|| defined(CONFIG_MACH_APEXQ)*/
#if 1 /*temp*/
/*
*	To support different center cell gamma setting
*/
unsigned char V1_300CD_R;
unsigned char V1_300CD_G;
unsigned char V1_300CD_B;

unsigned char V19_300CD_R;
unsigned char V19_300CD_G;
unsigned char V19_300CD_B;

unsigned char V43_300CD_R;
unsigned char V43_300CD_G;
unsigned char V43_300CD_B;

unsigned char V87_300CD_R;
unsigned char V87_300CD_G;
unsigned char V87_300CD_B;

unsigned char V171_300CD_R;
unsigned char V171_300CD_G;
unsigned char V171_300CD_B;

unsigned char V255_300CD_R;
unsigned char V255_300CD_G;
unsigned char V255_300CD_B;
#endif
/* #define SMART_DIMMING_DEBUG */


static int char_to_int(char data1)
{
	int cal_data;

	if (data1 & 0x80)
		cal_data = 0xffffff00 | data1;
	else
		cal_data = data1;

	return cal_data;
}

#define V255_COEFF 120
#define V255_DENOMTR 600
static int v255_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	int add_mtp;
	int LSB;

	LSB = char_to_int(pSmart->MTP.R_OFFSET.OFFSET_255_LSB);
	add_mtp = LSB + V255_300CD_R;
	result_1 = result_2 = (V255_COEFF+add_mtp) << BIT_SHIFT;
	do_div(result_2, V255_DENOMTR);
	result_3 = (S6E63M0_VREG0_REF * result_2) >> BIT_SHIFT;
	result_4 = S6E63M0_VREG0_REF - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_255 = result_4;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_0 = S6E63M0_VREG0_REF;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_255_LSB);
	add_mtp = LSB + V255_300CD_G;
	result_1 = result_2 = (V255_COEFF+add_mtp) << BIT_SHIFT;
	do_div(result_2, V255_DENOMTR);
	result_3 = (S6E63M0_VREG0_REF * result_2) >> BIT_SHIFT;
	result_4 = S6E63M0_VREG0_REF - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_255 = result_4;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_0 = S6E63M0_VREG0_REF;

	LSB = char_to_int(pSmart->MTP.B_OFFSET.OFFSET_255_LSB);
	add_mtp = LSB + V255_300CD_B;
	result_1 = result_2 = (V255_COEFF+add_mtp) << BIT_SHIFT;
	do_div(result_2, V255_DENOMTR);
	result_3 = (S6E63M0_VREG0_REF * result_2) >> BIT_SHIFT;
	result_4 = S6E63M0_VREG0_REF - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_255 = result_4;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_0 = S6E63M0_VREG0_REF;

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

	result_1 = S6E63M0_VREG0_REF -
		(pSmart->GRAY.TABLE[index[V255_INDEX]].R_Gray);

	result_2 = result_1 * V255_DENOMTR;
	do_div(result_2, S6E63M0_VREG0_REF);
	result_3 = result_2  - V255_COEFF;
	str[15] = (result_3 & 0xff00) >> 8;
	str[16] = result_3 & 0xff;

	result_1 = S6E63M0_VREG0_REF -
		(pSmart->GRAY.TABLE[index[V255_INDEX]].G_Gray);
	result_2 = result_1 * V255_DENOMTR;
	do_div(result_2, S6E63M0_VREG0_REF);
	result_3 = result_2  - V255_COEFF;
	str[17] = (result_3 & 0xff00) >> 8;
	str[18] = result_3 & 0xff;

	result_1 = S6E63M0_VREG0_REF -
			(pSmart->GRAY.TABLE[index[V255_INDEX]].B_Gray);
	result_2 = result_1 * V255_DENOMTR;
	do_div(result_2, S6E63M0_VREG0_REF);
	result_3 = result_2  - V255_COEFF;
	str[19] = (result_3 & 0xff00) >> 8;
	str[20] = result_3 & 0xff;
}

#define V1_COEFF 5
#define V1_DENOMTR 600
static int v1_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	int add_mtp;
	int LSB;

	LSB = char_to_int(pSmart->MTP.R_OFFSET.OFFSET_1);
	add_mtp = LSB + V1_300CD_R;
	result_1 = result_2 = (V1_COEFF + add_mtp) << BIT_SHIFT;
	do_div(result_2, V1_DENOMTR);
	result_3 = (S6E63M0_VREG0_REF * result_2) >> BIT_SHIFT;
	result_4 = S6E63M0_VREG0_REF - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_1 = result_4;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_1);
	add_mtp = LSB + V1_300CD_G;
	result_1 = result_2 = (V1_COEFF+add_mtp) << BIT_SHIFT;
	do_div(result_2, V1_DENOMTR);
	result_3 = (S6E63M0_VREG0_REF * result_2) >> BIT_SHIFT;
	result_4 = S6E63M0_VREG0_REF - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_1 = result_4;

	LSB = char_to_int(pSmart->MTP.B_OFFSET.OFFSET_1);
	add_mtp = LSB + V1_300CD_B;
	result_1 = result_2 = (V1_COEFF+add_mtp) << BIT_SHIFT;
	do_div(result_2, V1_DENOMTR);
	result_3 = (S6E63M0_VREG0_REF * result_2) >> BIT_SHIFT;
	result_4 = S6E63M0_VREG0_REF - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_1 = result_4;

	#ifdef SMART_DIMMING_DEBUG
	pr_info("%s V1 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_1,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_1,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_1);
	#endif

	return 0;

}

static void v1_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	str[0] =  pSmart->MTP.R_OFFSET.OFFSET_1 + V1_300CD_R;
	str[1] =  pSmart->MTP.G_OFFSET.OFFSET_1 + V1_300CD_G;
	str[2] =  pSmart->MTP.B_OFFSET.OFFSET_1 + V1_300CD_B;
}


#define V171_COEFF 65
#define V171_DENOMTR 320
static int v171_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	int add_mtp;
	int LSB;

	LSB = char_to_int(pSmart->MTP.R_OFFSET.OFFSET_171);
	add_mtp = LSB + V171_300CD_R;
	result_1 = (pSmart->RGB_OUTPUT.R_VOLTAGE.level_1)
				- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_255);
	result_2 = (V171_COEFF + add_mtp) << BIT_SHIFT;
	do_div(result_2, V171_DENOMTR);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.R_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_171 = result_4;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_171);
	add_mtp = LSB + V171_300CD_G;
	result_1 = (pSmart->RGB_OUTPUT.G_VOLTAGE.level_1)
				- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_255);
	result_2 = (V171_COEFF + add_mtp) << BIT_SHIFT;
	do_div(result_2, V171_DENOMTR);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.G_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_171 = result_4;

	LSB = char_to_int(pSmart->MTP.B_OFFSET.OFFSET_171);
	add_mtp = LSB + V171_300CD_B;
	result_1 = (pSmart->RGB_OUTPUT.B_VOLTAGE.level_1)
				- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_255);
	result_2 = (V171_COEFF+add_mtp) << BIT_SHIFT;
	do_div(result_2, V171_DENOMTR);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.B_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_171 = result_4;

	#ifdef SMART_DIMMING_DEBUG
	pr_info("%s V171 RED:%d GREEN:%d BLUE:%d\n", __func__,
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
	result_2 = result_1 * V171_DENOMTR;
	result_3 = (pSmart->GRAY.TABLE[1].R_Gray)
			- (pSmart->GRAY.TABLE[index[V255_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[12] = (result_2  - V171_COEFF) & 0xff;

	result_1 = (pSmart->GRAY.TABLE[1].G_Gray)
			- (pSmart->GRAY.TABLE[index[V171_INDEX]].G_Gray);
	result_2 = result_1 * V171_DENOMTR;
	result_3 = (pSmart->GRAY.TABLE[1].G_Gray)
			- (pSmart->GRAY.TABLE[index[V255_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[13] = (result_2  - V171_COEFF) & 0xff;

	result_1 = (pSmart->GRAY.TABLE[1].B_Gray)
			- (pSmart->GRAY.TABLE[index[V171_INDEX]].B_Gray);
	result_2 = result_1 * V171_DENOMTR;
	result_3 = (pSmart->GRAY.TABLE[1].B_Gray)
			- (pSmart->GRAY.TABLE[index[V255_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[14] = (result_2  - V171_COEFF) & 0xff;

}


#define V87_COEFF 65
#define V87_DENOMTR 320
static int v87_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	int add_mtp;
	int LSB;

	LSB = char_to_int(pSmart->MTP.R_OFFSET.OFFSET_87);
	add_mtp = LSB + V87_300CD_R;
	result_1 = (pSmart->RGB_OUTPUT.R_VOLTAGE.level_1)
			- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_171);
	result_2 = (V87_COEFF + add_mtp) << BIT_SHIFT;
	do_div(result_2, V87_DENOMTR);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.R_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_87 = result_4;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_87);
	add_mtp = LSB + V87_300CD_G;
	result_1 = (pSmart->RGB_OUTPUT.G_VOLTAGE.level_1)
			- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_171);
	result_2 = (V87_COEFF + add_mtp) << BIT_SHIFT;
	do_div(result_2, V87_DENOMTR);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.G_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_87 = result_4;

	LSB = char_to_int(pSmart->MTP.B_OFFSET.OFFSET_87);
	add_mtp = LSB + V87_300CD_B;
	result_1 = (pSmart->RGB_OUTPUT.B_VOLTAGE.level_1)
			- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_171);
	result_2 = (V87_COEFF + add_mtp) << BIT_SHIFT;
	do_div(result_2, V87_DENOMTR);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.B_VOLTAGE.level_1) - result_3;
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

	result_1 = (pSmart->GRAY.TABLE[1].R_Gray)
			- (pSmart->GRAY.TABLE[index[V87_INDEX]].R_Gray);
	result_2 = result_1 * V87_DENOMTR;
	result_3 = (pSmart->GRAY.TABLE[1].R_Gray)
			- (pSmart->GRAY.TABLE[index[V171_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[9] = (result_2  - V87_COEFF) & 0xff;

	result_1 = (pSmart->GRAY.TABLE[1].G_Gray)
			- (pSmart->GRAY.TABLE[index[V87_INDEX]].G_Gray);
	result_2 = result_1 * V87_DENOMTR;
	result_3 = (pSmart->GRAY.TABLE[1].G_Gray)
			- (pSmart->GRAY.TABLE[index[V171_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[10] = (result_2  - V87_COEFF) & 0xff;

	result_1 = (pSmart->GRAY.TABLE[1].B_Gray)
			- (pSmart->GRAY.TABLE[index[V87_INDEX]].B_Gray);
	result_2 = result_1 * V87_DENOMTR;
	result_3 = (pSmart->GRAY.TABLE[1].B_Gray)
			- (pSmart->GRAY.TABLE[index[V171_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[11] = (result_2  - V87_COEFF) & 0xff;
}

#define V43_COEFF 65
#define V43_DENOMTR 320
static int v43_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	int add_mtp;
	int LSB;

	LSB = char_to_int(pSmart->MTP.R_OFFSET.OFFSET_43);
	add_mtp = LSB + V43_300CD_R;
	result_1 = (pSmart->RGB_OUTPUT.R_VOLTAGE.level_1)
			- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_87);
	result_2 = (V43_COEFF + add_mtp) << BIT_SHIFT;
	do_div(result_2, V43_DENOMTR);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.R_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_43 = result_4;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_43);
	add_mtp = LSB + V43_300CD_G;
	result_1 = (pSmart->RGB_OUTPUT.G_VOLTAGE.level_1)
			- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_87);
	result_2 = (V43_COEFF + add_mtp) << BIT_SHIFT;
	do_div(result_2, V43_DENOMTR);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.G_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_43 = result_4;

	LSB = char_to_int(pSmart->MTP.B_OFFSET.OFFSET_43);
	add_mtp = LSB + V43_300CD_B;
	result_1 = (pSmart->RGB_OUTPUT.B_VOLTAGE.level_1)
			- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_87);
	result_2 = (V43_COEFF + add_mtp) << BIT_SHIFT;
	do_div(result_2, V43_DENOMTR);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.B_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_43 = result_4;

	#ifdef SMART_DIMMING_DEBUG
	pr_info("%s V43 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_43,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_43,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_43);
	#endif

	return 0;

}


static void v43_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = (pSmart->GRAY.TABLE[1].R_Gray)
			- (pSmart->GRAY.TABLE[index[V43_INDEX]].R_Gray);
	result_2 = result_1 * V43_DENOMTR;
	result_3 = (pSmart->GRAY.TABLE[1].R_Gray)
			- (pSmart->GRAY.TABLE[index[V87_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[6] = (result_2  - V43_COEFF) & 0xff;

	result_1 = (pSmart->GRAY.TABLE[1].G_Gray)
			- (pSmart->GRAY.TABLE[index[V43_INDEX]].G_Gray);
	result_2 = result_1 * V43_DENOMTR;
	result_3 = (pSmart->GRAY.TABLE[1].G_Gray)
			- (pSmart->GRAY.TABLE[index[V87_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[7] = (result_2  - V43_COEFF) & 0xff;

	result_1 = (pSmart->GRAY.TABLE[1].B_Gray)
			- (pSmart->GRAY.TABLE[index[V43_INDEX]].B_Gray);
	result_2 = result_1 * V43_DENOMTR;
	result_3 = (pSmart->GRAY.TABLE[1].B_Gray)
			- (pSmart->GRAY.TABLE[index[V87_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[8] = (result_2  - V43_COEFF) & 0xff;

}


#define V19_COEFF 65
#define V19_DENOMTR 320
static int v19_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	int add_mtp;
	int LSB;

	LSB = char_to_int(pSmart->MTP.R_OFFSET.OFFSET_19);
	add_mtp = LSB + V19_300CD_R;
	result_1 = (pSmart->RGB_OUTPUT.R_VOLTAGE.level_1)
			- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_43);
	result_2 = (V19_COEFF+add_mtp) << BIT_SHIFT;
	do_div(result_2, V19_DENOMTR);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.R_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_19 = result_4;

	LSB = char_to_int(pSmart->MTP.G_OFFSET.OFFSET_19);
	add_mtp = LSB + V19_300CD_G;
	result_1 = (pSmart->RGB_OUTPUT.G_VOLTAGE.level_1)
			- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_43);
	result_2 = (V19_COEFF + add_mtp) << BIT_SHIFT;
	do_div(result_2, V19_DENOMTR);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.G_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_19 = result_4;

	LSB = char_to_int(pSmart->MTP.B_OFFSET.OFFSET_19);
	add_mtp = LSB + V19_300CD_B;
	result_1 = (pSmart->RGB_OUTPUT.B_VOLTAGE.level_1)
			- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_43);
	result_2 = (V19_COEFF + add_mtp) << BIT_SHIFT;
	do_div(result_2, V19_DENOMTR);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->RGB_OUTPUT.B_VOLTAGE.level_1) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_19 = result_4;

	#ifdef SMART_DIMMING_DEBUG
	pr_info("%s V19 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_19,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_19,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_19);
	#endif

	return 0;

}

static void v19_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = (pSmart->GRAY.TABLE[1].R_Gray)
			- (pSmart->GRAY.TABLE[index[V19_INDEX]].R_Gray);
	result_2 = result_1 * V19_DENOMTR;
	result_3 = (pSmart->GRAY.TABLE[1].R_Gray)
			- (pSmart->GRAY.TABLE[index[V43_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[3] = (result_2  - V19_COEFF) & 0xff;

	result_1 = (pSmart->GRAY.TABLE[1].G_Gray)
			- (pSmart->GRAY.TABLE[index[V19_INDEX]].G_Gray);
	result_2 = result_1 * V19_DENOMTR;
	result_3 = (pSmart->GRAY.TABLE[1].G_Gray)
			- (pSmart->GRAY.TABLE[index[V43_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[4] = (result_2  - V19_COEFF) & 0xff;

	result_1 = (pSmart->GRAY.TABLE[1].B_Gray)
			- (pSmart->GRAY.TABLE[index[V19_INDEX]].B_Gray);
	result_2 = result_1 * V19_DENOMTR;
	result_3 = (pSmart->GRAY.TABLE[1].B_Gray)
			- (pSmart->GRAY.TABLE[index[V43_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[5] = (result_2  - V19_COEFF) & 0xff;
}

/*V0, V1,V19,V43,V87,V171,V255*/
int S6E63M0_ARRAY[S6E63M0_MAX] = {0, 1, 19, 43, 87, 171, 255};

int non_linear_V1toV19[] = {
75, 69, 63, 57, 51, 46,
41, 36, 31, 27, 23, 19,
15, 14, 9, 6, 3};
#define V1TOV19_DENOMTR 81

int non_linear_V19toV43[] = {
101, 94, 87, 80, 74, 68, 62,
56, 51, 46, 41, 36, 32, 28, 24,
20, 17, 14, 11, 8, 6, 4, 2,
};
#define V19TOV43_DENOMTR 108


#define V43TOV87_COEFF 43
#define V43TOV87_MULITPLE 1
#define V43TOV87_DENOMTR 44

#define V87TOV171_COEFF 83
#define V87TOV171_MULITPLE 1
#define V87TOV171_DENOMTR 84

#define V171TOV255_COEFF 83
#define V171TOV255_MULITPLE 1
#define V171TOV255_DENOMTR 84

int cal_gray_scale_linear(int up, int low, int coeff,
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

	for (cnt = 0; cnt < S6E63M0_MAX; cnt++) {
		pSmart->GRAY.TABLE[S6E63M0_ARRAY[cnt]].R_Gray =
			((int *)&(pSmart->RGB_OUTPUT.R_VOLTAGE))[cnt];

		pSmart->GRAY.TABLE[S6E63M0_ARRAY[cnt]].G_Gray =
			((int *)&(pSmart->RGB_OUTPUT.G_VOLTAGE))[cnt];

		pSmart->GRAY.TABLE[S6E63M0_ARRAY[cnt]].B_Gray =
			((int *)&(pSmart->RGB_OUTPUT.B_VOLTAGE))[cnt];
	}

	/*
		below codes use hard coded value.
		So it is possible to modify on each model.
		V0, V1,V19,V43,V87,V171,V255
	*/
	for (cnt = 0; cnt < S6E63M0_GRAY_SCALE_MAX; cnt++) {

		if (cnt == S6E63M0_ARRAY[0]) {
			/* 0 */
			array_index = 0;
		} else if (cnt == S6E63M0_ARRAY[1]) {
			/* 1 */
			cal_cnt = 0;
		} else if ((cnt > S6E63M0_ARRAY[1])
				&& (cnt < S6E63M0_ARRAY[2])) {
			/* 2 ~ 18 */
			array_index = 2;

			pSmart->GRAY.TABLE[cnt].R_Gray =
				cal_gray_scale_non_linear(
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index-1]].R_Gray,
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index]].R_Gray,
				non_linear_V1toV19, V1TOV19_DENOMTR,
				cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray =
				cal_gray_scale_non_linear(
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index-1]].G_Gray,
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index]].G_Gray,
				non_linear_V1toV19, V1TOV19_DENOMTR,
				cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray =
				cal_gray_scale_non_linear(
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index-1]].B_Gray,
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index]].B_Gray,
				non_linear_V1toV19, V1TOV19_DENOMTR,
				cal_cnt);

			cal_cnt++;
		} else if (cnt == S6E63M0_ARRAY[2]) {
			/* 19 */
			cal_cnt = 0;
		} else if ((cnt > S6E63M0_ARRAY[2])
				&& (cnt < S6E63M0_ARRAY[3])) {
			/* 20 ~ 42 */
			array_index = 3;

			pSmart->GRAY.TABLE[cnt].R_Gray =
				cal_gray_scale_non_linear(
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index-1]].R_Gray,
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index]].R_Gray,
				non_linear_V19toV43, V19TOV43_DENOMTR,
				cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray =
				cal_gray_scale_non_linear(
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index-1]].G_Gray,
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index]].G_Gray,
				non_linear_V19toV43, V19TOV43_DENOMTR,
				cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray =
				cal_gray_scale_non_linear(
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index-1]].B_Gray,
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index]].B_Gray,
				non_linear_V19toV43, V19TOV43_DENOMTR,
				cal_cnt);

			cal_cnt++;
		}  else if (cnt == S6E63M0_ARRAY[3]) {
			/* 43 */
			cal_cnt = 0;
		} else if ((cnt > S6E63M0_ARRAY[3])
				&& (cnt < S6E63M0_ARRAY[4])) {
			/* 44 ~ 86 */
			array_index = 4;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index-1]].R_Gray,
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index]].R_Gray,
				V43TOV87_COEFF, V43TOV87_MULITPLE,
				V43TOV87_DENOMTR, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index-1]].G_Gray,
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index]].G_Gray,
				V43TOV87_COEFF, V43TOV87_MULITPLE,
				V43TOV87_DENOMTR, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index-1]].B_Gray,
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index]].B_Gray,
				V43TOV87_COEFF, V43TOV87_MULITPLE,
				V43TOV87_DENOMTR , cal_cnt);

			cal_cnt++;
		} else if (cnt == S6E63M0_ARRAY[4]) {
			/* 87 */
			cal_cnt = 0;
		} else if ((cnt > S6E63M0_ARRAY[4])
				&& (cnt < S6E63M0_ARRAY[5])) {
			/* 88 ~ 170 */
			array_index = 5;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index-1]].R_Gray,
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
				[array_index]].R_Gray,
				V87TOV171_COEFF, V87TOV171_MULITPLE,
				V87TOV171_DENOMTR, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index-1]].G_Gray,
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
				[array_index]].G_Gray,
				V87TOV171_COEFF, V87TOV171_MULITPLE,
				V87TOV171_DENOMTR, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index-1]].B_Gray,
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index]].B_Gray,
				V87TOV171_COEFF, V87TOV171_MULITPLE,
				V87TOV171_DENOMTR, cal_cnt);
			cal_cnt++;

		} else if (cnt == S6E63M0_ARRAY[5]) {
			/* 171 */
			cal_cnt = 0;
		} else if ((cnt > S6E63M0_ARRAY[5])
				&& (cnt < S6E63M0_ARRAY[6])) {
			/* 172 ~ 254 */
			array_index = 6;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index-1]].R_Gray,
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index]].R_Gray,
				V171TOV255_COEFF, V171TOV255_MULITPLE,
				V171TOV255_DENOMTR, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index-1]].G_Gray,
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index]].G_Gray,
				V171TOV255_COEFF, V171TOV255_MULITPLE,
				V171TOV255_DENOMTR, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index-1]].B_Gray,
				pSmart->GRAY.TABLE[S6E63M0_ARRAY
					[array_index]].B_Gray,
				V171TOV255_COEFF, V171TOV255_MULITPLE,
				V171TOV255_DENOMTR, cal_cnt);

			cal_cnt++;
		} else {
			if (cnt == S6E63M0_ARRAY[6]) {
				pr_info("%s end\n", __func__);
			} else {
				pr_info(KERN_ERR "%s fail cnt:%d\n",
					__func__, cnt);
				return -1;
			}
		}

	}

	#ifdef SMART_DIMMING_DEBUG
		for (cnt = 0; cnt < S6E63M0_GRAY_SCALE_MAX; cnt++) {
			pr_info("%s %d R:%d G:%d B:%d\n", __func__, cnt,
				pSmart->GRAY.TABLE[cnt].R_Gray,
				pSmart->GRAY.TABLE[cnt].G_Gray,
				pSmart->GRAY.TABLE[cnt].B_Gray);
		}
	#endif
	return 0;
}

#if defined(CONFIG_MACH_GOLDEN)
static void gamma_cell_determine(int ldi_id)
{
	pr_info("%s Panel type : %s", __func__,
		(((ldi_id & 0x0000FF00) >> 8) == 0xB6) ? "SM2" : "M2");

		V1_300CD_R = V1_300CD_R_SM2;
		V1_300CD_G = V1_300CD_G_SM2;
		V1_300CD_B = V1_300CD_B_SM2;

		V19_300CD_R = V19_300CD_R_SM2;
		V19_300CD_G = V19_300CD_G_SM2;
		V19_300CD_B = V19_300CD_B_SM2;

		V43_300CD_R = V43_300CD_R_SM2;
		V43_300CD_G = V43_300CD_G_SM2;
		V43_300CD_B = V43_300CD_B_SM2;

		V87_300CD_R = V87_300CD_R_SM2;
		V87_300CD_G = V87_300CD_G_SM2;
		V87_300CD_B = V87_300CD_B_SM2;

		V171_300CD_R = V171_300CD_R_SM2;
		V171_300CD_G = V171_300CD_G_SM2;
		V171_300CD_B = V171_300CD_B_SM2;

		V255_300CD_R = V255_300CD_R_SM2;
		V255_300CD_G = V255_300CD_G_SM2;
		V255_300CD_B = V255_300CD_B_SM2;

}

#elif defined(CONFIG_MACH_AEGIS2)
static void gamma_cell_determine(int hw_revision)
{
	pr_info("%s HW_revision:%d", __func__, hw_revision);

	if (hw_revision >= 9) {
		/* HW REVISIOIN >= 9 */
		V1_300CD_R = V1_300CD_R_GE9;
		V1_300CD_G = V1_300CD_G_GE9;
		V1_300CD_B = V1_300CD_B_GE9;

		V19_300CD_R = V19_300CD_R_GE9;
		V19_300CD_G = V19_300CD_G_GE9;
		V19_300CD_B = V19_300CD_B_GE9;

		V43_300CD_R = V43_300CD_R_GE9;
		V43_300CD_G = V43_300CD_G_GE9;
		V43_300CD_B = V43_300CD_B_GE9;

		V87_300CD_R = V87_300CD_R_GE9;
		V87_300CD_G = V87_300CD_G_GE9;
		V87_300CD_B = V87_300CD_B_GE9;

		V171_300CD_R = V171_300CD_R_GE9;
		V171_300CD_G = V171_300CD_G_GE9;
		V171_300CD_B = V171_300CD_B_GE9;

		V255_300CD_R = V255_300CD_R_GE9;
		V255_300CD_G = V255_300CD_G_GE9;
		V255_300CD_B = V255_300CD_B_GE9;
	} else if (hw_revision >= 4) {
		/* HW REVISON >= 4*/
		V1_300CD_R = V1_300CD_R_GE4;
		V1_300CD_G = V1_300CD_G_GE4;
		V1_300CD_B = V1_300CD_B_GE4;

		V19_300CD_R = V19_300CD_R_GE4;
		V19_300CD_G = V19_300CD_G_GE4;
		V19_300CD_B = V19_300CD_B_GE4;

		V43_300CD_R = V43_300CD_R_GE4;
		V43_300CD_G = V43_300CD_G_GE4;
		V43_300CD_B = V43_300CD_B_GE4;

		V87_300CD_R = V87_300CD_R_GE4;
		V87_300CD_G = V87_300CD_G_GE4;
		V87_300CD_B = V87_300CD_B_GE4;

		V171_300CD_R = V171_300CD_R_GE4;
		V171_300CD_G = V171_300CD_G_GE4;
		V171_300CD_B = V171_300CD_B_GE4;

		V255_300CD_R = V255_300CD_R_GE4;
		V255_300CD_G = V255_300CD_G_GE4;
		V255_300CD_B = V255_300CD_B_GE4;
	} else {
		V1_300CD_R = V1_300CD_R_LE3;
		V1_300CD_G = V1_300CD_G_LE3;
		V1_300CD_B = V1_300CD_B_LE3;

		V19_300CD_R = V19_300CD_R_LE3;
		V19_300CD_G = V19_300CD_G_LE3;
		V19_300CD_B = V19_300CD_B_LE3;

		V43_300CD_R = V43_300CD_R_LE3;
		V43_300CD_G = V43_300CD_G_LE3;
		V43_300CD_B = V43_300CD_B_LE3;

		V87_300CD_R = V87_300CD_R_LE3;
		V87_300CD_G = V87_300CD_G_LE3;
		V87_300CD_B = V87_300CD_B_LE3;

		V171_300CD_R = V171_300CD_R_LE3;
		V171_300CD_G = V171_300CD_G_LE3;
		V171_300CD_B = V171_300CD_B_LE3;

		V255_300CD_R = V255_300CD_R_LE3;
		V255_300CD_G = V255_300CD_G_LE3;
		V255_300CD_B = V255_300CD_B_LE3;
	}
}
#elif defined(CONFIG_MACH_APEXQ)
static void gamma_cell_determine(int ldi_id)
{
	pr_info("%s Panel type : %s", __func__,
		(((ldi_id & 0x0000FF00) >> 8) == 0xB4) ? "SM2" : "M2");

	if (((ldi_id & 0x0000FF00) >> 8) == 0xB4 || \
		((ldi_id & 0x0000FF00) >> 8) == 0xB6) {
		V1_300CD_R = V1_300CD_R_SM2;
		V1_300CD_G = V1_300CD_G_SM2;
		V1_300CD_B = V1_300CD_B_SM2;

		V19_300CD_R = V19_300CD_R_SM2;
		V19_300CD_G = V19_300CD_G_SM2;
		V19_300CD_B = V19_300CD_B_SM2;

		V43_300CD_R = V43_300CD_R_SM2;
		V43_300CD_G = V43_300CD_G_SM2;
		V43_300CD_B = V43_300CD_B_SM2;

		V87_300CD_R = V87_300CD_R_SM2;
		V87_300CD_G = V87_300CD_G_SM2;
		V87_300CD_B = V87_300CD_B_SM2;

		V171_300CD_R = V171_300CD_R_SM2;
		V171_300CD_G = V171_300CD_G_SM2;
		V171_300CD_B = V171_300CD_B_SM2;

		V255_300CD_R = V255_300CD_R_SM2;
		V255_300CD_G = V255_300CD_G_SM2;
		V255_300CD_B = V255_300CD_B_SM2;

	} else {
		V1_300CD_R = V1_300CD_R_M2;
		V1_300CD_G = V1_300CD_G_M2;
		V1_300CD_B = V1_300CD_B_M2;

		V19_300CD_R = V19_300CD_R_M2;
		V19_300CD_G = V19_300CD_G_M2;
		V19_300CD_B = V19_300CD_B_M2;

		V43_300CD_R = V43_300CD_R_M2;
		V43_300CD_G = V43_300CD_G_M2;
		V43_300CD_B = V43_300CD_B_M2;

		V87_300CD_R = V87_300CD_R_M2;
		V87_300CD_G = V87_300CD_G_M2;
		V87_300CD_B = V87_300CD_B_M2;

		V171_300CD_R = V171_300CD_R_M2;
		V171_300CD_G = V171_300CD_G_M2;
		V171_300CD_B = V171_300CD_B_M2;

		V255_300CD_R = V255_300CD_R_M2;
		V255_300CD_G = V255_300CD_G_M2;
		V255_300CD_B = V255_300CD_B_M2;
	}
}

#endif

int smart_dimming_init(struct SMART_DIM *smart_dim)
{

#if defined(CONFIG_MACH_AEGIS2)
	gamma_cell_determine(system_rev);
#elif defined(CONFIG_MACH_APEXQ) || defined(CONFIG_MACH_GOLDEN)
	gamma_cell_determine(get_lcd_manufacture_id());

#endif

	v255_adjustment(smart_dim);
	v1_adjustment(smart_dim);
	v171_adjustment(smart_dim);
	v87_adjustment(smart_dim);
	v43_adjustment(smart_dim);
	v19_adjustment(smart_dim);

	if (generate_gray_scale(smart_dim)) {
		printk(KERN_ERR "lcd smart dimming fail Generate_gray_scale\n");
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

	for (cnt = 0; cnt < (S6E63M0_GRAY_SCALE_MAX-1); cnt++) {
		delta_1 = candela - s6e63m0_curve_2p2[cnt];
		delta_2 = candela - s6e63m0_curve_2p2[cnt+1];

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
#define S6E63M0_TABLE_MAX  (S6E63M0_MAX-1)
void (*make_hexa[S6E63M0_TABLE_MAX])(int*, struct SMART_DIM*, char*) = {
	v255_hexa,
	v171_hexa,
	v87_hexa,
	v43_hexa,
	v19_hexa,
	v1_hexa
};

void generate_gamma(struct SMART_DIM *pSmart, char *str, int size)
{
	long long candela_level[S6E63M0_TABLE_MAX] = {-1, -1, -1, -1, -1, -1};
	int bl_index[S6E63M0_TABLE_MAX] = {-1 , -1, -1, -1, -1, -1};

	unsigned long long temp_cal_data = 0;
	int bl_level, cnt;
	int level_255_temp = 0;

	bl_level = pSmart->brightness_level;
	/*calculate candela level */
	for (cnt = 0; cnt < S6E63M0_TABLE_MAX; cnt++) {
		temp_cal_data =
		((long long)(s6e63m0_candela_coeff[S6E63M0_ARRAY[cnt+1]])) *
		((long long)(bl_level));
		candela_level[cnt] = temp_cal_data;
	}

	#ifdef SMART_DIMMING_DEBUG
	pr_info("%s candela_1:%llu candela_19:%llu  candela_43:%llu  "
		"candela_87:%llu  candela_171:%llu  candela_255:%llu\n",
		__func__, candela_level[0], candela_level[1], candela_level[2],
		candela_level[3], candela_level[4], candela_level[5]);
	#endif

	/*calculate brightness level*/
	for (cnt = 0; cnt < S6E63M0_TABLE_MAX; cnt++)
		if (searching_function(candela_level[cnt], &(bl_index[cnt])))
			pr_info("%s searching functioin error cnt:%d\n",
				__func__, cnt);

	#ifdef SMART_DIMMING_DEBUG
	pr_info("%s bl_index_1:%d bl_index_19:%d bl_index_43:%d "
		"bl_index_87:%d bl_index_171:%d bl_index_255:%d\n", __func__,
		 bl_index[0], bl_index[1], bl_index[2],
		 bl_index[3], bl_index[4], bl_index[5]);
	#endif

	/*Generate Gamma table*/
	for (cnt = 0; cnt < S6E63M0_TABLE_MAX; cnt++)
		(void)make_hexa[cnt](bl_index , pSmart, str);

	/*subtration MTP_OFFSET value from generated gamma table*/
	str[3] -= pSmart->MTP.R_OFFSET.OFFSET_19;
	str[4] -= pSmart->MTP.G_OFFSET.OFFSET_19;
	str[5] -= pSmart->MTP.B_OFFSET.OFFSET_19;

	str[6] -= pSmart->MTP.R_OFFSET.OFFSET_43;
	str[7] -= pSmart->MTP.G_OFFSET.OFFSET_43;
	str[8] -= pSmart->MTP.B_OFFSET.OFFSET_43;

	str[9] -= pSmart->MTP.R_OFFSET.OFFSET_87;
	str[10] -= pSmart->MTP.G_OFFSET.OFFSET_87;
	str[11] -= pSmart->MTP.B_OFFSET.OFFSET_87;

	str[12] -= pSmart->MTP.R_OFFSET.OFFSET_171;
	str[13] -= pSmart->MTP.G_OFFSET.OFFSET_171;
	str[14] -= pSmart->MTP.B_OFFSET.OFFSET_171;

	level_255_temp = (str[15] << 8) | str[16] ;
	level_255_temp -=  pSmart->MTP.R_OFFSET.OFFSET_255_LSB;
	if (level_255_temp > 0xff) {
		str[15] = 0;
		str[16] = 0xff;
	} else {
		str[15] = 0;
		str[16] = level_255_temp & 0xff;
	}

	level_255_temp = (str[17] << 8) | str[18] ;
	level_255_temp -=  pSmart->MTP.G_OFFSET.OFFSET_255_LSB;
	if (level_255_temp > 0xff) {
		str[17] = 0;
		str[18] = 0xff;
	} else {
		str[17] = 0;
		str[18] = level_255_temp & 0xff;
	}

	level_255_temp = (str[19] << 8) | str[20] ;
	level_255_temp -=  pSmart->MTP.B_OFFSET.OFFSET_255_LSB;
	if (level_255_temp > 0xff) {
		str[19] = 0;
		str[20] = 0xff;
	} else {
		str[19] = 0;
		str[20] = level_255_temp & 0xff;
	}
}
