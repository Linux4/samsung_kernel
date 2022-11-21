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
#include <linux/pwm_backlight.h>
#if defined(CONFIG_TC35876X)
#include <mach/tc35876x.h>
#endif

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif

#ifdef CONFIG_SENSORS_GP2A0X0
#include <linux/input/gp2a0x0.h>
#endif
#ifdef CONFIG_SENSORS_BMC150
#include <linux/bst_sensor_common.h>
#endif
#ifdef CONFIG_SENSORS_ASP01
#include <linux/asp01.h>
#endif
#ifdef CONFIG_NFC_PN547
#include <linux/pn544.h>
#endif

#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>

#include <mach/addr-map.h>
#include <mach/clock-pxa988.h>
#include <mach/mmp_device.h>
#include <mach/mfp-pxa986-lt02.h>
#include <mach/irqs.h>
#include <mach/isl29043.h>
#include <mach/pxa988.h>
#include <mach/soc_coda7542.h>
#include <mach/regs-rtc.h>
#include <mach/regs-ciu.h>
#include <mach/regs-apbc.h>
#include <plat/pxa27x_keypad.h>
#include <plat/pm.h>
#include <media/soc_camera.h>
#include <mach/isp_dev.h>
#include <mach/gpio-edge.h>
#include <mach/samsung_camera_lt02.h>
#include <mach/dvfs.h>
#include <mach/regs-mpmu.h>
#include <mach/regs-apmu.h>
/*#include <mach/sec_common.h>*/

#ifdef CONFIG_PM_DEVFREQ
#include <plat/devfreq.h>
#endif
#include "common.h"
#include "onboard.h"
#include "board-lt02-regulators.h"
#if defined(CONFIG_TOUCHSCREEN_MMS136_TS)
#include <linux/input/mms136_ts.h>
#endif
#if defined(CONFIG_TOUCHSCREEN_BT532_TS)
#include <linux/input/bt532_ts.h>
#endif
#if defined(CONFIG_SPA)
#include <mach/spa.h>
#endif
#if defined(CONFIG_BQ24157_CHARGER)
#include <mach/bq24157_charger.h>
#endif
#if defined(CONFIG_ANDROID_TIMED_GPIO)
#include <../drivers/staging/android/timed_gpio.h>
#endif
#if defined(CONFIG_KEYBOARD_GPIO)
#include <linux/gpio_keys.h>
#endif

#if 0
#include <mach/stc3115_fuelgauge.h>
#endif

#if defined(CONFIG_FSA9480_MICROUSB)
#include <mach/fsa9480.h>
#endif

#ifdef CONFIG_SAMSUNG_JACK
#include <linux/mfd/sec_jack.h>
#endif
#ifdef CONFIG_SEC_THERMISTOR
#include <mach/sec_thermistor.h>
#endif

#include "board-lt02.h"

#define VER_1V0 0x10
#define VER_1V1 0x11
static int board_id;
static int production_mode_flag;
int panel_id;
int recovery_mode;
unsigned int sec_debug_mode;
EXPORT_SYMBOL(sec_debug_mode);

extern unsigned int lpcharge;
extern int lpm_poweroff;
extern int current_cable_type;

static int __init board_id_setup(char *str)
{
	if (!get_option(&str, &board_id))
		return 0;

	switch (board_id) {
#if 0
	case 2:
	case 3:
		printk(KERN_INFO "LT02 BringUp Board Rev 0.2\n");
		system_rev = LT02_BRINGUP_02;
		break;
#endif
	case 0:
		printk(KERN_INFO "LT02 Board Rev 0.0\n");
		system_rev = LT02_R0_0;
		break;
	case 1:
		printk(KERN_INFO "LT02 Board Rev 0.1\n");
		system_rev = LT02_R0_1;
		break;
#if 1
	case 2:
		printk(KERN_INFO "LT02 Board Rev 0.2\n");
		system_rev = LT02_R0_2;
		break;
	case 3:
		printk(KERN_INFO "LT02 Board Rev 0.3\n");
		system_rev = LT02_R0_3;
		break;
#endif
	case 4:
		printk(KERN_INFO "LT02 Board Rev 0.4\n");
		system_rev = LT02_R0_4;
		break;
	case 5:
		printk(KERN_INFO "LT02 Board Rev 0.5\n");
		system_rev = LT02_R0_5;
		break;
	case 6:
		printk(KERN_INFO "LT02 Board Rev 0.6\n");
		system_rev = LT02_R0_6;
		break;
	case 7:
		printk(KERN_INFO "LT02 Board Rev 0.7\n");
		system_rev = LT02_R0_7;
		break;
	default:
		printk(KERN_INFO "Unknown board_id=%c\n", *str);
		break;
	};

	return 1;
}
__setup("board_id=", board_id_setup);

void print_board_revision(void)
{
	extern int ddr_mode;
	
	switch (system_rev) {
	case LT02_R0_0:
		printk(KERN_INFO "LT02 Board Rev 0.0\n");
		break;
	case LT02_R0_1:
		printk(KERN_INFO "LT02 Board Rev 0.1\n");
		break;
	case LT02_R0_2:
		printk(KERN_INFO "LT02 Board Rev 0.2\n");
		break;
	case LT02_R0_3:
		printk(KERN_INFO "LT02 Board Rev 0.3\n");
		break;
	case LT02_R0_4:
		printk(KERN_INFO "LT02 Board Rev 0.4\n");
		break;
	case LT02_R0_5:
		printk(KERN_INFO "LT02 Board Rev 0.5\n");
		break;
	case LT02_R0_6:
		printk(KERN_INFO "LT02 Board Rev 0.6\n");
		break;
	case LT02_R0_7:
		printk(KERN_INFO "LT02 Board Rev 0.7\n");
		break;
	default:
		printk(KERN_INFO "Unknown board_id\n");
	}
	printk(KERN_INFO "ddr_mode : %d", ddr_mode);
}

static void __init production_mode(char *str)
{
	int n;
	if (!get_option(&str, &n))
		production_mode_flag = 0;
	production_mode_flag = 1;
}

__setup("androidboot.bsp=", production_mode);

static int __init get_sec_debug_mode(char *str)
{
	if (get_option(&str, &sec_debug_mode) != 1)
		sec_debug_mode = 0;
	return 1;
}
__setup("androidboot.debug_level=", get_sec_debug_mode);

static int __init panel_id_setup(char *str)
{
	int n;
	if (!get_option(&str, &n))
		return 0;
	panel_id = (u32)n;
	printk("panel_id : 0x%08x\n", panel_id);
	return 1;
}
__setup("panel_id=", panel_id_setup);

static int __init recovery_mode_setup(char *str)
{
	int n;
	if (!get_option(&str, &n))
		return 0;
	recovery_mode = n;
	return 1;
}
__setup("recovery_mode=", recovery_mode_setup);

#if defined(CONFIG_TC35876X)
static int tc358765_init(void)
{
/*
 * V_BUCK3 & V_BUCK2_1V8 always on, so just enable MIPI_EN to power on the bridge
 * MIPI_EN = GPIO_31, high/1 to enable
 */
 #if 0
       int tc3_en, err;
       static struct regulator *lvds1_1p2 = NULL,*lvds1_1p8 = NULL;

       tc3_en = mfp_to_gpio(GPIO018_GPIO_MIPI_BRIDGE_EN);
       err = gpio_request(tc3_en, "mipi en");
       if (err) {
               pr_warning("[ERROR] failed to request GPIO for mipi bridge enable\n");
               return -EIO;
       }

       if (!lvds1_1p2) {
               lvds1_1p2 = regulator_get(fbi->dev, "v_ldo16");
               if (IS_ERR(lvds1_1p2)) {
                       pr_err("%s regulator get error!\n", __func__);
                       goto regu_lcd_vdd;
               }
       }

       if (!lvds1_1p8) {
               lvds1_1p8 = regulator_get(fbi->dev, "v_ldo16");
               if (IS_ERR(lvds1_1p8)) {
                       pr_err("%s regulator get error!\n", __func__);
                       goto regu_lcd_vdd;
               }
       }
       gpio_direction_output(tc3_en, 1);
       gpio_free(tc3_en);
 #endif
       return 0;
}

static struct tc35876x_platform_data tc358765_data = {
        .platform_init = tc358765_init,
        .id = TC358765_CHIPID,
        .id_reg = TC358765_CHIPID_REG,
};

static struct tc35876x_platform_data vx5b3dx_data = {
        .platform_init = tc358765_init,
};

#endif

#ifdef CONFIG_KEYBOARD_PXA27x
static unsigned int emei_dkb_matrix_key_map[] = {
	KEY(0, 0, KEY_VOLUMEDOWN),
	KEY(1, 0, KEY_VOLUMEUP),
};

