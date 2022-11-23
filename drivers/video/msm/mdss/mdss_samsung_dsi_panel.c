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
#include <linux/leds.h>
#include <linux/qpnp/pwm.h>
#include <linux/err.h>
#include <linux/lcd.h>

#include "mdss_dsi.h"
#include "mdss_samsung_dsi_panel.h"
#include "mdss_fb.h"

#if defined(CONFIG_MDNIE_LITE_TUNING)
#include "mdnie_lite_tuning.h"
static struct dsi_cmd nv_mdnie_read_cmds;
#endif

#define DDI_VIDEO_ENHANCE_TUNING
#if defined(DDI_VIDEO_ENHANCE_TUNING)
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#endif
#if defined(CONFIG_LCD_CLASS_DEVICE) && defined(DDI_VIDEO_ENHANCE_TUNING)
#define MAX_FILE_NAME 128
#define TUNING_FILE_PATH "/sdcard/"
#define TUNE_FIRST_SIZE 5
#define TUNE_SECOND_SIZE 108

static char tuning_file[MAX_FILE_NAME];
static char mdni_tuning1[TUNE_FIRST_SIZE];
static char mdni_tuning2[TUNE_SECOND_SIZE];

static struct dsi_cmd_desc mdni_tune_cmd[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(mdni_tuning2)}, mdni_tuning2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(mdni_tuning1)}, mdni_tuning1},
};
#endif
int lcd_attached;
int get_samsung_lcd_attached(void);

#define DT_CMD_HDR 6
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_QHD_VIDEO_PT_PANEL)
#define HBM_RE
#define TEMPERATURE_ELVSS
/* #define CMD_DEBUG */

static int is_panel_ams485dl01_revA;
static int is_panel_ams485dl01_revC;
static int is_panel_ams485dl01_revD;
static int is_panel_ams452ef01_revA;
static int is_panel_ams452ef01_revC;
#endif

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_WVGA_VIDEO_PT_PANEL)
#define SMART_ACL
#endif

DEFINE_LED_TRIGGER(bl_led_trigger);

static struct dsi_buf dsi_panel_tx_buf;
static struct dsi_buf dsi_panel_rx_buf;
static struct dsi_cmd nv_mtp_read_cmds;
static struct dsi_cmd nv_mtp_read_register_set_cmds;


static struct dsi_cmd nv_enable_cmds;
static struct dsi_cmd nv_disable_cmds;

static struct dsi_cmd manufacture_id_cmds;
static struct dsi_cmd display_qcom_on_cmds;
static struct dsi_cmd display_qcom_off_cmds;
static struct dsi_cmd display_gamma_update_cmds;
static struct dsi_cmd display_on_cmd;
static struct dsi_cmd display_off_cmd;
static struct dsi_cmd display_blank_cmd;
static struct dsi_cmd display_unblank_cmd;
static struct dsi_cmd acl_off_cmd;
static struct dsi_cmd test_key_enable_cmds;
static struct dsi_cmd test_key_disable_cmds;
#if defined(HBM_RE)
static struct dsi_cmd nv_mtp_hbm_read_cmds;
static struct dsi_cmd nv_mtp_hbm2_read_cmds;
static struct dsi_cmd nv_mtp_hbm3_read_cmds;
static struct dsi_cmd hbm_gamma_cmds_list;
static struct dsi_cmd hbm_etc_cmds_list;
static struct dsi_cmd hbm_off_cmds;
#if defined(TEMPERATURE_ELVSS)
static struct dsi_cmd elvss_temp_cmds_hbm;
#endif
#endif
#if defined(TEMPERATURE_ELVSS)
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_QHD_VIDEO_PT_PANEL)
static struct dsi_cmd elvss_temp_cmds_revD;
#endif
static struct dsi_cmd elvss_temp_cmds;
#endif

/*
contains the list of acl commands
available for different brightness levels
*/
static struct dsi_cmd acl_cmds_list;
static struct dsi_cmd elvss_cmds_list;
static struct dsi_cmd aid_cmds_list;
static struct dsi_cmd gamma_cmds_list;

/*
contains mapping between bl_level and
index number of corresponding acl command
in acl command list
*/
static struct cmd_map elvss_map_table;
static struct cmd_map aid_map_table;
static struct cmd_map acl_map_table;
static struct candella_lux_map candela_map_table;

#if defined(octa_manufacture_date)
static struct dsi_cmd nv_date_read_cmds;
#endif

#if defined(SMART_ACL)
static struct dsi_cmd smart_acl_elvss_cmds_list;
static struct cmd_map smart_acl_elvss_map_table;
#endif
#if defined(CONFIG_ESD_RECOVERY)
#define ESD_DEBUG
struct work_struct err_fg_work;
static int esd_count = 0;
static int err_fg_working = 0;
static irqreturn_t err_fg_irq_handler(int irq, void *handle);
static void err_fg_work_func(struct work_struct *work);
#endif

static struct mipi_samsung_driver_data msd;
static struct  panel_hrev panel_supp_cdp[]= {
	{"Samsung qHD video mode dsi panel",PANEL_SAMSUNG_QHD_VIDEO},
	{"Samsung WVGA video mode dsi panel",PANEL_SAMSUNG_WVGA_VIDEO},
	{NULL}
};

static struct dsi_cmd_desc brightness_packet[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL},
};


#define MAX_BR_PACKET_SIZE sizeof(brightness_packet)/sizeof(struct dsi_cmd_desc)

DEFINE_LED_TRIGGER(bl_led_trigger);

static int mipi_samsung_disp_send_cmd(
		enum mipi_samsung_cmd_list cmd,
		unsigned char lock);
int set_panel_rev(unsigned int id)
{
	switch (id & 0xFF) {
			case 0x07:
				msd.panel_350cd = 0;
			break;
			case 0x00:
			case 0x01:
			case 0x02:
			case 0x03:
				msd.panel_350cd = 1;
			break;
			default:
			pr_err("%s : can't find panel id.. \n", __func__);
			return -EINVAL;
			break;
	}

	return 1;
}
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

	pr_info("%s: bklt_ctrl=%d pwm_period=%d pwm_gpio=%d pwm_lpg_chan=%d\n",
			__func__, ctrl->bklt_ctrl, ctrl->pwm_period,
				ctrl->pwm_pmic_gpio, ctrl->pwm_lpg_chan);

	pr_info("%s: ndx=%d level=%d duty=%d\n", __func__,
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

static void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_panel_cmds *pcmds)
{
	struct dcs_cmd_req cmdreq;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = pcmds->cmds;
	cmdreq.cmds_cnt = pcmds->cmd_cnt;
	cmdreq.flags = CMD_REQ_COMMIT;

	/*Panel ON/Off commands should be sent in DSI Low Power Mode*/
	if (pcmds->link_state == DSI_LP_MODE)
		cmdreq.flags  |= CMD_REQ_LP_MODE;

	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

static char led_pwm1[2] = {0x51, 0x0};	/* DTYPE_DCS_WRITE1 */
static struct dsi_cmd_desc backlight_cmd = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm1)},
	led_pwm1
};

static void mdss_dsi_panel_bklt_dcs(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	struct dcs_cmd_req cmdreq;

	pr_info("%s: level=%d\n", __func__, level);

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

#if !defined(CONFIG_FB_MSM_MIPI_SAMSUNG_WVGA_VIDEO_PT_PANEL)
	if (gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
		rc = gpio_request(ctrl_pdata->disp_en_gpio,
						"disp_enable");
		if (rc) {
			pr_err("request disp_en gpio failed, rc=%d\n",
				       rc);
			goto disp_en_gpio_err;
		}
	}
#endif
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
	return rc;

mode_gpio_err:
	if (gpio_is_valid(ctrl_pdata->bklt_en_gpio))
		gpio_free(ctrl_pdata->bklt_en_gpio);
bklt_en_gpio_err:
	gpio_free(ctrl_pdata->rst_gpio);
rst_gpio_err:
	if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
		gpio_free(ctrl_pdata->disp_en_gpio);
#if !defined(CONFIG_FB_MSM_MIPI_SAMSUNG_WVGA_VIDEO_PT_PANEL)
disp_en_gpio_err:
#endif
	return rc;
}

int mdss_dsi_samsung_panel_reset(struct mdss_panel_data *pdata, int enable)
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


	pr_info("%s: enable = %d\n", __func__, enable);
	pinfo = &(ctrl_pdata->panel_data.panel_info);

	if (enable) {
		rc = mdss_dsi_request_gpios(ctrl_pdata);
		if (rc) {
			pr_err("gpio request failed\n");
			return rc;
		}
#if !defined(CONFIG_FB_MSM_MIPI_SAMSUNG_WVGA_VIDEO_PT_PANEL)
		if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
			gpio_set_value((ctrl_pdata->disp_en_gpio), 1);
		usleep_range(5000, 5000);
#endif
		if (gpio_is_valid(ctrl_pdata->bklt_en_gpio))
			gpio_set_value((ctrl_pdata->bklt_en_gpio), 1);

		for (i = 0; i < pdata->panel_info.rst_seq_len; ++i) {
			gpio_set_value((ctrl_pdata->rst_gpio),
				pdata->panel_info.rst_seq[i]);
			if (pdata->panel_info.rst_seq[++i])
				usleep(pinfo->rst_seq[i] * 1000);
		}
		if (gpio_is_valid(ctrl_pdata->mode_gpio)) {
			if (pinfo->mode_gpio_state == MODE_GPIO_HIGH)
				gpio_set_value((ctrl_pdata->mode_gpio), 1);
			else if (pinfo->mode_gpio_state == MODE_GPIO_LOW)
				gpio_set_value((ctrl_pdata->mode_gpio), 0);
		}
		if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
			pr_info("%s: Panel Not properly turned OFF\n",
						__func__);
			ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
			pr_info("%s: Reset panel done\n", __func__);
		}
	} else {
		if (gpio_is_valid(ctrl_pdata->bklt_en_gpio)) {
			gpio_set_value((ctrl_pdata->bklt_en_gpio), 0);
			gpio_free(ctrl_pdata->bklt_en_gpio);
		}
		if (gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
			gpio_set_value((ctrl_pdata->disp_en_gpio), 0);
			gpio_free(ctrl_pdata->disp_en_gpio);
		}
		if (gpio_is_valid(ctrl_pdata->rst_gpio)) {
			gpio_set_value((ctrl_pdata->rst_gpio), 0);
			gpio_free(ctrl_pdata->rst_gpio);
		}
		if (gpio_is_valid(ctrl_pdata->mode_gpio))
			gpio_free(ctrl_pdata->mode_gpio);
	}
	return rc;
}
unsigned char get_auto_brightness(void)
{
	return  msd.dstat.auto_brightness;
}
static int get_candela_value(int bl_level)
{
		return candela_map_table.lux_tab[candela_map_table.bkl[bl_level]];
}

static int get_cmd_idx(int bl_level)
{
		return candela_map_table.cmd_idx[candela_map_table.bkl[bl_level]];
}

/*
	This function takes aid_map_table and uses cd_idx,
	to get the index of the command in aid command list.

*/

static struct dsi_cmd get_aid_aor_control_set(int cd_idx)
{
	struct dsi_cmd aid_control = {0,};
	int cmd_idx = 0, payload_size = 0;
	char *p_payload, *c_payload;
	int p_idx = msd.dstat.curr_aid_idx;

	if (!aid_map_table.size || !(cd_idx < aid_map_table.size))
		goto end;

