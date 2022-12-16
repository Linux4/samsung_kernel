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
//#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/poll.h>
//#include <linux/wait.h>
#include <linux/fs.h>
//#include <linux/irq.h>
//#include <linux/mm.h>
//#include <linux/fb.h>
#include <linux/ctype.h>
//#include <linux/miscdevice.h>
//#include <linux/dma-mapping.h>
//#include <linux/delay.h>
#include <linux/device.h>
//#include <linux/fb.h>
//#include <linux/msm_mdp.h>
//#include <linux/ioctl.h>
//#include <linux/lcd.h>

#include "mdss_fb.h"
#include "mdss_panel.h"
#include "mdss_dsi.h"
#include "mdnie_lite_tuning.h"

#define DDI_VIDEO_ENHANCE_TUNING

#if defined(CONFIG_FB_MSM_MIPI_HX8394C_PANEL)
static char tune_data1[MDNIE_TUNE_FIRST_SIZE] = {0xB0, 0x01};
static char tune_data2[MDNIE_TUNE_SECOND_SIZE] = {0xCD,};
static char tune_data3[MDNIE_TUNE_THIRD_SIZE] = {0xB0, 0x00};
static char tune_data4[MDNIE_TUNE_FOURTH_SIZE] = {0xCD,};

static struct dsi_cmd_desc mdnie_tune_cmd[] = {
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(tune_data1)}, tune_data1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(tune_data2)}, tune_data2},
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(tune_data3)}, tune_data3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(tune_data4)}, tune_data4},
};

#if defined(DDI_VIDEO_ENHANCE_TUNING)
static int tune_size_tbl[MAX_TUNE_SIZE] = {
	MDNIE_TUNE_FIRST_SIZE,
	MDNIE_TUNE_SECOND_SIZE,
	MDNIE_TUNE_THIRD_SIZE,
	MDNIE_TUNE_FOURTH_SIZE
};
#endif
#include "mdnie_lite_tuning_data_hx8394c.h"

#elif defined(CONFIG_FB_MSM_MIPI_EA8061V_PANEL)
static char tune_data1[MDNIE_TUNE_FIRST_SIZE] = {0, };
static char tune_data2[MDNIE_TUNE_SECOND_SIZE] = {0, };
static char level1_key[] = {
	0xF0,
	0x5A, 0x5A,
};
static char level2_key[] = {
	0xF1,
	0x5A, 0x5A,
};

static struct dsi_cmd_desc mdnie_tune_cmd[] = {
    {{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
        sizeof(level1_key)}, level1_key},
    {{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
        sizeof(level2_key)}, level2_key},

    {{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
        sizeof(tune_data1)}, tune_data1},
    {{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
        sizeof(tune_data2)}, tune_data2},
};
#if defined(DDI_VIDEO_ENHANCE_TUNING)
static int tune_size_tbl[MAX_TUNE_SIZE] = {
	MDNIE_TUNE_FIRST_SIZE,
	MDNIE_TUNE_SECOND_SIZE
};
#endif
static char tune_data1_adb[MDNIE_TUNE_FIRST_SIZE] = {0,};
static char tune_data2_adb[MDNIE_TUNE_SECOND_SIZE] = {0,};

void copy_tuning_data_from_adb(char *data1, char *data2)
{
	memcpy(tune_data1_adb, data1, MDNIE_TUNE_FIRST_SIZE);
	memcpy(tune_data2_adb, data2, MDNIE_TUNE_SECOND_SIZE);
}
#include "mdnie_lite_tuning_data_ea8061v.h"

#else
static char tune_data1[MDNIE_TUNE_FIRST_SIZE];

static struct dsi_cmd_desc mdnie_tune_cmd[] = {
	{{DTYPE_DCS_WRITE1, 0, 0, 0, 0, sizeof(tune_data1)}, tune_data1},
};

#if defined(DDI_VIDEO_ENHANCE_TUNING)
static int tune_size_tbl[MAX_TUNE_SIZE] = {
	MDNIE_TUNE_FIRST_SIZE,
};
#endif

#include "mdnie_lite_tuning_data.h"
#endif

static struct mdss_samsung_driver_data *mdnie_msd;

#define MDNIE_LITE_TUN_DEBUG

#ifdef MDNIE_LITE_TUN_DEBUG
#define DPRINT(x...)	printk(KERN_ERR "[mdnie lite] " x)
#else
#define DPRINT(x...)
#endif

