/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MACH_EXYNOS_BOARD_UNIVERSAL5260_PMIC_H
#define __MACH_EXYNOS_BOARD_UNIVERSAL5260_PMIC_H


extern void board_hl_pmic_init(void);

void __init exynos5_universal5260_pmic_init(void)
{
	board_hl_pmic_init();

}

#endif /* __MACH_EXYNOS_BOARD_UNIVERSAL5260_PMIC_H */
