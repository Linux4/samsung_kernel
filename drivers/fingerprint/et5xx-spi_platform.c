/*
 * Copyright (C) 2020 Samsung Electronics. All rights reserved.
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
#include "et5xx.h"
#ifdef ENABLE_SENSORS_FPRINT_SECURE
#include <linux/platform_data/spi-mt65xx.h>
#if defined(CONFIG_SECURE_OS_BOOSTER_API)
#include <mach/secos_booster.h>
#elif defined(CONFIG_TZDEV_BOOST)
#if defined(CONFIG_TEEGRIS_VERSION) && (CONFIG_TEEGRIS_VERSION >= 4)
#include <../drivers/misc/tzdev/extensions/boost.h>
#else
#include <../drivers/misc/tzdev/tz_boost.h>
#endif
#endif
#endif

int fps_resume_set(void) {
	return 0;
}

int fps_suspend_set(struct et5xx_data *etspi) {
	return 0;
}

int et5xx_register_platform_variable(struct et5xx_data *etspi)
{
	int retval = 0;

	pr_info("Entry\n");
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	etspi->fp_parent_clk = devm_clk_get(etspi->dev, "parent-clk");
	if (IS_ERR(etspi->fp_parent_clk)) {
		pr_err("Failed to get parent-clk\n");
		return PTR_ERR(etspi->fp_parent_clk);
	}

	etspi->fp_spi_pclk = devm_clk_get(etspi->dev, "sel-clk");
	if (IS_ERR(etspi->fp_spi_pclk)) {
		pr_err("Failed to get sel-clk\n");
		return PTR_ERR(etspi->fp_spi_pclk);
	}

	etspi->fp_spi_sclk = devm_clk_get(etspi->dev, "spi-clk");
	if (IS_ERR(etspi->fp_spi_sclk)) {
		pr_err("Failed to get spi-clk\n");
		return PTR_ERR(etspi->fp_spi_sclk);
	}
	retval = clk_set_parent(etspi->fp_spi_pclk, etspi->fp_parent_clk);
	if (retval < 0) {
		pr_err("Failed to clk_set_parent (%d)\n", retval);
	}
	pr_info("Clock preparation done : %d\n", retval);
#endif
	return retval;
}

int et5xx_unregister_platform_variable(struct et5xx_data *etspi)
{
	int retval = 0;

	pr_info("Entry\n");
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	clk_put(etspi->fp_parent_clk);
	clk_put(etspi->fp_spi_pclk);
	clk_put(etspi->fp_spi_sclk);
#endif

	return retval;
}


#ifdef ENABLE_SENSORS_FPRINT_SECURE
static int et5xx_sec_spi_prepare(struct et5xx_data* etspi)
{
	u32 clk_speed = etspi->spi_speed;
	int retval = 0;
	retval = clk_prepare_enable(etspi->fp_spi_sclk);
	if (clk_get_rate(etspi->fp_spi_sclk) != etspi->spi_speed * SPI_CLK_DIV_MTK) {
		retval = clk_set_rate(etspi->fp_spi_sclk, etspi->spi_speed * SPI_CLK_DIV_MTK);
		if (retval < 0)
			pr_info("%s, SPI clk set failed : %d, %d\n", __func__, retval, etspi->spi_speed);
		else
			pr_info("%s, Set SPI clock rate0 : %u (%lu)\n",
				__func__, clk_speed, clk_get_rate(etspi->fp_spi_sclk) / SPI_CLK_DIV_MTK);
	} else {
		pr_info("%s, Set SPI clock rate1 : %u (%lu)\n",
			__func__, clk_speed, clk_get_rate(etspi->fp_spi_sclk) / SPI_CLK_DIV_MTK);
	}
	return retval;
}

static int et5xx_sec_spi_unprepare(struct et5xx_data *etspi)
{
	clk_disable_unprepare(etspi->fp_spi_sclk);
	return 0;
}
#endif

int et5xx_spi_clk_enable(struct et5xx_data *etspi)
{
	int retval = 0;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	if (!etspi->enabled_clk) {
		retval = et5xx_sec_spi_prepare(etspi);
		if (retval < 0)
			pr_err("Unable to enable spi clk\n");
		else
			pr_info("ENABLE_SPI_CLOCK %d\n", etspi->spi_speed);
		wake_lock(&etspi->fp_spi_lock);
		etspi->enabled_clk = true;
	}
#endif
	return retval;
}

int et5xx_spi_clk_disable(struct et5xx_data *etspi)
{
	int retval = 0;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	if (etspi->enabled_clk) {
		retval = et5xx_sec_spi_unprepare(etspi);
		if (retval < 0)
			pr_err("couldn't disable spi clks\n");
		wake_unlock(&etspi->fp_spi_lock);
		etspi->enabled_clk = false;
		pr_debug("clk disalbed\n");
	}
#endif
	return retval;
}

int et5xx_set_cpu_speedup(struct et5xx_data *etspi, int onoff)
{
	int retval = 0;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
#ifdef CONFIG_SECURE_OS_BOOSTER_API
	int retry_cnt = 0;
#endif

#if defined(CONFIG_TZDEV_BOOST)
	if (onoff) {
		pr_info("SPEEDUP ON:%d\n", onoff);
		tz_boost_enable();
	} else {
		pr_info("SPEEDUP OFF:%d\n", onoff);
		tz_boost_disable();
	}
#elif defined(CONFIG_SECURE_OS_BOOSTER_API)
	if (onoff) {
		do {
			retval = secos_booster_start(onoff - 1);
			retry_cnt++;
			if (retval) {
				pr_err("booster start failed. (%d) retry: %d\n",
					retval, retry_cnt);
				if (retry_cnt < 7)
					usleep_range(500, 510);
			}
		} while (retval && retry_cnt < 7);
	} else {
		retval = secos_booster_stop();
		if (retval)
			pr_err("booster stop failed. (%d)\n", retval);
	}
#else
		pr_err("CPU_SPEEDUP is not used\n");
#endif
#endif /* ENABLE_SENSORS_FPRINT_SECURE */
	return retval;
}
