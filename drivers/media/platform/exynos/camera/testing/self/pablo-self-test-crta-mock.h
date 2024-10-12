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

#ifndef PABLO_SELF_TEST_CRTA_MOCK_H
#define PABLO_SELF_TEST_CRTA_MOCK_H

#include "is-hw.h"

void pst_deinit_crta_mock(struct is_hardware *hw);
int pst_init_crta_mock(struct is_hardware *hw);

#endif
