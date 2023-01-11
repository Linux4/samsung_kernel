/*
 * drivers/video/mmp/panel/mipi_panel.c
 * active panel using DSI interface to do init
 *
 * Copyright (C) 2013 Marvell Technology Group Ltd.
 * Authors: Yonghai_huang<huangyh@marvell.com>
 *		Xiaolong Ye<yexl@marvell.com>
 *          Yu Xu <yuxu@marvell.com>
 *          Guoqing Li <ligq@marvell.com>
 *          Lisa Du <cldu@marvell.com>
 *          Zhou Zhu <zzhu3@marvell.com>
 *          Jing Xiang <jxiang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <video/mmp_disp.h>
#include <video/mipi_display.h>
#include <video/mmp_esd.h>

#define MIPI_DCS_NORMAL_STATE 0x9c

static char pkt_size_cmd[] = {0x1};
static char read_status[] = {0x0a};

static struct mmp_dsi_cmd_desc panel_read_status_cmds[] = {
	{MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE, 0, 0, sizeof(pkt_size_cmd),
		pkt_size_cmd},
	{MIPI_DSI_DCS_READ, 0, 0, sizeof(read_status), read_status},
};

#ifdef CONFIG_OF
enum {
	EXT_PIN_GPIO = 0,
	EXT_PIN_REGULATOR = 1,
};

struct ext_pin {
	const char *name;
	u32 type;
	u32 value;
	union {
		int gpio;
		struct regulator *supply;
	};
};

struct ext_pins {
	struct ext_pin *pins;
	unsigned int nr_pins;
};

struct ext_pin_ctrl {
	const char *name;
	u32 on;
	u32 usec;
};

struct ext_pin_ctrls {
	struct ext_pin_ctrl *ctrls;
	unsigned int nr_ctrls;
};
#endif

struct panel_plat_data {
	u32 id;
	int esd_enable;
	struct mmp_panel *panel;
	struct mmp_mode mode;
	struct mmp_dsi_cmds cmds;
	struct mmp_dsi_cmds enable_cmds;
	struct mmp_dsi_cmds disable_cmds;
	struct mmp_dsi_cmds brightness_cmds;
		/* external pins */
	struct ext_pins pins;
	struct ext_pin_ctrls power_boot;
	struct ext_pin_ctrls power_on;
	struct ext_pin_ctrls power_off;

	void (*plat_onoff)(int status);
	void (*plat_set_backlight)(struct mmp_panel *panel, int level);
};

static unsigned long current_panel_id;

static int __init early_read_panel_id(char *str)
{
	unsigned long id;
	int ret = 0;

	ret = kstrtoul(str, 0, &id);
	if (ret < 0) {
		pr_err("strtoul err.\n");
		return ret;
	}

	current_panel_id = id;

	return 1;
}
early_param("panel.id", early_read_panel_id);

#ifdef CONFIG_OF
static DEFINE_SPINLOCK(bl_lock);

static inline int of_property_read_u32_with_suffix(const struct device_node *np,
		const char *propname, const char *suffix, u32 *out_value)
{
	char *name;
	size_t len;
	int ret;

	len = strlen(propname) + strlen(suffix) + 1;
	name = kzalloc(sizeof(char) * len, GFP_KERNEL);
	if (unlikely(!name))
		return -ENOMEM;

	snprintf(name, len, "%s%s", propname, suffix);
	ret = of_property_read_u32(np, name, out_value);
	if (unlikely(ret < 0))
		pr_err("%s: failed to read property(%s)\n",
				__func__, name);

	kfree(name);
	return ret;
}

static inline int of_property_read_string_with_suffix(struct device_node *np,
		const char *propname, const char *suffix,
		const char **out_string)
{
	char *name;
	size_t len;
	int ret;

	len = strlen(propname) + strlen(suffix) + 1;
	name = kzalloc(sizeof(char) * len, GFP_KERNEL);
	if (unlikely(!name))
		return -ENOMEM;

	snprintf(name, len, "%s%s", propname, suffix);
	ret = of_property_read_string(np, name, out_string);
	if (unlikely(ret))
		pr_err("%s: failed to read property(%s)\n",
				__func__, name);

	kfree(name);
	return ret;
}

static int mmp_dsi_panel_parse_video_mode(
		struct device_node *np, struct panel_plat_data *plat_data)
{
	int ret = 0;

