// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/types.h>

#include "scp.h"

#include "aov_recovery_v2.h"
#include "aov_recovery.h"

static int aov_recovery_scp_notifier_call(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	int ret = 0;

	if (event == SCP_EVENT_STOP) {
		pr_info("%s receive scp stop event\n", __func__);
		ret = aov_recovery_cb();
		if (!ret)
			pr_info("%s Failt to aov_recovery_cb, ret %d\n", __func__, ret);
	} else if (event == SCP_EVENT_READY)
		pr_info("%s receive scp ready event\n", __func__);

	return NOTIFY_DONE;
}

static struct notifier_block aov_recovery_scp_notifier = {
	.notifier_call = aov_recovery_scp_notifier_call,
};

int aov_recovery_v2_init(struct platform_device *pdev)
{
	scp_A_register_notify(&aov_recovery_scp_notifier);

	return 0;
}

void aov_recovery_v2_exit(struct platform_device *pdev)
{
	scp_A_unregister_notify(&aov_recovery_scp_notifier);
}
