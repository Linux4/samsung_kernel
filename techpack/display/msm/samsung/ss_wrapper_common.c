/*
 * =================================================================
 *
 *
 *	Description:  samsung display common file
 *	Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2021, Samsung Electronics. All rights reserved.

 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "ss_wrapper_common.h"

#define MAX_LEN_DUMP_BUF	(256)

/*
 * Check CONFIG_DPM_WATCHDOG_TIMEOUT time.
 * Device should not take resume/suspend over CONFIG_DPM_WATCHDOG_TIMEOUT time.
 */
#define WAIT_PM_RESUME_TIMEOUT_MS	(3000)	/* 3 seconds */

int __mockable ss_wait_for_pm_resume(
			struct samsung_display_driver_data *vdd)
{
	struct drm_device *ddev = GET_DRM_DEV(vdd);
	int timeout = WAIT_PM_RESUME_TIMEOUT_MS;

	while (!pm_runtime_enabled(ddev->dev) && --timeout)
		usleep_range(1000, 1000); /* 1ms */

	if (timeout < WAIT_PM_RESUME_TIMEOUT_MS)
		LCD_INFO(vdd, "wait for pm resume (timeout: %d)", timeout);

	if (!timeout) {
		LCD_ERR(vdd, "pm resume timeout \n");

		/* Should not reach here... Add panic to debug further */
		panic("timeout wait for pm resume");
		return -ETIMEDOUT;
	}

	return 0;
}

int __mockable ss_gpio_get_value(struct samsung_display_driver_data *vdd, unsigned int gpio)
{
	struct gpio_desc *desc;

	desc = gpio_to_desc(gpio);
	if (unlikely(gpiod_cansleep(desc)))
		return gpio_get_value_cansleep(gpio);
	else
		return gpio_get_value(gpio);
}

int __mockable ss_gpio_set_value(struct samsung_display_driver_data *vdd, unsigned int gpio, int value)
{
	struct gpio_desc *desc;

	desc = gpio_to_desc(gpio);
	if (unlikely(gpiod_cansleep(desc)))
		gpio_set_value_cansleep(gpio, value);
	else
		gpio_set_value(gpio, value);

	return 0;
}

bool __mockable ss_gpio_is_valid(int number)
{
	return gpio_is_valid(number);
}

int __mockable ss_gpio_to_irq(unsigned gpio)
{
	return gpio_to_irq(gpio);
}

int __mockable ss_wrapper_dsi_panel_tx_cmd_set(struct dsi_panel *panel, int type)
{
	int rc = 0;
	struct dsi_display *display = container_of(panel->host, struct dsi_display, host);
	struct samsung_display_driver_data *vdd = display->panel->panel_private;

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_ALL_CLKS, DSI_CLK_ON);
	if (rc) {
		LCD_ERR(vdd, "[%s] failed to enable DSI core clocks, rc=%d\n",
				display->name, rc);
		goto error;
	}

	rc = dsi_panel_tx_cmd_set(panel, type);

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_OFF);
	if (rc) {
		LCD_ERR(vdd, "[%s] failed to disable DSI core clocks, rc=%d\n",
				display->name, rc);
		goto error;
	}

error:
	return rc;
}

int __mockable ss_wrapper_regulator_get_voltage(struct samsung_display_driver_data *vdd,
						struct regulator *regulator)
{
	if (!regulator) {
		LCD_ERR(vdd, "err: null regulator\n");
		return -ENODEV;
	}

	return regulator_get_voltage(regulator);
}

int __mockable ss_wrapper_regulator_set_voltage(struct samsung_display_driver_data *vdd,
						struct regulator *regulator, int min_uV, int max_uV)
{
	if (!regulator) {
		LCD_ERR(vdd, "err: null regulator\n");
		return -ENODEV;
	}

	return regulator_set_voltage(regulator, min_uV, max_uV);
}

int __mockable ss_wrapper_regulator_set_short_detection(struct samsung_display_driver_data *vdd,
						struct regulator *regulator, bool enable, int lv_uA)
{
	if (!regulator) {
		LCD_ERR(vdd, "err: null regulator\n");
		return -ENODEV;
	}

#if IS_ENABLED(CONFIG_SEC_PM)
	return regulator_set_short_detection(regulator, enable, lv_uA);
#else
	LCD_ERR(vdd, "err: CONFG_SEC_PM is not Enabled\n");
	return -1;
#endif
}

int __mockable ss_wrapper_misc_register(struct samsung_display_driver_data *vdd,
					struct miscdevice *misc)
{
	if (!misc) {
		LCD_ERR(vdd, "err: null misc\n");
		return -ENODEV;
	}

	return misc_register(misc);
}

int ss_wrapper_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	if (!adap) {
		pr_debug("err: null adap\n");
		return -ENODEV;
	}

	return  i2c_transfer(adap, msgs, num);
}

