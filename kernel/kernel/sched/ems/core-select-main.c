/*
 * Core Exynos Mobile Scheduler
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd
 */

#include "ems.h"

static void qjump_rq_list_init(void)
{
	int cpu;

	for_each_cpu(cpu, cpu_possible_mask) {
		INIT_LIST_HEAD(ems_qjump_list(cpu_rq(cpu)));
		ems_rq_nr_prio_tex(cpu_rq(cpu)) = 0;
		ems_rq_nr_launch(cpu_rq(cpu)) = 0;
	}
}

void ems_core_select_init(struct platform_device *pdev)
{
	struct kobject *ems_kobj = &pdev->dev.kobj;

	qjump_rq_list_init();
	core_init(pdev->dev.of_node);
	ntu_init(ems_kobj);
	ontime_init(ems_kobj);
	frt_init();
	sysbusy_init(ems_kobj);
	gsc_init(ems_kobj);
	emstune_init(ems_kobj, pdev->dev.of_node, &pdev->dev);

	lb_init();
	mhdvfs_init(ems_kobj);

}

