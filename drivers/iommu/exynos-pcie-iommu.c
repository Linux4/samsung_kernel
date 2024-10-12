/* linux/drivers/iommu/exynos_pcie_iommu.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_iommu.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/smc.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/genalloc.h>
#include <linux/kmemleak.h>
#include <linux/dma-map-ops.h>

#include <asm/cacheflush.h>
#include <asm/pgtable.h>

#include "exynos-pcie-iommu.h"
#include <soc/samsung/exynos-pcie-iommu-exp.h>

static struct kmem_cache *lv2table_kmem_cache;
#ifndef USE_DYNAMIC_MEM_ALLOC /* If it doesn't use genpool */
static struct gen_pool *lv2table_pool;
#endif
static struct sysmmu_drvdata *g_sysmmu_drvdata;
static u32 alloc_counter;
static u32 lv2table_counter;
static u32 max_req_cnt;
static u32 wrong_pf_cnt;
#if IS_ENABLED(CONFIG_PCIE_IOMMU_HISTORY_LOG)
static struct history_buff pcie_map_history, pcie_unmap_history;
#endif

int pcie_iommu_map(unsigned long iova, phys_addr_t paddr,
			size_t size, int prot, int pcie_channel);
size_t pcie_iommu_unmap(unsigned long iova, size_t size, int pcie_channel);

/* For ARM64 only */
static inline void pgtable_flush(void *vastart, void *vaend)
{
	/* __dma_flush_area(vastart, vaend - vastart); */
	dma_sync_single_for_device(g_sysmmu_drvdata->dev,
			virt_to_phys(vastart),  vaend - vastart,
			DMA_TO_DEVICE);
}

static void __sysmmu_tlb_invalidate_all(void __iomem *sfrbase,
			enum pcie_sysmmu_vid pcie_vid)
{
	writel(0x1, sfrbase + REG_MMU_FLUSH_VID(pcie_vid));
}

static void __sysmmu_set_ptbase(void __iomem *sfrbase,
			phys_addr_t pfn_pgtable, enum pcie_sysmmu_vid pcie_vid)
{
	writel_relaxed(pfn_pgtable, sfrbase + REG_PT_BASE_PPN_VID(pcie_vid));

	__sysmmu_tlb_invalidate_all(sfrbase, pcie_vid);
}

static inline void __sysmmu_set_stream(struct sysmmu_drvdata *data, int pmmu_id)
{
	struct stream_props *props = &data->props[pmmu_id];
	struct stream_config *cfg = props->cfg;
	int id_cnt = props->id_cnt;
	unsigned int i, index;

	writel_relaxed(SET_PMMU_INDICATOR(pmmu_id), data->sfrbase + REG_MMU_PMMU_INDICATOR);

	writel_relaxed(MMU_STREAM_CFG_MASK(props->default_cfg),
		       data->sfrbase + REG_MMU_STREAM_CFG(0));

	for (i = 0; i < id_cnt; i++) {
		if (cfg[i].index == UNUSED_STREAM_INDEX)
			continue;

		index = cfg[i].index;
		writel_relaxed(MMU_STREAM_CFG_MASK(cfg[i].cfg),
			       data->sfrbase + REG_MMU_STREAM_CFG(index));
		writel_relaxed(MMU_STREAM_MATCH_CFG_MASK(cfg[i].match_cfg),
			       data->sfrbase + REG_MMU_STREAM_MATCH_CFG(index));
		writel_relaxed(cfg[i].match_id_value,
			       data->sfrbase + REG_MMU_STREAM_MATCH_SID_VALUE(index));
		writel_relaxed(cfg[i].match_id_mask,
			       data->sfrbase + REG_MMU_STREAM_MATCH_SID_MASK(index));
	}
}

static void __sysmmu_init_config(struct sysmmu_drvdata *drvdata,
				 enum pcie_sysmmu_vid pcie_vid)
{
	u32 cfg;

	cfg = readl_relaxed(drvdata->sfrbase + REG_CONTEXT_CFG_ATTR_VID(pcie_vid));

	if (drvdata->qos != DEFAULT_QOS_VALUE) {
		cfg &= ~CFG_QOS(0xF);
		cfg |= CFG_QOS_OVRRIDE | CFG_QOS(drvdata->qos);
	}

	/* Use master IP's shareable attribute */
	cfg |= VID_CFG_USE_MASTER_SHA;
	writel_relaxed(cfg, drvdata->sfrbase + REG_CONTEXT_CFG_ATTR_VID(pcie_vid));
}

static void __sysmmu_enable_nocount(struct sysmmu_drvdata *drvdata,
				enum pcie_sysmmu_vid pcie_vid)
{
	u32 ctrl_val;

	__sysmmu_init_config(drvdata, pcie_vid);

	__sysmmu_set_ptbase(drvdata->sfrbase,
				drvdata->pgtable[pcie_vid - SYSMMU_PCIE_VID_OFFSET] / PAGE_SIZE, pcie_vid);

	spin_lock(&drvdata->mmu_ctrl_lock);
	if (!is_sysmmu_active(drvdata)) {
		pr_info("PCIE SysMMU Global Enable...\n");
		/* PCIe Channel0 and 1 have the same PMMU ID(0).*/
		__sysmmu_set_stream(drvdata, 0);
	}
	set_sysmmu_active(drvdata, pcie_vid);
	spin_unlock(&drvdata->mmu_ctrl_lock);

	/* Set VID0 MMU_CTRL (Stall mode is default) */
	ctrl_val = readl_relaxed(drvdata->sfrbase + REG_MMU_CTRL_VID(pcie_vid));
	writel(ctrl_val | CTRL_MMU_ENABLE | CTRL_INT_ENABLE | CTRL_FAULT_STALL_MODE,
				drvdata->sfrbase + REG_MMU_CTRL_VID(pcie_vid));
}

static void __sysmmu_disable_nocount(struct sysmmu_drvdata *drvdata,
				enum pcie_sysmmu_vid pcie_vid)
{
	u32 ctrl_val;

	ctrl_val = readl_relaxed(drvdata->sfrbase + REG_MMU_CTRL_VID(pcie_vid));
	ctrl_val &= ~CTRL_MMU_ENABLE;
	writel(ctrl_val, drvdata->sfrbase + REG_MMU_CTRL_VID(pcie_vid));
	/* Disable SysMMU for specific VID */
	writel(CTRL_DISABLE, drvdata->sfrbase + REG_MMU_CTRL_VID(pcie_vid));

	spin_lock(&drvdata->mmu_ctrl_lock);
	set_sysmmu_inactive(drvdata, pcie_vid);
	if (!is_sysmmu_active(drvdata)) {
		pr_info("PCIE SysMMU Global Disable...\n");
	}
	spin_unlock(&drvdata->mmu_ctrl_lock);

	__sysmmu_tlb_invalidate_all(drvdata->sfrbase, pcie_vid);
}

void pcie_sysmmu_set_use_iocc(int pcie_channel)
{
	if (g_sysmmu_drvdata != NULL) {
		pr_info("Set PCIe use IOCC flag.\n");
		g_sysmmu_drvdata->pcie_use_iocc[pcie_channel] = 1;
	}
}
EXPORT_SYMBOL(pcie_sysmmu_set_use_iocc);

void pcie_sysmmu_enable(int pcie_channel)
{
	enum pcie_sysmmu_vid pcie_vid = pcie_channel + SYSMMU_PCIE_VID_OFFSET;

	if (g_sysmmu_drvdata == NULL) {
		pr_err("PCIe SysMMU feature is disabled!!!\n");
		return ;
	}

	__sysmmu_enable_nocount(g_sysmmu_drvdata, pcie_vid);
}
EXPORT_SYMBOL(pcie_sysmmu_enable);

void pcie_sysmmu_disable(int pcie_channel)
{
	enum pcie_sysmmu_vid pcie_vid = pcie_channel + SYSMMU_PCIE_VID_OFFSET;

	if (g_sysmmu_drvdata == NULL) {
		pr_err("PCIe SysMMU feature is disabled!!!\n");
		return ;
	}

	__sysmmu_disable_nocount(g_sysmmu_drvdata, pcie_vid);
	pr_info("SysMMU alloc num : %d(Max:%d), lv2_alloc : %d, fault : %d\n",
				alloc_counter, max_req_cnt,
				lv2table_counter, wrong_pf_cnt);
}
EXPORT_SYMBOL(pcie_sysmmu_disable);

void pcie_sysmmu_all_buff_free(int pcie_channel)
{
	struct exynos_iommu_domain *domain;
	void __maybe_unused *virt_addr;
	int i;

	if (g_sysmmu_drvdata == NULL) {
		pr_err("PCIe SysMMU feature is disabled!!!\n");
		return ;
	}

	domain = g_sysmmu_drvdata->domain[pcie_channel];

	for (i = 0; i < NUM_LV1ENTRIES; i++) {
		if (lv1ent_page(domain->pgtable + i)) {
#ifdef USE_DYNAMIC_MEM_ALLOC
			virt_addr = phys_to_virt(
					lv2table_base(domain->pgtable + i));
			kmem_cache_free(lv2table_kmem_cache, virt_addr);
#else /* Use genpool */
			gen_pool_free(lv2table_pool,
					lv1ent_page(domain->pgtable + i),
					LV2TABLE_AND_REFBUF_SZ);
#endif
		}
	}

}
EXPORT_SYMBOL(pcie_sysmmu_all_buff_free);

