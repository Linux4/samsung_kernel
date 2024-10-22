// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/sched/clock.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/seq_file.h>

#include "mt-plat/aee.h"

#include "apusys_rv_trace.h"

#include "apusys_power.h"
#include "apusys_secure.h"
#include "apu_regdump.h"
#include "../apu.h"
#include "../apu_debug.h"
#include "../apu_config.h"
#include "../apu_hw.h"
#include "../apu_excep.h"
#include "../apu_ipi.h"
#include "../apu_ce_excep.h"


#include "apummu_tbl.h"

enum apu_hw_sem_sys_id {
	APU_HW_SEM_SYS_APU = 0UL, // mbox0
	APU_HW_SEM_SYS_GZ = 1UL, // mbox1
	APU_HW_SEM_SYS_SCP = 3UL, // mbox3
	APU_HW_SEM_SYS_APMCU = 11UL, // mbox11
};

#define CHECK_BIT(var, pos) ((var) & (1<<(pos)))

enum APU_EXCEPTION_ID {
	rcx_ao_infra_apb_timeout = 0,
	rcx_ao_infra_apb_secure_vio_irq,
	rcx_infra_apb_timeout,
	rcx_infra_apb_secure_vio_irq,
	apu_idle2max_arb_to_irq,
	mbox_err_irq_0,
	mbox_err_irq_1,
	mbox_err_irq_2,
	mbox_err_irq_3,
	mbox_err_irq_4,
	mbox_err_irq_5,
	mbox_err_irq_6,
	mbox_err_irq_7,
	mbox_err_irq_8,
	mbox_err_irq_9,
	mbox_err_irq_10,
	mbox_err_irq_11,
	mbox_err_irq_12,
	mbox_err_irq_13,
	mbox_err_irq_14,
	mbox_err_irq_15,
	are_abnormal_irq,
	north_mmu_m0_hit_set,
	north_mmu_m1_hit_set,
	south_mmu_m0_hit_set,
	south_mmu_m1_hit_set,
	acx0_infra_apb_timeout,
	acx0_infra_apb_secure_vio_irq,
	reserved_0,
	reserved_1,
	reserved_2,
	reserved_3,
	apu_mmu_cmu_irq
};

/* for IPI IRQ affinity tuning*/
static struct cpumask perf_cpus, normal_cpus;

/* for power off timeout detection */
static struct mtk_apu *g_apu;
static struct workqueue_struct *apu_workq;
static struct delayed_work timeout_work;

#if IS_ENABLED(CONFIG_PM_SLEEP)
static struct wakeup_source *ws;
#endif

static bool is_under_lp_scp_recovery_flow;

static struct delayed_work apu_polling_on_work;

/*
 * COLD BOOT power off timeout
 *
 * exception will trigger
 * if power on and not power off
 * without ipi send
 */
#define APU_PWROFF_TIMEOUT_MS (90 * 1000)

/*
 * WARM BOOT power off timeout
 *
 * excpetion will trigger
 * if power off not done after ipi send
 */
#define IPI_PWROFF_TIMEOUT_MS (60 * 1000)

/*
 * power on polling timeout
 *
 * excpetion will trigger
 * if polling power on status timeout
 */
#define APU_PWRON_TIMEOUT_MS (1000)

static void apu_timeout_work(struct work_struct *work)
{
	int i;
	struct mtk_apu *apu = g_apu;
	struct device *dev = apu->dev;

	if (apu->bypass_pwr_off_chk) {
		dev_info(dev, "%s: skip aee\n", __func__);
		return;
	}

	apu->bypass_pwr_off_chk = true;

	dev_info(dev, "ipi_debug_dump:\n");

	dev_info(dev, "local_pwr_ref_cnt = %d\n",
		apu->local_pwr_ref_cnt);
	for (i = 0; i < APU_IPI_MAX; i++)
		dev_info(dev, "ipi_pwr_ref_cnt[%d] = %d\n",
			i, apu->ipi_pwr_ref_cnt[i]);

#if IS_ENABLED(CONFIG_PM_SLEEP)
	dev_info(dev, "wake_lock_ref_cnt = %d\n",
		apu->wake_lock_ref_cnt);
	for (i = 0; i < APU_IPI_MAX; i++)
		dev_info(dev, "ipi_wake_lock_ref_cnt[%d] = %d\n",
			i, apu->ipi_wake_lock_ref_cnt[i]);
#endif

	apusys_rv_aee_warn("APUSYS_RV", "APUSYS_RV_POWER_OFF_TIMEOUT");
}

static int apusys_rv_smc_call(struct device *dev, uint32_t smc_id,
	uint32_t a2)
{
	struct arm_smccc_res res;

	if (smc_id != MTK_APUSYS_KERNEL_OP_APUSYS_RV_PWR_CTRL)
		dev_info(dev, "%s: smc call %d(a2 = %d)\n",
				__func__, smc_id, a2);

	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL, smc_id,
				a2, 0, 0, 0, 0, 0, &res);
	if (((int) res.a0) < 0)
		dev_info(dev, "%s: smc call %d return error(%ld)\n",
			__func__,
			smc_id, res.a0);

	return res.a0;
}

