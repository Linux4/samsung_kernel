/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/cpuhotplug.h>

#include "hts_pmu.h"

static int hts_core_online(unsigned int cpu)
{
	int ret;

	ret = hts_register_core_event(cpu);
	if (ret)
		pr_err("HTS : Couldn't reigster event successfully");

	return 0;
}

static int hts_core_offline(unsigned int cpu)
{
	hts_release_core_event(cpu);

	return 0;
}

static int initialize_hts_hotplug_notifier(struct platform_device *pdev)
{
	if (cpuhp_setup_state(CPUHP_AP_ONLINE_DYN,
				"hts_core",
				hts_core_online, hts_core_offline) < 0)
		return -EINVAL;

	return 0;
}

int initialize_hts_core(struct platform_device *pdev)
{
	int ret;

	ret = initialize_hts_percpu();
	if (ret)
		return ret;

	ret = initialize_hts_hotplug_notifier(pdev);
	if (ret)
		return ret;

	return 0;
}
