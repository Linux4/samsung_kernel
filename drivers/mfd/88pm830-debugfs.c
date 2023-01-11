/*
 * Debugfs inteface for Marvell 88PM830
 *
 * Copyright (C) 2013 Marvell International Ltd.
 * Jett Zhou <jtzhou@marvell.com>
 * Yi Zhang <yizhang@marvell.com>
 * Shay Pathov <shayp@marvell.com>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file "COPYING" in the main directory of this
 * archive for more details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/irq.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/mfd/core.h>
#include <linux/mfd/88pm830.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/ctype.h>

#define PM830_REG_NUM			(0xf9)

#define PM830_VBAT_RESOLUTION		1367
#define PM830_VSYS_RESOLUTION		1367
#define PM830_VCHG_RESOLUTION		1709
#define PM830_VPWR_RESOLUTION		1709
#define PM830_GPADC0_RESOLUTION		342
#define PM830_GPADC1_RESOLUTION		342


#define PM830_DISPLAY_ONE_VALUE(name, reg)				\
{									\
	ret = regmap_bulk_read(chip->regmap, PM830_##name##_##reg, meas_val, 2);\
	if (ret < 0)							\
		return ret;						\
									\
	value = (meas_val[0] << 4) | (meas_val[1] & 0x0f);		\
	value = value * PM830_##name##_RESOLUTION / 1000;		\
	len += sprintf(&buf[len], " %-7d|", value);			\
}

#define PM830_DISPLAY_GPADC_BIAS(name)					\
{									\
	ret = regmap_bulk_read(chip->regmap, PM830_##name##_BIAS, &bias_val, 1);\
	if (ret < 0)							\
		return ret;						\
									\
	len += sprintf(&buf[len], "\n|%-6s(uA)| %s |", __stringify(name),\
	  (!(en_bias & PM830_##name##_BIAS_EN) ||			\
	  (en_bias & PM830_##name##_GP_BIAS_OUT) ? "enable " : "disable"));\
	if (!(en_bias & PM830_##name##_BIAS_EN) ||			\
		(en_bias & PM830_##name##_GP_BIAS_OUT)) {		\
		value = ((bias_val & 0x0F) * 5) + 1;			\
		len += sprintf(&buf[len], " %-7d|", value);		\
	} else								\
		len += sprintf(buf + len, "  -     |");			\
}

#define PM830_DISPLAY_GPADC_VALUES(name)				\
{									\
	len += sprintf(&buf[len], "\n|"__stringify(name)" (mV)| %s |",	\
		(en_val & PM830_##name##_MEAS_EN ? "enable " : "disable"));\
	if (en_val & PM830_##name##_MEAS_EN) {				\
		PM830_DISPLAY_ONE_VALUE(name, MEAS1)			\
		PM830_DISPLAY_ONE_VALUE(name, AVG)			\
		PM830_DISPLAY_ONE_VALUE(name, MIN)			\
		PM830_DISPLAY_ONE_VALUE(name, MAX)			\
	} else								\
		len += sprintf(&buf[len],				\
				"  -     |  -     |  -     |  -     |");\
}

static int reg_pm830 = 0xffff;

static ssize_t pm830_regdump_read(struct file *file, char __user *user_buf,
			       size_t count, loff_t *ppos)
{
	struct pm830_chip *chip = file->private_data;
	unsigned int reg_val = 0;
	int len = 0;
	char str[100];
	int i;

	if (reg_pm830 == 0xffff) {
		len = snprintf(str, sizeof(str) - 1, "%s\n",
			       "pm830: register dump:");
		for (i = 0; i < PM830_REG_NUM; i++) {
			regmap_read(chip->regmap, i, &reg_val);
			pr_info("[0x%02x]=0x%02x\n", i, reg_val);
		}
	} else {
		regmap_read(chip->regmap, reg_pm830, &reg_val);
		len = snprintf(str, sizeof(str),
			       "reg_pm830=0x%x, val=0x%x\n",
			       reg_pm830, reg_val);
	}

	return simple_read_from_buffer(user_buf, count, ppos, str, len);
}

static ssize_t pm830_regdump_write(struct file *file,
				const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	u8 reg_val;
	struct pm830_chip *chip = file->private_data;
	int i = 0;
	int ret;

	char messages[20];
	memset(messages, '\0', 20);

	if (copy_from_user(messages, user_buf, count))
		return -EFAULT;

	if ('+' == messages[0]) {
		/* enable to get all the reg value */
		reg_pm830 = 0xffff;
		pr_info("read all reg enabled!\n");
	} else {
		if (messages[1] != 'x') {
			pr_err("Right format: 0x[addr]\n");
			return -EINVAL;
		}

		if (strlen(messages) > 5) {
			while (messages[i] != ' ')
				i++;
			messages[i] = '\0';
			if (kstrtouint(messages, 16, &reg_pm830) < 0)
				return -EINVAL;
			i++;
			if (kstrtou8(messages + i, 16, &reg_val) < 0)
				return -EINVAL;
			ret = regmap_write(chip->regmap, reg_pm830,
					   reg_val & 0xff);
			if (ret < 0) {
				pr_err("write reg error!\n");
				return -EINVAL;
			}
		} else {
			if (kstrtouint(messages, 16, &reg_pm830) < 0)
				return -EINVAL;
		}
	}

	return count;
}

