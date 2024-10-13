// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *               http://www.samsung.com
 */

#include <linux/pm_runtime.h>

#include <soc/samsung/cal-if.h>
#include "soc15_common.h"

#include "gc/gc_10_4_0_offset.h"
#include "gc/gc_10_4_0_sh_mask.h"
#include "amdgpu.h"
#include "amdgpu_trace.h"
#include "vangogh_lite_reg.h"
#include "sgpu_debug.h"

#ifdef CONFIG_DRM_SGPU_EXYNOS
#include <soc/samsung/exynos-smc.h>
#endif

uint32_t sgpu_ifpo_get_hw_lock_value(struct amdgpu_device *adev)
{
	return ((readl_relaxed(adev->pm.pmu_mmio + BG3D_PWRCTL_STATUS_OFFSET) &
			BG3D_PWRCTL_STATUS_LOCK_VALUE_MASK) >>
			BG3D_PWRCTL_STATUS_LOCK_VALUE__SHIFT);
}

/*
 * IFPO half automation APIs
 * GPU can go to IFPO power off state if HW counter gets 0. The driver should
 * specify periods which GPU must be powered on, by using lock/unlock function.
 */

static void sgpu_ifpo_half_enable(struct amdgpu_device *adev)
{
	void *addr = adev->pm.pmu_mmio;
	int32_t data = 0;

	/* Enable CP interrupts */
	data = RREG32_SOC15(GC, 0, mmCP_INT_CNTL);
	data |= 0x3c0000;					// CMP_BUSY|CNTX_BUSY|CNTX_EMPTY|GFX_IDLE
	WREG32_SOC15(GC, 0, mmCP_INT_CNTL, data);

	/* Enabling Save/Restore FSM */
	WREG32_SOC15(GC, 0, mmRLC_SRM_CNTL, 0x3);

	/* RLC Safe mode entry */
	WREG32_SOC15(GC, 0, mmRLC_SAFE_MODE, 0x3);
	do { } while (RREG32_SOC15(GC, 0, mmRLC_SAFE_MODE) != 0x2);

	/* Initialize PM/IFPO registers */
	WREG32_SOC15(GC, 0, mmRLC_GPM_TIMER_INT_3, 0x186A);	// GFXOFF timer – 250us
	WREG32_SOC15(GC, 0, mmRLC_PG_CNTL, 0x1);		// POWER_GATING_ENABLE

	/* Default IFPO enable setting */
	writel(0x89, addr + BG3D_PWRCTL_CTL2_OFFSET);		// COLDVSGFXOFF|FAXI_MODE|PMU_INTF_EN

	/* RLC Safe mode exit */
	WREG32_SOC15(GC, 0, mmRLC_SAFE_MODE, 0x1);
	do { } while (RREG32_SOC15(GC, 0, mmRLC_SAFE_MODE) != 0x0);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO half init");
}

static void sgpu_ifpo_half_reset(struct amdgpu_device *adev)
{
	writel_relaxed(0x12, adev->pm.pmu_mmio + BG3D_PWRCTL_CTL2_OFFSET);
}

static void sgpu_ifpo_half_lock(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	void *addr = adev->pm.pmu_mmio;
	uint32_t gpu_status = 0;
	bool gpu_wakeup = false;

	if (addr == NULL)
		return;

	trace_sgpu_ifpo_state_change_start(1);

	gpu_status = readl_relaxed(addr + BG3D_PWRCTL_LOCK_OFFSET);

	/* Check if GPU was already powered off or middle of powering down */
	if ((gpu_status & BG3D_PWRCTL_LOCK_GPU_READY_MASK) == 0) {
		/* Dummy write to wake up GPU and disable power gating */
		WREG32_SOC15(GC, 0, mmRLC_PG_CNTL, 0x0);
		/* Increase lock counter */
		readl_relaxed(addr + BG3D_PWRCTL_LOCK_OFFSET);
		/* Enable Power Gating */
		WREG32_SOC15(GC, 0, mmRLC_PG_CNTL, 0x1);

		gpu_wakeup = true;

		sgpu_devfreq_set_didt_edc(adev, 0);

		trace_sgpu_ifpo_power_on(1);
	}

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER,
		"IFPO LOCK   : hw(%u) sw(%d) usr(%u) rtm(%u) wakeup(%u)",
		sgpu_ifpo_get_hw_lock_value(adev),
		atomic_inc_return(&ifpo->count),
		ifpo->user_enable, ifpo->runtime_enable, gpu_wakeup);

	trace_sgpu_ifpo_state_change_end(1);
}

