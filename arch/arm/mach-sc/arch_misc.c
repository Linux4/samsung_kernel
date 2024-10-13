/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 * Copyright (C) 2013 steve zhan
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
#include <linux/module.h>
#include <linux/init.h>
#include <linux/irqflags.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/hwspinlock.h>
#include <linux/mm.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/mm.h>

#include <asm/delay.h>
#include <asm/memory.h>
#include <asm/highmem.h>
#include <asm/tlbflush.h>

#include <mach/hardware.h>
#include <mach/adi.h>
#include <mach/irqs.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include <mach/arch_misc.h>
#include "../mm/mm.h"
/*#define DEBUG */
#ifdef DEBUG
#define DEBUGSEE printk
#else
#define DEBUGSEE(...)
#endif

static u32 chip_id __read_mostly;
u32 sci_get_chip_id(void)
{
	return chip_id;
}

u32 sci_get_ana_chip_id(void)
{
#if defined(CONFIG_ARCH_SCX35)
	return (u32)sci_adi_read(ANA_REG_GLB_CHIP_ID_HIGH) << 16 |
		(u32)(sci_adi_read(ANA_REG_GLB_CHIP_ID_LOW) & ~MASK_ANA_VER);
#else
	return 0;
#endif
}

int sci_get_ana_chip_ver(void)
{
#if defined(CONFIG_ARCH_SCX35)
	return ((u32)sci_adi_read(ANA_REG_GLB_CHIP_ID_LOW) & MASK_ANA_VER);
#else
	return 0;
#endif
}

void set_section_ro(unsigned long virt, unsigned long numsections)
{
	pmd_t *pmd;
	unsigned long start = virt;
	unsigned long end = virt + (numsections << SECTION_SHIFT);
	unsigned long pmd_end;

	while (virt < end) {
		pmd = pmd_off_k(virt);

		if ((pmd_val(*pmd) & PMD_TYPE_MASK) != PMD_TYPE_SECT) {
			pr_err("%s: pmd %p=%08lx for %08lx not section map\n",
				__func__, pmd, pmd_val(*pmd), virt);
			return;
		}

		/*
		 * We set section page table entry APX:1 AP:01
		 * Privileged : READ, Unprivileged : No Access
		 */
		*pmd |= PMD_SECT_APX;
		pr_debug("%s: pmd %p=%08lx for %08lx ok\n",
				__func__, pmd, pmd_val(*pmd), virt);
		virt += SECTION_SIZE;
	}

	flush_tlb_kernel_range(start, end);
}

void __iomap_page(unsigned long virt, unsigned long size, int enable)
{
	unsigned long addr = virt, end = virt + (size & ~(SZ_4K - 1));
	u32 *pgd;
	u32 *pte;

	pgd = (u32 *)init_mm.pgd + (addr >> 20);
	pte = (u32 *)__va(*pgd & 0xfffffc00);
	pte += (addr >> 12) & 0xff;

	pr_debug("%s (%d) pgd %p, pte %p, *pte %x\n",
		__FUNCTION__, enable, pgd, pte, *pte);

	if (enable)
		*pte |= 0x3;
	else
		*pte &= ~0x3;

	flush_tlb_kernel_range(virt, end);
}

