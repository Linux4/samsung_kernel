/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 * Authors: Thomas Abraham <thomas.ab@samsung.com>
 *         Chander Kashyap <k.chander@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Common Clock Framework support for Emulator SoC.
 */
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <dt-bindings/clock/emulator.h>

#include "clk.h"
#include "clk-pll.h"

/*
 * list of controller registers to be saved and restored during a
 * suspend/resume cycle.
 */
/* fixed rate clocks generated outside the soc */
struct samsung_fixed_rate_clock emulator_fixed_rate_ext_clks[] __initdata = {
	FRATE(CLK_FIN_PLL, "fin_pll", NULL, 0, 26000000),
	/* for Emulator */
	/* FRATE(CLK_FIN_PLL, "fin_pll", NULL, 0, 1000000), */
};

/* After complete to prepare clock tree, these will be removed. */
struct samsung_fixed_rate_clock emulator_fixed_rate_clks[] __initdata = {
	FRATE(CLK_GATE_UART0, "gate_uart_clk0", NULL, 0, 26000000),
	FRATE(CLK_UART0, "ipclk_uart0", NULL, 0, 200000000),
	FRATE(CLK_MCT, "mct", NULL, 0, 26000000),
	/* for Emulator */
	/*
	FRATE(CLK_GATE_UART0, "gate_uart_clk0", NULL, 0, 1000000),
	FRATE(CLK_UART0, "ipclk_uart0", NULL, 0, 6451200),
	FRATE(CLK_MCT, "mct", NULL, 0, 1000000),
	*/
};

static __initdata struct of_device_id ext_clk_match[] = {
	{ .compatible = "samsung,emulator-oscclk", .data = (void *)0, },
	{ },
};

/* register emulator clocks */
void __init emulator_clk_init(struct device_node *np)
{
	struct samsung_clk_provider *ctx;
	void __iomem *reg_base;

	if (np) {
		reg_base = of_iomap(np, 0);
		if (!reg_base)
			panic("%s: failed to map registers\n", __func__);
	} else {
		panic("%s: unable to determine soc\n", __func__);
	}

	ctx = samsung_clk_init(np, reg_base, CLK_NR_CLKS);
	if (!ctx)
		panic("%s: unable to allocate context.\n", __func__);

	samsung_clk_of_register_fixed_ext(ctx, emulator_fixed_rate_ext_clks,
			ARRAY_SIZE(emulator_fixed_rate_ext_clks),
			ext_clk_match);

	samsung_clk_register_fixed_rate(ctx, emulator_fixed_rate_clks,
			ARRAY_SIZE(emulator_fixed_rate_clks));

	samsung_clk_of_add_provider(np, ctx);
}
CLK_OF_DECLARE(emulator_clk, "samsung,emulator-clock", emulator_clk_init);
