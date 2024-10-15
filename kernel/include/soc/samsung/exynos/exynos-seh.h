/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 */

#ifndef EXYNOS_SEH_H
#define EXYNOS_SEH_H


struct seh_info_data {
	struct device *dev;

	char *fail_info;
	dma_addr_t fail_info_pa;
};
/******************************************************************************/
/* Define function                                                            */
/******************************************************************************/
void exynos_seh_set_cm_debug_function(unsigned long addr);

#endif
