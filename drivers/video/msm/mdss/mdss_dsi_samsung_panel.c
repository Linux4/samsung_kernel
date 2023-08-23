/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/qpnp/pin.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/lcd.h>
#include <linux/leds.h>
#include <linux/qpnp/pwm.h>
#include <linux/err.h>

#include "mdss_dsi.h"
#include "mdss_dsi_samsung_panel.h"

#define DT_CMD_HDR 6
#define SMART_DIMMING
#define SAMSUNG_DISP_SEND_CMD
/*#define SMART_ACL*/
/*#define CMD_DEBUG*/

#ifdef SMART_DIMMING

#ifdef SAMSUNG_DISP_SEND_CMD
static struct dsi_cmd_desc brightness_packet[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, 0}, NULL},
};

#define MAX_BR_PACKET_SIZE sizeof(brightness_packet)/sizeof(struct dsi_cmd_desc)

/*
static struct dsi_panel_cmds nv_enable_cmds;
static struct dsi_panel_cmds nv_disable_cmds;
static struct dsi_panel_cmds acl_off_cmd;
*/
#if defined(PARTIAL_UPDATE)
static struct dsi_cmd partialdisp_on_cmd;
static struct dsi_cmd partialdisp_off_cmd;
static int partial_disp_range[2];
#endif

#endif
#endif

DEFINE_LED_TRIGGER(bl_led_trigger);

void mdss_dsi_panel_pwm_cfg(struct mdss_dsi_ctrl_pdata *ctrl)
{
	ctrl->pwm_bl = pwm_request(ctrl->pwm_lpg_chan, "lcd-bklt");
	if (ctrl->pwm_bl == NULL || IS_ERR(ctrl->pwm_bl)) {
		pr_err("%s: Error: lpg_chan=%d pwm request failed",
				__func__, ctrl->pwm_lpg_chan);
	}
}

static void mdss_dsi_panel_bklt_pwm(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	int ret;
	u32 duty;

	if (ctrl->pwm_bl == NULL) {
		pr_err("%s: no PWM\n", __func__);
		return;
	}

	if (level == 0) {
		if (ctrl->pwm_enabled)
			pwm_disable(ctrl->pwm_bl);
		ctrl->pwm_enabled = 0;
		return;
	}

	duty = level * ctrl->pwm_period;
	duty /= ctrl->bklt_max;

	pr_debug("%s: bklt_ctrl=%d pwm_period=%d pwm_gpio=%d pwm_lpg_chan=%d\n",
			__func__, ctrl->bklt_ctrl, ctrl->pwm_period,
				ctrl->pwm_pmic_gpio, ctrl->pwm_lpg_chan);

	pr_debug("%s: ndx=%d level=%d duty=%d\n", __func__,
					ctrl->ndx, level, duty);

	if (ctrl->pwm_enabled) {
		pwm_disable(ctrl->pwm_bl);
		ctrl->pwm_enabled = 0;
	}

	ret = pwm_config_us(ctrl->pwm_bl, duty, ctrl->pwm_period);
	if (ret) {
		pr_err("%s: pwm_config_us() failed err=%d.\n", __func__, ret);
		return;
	}

	ret = pwm_enable(ctrl->pwm_bl);
	if (ret)
		pr_err("%s: pwm_enable() failed err=%d\n", __func__, ret);
	ctrl->pwm_enabled = 1;
}

static char dcs_cmd[2] = {0x54, 0x00}; /* DTYPE_DCS_READ */
static struct dsi_cmd_desc dcs_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(dcs_cmd)},
	dcs_cmd
};

u32 mdss_dsi_panel_cmd_read(struct mdss_dsi_ctrl_pdata *ctrl, char cmd0,
		char cmd1, void (*fxn)(int), char *rbuf, int len)
{
	struct dcs_cmd_req cmdreq;

	dcs_cmd[0] = cmd0;
	dcs_cmd[1] = cmd1;
	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &dcs_read_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq.rlen = len;
	cmdreq.rbuf = rbuf;
	cmdreq.cb = fxn; /* call back */
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
	/*
	 * blocked here, until call back called
	 */

	return 0;
}

u32 mdss_dsi_cmd_receive(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_cmd_desc *cmd, int rlen)
{
	struct dcs_cmd_req cmdreq;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq.rlen = rlen;
	cmdreq.rbuf = ctrl->rx_buf.data;
	cmdreq.cb = NULL; /* call back */
	/*
	 * This mutex is to sync up with dynamic FPS changes
	 * so that DSI lockups shall not happen
	 */
	BUG_ON(ctrl == NULL);
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
	/*
	 * blocked here, untill call back called
	 */
	return ctrl->rx_buf.len;
}

static void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_panel_cmds *pcmds)
{
	struct dcs_cmd_req cmdreq;
#ifdef PANEL_CMD_DEBUG
	int i, j;
#endif

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = pcmds->cmds;
	cmdreq.cmds_cnt = pcmds->cmd_cnt;
	cmdreq.flags = CMD_REQ_COMMIT;

	/*Panel ON/Off commands should be sent in DSI Low Power Mode*/
	if (pcmds->link_state == DSI_LP_MODE)
		cmdreq.flags  |= CMD_REQ_LP_MODE;

	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

#ifdef PANEL_CMD_DEBUG
	for (i = 0; i < pcmds->cmd_cnt; i++) {
		printk("%s: [CMD] :", __func__);
		for (j = 0; j < pcmds->cmds[i].dchdr.dlen; j++)
			printk("%x ", pcmds->cmds[i].payload[j]);
		printk("\n");
	}
#endif
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

static unsigned int mipi_samsung_manufacture_id(struct mdss_panel_data *pdata)
{
	unsigned int id = 0, i = 0, j = 0, read_size = 0;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mipi_samsung_driver_data *s_pdata = NULL;
	struct dsi_panel_cmds pcmds1;
	/* first byte is size of Register */
	static char packet_size[] = { 0x07, 0 };
	static struct dsi_cmd_desc s6e8aa0_packet_size_cmd = {
		{DTYPE_MAX_PKTSIZE, 1, 0, 0, 0, sizeof(packet_size)},
		packet_size };


	if (!pdata) {
		pr_err("%s : pdata is null\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	s_pdata = &pdata->samsung_pdata;

	if (!ctrl) {
		pr_err("%s : ctrl is null\n", __func__);
		return -EINVAL;
	}

	if (!s_pdata) {
		pr_err("%s : s_pdata is null\n", __func__);
		return -EINVAL;
	}


	if (!ctrl->cmd_list.panel_manufacture_id_cmds.cmd_cnt) {
		pr_err("%s : manufacture id cmd is null\n",__func__);
		return -EINVAL;
	}

	pcmds1.blen = 9;
	pcmds1.cmds = &s6e8aa0_packet_size_cmd;
	pcmds1.cmd_cnt = 1;

	mipi_samsung_disp_send_cmd(pdata, PANEL_MANUFACTURE_ID_REGISTER_SET_CMDS, true);
	mdss_dsi_panel_cmds_send(ctrl, &pcmds1);
	for (i = 0; i < ctrl->cmd_list.panel_manufacture_id_cmds.cmd_cnt; i++) {
		read_size = ctrl->cmd_list.panel_manufacture_id_cmds.read_size[i];
		mdss_dsi_cmd_receive(ctrl,
			&ctrl->cmd_list.panel_manufacture_id_cmds.cmds[i],
			read_size);

		for (j = 0; j < read_size; j++) {
			id <<= 8;
			pr_info("%s: manufacture_id[%d]=%x\n",
						 __func__, j, ctrl->rx_buf.data[j]);
			id |= ((unsigned int)ctrl->rx_buf.data[j] & 0xFF);
		}
	}
	s_pdata->manufacture_id = id;

	pr_info("%s: manufacture_id=%x\n", __func__, id);

	return id;
}


static char led_pwm1[2] = {0x51, 0x0};	/* DTYPE_DCS_WRITE1 */
static struct dsi_cmd_desc backlight_cmd = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm1)},
	led_pwm1
};

static void mdss_dsi_panel_bklt_dcs(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	struct dcs_cmd_req cmdreq;

	pr_debug("%s: level=%d\n", __func__, level);

	led_pwm1[1] = (unsigned char)level;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &backlight_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

static int mdss_dsi_request_gpios(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc = 0;
	static bool gpio_request_done = false;

	if (!gpio_request_done)
		return 0;

	if (gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
		rc = gpio_request(ctrl_pdata->disp_en_gpio,
						"disp_enable");
		if (rc) {
			pr_err("request disp_en gpio failed, rc=%d\n",
				       rc);
			goto disp_en_gpio_err;
		}
	}

	rc = gpio_request(ctrl_pdata->rst_gpio, "disp_rst_n");
	if (rc) {
		pr_err("request reset gpio failed, rc=%d\n",
			rc);
		goto rst_gpio_err;
	}
	if (gpio_is_valid(ctrl_pdata->bklt_en_gpio)) {
		rc = gpio_request(ctrl_pdata->bklt_en_gpio,
						"bklt_enable");
		if (rc) {
			pr_err("request bklt gpio failed, rc=%d\n",
				       rc);
			goto bklt_en_gpio_err;
		}
	}
	if (gpio_is_valid(ctrl_pdata->mode_gpio)) {
		rc = gpio_request(ctrl_pdata->mode_gpio, "panel_mode");
		if (rc) {
			pr_err("request panel mode gpio failed,rc=%d\n",
								rc);
			goto mode_gpio_err;
		}
	}
	gpio_request_done = true;
	return rc;

mode_gpio_err:
	if (gpio_is_valid(ctrl_pdata->bklt_en_gpio))
		gpio_free(ctrl_pdata->bklt_en_gpio);
bklt_en_gpio_err:
	gpio_free(ctrl_pdata->rst_gpio);
rst_gpio_err:
	if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
		gpio_free(ctrl_pdata->disp_en_gpio);
disp_en_gpio_err:
	return rc;
}

static int mdss_dsi_extra_power_request_gpios(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc = 0, i;
	/*
	 * gpio_name[] named as gpio_name + num(recomend as 0)
	 * because of the num will increase depend on number of gpio
	 */
	char gpio_name[17] = "disp_power_gpio0";
	u8 len = strlen(gpio_name) - 1;
	static u8 gpio_request_status = -EINVAL;

	if (!gpio_request_status)
		goto end;

	/* 15 of gpio_name is the index of "0" */
	for (i = 0; i < MAX_EXTRA_POWER_GPIO; i++, gpio_name[len]++) {
		if (gpio_is_valid(ctrl_pdata->panel_extra_power_gpio[i])) {
			rc = gpio_request(ctrl_pdata->panel_extra_power_gpio[i],
							gpio_name);
			if (rc) {
				pr_err("request %s failed, rc=%d\n", gpio_name, rc);
				goto extra_power_gpio_err;
			}
		}
	}

	gpio_request_status = rc;
end:
	return rc;
extra_power_gpio_err:
	if (i) {
		do {
			if (gpio_is_valid(ctrl_pdata->panel_extra_power_gpio[i]))
				gpio_free(ctrl_pdata->panel_extra_power_gpio[i--]);
			pr_err("%s : i = %d\n", __func__, i);
		} while (i > 0);
	}

	return rc;
}


static int mdss_dsi_extra_power(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int ret = 0, i = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
						panel_data);

	pr_info("%s: ++ enable(%d) ndx(%d)\n",
			__func__,enable, ctrl_pdata->ndx );

	if (ctrl_pdata->ndx == DSI_CTRL_1)
		return ret;

	if (mdss_dsi_extra_power_request_gpios(ctrl_pdata)) {
		pr_err("%s : fail to request extra power gpios", __func__);
		goto end;
	}

	pr_info("%s : %s extra power gpios\n", __func__, enable ? "enable" : "disable");

	do {
		if (gpio_is_valid(ctrl_pdata->panel_extra_power_gpio[i])) {
			gpio_set_value(ctrl_pdata->panel_extra_power_gpio[i], enable);
			pr_info("%s : set extra power gpio[%d] to %s\n",
						 __func__, ctrl_pdata->panel_extra_power_gpio[i],
						enable ? "high" : "low");
			usleep_range(5000, 5000);
		}
	} while (++i < MAX_EXTRA_POWER_GPIO);

end:
	pr_info("%s: --\n", __func__);
	return ret;
}

int mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	int i, rc = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (!gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
	}

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
		return rc;
	}

	pr_info("%s: enable = %d\n", __func__, enable);
	pinfo = &(ctrl_pdata->panel_data.panel_info);

	if (enable) {
		rc = mdss_dsi_request_gpios(ctrl_pdata);
		if (rc) {
			pr_err("gpio request failed\n");
			return rc;
		}
		if (pinfo->panel_power_on) {
			if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
				gpio_set_value((ctrl_pdata->disp_en_gpio), 1);

			for (i = 0; i < pdata->panel_info.rst_seq_len; ++i) {
				gpio_set_value((ctrl_pdata->rst_gpio),
					pdata->panel_info.rst_seq[i]);
				if (pdata->panel_info.rst_seq[++i])
					usleep(pinfo->rst_seq[i] * 1000);
			}

			if (gpio_is_valid(ctrl_pdata->bklt_en_gpio))
				gpio_set_value((ctrl_pdata->bklt_en_gpio), 1);
		}

		if (gpio_is_valid(ctrl_pdata->mode_gpio)) {
			if (pinfo->mode_gpio_state == MODE_GPIO_HIGH)
				gpio_set_value((ctrl_pdata->mode_gpio), 1);
			else if (pinfo->mode_gpio_state == MODE_GPIO_LOW)
				gpio_set_value((ctrl_pdata->mode_gpio), 0);
		}
		if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
			pr_debug("%s: Panel Not properly turned OFF\n",
						__func__);
			ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
			pr_debug("%s: Reset panel done\n", __func__);
		}
	} else {
		if (gpio_is_valid(ctrl_pdata->bklt_en_gpio))
			gpio_set_value((ctrl_pdata->bklt_en_gpio), 0);

		if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
			gpio_set_value((ctrl_pdata->disp_en_gpio), 0);

		if (gpio_is_valid(ctrl_pdata->rst_gpio))
			gpio_set_value((ctrl_pdata->rst_gpio), 0);

	}
	return rc;
}

