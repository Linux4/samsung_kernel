/*
 * Copyright (C) 2010-2011 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file mali_platform.c
 * Platform specific Mali driver functions for a default platform
 */

#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/pm.h>
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#include <asm/io.h>
#include <linux/mali/mali_utgard.h>
	 
#include <mach/globalregs.h>
#include "mali_kernel_common.h"
#include "mali_kernel_linux.h"
#include "base.h"

#define SPRD_MALI_PHYS 0xA0010000
#define IRQ_G3D_INT 25
	 
void mali_platform_device_release(struct device *device);
void mali_platform_utilization(unsigned int);

static struct resource mali_gpu_resources[] =
{
	MALI_GPU_RESOURCES_MALI300_PMU(SPRD_MALI_PHYS, IRQ_G3D_INT, IRQ_G3D_INT,
													IRQ_G3D_INT, IRQ_G3D_INT)
};

static struct mali_gpu_device_data mali_gpu_data =
{
	.shared_mem_size = ARCH_MALI_MEMORY_SIZE_DEFAULT,
	.utilization_interval = 300,
	.utilization_handler = mali_platform_utilization,
};

static struct platform_device mali_gpu_device =
{
	.name = MALI_GPU_NAME_UTGARD,
	.id = 0,
	.num_resources = ARRAY_SIZE(mali_gpu_resources),
	.resource = mali_gpu_resources,
	.dev.platform_data = &mali_gpu_data,
	.dev.release = mali_platform_device_release,
};

static int g_gpu_clock_on = 0;
static int g_gpu_power_on = 0;

int mali_platform_device_register(void)
{
	int err = -1;

	MALI_DEBUG_PRINT(4, ("mali_platform_device_register() called\n"));

	if(!g_gpu_power_on)
	{
		g_gpu_power_on = 1;
		sprd_greg_clear_bits(REG_TYPE_GLOBAL, BIT(23), GR_G3D_PWR_CTRL);
		udelay(300);
	}
	if(!g_gpu_clock_on)
	{
		g_gpu_clock_on = 1;
		sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL, BIT(21), AHB_CTL0);
	}

	err = platform_device_register(&mali_gpu_device);
	if (0 == err)
	{
#ifdef CONFIG_PM_RUNTIME
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
		pm_runtime_set_autosuspend_delay(&(mali_gpu_device.dev), 50);
		pm_runtime_use_autosuspend(&(mali_gpu_device.dev));
#endif
		pm_runtime_enable(&(mali_gpu_device.dev));
#endif
 
		return 0;
	}

	platform_device_unregister(&mali_gpu_device);
 
	if(g_gpu_clock_on)
	{
		g_gpu_clock_on = 0;
		sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL, BIT(21), AHB_CTL0);
	}
	if(g_gpu_power_on)
	{
		g_gpu_power_on = 0;
		sprd_greg_set_bits(REG_TYPE_GLOBAL, BIT(23), GR_G3D_PWR_CTRL);
	}
 
	return err;
}
 
void mali_platform_device_unregister(void)
{
	MALI_DEBUG_PRINT(4, ("mali_platform_device_unregister() called\n"));

	platform_device_unregister(&mali_gpu_device);
 
	if(g_gpu_clock_on)
	{
		g_gpu_clock_on = 0;
		sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL, BIT(21), AHB_CTL0);
	}
	if(g_gpu_power_on)
	{
		g_gpu_power_on = 0;
		sprd_greg_set_bits(REG_TYPE_GLOBAL, BIT(23), GR_G3D_PWR_CTRL);
	}
}

void mali_platform_device_release(struct device *device)
{
	MALI_DEBUG_PRINT(4, ("mali_platform_device_release() called\n"));
}

void mali_platform_power_mode_change(int power_mode)
{
	switch(power_mode)
	{
	case 0://MALI_POWER_MODE_ON:
		sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL, BIT(21), AHB_CTL0);
		break;
	case 1://MALI_POWER_MODE_LIGHT_SLEEP:
		sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL, BIT(21), AHB_CTL0);
		break;
	case 2://MALI_POWER_MODE_DEEP_SLEEP:
		sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL, BIT(21), AHB_CTL0);
		break;
	};
}
	
void mali_platform_utilization(u32 utilization)
{
}

