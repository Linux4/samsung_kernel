/*
 *  linux/arch/arm64/mach/mmpx-dt.c
 *
 *  Copyright (C) 2012 Marvell Technology Group Ltd.
 *  Author: Haojian Zhuang <haojian.zhuang@marvell.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publishhed by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/clocksource.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>

#include <asm/mach/arch.h>
#include <linux/cputype.h>

#include <media/soc_camera.h>
#include <media/mrvl-camera.h>

#include "mmpx-dt.h"
#include "regs-addr.h"

#ifdef CONFIG_SD8XXX_RFKILL
#include <linux/sd8x_rfkill.h>
#endif

#ifdef CONFIG_SOC_CAMERA_S5K8AA
#define CCIC2_PWDN_GPIO 13
#define CCIC2_RESET_GPIO 111
static int s5k8aa_sensor_power(struct device *dev, int on)
{
	static struct regulator *avdd_2v8;
	static struct regulator *dovdd_1v8;
	static struct regulator *af_2v8;
	static struct regulator *dvdd_1v2;
	int cam_enable, cam_reset;
	int ret;

	/* Get the regulators and never put it */
	/*
	 * The regulators is for sensor and should be in sensor driver
	 * As SoC camera does not support device tree, adding code here
	 */

	if (!avdd_2v8) {
		avdd_2v8 = regulator_get(dev, "avdd_2v8");
		if (IS_ERR(avdd_2v8)) {
			dev_warn(dev, "Failed to get regulator avdd_2v8\n");
			avdd_2v8 = NULL;
		}
	}

	if (!dovdd_1v8) {
		dovdd_1v8 = regulator_get(dev, "dovdd_1v8");
		if (IS_ERR(dovdd_1v8)) {
			dev_warn(dev, "Failed to get regulator dovdd_1v8\n");
			dovdd_1v8 = NULL;
		}
	}

	if (!af_2v8) {
		af_2v8 = regulator_get(dev, "af_2v8");
		if (IS_ERR(af_2v8)) {
			dev_warn(dev, "Failed to get regulator af_2v8\n");
			af_2v8 = NULL;
		}
	}

	if (!dvdd_1v2) {
		dvdd_1v2 = regulator_get(dev, "dvdd_1v2");
		if (IS_ERR(dvdd_1v2)) {
			dev_warn(dev, "Failed to get regulator dvdd_1v2\n");
			dvdd_1v2 = NULL;
		}
	}

	cam_enable = of_get_named_gpio(dev->of_node, "pwdn-gpios", 0);
	if (gpio_is_valid(cam_enable)) {
		if (gpio_request(cam_enable, "CAM2_POWER")) {
			dev_err(dev, "Request GPIO %d failed\n", cam_enable);
			goto cam_enable_failed;
		}
	} else {
		dev_err(dev, "invalid pwdn gpio %d\n", cam_enable);
		goto cam_enable_failed;
	}

	cam_reset = of_get_named_gpio(dev->of_node, "reset-gpios", 0);
	if (gpio_is_valid(cam_reset)) {
		if (gpio_request(cam_reset, "CAM2_RESET")) {
			dev_err(dev, "Request GPIO %d failed\n", cam_reset);
			goto cam_reset_failed;
		}
	} else {
		dev_err(dev, "invalid pwdn gpio %d\n", cam_reset);
		goto cam_reset_failed;
	}

	if (on) {
		if (avdd_2v8) {
			regulator_set_voltage(avdd_2v8, 2800000, 2800000);
			ret = regulator_enable(avdd_2v8);
			if (ret)
				goto cam_regulator_enable_failed;
		}

		if (af_2v8) {
			regulator_set_voltage(af_2v8, 2800000, 2800000);
			ret = regulator_enable(af_2v8);
			if (ret)
				goto cam_regulator_enable_failed;
		}

		if (dvdd_1v2) {
			regulator_set_voltage(dvdd_1v2, 1200000, 1200000);
			ret = regulator_enable(dvdd_1v2);
			if (ret)
				goto cam_regulator_enable_failed;
		}

		if (dovdd_1v8) {
			regulator_set_voltage(dovdd_1v8, 1800000, 1800000);
			ret = regulator_enable(dovdd_1v8);
			if (ret)
				goto cam_regulator_enable_failed;
		}

		gpio_direction_output(cam_reset, 0);
		usleep_range(5000, 20000);
		gpio_direction_output(cam_enable, 1);
		usleep_range(5000, 20000);
		gpio_direction_output(cam_reset, 1);
		usleep_range(5000, 20000);
	} else {
		/*
		 * Keep PWDN always on as defined in spec
		 * gpio_direction_output(cam_enable, 0);
		 * usleep_range(5000, 20000);
		 */
		gpio_direction_output(cam_reset, 0);

		if (dovdd_1v8)
			regulator_disable(dovdd_1v8);
		if (dvdd_1v2)
			regulator_disable(dvdd_1v2);
		if (avdd_2v8)
			regulator_disable(avdd_2v8);
		if (af_2v8)
			regulator_disable(af_2v8);
	}

	gpio_free(cam_enable);
	gpio_free(cam_reset);

	return 0;

