/*
 * drivers/media/platform/exynos/mfc/mfc_llc.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <soc/samsung/exynos-sci.h>

#include "mfc_llc.h"

void mfc_llc_enable(struct mfc_dev *dev)
{
	mfc_debug_dev_enter();

	if (llc_disable)
		return;

	llc_region_alloc(LLC_REGION_MFC_DISPLAY, 1);
	dev->llc_on_status = 1;
	mfc_info_dev("[LLC] enabled\n");
	MFC_TRACE_DEV("[LLC] enabled\n");

	mfc_debug_dev_leave();
}

void mfc_llc_disable(struct mfc_dev *dev)
{
	mfc_debug_dev_enter();

	llc_region_alloc(LLC_REGION_MFC_DISPLAY, 0);
	dev->llc_on_status = 0;
	mfc_info_dev("[LLC] disabled\n");
	MFC_TRACE_DEV("[LLC] disabled\n");

	mfc_debug_dev_leave();
}

void mfc_llc_flush(struct mfc_dev *dev)
{
	mfc_debug_dev_enter();

	if (llc_disable)
		return;

	llc_flush(LLC_REGION_MFC_DISPLAY);
	mfc_debug_dev(2, "[LLC] flushed\n");
	MFC_TRACE_DEV("[LLC] flushed\n");

	mfc_debug_dev_leave();
}
