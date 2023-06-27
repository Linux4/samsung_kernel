/*
 *  linux/arch/arm/mach-mmp/ttc_dkb.c
 *
 *  Support for the Marvell PXA910-based TTC_DKB Development Platform.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publishhed by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/onenand.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/input.h>
#include <linux/i2c/pca953x.h>
#include <linux/gpio.h>
#include <linux/mfd/88pm860x.h>
#include <linux/keyreset.h>
#include <linux/platform_data/mv_usb.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/consumer.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/flash.h>
#include <mach/mmp_device.h>
#include <mach/addr-map.h>
#include <mach/mfp-pxa910.h>
#include <mach/pxa910.h>
#include <mach/irqs.h>
#include <mach/regs-usb.h>
#include <mach/regs-mpmu.h>
#include <mach/regs-apmu.h>
#include <media/soc_camera.h>
#include <media/soc_mediabus.h>

#include "common.h"
#include "onboard.h"

#define TTCDKB_GPIO_EXT0(x)	(MMP_NR_BUILTIN_GPIO + ((x < 0) ? 0 :	\
				((x < 16) ? x : 15)))
#define TTCDKB_GPIO_EXT1(x)	(MMP_NR_BUILTIN_GPIO + 16 + ((x < 0) ? 0 : \
				((x < 16) ? x : 15)))

/*
 * 16 board interrupts -- MAX7312 GPIO expander
 * 16 board interrupts -- PCA9575 GPIO expander
 * 24 board interrupts -- 88PM860x PMIC
 */
#define TTCDKB_NR_IRQS		(MMP_NR_IRQS + 16 + 16 + 24)

int emmc_boot;
EXPORT_SYMBOL(emmc_boot);
static int __init emmc_setup(char *__unused)
{
#if defined(CONFIG_MMC_SDHCI_PXAV2)
	emmc_boot = 1;
#endif
	return 1;
}
__setup("emmc_boot", emmc_setup);

static unsigned long ttc_dkb_pin_config[] __initdata = {
	/* UART2 */
	GPIO47_UART2_RXD,
	GPIO48_UART2_TXD,
	/* I2C */
	GPIO53_CI2C_SCL,
	GPIO54_CI2C_SDA,

	/* DFI */
	DF_IO0_ND_IO0,
	DF_IO1_ND_IO1,
	DF_IO2_ND_IO2,
	DF_IO3_ND_IO3,
	DF_IO4_ND_IO4,
	DF_IO5_ND_IO5,
	DF_IO6_ND_IO6,
	DF_IO7_ND_IO7,
	DF_IO8_ND_IO8,
	DF_IO9_ND_IO9,
	DF_IO10_ND_IO10,
	DF_IO11_ND_IO11,
	DF_IO12_ND_IO12,
	DF_IO13_ND_IO13,
	DF_IO14_ND_IO14,
	DF_IO15_ND_IO15,
	DF_nCS0_SM_nCS2_nCS0,
	DF_ALE_SM_WEn_ND_ALE,
	DF_CLE_SM_OEn_ND_CLE,
	DF_WEn_DF_WEn,
	DF_REn_DF_REn,
	DF_RDY0_DF_RDY0,

	/* mmc */
	MMC1_DAT7_MMC1_DAT7,
	MMC1_DAT6_MMC1_DAT6,
	MMC1_DAT5_MMC1_DAT5,
	MMC1_DAT4_MMC1_DAT4,
	MMC1_DAT3_MMC1_DAT3,
	MMC1_DAT2_MMC1_DAT2,
	MMC1_DAT1_MMC1_DAT1,
	MMC1_DAT0_MMC1_DAT0,
	MMC1_CMD_MMC1_CMD,
	MMC1_CLK_MMC1_CLK,
	MMC1_CD_MMC1_CD | MFP_PULL_HIGH,
	MMC1_WP_MMC1_WP | MFP_PULL_LOW,

	/* one wire */
	ONEWIRE_CLK_REQ,

	/* SSP1 (I2S) */
	GPIO24_SSP1_SDATA_IN,
	GPIO21_SSP1_BITCLK,
	GPIO22_SSP1_SYNC,
	GPIO23_SSP1_DATA_OUT,
	/* GSSP */
	GPIO25_GSSP_BITCLK,
	GPIO26_GSSP_SYNC,
	GPIO27_GSSP_TXD,
	GPIO28_GSSP_RXD,
	VCXO_REQ,
};

