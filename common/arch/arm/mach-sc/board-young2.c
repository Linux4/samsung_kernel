/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/setup.h>
#include <asm/mach/time.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <linux/irqchip/arm-gic.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/localtimer.h>
#include <linux/i2c.h>
//#include <linux/i2c/ft53x6_ts.h>
#include <linux/i2c/focaltech.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/input/matrix_keypad.h>
#include <mach/board.h>
#include <mach/serial_sprd.h>
#include <mach/adi.h>
#include <mach/adc.h>
#include <mach/pinmap.h>
#include <linux/irq.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include <mach/hardware.h>
#include <mach/kpd.h>
#include "devices.h"
#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#if defined(CONFIG_TOUCHSCREEN_IST30XXB)
#include <linux/input/ist30xxb.h>
#endif

#ifdef CONFIG_I2C_GPIO
#include <linux/i2c-gpio.h>
#endif

#include <mach/i2s.h>
#if defined  (CONFIG_SENSORS_K3DH) || defined(CONFIG_SENSORS_K2HH)
#include <linux/k3dh_dev.h>
#endif

#ifdef CONFIG_STC3115_FUELGAUGE
#include <linux/stc3115_battery.h>
#endif

#ifdef CONFIG_MFD_RT8973
#include <linux/i2c-gpio.h>
#include <linux/battery/sec_battery.h>
#include <linux/battery/sec_fuelgauge.h>
#include <linux/battery/sec_charger.h>
#include <linux/power_supply.h>
#endif

#if defined(CONFIG_SEC_NFC_IF_I2C)
#include <linux/nfc/sec_nfc_com.h>
#include <linux/gpio.h>
#endif

#if defined(CONFIG_SPRD_SIMPLE_MUIC)
#include <mach/simple_muic.h>
#endif
#include "board-young2.h"

extern void __init sci_reserve(void);
extern void __init sci_map_io(void);
extern void __init sci_init_irq(void);
extern void __init sci_timer_init(void);
extern int __init sci_clock_init(void);
extern int __init sci_regulator_init(void);
void charger_enable(int enable);
bool is_jig_on;
#ifdef CONFIG_ANDROID_RAM_CONSOLE
extern int __init sprd_ramconsole_init(void);
#endif
extern void __init dev_bluetooth_init();


#ifdef CONFIG_STC3117_FUELGAUGE
#include <linux/stc3117_battery.h>
#endif


#if defined(CONFIG_SPRD_SIMPLE_MUIC)
static struct smuic_platform_data smuic_info = {
	.usb_dm_gpio = 240,
	.usb_dp_gpio = 241,
};
static struct platform_device sprd_simple_muic = {
	.name		= "sprd_simple_muic",
	.id		= -1,
	.dev		= {
		.platform_data = &smuic_info,
	},
};
#endif

#ifdef CONFIG_STC3117_FUELGAUGE
int null_fn(void)
{
        return 0;                // for discharging status
}

int Temperature_fn(void)
{
	return (25);
}

static struct stc311x_platform_data stc3117_data = {
                .battery_online = NULL,
                .charger_online = null_fn,		// used in stc311x_get_status()
                .charger_enable = null_fn,		// used in stc311x_get_status()
                .power_supply_register = NULL,
                .power_supply_unregister = NULL,

		.Vmode= 0,       /*REG_MODE, BIT_VMODE 1=Voltage mode, 0=mixed mode */
		.Alm_SOC = 10,      /* SOC alm level %*/
		.Alm_Vbat = 3600,   /* Vbat alm level mV*/
		.CC_cnf = 248,      /* nominal CC_cnf, coming from battery characterisation*/
		.VM_cnf = 201,      /* nominal VM cnf , coming from battery characterisation*/
		.Rint = 200,			/* nominal internal impedance*/
  		.Cnom = 1200,       /* nominal capacity in mAh, coming from battery characterisation*/
		.Rsense = 10,       /* sense resistor mOhms*/
		.RelaxCurrent = 60, /* current for relaxation in mA (< C/20) */
		.Adaptive = 1,     /* 1=Adaptive mode enabled, 0=Adaptive mode disabled */

        /* Elentec Co Ltd Battery pack - 80 means 8% */
        .CapDerating[6] = 200,              /* capacity derating in 0.1%, for temp = -20C */
        .CapDerating[5] = 120,              /* capacity derating in 0.1%, for temp = -10C */
        .CapDerating[4] = 50,              /* capacity derating in 0.1%, for temp = 0C */
        .CapDerating[3] = 20,              /* capacity derating in 0.1%, for temp = 10C */
        .CapDerating[2] = 0,              /* capacity derating in 0.1%, for temp = 25C */
        .CapDerating[1] = 0,              /* capacity derating in 0.1%, for temp = 40C */
        .CapDerating[0] = 0,              /* capacity derating in 0.1%, for temp = 60C */

        .OCVValue[15] = 4153,            /* OCV curve adjustment */
        .OCVValue[14] = 4059,            /* OCV curve adjustment */
        .OCVValue[13] = 3977,            /* OCV curve adjustment */
        .OCVValue[12] = 3915,            /* OCV curve adjustment */
        .OCVValue[11] = 3888,            /* OCV curve adjustment */
        .OCVValue[10] = 3838,            /* OCV curve adjustment */
        .OCVValue[9] = 3795,             /* OCV curve adjustment */
        .OCVValue[8] = 3775,             /* OCV curve adjustment */
        .OCVValue[7] = 3767,             /* OCV curve adjustment */
        .OCVValue[6] = 3756,             /* OCV curve adjustment */
        .OCVValue[5] = 3738,             /* OCV curve adjustment */
        .OCVValue[4] = 3703,             /* OCV curve adjustment */
        .OCVValue[3] = 3676,             /* OCV curve adjustment */
        .OCVValue[2] = 3647,             /* OCV curve adjustment */
        .OCVValue[1] = 3566,             /* OCV curve adjustment */
        .OCVValue[0] = 3300,             /* OCV curve adjustment */

