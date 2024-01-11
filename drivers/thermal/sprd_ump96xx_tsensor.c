// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2021 Spreadtrum Communications Inc.

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/thermal.h>
#include <linux/mutex.h>

#define UMP96XX_XTL_WAIT_CTRL0		0x20e0
#define UMP96XX_XTL_EN			BIT(8)

#define	UMP96XX_TSEN_CTRL0		0x0
#define UMP96XX_TSEN_CLK_SRC_SEL	BIT(4)

#define	UMP96XX_TSEN_CTRL1		0x4
#define UMP96XX_TSEN_26M_EN		BIT(0)
#define UMP96XX_TSEN_SDADC_EN		BIT(4)

#define	UMP96XX_TSEN_CTRL3		0xc
#define UMP96XX_TSEN_SEL_CH		BIT(3)
#define UMP96XX_TSEN_EN			BIT(4)
#define UMP96XX_TSEN_UGBUF_EN		BIT(8)
#define UMP96XX_TSEN_ADCLDO_EN		BIT(12)

#define	UMP96XX_TSEN_CTRL4		0x10
#define	UMP96XX_TSEN_CTRL5		0x14
#define	UMP96XX_TSEN_CTRL6		0x18
#define UMP96XX_TSEN_SEL_EN		(BIT(6) | BIT(7))

#define UMP96XX_TSEN_OSC_TEMP(h, l) \
	(((((h & GENMASK(5, 3)) << 16) | (l & GENMASK(15, 0))) << 4) & 0xFFFFF)
#define UMP96XX_TSEN_TSEN_TEMP(h, l) \
	(((((h & GENMASK(2, 0)) << 16) | (l & GENMASK(15, 0))) << 4) & 0xFFFFF)

#define UMP96XX_TSEN_INDEX_MASK		GENMASK(7, 0)
#define UMP96XX_TSEN_INDEX_SHIFT	12
#define UMP96XX_TSEN_FRAC_MASK		GENMASK(11, 0)

/* Temperature query table according to integral index */
static const int v2t_table[256] = {
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 280421, 264933, 251238, 238928, 227748, 217492, 208038, 199268,
	191078, 183383, 176104, 169240, 162710, 156500, 150570, 144893, 139428, 134191, 129138, 124253, 119525, 114943, 110500, 106187,
	101990, 97905, 93919, 90026, 86224, 82506, 78860, 75286, 71777, 68330, 64940, 61606, 58337, 55113, 51932, 48790,
	45696, 42642, 39620, 36627, 33675, 30750, 27848, 24974, 22128, 19300, 16490, 13707, 10937, 8180, 5444, 2717,
	0,      -2702, -5398, -8088, -10768, -13446, -16120, -18791, -21462, -24132, -26805, -29481, -32160, -34847, -37539, -40241,
	-42956, -45680, -48419, -51175, -53944, -56734, -59548, -62379, -65234, -68124, -71039, -73979, -76964, -79985, -83041, -86136,
	-89290, -92493, -95747, -99057, -102428, -105886, -109418, -113031, -116733, -120533, -124438,  -128458, -132609, -136903, -141357,
	-145991, -150823, -155881, -161216, -166830, -172781, -179140, -185969, -193354, -201441, -210423, -220528, -232181, -245984,
	-263191, -263191, -263191,
};

static bool cali_mode;
static DEFINE_MUTEX(tsen_mutex);

struct ump96xx_tsen {
	struct device *dev;
	struct thermal_zone_device *tz_dev;
	struct regmap *regmap;
	u32 base;
	int id;
};

