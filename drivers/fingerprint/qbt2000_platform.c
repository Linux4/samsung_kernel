/*
 * Copyright (C) 2018 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "fingerprint.h"
#include "qbt2000_common.h"

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
#include <soc/samsung/exynos_pm_qos.h>
static struct exynos_pm_qos_request fingerprint_boost_qos;
#endif

int qbt2000_sec_spi_prepare(struct qbt2000_drvdata *drvdata)
{
	int rc = 0;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	int speed = drvdata->spi_speed;

	clk_prepare_enable(drvdata->fp_spi_pclk);
	clk_prepare_enable(drvdata->fp_spi_sclk);

	if (clk_get_rate(drvdata->fp_spi_sclk) != (speed * 4)) {
		rc = clk_set_rate(drvdata->fp_spi_sclk, speed * 4);
		if (rc < 0)
			pr_err("SPI clk set failed: %d\n", rc);
		else
			pr_debug("Set SPI clock rate: %u(%lu)\n", 
				speed, clk_get_rate(drvdata->fp_spi_sclk) / 4);
	} else {
		pr_debug("Set SPI clock rate: %u(%lu)\n",
				speed, clk_get_rate(drvdata->fp_spi_sclk) / 4);
	}
#endif
	return rc;
}

int qbt2000_sec_spi_unprepare(struct qbt2000_drvdata *drvdata)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	clk_disable_unprepare(drvdata->fp_spi_pclk);
	clk_disable_unprepare(drvdata->fp_spi_sclk);
#endif
	return 0;
}

int qbt2000_set_clk(struct qbt2000_drvdata *drvdata, bool onoff)
{
	int rc = 0;

	if (drvdata->enabled_clk == onoff) {
		pr_err("already %s\n", onoff ? "enabled" : "disabled");
		return rc;
	}

	if (onoff) {
		rc = qbt2000_sec_spi_prepare(drvdata);
		if (rc < 0) {
			pr_err("couldn't enable spi clk: %d\n", rc);
			return rc;
		}
		__pm_stay_awake(drvdata->fp_spi_lock);
		drvdata->enabled_clk = true;
	} else {
		rc = qbt2000_sec_spi_unprepare(drvdata);
		if (rc < 0) {
			pr_err("couldn't disable spi clk: %d\n", rc);
			return rc;
		}
		__pm_relax(drvdata->fp_spi_lock);
		drvdata->enabled_clk = false;
	}
	return rc;
}

int fps_resume_set(void) {
	return 0;
}

int qbt2000_pinctrl_register(struct qbt2000_drvdata *drvdata)
{
	int rc = 0;

	drvdata->p = pinctrl_get_select_default(drvdata->dev);
	if (IS_ERR(drvdata->p)) {
		rc = -EINVAL;
		pr_err("failed pinctrl_get\n");
		goto pinctrl_register_default_exit;
	}

#if !defined(ENABLE_SENSORS_FPRINT_SECURE) || defined(DISABLED_GPIO_PROTECTION)
	drvdata->pins_poweroff = pinctrl_lookup_state(drvdata->p, "pins_poweroff");
	if (IS_ERR(drvdata->pins_poweroff)) {
		pr_err("could not get pins sleep_state (%li)\n",
			PTR_ERR(drvdata->pins_poweroff));
		drvdata->pins_poweroff = NULL;
		drvdata->pins_poweron = NULL;
		rc = -EINVAL;
		goto pinctrl_register_exit;
	}

	drvdata->pins_poweron = pinctrl_lookup_state(drvdata->p, "pins_poweron");
	if (IS_ERR(drvdata->pins_poweron)) {
		pr_err("could not get pins idle_state (%li)\n",
			PTR_ERR(drvdata->pins_poweron));
		drvdata->pins_poweron = NULL;
		rc = -EINVAL;
		goto pinctrl_register_exit;
	}
#endif
	pr_info("finished\n");
	return rc;
#if !defined(ENABLE_SENSORS_FPRINT_SECURE) || defined(DISABLED_GPIO_PROTECTION)
pinctrl_register_exit:
	pinctrl_put(drvdata->p);
#endif
pinctrl_register_default_exit:
	pr_err("failed %d\n", rc);
	return rc;
}

int qbt2000_register_platform_variable(struct qbt2000_drvdata *drvdata)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	drvdata->fp_spi_pclk = devm_clk_get(drvdata->dev, "gate_spi_clk");
	if (IS_ERR(drvdata->fp_spi_pclk)) {
		pr_err("Can't get gate_spi_clk\n");
		return PTR_ERR(drvdata->fp_spi_pclk);
	}

	drvdata->fp_spi_sclk = devm_clk_get(drvdata->dev, "ipclk_spi");
	if (IS_ERR(drvdata->fp_spi_sclk)) {
		pr_err("Can't get ipclk_spi\n");
		return PTR_ERR(drvdata->fp_spi_sclk);
	}
#endif
	return 0;
}

int qbt2000_unregister_platform_variable(struct qbt2000_drvdata *drvdata)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	clk_put(drvdata->fp_spi_pclk);
	clk_put(drvdata->fp_spi_sclk);
#endif
	return 0;
}

int qbt2000_set_cpu_speedup(struct qbt2000_drvdata *drvdata, int onoff)
{
	int rc = 0;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	if (onoff) {
		pr_info("SPEEDUP ON:%d\n", onoff);
		exynos_pm_qos_add_request(&fingerprint_boost_qos, PM_QOS_CLUSTER1_FREQ_MIN, PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);
	} else {
		pr_info("SPEEDUP OFF:%d\n", onoff);
		exynos_pm_qos_remove_request(&fingerprint_boost_qos);
	}
#endif
#endif
	return rc;
}
