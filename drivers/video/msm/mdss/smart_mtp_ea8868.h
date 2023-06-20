/*
 * =================================================================
 *
 *       Filename:  smart_mtp_EA8868.h
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
#ifndef _SMART_MTP_EA8868_H_
#define _SMART_MTP_EA8868_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>
#include <asm/div64.h>

#define LUMINANCE_MAX 30
#define GAMMA_SET_MAX 24
#define BIT_SHIFT 14
#define GEN_GAMMA_MAX 21

/*
	it means BIT_SHIFT is 14.  pow(2,BIT_SHIFT) is 16384.
	BIT_SHIFT is used for right bit shfit
*/
#define BIT_SHFIT_MUL 16384

#define EA8868_GRAY_SCALE_MAX 256

/*4.5*16384 */
#define EA8868_VREG0_REF 73728

/*V0,V1,V15,V35,V59,V87,V177,V255*/
#define EA8868_MAX 8

/*for LDI: EA8868 */
/* PANEL DEPENDET THINGS */
#define V255_300CD_R_MSB 0x00
#define V255_300CD_R_LSB 0xC0
#define V171_300CD_R 0xBA
#define V87_300CD_R 0xA5
#define V59_300CD_R 0xCF
#define V35_300CD_R 0xC0
#define V15_300CD_R 0xC8
#define V1_300CD_R 0x55

#define V255_300CD_G_MSB 0x00
#define V255_300CD_G_LSB 0xE0
#define V171_300CD_G 0xB8
#define V87_300CD_G 0xA3
#define V59_300CD_G 0xCD
#define V35_300CD_G 0xBE
#define V15_300CD_G 0xC9
#define V1_300CD_G 0x55

#define V255_300CD_B_MSB 0x00
#define V255_300CD_B_LSB 0xE7
#define V171_300CD_B 0xB8
#define V87_300CD_B 0xA3
#define V59_300CD_B 0xCD
#define V35_300CD_B 0xBE
#define V15_300CD_B 0xCF
#define V1_300CD_B 0x55
/* PANEL DEPENDET THINGS END*/

enum {
	V1_INDEX = 0,
	V15_INDEX = 1,
	V35_INDEX = 2,
	V59_INDEX = 3,
	V87_INDEX = 4,
	V171_INDEX = 5,
	V255_INDEX = 6,
};

struct GAMMA_LEVEL {
	int level_0;
	int level_1;
	int level_15;
	int level_35;
	int level_59;
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
	struct GRAY_VOLTAGE TABLE[EA8868_GRAY_SCALE_MAX];
} __packed;

struct MTP_SET {
	char OFFSET_255_MSB;
	char OFFSET_255_LSB;
	char OFFSET_171;
	char OFFSET_87;
	char OFFSET_59;
	char OFFSET_35;
	char OFFSET_15;
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

struct LUMINANCE_TABLE {
	int luminace;
	char gamma_setting[GAMMA_SET_MAX];
} __packed;

struct SMART_DIM {
	struct MTP_OFFSET MTP;
	struct RGB_OUTPUT_VOLTARE RGB_OUTPUT;
	struct GRAY_SCALE GRAY;
	struct LUMINANCE_TABLE TABLE[LUMINANCE_MAX];
	int brightness_level;
} __packed;

void generate_gamma(struct SMART_DIM *smart_dim, char *str, int size);
int smart_dimming_init(struct SMART_DIM *smart_dim);

#endif
