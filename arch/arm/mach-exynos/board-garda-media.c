/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/clock.h>
#include <plat/gpio-cfg.h>
#include <plat/fimc-core.h>
#include <plat/mipi_csis.h>
#include <plat/fimg2d.h>
#include <plat/jpeg.h>
#include <plat/s3c64xx-spi.h>
#include <mach/spi-clocks.h>
#include <mach/exynos-mfc.h>
#include <media/s5p_fimc.h>
#include <media/v4l2-mediabus.h>
#include <media/m5mols.h>
#include <mach/exynos-jpeg.h>

#include "board-universal222ap.h"

#if defined(CONFIG_TDMB) || defined(CONFIG_TDMB_MODULE)
#include <mach/tdmb_pdata.h>
#endif

#ifdef CONFIG_VIDEO_M5MOLS
static struct m5mols_platform_data m5mols_platdata = {
#ifdef CONFIG_CSI_C
	.gpio_rst = EXYNOS4_GPX1(2), /* ISP_RESET */
#endif
#ifdef CONFIG_CSI_D
	.gpio_rst = EXYNOS4_GPX1(0), /* ISP_RESET */
#endif
	.enable_rst = true, /* positive reset */
	.irq = IRQ_EINT(27),
};

static struct i2c_board_info m5mols_board_info = {
	I2C_BOARD_INFO("M5MOLS", 0x1F),
	.platform_data = &m5mols_platdata,
};
#endif

#ifndef CONFIG_VIDEO_EXYNOS_FIMC_LITE
static struct regulator_consumer_supply mipi_csi_fixed_voltage_supplies[] = {
	REGULATOR_SUPPLY("vdd11", "s5p-mipi-csis.0"),
	REGULATOR_SUPPLY("vdd11", "s5p-mipi-csis.1"),
};

static struct regulator_init_data mipi_csi_fixed_voltage_init_data = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(mipi_csi_fixed_voltage_supplies),
	.consumer_supplies	= mipi_csi_fixed_voltage_supplies,
};

static struct fixed_voltage_config mipi_csi_fixed_voltage_config = {
	.supply_name	= "DC_5V",
	.microvolts	= 5000000,
	.gpio		= -EINVAL,
	.init_data	= &mipi_csi_fixed_voltage_init_data,
};

static struct platform_device mipi_csi_fixed_voltage = {
	.name		= "reg-fixed-voltage",
	.id		= 3,
	.dev		= {
		.platform_data	= &mipi_csi_fixed_voltage_config,
	},
};
#endif

#ifdef CONFIG_VIDEO_M5MOLS
static struct regulator_consumer_supply m5mols_fixed_voltage_supplies[] = {
	REGULATOR_SUPPLY("core", NULL),
	REGULATOR_SUPPLY("dig_18", NULL),
	REGULATOR_SUPPLY("d_sensor", NULL),
	REGULATOR_SUPPLY("dig_28", NULL),
	REGULATOR_SUPPLY("a_sensor", NULL),
	REGULATOR_SUPPLY("dig_12", NULL),
};

static struct regulator_init_data m5mols_fixed_voltage_init_data = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(m5mols_fixed_voltage_supplies),
	.consumer_supplies	= m5mols_fixed_voltage_supplies,
};

static struct fixed_voltage_config m5mols_fixed_voltage_config = {
	.supply_name	= "CAM_SENSOR",
	.microvolts	= 1800000,
	.gpio		= -EINVAL,
	.init_data	= &m5mols_fixed_voltage_init_data,
};

static struct platform_device m5mols_fixed_voltage = {
	.name		= "reg-fixed-voltage",
	.id		= 4,
	.dev		= {
		.platform_data	= &m5mols_fixed_voltage_config,
	},
};
#endif

struct platform_device exynos4_fimc_md = {
	.name = "s5p-fimc-md",
	.id = 0,
};

static struct platform_device *smdk4270_media_devices[] __initdata = {
#ifdef CONFIG_VIDEO_EXYNOS_MFC
	&s5p_device_mfc,
#endif
	&exynos4_fimc_md,
	&s5p_device_fimc0,
	&s5p_device_fimc1,
	&s5p_device_fimc2,
	&s5p_device_fimc3,
#ifndef CONFIG_VIDEO_EXYNOS_FIMC_LITE
	&s5p_device_mipi_csis0,
	&s5p_device_mipi_csis1,
	&mipi_csi_fixed_voltage,
#if defined(CONFIG_VIDEO_M5MOLS)
	&m5mols_fixed_voltage,
#endif
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMG2D
	&s5p_device_fimg2d,
#endif
	&s5p_device_jpeg,
#if defined(CONFIG_TDMB) || defined(CONFIG_TDMB_MODULE)
	&s3c64xx_device_spi1,
#endif

};