	/* Get index in the aid command list*/
	cmd_idx = aid_map_table.cmd_idx[cd_idx];
	c_payload = aid_cmds_list.cmd_desc[cmd_idx].payload;

	/* Check if current & previous commands are same */
	if (p_idx >= 0) {
			p_payload = aid_cmds_list.cmd_desc[p_idx].payload;
			payload_size = aid_cmds_list.cmd_desc[p_idx].dchdr.dlen;
		if (!memcmp(p_payload, c_payload, payload_size))
			goto end;
	}

	/* Get the command desc */
		aid_control.cmd_desc = &(aid_cmds_list.cmd_desc[cmd_idx]);
	aid_control.num_of_cmds = 1;
	msd.dstat.curr_aid_idx = cmd_idx;

	switch (msd.panel) {
	case PANEL_SAMSUNG_QHD_VIDEO:
	case PANEL_SAMSUNG_WVGA_VIDEO:
		/* Do any panel specfific customization here */
		break;
	}
end:
	return aid_control;
}


/*
	This function takes acl_map_table and uses cd_idx,
	to get the index of the command in elvss command list.

*/
static struct dsi_cmd get_acl_control_set(int cd_idx)
{
	struct dsi_cmd acl_control = {0,};
	int cmd_idx = 0, payload_size = 0;
	char *p_payload, *c_payload;
	int p_idx = msd.dstat.curr_acl_idx;

	if (!acl_map_table.size || !(cd_idx < acl_map_table.size))
		goto end;

	/* Get index in the acl command list*/
	cmd_idx = acl_map_table.cmd_idx[cd_idx];
	c_payload = acl_cmds_list.cmd_desc[cmd_idx].payload;

	/* Check if current & previous commands are same */
	if (p_idx >= 0) {
		p_payload = acl_cmds_list.cmd_desc[p_idx].payload;
		payload_size = acl_cmds_list.cmd_desc[p_idx].dchdr.dlen;
		if (!memcmp(p_payload, c_payload, payload_size))
			goto end;
	}

	/* Get the command desc */
	acl_control.cmd_desc = &(acl_cmds_list.cmd_desc[cmd_idx]);
	acl_control.num_of_cmds = 1;
	msd.dstat.curr_acl_idx = cmd_idx;
	switch (msd.panel) {
	case PANEL_SAMSUNG_QHD_VIDEO:
	case PANEL_SAMSUNG_WVGA_VIDEO:
		/* Do any panel specfific customization here */
		break;
	}
end:
	return acl_control;
}

static struct dsi_cmd get_acl_control_off_set(void)
{
	struct dsi_cmd acl_control = {0,};
	int p_idx = msd.dstat.curr_acl_idx;

	/* Check if current & previous commands are same */
	if (p_idx == 0) {
		/* already acl off */
		goto end;
	}

	/* Get the command desc */
	acl_control.cmd_desc = &(acl_cmds_list.cmd_desc[0]); /* idx 0 : ACL OFF */
	acl_control.num_of_cmds = 1;

	msd.dstat.curr_acl_idx = 0;
	msd.dstat.curr_acl_cond = 0;

end:
	return acl_control;
}


/*
	This function takes acl_map_table and uses cd_idx,
	to get the index of the command in elvss command list.

*/
#if defined(SMART_ACL)
static struct dsi_cmd get_elvss_control_set(int cd_idx)
{
	struct dsi_cmd elvss_control = {0,};
	int cmd_idx = 0;
	char *payload;

	pr_info("%s for SMART_ACL\n",__func__);

	if (!elvss_map_table.size || !(cd_idx < elvss_map_table.size) ||
			!smart_acl_elvss_map_table.size ||
			!(cd_idx < smart_acl_elvss_map_table.size)) {
		pr_err("%s failed mapping elvss table\n",__func__);
		goto end;
	}

	/* Get the command desc */
	if(msd.dstat.acl_on || msd.dstat.siop_status) {
		cmd_idx = smart_acl_elvss_map_table.cmd_idx[cd_idx];
		payload = smart_acl_elvss_cmds_list.cmd_desc[cmd_idx].payload;
		elvss_control.cmd_desc = &(smart_acl_elvss_cmds_list.cmd_desc[cmd_idx]);
		pr_info("ELVSS for SMART_ACL cd_idx=%d, cmd_idx=%d\n", cd_idx, cmd_idx);
	} else {
		cmd_idx = elvss_map_table.cmd_idx[cd_idx];
		payload = elvss_cmds_list.cmd_desc[cmd_idx].payload;
		elvss_control.cmd_desc = &(elvss_cmds_list.cmd_desc[cmd_idx]);
		pr_info("ELVSS for normal cd_idx=%d, cmd_idx=%d\n", cd_idx, cmd_idx);
	}

	elvss_control.num_of_cmds = 1;
	msd.dstat.curr_elvss_idx = cmd_idx;

end:
	return elvss_control;

}

#else
static struct dsi_cmd get_elvss_control_set(int cd_idx)
{
	struct dsi_cmd elvss_control = {0,};
	int cmd_idx = 0;

	if (!elvss_map_table.size || !(cd_idx < elvss_map_table.size))
	{
		pr_err("%s invalid elvss mapping \n",__func__);
		goto end;
	}

	/* Get index in the acl command list*/
	cmd_idx = elvss_map_table.cmd_idx[cd_idx];

	/* Get the command desc */

	elvss_control.cmd_desc = &(elvss_cmds_list.cmd_desc[cmd_idx]);

	elvss_control.num_of_cmds = 1;
	msd.dstat.curr_elvss_idx = cmd_idx;

end:
	return elvss_control;
}
#endif
#if defined(TEMPERATURE_ELVSS)
static struct dsi_cmd get_elvss_temprature_set(void)
{
    struct dsi_cmd elvss_temperature = {0,};

    if (msd.dstat.temper_need_update) {
        msd.dstat.temper_need_update = 0;
		if (is_panel_ams452ef01_revA || is_panel_ams452ef01_revC) {
			elvss_temp_cmds.cmd_desc[2].payload[1] =
                msd.dstat.temperature_value;
		} else {
			elvss_temp_cmds.cmd_desc[1].payload[1] =
                msd.dstat.temperature_value;
		}
    }
    elvss_temperature.cmd_desc = &(elvss_temp_cmds.cmd_desc[0]);
    elvss_temperature.num_of_cmds = elvss_temp_cmds.num_of_cmds;
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_QHD_VIDEO_PT_PANEL)
	if(is_panel_ams485dl01_revD){
		if(msd.dstat.temperature_value >= 0x94){
			elvss_temp_cmds_revD.cmd_desc[1].payload[1] =
				msd.dstat.temperature_value;
			elvss_temperature.cmd_desc = &(elvss_temp_cmds_revD.cmd_desc[0]);
			elvss_temperature.num_of_cmds = elvss_temp_cmds_revD.num_of_cmds;
		}
	}
#endif

    return elvss_temperature;
}
#endif
static struct dsi_cmd get_gamma_control_set(int candella)
{
	struct dsi_cmd gamma_control = {0, 0, 0, 0, 0};
	/* Just a safety check to ensure smart dimming data is initialised well */
	BUG_ON(msd.sdimconf->generate_gamma == NULL);
#ifdef CMD_DEBUG
	for (i = 0; i < 3; i++)
	{
		for (j = 0; j < gamma_cmds_list.cmd_desc[0].dchdr.dlen; j++)
			pr_info("%x ",gamma_cmds_list.cmd_desc[0].payload[j]);
		pr_info("\n");
	}
#endif
	msd.sdimconf->generate_gamma(candella, &gamma_cmds_list.cmd_desc[0].payload[1]);
	gamma_control.cmd_desc = &(gamma_cmds_list.cmd_desc[0]);
	gamma_control.num_of_cmds = gamma_cmds_list.num_of_cmds;
	return gamma_control;
}


static int update_bright_packet(int cmd_count, struct dsi_cmd *cmd_set)
{
	int i = 0;
	if (cmd_count > (MAX_BR_PACKET_SIZE - 1)) /*cmd_count is index, if cmd_count >12 then panic*/
		panic("over max brightness_packet size(%d).. !!", MAX_BR_PACKET_SIZE);
	for (i = 0; i < cmd_set->num_of_cmds; i++) {
		brightness_packet[cmd_count].payload = \
			cmd_set->cmd_desc[i].payload;
		pr_debug("%s:  cmd_count: %d",__func__,cmd_count );
		brightness_packet[cmd_count].dchdr.dlen = \
			cmd_set->cmd_desc[i].dchdr.dlen;
		brightness_packet[cmd_count].dchdr.dtype = \
		cmd_set->cmd_desc[i].dchdr.dtype;
		brightness_packet[cmd_count].dchdr.wait = \
		cmd_set->cmd_desc[i].dchdr.wait;
		brightness_packet[cmd_count].dchdr.last = \
		cmd_set->cmd_desc[i].dchdr.last;
		cmd_count++;
	}
#ifdef CMD_DEBUG
	for (i = 0; i < 3; i++)
	{
		for (j = 0; j < brightness_packet[0].dchdr.dlen; j++)
			pr_info("%x ",brightness_packet[0].payload[j]);
		pr_info("\n");
	}
#endif
	return cmd_count;
}

static struct dsi_cmd get_testKey_set(int enable)/*F0 or F0 F1*/
{
	struct dsi_cmd testKey = {0,};

	if (enable)
		testKey.cmd_desc = &(test_key_enable_cmds.cmd_desc[0]);
	else
		testKey.cmd_desc = &(test_key_disable_cmds.cmd_desc[0]);

	testKey.num_of_cmds = test_key_disable_cmds.num_of_cmds;

	return testKey;
}

#if defined(HBM_RE)
static struct dsi_cmd get_hbm_etc_control_set(void)
{
	struct dsi_cmd etc_hbm_control = {0,};
	etc_hbm_control.cmd_desc = &(hbm_etc_cmds_list.cmd_desc[0]);
	etc_hbm_control.num_of_cmds = hbm_etc_cmds_list.num_of_cmds;

	return etc_hbm_control;
}

static struct dsi_cmd get_hbm_gamma_control_set(void)
{
	struct dsi_cmd gamma_hbm_control = {0,};

	/* Get the command desc */
	gamma_hbm_control.cmd_desc = &(hbm_gamma_cmds_list.cmd_desc[0]);
	gamma_hbm_control.num_of_cmds = hbm_gamma_cmds_list.num_of_cmds;

	return gamma_hbm_control;
}

static int make_brightcontrol_hbm_set(int bl_level)
{

	struct dsi_cmd hbm_etc_control = {0,};
	struct dsi_cmd gamma_control = {0,};
	struct dsi_cmd testKey = {0,};

	int cmd_count = 0;

	if (msd.dstat.hbm_mode) {
		pr_err("%s : already hbm mode! return .. \n", __func__);
		return 0;
	}

	testKey = get_testKey_set(1);
	cmd_count = update_bright_packet(cmd_count, &testKey);

	/*gamma*/
	gamma_control = get_hbm_gamma_control_set();
	cmd_count = update_bright_packet(cmd_count, &gamma_control);

	hbm_etc_control = get_hbm_etc_control_set();
	cmd_count = update_bright_packet(cmd_count, &hbm_etc_control);

	testKey = get_testKey_set(0);
	cmd_count = update_bright_packet(cmd_count, &testKey);

	/* for non hbm mode : reset */
	msd.dstat.curr_elvss_idx = -1;
	msd.dstat.curr_acl_idx = -1;
	msd.dstat.curr_aid_idx = -1;
	msd.dstat.curr_acl_cond = 0;

	pr_info("bright_level: HBM %d\n", bl_level);
	return cmd_count;

}
#endif

