/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_PM_H_
#define _NPU_PM_H_

void npu_pm_wake_lock(struct npu_session *session);
void npu_pm_wake_unlock(struct npu_session *session);

int npu_pm_suspend(struct device *dev);
int npu_pm_resume(struct device *dev);
int npu_pm_probe(struct npu_device *device);

#endif
