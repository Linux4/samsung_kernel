/* linux/include/soc/samsung/exynos-halt.h
 *
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_HALT_H
#define __EXYNOS_HALT_H

#ifdef CONFIG_EXYNOS_HALT
void set_stop_cpu(void);
#else
#define set_stop_cpu()		do { } while(0);
#endif

#endif