static void apu_setup_apummu(struct mtk_apu *apu, int boundary, int ns, int domain)
{
	struct device *dev = apu->dev;
	unsigned long hw_logger_addr = 0;
	unsigned long tcm_addr = 0;

	dev_info(dev, "%s: apu->apummu_hwlog_buf_da= 0x%llx\n",
		__func__, (unsigned long long) apu->apummu_hwlog_buf_da);

	apu->apummu_hwlog_buf_da = apu->apummu_hwlog_buf_da >> 12;
	hw_logger_addr = (u32)(apu->apummu_hwlog_buf_da);
		dev_info(dev, "%s: apu->apummu_hwlog_buf_da= 0x%x\n",
		__func__, (u32) apu->apummu_hwlog_buf_da);

	tcm_addr = 0x1d000000;
	tcm_addr = (u32)(tcm_addr >> 12);

	if (apu->platdata->flags & F_SECURE_BOOT) {
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_SETUP_APUMMU, 0);
	} else {
		rv_boot(apu->code_da >> 12, 0, hw_logger_addr, eAPUMMU_PAGE_LEN_1MB,
		tcm_addr, eAPUMMU_PAGE_LEN_512KB);
	}
}

static void apu_setup_devapc(struct mtk_apu *apu)
{
	int32_t ret;
	struct device *dev = apu->dev;

	ret = (int32_t)apusys_rv_smc_call(dev,
		MTK_APUSYS_KERNEL_OP_DEVAPC_INIT_RCX, 0);

	dev_info(dev, "%s: %d\n", __func__, ret);
}

static void apu_reset_mp(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	unsigned long flags;

	if (apu->platdata->flags & F_SECURE_BOOT) {
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_RESET_MP, 0);
	} else {
		spin_lock_irqsave(&apu->reg_lock, flags);
		/* reset uP */
		iowrite32(0, apu->md32_sysctrl + MD32_SYS_CTRL);
		spin_unlock_irqrestore(&apu->reg_lock, flags);

		udelay(10);

		spin_lock_irqsave(&apu->reg_lock, flags);
		/* md32_g2b_cg_en | md32_dbg_en | md32_soft_rstn */
		iowrite32(0xc01, apu->md32_sysctrl + MD32_SYS_CTRL);
		spin_unlock_irqrestore(&apu->reg_lock, flags);
		apu_drv_debug("%s: MD32_SYS_CTRL = 0x%x\n",
			__func__, ioread32(apu->md32_sysctrl + MD32_SYS_CTRL));

		spin_lock_irqsave(&apu->reg_lock, flags);
		/* md32 clk enable */
		iowrite32(0x1, apu->md32_sysctrl + MD32_CLK_EN);
		/* set up_wake_host_mask0 for wdt/mbox irq */
		iowrite32(0x1c0001, apu->md32_sysctrl + UP_WAKE_HOST_MASK0);
		spin_unlock_irqrestore(&apu->reg_lock, flags);
		apu_drv_debug("%s: MD32_CLK_EN = 0x%x\n",
			__func__, ioread32(apu->md32_sysctrl + MD32_CLK_EN));
		apu_drv_debug("%s: UP_WAKE_HOST_MASK0 = 0x%x\n",
			__func__, ioread32(apu->md32_sysctrl + UP_WAKE_HOST_MASK0));
	}
}

static void apu_setup_boot(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	unsigned long flags;
	int boot_from_tcm;

	if (TCM_OFFSET == 0)
		boot_from_tcm = 1;
	else
		boot_from_tcm = 0;

	if (apu->platdata->flags & F_SECURE_BOOT) {
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_SETUP_BOOT, 0);
	} else {
		/* Set uP boot addr to DRAM.
		 * If boot from tcm == 1, boot addr will always map to
		 * 0x1d000000 no matter what value boot_addr is
		 */
		spin_lock_irqsave(&apu->reg_lock, flags);
		if ((apu->platdata->flags & F_BYPASS_IOMMU) ||
			(apu->platdata->flags & F_PRELOAD_FIRMWARE))
			iowrite32((u32)apu->code_da,
				apu->apu_ao_ctl + MD32_BOOT_CTRL);
		else
			iowrite32((u32)CODE_BUF_DA | boot_from_tcm,
				apu->apu_ao_ctl + MD32_BOOT_CTRL);
		spin_unlock_irqrestore(&apu->reg_lock, flags);
		apu_drv_debug("%s: MD32_BOOT_CTRL = 0x%x\n",
			__func__, ioread32(apu->apu_ao_ctl + MD32_BOOT_CTRL));

		spin_lock_irqsave(&apu->reg_lock, flags);
		/* set predefined MPU region for cache access */
		iowrite32(0xAB, apu->apu_ao_ctl + MD32_PRE_DEFINE);
		spin_unlock_irqrestore(&apu->reg_lock, flags);
		apu_drv_debug("%s: MD32_PRE_DEFINE = 0x%x\n",
			__func__, ioread32(apu->apu_ao_ctl + MD32_PRE_DEFINE));
	}
}

