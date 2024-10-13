/*
 * sound/soc/sprd/codec/sprd/v3/sprd-audio-power.c
 *
 * SPRD-AUDIO-POWER -- SpreadTrum intergrated audio power supply.
 *
 * Copyright (C) 2013 SpreadTrum Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include "sprd-asoc-debug.h"
#define pr_fmt(fmt) pr_sprd_fmt("POWER") fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

#include <mach/sprd-audio.h>
#include "sprd-audio-power.h"
#include "sprd-asoc-common.h"

#ifdef CONFIG_AUDIO_POWER_ALLOW_UNSUPPORTED
#define UNSUP_MASK	0x0000
#else
#define UNSUP_MASK	0x8000
#endif

#define UNSUP(x)	(UNSUP_MASK | (x))
#define IS_UNSUP(x)	(UNSUP_MASK & (x))
#define LDO_MV(x)	(~UNSUP_MASK & (x))

struct sprd_audio_power_info {
	int id;
	/* enable/disable register information */
	int en_reg;
	int en_bit;
	int (*power_enable) (void);
	int (*power_disable) (void);
	/* configure deepsleep cut off or not */
	int (*sleep_ctrl) (void *, int);
	/* voltage level register information */
	int v_reg;
	int v_mask;
	int v_shift;
	int v_table_len;
	const u16 *v_table;
	int min_uV;
	int max_uV;

	/* unit: us */
	int on_delay;
	int off_delay;

	/* used by regulator core */
	struct regulator_desc desc;

	void *data;
};

#define SPRD_AUDIO_POWER_LDO_LOW(label, r_supply, d_id, \
e_reg, e_bit, p_enable, p_disable, s_ctrl, \
reg, mask, shift, table, table_size, d_on_delay, d_off_delay) \
static struct regulator_consumer_supply label[] = { \
	{ .supply =	#label },\
}; \
static struct regulator_init_data label##_data = { \
	.supply_regulator = r_supply, \
	.constraints = { \
		.name = "SRG_" #label, \
	}, \
	.num_consumer_supplies = ARRAY_SIZE(label), \
	.consumer_supplies = label, \
}; \
static struct sprd_audio_power_info SPRD_AUDIO_POWER_INFO_##label = { \
	.id = d_id, \
	.en_reg = e_reg, \
	.en_bit = e_bit, \
	.power_enable = p_enable, \
	.power_disable = p_disable, \
	.sleep_ctrl = s_ctrl, \
	.v_reg = reg, \
	.v_mask = mask, \
	.v_shift = shift, \
	.v_table_len = table_size, \
	.v_table = table, \
	.on_delay = d_on_delay, \
	.off_delay = d_off_delay, \
	.desc = { \
		.name = #label, \
		.id = SPRD_AUDIO_POWER_##label, \
		.n_voltages = table_size, \
		.ops = &sprd_audio_power_ops, \
		.type = REGULATOR_VOLTAGE, \
		.owner = THIS_MODULE, \
	}, \
}

#define SPRD_AUDIO_POWER_LDO(label, r_supply, id, en_reg, en_bit, sleep_ctrl, \
		v_reg, v_mask, v_shift, v_table, on_delay, off_delay) \
SPRD_AUDIO_POWER_LDO_LOW(label, r_supply, id, en_reg, en_bit, 0, 0, sleep_ctrl, \
		v_reg, v_mask, v_shift, v_table, ARRAY_SIZE(v_table), on_delay, off_delay)

#define SPRD_AUDIO_POWER_SIMPLE_LDO(label, r_supply, id, en_reg, en_bit, sleep_ctrl, \
		on_delay, off_delay) \
SPRD_AUDIO_POWER_LDO_LOW(label, r_supply, id, en_reg, en_bit, 0, 0, sleep_ctrl, \
		0, 0, 0, 0, 0, on_delay, off_delay)

#define SPRD_AUDIO_POWER_REG_LDO(label, id, power_enable, power_disable) \
SPRD_AUDIO_POWER_LDO_LOW(label, 0, id, 0, 0, power_enable, power_disable, \
		0, 0, 0, 0, 0, 0, 0, 0)

static inline int sprd_power_read(struct sprd_audio_power_info *info, int reg)
{
	return arch_audio_power_read(reg);
}

static inline int
sprd_power_write(struct sprd_audio_power_info *info, int reg, int val)
{
	int ret = 0;
	ret = arch_audio_power_write(reg, val);
	sp_asoc_pr_reg("[0x%04x] W:[0x%08x] R:[0x%08x]\n", reg & 0xFFFF, val,
		       arch_audio_power_read(reg));
	return ret;
}