static struct pxa27x_keypad_platform_data emei_dkb_keypad_info __initdata = {
	.matrix_key_rows	= 2,
	.matrix_key_cols	= 1,
	.matrix_key_map		= emei_dkb_matrix_key_map,
	.matrix_key_map_size	= ARRAY_SIZE(emei_dkb_matrix_key_map),
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

static void mic_set_power(int on)
{
	static int mic_power_flag = 0;
	struct regulator *v_ldo = regulator_get(NULL, "v_micbias");
	//printk("@@ mic_set_power: on = %d, mic_power_flag = %d\n", on, mic_power_flag);

	if (IS_ERR(v_ldo)) {
		v_ldo = NULL;
		pr_err("Get regulator error\n");
		return;
	}

	if (on && (!mic_power_flag)) {
		//printk("mic_set_power: v_micbias on!\n");
		/* Earphone-MIC Bias 2.8V */
		regulator_set_voltage(v_ldo, 2800000, 2800000);
		regulator_enable(v_ldo);		
		mic_power_flag = 1;
	}

	if (mic_power_flag && (!on)) {
		//printk("mic_set_power: v_micbias off!\n");
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

static int pm800_plat_config(struct pm80x_chip *chip,
				struct pm80x_platform_data *pdata)
{
	int data, i;
	int reg;

	if (!chip || !pdata || !chip->regmap || !chip->subchip
	    || !chip->subchip->regmap_power) {
		pr_err("%s:chip or pdata is not availiable!\n", __func__);
		return -EINVAL;
	}

	reg = 0xD0;
	for (i = 0; i < 0x20; i++ ) {
		regmap_read(chip->regmap, reg, &data);
		printk("[0x%2x: 0x%2x] ", reg, data);
		reg ++;
		if ((i+1)%8 == 0)
			printk("\n");
	}

	/* RESET_OUTn, RTC_RESET_MODE =0 */
	regmap_write(chip->regmap, PM800_RTC_MISC1, 0xb0);

	/* Set internal digital sleep voltage as 1.2V */
	regmap_write(chip->regmap, PM800_LOW_POWER1, 0x0);
	/* Enable 32Khz-out-3 low jitter XO_LJ = 1 */
	regmap_write(chip->regmap, PM800_LOW_POWER2, 0x20);

	/* Enabele LDO and BUCK clock gating in lpm */
	regmap_write(chip->regmap, PM800_LOW_POWER_CONFIG3, 0x80);

	/* Disable reference group sleep mode */
	regmap_write(chip->regmap, PM800_LOW_POWER_CONFIG4, 0x00);
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
	/* use a 1x scaling of the GPADC duty cycle
	 * when in sleep (instead of 2x, 4x, 8x).
	*/
	regmap_write(chip->subchip->regmap_gpadc,
	                 PM800_GPADC_MISC_CONFIG2, 0x11);
	/* Enlarge GPADC off slots */
	regmap_write(chip->subchip->regmap_gpadc, 0x08, 0x0f);

	/* Set buck1 sleep mode as 0.8V */
	regmap_write(chip->subchip->regmap_power, PM800_SLEEP_BUCK1, 0x10);

	/* Set buck1 audio mode as 0.8VDO */
	regmap_write(chip->subchip->regmap_power, PM800_AUDIO_MODE_CONFIG1,0x10);

	/* Enable buck sleep mode */
	regmap_write(chip->subchip->regmap_power, PM800_BUCK_SLP1, 0xaa);
	regmap_write(chip->subchip->regmap_power, PM800_BUCK_SLP2, 0x2);

	/* Enable LDO sleep mode */
	regmap_write(chip->subchip->regmap_power, PM800_LDO_SLP1, 0xa8);
	regmap_write(chip->subchip->regmap_power, PM800_LDO_SLP2, 0xaa);
	regmap_write(chip->subchip->regmap_power, PM800_LDO_SLP3, 0xaa);
	regmap_write(chip->subchip->regmap_power, PM800_LDO_SLP4, 0xaa);
	regmap_write(chip->subchip->regmap_power, PM800_LDO_SLP5, 0xaa);

	/*WIFI may power on in sleep mode, so set sleep voltage 3.3V*/
	regmap_write(chip->subchip->regmap_power, PM800_LDO9, 0xff);

	/*set LDO19 for GPS sleep voltage 1.8V*/
	regmap_write(chip->subchip->regmap_power, PM800_LDO19, 0x11);

	/*
	* Base page 0x50 and 0x55 should be set to 0x0C to allow PMIC
	* Buck clock and digital clocks to be locked to the 32KHz clock,
	* but make sure USE_XO field (Bas page 0xD0.7) is previously set.
	*
	* Once you set 0x0C, if you read back you will read 0x0D, as the
	* LSB is a Read Only bit representing the ?lock??fla which will
	* be set shortly after bit 1 of the same register is set to 0.
	*/
	data = 0x0C;
	regmap_write(chip->regmap, OSC_CNTRL1, data);
	regmap_write(chip->regmap, OSC_CNTRL6, data);

	/* Forcing the clock of the bucks to be active also during sleep */
	regmap_read(chip->regmap, OSC_CNTRL3, &data);
	data |= (1 << 4) | (1 << 7);
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

 	/*
         * In order to fix DSC & PCS phase error detected in
           LT02 product test.
	 */
        regmap_read(chip->subchip->regmap_power,
			    PM800_BUCK2_MISC2, &data);
	data &= ~(0x3 << 5);
	data |= (0x2 << 5);
	regmap_write(chip->subchip->regmap_power,
			     PM800_BUCK2_MISC2, data);

	regmap_read(chip->subchip->regmap_power,
			    PM800_BUCK4_MISC2, &data);
	data &= ~(0x3 << 5);
	data |= (0x2 << 5);
	regmap_write(chip->subchip->regmap_power,
			     PM800_BUCK4_MISC2, data);

	/* BUCK enable 0x50, BUCK1, 2, 3, 4 */
	regmap_write(chip->subchip->regmap_power, 0x50, 0x0f);
	/* LDO enable: LDO 3, 5, 7, 14, 16, 17 and 18 */
	regmap_write(chip->subchip->regmap_power, 0x51, 0x54);
	regmap_write(chip->subchip->regmap_power, 0x52, 0xa0);
	regmap_write(chip->subchip->regmap_power, 0x53, 0x07);

	/* Dump power-down log */
	regmap_read(chip->regmap, PM800_POWER_DOWN_LOG1, &data);
	pr_info("PowerDW Log1 0x%x: 0x%x\n", PM800_POWER_DOWN_LOG1, data);
	regmap_read(chip->regmap, PM800_POWER_DOWN_LOG2, &data);
	pr_info("PowerDW Log2 0x%x: 0x%x\n", PM800_POWER_DOWN_LOG2, data);
	/* Clear power-down log */
	regmap_write(chip->regmap, PM800_POWER_DOWN_LOG1, 0xff);
	regmap_write(chip->regmap, PM800_POWER_DOWN_LOG2, 0xff);

	/* Write svc level values except level 0 */
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

#if defined(CONFIG_TOUCHSCREEN_BT532_TS)
		regmap_read(chip->regmap, PM800_GPIO_0_1_CNTRL, &data);
		pr_info("bt532: TSP VENDOR %x\n",data);

//		data = PM800_GPIO0_GPIO_MODE(0X00) | PM800_GPIO0_VAL |
//				PM800_GPIO1_GPIO_MODE(0X00) | PM800_GPIO1_VAL;
//		regmap_write(chip->regmap, PM800_GPIO_0_1_CNTRL, data);
//		regmap_read(chip->regmap, PM800_GPIO_0_1_CNTRL, &data);
//		pr_info("bt532: TSP VENDOR %x\n",data);
#endif

	/* disable over voltage on VSYS node */
	regmap_write(chip->regmap, 0x1f, 0x01);
	regmap_write(chip->subchip->regmap_test, 0x43, 0xff);
	regmap_write(chip->subchip->regmap_test, 0x44, 0xbf);
	regmap_write(chip->subchip->regmap_test, 0x45, 0xff);
	regmap_write(chip->subchip->regmap_test, 0x46, 0x16);
	regmap_write(chip->regmap, 0x1f, 0x0);

	return 0;
}

static int pm805_workaround(struct pm80x_chip *chip)
{
	static unsigned char buf[65];
	/* 1 */
	regmap_write(chip->regmap, 0xfa, 0x0);
	regmap_write(chip->regmap, 0xfb, 0x0);
	/* 2 */
	pm800_extern_write(PM80X_BASE_PAGE, 0xd0, 0x80);
	pm800_extern_write(PM80X_BASE_PAGE, 0x0d, 0x80);
	pm800_extern_write(PM80X_BASE_PAGE, 0x1d, 0x01);
	pm800_extern_write(PM80X_BASE_PAGE, 0x21, 0x20);
	pm800_extern_write(PM80X_BASE_PAGE, 0xe2, 0x2a);

	/* 3 */
	regmap_write(chip->regmap, 0xbd, 0x80);
	regmap_write(chip->regmap, 0xbd, 0x80);
	regmap_write(chip->regmap, 0xbd, 0x80);
	regmap_write(chip->regmap, 0xbd, 0x80);
	regmap_write(chip->regmap, 0xbd, 0x80);
	msleep(10);
	regmap_write(chip->regmap, 0x01, 0x03);
	regmap_write(chip->regmap, 0x80, 0xad);
	msleep(20);

	/* 4 */
	regmap_write(chip->regmap, 0xc1, 0xe0);
	regmap_write(chip->regmap, 0xc1, 0xe0);
	regmap_write(chip->regmap, 0xc1, 0xe0);
	regmap_write(chip->regmap, 0xc1, 0xe0);
	regmap_write(chip->regmap, 0xc1, 0xe0);
	msleep(200);
	regmap_write(chip->regmap, 0x29, 0x08);
	regmap_write(chip->regmap, 0x26, 0x0);
	msleep(20);
	regmap_write(chip->regmap, 0x01, 0x02);
	regmap_write(chip->regmap, 0x80, 0x00);
	regmap_write(chip->regmap, 0x01, 0x03);
	msleep(20);

	/* 5 */
	regmap_write(chip->regmap, 0x07, 0x00);
	regmap_write(chip->regmap, 0x08, 0x00);
	regmap_write(chip->regmap, 0x0a, 0x04);
	regmap_write(chip->regmap, 0x80, 0x00);
	regmap_write(chip->regmap, 0x82, 0x10);
	regmap_write(chip->regmap, 0x95, 0x44);
	regmap_write(chip->regmap, 0x96, 0x71);
	regmap_write(chip->regmap, 0x97, 0x12);
	regmap_write(chip->regmap, 0x99, 0xf0);
	regmap_write(chip->regmap, 0xbd, 0x00);
	regmap_write(chip->regmap, 0xbf, 0x00);
	regmap_write(chip->regmap, 0xcc, 0x00);
	regmap_write(chip->regmap, 0xcd, 0x00);
	regmap_write(chip->regmap, 0xce, 0x04);
	regmap_write(chip->regmap, 0xcf, 0x00);
	regmap_write(chip->regmap, 0xd0, 0x00);
	regmap_write(chip->regmap, 0xd1, 0x00);
	regmap_write(chip->regmap, 0xdc, 0x00);
	regmap_write(chip->regmap, 0xdd, 0x00);
	regmap_write(chip->regmap, 0xde, 0x00);
	regmap_write(chip->regmap, 0xe2, 0x00);
	regmap_write(chip->regmap, 0xe3, 0x58);

	/* 6 */
	regmap_write(chip->regmap, 0x02, 0x00);
	regmap_write(chip->regmap, 0x03, 0x00);
	regmap_write(chip->regmap, 0x04, 0x00);
	regmap_write(chip->regmap, 0x05, 0x00);
	regmap_write(chip->regmap, 0x06, 0x00);
	regmap_write(chip->regmap, 0x10, 0x14);
	regmap_write(chip->regmap, 0x11, 0x00);
	regmap_write(chip->regmap, 0x12, 0x00);
	regmap_write(chip->regmap, 0x13, 0x00);
	regmap_write(chip->regmap, 0x14, 0x00);
	regmap_write(chip->regmap, 0x15, 0x00);
	regmap_write(chip->regmap, 0x16, 0x00);
	regmap_write(chip->regmap, 0x20, 0x12);
	regmap_write(chip->regmap, 0x21, 0x38);
	regmap_write(chip->regmap, 0x22, 0x00);
	regmap_write(chip->regmap, 0x23, 0x1b);
	regmap_write(chip->regmap, 0x24, 0x00);
	regmap_write(chip->regmap, 0x25, 0x00);
	regmap_write(chip->regmap, 0x26, 0x00);
	regmap_write(chip->regmap, 0x27, 0x00);
	regmap_write(chip->regmap, 0x29, 0x08);
	regmap_write(chip->regmap, 0x2a, 0x00);
	regmap_write(chip->regmap, 0x30, 0x1f);
	regmap_write(chip->regmap, 0x31, 0x4c);
	regmap_write(chip->regmap, 0x32, 0x04);
	regmap_write(chip->regmap, 0x33, 0x00);
	regmap_write(chip->regmap, 0x34, 0x00);
	regmap_write(chip->regmap, 0x35, 0x14);
	regmap_write(chip->regmap, 0x36, 0x23);
	regmap_write(chip->regmap, 0x37, 0x04);
	regmap_write(chip->regmap, 0x38, 0x00);
	regmap_write(chip->regmap, 0x39, 0x14);
	regmap_write(chip->regmap, 0x3b, 0x00);
	regmap_write(chip->regmap, 0x3c, 0x23);
	regmap_write(chip->regmap, 0x3d, 0x01);
	regmap_write(chip->regmap, 0x3e, 0x00);
	regmap_write(chip->regmap, 0x3f, 0x00);
	regmap_write(chip->regmap, 0x40, 0x00);
	regmap_write(chip->regmap, 0x41, 0x00);
	regmap_write(chip->regmap, 0x42, 0x00);
	regmap_write(chip->regmap, 0x50, 0x00);
	regmap_write(chip->regmap, 0x51, 0x00);
	regmap_write(chip->regmap, 0x52, 0x01);
	regmap_write(chip->regmap, 0x53, 0x01);
	regmap_write(chip->regmap, 0x54, 0x55);
	regmap_write(chip->regmap, 0x55, 0x20);
	regmap_write(chip->regmap, 0x56, 0x00);
	regmap_write(chip->regmap, 0x57, 0x00);
	regmap_write(chip->regmap, 0x58, 0x00);
	regmap_write(chip->regmap, 0x59, 0x00);
	regmap_write(chip->regmap, 0x5a, 0x00);
	regmap_write(chip->regmap, 0x5b, 0x01);

	buf[0] = 0x5c;
	buf[1] = 0x0;
	buf[2] = 0x0;
	buf[3] = 0x0;
	buf[4] = 0x0;
	buf[5] = 0x0;
	buf[6] = 0x0;
	buf[7] = 0x0;
	buf[8] = 0x0;
	buf[9] = 0x0;
	i2c_master_send(chip->client, &buf[0], 10);

	buf[1] = 0x04;
	i2c_master_send(chip->client, &buf[0], 10);

	buf[1] = 0x08;
	i2c_master_send(chip->client, &buf[0], 10);

	buf[1] = 0x0c;
	i2c_master_send(chip->client, &buf[0], 10);

	buf[1] = 0x10;
	i2c_master_send(chip->client, &buf[0], 10);

	buf[1] = 0x14;
	i2c_master_send(chip->client, &buf[0], 10);

	buf[1] = 0x18;
	i2c_master_send(chip->client, &buf[0], 10);

	buf[1] = 0x1c;
	i2c_master_send(chip->client, &buf[0], 10);
	regmap_write(chip->regmap, 0x5b, 0x02);

	buf[0] = 0x5c;
	buf[1] = 0x0;
	buf[2] = 0x10;
	buf[3] = 0x00;
	buf[4] = 0x00;
	buf[5] = 0x00;
	buf[6] = 0x00;
	buf[7] = 0x00;
	buf[8] = 0x00;
	buf[9] = 0x00;
	buf[10] = 0x00;
	buf[11] = 0x00;
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x03);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x04);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x05);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x06);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x07);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x08);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x09);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x0a);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x0b);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x0c);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x0d);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x0e);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x0f);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x10);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x11);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x12);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x13);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x14);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x15);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5d, 0x00);
	regmap_write(chip->regmap, 0x5b, 0x00);
	regmap_write(chip->regmap, 0x84, 0x88);
	regmap_write(chip->regmap, 0x85, 0x16);
	regmap_write(chip->regmap, 0x86, 0x16);
	regmap_write(chip->regmap, 0x87, 0x00);
	regmap_write(chip->regmap, 0x88, 0x40);
	regmap_write(chip->regmap, 0x8a, 0x08);
	regmap_write(chip->regmap, 0x8b, 0x05);
	regmap_write(chip->regmap, 0x8c, 0x00);
	regmap_write(chip->regmap, 0x8d, 0x00);
	regmap_write(chip->regmap, 0x8e, 0x09);
	regmap_write(chip->regmap, 0x8f, 0x00);
	regmap_write(chip->regmap, 0x90, 0x7e);
	regmap_write(chip->regmap, 0x91, 0x03);
	regmap_write(chip->regmap, 0x92, 0x55);
	regmap_write(chip->regmap, 0x93, 0x04);
	regmap_write(chip->regmap, 0x94, 0x02);
	regmap_write(chip->regmap, 0x99, 0xf0);
	regmap_write(chip->regmap, 0x9a, 0x12);

	regmap_write(chip->regmap, 0xa0, 0x00);
	regmap_write(chip->regmap, 0xa1, 0x00);
	regmap_write(chip->regmap, 0xa2, 0x00);
	regmap_write(chip->regmap, 0xa3, 0x00);
	regmap_write(chip->regmap, 0xa4, 0x00);
	regmap_write(chip->regmap, 0xa5, 0x00);
	regmap_write(chip->regmap, 0xa6, 0x00);
	regmap_write(chip->regmap, 0xa7, 0x00);
	regmap_write(chip->regmap, 0xa8, 0x00);
	regmap_write(chip->regmap, 0xaa, 0x00);
	regmap_write(chip->regmap, 0xac, 0x00);
	regmap_write(chip->regmap, 0xae, 0x00);

	regmap_write(chip->regmap, 0xb0, 0x00);
	regmap_write(chip->regmap, 0xb2, 0x00);
	regmap_write(chip->regmap, 0xb4, 0x00);
	regmap_write(chip->regmap, 0xb6, 0x00);
	regmap_write(chip->regmap, 0xb8, 0x00);
	regmap_write(chip->regmap, 0xba, 0x00);
	regmap_write(chip->regmap, 0xbc, 0x00);

	regmap_write(chip->regmap, 0xc1, 0x00);
	regmap_write(chip->regmap, 0xc2, 0x00);
	regmap_write(chip->regmap, 0xc3, 0x00);
	regmap_write(chip->regmap, 0xc4, 0x00);
	regmap_write(chip->regmap, 0xc9, 0x00);
	regmap_write(chip->regmap, 0xca, 0x00);

	regmap_write(chip->regmap, 0xd2, 0x00);
	regmap_write(chip->regmap, 0xd3, 0x00);
	regmap_write(chip->regmap, 0xd4, 0x00);
	regmap_write(chip->regmap, 0xd5, 0x00);
	regmap_write(chip->regmap, 0xd6, 0x00);
	regmap_write(chip->regmap, 0xd7, 0x00);
	regmap_write(chip->regmap, 0xd8, 0x00);
	regmap_write(chip->regmap, 0xd9, 0x00);
	regmap_write(chip->regmap, 0xda, 0x00);
	regmap_write(chip->regmap, 0xdb, 0x00);
	regmap_write(chip->regmap, 0x01, 0x02);

	msleep(10);
	regmap_write(chip->regmap, 0xfc, 0x00);

	pm800_extern_write(PM80X_BASE_PAGE, 0xe2, 0x2a);
	msleep(1);

	return 0;
}