static char caset[] = {0x2a, 0x00, 0x00, 0x03, 0x00};	/* DTYPE_DCS_LWRITE */
static char paset[] = {0x2b, 0x00, 0x00, 0x05, 0x00};	/* DTYPE_DCS_LWRITE */

static struct dsi_cmd_desc partial_update_enable_cmd[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(caset)}, caset},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(paset)}, paset},
};

static int mdss_dsi_panel_partial_update(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct dcs_cmd_req cmdreq;
	int rc = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	mipi  = &pdata->panel_info.mipi;

	pr_debug("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	caset[1] = (((pdata->panel_info.roi_x) & 0xFF00) >> 8);
	caset[2] = (((pdata->panel_info.roi_x) & 0xFF));
	caset[3] = (((pdata->panel_info.roi_x - 1 + pdata->panel_info.roi_w)
								& 0xFF00) >> 8);
	caset[4] = (((pdata->panel_info.roi_x - 1 + pdata->panel_info.roi_w)
								& 0xFF));
	partial_update_enable_cmd[0].payload = caset;

	paset[1] = (((pdata->panel_info.roi_y) & 0xFF00) >> 8);
	paset[2] = (((pdata->panel_info.roi_y) & 0xFF));
	paset[3] = (((pdata->panel_info.roi_y - 1 + pdata->panel_info.roi_h)
								& 0xFF00) >> 8);
	paset[4] = (((pdata->panel_info.roi_y - 1 + pdata->panel_info.roi_h)
								& 0xFF));
	partial_update_enable_cmd[1].payload = paset;

	pr_debug("%s: enabling partial update\n", __func__);
	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = partial_update_enable_cmd;
	cmdreq.cmds_cnt = 2;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);

	return rc;
}

static void mdss_dsi_panel_switch_mode(struct mdss_panel_data *pdata,
							int mode)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mipi_panel_info *mipi;
	struct dsi_panel_cmds *pcmds;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	mipi  = &pdata->panel_info.mipi;

	if (!mipi->dynamic_switch_enabled)
		return;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (mode == DSI_CMD_MODE)
		pcmds = &ctrl_pdata->video2cmd;
	else
		pcmds = &ctrl_pdata->cmd2video;

	mdss_dsi_panel_cmds_send(ctrl_pdata, pcmds);

	return;
}

static int get_candela_value(struct mdss_dsi_panel_map_table_list *map_table_list, int bl_level)
{
	return map_table_list->candela_map_table.lux_tab[map_table_list->candela_map_table.bkl[bl_level]];
}

static int get_cmd_idx(struct mdss_dsi_panel_map_table_list *map_table_list, int bl_level)
{
	return map_table_list->candela_map_table.cmd_idx[map_table_list->candela_map_table.bkl[bl_level]];
}

static struct dsi_panel_cmds get_testKey_set(struct mdss_panel_data *pdata, int enable)/*F0 or F0 F1*/
{
	struct dsi_panel_cmds testKey = {0,};
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (enable) {
		testKey.cmds = &ctrl->cmd_list.mtp_enable_cmd.cmds[0];
		testKey.cmd_cnt = ctrl->cmd_list.mtp_enable_cmd.cmd_cnt;
	} else {
		testKey.cmds = &ctrl->cmd_list.mtp_disable_cmd.cmds[0];
		testKey.cmd_cnt = ctrl->cmd_list.mtp_disable_cmd.cmd_cnt;
		testKey.cmds[0].dchdr.last = 1;
	}

	return testKey;
}

