/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/ctype.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fb.h>
#include <linux/msm_mdp.h>
#include <linux/ioctl.h>
#include <linux/lcd.h>
#include <asm/system_info.h>

#include "mdss_fb.h"
#include "mdss_panel.h"
#include "mdss_dsi.h"
#include "mdnie_lite_tuning.h"

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FULL_HD_PT_PANEL) // H
#include "mdnie_lite_tuning_data_hlte.h"
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_VIDEO_FULL_HD_PT_PANEL) // KS01
#include "mdnie_lite_tuning_data.h"
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_YOUM_CMD_FULL_HD_PT_PANEL) // F
#include "mdnie_lite_tuning_data_flte.h"
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_WQHD_PT_PANEL)
#include "mdnie_lite_tuning_data_klte_wqhd_s6e3ha0.h"
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FHD_FA2_PT_PANEL) // K cat6
#include "mdnie_lite_tuning_data_klte_fhd_s6e3fa2.h"
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQHD_PT_PANEL)|| defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQXGA_PT_PANEL)
#include "mdnie_lite_tuning_data_trlte_wqhd_s6e3ha2.h"
#else
#include "mdnie_lite_tuning_data.h"
#endif

#if defined(CONFIG_TDMB)
#include "mdnie_lite_tuning_data_dmb.h"
#endif

static struct mipi_samsung_driver_data *mdnie_msd;


#define MDNIE_LITE_TUN_DEBUG

#ifdef MDNIE_LITE_TUN_DEBUG
#define DPRINT(x...)	printk(KERN_ERR "[mdnie lite] " x)
#else
#define DPRINT(x...)
#endif

#define MAX_LUT_SIZE	256

/*#define MDNIE_LITE_TUN_DATA_DEBUG*/

#define PAYLOAD1 mdni_tune_cmd[3]
#define PAYLOAD2 mdni_tune_cmd[2]

#define INPUT_PAYLOAD1(x) PAYLOAD1.payload = x
#define INPUT_PAYLOAD2(x) PAYLOAD2.payload = x

int play_speed_1_5;

struct dsi_buf dsi_mdnie_tx_buf;

#if defined(CONFIG_LCD_CLASS_DEVICE) && defined(DDI_VIDEO_ENHANCE_TUNING)
extern int mdnie_adb_test;
#endif

struct mdnie_lite_tun_type mdnie_tun_state = {
	.mdnie_enable = false,
	.scenario = mDNIe_UI_MODE,
	.background = AUTO_MODE,
	.outdoor = OUTDOOR_OFF_MODE,
	.accessibility = ACCESSIBILITY_OFF,
#if defined(CONFIG_TDMB)
	.dmb = DMB_MODE_OFF,
#endif
#if defined(CONFIG_LCD_HMT)
	.hmt_color_temperature = HMT_COLOR_TEMP_OFF,
#endif
#if defined(SUPPORT_WHITE_RGB)
	.scr_white_red = 0xff,
	.scr_white_green = 0xff,
	.scr_white_blue = 0xff,
#endif
};

const char scenario_name[MAX_mDNIe_MODE][16] = {
	"UI_MODE",
	"VIDEO_MODE",
	"VIDEO_WARM_MODE",
	"VIDEO_COLD_MODE",
	"CAMERA_MODE",
	"NAVI",
	"GALLERY_MODE",
	"VT_MODE",
	"BROWSER",
	"eBOOK",
	"EMAIL",
#if defined(CONFIG_LCD_HMT)
	"HMT_8_235",
	"HMT_8_255",
	"HMT_0_235",
	"HMT_0_255",
	"HMT_8_235_2",
#endif
#if defined(CONFIG_TDMB)
	"DMB_MODE",
	"DMB_WARM_MODE",
	"DMB_COLD_MODE",
#endif
};

const char background_name[MAX_BACKGROUND_MODE][10] = {
	"DYNAMIC",
	"STANDARD",
	"NATURAL",
	"MOVIE",
	"AUTO",
};

const char outdoor_name[MAX_OUTDOOR_MODE][20] = {
	"OUTDOOR_OFF_MODE",
	"OUTDOOR_ON_MODE",
};

const char accessibility_name[ACCESSIBILITY_MAX][20] = {
	"ACCESSIBILITY_OFF",
	"NEGATIVE_MODE",
	"COLOR_BLIND_MODE",
	"SCREEN_CURTAIN_MODE",
};


static char level1_key[] = {
	0xF0,
	0x5A, 0x5A,
};

static char level2_key[] = {
	0xF1,
	0x5A, 0x5A,
};
#if defined(SUPPORT_WHITE_RGB)
static char white_rgb_buf[MDNIE_TUNE_BODY_SIZE] = {0,};
#endif
static char tune_body[MDNIE_TUNE_BODY_SIZE] = {0,};
static char tune_head[MDNIE_TUNE_HEAD_SIZE] = {0,};

