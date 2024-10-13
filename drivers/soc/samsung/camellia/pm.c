/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>

#include "log.h"

#include <drivers/soc/samsung/strong/strong_mailbox_common.h>
#include <drivers/soc/samsung/strong/strong_mailbox_ree.h>

uint32_t strong_power_on(void)
{
	LOG_DBG("strong_power_on");
	return mailbox_open();
}

uint32_t strong_power_off(void)
{
	LOG_DBG("strong_power_off");
	return mailbox_close();
}

#ifdef CONFIG_PM_SLEEP
static int camellia_resume(struct device *dev)
{
	LOG_INFO("Try to wake up SEE");

	//TODO wake up camellia

	return 0;
}

static int camellia_suspend(struct device *dev)
{
	LOG_INFO("Try to make SEE sleep");

	//TODO make camellia sleep

	return 0;
}
#endif

static const struct dev_pm_ops camellia_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(camellia_suspend, camellia_resume)
};

static struct platform_driver camellia_driver = {
	.driver		= {
		.name	= "camellia",
		.pm	= &camellia_pm_ops,
	},
};

int camellia_pm_init(void)
{
	int ret;

	ret = platform_driver_register(&camellia_driver);
	if (ret)
		LOG_ERR("Failed to add camellia pm driver");

	LOG_INFO("Power-management initialized");

	return ret;
}

void camellia_pm_deinit(void)
{
	platform_driver_unregister(&camellia_driver);
}
