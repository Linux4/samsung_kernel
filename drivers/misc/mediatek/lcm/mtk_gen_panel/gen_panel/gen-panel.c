/* mediatek/lcm/mtk_gen_panel/gen_panel/gen-panel.c
 *
 * Copyright (C) 2017 Samsung Electronics Co, Ltd.
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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <video/mipi_display.h>
#include <linux/gpio.h>
#include <linux/list.h>
#include "gen-panel.h"
#include "gen-panel-bl.h"
#ifdef CONFIG_GEN_PANEL_MDNIE
#include "gen-panel-mdnie.h"
#endif
#include <linux/power_supply.h>
#include <linux/of_platform.h>
#include "lcm_drv.h"
static int panel_id_index;
static int nr_panel_id;
static struct lcd *glcd;
static LIST_HEAD(extpin_list);
static LIST_HEAD(mani_list);
static LIST_HEAD(attr_list);
static int gen_panel_init_dev_attr(struct gen_dev_attr *);

enum {
	GEN_DTYPE_NONE = 0,
	GEN_DTYPE_8BIT = 1,
	GEN_DTYPE_16BIT = 2,
	GEN_DTYPE_32BIT = 4,
};

static int addr_offset;
static int addr_type;
static int data_offset;
static int data_type;

unsigned int lcdtype;
EXPORT_SYMBOL(lcdtype);

static int __init get_lcd_type(char *arg)
{
	get_option(&arg, &lcdtype);

	pr_info("lcdtype: %08x\n", lcdtype);

	return 0;
}
early_param("lcdtype", get_lcd_type);

static inline int op_cmd_index_valid(int index)
{
	return (index >= 0 && index < PANEL_OP_CMD_MAX) ? 1 : 0;
}

static inline int op_cmd_valid(struct lcd *lcd, int index)
{
	if (!lcd || !lcd->op_cmds ||
			!op_cmd_index_valid(index) ||
			!lcd->op_cmds[index].nr_desc ||
			!lcd->op_cmds[index].desc)
		return 0;
	return 1;
}

static inline int op_cmd_desc_valid(struct lcd *lcd, int index, int desc_index)
{
	if (!op_cmd_valid(lcd, index))
		return 0;

	if (desc_index < 0 ||
			desc_index >= lcd->op_cmds[index].nr_desc)
		return 0;

	return 1;
}

static inline struct gen_cmds_info *
find_op_cmd(struct lcd *lcd, int index)
{
	if (!op_cmd_valid(lcd, index))
		return NULL;

	return &lcd->op_cmds[index];
}

static inline struct gen_cmd_desc *
find_cmd_desc(struct gen_cmds_info *cmd, u32 reg)
{
	int i;
	void *paddr;

	for (i = 0; i < cmd->nr_desc; i++) {
		paddr = cmd->desc[i].data + addr_offset;
		if (!memcmp(paddr, (void *)&reg, addr_type)) {
			pr_debug("[LCD] %s found desc (0x%08X)\n",
							__func__, reg);
			pr_debug("[LCD] [addr:%08X] [reg:%08X] match\n",
						*((u32 *)paddr), reg);
			return &cmd->desc[i];
		}
		pr_debug("[LCD] [addr:%08X] [reg:%08X] not match\n",
				*((u32 *)paddr), reg);
	}

	pr_warn("[LCD] %s: can't find 0x%08X\n", __func__, reg);
	return NULL;
}

static inline struct gen_cmd_desc *
find_op_cmd_desc(struct lcd *lcd, int op_index, u32 reg)
{
	struct gen_cmds_info *cmd;
	struct gen_cmd_desc *desc;

	cmd = find_op_cmd(lcd, op_index);
	if (!cmd) {
		pr_warn("[LCD] %s: can't find [%d] op command\n",
				__func__, op_index);
		return 0;
	}

	desc = find_cmd_desc(cmd, reg);
	if (!desc) {
		pr_warn("[LCD] %s: can't find (0x%02X) cmd desc\n",
				__func__, reg);
		return 0;
	}

	return desc;
}

static inline void free_dcs_cmds(struct gen_cmds_info *cmd)
{
	struct gen_cmd_desc *desc;
	unsigned int i, nr_desc;

	if (!cmd || !cmd->desc || !cmd->nr_desc)
		return;

	desc = cmd->desc;
	nr_desc = cmd->nr_desc;
	for (i = 0; desc && i < nr_desc; i++) {
		kfree(desc[i].data);
		desc[i].data = NULL;
	}
	kfree(cmd->desc);
	cmd->desc = NULL;
}

static inline void free_cmd_array(struct lcd *lcd,
		struct gen_cmds_info *cmd, u32 nr_cmd)
{
	u32 i;

	if (cmd) {
		for (i = 0; i < nr_cmd; i++) {
			if (cmd[i].name)
				pr_info("[LCD] %s: unload %s, %p, %d ea\n",
						__func__, cmd[i].name,
						cmd[i].desc, cmd[i].nr_desc);
			free_dcs_cmds(&cmd[i]);
			kfree(cmd[i].name);
			cmd[i].name = NULL;
		}
		kfree(cmd);
		lcd->op_cmds = NULL;
	}
}

static inline void free_op_cmds_array(struct lcd *lcd)
{
	struct gen_cmds_info *cmd = lcd->op_cmds;
	unsigned int i;

	if (cmd) {
		for (i = 0; i < PANEL_OP_CMD_MAX; i++) {
			if (cmd[i].name)
				pr_info("[LCD] %s: unload %s, %p, %d ea\n",
						__func__, cmd[i].name,
						cmd[i].desc, cmd[i].nr_desc);
			free_dcs_cmds(&cmd[i]);
			kfree(cmd[i].name);
			cmd[i].name = NULL;
		}
		kfree(cmd);
		lcd->op_cmds = NULL;
	}
}

static inline void free_action_list(struct list_head *head)
{
	struct manipulate_action *action, *naction;

	list_for_each_entry_safe(action, naction, head, list) {
		list_del(&action->list);
		kfree(action->pmani);
		kfree(action);
	}
}

static inline void free_gen_dev_attr(void)
{
	struct gen_dev_attr *gen_attr, *ngen_attr;

	pr_info("%s +\n", __func__);
	list_for_each_entry_safe(gen_attr, ngen_attr, &attr_list, list) {
		list_del(&gen_attr->list);
		free_action_list(&gen_attr->action_list);
		gen_attr->action = NULL;
		kfree(gen_attr);
	}
	pr_info("%s -\n", __func__);
}

static inline void free_manipulate_table(void)
{
	struct manipulate_table *mani, *n_mani;

	pr_info("%s +\n", __func__);
	list_for_each_entry_safe(mani, n_mani, &mani_list, list) {
		list_del(&mani->list);
		free_dcs_cmds(&mani->cmd);
		kfree(mani->cmd.name);
		mani->cmd.name = NULL;
		kfree(mani);
	}
	pr_info("[LCD] %s -\n", __func__);
}

static inline void print_cmd_desc(struct gen_cmd_desc *desc)
{
	int i;
	char buffer[250];
	char *p = buffer;

	p += sprintf(p, "[%xh] ", desc->data[0]);
	for (i = 1; i < desc->length; i++)
		p += sprintf(p, "0x%02X ", desc->data[i]);
	pr_info("[LCD] %s\n", buffer);
}

static inline void print_cmds(struct gen_cmds_info *cmds)
{
	int i;

	for (i = 0; i < cmds->nr_desc; i++)
		print_cmd_desc(&cmds->desc[i]);
}

static inline int gen_panel_tx_cmds(struct lcd *lcd,
		struct gen_cmd_desc cmd[], int count)
{
	if (lcd && lcd->ops && lcd->ops->tx_cmds)
		return lcd->ops->tx_cmds(lcd, (void *)cmd, count);

	return 0;
}

static inline int gen_panel_rx_cmds(struct lcd *lcd, u8 *buf,
		struct gen_cmd_desc cmd[], int count)
{
	if (lcd && lcd->ops && lcd->ops->rx_cmds)
		return lcd->ops->rx_cmds(lcd, buf, (void *)cmd, count);

	return 0;
}

static inline int gen_panel_parse_dt_plat(const struct lcd *lcd,
		const struct device_node *np)
{
	if (lcd && lcd->ops && lcd->ops->parse_dt)
		return lcd->ops->parse_dt(np);

	return 0;
}

static inline int gen_panel_write_op_cmds(struct lcd *lcd, int index)
{
	if (!op_cmd_valid(lcd, index)) {
		pr_err("[LCD] %s, invalid op command %d\n", __func__, index);
		return -EINVAL;
	}

	pr_info("[LCD] %s\n", lcd->op_cmds[index].name);

	return gen_panel_tx_cmds(lcd,
			lcd->op_cmds[index].desc,
			lcd->op_cmds[index].nr_desc);
}

static inline int gen_panel_write_op_cmd_desc(struct lcd *lcd,
		int op_index, int desc_index)
{
	struct gen_cmds_info *cmd;

	if (!op_cmd_valid(lcd, op_index))
		return -EINVAL;

	cmd = &lcd->op_cmds[op_index];
	if (desc_index >= cmd->nr_desc ||
			desc_index < 0)
		return -EINVAL;

	pr_debug("[LCD] %s[%d]\n", cmd->name, desc_index);

	return gen_panel_tx_cmds(lcd, &cmd->desc[desc_index], 1);
}

static int gen_panel_get_temperature(int *temp)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name("battery");
	if (psy && psy->desc->get_property) {
		ret = psy->desc->get_property(psy, POWER_SUPPLY_PROP_TEMP,
									&val);
		if (ret)
			return ret;
		*temp = val.intval / 10;
		pr_debug("[LCD] %s: temperature (%d)\n", __func__, *temp);
	}
	return 0;
}

static int gen_panel_temp_compensation(struct lcd *lcd)
{
	struct temp_compensation *temp_comp;
	unsigned int nr_temp_comp = lcd->nr_temp_comp;
	int temperature = 0, ret, i;
	struct gen_cmd_desc *desc;

	if (!op_cmd_valid(lcd, PANEL_INIT_CMD)) {
		pr_warn("[LCD] %s: init_cmds empty\n", __func__);
		return 0;
	}

	ret = gen_panel_get_temperature(&temperature);
	if (ret) {
		pr_warn("[LCD] %s: can't get temperature (%d)\n",
				__func__, ret);
		return 0;
	}

	for (i = 0; i < nr_temp_comp; i++) {
		temp_comp = &lcd->temp_comp[i];
		if (!temp_comp || !temp_comp->old_data ||
				!temp_comp->new_data) {
			pr_warn("[LCD] %s: wrong input data\n", __func__);
			continue;
		}

		desc = find_op_cmd_desc(lcd, PANEL_INIT_CMD,
			temp_comp->old_data[0]);
		if (!desc) {
			pr_warn("[LCD] %s: can't find (0x%02X) cmd desc\n",
					__func__, temp_comp->old_data[0]);
			continue;
		}

		if ((temp_comp->trig_type == TEMPERATURE_LOW &&
					temperature <= temp_comp->temperature)
				|| (temp_comp->trig_type == TEMPERATURE_HIGH &&
				 temperature >= temp_comp->temperature)) {
			if (!memcmp(desc->data, temp_comp->new_data,
						desc->length))
				continue;
			memcpy(desc->data, temp_comp->new_data, desc->length);
			pr_info("[LCD] %s: update 0x%02Xh, 0x%02X\n",
					__func__, desc->data[0],
						desc->data[1]);
			pr_info("[LCD] %s: temperature (%d) is %s (%d)\n",
					__func__, temperature,
					temp_comp->trig_type ==
					TEMPERATURE_LOW ?
					"lower than" : "higher than",
					temp_comp->temperature);
		} else {
			if (!memcmp(desc->data, temp_comp->old_data,
						desc->length)) {
				continue;
			}
			memcpy(desc->data, temp_comp->old_data, desc->length);
			pr_info("[LCD] %s: restore 0x%02Xh, 0x%02X\n",
					__func__, desc->data[0],
						desc->data[1]);
			pr_info("[LCD] %s: temperature (%d) is %s (%d)\n",
					__func__, temperature,
					temp_comp->trig_type ==
					TEMPERATURE_LOW ?
					"lower than" : "higher than",
					temp_comp->temperature);
		}
	}

	return 0;
}

static size_t gen_panel_read_reg(struct lcd *lcd,
		u8 *buf, u8 addr, u8 len)
{
	static char read_len[] = {0x00};
	static char read_addr[] = {0x00};
	static struct gen_cmd_desc read_desc[] = {
		{MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE, DSI_HS_MODE, 0,
			sizeof(read_len), read_len},
		{MIPI_DSI_DCS_READ, DSI_HS_MODE, 0,
			sizeof(read_addr), read_addr},
	};

	read_len[0] = len;
	read_addr[0] = addr;

	pr_info("[LCD] %s: addr:0x%2x, len:%u\n", __func__, addr, len);
	return gen_panel_rx_cmds(lcd, buf, read_desc, ARRAY_SIZE(read_desc));
}

static u32 set_panel_id(struct lcd *lcd)
{
	u32 read_id;
	u8 out_buf[4] = {0,};

	mutex_lock(&lcd->access_ok);
	memset(out_buf, 0, sizeof(out_buf));
	gen_panel_write_op_cmds(lcd, PANEL_NV_ENABLE_CMD);

	if (lcd->id_rd_info[0].len > 1) { /*Read at the same time*/
		gen_panel_read_reg(lcd, out_buf, lcd->id_rd_info[0].reg,
		lcd->id_rd_info[0].len);
	} else {
		gen_panel_read_reg(lcd, &out_buf[0], lcd->id_rd_info[0].reg,
				lcd->id_rd_info[0].len);
		read_id = out_buf[0] << 16;
		gen_panel_read_reg(lcd, &out_buf[1], lcd->id_rd_info[1].reg,
				lcd->id_rd_info[1].len);
		read_id |= out_buf[1] << 8;
		gen_panel_read_reg(lcd, &out_buf[2], lcd->id_rd_info[2].reg,
				lcd->id_rd_info[2].len);
		read_id |= out_buf[2];
	}

	read_id = out_buf[0] << 16 | out_buf[1] << 8 | out_buf[2];

	gen_panel_write_op_cmds(lcd, PANEL_NV_DISABLE_CMD);
	mutex_unlock(&lcd->access_ok);

	return read_id;
}

