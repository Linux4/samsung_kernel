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
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_WQHD_PT_PANEL) // K
#include "mdnie_lite_tuning_data_klte_fhd_s6e3fa2.h"
#include "mdnie_lite_tuning_data_klte_wqhd_s6e3ha0.h"
/*
#elif defined(CONFIG_FB_MSM_MIPI_JDI_TFT_VIDEO_FULL_HD_PT_PANEL) // JA
#include "mdnie_lite_tuning_data_jactiveltexx.h"
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_VIDEO_WVGA_S6E88A0_PT_PANEL) // ?
#include "mdnie_lite_tuning_data_wvga_s6e88a0.h"
#elif defined(CONFIG_MACH_JS01LTEDCM) || defined(CONFIG_MACH_JS01LTESBM) // JS01
#include "mdnie_lite_tuning_data_js01.h"
*/
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

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_WQHD_PT_PANEL)
#if defined(CONFIG_LCD_CLASS_DEVICE) && defined(DDI_VIDEO_ENHANCE_TUNING)
extern int mdnie_adb_test;
#endif
int get_lcd_panel_res(void);
#endif

struct mdnie_lite_tun_type mdnie_tun_state = {
	.mdnie_enable = false, 
	.scenario = mDNIe_UI_MODE,
	.background = STANDARD_MODE,
	.outdoor = OUTDOOR_OFF_MODE,
	.accessibility = ACCESSIBILITY_OFF,
#if defined(CONFIG_TDMB)
	.dmb = DMB_MODE_OFF,
#endif
};

#if !defined(CONFIG_TDMB)
const
#endif
char scenario_name[MAX_mDNIe_MODE][16] = {
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
	"HMT_8",
	"HMT_16",
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
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_WQHD_PT_PANEL)
	"SCREEN_CURTAIN_MODE",
#endif
};


static char level1_key[] = {
	0xF0,
	0x5A, 0x5A,
};

static char level2_key[] = {
	0xF0,
	0x5A, 0x5A,
};

static char tune_data1[MDNIE_TUNE_FIRST_SIZE] = {0,};
static char tune_data2[MDNIE_TUNE_SECOND_SIZE] = {0,};

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_WQHD_PT_PANEL)
static char tune_data1_adb[MDNIE_TUNE_FIRST_SIZE] = {0,};
static char tune_data2_adb[MDNIE_TUNE_SECOND_SIZE] = {0,};

void copy_tuning_data_from_adb(char *data1, char *data2)
{
	memcpy(tune_data1_adb, data1, MDNIE_TUNE_FIRST_SIZE);
	memcpy(tune_data2_adb, data2, MDNIE_TUNE_SECOND_SIZE);
}
#endif

static struct dsi_cmd_desc mdni_tune_cmd[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(level1_key)}, level1_key},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(level2_key)}, level2_key},

	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(tune_data1)}, tune_data1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(tune_data2)}, tune_data2},
};

void print_tun_data(void)
{
	int i;

	DPRINT("\n");
	DPRINT("---- size1 : %d", PAYLOAD1.dchdr.dlen);
	for (i = 0; i < MDNIE_TUNE_SECOND_SIZE ; i++)
		DPRINT("0x%x ", PAYLOAD1.payload[i]);
	DPRINT("\n");
	DPRINT("---- size2 : %d", PAYLOAD2.dchdr.dlen);
	for (i = 0; i < MDNIE_TUNE_FIRST_SIZE ; i++)
		DPRINT("0x%x ", PAYLOAD2.payload[i]);
	DPRINT("\n");
}

void free_tun_cmd(void)
{
	memset(tune_data1, 0, MDNIE_TUNE_FIRST_SIZE);
	memset(tune_data2, 0, MDNIE_TUNE_SECOND_SIZE);
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

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_WQHD_PT_PANEL)
#if defined(CONFIG_LCD_CLASS_DEVICE) && defined(DDI_VIDEO_ENHANCE_TUNING)
	if (mdnie_adb_test) {
		DPRINT("[ERROR] mdnie_adb_test is doning .. copy from adb data .. \n");
		INPUT_PAYLOAD1(tune_data2_adb);
		INPUT_PAYLOAD2(tune_data1_adb);
	}
#endif
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

	DPRINT("mDNIe_Set_Mode start\n");

	if (!mfd) {
		DPRINT("[ERROR] mfd is null!\n");
		return;
	}

#if defined(CONFIG_LCD_FORCE_VIDEO_MODE)
	DPRINT("mDNIe_Set_Mode start : return cause of video mode\n");
	return;
#endif

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
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_WQHD_PT_PANEL)
		if (get_lcd_panel_res() == 0) { // 0 wqhd - will be removed
			DPRINT(" = ACCESSIBILITY MODE =\n");
			INPUT_PAYLOAD1(blind_tune_value[mdnie_tun_state.accessibility][0]);
			INPUT_PAYLOAD2(blind_tune_value[mdnie_tun_state.accessibility][1]);
		} else {
			DPRINT(" = ACCESSIBILITY MODE =\n");
			INPUT_PAYLOAD1(blind_tune_value_fhd[mdnie_tun_state.accessibility][0]);
			INPUT_PAYLOAD2(blind_tune_value_fhd[mdnie_tun_state.accessibility][1]);
		}
#else
		DPRINT(" = ACCESSIBILITY MODE =\n");
		INPUT_PAYLOAD1(blind_tune_value[mdnie_tun_state.accessibility][0]);
		INPUT_PAYLOAD2(blind_tune_value[mdnie_tun_state.accessibility][1]);