static void apu_start_mp(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	int i;
	unsigned long flags;

	if (apu->platdata->flags & F_SECURE_BOOT) {
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_START_MP, 0);
	} else {
		spin_lock_irqsave(&apu->reg_lock, flags);
		/* Release runstall */
		iowrite32(0x0, apu->apu_ao_ctl + MD32_RUNSTALL);
		spin_unlock_irqrestore(&apu->reg_lock, flags);

		for (i = 0; i < 20; i++) {
			dev_info(dev, "apu boot: pc=%08x, sp=%08x\n",
			ioread32(apu->md32_sysctrl + 0x838),
				ioread32(apu->md32_sysctrl+0x840));
			usleep_range(0, 20);
		}
	}
}

static int mt6989_rproc_start(struct mtk_apu *apu)
{
	int ns = 1; /* Non Secure */
	int domain = 0;
	int boundary = (u32) upper_32_bits(apu->code_da);

	if ((apu->platdata->flags & F_BRINGUP) == 0)
		apu_setup_devapc(apu);

	apu_setup_apummu(apu, boundary, ns, domain);

	apu_reset_mp(apu);

	apu_setup_boot(apu);

	apu_start_mp(apu);

	return 0;
}

static int mt6989_rproc_stop(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;

	/* Hold runstall */
	if (apu->platdata->flags & F_SECURE_BOOT)
		apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_STOP_MP, 0);
	else
		iowrite32(0x1, apu->apu_ao_ctl + 8);

	return 0;
}

static void mt6989_apu_pwr_wake_lock(struct mtk_apu *apu, uint32_t id)
{
#if IS_ENABLED(CONFIG_PM_SLEEP)
	unsigned long flags;
	uint32_t wake_lock_ref_cnt_local;

	spin_lock_irqsave(&apu->wakelock_spinlock, flags);
	apu->ipi_wake_lock_ref_cnt[id]++;
	apu->wake_lock_ref_cnt++;
	if (apu->wake_lock_ref_cnt == 1)
		__pm_stay_awake(ws);
	wake_lock_ref_cnt_local = apu->wake_lock_ref_cnt;
	spin_unlock_irqrestore(&apu->wakelock_spinlock, flags);
	/* remove to reduce log
	 * dev_info(apu->dev, "%s(%d): wake_lock_ref_cnt = %d\n",
	 *	__func__, id, wake_lock_ref_cnt_local);
	 */
#endif
}

static void mt6989_apu_pwr_wake_unlock(struct mtk_apu *apu, uint32_t id)
{
#if IS_ENABLED(CONFIG_PM_SLEEP)
	unsigned long flags;
	uint32_t wake_lock_ref_cnt_local;

	spin_lock_irqsave(&apu->wakelock_spinlock, flags);
	apu->ipi_wake_lock_ref_cnt[id]--;
	apu->wake_lock_ref_cnt--;
	if (apu->wake_lock_ref_cnt == 0)
		__pm_relax(ws);
	wake_lock_ref_cnt_local = apu->wake_lock_ref_cnt;
	spin_unlock_irqrestore(&apu->wakelock_spinlock, flags);
	/* remove to reduce log
	 * dev_info(apu->dev, "%s(%d): wake_lock_ref_cnt = %d\n",
	 *	__func__, id, wake_lock_ref_cnt_local);
	 */
#endif
}


/*
 * APU_SEMA_CTRL0
 * [15:00]      SEMA_KEY_SET    Each bit corresponds to different user.
 * [31:16]      SEMA_KEY_CLR    Each bit corresponds to different user.
 *
 * usr_bit: subsys_id
 * ctl:
 *      0x1 - acquire hw semaphore
 *      0x0 - release hw semaphore
 */
static int apu_hw_sema_ctl(struct mtk_apu *apu,
		uint8_t usr_bit, uint8_t ctl, int32_t timeout)
{
	struct device *dev = apu->dev;
	uint32_t timeout_cnt = 0;
	uint8_t ctl_bit = 0;

	if (ctl == 0x1) {
		// acquire is set
		ctl_bit = usr_bit;
	} else if (ctl == 0x0) {
		// release is clear
		ctl_bit = usr_bit + 16;
	} else {
		return -EINVAL;
	}

	/* dev_info(dev, "%s ++ usr_bit:%d ctl:%d (APU_SEMA_CTRL0 = 0x%08x)\n",
	 *		__func__, usr_bit, ctl,
	 *		ioread32(apu->apu_mbox + APU_SEMA_CTRL0));
	 */

	iowrite32(BIT(ctl_bit), apu->apu_mbox + APU_SEMA_CTRL0);

	/* todo: no need to check register value for semaphore release?
	 * need to consider other host may acquire hw sem right after release
	 * -> register check may be fail but actually no error occurred
	 */
	if (ctl == 0)
		goto end;

	while ((ioread32(apu->apu_mbox + APU_SEMA_CTRL0) & BIT(ctl_bit))
			>> ctl_bit != ctl) {

		if (timeout >= 0 && timeout_cnt++ >= timeout) {
			dev_info(dev,
			"%s timeout usr_bit:%d ctl:%d rnd:%d (APU_SEMA_CTRL0 = 0x%08x)\n",
				__func__, usr_bit, ctl, timeout_cnt,
				ioread32(apu->apu_mbox + APU_SEMA_CTRL0));
			return -EBUSY;
		}

		iowrite32(BIT(ctl_bit), apu->apu_mbox + APU_SEMA_CTRL0);
		udelay(1);
	}

end:
	dev_info(dev, "%s -- usr_bit:%d ctl:%d (APU_SEMA_CTRL0 = 0x%08x)(%u)\n",
			__func__, usr_bit, ctl,
			ioread32(apu->apu_mbox + APU_SEMA_CTRL0), timeout_cnt);

	return 0;
}