static inline u32 get_hw_version(struct sysmmu_drvdata *drvdata, void __iomem *sfrbase)
{
	return MMU_RAW_VER(readl_relaxed(sfrbase + REG_MMU_VERSION));
}

static inline u32 get_num_vm(struct sysmmu_drvdata *drvdata, void __iomem *sfrbase)
{
	return MMU_NUM_CONTEXT(readl_relaxed(sfrbase + REG_MMU_NUM_CONTEXT));
}

static inline u32 get_va_width(struct sysmmu_drvdata *drvdata, void __iomem *sfrbase)
{
	writel_relaxed(SET_PMMU_INDICATOR(0), sfrbase + REG_MMU_PMMU_INDICATOR);

	if (MMU_PMMU_INFO_VA_WIDTH(readl_relaxed(sfrbase + REG_MMU_PMMU_INFO)))
		return VA_WIDTH_36BIT;
	else
		return VA_WIDTH_32BIT;
}

static int sysmmu_parse_stream_property(struct device *dev, struct sysmmu_drvdata *drvdata,
					int pmmu_index)
{
	const char *default_props_name = pmmu_default_stream[pmmu_index];
	const char *props_name = pmmu_stream_property[pmmu_index];
	struct stream_props *props = &drvdata->props[pmmu_index];
	struct stream_config *cfg;
	int i, readsize, cnt, ret;

	if (of_property_read_u32(dev->of_node, default_props_name, &props->default_cfg))
		props->default_cfg = DEFAULT_STREAM_NONE;

	cnt = of_property_count_elems_of_size(dev->of_node, props_name, sizeof(*cfg));
	if (cnt <= 0)
		return 0;

	pr_info("Default Stream cfg : 0x%x - %d stream properties\n",
		props->default_cfg, cnt);

	cfg = devm_kcalloc(dev, cnt, sizeof(*cfg), GFP_KERNEL);
	if (!cfg)
		return -ENOMEM;

	readsize = cnt * sizeof(*cfg) / sizeof(u32);
	ret = of_property_read_variable_u32_array(dev->of_node, props_name, (u32 *)cfg,
						  readsize, readsize);
	if (ret < 0) {
		dev_err(dev, "failed to get stream property, return %d\n", ret);
		return ret;
	}

	for (i = 0; i < cnt; i++) {
		if (cfg[i].index >= PCIE_SYSMMU_VID_MAX) {
			dev_err(dev, "invalid index %d is ignored. (max:%d)\n",
				cfg[i].index, PCIE_SYSMMU_VID_MAX);
			cfg[i].index = UNUSED_STREAM_INDEX;
		}
	}

	props->id_cnt = cnt;
	props->cfg = cfg;

	return 0;
}


static char *sysmmu_fault_name[SYSMMU_FAULTS_NUM] = {
	"PTW ACCESS FAULT",
	"PAGE FAULT",
	"ACCESS FAULT",
	"CONTEXT FAULT",
	"UNKNOWN FAULT"
};

static inline void sysmmu_ptlb_compare(phys_addr_t pgtable, u32 vpn, u32 ppn)
{
	sysmmu_pte_t *entry;
	unsigned long vaddr = MMU_VADDR_FROM_PTLB((unsigned long)vpn);
	unsigned long paddr = MMU_PADDR_FROM_PTLB((unsigned long)ppn);
	unsigned long phys = 0;

	if (!pgtable)
		return;

	entry = section_entry(phys_to_virt(pgtable), vaddr);

	if (lv1ent_section(entry)) {
		phys = section_phys(entry);
	} else if (lv1ent_page(entry)) {
		entry = page_entry(entry, vaddr);

		if (lv2ent_large(entry))
			phys = lpage_phys(entry);
		else if (lv2ent_small(entry))
			phys = spage_phys(entry);
	} else {
		pr_crit(">> Invalid address detected! entry: %#lx", (unsigned long)*entry);
		return;
	}

	if (paddr != phys) {
		pr_crit(">> PTLB mismatch detected!\n");
		pr_crit("   PTLB: %#010lx, PT entry: %#010lx\n", paddr, phys);
	}
}

static inline int __dump_ptlb_entry(struct sysmmu_drvdata *drvdata,  phys_addr_t pgtable,
					    int idx_way, int idx_set)
{
	u32 tpn = readl_relaxed(drvdata->sfrbase + REG_MMU_READ_PTLB_TPN);

	if (MMU_READ_PTLB_TPN_VALID(tpn)) {
		u32 attr, ppn;

		ppn = readl_relaxed(drvdata->sfrbase + REG_MMU_READ_PTLB_PPN);
		attr = readl_relaxed(drvdata->sfrbase + REG_MMU_READ_PTLB_ATTRIBUTE);

		if (MMU_READ_PTLB_TPN_S1_ENABLE(tpn)) {
			pr_crit("[%02d][%02d] VPN: %#010x, PPN: %#010x, ATTR: %#010x\n",
				idx_way, idx_set, tpn, ppn, attr);

			sysmmu_ptlb_compare(pgtable, tpn, ppn);
		}
		else
			pr_crit("[%02d][%02d] TPN(PPN): %#010x, PPN: %#010x, ATTR: %#010x\n",
				idx_way, idx_set, tpn, ppn, attr);

		return 1;
	}

	return 0;
}

static inline int dump_ptlb_entry(struct sysmmu_drvdata *drvdata, phys_addr_t pgtable,
				  int ptlb_id, int way, int num_set, int pmmu_id)
{
	int cnt = 0;
	int set;
	u32 val;

	for (set = 0; set < num_set; set++) {
		val = MMU_SET_READ_PTLB_ENTRY(way, set, ptlb_id, pmmu_id);
		writel_relaxed(val, drvdata->sfrbase + REG_MMU_READ_PTLB);
		cnt += __dump_ptlb_entry(drvdata, pgtable, way, set);
	}

	return cnt;
}

static inline void dump_sysmmu_ptlb_status(struct sysmmu_drvdata *drvdata, phys_addr_t pgtable,
					   int num_ptlb, int pmmu_id, int fault_ptlb)
{
	int way, t;
	unsigned int cnt;
	u32 info;

	for (t = 0; t < num_ptlb; t++) {
		int num_way, num_set;

		info = readl_relaxed(drvdata->sfrbase + REG_MMU_PMMU_PTLB_INFO(t));
		num_way = MMU_PMMU_PTLB_INFO_NUM_WAY(info);
		num_set = MMU_PMMU_PTLB_INFO_NUM_SET(info);

		pr_crit("PMMU.%d PTLB.%d has %d way, %d set. %s\n",
			pmmu_id, t, num_way, num_set,
			(t == fault_ptlb ? "(Fault occurred!)" : ""));

		pr_crit("------------- PTLB[WAY][SET][ENTRY] -------------\n");
		for (way = 0, cnt = 0; way < num_way; way++)
			cnt += dump_ptlb_entry(drvdata, pgtable, t, way, num_set, pmmu_id);
	}
	if (!cnt)
		pr_crit(">> No Valid PTLB Entries\n");
}

static inline void sysmmu_stlb_compare(phys_addr_t pgtable, int idx_sub, u32 vpn, u32 ppn)
{
	sysmmu_pte_t *entry;
	unsigned long vaddr = MMU_VADDR_FROM_STLB((unsigned long)vpn);
	unsigned long paddr = MMU_PADDR_FROM_STLB((unsigned long)ppn);
	unsigned long phys = 0;

	if (!pgtable)
		return;

	entry = section_entry(phys_to_virt(pgtable), vaddr);

	if (lv1ent_section(entry)) {
		phys = section_phys(entry);
	} else if (lv1ent_page(entry)) {
		entry = page_entry(entry, vaddr);

		if (lv2ent_large(entry))
			phys = lpage_phys(entry);
		else if (lv2ent_small(entry))
			phys = spage_phys(entry);
	} else {
		pr_crit(">> Invalid address detected! entry: %#lx",
			(unsigned long)*entry);
		return;
	}

	if (paddr != phys) {
		pr_crit(">> STLB mismatch detected!\n");
		pr_crit("   STLB: %#010lx, PT entry: %#010lx\n", paddr, phys);
	}
}

