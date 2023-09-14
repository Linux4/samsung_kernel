/* * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __EFUSE_H__
#define __EFUSE_H__

//interface params.definition, pls don't modify it
#define __EFUSE_DDIE(_X_)	((_X_) | 0x0)
#define ADIE_EFUSE_MSK		(0x1 << 16)
#define __EFUSE_ADIE(_X_)	((_X_) | ADIE_EFUSE_MSK)

//blk id define
#define EFUSE_DDIE_BLK_0	__EFUSE_DDIE(0)
#define EFUSE_DDIE_BLK_1	__EFUSE_DDIE(1)
#define EFUSE_DDIE_BLK_2	__EFUSE_DDIE(2)
#define EFUSE_DDIE_BLK_3	__EFUSE_DDIE(3)
#define EFUSE_DDIE_BLK_4	__EFUSE_DDIE(4)
#define EFUSE_DDIE_BLK_5	__EFUSE_DDIE(5)
#define EFUSE_DDIE_BLK_6	__EFUSE_DDIE(6)
#define EFUSE_DDIE_BLK_7	__EFUSE_DDIE(7)

#define EFUSE_ADIE_BLK_0	__EFUSE_ADIE(0)

/**
 * sci_efuse_get(u32 blk) - read data from the specified adie/deie efuse block;
 * @blk: blk is the number that indicate block type(Adie/Ddie) and index number.
 * This function will read data from the block,
 * pls use the blk id MACRO to indicate blk.
 */
int sci_efuse_get(u32 blk);


#endif
