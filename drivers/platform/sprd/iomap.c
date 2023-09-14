/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <asm/io.h>
#include <soc/sprd/sci_glb_regs.h>

struct iotable_sprd io_addr_sprd;
EXPORT_SYMBOL_GPL(io_addr_sprd);

#define ADD_SPRD_DEVICE_BY_COMPAT(compat, id)		\
do {							\
	np = of_find_compatible_node(NULL, NULL, compat);\
	if (of_can_translate_address(np)) {		\
		of_address_to_resource(np, 0, &res);	\
		io_addr_sprd.id.paddr = res.start;	\
		io_addr_sprd.id.length =		\
			resource_size(&res);		\
		io_addr_sprd.id.vaddr = base + res.start;\
		pr_debug("sprd io map: phys=%16lx virt=%16lx size=%16lx\n", \
	io_addr_sprd.id.paddr, io_addr_sprd.id.vaddr, io_addr_sprd.id.length);\
	}						\
} while (0)

#define ADD_SPRD_DEVICE_BY_NAME(name, id)		\
do {							\
	np = of_find_node_by_name(NULL, name);		\
	if (of_can_translate_address(np)) {		\
		of_address_to_resource(np, 0, &res);	\
		io_addr_sprd.id.paddr = res.start;	\
		io_addr_sprd.id.length =		\
			resource_size(&res);		\
		io_addr_sprd.id.vaddr = base + res.start;\
		pr_debug("sprd io map: phys=%16lx virt=%16lx size=%16lx\n", \
	io_addr_sprd.id.paddr, io_addr_sprd.id.vaddr, io_addr_sprd.id.length);\
	}						\
} while (0)

static int sprd_create_iomap(void)
{
	struct device_node *np;
	struct resource res;
	unsigned long base;

	base = (unsigned long) ioremap_nocache(0, SZ_2G);
	if (!base)
		panic("%s: unable to alloc such memory!", __func__);
	io_addr_sprd.base = base;
	pr_debug("sprd io map base address: %016x\n", base);

	ADD_SPRD_DEVICE_BY_COMPAT("sprd,ahb", AHB);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,apbckg", APBCKG);
	//ADD_SPRD_DEVICE_BY_COMPAT("sprd,hwlock0", HWLOCK0);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,pub_apb", PUB);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,aon_apb", AONAPB);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,pmu_apb", PMU);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,mm_ahb", MMAHB);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,mm_clk", MMCKG);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,ap_apb", APBREG);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,dma0", DMA0);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,gpu_apb", GPUAPB);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,ap_ckg", APCKG);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,aon_dma", AONDMA);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,pwm", PWM);
	ADD_SPRD_DEVICE_BY_NAME("hwspinlock0", HWSPINLOCK0);
	ADD_SPRD_DEVICE_BY_NAME("hwspinlock1", HWSPINLOCK1);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,intc2", INTC2);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,aonckg", AONCKG);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,core", CORE);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,gpuckg", GPUCKG);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,int", INT);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,intc0", INTC0);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,intc1", INTC1);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,intc3", INTC3);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,uidefuse", UIDEFUSE);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,isp", ISP);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,csi2", CSI2);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,d-eic-gpio", EIC);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,ipi", IPI);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,dcam", DCAM);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,syscnt", SYSCNT);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,pin", PIN);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,d-gpio-gpio", GPIO);
#if defined(CONFIG_ARCH_SCX35LT8)
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,codecahb", CODECAHB);
#endif
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,lpddr2", LPDDR2);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,uart0", UART0);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,uart1", UART1);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,uart2", UART2);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,uart3", UART3);
	ADD_SPRD_DEVICE_BY_COMPAT("sprd,uart4", UART4);
	return 0;
}

early_initcall(sprd_create_iomap);