cam_reset_failed:
	gpio_free(cam_enable);
cam_enable_failed:
	ret = -EIO;
cam_regulator_enable_failed:
	if (dvdd_1v2)
		regulator_put(dvdd_1v2);
	dvdd_1v2 = NULL;
	if (af_2v8)
		regulator_put(af_2v8);
	af_2v8 = NULL;
	if (dovdd_1v8)
		regulator_put(dovdd_1v8);
	dovdd_1v8 = NULL;
	if (avdd_2v8)
		regulator_put(avdd_2v8);
	avdd_2v8 = NULL;

	return ret;
}

static struct sensor_board_data s5k8aa_data = {
	.mount_pos	= SENSOR_USED | SENSOR_POS_FRONT | SENSOR_RES_LOW,
	.bus_type	= V4L2_MBUS_CSI2,
	.bus_flag	= V4L2_MBUS_CSI2_1_LANE,
	.dphy = {0xFF1D00, 0x00024733, 0x04001001}, /* obsoleted, using dt */
	.flags = V4L2_MBUS_CSI2_1_LANE,
};

static struct i2c_board_info concord_i2c_camera = {
	I2C_BOARD_INFO("s5k8aay", 0x3c),
};

static struct soc_camera_desc s5k8aa_desc = {
	.subdev_desc = {
		.power			= s5k8aa_sensor_power,
		.drv_priv		= &s5k8aa_data,
		.flags		= 0,
	},
	.host_desc = {
		.bus_id = 1,	/* Must match with the camera ID */
		.i2c_adapter_id = 2,	/* twsi 3 */
		.board_info		= &concord_i2c_camera,
		.module_name	= "s5k8aay",
	},
};
#endif

static const struct of_dev_auxdata mmpx_auxdata_lookup[] __initconst = {
	OF_DEV_AUXDATA("mrvl,mmp-sspa-dai", 0xc0ffdc00, "mmp-sspa-dai.0", NULL),
	OF_DEV_AUXDATA("mrvl,mmp-sspa-dai", 0xc0ffdd00, "mmp-sspa-dai.1", NULL),
#ifdef CONFIG_VIDEO_MV_SC2_MMU
	OF_DEV_AUXDATA("marvell,mmp-sc2mmu", 0xd4209000, "mv_sc2_mmu.0", NULL),
#endif
#ifdef CONFIG_SOC_CAMERA_S5K8AA
	OF_DEV_AUXDATA("soc-camera-pdrv", 0, "soc-camera-pdrv.0", &s5k8aa_desc),
#endif
#ifdef CONFIG_VIDEO_MV_SC2_CCIC
	OF_DEV_AUXDATA("marvell,mmp-sc2ccic", 0xd420a000,
				"mv_sc2_ccic.0", NULL),
	OF_DEV_AUXDATA("marvell,mmp-sc2ccic", 0xd420a800,
				"mv_sc2_ccic.1", NULL),
#endif
#ifdef CONFIG_VIDEO_MV_SC2_CAMERA
	OF_DEV_AUXDATA("marvell,mv_sc2_camera", 1, "mv_sc2_camera.1", NULL),
#endif
#ifdef CONFIG_SD8XXX_RFKILL
	OF_DEV_AUXDATA("mrvl,sd8x-rfkill", 0, "sd8x-rfkill", NULL),
#endif
	{}
};

