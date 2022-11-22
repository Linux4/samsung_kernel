/* drivers/video/backlight/gen_panel/gen-panel.c
 *
 * Copyright (C) 2014 Samsung Electronics Co, Ltd.
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
#include <linux/platform_data/gen-panel.h>
#include <linux/platform_data/gen-panel-bl.h>
#include <linux/platform_data/gen-panel-mdnie.h>
#include <linux/power_supply.h>
#include <linux/of_platform.h>

#include "../../sprdfb/sprdfb.h"
#include "../../sprdfb/sprdfb_panel.h"
#include "../../sprdfb/sprdfb_dispc_reg.h"


#ifdef CONFIG_GEN_PANEL_DYNAMIC_AID
#include "dynamic_aid.h"
#endif

//////////////////////////////
/* workaround for J3 OCTA */
#define mtp_len	33
#define elvss_len	1
#define hbm1_len	6
#define hbm2_len	15
#define date_len	7
#define cordi_len	4
bool read_done_from_boot = false;
char tmp_boot_panel_mtp[(mtp_len * 2) + 1];
char tmp_boot_panel_elvss[(elvss_len * 2) + 1];
char tmp_boot_panel_hbm1[(hbm1_len * 2) + 1];
char tmp_boot_panel_hbm2[(hbm2_len * 2) + 1];
char tmp_boot_panel_date[(date_len * 2) + 1];
char tmp_boot_panel_cordi[(cordi_len * 2) + 1];
u8 boot_panel_mtp[mtp_len];
u8 boot_panel_elvss[elvss_len];
u8 boot_panel_hbm1[hbm1_len];
u8 boot_panel_hbm2[hbm2_len];
u8 boot_panel_date[date_len];
u8 boot_panel_cordi[cordi_len];
char ch_lcd_hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
///////////////////////////////
u32 boot_panel_id;
static int panel_id_index;
static int nr_panel_id;
static struct lcd *glcd;
static LIST_HEAD(extpin_list);
static LIST_HEAD(mani_list);
static LIST_HEAD(attr_list);
#ifdef CONFIG_GEN_PANEL_OCTA
static int *aid_table;
static int *elvss_table;
static int *mps_table;
static int *elvss_temp_table;
static int *acl_table;
static int *rgb_offset;
static int *candela_offset;
static int *base_lux_table;
static int *gamma_curve_table;
static unsigned int *candela_table;
static unsigned int nr_candela;
static u8 *gamma_table;
static int vregout_voltage;
static u8 date[7];
static u8 coordinate[4];
#ifdef CONFIG_GEN_PANEL_DYNAMIC_AID
static struct smartdim_conf *sdimconf;
static int gen_panel_dimming_init(struct lcd *);
#endif
#endif
static int gen_panel_init_dev_attr(struct gen_dev_attr *);
static inline void print_data(char *data, int len);
static int prev_intensity;

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

static int __init get_boot_panel_id(char *str)
{
	char *endp;

	if ((str[0] == 'I') && (str[1] == 'D'))
		sscanf(&str[2], "%x", &boot_panel_id);
	else
		boot_panel_id = memparse(str, &endp);

	pr_info("[LCD] %s: boot_panel_id = 0x%8x\n", __func__, boot_panel_id);

	return 1;
}
early_param("lcd_id", get_boot_panel_id);

//////////////////////////////
/* workaround for J3 OCTA */
static int c_to_i(char ch)
{
	int i;
	for (i = 0; i < 16; i++) {
		if (ch_lcd_hex[i] == ch)
			return i;
	}
	return -1;
}
static int __init get_boot_octa_mtp(char *str)
{
	int i = 0;
	if ((str[0] == 'J') && (str[1] == '3'))
		memcpy(&tmp_boot_panel_mtp, &str[2], mtp_len * 2);

	for (i = 0; i < mtp_len; i++) {
		boot_panel_mtp[i] = (c_to_i(tmp_boot_panel_mtp[(i * 2)]) * 16) + c_to_i(tmp_boot_panel_mtp[(i * 2)+ 1]);
	}
	/* set flag for boot read data */
	read_done_from_boot = true;
	return 1;
}
early_param("MTP_DATA", get_boot_octa_mtp);

static int __init get_boot_octa_elvss(char *str)
{
	int i = 0;
	if ((str[0] == 'J') && (str[1] == '3'))
		memcpy(&tmp_boot_panel_elvss, &str[2], elvss_len * 2);

	for (i = 0; i < elvss_len; i++) {
		boot_panel_elvss[i] = (c_to_i(tmp_boot_panel_elvss[(i * 2)]) * 16) + c_to_i(tmp_boot_panel_elvss[(i * 2)+ 1]);
	}

	return 1;
}
early_param("ELVSS", get_boot_octa_elvss);

static int __init get_boot_octa_hbm1(char *str)
{
	int i = 0;
	if ((str[0] == 'J') && (str[1] == '3'))
		memcpy(&tmp_boot_panel_hbm1, &str[2], hbm1_len * 2);

	for (i = 0; i < hbm1_len; i++) {
		boot_panel_hbm1[i] = (c_to_i(tmp_boot_panel_hbm1[(i * 2)]) * 16) + c_to_i(tmp_boot_panel_hbm1[(i * 2)+ 1]);
	}

	return 1;
}
early_param("HBM1", get_boot_octa_hbm1);

static int __init get_boot_octa_hbm2(char *str)
{
	int i = 0;
	if ((str[0] == 'J') && (str[1] == '3'))
		memcpy(&tmp_boot_panel_hbm2, &str[2], hbm2_len * 2);

	for (i = 0; i < hbm2_len; i++) {
		boot_panel_hbm2[i] = (c_to_i(tmp_boot_panel_hbm2[(i * 2)]) * 16) + c_to_i(tmp_boot_panel_hbm2[(i * 2)+ 1]);
	}

	return 1;
}
early_param("HBM2", get_boot_octa_hbm2);

static int __init get_boot_octa_date(char *str)
{
	int i = 0;
	if ((str[0] == 'J') && (str[1] == '3'))
		memcpy(&tmp_boot_panel_date, &str[2], date_len * 2);

	for (i = 0; i < date_len; i++) {
		boot_panel_date[i] = (c_to_i(tmp_boot_panel_date[(i * 2)]) * 16) + c_to_i(tmp_boot_panel_date[(i * 2)+ 1]);
	}

	return 1;
}
early_param("DATE", get_boot_octa_date);

static int __init get_boot_octa_cordi(char *str)
{
	int i = 0;
	if ((str[0] == 'J') && (str[1] == '3'))
		memcpy(&tmp_boot_panel_cordi, &str[2], cordi_len * 2);

	for (i = 0; i < cordi_len; i++) {
		boot_panel_cordi[i] = (c_to_i(tmp_boot_panel_cordi[(i * 2)]) * 16) + c_to_i(tmp_boot_panel_cordi[(i * 2)+ 1]);
	}

	return 1;
}
early_param("CORDI", get_boot_octa_cordi);
/////////////////////////////////
bool lcd_connected(void)
{
	return !!(boot_panel_id);
}
EXPORT_SYMBOL(lcd_connected);

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
			pr_debug("[LCD] %s found desc (0x%08X)\n", __func__, reg);
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

static inline u8 *OP_CMD_DESC_DATA(struct gen_cmd_desc *desc)
{
	if (desc && desc->data && desc->length)
		return desc->data + data_offset;
	return NULL;
}

static inline u8 *OP_CMD_DESC_NDATA(struct gen_cmd_desc *desc,
		int offset, int len)
{
	if (desc && desc->data && desc->length >= (offset + len))
		return desc->data + data_offset + offset;
	return NULL;
}

