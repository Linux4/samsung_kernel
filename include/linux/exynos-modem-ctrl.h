/*
 * Copyright (C) 2015 Samsung Electronics Co.Ltd
 * http://www.samsung.com
 *
 * EXYNOS MODEM CONTROL driver
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
*/

#ifndef __EXYNOS_MODEM_CTRL_H
#define __EXYNOS_MODEM_CTRL_H

#if defined(CONFIG_SOC_EXYNOS8890) || defined(CONFIG_SOC_EXYNOS7870) \
	|| defined(CONFIG_SOC_EXYNOS7570)
extern int ss310ap_force_crash_exit_ext(void);
extern u32 ss310ap_get_evs_mode_ext(void);
#endif

#endif
