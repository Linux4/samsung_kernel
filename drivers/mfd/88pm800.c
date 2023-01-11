/*
 * Base driver for Marvell 88PM800
 *
 * Copyright (C) 2012 Marvell International Ltd.
 * Haojian Zhuang <haojian.zhuang@marvell.com>
 * Joseph(Yossi) Hanin <yhanin@marvell.com>
 * Qiao Zhou <zhouqiao@marvell.com>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file "COPYING" in the main directory of this
 * archive for more details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/switch.h>
#include <linux/mfd/core.h>
#include <linux/mfd/88pm80x.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/reboot.h>
#include "88pm8xx-config.h"
#include "88pm8xx-debugfs.h"
#ifdef CONFIG_SEC_DEBUG
#include <linux/sec-common.h>
#endif

#define PM80X_BASE_REG_NUM		0xf0
#define PM80X_POWER_REG_NUM		0x9b
#define PM80X_GPADC_REG_NUM		0xb6

/* Interrupt Registers */
#define PM800_INT_STATUS1		(0x05)
#define PM800_ONKEY_INT_STS1		(1 << 0)
#define PM800_EXTON_INT_STS1		(1 << 1)
#define PM800_CHG_INT_STS1			(1 << 2)
#define PM800_BAT_INT_STS1			(1 << 3)
#define PM800_RTC_INT_STS1			(1 << 4)
#define PM800_CLASSD_OC_INT_STS1	(1 << 5)

#define PM800_INT_STATUS2		(0x06)
#define PM800_VBAT_INT_STS2		(1 << 0)
#define PM800_VSYS_INT_STS2		(1 << 1)
#define PM800_VCHG_INT_STS2		(1 << 2)
#define PM800_TINT_INT_STS2		(1 << 3)
#define PM800_GPADC0_INT_STS2	(1 << 4)
#define PM800_TBAT_INT_STS2		(1 << 5)
#define PM800_GPADC2_INT_STS2	(1 << 6)
#define PM800_GPADC3_INT_STS2	(1 << 7)

#define PM800_INT_STATUS3		(0x07)

#define PM800_INT_STATUS4		(0x08)
#define PM800_GPIO0_INT_STS4		(1 << 0)
#define PM800_GPIO1_INT_STS4		(1 << 1)
#define PM800_GPIO2_INT_STS4		(1 << 2)
#define PM800_GPIO3_INT_STS4		(1 << 3)
#define PM800_GPIO4_INT_STS4		(1 << 4)

#define PM800_INT_ENA_1		(0x09)
#define PM800_ONKEY_INT_ENA1		(1 << 0)
#define PM800_EXTON_INT_ENA1		(1 << 1)
#define PM800_CHG_INT_ENA1			(1 << 2)
#define PM800_BAT_INT_ENA1			(1 << 3)
#define PM800_RTC_INT_ENA1			(1 << 4)
#define PM800_CLASSD_OC_INT_ENA1	(1 << 5)

#define PM800_INT_ENA_2		(0x0A)
#define PM800_VBAT_INT_ENA2		(1 << 0)
#define PM800_VSYS_INT_ENA2		(1 << 1)
#define PM800_VCHG_INT_ENA2		(1 << 2)
#define PM800_TINT_INT_ENA2		(1 << 3)
#define PM822_IRQ_BUCK_PGOOD_EN		(1 << 4)
#define PM822_IRQ_LDO_PGOOD_EN		(1 << 5)

#define PM800_INT_ENA_3		(0x0B)
#define PM800_GPADC0_INT_ENA3		(1 << 0)
#define PM800_GPADC1_INT_ENA3		(1 << 1)
#define PM800_GPADC2_INT_ENA3		(1 << 2)
#define PM800_GPADC3_INT_ENA3		(1 << 3)
#define PM800_GPADC4_INT_ENA3		(1 << 4)
#define PM822_IRQ_HS_DET_EN		(1 << 5)

#define PM800_INT_ENA_4		(0x0C)
#define PM800_GPIO0_INT_ENA4		(1 << 0)
#define PM800_GPIO1_INT_ENA4		(1 << 1)
#define PM800_GPIO2_INT_ENA4		(1 << 2)
#define PM800_GPIO3_INT_ENA4		(1 << 3)
#define PM800_GPIO4_INT_ENA4		(1 << 4)

/* number of INT_ENA & INT_STATUS regs */
#define PM800_INT_REG_NUM			(4)

