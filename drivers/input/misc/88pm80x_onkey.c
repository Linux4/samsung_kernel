/*
 * Marvell 88PM80x ONKEY driver
 *
 * Copyright (C) 2012 Marvell International Ltd.
 * Haojian Zhuang <haojian.zhuang@marvell.com>
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
#include <linux/input.h>
#include <linux/mfd/88pm80x.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/of.h>

#define PM800_LONG_ONKEY_EN1           (1 << 0)
#define PM800_LONG_ONKEY_EN2           (1 << 1)
#define PM800_LONG_KEY_DELAY		(12)	/* 1 .. 16 seconds */
#define PM800_LONKEY_PRESS_TIME		((PM800_LONG_KEY_DELAY-1) << 4)
#define PM860_LONKEY_RESTOUTN_PULSE_MASK (0x3)

#define PM800_LONKEY_PRESS_TIME_MASK	(0xF0)
#define PM800_LONKEY_RESET		(1 << 3)
#define PM800_SW_PDOWN			(1 << 5)

#define PM860_ALWAYS_ON2		(0xe2)
#define PM860_HWRST_DB_MASK		(0x1 << 7)
#define PM860_HWRST_DB_2S		(0x0 << 7)
#define PM860_HWRST_DB_7S		(0x1 << 7)

struct pm80x_onkey_info {
	struct input_dev *idev;
	struct pm80x_chip *pm80x;
	struct regmap *map;
	int irq;
	int gpio_number;
	int hw_rst;
	int long_onkey_rst;
};

static int pm80x_config_gpio(struct pm80x_onkey_info *info)
{
	u8 ctrl_reg, ctrl_mask, ctrl_val;

	if (!info || !info->map) {
		pr_err("%s: No chip information!\n", __func__);
		return -ENODEV;
	}

	/* set debounce period of ONKEY as 7s, when used with GPIO */
	regmap_update_bits(info->map, PM860_ALWAYS_ON2,
			   PM860_HWRST_DB_MASK, PM860_HWRST_DB_7S);

	if (info->pm80x->chip_id == CHIP_PM86X_ID_Z3) {
		switch (info->gpio_number) {
		case 0:
			/* HW_RST1_N and HW_RST2_N */
			ctrl_reg = PM800_GPIO_0_1_CNTRL;
			ctrl_mask = PM860_GPIO0_MODE_MASK;
			if (info->hw_rst == 1)
				ctrl_val = PM800_GPIO0_GPIO_MODE(0x6);
			else
				ctrl_val = PM800_GPIO0_GPIO_MODE(0x7);
			break;
		case 1:
			/* HW_RST1_N and HW_RST2_N */
			ctrl_reg = PM800_GPIO_0_1_CNTRL;
			ctrl_mask = PM860_GPIO1_MODE_MASK;
			if (info->hw_rst == 1)
				ctrl_val = PM800_GPIO1_GPIO_MODE(0x6);
			else
				ctrl_val = PM800_GPIO1_GPIO_MODE(0x7);
			break;
		case 2:
			/* HW_RST1 and HW_RST2 */
			ctrl_reg = PM800_GPIO_2_3_CNTRL;
			ctrl_mask = PM860_GPIO2_MODE_MASK;
			if (info->hw_rst == 1)
				ctrl_val = PM800_GPIO2_GPIO_MODE(0x6);
			else
				ctrl_val = PM800_GPIO2_GPIO_MODE(0x7);
			break;
		case 5:
			/* HW_RST1 and HW_RST2 */
			ctrl_reg = PM800_GPIO_4_5_CNTRL;
			ctrl_mask = PM860_GPIO5_MODE_MASK;
			if (info->hw_rst == 1)
				ctrl_val = PM860_GPIO5_GPIO_MODE(0x6);
			else
				ctrl_val = PM860_GPIO5_GPIO_MODE(0x7);
			break;
		default:
			return -EINVAL;
		}
	} else if (info->pm80x->chip_id == CHIP_PM86X_ID_A0) {
		switch (info->gpio_number) {
		case 0:
			/* HW_RST1_N and HW_RST2_N */
			ctrl_reg = PM800_GPIO_0_1_CNTRL;
			ctrl_mask = PM860_GPIO0_MODE_MASK;
			if (info->hw_rst == 1)
				ctrl_val = PM800_GPIO0_GPIO_MODE(0x6);
			else
				ctrl_val = PM800_GPIO0_GPIO_MODE(0x7);
			break;
		case 1:
			/* HW_RST1_N and HW_RST2_N */
			ctrl_reg = PM800_GPIO_0_1_CNTRL;
			ctrl_mask = PM860_GPIO1_MODE_MASK;
			if (info->hw_rst == 1)
				ctrl_val = PM800_GPIO1_GPIO_MODE(0x6);
			else
				ctrl_val = PM800_GPIO1_GPIO_MODE(0x7);
			break;
		case 2:
			/* HW_RST1 and HW_RST2 */
			ctrl_reg = PM800_GPIO_2_3_CNTRL;
			ctrl_mask = PM860_GPIO2_MODE_MASK;
			if (info->hw_rst == 1)
				ctrl_val = PM800_GPIO2_GPIO_MODE(0x6);
			else
				ctrl_val = PM800_GPIO2_GPIO_MODE(0x7);
			break;
		case 3:
			/* HW_RST1 and HW_RST2 */
			ctrl_reg = PM800_GPIO_2_3_CNTRL;
			ctrl_mask = PM860_GPIO3_MODE_MASK;
			if (info->hw_rst == 1)
				ctrl_val = PM800_GPIO3_GPIO_MODE(0x6);
			else
				ctrl_val = PM800_GPIO3_GPIO_MODE(0x7);
			break;
		default:
			return -EINVAL;
		}
	} else
		return -EINVAL;

	regmap_update_bits(info->map, ctrl_reg, ctrl_mask, ctrl_val);

	return 0;
}

