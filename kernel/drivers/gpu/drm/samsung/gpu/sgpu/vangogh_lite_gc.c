/*
 *
 *  Copyright (C) 2020 - 2021 Advanced Micro Devices, Inc.  All rights reserved.
 *
 */

#include "soc15_common.h"

#include "gc/gc_10_4_0_offset.h"
#include "gc/gc_10_4_0_sh_mask.h"
#include "amdgpu.h"
#include "vangogh_lite_reg.h"

void vangogh_lite_gc_update_median_grain_clock_gating(struct amdgpu_device *adev,
						      bool enable)
{
	uint32_t data, def;

	if (!(adev->cg_flags & (AMD_CG_SUPPORT_GFX_MGCG | AMD_CG_SUPPORT_GFX_MGLS)))
		return;

	/* It is disabled by HW by default */
	if (enable && (adev->cg_flags & AMD_CG_SUPPORT_GFX_MGCG)) {
		/* 1 - RLC_CGTT_MGCG_OVERRIDE */
		def = data = RREG32_SOC15(GC, 0, mmRLC_CGTT_MGCG_OVERRIDE);
		data &= ~(RLC_CGTT_MGCG_OVERRIDE__GRBM_CGTT_SCLK_OVERRIDE_MASK |
			  RLC_CGTT_MGCG_OVERRIDE__GFXIP_MGCG_OVERRIDE_MASK |
			  RLC_CGTT_MGCG_OVERRIDE__RLC_CGTT_SCLK_OVERRIDE_MASK |
			  RLC_CGTT_MGCG_OVERRIDE__ENABLE_CGTS_LEGACY_MASK);

		/* FGCG */
		data &= ~(RLC_CGTT_MGCG_OVERRIDE__GFXIP_FGCG_OVERRIDE_MASK |
			  RLC_CGTT_MGCG_OVERRIDE__GFXIP_REP_FGCG_OVERRIDE_MASK);

		if (def != data)
			WREG32_SOC15(GC, 0, mmRLC_CGTT_MGCG_OVERRIDE, data);
	} else {
		/* 1 - MGCG_OVERRIDE */
		def = data = RREG32_SOC15(GC, 0, mmRLC_CGTT_MGCG_OVERRIDE);
		data |= (RLC_CGTT_MGCG_OVERRIDE__RLC_CGTT_SCLK_OVERRIDE_MASK |
			 RLC_CGTT_MGCG_OVERRIDE__GRBM_CGTT_SCLK_OVERRIDE_MASK |
			 RLC_CGTT_MGCG_OVERRIDE__GFXIP_MGCG_OVERRIDE_MASK |
			 RLC_CGTT_MGCG_OVERRIDE__ENABLE_CGTS_LEGACY_MASK |
			 RLC_CGTT_MGCG_OVERRIDE__GFXIP_FGCG_OVERRIDE_MASK |
			 RLC_CGTT_MGCG_OVERRIDE__GFXIP_REP_FGCG_OVERRIDE_MASK);
		if (def != data)
			WREG32_SOC15(GC, 0, mmRLC_CGTT_MGCG_OVERRIDE, data);
	}
}

void vangogh_lite_gc_clock_gating_workaround(struct amdgpu_device *adev)
{
	uint32_t data, def;

	/* Synopsys: Read return data can be dropped in tcp_mem
	 * due to the clk enable going false.  This results in
	 * head of line blocking in the return data path because
	 * tcp_ofifo is waiting for the lost data.
	 *
	 * Symptoms: TCP will lock up when the LFIFO becomes full.
	 *
	 * Workaround: Set CGTT_TCPI_CLK_CTRL_0[17] to 1.  This
	 * will override the clock gating for tcp_mem so that
	 * it's clock is always on.
	 */
	if (adev->cg_flags &
	    (AMD_CG_SUPPORT_GFX_CGCG | AMD_CG_SUPPORT_GFX_MGCG |
	     AMD_CG_SUPPORT_GFX_3D_CGCG)) {
		def = data = RREG32_SOC15(GC, 0, mmCGTT_TCPI_CLK_CTRL_0);
		data |= CGTT_TCPI_CLK_CTRL_0__SOFT_OVERRIDE_17_MASK;
		if (def != data)
			WREG32_SOC15(GC, 0, mmCGTT_TCPI_CLK_CTRL_0, data);
	}
}

void vangogh_lite_gc_perfcounter_cg_workaround(struct amdgpu_device *adev,
					        bool enable)
{
	uint32_t data, def;

	/* Synopsys: Four stall flops are on mgcg clock domain
	 * and are used for the respective stall perfcounter
	 * computation.  Under certain specific timing/pipeline
	 * scenarios when the clocks re off, the flops are
	 * errorneous and hence affect the perfcounter values.
	 *
	 * Symptoms: The perfcounters using the above stall
	 * signals can be wrong.  These perfcounter are not KPI
	 * but are used for debug purposes.
	 *
	 * Workaround: Driver to program
	 * CGTT_GL1C_4CYCLE_CLK_CTRL.SOFT_OVERRIDE8 to 1 to
	 * override the MGCG clock domain so that the clocks are
	 * on to get the correct perfcounter values.  Note: It is  This
	 * only suggested to override the Clocks only when used in
	 * perfcounter capture and analysis.
	 */
	if (adev->cg_flags & AMD_CG_SUPPORT_GFX_MGCG) {
		def = data = RREG32_SOC15(GC, 0, mmCGTT_GL1C_4CYCLE_CLK_CTRL);
		if (enable)
			data |= CGTT_GL1C_4CYCLE_CLK_CTRL__SOFT_OVERRIDE8_MASK;
		else
			data &= ~CGTT_GL1C_4CYCLE_CLK_CTRL__SOFT_OVERRIDE8_MASK;
		if (def != data)
			WREG32_SOC15(GC, 0, mmCGTT_GL1C_4CYCLE_CLK_CTRL, data);
	}
}

void vangogh_lite_gc_set_sysregs(struct amdgpu_device *adev)
{
#ifdef CONFIG_DRM_SGPU_EXYNOS
	void *addr = adev->pm.sysreg_mmio;

	/* Enable MGCG */
	writel(0x6, addr + 0x8);
#endif
}

#if defined(CONFIG_DRM_SGPU_EXYNOS) && defined(CONFIG_DRM_SGPU_EMULATOR_WORKAROUND)
/* Request GPU Power up when runtime pm is disabled */
void vangogh_lite_gc_gpu_power_up(struct amdgpu_device *adev)
{
	uint32_t gpu_status = 0;

	writel(BG3D_PWRCTL_CTL2_GPUPWRREQ, adev->pm.pmu_mmio + BG3D_PWRCTL_CTL2_OFFSET);
	do {
		gpu_status = readl(adev->pm.pmu_mmio + BG3D_PWRCTL_STATUS_OFFSET);
	} while ((gpu_status & BG3D_PWRCTL_STATUS_GPUPWRACK_MASK) == 0);
}
#else
void vangogh_lite_gc_gpu_power_up(struct amdgpu_device *adev)
{
	return;
}
#endif
