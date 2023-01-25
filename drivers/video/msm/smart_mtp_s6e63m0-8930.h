/*
 * =================================================================
 *
 *       Filename:  smart_mtp_s6e63m0.h
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
#ifndef _SMART_MTP_S6E63M0_H_
#define _SMART_MTP_S6E63M0_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>
#include <asm/div64.h>

#define GAMMA_SET_MAX 21

#define BIT_SHIFT 14
/*
	it means BIT_SHIFT is 14.  pow(2,BIT_SHIFT) is 16384.
	BIT_SHIFT is used for right bit shfit
*/
#define BIT_SHFIT_MUL 16384

#define S6E63M0_GRAY_SCALE_MAX 256

/*4.2*16384  4.5 is VREG1_REF value */
#define S6E63M0_VREG0_REF 68813

/*V0,V1,V19,V43,V87,V171,V255*/
#define S6E63M0_MAX 7
/* PANEL DEPENDET THINGS */

#if defined(CONFIG_MACH_GOLDEN)
/*
*	 SM2 PANEL
*/
#define V1_300CD_R_SM2 0x31
#define V1_300CD_G_SM2 0x00
#define V1_300CD_B_SM2 0x4F

#define V19_300CD_R_SM2 0x14
#define V19_300CD_G_SM2 0x6E
#define V19_300CD_B_SM2 0x02

#define V43_300CD_R_SM2 0xA3
#define V43_300CD_G_SM2 0xC0
#define V43_300CD_B_SM2 0x92

#define V87_300CD_R_SM2 0xA4
#define V87_300CD_G_SM2 0xBA
#define V87_300CD_B_SM2 0x93

#define V171_300CD_R_SM2 0xBD
#define V171_300CD_G_SM2 0xC8
#define V171_300CD_B_SM2 0xAF

#define V255_300CD_R_SM2 0xB0
#define V255_300CD_G_SM2 0xA2
#define V255_300CD_B_SM2 0xD1

/*
*	NEW M2 PANEL
*/
#define V1_300CD_R_M2 0x18
#define V1_300CD_G_M2 0x08
#define V1_300CD_B_M2 0x24

#define V19_300CD_R_M2 0x3A
#define V19_300CD_G_M2 0x48
#define V19_300CD_B_M2 0x16

#define V43_300CD_R_M2 0xB0
#define V43_300CD_G_M2 0xB7
#define V43_300CD_B_M2 0xA4

#define V87_300CD_R_M2 0xAA
#define V87_300CD_G_M2 0xB3
#define V87_300CD_B_M2 0x9E

#define V171_300CD_R_M2 0xC1
#define V171_300CD_G_M2 0xC3
#define V171_300CD_B_M2 0xB5

#define V255_300CD_R_M2 0xB1
#define V255_300CD_G_M2 0xAE
#define V255_300CD_B_M2 0xF3


#elif defined(CONFIG_MACH_AEGIS2)
/*
*	OLD M2 PANEL
*	HW Ver <=3,OLD,M2 PANEL, LE3=Less/Equal Than HW 3
*/
#define V1_300CD_R_LE3  0x18
#define V1_300CD_G_LE3  0x08
#define V1_300CD_B_LE3  0x24

#define V19_300CD_R_LE3  0x70
#define V19_300CD_G_LE3  0x6E
#define V19_300CD_B_LE3  0x4E

#define V43_300CD_R_LE3  0xBC
#define V43_300CD_G_LE3  0xC0
#define V43_300CD_B_LE3  0xAF

#define V87_300CD_R_LE3  0xB3
#define V87_300CD_G_LE3  0xB8
#define V87_300CD_B_LE3  0xA5

#define V171_300CD_R_LE3  0xC5
#define V171_300CD_G_LE3  0xC7
#define V171_300CD_B_LE3  0xBB

#define V255_300CD_R_LE3  0xB9
#define V255_300CD_G_LE3  0xBB
#define V255_300CD_B_LE3  0xFC

/*
*	NEW M2 PANEL
*	HW Ver>=.4,NEW M2 PANEL , GE4 =Greater /Equal HW 4
*/
#define V1_300CD_R_GE4  0x18
#define V1_300CD_G_GE4  0x08
#define V1_300CD_B_GE4  0x24

#define V19_300CD_R_GE4  0x3A
#define V19_300CD_G_GE4  0x48
#define V19_300CD_B_GE4  0x16

#define V43_300CD_R_GE4  0xB0
#define V43_300CD_G_GE4  0xB7
#define V43_300CD_B_GE4  0xA4

#define V87_300CD_R_GE4  0xAA
#define V87_300CD_G_GE4  0xB3
#define V87_300CD_B_GE4  0x9E

#define V171_300CD_R_GE4  0xC1
#define V171_300CD_G_GE4  0xC3
#define V171_300CD_B_GE4  0xB5

#define V255_300CD_R_GE4  0xB1
#define V255_300CD_G_GE4  0xAE
#define V255_300CD_B_GE4  0xF3


/*
*	 SM2 PANEL
*	 HW ver >= 9, SM2 PANEL, GE9 = Greater /Equal HW 9
*/
#define V1_300CD_R_GE9 0x31
#define V1_300CD_G_GE9 0x00
#define V1_300CD_B_GE9 0x4F

#define V19_300CD_R_GE9 0x14
#define V19_300CD_G_GE9 0x6E
#define V19_300CD_B_GE9 0x00

#define V43_300CD_R_GE9 0xA3
#define V43_300CD_G_GE9 0xC0
#define V43_300CD_B_GE9 0x92

