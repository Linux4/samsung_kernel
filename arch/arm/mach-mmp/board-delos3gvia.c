
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
#include <linux/mmc/card.h>
#include <linux/platform_data/pxa_sdhci.h>
#include <linux/sd8x_rfkill.h>
#include <linux/regmap.h>
#include <linux/mfd/88pm80x.h>
#include <linux/platform_data/mv_usb.h>
#include <linux/pm_qos.h>
#include <linux/clk.h>
#include <linux/lps331ap.h>
#include <linux/spi/pxa2xx_spi.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/i2c-gpio.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>

#include <linux/uio_coda7542.h>
#ifdef CONFIG_SEC_DEBUG
#include "mach/sec_debug.h"
#endif
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif

#ifdef CONFIG_SENSORS_GP2A002S
#include <linux/gp2a.h>
#endif

#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>

#include <mach/addr-map.h>
#include <mach/clock-pxa988.h>
#include <mach/mmp_device.h>
#include <mach/mfp-pxa1088-delos3gvia.h>
#include <mach/irqs.h>
#include <mach/isl29043.h>
#include <mach/pxa988.h>
#include <mach/soc_coda7542.h>
#include <mach/regs-rtc.h>
#include <mach/regs-ciu.h>
#include <plat/pxa27x_keypad.h>
#include <plat/pm.h>
#include <media/soc_camera.h>
#include <mach/isp_dev.h>
#include <mach/gpio-edge.h>

#include <mach/dvfs.h>
#include <mach/features.h>
#include <mach/regs-mpmu.h>
#include <mach/samsung_camera.h>


#ifdef CONFIG_PM_DEVFREQ
#include <plat/devfreq.h>
#endif
#include "common.h"
#include "onboard.h"
#if defined(CONFIG_TOUCHSCREEN_MMS136_TS)
#include <linux/input/mms136_ts.h>
#endif
#if defined(CONFIG_SPA)
#include <mach/spa.h>   
#endif 
#if defined(CONFIG_BQ24157_CHARGER)
#include <mach/bq24157_charger.h>
#endif
#if defined(CONFIG_STC3115_FUELGAUGE)
#include <mach/stc3115_fuelgauge.h>
#endif
#if defined(CONFIG_FSA9480_MICROUSB)
#include <mach/fsa9480.h>
#endif
#if defined(CONFIG_PN547_NFC)
#include <linux/nfc/pn544.h>
#endif

#if ((defined CONFIG_SENSORS_BMA2X2) ||(defined CONFIG_SENSORS_BMM050))
#include "../../../drivers/i2c/chips/bst_sensor_common.h"
#endif

#define VER_1V0 0x10
#define VER_1V1 0x11
static int board_id;
static int recovery_mode;

static int __init board_id_setup(char *str)
{
	int n;
	if (!get_option(&str, &n))
		return 0;
	board_id = n;
	return 1;
}
__setup("board_id=", board_id_setup);

int get_board_id()
{
	return board_id;
}

static int __init recovery_mode_setup(char *str)
{
	int n;
	if (!get_option(&str, &n))
		return 0;
	recovery_mode = n;
	return 1;
}
__setup("recovery_mode=", recovery_mode_setup);

int get_recoverymode()
{
	return recovery_mode;
}

static unsigned long helandkb_pin_config[] __initdata = {
	GPIO000_KP_MKIN0,			/* KP_MKIN[0] */
	GPIO001_KP_MKOUT0 | MFP_LPM_DRIVE_HIGH
			  | MFP_PULL_FLOAT,	/* KP_MKOUT[0] */
	GPIO002_KP_MKIN1,			/* KP_MKIN[1] */
	GPIO003_GPIO_3 | MFP_LPM_INPUT | MFP_PULL_LOW,
	GPIO004_GPIO_4 | MFP_PULL_HIGH,
	GPIO005_GPIO_5 | MFP_PULL_LOW | MFP_LPM_DRIVE_LOW,
	GPIO006_GPIO_6 | MFP_PULL_HIGH,
#define GPIO007_GPIO_AP_SPI_INT (GPIO007_GPIO_7 | MFP_PULL_HIGH)
	GPIO007_GPIO_AP_SPI_INT,
//	GPIO008_GPIO_8 | MFP_LPM_INPUT | MFP_PULL_LOW,

#define GPIO09_LCD_BL_CTRL		(GPIO009_GPIO_9 | MFP_PULL_LOW | MFP_LPM_DRIVE_LOW)
#define GPIO10_ACC_INT		(GPIO010_GPIO_10 | MFP_PULL_LOW | MFP_LPM_INPUT)
	GPIO09_LCD_BL_CTRL,
	GPIO10_ACC_INT,
	GPIO011_GPIO_11 | MFP_LPM_DRIVE_LOW ,
	GPIO012_GPIO_12 | MFP_LPM_INPUT,
	GPIO013_KP_DKIN4,
#define GPIO014_5M_CAM_STBY		(GPIO014_GPIO_14 | MFP_PULL_LOW | MFP_LPM_DRIVE_LOW)
#define GPIO015_5M_CAM_RESET		(GPIO015_GPIO_15 | MFP_PULL_LOW | MFP_LPM_DRIVE_LOW)
#define GPIO016_TSP_SCL		(GPIO016_GPIO_16 | MFP_LPM_FLOAT)
#define GPIO017_TSP_SDA		(GPIO017_GPIO_17 | MFP_LPM_FLOAT)
#define GPIO018_LCD_RESET		GPIO018_GPIO_18 | MFP_LPM_DRIVE_HIGH
#define GPIO019_LCD_ESD_DETECT		(GPIO019_GPIO_19 | MFP_LPM_INPUT)
#define GPIO020_CAM_FLASH_EN		(GPIO020_GPIO_20 | MFP_LPM_DRIVE_LOW)

	GPIO014_5M_CAM_STBY,
	GPIO015_5M_CAM_RESET,
	GPIO016_TSP_SCL,
	GPIO017_TSP_SDA,
	GPIO018_LCD_RESET,
	GPIO019_LCD_ESD_DETECT,
	GPIO020_CAM_FLASH_EN | MFP_LPM_INPUT| MFP_PULL_LOW,

	/*
	 * configure to be GPIO input to avoid leakage in production
	 * mode. would configure it to I2S MFP in sound start up
	 * function.
	 */
	GPIO021_GPIO_21 | MFP_LPM_INPUT,
	GPIO022_GPIO_22 | MFP_LPM_INPUT,
	GPIO023_GPIO_23  | MFP_LPM_INPUT,
	GPIO024_GPIO_24  | MFP_LPM_INPUT,

	GPIO025_GSSP_SCLK,	/* PCM_CLK */
	GPIO026_GSSP_SFRM,	/* PCM_SYNC */
	GPIO027_GSSP_TXD,	/* PCM_TXD */
	GPIO028_GSSP_RXD,	/* PCM_RXD */

#define GPIO029_GPIO_CHG_STAT	(GPIO029_GPIO_29 | MFP_PULL_LOW | MFP_LPM_INPUT)
	GPIO029_GPIO_CHG_STAT,

	GPIO030_GPIO_30 | MFP_PULL_LOW | MFP_LPM_INPUT,
#define GPIO031_GPIO_CPU_READY		(GPIO031_GPIO_31 | MFP_PULL_HIGH)
#define GPIO032_HW_REV_MOD_2            (GPIO032_GPIO_32 | MFP_PULL_LOW | MFP_LPM_INPUT)

	GPIO031_GPIO_CPU_READY,
	GPIO032_HW_REV_MOD_2,
	
	GPIO033_SPI_CLK | MFP_PULL_HIGH,
	GPIO034_SPI_CS0 | MFP_PULL_HIGH,
	GPIO035_SPI_MISO | MFP_PULL_HIGH,
	GPIO036_SPI_MOSI | MFP_PULL_HIGH,
	
#define GPIO035_GPIO_SENSOR_I2C_SCL	(GPIO075_GPIO_75 | MFP_LPM_FLOAT)
#define GPIO036_GPIO_SENSOR_I2C_SDA	(GPIO076_GPIO_76 | MFP_LPM_FLOAT)

	GPIO035_GPIO_SENSOR_I2C_SCL,
	GPIO036_GPIO_SENSOR_I2C_SDA,

	/* MMC2 WIB */
	GPIO037_MMC2_DATA3 | MFP_LPM_FLOAT,	/* WLAN_DAT3 */
	GPIO038_MMC2_DATA2 | MFP_LPM_FLOAT,	/* WLAN_DAT2 */
	GPIO039_MMC2_DATA1 | MFP_LPM_FLOAT,	/* WLAN_DAT1 */
	GPIO040_MMC2_DATA0 | MFP_LPM_FLOAT,	/* WLAN_DAT0 */
	GPIO041_MMC2_CMD,	/* WLAN_CMD */
	GPIO042_MMC2_CLK | MFP_LPM_DRIVE_HIGH,	/* WLAN_CLK */

#define GPIO043_GPIO_DVC1	(GPIO043_GPIO_43 | MFP_PULL_FLOAT)
#define GPIO044_GPIO_DVC2	(GPIO044_GPIO_44 | MFP_PULL_FLOAT)
	GPIO043_GPIO_DVC1,
	GPIO044_GPIO_DVC2,

	GPIO045_UART2_RXD | MFP_LPM_INPUT | MFP_PULL_LOW,	/* GPS_UART_RXD */
	GPIO046_UART2_TXD | MFP_LPM_FLOAT,	/* GPS_UART_TXD */

	GPIO047_UART1_RXD,	/* AP_RXD */
	GPIO048_UART1_TXD,	/* AP_TXD */

#define GPIO049_GPIO_MUS_SCL	GPIO049_GPIO_49 | MFP_LPM_FLOAT
#define GPIO050_GPIO_MUS_SDA	GPIO050_GPIO_50 | MFP_LPM_FLOAT

#define GPIO053_AP_AGPS_RESET	(GPIO091_GPIO_91)
	GPIO049_GPIO_MUS_SCL,
	GPIO050_GPIO_MUS_SDA,

	GPIO051_VIA_UART_RXD,
	GPIO052_VIA_UART_TXD,

	GPIO053_AP_AGPS_RESET,
	GPIO054_GPIO_54 | MFP_LPM_INPUT | MFP_PULL_HIGH,

#define GPIO067_WLAN_PD		(GPIO067_GPIO_67)
	GPIO067_WLAN_PD,
#define GPIO068_AP_AGPS_ONOFF	(GPIO068_GPIO_68)
	GPIO053_GPIO_53 | MFP_LPM_INPUT | MFP_PULL_HIGH,
	GPIO068_AP_AGPS_ONOFF,
//	GPIO070_CCIC_IN4 | MFP_LPM_DRIVE_LOW,
//	GPIO071_CCIC_IN3 | MFP_LPM_DRIVE_LOW,
//	GPIO072_CCIC_IN2 | MFP_LPM_DRIVE_LOW,
//	GPIO073_CCIC_IN1 | MFP_LPM_DRIVE_LOW,
//	GPIO074_CCIC_IN0 | MFP_LPM_DRIVE_LOW,
//	GPIO075_CAM_HSYNC | MFP_LPM_DRIVE_LOW,
//	GPIO076_CAM_VSYNC | MFP_LPM_DRIVE_LOW,
//	GPIO077_CAM_MCLK  | MFP_LPM_DRIVE_LOW, /* CAM_MCLK */
//	GPIO078_CAM_PCLK | MFP_LPM_INPUT | MFP_PULL_LOW,
	GPIO079_CAM_SCL | MFP_LPM_FLOAT,
	GPIO080_CAM_SDA | MFP_LPM_FLOAT,

#define GPIO081_GPIO_VT_CAM_RST		(GPIO081_GPIO_81 | MFP_LPM_DRIVE_LOW)
#define GPIO082_GPIO_VT_CAM_EN	(GPIO082_GPIO_82 | MFP_LPM_DRIVE_LOW)
#define GPIO083_GPIO_HOST_WU_WLAN	(GPIO083_GPIO_83 | MFP_LPM_DRIVE_LOW)
#define GPIO084_GPIO_WLAN_WU_HOST	(GPIO084_GPIO_84 | MFP_LPM_INPUT | MFP_PULL_LOW)
#define GPIO085_GPIO_WL_REG_ON		GPIO085_GPIO_85
#define GPIO086_GPIO_BT_REG_ON		GPIO086_GPIO_86
#define GPIO085_HW_REV_MOD_1            (GPIO085_GPIO_85 | MFP_LPM_INPUT)
#define GPIO086_HW_REV_MOD_0            (GPIO086_GPIO_86 | MFP_LPM_INPUT)

	GPIO081_GPIO_VT_CAM_RST,
	GPIO082_GPIO_VT_CAM_EN,
	GPIO083_GPIO_HOST_WU_WLAN,
	GPIO084_GPIO_WLAN_WU_HOST,
	GPIO085_HW_REV_MOD_1,
	GPIO086_HW_REV_MOD_0,

#define GPIO087_FUEL_SCL		(GPIO087_GPIO_87 | MFP_LPM_FLOAT)
#define GPIO088_FUEL_SDA		(GPIO087_GPIO_87 | MFP_LPM_FLOAT)
#define GPIO089_T_FLASH_DETECT	(GPIO089_GPIO_89 | MFP_LPM_INPUT)

	GPIO087_FUEL_SCL,
	GPIO088_FUEL_SDA,
	GPIO089_T_FLASH_DETECT,
	GPIO090_GPIO_90 | MFP_PULL_LOW,

#define GPIO091_GPIO_JACK2AP_INT_N	(GPIO091_GPIO_91 | MFP_LPM_INPUT | MFP_PULL_HIGH)
#define GPIO092_GPIO_PROXI_INT	(GPIO092_GPIO_92 | MFP_LPM_INPUT)
#define GPIO093_GPIO_MUIC_INT	GPIO093_GPIO_93
#define GPIO094_GPIO_TSP_INT		(GPIO094_GPIO_94 | MFP_LPM_INPUT)
#define GPIO095_GPIO_FUEL_ALERT		GPIO095_GPIO_95 
	GPIO091_GPIO_JACK2AP_INT_N,
	GPIO092_GPIO_PROXI_INT,
	GPIO093_GPIO_MUIC_INT,
	GPIO094_GPIO_TSP_INT,
	GPIO095_GPIO_FUEL_ALERT,
	GPIO096_GPIO_96,
	GPIO097_GPIO_97 | MFP_LPM_INPUT,
#define GPIO097_CAM_FLASH_SET	GPIO008_GPIO_8//(GPIO097_GPIO_97| MFP_PULL_LOW)
#define GPIO098_GPIO_CHG_EN		GPIO098_GPIO_98
#define GPIO124_GPIO_CODEC_INT	(GPIO124_GPIO_124 | MFP_LPM_INPUT | MFP_PULL_HIGH)

	GPIO097_CAM_FLASH_SET,
	GPIO098_GPIO_CHG_EN,
	GPIO124_GPIO_CODEC_INT,

	/* MMC1 Micro SD */
	MMC1_DAT7_MMC1_DAT7 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	MMC1_DAT6_MMC1_DAT6 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	MMC1_DAT5_MMC1_DAT5 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	MMC1_DAT4_MMC1_DAT4 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	MMC1_DAT3_MMC1_DAT3,
	MMC1_DAT2_MMC1_DAT2,
	MMC1_DAT1_MMC1_DAT1,
	MMC1_DAT0_MMC1_DAT0,
	MMC1_CMD_MMC1_CMD | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	MMC1_CLK_MMC1_CLK | MFP_LPM_PULL_HIGH,
	MMC1_CD_MMC1_CD | MFP_PULL_LOW | MFP_LPM_FLOAT,
	MMC1_WP_MMC1_WP | MFP_PULL_HIGH | MFP_LPM_FLOAT,

	/* MMC3 16GB EMMC */
	ND_IO7_MMC3_DAT7 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	ND_IO6_MMC3_DAT6 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	ND_IO5_MMC3_DAT5 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	ND_IO4_MMC3_DAT4 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	ND_IO3_MMC3_DAT3 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	ND_IO2_MMC3_DAT2 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	ND_IO1_MMC3_DAT1 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	ND_IO0_MMC3_DAT0 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	ND_CLE_SM_OEN_MMC3_CMD | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	SM_SCLK_MMC3_CLK | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	SM_BEN0_GPIO126 | MFP_PULL_HIGH | MFP_LPM_FLOAT,

#define GPIO_GPS_TIMER_SYNC	ANT_SW4_GPIO_28
#define GPIO_RF_PDET_EN		SM_ADV_GPIO_0 | MFP_LPM_FLOAT
#define GPIO_LCD_RESET_N	ND_RDY1_GPIO_1
#define GPIO_LED_B_CTRL		SM_ADVMUX_GPIO_2
#define GPIO_LED_R_CTRL		SM_BEN1_GPIO_127
#define GPIO_LED_G_CTRL		SM_CSN0_GPIO_103
#define GPIO_VCM_PWDN		ND_CS1N3_GPIO_102
	GPIO_GPS_TIMER_SYNC,
	SM_ADV_GPIO_0 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	GPIO_LCD_RESET_N | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	SM_ADVMUX_GPIO_2 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	SM_BEN1_GPIO_127 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	SM_CSN0_GPIO_103 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	SM_CSN1_GPIO_104 | MFP_PULL_HIGH |MFP_LPM_FLOAT,
	GPIO_VCM_PWDN | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	/* SM_RDY pin Low for download mode, High for normal boot */
	SM_RDY_GPIO_3 | MFP_PULL_HIGH | MFP_LPM_FLOAT,

	ND_IO15_ND_DAT15 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	ND_IO14_ND_DAT14 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	ND_IO13_ND_DAT13 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
//	ND_IO12_ND_DAT12 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
//	ND_IO11_ND_DAT11 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
//	ND_IO10_ND_DAT10 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
//	ND_IO9_ND_DAT9 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
//	ND_IO8_ND_DAT8 | MFP_PULL_HIGH |  MFP_LPM_FLOAT,
	ND_IO9_FPGA_SPI_CLK | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	ND_IO10_ND_DAT10 | MFP_PULL_LOW,
	ND_IO11_FPGA_SPI_MOSI | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	ND_IO12_FPGA_SPI_MISO | MFP_PULL_HIGH | MFP_LPM_FLOAT,

	ND_nCS0_SM_nCS2 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	DF_ALE_SM_WEn_ND_ALE | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	DF_WEn_DF_WEn | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	DF_REn_DF_REn | MFP_PULL_HIGH | MFP_LPM_FLOAT,
	DF_RDY0_DF_RDY0 | MFP_PULL_HIGH | MFP_LPM_FLOAT,
};