static int pm805_plat_config(struct pm80x_chip *chip,
				struct pm80x_platform_data *pdata)
{
	if (!chip || !pdata || !chip->regmap) {
		pr_err("%s:chip or pdata is not availiable!\n", __func__);
		return -EINVAL;
	}

	/* workaround */
	pm805_workaround(chip);

	return 0;
}


#ifdef CONFIG_SAMSUNG_JACK
static struct sec_jack_zone sec_jack_zones[] = {
	{
		/* adc == 0, default to 3pole if it stays
		 * in this range for 40ms (20ms delays, 2 samples)
		 */
		.adc_high = 0,
		.delay_ms = 20,
		.check_count = 2,
		.jack_type = SEC_HEADSET_3POLE,
	},
	{
		/* 0 < adc <= 500, unstable zone, default to 3pole if it stays
		 * in this range for a 100ms (20ms delays, 5 samples)
		 */
		.adc_high = 500,
		.delay_ms = 20,
		.check_count = 5,
		.jack_type = SEC_HEADSET_3POLE,
	},
	{
		/* 500 < adc <= 1000, unstable zone, default to 4pole if it
		 * stays in this range for 100ms (20ms delays, 5 samples)
		 */
		.adc_high = 1000,
		.delay_ms = 20,
		.check_count = 5,
		.jack_type = SEC_HEADSET_4POLE,
	},
	{
		/* 1000 < adc <= 1340, default to 4 pole if it stays */
		/* in this range for 40ms (20ms delays, 2 samples)
		 */
		.adc_high = 1340,
		.delay_ms = 20,
		.check_count = 2,
		.jack_type = SEC_HEADSET_4POLE,
	},
	{
		/* adc > 1680, unstable zone, default to 3pole if it stays
		 * in this range for a second (10ms delays, 100 samples)
		 */
		.adc_high = 0x7fffffff,
		.delay_ms = 10,
		.check_count = 10,
		.jack_type = SEC_HEADSET_4POLE,
	},
};

