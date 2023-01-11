/*
 * gc_hal_kernel_plat.h
 *
 * Author: Watson Wang <zswang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. *
 */

#ifndef __gc_hal_kernel_plat_h_
#define __gc_hal_kernel_plat_h_

#include "gc_hal.h"
#include "gc_hal_kernel.h"
#include <linux/mutex.h>
#include <linux/spinlock.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define KHZ_TO_HZ(val)     ((val) * 1000)
#define HZ_TO_KHZ(val)     ((val) / 1000)

struct gc_iface;

struct gc_ops {
    void (*init) (struct gc_iface *, void *);
    int (*enableclk) (struct gc_iface *);
    void (*disableclk) (struct gc_iface *);
    int (*setclkrate) (struct gc_iface *, unsigned long);
    unsigned long (*getclkrate) (struct gc_iface *);
    int (*pwrops) (struct gc_iface *, unsigned int);
    void (*pwr_enable_prepare) (struct gc_iface *);
    void (*pwr_enable_unprepare) (struct gc_iface *);
    void (*pwr_disable_prepare) (struct gc_iface *);
    void (*pwr_disable_unprepare) (struct gc_iface *);
};

struct gc_iface
{
    const struct gc_ops *ops;

    const char *name;
    const char *con_id;
    struct clk *clk;

    /*ptr for current device*/
    void *pdev;

    const unsigned int chains_count;
    struct gc_iface **chains_clk;

    unsigned int clk_refcnt;
    unsigned int pwr_refcnt;

    unsigned int inited;
    unsigned int cansleep;
    struct mutex clk_mutex;
    spinlock_t clk_spinlock;

    struct mutex pwr_mutex;
    spinlock_t pwr_spinlock;

    struct gc_iface *axi_clk;
};

int gpu_clk_init(struct gc_iface *iface, void *ptr);
void gpu_clk_disable(struct gc_iface *iface);
int gpu_clk_enable(struct gc_iface *iface);
int gpu_clk_setrate(struct gc_iface *iface, unsigned long rate_khz);
int gpu_clk_getrate(struct gc_iface *iface, unsigned long *rate_khz);

int gpu_pwr_disable(struct gc_iface *iface);
int gpu_pwr_enable(struct gc_iface *iface);
void gpu_pwr_disable_prepare(struct gc_iface *iface);
void gpu_pwr_disable_unprepare(struct gc_iface *iface);
void gpu_pwr_enable_prepare(struct gc_iface *iface);
void gpu_pwr_enable_unprepare(struct gc_iface *iface);

struct gc_iface* gpu_get_iface_mapping(gceCORE core);

#ifdef __cplusplus
}
#endif

#endif /* __gc_hal_kernel_plat_h_ */