	ret = of_property_read_u32(np, "marvell,dsi-panel-xres",
			&plat_data->mode.xres);
	ret = of_property_read_u32(np, "marvell,dsi-panel-yres",
			&plat_data->mode.yres);
	ret = of_property_read_u32(np, "marvell,dsi-panel-xres",
			&plat_data->mode.real_xres);
	ret = of_property_read_u32(np, "marvell,dsi-panel-yres",
			&plat_data->mode.real_yres);
	ret = of_property_read_u32(np, "marvell,dsi-panel-width",
			&plat_data->mode.width);
	ret = of_property_read_u32(np, "marvell,dsi-panel-height",
			&plat_data->mode.height);
	ret = of_property_read_u32(np, "marvell,dsi-h-front-porch",
			&plat_data->mode.right_margin);
	ret = of_property_read_u32(np, "marvell,dsi-h-back-porch",
			&plat_data->mode.left_margin);
	ret = of_property_read_u32(np, "marvell,dsi-h-sync-width",
			&plat_data->mode.hsync_len);
	ret = of_property_read_u32(np, "marvell,dsi-v-front-porch",
			&plat_data->mode.lower_margin);
	ret = of_property_read_u32(np, "marvell,dsi-v-back-porch",
			&plat_data->mode.upper_margin);
	ret = of_property_read_u32(np, "marvell,dsi-v-sync-width",
			&plat_data->mode.vsync_len);

	if (ret)
		ret = -EINVAL;

	plat_data->mode.refresh = 60;
	plat_data->mode.pix_fmt_out = PIXFMT_BGR888PACK;
	plat_data->mode.hsync_invert = 0;
	plat_data->mode.vsync_invert = 0;

	plat_data->mode.pixclock_freq = plat_data->mode.refresh *
		(plat_data->mode.xres + plat_data->mode.right_margin +
		 plat_data->mode.left_margin + plat_data->mode.hsync_len) *
		(plat_data->mode.yres + plat_data->mode.upper_margin +
		 plat_data->mode.lower_margin + plat_data->mode.vsync_len);

	return ret;
}

static int mmp_dsi_panel_parse_dt_pin(struct device_node *np,
		struct device *dev, const char *propname, struct ext_pin *pin)
{
	size_t len;
	int ret;

	ret = of_property_read_string_with_suffix(np,
			propname, "-name", &pin->name);
	if (unlikely(ret))
		return -EINVAL;

	ret = of_property_read_u32_with_suffix(np,
			propname, "-type", &pin->type);
	if (unlikely(ret))
		return -EINVAL;

	if (pin->type == EXT_PIN_REGULATOR) {
		struct device_node *node;
		struct regulator *supply;
		char *supply_name;

		ret = of_property_read_u32_with_suffix(np,
				propname, "-value", &pin->value);
		if (unlikely(ret))
			pin->value = 0;

		len = strlen(propname) + strlen("-supply") + 1;
		supply_name = kmalloc(sizeof(char) * len, GFP_KERNEL);
		if (unlikely(!supply_name))
			return -ENOMEM;
		snprintf(supply_name, len, "%s-supply", propname);

		node = of_parse_phandle(np, supply_name, 0);
		if (!node) {
			pr_err("%s: failed to parse %s\n",
					__func__, supply_name);
			kfree(supply_name);
			return -EINVAL;
		}
		supply = regulator_get(dev, node->name);
		if (IS_ERR_OR_NULL(supply)) {
			pr_err("%s regulator(%s) get error!\n",
					__func__, node->name);
			of_node_put(node);
			kfree(supply_name);
			regulator_put(supply);
			return -EINVAL;
		}
		pin->supply = supply;
		pr_info("%s, get regulator(%s)\n", pin->name, node->name);
		of_node_put(node);
		kfree(supply_name);
	} else {
		char *gpio_name;
		int gpio;

		len = strlen(propname) + strlen("-gpio") + 1;
		gpio_name = kmalloc(sizeof(char) * len, GFP_KERNEL);
		if (unlikely(!gpio_name))
			return -ENOMEM;
		snprintf(gpio_name, len, "%s-gpio", propname);

		gpio = of_get_named_gpio(np, gpio_name, 0);
		if (unlikely(gpio < 0)) {
			pr_err("%s: of_get_named_gpio failed: %d\n",
					__func__, gpio);
			kfree(gpio_name);
			return -EINVAL;
		}
		pin->gpio = gpio;
		pr_info("%s, get gpio(%d)\n", pin->name, pin->gpio);
		kfree(gpio_name);
	}

	return 0;
}

static int mmp_dsi_panel_parse_dt_pin_ctrl(struct device_node *np,
		const char *propname,
		struct ext_pin_ctrls *out_ctrl)
{
	static struct ext_pin_ctrl_dt {
		phandle phandle;
		u32 on;
		u32 usec;
	} *ctrl_dt;
	struct ext_pin_ctrl *tctrl;
	struct device_node *node;
	u32 *arr;
	size_t size, nr_arr, nr_ctrl_dt;
	int i, ret;