/* to support 3-buttons earjack */
static struct sec_jack_buttons_zone sec_jack_buttons_zones[] = {
	{
		/* 0 <= adc <=97, stable zone */
		.code		= KEY_MEDIA,
		.adc_low	= 0,
		.adc_high	= 97,
	},
	{
		/* 98 <= adc <= 180, stable zone */
		.code		= KEY_VOLUMEUP,
		.adc_low	= 98,
		.adc_high	= 180,
	},
	{
		/* 181 <= adc <= 400, stable zone */
		.code		= KEY_VOLUMEDOWN,
		.adc_low	= 181,
		.adc_high	= 400,
	},
};

static struct sec_jack_platform_data sec_jack_pdata = {
	.headset_flag = 0,
	.mic_set_power = mic_set_power,
	.zones = sec_jack_zones,
	.num_zones = ARRAY_SIZE(sec_jack_zones),
	.buttons_zones = sec_jack_buttons_zones,
	.num_buttons_zones = ARRAY_SIZE(sec_jack_buttons_zones),
	.press_release_th = 450,
};
#else
static struct pm80x_headset_pdata pm80x_headset = {
	.headset_flag = 0,
	.mic_set_power = mic_set_power,
	.hook_press_th = 60,
	.vol_up_press_th = 150,
	.vol_down_press_th = 256,
	.mic_det_th = 600,
	.press_release_th = 450,
};
#endif //CONFIG_SAMSUNG_JACK

#ifdef CONFIG_ANDROID_TIMED_GPIO
static struct timed_gpio lt02_timed_gpios[] = {
	{
		.name		= "vibrator",
		.gpio		= mfp_to_gpio(GPIO009_GPIO_9),
		.max_timeout	= 10000,
		.active_low	= 0,
	},
};

static struct timed_gpio_platform_data lt02_timed_gpio_pdata = {
	.num_gpios	= ARRAY_SIZE(lt02_timed_gpios),
	.gpios		= lt02_timed_gpios,
};

static struct platform_device lt02_timed_gpios_device = {
	.name 	= TIMED_GPIO_NAME,
	.id		= -1,
	.dev	= {
			.platform_data = &lt02_timed_gpio_pdata,
	},
};
#endif

#ifdef CONFIG_VIBRATOR_ANDROID
static struct platform_device lt02_vibrator_android_device = {
	.name = "android-vibrator",
	.id = -1,
	.dev = {
	},
};
#endif

#ifdef CONFIG_VIBRATOR_88PM80X
static struct pm80x_vibrator_pdata vibrator_pdata = {
	.min_timeout = 10,
};
#endif

static struct pm80x_bat_pdata pm80x_bat = {
};

static struct pm80x_platform_data pm800_info = {
#ifdef CONFIG_SAMSUNG_JACK
	.headset = &sec_jack_pdata,
#else
	.headset = &pm80x_headset,
#endif
	.power_page_addr = 0x31,	/* POWER */
	.gpadc_page_addr = 0x32,	/* GPADC */
	.test_page_addr = 0x37,		/* TEST */
	.irq_mode = 0,	/* 0: clear IRQ by read */

#ifdef CONFIG_VIBRATOR_88PM80X
	.vibrator = &vibrator_pdata,
#endif
	.rtc = &pm80x_rtc,
	.dvc = &pm80x_dvc,
	.bat = &pm80x_bat,
	.usb = &pm80x_usb,
	.plat_config = pm800_plat_config,
};

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

#if defined(CONFIG_SENSORS_BMC150)
static struct bosch_sensor_specific bss_bmm = {
	.name = "bmm050",
	.place = 7,
};
static struct i2c_board_info i2c_bmm050={
	I2C_BOARD_INFO("bmm050", 0x12),
	.platform_data = &bss_bmm,
};
#endif //CONFIG_SENSORS_BMC150

#if defined(CONFIG_SENSORS_BMC150)
static struct bosch_sensor_specific bss_bma = {
	.name = "bma2x2",
	.place = 7,
};
static struct i2c_board_info i2c_bma2x2={
	I2C_BOARD_INFO("bma2x2", 0x10),
	.platform_data = &bss_bma,
};
#endif //CONFIG_SENSORS_BMC150

#if defined(CONFIG_SENSORS_GP2A0X0)
static int light_sensor_power(int on) {
	static struct regulator *proxy_2v85 = NULL;
	static bool init_state = 1;
	static bool light_status = 0;

	pr_info("%s : %d\n", __func__, on);

	if (on == light_status)
		return 0;

	if (init_state)
	{
		if (!proxy_2v85) {
			proxy_2v85 = regulator_get(NULL, "v_proxy_2v85");
			if (IS_ERR(proxy_2v85)) {
				pr_err("%s v_proxy_2v85 regulator get error!\n", __func__);
				proxy_2v85 = NULL;
				return -1;
			}
		}
		regulator_set_voltage(proxy_2v85, 2850000, 2850000);
		init_state = 0;
	}

	if (on) {
		regulator_enable(proxy_2v85);
		msleep(2);
		light_status = 1;
	} else {
		regulator_disable(proxy_2v85);
		light_status = 0;
	}
	
	return 0;
}
#endif

#if defined(CONFIG_SENSORS_GP2A0X0)
static struct gp2a_platform_data gp2a_pdata = {
	.power_on = light_sensor_power,
	.led_on = NULL,
	.p_out = mfp_to_gpio(GPIO092_GPIO_92),
	.version = 1,
	.prox_cal_path = "/efs/prox_cal",
};

static struct i2c_board_info i2c_gp2a = {
	I2C_BOARD_INFO("gp2a0x0", 0x39),
	.platform_data = &gp2a_pdata,
};

static void gp2a_init(int rev)
{
	pr_info("gp2a system rev is %d\n", rev);
	gp2a_pdata.d0_value[D0_BND] = 91;
	gp2a_pdata.d0_value[D0_COND1] = 40;
	gp2a_pdata.d0_value[D0_COND1_A] = 978;
	gp2a_pdata.d0_value[D0_COND2] = 62;
	gp2a_pdata.d0_value[D0_COND2_A] = 2448;
	gp2a_pdata.d0_value[D0_COND2_B] = 3673;
	gp2a_pdata.d0_value[D0_COND3_A] = 534;
	gp2a_pdata.d0_value[D0_COND3_B] = 587;
}	

#endif

