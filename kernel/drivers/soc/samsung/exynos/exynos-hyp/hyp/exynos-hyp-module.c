/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - EL2 Hypervisor module
 *
 */

#include <asm/alternative-macros.h>
#include <asm/kvm_host.h>
#include <asm/kvm_hyp.h>
#include <asm/kvm_pkvm_module.h>
#include <asm/page.h>

#include <linux/elf.h>
#include <linux/err.h>
#include <linux/mm.h>

#include <soc/samsung/exynos/exynos-hyp.h>
#include <soc/samsung/exynos/exynos-hvc.h>


unsigned long long hyp_base;
unsigned long long hyp_size;
unsigned long long hyp_offset;
unsigned long long hyp_kimg_offset;
static hyp_entry_t hyp_entry;
static hyp_hvc_handler_t exynos_hyp_mod_hvc_handler;
static hyp_sync_handler_t exynos_hyp_mod_sync_handler;
static hyp_psci_handler_t exynos_hyp_mod_psci_handler;
static struct exynos_module_ops exynos_mod_ops;


void exynos_hyp_hvc_handler(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(unsigned int, id, host_ctxt, 1);
	DECLARE_REG(unsigned long, x1, host_ctxt, 2);
	DECLARE_REG(unsigned long, x2, host_ctxt, 3);
	DECLARE_REG(unsigned long, x3, host_ctxt, 4);
	DECLARE_REG(unsigned long, x4, host_ctxt, 5);
	unsigned long x0;

	if (exynos_hyp_mod_hvc_handler == NULL)
		return;

	x0 = exynos_hyp_mod_hvc_handler(id, x1, x2, x3, x4, NULL, host_ctxt);
	cpu_reg(host_ctxt, 0) = x0;
}

static bool exynos_hyp_smc_trap_handler(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(unsigned int, id, host_ctxt, 0);
	DECLARE_REG(unsigned long, x1, host_ctxt, 1);
	DECLARE_REG(unsigned long, x2, host_ctxt, 2);
	DECLARE_REG(unsigned long, x3, host_ctxt, 3);
	DECLARE_REG(unsigned long, x4, host_ctxt, 4);
	unsigned long x0;

	if (id != HVC_DRM_TZMP2_PROT && id != HVC_DRM_TZMP2_UNPROT)
		return false;

	if (exynos_hyp_mod_hvc_handler == NULL)
		return false;

	x0 = exynos_hyp_mod_hvc_handler(id, x1, x2, x3, x4, NULL, host_ctxt);
	cpu_reg(host_ctxt, 0) = x0;

	return true;
}

static int exynos_hyp_perm_fault_handler(struct kvm_cpu_context *host_ctxt,
					 unsigned long long esr,
					 unsigned long long addr)
{
	unsigned long elr_el2 = read_sysreg_el2(SYS_ELR);
	unsigned long sp_el1 = host_ctxt->sys_regs[SP_EL1];

	return exynos_hyp_mod_sync_handler(esr, elr_el2, sp_el1,
					   host_ctxt->regs.regs, NULL);
}

static void exynos_hyp_abt_notifier(struct kvm_cpu_context *host_ctxt)
{
	unsigned long esr_el2 = read_sysreg_el2(SYS_ESR);
	unsigned long elr_el2 = read_sysreg_el2(SYS_ELR);
	unsigned long sp_el1 = host_ctxt->sys_regs[SP_EL1];

	exynos_hyp_mod_sync_handler(esr_el2, elr_el2, sp_el1,
				    host_ctxt->regs.regs, NULL);
}

static bool exynos_hyp_trap_handler(struct kvm_cpu_context *host_ctxt)
{
	unsigned long esr_el2 = read_sysreg_el2(SYS_ESR);
	unsigned long elr_el2 = read_sysreg_el2(SYS_ELR);
	unsigned long sp_el1 = host_ctxt->sys_regs[SP_EL1];

	exynos_hyp_mod_sync_handler(esr_el2, elr_el2, sp_el1,
				    host_ctxt->regs.regs, NULL);

	return true;
}