static inline u8 *OP_CMD_DESC_NDATA_COPY(struct gen_cmd_desc *desc,
		int offset, u8 *src, int len)
{
	u8 *dest = OP_CMD_DESC_NDATA(desc, offset, len);
	if (unlikely(!dest)) {
		pr_warn("%s, out of range\n", __func__);
		return NULL;
	}
	return (u8 *)memcpy(dest, src, len);
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
	if (!op_cmd_valid(lcd, index))
		return -EINVAL;

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

#ifdef CONFIG_GEN_PANEL_OCTA
static inline int brt_to_candela(int br_stt, int br_end,
		int cd_stt, int cd_end, int value)
{
	if ((br_stt <= value) && (value <= br_end))
		return cd_stt + (((value - br_stt) * (cd_end - cd_stt) * 2
				/ (br_end - br_stt) + 1) >> 1);
	return -1;
}

static inline int find_candela_index_from_table(unsigned int candela,
		unsigned int *table, size_t size)
{
	int index;

	if (candela <= table[0])
		return 0;
	if (candela >= table[size - 1])
		return size - 1;

	for (index = 0; index < size - 1; index++)
		if ((table[index] <= candela) &&
				(candela < table[index + 1]))
			break;
	return index;
}


static inline int find_candela_from_table(unsigned int candela,
		unsigned int *table, size_t size)
{
	int i;

	if (candela <= table[0])
		return table[0];
	if (candela >= table[size - 1])
		return table[size - 1];

	for (i = 0; i < size - 1; i++)
		if ((table[i] <= candela) &&
				(candela < table[i + 1]))
			return table[i];

	return -1;
}
#endif	/* CONFIG_GEN_PANEL_OCTA */

static int gen_panel_get_temperature(int *temp)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name("battery");
	if (psy && psy->get_property) {
		ret = psy->get_property(psy, POWER_SUPPLY_PROP_TEMP, &val);
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
					__func__, desc->data[0], desc->data[1]);
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
					__func__, desc->data[0], desc->data[1]);
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

#define MAX_READ_BYTES	(100)

static size_t gen_panel_read_reg1(struct lcd *lcd,
                u8 *buf, u8 addr, u8 pos, u8 len)
{
        static char read_len[] = {0x00};
        static char read_addr[] = {0x00};
        static char gpara[] = {0xB0, 0x00};
        static struct gen_cmd_desc read_desc[] = {
                {MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE, DSI_HS_MODE, 0,
                        sizeof(read_len), read_len},
                {MIPI_DSI_DCS_SHORT_WRITE_PARAM, DSI_HS_MODE, 0,
                        sizeof(gpara), gpara},
                {MIPI_DSI_DCS_READ, DSI_HS_MODE, 0,
                        sizeof(read_addr), read_addr},
        };
	int ret, tlen = len;
        read_addr[0] = addr;
        gpara[1] = pos;

	while (tlen > 0) {
		read_len[0] = tlen < MAX_READ_BYTES ? len : MAX_READ_BYTES;
		ret = gen_panel_rx_cmds(lcd, buf, read_desc, ARRAY_SIZE(read_desc));
		if (ret != read_len[0]) {
			pr_err("%s, failed to read addr:%2Xh, pos:%d, len:%u\n",
					__func__, addr, pos, len);
			return -EINVAL;
		}
		gpara[1] += read_len[0];
		tlen -= read_len[0];
	}

        pr_info("[LCD] addr:%2Xh, pos:%d, len:%u\n", addr, pos, len);
	return len;
}

static size_t gen_panel_write_reg(struct lcd *lcd, u8 addr, u8 data)
{
	static char write_data[] = {0x00, 0x00};
	static struct gen_cmd_desc write_desc[] = {
		{MIPI_DSI_DCS_SHORT_WRITE_PARAM	, DSI_HS_MODE, 0,
			sizeof(write_data), write_data},
	};

	write_data[0] = addr;
	write_data[1] = data;

	pr_info("[LCD] %s: addr:0x%2x, data:0x%2x\n", __func__, addr, data);
	return gen_panel_tx_cmds(lcd, write_desc, ARRAY_SIZE(write_desc));
}

static u32 set_panel_id(struct lcd *lcd)
{
	u32 read_id;
	u8 out_buf[4];

	mutex_lock(&lcd->access_ok);
	memset(out_buf, 0, sizeof(out_buf));
	gen_panel_write_op_cmds(lcd, PANEL_NV_ENABLE_CMD);
	gen_panel_read_reg(lcd, out_buf, 0x04, 4);
	gen_panel_read_reg(lcd, out_buf, lcd->id_rd_info[0].reg,
			lcd->id_rd_info[0].len);
	read_id = out_buf[0] << 16;
	gen_panel_read_reg(lcd, out_buf, lcd->id_rd_info[1].reg,
			lcd->id_rd_info[1].len);
	read_id |= out_buf[0] << 8;
	gen_panel_read_reg(lcd, out_buf, lcd->id_rd_info[2].reg,
			lcd->id_rd_info[2].len);
	read_id |= out_buf[0];
	gen_panel_write_op_cmds(lcd, PANEL_NV_DISABLE_CMD);
	mutex_unlock(&lcd->access_ok);

	return read_id;
}

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

#ifdef CONFIG_GEN_PANEL_BACKLIGHT
#ifdef CONFIG_GEN_PANEL_OCTA
static int gen_panel_set_hbm(struct lcd *lcd)
{
	pr_info("%s, hbm %s\n", __func__, lcd->hbm ? "on" : "off");
	if (lcd->hbm)
		gen_panel_write_op_cmds(lcd, PANEL_HBM_ON);
	else
		gen_panel_write_op_cmds(lcd, PANEL_HBM_OFF);

	return 0;
}

static int gen_panel_set_elvss(struct lcd *lcd, int candela_index)
{
	struct gen_cmd_desc *desc;
	int itemp;

	/* Write TSET parameter */
	desc = find_op_cmd_desc(lcd, PANEL_TSET_CMD, 0xB8);
	if (desc) {
		u8 *tset_data = &desc->data[data_offset + lcd->tset_param_offset];
		*tset_data = abs(lcd->temperature) +
			((lcd->temperature < 0) ? 0x80 : 0x00);
		gen_panel_write_op_cmds(lcd, PANEL_TSET_CMD);
	}

	/* Write ELVSS Parameters */
	desc = find_op_cmd_desc(lcd, PANEL_ELVSS_CMD, 0xB6);
	if (desc) {
		if (mps_table) {
			u8 *mps_data = &desc->data[data_offset +
				lcd->mps_param_offset];
			*mps_data = mps_table[candela_index];
			pr_debug("%s, mps 0x%02X\n", __func__, *mps_data);
		}

		if (elvss_table) {
			u8 *elvss_data = &desc->data[data_offset +
				lcd->elvss_param_offset];
			*elvss_data = elvss_table[candela_index];
			pr_debug("%s, elvss 0x%02X\n", __func__, *elvss_data);
		}

		if (elvss_temp_table) {
			u8 *elvss_temp_data = &desc->data[data_offset +
				lcd->elvss_temp_param_offset];
			if (lcd->temperature > 0)
				itemp = 0;
			else if (lcd->temperature > -20)
				itemp = 1;
			else
				itemp = 2;

			*elvss_temp_data =
				elvss_temp_table[itemp * nr_candela + candela_index];
			pr_debug("%s, elvss_temp 0x%02X\n", __func__, *elvss_temp_data);
		}
		gen_panel_write_op_cmds(lcd, PANEL_ELVSS_CMD);
	}

	return 0;
}

static int gen_panel_set_aid(struct lcd *lcd, int candela_index)
{
	struct gen_cmd_desc *desc;
	u16 *aid_data;
	desc = find_op_cmd_desc(lcd, PANEL_AID_CMD, 0xB2);
	if (!desc) {
		pr_warn("%s, %s not found\n", __func__,
				op_cmd_names[PANEL_AID_CMD]);
		return -EINVAL;
	}
	if (aid_table) {
		aid_data = (u16 *)&desc->data[data_offset + lcd->aid_param_offset];
		*aid_data = htons((u16)aid_table[candela_index]);
		pr_debug("%s, aid 0x%04X\n", __func__, ntohs(*aid_data));
	}
	gen_panel_write_op_cmds(lcd, PANEL_AID_CMD);

	return 0;
}

