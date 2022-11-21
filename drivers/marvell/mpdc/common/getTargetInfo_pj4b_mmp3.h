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

#ifndef GETTARGETINFO_PJ4B_MMP3_H_
#define GETTARGETINFO_PJ4B_MMP3_H_

#include "getTargetInfo_pj4b.h"

/* index of TargetInfo rawData */
enum INDEX{
	/* all the cpuIds for cores */
	core_0_main_id_register = 0,
	core_1_main_id_register = 1,
	core_2_main_id_register = 2,

	/* core 0 related information */
	core_0_index = 3,
	core_0_frequency = 4,
	core_0_processor_feature_0_register = 5,
	core_0_processor_feature_1_register = 6,
	core_0_TLB_type_register = 7,
	core_0_memory_model_feature_0_register = 8,
	core_0_L1_D_cache_info = 9,
	core_0_L1_D_cache_size = 10,
	core_0_L1_D_cache_num_sets = 11,
	core_0_L1_D_cache_associativity = 12,
	core_0_L1_D_cache_line_size = 13,

	core_0_L1_I_cache_info = 14,
	core_0_L1_I_cache_size = 15,
	core_0_L1_I_cache_num_sets = 16,
	core_0_L1_I_cache_associativity = 17,
	core_0_L1_I_cache_line_size = 18,
	core_0_auxiliary_control_register = 19,

	/* core 1 related information */
	core_1_index = 20,
	core_1_frequency = 21,
	core_1_processor_feature_0_register = 22,
	core_1_processor_feature_1_register = 23,
	core_1_TLB_type_register = 24,
	core_1_memory_model_feature_0_register = 25,
	core_1_L1_D_cache_info = 26,
	core_1_L1_D_cache_size = 27,
	core_1_L1_D_cache_num_sets = 28,
	core_1_L1_D_cache_associativity = 29,
	core_1_L1_D_cache_line_size = 30,

	core_1_L1_I_cache_info = 31,
	core_1_L1_I_cache_size = 32,
	core_1_L1_I_cache_num_sets = 33,
	core_1_L1_I_cache_associativity = 34,
	core_1_L1_I_cache_line_size = 35,
	core_1_auxiliary_control_register = 36,

	/* core 2 related information */
	core_2_index = 37,
	core_2_frequency = 38,
	core_2_processor_feature_0_register = 39,
	core_2_processor_feature_1_register = 40,
	core_2_TLB_type_register = 41,
	core_2_memory_model_feature_0_register = 42,
	core_2_L1_D_cache_info = 43,
	core_2_L1_D_cache_size = 44,
	core_2_L1_D_cache_num_sets = 45,
	core_2_L1_D_cache_associativity = 46,
	core_2_L1_D_cache_line_size = 47,

	core_2_L1_I_cache_info = 48,
	core_2_L1_I_cache_size = 49,
	core_2_L1_I_cache_num_sets = 50,
	core_2_L1_I_cache_associativity = 51,
	core_2_L1_I_cache_line_size = 52,
	core_2_auxiliary_control_register = 53,

	system_L2_cache_enable = 54,
	system_L2_unified_cache_info = 55,
	system_L2_unified_cache_size = 56,
	system_L2_unified_cache_num_sets = 57,
	system_L2_unified_cache_associativity = 58,
	system_L2_unified_cache_line_size = 59,

	core_0_system_control_register = 60,
	core_1_system_control_register = 61,
	core_2_system_control_register = 62,

	SDRAM_type = 63,
	memory_address_map_area_length = 64,

	core_0_online = 65,
	core_1_online = 66,
	core_2_online = 67,

	core_0_floating_point_system_id_register = 68,
	core_0_floating_point_status_and_control_register = 69,
	core_0_floating_point_exception_register = 70,

	core_1_floating_point_system_id_register = 71,
	core_1_floating_point_status_and_control_register = 72,
	core_1_floating_point_exception_register = 73,

	core_2_floating_point_system_id_register = 74,
	core_2_floating_point_status_and_control_register = 75,
	core_2_floating_point_exception_register = 76,

	total_length, /* Keep this one always the last one, which stands for the total raw data length, and will be used by get_arm_target_raw_data_length() */
};

//#define MMP3_SL2_CACHE_CONTROL_REG_ADDR (0xD0020000 + 0x100) /* Refer to P48 of opt_guide_pj4b.docx and P304 of PJ4B-MP Datasheet.pdf */

unsigned long get_arm_target_raw_data_length(void);
int get_all_register_info(unsigned long *info);

#endif /* GETTARGETINFO_PJ4B_MMP3_H_ */
