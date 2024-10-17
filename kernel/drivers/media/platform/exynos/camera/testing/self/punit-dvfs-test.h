/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PUNIT_DVFS_TEST_H
#define PUNIT_DVFS_TEST_H

#include "punit-test-checker.h"

const struct punit_checker_ops *punit_get_dvfs_checker_ops(void);

#endif /* PUNIT_DVFS_TEST_H */