/* 88PM80x gives us an interrupt when ONKEY is held */
static irqreturn_t pm80x_onkey_handler(int irq, void *data)
{
	struct pm80x_onkey_info *info = data;
	int ret = 0;
	unsigned int val;

	/* reset the LONGKEY reset time */
	regmap_update_bits(info->map, PM800_WAKEUP1,
			   PM800_LONKEY_RESET, PM800_LONKEY_RESET);
	ret = regmap_read(info->map, PM800_STATUS_1, &val);
	if (ret < 0) {
		dev_err(info->idev->dev.parent, "failed to read status: %d\n", ret);
		return IRQ_NONE;
	}
	val &= PM800_ONKEY_STS1;

	input_report_key(info->idev, KEY_POWER, val);
	input_sync(info->idev);

	return IRQ_HANDLED;
}

static SIMPLE_DEV_PM_OPS(pm80x_onkey_pm_ops, pm80x_dev_suspend,
			 pm80x_dev_resume);

static int pm80x_onkey_dt_init(struct device_node *np,
			       struct pm80x_onkey_info *info)
{
	int ret;

	ret = of_property_read_u32(np, "onkey-gpio-number",
				   &info->gpio_number);
	if (ret < 0) {
		pr_warn("No GPIO number for long onkey!\n");
		return 0;
	}

	ret = of_property_read_u32(np, "hw-rst-type", &info->hw_rst);
	if (ret < 0) {
		pr_warn("No hw reset type for gpio + long onkey!\n");
		return 0;
	}

	ret = of_property_read_u32(np, "long-onkey-type",
				   &info->long_onkey_rst);
	if (ret < 0) {
		pr_warn("NO long onkey type!\n");
		return 0;
	}

	return 0;
}