static int make_brightcontrol_set(int bl_level)
{
	struct dsi_cmd elvss_control = {0, 0, 0, 0, 0};
	struct dsi_cmd acl_control = {0, 0, 0, 0, 0};
	struct dsi_cmd aid_control = {0, 0, 0, 0, 0};
	struct dsi_cmd gamma_control = {0, 0, 0, 0, 0};
	struct dsi_cmd testKey = {0, 0, 0, 0, 0};
#if defined(TEMPERATURE_ELVSS)
	struct dsi_cmd elvss_temp_control = {0, 0, 0, 0, 0};
#endif
	int cmd_count = 0, cd_idx = 0, cd_level =0;

	testKey = get_testKey_set(1);
	cmd_count = update_bright_packet(cmd_count, &testKey);

	cd_idx = get_cmd_idx(bl_level);
	cd_level = get_candela_value(bl_level);
	pr_debug("%s cmd_index : %d candela : %d backlight : %d", __func__, cd_idx, cd_level, bl_level);

	/* aid/aor */
	aid_control = get_aid_aor_control_set(cd_idx);
	cmd_count = update_bright_packet(cmd_count, &aid_control);

	/*gamma*/
	gamma_control = get_gamma_control_set(cd_level);
	cmd_count = update_bright_packet(cmd_count, &gamma_control);

	/*elvss*/
	elvss_control = get_elvss_control_set(cd_idx);
	cmd_count = update_bright_packet(cmd_count, &elvss_control);

#if defined(TEMPERATURE_ELVSS)
	elvss_temp_control = get_elvss_temprature_set();
	cmd_count = update_bright_packet(cmd_count, &elvss_temp_control);
#endif
	/* acl */
	if (msd.dstat.acl_on||msd.dstat.siop_status) {
		acl_control = get_acl_control_set(cd_idx);
		cmd_count = update_bright_packet(cmd_count, &acl_control);
	} else {
		/* acl off (hbm off) */
		acl_control = get_acl_control_off_set();
		cmd_count = update_bright_packet(cmd_count, &acl_control);
	}

	testKey = get_testKey_set(0);
	cmd_count = update_bright_packet(cmd_count, &testKey);

	pr_info("bright_level: %d, candela_idx: %d( %d cd ), "\
		"cmd_count(aid,acl,acl_ctrl,elvss,gamma)::(%d,%d,%d,%d)%d\n",
		msd.dstat.bright_level, cd_idx, cd_level,
		aid_control.num_of_cmds,
		acl_control.num_of_cmds,
		elvss_control.num_of_cmds,
		gamma_control.num_of_cmds,
		cmd_count);
	return cmd_count;

}

void mdss_dsi_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_cmd_desc *cmds, int cnt,int flag)
{
	struct dcs_cmd_req cmdreq;

	memset(&cmdreq, 0, sizeof(cmdreq));

	if (flag) {
		cmdreq.flags = CMD_CLK_CTRL | CMD_REQ_COMMIT;
	}else
		cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;

	cmdreq.cmds = cmds;
	cmdreq.cmds_cnt = cnt;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;
	/*
	 * This mutex is to sync up with dynamic FPS changes
	 * so that DSI lockups shall not happen
	 */
	BUG_ON(msd.ctrl_pdata == NULL);
//	mutex_lock(&msd.ctrl_pdata->dfps_mutex);
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
//	mutex_unlock(&msd.ctrl_pdata->dfps_mutex);
}

u32 mdss_dsi_cmd_receive(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_cmd_desc *cmd, int rlen)
{
        struct dcs_cmd_req cmdreq;

        memset(&cmdreq, 0, sizeof(cmdreq));
        cmdreq.cmds = cmd;
        cmdreq.cmds_cnt = 1;
        cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
        cmdreq.rbuf = ctrl->rx_buf.data;
        cmdreq.rlen = rlen;
        cmdreq.cb = NULL; /* call back */
        /*
    	 * This mutex is to sync up with dynamic FPS changes
    	 * so that DSI lockups shall not happen
    	 */
      	BUG_ON(msd.ctrl_pdata == NULL);
 //   	mutex_lock(&msd.ctrl_pdata->dfps_mutex);
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
//        mutex_unlock(&msd.ctrl_pdata->dfps_mutex);
        /*
         * blocked here, untill call back called
         */
        return ctrl->rx_buf.len;
}

static int samsung_nv_read(struct dsi_cmd_desc *desc, char *destBuffer,
		int srcLength, struct mdss_panel_data *pdata, int startoffset)
{
	int loop_limit = 0;
	int read_pos = startoffset;
	int read_count = 0;
	int show_cnt;
	int i, j;
	char show_buffer[256];
	int show_buffer_pos = 0;
	int read_size = 0;

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

	show_buffer_pos +=
		snprintf(show_buffer, 256, "read_reg : %X[%d] : ",
		desc[0].payload[0], srcLength);

	loop_limit = (srcLength + packet_size[0] - 1)
				/ packet_size[0];
	mdss_dsi_cmds_send(msd.ctrl_pdata, &(s6e8aa0_packet_size_cmd), 1, 0);
	show_cnt = 0;

	for (j = 0; j < loop_limit; j++) {
		reg_read_pos[1] = read_pos;
		read_size = ((srcLength - read_pos + startoffset) < packet_size[0]) ?
					(srcLength - read_pos + startoffset) : packet_size[0];

		mdss_dsi_cmds_send(msd.ctrl_pdata, &(s6e8aa0_read_pos_cmd), 1, 0);

		read_count = mdss_dsi_cmd_receive(msd.ctrl_pdata, desc, read_size);

		for (i = 0; i < read_count; i++, show_cnt++) {
			show_buffer_pos += snprintf(show_buffer +
						show_buffer_pos, 255, "%02x ",
						msd.ctrl_pdata->rx_buf.data[i]);
			if (destBuffer != NULL && show_cnt < srcLength) {
					destBuffer[show_cnt] =
					msd.ctrl_pdata->rx_buf.data[i];
			}
		}
		show_buffer_pos += snprintf(show_buffer +
				show_buffer_pos, 256, ".");
		read_pos += read_count;

		if (read_pos-startoffset >= srcLength)
			break;
	}

	pr_info("%s\n", show_buffer);
	return read_pos-startoffset;
}

static int mipi_samsung_read_nv_mem(struct mdss_panel_data *pdata, struct dsi_cmd *nv_read_cmds, char *buffer)
{
	int nv_size = 0;
	int nv_read_cnt = 0;
	int i = 0, j = 3;

	for (i = 0; i < nv_read_cmds->num_of_cmds; i++)
		nv_size += nv_read_cmds->read_size[i];
	pr_debug("nv_size= %d, nv_read_cmds->num_of_cmds = %d", nv_size, nv_read_cmds->num_of_cmds);
	for (i = 0; i < nv_read_cmds->num_of_cmds; i++) {
		int count = 0;
		int read_size = nv_read_cmds->read_size[i];
		int read_startoffset = nv_read_cmds->read_startoffset[i];
		do {
			count = samsung_nv_read(&(nv_read_cmds->cmd_desc[i]),
					&buffer[nv_read_cnt], read_size, pdata, read_startoffset);
			if (j++ == 5)
				break;
		} while(buffer[0] != 0x0 && buffer[0] != 0x1);
		nv_read_cnt += count;
		if (count != read_size)
			pr_err("Error reading LCD NV data !!!!\n");
	}

	return nv_read_cnt;
}


static unsigned int mipi_samsung_manufacture_id(struct mdss_panel_data *pdata)
{
	struct dsi_buf *rp, *tp;

	unsigned int id = 0 ;

	if (!manufacture_id_cmds.num_of_cmds)
		return 0;

	tp = &dsi_panel_tx_buf;
	rp = &dsi_panel_rx_buf;

	mdss_dsi_cmd_receive(msd.ctrl_pdata,
		&manufacture_id_cmds.cmd_desc[0],
		manufacture_id_cmds.read_size[0]);

	pr_info("%s: manufacture_id1=%x\n", __func__, *msd.ctrl_pdata->rx_buf.data);

	id = (*((unsigned int *)msd.ctrl_pdata->rx_buf.data) & 0xFF);
	id <<= 8;

	mdss_dsi_cmd_receive(msd.ctrl_pdata,
		&manufacture_id_cmds.cmd_desc[1],
		manufacture_id_cmds.read_size[1]);
	pr_info("%s: manufacture_id2=%x\n", __func__, *msd.ctrl_pdata->rx_buf.data);
	id |= (*((unsigned int *)msd.ctrl_pdata->rx_buf.data) & 0xFF);
	id <<= 8;

	mdss_dsi_cmd_receive(msd.ctrl_pdata,
		&manufacture_id_cmds.cmd_desc[2],
		manufacture_id_cmds.read_size[2]);
	pr_info("%s: manufacture_id3=%x\n", __func__, *msd.ctrl_pdata->rx_buf.data);
	id |= (*((unsigned int *)msd.ctrl_pdata->rx_buf.data) & 0xFF);
	gv_manufacture_id = id;

	pr_info("%s: manufacture_id=%x\n", __func__, id);
	return id;
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

static struct mdss_dsi_ctrl_pdata *get_rctrl_data(struct mdss_panel_data *pdata)
{
	if (!pdata || !pdata->next) {
		pr_err("%s: Invalid panel data\n", __func__);
		return NULL;
	}

	return container_of(pdata->next, struct mdss_dsi_ctrl_pdata,
			panel_data);
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

static void mdss_dsi_panel_bl_ctrl(struct mdss_panel_data *pdata,
							u32 bl_level)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	if (msd.mfd->resume_state != MIPI_RESUME_STATE)
		return;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	/*
	 * Some backlight controllers specify a minimum duty cycle
	 * for the backlight brightness. If the brightness is less
	 * than it, the controller can malfunction.
	 */
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

		 if(msd.dstat.is_smart_dim_loaded == false){
			pr_info("%s: *********** smart dimming init not done **********\n",__func__);
			return;
		 	}
		msd.dstat.bright_level = bl_level;
		mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);
		if (ctrl_pdata->shared_pdata.broadcast_enable &&
				ctrl_pdata->ndx == DSI_CTRL_0) {
			struct mdss_dsi_ctrl_pdata *rctrl_pdata = NULL;
			rctrl_pdata = get_rctrl_data(pdata);
			if (!rctrl_pdata) {
				pr_err("%s: Right ctrl data NULL\n", __func__);
				return;
			}
			mdss_dsi_panel_bklt_dcs(rctrl_pdata, bl_level);
		}
		break;
	default:
		pr_info("%s: Unknown bl_ctrl configuration\n",
			__func__);
		break;
	}

}

