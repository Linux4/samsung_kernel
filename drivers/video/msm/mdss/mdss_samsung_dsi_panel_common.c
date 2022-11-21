/* Copyright (c) 2012, Samsung Electronics. All rights reserved.
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
#include <linux/pwm.h>
#include <linux/err.h>
#include <linux/lcd.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "mdss_dsi.h"
#include "mdss_samsung_dsi_panel_common.h"
#include "mdss_fb.h"

#if defined(CONFIG_MDNIE_LITE_TUNING)
#include "mdnie_lite_tuning.h"
#endif

#define DDI_VIDEO_ENHANCE_TUNING
#if defined(DDI_VIDEO_ENHANCE_TUNING)
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#endif

#include <asm/system_info.h>

#define SMART_ACL

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQHD_PT_PANEL)
#define SMART_VINT
#endif

#define HBM_RE

#define TEMPERATURE_ELVSS
#define PARTIAL_UPDATE

#define TEST_RESOLUTION  //for sysfs of panel resolution

static struct dsi_buf dsi_panel_tx_buf;
static struct dsi_buf dsi_panel_rx_buf;

static struct dsi_cmd hsync_on_seq;

static struct dsi_cmd display_on_cmd;
static struct dsi_cmd display_off_cmd;

static struct dsi_cmd test_key_enable_cmds;
static struct dsi_cmd test_key_disable_cmds;

static struct dsi_cmd nv_mtp_read_cmds;
static struct dsi_cmd nv_enable_cmds;
static struct dsi_cmd nv_disable_cmds;
static struct dsi_cmd manufacture_id_cmds;
static struct dsi_cmd manufacture_date_cmds;
static struct dsi_cmd ddi_id_cmds;
static struct dsi_cmd rddpm_cmds;
static struct dsi_cmd rddsm_cmds;


static struct dsi_cmd mtp_read_sysfs_cmds;

static struct dsi_cmd acl_off_cmd;

static struct cmd_map acl_map_table;
static struct candella_lux_map candela_map_table;

static struct dsi_cmd acl_cmds_list;
static struct dsi_cmd opr_avg_cal_cmd;
static struct dsi_cmd aclcont_cmds_list;
static struct dsi_cmd gamma_cmds_list;
static struct dsi_cmd elvss_cmds_list;
static struct dsi_cmd elvss_cmds_revI_list;

static struct cmd_map aid_map_table;
static struct dsi_cmd aid_cmds_list;

#if defined(HBM_RE)
static struct dsi_cmd nv_mtp_hbm_read_cmds;
static struct dsi_cmd nv_mtp_hbm2_read_cmds;
static struct dsi_cmd hbm_gamma_cmds_list;

static struct dsi_cmd hbm_off_cmd;
static struct dsi_cmd hbm_etc_cmds_list;
#endif
static struct dsi_cmd nv_mtp_elvss_read_cmds;

#if defined(CONFIG_MDNIE_LITE_TUNING)
static struct dsi_cmd nv_mdnie_read_cmds;
#endif
#ifdef DEBUG_LDI_STATUS
static struct dsi_cmd ldi_debug_cmds;
#endif
#if defined(TEMPERATURE_ELVSS)
static struct dsi_cmd elvss_lowtemp_cmds_list;
static struct dsi_cmd elvss_lowtemp2_cmds_list;
#endif
#if defined(SMART_ACL)
static struct dsi_cmd smart_acl_elvss_cmds_list;
static struct cmd_map smart_acl_elvss_map_table;
#endif
#if defined(SMART_VINT)
static struct dsi_cmd smart_vint_cmds_list;
static struct cmd_map smart_vint_map_table;
#endif

#if defined(PARTIAL_UPDATE)
static struct dsi_cmd partialdisp_on_cmd;
static struct dsi_cmd partialdisp_off_cmd;
static int partial_disp_range[2];
#endif
#ifdef CONFIG_FB_MSM_SAMSUNG_AMOLED_LOW_POWER_MODE
static struct dsi_cmd alpm_on_seq;
static struct dsi_cmd alpm_off_seq;
#endif
#if defined(DYNAMIC_FPS_USE_TE_CTRL)
int dynamic_fps_use_te_ctrl;
#endif
#if defined(CONFIG_LCD_HMT)
static struct dsi_cmd hmt_bright_cmds_list;
static struct dsi_cmd hmt_single_scan_enable;
static struct dsi_cmd hmt_disable;
static struct dsi_cmd hmt_reverse_enable;
static struct dsi_cmd hmt_reverse_disable;
static struct dsi_cmd hmt_aid_cmds_list;
static struct cmd_map aid_map_table_reverse_hmt;
static struct candella_lux_map candela_map_table_reverse_hmt;
static int is_first = 1;
static struct dsi_cmd hmt_150cd_read_cmds;
#endif

#if defined(CONFIG_WACOM_LCD_FREQ_COMPENSATE)
static struct dsi_cmd write_ldi_fps_cmds;
#endif

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_WQHD_PT_PANEL)
#define LDI_ADJ_VDDM_OFFSET
#endif

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQHD_PT_PANEL) || defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQXGA_PT_PANEL)
#define CONFIG_LCD_RECOVERY
#endif

#ifdef LDI_ADJ_VDDM_OFFSET
static struct dsi_cmd read_vdd_ref_cmds;
static struct dsi_cmd write_vdd_offset_cmds;
static struct dsi_cmd read_vddm_ref_cmds;
static struct dsi_cmd write_vddm_offset_cmds;
#endif

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FHD_FA2_PT_PANEL)
static struct dsi_cmd panel_set_te_osc_b;
static struct dsi_cmd panel_set_te_restore;
static struct dsi_cmd panel_set_te;
static struct dsi_cmd panel_set_te_1;
static struct dsi_cmd panel_set_te_2;
static struct dsi_cmd panel_osc_type_read_cmds;
extern int te_set_done;
#endif


static struct mipi_samsung_driver_data msd;
/*List of supported Panels with HW revision detail
 * (one structure per project)
 * {hw_rev,"label string given in panel dtsi file"}
 * */
static struct  panel_hrev panel_supp_cdp[]= {
	{"samsung amoled 1080p video mode dsi S6E8FA0 panel", PANEL_FHD_OCTA_S6E8FA0},
	{"samsung amoled 1080p video mode dsi S6E3FA0 panel", PANEL_FHD_OCTA_S6E3FA0},
	{"samsung amoled 1080p command mode dsi S6E3FA0 panel", PANEL_FHD_OCTA_S6E3FA0_CMD},
	{"samsung amoled 1080p command mode dsi S6E3FA2 panel",PANEL_FHD_OCTA_S6E3FA2_CMD },
	{"samsung amoled wqhd command mode dsi1 S6E3HA0 panel", PANEL_WQHD_OCTA_S6E3HA0_CMD },
	{"samsung amoled wqhd command mode dsi0 S6E3HA0 panel", PANEL_WQHD_OCTA_S6E3HA0_CMD },
	{"samsung amoled wqhd command mode dsi0 S6E3HA2X01 panel", PANEL_WQHD_OCTA_S6E3HA2X01_CMD },
	{"samsung amoled wqhd command mode dsi1 S6E3HA2X01 panel", PANEL_WQHD_OCTA_S6E3HA2X01_CMD },
	{"samsung amoled wqxga command mode dsi0 S6E3HA2X01 panel", PANEL_WQXGA_OCTA_S6E3HA2X01_CMD },
	{"samsung amoled wqxga command mode dsi1 S6E3HA2X01 panel", PANEL_WQXGA_OCTA_S6E3HA2X01_CMD },
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
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL},
};

#ifdef LDI_ADJ_VDDM_OFFSET
unsigned int ldi_vddm_lut[128][2] = {
	{0, 12}, {1, 12}, {2, 13}, {3, 14}, {4, 15}, {5, 16}, {6, 17}, {7, 18}, {8, 19}, {9, 20},
	{10, 22}, {11, 23}, {12, 24}, {13, 25}, {14, 26}, {15, 27}, {16, 28}, {17, 29}, {18, 30}, {19, 31},
	{20, 32}, {21, 33}, {22, 34}, {23, 35}, {24, 37}, {25, 38}, {26, 39}, {27, 40}, {28, 41}, {29, 42},
	{30, 43}, {31, 44}, {32, 45}, {33, 46}, {34, 47}, {35, 48}, {36, 49}, {37, 50}, {38, 51}, {39, 53},
	{40, 54}, {41, 55}, {42, 56}, {43, 57}, {44, 58}, {45, 59}, {46, 60}, {47, 61}, {48, 62}, {49, 63},
	{50, 63}, {51, 63}, {52, 63}, {53, 63}, {54, 63}, {55, 63}, {56, 63}, {57, 63}, {58, 63}, {59, 63},
	{60, 63}, {61, 63}, {62, 63}, {63, 63}, {64, 11}, {65, 10}, {66, 9}, {67, 8}, {68, 7}, {69, 6},
	{70, 4}, {71, 3}, {72, 2}, {73, 1}, {74, 64}, {75, 65}, {76, 66}, {77, 67}, {78, 68}, {79, 69},
	{80, 70}, {81, 71}, {82, 72}, {83, 73}, {84, 74}, {85, 76}, {86, 77}, {87, 78}, {88, 79}, {89, 80},
	{90, 81}, {91, 82}, {92, 83}, {93, 84}, {94, 85}, {95, 86}, {96, 87}, {97, 88}, {98, 89}, {99, 91},
	{100, 92}, {101, 93}, {102, 94}, {103, 95}, {104, 96}, {105, 97}, {106, 98}, {107, 99}, {108, 100}, {109, 101},
	{110, 102}, {111, 103}, {112, 104}, {113, 105}, {114, 107}, {115, 108}, {116, 109}, {117, 110}, {118, 111}, {119, 112},
	{120, 113}, {121, 114}, {122, 115}, {123, 116}, {124, 117}, {125, 118}, {126, 119}, {127, 120},
};
#endif

#define MAX_BR_PACKET_SIZE sizeof(brightness_packet)/sizeof(struct dsi_cmd_desc)

DEFINE_LED_TRIGGER(bl_led_trigger);

static struct mdss_dsi_phy_ctrl phy_params;

char board_rev;

static int lcd_attached = 1;
static int lcd_id = 0;

#if defined(CONFIG_LCD_RECOVERY)
#define ESD_DEBUG 1

struct work_struct  lcd_refresh_work;
static int oled_det_gpio = 0;

#if defined(CONFIG_LCD_RECOVERY_ERR_FG)
static int err_fg_gpio = 0;
#endif
static int lcd_refresh_count = 0;
int lcd_refresh_working = 0;

static int esd_enable = 0;

struct mdss_panel_data *pdata_dsi0;
struct mdss_panel_data *pdata_dsi1;
#endif

#if defined(CONFIG_LCD_CLASS_DEVICE) && defined(DDI_VIDEO_ENHANCE_TUNING)
#define MAX_FILE_NAME 128
#define TUNING_FILE_PATH "/sdcard/"

static char tuning_file[MAX_FILE_NAME];

#if !defined (CONFIG_MDNIE_LITE_TUNING)
#define MDNIE_TUNE_HEAD_SIZE 22
#define MDNIE_TUNE_BODY_SIZE 128
#endif

static char mdnie_head[MDNIE_TUNE_HEAD_SIZE];
static char mdnie_body[MDNIE_TUNE_BODY_SIZE];

static struct dsi_cmd_desc mdni_tune_cmd[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(mdnie_body)}, mdnie_body},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(mdnie_head)}, mdnie_head},
};
#endif

static int mipi_samsung_disp_send_cmd(
		enum mipi_samsung_cmd_list cmd,
		unsigned char lock);
extern void mdss_dsi_panel_touchsensing(int enable);
int get_lcd_attached(void);
int get_lcd_id(void);

int set_panel_rev(unsigned int id)
{
		switch (id & 0xFF) {
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQHD_PT_PANEL)
			case 0x00:
			case 0x01:
					pr_info("%s : 0x01 EVT0_T_wqhd_REVA\n",__func__);
					msd.id3 = EVT0_T_wqhd_REVA;
				break;
			case 0x02:
					pr_info("%s : 0x02 EVT0_T_wqhd_REVB\n",__func__);
					msd.id3 = EVT0_T_wqhd_REVB;
				break;
			case 0x03:
					pr_info("%s : 0x03 EVT0_T_wqhd_REVC\n",__func__);
					msd.id3 = EVT0_T_wqhd_REVC;
				break;
			case 0x04:
					pr_info("%s : 0x04 EVT0_T_wqhd_REVD\n",__func__);
					msd.id3 = EVT0_T_wqhd_REVD;
				break;
			case 0x05:
					pr_info("%s : 0x05 EVT0_T_wqhd_REVF\n",__func__);
					msd.id3 = EVT0_T_wqhd_REVF;
				break;
			case 0x06:
					pr_info("%s : 0x06 EVT0_T_wqhd_REVG\n",__func__);
					msd.id3 = EVT0_T_wqhd_REVG;
		        	break;
			case 0x07:
					pr_info("%s : 0x07 EVT0_T_wqhd_REVH\n",__func__);
					msd.id3 = EVT0_T_wqhd_REVH;
				break;
			case 0x08:
					pr_info("%s : 0x08 EVT0_T_wqhd_REVI\n",__func__);
					msd.id3 = EVT0_T_wqhd_REVI;
		        	break;
			case 0x09:
					pr_info("%s : 0x08 EVT0_T_wqhd_REVJ\n",__func__);
					msd.id3 = EVT0_T_wqhd_REVJ;
				break;
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQXGA_PT_PANEL)
			case 0x10:
			case 0x11:
				if(((id & 0xFF00) >> 8 )== YM4_PANEL){
					pr_info("%s : 0x10/0x11 EVT0_T_wqxga YM4\n",__func__);
					msd.id2 = YM4_PANEL;
					msd.id3 = EVT0_T_wqxga_REVA;
					break;
				}else{
					pr_info("%s : 0x10/0x11 EVT0_T_wqxga_REVA\n",__func__);
					msd.id3 = EVT0_T_wqxga_REVA;
				break;
				}
			case 0x12:
			case 0x13:
				pr_info("%s : 0x13 EVT0_T_wqxga_REVD\n",__func__);
				msd.id3 = EVT0_T_wqxga_REVD;
				break;
			case 0x14:
				pr_info("%s : 0x14 EVT0_T_wqxga_REVE\n",__func__);
				msd.id3 = EVT0_T_wqxga_REVE;
				break;
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FHD_FA2_PT_PANEL)
			case 0x00:
				pr_info("%s : 0x00 EVT0_K_fhd_REVB \n",__func__);
				msd.id3 = EVT0_K_fhd_REVB;
				break;
			case 0x01:
				pr_info("%s : 0x01 EVT0_K_fhd_REVF \n",__func__);
				msd.id3 = EVT0_K_fhd_REVF;
				break;
			case 0x02:
				pr_info("%s : 0x02 EVT0_K_fhd_REVG \n",__func__);
				msd.id3 = EVT0_K_fhd_REVG;
				break;
			case 0x12:
				pr_info("%s : 0x12 EVT1_K_fhd_REVH \n",__func__);
				msd.id3 = EVT1_K_fhd_REVH;
				break;
			case 0x13:
				pr_info("%s : 0x13 EVT1_K_fhd_REVI \n",__func__);
				msd.id3 = EVT1_K_fhd_REVI;
				break;
#else
			case 0x00:
				pr_info("%s : 0x01 EVT0_L_wqhd_REVC \n",__func__);
				msd.id3 = EVT0_K_wqhd_REVB;
				break;
			case 0x01:
				pr_info("%s : 0x01 EVT0_L_wqhd_REVC \n",__func__);
				msd.id3 = EVT0_K_wqhd_REVC;
				break;
			case 0x02:
				pr_info("%s : 0x02 EVT0_L_wqhd_REVD \n",__func__);
				msd.id3 = EVT0_K_wqhd_REVD;
				break;
			case 0x03:
				pr_info("%s : 0x03 EVT0_L_wqhd_REVE \n",__func__);
				msd.id3 = EVT0_K_wqhd_REVE;
				break;
			case 0x04:
				pr_info("%s : 0x04 EVT0_L_wqhd_REVF \n",__func__);
				msd.id3 = EVT0_K_wqhd_REVF;
				break;
			case 0x05:
				pr_info("%s : 0x05 EVT0_L_wqhd_REVG \n",__func__);
				msd.id3 = EVT0_K_wqhd_REVG;
				break;
			case 0x17:
				pr_info("%s : 0x17 EVT0_L_wqhd_REVI \n",__func__);
				msd.id3 = EVT0_K_wqhd_REVI;
				break;
			case 0x18:
				pr_info("%s : 0x18 EVT0_L_wqhd_REVJ \n",__func__);
				msd.id3 = EVT0_K_wqhd_REVJ;
				break;
			case 0x19:
				pr_info("%s : 0x18 EVT0_L_wqhd_REVK \n",__func__);
				msd.id3 = EVT0_K_wqhd_REVK;
				break;
			case 0x1A:
				pr_info("%s : 0x1A EVT0_L_wqhd_REVL \n",__func__);
				msd.id3 = EVT0_K_wqhd_REVL;
				break;
#endif
			default:
				pr_err("%s : can't find panel id.. \n", __func__);
				return -EINVAL;
				break;
		}

	return 1;
}

void mdss_dsi_panel_pwm_cfg(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int ret;

	if (!gpio_is_valid(ctrl->pwm_pmic_gpio)) {
		pr_err("%s: pwm_pmic_gpio=%d Invalid\n", __func__,
			ctrl->pwm_pmic_gpio);
		return;
	}

	ret = gpio_request(ctrl->pwm_pmic_gpio, "disp_pwm");
	if (ret) {
		pr_err("%s: pwm_pmic_gpio=%d request failed\n", __func__,
				ctrl->pwm_pmic_gpio);
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

	pr_debug("%s: bklt_ctrl=%d pwm_period=%d pwm_pmic_gpio=%d pwm_lpg_chan=%d\n",
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

static char dcs_cmd[2] = {0x54, 0x00}; /* DTYPE_DCS_READ */
static struct dsi_cmd_desc dcs_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(dcs_cmd)},
	dcs_cmd
};

static void dcs_read_cb(int data)
{
	pr_info("%s: bklt_ctrl=%x\n", __func__, data);
}

u32 mdss_dsi_dcs_read(struct mdss_dsi_ctrl_pdata *ctrl,
			char cmd0, char cmd1)
{
	struct dcs_cmd_req cmdreq;

	dcs_cmd[0] = cmd0;
	dcs_cmd[1] = cmd1;
	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &dcs_read_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq.rlen = 1;
	cmdreq.cb = dcs_read_cb; /* call back */
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
	/*
	 * blocked here, untill call back called
	 */
	return 0;
}

void mdss_dsi_samsung_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	static struct mdss_panel_alpm_data *alpm_data = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	if (unlikely(!alpm_data))
		alpm_data = &pdata->alpm_data;

	if (alpm_data->alpm_status) {
		pr_info("[ALPM_DEBUG] %s: Panel is not reset, enable : %d\n",
					__func__, enable);
		if (enable && alpm_data->alpm_status(CHECK_PREVIOUS_STATUS))
			return;
		else if (!enable && alpm_data->alpm_status(CHECK_CURRENT_STATUS)) {
			alpm_data->alpm_status(STORE_CURRENT_STATUS);
			return;
		}
	} else
		pr_info("[ALPM_DEBUG] %s: Panel reset, enable : %d",
					__func__, enable);

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_info("%s: enable(%d) ndx(%d)\n",
			   __func__,enable, ctrl_pdata->ndx );

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_err("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
		return;
	}

	if (enable) {
		gpio_set_value((ctrl_pdata->rst_gpio), 1);
		usleep_range(5000, 5000);
		pr_info("ctrl_pdata->rst_gpio = %d\n", gpio_get_value(ctrl_pdata->rst_gpio));
		wmb();
		gpio_set_value((ctrl_pdata->rst_gpio), 0);
		usleep_range(5000, 5000);
		pr_info("ctrl_pdata->rst_gpio = %d\n", gpio_get_value(ctrl_pdata->rst_gpio));
		wmb();
		gpio_set_value((ctrl_pdata->rst_gpio), 1);
		usleep_range(11000, 11000);
		pr_info("ctrl_pdata->rst_gpio = %d\n", gpio_get_value(ctrl_pdata->rst_gpio));
		wmb();

	} else {
		gpio_set_value((ctrl_pdata->rst_gpio), 0);

	}
}

/**
 * mdss_dsi_roi_merge() -  merge two roi into single roi
 *
 * Function used by partial update with only one dsi intf take 2A/2B
 * (column/page) dcs commands.
 */
static int mdss_dsi_roi_merge(struct mdss_dsi_ctrl_pdata *ctrl,
					struct mdss_rect *roi)
{
	struct mdss_panel_info *l_pinfo;
	struct mdss_rect *l_roi;
	struct mdss_rect *r_roi;
	struct mdss_dsi_ctrl_pdata *other = NULL;
	int ans = 0;

	if (ctrl->ndx == DSI_CTRL_LEFT) {
		other = mdss_dsi_get_ctrl_by_index(DSI_CTRL_RIGHT);
		if (!other)
			return ans;
		l_pinfo = &(ctrl->panel_data.panel_info);
		l_roi = &(ctrl->panel_data.panel_info.roi);
		r_roi = &(other->panel_data.panel_info.roi);
	} else  {
		other = mdss_dsi_get_ctrl_by_index(DSI_CTRL_LEFT);
		if (!other)
			return ans;
		l_pinfo = &(other->panel_data.panel_info);
		l_roi = &(other->panel_data.panel_info.roi);
		r_roi = &(ctrl->panel_data.panel_info.roi);
	}

