/* SPDX-License-Identifier: GPL-2.0 */

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>

#include "dd.h"

static DEFINE_MUTEX(regmap_mutex);

#undef pr_fmt
#define pr_fmt(fmt) "smcdsd: %s: " fmt, __func__

//#define dbg_none
#define dbg_none(fmt, ...)		pr_debug(fmt, ##__VA_ARGS__)
//#define dbg_info(fmt, ...)		pr_info(fmt, ##__VA_ARGS__)
#define dbg_init(fmt, ...)		pr_info(fmt, ##__VA_ARGS__)

#define DTS_LCDTYPE		"lcdtype"
#define DTS_BLICTYPE		"blictype"
#define DTS_REG_MATCH		"reg_match"

#define DTS_REG_BITS		"reg_bits"
#define DTS_VAL_BITS		"val_bits"
#define DTS_MAX_REGISTER	"max_register"
#define DTS_BIG_LOCK		"big_lock"

extern unsigned int lcdtype;
extern unsigned int blictype;

static inline int get_boot_lcdtype(void)
{
	return (int)lcdtype;
}

static inline unsigned int get_boot_lcdconnected(void)
{
	return get_boot_lcdtype() ? 1 : 0;
}

static inline int get_boot_blictype(void)
{
	return (int)blictype;
}

static const struct regmap_config regmap_config_default = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int __of_cmp_type(struct device_node *np, const char *prop_name, unsigned int target_value)
{
	int i = 0, count = 0, ret = -EINVAL;
	u32 match_info[10] = {0, };
	unsigned int match_index, mask, expect;

	count = of_property_count_u32_elems(np, prop_name);
	if (count < 0 || count > ARRAY_SIZE(match_info) || count % 2) {
		dbg_init("%dth dts(%s) has invalid %s count(%d)\n", i, np->name, prop_name, count);
		return NOTIFY_DONE;
	}

	memset(match_info, 0, sizeof(match_info));
	ret = of_property_read_u32_array(np, prop_name, match_info, count);
	if (ret < 0) {
		dbg_init("of_property_read_u32_array fail. ret(%d)\n", ret);
		return NOTIFY_DONE;
	}

	for (match_index = 0; match_index < count; match_index += 2) {
		mask = match_info[match_index];
		expect = match_info[match_index + 1];

		if ((target_value & mask) != (expect & mask)) {
			dbg_init("%dth dts(%s) %s mismatch. %s(%06X) mask(%06X) expect(%06X)\n",
				i, np->name, prop_name, prop_name, target_value, mask, expect);

			return NOTIFY_BAD;
		}
	}

	return NOTIFY_DONE;
}

static int of_cmp_lcdtype(struct device *parent,
		struct device_node *np, const char *prop_name)
{
	return __of_cmp_type(np, prop_name, get_boot_lcdtype());
}

static int of_cmp_blictype(struct device *parent,
		struct device_node *np, const char *prop_name)
{
	return __of_cmp_type(np, prop_name, get_boot_blictype());
}

static int of_cmp_reg_match(struct device *parent,
		struct device_node *np, const char *prop_name)
{
	int i = 0, count = 0, ret = -EINVAL;
	u32 match_info[10] = {0, };
	unsigned int match_index, addr, mask, expect;
	struct regmap *map = dev_get_regmap(parent, NULL);
	unsigned int target_value;

	if (!get_boot_lcdconnected())
		return NOTIFY_DONE;

	if (map) {
		dbg_init("dev_get_regmap fail\n");
		return NOTIFY_DONE;
	}

	count = of_property_count_u32_elems(np, prop_name);
	if (count < 0 || count > ARRAY_SIZE(match_info) || count % 3) {
		dbg_init("%dth dts(%s) has invalid %s count(%d)\n", i, np->name, prop_name, count);
		return NOTIFY_DONE;
	}

	memset(match_info, 0, sizeof(match_info));
	ret = of_property_read_u32_array(np, prop_name, match_info, count);
	if (ret < 0) {
		dbg_init("of_property_read_u32_array fail. ret(%d)\n", ret);
		return NOTIFY_DONE;
	}

