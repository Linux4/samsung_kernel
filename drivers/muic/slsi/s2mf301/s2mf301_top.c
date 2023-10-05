/*
 * s2mf301_top.c - S2MF301 Power Meter Driver
 *
 * Copyright (C) 2016 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <linux/mfd/slsi/s2mf301/s2mf301.h>
#include <linux/muic/slsi/s2mf301/s2mf301_top.h>
#include <linux/muic/slsi/s2mf301/s2mf301-muic.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>

#if IS_ENABLED(CONFIG_MUIC_S2MF301_RID)
static int s2mf301_top_set_jig_on(void *_data, bool enable);
static int s2mf301_top_mask_rid_change(void *_data, bool enable);
struct s2mf301_top_rid_ops _s2mf301_top_rid_ops = {
	.set_jig_on			= s2mf301_top_set_jig_on,
	.mask_rid_change	= s2mf301_top_mask_rid_change,
};
#endif

static enum power_supply_property s2mf301_top_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int s2mf301_top_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct s2mf301_top_data *top = power_supply_get_drvdata(psy);
	enum power_supply_lsi_property lsi_prop = (enum power_supply_lsi_property) psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("[DEBUG]%s: POWER_SUPPLY_PROP_ONLINE\n", __func__);
		return 1;
	case POWER_SUPPLY_LSI_PROP_MIN ... POWER_SUPPLY_LSI_PROP_MAX:
		switch (lsi_prop) {
		case POWER_SUPPLY_LSI_PROP_RID_OPS:
#if IS_ENABLED(CONFIG_MUIC_S2MF301_RID)
			top->muic_data = (struct s2mf301_muic_data *)val->strval;
			_s2mf301_top_rid_ops._data = top;
			val->strval = (const char *)&_s2mf301_top_rid_ops;
#endif
			break;
		default:
			return -EINVAL;
		}
		return 0;
	default:
		return -EINVAL;
	}
	return 0;
}

static int s2mf301_top_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("[DEBUG]%s: POWER_SUPPLY_PROP_ONLINE\n", __func__);
		return 1;
	default:
		return -EINVAL;
	}
	return 0;
}

static const struct of_device_id s2mf301_top_match_table[] = {
	{ .compatible = "samsung,s2mf301-top",},
	{},
};

static void s2mf301_top_init_reg(struct s2mf301_top_data *top)
{
#if !IS_ENABLED(CONFIG_MUIC_S2MF301_RID)
	u8 data;
#endif

#if !IS_ENABLED(CONFIG_MUIC_S2MF301_RID)
	s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_AUTO_PPS_SETTING, &data);
	data &= ~(0x80);
	s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_TOPINT_MASK, data);
	s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_TOP_PM_RID_INT_MASK, 0xff);
#endif
	s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_TOP_TC_RID_INT_MASK, 0xff);
}

#if IS_ENABLED(CONFIG_MUIC_S2MF301_RID)
static irqreturn_t s2mf301_top_rid_isr(int irq, void *data)
{
	struct s2mf301_top_data *top = data;
	int ret = 0;

	pr_info("%s, \n", __func__);

	if (!top->muic_data) {
		pr_info("%s, muic_data is NULL\n", __func__);
		return IRQ_HANDLED;
	}

	if (top->muic_data->rid_isr)
		ret = top->muic_data->rid_isr(top->muic_data);
	else {
		pr_info("%s, top->muic_data->rid_isr is NULL\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

static int s2mf301_top_mask_rid_change(void *_data, bool enable)
{
	struct s2mf301_top_data *top = _data;
	u8 reg_data = 0;

	pr_info("%s, enable(%d)\n", __func__, enable);
	s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_TOPINT_MASK, &reg_data);
	if (enable)
		reg_data |= (1 << 7);
	else
		reg_data &= ~(1 << 7);
	s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_TOPINT_MASK, reg_data);

	if (enable)
		s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_TOP_PM_RID_INT_MASK, 0xff);
	else
		s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_TOP_PM_RID_INT_MASK, 0x00);

	return 0;
}

static int s2mf301_top_set_jig_on(void *_data, bool enable)
{
	struct s2mf301_top_data *top = _data;
	u8 reg_data = 0;

	pr_info("%s, en(%s)\n", __func__, enable ? "ENABLE" : "DISABLE");

	s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_JIG_CTRL, &reg_data);
	if (enable)
		reg_data |= S2MF301_TOP_REG_JIG_MANUAL_LOW_MASK;
	else
		reg_data &= ~S2MF301_TOP_REG_JIG_MANUAL_LOW_MASK;
	s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_JIG_CTRL, reg_data);

	return 0;
}
#endif

static int s2mf301_top_probe(struct platform_device *pdev)
{
	struct s2mf301_dev *s2mf301 = dev_get_drvdata(pdev->dev.parent);
	struct s2mf301_top_data *top;
	struct power_supply_config psy_cfg = {};
	int ret = 0;

	pr_info("%s:[BATT] S2MF301 TOP driver probe\n", __func__);
	top = kzalloc(sizeof(struct s2mf301_top_data), GFP_KERNEL);
	if (!top)
		return -ENOMEM;

	top->dev = &pdev->dev;
	/* need 0x74 i2c */
	top->i2c = s2mf301->i2c;

	s2mf301_top_init_reg(top);

	platform_set_drvdata(pdev, top);

	top->psy_top_desc.name			= "s2mf301-top";
	top->psy_top_desc.type			= POWER_SUPPLY_TYPE_UNKNOWN;
	top->psy_top_desc.get_property	= s2mf301_top_get_property;
	top->psy_top_desc.set_property	= s2mf301_top_set_property;
	top->psy_top_desc.properties	= s2mf301_top_props;
	top->psy_top_desc.num_properties = ARRAY_SIZE(s2mf301_top_props);

	psy_cfg.drv_data = top;

	top->psy_pm = power_supply_register(&pdev->dev, &top->psy_top_desc, &psy_cfg);
	if (IS_ERR(top->psy_pm)) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		ret = PTR_ERR(top->psy_pm);
		goto err_power_supply_register;
	}