	if (l_roi->w == 0 && l_roi->h == 0) {
		/* right only */
		*roi = *r_roi;
		roi->x += l_pinfo->xres;/* add left full width to x-offset */
	} else {
		/* left only and left+righ */
		*roi = *l_roi;
		roi->w +=  r_roi->w; /* add right width */
		ans = 1;
	}

	return ans;
}

static char caset[] = {0x2a, 0x00, 0x00, 0x03, 0x00};	/* DTYPE_DCS_LWRITE */
static char paset[] = {0x2b, 0x00, 0x00, 0x05, 0x00};	/* DTYPE_DCS_LWRITE */

/* pack into one frame before sent */
static struct dsi_cmd_desc set_col_page_addr_cmd[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(caset)}, caset},	/* packed */
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(paset)}, paset},
};

static int mdss_dsi_set_col_page_addr(struct mdss_panel_data *pdata)
{
	struct mdss_panel_info *pinfo;
	struct mdss_rect roi;
	struct mdss_rect *p_roi;
	struct mdss_rect *c_roi;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_dsi_ctrl_pdata *other = NULL;
	struct dcs_cmd_req cmdreq;
	int left_or_both = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pinfo = &pdata->panel_info;
	p_roi = &pinfo->roi;

	/*
	 * to avoid keep sending same col_page info to panel,
	 * if roi_merge enabled, the roi of left ctrl is used
	 * to compare against new merged roi and saved new
	 * merged roi to it after comparing.
	 * if roi_merge disabled, then the calling ctrl's roi
	 * and pinfo's roi are used to compare.
   	 */
	if (pinfo->partial_update_roi_merge) {
		left_or_both = mdss_dsi_roi_merge(ctrl, &roi);
		other = mdss_dsi_get_ctrl_by_index(DSI_CTRL_LEFT);
		c_roi = &other->roi;
	} else {
		c_roi = &ctrl->roi;
		roi = *p_roi;
	}

	/* roi had changed, do col_page update */
	if ( !mdss_rect_cmp(c_roi, &roi)) {
		pr_debug("%s: ndx=%d x=%d y=%d w=%d h=%d\n",
			__func__, ctrl->ndx, p_roi->x,
				p_roi->y, p_roi->w, p_roi->h);
		*c_roi = roi; /* keep to ctrl */
		if (c_roi->w == 0 || c_roi->h == 0) {
			/* no new frame update */
			pr_err("%s: ctrl=%d, no partial roi set\n",
						__func__, ctrl->ndx);
			if (!mdss_dsi_sync_wait_enable(ctrl))
				return 0;
		}

		pr_debug("%s: ndx=%d x=%d y=%d w=%d h=%d\n",
				__func__, ctrl->ndx, roi.x,
				roi.y, roi.w, roi.h);

		if (pinfo->dcs_cmd_by_left) {
			if (left_or_both && ctrl->ndx == DSI_CTRL_RIGHT) {
				/* 2A/2B sent by left already */
			pr_err("%s: ctrl=%d, sned-by-left\n",
						__func__, ctrl->ndx);
				return 0;
			}
		}

		caset[1] = (((roi.x) & 0xFF00) >> 8);
		caset[2] = (((roi.x) & 0xFF));
		caset[3] = (((roi.x - 1 + roi.w) & 0xFF00) >> 8);
		caset[4] = (((roi.x - 1 + roi.w) & 0xFF));

		pr_debug("%s:{0x%0x,0x%0x,0x%0x,0x%0x,0x%0x}\n",__func__,
				caset[0],caset[1],caset[2],caset[3],caset[4]);

		set_col_page_addr_cmd[0].payload = caset;

		paset[1] = (((roi.y) & 0xFF00) >> 8);
		paset[2] = (((roi.y) & 0xFF));
		paset[3] = (((roi.y - 1 + roi.h) & 0xFF00) >> 8);
		paset[4] = (((roi.y - 1 + roi.h) & 0xFF));

		pr_debug("%s:{0x%0x,0x%0x,0x%0x,0x%0x,0x%0x}\n",__func__,
				paset[0],paset[1],paset[2],paset[3],paset[4]);

		set_col_page_addr_cmd[1].payload = paset;

		memset(&cmdreq, 0, sizeof(cmdreq));
		cmdreq.cmds = set_col_page_addr_cmd;
		cmdreq.cmds_cnt = 2;
		cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;

		if (pinfo->dcs_cmd_by_left)
			ctrl = mdss_dsi_get_ctrl_by_index(DSI_CTRL_LEFT);
		else if(mdss_dsi_sync_wait_enable(ctrl))
			ctrl = mdss_dsi_get_ctrl_by_index(DSI_CTRL_RIGHT);

		mdss_dsi_cmdlist_put(ctrl, &cmdreq);
	}

	return 0;
}

static int get_candela_value(int bl_level)
{
	return candela_map_table.lux_tab[candela_map_table.bkl[bl_level]];
}

static int get_cmd_idx(int bl_level)
{
	return candela_map_table.cmd_idx[candela_map_table.bkl[bl_level]];
}

static struct dsi_cmd get_testKey_set(int enable)
{
	struct dsi_cmd testKey = {0,};

	if (enable)
		testKey.cmd_desc = &(test_key_enable_cmds.cmd_desc[0]);
	else
		testKey.cmd_desc = &(test_key_disable_cmds.cmd_desc[0]);

	testKey.num_of_cmds = 1;

	return testKey;
}

#if defined(HBM_RE)
static struct dsi_cmd get_hbm_off_set(void)
{
	struct dsi_cmd hbm_off = {0,};

	hbm_off.cmd_desc = &(hbm_off_cmd.cmd_desc[0]);
	hbm_off.num_of_cmds = 1;

	return hbm_off;
}
#endif

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

end:
	return aid_control;

}

/*
	This function takes acl_map_table and uses cd_idx,
	to get the index of the command in elvss command list.

*/

static struct dsi_cmd get_acl_control_on_set(void)
{
	struct dsi_cmd aclcont_control = {0,};
	int acl_cond = msd.dstat.curr_acl_cond;

	if (acl_cond) /* already acl condition setted */
		goto end;

	/* Get the command desc */
	aclcont_control.cmd_desc = aclcont_cmds_list.cmd_desc;
	aclcont_control.num_of_cmds = aclcont_cmds_list.num_of_cmds;
	msd.dstat.curr_acl_cond = 1;

	pr_info("%s #(%d)\n",
				__func__, aclcont_cmds_list.num_of_cmds);

end:
	return aclcont_control;
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
	acl_control.cmd_desc = acl_off_cmd.cmd_desc; /* idx 0 : ACL OFF */
	acl_control.num_of_cmds = acl_off_cmd.num_of_cmds;

	pr_info("%s #(%d)\n",
				__func__, acl_off_cmd.num_of_cmds);

	msd.dstat.curr_acl_idx = 0;
	msd.dstat.curr_acl_cond = 0;

end:
	return acl_control;
}

#if 0 /*not used*/
static struct dsi_cmd get_orp_avg_cal_set(void)
{
	struct dsi_cmd opr_set = {0,};
	char *p_payload, *c_payload;
	int idx, payload_size = 0;
	int p_idx = msd.dstat.curr_opr_idx;

	if (!opr_avg_cal_cmd.cmds_len)
		goto end;

	if (msd.dstat.acl_on)
		idx = 0;
	else
		idx = 1;

	c_payload = opr_avg_cal_cmd.cmd_desc[idx].payload;

	if (p_idx >=0) {
		p_payload = opr_avg_cal_cmd.cmd_desc[idx].payload;
		payload_size = opr_avg_cal_cmd.cmd_desc[idx].dchdr.dlen;
		if (!memcmp(p_payload, c_payload, payload_size))
			goto end;
	}

	/* Get the command desc */
	opr_set.cmd_desc = &(opr_avg_cal_cmd.cmd_desc[idx]);
	opr_set.num_of_cmds = 1;

end:
	return opr_set;
}
#endif

#if defined(TEMPERATURE_ELVSS)
// ELVSS TEMPERATURE COMPENSATION for S6E3FA0
static struct dsi_cmd get_elvss_tempcompen_control_set(void)
{
	struct dsi_cmd elvss_tempcompen_control = {0,};

	pr_debug("%s for ELVSS CONTROL acl(%d), temp(%d)\n",
				__func__, msd.dstat.acl_on, msd.dstat.temperature);

	/* Get the command desc */
	if (msd.dstat.temperature > 0) {
		pr_debug("%s temp > 0 \n",__func__);
		elvss_lowtemp_cmds_list.cmd_desc[1].payload[1] = 0x19; // B8
	} else if (msd.dstat.temperature > -20) {
		pr_debug("%s 0 >= temp > -20 \n",__func__);
		elvss_lowtemp_cmds_list.cmd_desc[1].payload[1] = 0x00; // B8
	} else {
		pr_debug("%s temp <= -20 \n",__func__);
		elvss_lowtemp_cmds_list.cmd_desc[1].payload[1] = 0x94; // B8
	}

	pr_info("%s for ELVSS CONTROL acl(%d), temp(%d) B8(0x%x) \n",
				__func__, msd.dstat.acl_on, msd.dstat.temperature,
				elvss_lowtemp_cmds_list.cmd_desc[1].payload[1]);

	elvss_tempcompen_control.cmd_desc = elvss_lowtemp_cmds_list.cmd_desc;
	elvss_tempcompen_control.num_of_cmds = elvss_lowtemp_cmds_list.num_of_cmds;

	return elvss_tempcompen_control;
}

static struct dsi_cmd get_elvss_tempcompen_control_set2(void)
{
	struct dsi_cmd elvss_tempcompen_control2 = {0,};

	pr_debug("%s for ELVSS CONTROL acl2(%d), temp(%d)\n",
				__func__, msd.dstat.acl_on, msd.dstat.temperature);

	/* Get the command desc */
	if (msd.dstat.temperature > -20) /*b6 21th para*/
		elvss_lowtemp2_cmds_list.cmd_desc[1].payload[1] = msd.dstat.elvss_value;
	else {
		/*temp <= -20 : b6 21th para-0x05*/
		if(msd.panel == PANEL_FHD_OCTA_S6E3FA2_CMD)
			elvss_lowtemp2_cmds_list.cmd_desc[1].payload[1] = (msd.dstat.elvss_value - 0x05);
		else
			elvss_lowtemp2_cmds_list.cmd_desc[1].payload[1] = (msd.dstat.elvss_value - 0x03);
	}

	pr_info("%s for ELVSS CONTROL acl(%d), temp(%d) B0(0x%x) B6(0x%x)\n",
				__func__, msd.dstat.acl_on, msd.dstat.temperature,
				elvss_lowtemp2_cmds_list.cmd_desc[0].payload[1],
				elvss_lowtemp2_cmds_list.cmd_desc[1].payload[1]);

	elvss_tempcompen_control2.cmd_desc = elvss_lowtemp2_cmds_list.cmd_desc;
	elvss_tempcompen_control2.num_of_cmds = elvss_lowtemp2_cmds_list.num_of_cmds;

	return elvss_tempcompen_control2;
}

#endif

/*
	This function takes acl_map_table and uses cd_idx,
	to get the index of the command in elvss command list.
*/

#ifdef SMART_ACL
static struct dsi_cmd get_elvss_control_set(int cd_idx)
{
	struct dsi_cmd elvss_control = {0,};
	int cmd_idx = 0;
	char *payload;

	pr_debug("%s for SMART_ACL acl(%d), temp(%d)\n",
			__func__, msd.dstat.acl_on, msd.dstat.temperature);

	if (!smart_acl_elvss_map_table.size || !(cd_idx < smart_acl_elvss_map_table.size) ||
			!smart_acl_elvss_map_table.size ||
			!(cd_idx < smart_acl_elvss_map_table.size)) {
		pr_err("%s failed mapping elvss table\n",__func__);
		goto end;
	}

	cmd_idx = smart_acl_elvss_map_table.cmd_idx[cd_idx];

	/* Get the command desc */
	if(msd.dstat.acl_on || msd.dstat.siop_status) {
		if (msd.panel == PANEL_WQHD_OCTA_S6E3HA0_CMD ||\
			msd.panel == PANEL_FHD_OCTA_S6E3FA2_CMD){
			if (msd.dstat.temperature > 0)
				smart_acl_elvss_cmds_list.cmd_desc[cmd_idx].payload[1] = 0x88;
			else
				smart_acl_elvss_cmds_list.cmd_desc[cmd_idx].payload[1] = 0x8C;
		}else{
			smart_acl_elvss_cmds_list.cmd_desc[cmd_idx].payload[1] = 0x8C;
		}

		payload = smart_acl_elvss_cmds_list.cmd_desc[cmd_idx].payload;
		elvss_control.cmd_desc = &(smart_acl_elvss_cmds_list.cmd_desc[cmd_idx]);


		pr_debug("ELVSS for SMART_ACL cd_idx=%d, cmd_idx=%d\n", cd_idx, cmd_idx);
	} else {
		if (msd.panel == PANEL_WQHD_OCTA_S6E3HA0_CMD ||\
			msd.panel == PANEL_FHD_OCTA_S6E3FA2_CMD){
			if (msd.dstat.temperature > 0)
				elvss_cmds_list.cmd_desc[cmd_idx].payload[1] = 0x98;
			else
				elvss_cmds_list.cmd_desc[cmd_idx].payload[1] = 0x9C;
		}else{
			elvss_cmds_list.cmd_desc[cmd_idx].payload[1] = 0x9C;

			if (cd_idx < 13) /*2~19nit*/
				elvss_cmds_list.cmd_desc[cmd_idx].payload[1] = 0x8C;	/*CAPS OFF*/
		}

		payload = elvss_cmds_list.cmd_desc[cmd_idx].payload;
		elvss_control.cmd_desc = &(elvss_cmds_list.cmd_desc[cmd_idx]);

		pr_debug("ELVSS for normal cd_idx=%d, cmd_idx=%d\n", cd_idx, cmd_idx);
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
	int cmd_idx = 0, payload_size = 0;
	char *p_payload, *c_payload;
	int p_idx = msd.dstat.curr_elvss_idx;

	if (!smart_acl_elvss_map_table.size || !(cd_idx < smart_acl_elvss_map_table.size))
	{
		pr_err("%s invalid elvss mapping \n",__func__);
		goto end;
	}

	/* Get index in the acl command list*/
	cmd_idx = smart_acl_elvss_map_table.cmd_idx[cd_idx];

	c_payload = elvss_cmds_list.cmd_desc[cmd_idx].payload;

	/* Check if current & previous commands are same */
	if (p_idx >= 0) {
		p_payload = elvss_cmds_list.cmd_desc[p_idx].payload;
		payload_size = elvss_cmds_list.cmd_desc[p_idx].dchdr.dlen;

		if (msd.dstat.curr_elvss_idx == cmd_idx ||
			!memcmp(p_payload, c_payload, payload_size))
			goto end;
	}

	elvss_control.cmd_desc = &(elvss_cmds_list.cmd_desc[cmd_idx]);

	elvss_control.num_of_cmds = 1;
	msd.dstat.curr_elvss_idx = cmd_idx;

end:
	return elvss_control;

}
#endif

#ifdef SMART_VINT
static struct dsi_cmd get_vint_control_set(int cd_idx)
{
	struct dsi_cmd vint_control = {0,};
	int cmd_idx = 0, payload_size = 0;
	char *p_payload, *c_payload;
	int p_idx = msd.dstat.curr_vint_idx;

	if (!smart_vint_map_table.size || !(cd_idx < smart_vint_map_table.size))
	{
		pr_err("%s invalid vint mapping \n",__func__);
		goto end;
	}

	/* Get index in the acl command list*/
	cmd_idx = smart_vint_map_table.cmd_idx[cd_idx];

	c_payload = smart_vint_cmds_list.cmd_desc[cmd_idx].payload;

	/* Check if current & previous commands are same */
	if (p_idx >= 0) {
		p_payload = smart_vint_cmds_list.cmd_desc[p_idx].payload;
		payload_size = smart_vint_cmds_list.cmd_desc[p_idx].dchdr.dlen;

		if (msd.dstat.curr_vint_idx == cmd_idx ||
			!memcmp(p_payload, c_payload, payload_size))
			goto end;
	}

	vint_control.cmd_desc = &(smart_vint_cmds_list.cmd_desc[cmd_idx]);

	vint_control.num_of_cmds = 1;
	msd.dstat.curr_vint_idx = cmd_idx;

end:
	return vint_control;

}

#endif
static struct dsi_cmd get_gamma_control_set(int candella)
{
	struct dsi_cmd gamma_control = {0,};
	/* Just a safety check to ensure smart dimming data is initialised well */
	BUG_ON(msd.sdimconf->generate_gamma == NULL);
	msd.sdimconf->generate_gamma(candella, &gamma_cmds_list.cmd_desc[0].payload[1]);

	gamma_control.cmd_desc = &(gamma_cmds_list.cmd_desc[0]);
	gamma_control.num_of_cmds = gamma_cmds_list.num_of_cmds;
	return gamma_control;
}

static int update_bright_packet(int cmd_count, struct dsi_cmd *cmd_set)
{
	int i = 0;

	if (cmd_count > (MAX_BR_PACKET_SIZE - 1))/*cmd_count is index, if cmd_count >13 then panic*/
		panic("over max brightness_packet size(%d).. !!", MAX_BR_PACKET_SIZE);

	for (i = 0; i < cmd_set->num_of_cmds; i++) {
		brightness_packet[cmd_count].payload = \
			cmd_set->cmd_desc[i].payload;
		brightness_packet[cmd_count].dchdr.dlen = \
			cmd_set->cmd_desc[i].dchdr.dlen;
		brightness_packet[cmd_count].dchdr.dtype = \
		cmd_set->cmd_desc[i].dchdr.dtype;
		brightness_packet[cmd_count].dchdr.wait = \
		cmd_set->cmd_desc[i].dchdr.wait;
		cmd_count++;
	}

	return cmd_count;
}

#if defined(HBM_RE)
static struct dsi_cmd get_hbm_etc_control_set(void)
{
	struct dsi_cmd etc_hbm_control = {0,};

	/* Get the command desc */
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

static int make_brightcontrol_hbm_set(void)
{

	struct dsi_cmd hbm_etc_control = {0,};
	struct dsi_cmd gamma_control = {0,};
	struct dsi_cmd testKey = {0, 0, 0, 0, 0};

	int cmd_count = 0;

	if (msd.dstat.hbm_mode) {
		pr_err("%s : already hbm mode! return .. \n", __func__);
		return 0;
	}

	/* level2 enable */
	testKey = get_testKey_set(1);
	cmd_count = update_bright_packet(cmd_count, &testKey);

	/*gamma*/
	if (msd.panel == PANEL_WQHD_OCTA_S6E3HA0_CMD ||\
		msd.panel == PANEL_FHD_OCTA_S6E3FA2_CMD){
		gamma_control = get_hbm_gamma_control_set();
		cmd_count = update_bright_packet(cmd_count, &gamma_control);
	}
	hbm_etc_control = get_hbm_etc_control_set();
	cmd_count = update_bright_packet(cmd_count, &hbm_etc_control);

	/* level2 disable */
	testKey = get_testKey_set(0);
	cmd_count = update_bright_packet(cmd_count, &testKey);

	/* for non hbm mode : reset */
	msd.dstat.curr_elvss_idx = -1;
#if defined(SMART_VINT)
	msd.dstat.curr_vint_idx = -1;
#endif
	msd.dstat.curr_acl_idx = -1;
	msd.dstat.curr_opr_idx = -1;
	msd.dstat.curr_aid_idx = -1;
	msd.dstat.curr_acl_cond = 0;

	LCD_DEBUG("HBM : %d\n", cmd_count);
	return cmd_count;

}
#endif

#if defined(CONFIG_LCD_HMT)
static struct smartdim_conf_hmt* get_smartdim_conf_hmt(void) {
	struct smartdim_conf_hmt *conf_hmt = NULL;

	conf_hmt = msd.sdimconf_reverse_hmt_single;

	return conf_hmt;
}
static struct candella_lux_map* get_candella_lux_map_hmt(void) {
	struct candella_lux_map *candela_map_table = NULL;

	candela_map_table = &candela_map_table_reverse_hmt;