static int mipi_samsung_disp_send_cmd(
		enum mipi_samsung_cmd_list cmd,
		unsigned char lock)
{
/* fix build error temporary */
/*	struct msm_fb_data_type *mfd = msd.mfd;*/
	struct dsi_cmd_desc *cmd_desc;
	int cmd_size = 0;
	int flag = 0;
#ifdef CMD_DEBUG
	int i,j;
#endif
	if (!get_samsung_lcd_attached())
		return 0;
	if (lock)
		mutex_lock(&msd.lock);

	switch (cmd) {
		case PANEL_NV_MTP_READ_REGISTER_SET_CMDS:
			cmd_desc = nv_mtp_read_register_set_cmds.cmd_desc;
			cmd_size = nv_mtp_read_register_set_cmds.num_of_cmds;
			break;
		case PANEL_READY_TO_ON:
			cmd_desc = display_qcom_on_cmds.cmd_desc;
			cmd_size = display_qcom_on_cmds.num_of_cmds;
			break;
		case PANEL_GAMMA_UPDATE:
			cmd_desc = display_gamma_update_cmds.cmd_desc;
			cmd_size = display_gamma_update_cmds.num_of_cmds;
			break;
		case PANEL_DISP_OFF:
			cmd_desc = display_qcom_off_cmds.cmd_desc;
			cmd_size = display_qcom_off_cmds.num_of_cmds;
			break;
		case PANEL_DISPLAY_ON:
			cmd_desc = display_on_cmd.cmd_desc;
			cmd_size = display_on_cmd.num_of_cmds;
			break;
		case PANEL_DISPLAY_OFF:
			cmd_desc = display_off_cmd.cmd_desc;
			cmd_size = display_off_cmd.num_of_cmds;
			break;
		case PANEL_DISPLAY_UNBLANK:
			cmd_desc = display_unblank_cmd.cmd_desc;
			cmd_size = display_unblank_cmd.num_of_cmds;
			break;
		case PANEL_DISPLAY_BLANK:
			cmd_desc = display_blank_cmd.cmd_desc;
			cmd_size = display_blank_cmd.num_of_cmds;
			break;
		case PANEL_BRIGHT_CTRL:
			if(msd.dstat.bright_level == -1) {
				if (lock)
					mutex_unlock(&msd.lock);
				return 0;
			}
			cmd_desc = brightness_packet;
			if (msd.dstat.bright_level)
				msd.dstat.recent_bright_level = msd.dstat.bright_level;
#if defined(HBM_RE)
			if (msd.dstat.auto_brightness == 6) {
				cmd_size = make_brightcontrol_hbm_set(msd.dstat.bright_level);
				msd.dstat.hbm_mode = 1;
			} else {
				if(msd.dstat.hbm_mode == 1) {
					mdss_dsi_cmds_send(msd.ctrl_pdata, hbm_off_cmds.cmd_desc, hbm_off_cmds.num_of_cmds, flag);
					msd.dstat.hbm_mode = 0;
				}
				cmd_size = make_brightcontrol_set(msd.dstat.bright_level);
			}
#else
			cmd_size = make_brightcontrol_set(msd.dstat.bright_level);
#endif
			if (msd.mfd->resume_state != MIPI_RESUME_STATE) {
				pr_info("%s : panel is off state!!\n", __func__);
				goto unknown_command;
			}
			break;
		case PANEL_MTP_ENABLE:
			cmd_desc = nv_enable_cmds.cmd_desc;
			cmd_size = nv_enable_cmds.num_of_cmds;
			break;
		case PANEL_MTP_DISABLE:
			cmd_desc = nv_disable_cmds.cmd_desc;
			cmd_size = nv_disable_cmds.num_of_cmds;
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
			cmd_desc = acl_off_cmd.cmd_desc;
			cmd_size = acl_off_cmd.num_of_cmds;
			break;

#if defined(PARTIAL_UPDATE)
		case PANEL_PARTIAL_ON:
			cmd_desc = partialdisp_on_cmd.cmd_desc;
			cmd_size = partialdisp_on_cmd.num_of_cmds;
			break;

		case PANEL_PARTIAL_OFF:
			cmd_desc = partialdisp_off_cmd.cmd_desc;
			cmd_size = partialdisp_off_cmd.num_of_cmds;
			break;
#endif
#if defined(CONFIG_LCD_CLASS_DEVICE) && defined(DDI_VIDEO_ENHANCE_TUNING)
		case MDNIE_ADB_TEST:
			cmd_desc = mdni_tune_cmd;
			cmd_size = ARRAY_SIZE(mdni_tune_cmd);
			break;
#endif
#if defined(TEMPERATURE_ELVSS)
		case HBM_ELVSS_TEMPER:
			if (msd.dstat.temper_need_update) {
				msd.dstat.temper_need_update = 0;
				if (is_panel_ams452ef01_revA || is_panel_ams452ef01_revC) {
					elvss_temp_cmds.cmd_desc[2].payload[1] =
						msd.dstat.temperature_value;
				} else {
					elvss_temp_cmds.cmd_desc[1].payload[1] =
						msd.dstat.temperature_value;
				}
			}
			if (is_panel_ams452ef01_revA || is_panel_ams452ef01_revC) {
				cmd_desc = elvss_temp_cmds.cmd_desc;
				cmd_size = elvss_temp_cmds.num_of_cmds;
			} else {
				cmd_desc = elvss_temp_cmds_hbm.cmd_desc;
				cmd_size = elvss_temp_cmds_hbm.num_of_cmds;
			}
			break;
#endif
		default:
			pr_err("%s : unknown_command.. \n", __func__);
			goto unknown_command;
			;
	}

	if (!cmd_size) {
		pr_err("%s : cmd_size is zero!.. \n", __func__);
		goto unknown_command;
	}

#ifdef CMD_DEBUG
	for (i = 0; i < cmd_size; i++)
	{
		for (j = 0; j < cmd_desc[i].dchdr.dlen; j++)
			pr_info("%x ",cmd_desc[i].payload[j]);
		pr_info("\n");
	}
#endif


#ifdef MDP_RECOVERY
	if (!mdss_recovery_start)
		mdss_dsi_cmds_send(msd.ctrl_pdata, cmd_desc, cmd_size, flag);
	else
		pr_err ("%s : Can't send command during mdss_recovery_start\n", __func__);
#else
	mdss_dsi_cmds_send(msd.ctrl_pdata, cmd_desc, cmd_size, flag);
#endif

	if (lock)
		mutex_unlock(&msd.lock);

	return 0;

unknown_command:
	LCD_DEBUG("Undefined command\n");

	if (lock)
		mutex_unlock(&msd.lock);

	return -EINVAL;
}


static int mdss_dsi_panel_registered(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	if (pdata == NULL) {
			pr_err("%s: Invalid input data\n", __func__);
			return -EINVAL;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	msd.mfd = (struct msm_fb_data_type *)registered_fb[0]->par;
	msd.pdata = pdata;
	msd.ctrl_pdata = ctrl_pdata;
#if defined(CONFIG_MDNIE_LITE_TUNING)
	pr_info("[%s] CONFIG_MDNIE_LITE_TUNING ok ! init class called!\n",
		__func__);
	mdnie_lite_tuning_init(&msd);
#endif
	/* Set the initial state to Suspend until it is switched on */
	msd.mfd->resume_state = MIPI_SUSPEND_STATE;
	pr_info("%s:%d, Panel registered succesfully\n", __func__, __LINE__);
	return 0;
}

#if defined(octa_manufacture_date)
static void mipi_samsung_manufacture_date_read(struct mdss_panel_data *pdata)
{
	char date[2];
	int year, month, day;
	int manufacture_date;

	/* Read mtp (C8h 41,42th) for manufacture date */
	mipi_samsung_read_nv_mem(pdata, &nv_date_read_cmds, date);

	year = date[0] & 0xf0;
	year >>= 4;
	year += 2011; // 0 = 2011 year
	month = date[0] & 0x0f;
	day = date[1] & 0x1f;

	manufacture_date = year * 10000 + month * 100 + day;

	pr_info("manufacture_date = (%d) - year(%d) month(%d) day(%d)\n",
		manufacture_date, year, month, day);

	msd.manufacture_date = manufacture_date;
}
#endif

static int mdss_dsi_panel_dimming_init(struct mdss_panel_data *pdata)
{
#if defined(TEMPERATURE_ELVSS)
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_QHD_VIDEO_PT_PANEL)
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
#endif
#endif
	mipi_samsung_disp_send_cmd(PANEL_MTP_ENABLE, true);

	/* If the ID is not read yet, then read it*/
	if (!msd.manufacture_id)
		msd.manufacture_id = mipi_samsung_manufacture_id(pdata);

	if (!msd.dstat.is_smart_dim_loaded) {
#if defined(HBM_RE)
		char hbm_buffer[20] = {0,};
#endif
		switch (msd.panel) {
			case PANEL_SAMSUNG_QHD_VIDEO:
			case PANEL_SAMSUNG_WVGA_VIDEO:
			default:
				msd.sdimconf = smart_S6E88A_get_conf();
			break;
	}
	/* Just a safety check to ensure smart dimming data is initialised well */
	BUG_ON(msd.sdimconf == NULL);

	/* Set the mtp read buffer pointer and read the NVM value*/
	mipi_samsung_read_nv_mem(pdata, &nv_mtp_read_cmds, msd.sdimconf->mtp_buffer);

	/* Initialize smart dimming related	things here */

	/* lux_tab setting for 350cd */
	msd.sdimconf->lux_tab = &candela_map_table.lux_tab[0];
	msd.sdimconf->lux_tabsize = candela_map_table.lux_tab_size;
	msd.sdimconf->man_id = msd.manufacture_id;
	/* Just a safety check to ensure smart dimming data is initialised well */
	BUG_ON(msd.sdimconf->init == NULL);
	msd.sdimconf->init();
	msd.dstat.is_smart_dim_loaded = true;
#if defined(octa_manufacture_date)
	mipi_samsung_manufacture_date_read(pdata);
#endif
#if defined(HBM_RE)
	mipi_samsung_read_nv_mem(pdata, &nv_mtp_hbm_read_cmds, hbm_buffer);
	memcpy(&hbm_gamma_cmds_list.cmd_desc[0].payload[1], hbm_buffer, 6);
	memcpy(&hbm_etc_cmds_list.cmd_desc[3].payload[1], hbm_buffer+6, 1);
	mipi_samsung_read_nv_mem(pdata, &nv_mtp_hbm2_read_cmds, hbm_buffer);
	memcpy(&hbm_gamma_cmds_list.cmd_desc[0].payload[7], hbm_buffer, 3);
	mipi_samsung_read_nv_mem(pdata, &nv_mtp_hbm3_read_cmds, hbm_buffer);
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_QHD_VIDEO_PT_PANEL)
	if ((is_panel_ams452ef01_revC) || (is_panel_ams485dl01_revD)) {
		memcpy(&hbm_off_cmds.cmd_desc[2].payload[1], hbm_buffer, 1);
		memcpy(&hbm_gamma_cmds_list.cmd_desc[0].payload[10], hbm_buffer+1, 12);
	} else {
		memcpy(&hbm_off_cmds.cmd_desc[2].payload[1], hbm_buffer+16, 1);
		memcpy(&hbm_gamma_cmds_list.cmd_desc[0].payload[10], hbm_buffer+2, 12);
	}
#else
	memcpy(&hbm_off_cmds.cmd_desc[2].payload[1], hbm_buffer+16, 1);
	memcpy(&hbm_gamma_cmds_list.cmd_desc[0].payload[10], hbm_buffer+2, 12);
#endif
#endif
#if defined(TEMPERATURE_ELVSS)
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_QHD_VIDEO_PT_PANEL)
	if ((is_panel_ams452ef01_revC) || (is_panel_ams485dl01_revD)){
		memcpy(&elvss_temp_cmds.cmd_desc[3].payload[1], hbm_buffer, 1);
		memcpy(&elvss_temp_cmds_revD.cmd_desc[3].payload[1], hbm_buffer, 1);
		elvss_temp_cmds_revD.cmd_desc[3].payload[1] =
			elvss_temp_cmds_revD.cmd_desc[3].payload[1] - 0x02;
	} else {
		memcpy(&elvss_temp_cmds.cmd_desc[3].payload[1], hbm_buffer+16, 1);
	}
	if(is_panel_ams485dl01_revD){
		memcpy(&ctrl_pdata->on_cmds.cmds[10].payload[1], hbm_buffer, 1);
		if(msd.dstat.temperature_value >= 0x94)
			ctrl_pdata->on_cmds.cmds[10].payload[1] =
				ctrl_pdata->on_cmds.cmds[10].payload[1] - 0x02;
		ctrl_pdata->on_cmds.cmds[7].payload[1] = msd.dstat.temperature_value;
	} else if((is_panel_ams485dl01_revA) || (is_panel_ams485dl01_revC)) {
		memcpy(&ctrl_pdata->on_cmds.cmds[10].payload[1], hbm_buffer+16, 1);
		ctrl_pdata->on_cmds.cmds[7].payload[1] = msd.dstat.temperature_value;
	} else {
		memcpy(&ctrl_pdata->on_cmds.cmds[9].payload[1], hbm_buffer+16, 1);
		ctrl_pdata->on_cmds.cmds[6].payload[1] = msd.dstat.temperature_value;
	}
