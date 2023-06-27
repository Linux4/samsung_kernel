/*
 * Marvell 88PM822 Interface
 *
 * Copyright (C) 2013 Marvell International Ltd.
 * Yipeng <ypyao@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_MFD_88PM822_H
#define __LINUX_MFD_88PM822_H

#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/atomic.h>
#include <linux/regulator/machine.h>
#include <linux/proc_fs.h>
#include <linux/battery/sec_fuelgauge.h>

enum {
	PM822_INVALID_PAGE = 0,
	PM822_BASE_PAGE,
	PM822_POWER_PAGE,
	PM822_GPADC_PAGE,
};

enum {
	PM822_ID_BUCK1 = 0,
	PM822_ID_BUCK2,
	PM822_ID_BUCK3,
	PM822_ID_BUCK4,
	PM822_ID_BUCK5,

	PM822_ID_LDO1,
	PM822_ID_LDO2,
	PM822_ID_LDO3,
	PM822_ID_LDO4,
	PM822_ID_LDO5,
	PM822_ID_LDO6,
	PM822_ID_LDO7,
	PM822_ID_LDO8,
	PM822_ID_LDO9,
	PM822_ID_LDO10,
	PM822_ID_LDO11,
	PM822_ID_LDO12,
	PM822_ID_LDO13,
	PM822_ID_LDO14,
	PM822_ID_VOUTSW,

#if defined(CONFIG_CPU_PXA1L88) || defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
	/* below 4 ids are fake id, they are only used in new dvc */
	PM822_ID_BUCK1_AP_ACTIVE,
	PM822_ID_BUCK1_AP_LPM,
	PM822_ID_BUCK1_APSUB_IDLE,
	PM822_ID_BUCK1_APSUB_SLEEP,
#endif

	PM822_ID_RG_MAX,
};

enum {
	PM822_NO_GPIO = -1,
	PM822_GPIO0 = 0,
	PM822_GPIO1,
	PM822_GPIO2,
	PM822_GPIO3,
};

enum {
	PM822_NO_GPADC = -1,
	PM822_GPADC0 = 0,
	PM822_GPADC1,
	PM822_GPADC2,
	PM822_GPADC3,
};

#define PM822_BASE_REG_NUM		0xF0
#define PM822_POWER_REG_NUM		0xD9
#define PM822_GPADC_REG_NUM		0xC8

/*
 * Page 0: basic
 */
#define PM822_CHIP_ID			(0x00)
#define PM822_STATUS1			(0x01)
#define PM822_ONKEY_STS1		(1 << 0)
#define PM822_CHG_STS1			(1 << 2)
#define PM822_BAT_STS1			(1 << 3)
#define PM822_LDO_PGOOD_STS1	(1 << 5)
#define PM822_BUCK_PGOOD_STS1	(1 << 6)

/* Interrupt Registers */
#define PM822_INT_STATUS1		(0x05)
#define PM822_INT_STATUS2		(0x06)
#define PM822_INT_STATUS3		(0x07)
#define PM822_INT_EN1			(0x09)
	/* PM822_INT_EN1 bit definition */
#define PM822_IRQ_ONKEY_EN		(1 << 0)
#define PM822_IRQ_CHG_EN		(1 << 2)
#define PM822_IRQ_BAT_EN		(1 << 3)
#define PM822_IRQ_RTC_EN		(1 << 4)
#define PM822_IRQ_CLASSD_EN		(1 << 5)
	/* end */
#define PM822_INT_EN2			(0x0A)
	/* PM822_INT_EN2 bit definition */
#define PM822_IRQ_VBAT_EN		(1 << 0)
#define PM822_IRQ_VSYS_EN		(1 << 1)
#define PM822_IRQ_TINT_EN		(1 << 3)
#define PM822_IRQ_LDO_PGOOD_EN		(1 << 4)
#define PM822_IRQ_BUCK_PGOOD_EN		(1 << 5)
	/* end */
#define PM822_INT_EN3			(0x0B)
	/* PM822_INT_EN3 bit definition */