static unsigned long lcd_pin_config[] __initdata = {
	GPIO81_LCD_FCLK,
	GPIO82_LCD_LCLK,
	GPIO83_LCD_PCLK,
	GPIO84_LCD_DENA,
	GPIO85_LCD_DD0,
	GPIO86_LCD_DD1,
	GPIO87_LCD_DD2,
	GPIO88_LCD_DD3,
	GPIO89_LCD_DD4,
	GPIO90_LCD_DD5,
	GPIO91_LCD_DD6,
	GPIO92_LCD_DD7,
	GPIO93_LCD_DD8,
	GPIO94_LCD_DD9,
	GPIO95_LCD_DD10,
	GPIO96_LCD_DD11,
	GPIO97_LCD_DD12,
	GPIO98_LCD_DD13,
	GPIO100_LCD_DD14,
	GPIO101_LCD_DD15,
	GPIO102_LCD_DD16,
	GPIO103_LCD_DD17,
};

#if defined(CONFIG_VIDEO_MMP)
static unsigned long ccic_dvp_pin_config[] __initdata = {
	GPIO67_CCIC_IN7,
	GPIO68_CCIC_IN6,
	GPIO69_CCIC_IN5,
	GPIO70_CCIC_IN4,
	GPIO71_CCIC_IN3,
	GPIO72_CCIC_IN2,
	GPIO73_CCIC_IN1,
	GPIO74_CCIC_IN0,
	GPIO75_CAM_HSYNC,
	GPIO76_CAM_VSYNC,
	GPIO77_CAM_MCLK,
	GPIO78_CAM_PCLK,
};
#endif
static unsigned long emmc_pin_config[] __initdata = {
	MMC3_DAT7_MMC3_DAT7,
	MMC3_DAT6_MMC3_DAT6,
	MMC3_DAT5_MMC3_DAT5,
	MMC3_DAT4_MMC3_DAT4,
	MMC3_DAT3_MMC3_DAT3,
	MMC3_DAT2_MMC3_DAT2,
	MMC3_DAT1_MMC3_DAT1,
	MMC3_DAT0_MMC3_DAT0,
	MMC3_CMD_MMC3_CMD,
	MMC3_CLK_MMC3_CLK,
};

static struct mtd_partition ttc_dkb_onenand_partitions[] = {
	{
		.name		= "bootloader",
		.offset		= 0,
		.size		= SZ_1M,
		.mask_flags	= MTD_WRITEABLE,
	}, {
		.name		= "reserved",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_128K,
		.mask_flags	= MTD_WRITEABLE,
	}, {
		.name		= "reserved",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_8M,
		.mask_flags	= MTD_WRITEABLE,
	}, {
		.name		= "kernel",
		.offset		= MTDPART_OFS_APPEND,
		.size		= (SZ_2M + SZ_1M),
		.mask_flags	= 0,
	}, {
		.name		= "filesystem",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_32M + SZ_16M,
		.mask_flags	= 0,
	}
};

static struct onenand_platform_data ttc_dkb_onenand_info = {
	.parts		= ttc_dkb_onenand_partitions,
	.nr_parts	= ARRAY_SIZE(ttc_dkb_onenand_partitions),
};