static unsigned long dvc_pin_config[] __initdata = {

#define GPIO043_GPIO_DVC1_Ax	(GPIO043_GPIO_43_Ax | MFP_PULL_FLOAT)
#define GPIO044_GPIO_DVC2_Ax	(GPIO044_GPIO_44_Ax | MFP_PULL_FLOAT)

	GPIO043_GPIO_DVC1_Ax,
	GPIO044_GPIO_DVC2_Ax,
};


#if defined (CONFIG_SEC_MODEM_DELOS3GVIA)
static struct pxa2xx_spi_master pxa_ssp_master_info = {
	.num_chipselect = 1,
	.enable_dma = 0,
};

static void pld_dummy_cs(u32 cmd)
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

static struct pxa2xx_spi_chip pld_spi_chip = {
	.rx_threshold   = 1,
	.tx_threshold   = 1,
	.cs_control     = pld_dummy_cs,
};

/* bus_num must match id in pxa2xx_set_spi_info() call */
static struct spi_board_info spi_board_info[] __initdata = {
	{
		.modalias		= "fpga_spi",
		.platform_data	= NULL,
		.controller_data	= &pld_spi_chip,
		.max_speed_hz	= 12000000,
		.bus_num		= 3,
		.chip_select	= 0,
		.mode		= SPI_MODE_3,
	},
};

static void __init emeidkb_init_spi(void)
{
	pr_info("%s: +++\n", __func__);
	pxa988_add_ssp(2);
	pxa988_add_spi(3, &pxa_ssp_master_info);
	if (spi_register_board_info(spi_board_info,
			ARRAY_SIZE(spi_board_info))) {
		pr_err("[ERROR] failed to register spi device.\n");
		return;
	}
	pr_info("%s: ---\n", __func__);
}
#endif

static unsigned int helan_dkb_matrix_key_map[] = {
	KEY(0, 0, KEY_VOLUMEDOWN),



	KEY(1, 0, KEY_VOLUMEUP),
};

static struct pxa27x_keypad_platform_data helan_dkb_keypad_info __initdata = {
	.matrix_key_rows	= 2,
	.matrix_key_cols	= 1,
	.matrix_key_map		= helan_dkb_matrix_key_map,
	.matrix_key_map_size	= ARRAY_SIZE(helan_dkb_matrix_key_map),
	.debounce_interval	= 30,

	.direct_key_num = 6,
	.direct_key_map = {KEY_RESERVED,
			KEY_RESERVED,
			KEY_RESERVED,
			KEY_RESERVED,
			KEY_RESERVED,
			KEY_HOME},
	.direct_wakeup_pad = {0,0,0,0,0,
			 MFP_PIN_GPIO13},
	.direct_key_low_active	= 1,
};

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

static struct platform_device helan_dkb_audio_device = {
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
	[PM800_ID_BUCK5] = REGULATOR_SUPPLY("v_cam_c", NULL),
	/* LDO power supplies: LDO[1..19] */
	[PM800_ID_LDO1]  = REGULATOR_SUPPLY("v_ldo1", NULL),
	[PM800_ID_LDO2]  = REGULATOR_SUPPLY("v_micbias", NULL),
	[PM800_ID_LDO3]  = REGULATOR_SUPPLY("v_analog_2v8", NULL),
	[PM800_ID_LDO4]  = REGULATOR_SUPPLY("v_usim1", NULL),
	[PM800_ID_LDO5]  = REGULATOR_SUPPLY("v_usb_3v1", NULL),
	[PM800_ID_LDO6]  = REGULATOR_SUPPLY("v_motor_3v", NULL),
	[PM800_ID_LDO7]  = REGULATOR_SUPPLY("v_tsp_3v"/*V_LDO7*/, NULL),
	[PM800_ID_LDO8]  = REGULATOR_SUPPLY("v_lcd_3V", NULL),
	[PM800_ID_LDO9]  = REGULATOR_SUPPLY("v_wib_3v3", NULL),
	[PM800_ID_LDO10] = REGULATOR_SUPPLY("v_proxy_3v", NULL),
	[PM800_ID_LDO11] = REGULATOR_SUPPLY("v_cam_io", NULL),
	[PM800_ID_LDO12] = REGULATOR_SUPPLY("vqmmc", "sdhci-pxav3.0"),
	[PM800_ID_LDO13] = REGULATOR_SUPPLY("vmmc", "sdhci-pxav3.0"),
	[PM800_ID_LDO14] = REGULATOR_SUPPLY("v_vramp_2v8", NULL),
	[PM800_ID_LDO15] = REGULATOR_SUPPLY("v_proxy_led_3v3", NULL),
	[PM800_ID_LDO16] = REGULATOR_SUPPLY("v_cam_avdd", NULL),
	[PM800_ID_LDO17] = REGULATOR_SUPPLY("v_cam_af", NULL),
	[PM800_ID_LDO18] = REGULATOR_SUPPLY("v_ldo18", NULL),
#ifdef GPS_LDO_POWER
	[PM800_ID_LDO19] = REGULATOR_SUPPLY("v_gps_1v8", NULL),
#else
	[PM800_ID_LDO19] = REGULATOR_SUPPLY("v_usim2", NULL),
#endif

