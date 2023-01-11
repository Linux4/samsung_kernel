/*
 * helanx smc generic driver.
 *
 * Copyright (C) 2015 Marvell Ltd.
 * Author: Xiaoguang Chen <chenxg@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/compiler.h>
#include <asm-generic/int-ll64.h>
#include <linux/helanx_smc.h>

static unsigned int get_gpio_address(unsigned long gpio)
{
	if (gpio <= 109)
		return GPIO_0_ADDR + gpio * 4;
	else if (gpio <= 116)
		return GPIO_110_ADDR + (gpio - 110) * 4;
	else if (gpio == 124)
		return GPIO_124_ADDR;
	else
		BUG_ON("GPIO number doesn't exist!\n");
}


int mfp_edge_wakeup_notify(struct notifier_block *nb,
			   unsigned long val, void *data)
{
	int error = 0;
	int (*invoke_smc_fn)(u64, u64) = __invoke_fn_smc;
	error = invoke_smc_fn(LC_ADD_GPIO_EDGE_WAKEUP, get_gpio_address(val));
	if (!error)
		return NOTIFY_OK;
	else
		return NOTIFY_BAD;
}

noinline int __invoke_fn_smc(u64 function_id, u64 arg0)
{
	asm volatile(
			__asmeq("%0", "x0")
			__asmeq("%1", "x1")
			"smc	#0\n"
		: "+r" (function_id)
		: "r" (arg0));

	return function_id;
}