/*
static int gen_panel_read_status(struct lcd *lcd)
{
	unsigned int status = 0;
	u8 out_buf[4];

	mutex_lock(&lcd->access_ok);
	if (!lcd->active) {
		mutex_unlock(&lcd->access_ok);
		return 0;
	}

	gen_panel_read_reg(lcd, out_buf, 0x04, 4);
	gen_panel_read_reg(lcd, out_buf, lcd->status_reg, 1);
	status = out_buf[0];
	pr_info("[LCD] %s: status 0x%x\n", __func__, status);

	if (status != lcd->status_ok)
		pr_info("[LCD] %s: err detected(0x%x, 0x%x)\n",
				__func__, status, lcd->status_ok);
	mutex_unlock(&lcd->access_ok);

	return (status == lcd->status_ok);
}
*/
static int gen_panel_read_comapre_reg(struct lcd *lcd,
		u8 *table, u8 len)
{
	unsigned char addr = table[0];
	unsigned char *data = table + 1;
	int i, ret = 0;
	u8 *out_buf;

	out_buf = kzalloc(sizeof(u8) * len, GFP_KERNEL);
	if (!out_buf) {
		pr_err("[LCD] %s: memory allocation failed\n", __func__);
		return -ENOMEM;
	}

	gen_panel_read_reg(lcd, out_buf, addr, len);
	for (i = 0; i < len - 1; i++)
		if (out_buf[i] != data[i]) {
			pr_warn("[LCD] [%Xh] - (0x%x, 0x%x) not match\n",
					addr, data[i], out_buf[i]);
			ret = -EINVAL;
		} else {
			pr_debug("[LCD] [%Xh] - (0x%x, 0x%x) match\n",
					addr, data[i], out_buf[i]);
		}
	kfree(out_buf);

	return ret;
}

int gen_dsi_panel_verify_reg(struct lcd *lcd,
		struct gen_cmd_desc desc[], int count)
{
	u32 loop;
	int ret = 0;

	for (loop = 0; loop < count; loop++) {
		ret = gen_panel_read_comapre_reg(lcd,
				desc[loop].data, desc[loop].length);
		if (ret < 0)
			pr_err("[LCD] %s: not match table\n", __func__);

	}
	return ret;
}
EXPORT_SYMBOL(gen_dsi_panel_verify_reg);

static int gen_panel_set_bl_level(struct lcd *lcd, int intensity)
{
	/*
	struct device *dev = lcd->dev;
	struct gen_cmd_desc *desc;
	*/

	pr_info("[LCD]set brightness (%d)\n", intensity);

	mutex_lock(&lcd->access_ok);
	lcd->op_cmds[PANEL_BL_SET_BRT_CMD].desc->data[1] = intensity;
	#if 0
	if (!desc) {
		dev_err(dev, "[LCD] not found brightness level cmd\n");
		return 0;
	}

	if (data_type)
		memcpy(&desc->data[data_offset], &intensity, data_type);
	/* gen_panel_write_op_cmds(lcd, index); */
	#endif

	gen_panel_write_op_cmds(lcd, PANEL_BL_ON_CMD);
	gen_panel_write_op_cmds(lcd, PANEL_BL_SET_BRT_CMD);
	mutex_unlock(&lcd->access_ok);

	return intensity;
}

static int gen_panel_set_brightness(void *bd_data, int intensity)
{
	struct lcd *lcd = (struct lcd *)bd_data;
	struct device *dev = lcd->dev;

	if (!lcd->active) {
		pr_warn("[LCD] %s, lcd is inactive\n", __func__);
		return 0;
	}
	dev_dbg(dev, "set brightness (%d)\n", intensity);

	/* set backlight-ic level directly */
	gen_panel_set_bl_level(lcd, intensity);

	return intensity;
}

static const struct gen_panel_backlight_ops backlight_ops = {
	.set_brightness = gen_panel_set_brightness,
	.get_brightness = NULL,
};

#ifdef CONFIG_GEN_PANEL_MDNIE
static int gen_panel_set_mdnie(struct mdnie_config *config)
{
	struct lcd *lcd = glcd;
	struct mdnie_lite *mdnie = &lcd->mdnie;
	int index = 0;
	/*
	* FB_SUSPEND = 0,
	* FB_RESUME,
	* DOZE_SUSPEND,
	* DOZE,
	*/
	if (ddp_check_power_mode() == 0) {

		pr_info("[LCD] %s: power mode =[%d]\n",
			__func__, ddp_check_power_mode());
		return 0;
	}

	mutex_lock(&lcd->access_ok);
	if (!lcd->active || lcd->mdnie_disable) {
		mutex_unlock(&lcd->access_ok);
		return 0;
	}

	if (config->tuning) {
		pr_info("[LCD] %s: set tuning data\n", __func__);
	} else if (config->accessibility) {
		pr_info("[LCD] %s: set accessibility(%d)\n",
				__func__, config->accessibility);
		switch (config->accessibility) {
		case NEGATIVE:
			index = PANEL_MDNIE_NEGATIVE_CMD;
			break;
		case COLOR_BLIND:
			index = PANEL_MDNIE_COLOR_ADJ_CMD;
			break;
		case SCREEN_CURTAIN:
			index = PANEL_MDNIE_CURTAIN_CMD;
			break;
		case GRAY_SCALE:
			index = PANEL_MDNIE_GRAY_SCALE_CMD;
			break;
		case GRAY_SCALE_NEGATIVE:
			index = PANEL_MDNIE_GRAY_SCALE_NEGATIVE_CMD;
			break;
		case OUTDOOR:
			index = PANEL_MDNIE_OUTDOOR_CMD;
			break;
		default:
			pr_warn("[LCD] %s, accessibility(%d) not exist\n",
					__func__, config->accessibility);
			break;
		}
	} else if (config->negative) {
		pr_info("[LCD] %s: set negative\n", __func__);
		index = PANEL_MDNIE_NEGATIVE_CMD;
	} else if (IS_HBM(lcd->hbm)) {
		pr_info("[LCD] %s: set outdoor\n", __func__);
		if (config->scenario == MDNIE_EBOOK_MODE ||
				config->scenario == MDNIE_BROWSER_MODE)
			index = PANEL_MDNIE_HBM_TEXT_CMD;
		else
			index = PANEL_MDNIE_HBM_CMD;
	} else {
		index = MDNIE_OP_INDEX(config->mode, config->scenario);
		pr_info("[LCD] %s: mode(%d), scenario(%d), index(%d), table(%s)\n",
			__func__, config->mode, config->scenario, index, op_cmd_names[index]);
		if (!op_cmd_valid(lcd, index)) {
			pr_warn("%s: mode(%d), scenario(%d) not exist\n",
					__func__, config->mode,
					config->scenario);
			index = PANEL_MDNIE_UI_CMD;
		}
	}

	mdnie->config = *config;

	gen_panel_write_op_cmds(lcd, PANEL_NV_ENABLE_CMD);
	gen_panel_write_op_cmds(lcd, index);
	gen_panel_write_op_cmds(lcd, PANEL_NV_DISABLE_CMD);
	mutex_unlock(&lcd->access_ok);

	return 1;
}

static int gen_panel_set_color_adjustment(struct mdnie_config *config)
{
	struct lcd *lcd = glcd;
	struct mdnie_lite *mdnie = &lcd->mdnie;
	struct gen_cmd_desc *desc;
	int i, j;

	desc = find_op_cmd_desc(lcd, PANEL_MDNIE_COLOR_ADJ_CMD,
			lcd->color_adj_reg);
	if (!desc) {
		pr_err("%s: cmd not found\n", __func__);
		return 0;
	}

	for (i = 0; i < NUMBER_OF_SCR_DATA; i++) {
		j = (lcd->rgb == GEN_PANEL_RGB) ?
			i : NUMBER_OF_SCR_DATA - 1 - i;
		desc->data[i * 2 + 1] = GET_LSB_8BIT(mdnie->scr[j]);
		desc->data[i * 2 + 2] = GET_MSB_8BIT(mdnie->scr[j]);
	}

	return 1;
}

