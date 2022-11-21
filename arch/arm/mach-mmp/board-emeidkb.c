/*
 *  linux/arch/arm/mach-mmp/board-emeidkb.c
 *
 *  Support for the Marvell PXA988 Emei DKB Development Platform.
 *
 *  Copyright (C) 2012 Marvell International Ltd.
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
#include <linux/i2c.h>
#include <linux/i2c/ft5306_touch.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/backlight.h>
#include <linux/mfd/88pm80x.h>
#include <linux/regulator/machine.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdhci.h>
#include <linux/platform_data/pxa_sdhci.h>
#include <linux/sd8x_rfkill.h>
#include <linux/regmap.h>
#include <linux/mfd/88pm80x.h>
#include <linux/platform_data/mv_usb.h>
#include <linux/pm_qos.h>
#include <linux/clk.h>
#include <linux/lps331ap.h>
#include <linux/spi/pxa2xx_spi.h>
#include <linux/spi/cmmb.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/keyreset.h>
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif

#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>

#include <mach/addr-map.h>
#include <mach/clock-pxa988.h>
#include <mach/mmp_device.h>
#include <mach/mfp-pxa988.h>
#include <mach/irqs.h>
#include <mach/isl29043.h>
#include <mach/pxa988.h>
#include <mach/soc_coda7542.h>
#include <mach/uio_isp.h>
#include <mach/regs-rtc.h>
#include <mach/regs-ciu.h>
#include <plat/pxa27x_keypad.h>
#include <plat/pm.h>
#include <media/soc_camera.h>
#include <mach/isp_dev.h>
#include <mach/gpio-edge.h>

#ifdef CONFIG_CHARGER_ISL9226
#include <linux/power/isl9226.h>
#endif

#if defined(CONFIG_INV_MPU_IIO)
#include <linux/mpu.h>
#endif
#ifdef CONFIG_VIDEO_MVISP_SENSOR
#include <mach/sensor-data.h>
#endif
#if defined(CONFIG_VIDEO_VCM_DW9714L)
#include <media/dw9714l.h>
#endif
#ifdef CONFIG_PM_DEVFREQ
#include <plat/devfreq.h>
#endif
#include "common.h"
#include "onboard.h"

#define VER_1V0 0x10
#define VER_1V1 0x11
static int board_id;
static int __init board_id_setup(char *str)
{
	int n;
	if (!get_option(&str, &n))
		return 0;
	board_id = n;
	return 1;
}
__setup("board_id=", board_id_setup);

static unsigned long emeidkb_pin_config[] __initdata = {
	GPIO000_KP_MKIN0,			/* KP_MKIN[0] */
	GPIO001_KP_MKOUT0 | MFP_LPM_DRIVE_HIGH
			  | MFP_PULL_FLOAT,	/* KP_MKOUT[0] */
	GPIO002_KP_MKIN1,			/* KP_MKIN[1] */
	GPIO003_KP_MKOUT1 | MFP_LPM_DRIVE_HIGH
			  | MFP_PULL_FLOAT,	/* KP_MKOUT[1] */
	GPIO004_KP_MKIN2,			/* KP_MKIN[2] */
	GPIO005_KP_MKOUT2 | MFP_LPM_DRIVE_HIGH
			  | MFP_PULL_FLOAT,	/* KP_MKOUT[2] */
	GPIO006_KP_MKIN3,			/* KP_MKIN[3] */

#define GPIO007_GPIO_WIB_PDn		(GPIO007_GPIO_7 | MFP_PULL_FLOAT)
#define GPIO008_GPIO_WIB_WLAN		(GPIO008_GPIO_8 | MFP_PULL_LOW)
#define GPIO009_GPIO_WIB_BT		(GPIO009_GPIO_9 | MFP_PULL_LOW)
#define GPIO010_GPIO_RF_DCDC_EN		GPIO010_GPIO_10
#define GPIO011_GPIO_WIB_RESETn		(GPIO011_GPIO_11 | MFP_PULL_FLOAT)
#define GPIO012_GPIO_TORCH_EN		GPIO012_GPIO_12
#define GPIO013_GPIO_CMMB_IRQ		GPIO013_GPIO_13
#define GPIO014_GPIO_PROX_IRQ		GPIO014_GPIO_14
#define GPIO015_GPIO_NFC_EN		(GPIO015_GPIO_15 \
					| MFP_PULL_LOW | MFP_LPM_FLOAT)
#define GPIO016_GPIO_TP_RESET		GPIO016_GPIO_16
#define GPIO017_GPIO_TP_INT		GPIO017_GPIO_17
#define GPIO018_GPIO_CMMB_EN		GPIO018_GPIO_18
#define GPIO019_GPIO_CMMB_RESET		GPIO019_GPIO_19
#define GPIO020_GPIO_FLASH_EN		GPIO020_GPIO_20
	GPIO007_GPIO_WIB_PDn,
	GPIO008_GPIO_WIB_WLAN,
	GPIO009_GPIO_WIB_BT,
	GPIO010_GPIO_RF_DCDC_EN,
	GPIO011_GPIO_WIB_RESETn,
	GPIO012_GPIO_TORCH_EN | MFP_PULL_FLOAT | MFP_LPM_FLOAT,
	GPIO013_GPIO_CMMB_IRQ,
	GPIO014_GPIO_PROX_IRQ | MFP_PULL_HIGH,
	GPIO015_GPIO_NFC_EN,
	GPIO016_GPIO_TP_RESET | MFP_PULL_FLOAT,
	GPIO017_GPIO_TP_INT | MFP_PULL_LOW,
	GPIO018_GPIO_CMMB_EN,
	GPIO019_GPIO_CMMB_RESET,
	GPIO020_GPIO_FLASH_EN | MFP_PULL_LOW | MFP_LPM_DRIVE_LOW,

	GPIO021_I2S_BITCLK,	/* I2S_BITCLK */
	GPIO022_I2S_SYNC,	/* I2S_SYNC */
	GPIO023_I2S_DATA_OUT,	/* I2S_DATA_OUT */
	GPIO024_I2S_SDATA_IN,	/* I2S_DATA_IN */

	GPIO025_GSSP_SCLK,	/* PCM_CLK */
	GPIO026_GSSP_SFRM,	/* PCM_SYNC */
	GPIO027_GSSP_TXD,	/* PCM_TXD */
	GPIO028_GSSP_RXD,	/* PCM_RXD */

#define GPIO029_GPIO_CHARGER_IND1	(GPIO029_GPIO_29 | MFP_PULL_LOW)
#define GPIO030_GPIO_CHARGER_IND2	(GPIO030_GPIO_30 | MFP_PULL_LOW)
#define GPIO031_GPIO_CHARGER_EN		(GPIO031_GPIO_31 | \
					 MFP_PULL_LOW | MFP_LPM_FLOAT)
#define GPIO032_GPIO_LCD_BL		GPIO032_GPIO_32
	GPIO029_GPIO_CHARGER_IND1,
	GPIO030_GPIO_CHARGER_IND2,
	GPIO031_GPIO_CHARGER_EN,
	GPIO032_GPIO_LCD_BL | MFP_PULL_FLOAT | MFP_LPM_FLOAT,

	GPIO033_SPI_DCLK,	/* CMMB_SPI_CLK */
	GPIO034_SPI_CS0,	/* CMMB_SPI_CS */
	GPIO035_SPI_DIN,	/* CMMB_SPI_DOUT */
	GPIO036_SPI_DOUT,	/* CMMB_SPI_DIN */

	/* MMC2 WIB */
	GPIO037_MMC2_DATA3,	/* WLAN_DAT3 */
	GPIO038_MMC2_DATA2,	/* WLAN_DAT2 */
	GPIO039_MMC2_DATA1,	/* WLAN_DAT1 */
	GPIO040_MMC2_DATA0,	/* WLAN_DAT0 */
	GPIO041_MMC2_CMD,	/* WLAN_CMD */
	GPIO042_MMC2_CLK | MFP_LPM_DRIVE_HIGH,	/* WLAN_CLK */

#define GPIO043_GPIO_DVC1	(GPIO043_GPIO_43 | MFP_PULL_FLOAT)
#define GPIO044_GPIO_DVC2	(GPIO044_GPIO_44 | MFP_PULL_FLOAT)
	GPIO043_GPIO_DVC1,
	GPIO044_GPIO_DVC2,

	GPIO045_UART2_RXD,	/* GPS_UART_RXD */
	GPIO046_UART2_TXD,	/* GPS_UART_TXD */

	GPIO047_UART1_RXD,	/* AP_RXD */
	GPIO048_UART1_TXD,	/* AP_TXD */

#define GPIO049_GPIO_BARA_INT2	GPIO049_GPIO_49
#define GPIO050_GPIO_BARA_INT1	GPIO050_GPIO_50
	GPIO049_GPIO_BARA_INT2 | MFP_PULL_LOW,
	GPIO050_GPIO_BARA_INT1 | MFP_PULL_LOW,

	GPIO051_UART0_RXD,	/* CP_RXD */
	GPIO052_UART0_TXD,	/* CP_TXD */

	GPIO053_CI2C_SCL | MFP_LPM_FLOAT,	/* CI2C_SCL */
	GPIO054_CI2C_SDA | MFP_LPM_FLOAT,	/* CI2C_SDA */

#define GPIO084_GPIO_GPS_RESET_N	(GPIO084_GPIO_84 | MFP_PULL_FLOAT)
#define GPIO085_GPIO_GPS_CLK_EN		(GPIO085_GPIO_85 | MFP_PULL_FLOAT)
#define GPIO086_GPIO_GPS_SEN_EN		(GPIO086_GPIO_86 | MFP_PULL_LOW | \
					 MFP_LPM_FLOAT)
	GPIO084_GPIO_GPS_RESET_N,
	GPIO085_GPIO_GPS_CLK_EN,
	GPIO086_GPIO_GPS_SEN_EN,

	GPIO087_CI2C_SCL_2 | MFP_LPM_FLOAT,	/* CI2C_SCL2 */
	GPIO088_CI2C_SDA_2 | MFP_LPM_FLOAT,	/* CI2C_SDA2 */

	GPIO089_GPIO_89,	/* GPS_ECLK: GPIO by default */
	GPIO090_CMMB_CLK,	/* CMMB_26MHz */

#define GPIO094_GPIO_NFC_IRQ		(GPIO094_GPIO_94 | MFP_PULL_LOW)
#define GPIO095_GPIO_MOTION_INT		(GPIO095_GPIO_95 | MFP_PULL_LOW)
#define GPIO096_GPIO_GPS_ON_OFF		(GPIO096_GPIO_96 | MFP_PULL_FLOAT)
#define GPIO097_GPIO_GPS_PPS		(GPIO097_GPIO_97 | MFP_PULL_LOW)
#define GPIO124_GPIO_CODEC_INT		GPIO124_GPIO_124
	GPIO094_GPIO_NFC_IRQ,
	GPIO095_GPIO_MOTION_INT,
	GPIO096_GPIO_GPS_ON_OFF,
	GPIO097_GPIO_GPS_PPS,
	GPIO124_GPIO_CODEC_INT,

	/* MMC1 Micro SD */
	MMC1_DAT7_MMC1_DAT7 | MFP_PULL_LOW | MFP_LPM_FLOAT,
	MMC1_DAT6_MMC1_DAT6 | MFP_PULL_LOW | MFP_LPM_FLOAT,
	MMC1_DAT5_MMC1_DAT5 | MFP_PULL_LOW | MFP_LPM_FLOAT,
	MMC1_DAT4_MMC1_DAT4 | MFP_PULL_LOW | MFP_LPM_FLOAT,
	MMC1_DAT3_MMC1_DAT3,
	MMC1_DAT2_MMC1_DAT2,
	MMC1_DAT1_MMC1_DAT1,
	MMC1_DAT0_MMC1_DAT0,
	MMC1_CMD_MMC1_CMD,
	MMC1_CLK_MMC1_CLK | MFP_LPM_DRIVE_HIGH,
	MMC1_CD_MMC1_CD | MFP_PULL_HIGH,
	MMC1_WP_MMC1_WP | MFP_PULL_LOW | MFP_LPM_FLOAT,

	/* MMC3 16GB EMMC */
	ND_IO7_MMC3_DAT7,
	ND_IO6_MMC3_DAT6,
	ND_IO5_MMC3_DAT5,
	ND_IO4_MMC3_DAT4,
	ND_IO3_MMC3_DAT3,
	ND_IO2_MMC3_DAT2,
	ND_IO1_MMC3_DAT1,
	ND_IO0_MMC3_DAT0,
	ND_CLE_SM_OEN_MMC3_CMD,
	SM_SCLK_MMC3_CLK | MFP_LPM_DRIVE_HIGH,
	SM_BEN0_MMC3_RSTN,