#define sci_mm_glb_set(reg, bit)	__raw_writel((bit), (void *)REG_GLB_SET(reg))
#define sci_mm_glb_clr(reg, bit)	__raw_writel((bit), (void *)REG_GLB_CLR(reg))
#define sci_mm_reg_set(reg, bit)	__raw_writel(__raw_readl((void *)(reg)) | (bit), (void *)(reg))
#define sci_mm_reg_clr(reg, bit)	__raw_writel((__raw_readl((void *)(reg)) & ~(bit)), (void *)(reg))
#ifdef CONFIG_OF
int local_mm_enable(void *c, int enable)
#else
int sci_mm_enable(void *c, int enable, unsigned long *pflags)
#endif
{
	if (enable) {
		int to = 2000;
		/* FIXME: mm domain power on at first
		 */
		sci_mm_glb_clr(REG_PMU_APB_PD_MM_TOP_CFG, BIT_PD_MM_TOP_FORCE_SHUTDOWN);

		__iomap_page(REGS_MM_AHB_BASE, SZ_4K, enable);
		__iomap_page(REGS_MM_CLK_BASE, SZ_4K, enable);
		__iomap_page(SPRD_DCAM_BASE, SZ_4K, enable);
		__iomap_page(SPRD_VSP_BASE, SZ_4K, enable);

#if defined(CONFIG_ARCH_SCX15)
		sci_glb_set(REG_AON_APB_APB_EB0, BIT_MM_EB);
#else
		sci_mm_glb_clr(REG_PMU_APB_PD_MM_TOP_CFG, BIT_PD_MM_TOP_FORCE_SHUTDOWN);
		sci_glb_set(REG_AON_APB_APB_EB0, BIT_MM_EB);
		/* FIXME: wait a moment for mm domain stable
		 */
		while ((__raw_readl((void *)REG_PMU_APB_PWR_STATUS0_DBG) & BITS_PD_MM_TOP_STATE(-1)) && to--) {
			udelay(50);
		}

		WARN(0 || 0 != sci_glb_read(REG_PMU_APB_PWR_STATUS0_DBG, BITS_PD_MM_TOP_STATE(-1)),
			"MM TOP CFG 0x%08x, TIMEOUT %d\n", __raw_readl((void *)REG_PMU_APB_PD_MM_TOP_CFG),
			(2000 - to) * 50);

		WARN(0 == sci_glb_read(REG_AON_APB_APB_EB0, BIT_MM_EB),
			"AON APB EB0 0x%08x\n", __raw_readl((void *)REG_AON_APB_APB_EB0));

		/* FIXME: force mm clock enable
		 */
		sci_mm_glb_set(REG_AON_APB_APB_EB0, BIT_MM_EB);
		sci_mm_glb_set(REG_MM_AHB_AHB_EB, BIT_MM_CKG_EB);
		sci_mm_reg_set(REG_MM_AHB_GEN_CKG_CFG, BIT_MM_MTX_AXI_CKG_EN | BIT_MM_AXI_CKG_EN);
		sci_mm_reg_set(REG_MM_CLK_MM_AHB_CFG, 0x3);/* default set mm ahb 153.6MHz */
#endif
	} else {
#if defined(CONFIG_ARCH_SCX15)
		sci_glb_clr(REG_AON_APB_APB_EB0, BIT_MM_EB);
#else
		sci_mm_reg_clr(REG_MM_AHB_GEN_CKG_CFG, BIT_MM_MTX_AXI_CKG_EN | BIT_MM_AXI_CKG_EN);
		sci_mm_glb_clr(REG_MM_AHB_AHB_EB, BIT_MM_CKG_EB);

		sci_glb_clr(REG_AON_APB_APB_EB0, BIT_MM_EB);
		/* FIXME: do not force mm domain power off before retention
		*/
		sci_mm_glb_set(REG_PMU_APB_PD_MM_TOP_CFG, BIT_PD_MM_TOP_FORCE_SHUTDOWN);
#endif
		/* FIXME: clean mm io map,
		 * do not access any reg in mm domain after mm clock disabled
		 */
		__iomap_page(REGS_MM_AHB_BASE, SZ_4K, enable);
		__iomap_page(REGS_MM_CLK_BASE, SZ_4K, enable);
		__iomap_page(SPRD_DCAM_BASE, SZ_4K, enable);
		__iomap_page(SPRD_VSP_BASE, SZ_4K, enable);
	}
	return 0;
}

void __init sc_init_chip_id(void)
{
#if defined(CONFIG_ARCH_SC8825)
	chip_id = __raw_readl(REG_AHB_CHIP_ID);
#elif defined(CONFIG_ARCH_SCX35)
	chip_id = __raw_readl((void *)REG_AON_APB_CHIP_ID);
	if (chip_id == 0)
		chip_id = SCX35_ALPHA_TAPOUT;	//alpha Tapout.
#else
	#error "Is chip_id ..?"
#endif
}

