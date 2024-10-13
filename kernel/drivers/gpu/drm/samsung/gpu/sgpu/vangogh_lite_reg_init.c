/*
 *Copyright (C) 2019-2021 Advanced Micro Devices, Inc.
 */
#include "amdgpu.h"
#include "nv.h"

#include "soc15_common.h"
#include "soc15_hw_ip.h"

#include "vangogh_lite_ip_offset.h"
#include "gc/gc_10_4_0_offset.h"
#include "gc/gc_10_4_0_sh_mask.h"
#include "vangogh_lite_reg.h"

#include <linux/delay.h>
#ifdef CONFIG_DRM_SGPU_EXYNOS
#include <soc/samsung/exynos-smc.h>
#endif

/* TODO, remove some IP's definitions */
int vangogh_lite_reg_base_init(struct amdgpu_device *adev)
{
	u32 i;

	for (i = 0 ; i < MAX_INSTANCE ; ++i) {
		adev->reg_offset[GC_HWIP][i] = (u32 *)(&GC_BASE.instance[i]);
		adev->reg_offset[NBIO_HWIP][i] =
				(u32 *)(&NBIO_BASE.instance[i]);
	}

	return 0;
}

/* TODO: GFXSW-3317 This code will be removed at silicon */
void vangogh_lite_gpu_hard_reset(struct amdgpu_device *adev)
{
#ifdef CONFIG_DRM_SGPU_EXYNOS
	uint32_t ret = 0;
	uint32_t val = 0x0;
	uint32_t addr = (uint32_t)adev->rmmio_base + RSMU_HARD_RESETB_GC_OFFSET;

	ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(addr), val, 0);
	if (ret)
		pr_err("writing 0x%x to 0x%x is fail\n", val, addr);

	udelay(SMC_SAFE_WAIT_US);
	val = 0x1;

	ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(addr), val, 0);
	if (ret)
		pr_err("writing 0x%x to 0x%x is fail\n", val, addr);
#else
	WREG32_SOC15(GC, 0, mmRSMU_HARD_RESETB_GC, 0x0);
	msleep(1);
	WREG32_SOC15(GC, 0, mmRSMU_HARD_RESETB_GC, 0x1);
#endif /* CONFIG_DRM_SGPU_EXYNOS */
}

void vangogh_lite_gpu_soft_reset(struct amdgpu_device *adev)
{
	WREG32_SOC15(GC, 0, mmRLC_RLCS_GRBM_SOFT_RESET, 0x1);
}


#ifdef CONFIG_DRM_SGPU_EXYNOS
#if IS_ENABLED(CONFIG_SOC_S5E9945) && !IS_ENABLED(CONFIG_SOC_S5E9945_EVT0)
static uint32_t read_gl2acem_rst(struct amdgpu_device *adev)
{
	return readl(((void __iomem *)adev->rmmio) + GL2ACEM_RST_OFFSET);
}
static void write_gl2acem_rst(struct amdgpu_device *adev, uint32_t val)
{
	return writel(val, ((void __iomem *)adev->rmmio) + GL2ACEM_RST_OFFSET);
}
#else
static uint32_t read_gl2acem_rst(struct amdgpu_device *adev)
{
	unsigned long ret, val;

	ret = exynos_smc_readsfr(adev->rmmio_base + GL2ACEM_RST_OFFSET, &val);
	if (ret)
		pr_err("reading ADDR %#llx is fail\n",
			adev->rmmio_base + GL2ACEM_RST_OFFSET);

	return (uint32_t)val;
}
static void write_gl2acem_rst(struct amdgpu_device *adev, uint32_t val)
{
	unsigned long ret;

	ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(
			adev->rmmio_base + GL2ACEM_RST_OFFSET), val, 0);
	if (ret)
		pr_err("writing ADDR %#llx is fail\n",
			adev->rmmio_base + GL2ACEM_RST_OFFSET);
}
#endif /* CONFIG_SOC_S5E9945 && !CONFIG_SOC_S5E9945_EVT0 */
#else
static uint32_t read_gl2acem_rst(struct amdgpu_device *adev)
{
	return RREG32_SOC15(GC, 0, mmGL2ACEM_RST);
}
static void write_gl2acem_rst(struct amdgpu_device *adev, uint32_t val)
{
	WREG32_SOC15(GC, 0, mmGL2ACEM_RST, val);
}
#endif /* CONFIG_DRM_SGPU_EXYNOS */