static int pm80x_onkey_probe(struct platform_device *pdev)
{

	struct pm80x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct device_node *node = pdev->dev.of_node;
	struct pm80x_onkey_info *info;
	int err;

	info = kzalloc(sizeof(struct pm80x_onkey_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->pm80x = chip;

	/* give the gpio number as a default value */
	info->gpio_number = -1;
	err = pm80x_onkey_dt_init(node, info);
	if (err < 0)
		return -ENODEV;

	info->irq = platform_get_irq(pdev, 0);
	if (info->irq < 0) {
		dev_err(&pdev->dev, "No IRQ resource!\n");
		err = -EINVAL;
		goto out;
	}

	info->map = info->pm80x->regmap;
	if (!info->map) {
		dev_err(&pdev->dev, "no regmap!\n");
		err = -EINVAL;
		goto out;
	}

	info->idev = input_allocate_device();
	if (!info->idev) {
		dev_err(&pdev->dev, "Failed to allocate input dev\n");
		err = -ENOMEM;
		goto out;
	}

	info->idev->name = "88pm80x_on";
	info->idev->phys = "88pm80x_on/input0";
	info->idev->id.bustype = BUS_I2C;
	info->idev->dev.parent = &pdev->dev;
	info->idev->evbit[0] = BIT_MASK(EV_KEY);
	__set_bit(KEY_POWER, info->idev->keybit);

	err = pm80x_request_irq(info->pm80x, info->irq, pm80x_onkey_handler,
				IRQF_ONESHOT | IRQF_NO_SUSPEND, "onkey", info);
	if (err < 0) {
		dev_err(&pdev->dev, "Failed to request IRQ: #%d: %d\n",
			info->irq, err);
		goto out_reg;
	}

	err = input_register_device(info->idev);
	if (err) {
		dev_err(&pdev->dev, "Can't register input device: %d\n", err);
		goto out_irq;
	}

	platform_set_drvdata(pdev, info);

	err = pm80x_config_gpio(info);
	if (err < 0) {
		dev_err(&pdev->dev, "Cannot configure gpio: %d\n", err);
		info->long_onkey_rst = 2;
	}

	/* set duration of RESETOUTN pulse as 1s */
	regmap_update_bits(chip->regmap, PM800_RTC_MISC3,
			   PM860_LONKEY_RESTOUTN_PULSE_MASK, 1);

	if (info->long_onkey_rst == 1)
		/* enable LONG_ONKEY_DETECT1, power down the supply */
		regmap_update_bits(chip->regmap, PM800_RTC_MISC4,
				   PM800_LONG_ONKEY_EN2 | PM800_LONG_ONKEY_EN1,
				   PM800_LONG_ONKEY_EN1);
	else
		/*
		 * enable LONG_ONKEY_DETECT2,
		 * which doesn't force a supply power down
		 */
		regmap_update_bits(chip->regmap, PM800_RTC_MISC4,
				   PM800_LONG_ONKEY_EN2 | PM800_LONG_ONKEY_EN1,
				   PM800_LONG_ONKEY_EN2);

	/* set debounce period of ONKEY as 12s */
	regmap_update_bits(chip->regmap, PM800_RTC_MISC3,
			   PM800_LONKEY_PRESS_TIME_MASK,
			   PM800_LONKEY_PRESS_TIME);
	/* set FAULT_WU_EN */
	regmap_update_bits(chip->regmap, PM800_RTC_MISC5, PM800_FAULT_WAKEUP_EN,
			   PM800_FAULT_WAKEUP_EN);

	device_init_wakeup(&pdev->dev, 1);
	return 0;

out_irq:
	pm80x_free_irq(info->pm80x, info->irq, info);
out_reg:
	input_free_device(info->idev);
out:
	kfree(info);
	return err;
}

static int pm80x_onkey_remove(struct platform_device *pdev)
{
	struct pm80x_onkey_info *info = platform_get_drvdata(pdev);

	device_init_wakeup(&pdev->dev, 0);
	pm80x_free_irq(info->pm80x, info->irq, info);
	input_unregister_device(info->idev);
	kfree(info);
	return 0;
}

static void pm80x_onkey_shutdown(struct platform_device *pdev)
{
	struct pm80x_onkey_info *info = platform_get_drvdata(pdev);
	pm80x_free_irq(info->pm80x, info->irq, info);
	return;
}

static const struct of_device_id pm80x_onkey_dt_match[] = {
	{ .compatible = "marvell,88pm80x-onkey", },
	{ },
};
MODULE_DEVICE_TABLE(of, pm80x_onkey_dt_match);

static struct platform_driver pm80x_onkey_driver = {
	.driver = {
		   .name = "88pm80x-onkey",
		   .owner = THIS_MODULE,
		   .pm = &pm80x_onkey_pm_ops,
		   .of_match_table = of_match_ptr(pm80x_onkey_dt_match),
		   },
	.probe = pm80x_onkey_probe,
	.remove = pm80x_onkey_remove,
	.shutdown = pm80x_onkey_shutdown,
};

module_platform_driver(pm80x_onkey_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Marvell 88PM80x ONKEY driver");
MODULE_AUTHOR("Qiao Zhou <zhouqiao@marvell.com>");
MODULE_ALIAS("platform:88pm80x-onkey");