static inline int __chip_raw_ddie_write(u32 vreg, u32 or_val, u32 clear_msk)
{
	u32 val = __raw_readl((void *)vreg);
	writel((val & ~clear_msk) | or_val, (void *)vreg);
	return 0;
}

int sci_read_va(u32 vreg, u32 * val)
{
	DEBUGSEE("sci_read_va  vreg = 0x%x type:", vreg);
	if (!val)
		return -1;

	if (unlikely(sci_is_adi_vaddr(vreg))) {
		DEBUGSEE("adie\n");
		*val = sci_adi_read(vreg);
	} else {
		DEBUGSEE("ddie\n");
		*val = __raw_readl((void *)vreg);
	}

	return 0;
}

int sci_write_va(u32 vreg, const u32 or_val, const u32 clear_msk)
{
	DEBUGSEE("sci_write_va\n");

	if (unlikely(sci_is_adi_vaddr(vreg))) {
		DEBUGSEE("adie\n");
		sci_adi_write(vreg, or_val, clear_msk);
	} else {		/*FIXME: is not safe */
		DEBUGSEE("ddie\n");
		__chip_raw_ddie_write(vreg, or_val, clear_msk);
	}
	return 0;
}

int sci_read_pa(u32 preg, u32 * val)
{
	void __iomem *addr;
	int ret = 0;
	int vaddr = 0;

	if (!val)
		return -1;

	DEBUGSEE("sci_read_pa 0x%x\n", preg);
	addr = __va(preg);
	if (!virt_addr_valid(addr)) {
		if (!sci_adi_p2v(preg, &vaddr)) {
			*val = sci_adi_read((u32) vaddr);
		} else {
			addr = ioremap_nocache(preg, PAGE_SIZE);
			if (!addr) {
				printk(KERN_WARNING
				       "unable to map i/o region\n");
				ret = -ENOMEM;
				goto Exit;
			}
			*val = __raw_readl((void *) addr);
			iounmap(addr);
		}
	} else {
		*val = __raw_readl((void *) addr);
	}
Exit:
	return ret;
}

int sci_write_pa(u32 paddr, const u32 or_val, const u32 clear_msk)
{
	int ret = 0;
	u32 vaddr = 0;

	DEBUGSEE("sci_write_pa 0x%x, 0x%x, 0x%x\n", paddr, or_val, clear_msk);
	vaddr = (u32) __va(paddr);
	if (!virt_addr_valid(vaddr)) {
		if (!sci_adi_p2v(paddr, &vaddr)) {
			sci_adi_write(vaddr, or_val, clear_msk);
		} else {
			vaddr = (u32) ioremap_nocache(paddr, PAGE_SIZE);
			if (!vaddr) {
				printk(KERN_WARNING
				       "unable to map i/o region\n");
				ret = -ENOMEM;
				goto Exit;
			}
			DEBUGSEE("vaddr = 0x%x\n", vaddr);
			__chip_raw_ddie_write((u32) vaddr, or_val, clear_msk);
			iounmap((void __iomem *)vaddr);
		}
	} else {
		__chip_raw_ddie_write((u32) vaddr, or_val, clear_msk);
	}
Exit:
	return ret;
}

#ifdef CONFIG_DEBUG_FS
struct sci_lookat {
	const char *name;

#define _LOOKAT_RWPV_					(1<<0)
#define _LOOKAT_INPUT_OUTPUT_DATA_	(1<<1)
	int entry_type;
};
static struct sci_lookat __lookat_array[] = {
	{"addr_rwpv", _LOOKAT_RWPV_},
	{"data", _LOOKAT_INPUT_OUTPUT_DATA_},
};

