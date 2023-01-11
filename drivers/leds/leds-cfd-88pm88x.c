/*
 * 88PM88X camera flash/torch LED driver
 *
 * Copyright (C) 2014 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/mfd/88pm88x.h>
#include <linux/mfd/88pm886.h>
#include <linux/mfd/88pm880.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/wakelock.h>

/* common register PM886 and PM880 */
#define PM88X_CFD_CONFIG1		(0x60)
#define PM88X_CLS_CONFIG1		(0x71)

#define PM88X_BST_CONFIG1		(0x6B)
#define PM88X_BST_CONFIG2		(0x6C)
#define PM88X_BST_CONFIG3		(0x6D)
#define PM88X_BST_CONFIG4		(0x6E)
#define PM88X_BST_CONFIG5		(0x6F)
#define PM88X_BST_CONFIG6		(0x70)

/* 88PM88X bitmask definitions */
#define PM88X_CFD_CLK_EN		(1 << 1)
#define PM88X_FLASH_TIMER_OFFSET	(0)
#define PM88X_FLASH_TIMER_MASK		(0x1F << PM88X_FLASH_TIMER_OFFSET)
#define PM88X_CF_CFD_PLS_ON		(1 << 0)
#define PM88X_CF_BITMASK_TRCH_RESET	(1 << 1)
#define PM88X_CF_BITMASK_MODE		(1 << 0)
#define PM88X_SELECT_FLASH_MODE	(1 << 0)
#define PM88X_SELECT_TORCH_MODE	(0 << 0)
#define PM88X_CF_EN_HIGH		(1 << 3)
#define PM88X_CFD_ENABLED		(1 << 2)
#define PM88X_CFD_MASKED		(0 << 2)

#define PM88X_BST_HSOC_OFFSET		(5)
#define PM88X_BST_HSOC_MASK		(0x7 << PM88X_BST_HSOC_OFFSET)
#define PM88X_BST_HSOC_SET_2P5A		(0x5 << PM88X_BST_HSOC_OFFSET)
#define PM88X_BST_HSOC_SET_3A		(0x6 << PM88X_BST_HSOC_OFFSET)
#define PM88X_BST_LSOC_OFFSET		(1)
#define PM88X_BST_LSOC_MASK		(0x7 << PM88X_BST_LSOC_OFFSET)
#define PM88X_BST_LSOC_SET_3A		(0x4 << PM88X_BST_LSOC_OFFSET)
#define PM88X_BST_LSOC_SET_4A		(0x6 << PM88X_BST_LSOC_OFFSET)
#define PM88X_BST_HSZC_OFFSET		(5)
#define PM88X_BST_HSZC_MASK		(0x3 << PM88X_BST_HSZC_OFFSET)


#define PM88X_BST_VSET_OFFSET		(4)
#define PM88X_BST_VSET_MASK		(0x7 << PM88X_BST_VSET_OFFSET)
#define PM880_BST_VSET_MASK		(0xf << PM88X_BST_VSET_OFFSET)
#define PM88X_BST_UVVBAT_OFFSET	(0)
#define PM88X_BST_UVVBAT_MASK		(0x3 << PM88X_BST_UVVBAT_OFFSET)

#define PM88X_BST_ILIM_DIS		(0x1 << 7)

#define PM88X_CURRENT_DIV(x, y)		DIV_ROUND_UP(256, ((x) / (y)))
#define PM88X_FLASH_ISET_OFFSET	(0)
#define PM88X_FLASH_ISET_MASK		(0x1f << PM88X_FLASH_ISET_OFFSET)
#define PM88X_TORCH_ISET_OFFSET	(5)
#define PM88X_TORCH_ISET_MASK		(0x7 << PM88X_TORCH_ISET_OFFSET)
#define PM88X_LED_TORCH_ISET(x, y)	((((x) - y) / y) << PM88X_TORCH_ISET_OFFSET)
#define PM88X_LED_FLASH_ISET(x, y)	((((x) - y) / y) << PM88X_FLASH_ISET_OFFSET)


#define PM88X_NUM_FLASH_CURRENT_LEVELS	0x20
#define PM88X_NUM_TORCH_CURRENT_LEVELS	0x8

#define PM88X_MAX_FLASH_CURRENT		(1600)		/* mA */
#define PM88X_MAX_SAFE_FLASH_CURRENT	(PM88X_MAX_FLASH_CURRENT - 600)		/* mA */

#define PM88X_MAX_TORCH_CURRENT		(400)		/* mA */
#define PM88X_MAX_SAFE_TORCH_CURRENT	(PM88X_MAX_TORCH_CURRENT - 200)		/* mA */

#define PM88X_BST_VSET_5P25V		(0x6 << PM88X_BST_VSET_OFFSET)
#define PM88X_BST_VSET_4P75V		(0x4 << PM88X_BST_VSET_OFFSET)
#define PM88X_BST_VSET_4P5V		(0x3 << PM88X_BST_VSET_OFFSET)
#define PM88X_BST_OV_SET_MASK		(0x3 << 1)
#define PM88X_BST_OV_SET_6V		(0x3 << 1)
#define PM88X_BST_OV_SET_5V		(0x2 << 1)
#define PM88X_BST_OV_SET_5P2V		(0x1 << 1)

#define CFD_MIN_BOOST_VOUT		3750
#define CFD_MAX_BOOST_VOUT		5500
#define CFD_VOUT_GAP_LEVEL		250

/* PM886 specific register */
#define PM886_CFD_CONFIG2		(0x61)
#define PM886_CFD_CONFIG3		(0x62)
#define PM886_CFD_CONFIG4		(0x63)
#define PM886_CFD_LOG1			(0x65)

#define PM886_BOOST_MODE		(1 << 1)

#define PM886_OV_SET_OFFSET	(4)
#define PM886_OV_SET_MASK		(0x3 << PM886_OV_SET_OFFSET)
#define PM886_UV_SET_OFFSET	(1)
#define PM886_UV_SET_MASK		(0x3 << PM886_UV_SET_OFFSET)

#define PM886_BST_CFD_STPFLS_DIS	(0x1 << 7)
#define PM886_BST_UVVBAT_EN_MASK	(0x1 << 3)
#define PM886_CFOUT_OV_EN_OFFSET	(3)
#define PM886_CFOUT_OV_EN_MASK		(0x1 << PM886_CFOUT_OV_EN_OFFSET)

#define PM88X_BST_TON_SET		(0x3 << 6)
#define PM886_CURRENT_LEVEL_STEPS	50

/* PM880 specific register */
#define PM880_CFD_CONFIG12		(0x61)
#define PM880_CFD_CONFIG2		(0x62)
#define PM880_CFD_CONFIG3		(0x63)
#define PM880_CFD_CONFIG4		(0x64)
#define PM880_CFD_LOG1			(0x66)
#define PM880_CFD_LOG12			(0x67)
#define PM880_CLS_CONFIG2		(0x72)

#define PM880_CL1_EN		(1 << 5)
#define PM880_CL2_EN		(1 << 6)

#define PM880_OV_SET_OFFSET	(6)
#define PM880_OV_SET_MASK		(0x3 << PM880_OV_SET_OFFSET)
#define PM880_UV_SET_OFFSET	(2)
#define PM880_UV_SET_MASK		(0x1 << PM880_UV_SET_OFFSET)
#define PM880_BST_UVVBAT_EN_MASK	(0x1 << 2)
#define PM880_CLS_OC_EN_MSK	(0x3 << 0)
#define PM880_CFOUT_OC_EN_OFFSET	(0)
#define PM880_CFOUT_OC_EN_MASK		(0x3 << PM880_CFOUT_OC_EN_OFFSET)
#define PM880_CLK_NOK_OC_DB		(0x1 << 3)