            /*if the application temperature data is preferred than the STC3117 temperature*/
        .ExternalTemperature = Temperature_fn, /*External temperature fonction, return C*/
        .ForceExternalTemperature = 0, /* 1=External temperature, 0=STC3117 temperature */

};
#endif
#ifdef CONFIG_STC3115_FUELGAUGE
#ifdef CONFIG_CHARGER_RT9532
extern int rt9532_get_charger_online(void);
#endif

static int Temperature_fn(void)
{
	return (25);
}

static struct stc311x_platform_data stc3115_data = {
	.battery_online = NULL,
#ifdef CONFIG_CHARGER_RT9532
	.charger_online = rt9532_get_charger_online,	// used in stc311x_get_status()
#else
	.charger_online = NULL,	// used in stc311x_get_status()
#endif
	.charger_enable = NULL,		// used in stc311x_get_status()
	.power_supply_register = NULL,
	.power_supply_unregister = NULL,

	.Vmode = 0,		/*REG_MODE, BIT_VMODE 1=Voltage mode, 0=mixed mode */
	.Alm_SOC = 15,		/* SOC alm level % */
	.Alm_Vbat = 3400,	/* Vbat alm level mV */
	.CC_cnf = 301,      /* nominal CC_cnf, coming from battery characterisation*/
	.VM_cnf = 339,      /* nominal VM cnf , coming from battery characterisation*/
	.Cnom = 1500,       /* nominal capacity in mAh, coming from battery characterisation*/
	.Rsense = 10,		/* sense resistor mOhms */
	.RelaxCurrent = 75, /* current for relaxation in mA (< C/20) */
	.Adaptive = 1,		/* 1=Adaptive mode enabled, 0=Adaptive mode disabled */

	/* Elentec Co Ltd Battery pack - 80 means 8% */
	.CapDerating[6] = 200,              /* capacity derating in 0.1%, for temp = -20C */
	.CapDerating[5] = 60,              /* capacity derating in 0.1%, for temp = -10C */
	.CapDerating[4] = 20,              /* capacity derating in 0.1%, for temp = 0C */
	.CapDerating[3] = 5,              /* capacity derating in 0.1%, for temp = 10C */
	.CapDerating[2] = 0,   /* capacity derating in 0.1%, for temp = 25C */
	.CapDerating[1] = 0,   /* capacity derating in 0.1%, for temp = 40C */
	.CapDerating[0] = 0,   /* capacity derating in 0.1%, for temp = 60C */

	.OCVOffset[15] = -78,             /* OCV curve adjustment */
	.OCVOffset[14] = -89,             /* OCV curve adjustment */
	.OCVOffset[13] = -56,             /* OCV curve adjustment */
	.OCVOffset[12] = -38,             /* OCV curve adjustment */
	.OCVOffset[11] = -39,             /* OCV curve adjustment */
	.OCVOffset[10] = -47,             /* OCV curve adjustment */
	.OCVOffset[9] = -24,              /* OCV curve adjustment */
	.OCVOffset[8] = -18,              /* OCV curve adjustment */
	.OCVOffset[7] = -13,              /* OCV curve adjustment */
	.OCVOffset[6] = -17,              /* OCV curve adjustment */
	.OCVOffset[5] = -19,              /* OCV curve adjustment */
	.OCVOffset[4] = -29,              /* OCV curve adjustment */
	.OCVOffset[3] = -48,              /* OCV curve adjustment */
	.OCVOffset[2] = -49,              /* OCV curve adjustment */
	.OCVOffset[1] = -25,              /* OCV curve adjustment */
	.OCVOffset[0] = 0,  /* OCV curve adjustment */

	.OCVOffset2[15] = -78,            /* OCV curve adjustment */
	.OCVOffset2[14] = -89,            /* OCV curve adjustment */
	.OCVOffset2[13] = -56,            /* OCV curve adjustment */
	.OCVOffset2[12] = -38,            /* OCV curve adjustment */
	.OCVOffset2[11] = -39,            /* OCV curve adjustment */
	.OCVOffset2[10] = -47,            /* OCV curve adjustment */
	.OCVOffset2[9] = -24,             /* OCV curve adjustment */
	.OCVOffset2[8] = -18,             /* OCV curve adjustment */
	.OCVOffset2[7] = -13,             /* OCV curve adjustment */
	.OCVOffset2[6] = -17,             /* OCV curve adjustment */
	.OCVOffset2[5] = -19,             /* OCV curve adjustment */
	.OCVOffset2[4] = -29,             /* OCV curve adjustment */
	.OCVOffset2[3] = -48,             /* OCV curve adjustment */
	.OCVOffset2[2] = -49,             /* OCV curve adjustment */
	.OCVOffset2[1] = -25,             /* OCV curve adjustment */
	.OCVOffset2[0] = 0,     /* OCV curve adjustment */

	/*if the application temperature data is preferred than the STC3115 temperature */
	.ExternalTemperature = Temperature_fn,	/*External temperature fonction, return C */
	.ForceExternalTemperature = 0,	/* 1=External temperature, 0=STC3115 temperature */
};
#endif

#define GPIO_SENSOR_SCL		(41)
#define GPIO_SENSOR_SDA		(42)


#if defined(CONFIG_SEC_NFC_IF_I2C)
#define GPIO_NFC_IRQ		(194)
#define GPIO_NFC_EN		(187)
#define GPIO_NFC_FIRMWARE	(195)
#define GPIO_NFC_SCL		(173)
#define GPIO_NFC_SDA		(174)
#define GPIO_NFC_CLKREQ		(169)
#define NFC_I2C_ADDR		(0x27)


static struct sec_nfc_platform_data s3fwrn5_pdata = {
	.irq = GPIO_NFC_IRQ,
	.ven = GPIO_NFC_EN,
	.firm = GPIO_NFC_FIRMWARE,
	.wake = GPIO_NFC_FIRMWARE,
#if defined(CONFIG_SEC_NFC_CLK_REQ)
	.clk_req = GPIO_NFC_CLKREQ,
#endif
};

