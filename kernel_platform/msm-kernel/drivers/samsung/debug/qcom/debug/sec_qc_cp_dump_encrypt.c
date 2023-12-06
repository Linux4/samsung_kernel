// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2022 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <linux/samsung/debug/sec_debug.h>

#include "sec_qc_debug.h"

#define ENCRYPT_CP_REGION	0x1
#define CLEAR_CP_REGION		0x0

static int __cp_dump_encrypt_init(struct qc_debug_drvdata *drvdata)
{
	struct device_node *np_imem;
	struct device *dev = drvdata->bd.dev;
	void __iomem *cp_dump_encrypt;
	unsigned int policy;

	np_imem = of_find_compatible_node(NULL, NULL,
			"qcom,msm-imem-cp_dump_encrypt");
	if (!np_imem) {
		pr_warn("can't find qcom,msm-imem node\n");
		return -ENODEV;
	}

	cp_dump_encrypt = of_iomap(np_imem, 0);
	if (!cp_dump_encrypt) {
		dev_err(dev, "Can't map imem\n");
		return -ENXIO;
	}

	policy = sec_debug_is_enabled() ?
			ENCRYPT_CP_REGION : CLEAR_CP_REGION;
	__raw_writel(policy, cp_dump_encrypt);
	iounmap(cp_dump_encrypt);

	return 0;
}

int sec_qc_cp_dump_encrypt_init(struct builder *bd)
{
	struct qc_debug_drvdata *drvdata =
			container_of(bd, struct qc_debug_drvdata, bd);

	if (!drvdata->use_cp_dump_encrypt)
		return 0;

	return __cp_dump_encrypt_init(drvdata);
}
