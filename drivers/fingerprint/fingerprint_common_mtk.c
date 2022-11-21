#include "fingerprint_common.h"


int spi_clk_enable(struct spi_clk_setting *clk_setting)
{
	int retval = 0;

#ifdef ENABLE_SENSORS_FPRINT_SECURE
	if (!clk_setting->enabled_clk) {
		retval = clk_prepare_enable(clk_setting->fp_spi_sclk);
		if (retval < 0) {
			pr_err("Unable to enable spi clk\n");
			return retval;
		}
		pr_debug("ENABLE_SPI_CLOCK %d\n", clk_setting->spi_speed);
		__pm_stay_awake(clk_setting->spi_wake_lock);
		clk_setting->enabled_clk = true;
	}
#endif

	return retval;
}

int spi_clk_disable(struct spi_clk_setting *clk_setting)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	if (clk_setting->enabled_clk) {
		clk_disable_unprepare(clk_setting->fp_spi_sclk);
		__pm_relax(clk_setting->spi_wake_lock);
		clk_setting->enabled_clk = false;
		pr_debug("DISABLE_SPI_CLOCK\n");
	}
#endif

	return 0;
}

int cpu_speedup_enable(struct boosting_config *boosting)
{
	return 0;
}

int cpu_speedup_disable(struct boosting_config *boosting)
{
	return 0;
}