/* Interrupt Number in 88PM800 */
enum {
	PM800_IRQ_ONKEY,	/*EN1b0 *//*0 */
	PM800_IRQ_EXTON,	/*EN1b1 */
	PM800_IRQ_CHG,		/*EN1b2 */
	PM800_IRQ_BAT,		/*EN1b3 */
	PM800_IRQ_RTC,		/*EN1b4 */
	PM800_IRQ_CLASSD,	/*EN1b5 *//*5 */
	PM800_IRQ_VBAT,		/*EN2b0 */
	PM800_IRQ_VSYS,		/*EN2b1 */
	PM800_IRQ_VCHG,		/*EN2b2 */
	PM800_IRQ_TINT,		/*EN2b3 */
	PM822_IRQ_BUCK_PGOOD,	/*EN2b4 *//*10 */
	PM822_IRQ_LDO_PGOOD,	/*EN2b5 */
	PM800_IRQ_GPADC0,	/*EN3b0 */
	PM800_IRQ_GPADC1,	/*EN3b1 */
	PM800_IRQ_GPADC2,	/*EN3b2 */
	PM800_IRQ_GPADC3,	/*EN3b3 *//*15 */
	PM800_IRQ_GPADC4 = 16,	/*EN3b4 */
	PM822_IRQ_MIC_DET = 16,	/*EN3b4 */
	PM822_IRQ_HS_DET = 17,	/*EN3b4 */
	PM800_IRQ_GPIO0,	/*EN4b0 */
	PM800_IRQ_GPIO1,	/*EN4b1 */
	PM800_IRQ_GPIO2,	/*EN4b2 *//*20 */
	PM800_IRQ_GPIO3,	/*EN4b3 */
	PM800_IRQ_GPIO4,	/*EN4b4 */
	PM800_MAX_IRQ,
};

/* PM800: generation identification number */
#define PM800_CHIP_GEN_ID_NUM	0x3

/*
 * globle device pointer
 * please think twice before use, will remove later
 */
struct pm80x_chip *chip_g;

enum pm8xx_parent {
	PM822 = 0x822,
	PM800 = 0x800,
};

static const struct i2c_device_id pm80x_id_table[] = {
	/* TODO: add meaningful data */
	{"88PM800", PM800},
	{"88pm822", PM822},
	{} /* NULL terminated */
};
MODULE_DEVICE_TABLE(i2c, pm80x_id_table);

static const struct of_device_id pm80x_dt_ids[] = {
	{ .compatible = "marvell,88pm800", },
	{},
};
MODULE_DEVICE_TABLE(of, pm80x_dt_ids);

static struct resource rtc_resources[] = {
	{
	 .name = "88pm80x-rtc",
	 .start = PM800_IRQ_RTC,
	 .end = PM800_IRQ_RTC,
	 .flags = IORESOURCE_IRQ,
	 },
};

static struct mfd_cell rtc_devs[] = {
	{
	 .name = "88pm80x-rtc",
	 .of_compatible = "marvell,88pm80x-rtc",
	 .num_resources = ARRAY_SIZE(rtc_resources),
	 .resources = &rtc_resources[0],
	 .id = -1,
	 },
};

static struct resource onkey_resources[] = {
	{
	 .name = "88pm80x-onkey",
	 .start = PM800_IRQ_ONKEY,
	 .end = PM800_IRQ_ONKEY,
	 .flags = IORESOURCE_IRQ,
	 },
};

static const struct mfd_cell onkey_devs[] = {
	{
	 .name = "88pm80x-onkey",
	 .of_compatible = "marvell,88pm80x-onkey",
	 .num_resources = 1,
	 .resources = &onkey_resources[0],
	 .id = -1,
	 },
};

static struct resource sw_bat_resources[] = {
	{
	.name = "88pm80x-sw-fuelgauge",
	.start = PM800_IRQ_VBAT,
	.end = PM800_IRQ_VBAT,
	.flags = IORESOURCE_IRQ,
	},
};

static struct mfd_cell sw_bat_devs[] = {
	{
	.name = "88pm80x-sw-fuelgauge",
	.of_compatible = "marvell,88pm80x-sw-battery",
	.num_resources = 1,
	.resources = &sw_bat_resources[0],
	.id = -1,
	},
};

static const struct mfd_cell regulator_devs[] = {
	{
	 .name = "88pm80x-regulator",
	 .of_compatible = "marvell,88pm80x-regulator",
	 .id = -1,
	},
};

static struct resource usb_resources[] = {
	{
	.name = "88pm80x-usb",
	.start = PM800_IRQ_CHG,
	.end = PM800_IRQ_CHG,
	.flags = IORESOURCE_IRQ,
	},
	{
	.name = "gpadc0",
	.start = PM800_IRQ_GPADC0,
	.end = PM800_IRQ_GPADC0,
	.flags = IORESOURCE_IRQ,
	},
	{
	.name = "gpadc1",
	.start = PM800_IRQ_GPADC1,
	.end = PM800_IRQ_GPADC1,
	.flags = IORESOURCE_IRQ,
	},
	{
	.name = "gpadc2",
	.start = PM800_IRQ_GPADC2,
	.end = PM800_IRQ_GPADC2,
	.flags = IORESOURCE_IRQ,
	},
};

static struct mfd_cell usb_devs[] = {
	{
	.name = "88pm80x-usb",
	.of_compatible = "marvell,88pm80x-usb",
	.num_resources = ARRAY_SIZE(usb_resources),
	.resources = &usb_resources[0],
	.id = -1,
	},
};

static struct mfd_cell vibrator_devs[] = {
	{
	 .name = "88pm80x-vibrator",
	 .of_compatible = "marvell,88pm80x-vibrator",
	 .id = -1,
	},
};

