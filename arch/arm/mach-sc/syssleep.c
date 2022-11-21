/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <soc/sprd/hardware.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/adi.h>

#ifdef CONFIG_64BIT
#define RTC_BASE (SPRD_ADI_BASE + 0x8080)
#else
#define RTC_BASE ANA_RTC_BASE
#endif
#define ANA_RTC_INT_EN			(RTC_BASE + 0x30)
#define ANA_RTC_INT_CLR			(RTC_BASE + 0x38)
#define RTC_INT_ALL_MSK			(0xFFFF&(~(BIT(5)|BIT(7))))

/* workaround to disable rtc alarm after AT+SYSSLEEP command */
bool factory_syssleep = 0;

static int syssleep_read_proc(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	return 0;
}

static int syssleep_write_proc(struct file *file, const char __user *buffer,
			   unsigned long count, void *data)
{
#if 0
    sci_glb_write(REG_PMU_APB_PD_CP0_ARM9_0_CFG, BIT(24) | BIT(25), -1UL);
    sci_glb_write(REG_PMU_APB_PD_CP0_HU3GE_CFG, BIT(24) | BIT(25), -1UL);
    sci_glb_write(REG_PMU_APB_PD_CP0_GSM_CFG, BIT(24) | BIT(25), -1UL);
    sci_glb_write(REG_PMU_APB_PD_CP0_L1RAM_CFG, BIT(24) | BIT(25), -1UL);
    sci_glb_write(REG_PMU_APB_PD_CP0_SYS_CFG, BIT(25) | BIT(28), -1UL);
#else
	factory_syssleep = 1;
	printk("syssleep_write_proc was called begin.\n");
    sci_glb_write(REG_PMU_APB_PD_CP0_ARM9_0_CFG, BIT(24) | BIT(25), -1UL);
    sci_glb_write(REG_PMU_APB_PD_CP0_HU3GE_CFG, BIT(24) | BIT(25), -1UL);
    sci_glb_write(REG_PMU_APB_PD_CP0_GSM_CFG, BIT(24) | BIT(25), -1UL);
    sci_glb_write(REG_PMU_APB_PD_CP0_TD_CFG, BIT(24) | BIT(25), -1UL);
    sci_glb_write(REG_PMU_APB_PD_CP0_CEVA_CFG, BIT(24) | BIT(25), -1UL);
    sci_glb_write(REG_PMU_APB_PD_CP0_SYS_CFG, BIT(25) | BIT(28), -1UL);
    sci_glb_write(REG_PMU_APB_SLEEP_CTRL,BIT(12),BIT(12));
#endif
	/*disable and clean irq*/
	sci_adi_clr(ANA_RTC_INT_EN, 0xffff);
	sci_adi_raw_write(ANA_RTC_INT_CLR, RTC_INT_ALL_MSK);
	
	printk("syssleep_write_proc was called end, RTC_EN=0x%08x, RTC_CLEAR=0x%08x.\n",
	sci_adi_read(ANA_RTC_INT_EN), sci_adi_read(ANA_RTC_INT_CLR));
    //sci_glb_write(REG_PMU_APB_CP_SOFT_RST,0x7,-1UL);
    //msleep(1000);
    //sci_glb_write(REG_PMU_APB_CP_SOFT_RST,0x6,-1UL);
    return count;
}

static const struct file_operations syssleep_proc_fops = {
    .owner      = THIS_MODULE,
    .read       = syssleep_read_proc,
    .write      = syssleep_write_proc,
};

static int __init syssleep_init(void)
{
    printk("syssleep_init \n");
    proc_create("syssleep", 0777, NULL, &syssleep_proc_fops);
	return 0;
}

module_init(syssleep_init);