static void exynos_hyp_psci_handler(enum pkvm_psci_notification psci_type,
				    struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(unsigned long, power_state, host_ctxt, 1);

	if (psci_type != PKVM_PSCI_CPU_SUSPEND)
		power_state = 0;

	exynos_hyp_mod_psci_handler(psci_type, power_state);
}

static int exynos_hyp_module_relocate(const struct pkvm_module_ops *ops,
				      unsigned long module_base,
				      unsigned long module_size,
				      unsigned long got_start,
				      unsigned long got_size,
				      unsigned long rela_start,
				      unsigned long rela_size,
				      unsigned long va_pa_offset)
{
	unsigned long mod_va, mod_off, got_st, got_end, rela_st, rela_end;
	unsigned long addr, rconst;
	unsigned long *raddr;

	Elf64_Rela *mod_rela;

	mod_va = (unsigned long)ops->linear_map_early(module_base, module_size, PAGE_HYP);
	if (!mod_va)
		return -ENOMEM;

	got_st = mod_va + (got_start - module_base);
	got_end = got_st + got_size;
	rela_st = mod_va + (rela_start - module_base);
	rela_end = rela_st + rela_size;

	/* Fixup Global Descriptor Table (GDT) */
	for (addr = got_st; addr < got_end; addr += sizeof(unsigned long)) {
		unsigned long *ptr;
		unsigned long val;

		ptr = (unsigned long *)addr;
		val = *ptr;

		if (val >= module_base && val < module_base + module_size)
			*ptr = val + va_pa_offset;
	}

	/* Fixup dynamic relocations */
	mod_rela = (Elf64_Rela *)rela_st;
	mod_off = mod_va - module_base;

	for (mod_rela = (Elf64_Rela *)rela_st;
	     mod_rela < (Elf64_Rela *)rela_end;
	     mod_rela++) {
		raddr = (unsigned long *)(mod_rela->r_offset + mod_off);
		rconst = mod_rela->r_addend;

		if (rconst >= module_base && rconst < module_base + module_size)
			*raddr = rconst + va_pa_offset;
	}

	ops->linear_unmap_early((void *)mod_va, module_size);

	return 0;
}

static int exynos_hyp_plugin_map_sections(const struct pkvm_module_ops *ops,
					  struct plugin_memory_info *plugin_mem)
{
	struct exynos_hyp_mod_sec *sec;
	unsigned long long txt_st, txt_sz, ro_st, ro_sz, rw_st, rw_sz;
	unsigned long long mod_st, mod_sz;
	void *mod_va, *va;
	size_t offset;
	u64 pfn;
	int ret;

	if (plugin_mem == NULL)
		return -EINVAL;

	sec = &plugin_mem->section;

	txt_st = sec->text_start;
	txt_sz = sec->text_end - txt_st;
	ro_st = sec->rodata_start;
	ro_sz = sec->rodata_end - ro_st;
	rw_st = sec->rwdata_start;
	rw_sz = sec->rwdata_end - rw_st;

	mod_st = txt_st;
	mod_sz = txt_sz + ro_sz + rw_sz;

	mod_va = ops->alloc_module_va(PAGE_ALIGN(mod_sz) >> PAGE_SHIFT);
	if (!mod_va)
		return -ENOMEM;

	va = mod_va;

	/* Do mapping for each section of the plug-in binary module */
	for (offset = 0; offset < txt_sz; offset += PAGE_SIZE) {
		pfn = (txt_st + offset) >> PAGE_SHIFT;
		ret = ops->map_module_page(pfn, va + offset, PAGE_HYP_EXEC, true);
		if (ret)
			/* TODO: Need to unmap */
			return ret;
	}

	va += txt_sz;

	for (offset = 0; offset < ro_sz; offset += PAGE_SIZE) {
		pfn = (ro_st + offset) >> PAGE_SHIFT;
		ret = ops->map_module_page(pfn, va + offset, PAGE_HYP_RO, true);
		if (ret)
			/* TODO: Need to unmap */
			return ret;
	}

	va += ro_sz;

