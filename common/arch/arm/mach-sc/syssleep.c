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
#include <mach/hardware.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>

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
    sci_glb_write(REG_PMU_APB_PD_CP0_ARM9_0_CFG, BIT(24) | BIT(25), -1UL);
    sci_glb_write(REG_PMU_APB_PD_CP0_HU3GE_CFG, BIT(24) | BIT(25), -1UL);
    sci_glb_write(REG_PMU_APB_PD_CP0_GSM_CFG, BIT(24) | BIT(25), -1UL);
    sci_glb_write(REG_PMU_APB_PD_CP0_TD_CFG, BIT(24) | BIT(25), -1UL);
    sci_glb_write(REG_PMU_APB_PD_CP0_CEVA_CFG, BIT(24) | BIT(25), -1UL);
    sci_glb_write(REG_PMU_APB_PD_CP0_SYS_CFG, BIT(25) | BIT(28), -1UL);
    sci_glb_write(REG_PMU_APB_SLEEP_CTRL,BIT(12),BIT(12));
#endif
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