	of_get_property(np, propname, &size);
	arr = kmalloc(sizeof(u32) * size, GFP_KERNEL);
	if (!arr)
		return -ENOMEM;

	nr_arr = size / sizeof(u32);
	of_property_read_u32_array(np, propname, arr, nr_arr);
	ctrl_dt = (struct ext_pin_ctrl_dt *)arr;
	nr_ctrl_dt = size / sizeof(struct ext_pin_ctrl_dt);

	tctrl = kzalloc(sizeof(struct ext_pin_ctrl) * nr_ctrl_dt , GFP_KERNEL);
	if (!tctrl)
		return -ENOMEM;

	for (i = 0; i < nr_ctrl_dt; i++) {
		node = of_find_node_by_phandle(ctrl_dt[i].phandle);
		if (unlikely(!node)) {
			pr_err("%s: failed to find node(%s)\n",
					__func__, propname);
			goto err_find_node;
		}
		ret = of_property_read_string(node,
				"panel-ext-pin-name", &tctrl[i].name);
		if (unlikely(ret)) {
			pr_err("%s: failed to read property(%s)\n",
					__func__, propname);
			goto err_find_node;
		}
		tctrl[i].on = ctrl_dt[i].on;
		tctrl[i].usec = ctrl_dt[i].usec;
		pr_info("[%d] %s, %s, %dusec\n", i, tctrl[i].name,
				tctrl[i].on ? "on" : "off", tctrl[i].usec);
		of_node_put(node);
	}
	out_ctrl->ctrls = tctrl;
	out_ctrl->nr_ctrls = nr_ctrl_dt;
	kfree(arr);
	return 0;

err_find_node:
	of_node_put(node);
	kfree(tctrl);
	kfree(arr);
	return -EINVAL;
}

static int mmp_dsi_panel_parse_external_pins(
		struct device_node *pin_node, struct device *dev,
		struct panel_plat_data *plat_data)
{
	struct device_node *np = NULL;
	struct ext_pin *pins;
	int nr_nodes = 0, i = 0;
	int ret;
	char *name;
	u32 len;

	if (!pin_node)
		return -EINVAL;

	len = strlen(pin_node->name) + strlen("-boot") + 1;
	name = kmalloc(sizeof(char) * len, GFP_KERNEL);
	if (unlikely(!name))
		return -ENOMEM;
	snprintf(name, len, "%s-boot", pin_node->name);
	mmp_dsi_panel_parse_dt_pin_ctrl(pin_node, name, &plat_data->power_boot);
	kfree(name);

	len = strlen(pin_node->name) + strlen("-on") + 1;
	name = kmalloc(sizeof(char) * len, GFP_KERNEL);
	if (unlikely(!name))
		return -ENOMEM;
	snprintf(name, len, "%s-on", pin_node->name);
	mmp_dsi_panel_parse_dt_pin_ctrl(pin_node, name, &plat_data->power_on);
	kfree(name);

	len = strlen(pin_node->name) + strlen("-off") + 1;
	name = kmalloc(sizeof(char) * len, GFP_KERNEL);
	if (unlikely(!name))
		return -ENOMEM;
	snprintf(name, len, "%s-off", pin_node->name);
	mmp_dsi_panel_parse_dt_pin_ctrl(pin_node, name, &plat_data->power_off);
	kfree(name);

	nr_nodes = of_get_child_count(pin_node);
	pins = kmalloc(sizeof(struct ext_pin) * nr_nodes, GFP_KERNEL);
	if (unlikely(!pins))
		return -ENOMEM;

	for_each_child_of_node(pin_node, np) {
		ret = mmp_dsi_panel_parse_dt_pin(np, dev,
				pin_node->name, &pins[i++]);
		if (unlikely(ret < 0)) {
			pr_err("%s: failed to parse %s node\n",
					__func__, pin_node->name);
			kfree(pins);
			return -EINVAL;
		}
	}
	plat_data->pins.pins = pins;
	plat_data->pins.nr_pins = nr_nodes;

	return 0;
}