#define PM880_BST_RAMP_DIS_OFFSET	(5)
#define PM880_BST_RAMP_DIS_MASK		(0x1 << PM880_BST_RAMP_DIS_OFFSET)
#define PM880_BST_RAMP_DIS_EN		(0x1 << PM880_BST_RAMP_DIS_OFFSET)

#define PM880_CURRENT_LEVEL_STEPS	25

static int gpio_en;
static unsigned int dev_num;
static unsigned int delay_flash_timer;
static int pa_en;
struct pm88x_led {
	struct led_classdev cdev;
	struct work_struct work;
	struct work_struct led_flash_off;
 	struct delayed_work delayed_work;
	struct workqueue_struct *led_wqueue;
	struct pm88x_chip *chip;
	struct mutex lock;
#define MFD_NAME_SIZE		(40)
	struct wake_lock wake_lock;
	char name[MFD_NAME_SIZE];

	unsigned int brightness;
	unsigned int current_brightness;
	unsigned int max_current_div;
	unsigned int iset_step;
	unsigned int force_max_current;
	unsigned int cfd_bst_vset;
	bool conn_cfout_ab;

	int id;
	/* for external CF_EN and CF_TXMSK */
	int cf_en;
	int cf_txmsk;
};

struct pm88x_led *g_led = NULL;

static char *supply_interface[] = {
	"sec-charger",
};

static void pm88x_led_bright_set(struct led_classdev *cdev, enum led_brightness value);

static unsigned int clear_errors(struct pm88x_led *led)
{
	struct pm88x_chip *chip;
	unsigned int reg;

	/*!!! mutex must be locked upon entering this function */

	chip = led->chip;

	if (chip->type == PM886) {
		regmap_read(chip->battery_regmap, PM886_CFD_LOG1, &reg);

		if (reg) {
			dev_err(led->cdev.dev, "flash/torch fail. error log=0x%x\n", reg);
			/* clear all errors, write 1 to clear */
			regmap_write(chip->battery_regmap, PM886_CFD_LOG1, reg);
		}
	} else {
		regmap_read(chip->battery_regmap, PM880_CFD_LOG1, &reg);

		if (reg) {
			dev_err(led->cdev.dev, "flash/torch fail. error log=0x%x\n", reg);
			/* clear all errors, write 1 to clear */
			regmap_write(chip->battery_regmap, PM880_CFD_LOG1, reg);
		}
	}
	return reg;
}

static void pm88x_led_delayed_work(struct work_struct *work)
{
	struct pm88x_led *led;
	unsigned long jiffies;
	unsigned int ret;

	led = container_of(work, struct pm88x_led,
			   delayed_work.work);
	mutex_lock(&led->lock);
	ret = clear_errors(led);
	/* set TRCH_TIMER_RESET to reset torch timer so
	 * led won't turn off */
	switch (led->chip->type) {
	case PM886:
		regmap_update_bits(led->chip->battery_regmap, PM886_CFD_CONFIG4,
			PM88X_CF_BITMASK_TRCH_RESET,
			PM88X_CF_BITMASK_TRCH_RESET);
		break;
	case PM880:
		regmap_update_bits(led->chip->battery_regmap, PM880_CFD_CONFIG4,
			PM88X_CF_BITMASK_TRCH_RESET,
			PM88X_CF_BITMASK_TRCH_RESET);
		break;
	default:
		dev_err(led->cdev.dev, "Unsupported chip type %ld\n", led->chip->type);
		mutex_unlock(&led->lock);
		return;
	}
	mutex_unlock(&led->lock);
	if (!ret) {
		/* reschedule torch timer reset */
		jiffies = msecs_to_jiffies(5000);
		schedule_delayed_work(&led->delayed_work, jiffies);
	} else
		pm88x_led_bright_set(&led->cdev, 0);
}

static void torch_set_current(struct pm88x_led *led)
{
	unsigned int brightness;
	struct pm88x_chip *chip;
	chip = led->chip;

	/* set torch current */
	switch (chip->type) {
	case PM886:
	brightness = led->brightness;
		regmap_update_bits(chip->battery_regmap, PM88X_CFD_CONFIG1,
			PM88X_TORCH_ISET_MASK,
			PM88X_LED_TORCH_ISET(brightness, led->iset_step));
		break;
	case PM880:
		if (led->conn_cfout_ab) {
			brightness = led->brightness / 2;
			regmap_update_bits(chip->battery_regmap, PM88X_CFD_CONFIG1,
					PM88X_TORCH_ISET_MASK,
					PM88X_LED_TORCH_ISET(brightness, led->iset_step));
			regmap_update_bits(chip->battery_regmap, PM880_CFD_CONFIG12,
					PM88X_TORCH_ISET_MASK,
					PM88X_LED_TORCH_ISET(brightness, led->iset_step));
		} else {
			brightness = led->brightness;
			regmap_update_bits(chip->battery_regmap, PM88X_CFD_CONFIG1,
					PM88X_TORCH_ISET_MASK,
					PM88X_LED_TORCH_ISET(brightness, led->iset_step));
		}
		break;
	default:
		dev_err(led->cdev.dev, "Unsupported chip type %ld\n", chip->type);
		return;
	}
}


static void torch_on(struct pm88x_led *led)
{
	unsigned long jiffies;
	struct pm88x_chip *chip;

	chip = led->chip;
	mutex_lock(&led->lock);
	if (!gpio_en) {
		if (chip->type == PM886)
			/* clear CFD_PLS_ON to disable */
			regmap_update_bits(chip->battery_regmap, PM886_CFD_CONFIG4,
					   PM88X_CF_CFD_PLS_ON, 0);
		else
			/* clear CFD_PLS_ON to disable */
			regmap_update_bits(chip->battery_regmap, PM880_CFD_CONFIG4,
				   PM88X_CF_CFD_PLS_ON, 0);
	} else {
		gpio_direction_output(led->cf_en, 0);
	}

	switch (chip->type) {
	case PM886:
		/* set TRCH_TIMER_RESET to reset torch timer so
		 * led won't turn off */
		regmap_update_bits(chip->battery_regmap, PM886_CFD_CONFIG4,
				PM88X_CF_BITMASK_TRCH_RESET,
				PM88X_CF_BITMASK_TRCH_RESET);
		clear_errors(led);

		/* set torch mode & boost voltage mode */
		regmap_update_bits(chip->battery_regmap, PM886_CFD_CONFIG3,
				PM88X_CF_BITMASK_MODE, PM88X_SELECT_TORCH_MODE);
		break;
	case PM880:
		/* set TRCH_TIMER_RESET to reset torch timer so
		 * led won't turn off */
		regmap_update_bits(chip->battery_regmap, PM880_CFD_CONFIG4,
				PM88X_CF_BITMASK_TRCH_RESET,
				PM88X_CF_BITMASK_TRCH_RESET);
		clear_errors(led);
		/* set torch mode & boost voltage mode */
		if (led->conn_cfout_ab)
			regmap_update_bits(chip->battery_regmap, PM880_CFD_CONFIG3,
					(PM88X_CF_BITMASK_MODE | PM880_CL1_EN | PM880_CL2_EN),
					PM88X_SELECT_TORCH_MODE | PM880_CL1_EN | PM880_CL2_EN);
		else
			regmap_update_bits(chip->battery_regmap, PM880_CFD_CONFIG3,
					(PM88X_CF_BITMASK_MODE | PM880_CL1_EN),
					PM88X_SELECT_TORCH_MODE | PM880_CL1_EN);
		break;
	default:
		dev_err(led->cdev.dev, "Unsupported chip type %ld\n", chip->type);
		mutex_unlock(&led->lock);
		return;
	}
	
	/* disable booster HS current limit */
	regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG6,
			PM88X_BST_ILIM_DIS, PM88X_BST_ILIM_DIS);

	/* automatic booster mode */
	torch_set_current(led);
	if (!gpio_en) {
		if (chip->type == PM886)
			/* set CFD_PLS_ON to enable */
			regmap_update_bits(chip->battery_regmap, PM886_CFD_CONFIG4,
					   PM88X_CF_CFD_PLS_ON, PM88X_CF_CFD_PLS_ON);
		else
			regmap_update_bits(chip->battery_regmap, PM880_CFD_CONFIG4,
					   PM88X_CF_CFD_PLS_ON, PM88X_CF_CFD_PLS_ON);
	} else {
		gpio_direction_output(led->cf_en, 1);
	}
	mutex_unlock(&led->lock);

	/* Once on, torch will only remain on for 10 seconds so we
	 * schedule work every 5 seconds (5000 msecs) to poke the
	 * chip register so it can remain on */
	jiffies = msecs_to_jiffies(5000);
	schedule_delayed_work(&led->delayed_work, jiffies);
	wake_lock(&led->wake_lock);
}