int __ss_parse_ss_cmd_sets_revision(struct samsung_display_driver_data *vdd,
					struct ss_cmd_set *set,
					int type, struct device_node *np,
					char *cmd_name)
{
	int rc = 0;
	int rev;
	char map[SS_CMD_PROP_STR_LEN];
	struct ss_cmd_set *set_rev[SUPPORT_PANEL_REVISION];
	int len;

	if (!set) {
		LCD_ERR(vdd, "err: set_rev: set is null\n");
		return -EINVAL;
	}

	/* For revA */
	set->cmd_set_rev[0] = set;

	/* For revB ~ revT */
	strncpy(map, cmd_name, SS_CMD_PROP_STR_LEN);
	len = strlen(cmd_name);

	if (len >= SS_CMD_PROP_STR_LEN) {
		LCD_ERR(vdd, "err: set_rev: too long cmd name\n");
		return -EINVAL;
	}

	for (rev = 1; rev < SUPPORT_PANEL_REVISION; rev++) {
		const char *data;
		u32 length = 0;

		if (map[len - 1] >= 'A' && map[len - 1] <= 'Z')
			map[len - 1] = 'A' + rev;

		data = ss_wrapper_of_get_property(np, map, &length);
		if (!data || !length) {
			/* If there is no data for the panel rev,
			 * copy previous panel rev data pointer.
			 */
			set->cmd_set_rev[rev] = set->cmd_set_rev[rev - 1];
			continue;
		}

		set_rev[rev] = kzalloc(sizeof(struct ss_cmd_set), GFP_KERNEL);

		rc = ss_wrapper_parse_cmd_sets(vdd, set_rev[rev], type, np, map);
		if (rc) {
			LCD_ERR(vdd, "set_rev: fail to parse rev(%d) set\n", rev);
			goto err_free_mem;
		}

		set->cmd_set_rev[rev] = set_rev[rev];
	}

	return 0;

err_free_mem:
	LCD_ERR(vdd, "err, set_rev: free set(rc: %d, type: %d, rev: %d)\n",
			rc, type, rev);
	for (rev = rev - 1; rev >= 1; --rev) {
		if (set_rev[rev])
			kfree(set_rev[rev]);
	}

	return rc;
}

__mockable u8 *ss_wrapper_of_get_property(const struct device_node *np,
					const char *name, int *lenp)
{
	return (u8 *)of_get_property(np, name, lenp);
}

__mockable int ss_wrapper_of_get_named_gpio(struct device_node *np,
					const char *name, int index)
{
	return of_get_named_gpio(np, name, index);
}

enum SS_CMD_CTRL {
	CMD_CTRL_INVAL = 0,
	CMD_CTRL_WRITE,
	CMD_CTRL_READ,
	CMD_CTRL_DELAY,
	MAX_CMD_CTRL,
};

enum SS_CMD_CTRL_STR {
	CMD_STR_WRITE = 'W',
	CMD_STR_READ = 'R',
	CMD_STR_DELAY = 'D',
	CMD_STR_SPACE = 0x20,
	CMD_STR_CRLF = 0xa,
	CMD_STR_TAB = 0x9
};

#define IS_CMD_DELIMITER(c)	((c) == CMD_STR_SPACE || (c) == CMD_STR_CRLF || (c) == CMD_STR_TAB)

static int ss_get_cmd_pkt_count(const char *data, u32 length, u32 *cnt)
{
	u32 count = 0;
	char c;
	int i = 0;

	while ((c = data[i++])) {
		if (c == CMD_STR_READ || c == CMD_STR_WRITE)
			count++;
	}

	*cnt = count;

	return 0;
}
static int ss_alloc_cmd_packets(struct ss_cmd_set *set, u32 packet_count)
{
	u32 size;

	size = packet_count * sizeof(*set->cmds);
	set->cmds = kzalloc(size, GFP_KERNEL);
	if (!set->cmds) {
		pr_err("fail to allocate cmds\n");
		return -ENOMEM;
	}

	set->count = packet_count;

	return 0;
}

static int ss_cmd_val_count(const char *data)
{
	int cnt_val = 0;
	char c, next_c;
	int length = strlen(data);
	int i;

	i = 0;
	while (i < length && (c = data[i])) {
		if (IS_CMD_DELIMITER(c)) {
			++i;
			continue;
		}

		next_c = data[i + 1];

		if (IS_CMD_DELIMITER(next_c))
			break;

		++cnt_val;
		i += 2;
	};

	return cnt_val;
}


u8 ss_cmd_get_type(int cmd_ctrl, int cnt_val)
{
	u8 type;

	if (cmd_ctrl == CMD_CTRL_READ) {
		type = MIPI_DSI_DCS_READ; /* 06h */
	} else if (cmd_ctrl == CMD_CTRL_WRITE) {
		if (cnt_val == 1)
			type = MIPI_DSI_DCS_SHORT_WRITE; /* 05h */
		else
			type = MIPI_DSI_GENERIC_LONG_WRITE; /* 29h */
		/* type = MIPI_DSI_DCS_LONG_WRITE; // 39h
		 * some ddi or some condition requires to set 39h type..
		 * TODO: find that condition and use it..
		 */
	} else {
		pr_err("err: get_type: invalid cmd_ctrl(%d)\n", cmd_ctrl);
		type = -EINVAL;
	}

	return type;
}

static int ss_create_cmd_packets(struct samsung_display_driver_data *vdd,
				const char *data, u32 length, u32 count,
				struct ss_cmd_desc *cmds, struct ss_cmd_set *set)
{
	int rc = 0;
	int p;
	char c, next_c;
	int cmd_idx = 0;
	int cmd_ctrl = CMD_CTRL_INVAL;
	char dump_buffer[MAX_LEN_DUMP_BUF] = {0,};
	char tmpdump_buffer[MAX_LEN_DUMP_BUF] = {0,};
	int pos;
	int tmppos;