#define PM822_IRQ_GPADC0_EN		(1 << 0)
#define PM822_IRQ_GPADC1_EN		(1 << 1)
#define PM822_IRQ_GPADC2_EN		(1 << 2)
#define PM822_IRQ_GPADC3_EN		(1 << 3)
#define PM822_IRQ_MIC_DET_EN		(1 << 4)
#define PM822_IRQ_HS_DET_EN		(1 << 5)
	/* end */

/* Wakeup Registers */
#define PM822_WAKEUP1			(0x0D)
#define PM822_WAKEUP1_WD_MODE	(1 << 0)
#define PM822_WAKEUP2			(0x0E)
	/* PM822_WAKEUP2 bit definition */
#define PM822_INV_INT			(1 << 0)
#define PM822_INT_CLEAR_MODE		(1 << 1)
#define PM822_INT_MASK_MODE		(1 << 2)
#define PM822_WD_TIMER_ACT_MASK  (7 << 3)
#define PM822_WD_TIMER_ACT_1S		(0 << 3)
#define PM822_WD_TIMER_ACT_2S		(1 << 3)
#define PM822_WD_TIMER_ACT_4S		(2 << 3)
#define PM822_WD_TIMER_ACT_8S		(3 << 3)
#define PM822_WD_TIMER_ACT_16S		(4 << 3)
#define PM822_WD_TIMER_ACT_32S		(5 << 3)
#define PM822_WD_TIMER_ACT_64S		(6 << 3)
#define PM822_WD_TIMER_ACT_256S		(7 << 3)

	/* end */
#define PM822_POWER_UP_LOG		(0x10)

#define PM822_WATCHDOG_REG		(0x1D)
#define PM822_WD_DIS			(1 << 0)
#define PM822_WD_EN				(0)

/* Referance and low power registers */
#define PM822_LOW_POWER1		(0x20)
#define PM822_LOW_POWER2		(0x21)
#define PM822_LOW_POWER_CONFIG3		(0x22)
#define PM822_LOW_POWER_CONFIG4		(0x23)
#define PM822_LOW_POWER_CONFIG5		(0x24)

/* GPIO register */
#define PM822_GPIO0_CTRL		(0x30)
#define PM822_GPIO0_VAL			(1 << 0)
#define PM822_GPIO0_GPIO_MODE(x)	(x << 1)
#define PM822_GPIO1_CTRL		(0x31)
#define PM822_GPIO1_VAL			(1 << 0)
#define PM822_GPIO1_GPIO_MODE(x)	(x << 1)
#define PM822_GPIO2_CTRL		(0x32)
#define PM822_GPIO2_VAL			(1 << 0)
#define PM822_GPIO2_GPIO_MODE(x)	(x << 1)
#define PM822_GPIO3_CTRL		(0x33)
#define PM822_GPIO3_VAL			(1 << 0)
#define PM822_GPIO3_GPIO_MODE(x)	(x << 1)

#define PM822_HEADSET_CNTRL		(0x38)
#define PM822_HEADSET_DET_EN		(1 << 7)
#define PM822_HSDET_PERIOD_128MS  	(0)
#define PM822_HSDET_PERIOD_256MS  	(1 << 2)
#define PM822_HSDET_PERIOD_512MS  	(2 << 2)
#define PM822_HSDET_PERIOD_CONT  	(3 << 2)
#define PM822_HSDET_SLP				(1 << 1)
#define PM822_HSDET_COUNT       	(1 << 0) 

#define PM822_MIC_CNTRL			(0x39)
#define PM822_HEADSET_DET_MASK		(1 << 7)
#define PM822_MIC_DET_EN			(1 << 0)

/* PWM register */
#define PM822_PWM1			(0x40)
#define PM822_PWM2			(0x41)
#define PM822_PWM3			(0x42)
#define PM822_PWM4			(0x43)

