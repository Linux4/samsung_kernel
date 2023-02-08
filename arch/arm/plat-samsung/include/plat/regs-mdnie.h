/* linux/arch/arm/plat-samsung/include/plat/regs_mdnie.h
 *
 * Header file for Samsung (MDNIE) driver
 *
 * Copyright (c) 2009 Samsung Electronics
 *	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __REGS_MDNIE_H__
#define __REGS_MDNIE_H__

#define S3C_MDNIE_PHY_BASE		0x11CA0000
#define S3C_MDNIE_MAP_SIZE		0x00001000

#define S3C_MDNIE_rR0			0x0000
#define S3C_MDNIE_rR1			0x0001
#define S3C_MDNIE_rR3			0x0003
#define S3C_MDNIE_rR4			0x0004
#define S3C_MDNIE_rRFF			0x00FF

#define S3C_MDNIE_rRED_R		0x0091		/*SCR RrCr*/
#define S3C_MDNIE_rRED_G		0x0092		/*SCR RgCg*/
#define S3C_MDNIE_rRED_B		0x0093		/*SCR RbCb*/
#define S3C_MDNIE_rBLUE_R		0x0094		/*SCR GrMr*/
#define S3C_MDNIE_rBLUE_G		0x0095		/*SCR GgMg*/
#define S3C_MDNIE_rBLUE_B		0x0096		/*SCR GbMb*/
#define S3C_MDNIE_rGREEN_R		0x0097		/*SCR BrYr*/
#define S3C_MDNIE_rGREEN_G		0x0098		/*SCR BgYg*/
#define S3C_MDNIE_rGREEN_B		0x0099		/*SCR BbYb*/
#define S3C_MDNIE_rWHITE_R		0x009A		/*SCR KrWr*/
#define S3C_MDNIE_rWHITE_G		0x009B		/*SCR KgWg*/
#define S3C_MDNIE_rWHITE_B		0x009C		/*SCR KbWb*/

#define S3C_MDNIE_INPUT_HSYNC	(1 << 9)
#define S3C_MDNIE_INPUT_DATA_ENABLE	(1 << 10)


#define	S3C_MDNIE_SIZE_MASK		0xfff
#define S3C_MDNIE_HSIZE(n)		(n & S3C_MDNIE_SIZE_MASK)
#define S3C_MDNIE_VSIZE(n)		(n & S3C_MDNIE_SIZE_MASK)


#define S3C_MDNIE_CLKCON	   (0x027C)
#define S3C_MDNIE_CLKCON_DISABLE   (0 << 0)
#define S3C_MDNIE_CLKCON_MDNIE (3 << 0)

#define TRUE				1
#define FALSE				0
#endif