static struct i2c_board_info __initdata seci2cnfc[] = {
	{
		I2C_BOARD_INFO(SEC_NFC_DRIVER_NAME, NFC_I2C_ADDR),
		.platform_data = &s3fwrn5_pdata,
		.irq = GPIO_NFC_IRQ,
	},
};

#if defined(CONFIG_SEC_NFC_IF_I2C_GPIO)
static struct i2c_gpio_platform_data nfc_i2c_gpio_data = {
	.sda_pin = GPIO_NFC_SDA,
	.scl_pin = GPIO_NFC_SCL,
	.udelay	 = 2,
	.timeout = 100,
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0,
};

static struct platform_device nfc_i2c_gpio_device = {
	.name = "i2c-gpio",
	.id   = 7,
	.dev  = {
	.platform_data = &nfc_i2c_gpio_data,
	},
};
static struct platform_device *nfc_i2c_devices[] __initdata = {
	&nfc_i2c_gpio_device,
};
#endif	// CONFIG_SEC_NFC_IF_I2C_GPIO
#endif	// CONFIG_SEC_NFC_IF_I2C


#if defined(CONFIG_SEC_CHARGING_FEATURE)
#include <linux/wakelock.h>
#include <linux/spa_power.h>
#include <linux/spa_agent.h>

/* Samsung charging feature
 +++ for board files, it may contain changeable values */
static struct spa_temp_tb batt_temp_tb[] = {
	{3441 , -200},	/* -20 */
	{3290 , -150},	/* -15 */
	{3016 , -70},	/* -7  */
	{2937 , -50},	/* -5  */
	{2722 ,   0},	/* 0   */
	{2634 ,  20},	/* 2   */
	{2496 ,  50},	/* 5   */
	{2265 ,  100},	/* 10  */
	{2056 ,  150},	/* 15  */
	{1781 ,  200},	/* 20  */
	{1574 ,  250},	/* 25  */
	{1370 ,  300},	/* 30  */
	{1177 ,  350},	/* 35  */
	{1019 ,  400},	/* 40  */
	{924 ,  430},	/* 43  */
	{870 ,  450},	/* 45  */
	{744 ,  500},	/* 50  */
	{631 ,  550},	/* 55  */
	{536 ,  600},	/* 60  */
	{480 ,  630},	/* 63  */
	{456 ,  650},	/* 65  */
	{420 ,  670},	/* 67  */
};

struct spa_power_data spa_power_pdata = {
	.charger_name = "spa_agent_chrg",
	.batt_cell_name = "SDI_SDI",
	.eoc_current = 120, // mA
	.recharge_voltage = 4300, //4.3V
#if defined(CONFIG_SPA_SUPPLEMENTARY_CHARGING)
	.backcharging_time = 20, //mins
	.recharging_eoc = 70, // mA
#endif
	.charging_cur_usb = 400, // not used
	.charging_cur_wall = 600, // not used
	.suspend_temp_hot = 600,
	.recovery_temp_hot = 400,
	.suspend_temp_cold = -50,
	.recovery_temp_cold = 0,
	.charge_timer_limit = CHARGE_TIMER_6HOUR,
	.batt_temp_tb = &batt_temp_tb[0],
	.batt_temp_tb_len = ARRAY_SIZE(batt_temp_tb),
};
EXPORT_SYMBOL(spa_power_pdata);

static struct platform_device spa_power_device = {
	.name = "spa_power",
	.id = -1,
	.dev.platform_data = &spa_power_pdata,
};

static struct platform_device spa_agent_device={
	.name = "spa_agent",
	.id=-1,
};

static int spa_power_init(void)
{
	platform_device_register(&spa_agent_device);
	platform_device_register(&spa_power_device);
	return 0;
}
#endif


/*keypad define */
#define CUSTOM_KEYPAD_ROWS          (SCI_ROW0 | SCI_ROW1)
#define CUSTOM_KEYPAD_COLS          (SCI_COL0 | SCI_COL1 | SCI_COL3)
#define ROWS	(2)
#define COLS	(4)

#define        ANA_REG_SET(_r, _v, _m) sci_adi_write((_r), ((_v) & (_m)), (_m))
#define        ANA_REG_GET(_r)         sci_adi_read(_r)


static const unsigned int board_keymap[] = {
	KEY(0, 0, KEY_VOLUMEUP ),
	KEY(1, 0, KEY_VOLUMEDOWN),
	KEY(0, 1, KEY_HOME),
	KEY(0, 3, KEY_VOLUMEUP),
};

static const struct matrix_keymap_data customize_keymap = {
	.keymap = board_keymap,
	.keymap_size = ARRAY_SIZE(board_keymap),
};

static struct sci_keypad_platform_data sci_keypad_data = {
	.rows_choose_hw = CUSTOM_KEYPAD_ROWS,
	.cols_choose_hw = CUSTOM_KEYPAD_COLS,
	.rows_number = ROWS,
	.cols_number = COLS,
	.keymap_data = &customize_keymap,
	.support_long_key = 0,
	.repeat = 0,
	.debounce_time = 5000,
};

static struct platform_device rfkill_device;
static struct platform_device brcm_bluesleep_device;
static struct platform_device kb_backlight_device;
#ifdef CONFIG_MFD_RT8973
static struct platform_device rt8973_mfd_device_i2cadaptor;
#endif

#ifdef CONFIG_I2C_GPIO
static struct i2c_gpio_platform_data i2c_gpio_data =
{
	.sda_pin = GPIO_SENSOR_SDA,
	.scl_pin = GPIO_SENSOR_SCL,
	.udelay	 = 2,
	.timeout = 100,
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0,
};

static struct platform_device i2c_gpio_device =
{
	.name = "i2c-gpio",
	.id   = 5,
	.dev  =
	{
		.platform_data = &i2c_gpio_data,
	},
};
#endif

