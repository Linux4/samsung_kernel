/* linux/drivers/video/decon_ext_display/decon_ext_display_driver.c
 *
 * Copyright (c) 2013 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/fs.h>
#include <linux/fb.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/lcd.h>
#include <linux/gpio.h>
#include <linux/exynos_iovmm.h>

#include "decon_display_driver.h"
#include "decon_dt.h"
#include "decon_pm.h"

#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
#include "decon_fb.h"
#else
#include "fimd_fb.h"
#endif

#include "decon_debug.h"

#ifdef CONFIG_OF
static const struct of_device_id decon_ext_disp_device_table[] = {
	{ .compatible = "samsung,exynos5-disp1_driver" },
	{},
};
MODULE_DEVICE_TABLE(of, decon_ext_disp_device_table);
#endif

static struct display_driver g_display2_driver;

int init_display_decon_clocks_ext(struct device *dev);
int enable_display_decon_clocks_ext(struct device *dev);
int disable_display_decon_ext_clocks(struct device *dev);
int enable_display_decon_runtimepm_ext(struct device *dev);
int disable_display_decon_runtimepm_ext(struct device *dev);
int enable_display_dsd_clocks_ext(struct device *dev);
int disable_display_dsd_clocks_ext(struct device *dev);
int init_display_dsi_clocks_ext(struct device *dev);
int enable_display_driver_power_ext(struct device *dev);
int disable_display_driver_power_ext(struct device *dev);
int reset_display_driver_panel_ext(struct device *dev);

int parse_display_driver_dt_ext(struct platform_device *np, struct display_driver *ddp);
struct s3c_fb_driverdata *get_display_drvdata_ext(void);
struct s3c_fb_platdata *get_display_platdata_ext(void);
struct mipi_dsim_config *get_display_dsi_drvdata_ext(void);
struct mipi_dsim_lcd_config *get_display_lcd_drvdata_ext(void);
struct display_gpio *get_display_dsi_reset_gpio_ext(void);
struct mic_config *get_display_mic_config(void);

extern int s5p_mipi_dsi_disable_ext(struct mipi_dsim_device *dsim);
/* init display operations for pm and parsing functions */
static int init_display_operations(void)
{
#define DT_OPS g_display2_driver.dt_ops
#define DSI_OPS g_display2_driver.dsi_driver.dsi_ops
#define DECON_OPS g_display2_driver.decon_driver.decon_ops
	DT_OPS.parse_display_driver_dt_ext = parse_display_driver_dt_ext;
	DT_OPS.get_display_drvdata_ext = get_display_drvdata_ext;
	DT_OPS.get_display_platdata_ext = get_display_platdata_ext;
	DT_OPS.get_display_dsi_drvdata_ext = get_display_dsi_drvdata_ext;
	DT_OPS.get_display_lcd_drvdata_ext = get_display_lcd_drvdata_ext;
	DT_OPS.get_display_dsi_reset_gpio_ext = get_display_dsi_reset_gpio_ext;
#ifdef CONFIG_DECON_MIC
	DT_OPS.get_display_mic_config = get_display_mic_config;
#endif

	DSI_OPS.init_display_dsi_clocks_ext = init_display_dsi_clocks_ext;
	DSI_OPS.enable_display_driver_power_ext = enable_display_driver_power_ext;
	DSI_OPS.disable_display_driver_power_ext = disable_display_driver_power_ext;
	DSI_OPS.reset_display_driver_panel_ext = reset_display_driver_panel_ext;

	DECON_OPS.init_display_decon_clocks_ext = init_display_decon_clocks_ext;
	DECON_OPS.enable_display_decon_clocks_ext = enable_display_decon_clocks_ext;
	DECON_OPS.disable_display_decon_ext_clocks = disable_display_decon_ext_clocks;
	DECON_OPS.enable_display_decon_runtimepm_ext = enable_display_decon_runtimepm_ext;
	DECON_OPS.disable_display_decon_runtimepm_ext = disable_display_decon_runtimepm_ext;
#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	DECON_OPS.enable_display_dsd_clocks_ext = enable_display_dsd_clocks_ext;
	DECON_OPS.disable_display_dsd_clocks_ext = disable_display_dsd_clocks_ext;
#endif
#undef DT_OPS
#undef DSI_OPS
#undef DECON_OPS

	return 0;
}

int save_decon_ext_operation_time(enum decon_ops_stamp ops)
{
#ifdef CONFIG_DEBUG_FS
	unsigned int num;
	struct display_driver *dispdrv;
	dispdrv = get_display_driver_ext();

	num = dispdrv->ops_timestamp[ops].num_timestamps++;
	num %= DISPLAY_DEBUG_FIFO_COUNT;

	dispdrv->ops_timestamp[ops].fifo_timestamps[num] = ktime_get();
#endif
	return 0;
}

int dump_decon_ext_operation_time(void)
{
#ifdef CONFIG_DEBUG_FS
	int i, j;
	struct display_driver *dispdrv;
	dispdrv = get_display_driver_ext();

	for (i = 0; i < OPS_CALL_MAX; i++) {
		pr_info("[INFO] DUMP index[%.2d]\n", i);
		for (j = 0; j < DISPLAY_DEBUG_FIFO_COUNT; j++) {
			pr_info("OPS_TIMESTAMP[%.2d]= \t%lld\n", j,
				ktime_to_ns(dispdrv->ops_timestamp[i].fifo_timestamps[j]));
		}
	}
#endif
	return 0;
}