static int gen_panel_set_acl(struct lcd *lcd, int candela_index)
{
	struct gen_cmd_desc *desc;
	u8 *acl_data;

	if (!lcd->acl || ((candela_index == (nr_candela - 1)) &&
		(lcd->brightness_mode == BRIGHTNESS_MODE_CLEAR_VIEW))) {
		gen_panel_write_op_cmds(lcd, PANEL_ACL_OFF_CMD);
		return 0;
	}

	desc = find_op_cmd_desc(lcd, PANEL_ACL_ON_CMD, 0x55);
	if (!desc) {
		pr_warn("%s, %s not found\n", __func__,
				op_cmd_names[PANEL_ACL_ON_CMD]);
		return -EINVAL;
	}
	if (acl_table) {
		acl_data = &desc->data[data_offset];
		*acl_data = acl_table[candela_index];
	}
	gen_panel_write_op_cmds(lcd, PANEL_ACL_ON_CMD);

	return 0;
}

static int gen_panel_set_gamma(struct lcd *lcd, int candela_index)
{
	struct device *dev = lcd->dev;
	struct gen_cmd_desc *desc;

	if (!gamma_table) {
		pr_err("%s, gamma_table not exist\n", __func__);
		return -EINVAL;
	}

	/* FIX ME : GAMMA REG should be variable by DT */
	desc = find_op_cmd_desc(lcd, PANEL_GAMMA_CMD, 0xCA);
	if (!desc) {
		dev_err(dev, "[LCD] not found gamma cmd\n");
		return 0;
	}
	memcpy(&desc->data[data_offset],
			&gamma_table[candela_index * 33], 33);
	gen_panel_write_op_cmds(lcd, PANEL_GAMMA_CMD);

	return 0;
}
#endif /* #ifdef CONFIG_GEN_PANEL_OCTA */

static int gen_panel_set_bl_level(struct lcd *lcd, int intensity)
{
	struct gen_cmd_desc *desc;

	pr_info("[LCD]set brightness (%d)\n", intensity);
	lcd->op_cmds[PANEL_BL_SET_BRT_CMD].desc->data[1] = intensity;

	gen_panel_write_op_cmds(lcd, PANEL_BL_ON_CMD);
	gen_panel_write_op_cmds(lcd, PANEL_BL_SET_BRT_CMD);

	return intensity;
}

static int gen_panel_set_brightness(void *bd_data, int intensity)
{
	struct lcd *lcd = (struct lcd *)bd_data;
	struct gen_panel_backlight_info *bl_info =
		(struct gen_panel_backlight_info *)bl_get_data(lcd->bd);
	struct device *dev = lcd->dev;
	int index;

	dev_dbg(dev, "set brightness (%d)\n", intensity);

	mutex_lock(&lcd->access_ok);
	if (!lcd->active) {
		pr_warn("[LCD] %s, lcd is inactive\n", __func__);
		mutex_unlock(&lcd->access_ok);
		return 0;
	}

#ifdef CONFIG_GEN_PANEL_OCTA
	/* TODO :
	 * 1. ELVSS
	 * 2. PARAM_ELVSS_HBM
	 * 3. ELVSS_HBM
	 * 4. AID
	 * 5. ACL
	 * 6. GAMMA
	 * */
	if (!intensity) {
		pr_debug("%s, amoled not support 0 level\n", __func__);
		mutex_unlock(&lcd->access_ok);
		return 0;
	}

	gen_panel_write_op_cmds(lcd, PANEL_NV_ENABLE_CMD);
	if (IS_HBM(bl_info->auto_brightness) &&
			op_cmd_valid(lcd, PANEL_HBM_ON)) {
		lcd->hbm = 1;
		gen_panel_set_hbm(lcd);
	} else {
		if (lcd->hbm) {
			lcd->hbm = 0;
			gen_panel_set_hbm(lcd);
		}

		index = find_candela_index_from_table(intensity,
				candela_table, nr_candela);
		gen_panel_set_gamma(lcd, index);
		gen_panel_set_aid(lcd, index);
		gen_panel_set_elvss(lcd, index);
		gen_panel_set_acl(lcd, index);
	}
	gen_panel_write_op_cmds(lcd, PANEL_GAMMA_UPDATE_CMD);
	prev_intensity = intensity;
	gen_panel_write_op_cmds(lcd, PANEL_NV_DISABLE_CMD);
#else
	/* set backlight-ic level directly */
	gen_panel_set_bl_level(lcd, intensity);
#endif
	mutex_unlock(&lcd->access_ok);

	return intensity;
}
#else
static int gen_panel_set_brightness(void *bd_data, int intensity)
{
	return 0;
}
#endif

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

	mutex_lock(&lcd->access_ok);
	if (!lcd->active) {
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
	} else if (IS_HBM(config->auto_brightness)) {
		pr_info("[LCD] %s: set outdoor\n", __func__);
		if (config->scenario == MDNIE_EBOOK_MODE ||
				config->scenario == MDNIE_BROWSER_MODE)
			index = PANEL_MDNIE_HBM_TEXT_CMD;
		else
			index = PANEL_MDNIE_HBM_CMD;
	} else {
		index = MDNIE_OP_INDEX(config->mode, config->scenario);
		pr_info("[LCD] %s: mode(%d), scenario(%d), index(%d)\n",
				__func__, config->mode, config->scenario, index);
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
	int adj_offset = data_offset + lcd->color_adj_offset;
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
		desc->data[adj_offset + (i * 2)] = GET_LSB_8BIT(mdnie->scr[j]);
		desc->data[adj_offset + (i * 2 + 1)] = GET_MSB_8BIT(mdnie->scr[j]);
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
		desc->data[1] = config->cabc;
		gen_panel_write_op_cmds(lcd, PANEL_CABC_ON_CMD);
	} else
		gen_panel_write_op_cmds(lcd, PANEL_CABC_OFF_CMD);
	pr_info("[LCD] %s: cabc %s\n", __func__, cabc_modes[config->cabc]);
	mutex_unlock(&lcd->access_ok);

	return 0;
}
#else
static inline int gen_panel_set_mdnie(struct mdnie_config *config)
{
	return 0;
}
static int gen_panel_set_color_adjustment(struct mdnie_config *config)
{
	return 0;
}
static int gen_panel_set_cabc(struct mdnie_config *config)
{
	return 0;
}
#endif

