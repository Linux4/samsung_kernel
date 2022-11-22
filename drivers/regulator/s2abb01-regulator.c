/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/regulator/driver.h>
#include <linux/regmap.h>
#include <linux/module.h>
#include <linux/regulator/of_regulator.h>
#include <linux/of.h>
#include <linux/regulator/s2abb01-regulator.h>

static const struct regmap_config s2abb01_regmap_config = {
	.reg_bits   = 8,
	.val_bits   = 8,
	.cache_type = REGCACHE_NONE,
};

struct s2abb01_reg {
	struct device *dev;
	struct regulator_dev **rdev;
	int num_regulators;
	struct regmap *regmap;
};

static struct regulator_ops s2abb01_reg_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= regulator_is_enabled_regmap,
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
};

#define REGULATOR_DESC_LDO() {					\
	.name			= "s2abb01-ldo",		\
	.id			= S2ABB01_LDO,			\
	.ops			= &s2abb01_reg_ops,		\
	.type			= REGULATOR_VOLTAGE,		\
	.owner			= THIS_MODULE,			\
	.n_voltages		= 31,				\
	.min_uV			= LDO_MINUV,			\
	.uV_step		= LDO_STEP,			\
	.linear_min_sel		= 0x0E,				\
	.vsel_reg		= REG_LDO_CTRL,			\
	.vsel_mask		= BIT_OUT_L,			\
	.enable_reg		= REG_LDO_CTRL,			\
	.enable_mask		= BIT_LDO_EN,			\
}

#define REGULATOR_DESC_BUCK() {					\
	.name		= "s2abb01-buck",			\
	.id		= S2ABB01_BUCK,				\
	.ops		= &s2abb01_reg_ops,			\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.n_voltages	= 81,					\
	.min_uV		= BUCK_MINUV,				\
	.uV_step	= BUCK_STEP,				\
	.linear_min_sel	= 0x10,					\
	.vsel_reg	= REG_BB_OUT,				\
	.vsel_mask	= BIT_VBB_CTRL,				\
	.enable_reg	= REG_BB_OUT,				\
	.enable_mask	= BIT_BB_EN,				\
}

static const struct regulator_desc s2abb01_reg_desc[] = {
	REGULATOR_DESC_LDO(),
	REGULATOR_DESC_BUCK(),
};