static struct platform_device *devices[] __initdata = {
	&sprd_serial_device0,
	&sprd_serial_device1,
	&sprd_serial_device2,
	&sprd_device_rtc,
	&sprd_eic_gpio_device,
	&sprd_nand_device,
	&sprd_lcd_device0,
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	&sprd_ram_console,
#endif
	&sprd_backlight_device,
	&sprd_i2c_device0,
	&sprd_i2c_device1,
	&sprd_i2c_device2,
	&sprd_i2c_device3,
	&sprd_i2c_device4,
	&sprd_spi0_device,
	&sprd_spi1_device,
	&sprd_spi2_device,
	&sprd_keypad_device,
	&sprd_battery_device,
#ifdef CONFIG_ION
	&sprd_ion_dev,
#endif
#if defined(CONFIG_ARCH_SCX15)
	&sprd_gsp_device,
#endif
#if defined(CONFIG_SPRD_IOMMU)
	&sprd_iommu_gsp_device,
	&sprd_iommu_mm_device,
#endif
	&sprd_emmc_device,
	&sprd_sdio0_device,
	&sprd_sdio1_device,
	&sprd_dcam_device,
	&sprd_scale_device,
	&sprd_rotation_device,
	&sprd_sensor_device,
	&sprd_dma_copy_device,
#if !defined(CONFIG_ARCH_SCX15)
	&sprd_sdio2_device,
#endif
	&sprd_vsp_device,
	&sprd_jpg_device,
#if 0
	&sprd_isp_device,
	&sprd_ahb_bm0_device,
	&sprd_ahb_bm1_device,
	&sprd_ahb_bm2_device,
	&sprd_ahb_bm3_device,
	&sprd_ahb_bm4_device,
	&sprd_axi_bm0_device,
	&sprd_axi_bm1_device,
	&sprd_axi_bm2_device,
#endif
#if 0
	&rfkill_device,
	&brcm_bluesleep_device,
#endif
#ifdef CONFIG_SIPC_TD
	&sprd_cproc_td_device,
	&sprd_spipe_td_device,
	&sprd_slog_td_device,
	&sprd_stty_td_device,
	&sprd_seth0_td_device,
	&sprd_seth1_td_device,
	&sprd_seth2_td_device,
	&sprd_saudio_td_device,
#endif
#ifdef CONFIG_SIPC_WCDMA
	&sprd_cproc_wcdma_device,
	&sprd_spipe_wcdma_device,
	&sprd_slog_wcdma_device,
	&sprd_stty_wcdma_device,
	&sprd_seth0_wcdma_device,
	&sprd_seth1_wcdma_device,
	&sprd_seth2_wcdma_device,
	&sprd_saudio_wcdma_device,
#endif
#ifdef CONFIG_SIPC_WCN
	&sprd_cproc_wcn_device,
	&sprd_spipe_wcn_device,
	&sprd_slog_wcn_device,
	&sprd_sttybt_td_device,
#endif
	&kb_backlight_device,
	&sprd_a7_pmu_device,
#ifdef  CONFIG_RF_SHARK
	&trout_fm_device,
#endif
	&sprd_headset_device,
	//add for nxp, xiaofu.liang,SRC-nanjing 20140213
	&sprd_saudio_voip_device,
	//add for nxp done
#ifdef CONFIG_I2C_GPIO
	&i2c_gpio_device,
#endif
};

static struct platform_device *late_devices[] __initdata = {
	/* 1. CODECS */
	&sprd_audio_sprd_codec_v3_device,
	&sprd_audio_null_codec_device,

	/* 2. CPU DAIS */
	&sprd_audio_vbc_r2p0_device,
	&sprd_audio_vaudio_device,
	&sprd_audio_i2s0_device,
	&sprd_audio_i2s1_device,
	&sprd_audio_i2s2_device,
	&sprd_audio_i2s3_device,

	/* 3. PLATFORM */
	&sprd_audio_platform_pcm_device,

	/* 4. MACHINE */
	&sprd_audio_vbc_r2p0_sprd_codec_v3_device,
	&sprd_audio_i2s_null_codec_device,
#ifdef CONFIG_MFD_RT8973
        &rt8973_mfd_device_i2cadaptor,
#endif
#if defined(CONFIG_SPRD_SIMPLE_MUIC)
	&sprd_simple_muic,
#endif
};

#if 0
/* BT suspend/resume */
static struct resource bluesleep_resources[] = {
	{
		.name	= "gpio_host_wake",
		.start	= GPIO_BT2AP_WAKE,
		.end	= GPIO_BT2AP_WAKE,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "gpio_ext_wake",
		.start	= GPIO_AP2BT_WAKE,
		.end	= GPIO_AP2BT_WAKE,
		.flags	= IORESOURCE_IO,
	},
};

static struct platform_device brcm_bluesleep_device = {
	.name = "bluesleep",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(bluesleep_resources),
	.resource	= bluesleep_resources,
};

static struct resource rfkill_resources[] = {
	{
		.name   = "bt_power",
		.start  = GPIO_BT_POWER,
		.end    = GPIO_BT_POWER,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "bt_reset",
		.start  = GPIO_BT_RESET,
		.end    = GPIO_BT_RESET,
		.flags  = IORESOURCE_IO,
	},
};
static struct platform_device rfkill_device = {
	.name = "rfkill",
	.id = -1,
	.num_resources	= ARRAY_SIZE(rfkill_resources),
	.resource	= rfkill_resources,
};
#endif

#if defined (CONFIG_SENSORS_K3DH) || defined(CONFIG_SENSORS_K2HH)
#define ACCEL_INT_GPIO_PIN 109
static struct k3dh_platform_data k3dh_platform_data = {
        .orientation = {
        -1, 0, 0,
        0, -1, 0,
        0, 0, -1
       },
        .irq_gpio = ACCEL_INT_GPIO_PIN,
};
#endif

#if defined(CONFIG_SENSORS_K3DH) || defined(CONFIG_SENSORS_K2HH)
static struct i2c_board_info sensor_gpio_i2c_device[] =
{
#ifdef CONFIG_SENSORS_K2HH
	{
		I2C_BOARD_INFO("k2hh", 0x1d),
		.platform_data = &k3dh_platform_data,
	}
#endif
#ifdef CONFIG_SENSORS_K3DH
	{
		I2C_BOARD_INFO("k3dh", 0x19),
		.platform_data = &k3dh_platform_data,
	}
#endif
};
#endif
/* keypad backlight */
static struct platform_device kb_backlight_device = {
	.name           = "keyboard-backlight",
	.id             =  -1,
};

