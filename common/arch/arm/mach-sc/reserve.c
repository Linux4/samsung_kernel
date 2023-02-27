/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>

#include <mach/hardware.h>
#include <mach/board.h>
#include <asm/memory.h>
#include <linux/memblock.h>
#include <linux/dma-contiguous.h>
#include <linux/platform_device.h>
#include "devices.h"

#include <asm/setup.h>

#ifdef CONFIG_SEC_LOG_BUF_NOCACHE
extern phys_addr_t sec_logbuf_base_addr;
extern unsigned sec_logbuf_size;
extern int sec_log_buf_nocache_enable;
#endif

static int __init __iomem_reserve_memblock(void)
{
	int ret;

#ifndef CONFIG_CMA
	if (memblock_is_region_reserved(SPRD_ION_MEM_BASE, SPRD_ION_MEM_SIZE))
		return -EBUSY;
	if (memblock_reserve(SPRD_ION_MEM_BASE, SPRD_ION_MEM_SIZE))
		return -ENOMEM;
#else
#ifndef CONFIG_OF
	ret = dma_declare_contiguous_reserved(&sprd_ion_dev.dev, SPRD_ION_MEM_SIZE, SPRD_ION_MEM_BASE, 0, CMA_RESERVE, CMA_THRESHOLD);
	if (unlikely(ret))
	{
		pr_err("reserve CMA area(base:%x size:%x) for ION failed!!!\n", SPRD_ION_MEM_BASE, SPRD_ION_MEM_SIZE);
		return -ENOMEM;
	}
	pr_info("reserve CMA area(base:%x size:%x) for ION\n", SPRD_ION_MEM_BASE, SPRD_ION_MEM_SIZE);
#endif
#endif
	return 0;
}

#ifdef CONFIG_PSTORE_RAM
int __init __ramconsole_reserve_memblock(void)
{
	if (memblock_is_region_reserved(SPRD_RAM_CONSOLE_START, SPRD_RAM_CONSOLE_SIZE))
		return -EBUSY;
	if (memblock_reserve(SPRD_RAM_CONSOLE_START, SPRD_RAM_CONSOLE_SIZE))
		return -ENOMEM;
	return 0;
}
#endif

#ifdef CONFIG_FB_LCD_RESERVE_MEM
static int __init __fbmem_reserve_memblock(void)
{
	pr_err("__fbmem_reserve_memblock,SPRD_FB_MEM_BASE:%x,SPRD_FB_MEM_SIZE:%x\n",SPRD_FB_MEM_BASE,SPRD_FB_MEM_SIZE);
	if (memblock_is_region_reserved(SPRD_FB_MEM_BASE, SPRD_FB_MEM_SIZE))
		return -EBUSY;
	if (memblock_reserve(SPRD_FB_MEM_BASE, SPRD_FB_MEM_SIZE))
		return -ENOMEM;
	pr_err("__fbmem_reserve_memblock-end,\n");
	return 0;
}
#endif

#if defined(CONFIG_SIPC) && !defined(CONFIG_ARCH_SC8825)
int __init __sipc_reserve_memblock(void)
{
	uint32_t smem_size = 0;

#ifdef CONFIG_SIPC_TD
	if (memblock_reserve(CPT_START_ADDR, CPT_TOTAL_SIZE))
		return -ENOMEM;
	smem_size += CPT_SMEM_SIZE;
#endif

#ifdef CONFIG_SIPC_WCDMA
	if (memblock_reserve(CPW_START_ADDR, CPW_TOTAL_SIZE))
		return -ENOMEM;
	smem_size += CPW_SMEM_SIZE;
	memblock_free(CPW_START_ADDR, CPW_TOTAL_SIZE);
	memblock_remove(CPW_START_ADDR, CPW_TOTAL_SIZE);
#endif

#ifdef CONFIG_SIPC_WCN
	if (memblock_reserve(WCN_START_ADDR, WCN_TOTAL_SIZE))
		return -ENOMEM;
	smem_size += WCN_SMEM_SIZE;
#endif

	if (memblock_reserve(SIPC_SMEM_ADDR, smem_size))
		return -ENOMEM;

	return 0;
}
#endif

#ifdef CONFIG_SPRD_IQ
static phys_addr_t s_iq_addr = 0xffffffff;
int in_iqmode(void);