/*
 * op:
 *      0 - power off
 *      1 - power on
 */
static int apu_power_ctrl(struct mtk_apu *apu, uint32_t op)
{
	int ret = 0;
	struct device *dev = apu->dev;
	uint32_t timeout = 300; /* 300 us to align with tf-a driver */
	uint32_t global_ref_cnt;
	struct timespec64 ts, te;

	if (apu->platdata->flags & F_SECURE_BOOT) {
		ktime_get_ts64(&ts);
		ret = apusys_rv_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_RV_PWR_CTRL, op);
		ktime_get_ts64(&te);
		ts = timespec64_sub(te, ts);
		apu->smc_time_diff_ns = timespec64_to_ns(&ts);
	} else {
		ret = apu_hw_sema_ctl(apu, APU_HW_SEM_SYS_APMCU, 1, timeout);
		if (ret) {
			dev_info(dev, "%s(%d): sem acquire timeout\n", __func__, op);
			return ret;
		}

		global_ref_cnt = ioread32(apu->apu_mbox + APU_SEMA_DATA0);
		dev_info(dev, "%s: op = %d, current global_ref_cnt = %d\n",
			__func__, op, global_ref_cnt);

		if (global_ref_cnt > 2) {
			/* only possible to be 0/1/2 */
			dev_info(dev, "%s: global_ref_cnt(%d) > 2\n",
				__func__, global_ref_cnt);
			ret = apu_hw_sema_ctl(apu, APU_HW_SEM_SYS_APMCU, 1, timeout);
			if (ret)
				dev_info(dev, "%s(%d): sem acquire timeout\n", __func__, op);
			return -EINVAL;
		}

		if (op == 0) {
			global_ref_cnt--;
			iowrite32(global_ref_cnt, apu->apu_mbox + APU_SEMA_DATA0);
			/* global_ref_cnt is from 1 to 0, need to power off */
			if (global_ref_cnt == 0) {
				/* set wkup bit to 0(use mbox11 for linux power ctrl) */
				iowrite32(0, apu->apu_mbox + MBOX_WKUP_CFG + 0x1300);
			}
		} else if (op == 1) {
			global_ref_cnt++;
			iowrite32(global_ref_cnt, apu->apu_mbox + APU_SEMA_DATA0);
			/* global_ref_cnt is from 0 to 1, need to power on */
			if (global_ref_cnt == 1) {
				/* set wkup bit to 1(use mbox11 for linux power ctrl) */
				iowrite32(1, apu->apu_mbox + MBOX_WKUP_CFG + 0x1300);
			}
		}

		ret = apu_hw_sema_ctl(apu, APU_HW_SEM_SYS_APMCU, 0, timeout);
		if (ret) {
			dev_info(dev, "%s(%d): sem release timeout\n", __func__, op);
			return ret;
		}

		dev_info(dev, "%s: op = %d, current global_ref_cnt = %d\n",
			__func__, op, global_ref_cnt);
	}

	return ret;
}

static void timesync_update(struct mtk_apu *apu)
{
	u64 timertick;
	unsigned long flags;
	uint32_t sys_timer_clk_mhz;
	struct device *dev = apu->dev;

	if (apu->platdata->flags & F_FPGA_EP)
		sys_timer_clk_mhz = 150;
	else
		sys_timer_clk_mhz = 13;

	spin_lock_irqsave(&apu->reg_lock, flags);
	apu->conf_buf->time_offset = sched_clock();
	timertick = arch_timer_read_counter();
	spin_unlock_irqrestore(&apu->reg_lock, flags);

	/* Calculate time diff for warm boot */
	apu->conf_buf->time_diff =
		(timertick * 1000 / sys_timer_clk_mhz) - apu->conf_buf->time_offset;
	apu->conf_buf->time_diff_cycle =
		timertick - (apu->conf_buf->time_offset * sys_timer_clk_mhz / 1000);

	iowrite32(1, apu->apu_mbox + MBOX_RV_TIMESYNC_FLG);

	if (apu->platdata->flags & F_DEBUG_LOG_ON) {
		dev_info(dev, "%s: time_diff = %llu, time_diff_cycle = %llu\n", __func__,
			apu->conf_buf->time_diff, apu->conf_buf->time_diff_cycle);
		dev_info(dev, "%s: time_offset = %llu, timertick = %llu\n", __func__,
			apu->conf_buf->time_offset, timertick);
	}
}

static bool pwr_on_fail_aee_triggered;
static bool pwr_off_fail_aee_triggered;
static int mt6989_cold_boot_power_on(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	int ret;

	apu->ipi_pwr_ref_cnt[APU_IPI_INIT]++;
	apu->local_pwr_ref_cnt++;

	/* initialize global ref cnt to zero because apu_top may already
	 * call power on smc call
	 */
	iowrite32(0, apu->apu_mbox + APU_SEMA_DATA0);
	ret = apu_power_ctrl(apu, 1);
	if (ret && !pwr_on_fail_aee_triggered) {
		apusys_rv_aee_warn("APUSYS_RV",
			"APUSYS_RV_POWER_ON_FAIL");
		pwr_on_fail_aee_triggered = true;
	}

	return ret;
}