	return candela_map_table;
}

static int get_candela_value_hmt(int bl_level)
{
	struct candella_lux_map *candela_map_table = get_candella_lux_map_hmt();

	return candela_map_table->lux_tab[candela_map_table->bkl[bl_level]];
}

static int get_cmd_idx_hmt(int bl_level)
{
	struct candella_lux_map *candela_map_table = get_candella_lux_map_hmt();

	return candela_map_table->cmd_idx[candela_map_table->bkl[bl_level]];
}

static void get_aor_set_hmt(int cd_idx)
{
	hmt_bright_cmds_list.cmd_desc[2] = hmt_aid_cmds_list.cmd_desc[cd_idx];

	return;
}

static void make_brightcontrol_set_hmt_tlte(int bl_level)
{
	int cd_idx = 0, cd_level =0;
	struct smartdim_conf_hmt *conf_hmt;

	if ( bl_level < 0 || bl_level > 255 ) {
		pr_err("[HMT] bl_level(%d) is out of range! set to 150cd \n",bl_level);
		cd_level = 150;
		cd_idx = 28;
	} else {
		cd_idx = get_cmd_idx_hmt(bl_level);
		cd_level = get_candela_value_hmt(bl_level);
	}

	LCD_DEBUG("[HMT] bright_level: %d, candela_idx: %d( %d cd )\n", bl_level, cd_idx, cd_level);

	/* CA */
	conf_hmt = get_smartdim_conf_hmt();

	BUG_ON(conf_hmt->generate_gamma == NULL);
	conf_hmt->generate_gamma(cd_level, &hmt_bright_cmds_list.cmd_desc[1].payload[1]);

	/* B2 */
	get_aor_set_hmt(cd_idx);

	/* B6 */
	if (msd.dstat.acl_on||msd.dstat.siop_status)
		hmt_bright_cmds_list.cmd_desc[3].payload[1] = 0x8C;
	else
		hmt_bright_cmds_list.cmd_desc[3].payload[1] = 0x9C;

	if ( bl_level < 0 || bl_level > 255 ) {
		hmt_bright_cmds_list.cmd_desc[3].payload[2] = 0x00;
	}

	return;
}

static void make_brightcontrol_set_hmt_lentis(int bl_level)
{
	int cd_idx = 0, cd_level =0;

	struct smartdim_conf_hmt *conf_hmt;

	cd_idx = get_cmd_idx_hmt(bl_level);
	cd_level = get_candela_value_hmt(bl_level);

	/* CA */
	conf_hmt = get_smartdim_conf_hmt();

	BUG_ON(conf_hmt->generate_gamma == NULL);
	conf_hmt->generate_gamma(cd_level, &hmt_bright_cmds_list.cmd_desc[1].payload[1]);

	LCD_DEBUG("[HMT] bright_level: %d, candela_idx: %d( %d cd ), ", bl_level, cd_idx, cd_level);

	return;
}

#endif

static int make_brightcontrol_set(int bl_level)
{
	struct dsi_cmd aid_control = {0,};
	struct dsi_cmd acl_control = {0,};
	struct dsi_cmd acl_on_cont = {0,};
	struct dsi_cmd acl_off_cont = {0,};
	struct dsi_cmd elvss_control = {0,};
	struct dsi_cmd gamma_control = {0,};
	struct dsi_cmd testKey = {0,};
#if defined(TEMPERATURE_ELVSS)
	struct dsi_cmd temperature_elvss_control = {0,};
	struct dsi_cmd temperature_elvss_control2 = {0,};
#endif
#if defined(SMART_VINT)
	struct dsi_cmd vint_control = {0,};
#endif
#if defined(HBM_RE)
		struct dsi_cmd hbm_off_control = {0,};
#endif
#if defined(CONFIG_LCD_HMT)
	int i;
	int hmt_cd_level;
#endif

	int cmd_count = 0, cd_idx = 0, cd_level =0;

	cd_idx = get_cmd_idx(bl_level);
	cd_level = get_candela_value(bl_level);

#if defined(CONFIG_LCD_HMT)
	if (msd.hmt_stat.hmt_on && !msd.hmt_stat.hmt_low_persistence) {
		pr_info("[HMT] [LOW PERSISTENCE OFF] - use normal aid for brightness\n");

		hmt_cd_level = get_candela_value_hmt(msd.hmt_stat.hmt_bl_level);

		for(i=0; i<candela_map_table.lux_tab_size; i++) {
			if (hmt_cd_level <= candela_map_table.lux_tab[i]) {
				pr_debug("%s: hmt_cd_level (%d), normal_cd_level (%d) idx(%d)\n",
					__func__, hmt_cd_level, candela_map_table.lux_tab[i], i);
				break;
			}
		}

		cd_idx = i;
		cd_level = candela_map_table.lux_tab[i];

		pr_info("[HMT] [LOW PERSISTENCE OFF] hmt_cd_level (%d) idx(%d) cd_level(%d)\n",
			hmt_cd_level, cd_idx, cd_level);
	}
#endif

	/* level2 enable */
	testKey = get_testKey_set(1);
	cmd_count = update_bright_packet(cmd_count, &testKey);
	/* aid/aor */

#if defined(HBM_RE)
	if (msd.panel == PANEL_WQHD_OCTA_S6E3HA2X01_CMD ||msd.panel == PANEL_WQXGA_OCTA_S6E3HA2X01_CMD){
		/*hbm off cmd*/
		if(msd.dstat.hbm_mode){
			hbm_off_control = get_hbm_off_set();/*53 00*/
			cmd_count = update_bright_packet(cmd_count, &hbm_off_control);
		}
	}
#endif
	aid_control = get_aid_aor_control_set(cd_idx);
	cmd_count = update_bright_packet(cmd_count, &aid_control);
	/* acl */
	if (msd.dstat.acl_on||msd.dstat.siop_status) {
		acl_on_cont = get_acl_control_on_set(); /*b5 51*/
		cmd_count = update_bright_packet(cmd_count, &acl_on_cont);
		acl_control = get_acl_control_set(cd_idx); /*55 02*/
		cmd_count = update_bright_packet(cmd_count, &acl_control);
	} else {
		/* acl off (hbm off) */
		acl_off_cont = get_acl_control_off_set(); /*b5 41,55 00 */
		cmd_count = update_bright_packet(cmd_count, &acl_off_cont);
	}

	/*elvss*/
	elvss_control = get_elvss_control_set(cd_idx);
	cmd_count = update_bright_packet(cmd_count, &elvss_control);

#if defined(SMART_VINT)
	vint_control = get_vint_control_set(cd_idx);
	cmd_count = update_bright_packet(cmd_count, &vint_control);
#endif

#if defined(TEMPERATURE_ELVSS)
	// ELVSS TEMPERATURE COMPENSATION
	// ELVSS for Temperature set cmd should be sent after normal elvss set cmd
	if (msd.dstat.elvss_need_update) {
		temperature_elvss_control = get_elvss_tempcompen_control_set();
		cmd_count = update_bright_packet(cmd_count, &temperature_elvss_control);

		if ((msd.panel == PANEL_FHD_OCTA_S6E3FA2_CMD)||\
			(msd.panel == PANEL_WQHD_OCTA_S6E3HA0_CMD)||\
			(msd.panel == PANEL_WQXGA_OCTA_S6E3HA2X01_CMD)||\
			(msd.panel == PANEL_WQHD_OCTA_S6E3HA2X01_CMD)){
			temperature_elvss_control2 = get_elvss_tempcompen_control_set2();
			cmd_count = update_bright_packet(cmd_count, &temperature_elvss_control2);
		}
		msd.dstat.elvss_need_update = 0;
	}
#endif

	/*gamma*/
	gamma_control = get_gamma_control_set(cd_level);
	cmd_count = update_bright_packet(cmd_count, &gamma_control);

	/* level2 disable */
	testKey = get_testKey_set(0);
	cmd_count = update_bright_packet(cmd_count, &testKey);

#if defined(TEMPERATURE_ELVSS)
	LCD_DEBUG("bright_level: %d, candela_idx: %d( %d cd ), "\
		"cmd_count(aid,acl,elvss,temperature,gamma)::(%d,%d,%d,%d,%d)%d,id3(0x%x)\n",
#else
	LCD_DEBUG("bright_level: %d, candela_idx: %d( %d cd ), "\
		"cmd_count(aid,acl,elvss,temperature,gamma)::(%d,%d,%d,%d)%d,id3(0x%x)\n",
#endif
		msd.dstat.bright_level, cd_idx, cd_level,
		aid_control.num_of_cmds,
		msd.dstat.acl_on | msd.dstat.siop_status,
		elvss_control.num_of_cmds,
#if defined(TEMPERATURE_ELVSS)
		temperature_elvss_control.num_of_cmds,
#endif
		gamma_control.num_of_cmds,
		cmd_count,
		msd.id3);
	return cmd_count;

}

#if !defined(CONFIG_FB_MSM_EDP_SAMSUNG)
static int __init current_boot_mode(char *mode)
{
	/*
	*	1,2 is recovery booting
	*	0 is normal booting
	*/

        if ((strncmp(mode, "1", 1) == 0)||(strncmp(mode, "2", 1) == 0))
		msd.dstat.recovery_boot_mode = 1;
	else
		msd.dstat.recovery_boot_mode = 0;

	pr_debug("%s %s", __func__, msd.dstat.recovery_boot_mode == 1 ?
						"recovery" : "normal");
	return 1;
}
__setup("androidboot.boot_recovery=", current_boot_mode);
#endif

static void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_panel_cmds *pcmds)
{
	struct dcs_cmd_req cmdreq;
	struct mdss_panel_info *pinfo;

	if (get_lcd_attached() == 0) {
		printk("%s: get_lcd_attached(0)!\n",__func__);
		return;
	}

	mutex_lock(&msd.lock);

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo->dcs_cmd_by_left) {
		if (ctrl->ndx != DSI_CTRL_LEFT) {
			mutex_unlock(&msd.lock);
			return;
		}
	}

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = pcmds->cmds;
	cmdreq.cmds_cnt = pcmds->cmd_cnt;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;

	/*Panel ON/Off commands should be sent in DSI Low Power Mode*/
	if (pcmds->link_state == DSI_LP_MODE)
		cmdreq.flags  |= CMD_REQ_LP_MODE;

	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);

	mutex_unlock(&msd.lock);
}

void mdss_dsi_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_cmd_desc *cmds, int cnt,int flag)
{
	struct dcs_cmd_req cmdreq;

	if (get_lcd_attached() == 0) {
		printk("%s: get_lcd_attached(0)!\n",__func__);
		return;
	}

	memset(&cmdreq, 0, sizeof(cmdreq));

	if (flag & CMD_REQ_SINGLE_TX) {
		cmdreq.flags = CMD_REQ_SINGLE_TX | CMD_CLK_CTRL | CMD_REQ_COMMIT;
	} else
		cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;

	cmdreq.cmds = cmds;
	cmdreq.cmds_cnt = cnt;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

u32 mdss_dsi_cmd_receive(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_cmd_desc *cmd, int rlen)
{
    struct dcs_cmd_req cmdreq;

	if (get_lcd_attached() == 0) {
		printk("%s: get_lcd_attached(0)!\n",__func__);
		return 0;
	}

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
	BUG_ON(msd.ctrl_pdata == NULL);
//	mutex_lock(&msd.ctrl_pdata->dfps_mutex);
    mdss_dsi_cmdlist_put(ctrl, &cmdreq);
//    mutex_unlock(&msd.ctrl_pdata->dfps_mutex);
    /*
     * blocked here, untill call back called
     */
    return ctrl->rx_buf.len;
}

static int samsung_nv_read(struct dsi_cmd_desc *desc, char *destBuffer,
		int srcLength, struct mdss_panel_data *pdata, int startoffset)
{
	int loop_limit = 0;
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

	show_buffer_pos +=
		snprintf(show_buffer, 256, "read_reg : %X[%d] : ",
		desc[0].payload[0], srcLength);

	loop_limit = (srcLength + packet_size[0] - 1)
				/ packet_size[0];
	mutex_lock(&msd.lock);
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
						show_buffer_pos, (256 - show_buffer_pos), "%02x ",
						msd.ctrl_pdata->rx_buf.data[i]);
			if (destBuffer != NULL && show_cnt < srcLength) {
					destBuffer[show_cnt] =
					msd.ctrl_pdata->rx_buf.data[i];
			}
		}
		show_buffer_pos += snprintf(show_buffer +
				show_buffer_pos, (256 - show_buffer_pos), ".");
		read_pos += read_count;

		if (read_pos-startoffset >= srcLength)
			break;
	}
	mutex_unlock(&msd.lock);

	pr_info("%s\n", show_buffer);
	return read_pos-startoffset;
}

static int mipi_samsung_read_nv_mem(struct mdss_panel_data *pdata, struct dsi_cmd *nv_read_cmds, char *buffer)
{
	int nv_size = 0;
	int nv_read_cnt = 0;
	int i = 0;
	mipi_samsung_disp_send_cmd(PANEL_MTP_ENABLE, true);

	for (i = 0; i < nv_read_cmds->num_of_cmds; i++)
		nv_size += nv_read_cmds->read_size[i];

	pr_debug("nv_size= %d, nv_read_cmds->num_of_cmds = %d\n", nv_size, nv_read_cmds->num_of_cmds);

	for (i = 0; i < nv_read_cmds->num_of_cmds; i++) {
		int count = 0;
		int read_size = nv_read_cmds->read_size[i];
		int read_startoffset = nv_read_cmds->read_startoffset[i];

		count = samsung_nv_read(&(nv_read_cmds->cmd_desc[i]),
				&buffer[nv_read_cnt], read_size, pdata, read_startoffset);
		nv_read_cnt += count;
		if (count != read_size)
			pr_err("Error reading LCD NV data count(%d), read_size(%d)!!!!\n",count,read_size);
	}

	mipi_samsung_disp_send_cmd(PANEL_MTP_DISABLE, true);

	return nv_read_cnt;
}
//#endif
#ifdef DEBUG_LDI_STATUS
int read_ldi_status(void)
{
	struct dsi_buf *rp, *tp;
	int i;

	if (!ldi_debug_cmds.num_of_cmds)
		return 1;

	if(!msd.dstat.on) {
		pr_err("%s can not read because of panel off \n", __func__);
		return 1;
	}

	tp = &dsi_panel_tx_buf;
	rp = &dsi_panel_rx_buf;

	mdss_dsi_cmd_receive(msd.ctrl_pdata,
		&ldi_debug_cmds.cmd_desc[0],
		ldi_debug_cmds.read_size[0]);

	pr_info("%s: LDI 0Ah Register Value = 0x%x (Normal Case:0x9C)\n", __func__, *msd.ctrl_pdata->rx_buf.data);

	mdss_dsi_cmd_receive(msd.ctrl_pdata,
		&ldi_debug_cmds.cmd_desc[1],
		ldi_debug_cmds.read_size[1]);

	pr_info("%s: LDI 0Eh Register Value  = 0x%x (Normal Case:0x80)\n", __func__, *msd.ctrl_pdata->rx_buf.data);

	mdss_dsi_cmd_receive(msd.ctrl_pdata,
		&ldi_debug_cmds.cmd_desc[2],
		ldi_debug_cmds.read_size[2]);
	for(i=0 ; i<8 ; i++) {
		pr_info("%s: LDI EAh Register Value[%d]  = 0x%x \n", __func__,i, msd.ctrl_pdata->rx_buf.data[i]);
	}


	return 0;

}
EXPORT_SYMBOL(read_ldi_status);
#endif

static void mipi_samsung_manufacture_date_read(struct mdss_panel_data *pdata)
{
	char date[4];
	int year, month, day;
	int hour, min;
	int manufacture_date, manufacture_time;

	/* Read mtp (C8h 41,42,43,44th) for manufacture date */
	mipi_samsung_read_nv_mem(pdata, &manufacture_date_cmds, date);

	year = date[0] & 0xf0;
	year >>= 4;
	year += 2011; // 0 = 2011 year
	month = date[0] & 0x0f;
	day = date[1] & 0x1f;
	hour = date[2]& 0x0f;
	min = date[3] & 0x1f;

	manufacture_date = year * 10000 + month * 100 + day;
	manufacture_time =  hour * 100 + min;

	pr_info("manufacture_date = (%d%04d) - year(%d) month(%d) day(%d) hour(%d) min(%d)\n",
		manufacture_date, manufacture_time, year, month, day, hour, min);

	msd.manufacture_date = manufacture_date;
	msd.manufacture_time = manufacture_time;
}

static void mipi_samsung_ddi_id_read(struct mdss_panel_data *pdata)
{
	char ddi_id[5];

	/* Read mtp (D6h 1~5th) for ddi id */
	mipi_samsung_read_nv_mem(pdata, &ddi_id_cmds, ddi_id);

	memcpy(msd.ddi_id, ddi_id, 5);

	pr_info("%s : %02x %02x %02x %02x %02x\n", __func__,
		msd.ddi_id[0], msd.ddi_id[1], msd.ddi_id[2], msd.ddi_id[3], msd.ddi_id[4]);
}

static unsigned int mipi_samsung_manufacture_id(struct mdss_panel_data *pdata)
{
	struct dsi_buf *rp, *tp;

	unsigned int id = 0 ;


#if defined(CAMERA_LP)
	return 0x501401;
#endif


	if (get_lcd_attached() == 0)
	{
		printk("%s: get_lcd_attached(0)!\n",__func__);
		return id;
	}

	if (!manufacture_id_cmds.num_of_cmds) {
		pr_err("%s : manufacture id cmds num is zero..\n",__func__);
		return 0;
	}

	tp = &dsi_panel_tx_buf;
	rp = &dsi_panel_rx_buf;

	mdss_dsi_cmd_receive(msd.ctrl_pdata,
		&manufacture_id_cmds.cmd_desc[0],
		manufacture_id_cmds.read_size[0]);

	pr_debug("%s: manufacture_id1=%x\n", __func__, *msd.ctrl_pdata->rx_buf.data);

	id = (*((unsigned int *)msd.ctrl_pdata->rx_buf.data) & 0xFF);
	id <<= 8;

	mdss_dsi_cmd_receive(msd.ctrl_pdata,
		&manufacture_id_cmds.cmd_desc[1],
		manufacture_id_cmds.read_size[1]);
	pr_debug("%s: manufacture_id2=%x\n", __func__, *msd.ctrl_pdata->rx_buf.data);
	id |= (*((unsigned int *)msd.ctrl_pdata->rx_buf.data) & 0xFF);
	id <<= 8;

	mdss_dsi_cmd_receive(msd.ctrl_pdata,
		&manufacture_id_cmds.cmd_desc[2],
		manufacture_id_cmds.read_size[2]);
	pr_debug("%s: manufacture_id3=%x\n", __func__, *msd.ctrl_pdata->rx_buf.data);
	id |= (*((unsigned int *)msd.ctrl_pdata->rx_buf.data) & 0xFF);

	pr_info("%s: manufacture_id=%x\n", __func__, id);

	return id;
}

static void mdss_dsi_panel_bl_ctrl(struct mdss_panel_data *pdata,
							u32 bl_level)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	/*Dont need to send backlight command if display off*/
	if (msd.mfd->resume_state != MIPI_RESUME_STATE)
		return;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	if (!ctrl_pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	switch (ctrl_pdata->bklt_ctrl) {
		case BL_WLED:
			led_trigger_event(bl_led_trigger, bl_level);
			break;
		case BL_PWM:
			mdss_dsi_panel_bklt_pwm(ctrl_pdata, bl_level);
			break;
		case BL_DCS_CMD:
			msd.dstat.bright_level = bl_level;
			mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);
			break;
		default:
			pr_err("%s: Unknown bl_ctrl configuration\n",
				__func__);
			break;
	}
}