#define GPIO_GPS_TIMER_SYNC	ANT_SW4_GPIO_28
#define GPIO_RF_PDET_EN		SM_ADV_GPIO_0
#define GPIO_LCD_RESET_N	ND_RDY1_GPIO_1
#define GPIO_LED_B_CTRL		SM_ADVMUX_GPIO_2
#define GPIO_LED_R_CTRL		SM_BEN1_GPIO_127
#define GPIO_LED_G_CTRL		SM_CSN0_GPIO_103
#define GPIO_GPS_LDO_EN		(SM_CSN1_GPIO_104 | MFP_PULL_FLOAT)
#define GPIO_VCM_PWDN		ND_CS1N3_GPIO_102
	GPIO_GPS_TIMER_SYNC,
	GPIO_RF_PDET_EN,
	GPIO_LCD_RESET_N | MFP_PULL_FLOAT,
	GPIO_LED_B_CTRL | MFP_PULL_FLOAT | MFP_LPM_FLOAT,
	GPIO_LED_R_CTRL | MFP_PULL_FLOAT | MFP_LPM_FLOAT,
	GPIO_LED_G_CTRL | MFP_PULL_FLOAT | MFP_LPM_FLOAT,
	GPIO_GPS_LDO_EN,
	GPIO_VCM_PWDN,
	/* SM_RDY pin Low for download mode, High for normal boot */
	SM_RDY_GPIO_3 | MFP_PULL_HIGH,

	ND_IO15_ND_DAT15 | MFP_LPM_FLOAT,
	ND_IO14_ND_DAT14 | MFP_LPM_FLOAT,
	ND_IO13_ND_DAT13 | MFP_LPM_FLOAT,
	ND_IO12_ND_DAT12 | MFP_LPM_FLOAT,
	ND_IO11_ND_DAT11 | MFP_LPM_FLOAT,
	ND_IO10_ND_DAT10 | MFP_LPM_FLOAT,
	ND_IO9_ND_DAT9 | MFP_LPM_FLOAT,
	ND_IO8_ND_DAT8 | MFP_LPM_FLOAT,
	ND_nCS0_SM_nCS2 | MFP_PULL_LOW | MFP_LPM_FLOAT,
	DF_ALE_SM_WEn_ND_ALE | MFP_LPM_FLOAT,
	DF_WEn_DF_WEn | MFP_LPM_FLOAT,
	DF_REn_DF_REn | MFP_LPM_FLOAT,
	DF_RDY0_DF_RDY0 | MFP_LPM_FLOAT,
};

static unsigned long dkb1v0_pin_config[] __initdata = {
#define GPIO091_GPIO_GYRO_INT		(GPIO091_GPIO_91 | MFP_PULL_LOW)
#define GPIO092_GPIO_G_INT		(GPIO092_GPIO_92 | MFP_PULL_LOW)
#define GPIO093_GPIO_MAG_INT		(GPIO093_GPIO_93 | MFP_PULL_LOW)
#define GPIO098_GPIO_PRESURE_DRDY	GPIO098_GPIO_98
	GPIO091_GPIO_GYRO_INT,
	GPIO092_GPIO_G_INT,
	GPIO093_GPIO_MAG_INT,   /* sensor func not ready, for power, just set */
	GPIO098_GPIO_PRESURE_DRDY,
};

static unsigned long dkb1v1_pin_config[] __initdata = {
#define GPIO091_GPIO_MMC1_CD		(GPIO091_GPIO_91 | MFP_PULL_HIGH)
#define GPIO092_GPIO_CTL_SLP		GPIO092_GPIO_92
#define GPIO093_GPIO_FB_BIN		(GPIO093_GPIO_93 | MFP_PULL_FLOAT)
#define GPIO098_GPIO_FG_INT		(GPIO098_GPIO_98 | MFP_PULL_HIGH)
	GPIO091_GPIO_MMC1_CD,
	GPIO092_GPIO_CTL_SLP,
	GPIO093_GPIO_FB_BIN,
	GPIO098_GPIO_FG_INT,
};

static unsigned int emei_dkb_matrix_key_map[] = {
	KEY(0, 0, KEY_BACKSPACE),
	KEY(0, 1, KEY_MENU),
	KEY(0, 2, KEY_CAMERA), /* 1st camera */

	KEY(1, 0, KEY_MENU),
	KEY(1, 1, KEY_HOME),
	KEY(1, 2, KEY_CAMERA), /* 2nd camera */

	KEY(2, 0, KEY_SEND),
	KEY(2, 1, KEY_SEARCH),
	KEY(2, 2, KEY_UNKNOWN), /* unused key */

	KEY(3, 0, KEY_VOLUMEUP),
	KEY(3, 1, KEY_VOLUMEDOWN),
	KEY(3, 2, KEY_UNKNOWN), /* unused key */
};

static struct pxa27x_keypad_platform_data emei_dkb_keypad_info __initdata = {
	.matrix_key_rows	= 4,
	.matrix_key_cols	= 3,
	.matrix_key_map		= emei_dkb_matrix_key_map,
	.matrix_key_map_size	= ARRAY_SIZE(emei_dkb_matrix_key_map),
	.debounce_interval	= 30,
};

#ifdef CONFIG_INPUT_KEYRESET
static struct keyreset_platform_data emei_dkb_panic_keys_pdata = {
	.keys_down = {
		KEY_VOLUMEUP,
		KEY_VOLUMEDOWN,
		0
	},
	.panic_before_reset = 1,
};

static struct platform_device emei_dkb_panic_keys_device = {
	.name = KEYRESET_NAME,
	.dev.platform_data = &emei_dkb_panic_keys_pdata,
};
#endif
static struct sram_platdata pxa988_asram_info = {
	.pool_name = "asram",
	.granularity = SRAM_GRANULARITY,
};

static struct platform_device pxa988_device_asoc_ssp1 = {
	.name		= "pxa-ssp-dai",
	.id		= 1,
};

static struct platform_device pxa988_device_asoc_gssp = {
	.name		= "pxa-ssp-dai",
	.id		= 4,
};

static struct platform_device pxa988_device_asoc_pcm = {
	.name		= "pxa-pcm-audio",
	.id		= -1,
};

static struct platform_device emei_dkb_audio_device = {
	.name	= "emei-dkb-hifi",
	.id	= -1,
};

/*
 * PMIC Regulator 88PM800
 * Power Supply ECOs:
 * ECO#6: V_2P8(LDO14) is wired to LDO7, so LDO14 should keep off
 */
static struct regulator_consumer_supply regulator_supplies[] = {
	/* BUCK power supplies: BUCK[1..5] */
	[PM800_ID_BUCK1] = REGULATOR_SUPPLY("vcc_main", NULL),
	[PM800_ID_BUCK2] = REGULATOR_SUPPLY("v_buck2", NULL),
	[PM800_ID_BUCK3] = REGULATOR_SUPPLY("v_buck3", NULL),
	[PM800_ID_BUCK4] = REGULATOR_SUPPLY("v_rf_vdd", NULL),
	[PM800_ID_BUCK5] = REGULATOR_SUPPLY("v_rf_pa", NULL),
	/* LDO power supplies: LDO[1..19] */
	[PM800_ID_LDO1]  = REGULATOR_SUPPLY("v_gps_1v1", NULL),
	[PM800_ID_LDO2]  = REGULATOR_SUPPLY("v_micbias", NULL),
	[PM800_ID_LDO3]  = REGULATOR_SUPPLY("v_ramp", NULL),
	[PM800_ID_LDO4]  = REGULATOR_SUPPLY("v_usim1", NULL),
	[PM800_ID_LDO5]  = REGULATOR_SUPPLY("v_ldo5", NULL),
	[PM800_ID_LDO6]  = REGULATOR_SUPPLY("v_wib_3v3", NULL),
	[PM800_ID_LDO7]  = REGULATOR_SUPPLY("v_vctcxo"/*V_LDO7*/, NULL),
	[PM800_ID_LDO8]  = REGULATOR_SUPPLY("v_ldo8", NULL),
	[PM800_ID_LDO9]  = REGULATOR_SUPPLY("v_wib_1v8", NULL),
	[PM800_ID_LDO10] = REGULATOR_SUPPLY("v_usim2", NULL),
	[PM800_ID_LDO11] = REGULATOR_SUPPLY("v_sensor", NULL),
	[PM800_ID_LDO12] = REGULATOR_SUPPLY("vqmmc", "sdhci-pxav3.0"),
	[PM800_ID_LDO13] = REGULATOR_SUPPLY("vmmc", "sdhci-pxav3.0"),
	[PM800_ID_LDO14] = REGULATOR_SUPPLY("vmmc", "sdhci-pxa.2"),
	[PM800_ID_LDO15] = REGULATOR_SUPPLY("v_ldo15", NULL),
	[PM800_ID_LDO16] = REGULATOR_SUPPLY("v_cam_avdd", NULL),
	[PM800_ID_LDO17] = REGULATOR_SUPPLY("v_cam_af", NULL),
	[PM800_ID_LDO18] = REGULATOR_SUPPLY("v_ldo18", NULL),
	[PM800_ID_LDO19] = REGULATOR_SUPPLY("v_gps_3v", NULL),
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
	[PM800_ID_BUCK5] = REG_INIT(BUCK5,  600000, 3950000, 1, 0),
	/* LDO power supplies: LDO[1..19] */
	[PM800_ID_LDO1]  = REG_INIT(LDO1,   600000, 1500000, 0, 0),
	[PM800_ID_LDO2]  = REG_INIT(LDO2,  1700000, 2800000, 0, 1),
	[PM800_ID_LDO3]  = REG_INIT(LDO3,  1200000, 3300000, 1, 1),
	[PM800_ID_LDO4]  = REG_INIT(LDO4,  1200000, 3300000, 0, 0),
	[PM800_ID_LDO5]  = REG_INIT(LDO5,  1200000, 3300000, 1, 1),
	[PM800_ID_LDO6]  = REG_INIT(LDO6,  1200000, 3300000, 0, 0),
	[PM800_ID_LDO7]  = REG_INIT(LDO7,  1200000, 3300000, 1, 1),
	[PM800_ID_LDO8]  = REG_INIT(LDO8,  1200000, 3300000, 0, 1),
	[PM800_ID_LDO9]  = REG_INIT(LDO9,  1200000, 3300000, 0, 0),
	[PM800_ID_LDO10] = REG_INIT(LDO10, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO11] = REG_INIT(LDO11, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO12] = REG_INIT(LDO12, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO13] = REG_INIT(LDO13, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO14] = REG_INIT(LDO14, 1200000, 3300000, 1, 1),
	[PM800_ID_LDO15] = REG_INIT(LDO15, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO16] = REG_INIT(LDO16, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO17] = REG_INIT(LDO17, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO18] = REG_INIT(LDO18, 1700000, 3300000, 1, 1),
	[PM800_ID_LDO19] = REG_INIT(LDO19, 1700000, 3300000, 1, 1),
};

static void mic_set_power(int on)
{
	struct regulator *v_ldo = regulator_get(NULL, "v_micbias");
	if (IS_ERR(v_ldo)) {
		v_ldo = NULL;
		pr_err("Get regulator error\n");
		return;
	}
	if (on)
		regulator_enable(v_ldo);
	else
		regulator_disable(v_ldo);

	regulator_put(v_ldo);
	v_ldo = NULL;
}

#ifdef CONFIG_RTC_DRV_SA1100
static int sync_time_to_soc(unsigned int ticks)
{
	RCNR = ticks;
	return 0;
}
#endif

static struct pm80x_usb_pdata pm80x_usb = {
	.vbus_gpio = PM800_GPIO2,
	.id_gpadc = PM800_NO_GPADC,
};


static struct pm80x_rtc_pdata pm80x_rtc = {
	.vrtc	= 1,
#ifdef CONFIG_RTC_DRV_SA1100
	.sync	= sync_time_to_soc,
#endif
};

static struct pm80x_dvc_pdata pm80x_dvc = {
	.dvc1		= MFP_PIN_GPIO43,
	.dvc2		= MFP_PIN_GPIO44,
	.gpio_dvc	= 1,
};

static struct pm80x_bat_pdata pm80x_bat = {
};

