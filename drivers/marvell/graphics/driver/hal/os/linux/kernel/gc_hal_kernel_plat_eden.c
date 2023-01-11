/*
 * gc_hal_kernel_plat_eden.c
 *
 * Author: Watson Wang <zswang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. *
 */

#include "gc_hal_kernel_plat_common.h"
#include <linux/kallsyms.h>

#define __PLAT_APINAME(apiname)     eden_##apiname

typedef int (*PFUNC_GC_PWR)(unsigned int);

static PFUNC_GC_PWR GC3D_PWR = gcvNULL;
static PFUNC_GC_PWR GC2D_PWR = gcvNULL;

static struct _clk_restore {
    unsigned int power_enabled;
    unsigned long clk_rate;
}clk_restore[3];

int eden_gpu_clk_setrate(struct gc_iface *iface, unsigned long rate_khz);
unsigned long eden_gpu_clk_getrate(struct gc_iface *iface);
int eden_gpu_clk_enable(struct gc_iface *iface);

/**
 * gc3d shader definition
 */
static struct gc_ops gc3dsh_ops = {
    .init       = gpu_lock_init_dft,
    .enableclk  = gpu_clk_enable_dft,
    .disableclk = gpu_clk_disable_dft,
    .setclkrate = eden_gpu_clk_setrate,
    .getclkrate = eden_gpu_clk_getrate,
};

static struct gc_iface gc3dsh_iface = {
    .name               = "gc3dsh",
    .con_id             = "GC3D_CLK2X",
    .ops                = &gc3dsh_ops,
};

/**
 * gc3d definition
 */
static int __PLAT_APINAME(gc3d_pwr_ops)(struct gc_iface *iface, unsigned int enabled)
{
    int retval = 0;
    PR_DEBUG("[%6s] %s %d\n", iface->name, __func__, enabled);
    if (has_feat_freq_change_indirect())
    {
        unsigned int count = iface->chains_count;

        clk_restore[gcvCORE_MAJOR].power_enabled = enabled;
        clk_restore[gcvCORE_SH].power_enabled = enabled;

        if (!enabled)
        {
            clk_restore[gcvCORE_MAJOR].clk_rate = gpu_clk_getrate_dft(iface);
            clk_restore[gcvCORE_SH].clk_rate = gpu_clk_getrate_dft(&gc3dsh_iface);
        }

        /* Make it safe to do power operations */
        gpu_clk_setrate_dft(iface, 156000);

        while(count-- != 0)
            gpu_clk_setrate_dft(iface->chains_clk[count], 156000);
    }

    if(GC3D_PWR == gcvNULL)
    {
#ifdef CONFIG_KALLSYMS
        GC3D_PWR = (PFUNC_GC_PWR)kallsyms_lookup_name("gc3d_pwr");

#elif (defined CONFIG_ARM64) || (defined CONFIG_CPU_EDEN) || (defined CONFIG_CPU_PXA1928)

        extern void gc3d_pwr(unsigned int);

        GC3D_PWR = gc3d_pwr;
#else
        gcmkPRINT("GC3D_PWR not implemented!");
        return -EINVAL;
#endif
    }

    GC3D_PWR(enabled);

    if (has_feat_freq_change_indirect())
    {
        if (enabled)
        {
            /* Delay 10 us to wait for chip stable. */
            gckOS_Udelay(gcvNULL, 10);

            /* restore clock */
            eden_gpu_clk_setrate(&gc3dsh_iface, clk_restore[gcvCORE_SH].clk_rate);
            eden_gpu_clk_setrate(iface, clk_restore[gcvCORE_MAJOR].clk_rate);
        }
    }

    return retval;
}

static struct gc_ops gc3d_ops = {
    .init       = gpu_lock_init_dft,
    .enableclk  = gpu_clk_enable_dft,
    .disableclk = gpu_clk_disable_dft,
    .setclkrate = eden_gpu_clk_setrate,
    .getclkrate = eden_gpu_clk_getrate,
    .pwrops     = __PLAT_APINAME(gc3d_pwr_ops),
};

static struct gc_iface *gc3d_iface_chains[] = {
    &gc3dsh_iface,
};

static struct gc_iface gc3d_iface = {
    .name               = "gc3d",
    .con_id             = "GC3D_CLK1X",
    .ops                = &gc3d_ops,
    .chains_count       = 1,
    .chains_clk         = gc3d_iface_chains,
};

/**
 * gc2d definition
 */
