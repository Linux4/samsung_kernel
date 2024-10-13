/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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
#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <asm/irqflags.h>
//#include <asm/hardware/cache-l2x0.h>
#include <asm/cacheflush.h>
#include <soc/sprd/system.h>
#include <soc/sprd/pm_debug.h>
#include <soc/sprd/common.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/sci_glb_regs.h>
//#include <mach/irqs.h>
#include <soc/sprd/sci.h>
//#include "emc_repower.h"
#include <linux/clockchips.h>
#include <linux/wakelock.h>
#include <soc/sprd/adi.h>
#include <soc/sprd/arch_misc.h>
#include <asm/suspend.h>
#include <asm/barrier.h>
#include <linux/cpu_pm.h>
#if defined(CONFIG_ARCH_SCX35L64)||defined(CONFIG_ARCH_SCX35LT8)
#include <soc/sprd/sprd_persistent_clock.h>
#endif
#if defined(CONFIG_SPRD_DEBUG)
/* For saving Fault status */
#include <soc/sprd/sprd_debug.h>
#endif
extern void (*arm_pm_restart)(char str, const char *cmd);
static void adie_power_off(void)
{
	u32 reg_val;
	/*turn off all modules's ldo*/
#if defined(CONFIG_ADIE_SC2731)
sci_adi_write(ANA_REG_GLB_PWR_WR_PROT_VALUE,BITS_PWR_WR_PROT_VALUE(0x6e7F),BITS_PWR_WR_PROT_VALUE(~0));
        while(!(sci_adi_read(ANA_REG_GLB_PWR_WR_PROT_VALUE)&BIT_PWR_WR_PROT))
		printk("powerdown wait key\n");
      sci_adi_set(ANA_REG_GLB_POWER_PD_HW,BIT_PWR_OFF_SEQ_EN);
      //auto poweroff by chip
#else
#ifdef CONFIG_SPRD_EXT_IC_POWER
	sprd_extic_otg_power(0);
#endif
	sci_adi_raw_write(ANA_REG_GLB_LDO_PD_CTRL, 0x1fff);

#if defined(CONFIG_ARCH_SCX15) || defined(CONFIG_ADIE_SC2723) || defined(CONFIG_ADIE_SC2723S)
	sci_adi_raw_write(ANA_REG_GLB_PWR_WR_PROT_VALUE,BITS_PWR_WR_PROT_VALUE(0x6e7f));
	do{
		reg_val = (sci_adi_read(ANA_REG_GLB_PWR_WR_PROT_VALUE) & BIT_PWR_WR_PROT);
	}while(reg_val == 0);
	sci_adi_raw_write(ANA_REG_GLB_LDO_PD_CTRL,0xfff);
	sci_adi_raw_write(ANA_REG_GLB_LDO_DCDC_PD,0x7fff);
#endif

#if !defined(CONFIG_64BIT)
#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
	/*turn off system core's ldo*/
	sci_adi_raw_write(ANA_REG_GLB_LDO_DCDC_PD_RTCCLR, 0x0);
	sci_adi_raw_write(ANA_REG_GLB_LDO_DCDC_PD_RTCSET, 0X7fff);
#endif
#endif
#endif

}

static void machine_restart(char mode, const char *cmd)
{
	local_irq_disable();
	local_fiq_disable();

	arch_reset(mode, cmd);

	mdelay(1000);

	printk("reboot failed!\n");

	while (1);
}

void __init sc_poweroff_init(void)
{
	pm_power_off   = adie_power_off;
	arm_pm_restart = machine_restart;
}

subsys_initcall(sc_poweroff_init);