static int update_bright_packet(struct mdss_panel_data *pdata, int cmd_count, struct dsi_panel_cmds *cmd_set)
{
	int i = 0;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct dsi_panel_cmds *brightness_cmd = NULL;

	if (!pdata) {
		pr_err("%s : pdata is null\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	brightness_cmd = &ctrl->cmd_list.brightness_cmd;

	if (!brightness_cmd->cmds) {
		pr_err("%s : brightness_cmd is null\n", __func__);
		return -EINVAL;
	}

	if (cmd_count > (MAX_BR_PACKET_SIZE - 1))/*cmd_count is index, if cmd_count >16 then panic*/
		panic("over max brightness_packet size(%d).. !!", MAX_BR_PACKET_SIZE);

	for (i = 0; i < cmd_set->cmd_cnt; i++) {
		brightness_cmd->cmds[cmd_count].payload = \
			cmd_set->cmds[i].payload;
		brightness_cmd->cmds[cmd_count].dchdr.dlen = \
			cmd_set->cmds[i].dchdr.dlen;
		brightness_cmd->cmds[cmd_count].dchdr.dtype = \
		cmd_set->cmds[i].dchdr.dtype;
		brightness_cmd->cmds[cmd_count].dchdr.wait = \
		cmd_set->cmds[i].dchdr.wait;
		cmd_count++;
	}

#if defined(ALPM_MODE)
	alpm_cmd_debug(brightness_cmd->cmds[cmd_count].payload,
					brightness_cmd->cmds[cmd_count].dchdr.dlen);
#endif

	if (&cmd_set->cmds[0])
		brightness_cmd->cmds[cmd_count-1].dchdr.last = cmd_set->cmds[0].dchdr.last;

	return cmd_count;
}

static struct dsi_panel_cmds get_aid_aor_control_set(struct mdss_panel_data *pdata, int cd_idx)
{
	struct dsi_panel_cmds aid_control = {0,};
	struct dsi_panel_cmds *aid_cmd_list = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mipi_samsung_driver_data *s_pdata = NULL;
	int cmd_idx = 0, payload_size = 0, p_idx = 0;
	char *p_payload, *c_payload;

	if (!pdata) {
		pr_err("%s : pdatat is null\n", __func__);
		goto end;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	s_pdata = &pdata->samsung_pdata;

	aid_cmd_list = &ctrl->cmd_list.aid_cmd_list;

	p_idx = s_pdata->dstat.curr_aid_idx;

	if (!ctrl->map_table_list.aid_map_table.size
			|| !(cd_idx < ctrl->map_table_list.aid_map_table.size))
		goto end;

		/* Get index in the aid command list*/
		cmd_idx = ctrl->map_table_list.aid_map_table.cmd_idx[cd_idx];
		c_payload = aid_cmd_list->cmds[cmd_idx].payload;


		/* Check if current & previous commands are same */
		if (p_idx >= 0) {
			p_payload = aid_cmd_list->cmds[p_idx].payload;
			payload_size = aid_cmd_list->cmds[p_idx].dchdr.dlen;

			if (!memcmp(p_payload, c_payload, payload_size))
				goto end;
		}

	/* Get the command desc */
	aid_control.cmds = &(aid_cmd_list->cmds[cmd_idx]);

	aid_control.cmd_cnt = 1;
	s_pdata->dstat.curr_aid_idx = cmd_idx;

end:
	return aid_control;
}

/*
	This function takes acl_map_table and uses cd_idx,
	to get the index of the command in elvss command list.

*/

static struct dsi_panel_cmds get_acl_control_on_set(struct mdss_panel_data *pdata)
{
	struct dsi_panel_cmds aclcont_control = {0,};
	struct dsi_panel_cmds *aclcont_cmd_list = NULL;
	struct mipi_samsung_driver_data *s_pdata = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	int acl_cond = 0;

	if (!pdata) {
		pr_err("%s : pdata is null\n", __func__);
		goto end;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	s_pdata = &pdata->samsung_pdata;

	acl_cond = s_pdata->dstat.curr_acl_cond;

	aclcont_cmd_list = &ctrl->cmd_list.aclcont_cmd_list;

	if (acl_cond) /* already acl condition setted */
		goto end;

	/* Get the command desc */
	aclcont_control.cmds = aclcont_cmd_list->cmds;
	aclcont_control.cmd_cnt = aclcont_cmd_list->cmd_cnt;
	s_pdata->dstat.curr_acl_cond = 1;

	pr_info("%s #(%d)\n",
				__func__, aclcont_cmd_list->cmd_cnt);

end:
	return aclcont_control;
}

/*
	This function takes acl_map_table and uses cd_idx,
	to get the index of the command in elvss command list.

*/
static struct dsi_panel_cmds get_acl_control_set(struct mdss_panel_data *pdata, int cd_idx)
{
	struct dsi_panel_cmds acl_control = {0,};
	struct dsi_panel_cmds *acl_cmd_list = NULL;
	struct mipi_samsung_driver_data *s_pdata = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	int cmd_idx = 0, payload_size = 0, p_idx = 0;
	char *p_payload, *c_payload;

	if (!pdata) {
		pr_err("%s : pdata is null\n", __func__);
		goto end;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	s_pdata = &pdata->samsung_pdata;

	acl_cmd_list = &ctrl->cmd_list.acl_cmd_list;

	p_idx = s_pdata->dstat.curr_acl_idx;

	if (!ctrl->map_table_list.acl_map_table.size
			|| !(cd_idx < ctrl->map_table_list.acl_map_table.size))
		goto end;

	/* Get index in the acl command list*/
	cmd_idx = ctrl->map_table_list.acl_map_table.cmd_idx[cd_idx];
	c_payload = acl_cmd_list->cmds[cmd_idx].payload;

	/* Check if current & previous commands are same */
	if (p_idx >= 0) {
		p_payload = acl_cmd_list->cmds[p_idx].payload;
		payload_size = acl_cmd_list->cmds[p_idx].dchdr.dlen;
		if (!memcmp(p_payload, c_payload, payload_size))
			goto end;
	}

	/* Get the command desc */
	acl_control.cmds = &(acl_cmd_list->cmds[cmd_idx]);
	acl_control.cmd_cnt = 1;
	s_pdata->dstat.curr_acl_idx = cmd_idx;

end:
	return acl_control;
}

static struct dsi_panel_cmds get_acl_control_off_set(struct mdss_panel_data *pdata)
{
	struct dsi_panel_cmds acl_control = {0,};
	struct dsi_panel_cmds *acl_off_cmd = NULL;
	struct mipi_samsung_driver_data *s_pdata = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	int p_idx = 0;

	if (!pdata) {
		pr_err("%s : pdata is null\n", __func__);
		goto end;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	s_pdata = &pdata->samsung_pdata;

	acl_off_cmd = &ctrl->cmd_list.acl_off_cmd;

	p_idx = s_pdata->dstat.curr_acl_idx;

	/* Check if current & previous commands are same */
	if (p_idx == 0) {
		/* already acl off */
		goto end;
	}

	/* Get the command desc */
	acl_control.cmds = acl_off_cmd->cmds; /* idx 0 : ACL OFF */
	acl_control.cmd_cnt = acl_off_cmd->cmd_cnt;

	pr_info("%s #(%d)\n",
				__func__, acl_off_cmd->cmd_cnt);

	s_pdata->dstat.curr_acl_idx = 0;
	s_pdata->dstat.curr_acl_cond = 0;

end:
	return acl_control;
}

#ifdef SMART_ACL
static struct dsi_panel_cmds get_elvss_control_set(struct mdss_panel_data *pdata, int cd_idx)
{
	struct dsi_panel_cmds elvss_control = {0,};
	struct dsi_panel_cmds *smart_acl_elvss_cmd_list = NULL;
	struct dsi_panel_cmds *elvss_cmd_list = NULL;
	struct mipi_samsung_driver_data *s_pdata = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	int cmd_idx = 0;
	char *payload;

	if (!pdata) {
		pr_err("%s : pdata is null\n", __func__);
		goto end;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	s_pdata = &pdata->samsung_pdata;

	elvss_cmd_list = &ctrl->cmd_list.elvss_cmd_list;
	smart_acl_elvss_cmd_list = &ctrl->cmd_list.smart_acl_elvss_cmd_list;

	pr_debug("%s for SMART_ACL acl(%d), temp(%d)\n",
			__func__, s_pdata->dstat.acl_on, s_pdata->dstat.temperature);

	if (!ctrl->map_table_list.smart_acl_elvss_map_table.size
			|| !(cd_idx < ctrl->map_table_list.smart_acl_elvss_map_table.size) ||
			!ctrl->map_table_list.smart_acl_elvss_map_table.size ||
			!(cd_idx < ctrl->map_table_list.smart_acl_elvss_map_table.size)) {
		pr_err("%s failed mapping elvss table\n",__func__);
		goto end;
	}

	cmd_idx = ctrl->map_table_list.smart_acl_elvss_map_table.cmd_idx[cd_idx];

	/* Get the command desc */
	if(s_pdata->dstat.acl_on || s_pdata->dstat.siop_status) {
		if (s_pdata->dstat.temperature > 0)
			smart_acl_elvss_cmd_list->cmds[cmd_idx].payload[1] = 0x88;
		else
			smart_acl_elvss_cmd_list->cmds[cmd_idx].payload[1] = 0x8C;
		payload = smart_acl_elvss_cmd_list->cmds[cmd_idx].payload;
		elvss_control.cmds = &(smart_acl_elvss_cmd_list->cmds[cmd_idx]);

		pr_debug("ELVSS for SMART_ACL cd_idx=%d, cmd_idx=%d\n", cd_idx, cmd_idx);
	} else {
		if (s_pdata->dstat.temperature > 0)
			elvss_cmd_list->cmds[cmd_idx].payload[1] = 0x98;
		else
			elvss_cmd_list->cmds[cmd_idx].payload[1] = 0x9C;
		payload = elvss_cmd_list->cmds[cmd_idx].payload;
		elvss_control.cmds = &(elvss_cmd_list->cmds[cmd_idx]);

		pr_debug("ELVSS for normal cd_idx=%d, cmd_idx=%d\n", cd_idx, cmd_idx);
	}

	elvss_control.cmd_cnt = 1;
	s_pdata->dstat.curr_elvss_idx = cmd_idx;

end:
	return elvss_control;

}
#endif

static struct dsi_panel_cmds get_gamma_control_set(struct mdss_panel_data *pdata, int candella)
{
	struct dsi_panel_cmds gamma_control = {0,};
	struct dsi_panel_cmds *gamma_cmd_list = NULL;
	struct mipi_samsung_driver_data *s_pdata = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	if (!pdata) {
		pr_err("%s : pdata is null\n", __func__);
		goto end;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	s_pdata = &pdata->samsung_pdata;

	gamma_cmd_list = &ctrl->cmd_list.gamma_cmd_list;

	/* Just a safety check to ensure smart dimming data is initialised well */
	BUG_ON(s_pdata->sdimconf->generate_gamma == NULL);
	s_pdata->sdimconf->generate_gamma(candella, &gamma_cmd_list->cmds[1].payload[1]);

	gamma_control.cmds = &(gamma_cmd_list->cmds[0]);
	gamma_control.cmd_cnt = gamma_cmd_list->cmd_cnt;

end:
	return gamma_control;
}


static int make_brightcontrol_set(struct mdss_panel_data *pdata, int bl_level)
{
	struct dsi_panel_cmds aid_control = {0,};
	struct dsi_panel_cmds acl_control = {0,};
	struct dsi_panel_cmds acl_on_cont = {0,};
	struct dsi_panel_cmds acl_off_cont = {0,};
	struct dsi_panel_cmds elvss_control = {0,};

#if defined(HBM_RE)
	struct dsi_panel_cmds hbm_off_control = {0,};
#endif
	struct dsi_panel_cmds gamma_control = {0,};
	struct dsi_panel_cmds testKey = {0,};
#if defined(TEMPERATURE_ELVSS)
	struct dsi_panel_cmds temperature_elvss_control = {0,};
	struct dsi_panel_cmds temperature_elvss_control2 = {0,};
#endif
	struct mipi_samsung_driver_data *s_pdata = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int cmd_count = 0, cd_idx = 0, cd_level =0;

	if (!pdata) {
		pr_err("%s : pdata is null\n", __func__);
		return -EINVAL;
	}
	
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	s_pdata = &pdata->samsung_pdata;

	cd_idx = get_cmd_idx(&ctrl_pdata->map_table_list, bl_level);
	cd_level = get_candela_value(&ctrl_pdata->map_table_list, bl_level);

	testKey = get_testKey_set(pdata, 1);
	cmd_count = update_bright_packet(pdata, cmd_count, &testKey);

	aid_control = get_aid_aor_control_set(pdata, cd_idx);

	cmd_count = update_bright_packet(pdata, cmd_count, &aid_control);

	/* acl */
	if (s_pdata->dstat.acl_on || s_pdata->dstat.siop_status) {
		acl_on_cont = get_acl_control_on_set(pdata); /*b5 51/29*/
		cmd_count = update_bright_packet(pdata, cmd_count, &acl_on_cont);
		acl_control = get_acl_control_set(pdata, cd_idx); /*55 02*/
		cmd_count = update_bright_packet(pdata, cmd_count, &acl_control);
	} else {
		/* acl off */
		acl_off_cont = get_acl_control_off_set(pdata); /*b5 41,55 00 */
		cmd_count = update_bright_packet(pdata, cmd_count, &acl_off_cont);
	}

#if defined(HBM_RE)
	if ((msd.panel == PANEL_HD_OCTA_EA8064G_CMD)||(msd.panel == PANEL_FHD_OCTA_EA8064G_CMD)) {
		/*hbm off cmd*/
		if(s_pdata->dstat.hbm_mode == 1){
			hbm_off_control = get_hbm_off_set();/*53 00*/
			cmd_count = update_bright_packet(pdata, cmd_count, &hbm_off_control);
		}
	}
#endif

#ifdef SMART_ACL
	/*elvss*/
	elvss_control = get_elvss_control_set(pdata, cd_idx);
	cmd_count = update_bright_packet(pdata, cmd_count, &elvss_control);
#endif

#if defined(TEMPERATURE_ELVSS)
	/* ELVSS TEMPERATURE COMPENSATION*/
	/* ELVSS for Temperature set cmd should be sent after normal elvss set cmd*/
	if (s_pdata->dstat.elvss_need_update) {
		temperature_elvss_control = get_elvss_tempcompen_control_set();
		cmd_count = update_bright_packet(pdata, cmd_count, &temperature_elvss_control);

	if (((msd.panel == PANEL_FHD_OCTA_S6E3FA2_CMD)  && (msd.id3 >= EVT0_K_fhd_REVG)) || \
           (msd.panel == PANEL_FHD_OCTA_EA8064G_CMD) ) {
		temperature_elvss_control2 = get_elvss_tempcompen_control_set2();
		cmd_count = update_bright_packet(pdata, cmd_count, &temperature_elvss_control2);
	}
		s_pdata->dstat.elvss_need_update = 0;
	}
#endif

	/*gamma*/
	gamma_control = get_gamma_control_set(pdata, cd_level);
	cmd_count = update_bright_packet(pdata, cmd_count, &gamma_control);

	testKey = get_testKey_set(pdata, 0);
	cmd_count = update_bright_packet(pdata, cmd_count, &testKey);

#if defined(TEMPERATURE_ELVSS)
	LCD_DEBUG("bright_level: %d, candela_idx: %d( %d cd ), "\
		"cmd_count(aid,acl,elvss,temperature,gamma)::(%d,%d,%d,%d,%d)%d,id3(0x%x)\n",
#else
	LCD_DEBUG("bright_level: %d, candela_idx: %d( %d cd ), "\
		"cmd_count(aid,acl,elvss,temperature,gamma)::(%d,%d,%d,%d)%d,id3(0x%x)\n",
#endif
		s_pdata->dstat.bright_level, cd_idx, cd_level,
		aid_control.cmd_cnt,
		s_pdata->dstat.acl_on | s_pdata->dstat.siop_status,
		elvss_control.cmd_cnt,
#if defined(TEMPERATURE_ELVSS)
		temperature_elvss_control.cmd_cnt,
#endif
		gamma_control.cmd_cnt,
		cmd_count,
		s_pdata->manufacture_id & 0xFF);
	return cmd_count;

}


static void mdss_dsi_panel_bl_ctrl(struct mdss_panel_data *pdata,
							u32 bl_level)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mipi_samsung_driver_data *s_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	s_pdata = &pdata->samsung_pdata;

	if (s_pdata->mfd->resume_state != MIPI_RESUME_STATE) {
		pr_err("%s : panel is not resume state\n", __func__);
		return;
	}

	if (unlikely(!s_pdata->dstat.is_smart_dim_loaded))
		ctrl_pdata->dimming_init(pdata);

	/*
	 * Some backlight controllers specify a minimum duty cycle
	 * for the backlight brightness. If the brightness is less
	 * than it, the controller can malfunction.
	 */

	mipi_samsung_disp_send_cmd(pdata, PANEL_MTP_ENABLE, true);
	if ((bl_level < pdata->panel_info.bl_min) && (bl_level != 0))
		bl_level = pdata->panel_info.bl_min;

	switch (ctrl_pdata->bklt_ctrl) {
	case BL_WLED:
		led_trigger_event(bl_led_trigger, bl_level);
		break;
	case BL_PWM:
		mdss_dsi_panel_bklt_pwm(ctrl_pdata, bl_level);
		break;
	case BL_DCS_CMD:
		s_pdata->dstat.bright_level = bl_level;
		mipi_samsung_disp_send_cmd(pdata, PANEL_BRIGHT_CTRL, true);
		break;
		mdss_dsi_panel_bklt_dcs(ctrl_pdata, bl_level);
		if (mdss_dsi_is_master_ctrl(ctrl_pdata)) {
			struct mdss_dsi_ctrl_pdata *sctrl =
				mdss_dsi_get_slave_ctrl();
			if (!sctrl) {
				pr_err("%s: Invalid slave ctrl data\n",
					__func__);
				return;
			}
			mdss_dsi_panel_bklt_dcs(sctrl, bl_level);
		}
		break;
	default:
		pr_err("%s: Unknown bl_ctrl configuration\n",
			__func__);
		break;
	}
	mipi_samsung_disp_send_cmd(pdata, PANEL_MTP_DISABLE, true);
}

#ifdef SMART_DIMMING
#ifdef SAMSUNG_DISP_SEND_CMD
int mipi_samsung_disp_send_cmd(
		struct mdss_panel_data *pdata,
		enum mipi_samsung_cmd_list cmd,
		unsigned char lock)
{
	struct mipi_samsung_driver_data *s_pdata;
	struct dsi_panel_cmds pcmds;
	int flag = 0;
#ifdef CMD_DEBUG
	int i,j;
#endif
	struct mdss_dsi_ctrl_pdata *ctrl;



	if (!pdata) {
		pr_err("%s : pdata is null\n", __func__);
		return -EINVAL;
	}

	s_pdata = &pdata->samsung_pdata;

	if (!s_pdata) {
		pr_err("%s : s_pdata is null\n", __func__);
		return -EINVAL;
	}


	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (!ctrl) {
		pr_err("%s : ctrl is null\n", __func__);
		return -EINVAL;
	}

	if (lock)
		mutex_lock(&s_pdata->lock);

	switch (cmd) {
		case PANEL_DISPLAY_ON_SEQ:
			pcmds.cmds = ctrl->cmd_list.disp_on_seq.cmds;
			pcmds.cmd_cnt = ctrl->cmd_list.disp_on_seq.cmd_cnt;
			break;
		case PANEL_DISPLAY_OFF_SEQ:
			pcmds.cmds = ctrl->cmd_list.disp_off_seq.cmds;
			pcmds.cmd_cnt = ctrl->cmd_list.disp_off_seq.cmd_cnt;
			break;
		case PANEL_DISPLAY_ON:
			pcmds.cmds = ctrl->cmd_list.disp_on_cmd.cmds;
			pcmds.cmd_cnt = ctrl->cmd_list.disp_on_cmd.cmd_cnt;
			break;
		case PANEL_DISPLAY_OFF:
			pcmds.cmds = ctrl->cmd_list.disp_off_cmd.cmds;
			pcmds.cmd_cnt = ctrl->cmd_list.disp_off_cmd.cmd_cnt;
			break;
		case PANEL_HSYNC_ON:
			pcmds.cmds = ctrl->cmd_list.hsync_on_seq.cmds;
			pcmds.cmd_cnt = ctrl->cmd_list.hsync_on_seq.cmd_cnt;
			break;
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_WQHD_PT_PANEL)
		case PANEL_SET_TE_OSC_B:
			pcmds.cmds = panel_set_te_osc_b.cmds;
			pcmds.cmd_cnt = panel_set_te_osc_b.cmd_cnt;
			break;
		case PANEL_SET_TE_RESTORE:
			pcmds.cmds = panel_set_te_restore.cmds;
			pcmds.cmd_cnt = panel_set_te_restore.cmd_cnt;
			break;
		case PANEL_SET_TE:
			pcmds.cmds = panel_set_te.cmds;
			pcmds.cmd_cnt = panel_set_te.cmd_cnt;
			break;
		case PANEL_SET_TE_1:
			pcmds.cmds = panel_set_te_1.cmds;
			pcmds.cmd_cnt = panel_set_te_1.cmd_cnt;
			break;
		case PANEL_SET_TE_2:
			pcmds.cmds = panel_set_te_2.cmds;
			pcmds.cmd_cnt = panel_set_te_2.cmd_cnt;
			break;
#endif
		case PANEL_BRIGHT_CTRL:
			/* TODO: remove "break;", if the smart dimming working fine */
			break;
			pcmds.cmds = brightness_packet;

			/* Single Tx use for DSI_VIDEO_MODE Only */
			if(pdata->panel_info.mipi.mode == DSI_VIDEO_MODE)
				flag = CMD_REQ_SINGLE_TX;
			else
				flag = 0;

			if (s_pdata->dstat.bright_level)
				s_pdata->dstat.recent_bright_level = s_pdata->dstat.bright_level;
			pcmds.cmd_cnt = make_brightcontrol_set(pdata, s_pdata->dstat.bright_level);

			if (s_pdata->mfd->resume_state != MIPI_RESUME_STATE) {
				pr_info("%s : panel is off state!!\n", __func__);
				goto unknown_command;
			}

			break;
		case PANEL_MTP_ENABLE:
			pcmds.cmds = ctrl->cmd_list.mtp_enable_cmd.cmds;
			pcmds.cmd_cnt = ctrl->cmd_list.mtp_enable_cmd.cmd_cnt;
			break;
		case PANEL_MTP_DISABLE:
			pcmds.cmds = ctrl->cmd_list.mtp_disable_cmd.cmds;
			pcmds.cmd_cnt = ctrl->cmd_list.mtp_disable_cmd.cmd_cnt;
			break;
		case PANEL_NV_MTP_READ_REGISTER_SET_CMDS:
			pcmds.cmds = ctrl->cmd_list.nv_mtp_register_set_cmds.cmds;
			pcmds.cmd_cnt = ctrl->cmd_list.nv_mtp_register_set_cmds.cmd_cnt;
			break;
		case PANEL_MANUFACTURE_ID_REGISTER_SET_CMDS:
			pcmds.cmds = ctrl->cmd_list.panel_manufacture_id_register_set_cmds.cmds;
			pcmds.cmd_cnt = ctrl->cmd_list.panel_manufacture_id_register_set_cmds.cmd_cnt;
			break;
		case PANEL_NEED_FLIP:
			/*
				May be required by Panel Like Fusion3
			*/
			break;
		case PANEL_ACL_ON:
			/*
				May be required by panel like D2,Commanche
			*/
			break;
		case PANEL_ACL_OFF:
			pcmds.cmds = ctrl->cmd_list.acl_off_cmd.cmds;
			pcmds.cmd_cnt = ctrl->cmd_list.acl_off_cmd.cmd_cnt;
			break;
#if defined(PARTIAL_UPDATE)
		case PANEL_PARTIAL_ON:
			pcmds.cmds = ctrl->cmd_list.partialdisp_on_cmd.cmds;
			pcmds.cmd_cnt = ctrl->cmd_list.partialdisp_on_cmd.cmd_cnt;
			break;
		case PANEL_PARTIAL_OFF:
			pcmds.cmds = ctrl->cmd_list.partialdisp_off_cmd.cmds;
			pcmds.cmd_cnt = ctrl->cmd_list.partialdisp_off_cmd.cmd_cnt;
			break;
#endif
#if defined(CONFIG_LCD_CLASS_DEVICE) && defined(DDI_VIDEO_ENHANCE_TUNING)
		case MDNIE_ADB_TEST:
			pcmds.cmds = mdni_tune_cmd;
			pcmds.cmd_cnt = ARRAY_SIZE(mdni_tune_cmd);
			break;
#endif
		default:
			pr_err("%s : unknown_command.. \n", __func__);
			goto unknown_command;
			break;
	}

	if (!pcmds.cmd_cnt) {
		pr_err("%s : cmd_size is zero!.. \n", __func__);
		goto err;
	}

#ifdef CMD_DEBUG
	for (i = 0; i < pcmds.cmd_cnt; i++) {
		for (j = 0; j < pcmds.cmds[i].dchdr.dlen; j++)
			printk("%x ",pcmds.cmds[i].payload[j]);
		printk("\n");
	}
#endif

#ifdef MDP_RECOVERY
	if (!mdss_recovery_start)
		mdss_dsi_cmds_send(ctrl, cmd_desc, cmd_size, flag);
	else
		pr_err ("%s : Can't send command during mdss_recovery_start\n", __func__);
#else
	mdss_dsi_panel_cmds_send(ctrl, &pcmds);
#endif

	if (lock)
		mutex_unlock(&s_pdata->lock);

	pr_debug("%s done..\n", __func__);

	return 0;

unknown_command:
	LCD_DEBUG("Undefined command\n");

err:
	if (lock)
		mutex_unlock(&s_pdata->lock);

	return -EINVAL;
}
#endif

static int samsung_nv_read(struct dsi_cmd_desc *desc, char *destBuffer,
		int srcLength, struct mdss_panel_data *pdata, int startoffset)
{
	int loop_limit = 0;
	struct dsi_panel_cmds pcmds1;
	struct dsi_panel_cmds pcmds2;
	/* first byte is size of Register */
	static char packet_size[] = { 0x07, 0 };
	static struct dsi_cmd_desc s6e8aa0_packet_size_cmd = {
		{DTYPE_MAX_PKTSIZE, 1, 0, 0, 0, sizeof(packet_size)},
		packet_size };

	/* second byte is Read-position */
	static char reg_read_pos[] = { 0xB0, 0x00 };
	static struct dsi_cmd_desc s6e8aa0_read_pos_cmd = {
		{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(reg_read_pos)},
		reg_read_pos };

	int read_pos = startoffset;
	int read_count = 0;
	int show_cnt;
	int i, j;
	char show_buffer[256];
	int show_buffer_pos = 0;
	int read_size = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pcmds1.blen = 9;
	pcmds1.cmds = &s6e8aa0_packet_size_cmd;
	pcmds1.cmd_cnt = 1;

	pcmds2.blen = 9;
	pcmds2.cmds = &s6e8aa0_read_pos_cmd;
	pcmds2.cmd_cnt = 1;

	show_buffer_pos +=
		snprintf(show_buffer, 256, "read_reg : %X[%d] : ",
		desc[0].payload[0], srcLength);

	loop_limit = (srcLength + packet_size[0] - 1)
				/ packet_size[0];
	mdss_dsi_panel_cmds_send(ctrl_pdata, &pcmds1);

	show_cnt = 0;
	for (j = 0; j < loop_limit; j++) {
		reg_read_pos[1] = read_pos;
		read_size = ((srcLength - read_pos + startoffset) < packet_size[0]) ?
					(srcLength - read_pos + startoffset) : packet_size[0];
		mdss_dsi_panel_cmds_send(ctrl_pdata, &pcmds2);

		read_count = mdss_dsi_cmd_receive(ctrl_pdata, desc, read_size);

		for (i = 0; i < read_count; i++, show_cnt++) {
			show_buffer_pos += snprintf(show_buffer +
						show_buffer_pos, (256 - show_buffer_pos), "%02x ",
						ctrl_pdata->rx_buf.data[i]);
			if (destBuffer != NULL && show_cnt < srcLength) {
					destBuffer[show_cnt] =
					ctrl_pdata->rx_buf.data[i];
			}
		}
		show_buffer_pos += snprintf(show_buffer +
				show_buffer_pos, (256 - show_buffer_pos), ".");
		read_pos += read_count;

		if (read_pos-startoffset >= srcLength)
			break;
	}

	return read_pos-startoffset;
}

static int mipi_samsung_read_nv_mem(struct mdss_panel_data *pdata, struct dsi_panel_cmds *nv_read_cmds, char *buffer)
{
	int nv_size = 0;
	int nv_read_cnt = 0;
	int i = 0, j = 0;

	mipi_samsung_disp_send_cmd(pdata, PANEL_NV_MTP_READ_REGISTER_SET_CMDS, true);
	mipi_samsung_disp_send_cmd(pdata, PANEL_MTP_ENABLE, true);

	for (i = 0; i < nv_read_cmds->cmd_cnt; i++)
		nv_size += nv_read_cmds->read_size[i];

	pr_debug("nv_size= %d, nv_read_cmds->num_of_cmds = %d\n", nv_size, nv_read_cmds->cmd_cnt);

	for (i = 0; i < nv_read_cmds->cmd_cnt; i++) {
		int count = 0;
		int read_size = nv_read_cmds->read_size[i];
		int read_startoffset = nv_read_cmds->read_startoffset[i];

		do {
			count = samsung_nv_read(&(nv_read_cmds->cmds[i]),
					&buffer[nv_read_cnt], read_size, pdata, read_startoffset);

			j++;
		} while(j <= 2);

		nv_read_cnt += count;
		if (count != read_size)
			pr_err("Error reading LCD NV data count(%d), read_size(%d)!!!!\n",count,read_size);
	}

	mipi_samsung_disp_send_cmd(pdata, PANEL_MTP_DISABLE, true);

	return nv_read_cnt;
}


static int mdss_dsi_panel_dimming_init(struct mdss_panel_data *pdata)
{
	struct mipi_samsung_driver_data *s_pdata = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (!pdata) {
		pr_err("%s : pdata is null ", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (!ctrl_pdata) {
		pr_err("%s : ctrl_pdata is null ", __func__);
		return -EINVAL;
	}

	s_pdata = &pdata->samsung_pdata;

	if (!s_pdata) {
		pr_err("%s : s_pdata is null ", __func__);
		return -EINVAL;
	}

	pr_info("%s : ++\n",__func__);

	s_pdata->sdimconf = smart_EA8061_get_conf();

	/* Just a safety check to ensure smart dimming data is initialised well */
	BUG_ON(s_pdata->sdimconf == NULL);

	/* Set the mtp read buffer pointer and read the NVM value*/
	mipi_samsung_read_nv_mem(pdata, &ctrl_pdata->cmd_list.nv_mtp_read_cmds,
			s_pdata->sdimconf->mtp_buffer);

	/* Initialize smart dimming related things here */
	/* lux_tab setting for 350cd */
	s_pdata->sdimconf->lux_tab =
		&ctrl_pdata->map_table_list.candela_map_table.lux_tab[0];
	s_pdata->sdimconf->lux_tabsize =
		ctrl_pdata->map_table_list.candela_map_table.lux_tab_size;
	s_pdata->sdimconf->man_id = s_pdata->manufacture_id;

	/* Just a safety check to ensure smart dimming data is initialised well */
	BUG_ON(s_pdata->sdimconf->init == NULL);
	s_pdata->sdimconf->init();

	s_pdata->dstat.temperature = 20; /* default temperature */
	s_pdata->dstat.elvss_need_update = 1;

	s_pdata->dstat.is_smart_dim_loaded = true;

	pr_info("%s : --\n",__func__);

	return 0;
}
#endif

static int panel_event_handler(struct mdss_panel_data *pdata, int event)
{
	struct mipi_samsung_driver_data *s_pdata = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_info *pinfo;

	if (!pdata) {
		pr_err("%s : pdata is null\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	s_pdata = &pdata->samsung_pdata;

	pinfo = &pdata->panel_info;

	pr_debug("%s : %d",__func__,event);
	switch (event) {
		case MDSS_EVENT_FRAME_UPDATE:
			if(s_pdata->dstat.wait_disp_on) {
				pr_info("DISPLAY_ON\n");
				if (pinfo->mipi.mode == DSI_CMD_MODE)
					mipi_samsung_disp_send_cmd(pdata, PANEL_DISPLAY_ON, true);
				mdss_dsi_cmd_receive(ctrl, ctrl->cmd_list.rddpm_cmd.cmds, ctrl->cmd_list.rddpm_cmd.cmd_cnt);
				s_pdata->dstat.wait_disp_on = 0;

#ifdef CONFIG_PANEL_RECOVERY
				if (rddpm_buf != 0x9c) {
					char *envp[2] = {"PANEL_ALIVE=0", NULL};
					struct device *dev = msd.mfd->fbi->dev;

					kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
					pr_err("Panel has gone bad, sending uevent - %s\n", envp[0]);
				}
#endif
			}
			break;
#if defined(CONFIG_MDNIE_LITE_TUNING)
		case MDSS_EVENT_MDNIE_DEFAULT_UPDATE:
			pr_info("%s : send CONFIG_MDNIE_LITE_TUNING... \n",__func__);
			is_negative_on();
			break;
#endif
		default:
			pr_err("%s : unknown event (%d)\n", __func__, event);
			break;
	}

	return 0;
}

static int mdss_dsi_panel_registered(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mipi_samsung_driver_data *s_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;

	if (pdata == NULL) {
			pr_err("%s: Invalid input data\n", __func__);
			return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	s_pdata = &pdata->samsung_pdata;

	pinfo = &pdata->panel_info;

	s_pdata->mfd = (struct msm_fb_data_type *)registered_fb[0]->par;
	s_pdata->pdata = pdata;
	s_pdata->ctrl_pdata = ctrl_pdata;

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_WQHD_PT_PANEL)
	te_set_done = TE_SET_INIT;
#endif

	if(!s_pdata->mfd)
	{
		pr_info("%s mds.mfd is null!!\n",__func__);
	} else
		pr_info("%s mds.mfd is ok!!\n",__func__);
#if defined(CONFIG_MDNIE_LITE_TUNING)
	pr_info("[%s] CONFIG_MDNIE_LITE_TUNING ok ! mdnie_lite_tuning_init called!\n",
		__func__);
	mdnie_lite_tuning_init(&s_pdata);
#endif
	/* Set the initial state to Suspend until it is switched on */
	if (pinfo->cont_splash_enabled)
		s_pdata->mfd->resume_state = MIPI_RESUME_STATE;
	else
		s_pdata->mfd->resume_state = MIPI_SUSPEND_STATE;
	pr_info("%s:%d, Panel registered succesfully\n", __func__, __LINE__);
	return 0;
}


static int mdss_dsi_panel_on(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mipi_samsung_driver_data *s_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	s_pdata = &pdata->samsung_pdata;

	mipi  = &pdata->panel_info.mipi;

	pr_info("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	/* Read DDI manufacture id */
	if (!s_pdata->manufacture_id)
		mipi_samsung_manufacture_id(pdata);

	/* Init smart dimming */
	if (unlikely(!s_pdata->dstat.is_smart_dim_loaded))
		ctrl->dimming_init(pdata);

	/* Hsync on */
	mipi_samsung_disp_send_cmd(pdata, PANEL_HSYNC_ON, true);

	mipi_samsung_disp_send_cmd(pdata, PANEL_DISPLAY_ON_SEQ, true);
	mipi_samsung_disp_send_cmd(pdata, PANEL_DISPLAY_ON, true);

	/* Init status and index Values */
	s_pdata->dstat.curr_elvss_idx = -1;
	s_pdata->dstat.curr_acl_idx = -1;
	s_pdata->dstat.curr_opr_idx = -1;
	s_pdata->dstat.curr_aid_idx = -1;
	s_pdata->dstat.on = 1;
	s_pdata->dstat.wait_disp_on = 1;
	/*default acl off(caps on :b5 41) in on seq. */
	s_pdata->dstat.curr_acl_idx = 0;
	s_pdata->dstat.curr_acl_cond = 0;

	s_pdata->mfd->resume_state = MIPI_RESUME_STATE;

	pr_info("%s:-\n", __func__);
	return 0;
}

static int mdss_dsi_panel_off(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mipi_samsung_driver_data *s_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	s_pdata = &pdata->samsung_pdata;

	pr_err("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	mipi  = &pdata->panel_info.mipi;

	if (ctrl->off_cmds.cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->off_cmds);

	mipi_samsung_disp_send_cmd(pdata, PANEL_DISPLAY_OFF_SEQ, true);

	s_pdata->dstat.on = 0;
	s_pdata->mfd->resume_state = MIPI_SUSPEND_STATE;

	pr_err("%s:-\n", __func__);
	return 0;
}

static void mdss_dsi_parse_lane_swap(struct device_node *np, char *dlane_swap)
{
	const char *data;

	*dlane_swap = DSI_LANE_MAP_0123;
	data = of_get_property(np, "qcom,mdss-dsi-lane-map", NULL);
	if (data) {
		if (!strcmp(data, "lane_map_3012"))
			*dlane_swap = DSI_LANE_MAP_3012;
		else if (!strcmp(data, "lane_map_2301"))
			*dlane_swap = DSI_LANE_MAP_2301;
		else if (!strcmp(data, "lane_map_1230"))
			*dlane_swap = DSI_LANE_MAP_1230;
		else if (!strcmp(data, "lane_map_0321"))
			*dlane_swap = DSI_LANE_MAP_0321;
		else if (!strcmp(data, "lane_map_1032"))
			*dlane_swap = DSI_LANE_MAP_1032;
		else if (!strcmp(data, "lane_map_2103"))
			*dlane_swap = DSI_LANE_MAP_2103;
		else if (!strcmp(data, "lane_map_3210"))
			*dlane_swap = DSI_LANE_MAP_3210;
	}
}

static void mdss_dsi_parse_trigger(struct device_node *np, char *trigger,
		char *trigger_key)
{
	const char *data;

	*trigger = DSI_CMD_TRIGGER_SW;
	data = of_get_property(np, trigger_key, NULL);
	if (data) {
		if (!strcmp(data, "none"))
			*trigger = DSI_CMD_TRIGGER_NONE;
		else if (!strcmp(data, "trigger_te"))
			*trigger = DSI_CMD_TRIGGER_TE;
		else if (!strcmp(data, "trigger_sw_seof"))
			*trigger = DSI_CMD_TRIGGER_SW_SEOF;
		else if (!strcmp(data, "trigger_sw_te"))
			*trigger = DSI_CMD_TRIGGER_SW_TE;
	}
}
#ifdef SMART_DIMMING
static int mdss_samsung_parse_candella_lux_mapping_table(struct device_node *np,
		struct candella_lux_map *table, char *keystring)
{
		const __be32 *data;
		int  data_offset, len = 0 , i = 0;
		int  cdmap_start=0, cdmap_end=0;

		data = of_get_property(np, keystring, &len);
		if (!data) {
			pr_err("%s:%d, Unable to read table %s ",
				__func__, __LINE__, keystring);
			return -EINVAL;
		}
		if ((len % 4) != 0) {
			pr_err("%s:%d, Incorrect table entries for %s",
						__func__, __LINE__, keystring);
			return -EINVAL;
		}
		table->lux_tab_size = len / (sizeof(int)*4);
		table->lux_tab = kzalloc((sizeof(int) * table->lux_tab_size), GFP_KERNEL);
		if (!table->lux_tab)
			return -ENOMEM;

		table->cmd_idx = kzalloc((sizeof(int) * table->lux_tab_size), GFP_KERNEL);
		if (!table->cmd_idx)
			goto error;

		data_offset = 0;
		for (i = 0 ; i < table->lux_tab_size; i++) {
			table->cmd_idx[i]= be32_to_cpup(&data[data_offset++]);	/* 1rst field => <idx> */
			cdmap_start = be32_to_cpup(&data[data_offset++]);		/* 2nd field => <from> */
			cdmap_end = be32_to_cpup(&data[data_offset++]);			/* 3rd field => <till> */
			table->lux_tab[i] = be32_to_cpup(&data[data_offset++]);	/* 4th field => <candella> */
			/* Fill the backlight level to lux mapping array */
			do{
				table->bkl[cdmap_start++] = i;
			}while(cdmap_start <= cdmap_end);
		}
		return 0;
error:
	kfree(table->lux_tab);

	return -ENOMEM;
}

static int mdss_samsung_parse_panel_table(struct device_node *np,
		struct cmd_map *table, char *keystring)
{
		const __be32 *data;
		int  data_offset, len = 0 , i = 0;

		data = of_get_property(np, keystring, &len);
		if (!data) {
			pr_err("%s:%d, Unable to read table %s ",
				__func__, __LINE__, keystring);
			return -EINVAL;
		}
		if ((len % 2) != 0) {
			pr_err("%s:%d, Incorrect table entries for %s",
						__func__, __LINE__, keystring);
			return -EINVAL;
		}
		table->size = len / (sizeof(int)*2);
		table->bl_level = kzalloc((sizeof(int) * table->size), GFP_KERNEL);
		if (!table->bl_level)
			return -ENOMEM;

		table->cmd_idx = kzalloc((sizeof(int) * table->size), GFP_KERNEL);
		if (!table->cmd_idx)
			goto error;

		data_offset = 0;
		for (i = 0 ; i < table->size; i++) {
			table->bl_level[i] = be32_to_cpup(&data[data_offset++]);
			table->cmd_idx[i] = be32_to_cpup(&data[data_offset++]);
		}

		return 0;
error:
	kfree(table->cmd_idx);

	return -ENOMEM;
}

#endif

static int mdss_dsi_parse_dcs_cmds(struct device_node *np,
		struct dsi_panel_cmds *pcmds, char *cmd_key, char *link_key)
{
	const char *data;
	int type, blen = 0, len;
	char *buf, *bp;
	struct dsi_ctrl_hdr *dchdr;
	int i, cnt;
	int is_read = 0;

	type = 0;
	is_read = 0;

	data = of_get_property(np, cmd_key, &blen);
	if (!data) {
		pr_err("%s: failed, key=%s\n", __func__, cmd_key);
		return -ENOMEM;
	}

	buf = kzalloc(sizeof(char) * blen, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	memcpy(buf, data, blen);

	/* scan dcs commands */
	bp = buf;
	len = blen;
	cnt = 0;
	while (len >= sizeof(*dchdr)) {
		dchdr = (struct dsi_ctrl_hdr *)bp;
		dchdr->dlen = ntohs(dchdr->dlen);
		if (dchdr->dlen > len) {
			pr_err("%s: dtsi cmd=%x error, len=%d",
				__func__, dchdr->dtype, dchdr->dlen);
			goto exit_free;
		}
		bp += sizeof(*dchdr);
		len -= sizeof(*dchdr);
		bp += dchdr->dlen;
		len -= dchdr->dlen;
		cnt++;

		type = dchdr->dtype;
		if (type == DTYPE_GEN_READ ||
			type == DTYPE_GEN_READ1 ||
			type == DTYPE_GEN_READ2 ||
			type == DTYPE_DCS_READ)	{
			/* Read command :last byte contain read size, read start */
			bp += 2;
			len -= 2;
			is_read = 1;
		}
	}

	if (len != 0) {
		pr_err("%s: dcs_cmd=%x len=%d error!",
				__func__, buf[0], blen);
		goto exit_free;
	}

	if (is_read) {
		/*
			Allocate an array which will store the number
			for bytes to read for each read command
		*/
		pcmds->read_size = kzalloc(sizeof(char) * \
				cnt, GFP_KERNEL);
		if (!pcmds->read_size) {
			pr_err("%s:%d, Unable to read NV cmds",
				__func__, __LINE__);
			goto exit_free;
		}
		pcmds->read_startoffset = kzalloc(sizeof(char) * \
				cnt, GFP_KERNEL);
		if (!pcmds->read_startoffset) {
			pr_err("%s:%d, Unable to read NV cmds",
				__func__, __LINE__);
			goto error1;
		}
	}

	pcmds->cmds = kzalloc(cnt * sizeof(struct dsi_cmd_desc),
						GFP_KERNEL);
	if (!pcmds->cmds)
		goto exit_free;

	pcmds->cmd_cnt = cnt;
	pcmds->buf = buf;
	pcmds->blen = blen;

	bp = buf;
	len = blen;
	for (i = 0; i < cnt; i++) {
		dchdr = (struct dsi_ctrl_hdr *)bp;
		len -= sizeof(*dchdr);
		bp += sizeof(*dchdr);
		pcmds->cmds[i].dchdr = *dchdr;
		pcmds->cmds[i].payload = bp;
		bp += dchdr->dlen;
		len -= dchdr->dlen;

		if (is_read) {
			pcmds->read_size[i] = *bp++;
			pcmds->read_startoffset[i] = *bp++;
			len -= 2;
		}
	}

	/*Set default link state to LP Mode*/
	pcmds->link_state = DSI_LP_MODE;

	if (link_key) {
		data = of_get_property(np, link_key, NULL);
		if (data && !strcmp(data, "dsi_hs_mode"))
			pcmds->link_state = DSI_HS_MODE;
		else
			pcmds->link_state = DSI_LP_MODE;
	}

	pr_debug("%s: dcs_cmd=%x len=%d, cmd_cnt=%d link_state=%d\n", __func__,
		pcmds->buf[0], pcmds->blen, pcmds->cmd_cnt, pcmds->link_state);

	return 0;
error1:
	kfree(pcmds->read_size);
exit_free:
	kfree(buf);
	return -ENOMEM;
}


int mdss_panel_get_dst_fmt(u32 bpp, char mipi_mode, u32 pixel_packing,
				char *dst_format)
{
	int rc = 0;
	switch (bpp) {
	case 3:
		*dst_format = DSI_CMD_DST_FORMAT_RGB111;
		break;
	case 8:
		*dst_format = DSI_CMD_DST_FORMAT_RGB332;
		break;
	case 12:
		*dst_format = DSI_CMD_DST_FORMAT_RGB444;
		break;
	case 16:
		switch (mipi_mode) {
		case DSI_VIDEO_MODE:
			*dst_format = DSI_VIDEO_DST_FORMAT_RGB565;
			break;
		case DSI_CMD_MODE:
			*dst_format = DSI_CMD_DST_FORMAT_RGB565;
			break;
		default:
			*dst_format = DSI_VIDEO_DST_FORMAT_RGB565;
			break;
		}
		break;
	case 18:
		switch (mipi_mode) {
		case DSI_VIDEO_MODE:
			if (pixel_packing == 0)
				*dst_format = DSI_VIDEO_DST_FORMAT_RGB666;
			else
				*dst_format = DSI_VIDEO_DST_FORMAT_RGB666_LOOSE;
			break;
		case DSI_CMD_MODE:
			*dst_format = DSI_CMD_DST_FORMAT_RGB666;
			break;
		default:
			if (pixel_packing == 0)
				*dst_format = DSI_VIDEO_DST_FORMAT_RGB666;
			else
				*dst_format = DSI_VIDEO_DST_FORMAT_RGB666_LOOSE;
			break;
		}
		break;
	case 24:
		switch (mipi_mode) {
		case DSI_VIDEO_MODE:
			*dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
			break;
		case DSI_CMD_MODE:
			*dst_format = DSI_CMD_DST_FORMAT_RGB888;
			break;
		default:
			*dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
			break;
		}
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}


static int mdss_dsi_parse_fbc_params(struct device_node *np,
				struct mdss_panel_info *panel_info)
{
	int rc, fbc_enabled = 0;
	u32 tmp;

	fbc_enabled = of_property_read_bool(np,	"qcom,mdss-dsi-fbc-enable");
	if (fbc_enabled) {
		pr_debug("%s:%d FBC panel enabled.\n", __func__, __LINE__);
		panel_info->fbc.enabled = 1;
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-bpp", &tmp);
		panel_info->fbc.target_bpp =	(!rc ? tmp : panel_info->bpp);
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-packing",
				&tmp);
		panel_info->fbc.comp_mode = (!rc ? tmp : 0);
		panel_info->fbc.qerr_enable = of_property_read_bool(np,
			"qcom,mdss-dsi-fbc-quant-error");
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-bias", &tmp);
		panel_info->fbc.cd_bias = (!rc ? tmp : 0);
		panel_info->fbc.pat_enable = of_property_read_bool(np,
				"qcom,mdss-dsi-fbc-pat-mode");
		panel_info->fbc.vlc_enable = of_property_read_bool(np,
				"qcom,mdss-dsi-fbc-vlc-mode");
		panel_info->fbc.bflc_enable = of_property_read_bool(np,
				"qcom,mdss-dsi-fbc-bflc-mode");
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-h-line-budget",
				&tmp);
		panel_info->fbc.line_x_budget = (!rc ? tmp : 0);
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-budget-ctrl",
				&tmp);
		panel_info->fbc.block_x_budget = (!rc ? tmp : 0);
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-block-budget",
				&tmp);
		panel_info->fbc.block_budget = (!rc ? tmp : 0);
		rc = of_property_read_u32(np,
				"qcom,mdss-dsi-fbc-lossless-threshold", &tmp);
		panel_info->fbc.lossless_mode_thd = (!rc ? tmp : 0);
		rc = of_property_read_u32(np,
				"qcom,mdss-dsi-fbc-lossy-threshold", &tmp);
		panel_info->fbc.lossy_mode_thd = (!rc ? tmp : 0);
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-rgb-threshold",
				&tmp);
		panel_info->fbc.lossy_rgb_thd = (!rc ? tmp : 0);
		rc = of_property_read_u32(np,
				"qcom,mdss-dsi-fbc-lossy-mode-idx", &tmp);
		panel_info->fbc.lossy_mode_idx = (!rc ? tmp : 0);
	} else {
		pr_debug("%s:%d Panel does not support FBC.\n",
				__func__, __LINE__);
		panel_info->fbc.enabled = 0;
		panel_info->fbc.target_bpp =
			panel_info->bpp;
	}
	return 0;
}

static void mdss_panel_parse_te_params(struct device_node *np,
				       struct mdss_panel_info *panel_info)
{

	u32 tmp;
	int rc = 0;
	/*
	 * TE default: dsi byte clock calculated base on 70 fps;
	 * around 14 ms to complete a kickoff cycle if te disabled;
	 * vclk_line base on 60 fps; write is faster than read;
	 * init == start == rdptr;
	 */
	panel_info->te.tear_check_en =
		!of_property_read_bool(np, "qcom,mdss-tear-check-disable");
	rc = of_property_read_u32
		(np, "qcom,mdss-tear-check-sync-cfg-height", &tmp);
	panel_info->te.sync_cfg_height = (!rc ? tmp : 0xfff0);
	rc = of_property_read_u32
		(np, "qcom,mdss-tear-check-sync-init-val", &tmp);
	panel_info->te.vsync_init_val = (!rc ? tmp : panel_info->yres);
	rc = of_property_read_u32
		(np, "qcom,mdss-tear-check-sync-threshold-start", &tmp);
	panel_info->te.sync_threshold_start = (!rc ? tmp : 4);
	rc = of_property_read_u32
		(np, "qcom,mdss-tear-check-sync-threshold-continue", &tmp);
	panel_info->te.sync_threshold_continue = (!rc ? tmp : 4);
	rc = of_property_read_u32(np, "qcom,mdss-tear-check-start-pos", &tmp);
	panel_info->te.start_pos = (!rc ? tmp : panel_info->yres);
	rc = of_property_read_u32
		(np, "qcom,mdss-tear-check-rd-ptr-trigger-intr", &tmp);
	panel_info->te.rd_ptr_irq = (!rc ? tmp : panel_info->yres + 1);
	rc = of_property_read_u32(np, "qcom,mdss-tear-check-frame-rate", &tmp);
	panel_info->te.refx100 = (!rc ? tmp : 6000);
}


static int mdss_dsi_parse_reset_seq(struct device_node *np,
		u32 rst_seq[MDSS_DSI_RST_SEQ_LEN], u32 *rst_len,
		const char *name)
{
	int num = 0, i;
	int rc;
	struct property *data;
	u32 tmp[MDSS_DSI_RST_SEQ_LEN];
	*rst_len = 0;
	data = of_find_property(np, name, &num);
	num /= sizeof(u32);
	if (!data || !num || num > MDSS_DSI_RST_SEQ_LEN || num % 2) {
		pr_debug("%s:%d, error reading %s, length found = %d\n",
			__func__, __LINE__, name, num);
	} else {
		rc = of_property_read_u32_array(np, name, tmp, num);
		if (rc)
			pr_debug("%s:%d, error reading %s, rc = %d\n",
				__func__, __LINE__, name, rc);
		else {
			for (i = 0; i < num; ++i)
				rst_seq[i] = tmp[i];
			*rst_len = num;
		}
	}
	return 0;
}

static void mdss_dsi_parse_roi_alignment(struct device_node *np,
		struct mdss_panel_info *pinfo)
{
	int len = 0;
	u32 value[6];
	struct property *data;
	data = of_find_property(np, "qcom,panel-roi-alignment", &len);
	len /= sizeof(u32);
	if (!data || (len != 6)) {
		pr_debug("%s: Panel roi alignment not found", __func__);
	} else {
		int rc = of_property_read_u32_array(np,
				"qcom,panel-roi-alignment", value, len);
		if (rc)
			pr_debug("%s: Error reading panel roi alignment values",
					__func__);
		else {
			pinfo->xstart_pix_align = value[0];
			pinfo->width_pix_align = value[1];
			pinfo->ystart_pix_align = value[2];
			pinfo->height_pix_align = value[3];
			pinfo->min_width = value[4];
			pinfo->min_height = value[5];
		}

		pr_debug("%s: ROI alignment: [%d, %d, %d, %d, %d, %d]",
				__func__, pinfo->xstart_pix_align,
				pinfo->width_pix_align, pinfo->ystart_pix_align,
				pinfo->height_pix_align, pinfo->min_width,
				pinfo->min_height);
	}
}

static int mdss_dsi_parse_panel_features(struct device_node *np,
	struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_panel_info *pinfo;

	if (!np || !ctrl) {
		pr_err("%s: Invalid arguments\n", __func__);
		return -ENODEV;
	}

	pinfo = &ctrl->panel_data.panel_info;

	pinfo->cont_splash_enabled = of_property_read_bool(np,
		"qcom,cont-splash-enabled");

	pinfo->partial_update_enabled = of_property_read_bool(np,
		"qcom,partial-update-enabled");
	pr_info("%s:%d Partial update %s\n", __func__, __LINE__,
		(pinfo->partial_update_enabled ? "enabled" : "disabled"));
	if (pinfo->partial_update_enabled)
		ctrl->partial_update_fnc = mdss_dsi_panel_partial_update;

	pinfo->ulps_feature_enabled = of_property_read_bool(np,
		"qcom,ulps-enabled");
	pr_info("%s: ulps feature %s", __func__,
		(pinfo->ulps_feature_enabled ? "enabled" : "disabled"));
	pinfo->esd_check_enabled = of_property_read_bool(np,
		"qcom,esd-check-enabled");

	pinfo->mipi.dynamic_switch_enabled = of_property_read_bool(np,
		"qcom,dynamic-mode-switch-enabled");

	if (pinfo->mipi.dynamic_switch_enabled) {
		mdss_dsi_parse_dcs_cmds(np, &ctrl->video2cmd,
			"qcom,video-to-cmd-mode-switch-commands", NULL);

		mdss_dsi_parse_dcs_cmds(np, &ctrl->cmd2video,
			"qcom,cmd-to-video-mode-switch-commands", NULL);

		if (!ctrl->video2cmd.cmd_cnt || !ctrl->cmd2video.cmd_cnt) {
			pr_warn("No commands specified for dynamic switch\n");
			pinfo->mipi.dynamic_switch_enabled = 0;
		}
	}

	pr_info("%s: dynamic switch feature enabled: %d\n", __func__,
		pinfo->mipi.dynamic_switch_enabled);

	pinfo->mipi.force_clk_lane_hs = of_property_read_bool(np,
		"qcom,mdss-dsi-force-clk-lane-hs");

	pr_info("%s: force-clk-lane-hs %s \n",
			__func__,
			pinfo->mipi.force_clk_lane_hs ? "enabled" : "disabled");


	return 0;
}

static int mdss_panel_parse_dt(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	u32 tmp;
	int rc, i, len;
	const char *data;
	static const char *pdest;
	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-width", &tmp);
	if (rc) {
		pr_err("%s:%d, panel width not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	pinfo->xres = (!rc ? tmp : 640);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-height", &tmp);
	if (rc) {
		pr_err("%s:%d, panel height not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	pinfo->yres = (!rc ? tmp : 480);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-physical-width-dimension", &tmp);
	pinfo->physical_width = (!rc ? tmp : 0);
	rc = of_property_read_u32(np,
		"qcom,mdss-pan-physical-height-dimension", &tmp);
	pinfo->physical_height = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-left-border", &tmp);
	pinfo->lcdc.xres_pad = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-right-border", &tmp);
	if (!rc)
		pinfo->lcdc.xres_pad += tmp;
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-top-border", &tmp);
	pinfo->lcdc.yres_pad = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-bottom-border", &tmp);
	if (!rc)
		pinfo->lcdc.yres_pad += tmp;
	rc = of_property_read_u32(np, "qcom,mdss-dsi-bpp", &tmp);
	if (rc) {
		pr_err("%s:%d, bpp not specified\n", __func__, __LINE__);
		return -EINVAL;
	}
	pinfo->bpp = (!rc ? tmp : 24);
	pinfo->mipi.mode = DSI_VIDEO_MODE;
	data = of_get_property(np, "qcom,mdss-dsi-panel-type", NULL);
	if (data && !strncmp(data, "dsi_cmd_mode", 12))
		pinfo->mipi.mode = DSI_CMD_MODE;
	tmp = 0;
	data = of_get_property(np, "qcom,mdss-dsi-pixel-packing", NULL);
	if (data && !strcmp(data, "loose"))
		pinfo->mipi.pixel_packing = 1;
	else
		pinfo->mipi.pixel_packing = 0;
	rc = mdss_panel_get_dst_fmt(pinfo->bpp,
		pinfo->mipi.mode, pinfo->mipi.pixel_packing,
		&(pinfo->mipi.dst_format));
	if (rc) {
		pr_debug("%s: problem determining dst format. Set Default\n",
			__func__);
		pinfo->mipi.dst_format =
			DSI_VIDEO_DST_FORMAT_RGB888;
	}
	pdest = of_get_property(np,
		"qcom,mdss-dsi-panel-destination", NULL);

	if (pdest) {
		if (strlen(pdest) != 9) {
			pr_err("%s: Unknown pdest specified\n", __func__);
			return -EINVAL;
		}
		if (!strcmp(pdest, "display_1"))
			pinfo->pdest = DISPLAY_1;
		else if (!strcmp(pdest, "display_2"))
			pinfo->pdest = DISPLAY_2;
		else {
			pr_debug("%s: incorrect pdest. Set Default\n",
				__func__);
			pinfo->pdest = DISPLAY_1;
		}
	} else {
		pr_debug("%s: pdest not specified. Set Default\n",
				__func__);
		pinfo->pdest = DISPLAY_1;
	}
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-front-porch", &tmp);
	pinfo->lcdc.h_front_porch = (!rc ? tmp : 6);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-back-porch", &tmp);
	pinfo->lcdc.h_back_porch = (!rc ? tmp : 6);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-pulse-width", &tmp);
	pinfo->lcdc.h_pulse_width = (!rc ? tmp : 2);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-sync-skew", &tmp);
	pinfo->lcdc.hsync_skew = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-back-porch", &tmp);
	pinfo->lcdc.v_back_porch = (!rc ? tmp : 6);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-front-porch", &tmp);
	pinfo->lcdc.v_front_porch = (!rc ? tmp : 6);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-pulse-width", &tmp);
	pinfo->lcdc.v_pulse_width = (!rc ? tmp : 2);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-underflow-color", &tmp);
	pinfo->lcdc.underflow_clr = (!rc ? tmp : 0xff);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-border-color", &tmp);
	pinfo->lcdc.border_clr = (!rc ? tmp : 0);
	pinfo->bklt_ctrl = UNKNOWN_CTRL;
	data = of_get_property(np, "qcom,mdss-dsi-bl-pmic-control-type", NULL);
	if (data) {
		if (!strncmp(data, "bl_ctrl_wled", 12)) {
			led_trigger_register_simple("bkl-trigger",
				&bl_led_trigger);
			pr_debug("%s: SUCCESS-> WLED TRIGGER register\n",
				__func__);
			ctrl_pdata->bklt_ctrl = BL_WLED;
		} else if (!strncmp(data, "bl_ctrl_pwm", 11)) {
			ctrl_pdata->bklt_ctrl = BL_PWM;
			rc = of_property_read_u32(np,
				"qcom,mdss-dsi-bl-pmic-pwm-frequency", &tmp);
			if (rc) {
				pr_err("%s:%d, Error, panel pwm_period\n",
						__func__, __LINE__);
				return -EINVAL;
			}
			ctrl_pdata->pwm_period = tmp;
			rc = of_property_read_u32(np,
				"qcom,mdss-dsi-bl-pmic-bank-select", &tmp);
			if (rc) {
				pr_err("%s:%d, Error, dsi lpg channel\n",
						__func__, __LINE__);
				return -EINVAL;
			}
			ctrl_pdata->pwm_lpg_chan = tmp;
			tmp = of_get_named_gpio(np,
				"qcom,mdss-dsi-pwm-gpio", 0);
			ctrl_pdata->pwm_pmic_gpio = tmp;
		} else if (!strncmp(data, "bl_ctrl_dcs", 11)) {
			ctrl_pdata->bklt_ctrl = BL_DCS_CMD;
		}
	}
	rc = of_property_read_u32(np, "qcom,mdss-brightness-max-level", &tmp);
	pinfo->brightness_max = (!rc ? tmp : MDSS_MAX_BL_BRIGHTNESS);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-bl-min-level", &tmp);
	pinfo->bl_min = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-bl-max-level", &tmp);
	pinfo->bl_max = (!rc ? tmp : 255);
	ctrl_pdata->bklt_max = pinfo->bl_max;

	rc = of_property_read_u32(np, "qcom,mdss-dsi-interleave-mode", &tmp);
	pinfo->mipi.interleave_mode = (!rc ? tmp : 0);

	pinfo->mipi.vsync_enable = of_property_read_bool(np,
		"qcom,mdss-dsi-te-check-enable");
	pinfo->mipi.hw_vsync_mode = of_property_read_bool(np,
		"qcom,mdss-dsi-te-using-te-pin");

	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-h-sync-pulse", &tmp);
	pinfo->mipi.pulse_mode_hsa_he = (!rc ? tmp : false);

	pinfo->mipi.hfp_power_stop = of_property_read_bool(np,
		"qcom,mdss-dsi-hfp-power-mode");
	pinfo->mipi.hsa_power_stop = of_property_read_bool(np,
		"qcom,mdss-dsi-hsa-power-mode");
	pinfo->mipi.hbp_power_stop = of_property_read_bool(np,
		"qcom,mdss-dsi-hbp-power-mode");
	pinfo->mipi.last_line_interleave_en = of_property_read_bool(np,
		"qcom,mdss-dsi-last-line-interleave");
	pinfo->mipi.bllp_power_stop = of_property_read_bool(np,
		"qcom,mdss-dsi-bllp-power-mode");
	pinfo->mipi.eof_bllp_power_stop = of_property_read_bool(
		np, "qcom,mdss-dsi-bllp-eof-power-mode");
	pinfo->mipi.traffic_mode = DSI_NON_BURST_SYNCH_PULSE;
	data = of_get_property(np, "qcom,mdss-dsi-traffic-mode", NULL);
	if (data) {
		if (!strcmp(data, "non_burst_sync_event"))
			pinfo->mipi.traffic_mode = DSI_NON_BURST_SYNCH_EVENT;
		else if (!strcmp(data, "burst_mode"))
			pinfo->mipi.traffic_mode = DSI_BURST_MODE;
	}
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-te-dcs-command", &tmp);
	pinfo->mipi.insert_dcs_cmd =
			(!rc ? tmp : 1);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-wr-mem-continue", &tmp);
	pinfo->mipi.wr_mem_continue =
			(!rc ? tmp : 0x3c);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-wr-mem-start", &tmp);
	pinfo->mipi.wr_mem_start =
			(!rc ? tmp : 0x2c);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-te-pin-select", &tmp);
	pinfo->mipi.te_sel =
			(!rc ? tmp : 1);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-virtual-channel-id", &tmp);
	pinfo->mipi.vc = (!rc ? tmp : 0);
	pinfo->mipi.rgb_swap = DSI_RGB_SWAP_RGB;
	data = of_get_property(np, "qcom,mdss-dsi-color-order", NULL);
	if (data) {
		if (!strcmp(data, "rgb_swap_rbg"))
			pinfo->mipi.rgb_swap = DSI_RGB_SWAP_RBG;
		else if (!strcmp(data, "rgb_swap_bgr"))
			pinfo->mipi.rgb_swap = DSI_RGB_SWAP_BGR;
		else if (!strcmp(data, "rgb_swap_brg"))
			pinfo->mipi.rgb_swap = DSI_RGB_SWAP_BRG;
		else if (!strcmp(data, "rgb_swap_grb"))
			pinfo->mipi.rgb_swap = DSI_RGB_SWAP_GRB;
		else if (!strcmp(data, "rgb_swap_gbr"))
			pinfo->mipi.rgb_swap = DSI_RGB_SWAP_GBR;
	}
	pinfo->mipi.data_lane0 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-0-state");
	pinfo->mipi.data_lane1 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-1-state");
	pinfo->mipi.data_lane2 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-2-state");
	pinfo->mipi.data_lane3 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-3-state");

	rc = of_property_read_u32(np, "qcom,mdss-dsi-t-clk-pre", &tmp);
	pinfo->mipi.t_clk_pre = (!rc ? tmp : 0x24);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-t-clk-post", &tmp);
	pinfo->mipi.t_clk_post = (!rc ? tmp : 0x03);

	pinfo->mipi.rx_eot_ignore = of_property_read_bool(np,
		"qcom,mdss-dsi-rx-eot-ignore");
	pinfo->mipi.tx_eot_append = of_property_read_bool(np,
		"qcom,mdss-dsi-tx-eot-append");

	rc = of_property_read_u32(np, "qcom,mdss-dsi-stream", &tmp);
	pinfo->mipi.stream = (!rc ? tmp : 0);

	data = of_get_property(np, "qcom,mdss-dsi-panel-mode-gpio-state", NULL);
	if (data) {
		if (!strcmp(data, "high"))
			pinfo->mode_gpio_state = MODE_GPIO_HIGH;
		else if (!strcmp(data, "low"))
			pinfo->mode_gpio_state = MODE_GPIO_LOW;
	} else {
		pinfo->mode_gpio_state = MODE_GPIO_NOT_VALID;
	}

	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-framerate", &tmp);
	pinfo->mipi.frame_rate = (!rc ? tmp : 60);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-clockrate", &tmp);
	pinfo->clk_rate = (!rc ? tmp : 0);
	data = of_get_property(np, "qcom,mdss-dsi-panel-timings", &len);
	if ((!data) || (len != 12)) {
		pr_err("%s:%d, Unable to read Phy timing settings",
		       __func__, __LINE__);
		goto error;
	}
	for (i = 0; i < len; i++)
		pinfo->mipi.dsi_phy_db.timing[i] = data[i];

	pinfo->mipi.lp11_init = of_property_read_bool(np,
					"qcom,mdss-dsi-lp11-init");
	rc = of_property_read_u32(np, "qcom,mdss-dsi-init-delay-us", &tmp);
	pinfo->mipi.init_delay = (!rc ? tmp : 0);

	mdss_dsi_parse_roi_alignment(np, pinfo);

	mdss_dsi_parse_trigger(np, &(pinfo->mipi.mdp_trigger),
		"qcom,mdss-dsi-mdp-trigger");

	mdss_dsi_parse_trigger(np, &(pinfo->mipi.dma_trigger),
		"qcom,mdss-dsi-dma-trigger");

	mdss_dsi_parse_lane_swap(np, &(pinfo->mipi.dlane_swap));

	mdss_dsi_parse_fbc_params(np, pinfo);

	mdss_panel_parse_te_params(np, pinfo);

	mdss_dsi_parse_reset_seq(np, pinfo->rst_seq, &(pinfo->rst_seq_len),
		"qcom,mdss-dsi-reset-sequence");

	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->on_cmds,
		"qcom,mdss-dsi-on-command", "qcom,mdss-dsi-on-command-state");
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cmd_list.disp_on_seq,
		"qcom,mdss-dsi-disp-on-seq", "qcom,mdss-dsi-disp-on-seq-state");
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cmd_list.disp_on_cmd,
		"qcom,mdss-dsi-disp-on-cmds", NULL);


	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->off_cmds,
		"qcom,mdss-dsi-off-command", "qcom,mdss-dsi-off-command-state");
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cmd_list.disp_off_seq,
		"qcom,mdss-dsi-disp-off-seq", "qcom,mdss-dsi-disp-off-seq-state");
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cmd_list.disp_off_cmd,
		"qcom,mdss-dsi-disp-off-cmds", NULL);


	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cmd_list.panel_manufacture_id_cmds,
		"qcom,mdss-dsi-panel-manufacture-id-command", "qcom,mdss-dsi-panel-manufacture-id-command-state");
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cmd_list.panel_manufacture_id_register_set_cmds,
		"qcom,mdss-dsi-panel-manufacture-id-register-set-cmds", NULL);


	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cmd_list.hsync_on_seq,
		"qcom,mdss-dsi-disp-hsync-on-seq", NULL);

	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cmd_list.acl_off_cmd,
		"qcom,mdss-dsi-disp-acl-off-cmds", NULL);

	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cmd_list.partialdisp_on_cmd,
		"qcom,mdss-dsi-disp-ldi-partial-disp-on", NULL);

	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cmd_list.partialdisp_off_cmd,
		"qcom,mdss-dsi-disp-ldi-partial-disp-off", NULL);

	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cmd_list.mtp_enable_cmd,
		"qcom,mdss-dsi-disp-mtp-enable-cmds", NULL);
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cmd_list.mtp_disable_cmd,
		"qcom,mdss-dsi-disp-mtp-disable-cmds", NULL);

	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cmd_list.rddpm_cmd,
		"qcom,mdss-dsi-panel-rddpm-read-cmds", NULL);

	mdss_samsung_parse_panel_table(np, &ctrl_pdata->map_table_list.aid_map_table,
				"samsung,panel-aid-map-table");
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cmd_list.aid_cmd_list,
				"samsung,panel-aid-cmds-list", NULL);

	mdss_samsung_parse_panel_table(np, &ctrl_pdata->map_table_list.acl_map_table,
				"samsung,panel-acl-map-table");
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cmd_list.smart_acl_elvss_cmd_list,
				"samsung,panel-smart-acl-elvss-cmds-list", NULL);
	mdss_samsung_parse_panel_table(np, &ctrl_pdata->map_table_list.smart_acl_elvss_map_table,
				"samsung,panel-smart-acl-elvss-map-table");
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cmd_list.elvss_cmd_list,
				"samsung,panel-elvss-cmds-list", NULL);
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cmd_list.gamma_cmd_list,
				"samsung,panel-gamma-cmds-list", NULL);
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cmd_list.mtp_read_sysfs_cmds,
				"samsung,panel-mtp-read-sysfs-cmds", NULL);

	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->status_cmds,
			"qcom,mdss-dsi-panel-status-command",
				"qcom,mdss-dsi-panel-status-command-state");
	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-status-value", &tmp);
	ctrl_pdata->status_value = (!rc ? tmp : 0);

