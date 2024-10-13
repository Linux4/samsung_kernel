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
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <asm/io.h>
#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include <mach/board.h>
#include <linux/sprd_iommu.h>
#include <linux/delay.h>

#include "dcam_drv.h"

//#define PARSE_DEBUG

#ifdef PARSE_DEBUG
	#define PARSE_TRACE             printk
#else
	#define PARSE_TRACE             pr_debug
#endif

#define  CLK_MM_I_IN_CLOCKTREE  1

#ifdef CONFIG_OF
uint32_t		dcam_regbase;
#else
#endif
static atomic_t	mm_enabe_cnt = ATOMIC_INIT(0);

void   parse_baseaddress(struct device_node	*dn)
{
#ifdef CONFIG_OF
	struct resource  r;

	if (0 == of_address_to_resource(dn, 0,&r)) {
		PARSE_TRACE("DCAM BASE=0x%x \n",r.start);
		dcam_regbase = r.start;
	} else {
		printk("DCAM BASE ADDRESS ERROR\n");
	}
#endif
}

uint32_t   parse_irq(struct device_node *dn)
{
#ifdef CONFIG_OF
	return irq_of_parse_and_map(dn, 0);
#else
	return DCAM_IRQ;
#endif
}

struct clk  * parse_clk(struct device_node *dn, char *clkname)
{
#ifdef CONFIG_OF
	PARSE_TRACE("parse_clk %s \n",clkname);
	return of_clk_get_by_name(dn, clkname);
#else
	return clk_get(NULL, clkname);
#endif
}

int32_t clk_mm_i_eb(struct device_node *dn, uint32_t enable)
{
#if CLK_MM_I_IN_CLOCKTREE
	int	ret = 0;
	struct clk*	clk_mm_i = NULL;

	clk_mm_i = parse_clk(dn, "clk_mm_i");
	if (IS_ERR(clk_mm_i)) {
		printk("clk_mm_i_eb: get fail.\n");
		return -1;
	}

	if(enable){
		ret = clk_enable(clk_mm_i);
		if (ret) {
			printk("clk_mm_i_eb: enable fail.\n");
			return -1;
		}
#if defined(CONFIG_SPRD_IOMMU)
		{
			sprd_iommu_module_enable(IOMMU_MM);
		}
#endif
		PARSE_TRACE("clk_mm_i_eb enable ok.\n");
		atomic_inc(&mm_enabe_cnt);
	}else{
#if defined(CONFIG_SPRD_IOMMU)
		{
			sprd_iommu_module_disable(IOMMU_MM);
		}
#endif
		clk_disable(clk_mm_i);
		clk_put(clk_mm_i);
		clk_mm_i = NULL;
		udelay(500);
		atomic_dec(&mm_enabe_cnt);
		PARSE_TRACE("clk_mm_i_eb disable ok.\n");
	}

#else
	if(enable){
		if(atomic_inc_return(&mm_enabe_cnt) == 1){
			REG_OWR(REG_AON_APB_APB_EB0,BIT_MM_EB);
			REG_AWR(REG_PMU_APB_PD_MM_TOP_CFG,~BIT_PD_MM_TOP_FORCE_SHUTDOWN);
			REG_OWR(REG_MM_AHB_GEN_CKG_CFG,BIT_SENSOR_CKG_EN|BIT_DCAM_AXI_CKG_EN);
			PARSE_TRACE("mm enable ok.\n");
		}
	}else{
		if(atomic_dec_return(&mm_enabe_cnt) == 0){
			REG_AWR(REG_MM_AHB_GEN_CKG_CFG,~(BIT_SENSOR_CKG_EN|BIT_DCAM_AXI_CKG_EN));
			REG_OWR(REG_PMU_APB_PD_MM_TOP_CFG,BIT_PD_MM_TOP_FORCE_SHUTDOWN);
			REG_AWR(REG_AON_APB_APB_EB0,~BIT_MM_EB);
			udelay(500);
			PARSE_TRACE("mm disable ok.\n");
		}
	}
#endif

	PARSE_TRACE("mm_enabe_cnt = %d.\n",atomic_read(&mm_enabe_cnt));
#ifdef PARSE_DEBUG
	mm_clk_register_trace();
#endif
	return 0;
}