#endif
	}
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
	else {
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_WQHD_PT_PANEL)
		if (get_lcd_panel_res() == 0) { // 0 wqhd - will be removed
			if (!mdnie_tune_value[mdnie_tun_state.scenario][mdnie_tun_state.background][mdnie_tun_state.outdoor][0] ||
				!mdnie_tune_value[mdnie_tun_state.scenario][mdnie_tun_state.background][mdnie_tun_state.outdoor][1]) {
				pr_err("mdnie tune data is NULL!\n");
				return;
			} else {
				INPUT_PAYLOAD1(
					mdnie_tune_value[mdnie_tun_state.scenario][mdnie_tun_state.background][mdnie_tun_state.outdoor][0]);
				INPUT_PAYLOAD2(
					mdnie_tune_value[mdnie_tun_state.scenario][mdnie_tun_state.background][mdnie_tun_state.outdoor][1]);	
			}
		} else { // 1 fhd
			if (!mdnie_tune_value_fhd[mdnie_tun_state.scenario][mdnie_tun_state.background][mdnie_tun_state.outdoor][0] ||
				!mdnie_tune_value_fhd[mdnie_tun_state.scenario][mdnie_tun_state.background][mdnie_tun_state.outdoor][1]) {
				pr_err("mdnie tune data is NULL!\n");
				return;
			} else {
				INPUT_PAYLOAD1(
					mdnie_tune_value_fhd[mdnie_tun_state.scenario][mdnie_tun_state.background][mdnie_tun_state.outdoor][0]);
				INPUT_PAYLOAD2(
					mdnie_tune_value_fhd[mdnie_tun_state.scenario][mdnie_tun_state.background][mdnie_tun_state.outdoor][1]);	
			}
		}
#else
		if (!mdnie_tune_value[mdnie_tun_state.scenario][mdnie_tun_state.background][mdnie_tun_state.outdoor][0] ||
			!mdnie_tune_value[mdnie_tun_state.scenario][mdnie_tun_state.background][mdnie_tun_state.outdoor][1]) {
			pr_err("mdnie tune data is NULL!\n");
			return;
		} else {
			INPUT_PAYLOAD1(
				mdnie_tune_value[mdnie_tun_state.scenario][mdnie_tun_state.background][mdnie_tun_state.outdoor][0]);
			INPUT_PAYLOAD2(
				mdnie_tune_value[mdnie_tun_state.scenario][mdnie_tun_state.background][mdnie_tun_state.outdoor][1]);	
		}
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
	if (value >= mDNIe_DMB_MODE && value <= mDNIe_DMB_COLD_MODE) {
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
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_WQHD_PT_PANEL)
	else if (cmd_value == SCREEN_CURTAIN) {
		mdnie_tun_state.accessibility = SCREEN_CURTAIN;
	}
#endif
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

	mdnie_tun_state.mdnie_enable = true;

#if defined(CONFIG_TDMB)
	strcpy((char*) scenario_name[mDNIe_DMB_MODE], "DMB_MODE");
	strcpy((char*) scenario_name[mDNIe_DMB_WARM_MODE], "DMB_WARM_MODE");
	strcpy((char*) scenario_name[mDNIe_DMB_COLD_MODE], "DMB_COLD_MODE");
#endif

	DPRINT("end!\n");
}

void mdnie_lite_tuning_init(struct mipi_samsung_driver_data *msd)
{
	mdnie_msd = msd;
}

#define coordinate_data_size 6
#define scr_wr_addr 36

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FULL_HD_PT_PANEL) \
	|| defined(CONFIG_FB_MSM_MIPI_SAMSUNG_YOUM_CMD_FULL_HD_PT_PANEL) \
        || defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_WQHD_PT_PANEL)
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
#else
#define F1(x,y) ((y)-((107*(x))/100)-60)
#define F2(x,y) ((y)-((44*(x))/43)-72)
#define F3(x,y) ((y)+((57*(x))/8)-25161)
#define F4(x,y) ((y)+((19*(x))/6)-12613)