	for (offset = 0; offset < rw_sz; offset += PAGE_SIZE) {
		pfn = (rw_st + offset) >> PAGE_SHIFT;
		ret = ops->map_module_page(pfn, va + offset, PAGE_HYP, true);
		if (ret)
			/* TODO: Need to unmap */
			return ret;
	}

	plugin_mem->base_va = (unsigned long)mod_va;
	plugin_mem->base = (unsigned long)mod_st;
	plugin_mem->size = mod_sz;
	plugin_mem->offset = (unsigned long)mod_va - mod_st;

	return 0;
}

static int exynos_hyp_plugin_init(const struct pkvm_module_ops *ops,
				  struct exynos_hyp_interface *hyp_interface)
{
	struct exynos_plugin_info *info = hyp_interface->plugin_module;
	struct plugin_module_info *mod;
	struct plugin_memory_info *mem;
	struct exynos_hyp_mod_sec *sec;
	unsigned int i;
	int ret = 0;

	if (info == NULL)
		return -EINVAL;

	if (info->modules == NULL)
		return -EINVAL;

	for (i = 0; i < info->plugin_num; i++) {
		mod = &info->modules[i];
		mem = &mod->mem;

		ret = exynos_hyp_plugin_map_sections(ops, mem);
		if (ret)
			break;

		sec = &mem->section;

		if (sec == NULL)
			return -EINVAL;

		ret = exynos_hyp_module_relocate(ops, mem->base, mem->size,
						 sec->got_start,
						 sec->got_end - sec->got_start,
						 sec->rela_start,
						 sec->rela_end - sec->rela_start,
						 mem->offset);
		if (ret)
			break;

		mod->plugin_runtime_entry += mem->offset;

		ret = hyp_interface->plugin_runtime_init(mod->plugin_runtime_entry,
							 mem->offset, mem->base,
							 mem->size);
		if (ret)
			break;
	}

	return ret;
}

static int exynos_hyp_module_init(const struct pkvm_module_ops *ops)
{
	struct exynos_hyp_header *hdr;
	unsigned long long txt_st, txt_sz, ro_st, ro_sz, rw_st, rw_sz;
	unsigned long long got_st, got_sz, rela_st, rela_sz, mod_st, mod_sz;
	unsigned long ep;
	void *mod_va, *va;
	size_t offset;
	u64 pfn;
	int ret;

	if (!hyp_base || !hyp_size || !ops)
		return -EINVAL;

	hdr = (struct exynos_hyp_header *)ops->fixmap_map(hyp_base);

	txt_st = hdr->sec_info.text_start;
	txt_sz = hdr->sec_info.text_end - txt_st;
	ro_st = hdr->sec_info.rodata_start;
	ro_sz = hdr->sec_info.rodata_end - ro_st;
	rw_st = hdr->sec_info.rwdata_start;
	rw_sz = hdr->sec_info.rwdata_end - rw_st;
	got_st = hdr->sec_info.got_start;
	got_sz = hdr->sec_info.got_end - got_st;
	rela_st = hdr->sec_info.rela_start;
	rela_sz = hdr->sec_info.rela_end - rela_st;
	ep = hdr->exynos_hyp_entrypoint;

	ops->fixmap_unmap();

	mod_st = txt_st;
	mod_sz = txt_sz + ro_sz + rw_sz;

	mod_va = ops->alloc_module_va(PAGE_ALIGN(mod_sz) >> PAGE_SHIFT);
	if (!mod_va)
		return -ENOMEM;

	va = mod_va;

	/* Do mapping for each section of the binary module */
	for (offset = 0; offset < txt_sz; offset += PAGE_SIZE) {
		pfn = (txt_st + offset) >> PAGE_SHIFT;
		ret = ops->map_module_page(pfn, va + offset, PAGE_HYP_EXEC, true);
		if (ret)
			/* TODO: Need to unmap */
			return ret;
	}

	va += txt_sz;

	for (offset = 0; offset < ro_sz; offset += PAGE_SIZE) {
		pfn = (ro_st + offset) >> PAGE_SHIFT;
		ret = ops->map_module_page(pfn, va + offset, PAGE_HYP_RO, true);
		if (ret)
			/* TODO: Need to unmap */
			return ret;
	}

	va += ro_sz;

	for (offset = 0; offset < rw_sz; offset += PAGE_SIZE) {
		pfn = (rw_st + offset) >> PAGE_SHIFT;
		ret = ops->map_module_page(pfn, va + offset, PAGE_HYP, true);
		if (ret)
			/* TODO: Need to unmap */
			return ret;
	}

	hyp_offset = (unsigned long long)mod_va - mod_st;
	hyp_entry = (hyp_entry_t)(ep + hyp_offset);

	return exynos_hyp_module_relocate(ops, mod_st, mod_sz,
					  got_st, got_sz, rela_st,
					  rela_sz, hyp_offset);

}

