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

#include "getTargetInfo_pj4b_mmp3.h"
#include "constants.h"
#include "common.h"
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/smp.h>
#ifndef PX_SOC_BG2
#include <vfpinstr.h>
#include <asm/vfp.h>
#endif

#if defined(CONFIG_DVFM_MMP2)
#include <mach/mmp2_dvfm.h>
#include <mach/dvfm.h>
#endif

static unsigned long get_SDRAM_type(void)
{
	return SDRAM_TYPE_UNKNOWN;
}

static unsigned long get_memory_address_map_area_length(void)
{
	return UNKNOWN_VALUE;
}

static unsigned long read_reg(unsigned long address)
{
	unsigned long regValue = 0;
	void * regAddr = ioremap(address, 4);
	regValue = readl(regAddr);
	if (regAddr != NULL)
		iounmap(regAddr);
	regAddr = NULL;
	return regValue;
}

static unsigned long get_L2_cache_enable(void)
{
//	unsigned long value = 0;
//	value = read_reg(MMP3_SL2_CACHE_CONTROL_REG_ADDR);
//	return px_get_bit(value, 0);
	return -1;
}

void get_reg_data_on_core_0(unsigned long* data)
{
	data[core_0_main_id_register] = get_main_id_register();
	data[core_0_TLB_type_register]= get_TLB_type_register();
	data[core_0_processor_feature_0_register] = get_processor_feature_0_register();
	data[core_0_processor_feature_1_register] = get_processor_feature_1_register();
	data[core_0_memory_model_feature_0_register] = get_memory_model_feature_0_register();

	data[core_0_L1_I_cache_info] = get_cache_info(1, CACHE_INFO, I_CACHE);
	data[core_0_L1_I_cache_size] = get_cache_info(1, CACHE_SIZE, I_CACHE);
	data[core_0_L1_I_cache_num_sets] = get_cache_info(1, NUM_SET, I_CACHE);
	data[core_0_L1_I_cache_line_size] = get_cache_info(1, LINE_SIZE, I_CACHE);
	data[core_0_L1_I_cache_associativity] = get_cache_info(1, ASSOCIATIVITY, I_CACHE);

	data[core_0_L1_D_cache_info] = get_cache_info(1, CACHE_INFO, D_CACHE);
	data[core_0_L1_D_cache_size] = get_cache_info(1, CACHE_SIZE, D_CACHE);
	data[core_0_L1_D_cache_num_sets] = get_cache_info(1, NUM_SET, D_CACHE);
	data[core_0_L1_D_cache_line_size] = get_cache_info(1, LINE_SIZE, D_CACHE);
	data[core_0_L1_D_cache_associativity] = get_cache_info(1, ASSOCIATIVITY, D_CACHE);

	data[system_L2_cache_enable] = get_L2_cache_enable();
	data[system_L2_unified_cache_info] = get_cache_info(2, CACHE_INFO, UNIFIED_CACHE);
	data[system_L2_unified_cache_size] = get_cache_info(2, CACHE_SIZE, UNIFIED_CACHE);
	data[system_L2_unified_cache_num_sets] = get_cache_info(2, NUM_SET, UNIFIED_CACHE);
	data[system_L2_unified_cache_line_size] = get_cache_info(2, LINE_SIZE, UNIFIED_CACHE);
	data[system_L2_unified_cache_associativity] = get_cache_info(2, ASSOCIATIVITY, UNIFIED_CACHE);

	data[core_0_system_control_register] = get_system_control_register();
	data[core_0_auxiliary_control_register] = get_auxiliary_control_register();
}

void get_reg_data_on_core_1(unsigned long* data)
{
	data[core_1_main_id_register] = get_main_id_register();
	data[core_1_TLB_type_register]= get_TLB_type_register();
	data[core_1_processor_feature_0_register] = get_processor_feature_0_register();
	data[core_1_processor_feature_1_register] = get_processor_feature_1_register();
	data[core_1_memory_model_feature_0_register] = get_memory_model_feature_0_register();

	data[core_1_L1_I_cache_info] = get_cache_info(1, CACHE_INFO, I_CACHE);
	data[core_1_L1_I_cache_size] = get_cache_info(1, CACHE_SIZE, I_CACHE);
	data[core_1_L1_I_cache_num_sets] = get_cache_info(1, NUM_SET, I_CACHE);
	data[core_1_L1_I_cache_line_size] = get_cache_info(1, LINE_SIZE, I_CACHE);
	data[core_1_L1_I_cache_associativity] = get_cache_info(1, ASSOCIATIVITY, I_CACHE);

	data[core_1_L1_D_cache_info] = get_cache_info(1, CACHE_INFO, D_CACHE);
	data[core_1_L1_D_cache_size] = get_cache_info(1, CACHE_SIZE, D_CACHE);
	data[core_1_L1_D_cache_num_sets] = get_cache_info(1, NUM_SET, D_CACHE);
	data[core_1_L1_D_cache_line_size] = get_cache_info(1, LINE_SIZE, D_CACHE);
	data[core_1_L1_D_cache_associativity] = get_cache_info(1, ASSOCIATIVITY, D_CACHE);

	data[system_L2_cache_enable] = get_L2_cache_enable();
	data[system_L2_unified_cache_info] = get_cache_info(2, CACHE_INFO, UNIFIED_CACHE);
	data[system_L2_unified_cache_size] = get_cache_info(2, CACHE_SIZE, UNIFIED_CACHE);
	data[system_L2_unified_cache_num_sets] = get_cache_info(2, NUM_SET, UNIFIED_CACHE);
	data[system_L2_unified_cache_line_size] = get_cache_info(2, LINE_SIZE, UNIFIED_CACHE);
	data[system_L2_unified_cache_associativity] = get_cache_info(2, ASSOCIATIVITY, UNIFIED_CACHE);

	data[core_1_system_control_register] = get_system_control_register();
	data[core_1_auxiliary_control_register] = get_auxiliary_control_register();
}

