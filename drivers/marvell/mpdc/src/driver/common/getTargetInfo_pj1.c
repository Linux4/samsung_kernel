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

#include "getTargetInfo_pj1.h"
#include "constants.h"
#include "common.h"
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/cpumask.h>
#include <linux/version.h>

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35)) && (LINUX_VERSION_CODE < KERNEL_VERSION(3, 0, 0))
#include <mach/pxa910_dvfm.h>
#include <mach/regs-apmu.h>
#include <mach/regs-mpmu.h>

#elif defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0))
#include <mach/regs-apmu.h>
#include <mach/regs-mpmu.h>
#define OP_NAME_LEN		16
struct pxa910_md_opt {
	int pp;
	int vcc_core;
	int pclk;
	int pdclk;
	int baclk;
	int xpclk;
	int dclk;
	int aclk;
	int cp_pclk;
	int cp_pdclk;
	int cp_baclk;
	int cp_xpclk;
	int cp_clk_src;
	int ap_clk_src;
	int ddr_clk_src;
	int axi_clk_src;
	int gc_clk_src;
	int pll2freq;
	int lpj;
	char name[OP_NAME_LEN];
	unsigned int run_time;
	unsigned int idle_time;
};
#endif

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
extern u32 get_pll2_freq(void);

static unsigned int transfer_to_clock_source(unsigned int select, u32 pll2_freq) {
	unsigned int clock = 0;
	switch (select) {
	case 0:
		clock = 312;
		break;
	case 1:
		clock = 624;
		break;
	case 2:
		clock = pll2_freq;
		break;
	case 3:
		clock = 26;
		break;
	default:
		printk("ERROR:wrong clock source\n");
		break;
	}
	return clock;
}

static void get_current_clock_info(struct pxa910_md_opt *opt) {
	unsigned int pll_select, apmu_ccsr, apmu_cp_ccr;

	apmu_ccsr = __raw_readl(APMU_CCSR);
	apmu_cp_ccr = __raw_readl(APMU_CP_CCR);
	apmu_cp_ccr |= 0x80000000;
	__raw_writel(apmu_cp_ccr, APMU_CP_CCR);
	apmu_cp_ccr &= 0x7fffffff;
	__raw_writel(apmu_cp_ccr, APMU_CP_CCR);
	pll_select = __raw_readl(APMU_PLL_SEL_STATUS);

	opt->pll2freq = get_pll2_freq();

	opt->ap_clk_src = transfer_to_clock_source(px_get_bits(pll_select, 2, 3), opt->pll2freq);
	opt->axi_clk_src = transfer_to_clock_source(px_get_bits(pll_select, 6, 7), opt->pll2freq);
	opt->ddr_clk_src = transfer_to_clock_source(px_get_bits(pll_select, 4, 5), opt->pll2freq);

	opt->pclk = opt->ap_clk_src / (px_get_bits(apmu_ccsr, 0, 2) + 1);
	opt->xpclk = opt->ap_clk_src / (px_get_bits(apmu_ccsr, 9, 11) + 1);
	opt->dclk = opt->ddr_clk_src / (px_get_bits(apmu_ccsr, 12, 14) + 1) / 2;
	opt->aclk = opt->axi_clk_src / (px_get_bits(apmu_ccsr, 15, 17)  + 1);
}

#endif

static unsigned long get_clock_info(enum CLOCK_TYPE type) {
#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	struct pxa910_md_opt opt;
	get_current_clock_info(&opt);
	switch (type) {
		case DDR_clock:
		return opt.dclk * MHZ_TO_KHZ;
		case L2_clock:
		return opt.xpclk * MHZ_TO_KHZ;
		case axi_bus_clock:
		return opt.aclk * MHZ_TO_KHZ;
		case processor_frequency:
		return opt.pclk * MHZ_TO_KHZ;
		default:
		return CLOCK_TYPE_UNKNOWN;
	}
#else
	return UNKNOWN_VALUE;
#endif
}

static unsigned long get_id_code_register(void) {
	unsigned long data;
	__asm__ __volatile__ ("mrc p15, 0, %0, c0, c0, 0" : "=r"(data));
	return data;
}

static unsigned long get_core_0_index(void) {
	return 0;
}

static unsigned long get_control_register(void) {
	unsigned long data;
	__asm__ __volatile__ ("mrc p15, 0, %0, c1, c0, 0" : "=r"(data));
	return data;
}