static const struct file_operations pm830_regdump_ops = {
	.owner		= THIS_MODULE,
	.open		= simple_open,
	.read		= pm830_regdump_read,
	.write		= pm830_regdump_write,
};


static ssize_t pm830_display_chip_ver(struct pm830_chip *chip, char *buf)
{
	int len;

	if ((chip == NULL) || (buf == NULL))
		return -EINVAL;

	len = sprintf(buf, "\n-----------------------------------\n");
	len += sprintf(&buf[len], "Chip version is: (0x%x) ", chip->version);

	switch (chip->version) {
	case PM830_A1_VERSION:
		len += sprintf(&buf[len], "A1\n");
		break;
	case PM830_B0_VERSION:
		len += sprintf(&buf[len], "B0\n");
		break;
	case PM830_B1_VERSION:
		len += sprintf(&buf[len], "B1\n");
		break;
	case PM830_B2_VERSION:
		len += sprintf(&buf[len], "B2\n");
		break;
	default:
		len += sprintf(&buf[len], "Unknown\n");
		break;
	};

	return len;
}

static ssize_t pm830_display_state(struct pm830_chip *chip, char *buf)
{
	int len, ret;
	unsigned char value;

	if ((chip == NULL) || (buf == NULL))
		return -EINVAL;

	if (chip->version >= PM830_B0_VERSION) {
		ret = regmap_bulk_read(chip->regmap, PM830_CHG_STATUS3,
					&value, 1);
		if (ret < 0)
			return ret;

		/* select bits 7:5 */
		value = (value >> 5) & 0x07;
		len = sprintf(buf, "PM830 state: (0x%x) ", value);
		switch (value) {
		case 0x0:
			len += sprintf(&buf[len], "power-up\n");
			break;
		case 0x1:
			len += sprintf(&buf[len],
					"Automatic Pre-charge Reserved\n");
			break;
		case 0x2:
			len += sprintf(&buf[len], "Pre-regulation\n");
			break;
		case 0x3:
			len += sprintf(&buf[len], "Linear Pre-charge\n");
			break;
		case 0x4:
			len += sprintf(&buf[len], "Fast-charge\n");
			break;
		case 0x5:
			len += sprintf(&buf[len], "Supplement Mode\n");
			break;
		case 0x6:
			len += sprintf(&buf[len], "Battery powered\n");
			break;
		case 0x7:
			len += sprintf(&buf[len],
					"Boost Mode (Flash/Torch/OTG)\n");
			break;
		default:
			len += sprintf(&buf[len], "Unknown mode\n");
			break;
		};

	} else {
		/* open test page */
		value = 0x01;
		ret = regmap_bulk_write(chip->regmap, 0xF0, &value, 1);
		if (ret < 0)
			return ret;

		/* read status from test register */
		ret = regmap_bulk_read(chip->regmap, 0xD1, &value, 1);
		if (ret < 0)
			return ret;

		/* select bits 3:0 */
		value = value & 0x0f;
		len = sprintf(buf, "PM830 state: (0x%x) ", value);

		switch (value) {
		case 0x3:
			len += sprintf(&buf[len], "Automatic pre-charge\n");
			break;
		case 0x5:
			len += sprintf(&buf[len], "Pre-regulation\n");
			break;
		case 0x6:
			len += sprintf(&buf[len], "Pre-charge\n");
			break;
		case 0x7:
			len += sprintf(&buf[len], "Fast-charge\n");
			break;
		case 0x9:
			len += sprintf(&buf[len], "supplement mode\n");
			break;
		case 0xa:
			len += sprintf(&buf[len], "battery powered\n");
			break;
		default:
			len += sprintf(&buf[len], "Unknown mode\n");
			break;
		}
	}

	len += sprintf(&buf[len], "\n-----------------------------------\n");

	return len;
}