#ifdef SMART_DIMMING
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cmd_list.nv_mtp_register_set_cmds,
				"samsung,panel-nv-mtp-register-set-cmds", NULL);
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cmd_list.nv_mtp_read_cmds,
				"samsung,panel-nv-mtp-read-cmds", NULL);

	/* Process the lux value table */
	mdss_samsung_parse_candella_lux_mapping_table(np,
			&ctrl_pdata->map_table_list.candela_map_table,
			"samsung,panel-candella-mapping-table");
#endif


	ctrl_pdata->status_mode = ESD_MAX;
	rc = of_property_read_string(np,
				"qcom,mdss-dsi-panel-status-check-mode", &data);
	if (!rc) {
		if (!strcmp(data, "bta_check"))
			ctrl_pdata->status_mode = ESD_BTA;
		else if (!strcmp(data, "reg_read"))
			ctrl_pdata->status_mode = ESD_REG;
	}

	rc = mdss_dsi_parse_panel_features(np, ctrl_pdata);
	if (rc) {
		pr_err("%s: failed to parse panel features\n", __func__);
		goto error;
	}

	return 0;

error:
	return -EINVAL;
}


static ssize_t mipi_samsung_disp_get_power(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc = 0;
/*
	struct msm_fb_data_type *mfd = msd.mfd;

	if (unlikely(!mfd))
		return -ENODEV;
	if (unlikely(mfd->key != MFD_KEY))
		return -EINVAL;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n", mfd->panel_power_on);
	pr_info("mipi_samsung_disp_get_power(%d)\n", mfd->panel_power_on);
*/
	return rc;
}