static const struct mdnie_ops mdnie_ops = {
	.set_mdnie = gen_panel_set_mdnie,
	.get_mdnie = NULL,
	.set_color_adj = gen_panel_set_color_adjustment,
	.set_cabc = gen_panel_set_cabc,
};

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
		if (pin->type == EXT_PIN_REGULATOR)	{
			if (!pin->supply) {
				dev_err(lcd->dev, "[LCD] invalid regulator(%s)\n",
						pin->name);
				return -EINVAL;
			}

			if (regulator_is_enabled(pin->supply))
				pr_info("[LCD] regulator(%s) already enabled\n",
						pin->name);
			rc = regulator_enable(pin->supply);
			if (unlikely(rc)) {
				dev_err(lcd->dev, "[LCD] regulator(%s) enable failed\n",
						pin->name);
				return -EINVAL;
			}
		} else {
			if (!gpio_is_valid(pin->gpio)) {
				dev_err(lcd->dev, "[LCD] invalid gpio(%s:%d)\n",
						pin->name, pin->gpio);
				return -EINVAL;
			}
			gpio_direction_output(pin->gpio, 1);
		}
	} else {
		if (pin->type == EXT_PIN_REGULATOR) {
			if (!pin->supply) {
				dev_err(lcd->dev, "[LCD] invalid regulator(%s)\n",
						pin->name);
				return -EINVAL;
			}

			if (!regulator_is_enabled(pin->supply))
				pr_info("[LCD] regulator(%s) already disabled\n",
						pin->name);
			rc = regulator_disable(pin->supply);
			if (unlikely(rc)) {
				dev_err(lcd->dev, "[LCD] regulator(%s) disable failed\n",
						pin->name);
				return -EINVAL;
			}
		} else {
			if (!gpio_is_valid(pin->gpio)) {
				dev_err(lcd->dev, "[LCD] invalid gpio(%s:%d)\n",
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
			dev_err(lcd->dev, "[LCD] invalid regulator(%s)\n",
					pin->name);
			return -EINVAL;
		}

		return regulator_is_enabled(pin->supply);
	} else {
		if (!gpio_is_valid(pin->gpio)) {
			dev_err(lcd->dev, "[LCD] invalid gpio(%s:%d)\n",
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

	pr_debug("%s, pwr %d, on %d\n", __func__, lcd->power, on);

	if ((on && (lcd->power & on)) || (!on && !lcd->power)) {
		pr_warn("%s: power already %s(%d) state\n",
				__func__, on ? "on" : "off", on);
		return;
	}

	if (on) {
		if (on == GEN_PANEL_PWR_ON_0) {
			pin_ctrl = lcd->extpin_on_0_seq.ctrls;
			nr_pin_ctrl = lcd->extpin_on_0_seq.nr_ctrls;
		} else if (on == GEN_PANEL_PWR_ON_1) {
			pin_ctrl = lcd->extpin_on_seq.ctrls;
			nr_pin_ctrl = lcd->extpin_on_seq.nr_ctrls;
		}
	} else {
		pin_ctrl = lcd->extpin_off_seq.ctrls;
		nr_pin_ctrl = lcd->extpin_off_seq.nr_ctrls;
	}

	if (!pin_ctrl || !nr_pin_ctrl) {
		pr_err("%s, pin_ctrl not exist\n", __func__);
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
					pr_warn("[LCD] %s, wait (%d msec) too long\n",
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
				pr_err("[LCD] %s, get extpin state failed (%d)\n",
						__func__, state);
			else
				pr_info("[LCD] %s keep %s for %d usec\n", pin->name,
						state ? "on" : "off",
						pin_ctrl[i].usec);
		} else {
			gen_panel_set_extpin(lcd, pin_ctrl[i].pin,
					pin_ctrl[i].on);
			panel_usleep(pin_ctrl[i].usec);
			pr_info("[LCD] %s %s %d usec\n", pin->name,
					pin_ctrl[i].on ? "on" : "off",
					pin_ctrl[i].usec);
		}
	}

	if (on)
		lcd->power |= on;
	else
		lcd->power = on;
	return;
}
#else
static void gen_panel_set_power(struct lcd *lcd, int on) {}
#endif

static void gen_panel_set_id(struct lcd *lcd)
{
	lcd->id = (boot_panel_id) ? boot_panel_id :
		lcd->set_panel_id(lcd);
}

static inline unsigned int gen_panel_get_panel_id(struct lcd *lcd)
{
	return lcd->id;
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

static inline void print_data(char *data, int len)
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

#ifdef CONFIG_GEN_PANEL_DYNAMIC_AID
static int gen_panel_read_hbm_info(struct lcd *lcd, u8 *dest)
{
	u8 *t = dest;
	int i = 0, len = 0;

	gen_panel_write_op_cmds(lcd, PANEL_NV_ENABLE_CMD);
	for (i = 0; i < ARRAY_SIZE(lcd->hbm_rd_info) &&
			lcd->hbm_rd_info[i].len; i++) {
		len = gen_panel_read_reg1(lcd, dest,
				lcd->hbm_rd_info[i].reg,
				lcd->hbm_rd_info[i].idx,
				lcd->hbm_rd_info[i].len);
		if (len != lcd->hbm_rd_info[i].len) {
			pr_warn("%s, hbm read failed\n", __func__);
			return -EINVAL;
		}
		dest += len;
	}
	print_data(t, (int)(dest - t));
	gen_panel_write_op_cmds(lcd, PANEL_NV_DISABLE_CMD);

	return (int)(dest - t);
}

size_t gen_panel_read_ldi_info(struct lcd *lcd)
{
	struct gen_cmd_desc *desc;
	int len, i;
	u8 *hbm_gamma;
	u8 octa_product_reg = lcd->octa_product_reg;
	u8 octa_coordinate_reg = lcd->octa_coordinate_reg;
	int product_reg_gpara = lcd->octa_product_gpara;
	int coordinate_gpara = lcd->octa_coordinate_gpara;
	if (read_done_from_boot) {
		/* set HBM cmd */
		if (op_cmd_valid(lcd, PANEL_HBM_ON)) {
			desc = find_op_cmd_desc(lcd, PANEL_HBM_ON, 0xB4);
			if (desc) {
				hbm_gamma = &desc->data[data_offset +
					lcd->hbm_gamma_param_offset];
				memcpy(hbm_gamma, boot_panel_hbm1, hbm1_len);
				memcpy(hbm_gamma + hbm1_len, boot_panel_hbm2, hbm2_len);
			}
		}

		/* set ELVSS cmd */
		if (elvss_temp_table)
			for (i = 0; i < nr_candela * 3; i++)
				if (!elvss_temp_table[i])
					elvss_temp_table[i] = boot_panel_elvss[0];

		/* set OCTA cell ID */
		memcpy(date, boot_panel_date, date_len);
		memcpy(coordinate, boot_panel_cordi, cordi_len);
	} else {
		if (op_cmd_valid(lcd, PANEL_HBM_ON)) {
			desc = find_op_cmd_desc(lcd, PANEL_HBM_ON, 0xB4);
			if (desc) {
				hbm_gamma = &desc->data[data_offset +
					lcd->hbm_gamma_param_offset];
				len = gen_panel_read_hbm_info(lcd, hbm_gamma);
				if (len < 0)
					pr_err("%s, hbm read failed\n", __func__);
			}
		}
		gen_panel_write_op_cmds(lcd, PANEL_NV_ENABLE_CMD);
		panel_usleep(20000);

		/* read OCTA cell ID */
		gen_panel_read_reg1(lcd, date, octa_product_reg, product_reg_gpara, 7);
		gen_panel_read_reg1(lcd, coordinate, octa_coordinate_reg, coordinate_gpara, 4);
		gen_panel_write_op_cmds(lcd, PANEL_NV_DISABLE_CMD);

		if (elvss_temp_table) {
			u8 temp_data;
			gen_panel_read_reg1(lcd, &temp_data, 0xB6,
					lcd->elvss_temp_param_offset, 1);
			for (i = 0; i < nr_candela * 3; i++)
				if (!elvss_temp_table[i])
					elvss_temp_table[i] = temp_data;
		}
	}
	return 0;
}

size_t gen_panel_read_mtp(struct lcd *lcd, char *mtp_data)
{
	unsigned char default_mtp_buffer[] = {
		0x00, 0x3E, 0x00, 0x41, 0x00, 0x7A,
		0x5E, 0x5E, 0x5C, 0x59, 0x59, 0x58,
		0x49, 0x49, 0x48, 0x54, 0x55, 0x52,
		0x61, 0x64, 0x67, 0x64, 0x66, 0x65,
		0x55, 0x60, 0x54, 0x50, 0x63, 0x57,
		0x02, 0x03, 0x02
	};
	int len, ret;
	u8 rd_reg = lcd->mtp_rd_info[0].reg;
	u8 rd_len = lcd->mtp_rd_info[0].len;
	if (read_done_from_boot) {
		memcpy(mtp_data, boot_panel_mtp, rd_len);
	} else {
		u8 *out_buf = kzalloc(sizeof(u8) * rd_len, GFP_KERNEL);
		if (!out_buf) {
			pr_err("[LCD] %s: memory allocation failed\n", __func__);
			return -ENOMEM;
		}

		ret = gen_panel_write_op_cmds(lcd, PANEL_NV_ENABLE_CMD);
		if (!ret) {
			pr_err("[LCD] %s: gen_panel_write_op_cmds failed\n", __func__);
			kfree(out_buf);
			return -EINVAL;
		}
		panel_usleep(20000);
		len = gen_panel_read_reg(lcd, out_buf, rd_reg, rd_len);
		if (len == rd_len) {
			memcpy(mtp_data, out_buf, rd_len);
		} else
			memcpy(mtp_data, default_mtp_buffer,
					ARRAY_SIZE(default_mtp_buffer));
		gen_panel_write_op_cmds(lcd, PANEL_NV_DISABLE_CMD);

		kfree(out_buf);
	}
	return 0;
}

#ifdef CONFIG_GEN_PANEL_MDNIE
#define COLOR_OFFSET_FUNC(x, y, num, den, con)	\
	((y) + (((x) * (num)) / (den)) + (con))
#define RECOGNIZE_REGION(sign, comp1, comp2)	\
	(((comp1) > 0) ? (((sign) > 0) ? 0 : 2) :	\
	  (((comp2) < 0) ? (((sign) > 0) ? 2 : 0) : 1))
static int gen_panel_coordinate_to_tune_area(struct lcd *lcd)
{
	struct mdnie_cal *cal = &lcd->mdnie.cal;
	long int x, y, result[4];
	int i, ret;

	x = (coordinate[0] << 8) | coordinate[1];
	y = (coordinate[2] << 8) | coordinate[3];
	for (i = 0; i < 4; i++)
		result[i] = COLOR_OFFSET_FUNC(x, y, cal->line[i].num,
				cal->line[i].den, cal->line[i].con);

	ret = RECOGNIZE_REGION(1, result[0], result[1]) * 3 +
		RECOGNIZE_REGION(-1, result[2], result[3]);
	pr_info("%s, x %ld, y %ld, result %ld %ld %ld %ld, area %d\n",
			__func__, x, y, result[0], result[1],
			result[2], result[3], ret);
	return ret;
}

static int gen_panel_mdnie_calibration(struct lcd *lcd)
{
	struct mdnie_cal *cal = &lcd->mdnie.cal;
	struct gen_cmd_desc *desc;
	int area, rgb, i, scenario, index;

	area = gen_panel_coordinate_to_tune_area(lcd);
	if (unlikely(!cal->modes || !cal->nr_mode ||
				!cal->offset || !cal->reg)) {
		pr_err("%s, error : modes %p, nr_mode %d"
				"offset %p, reg 0x%02Xh\n",
				__func__, cal->modes, cal->nr_mode,
				cal->offset, cal->reg);
		return -EINVAL;
	}

	for (i = 0; i < cal->nr_mode; i++) {
		for (scenario = 0; scenario < MDNIE_MODE_SIZE(cal->modes[i]);
				scenario++) {
			if (scenario == PANEL_MDNIE_SCENARIO_EBOOK ||
					scenario == PANEL_MDNIE_SCENARIO_EMAIL)
				continue;
			index = MDNIE_MODE_OP_INDEX(cal->modes[i]) + scenario;
			desc = find_op_cmd_desc(lcd, index, cal->reg);
			if (unlikely(!desc)) {
				pr_warn("%s, reg:0x%08X not found in %s\n",
						__func__, cal->reg,
						(op_cmd_index_valid(index) ?
						op_cmd_names[index] : "unknown"));
				continue;
			}
			for (rgb = OFFSET_R; rgb < RGB_OFFSET_MAX; rgb++) {
				u8 *data = OP_CMD_DESC_NDATA(desc, cal->offset[rgb], 1);
				if (data) {
					u8 old = *data;
					OP_CMD_DESC_NDATA_COPY(desc, cal->offset[rgb],
							&cal->tune[area][rgb], 1);
					pr_info("%s, offset 0x%02X, old 0x%02X, new 0x%02X\n",
							op_cmd_names[index],
							cal->offset[rgb], old, *data);
				}
			}
		}
	}
	return 0;
}
#endif	/* CONFIG_GEN_PANEL_MDNIE */

static int gen_panel_dimming_init(struct lcd *lcd)
{
	int i, ret;

	GET_CONF_FUNC get_conf =
		smartdim_get_conf_func("sdim_oled");

	if (!get_conf) {
		pr_err("[LCD] %s, smartdim not found\n", __func__);
		return -EINVAL;
	}

	if (sdimconf) {
		pr_debug("%s, already initialized\n", __func__);
		return 0;
	}

	sdimconf = get_conf();
	sdimconf->lux_tab = candela_table;
	sdimconf->lux_tabsize = nr_candela;
	sdimconf->rgb_offset = rgb_offset;
	sdimconf->candela_offset = candela_offset;
	sdimconf->base_lux_table = base_lux_table;
	sdimconf->gamma_curve_table = gamma_curve_table;
	sdimconf->vregout_voltage = vregout_voltage;

	ret = gen_panel_read_mtp(lcd, sdimconf->mtp_buffer);
	if (ret < 0)
		return -EINVAL;
	sdimconf->man_id = lcd->id;
	sdimconf->init();

	gamma_table = kzalloc(sizeof(u8) * nr_candela * 33, GFP_KERNEL);
	for (i = 0; i < nr_candela; i++) {
		sdimconf->generate_gamma(candela_table[i],
				&gamma_table[i * 33]);
	}
	gen_panel_read_ldi_info(lcd);
#ifdef CONFIG_GEN_PANEL_MDNIE
	gen_panel_mdnie_calibration(lcd);
#endif

	return 0;
}
#else
static inline int gen_panel_dimming_init(struct lcd *lcd) { return 0; }
#endif	/* CONFIG_GEN_PANEL_DYNAMIC_AID */

void gen_panel_set_external_pin(struct lcd *lcd, int status)
{
	gen_panel_set_pin_state(lcd, status);
	gen_panel_set_power(lcd, status);
}

void gen_panel_set_external_pin_1(struct lcd *lcd, int status)
{
	gen_panel_set_power(lcd, GEN_PANEL_PWR_ON_1);
}

void gen_panel_set_status(struct lcd *lcd, int status)
{
	int skip_on = (status == GEN_PANEL_ON_REDUCED);

	pr_info("[LCD] called %s with status %d\n", __func__, status);

	if (gen_panel_is_on(status)) {
		/* power on */
		gen_panel_set_id(lcd);
		gen_panel_dimming_init(lcd);
		if (!gen_panel_connected(lcd)) {
			lcd->active = true;
			pr_warn("[LCD] %s: no panel\n", __func__);
			if (gen_panel_backlight_is_on(lcd->bd))
				gen_panel_backlight_onoff(lcd->bd, 0);
			gen_panel_set_external_pin(lcd, 0);
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
		gen_panel_write_op_cmds(lcd, PANEL_DISABLE_CMD);
		mutex_unlock(&lcd->access_ok);
	} else
		pr_warn("[LCD] set status %d not supported\n", status);
}

void gen_panel_start(struct lcd *lcd, int status)
{
	struct gen_panel_backlight_info *bl_info =
		(struct gen_panel_backlight_info *)bl_get_data(lcd->bd);

	pr_debug("[LCD] %s\n", __func__);
	if (!lcd->active) {
		pr_warn("[LCD] %s: no panel\n", __func__);
		return;
	}

	if (gen_panel_is_reduced_on(status)) {
		if (lcd->mdnie.config.cabc)
			update_cabc_mode(&lcd->mdnie);
		update_mdnie_mode(&lcd->mdnie);
		if (!gen_panel_backlight_is_on(lcd->bd))
			gen_panel_backlight_onoff(lcd->bd, 1);
	} else if (gen_panel_is_on(status)) {
		gen_panel_write_op_cmds(lcd,
				PANEL_POST_ENABLE_CMD);
#ifdef CONFIG_GEN_PANEL_OCTA
		gen_panel_set_brightness(bl_info->bd_data,
				prev_intensity);
#endif
		if (lcd->mdnie.config.cabc)
			update_cabc_mode(&lcd->mdnie);
		update_mdnie_mode(&lcd->mdnie);
		/* To wait 2 frames for stability need to be applied
		   seperately in LDI. so it need to be moved on dts
		   If you need it, please add delay on dts mdnie cmd
		 */
		/* after video on, wait more than 2 frames */
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
		gen_panel_write_op_cmds(lcd, PANEL_DISABLE_CMD);
		mutex_unlock(&lcd->access_ok);
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
			pr_info("[LCD] found (%s) by name\n", panel_node->name);
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
				if (boot_panel_id == panel_ids[i]) {
					panel_node = np;
					panel_id_index = i;
					pr_info("[LCD] found (%s) by id(0x%X)\n",
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

#ifdef CONFIG_GEN_PANEL_OCTA
static int gen_panel_parse_dt_offset_table(struct device_node *np,
		const char *propname, int **table)
{
	int i, sz, sz_table;
	unsigned int value;

	if (unlikely(!table || !propname)) {
		pr_err("[LCD] %s: invalid input\n", __func__);
		return -EINVAL;
	}

	if (!of_find_property(np, propname, &sz)) {
		pr_err("[LCD] %s: %s not exist\n", __func__, propname);
		return -EINVAL;
	}

	if (sz % (nr_candela)) {
		pr_err("[LCD] %s: %s wrong table size(%d)\n",
				__func__, propname, sz);
		return -EINVAL;
	}

	*table = kzalloc(sz, GFP_KERNEL);
	if (!(*table)) {
		pr_err("[LCD] %s: %s memory allocation failed\n",
				__func__, propname);
		return -ENOMEM;
	}
	sz_table = sz / sizeof(u32);

	for (i = 0; i < sz_table; i++) {
		if (of_property_read_u32_index(np, propname, i, &value))
			return -EINVAL;
		(*table)[i] = (int)value;
	}

	return 0;
}

static int gen_panel_parse_dt_candela_map_table(struct device_node *np,
		const char *propname, struct candela_map *table)
{
	const __be32 *data;
	int  data_offset, len = 0 , i = 0;

	data = of_get_property(np, propname, &len);
	if (!data) {
		pr_err("[LCD] %s:%d, Unable to read table %s ",
				__func__, __LINE__, propname);
		return -EINVAL;
	}
	if ((len % 2) != 0) {
		pr_err("[LCD] %s:%d, Incorrect table entries for %s",
				__func__, __LINE__, propname);
		return -EINVAL;
	}
	table->size = len / (sizeof(int)*2);
	table->candela = kzalloc((sizeof(int) * table->size), GFP_KERNEL);
	if (!table->candela)
		return -ENOMEM;

	table->value = kzalloc((sizeof(int) * table->size), GFP_KERNEL);
	if (!table->value)
		goto error;

	data_offset = 0;
	for (i = 0; i < table->size; i++) {
		table->candela[i] = be32_to_cpup(&data[data_offset++]);
		table->value[i] = be32_to_cpup(&data[data_offset++]);
	}

	return 0;
error:
	kfree(table->candela);

	return -ENOMEM;
}
#endif /* CONFIG_GEN_PANEL_OCTA */

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

	data = kzalloc(sizeof(char) * sz , GFP_KERNEL);
	if (unlikely(!data))
		return -ENOMEM;

	memcpy(data, val, sz);

	/* scan dcs commands */
	while (sz > i + sizeof(*gchdr)) {
		gchdr = (struct gen_cmd_hdr *)(data + i);
		gchdr->dlen = ntohs(gchdr->dlen);
		gchdr->wait = ntohs(gchdr->wait);
		if (i + gchdr->dlen > sz) {
			pr_err("[LCD] %s: parse error dtsi name:%s type:0x%02x, txmode:0x%02x, len:0x%02x, nr_desc:%d\n",
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
		dev_info(&pdev->dev, "[LCD] %s: found pinctrl enable\n", __func__);
		lcd->pin_enable = pinctrl_state;
	}

	pinctrl_state = pinctrl_lookup_state(lcd->pinctrl, "disable");
	if (!IS_ERR(pinctrl_state)) {
		dev_info(&pdev->dev, "[LCD] %s: found pinctrl disable\n", __func__);
		lcd->pin_disable = pinctrl_state;
	}

	if ((!lcd->pin_enable && lcd->pin_disable) ||
			(lcd->pin_enable && !lcd->pin_disable)) {
		dev_warn(&pdev->dev, "[LCD] %s: warning - pinctrl %s not exist\n",
				__func__,
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
			pr_info("[LCD] %s: set temperature threshold (%d)\n",
					__func__, temp_comp[i].temperature);
		} else {
			pr_warn("[LCD] %s: temperature might be not initialized\n",
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
				sizeof(op_cmd_names[op_index])))
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
	struct manipulate_action *action;
	int config, state, size, ret;
	const __be32 *list;

	for_each_child_of_node(parent, sysfs_node) {
		gen_attr = kmalloc(sizeof(struct gen_dev_attr), GFP_KERNEL);
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
			pr_err("[LCD] %s: failed to allocate memory\n", __func__);
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
					pr_warn("[LCD] %s, failed to parse %s\n",
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

static int gen_panel_parse_dt_mdnie_cal(
		struct device_node *np, struct lcd *lcd)
{
	struct mdnie_cal *cal = &lcd->mdnie.cal;
	int i, sz;

	of_property_read_u8(np, "cal-reg", &cal->reg);
	of_property_read_u8_array(np, "cal-offset",
			cal->offset, RGB_OFFSET_MAX);

	if (!of_find_property(np, "cal-modes", &sz)) {
		pr_warn("%s, cal-modes not exist\n", __func__);
		return -EINVAL;
	}

	cal->nr_mode = (sz / sizeof(u32));
	if (unlikely((sz % sizeof(u32)) || (!cal->nr_mode))) {
		pr_err("%s: wrong cal-modes size(%d)\n",
				__func__, sz);
		return -EINVAL;
	}

	cal->modes = (u32 *)kzalloc(sz, GFP_KERNEL);
	if (!cal->modes) {
		pr_err("%s: memory allocation failed (size:%d)\n",
				__func__, sz);
		return -ENOMEM;
	}
	of_property_read_u32_array(np, "cal-modes", cal->modes, cal->nr_mode);
	of_property_read_u32_array(np, "cal-linear-equation", (u32 *)cal->line,
			sizeof(cal->line) / sizeof(int));
	of_property_read_u8_array(np, "cal-tune-value",
			(u8 *)cal->tune, sizeof(cal->tune) / sizeof(u8));
	return 0;
}

static int gen_panel_parse_dt_panel(
		struct device_node *np, struct lcd *lcd)
{
	const char *panel_type;
	struct device_node *temp_comp_node;
	struct device_node *mdnie_cal_node;
	u32 tmp, panel_ids[8];
	int ret, i, sz;

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
	ret = of_property_read_string(np, "gen-panel-type",
			&panel_type);
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
	ret = of_property_read_u32(np, "gen-panel-h-front-porch",
			&lcd->mode.right_margin);
	ret = of_property_read_u32(np, "gen-panel-h-back-porch",
			&lcd->mode.left_margin);
	ret = of_property_read_u32(np, "gen-panel-h-sync-width",
			&lcd->mode.hsync_len);
	ret = of_property_read_u32(np, "gen-panel-v-front-porch",
			&lcd->mode.lower_margin);
	ret = of_property_read_u32(np, "gen-panel-v-back-porch",
			&lcd->mode.upper_margin);
	ret = of_property_read_u32(np, "gen-panel-v-sync-width",
			&lcd->mode.vsync_len);

	ret = of_property_read_u32(np, "gen-panel-hsync-invert", &tmp);
	ret = of_property_read_u32(np, "gen-panel-vsync-invert", &tmp);
	ret = of_property_read_u32(np, "gen-panel-lanes", &tmp);
	ret = of_property_read_u32(np, "gen-panel-burst-mode", &tmp);
	if (of_property_read_bool(np, "gen-panel-master-mode"))
		pr_debug("[LCD] %s, master mode\n", __func__);
	else
		pr_debug("[LCD] %s, slave mode\n", __func__);
	if (of_property_read_bool(np, "gen-panel-hbp-en"))
		pr_debug("[LCD] %s, hbp_en\n", __func__);
	if (of_property_read_bool(np, "gen-panel-hfp-en"))
		pr_debug("[LCD] %s, hfp_en\n", __func__);

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
		pr_err("[LCD] %s, unknown packet data type(addr:%d, data:%d)\n",
				__func__, addr_type, data_type);
		return -EINVAL;
	}

	lcd->op_cmds = kzalloc(PANEL_OP_CMD_MAX *
			sizeof(struct gen_cmds_info), GFP_KERNEL);

	/* parse command tables in dt */
	for (i = 0; i < PANEL_OP_CMD_MAX; i++)
		gen_panel_parse_dt_dcs_cmds(np, op_cmd_names[i],
				&lcd->op_cmds[i]);

#ifdef CONFIG_GEN_PANEL_OCTA
	of_property_read_u8(np, "gen-panel-octa-product-reg",
			&lcd->octa_product_reg);
	of_property_read_u32(np, "gen-panel-octa-product-gpara",
			&lcd->octa_product_gpara);
	of_property_read_u8(np, "gen-panel-octa-coordinate-reg",
			&lcd->octa_coordinate_reg);
	of_property_read_u32(np, "gen-panel-octa-coordinate-gpara",
			&lcd->octa_coordinate_gpara);
	of_property_read_u32(np, "gen-panel-octa-vregout",
			&vregout_voltage);
	of_property_read_u32(np, "gen-panel-octa-tset-param-offset",
			&lcd->tset_param_offset);
	of_property_read_u32(np, "gen-panel-octa-mps-param-offset",
			&lcd->mps_param_offset);
	of_property_read_u32(np, "gen-panel-octa-elvss-param-offset",
			&lcd->elvss_param_offset);
	of_property_read_u32(np, "gen-panel-octa-elvss-temp-param-offset",
			&lcd->elvss_temp_param_offset);
	of_property_read_u32(np, "gen-panel-octa-aid-param-offset",
			&lcd->aid_param_offset);
	of_property_read_u32(np, "gen-panel-octa-hbm-gamma-param-offset",
			&lcd->hbm_gamma_param_offset);
	if (of_find_property(np, "gen-panel-candela-table", &sz)) {
		nr_candela = sz / sizeof(u32);
		candela_table =
			kzalloc(sizeof(unsigned int) * nr_candela, GFP_KERNEL);
		of_property_read_u32_array(np, "gen-panel-candela-table",
				candela_table, nr_candela);
	}
	gen_panel_parse_dt_offset_table(np,
			"gen-panel-aid-table", &aid_table);
	gen_panel_parse_dt_offset_table(np,
			"gen-panel-acl-table", &acl_table);
	gen_panel_parse_dt_offset_table(np,
			"gen-panel-mps-table", &mps_table);
	gen_panel_parse_dt_offset_table(np,
			"gen-panel-elvss-table", &elvss_table);
	gen_panel_parse_dt_offset_table(np,
			"gen-panel-elvss-temperature-table",
			&elvss_temp_table);
	gen_panel_parse_dt_offset_table(np,
			"gen-panel-candela-compensation-table",
			&candela_offset);
	gen_panel_parse_dt_offset_table(np,
			"gen-panel-rgb-compensation-table",
			&rgb_offset);
	gen_panel_parse_dt_offset_table(np,
			"gen-panel-base-lux-table",
			&base_lux_table);
	gen_panel_parse_dt_offset_table(np,
			"gen-panel-gamma-curve-table",
			&gamma_curve_table);

	if (of_property_read_bool(np, "gen-panel-acl-always-on"))
		lcd->acl = 1;
	else
		lcd->acl = 0;

#ifdef CONFIG_GEN_PANEL_MDNIE
	mdnie_cal_node = of_find_node_by_name(np, "gen-panel-mdnie-cal");
	if (mdnie_cal_node)
		gen_panel_parse_dt_mdnie_cal(mdnie_cal_node, lcd);
	of_node_put(mdnie_cal_node);
#endif
#endif
	ret = of_property_read_u32(np,
			"gen-panel-backlight-set-brightness-reg",
			&lcd->set_brt_reg);
	of_property_read_u8(np,
			"gen-panel-mdnie-color-adjustment-mode-reg",
			&lcd->color_adj_reg);
	of_property_read_u32(np,
			"gen-panel-mdnie-color-adjustment-mode-offset",
			&lcd->color_adj_offset);
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
			pr_err("[LCD] %s: failed to parse pin-supply\n", __func__);
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
		pr_info("[LCD] %s, get regulator(%s)\n", pin->name, node->name);
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
		pr_err("[LCD] %s: out of range(len:%d, buf_size:%u)\n",
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
			pr_err("[LCD] %s: failed to allocate memory\n", __func__);
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
			"panel-ext-pin-on-0", &lcd->extpin_on_0_seq);
	gen_panel_parse_dt_extpin_ctrl_seq(pin_node,
			"panel-ext-pin-on", &lcd->extpin_on_seq);
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

static ssize_t manufacture_code_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lcd *lcd = dev_get_drvdata(dev);
	unsigned int len = 0, i;

	for (i = 0; i < ARRAY_SIZE(lcd->manufacturer_code); i++)
		len += sprintf(buf + len, "%02X", lcd->manufacturer_code[i]);

	return len;
}
#ifdef CONFIG_GEN_PANEL_OCTA
static ssize_t power_reduce_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lcd *lcd = dev_get_drvdata(dev);
	pr_info("[LCD] %s: acl=%d, brightness_mode=%d\n",
		__func__, lcd->acl, lcd->brightness_mode);
	return sprintf(buf, "%d\n", lcd->acl);
}

static ssize_t power_reduce_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int value;
	struct lcd *lcd = dev_get_drvdata(dev);

	if (kstrtoul(buf, 0, (unsigned long *)&value))
		return -EINVAL;

	if (lcd->acl != !!value) {
		lcd->acl = !!value;
		backlight_update_status(lcd->bd);
	}
	return size;
}

static ssize_t read_mtp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return strlen(buf);
}

static ssize_t read_mtp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd *lcd = dev_get_drvdata(dev);
	unsigned int rd_reg, rd_pos, rd_len, i;
	unsigned char readbuf[256] = {0xff, };
	struct gen_cmds_info *cmd;
	struct gen_cmd_desc *desc;
	int len, ret;

	sscanf(buf, "%x %d %d", &rd_reg, &rd_pos, &rd_len);

	ret = gen_panel_write_op_cmds(lcd, PANEL_NV_ENABLE_CMD);
	if (!ret) {
		pr_err("[LCD] %s: gen_panel_write_op_cmds failed\n", __func__);
		return -EINVAL;
	}
	mdelay(20);
	len = gen_panel_read_reg1(lcd, readbuf, rd_reg, rd_pos, rd_len);

	if (len != rd_len)
		pr_err("[LCD] %s: gen_panel_read_reg failed\n", __func__);

	ret = gen_panel_write_op_cmds(lcd, PANEL_NV_DISABLE_CMD);
	if (!ret) {
		pr_err("[LCD] %s: gen_panel_write_op_cmds failed\n", __func__);
		return -EINVAL;
	}

	pr_info("[LCD] READ_Reg addr : %02x, start pos : %d len : %d\n",
		rd_reg, rd_pos, rd_len);

	for (i = 0; i < rd_len; i++)
		pr_info("[LCD] READ_Reg %dth : %02x\n", i + 1, readbuf[i]);

	return size;
}

static ssize_t write_mtp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return strlen(buf);
}

static ssize_t write_mtp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd *lcd = dev_get_drvdata(dev);
	unsigned int write_reg, write_pos, write_value;
	int ret;

	sscanf(buf, "%x %d %x", &write_reg, &write_pos, &write_value);

	ret = gen_panel_write_op_cmds(lcd, PANEL_NV_ENABLE_CMD);
	if (!ret) {
		pr_err("[LCD] %s: gen_panel_write_op_cmds failed\n", __func__);
		return -EINVAL;
	}

	gen_panel_write_reg(lcd, 0xB0, write_pos-1);
	gen_panel_write_reg(lcd, (u8)write_reg, (u8)write_value);


	ret = gen_panel_write_op_cmds(lcd, PANEL_NV_DISABLE_CMD);
	if (!ret) {
		pr_err("[LCD] %s: gen_panel_write_op_cmds failed\n", __func__);
		return -EINVAL;
	}

	pr_info("[LCD] QRITE_Reg addr : %02x, start pos : %d write_value : %d\n",
		write_reg, write_pos, write_value);

	return size;
}

static ssize_t temperature_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[] = "-20, -19, 0, 1\n";

	strcat(buf, temp);
	return strlen(buf);
}

static ssize_t temperature_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoint(buf, 10, &value);

	if (rc < 0)
		return rc;
	lcd->temperature = value;
	pr_info("%s, temperature %d\n", __func__, lcd->temperature);
	backlight_update_status(lcd->bd);

	return size;
}

static ssize_t cell_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	sprintf(buf, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
			date[0], date[1], date[2], date[3],
			date[4], date[5], date[6],
			coordinate[0], coordinate[1],
			coordinate[2], coordinate[3]);
	return strlen(buf);
}

static ssize_t weakness_hbm_comp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lcd *lcd = glcd;
	pr_info("[LCD] %s: acl=%d, brightness_mode=%d\n",
		__func__, lcd->acl, lcd->brightness_mode);
	return sprintf(buf, "%d\n", lcd->brightness_mode);
}

static ssize_t weakness_hbm_comp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int value;
	struct lcd *lcd = glcd;

	if (kstrtoul(buf, 0, (unsigned long *)&value))
		return -EINVAL;

	if (lcd->brightness_mode != value) {
		lcd->brightness_mode = value;
		backlight_update_status(lcd->bd);
	}
	return size;
}

static DEVICE_ATTR(weakness_hbm_comp, S_IRUGO | S_IWUSR | S_IWGRP,
		weakness_hbm_comp_show, weakness_hbm_comp_store);
static DEVICE_ATTR(power_reduce, S_IRUGO | S_IWUSR | S_IWGRP,
		power_reduce_show, power_reduce_store);
static DEVICE_ATTR(read_mtp, S_IRUGO | S_IWUSR | S_IWGRP,
		read_mtp_show, read_mtp_store);
static DEVICE_ATTR(write_mtp, S_IRUGO | S_IWUSR | S_IWGRP,
		write_mtp_show, write_mtp_store);
static DEVICE_ATTR(temperature, S_IRUGO | S_IWUSR | S_IWGRP,
		temperature_show, temperature_store);
static DEVICE_ATTR(cell_id, S_IRUGO, cell_id_show, NULL);
#endif
static DEVICE_ATTR(panel_name, S_IRUGO | S_IXOTH, panel_name_show, NULL);
static DEVICE_ATTR(lcd_type, S_IRUGO | S_IXOTH, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, S_IRUGO | S_IXOTH, window_type_show, NULL);
static DEVICE_ATTR(manufacture_code, S_IRUGO | S_IXOTH, manufacture_code_show, NULL);

static ssize_t gen_panel_common_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct gen_dev_attr *gen_attr =
		container_of(attr, struct gen_dev_attr, dev_attr);
	const char *action_name = "none";
	unsigned int len;

	struct manipulate_action *action, *naction;

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
				dev_err(&pdev->dev, "failed to parse sysfs node\n");
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

	lcd->id = boot_panel_id;
	lcd->set_panel_id = &set_panel_id;
	lcd->power = 1;
	lcd->temperature = 25;

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

	ret = device_create_file(lcd->dev, &dev_attr_manufacture_code);
	if (unlikely(ret < 0)) {
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_manufacture_code.attr.name);
		goto err_window_type;

	}

	list_for_each_entry(gen_attr, &attr_list, list) {
		ret = device_create_file(lcd->dev, &gen_attr->dev_attr);
		if (unlikely(ret < 0)) {
			pr_err("[LCD] Failed to create device file(%s)!\n",
					gen_attr->dev_attr.attr.name);
			goto err_manufacture_code;
		}
	}

#ifdef CONFIG_GEN_PANEL_OCTA
	ret = device_create_file(&lcd->bd->dev, &dev_attr_weakness_hbm_comp);
	if (unlikely(ret < 0)) {
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_weakness_hbm_comp.attr.name);
		goto err_weakness_hbm_comp;
	}

	ret = device_create_file(lcd->dev, &dev_attr_power_reduce);
	if (unlikely(ret < 0)) {
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_power_reduce.attr.name);
		goto err_power_reduce;
	}

	ret = device_create_file(lcd->dev, &dev_attr_read_mtp);
	if (unlikely(ret < 0)) {
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_read_mtp.attr.name);
		goto err_read_mtp;
	}

	ret = device_create_file(lcd->dev, &dev_attr_write_mtp);
	if (unlikely(ret < 0)) {
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_read_mtp.attr.name);
		goto err_write_mtp;
	}

	ret = device_create_file(lcd->dev, &dev_attr_temperature);
	if (unlikely(ret < 0)) {
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_temperature.attr.name);
		goto err_temperature;
	}

	ret = device_create_file(lcd->dev, &dev_attr_cell_id);
	if (unlikely(ret < 0)) {
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_cell_id.attr.name);
		goto err_cell_id;
	}