static int mmp_dsi_panel_parse_cmds(struct device_node *np,
		const char *propname, struct mmp_dsi_cmds *dsi_cmds)
{
	struct mmp_dsi_cmd_hdr *dchdr;
	struct mmp_dsi_cmd_desc *cmds;
	const char *val;
	char *data;
	int sz = 0, i = 0, nr_cmds = 0;
	int nr;

	if (!dsi_cmds) {
		pr_err("%s: invalid dsi_cmds address\n", __func__);
		return -EINVAL;
	}

	if (!of_find_property(np, propname, NULL)) {
		pr_debug("%s: %s is not exist\n", __func__, propname);
		return -EINVAL;
	}

	val = of_get_property(np, propname, &sz);
	if (unlikely(!val)) {
		pr_err("%s: failed, key=%s\n", __func__, propname);
		return -ENOMEM;
	}

	data = kzalloc(sizeof(char) * sz , GFP_KERNEL);
	if (unlikely(!data))
		return -ENOMEM;

	memcpy(data, val, sz);

	/* scan dcs commands */
	while (sz > i + sizeof(*dchdr)) {
		dchdr = (struct mmp_dsi_cmd_hdr *)(data + i);
		dchdr->dlen = ntohs(dchdr->dlen);
		dchdr->wait = ntohs(dchdr->wait);
		if (i + dchdr->dlen > sz) {
			pr_err("%s: parse error dtsi cmd=0x%02x, len=0x%02x, nr_cmds=%d",
				__func__, dchdr->dtype, dchdr->dlen, nr_cmds);
			dchdr = NULL;
			kfree(data);
			return -ENOMEM;
		}
		i += sizeof(*dchdr) + dchdr->dlen;
		nr_cmds++;
	}

	if (unlikely(sz != i)) {
		pr_err("%s: dcs_cmd=%x len=%d error!",
				__func__, data[0], sz);
		kfree(data);
		return -ENOMEM;
	}

	dsi_cmds->cmds = kzalloc(nr_cmds * sizeof(struct mmp_dsi_cmd_desc),
						GFP_KERNEL);
	dsi_cmds->nr_cmds = nr_cmds;
	if (unlikely(!dsi_cmds->cmds)) {
		kfree(data);
		return -ENOMEM;
	}

	cmds = dsi_cmds->cmds;
	for (i = 0, nr = 0; nr < nr_cmds; nr++) {
		dchdr = (struct mmp_dsi_cmd_hdr *)(data + i);
		cmds[nr].data_type = dchdr->dtype;
		cmds[nr].lp = dchdr->mode;
		cmds[nr].delay = dchdr->wait;
		cmds[nr].length = dchdr->dlen;
		cmds[nr].data = &data[i + sizeof(*dchdr)];
		i += sizeof(*dchdr) + dchdr->dlen;
		pr_info("type:%x, %x, %d ms, %d bytes, %02Xh\n",
				cmds[nr].data_type, cmds[nr].lp,
				cmds[nr].delay, cmds[nr].length,
				(cmds[nr].data)[0]);
	}
	pr_info("parse %s done!\n", propname);
	return 0;
}

static struct ext_pin *
mmp_dsi_panel_find_ext_pin(struct ext_pins *pins, const char *name)
{
	int i;
	for (i = 0; i < pins->nr_pins; i++) {
		if (!strcmp(pins->pins[i].name, name))
			return &pins->pins[i];
	}

	return NULL;
}

static int mmp_dsi_panel_ext_pin_ctrl(struct mmp_panel *panel,
		struct ext_pin *pin, int on)
{
	int rc;

	if (on) {
		if (pin->type == EXT_PIN_REGULATOR) {
			if (!pin->supply) {
				dev_err(panel->dev, "invalid regulator(%s)\n",
						pin->name);
				return -EINVAL;
			}

			if (pin->value)
				regulator_set_voltage(pin->supply, pin->value,
						pin->value);

			rc = regulator_enable(pin->supply);
			if (unlikely(rc)) {
				dev_err(panel->dev, "regulator(%s) enable failed\n",
						pin->name);
				return -EINVAL;
			}
		} else {
			if (!gpio_is_valid(pin->gpio)) {
				dev_err(panel->dev, "invalid gpio(%s:%d)\n",
						pin->name, pin->gpio);
				return -EINVAL;
			}
			rc = gpio_request(pin->gpio, pin->name);
			if (unlikely(rc)) {
				pr_err("%s: gpio_request failed: %d\n",
						__func__, rc);
				return -EINVAL;
			}
			gpio_direction_output(pin->gpio, 1);
			gpio_free(pin->gpio);
		}
	} else {
		if (pin->type == EXT_PIN_REGULATOR) {
			if (!pin->supply) {
				dev_err(panel->dev, "invalid regulator(%s)\n",
						pin->name);
				return -EINVAL;
			}
			rc = regulator_disable(pin->supply);
			if (unlikely(rc)) {
				dev_err(panel->dev, "regulator(%s) disable failed\n",
						pin->name);
				return -EINVAL;
			}
		} else {
			if (!gpio_is_valid(pin->gpio)) {
				dev_err(panel->dev, "invalid gpio(%s:%d)\n",
						pin->name, pin->gpio);
				return -EINVAL;
			}
			rc = gpio_request(pin->gpio, pin->name);
			if (unlikely(rc)) {
				pr_err("%s: gpio_request failed: %d\n",
						__func__, rc);
				return -EINVAL;
			}
			gpio_direction_output(pin->gpio, 0);
			gpio_free(pin->gpio);
		}
	}

