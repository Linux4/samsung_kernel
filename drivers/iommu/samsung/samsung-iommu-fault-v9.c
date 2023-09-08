// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 */

#define pr_fmt(fmt) "sysmmu: " fmt

#include <linux/smc.h>
#include <linux/arm-smccc.h>
#include <linux/sec_debug.h>
#include "samsung-iommu-v9.h"

#define SYSMMU_FAULT_PTW_ACCESS		0
#define SYSMMU_FAULT_PAGE		1
#define SYSMMU_FAULT_ACCESS		2
#define SYSMMU_FAULT_CONTEXT		3
#define SYSMMU_FAULT_UNKNOWN		4
#define SYSMMU_FAULTS_NUM		(SYSMMU_FAULT_UNKNOWN + 1)

#define REG_MMU_PMMU_PTLB_INFO(n)		(0x3400 + ((n) * 0x4))
#define REG_MMU_STLB_INFO(n)			(0x3800 + ((n) * 0x4))
#define REG_MMU_S1L1TLB_INFO			0x3C00

#define REG_MMU_READ_PTLB			0x5000
#define REG_MMU_READ_PTLB_TPN			0x5004
#define REG_MMU_READ_PTLB_PPN			0x5008
#define REG_MMU_READ_PTLB_ATTRIBUTE		0x500C

#define REG_MMU_READ_STLB			0x5010
#define REG_MMU_READ_STLB_TPN			0x5014
#define REG_MMU_READ_STLB_PPN			0x5018
#define REG_MMU_READ_STLB_ATTRIBUTE		0x501C

#define REG_MMU_READ_S1L1TLB			0x5020
#define REG_MMU_READ_S1L1TLB_VPN		0x5024
#define REG_MMU_READ_S1L1TLB_SLPT_OR_PPN	0x5028
#define REG_MMU_READ_S1L1TLB_ATTRIBUTE		0x502C

#define REG_MMU_FAULT_STATUS_VM			0x8060
#define REG_MMU_FAULT_CLEAR_VM			0x8064
#define REG_MMU_FAULT_VA_VM			0x8070
#define REG_MMU_FAULT_INFO0_VM			0x8074
#define REG_MMU_FAULT_INFO1_VM			0x8078
#define REG_MMU_FAULT_INFO2_VM			0x807C
#define REG_MMU_FAULT_RW_MASK			GENMASK(20, 20)
#define IS_READ_FAULT(x)			(((x) & REG_MMU_FAULT_RW_MASK) == 0)

#define MMU_PMMU_PTLB_INFO_NUM_WAY(reg)			(((reg) >> 16) & 0xFFFF)
#define MMU_PMMU_PTLB_INFO_NUM_SET(reg)			((reg) & 0xFFFF)
#define MMU_READ_PTLB_TPN_VALID(reg)			(((reg) >> 28) & 0x1)
#define MMU_READ_PTLB_TPN_S1_ENABLE(reg)		(((reg) >> 24) & 0x1)
#define MMU_VADDR_FROM_PTLB(reg)			(((reg) & 0xFFFFFF) << SPAGE_ORDER)
#define MMU_PADDR_FROM_PTLB(reg)			(((reg) & 0xFFFFFF) << SPAGE_ORDER)
#define MMU_SET_READ_PTLB_ENTRY(way, set, ptlb, pmmu)	((pmmu) | ((ptlb) << 4) |		\
							((set) << 16) | ((way) << 24))

#define MMU_SWALKER_INFO_NUM_STLB(reg)			(((reg) >> 16) & 0xFFFF)
#define MMU_SWALKER_INFO_NUM_PMMU(reg)			((reg) & 0xF)
#define MMU_STLB_INFO_NUM_WAY(reg)			(((reg) >> 16) & 0xFFFF)
#define MMU_STLB_INFO_NUM_SET(reg)			((reg) & 0xFFFF)
#define MMU_READ_STLB_TPN_VALID(reg)			(((reg) >> 28) & 0x1)
#define MMU_READ_STLB_TPN_S1_ENABLE(reg)		(((reg) >> 24) & 0x1)
#define MMU_VADDR_FROM_STLB(reg)			(((reg) & 0xFFFFFF) << SPAGE_ORDER)
#define MMU_PADDR_FROM_STLB(reg)			(((reg) & 0xFFFFFF) << SPAGE_ORDER)
#define MMU_SET_READ_STLB_ENTRY(way, set, stlb, line)	((set) | ((way) << 8) |			\
							((line) << 16) | ((stlb) << 20))