static ssize_t mipi_samsung_disp_set_power(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
/*
	unsigned int power;
	struct msm_fb_data_type *mfd = msd.mfd;

	if (sscanf(buf, "%u", &power) != 1)
		return -EINVAL;

	if (power == mfd->panel_power_on)
		return 0;

	if (power) {
		mfd->fbi->fbops->fb_blank(FB_BLANK_UNBLANK, mfd->fbi);
		mfd->fbi->fbops->fb_pan_display(&mfd->fbi->var, mfd->fbi);
		mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);
	} else {
		mfd->fbi->fbops->fb_blank(FB_BLANK_POWERDOWN, mfd->fbi);
	}

	pr_info("mipi_samsung_disp_set_power\n");
*/
	return size;
}


static ssize_t mipi_samsung_disp_lcdtype_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char temp[100] = {0,};
	struct mipi_samsung_driver_data *s_pdata = NULL;

	s_pdata = (struct mipi_samsung_driver_data *)dev_get_drvdata(dev);

	if (s_pdata->manufacture_id) {
		snprintf(temp, 20, "SDC_%x\n",s_pdata->manufacture_id);
	} else {
		pr_info("no manufacture id\n");
	}

/*
		switch (msd.panel) {
			case PANEL_FHD_OCTA_S6E3FA0:
			case PANEL_FHD_OCTA_S6E3FA0_CMD:
				snprintf(temp, 20, "SDC_AMS568AT01\n");
				break;
			case PANEL_FHD_OCTA_S6E3FA2_CMD:
				snprintf(temp, 20, "SDC_AMS520BQ01\n");
				break;
			case PANEL_WQHD_OCTA_S6E3HA0_CMD:
				snprintf(temp, 20, "SDC_AMS520BR01\n");
				break;
			default :
				snprintf(temp, strnlen(msd.panel_name, 100),
									msd.panel_name);
				break;
*/

	strlcat(buf, temp, 100);

	return strnlen(buf, 100);
}

