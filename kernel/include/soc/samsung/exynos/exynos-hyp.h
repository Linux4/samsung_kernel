/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - Hypervisor module
 *
 */

#ifndef __EXYNOS_HYP_H
#define __EXYNOS_HYP_H

#include <asm/kvm_pkvm_module.h>

#define cpu_reg(ctxt, r)	(ctxt)->regs.regs[r]
#define DECLARE_REG(type, name, ctxt, reg)	\
				type name = (type)cpu_reg(ctxt, (reg))

#define EXYNOS_HYP_HEADER_MAGIC_SIZE		(0x10)
#define EXYNOS_HYP_HEADER_INST			(2)

#ifndef __ASSEMBLY__

struct exynos_module_ops {
	int (*create_private_mapping)(phys_addr_t phys, size_t size,
				      enum kvm_pgtable_prot prot,
				      unsigned long *haddr);
	void *(*fixmap_map)(phys_addr_t phys);
	void (*fixmap_unmap)(void);
	void (*flush_dcache_to_poc)(void *addr, size_t size);
	int (*host_stage2_mod_prot)(u64 pfn, enum kvm_pgtable_prot prot);
	int (*host_stage2_get_leaf)(phys_addr_t phys, kvm_pte_t *ptep, u32 *level);
	int (*host_donate_hyp)(u64 pfn, u64 nr_pages);
	int (*hyp_donate_host)(u64 pfn, u64 nr_pages);
	phys_addr_t (*hyp_pa)(void *x);
	void* (*hyp_va)(phys_addr_t phys);
	int (*host_share_hyp)(u64 pfn);
	int (*host_unshare_hyp)(u64 pfn);
	int (*pin_shared_mem)(void *from, void *to);
	void (*unpin_shared_mem)(void *from, void *to);
	void (*update_hcr_el2)(unsigned long set_mask, unsigned long clear_mask);
	void (*update_hfgwtr_el2)(unsigned long set_mask, unsigned long clear_mask);
};

typedef unsigned long (*hyp_entry_t)(struct exynos_module_ops *ops,
				     unsigned long va_offset,
				     unsigned long kimg_offset,
				     unsigned long x3);

typedef unsigned long (*hyp_hvc_handler_t)(unsigned int hvc_fid,
					   unsigned long x1,
					   unsigned long x2,
					   unsigned long x3,
					   unsigned long x4,
					   void *cookie,
					   void *cpu_context);

typedef unsigned long (*hyp_s2mpu_handler_t)(unsigned long x0,
					     unsigned long x1,
					     unsigned long x2,
					     unsigned long x3,
					     unsigned long x4);
typedef int (*hyp_sync_handler_t)(unsigned long esr_el2,
				  unsigned long elr_el2,
				  unsigned long sp_el1,
				  void *gpregs,
				  void *cookie);

typedef void (*hyp_psci_handler_t)(unsigned int psci_type,
				   unsigned long power_state);

typedef int (*plugin_runtime_init_t)(uintptr_t plugin_runtime_entry,
				     unsigned long va_offset,
				     unsigned long plugin_base,
				     size_t plugin_size);

/*
 * struct exynos_hyp_mod_sec - Exynos Hypervisor module section
 *
 */
struct exynos_hyp_mod_sec {
	unsigned long long text_start;
	unsigned long long text_end;
	unsigned long long rodata_start;
	unsigned long long rodata_end;
	unsigned long long rwdata_start;
	unsigned long long rwdata_end;
	unsigned long long got_start;
	unsigned long long got_end;
	unsigned long long rela_start;
	unsigned long long rela_end;
};

/*
 * struct exynos_hyp_header - Exynos Hypervisor module binary header
 *
 */
struct exynos_hyp_header {
	unsigned long long head[EXYNOS_HYP_HEADER_INST];
	char bin_str[EXYNOS_HYP_HEADER_MAGIC_SIZE];
	struct exynos_hyp_mod_sec sec_info;
	unsigned long exynos_hyp_entrypoint;
};

/*
 * struct plugin_memory_info - Plug-in memory/address information
 *
 */
struct plugin_memory_info {
	unsigned long base_va;
	unsigned long base;
	size_t size;
	unsigned long offset;
	struct exynos_hyp_mod_sec section;
};

/*
 * struct exynos_module_info
 * struct exynos_plugin_info
 *
 * - The information of Plug-in modules
 */
struct plugin_module_info {
	struct plugin_memory_info mem;
	uintptr_t plugin_runtime_entry;
};

struct exynos_plugin_info {
	struct plugin_module_info *modules;
	unsigned int plugin_num;
};

struct sysreg_mask {
	unsigned long set_mask;
	unsigned long clear_mask;
};
/*
 * struct exynos_hyp_interface - Exynos Hypervisor module binary interface
 *
 */
struct exynos_hyp_interface {
	hyp_hvc_handler_t hvc_handler;
	hyp_s2mpu_handler_t s2mpu_prepare;
	hyp_sync_handler_t sync_handler;
	hyp_psci_handler_t psci_handler;
	struct exynos_plugin_info *plugin_module;
	plugin_runtime_init_t plugin_runtime_init;
	struct sysreg_mask hcr_el2_mask;
};

int exynos_hyp_update_fg_write_trap(unsigned long sysreg_bit_pos,
				    bool trap);

int exynos_hyp_s2mpu_driver_init(const struct pkvm_module_ops *ops,
				 struct exynos_hyp_interface *hyp_intfc);
int exynos_pkvm_s2mpu_register(struct device *dev, unsigned long pa);

#endif	/* __ASSEMBLY__ */

#endif	/* __EXYNOS_HYP_H */