static inline int __dump_stlb_entry(struct sysmmu_drvdata *drvdata, phys_addr_t pgtable,
					    int idx_way, int idx_set, int idx_sub)
{
	u32 tpn = readl_relaxed(drvdata->sfrbase + REG_MMU_READ_STLB_TPN);

	if (MMU_READ_STLB_TPN_VALID(tpn)) {
		u32 ppn, attr;

		ppn = readl_relaxed(drvdata->sfrbase + REG_MMU_READ_STLB_PPN);
		attr = readl_relaxed(drvdata->sfrbase + REG_MMU_READ_STLB_ATTRIBUTE);

		if (MMU_READ_STLB_TPN_S1_ENABLE(tpn)) {
			tpn += idx_sub;

			pr_crit("[%02d][%02d] VPN: %#010x, PPN: %#010x, ATTR: %#010x\n",
				idx_way, idx_set, tpn, ppn, attr);

			sysmmu_stlb_compare(pgtable, idx_sub, tpn, ppn);
		}
		else
			pr_crit("[%02d][%02d] TPN(PPN): %#010x, PPN: %#010x, ATTR: %#010x\n",
				idx_way, idx_set, tpn, ppn, attr);

		return 1;
	}

	return 0;
}

#define MMU_NUM_STLB_SUBLINE		4
static unsigned int dump_stlb_entry(struct sysmmu_drvdata *drvdata, phys_addr_t pgtable,
					 int stlb_id, int way, int num_set)
{
	int cnt = 0;
	int set, line;
	u32 val;

	for (set = 0; set < num_set; set++) {
		for (line = 0; line < MMU_NUM_STLB_SUBLINE; line++) {
			val = MMU_SET_READ_STLB_ENTRY(way, set, stlb_id, line);
			writel_relaxed(val, drvdata->sfrbase + REG_MMU_READ_STLB);
			cnt += __dump_stlb_entry(drvdata, pgtable, way, set, line);
		}
	}

	return cnt;
}

static inline void dump_sysmmu_stlb_status(struct sysmmu_drvdata *drvdata, phys_addr_t pgtable,
					   int num_stlb, int fault_stlb)
{
	int way, t;
	unsigned int cnt;
	u32 info;

	for (t = 0; t < num_stlb; t++) {
		int num_way, num_set;

		info = readl_relaxed(drvdata->sfrbase + REG_MMU_STLB_INFO(t));
		num_way = MMU_STLB_INFO_NUM_WAY(info);
		num_set = MMU_STLB_INFO_NUM_SET(info);

		pr_crit("STLB.%d has %d way, %d set. %s\n", t, num_way, num_set,
			(t == fault_stlb ? "Fault occurred!" : ""));
		pr_crit("------------- STLB[WAY][SET][ENTRY] -------------\n");
		for (way = 0, cnt = 0; way < num_way; way++)
			cnt += dump_stlb_entry(drvdata, pgtable, t, way, num_set);
	}
	if (!cnt)
		pr_crit(">> No Valid STLB Entries\n");
}

static inline void sysmmu_s1l1tlb_compare(phys_addr_t pgtable, u32 vpn, u32 base_or_ppn,
					  int is_slptbase)
{
	sysmmu_pte_t *entry;
	unsigned long vaddr = MMU_VADDR_FROM_S1L1TLB((unsigned long)vpn), paddr;
	unsigned long phys = 0;

	if (!pgtable)
		return;

	entry = section_entry(phys_to_virt(pgtable), vaddr);

	if (is_slptbase == SLPT_BASE_FLAG)
		paddr = MMU_PADDR_FROM_S1L1TLB_BASE((unsigned long)base_or_ppn);
	else
		paddr = MMU_PADDR_FROM_S1L1TLB_PPN((unsigned long)base_or_ppn);

	if (is_slptbase == SLPT_BASE_FLAG) {
		phys = lv2table_base(entry);
	} else if (lv1ent_section(entry)) {
		phys = section_phys(entry);
	} else {
		pr_crit(">> Invalid address detected! entry: %#lx\n", (unsigned long)*entry);
		return;
	}

	if (paddr != phys) {
		pr_crit(">> S1L1TLB mismatch detected!\n");
		if (is_slptbase == SLPT_BASE_FLAG)
			pr_crit("entry addr: %lx, slpt base addr: %lx\n", paddr, phys);
		else
			pr_crit("S1L1TLB: %#010lx, PT entry: %#010lx\n", paddr, phys);
	}
}

static inline int dump_s1l1tlb_entry(struct sysmmu_drvdata *drvdata, phys_addr_t pgtable,
					  int way, int set)
{
	int is_slptbase;
	u32 vpn = readl_relaxed(drvdata->sfrbase + REG_MMU_READ_S1L1TLB_VPN);

	if (MMU_READ_S1L1TLB_VPN_VALID(vpn)) {
		u32 base_or_ppn, attr;

		base_or_ppn = readl_relaxed(drvdata->sfrbase + REG_MMU_READ_S1L1TLB_SLPT_OR_PPN);
		attr = readl_relaxed(drvdata->sfrbase + REG_MMU_READ_S1L1TLB_ATTRIBUTE);
		is_slptbase = MMU_S1L1TLB_ATTRIBUTE_PS(attr);

		pr_crit("[%02d][%02d] VPN: %#010x, PPN: %#010x, ATTR: %#010x\n",
			way, set, vpn, base_or_ppn, attr);
		sysmmu_s1l1tlb_compare(pgtable, vpn, base_or_ppn, is_slptbase);

		return 1;
	}

	return 0;
}

static inline void dump_sysmmu_s1l1tlb_status(struct sysmmu_drvdata *drvdata,
					      phys_addr_t pgtable)
{
	int way, set;
	unsigned int cnt;
	u32 info;
	int num_way, num_set;

	info = readl_relaxed(drvdata->sfrbase + REG_MMU_S1L1TLB_INFO);
	num_way = MMU_S1L1TLB_INFO_NUM_WAY(info);
	num_set = MMU_S1L1TLB_INFO_NUM_SET(info);

	pr_crit("S1L1TLB has %d way, %d set.\n", num_way, num_set);
	pr_crit("------------- S1L1TLB[WAY][SET][ENTRY] -------------\n");
	for (way = 0, cnt = 0; way < num_way; way++) {
		for (set = 0; set < num_set; set++) {
			writel_relaxed(MMU_SET_READ_S1L1TLB_ENTRY(way, set),
				       drvdata->sfrbase + REG_MMU_READ_S1L1TLB);
			cnt += dump_s1l1tlb_entry(drvdata, pgtable, way, set);
		}
	}
	if (!cnt)
		pr_crit(">> No Valid S1L1TLB Entries\n");
}

static inline void dump_sysmmu_tlb_status(struct sysmmu_drvdata *drvdata, phys_addr_t pgtable,
					  int pmmu_id, int stream_id)
{
	u32 swalker;
	int num_stlb, num_ptlb;
	void __iomem *sfrbase = drvdata->sfrbase;
	int fault_ptlb, fault_stlb;
	u32 stream_cfg;

	swalker = readl_relaxed(sfrbase + REG_MMU_SWALKER_INFO);
	num_stlb = MMU_SWALKER_INFO_NUM_STLB(swalker);

	writel_relaxed(MMU_SET_PMMU_INDICATOR(pmmu_id), sfrbase + REG_MMU_PMMU_INDICATOR);
	num_ptlb = drvdata->num_pmmu;

	stream_cfg = readl_relaxed(sfrbase + REG_MMU_STREAM_CFG(stream_id));
	fault_ptlb = MMU_STREAM_CFG_PTLB_ID(stream_cfg);
	fault_stlb = MMU_STREAM_CFG_STLB_ID(stream_cfg);

	pr_crit("SysMMU has %d PTLBs(PMMU %d), %d STLBs, 1 S1L1TLB\n",
		num_ptlb, pmmu_id, num_stlb);

	dump_sysmmu_ptlb_status(drvdata, pgtable, num_ptlb, pmmu_id, fault_ptlb);
	dump_sysmmu_stlb_status(drvdata, pgtable, num_stlb, fault_stlb);
	dump_sysmmu_s1l1tlb_status(drvdata, pgtable);
}

static void dump_sysmmu_tlb_port(struct sysmmu_drvdata *drvdata, phys_addr_t pgtable,
				int pmmu_id, int stream_id)
{
	u32 swalker;
	int num_stlb, num_ptlb;
	void __iomem *sfrbase = drvdata->sfrbase;
	int fault_ptlb, fault_stlb;
	u32 stream_cfg;

	swalker = readl_relaxed(sfrbase + REG_MMU_SWALKER_INFO);
	num_stlb = MMU_SWALKER_INFO_NUM_STLB(swalker);

	writel_relaxed(MMU_SET_PMMU_INDICATOR(pmmu_id), sfrbase + REG_MMU_PMMU_INDICATOR);
	num_ptlb = drvdata->num_pmmu;

	stream_cfg = readl_relaxed(sfrbase + REG_MMU_STREAM_CFG(stream_id));
	fault_ptlb = MMU_STREAM_CFG_PTLB_ID(stream_cfg);
	fault_stlb = MMU_STREAM_CFG_STLB_ID(stream_cfg);

	pr_crit("SysMMU has %d PTLBs(PMMU %d), %d STLBs, 1 S1L1TLB\n",
		num_ptlb, pmmu_id, num_stlb);

	dump_sysmmu_ptlb_status(drvdata, pgtable, num_ptlb, pmmu_id, fault_ptlb);
	dump_sysmmu_stlb_status(drvdata, pgtable, num_stlb, fault_stlb);
	dump_sysmmu_s1l1tlb_status(drvdata, pgtable);
}