struct mdnie_lite_tune_type mdnie_tune_state = {
	.mdnie_enable = false,
	.scenario = mDNIe_UI_MODE,
	.background = STANDARD_MODE,
	.outdoor = OUTDOOR_OFF_MODE,
	.accessibility = ACCESSIBILITY_OFF,
};

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
};

const char background_name[MAX_BACKGROUND_MODE][10] = {
	"DYNAMIC",
	"STANDARD",
#if defined(CONFIG_AMOLED)
	"NATURAL",
#endif
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
};

#if defined(DDI_VIDEO_ENHANCE_TUNING)
static void print_tun_data(void)
{
	int i = 0;
	int j = 0;
	DPRINT("\n");
	for(j = 0 ; j < MAX_TUNE_SIZE ; j++) {
		DPRINT("---- size%d : %d", j+1, mdnie_tune_cmd[j].dchdr.dlen);

		for (i = 0; i < tune_size_tbl[j] ; i++) {
			DPRINT("0x%02X\n", mdnie_tune_cmd[j].payload[i]);
		}

		DPRINT("\n");
	}
}
#endif

static void free_tune_cmd(void)
{
#if defined(CONFIG_FB_MSM_MIPI_HX8394C_PANEL)
	memset(tune_data1, 0, MDNIE_TUNE_FIRST_SIZE);
	memset(tune_data2, 0, MDNIE_TUNE_SECOND_SIZE);
	memset(tune_data3, 0, MDNIE_TUNE_THIRD_SIZE);
	memset(tune_data4, 0, MDNIE_TUNE_FOURTH_SIZE);
#elif defined(CONFIG_FB_MSM_MIPI_EA8061V_PANEL)
	memset(tune_data1, 0, MDNIE_TUNE_FIRST_SIZE);
	memset(tune_data2, 0, MDNIE_TUNE_SECOND_SIZE);
#else
	memset(tune_data1, 0, MDNIE_TUNE_FIRST_SIZE);
#endif
}