static int mt6989_polling_rpc_status(struct mtk_apu *apu, u32 pwr_stat, u32 timeout)
{
	int ret = 0;
	void *addr = 0;
	uint32_t val = 0;

	if (pwr_stat == 0)
		addr = apu->apu_rpc + APU_RPC_INTF_PWR_RDY;
	else
		addr = apu->apu_mbox + MBOX_RV_PWR_STA_FLG;

	ret = readl_relaxed_poll_timeout_atomic(addr, val,
		((val & (0x1UL)) == pwr_stat), 1, timeout);

	if (ret)
		dev_info(apu->dev, "%s(pwr_stat = %u): timeout\n", __func__, pwr_stat);

	return ret;
}

static inline void profile_start(struct timespec64 *ts)
{
	ktime_get_ts64(ts);
}

static inline uint64_t profile_end(struct timespec64 *ts, struct timespec64 *te)
{
	ktime_get_ts64(te);
	*ts = timespec64_sub(*te, *ts);
	return timespec64_to_ns(ts);
}

/* TODO: block power on off after pwr_on_fail_aee_triggered
 *		 or pwr_off_fail_aee_triggered?
 */
static int mt6989_power_on_off_locked(struct mtk_apu *apu, u32 id, u32 on, u32 off)
{
	int ret = 0;
	struct device *dev = apu->dev;
	uint32_t rpc_state = 0, pwr_ready = 0;
	struct timespec64 ts, te;

	if (on == 1 && off == 0) {
		/* block normal cmds when under scp lp recovery flow  */
		if (is_under_lp_scp_recovery_flow)
			return -EBUSY;
		/* pwr on */
		if (apu->ipi_pwr_ref_cnt[id] == U32_MAX) {
			dev_info(dev, "%s: ipi_pwr_ref_cnt[%u] == U32_MAX\n", __func__, id);
			ret = -EINVAL;
		} else {
			profile_start(&ts);
			apu->ipi_pwr_ref_cnt[id]++;
			apu->local_pwr_ref_cnt++;
			mt6989_apu_pwr_wake_lock(apu, id);
			apu->sub_latency[0] = profile_end(&ts, &te);

			if (apu->local_pwr_ref_cnt == 1) {
				profile_start(&ts);
				pwr_ready = ioread32(apu->apu_rpc + APU_RPC_INTF_PWR_RDY) & 0x1;
				rpc_state = ioread32(apu->apu_rpc + APU_RPC_STATUS_1) & 0x1;
				/* rpc_state == 1 means in lp mode, need to retry
				 * only APU_IPI_SCP_NP_RECOVER can bypass the check
				 */
				if (pwr_ready == 1 && id != APU_IPI_SCP_NP_RECOVER && rpc_state == 1) {
					dev_info(dev, "%s(%d): APU_RPC_STATUS_1 = 0x%x\n",
						__func__, ret,
						ioread32(apu->apu_rpc + APU_RPC_STATUS_1));
					mt6989_apu_pwr_wake_unlock(apu, id);
					apu->ipi_pwr_ref_cnt[id]--;
					apu->local_pwr_ref_cnt--;
					return -EBUSY;
				}
				apu->sub_latency[1] = profile_end(&ts, &te);

				profile_start(&ts);
				timesync_update(apu);
				apu->sub_latency[2] = profile_end(&ts, &te);

				profile_start(&ts);
				if (apu->apusys_rv_trace_on)
					apusys_rv_trace_begin("apu_power_ctrl(%d/%d/%d)", id, on, off);
				ret = apu_power_ctrl(apu, 1);
				if (apu->apusys_rv_trace_on)
					apusys_rv_trace_end();
				apu->sub_latency[3] = profile_end(&ts, &te);

				profile_start(&ts);
				if (!ret) {
					/* for power timeout detection */
					if ((apu->platdata->flags & F_BRINGUP) == 0)
						queue_delayed_work(apu_workq,
							&timeout_work,
							msecs_to_jiffies(APU_PWROFF_TIMEOUT_MS));
					if (id == APU_IPI_SCP_NP_RECOVER && rpc_state == 1)
						is_under_lp_scp_recovery_flow = true;

					if (apu->ce_dbg_polling_dump_mode)
						apu_ce_start_timer_dump_reg();
					if (apu->pwr_on_polling_dbg_mode)
						queue_delayed_work(apu_workq,
						&apu_polling_on_work,
						msecs_to_jiffies(APU_PWRON_TIMEOUT_MS));

				} else {
					mt6989_apu_pwr_wake_unlock(apu, id);
					apu->ipi_pwr_ref_cnt[id]--;
					apu->local_pwr_ref_cnt--;
					dev_info(dev, "%s: power on fail(%d)\n", __func__, ret);
				}
				apu->sub_latency[4] = profile_end(&ts, &te);
			}
		}
	} else if (on == 0 && off == 1) {
		/* pwr off */
		if (apu->ipi_pwr_ref_cnt[id] == 0) {
			dev_info(dev, "%s: ipi_pwr_ref_cnt[%u] == 0\n", __func__, id);
			ret = -EINVAL;
		} else {
			profile_start(&ts);
			apu->ipi_pwr_ref_cnt[id]--;
			apu->local_pwr_ref_cnt--;
			mt6989_apu_pwr_wake_unlock(apu, id);
			apu->sub_latency[0] = profile_end(&ts, &te);
			if (apu->ce_dbg_polling_dump_mode)
				apu_ce_stop_timer_dump_reg();

			if (apu->local_pwr_ref_cnt == 0) {
				profile_start(&ts);

				if (apu->pwr_on_polling_dbg_mode)
					cancel_delayed_work_sync(&apu_polling_on_work);

				apu->sub_latency[1] = profile_end(&ts, &te);

				profile_start(&ts);
				ret = apu_power_ctrl(apu, 0);
				apu->sub_latency[2] = profile_end(&ts, &te);

				profile_start(&ts);
				if (!ret) {
					/* clear status & cancel timeout worker */
					apu->bypass_pwr_off_chk = false;
					if ((apu->platdata->flags & F_BRINGUP) == 0)
						cancel_delayed_work_sync(&timeout_work);
					if (id == APU_IPI_SCP_NP_RECOVER)
						is_under_lp_scp_recovery_flow = false;
				} else {
					mt6989_apu_pwr_wake_lock(apu, id);
					apu->ipi_pwr_ref_cnt[id]++;
					apu->local_pwr_ref_cnt++;
					dev_info(dev, "%s: power off fail(%d)\n", __func__, ret);
				}
				apu->sub_latency[3] = profile_end(&ts, &te);
			}
		}
	} else {
		dev_info(apu->dev, "%s: invalid operation: id(%d), on(%d), off(%d)\n",
			__func__, id, on, off);
		ret = -EINVAL;
	}

	return ret;
}