	p = 0;
	while (p < length && (c = data[p])) {
		if (IS_CMD_DELIMITER(c)) {
			++p;
			continue;
		}

		next_c = data[p + 1];

		if (IS_CMD_DELIMITER(next_c)) {
			/* control command: W, R, and D */
			switch (c) {
			case CMD_STR_WRITE:
				cmd_ctrl = CMD_CTRL_WRITE;
				break;
			case CMD_STR_READ:
				cmd_ctrl = CMD_CTRL_READ;
				break;
			case CMD_STR_DELAY:
				cmd_ctrl = CMD_CTRL_DELAY;
				break;
			default:
				LCD_ERR(vdd, "invalid ctrl: '%c'\n", c);
				cmd_ctrl = CMD_CTRL_INVAL;
				rc = -EINVAL;
				goto error_free_payloads;
			}
		} else {
			int cnt_val = ss_cmd_val_count(&data[p]);
			u8 *payload;
			u8 rxinfo[4]; /* addr len offset */
			int wait_ms;
			int j;

			/* hexadecimal values */
			switch (cmd_ctrl) {
			case CMD_CTRL_DELAY:
				if (cnt_val != 1) {
					LCD_ERR(vdd, "invalid value count(%d) for DELAY\n", cnt_val);
					rc = -EINVAL;
					goto error_free_payloads;
				}

				if (cmd_idx < 1) {
					LCD_ERR(vdd, "err: DELAY w/o cmd(%d), cnt: %d\n", cmd_idx, cnt_val);
					rc = -EINVAL;
					goto error_free_payloads;
				}

				rc = sscanf(&data[p], "%x", &wait_ms);
				cmds[cmd_idx - 1].post_wait_ms = wait_ms;
				break;

			case CMD_CTRL_WRITE:
				payload = kcalloc(cnt_val, sizeof(u8), GFP_KERNEL);
				if (!payload) {
					rc = -ENOMEM;
					goto error_free_payloads;
				}

				pos = 0;
				memset(dump_buffer, 0, MAX_LEN_DUMP_BUF);
				for (j = 0; j < cnt_val; j++, p += 2) {
					while (IS_CMD_DELIMITER(data[p]))
						++p;

					rc = sscanf(&data[p], "%x", &payload[j]);
					if (rc != 1) {
						LCD_ERR(vdd, "fail to pasrse (%s, p: %d)\n", &data[p], p);
						rc = -EINVAL;
						goto error_free_payloads;
					}

					pos += snprintf(dump_buffer + pos, MAX_LEN_DUMP_BUF - pos,
							"%02X ", payload[j]);
				}
				p -= 2;

				LCD_DEBUG(vdd, "WRITE: cnt: %d, data: %s\n", cnt_val, dump_buffer);

				cmds[cmd_idx].type = ss_cmd_get_type(cmd_ctrl, cnt_val);
				/* TODO: get some hint to set last_command */
				cmds[cmd_idx].last_command = 0;
				/* will be updated with following DELAY command */
				cmds[cmd_idx].post_wait_ms = 0;
				cmds[cmd_idx].tx_len = cnt_val;
				cmds[cmd_idx].txbuf = payload;
				cmds[cmd_idx].rx_len = 0;
				cmds[cmd_idx].rx_offset = 0;
				cmds[cmd_idx].rxbuf = NULL;

				++cmd_idx;
				break;

			case CMD_CTRL_READ:
				if (!((!vdd->two_byte_gpara && cnt_val == 3) ||
							(vdd->two_byte_gpara && cnt_val == 4))) {
					LCD_ERR(vdd, "invalid cmd count for RX (%d)\n", cnt_val);
					rc = -EINVAL;
					goto error_free_payloads;
				}

				pos = 0;
				memset(dump_buffer, 0, MAX_LEN_DUMP_BUF);
				tmppos = 0;
				memset(tmpdump_buffer, 0, MAX_LEN_DUMP_BUF);
				for (j = 0; j < cnt_val; j++, p += 2) {
					while (IS_CMD_DELIMITER(data[p]))
						++p;

					rc = sscanf(&data[p], "%x", &rxinfo[j]);
					if (rc != 1) {
						LCD_ERR(vdd, "fail to pasrse (%s, p: %d)\n", &data[p], p);
						rc = -EINVAL;
						goto error_free_payloads;
					}
					pos += snprintf(dump_buffer + pos, MAX_LEN_DUMP_BUF - pos,
							"%02X ", rxinfo[j]);
					tmppos += sprintf(tmpdump_buffer + tmppos, "%02X ", rxinfo[j]);
				}
				p -= 2;

				LCD_DEBUG(vdd, "READ: cnt: %d, data: %s\n", cnt_val, dump_buffer);

				cmds[cmd_idx].type = ss_cmd_get_type(cmd_ctrl, cnt_val);
				cmds[cmd_idx].last_command = 1;
				cmds[cmd_idx].post_wait_ms = 0; /* will be updated with later DELAY command */

				cmds[cmd_idx].tx_len = 1;
				cmds[cmd_idx].txbuf = kcalloc(cmds[cmd_idx].tx_len, sizeof(u8), GFP_KERNEL);
				if (!cmds[cmd_idx].txbuf) {
					rc = -ENOMEM;
					goto error_free_payloads;
				}
				cmds[cmd_idx].txbuf[0] = rxinfo[0];
				cmds[cmd_idx].rx_len = rxinfo[1];
				if (!vdd->two_byte_gpara)
					cmds[cmd_idx].rx_offset = rxinfo[2];
				else
					cmds[cmd_idx].rx_offset = (rxinfo[2] << 8) | rxinfo[3];

				/* rxbuf will be allocated in ss_pack_rx_gpara() and ss_pack_rx_no_gpara() */

				++cmd_idx;
				break;

			case CMD_CTRL_INVAL:
			default:
				LCD_ERR(vdd, "no valid ctrl(%d), count: %d\n", cmd_ctrl, cnt_val);
				rc = -EINVAL;
				goto error_free_payloads;
			}

			/* invalidate to check if next value is CTRL */
			cmd_ctrl = CMD_CTRL_INVAL;

		}
		p += 2;
	};