void vangogh_lite_gpu_quiesce(struct amdgpu_device *adev)
{
	uint32_t i;
	unsigned long val = (0x1 << GL2ACEM_RST__SRT_ST__SHIFT);
	unsigned long timeout = 0;
	bool quiescent = false;

	val = read_gl2acem_rst(adev);
	val |= (0x1 << GL2ACEM_RST__SRT_ST__SHIFT);
	write_gl2acem_rst(adev, val);

	timeout = jiffies + usecs_to_jiffies(QUIESCE_TIMEOUT_WAIT_US);

	do {
		quiescent = true;
		for (i = 0; i < adev->gl2acem_instances_count; i++) {
			WREG32_SOC15(GC, 0, mmGRBM_GFX_INDEX, i);

			val = read_gl2acem_rst(adev);

			quiescent &= val & GL2ACEM_RST__SRT_CP_MASK;
			udelay(SMC_SAFE_WAIT_US);

			if (!time_before(jiffies, timeout)) {
				DRM_ERROR("HW defect detected : failed to quiesce gpu\n");
				sgpu_debug_snapshot_expire_watchdog();
				return;
			}
		}
	} while (!quiescent);
}

#if IS_ENABLED(CONFIG_SOC_S5E9945) && !IS_ENABLED(CONFIG_SOC_S5E9945_EVT0)
void vangogh_lite_didt_edc_init(struct amdgpu_device *adev)
{
	adev->didt_enable = false;
	adev->edc_enable = false;

	WREG32_SOC15(GC, 0, mmGC_CAC_CTRL_2, 0x00000008);
	WREG32_SOC15(GC, 0, mmDIDT_INDEX_AUTO_INCR_EN, 0x00000001);
	WREG32_DIDT(DIDT_SQ_WEIGHT0_3, 0x64ff4a60);
	WREG32_DIDT(DIDT_SQ_WEIGHT4_7, 0x0000002a);

	/* DIDT only */
	WREG32_DIDT(DIDT_SQ_STALL_CTRL, 0x00104210);
	WREG32_DIDT(DIDT_SQ_TUNING_CTRL, 0x013d8716);
	WREG32_DIDT(DIDT_SQ_CTRL1, 0x8dc00000);
	WREG32_DIDT(DIDT_SQ_CTRL2, 0x30200716);

	/* EDC only */
	WREG32_DIDT(DIDT_SQ_EDC_THRESHOLD, 0x00000104);
	WREG32_DIDT(DIDT_SQ_EDC_STALL_PATTERN_1_2, 0x01010001);
	WREG32_DIDT(DIDT_SQ_EDC_STALL_PATTERN_3_4, 0x11110421);
	WREG32_DIDT(DIDT_SQ_EDC_STALL_PATTERN_5_6, 0x25291249);
	WREG32_DIDT(DIDT_SQ_EDC_STALL_PATTERN_7, 0x00002aaa);
}
#endif

void vangogh_lite_didt_enable(struct amdgpu_device *adev)
{
	adev->didt_enable = true;
	WREG32_DIDT(DIDT_SQ_CTRL0, 0x22550c61);

	SGPU_LOG(adev, DMSG_INFO, DMSG_DVFS, "didt enable 0x%08x\n",
		RREG32_DIDT(DIDT_SQ_CTRL0));
}

void vangogh_lite_didt_disable(struct amdgpu_device *adev)
{
	adev->didt_enable = false;
	WREG32_DIDT(DIDT_SQ_CTRL0, 0x22550c40);

	SGPU_LOG(adev, DMSG_INFO, DMSG_DVFS, "didt disable 0x%08x\n",
		RREG32_DIDT(DIDT_SQ_CTRL0));
}

void vangogh_lite_edc_enable(struct amdgpu_device *adev)
{
	adev->edc_enable = true;
	WREG32_DIDT(DIDT_SQ_EDC_CTRL, 0x00401c71);

	SGPU_LOG(adev, DMSG_INFO, DMSG_DVFS, "edc enable 0x%08x\n",
		RREG32_DIDT(DIDT_SQ_EDC_CTRL));
}

void vangogh_lite_edc_disable(struct amdgpu_device *adev)
{
	adev->edc_enable = false;
	WREG32_DIDT(DIDT_SQ_EDC_CTRL, 0x00401c70);

	SGPU_LOG(adev, DMSG_INFO, DMSG_DVFS, "edc disable 0x%08x\n",
		RREG32_DIDT(DIDT_SQ_EDC_CTRL));
}