static int mt6989_power_on_off(struct mtk_apu *apu, u32 id, u32 on, u32 off)
{
	int ret = 0;
	struct device *dev = apu->dev;
	/* struct timespec64 ts, te; */
	uint32_t retry_cnt = 500, i = 0;
	struct timespec64 ts, te;
	struct timespec64 t1, t2;

	/* ktime_get_ts64(&ts); */

	profile_start(&t1);
	for (i = 0; i < retry_cnt; i++) {
		mutex_lock(&apu->power_lock);

		profile_start(&ts);
		ret = mt6989_power_on_off_locked(apu, id, on, off);
		apu->sub_latency[5] = profile_end(&ts, &te);

		mutex_unlock(&apu->power_lock);
		/* retry if return value is -EBUSY because
		 * hw sem may be blocked by other host or apu under lp mode
		 */
		if (ret == -EBUSY) {
			if (!(i % 10))
				dev_info(dev, "%s: retry on(%u) off(%u)(%u/%u)\n",
					__func__, on, off, i, retry_cnt);
			if (i < 10)
				usleep_range(200, 500);
			else if (i < 50)
				usleep_range(1000, 2000);
			else
				usleep_range(10000, 11000);
			continue;
		}
		break;
	}
	apu->sub_latency[6] = profile_end(&t1, &t2);
	apu->sub_latency[7] = i;

	if (ret) {
		if (on == 1 && off == 0 && !pwr_on_fail_aee_triggered) {
			apusys_rv_aee_warn("APUSYS_RV",
				"APUSYS_RV_POWER_ON_FAIL");
			pwr_on_fail_aee_triggered = true;
		} else if (on == 0 && off == 1 && !pwr_off_fail_aee_triggered) {
			apusys_rv_aee_warn("APUSYS_RV",
				"APUSYS_RV_POWER_OFF_FAIL");
			pwr_off_fail_aee_triggered = true;
		}
	}

	/* remove to reduce latency
	 * ktime_get_ts64(&te);
	 * ts = timespec64_sub(te, ts);
	 * apu_info_ratelimited(dev,
	 *  "%s(%d/%d/%d): local_pwr_ref_cnt = %d, ipi_pwr_ref_cnt = %d, time = %lld ns\n",
	 *  __func__, id, on, off, apu->local_pwr_ref_cnt,
	 *  apu->ipi_pwr_ref_cnt[id], timespec64_to_ns(&ts));
	 */

	return ret;
}

static int mt6989_ipi_send_pre(struct mtk_apu *apu, uint32_t id, bool is_host_initiated)
{
	if (apu->platdata->flags & F_BRINGUP)
		return 0;

	if (!is_host_initiated)
		return 0;

	cancel_delayed_work_sync(&timeout_work);

	queue_delayed_work(apu_workq,
		&timeout_work,
		msecs_to_jiffies(IPI_PWROFF_TIMEOUT_MS));

	return 0;
}