	/* below 4 IDs are fake ids, they are only used in new dvc */
	[PM800_ID_BUCK1_AP_ACTIVE] = REGULATOR_SUPPLY("vcc_main_ap_active", NULL),
	[PM800_ID_BUCK1_AP_LPM] = REGULATOR_SUPPLY("vcc_main_ap_lpm", NULL),
	[PM800_ID_BUCK1_APSUB_IDLE] = REGULATOR_SUPPLY("vcc_main_apsub_idle", NULL),
	[PM800_ID_BUCK1_APSUB_SLEEP] = REGULATOR_SUPPLY("vcc_main_apsub_sleep", NULL),
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

	/* below 4 ids are fake id, they are only used in new dvc */
	PM800_ID_BUCK1_AP_ACTIVE,
	PM800_ID_BUCK1_AP_LPM,
	PM800_ID_BUCK1_APSUB_IDLE,
	PM800_ID_BUCK1_APSUB_SLEEP,
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
	[PM800_ID_LDO2]  = REG_INIT(LDO2,  1700000, 2800000, 0, 1),
	[PM800_ID_LDO3]  = REG_INIT(LDO3,  1200000, 3300000, 1, 1),
	[PM800_ID_LDO4]  = REG_INIT(LDO4,  1200000, 3300000, 0, 0),
	[PM800_ID_LDO5]  = REG_INIT(LDO5,  1200000, 3300000, 1, 1),
	[PM800_ID_LDO6]  = REG_INIT(LDO6,  1200000, 3300000, 0, 0),
	[PM800_ID_LDO7]  = REG_INIT(LDO7,  3000000, 3000000, 0, 1),
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
	[PM800_ID_LDO19] = REG_INIT(LDO19, 1700000, 3300000, 0, 0),

	/* below 4 items are fake, they are only used in new dvc */
	[PM800_ID_BUCK1_AP_ACTIVE] = REG_INIT(BUCK1_AP_ACTIVE,  1000, 10000, 1, 1),
	[PM800_ID_BUCK1_AP_LPM] = REG_INIT(BUCK1_AP_LPM,  1000, 10000, 1, 1),
	[PM800_ID_BUCK1_APSUB_IDLE] = REG_INIT(BUCK1_APSUB_IDLE,  1000, 10000, 1, 1),
	[PM800_ID_BUCK1_APSUB_SLEEP] = REG_INIT(BUCK1_APSUB_SLEEP,  1000, 10000, 1, 1),
};

static void mic_set_power(int on)
{
	static int mic_power_flag = 0;
	struct regulator *v_ldo = regulator_get(NULL, "v_micbias");
	if (IS_ERR(v_ldo)) {
		v_ldo = NULL;
		pr_err("Get regulator error\n");
		return;
	}

	if (on && (!mic_power_flag)) {
		regulator_enable(v_ldo);
		mic_power_flag = 1;
	}

	if (mic_power_flag && (!on)) {
		regulator_disable(v_ldo);
		mic_power_flag = 0;
	}

	regulator_put(v_ldo);
	v_ldo = NULL;
}

#ifdef CONFIG_RTC_DRV_SA1100
static int sync_time_to_soc(unsigned int ticks)
{
	RCNR = ticks;
	udelay(200);
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
	int data;
	u8 i;
	if (!chip || !pdata || !chip->regmap || !chip->subchip
	    || !chip->subchip->regmap_power) {
		pr_err("%s:chip or pdata is not availiable!\n", __func__);
		return -EINVAL;
	}

	/* RESET_OUTn, RTC_RESET_MODE =0 */
	regmap_write(chip->regmap, PM800_RTC_MISC1, 0xb0);

	/* Set internal digital sleep voltage as 1.2V */
	regmap_write(chip->regmap, PM800_LOW_POWER1, 0x0);
	/* Enable 32Khz-out-3 low jitter XO_LJ = 1 */
	regmap_write(chip->regmap, PM800_LOW_POWER2, 0x20);

	/* Enabele LDO and BUCK clock gating in lpm */
	regmap_write(chip->regmap, PM800_LOW_POWER_CONFIG3, 0x80);
	/* Enable reference group sleep mode */
	regmap_write(chip->regmap, PM800_LOW_POWER_CONFIG4, 0x80);

	/* Enable 32Khz-out-from XO 1, 2, 3 all enabled */
	regmap_write(chip->regmap, PM800_RTC_MISC2, 0x2a);

	/* Enable voltage change in pmic, POWER_HOLD = 1 */
	regmap_write(chip->regmap, PM800_WAKEUP1, 0x80);

	/*
	 * Block wakeup attempts when VSYS rises above
	 * VSYS_UNDER_RISE_TH1, or power off may fail
	 */
	regmap_read(chip->regmap,PM800_RTC_MISC5, &data);
	data |= 0x1;
	regmap_write(chip->regmap,PM800_RTC_MISC5, data);

	/* Enable GPADC sleep mode */
	regmap_write(chip->subchip->regmap_gpadc,
		     PM800_GPADC_MISC_CONFIG2, 0x71);
	/* Enlarge GPADC off slots */
	regmap_write(chip->subchip->regmap_gpadc, 0x08, 0x0f);

	/* Set buck1 sleep mode as 0.8V */
	regmap_write(chip->subchip->regmap_power, PM800_SLEEP_BUCK1, 0x10);
	/* Set buck1 audio mode as 0.8V and trun on LDO(7 or 18)*/
	regmap_write(chip->subchip->regmap_power, PM800_AUDIO_MODE_CONFIG1,
			0x90);
	/* Enable buck sleep mode */
	regmap_write(chip->subchip->regmap_power, PM800_BUCK_SLP1, 0xaa);
	regmap_write(chip->subchip->regmap_power, PM800_BUCK_SLP2, 0x2);

	/* Enable LDO sleep mode */
	regmap_write(chip->subchip->regmap_power, PM800_LDO_SLP1, 0xa8);
	regmap_write(chip->subchip->regmap_power, PM800_LDO_SLP2, 0xaa);
	regmap_write(chip->subchip->regmap_power, PM800_LDO_SLP3, 0xab);
	
	regmap_write(chip->subchip->regmap_power, PM800_LDO_SLP4, 0xaa);
	regmap_write(chip->subchip->regmap_power, PM800_LDO_SLP5, 0xaa);

	/*WIFI may power on in sleep mode, so set sleep voltage 3.3V*/
	regmap_write(chip->subchip->regmap_power, PM800_LDO9, 0xff);

#ifdef GPS_LDO_POWER
	/*set LDO19 for GPS sleep voltage 1.8V*/
	regmap_write(chip->subchip->regmap_power, PM800_LDO19, 0x11);
#endif
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

	/*
	 * Base page 0x50 and 0x55 should be set to 0x0C to allow PMIC
	 * Buck clock and digital clocks to be locked to the 32KHz clock,
	 * but make sure USE_XO field (Bas page 0xD0.7) is previously set.
	 *
	 * Once you set 0x0C, if you read back you will read 0x0D, as the
	 * LSB is a Read Only bit representing the ..ock.?fla which will
	 * be set shortly after bit 1 of the same register is set to 0.
	 */
	data = 0x0C;
	regmap_write(chip->regmap, OSC_CNTRL1, data);
	regmap_write(chip->regmap, OSC_CNTRL6, data);

	/* Forcing the clock of the bucks to be active also during sleep */
	regmap_read(chip->regmap, OSC_CNTRL3, &data);
	data |= (1 << 4) | (1 << 7);
	regmap_write(chip->regmap, OSC_CNTRL3, data);

	/* Set the 4 regs of buck4 as 1.8v */
	regmap_write(chip->subchip->regmap_power,
		     PM800_BUCK4, 0x54);
	regmap_write(chip->subchip->regmap_power,
		     PM800_BUCK4_1, 0x54);
	regmap_write(chip->subchip->regmap_power,
		     PM800_BUCK4_2, 0x54);
	regmap_write(chip->subchip->regmap_power,
		     PM800_BUCK4_3, 0x54);


	/* BUCK enable 0x50, BUCK1, 2, 3, 4 */
	regmap_write(chip->subchip->regmap_power, 0x50, 0x0f);
	/* LDO enable 0x51, 0x52, 0x53, LDO1, 3, 5, 7, 8 */
	regmap_write(chip->subchip->regmap_power, 0x51, 0xD4);
	regmap_write(chip->subchip->regmap_power, 0x52, 0x20);
	regmap_write(chip->subchip->regmap_power, 0x53, 0x02);

	/* Dump power-down log */
	regmap_read(chip->regmap, PM800_POWER_DOWN_LOG1, &data);
	pr_info("PowerDW Log1 0x%x: 0x%x\n", PM800_POWER_DOWN_LOG1, data);
	regmap_read(chip->regmap, PM800_POWER_DOWN_LOG2, &data);
	pr_info("PowerDW Log2 0x%x: 0x%x\n", PM800_POWER_DOWN_LOG2, data);
	/* Clear power-down log */
	regmap_write(chip->regmap, PM800_POWER_DOWN_LOG1, 0xff);
	regmap_write(chip->regmap, PM800_POWER_DOWN_LOG2, 0xff);

	if (dvc_flag) {
		int num = pxa988_get_vl_num();
		/* Write svc level values except level 0 */
		for (i = num - 1; i > 0 ; i--) {
			data = pm800_extern_write(PM80X_POWER_PAGE,
				0x3c + i, (pxa988_get_vl(i) - 600) * 10 / 125);
			if (data < 0) {
				printk(KERN_ERR "SVC table writting failed !\n");
				return -1;
			}
		}
	}
	return 0;
}

static struct pm80x_headset_pdata pm80x_headset = {
	.headset_flag = 0,
	.mic_set_power = mic_set_power,
	.hook_press_th = 68,
	.vol_up_press_th = 138,
	.vol_down_press_th = 345,
	.mic_det_th = 465,
	.press_release_th = 400,
};

int vibrator_control_power(int on){
	static struct regulator *vib_3_3;
	static bool bFirst=1;
	static int bStatus=0;

	if(bFirst)
	{
		//LDO Power On=============
		if (!vib_3_3) {			
			vib_3_3 = regulator_get(NULL, "v_motor_3v");
			if (IS_ERR(vib_3_3)) {
				pr_err("%s regulator get error!\n", __func__);
				vib_3_3 = NULL;
				return -1;
			}
		}
		regulator_set_voltage(vib_3_3, 3300000, 3300000);
		bFirst=0;
	}

	if(on == bStatus)
		return 0;

	if(on){
		regulator_enable(vib_3_3);
		msleep(2);
		bStatus = 1;
	}
	else{
		regulator_disable(vib_3_3);
		bStatus = 0;
	}

	return 0;
}

static struct pm80x_vibrator_pdata vibrator_pdata = {
	.min_timeout = 10,
	.vibrator_power = vibrator_control_power,
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

static int pm805_plat_config(struct pm80x_chip *chip,
		struct pm80x_platform_data *pdata)
{
	int data;
	if (!chip || !pdata || !chip->regmap) {
		pr_err("%s:chip or pdata is not availiable!\n", __func__);
		return -EINVAL;
	}