static void cfd_pls_off(struct pm88x_led *led)
{
	struct pm88x_chip *chip;

	chip = led->chip;

	mutex_lock(&led->lock);
	if (!gpio_en) {
		switch (chip->type) {
		case PM886:
			/* clear CFD_PLS_ON to disable */
			regmap_update_bits(chip->battery_regmap, PM886_CFD_CONFIG4,
				   PM88X_CF_CFD_PLS_ON, 0);
			break;
		case PM880:
			/* clear CFD_PLS_ON to disable */
			regmap_update_bits(chip->battery_regmap, PM880_CFD_CONFIG4,
				   PM88X_CF_CFD_PLS_ON, 0);
			break;
		default:
			dev_err(led->cdev.dev, "Unsupported chip type %ld\n", chip->type);
			break;
		}
	} else
		gpio_direction_output(led->cf_en, 0);

	mutex_unlock(&led->lock);
	wake_unlock(&led->wake_lock);
}

static void pm88x_flash_off(struct work_struct *work)
{
	struct pm88x_led *led;

	led = container_of(work, struct pm88x_led, led_flash_off);
 	dev_info(led->cdev.dev, "turning flash off!\n");
 	cfd_pls_off(led);
 }

static void torch_off(struct pm88x_led *led)
{
	cancel_delayed_work_sync(&led->delayed_work);
	cfd_pls_off(led);
}

