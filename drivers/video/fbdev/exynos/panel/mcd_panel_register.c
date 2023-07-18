// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>

/*
 * This code is to register MCD panel drivers and modules in case of GKI support.
 * Only for Nacho FB driver.
 */

/*
 * If you want to add model panel driver, please add here.
 */

#if IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_NT36672C_TIANMA_A14)
extern int __init nt36672c_tianma_a14_panel_init(void);
#endif

#if IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_ILI7807_BOE_A14)
extern int __init ili7807_boe_a14_panel_init(void);
#endif

#if IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_NT36672C_CSOT_A14)
extern int __init nt36672c_csot_a14_panel_init(void);
#endif

#if IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_FT8720_SKYWORTH_A14)
extern int __init ft8720_skyworth_a14_panel_init(void);
#endif

static void __init register_model_panels(void)
{
#if IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_NT36672C_TIANMA_A14)
	nt36672c_tianma_a14_panel_init();
#endif
#if IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_ILI7807_BOE_A14)
	ili7807_boe_a14_panel_init();
#endif
#if IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_NT36672C_CSOT_A14)
	nt36672c_csot_a14_panel_init();
#endif
#if IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_FT8720_SKYWORTH_A14)
	ft8720_skyworth_a14_panel_init();
#endif
}

/*
 * Nacho GKI mcd panel driver's probe sequence follows "previous version" totally.
 */
extern int __init decon_board_init(void);
extern struct platform_driver panel_driver;
extern int __init decon_notifier_init(void);
extern int __init regulator_void_init(void);
extern void __exit regulator_void_exit(void);
extern int __init decon_assist_init(void);

static int __init mcd_panel_register_init(void)
{
	int ret;

	pr_info("%s++\n", __func__);

	/* core_initcall */
	ret = decon_board_init();
	if (ret) {
		pr_err("failed to register decon decon_board_init\n");
		return ret;
	}
	ret = decon_notifier_init();
	if (ret) {
		pr_err("failed to register decon_notifier_init\n");
		return ret;
	}

	/* arch_initcall */
	register_model_panels();

	/* subsys_initcall */
	ret = regulator_void_init();
	if (ret) {
		pr_err("failed to register regulator_void_init\n");
		return ret;
	}

	/* device_initcall */
	ret = platform_driver_register(&panel_driver);
	if (ret) {
		pr_err("failed to register panel driver\n");
		return ret;
	}

	/* late_initcall_sync */
	ret = decon_assist_init();
	if (ret) {
		pr_err("failed to register decon_assist_init\n");
		return ret;
	}

	pr_info("%s--\n", __func__);

	return ret;
}

static void __exit mcd_panel_register_exit(void)
{
	platform_driver_unregister(&panel_driver);
	regulator_void_exit();
}

module_init(mcd_panel_register_init);
module_exit(mcd_panel_register_exit);

/* from decon_core + pmic */
MODULE_SOFTDEP("pre: cmupmucal clk_exynos exynos-pmu-if pinctrl-samsung-core fb phy-exynos-mipi exynos-pd exynos-pd-dbg samsung-iommu samsung_dma_heap acpm_mfd_bus s2mpu12-regulator");
MODULE_DESCRIPTION("Samsung's Panel Driver");
MODULE_AUTHOR("<minwoo7945.kim@samsung.com>");
MODULE_LICENSE("GPL");
