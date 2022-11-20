/*
 * Samsung Audio Subsystem Interface
 *
 * Copyright (c) 2013 Samsung Electronics Co. Ltd.
 *	HaeKwang Park <haekwang0808.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SOUND_EXYNOS_H
#define __SOUND_EXYNOS_H

extern void audss_uart_enable(void);
extern void audss_uart_disable(void);

extern void (*exynos_maudio_uart_cfg_gpio_pdn)(void);
#endif /* __SOUND_EXYNOS_H */