static int gen_panel_set_cabc(struct mdnie_config *config)
{
	struct lcd *lcd = glcd;
	struct gen_cmd_desc *desc;

	mutex_lock(&lcd->access_ok);
	if (!lcd->active) {
		mutex_unlock(&lcd->access_ok);
		return 0;
	}

	if (config->cabc < 0 ||
			config->cabc >= MAX_CABC) {
		pr_err("[LCD] %s: out of range (%d)\n", __func__, config->cabc);
		mutex_unlock(&lcd->access_ok);
		return 0;
	}

	if (config->cabc > 0) {
		/* FIX ME : 0x55 */
		desc = find_op_cmd_desc(lcd, PANEL_CABC_ON_CMD, 0x55);
		if (!desc) {
			pr_err("[LCD] %s: cabc on cmd not found\n", __func__);
			mutex_unlock(&lcd->access_ok);
			return 0;
		}
		if (desc->data[1] & 0xF0)
			desc->data[1] = config->cabc + (desc->data[1] & 0xF0);
		else
		desc->data[1] = config->cabc;

		gen_panel_write_op_cmds(lcd, PANEL_CABC_ON_CMD);
	} else
		gen_panel_write_op_cmds(lcd, PANEL_CABC_OFF_CMD);
	pr_info("[LCD] %s: cabc %s\n", __func__, cabc_modes[config->cabc]);
	mutex_unlock(&lcd->access_ok);

	return 0;
}

static const struct mdnie_ops mdnie_ops = {
	.set_mdnie = gen_panel_set_mdnie,
	.get_mdnie = NULL,
	.set_color_adj = gen_panel_set_color_adjustment,
	.set_cabc = gen_panel_set_cabc,
};
#endif

#ifdef CONFIG_OF
static struct manipulate_action *
find_mani_action_by_name(struct list_head *head, const char *name)
{
	struct manipulate_action *action, *n;

	pr_info("[LCD] %s, try to find : %s\n", __func__, name);
	list_for_each_entry_safe(action, n, head, list) {
	pr_info("[LCD] %s, action->name : %s\n", __func__, action->name);
		if (!strncmp(name, action->name, strlen(action->name))) {
			pr_info("[LCD] %s, %s found\n", __func__, action->name);
			return action;
		}
	}
	return NULL;
}

int print_mani_action_list(char *buf, struct list_head *head)
{
	struct manipulate_action *action, *n;
	int num = 0;

	list_for_each_entry_safe(action, n, head, list)
		num += sprintf(buf + num, "%s\n", action->name);

	return num;
}

static struct manipulate_table *
find_manipulate_table_by_node(const struct device_node *np)
{
	struct manipulate_table *mani, *n;

	list_for_each_entry_safe(mani, n, &mani_list, list)
		if (mani->np == np)
			return mani;
	pr_warn("[LCD] %s, %s not found\n", __func__, np->name);
	return NULL;
}

static struct extpin *find_extpin_by_name(const char *name)
{
	struct extpin *pin;

	list_for_each_entry(pin, &extpin_list, list)
		if (!strcmp(pin->name, name))
			return pin;

	return NULL;
}

static void free_extpin(void)
{
	struct extpin *pin;

	while (!list_empty(&extpin_list)) {
		pin = list_first_entry(&extpin_list,
				struct extpin, list);
		list_del(&pin->list);
		kfree(pin);
	}
}

static int gen_panel_set_extpin(struct lcd *lcd,
		struct extpin *pin, int on)
{
	int rc;

	if (on) {
		if (pin->type == EXT_PIN_REGULATOR) {
			if (!pin->supply) {
			dev_err(lcd->dev, "[LCD] invalid regulator(%s)\n",
						pin->name);
				return -EINVAL;
			}

			if (regulator_is_enabled(pin->supply))
				pr_info("[LCD]regulator(%s) already enabled\n",
						pin->name);
			rc = regulator_enable(pin->supply);
			if (unlikely(rc)) {
				dev_err(lcd->dev,
					"[LCD] regulator(%s) enable failed\n",
					pin->name);
				return -EINVAL;
			}
		} else {
			if (!gpio_is_valid(pin->gpio)) {
				dev_err(lcd->dev,
					"[LCD] invalid gpio(%s:%d)\n",
					pin->name, pin->gpio);
				return -EINVAL;
			}
			gpio_direction_output(pin->gpio, 1);
		}
	} else {
		if (pin->type == EXT_PIN_REGULATOR) {
			if (!pin->supply) {
				dev_err(lcd->dev,
					"[LCD] invalid regulator(%s)\n",
					pin->name);
				return -EINVAL;
			}

			if (!regulator_is_enabled(pin->supply))
				pr_info("[LCD]regulator(%s)already disabled\n",
						pin->name);
			rc = regulator_disable(pin->supply);
			if (unlikely(rc)) {
				dev_err(lcd->dev,
					"[LCD] regulator(%s) disable failed\n",
					pin->name);
				return -EINVAL;
			}
		} else {
			if (!gpio_is_valid(pin->gpio)) {
				dev_err(lcd->dev,
					"[LCD] invalid gpio(%s:%d)\n",
					pin->name, pin->gpio);
				return -EINVAL;
			}
			gpio_direction_output(pin->gpio, 0);
		}
	}

	return 0;
}

static int gen_panel_get_extpin(struct lcd *lcd, struct extpin *pin)
{
	if (pin->type == EXT_PIN_REGULATOR) {
		if (!pin->supply) {
			dev_err(lcd->dev,
				"[LCD] invalid regulator(%s)\n",
				pin->name);
			return -EINVAL;
		}
		return regulator_is_enabled(pin->supply);
	} else {
		if (!gpio_is_valid(pin->gpio)) {
			dev_err(lcd->dev,
				"[LCD] invalid gpio(%s:%d)\n",
				pin->name, pin->gpio);
			return -EINVAL;
		}
		return gpio_get_value(pin->gpio);
	}

	return 0;
}

static int gen_panel_set_pin_state(struct lcd *lcd, int on)
{
	struct pinctrl_state *pinctrl_state;
	int ret = 0;

	if (!lcd->pinctrl)
		return 0;

	pinctrl_state = on ? lcd->pin_enable : lcd->pin_disable;
	if (!pinctrl_state)
		return 0;

	ret = pinctrl_select_state(lcd->pinctrl, pinctrl_state);
	if (unlikely(ret)) {
		pr_err("[LCD] %s: can't set pinctrl %s state\n",
				__func__, on ? "enable" : "disable");
	}
	return ret;
}

static void gen_panel_set_power(struct lcd *lcd, int on)
{
	struct extpin_ctrl *pin_ctrl = NULL;
	size_t nr_pin_ctrl = 0;
	struct extpin *pin;
	unsigned long expires;
	unsigned int remain_msec, i;
	int state;

	pr_debug("%s, pwr %d, %s\n", __func__, lcd->power, on ? "on" : "off");

	if ((on && (lcd->power & on)) || (!on && !lcd->power)) {
		pr_warn("%s: power already %s(%d) state\n",
				__func__, on ? "on" : "off", (on >> 1));
		return;
	}

	if (on) {
		if (on == GEN_PANEL_PWR_ON_FIR) {
			pin_ctrl = lcd->extpin_on_fir_seq.ctrls;
			nr_pin_ctrl = lcd->extpin_on_fir_seq.nr_ctrls;
		} else if (on == GEN_PANEL_PWR_ON_SEC) {
			pin_ctrl = lcd->extpin_on_sec_seq.ctrls;
			nr_pin_ctrl = lcd->extpin_on_sec_seq.nr_ctrls;
		} else if (on == GEN_PANEL_PWR_ON_THI) {
			pr_info("third pwr is on!\n");
			pin_ctrl = lcd->extpin_on_thi_seq.ctrls;
			nr_pin_ctrl = lcd->extpin_on_thi_seq.nr_ctrls;
		} else if (on == GEN_PANEL_PWR_ON_FOU) {
			pin_ctrl = lcd->extpin_on_fou_seq.ctrls;
			nr_pin_ctrl = lcd->extpin_on_fou_seq.nr_ctrls;
		}
	} else {
		pin_ctrl = lcd->extpin_off_seq.ctrls;
		nr_pin_ctrl = lcd->extpin_off_seq.nr_ctrls;
	}

	if (!pin_ctrl || !nr_pin_ctrl) {
		pr_err("%s, pwr%d is pin not exist\n", __func__, (on >> 1));
		return;
	}

	for (i = 0; i < nr_pin_ctrl; i++) {
		pin = pin_ctrl[i].pin;
		if (mutex_is_locked(&pin->expires_lock)) {
			expires = pin->expires;
			if (!time_after(jiffies, expires)) {
				remain_msec = jiffies_to_msecs(
						abs((long)(expires) -
							(long)(jiffies)));
				if (remain_msec > 1000) {
				pr_warn("[LCD] %s, wait (%d msec) to long\n",
						__func__, remain_msec);
							remain_msec = 1000;
				}
				pr_info("[LCD] %s, wait %d msec\n",
						__func__, remain_msec);
				panel_usleep(remain_msec * 1000);
			}
			pin->expires = 0;
			mutex_unlock(&pin->expires_lock);
		}

		if (pin_ctrl[i].on == EXT_PIN_LOCK) {
			mutex_lock(&pin->expires_lock);
			pin->expires = jiffies +
				usecs_to_jiffies(pin_ctrl[i].usec);
			state = gen_panel_get_extpin(lcd, pin);
			if (state < 0)
				pr_err("[LCD] %s, get pin state failed (%d)\n",
						__func__, state);
			else
				pr_info("[LCD] %s keep %s for %d usec\n",
					pin->name, state ? "on" : "off",
					pin_ctrl[i].usec);

			mutex_unlock(&pin->expires_lock);
		} else {
			gen_panel_set_extpin(lcd, pin_ctrl[i].pin,
					pin_ctrl[i].on);
			pr_info("[LCD] %s %s %d usec\n", pin->name,
					pin_ctrl[i].on ? "on" : "off",
					pin_ctrl[i].usec);
			panel_usleep(pin_ctrl[i].usec);
		}
	}

	if (on)
		lcd->power |= on;
	else
		lcd->power = on;

}
#else
static void gen_panel_set_power(struct lcd *lcd, int on) {}
#endif

