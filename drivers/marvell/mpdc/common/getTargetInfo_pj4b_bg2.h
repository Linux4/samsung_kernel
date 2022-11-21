/*
** (C) Copyright 2011 Marvell International Ltd.
**  		All Rights Reserved

** This software file (the "File") is distributed by Marvell International Ltd.
** under the terms of the GNU General Public License Version 2, June 1991 (the "License").
** You may use, redistribute and/or modify this File in accordance with the terms and
** conditions of the License, a copy of which is available along with the File in the
** license.txt file or by writing to the Free Software Foundation, Inc., 59 Temple Place,
** Suite 330, Boston, MA 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
** THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES
** OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY DISCLAIMED.
** The License provides additional details about this warranty disclaimer.
*/

#ifndef GETTARGETINFO_PJ4B_BG2_H_
#define GETTARGETINFO_PJ4B_BG2_H_

#include "getTargetInfo_pj4b.h"

/* index of TargetInfo rawData */
enum INDEX{
	/* all the cpuIds for cores */
	core_0_main_id_register = 0,
	core_1_main_id_register = 1,

	/* core 0 related information */
	core_0_index = 2,
	core_0_frequency = 3,
	core_0_processor_feature_0_register = 4,
	core_0_processor_feature_1_register = 5,
	core_0_TLB_type_register = 6,
	core_0_memory_model_feature_0_register = 7,
	core_0_L1_D_cache_info = 8,
	core_0_L1_D_cache_size = 9,
	core_0_L1_D_cache_num_sets = 10,
	core_0_L1_D_cache_associativity = 11,
	core_0_L1_D_cache_line_size = 12,

	core_0_L1_I_cache_info = 13,
	core_0_L1_I_cache_size = 14,
	core_0_L1_I_cache_num_sets = 15,
	core_0_L1_I_cache_associativity = 16,
	core_0_L1_I_cache_line_size = 17,
	core_0_auxiliary_control_register = 18,

	/* core 1 related information */
	core_1_index = 19,
	core_1_frequency = 20,
	core_1_processor_feature_0_register = 21,
	core_1_processor_feature_1_register = 22,
	core_1_TLB_type_register = 23,
	core_1_memory_model_feature_0_register = 24,
	core_1_L1_D_cache_info = 25,
	core_1_L1_D_cache_size = 26,
	core_1_L1_D_cache_num_sets = 27,
	core_1_L1_D_cache_associativity = 28,
	core_1_L1_D_cache_line_size = 29,

	core_1_L1_I_cache_info = 30,
	core_1_L1_I_cache_size = 31,
	core_1_L1_I_cache_num_sets = 32,
	core_1_L1_I_cache_associativity = 33,
	core_1_L1_I_cache_line_size = 34,
	core_1_auxiliary_control_register = 35,

	system_L2_cache_enable = 36,
	system_L2_unified_cache_info = 37,
	system_L2_unified_cache_size = 38,
	system_L2_unified_cache_num_sets = 39,
	system_L2_unified_cache_associativity = 40,
	system_L2_unified_cache_line_size = 41,

	core_0_system_control_register = 42,
	core_1_system_control_register = 43,

	SDRAM_type = 44,
	memory_address_map_area_length = 45,

	core_0_online = 46,
	core_1_online = 47,

	core_0_floating_point_system_id_register = 48,
	core_0_floating_point_status_and_control_register = 49,
	core_0_floating_point_exception_register = 50,

	core_1_floating_point_system_id_register = 51,
	core_1_floating_point_status_and_control_register = 52,
	core_1_floating_point_exception_register = 53,

	total_length, /* Keep this one always the last one, which stands for the total raw data length, and will be used by get_arm_target_raw_data_length() */
};

#define SL2_CACHE_CONTROL_REG_ADDR (0xf7ac0000 + 0x100) /* 0xf7ac0000 is the base address of System L2 Cache Registers, 0x100 is the offset. Refer to P125 and P331 of spec named 88DE3100_PartII_30_May_2011.pdf */

unsigned long get_arm_target_raw_data_length(void);
int get_all_register_info(unsigned long *info);

#endif /* GETTARGETINFO_PJ4B_BG2_H_ */
