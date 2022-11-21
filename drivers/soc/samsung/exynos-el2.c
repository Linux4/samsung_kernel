/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - EL2 module
 * Author: Junho Choi <junhosj.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/hvc.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include <asm/barrier.h>
#include <asm/cacheflush.h>
#include <asm/io.h>

#include <soc/samsung/exynos-el2.h>


/* The buffer for EL2 crash information */
static uint64_t *el2_crash_buf;

static spinlock_t el2_lock;


static void exynos_report_el2_crash_info(int cpu,
					 int max_reg_num,
					 int crash_flag)
{
	char *buf = (char *)el2_crash_buf + (EL2_CRASH_BUFFER_SIZE * cpu);
	int i, len = 0;
	unsigned long flags;

	spin_lock_irqsave(&el2_lock, flags);

	pr_info("\n================[EL2 EXCEPTION REPORT]================\n");
	pr_info("\n");

	pr_info("[CPU %d]\n", cpu);
	pr_info("Exception Information\n");
	pr_info("\n");

	pr_info("The exception happened in%sH-Arx\n",
			(crash_flag == 0) ? " " : " not ");
	pr_info("\n");

	__inval_dcache_area(buf, EL2_CRASH_BUFFER_SIZE);

	for (i = 0; i < max_reg_num; i++) {
		len = pr_info("%s", buf);

		/* Skip '\n'(Line Feed) and '0'(NULL) characters */
		buf += (len + 2);

		pr_debug("\n");
		pr_debug("- String length : %x\n", len);
		pr_debug("- Next buffer : %#llx\n", (uint64_t)buf);
		pr_debug("- NO. of regs : %d\n", i);
		pr_debug("\n");

		if (*buf == '\0')
			break;
	}

	pr_info("\n======================================================\n");

	spin_unlock_irqrestore(&el2_lock, flags);

	BUG();

	/* Never reach here */
	do {
		wfi();
	} while (1);
}

static int __init exynos_deliver_el2_crash_info(void)
{
	uint64_t ret = 0;
	uint64_t buf_pa;
	uint64_t *buf;
	int retry_cnt = MAX_RETRY_COUNT;

	spin_lock_init(&el2_lock);

	do {
		buf = kzalloc(EL2_CRASH_BUFFER_SIZE * NR_CPUS, GFP_KERNEL);
		if (buf != NULL) {
			el2_crash_buf = buf;
			buf_pa = virt_to_phys(buf);
			pr_info("%s: Allocate EL2 crash buffer VA[%#llx]/IPA[%#llx]\n",
					__func__, (uint64_t)el2_crash_buf, buf_pa);

			break;
		}
	} while ((buf == NULL) && (retry_cnt-- > 0));

	if (buf == NULL) {
		pr_err("%s: Fail to allocate EL2 crash buffer\n", __func__);
		return -ENOMEM;
	}

	ret = exynos_hvc(HVC_FID_SET_EL2_CRASH_INFO_FP_BUF,
			 (uint64_t)exynos_report_el2_crash_info,
			 buf_pa, 0, 0);
	if (ret) {
		pr_err("%s: Already set EL2 exception report function\n",
				__func__);
		return -EPERM;
	}

	pr_info("%s: EL2 Crash Info Buffer and FP is delivered successfully\n",
			__func__);

	return 0;
}
core_initcall(exynos_deliver_el2_crash_info);

MODULE_DESCRIPTION("Exynos EL2 module driver");
MODULE_AUTHOR("<junhosj.choi@samsung.com>");
MODULE_LICENSE("GPL");