	return 0;
}

static int mmp_dsi_panel_enable(struct mmp_panel *panel)
{
	struct panel_plat_data *plat_data = panel->plat_data;

	if (IS_ENABLED(CONFIG_OF) && plat_data->enable_cmds.nr_cmds)
		mmp_panel_dsi_tx_cmd_array(panel, plat_data->enable_cmds.cmds,
			plat_data->enable_cmds.nr_cmds);

	return 0;
}

static int mmp_dsi_panel_disable(struct mmp_panel *panel)
{
	struct panel_plat_data *plat_data = panel->plat_data;

	if (IS_ENABLED(CONFIG_OF) && plat_data->disable_cmds.nr_cmds) {
		mmp_panel_dsi_tx_cmd_array(panel, plat_data->disable_cmds.cmds,
			plat_data->disable_cmds.nr_cmds);
		mmp_panel_dsi_ulps_set_on(panel, 1);
	}

	return 0;
}


static void mmp_dsi_panel_set_bl_panel(struct mmp_panel *panel, int level)
{
	struct panel_plat_data *plat_data = panel->plat_data;

	if (IS_ENABLED(CONFIG_OF) && plat_data->brightness_cmds.nr_cmds) {
		plat_data->brightness_cmds.cmds[0].data[1] = level;
		mmp_panel_dsi_tx_cmd_array(panel, plat_data->brightness_cmds.cmds,
			plat_data->brightness_cmds.nr_cmds);
	}
}

static void mmp_dsi_panel_set_bl_gpio(struct mmp_panel *panel, int intensity)
{
	int gpio_bl, bl_level, p_num;
	unsigned long flags;
	/*
	 * FIXME
	 * the initial value of bl_level_last is the
	 * uboot backlight level, it should be aligned.
	 */
	static int bl_level_last = 17;

	gpio_bl = of_get_named_gpio(panel->dev->of_node, "bl_gpio", 0);
	if (gpio_bl < 0) {
		pr_err("%s: of_get_named_gpio failed\n", __func__);
		return;
	}

	if (gpio_request(gpio_bl, "lcd backlight")) {
		pr_err("gpio %d request failed\n", gpio_bl);
		return;
	}

	/*
	 * Brightness is controlled by a series of pulses
	 * generated by gpio. It has 32 leves and level 1
	 * is the brightest. Pull low for 3ms makes
	 * backlight shutdown
	 */
	bl_level = (100 - intensity) * 32 / 100 + 1;

	if (bl_level == bl_level_last)
		goto set_bl_return;

	if (bl_level == 33) {
		/* shutdown backlight */
		gpio_direction_output(gpio_bl, 0);
		goto set_bl_return;
	}

	if (bl_level > bl_level_last)
		p_num = bl_level - bl_level_last;
	else
		p_num = bl_level + 32 - bl_level_last;

	while (p_num--) {
		spin_lock_irqsave(&bl_lock, flags);
		gpio_direction_output(gpio_bl, 0);
		udelay(1);
		gpio_direction_output(gpio_bl, 1);
		spin_unlock_irqrestore(&bl_lock, flags);
		udelay(1);
	}

set_bl_return:
	if (bl_level == 33)
		bl_level_last = 0;
	else
		bl_level_last = bl_level;
	gpio_free(gpio_bl);
	pr_debug("%s, intensity:%d\n", __func__, intensity);
}
#endif

static void mmp_panel_esd_onoff(struct mmp_panel *panel, int status)
{
	struct panel_plat_data *plat = panel->plat_data;

	if (status) {
		if (plat->esd_enable)
			esd_start(&panel->esd);
	} else {
		if (plat->esd_enable)
			esd_stop(&panel->esd);
	}
}