	/* send mipi rx packets in LP mode to prevent SoT error.
	 * case 03377897
	 * In combinaiton of tx/rx commands, use only HS mode...
	 */
	if (set->count == 1 && ss_is_read_ss_cmd(&set->cmds[0]))
		set->state = SS_CMD_SET_STATE_LP;
	else
		set->state = SS_CMD_SET_STATE_HS;

	return 0;

error_free_payloads:
	LCD_ERR(vdd, "err, free: cmd_idx: %d", cmd_idx);
	for (cmd_idx = cmd_idx - 1; cmd_idx >= 0; cmd_idx--) {
		cmds--;
		kfree(cmds->txbuf);
		kfree(cmds->rxbuf);
	}

	return rc;
}

__visible_for_testing int ss_bridge_qc_cmd_alloc(
				struct samsung_display_driver_data *vdd,
				struct ss_cmd_set *ss_set)
{
	struct dsi_panel_cmd_set *qc_set = &ss_set->base;
	struct ss_cmd_desc *cmd = ss_set->cmds;
	int rx_split_num;
	u32 size;
	int i;

	qc_set->count = ss_set->count;

	if (!vdd->gpara)
		goto alloc;

	for (i = 0; i < ss_set->count; i++, cmd++) {
		if (!ss_is_read_ss_cmd(cmd))
			continue;

		rx_split_num = DIV_ROUND_UP(cmd->rx_len, RX_SIZE_LIMIT);
		rx_split_num *= 2; /* gparam(B0h) and rx cmd */
		qc_set->count += rx_split_num  - 1; /* one ss cmd --> rx_split_num */
	}

alloc:
	size = qc_set->count * sizeof(*qc_set->cmds);
	qc_set->cmds = kzalloc(size, GFP_KERNEL);
	if (!qc_set->cmds)
		return -ENOMEM;

	return 0;
}

static struct dsi_cmd_desc *ss_pack_rx_gpara(struct samsung_display_driver_data *vdd,
		struct ss_cmd_desc *ss_cmd, struct dsi_cmd_desc *qc_cmd)
{
	int tot_rx_len;
	u8 rx_addr;
	int i;
	int rx_split_num;
	int rx_offset;

	ss_cmd->rxbuf = kzalloc(ss_cmd->rx_len, GFP_KERNEL);
	if (!ss_cmd->rxbuf) {
		LCD_ERR(vdd, "fail to alloc ss rxbuf(%zd)\n", ss_cmd->rx_len);
		return NULL;
	}

	rx_addr = ss_cmd->txbuf[0];
	rx_offset = ss_cmd->rx_offset;
	tot_rx_len = ss_cmd->rx_len;
	rx_split_num = DIV_ROUND_UP(tot_rx_len, RX_SIZE_LIMIT);

	for (i = 0; i < rx_split_num; i++) {
		int pos = 0;
		u8 *payload;
		int rx_len;

		/* 1. Send gpara first with new offset */
		/* if samsung,two_byte_gpara is true,
		 * that means DDI uses two_byte_gpara
		 * to figure it, check cmd length
		 * 06 01 00 00 00 00 01 DA 01 00 : use 1byte gpara, length is 10
		 * 06 01 00 00 00 00 01 DA 01 00 00 : use 2byte gpara, length is 11
		 */
		qc_cmd->msg.tx_len = 2;

		if (vdd->two_byte_gpara)
			qc_cmd->msg.tx_len++;
		if (vdd->pointing_gpara)
			qc_cmd->msg.tx_len++;

		payload = kzalloc(qc_cmd->msg.tx_len, GFP_KERNEL);
		if (!payload) {
			LCD_ERR(vdd, "fail to alloc tx_buf..\n");
			return NULL;
		}

		payload[pos++] = 0xB0;

		if (vdd->two_byte_gpara) {
			payload[pos++] = (rx_offset >> 8) & 0xff;
			payload[pos++] =  rx_offset & 0xff;
		} else {
			payload[pos++] = rx_offset & 0xff;
		}

		/* set pointing gpara */
		if (vdd->pointing_gpara)
			payload[pos] = rx_addr;

		qc_cmd->msg.type = MIPI_DSI_GENERIC_LONG_WRITE;
		qc_cmd->last_command = true; /* transmit B0h before rx cmd */
		qc_cmd->msg.channel = 0;
		qc_cmd->msg.flags = 0;
		qc_cmd->msg.ctrl = 0;
		qc_cmd->post_wait_ms = qc_cmd->msg.wait_ms = 0;
		qc_cmd->msg.tx_buf = payload;
		qc_cmd->ss_txbuf = payload; /* ss_txbuf: deprecated... */
		qc_cmd->msg.rx_len = 0;
		qc_cmd->msg.rx_buf = NULL;

		qc_cmd++;

		/* 2. Set new read length */
		if (i == (rx_split_num - 1))
			rx_len = (tot_rx_len - RX_SIZE_LIMIT * i);
		else
			rx_len = RX_SIZE_LIMIT;

		/* 3. RX */
		qc_cmd->msg.rx_len = rx_len;
		qc_cmd->msg.type = ss_cmd->type;
		qc_cmd->last_command = true;
		qc_cmd->msg.channel = 0;
		qc_cmd->msg.flags = 0;
		qc_cmd->msg.ctrl = 0;

		/* delay only after last recv cmd */
		if (i == (rx_split_num - 1))
			qc_cmd->post_wait_ms = qc_cmd->msg.wait_ms =
				ss_cmd->post_wait_ms;
		else
			qc_cmd->post_wait_ms = qc_cmd->msg.wait_ms = 0;

		qc_cmd->msg.tx_len = ss_cmd->tx_len;
		qc_cmd->msg.tx_buf = ss_cmd->txbuf;
		qc_cmd->ss_txbuf = ss_cmd->txbuf; /* ss_txbuf: deprecated... */
		qc_cmd->msg.rx_buf = ss_cmd->rxbuf + (i * RX_SIZE_LIMIT);

		if (vdd->dtsi_data.samsung_anapass_power_seq && rx_offset > 0) {
			payload = kzalloc(2 * sizeof(u8), GFP_KERNEL);
			if (!payload) {
				LCD_ERR(vdd, "fail to alloc tx_buf..\n");
				return NULL;
			}
			/* some panel need to update read size by address.
			 * It means "address + read_size" is essential.
			 */
			payload[0] = rx_addr;
			payload[1] = rx_len;
			LCD_DEBUG(vdd, "update address + read_size(%d)\n", rx_len);

			qc_cmd->msg.tx_buf = payload;
			qc_cmd->ss_txbuf = payload;
		}

		qc_cmd++;
		rx_offset += RX_SIZE_LIMIT;
	}

