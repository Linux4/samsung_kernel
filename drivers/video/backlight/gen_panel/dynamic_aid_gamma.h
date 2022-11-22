/*
 * =================================================================
 *
 *       Filename:  smart_mtp_s6e88a.h
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
#ifndef _DYNAMIC_AID_GAMMA_H_
#define _DYNAMIC_AID_GAMMA_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>
#include <asm/div64.h>

/*
 *	From 4.27 inch model use AID function
 *	CASE#1 is used for now.
 */
#define AID_OPERATION

#define GAMMA_CURVE_2P25 1
#define GAMMA_CURVE_2P2 2
#define GAMMA_CURVE_2P15 3
#define GAMMA_CURVE_2P1 4
#define GAMMA_CURVE_2P0 5
#define GAMMA_CURVE_1P9 6

#define MTP_START_ADDR 0xC8
#define LUMINANCE_MAX 62
#define GAMMA_SET_MAX 33
#define BIT_SHIFT 22
/*
   it means BIT_SHIFT is 22.  pow(2,BIT_SHIFT) is 4194304.
   BIT_SHIFT is used for right bit shfit
   */
#define BIT_SHFIT_MUL 4194304

#define GRAY_SCALE_MAX 256

/*6.3*4194304 */
#define VREG0_REF 26424115

/* PANEL DEPENDENT THINGS */
#define MAX_CANDELA 350
#define MIN_CANDELA 5

/* PANEL DEPENDENT THINGS END*/

enum {
	VOLT_VT,
	VOLT_V3,
	VOLT_V11,
	VOLT_V23,
	VOLT_V35,
	VOLT_V51,
	VOLT_V87,
	VOLT_V151,
	VOLT_V203,
	VOLT_V255,
	VOLT_MAX,
};

static char *volt_name[] = {
	"VT",
	"V3",
	"V11",
	"V23",
	"V35",
	"V51",
	"V87",
	"V151",
	"V203",
	"V255",
};

static int VT_CURVE[VOLT_MAX] = {
	0,
	3,
	11,
	23,
	35,
	51,
	87,
	151,
	203,
	255
};

enum {
	R_OFFSET,
	G_OFFSET,
	B_OFFSET,
	RGB_MAX_OFFSET,
};

struct GAMMA_LEVEL {
	int level_0;
	int level_3;
	int level_11;
	int level_23;
	int level_35;
	int level_51;
	int level_87;
	int level_151;
	int level_203;
	int level_255;
} __packed;

struct RGB_OUTPUT_VOLTARE {
	struct GAMMA_LEVEL R_VOLTAGE;
	struct GAMMA_LEVEL G_VOLTAGE;
	struct GAMMA_LEVEL B_VOLTAGE;
} __packed;

struct GRAY_VOLTAGE {
	/*
	   This voltage value use 14bit right shit
	   it means voltage is divied by 16384.
	   */
	int R_Gray;
	int G_Gray;
	int B_Gray;
} __packed;

struct GRAY_SCALE {
	struct GRAY_VOLTAGE TABLE[GRAY_SCALE_MAX];
	struct GRAY_VOLTAGE VT_TABLE;
} __packed;

/*V0,V1,V3,V11,V23,V35,V51,V87,V151,V203,V255*/

struct MTP_SET {
	char OFFSET_255_MSB;
	char OFFSET_255_LSB;
	char OFFSET_203;
	char OFFSET_151;
	char OFFSET_87;
	char OFFSET_51;
	char OFFSET_35;
	char OFFSET_23;
	char OFFSET_11;
	char OFFSET_3;
	char OFFSET_0;
} __packed;

struct HBM_REG {
	/* LSI */
	char b5_reg[16];	/*B5 13~28th reg*/
	char b6_reg[12];	/*B6 3~14th reg*/
	char b5_reg_19;		/*B5 19th reg*/
	char b6_reg_17;		/*B6 17th reg*/
	/* MAGNA */
} __packed;

struct MTP_OFFSET {
	struct MTP_SET R_OFFSET;
	struct MTP_SET G_OFFSET;
	struct MTP_SET B_OFFSET;
} __packed;

struct illuminance_table {
	int lux;
	char gamma_setting[GAMMA_SET_MAX];
} __packed;

struct coeff_t {
	int numerator;
	int denominator;
} __packed;

struct SMART_DIM {
	struct HBM_REG hbm_reg;
	struct MTP_OFFSET MTP_ORIGN;

	struct MTP_OFFSET MTP;
	struct RGB_OUTPUT_VOLTARE RGB_OUTPUT;
	struct GRAY_SCALE GRAY;

	/* Because of AID funtion, below members are added*/
	int lux_table_max;
	int *plux_table;
	int *rgb_offset;
	int *candela_offset;
	int *base_lux_table;
	int *gamma_curve_table;
	struct illuminance_table gen_table[LUMINANCE_MAX];

	int brightness_level;
	int ldi_revision;
	int vregout_voltage;
} __packed;
char* get_b5_reg(void);
char get_b5_reg_19(void);
char* get_b6_reg(void);
char get_b6_reg_17(void);

void get_min_lux_table(char *str, int size);
int smart_dimming_init(struct SMART_DIM *psmart);
void generate_gamma(struct SMART_DIM *smart_dim, char *str, int size);
#endif	/* _DYNAMIC_AID_GAMMA_H_ */