static int ump96xx_tsensor_read_config(struct ump96xx_tsen *tsen)
{
	int ret;

	ret = regmap_update_bits(tsen->regmap, UMP96XX_XTL_WAIT_CTRL0,
				 UMP96XX_XTL_EN, UMP96XX_XTL_EN);
	if (ret)
		return ret;

	ret = regmap_update_bits(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL0,
				 UMP96XX_TSEN_CLK_SRC_SEL, UMP96XX_TSEN_CLK_SRC_SEL);
	if (ret)
		return ret;

	ret = regmap_update_bits(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL1,
				 UMP96XX_TSEN_26M_EN, UMP96XX_TSEN_26M_EN);
	if (ret)
		return ret;

	ret = regmap_update_bits(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL3,
				 UMP96XX_TSEN_ADCLDO_EN, UMP96XX_TSEN_ADCLDO_EN);
	if (ret)
		return ret;

	ret = regmap_update_bits(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL1,
				 UMP96XX_TSEN_SDADC_EN, UMP96XX_TSEN_SDADC_EN);
	if (ret)
		return ret;

	ret = regmap_update_bits(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL3,
				 UMP96XX_TSEN_UGBUF_EN, UMP96XX_TSEN_UGBUF_EN);
	if (ret)
		return ret;

	return regmap_update_bits(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL6,
				 UMP96XX_TSEN_SEL_EN, UMP96XX_TSEN_SEL_EN);
}

static int ump96xx_tsensor_osc_rawdata_read(struct ump96xx_tsen *tsen, int *rawdata)
{
	int ret, th, tl;

	ret = ump96xx_tsensor_read_config(tsen);
	if (ret)
		return ret;

	ret = regmap_update_bits(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL3,
				 UMP96XX_TSEN_SEL_CH, UMP96XX_TSEN_SEL_CH);
	if (ret)
		return ret;

	ret = regmap_update_bits(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL3,
				 UMP96XX_TSEN_EN, UMP96XX_TSEN_EN);
	if (ret)
		return ret;

	/*
	 * According to the requirements of design document,
	 * trigger a data sample and wait data ready need wait 21ms
	 */
	msleep(21);

	ret = regmap_read(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL5, &tl);
	if (ret)
		return ret;


	ret = regmap_read(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL6, &th);
	if (ret)
		return ret;

	*rawdata = UMP96XX_TSEN_OSC_TEMP(th, tl);

	return ret;
}

static int ump96xx_tsensor_osc_temp_read(struct ump96xx_tsen *tsen, int *temp)
{
	int ret, rawdata;

	mutex_lock(&tsen_mutex);
	ret = ump96xx_tsensor_osc_rawdata_read(tsen, &rawdata);
	mutex_unlock(&tsen_mutex);
	if (ret)
		return ret;

	/*
	 * According to the requirements of design document,
	 * tsensor osc temp = 3641500 - (4856 * rawdata) / 1000
	 */
	*temp = 3641500 - (((4856 * (rawdata >> 4)) / 1000) << 4);

	return ret;
}

static int ump96xx_tsensor_out_rawdata_read(struct ump96xx_tsen *tsen, int *rawdata)
{
	int ret, th, tl;

	ret = ump96xx_tsensor_read_config(tsen);
	if (ret)
		return ret;

	ret = regmap_update_bits(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL3,
				 UMP96XX_TSEN_SEL_CH, 0x0);
	if (ret)
		return ret;

	ret = regmap_update_bits(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL3,
				 UMP96XX_TSEN_EN, UMP96XX_TSEN_EN);
	if (ret)
		return ret;

	/*
	 * According to the requirements of design document,
	 * trigger a data sample and wait data ready need wait 21ms
	 */
	msleep(21);

	ret = regmap_read(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL4, &tl);
	if (ret)
		return ret;

	ret = regmap_read(tsen->regmap, tsen->base + UMP96XX_TSEN_CTRL6, &th);
	if (ret)
		return ret;

	*rawdata = UMP96XX_TSEN_TSEN_TEMP(th, tl);

	return ret;
}

static int ump96xx_tsensor_out_temp_read(struct ump96xx_tsen *tsen, int *temp)
{
	int ret, rawdata, index, frac, t;

	mutex_lock(&tsen_mutex);
	ret = ump96xx_tsensor_out_rawdata_read(tsen, &rawdata);
	mutex_unlock(&tsen_mutex);
	if (ret)
		return ret;

	index = (rawdata >> UMP96XX_TSEN_INDEX_SHIFT) & UMP96XX_TSEN_INDEX_MASK;
	frac = rawdata & UMP96XX_TSEN_FRAC_MASK;

	/*
	 * According to the requirements of design document,get the index accrding
	 * to the integral result, and query the current temperature value according to
	 * the index.
	 * t = (v2t_table[index] * (0x1000-frac) + v2t_table[index+1] * frac + 0x800) / 4096;
	 */
	if (index != sizeof(v2t_table) / 4 - 1)
		t = (v2t_table[index] * (0x1000 - frac) + v2t_table[index + 1] * frac + 0x800) / 4096;
	else
		t = v2t_table[index];

	/*
	 * According to the requirements of design document,
	 * tsensor out temp = (t * 1000) / 4096 + 25000
	 */
	*temp = (t * 1000) / 4096 + 25000;

	return ret;
}

