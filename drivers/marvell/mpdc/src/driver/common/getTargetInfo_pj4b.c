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

#include "getTargetInfo.h"
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

/* cache_size = line_size * number_sets * associativity * 4 Bytes */
unsigned long calc_cache_size(unsigned long id_reg, unsigned long* num_sets, unsigned long* assoc, unsigned long* line_size)
{
	unsigned long cache_size = 0;
	*num_sets   = px_get_bits(id_reg, 13, 27) + 1;
	*line_size  = 1 << (px_get_bits(id_reg, 0, 2) + 2);
	*assoc      = px_get_bits(id_reg, 3, 12) + 1;
	cache_size  = (*line_size) * (*num_sets) * (*assoc) * 4;
	return cache_size/B_TO_KB;
}

unsigned long get_arm_cache_info(enum CACHE_TYPE cache_type, unsigned long cache_level, enum CACHE_DETAIL_INFO detail)
{
	unsigned long assoc = 0;
	unsigned long data = 0;
	unsigned long id_reg = 0;
	unsigned long num_sets = 0;
	unsigned long line_size = 0;
	unsigned long cache_size = 0;

	switch (cache_type)
	{
	case I_CACHE://instruction cache only
		data = 1;
		break;
	case D_CACHE:
		data = 0;
		break;
	case UNIFIED_CACHE:
		data = 0;
		break;
	default:
		return CACHE_TYPE_UNKNOWN;
	}

	data |= (cache_level - 1) << 1;

	/* write Cache Size Selection Register to select cache level and cache type*/
	asm("mcr p15, 2, %0, c0, c0, 0" :: "r" (data));
	isb();

	/* read Current Cache Size ID Register */
	asm("mrc p15, 1, %0, c0, c0, 0" : "=r" (id_reg));

	cache_size = calc_cache_size(id_reg, &num_sets, &assoc, &line_size);

	switch (cache_level) {
	case 1:
		switch (cache_type) {
		case I_CACHE:
			switch (detail) {
			case NUM_SET:
				return num_sets;
			case LINE_SIZE:
				return line_size;
			case ASSOCIATIVITY:
				return assoc;
			case CACHE_INFO:
				return id_reg;
			case CACHE_SIZE:
				return cache_size;
			default:
				return -1;
			}
			break;
		case D_CACHE:
			switch (detail) {
			case NUM_SET:
				return num_sets;
			case LINE_SIZE:
				return line_size;
			case ASSOCIATIVITY:
				return assoc;
			case CACHE_INFO:
				return id_reg;
			case CACHE_SIZE:
				return cache_size;
			default:
				return -1;
			}
			break;
		default:
			return -1;
		}
		break;
	case 2:
	case 3:
	case 4:
		switch (cache_type) {
		case UNIFIED_CACHE:
			switch (detail) {
			case NUM_SET:
				return num_sets;
			case LINE_SIZE:
				return line_size;
			case ASSOCIATIVITY:
				return assoc;
			case CACHE_INFO:
				return id_reg;
			case CACHE_SIZE:
				return cache_size;
			default:
				return -1;
			}
			break;
		default:
			return -1;
		}
		break;
	default:
		printk("UNKNOWN Cache Level\n");
		return -1;
	}
}

unsigned long get_cache_info(unsigned long level, enum CACHE_DETAIL_INFO detail, enum CACHE_TYPE type)
{
	int cache_level_reg;
	int begin_bit;
	int end_bit;

	/* read Current Cache Level ID Register */
	asm("mrc p15, 1, %0, c0, c0, 1" : "=r" (cache_level_reg));

	begin_bit = 3 * (level - 1);
	end_bit = begin_bit + 2;

	switch (px_get_bits(cache_level_reg, begin_bit, end_bit)) {
	case 0:/* all the not present level cache arrive here, such as CL7,CL6 and so on */
		break;
	case 1:
		if(type == I_CACHE)
			return get_arm_cache_info(I_CACHE, level, detail);
		break;
	case 2:
		if(type == D_CACHE)
			return get_arm_cache_info(D_CACHE, level, detail);
		break;
	case 3:
		if(type == D_CACHE)
			return get_arm_cache_info(D_CACHE, level, detail);
		else if(type == I_CACHE)
			return get_arm_cache_info(I_CACHE, level, detail);
		break;
	case 4:
		if(type == UNIFIED_CACHE)
			return get_arm_cache_info(UNIFIED_CACHE, level, detail);
		break;
	default:
		break;
	}
	return CACHE_TYPE_UNKNOWN;
}


unsigned long get_arm_cpu_freq(void) {
	return UNKNOWN_VALUE;
}

unsigned long get_main_id_register(void)
{
	unsigned long data;
	__asm__ __volatile__ ("mrc p15, 0, %0, c0, c0, 0" : "=r"(data));
	return data;
}

unsigned long get_TLB_type_register(void)
{
	unsigned long data;
	__asm__ __volatile__ ("mrc p15, 0, %0, c0, c0, 3" : "=r"(data));
	return data;
}

unsigned long get_processor_feature_0_register(void)
{
	unsigned long data;
	__asm__ __volatile__ ("mrc p15, 0, %0, c0, c1, 0" : "=r"(data));
	return data;
}

unsigned long get_processor_feature_1_register(void)
{
	unsigned long data;
	__asm__ __volatile__ ("mrc p15, 0, %0, c0, c1, 1" : "=r"(data));
	return data;
}

unsigned long get_memory_model_feature_0_register(void)
{
	unsigned long data;
	__asm__ __volatile__ ("mrc p15, 0, %0, c0, c1, 4" : "=r"(data));
	return data;
}

unsigned long get_system_control_register(void)
{
	unsigned long data;
	__asm__ __volatile__ ("mrc p15, 0, %0, c1, c0, 0" : "=r"(data));
	return data;
}

unsigned long get_auxiliary_control_register(void)
{
	unsigned long data;
	__asm__ __volatile__ ("mrc p15, 0, %0, c1, c0, 1" : "=r"(data));
	return data;
}


int get_online_cpu_info(int index) {
	return cpu_online(index);
}

unsigned long get_floating_point_system_id_register(void) {
#ifndef PX_SOC_BG2
	unsigned long data;
	data = fmrx(FPSID);
	return data;
#else
	return UNKNOWN_VALUE;
#endif
}

unsigned long get_floating_point_status_and_control_register(void) {
#ifndef PX_SOC_BG2
	unsigned long data;
	data = fmrx(FPSCR);
	return data;
#else
	return UNKNOWN_VALUE;
#endif
}

unsigned long get_floating_point_exception_register(void) {
#ifndef PX_SOC_BG2
	unsigned long data;
	data = fmrx(FPEXC);
	return data;
#else
	return UNKNOWN_VALUE;
#endif
}
