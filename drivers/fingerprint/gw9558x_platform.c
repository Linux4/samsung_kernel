#include "fingerprint.h"
#include "gw9558x_common.h"


#if defined(CONFIG_TZDEV_BOOST) && defined(ENABLE_SENSORS_FPRINT_SECURE)
#if defined(CONFIG_TEEGRIS_VERSION) && (CONFIG_TEEGRIS_VERSION >= 4)
#include <../drivers/misc/tzdev/extensions/boost.h>
#else
#include <../drivers/misc/tzdev/tz_boost.h>
#endif
#endif

int gw9558_register_platform_variable(struct gf_device *gf_dev)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	gf_dev->fp_spi_pclk = clk_get(NULL, "fp-spi-pclk");
	if (IS_ERR(gf_dev->fp_spi_pclk)) {
		pr_err("Can't get fp_spi_pclk\n");
		return PTR_ERR(gf_dev->fp_spi_pclk);
	}

	gf_dev->fp_spi_sclk = clk_get(NULL, "fp-spi-sclk");
	if (IS_ERR(gf_dev->fp_spi_sclk)) {
		pr_err("Can't get fp_spi_sclk\n");
		return PTR_ERR(gf_dev->fp_spi_sclk);
	}
#endif
	return 0;
}

int gw9558_unregister_platform_variable(struct gf_device *gf_dev)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	clk_put(gf_dev->fp_spi_pclk);
	clk_put(gf_dev->fp_spi_sclk);
#endif

	return 0;
}

void gw9558_spi_setup_conf(struct gf_device *gf_dev, u32 bits)
{
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	gf_dev->spi->bits_per_word = 8 * bits;
	if (gf_dev->prev_bits_per_word != gf_dev->spi->bits_per_word) {
		if (spi_setup(gf_dev->spi))
			pr_err("failed to setup spi conf\n");
		pr_info("prev-bpw:%d, bpw:%d\n",
				gf_dev->prev_bits_per_word, gf_dev->spi->bits_per_word);
		gf_dev->prev_bits_per_word = gf_dev->spi->bits_per_word;
	}
#endif
}

#ifdef ENABLE_SENSORS_FPRINT_SECURE
static int gw9558_sec_spi_prepare(struct gf_device *gf_dev)
{
	int retval = 0;
	unsigned int clk_speed = gf_dev->spi_speed;

	clk_prepare_enable(gf_dev->fp_spi_pclk);
	clk_prepare_enable(gf_dev->fp_spi_sclk);

	if (clk_get_rate(gf_dev->fp_spi_sclk) != (clk_speed * 4)) {
		retval = clk_set_rate(gf_dev->fp_spi_sclk, clk_speed * 4);
		if (retval < 0)
			pr_err("SPI clk set failed: %d\n", retval);
		else
			pr_debug("Set SPI clock rate: %u(%lu)\n",
				clk_speed, clk_get_rate(gf_dev->fp_spi_sclk) / 4);
	} else {
		pr_debug("Set SPI clock rate: %u(%lu)\n",
				clk_speed, clk_get_rate(gf_dev->fp_spi_sclk) / 4);
	}

	return retval;
}

static int gw9558_sec_spi_unprepare(struct gf_device *gf_dev)
{
	clk_disable_unprepare(gf_dev->fp_spi_pclk);
	clk_disable_unprepare(gf_dev->fp_spi_sclk);

	return 0;
}
#endif

int gw9558_spi_clk_enable(struct gf_device *gf_dev)
{
	int retval = 0;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	if (!gf_dev->enabled_clk) {
		retval = gw9558_sec_spi_prepare(gf_dev);
		if (retval < 0) {
			pr_err("Unable to enable spi clk. %d\n", retval);
		} else {
			pr_debug("ENABLE_SPI_CLOCK %d\n", gf_dev->spi_speed);
			wake_lock(&gf_dev->wake_lock);
			gf_dev->enabled_clk = true;
		}
	}
#endif
	return retval;
}

int gw9558_spi_clk_disable(struct gf_device *gf_dev)
{
	int retval = 0;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	if (gf_dev->enabled_clk) {
		retval = gw9558_sec_spi_unprepare(gf_dev);
		if (retval < 0) {
			pr_err("couldn't disable spi clks\n");
		} else {
			wake_unlock(&gf_dev->wake_lock);
			gf_dev->enabled_clk = false;
			pr_debug("DISABLE_SPI_CLOCK\n");
		}
	}
#endif
	return retval;
}

int gw9558_set_cpu_speedup(struct gf_device *gf_dev, int onoff)
{
#if defined(CONFIG_TZDEV_BOOST) && defined(ENABLE_SENSORS_FPRINT_SECURE)
	if (onoff) {
		pr_info("SPEEDUP ON:%d\n", onoff);
		tz_boost_enable();
	} else {
		pr_info("SPEEDUP OFF:%d\n", onoff);
		tz_boost_disable();
	}
#else
		pr_err("CPU_SPEEDUP is not used\n");
#endif
	return 0;
}

int gw9558_pin_control(struct gf_device *gf_dev, bool pin_set)
{
	int retval = 0;
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	if (pin_set) {
		if (!IS_ERR(gf_dev->pins_poweron)) {
			retval = pinctrl_select_state(gf_dev->p,
				gf_dev->pins_poweron);
			if (retval)
				pr_err("can't set pin default state\n");
			pr_debug("idle\n");
		}
	} else {
		if (!IS_ERR(gf_dev->pins_poweroff)) {
			retval = pinctrl_select_state(gf_dev->p,
				gf_dev->pins_poweroff);
			if (retval)
				pr_err("can't set pin sleep state\n");
			pr_debug("sleep\n");
		}
	}
#endif
	return retval;
}