static struct resource headset_resources_800[] = {
	{
		.name = "gpio-03",
		.start = PM800_IRQ_GPIO3,
		.end = PM800_IRQ_GPIO3,
		.flags = IORESOURCE_IRQ,
	},
	{
		.name = "gpadc4",
		.start = PM800_IRQ_GPADC4,
		.end = PM800_IRQ_GPADC4,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource headset_resources_822[] = {
	{
		.name = "88pm822-headset",
		.start = PM822_IRQ_HS_DET,
		.end = PM822_IRQ_HS_DET,
		.flags = IORESOURCE_IRQ,
	},
	{
		.name = "gpadc4",
		.start = PM800_IRQ_GPADC4,
		.end = PM800_IRQ_GPADC4,
		.flags = IORESOURCE_IRQ,
	},
};

static struct mfd_cell headset_devs_800[] = {
	{
	 .name = "88pm800-headset",
	 .of_compatible = "marvell,88pm80x-headset",
	 .num_resources = ARRAY_SIZE(headset_resources_800),
	 .resources = &headset_resources_800[0],
	 .id = -1,
	 },
};

static struct mfd_cell dvc_devs[] = {
	{
	 .name = "88pm8xx-dvc",
	 .of_compatible = "marvell,88pm8xx-dvc",
	 .id = -1,
	},
};

static const struct regmap_irq pm800_irqs[] = {
	/* INT0 */
	[PM800_IRQ_ONKEY] = {
		.mask = PM800_ONKEY_INT_ENA1,
	},
	[PM800_IRQ_EXTON] = {
		.mask = PM800_EXTON_INT_ENA1,
	},
	[PM800_IRQ_CHG] = {
		.mask = PM800_CHG_INT_ENA1,
	},
	[PM800_IRQ_BAT] = {
		.mask = PM800_BAT_INT_ENA1,
	},
	[PM800_IRQ_RTC] = {
		.mask = PM800_RTC_INT_ENA1,
	},
	[PM800_IRQ_CLASSD] = {
		.mask = PM800_CLASSD_OC_INT_ENA1,
	},
	/* INT1 */
	[PM800_IRQ_VBAT] = {
		.reg_offset = 1,
		.mask = PM800_VBAT_INT_ENA2,
	},
	[PM800_IRQ_VSYS] = {
		.reg_offset = 1,
		.mask = PM800_VSYS_INT_ENA2,
	},
	[PM800_IRQ_VCHG] = {
		.reg_offset = 1,
		.mask = PM800_VCHG_INT_ENA2,
	},
	[PM800_IRQ_TINT] = {
		.reg_offset = 1,
		.mask = PM800_TINT_INT_ENA2,
	},
	[PM822_IRQ_LDO_PGOOD] = {
		.reg_offset = 1,
		.mask = PM822_IRQ_LDO_PGOOD_EN,
	},
	[PM822_IRQ_BUCK_PGOOD] = {
		.reg_offset = 1,
		.mask = PM822_IRQ_BUCK_PGOOD_EN,
	},
	/* INT2 */
	[PM800_IRQ_GPADC0] = {
		.reg_offset = 2,
		.mask = PM800_GPADC0_INT_ENA3,
	},
	[PM800_IRQ_GPADC1] = {
		.reg_offset = 2,
		.mask = PM800_GPADC1_INT_ENA3,
	},
	[PM800_IRQ_GPADC2] = {
		.reg_offset = 2,
		.mask = PM800_GPADC2_INT_ENA3,
	},
	[PM800_IRQ_GPADC3] = {
		.reg_offset = 2,
		.mask = PM800_GPADC3_INT_ENA3,
	},
	[PM800_IRQ_GPADC4] = {
		.reg_offset = 2,
		.mask = PM800_GPADC4_INT_ENA3,
	},
	[PM822_IRQ_HS_DET] = {
		.reg_offset = 2,
		.mask = PM822_IRQ_HS_DET_EN,
	},
	/* INT3 */
	[PM800_IRQ_GPIO0] = {
		.reg_offset = 3,
		.mask = PM800_GPIO0_INT_ENA4,
	},
	[PM800_IRQ_GPIO1] = {
		.reg_offset = 3,
		.mask = PM800_GPIO1_INT_ENA4,
	},
	[PM800_IRQ_GPIO2] = {
		.reg_offset = 3,
		.mask = PM800_GPIO2_INT_ENA4,
	},
	[PM800_IRQ_GPIO3] = {
		.reg_offset = 3,
		.mask = PM800_GPIO3_INT_ENA4,
	},
	[PM800_IRQ_GPIO4] = {
		.reg_offset = 3,
		.mask = PM800_GPIO4_INT_ENA4,
	},
};

#if defined(CONFIG_SEC_DEBUG)
static u8 power_on_reason;
unsigned char pm80x_get_power_on_reason(void)
{
	return power_on_reason;
}
#endif

static int device_gpadc_init(struct pm80x_chip *chip,
				       struct pm80x_platform_data *pdata)
{
	struct pm80x_subchip *subchip = chip->subchip;
	struct regmap *map = subchip->regmap_gpadc;
	int data = 0, mask = 0, ret = 0;

	if (!map) {
		dev_warn(chip->dev,
			 "Warning: gpadc regmap is not available!\n");
		return -EINVAL;
	}
	/*
	 * initialize GPADC without activating it turn on GPADC
	 * measurments
	 */
	ret = regmap_update_bits(map,
				 PM800_GPADC_MISC_CONFIG2,
				 PM800_GPADC_MISC_GPFSM_EN,
				 PM800_GPADC_MISC_GPFSM_EN);
	if (ret < 0)
		goto out;
	/*
	 * This function configures the ADC as requires for
	 * CP implementation.CP does not "own" the ADC configuration
	 * registers and relies on AP.
	 * Reason: enable automatic ADC measurements needed
	 * for CP to get VBAT and RF temperature readings.
	 */
	ret = regmap_update_bits(map, PM800_GPADC_MEAS_EN1,
				 PM800_MEAS_EN1_VBAT, PM800_MEAS_EN1_VBAT);
	if (ret < 0)
		goto out;
	ret = regmap_update_bits(map, PM800_GPADC_MEAS_EN2,
				 (PM800_MEAS_EN2_RFTMP | PM800_MEAS_GP0_EN),
				 (PM800_MEAS_EN2_RFTMP | PM800_MEAS_GP0_EN));
	if (ret < 0)
		goto out;

	/*
	 * the defult of PM800 is GPADC operates at 100Ks/s rate
	 * and Number of GPADC slots with active current bias prior
	 * to GPADC sampling = 1 slot for all GPADCs set for
	 * Temprature mesurmants
	 */
	mask = (PM800_GPADC_GP_BIAS_EN0 | PM800_GPADC_GP_BIAS_EN1 |
		PM800_GPADC_GP_BIAS_EN2 | PM800_GPADC_GP_BIAS_EN3);

	if (pdata && (pdata->batt_det == 0))
		data = (PM800_GPADC_GP_BIAS_EN0 | PM800_GPADC_GP_BIAS_EN1 |
			PM800_GPADC_GP_BIAS_EN2 | PM800_GPADC_GP_BIAS_EN3);
	else
		data = (PM800_GPADC_GP_BIAS_EN0 | PM800_GPADC_GP_BIAS_EN2 |
			PM800_GPADC_GP_BIAS_EN3);

	ret = regmap_update_bits(map, PM800_GP_BIAS_ENA1, mask, data);
	if (ret < 0)
		goto out;

	dev_info(chip->dev, "pm800 device_gpadc_init: Done\n");
	return 0;

out:
	dev_info(chip->dev, "pm800 device_gpadc_init: Failed!\n");
	return ret;
}

static int device_onkey_init(struct pm80x_chip *chip,
				struct pm80x_platform_data *pdata)
{
	int ret;

	ret = mfd_add_devices(chip->dev, 0, &onkey_devs[0],
			      ARRAY_SIZE(onkey_devs), &onkey_resources[0], 0,
			      NULL);
	if (ret) {
		dev_err(chip->dev, "Failed to add onkey subdev\n");
		return ret;
	}

	return 0;
}

static int device_rtc_init(struct pm80x_chip *chip,
				struct pm80x_platform_data *pdata)
{
	int ret;

	if (pdata) {
		rtc_devs[0].platform_data = pdata->rtc;
		rtc_devs[0].pdata_size =
				pdata->rtc ? sizeof(struct pm80x_rtc_pdata) : 0;
	}
	ret = mfd_add_devices(chip->dev, 0, &rtc_devs[0],
			      ARRAY_SIZE(rtc_devs), NULL, 0, NULL);
	if (ret) {
		dev_err(chip->dev, "Failed to add rtc subdev\n");
		return ret;
	}

	return 0;
}

static int device_sw_battery_init(struct pm80x_chip *chip,
					   struct pm80x_platform_data *pdata)
{
	int ret;

	ret = mfd_add_devices(chip->dev, 0, &sw_bat_devs[0],
			      ARRAY_SIZE(sw_bat_devs), NULL, 0, NULL);
	if (ret) {
		dev_err(chip->dev, "Failed to add sw fuelgauge subdev\n");
		return ret;
	}

	return 0;
}

static int device_regulator_init(struct pm80x_chip *chip,
					   struct pm80x_platform_data *pdata)
{
	int ret;

	ret = mfd_add_devices(chip->dev, 0, &regulator_devs[0],
			      ARRAY_SIZE(regulator_devs), NULL, 0, NULL);
	if (ret) {
		dev_err(chip->dev, "Failed to add regulator subdev\n");
		return ret;
	}

	return 0;
}

static int device_vbus_init(struct pm80x_chip *chip,
			                   struct pm80x_platform_data *pdata)
{
	int ret;

	usb_devs[0].platform_data = pdata->usb;
	usb_devs[0].pdata_size = sizeof(struct pm80x_vbus_pdata);
	ret = mfd_add_devices(chip->dev, 0, &usb_devs[0],
			      ARRAY_SIZE(usb_devs), NULL, 0, NULL);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to add usb subdev\n");
		return ret;
	}

	return 0;
}

static int device_vibrator_init(struct pm80x_chip *chip,
				struct pm80x_platform_data *pdata)
{
	int ret;

