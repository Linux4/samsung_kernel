/*
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS5 - Header file for exynos Suspend to IDLE support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_S2I_H
#define __EXYNOS_S2I_H

#include <linux/list.h>

struct exynos_s2i_ops {
	struct list_head node;
	int (*suspend)(void);
	void (*resume)(void);
	int priority;
};

#if IS_ENABLED(CONFIG_EXYNOS_S2I)
extern void register_exynos_s2i_ops(struct exynos_s2i_ops *ops);
extern void unregister_exynos_s2i_ops(struct exynos_s2i_ops *ops);
#else
static inline void register_exynos_s2i_ops(struct exynos_s2i_ops *ops){ return; }
static inline void unregister_exynos_s2i_ops(struct exynos_s2i_ops *ops){ return; }
#endif

#endif /* __EXYNOS_S2I_H */
