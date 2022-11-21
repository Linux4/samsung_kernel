/* Copyright (c) 2013-2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/sec-pinmux.h>
#include <linux/gpio.h>
#include <linux/debugfs.h>
#if IS_ENABLED(CONFIG_SEC_GPIO_DVS)
#include <linux/errno.h>
#include <linux/secgpio_dvs.h>
#include <linux/platform_device.h>
#endif /* CONFIG_SEC_GPIO_DVS */

/******************************************************************************
 * Define value in accordance with the specification of each BB vendor.
 ******************************************************************************/
#define AP_GPIO_MAX   203

#define GET_RESULT_GPIO(a, b, c)	\
	((a<<4 & 0xF0) | (b<<1 & 0xE) | (c & 0x1))

#if IS_ENABLED(CONFIG_SEC_GPIO_DVS)
/****************************************************************/
/* Pre-defined variables. (DO NOT CHANGE THIS!!) */
static unsigned char checkgpiomap_result[GDVS_PHONE_STATUS_MAX][AP_GPIO_MAX];
static struct gpiomap_result gpiomap_result = {
	.init = checkgpiomap_result[PHONE_INIT],
	.sleep = checkgpiomap_result[PHONE_SLEEP]
};
	
static void msm_check_gpio_status(unsigned char phonestate)
{
	int chip_base = get_msm_gpio_chip_base();
	struct gpio_chip *gp = gpio_to_chip(chip_base);
	struct gpiomux_setting set;
	int i;
	u8 temp_io = 0, temp_pdpu = 0, temp_lh = 0;

	if (gp == NULL) {
		pr_info("%s: Can't get msm ghip_chip, chip_base = %d\n",
				__func__, chip_base);
		return;
	}

	pr_info("[dvs_%s] state : %s\n", __func__,
		(phonestate == PHONE_INIT) ? "init" : "sleep");

	for (i = 0; i < AP_GPIO_MAX; i++) {
		/* If it is not valid gpio or secure, Shows IN/PD/L */
		if (!sec_gpiochip_line_is_valid(gp, i)) {
			checkgpiomap_result[phonestate][i] =
				GET_RESULT_GPIO(0x1, 0x1, 0x0);
			continue;
		}
		
		msm_gp_get_all(gp, i, &set);

		if (set.func == GPIOMUX_FUNC_GPIO) {
			if (set.is_out)
				temp_io = 0x02;	/* GPIO_OUT */
			else
				temp_io = 0x01;	/* GPIO_IN */
		} else
			temp_io = 0x0;		/* FUNC */

		switch (set.pull) {
		case GPIOMUX_PULL_NONE:
			temp_pdpu = 0x00;
			break;
		case GPIOMUX_PULL_DOWN:
			temp_pdpu = 0x01;
			break;
		case GPIOMUX_PULL_UP:
			temp_pdpu = 0x02;
			break;
		case GPIOMUX_PULL_KEEPER:
			temp_pdpu = 0x03;
			break;
		default:
			temp_pdpu = 0x07;
			break;
		}

		temp_lh = set.val;

		checkgpiomap_result[phonestate][i] =
			GET_RESULT_GPIO(temp_io, temp_pdpu, temp_lh);
	}

	pr_info("[dvs_%s]-\n", __func__);
}

static int msm_get_gpiochip_base(void)
{
	int chip_base = get_msm_gpio_chip_base();
	return chip_base;
}

static struct gpio_dvs msm_gpio_dvs = {
	.result = &gpiomap_result,
	.check_gpio_status = msm_check_gpio_status,
	.count = AP_GPIO_MAX,
	.check_init = false,
	.check_sleep = false,
	.get_gpio_base = msm_get_gpiochip_base,
};
/****************************************************************/
#endif /* CONFIG_SEC_GPIO_DVS */

static const char * const gpiomux_func_str[] = {
	"GPIO",
	"Func_1",
	"Func_2",
	"Func_3",
	"Func_4",
	"Func_5",
	"Func_6",
	"Func_7",
	"Func_8",
	"Func_9",
	"Func_a",
	"Func_b",
	"Func_c",
	"Func_d",
	"Func_e",
	"Func_f",
};

static const char * const gpiomux_pull_str[] = {
	"PULL_NONE",
	"PULL_DOWN",
	"PULL_KEEPER",
	"PULL_UP",
};

static const char * const gpiomux_dir_str[] = {
	"IN",
	"OUT",
};

static const char * const gpiomux_val_str[] = {
	"LOW",
	"HIGH",
};

static void gpiomux_debug_print(struct seq_file *m)
{
	int chip_base = get_msm_gpio_chip_base();
	struct gpio_chip *gp = gpio_to_chip(chip_base);
	struct gpiomux_setting set = {0,};
	int i;

	if (gp == NULL) {
		pr_info("%s: Can't get msm ghip_chip, chip_base = %d\n",
				__func__, chip_base);
		return;
	}

	for (i = 0; i < AP_GPIO_MAX; i++) {
		if(!sec_gpiochip_line_is_valid(gp, i))
			continue;

		msm_gp_get_all(gp, i, &set);

		if (IS_ERR_OR_NULL(m)) {
			pr_info("GPIO[%u] %10s %10s %13s DRV_%dmA %10s\n",
				i,
				gpiomux_func_str[set.func],
				gpiomux_dir_str[set.is_out],
				gpiomux_pull_str[set.pull],
				set.drv,
				gpiomux_val_str[set.val]);
		} else {
			seq_printf(m, "GPIO[%u] \t%s \t%s \t%s \tDRV_%dmA \t%s\n",
				i,
				gpiomux_func_str[set.func],
				gpiomux_dir_str[set.is_out],
				gpiomux_pull_str[set.pull],
				set.drv,
				gpiomux_val_str[set.val]);
		}
	}
}

void msm_gpio_print_enabled(void)
{
	gpiomux_debug_print(NULL);
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
static int gpiomux_debug_showall(struct seq_file *m, void *unused)
{
	gpiomux_debug_print(m);
	return 0;
}

static int gpiomux_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, gpiomux_debug_showall, inode->i_private);
}

static const struct file_operations gpiomux_operations = {
	.open		= gpiomux_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int __init msm_gpiomux_debug_init(void)
{
	debugfs_create_file("gpiomux", 0440,
				NULL, NULL, &gpiomux_operations);
	return 0;
}
late_initcall(msm_gpiomux_debug_init);
#endif /* CONFIG_DEBUG_FS */

static int __init msm_gpiomux_init(void)
{
#if IS_ENABLED(CONFIG_SEC_GPIO_DVS)
	gpio_dvs_check_initgpio();
#endif
	return 0;
}
late_initcall(msm_gpiomux_init);

#if IS_ENABLED(CONFIG_SEC_GPIO_DVS)
static struct platform_device secgpio_dvs_device = {
	.name	= "secgpio_dvs",
	.id	= -1,
	/****************************************************************
	 * Designate appropriate variable pointer
	 * in accordance with the specification of each BB vendor.
	 ***************************************************************/
	.dev.platform_data = &msm_gpio_dvs,
};

static struct platform_device *secgpio_dvs_devices[] __initdata = {
	&secgpio_dvs_device,
};

static int __init secgpio_dvs_device_init(void)
{
	return platform_add_devices(
		secgpio_dvs_devices, ARRAY_SIZE(secgpio_dvs_devices));
}
arch_initcall(secgpio_dvs_device_init);
#endif /* CONFIG_SEC_GPIO_DVS */