	vibrator_devs[0].platform_data = pdata->vibrator;
	vibrator_devs[0].pdata_size = sizeof(struct pm80x_vibrator_pdata);

	ret = mfd_add_devices(chip->dev, 0, &vibrator_devs[0],
		ARRAY_SIZE(vibrator_devs), NULL,
		regmap_irq_chip_get_base(chip->irq_data), NULL);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to add vibrator subdev\n");
		return ret;
	}

	return 0;
}


static int device_headset_init(struct pm80x_chip *chip,
					   struct pm80x_platform_data *pdata)
{
	int ret;

	switch (chip->type) {
	case CHIP_PM800:
		headset_devs_800[0].resources =
			&headset_resources_800[0];
		headset_devs_800[0].num_resources =
			ARRAY_SIZE(headset_resources_800);
		break;
	case CHIP_PM822:
	case CHIP_PM86X:
		headset_devs_800[0].resources =
			&headset_resources_822[0];
		headset_devs_800[0].num_resources =
			ARRAY_SIZE(headset_resources_822);
		break;
	default:
		return -EINVAL;
	}

	headset_devs_800[0].platform_data = pdata->headset;
	ret = mfd_add_devices(chip->dev, 0, &headset_devs_800[0],
			      ARRAY_SIZE(headset_devs_800), NULL, 0, NULL);

