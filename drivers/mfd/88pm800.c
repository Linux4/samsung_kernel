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
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/88pm80x.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/irq.h>
#include <linux/proc_fs.h>
#include "../staging/android/switch/switch.h"

#define PM80X_BASE_REG_NUM		0xf0
#define PM80X_POWER_REG_NUM		0x9b
#define PM80X_GPADC_REG_NUM		0xb6
#define	PM800_PROC_FILE		"driver/pm800_reg"

#define PM800_CHIP_ID			(0x00)

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

#define PM800_INT_ENA_3		(0x0B)
#define PM800_GPADC0_INT_ENA3		(1 << 0)
#define PM800_GPADC1_INT_ENA3		(1 << 1)
#define PM800_GPADC2_INT_ENA3		(1 << 2)
#define PM800_GPADC3_INT_ENA3		(1 << 3)
#define PM800_GPADC4_INT_ENA3		(1 << 4)

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
	PM800_IRQ_GPADC0,	/*EN3b0 *//*10 */
	PM800_IRQ_GPADC1,	/*EN3b1 */
	PM800_IRQ_GPADC2,	/*EN3b2 */
	PM800_IRQ_GPADC3,	/*EN3b3 */
	PM800_IRQ_GPADC4,	/*EN3b4 */
	PM800_IRQ_GPIO0,	/*EN4b0 *//*15 */
	PM800_IRQ_GPIO1,	/*EN4b1 */
	PM800_IRQ_GPIO2,	/*EN4b2 */
	PM800_IRQ_GPIO3,	/*EN4b3 */
	PM800_IRQ_GPIO4,	/*EN4b4 *//*19 */
	PM800_MAX_IRQ,
};

enum {
	/* Procida */
	PM800_CHIP_A0  = 0x60,
	PM800_CHIP_A1  = 0x61,
	PM800_CHIP_B0  = 0x62,
	PM800_CHIP_C0  = 0x63,
	PM800_CHIP_D0  = 0x64,
	PM800_CHIP_END = PM800_CHIP_D0,

	/* Make sure to update this to the last stepping */
	PM8XXX_CHIP_END = PM800_CHIP_END
};

static const struct i2c_device_id pm80x_id_table[] = {
	{"88PM800", CHIP_PM800},
	{} /* NULL terminated */
};
MODULE_DEVICE_TABLE(i2c, pm80x_id_table);

static int reg_pm800 = 0xffff;
static int pg_index;

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

