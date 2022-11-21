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

#include "getTargetInfo_pj4.h"
#include "constants.h"
#include "common.h"

#include <asm/io.h>
#include <linux/version.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/smp.h>
#include <vfpinstr.h>
#include <asm/vfp.h>

#if defined(CONFIG_DVFM_MMP2)
#include <mach/mmp2_dvfm.h>
#include <mach/dvfm.h>
#endif

static unsigned long get_clock_info(enum CLOCK_TYPE type) {
#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35))
	return -1;
#else
	struct op_info *info = NULL;
	struct mmp2_md_opt *op;
	int result;
	result = dvfm_get_op(&info);
	if (result == -EINVAL)
	return -1;
	op = (struct mmp2_md_opt *) info->op;

	switch (type) {
		case DDR_clock:
			return op->dclk * MHZ_TO_KHZ;
		case L2_clock:
			return op->xpclk * MHZ_TO_KHZ;
		case axi_bus_clock:
			return op->aclk * MHZ_TO_KHZ;
		case processor_clock:
			return op->pclk * MHZ_TO_KHZ;
		default:
			return CLOCK_TYPE_UNKNOWN;
	}
#endif
}

/* Algorithm:
 * cache_size = line_size * number_sets * associativity * 4 Bytes
 */
static unsigned long calc_cache_size(unsigned long id_reg, unsigned long* num_sets, unsigned long* assoc, unsigned long* line_size)
{
	unsigned long cache_size = 0;
	*num_sets   = px_get_bits(id_reg, 13, 27) + 1;
	*line_size  = 1 << (px_get_bits(id_reg, 0, 2) + 2);
	*assoc      = px_get_bits(id_reg, 3, 12) + 1;
	cache_size  = (*line_size) * (*num_sets) * (*assoc) * 4;
	return cache_size/B_TO_KB;
}

static unsigned long get_arm_cache_info(enum CACHE_TYPE cache_type, unsigned long cache_level, enum CACHE_DETAIL_INFO detail)
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