static void sgpu_ifpo_half_unlock(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	void *addr = adev->pm.pmu_mmio;
	uint32_t hw_counter, sw_counter;

	if (addr == NULL)
		return;

	trace_sgpu_ifpo_state_change_start(0);

	/* Decrease lock counter */
	writel_relaxed(0x0, addr + BG3D_PWRCTL_LOCK_OFFSET);

	hw_counter = sgpu_ifpo_get_hw_lock_value(adev);
	sw_counter = atomic_dec_return(&ifpo->count);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER,
		"IFPO UNLOCK : hw(%u) sw(%d) usr(%u) rtm(%u)",
		hw_counter, sw_counter,
		ifpo->user_enable, ifpo->runtime_enable);

	/* Note : it's supposed that sw/hw counter should get 0 at same time
	 * but values could be different by scheduling of multi-threads
	 */
	if (sw_counter == 0 && hw_counter != sw_counter)
		SGPU_LOG(adev, DMSG_INFO, DMSG_POWER,
				"count mismatch : hw(%u) sw(%d)",
				hw_counter, sw_counter);

	trace_sgpu_ifpo_power_off(atomic_read(&ifpo->count));
	trace_sgpu_ifpo_state_change_end(0);
}

static void sgpu_ifpo_half_suspend(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;

	/* GPU should be IFPO power on state before entering suspend */
	sgpu_ifpo_half_lock(adev);
	/* reset ColdGfxOff = 2'b00 before entering suspend */
	sgpu_ifpo_half_reset(adev);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER,
			"IFPO SUSPEND : hw(%u) sw(%d) usr(%u) rtm(%u)",
			sgpu_ifpo_get_hw_lock_value(adev),
			atomic_read(&ifpo->count),
			ifpo->user_enable, ifpo->runtime_enable);
}

static void sgpu_ifpo_half_resume(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	void *addr = adev->pm.pmu_mmio;

	/*
	* hw counter could be reset if GPU has been entered sleep mode,
	* it's needed to clear both counters
	*/
	atomic_set(&adev->ifpo.count, 0);
	writel_relaxed(~0, addr + BG3D_PWRCTL_LOCK_OFFSET);

	/* recover counters for sysfs knob */
	if (!adev->ifpo.user_enable)
		sgpu_ifpo_half_lock(adev);
	if (!adev->ifpo.runtime_enable)
		sgpu_ifpo_half_lock(adev);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER,
			"IFPO RESUME : hw(%u) sw(%d) usr(%u) rtm(%u)",
			sgpu_ifpo_get_hw_lock_value(adev),
			atomic_read(&ifpo->count),
			ifpo->user_enable, ifpo->runtime_enable);
}

static void sgpu_ifpo_half_set_user_enable(struct amdgpu_device *adev, bool value)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;

	if (ifpo->user_enable == value)
		return;

	if (adev->runpm)
		pm_runtime_get_sync(adev->dev);

	ifpo->user_enable = value;
	if (value)
		/* False to True, IFPO is enabled */
		sgpu_ifpo_half_unlock(adev);
	else
		/* True to False, IFPO is disabled */
		sgpu_ifpo_half_lock(adev);

	if (adev->runpm)
		pm_runtime_put_sync(adev->dev);
}

static void sgpu_ifpo_half_set_runtime_enable(struct amdgpu_device *adev, bool value)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;

	if (ifpo->runtime_enable == value)
		return;

	if (adev->runpm)
		pm_runtime_get_sync(adev->dev);

	ifpo->runtime_enable = value;
	if (value)
		/* False to True, IFPO is enabled */
		sgpu_ifpo_half_unlock(adev);
	else
		/* True to False, IFPO is disabled */
		sgpu_ifpo_half_lock(adev);

	if (adev->runpm)
		pm_runtime_put_sync(adev->dev);
}