static struct mfd_cell onkey_devs[] = {
	{
	 .name = "88pm80x-onkey",
	 .num_resources = 1,
	 .resources = &onkey_resources[0],
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
};

static struct mfd_cell usb_devs[] = {
	{
	.name = "88pm80x-usb",
	.num_resources = 1,
	.resources = &usb_resources[0],
	.id = -1,
	},
};


static struct mfd_cell dvc_devs[] = {
	{
	 .name = "dvc",
	 .id = -1,
	},
};

static struct resource bat_resources[] = {
	{
	 .name = "88pm80x-bat",
	 .start = PM800_IRQ_VBAT,
	 .end = PM800_IRQ_VBAT,
	 .flags = IORESOURCE_IRQ,
	 },
};

static struct mfd_cell bat_devs[] = {
	{
	.name = "88pm80x-bat",
	.num_resources = 1,
	.resources = &bat_resources[0],
	.id = -1,
},
};

static struct mfd_cell vibrator_devs[] = {
	{
	 .name = "88pm80x-vibrator",
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

static struct gpio_switch_platform_data headset_switch_device_data[] = {
	{
	 /* headset switch */
	 .name = "h2w",
	 .gpio = 0,
	 .name_on = NULL,
	 .name_off = NULL,
	 .state_on = NULL,
	 .state_off = NULL,
	 }, {
	 /* hook switch */
	 .name = "h3w",
	 .gpio = 0,
	 .name_on = NULL,
	 .name_off = NULL,
	 .state_on = NULL,
	 .state_off = NULL,
	 },
};

static struct mfd_cell headset_devs_800[] = {
	{
	 .name = "88pm800-headset",
	 .num_resources = ARRAY_SIZE(headset_resources_800),
	 .resources = &headset_resources_800[0],
	 .id = -1,
	 .platform_data = headset_switch_device_data,
	 .pdata_size = sizeof(headset_switch_device_data),
	 },
};

static struct resource regulator_resources[] = {
	{PM800_ID_BUCK1, PM800_ID_BUCK1, "buck-1", IORESOURCE_IO,},
	{PM800_ID_BUCK2, PM800_ID_BUCK2, "buck-2", IORESOURCE_IO,},
	{PM800_ID_BUCK3, PM800_ID_BUCK3, "buck-3", IORESOURCE_IO,},
	{PM800_ID_BUCK4, PM800_ID_BUCK4, "buck-4", IORESOURCE_IO,},
	{PM800_ID_BUCK5, PM800_ID_BUCK5, "buck-5", IORESOURCE_IO,},
	{PM800_ID_LDO1, PM800_ID_LDO1, "ldo-01", IORESOURCE_IO,},
	{PM800_ID_LDO2, PM800_ID_LDO2, "ldo-02", IORESOURCE_IO,},
	{PM800_ID_LDO3, PM800_ID_LDO3, "ldo-03", IORESOURCE_IO,},
	{PM800_ID_LDO4, PM800_ID_LDO4, "ldo-04", IORESOURCE_IO,},
	{PM800_ID_LDO5, PM800_ID_LDO5, "ldo-05", IORESOURCE_IO,},
	{PM800_ID_LDO6, PM800_ID_LDO6, "ldo-06", IORESOURCE_IO,},
	{PM800_ID_LDO7, PM800_ID_LDO7, "ldo-07", IORESOURCE_IO,},
	{PM800_ID_LDO8, PM800_ID_LDO8, "ldo-08", IORESOURCE_IO,},
	{PM800_ID_LDO9, PM800_ID_LDO9, "ldo-09", IORESOURCE_IO,},
	{PM800_ID_LDO10, PM800_ID_LDO10, "ldo-10", IORESOURCE_IO,},
	{PM800_ID_LDO11, PM800_ID_LDO11, "ldo-11", IORESOURCE_IO,},
	{PM800_ID_LDO12, PM800_ID_LDO12, "ldo-12", IORESOURCE_IO,},
	{PM800_ID_LDO13, PM800_ID_LDO13, "ldo-13", IORESOURCE_IO,},
	{PM800_ID_LDO14, PM800_ID_LDO14, "ldo-14", IORESOURCE_IO,},
	{PM800_ID_LDO15, PM800_ID_LDO15, "ldo-15", IORESOURCE_IO,},
	{PM800_ID_LDO16, PM800_ID_LDO16, "ldo-16", IORESOURCE_IO,},
	{PM800_ID_LDO17, PM800_ID_LDO17, "ldo-17", IORESOURCE_IO,},
	{PM800_ID_LDO18, PM800_ID_LDO18, "ldo-18", IORESOURCE_IO,},
	{PM800_ID_LDO19, PM800_ID_LDO19, "ldo-19", IORESOURCE_IO,},

	/* Below 4 resources are fake, only used in new DVC */
	{PM800_ID_BUCK1_AP_ACTIVE, PM800_ID_BUCK1_AP_ACTIVE, "buck-1-ap-active", IORESOURCE_IO,},
	{PM800_ID_BUCK1_AP_LPM, PM800_ID_BUCK1_AP_LPM, "buck-1-ap-lpm", IORESOURCE_IO,},
	{PM800_ID_BUCK1_APSUB_IDLE, PM800_ID_BUCK1_APSUB_IDLE, "buck-1-apsub-idle", IORESOURCE_IO,},
	{PM800_ID_BUCK1_APSUB_SLEEP, PM800_ID_BUCK1_APSUB_SLEEP, "buck-1-apsub-sleep", IORESOURCE_IO,},
};

static struct mfd_cell regulator_devs[] = {
	{"88pm80x-regulator", 0,},
	{"88pm80x-regulator", 1,},
	{"88pm80x-regulator", 2,},
	{"88pm80x-regulator", 3,},
	{"88pm80x-regulator", 4,},
	{"88pm80x-regulator", 5,},
	{"88pm80x-regulator", 6,},
	{"88pm80x-regulator", 7,},
	{"88pm80x-regulator", 8,},
	{"88pm80x-regulator", 9,},
	{"88pm80x-regulator", 10,},
	{"88pm80x-regulator", 11,},
	{"88pm80x-regulator", 12,},
	{"88pm80x-regulator", 13,},
	{"88pm80x-regulator", 14,},
	{"88pm80x-regulator", 15,},
	{"88pm80x-regulator", 16,},
	{"88pm80x-regulator", 17,},
	{"88pm80x-regulator", 18,},
	{"88pm80x-regulator", 19,},
	{"88pm80x-regulator", 20,},
	{"88pm80x-regulator", 21,},
	{"88pm80x-regulator", 22,},
	{"88pm80x-regulator", 23,},

