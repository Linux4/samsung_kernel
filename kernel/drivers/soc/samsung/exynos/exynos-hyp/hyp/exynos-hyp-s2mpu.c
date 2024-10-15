/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - EL2 Hypervisor module
 *
 */

#include <asm/alternative-macros.h>
#include <asm/kvm_pkvm_module.h>
#include <asm/page.h>

#include <nvhe/iommu.h>

#include <linux/kernel.h>
#include <linux/err.h>

#include <soc/samsung/exynos/exynos-hyp.h>

#define S2MPU_MMIO_SIZE				SZ_64K

enum mpt_prot {
	MPT_PROT_NONE	= 0,
	MPT_PROT_R	= BIT(0),
	MPT_PROT_W	= BIT(1),
	MPT_PROT_RW	= MPT_PROT_R | MPT_PROT_W,
	MPT_PROT_MASK	= MPT_PROT_RW,
};

static bool s2mpu_initialized;
static hyp_s2mpu_handler_t exynos_hyp_mod_s2mpu_handler;


static inline enum mpt_prot prot_to_mpt(enum kvm_pgtable_prot prot)
{
	return ((prot & KVM_PGTABLE_PROT_R) ? MPT_PROT_R : 0) |
	       ((prot & KVM_PGTABLE_PROT_W) ? MPT_PROT_W : 0);
}

/*
 * exynos_hyp_mod_s2mpu_handler is a function pointer of
 * vendor pKVM binary. It is delivered dutin binary initialization.
 * s2mpu_ops will be handled in vendor pKVM binary.
 */
static int exynos_hyp_s2mpu_init(void *data, size_t size)
{
	/*
	 * Notify pKVM S2MPU driver has been initialized.
	 */
	if (exynos_hyp_mod_s2mpu_handler == NULL)
		return -ENODEV;

	return 0;
}

static int exynos_hyp_s2mpu_validate(struct pkvm_iommu *dev)
{
	/*
	 * Request validation of S2MPU info(pa, size).
	 */
	if (dev->size != S2MPU_MMIO_SIZE)
		return -EINVAL;

	return 0;
}

static void exynos_hyp_s2mpu_host_stage2_idmap_prepare(phys_addr_t start,
						       phys_addr_t end,
						       enum kvm_pgtable_prot prot)
{
	/*
	 * Request to set the prot to the mpt of all S2MPUs.
	 * Some S2MPUs may be in suspend state.
	 * Therefore, it is only done for S2MPU with power on.
	 */
	enum mpt_prot s2mpu_prot = prot_to_mpt(prot);

	exynos_hyp_mod_s2mpu_handler(start, end, s2mpu_prot, 0, 0);
}

static void exynos_hyp_s2mpu_host_stage2_idmap_apply(struct pkvm_iommu *dev,
						     phys_addr_t start,
						     phys_addr_t end)
{
	/*
	 * Do not use 'apply' api, it will be included in prepare
	 */
}

static int exynos_hyp_s2mpu_suspend(struct pkvm_iommu *dev)
{
	/*
	 * Suspend and resume will be managed by HVC
	 */
	return 0;
}

static int exynos_hyp_s2mpu_resume(struct pkvm_iommu *dev)
{
	/*
	 * Suspend and resume will be managed by HVC
	 */
	return 0;
}

static bool exynos_hyp_s2mpu_host_dabt_handler(struct pkvm_iommu *dev,
					       struct kvm_cpu_context *host_ctxt,
					       u32 esr,
					       size_t off)
{
	//TODO: need to check abort is at S2MPU address or not
	return false;	// make abort at host
}

static struct pkvm_iommu_ops exynos_hyp_s2mpu_ops = {
	.init = exynos_hyp_s2mpu_init,
	.validate = exynos_hyp_s2mpu_validate,
	.validate_child = NULL,
	.host_stage2_idmap_prepare = exynos_hyp_s2mpu_host_stage2_idmap_prepare,
	.host_stage2_idmap_apply = exynos_hyp_s2mpu_host_stage2_idmap_apply,
	.suspend = exynos_hyp_s2mpu_suspend,
	.resume = exynos_hyp_s2mpu_resume,
	.host_dabt_handler = exynos_hyp_s2mpu_host_dabt_handler,
	.data_size = 0,		//TODO: check if s2mpu device need data or not
};

struct pkvm_iommu_driver exynos_hyp_s2mpu_driver = {
	.ops = &exynos_hyp_s2mpu_ops,
};

int exynos_hyp_s2mpu_driver_init(const struct pkvm_module_ops *ops,
				 struct exynos_hyp_interface *hyp_intfc)
{
	int ret = 0;

	if (s2mpu_initialized)
		return -EPERM;

	if (hyp_intfc != NULL)
		exynos_hyp_mod_s2mpu_handler = hyp_intfc->s2mpu_prepare;

	exynos_hyp_s2mpu_driver.ops = &exynos_hyp_s2mpu_ops;

	s2mpu_initialized = true;

	return ret;
}