static int pm800_plat_config(struct pm80x_chip *chip,
				struct pm80x_platform_data *pdata)
{
	if (!chip || !pdata || !chip->regmap || !chip->subchip
	    || !chip->subchip->regmap_power) {
		pr_err("%s:chip or pdata is not availiable!\n", __func__);
		return -EINVAL;
	}

	/* RESET_OUTn, RTC_RESET_MODE =0 */
	regmap_write(chip->regmap, PM800_RTC_MISC1, 0xb0);

	/* Set internal digital sleep voltage as 0.9V */
	regmap_write(chip->regmap, PM800_LOW_POWER1, 0xf0);
	/*
	 * Enable 32Khz-out-3 low jitter XO_LJ = 1
	 * enable DVC for internal digital circuitry
	 * */
	regmap_write(chip->regmap, PM800_LOW_POWER2, 0x60);

	/* Enabele LDO and BUCK clock gating in lpm */
	regmap_write(chip->regmap, PM800_LOW_POWER_CONFIG3, 0x80);
	/* Enable reference group sleep mode */
	regmap_write(chip->regmap, PM800_LOW_POWER_CONFIG4, 0x80);

	/* Enable 32Khz-out-from XO 1, 2, 3 all enabled */
	regmap_write(chip->regmap, PM800_RTC_MISC2, 0x2a);

	/* Enable voltage change in pmic, POWER_HOLD = 1 */
	regmap_write(chip->regmap, PM800_WAKEUP1, 0x80);

	/* Enable GPADC sleep mode */
	regmap_write(chip->subchip->regmap_gpadc,
		     PM800_GPADC_MISC_CONFIG2, 0x71);
	/* Enlarge GPADC off slots */
	regmap_write(chip->subchip->regmap_gpadc, 0x08, 0x0f);

	/* Set buck1 sleep mode as 0.8V */
	regmap_write(chip->subchip->regmap_power, PM800_SLEEP_BUCK1, 0x10);
	/* Enable buck sleep mode */
	regmap_write(chip->subchip->regmap_power, PM800_BUCK_SLP1, 0xaa);
	regmap_write(chip->subchip->regmap_power, PM800_BUCK_SLP2, 0x2);

	/*
	 * Set buck4 as 2Mhz
	 *  base page:reg 0xd0.7 = 1
	 *            reg 0x50 = 0xc0
	 *            reg 0x52 OSC_DIV_SEL[3] = 1,
	 *            reg 0x53 OSC_2X_EN[3] = 0
	 * buck4 voltage will be set by CP
	 */
	regmap_read(chip->regmap, PM800_RTC_CONTROL, &data);
	data |= (1 << 7);
	regmap_write(chip->regmap, PM800_RTC_CONTROL, data);

	data = 0xc0;
	regmap_write(chip->regmap, OSC_CNTRL1, data);

	regmap_read(chip->regmap, OSC_CNTRL3, &data);
	data |= (1 << 4);
	regmap_write(chip->regmap, OSC_CNTRL3, data);

	if (cpu_is_pxa988()) {
		/* Set the 4 regs of buck4 as 1.8v */
		regmap_write(chip->subchip->regmap_power,
			     PM800_BUCK4, 0x54);
		regmap_write(chip->subchip->regmap_power,
			     PM800_BUCK4_1, 0x54);
		regmap_write(chip->subchip->regmap_power,
			     PM800_BUCK4_2, 0x54);
		regmap_write(chip->subchip->regmap_power,
			     PM800_BUCK4_3, 0x54);
	}

	if (cpu_is_pxa986()) {
		/* Set the 4 regs of buck4 as 1.85v */
		regmap_write(chip->subchip->regmap_power,
			     PM800_BUCK4, 0x55);
		regmap_write(chip->subchip->regmap_power,
			     PM800_BUCK4_1, 0x55);
		regmap_write(chip->subchip->regmap_power,
			     PM800_BUCK4_2, 0x55);
		regmap_write(chip->subchip->regmap_power,
			     PM800_BUCK4_3, 0x55);
	}

	/*
	 * Set ldo5 as 3.3V in active and sleep mode
	 * for pxa986
	 */
	if (cpu_is_pxa986()) {
		regmap_read(chip->subchip->regmap_power,
			    PM800_LDO5, &data);
		data |= 0xff;
		regmap_write(chip->subchip->regmap_power,
			     PM800_LDO5, data);
	}

	/* Set LDO19 as 1.8v for GPS */
	if (board_id == VER_1V1) {
		regmap_read(chip->subchip->regmap_power,
			    PM800_LDO19, &data);
		data |= 0x1;
		regmap_write(chip->subchip->regmap_power,
			    PM800_LDO19, data);
	}

	/* BUCK enable 0x50, BUCK1, 2, 3, 4 */
	regmap_write(chip->subchip->regmap_power, 0x50, 0x0f);
	/* LDO enable 0x51, 0x52, 0x53, LDO1, 3, 5, 7, 8, 15 */
	regmap_write(chip->subchip->regmap_power, 0x51, 0xd4);
	regmap_write(chip->subchip->regmap_power, 0x52, 0x60);
	regmap_write(chip->subchip->regmap_power, 0x53, 0x06);

	return 0;
}

static struct pm80x_headset_pdata pm80x_headset = {
	.headset_flag = 1,
	.mic_set_power = mic_set_power,
	.hook_press_th = 60,
	.vol_up_press_th = 250,
	.vol_down_press_th = 500,
	.mic_det_th = 600,
	.press_release_th = 600,
};

static struct pm80x_vibrator_pdata vibrator_pdata = {
	.min_timeout = 10,
};

static struct pm80x_platform_data pm800_info = {
	.headset = &pm80x_headset,
	.power_page_addr = 0x31,	/* POWER */
	.gpadc_page_addr = 0x32,	/* GPADC */
	.test_page_addr = 0x37,		/* TEST */
	.irq_mode = 0,	/* 0: clear IRQ by read */

	.num_regulators = ARRAY_SIZE(pm800_regulator_data),
	.regulator = pm800_regulator_data,
	.vibrator = &vibrator_pdata,
	.rtc = &pm80x_rtc,
	.dvc = &pm80x_dvc,
	.bat = &pm80x_bat,
	.usb = &pm80x_usb,
	.plat_config = pm800_plat_config,
};

static struct pm80x_platform_data pm805_info = {
	.irq_mode = 0,
};

#if defined(CONFIG_TOUCHSCREEN_FT5306)
static int ft5306_touch_io_power_onoff(struct device *dev, int on)
{
	static struct regulator *v_ldo8;

	if (!v_ldo8) {
		v_ldo8 = regulator_get(dev, "v_ldo8");
		if (IS_ERR(v_ldo8)) {
			v_ldo8 = NULL;
			pr_err("%s: enable v_ldo8 for touch fail!\n", __func__);
			return -EIO;
		}
	}

	if (on) {
		regulator_set_voltage(v_ldo8, 3100000, 3100000);
		regulator_enable(v_ldo8);
	} else
		regulator_disable(v_ldo8);

	msleep(100);
	return 0;
}

static void ft5306_touch_reset(void)
{
	unsigned int touch_reset;

	touch_reset = mfp_to_gpio(GPIO016_GPIO_TP_RESET);

	if (gpio_request(touch_reset, "ft5306_reset")) {
		pr_err("Failed to request GPIO for ft5306_reset pin!\n");
		goto out;
	}

	gpio_direction_output(touch_reset, 1);
	msleep(5);
	gpio_direction_output(touch_reset, 0);
	msleep(5);
	gpio_direction_output(touch_reset, 1);
	msleep(300);
	printk(KERN_INFO "ft5306_touch reset done.\n");
	gpio_free(touch_reset);
out:
	return;
}

#define VIRT_KEYS(x...)  __stringify(x)
static ssize_t virtual_keys_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
		VIRT_KEYS(EV_KEY) ":" VIRT_KEYS(KEY_MENU)
		":67:1010:135:100" ":"
		VIRT_KEYS(EV_KEY) ":" VIRT_KEYS(KEY_HOMEPAGE)
		":202:1010:135:100" ":"
		VIRT_KEYS(EV_KEY) ":" VIRT_KEYS(KEY_SEARCH)
		":338:1010:135:100" ":"
		VIRT_KEYS(EV_KEY) ":" VIRT_KEYS(KEY_BACK)
		":472:1010:135:100\n");
}

static struct kobj_attribute virtual_keys_attr = {
	.attr = {
		.name = "virtualkeys.ft5306-ts",
		.mode = S_IRUGO,
	},
	.show = &virtual_keys_show,
};

static struct attribute *props_attrs[] = {
	&virtual_keys_attr.attr,
	NULL
};

static struct attribute_group props_attr_group = {
	.attrs = props_attrs,
};

static int ft5306_set_virtual_key(struct input_dev *input_dev)
{
	struct kobject *props_kobj;
	int ret = 0;

	props_kobj = kobject_create_and_add("board_properties", NULL);
	if (props_kobj)
		ret = sysfs_create_group(props_kobj, &props_attr_group);
	if (!props_kobj || ret)
		printk(KERN_ERR "failed to create board_properties\n");

	return 0;
}

static struct ft5306_touch_platform_data ft5306_touch_data = {
	.power = ft5306_touch_io_power_onoff,
	.reset = ft5306_touch_reset,
	.abs_x_max = 540,
	.abs_y_max = 960,
	.set_virtual_key = ft5306_set_virtual_key,
};
#endif

static void pm800_dvctable_init(void)
{
	unsigned int *vol_table;
	/* dvc only support 4 lvl voltage*/
	unsigned int vol_tbsize = 4;
	unsigned int index, max_vl, lowest_rate;

	vol_table = kmalloc(vol_tbsize * sizeof(unsigned int), GFP_KERNEL);
	if (!vol_table) {
		pr_err("%s failed to malloc vol table!\n", __func__);
		return ;
	}

	max_vl = pxa988_get_vl_num();
	max_vl = (max_vl > 4) ? 4 : max_vl;
	for (index = 0; index < max_vl; index++)
		vol_table[index] = pxa988_get_vl(index) * 1000;

	lowest_rate = pxa988_get_vl(0);
	while (index < 4)
		vol_table[index++] = lowest_rate * 1000;

	pm80x_dvc.vol_val = vol_table;
	pm80x_dvc.size	= vol_tbsize;
	return ;
}
static int sensor_set_power(int on);
#if defined(CONFIG_INV_MPU_IIO)
static struct mpu_platform_data gyro_platform_data = {
	.int_config  = 0x10,
	.set_power	= sensor_set_power,
	.level_shifter = 0,
	.orientation = {  -1,  0,  0,
			   0,  -1,  0,
			   0,  0, 1 },
	.sec_slave_type = SECONDARY_SLAVE_TYPE_NONE,
	.key = { 0xec, 0x5c, 0xa6, 0x17, 0x54, 0x3, 0x42, 0x90, 0x74, 0x7e,
		0x3a, 0x6f, 0xc, 0x2c, 0xdd, 0xb },
};
static struct mpu_platform_data inv_mpu_mmc328x_data = {
	.orientation = {
				0, -1, 0,
				1, 0, 0,
				0, 0, -1 },
};
#endif

#if defined(CONFIG_SENSORS_CWMI) || defined(CONFIG_SENSORS_CWGD) \
	|| defined(CONFIG_SENSORS_ISL29043) \
	|| defined(CONFIG_SENSORS_LPS331AP) \
	|| defined(CONFIG_INV_MPU_IIO)
static int sensor_set_power(int on)
{
	static struct regulator *v_sensor;

	if (!v_sensor) {
		v_sensor = regulator_get(NULL, "v_sensor");
		if (IS_ERR(v_sensor)) {
			v_sensor = NULL;
			return -EIO;
		}
	}

	if (on) {
		regulator_set_voltage(v_sensor, 2800000, 2800000);
		regulator_enable(v_sensor);
	} else {
		regulator_disable(v_sensor);
	}
	msleep(100);
	return 0;
}
#endif

#if defined(CONFIG_SENSORS_ISL29043)
static struct isl29043_platform_data isl29043_plat_data = {
	.wakup_gpio = MFP_PIN_GPIO14,
	.set_power = sensor_set_power,
};
#endif

#if defined(CONFIG_SENSORS_LPS331AP)
struct lps331ap_prs_platform_data lps331ap_plat_data = {
	.power_on = sensor_set_power,
	.poll_interval = 1000,
};
#endif

