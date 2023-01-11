/*
 * gc_hal_kernel_plat.c
 *      based on linux/arch/arm/plat-pxa/clock.c
 *
 * Author: Watson Wang <zswang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. *
 */

#include "gc_hal_kernel_plat.h"

/********************************************************
 * basic initialization *********************************
 *******************************************************/
static inline unsigned int __cansleep(struct gc_iface *iface)
{
    return iface->cansleep;
}

#define gpu_clk_lock_save(iface, flags)                     \
    do {                                                \
        if (__cansleep(iface)) {                      \
            flags = 0;                                  \
            mutex_lock(&iface->clk_mutex);                  \
        } else {                                        \
            spin_lock_irqsave(&iface->clk_spinlock, flags); \
        }                                               \
    } while(0)

#define gpu_clk_lock_restore(iface, flags)                           \
    do {                                                     \
        if (__cansleep(iface)) {                           \
            mutex_unlock(&iface->clk_mutex);                     \
        } else {                                             \
            spin_unlock_irqrestore(&iface->clk_spinlock, flags); \
        }                                                    \
    } while(0)

#define gpu_pwr_lock_save(iface, flags)                     \
    do {                                                \
        if (__cansleep(iface)) {                      \
            flags = 0;                                  \
            mutex_lock(&iface->pwr_mutex);                  \
        } else {                                        \
            spin_lock_irqsave(&iface->pwr_spinlock, flags); \
        }                                               \
    } while(0)

#define gpu_pwr_lock_restore(iface, flags)                           \
    do {                                                     \
        if (__cansleep(iface)) {                           \
            mutex_unlock(&iface->pwr_mutex);                     \
        } else {                                             \
            spin_unlock_irqrestore(&iface->pwr_spinlock, flags); \
        }                                                    \
    } while(0)

/********************************************************
 * GC clock operations **********************************
 *******************************************************/
static void __gpu_clk_disable(struct gc_iface *iface)
{
    unsigned int chains_count = iface->chains_count;

    if(iface->ops && iface->ops->disableclk)
        iface->ops->disableclk(iface);

    while(0 != chains_count)
        __gpu_clk_disable(iface->chains_clk[--chains_count]);

    iface->clk_refcnt--;
}

static int __gpu_clk_enable(struct gc_iface *iface)
{
    int ret = 0, i = 0;
    unsigned int chains_count = iface->chains_count;

    while(0 != chains_count)
    {
        chains_count--;
        ret = __gpu_clk_enable(iface->chains_clk[i]);
        if(ret)
        {
            while(i > 0)
                __gpu_clk_disable(iface->chains_clk[--i]);
            goto out;
        }
        i++;
    }

    if(iface->ops && iface->ops->enableclk)
        ret = iface->ops->enableclk(iface);
    if(ret)
        goto disable_chains;

    iface->clk_refcnt++;

    return 0;

disable_chains:
    chains_count = iface->chains_count;
    while (0 != chains_count)
        __gpu_clk_disable(iface->chains_clk[--chains_count]);
out:
    return ret;
}

int gpu_clk_init(struct gc_iface *iface, void *ptr)
{
    unsigned int chains_count = iface->chains_count;

    if(NULL == iface)
        return -1;

    if(iface->inited)
        return 1;

    while(0 != chains_count)
    {
        chains_count--;
        gpu_clk_init(iface->chains_clk[chains_count], ptr);
    }

    if(iface->ops && iface->ops->init)
        iface->ops->init(iface, ptr);

    return 0;
}

void gpu_clk_disable(struct gc_iface *iface)
{
    unsigned long flags;

    gpu_clk_lock_save(iface, flags);
    __gpu_clk_disable(iface);
    gpu_clk_lock_restore(iface, flags);
}

int gpu_clk_enable(struct gc_iface *iface)
{
    unsigned long flags;
    int ret = 0;

    gpu_clk_lock_save(iface, flags);

    ret = __gpu_clk_enable(iface);

    gpu_clk_lock_restore(iface, flags);

    return ret;
}