static void gen_panel_set_id(struct lcd *lcd)
{
/*this code is re-checked for pass of lcd id from bootloader side*/
#ifdef CONFIG_SEC_FACTORY
	static int is_fac;

	/*PBA status & if ID is zero or read fail on FACTORY test, bypass*/
	if ((!lcd->id) && (!is_fac))
		pr_info("%s, PBA test & ID = [0x%x].\n",
			__func__, lcd->id);
	else {/*FACTORY Test*/
		if (lcd->mode.fac_panel_swap)
			lcd->id = lcdtype = lcd->set_panel_id(lcd);
		/*If lcd id is read fail or id is zero*/
		if (!lcd->id)
			is_fac = true;

		pr_info("%s, FACTORY test & ID = [0x%x].\n",
			__func__, lcd->id);
	}
#endif
}

static inline unsigned int gen_panel_get_panel_id(struct lcd *lcd)
{
	if (lcd->id)
		return true;
	else
		return false;
	/*return lcd->id;*/
}

static bool gen_panel_connected(struct lcd *lcd)
{
	/*
	struct lcd *lcd = panel_to_lcd(panel);
	*/

	/* Check Panel ID */
	if (gen_panel_get_panel_id(lcd))
		return true;

	/* Check VGH PIN */
	/*
	if (lcd->esd_en)
		return esd_check_vgh_on(&panel->esd);
	*/

	return false;
}

void print_data(char *data, int len)
{
	unsigned char buffer[1000];
	unsigned char *p = buffer;
	int i;

	if (len < 0 || len > 200) {
		pr_warn("%s, out of range (len %d)\n",
				__func__, len);
		return;
	}

	p += sprintf(p, "========== [DATA DUMP - stt] ==========\n");
	for (i = 0; i < len; i++) {
		p += sprintf(p, "0x%02X", data[i]);
		if (!((i + 1) % 8) || (i + 1 == len))
			p += sprintf(p, "\n");
		else
			p += sprintf(p, " ");
	}
	p += sprintf(p, "========== [DATA DUMP - end] ==========\n");
	pr_info("%s", buffer);
}


void gen_panel_set_external_main_pwr(struct lcd *lcd, int status)
{
	gen_panel_set_pin_state(lcd, status);
	gen_panel_set_power(lcd, status);
}

void gen_panel_set_external_subsec_pwr(struct lcd *lcd, int status)
{
	gen_panel_set_power(lcd, GEN_PANEL_PWR_ON_SEC);
}

void gen_panel_set_external_subthi_pwr(struct lcd *lcd, int status)
{
	gen_panel_set_power(lcd, GEN_PANEL_PWR_ON_THI);
}

void gen_panel_set_external_subfou_pwr(struct lcd *lcd, int status)
{
	gen_panel_set_power(lcd, GEN_PANEL_PWR_ON_FOU);
}

void gen_panel_set_status(struct lcd *lcd, int status)
{
	int skip_on = (status == GEN_PANEL_ON_REDUCED);

	pr_info("[LCD] called %s with status %d\n", __func__, status);

	if (gen_panel_is_on(status)) {
		/* power on */
		if (!skip_on)
		gen_panel_set_id(lcd);
		if (!gen_panel_connected(lcd)) {
			lcd->active = true;
			pr_warn("[LCD] %s: no panel\n", __func__);
			if (gen_panel_backlight_is_on(lcd->bd))
				gen_panel_backlight_onoff(lcd->bd, 0);
			gen_panel_set_external_main_pwr(lcd, 0);
			return;
		}
		mutex_lock(&lcd->access_ok);
		if (!skip_on) {
			if (lcd->temp_comp_en)
				gen_panel_temp_compensation(lcd);
			gen_panel_write_op_cmds(lcd, PANEL_INIT_CMD);
			gen_panel_write_op_cmds(lcd, PANEL_ENABLE_CMD);
		}
		lcd->active = true;
		mutex_unlock(&lcd->access_ok);
	} else if (gen_panel_is_off(status)) {
		/* power off */
		if (gen_panel_backlight_is_on(lcd->bd))
			gen_panel_backlight_onoff(lcd->bd, 0);

		mutex_lock(&lcd->access_ok);
		lcd->active = false;
		lcd->mdnie.config.scenario = 0;
		gen_panel_write_op_cmds(lcd, PANEL_DISABLE_CMD);
		mutex_unlock(&lcd->access_ok);
	} else
		pr_warn("[LCD] set status %d not supported\n", status);
}

void gen_panel_start(struct lcd *lcd, int status)
{
	pr_debug("[LCD] %s\n", __func__);
	if (!lcd->active) {
		pr_warn("[LCD] %s: no panel\n", __func__);
		return;
	}

	if (gen_panel_is_reduced_on(status)) {
#ifdef CONFIG_GEN_PANEL_MDNIE
		if (lcd->mdnie.config.cabc)
			update_cabc_mode(&lcd->mdnie);
		update_mdnie_mode(&lcd->mdnie);
#endif
		if (!gen_panel_backlight_is_on(lcd->bd))
			gen_panel_backlight_onoff(lcd->bd, 1);
	} else if (gen_panel_is_on(status)) {
		gen_panel_write_op_cmds(lcd,
				PANEL_POST_ENABLE_CMD);

#ifdef CONFIG_GEN_PANEL_MDNIE
		if (lcd->mdnie.config.cabc)
			update_cabc_mode(&lcd->mdnie);
		update_mdnie_mode(&lcd->mdnie);
#endif
		/* To wait 2 frames for stability need to be applied
		seperately in LDI. so it need to be moved on dts
		If you need it, please add delay on dts mdnie cmd
		after video on, wait more than 2 frames
		*/
		/* panel_usleep(33 * 1000); */

		if (!gen_panel_backlight_is_on(lcd->bd))
			gen_panel_backlight_onoff(lcd->bd, 1);
		gen_panel_write_op_cmds(lcd,
				PANEL_POST_ENABLE_1_CMD);
	} else {
		if (gen_panel_backlight_is_on(lcd->bd))
			gen_panel_backlight_onoff(lcd->bd, 0);
		mutex_lock(&lcd->access_ok);
		lcd->active = false;
		mutex_unlock(&lcd->access_ok);
		gen_panel_write_op_cmds(lcd, PANEL_DISABLE_CMD);

	}
}

#ifdef CONFIG_OF
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
		pr_err("[LCD] %s: failed to read property(%s)\n",
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
	int ret = 0;

	len = strlen(propname) + strlen(suffix) + 1;
	name = kzalloc(sizeof(char) * len, GFP_KERNEL);
	if (unlikely(!name))
		return -ENOMEM;

	snprintf(name, len, "%s%s", propname, suffix);
	if (!of_find_property(np, name, NULL)) {
		pr_debug("[LCD] %s: %s is not exist\n",
				__func__, name);
		kfree(name);
		return -EINVAL;
	}

	ret = of_property_read_string(np, name, out_string);
	if (unlikely(ret))
		pr_err("[LCD] %s: failed to read property(%s)\n",
				__func__, name);

	kfree(name);
	return ret;
}

static struct device_node *gen_panel_find_dt_panel(
		struct device_node *node, char *panel_cfg)
{
	struct device_node *parent;
	struct device_node *panel_node = NULL;
	char *panel_name;
	u32 panel_ids[8];
	int i, sz;

	/*
	 * priority of panel node being found
	 * 1. corresponding name of panel nodes with panel_cfg.
	 * 2. corresponding panel id of panel_node
	 * 3. first one of child nodes of panel_node.
	 */
	parent = of_parse_phandle(node, "gen-panel", 0);
	if (!parent) {
		pr_err("[LCD] %s: panel node not found\n", __func__);
		return NULL;
	}

	if (panel_cfg && strlen(panel_cfg)) {
		panel_name = panel_cfg + 2;
		panel_node = of_find_node_by_name(parent, panel_name);
		if (panel_node)
			pr_info("[LCD]found (%s) by name\n", panel_node->name);
	} else {
		struct device_node *np;
		int ret;

		/* Find a node corresponding panel id */
		for_each_child_of_node(parent, np) {
			if (!of_find_property(np, "gen-panel-id", &sz)) {
				pr_warn("not found id from (%s)\n",
						np->name);
				continue;
			}

			nr_panel_id = (sz / sizeof(u32));
			if (nr_panel_id > 8) {
				pr_warn("%s, constrain 8 panel_ids\n",
						__func__);
				nr_panel_id = 8;
			}

			ret = of_property_read_u32_array(np,
				"gen-panel-id", panel_ids, nr_panel_id);
			if (unlikely(ret < 0)) {
				pr_warn("[LCD] %s, failed to get panel_id\n",
					__func__);
				continue;
			}

			for (i = 0; i < nr_panel_id; i++) {
				if (lcdtype == panel_ids[i]) {
					panel_node = np;
					panel_id_index = i;
					pr_info("[LCD]found (%s) by id(0x%X)\n",
						np->name, panel_ids[i]);
					break;
				}
			}
			if (panel_node) {
				pr_debug("found successfully\n");
				break;
			}
		}

		if (!panel_node) {
			panel_node = of_get_next_child(parent, NULL);
			if (panel_node)
				pr_info("[LCD] found (%s) by node order\n",
						panel_node->name);
		}
	}
	of_node_put(parent);
	if (!panel_node)
		pr_err("[LCD] %s: panel_node not found\n", __func__);

	return panel_node;
}