static int mipi_samsung_disp_send_cmd(
		enum mipi_samsung_cmd_list cmd,
		unsigned char lock)
{
	struct dsi_cmd_desc *cmd_desc;
	int cmd_size = 0;
	int flag = 0;
#ifdef CMD_DEBUG
	int i,j;
#endif

	if (get_lcd_attached() == 0) {
		printk("%s: get_lcd_attached(0)!\n",__func__);
		return -ENODEV;
	}

	if (lock)
		mutex_lock(&msd.lock);

	switch (cmd) {
		case PANEL_DISPLAY_ON:
			cmd_desc = display_on_cmd.cmd_desc;
			cmd_size = display_on_cmd.num_of_cmds;
			break;
		case PANEL_DISPLAY_OFF:
			cmd_desc = display_off_cmd.cmd_desc;
			cmd_size = display_off_cmd.num_of_cmds;
			break;
		case PANEL_HSYNC_ON:
			cmd_desc = hsync_on_seq.cmd_desc;
			cmd_size = hsync_on_seq.num_of_cmds;
			break;
#if defined(CONFIG_WACOM_LCD_FREQ_COMPENSATE)
		case PANEL_LDI_FPS_CHANGE:
			cmd_desc = write_ldi_fps_cmds.cmd_desc;
			cmd_size = write_ldi_fps_cmds.num_of_cmds;
			break;
#endif
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FHD_FA2_PT_PANEL)
		case PANEL_SET_TE_OSC_B:
			cmd_desc = panel_set_te_osc_b.cmd_desc;
			cmd_size = panel_set_te_osc_b.num_of_cmds;
			break;
		case PANEL_SET_TE_RESTORE:
			cmd_desc = panel_set_te_restore.cmd_desc;
			cmd_size = panel_set_te_restore.num_of_cmds;
			break;
		case PANEL_SET_TE:
			cmd_desc = panel_set_te.cmd_desc;
			cmd_size = panel_set_te.num_of_cmds;
			break;
		case PANEL_SET_TE_1:
			cmd_desc = panel_set_te_1.cmd_desc;
			cmd_size = panel_set_te_1.num_of_cmds;
			break;
		case PANEL_SET_TE_2:
			cmd_desc = panel_set_te_2.cmd_desc;
			cmd_size = panel_set_te_2.num_of_cmds;
			break;
#endif
		case PANEL_BRIGHT_CTRL:
#if defined(CONFIG_SEC_TRLTE_PROJECT)
		if (system_rev < 2)
			goto err;
#endif

#if defined(CAMERA_LP)
			goto err;
#endif
#if defined(CONFIG_LCD_HMT)
			if (msd.hmt_stat.hmt_on) {
				pr_err("hmt is on!! do not set brightness..\n");
				goto err;
			}
#endif

			cmd_desc = brightness_packet;

#if defined(CONFIG_LCD_FORCE_VIDEO_MODE)
			flag = 0;
#else
			/* Single Tx use for DSI_VIDEO_MODE Only */
			if(msd.pdata->panel_info.mipi.mode == DSI_VIDEO_MODE)
				flag = CMD_REQ_SINGLE_TX;
			else
				flag = 0;
#endif
			msd.dstat.recent_bright_level = msd.dstat.bright_level;
#if defined(HBM_RE)
			if(msd.dstat.auto_brightness == 6) {
				cmd_size = make_brightcontrol_hbm_set();
				msd.dstat.hbm_mode = 1;
			} else {
				cmd_size = make_brightcontrol_set(msd.dstat.bright_level);
				msd.dstat.hbm_mode = 0;
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
#ifdef CONFIG_FB_MSM_SAMSUNG_AMOLED_LOW_POWER_MODE
		case PANEL_ALPM_ON:
			cmd_desc = alpm_on_seq.cmd_desc;
			cmd_size = alpm_on_seq.num_of_cmds;
			break;
		case PANEL_ALPM_OFF:
			cmd_desc = alpm_off_seq.cmd_desc;
			cmd_size = alpm_off_seq.num_of_cmds;
			break;
#endif
#if defined(CONFIG_LCD_CLASS_DEVICE) && defined(DDI_VIDEO_ENHANCE_TUNING)
		case MDNIE_ADB_TEST:
			cmd_desc = mdni_tune_cmd;
			cmd_size = ARRAY_SIZE(mdni_tune_cmd);
			break;
#endif
#if defined(CONFIG_LCD_HMT)
		case PANEL_HMT_BRIGHT:
			if (msd.panel == PANEL_WQHD_OCTA_S6E3HA0_CMD)
				make_brightcontrol_set_hmt_lentis(msd.hmt_stat.hmt_bl_level);
			else
				make_brightcontrol_set_hmt_tlte(msd.hmt_stat.hmt_bl_level);
			cmd_desc = hmt_bright_cmds_list.cmd_desc;
			cmd_size = hmt_bright_cmds_list.num_of_cmds;
			break;
		case PANEL_LOW_PERSISTENCE_BRIGHT:
			cmd_desc = brightness_packet;

			/* Single Tx use for DSI_VIDEO_MODE Only */
			if(msd.pdata->panel_info.mipi.mode == DSI_VIDEO_MODE)
				flag = CMD_REQ_SINGLE_TX;
			else
				flag = 0;

			cmd_size = make_brightcontrol_set(msd.dstat.bright_level);

			if (msd.mfd->resume_state != MIPI_RESUME_STATE) {
				pr_info("%s : panel is off state!!\n", __func__);
				goto unknown_command;
			}
			break;
		case PANEL_ENABLE:
			cmd_desc = hmt_single_scan_enable.cmd_desc;
			cmd_size = hmt_single_scan_enable.num_of_cmds;
			break;
		case PANEL_DISABLE:
			cmd_desc = hmt_disable.cmd_desc;
			cmd_size = hmt_disable.num_of_cmds;
			break;
		case PANEL_HMT_REVERSE_ENABLE:
			cmd_desc = hmt_reverse_enable.cmd_desc;
			cmd_size = hmt_reverse_enable.num_of_cmds;
			break;
		case PANEL_HMT_REVERSE_DISABLE:
			cmd_desc = hmt_reverse_disable.cmd_desc;
			cmd_size = hmt_reverse_disable.num_of_cmds;
			break;
		case PANEL_HMT_KEY_ENABLE:
			cmd_desc = test_key_enable_cmds.cmd_desc;
			cmd_size = test_key_enable_cmds.num_of_cmds;
			break;
#endif
#ifdef LDI_ADJ_VDDM_OFFSET
		case PANEL_LDI_SET_VDD_OFFSET:
			cmd_desc = write_vdd_offset_cmds.cmd_desc;
			cmd_size = write_vdd_offset_cmds.num_of_cmds;
			break;
		case PANEL_LDI_SET_VDDM_OFFSET:
			cmd_desc = write_vddm_offset_cmds.cmd_desc;
			cmd_size = write_vddm_offset_cmds.num_of_cmds;
		break;
#endif
		default:
			pr_err("%s : unknown_command.. \n", __func__);
			goto unknown_command;
			break;
	}

	if (!cmd_size) {
		pr_err("%s : cmd_size is zero!.. \n", __func__);
		goto err;
	}

#ifdef CMD_DEBUG
	for (i = 0; i < cmd_size; i++) {
		for (j = 0; j < cmd_desc[i].dchdr.dlen; j++)
			printk("%x ",cmd_desc[i].payload[j]);
		printk("\n");
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

	pr_debug("%s done..\n", __func__);

	return 0;

unknown_command:
	LCD_DEBUG("Undefined command\n");

err:
	if (lock)
		mutex_unlock(&msd.lock);

	return -EINVAL;
}

void mdss_dsi_panel_touchsensing(int enable)
{
	if(!msd.dstat.on)
	{
		pr_err("%s: No panel on! %d\n", __func__, enable);
		return;
	}

	if(enable)
		mipi_samsung_disp_send_cmd(PANEL_TOUCHSENSING_ON, true);
	else
		mipi_samsung_disp_send_cmd(PANEL_TOUCHSENSING_OFF, true);
}

static void mdss_dsi_panel_read_func(struct mdss_panel_data *pdata)
{

#if defined(CONFIG_MDNIE_LITE_TUNING)
	char temp[4];
	int	x, y;
#endif
#if defined(HBM_RE)
	char hbm_buffer[20];
#endif
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FHD_FA2_PT_PANEL)
	char read_buffer[1];
#endif

	char elvss_buffer[2];

	pr_info("%s : ++\n",__func__);

#if defined(CAMERA_LP)
	return;
#endif

	if (get_lcd_attached() == 0) {
		pr_err("%s: get_lcd_attached(0)!\n",__func__);
		return;
	}

	mipi_samsung_manufacture_date_read(pdata);
	mipi_samsung_ddi_id_read(pdata);

#if defined(HBM_RE)
	if (msd.panel == PANEL_WQHD_OCTA_S6E3HA0_CMD ||\
		msd.panel == PANEL_FHD_OCTA_S6E3FA2_CMD){
		/* Read mtp (C8h 34th ~ 40th) for HBM */
		mipi_samsung_read_nv_mem(pdata, &nv_mtp_hbm_read_cmds, hbm_buffer);
		memcpy(&hbm_gamma_cmds_list.cmd_desc[0].payload[1], hbm_buffer, 7);

			/* octa panel Read C8h 40th -> write B6h 21th */
		if (hbm_etc_cmds_list.cmd_desc)
			memcpy(&hbm_etc_cmds_list.cmd_desc[1].payload[21], hbm_buffer+6, 1);

		/* Read mtp (C8h 73th ~ 87th) for HBM */
		mipi_samsung_read_nv_mem(pdata, &nv_mtp_hbm2_read_cmds, hbm_buffer);
		memcpy(&hbm_gamma_cmds_list.cmd_desc[0].payload[7], hbm_buffer, 15);
	}
#endif

	/* Read mtp (B6h 21th) for elvss*/
	mipi_samsung_read_nv_mem(pdata, &nv_mtp_elvss_read_cmds, elvss_buffer);
	msd.dstat.elvss_value = elvss_buffer[0];

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FHD_FA2_PT_PANEL)
	mipi_samsung_read_nv_mem(pdata, &panel_osc_type_read_cmds, read_buffer);
	panel_set_te_restore.cmd_desc[2].payload[1] = read_buffer[0];
#endif

#if defined(CONFIG_MDNIE_LITE_TUNING)
	/* MDNIe tuning initialisation*/
	if (!msd.dstat.is_mdnie_loaded) {
		mipi_samsung_read_nv_mem(pdata, &nv_mdnie_read_cmds, temp);
		x =  temp[0] << 8 | temp[1];	/* X */
		y = temp[2] << 8 | temp[3];	/* Y */
		coordinate_tunning(x, y);
		msd.dstat.is_mdnie_loaded = true;
	}
#endif

	msd.dstat.is_panel_read_done = true;

	pr_info("%s : --\n",__func__);

	return;
}

static int mdss_dsi_panel_dimming_init(struct mdss_panel_data *pdata)
{
#ifdef LDI_ADJ_VDDM_OFFSET
	unsigned int vdd_offset, vddm_offset;
	char vol_ref_buffer, vol_ref_buffer2;
#endif
	pr_info("%s : ++\n",__func__);

#if  defined(CAMERA_LP)
	return 0;
#endif

#if defined(CONFIG_SEC_TRLTE_PROJECT) //TEMP for T
	if (system_rev < 2)
		return 0;
#endif

	switch (msd.panel) {
		case PANEL_FHD_OCTA_S6E3FA0:
		case PANEL_FHD_OCTA_S6E3FA0_CMD:
		case PANEL_FHD_OCTA_S6E3FA2_CMD:
		case PANEL_WQHD_OCTA_S6E3HA0_CMD:
		case PANEL_WQHD_OCTA_S6E3HA2X01_CMD:
		case PANEL_WQXGA_OCTA_S6E3HA2X01_CMD:
			msd.sdimconf = smart_S6E3_get_conf();
			break;
	}

	/* Just a safety check to ensure smart dimming data is initialised well */
	BUG_ON(msd.sdimconf == NULL);

	/* Set the mtp read buffer pointer and read the NVM value*/
	mipi_samsung_read_nv_mem(pdata, &nv_mtp_read_cmds, msd.sdimconf->mtp_buffer);

#ifdef LDI_ADJ_VDDM_OFFSET
	mipi_samsung_read_nv_mem(msd.pdata, &read_vdd_ref_cmds, &vol_ref_buffer);
	vdd_offset=(unsigned int)(vol_ref_buffer & 0x7F);
	pr_info("%s:vddm_offset = %d , ldi_vdd_lut[%d][1] = %d \n", __func__, vdd_offset, vdd_offset, ldi_vddm_lut[vdd_offset][1]);
	write_vdd_offset_cmds.cmd_desc[3].payload[1] = ldi_vddm_lut[vdd_offset][1];

	mipi_samsung_read_nv_mem(msd.pdata, &read_vddm_ref_cmds, &vol_ref_buffer2);
	vddm_offset=(unsigned int)(vol_ref_buffer2 & 0x7F);
	pr_info("%s:vddm_offset = %d , ldi_vddm_lut[%d][1] = %d \n", __func__, vddm_offset, vddm_offset, ldi_vddm_lut[vddm_offset][1]);
	write_vddm_offset_cmds.cmd_desc[2].payload[1] = ldi_vddm_lut[vddm_offset][1];
#endif

	/* Initialize smart dimming related	things here */
	/* lux_tab setting for 350cd */
	msd.sdimconf->lux_tab = &candela_map_table.lux_tab[0];
	msd.sdimconf->lux_tabsize = candela_map_table.lux_tab_size;
	msd.sdimconf->man_id = msd.manufacture_id;

	/* Just a safety check to ensure smart dimming data is initialised well */
	BUG_ON(msd.sdimconf->init == NULL);
	msd.sdimconf->init();

	msd.dstat.temperature = 20; // default temperature
	msd.dstat.elvss_need_update = 1;

	msd.dstat.is_smart_dim_loaded = true;

	pr_info("%s : --\n",__func__);

	return 0;
}

#if defined(CONFIG_LCD_HMT)
static void mdss_dsi_panel_make_sdimconf(struct mdss_panel_data *pdata, struct smartdim_conf_hmt *pSdimconf, struct candella_lux_map *pCandela_map_table) {
	int size;
	char mtp_buffer_for_150cd[30];

	/* Just a safety check to ensure smart dimming data is initialised well */
	BUG_ON(pSdimconf == NULL);

	/* Set the mtp read buffer pointer and read the NVM value*/
	size = mipi_samsung_read_nv_mem(pdata, &nv_mtp_read_cmds, pSdimconf->mtp_buffer);

	/* Read mtp (B4h 2nd ~ 31th) for HMT 150cd */
	mipi_samsung_read_nv_mem(pdata, &hmt_150cd_read_cmds, mtp_buffer_for_150cd);

	/* Initialize smart dimming related things here */
	/* lux_tab setting for 350cd */
	pSdimconf->lux_tab = &(pCandela_map_table->lux_tab[0]);
	pSdimconf->lux_tabsize = pCandela_map_table->lux_tab_size;
	pSdimconf->man_id = msd.manufacture_id;

	if (pSdimconf->set_para_for_150cd)
		pSdimconf->set_para_for_150cd(mtp_buffer_for_150cd, 30);
	/* Just a safety check to ensure smart dimming data is initialised well */

	BUG_ON(pSdimconf->init == NULL);
	pSdimconf->init();

	pr_info("[HMT] smart dimming done!\n");
}

static int mdss_dsi_panel_dimming_init_HMT(struct mdss_panel_data *pdata)
{
	pr_info("[HMT] %s : ++\n",__func__);

	msd.hmt_stat.hmt_bl_level = 0;
	msd.hmt_stat.hmt_on = 0;
	msd.hmt_stat.hmt_reverse = 0;
	msd.hmt_stat.hmt_low_persistence = 1;

	switch (msd.panel) {
		case PANEL_WQHD_OCTA_S6E3HA2X01_CMD:
		case PANEL_WQXGA_OCTA_S6E3HA2X01_CMD:
		case PANEL_WQHD_OCTA_S6E3HA0_CMD:
			msd.sdimconf_reverse_hmt_single = smart_S6E3_get_conf_hmt();
			break;
	}

	mdss_dsi_panel_make_sdimconf(pdata, msd.sdimconf_reverse_hmt_single, &candela_map_table_reverse_hmt);

	msd.dstat.is_hmt_smart_dim_loaded = true;

	pr_debug("[HMT] %s : --\n",__func__);

	return 0;
}
#endif

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

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FHD_FA2_PT_PANEL)
	te_set_done = TE_SET_INIT;
#endif

	if(!msd.mfd)
	{
		pr_info("%s mds.mfd is null!!\n",__func__);
	} else
		pr_info("%s mds.mfd is ok!!\n",__func__);
#if defined(CONFIG_MDNIE_LITE_TUNING)
	pr_info("[%s] CONFIG_MDNIE_LITE_TUNING ok ! mdnie_lite_tuning_init called!\n",
		__func__);
	mdnie_lite_tuning_init(&msd);
#endif
	/* Set the initial state to Suspend until it is switched on */
	msd.mfd->resume_state = MIPI_SUSPEND_STATE;
	pr_info("%s:%d, Panel registered succesfully\n", __func__, __LINE__);
	return 0;
}

#if defined(CONFIG_DUAL_LCD)
struct mdss_panel_data *mdss_dsi_switching = NULL;
int IsSwitching = 0;
extern int dsi_clk_on;
#endif

static int mdss_dsi_panel_on(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	static struct mdss_panel_alpm_data *alpm_data = NULL;
	struct mdss_panel_info *pinfo;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	if (unlikely(!alpm_data))
		alpm_data = &pdata->alpm_data;

	pr_info("%s : ++\n", __func__);

	pinfo = &pdata->panel_info;
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	msd.ctrl_pdata = ctrl;

	pr_debug("mdss_dsi_panel_on DSI_MODE = %d ++\n",msd.pdata->panel_info.mipi.mode);
	pr_info("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	if (mdss_dsi_sync_wait_enable(ctrl)) {
		if (ctrl->ndx == DSI_CTRL_0) {
#if defined(CONFIG_LCD_RECOVERY)
			pdata_dsi0 = &ctrl->panel_data;
#endif
			pr_info("%s: Broadcast mode. 1st ctrl(0). return..\n",__func__);
			goto end;
		}
#if defined(CONFIG_LCD_RECOVERY)
		else
			pdata_dsi1 = &ctrl->panel_data;
#endif
	}
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQHD_PT_PANEL) || defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQXGA_PT_PANEL) //TEMP for T
	if (system_rev >= 2)
		mipi_samsung_disp_send_cmd(PANEL_MTP_ENABLE, true);
#endif
	if (!msd.manufacture_id) {
		msd.manufacture_id = mipi_samsung_manufacture_id(pdata);
		if (set_panel_rev(msd.manufacture_id) < 0)
			pr_err("%s : can't find panel id.. \n", __func__);
	}

	if (!msd.dstat.is_panel_read_done){
		mdss_dsi_panel_read_func(pdata);
	}

	if (!msd.dstat.is_smart_dim_loaded)
		mdss_dsi_panel_dimming_init(pdata);

#if defined(CONFIG_LCD_HMT)
	if (!msd.dstat.is_hmt_smart_dim_loaded)
		mdss_dsi_panel_dimming_init_HMT(pdata);
#endif
	/*
	 * Normaly the else is working for PANEL_DISP_ON_SEQ
	 * if the alpm was not enabled
	 */
	if (alpm_data->alpm_status) {
		if (!alpm_data->alpm_status(CHECK_PREVIOUS_STATUS))
			mdss_dsi_panel_cmds_send(msd.ctrl_pdata , &msd.ctrl_pdata->on_cmds);

	} else {
		mdss_dsi_panel_cmds_send(msd.ctrl_pdata , &msd.ctrl_pdata->on_cmds);

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FHD_FA2_PT_PANEL)
		if (te_set_done == TE_SET_DONE) {
			mipi_samsung_disp_send_cmd(PANEL_SET_TE, true);
		} else
			msleep(120);
#endif
	}

	if (msd.panel == PANEL_FHD_OCTA_S6E3FA2_CMD)
		mipi_samsung_disp_send_cmd(PANEL_HSYNC_ON, true);

	if ((msd.panel == PANEL_WQHD_OCTA_S6E3HA2X01_CMD || msd.panel == PANEL_WQXGA_OCTA_S6E3HA2X01_CMD) && (msd.id2 == 0x20 ||msd.id2 == 0x40 ))
		mipi_samsung_disp_send_cmd(PANEL_HSYNC_ON, true);

	/* Recovery Mode : Set some default brightness */
	if (msd.dstat.recovery_boot_mode) {
		msd.dstat.bright_level = RECOVERY_BRIGHTNESS;
		mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);
	}

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQHD_PT_PANEL) || defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQXGA_PT_PANEL) //TEMP for T
	if (system_rev >= 2)
		mipi_samsung_disp_send_cmd(PANEL_MTP_DISABLE, true);
#endif

	/* Init Index Values */
	msd.dstat.curr_elvss_idx = -1;
#if defined(SMART_VINT)
	msd.dstat.curr_vint_idx = -1;
#endif
	msd.dstat.curr_acl_idx = -1;
	msd.dstat.curr_opr_idx = -1;
	msd.dstat.curr_aid_idx = -1;
	msd.dstat.hbm_mode = 0;
	msd.dstat.on = 1;
	msd.dstat.wait_disp_on = 1;

	/*default acl off(caps on :b5 41) in on seq. */
	msd.dstat.curr_acl_idx = 0;
	msd.dstat.curr_acl_cond = 0;

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FHD_FA2_PT_PANEL)
	if (te_set_done == TE_SET_INIT)
		te_set_done = TE_SET_READY;
#endif

	msd.mfd->resume_state = MIPI_RESUME_STATE;
#ifdef LDI_ADJ_VDDM_OFFSET
	mipi_samsung_disp_send_cmd(PANEL_LDI_SET_VDD_OFFSET, true);
	mipi_samsung_disp_send_cmd(PANEL_LDI_SET_VDDM_OFFSET, true);
#endif

	/* ALPM Mode Change */
	if (alpm_data->alpm_status) {
		if (!alpm_data->alpm_status(CHECK_PREVIOUS_STATUS)\
				&& alpm_data->alpm_status(CHECK_CURRENT_STATUS)) {
			/* Turn On ALPM Mode */
			mipi_samsung_disp_send_cmd(PANEL_ALPM_ON, true);
			alpm_data->alpm_status(STORE_CURRENT_STATUS);
			pr_info("[ALPM_DEBUG] %s: Send ALPM mode on cmds\n", __func__);
		} else if (!alpm_data->alpm_status(CHECK_CURRENT_STATUS)\
					&& alpm_data->alpm_status(CHECK_PREVIOUS_STATUS)) {
			/* Turn Off ALPM Mode */
			mipi_samsung_disp_send_cmd(PANEL_ALPM_OFF, true);
			alpm_data->alpm_status(CLEAR_MODE_STATUS);
			pr_info("[ALPM_DEBUG] %s: Send ALPM off cmds\n", __func__);
		}
	}
#if defined(CONFIG_MDNIE_LITE_TUNING)

	is_negative_on();


#if defined(PARTIAL_UPDATE)
	if (partial_disp_range[0] || partial_disp_range[1])
		mipi_samsung_disp_send_cmd(PANEL_PARTIAL_ON, true);
#endif

	// to prevent splash during wakeup
	msd.dstat.bright_level = msd.dstat.recent_bright_level;
	mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);
#endif

#if defined(CONFIG_LCD_RECOVERY)
	if(esd_enable){
		if (lcd_refresh_working){
			lcd_refresh_working = 0;
			lcd_refresh_count++;
		}

		enable_irq(gpio_to_irq(oled_det_gpio));
#if defined(CONFIG_LCD_RECOVERY_ERR_FG)
		enable_irq(gpio_to_irq(err_fg_gpio));
#endif
		pr_info("%s: ESD enable irq lcd_refresh_count(%d)\n", __func__,lcd_refresh_count);
	}
#endif

#if defined(CONFIG_WACOM_LCD_FREQ_COMPENSATE)
	/*LUT offset initalization*/
	write_ldi_fps_cmds.cmd_desc[1].payload[3] = 0xC5;
#endif

end:
	pinfo->blank_state = MDSS_PANEL_BLANK_UNBLANK;
	pr_info("%s : --\n", __func__);
	return 0;
}

static int mdss_dsi_panel_off(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	static struct mdss_panel_alpm_data *alpm_data = NULL;
	struct mdss_panel_info *pinfo;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	if (unlikely(!alpm_data))
		alpm_data = &pdata->alpm_data;

	pr_info("%s : ++\n",__func__);

	pinfo = &pdata->panel_info;
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	msd.ctrl_pdata = ctrl;


#if defined(CONFIG_LCD_RECOVERY)
	if (ctrl->ndx == DSI_CTRL_0) {
		if (esd_enable && !lcd_refresh_working && msd.dstat.on) {
			disable_irq_nosync(gpio_to_irq(oled_det_gpio));
#if defined(CONFIG_LCD_RECOVERY_ERR_FG)
			disable_irq_nosync(gpio_to_irq(err_fg_gpio));
#endif
			cancel_work_sync(&lcd_refresh_work);

			pr_info("%s: ESD disable irq\n", __func__);
		}
	}
#endif

	msd.dstat.on = 0;
	msd.mfd->resume_state = MIPI_SUSPEND_STATE;

	pr_info("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	if (mdss_dsi_sync_wait_enable(ctrl)) {
		if (ctrl->ndx == DSI_CTRL_0) {
			pr_info("%s: Broadcast mode. 1st ctrl(0). return..\n",__func__);
			goto end;
		}
	}

	if (alpm_data->alpm_status && alpm_data->alpm_status(CHECK_CURRENT_STATUS))
		pr_info("[ALPM_DEBUG] %s: Skip to send panel off cmds\n", __func__);
	else
		mdss_dsi_panel_cmds_send(msd.ctrl_pdata, &msd.ctrl_pdata->off_cmds);

	pr_info("DISPLAY_OFF\n");
end:
	pinfo->blank_state = MDSS_PANEL_BLANK_BLANK;
	pr_info("%s : --\n",__func__);
	return 0;
}