	return --qc_cmd;
}

static struct dsi_cmd_desc *ss_pack_rx_no_gpara(struct samsung_display_driver_data *vdd,
		struct ss_cmd_desc *ss_cmd, struct dsi_cmd_desc *qc_cmd)
{
	qc_cmd->msg.type = ss_cmd->type;
	qc_cmd->last_command = true;
	qc_cmd->msg.channel = 0;
	qc_cmd->msg.flags = 0;
	qc_cmd->msg.ctrl = 0;
	qc_cmd->post_wait_ms = qc_cmd->msg.wait_ms = ss_cmd->post_wait_ms;

	qc_cmd->msg.tx_len = ss_cmd->tx_len;
	qc_cmd->msg.tx_buf = ss_cmd->txbuf;
	qc_cmd->ss_txbuf = ss_cmd->txbuf; /* ss_txbuf: deprecated... */

	qc_cmd->msg.rx_len = ss_cmd->rx_len + ss_cmd->rx_offset;
	qc_cmd->msg.rx_buf = kzalloc(qc_cmd->msg.rx_len, GFP_KERNEL);
	if (!qc_cmd->msg.rx_buf) {
		LCD_ERR(vdd, "fail to alloc qc rx_buf(%zd)\n", qc_cmd->msg.rx_len);
		return NULL;
	}

	ss_cmd->rxbuf = qc_cmd->msg.rx_buf + ss_cmd->rx_offset;

	return qc_cmd;
}

__visible_for_testing int ss_bridge_qc_cmd_update(
			struct samsung_display_driver_data *vdd,
			struct ss_cmd_set *ss_set)
{
	struct dsi_panel_cmd_set *qc_set = &ss_set->base;
	struct dsi_cmd_desc *qc_cmd;
	struct ss_cmd_desc *ss_cmd = ss_set->cmds;
	int i;

	if (unlikely(!qc_set->cmds)) {
		LCD_ERR(vdd, "err: no qc cmd(%d)\n", ss_set->ss_type);
		return -ENOMEM;
	}

	qc_cmd = qc_set->cmds;

	for (i = 0; i < ss_set->count; ++i, ++ss_cmd, ++qc_cmd) {
		/* don't handle test key, should put test key in dtsi cmd */
		if (ss_is_read_ss_cmd(ss_cmd)) {
			if (vdd->gpara)
				qc_cmd = ss_pack_rx_gpara(vdd, ss_cmd, qc_cmd);
			else
				qc_cmd = ss_pack_rx_no_gpara(vdd, ss_cmd, qc_cmd);

			if (!qc_cmd) {
				LCD_ERR(vdd, "err: fail to handle rx cmd(i: %d)\n", i);
				return -EFAULT;
			}
		} else {
			qc_cmd->msg.type = ss_cmd->type;

			qc_cmd->last_command = 0;
			qc_cmd->msg.channel = 0;
			qc_cmd->msg.flags = 0;
			qc_cmd->msg.ctrl = 0;
			qc_cmd->post_wait_ms = qc_cmd->msg.wait_ms =
					ss_cmd->post_wait_ms;
			qc_cmd->msg.tx_len = ss_cmd->tx_len;
			qc_cmd->msg.tx_buf = ss_cmd->txbuf;
			qc_cmd->ss_txbuf = ss_cmd->txbuf; /* ss_txbuf: deprecated... */
			qc_cmd->msg.rx_len = 0;
			qc_cmd->msg.rx_buf = NULL;
		}
	}

	switch (ss_set->state) {
	case SS_CMD_SET_STATE_LP:
		qc_set->state = DSI_CMD_SET_STATE_LP;
		break;
	case SS_CMD_SET_STATE_HS:
		qc_set->state = DSI_CMD_SET_STATE_HS;
		break;
	default:
		LCD_ERR(vdd, "invalid ss state(%d)\n", ss_set->state);
		return -EINVAL;
		break;
	}

	if (--qc_cmd != &qc_set->cmds[qc_set->count - 1] ||
			--ss_cmd != &ss_set->cmds[ss_set->count - 1]) {
		LCD_ERR(vdd, "error: unmatched countcritical count err\n");
		return -EFAULT;
	}

	return 0;
}

#define kfree_and_null(x)	{ \
	kfree(x); \
	x = NULL; \
}