/* Oscillator control */
#define PM822_OSC_CTRL1			(0x50)
#define PM822_OSC_CTRL2			(0x51)
#define PM822_OSC_CTRL3			(0x52)
#define PM822_OSC_CTRL4			(0x53)
#define PM822_OSC_CTRL5			(0x54)
#define PM822_OSC_CTRL6			(0x55)
#define PM822_OSC_CTRL7			(0x56)
#define PM822_OSC_CTRL8			(0x57)
#define PM822_OSC_CTRL9			(0x58)
#define PM822_OSC_CTRL11		(0x5A)
#define PM822_OSC_CTRL12		(0x5B)
#define PM822_OSC_CTRL13		(0x5C)

/* RTC Registers */
#define PM822_RTC_CTRL			(0xD0)
/* bit definitions of RTC Register 1 (0xD0) */
#define PM822_ALARM1_EN			(1 << 0)
#define PM822_ALARM_WAKEUP		(1 << 4)
#define PM822_ALARM			(1 << 5)
#define PM822_RTC1_USE_XO		(1 << 7)

	/* end */
#define PM822_RTC_COUNTER1		(0xD1)
#define PM822_RTC_COUNTER2		(0xD2)
#define PM822_RTC_COUNTER3		(0xD3)
#define PM822_RTC_COUNTER4		(0xD4)
#define PM822_RTC_EXPIRE1_1		(0xD5)
#define PM822_RTC_EXPIRE1_2		(0xD6)
#define PM822_RTC_EXPIRE1_3		(0xD7)
#define PM822_RTC_EXPIRE1_4		(0xD8)
#define PM822_RTC_TRIM1			(0xD9)
#define PM822_RTC_TRIM2			(0xDA)
#define PM822_RTC_TRIM3			(0xDB)
#define PM822_RTC_TRIM4			(0xDC)
#define PM822_RTC_EXPIRE2_1		(0xDD)
#define PM822_RTC_EXPIRE2_2		(0xDE)
#define PM822_RTC_EXPIRE2_3		(0xDF)
#define PM822_RTC_EXPIRE2_4		(0xE0)
#define PM822_RTC_MISC1			(0xE1)
#define PM822_RTC_MISC2			(0xE2)
#define PM822_RTC_MISC3			(0xE3)
#define PM822_RTC_MISC4			(0xE4)
#define PM822_RTC_MISC5			(0xE7)
#define PM822_RTC_MISC6			(0xE8)
#define PM822_RTC_MISC7			(0xE9)
#define PM822_USER_DATA1		(0xEA)
#define PM822_USER_DATA2		(0xEB)
#define PM822_USER_DATA3		(0xEC)
#define PM822_USER_DATA4		(0xED)
#define PM822_USER_DATA5		(0xEE)
#define PM822_USER_DATA6		(0xEF)
#define PM822_POWER_DOWN_LOG1		(0xE5)
#define PM822_POWER_DOWN_LOG2		(0xE6)

/*
 * Page 1: power
 */
#define PM822_NOUSE			(0xFF)
#define PM822_LDO1			(0x08)
#define PM822_LDO2_AUDIO		(0x0A)
#define PM822_LDO2			(0x0B)
#define PM822_LDO3			(0x0C)
#define PM822_LDO4			(0x0D)
#define PM822_LDO5			(0x0E)
#define PM822_LDO6			(0x0F)
#define PM822_LDO7			(0x10)
#define PM822_LDO8			(0x11)
#define PM822_LDO9			(0x12)
#define PM822_LDO10			(0x13)
#define PM822_LDO11			(0x14)
#define PM822_LDO12			(0x15)
#define PM822_LDO13			(0x16)
#define PM822_LDO14			(0x17)
#define PM822_VOUTSW			(0xFF)	/* fake register */

#define PM822_BUCK1_SLP			(0x30)
#define PM822_BUCK2_SLP			(0x31)
#define PM822_BUCK3_SLP			(0x32)
#define PM822_BUCK4_SLP			(0x33)
#define PM822_BUCK5_SLP			(0x34)
#define PM822_AUDIO_MODE		(0x38)

#define PM822_BUCK1			(0x3C)
#define PM822_BUCK2			(0x40)
#define PM822_BUCK3			(0x41)
#define PM822_BUCK3_1			(0x42)
#define PM822_BUCK3_2			(0x43)
#define PM822_BUCK3_3			(0x44)
#define PM822_BUCK4			(0x45)
#define PM822_BUCK5			(0x46)