static void strobe_flash(struct pm88x_led *led)
{
	struct pm88x_chip *chip;
	struct power_supply *psy;
	union power_supply_propval val;
	int i, ret, chg_status, chg_online = 0;
	unsigned int brightness;
	chip = led->chip;

	mutex_lock(&led->lock);
	regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG2,
						PM886_BST_UVVBAT_EN_MASK, 0);
	if (!gpio_en) {
		if (chip->type == PM886)
			/* clear CFD_PLS_ON to disable */
			regmap_update_bits(chip->battery_regmap, PM886_CFD_CONFIG4,
				   PM88X_CF_CFD_PLS_ON, 0);
		else
		/* clear CFD_PLS_ON to disable */
			regmap_update_bits(chip->battery_regmap, PM880_CFD_CONFIG4,
				   PM88X_CF_CFD_PLS_ON, 0);
	} else {
		gpio_direction_output(led->cf_en, 0);
	}
	clear_errors(led);

	/* get charger status */
	for (i = 0; i < ARRAY_SIZE(supply_interface); i++) {
		psy = power_supply_get_by_name(supply_interface[i]);
		if (!psy || !psy->get_property)
			continue;
		ret = psy->get_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
		regmap_read(chip->battery_regmap, PM88X_CHG_STATUS2, &chg_status);
		if (ret >= 0 && (chg_status & 0x8)) {
			chg_online = val.intval;
			break;
		}
	}
	
	printk("psy->name = %s,chg_online = %d \n",psy->name,chg_online);
	/* while USB/CHG present block charging */
	if (psy && psy->set_property) {
		printk(" block the charging ... \n");
		val.intval = 0;
		psy->set_property(psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
	}

	brightness = led->brightness;
	if (chg_online) {
		switch (chip->type) {
		case PM886:
			regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG5,
					(PM886_BST_CFD_STPFLS_DIS | PM88X_BST_OV_SET_MASK),
					(PM886_BST_CFD_STPFLS_DIS | PM88X_BST_OV_SET_6V));
			regmap_update_bits(chip->battery_regmap, PM88X_CLS_CONFIG1,
					PM886_CFOUT_OV_EN_MASK, 0x0);
			regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG1,
					PM88X_BST_VSET_MASK, PM88X_BST_VSET_4P75V);
			/* unlock test page & force booster configuration */
			regmap_write(chip->base_regmap, 0x1F, 0x1);
			regmap_write(chip->test_regmap, 0x40, 0x0);
			regmap_write(chip->test_regmap, 0x41, 0x0);
			regmap_write(chip->test_regmap, 0x42, 0x0);
			regmap_write(chip->test_regmap, 0x43, 0x0);
			regmap_write(chip->test_regmap, 0x44, 0x9);
			regmap_write(chip->test_regmap, 0x45, 0x2);
			regmap_write(chip->test_regmap, 0x46, 0x10);

			/* max brightness allowed in the state 1200mA */
			led->brightness = min_t(unsigned int, 1200, led->brightness);
			brightness = led->brightness;
			regmap_update_bits(chip->battery_regmap, PM886_CFD_CONFIG3,
				PM88X_CF_BITMASK_MODE, PM88X_SELECT_FLASH_MODE);
			regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG6,
				PM88X_BST_ILIM_DIS, PM88X_BST_ILIM_DIS);
			break;
		case PM880:
			regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG1,
					PM880_BST_VSET_MASK, PM88X_BST_VSET_4P75V);
			regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG2,
					PM880_BST_RAMP_DIS_MASK, PM880_BST_RAMP_DIS_EN);
			regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG5,
					(PM88X_BST_OV_SET_MASK), (PM88X_BST_OV_SET_5V));
			regmap_update_bits(chip->battery_regmap, PM880_CLS_CONFIG2,
					PM880_CFOUT_OC_EN_MASK, 0);
			/* unlock test page & force booster configuration */
			regmap_write(chip->base_regmap, 0x1F, 0x1);
			regmap_write(chip->test_regmap, 0x40, 0x0);
			regmap_write(chip->test_regmap, 0x41, 0x0);
			regmap_write(chip->test_regmap, 0x42, 0x0);
			regmap_write(chip->test_regmap, 0x43, 0x0);
			regmap_write(chip->test_regmap, 0x44, 0x4);
			regmap_write(chip->test_regmap, 0x45, 0x4);
			regmap_write(chip->test_regmap, 0x46, 0x10);

			/* max brightness allowed in the state 650mA for one cf_out */
			if (led->conn_cfout_ab) {
				led->brightness = min_t(unsigned int, 1300, led->brightness);
				brightness = led->brightness / 2;
				regmap_update_bits(chip->battery_regmap, PM880_CFD_CONFIG3,
					(PM88X_CF_BITMASK_MODE | PM880_CL1_EN | PM880_CL2_EN),
					(PM88X_SELECT_FLASH_MODE | PM880_CL1_EN | PM880_CL2_EN));
			} else {
				led->brightness = min_t(unsigned int, 650, led->brightness);
				brightness = led->brightness;
				regmap_update_bits(chip->battery_regmap, PM880_CFD_CONFIG3,
 					(PM88X_CF_BITMASK_MODE | PM880_CL1_EN),
 					(PM88X_SELECT_FLASH_MODE | PM880_CL1_EN));
			}
			break;
		default:
			dev_err(led->cdev.dev, "Unsupported chip type %ld\n", chip->type);
			mutex_unlock(&led->lock);
			return;
		}
	} else {
		/*
		 * set flash mode
		 * if flash current is above 400mA, set booster mode to current
		 * regulation mode and enable booster HS current limit otherwise set booster
		 * mode to voltage regulation mode and disable booster HS current limit
		 */
		switch (chip->type) {
		case PM886:
			if (led->brightness > 400) {
				regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG6,
						PM88X_BST_ILIM_DIS, PM88X_BST_ILIM_DIS);
			} else {
				regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG6,
						PM88X_BST_ILIM_DIS, PM88X_BST_ILIM_DIS);
			}
			regmap_update_bits(chip->battery_regmap, PM886_CFD_CONFIG3,
					PM88X_CF_BITMASK_MODE, PM88X_SELECT_FLASH_MODE);
			regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG5,
					PM88X_BST_OV_SET_MASK, PM88X_BST_OV_SET_6V);
			break;
		case PM880:
			if (led->conn_cfout_ab) {
				brightness = led->brightness / 2;
				regmap_update_bits(chip->battery_regmap, PM880_CFD_CONFIG3,
					(PM88X_CF_BITMASK_MODE | PM880_CL1_EN | PM880_CL2_EN),
					(PM88X_SELECT_FLASH_MODE | PM880_CL1_EN | PM880_CL2_EN));
			} else {
				brightness = led->brightness;
				regmap_update_bits(chip->battery_regmap, PM880_CFD_CONFIG3,
					(PM88X_CF_BITMASK_MODE | PM880_CL1_EN),
					(PM88X_SELECT_FLASH_MODE | PM880_CL1_EN));
			}

			regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG5,
					(PM88X_BST_OV_SET_MASK), (PM88X_BST_OV_SET_5V));

			regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG6,
					PM88X_BST_ILIM_DIS, PM88X_BST_ILIM_DIS);
			break;
		default:
			dev_err(led->cdev.dev, "Unsupported chip type %ld\n", chip->type);
			mutex_unlock(&led->lock);
			return;
		}
	}

	/* automatic booster enable mode */
	/* set flash current
	 * for cf_out_a/b connected case, we already config flash current for cf_out_b
	 */
	regmap_update_bits(chip->battery_regmap, PM88X_CFD_CONFIG1,
			PM88X_FLASH_ISET_MASK,
			PM88X_LED_FLASH_ISET(brightness, led->iset_step));
	switch (chip->type) {
	case PM880:
		if (led->conn_cfout_ab)
			regmap_update_bits(chip->battery_regmap, PM880_CFD_CONFIG12,
				PM88X_FLASH_ISET_MASK,
				PM88X_LED_FLASH_ISET(brightness, led->iset_step));
		break;
	case PM886:
	default:
		break;
	}

	/* trigger flash */
	if (!gpio_en) {
		if (chip->type == PM886) {
			regmap_update_bits(chip->battery_regmap, PM886_CFD_CONFIG4,
					   PM88X_CF_CFD_PLS_ON, PM88X_CF_CFD_PLS_ON);
		} else {
			regmap_update_bits(chip->battery_regmap, PM880_CFD_CONFIG4,
			   PM88X_CF_CFD_PLS_ON, PM88X_CF_CFD_PLS_ON);
		}
	} else {
		gpio_direction_output(led->cf_en, 1);
	}

	if (chg_online) {
		/* unlock mutex during sleep because flash off might be call */
		mutex_unlock(&led->lock);
		/* add 50msec for the HW delay which was chosen */
		msleep(delay_flash_timer + 50);
		mutex_lock(&led->lock);

		switch (chip->type) {
		case PM886:
		case PM880:
			/* restore test page booster configuration */
			regmap_write(chip->test_regmap, 0x46, 0x0);
			regmap_write(chip->test_regmap, 0x40, 0x0);
			regmap_write(chip->test_regmap, 0x41, 0x0);
			regmap_write(chip->test_regmap, 0x42, 0x0);
			regmap_write(chip->test_regmap, 0x43, 0x0);
			regmap_write(chip->test_regmap, 0x44, 0x0);
			regmap_write(chip->test_regmap, 0x45, 0x0);

			if (chip->type == PM886) {
				regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG5,
						(PM886_BST_CFD_STPFLS_DIS | PM88X_BST_OV_SET_MASK),
						PM88X_BST_OV_SET_5P2V);
				regmap_update_bits(chip->battery_regmap, PM88X_CLS_CONFIG1,
						PM886_CFOUT_OV_EN_MASK, PM886_CFOUT_OV_EN_MASK);
				regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG1,
						PM88X_BST_VSET_MASK, PM88X_BST_VSET_4P75V);
			} else {
				regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG2,
						PM880_BST_RAMP_DIS_MASK, 0);

				regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG1,
						PM880_BST_VSET_MASK, PM88X_BST_VSET_4P75V);
			}

			/* lock test page */
			regmap_write(chip->base_regmap, 0x1F, 0x0);
			break;
		default:
			dev_err(led->cdev.dev, "Unsupported chip type %ld\n", chip->type);
			mutex_unlock(&led->lock);
			return;
		}
	}

	/* allow charging */
	if (psy && psy->set_property) {
		printk("restart charging ... \n");
		val.intval = 1;
		psy->set_property(psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
	}
#ifdef CONFIG_MACH_J1ACELTE_LTN
	regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG1,
			PM88X_BST_VSET_MASK, PM88X_BST_VSET_4P5V);
#endif
	mutex_unlock(&led->lock);
}

static void pm88x_led_work(struct work_struct *work)
{
	struct pm88x_led *led;

	led = container_of(work, struct pm88x_led, work);

	if (led->id == PM88X_FLASH_LED) {
		if (led->brightness != 0)
			strobe_flash(led);
	} else if (led->id == PM88X_TORCH_LED) {
		if ((led->current_brightness == 0) && led->brightness)
			torch_on(led);
		if (led->brightness && (led->brightness != led->current_brightness))
			torch_set_current(led);
		if (led->brightness == 0)
			torch_off(led);

		led->current_brightness = led->brightness;
	}
}

/**++ added for camera flash sysfs ++**/
extern struct class *camera_class;
struct device *flash_dev;	  
extern struct device *cam_dev_rear;
/**-- camera flash sysfs --**/