#define MPMU_PHYS_BASE		0xd4050000
#define GEN_TMR2_PHYS_BASE	0xd4080000
#define MPMU_PRR_SP		0x0020
#define MPMU_WDTPCR		0x0200
#define MPMU_WDTPCR1		0x0204
#define MPMU_PRR_PJ		0x1020

#define MPMU_PRR_SP_WDTR	(1 << 4)
#define MPMU_PRR_SP_CPR		(1 << 0)

#define TMR_WFAR               (0x009c)
#define TMR_WSAR               (0x00A0)

#define GEN_TMR_CFG            (0x00B0)
#define GEN_TMR_LD1            (0x00B8)

/* Get SoC Access to Generic Timer */
static void arch_timer_soc_access_enable(void __iomem *gen_tmr_base)
{
	__raw_writel(0xbaba, gen_tmr_base + TMR_WFAR);
	__raw_writel(0xeb10, gen_tmr_base + TMR_WSAR);
}

static void arch_timer_soc_config(void __iomem *mpmu_base)
{
	void __iomem *gen_tmr2_base;
	u32 tmp;

	gen_tmr2_base = ioremap(GEN_TMR2_PHYS_BASE, SZ_4K);
	if (!gen_tmr2_base) {
		pr_err("ioremap gen_tmr_base failed\n");
		return;
	}

	/* Enable WDTR2*/
	tmp  = __raw_readl(mpmu_base + MPMU_PRR_SP);
	tmp = tmp | MPMU_PRR_SP_WDTR;
	__raw_writel(tmp, mpmu_base + MPMU_PRR_SP);

	/* Initialize Counter to zero */
	arch_timer_soc_access_enable(gen_tmr2_base);
	__raw_writel(0x0, gen_tmr2_base + GEN_TMR_LD1);

	/* Program Generic Timer Clk Frequency */
	arch_timer_soc_access_enable(gen_tmr2_base);
	tmp = __raw_readl(gen_tmr2_base + GEN_TMR_CFG);
	tmp |= (3 << 4); /* 3.25MHz/32KHz Counter auto switch enabled */
	arch_timer_soc_access_enable(gen_tmr2_base);
	__raw_writel(tmp, gen_tmr2_base + GEN_TMR_CFG);

	/* Start the Generic Timer Counter */
	arch_timer_soc_access_enable(gen_tmr2_base);
	tmp = __raw_readl(gen_tmr2_base + GEN_TMR_CFG);
	tmp |= 0x3;
	arch_timer_soc_access_enable(gen_tmr2_base);
	__raw_writel(tmp, gen_tmr2_base + GEN_TMR_CFG);

	iounmap(gen_tmr2_base);
}

static __init void pxa1928_timer_init(void)
{
	void __iomem *mpmu_base;
	void __iomem *chip_id;

	regs_addr_iomap();

	mpmu_base = regs_addr_get_va(REGS_ADDR_MPMU);
	if (!mpmu_base) {
		pr_err("ioremap mpmu_base failed");
		return;
	}

	/* this is early, initialize mmp_chip_id here */
	chip_id = regs_addr_get_va(REGS_ADDR_CIU);
	mmp_chip_id = readl_relaxed(chip_id);

#ifdef CONFIG_ARM_ARCH_TIMER
	arch_timer_soc_config(mpmu_base);
#endif
	/*
	 * bit 7.enable wdt reset from thermal
	 * bit 6.enable wdt reset from timer2
	 */
	__raw_writel(0xD3, mpmu_base + MPMU_WDTPCR);

	clocksource_of_init();
}

static void __init pxa1928_init_machine(void)
{
	of_platform_populate(NULL, of_default_bus_match_table,
			     mmpx_auxdata_lookup, &platform_bus);
}

static const char * const pxa1928_dt_board_compat[] __initconst = {
	"marvell,pxa1928",
	NULL,
};

DT_MACHINE_START(PXA1928_DT, "PXA1928")
	.init_time      = pxa1928_timer_init,
	.init_machine	= pxa1928_init_machine,
	.dt_compat      = pxa1928_dt_board_compat,
MACHINE_END