#endif
#endif
		/*
		 * Since dimming is loaded, we can assume that device is out of suspend state
		 * and can accept backlight commands.
		 */
		msd.mfd->resume_state = MIPI_RESUME_STATE;
	}
#if defined(CONFIG_MDNIE_LITE_TUNING)
	/* MDNIe tuning initialisation*/
	if (!msd.dstat.is_mdnie_loaded) {
		char temp[4] = {0, };
		int	x, y;
		mipi_samsung_read_nv_mem(pdata, &nv_mdnie_read_cmds, temp);
		x =  temp[0] << 8 | temp[1];	/* X */
		y = temp[2] << 8 | temp[3];	/* Y */
		coordinate_tunning(x, y);
		msd.dstat.is_mdnie_loaded = true;
	}
#endif
	mipi_samsung_disp_send_cmd(PANEL_MTP_DISABLE, true);
	return 0;
}

static int first_power_on;
static int mdss_dsi_panel_on(struct mdss_panel_data *pdata)
{
#if defined(CONFIG_ESD_RECOVERY)
	static int first_boot = 1;
	int rc = 0;
#endif
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_QHD_VIDEO_PT_PANEL)\
	|| defined(CONFIG_FB_MSM_MIPI_SAMSUNG_WVGA_VIDEO_PT_PANEL)
	mdss_dsi_set_tx_power_mode(1, pdata);
#endif

	if (!msd.manufacture_id) {
		msd.manufacture_id = mipi_samsung_manufacture_id(pdata);
		if (set_panel_rev(msd.manufacture_id) < 0)
			pr_err("%s : can't find panel id.. \n", __func__);
	}

	if (!msd.dstat.is_smart_dim_loaded)
		mdss_dsi_panel_dimming_init(pdata);

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_QHD_VIDEO_PT_PANEL)\
	|| defined(CONFIG_FB_MSM_MIPI_SAMSUNG_WVGA_VIDEO_PT_PANEL)
	mdss_dsi_set_tx_power_mode(0, pdata);
#endif

	if (ctrl->on_cmds.cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->on_cmds);

	msd.dstat.curr_elvss_idx = -1;
	msd.dstat.curr_acl_idx = -1;
	msd.dstat.curr_aid_idx = -1;
	msd.dstat.hbm_mode = 0;
	msd.dstat.on = 1;
	msd.mfd->resume_state = MIPI_RESUME_STATE;
#if defined(CONFIG_MDNIE_LITE_TUNING)
	is_negative_on();
#endif
	if (msd.dstat.auto_brightness) {
		/* nothing to do if auto brightness enable */
	} else if (msd.dstat.recent_bright_level) {
		msd.dstat.bright_level = msd.dstat.recent_bright_level;
		mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);
	} else
		mipi_samsung_disp_send_cmd(PANEL_GAMMA_UPDATE, true);
	
	/* To fix flicker issue during power on*/
	if(first_power_on == 0) {
		msd.dstat.bright_level = -1;
		first_power_on++;
	}

#if defined(CONFIG_ESD_RECOVERY)
	if (first_boot) {
		INIT_WORK(&err_fg_work, err_fg_work_func);
		rc = request_threaded_irq(gpio_to_irq(ctrl->esd_gpio),
			NULL, err_fg_irq_handler,  IRQF_TRIGGER_RISING | IRQF_ONESHOT, "esd_detect", NULL);
		if (rc)
			pr_err("%s : Failed to request_irq.:ret=%d", __func__, rc);

		disable_irq(gpio_to_irq(ctrl->esd_gpio));
		first_boot = 0;
	}
	pr_info("ESD gpio check %d & enable irq\n", gpio_get_value(ctrl->esd_gpio));
	enable_irq(gpio_to_irq(ctrl->esd_gpio));
#endif
	pr_info("%s:-\n", __func__);

	return 0;
}

static int mdss_dsi_panel_off(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);
#if defined(CONFIG_ESD_RECOVERY)
	if (!err_fg_working && msd.dstat.on) {
		pr_info("[seungbae] esd irq disable\n");
		disable_irq_nosync(gpio_to_irq(ctrl->esd_gpio));
		cancel_work_sync(&err_fg_work);
	}
#endif
	msd.dstat.on = 0;
	msd.mfd->resume_state = MIPI_SUSPEND_STATE;
	if (ctrl->off_cmds.cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->off_cmds);

	pr_info("%s:-\n", __func__);
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

static int mdss_samsung_parse_panel_cmd(struct device_node *np,
		struct dsi_cmd *commands, char *keystring)
{

		const char *data;
		int type, len = 0, i = 0;
		char *bp;
		struct dsi_ctrl_hdr *dchdr;
		int is_read = 0;

		data = of_get_property(np, keystring, &len);
		if (!data) {
			pr_err("%s:%d, Unable to read %s",
				__func__, __LINE__, keystring);
			return -ENOMEM;
		}

		commands->cmds_buff = kzalloc(sizeof(char) * len, GFP_KERNEL);
		if (!commands->cmds_buff)
			return -ENOMEM;

		memcpy(commands->cmds_buff, data, len);
		commands->cmds_len = len;

		/* scan dcs commands */
		bp = commands->cmds_buff;
		while (len > sizeof(*dchdr)) {
			dchdr = (struct dsi_ctrl_hdr *)bp;
			dchdr->dlen = ntohs(dchdr->dlen);

		if (dchdr->dlen >200)
			goto error2;

			bp += sizeof(*dchdr);
			len -= sizeof(*dchdr);
			bp += dchdr->dlen;
			len -= dchdr->dlen;
			commands->num_of_cmds++;

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
			pr_err("%s: dcs OFF command byte Error, len=%d", __func__, len);
			commands->cmds_len = 0;
			commands->num_of_cmds = 0;
			goto error2;
		}

		if (is_read) {
			/*
				Allocate an array which will store the number
				for bytes to read for each read command
			*/
			commands->read_size = kzalloc(sizeof(char) * \
					commands->num_of_cmds, GFP_KERNEL);
			if (!commands->read_size) {
				pr_err("%s:%d, Unable to read NV cmds",
					__func__, __LINE__);
				goto error2;
			}
			commands->read_startoffset = kzalloc(sizeof(char) * \
					commands->num_of_cmds, GFP_KERNEL);
			if (!commands->read_startoffset) {
				pr_err("%s:%d, Unable to read NV cmds",
					__func__, __LINE__);
				goto error1;
			}
		}

		commands->cmd_desc = kzalloc(commands->num_of_cmds
					* sizeof(struct dsi_cmd_desc),
						GFP_KERNEL);
		if (!commands->cmd_desc)
			goto error1;

		bp = commands->cmds_buff;
		len = commands->cmds_len;
		for (i = 0; i < commands->num_of_cmds; i++) {
			dchdr = (struct dsi_ctrl_hdr *)bp;
			len -= sizeof(*dchdr);
			bp += sizeof(*dchdr);
			commands->cmd_desc[i].dchdr = *dchdr;
			commands->cmd_desc[i].payload = bp;
			bp += dchdr->dlen;
			len -= dchdr->dlen;
			if (is_read)
			{
				commands->read_size[i] = *bp++;
				commands->read_startoffset[i] = *bp++;
				len -= 2;
			}
		}
		return 0;
error1:
	kfree(commands->read_size);
error2:
	kfree(commands->cmds_buff);

	return -EINVAL;

}

static int mdss_dsi_parse_dcs_cmds(struct device_node *np,
		struct dsi_panel_cmds *pcmds, char *cmd_key, char *link_key)
{
	const char *data;
	int blen = 0, len;
	char *buf, *bp;
	struct dsi_ctrl_hdr *dchdr;
	int i, cnt;

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
	}

	if (len != 0) {
		pr_err("%s: dcs_cmd=%x len=%d error!",
				__func__, buf[0], blen);
		goto exit_free;
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

	pr_info("%s: dcs_cmd=%x len=%d, cmd_cnt=%d link_state=%d\n", __func__,
		pcmds->buf[0], pcmds->blen, pcmds->cmd_cnt, pcmds->link_state);

	return 0;

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
		pr_info("%s:%d FBC panel enabled.\n", __func__, __LINE__);
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
		pr_info("%s:%d Panel does not support FBC.\n",
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
		pr_info("%s:%d, error reading %s, length found = %d\n",
			__func__, __LINE__, name, num);
	} else {
		rc = of_property_read_u32_array(np, name, tmp, num);
		if (rc)
			pr_info("%s:%d, error reading %s, rc = %d\n",
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

	pr_info("%s: dynamic switch feature enabled: %d", __func__,
		pinfo->mipi.dynamic_switch_enabled);

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
			pr_info("%s: incorrect pdest. Set Default\n",
				__func__);
			pinfo->pdest = DISPLAY_1;
		}
	} else {
		pr_info("%s: pdest not specified. Set Default\n",
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
			pr_info("%s: SUCCESS-> WLED TRIGGER register\n",
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
	rc = of_property_read_u32(np, "qcom,mdss-force-clk-lane-hs", &tmp);
	pinfo->mipi.force_clk_lane_hs = (!rc ? tmp : 0);

	mdss_dsi_parse_trigger(np, &(pinfo->mipi.mdp_trigger),
		"qcom,mdss-dsi-mdp-trigger");

	mdss_dsi_parse_trigger(np, &(pinfo->mipi.dma_trigger),
		"qcom,mdss-dsi-dma-trigger");

	mdss_dsi_parse_lane_swap(np, &(pinfo->mipi.dlane_swap));

	mdss_dsi_parse_fbc_params(np, pinfo);

	mdss_panel_parse_te_params(np, pinfo);

	mdss_dsi_parse_reset_seq(np, pinfo->rst_seq, &(pinfo->rst_seq_len),
		"qcom,mdss-dsi-reset-sequence");
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_QHD_VIDEO_PT_PANEL)
	if(is_panel_ams485dl01_revA || is_panel_ams485dl01_revC) {
		mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->on_cmds,
			"qcom,mdss-dsi-on-command-revA", "qcom,mdss-dsi-on-command-state");
		mdss_samsung_parse_panel_cmd(np, &elvss_cmds_list,
			"samsung,panel-elvss-cmds-list-350-revA");
		mdss_samsung_parse_panel_cmd(np, &aid_cmds_list,
			"samsung,panel-aid-cmds-list-350-revB");
		mdss_samsung_parse_panel_table(np, &aid_map_table,
			"samsung,panel-aid-map-table-350-revB");
		mdss_samsung_parse_panel_table(np, &elvss_map_table,
			"samsung,panel-elvss-map-table-350-revA");
	} else if(is_panel_ams485dl01_revD) {
		mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->on_cmds,
			"qcom,mdss-dsi-on-command-revD", "qcom,mdss-dsi-on-command-state");
		mdss_samsung_parse_panel_cmd(np, &elvss_cmds_list,
			"samsung,panel-elvss-cmds-list-350-revD");
		mdss_samsung_parse_panel_cmd(np, &aid_cmds_list,
			"samsung,panel-aid-cmds-list-350-revA");
		mdss_samsung_parse_panel_table(np, &aid_map_table,
			"samsung,panel-aid-map-table-350-revA");
		mdss_samsung_parse_panel_table(np, &elvss_map_table,
			"samsung,panel-elvss-map-table-350-revD");
	} else if(is_panel_ams452ef01_revC) {
		mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->on_cmds,
			"qcom,mdss-dsi-on-command-revC", "qcom,mdss-dsi-on-command-state");
		mdss_samsung_parse_panel_cmd(np, &elvss_cmds_list,
			"samsung,panel-elvss-cmds-list-350-revC");
		mdss_samsung_parse_panel_cmd(np, &aid_cmds_list,
			"samsung,panel-aid-cmds-list-350");
		mdss_samsung_parse_panel_table(np, &aid_map_table,
			"samsung,panel-aid-map-table-350");
		mdss_samsung_parse_panel_table(np, &elvss_map_table,
			"samsung,panel-elvss-map-table-350-revC");
	} else {
		mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->on_cmds,
			"qcom,mdss-dsi-on-command", "qcom,mdss-dsi-on-command-state");
		mdss_samsung_parse_panel_cmd(np, &elvss_cmds_list,
			"samsung,panel-elvss-cmds-list-350");
		mdss_samsung_parse_panel_cmd(np, &aid_cmds_list,
			"samsung,panel-aid-cmds-list-350");
		mdss_samsung_parse_panel_table(np, &aid_map_table,
			"samsung,panel-aid-map-table-350");
		mdss_samsung_parse_panel_table(np, &elvss_map_table,
			"samsung,panel-elvss-map-table-350-revC");
	}