static ssize_t mipi_samsung_disp_windowtype_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char temp[15];
	int id1, id2, id3;
	struct mipi_samsung_driver_data *s_pdata = NULL;

	s_pdata = (struct mipi_samsung_driver_data *)dev_get_drvdata(dev);

	id1 = (s_pdata->manufacture_id & 0x00FF0000) >> 16;
	id2 = (s_pdata->manufacture_id & 0x0000FF00) >> 8;
	id3 = s_pdata->manufacture_id & 0xFF;

	snprintf(temp, sizeof(temp), "%x %x %x\n",	id1, id2, id3);
	strlcat(buf, temp, 15);
	return strnlen(buf, 15);
}

static ssize_t mipi_samsung_disp_manufacture_date_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char temp[30];
	struct mipi_samsung_driver_data *s_pdata = NULL;

	s_pdata = (struct mipi_samsung_driver_data *)dev_get_drvdata(dev);

	snprintf((char *)temp, sizeof(temp), "manufacture date : %d\n", s_pdata->manufacture_date);
	strlcat(buf, temp, 30);

	pr_info("manufacture date : %d\n", s_pdata->manufacture_date);

	return strnlen(buf, 30);
}

/*
static ssize_t mipi_samsung_disp_ddi_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char temp[30];
	struct mipi_samsung_driver_data *s_pdata = NULL;

	s_pdata = (struct mipi_samsung_driver_data *)dev_get_drvdata(dev);

	snprintf((char *)temp, sizeof(temp), "ddi id : %02x %02x %02x %02x %02x\n",
		msd.ddi_id[0], msd.ddi_id[1], msd.ddi_id[2], msd.ddi_id[3], msd.ddi_id[4]);
	strlcat(buf, temp, 30);

	pr_info("%s : %02x %02x %02x %02x %02x\n", __func__,
		msd.ddi_id[0], msd.ddi_id[1], msd.ddi_id[2], msd.ddi_id[3], msd.ddi_id[4]);

	return strnlen(buf, 30);
}
*/
static ssize_t mipi_samsung_disp_acl_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;
	struct mipi_samsung_driver_data *s_pdata = NULL;

	s_pdata = (struct mipi_samsung_driver_data *)dev_get_drvdata(dev);


	rc = snprintf((char *)buf, sizeof(buf), "%d\n", s_pdata->dstat.acl_on);
	pr_info("acl status: %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_disp_acl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd = NULL;
	int	acl_set;
	struct mipi_samsung_driver_data *s_pdata = NULL;
	struct mdss_panel_data *pdata = NULL;

	s_pdata = (struct mipi_samsung_driver_data *)dev_get_drvdata(dev);

	pdata = (struct mdss_panel_data *)s_pdata->mdss_panel_data;

	mfd = s_pdata->mfd;

	acl_set = s_pdata->dstat.acl_on;
	if (sysfs_streq(buf, "1"))
		acl_set = true;
	else if (sysfs_streq(buf, "0"))
		acl_set = false;
	else
		pr_info("%s: Invalid argument!!", __func__);

	s_pdata->dstat.elvss_need_update = 1;

	if (mfd->panel_power_on) {
		if (acl_set && !(s_pdata->dstat.acl_on ||
					s_pdata->dstat.siop_status)) {
			s_pdata->dstat.acl_on = true;
			pr_info("%s: acl on  : acl %d, siop %d", __func__,
					s_pdata->dstat.acl_on,
					s_pdata->dstat.siop_status);
			if (likely(pdata))
				mipi_samsung_disp_send_cmd(pdata, PANEL_BRIGHT_CTRL, true);

		} else if (!acl_set &&
				s_pdata->dstat.acl_on &&
				!s_pdata->dstat.siop_status) {
			s_pdata->dstat.acl_on = false;
			s_pdata->dstat.curr_acl_idx = -1;
			s_pdata->dstat.curr_opr_idx = -1;
			pr_info("%s: acl off : acl %d, siop %d", __func__,
					s_pdata->dstat.acl_on,
					s_pdata->dstat.siop_status);
			if(s_pdata->dstat.auto_brightness == 6)
				pr_info("%s: HBM mode No ACL off!!", __func__);
#ifdef SMART_ACL
			/* If SMART_ACL enabled, elvss table shoud be set again */
			if (likely(pdata))
				mipi_samsung_disp_send_cmd(pdata, PANEL_BRIGHT_CTRL, true);
#endif

		} else {
			s_pdata->dstat.acl_on = acl_set;
			pr_info("%s: skip but acl update!! acl %d, siop %d", __func__,
				s_pdata->dstat.acl_on,
				s_pdata->dstat.siop_status);
		}

	} else {
		pr_info("%s: panel is off state. updating state value.\n",
			__func__);
		s_pdata->dstat.acl_on = acl_set;
	}

	return size;
}