static char tune_body_adb[MDNIE_TUNE_BODY_SIZE] = {0,};
static char tune_head_adb[MDNIE_TUNE_HEAD_SIZE] = {0,};

void copy_tuning_data_from_adb(char *body, char *head)
{
	memcpy(tune_body_adb, body, MDNIE_TUNE_BODY_SIZE);
	memcpy(tune_head_adb, head, MDNIE_TUNE_HEAD_SIZE);
}

static struct dsi_cmd_desc mdni_tune_cmd[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(level1_key)}, level1_key},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(level2_key)}, level2_key},

	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(tune_body)}, tune_body},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(tune_head)}, tune_head},
};

void print_tun_data(void)
{
	int i;

	DPRINT("\n");
	DPRINT("---- size1 : %d", PAYLOAD1.dchdr.dlen);
	for (i = 0; i < MDNIE_TUNE_HEAD_SIZE ; i++)
		DPRINT("0x%x ", PAYLOAD1.payload[i]);
	DPRINT("\n");
	DPRINT("---- size2 : %d", PAYLOAD2.dchdr.dlen);
	for (i = 0; i < MDNIE_TUNE_BODY_SIZE ; i++)
		DPRINT("0x%x ", PAYLOAD2.payload[i]);
	DPRINT("\n");
}

void free_tun_cmd(void)
{
	memset(tune_body, 0, MDNIE_TUNE_BODY_SIZE);
	memset(tune_head, 0, MDNIE_TUNE_HEAD_SIZE);
}

void sending_tuning_cmd(void)
{
	struct msm_fb_data_type *mfd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;

	mfd = mdnie_msd->mfd;
	ctrl_pdata = mdnie_msd->ctrl_pdata;

	if (mfd->resume_state == MIPI_SUSPEND_STATE) {
		DPRINT("[ERROR] not ST_DSI_RESUME. do not send mipi cmd.\n");
		return;
	}

#if defined(CONFIG_LCD_CLASS_DEVICE) && defined(DDI_VIDEO_ENHANCE_TUNING)
	if (mdnie_adb_test) {
		DPRINT("[ERROR] mdnie_adb_test is doning .. copy from adb data .. \n");
		INPUT_PAYLOAD1(tune_head_adb);
		INPUT_PAYLOAD2(tune_body_adb);
	}
#endif

	mutex_lock(&mdnie_msd->lock);

#ifdef MDNIE_LITE_TUN_DATA_DEBUG
		print_tun_data();
#else
		DPRINT(" send tuning cmd!!\n");
#endif
		mdss_dsi_cmds_send(ctrl_pdata, mdni_tune_cmd, ARRAY_SIZE(mdni_tune_cmd),0);

		mutex_unlock(&mdnie_msd->lock);
	}

void mDNIe_Set_Mode(void)
{
	struct msm_fb_data_type *mfd;
	mfd = mdnie_msd->mfd;

#if defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQHD_PT_PANEL)|| defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQXGA_PT_PANEL)
		if (system_rev <  2)
			return;
#endif

	DPRINT("mDNIe_Set_Mode start\n");

	if (!mfd) {
		DPRINT("[ERROR] mfd is null!\n");
		return;
	}

	if (mfd->resume_state == MIPI_SUSPEND_STATE) {
		DPRINT("[ERROR] not ST_DSI_RESUME. do not send mipi cmd.\n");
		return;
	}

	if (!mdnie_tun_state.mdnie_enable) {
		DPRINT("[ERROR] mDNIE engine is OFF.\n");
		return;
	}

	if (mdnie_tun_state.scenario < mDNIe_UI_MODE || mdnie_tun_state.scenario >= MAX_mDNIe_MODE) {
		DPRINT("[ERROR] wrong Scenario mode value : %d\n",
			mdnie_tun_state.scenario);
		return;
	}

	play_speed_1_5 = 0;

	if (mdnie_tun_state.accessibility) {
		DPRINT(" = ACCESSIBILITY MODE =\n");
		INPUT_PAYLOAD1(blind_tune_value[mdnie_tun_state.accessibility][0]);
		INPUT_PAYLOAD2(blind_tune_value[mdnie_tun_state.accessibility][1]);
	}
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_WQHD_PT_PANEL) || \
	defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FHD_FA2_PT_PANEL) || \
	defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQHD_PT_PANEL)|| \
	defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQXGA_PT_PANEL)
	else if (mdnie_msd->dstat.auto_brightness == 6) {
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQHD_PT_PANEL)|| \
	defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQXGA_PT_PANEL)
		if((mdnie_tun_state.scenario == mDNIe_BROWSER_MODE) || (mdnie_tun_state.scenario == mDNIe_eBOOK_MODE)) {
			INPUT_PAYLOAD1(LOCAL_CE_1_TEXT);
			INPUT_PAYLOAD2(LOCAL_CE_2_TEXT);
		}
		else