	/* below 4 regulators are fake, only used in new dvc */
	{"88pm80x-regulator", 24,},
	{"88pm80x-regulator", 25,},
	{"88pm80x-regulator", 26,},
	{"88pm80x-regulator", 27,},
};

static struct regulator_init_data regulator_pdata[ARRAY_SIZE(regulator_devs)];

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
#if defined(CONFIG_SPA) ||			\
	defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
#define CHG_STATUS_HIGH 1
#define CHG_STATUS_LOW 0
#define PM800_USER_DATA3 0xEA

static u8 power_on_reason = 0;

unsigned char pm80x_get_power_on_reason(void)
{
	return power_on_reason;	
}
#endif
unsigned char pmic_chip_id = 0;
static ssize_t pm800_proc_read(char *buf, char **start, off_t off,
		int count, int *eof, void *data)
{
	unsigned int reg_val = 0;
	int len = 0;
	struct pm80x_chip *chip = data;
	int i;

	if (reg_pm800 == 0xffff) {
		pr_info("pm800: base page:\n");
		for (i = 0; i < PM80X_BASE_REG_NUM; i++) {
			regmap_read(chip->regmap, i, &reg_val);
			pr_info("[0x%02x]=0x%02x\n", i, reg_val);
		}
		pr_info("pm80x: power page:\n");
		for (i = 0; i < PM80X_POWER_REG_NUM; i++) {
			regmap_read(chip->subchip->regmap_power, i, &reg_val);
			pr_info("[0x%02x]=0x%02x\n", i, reg_val);
		}
		pr_info("pm80x: gpadc page:\n");
		for (i = 0; i < PM80X_GPADC_REG_NUM; i++) {
			regmap_read(chip->subchip->regmap_gpadc, i, &reg_val);
			pr_info("[0x%02x]=0x%02x\n", i, reg_val);
		}

		len = 0;
	} else {

		switch (pg_index) {
		case 0:
			regmap_read(chip->regmap, reg_pm800, &reg_val);
			break;
		case 1:
			regmap_read(chip->subchip->regmap_power, reg_pm800, &reg_val);
			break;
		case 2:
			regmap_read(chip->subchip->regmap_gpadc, reg_pm800, &reg_val);
			break;
		case 7:
			regmap_read(chip->subchip->regmap_test, reg_pm800, &reg_val);
			break;
		default:
			pr_err("pg_index error!\n");
			return 0;
		}

		len = sprintf(buf, "reg_pm800=0x%x, pg_index=0x%x, val=0x%x\n",
			      reg_pm800, pg_index, reg_val);
	}
	return len;
}

static ssize_t pm800_proc_write(struct file *filp,
				       const char *buff, size_t len,
				       void *data)
{
	u8 reg_val;
	struct pm80x_chip *chip = data;

	char messages[20], index[20];
	memset(messages, '\0', 20);
	memset(index, '\0', 20);

	if (copy_from_user(messages, buff, len))
		return -EFAULT;

	if ('-' == messages[0]) {
		if ((strlen(messages) != 10) &&
		    (strlen(messages) != 9)) {
			pr_err("Right format: -0x[page_addr] 0x[reg_addr]\n");
			return len;
		}
		/* set the register index */
		memcpy(index, messages + 1, 3);

		if (kstrtoint(index, 16, &pg_index) < 0)
			return -EINVAL;

		pr_info("pg_index = 0x%x\n", pg_index);

		memcpy(index, messages + 5, 4);
		if (kstrtoint(index, 16, &reg_pm800) < 0)
			return -EINVAL;
		pr_info("reg_pm800 = 0x%x\n", reg_pm800);
	} else if ('+' == messages[0]) {
		/* enable to get all the reg value */
		reg_pm800 = 0xffff;
		pr_info("read all reg enabled!\n");
	} else {
		if ((reg_pm800 == 0xffff) ||
		    ('0' != messages[0])) {
			pr_err("Right format: -0x[page_addr] 0x[reg_addr]\n");
			return len;
		}
		/* set the register value */
		if (kstrtou8(messages, 16, &reg_val) < 0)
			return -EINVAL;

		switch (pg_index) {
		case 0:
			regmap_write(chip->regmap, reg_pm800, reg_val & 0xff);
			break;
		case 1:
			regmap_write(chip->subchip->regmap_power,
				     reg_pm800, reg_val & 0xff);
			break;
		case 2:
			regmap_write(chip->subchip->regmap_gpadc,
				     reg_pm800, reg_val & 0xff);
			break;
		case 7:
			regmap_write(chip->subchip->regmap_test,
				     reg_pm800, reg_val & 0xff);
			break;
		default:
			pr_err("pg_index error!\n");
			break;

		}
	}

	return len;
}
static int __devinit device_gpadc_init(struct pm80x_chip *chip,
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
        /*make gpadc1 enable*/
	ret = regmap_update_bits(map, PM800_GPADC_MEAS_EN2,
				 (PM800_MEAS_EN2_RFTMP | PM800_MEAS_GP0_EN| PM800_MEAS_GP1_EN),
				 (PM800_MEAS_EN2_RFTMP | PM800_MEAS_GP0_EN| PM800_MEAS_GP1_EN));
	if (ret < 0)
		goto out;

	/*
	 * the defult of PM800 is GPADC operates at 100Ks/s rate
	 * and Number of GPADC slots with active current bias prior
	 * to GPADC sampling = 1 slot for all GPADCs set for
	 * Temprature mesurmants
	 */
#ifdef CONFIG_GPADC0_CURRENT_SINK_MODE
        /*make gpadc0, gpadc1 bias out enable*/
	mask = (PM800_GPADC_GP_BIAS_EN0 | PM800_GPADC_GP_BIAS_EN1 |
		PM800_GPADC_GP_BIAS_EN2 | PM800_GPADC_GP_BIAS_EN3 |
		PM800_GPADC_GP_BIAS_OUT0 | PM800_GPADC_GP_BIAS_OUT1);

	if (pdata && (pdata->batt_det == 0))
		data = (PM800_GPADC_GP_BIAS_EN0 | PM800_GPADC_GP_BIAS_EN1 |
			PM800_GPADC_GP_BIAS_EN2 | PM800_GPADC_GP_BIAS_EN3|
			PM800_GPADC_GP_BIAS_OUT0 | PM800_GPADC_GP_BIAS_OUT1);
	else
		data = (PM800_GPADC_GP_BIAS_EN0 | PM800_GPADC_GP_BIAS_EN2 |
			PM800_GPADC_GP_BIAS_EN3 | PM800_GPADC_GP_BIAS_OUT0 |
			PM800_GPADC_GP_BIAS_OUT1);
#else
        /*make gpadc1 bias out enable*/
	mask = (PM800_GPADC_GP_BIAS_EN0 | PM800_GPADC_GP_BIAS_EN1 |
		PM800_GPADC_GP_BIAS_EN2 | PM800_GPADC_GP_BIAS_EN3 |PM800_GPADC_GP_BIAS_OUT1);

	if (pdata && (pdata->batt_det == 0))
		data = (PM800_GPADC_GP_BIAS_EN0 | PM800_GPADC_GP_BIAS_EN1 |
			PM800_GPADC_GP_BIAS_EN2 | PM800_GPADC_GP_BIAS_EN3|PM800_GPADC_GP_BIAS_OUT1);
	else
		data = (PM800_GPADC_GP_BIAS_EN0 | PM800_GPADC_GP_BIAS_EN2 |
			PM800_GPADC_GP_BIAS_EN3 | PM800_GPADC_GP_BIAS_OUT1);
#endif

	ret = regmap_update_bits(map, PM800_GP_BIAS_ENA1, mask, data);
	if (ret < 0)
		{goto out;}
	/*make gpadc0, gpadc1 output 31uA bias */
	mask = PM800_GPADC_GP_BIAS_MASK0;
	data = (0x06 << PM800_GPADC_GP_BIAS_SHIFT1_D0); // 31uA
#ifdef CONFIG_GPADC0_CURRENT_SINK_MODE
	ret = regmap_update_bits(map, PM800_GPADC_BIAS1, mask, data);
	if (ret < 0)
		{goto out;}
#endif
	ret = regmap_update_bits(map, PM800_GPADC_BIAS2, mask, data);
	if (ret < 0)
		{goto out;}
	dev_info(chip->dev, "pm800 device_gpadc_init: Done\n");
	return 0;

out:
	dev_info(chip->dev, "pm800 device_gpadc_init: Failed!\n");
	return ret;
}

static int __devinit device_regulator_init(struct pm80x_chip *chip,
				struct pm80x_platform_data *pdata)
{
	struct regulator_init_data *initdata;
	int ret = 0;
	int i, seq;