static int ss_free_cmd_set(struct samsung_display_driver_data *vdd, struct ss_cmd_set *set)
{
	void *prev_set_rev = NULL;
	int i;

	if (!set)
		return -EINVAL;

	/* free ss cmds */
	for (i = 0; i < set->count; i++) {
		kfree_and_null(set->cmds[i].txbuf);
		if (vdd->gpara)
			kfree_and_null(set->cmds[i].rxbuf);
	}

	set->count = 0;
	kfree_and_null(set->cmds);
	kfree_and_null(set->self_disp_cmd_set_rev);

	prev_set_rev = set->cmd_set_rev[0];
	for (i = 1; i < SUPPORT_PANEL_REVISION; i++) {
		if (prev_set_rev == set->cmd_set_rev[i]) {
			set->cmd_set_rev[i] = NULL;
			continue;
		}

		prev_set_rev = set->cmd_set_rev[i];
		kfree_and_null(set->cmd_set_rev[i]);
	}

	/* free qc cmds */
	for (i = 0; i < set->base.count; i++) {
		if (!vdd->gpara)
			kfree_and_null(set->base.cmds[i].msg.rx_buf);
	}

	set->base.count = 0;
	kfree_and_null(set->base.cmds);

	return 0;
}

__visible_for_testing int ss_update_cmd_each_mode(struct samsung_display_driver_data *vdd,
					struct ss_cmd_set *set_common,
					struct ss_cmd_set *set_each)
{
	/* to free org memory (ss: cmds, txbuf, rxbuf, qc: cmds, tx_buf, rx_buf) */
	struct ss_cmd_set set_each_org = *set_each;
	int i, j;
	int ret = 0;

	/* re allocate for qc_cmds for each modes */
	set_each->count = set_common->count;
	set_each->cmds = kcalloc(set_each->count, sizeof(struct ss_cmd_desc), GFP_KERNEL);
	if (!set_each->cmds) {
		LCD_ERR(vdd, "fail to acllocate cmds\n");
		ret = -ENOMEM;
		goto end;
	}

	for (i = 0, j = 0; i < set_each->count; i++) {
		/* copy re allocated ss_cmds from common dsi on cmd ss cmds */
		set_each->cmds[i] = set_common->cmds[i];
		set_each->cmds[i].txbuf = kzalloc(set_each->cmds[i].tx_len, GFP_KERNEL);
		if (!set_each->cmds[i].txbuf) {
			LCD_ERR(vdd, "fail to acllocate txbuf\n");
			ret = -ENOMEM;
			goto end;
		}

		memcpy(set_each->cmds[i].txbuf, set_common->cmds[i].txbuf, set_each->cmds[i].tx_len);

		/* TODO: determine policy how to get hint for updatable.
		 * (ex: XX value from original qc_cmds value)
		 * its ADDR (txbuf[0]) and tx_len should be same.
		 */
		if (set_each->cmds[i].updatable) {
			if (j >= set_each_org.count) {
				LCD_ERR(vdd, "error: unmatched updatable count(%d, %d)\n",
						j, set_each_org.count);
				ret = -EINVAL;
				goto end;
			}

			if (set_each->cmds[i].txbuf[0] != set_each_org.cmds[j].txbuf[0] ||
					set_each->cmds[i].tx_len != set_each_org.cmds[j].tx_len) {
				LCD_ERR(vdd, "error: [%d %d] unmatched addr/len(%02X %02X, %zd %zd)\n", i, j,
						set_each->cmds[i].txbuf[0], set_each_org.cmds[j].txbuf[0],
						set_each->cmds[i].tx_len, set_each_org.cmds[j].tx_len);
				ret = -EINVAL;
				goto end;
			}

			memcpy(set_each->cmds[i].txbuf, set_each_org.cmds[j].txbuf,
					set_each->cmds[i].tx_len);
			j++;
		}
	}

	/* check error.. if matched XX count and each mode count */
	if (i != set_each->count || j != set_each_org.count) {
		LCD_ERR(vdd, "error: unmatched count (%d %d, %d %d)\n",
				i, set_each->count, j, set_each_org.count);
		ret = -EINVAL;
		goto end;
	}

	/* then, parse it and update for qc cmd in each mode */
	ret = ss_bridge_qc_cmd_alloc(vdd, set_each);
	if (ret) {
		LCD_ERR(vdd, "fail to alloc qc cmd(%d)\n", ret);
		ss_free_cmd_set(vdd, set_each);
		goto end;
	}

	ret = ss_bridge_qc_cmd_update(vdd, set_each);
	if (ret) {
		LCD_ERR(vdd, "fail to update qc cmd(%d)\n", ret);
		ss_free_cmd_set(vdd, set_each);
		goto end;
	}

end:
	ss_free_cmd_set(vdd, &set_each_org);

	return ret;
}