static void pm88x_led_bright_set(struct led_classdev *cdev,
			   enum led_brightness value)
{
	struct pm88x_led *led = container_of(cdev, struct pm88x_led, cdev);

	/* skip the lowest level */
	if (value == 0){
		led->brightness = 0;
		/*
		 * The reason to turn off the flash here and not in the system workqueue:
		 * While turning on the flash + usb connected, the strobe_flash function
		 * will sleep untill the flash will be off, and adding flash_off to system
		 * workqueue will occured only after that. To turn off the flash right, it
		 * should be done out of the system workqueue.
		 */
		if (led->id == PM88X_FLASH_LED) {
			queue_work(led->led_wqueue, &led->led_flash_off);
			return;
		}
	} else {
		if (led->force_max_current)
			value = LED_FULL;
		led->brightness = ((value / led->max_current_div) + 1)
				* led->iset_step;
		if (led->conn_cfout_ab)
			led->brightness *= 2;
	}

	dev_dbg(led->cdev.dev, "value = %d, brightness = %d\n",
		 value, led->brightness);
	schedule_work(&led->work);
}

static ssize_t gpio_ctrl_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct pm88x_led *led;
	struct led_classdev *cdev;

	int s = 0;
	cdev = dev_get_drvdata(dev);
	led = container_of(cdev, struct pm88x_led, cdev);

	s += sprintf(buf, "gpio control: %d\n", gpio_en);
	return s;
}

static ssize_t gpio_ctrl_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct pm88x_led *led;
	struct led_classdev *cdev;
	int ret;
	int gpio_state;

	cdev = dev_get_drvdata(dev);
	led = container_of(cdev, struct pm88x_led, cdev);

	ret = sscanf(buf, "%d", &gpio_state);
	if (ret < 0) {
		dev_dbg(dev, "%s: get gpio_en error, set to 0 as default\n", __func__);
		gpio_state = 0;
	}

	dev_info(dev, "%s: gpio control %s\n",
		 __func__, (gpio_state ? "enabled" : "disabled"));

	cancel_delayed_work_sync(&led->delayed_work);
	mutex_lock(&led->lock);

	/* clear CF_EN and CFD_PLS_ON before switching modes */

	/* request gpio only if it wasn't already requested */
	if (!gpio_en) {
		ret = gpio_request(led->cf_en, "cf-gpio");
		if (ret) {
			dev_err(led->cdev.dev, "request cf-gpio error!\n");
			goto error;
		}
	}
	gpio_direction_output(led->cf_en, 0);

	/* release gpio in case the user disabled it */
	if (!gpio_state)
		gpio_free(led->cf_en);

	gpio_en = gpio_state;

	if (led->chip->type == PM886) {
		/* clear CFD_PLS_ON to disable */
		ret = regmap_update_bits(led->chip->battery_regmap, PM886_CFD_CONFIG4,
					PM88X_CF_CFD_PLS_ON, 0);
		if (ret)
			goto error;

		if (gpio_en)
			/* high active */
			ret = regmap_update_bits(led->chip->battery_regmap, PM886_CFD_CONFIG3,
						 (PM88X_CF_EN_HIGH | PM88X_CFD_ENABLED),
						 (PM88X_CF_EN_HIGH | PM88X_CFD_ENABLED));
		else
			ret = regmap_update_bits(led->chip->battery_regmap, PM886_CFD_CONFIG3,
						(PM88X_CF_EN_HIGH | PM88X_CFD_ENABLED),
						PM88X_CFD_MASKED);
	} else {
		/* clear CFD_PLS_ON to disable */
		ret = regmap_update_bits(led->chip->battery_regmap, PM880_CFD_CONFIG4,
					PM88X_CF_CFD_PLS_ON, 0);
		if (ret)
			goto error;

		if (gpio_en)
			/* high active */
			ret = regmap_update_bits(led->chip->battery_regmap, PM880_CFD_CONFIG3,
						 (PM88X_CF_EN_HIGH | PM88X_CFD_ENABLED),
						 (PM88X_CF_EN_HIGH | PM88X_CFD_ENABLED));
		else
			ret = regmap_update_bits(led->chip->battery_regmap, PM880_CFD_CONFIG3,
						(PM88X_CF_EN_HIGH | PM88X_CFD_ENABLED),
						PM88X_CFD_MASKED);
	}
	if (ret) {
		dev_err(dev, "gpio configure fail: ret = %d\n", ret);
		goto error;
	}

	mutex_unlock(&led->lock);

	pm88x_led_bright_set(&led->cdev, 0);

	return size;
error:
	mutex_unlock(&led->lock);
	return size;
}

static DEVICE_ATTR(gpio_ctrl, S_IRUGO | S_IWUSR, gpio_ctrl_show, gpio_ctrl_store);

int get_flash_duration(unsigned int *duration)
{
	if (!duration)
		return -EINVAL;
	*duration = delay_flash_timer;

	return 0;
}
EXPORT_SYMBOL(get_flash_duration);

int set_torch_current(unsigned int cur)
{
	int old_cur;
	int min_current_step, max_current_step, max_current;

	if (cur <= 0)
		return -EINVAL;

	if (!g_led)
		return -EINVAL;

	if (strcmp(g_led->name, "torch"))
		return -EINVAL;

	mutex_lock(&g_led->lock);
	min_current_step = g_led->iset_step;
	max_current_step = g_led->iset_step * PM88X_NUM_TORCH_CURRENT_LEVELS;
	if (g_led->conn_cfout_ab) {
		min_current_step *= 2;
		max_current_step *= 2;
	}

	/* if input current is too small, just return error */
	if (cur < min_current_step) {
		pr_info("%s: current (%d) is too small\n", __func__, cur);
		mutex_unlock(&g_led->lock);
		return -EINVAL;
	}

	old_cur = ((LED_FULL / g_led->max_current_div) + 1) * g_led->iset_step;
	if (g_led->conn_cfout_ab)
		old_cur *= 2;
	max_current = (cur > max_current_step) ? max_current_step : cur;

	if (g_led->conn_cfout_ab)
		g_led->max_current_div = PM88X_CURRENT_DIV(max_current, g_led->iset_step * 2);
	else
		g_led->max_current_div = PM88X_CURRENT_DIV(max_current, g_led->iset_step);

	mutex_unlock(&g_led->lock);
	return old_cur;
}
EXPORT_SYMBOL(set_torch_current);

/* convert dts voltage value to register value, values in msec */
static void convert_flash_timer_val_to_reg(struct pm88x_led_pdata *pdata)
{
	/* in milliseconds */
	int flash_timer_values[] = { 16, 23, 31, 39, 47, 57, 62, 70, 125, 187, 250, 312, 375, 437,
					500, 1000, 1500, 2000, 2500, 3000, 3496, 4000, 4520};
	int num_of_flash_timer_values = ARRAY_SIZE(flash_timer_values);
	int i;

	for (i = 0; i < num_of_flash_timer_values; i++) {
		if (pdata->flash_timer <= flash_timer_values[i]) {
			pdata->delay_flash_timer = flash_timer_values[i];
			pdata->flash_timer = i;
			return;
		}
	}

	pdata->flash_timer = num_of_flash_timer_values - 1;
	pdata->delay_flash_timer = flash_timer_values[num_of_flash_timer_values - 1];
}