static int calibration_mode = false;
static int __init calibration_start(char *str)
{
	int calibration_device =0;
	int mode=0,freq=0,device=0;
	if(str){
		pr_info("modem calibartion:%s\n", str);
		sscanf(str, "%d,%d,%d", &mode,&freq,&device);
	}
	if(device & 0x80){
		calibration_device = device & 0xf0;
		calibration_mode = true;
		pr_info("calibration device = 0x%x\n",calibration_device);
	}
	return 1;
}
__setup("calibration=", calibration_start);

int in_calibration(void)
{
	return (calibration_mode == true);
}
EXPORT_SYMBOL(in_calibration);

static void __init sprd_add_otg_device(void)
{
	/*
	 * if in calibrtaion mode, we do nothing, modem will handle everything
	 */
	platform_device_register(&sprd_otg_device);
}

static struct serial_data plat_data0 = {
	.wakeup_type = BT_RTS_HIGH_WHEN_SLEEP,
	.clk = 48000000,
};
static struct serial_data plat_data1 = {
	.wakeup_type = BT_RTS_HIGH_WHEN_SLEEP,
	.clk = 26000000,
};
static struct serial_data plat_data2 = {
	.wakeup_type = BT_RTS_HIGH_WHEN_SLEEP,
	.clk = 26000000,
};

#ifdef CONFIG_TOUCHSCREEN_IST30XXB
static struct tsp_dev_info ist30xx_info = {
	.gpio = GPIO_TOUCH_IRQ,
	.tsp_3_0v_ldo = "vddcon",
};
#else
static struct ft5x0x_ts_platform_data ft5x0x_ts_info = {
	.irq_gpio_number	= GPIO_TOUCH_IRQ,
	.reset_gpio_number	= GPIO_TOUCH_RESET,
	.vdd_name = "vdd28",
};
#endif

static struct i2c_board_info i2c0_boardinfo[] = {
	{I2C_BOARD_INFO("sensor_main",0x3C),},
	{I2C_BOARD_INFO("sensor_sub",0x21),},
};

#ifdef CONFIG_TOUCHSCREEN_IST30XXB
static struct i2c_board_info i2c1_boardinfo[] = {
	{
		I2C_BOARD_INFO(IST30XX_DEV_NAME, 0x50),
		.platform_data =&ist30xx_info,
	},
};
#else
static struct i2c_board_info i2c0_boardinfo[] = {
	{
		I2C_BOARD_INFO(FOCALTECH_TS_NAME, FOCALTECH_TS_ADDR),
		.platform_data = &ft5x0x_ts_info,
	},
};
#endif

static struct i2c_board_info i2c2_boardinfo[] = {
#if defined(CONFIG_SEC_NFC_IF_I2C)
        {
            I2C_BOARD_INFO(SEC_NFC_DRIVER_NAME, NFC_I2C_ADDR),
            .platform_data = &s3fwrn5_pdata,
			.irq = 363,
		},
#endif
#ifndef CONFIG_SENSORS_EMUL_I2C
#ifdef CONFIG_SENSORS_K2HH
	{
		I2C_BOARD_INFO("k2hh", 0x1d),
		.platform_data = &k3dh_platform_data,
	}
#endif
#ifdef CONFIG_SENSORS_K3DH
	{
		I2C_BOARD_INFO("k3dh", 0x19),
		.platform_data = &k3dh_platform_data,
	}
#endif
#endif
};
static struct i2c_board_info i2c4_boardinfo[] = {
#ifdef CONFIG_STC3115_FUELGAUGE
	{
		I2C_BOARD_INFO("stc3115", 0x70),
		.platform_data	= &stc3115_data,
	},
#endif
#ifdef CONFIG_STC3117_FUELGAUGE
	{
		I2C_BOARD_INFO("stc3117", 0x70),
		.platform_data	= &stc3117_data,
	},
#endif
};
#ifdef CONFIG_MFD_RT8973
#define GPIO_MUIC_IRQ 48
#define GPIO_MUIC_SDA 49
#define GPIO_MUIC_SCL 51
static struct i2c_gpio_platform_data rt8973_i2cadaptor_data = {
	.sda_pin = GPIO_MUIC_SDA,
	.scl_pin = GPIO_MUIC_SCL,
	.udelay  = 10,
	.timeout = 0,
};

static struct platform_device rt8973_mfd_device_i2cadaptor = {
	.name   = "i2c-gpio",
	.id     = 7,
	.dev	= {
		.platform_data = &rt8973_i2cadaptor_data,
	}
};

#include <linux/mfd/rt8973.h>
static struct rt8973_platform_data rt8973_pdata = {
    .irq_gpio = GPIO_MUIC_IRQ,
    .cable_chg_callback = NULL,
    .usb_callback = NULL,
    .ocp_callback = NULL,
    .otp_callback = NULL,
    .ovp_callback = NULL,
    .usb_callback = NULL,
    .uart_callback = NULL,
    .otg_callback = NULL,
    .jig_callback = NULL,
};

static struct i2c_board_info rtmuic_i2c_boardinfo[] __initdata = {
    {
        I2C_BOARD_INFO("rt8973", 0x28 >> 1),
        .platform_data = &rt8973_pdata,
    },
};
#endif
static int sc8810_add_i2c_devices(void)
{
	i2c_register_board_info(0, i2c0_boardinfo, ARRAY_SIZE(i2c0_boardinfo));
	i2c_register_board_info(1, i2c1_boardinfo, ARRAY_SIZE(i2c1_boardinfo));
	i2c_register_board_info(2, i2c2_boardinfo, ARRAY_SIZE(i2c2_boardinfo));
#if defined(CONFIG_SENSORS_EMUL_I2C)
#if defined(CONFIG_SENSORS_K3DH) || defined(CONFIG_SENSORS_K2HH)
	i2c_register_board_info(5, sensor_gpio_i2c_device, ARRAY_SIZE(sensor_gpio_i2c_device));
#endif
#endif
#if !defined(CONFIG_FUELGAUGE_STC3117)
	i2c_register_board_info(4, i2c4_boardinfo, ARRAY_SIZE(i2c4_boardinfo));
#endif
#ifdef CONFIG_MFD_RT8973
	i2c_register_board_info(7, rtmuic_i2c_boardinfo, ARRAY_SIZE(rtmuic_i2c_boardinfo));
#endif
	return 0;
}