#define PM822_BUCK_EN1			(0x50)
#define PM822_LDO1_8_EN1		(0x51)
#define PM822_LDO9_14_EN1		(0x52)
#define PM822_MISC_EN1			(0x54)
#define PM822_BUCK_EN2			(0X55)
#define PM822_LDO1_8_EN2		(0x56)
#define PM822_LDO9_14_EN2		(0x57)
#define PM822_MISC_EN3			(0x59)

#define PM822_BUCK_SLP1			(0x5A)

/* BUCK Sleep Mode Register 1: BUCK[1..4] */
#define PM822_BUCK_SLP_PWR_OFF	0x0	/* power off */
#define PM822_BUCK_SLP_PWR_ACT1	0x1	/* auto & VBUCK_SET_SLP */
#define PM822_BUCK_SLP_PWR_LOW	0x2	/* VBUCK_SET_SLP */
#define PM822_BUCK_SLP_PWR_ACT2	0x3	/* auto & VBUCK_SET */

#define PM822_BUCK1_SLP1_SHIFT		0
#define PM822_BUCK1_SLP1_MASK		(0x3 << PM822_BUCK1_SLP1_SHIFT)
#define PM822_BUCK2_SLP1_SHIFT		2
#define PM822_BUCK2_SLP1_MASK		(0x3 << PM822_BUCK2_SLP1_SHIFT)
#define PM822_BUCK2_SLP1_UNMASK	(0x2 << PM822_BUCK2_SLP1_SHIFT)

#define PM822_BUCK_SLP2			(0x5B)
#define PM822_LDO_SLP1			(0x5C)
#define PM822_LDO_SLP2			(0x5D)
#define PM822_LDO_SLP3			(0x5E)
#define PM822_LDO_SLP4			(0x5F)

#define PM822_BUCK4_MISC2		(0x82)
#define PM822_BUCK_SHARED_CTRL		(0x8F)

#define PM822_LDO_MISC1			(0x90)
#define PM822_LDO_MISC2			(0x92)
#define PM822_LDO_MISC8			(0x98)
/*
 * Page 2: GPADC
 */
#define PM822_GPADC_MEAS_EN1		(0x01)
#define PM822_GPADC_MEAS_EN2		(0x02)
#define PM822_GPADC0_MEANS_EN		(1 << 2)
#define PM822_GPADC1_MEANS_EN		(1 << 3)
#define PM822_GPADC2_MEANS_EN		(1 << 4)
#define PM822_GPADC3_MEANS_EN		(1 << 5)
#define PM822_GPADC_MISC_CONFIG1	(0x05)
#define PM822_GPADC4_DIR		(1 << 6)
#define PM822_GPADC_MISC_CONFIG2	(0x06)
	/* PM822_GPADC_MISC_CONFIG2 bit definition */
#define PM822_GPADC_EN			(1 << 0)
#define PM822_GPADC_NON_STOP		(1 << 1)
#define PM822_MEANS_EN_SLP		(1 << 4)
#define PM822_MEANS_GP_SLP_MODE		(3 << 5)
	/* end */

#define PM822_GPADC_MEAS_OFF1		(0x07)
#define PM822_GPADC_MEAS_OFF2		(0x08)
#define PM822_GPADC_MISC_CONFIG3	(0x0A)
#define PM822_GPADC_BIAS1		(0x0B)
#define PM822_GPADC_BIAS2		(0x0C)
#define PM822_GPADC_BIAS3		(0x0D)
#define PM822_GPADC_BIAS4		(0x0E)
#define PM822_GPADC_BIAS_EN1		(0x14)
#define PM822_GPADC_GP_BIAS_EN0		(1 << 0)
#define PM822_GPADC_GP_BIAS_EN1		(1 << 1)
#define PM822_GPADC_GP_BIAS_EN2		(1 << 2)
#define PM822_GPADC_GP_BIAS_EN3		(1 << 3)
#define PM822_GPADC_GP_BIAS_OUT0	(1 << 4)
#define PM822_GPADC_GP_BIAS_OUT1	(1 << 5)
#define PM822_GPADC_GP_BIAS_OUT2	(1 << 6)
#define PM822_GPADC_GP_BIAS_OUT3	(1 << 7)
#define PM822_VBAT_LOW_TH		(0x18)
#define PM822_VSYS_LOW_TH		(0x19)
#define PM822_TINT_LOW_TH		(0x1B)
#define PM822_GPADC0_LOW_TH		(0x20)
#define PM822_GPADC1_LOW_TH		(0x21)
#define PM822_GPADC2_LOW_TH		(0x22)
#define PM822_GPADC3_LOW_TH		(0x23)
#define PM822_MIC_DET_LOW_TH		(0x24)