/* convert dts voltage value to register value, values in mv */
static void convert_cls_ov_set_val_to_reg_pm886(unsigned int *cls_ov_set)
{
	switch (*cls_ov_set) {
	case 0 ... 3500:
		*cls_ov_set = 0x2;
		break;
	case 3501 ... 4000:
		*cls_ov_set = 0x1;
		break;
	case 4001 ... 4400:
		*cls_ov_set = 0x0;
		break;
	case 4401 ... 4600:
		*cls_ov_set = 0x3;
		break;
	default:
		*cls_ov_set = 0x3;
	}
}
/* convert dts voltage value to register value, values in mv */
static void convert_cls_ov_set_val_to_reg_pm880(unsigned int *cls_ov_set)
{
	switch (*cls_ov_set) {
	case 0 ... 4000:
		*cls_ov_set = 0x1;
		break;
	case 4001 ... 4500:
		*cls_ov_set = 0x2;
		break;
	case 4501 ... 5000:
		*cls_ov_set = 0x0;
		break;
	default:
		*cls_ov_set = 0x0;
	}
}

/* convert dts voltage value to register value, values in mv */
static void convert_cls_uv_set_val_to_reg_pm886(unsigned int *cls_uv_set)
{
	switch (*cls_uv_set) {
	case 0 ... 1500:
		*cls_uv_set = 0x0;
		break;
	case 1501 ... 2000:
		*cls_uv_set = 0x1;
		break;
	case 2001 ... 2200:
		*cls_uv_set = 0x2;
		break;
	case 2201 ... 2400:
		*cls_uv_set = 0x3;
		break;
	default:
		*cls_uv_set = 0x3;
	}
}
/* convert dts voltage value to register value, values in mv */
static void convert_cls_uv_set_val_to_reg_pm880(unsigned int *cls_uv_set)
{
	if (*cls_uv_set <= 1200)
		*cls_uv_set = 0x0;
	else
		*cls_uv_set = 0x1;
}

/* convert dts voltage value to register value, values in mv */
static void convert_cfd_bst_vset_val_to_reg(unsigned int *cfd_bst_vset)
{
	if (*cfd_bst_vset <= CFD_MIN_BOOST_VOUT)
		*cfd_bst_vset = 0x0;
	else if (*cfd_bst_vset >= CFD_MAX_BOOST_VOUT)
		*cfd_bst_vset = 0x7;
	else
		*cfd_bst_vset = (*cfd_bst_vset - CFD_MIN_BOOST_VOUT) / CFD_VOUT_GAP_LEVEL;
}

/* convert dts voltage value to register value, values in mv */
static void convert_cfd_bst_uvvbat_set_val_to_reg_pm886(unsigned int *bst_uvvbat_set)
{
	switch (*bst_uvvbat_set) {
	case 0 ... 3100:
		*bst_uvvbat_set = 0x0;
		break;
	case 3101 ... 3200:
		*bst_uvvbat_set = 0x1;
		break;
	case 3201 ... 3300:
		*bst_uvvbat_set = 0x2;
		break;
	case 3301 ... 3400:
		*bst_uvvbat_set = 0x3;
		break;
	default:
		*bst_uvvbat_set = 0x3;
	}
}
/* convert dts voltage value to register value, values in mv */
static void convert_cfd_bst_uvvbat_set_val_to_reg_pm880(unsigned int *bst_uvvbat_set)
{
	switch (*bst_uvvbat_set) {
	case 0 ... 3200:
		*bst_uvvbat_set = 0x0;
		break;
	case 3201 ... 3300:
		*bst_uvvbat_set = 0x1;
		break;
	case 3301 ... 3400:
		*bst_uvvbat_set = 0x2;
		break;
	case 3401 ... 3500:
		*bst_uvvbat_set = 0x3;
		break;
	default:
		*bst_uvvbat_set = 0x3;
	}
}

static int pm88x_led_dt_init(struct device_node *np,
			 struct device *dev,
			 struct pm88x_led_pdata *pdata,
			 struct pm88x_chip *chip)
{
	int ret;

	ret = of_property_read_u32(np,
				   "gpio-en", &pdata->gpio_en);
	if (ret)
		return ret;
	ret = of_property_read_u32(np,
				   "flash-en-gpio", &pdata->cf_en);
	if (ret)
		return ret;
#ifdef CONFIG_MACH_J7MLTE
	ret = of_property_read_u32(np,
				   "pa-en-gpio", &pa_en);
	if (ret)
		return ret;
#endif
	
	ret = of_property_read_u32(np,
				   "flash-txmsk-gpio", &pdata->cf_txmsk);
	if (ret)
		return ret;
	ret = of_property_read_u32(np,
				   "flash-timer", &pdata->flash_timer);
	if (ret)
		return ret;
	convert_flash_timer_val_to_reg(pdata);

	ret = of_property_read_u32(np,
				   "cls-ov-set", &pdata->cls_ov_set);
	if (ret)
		return ret;
	if (chip->type == PM886)
		convert_cls_ov_set_val_to_reg_pm886(&pdata->cls_ov_set);
	else
		convert_cls_ov_set_val_to_reg_pm880(&pdata->cls_ov_set);

	ret = of_property_read_u32(np,
				   "cls-uv-set", &pdata->cls_uv_set);
	if (ret)
		return ret;
	if (chip->type == PM886)
		convert_cls_uv_set_val_to_reg_pm886(&pdata->cls_uv_set);
	else
		convert_cls_uv_set_val_to_reg_pm880(&pdata->cls_uv_set);

	ret = of_property_read_u32(np,
				   "cfd-bst-vset", &pdata->cfd_bst_vset);
	if (ret)
		return ret;

	convert_cfd_bst_vset_val_to_reg(&pdata->cfd_bst_vset);

	ret = of_property_read_u32(np,
				   "bst-uvvbat-set", &pdata->bst_uvvbat_set);
	if (ret)
		return ret;

	if (chip->type == PM886)
		convert_cfd_bst_uvvbat_set_val_to_reg_pm886(&pdata->bst_uvvbat_set);
	else
		convert_cfd_bst_uvvbat_set_val_to_reg_pm880(&pdata->bst_uvvbat_set);

	ret = of_property_read_u32(np,
				   "max-flash-current", &pdata->max_flash_current);
	if (ret) {
		pdata->max_flash_current = PM88X_MAX_SAFE_FLASH_CURRENT;
		dev_err(dev,
			"max-flash-current is not define in DTS, using default value\n");
	}

	ret = of_property_read_u32(np, "max-torch-current", &pdata->max_torch_current);
	if (ret) {
		pdata->max_torch_current = PM88X_MAX_SAFE_TORCH_CURRENT;
		dev_err(dev,
			"max-torch-current is not define in DTS, using default value\n");
	}

	ret = of_property_read_u32(np,
				"torch-force-max-current", &pdata->torch_force_max_current);
	if (ret)
		return ret;

	pdata->conn_cfout_ab = of_property_read_bool(np, "conn-cfout-ab");

