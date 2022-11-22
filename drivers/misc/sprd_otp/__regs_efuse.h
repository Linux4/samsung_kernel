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
 *************************************************
 * Automatically generated C header: do not edit *
 *************************************************
 */

#ifndef __REGS_EFUSE_H__
#define __REGS_EFUSE_H__

#define REGS_EFUSE

/* registers definitions for controller REGS_EFUSE */
#define REG_EFUSE_DATA_RD               SCI_ADDR(REGS_EFUSE_BASE, 0x0000)
#define REG_EFUSE_DATA_WR               SCI_ADDR(REGS_EFUSE_BASE, 0x0004)
#define REG_EFUSE_READ_WRITE_INDEX      SCI_ADDR(REGS_EFUSE_BASE, 0x0008)
#define REG_EFUSE_MODE_CTRL             SCI_ADDR(REGS_EFUSE_BASE, 0x000c)
#define REG_EFUSE_CFG0                  SCI_ADDR(REGS_EFUSE_BASE, 0x0010)
#define REG_EFUSE_CFG1                  SCI_ADDR(REGS_EFUSE_BASE, 0x0014)
#define REG_EFUSE_STATUS                SCI_ADDR(REGS_EFUSE_BASE, 0x0020)
#define REG_EFUSE_BLK_FLAG0             SCI_ADDR(REGS_EFUSE_BASE, 0x0024)
#define REG_EFUSE_BLK_FLAG1             SCI_ADDR(REGS_EFUSE_BASE, 0x0028)
#define REG_EFUSE_BLK_FLAG0_CLR         SCI_ADDR(REGS_EFUSE_BASE, 0x0030)
#define REG_EFUSE_BLK_FLAG1_CLR         SCI_ADDR(REGS_EFUSE_BASE, 0x0034)
#define REG_EFUSE_MAGIC_NUMBER          SCI_ADDR(REGS_EFUSE_BASE, 0x0040)
#define REG_EFUSE_STROBE_LOW_WIDTH      SCI_ADDR(REGS_EFUSE_BASE, 0x0044)
#define REG_EFUSE_EFUSE_DEB_CTRL        SCI_ADDR(REGS_EFUSE_BASE, 0x0048)

/* bits definitions for register REG_EFUSE_READ_WRITE_INDEX */
#define BITS_READ_WRITE_INDEX(_x_)      ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

#define SHFT_READ_WRITE_INDEX           ( 0 )
#define MASK_READ_WRITE_INDEX           ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4) )

/* bits definitions for register REG_EFUSE_MODE_CTRL */
/* Write 1 to this bit start A_PGM mode(array PGM mode).
 * This bit is self-clear, read this bit will always get 0.
 */
#define BIT_PG_START                    ( BIT(0) )
#define BIT_RD_START                    ( BIT(1) )
#define BIT_STANDBY_START               ( BIT(2) )

/* bits definitions for register REG_EFUSE_CFG0 */
#define BITS_TPGM_TIME_CNT(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)) )
#define BITS_EFS_TYPE(_x_)              ( (_x_) << 16 & (BIT(16)|BIT(17)) )
#define BIT_EFS_VDDQ_K1_ON              ( BIT(28) )
#define BIT_EFS_VDDQ_K2_ON              ( BIT(29) )
/* Set this bit will open 0.9v static power supply for efuse memory,
 * before any operation towards to efuse memory this bit have to set to 1.
 * Once this bit is cleared, the efuse will go to power down mode.
 */
#define BIT_EFS_VDD_ON                  ( BIT(30) )
/* ONly set this bit can SW write register field of TPGM_TIME_CNT and start PGM mode,
 * for protect sw unexpectedly programmed efuse memory.
 */
#define BIT_PGM_EN                      ( BIT(31) )

/* bits definitions for register REG_EFUSE_CFG1 */
#define BIT_BLK0_AUTO_TEST_EN           ( BIT(0) )

/* bits definitions for register REG_EFUSE_STATUS */
#define BIT_PGM_BUSY                    ( BIT(0) )
#define BIT_READ_BUSY                   ( BIT(1) )
#define BIT_STANDBY_BUSY                ( BIT(2) )

/* bits definitions for register REG_EFUSE_BLK_FLAG0 */
#define BIT_BLK0_PROT_FLAG              ( BIT(0) )

/* bits definitions for register REG_EFUSE_BLK_FLAG1 */
#define BIT_BLK0_ERR_FLAG               ( BIT(0) )

/* bits definitions for register REG_EFUSE_BLK_FLAG0_CLR */
#define BIT_BLK0_PROT_FLAG_CLR          ( BIT(0) )

/* bits definitions for register REG_EFUSE_BLK_FLAG1_CLR */
#define BIT_BLK0_ERR_FLAG_CLR           ( BIT(0) )

/* bits definitions for register REG_EFUSE_MAGIC_NUMBER */
/* Magic number, only when this field is 0x8810, the efuse programming command can be handle.
 * So, if SW want to program efuse memory, except open clocks and power, the follow conditions must be met:
 * 1. PGM_EN = 1;
 * 2. EFUSE_MAGIC_NUMBER = 0x8810
 */
#define BITS_MAGIC_NUMBER(_x_)          ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_DEB_MAGIC_NUMBER(_x_)      ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )

/* bits definitions for register REG_EFUSE_STROBE_LOW_WIDTH */
#define BITS_EFUSE_STROBE_LOW_WIDTH(_x_)( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_CLK_EFS_DIV(_x_)           ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )

#define SHFT_CLK_EFS_DIV                ( 16 )
#define MASK_CLK_EFS_DIV                ( BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23) )

/* bits definitions for register REG_EFUSE_EFUSE_DEB_CTRL */
#define BIT_MARGIN_MODE_EN              ( BIT(1) )
#define BIT_DOUBLE_BIT_DISABLE          ( BIT(0) )

/* vars definitions for controller REGS_EFUSE */
#define EFUSE_MAGIC_NUMBER              ( 0x8810 )
#define EFUSE_DEB_MAGIC_NUMBER          ( 0x6868 )
#define BIT_EFUSE_PROT_LOCK             ( BIT(31) )

#endif /* __REGS_EFUSE_H__ */