void get_reg_data_on_core_2(unsigned long* data)
{
	data[core_2_main_id_register] = get_main_id_register();
	data[core_2_TLB_type_register]= get_TLB_type_register();
	data[core_2_processor_feature_0_register] = get_processor_feature_0_register();
	data[core_2_processor_feature_1_register] = get_processor_feature_1_register();
	data[core_2_memory_model_feature_0_register] = get_memory_model_feature_0_register();

	data[core_2_L1_I_cache_info] = get_cache_info(1, CACHE_INFO, I_CACHE);
	data[core_2_L1_I_cache_size] = get_cache_info(1, CACHE_SIZE, I_CACHE);
	data[core_2_L1_I_cache_num_sets] = get_cache_info(1, NUM_SET, I_CACHE);
	data[core_2_L1_I_cache_line_size] = get_cache_info(1, LINE_SIZE, I_CACHE);
	data[core_2_L1_I_cache_associativity] = get_cache_info(1, ASSOCIATIVITY, I_CACHE);

	data[core_2_L1_D_cache_info] = get_cache_info(1, CACHE_INFO, D_CACHE);
	data[core_2_L1_D_cache_size] = get_cache_info(1, CACHE_SIZE, D_CACHE);
	data[core_2_L1_D_cache_num_sets] = get_cache_info(1, NUM_SET, D_CACHE);
	data[core_2_L1_D_cache_line_size] = get_cache_info(1, LINE_SIZE, D_CACHE);
	data[core_2_L1_D_cache_associativity] = get_cache_info(1, ASSOCIATIVITY, D_CACHE);

	data[system_L2_cache_enable] = get_L2_cache_enable();
	data[system_L2_unified_cache_info] = get_cache_info(2, CACHE_INFO, UNIFIED_CACHE);
	data[system_L2_unified_cache_size] = get_cache_info(2, CACHE_SIZE, UNIFIED_CACHE);
	data[system_L2_unified_cache_num_sets] = get_cache_info(2, NUM_SET, UNIFIED_CACHE);
	data[system_L2_unified_cache_line_size] = get_cache_info(2, LINE_SIZE, UNIFIED_CACHE);
	data[system_L2_unified_cache_associativity] = get_cache_info(2, ASSOCIATIVITY, UNIFIED_CACHE);

	data[core_2_system_control_register] = get_system_control_register();
	data[core_2_auxiliary_control_register] = get_auxiliary_control_register();
}

static void get_reg_data_on_each_core(void* arg) {
	int coreIndex;
	unsigned long* data = arg;
	coreIndex = smp_processor_id();
	switch (coreIndex) {
	case 0:
		get_reg_data_on_core_0(data);
		data[core_0_frequency] = get_arm_cpu_freq();
		data[core_0_floating_point_system_id_register] = get_floating_point_system_id_register();
		data[core_0_floating_point_status_and_control_register] = get_floating_point_status_and_control_register();
		data[core_0_floating_point_exception_register] = get_floating_point_exception_register();
		break;
	case 1:
		get_reg_data_on_core_1(data);
		data[core_1_frequency] = get_arm_cpu_freq();
		data[core_1_floating_point_system_id_register] = get_floating_point_system_id_register();
		data[core_1_floating_point_status_and_control_register] = get_floating_point_status_and_control_register();
		data[core_1_floating_point_exception_register] = get_floating_point_exception_register();
		break;
	case 2:
		get_reg_data_on_core_2(data);
		data[core_2_frequency] = get_arm_cpu_freq();
		data[core_2_floating_point_system_id_register] = get_floating_point_system_id_register();
		data[core_2_floating_point_status_and_control_register] = get_floating_point_status_and_control_register();
		data[core_2_floating_point_exception_register] = get_floating_point_exception_register();
		break;
	default:
		printk("UNKNOWN CORE ID:0x%x\n", coreIndex);
		break;
	}
}

static void get_arm_all_register_info(unsigned long* info)
{
	info[core_0_index] = 0;
	on_each_cpu(get_reg_data_on_each_core, info, 1);
	info[SDRAM_type] = get_SDRAM_type();
	info[memory_address_map_area_length] = get_memory_address_map_area_length();
	info[core_0_index] = 0;
	info[core_1_index] = 1;
	info[core_2_index] = 2;
	info[core_0_online] = get_online_cpu_info(0);
	info[core_1_online] = get_online_cpu_info(1);
	info[core_2_online] = get_online_cpu_info(2);
}

unsigned long get_arm_target_raw_data_length(void)
{
	return total_length;
}

int get_all_register_info(unsigned long* info)
{
	unsigned long rawData[total_length];
	memset(rawData, -1, sizeof(rawData));

	get_arm_all_register_info(rawData);

	if (copy_to_user(info, rawData, sizeof(rawData)) != 0)
	{
		return -EFAULT;
	}

	return 0;
}