	return 0;
}
static int pm88x_setup(struct platform_device *pdev, struct pm88x_led_pdata *pdata)
{
	struct pm88x_led *led = platform_get_drvdata(pdev);
	struct pm88x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	int ret;

	if (gpio_en) {
		/* high active */
		if (chip->type == PM886)
			ret = regmap_update_bits(chip->battery_regmap, PM886_CFD_CONFIG3,
					(PM88X_CF_EN_HIGH | PM88X_CFD_ENABLED),
					(PM88X_CF_EN_HIGH | PM88X_CFD_ENABLED));
		else
			ret = regmap_update_bits(chip->battery_regmap, PM880_CFD_CONFIG3,
						(PM88X_CF_EN_HIGH | PM88X_CFD_ENABLED),
						(PM88X_CF_EN_HIGH | PM88X_CFD_ENABLED));
		if (ret)
			return ret;
		gpio_direction_output(led->cf_en, 0);

		ret = devm_gpio_request(&pdev->dev, led->cf_txmsk,
					"cf_txmsk-gpio");
		if (ret) {
			dev_err(&pdev->dev,
				"request cf_txmsk-gpio error!\n");
			return ret;
		}
		gpio_direction_output(led->cf_txmsk, 0);
		devm_gpio_free(&pdev->dev, led->cf_txmsk);

	}

	if (chip->type == PM886) {
		/* set FLASH_TIMER as configured in DT */
		ret = regmap_update_bits(chip->battery_regmap, PM886_CFD_CONFIG2,
				PM88X_FLASH_TIMER_MASK,
				(pdata->flash_timer << PM88X_FLASH_TIMER_OFFSET));
		if (ret)
			return ret;
	} else {
		/* set FLASH_TIMER as configured in DT */
		ret = regmap_update_bits(chip->battery_regmap, PM880_CFD_CONFIG2,
				PM88X_FLASH_TIMER_MASK,
				(pdata->flash_timer << PM88X_FLASH_TIMER_OFFSET));
		if (ret)
			return ret;
	}
	ret = regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG3,
			PM88X_BST_TON_SET, PM88X_BST_TON_SET);
	if (ret)
		return ret;
	/* set booster high-side to 3A and low-side to 4A */
	ret = regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG4,
			PM88X_BST_HSOC_MASK, PM88X_BST_HSOC_SET_3A);
	if (ret)
		return ret;
	ret = regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG4,
			PM88X_BST_LSOC_MASK, PM88X_BST_LSOC_SET_4A);
	if (ret)
		return ret;

	/* set high-side zero current limit to 130mA */
	ret = regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG5,
			PM88X_BST_HSZC_MASK, 0);
	if (ret)
		return ret;

	if (chip->type == PM886) {
		/* set over & under voltage protection as configured in DT */
		ret = regmap_update_bits(chip->battery_regmap, PM88X_CLS_CONFIG1,
				(PM886_OV_SET_MASK | PM886_UV_SET_MASK),
				((pdata->cls_ov_set << PM886_OV_SET_OFFSET) |
				(pdata->cls_uv_set << PM886_UV_SET_OFFSET)));
		/* set booster voltage as configured in DT */
		ret = regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG1,
				PM88X_BST_VSET_MASK, (pdata->cfd_bst_vset << PM88X_BST_VSET_OFFSET));
		if (ret)
				return ret;
	} else {
		/* set over & under voltage protection as configured in DT */
		ret = regmap_update_bits(chip->battery_regmap, PM88X_CLS_CONFIG1,
			(PM880_OV_SET_MASK | PM880_UV_SET_MASK),
			((pdata->cls_ov_set << PM880_OV_SET_OFFSET) |
			(pdata->cls_uv_set << PM880_UV_SET_OFFSET)));
		if (ret)
			return ret;
		/* set booster voltage as configured in DT */
		ret = regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG1,
				PM880_BST_VSET_MASK, (pdata->cfd_bst_vset << PM88X_BST_VSET_OFFSET));
		if (ret)
				return ret;
	}
	/* set VBAT TH as configured in DT */
	ret = regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG2,
			PM88X_BST_UVVBAT_MASK, (pdata->bst_uvvbat_set << PM88X_BST_UVVBAT_OFFSET));
	if (ret)
		return ret;

	switch (chip->type) {
	case PM886:
		/* if A0, disable UVVBAT due to comparator faults in silicon */
		if (chip->chip_id == PM886_A0) {
			ret = regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG2,
						PM886_BST_UVVBAT_EN_MASK, 0);
				if (ret)
					return ret;
		}
		/* set boost mode to voltage mode, cofirmed with DE */
		regmap_update_bits(chip->battery_regmap, PM886_CFD_CONFIG3,
				PM886_BOOST_MODE, PM886_BOOST_MODE);
		break;
	case PM880:
		/* set CLS_NOK debounce period setting to 20us */
		regmap_update_bits(chip->battery_regmap, PM88X_BST_CONFIG2,
				PM880_CLK_NOK_OC_DB, PM880_CLK_NOK_OC_DB);
		/* if A0, disable over current comparators */
		if (chip->chip_id == PM880_A0) {
			ret = regmap_update_bits(chip->battery_regmap, PM880_CLS_CONFIG2,
						PM880_CLS_OC_EN_MSK, 0);
				if (ret)
					return ret;
		}
		break;
	default:
		break;
	}

	/* set CFD_CLK_EN to enable */
	ret = regmap_update_bits(chip->base_regmap, PM88X_CLK_CTRL1,
			PM88X_CFD_CLK_EN, PM88X_CFD_CLK_EN);
	if (ret)
		return ret;
	else
		return 0;
}

#define WIDGET_BRIGHTNESS	127	//Torch mode
static ssize_t rear_flash_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct pm88x_led *led;
	struct led_classdev *cdev;
	int ret;
	unsigned int data;
	int en_value = 0;

	cdev = dev_get_drvdata(dev);
	led = container_of(cdev, struct pm88x_led, cdev);
	ret = sscanf(buf, "%d", &en_value);
	if (ret < 0) {
		dev_dbg(dev, "%s: get gpio_en error, set to 0 as default\n", __func__);
		en_value = 0;
	}
	dev_info(dev, "%s: led control %s\n",
			__func__, (en_value ? "enabled" : "disabled"));
	mutex_lock(&led->lock);
	led->cf_en = en_value;
	//led->id = PM88X_TORCH_LED;
	led->brightness = 127;
	if(en_value)
		ledtrig_torch_ctrl(en_value);
	else
		ledtrig_torch_ctrl(en_value);
	mutex_unlock(&led->lock);
	return size;
}