static void sgpu_ifpo_half_set_afm_enable(struct amdgpu_device *adev, bool value)
{
	return;
}

static struct sgpu_ifpo_func sgpu_ifpo_half_func = {
	.enable = sgpu_ifpo_half_enable,
	.suspend = sgpu_ifpo_half_suspend,
	.resume = sgpu_ifpo_half_resume,
	.power_off = sgpu_ifpo_half_unlock,
	.lock = sgpu_ifpo_half_lock,
	.unlock = sgpu_ifpo_half_unlock,
	.reset = sgpu_ifpo_half_reset,
	.set_user_enable = sgpu_ifpo_half_set_user_enable,
	.set_runtime_enable = sgpu_ifpo_half_set_runtime_enable,
	.set_afm_enable = sgpu_ifpo_half_set_afm_enable,
};


/*
 * IFPO abort APIs
 * The driver can request IFPO power on even if IFPO power down sequence is
 * running. PMU abort function is required to support this strategy.
 */

static void sgpu_ifpo_abort_enable(struct amdgpu_device *adev)
{
	void *addr = adev->pm.pmu_mmio;
	int32_t data = 0;

	// Enable CP interrupt configuration
	data = RREG32_SOC15(GC, 0, mmCP_INT_CNTL);
	data |= 0x3c0000;
	WREG32_SOC15(GC, 0, mmCP_INT_CNTL, data);

	// RLC Safe mode entry
	WREG32_SOC15(GC, 0, mmRLC_SAFE_MODE, 0x3);
	do { } while(RREG32_SOC15(GC, 0, mmRLC_SAFE_MODE) != 0x2);

	// Initialize PM/IFPO registers
	WREG32_SOC15(GC, 0, mmRLC_GPM_TIMER_INT_3, 0x800);
	WREG32_SOC15(GC, 0, mmRLC_PG_CNTL, 0x1);		// POWER_GATING_ENABLE
	writel(0x8a, addr + BG3D_PWRCTL_CTL2_OFFSET);		// COLDVSGFXOFF|FAXI_MODE|GPUPWRREQ

	WREG32_SOC15(GC, 0, mmRLC_SAFE_MODE, 0x1);
	do { } while(RREG32_SOC15(GC, 0, mmRLC_SAFE_MODE) != 0x0);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO abort init");
}

static void sgpu_ifpo_abort_reset(struct amdgpu_device *adev)
{
	writel(0x12, adev->pm.pmu_mmio + BG3D_PWRCTL_CTL2_OFFSET);
}

