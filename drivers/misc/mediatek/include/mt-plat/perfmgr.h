/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __PERFMGR_H__
#define __PERFMGR_H__

/* data type */
#define PERFMGR_IGNORE (-1)

/* prototype */
extern int mt_perfmgr_boost_core(int core);
extern int mt_perfmgr_boost_freq(int freq);

#endif				/* !__PERFMGR_H__ */