static int s2abb01_reg_set_adchg(struct s2abb01_reg *s2abb01_reg,
			struct s2abb01_regulator_pdata *pdata, int reg_id) {
	int rc, reg, val;
	//int test ;
	pr_info("+s2abb01_reg_set_adchg\n");
	/* TEMP: need to check on new board */
	return 0;


	switch (reg_id) {
	case S2ABB01_LDO:
		pr_info("S2ABB01_LDO\n");
		val = pdata->regulators[reg_id - 1].adchg ? BIT_DSCH_LDO : 0;
		reg = REG_LDO_DSCH;
		//pr_info("val:0x%X, reg:0x%X\n", val, reg);
		//regmap_read(s2abb01_reg->regmap, reg ,&test);
		//pr_info(">>[1] test:0x%X\n", test);
		rc = regmap_update_bits(s2abb01_reg->regmap,
					reg, BIT_DSCH_LDO, val);
	        //regmap_read(s2abb01_reg->regmap, reg ,&test);
                //pr_info(">>[2]test:0x%X\n", test);

		break;
	case S2ABB01_BUCK:
		pr_info("S2ABB01_BUCK\n");
		val = pdata->regulators[reg_id - 1].adchg ? BIT_DSCH_BB : 0;
		reg = REG_BB_CTRL;
		//pr_info("val:0x%X, reg:0x%X\n", val, reg);
 		//regmap_read(s2abb01_reg->regmap, reg ,&test);
                //pr_info(">>[3] test:0x%X\n", test);

		rc = regmap_update_bits(s2abb01_reg->regmap,
					reg, BIT_DSCH_BB, val);
 		//regmap_read(s2abb01_reg->regmap, reg ,&test);
                //pr_info(">>[4] test:0x%X\n", test);

		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int s2abb01_reg_set_alwayson( struct s2abb01_reg *s2abb01_reg, struct s2abb01_regulator_pdata *pdata, int reg_id )
{
	int rc, reg, val;

	rc = 0;
	switch( reg_id )
	{
	case S2ABB01_LDO:
		
	break;
	case S2ABB01_BUCK:
		val = pdata->regulators[reg_id -1].alwayson ? BIT_BB_EN : 0;
		reg = REG_BB_OUT;
		//rc = regmap_update_bits
#if defined(CONFIG_MACH_KELLYLTE_CHN_OPEN)
			rc = regmap_write( s2abb01_reg->regmap, reg, val | BUCK_ALWAYS_3_4V );
#else
			rc = regmap_write( s2abb01_reg->regmap, reg, val | BUCK_ALWAYS_3_2V );
#endif
		regmap_read(s2abb01_reg->regmap,REG_BB_OUT,&val);
                pr_info("##val:0x%X\n", val);

	break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

#ifdef CONFIG_OF
static struct s2abb01_regulator_pdata
*s2abb01_reg_parse_dt(struct device *dev) {
	struct device_node *nproot = dev->of_node;
	struct device_node *regulators_np, *reg_np;
	struct s2abb01_regulator_pdata *pdata;
	struct s2abb01_regulator_data *rdata;
	int i;
	int ret;

	if (unlikely(nproot == NULL))
		return ERR_PTR(-EINVAL);

	regulators_np = of_find_node_by_name(nproot, "regulators");
	if (unlikely(regulators_np == NULL)) {
		dev_err(dev, "regulators node not found\n");
		return ERR_PTR(-EINVAL);
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);

	/* count the number of regulators to be supported in pmic */
	pdata->num_regulators = of_get_child_count(regulators_np);

	rdata = devm_kzalloc(dev, sizeof(*rdata) *
				pdata->num_regulators, GFP_KERNEL);
	if (!rdata) {
		of_node_put(regulators_np);
		dev_err(dev, "could not allocate memory for regulator data\n");
		return ERR_PTR(-ENOMEM);
	}

	pdata->regulators = rdata;
	for_each_child_of_node(regulators_np, reg_np) {
		for (i = 0; i < ARRAY_SIZE(s2abb01_reg_desc); i++)
			if (!of_node_cmp(reg_np->name,
				s2abb01_reg_desc[i].name))
				break;

		if (i == ARRAY_SIZE(s2abb01_reg_desc)) {
			dev_warn(dev, "don't know how to configure regulator %s\n",
				 reg_np->name);
			continue;
		}

		rdata->initdata = of_get_regulator_init_data(dev, reg_np,
							s2abb01_reg_desc);
		rdata->of_node = reg_np;
		ret = of_property_read_u32(reg_np, "active-discharge-enable",
					&rdata->adchg);
		if (ret != 0)
			rdata->adchg = 1;

		ret = of_property_read_u32(reg_np, "regulator-always-on", &rdata->alwayson );
		if (ret != 0)
			rdata->alwayson = 1;
		rdata++;
	}
	of_node_put(regulators_np);

	return pdata;
}
#endif

static void s2abb01_destroy(struct s2abb01_reg *me)
{
	struct device *dev = me->dev;

	if (likely(me->regmap))
		regmap_exit(me->regmap);

	devm_kfree(dev, me);
}

static int s2abb01_regulator_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct s2abb01_regulator_pdata *pdata;
	struct s2abb01_reg *s2abb01_reg;
	struct regulator_config config = { };
	int rc, i, size;
	int test=0;

	pr_info("%s probe++\n", client->name);

	s2abb01_reg = devm_kzalloc(dev, sizeof(struct s2abb01_reg), GFP_KERNEL);
	if (unlikely(!s2abb01_reg))
		return -ENOMEM;

	i2c_set_clientdata(client, s2abb01_reg);

#ifdef CONFIG_OF
	pr_info("%s s2abb01_reg_parse_dt\n", client->name);
	pdata = s2abb01_reg_parse_dt(dev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
#else
	pdata = dev_get_platdata(dev);
#endif

	size = sizeof(struct regulator_dev *) * pdata->num_regulators;
	s2abb01_reg->rdev = devm_kzalloc(dev, size, GFP_KERNEL);
	if (!s2abb01_reg->rdev)
		return -ENOMEM;
	s2abb01_reg->dev = &client->dev;

	s2abb01_reg->regmap = devm_regmap_init_i2c(client,
					&s2abb01_regmap_config);

	if (unlikely(IS_ERR(s2abb01_reg->regmap))) {
		rc = PTR_ERR(s2abb01_reg->regmap);
		s2abb01_reg->regmap = NULL;
		pr_err("%s failed to init i2c regmap [%d]\n", client->name, rc);
		goto abort;
	}
	s2abb01_reg->num_regulators = pdata->num_regulators;

	pr_info("%s num_regulators : %d\n", client->name, s2abb01_reg->num_regulators);
	for (i = 0; i < pdata->num_regulators; i++) {
		config.dev = &client->dev;
		config.init_data = pdata->regulators[i].initdata;
		config.of_node = pdata->regulators[i].of_node;
		config.regmap = s2abb01_reg->regmap;
		config.driver_data = s2abb01_reg;

		s2abb01_reg->rdev[i] =
			regulator_register(&s2abb01_reg_desc[i], &config);

		if (IS_ERR(s2abb01_reg->rdev[i])) {
			rc = PTR_ERR(s2abb01_reg->rdev[i]);
			pr_err("%s init failed for %d\n", client->name, i);
			s2abb01_reg->rdev[i] = NULL;
			goto abort;
		}

		rc = s2abb01_reg_set_adchg(s2abb01_reg, pdata,
						s2abb01_reg_desc[i].id);
		if (IS_ERR_VALUE(rc))
			goto abort;

		rc = s2abb01_reg_set_alwayson(s2abb01_reg, pdata, s2abb01_reg_desc[i].id);	
		if (IS_ERR_VALUE(rc))
                        goto abort;


		//test = 0xD8;
		//regmap_write( s2abb01_reg->regmap, REG_BB_OUT, test );
 		//regmap_read(s2abb01_reg->regmap,REG_BB_OUT,&test);
                //pr_info("##[3]0x%X\n", test);

	}

	/* TODO: voltage setting??? LDO disable??? */

	/* Turn on BB_EN again to make sure */
	pr_info("%s Enable BB_EN again\n", client->name);
	regmap_read(s2abb01_reg->regmap, REG_BB_OUT, &test);
	pr_info("%s Before : 0x%x\n", client->name, test);
	test |= BIT_BB_EN;
	regmap_write(s2abb01_reg->regmap, REG_BB_OUT, test);
	regmap_read(s2abb01_reg->regmap, REG_BB_OUT, &test);
	pr_info("%s After : 0x%x\n", client->name, test);

	pr_info("%s probe--\n", client->name);

	return 0;

abort:
	i2c_set_clientdata(client, NULL);
	s2abb01_destroy(s2abb01_reg);
	return rc;
}


static int s2abb01_regulator_remove(struct i2c_client *client)
{
	struct s2abb01_reg *s2abb01_reg = i2c_get_clientdata(client);
	int i;

	for (i = 0; i < s2abb01_reg->num_regulators; i++)
		regulator_unregister(s2abb01_reg->rdev[i]);

	i2c_set_clientdata(client, NULL);
	s2abb01_destroy(s2abb01_reg);

	return 0;
}

static void s2abb01_regulator_shutdown(struct i2c_client *client)
{
	struct s2abb01_reg *s2abb01_reg = i2c_get_clientdata(client);
	int test;

	/* Disable BB_EN before shutdown */
	regmap_read(s2abb01_reg->regmap, REG_BB_OUT, &test);
	test &= ~BIT_BB_EN;
	regmap_write(s2abb01_reg->regmap, REG_BB_OUT, test);
}

#ifdef CONFIG_OF
static const struct of_device_id s2abb01_of_id[] = {
	{ .compatible = "samsung,s2abb01" },
	{ },
};
MODULE_DEVICE_TABLE(of, s2abb01_of_id);
#endif /* CONFIG_OF */

static const struct i2c_device_id s2abb01_i2c_id[] = {
	{ "s2abb01", 0	},
	{ },
};
MODULE_DEVICE_TABLE(i2c, s2abb01_i2c_id);

static struct i2c_driver s2abb01_i2c_driver = {
	.driver.name            = "s2abb01",
	.driver.owner           = THIS_MODULE,
#ifdef CONFIG_OF
	.driver.of_match_table  = s2abb01_of_id,
#endif /* CONFIG_OF */
	.driver.suppress_bind_attrs = true,
	.id_table               = s2abb01_i2c_id,
	.probe                  = s2abb01_regulator_probe,
	.remove                 = s2abb01_regulator_remove,
	.shutdown				= s2abb01_regulator_shutdown,
};

static __init int s2abb01_init(void)
{
	int rc = -ENODEV;

	rc = i2c_add_driver(&s2abb01_i2c_driver);

	if (rc != 0)
		pr_err("Failed to register I2C driver: %d\n", rc);

	return rc;
}
subsys_initcall(s2abb01_init);

static __exit void s2abb01_exit(void)
{
	i2c_del_driver(&s2abb01_i2c_driver);
}
module_exit(s2abb01_exit);

MODULE_DESCRIPTION("Regulator driver for S2ABB01");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
