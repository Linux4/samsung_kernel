/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/of_device.h>
#include <linux/gpio.h>
#include <linux/qpnp/pin.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/pwm.h>
#include <linux/err.h>
#if defined (CONFIG_LCD_CLASS_DEVICE)
	#include <linux/lcd.h>
#endif
#include "mdss_fb.h"
#if defined(CONFIG_MDNIE_VIDEO_ENHANCED)
#include "mdss_video_enhance.h"
#endif
#if defined (CONFIG_MDNIE_LITE_TUNING)
#include "mdnie_lite_tuning.h"
#endif
#include "mdss_dsi.h"
#include "mdss_magna_octa_720p_panel.h"
#include "dlog.h"
#include "ea8061_parame.h"
#include "dynamic_aid.h"
#include "dynamic_aid_ea8061.h"



//#define DDI_VIDEO_ENHANCE_TUNING
#if defined(DDI_VIDEO_ENHANCE_TUNING)
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#endif
#if defined(CONFIG_LCD_CONNECTION_CHECK)
static int is_sharp_panel;
static int lcd_connected_status;
static char *panel_vendor = NULL;
#endif
#define DT_CMD_HDR 6
#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
struct work_struct  err_fg_work;
static int err_fg_gpio;
static int esd_count;
static int err_fg_working;
#define ESD_DEBUG 1
#endif
DEFINE_LED_TRIGGER(bl_led_trigger);

static struct mdss_samsung_driver_data msd;
extern int poweroff_charging;
static int first_boot;

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		255
#define DEFAULT_BRIGHTNESS		162
#define DEFAULT_GAMMA_LEVEL		IBRIGHTNESS_162NT

#define POWER_IS_ON(pwr)		(pwr <= FB_BLANK_NORMAL)
#define LEVEL_IS_HBM(level)		(level >= 6)

#define LDI_ID_REG			0xD1
#define LDI_ID_LEN			3
#define LDI_MTP_REG			0xDA
#define LDI_MTP_LEN			33	/* MTP 1~32th + Dummy because VT G/R share 31th para */
#define LDI_ELVSS_REG			0xB2
#define LDI_ELVSS_LEN			(ELVSS_PARAM_SIZE - 1)
#define LDI_HBM_REG			0xDB
#define LDI_HBM_LEN			34	/* HBM V35 ~ V255 + HBM ELVSS for D4h */

#define LDI_COORDINATE_REG		0xA1
#define LDI_COORDINATE_LEN		4

#define LDI_DATE_REG			0xC8
#define LDI_DATE_LEN			42

#define LDI_MPS_REG			0xD4
#define LDI_MPS_LEN			18
#define MPS_PARAM_SIZE		(LDI_MPS_LEN + 1)	/* REG + 18 para */

struct lcd_info {
	unsigned int			bl;
	unsigned int			auto_brightness;
	unsigned int			acl_enable;
	unsigned int			siop_enable;
	unsigned int			current_acl;
	unsigned int			current_bl;
	unsigned int			current_elvss;
	unsigned int			current_temperature;
	unsigned int			current_hbm;
	unsigned int			current_mps;
	unsigned int			ldi_enable;
	unsigned int			power;

	struct device			*dev;
	struct lcd_device		*ld;
	struct backlight_device		*bd;
	unsigned char			id[LDI_ID_LEN];
	unsigned char			**gamma_table;
	unsigned char			**elvss_table[2];
	unsigned char			*mps_table[HBM_STATUS_MAX][MPS_STATUS_MAX];

	struct dynamic_aid_param_t	daid;
	unsigned char			aor[IBRIGHTNESS_MAX][ARRAY_SIZE(SEQ_AID_SET)];
	unsigned int			connected;

	unsigned char			**tset_table;
	int				temperature;

	unsigned int			coordinate[2];

	struct mipi_dsim_device		*dsim;
};



void mdss_dsi_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_cmd_desc *cmds, int cnt)
{
	struct dcs_cmd_req cmdreq;
#if defined(CONFIG_LCD_CONNECTION_CHECK)
	if (is_lcd_attached() == 0)
	{
		printk("%s: LCD not connected!\n",__func__);
		return;
	}
#endif

	memset(&cmdreq, 0, sizeof(cmdreq));

	cmdreq.cmds = cmds;
	cmdreq.cmds_cnt = cnt;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;
	
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}


void mdss_dsi_panel_pwm_cfg(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int ret;

	if (!gpio_is_valid(ctrl->pwm_pmic_gpio)) {
		pr_err("%s: pwm_pmic_gpio=%d Invalid\n", __func__,
				ctrl->pwm_pmic_gpio);
		ctrl->pwm_pmic_gpio = -1;
		return;
	}

	ret = gpio_request(ctrl->pwm_pmic_gpio, "disp_pwm");
	if (ret) {
		pr_err("%s: pwm_pmic_gpio=%d request failed\n", __func__,
				ctrl->pwm_pmic_gpio);
		ctrl->pwm_pmic_gpio = -1;
		return;
	}

	ctrl->pwm_bl = pwm_request(ctrl->pwm_lpg_chan, "lcd-bklt");
	if (ctrl->pwm_bl == NULL || IS_ERR(ctrl->pwm_bl)) {
		pr_err("%s: lpg_chan=%d pwm request failed", __func__,
				ctrl->pwm_lpg_chan);
		gpio_free(ctrl->pwm_pmic_gpio);
		ctrl->pwm_pmic_gpio = -1;
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

	duty = level * ctrl->pwm_period;
	duty /= ctrl->bklt_max;

	pr_debug("%s: bklt_ctrl=%d pwm_period=%d pwm_gpio=%d pwm_lpg_chan=%d\n",
			__func__, ctrl->bklt_ctrl, ctrl->pwm_period,
				ctrl->pwm_pmic_gpio, ctrl->pwm_lpg_chan);

	pr_debug("%s: ndx=%d level=%d duty=%d\n", __func__,
					ctrl->ndx, level, duty);

	ret = pwm_config(ctrl->pwm_bl, duty, ctrl->pwm_period);
	if (ret) {
		pr_err("%s: pwm_config() failed err=%d.\n", __func__, ret);
		return;
	}

	ret = pwm_enable(ctrl->pwm_bl);
	if (ret)
		pr_err("%s: pwm_enable() failed err=%d\n", __func__, ret);
}


static void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_panel_cmds *pcmds)
{
	struct dcs_cmd_req cmdreq;
#if defined(CONFIG_LCD_CONNECTION_CHECK)
	if (is_lcd_attached() == 0)
	{
		printk("%s: LCD not connected!\n",__func__);
		return;
	}
#endif
	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = pcmds->cmds;
	cmdreq.cmds_cnt = pcmds->cmd_cnt;
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}





unsigned char mdss_dsi_panel_pwm_scaling(int level)
{
	unsigned char scaled_level;

	if (level >= MAX_BRIGHTNESS_LEVEL)
		scaled_level  = BL_MAX_BRIGHTNESS_LEVEL;
	else if (level >= MID_BRIGHTNESS_LEVEL) {
		scaled_level  = (level - MID_BRIGHTNESS_LEVEL) *
		(BL_MAX_BRIGHTNESS_LEVEL - BL_MID_BRIGHTNESS_LEVEL) / (MAX_BRIGHTNESS_LEVEL-MID_BRIGHTNESS_LEVEL) + BL_MID_BRIGHTNESS_LEVEL;
	} else if (level >= DIM_BRIGHTNESS_LEVEL) {
		scaled_level  = (level - DIM_BRIGHTNESS_LEVEL) *
		(BL_MID_BRIGHTNESS_LEVEL - BL_DIM_BRIGHTNESS_LEVEL) / (MID_BRIGHTNESS_LEVEL-DIM_BRIGHTNESS_LEVEL) + BL_DIM_BRIGHTNESS_LEVEL;
	} else if (level >= LOW_BRIGHTNESS_LEVEL) {
		scaled_level  = (level - LOW_BRIGHTNESS_LEVEL) *
		(BL_DIM_BRIGHTNESS_LEVEL - BL_LOW_BRIGHTNESS_LEVEL) / (DIM_BRIGHTNESS_LEVEL-LOW_BRIGHTNESS_LEVEL) + BL_LOW_BRIGHTNESS_LEVEL;
	}  else{
		if(poweroff_charging == 1)
			scaled_level  = level*BL_LOW_BRIGHTNESS_LEVEL/LOW_BRIGHTNESS_LEVEL;
		else
			scaled_level  = BL_MIN_BRIGHTNESS;
	}

	pr_info("level = [%d]: scaled_level = [%d]   autobrightness level:%d\n",level,scaled_level,msd.dstat.auto_brightness);

	return scaled_level;
}

int mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	int rc=0;
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return 0;
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
		return 0;
	}

	pr_info("%s: enable = %d\n", __func__, enable);
	pinfo = &(ctrl_pdata->panel_data.panel_info);
	if (enable) {		
		rc = gpio_tlmm_config(GPIO_CFG(ctrl_pdata->lcd_ldo_en1, 0,
					GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL,GPIO_CFG_8MA),
					GPIO_CFG_ENABLE);
		if (rc)
			pr_err("enabling disp_en_gpio_n failed, rc=%d\n",rc);
		
		rc = gpio_tlmm_config(GPIO_CFG(ctrl_pdata->lcd_ldo_en2, 0,
					GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL,GPIO_CFG_8MA),
					GPIO_CFG_ENABLE);
		
		if (rc)
			pr_err("enabling disp_en_gpio_n failed, rc=%d\n",rc);
		
		rc = gpio_tlmm_config(GPIO_CFG(ctrl_pdata->rst_gpio, 0,
					GPIO_CFG_OUTPUT,GPIO_CFG_PULL_UP,GPIO_CFG_8MA),
					GPIO_CFG_ENABLE);
		if (rc)
			pr_err("disabling rst_gpio failed, rc=%d\n",rc);
		
		mdelay(20);
		gpio_set_value(ctrl_pdata->lcd_ldo_en1, 1);
		mdelay(5);
		gpio_set_value(ctrl_pdata->lcd_ldo_en2, 1);
		mdelay(1);

		gpio_set_value((ctrl_pdata->rst_gpio), 1);
		msleep(20);
		gpio_set_value((ctrl_pdata->rst_gpio), 0);
		msleep(1);
		gpio_set_value((ctrl_pdata->rst_gpio), 1);
		msleep(20);
		
		if (gpio_is_valid(ctrl_pdata->mode_gpio)) {
			if (pinfo->mode_gpio_state == MODE_GPIO_HIGH)
				gpio_set_value((ctrl_pdata->mode_gpio), 1);
			else if (pinfo->mode_gpio_state == MODE_GPIO_LOW)
				gpio_set_value((ctrl_pdata->mode_gpio), 0);
		}
#if 0	// Blocked the disp_en_gpio
		if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
			gpio_set_value((ctrl_pdata->disp_en_gpio), 1);
#endif
		
		if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
			pr_debug("%s: Panel Not properly turned OFF\n",
						__func__);
			ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
			pr_debug("%s: Reset panel done\n", __func__);
		}
	} else {
#if 0	// Blocked the disp_en_gpio
		if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
			gpio_set_value((ctrl_pdata->disp_en_gpio), 0);
#endif

		rc = gpio_tlmm_config(GPIO_CFG(ctrl_pdata->lcd_ldo_en1, 0,
					GPIO_CFG_OUTPUT,GPIO_CFG_PULL_DOWN,GPIO_CFG_2MA),
					GPIO_CFG_ENABLE);
		if (rc)
			pr_err("disabling lcd_ldo_en1 failed, rc=%d\n",rc);
		
		rc = gpio_tlmm_config(GPIO_CFG(ctrl_pdata->lcd_ldo_en2, 0,
					GPIO_CFG_OUTPUT,GPIO_CFG_PULL_DOWN,GPIO_CFG_2MA),
					GPIO_CFG_ENABLE);
		if (rc)
			pr_err("disabling lcd_ldo_en2 failed, rc=%d\n",rc);
		
		rc = gpio_tlmm_config(GPIO_CFG(ctrl_pdata->rst_gpio, 0,
					GPIO_CFG_OUTPUT,GPIO_CFG_PULL_DOWN,GPIO_CFG_2MA),
					GPIO_CFG_ENABLE);
		if (rc)
			pr_err("disabling rst_gpio failed, rc=%d\n",rc);

		gpio_set_value(ctrl_pdata->lcd_ldo_en1, 0);
		gpio_set_value(ctrl_pdata->lcd_ldo_en2, 0);
		gpio_set_value((ctrl_pdata->rst_gpio), 0);
	}
	return 0;
}
#if 0
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
#endif



static int get_backlight_level_from_brightness(int brightness)
{
	int backlightlevel;

	switch (brightness) {
	case 0 ... 5:
		backlightlevel = IBRIGHTNESS_5NT;
		break;
	case 6:
		backlightlevel = IBRIGHTNESS_6NT;
		break;
	case 7:
		backlightlevel = IBRIGHTNESS_7NT;
		break;
	case 8:
		backlightlevel = IBRIGHTNESS_8NT;
		break;
	case 9:
		backlightlevel = IBRIGHTNESS_9NT;
		break;
	case 10:
		backlightlevel = IBRIGHTNESS_10NT;
		break;
	case 11:
		backlightlevel = IBRIGHTNESS_11NT;
		break;
	case 12:
		backlightlevel = IBRIGHTNESS_12NT;
		break;
	case 13:
		backlightlevel = IBRIGHTNESS_13NT;
		break;
	case 14:
		backlightlevel = IBRIGHTNESS_14NT;
		break;
	case 15:
		backlightlevel = IBRIGHTNESS_15NT;
		break;
	case 16:
		backlightlevel = IBRIGHTNESS_16NT;
		break;
	case 17 ... 18:
		backlightlevel = IBRIGHTNESS_17NT;
		break;
	case 19:
		backlightlevel = IBRIGHTNESS_19NT;
		break;
	case 20:
		backlightlevel = IBRIGHTNESS_20NT;
		break;
	case 21:
		backlightlevel = IBRIGHTNESS_21NT;
		break;
	case 22 ... 23:
		backlightlevel = IBRIGHTNESS_22NT;
		break;
	case 24:
		backlightlevel = IBRIGHTNESS_24NT;
		break;
	case 25 ... 26:
		backlightlevel = IBRIGHTNESS_25NT;
		break;
	case 27 ... 28:
		backlightlevel = IBRIGHTNESS_27NT;
		break;
	case 29:
		backlightlevel = IBRIGHTNESS_29NT;
		break;
	case 30 ... 31:
		backlightlevel = IBRIGHTNESS_30NT;
		break;
	case 32 ... 33:
		backlightlevel = IBRIGHTNESS_32NT;
		break;
	case 34 ... 36:
		backlightlevel = IBRIGHTNESS_34NT;
		break;
	case 37 ... 38:
		backlightlevel = IBRIGHTNESS_37NT;
		break;
	case 39 ... 40:
		backlightlevel = IBRIGHTNESS_39NT;
		break;
	case 41 ... 43:
		backlightlevel = IBRIGHTNESS_41NT;
		break;
	case 44 ... 46:
		backlightlevel = IBRIGHTNESS_44NT;
		break;
	case 47 ... 49:
		backlightlevel = IBRIGHTNESS_47NT;
		break;
	case 50 ... 52:
		backlightlevel = IBRIGHTNESS_50NT;
		break;
	case 53 ... 55:
		backlightlevel = IBRIGHTNESS_53NT;
		break;
	case 56 ... 59:
		backlightlevel = IBRIGHTNESS_56NT;
		break;
	case 60 ... 63:
		backlightlevel = IBRIGHTNESS_60NT;
		break;
	case 64 ... 67:
		backlightlevel = IBRIGHTNESS_64NT;
		break;
	case 68 ... 71:
		backlightlevel = IBRIGHTNESS_68NT;
		break;
	case 72 ... 76:
		backlightlevel = IBRIGHTNESS_72NT;
		break;
	case 77 ... 81:
		backlightlevel = IBRIGHTNESS_77NT;
		break;
	case 82 ... 86:
		backlightlevel = IBRIGHTNESS_82NT;
		break;
	case 87 ... 92:
		backlightlevel = IBRIGHTNESS_87NT;
		break;
	case 93 ... 97:
		backlightlevel = IBRIGHTNESS_93NT;
		break;
	case 98 ... 104:
		backlightlevel = IBRIGHTNESS_98NT;
		break;
	case 105:
		backlightlevel = IBRIGHTNESS_105NT;
		break;
	case 106:
		backlightlevel = IBRIGHTNESS_106NT;
		break;
	case 107:
		backlightlevel = IBRIGHTNESS_107NT;
		break;
	case 108:
		backlightlevel = IBRIGHTNESS_108NT;
		break;
	case 109:
		backlightlevel = IBRIGHTNESS_109NT;
		break;
	case 110:
		backlightlevel = IBRIGHTNESS_110NT;
		break;
	case 111 ... 118:
		backlightlevel = IBRIGHTNESS_111NT;
		break;
	case 119 ... 125:
		backlightlevel = IBRIGHTNESS_119NT;
		break;
	case 126 ... 133:
		backlightlevel = IBRIGHTNESS_126NT;
		break;
	case 134 ... 142:
		backlightlevel = IBRIGHTNESS_134NT;
		break;
	case 143 ... 149:
		backlightlevel = IBRIGHTNESS_143NT;
		break;
	case 150 ... 161:
		backlightlevel = IBRIGHTNESS_152NT;
		break;
	case 162 ... 171:
		backlightlevel = IBRIGHTNESS_162NT;
		break;
	case 172 ... 172:
		backlightlevel = IBRIGHTNESS_172NT;
		break;
	case 173 ... 174:
		backlightlevel = IBRIGHTNESS_173NT;
		break;
	case 175 ... 176:
		backlightlevel = IBRIGHTNESS_175NT;
		break;
	case 177 ... 178:
		backlightlevel = IBRIGHTNESS_177NT;
		break;
	case 179 ... 180:
		backlightlevel = IBRIGHTNESS_179NT;
		break;
	case 181 ... 182:
		backlightlevel = IBRIGHTNESS_181NT;
		break;
	case 183 ... 193:
		backlightlevel = IBRIGHTNESS_183NT;
		break;
	case 194 ... 205:
		backlightlevel = IBRIGHTNESS_195NT;
		break;
	case 206 ... 218:
		backlightlevel = IBRIGHTNESS_207NT;
		break;
	case 219 ... 229:
		backlightlevel = IBRIGHTNESS_220NT;
		break;
	case 230 ... 237:
		backlightlevel = IBRIGHTNESS_234NT;
		break;
	case 238 ... 241:
		backlightlevel = IBRIGHTNESS_249NT;
		break;
	case 242 ... 244:
		backlightlevel = IBRIGHTNESS_265NT;
		break;
	case 245 ... 247:
		backlightlevel = IBRIGHTNESS_282NT;
		break;
	case 248 ... 249:
		backlightlevel = IBRIGHTNESS_300NT;
		break;
	case 250 ... 251:
		backlightlevel = IBRIGHTNESS_316NT;
		break;
	case 252 ... 253:
		backlightlevel = IBRIGHTNESS_333NT;
		break;
	case 254 ... 255:
		backlightlevel = IBRIGHTNESS_350NT;
		break;
	default:
		backlightlevel = IBRIGHTNESS_162NT;
		break;
	}

	return backlightlevel;
}