static void mt6989_debug_info_dump(struct mtk_apu *apu, struct seq_file *s)
{
	int i;

	seq_puts(s, "\nipi_debug_dump:\n");

	seq_printf(s, "local_pwr_ref_cnt = %d\n",
		apu->local_pwr_ref_cnt);
	for (i = 0; i < APU_IPI_MAX; i++)
		seq_printf(s, "ipi_pwr_ref_cnt[%d] = %d\n",
			i, apu->ipi_pwr_ref_cnt[i]);

#if IS_ENABLED(CONFIG_PM_SLEEP)
	seq_printf(s, "wake_lock_ref_cnt = %d\n",
		apu->wake_lock_ref_cnt);
	for (i = 0; i < APU_IPI_MAX; i++)
		seq_printf(s, "ipi_wake_lock_ref_cnt[%d] = %d\n",
			i, apu->ipi_wake_lock_ref_cnt[i]);
#endif
}

static int mt6989_irq_affin_init(struct mtk_apu *apu)
{
	int i;

	/* init perf_cpus mask 0x80, CPU7 only */
	cpumask_clear(&perf_cpus);
	cpumask_set_cpu(7, &perf_cpus);

	/* init normal_cpus mask 0x0f, CPU0~CPU4 */
	cpumask_clear(&normal_cpus);
	for (i = 0; i < 4; i++)
		cpumask_set_cpu(i, &normal_cpus);

	irq_set_affinity_hint(apu->mbox0_irq_number, &normal_cpus);

	return 0;
}

static int mt6989_irq_affin_set(struct mtk_apu *apu)
{
	irq_set_affinity_hint(apu->mbox0_irq_number, &perf_cpus);

	return 0;
}

static int mt6989_irq_affin_unset(struct mtk_apu *apu)
{
	irq_set_affinity_hint(apu->mbox0_irq_number, &normal_cpus);

	return 0;
}

static int mt6989_irq_affin_clear(struct mtk_apu *apu)
{
	irq_set_affinity_hint(apu->mbox0_irq_number, NULL);

	return 0;
}

static int mt6989_check_apu_exp_irq(struct mtk_apu *apu, char *ce_module)
{
	return 1;
}