static void mdss_dsi_mdnie_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_cmd_desc *cmds, int cmds_cnt)
{
	struct dcs_cmd_req cmdreq;

	memset(&cmdreq, 0, sizeof(cmdreq));

	cmdreq.cmds = cmds;
	cmdreq.cmds_cnt = cmds_cnt;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

static void sending_tuning_cmd(void)
{
	struct msm_fb_data_type *mfd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;

	mfd = mdnie_msd->mfd;
	ctrl_pdata = mdnie_msd->ctrl_pdata;

	if (mfd->resume_state == MIPI_SUSPEND_STATE) {
		DPRINT("[ERROR] not ST_DSI_RESUME. do not send mipi cmd.\n");
		return;
	}

	mutex_lock(&mdnie_msd->lock);

	mdss_dsi_mdnie_cmds_send(ctrl_pdata, mdnie_tune_cmd, ARRAY_SIZE(mdnie_tune_cmd));

	mutex_unlock(&mdnie_msd->lock);
}
/*
 * mDnie priority
 * Accessibility > HBM > Screen Mode
 */
void mDNIe_Set_Mode(void)
{
	struct msm_fb_data_type *mfd;
	mfd = mdnie_msd->mfd;

	if (!mfd) {
		DPRINT("[ERROR] mfd is null!\n");
		return;
	}

	if (mfd->blank_mode) {
		DPRINT("[ERROR] blank_mode (%d). do not send mipi cmd.\n",
			mfd->blank_mode);
		return;
	}

	if (mfd->resume_state == MIPI_SUSPEND_STATE) {
		DPRINT("[ERROR] not ST_DSI_RESUME. do not send mipi cmd.\n");
		return;
	}

	if (!mdnie_tune_state.mdnie_enable) {
		DPRINT("[ERROR] mDNIE engine is OFF.\n");
		return;
	}

	if (mdnie_tune_state.scenario < mDNIe_UI_MODE || mdnie_tune_state.scenario >= MAX_mDNIe_MODE) {
		DPRINT("[ERROR] wrong Scenario mode value : %d\n",
			mdnie_tune_state.scenario);
		return;
	}

#ifdef CONFIG_SAMSUNG_LPM_MODE
	if (poweroff_charging) {
		DPRINT("return mdnie caused by lpm mode!\n");
		return;
	}
#endif

	if (mdnie_tune_state.accessibility) {
		DPRINT(" = ACCESSIBILITY MODE =\n");
#if defined(CONFIG_FB_MSM_MIPI_HX8394C_PANEL)
		INPUT_PAYLOAD1(blind_tune_value[mdnie_tune_state.accessibility][0]);
		INPUT_PAYLOAD2(blind_tune_value[mdnie_tune_state.accessibility][1]);
		INPUT_PAYLOAD3(blind_tune_value[mdnie_tune_state.accessibility][2]);
		INPUT_PAYLOAD4(blind_tune_value[mdnie_tune_state.accessibility][3]);
#elif defined(CONFIG_FB_MSM_MIPI_EA8061V_PANEL)
		INPUT_PAYLOAD1(blind_tune_value[mdnie_tune_state.accessibility][0]);
		INPUT_PAYLOAD2(blind_tune_value[mdnie_tune_state.accessibility][1]);
#else
		INPUT_PAYLOAD1(blind_tune_value[mdnie_tune_state.accessibility][0]);
#endif
	}
	else {
#if defined(CONFIG_FB_MSM_MIPI_HX8394C_PANEL)
		if (!mdnie_tune_value[mdnie_tune_state.scenario][mdnie_tune_state.background][mdnie_tune_state.outdoor][0] ||
			!mdnie_tune_value[mdnie_tune_state.scenario][mdnie_tune_state.background][mdnie_tune_state.outdoor][1] ||
			!mdnie_tune_value[mdnie_tune_state.scenario][mdnie_tune_state.background][mdnie_tune_state.outdoor][2] ||
			!mdnie_tune_value[mdnie_tune_state.scenario][mdnie_tune_state.background][mdnie_tune_state.outdoor][3]) {
#elif defined(CONFIG_FB_MSM_MIPI_EA8061V_PANEL)
		if (!mdnie_tune_value[mdnie_tune_state.scenario][mdnie_tune_state.background][mdnie_tune_state.outdoor][0] ||
			!mdnie_tune_value[mdnie_tune_state.scenario][mdnie_tune_state.background][mdnie_tune_state.outdoor][1]) {
#else
		if (!mdnie_tune_value[mdnie_tune_state.scenario][mdnie_tune_state.background][mdnie_tune_state.outdoor][0]) {
#endif
			pr_err("mdnie tune data is NULL!\n");
			return;
		} else {
#if defined(CONFIG_FB_MSM_MIPI_HX8394C_PANEL)
			INPUT_PAYLOAD1(
				mdnie_tune_value[mdnie_tune_state.scenario][mdnie_tune_state.background][mdnie_tune_state.outdoor][0]);
			INPUT_PAYLOAD2(
				mdnie_tune_value[mdnie_tune_state.scenario][mdnie_tune_state.background][mdnie_tune_state.outdoor][1]);
			INPUT_PAYLOAD3(
				mdnie_tune_value[mdnie_tune_state.scenario][mdnie_tune_state.background][mdnie_tune_state.outdoor][2]);
			INPUT_PAYLOAD4(
				mdnie_tune_value[mdnie_tune_state.scenario][mdnie_tune_state.background][mdnie_tune_state.outdoor][3]);
#elif defined(CONFIG_FB_MSM_MIPI_EA8061V_PANEL)
			INPUT_PAYLOAD1(
				mdnie_tune_value[mdnie_tune_state.scenario][mdnie_tune_state.background][mdnie_tune_state.outdoor][0]);
			INPUT_PAYLOAD2(
				mdnie_tune_value[mdnie_tune_state.scenario][mdnie_tune_state.background][mdnie_tune_state.outdoor][1]);
#else
			INPUT_PAYLOAD1(
				mdnie_tune_value[mdnie_tune_state.scenario][mdnie_tune_state.background][mdnie_tune_state.outdoor][0]);
#endif
		}
	}

	sending_tuning_cmd();
	free_tune_cmd();

	DPRINT("%s : %s(%d), %s(%d), %s(%d), %s(%d)\n", __func__,
		scenario_name[mdnie_tune_state.scenario], mdnie_tune_state.scenario,
		background_name[mdnie_tune_state.background], mdnie_tune_state.background,
		outdoor_name[mdnie_tune_state.outdoor], mdnie_tune_state.outdoor,
		accessibility_name[mdnie_tune_state.accessibility], mdnie_tune_state.accessibility);

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
		background_name[mdnie_tune_state.background]);

	return snprintf(buf, 256, "Current Background Mode : %s\n",
		background_name[mdnie_tune_state.background]);
}

static ssize_t mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int value;
	int backup;

	sscanf(buf, "%d", &value);

	if (value < 0 || value >= MAX_BACKGROUND_MODE) {
		DPRINT("[ERROR] wrong backgound mode value : %d\n",
			value);
		return size;
	}
	backup = mdnie_tune_state.background;
	mdnie_tune_state.background = value;

	if (mdnie_tune_state.accessibility == NEGATIVE) {
		DPRINT("already negative mode(%d), do not set background(%d)\n",
			mdnie_tune_state.accessibility, mdnie_tune_state.background);
	} else {
		DPRINT(" %s : (%s) -> (%s)\n",
			__func__, background_name[backup], background_name[mdnie_tune_state.background]);

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
		scenario_name[mdnie_tune_state.scenario]);

	return snprintf(buf, 256, "Current Scenario Mode : %s\n",
		scenario_name[mdnie_tune_state.scenario]);
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

	backup = mdnie_tune_state.scenario;
	mdnie_tune_state.scenario = value;

	if (mdnie_tune_state.accessibility == NEGATIVE) {
		DPRINT("already negative mode(%d), do not set mode(%d)\n",
			mdnie_tune_state.accessibility, mdnie_tune_state.scenario);
	} else {
		DPRINT(" %s : (%s) -> (%s)\n",
			__func__, scenario_name[backup], scenario_name[mdnie_tune_state.scenario]);
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
		mdnie_tune_state.scenario = mDNIe_UI_MODE;
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
		outdoor_name[mdnie_tune_state.outdoor]);

	return snprintf(buf, 256, "Current outdoor Mode : %s\n",
		outdoor_name[mdnie_tune_state.outdoor]);
}

static ssize_t outdoor_store(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t size)
{
	int value;
	int backup;

	sscanf(buf, "%d", &value);

	DPRINT("outdoor value = %d, scenario = %d\n",
		value, mdnie_tune_state.scenario);

	if (value < OUTDOOR_OFF_MODE || value >= MAX_OUTDOOR_MODE) {
		DPRINT("[ERROR] : wrong outdoor mode value : %d\n",
				value);
		return size;
	}

	backup = mdnie_tune_state.outdoor;
	mdnie_tune_state.outdoor = value;

	if (mdnie_tune_state.accessibility == NEGATIVE) {
		DPRINT("already negative mode(%d), do not outdoor mode(%d)\n",
			mdnie_tune_state.accessibility, mdnie_tune_state.outdoor);
	} else {
		DPRINT(" %s : (%s) -> (%s)\n",
			__func__, outdoor_name[backup], outdoor_name[mdnie_tune_state.outdoor]);
		mDNIe_Set_Mode();
	}

	return size;
}

static DEVICE_ATTR(outdoor, 0664, outdoor_show, outdoor_store);


static ssize_t accessibility_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	DPRINT("Current accessibility Mode : %s\n",
		accessibility_name[mdnie_tune_state.accessibility]);

	return snprintf(buf, 256, "Current accessibility Mode : %s\n",
		accessibility_name[mdnie_tune_state.accessibility]);
}