static unsigned long get_cpu_online(int index) {
	return cpu_online(index);
}

static unsigned long get_L1_cache_info(void) {
	unsigned long data;
	__asm__ __volatile__ ("mrc p15, 0, %0, c0, c0, 1" : "=r"(data));
	return data;
}

//data cache size unit is in KB
static unsigned long get_L1_D_cache_size(void) {
	unsigned long data, info;
	__asm__ __volatile__ ("mrc p15, 0, %0, c0, c0, 1" : "=r"(data));
	info = px_get_bits(data, 18, 21);
	return 1 << (info -1);
}

static unsigned long get_L1_D_cache_set_associativity(void) {
	unsigned long data, info;
	__asm__ __volatile__ ("mrc p15, 0, %0, c0, c0, 1" : "=r"(data));
	info = px_get_bits(data, 15, 17);
	return 1 << info;
}

static unsigned long get_L1_D_cache_line_length(void) {
	unsigned long data, info;
	__asm__ __volatile__ ("mrc p15, 0, %0, c0, c0, 1" : "=r"(data));
	info = px_get_bits(data, 15, 17);
	switch(info) {
	case 2:
		return 8;
	default:
		return UNKNOWN_VALUE;
	}
}

static unsigned long get_L1_D_cache_num_sets(void) {
	unsigned long size, associativity, line_length, num_sets;
	size 			= get_L1_D_cache_size() * KB_TO_B;
	associativity 	= get_L1_D_cache_set_associativity();
	line_length		= get_L1_D_cache_line_length();
	num_sets		= size / (associativity * line_length * 4);
	return num_sets;
}

static unsigned long get_L1_I_cache_size(void) {
	unsigned long data, info;
	__asm__ __volatile__ ("mrc p15, 0, %0, c0, c0, 1" : "=r"(data));
	info = px_get_bits(data, 6, 9);
	return 1 << (info -1);
}

static unsigned long get_L1_I_cache_set_associativity(void) {
	unsigned long data, info;
	__asm__ __volatile__ ("mrc p15, 0, %0, c0, c0, 1" : "=r"(data));
	info = px_get_bits(data, 3, 5);
	switch(info) {
	case 0:
		return 1;//Direct mapped
	case 1:
		return 2;//2-way- Set-Associative
	case 2:
		return 4;//4-way- Set-Associative
	default:
		return UNKNOWN_VALUE;
	}
}

static unsigned long get_L1_I_cache_line_length(void) {
	unsigned long data, info;
	__asm__ __volatile__ ("mrc p15, 0, %0, c0, c0, 1" : "=r"(data));
	info = px_get_bits(data, 0, 1);
	switch(info) {
	case 2:
		return 8;
	default:
		return UNKNOWN_VALUE;
	}
}

static unsigned long get_L1_I_cache_num_sets(void) {
	unsigned long size, associativity, line_length, num_sets;
	size 			= get_L1_I_cache_size() * KB_TO_B;
	associativity 	= get_L1_I_cache_set_associativity();
	line_length		= get_L1_I_cache_line_length();
	num_sets		= size / (associativity * line_length * 4);
	return num_sets;
}

static unsigned long get_L2_unified_cache_info(void) {
	unsigned long data;
	__asm__ __volatile__ ("mrc p15, 1, %0, c0, c0, 1" : "=r"(data));
	return data;
}

static unsigned long get_L2_unified_cache_way_size(void) {
	unsigned long data, info;
	__asm__ __volatile__ ("mrc p15, 1, %0, c0, c0, 1" : "=r"(data));
	info = px_get_bits(data, 20, 23);
	switch(info) {
	case 2:
		return 32;//in KB
	case 3:
		return 64;//in KB
	default:
		return UNKNOWN_VALUE;
	}
}

static unsigned long get_L2_unified_cache_associativity(void) {
	unsigned long data, info;
	__asm__ __volatile__ ("mrc p15, 1, %0, c0, c0, 1" : "=r"(data));
	info = px_get_bits(data, 15, 19);
	switch(info) {
	case 0:
		//printk("L2 Cache not present\n");
		return UNKNOWN_VALUE;
	case 8:
		return 8;//8-way
	default:
		return UNKNOWN_VALUE;
	}
}

static unsigned long get_L2_unified_cache_line_length(void) {
	unsigned long data, info;
	__asm__ __volatile__ ("mrc p15, 1, %0, c0, c0, 1" : "=r"(data));
	info = px_get_bits(data, 12, 13);
	switch(info) {
	case 0:
		return 32;//32 bytes/line
	default:
		return UNKNOWN_VALUE;
	}
}

