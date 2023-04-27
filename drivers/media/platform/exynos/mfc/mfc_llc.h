/*
 * drivers/media/platform/exynos/mfc/mfc_llc.h
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MFC_LLC_H
#define __MFC_LLC_H __FILE__

#include "mfc_common.h"

void mfc_llc_enable(struct mfc_dev *dev);
void mfc_llc_disable(struct mfc_dev *dev);
void mfc_llc_flush(struct mfc_dev *dev);

#endif /* __MFC_LLC_H */
