/*
 * gc_hal_kernel_plat_common.c
 *
 * Author: Watson Wang <zswang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. *
 */

#include <linux/clk.h>
#include <linux/printk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include "gc_hal_kernel_plat_common.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
#define GC_CLK_ENABLE       clk_prepare_enable
#define GC_CLK_DISABLE      clk_disable_unprepare
#else
#define GC_CLK_ENABLE       clk_enable
#define GC_CLK_DISABLE      clk_disable
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
#define get_clk_ptr(dev, id) devm_clk_get(&(dev), id)
#else
#define get_clk_ptr(dev, id) clk_get(NULL, id)
#endif
static struct clk * __get_gpu_clk(struct gc_iface *iface)
{
    struct platform_device *pdev = gcvNULL;

    pdev = (struct platform_device *)iface->pdev;

    if(WARN_ON(unlikely(!iface)))
        return NULL;

    if(unlikely(!iface->clk))
        iface->clk = get_clk_ptr(pdev->dev, iface->con_id);

    BUG_ON(IS_ERR_OR_NULL(iface->clk));

    return iface->clk;
}

void gpu_lock_init_dft(struct gc_iface *iface, gctPOINTER pdev)
{
    /* init lock for iface at the first time */
    if(unlikely(!iface->inited))
    {
        PR_DEBUG("[%6s] %s\n", iface->name, __func__);
        mutex_init(&iface->clk_mutex);
        spin_lock_init(&iface->clk_spinlock);

        mutex_init(&iface->pwr_mutex);
        spin_lock_init(&iface->pwr_spinlock);
        iface->inited = 1;

        /* set clk to be cansleep by default */
        iface->cansleep = 1;

        /* init all pdev in iface*/
        iface->pdev = pdev;
    }
}

int gpu_clk_enable_dft(struct gc_iface *iface)
{
    struct clk * gc_clk = __get_gpu_clk(iface);
    PR_DEBUG("[%6s] %s\n", iface->name, __func__);
    return GC_CLK_ENABLE(gc_clk);
}

void gpu_clk_disable_dft(struct gc_iface *iface)
{
    struct clk * gc_clk = __get_gpu_clk(iface);
    PR_DEBUG("[%6s] %s\n", iface->name, __func__);
    GC_CLK_DISABLE(gc_clk);
}

int gpu_clk_setrate_dft(struct gc_iface *iface, unsigned long rate_khz)
{
    struct clk *gc_clk = __get_gpu_clk(iface);
    PR_DEBUG("[%6s] %s %luKHZ\n", iface->name, __func__, rate_khz);
    return clk_set_rate(gc_clk, KHZ_TO_HZ(rate_khz));
}

unsigned long gpu_clk_getrate_dft(struct gc_iface *iface)
{
    struct clk *gc_clk = __get_gpu_clk(iface);
    unsigned long rate = HZ_TO_KHZ(clk_get_rate(gc_clk));
    PR_DEBUG("[%6s] %s %luKHZ\n", iface->name, __func__, rate);
    return rate;
}