static inline int
sprd_power_u_bits(struct sprd_audio_power_info *info,
		  unsigned int reg, unsigned int mask, unsigned int value)
{
	int ret = 0;
	ret = arch_audio_power_write_mask(reg, value, mask);
	sp_asoc_pr_reg("[0x%04x] U:[0x%08x] R:[0x%08x]\n", reg & 0xFFFF, value,
		       arch_audio_power_read(reg));
	return ret;
}

static int sprd_audio_power_is_enabled(struct regulator_dev *rdev)
{
	int is_enable;
	struct sprd_audio_power_info *info = rdev_get_drvdata(rdev);

	sp_asoc_pr_dbg("%s Is Enabled\n", rdev->desc->name);

	if (info->en_reg) {
		is_enable = sprd_power_read(info, info->en_reg) & info->en_bit;
	} else {
		is_enable = info->en_bit;
	}

	return ! !is_enable;
}

static int audio_power_set_voltage(struct regulator_dev *rdev, int min_uV,
				   int max_uV)
{
	struct sprd_audio_power_info *info = rdev_get_drvdata(rdev);
	int vsel;

	BUG_ON(!info->v_table);

	for (vsel = 0; vsel < info->v_table_len; vsel++) {
		int mV = info->v_table[vsel];
		int uV;

		if (IS_UNSUP(mV))
			continue;
		uV = LDO_MV(mV) * 1000;

		/* use the first in-range value */
		if (min_uV <= uV && uV <= max_uV) {
			sp_asoc_pr_dbg("%s: %dus\n", __func__, uV);
			sprd_power_u_bits(info,
					  info->v_reg,
					  info->v_mask << info->v_shift,
					  vsel << info->v_shift);

			return vsel;
		}
	}
	return -EDOM;
}

static int sprd_audio_power_enable(struct regulator_dev *rdev)
{
	struct sprd_audio_power_info *info = rdev_get_drvdata(rdev);
	int ret = 0;

	sp_asoc_pr_dbg("%s Enable\n", rdev->desc->name);

	if (info->en_reg) {
		ret = sprd_power_u_bits(info,
					info->en_reg, info->en_bit,
					info->en_bit);
	} else {
		if (info->power_enable)
			ret = info->power_enable();
		info->en_bit = 1;
	}

	udelay(info->on_delay);

	if (info->v_table) {
		if (info->min_uV || info->max_uV)
			audio_power_set_voltage(rdev, info->min_uV,
						info->max_uV);
	}
	return ret;
}

static int sprd_audio_power_disable(struct regulator_dev *rdev)
{
	struct sprd_audio_power_info *info = rdev_get_drvdata(rdev);
	int ret = 0;

	sp_asoc_pr_dbg("%s Disable\n", rdev->desc->name);

	if (info->en_reg) {
		ret = sprd_power_u_bits(info, info->en_reg, info->en_bit, 0);
	} else {
		if (info->power_disable)
			ret = info->power_disable();
		info->en_bit = 0;
	}

	udelay(info->off_delay);

	return ret;
}

static int sprd_audio_power_get_status(struct regulator_dev *rdev)
{
	sp_asoc_pr_dbg("%s Get Status\n", rdev->desc->name);

	if (!sprd_audio_power_is_enabled(rdev))
		return REGULATOR_STATUS_OFF;
	return REGULATOR_STATUS_NORMAL;
}