static int gen_panel_parse_dt_dcs_cmds(struct device_node *np,
		const char *propname, struct gen_cmds_info *cmd)
{
	struct gen_cmd_hdr *gchdr;
	struct gen_cmd_desc *desc;
	const char *val;
	char *data;
	int sz = 0, i = 0, nr_desc = 0, nr;

	if (!cmd) {
		pr_err("[LCD] %s: invalid cmd address\n", __func__);
		return -EINVAL;
	}

	if (!of_find_property(np, propname, NULL)) {
		pr_debug("[LCD] %s: %s is not exist\n", __func__, propname);
		return -EINVAL;
	}

	val = of_get_property(np, propname, &sz);
	if (unlikely(!val)) {
		pr_err("[LCD] %s: failed, key=%s\n", __func__, propname);
		return -ENOMEM;
	}

	data = kzalloc(sizeof(char) * sz, GFP_KERNEL);
	if (unlikely(!data))
		return -ENOMEM;

	memcpy(data, val, sz);

	/* scan dcs commands */
	while (sz > i + sizeof(*gchdr)) {
		gchdr = (struct gen_cmd_hdr *)(data + i);
		gchdr->dlen = ntohs(gchdr->dlen);
		gchdr->wait = ntohs(gchdr->wait);
		printk("[LCD] %s: parse DEBUG dtsi name:%s type:0x%02x,txmode:0x%02x, len:0x%02x, nr_desc:%d\n",
				__func__, propname, gchdr->dtype, gchdr->txmode,
				gchdr->dlen, nr_desc);
		if (i + gchdr->dlen > sz) {
			printk("[LCD] %s: parse error dtsi name:%s type:0x%02x,txmode:0x%02x, len:0x%02x, nr_desc:%d\n",
				__func__, propname, gchdr->dtype, gchdr->txmode,
				gchdr->dlen, nr_desc);
			gchdr = NULL;
			kfree(data);
			return -ENOMEM;
		}
		i += sizeof(*gchdr) + gchdr->dlen;
		nr_desc++;
	}

	if (unlikely(sz != i)) {
		pr_err("[LCD] %s: dcs_cmd=%x len=%d error!",
				__func__, data[0], sz);
		kfree(data);
		return -ENOMEM;
	}

	cmd->desc = kzalloc(nr_desc * sizeof(struct gen_cmd_desc),
			GFP_KERNEL);
	cmd->name = kzalloc(sizeof(char) * (strlen(propname) + 1),
			GFP_KERNEL);
	strncpy(cmd->name, propname, strlen(propname) + 1);
	cmd->nr_desc = nr_desc;
	if (unlikely(!cmd->desc)) {
		pr_err("[LCD] %s: fail to allocate cmd\n", __func__);
		goto err_alloc_cmds;
	}

	desc = cmd->desc;
	for (i = 0, nr = 0; nr < nr_desc; nr++) {
		gchdr = (struct gen_cmd_hdr *)(data + i);
		desc[nr].data_type = gchdr->dtype;
		desc[nr].lp = (!!gchdr->txmode);
		desc[nr].delay = gchdr->wait;
		desc[nr].length = gchdr->dlen;
		desc[nr].data = kzalloc(gchdr->dlen * sizeof(unsigned char),
				GFP_KERNEL);
		if (!desc[nr].data) {
			pr_err("[LCD] %s: fail to allocate data\n", __func__);
			goto err_alloc_data;
		}
		memcpy(desc[nr].data, &data[i + sizeof(*gchdr)],
				gchdr->dlen * sizeof(unsigned char));
		i += sizeof(*gchdr) + gchdr->dlen;
		pr_debug("[LCD] type:%x, %s, %d ms, %d bytes, %02Xh\n",
				desc[nr].data_type,
				tx_modes[desc[nr].lp],
				desc[nr].delay, desc[nr].length,
				(desc[nr].data)[0]);
	}
	kfree(data);
	pr_debug("[LCD] parse %s done!\n", propname);
	return 0;
err_alloc_data:
	for (nr = 0; nr < nr_desc; nr++) {
		kfree(desc[nr].data);
		desc[nr].data = NULL;
	}
err_alloc_cmds:
	kfree(cmd->name);
	cmd->name = NULL;
	kfree(cmd->desc);
	cmd->desc = NULL;
	kfree(data);

	return -ENOMEM;
}

static int gen_panel_parse_dt_pinctrl(
		struct platform_device *pdev, struct lcd *lcd)
{
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_state;

	if (unlikely(!pdev))
		return -ENODEV;

	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pinctrl))
		return 0;

	lcd->pinctrl = pinctrl;
	pinctrl_state = pinctrl_lookup_state(lcd->pinctrl, "enable");
	if (!IS_ERR(pinctrl_state)) {
		dev_info(&pdev->dev,
			"[LCD] %s: found pinctrl enable\n", __func__);
			lcd->pin_enable = pinctrl_state;
	}

	pinctrl_state = pinctrl_lookup_state(lcd->pinctrl, "disable");
	if (!IS_ERR(pinctrl_state)) {
		dev_info(&pdev->dev,
			"[LCD] %s: found pinctrl disable\n", __func__);
			lcd->pin_disable = pinctrl_state;
	}

	if ((!lcd->pin_enable && lcd->pin_disable) ||
			(lcd->pin_enable && !lcd->pin_disable)) {
			dev_warn(&pdev->dev,
			"[LCD] %s: warning - pinctrl %s not exist\n", __func__,
			!lcd->pin_enable ? "enable" : "disable");
	}

	return 0;
}

static int gen_panel_parse_dt_esd(
		struct device_node *np, struct lcd *lcd)
{
	int ret;

	if (!np)
		return -EINVAL;

	if (!of_property_read_bool(np, "gen-panel-esd-en"))
		return 0;

	lcd->esd_gpio = of_get_named_gpio(np,
			"gen-panel-esd-gpio", 0);
	if (unlikely(lcd->esd_gpio < 0)) {
		pr_err("[LCD] %s: of_get_named_gpio failed: %d\n",
				__func__, lcd->esd_gpio);
		return -EINVAL;
	}
	ret = of_property_read_u32(np, "gen-panel-esd-type",
			&lcd->esd_type);
	if (unlikely(ret < 0)) {
		pr_err("[LCD] %s: failed to read property(%s)\n",
				__func__, "gen-panel-esd-type");
		return -EINVAL;
	}
	lcd->esd_en = true;
	return 0;
}

static int gen_panel_parse_rst_gpio(
		struct device_node *np, struct lcd *lcd)
{
	if (!of_property_read_bool(np, "gen-panel-rst-gpio-en"))
		return 0;

	lcd->rst_gpio_en = 1;
	lcd->rst_gpio = of_get_named_gpio(np,
			"gen-panel-rst-gpio", 0);
	if (unlikely(lcd->rst_gpio < 0)) {
		pr_err("[LCD] %s: of_get_named_gpio failed: %d\n",
				__func__, lcd->esd_gpio);
		return -EINVAL;
	}

	return 0;
}

static int gen_panel_parse_dt_temp_compensation(
		struct device_node *parent, struct lcd *lcd)
{
	struct temp_compensation *temp_comp;
	struct device_node *temp_comp_node = NULL;
	int nr_nodes = 0, i = 0, ret = 0;
	u32 data_len, temperature;

	if (lcd->temp_comp) {
		pr_warn("[LCD] %s: temperature compensation already exist\n",
				__func__);
		return 0;
	}
	nr_nodes = of_get_child_count(parent);
	temp_comp = kmalloc(sizeof(struct temp_compensation) * nr_nodes,
			GFP_KERNEL);
	if (unlikely(!temp_comp)) {
		ret = -ENOMEM;
		goto err_temp_compensation;
	};

	for_each_child_of_node(parent, temp_comp_node) {
		pr_info("[LCD] found (%s)\n", temp_comp_node->name);
		ret = of_property_read_u32(temp_comp_node, "trig-type",
				&temp_comp[i].trig_type);
		if (unlikely(ret < 0)) {
			pr_err("[LCD] %s: failed to read property(%s)\n",
					__func__, "trig-type");
			ret = -EINVAL;
			goto err_temp_compensation;
		}
		ret = of_property_read_u32(temp_comp_node, "temperature",
				&temperature);
		if (unlikely(ret < 0)) {
			pr_err("[LCD] %s: failed to read property(%s)\n",
					__func__, "temperature");
			ret = -EINVAL;
			goto err_temp_compensation;
		}
		if (temperature) {
			temp_comp[i].temperature = temperature - 273;
			pr_info("[LCD]%s: set temperature threshold (%d)\n",
					__func__, temp_comp[i].temperature);
		} else {
			pr_warn("[LCD]%s:temperature is not initialized\n",
					__func__);
		}
		if (of_find_property(temp_comp_node, "old-data", &data_len)) {
			temp_comp[i].old_data = kzalloc(sizeof(char) *
					data_len, GFP_KERNEL);
			of_property_read_u8_array(temp_comp_node, "old-data",
					temp_comp[i].old_data, data_len);
			temp_comp[i].data_len = data_len;
		} else {
			pr_err("[LCD] %s: failed to read property(%s)\n",
					__func__, "old-data");
			ret = -EINVAL;
			goto err_temp_compensation;
		}
		if (of_find_property(temp_comp_node, "new-data", &data_len)) {
			temp_comp[i].new_data = kzalloc(sizeof(char) *
					temp_comp[i].data_len, GFP_KERNEL);
			of_property_read_u8_array(temp_comp_node, "new-data",
					temp_comp[i].new_data, data_len);
			temp_comp[i].data_len = data_len;
		} else {
			pr_err("[LCD] %s: failed to read property(%s)\n",
					__func__, "new-data");
			ret = -EINVAL;
			goto err_temp_compensation;
		}
		i++;
	}

	lcd->temp_comp = temp_comp;
	lcd->nr_temp_comp = nr_nodes;
	lcd->temp_comp_en = true;
	return 0;

err_temp_compensation:
	if (temp_comp) {
		for (i = 0; i < nr_nodes; i++) {
			kfree(temp_comp[i].new_data);
			kfree(temp_comp[i].old_data);
		}
		kfree(temp_comp);
	}
	return ret;
}

static int gen_panel_parse_dt_rd_info(struct device_node *np,
		const char *propname, struct read_info *rd_info)
{
	int i, sz;
	unsigned char *buf;

	if (of_find_property(np, propname, &sz)) {
		buf = kmalloc(sizeof(unsigned char) * sz, GFP_KERNEL);

		if (unlikely(!buf))
			return -EINVAL;

		of_property_read_u8_array(np, propname, buf, sz);
		for (i = 0; i < sz / 3; i++) {
			rd_info[i].reg = buf[i * 3];
			rd_info[i].idx = buf[i * 3 + 1];
			rd_info[i].len = buf[i * 3 + 2];
		}
		kfree(buf);
	}

	return 0;
}

static void print_buffer(u8 *data, int len)
{
	unsigned int i, n = 0;
	u8 buf[256];

	for (i = 0; i < len; i++) {
		n += sprintf(buf + n, "0x%02X ", data[i]);
		if ((!(i + 1) % 16) || (i + 1) == len)
			n += sprintf(buf + n, "\n");
	}

	pr_info("[LCD] data = %s", buf);
}

static int gen_panel_manipulate_cmd_table(struct lcd *lcd,
		struct manipulate_table *mani)
{
	struct gen_cmd_desc *desc;
	int op_index, i;
	unsigned int reg = 0;

	for (op_index = 0; op_index < PANEL_OP_CMD_MAX; op_index++)
		if (!strncmp(mani->cmd.name, op_cmd_names[op_index],
				sizeof(*op_cmd_names[op_index])))
			break;

	if (op_index == PANEL_OP_CMD_MAX) {
		pr_err("[LCD] %s, %s setting table not exist\n",
				__func__, mani->cmd.name);
		return -EINVAL;
	}

	mutex_lock(&lcd->access_ok);
	for (i = 0; i < mani->cmd.nr_desc; i++) {
		memcpy(&reg, &mani->cmd.desc[i].data[addr_offset], addr_type);
		desc = find_op_cmd_desc(lcd, op_index, reg);
		if (!desc) {
			pr_warn("[LCD] %s, reg:0x%08X not found in %s\n",
					__func__, reg, op_cmd_names[op_index]);
			mutex_unlock(&lcd->access_ok);
			return -EINVAL;
		}

		pr_info("[LCD] %s, reg:0x%08X\n", __func__, reg);
		print_buffer(&desc->data[data_offset],
				desc->length - data_offset);
		memcpy(&desc->data[data_offset],
				&mani->cmd.desc[i].data[data_offset],
				desc->length - data_offset);
		print_buffer(&desc->data[data_offset],
				desc->length - data_offset);
	}
	mutex_unlock(&lcd->access_ok);

	return 0;
}