static int mt6989_apu_memmap_init(struct mtk_apu *apu)
{
	struct platform_device *pdev = apu->pdev;
	struct device *dev = apu->dev;
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_mbox");
	if (res == NULL) {
		dev_info(dev, "%s: apu_mbox get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->apu_mbox = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->apu_mbox)) {
		dev_info(dev, "%s: apu_mbox remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(
		pdev, IORESOURCE_MEM, "md32_sysctrl");
	if (res == NULL) {
		dev_info(dev, "%s: md32_sysctrl get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->md32_sysctrl = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->md32_sysctrl)) {
		dev_info(dev, "%s: md32_sysctrl remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(
		pdev, IORESOURCE_MEM, "md32_debug_apb");
	if (res == NULL) {
		dev_info(dev, "%s: md32_debug_apb get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->md32_debug_apb = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->md32_debug_apb)) {
		dev_info(dev, "%s: md32_debug_apb remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_wdt");
	if (res == NULL) {
		dev_info(dev, "%s: apu_wdt get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->apu_wdt = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->apu_wdt)) {
		dev_info(dev, "%s: apu_wdt remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_ao_ctl");
	if (res == NULL) {
		dev_info(dev, "%s: apu_ao_ctl get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->apu_ao_ctl = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->apu_ao_ctl)) {
		dev_info(dev, "%s: apu_ao_ctl remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "md32_tcm");
	if (res == NULL) {
		dev_info(dev, "%s: md32_tcm get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->md32_tcm = devm_ioremap_wc(dev, res->start, res->end - res->start + 1);
	if (IS_ERR((void const *)apu->md32_tcm)) {
		dev_info(dev, "%s: md32_tcm remap base fail\n", __func__);
		return -ENOMEM;
	}
	apu->md32_tcm_sz = res->end - res->start + 1;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "md32_cache_dump");
	if (res == NULL) {
		dev_info(dev, "%s: md32_cache_dump get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->md32_cache_dump = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->md32_cache_dump)) {
		dev_info(dev, "%s: md32_cache_dump remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_rpc");
	if (res == NULL) {
		dev_info(dev, "%s: apu_rpc get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->apu_rpc = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->apu_rpc)) {
		dev_info(dev, "%s: apu_rpc remap base fail\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

static void mt6989_apu_memmap_remove(struct mtk_apu *apu)
{
}

static void mt6989_rv_cg_gating(struct mtk_apu *apu)
{
	iowrite32(0x0, apu->md32_sysctrl + MD32_CLK_EN);
}

static void mt6989_rv_cg_ungating(struct mtk_apu *apu)
{
	iowrite32(0x1, apu->md32_sysctrl + MD32_CLK_EN);
}

static void mt6989_rv_cachedump(struct mtk_apu *apu)
{
	int offset;
	unsigned long flags;

	struct apu_coredump *coredump =
		(struct apu_coredump *) apu->coredump;

	spin_lock_irqsave(&apu->reg_lock, flags);
	/* set APU_UP_SYS_DBG_EN for cache dump enable through normal APB */
	iowrite32(ioread32(apu->md32_sysctrl + DBG_BUS_SEL) |
		APU_UP_SYS_DBG_EN, apu->md32_sysctrl + DBG_BUS_SEL);
	spin_unlock_irqrestore(&apu->reg_lock, flags);

	for (offset = 0; offset < CACHE_DUMP_SIZE/sizeof(uint32_t); offset++)
		coredump->cachedump[offset] =
			ioread32(apu->md32_cache_dump + offset*sizeof(uint32_t));

	spin_lock_irqsave(&apu->reg_lock, flags);
	/* clear APU_UP_SYS_DBG_EN */
	iowrite32(ioread32(apu->md32_sysctrl + DBG_BUS_SEL) &
		~(APU_UP_SYS_DBG_EN), apu->md32_sysctrl + DBG_BUS_SEL);
	spin_unlock_irqrestore(&apu->reg_lock, flags);
}

static void apu_polling_on_work_func(struct work_struct *p_work)
{
	int ret;
	struct mtk_apu *apu = g_apu;
	struct device *dev = apu->dev;

	ret = mt6989_polling_rpc_status(apu, 1, 1);
	if (ret) {
		apu->bypass_pwr_off_chk = true;
		dev_info(dev, "%s: APU_RPC_TOP_CON = 0x%x\n",
			__func__, ioread32(apu->apu_rpc + 0x0));
		dev_info(dev, "%s: APU_RPC_INTF_PWR_RDY = 0x%x\n",
			__func__, ioread32(apu->apu_rpc + APU_RPC_INTF_PWR_RDY));
		dev_info(dev, "%s: MBOX0_RV_PWR_STA = 0x%x\n",
			__func__, ioread32(apu->apu_mbox + MBOX_RV_PWR_STA_FLG));

		apu_coredump_trigger(apu);
	}
}

static int mt6989_apu_power_init(struct mtk_apu *apu)
{
	char wq_name[] = "apusys_rv_pwr";

	/* init delay worker for power off detection */
	INIT_DELAYED_WORK(&timeout_work, apu_timeout_work);
	INIT_DELAYED_WORK(&apu_polling_on_work, &apu_polling_on_work_func);
	apu_workq = alloc_ordered_workqueue(wq_name, WQ_MEM_RECLAIM);
	g_apu = apu;

	return 0;
}

static int mt6989_rproc_init(struct mtk_apu *apu)
{
	int ret;
#if IS_ENABLED(CONFIG_PM_SLEEP)
	apu->wake_lock_ref_cnt = 0;
	spin_lock_init(&apu->wakelock_spinlock);
	ws = wakeup_source_register(NULL, "mt6989_apusys_rv");
	if (!ws) {
		dev_info(apu->dev, "%s: wakelock register fail\n", __func__);
		return -1;
	}

	/* cold boot done will call unlock */
	mt6989_apu_pwr_wake_lock(apu, APU_IPI_INIT);
#endif

	/* TODO: change pwr_on_polling_dbg_mode to false to reduce latency */
	apu->pwr_on_polling_dbg_mode = true;
	apu->ce_dbg_polling_dump_mode = false;
	apu->apusys_rv_trace_on = false;

	is_under_lp_scp_recovery_flow = false;

	/* for cold boot(already power on by apu_top.ko) */
	ret = mt6989_cold_boot_power_on(apu);
	if (ret)
		dev_info(apu->dev, "%s: call mt6989_cold_boot_power_on fail(%d)\n",
			__func__, ret);

	return ret;
}

static int mt6989_rproc_exit(struct mtk_apu *apu)
{
#if IS_ENABLED(CONFIG_PM_SLEEP)
	wakeup_source_unregister(ws);
#endif
	cancel_delayed_work_sync(&apu_polling_on_work);

	return 0;
}

const struct mtk_apu_platdata mt6989_platdata = {
	.flags		= F_AUTO_BOOT | F_FAST_ON_OFF | F_APU_IPI_UT_SUPPORT |
					F_TCM_WA | F_SMMU_SUPPORT |
					F_APUSYS_RV_TAG_SUPPORT | F_PRELOAD_FIRMWARE |
					F_SECURE_BOOT | F_SECURE_COREDUMP | F_CE_EXCEPTION_ON |
					F_EXCEPTION_KE,
	.ops		= {
		.init	= mt6989_rproc_init,
		.exit	= mt6989_rproc_exit,
		.start	= mt6989_rproc_start,
		.stop	= mt6989_rproc_stop,
		.apu_memmap_init = mt6989_apu_memmap_init,
		.apu_memmap_remove = mt6989_apu_memmap_remove,
		.power_on_off = mt6989_power_on_off,
		.polling_rpc_status = mt6989_polling_rpc_status,
		.wake_lock = mt6989_apu_pwr_wake_lock,
		.wake_unlock = mt6989_apu_pwr_wake_unlock,
		.debug_info_dump = mt6989_debug_info_dump,
		.cg_gating = mt6989_rv_cg_gating,
		.cg_ungating = mt6989_rv_cg_ungating,
		.rv_cachedump = mt6989_rv_cachedump,
		.power_init = mt6989_apu_power_init,
		.ipi_send_pre = mt6989_ipi_send_pre,
		.irq_affin_init = mt6989_irq_affin_init,
		.irq_affin_set = mt6989_irq_affin_set,
		.irq_affin_unset = mt6989_irq_affin_unset,
		.irq_affin_clear = mt6989_irq_affin_clear,
		.check_apu_exp_irq = mt6989_check_apu_exp_irq,
	},
};
