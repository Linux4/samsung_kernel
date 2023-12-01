// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_SELF_TEST_RESULT_H
#define PABLO_SELF_TEST_RESULT_H

#include "is-device-csi.h"

void pst_result_csi_irq(struct is_device_csi *csi);

#endif
