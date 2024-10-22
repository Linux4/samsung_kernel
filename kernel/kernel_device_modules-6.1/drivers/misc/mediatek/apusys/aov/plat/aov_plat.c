// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/types.h>

#include "aov_plat.h"

#include "v1/aov_rpmsg_v1.h"
#include "v1/aov_recovery_v1.h"

#include "v2/aov_recovery_v2.h"
#include "v2/aov_mem_service_v2.h"

int aov_plat_init(struct platform_device *pdev, unsigned int version)
{
	int ret = 0;

	pr_info("%s %d start\n", __func__, version);

	switch (version) {
	case 0:
		/* No need platform callback */
		break;
	case 1:
	{
		ret = aov_rpmsg_v1_init();
		if (ret) {
			pr_info("%s Failed to init rpmsg v1, ret %d\n", __func__, ret);
			return ret;
		}

		ret = aov_recovery_v1_init();
		if (ret) {
			pr_info("%s Failed to init apu recovery v1, ret %d\n", __func__, ret);
			return ret;
		}
	}
		break;
	case 2:
	{
		ret = aov_mem_service_v2_init(pdev);
		if (ret) {
			pr_info("%s Failed to init mem service v2, ret %d\n", __func__, ret);
			return ret;
		}
		ret = aov_recovery_v2_init(pdev);
		if (ret) {
			pr_info("%s Failed to init apu recovery v2, ret %d\n", __func__, ret);
			return ret;
		}
	}
		break;
	default:
		pr_info("%s Not supported version %d\n", __func__, version);
		break;
	}

	pr_info("%s %d end\n", __func__, version);

	return 0;
}

void aov_plat_exit(struct platform_device *pdev, unsigned int version)
{
	switch (version) {
	case 0:
		/* No need platform callback */
		break;
	case 1:
		aov_rpmsg_v1_exit();
		aov_recovery_v1_exit();
		break;
	case 2:
		aov_mem_service_v2_exit(pdev);
		aov_recovery_v2_exit(pdev);
		break;
	default:
		pr_info("%s Not supported version %d\n", __func__, version);
		break;
	}

}