static int ump96xx_tsensor_disable(struct regmap *regmap, u32 base)
{
	int ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL0,
				UMP96XX_TSEN_CLK_SRC_SEL, 0);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL3,
				UMP96XX_TSEN_EN | UMP96XX_TSEN_SEL_CH, 0);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL6,
				UMP96XX_TSEN_SEL_EN, 0);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL1,
				UMP96XX_TSEN_SDADC_EN, 0);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL3,
				UMP96XX_TSEN_UGBUF_EN, 0);
	if (ret)
		return ret;

	return regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL3,
				UMP96XX_TSEN_ADCLDO_EN, 0);
}

static int ump96xx_tsensor_mode_check(char *str)
{
	if (str && !strncmp(str, "cali", strlen("cali")))
		cali_mode = true;
	else
		cali_mode = false;

	return 0;
}
__setup("androidboot.mode=", ump96xx_tsensor_mode_check);

static int ump96xx_tsensor_get_temp(void *data, int *temp)
{
	struct ump96xx_tsen *tsen = data;
	int ret;

	if (tsen->id == 0)
		ret = ump96xx_tsensor_osc_temp_read(tsen, temp);
	else if (tsen->id == 1)
		ret = ump96xx_tsensor_out_temp_read(tsen, temp);
	else
		ret = -ENOTSUPP;

	return ret;
}

static const struct thermal_zone_of_device_ops tsensor_thermal_ops = {
	.get_temp = ump96xx_tsensor_get_temp,
};

static int ump96xx_tsen_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *sen_child;
	struct ump96xx_tsen *tsen;
	struct regmap *regmap;
	u32 base;
	int ret;

	if (!cali_mode) {
		dev_warn(&pdev->dev,
			"no calibration mode, don't register ump96xx tsen");
		return 0;
	}

	regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!regmap) {
		dev_err(&pdev->dev, "failed to get efuse regmap\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "reg", &base);
	if (ret) {
		dev_err(&pdev->dev, "failed to get efuse base address\n");
		return ret;
	}

	for_each_child_of_node(np, sen_child) {
		tsen = devm_kzalloc(&pdev->dev, sizeof(*tsen), GFP_KERNEL);
		if (!tsen)
			return -ENOMEM;

		tsen->regmap = regmap;
		tsen->base = base;

		ret = of_property_read_u32(sen_child, "reg", &tsen->id);
		if (ret) {
			dev_err(&pdev->dev, "get sensor reg failed");
			return ret;
		}
		tsen->tz_dev = thermal_zone_of_sensor_register(&pdev->dev, tsen->id,
								tsen, &tsensor_thermal_ops);
		if (IS_ERR(tsen->tz_dev)) {
			ret = PTR_ERR(tsen->tz_dev);
			dev_err(&pdev->dev, "Thermal zone register failed\n");
			return ret;
		}
	}

	return ump96xx_tsensor_disable(regmap, base);
}

static const struct of_device_id ump96xx_tsen_of_match[] = {
	{ .compatible = "sprd,ump9622-tsensor",},
	{ }
};

static struct platform_driver ump96xx_tsen_driver = {
	.probe = ump96xx_tsen_probe,
	.driver = {
		.name = "ump96xx-tsensor",
		.of_match_table = ump96xx_tsen_of_match,
	},
};

module_platform_driver(ump96xx_tsen_driver);

MODULE_DESCRIPTION("Spreadtrum UMP96xx tsensor driver");
MODULE_LICENSE("GPL v2");
