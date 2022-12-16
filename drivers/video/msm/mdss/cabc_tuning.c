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
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/ctype.h>
#include <linux/device.h>

#include "mdss_fb.h"
#include "mdss_panel.h"
#include "mdss_dsi.h"

#include "cabc_tuning.h"

//HX8394C
#include "cabc_tuning_data_hx8394c.h"


static char cabc_mode_name[CABC_MODE_MAX][16] = {
	"CABC_OFF_MODE",
	"CABC_UI_MODE",
	"CABC_STILL_MODE",
	"CABC_VIDEO_MODE",
};

static struct cabc_type cabc_state = {
	.enable = 0,
	.mode = CABC_MODE_OFF,
};

static struct mdss_samsung_driver_data *cabc_msd;


// HX8394C
#define CABC_MAX_TUNE_SIZE	2

#define CABC_TUNE_SIZE_1 2
#define CABC_TUNE_SIZE_2 52

#define CABC_PAYLOAD1 cabc_tune_cmd[0]
#define CABC_PAYLOAD2 cabc_tune_cmd[1]

#define CABC_INPUT_PAYLOAD1(x) CABC_PAYLOAD1.payload = x
#define CABC_INPUT_PAYLOAD2(x) CABC_PAYLOAD2.payload = x

static char cabc_tune_data1[CABC_TUNE_SIZE_1] = {0x55,};
static char cabc_tune_data2[CABC_TUNE_SIZE_2] = {0xCA,};

static struct dsi_cmd_desc cabc_tune_cmd[] = {
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 5, sizeof(cabc_tune_data1)}, cabc_tune_data1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 5, sizeof(cabc_tune_data2)}, cabc_tune_data2},
};


static void mdss_dsi_cabc_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_cmd_desc *cmds, int cmds_cnt)
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

static void cabc_sending_tuning_cmd(void)
{
	struct msm_fb_data_type *mfd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;

	mfd = cabc_msd->mfd;
	ctrl_pdata = cabc_msd->ctrl_pdata;

	if (mfd->resume_state == MIPI_SUSPEND_STATE) {
		pr_err("%s : [ERROR] not ST_DSI_RESUME. do not send mipi cmd.\n", __func__);
		return;
	}

	mutex_lock(&cabc_msd->lock);

	mdss_dsi_cabc_cmds_send(ctrl_pdata, cabc_tune_cmd, ARRAY_SIZE(cabc_tune_cmd));

	mutex_unlock(&cabc_msd->lock);
}

static void cabc_free_tune_cmd(void)
{
//CONFIG_FB_MSM_MIPI_HX8394C_PANEL
	memset(cabc_tune_data1, 0, CABC_TUNE_SIZE_1);
	memset(cabc_tune_data2, 0, CABC_TUNE_SIZE_2);
}


void cabc_set_mode(void)
{
	struct msm_fb_data_type *mfd;
	mfd = cabc_msd->mfd;

	if (!mfd) {
		pr_err("%s: [ERROR] mfd is null!\n", __func__);
		return;
	}

	if (mfd->blank_mode) {
		pr_err("%s: [ERROR] blank_mode (%d). do not send mipi cmd.\n", __func__,
				mfd->blank_mode);
		return;
	}

	if (mfd->resume_state == MIPI_SUSPEND_STATE) {
		pr_err("%s: [ERROR] not ST_DSI_RESUME. do not send mipi cmd.\n", __func__);
		return;
	}

	if (!cabc_state.enable) {
		pr_err("%s: [ERROR] CABC engine is OFF.\n", __func__);
		return;
	}

	if (cabc_state.mode < CABC_MODE_OFF || cabc_state.mode  >= CABC_MODE_MAX) {
		pr_err("%s: [ERROR] Undefied CABC MODE value : %d\n\n", __func__, cabc_state.mode );
		return;
	}

#ifdef CONFIG_SAMSUNG_LPM_MODE
	if (poweroff_charging) {
		pr_err("%s: return cabc caused by lpm mode!\n", __func__);
		return;
	}
#endif

	if (!cabc_tune_value[cabc_state.mode][0]) {
			pr_err("%s: tune data is NULL!\n", __func__);
			return;
	} else {
		CABC_INPUT_PAYLOAD1(
			cabc_tune_value[cabc_state.mode][0]);
		CABC_INPUT_PAYLOAD2(
			cabc_tune_value[cabc_state.mode][1]);
	}

	cabc_sending_tuning_cmd();
	cabc_free_tune_cmd();

	pr_info("%s: mode(%d:%s)\n", __func__, cabc_state.mode, cabc_mode_name[cabc_state.mode]);
}


static ssize_t cabc_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int rc;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n", cabc_state.mode);

	pr_info("%s : value=%d name=%s\n", __func__, cabc_state.mode, cabc_mode_name[cabc_state.mode]);

	return rc;
}

static ssize_t cabc_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	int value;
	int backup;

	sscanf(buf, "%d", &value);

	backup = cabc_state.mode;
	cabc_state.mode = value;

	pr_info("%s : (%s) -> (%s)\n",
		__func__, cabc_mode_name[backup], cabc_mode_name[cabc_state.mode]);

	cabc_set_mode();

	return size;
}
static DEVICE_ATTR(cabc, 0664, cabc_show, cabc_store);


static struct class *cabc_class;
extern struct device *tune_mdnie_dev;

void init_cabc_class(void)
{
	if (cabc_state.enable) {
		pr_err("%s : cabc already enable.. \n",__func__);
		return;
	}

	pr_info("%s: +\n", __func__);


	if(!tune_mdnie_dev) {
		cabc_class = class_create(THIS_MODULE, "mdnie");
		if (IS_ERR(cabc_class))
			pr_err("%s : Failed to create class!\n", __func__);

		tune_mdnie_dev =
		    device_create(cabc_class, NULL, 0, NULL,
			  "mdnie");
		if (IS_ERR(tune_mdnie_dev))
			pr_err("%s : Failed to create device!\n", __func__);
	}

	if (device_create_file
		(tune_mdnie_dev, &dev_attr_cabc) < 0)
		pr_err("%s : Failed to create device file(%s)!\n", __func__,
			dev_attr_cabc.attr.name);

	cabc_state.enable = true;

	pr_info("%s: -\n", __func__);
}


void cabc_tuning_init(struct mdss_samsung_driver_data *msd)
{
	cabc_msd = msd;
}

