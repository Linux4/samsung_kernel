/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DECON_ASSIST_H__
#define __DECON_ASSIST_H__

#include <linux/bits.h>
#include <linux/bug.h>

/*
 *
#define __XX_DBV2REG_BIT_SHIFT(__XX)	\
__XX(PARA1,	1st DBV_MSB:DBV_LSB,	Parameter 1 REG_MSB:REG_LSB	)	\
__XX(PARA2,	2nd DBV_MSB:DBV_LSB,	Parameter 2 REG_MSB:REG_LSB	)	\
...
__XX(PARAN,	Nth DBV_MSB:DBV_LSB,	Parameter N REG_MSB:REG_LSB	)	\

__XX_DBV2REG_INIT(__XX_DBV2REG_BIT_SHIFT);
#undef __XX_DBV2REG_BIT_SHIFT

1. if 51h parameter length > 1, please refer case1)
2. if 51h parameter length = 1, please refer case2)
*/

/*
 *
case1 example)
#define __XX_DBV2REG_BIT_SHIFT(__XX)	\
__XX(PARA1,	7:4,	3:0	)	\
__XX(PARA2,	3:0,	7:4	)	\

__XX_DBV2REG_INIT(__XX_DBV2REG_BIT_SHIFT);
#undef __XX_DBV2REG_BIT_SHIFT

static u8 foo_brt_table[EXTEND_BRIGHTNESS + 1][2];
static struct maptbl foo_maptbl[MAX_MAPTBL] = {
	[...] = DEFINE_2D_MAPTBL(foo_brt_table, ..., ..., ...),
};
struct common_panel_info foo_panel_info = {
...
};
unsigned int foo_brightness_table[EXTEND_BRIGHTNESS + 1] = {
	0,
	1, 1, 2, 3, 3, 4, 5, 5, 6, 7,
...
};

__XX_DBV2REG_BRIGHTNESS_INIT(foo_panel_info, __XX_DBV2REG_BIT_SHIFT_INIT, foo_brightness_table);

case1 explanation)
__XX_DBV2REG_BIT_SHIFT based on HW SPEC
__XX(PARA1,	7:4,	3:0	)	<- this means
7:4 is DBV RANGE
3:0 is REG RANGE
DBV[7:4] <-> REG[3:0]
DBV[7] <-> REG[3]
DBV[6] <-> REG[2]
DBV[5] <-> REG[1]
DBV[4] <-> REG[0]

__XX(PARA2,	3:0,	7:4	)	<- this means
3:0 is DBV RANGE
7:4 is REG RANGE
DBV[3:0] <-> REG[7:4]
DBV[3] <-> REG[7]
DBV[2] <-> REG[6]
DBV[1] <-> REG[5]
DBV[0] <-> REG[4]

So if DBV (= brightness code) is 211(DEC) = D3h
|  DBV_7 |  DBV_6 |  DBV_5 |  DBV_4 |  DBV_3 |  DBV_2 |  DBV_1 |  DBV_0 |
|--------+--------+--------+--------+--------+--------+--------+--------|
|    1   |    1   |    0   |    1   |    0   |    0   |    1   |    0   |

Parameter 1 is Dh
|  REG_7 |  REG_6 |  REG_5 |  REG_4 |  REG_3 |  REG_2 |  REG_1 |  REG_0 |
|--------+--------+--------+--------+--------+--------+--------+--------|
|    0   |    0   |    0   |    0   |    1   |    1   |    0   |    1   |

Parameter 2 is 3h
|  REG_7 |  REG_6 |  REG_5 |  REG_4 |  REG_3 |  REG_2 |  REG_1 |  REG_0 |
|--------+--------+--------+--------+--------+--------+--------+--------|
|    0   |    0   |    0   |    0   |    0   |    0   |    1   |    0   |
*/

/*
 *
case2 example)
If 51h parameter length = 1, it may mean DBV_RANGE = REG_RANGE
In this case, you don't need to setup __XX_DBV2REG_BIT_SHIFT. JUST

static u8 foo_brt_table[EXTEND_BRIGHTNESS + 1][1];
static struct maptbl foo_maptbl[MAX_MAPTBL] = {
	[...] = DEFINE_2D_MAPTBL(foo_brt_table, ..., ..., ...),
};
unsigned int foo_brightness_table[EXTEND_BRIGHTNESS + 1] = {
	0,
	1, 1, 2, 3, 3, 4, 5, 5, 6, 7,
...
};

__XX_DBV2REG_BRIGHTNESS_INIT(foo_panel_info, __XX_DBV2REG_BIT_SHIFT_NONE, foo_brightness_table);
*/

