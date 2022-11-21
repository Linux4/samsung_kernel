/*
 *  linux/arch/arm/mach-mmp/yellowstone.c
 *
 *  Support for the Marvell MMP3 YellowStone Development Platform.
 *
 *  Copyright (C) 2009-2011 Marvell International Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publishhed by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mfd/88pm80x.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/fixed.h>
#include <linux/regmap.h>
#include <linux/pwm_backlight.h>
#include <linux/mmc/sdhci.h>
#include <linux/platform_data/mmp_audio.h>
#ifdef CONFIG_SD8XXX_RFKILL
#include <linux/sd8x_rfkill.h>
#endif
#include <linux/regulator/fan53555.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>
#include <asm/hardware/gic.h>
#include <mach/addr-map.h>
#include <mach/mfp-mmp2.h>
#include <mach/mmp3.h>
#include <mach/irqs.h>
#include <mach/uio_hdmi.h>
#include <mach/mmp_device.h>
#include <media/soc_camera.h>
#include <mach/isp_dev.h>
#include <mach/camera.h>
#include <linux/platform_data/mv_usb.h>

#include "common.h"
#include "onboard.h"

static unsigned long yellowstone_pin_config[] __initdata = {
	/* UART3 */
	GPIO51_UART3_RXD,
	GPIO52_UART3_TXD,

	/* TWSI5 */
	GPIO99_TWSI5_SCL,
	GPIO100_TWSI5_SDA,

	/* TWSI6 */
	GPIO97_TWSI6_SCL,
	GPIO98_TWSI6_SDA,

	/* TWSI2 */
	GPIO43_TWSI2_SCL,
	GPIO44_TWSI2_SDA,

	/* TWSI3 */
	GPIO71_TWSI3_SCL,
	GPIO72_TWSI3_SDA,

	/* TWSI4 */
	TWSI4_SCL,
	TWSI4_SDA,

	/*PWM3*/
	GPIO53_PWM3,
	GPIO84_GPIO,

	/* SSPA1 (I2S) */
	GPIO23_GPIO,
	GPIO24_I2S_SYSCLK,
	GPIO25_I2S_BITCLK,
	GPIO26_I2S_SYNC,
	GPIO27_I2S_DATA_OUT,
	GPIO28_I2S_SDATA_IN,

	/* Camera */
	GPIO64_GPIO,
	GPIO68_GPIO,
	GPIO0_GPIO,
	GPIO1_GPIO,
	GPIO73_CAM_MCLK,

	/* DFI */
	GPIO168_DFI_D0,
	GPIO167_DFI_D1,
	GPIO166_DFI_D2,
	GPIO165_DFI_D3,
	GPIO107_DFI_D4,
	GPIO106_DFI_D5,
	GPIO105_DFI_D6,
	GPIO104_DFI_D7,
	GPIO111_DFI_D8,
	GPIO164_DFI_D9,
	GPIO163_DFI_D10,
	GPIO162_DFI_D11,
	GPIO161_DFI_D12,
	GPIO110_DFI_D13,
	GPIO109_DFI_D14,
	GPIO108_DFI_D15,
	GPIO143_ND_nCS0,
	GPIO144_ND_nCS1,
	GPIO147_ND_nWE,
	GPIO148_ND_nRE,
	GPIO150_ND_ALE,
	GPIO149_ND_CLE,
	GPIO112_ND_RDY0,
	GPIO160_ND_RDY1,

	/* Keypad */
	GPIO16_KP_DKIN0 | MFP_PULL_HIGH,
	GPIO17_KP_DKIN1 | MFP_PULL_HIGH,
	GPIO18_KP_DKIN2 | MFP_PULL_HIGH,
	GPIO19_KP_DKIN3 | MFP_PULL_HIGH,
	GPIO20_KP_DKIN4 | MFP_PULL_HIGH,
	GPIO22_KP_DKIN6 | MFP_PULL_HIGH,

	PMIC_PMIC_INT | MFP_LPM_EDGE_FALL,

	/* HDMI */
	GPIO59_HDMI_DET,
	GPIO54_HDMI_CEC,

	/* OTG vbus enable signal */
	MFP_CFG(GPIO82, AF0),

	/* HSIC1 reset pin*/
	MFP_CFG_LPM(GPIO96, AF0, DRIVE_LOW),

	GPIO101_GPIO, /* TS INT*/
	GPIO85_GPIO, /* TS_IO_EN */
};

static unsigned long mmc1_pin_config[] __initdata = {
	GPIO131_MMC1_DAT3,
	GPIO132_MMC1_DAT2,
	GPIO133_MMC1_DAT1,
	GPIO134_MMC1_DAT0,
	GPIO136_MMC1_CMD,
	GPIO135_MMC1_CLK,
	GPIO140_MMC1_CD | MFP_PULL_HIGH,
	GPIO141_MMC1_WP | MFP_PULL_HIGH,
};