struct platform_device audio_pa_amplifier_device = {
	.name = "speaker-pa",
	.id = -1,
};

static int audio_pa_amplifier_l(u32 cmd, void *data)
{
	return 0;
}

static int hw_rev;

static int __init hw_revision_setup(char *str)
{
	int n;
	if (!get_option(&str, &n))
		return 0;
	hw_rev = n;
	return 1;
}
__setup("hw_revision=", hw_revision_setup);

int get_hw_rev(void)
{
	return hw_rev;
}
#ifdef CONFIG_MFD_RT8973
u8 attached_cable;
extern int current_cable_type;
int epmic_event_handler(int level);
void sec_charger_cb(u8 cable_type)
{
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");
    pr_info("%s: cable type (0x%02x)\n", __func__, cable_type);
	attached_cable = cable_type;
	switch (cable_type) {
	case MUIC_RT8973_CABLE_TYPE_NONE:
	case MUIC_RT8973_CABLE_TYPE_UNKNOWN:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		is_jig_on = false;
		charger_enable(0);
		break;
	case MUIC_RT8973_CABLE_TYPE_USB:
	case MUIC_RT8973_CABLE_TYPE_CDP:
	case MUIC_RT8973_CABLE_TYPE_L_USB:
        case MUIC_RT8973_CABLE_TYPE_0x15:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		is_jig_on = false;
		charger_enable(1);
		break;
	case MUIC_RT8973_CABLE_TYPE_REGULAR_TA:
        case MUIC_RT8973_CABLE_TYPE_ATT_TA:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		is_jig_on = false;
		charger_enable(1);
		break;
	case MUIC_RT8973_CABLE_TYPE_OTG:
		goto skip;
	case MUIC_RT8973_CABLE_TYPE_JIG_UART_OFF_WITH_VBUS:
		current_cable_type = POWER_SUPPLY_TYPE_UARTOFF;
		is_jig_on = true;
		break;
	case MUIC_RT8973_CABLE_TYPE_JIG_UART_OFF:
	/*
		if (!gpio_get_value(mfp_to_gpio(GPIO008_GPIO_8))) {
			pr_info("%s cable type POWER_SUPPLY_TYPE_UARTOFF\n", __func__);
			current_cable_type = POWER_SUPPLY_TYPE_UARTOFF;
		}
		else {
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		}*/
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		is_jig_on = true;
		break;
	case MUIC_RT8973_CABLE_TYPE_JIG_USB_ON:
	case MUIC_RT8973_CABLE_TYPE_JIG_USB_OFF:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		is_jig_on = true;
		charger_enable(1);
		break;
	case MUIC_RT8973_CABLE_TYPE_0x1A:
	case MUIC_RT8973_CABLE_TYPE_TYPE1_CHARGER:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		is_jig_on = false;
		charger_enable(1);
		break;
	default:
		pr_err("%s: invalid type for charger:%d\n",
			__func__, cable_type);
		current_cable_type = POWER_SUPPLY_TYPE_UNKNOWN;
		goto skip;
	}

	if (!psy || !psy->set_property)
		pr_err("%s: fail to get battery psy\n", __func__);
	else {
		value.intval = current_cable_type;
		psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	}

	if (current_cable_type == POWER_SUPPLY_TYPE_USB) {
		epmic_event_handler(1);
	}
skip:
	return;
}
EXPORT_SYMBOL(sec_charger_cb);
#endif
#if 0
/* Control ldo for maxscend cmmb chip according to HW design */
static struct regulator *cmmb_regulator_1v8 = NULL;

#define SPI_PIN_FUNC_MASK  (0x3<<4)
#define SPI_PIN_FUNC_DEF   (0x0<<4)
#define SPI_PIN_FUNC_GPIO  (0x3<<4)

struct spi_pin_desc {
	const char   *name;
	unsigned int pin_func;
	unsigned int reg;
	unsigned int gpio;
};

static struct spi_pin_desc spi_pin_group[] = {
	{"SPI_DI",  SPI_PIN_FUNC_DEF,  REG_PIN_SPI0_DI   + CTL_PIN_BASE,  158},
	{"SPI_CLK", SPI_PIN_FUNC_DEF,  REG_PIN_SPI0_CLK  + CTL_PIN_BASE,  159},
	{"SPI_DO",  SPI_PIN_FUNC_DEF,  REG_PIN_SPI0_DO   + CTL_PIN_BASE,  157},
	{"SPI_CS0", SPI_PIN_FUNC_GPIO, REG_PIN_SPI0_CSN  + CTL_PIN_BASE,  156}
};
static void sprd_restore_spi_pin_cfg(void)
{
	unsigned int reg;
	unsigned int  gpio;
	unsigned int  pin_func;
	unsigned int value;
	unsigned long flags;
	int i = 0;
	int regs_count = sizeof(spi_pin_group)/sizeof(struct spi_pin_desc);

	for (; i < regs_count; i++) {
	    pin_func = spi_pin_group[i].pin_func;
	    gpio = spi_pin_group[i].gpio;
	    if (pin_func == SPI_PIN_FUNC_DEF) {
		 reg = spi_pin_group[i].reg;
		 /* free the gpios that have request */
		 gpio_free(gpio);
		 local_irq_save(flags);
		 /* config pin default spi function */
		 value = ((__raw_readl(reg) & ~SPI_PIN_FUNC_MASK) | SPI_PIN_FUNC_DEF);
		 __raw_writel(value, reg);
		 local_irq_restore(flags);
	    }
	    else {
		 /* CS should config output */
		 gpio_direction_output(gpio, 1);
	    }
	}
}