static int gen_panel_parse_dt_action(struct device_node *sysfs_node,
		struct gen_dev_attr *gen_attr)
{
	struct device_node *np_mani;
	int config, state, size, ret;
	struct manipulate_action *action;
	struct property *prop;
	const __be32 *list;
	char *propname;
	phandle phandle;

	for (state = 0; ; state++) {
		/* Retrieve the action-* property */
		propname = kasprintf(GFP_KERNEL, "action-%d", state);
		prop = of_find_property(sysfs_node, propname, &size);
		kfree(propname);
		if (!prop) {
			pr_debug("end of action\n");
			break;
		}

		list = prop->value;
		size /= sizeof(*list);
		action = kmalloc(sizeof(struct manipulate_action),
				GFP_KERNEL);
		ret = of_property_read_string_index(sysfs_node,
				"action-names", state, &action->name);
		if (unlikely(ret < 0)) {
			pr_err("%s, action-names[%d] not exist\n",
					__func__, state);
			kfree(action);
			goto err_parse_action;
		}

		pr_info("add action - %s\n", action->name);
		action->nr_mani = size;
		action->pmani =
			kzalloc(sizeof(struct manipulate_table *)
					* size, GFP_KERNEL);
		for (config = 0; config < size; config++) {
			phandle = be32_to_cpup(list++);
			np_mani = of_find_node_by_phandle(phandle);
			if (!np_mani) {
				pr_err("%s, invalid phandle\n", __func__);
				kfree(action->pmani);
				kfree(action);
				of_node_put(np_mani);
				goto err_parse_action;
			}

			action->pmani[config] =
				find_manipulate_table_by_node(np_mani);
			of_node_put(np_mani);
		}
		list_add_tail(&action->list, &gen_attr->action_list);
	}

	return 0;

err_parse_action:
	free_action_list(&gen_attr->action_list);
	return ret;
}

static int gen_panel_parse_dt_sysfs_node(struct device_node *parent)
{
	struct device_node *sysfs_node;
	struct gen_dev_attr *gen_attr;
	int ret;
	/*const __be32 *list;*/

	for_each_child_of_node(parent, sysfs_node) {
		gen_attr = kmalloc(sizeof(struct gen_dev_attr), GFP_KERNEL);

		if (unlikely(!gen_attr))
			goto err_parse_sysfs_node;

		INIT_LIST_HEAD(&gen_attr->action_list);
		gen_attr->attr_name = sysfs_node->name;
		pr_info("add sysfs_node - %s\n", sysfs_node->name);

		ret = gen_panel_parse_dt_action(sysfs_node, gen_attr);
		if (unlikely(ret < 0)) {
			pr_warn("%s, failed to parse action\n", __func__);
			kfree(gen_attr);
			goto err_parse_sysfs_node;
		}

		gen_panel_init_dev_attr(gen_attr);
		gen_attr->action =
			list_entry(gen_attr->action_list.next,
					struct manipulate_action, list);
		list_add_tail(&gen_attr->list, &attr_list);
	}

	return 0;

err_parse_sysfs_node:
	free_gen_dev_attr();

	return -EINVAL;
}

static int gen_panel_parse_dt_manipulate_table(
		struct device_node *mani_node, struct lcd *lcd)
{
	struct device_node *np;
	struct manipulate_table *mani;
	int ret, i;

	for_each_child_of_node(mani_node, np) {
		if (strncmp(np->name, "manipulate-",
					strlen("manipulate-")))
			continue;

		mani = kzalloc(sizeof(struct manipulate_table), GFP_KERNEL);
		if (!mani) {
			pr_err("[LCD] %s: failed to allocate memory\n",
				__func__);
			goto err_parse_mani;
		}
		ret = of_property_read_u32(np, "type", &mani->type);
		if (unlikely(ret < 0))
			pr_debug("[LCD] %s: failed to read property(%s)\n",
					__func__, np->name);

		for (i = 0; i < PANEL_OP_CMD_MAX; i++) {
			if (of_find_property(np, op_cmd_names[i], NULL)) {
				ret = gen_panel_parse_dt_dcs_cmds(np,
						op_cmd_names[i], &mani->cmd);
				if (ret) {
					pr_warn("[LCD]%s,failed to parse %s\n",
							__func__,
							op_cmd_names[i]);
					kfree(mani);
					goto err_parse_mani;
				}
				break;
			}
		}
		if (i == PANEL_OP_CMD_MAX) {
			pr_err("[LCD] %s, op cmd table not found\n", __func__);
			kfree(mani);
			goto err_parse_mani;
		}
		mani->np = np;
		list_add_tail(&mani->list, &mani_list);
	}

	return 0;

err_parse_mani:
	free_manipulate_table();
	return -EINVAL;
}

static int gen_panel_parse_dt_panel(
		struct device_node *np, struct lcd *lcd)
{
	struct device_node *temp_comp_node;
	u32 panel_ids[8];
	int ret, i;

	if (!np)
		return -EINVAL;

	ret = of_property_read_string(np, "gen-panel-manu",
			&lcd->manufacturer_name);
	ret = of_property_read_string(np, "gen-panel-model",
			&lcd->panel_model_name);
	ret = of_property_read_string(np, "gen-panel-name",
			&lcd->panel_name);
	ret = of_property_read_u32_array(np, "gen-panel-id",
			panel_ids, nr_panel_id);
	ret = of_property_read_u32(np, "gen-panel-type",
			&lcd->mode.panel_type);

	ret = of_property_read_u32(np, "gen-panel-refresh",
			&lcd->mode.refresh);
	ret = of_property_read_u32(np, "gen-panel-xres",
			&lcd->mode.xres);
	ret = of_property_read_u32(np, "gen-panel-yres",
			&lcd->mode.yres);
	ret = of_property_read_u32(np, "gen-panel-xres",
			&lcd->mode.real_xres);
	ret = of_property_read_u32(np, "gen-panel-yres",
			&lcd->mode.real_yres);
	ret = of_property_read_u32(np, "gen-panel-width",
			&lcd->mode.width);
	ret = of_property_read_u32(np, "gen-panel-height",
			&lcd->mode.height);

	ret = of_property_read_u32(np, "gen-panel-dsi-mode",
			&lcd->mode.dsi_mode);
	ret = of_property_read_u32(np, "gen-panel-lcm-if",
			&lcd->mode.lcm_if);
	ret = of_property_read_u32(np, "gen-panel-switch-mode-en",
			&lcd->mode.switch_mode_en);

	ret = of_property_read_u32(np, "gen-panel-direction",
			&lcd->mode.direc);
	ret = of_property_read_u32(np, "gen-panel-suspend-mode",
			&lcd->mode.suspend_mode);
	ret = of_property_read_u32(np, "gen-panel-bus-width",
			&lcd->mode.bus_width);
	ret = of_property_read_u32(np, "gen-panel-lanes",
			&lcd->mode.number_of_lane);
	ret = of_property_read_u32(np, "gen-panel-phy-feq",
			&lcd->mode.dsi_freq);

	ret = of_property_read_u32(np, "gen-panel-ssc-disable",
			&lcd->mode.ssc_disable);
	ret = of_property_read_u32(np, "gen-panel-clk-lp-enable",
			&lcd->mode.clk_lp_enable);
	ret = of_property_read_u32(np, "gen-panel-cont-clock",
			&lcd->mode.cont_clk);
	ret = of_property_read_u32(np, "gen-panel-dbi-te",
			&lcd->mode.te_dbi_mode);
	ret = of_property_read_u32(np, "gen-panel-te-pol",
			&lcd->mode.te_pol);


	ret = of_property_read_u32(np, "gen-panel-h-front-porch",
			&lcd->mode.right_margin);
	ret = of_property_read_u32(np, "gen-panel-h-back-porch",
			&lcd->mode.left_margin);
	ret = of_property_read_u32(np, "gen-panel-h-sync-width",
			&lcd->mode.hsync_len);
	ret = of_property_read_u32(np, "gen-panel-h-active-line",
			&lcd->mode.h_active_line);
	ret = of_property_read_u32(np, "gen-panel-v-front-porch",
			&lcd->mode.lower_margin);
	ret = of_property_read_u32(np, "gen-panel-v-back-porch",
			&lcd->mode.upper_margin);
	ret = of_property_read_u32(np, "gen-panel-v-sync-width",
			&lcd->mode.vsync_len);
	ret = of_property_read_u32(np, "gen-panel-v-active-line",
			&lcd->mode.v_active_line);

	ret = of_property_read_u32(np, "gen-panel-color-order",
			&lcd->mode.color_order);
	ret = of_property_read_u32(np, "gen-panel-trans-seq",
			&lcd->mode.trans_seq);
	ret = of_property_read_u32(np, "gen-panel-padding-on",
			&lcd->mode.padding_on);
	ret = of_property_read_u32(np, "gen-panel-rgb-format",
			&lcd->mode.rgb_format);

	ret = of_property_read_u32(np, "gen-panel-dsi_wmem_cont",
			&lcd->mode.wmem_cont);
	ret = of_property_read_u32(np, "gen-panel-dsi_rmem_cont",
			&lcd->mode.rmem_cont);
	ret = of_property_read_u32(np, "gen-panel-dsi_packet_size",
			&lcd->mode.packet_size);
	ret = of_property_read_u32(np, "gen-panel-dsi_intermediate_buff_num",
			&lcd->mode.inter_buff_num);
	ret = of_property_read_u32(np, "gen-panel-ps-to-bpp",
			&lcd->mode.ps_to_bpp);

	ret = of_property_read_u32(np, "gen-panel-cust-esd-enable",
			&lcd->mode.cust_esd_enable);
	ret = of_property_read_u32(np, "gen-panel-esd-check-enable",
			&lcd->mode.esd_check_enable);

	ret = of_property_read_u32(np, "gen-panel-partial-update",
			&lcd->mode.patial_update);

	ret = of_property_read_u32(np, "gen-panel-partial-col-multi-num",
			&lcd->mode.patial_col_multi_num);
	ret = of_property_read_u32(np, "gen-panel-partial-page-multi-num",
			&lcd->mode.patial_page_multi_num);
	ret = of_property_read_u32(np, "gen-panel-partial-page-min-size",
			&lcd->mode.patial_page_min_size);
	ret = of_property_read_u32(np, "gen-panel-partial-col-min-size",
			&lcd->mode.patial_col_min_size);

	ret = of_property_read_u32(np, "gen-panel-factory-panel-swap",
			&lcd->mode.fac_panel_swap);

	ret = of_property_read_u32(np, "gen-panel-pkt-dinfo-addr-offset",
			&addr_offset);
	ret = of_property_read_u32(np, "gen-panel-pkt-dinfo-addr-length",
			&addr_type);
	ret = of_property_read_u32(np, "gen-panel-pkt-dinfo-data-offset",
			&data_offset);
	ret = of_property_read_u32(np, "gen-panel-pkt-dinfo-data-length",
			&data_type);
	pr_info("[LCD] %s, addr[offset:%d, len:%d], data[offset:%d, len:%d]\n",
			__func__, addr_offset, addr_type,
			data_offset, data_type);
	if (!addr_type || !data_type) {
		pr_err("[LCD]%s, unknown packet data type(addr:%d, data:%d)\n",
				__func__, addr_type, data_type);
		return -EINVAL;
	}