/* MMC2 is used for WIB card */
static unsigned long mmc2_pin_config[] __initdata = {
	GPIO37_MMC2_DAT3,
	GPIO38_MMC2_DAT2,
	GPIO39_MMC2_DAT1,
	GPIO40_MMC2_DAT0,
	GPIO41_MMC2_CMD,
	GPIO42_MMC2_CLK,

	/* GPIO used for power */
	GPIO58_GPIO | MFP_LPM_DRIVE_HIGH, /* WIFI_RST_N */
	GPIO57_GPIO | MFP_LPM_DRIVE_LOW, /* WIFI_PD_N */
};

static unsigned long mmc3_pin_config[] __initdata = {
	GPIO108_MMC3_DAT7,
	GPIO109_MMC3_DAT6,
	GPIO161_MMC3_DAT5,
	GPIO163_MMC3_DAT4,
	GPIO111_MMC3_DAT3,
	GPIO110_MMC3_DAT2,
	GPIO162_MMC3_DAT1,
	GPIO164_MMC3_DAT0,
	GPIO145_MMC3_CMD,
	GPIO146_MMC3_CLK,
};

#ifdef CONFIG_VIDEO_MVISP_OV882X
static int ov882x_sensor_power_on(int on)
{
	struct regulator *af_vcc;
	struct regulator *avdd;
	int rst = mfp_to_gpio(MFP_PIN_GPIO0);
	int pwdn = mfp_to_gpio(MFP_PIN_GPIO64);

	if (gpio_request(pwdn, "CAM_ENABLE_LOW"))
		return -EIO;

	if (gpio_request(rst, "CAM_RESET_HI"))
		return -EIO;

	af_vcc = regulator_get(NULL, "V_2P8");
	if (IS_ERR(af_vcc)) {
		af_vcc = NULL;
		return -EIO;
	}

	avdd = regulator_get(NULL, "AVDD_CAM_2P8V");
	if (IS_ERR(avdd)) {
		avdd = NULL;
		return -EIO;
	}

	/*   Enable voltage for camera sensor OV882x */
	if (on) {
		regulator_set_voltage(af_vcc, 2800000, 2800000);
		regulator_enable(af_vcc);
		regulator_set_voltage(avdd, 2800000, 2800000);
		regulator_enable(avdd);
		mdelay(10);
		gpio_direction_output(pwdn, 1);
		mdelay(5);
	} else {
		regulator_disable(af_vcc);
		regulator_disable(avdd);
		gpio_direction_output(pwdn, 0);
	}

	/*   pwdn is low active, reset the sensor now*/
	gpio_direction_output(rst, 0);
	mdelay(20);
	/*   pwdn is low active, enable the sensor now*/
	gpio_direction_output(rst, 1);
	gpio_free(rst);
	gpio_free(pwdn);

	regulator_put(af_vcc);
	regulator_put(avdd);

	return 0;
}

static struct sensor_platform_data ov882x_platdata = {
	.power_on = ov882x_sensor_power_on,
};

static struct i2c_board_info ov882x_info = {
	.type = "ov8820",
	.addr = 0x36,
	.platform_data = &ov882x_platdata,
};

static struct mvisp_subdev_i2c_board_info ov882x_isp_info[] = {
	[0] = {
		.board_info = &ov882x_info,
		.i2c_adapter_id = 2,
	},
	[1] = {
		.board_info = NULL,
		.i2c_adapter_id = 0,
	},
};

static struct mvisp_v4l2_subdevs_group dxoisp_subdevs_group[] = {
	[0] = {
		.i2c_board_info = ov882x_isp_info,
		.if_type = ISP_INTERFACE_CCIC_1,
	},
	[1] = {
		.i2c_board_info = NULL,
		.if_type = 0,
	},
};
#endif

#ifdef CONFIG_VIDEO_MVISP
#ifndef CONFIG_VIDEO_MVISP_OV882X
static struct mvisp_v4l2_subdevs_group dxoisp_subdevs_group[] = {
	[0] = {
		.i2c_board_info = NULL,
		.if_type = 0,
	},
};
#endif

static char *mmp3_isp_ccic_clk_name[] = {
	[0] = "ISP-CLK",
	[1] = "CCIC-CLK",
};

static struct mvisp_platform_data mmp3_dxoisp_pdata = {
	.isp_clknum       = 1,
	.ccic_clknum      = 1,
	.clkname          = mmp3_isp_ccic_clk_name,
	.mvisp_reset      = mmp3_isp_reset_hw,
	.isp_pwr_ctrl     = isppwr_power_control,
	.subdev_group     = dxoisp_subdevs_group,
	.ccic_dummy_ena   = false,
	.ispdma_dummy_ena = false,
};

static void __init mmp3_init_dxoisp(void)
{
	mmp3_register_dxoisp(&mmp3_dxoisp_pdata);
}
#endif