#define MMU_S1L1TLB_INFO_NUM_SET(reg)			(((reg) >> 16) & 0xFFFF)
#define MMU_S1L1TLB_INFO_NUM_WAY(reg)			(((reg) >> 12) & 0xF)
#define MMU_SET_READ_S1L1TLB_ENTRY(way, set)		((set) | ((way) << 8))
#define MMU_READ_S1L1TLB_VPN_VALID(reg)			(((reg) >> 28) & 0x1)
#define MMU_VADDR_FROM_S1L1TLB(reg)			(((reg) & 0xFFFFFF) << SPAGE_ORDER)
#define MMU_PADDR_FROM_S1L1TLB_PPN(reg)			(((reg) & 0xFFFFFF) << SPAGE_ORDER)
#define MMU_PADDR_FROM_S1L1TLB_BASE(reg)		(((reg) & 0x3FFFFFF) << 10)
#define MMU_S1L1TLB_ATTRIBUTE_PS(reg)			(((reg) >> 8) & 0x7)

#define MMU_FAULT_INFO0_VA_36(reg)			(((reg) >> 21) & 0x1)
#define MMU_FAULT_INFO0_VA_HIGH(reg)			(((u64)(reg) & 0x3C00000) << 10)
#define MMU_FAULT_INFO0_LEN(reg)			(((reg) >> 16) & 0xF)
#define MMU_FAULT_INFO0_ASID(reg)			((reg) & 0xFFFF)
#define MMU_FAULT_INFO1_AXID(reg)			(reg)
#define MMU_FAULT_INFO2_PMMU_ID(reg)			(((reg) >> 24) & 0xFF)
#define MMU_FAULT_INFO2_STREAM_ID(reg)			((reg) & 0xFFFFFF)

#define SLPT_BASE_FLAG		0x6

#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
#define SMC_DRM_SEC_SMMU_INFO          (0x820020D0)
/* secure SysMMU SFR access */
enum sec_sysmmu_sfr_access_t {
	SEC_SMMU_SFR_READ,
	SEC_SMMU_SFR_WRITE,
};

#define is_secure_info_fail(x)		((((x) >> 16) & 0xffff) == 0xdead)
static inline u32 read_sec_info(unsigned int addr)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SMC_DRM_SEC_SMMU_INFO,
		      (unsigned long)addr, 0, SEC_SMMU_SFR_READ, 0, 0, 0, 0,
		      &res);
	if (is_secure_info_fail(res.a0))
		pr_err("Invalid value returned, %#lx\n", res.a0);

	return (u32)res.a0;
}
#else
static inline u32 read_sec_info(unsigned int addr)
{
	return 0xdead;
}
#endif

static char *sysmmu_fault_name[SYSMMU_FAULTS_NUM] = {
	"PTW ACCESS FAULT",
	"PAGE FAULT",
	"ACCESS FAULT",
	"CONTEXT FAULT",
	"UNKNOWN FAULT"
};

static int sysmmu_fault_type[SYSMMU_FAULTS_NUM] = {
	IOMMU_FAULT_REASON_WALK_EABT,
	IOMMU_FAULT_REASON_PTE_FETCH,
	IOMMU_FAULT_REASON_ACCESS,
	IOMMU_FAULT_REASON_PASID_FETCH,
	IOMMU_FAULT_REASON_UNKNOWN,
};

struct samsung_sysmmu_fault_info {
	struct sysmmu_drvdata *drvdata;
	struct iommu_fault_event event;
};