#endif
		{
			DPRINT("[LOCAL CE] HBM mode! only LOCAL CE tuning\n");
			INPUT_PAYLOAD1(LOCAL_CE_1);
			INPUT_PAYLOAD2(LOCAL_CE_2);
		}
	}
#endif
#if defined(CONFIG_LCD_HMT)
	else if (mdnie_tun_state.hmt_color_temperature) {
		DPRINT(" = HMT Color Temperature =\n");
		INPUT_PAYLOAD1(hmt_color_temperature_tune_value[mdnie_tun_state.hmt_color_temperature][0]);
		INPUT_PAYLOAD2(hmt_color_temperature_tune_value[mdnie_tun_state.hmt_color_temperature][1]);
	}
#endif
#if defined(CONFIG_TDMB)
	else if (mdnie_tun_state.dmb > DMB_MODE_OFF){
		if (!dmb_tune_value[mdnie_tun_state.dmb][mdnie_tun_state.background][mdnie_tun_state.outdoor][0] ||
			!dmb_tune_value[mdnie_tun_state.dmb][mdnie_tun_state.background][mdnie_tun_state.outdoor][1]) {
			pr_err("dmb tune data is NULL!\n");
			return;
		} else {
			INPUT_PAYLOAD1(
				dmb_tune_value[mdnie_tun_state.dmb][mdnie_tun_state.background][mdnie_tun_state.outdoor][0]);
			INPUT_PAYLOAD2(
				dmb_tune_value[mdnie_tun_state.dmb][mdnie_tun_state.background][mdnie_tun_state.outdoor][1]);
		}
	}
#endif
	else if (!mdnie_tune_value[mdnie_tun_state.scenario][mdnie_tun_state.background][mdnie_tun_state.outdoor][0] ||
			!mdnie_tune_value[mdnie_tun_state.scenario][mdnie_tun_state.background][mdnie_tun_state.outdoor][1]) {
			pr_err("mdnie tune data is NULL!\n");
			return;
	} else {
		INPUT_PAYLOAD1(
			mdnie_tune_value[mdnie_tun_state.scenario][mdnie_tun_state.background][mdnie_tun_state.outdoor][0]);
		INPUT_PAYLOAD2(
			mdnie_tune_value[mdnie_tun_state.scenario][mdnie_tun_state.background][mdnie_tun_state.outdoor][1]);
#if defined(SUPPORT_WHITE_RGB)
		mdnie_tun_state.scr_white_red = mdnie_tune_value[mdnie_tun_state.scenario][mdnie_tun_state.background][mdnie_tun_state.outdoor][1][ADDRESS_SCR_WHITE_RED];
		mdnie_tun_state.scr_white_green = mdnie_tune_value[mdnie_tun_state.scenario][mdnie_tun_state.background][mdnie_tun_state.outdoor][1][ADDRESS_SCR_WHITE_GREEN];
		mdnie_tun_state.scr_white_blue= mdnie_tune_value[mdnie_tun_state.scenario][mdnie_tun_state.background][mdnie_tun_state.outdoor][1][ADDRESS_SCR_WHITE_BLUE];
#endif
	}
	sending_tuning_cmd();
	free_tun_cmd();

	DPRINT("mDNIe_Set_Mode end , %s(%d), %s(%d), %s(%d), %s(%d)\n",
		scenario_name[mdnie_tun_state.scenario], mdnie_tun_state.scenario,
		background_name[mdnie_tun_state.background], mdnie_tun_state.background,
		outdoor_name[mdnie_tun_state.outdoor], mdnie_tun_state.outdoor,
		accessibility_name[mdnie_tun_state.accessibility], mdnie_tun_state.accessibility);

}

void is_play_speed_1_5(int enable)
{
	play_speed_1_5 = enable;
}

/* ##########################################################
 * #
 * # MDNIE BG Sysfs node
 * #
 * ##########################################################*/

/* ##########################################################
 * #
 * #	0. Dynamic
 * #	1. Standard
 * #	2. Video
 * #	3. Natural
 * #
 * ##########################################################*/

static ssize_t mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	DPRINT("Current Background Mode : %s\n",
		background_name[mdnie_tun_state.background]);

	return snprintf(buf, 256, "Current Background Mode : %s\n",
		background_name[mdnie_tun_state.background]);
}