#else
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->on_cmds,
		"qcom,mdss-dsi-on-command", "qcom,mdss-dsi-on-command-state");
	mdss_samsung_parse_panel_cmd(np, &elvss_cmds_list,
		"samsung,panel-elvss-cmds-list-350");
	mdss_samsung_parse_panel_cmd(np, &aid_cmds_list,
		"samsung,panel-aid-cmds-list-350");
	mdss_samsung_parse_panel_table(np, &aid_map_table,
		"samsung,panel-aid-map-table-350");
	mdss_samsung_parse_panel_table(np, &elvss_map_table,
		"samsung,panel-elvss-map-table-350");
#endif
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->off_cmds,
		"qcom,mdss-dsi-off-command", "qcom,mdss-dsi-off-command-state");

	mdss_samsung_parse_panel_cmd(np, &display_qcom_on_cmds,
		"qcom,mdss-dsi-on-command");

	mdss_samsung_parse_panel_cmd(np, &display_gamma_update_cmds,
		"qcom,mdss-dsi-gamma-update");

	mdss_samsung_parse_panel_cmd(np, &display_qcom_off_cmds,
		"qcom,mdss-dsi-off-command");

	mdss_samsung_parse_panel_cmd(np, &nv_mtp_read_cmds,
		"samsung,panel-nv-mtp-read-cmds");
	mdss_samsung_parse_panel_cmd(np, &nv_mtp_read_register_set_cmds,
		"samsung,panel-mtp-read-cmds");

	mdss_samsung_parse_panel_cmd(np, &nv_enable_cmds,
		"samsung,panel-nv-read-enable-cmds");
	mdss_samsung_parse_panel_cmd(np, &nv_disable_cmds,
		"samsung,panel-nv-read-disable-cmds");
	mdss_samsung_parse_panel_cmd(np, &manufacture_id_cmds,
		"samsung,panel-manufacture-id-read-cmds");

	mdss_samsung_parse_panel_cmd(np, &display_on_cmd,
		"qcom,panel-display-on-cmds");
	mdss_samsung_parse_panel_cmd(np, &display_off_cmd,
		"qcom,panel-display-off-cmds");
	mdss_samsung_parse_panel_cmd(np, &display_unblank_cmd,
		"qcom,panel-display-unblank-cmds");
	mdss_samsung_parse_panel_cmd(np, &display_blank_cmd,
		"qcom,panel-display-blank-cmds");
	mdss_samsung_parse_panel_cmd(np, &acl_off_cmd,
		"samsung,panel-acl-off-cmds");

	mdss_samsung_parse_panel_cmd(np, &acl_cmds_list,
		"samsung,panel-acl-cmds-list");
	mdss_samsung_parse_panel_cmd(np, &gamma_cmds_list,
		"samsung,panel-gamma-cmds-list");
#if defined(octa_manufacture_date)
	mdss_samsung_parse_panel_cmd(np, &nv_date_read_cmds,
		"samsung,panel-nv-mdate-read-cmds");
#endif
#if defined(HBM_RE)
	mdss_samsung_parse_panel_cmd(np, &nv_mtp_hbm_read_cmds,
		"samsung,panel-nv-mtp-read-hbm-cmds");
	mdss_samsung_parse_panel_cmd(np, &nv_mtp_hbm2_read_cmds,
		"samsung,panel-nv-mtp-read-hbm2-cmds");
	mdss_samsung_parse_panel_cmd(np, &hbm_gamma_cmds_list,
		"samsung,panel-gamma-hbm-cmds-list");
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_QHD_VIDEO_PT_PANEL)
	if (is_panel_ams452ef01_revC) {
		mdss_samsung_parse_panel_cmd(np, &nv_mtp_hbm3_read_cmds,
			"samsung,panel-nv-mtp-read-hbm3-cmds-revC");
		mdss_samsung_parse_panel_cmd(np, &hbm_etc_cmds_list,
			"samsung,panel-etc-hbm-cmds-revC");
		mdss_samsung_parse_panel_cmd(np, &hbm_off_cmds,
			"samsung,panel-hbm-off-cmds-revC");
	} else if (is_panel_ams485dl01_revD) {
		mdss_samsung_parse_panel_cmd(np, &nv_mtp_hbm3_read_cmds,
			"samsung,panel-nv-mtp-read-hbm3-cmds-revD");
		mdss_samsung_parse_panel_cmd(np, &hbm_etc_cmds_list,
			"samsung,panel-etc-hbm-cmds-revD");
		mdss_samsung_parse_panel_cmd(np, &hbm_off_cmds,
			"samsung,panel-hbm-off-cmds-revD");
	} else {
		mdss_samsung_parse_panel_cmd(np, &nv_mtp_hbm3_read_cmds,
			"samsung,panel-nv-mtp-read-hbm3-cmds");
		mdss_samsung_parse_panel_cmd(np, &hbm_etc_cmds_list,
			"samsung,panel-etc-hbm-cmds");
		mdss_samsung_parse_panel_cmd(np, &hbm_off_cmds,
			"samsung,panel-hbm-off-cmds");
	}
#else
	mdss_samsung_parse_panel_cmd(np, &nv_mtp_hbm3_read_cmds,
		"samsung,panel-nv-mtp-read-hbm3-cmds");
	mdss_samsung_parse_panel_cmd(np, &hbm_etc_cmds_list,
		"samsung,panel-etc-hbm-cmds");
	mdss_samsung_parse_panel_cmd(np, &hbm_off_cmds,
		"samsung,panel-hbm-off-cmds");
#endif
#if defined(TEMPERATURE_ELVSS)
	mdss_samsung_parse_panel_cmd(np, &elvss_temp_cmds_hbm,
		"samsung,panel-elvss-temp-cmds-hbm");
#endif
#endif
#if defined(TEMPERATURE_ELVSS)
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_QHD_VIDEO_PT_PANEL)
	if(is_panel_ams485dl01_revD)
		mdss_samsung_parse_panel_cmd(np, &elvss_temp_cmds_revD,
			"samsung,panel-elvss-temp-cmds-revD");
#endif
	mdss_samsung_parse_panel_cmd(np, &elvss_temp_cmds,
		"samsung,panel-elvss-temp-cmds");
#endif
	/* Process the mapping tables */

	mdss_samsung_parse_panel_table(np, &acl_map_table,
		"samsung,panel-acl-map-table");

	/* Process the lux value table */
	mdss_samsung_parse_candella_lux_mapping_table(np, &candela_map_table,
		"samsung,panel-candella-mapping-table-350");

	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->status_cmds,
		"qcom,mdss-dsi-panel-status-command","qcom,mdss-dsi-panel-status-command-state");
	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-status-value", &tmp);
	ctrl_pdata->status_value = (!rc ? tmp : 0);


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
#if defined(CONFIG_MDNIE_LITE_TUNING)
	mdss_samsung_parse_panel_cmd(np, &nv_mdnie_read_cmds,
					"samsung,panel-nv-mdnie-read-cmds");
#endif
	mdss_samsung_parse_panel_cmd(np, &test_key_enable_cmds,
		"samsung,panel-test-key-enable-cmds");
	mdss_samsung_parse_panel_cmd(np, &test_key_disable_cmds,
		"samsung,panel-test-key-disable-cmds");
#if defined(SMART_ACL)
	mdss_samsung_parse_panel_cmd(np, &smart_acl_elvss_cmds_list,
		"samsung,panel-smart-acl-elvss-cmds-list");
	mdss_samsung_parse_panel_table(np, &smart_acl_elvss_map_table,
		"samsung,panel-smart-acl-elvss-map-table");
#endif
	return 0;

error:
	return -EINVAL;
}
static int is_panel_supported(const char *panel_name)
{
	int i = 0;

	if (panel_name == NULL)
		return -EINVAL;

	while(panel_supp_cdp[i].name != NULL)	{
		if(!strcmp(panel_name,panel_supp_cdp[i].name))
			break;
		i++;
	}

	if( i < ARRAY_SIZE(panel_supp_cdp)) {
		memcpy(msd.panel_name, panel_name, MAX_PANEL_NAME_SIZE);
		msd.panel = panel_supp_cdp[i].panel_code;
		return 0;
	}
	return -EINVAL;
}

