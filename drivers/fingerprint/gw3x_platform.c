#include "fingerprint.h"
#include "gw3x_common.h"
#include <linux/platform_data/spi-mt65xx.h>

int gw3x_register_platform_variable(struct gf_device *gf_dev)
{
	pr_info("Entry\n");
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	gf_dev->clk_setting->fp_spi_pclk = devm_clk_get(gf_dev->dev, "sel-clk");
	if (IS_ERR(gf_dev->clk_setting->fp_spi_pclk)) {
		pr_err("Failed to get sel-clk\n");
		return PTR_ERR(gf_dev->clk_setting->fp_spi_pclk);
	}

	gf_dev->clk_setting->fp_spi_sclk = devm_clk_get(gf_dev->dev, "spi-clk");
	if (IS_ERR(gf_dev->clk_setting->fp_spi_sclk)) {
		pr_err("Failed to get spi-clk\n");
		return PTR_ERR(gf_dev->clk_setting->fp_spi_sclk);
	}

	pr_info("Clock preparation done\n");
#endif

	return 0;
}

int gw3x_unregister_platform_variable(struct gf_device *gf_dev)
{
	pr_info("Entry\n");
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	clk_put(gf_dev->clk_setting->fp_spi_pclk);
	clk_put(gf_dev->clk_setting->fp_spi_sclk);
#endif

	return 0;
}

void gw3x_spi_setup_conf(struct gf_device *gf_dev, u32 bits)
{
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	gf_dev->spi->bits_per_word = 8;
	if (gf_dev->prev_bits_per_word != gf_dev->spi->bits_per_word) {
		if (spi_setup(gf_dev->spi))
			pr_err("failed to setup spi conf\n");
		pr_info("prev-bpw:%d, bpw:%d\n",
				gf_dev->prev_bits_per_word, gf_dev->spi->bits_per_word);
		gf_dev->prev_bits_per_word = gf_dev->spi->bits_per_word;
	}
#endif
}

int gw3x_pin_control(struct gf_device *gf_dev, bool pin_set)
{
	int status = 0;
	pr_info("Pin control entry");
	gf_dev->p->state = NULL;
	if (pin_set) {
		if (!IS_ERR(gf_dev->pins_poweron)) {
			status = pinctrl_select_state(gf_dev->p, gf_dev->pins_poweron);
			if (status)
				pr_err("can't set pin default state\n");
			pr_info("idle\n");
		}
	} else {
		if (!IS_ERR(gf_dev->pins_poweroff)) {
			status = pinctrl_select_state(gf_dev->p, gf_dev->pins_poweroff);
			if (status)
				pr_err("can't set pin sleep state\n");
			pr_info("sleep\n");
		}
	}
	return status;
}