#ifdef CONFIG_CHARGER_ISL9226
static struct isl9226_charger_pdata isl9226_data = {
	.default_input_current = 95,	/*charge current if USB enum fail: mA*/
	.usb_input_current = 475,	/*USB charger current: mA*/
	.ac_input_current = 950,	/*AC charger current: mA*/
	.eoc_current = 100,		/*end of charge current: mA*/
	.prechg_current = 120,		/*pre-charge current: mA */
	.prechg_voltage = 3000,		/*pre-charge voltage: mV*/
};

struct platform_device isl9226_device = {
	.name = "isl9226-charger",
	.id	= -1,
};
#endif

static struct i2c_board_info emeidkb_pwr_i2c_info[] = {
	{
		.type		= "88PM800",
		.addr		= 0x30,
		.irq		= IRQ_PXA988_PMIC,
		.platform_data	= &pm800_info,
	},
	{
		.type = "88PM805",
		.addr = 0x38,
		.irq  = MMP_GPIO_TO_IRQ(mfp_to_gpio(GPIO124_GPIO_124)),
		.platform_data = &pm805_info,
	},

#if defined(CONFIG_CHARGER_ISL9226)
	{
		.type		= "isl9226",
		.addr		= 0x59,
		.platform_data	= &isl9226_data,
	},
#endif

#if defined(CONFIG_TOUCHSCREEN_FT5306)
	{
		.type = "ft5306_touch",
		.addr = 0x39,
		.irq  = MMP_GPIO_TO_IRQ(mfp_to_gpio(GPIO017_GPIO_TP_INT)),
		.platform_data	= &ft5306_touch_data,
	},
#endif
};

/* CCIC pin definition */
#define GPIO079_GPIO_CAM_STROBE		GPIO079_GPIO_79
#define GPIO080_GPIO_CAM_PD_MAIN	GPIO080_GPIO_80
#define GPIO081_GPIO_CAM_RST_MAIN	GPIO081_GPIO_81
#define GPIO082_GPIO_CAM_PD_SUB		GPIO082_GPIO_82
#define GPIO083_GPIO_CAM_RST_SUB	GPIO083_GPIO_83

static unsigned long ccic_pin_config[] = {
	GPIO067_CCIC_IN7 | MFP_LPM_DRIVE_LOW,	/* CAM_DATA<9> */
	GPIO068_CCIC_IN6 | MFP_LPM_DRIVE_LOW,	/* CAM_DATA<8> */
	GPIO069_CCIC_IN5 | MFP_LPM_DRIVE_LOW,	/* CAM_DATA<7> */
	GPIO070_CCIC_IN4 | MFP_LPM_DRIVE_LOW,	/* CAM_DATA<6> */
	GPIO071_CCIC_IN3 | MFP_LPM_DRIVE_LOW,	/* CAM_DATA<5> */
	GPIO072_CCIC_IN2 | MFP_LPM_DRIVE_LOW,	/* CAM_DATA<4> */
	GPIO073_CCIC_IN1 | MFP_LPM_DRIVE_LOW,	/* CAM_DATA<3> */
	GPIO074_CCIC_IN0 | MFP_LPM_DRIVE_LOW,	/* CAM_DATA<2> */
	GPIO075_CAM_HSYNC | MFP_LPM_DRIVE_LOW,	/* CAM_HSYNC */
	GPIO076_CAM_VSYNC | MFP_LPM_DRIVE_LOW,	/* CAM_VSYNC */
	GPIO077_CAM_MCLK | MFP_LPM_DRIVE_LOW,	/* CAM_MCLK */
	GPIO078_CAM_PCLK | MFP_LPM_DRIVE_LOW,	/* CAM_PCLK */

	GPIO079_GPIO_CAM_STROBE,
	GPIO080_GPIO_CAM_PD_MAIN | MFP_PULL_FLOAT,
	GPIO081_GPIO_CAM_RST_MAIN | MFP_PULL_FLOAT,
	GPIO082_GPIO_CAM_PD_SUB | MFP_PULL_FLOAT,
	GPIO083_GPIO_CAM_RST_SUB | MFP_PULL_FLOAT,

};

static void pxa988_cam_pin_init(struct device *dev, int on)
{
	unsigned int pin_index = 0;

	if (on)
		mfp_config(ARRAY_AND_SIZE(ccic_pin_config));
	else {
		for (pin_index = MFP_PIN_GPIO67;
				pin_index <= MFP_PIN_GPIO80; pin_index++)
			mfp_write(pin_index, 0xd0c0);
	}
}

#ifdef CONFIG_VIDEO_MVISP_OV882X
static int ov8825_sensor_power_on(int on)
{
	static struct regulator *af_vcc;
	static struct regulator *avdd;
	int rst  = mfp_to_gpio(GPIO081_GPIO_CAM_RST_MAIN);
	int pwdn = mfp_to_gpio(GPIO080_GPIO_CAM_PD_MAIN);
	int ret = 0;

	if (gpio_request(pwdn, "CAM_ENABLE_LOW")) {
		ret = -EIO;
		goto out;
	}

	if (gpio_request(rst, "CAM_RESET_LOW")) {
		ret = -EIO;
		goto out_rst;
	}

	if (!af_vcc) {
		af_vcc = regulator_get(NULL, "v_cam_af");
		if (IS_ERR(af_vcc)) {
			ret = -EIO;
			goto out_af_vcc;
		}
	}

	if (!avdd) {
		avdd = regulator_get(NULL, "v_cam_avdd");
		if (IS_ERR(avdd)) {
			ret =  -EIO;
			goto out_avdd;
		}
	}

	/* Enable voltage for camera sensor OV8825 */
	if (on) {
		regulator_set_voltage(af_vcc, 2800000, 2800000);
		regulator_enable(af_vcc);
		regulator_set_voltage(avdd, 2800000, 2800000);
		regulator_enable(avdd);
		mdelay(5);
		/* enable the sensor now*/
		gpio_direction_output(pwdn, 1);
		mdelay(1);
		gpio_direction_output(rst, 1);
		mdelay(20);
	} else {
		gpio_direction_output(rst, 0);

		regulator_disable(avdd);
		regulator_disable(af_vcc);

		gpio_direction_output(pwdn, 0);
	}

	gpio_free(rst);
	gpio_free(pwdn);
	return 0;

out_avdd:
	avdd = NULL;
	regulator_put(af_vcc);
out_af_vcc:
	af_vcc = NULL;
	gpio_free(rst);
out_rst:
	gpio_free(pwdn);
out:
	return ret;
}

static struct sensor_platform_data ov8825_platdata = {
	.power_on = ov8825_sensor_power_on,
	.flash_enable = mfp_to_gpio(GPIO020_GPIO_FLASH_EN),
};

static struct i2c_board_info ov8825_info = {
	.type = "ov8825",
	.addr = 0x36,
	.platform_data = &ov8825_platdata,
};

static struct mvisp_subdev_i2c_board_info ov8825_isp_info[] = {
	[0] = {
		.board_info = &ov8825_info,
		.i2c_adapter_id = 0,
	},
	[1] = {
		.board_info = NULL,
		.i2c_adapter_id = 0,
	},
};

static struct mvisp_v4l2_subdevs_group dxoisp_subdevs_group[] = {
	[0] = {
		.i2c_board_info = ov8825_isp_info,
		.if_type = ISP_INTERFACE_CCIC_1,
	},
	[1] = {
		.i2c_board_info = NULL,
		.if_type = 0,
	},
};
#endif
#ifdef CONFIG_VIDEO_MVISP_SENSOR
static struct sensor_power_data sensor_pwd_id[] = {
	{"lsi3h5", mfp_to_gpio(GPIO081_GPIO_CAM_RST_MAIN),
		mfp_to_gpio(GPIO080_GPIO_CAM_PD_MAIN),
		 1, 1, "v_cam_af", "v_cam_avdd", 2800000, 2800000},
	{"ov5647", mfp_to_gpio(GPIO081_GPIO_CAM_RST_MAIN),
		mfp_to_gpio(GPIO080_GPIO_CAM_PD_MAIN),
		1, 0, NULL, "v_cam_avdd", 0, 2800000},
	{"ov8825", mfp_to_gpio(GPIO081_GPIO_CAM_RST_MAIN),
		mfp_to_gpio(GPIO080_GPIO_CAM_PD_MAIN),
		1, 1, "v_cam_af", "v_cam_avdd", 2800000, 2800000},
	{"ov8820", mfp_to_gpio(GPIO081_GPIO_CAM_RST_MAIN),
		 mfp_to_gpio(GPIO080_GPIO_CAM_PD_MAIN),
		1, 1, "v_cam_af", "v_cam_avdd", 2800000, 2800000},
};
static struct sensor_type sensor_id[] = {
	{REG_PIDH_VALUE_LSI, "lsi3h5", REG_NUM_2,
		{REG_PIDH_LSI, REG_PIDM_LSI, REG_PIDL_LSI},
		{REG_PIDH_VALUE_LSI, REG_PIDM_VALUE_LSI, REG_PIDL_VALUE} },
	{REG_PIDM_VALUE_5647, "ov5647", REG_NUM_2,
		{REG_PIDH, REG_PIDM, REG_PIDL},
		{REG_PIDH_VALUE_5647, REG_PIDM_VALUE_5647, REG_PIDL_VALUE} },
	{REG_PIDM_VALUE_8825, "ov8825", REG_NUM_2,
		{REG_PIDH, REG_PIDM, REG_PIDL},
		{REG_PIDH_VALUE, REG_PIDM_VALUE_8825, REG_PIDL_VALUE} },
	{REG_PIDM_VALUE_8820, "ov8820", REG_NUM_2,
		{REG_PIDH, REG_PIDM, REG_PIDL},
		{REG_PIDH_VALUE, REG_PIDM_VALUE_8820, REG_PIDL_VALUE} },
};
static struct sensor_platform_data sensor_platdata = {
	.sensor_pwd = sensor_pwd_id,
	.sensor_cid = sensor_id,
	.sensor_num = ARRAY_SIZE(sensor_id),
	.flash_enable = mfp_to_gpio(GPIO020_GPIO_FLASH_EN),
	.chip_ident_id = chip_ident_id,
};
static struct i2c_board_info ov882x_info = {
	.type = "sensor",
	.addr = 0x36,
	.platform_data = &sensor_platdata,
};
static struct i2c_board_info ov5647_info = {
	.type = "sensor",
	.addr = 0x36,
	.platform_data = &sensor_platdata,
};
static struct i2c_board_info lsi3h5_info = {
	.type = "sensor",
	.addr = 0x2D,
	.platform_data = &sensor_platdata,
};

static struct mvisp_subdev_i2c_board_info sensor_isp_info[] = {
	{
		.board_info = &lsi3h5_info,
		.i2c_adapter_id = 0,
	},
	{
		.board_info = &ov882x_info,
		.i2c_adapter_id = 0,
	},
	{
		.board_info = &ov5647_info,
		.i2c_adapter_id = 0,
	},
};
#ifdef CONFIG_VIDEO_VCM_DW9714L
static struct vcm_platform_data dw9714l_platdata = {
	.power_domain = "v_cam_af",
	.pwdn_gpio = mfp_to_gpio(GPIO080_GPIO_CAM_PD_MAIN),
};

static struct i2c_board_info dw9714l_info = {
	.type = "dw9714l",
	.addr = 0x0c,
	.platform_data = &dw9714l_platdata,
};
#endif
#ifdef CONFIG_VIDEO_VCM_AK7343
static struct vcm_platform_data ak7343_platdata = {
	.power_domain = "v_cam_af",
	.pwdn_gpio = mfp_to_gpio(GPIO080_GPIO_CAM_PD_MAIN),
};

static struct i2c_board_info ak7343_info = {
	.type = "ak7343",
	.addr = 0x0c,
	.platform_data = &ak7343_platdata,
};
#endif
static struct mvisp_subdev_i2c_board_info vcm_isp_info[] = {
#ifdef CONFIG_VIDEO_VCM_DW9714L
	{
		.board_info = &dw9714l_info,
		.i2c_adapter_id = 0,
	},
#endif
#ifdef CONFIG_VIDEO_VCM_AK7343
	{
		.board_info = &ak7343_info,
		.i2c_adapter_id = 0,
	},
#endif
	{
		.board_info = NULL,
		.i2c_adapter_id = 0,
	},
};
static struct mvisp_v4l2_subdevs_group dxoisp_subdevs_group[] = {
	{
		.name = "sensor group",
		.i2c_board_info = sensor_isp_info,
		.if_type = ISP_INTERFACE_CCIC_1,
	},
#ifdef CONFIG_VIDEO_VCM_DW9714L
	{
		.name = "vcm group",
		.i2c_board_info = vcm_isp_info,
		.if_type = 0,
	},
#endif
	{
		.name = "",
		.i2c_board_info = NULL,
		.if_type = 0,
	},
};
#endif