#if defined(CONFIG_VIDEO_MMP)
/* soc  camera */
static int camera_sensor_power(struct device *dev, int on)
{
	int cam_pwdn = mfp_to_gpio(MFP_PIN_GPIO68);

	if (gpio_request(cam_pwdn, "CAM_PWDN")) {
		printk(KERN_ERR"Request GPIO failed, gpio: %d\n", cam_pwdn);
		return -EIO;
	}

	/*
	 * pull up camera pwdn pin to disable camera sensor
	 * pull down camera pwdn pin to enable camera sensor
	 */
	if (on)
		gpio_direction_output(cam_pwdn, 0);
	else
		gpio_direction_output(cam_pwdn, 1);

	msleep(100);

	gpio_free(cam_pwdn);
	return 0;
}

static struct i2c_board_info yellowstone_i2c_camera[] = {
	{
		I2C_BOARD_INFO("ov5642", 0x3c),
	},
};

static struct soc_camera_link iclink_ov5642 = {
	.bus_id         = 1,	/* Must match with the camera ID */
	.power          = camera_sensor_power,
	.board_info     = &yellowstone_i2c_camera[0],
	.i2c_adapter_id = 2,
	.flags		= V4L2_MBUS_CSI2_LANES,
	.module_name    = "ov5642",
	.priv		= "pxa2128-mipi",
};

static struct platform_device yellowstone_ov5642 = {
	.name   = "soc-camera-pdrv",
	.id     = 0,
	.dev    = {
		.platform_data = &iclink_ov5642,
	},
};

static int pxa2128_cam_clk_init(struct device *dev, int init)
{
	static struct regulator *af_vcc;
	static struct regulator *avdd;
	struct mmp_cam_pdata *data = dev->platform_data;
	int cam_enable = mfp_to_gpio(MFP_PIN_GPIO1);
	unsigned long tx_clk_esc;
	struct clk *pll1;

	pll1 = clk_get(dev, "pll1");
	if (IS_ERR(pll1)) {
		dev_err(dev, "Could not get pll1 clock\n");
		return PTR_ERR(pll1);
	}

	tx_clk_esc = clk_get_rate(pll1) / 1000000 / 12;
	clk_put(pll1);

	/*
	 * Update dphy6 according to current tx_clk_esc
	 */
	data->dphy[2] = ((534 * tx_clk_esc / 2000 - 1) & 0xff) << 8
			| ((38 * tx_clk_esc / 1000 - 1) & 0xff);

	if (gpio_request(cam_enable, "CAM_ENABLE_HI_SENSOR")) {
		printk(KERN_ERR"Request GPIO failed, gpio: %d\n", cam_enable);
		return -EIO;
	}

	af_vcc = regulator_get(NULL, "V_2P8");
	if (IS_ERR(af_vcc)) {
		af_vcc = NULL;
		return -EIO;
	}

	avdd = regulator_get(NULL, "AVDD_CAM_2P8V");
	if (IS_ERR(avdd)) {
		avdd = NULL;
		return -EIO;
	}
	if ((!data->clk_enabled) && init) {
		data->clk[0] = clk_get(dev, "CCICRSTCLK");
		if (IS_ERR(data->clk[0])) {
			dev_err(dev, "Could not get rstclk\n");
			return PTR_ERR(data->clk[0]);
		}
		data->clk_enabled = 1;
		regulator_set_voltage(af_vcc, 3000000, 3000000);
		regulator_enable(af_vcc);
		regulator_set_voltage(avdd, 2800000, 2800000);
		regulator_enable(avdd);

		gpio_direction_output(cam_enable, 1);
		gpio_free(cam_enable);
		return 0;
	}

	if (!init && data->clk_enabled) {
		clk_put(data->clk[0]);
		regulator_disable(af_vcc);
		regulator_put(af_vcc);
		regulator_disable(avdd);
		regulator_put(avdd);
		gpio_direction_output(cam_enable, 0);
		gpio_free(cam_enable);
		return 0;
	}
	return -EFAULT;
}

static void pxa2128_cam_set_clk(struct device *dev, int on)
{
	struct mmp_cam_pdata *data = dev->platform_data;

	isppwr_power_control(on);

	if (on)
		clk_enable(data->clk[0]);
	else
		clk_disable(data->clk[0]);
}

static struct mmp_cam_pdata mmp_cam_data = {
	.name = "yellowstone",
	.clk_enabled = 0,
	.dphy = {0x1b0b, 0x33, 0x1a03},
	.dphy3_algo = 2,
	.bus_type = V4L2_MBUS_CSI2_LANES,
	.dma_burst = 128,
	.mclk_src = 3,
	.mclk_min = 26,
	.mclk_div = 8,
	.init_clk = pxa2128_cam_clk_init,
	.enable_clk = pxa2128_cam_set_clk,
};
#endif