#endif

	glcd = lcd;

	gen_panel_attach_mdnie(&lcd->mdnie, &mdnie_ops);
	if (gen_panel_match_backlight(lcd->bd, GEN_PANEL_BL_NAME))
		gen_panel_backlight_device_register(lcd->bd,
				lcd, &backlight_ops);
	gen_panel_attach_tuning(lcd);
	dev_info(lcd->dev, "[LCD] GEN OCTA device init done\n");

	return 0;
#ifdef CONFIG_GEN_PANEL_OCTA
err_cell_id:
	device_remove_file(lcd->dev, &dev_attr_temperature);
err_temperature:
	device_remove_file(lcd->dev, &dev_attr_write_mtp);
err_write_mtp:
	device_remove_file(lcd->dev, &dev_attr_read_mtp);
err_read_mtp:
	device_remove_file(lcd->dev, &dev_attr_power_reduce);
err_power_reduce:
	device_remove_file(&lcd->bd->dev, &dev_attr_weakness_hbm_comp);
err_weakness_hbm_comp:
	list_for_each_entry(gen_attr, &attr_list, list)
		device_remove_file(lcd->dev, &gen_attr->dev_attr);
#endif
err_manufacture_code:
	device_remove_file(lcd->dev, &dev_attr_manufacture_code);