void print_pcie_sysmmu_tlb(enum pcie_sysmmu_vid pcie_vid)
{
	phys_addr_t pgtable;

	pgtable = __raw_readl(g_sysmmu_drvdata->sfrbase +
				REG_PT_BASE_PPN_VID(pcie_vid));
	pgtable <<= PAGE_SHIFT;
	pr_crit("Page Table Base Address : 0x%llx\n", pgtable);

	/* dump_sysmmu_tlb_port(g_sysmmu_drvdata, 0 1); */
}
EXPORT_SYMBOL(print_pcie_sysmmu_tlb);

static int show_fault_information(struct sysmmu_drvdata *drvdata, int flags,
		unsigned long fault_addr, enum pcie_sysmmu_vid pcie_vid)
{
	unsigned int info;
	phys_addr_t pgtable;
	int fault_id = SYSMMU_FAULT_ID(flags);
	const char *port_name = NULL;
	int ret = 0;
	u32 info0, info1, info2;
	int pmmu_id, stream_id;

	pgtable = __raw_readl(drvdata->sfrbase + REG_PT_BASE_PPN_VID(pcie_vid));
	pgtable <<= PAGE_SHIFT;

	info = __raw_readl(drvdata->sfrbase + REG_FAULT_INFO0_VID(pcie_vid));

	of_property_read_string(drvdata->dev->of_node,
					"port-name", &port_name);

	pr_crit("----------------------------------------------------------\n");
	pr_crit("From [%s], SysMMU %s %s at %#010lx (page table @ %pa)\n",
		port_name ? port_name : dev_name(drvdata->dev),
		(flags & IOMMU_FAULT_WRITE) ? "WRITE" : "READ",
		sysmmu_fault_name[fault_id], fault_addr, &pgtable);

	info0 = readl_relaxed(drvdata->sfrbase + REG_FAULT_INFO0_VID(pcie_vid));
	info1 = readl_relaxed(drvdata->sfrbase + REG_FAULT_INFO1_VID(pcie_vid));
	info2 = readl_relaxed(drvdata->sfrbase + REG_FAULT_INFO2_VID(pcie_vid));
	pmmu_id = MMU_FAULT_INFO2_PMMU_ID(info2);
	stream_id = MMU_FAULT_INFO2_STREAM_ID(info2);

	pr_crit("ASID: %d, Burst LEN: %d, AXI ID: %d, PMMU ID: %d, STREAM ID: %d\n",
		MMU_FAULT_INFO0_ASID(info0), MMU_FAULT_INFO0_LEN(info0),
		MMU_FAULT_INFO1_AXID(info1), pmmu_id, stream_id);


	if (fault_id == SYSMMU_FAULT_UNKNOWN) {
		pr_crit("The fault is not caused by this System MMU.\n");
		pr_crit("Please check IRQ and SFR base address.\n");
		goto finish;
	}

	pr_crit("AxID: %#x, AxLEN: %#x\n", info & 0xFFFF, (info >> 16) & 0xF);

	if (pgtable != drvdata->pgtable[pcie_vid - SYSMMU_PCIE_VID_OFFSET])
		pr_crit("Page table base of driver: %pa\n",
			&drvdata->pgtable);

	if (fault_id == SYSMMU_FAULT_PTW_ACCESS)
		pr_crit("System MMU has failed to access page table\n");

	if (!pfn_valid(pgtable >> PAGE_SHIFT)) {
		pr_crit("Page table base is not in a valid memory region\n");
	} else {
		sysmmu_pte_t *ent;

		ent = section_entry(phys_to_virt(pgtable), fault_addr);
		pr_crit("Lv1 entry: %#010x\n", *ent);

		if (lv1ent_page(ent)) {
			u64 sft_ent_addr, sft_fault_addr;

			ent = page_entry(ent, fault_addr);
			pr_crit("Lv2 entry: %#010x\n", *ent);

			sft_ent_addr = (*ent) >> 8;
			sft_fault_addr = fault_addr >> 12;

			if (sft_ent_addr == sft_fault_addr) {
				pr_crit("ent(%#llx) == faddr(%#llx)...\n",
						sft_ent_addr, sft_fault_addr);
				pr_crit("Try to IGNORE Page fault panic...\n");
				ret = SYSMMU_NO_PANIC;
				wrong_pf_cnt++;
			}
		} else if (lv1ent_section(ent)) {
			u64 sft_ent_addr, sft_fault_addr;

			sft_ent_addr = (*ent) >>  16;
			sft_fault_addr = fault_addr >> 20;
			if (sft_ent_addr == sft_fault_addr) {
				pr_crit("SEC : ent(%#llx) == faddr(%#llx)...\n",
						sft_ent_addr, sft_fault_addr);
				pr_crit("Try to IGNORE Page fault panic...\n");
				ret = SYSMMU_NO_PANIC;
				wrong_pf_cnt++;
			}
		}
	}

	dump_sysmmu_tlb_port(drvdata, pgtable, pmmu_id, stream_id);
finish:
	pr_crit("----------------------------------------------------------\n");

	return ret;
}

int pcie_sysmmu_add_fault_handler(struct notifier_block *pcie_sysmmu_nb)
{
	struct sysmmu_drvdata *drvdata = g_sysmmu_drvdata;

	dev_info(drvdata->dev, "Add SysMMU Page Fault handler.");
	atomic_notifier_chain_register(&drvdata->fault_notifiers, pcie_sysmmu_nb);

	return 0;
}
EXPORT_SYMBOL_GPL(pcie_sysmmu_add_fault_handler);

void (*pci_fault_handler)(int id);

int pcie_sysmmu_add_pcie_fault_handler(void (*func)(int))
{
	pci_fault_handler = func;

	return 0;
}
EXPORT_SYMBOL_GPL(pcie_sysmmu_add_pcie_fault_handler);

static char *sysmmu_fault_vid[SYSMMU_MAX_VID] = {
	"PCIE_SYSMMU_NOT_USED",
	"PCIE_SYSMMU_VID_CH0",
	"PCIE_SYSMMU_VID_CH1",
};

static irqreturn_t exynos_sysmmu_irq(int irq, void *dev_id)
{
	struct sysmmu_drvdata *drvdata = dev_id;
	unsigned long addr = -1;
	int flags = 0;
	u32 info;
	u32 int_status, ctrl_val;
	int ret, i, fault_id;
	enum pcie_sysmmu_vid pcie_vid = PCIE_SYSMMU_NOT_USED;

	dev_info(drvdata->dev, "%s:%d: irq(%d) happened\n",
					__func__, __LINE__, irq);

	WARN(!is_sysmmu_active(drvdata),
		"Fault occurred while System MMU %s is not enabled!\n",
		dev_name(drvdata->dev));

	for (i = 0; i < drvdata->num_pmmu; i++) {
		int_status = readl_relaxed(drvdata->sfrbase + REG_FAULT_STATUS_VID(i));

		if (int_status & 0xF) {
			pcie_vid = i;
			break;
		}
	}

	pr_crit("PCIe SysMMU Fault - %s(0x%x)\n", sysmmu_fault_vid[pcie_vid],
						int_status);
	addr = readl_relaxed(drvdata->sfrbase + REG_FAULT_VA_VID(pcie_vid));
	info = readl_relaxed(drvdata->sfrbase + REG_FAULT_INFO0_VID(pcie_vid));

	if (MMU_FAULT_INFO0_VA_36(info))
		addr += MMU_FAULT_INFO0_VA_HIGH(info);

	flags = MMU_IS_READ_FAULT(info) ?
		IOMMU_FAULT_READ : IOMMU_FAULT_WRITE;

	/* Clear interrupts */
	int_status = readl_relaxed(drvdata->sfrbase + REG_FAULT_STATUS_VID(pcie_vid));
	writel(int_status, drvdata->sfrbase + REG_FAULT_CLEAR_VID(pcie_vid));

	flags |= SYSMMU_FAULT_FLAG(int_status);

	ret = show_fault_information(drvdata, flags, addr, pcie_vid);
	if (ret == SYSMMU_NO_PANIC)
		return IRQ_HANDLED;

	if (pci_fault_handler != NULL)
		pci_fault_handler(pcie_vid - SYSMMU_PCIE_VID_OFFSET);

	ret = atomic_notifier_call_chain(&drvdata->fault_notifiers, addr, &flags);
	if (ret == SYSMMU_PANIC_BY_DEV) {
		pr_crit("Kernel panic by SysMMU is skipped and disable interrupt.\n");
		ctrl_val = readl_relaxed(drvdata->sfrbase + REG_MMU_CTRL_VID(pcie_vid));
		ctrl_val &= ~(CTRL_INT_ENABLE);
		writel_relaxed(ctrl_val, drvdata->sfrbase + REG_MMU_CTRL_VID(pcie_vid));

		disable_irq_nosync(drvdata->irq_num);

		return IRQ_HANDLED;
	}

	fault_id = SYSMMU_FAULT_ID(flags);
	panic("(%s) From [PCIe CH%d], SysMMU %s %s at %#010lx\n", dev_name(drvdata->dev),
		pcie_vid - SYSMMU_PCIE_VID_OFFSET, /* PCIe channel number */
		(flags & IOMMU_FAULT_WRITE) ? "WRITE" : "READ",
		sysmmu_fault_name[fault_id], addr);

	return IRQ_HANDLED;
}