static int ea8061_gamma_ctl(struct lcd_info *lcd, struct mdss_dsi_ctrl_pdata *ctrl)
{
	int i= 0;

	for(i=0;i<sizeof(gamma_set_cmd1);i++){
		gamma_set_cmd1[i] = lcd->gamma_table[lcd->bl][i];
         }

	mdss_dsi_cmds_send(ctrl, samsung_panel_gamma_update_cmds, ARRAY_SIZE(samsung_panel_gamma_update_cmds));

	return 0;
}

static int ea8061_aid_parameter_ctl(struct lcd_info *lcd, u8 force, struct mdss_dsi_ctrl_pdata *ctrl)
{
	int i =0;

	if (force)
		goto aid_update;
	else if (lcd->aor[lcd->bl][3] != lcd->aor[lcd->current_bl][3])
		goto aid_update;
	else if (lcd->aor[lcd->bl][4] != lcd->aor[lcd->current_bl][4])
		goto aid_update;
	else
		goto exit;

aid_update:
	for(i=0;i<sizeof(SEQ_AID_SET);i++){
		SEQ_AID_SET[i] = lcd->aor[lcd->bl][i];		
        }
	mdss_dsi_cmds_send(ctrl, samsung_panel_SEQ_AID_SET_cmds, ARRAY_SIZE(samsung_panel_SEQ_AID_SET_cmds));	

exit:
	return 0;
}

