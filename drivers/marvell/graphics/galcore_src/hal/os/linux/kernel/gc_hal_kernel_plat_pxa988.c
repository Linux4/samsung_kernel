/*
 * gc_hal_kernel_plat_pxa988.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. *
 */

#include "gc_hal_kernel_plat_common.h"
#include <linux/kallsyms.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>

#define __PLAT_APINAME(apiname)     pxa988_##apiname

#if defined(CONFIG_ARM64)
#define PARAM_TYPE_PWR unsigned int
#else
#define PARAM_TYPE_PWR int
#endif

#define chain_num_3D gcmCOUNTOF(gc3d_iface_chains)

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
#define clk_3D_name "GC3DFCLK"
#define clk_2D_name "GC2DFCLK"
#define clk_sh_name "GCSHCLK"
#define chain_num_2D gcmCOUNTOF(gc2d_iface_chains)
#else
#define clk_3D_name "GCCLK"
#define clk_2D_name "GC2DCLK"
#define clk_sh_name "GC_SHADER_CLK"
#define chain_num_2D 0
#endif

typedef int (*PFUNC_GC_PWR)(PARAM_TYPE_PWR);

static PFUNC_GC_PWR GC3D_PWR = gcvNULL;
static PFUNC_GC_PWR GC2D_PWR = gcvNULL;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
/**
* gc3D AXI CLK definition
*/
static struct gc_ops gc3daxi_ops = {
    .init       = gpu_lock_init_dft,
    .enableclk  = gpu_clk_enable_dft,
    .disableclk = gpu_clk_disable_dft,
    .setclkrate = gpu_clk_setrate_dft,
    .getclkrate = gpu_clk_getrate_dft,
};

static struct gc_iface gc3daxi_iface = {
    .name               = "gc3da",
    .con_id             = "GC3DACLK",
    .ops                = &gc3daxi_ops,
};
#endif

/**
 * gc3d shader definition
 */
static struct gc_ops gc3dsh_ops = {
    .init       = gpu_lock_init_dft,
    .enableclk  = gpu_clk_enable_dft,
    .disableclk = gpu_clk_disable_dft,
    .setclkrate = gpu_clk_setrate_dft,
    .getclkrate = gpu_clk_getrate_dft,
};

static struct gc_iface gc3dsh_iface = {
    .name               = "gc3dsh",
    .con_id             = clk_sh_name,
    .ops                = &gc3dsh_ops,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
    .axi_clk            = &gc3daxi_iface,
#endif
};

/**
 * gc3d definition
 */

static int __PLAT_APINAME(gc3d_pwr_ops)(struct gc_iface *iface, unsigned int enabled)
{
    int retval = 0;
    struct device *dev_GC3d;
    PR_DEBUG("[%6s] %s %d\n", iface->name, __func__, enabled);

    if(has_feat_power_domain())
    {
        if(!iface->pdev)
        {
            printk("%s(%d)Invalidate platform device, power %s failed\n",
                __func__,
                __LINE__,
                enabled?"on":"off");
            return -ENODEV;
        }

        dev_GC3d = &(((struct platform_device*)(iface->pdev))->dev);

        if(enabled)
        {
            trace_printk("%s usage_count: before get %d\n", iface->name, atomic_read(&dev_GC3d->power.usage_count));
            retval = pm_runtime_get_sync((&(((struct platform_device*)(iface->pdev))->dev)));
            trace_printk("%s usage_count: after get %d, retval %d\n", iface->name, atomic_read(&dev_GC3d->power.usage_count), retval);
            BUG_ON(atomic_read(&dev_GC3d->power.usage_count) <= 0);
        }
        else
        {
            trace_printk("%s usage_count: before put %d\n", iface->name, atomic_read(&dev_GC3d->power.usage_count));
            BUG_ON(atomic_read(&dev_GC3d->power.usage_count) <= 0);
            retval = pm_runtime_put_sync((&(((struct platform_device*)(iface->pdev))->dev)));
            trace_printk("%s usage_count: after put %d, retval %d\n", iface->name, atomic_read(&dev_GC3d->power.usage_count), retval);
        }
    }
    else
    {
        if(GC3D_PWR == gcvNULL)
        {
#ifdef CONFIG_KALLSYMS
            GC3D_PWR = (PFUNC_GC_PWR)kallsyms_lookup_name("gc_pwr");

#elif (defined CONFIG_ARM64) || (defined CONFIG_CPU_EDEN) || (defined CONFIG_CPU_PXA1928)

            extern void gc3d_pwr(unsigned int);

            GC3D_PWR = gc3d_pwr;
#elif (defined CONFIG_CPU_PXA988)

            extern void gc_pwr(PARAM_TYPE_PWR);

            GC3D_PWR = gc_pwr;
#else
            gcmkPRINT("GC3D_PWR not implemented!");
            return -EINVAL;
#endif
        }

        GC3D_PWR((PARAM_TYPE_PWR)enabled);
    }

    return retval;
}