static struct pxa27x_keypad_platform_data mmp3_keypad_info = {
	.direct_key_map = {
		KEY_BACK,
		KEY_MENU,
		KEY_HOME,
		KEY_SEARCH,
		KEY_VOLUMEUP,
		KEY_RESERVED,
		KEY_VOLUMEDOWN,
	},
	.direct_key_num = 7,
	.debounce_interval = 30,
	.direct_key_low_active = 1,
};

#ifdef CONFIG_SD8XXX_RFKILL
/*
 * during wifi enabled, power should always on and 8787 should always
 * released from reset. 8787 handle power management by itself.
 */
static unsigned long wifi_pin_config_on[] = {
	GPIO57_GPIO | MFP_LPM_DRIVE_HIGH,
};
static unsigned long wifi_pin_config_off[] = {
	GPIO57_GPIO | MFP_LPM_DRIVE_LOW,
};
static void mmp3_8787_set_power(unsigned int on)
{
	static struct regulator *vbat_fem;
	static int f_enabled = 0;
	/* VBAT_FEM 3.3v */
	if (on && (!f_enabled)) {
		vbat_fem = regulator_get(NULL, "VBAT_FEM");
		if (IS_ERR(vbat_fem)) {
			vbat_fem = NULL;
			pr_err("get VBAT_FEM failed %s.\n", __func__);
		} else {
			regulator_set_voltage(vbat_fem, 3300000, 3300000);
			regulator_enable(vbat_fem);
			f_enabled = 1;
			mfp_config(ARRAY_AND_SIZE(wifi_pin_config_on));
		}
	}

	if (f_enabled && (!on)) {
		if (vbat_fem) {
			mfp_config(ARRAY_AND_SIZE(wifi_pin_config_off));
			regulator_disable(vbat_fem);
			regulator_put(vbat_fem);
			vbat_fem = NULL;
		}
		f_enabled = 0;
	}
}
#endif

#ifdef CONFIG_MMC_SDHCI_PXAV3
#include <linux/mmc/host.h>
static void yellowstone_sd_vol_change(unsigned int set)
{
	static struct regulator *v_ldo_sd;

	v_ldo_sd = regulator_get(NULL, "v_ldo11");
	if (IS_ERR(v_ldo_sd)) {
		printk(KERN_ERR "Failed to get v_ldo11\n");
		return;
	}

	regulator_set_voltage(v_ldo_sd, set, set);
	regulator_enable(v_ldo_sd);

	mmp3_io_domain_1v8(AIB_SDMMC_IO_REG, set);

	msleep(10);
	regulator_put(v_ldo_sd);
}

static struct sdhci_pxa_platdata mmp3_sdh_platdata_mmc0 = {
	.clk_delay_cycles	= 0x1F,
	.signal_vol_change	= yellowstone_sd_vol_change,
};

static struct sdhci_pxa_platdata mmp3_sdh_platdata_mmc1 = {
	.flags          = PXA_FLAG_CARD_PERMANENT,
	.pm_caps        = MMC_PM_KEEP_POWER,
};

static struct sdhci_pxa_platdata mmp3_sdh_platdata_mmc2 = {
	.flags		= PXA_FLAG_SD_8_BIT_CAPABLE_SLOT,
	.host_caps	= MMC_CAP_1_8V_DDR,
};

static void __init yellowstone_init_mmc(void)
{
#ifdef CONFIG_SD8XXX_RFKILL
	int WIB_PDn = mfp_to_gpio(GPIO57_GPIO);
	int WIB_RESETn = mfp_to_gpio(GPIO58_GPIO);
	add_sd8x_rfkill_device(WIB_PDn, WIB_RESETn,\
			&mmp3_sdh_platdata_mmc1.pmmc, mmp3_8787_set_power);
#endif

	mfp_config(ARRAY_AND_SIZE(mmc3_pin_config));
	mmp3_add_sdh(2, &mmp3_sdh_platdata_mmc2); /* eMMC */

	mfp_config(ARRAY_AND_SIZE(mmc1_pin_config));
	if (cpu_is_mmp3_b0())
		mmp3_sdh_platdata_mmc0.quirks = SDHCI_QUIRK_INVERTED_WRITE_PROTECT;
	mmp3_add_sdh(0, &mmp3_sdh_platdata_mmc0); /* SD/MMC */

	mfp_config(ARRAY_AND_SIZE(mmc2_pin_config));
	mmp3_add_sdh(1, &mmp3_sdh_platdata_mmc1); /* SDIO for WIFI card */
}
#endif /* CONFIG_MMC_SDHCI_PXAV3 */

#ifdef CONFIG_USB_SUPPORT

#if defined(CONFIG_USB_MV_UDC) || defined(CONFIG_USB_EHCI_MV)

static char *mmp3_usb_clock_name[] = {
	[0] = "U2OCLK",
};

static struct mv_usb_platform_data mmp3_usb_pdata = {
	.clknum		= 1,
	.clkname	= mmp3_usb_clock_name,
	.mode		= MV_USB_MODE_OTG,
	.phy_init	= pxa_usb_phy_init,
	.phy_deinit	= pxa_usb_phy_deinit,
};
#endif