#if IS_ENABLED(CONFIG_MUIC_S2MF301_RID)
	top->irq_rid_attach = s2mf301->pdata->irq_base + S2MF301_TOP_PM_RID_IRQ_RID_ATTACH;
	ret = request_threaded_irq(top->irq_rid_attach, NULL, s2mf301_top_rid_isr, 0, "rid_attach_isr", top);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__, top->irq_rid_attach, ret);

	top->irq_rid_detach = s2mf301->pdata->irq_base + S2MF301_PM_ADC_CHANGE_INT4_PMOFF;
	ret = request_threaded_irq(top->irq_rid_detach, NULL, s2mf301_top_rid_isr, 0, "rid_detach_isr", top);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__, top->irq_rid_attach, ret);

	top->irq_rid_255k = s2mf301->pdata->irq_base + S2MF301_TOP_PM_RID_IRQ_RID_255K;
	ret = request_threaded_irq(top->irq_rid_255k, NULL, s2mf301_top_rid_isr, 0, "rid_255k_isr", top);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__, top->irq_rid_255k, ret);

	top->irq_rid_301k = s2mf301->pdata->irq_base + S2MF301_TOP_PM_RID_IRQ_RID_301K;
	ret = request_threaded_irq(top->irq_rid_301k, NULL, s2mf301_top_rid_isr, 0, "rid_301k_isr", top);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__, top->irq_rid_301k, ret);

	top->irq_rid_523k = s2mf301->pdata->irq_base + S2MF301_TOP_PM_RID_IRQ_RID_523K;
	ret = request_threaded_irq(top->irq_rid_523k, NULL, s2mf301_top_rid_isr, 0, "rid_523k_isr", top);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__, top->irq_rid_523k, ret);

	top->irq_rid_619k = s2mf301->pdata->irq_base + S2MF301_TOP_PM_RID_IRQ_RID_619K;
	ret = request_threaded_irq(top->irq_rid_619k, NULL, s2mf301_top_rid_isr, 0, "rid_619k_isr", top);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__, top->irq_rid_619k, ret);
#endif

	pr_info("%s:[BATT] S2MF301 TOP driver loaded OK\n", __func__);

	return ret;

err_power_supply_register:
	kfree(top);

	return ret;
}

static int s2mf301_top_remove(struct platform_device *pdev)
{
	struct s2mf301_top_data *top = platform_get_drvdata(pdev);

	kfree(top);
	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int s2mf301_top_suspend(struct device *dev)
{
	return 0;
}

static int s2mf301_top_resume(struct device *dev)
{
	return 0;
}
#else
#define s2mf301_top_suspend NULL
#define s2mf301_top_resume NULL
#endif

static void s2mf301_top_shutdown(struct platform_device *pdev)
{
	pr_info("%s: S2MF301 TOP driver shutdown\n", __func__);
}

static SIMPLE_DEV_PM_OPS(s2mf301_top_pm_ops, s2mf301_top_suspend,
		s2mf301_top_resume);

static struct platform_driver s2mf301_top_driver = {
	.driver		 = {
		.name = "s2mf301-top",
		.owner = THIS_MODULE,
		.of_match_table	= s2mf301_top_match_table,
		.pm = &s2mf301_top_pm_ops,
	},
	.probe		= s2mf301_top_probe,
	.remove		= s2mf301_top_remove,
	.shutdown	= s2mf301_top_shutdown,
};

static int __init s2mf301_top_init(void)
{
	pr_info("%s\n", __func__);
	return platform_driver_register(&s2mf301_top_driver);
}
module_init(s2mf301_top_init);

static void __exit s2mf301_top_exit(void)
{
	platform_driver_unregister(&s2mf301_top_driver);
}
module_exit(s2mf301_top_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("TOP driver for S2MF301");
