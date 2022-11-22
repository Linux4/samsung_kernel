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

#ifndef __GET_TARGET_INFO_PJ4_H__
#define __GET_TARGET_INFO_PJ4_H__

#define MHZ_TO_KHZ 1000
#define MB_TO_KB   1024
#define B_TO_KB    1024

enum CACHE_TYPE
{
	CACHE_TYPE_UNKNOWN	= -1, /* 0xffffffff */
	I_CACHE				= 0,
	D_CACHE				= 1,
	UNIFIED_CACHE		= 2,
};

enum CLOCK_TYPE
{
	CLOCK_TYPE_UNKNOWN 	= -1, /* 0xffffffff */
	DDR_clock 			= 0,
	L2_clock 			= 1,
	axi_bus_clock		= 2,
	processor_clock		= 3,
};

enum CACHE_DETAIL_INFO
{
	CACHE_DETAIL_INFO_UNKNOWN	= -1, /* 0xffffffff */
	NUM_SET						= 0,
	LINE_SIZE					= 1,
	ASSOCIATIVITY				= 2,
	CACHE_INFO					= 3,
	CACHE_SIZE					= 4,
};

enum SDRAM_TYPE
{
	SDRAM_TYPE_UNKNOWN = -1, /* 0xffffffff */
	DDR1 = 0,
	DDR2 = 1,
	DDR3 = 2,
	LPDDR1 = 3,
	LPDDR2_S2 = 4,
	LPDDR2_S4 = 5,

};

enum CORE_ONLINE_STATUS
{
	CORE_UNKNOWN_STATUS = -1,
	CORE_OFF_LINE 		= 0,
	CORE_ON_LINE  		= 1
};

/* index of TargetInfo rawData */
enum INDEX{
	main_id_register = 0,
	core_0_index = 1,
	core_0_frequency = 2,
	processor_feature_0_register = 3,
	processor_feature_1_register = 4,
	TLB_type_register = 5,
	memory_model_feature_0_register = 6,
	L1_D_cache_info = 7,				/* When L1 DCache is selected, get the value of Current Cache Size ID Register */
	L1_D_cache_size = 8,
	L1_D_cache_num_sets = 9,
	L1_D_cache_associativity = 10,
	L1_D_cache_line_size = 11,
	L1_I_cache_info = 12,				/* When L1 ICache is selected, get the value of Current Cache Size ID Register */
	L1_I_cache_size = 13,
	L1_I_cache_num_sets = 14,
	L1_I_cache_associativity = 15,
	L1_I_cache_line_size = 16,
	L2_unified_cache_info = 17,			/* When L2 unified cache is selected, get the value of Current Cache Size ID Register */
	L2_unified_cache_size = 18,
	L2_unified_cache_num_sets = 19,
	L2_unified_cache_associativity = 20,
	L2_unified_cache_line_size = 21,
	control_register = 22,
	L2_cache_enable = 23, 				/* enable:1,disable:0,L2 cache enable only when L1 cache enable, calculate this value from control register and auxiliary control register */
	SDRAM_type = 24,					/* Calculate this value according to SDRAM Control Register 4 */
	memory_address_map_area_length = 25,/* length of the memory block, in kb, calculate from Memory Address Map Register  */

	/*
	 * Calculate the following values from
	 * PJ4 Clock Control Register(CC_MOH_OFF)
	 * PJ4 Clock Control Status Register(DM_CC_MOH_OFF)
	 * Frequency Change Control Register(FCCR_OFF)
	 * PLL2 control register (PLL2CR_OFF)
	 */
	DDR_clock_frequency = 26,
	L2_clock_frequency = 27,
	axi_bus_clock_frequency = 28,
	core_0_online = 29,					/* core 0 status, online:1, offline:0 */

	floating_point_system_id_register = 30,
	floating_point_status_and_control_register = 31,
	floating_point_exception_register = 32,

	total_length, /* this one is always the last one, which stands for the raw data length, and will be used by get_arm_target_raw_data_length() */

};

unsigned long get_arm_target_raw_data_length(void);
int get_all_register_info(unsigned long *info);

#endif