static ssize_t pm830_display_status_register(struct pm830_chip *chip, char *buf)
{
	int len, ret;
	unsigned char value;

	if ((chip == NULL) || (buf == NULL))
		return -EINVAL;

	ret = regmap_bulk_read(chip->regmap, PM830_STATUS, &value, 1);
	if (ret < 0)
		return ret;

	len = sprintf(buf, "Status register:\n\n");

	if (value & PM830_CHG_DET)
		len += sprintf(&buf[len], "CHG_DET=1:     charger detected\n");
	else
		len += sprintf(&buf[len],
				"CHG_DET=0:     charger not present\n");

	if (value & PM830_BAT_DET)
		len += sprintf(&buf[len], "BAT_DET=1:     battery detected\n");
	else
		len += sprintf(&buf[len],
				"BAT_DET=0:     no battery detected\n");

	if (value & PM830_VBUS_STATUS)
		len += sprintf(&buf[len],
				"VBUS_STATUS=1: USB voltage is valid\n");
	else
		len += sprintf(&buf[len],
				"VBUS_STATUS=0: USB voltage is not valid\n");

	return len;
}

char temp_buf[5000];

static ssize_t pm830_debug_read(struct file *file, char __user *user_buf,
				size_t count, loff_t *ppos)
{
	int len, ret, value;
	unsigned char en_val;
	unsigned char en_bias;
	struct pm830_chip *chip = file->private_data;
	unsigned char meas_val[3];
	unsigned char bias_val;
	char *buf;

	buf = kzalloc(2048, GFP_KERNEL);

	if ((chip == NULL) || (buf == NULL))
		return -EINVAL;

	len = pm830_display_chip_ver(chip, buf);

	ret = pm830_display_state(chip, &buf[len]);

	if (ret < 0) {
		pr_err("%s:can't read chip status\n", __func__);
		return ret;
	}
	len += ret;

	ret = pm830_display_status_register(chip, &buf[len]);
	if (ret < 0) {
		pr_err("%s:can't read chip status\n", __func__);
		return ret;
	}
	len += ret;

	ret = regmap_bulk_read(chip->regmap, PM830_GPADC_MEAS_EN, &en_val, 1);
	if (ret < 0) {
		pr_err("%s:can't read PM830_GPADC_MEAS_EN\n", __func__);
		return ret;
	}

	ret = regmap_bulk_read(chip->regmap, PM830_GPADC_BIAS_ENA, &en_bias, 1);
	if (ret < 0) {
		pr_err("%s:can't read PM830_GPADC_BIAS_ENA\n", __func__);
		return ret;
	}

	len += sprintf(&buf[len],
	       "\n---------------------------------------------------------\n");
	len += sprintf(&buf[len],
		"| name    | status  | value  | average|  min   |  max   |\n");
	len += sprintf(&buf[len],
		"|---------|---------|--------|--------|--------|--------|");

	PM830_DISPLAY_GPADC_VALUES(VBAT);
	PM830_DISPLAY_GPADC_VALUES(VSYS);
	PM830_DISPLAY_GPADC_VALUES(VCHG);
	PM830_DISPLAY_GPADC_VALUES(VPWR);

	len += sprintf(&buf[len],
	       "\n---------------------------------------------------------\n");

	len += sprintf(&buf[len],
		"\n-------------------------------\n");
	len += sprintf(&buf[len],
		"| name     | status  |  value |\n");
	len += sprintf(&buf[len],
		"|----------|---------|--------|\n");

	/* prepare and add to output ITEMP measurement value */
	len += sprintf(&buf[len], "|ITEMP (C) | %s |",
			(en_val & PM830_ITEMP_MEAS_EN ? "enable " : "disable"));

	if (en_val & PM830_ITEMP_MEAS_EN) {
		ret = regmap_bulk_read(chip->regmap, PM830_ITEMP_HI, meas_val, 1);
		if (ret < 0)
			return ret;

		value = (meas_val[0] << 4) | (meas_val[1] & 0x0f);
		value = value * 104 / 1000 - 273; /* resolution in mK -> C */

		len += sprintf(&buf[len], " %-7d|", value);
	} else
		len += sprintf(&buf[len], "  -     |");

	/* prepare and add to output IBAT measurement value */
	ret = regmap_bulk_read(chip->regmap, PM830_IBAT_CC1, meas_val, 3);
	if (ret < 0)
		return ret;

	value = ((meas_val[2] & 0x3) << 16)
			| ((meas_val[1] & 0xff) << 8)
			| (meas_val[0] & 0xff);
	/* ibat is negative if discharging, poistive is charging */
	if (value & (1 << 17))
		value = (0xFF << 24) | (0x3F << 18) | (value);
	value = (value * 458) / 10000;

	len += sprintf(&buf[len], "\n|IBAT  (mA)|         | %-7d|", value);

	/* prepare and add to output GPADC0 measurement value */
	len += sprintf(&buf[len], "\n|GPADC0(mV)| %s |",
		(en_val & PM830_GPADC0_MEAS_EN ? "enable " : "disable"));

	if (en_val & PM830_GPADC0_MEAS_EN)
		PM830_DISPLAY_ONE_VALUE(GPADC0, MEAS1)
	else
		len += sprintf(buf + len, "  -     |");
	PM830_DISPLAY_GPADC_BIAS(GPADC0);

	/* prepare and add to output GPADC1 measurement value */
	len += sprintf(&buf[len], "\n|GPADC1(mV)| %s |",
		(en_val & PM830_GPADC1_MEAS_EN ? "enable " : "disable"));

	if (en_val & PM830_GPADC1_MEAS_EN)
		PM830_DISPLAY_ONE_VALUE(GPADC1, MEAS1)
	else
		len += sprintf(buf + len, "  -     |");
	PM830_DISPLAY_GPADC_BIAS(GPADC1);

	len += sprintf(&buf[len], "\n-------------------------------\n\n");

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return ret;

}

