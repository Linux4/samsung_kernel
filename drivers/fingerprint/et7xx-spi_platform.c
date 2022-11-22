/*
 * Copyright (C) 2016 Samsung Electronics. All rights reserved.
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
#include "et7xx.h"

int fps_resume_set(void) {
	return 0;
}

int fps_suspend_set(struct et7xx_data *etspi) {
	return 0;
}

int et7xx_register_platform_variable(struct et7xx_data *etspi)
{
	int retval = 0;

	pr_info("Entry\n");
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	etspi->fp_spi_pclk = devm_clk_get(etspi->dev, "gate_spi_clk");
	if (IS_ERR(etspi->fp_spi_pclk)) {
		pr_err("Can't get gate_spi_clk\n");
		return PTR_ERR(etspi->fp_spi_pclk);
	}

	etspi->fp_spi_sclk = devm_clk_get(etspi->dev, "ipclk_spi");
	if (IS_ERR(etspi->fp_spi_sclk)) {
		pr_err("Can't get ipclk_spi\n");
		return PTR_ERR(etspi->fp_spi_sclk);
	}
#endif

	return retval;
}

int et7xx_unregister_platform_variable(struct et7xx_data *etspi)
{
	int retval = 0;

	pr_info("Entry\n");
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	clk_put(etspi->fp_spi_pclk);
	clk_put(etspi->fp_spi_sclk);
#endif

	return retval;
}


#ifdef ENABLE_SENSORS_FPRINT_SECURE
static int et7xx_sec_spi_prepare(struct et7xx_data *etspi)
{
	u32 clk_speed = etspi->spi_speed;
	int retval = 0;

	clk_prepare_enable(etspi->fp_spi_pclk);
	clk_prepare_enable(etspi->fp_spi_sclk);

	if (clk_get_rate(etspi->fp_spi_sclk) != (clk_speed * 4)) {
		retval = clk_set_rate(etspi->fp_spi_sclk, clk_speed * 4);
		if (retval < 0)
			pr_err("SPI clk set failed: %d\n", retval);
		else
			pr_debug("Set SPI clock rate: %u(%lu)\n",
				clk_speed, clk_get_rate(etspi->fp_spi_sclk) / 4);
	} else {
		pr_debug("Set SPI clock rate: %u(%lu)\n",
				clk_speed, clk_get_rate(etspi->fp_spi_sclk) / 4);
	}

	return retval;
}

static int et7xx_sec_spi_unprepare(struct et7xx_data *etspi)
{
	clk_disable_unprepare(etspi->fp_spi_pclk);
	clk_disable_unprepare(etspi->fp_spi_sclk);

	return 0;
}
#endif

int et7xx_spi_clk_enable(struct et7xx_data *etspi)
{
	int retval = 0;

#ifdef ENABLE_SENSORS_FPRINT_SECURE
	if (!etspi->enabled_clk) {
		retval = et7xx_sec_spi_prepare(etspi);
		if (retval < 0)
			pr_err("Unable to enable spi clk\n");
		else
			pr_debug("ENABLE_SPI_CLOCK %d\n", etspi->spi_speed);
		__pm_stay_awake(etspi->fp_spi_lock);
		etspi->enabled_clk = true;
	}
#endif

	return retval;
}

int et7xx_spi_clk_disable(struct et7xx_data *etspi)
{
	int retval = 0;

#ifdef ENABLE_SENSORS_FPRINT_SECURE
	if (etspi->enabled_clk) {
		retval = et7xx_sec_spi_unprepare(etspi);
		if (retval < 0)
			pr_err("couldn't disable spi clks\n");
		__pm_relax(etspi->fp_spi_lock);
		etspi->enabled_clk = false;
		pr_debug("clk disalbed\n");
	}
#endif

	return retval;
}
