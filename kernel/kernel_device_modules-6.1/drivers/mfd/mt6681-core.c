// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>

#include <linux/mfd/mt6681.h>
#include <linux/mfd/mt6681-private.h>

struct pinctrl *codec_pinctrl;

enum mt6681_codec_gpio {
	MT6681_CODEC_GPIO_EN_ON,
	MT6681_CODEC_GPIO_EN_OFF,
	MT6681_CODEC_GPIO_NUM
};

struct codec_gpio_attr {
	const char *name;
	bool gpio_prepare;
	struct pinctrl_state *gpioctrl;
};
static struct codec_gpio_attr codec_gpios[MT6681_CODEC_GPIO_NUM] = {
	[MT6681_CODEC_GPIO_EN_ON] = {"codec-en-on", false, NULL},
	[MT6681_CODEC_GPIO_EN_OFF] = {"codec-en-off", false, NULL},
};

bool mt6681_probe_done;
EXPORT_SYMBOL_GPL(mt6681_probe_done);

#define MT6681_MFD_CELL(_name)					\
	{							\
		.name = #_name,					\
		.of_compatible = "mediatek," #_name,		\
	}

static bool mt6681_is_volatile_reg(struct device *dev, unsigned int reg)
{
	return true;
}

static struct regmap_config mt6681_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = MT6681_MAX_REGISTER,

	.cache_type = REGCACHE_FLAT,
	.volatile_reg = mt6681_is_volatile_reg,
};

static int mt6681_codec_gpio_select(struct mt6681_pmic_info *mpi,
				  enum mt6681_codec_gpio type)
{
	int ret = 0;

	if (type >= MT6681_CODEC_GPIO_NUM) {
		dev_info(mpi->dev, "%s(), error, invalid gpio type %d\n",
			__func__, type);
		return -EINVAL;
	}

	if (!codec_gpios[type].gpio_prepare) {
		dev_info(mpi->dev, "%s(), error, gpio type %d not prepared\n",
			 __func__, type);
		return -EIO;
	}

	ret = pinctrl_select_state(codec_pinctrl,
				   codec_gpios[type].gpioctrl);
	if (ret)
		dev_info(mpi->dev, "%s(), error, can not set gpio type %d\n",
			__func__, type);

	return ret;
}

int mt6681_codec_gpio_init(struct mt6681_pmic_info *mpi)
{
	int ret;
	int i = 0;

	codec_pinctrl = devm_pinctrl_get(mpi->dev);
	if (IS_ERR(codec_pinctrl)) {
		ret = PTR_ERR(codec_pinctrl);
		dev_info(mpi->dev, "%s(), ret %d, cannot get codec_pinctrl!\n",
			__func__, ret);
		return -ENODEV;
	}

	for (i = 0; i < ARRAY_SIZE(codec_gpios); i++) {
		codec_gpios[i].gpioctrl = pinctrl_lookup_state(codec_pinctrl,
					codec_gpios[i].name);
		if (IS_ERR(codec_gpios[i].gpioctrl)) {
			ret = PTR_ERR(codec_gpios[i].gpioctrl);
			dev_info(mpi->dev, "%s(), pinctrl_lookup_state %s fail, ret %d\n",
				__func__, codec_gpios[i].name, ret);
		} else
			codec_gpios[i].gpio_prepare = true;
	}

	/* gpio status init */
	mt6681_codec_gpio_select(mpi, MT6681_CODEC_GPIO_EN_ON);

	return 0;
}


static const struct mfd_cell mt6681_devs[] = {
	MT6681_MFD_CELL(mt6681-accdet),
	MT6681_MFD_CELL(mt6681-auxadc),
	MT6681_MFD_CELL(mt6681-sound),
	MT6681_MFD_CELL(mt6681-efuse),
#if IS_ENABLED(CONFIG_REGULATOR_MT6681)
	MT6681_MFD_CELL(mt6681-regulator),
#endif
	/* debug dev */
	/* { .name = "mt6360_dbg", },*/
};
static int retry_total;

#ifdef DEBUG_DUMP
unsigned int dump_reg[] = {
	MT6681_HWCID_H, MT6681_SWCID_L, MT6681_SWCID_H, MT6681_TOP_CON,
	MT6681_TOP0_ID_H, MT6681_TOP1_ID_H, MT6681_TOP2_ID_H,
	MT6681_TOP3_ID_H, MT6681_TOP_RST_REG_ID_H,
	MT6681_PLT0_ID_DIG_ID, MT6681_I2CRECORD_ID_DIG_ID};