static struct s5p_fimc_isp_info isp_info[] = {
#if defined(CONFIG_VIDEO_M5MOLS)
	{
		.board_info	= &m5mols_board_info,
		.is_sensor_info	= NULL,
		.clk_frequency	= 24000000UL,
		.bus_type	= FIMC_MIPI_CSI2,
#ifdef CONFIG_CSI_C
		.i2c_bus_num	= 4,
		.mux_id		= 0, /* A-Port : 0, B-Port : 1 */
		.clk_id		= 0,
#endif
#ifdef CONFIG_CSI_D
		.i2c_bus_num	= 5,
		.mux_id		= 1, /* A-Port : 0, B-Port : 1 */
		.clk_id		= 1,
#endif
		.flags		= 0,
		.csi_data_align	= 32,
		.use_isp	= false,
	},
#endif
};

static void __init smdk4270_subdev_config(void)
{
	s5p_fimc_md_platdata.num_clients = ARRAY_SIZE(isp_info);
	if (s5p_fimc_md_platdata.num_clients)
		s5p_fimc_md_platdata.isp_info = &isp_info[0];
}

static void __init smdk4270_camera_config(void)
{
	/* MIPI CAM MCLK */
	s3c_gpio_cfgrange_nopull(EXYNOS4_GPM2(2), 1, S3C_GPIO_SFN(3));

#if defined(CONFIG_VIDEO_EXYNOS_FIMC_LITE)
	/* MIPI CAM EN */
	s3c_gpio_cfgpin(EXYNOS4_GPM4(4), S3C_GPIO_SFN(1));
#endif
}

#ifdef CONFIG_VIDEO_EXYNOS_MFC
#ifdef CONFIG_ARM_EXYNOS3470_BUS_DEVFREQ
/*
 * thrd_mb means threadhold of macroblock(MB) count.
 * If current MB counts are higher than table's thrd_mb,
 * table is selected for INT/MIF/CPU locking.
 * Ex> Current MBs = 196000, table[3] is selected.
 *
 * thrd_mb is obtained by the ratio of INT clock for MFC.
 * Maximum MB count is based on 1080p@30fps on 200MHz
 *
 * +------------+---------------+---------------+---------------+
 * | INT Level	| INT clock(Hz)	|  MB count	|  Ratio	|
 * +------------+---------------+---------------+---------------+
 * |   LV0	|    200000	|    244800	|    1.0	|
 * |   LV1	|    160000	|    195840	|    0.8	|
 * |   LV2	|    133000	|    162792	|    0.665	|
 * |   LV3	|    100000	|    122400	|    0.5	|
 * +------------+---------------+---------------+---------------+
 */
static struct s5p_mfc_qos smdk4270_mfc_qos_table[] = {
	[0] = {
		.thrd_mb	= 0,
		.freq_int	= 100000,
		.freq_mif	= 200000,
		.freq_cpu	= 0,
	},
	[1] = {
		.thrd_mb	= 122400,
		.freq_int	= 133000,
		.freq_mif	= 200000,
		.freq_cpu	= 0,
	},
	[2] = {
		.thrd_mb	= 162792,
		.freq_int	= 160000,
		.freq_mif	= 400000,
		.freq_cpu	= 0,
	},
	[3] = {
		.thrd_mb	= 195840,
		.freq_int	= 200000,
		.freq_mif	= 400000,
		.freq_cpu	= 0,
	},
};
#endif

static struct s5p_mfc_platdata smdk4270_mfc_pd = {
	.ip_ver = IP_VER_MFC_4P_1,
	.clock_rate = 200 * MHZ,
#ifdef CONFIG_ARM_EXYNOS3470_BUS_DEVFREQ
	.num_qos_steps	= ARRAY_SIZE(smdk4270_mfc_qos_table),
	.qos_table	= smdk4270_mfc_qos_table,
#endif
};
#endif

#ifdef CONFIG_VIDEO_EXYNOS_FIMG2D
static struct fimg2d_platdata fimg2d_data __initdata = {
	.ip_ver = IP_VER_G2D_4C,
	.hw_ver = 0x43,
	.parent_clkname	= "mout_g2d_acp",
	.clkname	= "sclk_fimg2d",
	.gate_clkname = "fimg2d",
	.clkrate	= 200000000,
};
#endif

#if defined(CONFIG_TDMB) || defined(CONFIG_TDMB_MODULE)
static struct s3c64xx_spi_csinfo spi1_csi[] = {
	[0] = {
		.line = EXYNOS4_GPB(5),
		.set_level = gpio_set_value,
		.fb_delay = 0x0,
	},
};
static struct spi_board_info spi1_board_info[] __initdata = {
	{
		.modalias = "tdmbspi",
		.platform_data = NULL,
		.max_speed_hz = 5000000,
		.bus_num = 1,
		.chip_select = 0,
		.mode = SPI_MODE_0,
		.controller_data = &spi1_csi[0],
	}
};

