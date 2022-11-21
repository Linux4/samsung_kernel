/*
 *  Copyright (C) 2007-2012 Marvell International Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publishhed by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/i2c/pxa-i2c.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/gpio.h>
#if defined (CONFIG_MFD_88PM800) || defined (CONFIG_MFD_88PM805)
#include <linux/mfd/88pm80x.h>
#endif

#ifdef CONFIG_RTC_DRV_PXA
#include <linux/rtc-pxa.h>
#endif

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/pxa3xx-regs.h>
#include <mach/pxa95x.h>

#include "devices.h"
#include "generic.h"

#ifdef CONFIG_MFD_88PM800

#define PM8XXX_REGULATOR_MAX PM800_ID_RG_MAX

static struct regulator_consumer_supply regulator_supply[PM8XXX_REGULATOR_MAX];
static struct regulator_init_data regulator_data[PM8XXX_REGULATOR_MAX];

static int PM800_ID_regulator_index[] = {
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


#define REG_SUPPLY_INIT(_id, _name, _dev_name)		\
{							\
	int _i = _id;				\
	regulator_supply[_i].supply		= _name;		\
	regulator_supply[_i].dev_name	= _dev_name;	\
}

/* notes: apply_uV which means proper voltage value (latest set value or min)
* would be applied first time when enabled. So it would be set 1 if min voltage
* == max voltage*/
#define REG_INIT(_id, _chip, _name, _min, _max, _always, _boot)	\
{									\
	int _i = _id;				\
	regulator_data[_i].constraints.name	=	__stringify(_name);	\
	regulator_data[_i].constraints.min_uV	= _min;	\
	regulator_data[_i].constraints.max_uV	= _max;	\
	regulator_data[_i].constraints.always_on	= _always;	\
	regulator_data[_i].constraints.boot_on	= _boot;	\
	regulator_data[_i].constraints.valid_ops_mask	=	\
		REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS;	\
	regulator_data[_i].num_consumer_supplies	= 1;	\
	regulator_data[_i].consumer_supplies	=	\
		&regulator_supply[_chip##_##_name];	\
	regulator_data[_i].driver_data	=	\
		&_chip##_regulator_index[_chip##_##_name];	\
	regulator_data[_i].constraints.apply_uV = (_min == _max);	\
}

static void mic_set_power(int on)
{
	struct regulator *v_ldo = regulator_get(NULL, "mic_bias");
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

struct pm80x_headset_pdata pm80x_headset = {
	.headset_flag		= 0,
	.mic_set_power		= mic_set_power,
};

struct pm80x_rtc_pdata pm80x_rtc = {
	.vrtc           = 1,
	.rtc_wakeup     = 0,
#ifdef CONFIG_RTC_DRV_PXA
	.sync           = pxa_rtc_sync_time,
#endif
};
static int pm800_plat_config(struct pm80x_chip *chip,
				struct pm80x_platform_data *pdata)
{
	if (!chip || !pdata || chip->id != CHIP_PM800 || !chip->regmap
			|| !chip->subchip || !chip->subchip->regmap_gpadc) {
		pr_err("%s:chip or pdata is not availiable!\n", __func__);
		return -EINVAL;
	}
	/* Initializain actions to enable 88pm805 */
	/* Enable 32Khz-out-1 and resetoutn */
	regmap_write(chip->regmap, 0xE1, 0xB0);
	/* Set internal digital sleep voltage to 0.9V */
	regmap_write(chip->regmap, 0x20, 0xf0);
	/* Enable 32Khz-out-3  low jitter and DVC for internal digital circuitry */
	regmap_write(chip->regmap, 0x21, 0x60);
	/* Enable LDO and BUCK clock gating in low power mode */
	regmap_write(chip->regmap, 0x22, 0x80);
	/* Enable reference group sleep mode */
	regmap_write(chip->regmap, 0x23, 0x80);
	/* Enable 32Khz-out-3 */
	regmap_write(chip->regmap, 0xE2, 0x22);
	/* Set XO CAP to 22pF to avoid speaker noise */
	regmap_write(chip->regmap, 0xE8, 0x70);

	/* Enable GPADC sleep mode */
	regmap_write(chip->subchip->regmap_gpadc, 0x06, 0x71);
	/* Enlarge GPADC off slots */
	regmap_write(chip->subchip->regmap_gpadc, 0x08, 0x0f);

	return 0;
}

static struct pm80x_platform_data pm800_info = {
	.headset		= &pm80x_headset,
	.regulator		= regulator_data,
	.rtc			= &pm80x_rtc,
	.power_page_addr	= 0x31,
	.gpadc_page_addr	= 0x32,
	.irq_mode		= 0,
	.plat_config		= pm800_plat_config,
};

static void regulator_init_pm800(void)
{
	int i = 0;
	REG_SUPPLY_INIT(PM800_ID_LDO16, "v_cam", NULL);
	REG_SUPPLY_INIT(PM800_ID_LDO6, "v_ihdmi", NULL);
	REG_SUPPLY_INIT(PM800_ID_LDO18, "Vdd_IO", NULL);
	REG_SUPPLY_INIT(PM800_ID_LDO2, "mic_bias", NULL);
	REG_SUPPLY_INIT(PM800_ID_LDO15, "Vdd_CMMB18", NULL);
	REG_SUPPLY_INIT(PM800_ID_LDO17, "v_cam_af_vcc", NULL);
	REG_SUPPLY_INIT(PM800_ID_LDO9, "v_wifi_3v3", NULL);
	REG_SUPPLY_INIT(PM800_ID_LDO10, "v_vibrator", NULL);
	REG_SUPPLY_INIT(PM800_ID_LDO12, "vmmc_io", NULL);
	REG_SUPPLY_INIT(PM800_ID_LDO13, "vmmc", "sdhci-pxa.1");
	REG_SUPPLY_INIT(PM800_ID_LDO19, "v_gps", NULL);
	REG_SUPPLY_INIT(PM800_ID_LDO11, "v_ls043", NULL);
	REG_SUPPLY_INIT(PM800_ID_LDO8, "v_lcd_cywee_touch", NULL);

	REG_INIT(i++, PM800_ID, LDO16, 1800000, 3300000, 0, 0);
	REG_INIT(i++, PM800_ID, LDO6, 1200000, 3300000, 0, 0);
	REG_INIT(i++, PM800_ID, LDO18, 1800000, 3300000, 0, 0);
	REG_INIT(i++, PM800_ID, LDO2, 1200000, 3300000, 0, 0);
	REG_INIT(i++, PM800_ID, LDO15, 1800000, 3300000, 0, 0);
	REG_INIT(i++, PM800_ID, LDO17, 1200000, 3300000, 0, 0);
	REG_INIT(i++, PM800_ID, LDO9, 3300000, 3300000, 0, 0);
	REG_INIT(i++, PM800_ID, LDO10, 2800000, 2800000, 0, 0);
	REG_INIT(i++, PM800_ID, LDO12, 1800000, 2800000, 0, 0);
	REG_INIT(i++, PM800_ID, LDO13, 2800000, 2800000, 0, 0);
	REG_INIT(i++, PM800_ID, LDO19, 1200000, 3300000, 0, 0);
	REG_INIT(i++, PM800_ID, LDO11, 3100000, 3100000, 1, 1);
	REG_INIT(i++, PM800_ID, LDO8, 1800000, 3300000, 1, 1);
	pr_info("%s: select NEVO dkb pm800 ldo map\n", __func__);

	pm800_info.num_regulators = i;
}

#endif

#ifdef CONFIG_MFD_88PM805
static struct pm80x_platform_data pm805_info = {
	.irq_mode		= 0,
};
#endif

static struct i2c_board_info i2c0_info[] = {
#ifdef CONFIG_MFD_88PM800
	{
		I2C_BOARD_INFO("88PM800", 0x30),
		.platform_data	= &pm800_info,
		.irq		= IRQ_PMIC_INT,
	},
#endif
#ifdef CONFIG_MFD_88PM805
	{
		I2C_BOARD_INFO("88PM805", 0x38),
		.platform_data	= &pm805_info,
		.irq		= PXA_GPIO_TO_IRQ(MFP_PIN_GPIO96),
	},
#endif
};

static void register_i2c_board_info(void)
{
	i2c_register_board_info(0, ARRAY_AND_SIZE(i2c0_info));
}

static struct i2c_pxa_platform_data i2c0_pdata = {
	.use_pio        = 0,
};

static struct i2c_pxa_platform_data i2c1_pdata = {
	.use_pio	= 0,
};

static struct i2c_pxa_platform_data i2c2_pdata = {
	.use_pio	= 0,
};

static struct platform_device *i2c_devices[] __initdata = {
	&pxa95x_device_i2c0,
	&pxa95x_device_i2c1,
	&pxa95x_device_i2c2,
};


static void __init init(void)
{
	regulator_init_pm800();
	platform_device_add_data(&pxa95x_device_i2c0, &i2c0_pdata,
			sizeof(i2c0_pdata));
	platform_device_add_data(&pxa95x_device_i2c1, &i2c1_pdata,
			sizeof(i2c1_pdata));
	platform_device_add_data(&pxa95x_device_i2c2, &i2c2_pdata,
			sizeof(i2c2_pdata));
	platform_add_devices(ARRAY_AND_SIZE(i2c_devices));

	register_i2c_board_info();
}

MACHINE_START(NEVODKB, "PXA978")
	.map_io		= pxa_map_io,
	.nr_irqs	= IRQ_BOARD_START + 40,
	.init_irq	= pxa95x_init_irq,
	.handle_irq	= pxa95x_handle_irq_intc,
	.timer		= &pxa_timer,
	.reserve	= pxa95x_mem_reserve,
	.init_machine	= init,
MACHINE_END