static ssize_t accessibility_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	int cmd_value = 0;
	int backup = 0;
	char buffer[MDNIE_COLOR_BLINDE_CMD] = {0,};
	int buffer2[MDNIE_COLOR_BLINDE_CMD/2] = {0,};
	int loop;
	char temp;

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

	backup = mdnie_tune_state.accessibility;

	if (cmd_value == NEGATIVE) {
		mdnie_tune_state.accessibility = NEGATIVE;
	} else if (cmd_value == COLOR_BLIND) {
		mdnie_tune_state.accessibility = COLOR_BLIND;
#if defined(CONFIG_FB_MSM_MIPI_HX8394C_PANEL)
		memcpy(&COLOR_BLIND_4[22],buffer, MDNIE_COLOR_BLINDE_CMD);
#endif
	} else if (cmd_value == ACCESSIBILITY_OFF) {
		mdnie_tune_state.accessibility = ACCESSIBILITY_OFF;
	} else
		pr_info("%s ACCESSIBILITY_MAX", __func__);

	DPRINT(" %s : (%s) -> (%s)\n",
			__func__, accessibility_name[backup], accessibility_name[mdnie_tune_state.accessibility]);

	mDNIe_Set_Mode();

	pr_info("%s cmd_value : %d size : %d", __func__, cmd_value, size);

	return size;
}