static unsigned long get_cache_info(unsigned long level, enum CACHE_DETAIL_INFO detail, enum CACHE_TYPE type)
{
	int cache_level_reg;
	int begin_bit;
	int end_bit;

	/* read Current Cache Level ID Register */
	asm("mrc p15, 1, %0, c0, c0, 1" : "=r" (cache_level_reg));

	begin_bit = 3 * (level - 1);
	end_bit = begin_bit + 2;

	switch (px_get_bits(cache_level_reg, begin_bit, end_bit)) {
	case 0:
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

#if 0
static unsigned long get_cpu_freq(void) {
//	return dvfm_current_core_freq_get() * MHZ_TO_KHZ;
	return 0;
}
#endif

static unsigned long read_reg(unsigned long address) {
	unsigned long regValue = 0;
	void * regAddr = ioremap(address, 4);
	regValue = readl(regAddr);
	if (regAddr != NULL)
		iounmap(regAddr);
	regAddr = NULL;
	return regValue;
}

static unsigned long get_SDRAM_type(void)
{
	unsigned long type;
	unsigned long value = 0;
	enum SDRAM_TYPE DDRType = 0;
	/* read SDRAM Control Register 4 */
	value = read_reg(0xD0000000 + 0x01A0);
	DDRType = px_get_bits(value, 2, 4);
	switch(DDRType)
	{
	case 0:
		type = DDR1;
		break;
	case 1:
		type = DDR2;
		break;
	case 2:
		type = DDR3;
		break;
	case 4:
		type = LPDDR1;
		break;
	case 5:
		if(px_get_bit(value, 8))
			type = LPDDR2_S4;
		else
			type = LPDDR2_S2;
		break;
	default:
		printk("UNKNOWN SDRAM TYPE\n");
		type = SDRAM_TYPE_UNKNOWN;
		break;
	}
	return type;
}

static unsigned long get_memory_address_map_area_length(void)
{
	int length = 0;
	int index  = 0;
	unsigned long value = 0;
	unsigned long data = 0;
	unsigned long memory_address_map_register[4];
	/* read Memory Address Map Register 0 1 2 3 */
	memory_address_map_register[0] = 0xD0000000 + 0x0100;
	memory_address_map_register[1] = 0xD0000000 + 0x0110;
	memory_address_map_register[2] = 0xD0000000 + 0x0130;
	memory_address_map_register[3] = 0xD0000000 + 0x0A30;

	for (; index < 4; index++) {
		value = read_reg(memory_address_map_register[index]);
		/* whether this chip is selected */
		if (px_get_bit(value, 0)) {
			length = px_get_bits(value, 16, 19);
			switch (length) {
			/* area length of this chip, unit:MB */
			case 0x7:
				data += 8;
				break;
			case 0x8:
				data += 16;
				break;
			case 0x9:/* the spec gives two description for 0xB, 0x9 is a guessed value */
				data += 32;
				break;
			case 0xA:
				data += 64;
				break;
			case 0xB:
				data += 128;
				break;
			case 0xC:
				data += 256;
				break;
			case 0xD:
				data += 512;
				break;
			case 0xE:
				data += 1024;
				break;
			case 0xF:
				data += 2048;
				break;
			default:
				printk("UNKNOWN AREA LENGTH\n");
				return -1;
			}
		}
	}
	data *= MB_TO_KB;
	return data;
}

static unsigned long get_main_id_register(void)
{
	unsigned long data;
	__asm__ __volatile__ ("mrc p15, 0, %0, c0, c0, 0" : "=r"(data));
	return data;
}

static unsigned long get_core_0_index(void)
{
	return 0;
}

static unsigned long get_TLB_type_register(void)
{
	unsigned long data;
	__asm__ __volatile__ ("mrc p15, 0, %0, c0, c0, 3" : "=r"(data));
	return data;
}

static unsigned long get_processor_feature_0_register(void)
{
	unsigned long data;
	__asm__ __volatile__ ("mrc p15, 0, %0, c0, c1, 0" : "=r"(data));
	return data;
}

static unsigned long get_processor_feature_1_register(void)
{
	unsigned long data;
	__asm__ __volatile__ ("mrc p15, 0, %0, c0, c1, 1" : "=r"(data));
	return data;
}

static unsigned long get_memory_model_feature_0_register(void)
{
	unsigned long data;
	__asm__ __volatile__ ("mrc p15, 0, %0, c0, c1, 4" : "=r"(data));
	return data;
}

static unsigned long get_control_register(void)
{
	unsigned long data;
	__asm__ __volatile__ ("mrc p15, 0, %0, c1, c0, 0" : "=r"(data));
	return data;
}

static unsigned long get_L2_cache_enable(void)
{
	unsigned long data;
	bool l1_D_cache_enable;
	bool l2_cache_enable;
	/* read auxiliary control register */
	__asm__ __volatile__ ("mrc p15, 0, %0, c1, c0, 1" : "=r"(data));
	l2_cache_enable = px_get_bit(data, 1);
	/* read control register */
	__asm__ __volatile__ ("mrc p15, 0, %0, c1, c0, 0" : "=r"(data));
	l1_D_cache_enable = px_get_bit(data, 2);

	if(l2_cache_enable && l1_D_cache_enable)
		return 1;
	else
		return 0;
}

static unsigned long get_online_cpu_info(int index) {
	return cpu_online(index);
}

static unsigned long get_floating_point_system_id_register(void) {
	unsigned long data;
	data = fmrx(FPSID);
	return data;
}

static unsigned long get_floating_point_status_and_control_register(void) {
	unsigned long data;
	data = fmrx(FPSCR);
	return data;
}

static unsigned long get_floating_point_exception_register(void) {
	unsigned long data;
	data = fmrx(FPEXC);
	return data;
}

static void get_arm_all_register_info(unsigned long* info)
{
	info[main_id_register] = get_main_id_register();
	info[core_0_index] = get_core_0_index();
	info[core_0_frequency] = get_clock_info(processor_clock);
	info[TLB_type_register] = get_TLB_type_register();
	info[processor_feature_0_register] = get_processor_feature_0_register();
	info[processor_feature_1_register] = get_processor_feature_1_register();
	info[memory_model_feature_0_register] = get_memory_model_feature_0_register();
	info[control_register] = get_control_register();

	info[L1_I_cache_info] = get_cache_info(1, CACHE_INFO, I_CACHE);
	info[L1_I_cache_size] = get_cache_info(1, CACHE_SIZE, I_CACHE);
	info[L1_I_cache_num_sets] = get_cache_info(1, NUM_SET, I_CACHE);
	info[L1_I_cache_line_size] = get_cache_info(1, LINE_SIZE, I_CACHE);
	info[L1_I_cache_associativity] = get_cache_info(1, ASSOCIATIVITY, I_CACHE);

	info[L1_D_cache_info] = get_cache_info(1, CACHE_INFO, D_CACHE);
	info[L1_D_cache_size] = get_cache_info(1, CACHE_SIZE, D_CACHE);
	info[L1_D_cache_num_sets] = get_cache_info(1, NUM_SET, D_CACHE);
	info[L1_D_cache_line_size] = get_cache_info(1, LINE_SIZE, D_CACHE);
	info[L1_D_cache_associativity] = get_cache_info(1, ASSOCIATIVITY, D_CACHE);

	info[L2_cache_enable] = get_L2_cache_enable();
	info[L2_unified_cache_info] = get_cache_info(2, CACHE_INFO, UNIFIED_CACHE);
	info[L2_unified_cache_size] = get_cache_info(2, CACHE_SIZE, UNIFIED_CACHE);
	info[L2_unified_cache_num_sets] = get_cache_info(2, NUM_SET, UNIFIED_CACHE);
	info[L2_unified_cache_line_size] = get_cache_info(2, LINE_SIZE, UNIFIED_CACHE);
	info[L2_unified_cache_associativity] = get_cache_info(2, ASSOCIATIVITY, UNIFIED_CACHE);

	info[SDRAM_type] = get_SDRAM_type();
	info[memory_address_map_area_length] = get_memory_address_map_area_length();
	info[DDR_clock_frequency]		= get_clock_info(DDR_clock);
	info[L2_clock_frequency]		= get_clock_info(L2_clock);
	info[axi_bus_clock_frequency]	= get_clock_info(axi_bus_clock);
	info[core_0_online]				= get_online_cpu_info(0);

	info[floating_point_system_id_register] = get_floating_point_system_id_register();
	info[floating_point_status_and_control_register] = get_floating_point_status_and_control_register();
	info[floating_point_exception_register] = get_floating_point_exception_register();
}

unsigned long get_arm_target_raw_data_length(void)
{
	return total_length;
}

int get_all_register_info(unsigned long* info)
{
	unsigned long rawData[total_length];
	memset(rawData, 0, sizeof(rawData));

	get_arm_all_register_info(rawData);

	if (copy_to_user(info, rawData, sizeof(rawData)) != 0)
	{
		return -EFAULT;
	}

	return 0;
}
