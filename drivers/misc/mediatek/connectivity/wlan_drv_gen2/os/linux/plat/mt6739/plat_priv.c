/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#include "gl_typedef.h"
#include "typedef.h"
#include "connectivity_build_in_adapter.h"

#define MAX_CPU_FREQ     23400000

int kalBoostCpu(P_GLUE_INFO_T prGlueInfo, unsigned int level)
{
	unsigned long freq = MAX_CPU_FREQ;

	if (level >= 1)
		spm_resource_req(SPM_RESOURCE_USER_CONN, SPM_RESOURCE_ALL); /* Disable deepidle/SODI */
	else
		spm_resource_req(SPM_RESOURCE_USER_CONN, 0); /* Enable deepidle/SODI */

	freq = level == 0 ? 0 : freq;

	mt_ppm_sysboost_freq(BOOST_BY_WIFI, freq);

	return 0;
}