	if (!pdata || !pdata->regulator) {
		dev_warn(chip->dev, "Regulator pdata is unavailable!\n");
		return 0;
	}

	if (pdata->num_regulators > ARRAY_SIZE(regulator_devs))
		pdata->num_regulators = ARRAY_SIZE(regulator_devs);

	for (i = 0; i < pdata->num_regulators; i++) {
		initdata = &pdata->regulator[i];
		seq = *(unsigned int *)initdata->driver_data;
		if ((seq < 0) || (seq > PM800_ID_RG_MAX)) {
			dev_err(chip->dev, "Wrong ID(%d) on regulator(%s)\n",
				seq, initdata->constraints.name);
			ret = -EINVAL;
			goto out_err;
		}
		memcpy(&regulator_pdata[i], &pdata->regulator[i],
		       sizeof(struct regulator_init_data));
		regulator_devs[i].platform_data = &regulator_pdata[i];
		regulator_devs[i].pdata_size =
		    sizeof(struct regulator_init_data);
		regulator_devs[i].num_resources = 1;
		regulator_devs[i].resources = &regulator_resources[seq];

		ret = mfd_add_devices(chip->dev, 0, &regulator_devs[i], 1,
				      &regulator_resources[seq], 0);
		if (ret < 0) {
			dev_err(chip->dev, "Failed to add regulator subdev\n");
			goto out_err;
		}
	}
	dev_info(chip->dev, "[%s]:Added mfd regulator_devs\n", __func__);
	return 0;

out_err:
	return ret;
}

static int __devinit device_irq_init_800(struct pm80x_chip *chip)
{
	struct regmap *map = chip->regmap;
	unsigned long flags = IRQF_ONESHOT;
	struct irq_desc *desc;
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

	data = PM800_WAKEUP2_INT_CLEAR;
	ret = regmap_update_bits(map, PM800_WAKEUP2, mask, data);

	if (ret < 0)
		goto out;

	ret =
	    regmap_add_irq_chip(chip->regmap, chip->irq, flags, -1,
				chip->regmap_irq_chip, &chip->irq_data);

	chip->irq_base = regmap_irq_chip_get_base(chip->irq_data);

	desc = irq_to_desc(chip->irq);
	irq_get_chip(chip->irq_base)->irq_set_wake =
		desc->irq_data.chip->irq_set_wake;
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

	subchip = chip->subchip;
	/* PM800 block power: i2c addr 0x31 */
	if (subchip->power_page_addr) {
		subchip->power_page =
		    i2c_new_dummy(client->adapter, subchip->power_page_addr);
		subchip->regmap_power =
		    devm_regmap_init_i2c(subchip->power_page,
					 &pm80x_regmap_config);
		i2c_set_clientdata(subchip->power_page, chip);
	} else
		dev_info(chip->dev,
			 "PM800 block power 0x31: No power_page_addr\n");

	/* PM800 block GPADC: i2c addr 0x32 */
	if (subchip->gpadc_page_addr) {
		subchip->gpadc_page = i2c_new_dummy(client->adapter,
						    subchip->gpadc_page_addr);
		subchip->regmap_gpadc =
		    devm_regmap_init_i2c(subchip->gpadc_page,
					 &pm80x_regmap_config);
		i2c_set_clientdata(subchip->gpadc_page, chip);
	} else
		dev_info(chip->dev,
			 "PM800 block GPADC 0x32: No gpadc_page_addr\n");

	/* PM800 block TEST: i2c addr 0x37 */
	if (subchip->test_page_addr) {
		subchip->test_page = i2c_new_dummy(client->adapter,
						    subchip->test_page_addr);
		subchip->regmap_test =
		    devm_regmap_init_i2c(subchip->test_page,
					 &pm80x_regmap_config);
		i2c_set_clientdata(subchip->test_page, chip);
	} else
		dev_info(chip->dev,
			 "PM800 block GPADC 0x37: No test_page_addr\n");
	return 0;
}

static void pm800_pages_exit(struct pm80x_chip *chip)
{
	struct pm80x_subchip *subchip;

	subchip = chip->subchip;
	if (subchip->power_page) {
		regmap_exit(subchip->regmap_power);
		i2c_unregister_device(subchip->power_page);
	}
	if (subchip->gpadc_page) {
		regmap_exit(subchip->regmap_gpadc);
		i2c_unregister_device(subchip->gpadc_page);
	}
	if (subchip->test_page) {
		regmap_exit(subchip->regmap_test);
		i2c_unregister_device(subchip->test_page);
	}
	i2c_unregister_device(chip->client);
}

static int __devinit device_800_init(struct pm80x_chip *chip,
				     struct pm80x_platform_data *pdata)
{
	int ret, pmic_id;
	unsigned int val;