	for (match_index = 0; match_index < count; match_index += 3) {
		addr = match_info[match_index];
		mask = match_info[match_index + 1];
		expect = match_info[match_index + 2];

		ret = regmap_read(map, addr, &target_value);
		if (ret < 0) {
			dbg_init("regmap_read fail addr(%2x) ret(%d)\n", addr, ret);
			return NOTIFY_BAD;
		}

		if ((target_value & mask) != (expect & mask)) {
			dbg_init("%dth dts(%s) %s mismatch. %s(%06X) mask(%06X) expect(%06X)\n",
				i, np->name, prop_name, prop_name, target_value, mask, expect);

			return NOTIFY_BAD;
		}
	}

	return NOTIFY_DONE;
}

struct __of_match_cb {
	const char *prop_name;
	int (*cb)(struct device *parent, struct device_node *np, const char *prop_name);
} of_match_cb[] = {
	{ DTS_LCDTYPE, of_cmp_lcdtype },
	{ DTS_BLICTYPE, of_cmp_blictype},
	{ DTS_REG_MATCH, of_cmp_reg_match},
	{},
};

static int __of_match_condition(struct device *parent,
		struct device_node *np, struct property *prop)
{
	int i;
	int ret;

	for (i = 0; of_match_cb[i].prop_name; i++) {
		if (of_prop_cmp(of_match_cb[i].prop_name, prop->name))
			continue;

		ret = of_match_cb[i].cb(parent, np, of_match_cb[i].prop_name);
		if (ret)
			return ret;
	}

	return 0;
}

static void smcdsd_regmap_lock(void *dev)
{
	//struct regmap *map = dev_get_regmap(data, NULL);
	struct i2c_client *client = to_i2c_client(dev);

	i2c_lock_bus(client->adapter, I2C_LOCK_ROOT_ADAPTER);
	mutex_lock(&regmap_mutex);
}

static void smcdsd_regmap_unlock(void *dev)
{
	//struct regmap *map = dev_get_regmap(data, NULL);
	struct i2c_client *client = to_i2c_client(dev);

	mutex_unlock(&regmap_mutex);
	i2c_unlock_bus(client->adapter, I2C_LOCK_ROOT_ADAPTER);
}

static struct regmap_config *of_get_regmap_config(struct device *parent,
		struct device_node *np)
{
	struct regmap_config *regmap_cfg = devm_kmemdup(parent,
		&regmap_config_default, sizeof(struct regmap_config), GFP_KERNEL);

	of_property_read_s32(np, DTS_REG_BITS, &regmap_cfg->reg_bits);
	of_property_read_s32(np, DTS_VAL_BITS, &regmap_cfg->val_bits);
	of_property_read_u32(np, DTS_MAX_REGISTER, &regmap_cfg->max_register);

	return regmap_cfg;
}

static int of_check_child_want_big_lock(struct device *parent)
{
	struct device_node *child;
	struct device_node *root = parent->of_node;
	int big_lock = 0;

	for_each_available_child_of_node(root, child) {
		if (of_find_property(child, DTS_BIG_LOCK, NULL)) {
			big_lock = 1;
			break;
		}
	};

	return big_lock;
}

static int of_lcd_child_device_create(struct device *parent)
{
	struct device_node *root = parent->of_node;
	struct device_node *child;
	struct property *prop;
	struct platform_device *pdev;
	int skip;

	for_each_available_child_of_node(root, child) {
		skip = 0;

		for_each_property_of_node(child, prop) {
			if (__of_match_condition(parent, child, prop) < 0) {
				skip = 1;
				break;
			}
		}

		if (skip) {
			of_node_put(child);
			continue;
		}

		dbg_init("%pOF\n", child);

		pdev = of_platform_device_create(child, NULL, parent);
		if (!pdev) {
			dbg_init("of_platform_device_create fail\n");
			of_node_put(child);
			break;
		}
	}

	of_node_set_flag(root, OF_POPULATED_BUS);

	of_node_put(root);

	return 0;
}

