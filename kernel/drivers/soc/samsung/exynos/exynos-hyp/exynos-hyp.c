/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - EL2 Hypervisor module loader
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/arm-smccc.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/smp.h>

#include <asm/kvm_asm.h>
#include <asm/kvm_pkvm_module.h>

#include <soc/samsung/exynos/exynos-hvc.h>
#include <soc/samsung/exynos/exynos-hyp.h>


int __kvm_nvhe_exynos_hyp_init(const struct pkvm_module_ops *ops);
void __kvm_nvhe_exynos_hyp_hvc_handler(struct kvm_cpu_context *host_ctxt);

extern unsigned long long __kvm_nvhe_hyp_base;
extern unsigned long long __kvm_nvhe_hyp_size;
extern unsigned long long __kvm_nvhe_hyp_kimg_offset;
extern struct pkvm_iommu_driver __kvm_nvhe_exynos_hyp_s2mpu_driver;

static unsigned long exynos_hyp_fid;
static bool exynos_hyp_mod_enabled;
static unsigned long long exynos_hyp_driver_addr;


int exynos_pkvm_s2mpu_register(struct device *dev, unsigned long pa)
{
	if (!IS_ENABLED(CONFIG_EXYNOS_HYP_S2MPU))
		return -ENODEV;

	if (!exynos_hyp_driver_addr)
		return -EOPNOTSUPP;

	return pkvm_iommu_register(dev, exynos_hyp_driver_addr,
				   pa, SZ_64K, NULL, 0);
}
EXPORT_SYMBOL(exynos_pkvm_s2mpu_register);

unsigned long exynos_hvc(unsigned long hvc_fid,
			 unsigned long arg1,
			 unsigned long arg2,
			 unsigned long arg3,
			 unsigned long arg4)
{
	struct arm_smccc_res res;

	if (exynos_hyp_mod_enabled)
		arm_smccc_hvc(KVM_HOST_SMCCC_ID(exynos_hyp_fid),
				hvc_fid, arg1, arg2, arg3, arg4, 0, 0, &res);
	else
		arm_smccc_hvc(hvc_fid, arg1, arg2, arg3, arg4, 0, 0, 0, &res);

	return res.a0;
};
EXPORT_SYMBOL(exynos_hvc);

static void __exynos_hyp_update_fg_write_trap(void *hfgwtr_mask)
{
	struct sysreg_mask *mask = (struct sysreg_mask *)hfgwtr_mask;
	unsigned long ret;

	ret = exynos_hvc(HVC_FID_PKVM_UPDATE_FG_WRITE_TRAP,
			 mask->set_mask, mask->clear_mask,
			 0, 0);
	if (ret)
		pr_err("%s: Fine-Grained write trap update failure (%#lx)\n",
				__func__, ret);
}

int exynos_hyp_update_fg_write_trap(unsigned long sysreg_bit_pos,
				    bool trap)
{
	struct sysreg_mask mask;

	if (sysreg_bit_pos >= 64) {
		pr_err("%s: Invalid sysreg_bit_pos (%ld)\n",
				__func__, sysreg_bit_pos);
		return -EINVAL;
	}

	if (trap) {
		mask.set_mask = BIT(sysreg_bit_pos);
		mask.clear_mask = 0;
	} else {
		mask.set_mask = 0;
		mask.clear_mask = BIT(sysreg_bit_pos);
	}

	on_each_cpu(__exynos_hyp_update_fg_write_trap, (void *)&mask, 1);

	return 0;

}
EXPORT_SYMBOL(exynos_hyp_update_fg_write_trap);

static void exynos_hyp_update_hcr_el2(void *discard)
{
	unsigned long ret;

	ret = exynos_hvc(HVC_FID_PKVM_UPDATE_HCR, 0, 0, 0, 0);
	if (ret)
		pr_err("%s: HCR_EL2 update failure (%#lx)\n",
				__func__, ret);
}

static void exynos_hyp_set_cpu_context(void *discard)
{
	unsigned long ret;

	ret = exynos_hvc(HVC_FID_PKVM_SET_CPU_CONTEXT, 0, 0, 0, 0);
	if (ret)
		pr_err("%s: CPU context update failure (%#lx)\n",
				__func__, ret);
}

static int __init exynos_hyp_module_init(void)
{
	struct device_node *np;
	struct device_node *mem_np;
	struct resource res;
	unsigned long token;
	unsigned long long symbol;
	int ret;

	np = of_find_compatible_node(NULL, NULL, "samsung,exynos-hyp");
	if (np == NULL) {
		pr_err("%s: Fail to find Exynos Hypervisor node\n", __func__);
		return -ENODEV;
	}

	mem_np = of_parse_phandle(np, "memory_region", 0);
	if (mem_np == NULL) {
		pr_err("%s: Fail to find harx_binary node\n", __func__);
		return -ENODEV;
	}

	ret = of_address_to_resource(mem_np, 0, &res);
	if (ret) {
		pr_err("%s: Fail to get binary base and size\n", __func__);;
		return ret;
	}

	__kvm_nvhe_hyp_base = res.start;
	__kvm_nvhe_hyp_size = res.end - res.start;

	/* Calculate kimg_offset with any symbol */
	symbol = (unsigned long long)exynos_hvc;
	__kvm_nvhe_hyp_kimg_offset = __phys_to_kimg(symbol) - symbol - kaslr_offset();

	ret = pkvm_load_el2_module(__kvm_nvhe_exynos_hyp_init, &token);
	if (ret && ret != -EOPNOTSUPP) {
		pr_err("%s: Exynos hypervisor module init fail ret[%d]\n",
				__func__, ret);
		return ret;
	}

	if (ret == -EOPNOTSUPP) {
		pr_info("%s: Hypervisor module disabled, pKVM not enabled\n",
				__func__);
		return 0;
	}

	exynos_hyp_mod_enabled = true;

	ret = pkvm_register_el2_mod_call(__kvm_nvhe_exynos_hyp_hvc_handler, token);
	if (ret < 0) {
		pr_err("%s: Module HVC handler registration fail ret[%d]\n",
				__func__, ret);
		return ret;
	} else {
		exynos_hyp_fid = (unsigned long)ret;
	}

	/* Update HCR_EL2 bits which plug-in set at bootloader stage */
	on_each_cpu(exynos_hyp_update_hcr_el2, NULL, 1);

	on_each_cpu(exynos_hyp_set_cpu_context, NULL, 1);


	pr_notice("%s: Exynos hypervisor module init done\n", __func__);

	if (IS_ENABLED(CONFIG_EXYNOS_HYP_S2MPU)) {
		exynos_hyp_driver_addr =
			pkvm_el2_mod_va(&__kvm_nvhe_exynos_hyp_s2mpu_driver,
					token);

		ret = pkvm_iommu_driver_init(exynos_hyp_driver_addr, NULL, 0);
		if (ret) {
			pr_err("%s: Exynos hypervisor S2MPU driver init fail ret[%d]\n",
					__func__, ret);

			exynos_hyp_driver_addr = 0;

			return ret;
		}

		pr_notice("%s: Exynos hypervisor S2MPU driver init done\n", __func__);
	}

	return 0;
}

module_init(exynos_hyp_module_init);

MODULE_DESCRIPTION("Exynos Hypervisor module driver");
MODULE_LICENSE("GPL");