#if defined(CONFIG_SENSORS_GP2A0X0) || defined(CONFIG_SENSORS_BMC150)
static struct i2c_gpio_platform_data i2c_gpio_data = {
	.sda_pin		= mfp_to_gpio(GPIO036_GPIO_36),
	.scl_pin		= mfp_to_gpio(GPIO035_GPIO_35),
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

#endif

#ifdef CONFIG_NFC_PN547
static struct i2c_gpio_platform_data i2c_pn547_data = {
	.sda_pin		= mfp_to_gpio(GPIO054_GPIO_54),
	.scl_pin		= mfp_to_gpio(GPIO053_GPIO_53),
	.udelay			= 3,
	.timeout = 100,
};

static struct platform_device i2c_nfc_device = {
	.name	= "i2c-gpio",
	.id	= 9,
	.dev	= {
		.platform_data	= &i2c_pn547_data,
	},
};

static struct pn544_i2c_platform_data pn547_pdata = {
	.irq_gpio = mfp_to_gpio(GPIO091_GPIO_91),
	.ven_gpio = mfp_to_gpio(MFP_PIN_GPIO126),  //MFP_PIN_GPIO126
	.firm_gpio = mfp_to_gpio(MFP_PIN_GPIO127),   // MFP_PIN_GPIO127
};

static struct i2c_board_info __initdata pn547_i2c[] = {
	{
		I2C_BOARD_INFO("pn547", 0x2B),
		.irq = MMP_GPIO_TO_IRQ(mfp_to_gpio(GPIO091_GPIO_91)),
		.platform_data = &pn547_pdata,
	},
};

static unsigned long __initdata pn547_pin_config[] = {
#define GPIO0126_NFC_EN SM_BEN0_GPIO_126 | MFP_PULL_LOW
#define GPIO0127_NFC_FIRMWARE SM_BEN1_GPIO_127 | MFP_PULL_LOW
#define GPIO091_NFC_IRQ GPIO091_GPIO_91 | MFP_LPM_INPUT | MFP_PULL_LOW
#define GPIO053_NFC_SCL GPIO053_GPIO_53 | MFP_LPM_FLOAT
#define GPIO054_NFC_SDA GPIO054_GPIO_54 | MFP_LPM_FLOAT

	GPIO054_NFC_SDA,
	GPIO053_NFC_SCL,
	GPIO091_NFC_IRQ,	
	GPIO0127_NFC_FIRMWARE,
	GPIO0126_NFC_EN,
};


static void pn547_gpio_init(void)
{
	mfp_config(ARRAY_AND_SIZE(pn547_pin_config));
}

#endif

#ifdef CONFIG_SENSORS_ASP01
static struct i2c_gpio_platform_data i2c_grip_data = {
	.sda_pin		= mfp_to_gpio(GPIO054_GPIO_54),
	.scl_pin		= mfp_to_gpio(GPIO053_GPIO_53),
	.udelay			= 10,
	.timeout = 100,
};

static struct platform_device i2c_grip_device = {
	.name	= "i2c-gpio",
	.id	= 10,
	.dev	= {
		.platform_data	= &i2c_grip_data,
	},
};

static struct asp01_platform_data asp01_pdata = {
	.t_out = mfp_to_gpio(MFP_PIN_GPIO108),
	.adj_det = mfp_to_gpio(MFP_PIN_GPIO106),
	.irq = MMP_GPIO_TO_IRQ(mfp_to_gpio(MFP_PIN_GPIO108)),
};

static struct i2c_board_info i2c_devs10 __initdata = {
	I2C_BOARD_INFO("asp01", (0x48 >> 1)),
	.platform_data = &asp01_pdata,
};

static int grip_gpio_init(void)
{
	unsigned int grip_gpio;
	int ret;
	grip_gpio = mfp_to_gpio(MFP_PIN_GPIO108);
	ret = gpio_request(grip_gpio, "asp01_grip_irq");
	/* Grip sensor interrupt pin initialization */
	gpio_direction_input(grip_gpio);
	gpio_free(grip_gpio);
	/* test jig detect pin initialization */
	grip_gpio = mfp_to_gpio(MFP_PIN_GPIO106);
	ret = gpio_request(grip_gpio, "asp01_adj_det");
	gpio_direction_input(grip_gpio);
	gpio_free(grip_gpio);
	return ret;
}

static void grip_init_code_set(void)
{
	asp01_pdata.cr_divsr = 10;
	asp01_pdata.cr_divnd = 13;
	asp01_pdata.cs_divsr = 10;
	asp01_pdata.cs_divnd = 13;

	asp01_pdata.init_code[SET_UNLOCK] =	0x5a;
	asp01_pdata.init_code[SET_RST_ERR] =	0x33;
	asp01_pdata.init_code[SET_PROX_PER] =	0x34;
	asp01_pdata.init_code[SET_PAR_PER] =	0x34;
	asp01_pdata.init_code[SET_TOUCH_PER] =	0x3c;
	asp01_pdata.init_code[SET_HI_CAL_PER] =	0x18;
	asp01_pdata.init_code[SET_BSMFM_SET] =	0x31;
	asp01_pdata.init_code[SET_ERR_MFM_CYC] =0x33;
	asp01_pdata.init_code[SET_TOUCH_MFM_CYC] =0x25;
	asp01_pdata.init_code[SET_HI_CAL_SPD] =	0x19;
	asp01_pdata.init_code[SET_CAL_SPD] =	0x03;
	/* asp01_pdata.init_code[SET_INIT_REF] = 0x00; */
	asp01_pdata.init_code[SET_BFT_MOT] =	0x40;
	asp01_pdata.init_code[SET_TOU_RF_EXT] =	0x00;
	asp01_pdata.init_code[SET_SYS_FUNC] =	0x10;
	asp01_pdata.init_code[SET_OFF_TIME] =	0x50;
	asp01_pdata.init_code[SET_SENSE_TIME] =	0x50;
	asp01_pdata.init_code[SET_DUTY_TIME] =	0x50;
	asp01_pdata.init_code[SET_HW_CON1] =	0x78;
	asp01_pdata.init_code[SET_HW_CON2] =	0x27;
	asp01_pdata.init_code[SET_HW_CON3] =	0x20;
	asp01_pdata.init_code[SET_HW_CON4] =	0x27;
	asp01_pdata.init_code[SET_HW_CON5] =	0x83;
	asp01_pdata.init_code[SET_HW_CON6] =	0x3f;
	asp01_pdata.init_code[SET_HW_CON7] =	0x48;
	/* asp01_pdata.init_code[SET_HW_CON8] =	0x20; */
	asp01_pdata.init_code[SET_HW_CON10] =	0x27;
	asp01_pdata.init_code[SET_HW_CON11] =	0x00;
}

#endif //CONFIG_SENSORS_ASP01


#if defined(CONFIG_BQ24157_CHARGER)
static struct bq24157_platform_data  bq24157_charger_info = {
	.cd = mfp_to_gpio(GPIO098_GPIO_98),
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
	},
#endif
#if defined(CONFIG_BQ24157_CHARGER)
	{ 
		.type		= "bq24157_6A",
		.addr		= 0x6A,
		.platform_data	= &bq24157_charger_info,
		.irq		= MMP_GPIO_TO_IRQ(mfp_to_gpio(GPIO029_GPIO_29)), 
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
	.charger_cb = sec_charger_cb,
};

static struct i2c_board_info __initdata fsa9480_i2c_devices[] = {
	{
		I2C_BOARD_INFO("fsa9480", 0x25),
		.platform_data	= &FSA9480_info,
		.irq = MMP_GPIO_TO_IRQ(mfp_to_gpio(GPIO093_GPIO_93)),
	},
};
#endif

#if 0
static struct stc311x_platform_data stc3115_platform_data = {
        .battery_online = NULL,
        .charger_online = NULL,		// used in stc311x_get_status()
        .charger_enable = NULL,		// used in stc311x_get_status()
        .power_supply_register = NULL,
        .power_supply_unregister = NULL,

		.Vmode= 0,       /*REG_MODE, BIT_VMODE 1=Voltage mode, 0=mixed mode */
		.Alm_SOC = 10,      /* SOC alm level %*/
		.Alm_Vbat = 3600,   /* Vbat alm level mV*/
		.CC_cnf = 333,      /* nominal CC_cnf, coming from battery characterisation*/
  		.VM_cnf = 337,      /* nominal VM cnf , coming from battery characterisation*/
		.Cnom = 1700,       /* nominal capacity in mAh, coming from battery characterisation*/
		.Rsense = 10,       /* sense resistor mOhms*/
		.RelaxCurrent = 85, /* current for relaxation in mA (< C/20) */
		.Adaptive = 1,     /* 1=Adaptive mode enabled, 0=Adaptive mode disabled */

		.CapDerating[6] = 0,   /* capacity derating in 0.1%, for temp = -20C */
  		.CapDerating[5] = 0,   /* capacity derating in 0.1%, for temp = -10C */
		.CapDerating[4] = 0,   /* capacity derating in 0.1%, for temp = 0C */
		.CapDerating[3] = 0,   /* capacity derating in 0.1%, for temp = 10C */
		.CapDerating[2] = 0,  /* capacity derating in 0.1%, for temp = 25C */
		.CapDerating[1] = 0,   /* capacity derating in 0.1%, for temp = 40C */
		.CapDerating[0] = 0,   /* capacity derating in 0.1%, for temp = 60C */

  		.OCVOffset[15] = -117,    /* OCV curve adjustment */
		.OCVOffset[14] = -17,   /* OCV curve adjustment */
		.OCVOffset[13] = -12,    /* OCV curve adjustment */
		.OCVOffset[12] = -9,    /* OCV curve adjustment */
		.OCVOffset[11] = 0,    /* OCV curve adjustment */
		.OCVOffset[10] = -9,    /* OCV curve adjustment */
		.OCVOffset[9] = -13,     /* OCV curve adjustment */
		.OCVOffset[8] = -8,      /* OCV curve adjustment */
		.OCVOffset[7] = -1,      /* OCV curve adjustment */
		.OCVOffset[6] = 10,    /* OCV curve adjustment */
		.OCVOffset[5] = 7,    /* OCV curve adjustment */
		.OCVOffset[4] = 17,     /* OCV curve adjustment */
		.OCVOffset[3] = 41,    /* OCV curve adjustment */
		.OCVOffset[2] = 32,     /* OCV curve adjustment */
		.OCVOffset[1] = -69,    /* OCV curve adjustment */
		.OCVOffset[0] = 42,     /* OCV curve adjustment */

		.OCVOffset2[15] = -24,    /* OCV curve adjustment */
		.OCVOffset2[14] = -21,   /* OCV curve adjustment */
		.OCVOffset2[13] = -20,    /* OCV curve adjustment */
		.OCVOffset2[12] = -12,    /* OCV curve adjustment */
		.OCVOffset2[11] = -14,    /* OCV curve adjustment */
		.OCVOffset2[10] = -25,    /* OCV curve adjustment */
		.OCVOffset2[9] = -11,     /* OCV curve adjustment */
		.OCVOffset2[8] = -5,      /* OCV curve adjustment */
		.OCVOffset2[7] = 2,      /* OCV curve adjustment */
		.OCVOffset2[6] = 2,    /* OCV curve adjustment */
		.OCVOffset2[5] = 5,    /* OCV curve adjustment */
		.OCVOffset2[4] = 13,     /* OCV curve adjustment */
		.OCVOffset2[3] = 9,    /* OCV curve adjustment */
		.OCVOffset2[2] = -38,     /* OCV curve adjustment */
		.OCVOffset2[1] = -51,    /* OCV curve adjustment */
		.OCVOffset2[0] = 0,     /* OCV curve adjustment */

			/*if the application temperature data is preferred than the STC3115 temperature*/
		.ExternalTemperature = NULL, /*External temperature fonction, return C*/
		.ForceExternalTemperature = 0, /* 1=External temperature, 0=STC3115 temperature */
};

static struct i2c_gpio_platform_data i2c_stc3115_bus_data = {
    .sda_pin = mfp_to_gpio(MFP_PIN_GPIO74),
	  .scl_pin = mfp_to_gpio(MFP_PIN_GPIO73),
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
		.irq = MMP_GPIO_TO_IRQ(TSP_INT),
	},
};
#endif

#if defined(CONFIG_TOUCHSCREEN_BT532_TS)
#define TSP_INT_LT02_R0_0		mfp_to_gpio(GPIO094_GPIO_94)
#define TSP_SCL_LT02_R0_0		mfp_to_gpio(GPIO016_GPIO_16)
#define TSP_SDA_LT02_R0_0		mfp_to_gpio(GPIO017_GPIO_17)
#define TSP_LDO_ON_LT02_R0_0	mfp_to_gpio(GPIO020_GPIO_20)
#define TSP_LDO_ON_LT02_R0_1	mfp_to_gpio(GPIO096_GPIO_96)

static struct bt532_ts_platform_data bt532_ts_pdata = {
	.gpio_int		= TSP_INT_LT02_R0_0,
	.x_resolution	= 599,
	.y_resolution	= 1023,
	.page_size	= 128,
	.orientation	= 0x00,
};

static int __init bt532_ts_init(void)
{
	int ret = 0;
	u32 tsp_ldo_en = TSP_LDO_ON_LT02_R0_0;

	if (system_rev > LT02_R0_0)
		tsp_ldo_en = TSP_LDO_ON_LT02_R0_1;

	bt532_ts_pdata.gpio_ldo_en = tsp_ldo_en;

	ret = gpio_request(tsp_ldo_en, "bt532_ldo_en");
	if (ret < 0) {
		pr_err("bt532: Failed to obtain gpio for ldo pin\n");
		return ret;
	}

	gpio_direction_output(tsp_ldo_en, 0);

	/* configure touchscreen interrupt gpio */
	ret = gpio_request(TSP_INT_LT02_R0_0, "bt532_int");
	if (ret < 0) {
		pr_err("bt532: Failed to obtain gpio for int\n");
		return ret;
	}

	gpio_direction_input(TSP_INT_LT02_R0_0);

	pr_info("bt532: initialize pins\n");

	return 0;
}