static const struct file_operations pm830_debug_ops = {
	.owner		= THIS_MODULE,
	.open		= simple_open,
	.read		= pm830_debug_read,
};

static int pm830_debug_probe(struct platform_device *pdev)
{
	struct pm830_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct dentry *pm830_regdump;
	struct dentry *pm830_debug;

	pm830_regdump = debugfs_create_file("pm830_reg", S_IRUGO | S_IFREG,
			    NULL, (void *)chip, &pm830_regdump_ops);

	if (pm830_regdump == NULL) {
		pr_err("create pm830_regdump debugfs error!\n");
		return -ENOENT;
	} else if (pm830_regdump == ERR_PTR(-ENODEV)) {
		pr_err("CONFIG_DEBUG_FS is not enabled!\n");
		return -ENOENT;
	}

	pm830_debug = debugfs_create_file("pm830_debug", S_IRUGO | S_IFREG,
			    NULL, (void *)chip, &pm830_debug_ops);

	if (pm830_debug == NULL) {
		pr_err("create pm830_debug debugfs error!\n");
		return -ENOENT;
	} else if (pm830_debug == ERR_PTR(-ENODEV)) {
		pr_err("CONFIG_DEBUG_FS is not enabled!\n");
		return -ENOENT;
	}

	return 0;
}

static int pm830_debug_remove(struct platform_device *pdev)
{
	struct pm830_chip *chip = dev_get_drvdata(pdev->dev.parent);

	debugfs_remove_recursive(chip->debugfs);
	return 0;
}

static struct platform_driver pm830_debug_driver = {
	.probe = pm830_debug_probe,
	.remove = pm830_debug_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "88pm830-debug",
	},
};

module_platform_driver(pm830_debug_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("88pm830 debug Driver");
MODULE_AUTHOR("Yi Zhang<yizhang@marvell.com>");
