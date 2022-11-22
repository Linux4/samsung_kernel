/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
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

#ifndef __ANA_REGS_EFUSE_H__
#define __ANA_REGS_EFUSE_H__

#define ANA_REGS_EFUSE

/* registers definitions for controller ANA_REGS_EFUSE */
#define ANA_REG_EFUSE_GLB_CTRL          SCI_ADDR(ANA_REGS_EFUSE_BASE, 0x0000)
#define ANA_REG_EFUSE_DATA_RD           SCI_ADDR(ANA_REGS_EFUSE_BASE, 0x0004)
#define ANA_REG_EFUSE_DATA_WR           SCI_ADDR(ANA_REGS_EFUSE_BASE, 0x0008)
#define ANA_REG_EFUSE_BLOCK_INDEX       SCI_ADDR(ANA_REGS_EFUSE_BASE, 0x000c)
#define ANA_REG_EFUSE_MODE_CTRL         SCI_ADDR(ANA_REGS_EFUSE_BASE, 0x0010)
#define ANA_REG_EFUSE_STATUS            SCI_ADDR(ANA_REGS_EFUSE_BASE, 0x0014)
#define ANA_REG_EFUSE_WR_TIMING_CTRL    SCI_ADDR(ANA_REGS_EFUSE_BASE, 0x0028)
#define ANA_REG_EFUSE_RD_TIMING_CTRL    SCI_ADDR(ANA_REGS_EFUSE_BASE, 0x002c)
#define ANA_REG_EFUSE_EFUSE_DEB_CTRL    SCI_ADDR(ANA_REGS_EFUSE_BASE, 0x0030)

/* bits definitions for register ANA_REG_EFUSE_GLB_CTRL */
/* Efuse SW programme enable.
 */
#define BIT_EFUSE_PGM_EN                ( BIT(0) )
/* Efuse type select, 00:TSMC, 01, 1x reserved.
 */
#define BITS_EFUSE_TYPE(_x_)            ( (_x_) << 1 & (BIT(1)|BIT(2)) )

/* bits definitions for register ANA_REG_EFUSE_DATA_RD */
#define BITS_EFUSE_DATA_RD(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_EFUSE_DATA_WR */
#define BITS_EFUSE_DATA_WR(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_EFUSE_BLOCK_INDEX */
#define BITS_READ_WRITE_INDEX(_x_)      ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

#define SHFT_READ_WRITE_INDEX           ( 0 )
#define MASK_READ_WRITE_INDEX           ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4) )

/* bits definitions for register ANA_REG_EFUSE_MODE_CTRL */
/* Write 1 to this bit start A_PGM mode(array PGM mode).
 * This bit is self-clear, read this bit will always get 0.
 */
#define BIT_PG_START                    ( BIT(0) )
#define BIT_RD_START                    ( BIT(1) )
#define BIT_STANDBY_START               ( BIT(2) )

/* bits definitions for register ANA_REG_EFUSE_STATUS */
#define BIT_PGM_BUSY                    ( BIT(0) )
#define BIT_READ_BUSY                   ( BIT(1) )
#define BIT_STANDBY_BUSY                ( BIT(2) )

/* bits definitions for register ANA_REG_EFUSE_WR_TIMING_CTRL */
#define BITS_EFUSE_WR_TIMING(_x_)       ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)) )

/* bits definitions for register ANA_REG_EFUSE_RD_TIMING_CTRL */
#define BITS_EFUSE_RD_TIMING(_x_)       ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )

/* bits definitions for register ANA_REG_EFUSE_EFUSE_DEB_CTRL */
#define BIT_MARGIN_MODE_EN              ( BIT(1) )
#define BIT_DOUBLE_BIT_DISABLE          ( BIT(0) )

/* vars definitions for controller ANA_REGS_EFUSE */
#define EFUSE_MAGIC_NUMBER              ( 0x2723 )

#endif /* __ANA_REGS_EFUSE_H__ */