static struct i2c_gpio_platform_data i2c_bt532_bus_data = {
	.sda_pin = TSP_SDA_LT02_R0_0,
	.scl_pin = TSP_SCL_LT02_R0_0,
	.udelay  = 1, /* 500/udelay KHz */
};

static struct platform_device i2c_bt532_bus_device = {
	.name	= "i2c-gpio",
	.id		= 4,
	.dev	= {
		.platform_data = &i2c_bt532_bus_data,
	}
};

static struct i2c_board_info __initdata bt532_i2c_devices[] = {
	{
		I2C_BOARD_INFO(BT532_TS_DEVICE, 0x20),
		.platform_data = &bt532_ts_pdata,
		.irq = MMP_GPIO_TO_IRQ(TSP_INT_LT02_R0_0),
	},
};
#endif

#ifdef CONFIG_SEC_IRLED
#define GPIO_IRDA_SCL mfp_to_gpio(MFP_PIN_GPIO126)
#define GPIO_IRDA_SDA mfp_to_gpio(MFP_PIN_GPIO127)

static struct i2c_gpio_platform_data lt02_i2c_bus_11_data = {
	.sda_pin		= GPIO_IRDA_SDA,
	.scl_pin		= GPIO_IRDA_SCL,
	.udelay			= 10,
	.timeout		= 0,
};