static int ea8061_set_acl(struct lcd_info *lcd, u8 force, struct mdss_dsi_ctrl_pdata *ctrl)
{
	int ret = 0, level;

	level = ACL_STATUS_25P;

	if (lcd->siop_enable || LEVEL_IS_HBM(lcd->auto_brightness))
		goto acl_update;

	if (!lcd->acl_enable)
		level = ACL_STATUS_0P;

acl_update:
	if (force || lcd->current_acl != ACL_CUTOFF_TABLE[level][1]) {
		lcd->current_acl = ACL_CUTOFF_TABLE[level][1];
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

static int ea8061_set_elvss(struct lcd_info *lcd, u8 force, struct mdss_dsi_ctrl_pdata *ctrl)
{
	int ret = 0, elvss_level = 0, elvss;
	u32 candela = index_brightness_table[lcd->bl];
	int smart_acl = lcd->current_acl ? 1 : 0;
	int i =0;

	switch (candela) {
	case 0 ... 105:
		elvss_level = ELVSS_STATUS_105;
		break;
	case 106:
		elvss_level = ELVSS_STATUS_106;
		break;
	case 107:
		elvss_level = ELVSS_STATUS_107;
		break;
	case 108 ... 109:
		elvss_level = ELVSS_STATUS_109;
		break;
	case 110 ... 111:
		elvss_level = ELVSS_STATUS_111;
		break;
	case 112 ... 126:
		elvss_level = ELVSS_STATUS_126;
		break;
	case 127 ... 134:
		elvss_level = ELVSS_STATUS_134;
		break;
	case 135 ... 143:
		elvss_level = ELVSS_STATUS_143;
		break;
	case 144 ... 152:
		elvss_level = ELVSS_STATUS_152;
		break;
	case 153 ... 162:
		elvss_level = ELVSS_STATUS_162;
		break;
	case 163 ... 173:
		elvss_level = ELVSS_STATUS_173;
		break;
	case 174 ... 175:
		elvss_level = ELVSS_STATUS_175;
		break;
	case 176 ... 177:
		elvss_level = ELVSS_STATUS_177;
		break;
	case 178 ... 179:
		elvss_level = ELVSS_STATUS_179;
		break;
	case 180 ... 181:
		elvss_level = ELVSS_STATUS_181;
		break;
	case 182 ... 183:
		elvss_level = ELVSS_STATUS_183;
		break;
	case 184 ... 195:
		elvss_level = ELVSS_STATUS_195;
		break;
	case 196 ... 207:
		elvss_level = ELVSS_STATUS_207;
		break;
	case 208 ... 220:
		elvss_level = ELVSS_STATUS_220;
		break;
	case 221 ... 234:
		elvss_level = ELVSS_STATUS_234;
		break;
	case 235 ... 249:
		elvss_level = ELVSS_STATUS_249;
		break;
	case 250 ... 265:
		elvss_level = ELVSS_STATUS_265;
		break;
	case 266 ... 282:
		elvss_level = ELVSS_STATUS_282;
		break;
	case 283 ... 300:
		elvss_level = ELVSS_STATUS_300;
		break;
	case 301 ... 316:
		elvss_level = ELVSS_STATUS_316;
		break;
	case 317 ... 333:
		elvss_level = ELVSS_STATUS_333;
		break;
	case 334 ... 350:
		elvss_level = ELVSS_STATUS_350;
		break;
	case 500:
		elvss_level = ELVSS_STATUS_HBM;
		break;
	default:
		elvss_level = ELVSS_STATUS_350;
		break;
	}

	elvss = lcd->elvss_table[0][elvss_level][1];

	if (force || lcd->elvss_table[smart_acl][lcd->current_elvss][1] != elvss || lcd->current_temperature != lcd->temperature) {
		if (lcd->temperature < 0)
			lcd->elvss_table[smart_acl][elvss_level][7] = (unsigned char)((-1) * (char)lcd->temperature) | (1<<7);
		else
			lcd->elvss_table[smart_acl][elvss_level][7] = (unsigned char)lcd->temperature;

	for(i=0;i<sizeof(SEQ_ELVSS_SET);i++){
		SEQ_ELVSS_SET[i] =lcd->elvss_table[smart_acl][elvss_level][i];
        }
						
	mdss_dsi_cmds_send(ctrl, samsung_panel_SEQ_ELVSS_SET_cmds, ARRAY_SIZE(samsung_panel_SEQ_ELVSS_SET_cmds));	

	lcd->current_elvss = elvss_level;
	lcd->current_temperature = lcd->temperature;
	}

	if (!ret) {
		ret = -EPERM;
		goto elvss_err;
	}

elvss_err:
	return ret;
}

static int ea8061_set_mps(struct lcd_info *lcd, u8 force, struct mdss_dsi_ctrl_pdata *ctrl)
{
	int ret = 0;
	int mps = (lcd->temperature > 0) ? 0 : 1;
	int hbm = LEVEL_IS_HBM(lcd->auto_brightness);
	int i=0;

	if (force || lcd->current_hbm != hbm || lcd->current_mps != mps) {

	for(i=0;i<sizeof(MPS_cmds);i++){
		MPS_cmds[i] = lcd->mps_table[hbm][mps][i];		
       }
       MPS_cmds[1]  = 0x48;
					
	mdss_dsi_cmds_send(ctrl, samsung_panel_SEQ_MTP_KEY_UNLOCK_cmds, ARRAY_SIZE(samsung_panel_SEQ_MTP_KEY_UNLOCK_cmds));
	mdss_dsi_cmds_send(ctrl, samsung_panel_MPS_cmds, ARRAY_SIZE(samsung_panel_MPS_cmds));
	mdss_dsi_cmds_send(ctrl, samsung_panel_SEQ_MTP_KEY_LOCK_cmds, ARRAY_SIZE(samsung_panel_SEQ_MTP_KEY_LOCK_cmds));

	lcd->current_mps = mps;
	lcd->current_hbm = hbm;
	}

	if (!ret) {
		ret = -EPERM;
		goto err;
	}

err:
	return ret;
}

static void init_mtp_data(struct lcd_info *lcd, u8 *mtp_data)
{
	int i, c, j;
	int *mtp;

	mtp = lcd->daid.mtp;

	mtp_data[32] = mtp_data[31];			/* VT B */
	mtp_data[31] = (mtp_data[30] >> 4) & 0xF;		/* VT G */
	mtp_data[30] = (mtp_data[30] & 0xF);		/* VT R */

	for (c = 0, j = 0; c < CI_MAX; c++, j++) {
		if (mtp_data[j++] & 0x01)
			mtp[(IV_MAX-1)*CI_MAX+c] = (256-mtp_data[j]) * (-1);
		else
			mtp[(IV_MAX-1)*CI_MAX+c] = mtp_data[j];
	}

	for (i = IV_203; i >= 0; i--) {
		for (c = 0; c < CI_MAX; c++, j++) {
			if (mtp_data[j] & 0x80)
				mtp[CI_MAX*i+c] = (128-(mtp_data[j]&0x7F)) * (-1);
			else
				mtp[CI_MAX*i+c] = mtp_data[j]&0x7F;
		}
	}

	for (i = 0, j = 0; i <= IV_MAX; i++)
		for (c = 0; c < CI_MAX; c++, j++)
			pr_debug("mtp_data[%d] = %x\n", j, mtp_data[j]);

	for (i = 0, j = 0; i < IV_MAX; i++)
		for (c = 0; c < CI_MAX; c++, j++)
			pr_debug("mtp[%d] = %x\n", j, mtp[j]);

	for (i = 0, j = 0; i < IV_MAX; i++) {
		for (c = 0; c < CI_MAX; c++, j++)
			pr_debug("%x ", mtp[j]);
		pr_debug("\n");
	}
}



static int update_brightness(struct lcd_info *lcd, u8 force, struct mdss_dsi_ctrl_pdata *ctrl, int bl_level)
{
	u32 brightness;

	brightness = bl_level;

	lcd->bl = get_backlight_level_from_brightness(brightness);

	if (LEVEL_IS_HBM(lcd->auto_brightness) && (brightness == MAX_BRIGHTNESS))
		lcd->bl = IBRIGHTNESS_500NT;

	if ((force) || ((lcd->ldi_enable) && (lcd->current_bl != lcd->bl))) {

	mdss_dsi_cmds_send(ctrl, samsung_panel_gamma_update_off_cmds, ARRAY_SIZE(samsung_panel_gamma_update_off_cmds));

		ea8061_gamma_ctl(lcd,ctrl);

		ea8061_aid_parameter_ctl(lcd, force,ctrl);

		mdss_dsi_cmds_send(ctrl, samsung_panel_gamma_update_on_cmds, ARRAY_SIZE(samsung_panel_gamma_update_on_cmds));

		ea8061_set_elvss(lcd, force,ctrl);

		ea8061_set_mps(lcd, force,ctrl);
		
		if(bl_level == 1000){
		ea8061_set_acl(lcd, force,ctrl);
		}
		
		lcd->current_bl = lcd->bl;

		pr_info("brightness=%d, bl=%d, candela=%d\n", brightness, lcd->bl, index_brightness_table[lcd->bl]);
	}

	return 0;
}



static int init_gamma_table(struct lcd_info *lcd, u8 *mtp_data)
{
	int i, c, j, v;
	int ret = 0;
	int *pgamma;
	int **gamma;

	/* allocate memory for local gamma table */
	gamma = kzalloc(IBRIGHTNESS_MAX * sizeof(int *), GFP_KERNEL);
	if (IS_ERR_OR_NULL(gamma)) {
		pr_err("failed to allocate gamma table\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table;
	}

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		gamma[i] = kzalloc(IV_MAX*CI_MAX * sizeof(int), GFP_KERNEL);
		if (IS_ERR_OR_NULL(gamma[i])) {
			pr_err("failed to allocate gamma\n");
			ret = -ENOMEM;
			goto err_alloc_gamma;
		}
	}

	/* allocate memory for gamma table */
	lcd->gamma_table = kzalloc(IBRIGHTNESS_MAX * sizeof(u8 *), GFP_KERNEL);
	if (IS_ERR_OR_NULL(lcd->gamma_table)) {
		pr_err("failed to allocate gamma table 2\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table2;
	}

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		lcd->gamma_table[i] = kzalloc(GAMMA_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
		if (IS_ERR_OR_NULL(lcd->gamma_table[i])) {
			pr_err("failed to allocate gamma 2\n");
			ret = -ENOMEM;
			goto err_alloc_gamma2;
		}
		lcd->gamma_table[i][0] = 0xCA;
	}


	/* calculate gamma table */
	init_mtp_data(lcd, mtp_data);
	dynamic_aid(lcd->daid, gamma);



	/* relocate gamma order */
	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		/* Brightness table */
		v = IV_MAX - 1;
		pgamma = &gamma[i][v * CI_MAX];
		for (c = 0, j = 1; c < CI_MAX; c++, pgamma++) {
			if (*pgamma & 0x100)
				lcd->gamma_table[i][j++] = 1;
			else
				lcd->gamma_table[i][j++] = 0;

			lcd->gamma_table[i][j++] = *pgamma & 0xff;
		}

		for (v = IV_MAX - 2; v >= 0; v--) {
			pgamma = &gamma[i][v * CI_MAX];
			for (c = 0; c < CI_MAX; c++, pgamma++)
				lcd->gamma_table[i][j++] = *pgamma;
		}

		lcd->gamma_table[i][31] = 0;	/* VT: Green / Red */
		lcd->gamma_table[i][32] = 0;	/* VT: Blue */
	}

	/* free local gamma table */
	for (i = 0; i < IBRIGHTNESS_MAX; i++)
		kfree(gamma[i]);
	kfree(gamma);


	return 0;

err_alloc_gamma2:
	while (i > 0) {
		kfree(lcd->gamma_table[i-1]);
		i--;
	}
	kfree(lcd->gamma_table);
err_alloc_gamma_table2:
	i = IBRIGHTNESS_MAX;
err_alloc_gamma:
	while (i > 0) {
		kfree(gamma[i-1]);
		i--;
	}
	kfree(gamma);
err_alloc_gamma_table:
	return ret;
}

static int init_aid_dimming_table(struct lcd_info *lcd)
{
	int i;

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		memcpy(lcd->aor[i], SEQ_AID_SET, AID_PARAM_SIZE);
		lcd->aor[i][3] = aor_cmd[i][0];
		lcd->aor[i][4] = aor_cmd[i][1];
	}

	return 0;
}

static int init_elvss_table(struct lcd_info *lcd, u8 *elvss_data)
{
	int i, k, ret;

	for (k = 0; k < 2; k++) {
		lcd->elvss_table[k] = kzalloc(ELVSS_STATUS_MAX * sizeof(u8 *), GFP_KERNEL);

		if (IS_ERR_OR_NULL(lcd->elvss_table[k])) {
			pr_err("failed to allocate elvss table\n");
			ret = -ENOMEM;
			goto err_alloc_elvss_table;
		}

		for (i = 0; i < ELVSS_STATUS_MAX; i++) {
			lcd->elvss_table[k][i] = kzalloc(ELVSS_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
			if (IS_ERR_OR_NULL(lcd->elvss_table[k][i])) {
				pr_err("failed to allocate elvss\n");
				ret = -ENOMEM;
				goto err_alloc_elvss;
			}

			memcpy(lcd->elvss_table[k][i], SEQ_ELVSS_SET, ELVSS_PARAM_SIZE);

			lcd->elvss_table[k][i][1] = ELVSS_TABLE[i];
		}

	}

	/* this (elvss_table[1]) is Smart ACL ON*/
	for (i = 0; i < ELVSS_STATUS_MAX; i++)
		lcd->elvss_table[1][i][1] += 4;
	
	return 0;

err_alloc_elvss:
	while (i > 0) {
		kfree(lcd->elvss_table[k][i-1]);
		i--;
	}
	kfree(lcd->elvss_table[k]);
err_alloc_elvss_table:
	return ret;
}

static int init_mps_table(struct lcd_info *lcd, u8 *mps_data)
{
	int i, j, ret;

	for (j = 0; j < HBM_STATUS_MAX; j++) {
		for (i = 0; i < MPS_STATUS_MAX; i++) {
			lcd->mps_table[j][i] = kzalloc(MPS_PARAM_SIZE * sizeof(u8 *), GFP_KERNEL);

			if (IS_ERR_OR_NULL(lcd->mps_table[j][i])) {
				pr_err("failed to allocate mps table\n");
				ret = -ENOMEM;
				goto err_alloc_mps;
			}

			lcd->mps_table[j][i][0] = LDI_MPS_REG;
			memcpy(&lcd->mps_table[j][i][1], mps_data, LDI_MPS_LEN);
		}
	}

	lcd->mps_table[HBM_ON][MPS_ON][3] = 0x4C;
	lcd->mps_table[HBM_OFF][MPS_ON][3] = 0x4C;
	lcd->mps_table[HBM_ON][MPS_OFF][3] = 0x48;
	lcd->mps_table[HBM_OFF][MPS_OFF][3] = 0x48;

	return 0;

err_alloc_mps:
	while (i > 0) {
		kfree(lcd->mps_table[j][i-1]);
		i--;
	}
	if (j > 0) {
		j--;
		i = HBM_STATUS_MAX;
		goto err_alloc_mps;
	}
	return ret;
}

static int init_hbm_parameter(struct lcd_info *lcd, const u8 *hbm_data)
{
	int i;

	/* CA 1~21th = DB 1~21 */
	for (i = 0; i < 21; i++)
		lcd->gamma_table[IBRIGHTNESS_500NT][i+1] = hbm_data[i];

	/* D4 18th = DB 34 */
	lcd->mps_table[HBM_ON][MPS_ON][18] = hbm_data[33];
	lcd->mps_table[HBM_ON][MPS_OFF][18] = hbm_data[33];

	lcd->elvss_table[1][ELVSS_STATUS_HBM][1] = lcd->elvss_table[0][ELVSS_STATUS_HBM][1];

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
        cmdreq.cb = NULL;
        cmdreq.rbuf = ctrl->rx_buf.data;
        
        /* This mutex is to sync up with dynamic FPS changes
    	 so that DSI lockups shall not happen*/
    	
    	BUG_ON(msd.ctrl_pdata == NULL);
        //mutex_lock(&msd.ctrl_pdata->dfps_mutex);
        mdss_dsi_cmdlist_put(ctrl, &cmdreq);
        //mutex_unlock(&msd.ctrl_pdata->dfps_mutex);
        
        /*blocked here, untill call back called*/
        return ctrl->rx_buf.len;
}


static void read_reg(int srcLength, char *destBuffer, struct msm_fb_data_type *pMFD, int add)
{
	/*
	*	EA8868 has 3 bank MTP register
	*	Each bank has 7 byte MTP data.
	*/
	const int one_read_size = 7;

	/* second byte is read address set */
	static char reg_set_read_register[] = { 0xFD, 0x00 };
	static struct dsi_cmd_desc ea8868_read_pos_cmd[] = {{
		{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(reg_set_read_register)}, reg_set_read_register}, };

	/* first byte = read buffer register */
	static char read_reg[] = { 0xFE, 0x00 };
	static struct dsi_cmd_desc ea8868_read_reg_cmd[] = {{
		{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(read_reg)}, read_reg}, };

	/* first byte is size of Register */
	static char packet_size[] = { 0x07, 0x00 };
	static struct dsi_cmd_desc ea8868_packet_size_cmd[] = {{
		{DTYPE_MAX_PKTSIZE, 1, 0, 0, 0, sizeof(packet_size)}, packet_size}, };

	static char reg_read_pos[] = { 0xB0, 0x00 };
	static struct dsi_cmd_desc ea8061_read_pos_cmd[] = {{
		{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(reg_read_pos)},
		reg_read_pos },};

	int readed_size, rx_cnt;
	int show_cnt = 0;
	int read_pos=0;

	int read_addr_cnt;
	char show_buffer[256] = {0,};
	int show_buffer_pos = 0;
	struct dsi_cmd_desc *cmd_desc;
	int cmd_size = 0;

	reg_set_read_register[1] = add;

	cmd_desc = ea8868_read_pos_cmd;
	cmd_size = ARRAY_SIZE(ea8868_read_pos_cmd);

	mdss_dsi_cmds_send(msd.ctrl_pdata, cmd_desc, cmd_size);

	/*set maxpacket size */
	packet_size[0] = (char)one_read_size;

	cmd_desc = ea8868_packet_size_cmd;
	cmd_size = ARRAY_SIZE(ea8868_packet_size_cmd);

	mdss_dsi_cmds_send(msd.ctrl_pdata, cmd_desc, cmd_size);

	for (read_addr_cnt = 0; read_addr_cnt < 16; read_addr_cnt++) {

		msleep(50);
		reg_read_pos[1] = read_pos;

		cmd_desc = ea8061_read_pos_cmd;
		cmd_size = ARRAY_SIZE(ea8061_read_pos_cmd);
		mdss_dsi_cmds_send(msd.ctrl_pdata, cmd_desc, cmd_size);

		readed_size = mdss_dsi_cmd_receive(msd.ctrl_pdata, ea8868_read_reg_cmd, one_read_size);

		for (rx_cnt = 0; rx_cnt < readed_size; rx_cnt++, show_cnt++) {
			if (destBuffer != NULL && show_cnt < srcLength){
				if(show_cnt < 33){
					destBuffer[show_cnt] = msd.ctrl_pdata->rx_buf.data[rx_cnt];
				}
			}
		}

		read_pos += readed_size;
		
		if (read_pos >= srcLength)
			break;
	}

	/* only LSB is available */
//	destBuffer[14] = 0x01;

	/* for debug */
	show_buffer_pos +=
		    snprintf(show_buffer, 256, "read_reg : ");

	for (read_addr_cnt = 0; read_addr_cnt < srcLength;) {
		show_buffer_pos +=
			snprintf(show_buffer + show_buffer_pos, 256,
				"%02x ", destBuffer[read_addr_cnt]);

		read_addr_cnt++;

		if (read_addr_cnt % 7 == 0)
			show_buffer_pos +=
			snprintf(show_buffer + show_buffer_pos, 256, ". ");
	}

	pr_debug("%s\n", show_buffer);
}

static void ea8061_read_id(struct lcd_info *lcd, u8 *buf)
{
	read_reg(LDI_ID_LEN, buf, msd.mfd,LDI_ID_REG);
}


static int ea8061_read_mtp(struct lcd_info *lcd, u8 *buf)
{
	int mtp_size = 33;
	read_reg(mtp_size, buf, msd.mfd,0xDA);

	return 0;
}

static void ea8061_read_coordinate(struct lcd_info *lcd)
{

	unsigned char buf[LDI_COORDINATE_LEN] = {0,};

	read_reg(LDI_COORDINATE_LEN, buf, msd.mfd,LDI_COORDINATE_REG);

	lcd->coordinate[0] = buf[0] << 8 | buf[1];	/* X */
	lcd->coordinate[1] = buf[2] << 8 | buf[3];	/* Y */

	pr_err("%s 0 = %d      1 = %d\n", __func__,lcd->coordinate[0],lcd->coordinate[1]);
}

static int ea8061_read_hbm(struct lcd_info *lcd, u8 *buf)
{
	read_reg(LDI_HBM_LEN, buf, msd.mfd,LDI_HBM_REG);
	return 0;
}

static int ea8061_read_elvss(struct lcd_info *lcd, u8 *buf)
{
	read_reg(LDI_ELVSS_LEN, buf, msd.mfd,LDI_ELVSS_REG);
	return 0;
}

static int ea8061_read_mps(struct lcd_info *lcd, u8 *buf)
{
	read_reg(LDI_MPS_LEN, buf, msd.mfd,LDI_MPS_REG);
	return 0;
}


static void init_dynamic_aid(struct lcd_info *lcd)
{
	lcd->daid.vreg = VREG_OUT_X1000;
	lcd->daid.iv_tbl = index_voltage_table;
	lcd->daid.iv_max = IV_MAX;
	lcd->daid.mtp = kzalloc(IV_MAX * CI_MAX * sizeof(int), GFP_KERNEL);
	lcd->daid.gamma_default = gamma_default;
	lcd->daid.formular = gamma_formula;
	lcd->daid.vt_voltage_value = vt_voltage_value;

	lcd->daid.ibr_tbl = index_brightness_table;
	lcd->daid.ibr_max = IBRIGHTNESS_MAX;
	lcd->daid.br_base = brightness_base_table;
	lcd->daid.gc_tbls = gamma_curve_tables;
	lcd->daid.gc_lut = gamma_curve_lut;
	lcd->daid.offset_gra = offset_gradation;
	lcd->daid.offset_color = (const struct rgb_t(*)[])offset_color;
}

struct lcd_info *lcd;

static void mdss_dsi_panel_bl_ctrl(struct mdss_panel_data *pdata,
							u32 bl_level)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	msd.ctrl_pdata = ctrl_pdata;

	if(first_boot == 0)
	{
		u8 mtp_data[LDI_MTP_LEN] = {0,};
		u8 elvss_data[LDI_ELVSS_LEN] = {0,};
		u8 hbm_data[LDI_HBM_LEN] = {0,};
		u8 mps_data[LDI_MPS_LEN] = {0,};

		lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);

		lcd->bl = DEFAULT_GAMMA_LEVEL;
		lcd->current_bl = lcd->bl;
		lcd->acl_enable = 0;
		lcd->current_acl = 0;
		lcd->ldi_enable = 1;
		lcd->auto_brightness = 0;
		lcd->connected = 1;
		lcd->siop_enable = 0;
		lcd->temperature = 10;
		lcd->current_temperature = 10;
		lcd->current_mps = 0;
		
		msd.mfd = (struct msm_fb_data_type *)registered_fb[0]->par;
		msd.mfd->resume_state = MIPI_RESUME_STATE;
		msd.mpd = pdata;
		first_boot =1;

		ea8061_read_id(lcd, lcd->id);
		ea8061_read_mtp(lcd, mtp_data);
		ea8061_read_elvss(lcd, elvss_data);
		ea8061_read_coordinate(lcd);
		ea8061_read_hbm(lcd, hbm_data);
		ea8061_read_mps(lcd, mps_data);

		init_dynamic_aid(lcd);

		init_gamma_table(lcd, mtp_data);
		init_aid_dimming_table(lcd);
		init_elvss_table(lcd, elvss_data);
		init_mps_table(lcd, mps_data);
		init_hbm_parameter(lcd, hbm_data); 
	}

#if defined(CONFIG_LCD_CONNECTION_CHECK)
	if (is_lcd_attached() == 0)
	{
		printk("%s: LCD not connected!\n",__func__);
		return;
	}
#endif


	if( msd.mfd->panel_power_on == false){
		pr_err("%s: panel power off no bl ctrl\n", __func__);
		return;
	}
#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	if (err_fg_working) {
		pr_info("[LCD] %s : esd is working!! return.. \n", __func__);
		return;
	}