static void __sgpu_ifpo_abort_power_on(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	void *addr = adev->pm.pmu_mmio;
	uint32_t gpu_status = 0;
	uint32_t timeout = 2000;
	unsigned long ret = 0, val = 0;
	uint32_t idx = 0;

	if (ifpo->state)
		return;

	/* BG3D_DVFS_CTL 0x058 enable */
	writel(0x1, adev->pm.pmu_mmio + 0x58);

	trace_sgpu_ifpo_state_change_start(1);

#if IS_ENABLED(CONFIG_CAL_IF)
	gpu_status = readl(addr + BG3D_PWRCTL_LOCK_OFFSET);

	if (gpu_status & BG3D_PWRCTL_LOCK_GPU_READY_MASK) {
		/* Disable IFPO */
		writel(0x8a, addr + BG3D_PWRCTL_CTL2_OFFSET);
		/* Decrement lock counter */
		writel(0x0, addr + BG3D_PWRCTL_LOCK_OFFSET);
	} else {
		do {
			gpu_status = readl(addr + BG3D_PWRCTL_STATUS_OFFSET);
		} while ((gpu_status & (BG3D_PWRCTL_STATUS_GPUPWRREQ_VALUE_MASK |
					BG3D_PWRCTL_STATUS_GPUPWRACK_MASK)) != 0);

		/* Disable IFPO, and keep GPUPWRREQ=0 */
		writel(0x88, addr + BG3D_PWRCTL_CTL2_OFFSET);
		if (cal_pd_control(ifpo->cal_id, 1) != 0)
			DRM_ERROR("[%s][%d] - failed to gpu inter frame power on\n", __func__, __LINE__);

		/* No need to decrement lock counter since IFPO is disabled
		 * before power-on, hence BG3D_PWRCTL_STATUS was polled.
		 * 2ms timeout is added to prevent infinite loop.
		 */
		do {
			udelay(1);
			--timeout;
			gpu_status = readl(addr + BG3D_PWRCTL_STATUS_OFFSET);
		} while ((gpu_status & BG3D_PWRCTL_STATUS_GPU_READY_MASK) == 0
				&& timeout > 0);

		if (!timeout) {
			/* set BG3D_PWRCTL_CTL1.FIFO_FENCE_MODE 2'b10 */
			ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(
						adev->pm.pmu_mmio_base), 0x4, 0);
			DRM_INFO("IFPO power on timeout! ret: %ld ctrl2 0x%x \
					stauts = 0x%x lock = 0x%x pg_cntl 0x%x",
					ret,
					readl(addr + BG3D_PWRCTL_CTL2_OFFSET),
					readl(addr + BG3D_PWRCTL_STATUS_OFFSET),
					readl(addr + BG3D_PWRCTL_LOCK_OFFSET),
					RREG32_SOC15(GC, 0, mmRLC_PG_CNTL));

			ret = exynos_smc_readsfr(adev->pm.pmu_mmio_base, &val);
			DRM_INFO("ret: %ld BG3D_PWRCTL_CTL1 value: 0x%lx\n", ret, val);

			if (AMDGPU_IS_MGFX2_EVT1(adev->grbm_chip_rev)) {
				/* GL2ACEM_STUS per GRBM_GFX_INDEX */
				for (idx = 0; idx < 4; ++idx) {
					writel((0xe0000000 + idx), adev->rmmio + 0x30800);
					DRM_INFO("GL2ACEM_STUS[%u] %#10x", idx,
							readl(adev->rmmio + 0x33A10));
				}
			}

			DRM_INFO("RLC_SRM_CNTL 0x%x, RLC_SRM_STAT 0x%x, RLC_CGTT_MGCG_OVERRIDE 0x%x, \
					RLC_MGCG_CTRL 0x%x, RLC_RLCS_CP_INT_CTRL_2 0x%x \
					CP_PFP_IB_CONTROL 0x%x, CP_PFP_LOAD_CONTROL 0x%x \n \
					CP_MEC_CNTL 0x%x, CP_PQ_STATUS 0x%x \
					CP_ME_CNTL 0x%x, CP_PWR_CNTL 0x%x \
					CP_RB0_ACTIVE 0x%x, CP_CPF_STATUS 0x%x \
					CP_STAT 0x%x, RLC_RLCS_BG3D_CTRL 0x%x",
					RREG32_SOC15(GC, 0, mmRLC_SRM_CNTL),
					RREG32_SOC15(GC, 0, mmRLC_SRM_STAT),
					RREG32_SOC15(GC, 0, mmRLC_CGTT_MGCG_OVERRIDE),
					RREG32_SOC15(GC, 0, mmRLC_MGCG_CTRL),
					RREG32_SOC15(GC, 0, mmRLC_RLCS_CP_INT_CTRL_2),
					RREG32_SOC15(GC, 0, mmCP_PFP_IB_CONTROL),
					RREG32_SOC15(GC, 0, mmCP_PFP_LOAD_CONTROL),
					RREG32_SOC15(GC, 0, mmCP_MEC_CNTL),
					RREG32_SOC15(GC, 0, mmCP_PQ_STATUS),
					RREG32_SOC15(GC, 0, mmCP_ME_CNTL),
					RREG32_SOC15(GC, 0, mmCP_PWR_CNTL),
					readl_relaxed(adev->rmmio + 0xC680),
					RREG32_SOC15(GC, 0, mmCP_CPF_STATUS),
					RREG32_SOC15(GC, 0, mmCP_STAT),
					RREG32_SOC15(GC, 0, mmRLC_RLCS_BG3D_CTRL));

			sgpu_debug_snapshot_expire_watchdog();
		}

		sgpu_devfreq_set_didt_edc(adev, 0);

		trace_sgpu_ifpo_power_on(1);
	}

	WREG32_SOC15(GC, 0, mmRLC_SRM_CNTL, 0x2);