static void exynos_hyp_init_module_ops(const struct pkvm_module_ops *ops)
{
	exynos_mod_ops.create_private_mapping = ops->create_private_mapping;
	exynos_mod_ops.fixmap_map = ops->fixmap_map;
	exynos_mod_ops.fixmap_unmap = ops->fixmap_unmap;
	exynos_mod_ops.flush_dcache_to_poc = ops->flush_dcache_to_poc;
	exynos_mod_ops.host_stage2_mod_prot = ops->host_stage2_mod_prot;
	exynos_mod_ops.host_stage2_get_leaf = ops->host_stage2_get_leaf;
	exynos_mod_ops.host_donate_hyp = ops->host_donate_hyp;
	exynos_mod_ops.hyp_donate_host = ops->hyp_donate_host;
	exynos_mod_ops.hyp_pa = ops->hyp_pa;
	exynos_mod_ops.hyp_va = ops->hyp_va;
	exynos_mod_ops.host_share_hyp = ops->host_share_hyp;
	exynos_mod_ops.host_unshare_hyp = ops->host_unshare_hyp;
	exynos_mod_ops.pin_shared_mem = ops->pin_shared_mem;
	exynos_mod_ops.unpin_shared_mem = ops->unpin_shared_mem;
	exynos_mod_ops.update_hcr_el2 = ops->update_hcr_el2;
	exynos_mod_ops.update_hfgwtr_el2 = ops->update_hfgwtr_el2;
}

int exynos_hyp_init(const struct pkvm_module_ops *ops)
{
	int ret;
	struct exynos_hyp_interface *hyp_intfc;

	if (!ops)
		return -EINVAL;

	ret = exynos_hyp_module_init(ops);
	if (ret)
		return ret;

	exynos_hyp_init_module_ops(ops);

	hyp_intfc = (struct exynos_hyp_interface *)hyp_entry(
				&exynos_mod_ops, hyp_offset, hyp_kimg_offset, 0);
	if (hyp_intfc == NULL)
		return -EINVAL;

	exynos_hyp_mod_hvc_handler = hyp_intfc->hvc_handler;
	if (exynos_hyp_mod_hvc_handler) {
		ret = ops->register_host_smc_handler(exynos_hyp_smc_trap_handler);
		if (ret)
			return ret;
	}

	if (IS_ENABLED(CONFIG_EXYNOS_HYP_MODULE)) {
		ret = exynos_hyp_s2mpu_driver_init(ops, hyp_intfc);
		if (ret)
			return ret;
	}

	exynos_hyp_mod_sync_handler = hyp_intfc->sync_handler;
	if (exynos_hyp_mod_sync_handler) {
		ret = ops->register_host_perm_fault_handler(
				exynos_hyp_perm_fault_handler);
		if (ret)
			return ret;

		ret = ops->register_illegal_abt_notifier(exynos_hyp_abt_notifier);
		if (ret)
			return ret;

		ret = ops->register_hyp_panic_notifier(exynos_hyp_abt_notifier);
		if (ret)
			return ret;

		ret = ops->register_default_trap_handler(exynos_hyp_trap_handler);
		if (ret)
			return ret;
	}

	exynos_hyp_mod_psci_handler = hyp_intfc->psci_handler;
	if (exynos_hyp_mod_psci_handler) {
		ret = ops->register_psci_notifier(exynos_hyp_psci_handler);
		if (ret)
			return ret;
	}

	return exynos_hyp_plugin_init(ops, hyp_intfc);
}
