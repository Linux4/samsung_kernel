/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#include "legacy_controller.h"
#include "gl_typedef.h"
#include "typedef.h"

#define CLUSTER_NUM	2	/* kibo series have only 2 cluster */

INT32 kalBoostCpu(P_GLUE_INFO_T prGlueInfo, UINT_32 core_num)
{
	int i = 0;
	struct ppm_limit_data core_to_set[CLUSTER_NUM];

	pr_warn("enter kalBoostCpu, core_num:%d\n", core_num);

	if (core_num == 0) {
		for (i = 0; i < CLUSTER_NUM; i++) {
			core_to_set[i].max = -1;
			core_to_set[i].min = -1;
		}
	} else {
		for (i = 0; i < CLUSTER_NUM; i++)
			core_to_set[i].max = 4;
		core_to_set[0].min = 4;
		core_to_set[1].min = core_num;
	}
	update_userlimit_cpu_core(PPM_KIR_WIFI, CLUSTER_NUM, core_to_set);

	return 0;
}