#endif	/* IS_ENABLED(CONFIG_CAL_IF) */

	ifpo->state = true;

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO POWER ON");

	trace_sgpu_ifpo_state_change_end(1);
}

static void sgpu_ifpo_abort_power_on(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;

	mutex_lock(&ifpo->lock);

	if (!adev->probe_done || adev->in_runpm) {
		mutex_unlock(&ifpo->lock);
		return;
	}

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER,
		"IFPO POEWR ON : count(%d) usr(%u) rtm(%u) afm(%u)",
		atomic_inc_return(&ifpo->count),
		ifpo->user_enable, ifpo->runtime_enable, ifpo->afm_enable);

	__sgpu_ifpo_abort_power_on(adev);

	mutex_unlock(&ifpo->lock);
}

static void sgpu_ifpo_abort_power_off(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	uint32_t ih_wptr, ih_rptr, grbm_status, cp_busy;

	mutex_lock(&ifpo->lock);

	trace_sgpu_ifpo_state_change_start(0);

	if (!adev->probe_done || adev->in_runpm)
		goto out;

	if (atomic_read(&ifpo->count) > 0 ||
		ifpo->user_enable == false ||
		ifpo->runtime_enable == false ||
		ifpo->afm_enable == false)
		goto out;

	if (!ifpo->state)
		goto out;

	ih_rptr = adev->irq.ih.last_rptr;
	ih_wptr = amdgpu_ih_get_wptr(adev, &adev->irq.ih);

	grbm_status = RREG32_SOC15(GC, 0, mmGRBM_STATUS);
	cp_busy = (grbm_status & GRBM_STATUS__CP_BUSY_MASK) >>
			GRBM_STATUS__CP_BUSY__SHIFT;

	if ((ih_rptr != ih_wptr) || cp_busy ||
	    (atomic_read(&adev->pc_count) != 0) ||
	    (atomic_read(&adev->sqtt_count) != 0)) {
		SGPU_LOG(adev, DMSG_INFO, DMSG_POWER,
			"GRBM_STATUE=0x%08x, CP_BUSY= 0x%x, IH rptr=%u, wptr=%u",
			grbm_status, cp_busy, ih_rptr, ih_wptr);
		goto out;
	}

	/* BG3D_DVFS_CTL 0x058 disable */
	writel(0x0, adev->pm.pmu_mmio + 0x58);

#if IS_ENABLED(CONFIG_CAL_IF)
	WREG32_SOC15(GC, 0, mmRLC_SRM_CNTL, 0x3);

	if (cal_pd_control(ifpo->cal_id, 0) != 0)
			DRM_ERROR("%s fail to power down\n", __func__);
#endif	/* IS_ENABLED(CONFIG_CAL_IF) */

	ifpo->state = false;

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO POWER OFF");

	trace_sgpu_ifpo_power_off(0);
out:
	trace_sgpu_ifpo_state_change_end(0);

	mutex_unlock(&ifpo->lock);
}

static void sgpu_ifpo_abort_suspend(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;

	mutex_lock(&ifpo->lock);

	if (!adev->probe_done) {
		mutex_unlock(&ifpo->lock);
		return;
	}

	/* GPU should be in IFPO power on state before entering suspend */
	__sgpu_ifpo_abort_power_on(adev);

	/* reset ColdGfxOff = 2'b00 before entering suspend */
	sgpu_ifpo_abort_reset(adev);

	mutex_unlock(&ifpo->lock);
}

static void sgpu_ifpo_abort_resume(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;

	atomic_set(&ifpo->count, 0);
}

