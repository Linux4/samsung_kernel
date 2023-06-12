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

#include "perfmgr_internal.h"

/*--------------------function--------------------*/

/* Set floor of cpu core            */
/* input parameter: -1: don't care  */
int mt_perfmgr_boost_core(int core)
{
#ifdef MTK_BOOST_SUPPORT
	perfmgr_boost_core(PERFMGR_SCN_USER_SETTING, core);
	return 0;
#else
	return -1;
#endif
}

/* Set floor of cpu freq            */
/* input parameter: -1: don't care  */
int mt_perfmgr_boost_freq(int freq)
{
#ifdef MTK_BOOST_SUPPORT
	perfmgr_boost_freq(PERFMGR_SCN_USER_SETTING, freq);
	return 0;
#else
	return -1;
#endif
}
