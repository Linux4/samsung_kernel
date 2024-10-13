/*
 * Core Exynos Mobile Scheduler
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd
 */

#include "ems.h"

void ems_freq_select_init(struct platform_device *pdev)
{
	struct kobject *ems_kobj = &pdev->dev.kobj;

	cpufreq_init();
	freqboost_init();
	ego_pre_init(ems_kobj);
	et_init(ems_kobj);
	pago_init(ems_kobj);
}