#if defined(CONFIG_VIDEO_MVISP) && defined(CONFIG_UIO_MVISP)
#if !defined(CONFIG_VIDEO_MVISP_OV882X) && !defined(CONFIG_VIDEO_MVISP_SENSOR)
static struct mvisp_v4l2_subdevs_group dxoisp_subdevs_group[] = {
	[0] = {
		.name = "",
		.i2c_board_info = NULL,
		.if_type = 0,
	},
};
#endif

static char *pxa988_isp_ccic_clk_name[] = {
	[0] = "ISP-CLK",
	[1] = "CCICPHYCLK",
	[2] = "CCICFUNCLK",
};

static struct mvisp_platform_data pxa988_dxoisp_pdata = {
	.isp_clknum       = 1,
	.ccic_clknum      = 2,
	.clkname          = pxa988_isp_ccic_clk_name,
	.mvisp_reset      = pxa988_isp_reset_hw,
	.isp_pwr_ctrl     = pxa988_isp_power_control,
	.init_pin         = pxa988_cam_pin_init,
	.subdev_group     = dxoisp_subdevs_group,
	.ccic_dummy_ena   = false,
	.ispdma_dummy_ena = false,
};

static void __init pxa988_init_dxoisp(void)
{
	pxa_register_uio_mvisp();
	pxa988_register_dxoisp(&pxa988_dxoisp_pdata);
}
#endif

static struct i2c_board_info emeidkb_i2c_info[] = {
};

static struct i2c_board_info emeidkb_i2c2_info[] = {
#if defined(CONFIG_SENSORS_ISL29043)
	{
		.type		= "isl29043",
		.addr		= 0x44,
		.irq = MMP_GPIO_TO_IRQ(mfp_to_gpio(GPIO014_GPIO_PROX_IRQ)),
		.platform_data	= &isl29043_plat_data,
	},
#endif
#if defined(CONFIG_SENSORS_LPS331AP)
	{
		.type       = "lps331ap",
		.addr       = 0x5c,
		.platform_data  = &lps331ap_plat_data,
	},
#endif
#if defined(CONFIG_INV_MPU_IIO)
	{
		.type       = "mpu6050",
		.addr       = 0x68,
		.irq = MMP_GPIO_TO_IRQ(mfp_to_gpio(GPIO095_GPIO_MOTION_INT)),
		.platform_data = &gyro_platform_data,
	},
	{
		.type       = "mmc328x",
		.addr       = 0x30,
		.platform_data = &inv_mpu_mmc328x_data,
	},
#endif
};

/*
 * workaround for reset i2c bus
 * i2c0: GPIO53 -> SCL, GPIO54 -> SDA,
 * i2c1: GPIO87 -> SCL, GPIO88 -> SDA,
 */
static void i2c_pxa_bus_reset(int i2c_adap_id)
{
	unsigned long mfp_pin[2];
	int ccnt;
	unsigned long scl, sda;

	unsigned long i2c0_mfps[] = {
		GPIO053_GPIO_53,		/* SCL */
		GPIO054_GPIO_54,		/* SDA */
	};

	unsigned long i2c1_mfps[] = {
		GPIO087_GPIO_87,		/* SCL */
		GPIO088_GPIO_88,		/* SDA */
	};
	if (i2c_adap_id == 0) {
		scl = MFP_PIN_GPIO53;
		sda = MFP_PIN_GPIO54;
	} else if (i2c_adap_id == 1) {
		scl = MFP_PIN_GPIO87;
		sda = MFP_PIN_GPIO88;
	} else {
		pr_err("i2c bus num error!\n");
		return;
	}
	if (gpio_request(scl, "SCL")) {
		pr_err("Failed to request GPIO for SCL pin!\n");
		goto out0;
	}
	if (gpio_request(sda, "SDA")) {
		pr_err("Failed to request GPIO for SDA pin!\n");
		goto out_sda0;
	}
	/* set mfp pins to gpios */
	mfp_pin[0] = mfp_read(scl);
	mfp_pin[1] = mfp_read(sda);
	if (i2c_adap_id == 0)
		mfp_config(ARRAY_AND_SIZE(i2c0_mfps));
	if (i2c_adap_id == 1)
		mfp_config(ARRAY_AND_SIZE(i2c1_mfps));

	gpio_direction_input(sda);
	for (ccnt = 20; ccnt; ccnt--) {
		gpio_direction_output(scl, ccnt & 0x01);
		udelay(4);
	}
	gpio_direction_output(scl, 0);
	udelay(4);
	gpio_direction_output(sda, 0);
	udelay(4);
	/* stop signal */
	gpio_direction_output(scl, 1);
	udelay(4);
	gpio_direction_output(sda, 1);
	udelay(4);
	if (i2c_adap_id == 0) {
		mfp_write(MFP_PIN_GPIO53, mfp_pin[0]);
		mfp_write(MFP_PIN_GPIO54, mfp_pin[1]);
	}
	if (i2c_adap_id == 1) {
		mfp_write(MFP_PIN_GPIO87, mfp_pin[0]);
		mfp_write(MFP_PIN_GPIO88, mfp_pin[1]);
	}
	gpio_free(sda);
out_sda0:
	gpio_free(scl);
out0:
	return;
}

static struct i2c_pxa_platform_data emeidkb_ci2c_pdata = {
	.fast_mode		 = 1,
	/* ilcr:fs mode b17~9=0x23,about 390K, standard mode b8~0=0x9f,97K */
	.ilcr		= 0x082C469F,
	/* iwcr:b5~0=b01010 recommended value according to spec*/
	.iwcr		= 0x0000142A,
	.i2c_bus_reset		= i2c_pxa_bus_reset,
};

static struct i2c_pxa_platform_data emeidkb_ci2c2_pdata = {
	.fast_mode		 = 1,
	/* ilcr:fs mode b17~9=0x23,about 390K, standard mode b8~0=0x9f,97K */
	.ilcr		= 0x082C469F,
	/* iwcr:b5~0=b01010 recommended value according to spec*/
	.iwcr		= 0x0000142A,
	.i2c_bus_reset		= i2c_pxa_bus_reset,
};

static struct i2c_pxa_platform_data emeidkb_pwr_i2c_pdata = {
	.fast_mode		 = 1,
	/* ilcr:fs mode b17~9=0x23,about 390K, standard mode b8~0=0x9f,97K */
	.ilcr		= 0x082C469F,
	/* iwcr:b5~0=b01010 recommended value according to spec*/
	.iwcr		= 0x0000142A,
	.hardware_lock		= pxa988_ripc_lock,
	.hardware_unlock	= pxa988_ripc_unlock,
	.hardware_trylock	= pxa988_ripc_trylock,
};

static void emei_dkb_set_bl(int intensity)
{
	int gpio_bl, bl_level, p_num;
	/*
	 * FIXME
	 * the initial value of bl_level_last is the
	 * uboot backlight level, it should be aligned.
	 */
	static int bl_level_last = 17;

	gpio_bl = mfp_to_gpio(GPIO032_GPIO_LCD_BL);
	if (gpio_request(gpio_bl, "lcd backlight")) {
		pr_err("gpio %d request failed\n", gpio_bl);
		return;
	}

	/*
	 * Brightness is controlled by a series of pulses
	 * generated by gpio. It has 32 leves and level 1
	 * is the brightest. Pull low for 3ms makes
	 * backlight shutdown
	 */
	bl_level = (100 - intensity) * 32 / 100 + 1;

	if (bl_level == bl_level_last)
		goto set_bl_return;

	if (bl_level == 33) {
		/* shutdown backlight */
		gpio_direction_output(gpio_bl, 0);
		goto set_bl_return;
	}

	if (bl_level > bl_level_last)
		p_num = bl_level - bl_level_last;
	else
		p_num = bl_level + 32 - bl_level_last;

	while (p_num--) {
		gpio_direction_output(gpio_bl, 0);
		udelay(1);
		gpio_direction_output(gpio_bl, 1);
		udelay(1);
	}

set_bl_return:
	if (bl_level == 33)
		bl_level_last = 0;
	else
		bl_level_last = bl_level;
	gpio_free(gpio_bl);
	pr_debug("%s, intensity:%d\n", __func__, intensity);
}

static struct generic_bl_info emei_dkb_lcd_backlight_data = {
	.name = "emei-bl",
	.max_intensity = 100,
	.default_intensity = 50,
	.set_bl_intensity = emei_dkb_set_bl,
};

static struct platform_device emei_dkb_lcd_backlight_devices = {
	.name = "generic-bl",
	.dev = {
		.platform_data = &emei_dkb_lcd_backlight_data,
	},
};

#ifdef CONFIG_USB_MV_UDC
static char *pxa988_usb_clock_name[] = {
	[0] = "UDCCLK",
};

static struct mv_usb_platform_data emeidkb_usb_pdata = {
	.clknum		= 1,
	.clkname	= pxa988_usb_clock_name,
	.id		= PXA_USB_DEV_OTG,
	.extern_attr	= MV_USB_HAS_VBUS_DETECTION,
	.mode		= MV_USB_MODE_DEVICE,
	.phy_init	= pxa_usb_phy_init,
	.phy_deinit	= pxa_usb_phy_deinit,
};
#endif /* CONFIG_USB_MV_UDC */

#ifdef CONFIG_MMC_SDHCI_PXAV3
#define MFP_WIB_PDn		(GPIO007_GPIO_7 | MFP_PULL_FLOAT)
#define MFP_WIB_RESETn		(GPIO011_GPIO_11 | MFP_PULL_FLOAT)

static void emeidkb_sdcard_signal(unsigned int set)
{
	int vol = set;

	pxa988_aib_mmc1_iodomain(vol);
}

static struct wakeup_source wlan_dat1_wakeup;
static struct work_struct wlan_wk;

static void wlan_edge_wakeup(struct work_struct *work)
{
	/*
	 * it is handled in SDIO driver instead, so code not need now
	 * but temparally keep the code here,it may be used for debug
	 */
#if 0
	unsigned int sec = 3;

	/*
	 * Why use a workqueue to call this function?
	 *
	 * As now dat1_edge_wakeup is called just after CPU exist LPM,
	 * and if __pm_wakeup_event is called before syscore_resume,
	 * WARN_ON(timekeeping_suspended) will happen in ktime_get in
	 * /kernel/time/timekeeping.c
	 *
	 * So we create a workqueue to fix this issue
	 */
	__pm_wakeup_event(&wlan_dat1_wakeup, 1000 * sec);
	printk(KERN_INFO "SDIO wake up+++\n");
#endif
}

static void dat1_edge_wakeup(int irq, void *pRsv)
{
	queue_work(system_wq, &wlan_wk);
}

static struct gpio_edge_desc gpio_edge_sdio_dat1 = {
	.mfp = MFP_PIN_GPIO39,
	.gpio = mfp_to_gpio(MFP_PIN_GPIO39),
	/*
	 * SDIO Spec difine falling as SDIO interrupt, but set BOTH edge
	 * should be more safe to wake up.
	 */
	.type = MFP_LPM_EDGE_BOTH,
	.handler = dat1_edge_wakeup,
};

static void wlan_wakeup_init(void)
{
	 INIT_WORK(&wlan_wk, wlan_edge_wakeup);
	 wakeup_source_init(&wlan_dat1_wakeup,
		"wifi_hs_wakeups");
}

#ifdef CONFIG_SD8XXX_RFKILL
static void emeidkb_8787_set_power(unsigned int on)
{
	static struct regulator *wib_1v8;
	static struct regulator *wib_3v3;
	static int enabled;

	/* LDO9 = 1.8V/300mA Off by default; LDO6 = 2.8V/300mA Off by default */
	if (!wib_1v8) {
		wib_1v8 = regulator_get(NULL, "v_wib_1v8");
		if (IS_ERR(wib_1v8)) {
			wib_1v8 = NULL;
			printk(KERN_ERR "get v_wib_1v8 failed %s %d\n",
				__func__, __LINE__);
			return;
		}
	}

	if (!wib_3v3) {
		wib_3v3 = regulator_get(NULL, "v_wib_3v3");
		if (IS_ERR(wib_3v3)) {
			wib_3v3 = NULL;
			printk(KERN_ERR "get v_wib_3v3 failed %s %d\n",
				__func__, __LINE__);
			return;
		}
	}

	if (on && !enabled) {
		regulator_set_voltage(wib_1v8, 1800000, 1800000);
		regulator_enable(wib_1v8);
		regulator_set_voltage(wib_3v3, 2800000, 2800000);
		regulator_enable(wib_3v3);
		enabled = 1;

		/* Only when SD8787 are active (power on),
		 * it is meanful to enable the edge wakeup
		 */
		if (cpu_pxa98x_stepping() < PXA98X_Z3 || cpu_is_pxa986_z3())
			mmp_gpio_edge_add(&gpio_edge_sdio_dat1);
	}

	if (!on && enabled) {
		regulator_disable(wib_1v8);
		regulator_disable(wib_3v3);
		enabled = 0;

		if (cpu_pxa98x_stepping() < PXA98X_Z3 || cpu_is_pxa986_z3())
			mmp_gpio_edge_del(&gpio_edge_sdio_dat1);
	}
}
#endif