static inline u32 __sysmmu_get_intr_status(struct sysmmu_drvdata *data, bool is_secure, int *vmid)
{
	int i;
	u32 val = 0x0;

	for (i = 0; i < data->max_vm; i++) {
		if (!(data->vmid_mask & (1 << i)))
			continue;

		if (is_secure)
			val = read_sec_info(
				MMU_VM_ADDR(data->secure_base + REG_MMU_FAULT_STATUS_VM, i));
		else
			val = readl_relaxed(
				MMU_VM_ADDR(data->sfrbase + REG_MMU_FAULT_STATUS_VM, i));

		if (val & 0xF) {
			*vmid = i;
			break;
		}
	}

	return val;
}

static inline u64 __sysmmu_get_fault_address(struct sysmmu_drvdata *data, bool is_secure, int vmid)
{
	u64 va;
	u32 val;

	if (is_secure) {
		va = read_sec_info(MMU_VM_ADDR(data->secure_base + REG_MMU_FAULT_VA_VM, vmid));
		val = read_sec_info(MMU_VM_ADDR(data->secure_base + REG_MMU_FAULT_INFO0_VM, vmid));
	} else {
		va = readl_relaxed(MMU_VM_ADDR(data->sfrbase + REG_MMU_FAULT_VA_VM, vmid));
		val = readl_relaxed(MMU_VM_ADDR(data->sfrbase + REG_MMU_FAULT_INFO0_VM, vmid));
	}

	if (MMU_FAULT_INFO0_VA_36(val))
		va += MMU_FAULT_INFO0_VA_HIGH(val);

	return va;
}