#define V87_300CD_R_GE9 0xA4
#define V87_300CD_G_GE9 0xBA
#define V87_300CD_B_GE9 0x93

#define V171_300CD_R_GE9 0xBD
#define V171_300CD_G_GE9 0xC8
#define V171_300CD_B_GE9 0xAF

#define V255_300CD_R_GE9 0xB0
#define V255_300CD_G_GE9 0xA2
#define V255_300CD_B_GE9 0xD1

/* PANEL DEPENDENT THINGS END*/
#elif defined(CONFIG_MACH_COMANCHE)
/*
*	NEW M2 PANEL
*/
#define V1_300CD_R 0x18
#define V1_300CD_G 0x08
#define V1_300CD_B 0x24

#define V19_300CD_R 0x3A
#define V19_300CD_G 0x48
#define V19_300CD_B 0x16

#define V43_300CD_R 0xB0
#define V43_300CD_G 0xB7
#define V43_300CD_B 0xA4

#define V87_300CD_R 0xAA
#define V87_300CD_G 0xB3
#define V87_300CD_B 0x9E

#define V171_300CD_R 0xC1
#define V171_300CD_G 0xC3
#define V171_300CD_B 0xB5

#define V255_300CD_R 0xB1
#define V255_300CD_G 0xAE
#define V255_300CD_B 0xF3

#elif defined(CONFIG_MACH_APEXQ)
/*
*	 SM2 PANEL
*/
#define V1_300CD_R_SM2 0x31
#define V1_300CD_G_SM2 0x00
#define V1_300CD_B_SM2 0x4F

#define V19_300CD_R_SM2 0x14
#define V19_300CD_G_SM2 0x6E
#define V19_300CD_B_SM2 0x00

#define V43_300CD_R_SM2 0xA3
#define V43_300CD_G_SM2 0xC0
#define V43_300CD_B_SM2 0x92

#define V87_300CD_R_SM2 0xA4
#define V87_300CD_G_SM2 0xBA
#define V87_300CD_B_SM2 0x93

#define V171_300CD_R_SM2 0xBD
#define V171_300CD_G_SM2 0xC8
#define V171_300CD_B_SM2 0xAF

#define V255_300CD_R_SM2 0xB0
#define V255_300CD_G_SM2 0xA2
#define V255_300CD_B_SM2 0xD1

/*
*	NEW M2 PANEL
*/
#define V1_300CD_R_M2 0x18
#define V1_300CD_G_M2 0x08
#define V1_300CD_B_M2 0x24

#define V19_300CD_R_M2 0x3A
#define V19_300CD_G_M2 0x48
#define V19_300CD_B_M2 0x16

#define V43_300CD_R_M2 0xB0
#define V43_300CD_G_M2 0xB7
#define V43_300CD_B_M2 0xA4

#define V87_300CD_R_M2 0xAA
#define V87_300CD_G_M2 0xB3
#define V87_300CD_B_M2 0x9E

#define V171_300CD_R_M2 0xC1
#define V171_300CD_G_M2 0xC3
#define V171_300CD_B_M2 0xB5

#define V255_300CD_R_M2 0xB1
#define V255_300CD_G_M2 0xAE
#define V255_300CD_B_M2 0xF3
#else
/*
*	OLD M2 PANEL
*/
#define V1_300CD_R 0x18
#define V1_300CD_G 0x08
#define V1_300CD_B 0x24

#define V19_300CD_R 0x70
#define V19_300CD_G 0x6E
#define V19_300CD_B 0x4E

#define V43_300CD_R 0xBC
#define V43_300CD_G 0xC0
#define V43_300CD_B 0xAF

#define V87_300CD_R 0xB3
#define V87_300CD_G 0xB8
#define V87_300CD_B 0xA5

#define V171_300CD_R 0xC5
#define V171_300CD_G 0xC7
#define V171_300CD_B 0xBB

#define V255_300CD_R 0xB9
#define V255_300CD_G 0xBB
#define V255_300CD_B 0xFC

#endif
/* PANEL DEPENDET THINGS END*/


enum {
	V1_INDEX = 0,
	V19_INDEX = 1,
	V43_INDEX = 2,
	V87_INDEX = 3,
	V171_INDEX = 4,
	V255_INDEX = 5,
};

struct GAMMA_LEVEL {
	int level_0;
	int level_1;
	int level_19;
	int level_43;
	int level_87;
	int level_171;
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
	struct GRAY_VOLTAGE TABLE[S6E63M0_GRAY_SCALE_MAX];
} __packed;

struct MTP_SET {
	char OFFSET_1;
	char OFFSET_19;
	char OFFSET_43;
	char OFFSET_87;
	char OFFSET_171;
	char OFFSET_255_MSB;
	char OFFSET_255_LSB;
} __packed;

struct MTP_OFFSET {
	/*
		MTP_OFFSET is consist of 22 byte.
		First byte is dummy and 21 byte is useful.
	*/
	struct MTP_SET R_OFFSET;
	struct MTP_SET G_OFFSET;
	struct MTP_SET B_OFFSET;
} __packed;


struct SMART_DIM {
	struct MTP_OFFSET MTP;
	struct RGB_OUTPUT_VOLTARE RGB_OUTPUT;
	struct GRAY_SCALE GRAY;
	int brightness_level;
} __packed;

void generate_gamma(struct SMART_DIM *smart_dim, char *str, int size);
int smart_dimming_init(struct SMART_DIM *smart_dim);
extern unsigned int get_lcd_manufacture_id(void);
#endif /* SMART_MTP_S6E63M0_H */