#endif


	switch (ctrl_pdata->bklt_ctrl) {
	case BL_WLED:
		led_trigger_event(bl_led_trigger, bl_level);
		break;
	case BL_PWM:
		mdss_dsi_panel_bklt_pwm(ctrl_pdata, bl_level);
		break;
	case BL_DCS_CMD:
		update_brightness(lcd,1,ctrl_pdata,bl_level);
		break;
	default:
		pr_err("%s: Unknown bl_ctrl configuration\n",
			__func__);
		break;
	}
}


static int mdss_dsi_panel_on(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	msd.mfd = (struct msm_fb_data_type *)registered_fb[0]->par;
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	msd.mpd = pdata;
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	
	pr_debug("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	if (ctrl->on_cmds.cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->on_cmds);

	msd.mfd->resume_state = MIPI_RESUME_STATE;

#if defined(CONFIG_MDNIE_LITE_TUNING) || defined(CONFIG_MDNIE_VIDEO_ENHANCED)
	is_negative_on();
#endif
#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	enable_irq(err_fg_gpio);
#endif
	pr_info("%s:-\n", __func__);
	return 0;
}

static int mdss_dsi_panel_off(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	msd.mfd = (struct msm_fb_data_type *)registered_fb[0]->par;
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	if (!err_fg_working) {
		disable_irq_nosync(err_fg_gpio);
		cancel_work_sync(&err_fg_work);
	}
#endif
	pr_debug("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	if (ctrl->off_cmds.cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->off_cmds);

	msd.mfd->resume_state = MIPI_SUSPEND_STATE;
	pr_info("%s:-\n", __func__);
	return 0;
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
	while (len > sizeof(*dchdr)) {
		dchdr = (struct dsi_ctrl_hdr *)bp;
		dchdr->dlen = ntohs(dchdr->dlen);
		if (dchdr->dlen > len) {
			pr_err("%s: dtsi cmd=%x error, len=%d",
				__func__, dchdr->dtype, dchdr->dlen);
			return -ENOMEM;
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
		kfree(buf);
		return -ENOMEM;
	}

	pcmds->cmds = kzalloc(cnt * sizeof(struct dsi_cmd_desc),
						GFP_KERNEL);
	if (!pcmds->cmds){
		kfree(buf);
		return -ENOMEM;
	}

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


	data = of_get_property(np, link_key, NULL);
	if (!strncmp(data, "dsi_hs_mode", 11))
		pcmds->link_state = DSI_HS_MODE;
	else
		pcmds->link_state = DSI_LP_MODE;
	pr_debug("%s: dcs_cmd=%x len=%d, cmd_cnt=%d link_state=%d\n", __func__,
		pcmds->buf[0], pcmds->blen, pcmds->cmd_cnt, pcmds->link_state);

	return 0;
}
static int mdss_panel_dt_get_dst_fmt(u32 bpp, char mipi_mode, u32 pixel_packing,
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
static int mdss_panel_parse_dt(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	u32	tmp;
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
	rc = of_property_read_u32(np, "qcom,mdss-dsi-pixel-packing", &tmp);
	tmp = (!rc ? tmp : 0);
	rc = mdss_panel_dt_get_dst_fmt(pinfo->bpp,
		pinfo->mipi.mode, tmp,
		&(pinfo->mipi.dst_format));
	if (rc) {
		pr_debug("%s: problem determining dst format. Set Default\n",
			__func__);
		pinfo->mipi.dst_format =
			DSI_VIDEO_DST_FORMAT_RGB888;
	}
	
	pdest = of_get_property(np,
			"qcom,mdss-dsi-panel-destination", NULL);
	if (strlen(pdest) != 9) {
		pr_err("%s: Unknown pdest specified\n", __func__);
		return -EINVAL;
	}
	if (!strncmp(pdest, "display_1", 9))
		pinfo->pdest = DISPLAY_1;
	else if (!strncmp(pdest, "display_2", 9))
		pinfo->pdest = DISPLAY_2;
	else {
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
	pinfo->mipi.bllp_power_stop = of_property_read_bool(np,
		"qcom,mdss-dsi-bllp-power-mode");
	pinfo->mipi.eof_bllp_power_stop = of_property_read_bool(
		np, "qcom,mdss-dsi-bllp-eof-power-mode");
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-traffic-mode", &tmp);
	pinfo->mipi.traffic_mode =
			(!rc ? tmp : DSI_NON_BURST_SYNCH_PULSE);

	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-te-dcs-command", &tmp);
	pinfo->mipi.insert_dcs_cmd =
			(!rc ? tmp : 1);

	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-te-v-sync-continue-lines", &tmp);
	pinfo->mipi.wr_mem_continue =
			(!rc ? tmp : 0x3c);

	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-te-v-sync-rd-ptr-irq-line", &tmp);
	pinfo->mipi.wr_mem_start =
			(!rc ? tmp : 0x2c);

	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-te-pin-select", &tmp);
	pinfo->mipi.te_sel =
			(!rc ? tmp : 1);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-virtual-channel-id", &tmp);
	pinfo->mipi.vc = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-color-order", &tmp);
	pinfo->mipi.rgb_swap = (!rc ? tmp : DSI_RGB_SWAP_RGB);
	
	rc = of_property_read_u32(np, "qcom,mdss-force-clk-lane-hs", &tmp);
	pinfo->mipi.force_clk_lane_hs = (!rc ? tmp : 0);

	pinfo->mipi.data_lane0 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-0-state");
	pinfo->mipi.data_lane1 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-1-state");
	pinfo->mipi.data_lane2 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-2-state");
	pinfo->mipi.data_lane3 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-3-state");

	rc = of_property_read_u32(np, "qcom,mdss-dsi-lane-map", &tmp);
	pinfo->mipi.dlane_swap = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-t-clk-pre", &tmp);
	pinfo->mipi.t_clk_pre = (!rc ? tmp : 0x24);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-t-clk-post", &tmp);
	pinfo->mipi.t_clk_post = (!rc ? tmp : 0x03);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-stream", &tmp);
	pinfo->mipi.stream = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-mdp-trigger", &tmp);
	pinfo->mipi.mdp_trigger =
			(!rc ? tmp : DSI_CMD_TRIGGER_SW);
	if (pinfo->mipi.mdp_trigger > 6) {
		pr_err("%s:%d, Invalid mdp trigger. Forcing to sw trigger",
						 __func__, __LINE__);
		pinfo->mipi.mdp_trigger =
					DSI_CMD_TRIGGER_SW;
	}

	rc = of_property_read_u32(np, "qcom,mdss-dsi-dma-trigger", &tmp);
	pinfo->mipi.dma_trigger =
			(!rc ? tmp : DSI_CMD_TRIGGER_SW);
	if (pinfo->mipi.dma_trigger > 6) {
		pr_err("%s:%d, Invalid dma trigger. Forcing to sw trigger",
						 __func__, __LINE__);
		pinfo->mipi.dma_trigger =
					DSI_CMD_TRIGGER_SW;
	}
	data = of_get_property(np, "qcom,mdss-dsi-panel-mode-gpio-state", &tmp);
	if (data) {
		if (!strcmp(data, "high"))
			pinfo->mode_gpio_state = MODE_GPIO_HIGH;
		else if (!strcmp(data, "low"))
			pinfo->mode_gpio_state = MODE_GPIO_LOW;
	} else {
		pinfo->mode_gpio_state = MODE_GPIO_NOT_VALID;
	}

	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-frame-rate", &tmp);
	pinfo->mipi.frame_rate = (!rc ? tmp : 60);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-clock-rate", &tmp);
	pinfo->clk_rate = (!rc ? tmp : 0);

	data = of_get_property(np, "qcom,mdss-dsi-panel-timings", &len);
	if ((!data) || (len != 12)) {
		pr_err("%s:%d, Unable to read Phy timing settings",
		       __func__, __LINE__);
		goto error;
	}
	for (i = 0; i < len; i++)
		pinfo->mipi.dsi_phy_db.timing[i] = data[i];

	mdss_dsi_parse_fbc_params(np, pinfo);

	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->on_cmds,
		"qcom,mdss-dsi-on-command", "qcom,mdss-dsi-on-command-state");

	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->off_cmds,
		"qcom,mdss-dsi-off-command", "qcom,mdss-dsi-off-command-state");
	return 0;
error:
	return -EINVAL;
}

#if defined(CONFIG_LCD_CLASS_DEVICE)

static ssize_t mdss_sharp_disp_get_power(struct device *dev,
			struct device_attribute *attr, char *buf)
{

	pr_info("mipi_samsung_disp_get_power(0)\n");

	return 0;
}

static ssize_t mdss_sharp_disp_set_power(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int power;
	if (sscanf(buf, "%u", &power) != 1)
		return -EINVAL;

	pr_info("mipi_samsung_disp_set_power:%d\n",power);

	return size;
}

static DEVICE_ATTR(lcd_power, S_IRUGO | S_IWUSR | S_IWGRP,
			mdss_sharp_disp_get_power,
			mdss_sharp_disp_set_power);

static ssize_t mdss_siop_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf(buf, 2, "%d\n",msd.dstat.siop_status);
	pr_info("%s :[MDSS_SHARP] CABC: %d\n", __func__, msd.dstat.siop_status);
	return rc;
}
static ssize_t mdss_siop_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	
	if (sysfs_streq(buf, "1") && !msd.dstat.siop_status)
		msd.dstat.siop_status = true;
	else if (sysfs_streq(buf, "0") && msd.dstat.siop_status)
		msd.dstat.siop_status = false;
	else
		pr_info("%s: Invalid argument!!", __func__);
	
	return size;

}

static DEVICE_ATTR(siop_enable, S_IRUGO | S_IWUSR | S_IWGRP,
			mdss_siop_enable_show,
			mdss_siop_enable_store);


static struct lcd_ops mdss_sharp_disp_props = {

	.get_power = NULL,
	.set_power = NULL,

};

static ssize_t mdss_sharp_auto_brightness_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf(buf, sizeof(buf), "%d\n",
					msd.dstat.auto_brightness);
	pr_info("%s : auto_brightness : %d\n", __func__, msd.dstat.auto_brightness);

	return rc;
}