#define PM822_VBAT_UPP_TH		(0x28)
#define PM822_VSYS_UPP_TH		(0x29)
#define PM822_VCHG_UPP_TH		(0x2A)
#define PM822_TINT_UPP_TH		(0x2B)
#define PM822_GPADC0_UPP_TH		(0x30)
#define PM822_GPADC1_UPP_TH		(0x31)
#define PM822_GPADC2_UPP_TH		(0x32)
#define PM822_GPADC3_UPP_TH		(0x33)
#define PM822_MIC_DET_UPP_TH		(0x34)

#define PM822_VBBAT_MEAS1		(0x40)
#define PM822_VBBAT_MEAS2		(0x41)
#define PM822_VBAT_MEAS1		(0x42)
#define PM822_VBAT_MEAS2		(0x43)
#define PM822_VSYS_MEAS1		(0x44)
#define PM822_VSYS_MEAS2		(0x45)
#define PM822_TINT_MEAS1		(0x50)
#define PM822_TINT_MEAS2		(0x51)

#define PM822_GPADC0_MEAS1		(0x54)
#define PM822_GPADC0_MEAS2		(0x55)
#define PM822_GPADC1_MEAS1		(0x56)
#define PM822_GPADC1_MEAS2		(0x57)
#define PM822_GPADC2_MEAS1		(0x58)
#define PM822_GPADC2_MEAS2		(0x59)
#define PM822_GPADC3_MEAS1		(0x5A)
#define PM822_GPADC3_MEAS2		(0x5B)
#define PM822_MIC_DET_MEAS1		(0x5C)
#define PM822_MIC_DET_MEAS2		(0x5D)

#define PM822_VBAT_AVG                  (0xA0)
#define PM822_VBAT_AVG2                 (0xA1)
#define PM822_VSYS_AVG                  (0xA2)
#define PM822_VSYS_AVG2                 (0xA3)
#define PM822_GPADC3_AVG1		(0xA6)
#define PM822_GPADC3_AVG2		(0xA7)
#define PM822_MIC_DET_AVG1		(0xA8)
#define PM822_MIC_DET_AVG2		(0xA9)
#define PM822_GPADC4_AVG2		(0xA9)
#define PM822_VBAT_SLP			(0xB0)

#define PMIC_GENERAL_DOWNLOAD_MODE_NONE		0
#define PMIC_GENERAL_DOWNLOAD_MODE_SUD1		1
#define PMIC_GENERAL_DOWNLOAD_MODE_SUD2		2
#define PMIC_GENERAL_DOWNLOAD_MODE_SUD3		3
#define PMIC_GENERAL_DOWNLOAD_MODE_SUD4		4
#define PMIC_GENERAL_DOWNLOAD_MODE_SUD5		5
#define PMIC_GENERAL_DOWNLOAD_MODE_SUD6		6
#define PMIC_GENERAL_DOWNLOAD_MODE_SUD7		7
#define PMIC_GENERAL_DOWNLOAD_MODE_SUD8		8
#define PMIC_GENERAL_DOWNLOAD_MODE_SUD9		9
#define PMIC_GENERAL_DOWNLOAD_MODE_FUS		10