static ssize_t mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int value;
	int backup;

	sscanf(buf, "%d", &value);

	if (value < DYNAMIC_MODE || value >= MAX_BACKGROUND_MODE) {
		DPRINT("[ERROR] wrong backgound mode value : %d\n",
			value);
		return size;
	}
	backup = mdnie_tun_state.background;
	mdnie_tun_state.background = value;

	if (mdnie_tun_state.accessibility == NEGATIVE) {
		DPRINT("already negative mode(%d), do not set background(%d)\n",
			mdnie_tun_state.accessibility, mdnie_tun_state.background);
	} else {
		DPRINT(" %s : (%s) -> (%s)\n",
			__func__, background_name[backup], background_name[mdnie_tun_state.background]);

		mDNIe_Set_Mode();
	}

	return size;
}

static DEVICE_ATTR(mode, 0664, mode_show, mode_store);

static ssize_t scenario_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	DPRINT("Current Scenario Mode : %s\n",
		scenario_name[mdnie_tun_state.scenario]);

	return snprintf(buf, 256, "Current Scenario Mode : %s\n",
		scenario_name[mdnie_tun_state.scenario]);
}

static ssize_t scenario_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	int value;
	int backup;

	sscanf(buf, "%d", &value);

	if (value < mDNIe_UI_MODE || value >= MAX_mDNIe_MODE) {
		DPRINT("[ERROR] wrong Scenario mode value : %d\n",
			value);
		return size;
	}

	backup = mdnie_tun_state.scenario;
	mdnie_tun_state.scenario = value;

#if defined(CONFIG_TDMB)
	/* mDNIe_DMB_MODE = 20 */
	if ((value > DMB_MODE_OFF) && (value < MAX_DMB_MODE)) {
		DPRINT("DMB scenario.. (%d)\n", mdnie_tun_state.scenario);
		mdnie_tun_state.dmb = value - mDNIe_DMB_MODE;
	} else
		mdnie_tun_state.dmb = DMB_MODE_OFF;
#endif

	if (mdnie_tun_state.accessibility == NEGATIVE) {
		DPRINT("already negative mode(%d), do not set mode(%d)\n",
			mdnie_tun_state.accessibility, mdnie_tun_state.scenario);
	} else {
		DPRINT(" %s : (%s) -> (%s)\n",
			__func__, scenario_name[backup], scenario_name[mdnie_tun_state.scenario]);
		mDNIe_Set_Mode();
	}
	return size;
}
static DEVICE_ATTR(scenario, 0664, scenario_show,
		   scenario_store);

static ssize_t mdnieset_user_select_file_cmd_show(struct device *dev,
						  struct device_attribute *attr,
						  char *buf)
{
	int mdnie_ui = 0;
	DPRINT("called %s\n", __func__);

	return snprintf(buf, 256, "%u\n", mdnie_ui);
}

static ssize_t mdnieset_user_select_file_cmd_store(struct device *dev,
						   struct device_attribute
						   *attr, const char *buf,
						   size_t size)
{
	int value;

	sscanf(buf, "%d", &value);
	DPRINT
	("inmdnieset_user_select_file_cmd_store, input value = %d\n",
	     value);

	return size;
}

static DEVICE_ATTR(mdnieset_user_select_file_cmd, 0664,
		   mdnieset_user_select_file_cmd_show,
		   mdnieset_user_select_file_cmd_store);

static ssize_t mdnieset_init_file_cmd_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	char temp[] = "mdnieset_init_file_cmd_show\n\0";
	DPRINT("called %s\n", __func__);
	strcat(buf, temp);
	return strlen(buf);
}

static ssize_t mdnieset_init_file_cmd_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t size)
{
	int value;

	sscanf(buf, "%d", &value);
	DPRINT("mdnieset_init_file_cmd_store  : value(%d)\n", value);

	switch (value) {
	case 0:
		mdnie_tun_state.scenario = mDNIe_UI_MODE;
		break;

	default:
		printk(KERN_ERR
		       "mdnieset_init_file_cmd_store value is wrong : value(%d)\n",
		       value);
		break;
	}
	mDNIe_Set_Mode();

	return size;
}

static DEVICE_ATTR(mdnieset_init_file_cmd, 0664, mdnieset_init_file_cmd_show,
		   mdnieset_init_file_cmd_store);

static ssize_t outdoor_show(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	DPRINT("Current outdoor Mode : %s\n",
		outdoor_name[mdnie_tun_state.outdoor]);

	return snprintf(buf, 256, "Current outdoor Mode : %s\n",
		outdoor_name[mdnie_tun_state.outdoor]);
}

static ssize_t outdoor_store(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t size)
{
	int value;
	int backup;

	sscanf(buf, "%d", &value);

	DPRINT("outdoor value = %d, scenario = %d\n",
		value, mdnie_tun_state.scenario);

	if (value < OUTDOOR_OFF_MODE || value >= MAX_OUTDOOR_MODE) {
		DPRINT("[ERROR] : wrong outdoor mode value : %d\n",
				value);
	}

	backup = mdnie_tun_state.outdoor;
	mdnie_tun_state.outdoor = value;

	if (mdnie_tun_state.accessibility == NEGATIVE) {
		DPRINT("already negative mode(%d), do not outdoor mode(%d)\n",
			mdnie_tun_state.accessibility, mdnie_tun_state.outdoor);
	} else {
		DPRINT(" %s : (%s) -> (%s)\n",
			__func__, outdoor_name[backup], outdoor_name[mdnie_tun_state.outdoor]);
		mDNIe_Set_Mode();
	}

	return size;
}