static ssize_t mdss_sharp_auto_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	static unsigned char prev_auto_brightness;
	struct mdss_panel_data *pdata = msd.mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
#if defined(CONFIG_LCD_CONNECTION_CHECK)
	if (is_lcd_attached() == 0)
	{
		printk("%s: LCD not connected!\n",__func__);
		return size;
	}
#endif

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
	else if (sysfs_streq(buf, "6"))
		msd.dstat.auto_brightness = 6;
	else
		pr_info("%s: Invalid argument!!", __func__);

	if(prev_auto_brightness == msd.dstat.auto_brightness)
		return size;

	mdelay(1);

	if((msd.dstat.auto_brightness >=5 )|| (msd.dstat.auto_brightness == 0 ))
		msd.dstat.siop_status = false;
	else
		msd.dstat.siop_status = true;
	if(msd.mfd == NULL){
		pr_err("%s: mfd not initialized\n", __func__);
		return size;
	}

	if( msd.mfd->panel_power_on == false){
		pr_err("%s: panel power off no bl ctrl\n", __func__);
		return size;
	}

	if(pdata == NULL){
		pr_err("%s: pdata not available... skipping update\n", __func__);
		return size;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
						panel_data);

	prev_auto_brightness = msd.dstat.auto_brightness;
	pr_info("%s %d %d\n", __func__, msd.dstat.auto_brightness, msd.dstat.siop_status);
	return size;
}