/* For emeiDKB, MMC1(SDH1) used for SD/MMC Card slot */
static struct sdhci_pxa_platdata pxa988_sdh_platdata_mmc1 = {
	.flags		= PXA_FLAG_ENABLE_CLOCK_GATING,
	.cd_type	 = PXA_SDHCI_CD_HOST,
	.clk_delay_cycles	= 0x1F,
	.host_caps_disable	= MMC_CAP_UHS_SDR104,
	.quirks			= SDHCI_QUIRK_INVERTED_WRITE_PROTECT,
	.signal_vol_change	= emeidkb_sdcard_signal,
};

/* For emeiDKB, MMC2(SDH2) used for WIB card */
static struct sdhci_pxa_platdata pxa988_sdh_platdata_mmc2 = {
	.flags          = PXA_FLAG_WAKEUP_HOST,
	.cd_type	 = PXA_SDHCI_CD_EXTERNAL,
	.pm_caps	= MMC_PM_KEEP_POWER,
};

/* For emeiDKB, MMC3(SDH3) used for eMMC */
static struct sdhci_pxa_platdata pxa988_sdh_platdata_mmc3 = {
	.flags		= PXA_FLAG_ENABLE_CLOCK_GATING
				| PXA_FLAG_SD_8_BIT_CAPABLE_SLOT,
	.cd_type	 = PXA_SDHCI_CD_PERMANENT,
	.clk_delay_cycles	= 0xF,
	.host_caps	= MMC_CAP_1_8V_DDR,
};

static void __init emeidkb_init_mmc(void)
{
#ifdef CONFIG_SD8XXX_RFKILL
	int WIB_PDn = mfp_to_gpio(MFP_WIB_PDn);
	int WIB_RESETn = mfp_to_gpio(MFP_WIB_RESETn);

	if (!gpio_request(WIB_PDn, "WIB_PDn")) {
		gpio_direction_output(WIB_PDn, 0);
		gpio_free(WIB_PDn);
	}

	if (!gpio_request(WIB_RESETn, "WIB_RSTn")) {
		gpio_direction_output(WIB_RESETn, 0);
		gpio_free(WIB_RESETn);
	}

	add_sd8x_rfkill_device(WIB_PDn, WIB_RESETn,
			&pxa988_sdh_platdata_mmc2.pmmc,
			emeidkb_8787_set_power);
#endif

	/*
	 * Note!!
	 *  The regulator can't be used here, as this is called in arch_init
	 */

	/* HW MMC3(sdh3) used for eMMC, and register first */
	pxa988_add_sdh(3, &pxa988_sdh_platdata_mmc3);

	/* HW MMC1(sdh1) used for SD/MMC card */
	if (board_id == VER_1V1) {
		pxa988_sdh_platdata_mmc1.cd_type = PXA_SDHCI_CD_GPIO;
		pxa988_sdh_platdata_mmc1.ext_cd_gpio =
				mfp_to_gpio(GPIO091_GPIO_91);
	}
	pxa988_add_sdh(1, &pxa988_sdh_platdata_mmc1);

	/* HW MMC2(sdh2) used for SDIO(WIFI/BT/FM module), and register last */
	pxa988_add_sdh(2, &pxa988_sdh_platdata_mmc2);
	wlan_wakeup_init();
}
#else
static void __init emeidkb_init_mmc(void)
{

}
#endif /* CONFIG_MMC_SDHCI_PXAV3 */

#if (defined CONFIG_CMMB)

#define CMMB_SPI_CLK		(GPIO033_SPI_DCLK | MFP_LPM_FLOAT)
#define CMMB_SPI_CSn		(GPIO034_GPIO_34 | MFP_PULL_FLOAT)
#define CMMB_SPI_DIN		(GPIO035_SPI_DIN | MFP_LPM_FLOAT)
#define CMMB_SPI_DOUT		(GPIO036_SPI_DOUT | MFP_LPM_FLOAT)
#define CMMB_POWER_EN		(GPIO018_GPIO_18 | MFP_PULL_FLOAT)
#define CMMB_POWER_RESETn	(GPIO019_GPIO_19 | MFP_PULL_FLOAT)
#define CMMB_IRQ_GPIO		GPIO013_GPIO_13
#define CMMB_EXT_CLK_EN	(GPIO090_CMMB_CLK | MFP_PULL_FLOAT)
#define CMMB_EXT_CLK_DIS	(GPIO090_GPIO_90 | MFP_PULL_FLOAT)

static unsigned long cmmb_pin_config[] = {
	CMMB_SPI_CLK,
	CMMB_SPI_CSn,
	CMMB_SPI_DIN,
	CMMB_SPI_DOUT,
	CMMB_POWER_EN,
	CMMB_POWER_RESETn,
	CMMB_IRQ_GPIO,
	CMMB_EXT_CLK_DIS,
};

static struct pxa2xx_spi_master pxa_ssp_master_info = {
	.num_chipselect = 1,
	.enable_dma = 1,
};

/*
 * FIXME:Need to fine tune the delay time for the below 3 functions
 * Maybe we can short the time
 */
static int cmmb_power_reset(void)
{
	int cmmb_rst;

	cmmb_rst = mfp_to_gpio(CMMB_POWER_RESETn);

	if (gpio_request(cmmb_rst, "cmmb rst")) {
		pr_warning("failed to request GPIO for CMMB RST\n");
		return -EIO;
	}

	gpio_direction_output(cmmb_rst, 0);
	msleep(100);

	/* get cmmb go out of reset state */
	gpio_direction_output(cmmb_rst, 1);
	gpio_free(cmmb_rst);

	return 0;
}

static int cmmb_power_on(void)
{
	int cmmb_en;

	cmmb_en = mfp_to_gpio(CMMB_POWER_EN);
	if (gpio_request(cmmb_en, "cmmb power")) {
		pr_warning("[ERROR] failed to request GPIO for CMMB POWER\n");
		return -EIO;
	}

	gpio_direction_output(cmmb_en, 0);
	msleep(100);

	gpio_direction_output(cmmb_en, 1);
	gpio_free(cmmb_en);

	msleep(100);

	cmmb_power_reset();

	return 0;
}

static int cmmb_power_off(void)
{
	int cmmb_en;

	cmmb_en = mfp_to_gpio(CMMB_POWER_EN);

	if (gpio_request(cmmb_en, "cmmb power")) {
		pr_warning("failed to request GPIO for CMMB POWER\n");
		return -EIO;
	}

	gpio_direction_output(cmmb_en, 0);
	gpio_free(cmmb_en);
	msleep(100);

	return 0;
}

/*
 * Add two functions: cmmb_cs_assert and cmmb_cs_deassert.
 * Provide the capbility that
 * cmmb driver can handle the SPI_CS by itself.
 */
static int cmmb_cs_assert(void)
{
	int cs;
	cs = mfp_to_gpio(CMMB_SPI_CSn);
	gpio_direction_output(cs, 0);
	return 0;
}

static int cmmb_cs_deassert(void)
{
	int cs;
	cs = mfp_to_gpio(CMMB_SPI_CSn);
	gpio_direction_output(cs, 1);
	return 0;
}

static struct cmmb_platform_data cmmb_info = {
	.power_on = cmmb_power_on,
	.power_off = cmmb_power_off,
	.power_reset = cmmb_power_reset,
	.cs_assert = cmmb_cs_assert,
	.cs_deassert = cmmb_cs_deassert,

	.gpio_power = mfp_to_gpio(CMMB_POWER_EN),
	.gpio_reset = mfp_to_gpio(CMMB_POWER_RESETn),
	.gpio_cs = mfp_to_gpio(CMMB_SPI_CSn),
	.gpio_defined = 1,
};

static void cmmb_dummy_cs(u32 cmd)
{
/*
 * Because in CMMB read/write,the max data size is more than 8kB
 * 8k = max data length per dma transfer for pxaxxx
 * But till now,The spi_read/write driver doesn't support muti DMA cycles
 *
 * Here the spi_read/write will not affect the SPI_CS,but provides
 * cs_assert and cs_deassert in the struct cmmb_platform_data
 *
 * And cmmb driver can/should control SPI_CS by itself
 */
}

/*
 * Init the pins used for CMMB module, make sure the pins' status is dedicate
 * after system reboot
 */
static void mfp_set_output(int mfp, int value)
{
	int err;
	int gpio = mfp_to_gpio(mfp);

	err = gpio_request(gpio, "tmp_pin");
	if (err) {
		/* Fail to request GPIO */
		pr_warning("[ERROR] CMMB failed to request gpio%d\n", gpio);
	} else {
		gpio_direction_output(gpio, !!value);
		gpio_free(gpio);
	}
}

static int cmmb_26M_output(int on)
{
	mfp_cfg_t cmmb_ext_clk_en = CMMB_EXT_CLK_EN;
	mfp_cfg_t cmmb_ext_clk_dis = CMMB_EXT_CLK_DIS;

	if (on)
		mfp_config(&cmmb_ext_clk_en, 1);
	else {
		mfp_config(&cmmb_ext_clk_dis, 1);
		mfp_set_output(cmmb_ext_clk_dis, 0);
	}

	return 0;
}

static void cmmb_pins_level_init(void)
{
	mfp_set_output(CMMB_POWER_RESETn, 1);
	mfp_set_output(CMMB_POWER_EN, 0);

	/* disable CMMB 26MHz output by default */
	cmmb_26M_output(0);
}

static struct pxa2xx_spi_chip cmmb_spi_chip = {
	.rx_threshold   = 1,
	.tx_threshold   = 1,
	.cs_control     = cmmb_dummy_cs,
};

/* bus_num must match id in pxa2xx_set_spi_info() call */
static struct spi_board_info spi_board_info[] __initdata = {
	{
		.modalias		= "cmmb_if",
		.platform_data	= &cmmb_info,
		.controller_data	= &cmmb_spi_chip,
		.irq		= MMP_GPIO_TO_IRQ(mfp_to_gpio(CMMB_IRQ_GPIO)),
		.max_speed_hz	= 8000000,
		.bus_num		= 1,
		.chip_select	= 0,
		.mode			= SPI_MODE_0,
	},
};

static void __init emeidkb_init_spi(void)
{
	int err;
	int cmmb_int, cmmb_cs;

	mfp_config(ARRAY_AND_SIZE(cmmb_pin_config));
	cmmb_pins_level_init();

	cmmb_cs = mfp_to_gpio(CMMB_SPI_CSn);
	err = gpio_request(cmmb_cs, "cmmb cs");
	if (err) {
		pr_warning("[ERROR] failed to request GPIO for CMMB CS\n");
		return;
	}
	gpio_direction_output(cmmb_cs, 1);

	cmmb_int = mfp_to_gpio(CMMB_IRQ_GPIO);

	err = gpio_request(cmmb_int, "cmmb irq");
	if (err) {
		pr_warning("[ERROR] failed to request GPIO for CMMB IRQ\n");
		return;
	}
	gpio_direction_input(cmmb_int);
	gpio_free(cmmb_int);

	pxa988_add_ssp(0);
	pxa988_add_spi(1, &pxa_ssp_master_info);
	if (spi_register_board_info(spi_board_info,
			ARRAY_SIZE(spi_board_info))) {
		pr_warning("[ERROR] failed to register spi device.\n");
		return;
	}
}
#endif /* defined CONFIG_CMMB */

