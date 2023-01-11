/*
 * gc_hal_kernel_plat_common.h
 *
 * Author: Watson Wang <zswang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. *
 */

#ifndef __gc_hal_kernel_plat_common_h_
#define __gc_hal_kernel_plat_common_h_

#include "gc_hal_kernel_plat.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef PR_DEBUG
#define PR_DEBUG    pr_debug
#endif

void gpu_lock_init_dft(struct gc_iface *iface, void *pdev);
int gpu_clk_enable_dft(struct gc_iface *iface);
void gpu_clk_disable_dft(struct gc_iface *iface);
int gpu_clk_setrate_dft(struct gc_iface *iface, unsigned long rate_khz);
unsigned long gpu_clk_getrate_dft(struct gc_iface *iface);

#ifdef __cplusplus
}
#endif

#endif /* __gc_hal_kernel_plat_common_h_ */