static DEVICE_ATTR(accessibility, 0664,
			accessibility_show,
			accessibility_store);
void is_negative_on(void)
{
	pr_info("negative mode = %d\n", mdnie_tune_state.accessibility);
	mDNIe_Set_Mode();
}
#if defined(DDI_VIDEO_ENHANCE_TUNING)
#define MAX_FILE_NAME	128
#define TUNING_FILE_PATH "/sdcard/"
static char tuning_file[MAX_FILE_NAME];

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
	int data_pos = 0;
	int cmd_step = 0;
	int cmd_pos = 0;
	int tbl_cnt = 0;

	for (data_pos = 0; data_pos < len;) {
		if (*(src + data_pos) == '0') {
			if (*(src + data_pos + 1) == 'x') {

#if defined(CONFIG_FB_MSM_MIPI_HX8394C_PANEL)
				if (cmd_step == 0)
					tune_data1[cmd_pos] = char_to_dec(*(src + data_pos + 2), *(src + data_pos + 3));
				else if (cmd_step == 1)
					tune_data2[cmd_pos] = char_to_dec(*(src + data_pos + 2), *(src + data_pos + 3));
				else if (cmd_step == 2)
					tune_data3[cmd_pos] = char_to_dec(*(src + data_pos + 2), *(src + data_pos + 3));
				else if (cmd_step == 3)
					tune_data4[cmd_pos] = char_to_dec(*(src + data_pos + 2), *(src + data_pos + 3));
#elif defined(CONFIG_FB_MSM_MIPI_EA8061V_PANEL)
				if (cmd_step == 0)
					tune_data1[cmd_pos] = char_to_dec(*(src + data_pos + 2), *(src + data_pos + 3));
				else if (cmd_step == 1)
					tune_data2[cmd_pos] = char_to_dec(*(src + data_pos + 2), *(src + data_pos + 3));
#else
				if (cmd_step == 0)
					tune_data1[cmd_pos] = char_to_dec(*(src + data_pos + 2), *(src + data_pos + 3));
#endif

				data_pos += 3;
				cmd_pos++;

				if (cmd_pos == tune_size_tbl[tbl_cnt] && cmd_step == tbl_cnt) {
					cmd_pos = 0;
					cmd_step++;
					tbl_cnt++;
				}
			} else {
				data_pos++;
			}
		} else {
			data_pos++;
		}
	}

#if defined(CONFIG_FB_MSM_MIPI_HX8394C_PANEL)
	INPUT_PAYLOAD1(tune_data1);
	INPUT_PAYLOAD2(tune_data2);
	INPUT_PAYLOAD3(tune_data3);
	INPUT_PAYLOAD4(tune_data4);
#elif defined(CONFIG_FB_MSM_MIPI_EA8061V_PANEL)
	INPUT_PAYLOAD1(tune_data1);
	INPUT_PAYLOAD2(tune_data2);
#else
	INPUT_PAYLOAD1(tune_data1);
#endif

	print_tun_data();

	sending_tuning_cmd();
	free_tune_cmd();
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

	ret = snprintf(buf, MAX_FILE_NAME, "tuned file name : %s\n", tuning_file);

	return ret;
}

static ssize_t tuning_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t size)
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

	DPRINT("%s\n", tuning_file);

	load_tuning_file(tuning_file);

	return size;
}
static DEVICE_ATTR(tuning, S_IRUGO | S_IWUSR | S_IWGRP,
						tuning_show,
						tuning_store);

#endif /* DDI_VIDEO_ENHANCE_TUNING */


static struct class *mdnie_class;
struct device *tune_mdnie_dev;

void init_mdnie_class(void)
{
	if (mdnie_tune_state.mdnie_enable) {
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

	if (device_create_file
		(tune_mdnie_dev, &dev_attr_accessibility) < 0)
		pr_err("Failed to create device file(%s)!=n",
			dev_attr_accessibility.attr.name);

#if defined(DDI_VIDEO_ENHANCE_TUNING)
	if (device_create_file
		(tune_mdnie_dev, &dev_attr_tuning) < 0)
		pr_err("Failed to create device file(%s)!=n",
			dev_attr_tuning.attr.name);
#endif

	mdnie_tune_state.mdnie_enable = true;

	DPRINT("end!\n");
}


void mdnie_lite_tuning_init(struct mdss_samsung_driver_data *msd)
{
	mdnie_msd = msd;
}

