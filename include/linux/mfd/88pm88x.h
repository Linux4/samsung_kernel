/*
 * Marvell 88PM88X Common Interface
 *
 * Copyright (C) 2014 Marvell International Ltd.
 *  Yi Zhang <yizhang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * This file configures the common part of the 88pm88x series PMIC
 */

#ifndef __LINUX_MFD_88PM88X_H
#define __LINUX_MFD_88PM88X_H

#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/atomic.h>
#include <linux/reboot.h>
#include "88pm88x-reg.h"
#include "88pm886-reg.h"
#include <linux/battery/sec_charger.h>

#define PM88X_RTC_NAME		"88pm88x-rtc"
#define PM88X_ONKEY_NAME	"88pm88x-onkey"
#define PM88X_CHARGER_NAME	"88pm88x-charger"
#define PM88X_BATTERY_NAME	"88pm88x-battery"
#define PM88X_HEADSET_NAME	"88pm88x-headset"
#define PM88X_VBUS_NAME		"88pm88x-vbus"
#define PM88X_CFD_NAME		"88pm88x-leds"
#define PM88X_RGB_NAME		"88pm88x-rgb"
#define PM88X_DEBUGFS_NAME	"88pm88x-debugfs"
#define PM88X_GPADC_NAME	"88pm88x-gpadc"
#define PM88X_HWMON_NAME	"88pm88x-hwmon"
#define PM88X_DVC_NAME		"88pm88x-dvc"
#define PM88X_VIRTUAL_REGULATOR_NAME "88pm88x-vr"

enum pm88x_type {
	PM886 = 1,
	PM880 = 2,
};

enum {
	PM88X_ID_VOTG = 0,
};

enum {
	PM88X_RGB_LED0,
	PM88X_RGB_LED1,
	PM88X_RGB_LED2,
};

enum pm88x_pages {
	PM88X_BASE_PAGE = 0,
	PM88X_LDO_PAGE,
	PM88X_GPADC_PAGE,
	PM88X_BATTERY_PAGE,
	PM88X_BUCK_PAGE = 4,
	PM88X_TEST_PAGE = 7,
};

enum pm88x_gpadc {
	PM88X_NO_GPADC = -1,
	PM88X_GPADC0 = 0,
	PM88X_GPADC1,
	PM88X_GPADC2,
	PM88X_GPADC3,
};

/* Interrupt Number in 88PM886 */
enum pm88x_irq_number {
	PM88X_IRQ_ONKEY,	/* EN1b0 *//* 0 */
	PM88X_IRQ_EXTON,	/* EN1b1 */
	PM88X_IRQ_CHG_GOOD,	/* EN1b2 */
	PM88X_IRQ_BAT_DET,	/* EN1b3 */
	PM88X_IRQ_RTC,		/* EN1b4 */
	PM88X_IRQ_CLASSD,	/* EN1b5 *//* 5 */
	PM88X_IRQ_XO,		/* EN1b6 */
	PM88X_IRQ_GPIO,		/* EN1b7 */

	PM88X_IRQ_VBAT,		/* EN2b0 *//* 8 */
				/* EN2b1 */
	PM88X_IRQ_VBUS,		/* EN2b2 */
	PM88X_IRQ_ITEMP,	/* EN2b3 *//* 10 */
	PM88X_IRQ_BUCK_PGOOD,	/* EN2b4 */
	PM88X_IRQ_LDO_PGOOD,	/* EN2b5 */

	PM88X_IRQ_GPADC0,	/* EN3b0 */
	PM88X_IRQ_GPADC1,	/* EN3b1 */
	PM88X_IRQ_GPADC2,	/* EN3b2 *//* 15 */
	PM88X_IRQ_GPADC3,	/* EN3b3 */
	PM88X_IRQ_MIC_DET,	/* EN3b4 */
	PM88X_IRQ_HS_DET,	/* EN3b5 */
	PM88X_IRQ_GND_DET,	/* EN3b6 */

	PM88X_IRQ_CHG_FAIL,	/* EN4b0 *//* 20 */
	PM88X_IRQ_CHG_DONE,	/* EN4b1 */
				/* EN4b2 */
	PM88X_IRQ_CFD_FAIL,	/* EN4b3 */
	PM88X_IRQ_OTG_FAIL,	/* EN4b4 */
	PM88X_IRQ_CHG_ILIM,	/* EN4b5 *//* 25 */
				/* EN4b6 */
	PM88X_IRQ_CC,		/* EN4b7 *//* 27 */

	PM88X_MAX_IRQ,			   /* 28 */
};

enum {
	PM88X_NO_LED = -1,
	PM88X_FLASH_LED = 0,
	PM88X_TORCH_LED,
};

struct pm88x_led_pdata {
	unsigned int cf_en;
	unsigned int cf_txmsk;
	int gpio_en;
	unsigned int flash_timer;
	unsigned int cls_ov_set;
	unsigned int cls_uv_set;
	unsigned int cfd_bst_vset;
	unsigned int bst_uvvbat_set;
	unsigned int max_flash_current;
	unsigned int max_torch_current;
	unsigned int torch_force_max_current;
	unsigned int delay_flash_timer;
	bool conn_cfout_ab;
};

struct pm88x_dvc_ops {
	void (*level_to_reg)(u8 level);
};

struct pm88x_buck1_dvc_desc {
	u8 current_reg;
	int max_level;
	int uV_step1;
	int uV_step2;
	int min_uV;
	int mid_uV;
	int max_uV;
	int mid_reg_val;
};

struct pm88x_dvc {
	struct device *dev;
	struct pm88x_chip *chip;
	struct pm88x_dvc_ops ops;
	struct pm88x_buck1_dvc_desc desc;
};

struct pm88x_chip {
	struct i2c_client *client;
	struct device *dev;