static int generic_lcd_regmap_i2c_probe(struct i2c_client *i2c)
{
	struct regmap_config *regmap_cfg;
	struct regmap *regmap_map;
	int big_lock = 0;
	struct device *root = &i2c->dev;
	struct i2c_client *clients[] = {i2c, NULL};

	dbg_init("starting at: %pOF\n", root->of_node);

	big_lock = of_check_child_want_big_lock(&i2c->dev);

	regmap_cfg = of_get_regmap_config(&i2c->dev, i2c->dev.of_node);

	if (big_lock) {
		regmap_cfg->lock = smcdsd_regmap_lock;
		regmap_cfg->unlock = smcdsd_regmap_unlock;
		regmap_cfg->lock_arg = &i2c->dev;
	}

	regmap_map = devm_regmap_init_i2c(i2c, regmap_cfg);
	if (IS_ERR(regmap_map))
		return PTR_ERR(regmap_map);

	init_debugfs_backlight(NULL, NULL, clients);

	return of_lcd_child_device_create(root);
}

static const struct of_device_id generic_lcd_regmap_of_match[] = {
	{ .compatible = "samsung,generic_lcd_regmap" },
	{}
};
MODULE_DEVICE_TABLE(of, generic_lcd_regmap_of_match);

static struct i2c_driver generic_lcd_regmap_i2c_driver = {
	.probe_new = generic_lcd_regmap_i2c_probe,
	.driver = {
		.name = "generic_lcd_regmap",
		.of_match_table = generic_lcd_regmap_of_match,
	},
};
builtin_i2c_driver(generic_lcd_regmap_i2c_driver);

static int generic_lcd_parent_probe(struct platform_device *pdev)
{
	struct device *root = &pdev->dev;

	dbg_init("%pOF\n", root->of_node);

	return of_lcd_child_device_create(root);
}

static const struct of_device_id generic_lcd_device_of_match[] = {
	{ .compatible = "samsung,generic_lcd_device" },
	{}
};
MODULE_DEVICE_TABLE(of, generic_lcd_device_of_match);

static struct platform_driver generic_lcd_device_driver = {
	.probe = generic_lcd_parent_probe,
	.driver = {
		.name = "generic_lcd_device",
		.of_match_table = generic_lcd_device_of_match,
	},
};
builtin_platform_driver(generic_lcd_device_driver);

static int i2c_clean_uncompatible_child(struct device *dev, void *addrp)
{
	struct i2c_client *client;

	if (!i2c_verify_client(dev))
		return NOTIFY_DONE;

	client = to_i2c_client(dev);
	if (!client)
		return NOTIFY_DONE;

	if (client->addr != *(unsigned int *)addrp)
		return NOTIFY_DONE;

	if (i2c_of_match_device(generic_lcd_regmap_of_match, client))
		return NOTIFY_DONE;

	dbg_init("i2c_unregister_device(%s) addr(%2x) %pOF\n",
		dev_name(dev), *(unsigned int *)addrp, dev->of_node);

	i2c_unregister_device(client);

	return NOTIFY_STOP;
}

int generic_lcd_i2c_cleanup(void)
{
	const struct of_device_id *of_id;
	struct device_node *np = NULL;
	struct device_node *parent_np;
	struct i2c_adapter *adapter;
	u32 addr;
	int ret;
	struct i2c_client *client;
	struct i2c_board_info info;

	for_each_matching_node_and_match(np, generic_lcd_regmap_of_match, &of_id) {
		if (!of_device_is_available(np)) {
			of_node_put(np);
			continue;
		}

		ret = of_property_read_u32(np, "reg", &addr);
		if (ret) {
			of_node_put(np);
			continue;
		}

		parent_np = of_get_parent(np);
		of_node_put(np);

		adapter = of_find_i2c_adapter_by_node(parent_np);
		of_node_put(parent_np);

		dbg_none("%pOF, adapter_id(%d) adapter_depth(%d)\n", np,
			i2c_adapter_id(adapter), i2c_adapter_depth(adapter));

		ret = device_for_each_child(&adapter->dev, &addr, i2c_clean_uncompatible_child);
		if (!ret)
			continue;

		ret = of_i2c_get_board_info(&adapter->dev, np, &info);
		if (ret)
			return ret;

		client = i2c_new_device(adapter, &info);
		if (!client) {
			dbg_init("i2c_new_device fail %pOF\n", np);
			return -EINVAL;
		}
	}

	return 0;
}
EXPORT_SYMBOL(generic_lcd_i2c_cleanup);