	if (ret < 0) {
		dev_err(chip->dev, "Failed to add headset subdev\n");
		return ret;
	}

	return 0;
}

static int device_dvc_init(struct pm80x_chip *chip,
					   struct pm80x_platform_data *pdata)
{
	int ret;

	ret = mfd_add_devices(chip->dev, 0, &dvc_devs[0],
			      ARRAY_SIZE(dvc_devs), NULL,
			      regmap_irq_chip_get_base(chip->irq_data), NULL);
	if (ret) {
		dev_err(chip->dev, "Failed to add dvc subdev\n");
		return ret;
	}

	return 0;
}

static int device_irq_init_800(struct pm80x_chip *chip)
{
	struct regmap *map = chip->regmap;
	unsigned long flags = IRQF_ONESHOT;
	int data, mask, ret = -EINVAL;

	if (!map || !chip->irq) {
		dev_err(chip->dev, "incorrect parameters\n");
		return -EINVAL;
	}

	/*
	 * irq_mode defines the way of clearing interrupt. it's read-clear by
	 * default.
	 */
	mask =
	    PM800_WAKEUP2_INV_INT | PM800_WAKEUP2_INT_CLEAR |
	    PM800_WAKEUP2_INT_MASK;

	data = (chip->irq_mode) ? PM800_WAKEUP2_INT_WC : PM800_WAKEUP2_INT_RC;
	ret = regmap_update_bits(map, PM800_WAKEUP2, mask, data);

	if (ret < 0)
		goto out;

	ret =
	    regmap_add_irq_chip(chip->regmap, chip->irq, flags, -1,
				chip->regmap_irq_chip, &chip->irq_data);

out:
	return ret;
}

static void device_irq_exit_800(struct pm80x_chip *chip)
{
	regmap_del_irq_chip(chip->irq, chip->irq_data);
}

static struct regmap_irq_chip pm800_irq_chip = {
	.name = "88pm800",
	.irqs = pm800_irqs,
	.num_irqs = ARRAY_SIZE(pm800_irqs),

	.num_regs = 4,
	.status_base = PM800_INT_STATUS1,
	.mask_base = PM800_INT_ENA_1,
	.ack_base = PM800_INT_STATUS1,
	.mask_invert = 1,
};

static int pm800_pages_init(struct pm80x_chip *chip)
{
	struct pm80x_subchip *subchip;
	struct i2c_client *client = chip->client;

	int ret = 0;

	subchip = chip->subchip;
	if (!subchip || !subchip->power_page_addr || !subchip->gpadc_page_addr)
		return -ENODEV;

	/* PM800 block power page */
	subchip->power_page = i2c_new_dummy(client->adapter,
					    subchip->power_page_addr);
	if (subchip->power_page == NULL) {
		ret = -ENODEV;
		goto out;
	}

	subchip->regmap_power = devm_regmap_init_i2c(subchip->power_page,
						     &pm80x_regmap_config);
	if (IS_ERR(subchip->regmap_power)) {
		ret = PTR_ERR(subchip->regmap_power);
		dev_err(chip->dev,
			"Failed to allocate regmap_power: %d\n", ret);
		goto out;
	}

	i2c_set_clientdata(subchip->power_page, chip);

	/* PM800 block GPADC */
	subchip->gpadc_page = i2c_new_dummy(client->adapter,
					    subchip->gpadc_page_addr);
	if (subchip->gpadc_page == NULL) {
		ret = -ENODEV;
		goto out;
	}

	subchip->regmap_gpadc = devm_regmap_init_i2c(subchip->gpadc_page,
						     &pm80x_regmap_config);
	if (IS_ERR(subchip->regmap_gpadc)) {
		ret = PTR_ERR(subchip->regmap_gpadc);
		dev_err(chip->dev,
			"Failed to allocate regmap_gpadc: %d\n", ret);
		goto out;
	}
	i2c_set_clientdata(subchip->gpadc_page, chip);

	/* PM800 block TEST: i2c addr 0x37 */
	subchip->test_page = i2c_new_dummy(client->adapter,
					    subchip->test_page_addr);
	if (subchip->test_page == NULL) {
		ret = -ENODEV;
		goto out;
	}
	subchip->regmap_test = devm_regmap_init_i2c(subchip->test_page,
						    &pm80x_regmap_config);
	if (IS_ERR(subchip->regmap_test)) {
		ret = PTR_ERR(subchip->regmap_test);
		dev_err(chip->dev,
			"Failed to allocate regmap_test: %d\n", ret);
		goto out;
	}
	i2c_set_clientdata(subchip->test_page, chip);

out:
	return ret;
}