int ss_wrapper_parse_cmd_sets_each_mode(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel *panel;
	struct ss_cmd_set_each_mode *each_mode;

	int rc = 0;
	int type;
	struct dsi_parser_utils *utils;

	struct device_node *timings_np, *child_np;
	int num_timings;

	struct ss_cmd_set *set_common;
	struct ss_cmd_set *set_each;
	struct dsi_panel_cmd_set *qc_set;
	char *cmd_name;

	if (!vdd)
		return -ENODEV;

	if (!vdd->support_ss_cmd)
		return 0;

	panel = GET_DSI_PANEL(vdd);
	if (!panel) {
		LCD_ERR(vdd, "panel is null\n");
		return -ENODEV;
	}

	/* refer to dsi_panel_get_mode */
	utils = &panel->utils;

	timings_np = of_get_child_by_name(utils->data,
		"qcom,mdss-dsi-display-timings");
	if (!timings_np) {
		LCD_ERR(vdd, "no display timing nodes defined\n");
		return -EINVAL;
	}

	num_timings = of_get_child_count(timings_np);

	each_mode = kcalloc(num_timings, sizeof(*each_mode), GFP_KERNEL);
	if (!each_mode) {
		LCD_ERR(vdd, "fail to alloc each_mode\n");
		return -ENOMEM;
	}

	vdd->dtsi_data.ss_cmd_sets_each_mode = each_mode;
	vdd->dtsi_data.num_each_mode = num_timings;

	for_each_child_of_node(timings_np, child_np) {
		/* refer to dsi_panel_parse_timing */
		rc = of_property_read_u32(child_np, "qcom,mdss-dsi-panel-width", &each_mode->h_active);
		if (rc) {
			LCD_ERR(vdd, "failed to read qcom,mdss-dsi-panel-width, rc=%d\n", rc);
			return rc;
		}

		rc = of_property_read_u32(child_np, "qcom,mdss-dsi-panel-height", &each_mode->v_active);
		if (rc) {
			LCD_ERR(vdd, "failed to read qcom,mdss-dsi-panel-height, rc=%d\n", rc);
			return rc;
		}

		rc = of_property_read_u32(child_np, "qcom,mdss-dsi-panel-framerate", &each_mode->refresh_rate);
		if (rc) {
			LCD_ERR(vdd, "failed to read qcom,mdss-dsi-panel-framerate, rc=%d\n", rc);
			return rc;
		}

		each_mode->sot_hs_mode = of_property_read_bool(child_np, "samsung,mdss-dsi-sot-hs-mode");
		each_mode->phs_mode = of_property_read_bool(child_np, "samsung,mdss-dsi-phs-mode");

		for (type = SS_DSI_CMD_SET_FOR_EACH_MODE_START;
				type <= SS_DSI_CMD_SET_FOR_EACH_MODE_END; type++) {
			cmd_name = ss_cmd_set_prop_map[type - SS_DSI_CMD_SET_START];
			if (!ss_is_prop_string_style(child_np, cmd_name))
				continue;

			set_each = &each_mode->sets[type - SS_DSI_CMD_SET_FOR_EACH_MODE_START];
			set_each->ss_type = type;
			set_each->count = 0;

			qc_set = &set_each->base;
			qc_set->ss_cmd_type = type;
			qc_set->count = 0;

			rc = ss_wrapper_parse_cmd_sets(vdd, set_each, type, child_np, cmd_name);
			if (rc) {
				LCD_ERR(vdd, "failed to parse set %d, rc: %d\n", type, rc);
				return rc;
			}

			rc = __ss_parse_ss_cmd_sets_revision(vdd, set_each, type, child_np, cmd_name);
			if (rc) {
				LCD_ERR(vdd, "failed to parse subrev set %d, rc: %d\n", type, rc);
				return rc;
			}
		}
		each_mode++;
	}

	/* update qcom,mdss-dsi-on-command for each DMS/VRR mocdes */
	set_common = ss_get_ss_cmds(vdd, TX_DSI_CMD_SET_ON);
	each_mode = vdd->dtsi_data.ss_cmd_sets_each_mode;
	for_each_child_of_node(timings_np, child_np) {
		set_each = &each_mode->sets[TX_DSI_CMD_SET_ON_EACH_MODE -
					SS_DSI_CMD_SET_FOR_EACH_MODE_START];

		rc = ss_update_cmd_each_mode(vdd, set_common, set_each);
		if (rc) {
			LCD_ERR(vdd, "fail to update dsi-on each mode(%d)\n", rc);
			return rc;
		}

		each_mode++;
	}

	return rc;
}

int ss_wrapper_parse_cmd_sets(struct samsung_display_driver_data *vdd,
				struct ss_cmd_set *set, int type,
				struct device_node *np, char *cmd_name)
{
	int rc = 0;
	const char *data;
	u32 length = 0;
	u32 packet_count = 0;
	/* Samsung RX cmd is splited to multiple QCT cmds, with RX cmds and
	 * B0h grapam cmds with RX_LIMIT(10 bytets)
	 */

	data = ss_wrapper_of_get_property(np, cmd_name, &length);
	if (!data) {
		LCD_ERR(vdd, "err: no matched data\n");
		return -EINVAL;
	}

	rc = ss_get_cmd_pkt_count(data, length, &packet_count);
	if (rc) {
		LCD_ERR(vdd, "fail to count pkt (%d)\n", rc);
		goto error;
	}

	rc = ss_alloc_cmd_packets(set, packet_count);
	if (rc) {
		LCD_ERR(vdd, "fail to allocate cmd(%d)\n", rc);
		goto error;
	}

	rc = ss_create_cmd_packets(vdd, data, length, packet_count, set->cmds, set);
	if (rc) {
		LCD_ERR(vdd, "fail to create cmd(%d)\n", rc);
		goto err_free_cmds;
	}

	/* instead of calling ss_bridge_qc_cmd_alloc, alloc cmd here...???
	 * todo: care of qc_set for sub revisions...
	 */
	rc = ss_bridge_qc_cmd_alloc(vdd, set);
	if (rc) {
		LCD_ERR(vdd, "fail to alloc qc cmd(%d)\n", rc);
		goto err_free_cmds;
	}