#ifdef CONFIG_OF
static void mmp_dsi_panel_power(struct mmp_panel *panel, int skip, int on)
{
	struct panel_plat_data *plat = panel->plat_data;
	static struct ext_pin *pin;
	struct ext_pin_ctrl *pin_ctrl;
	size_t nr_pin_ctrl;
	int i;

	if (on) {
		if (skip) {
			pin_ctrl = plat->power_boot.ctrls;
			nr_pin_ctrl = plat->power_boot.nr_ctrls;
		} else {
			pin_ctrl = plat->power_on.ctrls;
			nr_pin_ctrl = plat->power_on.nr_ctrls;
		}
	} else {
		pin_ctrl = plat->power_off.ctrls;
		nr_pin_ctrl = plat->power_off.nr_ctrls;
	}

	for (i = 0; i < nr_pin_ctrl; i++) {
		pin = mmp_dsi_panel_find_ext_pin(&plat->pins,
				pin_ctrl[i].name);
		if (unlikely(!pin)) {
			dev_err(panel->dev, "external pin(%s) not found\n",
					pin_ctrl[i].name);
			return;
		}
		pr_info("%s %s %dusec\n",
				pin_ctrl[i].name,
				pin_ctrl[i].on ? "on" : "off",
				pin_ctrl[i].usec);
		mmp_dsi_panel_ext_pin_ctrl(panel, pin, pin_ctrl[i].on);
		if (pin_ctrl[i].usec < 20 * 1000)
			usleep_range(pin_ctrl[i].usec, pin_ctrl[i].usec + 10);
		else
			msleep(pin_ctrl[i].usec / 1000);
	}
	return;
}
#else
static void mmp_dsi_panel_power(struct mmp_panel *panel, int on) {}
#endif

#ifdef CONFIG_OF
static void mmp_dsi_panel_on(struct mmp_panel *panel)
{
	struct panel_plat_data *plat = panel->plat_data;

	mmp_panel_dsi_ulps_set_on(panel, 0);

	mmp_panel_dsi_tx_cmd_array(panel, plat->cmds.cmds,
		plat->cmds.nr_cmds);
}
#else
static void mmp_dsi_panel_on(struct mmp_panel *panel) {}
#endif

static int mmp_dsi_panel_get_status(struct mmp_panel *panel)
{
	struct mmp_dsi_buf dbuf;
	u8 read_status = 0;
	int ret;

	ret = mmp_panel_dsi_rx_cmd_array(panel, &dbuf,
			panel_read_status_cmds,
			ARRAY_SIZE(panel_read_status_cmds));
	if (ret < 0) {
		pr_err("[ERROR] DSI receive failure!\n");
		return 1;
	}

	read_status = dbuf.data[0];
	if (read_status != MIPI_DCS_NORMAL_STATE) {
		pr_err("[ERROR] panel status is 0x%x\n", read_status);
		return 1;
	} else {
		pr_debug("panel status is 0x%x\n", read_status);
	}

	return 0;
}

static void mmp_dsi_panel_set_status(struct mmp_panel *panel, int status)
{
	struct panel_plat_data *plat = panel->plat_data;
	int skip_on = (status == MMP_ON_REDUCED);

	if (status_is_on(status)) {
#ifdef CONFIG_DDR_DEVFREQ
		if (panel->ddrfreq_qos != PM_QOS_DEFAULT_VALUE)
			pm_qos_update_request(&panel->ddrfreq_qos_req_min,
				panel->ddrfreq_qos);
#endif
		/* power on */
		if (plat->plat_onoff)
			plat->plat_onoff(1);
		else
			mmp_dsi_panel_power(panel, skip_on, 1);
		if (!skip_on) {
			mmp_dsi_panel_on(panel);
			mmp_dsi_panel_enable(panel);
		}

		mmp_dsi_panel_get_status(panel);
	} else if (status_is_off(status)) {
		mmp_dsi_panel_disable(panel);
		/* power off */
		if (plat->plat_onoff)
			plat->plat_onoff(0);
		else
			mmp_dsi_panel_power(panel, skip_on, 0);
#ifdef CONFIG_DDR_DEVFREQ
		if (panel->ddrfreq_qos != PM_QOS_DEFAULT_VALUE)
			pm_qos_update_request(&panel->ddrfreq_qos_req_min,
				PM_QOS_DEFAULT_VALUE);
#endif
	} else
		dev_warn(panel->dev, "set status %s not supported\n",
				status_name(status));
}

static int mmp_dsi_panel_get_modelist(struct mmp_panel *panel,
		struct mmp_mode **modelist)
{
	struct panel_plat_data *plat = panel->plat_data;

	*modelist = &plat->mode;
	return 1;
}


static void mmp_panel_esd_recover(struct mmp_panel *panel)
{
	struct mmp_path *path = mmp_get_path(panel->plat_path_name);
	esd_panel_recover(path);
}

static struct mmp_panel mmp_dsi_panel = {
	.name = "mmp-dsi-panel",
	.panel_type = PANELTYPE_DSI_VIDEO,
	.get_modelist = mmp_dsi_panel_get_modelist,
	.set_status = mmp_dsi_panel_set_status,
	.get_status = mmp_dsi_panel_get_status,
	.panel_esd_recover = mmp_panel_esd_recover,
	.esd_set_onoff = mmp_panel_esd_onoff,
};