static int sprd_audio_power_set_mode(struct regulator_dev *rdev, unsigned mode)
{
	int ret = 0;
	struct sprd_audio_power_info *info = rdev_get_drvdata(rdev);

	sp_asoc_pr_dbg("%s Set Mode 0x%x\n", rdev->desc->name, mode);

	if (!info->sleep_ctrl) {
		return ret;
	}

	switch (mode) {
	case REGULATOR_MODE_NORMAL:
		ret = info->sleep_ctrl(info, 0);
		break;
	case REGULATOR_MODE_STANDBY:
		ret = info->sleep_ctrl(info, 1);
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static const u16 VCOM_VSEL_table[] = {
	2900, 3100, 3200, 3300,
	3400, 3500, 3600, 3800,
};

static const u16 VPA_VSEL_table[] = {
	2900, 3100, 3200, 3300,
	3400, 3500, 3600, 3800,
};

static const u16 VMIC_VSEL_table[] = {
	1700, 2210, 1900, 2470,
	2100, 2730, 2300, 3000,
};

static const u16 VBG_IBAIS_VSEL_table[] = {
	150, 200, 250, 300,
};

static const u16 HIB_VSEL_table[] = {
	1000, 950, 900, 850, 800, 750,
	700, 650, 600, 550, 500, 450, 400,
};

static int sprd_audio_power_list_voltage(struct regulator_dev *rdev,
					 unsigned index)
{
	struct sprd_audio_power_info *info = rdev_get_drvdata(rdev);
	int mV;

	sp_asoc_pr_dbg("%s List Voltage\n", rdev->desc->name);

	if (!info->v_table) {
		return 0;
	}

	if (index >= info->v_table_len) {
		return 0;
	}

	mV = info->v_table[index];
	return IS_UNSUP(mV) ? 0 : (LDO_MV(mV) * 1000);
}

static int
sprd_audio_power_set_voltage(struct regulator_dev *rdev, int min_uV, int max_uV,
			     unsigned *selector)
{
	struct sprd_audio_power_info *info = rdev_get_drvdata(rdev);
	int vsel;

	sp_asoc_pr_info("%s Set Voltage %d--%d\n", rdev->desc->name, min_uV,
			max_uV);

	if (!info->v_table) {
		return 0;
	}
	info->min_uV = min_uV;
	info->max_uV = max_uV;

	vsel = audio_power_set_voltage(rdev, min_uV, max_uV);
	if (vsel != -EDOM)
		*selector = vsel;

	return vsel;
}

static int sprd_audio_power_get_voltage(struct regulator_dev *rdev)
{
	struct sprd_audio_power_info *info = rdev_get_drvdata(rdev);
	int vsel;

	sp_asoc_pr_dbg("%s Get Voltage\n", rdev->desc->name);

	if (!info->v_table) {
		return 0;
	}

	vsel = (sprd_power_read(info, info->v_reg) & info->v_mask)
	    >> info->v_shift;
	return LDO_MV(info->v_table[vsel]) * 1000;
}

static struct regulator_ops sprd_audio_power_ops = {
	.list_voltage = sprd_audio_power_list_voltage,

	.set_voltage = sprd_audio_power_set_voltage,
	.get_voltage = sprd_audio_power_get_voltage,

	.enable = sprd_audio_power_enable,
	.disable = sprd_audio_power_disable,
	.is_enabled = sprd_audio_power_is_enabled,

	.set_mode = sprd_audio_power_set_mode,

	.get_status = sprd_audio_power_get_status,
};

static int sprd_audio_power_sleep_ctrl(void *data, int en)
{
	struct sprd_audio_power_info *info = data;
	int reg, mask, value;
	switch (info->desc.id) {
	case SPRD_AUDIO_POWER_HEADMICBIAS:
		reg = PMUR2_PMUR1;
		mask = BIT(HEADMIC_SLEEP_EN);
		value = en ? mask : 0;
		return sprd_power_u_bits(info, reg, mask, value);
		break;
	default:
		BUG_ON(0);
	}
	return 0;
}

static inline int vreg_enable(void)
{
	int ret = 0;
	static int i = 0;
	arch_audio_codec_switch(AUDIO_TO_ARM_CTRL);
	arch_audio_codec_analog_reg_enable();
	if (i  == 0) {
		arch_audio_codec_analog_reset();
		/*set initial value from xun.zhang*/
		arch_audio_codec_write_mask(PMUR4_PMUR3, BIT(ICM_PLUS_EN),
					BIT(ICM_PLUS_EN));
		i = 1;
	}
	return ret;
}

static inline int vreg_disable(void)
{
	/*arch_audio_codec_reset();*/
	arch_audio_codec_analog_reg_disable();
	return 0;
}

/*
 * We list regulators here if systems need some level of
 * software control over them after boot.
 */
SPRD_AUDIO_POWER_REG_LDO(VREG, 1, vreg_enable, vreg_disable);
SPRD_AUDIO_POWER_SIMPLE_LDO(VB, "VREG", 2, PMUR2_PMUR1, BIT(VB_EN), NULL, 0, 0);
SPRD_AUDIO_POWER_SIMPLE_LDO(BG, "VB", 3, PMUR4_PMUR3, BIT(BG_EN), NULL, 0, 0);
SPRD_AUDIO_POWER_LDO(BG_IBIAS, "BG", 4, PMUR4_PMUR3, BIT(BG_IBIAS_EN), NULL,
		     PMUR4_PMUR3, BG_I_MASK, BG_I, VBG_IBAIS_VSEL_table, 0, 0);
SPRD_AUDIO_POWER_LDO(VCOM, "BG_IBIAS", 5, PMUR4_PMUR3, BIT(VCM_EN), NULL,
		     PMUR4_PMUR3, VCM_V_MASK, VCM_V, VCOM_VSEL_table, 0, 0);
SPRD_AUDIO_POWER_SIMPLE_LDO(VCOM_BUF, "VCOM", 6, PMUR4_PMUR3, BIT(VCM_BUF_EN),
			    NULL, 0, 0);
SPRD_AUDIO_POWER_SIMPLE_LDO(VBO, "VB", 7, PMUR2_PMUR1, BIT(VBO_EN), NULL, 0, 0);
SPRD_AUDIO_POWER_LDO(VMICBIAS, "VCOM", 8, 0, 0, NULL,
		     PMUR2_PMUR1, MICBIAS_V_MASK, MICBIAS_V, VMIC_VSEL_table, 0,
		     0);
SPRD_AUDIO_POWER_SIMPLE_LDO(MICBIAS, "VMICBIAS", 9, PMUR4_PMUR3,
			    BIT(MICBIAS_EN), NULL, 0, 0);
SPRD_AUDIO_POWER_SIMPLE_LDO(AUXMICBIAS, "VMICBIAS", 10, PMUR4_PMUR3,
			    BIT(AUXMICBIAS_EN), NULL, 0, 0);
SPRD_AUDIO_POWER_LDO(HEADMICBIAS, "VMICBIAS", 11, PMUR2_PMUR1,
		     BIT(HEADMICBIAS_EN), sprd_audio_power_sleep_ctrl,
		     HIBDR2_HIBDR1, HIB_SBUT_MASK, HIB_SBUT, HIB_VSEL_table, 0,
		     0);

#define SPRD_OF_MATCH(comp, label) \
	{ \
		.compatible = comp, \
		.data = &SPRD_AUDIO_POWER_INFO_##label, \
	}

static const struct of_device_id sprd_audio_power_of_match[] = {
	SPRD_OF_MATCH("sp,audio-vreg", VREG),
	SPRD_OF_MATCH("sp,audio-vb", VB),
	SPRD_OF_MATCH("sp,audio-bg", BG),
	SPRD_OF_MATCH("sp,audio-bg-ibias", BG_IBIAS),
	SPRD_OF_MATCH("sp,audio-vcom", VCOM),
	SPRD_OF_MATCH("sp,audio-vcom-buf", VCOM_BUF),
	SPRD_OF_MATCH("sp,audio-vbo", VBO),
	SPRD_OF_MATCH("sp,audio-vmicbias", VMICBIAS),
	SPRD_OF_MATCH("sp,audio-micbias", MICBIAS),
	SPRD_OF_MATCH("sp,audio-auxmicbias", AUXMICBIAS),
	SPRD_OF_MATCH("sp,audio-headmicbias", HEADMICBIAS),
};

static int sprd_audio_power_probe(struct platform_device *pdev)
{
	int i, id;
	struct sprd_audio_power_info *info;
	const struct sprd_audio_power_info *template;
	struct regulator_init_data *initdata;
	struct regulation_constraints *c;
	struct regulator_dev *rdev;
	struct regulator_config config = { };
	int min_index = 0;
	int max_index = 0;

	id = pdev->id;

	sp_asoc_pr_info("Probe %d\n", id);

	initdata = pdev->dev.platform_data;
	for (i = 0, template = NULL; i < ARRAY_SIZE(sprd_audio_power_of_match);
	     i++) {
		template = sprd_audio_power_of_match[i].data;
		if (!template || template->desc.id != id)
			continue;
		break;
	}

	if (!template)
		return -ENODEV;

	if (!initdata)
		return -EINVAL;

	info = kmemdup(template, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	/* Constrain board-specific capabilities according to what
	 * this driver and the chip itself can actually do.
	 */
	c = &initdata->constraints;
	c->valid_modes_mask |= REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY;
	c->valid_ops_mask |= REGULATOR_CHANGE_VOLTAGE
	    | REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS;

	if (info->v_table) {
		for (i = 0; i < info->v_table_len; i++) {
			if (info->v_table[i] < info->v_table[min_index])
				min_index = i;
			if (info->v_table[i] > info->v_table[max_index])
				max_index = i;
		}
		c->min_uV = LDO_MV((info->v_table)[min_index]) * 1000;
		c->max_uV = LDO_MV((info->v_table)[max_index]) * 1000;
		sp_asoc_pr_info("min_uV:%d, max_uV:%d\n", c->min_uV, c->max_uV);
	}
	info->min_uV = 0;
	info->max_uV = 0;

	config.dev = &pdev->dev;
	config.init_data = initdata;
	config.driver_data = info;
	config.of_node = pdev->dev.of_node;

	rdev = regulator_register(&info->desc, &config);
	if (IS_ERR(rdev)) {
		dev_err(&pdev->dev, "ERR:Can't Register %s, %ld\n",
			info->desc.name, PTR_ERR(rdev));
		kfree(info);
		return PTR_ERR(rdev);
	}
	sp_asoc_pr_info("Register %s Success!\n", info->desc.name);

	platform_set_drvdata(pdev, rdev);

	return 0;
}

static int sprd_audio_power_remove(struct platform_device *pdev)
{
	struct regulator_dev *rdev = platform_get_drvdata(pdev);
	struct twlreg_info *info = rdev->reg_data;

	regulator_unregister(platform_get_drvdata(pdev));
	kfree(info);
	return 0;
}

static struct platform_driver sprd_audio_power_driver = {
	.driver = {
		   .name = "sprd-audio-power",
		   .owner = THIS_MODULE,
		   },
	.probe = sprd_audio_power_probe,
	.remove = sprd_audio_power_remove,
};

static int __init sprd_audio_power_regulator_init(void)
{
	return platform_driver_register(&sprd_audio_power_driver);
}

postcore_initcall(sprd_audio_power_regulator_init);

static void __exit sprd_audio_power_regulator_exit(void)
{
	platform_driver_unregister(&sprd_audio_power_driver);
}

module_exit(sprd_audio_power_regulator_exit);

MODULE_DESCRIPTION("SPRD audio power regulator driver");
MODULE_AUTHOR("Ken Kuang <ken.kuang@spreadtrum.com>");
MODULE_ALIAS("platform:sprd-audio-power");
MODULE_LICENSE("GPL");

#define SPRD_AUDIO_DEVICE(label) \
{ \
	.name = "sprd-audio-power", \
	.id = SPRD_AUDIO_POWER_##label, \
	.dev = { \
		.platform_data = &label##_data, \
	}, \
}

static struct platform_device sprd_audio_regulator_devices[] = {
	SPRD_AUDIO_DEVICE(VREG),
	SPRD_AUDIO_DEVICE(VB),
	SPRD_AUDIO_DEVICE(BG),
	SPRD_AUDIO_DEVICE(BG_IBIAS),
	SPRD_AUDIO_DEVICE(VCOM),
	SPRD_AUDIO_DEVICE(VCOM_BUF),
	SPRD_AUDIO_DEVICE(VBO),
	SPRD_AUDIO_DEVICE(VMICBIAS),
	SPRD_AUDIO_DEVICE(MICBIAS),
	SPRD_AUDIO_DEVICE(AUXMICBIAS),
	SPRD_AUDIO_DEVICE(HEADMICBIAS),
};

#ifdef CONFIG_OF
static int sprd_audio_power_version(void)
{
	struct device_node *node = of_find_node_by_path("/sprd-audio-devices");
	struct device_node *child;
	if (node) {
		u32 val;
		for_each_child_of_node(node, child) {
			if (!of_property_read_u32
			    (child, "sprd,audio_power_ver", &val)) {
				sp_asoc_pr_dbg
				    ("Configure Audio Power Version is %d\n",
				     val);
				return val;
			}
		}
		pr_err("ERR:Not found sprd,audio_power_ver!\n");
	} else {
		pr_err("ERR:No sprd-audio-devices Node!\n");
	}
	return 0;
}
#else
int sprd_audio_power_version(void)
    __attribute__ ((weak, alias("__sprd_audio_power_version")));

static int __sprd_audio_power_version(void)
{
	return 3;
}
#endif

static int sprd_audio_power_add_devices(void)
{
	int ret = 0;
	int i;
	if (3 != sprd_audio_power_version()) {
		return 0;
	}
	for (i = 0; i < ARRAY_SIZE(sprd_audio_regulator_devices); i++) {
		ret =
		    platform_device_register(&sprd_audio_regulator_devices[i]);
		if (ret < 0) {
			pr_err("ERR:Add Regulator %d Failed %d\n",
			       sprd_audio_regulator_devices[i].id, ret);
			return ret;
		}
	}
	return ret;
}

static int __init sprd_audio_power_init(void)
{
	return sprd_audio_power_add_devices();
}

postcore_initcall(sprd_audio_power_init);