static void __sysmmu_tlb_invalidate(struct sysmmu_drvdata *drvdata, dma_addr_t iova,
				size_t size, enum pcie_sysmmu_vid pcie_vid)
{
	void * __iomem sfrbase = drvdata->sfrbase;
	u32 start_addr, end_addr;
	start_addr = (iova >> 4) & 0xffffff00;
	writel_relaxed(start_addr, sfrbase + REG_FLUSH_RANGE_START_VID(pcie_vid));

	end_addr = (((iova + size - 1) >> 4) & 0xffffff00) | TLB_INVALIDATE;
	writel_relaxed(end_addr, sfrbase + REG_FLUSH_RANGE_END_VID(pcie_vid));
}

static void exynos_sysmmu_tlb_invalidate(dma_addr_t d_start, size_t size,
				enum pcie_sysmmu_vid pcie_vid)
{
	struct sysmmu_drvdata *drvdata = g_sysmmu_drvdata;
	sysmmu_iova_t start = (sysmmu_iova_t)d_start;

	spin_lock(&drvdata->lock);
	if (!is_sysmmu_active(drvdata)) {
		spin_unlock(&drvdata->lock);
		dev_dbg(drvdata->dev,
			"Skip TLB invalidation %#zx@%#llx\n", size, start);
		return ;
	}

	dev_dbg(drvdata->dev,
		"TLB invalidation %#zx@%#llx\n", size, start);

	__sysmmu_tlb_invalidate(drvdata, start, size, pcie_vid);

	spin_unlock(&drvdata->lock);
}

static void clear_lv2_page_table(sysmmu_pte_t *ent, int n)
{
	if (n > 0)
		memset(ent, 0, sizeof(*ent) * n);
}

static int lv1set_section(struct exynos_iommu_domain *domain,
			  sysmmu_pte_t *sent, sysmmu_iova_t iova,
			  phys_addr_t paddr, int prot, atomic_t *pgcnt)
{
	bool shareable = !!(prot & IOMMU_CACHE);

	if (lv1ent_section(sent)) {
		WARN(1, "Trying mapping on 1MiB@%#09llx that is mapped",
			iova);
		return -EADDRINUSE;
	}

	if (lv1ent_page(sent)) {
		if (WARN_ON(atomic_read(pgcnt) != NUM_LV2ENTRIES)) {
			WARN(1, "Trying mapping on 1MiB@%#09llx that is mapped",
				iova);
			return -EADDRINUSE;
		}
		/* TODO: for v7, free lv2 page table */
	}

	*sent = mk_lv1ent_sect(paddr);
	if (shareable)
		set_lv1ent_shareable(sent);
	pgtable_flush(sent, sent + 1);

	return 0;
}

static int lv2set_page(sysmmu_pte_t *pent, phys_addr_t paddr, size_t size,
						int prot, atomic_t *pgcnt)
{
	bool shareable = !!(prot & IOMMU_CACHE);

	if (size == SPAGE_SIZE) {
		if (!lv2ent_fault(pent)) {
			sysmmu_pte_t *refcnt_buf;

			/* Duplicated IOMMU map 4KB */
			refcnt_buf = pent + NUM_LV2ENTRIES;
			*refcnt_buf = *refcnt_buf + 1;
			return 0;
		}

		*pent = mk_lv2ent_spage(paddr);
		if (shareable)
			set_lv2ent_shareable(pent);
		pgtable_flush(pent, pent + 1);
		atomic_dec(pgcnt);
	} else { /* size == LPAGE_SIZE */
		int i;

		//pr_err("Allocate LPAGE_SIZE!!!\n");
		for (i = 0; i < SPAGES_PER_LPAGE; i++, pent++) {
			if (WARN_ON(!lv2ent_fault(pent))) {
				clear_lv2_page_table(pent - i, i);
				return -EADDRINUSE;
			}

			*pent = mk_lv2ent_lpage(paddr);
			if (shareable)
				set_lv2ent_shareable(pent);
		}
		pgtable_flush(pent - SPAGES_PER_LPAGE, pent);
		/* Need to add refcnt process */
		atomic_sub(SPAGES_PER_LPAGE, pgcnt);
	}

	return 0;
}

#ifdef USE_DYNAMIC_MEM_ALLOC
static sysmmu_pte_t *alloc_extend_buff(struct exynos_iommu_domain *domain)
{
	sysmmu_pte_t *alloc_buff = NULL;
	int i;

	/* Find Empty buffer */
	for (i = 0; i < MAX_EXT_BUFF_NUM; i++) {
		if (domain->ext_buff[i].used == 0) {
			pr_err("Use extend buffer index : %d...\n", i);
			break;
		}
	}

	if (i == MAX_EXT_BUFF_NUM) {
		pr_err("WARNNING - Extend buffers are full!!!\n");
		return NULL;
	}

	domain->ext_buff[i].used = 1;
	alloc_buff = domain->ext_buff[i].buff;

	return alloc_buff;
}
#endif

static sysmmu_pte_t *alloc_lv2entry(struct exynos_iommu_domain *domain,
				sysmmu_pte_t *sent, sysmmu_iova_t iova,
				atomic_t *pgcounter, gfp_t gfpmask)
{
	sysmmu_pte_t *pent = NULL;

	if (lv1ent_section(sent)) {
		WARN(1, "Trying mapping on %#09llx mapped with 1MiB page",
						iova);
		return ERR_PTR(-EADDRINUSE);
	}

	if (lv1ent_fault(sent)) {
		lv2table_counter++;

#ifdef USE_DYNAMIC_MEM_ALLOC
		pent = kmem_cache_zalloc(lv2table_kmem_cache,
				gfpmask);
		if (!pent) {
			/* Use extended Buffer */
			pent = alloc_extend_buff(domain);

			if (!pent)
				return ERR_PTR(-ENOMEM);
		}
#else /* Use genpool */
		if (gen_pool_avail(lv2table_pool) >= LV2TABLE_AND_REFBUF_SZ) {
			pent = phys_to_virt(gen_pool_alloc(lv2table_pool,
						LV2TABLE_AND_REFBUF_SZ));
		} else {
			pr_err("Gen_pool is full!! Try dynamic alloc\n");
			pent = kmem_cache_zalloc(lv2table_kmem_cache, gfpmask);
			if (!pent)
				return ERR_PTR(-ENOMEM);
		}
#endif

		*sent = mk_lv1ent_page(virt_to_phys(pent));
		pgtable_flush(sent, sent + 1);
		pgtable_flush(pent, pent + NUM_LV2ENTRIES);
		atomic_set(pgcounter, NUM_LV2ENTRIES);
		kmemleak_ignore(pent);
	}

	return page_entry(sent, iova);
}

static size_t iommu_pgsize(unsigned long addr_merge, size_t size, int domain_num)
{
	struct exynos_iommu_domain *domain = g_sysmmu_drvdata->domain[domain_num];
	unsigned int pgsize_idx;
	size_t pgsize;

	/* Max page size that still fits into 'size' */
	pgsize_idx = __fls(size);

	/* need to consider alignment requirements ? */
	if (likely(addr_merge)) {
		/* Max page size allowed by address */
		unsigned int align_pgsize_idx = __ffs(addr_merge);
		pgsize_idx = min(pgsize_idx, align_pgsize_idx);
	}

	/* build a mask of acceptable page sizes */
	pgsize = (1UL << (pgsize_idx + 1)) - 1;

	/* throw away page sizes not supported by the hardware */
	pgsize &= domain->pgsize_bitmap;

	/* make sure we're still sane */
	BUG_ON(!pgsize);

	/* pick the biggest page */
	pgsize_idx = __fls(pgsize);
	pgsize = 1UL << pgsize_idx;

	return pgsize;
}