	lcd->op_cmds = kzalloc(PANEL_OP_CMD_MAX *
			sizeof(struct gen_cmds_info), GFP_KERNEL);

	/* parse command tables in dt */
	for (i = 0; i < PANEL_OP_CMD_MAX; i++)
		gen_panel_parse_dt_dcs_cmds(np, op_cmd_names[i],
				&lcd->op_cmds[i]);

	ret = of_property_read_u32(np,
			"gen-panel-backlight-set-brightness-reg",
			&lcd->set_brt_reg);
	ret = of_property_read_u32(np,
			"gen-panel-mdnie-color-adjustment-mode-reg",
			&lcd->color_adj_reg);
	gen_panel_parse_dt_rd_info(np, "gen-panel-read-id",
			lcd->id_rd_info);
	gen_panel_parse_dt_rd_info(np, "gen-panel-read-hbm",
			lcd->hbm_rd_info);
	gen_panel_parse_dt_rd_info(np, "gen-panel-read-mtp",
			lcd->mtp_rd_info);
	of_property_read_u32(np, "gen-panel-read-status-regs",
			&lcd->status_reg);
	of_property_read_u32(np, "gen-panel-read-status-ok",
			&lcd->status_ok);

	temp_comp_node = of_find_node_by_name(np,
			"gen-panel-temp-compensation");
	if (temp_comp_node)
		gen_panel_parse_dt_temp_compensation(temp_comp_node, lcd);
	of_node_put(temp_comp_node);

	return 0;
}

static int gen_panel_parse_dt_extpin(struct device_node *np,
		struct device *dev, const char *propname, struct extpin *pin)
{
	int ret;

	if (of_property_read_string(np, "pin-name", &pin->name))
		return -EINVAL;

	if (of_property_read_u32(np, "pin-type", &pin->type))
		return -EINVAL;

	if (pin->type == EXT_PIN_REGULATOR) {
		struct device_node *node;
		struct regulator *supply;

		node = of_parse_phandle(np, "pin-supply", 0);
		if (!node) {
			pr_err("[LCD] %s: failed to parse pin-supply\n",
				__func__);
			return -EINVAL;
		}
		supply = regulator_get(dev, node->name);
		if (IS_ERR_OR_NULL(supply)) {
			pr_err("[LCD] %s regulator(%s) get error!\n",
					__func__, node->name);
			of_node_put(node);
			regulator_put(supply);
			return -EINVAL;
		}
		pin->supply = supply;

		if (regulator_is_enabled(pin->supply)) {
			ret = regulator_enable(pin->supply);
			if (unlikely(ret)) {
				pr_err("[LCD] regulator(%s) enable failed\n",
					pin->name);
			}
		}
		pr_info("[LCD] %s, get regulator(%s)\n",
				pin->name, node->name);
		of_node_put(node);
	} else {
		int gpio = of_get_named_gpio(np, "pin-gpio", 0);

		if (unlikely(gpio < 0)) {
			pr_err("[LCD] %s: of_get_named_gpio failed: %d\n",
					__func__, gpio);
			return -EINVAL;
		}

		ret = gpio_request(gpio, pin->name);
		if (unlikely(ret)) {
			pr_err("[LCD] %s: gpio_request failed: %d\n",
					__func__, ret);
			return -EINVAL;
		}
		pin->gpio = gpio;
		pr_info("[LCD] %s, get gpio(%d)\n", pin->name, pin->gpio);
	}
	mutex_init(&pin->expires_lock);

	return 0;
}

static int gen_panel_parse_dt_extpin_ctrl_seq(struct device_node *np,
		const char *propname, struct extpin_ctrl_list *out_ctrl)
{
	static struct extpin_onoff_seq {
		phandle phandle;
		u32 on;
		u32 usec;
	} seq[20];
	unsigned int nr_seq, len, i;
	struct extpin_ctrl *pin_ctrl;
	struct device_node *node;
	const char *name;

	if (!of_find_property(np, propname, &len)) {
		pr_err("[LCD] %s not found\n", propname);
		return -EINVAL;
	}

	if (len > sizeof(seq)) {
		pr_err("[LCD] %s: out of range(len:%d, buf_size:%lu)\n",
				__func__, len, sizeof(seq));
		return -EINVAL;
	}

	if (of_property_read_u32_array(np, propname,
			(u32 *)seq, len / sizeof(u32))) {
			pr_err("[LCD] %s, failed to parse %s\n",
					__func__, propname);
			return -ENODATA;
	}
	nr_seq = len / sizeof(struct extpin_onoff_seq);
	pin_ctrl = kzalloc(sizeof(struct extpin_ctrl) * nr_seq, GFP_KERNEL);
	if (!pin_ctrl) {
		pr_err("[LCD] %s, memory allocation failed(nr_seq:%d)\n",
				__func__, nr_seq);
		return -ENOMEM;
	}

	for (i = 0; i < nr_seq; i++) {
		node = of_find_node_by_phandle(seq[i].phandle);
		if (unlikely(!node)) {
			pr_err("[LCD] %s: failed to find node(%s)\n",
					__func__, propname);
			goto err_find_node;
		}

		if (of_property_read_string(node, "pin-name", &name)) {
			pr_err("[LCD] %s: failed to read property(%s)\n",
					__func__, propname);
			goto err_find_node;
		}
		pin_ctrl[i].pin = find_extpin_by_name(name);
		if (unlikely(!pin_ctrl[i].pin)) {
			pr_err("[LCD] external pin(%s) not found\n", name);
			goto err_find_node;
		}
		pin_ctrl[i].on = seq[i].on;
		pin_ctrl[i].usec = seq[i].usec;
		pr_info("[LCD] [%d] %s, %s, %dusec\n", i, name,
				pin_ctrl[i].on ? "on" : "off",
				pin_ctrl[i].usec);
		of_node_put(node);
	}
	out_ctrl->ctrls = pin_ctrl;
	out_ctrl->nr_ctrls = nr_seq;
	return 0;

err_find_node:
	of_node_put(node);
	kfree(pin_ctrl);
	return -EINVAL;
}

static int gen_panel_parse_dt_external_pin(struct device_node *pin_node,
		struct device *dev, struct lcd *lcd)
{
	struct device_node *np = NULL;
	struct extpin *pin;

	if (!pin_node)
		return -EINVAL;

	for_each_child_of_node(pin_node, np) {
		pin = kmalloc(sizeof(struct extpin), GFP_KERNEL);
		if (!pin) {
			pr_err("[LCD] %s: failed to allocate memory\n",
				__func__);
			goto extpin_add_fail;
		}
		if (gen_panel_parse_dt_extpin(np, dev, pin_node->name, pin)) {
			pr_err("[LCD] %s: failed to parse %s node\n",
					__func__, pin_node->name);
			kfree(pin);
			goto extpin_add_fail;
		}
		list_add_tail(&pin->list, &extpin_list);
	}

	gen_panel_parse_dt_extpin_ctrl_seq(pin_node,
			"panel-ext-pin-on-fir", &lcd->extpin_on_fir_seq);
	gen_panel_parse_dt_extpin_ctrl_seq(pin_node,
			"panel-ext-pin-on-sec", &lcd->extpin_on_sec_seq);
	gen_panel_parse_dt_extpin_ctrl_seq(pin_node,
			"panel-ext-pin-on-thi", &lcd->extpin_on_thi_seq);
	gen_panel_parse_dt_extpin_ctrl_seq(pin_node,
			"panel-ext-pin-on-fou", &lcd->extpin_on_fou_seq);

	gen_panel_parse_dt_extpin_ctrl_seq(pin_node,
			"panel-ext-pin-off", &lcd->extpin_off_seq);

	return 0;

extpin_add_fail:
	free_extpin();
	return -EINVAL;
}
#endif

static ssize_t panel_name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lcd *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%s", lcd->panel_name);
}

static ssize_t lcd_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lcd *lcd = dev_get_drvdata(dev);
	const char *manufacturer_name = "INH";
	u32 panel_id = lcd->id;

	if (lcd->manufacturer_name)
		manufacturer_name = lcd->manufacturer_name;
	if (lcd->panel_model_name) {
		return sprintf(buf, "%s_%s",
				manufacturer_name,
				lcd->panel_model_name);
	}

	/*HW indicator of DFMS */
	/* if (panel_id < 0x000000) *//*If mipi read fail when the panel id is read*/
	if (!lcd->id)
		return sprintf(buf, "NULL");

	return sprintf(buf, "%s_%02x%02x%02x",
			manufacturer_name,
			(panel_id >> 16) & 0xFF,
			(panel_id >> 8) & 0xFF,
			panel_id  & 0xFF);
}

static ssize_t window_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lcd *lcd = dev_get_drvdata(dev);
	u32 panel_id = lcd->id;

	char temp[15];
	int id1, id2, id3;

	id1 = (panel_id & 0x00FF0000) >> 16;
	id2 = (panel_id & 0x0000FF00) >> 8;
	id3 = panel_id & 0xFF;

	snprintf(temp, sizeof(temp), "%x %x %x\n", id1, id2, id3);
	strlcat(buf, temp, 15);
	return strnlen(buf, 15);
}

static ssize_t lux_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lcd *lcd = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", lcd->hbm);
}

static ssize_t lux_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd *lcd = dev_get_drvdata(dev);
	int value, rc;
	static int pre_state;

	rc = kstrtoint(buf, 0, &value);

	if (rc < 0)
		return rc;

	pr_info("[LCD] %s, %d\n", __func__, value);

	lcd->hbm = (value < 20000) ? 0 : 1;

#ifdef CONFIG_GEN_PANEL_MDNIE
	if (pre_state != lcd->hbm)
		update_mdnie_mode(&lcd->mdnie);
#endif

	pre_state = lcd->hbm;

	return size;
}

static DEVICE_ATTR(panel_name, 0444, panel_name_show, NULL);
static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(lux, 0644, lux_show, lux_store);

static ssize_t gen_panel_common_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct gen_dev_attr *gen_attr =
		container_of(attr, struct gen_dev_attr, dev_attr);
	const char *action_name = "none";
	unsigned int len;

	struct manipulate_action *action;

	len = sprintf(buf, "[action list]\n");
	list_for_each_entry(action, &gen_attr->action_list, list)
		len += sprintf(buf + len, "%s\n", action_name);

	if (gen_attr->action && gen_attr->action->name)
		action_name = gen_attr->action->name;

	return sprintf(buf + len, "%s : %s\n", attr->attr.name, action_name);
}