#define PMIC_GENERAL_USE_REGISTER			PM822_USER_DATA6
#define PMIC_GENERAL_USE_REBOOT_DN_MASK			(0x1F)
#define PMIC_GENERAL_USE_BOOT_BY_NONE			(0)
#define PMIC_GENERAL_USE_BOOT_BY_ONKEY			(1)
#define PMIC_GENERAL_USE_BOOT_BY_CHG			(2)
#define PMIC_GENERAL_USE_BOOT_BY_EXTON			(3)
#define PMIC_GENERAL_USE_BOOT_BY_RTC_ALARM		(4)
#define PMIC_GENERAL_USE_BOOT_BY_FOTA			(5)
#define PMIC_GENERAL_USE_BOOT_BY_FULL_RESET		(6)
#define PMIC_GENERAL_USE_BOOT_BY_HW_RESET		(7)
#define PMIC_GENERAL_USE_BOOT_BY_INTENDED_RESET		(8)
#define PMIC_GENERAL_USE_BOOT_BY_DEBUGLEVEL_LOW		(9)
#define PMIC_GENERAL_USE_BOOT_BY_DEBUGLEVEL_MID		(10)
#define PMIC_GENERAL_USE_BOOT_BY_DEBUGLEVEL_HIGH	(11)
#define PMIC_GENERAL_USE_BOOT_BY_RECOVERY_DONE		(12)
#define PMIC_GENERAL_USE_BOOT_BY_SET_SWITCH_SEL		(13)
#define DOWNLOAD_FUS_SUD_BASE				(20)
#define CODE_NOT_SYS_POWER_OFF				(31)
#define PMIC_GENERAL_USE_SHUTDOWN_SHIFT		(5)
#define PMIC_GENERAL_USE_SHUTDOWN_MASK	(7 << PMIC_GENERAL_USE_SHUTDOWN_SHIFT)
#define PMIC_GENERAL_USE_SHUTDOWN_BY_FGERR	(1 << PMIC_GENERAL_USE_SHUTDOWN_SHIFT)
#define PMIC_GENERAL_USE_SHUTDOWN_BY_CHGERR	(2 << PMIC_GENERAL_USE_SHUTDOWN_SHIFT)
#define PMIC_GENERAL_USE_SHUTDOWN_BY_ONKEY	(3 << PMIC_GENERAL_USE_SHUTDOWN_SHIFT)
#define PMIC_GENERAL_USE_SHUTDOWN_BY_UNKNOWN	(4 << PMIC_GENERAL_USE_SHUTDOWN_SHIFT)
#define PMIC_GENERAL_USE_SHUTDOWN_BY_POWEROFF	(5 << PMIC_GENERAL_USE_SHUTDOWN_SHIFT)

struct  __attribute((unused)) pm822_dvc_pdata {
	unsigned int *vol_val;
	int size;
	int reg_dvc;
	int (*set_dvc)(int reg_id, int volt);
	void __iomem *write_reg; /* Register to set level */
	void __iomem *read_reg;  /* Register to read level */
};

struct pm822_rtc_pdata {
	int		(*sync)(unsigned int ticks);
	int		vrtc;
	int		rtc_wakeup;
};

#ifndef CONFIG_SAMSUNG_JACK
struct pm822_headset_pdata {
	int		headset_flag;
	void		(*mic_set_power)(int on);
	int		hook_press_th;
	int		vol_up_press_th;
	int		vol_down_press_th;
	int		mic_det_th;
	int		press_release_th;
};
#endif

struct pm822_vibrator_pdata {
	int		min_timeout;
	int		duty_cycle; /* duty cycle of the PWM */
	void		(*vibrator_power)(struct device *vib_dev, int on);
};

struct pm822_usb_pdata {
	int	vbus_gpio;
	int	id_gpadc;
};

struct pm822_subchip {
	struct i2c_client *power_page;	/* chip client for power page */
	struct i2c_client *gpadc_page;	/* chip client for gpadc page */
	struct i2c_client *test_page;	/* chip client for test page */
	struct regmap *regmap_power;
	struct regmap *regmap_gpadc;
	struct regmap *regmap_test;
	unsigned short power_page_addr;	/* power page I2C address */
	unsigned short gpadc_page_addr;	/* gpadc page I2C address */
	unsigned short test_page_addr;	/* test page I2C address */
};