static int mt6681_debug_read(struct mt6681_pmic_info *mpi)
{
	int i = 0, ret = 0;
	unsigned int data = 0;

	for (i = 0; i < ARRAY_SIZE(dump_reg); i++) {
		ret = regmap_read(mpi->regmap, dump_reg[i], &data);
		dev_info(mpi->dev, "reg (0x%x) = 0x%x, ret %d\n",
				 dump_reg[i], data, ret);
	}
	return 0;
}
#endif
static int mt6681_retry_read(struct mt6681_pmic_info *mpi)
{
	int i = 0, ret = 0;
	unsigned int data = 0;

	while ((i < 3) && (data != MT6681_SWCID_H_CODE)) {
		ret = regmap_read(mpi->regmap, MT6681_SWCID_H, &data);
		dev_info(mpi->dev, "retry (%d) MT6681_SWCID_H = 0x%x, ret %d\n",
				 i, data, ret);

		retry_total++;
		i++;
	}
	if (data != MT6681_SWCID_H_CODE)
		return -1;
	else
		return 0;
}
static int mt6681_retry_wait(struct mt6681_pmic_info *mpi)
{
	int i = 0, ret = 0;
	unsigned int data = 0;
	struct timespec64 ts64;
	unsigned long long t1, t2 = 0;
	/* one interrupt period = 100ms */
	unsigned long long timeout_limit = 50000000;

	ktime_get_ts64(&ts64);
	t1 = timespec64_to_ns(&ts64);

	while ((data & 0x2) != 0x2) {
		if (t2 > timeout_limit){
			dev_info(mpi->dev, "retry wait Timeout!!!\n");
			break;
		}
		/* MT6681_ACDIF_MON0[1] : MON_DA_QI_INTERNAL_RSTB */
		ret = regmap_read(mpi->regmap, MT6681_ACDIF_MON0, &data);
		dev_info(mpi->dev, "retry wait for RDY (%d) MT6681_ACDIF_MON0 = 0x%x, ret %d\n",
				 i, data, ret);
		i++;
		ktime_get_ts64(&ts64);
		t2 = timespec64_to_ns(&ts64);
		t2 = t2 - t1; /* in ns (10^9) */
	}
	return i;
}

static int mt6681_check_id(struct mt6681_pmic_info *mpi)
{
	int ret = 0, wait = 0;
	unsigned int data = 0;
	retry_total = 0;

	ret = regmap_read(mpi->regmap, MT6681_SWCID_H, &data);
	if (ret < 0) {
		dev_info(mpi->dev, "device not found %d\n", ret);
		//return ret;
	}
	if (data != MT6681_SWCID_H_CODE) {
		dev_info(mpi->dev, "data = %d, not mt6681 chip\n", data);
		//return -ENODEV;
		wait = mt6681_retry_wait(mpi);
		/* Step 1: retry test */
#ifdef DEBUG_DUMP
		mt6681_debug_read(mpi);
#endif
		ret = mt6681_retry_read(mpi);
		dev_info(mpi->dev, "total retry (%d) MT6681_SWCID_H, ready wait time %d, ret %d\n",
			 retry_total, ret, wait);
		/*Step 2: mt6681 reset */
		if ( ret < 0) {
			mt6681_codec_gpio_select(mpi, MT6681_CODEC_GPIO_EN_OFF);
			mt6681_codec_gpio_select(mpi, MT6681_CODEC_GPIO_EN_ON);
			wait = mt6681_retry_wait(mpi);
			ret = mt6681_retry_read(mpi);
			dev_info(mpi->dev, "total retry (%d) MT6681_SWCID_H, ready wait time %d, ret %d\n",
				 retry_total, ret, wait);

		}

		if (ret < 0) {
			dev_info(mpi->dev, "total retry (%d) MT6681_SWCID_H, ready wait time %d, ret %d\n",
				 retry_total, ret, wait);
			BUG_ON(1);
		}
	}
	mpi->chip_rev = data;

	return 0;
}

void mt6681_Keyunlock(struct mt6681_pmic_info *mpi)
{
	regmap_write(mpi->regmap, MT6681_TOP_DIG_WPK, 0x81);
	regmap_write(mpi->regmap, MT6681_TOP_DIG_WPK_H, 0x66);
	regmap_write(mpi->regmap, MT6681_TOP_TMA_KEY, 0x7e);
	regmap_write(mpi->regmap, MT6681_TOP_TMA_KEY_H, 0x99);
	regmap_write(mpi->regmap, MT6681_PSC_WPK_L, 0x29);
	regmap_write(mpi->regmap, MT6681_PSC_WPK_H, 0x47);
	regmap_write(mpi->regmap, MT6681_HK_TOP_WKEY_L, 0x81);
	regmap_write(mpi->regmap, MT6681_HK_TOP_WKEY_H, 0x66);
}

