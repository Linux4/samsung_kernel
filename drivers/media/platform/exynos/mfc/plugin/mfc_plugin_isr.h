/*
 * drivers/media/platform/exynos/mfc/plugin/mfc_plugin_isr.h
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MFC_PLUGIN_ISR_H
#define __MFC_PLUGIN_ISR_H __FILE__

#include <linux/interrupt.h>

irqreturn_t mfc_plugin_irq(int irq, void *priv);

#endif /* __MFC_PLUGIN_ISR_H */