static struct gc_ops gc3d_ops = {
    .init       = gpu_lock_init_dft,
    .enableclk  = gpu_clk_enable_dft,
    .disableclk = gpu_clk_disable_dft,
    .setclkrate = gpu_clk_setrate_dft,
    .getclkrate = gpu_clk_getrate_dft,
    .pwrops     = __PLAT_APINAME(gc3d_pwr_ops),
};

static struct gc_iface *gc3d_iface_chains[] = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
    &gc3daxi_iface,
#endif
    &gc3dsh_iface,
};

static struct gc_iface gc3d_iface = {
    .name               = "gc3d",
    .con_id             = clk_3D_name,
    .ops                = &gc3d_ops,
    .chains_count       = chain_num_3D,
    .chains_clk         = gc3d_iface_chains,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
    .axi_clk            = &gc3daxi_iface,
#endif
};

/**
 * gc2d definition
 */
static int __PLAT_APINAME(gc2d_pwr_ops)(struct gc_iface *iface, unsigned int enabled)
{
    int retval = 0;
    struct device *dev_gc2d;
    PR_DEBUG("[%6s] %s %d\n", iface->name, __func__, enabled);

    if(has_feat_power_domain())
    {
        if(!iface->pdev)
        {
            printk("%s(%d)Invalidate platform device, power %s failed\n",
                __func__,
                __LINE__,
                enabled?"on":"off");
            return -ENODEV;
        }

        dev_gc2d = &(((struct platform_device*)(iface->pdev))->dev);

        if(enabled)
        {
            trace_printk("%s usage_count: before get %d\n", iface->name, atomic_read(&dev_gc2d->power.usage_count));
            retval = pm_runtime_get_sync((&(((struct platform_device*)(iface->pdev))->dev)));
            trace_printk("%s usage_count: after get %d, retval %d\n", iface->name, atomic_read(&dev_gc2d->power.usage_count), retval);
            BUG_ON(atomic_read(&dev_gc2d->power.usage_count) <= 0);
        }
        else
        {
            trace_printk("%s usage_count: before put %d\n", iface->name, atomic_read(&dev_gc2d->power.usage_count));
            BUG_ON(atomic_read(&dev_gc2d->power.usage_count) <= 0);
            retval = pm_runtime_put_sync((&(((struct platform_device*)(iface->pdev))->dev)));
            trace_printk("%s usage_count: after put %d, retval %d\n", iface->name, atomic_read(&dev_gc2d->power.usage_count), retval);
        }
    }
    else
    {
        if(GC2D_PWR == gcvNULL)
        {
#ifdef CONFIG_KALLSYMS
            GC2D_PWR = (PFUNC_GC_PWR)kallsyms_lookup_name("gc2d_pwr");

#elif (defined CONFIG_ARM64) || (defined CONFIG_CPU_EDEN) || (defined CONFIG_CPU_PXA1928)

            extern void gc2d_pwr(unsigned int);

            GC2D_PWR = gc2d_pwr;
#elif (defined CONFIG_CPU_PXA988)

            extern void gc2d_pwr(PARAM_TYPE_PWR);

            GC2D_PWR = gc2d_pwr;
#else
            gcmkPRINT("GC2D_PWR not implemented!");
            return -EINVAL;
#endif
        }

        GC2D_PWR((PARAM_TYPE_PWR)enabled);
    }

    return retval;
}

static struct gc_ops gc2d_ops = {
    .init       = gpu_lock_init_dft,
    .enableclk  = gpu_clk_enable_dft,
    .disableclk = gpu_clk_disable_dft,
    .setclkrate = gpu_clk_setrate_dft,
    .getclkrate = gpu_clk_getrate_dft,
    .pwrops     = __PLAT_APINAME(gc2d_pwr_ops),
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
static struct gc_iface *gc2d_iface_chains[] = {
    &gc3daxi_iface,
};
#endif

static struct gc_iface gc2d_iface = {
    .name               = "gc2d",
    .con_id             = clk_2D_name,
    .ops                = &gc2d_ops,
    .chains_count       = chain_num_2D,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
    .chains_clk         = gc2d_iface_chains,
    .axi_clk            = &gc3daxi_iface,
#endif
};

struct gc_iface *pxa988_gc_ifaces[] = {
    [gcvCORE_MAJOR] = &gc3d_iface,
    [gcvCORE_2D]    = &gc2d_iface,
    [gcvCORE_SH]    = &gc3dsh_iface,
    gcvNULL,
};