static DEVICE_ATTR(outdoor, 0664, outdoor_show, outdoor_store);

#if 0 // accessibility
static ssize_t negative_show(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	return snprintf(buf, 256, "Current negative Value : %s\n",
		(mdnie_tun_state.accessibility == 1) ? "Enabled" : "Disabled");
}

static ssize_t negative_store(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t size)
{
	int value;

	sscanf(buf, "%d", &value);

	DPRINT
	    ("negative_store, input value = %d\n",
	     value);

	mdnie_tun_state.accessibility = value;

	mDNIe_Set_Mode();

	return size;
}
static DEVICE_ATTR(negative, 0664,
		   negative_show,
		   negative_store);

#endif

void is_negative_on(void)
{
	DPRINT("is negative Mode On = %d\n", mdnie_tun_state.accessibility);

	mDNIe_Set_Mode();
}

static ssize_t playspeed_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	DPRINT("called %s\n", __func__);
	return snprintf(buf, 256, "%d\n", play_speed_1_5);
}

static ssize_t playspeed_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	int value;
	sscanf(buf, "%d", &value);

	DPRINT("[Play Speed Set]play speed value = %d\n", value);

	is_play_speed_1_5(value);
	return size;
}
static DEVICE_ATTR(playspeed, 0664,
			playspeed_show,
			playspeed_store);

static ssize_t accessibility_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	DPRINT("Current accessibility Mode : %s\n",
		accessibility_name[mdnie_tun_state.accessibility]);

	return snprintf(buf, 256, "Current accessibility Mode : %s\n",
		accessibility_name[mdnie_tun_state.accessibility]);
}

static ssize_t accessibility_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	int cmd_value;
	char buffer[MDNIE_COLOR_BLINDE_CMD] = {0,};
	int buffer2[MDNIE_COLOR_BLINDE_CMD/2] = {0,};
	int loop;
	char temp;
	int backup;

	sscanf(buf, "%d %x %x %x %x %x %x %x %x %x", &cmd_value,
		&buffer2[0], &buffer2[1], &buffer2[2], &buffer2[3], &buffer2[4],
		&buffer2[5], &buffer2[6], &buffer2[7], &buffer2[8]);

	for(loop = 0; loop < MDNIE_COLOR_BLINDE_CMD/2; loop++) {
		buffer2[loop] = buffer2[loop] & 0xFFFF;

		buffer[loop * 2] = (buffer2[loop] & 0xFF00) >> 8;
		buffer[loop * 2 + 1] = buffer2[loop] & 0xFF;
	}

	for(loop = 0; loop < MDNIE_COLOR_BLINDE_CMD; loop+=2) {
		temp = buffer[loop];
		buffer[loop] = buffer[loop + 1];
		buffer[loop + 1] = temp;
	}

	backup = mdnie_tun_state.accessibility;

	if (cmd_value == NEGATIVE) {
		mdnie_tun_state.accessibility = NEGATIVE;
	} else if (cmd_value == COLOR_BLIND) {
		mdnie_tun_state.accessibility = COLOR_BLIND;
		memcpy(&COLOR_BLIND_2[MDNIE_COLOR_BLINDE_OFFSET],
				buffer, MDNIE_COLOR_BLINDE_CMD);
	}
	else if (cmd_value == SCREEN_CURTAIN) {
		mdnie_tun_state.accessibility = SCREEN_CURTAIN;
	}
	else if (cmd_value == ACCESSIBILITY_OFF) {
		mdnie_tun_state.accessibility = ACCESSIBILITY_OFF;
	} else
		pr_info("%s ACCESSIBILITY_MAX", __func__);

	DPRINT(" %s : (%s) -> (%s)\n",
			__func__, accessibility_name[backup], accessibility_name[mdnie_tun_state.accessibility]);

	mDNIe_Set_Mode();

	pr_info("%s cmd_value : %d size : %d", __func__, cmd_value, size);

	return size;
}

static DEVICE_ATTR(accessibility, 0664,
			accessibility_show,
			accessibility_store);

#if defined(CONFIG_LCD_HMT)
static ssize_t hmt_color_temperature_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	DPRINT("Current color temperature : %d\n", mdnie_tun_state.hmt_color_temperature);

	return snprintf(buf, 256, "Current color temperature : %d\n", mdnie_tun_state.hmt_color_temperature);
}