#if defined(CONFIG_LCD_CLASS_DEVICE)
#if defined(DDI_VIDEO_ENHANCE_TUNING)
static char char_to_dec(char data1, char data2)
{
	char dec;

	dec = 0;

	if (data1 >= 'a') {
		data1 -= 'a';
		data1 += 10;
	} else if (data1 >= 'A') {
		data1 -= 'A';
		data1 += 10;
	} else
		data1 -= '0';

	dec = data1 << 4;

	if (data2 >= 'a') {
		data2 -= 'a';
		data2 += 10;
	} else if (data2 >= 'A') {
		data2 -= 'A';
		data2 += 10;
	} else
		data2 -= '0';

	dec |= data2;

	return dec;
}
static void sending_tune_cmd(char *src, int len)
{
	int data_pos;
	int cmd_step;
	int cmd_pos;

	cmd_step = 0;
	cmd_pos = 0;

	for (data_pos = 0; data_pos < len;) {
		if (*(src + data_pos) == '0') {
			if (*(src + data_pos + 1) == 'x') {
				if (!cmd_step) {
					mdni_tuning1[cmd_pos] =
					char_to_dec(*(src + data_pos + 2),
							*(src + data_pos + 3));
				} else {
					mdni_tuning2[cmd_pos] =
					char_to_dec(*(src + data_pos + 2),
							*(src + data_pos + 3));
				}

				data_pos += 3;
				cmd_pos++;

				if (cmd_pos == TUNE_FIRST_SIZE && !cmd_step) {
					cmd_pos = 0;
					cmd_step = 1;
				}else if (cmd_pos == TUNE_SECOND_SIZE && cmd_step) {/*blocking overflow*/
					cmd_pos = 0;
					break;
				}
			} else
				data_pos++;
		} else {
			data_pos++;
		}
	}

	printk(KERN_INFO "\n");
	for (data_pos = 0; data_pos < TUNE_FIRST_SIZE ; data_pos++)
		printk(KERN_INFO "0x%x ", mdni_tuning1[data_pos]);
	printk(KERN_INFO "\n");
	for (data_pos = 0; data_pos < TUNE_SECOND_SIZE ; data_pos++)
		printk(KERN_INFO "0x%x ", mdni_tuning2[data_pos]);
	printk(KERN_INFO "\n");

	mipi_samsung_disp_send_cmd(PANEL_MTP_ENABLE, true);

	mipi_samsung_disp_send_cmd(MDNIE_ADB_TEST, true);

	mipi_samsung_disp_send_cmd(PANEL_MTP_DISABLE, true);

}

static void load_tuning_file(char *filename)
{
	struct file *filp;
	char *dp;
	long l;
	loff_t pos;
	int ret;
	mm_segment_t fs;

	pr_info("%s called loading file name : [%s]\n", __func__,
	       filename);

	fs = get_fs();
	set_fs(get_ds());

	filp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		printk(KERN_ERR "%s File open failed\n", __func__);
		return;
	}

	l = filp->f_path.dentry->d_inode->i_size;
	pr_info("%s Loading File Size : %ld(bytes)", __func__, l);

	dp = kmalloc(l + 10, GFP_KERNEL);
	if (dp == NULL) {
		pr_info("Can't not alloc memory for tuning file load\n");
		filp_close(filp, current->files);
		return;
	}
	pos = 0;
	memset(dp, 0, l);

	pr_info("%s before vfs_read()\n", __func__);
	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	pr_info("%s after vfs_read()\n", __func__);

	if (ret != l) {
		pr_info("vfs_read() filed ret : %d\n", ret);
		kfree(dp);
		filp_close(filp, current->files);
		return;
	}

	filp_close(filp, current->files);

	set_fs(fs);

	sending_tune_cmd(dp, l);

	kfree(dp);
}

int mdnie_adb_test;
void copy_tuning_data_from_adb(char *data1, char *data2);
static ssize_t tuning_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, MAX_FILE_NAME, "Tunned File Name : %s\n",
								tuning_file);

	mdnie_adb_test = 0;

	return ret;
}

static ssize_t tuning_store(struct device *dev,
			    struct device_attribute *attr, const char *buf,
			    size_t size)
{
	char *pt;
	memset(tuning_file, 0, sizeof(tuning_file));
	snprintf(tuning_file, MAX_FILE_NAME, "%s%s", TUNING_FILE_PATH, buf);

	pt = tuning_file;
	while (*pt) {
		if (*pt == '\r' || *pt == '\n') {
			*pt = 0;
			break;
		}
		pt++;
	}

	pr_info("%s:%s\n", __func__, tuning_file);

	load_tuning_file(tuning_file);

#if defined(CONFIG_MDNIE_LITE_TUNING)
	copy_tuning_data_from_adb(mdni_tuning2, mdni_tuning1);
#endif

	mdnie_adb_test = 1;

	return size;
}

static DEVICE_ATTR(tuning, 0664, tuning_show, tuning_store);
#endif
static ssize_t mipi_samsung_disp_get_power(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct msm_fb_data_type *mfd = msd.mfd;
	int rc;

	if (unlikely(!mfd))
		return -ENODEV;
	if (unlikely(mfd->key != MFD_KEY))
		return -EINVAL;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n", mfd->panel_power_on);
	pr_info("mipi_samsung_disp_get_power(%d)\n", mfd->panel_power_on);

	return rc;
}

static ssize_t mipi_samsung_disp_set_power(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
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

	return size;
}


static ssize_t mipi_samsung_disp_lcdtype_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char temp[20];
	if (msd.manufacture_id)
		snprintf(temp, 20, "SDC_%x\n",msd.manufacture_id);
	strlcat(buf, temp, 20);
	return strnlen(buf, 20);
}

static ssize_t mipi_samsung_disp_windowtype_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char temp[15];
	int id1, id2, id3;
	id1 = (msd.manufacture_id & 0x00FF0000) >> 16;
	id2 = (msd.manufacture_id & 0x0000FF00) >> 8;
	id3 = msd.manufacture_id & 0xFF;

	snprintf(temp, sizeof(temp), "%x %x %x\n",	id1, id2, id3);
	strlcat(buf, temp, 15);
	return strnlen(buf, 15);
}


static ssize_t mipi_samsung_disp_acl_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n", msd.dstat.acl_on);
	pr_info("acl status: %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_disp_acl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd = msd.mfd;
	int	acl_set;

	acl_set = msd.dstat.acl_on;
	if (sysfs_streq(buf, "1"))
		acl_set = true;
	else if (sysfs_streq(buf, "0"))
		acl_set = false;
	else
		pr_info("%s: Invalid argument!!", __func__);

	if ((mfd->panel_power_on)&&(msd.manufacture_id)) {
		//pr_info("%s: msd.manufacture_id is %d", __func__,msd.manufacture_id);
		if (acl_set && !(msd.dstat.acl_on||msd.dstat.siop_status)) {
			msd.dstat.acl_on = true;
			pr_info("%s: acl on  : acl %d, siop %d", __func__,
					msd.dstat.acl_on, msd.dstat.siop_status);
			mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);

		} else if (!acl_set && msd.dstat.acl_on && !msd.dstat.siop_status) {
			msd.dstat.acl_on = false;
			msd.dstat.curr_acl_idx = -1;
			pr_info("%s: acl off : acl %d, siop %d", __func__,
					msd.dstat.acl_on, msd.dstat.siop_status);
			if(msd.dstat.auto_brightness == 6)
				pr_info("%s: HBM mode No ACL off!!", __func__);
			else
				mipi_samsung_disp_send_cmd(PANEL_ACL_OFF, true);
#if defined(SMART_ACL)
			/* If SMART_ACL enabled, elvss table shoud be set again */
			mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);
#endif

		} else {
			msd.dstat.acl_on = acl_set;
			pr_info("%s: skip but acl update!! acl %d, siop %d", __func__,
				msd.dstat.acl_on, msd.dstat.siop_status);
		}

	}else {
		pr_info("%s: panel is off state. updating state value.\n",
			__func__);
		msd.dstat.acl_on = acl_set;
	}

	return size;
}

#if defined(octa_manufacture_date)
static ssize_t manufacture_date_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char temp[30];

	snprintf((char *)temp, sizeof(temp), "manufacture date : %d\n", msd.manufacture_date);
	strlcat(buf, temp, 30);

	pr_info("manufacture date : %d\n", msd.manufacture_date);

	return strnlen(buf, 30);
}
#endif

static ssize_t mipi_samsung_disp_siop_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n", msd.dstat.siop_status);
	pr_info("siop status: %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_disp_siop_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd = msd.mfd;
	int siop_set;

	siop_set = msd.dstat.siop_status;
	if (sysfs_streq(buf, "1"))
		siop_set = true;
	else if (sysfs_streq(buf, "0"))
		siop_set = false;
	else
		pr_info("%s: Invalid argument!!", __func__);

	if (mfd->panel_power_on) {
		if (siop_set && !(msd.dstat.acl_on||msd.dstat.siop_status)) {
			msd.dstat.siop_status = true;
			mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);
			pr_info("%s: acl on  : acl %d, siop %d", __func__,
				msd.dstat.acl_on, msd.dstat.siop_status);

		} else if (!siop_set && !msd.dstat.acl_on && msd.dstat.siop_status) {
			mutex_lock(&msd.lock);
			msd.dstat.siop_status = false;
			if(msd.dstat.auto_brightness == 6)
				pr_info("%s: HBM mode No ACL off!!", __func__);
			else
				mipi_samsung_disp_send_cmd(PANEL_ACL_OFF, false);
#if defined(SMART_ACL)
			/* If SMART_ACL enabled, elvss table shoud be set again */
			mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);
#endif
			mutex_unlock(&msd.lock);
			pr_info("%s: acl off : acl %d, siop %d", __func__,
				msd.dstat.acl_on, msd.dstat.siop_status);

		} else {
			msd.dstat.siop_status = siop_set;
			pr_info("%s: skip but siop update!! acl %d, siop %d", __func__,
				msd.dstat.acl_on, msd.dstat.siop_status);
		}
	}else {
	msd.dstat.siop_status = siop_set;
	pr_info("%s: panel is off state. updating state value.\n",
		__func__);
	}

	return size;
}


static int atoi(const char *name)
{
	int val = 0;

	for (;; name++) {
		switch (*name) {
		case '0' ... '9':
			val = 10*val+(*name-'0');
			break;
		default:
			return val;
		}
	}
}

static ssize_t mipi_samsung_backlight_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n",
					msd.dstat.bright_level );
	pr_info("backlight : %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_backlight_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int bl_level = atoi(buf);

	pr_info("%s : level (%d)\n",__func__,bl_level);

	msd.dstat.bright_level = bl_level;
	mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);

	return size;
}

static ssize_t mipi_samsung_auto_brightness_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n",
					msd.dstat.auto_brightness);
	pr_info("auto_brightness: %d\n", *buf);

	return rc;
}


static unsigned int mipi_samsung_manufacture_id(struct mdss_panel_data *pdata);
static int mdss_dsi_panel_dimming_init(struct mdss_panel_data *pdata);


static ssize_t mipi_samsung_auto_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	if (!msd.manufacture_id)
		msd.manufacture_id = mipi_samsung_manufacture_id(msd.pdata);

	if (!msd.dstat.is_smart_dim_loaded)
		mdss_dsi_panel_dimming_init(msd.pdata);

	if (sysfs_streq(buf, "0"))
		msd.dstat.auto_brightness = 0;
	else if (sysfs_streq(buf, "1"))
		msd.dstat.auto_brightness = 1;
	else if (sysfs_streq(buf, "2"))
		msd.dstat.auto_brightness = 2;
	else if (sysfs_streq(buf, "3"))
		msd.dstat.auto_brightness = 3;
	else if (sysfs_streq(buf, "4"))
		msd.dstat.auto_brightness = 4;
	else if (sysfs_streq(buf, "5"))
		msd.dstat.auto_brightness = 5;
	else if (sysfs_streq(buf, "6")) // HBM mode
		msd.dstat.auto_brightness = 6;
	else if (sysfs_streq(buf, "7"))
		msd.dstat.auto_brightness = 7;
	else
		pr_info("%s: Invalid argument!!", __func__);

	if (msd.mfd->resume_state == MIPI_RESUME_STATE) {
		mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_QHD_VIDEO_PT_PANEL)
#if defined(CONFIG_MDNIE_LITE_TUNING)
		mDNIe_Set_Mode(); // LOCAL CE tuning
