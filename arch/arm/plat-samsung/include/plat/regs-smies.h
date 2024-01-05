/* linux/arch/arm/plat-samsung/include/plat/regs-smies.h
 *
 * Copyright (c) 2014 Samsung Electronics
 *		http://www.samsung.com/
 *
 * SMIES register header file for Samsung SMIES driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __PLAT_SAMSUNG_REGS_SMIES_H
#define __PLAT_SAMSUNG_REGS_SMIES_H

#define SMIESCON			(0x0000)
#define SMIESIMG_SIZESET		(0x0004)
#define SMIESSCR_RED			(0x0008)
#define SMIESSCR_GREEN			(0x000c)
#define SMIESSCR_BLUE			(0x0010)
#define SMIESSCR_CYAN			(0x0014)
#define SMIESSCR_MAGENTA		(0x0018)
#define SMIESSCR_YELLOW			(0x001c)
#define SMIESSCR_WHITE			(0x0020)
#define SMIESSCR_BLACK			(0x0024)
#define SMIESGAMMALUT_R			(0x0028)
#define SMIESGAMMALUT_G			(0x00AC)
#define SMIESGAMMALUT_B			(0x0130)
#define SMIESCRC_CON			(0x01B4)
#define SMIESCRC_RD_DATA		(0x01B8)
#define SMIESCON_MASK			(0x0200)

/* generates mask for range of bits */
#define SMIES_MASK(high_bit, low_bit) \
	(((2 << ((high_bit) - (low_bit))) - 1) << (low_bit))

#define SMIES_MASK_VAL(val, high_bit, low_bit) \
	(((val) << (low_bit)) & SMIES_MASK(high_bit, low_bit))

/* SMIESCON */
#define SMIES_CON_ALL_RESET		(1 << 17)
#define SMIES_CON_SW_RESET		(1 << 16)
#define SMIES_CON_UPDATE_NO_COND	(1 << 15)
#define SMIES_CON_CLK_GATE_ON		(1 << 12)
#define SMIES_CON_SAE_SKIN_CHECK	(1 << 10)
#define SMIES_CON_SAE_GAIN_VAL(x)	SMIES_MASK_VAL(x, 9, 6)
#define SMIES_CON_SAE_GAIN_MASK		(0xF << 6)
#define SMIES_CON_I80_MODE		(1 << 5)
#define SMIES_CON_SAE_ON		(1 << 4)
#define SMIES_CON_GAMMA_ON		(1 << 3)
#define SMIES_CON_SCR_ON		(1 << 2)
#define SMIES_CON_DITHER_ON		(1 << 1)
#define SMIES_CON_SMIES_ON		(1 << 0)

/* SMIESIMG_SIZESET */
#define SMIES_IMGSIZE_HEIGHT_VAL(x)	SMIES_MASK_VAL(x, 29, 16)
#define SMIES_IMGSIZE_WIDTH_VAL(x)	SMIES_MASK_VAL(x, 13, 0)

/* SMIESCRC_CON */
#define SMIES_CRC_CON_CLKEN		(1 << 2)
#define SMIES_CRC_CON_START		(1 << 1)
#define SMIES_CRC_CON_EN		(1 << 0)

/* SMIESCON_MASK */
#define SMIES_CON_MASK_CTRL		(1 << 0)

/* SMIESGAMMALUT_R/G/B */
#define SMIES_GAMMALUT_EVEN_VAL(x)	SMIES_MASK_VAL(x, 10, 2)
#define SMIES_GAMMALUT_ODD_VAL(x)	SMIES_MASK_VAL(x, 26, 18)

#endif /* __PLAT_SAMSUNG_REGS_SMIES_H */