	struct i2c_client *ldo_page;	/* chip client for ldo page */
	struct i2c_client *power_page;	/* chip client for power page */
	struct i2c_client *gpadc_page;	/* chip client for gpadc page */
	struct i2c_client *battery_page;/* chip client for battery page */
	struct i2c_client *buck_page;	/* chip client for buck page */
	struct i2c_client *test_page;	/* chip client for test page */

	struct regmap *base_regmap;
	struct regmap *ldo_regmap;
	struct regmap *power_regmap;
	struct regmap *gpadc_regmap;
	struct regmap *battery_regmap;
	struct regmap *buck_regmap;
	struct regmap *test_regmap;
	struct regmap *codec_regmap;

	unsigned short ldo_page_addr;	/* ldo page I2C address */
	unsigned short power_page_addr;	/* power page I2C address */
	unsigned short gpadc_page_addr;	/* gpadc page I2C address */
	unsigned short battery_page_addr;/* battery page I2C address */
	unsigned short buck_page_addr;	/* buck page I2C address */
	unsigned short test_page_addr;	/* test page I2C address */

	unsigned int chip_id;
	long type;
	int irq;

	int irq_mode;
	struct regmap_irq_chip_data *irq_data;

	bool rtc_wakeup;
	u8 powerdown1;
	u8 powerdown2;
	u8 powerup;

	struct notifier_block reboot_notifier;
	struct notifier_block cb_nb;
	struct pm88x_dvc *dvc;
	
	struct pm88x_platform_data *pdata;
};

struct pm88x_platform_data {
	unsigned int chgen;
	int irq_gpio;
	unsigned long chg_irq_attr;
	sec_battery_platform_data_t *battery_data;
};

extern bool buck1slp_is_ever_changed;
extern bool buck1slp_ever_used_by_map;

struct pm88x_chip *pm88x_init_chip(struct i2c_client *client);
int pm88x_parse_dt(struct device_node *np, struct pm88x_chip *chip);

int pm88x_init_pages(struct pm88x_chip *chip);
int pm88x_post_init_chip(struct pm88x_chip *chip);
void pm800_exit_pages(struct pm88x_chip *chip);

int pm88x_init_subdev(struct pm88x_chip *chip);
long pm88x_of_get_type(struct device *dev);
void pm88x_dev_exit(struct pm88x_chip *chip);

int pm88x_irq_init(struct pm88x_chip *chip);
int pm88x_irq_exit(struct pm88x_chip *chip);
int pm88x_apply_patch(struct pm88x_chip *chip);
int pm88x_stepping_fixup(struct pm88x_chip *chip);
int pm88x_apply_bd_patch(struct pm88x_chip *chip, struct device_node *np);

extern struct regmap_irq_chip pm88x_irq_chip;
extern const struct of_device_id pm88x_of_match[];

struct pm88x_chip *pm88x_get_chip(void);
void pm88x_set_chip(struct pm88x_chip *chip);
void pm88x_power_off(void);
int pm88x_reboot_notifier_callback(struct notifier_block *nb,
				   unsigned long code, void *cmd);
/* gpadc part */
int extern_pm88x_gpadc_set_current_generator(int gpadc_number, int on);
int extern_pm88x_gpadc_get_volt(int gpadc_number, int *volt);
int extern_pm88x_gpadc_set_bias_current(int gpadc_number, int bias);

/* dvc external interface */
#ifdef CONFIG_MFD_88PM88X
int pm88x_dvc_set_volt(u8 level, int uv);
int pm88x_dvc_get_volt(u8 level);
struct regmap *get_companion(void);
struct regmap *get_codec_companion(void);
#else
static inline int pm88x_dvc_set_volt(u8 level, int uv)
{
	return 0;
}
static inline int pm88x_dvc_get_volt(u8 level)
{
	return 0;
}
static inline struct regmap *get_companion(void)
{
	return NULL;
}
static inline struct regmap *get_codec_companion(void)
{
	return NULL;
}
#endif

#ifdef CONFIG_LEDS_88PM88X_CFD
int get_flash_duration(unsigned int *duration);
int set_torch_current(unsigned int cur);
#else
static inline int get_flash_duration(unsigned int *duration)
{
	return -EINVAL;
}
static inline int set_torch_current(unsigned int cur) {
	return -EINVAL;
}
#endif

static const struct resource vr_resources[] = {
	{
	.name = PM88X_VIRTUAL_REGULATOR_NAME,
	},
};

/* debugfs part */
#ifdef CONFIG_REGULATOR_88PM88X
extern int pm88x_display_buck(struct pm88x_chip *chip, char *buf);
extern int pm88x_display_vr(struct pm88x_chip *chip, char *buf);
extern int pm88x_display_ldo(struct pm88x_chip *chip, char *buf);
#else
static inline int pm88x_display_buck(struct pm88x_chip *chip, char *buf)
{
	return 0;
}
static inline int pm88x_display_vr(struct pm88x_chip *chip, char *buf)
{
	return 0;
}
static inline int pm88x_display_ldo(struct pm88x_chip *chip, char *buf)
{
	return 0;
}
#endif
#ifdef CONFIG_MFD_88PM8XX_DVC
extern int pm88x_display_dvc(struct pm88x_chip *chip, char *buf);
#else
static inline int pm88x_display_dvc(struct pm88x_chip *chip, char *buf)
{
	return 0;
}
#endif
#ifdef CONFIG_88PM88X_GPADC
extern int pm88x_display_gpadc(struct pm88x_chip *chip, char *buf);
#else
static inline int pm88x_display_gpadc(struct pm88x_chip *chip, char *buf)
{
	return 0;
}
#endif
#endif /* __LINUX_MFD_88PM88X_H */