static ssize_t hmt_color_temperature_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	int value;
	int backup;

	sscanf(buf, "%d", &value);

	if (value < HMT_COLOR_TEMP_OFF || value >= HMT_COLOR_TEMP_MAX) {
		DPRINT("[ERROR] wrong color temperature value : %d\n", value);
		return size;
	}

	if (mdnie_tun_state.accessibility == NEGATIVE) {
		DPRINT("already negative mode(%d), do not update color temperature(%d)\n",
			mdnie_tun_state.accessibility, value);
		return size;
	}

	backup = mdnie_tun_state.hmt_color_temperature;
	mdnie_tun_state.hmt_color_temperature = value;

	DPRINT("%s : (%d) -> (%d)\n", __func__, backup, value);
	mDNIe_Set_Mode();

	return size;
}

static DEVICE_ATTR(hmt_color_temperature, 0660, hmt_color_temperature_show,
		   hmt_color_temperature_store);
#endif

#if defined(SUPPORT_WHITE_RGB)
static ssize_t sensorRGB_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
		return sprintf(buf, "%d %d %d\n", mdnie_tun_state.scr_white_red, mdnie_tun_state.scr_white_green, mdnie_tun_state.scr_white_blue);
}

static ssize_t sensorRGB_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int red, green, blue;
	char white_red, white_green, white_blue;

	sscanf(buf, "%d %d %d", &red, &green, &blue);

	if ((mdnie_tun_state.accessibility == ACCESSIBILITY_OFF) && (mdnie_tun_state.background == AUTO_MODE) &&	\
		((mdnie_tun_state.scenario == mDNIe_BROWSER_MODE) || (mdnie_tun_state.scenario == mDNIe_eBOOK_MODE))) 
	{
		white_red = (char)(red);
		white_green = (char)(green);
		white_blue= (char)(blue);
		mdnie_tun_state.scr_white_red = red;
		mdnie_tun_state.scr_white_green = green;
		mdnie_tun_state.scr_white_blue= blue;
		DPRINT("%s: white_red = %d, white_green = %d, white_blue = %d\n", __func__, white_red, white_green, white_blue);

		INPUT_PAYLOAD1(mdnie_tune_value[mdnie_tun_state.scenario][mdnie_tun_state.background][mdnie_tun_state.outdoor][0]);
		memcpy( white_rgb_buf, mdnie_tune_value[mdnie_tun_state.scenario][mdnie_tun_state.background][mdnie_tun_state.outdoor][1], MDNIE_TUNE_BODY_SIZE);

		white_rgb_buf[ADDRESS_SCR_WHITE_RED] = white_red;
		white_rgb_buf[ADDRESS_SCR_WHITE_GREEN] = white_green;
		white_rgb_buf[ADDRESS_SCR_WHITE_BLUE] = white_blue;

		INPUT_PAYLOAD2(white_rgb_buf);
		sending_tuning_cmd();
		free_tun_cmd();
	}

	return size;
}

static DEVICE_ATTR(sensorRGB, 0664, sensorRGB_show, sensorRGB_store);
#endif

static struct class *mdnie_class;
struct device *tune_mdnie_dev;

void init_mdnie_class(void)
{
	if (mdnie_tun_state.mdnie_enable) {
		pr_err("%s : mdnie already enable.. \n",__func__);
		return;
	}

	DPRINT("start!\n");

	mdnie_class = class_create(THIS_MODULE, "mdnie");
	if (IS_ERR(mdnie_class))
		pr_err("Failed to create class(mdnie)!\n");

	tune_mdnie_dev =
	    device_create(mdnie_class, NULL, 0, NULL,
		  "mdnie");
	if (IS_ERR(tune_mdnie_dev))
		pr_err("Failed to create device(mdnie)!\n");

	if (device_create_file
	    (tune_mdnie_dev, &dev_attr_scenario) < 0)
		pr_err("Failed to create device file(%s)!\n",
	       dev_attr_scenario.attr.name);

	if (device_create_file
	    (tune_mdnie_dev,
	     &dev_attr_mdnieset_user_select_file_cmd) < 0)
		pr_err("Failed to create device file(%s)!\n",
			dev_attr_mdnieset_user_select_file_cmd.attr.name);

	if (device_create_file
	    (tune_mdnie_dev, &dev_attr_mdnieset_init_file_cmd) < 0)
		pr_err("Failed to create device file(%s)!\n",
			dev_attr_mdnieset_init_file_cmd.attr.name);

	if (device_create_file
		(tune_mdnie_dev, &dev_attr_mode) < 0)
		pr_err("Failed to create device file(%s)!\n",
			dev_attr_mode.attr.name);

	if (device_create_file
		(tune_mdnie_dev, &dev_attr_outdoor) < 0)
		pr_err("Failed to create device file(%s)!\n",
	       dev_attr_outdoor.attr.name);

#if 0 // accessibility
	if (device_create_file
		(tune_mdnie_dev, &dev_attr_negative) < 0)
		pr_err("Failed to create device file(%s)!\n",
			dev_attr_negative.attr.name);
#endif

	if (device_create_file
		(tune_mdnie_dev, &dev_attr_playspeed) < 0)
		pr_err("Failed to create device file(%s)!=n",
			dev_attr_playspeed.attr.name);

	if (device_create_file
		(tune_mdnie_dev, &dev_attr_accessibility) < 0)
		pr_err("Failed to create device file(%s)!=n",
			dev_attr_accessibility.attr.name);

#if defined(CONFIG_LCD_HMT)
	if (device_create_file
		(tune_mdnie_dev, &dev_attr_hmt_color_temperature) < 0)
		pr_err("Failed to create device file(%s)!=n",
			dev_attr_hmt_color_temperature.attr.name);
#endif

#if defined(SUPPORT_WHITE_RGB)
	if (device_create_file
		(tune_mdnie_dev, &dev_attr_sensorRGB) < 0)
		pr_err("Failed to create device file(%s)!=n",
			dev_attr_sensorRGB.attr.name);
#endif

	mdnie_tun_state.mdnie_enable = true;

	DPRINT("end!\n");
}