	rc = ss_bridge_qc_cmd_update(vdd, set);
	if (rc) {
		LCD_ERR(vdd, "fail to update qc cmd(%d)\n", rc);
		goto err_free_cmds;
	}

	return 0;

err_free_cmds:
	ss_free_cmd_set(vdd, set);

error:
	return rc;

}

int ss_is_prop_string_style(struct device_node *np, char *cmd_name)
{
	const char *data;
	u32 length = 0;
	int is_str;

	data = ss_wrapper_of_get_property(np, cmd_name, &length);

	/* qc command style is
	 * samsung,mcd_on_tx_cmds_revA = [ 29 01 00 ...];,
	 * and it parses as integer value.
	 * It always '00' among the values,
	 * strlen is always smaller than  (prop->length - 1).
	 */
	if (data && strnlen(data, length) == (length - 1))
		is_str = 1;
	else
		is_str = 0;

	return is_str;
}

__visible_for_testing struct ss_cmd_set *ss_get_ss_cmds_each_mode(
		struct samsung_display_driver_data *vdd, int type)
{
	struct ss_cmd_set *set = NULL;
	struct ss_cmd_set_each_mode *each_mode;

	struct dsi_panel *panel = GET_DSI_PANEL(vdd);
	struct dsi_mode_info *timing;
	int mode_idx;

	if (type < SS_DSI_CMD_SET_FOR_EACH_MODE_START ||
			type > SS_DSI_CMD_SET_FOR_EACH_MODE_END) {
		LCD_ERR(vdd, "err: type(%d) is not ss each mode cmd\n", type);
		return NULL;
	}

	if (!vdd->support_ss_cmd) {
		LCD_ERR(vdd, "err: called without support_ss_cmd(%d)\n", type);
		return NULL;
	}

	if (!panel || !panel->cur_mode) {
		LCD_ERR(vdd, "err: (%d) panel has no valid priv\n", type);
		return NULL;
	}

	timing = &panel->cur_mode->timing;

	/* save the mipi cmd type */
	vdd->cmd_type = type;

	for (mode_idx = 0; mode_idx < vdd->dtsi_data.num_each_mode; mode_idx++) {
		each_mode = &vdd->dtsi_data.ss_cmd_sets_each_mode[mode_idx];
		if (timing->h_active == each_mode->h_active &&
			timing->v_active == each_mode->v_active &&
			timing->refresh_rate == each_mode->refresh_rate &&
			timing->sot_hs_mode == each_mode->sot_hs_mode &&
			timing->phs_mode == each_mode->phs_mode) {

			set = &each_mode->sets[type - SS_DSI_CMD_SET_FOR_EACH_MODE_START];
			break;
		}
	}

	if (!set) {
		LCD_INFO(vdd, "no matched timing set\n");
		goto no_matched_set;
	}

	return set;

no_matched_set:
	LCD_INFO(vdd, "fail to get set(%s) for %dx%d@%d(%d,%d)\n",
			ss_get_cmd_name(type),
			timing->h_active, timing->v_active,
			timing->refresh_rate,
			timing->sot_hs_mode, timing->phs_mode);

	return NULL;
}

/* Return SS cmd format */
struct ss_cmd_set *ss_get_ss_cmds(struct samsung_display_driver_data *vdd, int type)
{
	struct ss_cmd_set *set;
	int rev = vdd->panel_revision;

	/* save the mipi cmd type */
	vdd->cmd_type = type;

	if (type < SS_DSI_CMD_SET_START) {
		LCD_ERR(vdd, "err: type(%d) is not ss cmd\n", type);
		return NULL;
	}

	/* SS cmds */
	if (type == TX_AID_SUBDIVISION && vdd->br_info.common_br.pac)
		type = TX_PAC_AID_SUBDIVISION;
	else if (type == TX_IRC_SUBDIVISION && vdd->br_info.common_br.pac)
		type = TX_PAC_IRC_SUBDIVISION;

	if (type >= SS_DSI_CMD_SET_FOR_EACH_MODE_START &&
			type <= SS_DSI_CMD_SET_FOR_EACH_MODE_END)
		set = ss_get_ss_cmds_each_mode(vdd, type);
	else
		set = &vdd->dtsi_data.ss_cmd_sets[type - SS_DSI_CMD_SET_START];

	if (set && set->cmd_set_rev[rev])
		set = (struct ss_cmd_set *) set->cmd_set_rev[rev];

	return set;
}

int ss_print_rx_buf(struct samsung_display_driver_data *vdd, int type)
{
	struct ss_cmd_set *set;
	struct ss_cmd_desc *cmds;
	char dump_buffer[MAX_LEN_DUMP_BUF] = {0,};
	int pos;
	int i, j;

	if (type < SS_DSI_CMD_SET_START)
		return 0;

	set = ss_get_ss_cmds(vdd, type);
	if (!vdd->support_ss_cmd || !set || !set->count || !set->cmds)
		return 0;

	cmds = set->cmds;

	for (i = 0; i < set->count; i++, cmds++) {
		if (!cmds->rx_len || !cmds->rxbuf)
			continue;

		pos = 0;
		for (j = 0; j < cmds->rx_len && pos < MAX_LEN_DUMP_BUF; j++)
			pos += snprintf(dump_buffer + pos, MAX_LEN_DUMP_BUF - pos,
					"%02x ", cmds->rxbuf[j]);

		LCD_INFO(vdd, "%s: [cmdid %d] addr: 0x%02X, off: 0x%02X, len: %zd, data: %s\n",
				ss_get_cmd_name(type), i, cmds->txbuf[0],
				cmds->rx_offset, cmds->rx_len, dump_buffer);
	}

	return 0;
}