static int exynos_iommu_map(unsigned long l_iova, phys_addr_t paddr,
		size_t size, int prot, int domain_num)
{
	struct exynos_iommu_domain *domain = g_sysmmu_drvdata->domain[domain_num];
	sysmmu_pte_t *entry;
	sysmmu_iova_t iova = (sysmmu_iova_t)l_iova;
	int ret = -ENOMEM;

	BUG_ON(domain->pgtable == NULL);

	entry = section_entry(domain->pgtable, iova);

	if (size == SECT_SIZE) {
		ret = lv1set_section(domain, entry, iova, paddr, prot,
				     &domain->lv2entcnt[lv1ent_offset(iova)]);
	} else {
		sysmmu_pte_t *pent;
		pent = alloc_lv2entry(domain, entry, iova,
			      &domain->lv2entcnt[lv1ent_offset(iova)],
			      GFP_ATOMIC);

		if (IS_ERR(pent))
			ret = PTR_ERR(pent);
		else
			ret = lv2set_page(pent, paddr, size, prot,
				       &domain->lv2entcnt[lv1ent_offset(iova)]);
	}

	if (ret)
		pr_err("%s: Failed(%d) to map %#zx bytes @ %#llx\n",
			__func__, ret, size, iova);

	return ret;
}

static size_t exynos_iommu_unmap(unsigned long l_iova, size_t size, int domain_num)
{
	struct exynos_iommu_domain *domain = g_sysmmu_drvdata->domain[domain_num];
	sysmmu_iova_t iova = (sysmmu_iova_t)l_iova;
	sysmmu_pte_t *sent, *pent;
	size_t err_pgsize;
	atomic_t *lv2entcnt = &domain->lv2entcnt[lv1ent_offset(iova)];

	BUG_ON(domain->pgtable == NULL);

	sent = section_entry(domain->pgtable, iova);

	if (lv1ent_section(sent)) {
		if (WARN_ON(size < SECT_SIZE)) {
			err_pgsize = SECT_SIZE;
			goto err;
		}

		*sent = 0;
		pgtable_flush(sent, sent + 1);
		size = SECT_SIZE;
		goto done;
	}

	if (unlikely(lv1ent_fault(sent))) {
		if (size > SECT_SIZE)
			size = SECT_SIZE;
		goto done;
	}

	/* lv1ent_page(sent) == true here */

	pent = page_entry(sent, iova);

	if (unlikely(lv2ent_fault(pent))) {
		size = SPAGE_SIZE;
		goto done;
	}

	if (lv2ent_small(pent)) {
		/* Check Duplicated IOMMU Unmp */
		sysmmu_pte_t *refcnt_buf;

		refcnt_buf = (pent + NUM_LV2ENTRIES);
		if (*refcnt_buf != 0) {
			*refcnt_buf = *refcnt_buf - 1;
			goto done;
		}

		*pent = 0;
		size = SPAGE_SIZE;
		pgtable_flush(pent, pent + 1);
		atomic_inc(lv2entcnt);
		goto unmap_flpd;
	}

	/* lv1ent_large(pent) == true here */
	if (WARN_ON(size < LPAGE_SIZE)) {
		err_pgsize = LPAGE_SIZE;
		goto err;
	}

	clear_lv2_page_table(pent, SPAGES_PER_LPAGE);
	pgtable_flush(pent, pent + SPAGES_PER_LPAGE);
	size = LPAGE_SIZE;
	atomic_add(SPAGES_PER_LPAGE, lv2entcnt);

unmap_flpd:
	/* TODO: for v7, remove all */
	if (atomic_read(lv2entcnt) == NUM_LV2ENTRIES) {
		sysmmu_pte_t* free_buff;
		phys_addr_t __maybe_unused paddr;

		free_buff = page_entry(sent, 0);
		paddr = virt_to_phys(free_buff);
		lv2table_counter--;

		*sent = 0;
		pgtable_flush(sent, sent + 1);
		atomic_set(lv2entcnt, 0);

#ifdef USE_DYNAMIC_MEM_ALLOC
		if (free_buff >= domain->ext_buff[0].buff && free_buff
				<= domain->ext_buff[MAX_EXT_BUFF_NUM - 1].buff) {
			/* Allocated from extend buffer */
			u64 index =
				(u64)free_buff - (u64)domain->ext_buff[0].buff;
			index /= SZ_2K;
			if (index < MAX_EXT_BUFF_NUM) {
				pr_err("Extend buffer %llu free...\n", index);
				domain->ext_buff[index].used = 0;
			} else
				pr_err("WARN - Lv2table free ERR!!!\n");
		} else {
			kmem_cache_free(lv2table_kmem_cache,
					free_buff);
		}
#else
		if (gen_pool_has_addr(lv2table_pool, paddr, LV2TABLE_AND_REFBUF_SZ))
			gen_pool_free(lv2table_pool, paddr, LV2TABLE_AND_REFBUF_SZ);
		else
			kmem_cache_free(lv2table_kmem_cache, free_buff);
#endif
	}

done:

	return size;
err:
	pr_err("%s: Failed: size(%#zx)@%#llx is smaller than page size %#zx\n",
		__func__, size, iova, err_pgsize);

	return 0;
}

#if IS_ENABLED(CONFIG_PCIE_IOMMU_MAP_ONCE)
static int exynos_iommu_map_once(unsigned long l_iova, phys_addr_t paddr,
		size_t size, int prot, int domain_num)
{
	struct exynos_iommu_domain *domain = g_sysmmu_drvdata->domain[domain_num];
	sysmmu_pte_t *entry, *pent;
	sysmmu_iova_t iova = (sysmmu_iova_t)l_iova;
	int i, cnt, ret = 0;
	bool shareable = !!(prot & IOMMU_CACHE);
	atomic_t *pgcnt = &domain->lv2entcnt[lv1ent_offset(iova)];

	WARN_ON(domain->pgtable == NULL);

	/* Check it is over section size at one request. */
	if ((l_iova & 0xfffff) + size > SZ_1M) {
		pr_err("Don't allow : address + size is over section size (0x%llx + 0x%x)\n",
			iova, size);
		return -EINVAL;
	}

	entry = section_entry(domain->pgtable, iova);

	pent = alloc_lv2entry(domain, entry, iova,
			&domain->lv2entcnt[lv1ent_offset(iova)],
			GFP_ATOMIC);
	if (IS_ERR(pent)) {
		ret = PTR_ERR(pent);
		pr_err("Can't alloc LV2 table!\n");
		return ret;
	}

	cnt = size / SZ_4K;
	for (i = 0; i < cnt; i++, pent++) {
		if (!lv2ent_fault(pent)) {
			sysmmu_pte_t *refcnt_buf;

			pr_debug("Duplicated Memory Allocation : PTE will be overwritten!\n");
			/* Duplicated IOMMU map 4KB */
			refcnt_buf = pent + NUM_LV2ENTRIES;
			*refcnt_buf = *refcnt_buf + 1;
		}

		*pent = mk_lv2ent_spage(paddr);
		if (shareable)
			set_lv2ent_shareable(pent);
		paddr += SZ_4K;
	}
	pgtable_flush(pent - cnt, pent);
	atomic_sub(cnt, pgcnt);


	if (ret)
		pr_err("%s: Failed(%d) to map %#zx bytes @ %#llx\n",
			__func__, ret, size, iova);

	return ret;
}

static size_t exynos_iommu_unmap_once(unsigned long l_iova, size_t size, int domain_num)
{
	struct exynos_iommu_domain *domain = g_sysmmu_drvdata->domain[domain_num];
	sysmmu_iova_t iova = (sysmmu_iova_t)l_iova;
	sysmmu_pte_t *sent, *pent;
	atomic_t *lv2entcnt = &domain->lv2entcnt[lv1ent_offset(iova)];
	int cnt, i;

	WARN_ON(domain->pgtable == NULL);

	/* Check it is over section size at one request. */
	if ((l_iova & 0xfffff) + size > SZ_1M) {
		pr_err("Don't allow : address + size over is section size (0x%llx + 0x%x)\n",
			iova, size);
		return 0;
	}

	sent = section_entry(domain->pgtable, iova);

	if (unlikely(lv1ent_fault(sent))) {
		pr_info("%s : LV1 entry fault!\n", __func__);
		return -EINVAL;
	}

	pent = page_entry(sent, iova);
	cnt = size / SZ_4K;

	for (i = 0; i < cnt; i++, pent++) {
		/* Check Duplicated IOMMU Unmp */
		sysmmu_pte_t *refcnt_buf;

		refcnt_buf = (pent + NUM_LV2ENTRIES);
		if (*refcnt_buf != 0) {
			pr_debug("VA : 0x%llx's ref conunt is not 0! - SKIP unmap\n");
			*refcnt_buf = *refcnt_buf - 1;
			continue;
		}

		*pent = 0;
	}
	pgtable_flush(pent - cnt, pent);
	atomic_add(cnt, lv2entcnt);

	/* Ignore Free LV2 table at here. */

	return size;
}
#endif

#if IS_ENABLED(CONFIG_PCIE_IOMMU_HISTORY_LOG)
static inline void add_history_buff(struct history_buff *hbuff,
			phys_addr_t addr, phys_addr_t orig_addr,
			size_t size, size_t orig_size)
{
	hbuff->save_addr[hbuff->index] = (u32)((addr >> 0x4) & 0xffffffff);
	hbuff->orig_addr[hbuff->index] = (u32)((orig_addr >> 0x4) & 0xffffffff);
	hbuff->size[hbuff->index] = size;
	hbuff->orig_size[hbuff->index] = orig_size;
	hbuff->index++;
	if (hbuff->index >= MAX_HISTROY_BUFF)
		hbuff->index = 0;
}
#endif