int __init __sprd_iq_memblock(void)
{
	int i;
	struct membank bank;
	bool bfound = false;
	if(!in_iqmode())
		return -EINVAL;
	for(i = meminfo.nr_banks; i > 0; i--) {
		printk("sprd_iq high: %d, start %d, size %d \n", meminfo.bank[i-1].highmem, meminfo.bank[i-1].start,
			meminfo.bank[i-1].size);
		if(meminfo.bank[i-1].highmem || meminfo.bank[i-1].size < SPRD_IQ_SIZE)
			continue;
		bank.start = meminfo.bank[i-1].start;
		bank.size = meminfo.bank[i-1].size;
		while(bank.size - SPRD_IQ_SIZE > 0) {
			if(memblock_is_region_reserved(bank.start + bank.size - SPRD_IQ_SIZE, SPRD_IQ_SIZE)) {
				bank.size -= SZ_1M;
			} else {
				bfound = true;
				break;
			}
		}
		if(bfound)
			break;
	}
	printk("sprd_iq found mem %d \n", bfound ? bank.size : 0);
	if(bfound) {
		int err = memblock_reserve(bank.start + bank.size - SPRD_IQ_SIZE, SPRD_IQ_SIZE);
		if(0 != err)
		{
			printk("sprd_iq memblock_reserve err =  %d \n", err);
			return -ENOMEM;
		}
		else {
			s_iq_addr = bank.start + bank.size - SPRD_IQ_SIZE;
			return 0;
		}
	} else
		return -ENOMEM;

}

phys_addr_t sprd_iq_addr(void)
{
	return s_iq_addr;
}

#endif

#ifdef SPRD_ION_BASE_USE_VARIABLE
phys_addr_t sprd_reserve_limit;
extern phys_addr_t arm_lowmem_limit;
#endif
static int dram_cs_num = 0;
static u32 dram_cs0_size = 0x0;
static int __init mem_cs_num_get(char *str)
{
	dram_cs_num=simple_strtoul(str,0, 16);
	printk("mem_cs_get dram_cs_num=%d\n",dram_cs_num);
	return 0;
}
static int __init mem_cs0_size_get(char *str)
{
	dram_cs0_size=simple_strtoul(str,0, 16);
	printk("mem_cs0_size_get dram_cs0_size =0x%08x\n",dram_cs0_size);
	return 0;
}
early_param("mem_cs", mem_cs_num_get);
early_param("mem_cs0_sz", mem_cs0_size_get);
static int __ddr_training_memblock(void)
{
	memblock_reserve(CONFIG_PHYS_OFFSET, PAGE_SIZE);
	if(2 == dram_cs_num) {
		if(0==dram_cs0_size){
			pr_err("dram_cs0_size = 0, error, please check the dram size\n");
			return -ENOMEM;
		}
		memblock_reserve(CONFIG_PHYS_OFFSET+dram_cs0_size, PAGE_SIZE);
	}
	return 0;
}
void __init sci_reserve(void)
{
	int ret;
#ifdef CONFIG_ARCH_SCX30G
	__ddr_training_memblock();
#endif
#ifdef SPRD_ION_BASE_USE_VARIABLE
	/*sprd_reserve_limit is used save arm_lowmem_limit,will be use by ION*/
	sprd_reserve_limit = arm_lowmem_limit;
#endif

#ifndef CONFIG_OF
	ret = __iomem_reserve_memblock();
	if (ret != 0)
		pr_err("Fail to reserve mem for iomem. errno=%d\n", ret);

#if defined(CONFIG_SIPC) && !defined(CONFIG_ARCH_SC8825)
	ret = __sipc_reserve_memblock();
	if (ret != 0)
		pr_err("Fail to reserve mem for sipc. errno=%d\n", ret);
#endif

#ifdef CONFIG_PSTORE_RAM
	ret = __ramconsole_reserve_memblock();
	if (ret != 0)
		pr_err("Fail to reserve mem for ram_console. errno=%d\n", ret);
#endif

#ifdef CONFIG_FB_LCD_RESERVE_MEM
	ret = __fbmem_reserve_memblock();
	if (ret != 0)
		pr_err("Fail to reserve mem for framebuffer . errno=%d\n", ret);
#endif
#endif

#ifdef CONFIG_SPRD_IQ
	ret = __sprd_iq_memblock();
	if (ret != 0)
		printk("Fail to reserve mem for sprd iq. errno=%d\n", ret);
#endif
#ifdef CONFIG_SEC_LOG_BUF_NOCACHE
	if(sec_log_buf_nocache_enable==1)
        {
                unsigned int size = (1024*1024);
				ret = memblock_remove(sec_logbuf_base_addr, (phys_addr_t)size);
                if( ret != 0 )
                        pr_err("Fail to remove memory for nocache sec log buf\n");
        }
#endif
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG_NONCACHED
	{
		extern phys_addr_t phy_sec_debug_log_struct;
		extern size_t phy_sec_debug_log_struct_size;

		ret = memblock_remove(phy_sec_debug_log_struct,(phys_addr_t)phy_sec_debug_log_struct_size);
		if(ret != 0)
			pr_err("Fail to remove memory for nocache sec_debug_log\n");		
	}
#endif
}