	/* power up */
	regmap_read(chip->regmap, 0x01, &data);
	data |= 0x3;
	regmap_write(chip->regmap, 0x01, data);
	msleep(1);
	regmap_write(chip->regmap, 0x30, 0x00);
	return 0;
}

static struct pm80x_platform_data pm805_info = {
	.irq_mode = 0,
	.plat_config = pm805_plat_config,
};


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

/* compass */

#if defined(CONFIG_SENSORS_BMM050)
static struct bosch_sensor_specific bss_bmm = {
	.name = "bmc150",
	.place = 1,/*marvell-via*/
};
static struct i2c_board_info i2c_bmm050={
	I2C_BOARD_INFO("bmm050", 0x12),
	.platform_data = &bss_bmm,		
};
#endif //CONFIG_SENSORS_BMM050

#if defined(CONFIG_SENSORS_BMA2X2)
static struct i2c_board_info i2c_bma2x2={
	I2C_BOARD_INFO("bma2x2", 0x10),
#if defined(CONFIG_SENSORS_BMM050)		
	.platform_data = &bss_bmm,
#elif CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
	.platform_data = &bma2x2_pdata,
#endif			
};
#endif //CONFIG_SENSORS_BMA2X2

#if defined(CONFIG_SENSORS_GP2A002S)

int gp2a_device_power(bool on){
	static struct regulator *prox_3_0;
	static struct regulator *proxLed;
	static bool bFirst=1;
	static bool bStatus=0;

	if(bFirst)
	{
		//LDO Power On=============
		if (!prox_3_0) {
			prox_3_0 = regulator_get(NULL, "v_proxy_3v");
			if (IS_ERR(prox_3_0)) {
				pr_err("%s regulator get error!\n", __func__);
				prox_3_0 = NULL;
				return -1;
			}
		}
		regulator_set_voltage(prox_3_0, 3000000, 3000000);

		if (!proxLed) {
			proxLed = regulator_get(NULL, "v_proxy_led_3v3");
			if (IS_ERR(proxLed)) {
				pr_err("%s regulator get error!\n", __func__);
				proxLed = NULL;
				return -1;
			}
		}
		regulator_set_voltage(proxLed, 3300000, 3300000);
		bFirst=0;
	}

	if(on == bStatus)
		return 0;

	if(on){
		regulator_enable(prox_3_0);
		msleep(2);

		regulator_enable(proxLed);
		msleep(2);

		bStatus = 1;
	}
	else{
		regulator_disable(proxLed);
		regulator_disable(prox_3_0);
		bStatus = 0;
	}

	return 0;
}

static struct gp2a_platform_data gp2a_dev_data={
	.power = gp2a_device_power,
	.p_out = mfp_to_gpio(GPIO092_GPIO_92),
};

static struct i2c_board_info i2c_gp2a[]={
	{
		I2C_BOARD_INFO("gp2a", 0x44),
		.platform_data = &gp2a_dev_data,
	},
};

static struct pxa_i2c_board_gpio i2c_gp2a_gpio[] = {
	{
		.type = "gp2a",
		.gpio = mfp_to_gpio(GPIO092_GPIO_92),
	},
};
#endif
#if defined(CONFIG_SENSORS_BMA2X2) || defined(CONFIG_SENSORS_BMM050) || defined(CONFIG_SENSORS_GP2A002S)
static struct i2c_gpio_platform_data i2c_gpio_data = {
	.sda_pin		= mfp_to_gpio(GPIO076_GPIO_76),
	.scl_pin		= mfp_to_gpio(GPIO075_GPIO_75),
	.udelay			= 3,
	.timeout = 100,
};
static struct platform_device i2c_gpio_device = {
	.name	= "i2c-gpio",
	.id	= 5,
	.dev	= {
		.platform_data	= &i2c_gpio_data,
	},
};
static void board_sensors_init()
{
	#if defined(CONFIG_SENSORS_BMA2X2)
		i2c_register_board_info(5, &i2c_bma2x2, 1);
	#endif
	#if defined(CONFIG_SENSORS_BMM050)
		i2c_register_board_info(5, &i2c_bmm050, 1);
	#endif

	#if defined(CONFIG_SENSORS_GP2A002S)
		i2c_register_board_info(5, &i2c_gp2a, 1);
	#endif
}
#endif

#if defined(CONFIG_BQ24157_CHARGER)
/* On emeidkb, the eoc current is decided by Riset2 */
static struct bq24157_platform_data  bq24157_charger_info = {
	.cd = mfp_to_gpio(GPIO098_GPIO_98),

};
#endif

static struct i2c_board_info helandkb_pwr_i2c_info[] = {
	{
		.type		= "88PM800",
		.addr		= 0x30,
		.irq		= IRQ_PXA988_PMIC,
		.platform_data	= &pm800_info,
	},
	{
		.type = "88PM805",
		.addr = 0x38,
		.platform_data = &pm805_info,
	},
#if defined(CONFIG_BQ24157_CHARGER)
	{ 
		.type		= "bq24157_6A",
		.addr		= 0x6A,
		.platform_data	= &bq24157_charger_info,
	},
#endif
};

static struct pxa_i2c_board_gpio helandkb_pwr_i2c_gpio[] = {
	{
		.type = "88PM805",
		.gpio = mfp_to_gpio(GPIO124_GPIO_124),
	},
#if defined(CONFIG_BQ24157_CHARGER)
	{
		.type = "bq24157_6A",
		.gpio = mfp_to_gpio(GPIO012_GPIO_12),
	},
#endif
};

#if defined(CONFIG_FSA9480_MICROUSB)
static struct i2c_gpio_platform_data i2c_fsa9480_bus_data = {
	.sda_pin = mfp_to_gpio(GPIO050_GPIO_50),
	.scl_pin = mfp_to_gpio(GPIO049_GPIO_49),
	.udelay  = 3,
	.timeout = 100,
};

static struct platform_device i2c_fsa9480_bus_device = {
	.name		= "i2c-gpio",
	.id		= 7, /* pxa92x-i2c are bus 0, 1 so start at 2 */
	.dev = {
		.platform_data = &i2c_fsa9480_bus_data,
	}
};
static struct fsa9480_platform_data FSA9480_info = {
};
static struct i2c_board_info __initdata fsa9480_i2c_devices[] = {
	{
		I2C_BOARD_INFO("fsa9480", 0x25),
		.platform_data	= &FSA9480_info,
	},
};
static struct pxa_i2c_board_gpio fsa9480_i2c_gpio[] = {
	{
		.type = "fsa9480",
		.gpio = mfp_to_gpio(GPIO093_GPIO_93),
	},
};
#endif
#if defined(CONFIG_STC3115_FUELGAUGE)
static struct stc311x_platform_data stc3115_platform_data = {
        .battery_online = NULL,
        .charger_online = NULL,		// used in stc311x_get_status()
        .charger_enable = NULL,		// used in stc311x_get_status()
        .power_supply_register = NULL,
        .power_supply_unregister = NULL,
		.Vmode= 0,       /*REG_MODE, BIT_VMODE 1=Voltage mode, 0=mixed mode */
		.Alm_SOC = 10,      /* SOC alm level %*/
		.Alm_Vbat = 3600,   /* Vbat alm level mV*/
  		.CC_cnf = 400,      /* nominal CC_cnf, coming from battery characterisation*/
  		.VM_cnf = 405,      /* nominal VM cnf , coming from battery characterisation*/
  		.Cnom = 2000,       /* nominal capacity in mAh, coming from battery characterisation*/
		.Rsense = 10,       /* sense resistor mOhms*/
  		.RelaxCurrent = 100, /* current for relaxation in mA (< C/20) */
		.Adaptive = 1,     /* 1=Adaptive mode enabled, 0=Adaptive mode disabled */

		.CapDerating[6] = 190,   /* capacity derating in 0.1%, for temp = -20°C */
  		.CapDerating[5] = 70,   /* capacity derating in 0.1%, for temp = -10°C */
		.CapDerating[4] = 30,   /* capacity derating in 0.1%, for temp = 0°C */
		.CapDerating[3] = 0,   /* capacity derating in 0.1%, for temp = 10°C */
		.CapDerating[2] = 0,   /* capacity derating in 0.1%, for temp = 25°C */
		.CapDerating[1] = -20,   /* capacity derating in 0.1%, for temp = 40°C */
		.CapDerating[0] = -40,   /* capacity derating in 0.1%, for temp = 60°C */

  		.OCVOffset[15] = -123,    /* OCV curve adjustment */
		.OCVOffset[14] = -30,   /* OCV curve adjustment */
		.OCVOffset[13] = -12,    /* OCV curve adjustment */
		.OCVOffset[12] = -27,    /* OCV curve adjustment */
		.OCVOffset[11] = 0,    /* OCV curve adjustment */
		.OCVOffset[10] = -27,    /* OCV curve adjustment */
		.OCVOffset[9] = 4,     /* OCV curve adjustment */
		.OCVOffset[8] = 1,      /* OCV curve adjustment */
		.OCVOffset[7] = 7,      /* OCV curve adjustment */
		.OCVOffset[6] = 9,    /* OCV curve adjustment */
		.OCVOffset[5] = 9,    /* OCV curve adjustment */
		.OCVOffset[4] = 16,     /* OCV curve adjustment */
		.OCVOffset[3] = 33,    /* OCV curve adjustment */
		.OCVOffset[2] = 34,     /* OCV curve adjustment */
		.OCVOffset[1] = 46,    /* OCV curve adjustment */
		.OCVOffset[0] = -3,     /* OCV curve adjustment */
		
		 .OCVOffset2[15] = -109,    /* OCV curve adjustment */
		.OCVOffset2[14] = -86,   /* OCV curve adjustment */
		.OCVOffset2[13] = -59,    /* OCV curve adjustment */
		.OCVOffset2[12] = -59,    /* OCV curve adjustment */
		.OCVOffset2[11] = -29,    /* OCV curve adjustment */
		.OCVOffset2[10] = -46,    /* OCV curve adjustment */
		.OCVOffset2[9] = -8,     /* OCV curve adjustment */
		.OCVOffset2[8] = 0,      /* OCV curve adjustment */
		.OCVOffset2[7] = -2,      /* OCV curve adjustment */
		.OCVOffset2[6] = -6,    /* OCV curve adjustment */
		.OCVOffset2[5] = -7,    /* OCV curve adjustment */
		.OCVOffset2[4] = -9,     /* OCV curve adjustment */
		.OCVOffset2[3] = 19,    /* OCV curve adjustment */
		.OCVOffset2[2] = 44,     /* OCV curve adjustment */
		.OCVOffset2[1] = 81,    /* OCV curve adjustment */
		.OCVOffset2[0] = 0,     /* OCV curve adjustment */
		