static struct resource ttc_dkb_resource_onenand[] = {
	[0] = {
		.start	= SMC_CS0_PHYS_BASE,
		.end	= SMC_CS0_PHYS_BASE + SZ_1M,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device ttc_dkb_device_onenand = {
	.name		= "onenand-flash",
	.id		= -1,
	.resource	= ttc_dkb_resource_onenand,
	.num_resources	= ARRAY_SIZE(ttc_dkb_resource_onenand),
	.dev		= {
		.platform_data	= &ttc_dkb_onenand_info,
	},
};

#if defined(CONFIG_SOC_CAMERA_OV5642) || defined(CONFIG_SOC_CAMERA_OV5640)
static int cam_ldo12_1p2v_enable(int on)
{
	static struct regulator *r_vcam;
	static int f_enabled;
	if (on && (!f_enabled)) {
		r_vcam = regulator_get(NULL, "v_ldo12");
		if (IS_ERR(r_vcam)) {
			r_vcam = NULL;
			return -EIO;
		} else {
			regulator_set_voltage(r_vcam, 1200000, 1200000);
			regulator_enable(r_vcam);
			f_enabled = 1;
		}
	}

	if (f_enabled && (!on)) {
		if (r_vcam) {
			regulator_disable(r_vcam);
			regulator_put(r_vcam);
			f_enabled = 0;
			r_vcam = NULL;
		}
	}
	return 0;
}

static int camera_sensor_power(struct device *dev, int on)
{
	unsigned int cam_pwr;
	unsigned int cam_reset;
	unsigned int cam_afen;

	cam_pwr = TTCDKB_GPIO_EXT0(6);
	cam_reset = TTCDKB_GPIO_EXT0(4);
	cam_afen = mfp_to_gpio(GPIO49_GPIO49);

	if (cam_pwr) {
		if (gpio_request(cam_pwr, "CAM_PWR")) {
			printk(KERN_ERR "Request GPIO failed,"
					"gpio: %d\n", cam_pwr);
			return -EIO;
		}
	}

	if (gpio_request(cam_reset, "CAM_RESET")) {
		printk(KERN_ERR "Request GPIO failed,"
			"gpio: %d\n", cam_reset);
		return -EIO;
	}
	if (gpio_request(cam_afen, "CAM_AFEN")) {
		printk(KERN_ERR "Request GPIO failed,"
			"gpio: %d\n", cam_afen);
		return -EIO;
	}

	if (on) {
		gpio_direction_output(cam_afen, 1);
		mdelay(1);
		if (cam_pwr)
			gpio_direction_output(cam_pwr, 0);
		mdelay(1);
		gpio_direction_output(cam_reset, 0);
		mdelay(1);
		gpio_direction_output(cam_reset, 1);
		mdelay(1);
		/* set MIPI_AVDD1P2V for MIPI IO */
		cam_ldo12_1p2v_enable(1);
		mdelay(1);
	} else {
		gpio_direction_output(cam_reset, 0);
		mdelay(1);
		gpio_direction_output(cam_reset, 1);
		if (cam_pwr)
			gpio_direction_output(cam_pwr, 1);
		gpio_direction_output(cam_afen, 0);
		mdelay(1);
		cam_ldo12_1p2v_enable(0);
	}
	if (cam_pwr)
		gpio_free(cam_pwr);
	gpio_free(cam_reset);
	gpio_free(cam_afen);
	return 0;
}
#endif

static struct i2c_board_info dkb_i2c_camera[] = {
#if defined(CONFIG_SOC_CAMERA_OV5642)
	{
		I2C_BOARD_INFO("ov5642", 0x3c),
	},
#elif defined(CONFIG_SOC_CAMERA_OV5640)
	{
		I2C_BOARD_INFO("ov5640", 0x3c),
	},
#endif
};

#if defined(CONFIG_SOC_CAMERA_OV5642)
static struct soc_camera_link iclink_ov5642_dvp = {
	.bus_id         = 0,            /* Must match with the camera ID */
	.power          = camera_sensor_power,
	.board_info     = &dkb_i2c_camera[0],
	.i2c_adapter_id = 0,
 /* .flags = SOCAM_MIPI, */
	.module_name    = "ov5642",
	.priv	= "pxa910-dvp",
};

static struct platform_device dkb_ov5642_dvp = {
	.name   = "soc-camera-pdrv",
	.id     = 0,
	.dev    = {
		.platform_data = &iclink_ov5642_dvp,
	},
};
#elif defined(CONFIG_SOC_CAMERA_OV5640)
static struct soc_camera_link iclink_ov5640_mipi = {
	.bus_id         = 0,            /* Must match with the camera ID */
	.power          = camera_sensor_power,
	.board_info     = &dkb_i2c_camera[0],
	.i2c_adapter_id = 0,
	.module_name    = "ov5640",
};

static struct platform_device dkb_ov5640_mipi = {
	.name   = "soc-camera-pdrv",
	.id     = 0,
	.dev    = {
		.platform_data = &iclink_ov5640_mipi,
	},
};
#endif

static struct platform_device *ttc_dkb_devices[] = {
	&pxa910_device_gpio,
	&pxa910_device_rtc,
	&ttc_dkb_device_onenand,
#if defined(CONFIG_SOC_CAMERA_OV5642)
	&dkb_ov5642_dvp,
#elif defined(CONFIG_SOC_CAMERA_OV5640)
	&dkb_ov5640_mipi,
#endif
};

static struct pca953x_platform_data max7312_data[] = {
	{
		.gpio_base	= TTCDKB_GPIO_EXT0(0),
		.irq_base	= MMP_NR_IRQS,
	},
};

static int ttc_dkb_pm860x_fixup(struct pm860x_chip *chip,
			struct pm860x_platform_data *pdata)
{
	int data;
	/*
	Check testpage 0xD7:bit[0~1],if it is b00 or b11, that's to say
	2LSB of 0xD7 is maybe broken, will reset 0xD0~0xD7 to its default
	in test page by set 0xE1:b[7~6]=b00 for loading OTP;
	Besides, 0xE1:b[5~0] work as a counter to record times of D7 broken
	*/
	data = pm860x_page_reg_read(chip->client, 0xD7);
	data &= 0x3;
	if (data == 0x0 || data == 0x3) {
		data = pm860x_page_reg_read(chip->client, 0xE1);
		data &= 0x3F;
		if (data < 0x3F)
			data += 1;
		pm860x_page_reg_write(chip->client, 0xE1, data);
		data = pm860x_page_reg_read(chip->client, 0xE1);
		dev_dbg(chip->dev, "detect 0xD7 broken counter: %d", data);
	}
	/*
	 * Check whether 0xD3.0(plat_det_dis)is set, it means dedicated fused
	 * version of saremo for TD. Then Clear 0xE1[7-6] for reload OTP.
	 */
	data = pm860x_page_reg_read(chip->client, 0xD3);
	if (data & 1) {
		dev_dbg(chip->dev, "Detect [0xD3]:0x%x\n", data);
		data = pm860x_page_reg_read(chip->client, 0xE1);
		data &= 0x3F;
		pm860x_page_reg_write(chip->client, 0xE1, data);
		data = pm860x_page_reg_read(chip->client, 0xE1);
		dev_dbg(chip->dev, "Update [0xE1]: 0x%x\n", data);
	}

	/*confirm the interrupt mask*/
	pm860x_reg_write(chip->client, PM8607_INT_MASK_1, 0x00);
	pm860x_reg_write(chip->client, PM8607_INT_MASK_2, 0x00);
	pm860x_reg_write(chip->client, PM8607_INT_MASK_3, 0x00);

	pm860x_reg_write(chip->client, PM8607_INT_STATUS1, 0x3f);
	pm860x_reg_write(chip->client, PM8607_INT_STATUS1, 0xff);
	pm860x_reg_write(chip->client, PM8607_INT_STATUS1, 0xff);

	/* disable LDO5 turn on/off by LDO3_EN */
	pm860x_reg_write(chip->client, PM8607_MISC2,
	pm860x_reg_read(chip->client, PM8607_MISC2)|0x80);
	/* enable LDO5 for AVDD_USB */
	pm860x_reg_write(chip->client, PM8607_SUPPLIES_EN11,
	pm860x_reg_read(chip->client, PM8607_SUPPLIES_EN11)|0x80);

	/* init GPADC*/
	pm860x_reg_write(chip->client, PM8607_GPADC_MISC1, 0x0b);
	/* init power mode*/
	pm860x_reg_write(chip->client, PM8607_SLEEP_MODE1, 0xaa);
	pm860x_reg_write(chip->client, PM8607_SLEEP_MODE2, 0xaa);
	pm860x_reg_write(chip->client, PM8607_SLEEP_MODE3, 0xa2);
	/* set LDO14_SLP to be active in sleep mode */
	pm860x_reg_write(chip->client, PM8607_SLEEP_MODE4, 0x38);

	/* set vbuck1 0.9v in sleep*/
	pm860x_reg_write(chip->client, PM8607_SLEEP_BUCK1, 0x24);
	/* set vbuck2(power supply to DDR) to 1.8V for 920/910 */
	pm860x_reg_write(chip->client, PM8607_SLEEP_BUCK2, 0x24);

	/*RTC to use ext 32k clk*/
	pm860x_set_bits(chip->client, PM8607_RTC1, 1<<6, 1<<6);
	/*Enable RTC to use ext 32k clk*/
	pm860x_set_bits(chip->client, PM8607_RTC_MISC2, 0x7, 0x2);

	/* audio save power */
	pm860x_reg_write(chip->client, PM8607_LP_CONFIG1, 0x40);
	/*to save pmic leakage*/
	pm860x_reg_write(chip->client, PM8607_LP_CONFIG3, 0x80);
	pm860x_reg_write(chip->client, PM8607_B0_MISC1, 0x80);
	pm860x_reg_write(chip->client, PM8607_MEAS_OFF_TIME1, 0x2);
	/* config sanremo Buck Controls Register to its default value
	to save 0.04mA in suspend. */
	pm860x_reg_write(chip->client, PM8607_BUCK_CONTROLS, 0x2b);
	pm860x_reg_write(chip->client, PM8607_LP_CONFIG2, 0x98);

	/* force LDO4 be active in sleep mode, required by CP */
	pm860x_set_bits(chip->client, PM8607_SLEEP_MODE2, 3 << 4, 3 << 4);
	return 0;
}

static struct pm860x_touch_pdata ttc_dkb_touch = {
	.gpadc_prebias	= 1,
	.slot_cycle	= 1,
	.tsi_prebias	= 6,
	.pen_prebias	= 16,
	.pen_prechg	= 2,
	.res_x		= 300,
};

static struct pm860x_headset_pdata headset_platform_info = {
	.headset_flag = 1,
	/* headset switch */
	.headset_data[0].name = "h2w",
	.headset_data[0].gpio = 0,
	.headset_data[0].name_on = NULL,
	.headset_data[0].name_off = NULL,
	.headset_data[0].state_on = NULL,
	.headset_data[0].state_off = NULL,
	/* hook switch */
	.headset_data[1].name = "h3w",
	.headset_data[1].gpio = 0,
	.headset_data[1].name_on = NULL,
	.headset_data[1].name_off = NULL,
	.headset_data[1].state_on = NULL,
	.headset_data[1].state_off = NULL,
};

static struct pm860x_backlight_pdata ttc_dkb_backlight[] = {
	{
		.id	= PM8606_ID_BACKLIGHT,
		.iset	= PM8606_WLED_CURRENT(4),
		.flags	= PM8606_BACKLIGHT1,
	},
};

static struct pm860x_led_pdata ttc_dkb_led[] = {
	{
		.id	= PM8606_ID_LED,
		.iset	= PM8606_LED_CURRENT(12),
		.flags	= PM8606_LED1_RED,
	}, {
		.id	= PM8606_ID_LED,
		.iset	= PM8606_LED_CURRENT(12),
		.flags	= PM8606_LED1_GREEN,
	}, {
		.id	= PM8606_ID_LED,
		.iset	= PM8606_LED_CURRENT(12),
		.flags	= PM8606_LED1_BLUE,
	},
};

static struct regulator_consumer_supply ttc_dkb_regulator_supply[] = {
	[PM8607_ID_BUCK1]	= REGULATOR_SUPPLY("v_buck1", NULL),
	[PM8607_ID_BUCK3]	= REGULATOR_SUPPLY("v_buck3", NULL),
	[PM8607_ID_LDO1]	= REGULATOR_SUPPLY("v_ldo1", NULL),
	[PM8607_ID_LDO2]	= REGULATOR_SUPPLY("v_ldo2", NULL),
	[PM8607_ID_LDO3]	= REGULATOR_SUPPLY("v_ldo3", NULL),
	[PM8607_ID_LDO4]	= REGULATOR_SUPPLY("v_ldo4", NULL),
	[PM8607_ID_LDO5]	= REGULATOR_SUPPLY("v_ldo5", NULL),
	[PM8607_ID_LDO6]	= REGULATOR_SUPPLY("v_ldo6", NULL),
	[PM8607_ID_LDO7]	= REGULATOR_SUPPLY("v_ldo7", NULL),
	[PM8607_ID_LDO8]	= REGULATOR_SUPPLY("v_ldo8", NULL),
	[PM8607_ID_LDO9]	= REGULATOR_SUPPLY("v_ldo9", NULL),
	[PM8607_ID_LDO10]	= REGULATOR_SUPPLY("v_ldo10", NULL),
	[PM8607_ID_LDO12]	= REGULATOR_SUPPLY("v_ldo12", NULL),
	[PM8607_ID_LDO13]	= REGULATOR_SUPPLY("v_ldo13", NULL),
	[PM8607_ID_LDO14]	= REGULATOR_SUPPLY("v_ldo14", NULL),
};

static int regulator_index[] = {
	PM8607_ID_BUCK1,
	PM8607_ID_BUCK2,
	PM8607_ID_BUCK3,
	PM8607_ID_LDO1,
	PM8607_ID_LDO2,
	PM8607_ID_LDO3,
	PM8607_ID_LDO4,
	PM8607_ID_LDO5,
	PM8607_ID_LDO6,
	PM8607_ID_LDO7,
	PM8607_ID_LDO8,
	PM8607_ID_LDO9,
	PM8607_ID_LDO10,
	PM8607_ID_LDO11,
	PM8607_ID_LDO12,
	PM8607_ID_LDO13,
	PM8607_ID_LDO14,
	PM8607_ID_LDO15,
};

#define DKB_REG_INIT(_name, _min, _max, _always, _boot)			\
{									\
	.constraints = {						\
		.name		= __stringify(_name),			\
		.min_uV		= _min,					\
		.max_uV		= _max,					\
		.always_on	= _always,				\
		.boot_on	= _boot,				\
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE		\
				| REGULATOR_CHANGE_STATUS,		\
	},								\
	.num_consumer_supplies	= 1,					\
	.consumer_supplies	=					\
			&ttc_dkb_regulator_supply[PM8607_ID_##_name],	\
	.driver_data		= &regulator_index[PM8607_ID_##_name],  \
}

static struct regulator_init_data ttc_dkb_regulator_init_data[] = {
	DKB_REG_INIT(BUCK1, 1000000, 1500000, 1, 1),
	DKB_REG_INIT(BUCK3, 1000000, 3000000, 1, 1),
	DKB_REG_INIT(LDO1, 1200000, 2800000, 1, 1),
	DKB_REG_INIT(LDO2, 1800000, 3300000, 1, 1),
	DKB_REG_INIT(LDO3, 1800000, 3300000, 1, 1),
	DKB_REG_INIT(LDO4, 1800000, 3300000, 0, 0),
	DKB_REG_INIT(LDO5, 2900000, 3300000, 1, 1),
	DKB_REG_INIT(LDO6, 1800000, 3300000, 1, 1),
	DKB_REG_INIT(LDO7, 1800000, 2900000, 1, 1),
	DKB_REG_INIT(LDO8, 1800000, 2900000, 1, 1),
	DKB_REG_INIT(LDO9, 1800000, 3300000, 1, 1),
	DKB_REG_INIT(LDO10, 1200000, 3300000, 1, 1),
	DKB_REG_INIT(LDO12, 1200000, 3300000, 0, 1),
	DKB_REG_INIT(LDO13, 1200000, 3000000, 0, 0),
	DKB_REG_INIT(LDO14, 1800000, 3300000, 0, 1),
};

#if defined(CONFIG_GPIO_PCA953X)
static struct pca953x_platform_data pca9575_data[] = {
	[0] = {
		.gpio_base  = TTCDKB_GPIO_EXT1(0),
	},
};
#endif

static struct pm860x_platform_data ttc_dkb_pm8607_info = {
	.backlight	= &ttc_dkb_backlight[0],
	.led		= &ttc_dkb_led[0],
	.touch		= &ttc_dkb_touch,
	.regulator	= &ttc_dkb_regulator_init_data[0],
	.fixup		= ttc_dkb_pm860x_fixup,
	.headset	= &headset_platform_info,
	.companion_addr	= 0x11,
	.irq_mode	= 0,
	.irq_base	= IRQ_BOARD_START,
	.i2c_port	= GI2C_PORT,
	.num_backlights	= ARRAY_SIZE(ttc_dkb_backlight),
	.num_leds	= ARRAY_SIZE(ttc_dkb_led),
	.num_regulators	= ARRAY_SIZE(ttc_dkb_regulator_init_data),
};

static struct i2c_board_info ttc_dkb_i2c_info[] = {
	{
		.type           = "88PM860x",
		.addr           = 0x34,
		.platform_data  = &ttc_dkb_pm8607_info,
		.irq            = IRQ_PXA910_PMIC_INT,
	},
	{
		.type		= "max7312",
		.addr		= 0x23,
		.irq		= MMP_GPIO_TO_IRQ(80),
		.platform_data	= &max7312_data,
	},
#if defined(CONFIG_GPIO_PCA953X)
	{
		.type           = "pca9575",
		.addr           = 0x20,
		.irq            = MMP_GPIO_TO_IRQ(19),
		.platform_data  = &pca9575_data,
	},
#endif
};

#ifdef CONFIG_USB_SUPPORT
#if defined(CONFIG_USB_MV_UDC) || defined(CONFIG_USB_EHCI_MV_U2O)

static char *pxa910_usb_clock_name[] = {
	[0] = "U2OCLK",
};

static struct mv_usb_platform_data ttc_usb_pdata = {
	.clknum		= 1,
	.clkname	= pxa910_usb_clock_name,
	.vbus		= NULL,
	.mode		= MV_USB_MODE_OTG,
	.otg_force_a_bus_req = 1,
	.phy_init	= pxa_usb_phy_init,
	.phy_deinit	= pxa_usb_phy_deinit,
	.set_vbus	= NULL,
};
#endif
#endif

/* MMC0 controller for SD-MMC */
static struct sdhci_pxa_platdata pxa910_sdh_platdata_mmc0 = {
	.clk_delay_sel		= 1,
	.clk_delay_cycles	= 2,
};

/* MMC2 controller for eMMC */
static struct sdhci_pxa_platdata pxa910_sdh_platdata_mmc2 = {
	.flags			= PXA_FLAG_CARD_PERMANENT
					| PXA_FLAG_SD_8_BIT_CAPABLE_SLOT,
	.clk_delay_sel		= 1,
	.clk_delay_cycles	= 2,
};

static void __init pxa910_init_mmc(void)
{
	unsigned long sd_pwr_cfg = GPIO15_MMC1_POWER;
	int sd_pwr_en = 0;

	mfp_config(&sd_pwr_cfg, 1);
	sd_pwr_en = mfp_to_gpio(sd_pwr_cfg);

	if (gpio_request(sd_pwr_en, "SD Power Ctrl")) {
		printk(KERN_ERR "Failed to request SD_PWR_EN(gpio %d)\n", sd_pwr_en);
		sd_pwr_en = 0;
	} else {
		gpio_direction_output(sd_pwr_en, 1);
		gpio_free(sd_pwr_en);
	}

	if (emmc_boot)
		mfp_config(ARRAY_AND_SIZE(emmc_pin_config));

	/* Always register SDHC2 as we need to support both PXA920 (no eMMC)
	 * and PXA921 (with eMMC). Otherwise the controller device number will
	 * be different on two platform, which causes Android cannot mount SD
	 * card correctly.
	 */
	pxa910_add_sdh(2, &pxa910_sdh_platdata_mmc2); /* eMMC */

	pxa910_add_sdh(0, &pxa910_sdh_platdata_mmc0); /* SD/MMC */
}

static struct sram_platdata pxa910_asram_info = {
	.pool_name = "asram",
	.granularity = SRAM_GRANULARITY,
};

struct platform_device pxa910_device_asoc_ssp1 = {
	.name		= "pxa-ssp-dai",
	.id		= 1,
};

struct platform_device pxa910_device_asoc_gssp = {
	.name		= "pxa-ssp-dai",
	.id		= 4,
};

struct platform_device pxa910_device_asoc_pcm = {
	.name		= "pxa-pcm-audio",
	.id		= -1,
};

static struct platform_device ttc_dkb_audio_device = {
	.name	= "ttc-dkb-audio",
	.id	= -1,
};

#ifdef CONFIG_MTD_NAND_PXA3xx
static struct pxa3xx_nand_platform_data dkb_nand_info = {
	.attr		= ARBI_EN | NAKED_CMD | POLLING,
	.num_cs		= 1,
};
#endif

/*
 * for wvga panel:
 * 1: truly wvga panel
 * 2: sharp wvga panel
 */
#define TRULY_WVGA_PANEL 1
#define SHARP_WVGA_PANEL 2
static int wvga_lcd = 0;
static int __init wvga_lcd_setup(char *str)
{
	int n;
	if (!get_option(&str, &n))
		return 0;
	wvga_lcd = n;
	return 1;
}
__setup("wvga_lcd=", wvga_lcd_setup);

static int is_wvga_lcd(void)
{
	return wvga_lcd;
}

#ifdef CONFIG_INPUT_KEYRESET
static struct keyreset_platform_data ttc_dkb_panic_keys_pdata = {
	.keys_down = {
		KEY_HOME,
		KEY_VOLUMEUP,
		0
	},
	.panic_before_reset = 1,
};

static struct platform_device ttc_dkb_panic_keys_device = {
	.name = KEYRESET_NAME,
	.dev.platform_data = &ttc_dkb_panic_keys_pdata,
};
#endif

#if defined(CONFIG_VIDEO_MMP)
static int pxa910_cam_clk_init(struct device *dev, int init)
{
	struct mmp_cam_pdata *data = dev->platform_data;
	if ((!data->clk_enabled) && init) {
		data->clk[0] = clk_get(dev, "CCICGATECLK");
		if (IS_ERR(data->clk[0])) {
			dev_err(dev, "Could not get gateclk\n");
			goto out_clk0;
		}
		data->clk[1] = clk_get(dev, "CCICRSTCLK");
		if (IS_ERR(data->clk[1])) {
			dev_err(dev, "Could not get rstclk\n");
			goto out_clk1;
		}
		data->clk[2] = clk_get(dev, "LCDCLK");
		if (IS_ERR(data->clk[2])) {
			dev_err(dev, "Could not get lcd clk\n");
			goto out_clk2;
		}
		data->clk_enabled = 1;

		return 0;
	}

	if (!init && data->clk_enabled) {
		clk_put(data->clk[0]);
		clk_put(data->clk[1]);
		clk_put(data->clk[2]);
		data->clk_enabled = 0;
		return 0;
	}
	return -EFAULT;

out_clk0:
		return PTR_ERR(data->clk[0]);
out_clk1:
		clk_put(data->clk[0]);
		return PTR_ERR(data->clk[1]);
out_clk2:
		clk_put(data->clk[0]);
		clk_put(data->clk[1]);
		return PTR_ERR(data->clk[2]);
}

static void pxa910_cam_set_clk(struct device *dev, int on)
{
	struct mmp_cam_pdata *data = dev->platform_data;
	if (on == 1) {
		clk_enable(data->clk[0]);
		if (data->bus_type == V4L2_MBUS_CSI2_LANES) {
			clk_set_rate(data->clk[1], 0x6a3f);
			__raw_writel(0x06000000 | __raw_readl(APMU_DEBUG),
					APMU_DEBUG);
		} else {
			clk_set_rate(data->clk[1], 0x01f);
		}
		clk_enable(data->clk[2]);
	} else {
		clk_disable(data->clk[0]);
		clk_set_rate(data->clk[1], 0x6800);
		__raw_writel((~0x06000000) & __raw_readl(APMU_DEBUG),
				APMU_DEBUG);
		clk_disable(data->clk[2]);
	}
}

struct mmp_cam_pdata mv_cam_data = {
	.name = "TD/TTC",
	.clk_enabled = 0,
	.lane = 2,
	/*.dphy = {0x1b0b, 0x33, 0x1a03}, */
	/*.qos_req_min = 624, */
	.dma_burst = 64,
	/*.bus_type = V4L2_MBUS_CSI2_LANES, */
	.mipi_enabled = 0,
	.mclk_div = 13,
	.mclk_min = 24,
	.mclk_src = 2,
	.init_clk = pxa910_cam_clk_init,
	.enable_clk = pxa910_cam_set_clk,
#if defined(CONFIG_SOC_CAMERA_OV5640)
	.bus_type = V4L2_MBUS_CSI2_LANES,
	.dphy = {0x0a06, 0x33, 0x0900},
	.dphy3_algo = 1,
#endif
};
#else
struct mmp_cam_pdata mv_cam_data;
#endif

/* clk usage desciption */
MMP_HW_DESC(fb, "pxa168-fb", 0, 0, "LCDCLK");
struct mmp_hw_desc *ttc_dkb_hw_desc[] __initdata = {
	&mmp_device_hw_fb,
};

static void __init ttc_dkb_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ttc_dkb_hw_desc); i ++)
		mmp_device_hw_register(ttc_dkb_hw_desc[i]);

	mfp_config(ARRAY_AND_SIZE(ttc_dkb_pin_config));

	/* on-chip devices */
	pxa910_add_uart(1);
	pxa910_add_1wire();
#ifdef CONFIG_MTD_NAND_PXA3xx
	pxa910_add_nand(&dkb_nand_info);
#endif

#if defined(CONFIG_MMC_SDHCI_PXAV2)
	pxa910_init_mmc();
#endif

	if (!emmc_boot)
		pxa910_add_nand(&dkb_nand_info);

	/* off-chip devices */
	pxa910_add_twsi(0, NULL, ARRAY_AND_SIZE(ttc_dkb_i2c_info));
	platform_add_devices(ARRAY_AND_SIZE(ttc_dkb_devices));

	/* add audio device: sram, ssp2, gssp, squ(tdma), pxa-ssp, mmp-pcm */
	pxa910_add_asram(&pxa910_asram_info);
	pxa910_add_ssp(1);
	pxa910_add_ssp(4);
	platform_device_register(&pxa910_device_squ);
	platform_device_register(&pxa910_device_asoc_platform);
	platform_device_register(&pxa910_device_asoc_ssp1);
	platform_device_register(&pxa910_device_asoc_gssp);
	platform_device_register(&pxa910_device_asoc_pcm);
	platform_device_register(&ttc_dkb_audio_device);
	/* enable vcxo for audio */
	__raw_writel(0x1, MPMU_REG(0x0018));
	/* enable i2s bus clock for audio */
	__raw_writel(0xf820130b, MPMU_ISCCRX0);
	__raw_writel(0xf820130b, MPMU_ISCCRX1);

#ifdef CONFIG_FB_PXA168
	mfp_config(ARRAY_AND_SIZE(lcd_pin_config));
	if (TRULY_WVGA_PANEL == is_wvga_lcd()) {
		dkb_add_lcd_truly();
		pr_info("LCD: truly WVGA panel selected.\n");
	} else if (SHARP_WVGA_PANEL == is_wvga_lcd()) {
		dkb_add_lcd_sharp();
		pr_info("LCD: sharp WVGA panel selected.\n");
	} else
		dkb_add_lcd_tpo();
#endif

#ifdef CONFIG_USB_MV_UDC
	pxa168_device_u2o.dev.platform_data = &ttc_usb_pdata;
	platform_device_register(&pxa168_device_u2o);
#endif

#ifdef CONFIG_USB_EHCI_MV_U2O
	pxa168_device_u2oehci.dev.platform_data = &ttc_usb_pdata;
	platform_device_register(&pxa168_device_u2oehci);
#endif

#ifdef CONFIG_VIDEO_MMP
	mfp_config(ARRAY_AND_SIZE(ccic_dvp_pin_config));
	pxa910_add_cam(&mv_cam_data);
#endif

#ifdef CONFIG_USB_MV_OTG
	pxa168_device_u2ootg.dev.platform_data = &ttc_usb_pdata;
	platform_device_register(&pxa168_device_u2ootg);
#endif

#ifdef CONFIG_INPUT_KEYRESET
	if (platform_device_register(&ttc_dkb_panic_keys_device))
		pr_warning("%s: register reset key fail\n", __func__);
#endif

	pxa910_add_cnm();
}

MACHINE_START(TTC_DKB, "PXA910-based")
	.map_io		= mmp_map_io,
	.nr_irqs	= TTCDKB_NR_IRQS,
	.init_irq       = pxa910_init_irq,
	.timer          = &pxa910_timer,
	.init_machine   = ttc_dkb_init,
	.restart	= mmp_restart,
MACHINE_END