static bool backlight_state = true;
static int mmp_dsi_panel_bl_update_status(struct backlight_device *bl)
{
	struct panel_plat_data *data = dev_get_drvdata(&bl->dev);
	struct mmp_panel *panel = data->panel;
	int level;

	if (bl->props.fb_blank == FB_BLANK_UNBLANK &&
			bl->props.power == FB_BLANK_UNBLANK)
		level = bl->props.brightness;
	else
		level = 0;

	if (!backlight_state && (level > 0)) {
		backlight_state = true;
		msleep(50);
	}

	if (!level)
		backlight_state = false;

	/* If there is backlight function of board, use it */
	if (data && data->plat_set_backlight) {
		data->plat_set_backlight(panel, level);
		return 0;
	}

	if (panel && panel->set_brightness)
		panel->set_brightness(panel, level);

	return 0;
}

static int mmp_dsi_panel_bl_get_brightness(struct backlight_device *bl)
{
	if (bl->props.fb_blank == FB_BLANK_UNBLANK &&
			bl->props.power == FB_BLANK_UNBLANK)
		return bl->props.brightness;

	return 0;
}

static const struct backlight_ops mmp_dsi_panel_bl_ops = {
	.get_brightness = mmp_dsi_panel_bl_get_brightness,
	.update_status  = mmp_dsi_panel_bl_update_status,
};

static int mmp_dsi_panel_probe(struct platform_device *pdev)
{
	struct mmp_mach_panel_info *mi;
	struct panel_plat_data *plat_data;
	struct device_node *np = pdev->dev.of_node;
	const char *path_name;
	struct backlight_properties props;
	struct backlight_device *bl;
	int ret;
	struct device_node *pin_node;
	struct device_node *videomode_node;
	u32 panel_num;
	struct device_node *child_np;
	int i = 0;
	u32 esd_enable;

	plat_data = kzalloc(sizeof(*plat_data), GFP_KERNEL);
	if (!plat_data)
		return -ENOMEM;

	if (of_property_read_u32(child_np, "panel_esd", &esd_enable))
		esd_enable = 0;

	plat_data->esd_enable = esd_enable;

	if (IS_ENABLED(CONFIG_OF)) {
		ret = of_property_read_string(np, "marvell,path-name",
			&path_name);
		if (ret < 0)
			return ret;

		mmp_dsi_panel.plat_path_name = path_name;

		if (of_property_read_u32(np, "marvell,dsi-panel-num",
					&panel_num))
			return -EINVAL;

		for_each_child_of_node(np, child_np) {
			if (of_property_read_u32(child_np, "panel_id",
						&plat_data->id))
				return -EINVAL;

			if (current_panel_id != plat_data->id) {
				if (panel_num == ++i) {
					pr_info("can't find suitable panel in dts\n");
					return -EINVAL;
				}
				continue;
			}

			ret = mmp_dsi_panel_parse_cmds(child_np,
					"marvell,dsi-panel-init-cmds",
					&plat_data->cmds);
			if (ret < 0)
				return ret;

			/* parse dt of video mode node */
			videomode_node = of_find_node_by_name(child_np,
					"video_mode");
			if (unlikely(!videomode_node)) {
				dev_err(&pdev->dev, "not found videomode_node!\n");
				return -EINVAL;
			}
			ret = mmp_dsi_panel_parse_video_mode(videomode_node,
					plat_data);
			if (ret < 0) {
				dev_err(&pdev->dev, "failed to parse video mode\n");
				return ret;
			}

			/* parse dt of external pin node */
			pin_node = of_find_node_by_name(child_np,
					"panel-ext-pin");
			if (unlikely(!pin_node)) {
				dev_err(&pdev->dev, "not found pin_node!\n");
				return -EINVAL;
			}
			ret = mmp_dsi_panel_parse_external_pins(pin_node,
					&pdev->dev, plat_data);
			if (unlikely(ret)) {
				dev_err(&pdev->dev, "failed to parse pin_node\n");
				return -EINVAL;
			}
			of_node_put(pin_node);

			mmp_dsi_panel_parse_cmds(child_np,
					"marvell,dsi-panel-enable-cmds",
					&plat_data->enable_cmds);

			mmp_dsi_panel_parse_cmds(child_np,
					"marvell,dsi-panel-disable-cmds",
					&plat_data->disable_cmds);

			if (of_get_named_gpio(child_np, "bl_gpio", 0) < 0) {
				mmp_dsi_panel_parse_cmds(child_np,
					"marvell,dsi-panel-backlight-set-brightness-cmds",
					&plat_data->brightness_cmds);
				mmp_dsi_panel.set_brightness =
					mmp_dsi_panel_set_bl_panel;
			} else
				mmp_dsi_panel.set_brightness =
					mmp_dsi_panel_set_bl_gpio;

#ifdef CONFIG_DDR_DEVFREQ
			ret = of_property_read_u32(child_np,
					"marvell,ddrfreq-qos",
					&mmp_dsi_panel.ddrfreq_qos);
			if (ret < 0) {
				mmp_dsi_panel.ddrfreq_qos =
					PM_QOS_DEFAULT_VALUE;
				pr_debug("panel %s didn't has ddrfreq min ",
						"request\n",
						mmp_dsi_panel.name);
			} else {
				mmp_dsi_panel.ddrfreq_qos_req_min.name = "lcd";
				pm_qos_add_request(
					&mmp_dsi_panel.ddrfreq_qos_req_min,
						PM_QOS_DDR_DEVFREQ_MIN,
						PM_QOS_DEFAULT_VALUE);
				pr_debug("panel %s has ddrfreq min request: %u\n",
					 mmp_dsi_panel.name,
					 mmp_dsi_panel.ddrfreq_qos);
			}
#endif
			break;
		}
	} else {
		/* get configs from platform data */
		mi = pdev->dev.platform_data;
		if (mi == NULL) {
			dev_err(&pdev->dev, "no platform data defined\n");
			return -EINVAL;
		}
		plat_data->plat_onoff = mi->plat_set_onoff;
		mmp_dsi_panel.plat_path_name = mi->plat_path_name;
		plat_data->plat_set_backlight = mi->plat_set_backlight;
	}

	mmp_dsi_panel.plat_data = plat_data;
	mmp_dsi_panel.dev = &pdev->dev;
	plat_data->panel = &mmp_dsi_panel;
	mmp_register_panel(&mmp_dsi_panel);

	if (plat_data->esd_enable)
		esd_init(&mmp_dsi_panel);
	/*
	 * if no panel or plat associate backlight control,
	 * don't register backlight device here.
	 */
	if (!mmp_dsi_panel.set_brightness && !plat_data->plat_set_backlight)
		return 0;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = 100;
	props.type = BACKLIGHT_RAW;

	bl = backlight_device_register("lcd-bl", &pdev->dev, plat_data,
			&mmp_dsi_panel_bl_ops, &props);
	if (IS_ERR(bl)) {
		ret = PTR_ERR(bl);
		dev_err(&pdev->dev, "failed to register lcd-backlight\n");
		return ret;
	}

	bl->props.fb_blank = FB_BLANK_UNBLANK;
	bl->props.power = FB_BLANK_UNBLANK;
	bl->props.brightness = 40;

	return 0;
}

