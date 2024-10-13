/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/string.h>
#include "sprd_otp.h"

u32 __adie_efuse_read_bits(int bit_index, int length);

struct otp_bitmap {
	const char *name;	/* MUST same as sc2723-regulators.dtsi */
	int bits_idx, bits_len;/* field bits offset and length */
};

const struct otp_bitmap otp_bitmap[] = {
	/* regulator name,   block, offset, length */
	//DCDC (6)
	{"vddcore", 	BITSINDEX(0, 0), 6},	/* DCDC_CORE_NOR */
	{"vddcore_ds", 	BITSINDEX(1, 0), 6},	/* DCDC_CORE_SLP */
	{"vddarm", 		BITSINDEX(1, 6), 7},	/* DCDC_ARM_A */
	{"vddarm_b", 	BITSINDEX(2, 5), 7},	/* DCDC_ARM_B */
	{"vddmem", 		BITSINDEX(3, 6), 7},	/* DCDC_MEM */
	{"vddgen", 		BITSINDEX(4, 5), 7},	/* DCDC_GEN */
	{"vddrf", 		BITSINDEX(5, 4), 7},
	{"vddcon", 		BITSINDEX(6, 3), 7},	/* DCDC_CON?DCDC_WPA? */

	//LDO (20)
	{"vdd25", 		BITSINDEX(18, 2), 6},	/* AVDD28? *//* FIXME: be care LDO_VDD2V5_OPT 0 : 1.8v, 1 : 2.8V */
	{"vddkpled", 	BITSINDEX(19, 0), 6},
	{"vddvibr", 	BITSINDEX(19, 6), 6},
	{"vddwifipa", 	BITSINDEX(20, 4), 6},
	{"vddcammot", 	BITSINDEX(21, 2), 6},
	{"vddemmccore", BITSINDEX(22, 0), 6},
	{"vdddcxo", 	BITSINDEX(22, 6), 6},
	{"vddsdcore", 	BITSINDEX(23, 4), 6},
	{"vdd28", 		BITSINDEX(24, 2), 6},
	{"vddcama", 	BITSINDEX(25, 0), 6},
	{"vddsim0", 	BITSINDEX(25, 6), 6},
	{"vddsim1", 	BITSINDEX(25, 6), 6},	/* share with vddsim0 */
	{"vddsim2", 	BITSINDEX(25, 6), 6},	/* share with vddsim0 */
	{"vddsdio", 	BITSINDEX(26, 4), 6},
	{"vddusb", 		BITSINDEX(27, 2), 6},
	{"vddcamio", 	BITSINDEX(28, 0), 6},
	{"vdd18", 		BITSINDEX(28, 0), 6},	/* share with vddcamio */
	{"vddcamd", 	BITSINDEX(28, 6), 6},
	{"vddrf0", 		BITSINDEX(29, 4), 6},
	{"vddgen1", 	BITSINDEX(30, 2), 6},
	{"vddgen0", 	BITSINDEX(31, 0), 6},
	{0},
};

int sci_otp_get_offset(const char *name)
{
	const struct otp_bitmap *map = otp_bitmap;
	int offset = 0;
	for (; map->name; map++) {
		if (0 == strcmp(map->name, name)) {
			offset -= (int)(BIT(map->bits_len) / 2);	/* middle */
			offset +=
			    (int)__adie_efuse_read_bits(map->bits_idx,
							map->bits_len);
		}
	}
	if ((0 == strcmp(name, "vddcore"))
	    || (0 == strcmp(name, "vddarm"))) {
		int bb_chip = (int)__adie_efuse_read_bits(BITSINDEX(31, 6), 2);
		pr_debug("%s bb_chip magic %d\n", name, bb_chip);
		offset += -32 + 32 * bb_chip;
	}
	pr_debug("%s offset %d (steps)\n", name, offset);
	return offset;
}