void mdnie_lite_tuning_init(struct mipi_samsung_driver_data *msd)
{
	mdnie_msd = msd;
}

#define coordinate_data_size 6

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_WQHD_PT_PANEL) || \
	defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FHD_FA2_PT_PANEL)
#define scr_wr_addr 122

#define F1(x,y) ((y)-((164*(x))/151)+8)
#define F2(x,y) ((y)-((70*(x))/67)-7)
#define F3(x,y) ((y)+((181*(x))/35)-18852)
#define F4(x,y) ((y)+((157*(x))/52)-12055)

static char coordinate_data[][coordinate_data_size] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xfa, 0x00, 0xfa, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xfb, 0x00, 0xfe, 0x00}, /* Tune_2 */
	{0xfc, 0x00, 0xfb, 0x00, 0xff, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfe, 0x00, 0xfb, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_5 */
	{0xfb, 0x00, 0xfc, 0x00, 0xff, 0x00}, /* Tune_6 */
	{0xfc, 0x00, 0xff, 0x00, 0xfa, 0x00}, /* Tune_7 */
	{0xfb, 0x00, 0xff, 0x00, 0xfb, 0x00}, /* Tune_8 */
	{0xfb, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_9 */
};

#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQHD_PT_PANEL)|| defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQXGA_PT_PANEL)

#define scr_wr_addr 151

#define F1(x,y) ((y)-((67*(x))/62)+3)
#define F2(x,y) ((y)-((59*(x))/56)+9)
#define F3(x,y) ((y)+((29*(x))/6)-17787)
#define F4(x,y) ((y)+((43*(x))/14)-12229)

static char coordinate_data_1[][coordinate_data_size] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xfb, 0x00, 0xfb, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xfc, 0x00, 0xff, 0x00}, /* Tune_2 */
	{0xfb, 0x00, 0xfa, 0x00, 0xff, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfe, 0x00, 0xfc, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_5 */
	{0xfb, 0x00, 0xfd, 0x00, 0xff, 0x00}, /* Tune_6 */
	{0xfd, 0x00, 0xff, 0x00, 0xfa, 0x00}, /* Tune_7 */
	{0xfc, 0x00, 0xff, 0x00, 0xfc, 0x00}, /* Tune_8 */
	{0xfb, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_9 */
};

static char coordinate_data_2[][coordinate_data_size] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xf4, 0x00, 0xec, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xf5, 0x00, 0xf0, 0x00}, /* Tune_2 */
	{0xff, 0x00, 0xf7, 0x00, 0xf4, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xf7, 0x00, 0xed, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xf8, 0x00, 0xf0, 0x00}, /* Tune_5 */
	{0xff, 0x00, 0xf9, 0x00, 0xf3, 0x00}, /* Tune_6 */
	{0xff, 0x00, 0xfa, 0x00, 0xed, 0x00}, /* Tune_7 */
	{0xff, 0x00, 0xfb, 0x00, 0xf0, 0x00}, /* Tune_8 */
	{0xff, 0x00, 0xfb, 0x00, 0xf3, 0x00}, /* Tune_9 */
};

static char (*coordinate_data[MAX_BACKGROUND_MODE])[coordinate_data_size] = {
	coordinate_data_1,
	coordinate_data_2,
	coordinate_data_2,
	coordinate_data_1,
	coordinate_data_1,
};

#else
#define scr_wr_addr 36

#define F1(x,y) ((y)-((99*(x))/91)-6)
#define F2(x,y) ((y)-((164*(x))/157)-8)
#define F3(x,y) ((y)+((218*(x))/39)-20166)
#define F4(x,y) ((y)+((23*(x))/8)-11610)