static int mdss_dsi_panel_low_power_config(struct mdss_panel_data *pdata,
	int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_info *pinfo;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	pinfo = &pdata->panel_info;
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s: ctrl=%p ndx=%d enable=%d\n", __func__, ctrl, ctrl->ndx,
		enable);

	/* Any panel specific low power commands/config */
	if (enable)
		pinfo->blank_state = MDSS_PANEL_BLANK_LOW_POWER;
	else
		pinfo->blank_state = MDSS_PANEL_BLANK_UNBLANK;

	pr_debug("%s:-\n", __func__);
	return 0;
}

#if defined(CONFIG_DUAL_LCD)
int samsung_switching_lcd(int flip)
{
	int ret = 0;

	msd.dstat.lcd_sel=!flip; //Change LCD SEL

	if(mdss_dsi_switching == NULL)
		return 0;

	if (get_lcd_attached() == 0)
	{
		pr_err("%s: get_lcd_attached(0)!\n",__func__);
		return -ENODEV;
	}

	LCD_DEBUG("msd.dstat.on=%d, lcd_sel=%d +\n", msd.dstat.on, msd.dstat.lcd_sel);

	if(!msd.dstat.on && dsi_clk_on) {
		int retry=5;
		while(retry>0 && !msd.dstat.on)  {
			msleep(100);
			retry--;
		}
	}

	if(msd.dstat.on && dsi_clk_on) {
		IsSwitching = 1;

		ret = mdss_dsi_panel_off(msd.pdata);
		if(ret)
			pr_err("%s: mdss_dsi_panel_off error\n",__func__);

		msd.dstat.bright_level = msd.mfd->bl_level;

		/* Init Index Values */
		msd.dstat.curr_elvss_idx = -1;
		msd.dstat.curr_acl_idx = -1;
		msd.dstat.curr_aid_idx = -1;
		msd.dstat.hbm_mode = 0;

		ret = mdss_dsi_panel_on(msd.pdata);
		if(ret)
			pr_err("%s: mdss_dsi_panel_on error\n",__func__);

		IsSwitching = 0;
	}

	LCD_DEBUG(" -\n");

	return ret;
}
EXPORT_SYMBOL(samsung_switching_lcd);
#endif
static int mdss_samsung_parse_candella_lux_mapping_table(struct device_node *np,
		struct candella_lux_map *table, char *keystring)
{
		const __be32 *data;
		int  data_offset, len = 0 , i = 0;
		int  cdmap_start=0, cdmap_end=0;