static void tdmb_set_config_poweron(void)
{
	s3c_gpio_cfgpin(GPIO_TDMB_EN, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_TDMB_EN, S3C_GPIO_PULL_NONE);
	gpio_set_value(GPIO_TDMB_EN, GPIO_LEVEL_LOW);

	s3c_gpio_cfgpin(GPIO_TDMB_INT, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(GPIO_TDMB_INT, S3C_GPIO_PULL_NONE);
}
static void tdmb_set_config_poweroff(void)
{
	s3c_gpio_cfgpin(GPIO_TDMB_EN, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_TDMB_EN, S3C_GPIO_PULL_DOWN);
	gpio_set_value(GPIO_TDMB_EN, GPIO_LEVEL_LOW);

	s3c_gpio_cfgpin(GPIO_TDMB_INT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_TDMB_INT, S3C_GPIO_PULL_DOWN);
	gpio_set_value(GPIO_TDMB_INT, GPIO_LEVEL_LOW);
}

static void tdmb_gpio_on(void)
{
	printk(KERN_DEBUG "tdmb_gpio_on\n");

	tdmb_set_config_poweron();


	gpio_set_value(GPIO_TDMB_EN, GPIO_LEVEL_LOW);
	usleep_range(1000, 1000);
	gpio_set_value(GPIO_TDMB_EN, GPIO_LEVEL_HIGH);
	usleep_range(20000, 20000);
}

static void tdmb_gpio_off(void)
{
	printk(KERN_DEBUG "tdmb_gpio_off\n");

	tdmb_set_config_poweroff();
}

static struct tdmb_platform_data tdmb_pdata = {
	.gpio_on = tdmb_gpio_on,
	.gpio_off = tdmb_gpio_off,
};

static struct platform_device tdmb_device = {
	.name			= "tdmb",
	.id				= -1,
	.dev			= {
		.platform_data = &tdmb_pdata,
	},
};

static int __init tdmb_dev_init(void)
{
	tdmb_set_config_poweroff();

	s5p_register_gpio_interrupt(GPIO_TDMB_INT);
	tdmb_pdata.irq = gpio_to_irq(GPIO_TDMB_INT);
	platform_device_register(&tdmb_device);

	return 0;
}
#endif

void __init exynos4_smdk4270_media_init(void)
{
	platform_add_devices(smdk4270_media_devices,
			ARRAY_SIZE(smdk4270_media_devices));

#ifdef CONFIG_VIDEO_EXYNOS_MFC
	s5p_mfc_set_platdata(&smdk4270_mfc_pd);

	dev_set_name(&s5p_device_mfc.dev, "s5p-mfc");
	clk_add_alias("mfc", "s5p-mfc-v5", "mfc", &s5p_device_mfc.dev);
	clk_add_alias("sclk_mfc", "s5p-mfc-v5", "sclk_mfc",
						&s5p_device_mfc.dev);
	s5p_mfc_setname(&s5p_device_mfc, "s5p-mfc-v5");
#endif
	smdk4270_camera_config();
	smdk4270_subdev_config();

	dev_set_name(&s5p_device_fimc0.dev, "exynos4-fimc.0");
	dev_set_name(&s5p_device_fimc1.dev, "exynos4-fimc.1");
	dev_set_name(&s5p_device_fimc2.dev, "exynos4-fimc.2");
	dev_set_name(&s5p_device_fimc3.dev, "exynos4-fimc.3");

	s3c_set_platdata(&s5p_fimc_md_platdata,
			 sizeof(s5p_fimc_md_platdata), &exynos4_fimc_md);
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_fimc0.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_fimc1.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_fimc2.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_fimc3.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#endif
#if !defined(CONFIG_VIDEO_EXYNOS_FIMC_LITE)
	dev_set_name(&s5p_device_mipi_csis0.dev, "s5p-mipi-csis.0");
	dev_set_name(&s5p_device_mipi_csis1.dev, "s5p-mipi-csis.1");

	s5p_mipi_csis0_default_data.clk_rate = 266000000;
	s5p_mipi_csis1_default_data.clk_rate = 266000000;
	s3c_set_platdata(&s5p_mipi_csis0_default_data,
			sizeof(s5p_mipi_csis0_default_data),
			&s5p_device_mipi_csis0);
	s3c_set_platdata(&s5p_mipi_csis1_default_data,
			sizeof(s5p_mipi_csis1_default_data),
			&s5p_device_mipi_csis1);

#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_mipi_csis0.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_mipi_csis1.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#endif
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMG2D
	s5p_fimg2d_set_platdata(&fimg2d_data);
#endif

	exynos4_jpeg_setup_clock(&s5p_device_jpeg.dev, 160000000);

#if defined(CONFIG_TDMB) || defined(CONFIG_TDMB_MODULE)
	if (!exynos_spi_cfg_cs(spi1_csi[0].line, 1)) {
		s3c64xx_spi1_set_platdata(&s3c64xx_spi1_pdata,
			EXYNOS_SPI_SRCCLK_SCLK, ARRAY_SIZE(spi1_csi));
		spi_register_board_info(spi1_board_info,
			ARRAY_SIZE(spi1_board_info));
	} else {
		pr_err("%s: Error requesting gpio for TDMB_SPI CS\n", __func__);
	}

	tdmb_dev_init();
#endif
}