static void sgpu_ifpo_abort_count_decrease(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;

	if (!adev->probe_done || adev->in_runpm)
		return;

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER,
		"IFPO COUNT DEC : count(%d) usr(%u) rtm(%u) afm(%u)",
		atomic_dec_return(&ifpo->count),
		ifpo->user_enable, ifpo->runtime_enable, ifpo->afm_enable);
}

static void sgpu_ifpo_abort_set_user_enable(struct amdgpu_device *adev, bool value)
{
	adev->ifpo.user_enable = value;
}

static void sgpu_ifpo_abort_set_runtime_enable(struct amdgpu_device *adev, bool value)
{
	adev->ifpo.runtime_enable = value;
}

static void sgpu_ifpo_abort_set_afm_enable(struct amdgpu_device *adev, bool value)
{
	adev->ifpo.afm_enable = value;
}

static struct sgpu_ifpo_func sgpu_ifpo_abort_func = {
	.enable = sgpu_ifpo_abort_enable,
	.suspend = sgpu_ifpo_abort_suspend,
	.resume = sgpu_ifpo_abort_resume,
	.power_off = sgpu_ifpo_abort_power_off,
	.lock = sgpu_ifpo_abort_power_on,
	.unlock = sgpu_ifpo_abort_count_decrease,
	.reset = sgpu_ifpo_abort_reset,
	.set_user_enable = sgpu_ifpo_abort_set_user_enable,
	.set_runtime_enable = sgpu_ifpo_abort_set_runtime_enable,
	.set_afm_enable = sgpu_ifpo_abort_set_afm_enable,
};

/* Dummy function for IFPO disable */

static void sgpu_ifpo_dummy_function(struct amdgpu_device *adev)
{
	return;
}

static void sgpu_ifpo_dummy_function_val(struct amdgpu_device *adev, bool value)
{
	return;
}

static struct sgpu_ifpo_func sgpu_ifpo_dummy_func = {
	.enable = sgpu_ifpo_dummy_function,
	.suspend = sgpu_ifpo_dummy_function,
	.resume = sgpu_ifpo_dummy_function,
	.power_off = sgpu_ifpo_dummy_function,
	.lock = sgpu_ifpo_dummy_function,
	.unlock = sgpu_ifpo_dummy_function,
	.reset = sgpu_ifpo_dummy_function,
	.set_user_enable = sgpu_ifpo_dummy_function_val,
	.set_runtime_enable = sgpu_ifpo_dummy_function_val,
	.set_afm_enable = sgpu_ifpo_dummy_function_val,
};

/* IFPO module initialization */

void sgpu_ifpo_init(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	int ret = 0;

	ret |= of_property_read_u32(adev->pldev->dev.of_node,
					"ifpo_type", &ifpo->type);
	ret |= of_property_read_u32(adev->pldev->dev.of_node,
					"ifpo_cal_id", &ifpo->cal_id);
	if (ret) {
		DRM_ERROR("cannot find ifpo_type or ifpo_cal_id field in dt");
		ifpo->type = IFPO_DISABLED;
		ifpo->cal_id = 0;
	}

	switch (ifpo->type) {
	case IFPO_DISABLED :
		ifpo->func = &sgpu_ifpo_dummy_func;
		break;

	case IFPO_ABORT :
		ifpo->func = &sgpu_ifpo_abort_func;
		break;

	case IFPO_HALF_AUTO :
		ifpo->func = &sgpu_ifpo_half_func;
		break;

	default :
		DRM_ERROR("invalid ifpo_type(%u)", ifpo->type);
		ifpo->func = &sgpu_ifpo_dummy_func;
		ifpo->type = IFPO_DISABLED;
		ifpo->cal_id = 0;
		break;
	}

	ifpo->cal_id |= BLKPWR_MAGIC;

	DRM_INFO("%s : IFPO type(%u) cal_id(%#010x)", __func__,
			ifpo->type, ifpo->cal_id);

	mutex_init(&ifpo->lock);
	atomic_set(&ifpo->count, 0);

	ifpo->state = true;		// IFPO power on
	ifpo->user_enable = true;
	ifpo->runtime_enable = true;
	ifpo->afm_enable = true;
}