		data = of_get_property(np, keystring, &len);
		if (!data) {
			pr_err("%s:%d, Unable to read table %s \n",
				__func__, __LINE__, keystring);
			return -EINVAL;
		}
		if ((len % 4) != 0) {
			pr_err("%s:%d, Incorrect table entries for %s \n",
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
			pr_err("%s:%d, Unable to read table %s \n",
				__func__, __LINE__, keystring);
			return -EINVAL;
		}
		if ((len % 2) != 0) {
			pr_err("%s:%d, Incorrect table entries for %s \n",
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
			pr_err("%s:%d, Unable to read %s \n",
				__func__, __LINE__, keystring);
			return -ENOMEM;
		}

		commands->cmds_buff = kzalloc(sizeof(char) * len, GFP_KERNEL);
		if (!commands->cmds_buff)
			return -ENOMEM;

		memcpy(commands->cmds_buff, data, len);
		commands->cmds_len = len;

		commands->num_of_cmds = 0;

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
static void mdss_panel_parse_te_params(struct device_node *np,
				       struct mdss_panel_info *panel_info)
{

	u32 tmp;
	int rc = 0;

	pr_info("%s ++ \n", __func__);
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

	data = of_get_property(np, link_key, NULL);
	if (data && !strcmp(data, "dsi_hs_mode"))
		pcmds->link_state = DSI_HS_MODE;
	else
		pcmds->link_state = DSI_LP_MODE;

	pr_info("%s: dcs_cmd=%x len=%d, cmd_cnt=%d link_state=%d\n", __func__,
		pcmds->buf[0], pcmds->blen, pcmds->cmd_cnt, pcmds->link_state);

	return 0;

exit_free:
	kfree(buf);
	return -ENOMEM;
}

static int mdss_panel_parse_dt(struct device_node *np,
					struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	u32 res[6], tmp;
	u32 fbc_res[7];
	int rc, i, len;
	const char *data;
	static const char *bl_ctrl_type, *pdest;
	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);
	bool fbc_enabled = false;

	rc = of_property_read_u32_array(np, "qcom,mdss-pan-res", res, 2);
	if (rc) {
		pr_err("%s:%d, panel resolution not specified\n",
						__func__, __LINE__);
		return -EINVAL;
		}
		pinfo->xres = (!rc ? res[0] : 640);
		pinfo->yres = (!rc ? res[1] : 480);

	rc = of_property_read_u32_array(np, "qcom,mdss-pan-size", res, 2);
	if (rc == 0) {
		pinfo->physical_width = (!rc ? res[0] : -1);
		pinfo->physical_height = (!rc ? res[1] : -1);
	}

	pr_debug("Panel Physical Width=%d, Height=%d\n",
		pinfo->physical_width,
		pinfo->physical_height);

	rc = of_property_read_u32_array(np, "qcom,mdss-pan-active-res", res, 2);
	if (rc == 0) {
		pinfo->lcdc.xres_pad =
			pinfo->xres - res[0];
		pinfo->lcdc.yres_pad =
			pinfo->yres - res[1];
	}

	rc = of_property_read_u32(np, "qcom,mdss-pan-bpp", &tmp);
	if (rc) {
		pr_err("%s:%d, panel bpp not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	pinfo->bpp = (!rc ? tmp : 24);

	pdest = of_get_property(np, "qcom,mdss-pan-dest", NULL);
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

#if defined(CONFIG_LCD_RECOVERY)

	if (pinfo->pdest == DISPLAY_1) {
		oled_det_gpio = of_get_named_gpio(np, "qcom,oled-det-gpio", 0);
#if defined(CONFIG_LCD_RECOVERY_ERR_FG)
		err_fg_gpio = of_get_named_gpio(np, "qcom,fg-err-gpio", 0);
		pr_info("%s:%d, err_fg_gpio(%d)",__func__, __LINE__, err_fg_gpio);
#endif
		pr_info("%s:%d, oled_det_gpio(%d)",__func__, __LINE__,oled_det_gpio);


		if (!gpio_is_valid(oled_det_gpio)) {
			pr_err("%s:%d, oled_det gpio not specified\n",
							__func__, __LINE__);
		} else {

			rc = gpio_request(oled_det_gpio, "oled_det_enable");
			if (rc) {
				pr_err("request oled_det gpio failed, rc=%d\n",
					   rc);
				gpio_free(oled_det_gpio);
/*				esd_enable = 0;*/
/*				return -ENODEV;*/
				}
			}

#if defined(CONFIG_LCD_RECOVERY_ERR_FG)
		if (!gpio_is_valid(err_fg_gpio)) {
			pr_err("%s:%d, err fg gpio not specified\n",
							__func__, __LINE__);
		} else {

			rc = gpio_request(err_fg_gpio, "err_fg_enable");
			if (rc) {
				pr_err("request err_fg gpio failed, rc=%d\n",
					   rc);
				gpio_free(oled_det_gpio);
/*				esd_enable = 0;*/
/*				return -ENODEV;*/
				}
			}
#endif
	    if (system_rev < 12)
				esd_enable = 0;
			else
				esd_enable = 1;

		pr_info("%s:%d, esd_enable(%d) system_rev(%d)\n",__func__, __LINE__,esd_enable, system_rev );
	}
#endif

	rc = of_property_read_u32_array(np,	"qcom,mdss-pan-porch-values", res, 6);
	pinfo->lcdc.h_back_porch = (!rc ? res[0] : 6);
	pinfo->lcdc.h_pulse_width = (!rc ? res[1] : 2);
	pinfo->lcdc.h_front_porch = (!rc ? res[2] : 6);
	pinfo->lcdc.v_back_porch = (!rc ? res[3] : 6);
	pinfo->lcdc.v_pulse_width = (!rc ? res[4] : 2);
	pinfo->lcdc.v_front_porch = (!rc ? res[5] : 6);

	rc = of_property_read_u32(np, "qcom,mdss-pan-underflow-clr", &tmp);
	pinfo->lcdc.underflow_clr = (!rc ? tmp : 0xff);

	bl_ctrl_type = of_get_property(np, "qcom,mdss-pan-bl-ctrl", NULL);

	if ((bl_ctrl_type) && (!strncmp(bl_ctrl_type, "bl_ctrl_wled", 12))) {
		led_trigger_register_simple("bkl-trigger", &bl_led_trigger);
		pr_debug("%s: SUCCESS-> WLED TRIGGER register\n", __func__);

		pinfo->bklt_ctrl = BL_WLED;
	} else if (!strncmp(bl_ctrl_type, "bl_ctrl_pwm", 11)) {
		pinfo->bklt_ctrl = BL_PWM;

		rc = of_property_read_u32(np, "qcom,dsi-pwm-period", &tmp);
		if (rc) {
			pr_err("%s:%d, Error, dsi pwm_period\n",
					__func__, __LINE__);
			return -EINVAL;
		}
		pinfo->pwm_period = tmp;

		rc = of_property_read_u32(np, "qcom,dsi-lpg-channel", &tmp);
		if (rc) {
			pr_err("%s:%d, Error, dsi lpg channel\n",
					__func__, __LINE__);
			return -EINVAL;
		}
		pinfo->pwm_lpg_chan = tmp;

		tmp = of_get_named_gpio(np, "qcom,dsi-pwm-gpio", 0);
		pinfo->pwm_pmic_gpio =  tmp;
	} else if (!strncmp(bl_ctrl_type, "bl_ctrl_dcs_cmds", 12)) {
		pr_debug("%s: SUCCESS-> DCS CMD BACKLIGHT register\n",
			 __func__);
		pinfo->bklt_ctrl = BL_DCS_CMD;
	} else {
		pr_debug("%s: Unknown backlight control\n", __func__);
		pinfo->bklt_ctrl = UNKNOWN_CTRL;
	}

	rc = of_property_read_u32(np, "qcom,mdss-brightness-max-level", &tmp);
	pinfo->brightness_max = (!rc ? tmp : MDSS_MAX_BL_BRIGHTNESS);

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-bl-levels", res, 2);
	pinfo->bl_min = (!rc ? res[0] : 0);
	pinfo->bl_max = (!rc ? res[1] : 255);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-mode", &tmp);
	pinfo->mipi.mode = (!rc ? tmp : DSI_VIDEO_MODE);

#if defined(CONFIG_LCD_FORCE_VIDEO_MODE)
	pinfo->mipi.mode = DSI_VIDEO_MODE;
#endif

	rc = of_property_read_u32(np, "qcom,mdss-vsync-enable", &tmp);
	pinfo->mipi.vsync_enable = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-hw-vsync-mode", &tmp);
	pinfo->mipi.hw_vsync_mode = (!rc ? tmp : 0);

	pr_info("pinfo->mipi.hw_vsync_mode = %d, pinfo->mipi.vsync_enable = %d ", pinfo->mipi.hw_vsync_mode, pinfo->mipi.vsync_enable);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-dsi-h-pulse-mode", &tmp);
	pinfo->mipi.pulse_mode_hsa_he = (!rc ? tmp : false);

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-dsi-h-power-stop", res, 3);
	pinfo->mipi.hbp_power_stop = (!rc ? res[0] : false);
	pinfo->mipi.hsa_power_stop = (!rc ? res[1] : false);
	pinfo->mipi.hfp_power_stop = (!rc ? res[2] : false);

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-dsi-bllp-power-stop", res, 2);
	pinfo->mipi.bllp_power_stop =
					(!rc ? res[0] : false);
	pinfo->mipi.eof_bllp_power_stop =
					(!rc ? res[1] : false);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-dsi-traffic-mode", &tmp);
	pinfo->mipi.traffic_mode =
			(!rc ? tmp : DSI_NON_BURST_SYNCH_PULSE);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-insert-dcs-cmd", &tmp);
	pinfo->mipi.insert_dcs_cmd =
		(!rc ? tmp : 1);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-wr-mem-continue", &tmp);
	pinfo->mipi.wr_mem_continue =
		(!rc ? tmp : 0x3c);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-wr-mem-start", &tmp);
	pinfo->mipi.wr_mem_start =
		(!rc ? tmp : 0x2c);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-te-sel", &tmp);
	pinfo->mipi.te_sel =
		(!rc ? tmp : 1);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-dsi-dst-format", &tmp);
	pinfo->mipi.dst_format =
			(!rc ? tmp : DSI_VIDEO_DST_FORMAT_RGB888);

#if defined(CONFIG_LCD_FORCE_VIDEO_MODE)
	pinfo->mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
#endif

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-vc", &tmp);
	pinfo->mipi.vc = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-rgb-swap", &tmp);
	pinfo->mipi.rgb_swap = (!rc ? tmp : DSI_RGB_SWAP_RGB);

	rc = of_property_read_u32(np, "qcom,mdss-force-clk-lane-hs", &tmp);
	pinfo->mipi.force_clk_lane_hs = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "samsung,mdss-early-lcd-on", &tmp);
			pinfo->early_lcd_on = (!rc ? tmp : 0);
	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-dsi-data-lanes", res, 4);
	pinfo->mipi.data_lane0 = (!rc ? res[0] : true);
	pinfo->mipi.data_lane1 = (!rc ? res[1] : false);
	pinfo->mipi.data_lane2 = (!rc ? res[2] : false);
	pinfo->mipi.data_lane3 = (!rc ? res[3] : false);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-dlane-swap", &tmp);
	pinfo->mipi.dlane_swap = (!rc ? tmp : 0);

	rc = of_property_read_u32_array(np, "qcom,mdss-pan-dsi-t-clk", res, 2);
	pinfo->mipi.t_clk_pre = (!rc ? res[0] : 0x24);
	pinfo->mipi.t_clk_post = (!rc ? res[1] : 0x03);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-stream", &tmp);
	pinfo->mipi.stream = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-tx-eot-append", &tmp);
	pinfo->mipi.tx_eot_append = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-mdp-tr", &tmp);
	pinfo->mipi.mdp_trigger =
			(!rc ? tmp : DSI_CMD_TRIGGER_SW);
	if (pinfo->mipi.mdp_trigger > 6) {
		pr_err("%s:%d, Invalid mdp trigger. Forcing to sw trigger",
						 __func__, __LINE__);
		pinfo->mipi.mdp_trigger =
					DSI_CMD_TRIGGER_SW;
	}

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-dma-tr", &tmp);
	pinfo->mipi.dma_trigger =
			(!rc ? tmp : DSI_CMD_TRIGGER_SW);
	if (pinfo->mipi.dma_trigger > 6) {
		pr_err("%s:%d, Invalid dma trigger. Forcing to sw trigger",
						 __func__, __LINE__);
		pinfo->mipi.dma_trigger =
					DSI_CMD_TRIGGER_SW;
	}

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-frame-rate", &tmp);
	pinfo->mipi.frame_rate = (!rc ? tmp : 60);

	rc = of_property_read_u32(np, "qcom,mdss-pan-clk-rate", &tmp);
	pinfo->clk_rate = (!rc ? tmp : 0);

	data = of_get_property(np, "qcom,panel-phy-regulatorSettings", &len);
	if ((!data) || (len != 7)) {
		pr_err("%s:%d, Unable to read Phy regulator settings",
		       __func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++)
		phy_params.regulator[i] = data[i];

	data = of_get_property(np, "qcom,panel-phy-timingSettings", &len);
	if ((!data) || (len != 12)) {
		pr_err("%s:%d, Unable to read Phy timing settings",
		       __func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++)
		phy_params.timing[i] = data[i];

	data = of_get_property(np, "qcom,panel-phy-strengthCtrl", &len);
	if ((!data) || (len != 2)) {
		pr_err("%s:%d, Unable to read Phy Strength ctrl settings",
		       __func__, __LINE__);
		return -EINVAL;
	}
	phy_params.strength[0] = data[0];
	phy_params.strength[1] = data[1];

	data = of_get_property(np, "qcom,panel-phy-bistCtrl", &len);
	if ((!data) || (len != 6)) {
		pr_err("%s:%d, Unable to read Phy Bist Ctrl settings",
		       __func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++)
		phy_params.bistctrl[i] = data[i];

	data = of_get_property(np, "qcom,panel-phy-laneConfig", &len);
	if ((!data) || (len != 45)) {
		pr_err("%s:%d, Unable to read Phy lane configure settings",
		       __func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++)
		phy_params.lanecfg[i] = data[i];

	pinfo->mipi.dsi_phy_db = phy_params;

	fbc_enabled = of_property_read_bool(np,
		"qcom,fbc-enabled");
	if (fbc_enabled) {
		pr_debug("%s:%d FBC panel enabled.\n", __func__, __LINE__);
		pinfo->fbc.enabled = 1;

		rc = of_property_read_u32_array(np,
				"qcom,fbc-mode", fbc_res, 7);
		pinfo->fbc.target_bpp =
			(!rc ?	fbc_res[0] : pinfo->bpp);
		pinfo->fbc.comp_mode = (!rc ? fbc_res[1] : 0);
		pinfo->fbc.qerr_enable =
			(!rc ? fbc_res[2] : 0);
		pinfo->fbc.cd_bias = (!rc ? fbc_res[3] : 0);
		pinfo->fbc.pat_enable = (!rc ? fbc_res[4] : 0);
		pinfo->fbc.vlc_enable = (!rc ? fbc_res[5] : 0);
		pinfo->fbc.bflc_enable =
			(!rc ? fbc_res[6] : 0);

		rc = of_property_read_u32_array(np,
				"qcom,fbc-budget-ctl", fbc_res, 3);
		pinfo->fbc.line_x_budget =
			(!rc ? fbc_res[0] : 0);
		pinfo->fbc.block_x_budget =
			(!rc ? fbc_res[1] : 0);
		pinfo->fbc.block_budget =
			(!rc ? fbc_res[2] : 0);

		rc = of_property_read_u32_array(np,
				"qcom,fbc-lossy-mode", fbc_res, 4);
		pinfo->fbc.lossless_mode_thd =
			(!rc ? fbc_res[0] : 0);
		pinfo->fbc.lossy_mode_thd =
			(!rc ? fbc_res[1] : 0);
		pinfo->fbc.lossy_rgb_thd =
			(!rc ? fbc_res[2] : 0);
		pinfo->fbc.lossy_mode_idx =
			(!rc ? fbc_res[3] : 0);

	} else {
		pr_debug("%s:%d Panel does not support FBC.\n",
				__func__, __LINE__);
		pinfo->fbc.enabled = 0;
		pinfo->fbc.target_bpp =
			pinfo->bpp;
	}

	mdss_dsi_parse_roi_alignment(np, pinfo);

	mdss_panel_parse_te_params(np, pinfo);

	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->on_cmds,
		"qcom,mdss-dsi-on-command", "qcom,mdss-dsi-on-command-state");

	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->off_cmds,
		"qcom,mdss-dsi-off-command", "qcom,mdss-dsi-off-command-state");

	mdss_samsung_parse_panel_cmd(np, &hsync_on_seq,
				"qcom,panel-hsync-on-seq");

	mdss_samsung_parse_panel_cmd(np, &display_on_cmd,
				"qcom,panel-display-on-cmds");
	mdss_samsung_parse_panel_cmd(np, &display_off_cmd,
				"qcom,panel-display-off-cmds");

	mdss_samsung_parse_panel_cmd(np, &test_key_enable_cmds,
				"samsung,panel-test-key-enable-cmds");
	mdss_samsung_parse_panel_cmd(np, &test_key_disable_cmds,
				"samsung,panel-test-key-disable-cmds");

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FHD_FA2_PT_PANEL)
	mdss_samsung_parse_panel_cmd(np, &panel_set_te_osc_b,
				"samsung,panel-set-te-osc-b");
	mdss_samsung_parse_panel_cmd(np, &panel_set_te_restore,
				"samsung,panel-set-te-restore");
	mdss_samsung_parse_panel_cmd(np, &panel_set_te_1,
				"samsung,panel-set-te1");
	mdss_samsung_parse_panel_cmd(np, &panel_set_te_2,
				"samsung,panel-set-te2");
	mdss_samsung_parse_panel_cmd(np, &panel_set_te,
				"samsung,panel-set-te");
	mdss_samsung_parse_panel_cmd(np, &panel_osc_type_read_cmds,
				"samsung,panel-osc-type-read");
#endif
#if defined(CONFIG_WACOM_LCD_FREQ_COMPENSATE)
	mdss_samsung_parse_panel_cmd(np, &write_ldi_fps_cmds,
				"samsung,panel-ldi-fps-write-cmds");
#endif

	mdss_samsung_parse_panel_cmd(np, &nv_mtp_read_cmds,
				"samsung,panel-nv-mtp-read-cmds");
	mdss_samsung_parse_panel_cmd(np, &nv_enable_cmds,
				"samsung,panel-nv-read-enable-cmds");
	mdss_samsung_parse_panel_cmd(np, &nv_disable_cmds,
				"samsung,panel-nv-read-disable-cmds");

	mdss_samsung_parse_panel_cmd(np, &manufacture_id_cmds,
				"samsung,panel-manufacture-id-read-cmds");
	mdss_samsung_parse_panel_cmd(np, &manufacture_date_cmds,
				"samsung,panel-manufacture-date-read-cmds");
	mdss_samsung_parse_panel_cmd(np, &ddi_id_cmds,
				"samsung,panel-ddi-id-read-cmds");
	mdss_samsung_parse_panel_cmd(np, &rddpm_cmds,
				"samsung,panel-rddpm-read-cmds");
	mdss_samsung_parse_panel_cmd(np, &rddsm_cmds,
				"samsung,panel-rddsm-read-cmds");
	mdss_samsung_parse_panel_cmd(np, &mtp_read_sysfs_cmds,
				"samsung,panel-mtp-read-sysfs-cmds");

	mdss_samsung_parse_panel_cmd(np, &acl_off_cmd,
				"samsung,panel-acl-off-cmds");
	mdss_samsung_parse_panel_cmd(np, &acl_cmds_list,
				"samsung,panel-acl-cmds-list");
	mdss_samsung_parse_panel_cmd(np, &opr_avg_cal_cmd,
				"samsung,panel-acl-OPR-avg-cal");
	mdss_samsung_parse_panel_cmd(np, &aclcont_cmds_list,
				"samsung,panel-aclcont-cmds-list");

	mdss_samsung_parse_panel_cmd(np, &gamma_cmds_list,
				"samsung,panel-gamma-cmds-list");
	mdss_samsung_parse_panel_cmd(np, &elvss_cmds_list,
				"samsung,panel-elvss-cmds-list");
	mdss_samsung_parse_panel_cmd(np, &elvss_cmds_revI_list,
				"samsung,panel-elvss-cmds-revI-list");

	if((msd.panel == PANEL_WQHD_OCTA_S6E3HA0_CMD)&& (msd.id3 == EVT0_K_wqhd_REVE)){
		mdss_samsung_parse_panel_cmd(np, &aid_cmds_list,
					"samsung,panel-aid-cmds-revE-list");
		mdss_samsung_parse_panel_table(np, &aid_map_table,
					"samsung,panel-aid-map-revE-table");
	}else if((msd.panel == PANEL_WQHD_OCTA_S6E3HA0_CMD)&& (msd.id3 == EVT0_K_wqhd_REVF)){
		mdss_samsung_parse_panel_cmd(np, &aid_cmds_list,
				"samsung,panel-aid-cmds-revF-list");
		mdss_samsung_parse_panel_table(np, &aid_map_table,
				"samsung,panel-aid-map-revF-table");
	}else if((msd.panel == PANEL_WQHD_OCTA_S6E3HA0_CMD)
		&& ((msd.id3 == EVT0_K_wqhd_REVG)||(msd.id3 == EVT0_K_wqhd_REVH)||(msd.id3 == EVT0_K_wqhd_REVI))){
		mdss_samsung_parse_panel_cmd(np, &aid_cmds_list,
				"samsung,panel-aid-cmds-revG-list");
		mdss_samsung_parse_panel_table(np, &aid_map_table,
				"samsung,panel-aid-map-revG-table");
	}else if((msd.panel == PANEL_WQHD_OCTA_S6E3HA0_CMD)&& (msd.id3 == EVT0_K_wqhd_REVJ)){
		mdss_samsung_parse_panel_cmd(np, &aid_cmds_list,
				"samsung,panel-aid-cmds-revJ-list");
		mdss_samsung_parse_panel_table(np, &aid_map_table,
				"samsung,panel-aid-map-revJ-table");
	}else if((msd.panel == PANEL_WQHD_OCTA_S6E3HA0_CMD)&& (msd.id3 == EVT0_K_wqhd_REVK)){
		mdss_samsung_parse_panel_cmd(np, &aid_cmds_list,
				"samsung,panel-aid-cmds-revK-list");
		mdss_samsung_parse_panel_table(np, &aid_map_table,
				"samsung,panel-aid-map-revK-table");
	}else if((msd.panel == PANEL_WQHD_OCTA_S6E3HA0_CMD)&& (msd.id3 == EVT0_K_wqhd_REVL)){
		mdss_samsung_parse_panel_cmd(np, &aid_cmds_list,   "samsung,panel-aid-cmds-revL-list");
		mdss_samsung_parse_panel_table(np, &aid_map_table, "samsung,panel-aid-map-revL-table");
	}else if(msd.panel == PANEL_WQHD_OCTA_S6E3HA2X01_CMD){
			if (msd.id3 == EVT0_T_wqhd_REVF ||msd.id3 == EVT0_T_wqhd_REVG)
				mdss_samsung_parse_panel_cmd(np, &aid_cmds_list,
					"samsung,panel-aid-cmds-revF-list");
			else if(msd.id3 == EVT0_T_wqhd_REVH || msd.id3 == EVT0_T_wqhd_REVI || msd.id3 == EVT0_T_wqhd_REVJ)
				mdss_samsung_parse_panel_cmd(np, &aid_cmds_list,
					"samsung,panel-aid-cmds-revH-list");
			else
				mdss_samsung_parse_panel_cmd(np, &aid_cmds_list,
					"samsung,panel-aid-cmds-revA-list");

			mdss_samsung_parse_panel_table(np, &aid_map_table,
				"samsung,panel-aid-map-revA-table");
	}else if(msd.panel == PANEL_WQXGA_OCTA_S6E3HA2X01_CMD){
		if(msd.id2 == YM4_PANEL){
			mdss_samsung_parse_panel_cmd(np, &aid_cmds_list,
				"samsung,panel-aid-cmds-ym4-list");
			mdss_samsung_parse_panel_table(np, &aid_map_table,
				"samsung,panel-aid-map-revA-table");
		}else if(msd.id3 == EVT0_T_wqxga_REVD || msd.id3 == EVT0_T_wqxga_REVB){
			mdss_samsung_parse_panel_cmd(np, &aid_cmds_list,
						"samsung,panel-aid-cmds-revD-list");
			mdss_samsung_parse_panel_table(np, &aid_map_table,
					"samsung,panel-aid-map-revA-table");
		}else if(msd.id3 == EVT0_T_wqxga_REVE){
			mdss_samsung_parse_panel_cmd(np, &aid_cmds_list,
						"samsung,panel-aid-cmds-revE-list");
			mdss_samsung_parse_panel_table(np, &aid_map_table,
					"samsung,panel-aid-map-revA-table");
		}else{
		mdss_samsung_parse_panel_cmd(np, &aid_cmds_list,
					"samsung,panel-aid-cmds-revA-list");
			mdss_samsung_parse_panel_table(np, &aid_map_table,
				"samsung,panel-aid-map-revA-table");
		}
	}else{
		mdss_samsung_parse_panel_cmd(np, &aid_cmds_list,
					"samsung,panel-aid-cmds-list");
		mdss_samsung_parse_panel_table(np, &aid_map_table,
					"samsung,panel-aid-map-table");
	}

	mdss_samsung_parse_panel_table(np, &acl_map_table,
				"samsung,panel-acl-map-table");

	/* Process the lux value table */
	mdss_samsung_parse_candella_lux_mapping_table(np, &candela_map_table,
				"samsung,panel-candella-mapping-table");

#ifdef LDI_ADJ_VDDM_OFFSET
	mdss_samsung_parse_panel_cmd(np, &read_vdd_ref_cmds,
				"samsung,panel-ldi-vdd-read-cmds");
	mdss_samsung_parse_panel_cmd(np, &write_vdd_offset_cmds,
				"samsung,panel-ldi-vdd-offset-write-cmds");
	mdss_samsung_parse_panel_cmd(np, &read_vddm_ref_cmds,
				"samsung,panel-ldi-vddm-read-cmds");
	mdss_samsung_parse_panel_cmd(np, &write_vddm_offset_cmds,
				"samsung,panel-ldi-vddm-offset-write-cmds");
#endif

#if defined(HBM_RE)
	if (msd.panel == PANEL_WQHD_OCTA_S6E3HA0_CMD ||\
		msd.panel == PANEL_FHD_OCTA_S6E3FA2_CMD){
		mdss_samsung_parse_panel_cmd(np, &nv_mtp_hbm_read_cmds,
					"samsung,panel-nv-mtp-read-hbm-cmds");
		mdss_samsung_parse_panel_cmd(np, &nv_mtp_hbm2_read_cmds,
					"samsung,panel-nv-mtp-read-hbm2-cmds");
		mdss_samsung_parse_panel_cmd(np, &hbm_gamma_cmds_list,
					"samsung,panel-gamma-hbm-cmds-list");

		if(msd.panel == PANEL_FHD_OCTA_S6E3FA2_CMD) {
			mdss_samsung_parse_panel_cmd(np, &hbm_etc_cmds_list,
						"samsung,panel-etc-hbm-cmds");
		} else {
			if (msd.id3 == EVT0_K_wqhd_REVB)
				mdss_samsung_parse_panel_cmd(np, &hbm_etc_cmds_list,
						"samsung,panel-etc-hbm-revB-cmds");
			else if (msd.id3 == EVT0_K_wqhd_REVC || msd.id3 == EVT0_K_wqhd_REVD)
				mdss_samsung_parse_panel_cmd(np, &hbm_etc_cmds_list,
						"samsung,panel-etc-hbm-revC-cmds");
			else if (msd.id3 == EVT0_K_wqhd_REVE )
				mdss_samsung_parse_panel_cmd(np, &hbm_etc_cmds_list,
						"samsung,panel-etc-hbm-revE-cmds");
			else if (msd.id3 >= EVT0_K_wqhd_REVF )
				mdss_samsung_parse_panel_cmd(np, &hbm_etc_cmds_list,
						"samsung,panel-etc-hbm-revF-cmds");
		}
	}else{
		mdss_samsung_parse_panel_cmd(np, &hbm_etc_cmds_list,
					"samsung,panel-hbm-cmds-list");
		mdss_samsung_parse_panel_cmd(np, &hbm_off_cmd,
					"samsung,panel-hbm-off-cmds");
	}
#endif

	mdss_samsung_parse_panel_cmd(np, &nv_mtp_elvss_read_cmds,
				"samsung,panel-nv-mtp-read-elvss-cmds");

#if defined(CONFIG_MDNIE_LITE_TUNING)
	mdss_samsung_parse_panel_cmd(np, &nv_mdnie_read_cmds,
					"samsung,panel-nv-mdnie-read-cmds");
#endif
#ifdef DEBUG_LDI_STATUS
	mdss_samsung_parse_panel_cmd(np, &ldi_debug_cmds,
				"samsung,panel-ldi-debug-read-cmds");
#endif
#if defined(TEMPERATURE_ELVSS)
	mdss_samsung_parse_panel_cmd(np, &elvss_lowtemp_cmds_list,
				"samsung,panel-elvss-lowtemp-cmds-list");

	mdss_samsung_parse_panel_cmd(np, &elvss_lowtemp2_cmds_list,
				"samsung,panel-elvss-lowtemp2-cmds-list");
#endif
#if defined(SMART_ACL)
	mdss_samsung_parse_panel_cmd(np, &smart_acl_elvss_cmds_list,
				"samsung,panel-smart-acl-elvss-cmds-list");
	mdss_samsung_parse_panel_table(np, &smart_acl_elvss_map_table,
				"samsung,panel-smart-acl-elvss-map-table");
#endif
#if defined(SMART_VINT)
mdss_samsung_parse_panel_cmd(np, &smart_vint_cmds_list,
			"samsung,panel-smart-vint-cmds-list");
mdss_samsung_parse_panel_table(np, &smart_vint_map_table,
			"samsung,panel-smart-vint-map-table");
#endif
#if defined(PARTIAL_UPDATE)
	mdss_samsung_parse_panel_cmd(np, &partialdisp_on_cmd,
				"samsung,panel-ldi-partial-disp-on");
	mdss_samsung_parse_panel_cmd(np, &partialdisp_off_cmd,
				"samsung,panel-ldi-partial-disp-off");
#endif
#ifdef CONFIG_FB_MSM_SAMSUNG_AMOLED_LOW_POWER_MODE
	mdss_samsung_parse_panel_cmd(np, &alpm_on_seq,
				"samsung,panel-alpm-on-seq");
	mdss_samsung_parse_panel_cmd(np, &alpm_off_seq,
				"samsung,panel-alpm-off-seq");
#endif
#if defined(CONFIG_LCD_HMT)
	mdss_samsung_parse_panel_cmd(np, &hmt_single_scan_enable,
				"samsung,panel-hmt-single-scan-enable");
	mdss_samsung_parse_panel_cmd(np, &hmt_disable,
				"samsung,panel-hmt-disable");
	mdss_samsung_parse_panel_cmd(np, &hmt_reverse_enable,
				"samsung,panel-hmt-reverse-enable");
	mdss_samsung_parse_panel_cmd(np, &hmt_reverse_disable,
				"samsung,panel-hmt-reverse-disable");
	mdss_samsung_parse_panel_cmd(np, &hmt_bright_cmds_list,
				"samsung,hmt-panel-bright-cmds-list");
	mdss_samsung_parse_panel_cmd(np, &hmt_aid_cmds_list,
				"samsung,panel-aid-cmds-list-hmt");

	mdss_samsung_parse_panel_table(np, &aid_map_table_reverse_hmt,
				"samsung,panel-aid-map-table-reverse-hmt");
	mdss_samsung_parse_candella_lux_mapping_table(np, &candela_map_table_reverse_hmt,
				"samsung,panel-candella-mapping-table-reverse-hmt");

	mdss_samsung_parse_panel_cmd(np, &hmt_150cd_read_cmds,
					"samsung,panel-hmt_150cd-read-cmds");
#endif

	if(lcd_attached){
		pinfo->ulps_feature_enabled = of_property_read_bool(np,
				"qcom,ulps-enabled");
		pr_info(" ulps feature %s", (pinfo->ulps_feature_enabled ? "enabled" : "disabled"));
	}

	return 0;
}

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void mipi_samsung_disp_early_suspend(struct early_suspend *h)
{
	msd.mfd->resume_state = MIPI_SUSPEND_STATE;

	LCD_DEBUG("------");
}

static void mipi_samsung_disp_late_resume(struct early_suspend *h)
{

	msd.mfd->resume_state = MIPI_RESUME_STATE;

	LCD_DEBUG("------");
}
#endif

static int is_panel_supported(const char *panel_name)
{
	int i = 0;

	if (panel_name == NULL)
		return -EINVAL;

	if(get_lcd_id()){
		msd.id3 = (get_lcd_id()&0xFF);
		msd.id2 = ((get_lcd_id()&0xFF00)>>8);
	}

	while(panel_supp_cdp[i].name != NULL)	{
		if(!strcmp(panel_name,panel_supp_cdp[i].name))
			break;
		i++;
	}

	if (i < ARRAY_SIZE(panel_supp_cdp)) {
		memcpy(msd.panel_name, panel_name, MAX_PANEL_NAME_SIZE);
		msd.panel = panel_supp_cdp[i].panel_code;
		return 0;
	}
	return -EINVAL;
}

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FHD_FA2_PT_PANEL)

struct te_fctrl_lookup_table te_fctrl_lookup_table[] = {
	{33333, 0x0 },
	{22999, 0x32},
	{22861, 0x33},
	{22726, 0x34},
	{22609, 0x35},
	{22490, 0x36},
	{22363, 0x37},
	{22237, 0x38},
	{22121, 0x39},
	{22005, 0x3A},
	{21880, 0x3B},
	{21761, 0x3C},
	{21649, 0x3D},
	{21535, 0x3E},
	{21461, 0x3F},
	{21385, 0x40},
	{21275, 0x41},
	{21170, 0x42},
	{21056, 0x43},
	{20943, 0x44},
	{20834, 0x45},
	{20723, 0x46},
	{20614, 0x47},
	{20506, 0x48},
	{20406, 0x49},
	{20311, 0x4A},
	{20206, 0x4B},
	{20099, 0x4C},
	{20003, 0x4D},
	{19903, 0x4E},
	{19808, 0x4F},
	{19717, 0x50},
	{19621, 0x51},
	{19526, 0x52},
	{19428, 0x53},
	{19331, 0x54},
	{19244, 0x55},
	{19153, 0x56},
	{19056, 0x57},
	{18963, 0x58},
	{18879, 0x59},
	{18794, 0x5A},
	{18702, 0x5B},
	{18610, 0x5C},
	{18522, 0x5D},
	{18438, 0x5E},
	{18359, 0x5F},
	{18277, 0x60},
	{18196, 0x61},
	{18116, 0x62},
	{18030, 0x63},
	{17943, 0x64},
	{17862, 0x65},
	{17783, 0x66},
	{17704, 0x67},
	{17625, 0x68},
	{17545, 0x69},
	{17464, 0x6A},
	{17384, 0x6B},
	{17306, 0x6C},
	{17231, 0x6D},
	{17158, 0x6E},
	{17089, 0x6F},
	{17016, 0x70},
	{16944, 0x71},
	{16874, 0x72},
	{16797, 0x73},
	{16717, 0x74},
	{16646, 0x75},
	{16578, 0x76},
	{16505, 0x77},
	{16433, 0x78},
	{16365, 0x79},
	{16299, 0x7A},
	{16228, 0x7B},
	{16152, 0x7C},
	{16089, 0x7D},
	{16027, 0x7E},
	{15973, 0x7F},
	{15921, 0x80},
	{15855, 0x81},
	{15787, 0x82},
	{15720, 0x83},
	{15657, 0x84},
	{15594, 0x85},
	{15532, 0x86},
	{15466, 0x87},
	{15401, 0x88},
	{15342, 0x89},
	{15283, 0x8A},
	{14286, 0x8B},
};
struct te_offset_lookup_table te_offset_lookup_table[] = {
	{19654, 0  },
	{19551, -34},
	{19448, -33},
	{19354, -32},
	{19264, -31},
	{19164, -30},
	{19063, -29},
	{18971, -28},
	{18877, -27},
	{18786, -26},
	{18701, -25},
	{18610, -24},
	{18519, -23},
	{18427, -22},
	{18335, -21},
	{18252, -20},
	{18166, -19},
	{18074, -18},
	{17986, -17},
	{17906, -16},
	{17825, -15},
	{17738, -14},
	{17650, -13},
	{17567, -12},
	{17487, -11},
	{17412, -10},
	{17334, -9 },
	{17257, -8 },
	{17182, -7 },
	{17101, -6 },
	{17018, -5 },
	{16941, -4 },
	{16866, -3 },
	{16818, -2 },
	{16518, 0  },
	{16488, 2 },
	{16414, 3 },
	{16343, 4 },
	{16273, 5 },
	{16207, 6 },
	{16139, 7 },
	{16070, 8 },
	{16005, 9 },
	{15931, 10},
	{15855, 11},
	{15788, 12},
	{15723, 13},
	{15654, 14},
	{15586, 15},
	{15521, 16},
	{15459, 17},
	{15391, 18},
	{15320, 19},
	{15259, 20},
	{15200, 21},
	{15149, 22},
	{15100, 23},
	{15037, 24},
	{14973, 25},
	{14909, 26},
	{14849, 27},
	{14790, 28},
	{14731, 29},
	{14669, 30},
	{14607, 31},
	{14551, 32},
	{14495, 33},
	{14435, 34},
};

extern int te;
extern int te_cnt;
extern struct completion te_check_comp;

static char check_te_step1(void)
{
	int ret;
	int size = sizeof(te_fctrl_lookup_table)/(sizeof(int)+sizeof(char)) - 1;
	int i;
	char fctrl;

	pr_info("[%s] ++ \n", __func__);

	INIT_COMPLETION(te_check_comp);

	te_cnt = 0;
	ret = wait_for_completion_timeout(&te_check_comp,
						msecs_to_jiffies(16 * 4));
	if (!ret) {
		pr_err("[ERROR] te_check_comp timeout!!\n");
		return 0;
	}

	if (te != 0) {
		pr_info("[%s] first TE = %d\n", __func__, te);
		for (i = 0; i < size; i++) {
			if (te < te_fctrl_lookup_table[i].te && te >= te_fctrl_lookup_table[i+1].te) {
				fctrl = te_fctrl_lookup_table[i+1].value;
				break;
			}
		}

		if (i == size) {
			pr_err("[%s] out of range ... (%d)\n", __func__, te);
			return 0;
		}

		pr_info("[%s] fctrl = %x\n", __func__, fctrl);

		panel_set_te_1.cmd_desc[3].payload[1] = fctrl;
		mipi_samsung_disp_send_cmd(PANEL_SET_TE_1, true);
	} else {
		pr_info("[%s] TE is 0..\n", __func__);
		return 0;
	}

	pr_info("[%s] -- \n", __func__);

	return fctrl;
}

static int check_te_step2(char fctrl)
{
	int ret;
	int size = sizeof(te_offset_lookup_table)/(sizeof(int)*2) - 1;
	int i;
	int offset;

	pr_info("[%s] ++ \n", __func__);

	INIT_COMPLETION(te_check_comp);

	te_cnt = 0;
	ret = wait_for_completion_timeout(&te_check_comp,
						msecs_to_jiffies(16 * 4));
	if (!ret) {
		pr_err("[ERROR] te_check_comp timeout!!\n");
		return -1;
	}

	if (te != 0) {
		pr_info("[%s] second TE = %d\n", __func__, te);
		for (i = 0; i < size; i++) {
			if (te < te_offset_lookup_table[i].te && te >= te_offset_lookup_table[i+1].te) {
				offset = te_offset_lookup_table[i+1].offset;
				break;
			}
		}
		if (i == size) {
			pr_err("[%s] out of range ... (%d)\n", __func__, te);
			return -1;
		}

		pr_info("[%s] offset = %d\n", __func__, offset);

		panel_set_te_2.cmd_desc[3].payload[1] = fctrl + offset;
		panel_set_te.cmd_desc[4].payload[1] = fctrl + offset;
		mipi_samsung_disp_send_cmd(PANEL_SET_TE_2, true);
	} else {
		pr_info("[%s] TE is 0..\n", __func__);
		return -1;
	}

	pr_info("[%s] -- \n", __func__);

	return 0;
}

static int check_te_step3(void)
{
	int ret;

	pr_info("[%s] ++ \n", __func__);

	INIT_COMPLETION(te_check_comp);

	te_cnt = 0;
	ret = wait_for_completion_timeout(&te_check_comp,
						msecs_to_jiffies(16 * 4));
	if (!ret) {
		pr_err("[ERROR] te_check_comp timeout!!\n");
		return -1;
	}

	// +5% = 17.499 , -5% = 15.832
	if (te >= (16666 * 105 / 100) || te <= (16666 * 95 / 100)) {
		pr_err("[%s] TE is not correct!! (%d) back to OSC type A..\n", __func__, te);
		return -1;
	} else
		pr_info("[%s] finals TE is (%d) - OK\n", __func__, te);

	pr_info("[%s] -- \n", __func__);

	return 0;
}
#endif

static int samsung_dsi_panel_event_handler(int event)
{
	char rddpm_buf[4], rddsm_buf[4];
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FHD_FA2_PT_PANEL)
	char fctrl;
#endif

	pr_debug("%s : %d",__func__,event);
	switch (event) {
		case MDSS_EVENT_FRAME_UPDATE:
			if(msd.dstat.wait_disp_on) {
				pr_info("DISPLAY_ON\n");
				mipi_samsung_disp_send_cmd(PANEL_DISPLAY_ON, true);

				mipi_samsung_read_nv_mem(msd.pdata, &rddpm_cmds, rddpm_buf);

#if defined(CONFIG_LCD_HMT)
				if (!is_first) {
					if (msd.hmt_stat.hmt_on && msd.hmt_stat.hmt_low_persistence) {
						pr_info("hmt on (%d), setting for HMT!\n", msd.hmt_stat.hmt_on);
						hmt_enable(1);
						hmt_reverse_update(1);
					}
					hmt_bright_update();
				} else
					is_first = 0;
#endif

				msd.dstat.wait_disp_on = 0;
			}
			break;
		case MDSS_EVENT_READ_LDI_STATUS:
			mipi_samsung_read_nv_mem(msd.pdata, &rddpm_cmds, rddpm_buf);
			mipi_samsung_read_nv_mem(msd.pdata, &rddsm_cmds, rddsm_buf);
			return (int)rddpm_buf[0];
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FHD_FA2_PT_PANEL)
		case MDSS_EVENT_TE_UPDATE:
			if (te_set_done == TE_SET_READY) {

				te_set_done = TE_SET_START;

				mipi_samsung_disp_send_cmd(PANEL_SET_TE_OSC_B, true);

				fctrl = check_te_step1();
				if (!fctrl)
					return -1;

				if (check_te_step2(fctrl) < 0)
					return -1;

				if (check_te_step3() < 0)
					return -1;

				te_set_done = TE_SET_DONE;
			}
			break;
		case MDSS_EVENT_TE_RESTORE:
			pr_info("RESTORE_TE (OSC TYPE A)\n");
			mipi_samsung_disp_send_cmd(PANEL_SET_TE_RESTORE, true);
			te_set_done = TE_SET_FAIL;
			break;
#endif

#if defined(CONFIG_MDNIE_LITE_TUNING)
		case MDSS_EVENT_MDNIE_DEFAULT_UPDATE:
			pr_info("%s : send CONFIG_MDNIE_LITE_TUNING... \n",__func__);
			is_negative_on();
			break;
#endif
		default:
			pr_debug("%s: unhandled event=%d\n", __func__, event);
			break;
	}

	return 0;
}

static int mdss_dsi_panel_blank(struct mdss_panel_data *pdata, int blank)
{
	if(blank) {
		pr_debug("%s:%d, blanking panel\n", __func__, __LINE__);
		mipi_samsung_disp_send_cmd(PANEL_DISPLAY_BLANK, false);
	}
	else {
		pr_debug("%s:%d, unblanking panel\n", __func__, __LINE__);
		mipi_samsung_disp_send_cmd(PANEL_DISPLAY_UNBLANK, false);
	}
	return 0;
}

#if defined(CONFIG_WACOM_LCD_FREQ_COMPENSATE)

#define LUT_SIZE (sizeof(freq_cal_lut_offset) / sizeof(int)) / 3

static int freq_cal_lut_offset[][3] = {
	{56568, 56686,	-25},
	{56686, 56805,	-24},
	{56805, 56924,	-23},
	{56924, 57044,	-22},
	{57044, 57165,	-21},
	{57165, 57286,	-20},
	{57286, 57407,	-19},
	{57407, 57529,	-18},
	{57529, 57651,	-17},
	{57651, 57774,	-16},
	{57774, 57898,	-15},
	{57898, 58022,	-14},
	{58022, 58146,	-13},
	{58146, 58272,	-12},
	{58272, 58397,	-11},
	{58397, 58523,	-10},
	{58523, 58650,	-9},
	{58650, 58777,	-8},
	{58777, 58905,	-7},
	{58905, 59034,	-6},
	{59034, 59163,	-5},
	{59163, 59292,	-4},
	{59292, 59422,	-3},
	{59422, 59553,	-2},
	{59553, 59684,	-1},
	{59684, 59816,	0},
	{59816, 59948,	1},
	{59948, 60081,	2},
	{60081, 60215,	3},
	{60215, 60349,	4},
	{60349, 60484,	5},
	{60484, 60619,	6},
	{60619, 60755,	7},
	{60755, 60892,	8},
	{60892, 61029,	9},
	{61029, 61167,	10},
	{61167, 61305,	11},
	{61305, 61444,	12},
	{61444, 61584,	13},
	{61584, 61724,	14},
	{61724, 61865,	15},
	{61865, 62007,	16},
	{62007, 62149,	17},
	{62149, 62292,	18},
	{62292, 62436,	19},
	{62436, 62580,	20},
	{62580, 62725,	21},
	{62725, 62871,	22},
	{62871, 63017,	23},
	{63017, 63164,	24},
	{63164, 63312,	25},
	{63312, 63460,	26},
	{63460, 63609,	27},
	{63609, 63759,	28},
	{63759, 63909,	29},
	{63909, 64010,	30},
};

int ldi_fps(unsigned int input_fps)
{
	int offset = 0;
	int i;
	int dest_cal_val;

	pr_info("%s :input_fps (%d), lut size (%d)\n", __func__, input_fps, LUT_SIZE);

	if ((msd.manufacture_id & 0xFF) <= 0x01) {
		pr_err("%s :LDI EVT0 Not Support. Skip!! \n",__func__);
		return 0;
	}

	for (i = 0; i < LUT_SIZE; i++) {
		if (input_fps >= freq_cal_lut_offset[i][0] &&
			input_fps < freq_cal_lut_offset[i][1]) {
			offset = freq_cal_lut_offset[i][2];
			break;
		}
	}

	if (i == LUT_SIZE) {
		pr_err("%s :can not find offset !!\n", __func__);
		return 0;
	}

	if (msd.mfd->resume_state == MIPI_RESUME_STATE) {
		pr_info("%s :current comp value(0x%x),offset(%d)\n", __func__,
			write_ldi_fps_cmds.cmd_desc[1].payload[3], offset);

		dest_cal_val = write_ldi_fps_cmds.cmd_desc[1].payload[3] + offset;

		if((dest_cal_val < 0xAC) || (dest_cal_val > 0xE3)) {
			pr_err("Invalid cal value(0x%x)", dest_cal_val);
			return 0;
		}
		else
			pr_info("%s :dest write value (0x%x)\n", __func__, dest_cal_val);

		write_ldi_fps_cmds.cmd_desc[1].payload[3] = dest_cal_val;
		mipi_samsung_disp_send_cmd(PANEL_LDI_FPS_CHANGE, true);
	} else {
		pr_err("%s : Panel is off state!!\n", __func__);
		return 0;
	}

	return 1;
}
EXPORT_SYMBOL(ldi_fps);
#endif

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
					mdnie_head[cmd_pos] =
					char_to_dec(*(src + data_pos + 2),
							*(src + data_pos + 3));
				} else {
					mdnie_body[cmd_pos] =
					char_to_dec(*(src + data_pos + 2),
							*(src + data_pos + 3));
				}

				data_pos += 3;
				cmd_pos++;

				if (cmd_pos == MDNIE_TUNE_HEAD_SIZE && !cmd_step) {
					cmd_pos = 0;
					cmd_step = 1;
				}else if (cmd_pos == MDNIE_TUNE_BODY_SIZE && cmd_step) {/*blocking overflow*/
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
	for (data_pos = 0; data_pos < MDNIE_TUNE_HEAD_SIZE ; data_pos++)
		printk(KERN_INFO "0x%x ", mdnie_head[data_pos]);
	printk(KERN_INFO "\n");
	for (data_pos = 0; data_pos < MDNIE_TUNE_BODY_SIZE ; data_pos++)
		printk(KERN_INFO "0x%x ", mdnie_body[data_pos]);
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
void copy_tuning_data_from_adb(char *body, char *head);
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
	copy_tuning_data_from_adb(mdnie_body, mdnie_head);
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

	rc = snprintf((char *)buf, 4, "%d\n", mdss_fb_is_power_on(mfd));
	pr_info("mipi_samsung_disp_get_power(%d)\n", mdss_fb_is_power_on(mfd));

	return rc;
}