	ret = regmap_read(chip->regmap, PM800_CHIP_ID, &val);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read CHIP ID: %d\n", ret);
		goto out;
	}

	pmic_id = val & PM80X_VERSION_MASK;

	if ((pmic_id >= PM800_CHIP_A0) && (pmic_id <= PM800_CHIP_END)) {
		chip->version = val;
                pmic_chip_id = pmic_id;
		dev_info(chip->dev,
			 "88PM80x:Marvell 88PM800 (ID:0x%x) detected\n", val);
	} else {
		dev_err(chip->dev,
			"Failed to detect Marvell 88PM800:ChipID[0x%x]\n", val);
		ret = -EINVAL;
		goto out;
	}
#if defined(CONFIG_SPA)	||			\
	defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
	/* read power on reason from PMIC general use register */
	ret = regmap_read(chip->regmap, PM800_USER_DATA3, &val);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read PM800_USER_DATA3 : %d\n", ret);
		goto out;
	}
	printk("%s read register PM800_USER_DATA3 [%d]\n", __func__, val);
#if 0
	if (ret & PM800_CHG_STS1)
	{
		ret = PMIC_GENERAL_USE_BOOT_BY_CHG;
		printk("%s charger detected %d\n", __func__, ret);
	}
	else
	{
		ret = PMIC_GENERAL_USE_BOOT_BY_ONKEY;
		printk("%s charger not detected %d\n", __func__, ret);
	}
#endif	
	/* read power on reason from PMIC general use register */
	regmap_write(chip->regmap, PM800_USER_DATA3, PMIC_GENERAL_USE_BOOT_BY_HW_RESET);
	power_on_reason	= (u8)val; 
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