static inline void sysmmu_tlb_compare(phys_addr_t pgtable, u32 vpn, u32 ppn, const char *tlb_type)
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
		phys |= (vpn & 0xFF) << SPAGE_ORDER;
	} else if (lv1ent_page(entry)) {
		entry = page_entry(entry, vaddr);

		if (lv2ent_large(entry)) {
			phys = lpage_phys(entry);
			phys |= (vpn & 0xF) << SPAGE_ORDER;
		} else if (lv2ent_small(entry))
			phys = spage_phys(entry);
	} else {
		pr_crit(">> Invalid address detected! entry: %#lx", (unsigned long)*entry);
		return;
	}

	if (paddr != phys) {
		pr_crit(">> %s mismatch detected!\n", tlb_type);
		pr_crit("   %s: %#010lx, PT entry: %#010lx\n", tlb_type, paddr, phys);
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

			sysmmu_tlb_compare(pgtable, tpn, ppn, "PTLB");
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
					   int num_ptlb, int pmmu_id)
{
	int way, t;
	unsigned int cnt;
	u32 info;

	for (t = 0; t < num_ptlb; t++) {
		int num_way, num_set;

		info = readl_relaxed(drvdata->sfrbase + REG_MMU_PMMU_PTLB_INFO(t));
		num_way = MMU_PMMU_PTLB_INFO_NUM_WAY(info);
		num_set = MMU_PMMU_PTLB_INFO_NUM_SET(info);

		pr_crit("PMMU.%d PTLB.%d has %d way, %d set.\n", pmmu_id, t, num_way, num_set);
		pr_crit("------------- PTLB[WAY][SET][ENTRY] -------------\n");
		for (way = 0, cnt = 0; way < num_way; way++)
			cnt += dump_ptlb_entry(drvdata, pgtable, t, way, num_set, pmmu_id);
	}
	if (!cnt)
		pr_crit(">> No Valid PTLB Entries\n");
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

			sysmmu_tlb_compare(pgtable, tpn, ppn, "STLB");
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

DEFINE_STATIC_PR_AUTO_NAME_ONCE(iommu, ASL4);

static inline void dump_sysmmu_stlb_status(struct sysmmu_drvdata *drvdata, phys_addr_t pgtable,
					   int num_stlb)
{
	int way, t;
	unsigned int cnt;
	u32 info;

	for (t = 0; t < num_stlb; t++) {
		int num_way, num_set;

		info = readl_relaxed(drvdata->sfrbase + REG_MMU_STLB_INFO(t));
		num_way = MMU_STLB_INFO_NUM_WAY(info);
		num_set = MMU_STLB_INFO_NUM_SET(info);

		pr_crit("STLB.%d has %d way, %d set.\n", t, num_way, num_set);
		pr_crit("------------- STLB[WAY][SET][ENTRY] -------------\n");
		for (way = 0, cnt = 0; way < num_way; way++)
			cnt += dump_stlb_entry(drvdata, pgtable, t, way, num_set);
	}
	if (!cnt)
		pr_auto_name(iommu, ">> No Valid STLB Entries\n");
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
		pr_auto_name(iommu, ">> No Valid S1L1TLB Entries\n");
}

static inline void dump_sysmmu_tlb_status(struct sysmmu_drvdata *drvdata, phys_addr_t pgtable,
					  int pmmu_id)
{
	u32 swalker;
	int num_stlb, num_ptlb;
	void __iomem *sfrbase = drvdata->sfrbase;

	swalker = readl_relaxed(sfrbase + REG_MMU_SWALKER_INFO);
	num_stlb = MMU_SWALKER_INFO_NUM_STLB(swalker);

	writel_relaxed(MMU_SET_PMMU_INDICATOR(pmmu_id), sfrbase + REG_MMU_PMMU_INDICATOR);
	num_ptlb = drvdata->num_ptlb[pmmu_id];

	pr_crit("SysMMU has %d PTLBs(PMMU %d), %d STLBs, 1 S1L1TLB\n",
		num_ptlb, pmmu_id, num_stlb);

	dump_sysmmu_ptlb_status(drvdata, pgtable, num_ptlb, pmmu_id);
	dump_sysmmu_stlb_status(drvdata, pgtable, num_stlb);
	dump_sysmmu_s1l1tlb_status(drvdata, pgtable);
}

static inline void dump_sysmmu_status(struct sysmmu_drvdata *drvdata, phys_addr_t pgtable,
				      int vmid, int pmmu_id)
{
	int info;
	void __iomem *sfrbase = drvdata->sfrbase;

	info = MMU_VERSION_RAW(readl_relaxed(sfrbase + REG_MMU_VERSION));

	pr_crit("ADDR: (VA: %p), MMU_CTRL: %#010x, PT_BASE: %#010x, VMID:%d\n",
		sfrbase,
		readl_relaxed(sfrbase + REG_MMU_CTRL),
		readl_relaxed(MMU_VM_ADDR(sfrbase + REG_MMU_CONTEXT0_CFG_FLPT_BASE_VM, vmid)),
		vmid);
	pr_crit("VERSION %d.%d.%d, MMU_STATUS: %#010x\n",
		MMU_VERSION_MAJOR(info), MMU_VERSION_MINOR(info), MMU_VERSION_REVISION(info),
		readl_relaxed(sfrbase + REG_MMU_STATUS));

	pr_crit("MMU_CTRL_VM: %#010x, MMU_CFG_VM: %#010x\n",
		readl_relaxed(MMU_VM_ADDR(sfrbase + REG_MMU_CTRL_VM, vmid)),
		readl_relaxed(MMU_VM_ADDR(sfrbase + REG_MMU_CONTEXT0_CFG_ATTRIBUTE_VM, vmid)));

	if (pmmu_id < 0 || pmmu_id >= drvdata->num_pmmu) {
		pr_crit("pmmu is out of range\n");
		return;
	}

	dump_sysmmu_tlb_status(drvdata, pgtable, pmmu_id);
}

static void sysmmu_show_secure_fault_information(struct sysmmu_drvdata *drvdata, int intr_type,
						 unsigned long fault_addr, int vmid)
{
	const char *port_name = NULL;
	unsigned int info0, info1, info2;
	phys_addr_t pgtable;
	unsigned int sfrbase = drvdata->secure_base;
#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	char temp_buf[SZ_128];
#endif

	pgtable = read_sec_info(MMU_VM_ADDR(sfrbase + REG_MMU_CONTEXT0_CFG_FLPT_BASE_VM, vmid));
	pgtable <<= PAGE_SHIFT;

	info0 = read_sec_info(MMU_VM_ADDR(sfrbase + REG_MMU_FAULT_INFO0_VM, vmid));
	info1 = read_sec_info(MMU_VM_ADDR(sfrbase + REG_MMU_FAULT_INFO1_VM, vmid));
	info2 = read_sec_info(MMU_VM_ADDR(sfrbase + REG_MMU_FAULT_INFO2_VM, vmid));

	of_property_read_string(drvdata->dev->of_node, "port-name", &port_name);

	pr_auto_name_once(iommu);
	pr_auto_name(iommu, "----------------------------------------------------------\n");
	pr_auto_name(iommu, "From [%s], SysMMU %s %s at %#010lx (page table @ %pa)\n",
		port_name ? port_name : dev_name(drvdata->dev),
		IS_READ_FAULT(info0) ? "READ" : "WRITE",
		sysmmu_fault_name[intr_type], fault_addr, &pgtable);

#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	snprintf(temp_buf, SZ_128, "%s %s %s at %#010lx (%pa)",
		port_name ? port_name : dev_name(drvdata->dev),
		IS_READ_FAULT(info0) ? "READ" : "WRITE",
		sysmmu_fault_name[intr_type], fault_addr, &pgtable);
	secdbg_exin_set_sysmmu(temp_buf);
#endif

	if (intr_type == SYSMMU_FAULT_UNKNOWN) {
		pr_auto_name(iommu, "The fault is not caused by this System MMU.\n");
		pr_auto_name(iommu, "Please check IRQ and SFR base address.\n");
		goto finish;
	}

	if (intr_type == SYSMMU_FAULT_CONTEXT) {
		pr_auto_name(iommu, "Context fault\n");
		goto finish;
	}

	pr_auto_name(iommu, "ASID: %#x, Burst LEN: %#x, AXI ID: %#x, PMMU ID: %#x, STREAM ID: %#x\n",
		MMU_FAULT_INFO0_ASID(info0), MMU_FAULT_INFO0_LEN(info0),
		MMU_FAULT_INFO1_AXID(info1), MMU_FAULT_INFO2_PMMU_ID(info2),
		MMU_FAULT_INFO2_STREAM_ID(info2));

	if (!pfn_valid(pgtable >> PAGE_SHIFT)) {
		pr_auto_name(iommu, "Page table base is not in a valid memory region\n");
		pgtable = 0;
	}

	if (intr_type == SYSMMU_FAULT_PTW_ACCESS) {
		pr_auto_name(iommu, "System MMU has failed to access page table\n");
		pgtable = 0;
	}

	info0 = MMU_VERSION_RAW(read_sec_info(sfrbase + REG_MMU_VERSION));

	pr_auto_name(iommu, "ADDR: %#x, MMU_CTRL: %#010x, PT_BASE: %#010x\n",
		sfrbase,
		read_sec_info(sfrbase + REG_MMU_CTRL),
		read_sec_info(MMU_VM_ADDR(sfrbase + REG_MMU_CONTEXT0_CFG_FLPT_BASE_VM, vmid)));
	pr_auto_name(iommu, "VERSION %d.%d.%d, MMU_STATUS: %#010x\n",
		MMU_VERSION_MAJOR(info0), MMU_VERSION_MINOR(info0), MMU_VERSION_REVISION(info0),
		read_sec_info(sfrbase + REG_MMU_STATUS));

finish:
	pr_auto_name(iommu, "----------------------------------------------------------\n");
	pr_auto_name_disable(iommu);
}