		/*if the application temperature data is preferred than the STC3115 temperature*/
		.ExternalTemperature = NULL, /*External temperature fonction, return C*/
		.ForceExternalTemperature = 0, /* 1=External temperature, 0=STC3115 temperature */
};
static struct i2c_gpio_platform_data i2c_stc3115_bus_data = {
      .sda_pin = mfp_to_gpio(MFP_PIN_GPIO88),
	  .scl_pin = mfp_to_gpio(MFP_PIN_GPIO87), 
	  .udelay  = 3,  //// brian :3
	  .timeout = 100,
};
static struct platform_device i2c_stc3115_bus_device = {
	.name	= "i2c-gpio",
	.id		= 6,
	.dev		= {
		.platform_data = &i2c_stc3115_bus_data,
	}
};
static struct i2c_board_info __initdata stc3115_i2c_devices[] = {
	{
		.type		= "stc3115_fuelgauge",
		.addr		= 0x70,
		.platform_data = &stc3115_platform_data,
	},
};
static struct pxa_i2c_board_gpio stc3115_i2c_gpio[] = {
	{
		.type = "stc3115_fuelgauge",
		.gpio = mfp_to_gpio(GPIO095_GPIO_95),
	},
};
#endif

#if defined(CONFIG_TOUCHSCREEN_MMS136_TS)
#define TSP_SDA	mfp_to_gpio(GPIO017_GPIO_17)
#define TSP_SCL	mfp_to_gpio(GPIO016_GPIO_16)
#define TSP_INT	mfp_to_gpio(GPIO094_GPIO_94)
static struct i2c_gpio_platform_data i2c_mms136_bus_data = {
	.sda_pin = TSP_SDA,
	.scl_pin = TSP_SCL,
	.udelay  = 3,
	.timeout = 100,
};
static struct platform_device i2c_mms136_bus_device = {
	.name		= "i2c-gpio",
	.id		= 4,
	.dev = {
		.platform_data = &i2c_mms136_bus_data,
	}
};

static struct i2c_board_info __initdata mms136_i2c_devices[] = {
	{
		I2C_BOARD_INFO("mms_ts", 0x48),
	},
};

static struct pxa_i2c_board_gpio mms136_i2c_gpio[] = {
	{
		.type = "mms_ts",
		.gpio = TSP_INT,
	},
};

#endif

#if defined(CONFIG_SPA)
static struct spa_platform_data spa_info = {
	.use_fuelgauge = 1,
	.battery_capacity = 1650,
	.VF_low	= 50,
	.VF_high = 600,
}; 
static struct platform_device Sec_BattMonitor = {
	.name		= "Sec_BattMonitor",
	.id		= -1, 
	.dev		= {
		.platform_data = &spa_info,
	},		
};
#endif
static struct i2c_board_info helandkb_i2c_info[] = {
};

#ifdef CONFIG_PN547_NFC
static struct pn544_i2c_platform_data pn547_pdata = {
//	.conf_gpio = pn544_conf_gpio,
	.irq_gpio = mfp_to_gpio(GPIO030_GPIO_NFC_IRQ),
	.ven_gpio = mfp_to_gpio(GPIO070_NFC_EN),
	.firm_gpio = mfp_to_gpio(GPIO071_NFC_FIRMWARE),
};
#endif
static struct i2c_board_info helandkb_i2c2_info[] = {
#ifdef CONFIG_PN547_NFC
	{
		I2C_BOARD_INFO("pn547", 0x2b),
		.irq = MMP_GPIO_TO_IRQ(mfp_to_gpio(GPIO030_GPIO_NFC_IRQ)),
		.platform_data = &pn547_pdata,
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
		GPIO079_GPIO_79,		/* SCL */
		GPIO080_GPIO_80,		/* SDA */
	};