#endif
#endif
		pr_info("%s %d %d\n", __func__, msd.dstat.auto_brightness, msd.dstat.bright_level);
	} else {
		pr_info("%s : panel is off state!!\n", __func__);
	}
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

	msd.dstat.temper_need_update = 1;

	if(msd.mfd->resume_state == MIPI_RESUME_STATE) {
		mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);
		pr_info("%s brightness=%d msd.dstat.temperature :	%d msd.dstat.temperature_value :0x%x",
			__func__, msd.dstat.bright_level, msd.dstat.temperature, msd.dstat.temperature_value);
		if (msd.dstat.hbm_mode)
			mipi_samsung_disp_send_cmd(HBM_ELVSS_TEMPER, true);
	} else {
		pr_info("%s: skip but temperature update!! temperature %d, temperature_value %d", __func__,
				msd.dstat.temperature, msd.dstat.temperature_value);
	}

	return size;
}
#endif

static struct lcd_ops mipi_samsung_disp_props = {
	.get_power = NULL,
	.set_power = NULL,
};


static DEVICE_ATTR(lcd_power, S_IRUGO | S_IWUSR,
			mipi_samsung_disp_get_power,
			mipi_samsung_disp_set_power);

static DEVICE_ATTR(lcd_type, S_IRUGO, mipi_samsung_disp_lcdtype_show, NULL);

static DEVICE_ATTR(window_type, S_IRUGO,
			mipi_samsung_disp_windowtype_show, NULL);


static DEVICE_ATTR(power_reduce, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_disp_acl_show,
			mipi_samsung_disp_acl_store);
static DEVICE_ATTR(auto_brightness, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_auto_brightness_show,
			mipi_samsung_auto_brightness_store);

static DEVICE_ATTR(backlight, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_backlight_show,
			mipi_samsung_backlight_store);

#if defined(TEMPERATURE_ELVSS)
static DEVICE_ATTR(temperature, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_temperature_show,
			mipi_samsung_temperature_store);
#endif

static DEVICE_ATTR(siop_enable, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_disp_siop_show,
			mipi_samsung_disp_siop_store);

#if defined(octa_manufacture_date)
static DEVICE_ATTR(manufacture_date, S_IRUGO,
			manufacture_date_show, NULL);
#endif

static struct attribute *panel_sysfs_attributes[] = {
	&dev_attr_lcd_power.attr,
	&dev_attr_lcd_type.attr,
	&dev_attr_window_type.attr,
	&dev_attr_power_reduce.attr,
	&dev_attr_siop_enable.attr,
#if defined(TEMPERATURE_ELVSS)
	&dev_attr_temperature.attr,
#endif
#if defined(octa_manufacture_date)
	&dev_attr_manufacture_date.attr,
#endif
	NULL
};
static const struct attribute_group panel_sysfs_group = {
	.attrs = panel_sysfs_attributes,
};

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
static struct attribute *bl_sysfs_attributes[] = {
	&dev_attr_auto_brightness.attr,
	&dev_attr_backlight.attr,
	NULL
};
static const struct attribute_group bl_sysfs_group = {
	.attrs = bl_sysfs_attributes,
};
#endif
#endif

#if defined(CONFIG_ESD_RECOVERY)
static irqreturn_t err_fg_irq_handler(int irq, void *handle)
{
	struct msm_fb_data_type *mfd = msd.mfd;

	if (err_fg_working || !(mfd->panel_power_on))
		return IRQ_HANDLED;
	pr_info("%s handler start irq=%d", __func__, irq);

	err_fg_working = 1;
	disable_irq_nosync(gpio_to_irq(msd.ctrl_pdata->esd_gpio));
	schedule_work(&err_fg_work);
	pr_info("%s : handler end", __func__);

	return IRQ_HANDLED;
}

static void err_fg_work_func(struct work_struct *work)
{
    struct msm_fb_data_type *mfd = msd.mfd;

    pr_info("%s : start", __func__);

    if (mfd->panel_power_on) {
        int bl_backup = msd.dstat.bright_level;

        //mutex_lock(&mfd->power_state);
        mutex_lock(&mfd->ctx_lock);

        msd.pdata->event_handler(msd.pdata, MDSS_EVENT_BLANK, NULL);
        msd.pdata->event_handler(msd.pdata, MDSS_EVENT_PANEL_OFF, NULL);
        mdelay(20);
        msd.pdata->event_handler(msd.pdata, MDSS_EVENT_UNBLANK, NULL);
        msd.pdata->event_handler(msd.pdata, MDSS_EVENT_PANEL_ON, NULL);

        mutex_unlock(&mfd->ctx_lock);
        //mutex_unlock(&mfd->power_state);

        /* Restore brightness */
        msd.dstat.bright_level = bl_backup;
        mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);
    }
    esd_count++;
    err_fg_working = 0;

    pr_info("%s end", __func__);
    return;
}
#ifdef ESD_DEBUG
static ssize_t mipi_samsung_esd_check_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
    int rc;

    rc = snprintf((char *)buf, 20, "esd count %d\n", esd_count);

    return rc;
}
static ssize_t mipi_samsung_esd_check_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
    struct msm_fb_data_type *mfd = msd.mfd;

    err_fg_irq_handler(0, mfd);
    return 1;
}

static DEVICE_ATTR(esd_check, S_IRUGO , mipi_samsung_esd_check_show,\
             mipi_samsung_esd_check_store);
#endif
#endif

static int __init detect_lcd_panel_vendor(char* read_id)
{
	int lcd_id = simple_strtol(read_id, NULL, 16);
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_QHD_VIDEO_PT_PANEL)
	if (lcd_id == 0x401001)
		is_panel_ams485dl01_revA = 1;
	else if (lcd_id == 0x421002)
		is_panel_ams485dl01_revC = 1;
	else if (lcd_id == 0x421003)
		is_panel_ams485dl01_revD = 1;
	else if (lcd_id == 0x420000)
		is_panel_ams452ef01_revA = 1;
	else if (lcd_id == 0x420001)
		is_panel_ams452ef01_revC = 1;
	else {
		pr_info("%s: manufacture id read may be faulty id[0x%x]\n", __func__, lcd_id);
		is_panel_ams485dl01_revA = 0;
	}
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_WVGA_VIDEO_PT_PANEL)
	gv_manufacture_id = 1;
#endif
	pr_info("%s: detected panel ID --> [0x%x]\n", __func__, lcd_id);
	return 1;
}
__setup("lcd_id=0x", detect_lcd_panel_vendor);


int mdss_dsi_panel_init(struct device_node *node,
	struct mdss_dsi_ctrl_pdata *ctrl_pdata,
	bool cmd_cfg_cont_splash)
{
	int rc = 0;
	static const char *panel_name;
	struct mdss_panel_info *pinfo;
#if defined(CONFIG_LCD_CLASS_DEVICE)
	struct lcd_device *lcd_device;
#endif
#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	struct backlight_device *bd = NULL;
#endif
	if (!node || !ctrl_pdata) {
		pr_err("%s: Invalid arguments\n", __func__);
		return -ENODEV;
	}

	pinfo = &ctrl_pdata->panel_data.panel_info;

	pr_info("%s:%d\n", __func__, __LINE__);
	panel_name = of_get_property(node, "qcom,mdss-dsi-panel-name", NULL);
	if (!panel_name)
		pr_info("%s:%d, Panel name not specified\n",
						__func__, __LINE__);
	else
		pr_info("%s: Panel Name = %s\n", __func__, panel_name);

	if(is_panel_supported(panel_name)) {
		LCD_DEBUG("Panel : %s is not supported:",panel_name);
		return -1;
	}
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
	ctrl_pdata->bl_fnc = mdss_dsi_panel_bl_ctrl;
	ctrl_pdata->panel_reset = mdss_dsi_samsung_panel_reset;
	ctrl_pdata->registered = mdss_dsi_panel_registered;
	ctrl_pdata->dimming_init = mdss_dsi_panel_dimming_init;
	ctrl_pdata->panel_data.set_backlight = mdss_dsi_panel_bl_ctrl;
	ctrl_pdata->panel_gpio_request = mdss_dsi_request_gpios;
	ctrl_pdata->switch_mode = mdss_dsi_panel_switch_mode;
	/* Init driver  specific data */
	mutex_init(&msd.lock);


#if defined(CONFIG_LCD_CLASS_DEVICE)
	lcd_device = lcd_device_register("panel", NULL, NULL,
					&mipi_samsung_disp_props);

	if (IS_ERR(lcd_device)) {
		rc = PTR_ERR(lcd_device);
		printk(KERN_ERR "lcd : failed to register device\n");
		return rc;
	}

	sysfs_remove_file(&lcd_device->dev.kobj,
		&dev_attr_lcd_power.attr);

	rc = sysfs_create_group(&lcd_device->dev.kobj, &panel_sysfs_group);
	if (rc) {
		pr_err("Failed to create panel sysfs group..\n");
		sysfs_remove_group(&lcd_device->dev.kobj, &panel_sysfs_group);
	}
#endif
#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	bd = backlight_device_register("panel", &lcd_device->dev,
						NULL, NULL, NULL);
	if (IS_ERR(bd)) {
		rc = PTR_ERR(bd);
		pr_info("backlight : failed to register device\n");
		return rc;
	}

	rc = sysfs_create_group(&bd->dev.kobj, &bl_sysfs_group);
	if (rc) {
		pr_err("Failed to create backlight sysfs group..\n");
		sysfs_remove_group(&bd->dev.kobj, &bl_sysfs_group);
	}
#endif
#if defined(CONFIG_ESD_RECOVERY)
#ifdef ESD_DEBUG
    rc= sysfs_create_file(&lcd_device->dev.kobj,
                            &dev_attr_esd_check.attr);
    if (rc) {
        pr_info("sysfs create fail-%s\n",
                dev_attr_esd_check.attr.name);
    }
#endif
#endif
#if defined(CONFIG_MDNIE_LITE_TUNING)
	pr_info("[%s] CONFIG_MDNIE_LITE_TUNING ok ! init class called!\n",
		__func__);
	init_mdnie_class();
#endif

#if defined(DDI_VIDEO_ENHANCE_TUNING)
	rc = sysfs_create_file(&lcd_device->dev.kobj,
			&dev_attr_tuning.attr);
	if (rc) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_tuning.attr.name);
	}
#endif
	return 0;
}
int get_samsung_lcd_attached(void)
{
	return lcd_attached;

}
EXPORT_SYMBOL(get_samsung_lcd_attached);
static int __init get_lcd_id_cmdline(char *mode)
{
	char *pt;

	int lcd_id = 0;
	if( mode == NULL ) return 1;
	for( pt = mode; *pt != 0; pt++ )
	{
		lcd_id <<= 4;
		switch(*pt)
		{
			case '0' ... '9' :
				lcd_id += *pt -'0';
			break;
			case 'a' ... 'f' :
				lcd_id += 10 + *pt -'a';
			break;
			case 'A' ... 'F' :
				lcd_id += 10 + *pt -'A';
			break;
		}
	}
	lcd_attached = ((lcd_id&0xFFFFFF)!=0x000000);

	pr_info( "%s: LCD_ID = 0x%X, lcd_attached =%d", __func__, lcd_id, lcd_attached);

	return 0;
}
__setup( "lcd_id=0x", get_lcd_id_cmdline );
MODULE_DESCRIPTION("Samsung DSI panel driver");
MODULE_AUTHOR("Raghu J <raghu.j@samsung.com>");
MODULE_LICENSE("GPL");