static int mmp_dsi_panel_remove(struct platform_device *dev)
{
#ifdef CONFIG_DDR_DEVFREQ
	if (mmp_dsi_panel.ddrfreq_qos != PM_QOS_DEFAULT_VALUE)
		pm_qos_remove_request(&mmp_dsi_panel.ddrfreq_qos_req_min);
#endif
	mmp_dsi_panel_set_status(&mmp_dsi_panel, 0);
	mmp_unregister_panel(&mmp_dsi_panel);
	kfree(mmp_dsi_panel.plat_data);

	return 0;
}

static int mmp_dsi_panel_shutdown(struct platform_device *dev)
{
#ifdef CONFIG_DDR_DEVFREQ
	if (mmp_dsi_panel.ddrfreq_qos != PM_QOS_DEFAULT_VALUE)
		pm_qos_remove_request(&mmp_dsi_panel.ddrfreq_qos_req_min);
#endif
	if (mmp_dsi_panel.set_brightness)
		mmp_dsi_panel.set_brightness(&mmp_dsi_panel, 0);

	mmp_dsi_panel_set_status(&mmp_dsi_panel, 0);
	mmp_unregister_panel(&mmp_dsi_panel);
	kfree(mmp_dsi_panel.plat_data);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mmp_dsi_panel_dt_match[] = {
	{ .compatible = "marvell,mmp-dsi-panel" },
	{},
};
#endif

static struct platform_driver mmp_dsi_panel_driver = {
	.driver		= {
		.name	= "mmp-dsi-panel",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mmp_dsi_panel_dt_match),
	},
	.probe		= mmp_dsi_panel_probe,
	.remove		= mmp_dsi_panel_remove,
	.shutdown	= mmp_dsi_panel_shutdown,
};

static int mmp_dsi_panel_init(void)
{
	return platform_driver_register(&mmp_dsi_panel_driver);
}
static void mmp_dsi_panel_exit(void)
{
	platform_driver_unregister(&mmp_dsi_panel_driver);
}
module_init(mmp_dsi_panel_init);
module_exit(mmp_dsi_panel_exit);

MODULE_AUTHOR("yonghai_huang <huangyh@marvell.com>");
MODULE_DESCRIPTION("Panel driver for MIPI panel");
MODULE_LICENSE("GPL");
