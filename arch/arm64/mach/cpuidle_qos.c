/* arch/arm64/mach/cpuidle_qos.c
 *
 *  Copyright (C) 2014 Marvell, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/cpu.h>
#include <linux/pm_qos.h>
#include <linux/sysfs.h>
#include <linux/sched.h>
#include <linux/stat.h>

#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/cputype.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <asm/mcpm.h>
#include <asm/mcpm_plat.h>
#include "regs-addr.h"
#include "pxa1936_lowpower.h"
#include <linux/pxa1936_powermode.h>


#define APCR_PER_PORTS_SET	\
	(PMUM_AXISD | PMUM_SLPEN | PMUM_DDRCORSD | PMUM_APBSD | PMUM_VCTCXOSD | PMUM_STBYEN)

#define APCR_PER_PORTS_ALWAYS_ON \
	(PMUM_DTCMSD | PMUM_BBSD | PMUM_MSASLPEN)

/* ========================= Wakeup ports settings=========================== */
#define DISABLE_ALL_WAKEUP_PORTS		\
	(PMUM_SLPWP0 | PMUM_SLPWP1 | PMUM_SLPWP2 | PMUM_SLPWP3 |	\
	 PMUM_SLPWP4 | PMUM_SLPWP5 | PMUM_SLPWP6 | PMUM_SLPWP7)

static void __iomem *mpmu_virt_addr;

/* sync hotplug policy and qos request */
static DEFINE_SPINLOCK(cpuidle_apcr_qos_lock);

int cpuidle_setmode(int qos_min)
{
	u32 apcr_per = readl_relaxed(mpmu_virt_addr + APCR_PER);
	u32 apcr_clear = 0, apcr_set = 0;

	switch (qos_min) {
	case PM_QOS_CPUIDLE_BLOCK_C1:
	case PM_QOS_CPUIDLE_BLOCK_C2:
	case PM_QOS_CPUIDLE_BLOCK_M2:
			/* fall through */
			/* disable D1P mode, POWER_MODE_APPS_IDLE */
	case PM_QOS_CPUIDLE_BLOCK_AXI:
			apcr_clear |= PMUM_AXISD;

			apcr_clear |= PMUM_SLPEN;
			apcr_clear |= PMUM_DDRCORSD;
			apcr_clear |= PMUM_APBSD;
			/* UDR mode, POWER_MODE_UDR*/
			apcr_clear |= PMUM_STBYEN;
			/* D2 mode, POWER_MODE_UDR_VCTCXO */
			apcr_clear |= PMUM_VCTCXOSD;
			break;
			/* disable D1 mode POWER_MODE_SYS_SLEEP */
	case PM_QOS_CPUIDLE_BLOCK_DDR:
			apcr_clear |= PMUM_SLPEN;
			apcr_clear |= PMUM_DDRCORSD;
			apcr_clear |= PMUM_APBSD;
			/* UDR mode, POWER_MODE_UDR */
			apcr_clear |= PMUM_STBYEN;
			/* D2 mode, POWER_MODE_UDR_VCTCXO */
			apcr_clear |= PMUM_VCTCXOSD;

			/* if only disable D1 mode, set the AXISD bit */
			apcr_set |= PMUM_AXISD;
			break;
			/* disable D2 mode, PM_QOS_CPUIDLE_BLOCK_UDR */
	case PM_QOS_CPUIDLE_BLOCK_UDR:
			/* UDR mode, POWER_MODE_UDR */
			apcr_clear |= PMUM_STBYEN;
			/* disable D2 mode */
			apcr_set |= PMUM_VCTCXOSD;
			apcr_set |= PMUM_DDRCORSD;
			apcr_set |= PMUM_SLPEN;
			apcr_set |= PMUM_APBSD;
			apcr_set |= PMUM_AXISD;
			break;
		    /* set all need vote apcr_per bit  */
	case PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE:
			apcr_set |= APCR_PER_PORTS_SET;
			break;
	default:
		WARN(1, "Invalid cpuidle block qos!\n");
	}

	/* clear apcr_per bit */
	apcr_per &= ~(apcr_clear);
	/* set apcr_per bit */
	apcr_per |= apcr_set;

	writel_relaxed(apcr_per, mpmu_virt_addr + APCR_PER);
	/* RAW for barrier */
	apcr_per = readl_relaxed(mpmu_virt_addr + APCR_PER);
	return 0;

}

static int cpuidle_notify(struct notifier_block *b, unsigned long val, void *v)
{
	s32 ret;
	unsigned long flags = 0;

	spin_lock_irqsave(&cpuidle_apcr_qos_lock, flags);
	ret = cpuidle_setmode(val);
	spin_unlock_irqrestore(&cpuidle_apcr_qos_lock, flags);
	return NOTIFY_OK;
}

static struct notifier_block cpuidle_notifier = {
	.notifier_call = cpuidle_notify,
};

void lowpower_init(void)
{
	u32 apcr_per, apcr_cluster0, apcr_cluster1, apslpw;
	mpmu_virt_addr = regs_addr_get_va(REGS_ADDR_MPMU);
	apcr_per = readl_relaxed(mpmu_virt_addr + APCR_PER);
	apcr_per |= APCR_PER_PORTS_ALWAYS_ON;
	apcr_per |= APCR_PER_PORTS_SET;
	writel_relaxed(apcr_per, mpmu_virt_addr + APCR_PER);

	apcr_cluster0 = readl_relaxed(mpmu_virt_addr + APCR_CLUSTER0);
	apcr_cluster0 |= APCR_PER_PORTS_ALWAYS_ON;
	apcr_cluster0 &= ~(PMUM_STBYEN);
	writel_relaxed(apcr_cluster0, mpmu_virt_addr + APCR_CLUSTER0);

	apcr_cluster1 = readl_relaxed(mpmu_virt_addr + APCR_CLUSTER1);
	apcr_cluster1 |= APCR_PER_PORTS_ALWAYS_ON;
	apcr_cluster1 &= ~(PMUM_STBYEN);
	writel_relaxed(apcr_cluster1, mpmu_virt_addr + APCR_CLUSTER1);

	apslpw = readl_relaxed(mpmu_virt_addr + APSLPW);
	apslpw &= ~(DISABLE_ALL_WAKEUP_PORTS);
	writel_relaxed(apslpw, mpmu_virt_addr + APSLPW);

	return;
}


static int __init cpuidle_qos_init(void)
{
	int ret = 0;

	if (!of_machine_is_compatible("marvell,pxa1936"))
		return -ENODEV;

	pr_info("%s, cpuidle qos init function\n", __func__);
	lowpower_init();

	ret = pm_qos_add_notifier(PM_QOS_CPUIDLE_BLOCK, &cpuidle_notifier);
	if (ret) {
		pr_err("register cpuidle qos notifier failed\n");
		return ret;
	}

	return ret;
}

module_init(cpuidle_qos_init);