/* create_disp_components - create all components in display sub-system.
 * */
static int create_disp_components(struct platform_device *pdev)
{
	int ret = 0;

#ifdef CONFIG_DECON_MIC
	ret = create_decon_mic_ext(pdev);
	if (ret < 0) {
		pr_err("display error: MIC create failed.");
		return ret;
	}
#endif

	/* IMPORTANT: MIPI-DSI component should be 1'st created. */
	ret = create_mipi_dsi_controller_ext(pdev);
	if (ret < 0) {
		pr_err("display error: mipi-dsi controller create failed.");
		return ret;
	}

	ret = create_decon_ext_display_controller(pdev);
	if (ret < 0) {
		pr_err("display error: display controller create failed.");
		return ret;
	}

	return ret;
}

/* disp2_driver_fault_handler - fault handler for display device driver */
int disp2_driver_fault_handler(struct iommu_domain *iodmn, struct device *dev,
	unsigned long addr, int id, void *param)
{
	struct display_driver *dispdrv;

	dispdrv = (struct display_driver*)param;
	decon_ext_dump_registers(dispdrv);
	return 0;
}

/* register_debug_features - for registering debug features.
 * currently registered features are like as follows...
 * - iovmm falult handler
 * - ... */
static void register_debug_features(void)
{
	/* 1. fault handler registration */
	iovmm_set_fault_handler(g_display2_driver.display_driver,
		disp2_driver_fault_handler, &g_display2_driver);
}

/* s5p_decon_ext_disp_probe - probe function of the display driver */
static int s5p_decon_ext_disp_probe(struct platform_device *pdev)
{
	int ret = 0;

	init_display_operations();

#ifdef CONFIG_FB_HIBERNATION_DISPLAY2
	init_display_pm(&g_display2_driver);
#endif

	/* parse display driver device tree & convers it to objects
	 * for each platform device */
	ret = g_display2_driver.dt_ops.parse_display_driver_dt_ext(pdev, &g_display2_driver);
	if (ret < 0) {
		pr_err("%s: fail parse display dt", __func__);
		return ret;
	}

	GET_DISPDRV_OPS(&g_display2_driver).init_display_dsi_clocks_ext(&pdev->dev);
#ifndef CONFIG_PM_RUNTIME
	GET_DISPCTL_OPS(&g_display2_driver).init_display_decon_clocks_ext(&pdev->dev);
#endif

	ret = create_disp_components(pdev);
	if (ret < 0) {
		pr_err("%s: fail create disp components", __func__);
		return ret;
	}
	if (0)
		register_debug_features();

	return ret;
}

static int s5p_decon_ext_disp_remove(struct platform_device *pdev)
{
	return 0;
}

static int display_driver_runtime_suspend(struct device *dev)
{
#ifdef CONFIG_PM_RUNTIME
	return s3c_fb_runtime_suspend_ext(dev);
#else
	return 0;
#endif
}

static int display_driver_runtime_resume(struct device *dev)
{
#ifdef CONFIG_PM_RUNTIME
	return s3c_fb_runtime_resume_ext(dev);
#else
	return 0;
#endif
}

static void display_driver_shutdown(struct platform_device *pdev)
{
#ifdef CONFIG_FB_HIBERNATION_DISPLAY2
	struct display_driver *dispdrv = get_display_driver_ext();
	disp_set_pm_status(DISP_STATUS_PM2);
	disp_pm_gate_lock(dispdrv, true);
	disp_pm_add_refcount(get_display_driver_ext());
#endif
	s5p_mipi_dsi_disable_ext(g_display2_driver.dsi_driver.dsim);
}

static const struct dev_pm_ops s5p_decon_ext_disp_ops = {
#ifdef CONFIG_PM_SLEEP
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = NULL,
	.resume = NULL,
#endif
#endif
	.runtime_suspend	= display_driver_runtime_suspend,
	.runtime_resume		= display_driver_runtime_resume,
};

/* get_display_driver_ext - for returning reference of display
 * driver context */
struct display_driver *get_display_driver_ext(void)
{
	return &g_display2_driver;
}

static struct platform_driver s5p_decon_ext_disp_driver = {
	.probe = s5p_decon_ext_disp_probe,
	.remove = s5p_decon_ext_disp_remove,
	.shutdown = display_driver_shutdown,
	.driver = {
		.name = "s5p-decon_ext-display",
		.owner = THIS_MODULE,
		.pm = &s5p_decon_ext_disp_ops,
		.of_match_table = of_match_ptr(decon_ext_disp_device_table),
	},
};

static int s5p_decon_ext_disp_register(void)
{
	return platform_driver_register(&s5p_decon_ext_disp_driver);
}

static void s5p_decon_ext_disp_unregister(void)
{
	platform_driver_unregister(&s5p_decon_ext_disp_driver);
}
late_initcall(s5p_decon_ext_disp_register);
module_exit(s5p_decon_ext_disp_unregister);

MODULE_AUTHOR("Donggyun, ko <donggyun.ko@samsung.com>");
MODULE_DESCRIPTION("Samusung DECON-DISP driver");
MODULE_LICENSE("GPL");