#ifdef CONFIG_USB_EHCI_PXA_U2H_HSIC
static int mmp3_hsic1_reset(void)
{
	int reset;
	reset = mfp_to_gpio(GPIO96_HSIC_RESET);

	if (gpio_request(reset, "hsic reset")) {
		pr_err("Failed to request hsic reset gpio\n");
		return -EIO;
	}

	gpio_direction_output(reset, 0);
	mdelay(100);
	gpio_direction_output(reset, 1);
	mdelay(50);

	gpio_free(reset);
	return 0;
}

static int mmp3_hsic1_set_vbus(unsigned int vbus)
{
	static struct regulator *v_1p2_hsic;

	printk(KERN_INFO "%s: set %d\n", __func__, vbus);
	if (vbus) {
		if (!v_1p2_hsic) {
			v_1p2_hsic = regulator_get(NULL, "V_1P2_HSIC");
			if (IS_ERR(v_1p2_hsic)) {
				printk(KERN_INFO "V_1P2_HSIC not found\n");
				return -EIO;
			}
			regulator_set_voltage(v_1p2_hsic, 1200000, 1200000);
			regulator_enable(v_1p2_hsic);
			printk(KERN_INFO "%s: enable regulator\n", __func__);
			udelay(2);
		}

		mmp3_hsic1_reset();
	} else {
		if (v_1p2_hsic) {
			regulator_disable(v_1p2_hsic);
			regulator_put(v_1p2_hsic);
			v_1p2_hsic = NULL;
		}
	}

	return 0;
}

static char *mmp3_hsic1_clock_name[] = {
	[0] = "U2OCLK",
	[1] = "HSIC1CLK",
};

static struct mv_usb_platform_data mmp3_hsic1_pdata = {
	.clknum		= 2,
	.clkname	= mmp3_hsic1_clock_name,
	.vbus		= NULL,
	.mode		= MV_USB_MODE_HOST,
	.phy_init	= mmp3_hsic_phy_init,
	.phy_deinit     = mmp3_hsic_phy_deinit,
	.set_vbus	= mmp3_hsic1_set_vbus,
	.private_init	= mmp3_hsic_private_init,
};

#endif
#endif

static struct sram_platdata mmp3_asram_info = {
	.pool_name = "asram",
	.granularity = SRAM_GRANULARITY,
};

static struct sram_platdata mmp3_isram_info = {
	.pool_name = "isram",
	.granularity = SRAM_GRANULARITY,
};

static struct i2c_board_info yellowstone_twsi6_info[] = {
	{
	},
};

#ifdef CONFIG_UIO_HDMI
static struct uio_hdmi_platform_data mmp3_hdmi_info __initdata = {
	.sspa_reg_base = 0xD42A0C00,
	/* Fix me: gpio 59 lpm pull ? */
	.gpio = mfp_to_gpio(GPIO59_HDMI_DET),
	.edid_bus_num = 6,
	.hpd_val = 0,
};
#endif

static int yellowstone_pwm_init(struct device *dev)
{
	int gpio = mfp_to_gpio(GPIO84_GPIO);

	if (gpio_request(gpio, "LCD_BKL_EN")) {
		printk(KERN_INFO "gpio %d request failed\n", gpio);
		return -1;
	}

	gpio_direction_output(gpio, 1);
	gpio_free(gpio);

	return 0;
}

static struct platform_pwm_backlight_data yellowstone_lcd_backlight_data = {
	/* primary backlight */
	.pwm_id = 2,
	.max_brightness = 100,
	.dft_brightness = 50,
	.pwm_period_ns = 2000000,
	.init = yellowstone_pwm_init,
};

static struct platform_device yellowstone_lcd_backlight_devices = {
	.name = "pwm-backlight",
	.id = 2,
	.dev = {
		.platform_data = &yellowstone_lcd_backlight_data,
	},
};


static struct resource mmp3_resource_pcm_audio[] = {
	 {
		 /* playback dma */
		.name	= "mmp-adma",
		.start	= 0,
		.flags	= IORESOURCE_DMA,
	},
	 {
		 /* record dma */
		.name	= "mmp-adma",
		.start	= 1,
		.flags	= IORESOURCE_DMA,
	},
};

static struct mmp_audio_platdata mmp_audio_pdata = {
	.period_max_capture = 10 * 1024,
	.buffer_max_capture = 24 * 1024,
	.period_max_playback = 10 * 1024,
	.buffer_max_playback = 24 * 1024,
};

struct platform_device mmp3_device_asoc_platform = {
	.name		= "mmp-pcm-audio",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(mmp3_resource_pcm_audio),
	.resource	= mmp3_resource_pcm_audio,
	.dev = {
		.platform_data  = &mmp_audio_pdata,
	},

};

struct platform_device mmp3_audio_device = {
	.name	= "mmp-audio",
	.id	= -1,
};