static char coordinate_data[][coordinate_data_size] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xf7, 0x00, 0xf8, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xf9, 0x00, 0xfe, 0x00}, /* Tune_2 */
	{0xfa, 0x00, 0xf8, 0x00, 0xff, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfc, 0x00, 0xf9, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_5 */
	{0xf8, 0x00, 0xfa, 0x00, 0xff, 0x00}, /* Tune_6 */
	{0xfc, 0x00, 0xff, 0x00, 0xf8, 0x00}, /* Tune_7 */
	{0xfb, 0x00, 0xff, 0x00, 0xfb, 0x00}, /* Tune_8 */
	{0xf9, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_9 */
};
#endif

void coordinate_tunning(int x, int y)
{
	int tune_number;
#if defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_WQHD_PT_PANEL)
	pr_err("%s : temp return!\n", __func__);
	return;
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

}

#if 0
void mDNIe_Set_Mode(enum Lcd_mDNIe_UI mode)
{
	struct msm_fb_data_type *mfd;
	mfd = mdnie_msd->mfd;

	DPRINT("mDNIe_Set_Mode start , mode(%d), background(%d)\n",
		mode, mdnie_tun_state.background);

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

	if (mode < mDNIe_UI_MODE || mode >= MAX_mDNIe_MODE) {
		DPRINT("[ERROR] wrong Scenario mode value : %d\n",
			mode);
		return;
	}

	if (mdnie_tun_state.negative) {
		DPRINT("already negative mode(%d), do not set background(%d)\n",
			mdnie_tun_state.negative, mdnie_tun_state.background);
		return;
	}

	play_speed_1_5 = 0;

	/*
	*	Blind mode & Screen mode has separated menu.
	*	To make a sync below code added.
	*	Bline mode has priority than Screen mode
	*/
	if (mdnie_tun_state.accessibility == COLOR_BLIND)
		mode = mDNIE_BLINE_MODE;
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_WQHD_PT_PANEL)
	if (get_lcd_panel_res() == 0) { // 0 : wqhd
#endif

	switch (mode) {
	case mDNIe_UI_MODE:
		DPRINT(" = UI MODE =\n");
		if (mdnie_tun_state.background == STANDARD_MODE) {
			DPRINT(" = STANDARD MODE =\n");
			INPUT_PAYLOAD1(STANDARD_UI_1);
			INPUT_PAYLOAD2(STANDARD_UI_2);
#if !defined(CONFIG_SUPPORT_DISPLAY_OCTA_TFT)
		} else if (mdnie_tun_state.background == NATURAL_MODE) {
			DPRINT(" = NATURAL MODE =\n");
			INPUT_PAYLOAD1(NATURAL_UI_1);
			INPUT_PAYLOAD2(NATURAL_UI_2);
#endif
		} else if (mdnie_tun_state.background == DYNAMIC_MODE) {
			DPRINT(" = DYNAMIC MODE =\n");
			INPUT_PAYLOAD1(DYNAMIC_UI_1);
			INPUT_PAYLOAD2(DYNAMIC_UI_2);
		} else if (mdnie_tun_state.background == MOVIE_MODE) {
			DPRINT(" = MOVIE MODE =\n");
			INPUT_PAYLOAD1(MOVIE_UI_1);
			INPUT_PAYLOAD2(MOVIE_UI_2);
		} else if (mdnie_tun_state.background == AUTO_MODE) {
			DPRINT(" = AUTO MODE =\n");
			INPUT_PAYLOAD1(AUTO_UI_1);
			INPUT_PAYLOAD2(AUTO_UI_2);
		}
		break;

	case mDNIe_VIDEO_MODE:
		DPRINT(" = VIDEO MODE =\n");
		if (mdnie_tun_state.outdoor == OUTDOOR_ON_MODE) {
			DPRINT(" = OUTDOOR ON MODE =\n");
			INPUT_PAYLOAD1(OUTDOOR_VIDEO_1);
			INPUT_PAYLOAD2(OUTDOOR_VIDEO_2);
		} else if (mdnie_tun_state.outdoor == OUTDOOR_OFF_MODE) {
			DPRINT(" = OUTDOOR OFF MODE =\n");
			if (mdnie_tun_state.background == STANDARD_MODE) {
				DPRINT(" = STANDARD MODE =\n");
				INPUT_PAYLOAD1(STANDARD_VIDEO_1);
				INPUT_PAYLOAD2(STANDARD_VIDEO_2);
#if !defined(CONFIG_SUPPORT_DISPLAY_OCTA_TFT)
			} else if (mdnie_tun_state.background == NATURAL_MODE) {
				DPRINT(" = NATURAL MODE =\n");
				INPUT_PAYLOAD1(NATURAL_VIDEO_1);
				INPUT_PAYLOAD2(NATURAL_VIDEO_2);
#endif
			} else if (mdnie_tun_state.background == DYNAMIC_MODE) {
				DPRINT(" = DYNAMIC MODE =\n");
				INPUT_PAYLOAD1(DYNAMIC_VIDEO_1);
				INPUT_PAYLOAD2(DYNAMIC_VIDEO_2);
			} else if (mdnie_tun_state.background == MOVIE_MODE) {
				DPRINT(" = MOVIE MODE =\n");
				INPUT_PAYLOAD1(MOVIE_VIDEO_1);
				INPUT_PAYLOAD2(MOVIE_VIDEO_2);
			} else if (mdnie_tun_state.background == AUTO_MODE) {
				DPRINT(" = AUTO MODE =\n");
				INPUT_PAYLOAD1(AUTO_VIDEO_1);
				INPUT_PAYLOAD2(AUTO_VIDEO_2);
			}
		}
		break;

	case mDNIe_VIDEO_WARM_MODE:
		DPRINT(" = VIDEO WARM MODE =\n");
		if (mdnie_tun_state.outdoor == OUTDOOR_ON_MODE) {
			DPRINT(" = OUTDOOR ON MODE =\n");
			INPUT_PAYLOAD1(WARM_OUTDOOR_1);
			INPUT_PAYLOAD2(WARM_OUTDOOR_2);
		} else if (mdnie_tun_state.outdoor == OUTDOOR_OFF_MODE) {
			DPRINT(" = OUTDOOR OFF MODE =\n");
			INPUT_PAYLOAD1(WARM_1);
			INPUT_PAYLOAD2(WARM_2);
		}
		break;

	case mDNIe_VIDEO_COLD_MODE:
		DPRINT(" = VIDEO COLD MODE =\n");
		if (mdnie_tun_state.outdoor == OUTDOOR_ON_MODE) {
			DPRINT(" = OUTDOOR ON MODE =\n");
			INPUT_PAYLOAD1(COLD_OUTDOOR_1);
			INPUT_PAYLOAD2(COLD_OUTDOOR_2);
		} else if (mdnie_tun_state.outdoor == OUTDOOR_OFF_MODE) {
			DPRINT(" = OUTDOOR OFF MODE =\n");
			INPUT_PAYLOAD1(COLD_1);
			INPUT_PAYLOAD2(COLD_2);
		}
		break;

	case mDNIe_CAMERA_MODE:
		DPRINT(" = CAMERA MODE =\n");
		if (mdnie_tun_state.outdoor == OUTDOOR_OFF_MODE) {
			if (mdnie_tun_state.background == AUTO_MODE) {
				DPRINT(" = AUTO MODE =\n");
				INPUT_PAYLOAD1(AUTO_CAMERA_1);
				INPUT_PAYLOAD2(AUTO_CAMERA_2);
			} else {
				DPRINT(" = STANDARD MODE =\n");
				INPUT_PAYLOAD1(CAMERA_1);
				INPUT_PAYLOAD2(CAMERA_2);
			}
		} else if (mdnie_tun_state.outdoor == OUTDOOR_ON_MODE) {
			DPRINT(" = NATURAL MODE =\n");
			INPUT_PAYLOAD1(CAMERA_OUTDOOR_1);
			INPUT_PAYLOAD2(CAMERA_OUTDOOR_2);
		}
		break;

	case mDNIe_NAVI:
		DPRINT(" = NAVI MODE =\n");
		DPRINT("no data for NAVI MODE..\n");
		break;

	case mDNIe_GALLERY:
		DPRINT(" = GALLERY MODE =\n");
		if (mdnie_tun_state.background == STANDARD_MODE) {
			DPRINT(" = STANDARD MODE =\n");
			INPUT_PAYLOAD1(STANDARD_GALLERY_1);
			INPUT_PAYLOAD2(STANDARD_GALLERY_2);
#if !defined(CONFIG_SUPPORT_DISPLAY_OCTA_TFT)
		} else if (mdnie_tun_state.background == NATURAL_MODE) {
			DPRINT(" = NATURAL MODE =\n");
			INPUT_PAYLOAD1(NATURAL_GALLERY_1);
			INPUT_PAYLOAD2(NATURAL_GALLERY_2);
#endif
		} else if (mdnie_tun_state.background == DYNAMIC_MODE) {
			DPRINT(" = DYNAMIC MODE =\n");
			INPUT_PAYLOAD1(DYNAMIC_GALLERY_1);
			INPUT_PAYLOAD2(DYNAMIC_GALLERY_2);
		} else if (mdnie_tun_state.background == MOVIE_MODE) {
			DPRINT(" = MOVIE MODE =\n");
			INPUT_PAYLOAD1(MOVIE_GALLERY_1);
			INPUT_PAYLOAD2(MOVIE_GALLERY_2);
		} else if (mdnie_tun_state.background == AUTO_MODE) {
			DPRINT(" = AUTO MODE =\n");
			INPUT_PAYLOAD1(AUTO_GALLERY_1);
			INPUT_PAYLOAD2(AUTO_GALLERY_2);
		}
		break;

	case mDNIe_VT_MODE:
		DPRINT(" = VT MODE =\n");
		if (mdnie_tun_state.background == STANDARD_MODE) {
			DPRINT(" = STANDARD MODE =\n");
			INPUT_PAYLOAD1(STANDARD_VT_1);
			INPUT_PAYLOAD2(STANDARD_VT_2);
#if !defined(CONFIG_SUPPORT_DISPLAY_OCTA_TFT)
		} else if (mdnie_tun_state.background == NATURAL_MODE) {
			DPRINT(" = NATURAL MODE =\n");
			INPUT_PAYLOAD1(NATURAL_VT_1);
			INPUT_PAYLOAD2(NATURAL_VT_2);
#endif
		} else if (mdnie_tun_state.background == DYNAMIC_MODE) {
			DPRINT(" = DYNAMIC MODE =\n");
			INPUT_PAYLOAD1(DYNAMIC_VT_1);
			INPUT_PAYLOAD2(DYNAMIC_VT_2);
		} else if (mdnie_tun_state.background == MOVIE_MODE) {
			DPRINT(" = MOVIE MODE =\n");
			INPUT_PAYLOAD1(MOVIE_VT_1);
			INPUT_PAYLOAD2(MOVIE_VT_2);
		} else if (mdnie_tun_state.background == AUTO_MODE) {
			DPRINT(" = AUTO MODE =\n");
			INPUT_PAYLOAD1(AUTO_VT_1);
			INPUT_PAYLOAD2(AUTO_VT_2);
		}
		break;

#if defined(CONFIG_TDMB)
	case mDNIe_DMB_MODE:
		DPRINT(" = DMB MODE =\n");
		if (mdnie_tun_state.outdoor == OUTDOOR_ON_MODE) {
			DPRINT(" = OUTDOOR ON MODE =\n");
			INPUT_PAYLOAD1(OUTDOOR_DMB_1);
			INPUT_PAYLOAD2(OUTDOOR_DMB_2);
		} else if (mdnie_tun_state.outdoor == OUTDOOR_OFF_MODE) {
			DPRINT(" = OUTDOOR OFF MODE =\n");
			if (mdnie_tun_state.background == STANDARD_MODE) {
				DPRINT(" = STANDARD MODE =\n");
				INPUT_PAYLOAD1(STANDARD_DMB_1);
				INPUT_PAYLOAD2(STANDARD_DMB_2);
#if !defined(CONFIG_SUPPORT_DISPLAY_OCTA_TFT)
			} else if (mdnie_tun_state.background == NATURAL_MODE) {
				DPRINT(" = NATURAL MODE =\n");
				INPUT_PAYLOAD1(NATURAL_DMB_1);
				INPUT_PAYLOAD2(NATURAL_DMB_2);
#endif
			} else if (mdnie_tun_state.background == DYNAMIC_MODE) {
				DPRINT(" = DYNAMIC MODE =\n");
				INPUT_PAYLOAD1(DYNAMIC_DMB_1);
				INPUT_PAYLOAD2(DYNAMIC_DMB_2);
			} else if (mdnie_tun_state.background == MOVIE_MODE) {
				DPRINT(" = MOVIE MODE =\n");
				INPUT_PAYLOAD1(MOVIE_DMB_1);
				INPUT_PAYLOAD2(MOVIE_DMB_2);
			} else if (mdnie_tun_state.background == AUTO_MODE) {
				DPRINT(" = AUTO MODE =\n");
				INPUT_PAYLOAD1(AUTO_DMB_1);
				INPUT_PAYLOAD2(AUTO_DMB_2);
			}
		}
		break;

	case mDNIe_DMB_WARM_MODE:
		DPRINT(" = DMB WARM MODE =\n");
		if (mdnie_tun_state.outdoor == OUTDOOR_ON_MODE) {
			DPRINT(" = OUTDOOR ON MODE =\n");
			INPUT_PAYLOAD1(WARM_OUTDOOR_DMB_1);
			INPUT_PAYLOAD2(WARM_OUTDOOR_DMB_2);
		} else if (mdnie_tun_state.outdoor == OUTDOOR_OFF_MODE) {
			DPRINT(" = OUTDOOR OFF MODE =\n");
			INPUT_PAYLOAD1(WARM_DMB_1);
			INPUT_PAYLOAD2(WARM_DMB_2);
		}
		break;

	case mDNIe_DMB_COLD_MODE:
		DPRINT(" = DMB COLD MODE =\n");
		if (mdnie_tun_state.outdoor == OUTDOOR_ON_MODE) {
			DPRINT(" = OUTDOOR ON MODE =\n");
			INPUT_PAYLOAD1(COLD_OUTDOOR_DMB_1);
			INPUT_PAYLOAD2(COLD_OUTDOOR_DMB_2);
		} else if (mdnie_tun_state.outdoor == OUTDOOR_OFF_MODE) {
			DPRINT(" = OUTDOOR OFF MODE =\n");
			INPUT_PAYLOAD1(COLD_DMB_1);
			INPUT_PAYLOAD2(COLD_DMB_2);
		}
		break;
#endif

	case mDNIe_BROWSER_MODE:
		DPRINT(" = BROWSER MODE =\n");
		if (mdnie_tun_state.background == STANDARD_MODE) {
			DPRINT(" = STANDARD MODE =\n");
			INPUT_PAYLOAD1(STANDARD_BROWSER_1);
			INPUT_PAYLOAD2(STANDARD_BROWSER_2);
#if !defined(CONFIG_SUPPORT_DISPLAY_OCTA_TFT)
		} else if (mdnie_tun_state.background == NATURAL_MODE) {
			DPRINT(" = NATURAL MODE =\n");
			INPUT_PAYLOAD1(NATURAL_BROWSER_1);
			INPUT_PAYLOAD2(NATURAL_BROWSER_2);
#endif
		} else if (mdnie_tun_state.background == DYNAMIC_MODE) {
			DPRINT(" = DYNAMIC MODE =\n");
			INPUT_PAYLOAD1(DYNAMIC_BROWSER_1);
			INPUT_PAYLOAD2(DYNAMIC_BROWSER_2);
		} else if (mdnie_tun_state.background == MOVIE_MODE) {
			DPRINT(" = MOVIE MODE =\n");
			INPUT_PAYLOAD1(MOVIE_BROWSER_1);
			INPUT_PAYLOAD2(MOVIE_BROWSER_2);
		} else if (mdnie_tun_state.background == AUTO_MODE) {
			DPRINT(" = AUTO MODE =\n");
			INPUT_PAYLOAD1(AUTO_BROWSER_1);
			INPUT_PAYLOAD2(AUTO_BROWSER_2);
		}
		break;

	case mDNIe_eBOOK_MODE:
		DPRINT(" = eBOOK MODE =\n");
		if (mdnie_tun_state.background == STANDARD_MODE) {
			DPRINT(" = STANDARD MODE =\n");
			INPUT_PAYLOAD1(STANDARD_EBOOK_1);
			INPUT_PAYLOAD2(STANDARD_EBOOK_2);
#if !defined(CONFIG_SUPPORT_DISPLAY_OCTA_TFT)
		} else if (mdnie_tun_state.background == NATURAL_MODE) {
			DPRINT(" = NATURAL MODE =\n");
			INPUT_PAYLOAD1(NATURAL_EBOOK_1);
			INPUT_PAYLOAD2(NATURAL_EBOOK_2);
#endif
		} else if (mdnie_tun_state.background == DYNAMIC_MODE) {
			DPRINT(" = DYNAMIC MODE =\n");
			INPUT_PAYLOAD1(DYNAMIC_EBOOK_1);
			INPUT_PAYLOAD2(DYNAMIC_EBOOK_2);
		} else if (mdnie_tun_state.background == MOVIE_MODE) {
			DPRINT(" = MOVIE MODE =\n");
			INPUT_PAYLOAD1(MOVIE_EBOOK_1);
			INPUT_PAYLOAD2(MOVIE_EBOOK_2);
		} else if (mdnie_tun_state.background == AUTO_MODE) {
			DPRINT(" = AUTO MODE =\n");
			INPUT_PAYLOAD1(AUTO_EBOOK_1);
			INPUT_PAYLOAD2(AUTO_EBOOK_2);
		}
		break;

#if !defined(CONFIG_SUPPORT_DISPLAY_OCTA_TFT)
	case mDNIe_EMAIL_MODE:
		DPRINT(" = EMAIL MODE =\n");
		if (mdnie_tun_state.background == STANDARD_MODE) {
			DPRINT(" = STANDARD MODE =\n");
			INPUT_PAYLOAD1(AUTO_EMAIL_1);
			INPUT_PAYLOAD2(AUTO_EMAIL_2);
		} else if (mdnie_tun_state.background == NATURAL_MODE) {
			DPRINT(" = NATURAL MODE =\n");
			INPUT_PAYLOAD1(AUTO_EMAIL_1);
			INPUT_PAYLOAD2(AUTO_EMAIL_2);
		} else if (mdnie_tun_state.background == DYNAMIC_MODE) {
			DPRINT(" = DYNAMIC MODE =\n");
			INPUT_PAYLOAD1(AUTO_EMAIL_1);
			INPUT_PAYLOAD2(AUTO_EMAIL_2);
		} else if (mdnie_tun_state.background == MOVIE_MODE) {
			DPRINT(" = MOVIE MODE =\n");
			INPUT_PAYLOAD1(AUTO_EMAIL_1);
			INPUT_PAYLOAD2(AUTO_EMAIL_2);
		} else if (mdnie_tun_state.background == AUTO_MODE) {
			DPRINT(" = AUTO MODE =\n");
			INPUT_PAYLOAD1(AUTO_EMAIL_1);
			INPUT_PAYLOAD2(AUTO_EMAIL_2);
		}
		break;
#endif

	case mDNIE_BLINE_MODE:
		DPRINT(" = BLIND MODE =\n");
		INPUT_PAYLOAD1(COLOR_BLIND_1);
		INPUT_PAYLOAD2(COLOR_BLIND_2);
		break;

	default:
		DPRINT("[%s] no option (%d)\n", __func__, mode);
		return;
	}

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_WQHD_PT_PANEL)
	 }else { // 1: fhd

		switch (mode) {
			case mDNIe_UI_MODE:
				DPRINT(" = UI MODE =\n");
				if (mdnie_tun_state.background == STANDARD_MODE) {
					DPRINT(" = STANDARD MODE =\n");
					INPUT_PAYLOAD1(STANDARD_UI_1_FHD);
					INPUT_PAYLOAD2(STANDARD_UI_2_FHD);
#if !defined(CONFIG_SUPPORT_DISPLAY_OCTA_TFT)
				} else if (mdnie_tun_state.background == NATURAL_MODE) {
					DPRINT(" = NATURAL MODE =\n");
					INPUT_PAYLOAD1(NATURAL_UI_1_FHD);
					INPUT_PAYLOAD2(NATURAL_UI_2_FHD);
#endif
				} else if (mdnie_tun_state.background == DYNAMIC_MODE) {
					DPRINT(" = DYNAMIC MODE =\n");
					INPUT_PAYLOAD1(DYNAMIC_UI_1_FHD);
					INPUT_PAYLOAD2(DYNAMIC_UI_2_FHD);
				} else if (mdnie_tun_state.background == MOVIE_MODE) {
					DPRINT(" = MOVIE MODE =\n");
					INPUT_PAYLOAD1(MOVIE_UI_1_FHD);
					INPUT_PAYLOAD2(MOVIE_UI_2_FHD);
				} else if (mdnie_tun_state.background == AUTO_MODE) {
					DPRINT(" = AUTO MODE =\n");
					INPUT_PAYLOAD1(AUTO_UI_1_FHD);
					INPUT_PAYLOAD2(AUTO_UI_2_FHD);
				}
				break;
		
			case mDNIe_VIDEO_MODE:
				DPRINT(" = VIDEO MODE =\n");
				if (mdnie_tun_state.outdoor == OUTDOOR_ON_MODE) {
					DPRINT(" = OUTDOOR ON MODE =\n");
					INPUT_PAYLOAD1(OUTDOOR_VIDEO_1_FHD);
					INPUT_PAYLOAD2(OUTDOOR_VIDEO_2_FHD);
				} else if (mdnie_tun_state.outdoor == OUTDOOR_OFF_MODE) {
					DPRINT(" = OUTDOOR OFF MODE =\n");
					if (mdnie_tun_state.background == STANDARD_MODE) {
						DPRINT(" = STANDARD MODE =\n");
						INPUT_PAYLOAD1(STANDARD_VIDEO_1_FHD);
						INPUT_PAYLOAD2(STANDARD_VIDEO_2_FHD);
#if !defined(CONFIG_SUPPORT_DISPLAY_OCTA_TFT)
					} else if (mdnie_tun_state.background == NATURAL_MODE) {
						DPRINT(" = NATURAL MODE =\n");
						INPUT_PAYLOAD1(NATURAL_VIDEO_1_FHD);
						INPUT_PAYLOAD2(NATURAL_VIDEO_2_FHD);
#endif
					} else if (mdnie_tun_state.background == DYNAMIC_MODE) {
						DPRINT(" = DYNAMIC MODE =\n");
						INPUT_PAYLOAD1(DYNAMIC_VIDEO_1_FHD);
						INPUT_PAYLOAD2(DYNAMIC_VIDEO_2_FHD);
					} else if (mdnie_tun_state.background == MOVIE_MODE) {
						DPRINT(" = MOVIE MODE =\n");
						INPUT_PAYLOAD1(MOVIE_VIDEO_1_FHD);
						INPUT_PAYLOAD2(MOVIE_VIDEO_2_FHD);
					} else if (mdnie_tun_state.background == AUTO_MODE) {
						DPRINT(" = AUTO MODE =\n");
						INPUT_PAYLOAD1(AUTO_VIDEO_1_FHD);
						INPUT_PAYLOAD2(AUTO_VIDEO_2_FHD);
					}
				}
				break;
		
			case mDNIe_VIDEO_WARM_MODE:
				DPRINT(" = VIDEO WARM MODE =\n");
				if (mdnie_tun_state.outdoor == OUTDOOR_ON_MODE) {
					DPRINT(" = OUTDOOR ON MODE =\n");
					INPUT_PAYLOAD1(WARM_OUTDOOR_1_FHD);
					INPUT_PAYLOAD2(WARM_OUTDOOR_2_FHD);
				} else if (mdnie_tun_state.outdoor == OUTDOOR_OFF_MODE) {
					DPRINT(" = OUTDOOR OFF MODE =\n");
					INPUT_PAYLOAD1(WARM_1_FHD);
					INPUT_PAYLOAD2(WARM_2_FHD);
				}
				break;
		
			case mDNIe_VIDEO_COLD_MODE:
				DPRINT(" = VIDEO COLD MODE =\n");
				if (mdnie_tun_state.outdoor == OUTDOOR_ON_MODE) {
					DPRINT(" = OUTDOOR ON MODE =\n");
					INPUT_PAYLOAD1(COLD_OUTDOOR_1_FHD);
					INPUT_PAYLOAD2(COLD_OUTDOOR_2_FHD);
				} else if (mdnie_tun_state.outdoor == OUTDOOR_OFF_MODE) {
					DPRINT(" = OUTDOOR OFF MODE =\n");
					INPUT_PAYLOAD1(COLD_1_FHD);
					INPUT_PAYLOAD2(COLD_2_FHD);
				}
				break;
		
			case mDNIe_CAMERA_MODE:
				DPRINT(" = CAMERA MODE =\n");
				if (mdnie_tun_state.outdoor == OUTDOOR_OFF_MODE) {
					if (mdnie_tun_state.background == AUTO_MODE) {
						DPRINT(" = AUTO MODE =\n");
						INPUT_PAYLOAD1(AUTO_CAMERA_1_FHD);
						INPUT_PAYLOAD2(AUTO_CAMERA_2_FHD);
					} else {
						DPRINT(" = STANDARD MODE =\n");
						INPUT_PAYLOAD1(CAMERA_1_FHD);
						INPUT_PAYLOAD2(CAMERA_2_FHD);
					}
				} else if (mdnie_tun_state.outdoor == OUTDOOR_ON_MODE) {
					DPRINT(" = NATURAL MODE =\n");
					INPUT_PAYLOAD1(CAMERA_OUTDOOR_1_FHD);
					INPUT_PAYLOAD2(CAMERA_OUTDOOR_2_FHD);
				}
				break;
		
			case mDNIe_NAVI:
				DPRINT(" = NAVI MODE =\n");
				DPRINT("no data for NAVI MODE..\n");
				break;
		
			case mDNIe_GALLERY:
				DPRINT(" = GALLERY MODE =\n");
				if (mdnie_tun_state.background == STANDARD_MODE) {
					DPRINT(" = STANDARD MODE =\n");
					INPUT_PAYLOAD1(STANDARD_GALLERY_1_FHD);
					INPUT_PAYLOAD2(STANDARD_GALLERY_2_FHD);
#if !defined(CONFIG_SUPPORT_DISPLAY_OCTA_TFT)
				} else if (mdnie_tun_state.background == NATURAL_MODE) {
					DPRINT(" = NATURAL MODE =\n");
					INPUT_PAYLOAD1(NATURAL_GALLERY_1_FHD);
					INPUT_PAYLOAD2(NATURAL_GALLERY_2_FHD);
#endif
				} else if (mdnie_tun_state.background == DYNAMIC_MODE) {
					DPRINT(" = DYNAMIC MODE =\n");
					INPUT_PAYLOAD1(DYNAMIC_GALLERY_1_FHD);
					INPUT_PAYLOAD2(DYNAMIC_GALLERY_2_FHD);
				} else if (mdnie_tun_state.background == MOVIE_MODE) {
					DPRINT(" = MOVIE MODE =\n");
					INPUT_PAYLOAD1(MOVIE_GALLERY_1_FHD);
					INPUT_PAYLOAD2(MOVIE_GALLERY_2_FHD);
				} else if (mdnie_tun_state.background == AUTO_MODE) {
					DPRINT(" = AUTO MODE =\n");
					INPUT_PAYLOAD1(AUTO_GALLERY_1_FHD);
					INPUT_PAYLOAD2(AUTO_GALLERY_2_FHD);
				}
				break;
		
			case mDNIe_VT_MODE:
				DPRINT(" = VT MODE =\n");
				if (mdnie_tun_state.background == STANDARD_MODE) {
					DPRINT(" = STANDARD MODE =\n");
					INPUT_PAYLOAD1(STANDARD_VT_1_FHD);
					INPUT_PAYLOAD2(STANDARD_VT_2_FHD);
#if !defined(CONFIG_SUPPORT_DISPLAY_OCTA_TFT)
				} else if (mdnie_tun_state.background == NATURAL_MODE) {
					DPRINT(" = NATURAL MODE =\n");
					INPUT_PAYLOAD1(NATURAL_VT_1_FHD);
					INPUT_PAYLOAD2(NATURAL_VT_2_FHD);
#endif
				} else if (mdnie_tun_state.background == DYNAMIC_MODE) {
					DPRINT(" = DYNAMIC MODE =\n");
					INPUT_PAYLOAD1(DYNAMIC_VT_1_FHD);
					INPUT_PAYLOAD2(DYNAMIC_VT_2_FHD);
				} else if (mdnie_tun_state.background == MOVIE_MODE) {
					DPRINT(" = MOVIE MODE =\n");
					INPUT_PAYLOAD1(MOVIE_VT_1_FHD);
					INPUT_PAYLOAD2(MOVIE_VT_2_FHD);
				} else if (mdnie_tun_state.background == AUTO_MODE) {
					DPRINT(" = AUTO MODE =\n");
					INPUT_PAYLOAD1(AUTO_VT_1_FHD);
					INPUT_PAYLOAD2(AUTO_VT_2_FHD);
				}
				break;
		
#if defined(CONFIG_TDMB)
			case mDNIe_DMB_MODE:
				DPRINT(" = DMB MODE =\n");
				if (mdnie_tun_state.outdoor == OUTDOOR_ON_MODE) {
					DPRINT(" = OUTDOOR ON MODE =\n");
					INPUT_PAYLOAD1(OUTDOOR_DMB_1);
					INPUT_PAYLOAD2(OUTDOOR_DMB_2);
				} else if (mdnie_tun_state.outdoor == OUTDOOR_OFF_MODE) {
					DPRINT(" = OUTDOOR OFF MODE =\n");
					if (mdnie_tun_state.background == STANDARD_MODE) {
						DPRINT(" = STANDARD MODE =\n");
						INPUT_PAYLOAD1(STANDARD_DMB_1);
						INPUT_PAYLOAD2(STANDARD_DMB_2);
#if !defined(CONFIG_SUPPORT_DISPLAY_OCTA_TFT)
					} else if (mdnie_tun_state.background == NATURAL_MODE) {
						DPRINT(" = NATURAL MODE =\n");
						INPUT_PAYLOAD1(NATURAL_DMB_1);
						INPUT_PAYLOAD2(NATURAL_DMB_2);
#endif
					} else if (mdnie_tun_state.background == DYNAMIC_MODE) {
						DPRINT(" = DYNAMIC MODE =\n");
						INPUT_PAYLOAD1(DYNAMIC_DMB_1);
						INPUT_PAYLOAD2(DYNAMIC_DMB_2);
					} else if (mdnie_tun_state.background == MOVIE_MODE) {
						DPRINT(" = MOVIE MODE =\n");
						INPUT_PAYLOAD1(MOVIE_DMB_1);
						INPUT_PAYLOAD2(MOVIE_DMB_2);
					} else if (mdnie_tun_state.background == AUTO_MODE) {
						DPRINT(" = AUTO MODE =\n");
						INPUT_PAYLOAD1(AUTO_DMB_1);
						INPUT_PAYLOAD2(AUTO_DMB_2);
					}
				}
				break;
		
			case mDNIe_DMB_WARM_MODE:
				DPRINT(" = DMB WARM MODE =\n");
				if (mdnie_tun_state.outdoor == OUTDOOR_ON_MODE) {
					DPRINT(" = OUTDOOR ON MODE =\n");
					INPUT_PAYLOAD1(WARM_OUTDOOR_DMB_1);
					INPUT_PAYLOAD2(WARM_OUTDOOR_DMB_2);
				} else if (mdnie_tun_state.outdoor == OUTDOOR_OFF_MODE) {
					DPRINT(" = OUTDOOR OFF MODE =\n");
					INPUT_PAYLOAD1(WARM_DMB_1);
					INPUT_PAYLOAD2(WARM_DMB_2);
				}
				break;
		
			case mDNIe_DMB_COLD_MODE:
				DPRINT(" = DMB COLD MODE =\n");
				if (mdnie_tun_state.outdoor == OUTDOOR_ON_MODE) {
					DPRINT(" = OUTDOOR ON MODE =\n");
					INPUT_PAYLOAD1(COLD_OUTDOOR_DMB_1);
					INPUT_PAYLOAD2(COLD_OUTDOOR_DMB_2);
				} else if (mdnie_tun_state.outdoor == OUTDOOR_OFF_MODE) {
					DPRINT(" = OUTDOOR OFF MODE =\n");
					INPUT_PAYLOAD1(COLD_DMB_1);
					INPUT_PAYLOAD2(COLD_DMB_2);
				}
				break;
#endif
		
			case mDNIe_BROWSER_MODE:
				DPRINT(" = BROWSER MODE =\n");
				if (mdnie_tun_state.background == STANDARD_MODE) {
					DPRINT(" = STANDARD MODE =\n");
					INPUT_PAYLOAD1(STANDARD_BROWSER_1);
					INPUT_PAYLOAD2(STANDARD_BROWSER_2);
#if !defined(CONFIG_SUPPORT_DISPLAY_OCTA_TFT)
				} else if (mdnie_tun_state.background == NATURAL_MODE) {
					DPRINT(" = NATURAL MODE =\n");
					INPUT_PAYLOAD1(NATURAL_BROWSER_1);
					INPUT_PAYLOAD2(NATURAL_BROWSER_2);
#endif
				} else if (mdnie_tun_state.background == DYNAMIC_MODE) {
					DPRINT(" = DYNAMIC MODE =\n");
					INPUT_PAYLOAD1(DYNAMIC_BROWSER_1);
					INPUT_PAYLOAD2(DYNAMIC_BROWSER_2);
				} else if (mdnie_tun_state.background == MOVIE_MODE) {
					DPRINT(" = MOVIE MODE =\n");
					INPUT_PAYLOAD1(MOVIE_BROWSER_1);
					INPUT_PAYLOAD2(MOVIE_BROWSER_2);
				} else if (mdnie_tun_state.background == AUTO_MODE) {
					DPRINT(" = AUTO MODE =\n");
					INPUT_PAYLOAD1(AUTO_BROWSER_1);
					INPUT_PAYLOAD2(AUTO_BROWSER_2);
				}
				break;
		
			case mDNIe_eBOOK_MODE:
				DPRINT(" = eBOOK MODE =\n");
				if (mdnie_tun_state.background == STANDARD_MODE) {
					DPRINT(" = STANDARD MODE =\n");
					INPUT_PAYLOAD1(STANDARD_EBOOK_1);
					INPUT_PAYLOAD2(STANDARD_EBOOK_2);
#if !defined(CONFIG_SUPPORT_DISPLAY_OCTA_TFT)
				} else if (mdnie_tun_state.background == NATURAL_MODE) {
					DPRINT(" = NATURAL MODE =\n");
					INPUT_PAYLOAD1(NATURAL_EBOOK_1);
					INPUT_PAYLOAD2(NATURAL_EBOOK_2);
#endif
				} else if (mdnie_tun_state.background == DYNAMIC_MODE) {
					DPRINT(" = DYNAMIC MODE =\n");
					INPUT_PAYLOAD1(DYNAMIC_EBOOK_1);
					INPUT_PAYLOAD2(DYNAMIC_EBOOK_2);
				} else if (mdnie_tun_state.background == MOVIE_MODE) {
					DPRINT(" = MOVIE MODE =\n");
					INPUT_PAYLOAD1(MOVIE_EBOOK_1);
					INPUT_PAYLOAD2(MOVIE_EBOOK_2);
				} else if (mdnie_tun_state.background == AUTO_MODE) {
					DPRINT(" = AUTO MODE =\n");
					INPUT_PAYLOAD1(AUTO_EBOOK_1);
					INPUT_PAYLOAD2(AUTO_EBOOK_2);
				}
				break;
		
#if !defined(CONFIG_SUPPORT_DISPLAY_OCTA_TFT)
			case mDNIe_EMAIL_MODE:
				DPRINT(" = EMAIL MODE =\n");
				if (mdnie_tun_state.background == STANDARD_MODE) {
					DPRINT(" = STANDARD MODE =\n");
					INPUT_PAYLOAD1(AUTO_EMAIL_1);
					INPUT_PAYLOAD2(AUTO_EMAIL_2);
				} else if (mdnie_tun_state.background == NATURAL_MODE) {
					DPRINT(" = NATURAL MODE =\n");
					INPUT_PAYLOAD1(AUTO_EMAIL_1);
					INPUT_PAYLOAD2(AUTO_EMAIL_2);
				} else if (mdnie_tun_state.background == DYNAMIC_MODE) {
					DPRINT(" = DYNAMIC MODE =\n");
					INPUT_PAYLOAD1(AUTO_EMAIL_1);
					INPUT_PAYLOAD2(AUTO_EMAIL_2);
				} else if (mdnie_tun_state.background == MOVIE_MODE) {
					DPRINT(" = MOVIE MODE =\n");
					INPUT_PAYLOAD1(AUTO_EMAIL_1);
					INPUT_PAYLOAD2(AUTO_EMAIL_2);
				} else if (mdnie_tun_state.background == AUTO_MODE) {
					DPRINT(" = AUTO MODE =\n");
					INPUT_PAYLOAD1(AUTO_EMAIL_1);
					INPUT_PAYLOAD2(AUTO_EMAIL_2);
				}
				break;
#endif
		
			case mDNIE_BLINE_MODE:
				DPRINT(" = BLIND MODE =\n");
				INPUT_PAYLOAD1(COLOR_BLIND_1);
				INPUT_PAYLOAD2(COLOR_BLIND_2);
				break;
		
			default:
				DPRINT("[%s] no option (%d)\n", __func__, mode);
				return;
			}
	 }
#endif	 
	sending_tuning_cmd();
	free_tun_cmd();

	DPRINT("mDNIe_Set_Mode end , mode(%d), background(%d)\n",
		mode, mdnie_tun_state.background);
}
#endif