void mt6681_Keylock(struct mt6681_pmic_info *mpi)
{
	regmap_write(mpi->regmap, MT6681_TOP_DIG_WPK, 0x0);
	regmap_write(mpi->regmap, MT6681_TOP_DIG_WPK_H, 0x0);
	regmap_write(mpi->regmap, MT6681_TOP_TMA_KEY, 0x0);
	regmap_write(mpi->regmap, MT6681_TOP_TMA_KEY_H, 0x0);
	regmap_write(mpi->regmap, MT6681_PSC_WPK_L, 0x0);
	regmap_write(mpi->regmap, MT6681_PSC_WPK_H, 0x0);
	regmap_write(mpi->regmap, MT6681_HK_TOP_WKEY_L, 0x0);
	regmap_write(mpi->regmap, MT6681_HK_TOP_WKEY_H, 0x0);
}
void mt6681_LP_Setting(struct mt6681_pmic_info *mpi)
{
#ifdef LP_SETTING
	/*---Turn ON hardware clock DCM mode to save more power---*/
	regmap_update_bits(mpi->regmap,
				MT6681_LDO_VAUD18_CON2,
				RG_LDO_VAUD18_CK_SW_MODE_MASK_SFT,
				0x0 << RG_LDO_VAUD18_CK_SW_MODE_MASK_SFT);

	/*---LP Voltage Set---*/
	/*---Using PMRC_EN[15:0] in DVT---*/
	/*---HW0 (SRCLKEN0), HW1 (SRCLKEN1), SCP_VAO (SSHUB/VOW)---*/
	regmap_update_bits(mpi->regmap,
				MT6681_PMRC_CON1,
				RG_VR_SPM_MODE_MASK_SFT,
				0x1 << RG_VR_SPM_MODE_SFT);

	/* Change PAD_PAD_SRCLKEN_IN0 into SW mode */
	regmap_update_bits(mpi->regmap,
				MT6681_TOP_CON,
				RG_SRCLKEN_IN_HW_MODE_MASK_SFT,
				0x0 << RG_SRCLKEN_IN_HW_MODE_SFT);
	regmap_update_bits(mpi->regmap,
				MT6681_TOP_CON,
				RG_SRCLKEN_IN_EN_MASK_SFT,
				0x1 << RG_SRCLKEN_IN_EN_SFT);

	/*---Multi-User---*/
	regmap_update_bits(mpi->regmap,
				MT6681_LDO_VAUD18_MULTI_SW_0,
				RG_LDO_VAUD18_EN_1_MASK_SFT,
				0x0 << RG_LDO_VAUD18_EN_1_SFT);
	regmap_update_bits(mpi->regmap,
				MT6681_LDO_VAUD18_MULTI_SW_1,
				RG_LDO_VAUD18_EN_2_MASK_SFT,
				0x0 << RG_LDO_VAUD18_EN_2_SFT);
#endif
}

void mt6681_Suspend_Setting(struct mt6681_pmic_info *mpi)
{
	regmap_write(mpi->regmap, MT6681_MTC_CTL0, 0x10);
	regmap_write(mpi->regmap, MT6681_MTC_CTL0, 0x11);
	regmap_write(mpi->regmap, MT6681_MTC_CTL0, 0x13);

	regmap_write(mpi->regmap, MT6681_DA_INTF_STTING3, 0x08);

	regmap_write(mpi->regmap, MT6681_LDO_VAUD18_CON2, 0x1C);
	regmap_write(mpi->regmap, MT6681_LDO_VAUD18_OP_EN0, 0x01);
	regmap_write(mpi->regmap, MT6681_LDO_VAUD18_OP_CFG0, 0x01);

	regmap_write(mpi->regmap, MT6681_DA_INTF_STTING1, 0x64);
	regmap_write(mpi->regmap, MT6681_DA_INTF_STTING1, 0x66);
	regmap_write(mpi->regmap, MT6681_DA_INTF_STTING1, 0x76);
}