/*
 * PMIC Regulator 88PM800
 * Power Supply ECOs:
 * ECO#6: V_2P8(LDO14) is wired to LDO7, so LDO14 should keep off
 */
static struct regulator_consumer_supply regulator_supplies[] = {
	/* BUCK power supplies: BUCK[1..5] */
	[PM800_ID_BUCK1] = REGULATOR_SUPPLY("V_PMIC_SD0", NULL),
	[PM800_ID_BUCK2] = REGULATOR_SUPPLY("V_DDR3", NULL),
	[PM800_ID_BUCK3] = REGULATOR_SUPPLY("V_SD3", NULL),
	[PM800_ID_BUCK4] = REGULATOR_SUPPLY("V_1P8", NULL),
	[PM800_ID_BUCK5] = REGULATOR_SUPPLY("V_SD5", NULL),
	/* LDO power supplies: LDO[1..19] */
	[PM800_ID_LDO1]  = REGULATOR_SUPPLY("V_LDO1", NULL),
	[PM800_ID_LDO2]  = REGULATOR_SUPPLY("V_MIC_BIAS", NULL),
	[PM800_ID_LDO3]  = REGULATOR_SUPPLY("V_1P2_MIPI", NULL),
	[PM800_ID_LDO4]  = REGULATOR_SUPPLY("V_LDO4", NULL),
	[PM800_ID_LDO5]  = REGULATOR_SUPPLY("V_3P3", NULL),
	[PM800_ID_LDO6]  = REGULATOR_SUPPLY("V_PMIC", NULL),
	[PM800_ID_LDO7]  = REGULATOR_SUPPLY("V_2P8"/*V_LDO7*/, NULL),
	[PM800_ID_LDO8]  = REGULATOR_SUPPLY("V_1P2_HSIC", NULL),
	[PM800_ID_LDO9]  = REGULATOR_SUPPLY("V_1P8_USBFE", NULL),
	[PM800_ID_LDO10] = REGULATOR_SUPPLY("V_LCD", NULL),
	[PM800_ID_LDO11] = REGULATOR_SUPPLY("V_1P2_CODEC", NULL),
	[PM800_ID_LDO12] = REGULATOR_SUPPLY("V_LDO12", NULL),
	[PM800_ID_LDO13] = REGULATOR_SUPPLY("V_SDMMC", NULL),
	[PM800_ID_LDO14] = REGULATOR_SUPPLY("V_LDO14"/*V_2P8*/, NULL),
	[PM800_ID_LDO15] = REGULATOR_SUPPLY("V_LDO15", NULL),
	[PM800_ID_LDO16] = REGULATOR_SUPPLY("VBAT_FEM", NULL),
	[PM800_ID_LDO17] = REGULATOR_SUPPLY("V_BB", NULL),
	[PM800_ID_LDO18] = REGULATOR_SUPPLY("AVDD_CAM_2P8V", NULL),
	[PM800_ID_LDO19] = REGULATOR_SUPPLY("V_1P8_ANA", NULL),
};

static int regulator_index[] = {
	PM800_ID_BUCK1,
	PM800_ID_BUCK2,
	PM800_ID_BUCK3,
	PM800_ID_BUCK4,
	PM800_ID_BUCK5,
	PM800_ID_LDO1,
	PM800_ID_LDO2,
	PM800_ID_LDO3,
	PM800_ID_LDO4,
	PM800_ID_LDO5,
	PM800_ID_LDO6,
	PM800_ID_LDO7,
	PM800_ID_LDO8,
	PM800_ID_LDO9,
	PM800_ID_LDO10,
	PM800_ID_LDO11,
	PM800_ID_LDO12,
	PM800_ID_LDO13,
	PM800_ID_LDO14,
	PM800_ID_LDO15,
	PM800_ID_LDO16,
	PM800_ID_LDO17,
	PM800_ID_LDO18,
	PM800_ID_LDO19,
};

