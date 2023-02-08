/* linux/arch/arm/mach-exynos/board-universal5420-gpio.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include <asm/system_info.h>

#include "board-universal5260.h"

#define GPIO_LV_L	0	/* GPIO level low */
#define GPIO_LV_H	1	/* GPIO level high */
#define GPIO_LV_N	2	/* GPIO level none */

struct gpio_init_data {
	u32 num;
	u32 cfg;
	u32 val;
	u32 pud;
};

struct gpio_sleep_data {
	u32 num;
	u32 cfg;
	u32 pud;
};

struct sleep_gpio_tables {
	struct gpio_sleep_data *table;
	u32 arr_size;
};

#define MAX_BOARD_REV	0xf
static struct sleep_gpio_tables xy5260_sleep_gpio_tables[MAX_BOARD_REV];
static int nr_xy5260_sleep_gpio_table;

static struct gpio_init_data __initdata init_gpio_table[] = {
#if 0
	{ EXYNOS5420_GPX0(0), S3C_GPIO_INPUT, GPIO_LV_N, S3C_GPIO_PULL_NONE }, /* WACOM_SENSE */
	{ EXYNOS5420_GPX1(5), S3C_GPIO_INPUT, GPIO_LV_N, S3C_GPIO_PULL_NONE }, /* FUEL_ALERT */
	{ EXYNOS5420_GPX3(3), S3C_GPIO_INPUT, GPIO_LV_N, S3C_GPIO_PULL_NONE }, /* MCU_AP_INT_1_1.8V */
	{ EXYNOS5420_GPX3(4), S3C_GPIO_INPUT, GPIO_LV_N, S3C_GPIO_PULL_NONE }, /* MCU_AP_INT_1_1.8V */
#endif
};

static struct gpio_sleep_data sleep_gpio_table[] = {
	{ EXYNOS5260_GPB5(0), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE }, /* IF_PMIC_SDA */
	{ EXYNOS5260_GPB5(1), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE }, /* IF_PMIC_SCL */
};

/* This funtion will be init breakpoint for GPIO verification */
void gpio_init_done(void)
{
printk(KERN_CRIT "%s GPIO!!!!!!!!!!!!!!!!!!\n",__func__);
	pr_info("%s\n", __func__);
}

static void __init config_init_gpio(void)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(init_gpio_table); i++) {
		u32 gpio = init_gpio_table[i].num;
		s3c_gpio_setpull(gpio, init_gpio_table[i].pud);
		s3c_gpio_cfgpin(gpio, init_gpio_table[i].cfg);

		if (init_gpio_table[i].cfg == S3C_GPIO_OUTPUT)
			gpio_set_value(gpio, init_gpio_table[i].val);
	}

	gpio_init_done();
}


static void config_sleep_gpio(struct gpio_sleep_data *table, u32 arr_size)
{
	u32 i;

	for (i = 0; i < arr_size; i++) {
		s5p_gpio_set_pd_pull(table[i].num, table[i].pud);
		s5p_gpio_set_pd_cfg(table[i].num, table[i].cfg);
	}
}

static void config_sleep_gpio_tables(void)
{
	int i;

	for (i = nr_xy5260_sleep_gpio_table - 1; i >= 0; i--)
		config_sleep_gpio(xy5260_sleep_gpio_tables[i].table,
				xy5260_sleep_gpio_tables[i].arr_size);
}

void __init exynos5_universal5260_gpio_init(void)
{
	int i = 0;

	if (system_rev > MAX_BOARD_REV)
		panic("Invalid Board Revision: %d", system_rev);

	config_init_gpio();

	xy5260_sleep_gpio_tables[i].table = sleep_gpio_table;
	xy5260_sleep_gpio_tables[i].arr_size =
		ARRAY_SIZE(sleep_gpio_table);
	i++;

	nr_xy5260_sleep_gpio_table = i;

	exynos_config_sleep_gpio = config_sleep_gpio_tables;
}