static unsigned long get_L2_unified_num_sets(void) {
	unsigned long way_size, associativity, line_length, num_sets;
	way_size 		= get_L2_unified_cache_way_size() * KB_TO_B;
	associativity 	= get_L2_unified_cache_associativity();
	line_length		= get_L2_unified_cache_line_length();
	num_sets 		= way_size / (associativity * line_length * 4);
	return num_sets;
}

static unsigned long read_reg(unsigned long address) {
	unsigned long regValue = 0;
	void * regAddr = ioremap(address, 4);
	regValue = readl(regAddr);
	if (regAddr != NULL)
		iounmap(regAddr);
	regAddr = NULL;
	return regValue;
}

static unsigned long get_memory_address_map_area_length(void) {
	unsigned long data, info, value;
	unsigned long address[2];
	int index;

	//address of Memory Address Map Register 0 and 1
	address[0] = 0x0100 + 0xB0000000;
	address[1] = 0x0110 + 0xB0000000;
	value = 0;
	for (index = 0; index < 2; index++) {
		data = read_reg(address[index]);
		//whether this chip is selected
		if (px_get_bit(data, 0)) {
			info = px_get_bits(data, 16, 19);
			switch (info) {
			//area length of this chip, unit:MB
			case 0x7:
				value += 8;//8 MB
				break;
			case 0x8:
				value += 16;//16 MB
				break;
			case 0x9:
				value += 32;//32 MB
				break;
			case 0xA:
				value += 64;//64 MB
				break;
			case 0xB:
				value += 128;//128 MB
				break;
			case 0xC:
				value += 256;//256 MB
				break;
			case 0xD:
				value += 512;//512 MB
				break;
			case 0xE:
				value += 1024;//1024 MB
				break;
			case 0xF:
				value += 2048;//2048 MB
				break;
			default:
				printk("UNKNOWN AREA LENGTH\n");
				return UNKNOWN_VALUE;
			}
		}
	}
	return (value * MB_TO_KB);
}

static unsigned long get_SDRAM_type(void) {
	unsigned long data, info, address;
	address = 0x01A0 + 0xB0000000;
	data = read_reg(address);
	info = px_get_bits(data, 2, 4);
	switch(info) {
	case 0x0:
		return DDR1;
	case 0x4:
		return MOBILE_DDR1;
	case 0x5:
		return LPDDR2;
	default:
		return SDRAM_TYPE_UNKNOWN;
	}
}

static void get_arm_all_register_info(unsigned long* info)
{
	info[id_code_register]						= get_id_code_register();
	info[core_0_index]							= get_core_0_index();
	info[core_0_online]							= get_cpu_online(0);
	info[control_register]						= get_control_register();

	info[L1_cache_info]							= get_L1_cache_info();
	info[L1_D_cache_size]						= get_L1_D_cache_size();
	info[L1_D_cache_num_sets]					= get_L1_D_cache_num_sets();
	info[L1_D_cache_set_associativity] 			= get_L1_D_cache_set_associativity();
	info[L1_D_cache_line_length]				= get_L1_D_cache_line_length();

	info[L1_I_cache_size]						= get_L1_I_cache_size();
	info[L1_I_cache_num_sets]					= get_L1_I_cache_num_sets();
	info[L1_I_cache_set_associativity] 			= get_L1_I_cache_set_associativity();
	info[L1_I_cache_line_length]				= get_L1_I_cache_line_length();

	info[L2_unified_cache_info]					= get_L2_unified_cache_info();
	info[L2_unified_cache_way_size]				= get_L2_unified_cache_way_size();
	info[L2_unified_num_sets]					= get_L2_unified_num_sets();
	info[L2_unified_cache_associativity]		= get_L2_unified_cache_associativity();
	info[L2_unified_cache_line_length]			= get_L2_unified_cache_line_length();

	info[memory_address_map_area_length]		= get_memory_address_map_area_length();
	info[DDR_type]								= get_SDRAM_type();

	info[DDR_clock_frequency]					= get_clock_info(DDR_clock);
	info[L2_clock_frequency]					= get_clock_info(L2_clock);
	info[axi_bus_clock_frequency]				= get_clock_info(axi_bus_clock);
	info[processor_0_frequency]					= get_clock_info(processor_frequency);

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