#define REG_INIT(_name, _min, _max, _always, _boot)	\
{								\
	.constraints = {					\
		.name		= __stringify(_name),		\
		.min_uV		= _min,				\
		.max_uV		= _max,				\
		.always_on	= _always,			\
		.boot_on	= _boot,			\
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE	\
				| REGULATOR_CHANGE_STATUS,	\
	},							\
	.num_consumer_supplies	= 1,				\
	.consumer_supplies	= &regulator_supplies[PM800_ID_##_name], \
	.driver_data = &regulator_index[PM800_ID_##_name],	\
}
static struct regulator_init_data pm800_regulator_data[] = {
	/* BUCK power supplies: BUCK[1..5] */
	[PM800_ID_BUCK1] = REG_INIT(BUCK1,  600000, 3950000, 1, 1),
	[PM800_ID_BUCK2] = REG_INIT(BUCK2,  600000, 3950000, 1, 1),
	[PM800_ID_BUCK3] = REG_INIT(BUCK3,  600000, 3950000, 1, 1),
	[PM800_ID_BUCK4] = REG_INIT(BUCK4,  600000, 3950000, 1, 1),
	[PM800_ID_BUCK5] = REG_INIT(BUCK5,  600000, 3950000, 0, 0),
	/* LDO power supplies: LDO[1..19] */
	[PM800_ID_LDO1]  = REG_INIT(LDO1,   600000, 1500000, 0, 0),
	[PM800_ID_LDO2]  = REG_INIT(LDO2,   600000, 1500000, 0, 0),
	[PM800_ID_LDO3]  = REG_INIT(LDO3,  1200000, 3300000, 1, 1),
	[PM800_ID_LDO4]  = REG_INIT(LDO4,  1200000, 3300000, 0, 0),
	[PM800_ID_LDO5]  = REG_INIT(LDO5,  1200000, 3300000, 1, 1),
	[PM800_ID_LDO6]  = REG_INIT(LDO6,  1200000, 3300000, 1, 1),
	[PM800_ID_LDO7]  = REG_INIT(LDO7,  1200000, 3300000, 1, 1),
	[PM800_ID_LDO8]  = REG_INIT(LDO8,  1200000, 3300000, 1, 1),
	[PM800_ID_LDO9]  = REG_INIT(LDO9,  1200000, 3300000, 1, 1),
	[PM800_ID_LDO10] = REG_INIT(LDO10, 1200000, 3300000, 1, 1),
	[PM800_ID_LDO11] = REG_INIT(LDO11, 1200000, 3300000, 1, 1),
	[PM800_ID_LDO12] = REG_INIT(LDO12, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO13] = REG_INIT(LDO13, 1200000, 3300000, 1, 1),
	[PM800_ID_LDO14] = REG_INIT(LDO14, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO15] = REG_INIT(LDO15, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO16] = REG_INIT(LDO16, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO17] = REG_INIT(LDO17, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO18] = REG_INIT(LDO18, 1700000, 3300000, 0, 0),
	[PM800_ID_LDO19] = REG_INIT(LDO19, 1700000, 3300000, 1, 1),
};

static struct pm80x_rtc_pdata pm80x_rtc = {
	.rtc_wakeup	= 0,
};

static int pm800_plat_config(struct pm80x_chip *chip,
				struct pm80x_platform_data *pdata)
{
	if (!chip || !pdata || !chip->regmap) {
		pr_err("%s:chip or pdata is not availiable!\n", __func__);
		return -EINVAL;
	}

	/*
	 * Select XO 32KHZ(USE_XO)
	 * Force all CLK32K_1/2/3 buffers to use the XO 32KHZ
	 */
	regmap_update_bits(chip->regmap, PM800_RTC_CONTROL, (1 << 7), (1 << 7));
	/*
	 * Enable 32k out1, out2 and out3 from XO
	 * CLK32K_1: EXT_32K_IN
	 * CLK32K_2: 32K_CLK for WIFI and PM805 bofore B0rev2
	 * CLK32K_3: 32K_CLK_GPS
	 * 32K_CLK for WIFI and PM805 since B0rev2
	 * Enable low jitter version for CLK32K_3 (0x21[5] = 0b1)
	 */
	regmap_write(chip->regmap, 0x21, 0x20);
	regmap_update_bits(chip->regmap, PM800_RTC_MISC2,
			0x3f, (0x2 << 4) | (0x2 << 2) | 0x2);
	return 0;
}

static struct pm80x_headset_pdata pm80x_headset = {
	.headset_flag = 1,
};

static struct pm80x_platform_data pm800_info = {
	.headset = &pm80x_headset,
	.power_page_addr = 0x31,	/* POWER */
	.gpadc_page_addr = 0x32,	/* GPADC */
	.irq_mode = 0,	/* 0: clear IRQ by read */

	.num_regulators = ARRAY_SIZE(pm800_regulator_data),
	.regulator = pm800_regulator_data,
	.rtc = &pm80x_rtc,
	.plat_config = pm800_plat_config,
};
/* End Of PM800 */

static struct pm80x_platform_data pm805_info = {
	.irq_mode = 0,
};

/* Core Voltage Regulator: V_MAIN */
static struct regulator_consumer_supply fan53555_regulator_supply[] = {
	REGULATOR_SUPPLY("V_MAIN", NULL),
};

/* FAN53555 VOUT range:
 * Option 0/1/3/5:	600<-->1230mV
 * Option 4:		603<-->1411mV */
static struct regulator_init_data fan53555_regulator_data = {
	.constraints = {
		.name = "V_MAIN",
		.min_uV = 600000,
		.max_uV = 1411038,
		.always_on = 1,
		.boot_on = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
			| REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_FAST | REGULATOR_MODE_NORMAL,
	},
	.num_consumer_supplies = ARRAY_SIZE(fan53555_regulator_supply),
	.consumer_supplies = fan53555_regulator_supply,
};

static struct fan53555_platform_data fan53555_pdata = {
	.regulator = &fan53555_regulator_data,
	.slew_rate = FAN53555_SLEW_RATE_64MV, /* mV/uS */
	.sleep_vsel_id = FAN53555_VSEL_ID_0, /* VSEL0 */
	.sleep_vol = 1000000, /* uV */
};

static struct i2c_board_info yellowstone_twsi1_info[] = {
	{
		.type = "88PM800",
		.addr = 0x30,
		.irq  = IRQ_MMP3_PMIC,
		.platform_data = &pm800_info,
	},
	{
		.type = "fan53555",
		.addr = 0x60,
		.platform_data = &fan53555_pdata,
	},
};

static struct i2c_board_info yellowstone_twsi3_info[] = {
	{
		.type = "88PM805",
		.addr = 0x38,
		.irq  = MMP_GPIO_TO_IRQ(mfp_to_gpio(GPIO23_GPIO)),
		.platform_data = &pm805_info,
	},
};

/* LCD clock description */
MMP_HW_DESC(fb, "pxa168-fb", 0, 0, "LCDCLK");
struct mmp_hw_desc *yellowstone_hw_desc[] __initdata = {
	&mmp_device_hw_fb,
};

static void __init yellowstone_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(yellowstone_hw_desc); i++)
		mmp_device_hw_register(yellowstone_hw_desc[i]);

	mfp_config(ARRAY_AND_SIZE(yellowstone_pin_config));

	/* on-chip devices */
	mmp3_add_uart(3);
	mmp3_add_twsi(1, NULL, ARRAY_AND_SIZE(yellowstone_twsi1_info));
	mmp3_add_twsi(3, NULL, ARRAY_AND_SIZE(yellowstone_twsi3_info));
	mmp3_add_twsi(6, NULL, ARRAY_AND_SIZE(yellowstone_twsi6_info));

	mmp3_add_keypad(&mmp3_keypad_info);

	mmp3_add_asram(&mmp3_asram_info);
	mmp3_add_isram(&mmp3_isram_info);
	platform_device_register(&mmp3_device_adma0);
	platform_device_register(&mmp3_device_adma1);
	/* audio and codec devices */
	platform_device_register(&mmp3_device_asoc_sspa1);
	platform_device_register(&mmp3_device_asoc_sspa2);
	platform_device_register(&mmp3_device_asoc_platform);
	platform_device_register(&mmp3_audio_device);

#ifdef CONFIG_FB_PXA168
	yellowstone_add_lcd_mipi();
	mmp3_add_tv_out();
#endif

#ifdef CONFIG_UIO_HDMI
	mmp3_add_hdmi(&mmp3_hdmi_info);
#endif

	/* backlight */
	mmp3_add_pwm(3);
	platform_device_register(&yellowstone_lcd_backlight_devices);

	mmp3_add_thermal();

#ifdef CONFIG_MMC_SDHCI_PXAV3
	yellowstone_init_mmc();
#endif /* CONFIG_MMC_SDHCI_PXAV3 */

	platform_device_register(&mmp3_device_rtc);

#if defined(CONFIG_TOUCHSCREEN_VNC)
	platform_device_register(&mmp3_device_vnc_touch);
#endif

#if defined(CONFIG_VIDEO_MMP)
	platform_device_register(&yellowstone_ov5642);
	mmp3_add_cam(1, &mmp_cam_data);
#endif

#ifdef CONFIG_VIDEO_MVISP
	mmp3_init_dxoisp();
#endif

#ifdef CONFIG_USB_MV_UDC
	mmp3_device_u2o.dev.platform_data = (void *)&mmp3_usb_pdata;
	platform_device_register(&mmp3_device_u2o);
#endif

#ifdef CONFIG_USB_EHCI_MV
	mmp3_device_u2oehci.dev.platform_data = (void *)&mmp3_usb_pdata;
	platform_device_register(&mmp3_device_u2oehci);

#ifdef CONFIG_USB_MV_OTG
	mmp3_device_u2ootg.dev.platform_data = (void *)&mmp3_usb_pdata;
	platform_device_register(&mmp3_device_u2ootg);
#endif
#endif

#ifdef CONFIG_USB_EHCI_PXA_U2H_HSIC
	mmp3_hsic1_device.dev.platform_data = (void *)&mmp3_hsic1_pdata;
	platform_device_register(&mmp3_hsic1_device);
#endif
	/* If we have a full configuration then disable any regulators
	 * which are not in use or always_on. */
	regulator_has_full_constraints();
}

MACHINE_START(YELLOWSTONE, "YellowStone")
	.map_io		= mmp_map_io,
	.init_irq	= mmp3_init_irq,
	.timer		= &mmp3_timer,
	.reserve	= mmp3_reserve,
	.handle_irq	= gic_handle_irq,
	.init_machine	= yellowstone_init,
MACHINE_END