void mt6681_InitSetting(struct mt6681_pmic_info *mpi)
{

	regmap_write(mpi->regmap, MT6681_TOP_CON, 0x7);
	regmap_write(mpi->regmap, MT6681_TEST_CON0, 0x1f);
	regmap_write(mpi->regmap, MT6681_SMT_CON0, 0x3);
	regmap_write(mpi->regmap, MT6681_GPIO_PULLEN0, 0xf9);
	regmap_write(mpi->regmap, MT6681_GPIO_PULLEN1, 0x1f);
	regmap_write(mpi->regmap, MT6681_TOP_CKPDN_CON0, 0x5b);
	regmap_write(mpi->regmap, MT6681_HK_TOP_CLK_CON0, 0x15);

	regmap_write(mpi->regmap, MT6681_DA_INTF_STTING3, 0x0c);
	regmap_write(mpi->regmap, MT6681_MTC_CTL0, 0x13);
	regmap_write(mpi->regmap, MT6681_PLT_CON0, 0x0);
	regmap_write(mpi->regmap, MT6681_PLT_CON1, 0x0);

	regmap_write(mpi->regmap, MT6681_HK_TOP_CLK_CON0, 0x15);
	regmap_write(mpi->regmap, MT6681_AUXADC_CON0, 0x0);
	regmap_write(mpi->regmap, MT6681_AUXADC_TRIM_SEL2, 0x40);

	regmap_write(mpi->regmap, MT6681_TOP_TOP_CKHWEN_CON0, 0x0f);
	regmap_write(mpi->regmap, MT6681_LDO_TOP_CLK_DCM_CON0, 0x01);
	regmap_write(mpi->regmap, MT6681_LDO_TOP_VR_CLK_CON0, 0x00);
	regmap_write(mpi->regmap, MT6681_LDO_VAUD18_CON2, 0x1c);
	regmap_write(mpi->regmap, MT6681_PLT_CON0, 0x3a);
	regmap_write(mpi->regmap, MT6681_PLT_CON1, 0x0c);
}

static const unsigned short mt6681_slave_addr = MT6681_PMIC_SLAVEID;

static int mt6681_pmic_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct mt6681_pmic_info *mpi;
	struct regmap_config *regmap_config = &mt6681_regmap_config;
	int ret;
	mt6681_probe_done = false;

	dev_info(&client->dev, "+%s()\n", __func__);

	mpi = devm_kzalloc(&client->dev, sizeof(*mpi), GFP_KERNEL);
	if (!mpi)
		return -ENOMEM;
	mpi->i2c = client;
	mpi->dev = &client->dev;
	i2c_set_clientdata(client, mpi);
	mutex_init(&mpi->io_lock);

	dev_info(&client->dev, "+%s() mutex_init\n", __func__);
	mt6681_codec_gpio_init(mpi);

	/* regmap regiser */
	regmap_config->lock_arg = &mpi->io_lock;
	mpi->regmap = devm_regmap_init_i2c(client, regmap_config);
	if (IS_ERR(mpi->regmap)) {
		dev_info(&client->dev, "regmap register fail\n");
		return PTR_ERR(mpi->regmap);
	}
	/* chip id check */
	ret = mt6681_check_id(mpi);
	if (ret < 0) {
		dev_info(&client->dev, "mt6681_check_id fail, return 0\n");
		return ret;
	}

	/* mfd cell register */
	ret = devm_mfd_add_devices(&client->dev, PLATFORM_DEVID_NONE,
				   mt6681_devs, ARRAY_SIZE(mt6681_devs), NULL,
				   0, NULL);
	if (ret < 0) {
		dev_info(&client->dev, "mfd add cells fail\n");
		goto out;
	}
	dev_info(&client->dev, "execute InitSetting\n");

	/* initial setting */
	mt6681_Keyunlock(mpi);
	mt6681_LP_Setting(mpi);
	mt6681_InitSetting(mpi);
	mt6681_Suspend_Setting(mpi);

	dev_info(&client->dev, "Successfully probed\n");
	mt6681_probe_done = true;
	return 0;
out:
	i2c_unregister_device(mpi->i2c);

	return ret;
}

static void mt6681_pmic_remove(struct i2c_client *client)
{
	struct mt6681_pmic_info *mpi = i2c_get_clientdata(client);

	i2c_unregister_device(mpi->i2c);
}

static const struct of_device_id __maybe_unused mt6681_pmic_of_id[] = {
	{ .compatible = "mediatek,mt6681_pmic", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6681_pmic_of_id);

static const struct i2c_device_id mt6681_pmic_id[] = {
	{ "mt6681_pmic", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, mt6681_pmic_id);

static struct i2c_driver mt6681_pmic_driver = {
	.driver = {
		.name = "mt6681_pmic",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mt6681_pmic_of_id),
	},
	.probe = mt6681_pmic_probe,
	.remove = mt6681_pmic_remove,
	.id_table = mt6681_pmic_id,
};
module_i2c_driver(mt6681_pmic_driver);

MODULE_AUTHOR("Ting-Fang Hou<ting-fang.hou@mediatek.com>");
MODULE_DESCRIPTION("MT6681 PMIC I2C Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