err_window_type:
	device_remove_file(lcd->dev, &dev_attr_window_type);
err_lcd_type:
	device_remove_file(lcd->dev, &dev_attr_lcd_type);
err_lcd_name:
	device_remove_file(lcd->dev, &dev_attr_panel_name);
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
	gen_panel_detach_mdnie(&lcd->mdnie);
	if (gen_panel_match_backlight(lcd->bd, GEN_PANEL_BL_NAME))
		gen_panel_backlight_device_unregister(lcd->bd);
	list_for_each_entry(gen_attr, &attr_list, list)
		device_remove_file(lcd->dev, &gen_attr->dev_attr);
	device_remove_file(lcd->dev, &dev_attr_panel_name);
	device_remove_file(lcd->dev, &dev_attr_lcd_type);
	device_remove_file(lcd->dev, &dev_attr_window_type);
	device_remove_file(lcd->dev, &dev_attr_manufacture_code);
	device_destroy(lcd->class, 0);
	class_destroy(lcd->class);

	free_gen_dev_attr();
	free_manipulate_table();
	kfree(lcd->extpin_on_0_seq.ctrls);
	kfree(lcd->extpin_on_seq.ctrls);
	kfree(lcd->extpin_off_seq.ctrls);
	free_extpin();
#ifdef CONFIG_GEN_PANEL_OCTA
	device_remove_file(&lcd->bd->dev, &dev_attr_weakness_hbm_comp);
	device_remove_file(lcd->dev, &dev_attr_power_reduce);
	device_remove_file(lcd->dev, &dev_attr_read_mtp);
	device_remove_file(lcd->dev, &dev_attr_write_mtp);
	device_remove_file(lcd->dev, &dev_attr_temperature);
	device_remove_file(lcd->dev, &dev_attr_cell_id);
	/* free candela map tables */
	kfree(aid_table);
	kfree(acl_table);
	kfree(mps_table);
	kfree(elvss_table);
	kfree(elvss_temp_table);
	kfree(base_lux_table);
	kfree(gamma_curve_table);
	kfree(rgb_offset);
	kfree(candela_offset);
	kfree(candela_table);
	kfree(gamma_table);
#endif
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