static void sprd_set_spi_pin_input(void)
{
	unsigned int reg;
	unsigned int value;
	unsigned int  gpio;
	unsigned int  pin_func;
	const char    *name;
	unsigned long flags;
	int i = 0;
	int regs_count = sizeof(spi_pin_group)/sizeof(struct spi_pin_desc);

	for (; i < regs_count; i++) {
	    pin_func = spi_pin_group[i].pin_func;
	    gpio = spi_pin_group[i].gpio;
	    name = spi_pin_group[i].name;

	    /* config pin GPIO function */
	    if (pin_func == SPI_PIN_FUNC_DEF) {
		 reg = spi_pin_group[i].reg;

		 local_irq_save(flags);
		 value = ((__raw_readl(reg) & ~SPI_PIN_FUNC_MASK) | SPI_PIN_FUNC_GPIO);
		 __raw_writel(value, reg);
		 local_irq_restore(flags);
		 if (gpio_request(gpio, name)) {
		     printk("smsspi: request gpio %d failed, pin %s\n", gpio, name);
		 }
	    }

	    gpio_direction_input(gpio);
	}
}

static void mxd_cmmb_poweron(void)
{
        regulator_set_voltage(cmmb_regulator_1v8, 1700000, 1800000);
        regulator_disable(cmmb_regulator_1v8);
        msleep(3);
        regulator_enable(cmmb_regulator_1v8);
        msleep(5);

        /* enable 26M external clock */
        gpio_direction_output(GPIO_CMMB_26M_CLK_EN, 1);
}

static void mxd_cmmb_poweroff(void)
{
        regulator_disable(cmmb_regulator_1v8);
        gpio_direction_output(GPIO_CMMB_26M_CLK_EN, 0);
}

static int mxd_cmmb_init(void)
{
         int ret=0;
         ret = gpio_request(GPIO_CMMB_26M_CLK_EN,   "MXD_CMMB_CLKEN");
         if (ret)
         {
                   pr_debug("mxd spi req gpio clk en err!\n");
                   goto err_gpio_init;
         }
         gpio_direction_output(GPIO_CMMB_26M_CLK_EN, 0);
         cmmb_regulator_1v8 = regulator_get(NULL, "vddcmmb1p8");
         return 0;

err_gpio_init:
	 gpio_free(GPIO_CMMB_26M_CLK_EN);
         return ret;
}

static struct mxd_cmmb_026x_platform_data mxd_plat_data = {
	.poweron  = mxd_cmmb_poweron,
	.poweroff = mxd_cmmb_poweroff,
	.init     = mxd_cmmb_init,
	.set_spi_pin_input   = sprd_set_spi_pin_input,
	.restore_spi_pin_cfg = sprd_restore_spi_pin_cfg,
};

static int spi_cs_gpio_map[][2] = {
    {SPI0_CMMB_CS_GPIO,  0},
    {SPI0_CMMB_CS_GPIO,  0},
    {SPI0_CMMB_CS_GPIO,  0},
} ;

static struct spi_board_info spi_boardinfo[] = {
	{
	.modalias = "cmmb-dev",
	.bus_num = 0,
	.chip_select = 0,
	.max_speed_hz = 8 * 1000 * 1000,
	.mode = SPI_CPOL | SPI_CPHA,
        .platform_data = &mxd_plat_data,
	},
	{
	.modalias = "spidev",
	.bus_num = 1,
	.chip_select = 0,
	.max_speed_hz = 1000 * 1000,
	.mode = SPI_CPOL | SPI_CPHA,
	},
	{
	.modalias = "spidev",
	.bus_num = 2,
	.chip_select = 0,
	.max_speed_hz = 1000 * 1000,
	.mode = SPI_CPOL | SPI_CPHA,
	}
};
#endif
static void sprd_spi_init(void)
{
#if 0
	int busnum, cs, gpio;
	int i;
	struct spi_board_info *info = spi_boardinfo;

	for (i = 0; i < ARRAY_SIZE(spi_boardinfo); i++) {
		busnum = info[i].bus_num;
		cs = info[i].chip_select;
		gpio   = spi_cs_gpio_map[busnum][cs];
		info[i].controller_data = (void *)gpio;
	}

        spi_register_board_info(info, ARRAY_SIZE(spi_boardinfo));
#endif
}

static int sc8810_add_misc_devices(void)
{
	if (0) {
		platform_set_drvdata(&audio_pa_amplifier_device, audio_pa_amplifier_l);
		if (platform_device_register(&audio_pa_amplifier_device))
			pr_err("faile to install audio_pa_amplifier_device\n");
	}
	return 0;
}

int __init __clock_init_early(void)
{
#if defined(CONFIG_ARCH_SCX15)
#else
	pr_info("ahb ctl0 %08x, ctl2 %08x glb aon apb0 %08x aon apb1 %08x clk_en %08x\n",
		sci_glb_raw_read(REG_AP_AHB_AHB_EB),
		sci_glb_raw_read(REG_AP_AHB_AHB_RST),
		sci_glb_raw_read(REG_AON_APB_APB_EB0),
		sci_glb_raw_read(REG_AON_APB_APB_EB1),
		sci_glb_raw_read(REG_AON_CLK_PUB_AHB_CFG));
#endif

	sci_glb_clr(REG_AP_AHB_AHB_EB,
		BIT_BUSMON2_EB		|
		BIT_BUSMON1_EB		|
		BIT_BUSMON0_EB		|
		//BIT_SPINLOCK_EB		|
		BIT_GPS_EB		|
		//BIT_EMMC_EB		|
		//BIT_SDIO2_EB		|
		//BIT_SDIO1_EB		|
		//BIT_SDIO0_EB		|
		BIT_DRM_EB		|
		BIT_NFC_EB		|
		//BIT_DMA_EB		|
		//BIT_USB_EB		|
		//BIT_GSP_EB		|
		//BIT_DISPC1_EB		|
		//BIT_DISPC0_EB		|
		//BIT_DSI_EB		|
		0);
	sci_glb_clr(REG_AP_APB_APB_EB,
		BIT_INTC3_EB		|
		BIT_INTC2_EB		|
		BIT_INTC1_EB		|
		BIT_IIS1_EB		|
		BIT_UART2_EB		|
		BIT_UART0_EB		|
		BIT_SPI1_EB		|
		BIT_SPI0_EB		|
		BIT_IIS0_EB		|
		BIT_I2C0_EB		|
		BIT_SPI2_EB		|
		BIT_UART3_EB		|
		0);
	sci_glb_clr(REG_AON_APB_APB_RTC_EB,
		BIT_KPD_RTC_EB		|
		BIT_KPD_EB		|
		BIT_EFUSE_EB		|
		0);

	sci_glb_clr(REG_AON_APB_APB_EB0,
		BIT_AUDIF_EB			|
		BIT_VBC_EB			|
		BIT_PWM3_EB			|
		BIT_PWM1_EB			|
		0);
	sci_glb_clr(REG_AON_APB_APB_EB1,
		BIT_AUX1_EB			|
		BIT_AUX0_EB			|
		0);
	printk("sc clock module early init ok\n");
	return 0;
}