static int __PLAT_APINAME(gc2d_pwr_ops)(struct gc_iface *iface, unsigned int enabled)
{
    int retval = 0;
    PR_DEBUG("[%6s] %s %d\n", iface->name, __func__, enabled);
    if (has_feat_freq_change_indirect())
    {
        clk_restore[gcvCORE_2D].power_enabled = enabled;

        if (!enabled &&
            !has_feat_freq_change_when_idle())
        {
            clk_restore[gcvCORE_2D].clk_rate = gpu_clk_getrate_dft(iface);
        }

        /* make it safe to do Power operation */
        gpu_clk_setrate_dft(iface, 156000);
    }

    if(GC2D_PWR == gcvNULL)
    {
#ifdef CONFIG_KALLSYMS
        GC2D_PWR = (PFUNC_GC_PWR)kallsyms_lookup_name("gc2d_pwr");

#elif (defined CONFIG_ARM64) || (defined CONFIG_CPU_EDEN) || (defined CONFIG_CPU_PXA1928)

extern void gc2d_pwr(unsigned int);

        GC2D_PWR = gc2d_pwr;
#else
        gcmkPRINT("GC2D_PWR not implemented!");
        return -EINVAL;
#endif
    }

    GC2D_PWR(enabled);

    if (has_feat_freq_change_indirect() &&
        !has_feat_freq_change_when_idle())
    {
        if (enabled)
        {
            /* Delay 10 us to wait for chip stable. */
            gckOS_Udelay(gcvNULL, 10);

            /* restore clock */
            eden_gpu_clk_setrate(iface, clk_restore[gcvCORE_2D].clk_rate);
        }
    }

    return retval;
}

static struct gc_ops gc2d_ops = {
    .init       = gpu_lock_init_dft,
    .enableclk  = eden_gpu_clk_enable,
    .disableclk = gpu_clk_disable_dft,
    .setclkrate = eden_gpu_clk_setrate,
    .getclkrate = eden_gpu_clk_getrate,
    .pwrops     = __PLAT_APINAME(gc2d_pwr_ops),
};

static struct gc_iface gc2d_iface = {
    .name               = "gc2d",
    .con_id             = "GC2D_CLK",
    .ops                = &gc2d_ops,
};

struct gc_iface *eden_gc_ifaces[] = {
    [gcvCORE_MAJOR] = &gc3d_iface,
    [gcvCORE_2D]    = &gc2d_iface,
    [gcvCORE_SH]    = &gc3dsh_iface,
    gcvNULL,
};

int eden_gpu_clk_setrate(struct gc_iface *iface, unsigned long rate_khz)
{
    int ret = 0;
    gceCORE core;

    if (iface == gpu_get_iface_mapping(gcvCORE_MAJOR))
    {
        core = gcvCORE_MAJOR;
    }
    else if (iface == gpu_get_iface_mapping(gcvCORE_2D))
    {
        core = gcvCORE_2D;
    }
    else if (iface == gpu_get_iface_mapping(gcvCORE_SH))
    {
        core = gcvCORE_SH;
    }
    else
    {
        goto OnError;
    }

    if ((has_feat_freq_change_indirect() &&
         !clk_restore[core].power_enabled)
        || (has_feat_freq_change_when_idle() &&
            core == gcvCORE_2D))
    {
        clk_restore[core].clk_rate = rate_khz;

        if (has_feat_freq_change_when_idle() &&
            core == gcvCORE_2D)
        {
            /*just return directly, clock will be set when next time clock on.*/
            return 0;
        }
    }

    if(has_feat_freq_change_indirect()
       && (rate_khz > 312000))
    {
        gpu_clk_setrate_dft(iface, 312000);
    }

    ret = gpu_clk_setrate_dft(iface, rate_khz);

    return ret;
OnError:
    return 0;
}

unsigned long eden_gpu_clk_getrate(struct gc_iface *iface)
{
    unsigned long rate;
    gceCORE core;

    if (iface == gpu_get_iface_mapping(gcvCORE_MAJOR))
    {
        core = gcvCORE_MAJOR;
    }
    else if (iface == gpu_get_iface_mapping(gcvCORE_2D))
    {
        core = gcvCORE_2D;
    }
    else if (iface == gpu_get_iface_mapping(gcvCORE_SH))
    {
        core = gcvCORE_SH;
    }
    else
    {
        goto OnError;
    }

    if ((has_feat_freq_change_indirect() &&
         !clk_restore[core].power_enabled)
        || (has_feat_freq_change_when_idle() &&
            core == gcvCORE_2D))
    {
        rate = clk_restore[core].clk_rate;
    }
    else
    {
        rate = gpu_clk_getrate_dft(iface);
    }

    return rate;

OnError:
    return 0;
}

int eden_gpu_clk_enable(struct gc_iface *iface)
{
    gceCORE core;

    if (iface == gpu_get_iface_mapping(gcvCORE_MAJOR))
    {
        core = gcvCORE_MAJOR;
    }
    else if (iface == gpu_get_iface_mapping(gcvCORE_2D))
    {
        core = gcvCORE_2D;
    }
    else if (iface == gpu_get_iface_mapping(gcvCORE_SH))
    {
        core = gcvCORE_SH;
    }
    else
    {
        goto OnError;
    }

    gpu_clk_setrate_dft(iface, clk_restore[core].clk_rate);

    gpu_clk_enable_dft(iface);

OnError:
    return 0;
}