static void sysmmu_show_fault_info_simple(struct sysmmu_drvdata *drvdata, int intr_type,
					  unsigned long fault_addr, int vmid, phys_addr_t *pt)
{
	const char *port_name = NULL;
	phys_addr_t pgtable;
	u32 info0;
#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	char temp_buf[SZ_128];
#endif

	pgtable = readl_relaxed(MMU_VM_ADDR(drvdata->sfrbase + REG_MMU_CONTEXT0_CFG_FLPT_BASE_VM,
					    vmid));
	pgtable <<= PAGE_SHIFT;

	of_property_read_string(drvdata->dev->of_node, "port-name", &port_name);

	info0 = readl_relaxed(MMU_VM_ADDR(drvdata->sfrbase + REG_MMU_FAULT_INFO0_VM, vmid));

	pr_auto_name_on(pt, iommu, "From [%s], SysMMU %s %s at %#010lx (pgtable @ %pa\n",
		port_name ? port_name : dev_name(drvdata->dev),
		IS_READ_FAULT(info0) ? "READ" : "WRITE",
		sysmmu_fault_name[intr_type], fault_addr, &pgtable);
#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	/* only for called by sysmmu_show_fault_information */
	if (pt) {
		snprintf(temp_buf, SZ_128, "%s %s %s at %#010lx (%pa)\n",
			port_name ? port_name : dev_name(drvdata->dev),
			IS_READ_FAULT(info0) ? "READ" : "WRITE",
			sysmmu_fault_name[intr_type], fault_addr, &pgtable);
		secdbg_exin_set_sysmmu(temp_buf);
	}
#endif

	if (pt)
		*pt = pgtable;
}