/*
 *
other example)
1.
#define __XX_DBV2REG_BIT_SHIFT(__XX)	\
__XX(51H_PARA1,	__XX_FROM(7:4),	__XX_TO(3:0)	)	\
__XX(51H_PARA2,	__XX_FROM(3:0),	__XX_TO(7:4)	)	\

2.
#define __XX_DBV2REG_BIT_SHIFT(__XX)	\
__XX(PARA1,	11:8,	3:0	)	\
__XX(PARA2,	7:0,	7:0	)	\

3.
#define __XX_DBV2REG_BIT_SHIFT(__XX)	\
__XX(PARA1,	11:4,	7:0	)	\
__XX(PARA2,	3:0,	3:0	)	\

4.
__XX_DBV2REG_BRIGHTNESS_INIT(foo_panel_info, __XX_DBV2REG_BIT_SHIFT_INIT, NULL);
__XX_DBV2REG_BRIGHTNESS_INIT(foo_panel_info, __XX_DBV2REG_BIT_SHIFT_INIT, foo_brightness_table);
__XX_DBV2REG_BRIGHTNESS_INIT(foo_panel_info, __XX_DBV2REG_BIT_SHIFT_NONE, foo_brightness_table);

*/

enum {
	CONVERT_DBV_TO_REG,
	CONVERT_REG_TO_DBV,
	CONVERT_DBVREG_MAX,
};

#define GEN_BITS_MASK(_mask)		((BUILD_BUG_ON_ZERO((1?_mask) < (0?_mask))) + (GENMASK(1?_mask, 0?_mask)))
#define GET_BITS_MASK(_val, _mask)	(((_val) & GEN_BITS_MASK(_mask)) >> (0?_mask))
#define GET_BITS(_val, _src, _dst)	((BUILD_BUG_ON_ZERO(((1?_src)-(0?_src)) != ((1?_dst)-(0?_dst)))) + ((GET_BITS_MASK(_val, _src)) << (0?_dst)))

#define __XX_FROM(_src)	_src
#define __XX_TO(_dst)	_dst

#define __XX_DBV2REG_LIST(_name, _dbv, _reg)	DBV2REG_BIT_SHIFT_##_name,
#define __XX_DBV2REG_FUNC(_name, _dbv, _reg)	case DBV2REG_BIT_SHIFT_##_name:	return GET_BITS(value, _dbv, _reg);
#define __XX_REG2DBV_FUNC(_name, _dbv, _reg)	case DBV2REG_BIT_SHIFT_##_name:	return GET_BITS(value, _reg, _dbv);

#define __XX_DBV2REG_INIT(__XX_INFO)	\
enum { __XX_INFO(__XX_DBV2REG_LIST) };	\
static unsigned int dbv_to_reg(unsigned int value, unsigned int index)	\
{	\
	switch (index) {	\
	__XX_INFO(__XX_DBV2REG_FUNC)	\
	default:	\
		pr_info("%s: invalid index(%d)\n", __func__, index);	\
	}	\
	return 0;	\
};	\
static unsigned int reg_to_dbv(unsigned int value, unsigned int index)	\
{	\
	switch (index) {	\
	__XX_INFO(__XX_REG2DBV_FUNC)	\
	default:	\
		pr_info("%s: invalid index(%d)\n", __func__, index);	\
	}	\
	return 0;	\
}	\

#define SEQTBL_INDEX	PANEL_SET_BL_SEQ
#define SUBDEV_INDEX	PANEL_BL_SUBDEV_TYPE_DISP

#define __XX_DBV2REG_BIT_SHIFT_NONE(_info)

#define __XX_DBV2REG_BIT_SHIFT_INIT(_info)	\
do {	\
	_info.panel_dim_info[SUBDEV_INDEX]->brightness_func[CONVERT_DBV_TO_REG] = dbv_to_reg;	\
	_info.panel_dim_info[SUBDEV_INDEX]->brightness_func[CONVERT_REG_TO_DBV] = reg_to_dbv;	\
} while (0)

#define __XX_DBV2REG_BRIGHTNESS_INIT(_info, _func, _code)	\
do {	\
	_info.panel_dim_info[SUBDEV_INDEX]->brightness_code = _code;	\
	_func(_info);	\
} while (0)	\

extern unsigned int common_dbv_to_reg(unsigned int value, unsigned int index);
extern unsigned int common_reg_to_dbv(unsigned int value, unsigned int index);

struct panel_device;
extern void decon_assist_probe(struct panel_device *panel);

struct maptbl;
struct seqinfo;
extern struct maptbl *find_brightness_maptbl(struct seqinfo *seq);

#endif /* __DECON_ASSIST_H__ */
