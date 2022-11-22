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

#ifndef __GET_TARGET_INFO_PJ1_H__
#define __GET_TARGET_INFO_PJ1_H__

#define UNKNOWN_VALUE 	-1
#define MHZ_TO_KHZ		1000
#define MB_TO_KB		1024
#define KB_TO_B			1024
#define B_TO_KB			1024

enum SDRAM_TYPE
{
	SDRAM_TYPE_UNKNOWN = -1, /* 0xffffffff */
	DDR1 = 0,
	MOBILE_DDR1 = 1,
	LPDDR2		= 5,
};

enum CLOCK_TYPE
{
	CLOCK_TYPE_UNKNOWN 	= -1, /* 0xffffffff */
	DDR_clock 			= 0,
	L2_clock 			= 1,
	axi_bus_clock		= 2,
	processor_frequency = 3,
};

/* index of TargetInfo rawData */
enum INDEX{
	id_code_register = 0,
	core_0_index = 1,
	core_0_online = 2,					/* core 0 status, online:1, offline:0 */
	control_register = 3,

	L1_cache_info = 4,					/* store the value of Cache Type Register */
	L1_D_cache_size = 5,
	L1_D_cache_num_sets = 6,
	L1_D_cache_set_associativity = 7,
	L1_D_cache_line_length = 8,

	L1_I_cache_size = 9,
	L1_I_cache_num_sets = 10,
	L1_I_cache_set_associativity = 11,
	L1_I_cache_line_length = 12,

	L2_unified_cache_info = 13,			/* store the value of L2 Cache Type Register */
	L2_unified_cache_way_size = 14,
	L2_unified_num_sets = 15,
	L2_unified_cache_associativity = 16,
	L2_unified_cache_line_length = 17,

	DDR_type = 18,
	memory_address_map_area_length = 19,

	DDR_clock_frequency = 20,
	L2_clock_frequency = 21,
	axi_bus_clock_frequency = 22,
	processor_0_frequency = 23,

	total_length, /* this one is always the last one, which stands for the raw data length, and will be used by get_arm_target_raw_data_length() */

};

unsigned long get_arm_target_raw_data_length(void);
int get_all_register_info(unsigned long *info);

#endif
