/*
 * arch/arm/mach-exynos/board-kmini-fpga.c
 *
 * c source file supporting fpga in kmini board
 *
 * Copyright (C) 2013 Samsung Electronics
 * Jeong-Seok Yang <jseok.yang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <plat/gpio-cfg.h>
#include "board-universal222ap.h"

#if defined(CONFIG_IR_REMOCON_MC96FR116C)
#include <linux/platform_data/mc96fr116c.h>

#define GPIO_IRDA_WAKE		EXYNOS4_GPM0(2)	/* P00/WAKE */
#define GPIO_IRDA_IRQ		EXYNOS4_GPM0(4)	/* P01/READY */
#define GPIO_IRDA_RESET		EXYNOS4_GPM3(1)	/* P12/RESETB */

static void irda_wake_en(bool onoff)
{
	gpio_set_value(GPIO_IRDA_WAKE, onoff);
}

static int irda_hwreset(void)
{
	gpio_set_value(GPIO_IRDA_RESET, 0);
	msleep(20);
	gpio_direction_output(GPIO_IRDA_WAKE, 0); /* Bootloader mode */
	gpio_set_value(GPIO_IRDA_RESET, 1);
	msleep(53);
	gpio_direction_output(GPIO_IRDA_WAKE, 1); /* Not for user IR mode */

	return 0;
}

static int irda_device_init(void)
{
	int ret;

	pr_debug("%s called!\n", __func__);

	ret = gpio_request(GPIO_IRDA_RESET, "irda_reset");
	if (ret) {
		pr_err("%s: gpio_request GPIO_IRDA_RESET fail[%d], ret = %d\n",
				__func__, GPIO_IRDA_RESET, ret);
		return ret;
	}
	gpio_direction_output(GPIO_IRDA_RESET, 1); /* The reset will do later */
	s3c_gpio_setpull(GPIO_IRDA_RESET, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_pd_cfg(GPIO_IRDA_RESET, S5P_GPIO_PD_OUTPUT1);
	s5p_gpio_set_pd_pull(GPIO_IRDA_RESET, S5P_GPIO_PD_UPDOWN_DISABLE);

	ret = gpio_request(GPIO_IRDA_WAKE, "irda_wake");
	if (ret) {
		pr_err("%s: gpio_request GPIO_IRDA_WAKE fail[%d], ret = %d\n",
				__func__, GPIO_IRDA_WAKE, ret);
		goto error;
	}
	gpio_direction_output(GPIO_IRDA_WAKE, 1); /* Not for user IR mode */
	s3c_gpio_setpull(GPIO_IRDA_WAKE, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_pd_cfg(GPIO_IRDA_WAKE, S5P_GPIO_PD_OUTPUT1);
	s5p_gpio_set_pd_pull(GPIO_IRDA_WAKE, S5P_GPIO_PD_UPDOWN_DISABLE);

	ret = gpio_request(GPIO_IRDA_IRQ, "irda_irq");
	s3c_gpio_cfgpin(GPIO_IRDA_IRQ, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_IRDA_IRQ, S3C_GPIO_PULL_UP);
	gpio_direction_input(GPIO_IRDA_IRQ);
	s5p_gpio_set_pd_cfg(GPIO_IRDA_IRQ, S5P_GPIO_PD_INPUT);
	s5p_gpio_set_pd_pull(GPIO_IRDA_IRQ, S5P_GPIO_PD_DOWN_ENABLE);

	irda_hwreset();

	return ret;
error:
	gpio_free(GPIO_IRDA_RESET);

	return ret;
}

static void irda_irled_onoff(bool onoff)
{
	struct regulator *vled_ic;
	int ret;

	vled_ic = regulator_get(NULL, "vled_3v3");
	if (IS_ERR(vled_ic)) {
		pr_err("could not get regulator vled_3v3\n");
		return;
	}

	if (onoff)
		ret = regulator_enable(vled_ic);
	else
		ret = regulator_disable(vled_ic);

	if (ret < 0)
		pr_warn("%s: regulator %sable is failed: %d\n", __func__,
				onoff ? "En" : "Dis", ret);

	regulator_put(vled_ic);
}

static int irda_read_ready(void)
{
	return gpio_get_value(GPIO_IRDA_IRQ);
}

static struct mc96fr116c_platform_data mc96fr116c_pdata __initdata = {
	.ir_reset	= irda_hwreset,
	.ir_wake_en	= irda_wake_en,
	.ir_irled_onoff	= irda_irled_onoff,
	.ir_read_ready	= irda_read_ready,
};

#endif /* CONFIG_IR_REMOCON_MC96FR116C */

void __init board_universal_ss222ap_init_fpga(void)
{
#if defined(CONFIG_IR_REMOCON_MC96FR116C)
	if (board_universal_ss222ap_add_platdata_gpio_i2c(1,
				&mc96fr116c_pdata) < 0)
		pr_err("%s: Error adding platform data of mc96fr116c\n",
				__func__);

	/* IR_LED */
	irda_device_init();
#endif /* CONFIG_IR_REMOCON_MC96FR116C */
}