	ret = device_irq_init_800(chip);
	if (ret < 0) {
		dev_err(chip->dev, "[%s]Failed to init pm800 irq\n", __func__);
		goto out;
	}

	if (device_regulator_init(chip, pdata)) {
		dev_err(chip->dev, "Failed to init regulators\n");
		goto out_dev;
	}

	ret =
	    mfd_add_devices(chip->dev, 0, &onkey_devs[0],
			    ARRAY_SIZE(onkey_devs), &onkey_resources[0], 0);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to add onkey subdev\n");
		goto out_dev;
	} else
		dev_info(chip->dev, "[%s]:Added mfd onkey_devs\n", __func__);

	if (pdata && pdata->bat) {
		bat_devs[0].platform_data = pdata->bat;
		bat_devs[0].pdata_size = sizeof(struct pm80x_bat_pdata);
		ret = mfd_add_devices(chip->dev, 0, &bat_devs[0],
				      ARRAY_SIZE(bat_devs), NULL, 0);
		if (ret < 0) {
			dev_err(chip->dev, "Failed to add bat subdev\n");
			goto out_dev;
		} else
			dev_info(chip->dev,
				"[%s]:Added mfd bat_devs\n", __func__);
	}

	if (pdata && pdata->vibrator) {
		vibrator_devs[0].platform_data = pdata->vibrator;
		vibrator_devs[0].pdata_size = sizeof(struct pm80x_vibrator_pdata);
		ret = mfd_add_devices(chip->dev, 0, &vibrator_devs[0],
			ARRAY_SIZE(vibrator_devs), NULL, 0);
		if (ret < 0) {
			dev_err(chip->dev, "Failed to add vibrator subdev\n");
			goto out_dev;
		} else
			dev_info(chip->dev,
				"[%s]:Added mfd vibrator_devs\n", __func__);
	}