static ssize_t mipi_samsung_disp_set_power(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int power;
	struct msm_fb_data_type *mfd = msd.mfd;

	if (sscanf(buf, "%u", &power) != 1)
		return -EINVAL;

	if (power == mdss_fb_is_power_on(mfd))
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
	char temp[100];

	if(msd.manufacture_id){
		snprintf(temp, 20, "SDC_%x\n",msd.manufacture_id);
	}else{
		pr_info("no manufacture id\n");

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
	}

	strlcat(buf, temp, 100);

	return strnlen(buf, 100);
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

static ssize_t mipi_samsung_disp_manufacture_date_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char temp[60];

	snprintf((char *)temp, sizeof(temp), "manufacture date(%d) time(%d)\n", msd.manufacture_date, msd.manufacture_time);
	strlcat(buf, temp, 60);

	pr_info("manufacture date(%d) time(%d)\n", msd.manufacture_date, msd.manufacture_time);
	return strnlen(buf, 60);
}

static ssize_t mipi_samsung_disp_manufacture_code_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char temp[30];

	snprintf((char *)temp, sizeof(temp), "%02x%02x%02x%02x%02x\n",
		msd.ddi_id[0], msd.ddi_id[1], msd.ddi_id[2], msd.ddi_id[3], msd.ddi_id[4]);
	strlcat(buf, temp, 30);

	pr_info("%02x%02x%02x%02x%02x\n",
		msd.ddi_id[0], msd.ddi_id[1], msd.ddi_id[2], msd.ddi_id[3], msd.ddi_id[4]);

	return strnlen(buf, 30);
}

#if defined(TEST_RESOLUTION)
static ssize_t mipi_samsung_disp_panel_res_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char temp[100];

/*	{"samsung amoled 1080p command mode dsi S6E3FA2 panel",PANEL_FHD_OCTA_S6E3FA2_CMD },*/
/*	{"samsung amoled wqhd command mode dsi1 S6E3HA0 panel", PANEL_WQHD_OCTA_S6E3HA0_CMD },*/

	switch (msd.panel) {
		case PANEL_FHD_OCTA_S6E3FA2_CMD:
			snprintf(temp, 10, "FHD\n");
			break;
		case PANEL_WQHD_OCTA_S6E3HA0_CMD:
			snprintf(temp, 10, "WQHD\n");
			break;
		default :
			snprintf(temp, 10, "WQHD\n");
			break;
	}
	strlcat(buf, temp, 100);

	return strnlen(buf, 100);

}
#endif

static ssize_t mipi_samsung_disp_acl_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, 30, "%d\n", msd.dstat.acl_on);
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

	msd.dstat.elvss_need_update = 1;

	if (mdss_fb_is_power_on(mfd)) {
		if (acl_set && !(msd.dstat.acl_on||msd.dstat.siop_status)) {
			msd.dstat.acl_on = true;
			pr_info("%s: acl on  : acl %d, siop %d", __func__,
					msd.dstat.acl_on, msd.dstat.siop_status);
			mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);

		} else if (!acl_set && msd.dstat.acl_on && !msd.dstat.siop_status) {
			msd.dstat.acl_on = false;
			msd.dstat.curr_acl_idx = -1;
			msd.dstat.curr_opr_idx = -1;
			pr_info("%s: acl off : acl %d, siop %d", __func__,
					msd.dstat.acl_on, msd.dstat.siop_status);
			if(msd.dstat.auto_brightness == 6)
				pr_info("%s: HBM mode No ACL off!!", __func__);
#ifdef SMART_ACL
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

static ssize_t mipi_samsung_disp_siop_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, 4, "%d\n", msd.dstat.siop_status);
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

	if (mdss_fb_is_power_on(mfd)) {
		if (siop_set && !(msd.dstat.acl_on||msd.dstat.siop_status)) {
			msd.dstat.siop_status = true;
			mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);
			pr_info("%s: acl on  : acl %d, siop %d", __func__,
				msd.dstat.acl_on, msd.dstat.siop_status);

		} else if (!siop_set && !msd.dstat.acl_on && msd.dstat.siop_status) {
			mutex_lock(&msd.lock);
			msd.dstat.siop_status = false;
			msd.dstat.curr_acl_idx = -1;
			msd.dstat.curr_opr_idx = -1;
			if(msd.dstat.auto_brightness == 6)
				pr_info("%s: HBM mode No ACL off!!", __func__);
#ifdef SMART_ACL
			/* If SMART_ACL enabled, elvss table shoud be set again */
			mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, false);
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

static ssize_t mipi_samsung_aid_log_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc = 0;

	if (msd.dstat.is_smart_dim_loaded)
		msd.sdimconf->print_aid_log();
	else
		pr_err("smart dim is not loaded..\n");

#if defined(CONFIG_LCD_HMT)
	if (msd.dstat.is_hmt_smart_dim_loaded) {
		msd.sdimconf_reverse_hmt_single->print_aid_log();
	} else
		pr_err("smart dim for HMT is not loaded..\n");
#endif
	return rc;
}

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
static ssize_t mipi_samsung_auto_brightness_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, 30, "%d\n",
					msd.dstat.auto_brightness);
	pr_info("auto_brightness: %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_auto_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	static int first_auto_br = 0;

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

	if (!first_auto_br) {
		pr_info("%s : skip first auto brightness store (%d) (%d)!!\n",
				__func__, msd.dstat.auto_brightness, msd.dstat.bright_level);
		first_auto_br++;
		return size;
	}

	msd.dstat.elvss_need_update = 1;

	if (msd.mfd->resume_state == MIPI_RESUME_STATE) {
		mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);
		mDNIe_Set_Mode(); // LOCAL CE tuning
		pr_info("%s %d %d\n", __func__, msd.dstat.auto_brightness, msd.dstat.bright_level);
	} else {
		pr_info("%s : panel is off state!!\n", __func__);
	}

	return size;
}

static ssize_t mipi_samsung_read_mtp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int addr, len, start;
	char *read_buf = NULL;

	sscanf(buf, "%x %d %x" , &addr, &len, &start);

	read_buf = kmalloc(len * sizeof(char), GFP_KERNEL);

	pr_info("%x %d %x\n", addr, len, start);

	mtp_read_sysfs_cmds.cmd_desc[0].payload[0] = addr; // addr
	mtp_read_sysfs_cmds.cmd_desc[0].payload[1] = len; // size
	mtp_read_sysfs_cmds.cmd_desc[0].payload[2] = start; // start

	mtp_read_sysfs_cmds.read_size = kzalloc(sizeof(char) * \
					mtp_read_sysfs_cmds.num_of_cmds, GFP_KERNEL);
	mtp_read_sysfs_cmds.read_startoffset = kzalloc(sizeof(char) * \
					mtp_read_sysfs_cmds.num_of_cmds, GFP_KERNEL);

	mtp_read_sysfs_cmds.read_size[0] = len;
	mtp_read_sysfs_cmds.read_startoffset[0] = start;

	pr_info("%x %x %x %x %x %x %x %x %x\n",
		mtp_read_sysfs_cmds.cmd_desc[0].dchdr.dtype,
		mtp_read_sysfs_cmds.cmd_desc[0].dchdr.last,
		mtp_read_sysfs_cmds.cmd_desc[0].dchdr.vc,
		mtp_read_sysfs_cmds.cmd_desc[0].dchdr.ack,
		mtp_read_sysfs_cmds.cmd_desc[0].dchdr.wait,
		mtp_read_sysfs_cmds.cmd_desc[0].dchdr.dlen,
		mtp_read_sysfs_cmds.cmd_desc[0].payload[0],
		mtp_read_sysfs_cmds.cmd_desc[0].payload[1],
		mtp_read_sysfs_cmds.cmd_desc[0].payload[2]);

	mipi_samsung_read_nv_mem(msd.pdata, &mtp_read_sysfs_cmds, read_buf);

	kfree(read_buf);
	return size;
}

#endif

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
#endif

#if defined(PARTIAL_UPDATE)
static ssize_t mipi_samsung_disp_partial_disp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, 40,"partial display range %d to %d \n", partial_disp_range[0], partial_disp_range[1]);

	pr_info("partial display range %d to %d \n", partial_disp_range[0], partial_disp_range[1]);

	return rc;
}

static ssize_t mipi_samsung_disp_partial_disp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	sscanf(buf, "%d %d" , &partial_disp_range[0], &partial_disp_range[1]);
	pr_info("%s: partial_disp range[0] = 0x%x, range[1] = 0x%x\n", __func__, partial_disp_range[0], partial_disp_range[1]);

	if(partial_disp_range[0] > msd.pdata->panel_info.yres-1
		|| partial_disp_range[1] > msd.pdata->panel_info.yres-1)
	{
		pr_err("%s:Invalid Input\n",__func__);
		return -EINVAL;
	}
	partialdisp_on_cmd.cmd_desc[0].payload[1] = (partial_disp_range[0] >> 8) & 0xFF;/*select msb 1byte*/
	partialdisp_on_cmd.cmd_desc[0].payload[2] = partial_disp_range[0] & 0xFF;
	partialdisp_on_cmd.cmd_desc[0].payload[3] = (partial_disp_range[1] >> 8) & 0xFF;/*select msb 1byte*/
	partialdisp_on_cmd.cmd_desc[0].payload[4] = partial_disp_range[1] & 0xFF;

	if (msd.dstat.on) {
		if (partial_disp_range[0] || partial_disp_range[1])
			mipi_samsung_disp_send_cmd(PANEL_PARTIAL_ON, true);
		else
			mipi_samsung_disp_send_cmd(PANEL_PARTIAL_OFF, true);
	} else {
		pr_info("%s : LCD is off state\n", __func__);
		return -EINVAL;
	}

	pr_info("%s: partialdisp_on_cmd = 0x%x\n", __func__, partialdisp_on_cmd.cmd_desc[0].payload[1]);
	pr_info("%s: partialdisp_on_cmd = 0x%x\n", __func__, partialdisp_on_cmd.cmd_desc[0].payload[2]);
	pr_info("%s: partialdisp_on_cmd = 0x%x\n", __func__, partialdisp_on_cmd.cmd_desc[0].payload[3]);
	pr_info("%s: partialdisp_on_cmd = 0x%x\n", __func__, partialdisp_on_cmd.cmd_desc[0].payload[4]);

	return size;
}
#endif

static ssize_t mipi_samsung_alpm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;
	static struct mdss_panel_alpm_data *alpm_data = NULL;
	u8 current_status = 0;

	if (unlikely(!alpm_data) && msd.pdata)
		alpm_data = &msd.pdata->alpm_data;

	if (likely(alpm_data) && alpm_data->alpm_status)
		current_status = alpm_data->alpm_status(CHECK_CURRENT_STATUS);

	rc = snprintf((char *)buf, 30, "%d\n", current_status);
	pr_info("[ALPM_DEBUG] %s: current status : %d \n",\
					 __func__, (int)current_status);

	return rc;
}

static ssize_t mipi_samsung_alpm_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int mode = 0;
	static struct mdss_panel_alpm_data *alpm_data = NULL;
	struct display_status *dstat = &msd.dstat;

	sscanf(buf, "%d" , &mode);
	pr_info("[ALPM_DEBUG] %s: mode : %d\n", __func__, mode);

	if (unlikely(!alpm_data) && msd.pdata)
		alpm_data = &msd.pdata->alpm_data;

	if (!alpm_data->alpm_status)
		return size;

	/*
	 * Possible mode status for Blank(0) or Unblank(1)
	 *	* Blank *
	 *		1) ALPM_MODE_ON
	 *			-> That will set during wakeup
	 *	* Unblank *
	 *		1) NORMAL_MODE_ON
	 *			-> That will send partial update commands
	 */
	alpm_data->alpm_status(mode ? ALPM_MODE_ON : MODE_OFF);
	if (mode == ALPM_MODE_ON) {
		/*
		 * This will work if the ALPM must be on or chagne partial area
		 * if that already in the status of unblank
		 */
		if (dstat->on) {
			if (!alpm_data->alpm_status(CHECK_PREVIOUS_STATUS)\
					&& alpm_data->alpm_status(CHECK_CURRENT_STATUS)) {
				/* Turn On ALPM Mode */
				mipi_samsung_disp_send_cmd(PANEL_ALPM_ON, true);
				if (dstat->wait_disp_on == 0) {
					msleep(20); /* wait 1 frame(more than 16ms) */
					mipi_samsung_disp_send_cmd(PANEL_DISPLAY_ON, true);
				}
				alpm_data->alpm_status(STORE_CURRENT_STATUS);
				pr_info("[ALPM_DEBUG] %s: Send ALPM mode on cmds\n", __func__);
			}
		}
	} else if (mode == MODE_OFF) {
		if (alpm_data->alpm_status(CHECK_PREVIOUS_STATUS) == ALPM_MODE_ON) {
				if (dstat->on) {
					mipi_samsung_disp_send_cmd(PANEL_ALPM_OFF, true);
				msleep(20); /* wait 1 frame(more than 16ms) */
					mipi_samsung_disp_send_cmd(PANEL_DISPLAY_ON, true);
				alpm_data->alpm_status(CLEAR_MODE_STATUS);
				}
				pr_info("[ALPM_DEBUG] %s: Send ALPM off cmds\n", __func__);
			}
	} else
		pr_info("[ALPM_DEBUG] %s: no operation \n:", __func__);

	return size;
}
#if defined(DYNAMIC_FPS_USE_TE_CTRL)
static ssize_t dynamic_fps_use_te_ctrl_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, 40,"dynamic_fps_use_te_ctrl %d \n", dynamic_fps_use_te_ctrl);

	pr_info("dynamic_fps_use_te_ctrl %d \n", dynamic_fps_use_te_ctrl);

	return rc;
}