static inline int check_memory_validation(phys_addr_t paddr)
{
	int ret;

	ret = pfn_valid(paddr >> PAGE_SHIFT);
	if (!ret) {
		pr_err("Requested address %llx is NOT in DRAM rergion!!\n", paddr);
		return -EINVAL;
	}

	return 0;
}

int pcie_iommu_map(unsigned long iova, phys_addr_t paddr, size_t size,
		int prot, int pcie_channel)
{
	struct exynos_iommu_domain *domain = g_sysmmu_drvdata->domain[pcie_channel];
	unsigned long orig_iova = iova;
	unsigned int min_pagesz;
	size_t orig_size = size;
	phys_addr_t __maybe_unused orig_paddr = paddr;
	int ret = 0;
	unsigned long changed_iova, changed_size;
	unsigned long flags;
	enum pcie_sysmmu_vid pcie_vid = pcie_channel + SYSMMU_PCIE_VID_OFFSET;

	/* Make sure start address align least 4KB */
	if ((iova & SYSMMU_4KB_MASK) != 0) {
		size += iova & SYSMMU_4KB_MASK;
		iova &= ~(SYSMMU_4KB_MASK);
		paddr &= ~(SYSMMU_4KB_MASK);
	}
	/* Make sure size align least 4KB */
	if ((size & SYSMMU_4KB_MASK) != 0) {
		size &= ~(SYSMMU_4KB_MASK);
		size += SZ_4K;
	}

	/* Check for debugging */
	changed_iova = iova;
	changed_size = size;

	/* find out the minimum page size supported */
	min_pagesz = 1 << __ffs(domain->pgsize_bitmap);

	/*
	 * both the virtual address and the physical one, as well as
	 * the size of the mapping, must be aligned (at least) to the
	 * size of the smallest page supported by the hardware
	 */
	if (!IS_ALIGNED(iova | paddr | size, min_pagesz)) {
		pr_err("unaligned: iova 0x%lx pa %p sz 0x%zx min_pagesz 0x%x\n",
		       iova, &paddr, size, min_pagesz);
		return -EINVAL;
	}

	spin_lock_irqsave(&domain->pgtablelock, flags);
#if IS_ENABLED(CONFIG_PCIE_IOMMU_MAP_ONCE)
	if (size < SZ_64K) { /* This code assume that there are no LARGE Pages(64KB) */
		ret = exynos_iommu_map_once(iova, paddr, size, prot, pcie_channel);
		if (ret == 0)
			goto end_map;
	}
#endif

	while (size) {
		size_t pgsize = iommu_pgsize(iova | paddr, size, pcie_channel);

		pr_debug("mapping: iova 0x%lx pa %pa pgsize 0x%zx\n",
			 iova, &paddr, pgsize);

		alloc_counter++;
		if (alloc_counter > max_req_cnt)
			max_req_cnt = alloc_counter;
		ret = exynos_iommu_map(iova, paddr, pgsize, prot, pcie_channel);
		/* exynos_sysmmu_tlb_invalidate(iova, pgsize, pcie_vid); */
#if IS_ENABLED(CONFIG_PCIE_IOMMU_HISTORY_LOG)
		add_history_buff(&pcie_map_history, paddr, orig_paddr,
				changed_size, orig_size);
#endif
		if (ret)
			break;

		iova += pgsize;
		paddr += pgsize;
		size -= pgsize;
	}
#if IS_ENABLED(CONFIG_PCIE_IOMMU_MAP_ONCE)
end_map:
#endif
	exynos_sysmmu_tlb_invalidate(orig_iova, orig_size, pcie_vid);
	spin_unlock_irqrestore(&domain->pgtablelock, flags);

	/* unroll mapping in case something went wrong */
	if (ret) {
		pr_err("PCIe SysMMU mapping Error!\n");
		pcie_iommu_unmap(orig_iova, orig_size - size, pcie_channel);
	}

	pr_debug("mapped: req 0x%lx(org : 0x%lx)  size 0x%zx(0x%zx)\n",
			changed_iova, orig_iova, changed_size, orig_size);

	return ret;
}
EXPORT_SYMBOL_GPL(pcie_iommu_map);

size_t pcie_iommu_unmap(unsigned long iova, size_t size, int pcie_channel)
{
	struct exynos_iommu_domain *domain = g_sysmmu_drvdata->domain[pcie_channel];
	size_t unmapped_page, unmapped = 0;
	unsigned int min_pagesz;
	unsigned long __maybe_unused orig_iova = iova;
	unsigned long __maybe_unused changed_iova;
	size_t __maybe_unused orig_size = size;
	unsigned long flags;
	enum pcie_sysmmu_vid pcie_vid = pcie_channel + SYSMMU_PCIE_VID_OFFSET;

	/* Make sure start address align least 4KB */
	if ((iova & SYSMMU_4KB_MASK) != 0) {
		size += iova & SYSMMU_4KB_MASK;
		iova &= ~(SYSMMU_4KB_MASK);
	}
	/* Make sure size align least 4KB */
	if ((size & SYSMMU_4KB_MASK) != 0) {
		size &= ~(SYSMMU_4KB_MASK);
		size += SZ_4K;
	}

	changed_iova = iova;

	/* find out the minimum page size supported */
	min_pagesz = 1 << __ffs(domain->pgsize_bitmap);

	/*
	 * The virtual address, as well as the size of the mapping, must be
	 * aligned (at least) to the size of the smallest page supported
	 * by the hardware
	 */
	if (!IS_ALIGNED(iova | size, min_pagesz)) {
		pr_err("unaligned: iova 0x%lx size 0x%zx min_pagesz 0x%x\n",
		       iova, size, min_pagesz);
		return -EINVAL;
	}

	pr_debug("unmap this: iova 0x%lx size 0x%zx\n", iova, size);

	spin_lock_irqsave(&domain->pgtablelock, flags);
#if IS_ENABLED(CONFIG_PCIE_IOMMU_MAP_ONCE)
	if (size < SZ_64K) { /* This code assume that there are no LARGE Pages(64KB) */
		unmapped = exynos_iommu_unmap_once(iova, size, pcie_channel);
		if (unmapped == size)
			goto end_unmap;
		else
			unmapped = 0;
	}
#endif
	/*
	 * Keep iterating until we either unmap 'size' bytes (or more)
	 * or we hit an area that isn't mapped.
	 */
	while (unmapped < size) {
		size_t pgsize = iommu_pgsize(iova, size - unmapped, pcie_channel);

		alloc_counter--;
		unmapped_page = exynos_iommu_unmap(iova, pgsize, pcie_channel);
		/* exynos_sysmmu_tlb_invalidate(iova, pgsize, pcie_vid); */
#if IS_ENABLED(CONFIG_PCIE_IOMMU_HISTORY_LOG)
		add_history_buff(&pcie_unmap_history, iova, orig_iova,
				size, orig_size);
#endif
		if (!unmapped_page)
			break;

		pr_debug("unmapped: iova 0x%lx size 0x%zx\n",
			 iova, unmapped_page);

		iova += unmapped_page;
		unmapped += unmapped_page;
	}
#if IS_ENABLED(CONFIG_PCIE_IOMMU_MAP_ONCE)
end_unmap:
#endif
	exynos_sysmmu_tlb_invalidate(orig_iova, orig_size, pcie_vid);
	spin_unlock_irqrestore(&domain->pgtablelock, flags);

	pr_debug("UNMAPPED : req 0x%lx(0x%lx) size 0x%zx(0x%zx)\n",
				changed_iova, orig_iova, size, orig_size);

	return unmapped;
}
EXPORT_SYMBOL_GPL(pcie_iommu_unmap);

static int sysmmu_parse_dt(struct device *sysmmu,
				struct sysmmu_drvdata *drvdata)
{
	unsigned int qos = DEFAULT_QOS_VALUE, mask, num_pmmu;
	int ret = 0, i;
	struct stream_props *props;

	/* Parsing QoS */
	ret = of_property_read_u32_index(sysmmu->of_node, "qos", 0, &qos);
	if (!ret && (qos > 15)) {
		dev_err(sysmmu, "Invalid QoS value %d, use default.\n", qos);
		qos = DEFAULT_QOS_VALUE;
	}
	drvdata->qos = qos;

	ret = of_property_read_u32_index(sysmmu->of_node, "vmid_mask", 0, &mask);
	if (!ret && (mask & ((1 << drvdata->max_vm) - 1)))
		drvdata->vmid_mask = mask;

