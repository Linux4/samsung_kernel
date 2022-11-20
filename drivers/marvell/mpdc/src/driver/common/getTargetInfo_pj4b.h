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

#ifndef GETTARGETINFO_PJ4B_H_
#define GETTARGETINFO_PJ4B_H_

#define MHZ_TO_KHZ 1000
#define MB_TO_KB   1024
#define B_TO_KB    1024
#define UNKNOWN_VALUE -1

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
	CORE_UNKNOWN_STATUS = -1, /* 0xffffffff */
	CORE_OFF_LINE 		= 0,
	CORE_ON_LINE  		= 1
};

extern unsigned long get_main_id_register(void);
extern unsigned long get_TLB_type_register(void);
extern unsigned long get_processor_feature_0_register(void);
extern unsigned long get_processor_feature_1_register(void);
extern unsigned long get_memory_model_feature_0_register(void);
extern unsigned long get_cache_info(unsigned long level, enum CACHE_DETAIL_INFO detail, enum CACHE_TYPE type);
extern unsigned long get_system_control_register(void);
extern unsigned long get_auxiliary_control_register(void);
extern unsigned long get_arm_cpu_freq(void);
extern unsigned long get_floating_point_exception_register(void);
extern unsigned long get_floating_point_status_and_control_register(void);
extern unsigned long get_floating_point_system_id_register(void);
extern int get_online_cpu_info(int index);

#endif /* GETTARGETINFO_PJ4B_H_ */