#if defined(CONFIG_VIDEO_MMP)
static int pxa988_cam_clk_init(struct device *dev, int init)
{
	struct mmp_cam_pdata *data = dev->platform_data;

	if ((!data->clk_enabled) && init) {
		data->clk[0] = clk_get(dev, "CCICFUNCLK");
		if (IS_ERR(data->clk[0])) {
			dev_err(dev, "Could not get function clk\n");
			goto out_clk0;
		}
		data->clk[1] = clk_get(dev, "CCICAXICLK");
		if (IS_ERR(data->clk[1])) {
			dev_err(dev, "Could not get AXI clk\n");
			goto out_clk1;
		}
		data->clk[2] = clk_get(dev, "LCDCIHCLK");
		if (IS_ERR(data->clk[2])) {
			dev_err(dev, "Could not get lcd/ccic AHB clk\n");
			goto out_clk2;
		}
		if (data->bus_type == V4L2_MBUS_CSI2_LANES) {
			data->clk[3] = clk_get(dev, "CCICPHYCLK");
			if (IS_ERR(data->clk[3])) {
				dev_err(dev, "Could not get PHY clk\n");
				goto out_clk3;
			}
		}
		data->clk_enabled = 1;
		return 0;
	}

	if (!init && data->clk_enabled) {
		clk_put(data->clk[0]);
		clk_put(data->clk[1]);
		clk_put(data->clk[2]);
		if (data->bus_type == V4L2_MBUS_CSI2_LANES)
			clk_put(data->clk[3]);
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
out_clk3:
		clk_put(data->clk[0]);
		clk_put(data->clk[1]);
		clk_put(data->clk[2]);
		return PTR_ERR(data->clk[3]);
}

static void pxa988_cam_set_clk(struct device *dev, int on)
{
	struct mmp_cam_pdata *data = dev->platform_data;

	if (data->clk_enabled) {
		if (on == 1) {
			clk_enable(data->clk[1]);
			clk_enable(data->clk[0]);
			if (data->bus_type == V4L2_MBUS_CSI2_LANES)
				clk_enable(data->clk[3]);
			clk_enable(data->clk[2]);
		} else {
			clk_disable(data->clk[0]);
			if (data->bus_type == V4L2_MBUS_CSI2_LANES)
				clk_disable(data->clk[3]);
			clk_disable(data->clk[1]);
			clk_disable(data->clk[2]);
		}
	}
}

struct mmp_cam_pdata mv_cam_data = {
	.name = "EMEI",
	.clk_enabled = 0,
	.dma_burst = 64,
	.mipi_enabled = 0,
	.mclk_min = 24,
	.mclk_src = 3,
	.mclk_div = 13,
	.init_pin = pxa988_cam_pin_init,
	.init_clk = pxa988_cam_clk_init,
	.enable_clk = pxa988_cam_set_clk,
#if defined(CONFIG_SOC_CAMERA_OV5640) || defined(CONFIG_SOC_CAMERA_OV5640_ECS)
	.bus_type = V4L2_MBUS_CSI2_LANES,
	.dphy = {0x0D06, 0x33, 0x0900},
	.lane = 2,
#endif
};
#endif

static struct i2c_board_info dkb_i2c_camera[] = {
	{
		I2C_BOARD_INFO("ov2659", 0x30),
	},
	{
		I2C_BOARD_INFO("ov5640", 0x3c),
	},
};

#if defined(CONFIG_SOC_CAMERA_OV2659)
static int ov2659_sensor_power(struct device *dev, int on)
{
	unsigned int cam_pwr;
	unsigned int cam_reset;
	static struct regulator *v_sensor;

	if (!v_sensor) {
		v_sensor = regulator_get(dev, "v_cam_avdd");
		if (IS_ERR(v_sensor)) {
			v_sensor = NULL;
			pr_err(KERN_ERR "Enable v_ldo16 failed!\n");
			return -EIO;
		}
	}

	cam_pwr = mfp_to_gpio(GPIO082_GPIO_CAM_PD_SUB);
	cam_reset = mfp_to_gpio(GPIO083_GPIO_CAM_RST_SUB);

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

	if (on) {
		regulator_set_voltage(v_sensor, 2800000, 2800000);
		regulator_enable(v_sensor);
		msleep(20);
		gpio_direction_output(cam_pwr, 0);
		mdelay(1);
		gpio_direction_output(cam_reset, 0);
		mdelay(1);
		gpio_direction_output(cam_reset, 1);
		mdelay(1);
	} else {
		gpio_direction_output(cam_reset, 0);
		mdelay(1);
		gpio_direction_output(cam_reset, 1);
		gpio_direction_output(cam_pwr, 1);
		regulator_disable(v_sensor);
	}
	gpio_free(cam_pwr);
	gpio_free(cam_reset);
	return 0;
}

static struct soc_camera_link iclink_ov2659_dvp = {
	.bus_id         = 0,            /* Must match with the camera ID */
	.power          = ov2659_sensor_power,
	.board_info     = &dkb_i2c_camera[0],
	.i2c_adapter_id = 0,
	.module_name    = "ov2659",
};

static struct platform_device dkb_ov2659_dvp = {
	.name   = "soc-camera-pdrv",
	.id     = 0,
	.dev    = {
		.platform_data = &iclink_ov2659_dvp,
	},
};
#endif

#if defined(CONFIG_SOC_CAMERA_OV5640) || defined(CONFIG_SOC_CAMERA_OV5640_ECS)
static int ov5640_sensor_power(struct device *dev, int on)
{
	static struct regulator *af_vcc;
	static struct regulator *avdd;
	int cam_reset;
	int pwdn = mfp_to_gpio(GPIO080_GPIO_CAM_PD_MAIN);
	int ret = 0;

	cam_reset = mfp_to_gpio(GPIO081_GPIO_CAM_RST_MAIN);

	if (gpio_request(pwdn, "CAM_ENABLE_LOW")) {
		ret = -EIO;
		goto out;
	}

	if (gpio_request(cam_reset, "CAM_RESET_LOW")) {
		ret = -EIO;
		goto out_rst;
	}

	if (!af_vcc) {
		af_vcc = regulator_get(dev, "v_cam_af");
		if (IS_ERR(af_vcc)) {
			ret = -EIO;
			goto out_af_vcc;
		}
	}

	if (!avdd) {
		avdd = regulator_get(dev, "v_cam_avdd");
		if (IS_ERR(avdd)) {
			ret =  -EIO;
			goto out_avdd;
		}
	}

	/* Enable voltage for camera sensor OV5640 */
	if (on) {
		regulator_set_voltage(af_vcc, 2800000, 2800000);
		regulator_enable(af_vcc);
		regulator_set_voltage(avdd, 2800000, 2800000);
		regulator_enable(avdd);
		mdelay(5);
		/* enable the sensor now*/
		gpio_direction_output(pwdn, 0);
		mdelay(1);
		gpio_direction_output(cam_reset, 0);
		mdelay(1);
		gpio_direction_output(cam_reset, 1);
		mdelay(20);
	} else {
		gpio_direction_output(cam_reset, 0);
		mdelay(1);
		gpio_direction_output(cam_reset, 1);

		regulator_disable(avdd);
		regulator_disable(af_vcc);

		gpio_direction_output(pwdn, 1);
		mdelay(1);
	}

	gpio_free(cam_reset);
	gpio_free(pwdn);
	return 0;

out_avdd:
	avdd = NULL;
	regulator_put(af_vcc);
out_af_vcc:
	af_vcc = NULL;
	gpio_free(cam_reset);
out_rst:
	gpio_free(pwdn);
out:
	return ret;
}
static struct soc_camera_link iclink_ov5640_mipi = {
	.bus_id         = 0,            /* Must match with the camera ID */
	.power          = ov5640_sensor_power,
	.board_info     = &dkb_i2c_camera[1],
	.i2c_adapter_id = 0,
	.module_name    = "ov5640",
	.priv		= "pxa98x-mipi",
};
static struct platform_device dkb_ov5640_mipi = {
	.name   = "soc-camera-pdrv",
	.id     = 1,
	.dev    = {
		.platform_data = &iclink_ov5640_mipi,
	},
};
#endif

static struct platform_device *dkb_platform_devices[] = {
#if defined(CONFIG_SOC_CAMERA_OV2659)
	&dkb_ov2659_dvp,
#endif
#if defined(CONFIG_SOC_CAMERA_OV5640) || defined(CONFIG_SOC_CAMERA_OV5640_ECS)
	&dkb_ov5640_mipi,
#endif
};

#ifdef CONFIG_VPU_DEVFREQ
static struct devfreq_frequency_table *vpu_freq_table;

static struct devfreq_platform_data devfreq_vpu_pdata = {
	.clk_name = "VPUCLK",
};

static struct platform_device pxa988_device_vpudevfreq = {
	.name = "devfreq-vpu",
	.id = -1,
};

static void __init pxa988_init_device_vpudevfreq(void)
{
	u32 i = 0;
	u32 vpu_freq_num = pxa988_get_vpu_op_num();

	vpu_freq_table = kmalloc(sizeof(struct devfreq_frequency_table) * \
					(vpu_freq_num + 1), GFP_KERNEL);
	if (!vpu_freq_table)
		return;

	for (i = 0; i < vpu_freq_num; i++) {
		vpu_freq_table[i].index = i;
		vpu_freq_table[i].frequency = pxa988_get_vpu_op_rate(i);
	}
	vpu_freq_table[i].index = i;
	vpu_freq_table[i].frequency = DEVFREQ_TABLE_END;

	devfreq_vpu_pdata.freq_table = vpu_freq_table;

	pxa988_device_vpudevfreq.dev.platform_data = (void *)&devfreq_vpu_pdata;
	platform_device_register(&pxa988_device_vpudevfreq);
}
#endif

#ifdef CONFIG_DDR_DEVFREQ
static struct devfreq_frequency_table *ddr_freq_table;

static struct devfreq_pm_qos_table ddr_freq_qos_table[] = {
	/* list all possible frequency level here */
	{
		.freq = 208000,
		.qos_value = DDR_CONSTRAINT_LVL0,
	},
	{
		.freq = 312000,
		.qos_value = DDR_CONSTRAINT_LVL1,
	},
	{
		.freq = 400000,
		.qos_value = DDR_CONSTRAINT_LVL2,
	},
	{
		.freq = 533000,
		.qos_value = DDR_CONSTRAINT_LVL3,
	},
	{0, 0},
};


static struct devfreq_platform_data devfreq_ddr_pdata = {
	.clk_name = "ddr",
	.interleave_is_on = 0,	/* only one mc */
};

static struct platform_device pxa988_device_ddrdevfreq = {
	.name = "devfreq-ddr",
	.id = -1,
};

static void __init pxa988_init_device_ddrdevfreq(void)
{
	u32 i = 0;
	u32 ddr_freq_num = pxa988_get_ddr_op_num();

	ddr_freq_table = kmalloc(sizeof(struct devfreq_frequency_table) * \
					(ddr_freq_num + 1), GFP_KERNEL);
	if (!ddr_freq_table)
		return;

	for (i = 0; i < ddr_freq_num; i++) {
		ddr_freq_table[i].index = i;
		ddr_freq_table[i].frequency = pxa988_get_ddr_op_rate(i);
	}
	ddr_freq_table[i].index = i;
	ddr_freq_table[i].frequency = DEVFREQ_TABLE_END;

	devfreq_ddr_pdata.freq_table = ddr_freq_table;
	devfreq_ddr_pdata.hw_base[0] =  DMCU_VIRT_BASE;
	devfreq_ddr_pdata.hw_base[1] =  DMCU_VIRT_BASE;

	if (cpu_is_pxa986() && (!cpu_is_z1z2()))
		devfreq_ddr_pdata.qos_list = ddr_freq_qos_table;

	pxa988_device_ddrdevfreq.dev.platform_data = (void *)&devfreq_ddr_pdata;
	platform_device_register(&pxa988_device_ddrdevfreq);
}
#endif

/* clk usage desciption */
MMP_HW_DESC(fb, "pxa168-fb", 0, PM_QOS_CPUIDLE_BLOCK_DDR_VALUE, "LCDCLK");
struct mmp_hw_desc *emei_dkb_hw_desc[] __initdata = {
	&mmp_device_hw_fb,
};

#ifdef CONFIG_PROC_FS

static void gps_eclk_ctrl(int on)
{
	mfp_cfg_t gps_eclk_en = GPIO089_GPS_CLK;
	mfp_cfg_t gps_eclk_dis = GPIO089_GPIO_89;

	if (on)
		mfp_config(&gps_eclk_en, 1);
	else
		mfp_config(&gps_eclk_dis, 1);
}

/* GPS: power on/off control */
static void gps_power_on(void)
{
	unsigned int gps_ldo, gps_rst_n;
	gps_eclk_ctrl(0);

	/* hardcode */
	gps_ldo = 104;
	if (gpio_request(gps_ldo, "gpio_gps_ldo")) {
		pr_err("Request GPIO failed, gpio: %d\n", gps_ldo);
		return;
	}

	gps_rst_n = mfp_to_gpio(GPIO084_GPIO_GPS_RESET_N);
	if (gpio_request(gps_rst_n, "gpio_gps_rst")) {
		pr_err("Request GPIO failed, gpio: %d\n", gps_rst_n);
		goto out;
	}

	gpio_direction_output(gps_ldo, 0);
	gpio_direction_output(gps_rst_n, 0);
	mdelay(1);
	gpio_direction_output(gps_ldo, 1);

	pr_info("gps chip powered on\n");

	gpio_free(gps_rst_n);
out:
	gpio_free(gps_ldo);
	return;
}

static void gps_power_off(void)
{
	unsigned int gps_ldo, gps_rst_n, gps_on;
	gps_eclk_ctrl(0);

	/* hardcode */
	gps_ldo = 104;
	if (gpio_request(gps_ldo, "gpio_gps_ldo")) {
		pr_err("Request GPIO failed, gpio: %d\n", gps_ldo);
		return;
	}

	gps_on = mfp_to_gpio(GPIO096_GPIO_GPS_ON_OFF);
	if (gpio_request(gps_on, "gpio_gps_on")) {
		pr_err("Request GPIO failed,gpio: %d\n", gps_on);
		goto out1;
	}

	gps_rst_n = mfp_to_gpio(GPIO084_GPIO_GPS_RESET_N);
	if (gpio_request(gps_rst_n, "gpio_gps_rst")) {
		pr_debug("Request GPIO failed, gpio: %d\n", gps_rst_n);
		goto out2;
	}

	gpio_direction_output(gps_ldo, 0);
	gpio_direction_output(gps_rst_n, 0);
	gpio_direction_output(gps_on, 0);

	pr_info("gps chip powered off\n");

	gpio_free(gps_rst_n);
out2:
	gpio_free(gps_on);
out1:
	gpio_free(gps_ldo);
	return;
}

static void gps_reset(int flag)
{
	unsigned int gps_rst_n;

	gps_rst_n = mfp_to_gpio(GPIO084_GPIO_GPS_RESET_N);
	if (gpio_request(gps_rst_n, "gpio_gps_rst")) {
		pr_err("Request GPIO failed, gpio: %d\n", gps_rst_n);
		return;
	}

	gpio_direction_output(gps_rst_n, flag);
	gpio_free(gps_rst_n);
}

static void gps_on_off(int flag)
{
	unsigned int gps_on;

	gps_on = mfp_to_gpio(GPIO096_GPIO_GPS_ON_OFF);
	if (gpio_request(gps_on, "gpio_gps_on")) {
		pr_err("Request GPIO failed, gpio: %d\n", gps_on);
		return;
	}

	gpio_direction_output(gps_on, flag);
	gpio_free(gps_on);
}

#define SIRF_STATUS_LEN	16
static char sirf_status[SIRF_STATUS_LEN] = "off";

static ssize_t sirf_read_proc(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	int len = strlen(sirf_status);

	sprintf(page, "%s\n", sirf_status);
	return len + 1;
}

static ssize_t sirf_write_proc(struct file *filp,
		const char *buff, size_t len, loff_t *off)
{
	char messages[256];
	int flag, ret;
	char buffer[7];

	if (len > 255)
		len = 255;

	memset(messages, 0, sizeof(messages));

	if (!buff || copy_from_user(messages, buff, len))
		return -EFAULT;

	if (strlen(messages) > (SIRF_STATUS_LEN - 1)) {
		pr_warning("[ERROR] messages too long! (%d) %s\n",
			strlen(messages), messages);
		return -EFAULT;
	}

	if (strncmp(messages, "off", 3) == 0) {
		strcpy(sirf_status, "off");
		gps_power_off();
	} else if (strncmp(messages, "on", 2) == 0) {
		strcpy(sirf_status, "on");
		gps_power_on();
	} else if (strncmp(messages, "reset", 5) == 0) {
		strcpy(sirf_status, messages);
		ret = sscanf(messages, "%s %d", buffer, &flag);
		if (ret == 2)
			gps_reset(flag);
	} else if (strncmp(messages, "sirfon", 6) == 0) {
		strcpy(sirf_status, messages);
		ret = sscanf(messages, "%s %d", buffer, &flag);
		if (ret == 2)
			gps_on_off(flag);
	} else if (strncmp(messages, "eclk", 4) == 0) {
		ret = sscanf(messages, "%s %d", buffer, &flag);
		if (ret == 2)
			gps_eclk_ctrl(flag);
	} else
		pr_info("usage: echo {on/off} > /proc/driver/sirf\n");

	return len;
}

static void create_sirf_proc_file(void)
{
	struct proc_dir_entry *sirf_proc_file = NULL;

	/*
	 * CSR and Marvell GPS lib will both use this file
	 * "/proc/drver/sirf" may be modified in future
	 */
	sirf_proc_file = create_proc_entry("driver/sirf", 0644, NULL);
	if (!sirf_proc_file) {
		pr_err("sirf proc file create failed!\n");
		return;
	}

	sirf_proc_file->read_proc = sirf_read_proc;
	sirf_proc_file->write_proc = (write_proc_t  *)sirf_write_proc;
}
#endif

static struct timer_list uart_constraint_timer;
static struct pm_qos_request uart_lpm_cons;
static const char uart_cons_name[] = "uart rx pad";
static void uart_add_constraint(int mfp, void *unused)
{
	if (!mod_timer(&uart_constraint_timer, jiffies + 3 * HZ))
		pm_qos_update_request(&uart_lpm_cons,
			PM_QOS_CPUIDLE_BLOCK_DDR_VALUE);
}

static void uart_timer_handler(unsigned long data)
{
	pm_qos_update_request(&uart_lpm_cons,
		PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
}

struct gpio_edge_desc uart_rx_pad = {
	.mfp = MFP_PIN_GPIO47, /* ap UART rx */
	.handler = uart_add_constraint,
};

#define PM800_SW_PDOWN			(1 << 5)
static void emei_dkb_poweroff(void)
{
	unsigned char data;
	pr_info("turning off power....\n");

	preempt_enable();
	data = pm800_extern_read(PM80X_BASE_PAGE, PM800_WAKEUP1);
	pm800_extern_write(PM80X_BASE_PAGE, PM800_WAKEUP1,
			   data | PM800_SW_PDOWN);
}

#ifdef CONFIG_ANDROID_RAM_CONSOLE
static struct platform_device pxa988_device_ramconsole = {
	.name = "ram_console",
	.id = -1,
};
#endif

static int reboot_notifier_func(struct notifier_block *this,
		unsigned long code, void *cmd)
{
	unsigned char data;
	pr_info("reboot notifier...\n");
	if (cmd && (0 == strcmp(cmd, "recovery"))) {
		pr_info("Enter recovery mode\n");
		data = pm800_extern_read(PM80X_BASE_PAGE, 0xef);
		pm800_extern_write(PM80X_BASE_PAGE,
				   0xef, data | 0x1);
	} else {
		data = pm800_extern_read(PM80X_BASE_PAGE, 0xef);
		pm800_extern_write(PM80X_BASE_PAGE,
				   0xef, data & 0xfe);
	}

	if (code != SYS_POWER_OFF) {
		data = pm800_extern_read(PM80X_BASE_PAGE, 0xef);
		/* this bit is for charger server */
		pm800_extern_write(PM80X_BASE_PAGE, 0xef, data | 0x2);
	}

	return 0;
}

static struct notifier_block reboot_notifier = {
	.notifier_call = reboot_notifier_func,
};

static void __init emeidkb_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(emei_dkb_hw_desc); i++)
		mmp_device_hw_register(emei_dkb_hw_desc[i]);

	mfp_config(ARRAY_AND_SIZE(emeidkb_pin_config));

	switch (board_id) {
	case VER_1V1:
		mfp_config(ARRAY_AND_SIZE(dkb1v1_pin_config));
		break;
	case VER_1V0:
		mfp_config(ARRAY_AND_SIZE(dkb1v0_pin_config));
		break;
	default:
		WARN_ON(1);
		pr_warn("Use dkb1.1 configuration!\n");
		mfp_config(ARRAY_AND_SIZE(dkb1v1_pin_config));
		break;
	}

	pm_power_off = emei_dkb_poweroff;
	register_reboot_notifier(&reboot_notifier);

	/* Uart1, AP kernel console and debug */
	pxa988_add_uart(1);
	/* Uart2, GPS */
	pxa988_add_uart(2);

	pxa988_add_keypad(&emei_dkb_keypad_info);
	emeidkb_init_mmc();

#ifdef CONFIG_INPUT_KEYRESET
	if (platform_device_register(&emei_dkb_panic_keys_device))
		printk(KERN_WARNING "%s: register reset key fail\n", __func__);
#endif

#ifdef CONFIG_ANDROID_RAM_CONSOLE
	platform_device_register(&pxa988_device_ramconsole);
#endif

	/* soc-rtc */
	platform_device_register(&pxa988_device_rtc);
	/* backlight */
	platform_device_register(&emei_dkb_lcd_backlight_devices);
	/* set pm800 dvc information,must before pm800 init */
	pm800_dvctable_init();
	pxa988_add_twsi(0, &emeidkb_ci2c_pdata,
			ARRAY_AND_SIZE(emeidkb_i2c_info));
	pxa988_add_twsi(1, &emeidkb_ci2c2_pdata,
			ARRAY_AND_SIZE(emeidkb_i2c2_info));
	pxa988_add_twsi(2, &emeidkb_pwr_i2c_pdata,
			ARRAY_AND_SIZE(emeidkb_pwr_i2c_info));

	/* add audio device: sram, ssp2, squ(tdma), pxa-ssp, mmp-pcm */
	pxa988_add_asram(&pxa988_asram_info);
	pxa988_add_ssp(1);
	pxa988_add_ssp(4);
	platform_device_register(&pxa988_device_squ);
	platform_device_register(&pxa988_device_asoc_platform);
	platform_device_register(&pxa988_device_asoc_ssp1);
	platform_device_register(&pxa988_device_asoc_gssp);
	platform_device_register(&pxa988_device_asoc_pcm);
	platform_device_register(&emei_dkb_audio_device);

	/* off-chip devices */
	platform_add_devices(ARRAY_AND_SIZE(dkb_platform_devices));

#ifdef CONFIG_FB_PXA168
	emeidkb_add_lcd_mipi();
	if (cpu_pxa98x_stepping() < PXA98X_Z3)
		emeidkb_add_tv_out();
#endif

#ifdef CONFIG_UIO_CODA7542
	pxa_register_coda7542();
#endif

#ifdef CONFIG_USB_MV_UDC
	pxa988_device_udc.dev.platform_data = &emeidkb_usb_pdata;
	platform_device_register(&pxa988_device_udc);
#endif

#if defined(CONFIG_VIDEO_MMP)
	pxa988_add_cam(&mv_cam_data);
#endif

#if defined(CONFIG_VIDEO_MVISP) && defined(CONFIG_UIO_MVISP)
	pxa988_init_dxoisp();
#endif

#if (defined CONFIG_CMMB)
	emeidkb_init_spi();
#endif

#ifdef CONFIG_VPU_DEVFREQ
	pxa988_init_device_vpudevfreq();
#endif

#ifdef CONFIG_DDR_DEVFREQ
	pxa988_init_device_ddrdevfreq();
#endif

#ifdef CONFIG_PXA9XX_ACIPC
	platform_device_register(&pxa9xx_device_acipc);
#endif
	pxa988_add_thermal();

#ifdef CONFIG_PROC_FS
	/* create proc for sirf GPS control */
	create_sirf_proc_file();
#endif
	/* add uart pad wakeup */
	mmp_gpio_edge_add(&uart_rx_pad);
	uart_lpm_cons.name = uart_cons_name;
	pm_qos_add_request(&uart_lpm_cons,
		PM_QOS_CPUIDLE_BLOCK, PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
	init_timer(&uart_constraint_timer);
	uart_constraint_timer.function = uart_timer_handler;

	/* If we have a full configuration then disable any regulators
	 * which are not in use or always_on. */
	regulator_has_full_constraints();
}

MACHINE_START(EMEIDKB, "PXA988")
	.map_io		= mmp_map_io,
	.init_irq	= pxa988_init_irq,
	.timer		= &pxa988_timer,
	.reserve	= pxa988_reserve,
	.handle_irq	= gic_handle_irq,
	.init_machine	= emeidkb_init,
	.restart	= mmp_arch_reset,
MACHINE_END