ssize_t rear_flash_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pm88x_led *led;
	struct led_classdev *cdev;

	int s = 0;
	cdev = dev_get_drvdata(dev);
	led = container_of(cdev, struct pm88x_led, cdev);

	s += sprintf(buf, "gpio control: %d\n", gpio_en);
	return s;
}
static DEVICE_ATTR(rear_flash, S_IRUGO | S_IXOTH,  rear_flash_show, rear_flash_store);
extern int create_front_flash_sysfs(void);
static int pm88x_led_probe(struct platform_device *pdev)
{
	struct pm88x_led *led;
	struct pm88x_led_pdata *pdata = pdev->dev.platform_data;
	struct pm88x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct device_node *node = pdev->dev.of_node;
	static int init_flag = 0;
	unsigned int max_current;
	int ret;
	int min_current_step, max_current_step;

	led = devm_kzalloc(&pdev->dev, sizeof(struct pm88x_led),
			    GFP_KERNEL);
	if (led == NULL)
		return -ENOMEM;

	if (IS_ENABLED(CONFIG_OF)) {
		if (!pdata) {
			pdata = devm_kzalloc(&pdev->dev,
					     sizeof(*pdata), GFP_KERNEL);
			if (!pdata)
				return -ENOMEM;
		}
		ret = pm88x_led_dt_init(node, &pdev->dev, pdata, chip);
		if (ret)
			return ret;
	} else if (!pdata) {
		return -EINVAL;
	}

	switch (chip->type) {
	case PM886:
	led->iset_step = PM886_CURRENT_LEVEL_STEPS;
	led->conn_cfout_ab = false;
		break;
	case PM880:
	led->iset_step = PM880_CURRENT_LEVEL_STEPS;
	led->conn_cfout_ab = pdata->conn_cfout_ab;
		break;
	default:
		dev_err(&pdev->dev, "Unsupported chip type %ld\n", chip->type);
		return -EINVAL;
	}

	switch (pdev->id) {
	case PM88X_FLASH_LED:
		strncpy(led->name, "flash", MFD_NAME_SIZE - 1);
		min_current_step = led->iset_step;
		max_current_step = led->iset_step * PM88X_NUM_FLASH_CURRENT_LEVELS;
		if (led->conn_cfout_ab) {
			min_current_step *= 2;
			max_current_step *= 2;
		}
		pdata->max_flash_current = (pdata->max_flash_current < min_current_step) ?
				min_current_step : pdata->max_flash_current;
		max_current = (pdata->max_flash_current > max_current_step) ?
				max_current_step : pdata->max_flash_current;
		if (led->conn_cfout_ab)
			led->max_current_div = PM88X_CURRENT_DIV(max_current, led->iset_step * 2);
		else
			led->max_current_div = PM88X_CURRENT_DIV(max_current, led->iset_step);
		break;
	case PM88X_TORCH_LED:
		strncpy(led->name, "torch", MFD_NAME_SIZE - 1);
		min_current_step = led->iset_step;
		max_current_step = led->iset_step * PM88X_NUM_TORCH_CURRENT_LEVELS;
		if (led->conn_cfout_ab) {
			min_current_step *= 2;
			max_current_step *= 2;
		}
		pdata->max_torch_current = (pdata->max_torch_current < min_current_step) ?
				min_current_step : pdata->max_torch_current;
		max_current = (pdata->max_torch_current > max_current_step) ?
				max_current_step : pdata->max_torch_current;
		if (led->conn_cfout_ab)
			led->max_current_div = PM88X_CURRENT_DIV(max_current, led->iset_step * 2);
		else
			led->max_current_div = PM88X_CURRENT_DIV(max_current, led->iset_step);
		/*
		 * Allow to force a constant current regardless of upper
		 * layer request
		 */
		led->force_max_current = pdata->torch_force_max_current;
		wake_lock_init(&led->wake_lock, WAKE_LOCK_SUSPEND, "torch_wl");
		break;
	case PM88X_NO_LED:
	default:
		dev_err(&pdev->dev, "Invalid LED id 0x%x\n", pdev->id);
		return -EINVAL;
	}

	led->chip = chip;
	led->id = pdev->id;
	led->cf_en = pdata->cf_en;
	led->cf_txmsk = pdata->cf_txmsk;
	gpio_en = pdata->gpio_en;
	led->cfd_bst_vset = pdata->cfd_bst_vset;
	delay_flash_timer = pdata->delay_flash_timer;

	led->current_brightness = 0;
	led->cdev.name = led->name;
	led->cdev.brightness_set = pm88x_led_bright_set;
	/* for camera trigger */
	led->cdev.default_trigger = led->name;

	dev_set_drvdata(&pdev->dev, led);

	mutex_init(&led->lock);

	mutex_lock(&led->lock);
	if (!dev_num) {
#ifndef CONFIG_MACH_J7MLTE
		if(gpio_en)
#endif
			{
			ret = gpio_request(led->cf_en, "cf-gpio");
			if (ret) {
				dev_err(&pdev->dev, "request cf-gpio error!\n");
				return ret;
			}
		}
#ifdef CONFIG_MACH_J7MLTE
		gpio_direction_output(led->cf_en,0);
		ret = gpio_request(pa_en, "pa-en-gpio");
		if (ret) {
			dev_err(&pdev->dev, "request pa-en-gpio error!\n");
			return ret;
		}
		gpio_direction_output(pa_en,0);	
#endif
		ret = pm88x_setup(pdev, pdata);
		if (ret < 0) {
			dev_err(&pdev->dev, "device setup failed: %d\n", ret);
			return ret;
		}
	}
	dev_num++;
	mutex_unlock(&led->lock);

	led->led_wqueue = create_singlethread_workqueue("88pm88x-led-wq");
	if (!led->led_wqueue) {
		dev_err(&pdev->dev, "%s: failed to create wq.\n", __func__);
		return -ESRCH;
	}

	INIT_WORK(&led->work, pm88x_led_work);
	INIT_WORK(&led->led_flash_off, pm88x_flash_off);
	INIT_DELAYED_WORK(&led->delayed_work, pm88x_led_delayed_work);


	ret = led_classdev_register(&pdev->dev, &led->cdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register LED: %d\n", ret);
		return ret;
	}
	pm88x_led_bright_set(&led->cdev, 0);

	ret = device_create_file(led->cdev.dev, &dev_attr_gpio_ctrl);
	if (ret < 0) {
		dev_err(&pdev->dev, "device attr create fail: %d\n", ret);
		return ret;
	}

	g_led = led;

	/*++ added for camera flash sysfs ++*/
	if(init_flag == 0) {
		init_flag = 1;
		if(camera_class == NULL)
			camera_class = class_create(THIS_MODULE, "camera");

		if (IS_ERR(camera_class))
			printk("Failed to create camera_class!\n");
		if(flash_dev == NULL)
			flash_dev = device_create(camera_class, NULL, 0, "%s", "flash");
		dev_set_drvdata(flash_dev, led);

		if (IS_ERR(flash_dev)) {
			pr_err("flash_sysfs: failed to create device(flash)\n");
			return -ENODEV;
		}

		if (device_create_file(flash_dev, &dev_attr_rear_flash) < 0)
			printk("Failed to create device file(%s)!\n", dev_attr_rear_flash.attr.name);
		/*-- camera flash sysfs --*/
#if defined(CONFIG_MACH_J7MLTE)
		create_front_flash_sysfs();
#endif
	}
	return 0;
}

static int pm88x_led_remove(struct platform_device *pdev)
{
	struct pm88x_led *led = platform_get_drvdata(pdev);

	mutex_lock(&led->lock);

	if (dev_num > 0)
		dev_num--;

	if ((!dev_num) && (gpio_en))
		gpio_free(led->cf_en);

	mutex_unlock(&led->lock);

	if (led)
		led_classdev_unregister(&led->cdev);
	return 0;
}

static const struct of_device_id pm88x_led_dt_match[] = {
	{ .compatible = "marvell,88pm88x-leds", },
	{ },
};
MODULE_DEVICE_TABLE(of, pm88x_led_dt_match);

static struct platform_driver pm88x_led_driver = {
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "88pm88x-leds",
		.of_match_table = of_match_ptr(pm88x_led_dt_match),
	},
	.probe	= pm88x_led_probe,
	.remove	= pm88x_led_remove,
};

module_platform_driver(pm88x_led_driver);

MODULE_DESCRIPTION("Camera flash/torch driver for Marvell 88PM88X");
MODULE_AUTHOR("Ariel Heller <arielh@marvell.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:88PM88x-leds");