int gpu_clk_setrate(struct gc_iface *iface, unsigned long rate_khz)
{
    unsigned long flags;
    int ret = 0;

    gpu_clk_lock_save(iface, flags);

    if(iface->ops && iface->ops->setclkrate)
        ret = iface->ops->setclkrate(iface, rate_khz);

    gpu_clk_lock_restore(iface, flags);

    return ret;
}

int gpu_clk_getrate(struct gc_iface *iface, unsigned long *rate_khz)
{
    unsigned long flags;
    unsigned long rate = 0;

    gpu_clk_lock_save(iface, flags);

    if(iface->ops && iface->ops->getclkrate)
        rate = iface->ops->getclkrate(iface);

    *rate_khz = rate;

    gpu_clk_lock_restore(iface, flags);

    return 0;
}

/********************************************************
 * GC power operations **********************************
 *******************************************************/
static int __gpu_pwr_disable(struct gc_iface *iface)
{
    int retval = 0;

    if(iface->ops && iface->ops->pwrops)
        retval = iface->ops->pwrops(iface, 0);

    if(likely(retval >= 0))
        iface->pwr_refcnt--;

    return retval;
}

static int __gpu_pwr_enable(struct gc_iface *iface)
{
    int retval = 0;

    if(iface->ops && iface->ops->pwrops)
        retval = iface->ops->pwrops(iface, 1);

    if(likely(retval >= 0))
        iface->pwr_refcnt++;

    return retval;
}

int gpu_pwr_disable(struct gc_iface *iface)
{
    unsigned long flags;
    int retval = 0;

    gpu_pwr_lock_save(iface, flags);
    retval = __gpu_pwr_disable(iface);
    gpu_pwr_lock_restore(iface, flags);

    return retval;
}

int gpu_pwr_enable(struct gc_iface *iface)
{
    unsigned long flags;
    int retval = 0;

    gpu_pwr_lock_save(iface, flags);
    retval = __gpu_pwr_enable(iface);
    gpu_pwr_lock_restore(iface, flags);

    /* Delay 10 us to wait for chip stable. */
    gckOS_Udelay(gcvNULL, 10);

    return retval;
}

void gpu_pwr_disable_prepare(struct gc_iface *iface)
{
    if(iface->ops && iface->ops->pwr_disable_prepare)
        iface->ops->pwr_disable_prepare(iface);
}

void gpu_pwr_disable_unprepare(struct gc_iface *iface)
{
    if(iface->ops && iface->ops->pwr_disable_unprepare)
        iface->ops->pwr_disable_unprepare(iface);
}

void gpu_pwr_enable_prepare(struct gc_iface *iface)
{
    if(iface->ops && iface->ops->pwr_enable_prepare)
        iface->ops->pwr_enable_prepare(iface);
}

void gpu_pwr_enable_unprepare(struct gc_iface *iface)
{
    if(iface->ops && iface->ops->pwr_enable_unprepare)
        iface->ops->pwr_enable_unprepare(iface);
}

static struct gc_iface **gc_ifaces = gcvNULL;
extern struct gc_iface *pxa988_gc_ifaces[];
extern struct gc_iface *eden_gc_ifaces[];

void gpu_init_iface_mapping(void)
{
    if(cpu_is_pxa1928())
    {
        gc_ifaces = eden_gc_ifaces;
    }
    else
    {
        gc_ifaces = pxa988_gc_ifaces;
    }
}

struct gc_iface* gpu_get_iface_mapping(gceCORE core)
{
    struct gc_iface* iface = gcvNULL;

    if(gc_ifaces == gcvNULL)
    {
        gpu_init_iface_mapping();
    }

    switch(core) {
    case gcvCORE_MAJOR:
        iface = gc_ifaces[gcvCORE_MAJOR];
        break;
    case gcvCORE_2D:
        iface = gc_ifaces[gcvCORE_2D];
        break;
    case gcvCORE_SH:
        iface = gc_ifaces[gcvCORE_SH];
        break;
    default:
        gcmkPRINT("Invalid core to get iface mapping!\n");
    }

    return iface;
}