static void pm800_pages_exit(struct pm80x_chip *chip)
{
	struct pm80x_subchip *subchip;

	subchip = chip->subchip;

	if (subchip && subchip->power_page)
		i2c_unregister_device(subchip->power_page);

	if (subchip && subchip->gpadc_page)
		i2c_unregister_device(subchip->gpadc_page);

	if (subchip && subchip->test_page)
		i2c_unregister_device(subchip->test_page);
}

static int device_800_init(struct pm80x_chip *chip,
				     struct pm80x_platform_data *pdata)
{
	int ret;
	unsigned int val, data;

#if defined(CONFIG_SEC_DEBUG)
	/* read power on reason from PMIC general use register */
	ret = regmap_read(chip->regmap, PMIC_GENERAL_USE_REGISTER, &data);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read PMIC_GENERAL_USE_REGISTER : %d\n"
				, ret);
		goto out;
	}

	pr_info("%s read register PMIC_GENERAL_USE_REGISTER [%d]\n", __func__,
			data);
	val = data & (PMIC_GENERAL_USE_REBOOT_DN_MASK);
	/* read power on reason from PMIC general use register */
	if (val != PMIC_GENERAL_USE_BOOT_BY_FULL_RESET)
	{
		data &= ~(PMIC_GENERAL_USE_REBOOT_DN_MASK);
		data |= PMIC_GENERAL_USE_BOOT_BY_HW_RESET;
		regmap_write(chip->regmap, PMIC_GENERAL_USE_REGISTER,data);
	}
	power_on_reason = (u8)val;
#endif
	/*
	 * alarm wake up bit will be clear in device_irq_init(),
	 * read before that
	 */
	ret = regmap_read(chip->regmap, PM800_RTC_CONTROL, &val);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read RTC register: %d\n", ret);
		goto out;
	}
	if (val & PM800_ALARM_WAKEUP) {
		if (pdata && pdata->rtc)
			pdata->rtc->rtc_wakeup = 1;
	}

	ret = device_gpadc_init(chip, pdata);
	if (ret < 0) {
		dev_err(chip->dev, "[%s]Failed to init gpadc\n", __func__);
		goto out;
	}

	chip->regmap_irq_chip = &pm800_irq_chip;
	if (pdata)
		chip->irq_mode = pdata->irq_mode;

	ret = device_irq_init_800(chip);
	if (ret < 0) {
		dev_err(chip->dev, "[%s]Failed to init pm800 irq\n", __func__);
		goto out;
	}

	ret = device_onkey_init(chip, pdata);
	if (ret) {
		dev_err(chip->dev, "Failed to add onkey subdev\n");
		goto out_dev;
	}

	ret = device_rtc_init(chip, pdata);
	if (ret) {
		dev_err(chip->dev, "Failed to add rtc subdev\n");
		goto out;
	}

	ret = device_regulator_init(chip, pdata);
	if (ret) {
		dev_err(chip->dev, "Failed to add regulators subdev\n");
		goto out;
	}

	ret = device_sw_battery_init(chip, pdata);
	if (ret) {
		dev_err(chip->dev, "Failed to add sw fuelgauge subdev\n");
		goto out;
	}

	ret = device_vbus_init(chip, pdata);
	if (ret) {
		dev_err(chip->dev, "Failed to add vbus detection subdev\n");
		goto out;
	}

	ret = device_vibrator_init(chip, pdata);
	if (ret) {
		dev_err(chip->dev, "Failed to add vibrator subdev\n");
		goto out;
	}

	ret = device_headset_init(chip, pdata);
	if (ret) {
		dev_err(chip->dev, "Failed to add headset subdev\n");
		goto out;
	}

	ret = device_dvc_init(chip, pdata);
	if (ret)
		dev_warn(chip->dev, "Failied to add dvc subdev\n");

	return 0;
out_dev:
	mfd_remove_devices(chip->dev);
	device_irq_exit_800(chip);
out:
	return ret;
}

static int pm800_dt_init(struct device_node *np,
			 struct device *dev,
			 struct pm80x_platform_data *pdata)
{
	/* parent platform data only */
	pdata->irq_mode =
		!of_property_read_bool(np, "marvell,88pm800-irq-write-clear");

	return 0;
}

