/*
 * Core Exynos Mobile Scheduler
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd
 */

#include "ems.h"

void ems_idle_select_init(struct platform_device *pdev)
{
	struct kobject *ems_kobj = &pdev->dev.kobj;

	ecs_init(ems_kobj);
	ecs_gov_dynamic_init(ems_kobj);
	fv_init(ems_kobj);
	halo_governor_init(ems_kobj);
}