static struct platform_device lt02_i2c_bus_11_device = {
	.name	= "i2c-gpio",
	.id	= 11,
	.dev	= {
		.platform_data	= &lt02_i2c_bus_11_data,
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

static struct i2c_board_info emeidkb_i2c_info[] = {
};

static struct i2c_board_info lt02_i2c2_info[] = {
#if defined(CONFIG_TC35876X)
       {
               .type           = "tc35876x",
	       .addr           = 0x64,      
               .platform_data = &vx5b3dx_data,
       },
#endif
};

static struct i2c_board_info lt02_i2c2_r03_info[] = {
#if defined(CONFIG_TC35876X)
       {
               .type           = "tc35876x",
	       .addr           = 0x64,      
               .platform_data = &vx5b3dx_data,
       },
#endif
#if defined(CONFIG_TOUCHSCREEN_BT532_TS)
	{
		I2C_BOARD_INFO(BT532_TS_DEVICE, 0x20),
		.platform_data = &bt532_ts_pdata,
		.irq = MMP_GPIO_TO_IRQ(TSP_INT_LT02_R0_0),
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
		//GPIO053_GPIO_53,		/* SCL */
		//GPIO054_GPIO_54,		/* SDA */

	};

	unsigned long i2c1_mfps[] = {
		GPIO087_GPIO_87,		/* SCL */
		GPIO088_GPIO_88,		/* SDA */
	};
	if (i2c_adap_id == 0) {
		scl = MFP_PIN_GPIO79;
		sda = MFP_PIN_GPIO80;
		//scl = MFP_PIN_GPIO53;
		//sda = MFP_PIN_GPIO54;

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
		//mfp_write(MFP_PIN_GPIO53, mfp_pin[0]);
		//mfp_write(MFP_PIN_GPIO54, mfp_pin[1]);

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
#define MFP_WIB_PDn		123
#define MFP_WIB_RESETn		34

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
		mmp_gpio_edge_add(&gpio_edge_sdio_dat1);

		/*disable buck2 sleep mode when wifi power on*/
		pm800_extern_setbits(PM80X_POWER_PAGE, PM800_BUCK_SLP1,
				PM800_BUCK2_SLP1_MASK, PM800_BUCK2_SLP1_MASK);
	}

	if (!on && enabled) {
		regulator_disable(wib_3v3);
		enabled = 0;

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

static unsigned int card_power_state(int reg)
{
	unsigned int data;

	data = pm800_extern_read(PM80X_POWER_PAGE, reg);
	printk("mmc0: eMMC Base state: %x, Power: %x\n", data);
	return data;
}

/* For emeiDKB, MMC1(SDH1) used for SD/MMC Card slot */
static struct sdhci_pxa_platdata pxa988_sdh_platdata_mmc1 = {
	.flags          = PXA_FLAG_ENABLE_CLOCK_GATING,
	.cd_type         = PXA_SDHCI_CD_GPIO,
	.clk_delay_cycles	= 0x1F,
	.host_caps_disable      = MMC_CAP_UHS_SDR12
				| MMC_CAP_UHS_SDR25
				| MMC_CAP_UHS_SDR50
				| MMC_CAP_UHS_SDR104
				| MMC_CAP_UHS_DDR50,
	.quirks			= SDHCI_QUIRK_INVERTED_WRITE_PROTECT,
	.signal_vol_change	= emeidkb_sdcard_signal,
	.ext_cd_gpio         = mfp_to_gpio(GPIO090_GPIO_90),
	.ext_cd_gpio_invert =1,
	.dtr_data		= sd_dtr_data,
};

/* For emeiDKB, MMC2(SDH2) used for WIB card */
static struct sdhci_pxa_platdata pxa988_sdh_platdata_mmc2 = {
	.flags          = PXA_FLAG_WAKEUP_HOST
				| PXA_FLAG_EN_PM_RUNTIME
				| PXA_FLAG_DISABLE_PROBE_CDSCAN,
	.cd_type         = PXA_SDHCI_CD_EXTERNAL,
	.pm_caps	= MMC_PM_KEEP_POWER,
	.quirks2	= SDHCI_QUIRK2_HOLDSUSPEND_AFTER_REQUEST,
	.dtr_data	= sdio_dtr_data,
};

/* For emeiDKB, MMC3(SDH3) used for eMMC */
static struct sdhci_pxa_platdata pxa988_sdh_platdata_mmc3 = {
	.flags		= PXA_FLAG_ENABLE_CLOCK_GATING
				| PXA_FLAG_SD_8_BIT_CAPABLE_SLOT
				| PXA_FLAG_EN_PM_RUNTIME,
	.cd_type         = PXA_SDHCI_CD_PERMANENT,
	.clk_delay_cycles	= 0xF,
	.host_caps	= MMC_CAP_1_8V_DDR,
	.dtr_data	= emmc_dtr_data,
	.pm_state	= card_power_state,
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
	pxa988_sdh_platdata_mmc1.flags	 = PXA_FLAG_EN_PM_RUNTIME
						| PXA_FLAG_ENABLE_CLOCK_GATING;
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
	.init_clk = pxa988_cam_clk_init,
	.enable_clk = pxa988_cam_set_clk,
};
#endif

#if 0
#if defined(CONFIG_SOC_CAMERA_OV2659)
static struct i2c_board_info dkb_i2c_camera[] = {
	{
		I2C_BOARD_INFO("ov2659", 0x30),
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

static struct soc_camera_link iclink_ov2659_dvp = {
	.bus_id         = 0,            /* Must match with the camera ID */
	.power          = camera_sensor_power,
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

static struct platform_device *dkb_platform_devices[] = {
#if defined(CONFIG_SOC_CAMERA_OV2659)
	&dkb_ov2659_dvp,
#endif
};
#endif

#ifdef CONFIG_KEYBOARD_GPIO
struct gpio_keys_button lt02_gpio_keys[] = {
	{
	.code = KEY_VOLUMEUP,       /* input event code (KEY_*, SW_*) */
	.gpio = mfp_to_gpio(GPIO000_GPIO_0),
	.active_low = 1,
	.desc = "volup_key",
	.type = EV_KEY,     /* input event type (EV_KEY, EV_SW) */
	.wakeup = 0,
	.debounce_interval = 30,    /* debounce ticks interval in msecs */
	.can_disable = false,
	},
	{
	.code = KEY_VOLUMEDOWN,     /* input event code (KEY_*, SW_*) */
	.gpio = mfp_to_gpio(GPIO001_GPIO_1),
	.active_low = 1,
	.desc = "voldown_key",
	.type = EV_KEY,     /* input event type (EV_KEY, EV_SW) */
	.wakeup = 0,        /* configure the button as a wake-up source */
	.debounce_interval = 30,    /* debounce ticks interval in msecs */
	.can_disable = false,
	},
	{
	.code = KEY_HOMEPAGE,     /* input event code (KEY_*, SW_*) */
	.gpio = mfp_to_gpio(GPIO013_GPIO_13),
	.active_low = 1,
	.desc = "home_key",
	.type = EV_KEY,     /* input event type (EV_KEY, EV_SW) */
	.wakeup = 1,        /* configure the button as a wake-up source */
	.debounce_interval = 30,    /* debounce ticks interval in msecs */
	.can_disable = false,
	},

};

struct gpio_keys_platform_data lt02_gpio_data = {
	.buttons = lt02_gpio_keys,
	.nbuttons = ARRAY_SIZE(lt02_gpio_keys),
};

struct platform_device lt02_gpio_keys_device = {
	.name = "gpio-keys",
	.dev = {
	.platform_data = &lt02_gpio_data,
	},
};
#endif

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

	if ((!cpu_is_z1z2()))
		devfreq_ddr_pdata.qos_list = ddr_freq_qos_table;
	pxa988_device_ddrdevfreq.dev.platform_data = (void *)&devfreq_ddr_pdata;
	platform_device_register(&pxa988_device_ddrdevfreq);
}
#endif

#if defined(CONFIG_SEC_THERMISTOR)
#if defined(CONFIG_WIFIONLY_BOARD)
static int siop_table[] = {390, 410, 430, 600, 650};
#else
static int siop_table[] = {400, 420, 440, 600, 650};
#endif
/* when the next level is same as prev, returns -1 */
static int get_pxa986_siop_level(int temp)
{
	static int prev_temp = 300;
	static int prev_level;
	int level = -1;

	if (temp > prev_temp) {
		level = 4;
		while (level > 0 && temp < siop_table[level])
			level--;
	} else {
		level = 0;
		while (level < 4 && temp >= siop_table[level])
			level++;

		if (level > prev_level)
			level = prev_level;
	}

	prev_temp = temp;
	if (prev_level == level)
		return -1;

	prev_level = level;

	return level;
}

#ifdef CONFIG_WIFIONLY_BOARD
static struct sec_therm_adc_table adc_ch0_table[] = {
	/* ADC, Temperature */
	{ 66,   700 },
	{ 67,   690 },
	{ 69,   680 },
	{ 73,   670 },
	{ 78,   660 },
	{ 82,   650 },
	{ 88,   640 },
	{ 90,   630 },
	{ 93,   620 },
	{ 96,   610 },
	{ 99,   600 },
	{ 101,  590 },
	{ 103,  580 },
	{ 106,  570 },
	{ 108,  560 },
	{ 113,  550 },
	{ 117,  540 },
	{ 121,  530 },
	{ 125,  520 },
	{ 129,  510 },
	{ 133,  500 },
	{ 138,  490 },
	{ 142,  480 },
	{ 145,  470 },
	{ 151,  460 },
	{ 155,  450 },
	{ 161,  440 },
	{ 166,  430 },
	{ 171,  420 },
	{ 179,  410 },
	{ 184,  400 },
	{ 192,  390 },
	{ 198,  380 },
	{ 205,  370 },
	{ 212,  360 },
	{ 218,  350 },
	{ 227,  340 },
	{ 236,  330 },
	{ 247,  320 },
	{ 255,  310 },
	{ 264,  300 },
	{ 275,  290 },
	{ 288,  280 },
	{ 297,  270 },
	{ 308,  260 },
	{ 314,  250 },
	{ 320,  240 },
	{ 345,  230 },
	{ 357,  220 },
	{ 370,  210 },
	{ 382,  200 },
	{ 396,  190 },
	{ 411,  180 },
	{ 427,  170 },
	{ 442,  160 },
	{ 458,  150 },
	{ 479,  140 },
	{ 498,  130 },
	{ 520,  120 },
	{ 541,  110 },
	{ 562,  100 },
	{ 587,  90 },
	{ 618,  80 },
	{ 642,  70 },
	{ 668,  60 },
	{ 694,  50 },
	{ 707,  40 },
	{ 718,  30 },
	{ 735,  20 },
	{ 750,  10 },
	{ 763,  0 },
	{ 795,  -10 },
	{ 829,  -20 },
	{ 862,  -30 },
	{ 890,  -40 },
	{ 921,  -50 },
	{ 970,  -60 },
	{ 1015,  -70 },
	{ 1064,  -80 },
	{ 1106,  -90 },
	{ 1148,  -100 },
};
#else
static struct sec_therm_adc_table adc_ch0_table[] = {
	/* ADC, Temperature */
	{ 75,   -200 },
	{ 79,   -190 },
	{ 84,   -180 },
	{ 89,   -170 },
	{ 93,   -160 },
	{ 97,   -150 },
	{ 102,  -140 },
	{ 108,  -130 },
	{ 113,  -120 },
	{ 119,  -110 },
	{ 125,  -100 },
	{ 130,  -90 },
	{ 136,  -80 },
	{ 141,  -70 },
	{ 147,  -60 },
	{ 153,  -50 },
	{ 162,  -40 },
	{ 171,  -30 },
	{ 181,  -20 },
	{ 190,  -10 },
	{ 199,  0 },
	{ 209,  10 },
	{ 219,  20 },
	{ 230,  30 },
	{ 240,  40 },
	{ 250,  50 },
	{ 262,  60 },
	{ 275,  70 },
	{ 287,  80 },
	{ 300,  90 },
	{ 312,  100 },
	{ 326,  110 },
	{ 339,  120 },
	{ 353,  130 },
	{ 366,  140 },
	{ 380,  150 },
	{ 395,  160 },
	{ 410,  170 },
	{ 426,  180 },
	{ 441,  190 },
	{ 456,  200 },
	{ 472,  210 },
	{ 488,  220 },
	{ 505,  230 },
	{ 521,  240 },
	{ 537,  250 },
	{ 555,  260 },
	{ 573,  270 },
	{ 590,  280 },
	{ 608,  290 },
	{ 626,  300 },
	{ 647,  310 },
	{ 667,  320 },
	{ 688,  330 },
	{ 708,  340 },
	{ 729,  350 },
	{ 748,  360 },
	{ 767,  370 },
	{ 785,  380 },
	{ 804,  390 },
	{ 823,  400 },
	{ 842,  410 },
	{ 862,  420 },
	{ 881,  430 },
	{ 901,  440 },
	{ 920,  450 },
	{ 939,  460 },
	{ 958,  470 },
	{ 978,  480 },
	{ 997,  490 },
	{ 1016,  500 },
	{ 1034,  510 },
	{ 1052,  520 },
	{ 1069,  530 },
	{ 1087,  540 },
	{ 1105,  550 },
	{ 1122,  560 },
	{ 1139,  570 },
	{ 1157,  580 },
	{ 1174,  590 },
	{ 1191,  600 },
	{ 1208,  610 },
	{ 1225,  620 },
	{ 1241,  630 },
	{ 1258,  640 },
	{ 1275,  650 },
	{ 1288,  660 },
	{ 1301,  670 },
	{ 1313,  680 },
	{ 1326,  690 },
	{ 1339,  700 },
	{ 1350,  710 },
	{ 1362,  720 },
	{ 1373,  730 },
	{ 1385,  740 },
	{ 1396,  750 },
};
#endif

static struct sec_therm_platform_data sec_therm_pdata = {
	.adc_arr_size	= ARRAY_SIZE(adc_ch0_table),
	.adc_table	= adc_ch0_table,
	.polling_interval = 30 * 1000, /* msecs */
	.get_siop_level = get_pxa986_siop_level,
};

static struct platform_device sec_device_thermistor = {
	.name = "sec-thermistor",
	.id = -1,
	.dev.platform_data = &sec_therm_pdata,
};
#endif /* CONFIG_SEC_THERMISTOR */
/* clk usage desciption */
MMP_HW_DESC(fb, "pxa168-fb", 0, PM_QOS_CPUIDLE_BLOCK_DDR_VALUE, "LCDCLK");
struct mmp_hw_desc *emei_dkb_hw_desc[] __initdata = {
	&mmp_device_hw_fb,
};

#ifdef CONFIG_PROC_FS
/* GPS: power on/off control */
static int __attribute__((unused)) gps_enable_control(int flag)
{
        static struct regulator *gps_regulator = NULL;
        static int f_enabled = 0;
        printk("[GPS] LDO control : %s\n", flag ? "ON" : "OFF");

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
        return 0;
}

/* GPS: power on/off control */
static void gps_power_on(void)
{
	unsigned int gps_rst_n,gps_on;

	gps_rst_n = mfp_to_gpio(GPIO006_GPIO_6);
	if (gpio_request(gps_rst_n, "gpio_gps_rst")) {
		pr_err("Request GPIO failed, gpio: %d\n", gps_rst_n);
		return;
	}
	gps_on = mfp_to_gpio(GPIO007_GPIO_7);
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

	gps_on = mfp_to_gpio(GPIO007_GPIO_7);
	if (gpio_request(gps_on, "gpio_gps_on")) {
		pr_err("Request GPIO failed,gpio: %d\n", gps_on);
		return;
	}

	gps_rst_n = mfp_to_gpio(GPIO006_GPIO_6);
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

	gps_rst_n = mfp_to_gpio(GPIO006_GPIO_6);
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

	gps_on = mfp_to_gpio(GPIO007_GPIO_7);
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
static struct wakeup_source uart_ws;
static struct work_struct uart_wakeup_work;

static void uart_add_constraint(int mfp, void *unused)
{
	if (!mod_timer(&uart_constraint_timer, jiffies + 10 * HZ))
		pm_qos_update_request(&uart_lpm_cons,
			PM_QOS_CPUIDLE_BLOCK_AXI_VALUE);
	/* Refer to wlan_edge_wakeup() for why using work_queue here. */
	schedule_work(&uart_wakeup_work);
}

static void uart_timer_handler(unsigned long data)
{
	pm_qos_update_request(&uart_lpm_cons,
		PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
}

static void uart_wakeup(struct work_struct *work)
{
	__pm_wakeup_event(&uart_ws, 10000); /* time in milliseconds */
}

struct gpio_edge_desc uart_rx_pad = {
	.mfp = MFP_PIN_GPIO47, /* ap UART rx */
	.handler = uart_add_constraint,
};

static void uart_wakeup_init(void)
{
	INIT_WORK(&uart_wakeup_work, uart_wakeup);
	wakeup_source_init(&uart_ws, "uart");
}

#define PM800_SW_PDOWN			(1 << 5)
#define PM800_USER_DATA3		0xEA
static void emei_dkb_poweroff(void)
{
	unsigned char data;
	pr_info("turning off power....\n");

	preempt_enable();
        /* save power off reason */
	if(lpcharge && (current_cable_type > POWER_SUPPLY_TYPE_BATTERY)
			&& !lpm_poweroff)
		pm800_extern_write(PM80X_BASE_PAGE, PM800_USER_DATA3, PMIC_GENERAL_USE_BOOT_BY_INTENDED_RESET);

	else
		pm800_extern_write(PM80X_BASE_PAGE, PM800_USER_DATA3, PMIC_GENERAL_USE_BOOT_BY_NONE);

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

#ifdef CONFIG_MACH_LT02
#if defined (CONFIG_BACKLIGHT_PWM) && defined (CONFIG_BACKLIGHT_TPS61165)
/*******************************start*************************************/
static struct platform_pwm_backlight_data lt02_lcd_backlight_data = {
       /* primary backlight */
	 .name = "emei-bl",
       .pwm_id = 4,
       .max_brightness = 255,
       .dft_brightness = 100,
       .pwm_period_ns = 1000,
 };

static struct platform_device lt02_lcd_backlight_devices = {
       .name = "pwm-tps61165",
       .id = 0,
       .dev = {
               .platform_data = &lt02_lcd_backlight_data,
       },
 };

/*******************************end*************************************/
#endif
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

static unsigned long production_pin_config[] __initdata = {
	/*production mode need set I2S pin as below*/
#define GPIO021_I2S_BITCLK_PM	  GPIO021_GPIO_21 | MFP_LPM_INPUT
#define GPIO022_I2S_SYNC_PM		GPIO022_GPIO_22 | MFP_LPM_INPUT
#define GPIO023_I2S_DATA_OUT_PM 	GPIO023_GPIO_23  | MFP_LPM_INPUT
#define GPIO024_I2S_SDATA_IN_PM		GPIO024_GPIO_24  | MFP_LPM_INPUT

	GPIO021_I2S_BITCLK_PM,	/* I2S_BITCLK */
	GPIO022_I2S_SYNC_PM,	/* I2S_SYNC */
	GPIO023_I2S_DATA_OUT_PM,	/* I2S_DATA_OUT */
	GPIO024_I2S_SDATA_IN_PM,	/* I2S_DATA_IN */

//KSND
	GPIO025_GSSP_SCLK,	/* PCM_CLK */
	GPIO026_GSSP_SFRM,	/* PCM_SYNC */
	GPIO027_GSSP_TXD, /* PCM_TXD */
	GPIO028_GSSP_RXD, /* PCM_RXD */
};

static unsigned long dvc_pin_config[] __initdata = {
	#define GPIO043_GPIO_DVC1_Ax	(GPIO043_GPIO_43_Ax | MFP_PULL_FLOAT)
	#define GPIO044_GPIO_DVC2_Ax	(GPIO044_GPIO_44_Ax | MFP_PULL_FLOAT)

	GPIO043_GPIO_DVC1_Ax,
	GPIO044_GPIO_DVC2_Ax,
};

static void __init emeidkb_init(void)
{
	int i;

	/*sec_common_init();*/

	for (i = 0; i < ARRAY_SIZE(emei_dkb_hw_desc); i++)
		mmp_device_hw_register(emei_dkb_hw_desc[i]);

	mmp_pins_init();

	if(production_mode_flag)
		mfp_config(ARRAY_AND_SIZE(production_pin_config));

	if (dvc_flag)
		mfp_config(ARRAY_AND_SIZE(dvc_pin_config));

	pm_power_off = emei_dkb_poweroff;
	register_reboot_notifier(&reboot_notifier);

	/* Uart1, AP kernel console and debug */
	pxa988_add_uart(1);
	/* Uart2, GPS */
	pxa988_add_uart(2);

	if (system_rev <= LT02_R0_0) {
		pm800_info.num_regulators = pm800_regulators_num;
		pm800_info.regulator = pm800_regulator_data_lt02_r0_0;
	} else {
		pm800_info.num_regulators = pm800_regulators_num;
		pm800_info.regulator = pm800_regulator_data_lt02_r0_1;
	}

#ifdef CONFIG_KEYBOARD_PXA27x
	pxa988_add_keypad(&emei_dkb_keypad_info);
#endif

#ifdef CONFIG_KEYBOARD_GPIO
	u32 kpc;

	/*disable keypad function*/
	kpc = __raw_readl(APB_PHYS_BASE+0x12000);

	__raw_writel(0, APB_PHYS_BASE+0x12000);
	kpc = __raw_readl(APB_PHYS_BASE+0x12000);

	/*close keypad clock*/
	__raw_writel(0, APBC_PXA988_KPC);

	kpc = __raw_readl(APBC_PXA988_KPC);

	/*clear keypad wakeup source*/
	kpc = __raw_readl(APMU_WAKE_CLR);
	__raw_writel(kpc | (1<<3), APMU_WAKE_CLR);

	platform_device_register(&lt02_gpio_keys_device);
#endif

	emeidkb_init_mmc();

#ifdef CONFIG_ANDROID_RAM_CONSOLE
	platform_device_register(&pxa988_device_ramconsole);
#endif

#ifdef CONFIG_ANDROID_TIMED_GPIO
	/* Vibrator */
	platform_device_register(&lt02_timed_gpios_device);
#endif
#ifdef CONFIG_VIBRATOR_ANDROID
	platform_device_register(&lt02_vibrator_android_device);
#endif
	/* soc-rtc */
	platform_device_register(&pxa988_device_rtc);
	/* backlight */
	//platform_device_register(&emei_dkb_lcd_backlight_devices);//zswan add it
#if defined (CONFIG_BACKLIGHT_PWM) && defined (CONFIG_BACKLIGHT_TPS61165)	
	pxa988_add_pwm(4);
       platform_device_register(&lt02_lcd_backlight_devices);
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

	pxa988_add_twsi(0, &emeidkb_ci2c_pdata,
			ARRAY_AND_SIZE(emeidkb_i2c_info));

	if (system_rev < LT02_R0_3) {
		pxa988_add_twsi(1, &emeidkb_ci2c2_pdata,
						ARRAY_AND_SIZE(lt02_i2c2_info));
	} else {
		pxa988_add_twsi(1, &emeidkb_ci2c2_pdata,
						ARRAY_AND_SIZE(lt02_i2c2_r03_info));
	}

	pxa988_add_twsi(2, &emeidkb_pwr_i2c_pdata,
			ARRAY_AND_SIZE(emeidkb_pwr_i2c_info));

#if defined (CONFIG_TOUCHSCREEN_BT532_TS)
	if (system_rev < LT02_R0_3) {
		platform_device_register(&i2c_bt532_bus_device);
		i2c_register_board_info(4, ARRAY_AND_SIZE(bt532_i2c_devices));
	}

	bt532_ts_init();
#endif
#if 0
	platform_device_register(&i2c_stc3115_bus_device);
	i2c_register_board_info(6, ARRAY_AND_SIZE(stc3115_i2c_devices));
#endif

#if defined(CONFIG_FSA9480_MICROUSB)
	platform_device_register(&i2c_fsa9480_bus_device);
	i2c_register_board_info(7, ARRAY_AND_SIZE(fsa9480_i2c_devices));
#endif

#ifdef CONFIG_NFC_PN547
	pn547_gpio_init();
	platform_device_register(&i2c_nfc_device);
	i = i2c_register_board_info(9, ARRAY_AND_SIZE(pn547_i2c));
	if (i < 0) {
		pr_err("%s, pn547_i2c adding i2c fail(err=%d)\n", __func__, i);
	}
#endif

#if defined(CONFIG_BATTERY_SAMSUNG)
	pxa986_lt02_charger_init();
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
	platform_device_register(&emei_dkb_audio_device);

	/* off-chip devices */
	//platform_add_devices(ARRAY_AND_SIZE(dkb_platform_devices));
#ifdef CONFIG_FB_PXA168
//	extern void emeidkb_add_lcd_mipi(void);
//	emeidkb_add_lcd_mipi();
	lt02_add_lcd_mipi();
#ifdef CONFIG_PXA988_DISP_HACK
	if (cpu_pxa98x_stepping() < PXA98X_Z3)
		emeidkb_add_tv_out();
#endif
#endif

#ifdef CONFIG_UIO_CODA7542
	pxa_register_coda7542();
#endif

#ifdef CONFIG_USB_MV_UDC
	pxa988_device_udc.dev.platform_data = &emeidkb_usb_pdata;
	platform_device_register(&pxa988_device_udc);
#endif
#ifdef CONFIG_VIDEO_MMP
	/* For following apse codebase, I did not want to modify too many board-aruba files
	 *  so I try to add a little code to apply to apse's code, such as mv_cam_pdata init.
	 *  By Vincent Wan.
	*/
	mv_cam_data_forssg.init_clk = pxa988_cam_clk_init,
	mv_cam_data_forssg.enable_clk = pxa988_cam_set_clk,
	init_samsung_cam();
#endif
#if 0
#ifdef CONFIG_VIDEO_MVISP
	pxa988_init_dxoisp();
#endif
#endif
#if defined(CONFIG_SPA)
	platform_device_register(&Sec_BattMonitor);
#endif

#if defined(CONFIG_SENSORS_GP2A0X0) || defined(CONFIG_SENSORS_BMC150)
	platform_device_register(&i2c_gpio_device);
#if defined(CONFIG_SENSORS_BMC150)

	if (system_rev >= LT02_R0_0) {
		((struct bosch_sensor_specific*)(i2c_bma2x2.platform_data))->place = 6;
		((struct bosch_sensor_specific*)(i2c_bmm050.platform_data))->place = 6;
	}

	if (system_rev >= LT02_R0_3) {
		i2c_bma2x2.addr = 0x11;
		i2c_bmm050.addr = 0x13;
	}

	{
		#define ACC_INT mfp_to_gpio(MFP_PIN_GPIO10)

		int acc_int;
		acc_int = ACC_INT;
		if(gpio_request(acc_int,"ACC INT")){
			printk(KERN_ERR"%s:failed to request gpio(%d)\n", __func__,acc_int);
			return -EINVAL;
		}
		gpio_direction_input(acc_int);
		gpio_free(acc_int);
	}

	i2c_register_board_info(5, &i2c_bma2x2, 1);
	i2c_register_board_info(5, &i2c_bmm050, 1);
#endif
#if defined(CONFIG_SENSORS_GP2A0X0)
	gp2a_init(system_rev);
	i2c_register_board_info(5, &i2c_gp2a, 1);
#endif
#endif //CONFIG_SENSORS_GP2A0X0 || CONFIG_SENSORS_BMC150
#ifdef CONFIG_SENSORS_ASP01
	grip_gpio_init();
	grip_init_code_set();
	platform_device_register(&i2c_grip_device);
	i = i2c_register_board_info(10, &i2c_devs10, 1);;
	if (i < 0) {
		pr_err("%s, i2c10 adding i2c fail(err=%d)\n", __func__, i);
		return i;
	}
#endif //CONFIG_SENSORS_asp01

#ifdef CONFIG_SEC_IRLED
	platform_device_register(&lt02_i2c_bus_11_device);
	pxa986_lt02_irled_init();	
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
#ifdef CONFIG_SEC_THERMISTOR
	platform_device_register(&sec_device_thermistor);
#endif
	pxa988_add_thermal();

#ifdef CONFIG_PROC_FS
	/* create proc for sirf GPS control */
	create_sirf_proc_file();
#endif
	/* add uart pad wakeup */
	uart_wakeup_init();
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

MACHINE_START(LT02, "PXA988")
	.map_io		= mmp_map_io,
	.init_early	= pxa988_init_early,
	.init_irq	= pxa988_init_irq,
	.timer		= &pxa988_timer,
	.reserve	= pxa988_reserve,
	.handle_irq	= gic_handle_irq,
	.init_machine	= emeidkb_init,
	.restart	= mmp_arch_reset,
MACHINE_END