static int reboot_notifier_func(struct notifier_block *this,
		unsigned long code, void *p)
{
#ifdef CONFIG_SEC_DEBUG
	int data;
	struct pm80x_chip *chip;
	unsigned char pmic_download_register = 0;
	char *cmd = p;

	pr_info("reboot notifier...\n");

	chip = container_of(this, struct pm80x_chip, reboot_notifier);
	regmap_read(chip->regmap, PMIC_GENERAL_USE_REGISTER, &data);
	data &= ~(PMIC_GENERAL_USE_REBOOT_DN_MASK);

	if (cmd) {
		if (!strcmp(cmd, "recovery")) {
			pr_info("Device will enter recovery mode on next booting\n");
			data |= PMIC_GENERAL_USE_BOOT_BY_FULL_RESET;
			regmap_write(chip->regmap, PMIC_GENERAL_USE_REGISTER,
									data);
		} else if (!strcmp(cmd, "recovery_done")) {
			data |= PMIC_GENERAL_USE_BOOT_BY_RECOVERY_DONE;
			regmap_write(chip->regmap, PMIC_GENERAL_USE_REGISTER,
									data);
		} else if (!strcmp(cmd, "arm11_fota")) {
			data |= PMIC_GENERAL_USE_BOOT_BY_FOTA;
			regmap_write(chip->regmap, PMIC_GENERAL_USE_REGISTER,
									data);
		} else if (!strcmp(cmd, "alarm")) {
			data |= PMIC_GENERAL_USE_BOOT_BY_RTC_ALARM;
			regmap_write(chip->regmap, PMIC_GENERAL_USE_REGISTER,
									data);
		} else if (!strcmp(cmd, "debug0x4f4c")) {
			data |= PMIC_GENERAL_USE_BOOT_BY_DEBUGLEVEL_LOW;
			regmap_write(chip->regmap, PMIC_GENERAL_USE_REGISTER,
									data);
		} else if (!strcmp(cmd, "debug0x494d")) {
			data |= PMIC_GENERAL_USE_BOOT_BY_DEBUGLEVEL_MID;
			regmap_write(chip->regmap, PMIC_GENERAL_USE_REGISTER,
									data);
		} else if (!strcmp(cmd, "debug0x4948")) {
			data |= PMIC_GENERAL_USE_BOOT_BY_DEBUGLEVEL_HIGH;
			regmap_write(chip->regmap, PMIC_GENERAL_USE_REGISTER,
									data);
		} else if (!strcmp(cmd, "GlobalActions restart")) {
			data |= PMIC_GENERAL_USE_BOOT_BY_INTENDED_RESET;
			regmap_write(chip->regmap, PMIC_GENERAL_USE_REGISTER,
									data);
		} else if (!strcmp(cmd, "download")) {
			pmic_download_register = PMIC_GENERAL_DOWNLOAD_MODE_FUS
							+ DOWNLOAD_FUS_SUD_BASE;
			data |= pmic_download_register;
			regmap_write(chip->regmap, PMIC_GENERAL_USE_REGISTER,
									data);
		} else if (!strncmp(cmd, "sud", 3)) {
			/* Value : 21 ~ 29 */
			pmic_download_register = cmd[3] - '0' +
							DOWNLOAD_FUS_SUD_BASE;
			data |= pmic_download_register;
			regmap_write(chip->regmap, PMIC_GENERAL_USE_REGISTER,
									data);
		} else {
			if (get_recoverymode() == 1) {
				pr_info("reset noti recovery mode\n");
				data |= PMIC_GENERAL_USE_BOOT_BY_RECOVERY_DONE;
				regmap_write(chip->regmap,
					PMIC_GENERAL_USE_REGISTER, data);
			} else {
				pr_info("reset noti intended\n");
				data |= PMIC_GENERAL_USE_BOOT_BY_INTENDED_RESET;
				regmap_write(chip->regmap,
					PMIC_GENERAL_USE_REGISTER, data);
			}
		}
	} else {
		if (get_recoverymode() == 1) {
			pr_info("reset noti recovery p = null\n");
			data |= PMIC_GENERAL_USE_BOOT_BY_RECOVERY_DONE;
			regmap_write(chip->regmap, PMIC_GENERAL_USE_REGISTER,
								data);
		} else {
			pr_info("reset noti intended p = null\n");
			data |= PMIC_GENERAL_USE_BOOT_BY_INTENDED_RESET;
			regmap_write(chip->regmap, PMIC_GENERAL_USE_REGISTER,
								data);
		}
	}

	if (code != SYS_POWER_OFF) {
		pr_info("enable PMIC watchdog\n");
		regmap_read(chip->regmap, PM800_WAKEUP1, &data);
		pr_info("PM800_WAKEUP1 Reg(0x%02x) is 0x%02x\n",
				PM800_WAKEUP1, data);
		data |= PM800_WAKEUP1_WD_MODE;
		regmap_write(chip->regmap, PM800_WAKEUP1, data);

		regmap_read(chip->regmap, PM800_WAKEUP2, &data);
		pr_info("PM800_WAKEUP2 Reg(0x%02x) is 0x%02x\n",
				PM800_WAKEUP2, data);
		data &= ~(PM800_WD_TIMER_ACT_MASK);
		data |= PM800_WD_TIMER_ACT_8S;
		regmap_write(chip->regmap,
				PM800_WAKEUP2, data);
		pr_info("0x%02x is written to PM800_WAKEUP2 Reg(0x%02x)\n",
				data, PM800_WAKEUP2);

		regmap_write(chip->regmap, PM800_WATCHDOG_REG,
				PM800_WD_EN);
		regmap_read(chip->regmap, PM800_WATCHDOG_REG, &data);
		pr_info("WATCHDOG Reg(0x%02x) is 0x%02x\n",
				PM800_WATCHDOG_REG, data);
	}

	return 0;
#else
	int data;
	struct pm80x_chip *chip;
	char *cmd = p;

	pr_info("reboot notifier...\n");

	chip = container_of(this, struct pm80x_chip, reboot_notifier);
	if (cmd && (0 == strcmp(cmd, "recovery"))) {
		pr_info("Enter recovery mode\n");
		regmap_read(chip->regmap, PM800_USER_DATA6, &data);
		regmap_write(chip->regmap, PM800_USER_DATA6, data | 0x1);

	} else {
		regmap_read(chip->regmap, PM800_USER_DATA6, &data);
		regmap_write(chip->regmap, PM800_USER_DATA6, data & 0xfe);
	}

	if (code != SYS_POWER_OFF) {
		regmap_read(chip->regmap, PM800_USER_DATA6, &data);
		/* this bit is for charger server */
		regmap_write(chip->regmap, PM800_USER_DATA6, data | 0x2);
	}

	return 0;
#endif
}