	if (pdata && pdata->rtc) {
		rtc_devs[0].platform_data = pdata->rtc;
		rtc_devs[0].pdata_size = sizeof(struct pm80x_rtc_pdata);
		ret = mfd_add_devices(chip->dev, 0, &rtc_devs[0],
				      ARRAY_SIZE(rtc_devs), NULL, 0);
		if (ret < 0) {
			dev_err(chip->dev, "Failed to add rtc subdev\n");
			goto out_dev;
		} else
			dev_info(chip->dev,
				 "[%s]:Added mfd rtc_devs\n", __func__);
	}

	ret =
	    mfd_add_devices(chip->dev, 0, &headset_devs_800[0],
			    ARRAY_SIZE(headset_devs_800),
			    &headset_resources_800[0], 0);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to add headset subdev\n");
		goto out_dev;
	} else
		dev_info(chip->dev, "[%s]:Added mfd headset_devs\n", __func__);

	if (pdata && pdata->dvc) {
		dvc_devs[0].platform_data = pdata->dvc;
		dvc_devs[0].pdata_size = sizeof(struct pm80x_dvc_pdata);
		ret = mfd_add_devices(chip->dev, 0, &dvc_devs[0],
				      ARRAY_SIZE(dvc_devs), NULL, 0);
		if (ret < 0) {
			dev_err(chip->dev, "Failed to add dvc subdev\n");
			goto out_dev;
		} else
			dev_info(chip->dev, "[%s]:Added mfd dvc_devs\n", __func__);
	}

	if (chip->proc_file == NULL) {
		chip->proc_file =
			create_proc_entry(PM800_PROC_FILE, 0644, NULL);
		if (chip->proc_file) {
			chip->proc_file->read_proc = pm800_proc_read;
			chip->proc_file->write_proc = (write_proc_t  *)pm800_proc_write;
			chip->proc_file->data = chip;
		} else
			pr_info("pm800 proc file create failed!\n");
	}

	if (pdata && pdata->usb) {
		usb_devs[0].platform_data = pdata->usb;
		usb_devs[0].pdata_size = sizeof(struct pm80x_usb_pdata);
		ret = mfd_add_devices(chip->dev, 0, &usb_devs[0],
				      ARRAY_SIZE(usb_devs), NULL, 0);
		if (ret < 0) {
			dev_err(chip->dev, "Failed to add usb subdev\n");
			goto out_dev;
		} else
			dev_info(chip->dev,
				 "[%s]:Added mfd usb_devs\n", __func__);
	}

	return 0;
out_dev:
	mfd_remove_devices(chip->dev);
	device_irq_exit_800(chip);
out:
	return ret;
}

static int __devinit pm800_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	int ret = 0;
	struct pm80x_chip *chip;
	struct pm80x_platform_data *pdata = client->dev.platform_data;
	struct pm80x_subchip *subchip;

	ret = pm80x_init(client, id);
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

	subchip->power_page_addr = pdata->power_page_addr;
	subchip->gpadc_page_addr = pdata->gpadc_page_addr;
	subchip->test_page_addr = pdata->test_page_addr;
	chip->subchip = subchip;

	ret = pm800_pages_init(chip);
	if (ret) {
		dev_err(&client->dev, "pm800_pages_init failed!\n");
		goto err_page_init;
	}

	ret = device_800_init(chip, pdata);
	if (ret) {
		dev_err(chip->dev, "%s id 0x%x failed!\n", __func__, chip->id);
		goto err_800_init;
	}

	if (pdata->plat_config)
		pdata->plat_config(chip, pdata);

	return 0;

err_800_init:
	pm800_pages_exit(chip);
err_page_init:
	devm_kfree(&client->dev, subchip);
err_subchip_alloc:
	pm80x_deinit();
out_init:
	return ret;
}

static int __devexit pm800_remove(struct i2c_client *client)
{
	struct pm80x_chip *chip = i2c_get_clientdata(client);

	mfd_remove_devices(chip->dev);
	device_irq_exit_800(chip);

	pm800_pages_exit(chip);
	remove_proc_entry(PM800_PROC_FILE, NULL);
	devm_kfree(&client->dev, chip->subchip);

	pm80x_deinit();

	return 0;
}

static struct i2c_driver pm800_driver = {
	.driver = {
		.name = "88PM800",
		.owner = THIS_MODULE,
		.pm = &pm80x_pm_ops,
		},
	.probe = pm800_probe,
	.remove = __devexit_p(pm800_remove),
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
