/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _GSPN_COEF_GENERATE_H_
#define _GSPN_COEF_GENERATE_H_

//#include "gspn_drv.h"
#include "sin_cos.h"

#define SIGN2(input,p)			{if (p>=0) input = 1; if (p < 0) input = -1;}
#define COUNT					256
#define COEF_ARR_ROWS           9
//#define COEF_ARR_COLUMNS        8
#define COEF_ARR_COL_MAX        32
#define GSC_ABS(_a)             ((_a) < 0 ? -(_a) : (_a))
//#define pi						3.14159265357
#define GSC_FIX					24
#define MAX( _x, _y )           (((_x) > (_y)) ? (_x) : (_y) )

#define LIST_SET_ENTRY_KEY(pEntry,i_w,i_h,o_w,o_h,h_t,v_t)\
{\
    pEntry->in_w = i_w;\
    pEntry->in_h = i_h;\
    pEntry->out_w = o_w;\
    pEntry->out_h = o_h;\
    pEntry->hor_tap = h_t;\
    pEntry->ver_tap = v_t;\
}

typedef struct
{
    ulong begin_addr;
    ulong total_size;
    ulong used_size;
} GSC_MEM_POOL;



#endif