static inline int	__sci_get_chip_id(void)
{
	return __raw_readl(CHIP_ID_LOW_REG);
}

#if 0
/*i2s0 config for BT, use pcm mode*/
static struct i2s_config i2s0_config = {
	.fs = 8000,
	.slave_timeout = 0xF11,
	.bus_type = PCM_BUS,
	.byte_per_chan = I2S_BPCH_16,
	.mode = I2S_MASTER,
	.lsb = I2S_LSB,
	.rtx_mode = I2S_RTX_MODE,
	.sync_mode = I2S_LRCK,
	.lrck_inv = I2S_L_LEFT,	/*NOTE: MUST I2S_L_LEFT in pcm mode*/
	.clk_inv = I2S_CLK_N,
	.pcm_bus_mode = I2S_SHORT_FRAME,
	.pcm_slot = 0x1,
	.pcm_cycle = 1,
	.tx_watermark = 8,
	.rx_watermark = 24,
};
#else
static struct i2s_config i2s0_config = {
        .hw_port = 0,
        .fs = 8000,
        .slave_timeout = 0xF11,
        .bus_type = I2S_BUS,
        .byte_per_chan = I2S_BPCH_16,
        .mode = I2S_MASTER,
        .lsb = I2S_MSB,
        .rtx_mode = I2S_RTX_MODE,
        .sync_mode = I2S_LRCK,
        .lrck_inv = I2S_L_LEFT,
        .clk_inv = I2S_CLK_R,
        .i2s_bus_mode = I2S_COMPATIBLE,
        .tx_watermark = 12,
        .rx_watermark = 20,
};
#endif
/*you can change param here to config the i2s modules*/
static struct i2s_config i2s1_config = {0};
static struct i2s_config i2s2_config = {0};
static struct i2s_config i2s3_config = {0};

static void __init sc8830_init_machine(void)
{
	u32 reg_val;
	printk("sci get chip id = 0x%x\n",__sci_get_chip_id());

	sci_adc_init((void __iomem *)ADC_BASE);
	sci_regulator_init();
	sprd_add_otg_device();
	platform_device_add_data(&sprd_serial_device0,(const void*)&plat_data0,sizeof(plat_data0));
	platform_device_add_data(&sprd_serial_device1,(const void*)&plat_data1,sizeof(plat_data1));
	platform_device_add_data(&sprd_serial_device2,(const void*)&plat_data2,sizeof(plat_data2));
	platform_device_add_data(&sprd_keypad_device,(const void*)&sci_keypad_data,sizeof(sci_keypad_data));
	platform_device_add_data(&sprd_audio_i2s0_device,(const void*)&i2s0_config,sizeof(i2s0_config));
	platform_device_add_data(&sprd_audio_i2s1_device,(const void*)&i2s1_config,sizeof(i2s1_config));
	platform_device_add_data(&sprd_audio_i2s2_device,(const void*)&i2s2_config,sizeof(i2s2_config));
	platform_device_add_data(&sprd_audio_i2s3_device,(const void*)&i2s3_config,sizeof(i2s3_config));
	platform_add_devices(devices, ARRAY_SIZE(devices));
	sc8810_add_i2c_devices();
#if defined(CONFIG_SEC_CHARGING_FEATURE)
	spa_power_init();
#endif
	sc8810_add_misc_devices();
	sprd_spi_init();
	reg_val = ANA_REG_GET(ANA_REG_GLB_DCDC_GEN_ADI);
	reg_val &= ~0xff;
	reg_val |= 0x80;
	ANA_REG_SET(ANA_REG_GLB_DCDC_GEN_ADI,reg_val,-1);
	reg_val = ANA_REG_GET(ANA_REG_GLB_DCDC_GEN_ADI);
	printk("!!!!!!!!!!!!reg_val = %x\n!!!!!!!!!!!!!",reg_val);

#if defined(CONFIG_FUELGAUGE_STC3117) || defined(CONFIG_CHARGER_SPRD2713)
	sprd_battery_init();
#endif
        dev_bluetooth_init();
}

static void __init sc8830_init_late(void)
{
	platform_add_devices(late_devices, ARRAY_SIZE(late_devices));
}

extern void __init  sci_enable_timer_early(void);
static void __init sc8830_init_early(void)
{
	/* earlier init request than irq and timer */
	__clock_init_early();
	sci_enable_timer_early();
	sci_adi_init();
	/*ipi reg init for sipc*/
	sci_glb_set(REG_AON_APB_APB_EB0, BIT_IPI_EB);
}

/*
 * Setup the memory banks.
 */

static void __init sc8830_fixup(struct machine_desc *desc,
	struct tag *tags, char **cmdline, struct meminfo *mi)
{
}

MACHINE_START(SCPHONE, "scx15")
	.reserve	= sci_reserve,
	.map_io		= sci_map_io,
	.fixup		= sc8830_fixup,
	.init_early	= sc8830_init_early,
	.init_irq	= sci_init_irq,
	.init_time		= sci_timer_init,
	.init_machine	= sc8830_init_machine,
	.init_late	= sc8830_init_late,
MACHINE_END