static ssize_t mipi_samsung_disp_siop_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;
	struct mipi_samsung_driver_data *s_pdata = NULL;

	s_pdata = (struct mipi_samsung_driver_data *)dev_get_drvdata(dev);


	rc = snprintf((char *)buf, sizeof(buf), "%d\n", s_pdata->dstat.siop_status);
	pr_info("siop status: %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_disp_siop_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd = NULL;
	int siop_set;
	struct mipi_samsung_driver_data *s_pdata = NULL;
	struct mdss_panel_data *pdata = NULL;

	s_pdata = (struct mipi_samsung_driver_data *)dev_get_drvdata(dev);

	pdata = (struct mdss_panel_data *)s_pdata->mdss_panel_data;

	mfd = s_pdata->mfd;


	siop_set = s_pdata->dstat.siop_status;
	if (sysfs_streq(buf, "1"))
		siop_set = true;
	else if (sysfs_streq(buf, "0"))
		siop_set = false;
	else
		pr_info("%s: Invalid argument!!", __func__);

	if (mfd->panel_power_on) {
		if (siop_set && !(s_pdata->dstat.acl_on
					|| s_pdata->dstat.siop_status)) {
			s_pdata->dstat.siop_status = true;
			mipi_samsung_disp_send_cmd(pdata, PANEL_BRIGHT_CTRL, true);
			pr_info("%s: acl on  : acl %d, siop %d", __func__,
				s_pdata->dstat.acl_on,
				s_pdata->dstat.siop_status);

		} else if (!siop_set &&
				!s_pdata->dstat.acl_on &&
				s_pdata->dstat.siop_status) {
			mutex_lock(&s_pdata->lock);
			s_pdata->dstat.siop_status = false;
			s_pdata->dstat.curr_acl_idx = -1;
			s_pdata->dstat.curr_opr_idx = -1;
			if(s_pdata->dstat.auto_brightness == 6)
				pr_info("%s: HBM mode No ACL off!!", __func__);
#ifdef SMART_ACL
			/* If SMART_ACL enabled, elvss table shoud be set again */
			mipi_samsung_disp_send_cmd(pdata, PANEL_BRIGHT_CTRL, false);
#endif
			mutex_unlock(&s_pdata->lock);
			pr_info("%s: acl off : acl %d, siop %d", __func__,
				s_pdata->dstat.acl_on,
				s_pdata->dstat.siop_status);

		} else {
			s_pdata->dstat.siop_status = siop_set;
			pr_info("%s: skip but siop update!! acl %d, siop %d", __func__,
				s_pdata->dstat.acl_on,
				s_pdata->dstat.siop_status);
		}
	} else {
		s_pdata->dstat.siop_status = siop_set;
		pr_info("%s: panel is off state. updating state value.\n",
				__func__);
	}

	return size;
}