static ssize_t gen_panel_common_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct gen_dev_attr *gen_attr =
		container_of(attr, struct gen_dev_attr, dev_attr);
	struct lcd *lcd = dev_get_drvdata(dev);
	unsigned int value, i;
	struct manipulate_action *action;

	if (kstrtoul(buf, 0, (unsigned long *)&value))
		return -EINVAL;

	pr_info("[LCD] %s, %s\n", __func__, gen_attr->dev_attr.attr.name);
	action = find_mani_action_by_name(&gen_attr->action_list, buf);

	if (!action || gen_attr->action == action)
		return size;

	for (i = 0; i < action->nr_mani; i++)
		gen_panel_manipulate_cmd_table(lcd, action->pmani[i]);

	gen_attr->action = action;

	return size;
}

static int gen_panel_init_dev_attr(struct gen_dev_attr *gen_attr)
{
	struct device_attribute *dev_attr = &gen_attr->dev_attr;

	sysfs_attr_init(&dev_attr->attr);
	dev_attr->attr.name = gen_attr->attr_name;
	dev_attr->attr.mode = 0644;
	dev_attr->show = gen_panel_common_show;
	dev_attr->store = gen_panel_common_store;

	return 0;
}

static int fb_lcd_notifier_callback(struct notifier_block *self,
			unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	struct lcd *lcd;
	int blank;

	switch (event) {
	case FB_EVENT_BLANK:
	case FB_EARLY_EVENT_BLANK:
		break;
	default:
		return NOTIFY_DONE;
	}

	lcd = container_of(self, struct lcd, fb_notifier);

	blank = *(int *)evdata->data;

	if (evdata->info->node != 0)
		return NOTIFY_DONE;

	if (blank == FB_BLANK_UNBLANK && event == FB_EVENT_BLANK) {
		mutex_lock(&lcd->access_ok);
		lcd->mdnie_disable = 0;
		mutex_unlock(&lcd->access_ok);
#ifdef CONFIG_GEN_PANEL_MDNIE
		update_mdnie_mode(&lcd->mdnie);
#endif
		pr_info("%s: unblank!\n", __func__);
	} else if (blank == FB_BLANK_POWERDOWN && event == FB_EARLY_EVENT_BLANK) {
		mutex_lock(&lcd->access_ok);
		lcd->mdnie_disable = 1;
		mutex_unlock(&lcd->access_ok);
		pr_info("%s: blank!\n", __func__);
	}

	return NOTIFY_DONE;
}

int gen_panel_probe(struct device_node *node, struct lcd *lcd)
{
	struct device_node *panel_node, *pin_node,
			   *backlight_node, *mani_node, *sysfs_node;
	struct platform_device *pdev = of_find_device_by_node(node);
	static struct gen_dev_attr *gen_attr;
	int ret;

	if (IS_ENABLED(CONFIG_OF)) {
		/* 1. Parse dt of pinctrl */
		ret = gen_panel_parse_dt_pinctrl(pdev, lcd);
		if (unlikely(ret)) {
			dev_err(&pdev->dev, "fail to parse pinctrl\n");
			goto err_no_platform_data;
		}

		/* 2. Parse dt of esd */
		ret = gen_panel_parse_dt_esd(node, lcd);
		if (unlikely(ret)) {
			dev_err(&pdev->dev, "fail to parse dsi-esd\n");
			goto err_no_platform_data;
		}

		/* 3. Find & Parse dt of external pin */
		pin_node = of_find_node_by_name(node, "panel-ext-pin");
		if (unlikely(!pin_node)) {
			dev_err(&pdev->dev, "not found pin_node!\n");
			goto no_external_pin;
		}
		ret = gen_panel_parse_dt_external_pin(pin_node,
				&pdev->dev, lcd);
		if (unlikely(ret)) {
			dev_err(&pdev->dev, "failed to parse pin_node\n");
			goto err_no_platform_data;
		}
		of_node_put(pin_node);

no_external_pin:
		/* 4. Find dt of backlight */
		backlight_node = gen_panel_find_dt_backlight(node,
				"gen-panel-backlight");
		if (backlight_node) {
			lcd->bd = of_find_backlight_by_node(backlight_node);
			of_node_put(backlight_node);
		}

		/* 5. Find & Parse dt of panel node */
		panel_node = gen_panel_find_dt_panel(node, NULL);
		if (unlikely(!panel_node)) {
			dev_err(&pdev->dev, "not found panel_node!\n");
			goto err_no_platform_data;
		}
		ret = gen_panel_parse_dt_panel(panel_node, lcd);
		if (unlikely(ret)) {
			dev_err(&pdev->dev, "failed to parse panel_node\n");
			of_node_put(panel_node);
			goto err_no_platform_data;
		}

		/* 6. Parse manipulation command table node */
		mani_node = of_find_node_by_name(panel_node,
				"gen-panel-manipulate-table");
		if (mani_node) {
			ret = gen_panel_parse_dt_manipulate_table(
					mani_node, lcd);
			if (unlikely(ret))
				dev_err(&pdev->dev, "failed to parse mani_node\n");
			of_node_put(mani_node);
		}

		/* 7. Parse sysfs node for a certain action */
		sysfs_node = of_find_node_by_name(panel_node,
				"gen-panel-sysfs-node");
		if (sysfs_node) {
			ret = gen_panel_parse_dt_sysfs_node(sysfs_node);
			if (unlikely(ret))
				dev_err(&pdev->dev,
				"failed to parse sysfs node\n");
			of_node_put(sysfs_node);
		}

		/* 8. Parse platform dependent properties */
		ret = gen_panel_parse_dt_plat(lcd, panel_node);
		if (unlikely(ret)) {
			dev_err(&pdev->dev, "failed to parse plat\n");
			of_node_put(panel_node);
			goto err_no_platform_data;
		}

		ret = gen_panel_parse_rst_gpio(node, lcd);
		if (unlikely(ret)) {
			dev_err(&pdev->dev, "fail to parse rst-gpio\n");
			goto err_no_platform_data;
		}
		of_node_put(panel_node);
	}

	lcd->id = lcdtype;
	lcd->set_panel_id = &set_panel_id;
	lcd->power = 1;
	lcd->temperature = 25;
	lcd->disp_cmdq_en = true;
	lcd->class = class_create(THIS_MODULE, "lcd");
	if (IS_ERR(lcd->class)) {
		ret = PTR_ERR(lcd->class);
		pr_err("[LCD] Failed to create class!");
		goto err_class_create;
	}

	lcd->dev = device_create(lcd->class, NULL, 0, "%s", "panel");
	if (IS_ERR(lcd->dev)) {
		ret = PTR_ERR(lcd->dev);
		pr_err("[LCD] Failed to create device(panel)!\n");
		goto err_device_create;
	}

	dev_set_drvdata(lcd->dev, lcd);
	mutex_init(&lcd->access_ok);

	ret = device_create_file(lcd->dev, &dev_attr_panel_name);
	if (unlikely(ret < 0)) {
		pr_err("[LCD] Failed to create device file(%s)!\n",
				dev_attr_panel_name.attr.name);
		goto err_lcd_device;
	}
	ret = device_create_file(lcd->dev, &dev_attr_lcd_type);
	if (unlikely(ret < 0)) {
		pr_err("[LCD] Failed to create device file(%s)!\n",
				dev_attr_lcd_type.attr.name);
		goto err_lcd_name;
	}

	ret = device_create_file(lcd->dev, &dev_attr_window_type);
	if (unlikely(ret < 0)) {
		pr_err("[LCD] Failed to create device file(%s)!\n",
				dev_attr_window_type.attr.name);
		goto err_lcd_type;
	}

	ret = device_create_file(lcd->dev, &dev_attr_lux);
	if (unlikely(ret < 0)) {
		pr_err("[LCD] Failed to create device file(%s)!\n",
				dev_attr_lux.attr.name);
		goto err_lux;
	}

	list_for_each_entry(gen_attr, &attr_list, list) {
		ret = device_create_file(lcd->dev, &gen_attr->dev_attr);
		if (unlikely(ret < 0)) {
			pr_err("[LCD] Failed to create device file(%s)!\n",
					gen_attr->dev_attr.attr.name);
			goto err_window_type;
		}
	}

	glcd = lcd;

#ifdef CONFIG_GEN_PANEL_MDNIE
	gen_panel_attach_mdnie(&lcd->mdnie, &mdnie_ops);
#endif
	if (gen_panel_match_backlight(lcd->bd, GEN_PANEL_BL_NAME))
		gen_panel_backlight_device_register(lcd->bd,
				lcd, &backlight_ops);
	gen_panel_attach_tuning(lcd);

	lcd->fb_notifier.notifier_call = fb_lcd_notifier_callback;
	fb_register_client(&lcd->fb_notifier);

	dev_info(lcd->dev, "[LCD] device init done\n");

	return 0;

err_window_type:
	device_remove_file(lcd->dev, &dev_attr_window_type);
err_lcd_type:
	device_remove_file(lcd->dev, &dev_attr_lcd_type);
err_lcd_name:
	device_remove_file(lcd->dev, &dev_attr_panel_name);
err_lux:
	device_remove_file(lcd->dev, &dev_attr_lux);
err_lcd_device:
	device_destroy(lcd->class, 0);
err_device_create:
	class_destroy(lcd->class);
err_class_create:
err_no_platform_data:

	return ret;
}
EXPORT_SYMBOL(gen_panel_probe);

int gen_panel_remove(struct lcd *lcd)
{
	int i;
	static struct gen_dev_attr *gen_attr;

	gen_panel_detach_tuning(lcd);
#ifdef CONFIG_GEN_PANEL_MDNIE
	gen_panel_detach_mdnie(&lcd->mdnie);
#endif
	if (gen_panel_match_backlight(lcd->bd, GEN_PANEL_BL_NAME))
		gen_panel_backlight_device_unregister(lcd->bd);
	list_for_each_entry(gen_attr, &attr_list, list)
		device_remove_file(lcd->dev, &gen_attr->dev_attr);
	device_remove_file(lcd->dev, &dev_attr_panel_name);
	device_remove_file(lcd->dev, &dev_attr_lcd_type);
	device_remove_file(lcd->dev, &dev_attr_window_type);
	device_remove_file(lcd->dev, &dev_attr_lux);

	device_destroy(lcd->class, 0);
	class_destroy(lcd->class);

	free_gen_dev_attr();
	free_manipulate_table();
	kfree(lcd->extpin_on_fir_seq.ctrls);
	kfree(lcd->extpin_on_sec_seq.ctrls);
	kfree(lcd->extpin_on_thi_seq.ctrls);
	kfree(lcd->extpin_on_fou_seq.ctrls);

	kfree(lcd->extpin_off_seq.ctrls);
	free_extpin();
	/* free command tables */
	free_op_cmds_array(lcd);
	/* free temperature compensation */
	if (lcd->temp_comp) {
		for (i = 0; i < lcd->nr_temp_comp; i++) {
			kfree(lcd->temp_comp[i].new_data);
			kfree(lcd->temp_comp[i].old_data);
		}
		kfree(lcd->temp_comp);
	}
	kfree(lcd);
	return 0;
}
EXPORT_SYMBOL(gen_panel_remove);

MODULE_DESCRIPTION("GENERIC PANEL Driver");
MODULE_LICENSE("GPL");