static void sysmmu_show_fault_information(struct sysmmu_drvdata *drvdata, int intr_type,
					  unsigned long fault_addr, int vmid)
{
	phys_addr_t pgtable;
	u32 info0, info1, info2;
	int pmmu_id, stream_id;

	pr_auto_name_once(iommu);
	pr_auto_name(iommu, "----------------------------------------------------------\n");
	sysmmu_show_fault_info_simple(drvdata, intr_type, fault_addr, vmid, &pgtable);

	info0 = readl_relaxed(MMU_VM_ADDR(drvdata->sfrbase + REG_MMU_FAULT_INFO0_VM, vmid));
	info1 = readl_relaxed(MMU_VM_ADDR(drvdata->sfrbase + REG_MMU_FAULT_INFO1_VM, vmid));
	info2 = readl_relaxed(MMU_VM_ADDR(drvdata->sfrbase + REG_MMU_FAULT_INFO2_VM, vmid));
	pmmu_id = MMU_FAULT_INFO2_PMMU_ID(info2);
	stream_id = MMU_FAULT_INFO2_STREAM_ID(info2);

	pr_crit("ASID: %d, Burst LEN: %d, AXI ID: %d, PMMU ID: %d, STREAM ID: %d\n",
		MMU_FAULT_INFO0_ASID(info0), MMU_FAULT_INFO0_LEN(info0),
		MMU_FAULT_INFO1_AXID(info1), pmmu_id, stream_id);

	if (intr_type == SYSMMU_FAULT_UNKNOWN) {
		pr_crit("The fault is not caused by this System MMU.\n");
		pr_crit("Please check IRQ and SFR base address.\n");
		goto finish;
	}

	if (intr_type == SYSMMU_FAULT_CONTEXT) {
		pr_crit("Context fault\n");
		goto finish;
	}

	if (pgtable != drvdata->pgtable)
		pr_auto_name(iommu, "Page table base of driver: %pK\n", &drvdata->pgtable);

	if (!pfn_valid(pgtable >> PAGE_SHIFT)) {
		pr_auto_name(iommu, "Page table base is not in a valid memory region\n");
		pgtable = 0;
	} else {
		sysmmu_pte_t *ent;

		ent = section_entry(phys_to_virt(pgtable), fault_addr);
		pr_auto_name(iommu, "Lv1 entry: %#010x\n", *ent);

		if (lv1ent_page(ent)) {
			ent = page_entry(ent, fault_addr);
			pr_auto_name(iommu, "Lv2 entry: %#010x\n", *ent);
		}
	}

	if (intr_type == SYSMMU_FAULT_PTW_ACCESS) {
		pr_auto_name(iommu, "System MMU has failed to access page table\n");
		pgtable = 0;
	}

	dump_sysmmu_status(drvdata, pgtable, vmid, pmmu_id);
finish:
	pr_auto_name(iommu, "----------------------------------------------------------\n");
	pr_auto_name_disable(iommu);
}

static void sysmmu_get_interrupt_info(struct sysmmu_drvdata *data, bool is_secure,
				      unsigned long *addr, int *intr_type, int *vmid)
{
	*intr_type = __ffs(__sysmmu_get_intr_status(data, is_secure, vmid));
	*addr = __sysmmu_get_fault_address(data, is_secure, *vmid);
}