static DEVICE_ATTR(auto_brightness, S_IRUGO | S_IWUSR | S_IWGRP,
			mdss_sharp_auto_brightness_show,
			mdss_sharp_auto_brightness_store);

static ssize_t mdss_disp_lcdtype_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char temp[20];

	snprintf(temp, 20, "SMD_AMS549BU01");
	strncat(buf, temp, 20);
	return strnlen(buf, 20);
}
static DEVICE_ATTR(lcd_type, S_IRUGO, mdss_disp_lcdtype_show, NULL);
unsigned int mdss_dsi_show_cabc(void )
{
	return msd.dstat.siop_status;
}
#endif
#if 0// defined(CONFIG_ESD_ERR_FG_RECOVERY)
static irqreturn_t err_fg_irq_handler(int irq, void *handle)
{
	pr_info("%s : handler start", __func__);
	disable_irq_nosync(err_fg_gpio);
	schedule_work(&err_fg_work);
	pr_info("%s : handler end", __func__);

	return IRQ_HANDLED;
}
static void err_fg_work_func(struct work_struct *work)
{
	int bl_backup;
	struct mdss_panel_data *pdata = msd.mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if(msd.mfd == NULL){
		pr_err("%s: mfd not initialized Skip ESD recovery\n", __func__);
		return;
	}
	if(pdata == NULL){
		pr_err("%s: pdata not available... skipping update\n", __func__);
		return;
	}
	bl_backup=msd.mfd->bl_level;
	if( msd.mfd->panel_power_on == false){
		pr_err("%s: Display off Skip ESD recovery\n", __func__);
		return;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
						panel_data);

	pr_info("%s : start", __func__);
	err_fg_working = 1;
	msd.mfd->fbi->fbops->fb_blank(FB_BLANK_POWERDOWN, msd.mfd->fbi);
	msd.mfd->fbi->fbops->fb_blank(FB_BLANK_UNBLANK, msd.mfd->fbi);
	esd_count++;
	err_fg_working = 0;
	msd.mfd->bl_level=bl_backup;
	mdss_dsi_panel_bl_ctrl(pdata,msd.mfd->bl_level);
	pr_info("%s : end", __func__);
	return;
}
#ifdef ESD_DEBUG
static ssize_t mipi_samsung_esd_check_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, 20, "esd count:%d \n",esd_count);

	return rc;
}
static ssize_t mipi_samsung_esd_check_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd;
	mfd = platform_get_drvdata(msd.msm_pdev);

	err_fg_irq_handler(0, mfd);
	return 1;
}

static DEVICE_ATTR(esd_check, S_IRUGO , mipi_samsung_esd_check_show,\
			 mipi_samsung_esd_check_store);