struct lookat_request {
#define __READ__	(0<<1)
#define __WRITE__	(1<<1)
#define __P__	(0<<0)
#define __V__	(1<<0)
	u32 cmd;
	u32 addr;
	u32 value;		/*if is read, save result to value, if is write, value is need to write */
};
static struct lookat_request __request[5];
static int index = 0;
static int queue_add_element(u32 raw_cmd)
{
	__request[index].cmd = raw_cmd & (0x3);
	__request[index].addr = raw_cmd & (~0x3);
	if (index == ARRAY_SIZE(__request) - 1)
		index = 0;
	else
		index++;
	return 0;
}

static struct lookat_request *__return_last_eliment(void)
{
	if (!index)
		return &__request[ARRAY_SIZE(__request) - 1];
	else
		return &__request[index - 1];
}

static inline int queue_modify_value(u32 value)
{
	struct lookat_request *p = __return_last_eliment();

	if (!(p->cmd & __WRITE__)) {
		return -1;
	} else {		/*write, save the write data that need to send */
		p->value = value;
		return 0;
	}
}

static int do_request(void)
{
	struct lookat_request *p;
	int ret = 0;

	p = __return_last_eliment();
	DEBUGSEE("p->cmd = 0x%x, p->addr = 0x%x ", p->cmd, p->addr);

	if ((p->cmd & __WRITE__) == __WRITE__) {
		if ((p->cmd & __V__) == __V__)
			ret = sci_write_va(p->addr, p->value, ~0);
		else
			ret = sci_write_pa(p->addr, p->value, ~0);
	} else {
		if ((p->cmd & __V__) == __V__)
			ret = sci_read_va(p->addr, &p->value);
		else
			ret = sci_read_pa(p->addr, &p->value);
	}
	DEBUGSEE("p->value = 0x%x\n", p->value);

	return ret;
}

static int __debug_set(void *data, u64 val)
{
	struct sci_lookat *p = data;
	int ret = 0;
	struct lookat_request *r;

	if (p->entry_type == _LOOKAT_INPUT_OUTPUT_DATA_) {
		if (queue_modify_value((u32) val) || do_request()) {
			ret = -1;
			goto Exit;
		}
	} else if (p->entry_type == _LOOKAT_RWPV_) {
		ret = queue_add_element((u32) val);
		if (!ret) {
			r = __return_last_eliment();
			if (!((r->cmd & __WRITE__) == __WRITE__))
				ret = do_request();
			goto Exit;
		}
	} else {
		BUG_ON(1);
	}

Exit:
	return ret;
}

static int __debug_get(void *data, u64 * val)
{
	struct sci_lookat *p = data;

	if (p->entry_type == _LOOKAT_INPUT_OUTPUT_DATA_) {
		struct lookat_request *r = __return_last_eliment();
		*val = r->value;
		DEBUGSEE("__debug_get *val = 0x%08x ", r->value);
	} else if (p->entry_type == _LOOKAT_RWPV_) {
		printk("echo addr | b00 to read Paddr's value\n");
		printk("echo addr | b01 to read Vaddr's value\n");
		printk
		    ("echo addr | b10 to write Paddr's value,Pls echo value to data file\n");
		printk
		    ("echo addr | b11 to write Vaddr's value,Pls echo value to data file\n");
	} else {
		return -1;
	}
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(lookat_fops, __debug_get, __debug_set, "0x%08llx");

static struct dentry *lookat_base;
static int __init __debug_add(struct sci_lookat *lookat)
{
	if (!debugfs_create_file(lookat->name, 0644, lookat_base,
				 lookat, &lookat_fops))
		return -ENOMEM;

	return 0;
}

int __init lookat_debug_init(void)
{
	int i;
	lookat_base = debugfs_create_dir("lookat", NULL);
	if (!lookat_base)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(__lookat_array); ++i) {
		if (__debug_add(&__lookat_array[i]))
			goto err;
	}

	return 0;
err:
	debugfs_remove(lookat_base);
	return -1;
}

module_init(lookat_debug_init);
#endif