static void sysmmu_clear_interrupt(struct sysmmu_drvdata *data, int *vmid)
{
	u32 val = __sysmmu_get_intr_status(data, false, vmid);

	writel(val, MMU_VM_ADDR(data->sfrbase + REG_MMU_FAULT_CLEAR_VM, *vmid));
}

irqreturn_t samsung_sysmmu_irq(int irq, void *dev_id)
{
	int itype, vmid;
	unsigned long addr;
	struct sysmmu_drvdata *drvdata = dev_id;
	bool is_secure = (irq == drvdata->secure_irq);

	dev_info(drvdata->dev, "[%s] interrupt (%d) happened\n",
		 is_secure ? "Secure" : "Non-secure", irq);

	if (drvdata->async_fault_mode)
		return IRQ_WAKE_THREAD;

	sysmmu_get_interrupt_info(drvdata, is_secure, &addr, &itype, &vmid);
	if (is_secure)
		sysmmu_show_secure_fault_information(drvdata, itype, addr, vmid);
	else
		sysmmu_show_fault_information(drvdata, itype, addr, vmid);

	return IRQ_WAKE_THREAD;
}

static int samsung_sysmmu_fault_notifier(struct device *dev, void *data)
{
	struct samsung_sysmmu_fault_info *fi;
	struct sysmmu_clientdata *client;
	struct sysmmu_drvdata *drvdata;
	int i, ret, result = 0;

	fi = (struct samsung_sysmmu_fault_info *)data;
	drvdata = fi->drvdata;

	client = (struct sysmmu_clientdata *) dev_iommu_priv_get(dev);

	for (i = 0; i < client->sysmmu_count; i++) {
		if (drvdata == client->sysmmus[i]) {
			ret = iommu_report_device_fault(dev, &fi->event);
			if (ret == -EAGAIN)
				result = ret;
			break;
		}
	}

	return result;
}

irqreturn_t samsung_sysmmu_irq_thread(int irq, void *dev_id)
{
	int itype, vmid, ret;
	unsigned long addr;
	struct sysmmu_drvdata *drvdata = dev_id;
	bool is_secure = (irq == drvdata->secure_irq);
	struct iommu_group *group = drvdata->group;
	enum iommu_fault_reason reason;
	struct samsung_sysmmu_fault_info fi = {
		.drvdata = drvdata,
		.event.fault.type = IOMMU_FAULT_DMA_UNRECOV,
	};
	const char *port_name = NULL;
	u32 info;

	sysmmu_get_interrupt_info(drvdata, is_secure, &addr, &itype, &vmid);
	reason = sysmmu_fault_type[itype];

	fi.event.fault.event.addr = addr;
	fi.event.fault.event.reason = reason;
	if (reason == IOMMU_FAULT_REASON_PTE_FETCH ||
	    reason == IOMMU_FAULT_REASON_PERMISSION)
		fi.event.fault.type = IOMMU_FAULT_PAGE_REQ;

	ret = iommu_group_for_each_dev(group, &fi, samsung_sysmmu_fault_notifier);
	if (ret == -EAGAIN && !is_secure) {
		sysmmu_show_fault_info_simple(drvdata, itype, addr, vmid, NULL);
		sysmmu_clear_interrupt(drvdata, &vmid);
		return IRQ_HANDLED;
	}

	if (drvdata->async_fault_mode) {
		if (is_secure)
			sysmmu_show_secure_fault_information(drvdata, itype, addr, vmid);
		else
			sysmmu_show_fault_information(drvdata, itype, addr, vmid);
	}

	info = readl_relaxed(MMU_VM_ADDR(drvdata->sfrbase + REG_MMU_FAULT_INFO0_VM, vmid));
	of_property_read_string(drvdata->dev->of_node, "port-name", &port_name);
	panic("(%s) From [%s], SysMMU %s %s at %#010lx\n",
	      dev_name(drvdata->dev), port_name ? port_name : dev_name(drvdata->dev),
	      IS_READ_FAULT(info) ? "READ" : "WRITE", sysmmu_fault_name[itype], addr);

	return IRQ_HANDLED;
}
