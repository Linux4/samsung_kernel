/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include <mach/map.h>
#include <mach/regs-clock.h>

#include "fimc-is-config.h"
#include "fimc-is-type.h"
#include "fimc-is-regs.h"
#include "../../fimc-is-device-sensor.h"
#include "../../fimc-is-device-ischain.h"

int fimc_is_sensor_runtime_suspend_pre(struct device *dev)
{
	/*TODO*/

	return 0;
}

int fimc_is_sensor_runtime_resume_pre(struct device *dev)
{
	/*TODO*/

	return 0;
}

int fimc_is_ischain_runtime_suspend_post(struct device *dev)
{
	int ret = 0;
	/* if runtime PM is disable, turning off ISP local power is not needed to restart camera */
#if 0
	u32 val, timeout;

	/* check ISP power on */
	if (!(__raw_readl(PMUREG_ISP_STATUS) & 0xF)) {
		warn("ISP already power off state");
		goto p_err;
	}

	/* PHY CLK */
	/* HACK : after phy clock enable issue resolved, comment will remove */
	val = readl(EXYNOS3475_CLK_CON_MUX_PHYCLK_CSI_LINK_RX_BYTE_CLK_HS_USER);
	val |= (0 << 12);
	writel(val, EXYNOS3475_CLK_CON_MUX_PHYCLK_CSI_LINK_RX_BYTE_CLK_HS_USER);

	val = readl(EXYNOS3475_CLK_CON_MUX_PHYCLK_CSI_LINK_RX_BYTE_CLK_HS_USER);
	val |= (1 << 27);
	writel(val, EXYNOS3475_CLK_CON_MUX_PHYCLK_CSI_LINK_RX_BYTE_CLK_HS_USER);

	/* Measure power on/off duration of ISP by USE_SC_FEEDBACK */
	writel(0x1, PMUREG_ISP_OPTION);

	/* LPI MASK */
	writel(0x3, PMUREG_LPI_MASK_ISP_BUSMASTER);

	/* ISP power off */
	writel(0x0, PMUREG_ISP_CONFIGURATION);

	/* check power off */
	while ((readl(PMUREG_ISP_STATUS) & 0xF) && timeout) {
		timeout--;
		usleep_range(1000, 1000);
	}

	if (timeout == 0) {
		err("ISP power down fail(0x%08x)\n", readl(PMUREG_ISP_STATUS));
		ret = -ETIME;
		goto p_err;
	}

p_err:
#endif
	return ret;
}

int fimc_is_ischain_runtime_resume_pre(struct device *dev)
{
	u32 timeout;
	int ret = 0;

	if (__raw_readl(PMUREG_ISP_STATUS) == 0x0) {
		/* Measure power on/off duration of ISP by counter*/
		writel(0x1, PMUREG_ISP_OPTION);

		/* ISP power on */
		writel(0xF, PMUREG_ISP_CONFIGURATION);

		/* check power on */
		timeout = 1000;
		while (--timeout && ((__raw_readl(PMUREG_ISP_STATUS) & 0xF) != 0xF))
			udelay(1);

		if (!timeout) {
			err("ISP power on failed");
			ret = -EINVAL;
			goto p_err;
		}
	}

	/* PHY CLK control */
	/* HACK : after phy clock enable issue resolved, comment will remove
	 * writel((0x1 << 27), EXYNOS3475_CLK_CON_MUX_PHYCLK_CSI_LINK_RX_BYTE_CLK_HS_USER);
	 */
p_err:
	return ret;
}

int fimc_is_ischain_runtime_resume_post(struct device *dev)
{
	/*TODO*/

	return 0;
}

int fimc_is_runtime_suspend_post(struct device *dev)
{
	int ret = 0;
#if defined(CONFIG_PM_RUNTIME)
	u32 timeout;

	timeout = 1000;
	while ((readl(PMUREG_ISP_STATUS) & 0xF) && timeout) {
		timeout--;
		usleep_range(1000, 1000);
	}

	if (timeout == 0) {
		err("ISP power down fail(0x%08x)\n", readl(PMUREG_ISP_STATUS));
		ret = -ETIME;
		goto p_err;
	}

p_err:
#endif
	return ret;
}