#endif
#endif
#ifdef DDI_VIDEO_ENHANCE_TUNING
#define MAX_FILE_NAME 128
#define TUNING_FILE_PATH "/sdcard/"
#define TUNE_FIRST_SIZE 5
#define TUNE_SECOND_SIZE 92
static char tuning_file[MAX_FILE_NAME];
static char mdni_tuning1[TUNE_FIRST_SIZE];
static char mdni_tuning2[TUNE_SECOND_SIZE];
static struct dsi_cmd_desc mdni_tune_cmd[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(mdni_tuning2)}, mdni_tuning2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(mdni_tuning1)}, mdni_tuning1},
};
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
	struct mdss_panel_data *pdata = msd.mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
						panel_data);

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
		printk(KERN_INFO"0x%x ", mdni_tuning2[data_pos]);
	printk(KERN_INFO "\n");
	mutex_lock(&msd.lock);
	mdss_dsi_cmds_send(ctrl_pdata, mdni_tune_cmd, ARRAY_SIZE(mdni_tune_cmd));
	mutex_unlock(&msd.lock);

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

static ssize_t tuning_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, MAX_FILE_NAME, "tuned file name : %s\n",
								tuning_file);

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

	return size;
}
static DEVICE_ATTR(tuning, S_IRUGO | S_IWUSR | S_IWGRP,
			tuning_show,
			tuning_store);
#endif
#if defined(CONFIG_LCD_CONNECTION_CHECK)
int is_lcd_attached(void)
{
	return lcd_connected_status;
}
EXPORT_SYMBOL(is_lcd_attached);

static int __init lcd_attached_status(char *state)
{
	/*
	*	1 is lcd attached
	*	0 is lcd detached
	*/

	if (strncmp(state, "1", 1) == 0)
		lcd_connected_status = 1;
	else
		lcd_connected_status = 0;

	pr_info("%s %s", __func__, lcd_connected_status == 1 ?
				"lcd_attached" : "lcd_detached");
	return 1;
}
__setup("lcd_attached=", lcd_attached_status);

static int __init detect_lcd_panel_vendor(char* read_id)
{
	/*
	*	0x591810 --> sharp
	*	0x591060 --> JDI
	*/
	int lcd_id = simple_strtol(read_id, NULL, 16);
	if(lcd_id == 0x591810) {
		panel_vendor = "SHARP";
		is_sharp_panel=1;
	}
	else if(lcd_id == 0x591060) {
		panel_vendor = "JDI";
		is_sharp_panel=0;
	}
	else{
		pr_info("%s: manufacture id read may be faulty id[0x%x]\n", __func__, lcd_id);
		return 1;
	}
	pr_info("%s: detected panel vendor --> %s [0x%x]\n", __func__, panel_vendor, lcd_id);
	return 1;
}
__setup("lcd_id=0x", detect_lcd_panel_vendor);

#endif
int mdss_dsi_panel_init(struct device_node *node,
	struct mdss_dsi_ctrl_pdata *ctrl_pdata,
	bool cmd_cfg_cont_splash)
{
	int rc = 0;
	static const char *panel_name;
	bool cont_splash_enabled;
#if 0
	bool partial_update_enabled;
#endif
#if 0 //defined(CONFIG_ESD_ERR_FG_RECOVERY)
	int disp_esd_gpio;
#endif
#if defined(CONFIG_LCD_CLASS_DEVICE)
	struct lcd_device *lcd_device;

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	struct backlight_device *bd = NULL;
#endif
#endif
#if defined(CONFIG_LCD_CLASS_DEVICE)
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;
	np = of_parse_phandle(node,
			"qcom,mdss-dsi-panel-controller", 0);
	if (!np) {
		pr_err("%s: Dsi controller node not initialized\n", __func__);
		return -EPROBE_DEFER;
	}

	pdev = of_find_device_by_node(np);
#endif

#if defined(CONFIG_LCD_CONNECTION_CHECK)
	printk("%s: LCD attached status: %d !\n",
				__func__, is_lcd_attached());
#endif
#ifdef DDI_VIDEO_ENHANCE_TUNING
	mutex_init(&msd.lock);
#endif
	if (!node) {
		pr_err("%s: no panel node\n", __func__);
		return -ENODEV;
	}

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

	if (cmd_cfg_cont_splash)
		cont_splash_enabled = of_property_read_bool(node,
				"qcom,cont-splash-enabled");
	else
		cont_splash_enabled = false;
	if (!cont_splash_enabled) {
		pr_info("%s:%d Continuous splash flag not found.\n",
				__func__, __LINE__);
		ctrl_pdata->panel_data.panel_info.cont_splash_enabled = 0;
	} else {
		pr_info("%s:%d Continuous splash flag enabled.\n",
				__func__, __LINE__);
		ctrl_pdata->panel_data.panel_info.cont_splash_enabled = 1;
	}
#if 0	// Blocked the code
	partial_update_enabled = of_property_read_bool(node,
						"qcom,partial-update-enabled");
	if (partial_update_enabled) {
		pr_info("%s:%d Partial update enabled.\n", __func__, __LINE__);
		ctrl_pdata->panel_data.panel_info.partial_update_enabled = 1;
		ctrl_pdata->partial_update_fnc = mdss_dsi_panel_partial_update;
	} else {
		pr_info("%s:%d Partial update disabled.\n", __func__, __LINE__);
		ctrl_pdata->panel_data.panel_info.partial_update_enabled = 0;
		ctrl_pdata->partial_update_fnc = NULL;
	}
#endif

	pr_err("HHJ 1");

#if defined(CONFIG_LCD_CONNECTION_CHECK)
	if (is_lcd_attached() == 0)
	{
		printk("%s: LCD not connected.... Disabling Continous Splash!\n",__func__);
		ctrl_pdata->panel_data.panel_info.cont_splash_enabled = 0;
	}
#endif
	ctrl_pdata->on = mdss_dsi_panel_on;
	ctrl_pdata->off = mdss_dsi_panel_off;
	ctrl_pdata->panel_reset = mdss_dsi_panel_reset;
	ctrl_pdata->panel_data.set_backlight = mdss_dsi_panel_bl_ctrl;

	pr_err("HHJ 2");

#if defined(CONFIG_LCD_CLASS_DEVICE)
	lcd_device = lcd_device_register("panel", &pdev->dev, NULL,
					&mdss_sharp_disp_props);

	if (IS_ERR(lcd_device)) {
		rc = PTR_ERR(lcd_device);
		printk(KERN_ERR "lcd : failed to register device\n");
		return rc;
	}

	sysfs_remove_file(&lcd_device->dev.kobj,&dev_attr_lcd_power.attr);

	rc = sysfs_create_file(&lcd_device->dev.kobj,&dev_attr_lcd_power.attr);

	if (rc) {
		pr_info("sysfs create fail-%s\n",dev_attr_lcd_power.attr.name);
	
	}
	rc = sysfs_create_file(&lcd_device->dev.kobj,
					&dev_attr_lcd_type.attr);
	if (rc) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_lcd_type.attr.name);
	}

	
	rc= sysfs_create_file(&lcd_device->dev.kobj,
					&dev_attr_siop_enable.attr);
	if (rc) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_siop_enable.attr.name);
	}
#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	bd = backlight_device_register("panel", &lcd_device->dev,
			NULL, NULL, NULL);
	if (IS_ERR(bd)) {
		rc = PTR_ERR(bd);
		pr_info("backlight : failed to register device\n");
		return rc;
	}
	rc= sysfs_create_file(&bd->dev.kobj,
					&dev_attr_auto_brightness.attr);
	if (rc) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_auto_brightness.attr.name);
	}


	
#endif
#endif
#if defined(DDI_VIDEO_ENHANCE_TUNING)
	rc = sysfs_create_file(&lcd_device->dev.kobj,
			&dev_attr_tuning.attr);
	if (rc) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_tuning.attr.name);
	}
#endif
#if defined(CONFIG_MDNIE_LITE_TUNING) || defined(CONFIG_MDNIE_VIDEO_ENHANCED)
		pr_info("[%s] CONFIG_MDNIE_LITE_TUNING ok ! initclass called!\n",__func__);
		init_mdnie_class();
#if defined(CONFIG_MDNIE_LITE_TUNING)
		mdnie_lite_tuning_init(&msd);
#endif
#endif
#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
#ifdef ESD_DEBUG
	rc = sysfs_create_file(&lcd_device->dev.kobj,
							&dev_attr_esd_check.attr);
	if (rc) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_esd_check.attr.name);
	}
#endif
	msd.msm_pdev = pdev;
#endif

	pr_err("HHJ 9");

	return 0;
}