static ssize_t dynamic_fps_use_te_ctrl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	sscanf(buf, "%d" , &dynamic_fps_use_te_ctrl);
	return size;
}
#endif

static struct lcd_ops mipi_samsung_disp_props = {
	.get_power = NULL,
	.set_power = NULL,
};

#ifdef CONFIG_FB_MSM_SAMSUNG_AMOLED_LOW_POWER_MODE
/*
 * This will use to enable/disable or check the status of ALPM
 * * Description for STATUS_OR_EVENT_FLAG *
 *	1) ALPM_MODE_ON
 *	2) NORMAL_MODE_ON
 *		-> Set by user using sysfs(/sys/class/lcd/panel/alpm)
 *			The value will save to current_status
 *	3) CHECK_CURRENT_STATUS
 *		-> Check current status
 *			that will return current status like ALPM_MODE_ON, NORMAL_MODE_ON or MODE_OFF
 *	4) CHECK_PREVIOUS_STATUS
 *		-> Check previous status that will return previous status like
 *			 ALPM_MODE_ON, NORMAL_MODE_ON or MODE_OFF
 *	5) STORE_CURRENT_STATUS
 *		-> Store current status to previous status because that will use
 *			for next turn on sequence
 *	6) CLEAR_MODE_STATUS
 *		-> Clear current and previous status as MODE_OFF status that can use with
 *	* Usage *
 *		Call function "mdss_dsi_panel_alpm_status_func(STATUS_FLAG)"
 */
u8 mdss_dsi_panel_alpm_status_func(u8 flag)
{
	static u8 current_status = 0;
	static u8 previous_status = 0;
	u8 ret = 0;

	switch (flag) {
		case ALPM_MODE_ON:
			current_status = ALPM_MODE_ON;
			break;
		case NORMAL_MODE_ON:
			/*current_status = NORMAL_MODE_ON;*/
			break;
		case MODE_OFF:
			current_status = MODE_OFF;
			break;
		case CHECK_CURRENT_STATUS:
			ret = current_status;
			break;
		case CHECK_PREVIOUS_STATUS:
			ret = previous_status;
			break;
		case STORE_CURRENT_STATUS:
			previous_status = current_status;
			break;
		case CLEAR_MODE_STATUS:
			previous_status = 0;
			current_status = 0;
			break;
		default:
			break;
	}

	pr_debug("[ALPM_DEBUG] current_status : %d, previous_status : %d, ret : %d\n",\
				 current_status, previous_status, ret);

	return ret;
}
#endif
void mdss_dsi_panel_alpm_register(struct mdss_panel_alpm_data *alpm_data)
{
	if (!alpm_data) {
		pr_err("%s: pdata is null\n", __func__);
		return;
	}
	alpm_data->alpm_status = mdss_dsi_panel_alpm_status_func;
}


#if defined(CONFIG_LCD_RECOVERY)
static irqreturn_t lcd_esd_handler(int irq, void *handle)
{
	struct msm_fb_data_type *mfd = msd.mfd;

	if(lcd_refresh_working || !(mdss_fb_is_power_on(mfd))|| mdss_fb_get_first_cmt_flag()) return IRQ_HANDLED;

	pr_info("%s + irq(%d) oled_det(%d)\n", __func__, irq, gpio_get_value(oled_det_gpio));
#if defined(CONFIG_LCD_RECOVERY_ERR_FG)
	pr_info(" err_fg(%d)\n", gpio_get_value(err_fg_gpio));
#endif

	lcd_refresh_working = 1;
	disable_irq_nosync(gpio_to_irq(oled_det_gpio));

#if defined(CONFIG_LCD_RECOVERY_ERR_FG)
	disable_irq_nosync(gpio_to_irq(err_fg_gpio));
#endif

	schedule_work(&lcd_refresh_work);

	pr_info("%s - oled_det(%d)\n", __func__, gpio_get_value(oled_det_gpio));
#if defined(CONFIG_LCD_RECOVERY_ERR_FG)
	pr_info(" err_fg(%d)\n", gpio_get_value(err_fg_gpio));
#endif
	return IRQ_HANDLED;
}

void lcd_refresh_work_func(struct work_struct *work)
{
	struct msm_fb_data_type *mfd = msd.mfd;
	struct mdss_panel_data *pdata = msd.pdata;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	char *envp[2] = {"PANEL_ALIVE=0", NULL};
	struct device *dev = msd.mfd->fbi->dev;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_info("%s+ oled_det(%d)\n", __func__, gpio_get_value(oled_det_gpio));
#if defined(CONFIG_LCD_RECOVERY_ERR_FG)
	pr_info("err_fg(%d)\n", gpio_get_value(err_fg_gpio));
#endif
	if(msd.mfd == NULL){
		pr_err("%s: mfd not initialized Skip ESD recovery\n", __func__);
		return;
	}

	if (mdss_fb_is_power_on(mfd)) {
		if (pdata->panel_info.type == MIPI_CMD_PANEL){
			if(mdss_dsi_sync_wait_enable(ctrl)){
				pdata_dsi0->panel_info.panel_dead = true;
				pdata_dsi1->panel_info.panel_dead = true;
			}else
				pdata->panel_info.panel_dead = true;
		}
		kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
		pr_err("Panel has gone bad, sending uevent - %s\n", envp[0]);
	}
	return;
}


#ifdef ESD_DEBUG
static ssize_t mipi_samsung_esd_check_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, 20, "lcd_refresh_count %d\n", lcd_refresh_count);

	return rc;
}
static ssize_t mipi_samsung_esd_check_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd = msd.mfd;

	lcd_esd_handler(0, mfd);
	return 1;
}

static DEVICE_ATTR(esd_check, /*S_IRUGO*/0664 , mipi_samsung_esd_check_show,\
			 mipi_samsung_esd_check_store);
#endif
#endif

#if defined(CONFIG_LCD_HMT)
int hmt_bright_update(void)
{
	msleep(20);

	if (msd.hmt_stat.hmt_on) {
		if (msd.hmt_stat.hmt_low_persistence)
			mipi_samsung_disp_send_cmd(PANEL_HMT_BRIGHT, true);
		else
			mipi_samsung_disp_send_cmd(PANEL_LOW_PERSISTENCE_BRIGHT, true);
	} else {
		mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);
	}

	return 0;
}

int hmt_enable(int enable)
{
	msleep(20);

	if (enable) {
		pr_info("Single Scan Enable ++ \n");
		mipi_samsung_disp_send_cmd(PANEL_ENABLE, true);
		pr_info("Single Scan Enable -- \n");
	} else {
		pr_info("HMT OFF.. \n");
		mipi_samsung_disp_send_cmd(PANEL_DISABLE, true);
	}

	return 0;
}

int hmt_reverse_update(int enable)
{
	msleep(20);

	if (enable) {
		pr_info("REVERSE ENABLE ++\n");
		mipi_samsung_disp_send_cmd(PANEL_HMT_REVERSE_ENABLE, true);
		pr_info("REVERSE ENABLE --\n");
	} else {
		pr_info("REVERSE DISABLE ++ \n");
		mipi_samsung_disp_send_cmd(PANEL_HMT_REVERSE_DISABLE, true);
		pr_info("REVERSE DISABLE -- \n");
	}

	return 0;
}

static ssize_t mipi_samsung_hmt_bright_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, 30, "%d\n", msd.hmt_stat.hmt_bl_level);
	pr_info("[HMT] hmt bright : %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_hmt_bright_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int input;

	sscanf(buf, "%d" , &input);
	pr_info("[HMT] %s: input (%d) ++ \n", __func__, input);

	if (!msd.hmt_stat.hmt_on) {
		pr_info("[HMT] hmt is off!\n");
		return size;
	}

	if (!msd.dstat.on) {
		pr_err("[HMT] panel is off!\n");
		msd.hmt_stat.hmt_bl_level = input;
		return size;
	}

	if (msd.hmt_stat.hmt_bl_level == input) {
		pr_err("[HMT] hmt bright already %d!\n", msd.hmt_stat.hmt_bl_level);
		return size;
	}

	msd.hmt_stat.hmt_bl_level = input;
	hmt_bright_update();

	pr_info("[HMT] %s: input (%d) -- \n", __func__, input);

	return size;
}

static ssize_t mipi_samsung_hmt_on_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, 30, "%d\n", msd.hmt_stat.hmt_on);
	pr_info("[HMT] hmt on input : %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_hmt_on_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int input;

	sscanf(buf, "%d" , &input);
	pr_info("[HMT] %s: input (%d) ++ \n", __func__, input);

	if (!msd.dstat.on) {
		pr_err("[HMT] panel is off!\n");
		msd.hmt_stat.hmt_on = input;
		return size;
	}

	if (msd.hmt_stat.hmt_on == input) {
		pr_info("[HMT] hmt already %s !\n", msd.hmt_stat.hmt_on?"ON":"OFF");
		return size;
	}

	msd.hmt_stat.hmt_on = input;

	if (msd.hmt_stat.hmt_on && msd.hmt_stat.hmt_low_persistence) {
		hmt_enable(1);
		hmt_reverse_update(1);
	} else {
		hmt_enable(0);
		hmt_reverse_update(0);
	}

	hmt_bright_update();

	pr_info("[HMT] %s: input hmt (%d) hmt lp (%d)-- \n",
		__func__, input, msd.hmt_stat.hmt_low_persistence);

	return size;
}

static ssize_t mipi_samsung_hmt_low_persistence_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, 30, "%d\n", msd.hmt_stat.hmt_low_persistence);
	pr_info("[HMT] hmt low persistence : %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_hmt_low_persistence_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int input;

	sscanf(buf, "%d" , &input);
	pr_info("[HMT] %s: input (%d) ++ \n", __func__, input);

	if (!msd.hmt_stat.hmt_on) {
		pr_info("[HMT] hmt is off!\n");
		return size;
	}

	if (!msd.dstat.on) {
		pr_err("[HMT] panel is off!\n");
		msd.hmt_stat.hmt_low_persistence = input;
		return size;
	}

	msd.hmt_stat.hmt_low_persistence = input;

	if (!msd.hmt_stat.hmt_low_persistence) {
		hmt_enable(0);
		hmt_reverse_update(0);
	} else {
		hmt_enable(1);
		hmt_reverse_update(1);
	}

	hmt_bright_update();

	pr_info("[HMT] %s: input hmt (%d) hmt lp (%d)-- \n",
		__func__, msd.hmt_stat.hmt_on, msd.hmt_stat.hmt_low_persistence);

	return size;
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
static DEVICE_ATTR(manufacture_code, S_IRUGO,
			mipi_samsung_disp_manufacture_code_show, NULL);
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
#if defined(PARTIAL_UPDATE)
static DEVICE_ATTR(partial_disp, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_disp_partial_disp_show,
			mipi_samsung_disp_partial_disp_store);
#endif
static DEVICE_ATTR(alpm, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_alpm_show,
			mipi_samsung_alpm_store);
#if defined(DYNAMIC_FPS_USE_TE_CTRL)
static DEVICE_ATTR(dynamic_fps_use_te, S_IRUGO | S_IWUSR | S_IWGRP,
			dynamic_fps_use_te_ctrl_show,
			dynamic_fps_use_te_ctrl_store);
#endif
#if defined(TEST_RESOLUTION)
static DEVICE_ATTR(panel_res, S_IRUGO,
			mipi_samsung_disp_panel_res_show,
			NULL);
#endif
#if defined(CONFIG_LCD_HMT)
static DEVICE_ATTR(hmt_bright, 660,
			mipi_samsung_hmt_bright_show,
			mipi_samsung_hmt_bright_store);
static DEVICE_ATTR(hmt_on, 660,
			mipi_samsung_hmt_on_show,
			mipi_samsung_hmt_on_store);
static DEVICE_ATTR(hmt_low_persistence, 660,
			mipi_samsung_hmt_low_persistence_show,
			mipi_samsung_hmt_low_persistence_store);
#endif

static struct attribute *panel_sysfs_attributes[] = {
	&dev_attr_lcd_power.attr,
	&dev_attr_lcd_type.attr,
	&dev_attr_window_type.attr,
	&dev_attr_manufacture_date.attr,
	&dev_attr_manufacture_code.attr,
	&dev_attr_power_reduce.attr,
	&dev_attr_siop_enable.attr,
	&dev_attr_aid_log.attr,
	&dev_attr_read_mtp.attr,
#if defined(TEMPERATURE_ELVSS)
	&dev_attr_temperature.attr,
#endif
#if defined(PARTIAL_UPDATE)
	&dev_attr_partial_disp.attr,
#endif
	&dev_attr_alpm.attr,
#if defined(DYNAMIC_FPS_USE_TE_CTRL)
	&dev_attr_dynamic_fps_use_te.attr,
#endif
#if defined(TEST_RESOLUTION)
	&dev_attr_panel_res.attr,
#endif
#if defined(CONFIG_LCD_HMT)
	&dev_attr_hmt_bright.attr,
	&dev_attr_hmt_on.attr,
	&dev_attr_hmt_low_persistence.attr,
#endif
	NULL
};
static const struct attribute_group panel_sysfs_group = {
	.attrs = panel_sysfs_attributes,
};

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
static DEVICE_ATTR(auto_brightness, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_auto_brightness_show,
			mipi_samsung_auto_brightness_store);

static struct attribute *bl_sysfs_attributes[] = {
	&dev_attr_auto_brightness.attr,
	NULL
};
static const struct attribute_group bl_sysfs_group = {
	.attrs = bl_sysfs_attributes,
};
#endif
static int sysfs_enable;
static int mdss_samsung_create_sysfs(void)
{
	int rc = 0;

	struct lcd_device *lcd_device;
#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	struct backlight_device *bd = NULL;
#endif

	/* sysfs creat func should be called one time in dual dsi mode */
	if (sysfs_enable)
		return 0;

	lcd_device = lcd_device_register("panel", NULL, NULL,
					&mipi_samsung_disp_props);

	if (IS_ERR(lcd_device)) {
		rc = PTR_ERR(lcd_device);
		pr_err("Failed to register lcd device..\n");
		return rc;
	}

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

	rc = sysfs_create_group(&bd->dev.kobj, &bl_sysfs_group);
	if (rc) {
		pr_err("Failed to create backlight sysfs group..\n");
		sysfs_remove_group(&bd->dev.kobj, &bl_sysfs_group);
		return rc;
	}
#endif

#if defined(CONFIG_LCD_RECOVERY)
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

	pr_info("%s: done!! \n", __func__);

	return rc;
}
#endif

int mdss_dsi_panel_init(struct device_node *node, struct mdss_dsi_ctrl_pdata *ctrl_pdata,
				bool cmd_cfg_cont_splash)
{
	int rc = 0;
	static const char *panel_name;
	bool cont_splash_enabled;
	bool partial_update_enabled;

	pr_debug("%s: ++ \n", __func__);

	if (!node)
		return -ENODEV;

	panel_name = of_get_property(node, "label", NULL);
	if (!panel_name)
		pr_info("%s:%d, panel name not specified\n",
						__func__, __LINE__);
	else
		pr_info("%s: Panel Name = %s\n", __func__, panel_name);

	if (is_panel_supported(panel_name))
		LCD_DEBUG("Panel : %s is not supported:",panel_name);

	rc = mdss_panel_parse_dt(node, ctrl_pdata);
	if (rc)
		return rc;

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

#if defined(CONFIG_LCD_RECOVERY)
	if (ctrl_pdata->panel_data.panel_info.pdest == DISPLAY_1 && esd_enable) {

		INIT_WORK(&lcd_refresh_work, lcd_refresh_work_func);

		rc = request_threaded_irq(gpio_to_irq(oled_det_gpio),
			NULL, lcd_esd_handler, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "oled_detect", NULL);
		if (rc) {
			pr_err("%s : Failed to request_irq.:ret=%d", __func__, rc);
		}
		disable_irq(gpio_to_irq(oled_det_gpio));

#if defined(CONFIG_LCD_RECOVERY_ERR_FG)
		rc = request_threaded_irq(gpio_to_irq(err_fg_gpio),
			NULL, lcd_esd_handler, IRQF_TRIGGER_RISING | IRQF_ONESHOT, "err_fg_detect", NULL);
		if (rc) {
			pr_err("%s : Failed to request_irq.:ret=%d", __func__, rc);
		}
		disable_irq(gpio_to_irq(err_fg_gpio));
#endif
	}
#endif

	ctrl_pdata->on = mdss_dsi_panel_on;
	ctrl_pdata->off = mdss_dsi_panel_off;
	ctrl_pdata->low_power_config = mdss_dsi_panel_low_power_config;
	ctrl_pdata->event_handler = samsung_dsi_panel_event_handler;
	ctrl_pdata->bl_fnc = mdss_dsi_panel_bl_ctrl;
	ctrl_pdata->panel_reset = mdss_dsi_samsung_panel_reset;
	ctrl_pdata->registered = mdss_dsi_panel_registered;
	ctrl_pdata->dimming_init = mdss_dsi_panel_dimming_init;
	ctrl_pdata->panel_blank = mdss_dsi_panel_blank;
	ctrl_pdata->bklt_ctrl = ctrl_pdata->panel_data.panel_info.bklt_ctrl;
	ctrl_pdata->panel_data.set_backlight = mdss_dsi_panel_bl_ctrl;
	ctrl_pdata->panel_data.alpm_data.alpm_register =\
							mdss_dsi_panel_alpm_register;

	mutex_init(&msd.lock);
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FHD_FA2_PT_PANEL)
	init_completion(&te_check_comp);
#endif

	msd.dstat.on = 0;
	msd.dstat.recent_bright_level = 255;

#if 1
	partial_update_enabled =of_property_read_bool(node,"qcom,partial-update-enabled");
#else
	partial_update_enabled = 0;
#endif
	if (partial_update_enabled) {
		pr_info("%s:%d Partial update enabled.\n", __func__, __LINE__);
		ctrl_pdata->panel_data.panel_info.partial_update_enabled = 1;

		if(msd.panel == PANEL_FHD_OCTA_S6E3FA2_CMD) // temp for K FHD
			ctrl_pdata->set_col_page_addr = NULL;
		else
			ctrl_pdata->set_col_page_addr = mdss_dsi_set_col_page_addr;

		ctrl_pdata->panel_data.panel_info.dcs_cmd_by_left =
					of_property_read_bool(node, "qcom,dcs-cmd-by-left");
		ctrl_pdata->panel_data.panel_info.partial_update_roi_merge =
					of_property_read_bool(node, "qcom,partial-update-roi-merge");
	} else {
		pr_info("%s:%d Partial update disabled.\n", __func__, __LINE__);
		ctrl_pdata->panel_data.panel_info.partial_update_enabled = 0;
		ctrl_pdata->panel_data.panel_info.dcs_cmd_by_left = 0;
		ctrl_pdata->panel_data.panel_info.partial_update_roi_merge = 0;
		ctrl_pdata->set_col_page_addr = NULL;
	}
#if defined(CONFIG_LCD_CLASS_DEVICE)
	rc = mdss_samsung_create_sysfs();
	if (rc) {
		pr_err("Failed to create sysfs for lcd driver..\n");
		return rc;
	}
#endif

#if defined(CONFIG_MDNIE_LITE_TUNING)
	pr_info("[%s] CONFIG_MDNIE_LITE_TUNING ok ! init class called!\n",
		__func__);
	init_mdnie_class();
#endif

#if defined(CONFIG_HAS_EARLYSUSPEND)
	msd.early_suspend.suspend = mipi_samsung_disp_early_suspend;
	msd.early_suspend.resume = mipi_samsung_disp_late_resume;
	msd.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN-1;
	register_early_suspend(&msd.early_suspend);
#endif

	pr_debug("%s : --\n",__func__);

	return 0;
}

int get_lcd_id(void)
{
	return lcd_id;
}
EXPORT_SYMBOL(get_lcd_id);

int get_samsung_lcd_attached(void)
{

	return lcd_attached;

}
EXPORT_SYMBOL(get_samsung_lcd_attached);

static int __init get_lcd_id_cmdline(char *mode)
{
	char *pt;

	lcd_id = 0;
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

	pr_info( "%s: LCD_ID = 0x%X, lcd_attached =%d", __func__,lcd_id, lcd_attached);

	return 0;
}

__setup( "lcd_id=0x", get_lcd_id_cmdline );

MODULE_DESCRIPTION("Samsung DSI panel driver");
MODULE_AUTHOR("Krishna Kishor Jha <krishna.jha@samsung.com>");
MODULE_LICENSE("GPL");