	unsigned long i2c1_mfps[] = {
		GPIO087_GPIO_87,		/* SCL */
		GPIO088_GPIO_88,		/* SDA */
	};
	if (i2c_adap_id == 0) {
		scl = MFP_PIN_GPIO79;
		sda = MFP_PIN_GPIO80;
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
		mfp_write(MFP_PIN_GPIO79, mfp_pin[0]);
		mfp_write(MFP_PIN_GPIO80, mfp_pin[1]);
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

static struct i2c_pxa_platform_data helandkb_ci2c_pdata = {
	.fast_mode		 = 1,
	/* ilcr:fs mode b17~9=0x23,about 390K, standard mode b8~0=0x9f,97K */
	.ilcr		= 0x082C469F,
	/* iwcr:b5~0=b01010 recommended value according to spec*/
	.iwcr		= 0x0000142A,
	.i2c_bus_reset		= i2c_pxa_bus_reset,
};

static struct i2c_pxa_platform_data helandkb_ci2c2_pdata = {
	.fast_mode		 = 1,
	/* ilcr:fs mode b17~9=0x23,about 390K, standard mode b8~0=0x9f,97K */
	.ilcr		= 0x082C469F,
	/* iwcr:b5~0=b01010 recommended value according to spec*/
	.iwcr		= 0x0000142A,
	.i2c_bus_reset		= i2c_pxa_bus_reset,
};

static struct i2c_pxa_platform_data helandkb_pwr_i2c_pdata = {
	.fast_mode		 = 1,
	/* ilcr:fs mode b17~9=0x23,about 390K, standard mode b8~0=0x9f,97K */
	.ilcr		= 0x082C469F,
	/* iwcr:b5~0=b01010 recommended value according to spec*/
	.iwcr		= 0x0000142A,
	.hardware_lock		= pxa988_ripc_lock,
	.hardware_unlock	= pxa988_ripc_unlock,
	.hardware_trylock	= pxa988_ripc_trylock,
};

static DEFINE_SPINLOCK(bl_lock);

#if defined(CONFIG_BACKLIGHT_KTD253)
#include <linux/ktd_bl.h>
struct brt_value brt_table_ktd[] = {
	{ 255,	4 }, /* Max */
	{ 245,	6 },
	{ 235,	8 },
	{ 225,	10 },
	{ 215,	12 },
	{ 200,	14 },
	{ 185,	16 },
	{ 170,	17 },
	{ 155,	18 },
	{ 140,	19 },
	{ 133,	20 },
	{ 125,	21 }, /* default */
	{ 110,	23 },
	{ 95,	25 },
	{ 80,	27 },
	{ 70,	28 },
	{ 60,	29 },
	{ 50,	30 },
	{ 40,	31 },
	{ 30,	32 }, /* Min */
	{ 20,	32 }, /* Dimming */
	{ 0, 0 }, /* Off */
};

static struct ktd_bl_info ktd_lcd_backlight_data = {
	.name = "ktd253-bl",
	.max_brightness = 255,
	.min_brightness = 20,
	.def_brightness = 125,
	.gpio_bl_ctrl = 9,
	.gpio_bl_pwm_en = -1,
	.brt_table = ktd_brt_table,
	.sz_table = ARRAY_SIZE(ktd_brt_table),
};

static struct platform_device ktd_device  = {
	.name	= "panel",
	.dev = {
		.platform_data = &ktd_lcd_backlight_data,
	},
};
#else
static void helan_dkb_set_bl(int intensity)
{
	int gpio_bl, bl_level, p_num;
	unsigned long flags;
	/* 
	 * FIXME
	 * the initial value of bl_level_last is the UBOOT  backlight level, 
	 * It should be aligned with uboot. For example,
      	 *  If uboot uses 100% backlight level, bl_level_last = 33;
	 *  If uboot uses 50% backlight level, bl_level_last = 17;
	 */
	static int bl_level_last = 17; 

	gpio_bl = mfp_to_gpio(GPIO09_LCD_BL_CTRL);
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

	pr_info("%s : %d pulse for BL\n", __func__, p_num);
	while (p_num--) {
		spin_lock_irqsave(&bl_lock, flags);
		gpio_direction_output(gpio_bl, 0);
		udelay(1);
		gpio_direction_output(gpio_bl, 1);
		spin_unlock_irqrestore(&bl_lock, flags);
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

static struct generic_bl_info helan_dkb_lcd_backlight_data = {
	.name = "lcd-bl",
	.max_intensity = 100,
	.default_intensity = 50,
	.set_bl_intensity = helan_dkb_set_bl,
};

static struct platform_device helan_dkb_lcd_backlight_devices = {
	.name = "generic-bl",
	.dev = {
		.platform_data = &helan_dkb_lcd_backlight_data,
	},
};
#endif


#ifdef CONFIG_USB_MV_UDC
static char *pxa988_usb_clock_name[] = {
	[0] = "UDCCLK",
};

static struct mv_usb_platform_data helandkb_usb_pdata = {
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
#define MFP_WIB_PDn		(GPIO067_GPIO_67 | MFP_PULL_FLOAT)
#define MFP_WIB_RESETn		0//(GPIO034_GPIO_34 | MFP_PULL_FLOAT)

static void helandkb_sdcard_signal(unsigned int set)
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
static void helandkb_8787_set_power(unsigned int on)
{
	static struct regulator *wib_3v3;
	static int enabled;


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
		regulator_set_voltage(wib_3v3, 3300000, 3300000);
		regulator_enable(wib_3v3);
		enabled = 1;

		/* Only when SD8787 are active (power on),
		 * it is meanful to enable the edge wakeup
		 */
		if (has_feat_sdio_edge_wakeup_only())
			mmp_gpio_edge_add(&gpio_edge_sdio_dat1);
		/*disable buck2 sleep mode when wifi power on*/
		pm800_extern_setbits(PM80X_POWER_PAGE, PM800_BUCK_SLP1,
					PM800_BUCK2_SLP1_MASK, PM800_BUCK2_SLP1_MASK);
	}

	if (!on && enabled) {
//		regulator_disable(wib_3v3);
		enabled = 0;

		if (has_feat_sdio_edge_wakeup_only())
			mmp_gpio_edge_del(&gpio_edge_sdio_dat1);
		/*enable buck2 sleep mode when wifi power off*/
		pm800_extern_setbits(PM80X_POWER_PAGE, PM800_BUCK_SLP1,
					PM800_BUCK2_SLP1_MASK, PM800_BUCK2_SLP1_UNMASK);
	}
}
#endif

static struct sdhci_pxa_dtr_data sd_dtr_data[] = {
	{
		.timing		= MMC_TIMING_LEGACY, /* < 25MHz */
		.preset_rate	= PXA_SDH_DTR_26M,
		.src_rate	= PXA_SDH_DTR_52M,
	},
	{
		.timing		= MMC_TIMING_UHS_SDR12, /* 25MHz */
		.preset_rate	= PXA_SDH_DTR_26M,
		.src_rate	= PXA_SDH_DTR_104M,
	},
	{
		.timing		= MMC_TIMING_UHS_SDR25, /* 50MHz */
		.preset_rate	= PXA_SDH_DTR_52M,
		.src_rate	= PXA_SDH_DTR_104M,
	},
	{
		.timing		= MMC_TIMING_SD_HS, /* 50MHz */
		.preset_rate	= PXA_SDH_DTR_52M,
		.src_rate	= PXA_SDH_DTR_104M,
	},
	{
		.timing		= MMC_TIMING_UHS_DDR50, /* 50MHz */
		.preset_rate	= PXA_SDH_DTR_52M,
		.src_rate	= PXA_SDH_DTR_104M,
	},
	{
		.timing		= MMC_TIMING_UHS_SDR50, /* 100MHz */
		.preset_rate	= PXA_SDH_DTR_104M,
		.src_rate	= PXA_SDH_DTR_208M,
	},
	{
		.timing		= MMC_TIMING_UHS_SDR104, /* 208MHz */
		.preset_rate	= PXA_SDH_DTR_208M,
		.src_rate	= PXA_SDH_DTR_416M,
	},
		/*
		 * end of sdhc dtr table
		 * set as the default src rate
		 */
	{
		.timing		= MMC_TIMING_MAX,
		.preset_rate	= PXA_SDH_DTR_PS_NONE,
		.src_rate	= PXA_SDH_DTR_208M,
	},
};

static struct sdhci_pxa_dtr_data sdio_dtr_data[] = {
	{
		.timing		= MMC_TIMING_LEGACY, /* < 25MHz */
		.preset_rate	= PXA_SDH_DTR_26M,
		.src_rate	= PXA_SDH_DTR_52M,
	},
	{
		.timing		= MMC_TIMING_UHS_SDR12, /* 25MHz */
		.preset_rate	= PXA_SDH_DTR_26M,
		.src_rate	= PXA_SDH_DTR_104M,
	},
	{
		.timing		= MMC_TIMING_UHS_SDR25, /* 50MHz */
		.preset_rate	= PXA_SDH_DTR_52M,
		.src_rate	= PXA_SDH_DTR_104M,
	},
	{
		.timing		= MMC_TIMING_SD_HS, /* 50MHz */
		.preset_rate	= PXA_SDH_DTR_45M,
		.src_rate	= PXA_SDH_DTR_89M,
	},
	{
		.timing		= MMC_TIMING_UHS_DDR50, /* 50MHz */
		.preset_rate	= PXA_SDH_DTR_52M,
		.src_rate	= PXA_SDH_DTR_104M,
	},
	{
		.timing		= MMC_TIMING_UHS_SDR50, /* 100MHz */
		.preset_rate	= PXA_SDH_DTR_104M,
		.src_rate	= PXA_SDH_DTR_208M,
	},
	{
		.timing		= MMC_TIMING_UHS_SDR104, /* 208MHz */
		.preset_rate	= PXA_SDH_DTR_208M,
		.src_rate	= PXA_SDH_DTR_416M,
	},
	{
		.timing		= MMC_TIMING_MAX,
		.preset_rate	= PXA_SDH_DTR_PS_NONE,
		.src_rate	= PXA_SDH_DTR_89M,
	},
};
static struct sdhci_pxa_dtr_data emmc_dtr_data[] = {
	{
		.timing		= MMC_TIMING_LEGACY, /* < 25MHz */
		.preset_rate	= PXA_SDH_DTR_26M,
		.src_rate	= PXA_SDH_DTR_52M,
	},
	{
		.timing		= MMC_TIMING_MMC_HS, /* 50MHz */
		.preset_rate	= PXA_SDH_DTR_52M,
		.src_rate	= PXA_SDH_DTR_104M,
	},
	{
		.timing		= MMC_TIMING_UHS_DDR50, /* 50MHz */
		.preset_rate	= PXA_SDH_DTR_52M,
		.src_rate	= PXA_SDH_DTR_104M,
		.tx_delay	= 0xd4,
	},
	{
		.timing		= MMC_TIMING_MMC_HS200, /* 208MHz */
		.preset_rate	= PXA_SDH_DTR_156M,
		.src_rate	= PXA_SDH_DTR_156M,
	},
	{
		.timing		= MMC_TIMING_MAX,
		.preset_rate	= PXA_SDH_DTR_PS_NONE,
		.src_rate	= PXA_SDH_DTR_208M,
	},
};



/* For emeiDKB, MMC1(SDH1) used for SD/MMC Card slot */
static struct sdhci_pxa_platdata pxa988_sdh_platdata_mmc1 = {
	.flags		= PXA_FLAG_ENABLE_CLOCK_GATING
			| PXA_FLAG_TX_SEL_BUS_CLK,
	.cd_type         = PXA_SDHCI_CD_GPIO,
	.clk_delay_cycles	= 0x1F,
	.host_caps_disable	= MMC_CAP_UHS_SDR104,
	.quirks			= SDHCI_QUIRK_INVERTED_WRITE_PROTECT,
	.quirks2                = SDHCI_QUIRK2_BUS_CLK_GATE_ENABLED,
	.signal_vol_change	= helandkb_sdcard_signal,
	.ext_cd_gpio         = mfp_to_gpio(MFP_PIN_GPIO89),
	.ext_cd_gpio_invert =1,
	.dtr_data		= sd_dtr_data,
};

/* For helanDKB, MMC2(SDH2) used for WIB card */
static struct sdhci_pxa_platdata pxa988_sdh_platdata_mmc2 = {
	.flags          = PXA_FLAG_WAKEUP_HOST
				| PXA_FLAG_ENABLE_CLOCK_GATING
				| PXA_FLAG_TX_SEL_BUS_CLK
				| PXA_FLAG_EN_PM_RUNTIME
				| PXA_FLAG_DISABLE_PROBE_CDSCAN,
	.cd_type	= PXA_SDHCI_CD_EXTERNAL,
	.quirks2	= SDHCI_QUIRK2_BUS_CLK_GATE_ENABLED
			| SDHCI_QUIRK2_SDIO_SW_CLK_GATE,
	.pm_caps	= MMC_PM_KEEP_POWER,
	.dtr_data	= sdio_dtr_data,
};

/* For helanDKB, MMC3(SDH3) used for eMMC */
static struct sdhci_pxa_platdata pxa988_sdh_platdata_mmc3 = {
	.flags		= PXA_FLAG_ENABLE_CLOCK_GATING
				| PXA_FLAG_TX_SEL_BUS_CLK
				| PXA_FLAG_SD_8_BIT_CAPABLE_SLOT
				| PXA_FLAG_EN_PM_RUNTIME,
	.cd_type	= PXA_SDHCI_CD_PERMANENT,
	.quirks2	= SDHCI_QUIRK2_BUS_CLK_GATE_ENABLED,
	.clk_delay_cycles	= 0xF,
	.host_caps	= MMC_CAP_1_8V_DDR,
	.dtr_data	= emmc_dtr_data,
};

static unsigned int vpu_feature;

static void __init helandkb_init_mmc(void)
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
			helandkb_8787_set_power);
#endif

	/*
	 * Note!!
	 *  The regulator can't be used here, as this is called in arch_init
	 */

	/* HW MMC3(sdh3) used for eMMC, and register first */
	pxa988_add_sdh(3, &pxa988_sdh_platdata_mmc3);

	/* HW MMC1(sdh1) used for SD/MMC card */
	pxa988_sdh_platdata_mmc1.flags	 = PXA_FLAG_EN_PM_RUNTIME
				| PXA_FLAG_ENABLE_CLOCK_GATING;

	pxa988_add_sdh(1, &pxa988_sdh_platdata_mmc1);

	/* HW MMC2(sdh2) used for SDIO(WIFI/BT/FM module), and register last */
	pxa988_add_sdh(2, &pxa988_sdh_platdata_mmc2);
	wlan_wakeup_init();
}
#else
static void __init helandkb_init_mmc(void)
{

}
#endif /* CONFIG_MMC_SDHCI_PXAV3 */


#if defined(CONFIG_VIDEO_MMP)
static int pxa988_cam_clk_init(struct device *dev)
{
	struct mmp_cam_pdata *data = dev->platform_data;
	int ret;

	data->clk[0] = devm_clk_get(dev, "CCICFUNCLK");
	if (IS_ERR(data->clk[0])) {
		dev_err(dev, "Could not get function clk\n");
		ret = PTR_ERR(data->clk[0]);
		return ret;
	}

	data->clk[1] = devm_clk_get(dev, "CCICAXICLK");
	if (IS_ERR(data->clk[1])) {
		dev_err(dev, "Could not get AXI clk\n");
		ret = PTR_ERR(data->clk[1]);
		return ret;
	}

	data->clk[2] = devm_clk_get(dev, "LCDCIHCLK");
	if (IS_ERR(data->clk[2])) {
		dev_err(dev, "Could not get lcd/ccic AHB clk\n");
		ret = PTR_ERR(data->clk[2]);
		return ret;
	}

	data->clk[3] = devm_clk_get(dev, "CCICPHYCLK");
	if (IS_ERR(data->clk[3])) {
		dev_err(dev, "Could not get PHY clk\n");
		ret = PTR_ERR(data->clk[3]);
		return ret;
	}

	return 0;
}

static void pxa988_cam_set_clk(struct device *dev, int on)
{
	struct mmp_cam_pdata *data = dev->platform_data;
	int mipi = on & MIPI_ENABLE;

	on &= POWER_ON;
	if (on) {
		clk_enable(data->clk[2]);
		clk_enable(data->clk[1]);
		clk_enable(data->clk[0]);
		if (mipi)
			clk_enable(data->clk[3]);
	} else {
		if (mipi)
			clk_disable(data->clk[3]);
		clk_disable(data->clk[0]);
		clk_disable(data->clk[1]);
		clk_disable(data->clk[2]);
	}
}
#endif
#if 0
struct mmp_cam_pdata mv_cam_data = {
	.name = "EMEI",
	.dma_burst = 64,
	.mclk_min = 24,
	.mclk_src = 3,
	.mclk_div = 13,
//	.init_pin = pxa988_cam_pin_init,
	.init_clk = pxa988_cam_clk_init,
	.enable_clk = pxa988_cam_set_clk,
};

static struct i2c_board_info dkb_i2c_camera[] = {
	{
		I2C_BOARD_INFO("ov2659", 0x30),
	},
	{
		I2C_BOARD_INFO("ov5640", 0x3c),
	},
};


static int camera_sensor_power(struct device *dev, int on)
{
	unsigned int cam_pwr;
	unsigned int cam_reset;
	static struct regulator *v_sensor;

	if (!v_sensor) {
		v_sensor = regulator_get(NULL, "v_cam_avdd");
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

static struct sensor_board_data ov2659_data = {
	.mount_pos	= SENSOR_USED | SENSOR_POS_FRONT | SENSOR_RES_LOW,
	.bus_type	= V4L2_MBUS_PARALLEL,
	.bus_flag	= 0,	/* EMEI DKB connection don't change polarity of
				 * PCLK/HSYNC/VSYNC signal */
	.plat		= &mv_cam_data,
};

static struct soc_camera_link iclink_ov2659_dvp = {
	.bus_id         = 0,            /* Must match with the camera ID */
	.power          = camera_sensor_power,
	.board_info     = &dkb_i2c_camera[0],
	.i2c_adapter_id = 0,
	.module_name    = "ov2659",
	.priv		= &ov2659_data,
	.flags		= 0,	/* controller driver should copy priv->bus_flag
				 * here, so soc_camera_apply_board_flags can
				 * take effect */
};

static struct platform_device dkb_ov2659_dvp = {
	.name   = "soc-camera-pdrv",
	.id     = 0,
	.dev    = {
		.platform_data = &iclink_ov2659_dvp,
	},
};

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

	switch (on) {
	case POWER_OFF:
		gpio_direction_output(cam_reset, 0);
		mdelay(1);
		gpio_direction_output(cam_reset, 1);

		regulator_disable(avdd);
		regulator_disable(af_vcc);

		gpio_direction_output(pwdn, 1);
		mdelay(1);
		break;
	case POWER_ON:
		regulator_set_voltage(af_vcc, 2800000, 2800000);
		regulator_enable(af_vcc);
		regulator_set_voltage(avdd, 2800000, 2800000);
		regulator_enable(avdd);
		mdelay(5);
		gpio_direction_output(pwdn, 0);
		mdelay(1);
		gpio_direction_output(cam_reset, 0);
		mdelay(1);
		gpio_direction_output(cam_reset, 1);
		mdelay(20);
		break;
	case POWER_SAVING:
		gpio_direction_output(pwdn, 1);
		mdelay(1);
		break;
	case POWER_RESTORE:
		gpio_direction_output(pwdn, 0);
		mdelay(1);
		break;
	default:
		dev_err(dev, "unknown sensor power operation!\n");
		break;
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

static struct sensor_board_data ov5640_data = {
	.mount_pos	= SENSOR_USED | SENSOR_POS_BACK | SENSOR_RES_HIGH,
	.bus_type	= V4L2_MBUS_CSI2,
	.bus_flag	= V4L2_MBUS_CSI2_2_LANE, /* ov5640 used 2 lanes */
	.dphy		= {0x0D06, 0x33, 0x0900},
	.mipi_enabled	= 0,
	.plat		= &mv_cam_data,
};

static struct soc_camera_link iclink_ov5640_mipi = {
	.bus_id         = 0,            /* Must match with the camera ID */
	.power          = ov5640_sensor_power,
	.board_info     = &dkb_i2c_camera[1],
	.i2c_adapter_id = 0,
	.module_name    = "ov5640",
	.priv		= &ov5640_data,
};

static struct platform_device dkb_ov5640_mipi = {
	.name   = "soc-camera-pdrv",
	.id     = 1,
	.dev    = {
		.platform_data = &iclink_ov5640_mipi,
	},
};
#endif
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

	devfreq_ddr_pdata.qos_list = ddr_freq_qos_table;

	pxa988_device_ddrdevfreq.dev.platform_data = (void *)&devfreq_ddr_pdata;
	platform_device_register(&pxa988_device_ddrdevfreq);
}
#endif

/* clk usage desciption */
MMP_HW_DESC(fb, "pxa168-fb", 0, PM_QOS_CPUIDLE_BLOCK_DDR_VALUE, "LCDCIHCLK");
struct mmp_hw_desc *helan_dkb_hw_desc[] __initdata = {
	&mmp_device_hw_fb,
};

#ifdef CONFIG_PROC_FS

static int gps_enable_control(int flag)
{
        static struct regulator *gps_regulator = NULL;
        static int f_enabled = 0;
        printk("[GPS] LDO control : %s\n", flag ? "ON" : "OFF");

#ifdef GPS_LDO_POWER
        if (flag && (!f_enabled)) {
                      gps_regulator = regulator_get(NULL, "v_gps_1v8");
                      if (IS_ERR(gps_regulator)) {
                                   gps_regulator = NULL;
                                   return EIO;
                      } else {
                                   regulator_set_voltage(gps_regulator, 1800000, 1800000);
                                   regulator_enable(gps_regulator);
                      }
                      f_enabled = 1;
        }

        if (f_enabled && (!flag))
        {
                      if (gps_regulator) {
                                   regulator_disable(gps_regulator);
                                   regulator_put(gps_regulator);
                                   gps_regulator = NULL;
                      }
                      f_enabled = 0;
        }
#else
	int gps_power_en;

	gps_power_en = mfp_to_gpio(GPIO011_GPIO_11);
	if (!gpio_request(gps_power_en, "GPS_POWER")) {
		gpio_direction_output(gps_power_en, flag);
		gpio_free(gps_power_en);
	}
#endif

	return 0;
}

/* GPS: power on/off control */
static void gps_power_on(void)
{
	unsigned int gps_rst_n,gps_on;


	gps_rst_n = mfp_to_gpio(GPIO053_AP_AGPS_RESET);
	if (gpio_request(gps_rst_n, "gpio_gps_rst")) {
		pr_err("Request GPIO failed, gpio: %d\n", gps_rst_n);
		return;
	}
	gps_on = mfp_to_gpio(GPIO068_AP_AGPS_ONOFF);
	if (gpio_request(gps_on, "gpio_gps_on")) {
		pr_err("Request GPIO failed,gpio: %d\n", gps_on);
		goto out;
	}

	gpio_direction_output(gps_rst_n, 0);
	gpio_direction_output(gps_on, 0);
	gps_enable_control(1);
	mdelay(10);
	gpio_direction_output(gps_rst_n, 1);
	mdelay(10);
	gpio_direction_output(gps_on, 1);

	pr_info("gps chip powered on\n");

	gpio_free(gps_on);
out:
	gpio_free(gps_rst_n);
	return;
}

static void gps_power_off(void)
{
	unsigned int gps_rst_n, gps_on;


	/* hardcode */

	gps_on = mfp_to_gpio(GPIO068_AP_AGPS_ONOFF);
	if (gpio_request(gps_on, "gpio_gps_on")) {
		pr_err("Request GPIO failed,gpio: %d\n", gps_on);
		return;
	}

	gps_rst_n = mfp_to_gpio(GPIO053_AP_AGPS_RESET);
	if (gpio_request(gps_rst_n, "gpio_gps_rst")) {
		pr_debug("Request GPIO failed, gpio: %d\n", gps_rst_n);
		goto out2;
	}

	gpio_direction_output(gps_rst_n, 0);
	gpio_direction_output(gps_on, 0);
	gps_enable_control(0);

	pr_info("gps chip powered off\n");

	gpio_free(gps_rst_n);
out2:
	gpio_free(gps_on);
	return;
}

static void gps_reset(int flag)
{
	unsigned int gps_rst_n;

	gps_rst_n = mfp_to_gpio(GPIO053_AP_AGPS_RESET);
	if (gpio_request(gps_rst_n, "gpio_gps_rst")) {
		pr_err("Request GPIO failed, gpio: %d\n", gps_rst_n);
		return;
	}

	gpio_direction_output(gps_rst_n, flag);
	gpio_free(gps_rst_n);
	printk(KERN_INFO "gps chip reset with %s\n", flag ? "ON" : "OFF");
}

static void gps_on_off(int flag)
{
	unsigned int gps_on;

	gps_on = mfp_to_gpio(GPIO068_AP_AGPS_ONOFF);
	if (gpio_request(gps_on, "gpio_gps_on")) {
		pr_err("Request GPIO failed, gpio: %d\n", gps_on);
		return;
	}

	gpio_direction_output(gps_on, flag);
	gpio_free(gps_on);
	printk(KERN_INFO "gps chip onoff with %s\n", flag ? "ON" : "OFF");
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
	} else
		pr_info("usage: echo {on/off} > /proc/driver/sirf\n");

	return len;
}

static void create_sirf_proc_file(void)
{
	struct proc_dir_entry *sirf_proc_file = NULL;

	/*
	 * CSR and Marvell GPS lib will both use this file
	 * "/proc/drver/gps" may be modified in future
	 */
	sirf_proc_file = create_proc_entry("driver/sirf", 0644, NULL);
	if (!sirf_proc_file) {
		pr_err("sirf proc file create failed!\n");
		return;
	}

	sirf_proc_file->read_proc = sirf_read_proc;
	sirf_proc_file->write_proc = (write_proc_t  *)sirf_write_proc;
}

static ssize_t pcm_mfp_read_proc(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	int mfp = mfp_read(mfp_to_gpio(GPIO025_GPIO_INPUT));

	mfp &= 0x7;

	sprintf(page, "GPIO25~28 are configured to %s\n",
			mfp ? "GSSP" : "GPIO input");

	return strlen(page) + 1;
}

static ssize_t pcm_mfp_write_proc(struct file *filp,
				const char *buff, size_t len, loff_t *off)
{
	char a;
	unsigned long i, j, gpio;
	unsigned long gssp_mfp[2][4] = {
		{
			GPIO025_GPIO_INPUT,
			GPIO026_GPIO_INPUT,
			GPIO027_GPIO_INPUT,
			GPIO028_GPIO_INPUT,
		},
		{
			GPIO025_GSSP_SCLK,	/* PCM_CLK */
			GPIO026_GSSP_SFRM,	/* PCM_SYNC */
			GPIO027_GSSP_TXD,	/* PCM_TXD */
			GPIO028_GSSP_RXD,	/* PCM_RXD */
		},
	};

	if (copy_from_user(&a, buff, 1))
		return -EINVAL;
	switch (a) {
	case '0':
		i = 0;
		pr_debug("Switch GPIO25~28 to GPIO input\n");
		break;
	case '1':
		i = 1;
		pr_debug("Switch GPIO25~28 to GSSP function\n");
		break;
	default:
		pr_err("[PCM_MFP] Error: invalid configuration\n");
		return len;
	}

	mfp_config(ARRAY_AND_SIZE(gssp_mfp[i]));

	for (j = 0; j < ARRAY_SIZE(gssp_mfp[i]); j++) {
		gpio = mfp_to_gpio(gssp_mfp[i][j]);
		gpio_request(gpio, NULL);
		gpio_direction_input(gpio);
		gpio_free(gpio);
	}

	return len;
}

static void create_pcm_mfp_proc_file(void)
{
	struct proc_dir_entry *proc_file = NULL;

	proc_file = create_proc_entry("driver/pcm_mfp", 0644, NULL);
	if (!proc_file) {
		pr_err("%s: create proc file failed\n", __func__);
		return;
	}

	proc_file->write_proc = (write_proc_t *)pcm_mfp_write_proc;
	proc_file->read_proc = (read_proc_t *)pcm_mfp_read_proc;

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
static void helan_dkb_poweroff(void)
{
	unsigned char data;
	pr_info("turning off power....\n");

	preempt_enable();

	/* save power off reason */
	pm800_extern_write(PM80X_BASE_PAGE, PMIC_GENERAL_USE_REGISTER, PMIC_GENERAL_USE_BOOT_BY_NONE);
			
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
		unsigned long code, void *p)
{
	char *cmd = p;
	unsigned char data;
	unsigned char pmic_download_register = 0;
	unsigned char pmic_register = 0;
    
	pr_info("reboot notifier: %s\n", cmd);
#if defined (CONFIG_MFD_88PM800)
	if (cmd && !strcmp(cmd, "recovery")) {
		data = pm800_extern_read(PM80X_BASE_PAGE, 0xef);
		pm800_extern_write(PM80X_BASE_PAGE, 0xef, data | 0x1);
	} else {
		data = pm800_extern_read(PM80X_BASE_PAGE, 0xef);
		pm800_extern_write(PM80X_BASE_PAGE, 0xef, data & 0xfe);
	}

	if (cmd) {
		if (!strcmp(cmd, "recovery"))
			pm800_extern_write(PM80X_BASE_PAGE, PMIC_GENERAL_USE_REGISTER, PMIC_GENERAL_USE_BOOT_BY_FULL_RESET);
		else if (!strcmp(cmd, "recovery_done"))
			pm800_extern_write(PM80X_BASE_PAGE, PMIC_GENERAL_USE_REGISTER, PMIC_GENERAL_USE_BOOT_BY_RECOVERY_DONE);
		else if (!strcmp(cmd, "arm11_fota"))
			pm800_extern_write(PM80X_BASE_PAGE, PMIC_GENERAL_USE_REGISTER, PMIC_GENERAL_USE_BOOT_BY_FOTA);
		else if (!strcmp(cmd, "alarm"))
			pm800_extern_write(PM80X_BASE_PAGE, PMIC_GENERAL_USE_REGISTER, PMIC_GENERAL_USE_BOOT_BY_RTC_ALARM);
		else if(!strcmp(cmd, "debug0x4f4c")) 
			pm800_extern_write(PM80X_BASE_PAGE, PMIC_GENERAL_USE_REGISTER, PMIC_GENERAL_USE_BOOT_BY_DEBUGLEVEL_LOW);
		else if(!strcmp(cmd, "debug0x494d")) 
			pm800_extern_write(PM80X_BASE_PAGE, PMIC_GENERAL_USE_REGISTER, PMIC_GENERAL_USE_BOOT_BY_DEBUGLEVEL_MID);
		else if(!strcmp(cmd, "debug0x4948")) 
			pm800_extern_write(PM80X_BASE_PAGE, PMIC_GENERAL_USE_REGISTER, PMIC_GENERAL_USE_BOOT_BY_DEBUGLEVEL_HIGH);
		else if (!strcmp(cmd, "GlobalActions restart"))
			pm800_extern_write(PM80X_BASE_PAGE, PMIC_GENERAL_USE_REGISTER, PMIC_GENERAL_USE_BOOT_BY_INTENDED_RESET);
#ifdef CONFIG_ONECHIP_DUAL_MODEM
		else if (!strncmp(cmd, "swsel",5))
			pm800_extern_write(PM80X_BASE_PAGE, PMIC_GENERAL_USE_REGISTER, PMIC_GENERAL_USE_BOOT_BY_SET_SWITCH_SEL | cmd[5]-'0');
#endif
		else if (!strcmp(cmd,"download"))
		{
			pmic_download_register = PMIC_GENERAL_DOWNLOAD_MODE_FUS;
			pmic_register =(pmic_download_register<<4)&0xF0;
			pm800_extern_write(PM80X_BASE_PAGE, PM800_USER_DATA6, pmic_register);
			pr_info("pmic_download_register FUS : %d ,read result : 0x%02x \n",
				pmic_download_register, pm800_extern_read(PM80X_BASE_PAGE, PM800_USER_DATA6));
		}
		else if (!strncmp(cmd,"sud", 3))
		{
			pmic_download_register = cmd[3] - '0';
			pmic_register = (pmic_download_register << 4) & 0xF0;
			pm800_extern_write(PM80X_BASE_PAGE, PM800_USER_DATA6, pmic_register);
			pr_info("pmic_download_register SUDDLMOD : %d, read result : 0x%02x \n",
				pmic_download_register, pm800_extern_read(PM80X_BASE_PAGE, PM800_USER_DATA6));
		}
		else
			pm800_extern_write(PM80X_BASE_PAGE, PMIC_GENERAL_USE_REGISTER, PMIC_GENERAL_USE_BOOT_BY_INTENDED_RESET);

	}
	else
		pm800_extern_write(PM80X_BASE_PAGE, PMIC_GENERAL_USE_REGISTER, PMIC_GENERAL_USE_BOOT_BY_INTENDED_RESET);


	if (code != SYS_POWER_OFF) {
		data = pm800_extern_read(PM80X_BASE_PAGE, 0xef);
		/* this bit is for charger server */
		pm800_extern_write(PM80X_BASE_PAGE, 0xef, data | 0x2);
	}

#elif defined (CONFIG_MFD_D2199)
	if (cmd && !strcmp(cmd, "recovery")) {
		d2199_extern_reg_read(D2199_GP_ID_1_REG, &data);
		d2199_extern_reg_write(D2199_GP_ID_1_REG, data | 0x1);
	} else {
		d2199_extern_reg_read(D2199_GP_ID_1_REG, &data);
		d2199_extern_reg_write(D2199_GP_ID_1_REG, data & 0xfe);
	}

	if (cmd) {
		if (!strcmp(cmd, "recovery"))
			d2199_extern_reg_write(D2199_GP_ID_0_REG, PMIC_GENERAL_USE_BOOT_BY_FULL_RESET);
		else if (!strcmp(cmd, "recovery_done"))
			d2199_extern_reg_write(D2199_GP_ID_0_REG, PMIC_GENERAL_USE_BOOT_BY_RECOVERY_DONE);
		else if (!strcmp(cmd, "arm11_fota"))
			d2199_extern_reg_write(D2199_GP_ID_0_REG, PMIC_GENERAL_USE_BOOT_BY_FOTA);
		else if (!strcmp(cmd, "alarm"))
			d2199_extern_reg_write(D2199_GP_ID_0_REG, PMIC_GENERAL_USE_BOOT_BY_RTC_ALARM);
		else if(!strcmp(cmd, "debug0x4f4c")) 
			d2199_extern_reg_write(D2199_GP_ID_0_REG, PMIC_GENERAL_USE_BOOT_BY_DEBUGLEVEL_LOW);
		else if(!strcmp(cmd, "debug0x494d")) 
			d2199_extern_reg_write(D2199_GP_ID_0_REG, PMIC_GENERAL_USE_BOOT_BY_DEBUGLEVEL_MID);
		else if(!strcmp(cmd, "debug0x4948")) 
			d2199_extern_reg_write(D2199_GP_ID_0_REG, PMIC_GENERAL_USE_BOOT_BY_DEBUGLEVEL_HIGH);
		else if (!strcmp(cmd, "GlobalActions restart"))
			d2199_extern_reg_write(D2199_GP_ID_0_REG, PMIC_GENERAL_USE_BOOT_BY_INTENDED_RESET);
		else if (!strcmp(cmd,"download"))
		{
			pr_info("pmic_download_register FUS : %d \n",pmic_download_register);
			pmic_download_register = PMIC_GENERAL_DOWNLOAD_MODE_FUS;
			pmic_register =(pmic_download_register<<4)&0xF0;
			d2199_extern_reg_write(D2199_GP_ID_1_REG, pmic_register);
		}
		else if (!strncmp(cmd,"sud", 3))
		{
			pmic_download_register = ((char*)cmd)[3] - '0';
			pmic_register = (pmic_download_register << 4) & 0xF0;
			d2199_extern_reg_write(D2199_GP_ID_1_REG, pmic_register);
			pr_info("pmic_download_register SUDDLMOD : %d \n", pmic_download_register);
		}
		else
			d2199_extern_reg_write(D2199_GP_ID_0_REG, PMIC_GENERAL_USE_BOOT_BY_INTENDED_RESET);

	}
	else
		d2199_extern_reg_write(D2199_GP_ID_0_REG, PMIC_GENERAL_USE_BOOT_BY_INTENDED_RESET);

	if (code != SYS_POWER_OFF) {
		d2199_extern_reg_read(D2199_GP_ID_1_REG, &data);
		/* this bit is for charger server */
		d2199_extern_reg_write(D2199_GP_ID_1_REG, data | 0x2);
	}
#endif
	return 0;
}

static struct notifier_block reboot_notifier = {
	.notifier_call = reboot_notifier_func,
};

static void __init helandkb_init(void)
{
	int i;

#ifdef CONFIG_SEC_DEBUG
	sec_debug_init();
#endif

	for (i = 0; i < ARRAY_SIZE(helan_dkb_hw_desc); i++)
		mmp_device_hw_register(helan_dkb_hw_desc[i]);

	mfp_config(ARRAY_AND_SIZE(helandkb_pin_config));

	if (dvc_flag)
		mfp_config(ARRAY_AND_SIZE(dvc_pin_config));


	pm_power_off = helan_dkb_poweroff;
	register_reboot_notifier(&reboot_notifier);

	/* Uart0, CBD to CP */
	pxa988_add_uart(0);

	/* Uart1, AP kernel console and debug */
	pxa988_add_uart(1);
	/* Uart2, GPS */
	pxa988_add_uart(2);

	pxa988_add_keypad(&helan_dkb_keypad_info);
	helandkb_init_mmc();


#ifdef CONFIG_ANDROID_RAM_CONSOLE
	platform_device_register(&pxa988_device_ramconsole);
#endif

	/* soc-rtc */
	platform_device_register(&pxa988_device_rtc);
	/* backlight */
#ifdef CONFIG_BACKLIGHT_KTD253
	platform_device_register(&ktd_device);
#else    
	platform_device_register(&helan_dkb_lcd_backlight_devices);
#endif    
	/* set pm800 dvc information,must before pm800 init */
	if (!dvc_flag)
		pm800_dvctable_init();
	else {
		pm80x_dvc.gpio_dvc = 0;
		pm80x_dvc.reg_dvc = 1;
		pm80x_dvc.set_dvc = dvc_set_voltage;
		pm80x_dvc.write_reg = PMUM_DVC_AP;
		pm80x_dvc.read_reg = PMUM_DVC_STATUS;
	}
      pxa_init_i2c_gpio_irq(ARRAY_AND_SIZE(helandkb_pwr_i2c_gpio),
			ARRAY_AND_SIZE(helandkb_pwr_i2c_info));
	pxa988_add_twsi(0, &helandkb_ci2c_pdata,
			ARRAY_AND_SIZE(helandkb_i2c_info));
	pxa988_add_twsi(1, &helandkb_ci2c2_pdata,
			ARRAY_AND_SIZE(helandkb_i2c2_info));
	pxa988_add_twsi(2, &helandkb_pwr_i2c_pdata,
			ARRAY_AND_SIZE(helandkb_pwr_i2c_info));


#if defined(CONFIG_SENSORS_GP2A002S)
	pxa_init_i2c_gpio_irq(ARRAY_AND_SIZE(i2c_gp2a_gpio),
				ARRAY_AND_SIZE(i2c_gp2a));
#endif
#if defined (CONFIG_STC3115_FUELGAUGE)
	 pxa_init_i2c_gpio_irq(ARRAY_AND_SIZE(stc3115_i2c_gpio),
				ARRAY_AND_SIZE(stc3115_i2c_devices));	
#endif		
#if defined(CONFIG_FSA9480_MICROUSB)
	 pxa_init_i2c_gpio_irq(ARRAY_AND_SIZE(fsa9480_i2c_gpio),
				ARRAY_AND_SIZE(fsa9480_i2c_devices));	
#endif
#if defined (CONFIG_TOUCHSCREEN_MMS136_TS)
	pr_info("add i2c device: TOUCHSCREEN MELFAS\n");
	platform_device_register(&i2c_mms136_bus_device);
	i2c_register_board_info(4, ARRAY_AND_SIZE(mms136_i2c_devices));

	pxa_init_i2c_gpio_irq(ARRAY_AND_SIZE(mms136_i2c_gpio),
			ARRAY_AND_SIZE(mms136_i2c_devices));
#endif
#if defined (CONFIG_STC3115_FUELGAUGE)
	platform_device_register(&i2c_stc3115_bus_device);
	i2c_register_board_info(6, ARRAY_AND_SIZE(stc3115_i2c_devices));
#endif
#if defined(CONFIG_FSA9480_MICROUSB)
	platform_device_register(&i2c_fsa9480_bus_device);
	i2c_register_board_info(7, ARRAY_AND_SIZE(fsa9480_i2c_devices));
#endif
	/* add audio device: sram, ssp2, squ(tdma), pxa-ssp, mmp-pcm */
	pxa988_add_asram(&pxa988_asram_info);
	pxa988_add_ssp(1);
	pxa988_add_ssp(4);
	platform_device_register(&pxa988_device_squ);
	platform_device_register(&pxa988_device_asoc_platform);
	platform_device_register(&pxa988_device_asoc_ssp1);
	platform_device_register(&pxa988_device_asoc_gssp);
	platform_device_register(&pxa988_device_asoc_pcm);
	platform_device_register(&helan_dkb_audio_device);

	/* off-chip devices */
	platform_add_devices(ARRAY_AND_SIZE(dkb_platform_devices));

#ifdef CONFIG_FB_PXA168
	emeidkb_add_lcd_mipi();
	if (has_feat_video_replace_graphics_dma())
		emeidkb_add_tv_out();
#endif

#ifdef CONFIG_UIO_CODA7542
	vpu_feature = VPU_FEATURE_BITMASK_SRAM(SRAM_INTERNAL);
	pxa_device_coda7542.dev.platform_data = &vpu_feature;
	pxa_register_coda7542();
#endif

#ifdef CONFIG_USB_MV_UDC
	pxa988_device_udc.dev.platform_data = &helandkb_usb_pdata;
	platform_device_register(&pxa988_device_udc);
#endif
#ifdef CONFIG_MACH_HELANDELOS
	/* For following apse codebase, I did not want to modify too many board-aruba files
	 *  so I try to add a little code to apply to apse's code, such as mv_cam_pdata init.
	 *  By Vincent Wan.
	*/
	mv_cam_data_forssg.init_clk = pxa988_cam_clk_init,
	mv_cam_data_forssg.enable_clk = pxa988_cam_set_clk,
	init_samsung_cam();
#endif
#if defined(CONFIG_SPA)
        platform_device_register(&Sec_BattMonitor);
#endif

#if defined(CONFIG_SENSORS_BMA2X2) || defined(CONFIG_SENSORS_BMM050) || defined(CONFIG_SENSORS_GP2A002S)
	platform_device_register(&i2c_gpio_device);
	//board_sensors_init();	
#endif
#if defined(CONFIG_SENSORS_BMA2X2)
	i2c_register_board_info(5, &i2c_bma2x2, 1);
#endif
#if defined(CONFIG_SENSORS_BMM050)
	i2c_register_board_info(5, &i2c_bmm050, 1);
#endif
#if defined(CONFIG_SENSORS_GP2A002S)
	i2c_register_board_info(5, &i2c_gp2a, 1);
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
	/* create proc for gps GPS control */
	create_sirf_proc_file();
	/* create proc for gssp mfp control */
	create_pcm_mfp_proc_file();
#endif
#if defined (CONFIG_SEC_MODEM_DELOS3GVIA)
	emeidkb_init_spi();
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

MACHINE_START(HELANDELOS, "PXA1088")
	.map_io		= mmp_map_io,
	.init_irq	= pxa988_init_irq,
	.timer		= &pxa988_timer,
	.reserve	= pxa988_reserve,
	.handle_irq	= gic_handle_irq,
	.init_machine	= helandkb_init,
	.restart	= mmp_arch_reset,
MACHINE_END
