/*
 * Samsung Elec.
 *
 * drivers/gpio/secgpio_dvs_mt.c - Read GPIO for MT6755 of MTK
 *
 * Copyright (C) 2016, Samsung Electronics.
 *
 * This program is free software. You can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation
 */

#include <linux/secgpio_dvs.h>
#include <mt-plat/mt_gpio.h>
#include <mt-plat/mt_gpio_core.h>

#define AP_GPIO_COUNT 204

static unsigned char checkgpiomap_result[GDVS_PHONE_STATUS_MAX][AP_GPIO_COUNT];
static struct gpiomap_result gpiomap_result = {
	.init = checkgpiomap_result[PHONE_INIT],
	.sleep = checkgpiomap_result[PHONE_SLEEP]
};

void mt_get_gpio_info(unsigned int index, unsigned char* direction, unsigned char* level, unsigned char* pull, unsigned char* func, unsigned char phonestate);

enum gpiomux_dir {
	GPIOMUX_IN = 0,
	GPIOMUX_OUT_HIGH,
};
enum gpiomux_level {
	GPIOMUX_LOW=0,
	GPIOMUX_HIGH,
};
enum gpiomux_pull {
	GPIOMUX_PULL_NONE = 0,
	GPIOMUX_PULL_DOWN,
	GPIOMUX_PULL_UP,
};

/****************************************************************/
/* Define value in accordance with
   the specification of each BB vendor. */
/****************************************************************/

enum {
	GPIO_IN_BIT  = 0,
	GPIO_OUT_BIT = 1
};

#define GET_RESULT_GPIO(a, b, c)        \
	((a<<4 & 0xF0) | (b<<1 & 0xE) | (c & 0x1))

/***************************************************************
  Pre-defined variables. (DO NOT CHANGE THIS!!)
  static unsigned char checkgpiomap_result[GDVS_PHONE_STATUS_MAX][AP_GPIO_COUNT];
  static struct gpiomap_result gpiomap_result = {
  .init = checkgpiomap_result[PHONE_INIT],
  .sleep = checkgpiomap_result[PHONE_SLEEP]
  };
 ***************************************************************/
/*
 *  index of dvs, gpio number, the gpio control register of the corresponding gpio number
 */
extern struct gpio_dvs mt_gpio_dvs;


void mt_get_gpio_info(unsigned int gpio_num,
		unsigned char* direction, /* set 1-output or 0-input */
		unsigned char* level,
		unsigned char* pull, /*  set  0-no pull up and pull down/1-pull down/2-pull     up */
		unsigned char* func, unsigned char phonestate)
{
	*level = 0xff;
	*direction = 0xff;
	*pull = 0xff;
	*func = 0xff;

	if (gpio_num > AP_GPIO_COUNT) {
		return;
	}

	*func = mt_get_gpio_mode_base(gpio_num);

	if (mt_get_gpio_pull_enable_base(gpio_num) == GPIO_PULL_UNSUPPORTED || mt_get_gpio_pull_enable_base(gpio_num) == 0) {
		*pull = 0; /* NP */
		
//		if(mt_get_gpio_pull_enable_base(gpio_num) == GPIO_PULL_UNSUPPORTED)
//			pr_info("gpio[%d] PULL not supported\n", gpio_num);		
	}
	else {
		if (mt_get_gpio_pull_select_base(gpio_num))
			*pull = 2; /* PU */
		else
			*pull = 1; /* PD */
	}

	if (*func == 0) /* GPIO */
	{
		if (mt_get_gpio_dir_base(gpio_num)) /* 0 : I, 1 : O*/
		{
			*direction = 1; 
			*level = mt_get_gpio_out_base(gpio_num);
		}
		else
		{
			*direction = 0; 
			*level = mt_get_gpio_in_base(gpio_num);
		}
	}

}

static void mt_check_gpio_status(unsigned char phonestate, unsigned int num)
{
	unsigned char direction, level, pull, func;

	u8 temp_io = 0, temp_pdpu = 0, temp_lh = 0;

	mt_get_gpio_info(num, &direction, &level, &pull, &func, phonestate);

	if (func == 0)
	{
		if (direction == GPIOMUX_IN)
			temp_io = 0x1;  /* GPIO_IN */
		else
			temp_io = 0x2;  /* GPIO_OUT */
	}
	else
	{
		temp_io = 0x0;      /* FUNC */
	}

	if (pull  == GPIOMUX_PULL_NONE)
		temp_pdpu = 0x0;
	else if (pull  == GPIOMUX_PULL_DOWN)
		temp_pdpu = 0x1;
	else if (pull == GPIOMUX_PULL_UP)
		temp_pdpu = 0x2;
	if(level == GPIOMUX_LOW || level == GPIOMUX_HIGH)
	{
		if (level == GPIOMUX_LOW)
			temp_lh = 0x0;
		else if (level == GPIOMUX_HIGH)
			temp_lh = 0x1;
	}
	else
		temp_lh = 0xE;


	checkgpiomap_result[phonestate][num] =
		GET_RESULT_GPIO(temp_io, temp_pdpu, temp_lh);

	return;
}

static void check_gpio_status(unsigned char phonestate)
{
	unsigned int i;

	pr_info("[secgpio_dvs][%s] state : %s\n", __func__,
			(phonestate == PHONE_INIT) ? "init" : "sleep");

	for (i= 0; i < mt_gpio_dvs.count; i++)
		mt_check_gpio_status(phonestate, i);
	return ;
}

struct gpio_dvs mt_gpio_dvs = {
	.result =&gpiomap_result,
	.check_gpio_status = check_gpio_status,
	.count = AP_GPIO_COUNT,
};



struct platform_device secgpio_dvs_device = {
	.name   = "secgpio_dvs",
	.id             = -1,
	/****************************************************************/
	/* Designate appropriate variable pointer
	   in accordance with the specification of each BB vendor.*/
	.dev.platform_data = &mt_gpio_dvs,
	/****************************************************************/
};

static struct platform_device *secgpio_dvs_devices[] __initdata= {
	&secgpio_dvs_device,
};

static int __init secgpio_dvs_device_init(void)
{
	return platform_add_devices(
			secgpio_dvs_devices, ARRAY_SIZE(secgpio_dvs_devices));
}
arch_initcall(secgpio_dvs_device_init);