static ssize_t mipi_samsung_read_mtp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int addr, len, start;
	char *read_buf = NULL;
	struct mipi_samsung_driver_data *s_pdata = NULL;
	struct mdss_panel_data *pdata = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_dsi_panel_cmd_list *cmd_list = NULL;

	s_pdata = (struct mipi_samsung_driver_data *)dev_get_drvdata(dev);

	pdata = (struct mdss_panel_data *)s_pdata->mdss_panel_data;
	ctrl_pdata = (struct mdss_dsi_ctrl_pdata *)s_pdata->mdss_dsi_ctrl_pdata;
	cmd_list = &ctrl_pdata->cmd_list;

	sscanf(buf, "%x %d %x" , &addr, &len, &start);

	pr_info("%x %d %x\n", addr, len, start);

	if (len > 0x100){
		pr_info("len(%d)is over\n", len);
		return size; /*let the len has limitation*/
	}

	read_buf = kmalloc(len * sizeof(char), GFP_KERNEL);

	cmd_list->mtp_read_sysfs_cmds.cmds[0].payload[0] = addr; /* addr */
	cmd_list->mtp_read_sysfs_cmds.cmds[0].payload[1] = len; /* size */
	cmd_list->mtp_read_sysfs_cmds.cmds[0].payload[2] = start; /* start */

	cmd_list->mtp_read_sysfs_cmds.read_size =
		kzalloc(sizeof(char) * cmd_list->mtp_read_sysfs_cmds.cmd_cnt,
				GFP_KERNEL);
	cmd_list->mtp_read_sysfs_cmds.read_startoffset =
		kzalloc(sizeof(char) * cmd_list->mtp_read_sysfs_cmds.cmd_cnt,
				GFP_KERNEL);

	cmd_list->mtp_read_sysfs_cmds.read_size[0] = len;
	cmd_list->mtp_read_sysfs_cmds.read_startoffset[0] = start;

	pr_info("%x %x %x %x %x %x %x %x %x\n",
		cmd_list->mtp_read_sysfs_cmds.cmds[0].dchdr.dtype,
		cmd_list->mtp_read_sysfs_cmds.cmds[0].dchdr.last,
		cmd_list->mtp_read_sysfs_cmds.cmds[0].dchdr.vc,
		cmd_list->mtp_read_sysfs_cmds.cmds[0].dchdr.ack,
		cmd_list->mtp_read_sysfs_cmds.cmds[0].dchdr.wait,
		cmd_list->mtp_read_sysfs_cmds.cmds[0].dchdr.dlen,
		cmd_list->mtp_read_sysfs_cmds.cmds[0].payload[0],
		cmd_list->mtp_read_sysfs_cmds.cmds[0].payload[1],
		cmd_list->mtp_read_sysfs_cmds.cmds[0].payload[2]);

	mipi_samsung_read_nv_mem(pdata, &cmd_list->mtp_read_sysfs_cmds, read_buf);

	kfree(read_buf);
	return size;
}

#if defined(TEMPERATURE_ELVSS)
static ssize_t mipi_samsung_temperature_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, 40,"-20, -19, 0, 1, 30, 40\n");

	pr_info("%s msd.mpd->temperature : %d msd.mpd->temperature_value : 0x%x", __func__,
				msd.dstat.temperature, msd.dstat.temperature_value);

	return rc;
}

static ssize_t mipi_samsung_temperature_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int temp;

	sscanf(buf, "%d" , &msd.dstat.temperature);

	temp = msd.dstat.temperature;

	if (temp > 0)
		msd.dstat.temperature_value = (char)temp;
	else {
		temp *= -1;
		msd.dstat.temperature_value = (char)temp;
		msd.dstat.temperature_value |=0x80;
	}

	msd.dstat.elvss_need_update = 1;

	if(msd.mfd->resume_state == MIPI_RESUME_STATE) {
		mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);
		pr_info("mipi_samsung_temperature_store %d\n", msd.dstat.bright_level);
		pr_info("%s msd.dstat.temperature : %d msd.dstat.temperature_value : 0x%x", __func__,
				msd.dstat.temperature, msd.dstat.temperature_value);
	} else {
		pr_info("%s: skip but temperature update!! temperature %d, temperature_value %d", __func__,
				msd.dstat.temperature, msd.dstat.temperature_value);
	}

	return size;
}
static ssize_t mipi_samsung_aid_log_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc = 0;

	if (msd.dstat.is_smart_dim_loaded)
		msd.sdimconf->print_aid_log();
	else
		pr_err("smart dim is not loaded..\n");

	return rc;
}

#endif

#if defined(TEST_RESOLUTION)
static ssize_t mipi_samsung_disp_panel_res_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char temp[100];

	switch (msd.panel) {
		case PANEL_FHD_OCTA_S6E3FA2_CMD:
		case PANEL_FHD_OCTA_EA8064G_CMD:
		case PANEL_HD_OCTA_EA8064G_CMD: /*temp*/
			snprintf(temp, 10, "FHD\n");
			break;
		default :
			snprintf(temp, 10, "FHD\n");
			break;
	}
	strlcat(buf, temp, 100);

	return strnlen(buf, 100);

}
#endif

static DEVICE_ATTR(lcd_power, S_IRUGO | S_IWUSR,
			mipi_samsung_disp_get_power,
			mipi_samsung_disp_set_power);

static DEVICE_ATTR(lcd_type, S_IRUGO,
			mipi_samsung_disp_lcdtype_show,
			NULL);
static DEVICE_ATTR(window_type, S_IRUGO,
			mipi_samsung_disp_windowtype_show, NULL);
static DEVICE_ATTR(manufacture_date, S_IRUGO,
			mipi_samsung_disp_manufacture_date_show, NULL);
/*
static DEVICE_ATTR(ddi_id, S_IRUGO,
			mipi_samsung_disp_ddi_id_show, NULL);
*/
static DEVICE_ATTR(power_reduce, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_disp_acl_show,
			mipi_samsung_disp_acl_store);
static DEVICE_ATTR(siop_enable, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_disp_siop_show,
			mipi_samsung_disp_siop_store);
static DEVICE_ATTR(read_mtp, S_IRUGO | S_IWUSR | S_IWGRP,
			NULL,
			mipi_samsung_read_mtp_store);
#if defined(TEMPERATURE_ELVSS)
static DEVICE_ATTR(temperature, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_temperature_show,
			mipi_samsung_temperature_store);
static DEVICE_ATTR(aid_log, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_aid_log_show,
			NULL);
#endif
#if defined(TEST_RESOLUTION)
static DEVICE_ATTR(panel_res, S_IRUGO,
			mipi_samsung_disp_panel_res_show,
			NULL);
#endif

static struct attribute *panel_sysfs_attributes[] = {
	&dev_attr_lcd_power.attr,
	&dev_attr_lcd_type.attr,
	&dev_attr_window_type.attr,
	&dev_attr_manufacture_date.attr,
	/*&dev_attr_ddi_id.attr,*/
	&dev_attr_power_reduce.attr,
	&dev_attr_siop_enable.attr,
	&dev_attr_read_mtp.attr,
#if defined(TEMPERATURE_ELVSS)
	&dev_attr_aid_log.attr,
	&dev_attr_temperature.attr,
#endif
#if defined(TEST_RESOLUTION)
	&dev_attr_panel_res.attr,
#endif
	NULL
};

static const struct attribute_group panel_sysfs_group = {
	.attrs = panel_sysfs_attributes,
};

static struct lcd_ops mipi_samsung_disp_props = {
	.get_power = NULL,
	.set_power = NULL,
};

static int sysfs_enable;
static int mdss_samsung_create_sysfs(struct mipi_samsung_driver_data *s_pdata)
{
	int rc = 0;

	struct lcd_device *lcd_device;
#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	struct backlight_device *bd = NULL;
#endif

	/* sysfs creat func should be called one time in dual dsi mode */
	if (sysfs_enable)
		return 0;

	lcd_device = lcd_device_register("panel", NULL, s_pdata,
					&mipi_samsung_disp_props);

	if (IS_ERR(lcd_device)) {
		rc = PTR_ERR(lcd_device);
		pr_err("Failed to register lcd device..\n");
		return rc;
	}
	lcd_device->dev.platform_data = (void *)s_pdata;
	s_pdata->lcd_device = lcd_device;
	sysfs_remove_file(&lcd_device->dev.kobj,
		&dev_attr_lcd_power.attr);

	rc = sysfs_create_group(&lcd_device->dev.kobj, &panel_sysfs_group);
	if (rc) {
		pr_err("Failed to create panel sysfs group..\n");
		sysfs_remove_group(&lcd_device->dev.kobj, &panel_sysfs_group);
		return rc;
	}

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	bd = backlight_device_register("panel", &lcd_device->dev,
						NULL, NULL, NULL);
	if (IS_ERR(bd)) {
		rc = PTR_ERR(bd);
		pr_err("backlight : failed to register device\n");
		return rc;
	}

/*
	rc = sysfs_create_group(&bd->dev.kobj, &bl_sysfs_group);
	if (rc) {
		pr_err("Failed to create backlight sysfs group..\n");
		sysfs_remove_group(&bd->dev.kobj, &bl_sysfs_group);
		return rc;
	}
*/
#endif

#if defined(CONFIG_ESD_FG_RECOVERY)
#ifdef ESD_DEBUG
	rc= sysfs_create_file(&lcd_device->dev.kobj,
							&dev_attr_esd_check.attr);
	if (rc) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_esd_check.attr.name);
	}
#endif
#endif

#if defined(DDI_VIDEO_ENHANCE_TUNING)
     rc = sysfs_create_file(&lcd_device->dev.kobj,
			 &dev_attr_tuning.attr);
    if (rc) {
	   pr_err("sysfs create fail-%s\n",
				   dev_attr_tuning.attr.name);
	   return rc;
	}
#endif

	sysfs_enable = 1;

	pr_debug("%s: done!! \n", __func__);

	return rc;
}


int mdss_dsi_panel_init(struct device_node *node,
	struct mdss_dsi_ctrl_pdata *ctrl_pdata,
	bool cmd_cfg_cont_splash)
{
	int rc = 0;
	static const char *panel_name;
	struct mdss_panel_info *pinfo;
	struct mdss_panel_data *pdata;
	struct mipi_samsung_driver_data *s_pdata;

	if (!node || !ctrl_pdata) {
		pr_err("%s: Invalid arguments\n", __func__);
		return -ENODEV;
	}

	pinfo = &ctrl_pdata->panel_data.panel_info;
	pdata = &ctrl_pdata->panel_data;
	s_pdata = &pdata->samsung_pdata;
	s_pdata->mdss_panel_data = (void *)pdata;
	s_pdata->mdss_dsi_ctrl_pdata = (void *)ctrl_pdata;

	pr_debug("%s:%d\n", __func__, __LINE__);
	panel_name = of_get_property(node, "qcom,mdss-dsi-panel-name", NULL);
	if (!panel_name)
		pr_info("%s:%d, Panel name not specified\n",
						__func__, __LINE__);
	else
		pr_info("%s: Panel Name = %s\n", __func__, panel_name);

	rc = mdss_panel_parse_dt(node, ctrl_pdata);
	if (rc) {
		pr_err("%s:%d panel dt parse failed\n", __func__, __LINE__);
		return rc;
	}

	if (!cmd_cfg_cont_splash)
		pinfo->cont_splash_enabled = false;
	pr_info("%s: Continuous splash %s", __func__,
		pinfo->cont_splash_enabled ? "enabled" : "disabled");

	pinfo->dynamic_switch_pending = false;
	pinfo->is_lpm_mode = false;

	ctrl_pdata->on = mdss_dsi_panel_on;
	ctrl_pdata->off = mdss_dsi_panel_off;
	ctrl_pdata->panel_data.set_backlight = mdss_dsi_panel_bl_ctrl;
	ctrl_pdata->switch_mode = mdss_dsi_panel_switch_mode;
	ctrl_pdata->panel_extra_power = mdss_dsi_extra_power;
	ctrl_pdata->panel_reset = mdss_dsi_panel_reset;
	ctrl_pdata->dimming_init = mdss_dsi_panel_dimming_init;
	ctrl_pdata->event_handler = panel_event_handler;
	ctrl_pdata->registered = mdss_dsi_panel_registered;
	if (pinfo->partial_update_enabled)
		ctrl_pdata->partial_update_fnc = mdss_dsi_panel_partial_update;
	ctrl_pdata->cmd_list.brightness_cmd.cmds = brightness_packet;

	mutex_init(&pdata->samsung_pdata.lock);

#if defined(CONFIG_LCD_CLASS_DEVICE)
	rc = mdss_samsung_create_sysfs(s_pdata);
	if (rc) {
		pr_err("Failed to create sysfs for lcd driver..\n");
		return rc;
	}
#endif

	return 0;
}
