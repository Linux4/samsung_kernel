/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - CPU SPARING Governor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_SPARING_GOVERNOR
#define __EXYNOS_SPARING_GOVERNOR

/* Sparing modes for boosting/saving system */
#define NORMAL			(0x0)
#define BOOSTING		(0x1 << 1)
#define SAVING			(0x1 << 2)

/* Roles of domains for boosting/saving system */
#define	NOTHING			(0x0)
#define	TRIGGER			(0x1 << 0)
#define	BOOSTER			(0x1 << 1)
#define SAVER			(0x1 << 2)
#define	TRIGGER_AND_BOOSTER	(TRIGGER | BOOSTER)
#define TRIGGER_AND_SAVER	(TRIGGER | SAVER)

#endif /* __EXYNOS_SPARING_GOV__ */
