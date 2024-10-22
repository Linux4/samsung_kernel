// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <mt-plat/aee.h>
#include <linux/interrupt.h>
#include <linux/io.h>
//#include <mt-plat/sync_write.h>
#include "scp_ipi_pin.h"
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp_dvfs.h"
#include "sap.h"

static inline void scp_wdt_clear(uint32_t coreid)
{
	coreid == 0 ? writel(B_WDT_IRQ, R_CORE0_WDT_IRQ) :
		writel(B_WDT_IRQ, R_CORE1_WDT_IRQ);
}

static void wait_scp_ready_to_reboot(void)
{
	int retry = 0;
	unsigned long c0, c1;
	unsigned long core_sap = CORE_RDY_TO_REBOOT;

	/* clr after SCP side INT trigger,
	 * or SCP may lost INT max wait = 200ms
	 */
	for (retry = SCP_AWAKE_TIMEOUT; retry > 0; retry--) {
		c0 = readl(SCP_GPR_CORE0_REBOOT);
		c1 = scpreg.core_nums == 2 ? readl(SCP_GPR_CORE1_REBOOT) :
			CORE_RDY_TO_REBOOT;
		if (sap_enabled())
			core_sap = sap_cfg_reg_read(CFG_GPR5_OFFSET);
		if ((c0 == CORE_RDY_TO_REBOOT) && (c1 == CORE_RDY_TO_REBOOT)
			&& core_sap == CORE_RDY_TO_REBOOT)
			break;
		udelay(2);
	}

	if (retry == 0)
		pr_notice("[SCP] SCP don't stay in wfi c0:%lx c1:%lx sap:%lx\n", c0, c1, core_sap);
	udelay(10);
}

/*
 * handler for wdt irq for scp
 * dump scp register
 */
static void scp_A_wdt_handler(struct tasklet_struct *t)
{
	unsigned int reg0 = readl(R_CORE0_WDT_IRQ);
	unsigned int reg1 = scpreg.core_nums == 2 ?
		readl(R_CORE1_WDT_IRQ) : 0;
	unsigned int reg_sap = 0;

#if SCP_RECOVERY_SUPPORT
	if (scp_set_reset_status() == RESET_STATUS_STOP) {
		pr_debug("[SCP] start to reset scp...\n");
		scp_dump_last_regs();
		sap_dump_last_regs();
		scp_send_reset_wq(RESET_TYPE_WDT);
	} else
		pr_notice("%s: scp resetting\n", __func__);
#endif
	wait_scp_ready_to_reboot();
	if (sap_enabled())
		reg_sap = sap_cfg_reg_read(CFG_WDT_IRQ_OFFSET);

#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM)
	if (scpreg.secure_dump) {
		if (reg0)
			scp_do_wdt_clear(0);
		if (reg1)
			scp_do_wdt_clear(1);
		if (reg_sap)
			scp_do_wdt_clear(sap_get_core_id());
	} else
#endif
	{
		if (reg0)
			scp_wdt_clear(0);
		if (reg1)
			scp_wdt_clear(1);
		if (reg_sap)
			sap_cfg_reg_write(CFG_WDT_IRQ_OFFSET, B_WDT_IRQ);
	}
	enable_irq(t->data);
}

DECLARE_TASKLET(scp_A_irq0_tasklet, scp_A_wdt_handler);
DECLARE_TASKLET(scp_A_irq1_tasklet, scp_A_wdt_handler);

/*
 * dispatch scp irq
 * reset scp and generate exception if needed
 * @param irq:      irq id
 * @param dev_id:   should be NULL
 */
irqreturn_t scp_A_irq_handler(int irq, void *dev_id)
{
	disable_irq_nosync(irq);
	if (likely(irq == scpreg.irq0))
		tasklet_schedule(&scp_A_irq0_tasklet);
	else
		tasklet_schedule(&scp_A_irq1_tasklet);
	return IRQ_HANDLED;
}