static char coordinate_data[][coordinate_data_size] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xf7, 0x00, 0xf8, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xfa, 0x00, 0xfe, 0x00}, /* Tune_2 */
	{0xfb, 0x00, 0xf9, 0x00, 0xff, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfd, 0x00, 0xfa, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_5 */
	{0xf9, 0x00, 0xfb, 0x00, 0xff, 0x00}, /* Tune_6 */
	{0xfc, 0x00, 0xff, 0x00, 0xf8, 0x00}, /* Tune_7 */
	{0xfb, 0x00, 0xff, 0x00, 0xfb, 0x00}, /* Tune_8 */
	{0xf9, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_9 */
};
#endif

void coordinate_tunning(int x, int y)
{
	int tune_number;
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQHD_PT_PANEL)|| defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQXGA_PT_PANEL)
	int i, j;
#endif
	tune_number = 0;

	if (F1(x,y) > 0) {
		if (F3(x,y) > 0) {
			tune_number = 3;
		} else {
			if (F4(x,y) < 0)
				tune_number = 1;
			else
				tune_number = 2;
		}
	} else {
		if (F2(x,y) < 0) {
			if (F3(x,y) > 0) {
				tune_number = 9;
			} else {
				if (F4(x,y) < 0)
					tune_number = 7;
				else
					tune_number = 8;
			}
		} else {
			if (F3(x,y) > 0)
				tune_number = 6;
			else {
				if (F4(x,y) < 0)
					tune_number = 4;
				else
					tune_number = 5;
			}
		}
	}

	pr_info("%s x : %d, y : %d, tune_number : %d", __func__, x, y, tune_number);
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQHD_PT_PANEL)|| defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQXGA_PT_PANEL)
	for(i = 0; i < MAX_mDNIe_MODE; i++)
	{
		for(j = 0; j < MAX_BACKGROUND_MODE; j++)
		{
			if((mdnie_tune_value[i][j][0][1] != NULL) && (i != mDNIe_eBOOK_MODE))
			{
				memcpy(&mdnie_tune_value[i][j][0][1][scr_wr_addr], &coordinate_data[j][tune_number][0], coordinate_data_size);
			}
		}
	}
#else
	memcpy(&DYNAMIC_BROWSER_2[scr_wr_addr], &coordinate_data[tune_number][0], coordinate_data_size);
	memcpy(&DYNAMIC_GALLERY_2[scr_wr_addr], &coordinate_data[tune_number][0], coordinate_data_size);
	memcpy(&DYNAMIC_UI_2[scr_wr_addr], &coordinate_data[tune_number][0], coordinate_data_size);
	memcpy(&DYNAMIC_VIDEO_2[scr_wr_addr], &coordinate_data[tune_number][0], coordinate_data_size);
	memcpy(&DYNAMIC_VT_2[scr_wr_addr], &coordinate_data[tune_number][0], coordinate_data_size);
	memcpy(&DYNAMIC_EBOOK_2[scr_wr_addr], &coordinate_data[tune_number][0], coordinate_data_size);

	memcpy(&STANDARD_BROWSER_2[scr_wr_addr], &coordinate_data[tune_number][0], coordinate_data_size);
	memcpy(&STANDARD_GALLERY_2[scr_wr_addr], &coordinate_data[tune_number][0], coordinate_data_size);
	memcpy(&STANDARD_UI_2[scr_wr_addr], &coordinate_data[tune_number][0], coordinate_data_size);
	memcpy(&STANDARD_VIDEO_2[scr_wr_addr], &coordinate_data[tune_number][0], coordinate_data_size);
	memcpy(&STANDARD_VT_2[scr_wr_addr], &coordinate_data[tune_number][0], coordinate_data_size);
	memcpy(&STANDARD_EBOOK_2[scr_wr_addr], &coordinate_data[tune_number][0], coordinate_data_size);

	memcpy(&AUTO_BROWSER_2[scr_wr_addr], &coordinate_data[tune_number][0], coordinate_data_size);
	memcpy(&AUTO_CAMERA_2[scr_wr_addr], &coordinate_data[tune_number][0], coordinate_data_size);
	memcpy(&AUTO_GALLERY_2[scr_wr_addr], &coordinate_data[tune_number][0], coordinate_data_size);
	memcpy(&AUTO_UI_2[scr_wr_addr], &coordinate_data[tune_number][0], coordinate_data_size);
	memcpy(&AUTO_VIDEO_2[scr_wr_addr], &coordinate_data[tune_number][0], coordinate_data_size);
	memcpy(&AUTO_VT_2[scr_wr_addr], &coordinate_data[tune_number][0], coordinate_data_size);

	memcpy(&CAMERA_2[scr_wr_addr], &coordinate_data[tune_number][0], coordinate_data_size);
#endif
}