	/* Parsing pmmu num */
	ret = of_property_read_u32_index(sysmmu->of_node, "num_pmmu", 0, &num_pmmu);
	if (ret) {
		dev_err(sysmmu, "failed to init number of pmmu\n");
		return ret;
	}
	drvdata->num_pmmu = num_pmmu;
	dev_info(sysmmu, "Number of PMMU : %d.\n", num_pmmu);

	props = devm_kcalloc(sysmmu, num_pmmu, sizeof(*props), GFP_KERNEL);
	if (!props)
		return -ENOMEM;
	drvdata->props = props;

	for (i = 0; i < drvdata->num_pmmu; i++) {
		ret = sysmmu_parse_stream_property(sysmmu, drvdata, i);
		if (ret)
			dev_err(sysmmu, "Failed to parse TLB property\n");
	}


	return 0;
}

static struct exynos_iommu_domain *exynos_iommu_domain_alloc(void)
{
	struct exynos_iommu_domain *domain;
	int __maybe_unused i;

	domain = kzalloc(sizeof(*domain), GFP_KERNEL);
	if (!domain)
		return NULL;

	/* 36bit VA FLPD must be aligned in 256KB */
	domain->pgtable =
		(sysmmu_pte_t *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, 6);
	if (!domain->pgtable)
		goto err_pgtable;

	domain->lv2entcnt = (atomic_t *)
			__get_free_pages(GFP_KERNEL | __GFP_ZERO, 6);
	if (!domain->lv2entcnt)
		goto err_counter;

	pgtable_flush(domain->pgtable, domain->pgtable + NUM_LV1ENTRIES);

	spin_lock_init(&domain->lock);
	spin_lock_init(&domain->pgtablelock);
	INIT_LIST_HEAD(&domain->clients_list);

	/* TODO: get geometry from device tree */
	domain->domain.geometry.aperture_start = 0;
	domain->domain.geometry.aperture_end   = ~0UL;
	domain->domain.geometry.force_aperture = true;

	domain->pgsize_bitmap = SECT_SIZE | LPAGE_SIZE | SPAGE_SIZE;

#ifdef USE_DYNAMIC_MEM_ALLOC
	domain->ext_buff[0].buff =
		kzalloc(SZ_2K * MAX_EXT_BUFF_NUM, GFP_KERNEL);

	if (!domain->ext_buff[0].buff) {
		pr_err("Can't allocate extend buffer!\n");
		goto err_counter;
	}

	/* ext_buff[0] is already initialized */
	for (i = 1; i < MAX_EXT_BUFF_NUM; i ++) {
		domain->ext_buff[i].index = i;
		domain->ext_buff[i].used = 0;
		domain->ext_buff[i].buff =
			domain->ext_buff[i - 1].buff +
			(SZ_2K / sizeof(sysmmu_pte_t));
	}
#endif

	return domain;

err_counter:
	free_pages((unsigned long)domain->pgtable, 6);
err_pgtable:
	kfree(domain);
	return NULL;
}


static int exynos_sysmmu_probe(struct platform_device *pdev)
{
	int irq, ret;
	struct device *dev = &pdev->dev;
	struct sysmmu_drvdata *data;
	struct resource *res;
	int __maybe_unused i;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "Failed to get resource info\n");
		return -ENOENT;
	}

	data->sfrbase = devm_ioremap_resource(dev, res);
	if (IS_ERR(data->sfrbase))
		return PTR_ERR(data->sfrbase);

	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		dev_err(dev, "Unable to find IRQ resource\n");
		return irq;
	}

	ret = devm_request_threaded_irq(dev, irq, NULL, exynos_sysmmu_irq,
				IRQF_ONESHOT, dev_name(dev), data);
	if (ret) {
		dev_err(dev, "Unabled to register handler of irq %d\n", irq);
		return ret;
	}
	data->irq_num = irq;

	data->dev = dev;
	spin_lock_init(&data->lock);
	ATOMIC_INIT_NOTIFIER_HEAD(&data->fault_notifiers);

	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	platform_set_drvdata(pdev, data);

	data->qos = DEFAULT_QOS_VALUE;
	g_sysmmu_drvdata = data;

	spin_lock_init(&data->mmu_ctrl_lock);

	data->version = get_hw_version(data, data->sfrbase);
	data->max_vm = get_num_vm(data, data->sfrbase);
	data->va_width = get_va_width(data, data->sfrbase);

	data->vmid_mask = SYSMMU_MASK_VMID;

	ret = sysmmu_parse_dt(data->dev, data);
	if (ret) {
		dev_err(dev, "Failed to parse DT\n");
		return ret;
	}

	/* Create PCIe Channel0/1 domains */
	data->domain[SYSMMU_PCIE_CH0] = exynos_iommu_domain_alloc();
	data->domain[SYSMMU_PCIE_CH1] = exynos_iommu_domain_alloc();
	data->pgtable[SYSMMU_PCIE_CH0] = virt_to_phys(data->domain[SYSMMU_PCIE_CH0]->pgtable);
	data->pgtable[SYSMMU_PCIE_CH1] = virt_to_phys(data->domain[SYSMMU_PCIE_CH1]->pgtable);

	dev_info(dev, "L1Page Table Address for PCIe Ch0 : 0x%llx(phys)\n", data->pgtable[SYSMMU_PCIE_CH0]);
	dev_info(dev, "L1Page Table Address for PCIe Ch1 : 0x%llx(phys)\n", data->pgtable[SYSMMU_PCIE_CH1]);

	dev_info(data->dev, "is probed. Version %d.%d.%d - MAX VM : %d, 36bit addr : %d\n",
			MMU_MAJ_VER(data->version),
			MMU_MIN_VER(data->version),
			MMU_REV_VER(data->version),
			data->max_vm, data->va_width);

	pm_runtime_put(&pdev->dev);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int exynos_sysmmu_suspend(struct device *dev)
{
	unsigned long flags;
	struct sysmmu_drvdata *drvdata = dev_get_drvdata(dev);

	spin_lock_irqsave(&drvdata->lock, flags);
	drvdata->is_suspended = true;
	spin_unlock_irqrestore(&drvdata->lock, flags);

	return 0;
}

static int exynos_sysmmu_resume(struct device *dev)
{
	unsigned long flags;
	struct sysmmu_drvdata *drvdata = dev_get_drvdata(dev);

	spin_lock_irqsave(&drvdata->lock, flags);
	drvdata->is_suspended = false;
	spin_unlock_irqrestore(&drvdata->lock, flags);

	return 0;
}
#endif

static const struct dev_pm_ops sysmmu_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(exynos_sysmmu_suspend,
					exynos_sysmmu_resume)
};

static const struct of_device_id sysmmu_of_match[] = {
	{ .compatible	= "samsung,pcie-sysmmu", },
	{ },
};

static struct platform_driver exynos_sysmmu_driver __refdata = {
	.probe	= exynos_sysmmu_probe,
	.driver	= {
		.name		= "pcie-sysmmu",
		.of_match_table	= sysmmu_of_match,
		.pm		= &sysmmu_pm_ops,
	}
};

static int __init pcie_iommu_init(void)
{
#ifndef USE_DYNAMIC_MEM_ALLOC /* If it doesn't use genpool */
	u8 *gen_buff;
#endif
	phys_addr_t buff_paddr;
	int ret;

	if (lv2table_kmem_cache)
		return 0;

	lv2table_kmem_cache = kmem_cache_create("pcie-iommu-lv2table",
			LV2TABLE_AND_REFBUF_SZ, LV2TABLE_AND_REFBUF_SZ, 0, NULL);
	if (!lv2table_kmem_cache) {
		pr_err("%s: Failed to create kmem cache\n", __func__);
		return -ENOMEM;
	}

#ifndef USE_DYNAMIC_MEM_ALLOC /* If it doesn't use genpool */
	gen_buff = kzalloc(LV2_GENPOOL_SZIE, GFP_KERNEL);
	if (gen_buff == NULL) {
		pr_err("Failed to allocate pool buffer\n");
		return -ENOMEM;
	}
	buff_paddr = virt_to_phys(gen_buff);
	lv2table_pool = gen_pool_create(ilog2(LV2TABLE_AND_REFBUF_SZ), -1);
	if (!lv2table_pool) {
		pr_err("Failed to allocate lv2table gen pool\n");
		return -ENOMEM;
	}

	ret = gen_pool_add(lv2table_pool, (unsigned long)buff_paddr,
			LV2_GENPOOL_SZIE, -1);
	if (ret)
		return -ENOMEM;
#endif

	ret = platform_driver_register(&exynos_sysmmu_driver);
	if (ret != 0) {
		kmem_cache_destroy(lv2table_kmem_cache);
#ifndef USE_DYNAMIC_MEM_ALLOC /* If it doesn't use genpool */
		gen_pool_destroy(lv2table_pool);
#endif
	}

	return ret;
}
device_initcall(pcie_iommu_init);

MODULE_AUTHOR("Kisang Lee <kisang80.lee@samsung.com>");
MODULE_DESCRIPTION("Exynos PCIe SysMMU driver");
MODULE_LICENSE("GPL v2");