static int pm800_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	int ret = 0;
	struct pm80x_chip *chip;
	struct pm80x_platform_data *pdata = dev_get_platdata(&client->dev);
	struct device_node *node = client->dev.of_node;
	struct pm80x_subchip *subchip;

	if (IS_ENABLED(CONFIG_OF)) {
		if (!pdata) {
			pdata = devm_kzalloc(&client->dev,
					     sizeof(*pdata), GFP_KERNEL);
			if (!pdata)
				return -ENOMEM;
		}
		ret = pm800_dt_init(node, &client->dev, pdata);
		if (ret)
			return ret;
	} else if (!pdata) {
		return -EINVAL;
	}

	/*
	 * RTC in pmic is alive even the core is powered off, expired-alarm is
	 * a power-up event to the system; every time the system boots up,
	 * whether it's powered up by PMIC-rtc needs to be recorded and pass
	 * this information to RTC driver
	 */
	if (!pdata->rtc) {
		pdata->rtc = devm_kzalloc(&client->dev,
					  sizeof(*(pdata->rtc)), GFP_KERNEL);
		if (!pdata->rtc)
			return -ENOMEM;
	}

	ret = pm80x_init(client);
	if (ret) {
		dev_err(&client->dev, "pm800_init fail\n");
		goto out_init;
	}

	chip = i2c_get_clientdata(client);

	/* init subchip for PM800 */
	subchip =
	    devm_kzalloc(&client->dev, sizeof(struct pm80x_subchip),
			 GFP_KERNEL);
	if (!subchip) {
		ret = -ENOMEM;
		goto err_subchip_alloc;
	}

	/* pm800 has 2 addtional pages to support power and gpadc. */
	subchip->power_page_addr = client->addr + 1;
	subchip->gpadc_page_addr = client->addr + 2;
	subchip->test_page_addr = client->addr + 7;
	chip->subchip = subchip;

	ret = pm800_pages_init(chip);
	if (ret) {
		dev_err(&client->dev, "pm800_pages_init failed!\n");
		goto err_device_init;
	}

	ret = device_800_init(chip, pdata);
	if (ret) {
		dev_err(chip->dev, "Failed to initialize 88pm800 devices\n");
		goto err_device_init;
	}

	/*
	 * config registers for pmic.
	 * common registers is configed in pm800_init_config directly,
	 * board specfic registers and configuration is passed
	 * from board specific dts file.
	 */
	if (IS_ENABLED(CONFIG_OF))
		pm800_init_config(chip, node);
	else
		pm800_init_config(chip, NULL);

	if (pdata && pdata->plat_config)
		pdata->plat_config(chip, pdata);

	parse_powerup_down_log(chip);
	chip->reboot_notifier.notifier_call = reboot_notifier_func;

	ret = pm800_dump_debugfs_init(chip);
	if (ret) {
		dev_err(chip->dev, "create debugfs fails, but let's continue.\n");
		ret = 0;
	}

	chip_g = chip;
	pm_power_off = sw_poweroff;
	register_reboot_notifier(&(chip->reboot_notifier));

	return 0;

err_device_init:
	pm800_pages_exit(chip);
err_subchip_alloc:
	pm80x_deinit();
out_init:
	return ret;
}

static int pm800_remove(struct i2c_client *client)
{
	struct pm80x_chip *chip = i2c_get_clientdata(client);

	mfd_remove_devices(chip->dev);
	device_irq_exit_800(chip);

	pm800_pages_exit(chip);
	pm80x_deinit();
	pm800_dump_debugfs_remove(chip);

	return 0;
}

static struct i2c_driver pm800_driver = {
	.driver = {
		.name = "88PM800",
		.owner = THIS_MODULE,
		.pm = &pm80x_pm_ops,
		.of_match_table	= of_match_ptr(pm80x_dt_ids),
		},
	.probe = pm800_probe,
	.remove = pm800_remove,
	.id_table = pm80x_id_table,
};

static int __init pm800_i2c_init(void)
{
	return i2c_add_driver(&pm800_driver);
}
subsys_initcall(pm800_i2c_init);

static void __exit pm800_i2c_exit(void)
{
	i2c_del_driver(&pm800_driver);
}
module_exit(pm800_i2c_exit);

MODULE_DESCRIPTION("PMIC Driver for Marvell 88PM800");
MODULE_AUTHOR("Qiao Zhou <zhouqiao@marvell.com>");
MODULE_LICENSE("GPL");