struct pm822_chip {
	struct pm822_subchip *subchip;
	struct device *dev;
	struct i2c_client *i2c;
	struct regmap *regmap;
	struct regmap_irq_chip *regmap_irq_chip;
	struct regmap_irq_chip_data *irq_data;
	unsigned char version;
	int irq;
	int irq_base;
	unsigned long wu_flag;
	struct proc_dir_entry *proc_file;
	int batt_gp_nr;
};

struct pm822_bat_pdata {
	int bat_ntc; /* bat det by GPADC1 */
	int capacity; /* mAh */
	int r_int; /* mOhm */
};

struct pm822_platform_data {
	struct pm822_rtc_pdata *rtc;
#ifdef CONFIG_SAMSUNG_JACK
	struct sec_jack_platform_data *headset; /* Samsung headset driver data KSND */
#else
	struct pm822_headset_pdata *headset;
#endif
	struct pm822_vibrator_pdata *vibrator;
	struct pm822_usb_pdata *usb;
#if defined(CONFIG_BATTERY_SAMSUNG)
	struct sec_battery_platform_data *fuelgauge_data;
#else
	struct pm822_bat_pdata *bat;
#endif
	struct regulator_init_data *regulator;
#if defined(CONFIG_CPU_PXA1L88) || defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
	struct pm822_dvc_pdata *dvc;
#endif
	int irq_mode;		/* Clear interrupt by read/write(0/1) */
	unsigned short power_page_addr;	/* power page I2C address */
	unsigned short gpadc_page_addr;	/* gpadc page I2C address */
	unsigned short test_page_addr;	/* test page I2C address */
	int num_regulators;
	int (*plat_config)(struct pm822_chip *chip,
				struct pm822_platform_data *pdata);
	unsigned int dummy_dvc_reg;
	unsigned int (*dvc_init_map)[4];
	unsigned long irq_flags;
	int batt_gp_nr;
};

static inline int pm822_request_irq(struct pm822_chip *pm822, int irq,
				     irq_handler_t handler, unsigned long flags,
				     const char *name, void *data)
{
	/*
	 * kernel 3.4 doesn't have regmap_irq_get_virq support yet, so
	 * switch to use normal real irq to request_threaded_irq.
	 */
	return request_threaded_irq(irq, NULL, handler, flags, name, data);
}

static inline void pm822_free_irq(struct pm822_chip *pm822, int irq, void *data)
{
	if (!pm822->irq_data)
		return;

	free_irq(irq, data);
}

#ifdef CONFIG_PM
static inline int pm822_dev_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct pm822_chip *chip = dev_get_drvdata(pdev->dev.parent);
	int irq = platform_get_irq(pdev, 0);

	if (device_may_wakeup(dev))
		set_bit(irq, &chip->wu_flag);

	return 0;
}

static inline int pm822_dev_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct pm822_chip *chip = dev_get_drvdata(pdev->dev.parent);
	int irq = platform_get_irq(pdev, 0);

	if (device_may_wakeup(dev))
		clear_bit(irq, &chip->wu_flag);

	return 0;
}
#endif

extern int pm822_init(struct i2c_client *client,
			     const struct i2c_device_id *id) __devinit;
extern int pm822_deinit(void);
extern int pm822_read_gpadc(int *tbat, unsigned int channel);
extern int get_dynamicbiasgpadc(int *tbat, int gpadc_bias, unsigned int channel);
extern int is_pm822_a0(void);

#ifdef CONFIG_MFD_88PM822
extern int pm822_extern_read(int page, int reg);
extern int pm822_extern_write(int page, int reg, unsigned char val);
extern int pm822_extern_setbits(int page, int reg,
			 unsigned char mask, unsigned char val);
#endif

#ifdef CONFIG_REGULATOR_88PM822
extern int pm822_set_dvc_idx(struct regulator *reg, int dvc_idx);
extern int pm822_get_dvc_vol(struct regulator *reg, int dvc_idx);
#endif

#endif /* __LINUX_MFD_88PM822_H */
