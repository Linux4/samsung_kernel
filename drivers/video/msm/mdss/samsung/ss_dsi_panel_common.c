/*
 * =================================================================
 *
 *
 *	Description:  samsung display common file
 *
 *	Author: jb09.kim
 *	Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2012, Samsung Electronics. All rights reserved.

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
#include "ss_dsi_panel_common.h"
#include "../mdss_debug.h"

static void mdss_samsung_event_osc_te_fitting(struct mdss_panel_data *pdata, int event, void *arg);
static irqreturn_t samsung_te_check_handler(int irq, void *handle);
static void samsung_te_check_done_work(struct work_struct *work);
static void mdss_samsung_event_esd_recovery_init(struct mdss_panel_data *pdata);
struct samsung_display_driver_data vdd_data;

struct dsi_cmd_desc default_cmd = {{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL};

#if defined(CONFIG_LCD_CLASS_DEVICE)
/* SYSFS RELATED VALUE */
#define MAX_FILE_NAME 128
#define TUNING_FILE_PATH "/sdcard/"
static char tuning_file[MAX_FILE_NAME];
#endif
u8 csc_update = 1;
u8 csc_change = 0;

struct mdss_panel_data *pdata_dsi0;
struct mdss_panel_data *pdata_dsi1;
struct work_struct esd_irq_work;
struct samsung_display_driver_data * check_valid_ctrl(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = NULL;

	if (IS_ERR_OR_NULL(ctrl)) {
		pr_err("%s: Invalid data ctrl : 0x%zx\n", __func__, (size_t)ctrl);
		return NULL;
	}

	vdd = (struct samsung_display_driver_data *)ctrl->panel_data.panel_private;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid vdd_data : 0x%zx\n", __func__, (size_t)vdd);
		return NULL;
	}

	return vdd;
}

char mdss_panel_id0_get(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return -ERANGE;
	}

	return (vdd->manufacture_id_dsi[ctrl->ndx] & 0xFF0000) >> 16;
}

char mdss_panel_id1_get(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return -ERANGE;
	}

	return (vdd->manufacture_id_dsi[ctrl->ndx] & 0xFF00) >> 8;
}

char mdss_panel_id2_get(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return -ERANGE;
	}

	return vdd->manufacture_id_dsi[ctrl->ndx] & 0xFF;
}

int mdss_panel_attach_get(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return -ERANGE;
	}

	return (vdd->panel_attach_status & (0x01 << ctrl->ndx)) > 0 ? true : false;
}

int mdss_panel_attach_set(struct mdss_dsi_ctrl_pdata *ctrl, int status)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return -ERANGE;
	}

	/* 0bit->DSI0 1bit->DSI1 */
	if (likely(get_lcd_attached("GET")) && status) {
		if (ctrl->cmd_sync_wait_broadcast)
			vdd->panel_attach_status |= (BIT(1) | BIT(0));
		else
			vdd->panel_attach_status |= BIT(0) << ctrl->ndx;
	} else {
		if (ctrl->cmd_sync_wait_broadcast)
			vdd->panel_attach_status &= ~(BIT(1) | BIT(0));
		else
			vdd->panel_attach_status &= ~(BIT(0) << ctrl->ndx);
	}

	pr_info("%s panel_attach_status : %d\n", __func__, vdd->panel_attach_status);

	return vdd->panel_attach_status;
}

int get_lcd_attached(char *mode)
{
	char *pt;
	static int lcd_id = 0;

	pr_debug("%s: %s", __func__, mode);

	if (mode == NULL)
		return true;

	if (!strncmp(mode, "GET", 3)) {
		return lcd_id;
	} else {
		for (pt = mode; *pt != 0; pt++)  {
			lcd_id <<= 4;
			switch (*pt) {
			case '0' ... '9':
				lcd_id += *pt - '0';
				break;
			case 'a' ... 'f':
				lcd_id += 10 + *pt - 'a';
				break;
			case 'A' ... 'F':
				lcd_id += 10 + *pt - 'A';
				break;
			}
		}
		pr_info("%s: LCD_ID = 0x%X", __func__, lcd_id);
	}

	return lcd_id;
}
EXPORT_SYMBOL(get_lcd_attached);
__setup("lcd_id=0x", get_lcd_attached);

static void mdss_samsung_event_frame_update(struct mdss_panel_data *pdata, int event, void *arg)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)pdata->panel_private;
	struct panel_func *panel_func = NULL;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	panel_func = &vdd->panel_func;

	if (ctrl->cmd_sync_wait_broadcast) {
		if (ctrl->cmd_sync_wait_trigger) {
			if(vdd->display_ststus_dsi[ctrl->ndx].wait_disp_on) {
				mdss_samsung_send_cmd(ctrl, PANEL_DISPLAY_ON);
				vdd->display_ststus_dsi[ctrl->ndx].wait_disp_on = 0;

				if (vdd->dtsi_data[0].hmt_enabled) {
					if (!vdd->hmt_stat.hmt_is_first) {
						if (vdd->hmt_stat.hmt_on) {
							if (vdd->hmt_stat.hmt_low_persistence) {
								vdd->hmt_stat.hmt_enable(ctrl, 1);
								vdd->hmt_stat.hmt_reverse_update(ctrl, 1);
							}
							vdd->hmt_stat.hmt_bright_update(ctrl);
						}
					} else
						vdd->hmt_stat.hmt_is_first = 0;
				}
				pr_info("DISPLAY_ON\n");
			}
		} else
			vdd->display_ststus_dsi[ctrl->ndx].wait_disp_on = 0;
	} else {
		/* Check TE duration when the panel turned on */
		/*
		if (vdd->display_ststus_dsi[ctrl->ndx].wait_disp_on) {
			vdd->te_fitting_info.status &= ~TE_FITTING_DONE;
			vdd->te_fitting_info.te_duration = 0;
		}
		*/

		if (vdd->dtsi_data[ctrl->ndx].samsung_osc_te_fitting &&
				!(vdd->te_fitting_info.status & TE_FITTING_DONE)) {
				if (panel_func->mdss_samsung_event_osc_te_fitting)
			panel_func->mdss_samsung_event_osc_te_fitting(pdata, event, arg);
		}

		if (vdd->display_ststus_dsi[ctrl->ndx].wait_disp_on) {
			mdss_samsung_send_cmd(ctrl, PANEL_DISPLAY_ON);
			vdd->display_ststus_dsi[ctrl->ndx].wait_disp_on = 0;

			if (vdd->dtsi_data[0].hmt_enabled) {
				if (!vdd->hmt_stat.hmt_is_first) {
					if (vdd->hmt_stat.hmt_on) {
						if (vdd->hmt_stat.hmt_low_persistence) {
							vdd->hmt_stat.hmt_enable(ctrl, 1);
							vdd->hmt_stat.hmt_reverse_update(ctrl, 1);
						}
						vdd->hmt_stat.hmt_bright_update(ctrl);
					}
				} else
					vdd->hmt_stat.hmt_is_first = 0;
			}
			pr_info("DISPLAY_ON\n");
		}
	}
}

static void mdss_samsung_event_fb_event_callback(struct mdss_panel_data *pdata, int event, void *arg)
{
	struct samsung_display_driver_data *vdd = NULL;
	struct panel_func *panel_func = NULL;

	if (IS_ERR_OR_NULL(pdata)) {
		pr_err("%s: Invalid data pdata : 0x%zx\n",
				__func__, (size_t)pdata);
		return;
	}

	vdd = (struct samsung_display_driver_data *)pdata->panel_private;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd : 0x%zx\n", __func__, (size_t)vdd);
		return;
	}

	panel_func = &vdd->panel_func;

	if (IS_ERR_OR_NULL(panel_func)) {
		pr_err("%s: Invalid data panel_func : 0x%zx\n",
				__func__, (size_t)panel_func);
		return;
	}

}


static int mdss_samsung_dsi_panel_event_handler(
		struct mdss_panel_data *pdata, int event, void *arg)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)pdata->panel_private;
	struct panel_func *panel_func = NULL;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n",
				__func__, (size_t)ctrl, (size_t)vdd);
		return -EINVAL;
	}

	pr_debug("%s : %d\n", __func__, event);

	panel_func = &vdd->panel_func;

	if (IS_ERR_OR_NULL(panel_func)) {
		pr_err("%s: Invalid data panel_func : 0x%zx\n", __func__, (size_t)panel_func);
		return -EINVAL;
	}

	switch (event) {
		case MDSS_SAMSUNG_EVENT_FRAME_UPDATE:
			if (!IS_ERR_OR_NULL(panel_func->mdss_samsung_event_frame_update))
				panel_func->mdss_samsung_event_frame_update(pdata, event, arg);
			break;
		case MDSS_SAMSUNG_EVENT_FB_EVENT_CALLBACK:
			if (!IS_ERR_OR_NULL(panel_func->mdss_samsung_event_fb_event_callback))
				panel_func->mdss_samsung_event_fb_event_callback(pdata, event, arg);
			break;
		default:
			pr_debug("%s: unhandled event=%d\n", __func__, event);
			break;
	}

	return 0;
}

void mdss_samsung_panel_init(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	/* At this time ctrl_pdata->ndx is not set */
	struct mdss_panel_info *pinfo = NULL;
	int ndx = ctrl_pdata->panel_data.panel_info.pdest;
	int loop;

	pr_info("%s ++\n", __func__);

	ctrl_pdata->panel_data.panel_private = &vdd_data;

	mutex_init(&vdd_data.vdd_lock);

	vdd_data.ctrl_dsi[ndx] = ctrl_pdata;

	pinfo = &ctrl_pdata->panel_data.panel_info;

	/* Set default link_state of brightness command */
	vdd_data.brightness[0].brightness_packet_tx_cmds_dsi.link_state = DSI_HS_MODE;

	if (pinfo && pinfo->mipi.mode == DSI_CMD_MODE) {
		vdd_data.panel_func.mdss_samsung_event_osc_te_fitting =
			mdss_samsung_event_osc_te_fitting;
	}

	vdd_data.panel_func.mdss_samsung_event_frame_update =
		mdss_samsung_event_frame_update;
	vdd_data.panel_func.mdss_samsung_event_fb_event_callback =
		mdss_samsung_event_fb_event_callback;

	if (IS_ERR_OR_NULL(vdd_data.panel_func.samsung_panel_init))
		pr_err("%s samsung_panel_init is error", __func__);
	else
		vdd_data.panel_func.samsung_panel_init(&vdd_data);

	vdd_data.ctrl_dsi[ndx]->event_handler = mdss_samsung_dsi_panel_event_handler;

	if (vdd_data.support_mdnie_lite)
		vdd_data.mdnie_tune_state_dsi[ndx] = init_dsi_tcon_mdnie_class(ndx, &vdd_data);

	/*
		Below for loop are same as initializing sturct brightenss_pasket_dsi.
	vdd_data.brightness[ndx].brightness_packet_dsi[0] = {{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL};
	vdd_data.brightness[ndx].brightness_packet_dsi[1] = {{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL};
	...
	vdd_data.brightness[ndx].brightness_packet_dsi[BRIGHTNESS_MAX_PACKET - 2] = {{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL};
	vdd_data.brightness[ndx].brightness_packet_dsi[BRIGHTNESS_MAX_PACKET - 1 ] = {{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL};
	*/
	for (loop = 0; loop < BRIGHTNESS_MAX_PACKET; loop++)
		vdd_data.brightness[ndx].brightness_packet_dsi[loop] = default_cmd;

	vdd_data.brightness[ndx].brightness_packet_tx_cmds_dsi.cmds = &vdd_data.brightness[ndx].brightness_packet_dsi[0];
	vdd_data.brightness[ndx].brightness_packet_tx_cmds_dsi.cmd_cnt = 0;

	spin_lock_init(&vdd_data.esd_recovery.irq_lock);

	vdd_data.hmt_stat.hmt_enable = hmt_enable;
	vdd_data.hmt_stat.hmt_reverse_update = hmt_reverse_update;
	vdd_data.hmt_stat.hmt_bright_update = hmt_bright_update;

	mdss_panel_attach_set(ctrl_pdata, true);

	mdss_samsung_event_esd_recovery_init(&ctrl_pdata->panel_data);
	/* Set init brightness level */
	vdd_data.init_bl_level = DEFAULT_BRIGHTNESS;
}

void mdss_samsung_dsi_panel_registered(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)pdata->panel_private;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return ;
	}

	vdd->mfd_dsi[ctrl->ndx] = (struct msm_fb_data_type *)registered_fb[ctrl->ndx]->par;

	mdss_samsung_create_sysfs(vdd);
	pr_info("%s DSI%d success", __func__, ctrl->ndx);
}

int mdss_samsung_parse_candella_lux_mapping_table(struct device_node *np,
		struct candella_lux_map *table, char *keystring)
{
	const __be32 *data;
	int  data_offset, len = 0 , i = 0;
	int  cdmap_start=0, cdmap_end=0;

	data = of_get_property(np, keystring, &len);
	if (!data) {
		pr_debug("%s:%d, Unable to read table %s ", __func__, __LINE__, keystring);
		return -EINVAL;
	} else
		pr_err("%s:Success to read table %s\n", __func__, keystring);

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
		} while (cdmap_start <= cdmap_end);
	}
	return 0;
error:
	kfree(table->lux_tab);

	return -ENOMEM;
}

int mdss_samsung_parse_panel_table(struct device_node *np,
		struct cmd_map *table, char *keystring)
{
	const __be32 *data;
	int  data_offset, len = 0 , i = 0;

	data = of_get_property(np, keystring, &len);
	if (!data) {
		pr_debug("%s:%d, Unable to read table %s ", __func__, __LINE__, keystring);
		return -EINVAL;
	} else
		pr_err("%s:Success to read table %s\n", __func__, keystring);

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

static int samsung_nv_read(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *cmds, unsigned char *destBuffer)
{
	int loop_limit = 0;
	int read_pos = 0;
	int read_count = 0;
	int show_cnt;
	int i, j;
	char show_buffer[256] = {0,};
	int show_buffer_pos = 0;

	int read_size = 0;
	int srcLength = 0;
	int startoffset = 0;
	struct samsung_display_driver_data *vdd = NULL;
	int ndx = 0;
	int packet_size = 0;

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(cmds)) {
		pr_err("%s: Invalid ctrl data\n", __func__);
		return -EINVAL;
	}

	vdd = (struct samsung_display_driver_data *)ctrl->panel_data.panel_private;
	ndx = ctrl->panel_data.panel_info.pdest;
	packet_size = vdd->dtsi_data[ndx].packet_size_tx_cmds[vdd->panel_revision].cmds->payload[0];

	srcLength = cmds->read_size[0];
	startoffset = read_pos = cmds->read_startoffset[0];

	show_buffer_pos += snprintf(show_buffer, 256, "read_reg : %X[%d] : ", cmds->cmds->payload[0], srcLength);

	loop_limit = (srcLength + packet_size - 1) / packet_size;
	mdss_samsung_send_cmd(ctrl, PANEL_PACKET_SIZE);

	show_cnt = 0;
	for (j = 0; j < loop_limit; j++) {
		vdd->dtsi_data[ndx].reg_read_pos_tx_cmds[vdd->panel_revision].cmds->payload[1] = read_pos;
		read_size = ((srcLength - read_pos + startoffset) < packet_size) ?
					(srcLength - read_pos + startoffset) : packet_size;
		mdss_samsung_send_cmd(ctrl, PANEL_REG_READ_POS);

		mutex_lock(&vdd->vdd_lock);
		read_count = mdss_samsung_panel_cmd_read(ctrl, cmds, read_size);
		mutex_unlock(&vdd->vdd_lock);

		for (i = 0; i < read_count; i++, show_cnt++) {
			show_buffer_pos += snprintf(show_buffer + show_buffer_pos, 256, "%02x ",
					ctrl->rx_buf.data[i]);
			if (destBuffer != NULL && show_cnt < srcLength) {
					destBuffer[show_cnt] =
					ctrl->rx_buf.data[i];
			}
		}

		show_buffer_pos += snprintf(show_buffer + show_buffer_pos, 256, ".");
		read_pos += read_count;

		if (read_pos-startoffset >= srcLength)
			break;
	}

	pr_err("DSI%d %s\n", ctrl->ndx, show_buffer);

	return read_pos-startoffset;
}

struct dsi_panel_cmds *mdss_samsung_cmds_select(struct mdss_dsi_ctrl_pdata *ctrl, enum mipi_samsung_cmd_list cmd)
{
	struct dsi_panel_cmds *cmds = NULL;
	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)ctrl->panel_data.panel_private;

	if (IS_ERR_OR_NULL(vdd))
		return NULL;

	switch (cmd) {
		case PANEL_DISPLAY_ON:
			cmds = &vdd->dtsi_data[ctrl->ndx].display_on_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_DISPLAY_OFF:
			cmds = &vdd->dtsi_data[ctrl->ndx].display_off_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_BRIGHT_CTRL:
			cmds = &vdd->brightness[ctrl->ndx].brightness_packet_tx_cmds_dsi;
			break;
		case PANEL_LEVE1_KEY_ENABLE:
			cmds = &vdd->dtsi_data[ctrl->ndx].level1_key_enable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LEVE1_KEY_DISABLE:
			cmds = &vdd->dtsi_data[ctrl->ndx].level1_key_disable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LEVE2_KEY_ENABLE:
			cmds = &vdd->dtsi_data[ctrl->ndx].level2_key_enable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LEVE2_KEY_DISABLE:
			cmds = &vdd->dtsi_data[ctrl->ndx].level2_key_disable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_PACKET_SIZE:
			cmds = &vdd->dtsi_data[ctrl->ndx].packet_size_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_REG_READ_POS:
			cmds = &vdd->dtsi_data[ctrl->ndx].reg_read_pos_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_MDNIE_TUNE:
			cmds = &vdd->mdnie_tune_data[ctrl->ndx].mdnie_tune_packet_tx_cmds_dsi;
			break;
		case PANEL_OSC_TE_FITTING:
			cmds = &vdd->dtsi_data[ctrl->ndx].osc_te_fitting_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LDI_FPS_CHANGE:
			cmds = &vdd->dtsi_data[ctrl->ndx].ldi_fps_change_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_HMT_ENABLE:
			cmds = &vdd->dtsi_data[ctrl->ndx].hmt_enable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_HMT_DISABLE:
			cmds = &vdd->dtsi_data[ctrl->ndx].hmt_disable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_HMT_REVERSE_ENABLE:
			cmds = &vdd->dtsi_data[ctrl->ndx].hmt_reverse_enable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_HMT_REVERSE_DISABLE:
			cmds = &vdd->dtsi_data[ctrl->ndx].hmt_reverse_disable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LDI_SET_VDD_OFFSET:
			cmds = &vdd->dtsi_data[ctrl->ndx].write_vdd_offset_cmds[vdd->panel_revision];
			break;
		case PANEL_LDI_SET_VDDM_OFFSET:
			cmds = &vdd->dtsi_data[ctrl->ndx].write_vddm_offset_cmds[vdd->panel_revision];
			break;
		case PANEL_ALPM_ON:
			cmds = &vdd->dtsi_data[ctrl->ndx].alpm_on_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_ALPM_OFF:
			cmds = &vdd->dtsi_data[ctrl->ndx].alpm_off_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_HSYNC_ON:
			cmds = &vdd->dtsi_data[ctrl->ndx].hsync_on_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_CABC_ON:
			cmds = &vdd->dtsi_data[ctrl->ndx].cabc_on_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_CABC_OFF:
			cmds = &vdd->dtsi_data[ctrl->ndx].cabc_off_tx_cmds[vdd->panel_revision];
			break;
		default:
			pr_err("%s : unknown_command.. \n", __func__);
			break;
	}

	return cmds;
}

int mdss_samsung_send_cmd(struct mdss_dsi_ctrl_pdata *ctrl, enum mipi_samsung_cmd_list cmd)
{
	struct mdss_panel_info *pinfo = NULL;
	struct samsung_display_driver_data *vdd = NULL;

	if (!mdss_panel_attach_get(ctrl)) {
		pr_err("%s: mdss_panel_attach_get(%d) : %d\n",__func__, ctrl->ndx, mdss_panel_attach_get(ctrl));
		return -EAGAIN;
	}

	vdd = (struct samsung_display_driver_data *)ctrl->panel_data.panel_private;
	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Skip to tx command %d\n", __func__, __LINE__);
		return 0;
	}

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo->blank_state == MDSS_PANEL_BLANK_BLANK) {
		pr_err("%s: Skip to tx command %d\n", __func__, __LINE__);
		return 0;
	}

	/* To check registered FB */
	if (IS_ERR_OR_NULL(vdd->mfd_dsi[ctrl->ndx])) {
		/* Do not send any CMD data under FB_BLANK_POWERDOWN condition*/
		if (vdd->vdd_blank_mode[DISPLAY_1] == FB_BLANK_POWERDOWN) {
			pr_err("%s: Skip to tx command %d\n", __func__, __LINE__);
			return 0;
		}
	} else {
		/* Do not send any CMD data under FB_BLANK_POWERDOWN condition*/
		if (vdd->vdd_blank_mode[ctrl->ndx] == FB_BLANK_POWERDOWN) {
			pr_err("%s: Skip to tx command %d\n", __func__, __LINE__);
			return 0;
		}
	}

	mutex_lock(&vdd->vdd_lock);
	mdss_dsi_panel_cmds_send(ctrl, mdss_samsung_cmds_select(ctrl, cmd));
	mutex_unlock(&vdd->vdd_lock);

	return 0;
}

static int mdss_samsung_read_nv_mem(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *cmds, unsigned char *buffer, int level_key)
{
	int nv_read_cnt = 0;

	if (level_key & PANEL_LEVE1_KEY)
		mdss_samsung_send_cmd(ctrl, PANEL_LEVE1_KEY_ENABLE);
	if (level_key & PANEL_LEVE2_KEY)
		mdss_samsung_send_cmd(ctrl, PANEL_LEVE2_KEY_ENABLE);

	nv_read_cnt = samsung_nv_read(ctrl, cmds, buffer);

	if (level_key & PANEL_LEVE1_KEY)
		mdss_samsung_send_cmd(ctrl, PANEL_LEVE1_KEY_DISABLE);
	if (level_key & PANEL_LEVE2_KEY)
		mdss_samsung_send_cmd(ctrl, PANEL_LEVE2_KEY_DISABLE);

	return nv_read_cnt;
}

void mdss_samsung_panel_data_read(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *cmds, char *buffer, int level_key)
{
	if (!ctrl) {
		pr_err("%s: Invalid ctrl data\n", __func__);
		return ;
	}

	if (!mdss_panel_attach_get(ctrl)) {
		pr_err("%s: mdss_panel_attach_get(%d) : %d\n",__func__, ctrl->ndx, mdss_panel_attach_get(ctrl));
		return ;
	}

	if (!cmds->cmd_cnt) {
		pr_err("%s : cmds_count is zero..\n",__func__);
		return ;
	}

	mdss_samsung_read_nv_mem(ctrl, cmds, buffer, level_key);
}

static unsigned char mdss_samsung_manufacture_id(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *cmds)
{
	char manufacture_id = 0;

	mdss_samsung_panel_data_read(ctrl, cmds, &manufacture_id, PANEL_LEVE1_KEY);

	return manufacture_id;
}

int mdss_samsung_panel_on_pre(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)pdata->panel_private;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	mdss_panel_attach_set(ctrl, true);

	if (!mdss_panel_attach_get(ctrl)) {
		pr_err("%s: mdss_panel_attach_get(%d) : %d\n",__func__, ctrl->ndx, mdss_panel_attach_get(ctrl));
		return false;
	}

	pr_info("%s+: ndx=%d \n", __func__, ctrl->ndx);

	if (!vdd->manufacture_id_dsi[ctrl->ndx]) {
		/*
		*	At this time, panel revision it not selected.
		*	So last index(SUPPORT_PANEL_REVISION-1) used.
		*/
		vdd->panel_revision = SUPPORT_PANEL_REVISION - 1;

		/*
		*	Some panel needs to update register at init time to read ID & MTP
		*	Such as, dual-dsi-control or sleep-out so on.
		*/
		if (!IS_ERR_OR_NULL(vdd->dtsi_data[ctrl->ndx].manufacture_read_pre_tx_cmds[vdd->panel_revision].cmds)) {
			mutex_lock(&vdd->vdd_lock);
			mdss_dsi_panel_cmds_send(ctrl, &vdd->dtsi_data[ctrl->ndx].manufacture_read_pre_tx_cmds[vdd->panel_revision]);
			mutex_unlock(&vdd->vdd_lock);
			pr_err("%s DSI%d manufacture_read_pre_tx_cmds ", __func__, ctrl->ndx);
		}

		if (!IS_ERR_OR_NULL(vdd->dtsi_data[ctrl->ndx].manufacture_id0_rx_cmds[vdd->panel_revision].cmds)) {
			vdd->manufacture_id_dsi[ctrl->ndx] = mdss_samsung_manufacture_id(ctrl, &vdd->dtsi_data[ctrl->ndx].manufacture_id0_rx_cmds[SUPPORT_PANEL_REVISION-1]);
			vdd->manufacture_id_dsi[ctrl->ndx] <<= 8;
		} else
			pr_err("%s DSI%d manufacture_id0_rx_cmds NULL", __func__, ctrl->ndx);

		if (!IS_ERR_OR_NULL(vdd->dtsi_data[ctrl->ndx].manufacture_id1_rx_cmds[vdd->panel_revision].cmds)) {
			vdd->manufacture_id_dsi[ctrl->ndx] |= mdss_samsung_manufacture_id(ctrl, &vdd->dtsi_data[ctrl->ndx].manufacture_id1_rx_cmds[SUPPORT_PANEL_REVISION-1]);
			vdd->manufacture_id_dsi[ctrl->ndx] <<= 8;
		} else
			pr_err("%s DSI%d manufacture_id1_rx_cmds NULL", __func__, ctrl->ndx);

		if (!IS_ERR_OR_NULL(vdd->dtsi_data[ctrl->ndx].manufacture_id2_rx_cmds[vdd->panel_revision].cmds))
			vdd->manufacture_id_dsi[ctrl->ndx] |= mdss_samsung_manufacture_id(ctrl, &vdd->dtsi_data[ctrl->ndx].manufacture_id2_rx_cmds[SUPPORT_PANEL_REVISION-1]);
		else
			pr_err("%s DSI%d manufacture_id2_rx_cmds NULL", __func__, ctrl->ndx);


		pr_info("%s: DSI%d manufacture_id=0x%x\n", __func__, ctrl->ndx, vdd->manufacture_id_dsi[ctrl->ndx]);

		/* Panel revision selection */
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_revision))
			pr_err("%s: DSI%d panel_revision_selection_error\n", __func__, ctrl->ndx);
		else
			vdd->panel_func.samsung_panel_revision(ctrl);

		pr_info("%s: DSI%d Panel_Revision = %c %d\n", __func__, ctrl->ndx, vdd->panel_revision + 'A', vdd->panel_revision);
	}

	/* Manufacture date */
	if (!vdd->manufacture_date_loaded_dsi[ctrl->ndx]) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_manufacture_date_read))
			pr_err("%s: DSI%d manufacture_date_error\n", __func__, ctrl->ndx);
		else
			vdd->manufacture_date_loaded_dsi[ctrl->ndx] = vdd->panel_func.samsung_manufacture_date_read(ctrl);
	}

	/* DDI ID */
	if (!vdd->ddi_id_loaded_dsi[ctrl->ndx]) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_ddi_id_read))
			pr_err("%s: DSI%d ddi_id_error\n", __func__, ctrl->ndx);
		else
			vdd->ddi_id_loaded_dsi[ctrl->ndx] = vdd->panel_func.samsung_ddi_id_read(ctrl);
	}

	/* HBM */
	if (!vdd->hbm_loaded_dsi[ctrl->ndx]) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_hbm_read))
			pr_err("%s: DSI%d ddi_id_error\n", __func__, ctrl->ndx);
		else
			vdd->hbm_loaded_dsi[ctrl->ndx] = vdd->panel_func.samsung_hbm_read(ctrl);
	}

	/* MDNIE X,Y */
	if (!vdd->mdnie_loaded_dsi[ctrl->ndx]) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_mdnie_read))
			pr_err("%s: DSI%d mdnie_x_y_error\n", __func__, ctrl->ndx);
		else
			vdd->mdnie_loaded_dsi[ctrl->ndx] = vdd->panel_func.samsung_mdnie_read(ctrl);
	}

	/* Smart dimming*/
	if (!vdd->smart_dimming_loaded_dsi[ctrl->ndx]) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_smart_dimming_init))
			pr_err("%s: DSI%d smart dimming error\n", __func__, ctrl->ndx);
		else
			vdd->smart_dimming_loaded_dsi[ctrl->ndx] = vdd->panel_func.samsung_smart_dimming_init(ctrl);
	}

	/* Smart dimming for hmt */
	if (vdd->dtsi_data[0].hmt_enabled) {
		if (!vdd->smart_dimming_hmt_loaded_dsi[ctrl->ndx]) {
			if (IS_ERR_OR_NULL(vdd->panel_func.samsung_smart_dimming_hmt_init))
				pr_err("%s: DSI%d smart dimming hmt init error\n", __func__, ctrl->ndx);
			else
				vdd->smart_dimming_hmt_loaded_dsi[ctrl->ndx] = vdd->panel_func.samsung_smart_dimming_hmt_init(ctrl);
		}
	}

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_on_pre))
		vdd->panel_func.samsung_panel_on_pre(ctrl);

	pr_info("%s-: ndx=%d \n", __func__, ctrl->ndx);

	return true;
}

int mdss_samsung_panel_on_post(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd = NULL;
	struct mdss_panel_info *pinfo = NULL;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(ctrl)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	if (!mdss_panel_attach_get(ctrl)) {
		pr_err("%s: mdss_panel_attach_get(%d) : %d\n",__func__, ctrl->ndx, mdss_panel_attach_get(ctrl));
		return false;
	}

	pinfo = &(ctrl->panel_data.panel_info);

	if (IS_ERR_OR_NULL(pinfo)) {
		pr_err("%s: Invalid data pinfo : 0x%zx\n", __func__,  (size_t)pinfo);
		return false;
	}

	pr_info("%s+: ndx=%d \n", __func__, ctrl->ndx);

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_on_post))
		vdd->panel_func.samsung_panel_on_post(ctrl);

	/*
	*	To prevent sudden splash at wakeup
	*	init_bl_level value is updated at mdss_samsung_panel_off_post().
	* 	First brightness value is set at mdss_fb_probe() function like below.
	*	mfd->bl_level = DEFAULT_BRIGHTNESS;
	*/
	if (vdd->bl_level != 0) /* if bl level changed during blank or ready to unblank mode, make bl work with latest saved bl level*/
		vdd->init_bl_level = vdd->bl_level;

	/* Recovery Mode : Set some default brightness */
	if (vdd->recovery_boot_mode)
		vdd->init_bl_level = DEFAULT_BRIGHTNESS;

	mdss_samsung_brightness_dcs(ctrl, vdd->init_bl_level);

	if (vdd->support_mdnie_lite)
		update_dsi_tcon_mdnie_register(vdd);

	vdd->display_ststus_dsi[ctrl->ndx].wait_disp_on = true;

	pr_info("%s-: ndx=%d \n", __func__, ctrl->ndx);

	return true;
}

int mdss_samsung_panel_off_pre(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)pdata->panel_private;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__,
				(size_t)ctrl, (size_t)vdd);
		return false;
	}

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_off_pre))
		vdd->panel_func.samsung_panel_off_pre(ctrl);

	return ret;
}

int mdss_samsung_panel_off_post(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)pdata->panel_private;
	struct mdss_panel_info *pinfo = NULL;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__,
				(size_t)ctrl, (size_t)vdd);
		return false;
	}

	pinfo = &(ctrl->panel_data.panel_info);

	if (IS_ERR_OR_NULL(pinfo)) {
		pr_err("%s: Invalid data pinfo : 0x%zx\n", __func__,  (size_t)pinfo);
		return false;
	}

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_off_post))
		vdd->panel_func.samsung_panel_off_post(ctrl);

	/* Set init_bl_level for adb shell stop/start */
	mutex_lock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);
	vdd->init_bl_level = vdd->mfd_dsi[DISPLAY_1]->bl_level;
	mutex_unlock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);

	return ret;
}

/*************************************************************
*
*		EXTRA POWER RELATED FUNCTION BELOW.
*
**************************************************************/
static int mdss_dsi_extra_power_request_gpios(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc = 0, i;
	/*
	 * gpio_name[] named as gpio_name + num(recomend as 0)
	 * because of the num will increase depend on number of gpio
	 */
	static const char gpio_name[] = "panel_extra_power";
	static u8 gpio_request_status = -EINVAL;
	struct samsung_display_driver_data *vdd = NULL;

	if (!gpio_request_status)
		goto end;

	if (!IS_ERR_OR_NULL(ctrl))
		vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)){
		pr_err("%s: Invalid data pinfo : 0x%zx\n", __func__,  (size_t)vdd);
		goto end;
	}

	for (i = 0; i < MAX_EXTRA_POWER_GPIO; i++) {
		if (gpio_is_valid(vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i])) {
			rc = gpio_request(vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i],
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
			if (gpio_is_valid(vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i]))
				gpio_free(vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i--]);
			pr_err("%s : i = %d\n", __func__, i);
		} while (i > 0);
	}

	return rc;
}

int mdss_samsung_panel_extra_power(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	int ret = 0, i = 0, add_value = 1;
	struct samsung_display_driver_data *vdd = NULL;

	if (IS_ERR_OR_NULL(pdata)) {
		pr_err("%s: Invalid pdata : 0x%zx\n", __func__, (size_t)pdata);
		ret = -EINVAL;
		goto end;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
						panel_data);

	if (!IS_ERR_OR_NULL(ctrl))
		vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)){
		pr_err("%s: Invalid data pinfo : 0x%zx\n", __func__,  (size_t)vdd);
		goto end;
	}

	pr_info("%s: ++ enable(%d) ndx(%d)\n",
			__func__,enable, ctrl->ndx );

	if (ctrl->ndx == DSI_CTRL_1)
		goto end;

	if (mdss_dsi_extra_power_request_gpios(ctrl)) {
		pr_err("%s : fail to request extra power gpios", __func__);
		goto end;
	}

	pr_info("%s : %s extra power gpios\n", __func__, enable ? "enable" : "disable");

	/*
	 * The order of extra_power_gpio enable/disable
	 * 1. Enable : panel_extra_power_gpio[0], [1], ... [MAX_EXTRA_POWER_GPIO - 1]
	 * 2. Disable : panel_extra_power_gpio[MAX_EXTRA_POWER_GPIO - 1], ... [1], [0]
	 */
	if (!enable) {
		add_value = -1;
		i = MAX_EXTRA_POWER_GPIO - 1;
	}

	do {
		if (gpio_is_valid(vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i])) {
			gpio_set_value(vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i], enable);
			pr_info("%s : set extra power gpio[%d] to %s\n",
						 __func__, vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i],
						enable ? "high" : "low");
			usleep_range(5000, 5000);
		}
	} while (((i += add_value) < MAX_EXTRA_POWER_GPIO) && (i >= 0));

end:
	pr_info("%s: --\n", __func__);
	return ret;
}


/*************************************************************
*
*		ESD RECOVERY RELATED FUNCTION BELOW.
*
**************************************************************/

static int mdss_dsi_esd_irq_status(struct mdss_dsi_ctrl_pdata *ctrl)
{
	pr_info("%s: lcd esd recovery\n", __func__);

	return !ctrl->status_value;
}

/*
 * esd_irq_enable() - Enable or disable esd irq.
 *
 * @enable	: flag for enable or disabled
 * @nosync	: flag for disable irq with nosync
 * @data	: point ot struct mdss_panel_info
 */
static void esd_irq_enable(bool enable, bool nosync, void *data)
{
	/* The irq will enabled when do the request_threaded_irq() */
	static bool is_enabled = true;
	int gpio;
	unsigned long flags;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)data;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd : 0x%zx\n", __func__, (size_t)vdd);
		return;
	}

	spin_lock_irqsave(&vdd->esd_recovery.irq_lock, flags);
	gpio = vdd->esd_recovery.esd_gpio;

	if (enable == is_enabled) {
		pr_info("%s: ESD irq already %s\n",
				__func__, enable ? "enabled" : "disabled");
		goto error;
	}

	pr_info("%s: esd_irq_enable : %s gpio %d esd_gpio %d\n",
				__func__, enable ? "enabled" : "disabled", gpio, vdd->esd_recovery.esd_gpio);

	if (enable) {
		is_enabled = true;
		enable_irq(gpio_to_irq(vdd->esd_recovery.esd_gpio));
	} else {
		if (nosync)
			disable_irq_nosync(gpio_to_irq(vdd->esd_recovery.esd_gpio));
		else
			disable_irq(gpio_to_irq(vdd->esd_recovery.esd_gpio));
		is_enabled = false;
	}

	/* TODO: Disable log if the esd function stable */
	pr_err("%s: ESD irq %s with %s\n",
				__func__,
				enable ? "enabled" : "disabled",
				nosync ? "nosync" : "sync");
error:
	spin_unlock_irqrestore(&vdd->esd_recovery.irq_lock, flags);

}

static void esd_irq_work_func(struct work_struct *work)
{
	struct samsung_display_driver_data *vdd = samsung_get_vdd();
	char *envp[2] = {"PANEL_ALIVE=0", NULL};

	struct device *dev = vdd->mfd_dsi[DISPLAY_1]->fbi->dev;
	struct mdss_panel_data *pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;
	struct msm_fb_data_type *mfd = vdd->mfd_dsi[DISPLAY_1];

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd : 0x%zx\n", __func__, (size_t)vdd);
		return ;
	}

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		pr_err("%s vdd is error", __func__);
		return ;
	}

	if (mdss_fb_is_power_on(mfd)) {
		if (pdata->panel_info.type == MIPI_CMD_PANEL){
			if(mdss_dsi_sync_wait_enable(vdd->ctrl_dsi[DISPLAY_1])){
				pdata_dsi0->panel_info.panel_dead = true;
				pdata_dsi1->panel_info.panel_dead = true;
				pr_err("%s here 1", __func__);
			}else{

				pdata->panel_info.panel_dead = true;
				pr_err("%s here 2", __func__);
			}
		}

		kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
		pr_err("Panel has gone bad, sending uevent - %s\n", envp[0]);
	}
	return;
}

static irqreturn_t esd_irq_handler(int irq, void *handle)
{
	struct samsung_display_driver_data *vdd = samsung_get_vdd();
	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd : 0x%zx\n", __func__, (size_t)vdd);
		return 0;
	}

	pr_err("++\n");

	if (!vdd->esd_recovery.is_enabled_esd_recovery) {
		pr_err("esd recovery is not enabled yet");
		goto end;
	}

	esd_irq_enable(false, true, (void *)vdd);

	schedule_work(&esd_irq_work);

	pr_info("%s--\n", __func__);

	end:

	return IRQ_HANDLED;
}

static void mdss_samsung_event_esd_recovery_init(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd = NULL;
	int ret;
	struct mdss_panel_info *pinfo;

	if (IS_ERR_OR_NULL(pdata)) {
		pr_err("%s: Invalid pdata : 0x%zx\n", __func__, (size_t)pdata);
		return;
	}

	vdd = (struct samsung_display_driver_data *)pdata->panel_private;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	pinfo = &ctrl->panel_data.panel_info;

	if (IS_ERR_OR_NULL(ctrl) ||	IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n",
				__func__, (size_t)ctrl, (size_t)vdd);
		return;
	}

	if (!pinfo->esd_check_enabled)
		return;

	pr_info("%s+: ndx=%d esd_gpio(%d)\n", __func__, ctrl->ndx, vdd->esd_recovery.esd_gpio);

	if (!vdd->esd_recovery.esd_recovery_init) {
		vdd->esd_recovery.esd_recovery_init = true;

		vdd->esd_recovery.esd_irq_enable = esd_irq_enable;

		INIT_WORK(&esd_irq_work, esd_irq_work_func);

		if (ctrl->status_mode == ESD_REG_IRQ) {
			if (gpio_is_valid(vdd->esd_recovery.esd_gpio)) {
				gpio_request(vdd->esd_recovery.esd_gpio, "esd_recovery");
				ret = request_threaded_irq(
						gpio_to_irq(vdd->esd_recovery.esd_gpio),
						NULL,
						esd_irq_handler,
						vdd->esd_recovery.irqflags,
						"esd_recovery",
						(void *)ctrl);
				if (ret)
					pr_err("%s : Failed to request_irq, ret=%d\n",
							__func__, ret);
				else{
					esd_irq_enable(false, true, (void *)vdd);
					pr_info("%s-: esd_irq_enabled\n", __func__);
				}

			}else
				pr_info("%s-: gpio is invalid ndx=%d\n", __func__, ctrl->ndx);
		}
	}

	pr_info("%s-: ndx=%d\n", __func__, ctrl->ndx);
}

/*************************************************************
*
*		BRIGHTNESS RELATED FUNCTION BELOW.
*
**************************************************************/
int get_cmd_index(struct samsung_display_driver_data *vdd, int ndx)
{
	int index = vdd->dtsi_data[ndx].candela_map_table[vdd->panel_revision].bkl[vdd->bl_level];

	if (vdd->dtsi_data[0].hmt_enabled && vdd->hmt_stat.hmt_on)
		index = vdd->cmd_idx;

	return vdd->dtsi_data[ndx].candela_map_table[vdd->panel_revision].cmd_idx[index];
}

int get_candela_value(struct samsung_display_driver_data *vdd, int ndx)
{
	int index = vdd->dtsi_data[ndx].candela_map_table[vdd->panel_revision].bkl[vdd->bl_level];

	if (vdd->dtsi_data[0].hmt_enabled && vdd->hmt_stat.hmt_on)
		index = vdd->cmd_idx;

	return vdd->dtsi_data[ndx].candela_map_table[vdd->panel_revision].lux_tab[index];
}

int get_scaled_level(struct samsung_display_driver_data *vdd, int ndx)
{
	int index = vdd->dtsi_data[ndx].scaled_level_map_table[vdd->panel_revision].bkl[vdd->bl_level];

	return vdd->dtsi_data[ndx].scaled_level_map_table[vdd->panel_revision].lux_tab[index];
}

static void mdss_samsung_update_brightness_packet(struct dsi_cmd_desc *packet, int *count, struct dsi_panel_cmds *tx_cmd)
{
	int loop = 0;

	if (IS_ERR_OR_NULL(packet)) {
		pr_err("%s %ps packet error", __func__, __builtin_return_address(0));
		return ;
	}

	if (IS_ERR_OR_NULL(tx_cmd)) {
		pr_err("%s %ps tx_cmd error", __func__, __builtin_return_address(0));
		return ;
	}

	if (*count > (BRIGHTNESS_MAX_PACKET - 1))/*cmd_count is index, if cmd_count >13 then panic*/
		panic("over max brightness_packet size(%d).. !!", BRIGHTNESS_MAX_PACKET);

	for (loop = 0; loop < tx_cmd->cmd_cnt; loop++) {
		packet[*count].dchdr.dtype = tx_cmd->cmds[loop].dchdr.dtype;
		packet[*count].dchdr.wait = tx_cmd->cmds[loop].dchdr.wait;
		packet[*count].dchdr.dlen = tx_cmd->cmds[loop].dchdr.dlen;

		packet[*count].payload = tx_cmd->cmds[loop].payload;

		(*count)++;
	}
}

static void update_packet_level_key_enable(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_cmd_desc *packet, int *cmd_cnt, int level_key)
{
	if (!level_key)
		return;
	else {
		if (level_key & PANEL_LEVE1_KEY)
			mdss_samsung_update_brightness_packet(packet, cmd_cnt, mdss_samsung_cmds_select(ctrl, PANEL_LEVE1_KEY_ENABLE));

		if (level_key & PANEL_LEVE2_KEY)
			mdss_samsung_update_brightness_packet(packet, cmd_cnt, mdss_samsung_cmds_select(ctrl, PANEL_LEVE2_KEY_ENABLE));

	}
}

static void update_packet_level_key_disable(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_cmd_desc *packet, int *cmd_cnt, int level_key)
{
	if (!level_key)
		return;
	else {
		if (level_key & PANEL_LEVE1_KEY)
			mdss_samsung_update_brightness_packet(packet, cmd_cnt, mdss_samsung_cmds_select(ctrl, PANEL_LEVE1_KEY_DISABLE));

		if (level_key & PANEL_LEVE2_KEY)
			mdss_samsung_update_brightness_packet(packet, cmd_cnt, mdss_samsung_cmds_select(ctrl, PANEL_LEVE1_KEY_DISABLE));

	}
}

int mdss_samsung_hbm_brightenss_packet_set(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int cmd_cnt = 0;
	int level_key = 0;
	struct dsi_cmd_desc *packet = NULL;
	struct dsi_panel_cmds *tx_cmd = NULL;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return 0;
	}

	if (vdd->display_ststus_dsi[ctrl->ndx].hbm_mode) {
		pr_err("%s DSI%d: already hbm mode! return ", __func__, ctrl->ndx);
		return 0;
	}

	/* init packet */
	packet = &vdd->brightness[ctrl->ndx].brightness_packet_dsi[0];

	/*
	*	HBM doesn't need calculated cmds. So Just use previously fixed data.
	*/
	/* To check supporting HBM mdoe by hbm_gamma_tx_cmds */
	if (vdd->dtsi_data[ctrl->ndx].hbm_gamma_tx_cmds[vdd->panel_revision].cmd_cnt) {
		/* hbm gamma */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_hbm_gamma)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_hbm_gamma(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}
	}

	if (vdd->dtsi_data[ctrl->ndx].hbm_etc_tx_cmds[vdd->panel_revision].cmd_cnt) {
		/* hbm etc */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_hbm_etc)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_hbm_etc(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}
	}

	return cmd_cnt;
}

int mdss_samsung_normal_brightenss_packet_set(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int cmd_cnt = 0;
	int level_key = 0;
	struct dsi_cmd_desc *packet = NULL;
	struct dsi_panel_cmds *tx_cmd = NULL;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return 0;
	}

	packet = &vdd->brightness[ctrl->ndx].brightness_packet_dsi[0]; /* init packet */

	if (vdd->smart_dimming_loaded_dsi[ctrl->ndx]) { /* OCTA PANEL */
		/* hbm off */
		if (vdd->display_ststus_dsi[ctrl->ndx].hbm_mode) {
			if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_hbm_off)) {
				level_key = false;
				tx_cmd = vdd->panel_func.samsung_brightness_hbm_off(ctrl, &level_key);

				update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
				mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
				update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
			}
		}

		/* aid/aor */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_aid)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_aid(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		/* acl */
		if (vdd->acl_status || vdd->siop_status) {
			if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_acl_on)) {
				level_key = false;
				tx_cmd = vdd->panel_func.samsung_brightness_acl_on(ctrl, &level_key);

				update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
				mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
				update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
			}

			if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_acl_percent)) {
				level_key = false;
				tx_cmd = vdd->panel_func.samsung_brightness_acl_percent(ctrl, &level_key);

				update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
				mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
				update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
			}
		} else {
			if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_acl_off)) {
				level_key = false;
				tx_cmd = vdd->panel_func.samsung_brightness_acl_off(ctrl, &level_key);

				update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
				mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
				update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
			}
		}

		/* elvss */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_elvss)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_elvss(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		/* temperature elvss */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_elvss_temperature1)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_elvss_temperature1(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_elvss_temperature2)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_elvss_temperature2(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		/* vint */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_vint)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_vint(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		/* gamma */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_gamma)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_gamma(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}
	} else { /* TFT PANEL */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_tft_pwm)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_tft_pwm(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}
	}

	return cmd_cnt;
}

static int mdss_samsung_single_transmission_packet(struct samsung_brightenss_data *tx_packet)
{
	int loop;
	struct dsi_cmd_desc *packet = tx_packet->brightness_packet_dsi;
	int packet_cnt = tx_packet->brightness_packet_tx_cmds_dsi.cmd_cnt;

	for(loop = 0; (loop < packet_cnt) && (loop < BRIGHTNESS_MAX_PACKET); loop++) {
		if (packet[loop].dchdr.dtype == DTYPE_DCS_LWRITE ||
			packet[loop].dchdr.dtype == DTYPE_GEN_LWRITE)
			packet[loop].dchdr.last = 0;
		else {
			if (loop > 0)
				packet[loop - 1].dchdr.last = 1; /*To ensure previous single tx packet */

			packet[loop].dchdr.last = 1;
		}
	}

	if (loop == BRIGHTNESS_MAX_PACKET)
		return false;
	else {
		packet[loop - 1].dchdr.last = 1; /* To make last packet flag */
		return true;
	}
}

int mdss_samsung_brightness_dcs(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	struct samsung_display_driver_data *vdd = NULL;
	int cmd_cnt = 0;
	int ret = 0;

	vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	if (!get_lcd_attached("GET"))
		return false;

	vdd->bl_level = level;

	if (vdd->dtsi_data[0].hmt_enabled && vdd->hmt_stat.hmt_on) {
		pr_err("%s : HMT is on. do not set normal brightness..(%d)\n", __func__, level);
		return false;
	}

	if (alpm_status_func(CHECK_PREVIOUS_STATUS)) {
		pr_info("[ALPM_DEBUG] ALPM is on. do not set brightness.. (%d)\n", level);
		return false;
	}

	if (vdd->auto_brightness >= HBM_MODE && vdd->bl_level == 255) {
		cmd_cnt = mdss_samsung_hbm_brightenss_packet_set(ctrl);
		cmd_cnt > 0 ? vdd->display_ststus_dsi[ctrl->ndx].hbm_mode = true : false;
	} else {
		cmd_cnt = mdss_samsung_normal_brightenss_packet_set(ctrl);
		cmd_cnt > 0 ? vdd->display_ststus_dsi[ctrl->ndx].hbm_mode = false : false;
	}

	if (cmd_cnt) {
		/* setting tx cmds cmt */
		vdd->brightness[ctrl->ndx].brightness_packet_tx_cmds_dsi.cmd_cnt = cmd_cnt;

		/* generate single tx packet */
		ret = mdss_samsung_single_transmission_packet(&vdd->brightness[ctrl->ndx]);

		/* sending tx cmds */
		if (ret) {
			mdss_samsung_send_cmd(ctrl, PANEL_BRIGHT_CTRL);

			pr_info("%s DSI%d level : %d  candela : %dCD hbm : %d\n", __func__,
				ctrl->ndx, level, vdd->candela_level,
				vdd->display_ststus_dsi[ctrl->ndx].hbm_mode);
		} else
			pr_info("%s DSDI%d single_transmission_fail error\n", __func__, ctrl->ndx);
	} else
		pr_info("%s DSI%d level : %d skip\n", __func__, ctrl->ndx, vdd->bl_level);

	return cmd_cnt;
}

/*************************************************************
*
*		OSC TE FITTING RELATED FUNCTION BELOW.
*
**************************************************************/
static void mdss_samsung_event_osc_te_fitting(struct mdss_panel_data *pdata, int event, void *arg)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd = NULL;
	struct osc_te_fitting_info *te_info = NULL;
	struct mdss_mdp_ctl *ctl = NULL;
	int ret, i, lut_count;

	if (IS_ERR_OR_NULL(pdata)) {
		pr_err("%s: Invalid pdata : 0x%zx\n", __func__, (size_t)pdata);
		return;
	}

	vdd = (struct samsung_display_driver_data *)pdata->panel_private;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	ctl = arg;

	if (IS_ERR_OR_NULL(ctrl) ||	IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(ctl)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx ctl : 0x%zx\n",
				__func__, (size_t)ctrl, (size_t)vdd, (size_t)ctl);
		return;
	}

	te_info = &vdd->te_fitting_info;

	if (IS_ERR_OR_NULL(te_info)) {
		pr_err("%s: Invalid te data : 0x%zx\n",
				__func__, (size_t)te_info);
		return;
	}

	if (pdata->panel_info.cont_splash_enabled) {
		pr_err("%s: cont splash enabled\n", __func__);
		return;
	}

	if (!ctl->vsync_handler.enabled) {
		pr_err("%s: vsync handler does not enabled yet\n", __func__);
		return;
	}

	te_info->status |= TE_FITTING_DONE;

	pr_debug("%s:++\n", __func__);

	if (!(te_info->status & TE_FITTING_REQUEST_IRQ)) {
		te_info->status |= TE_FITTING_REQUEST_IRQ;

		ret = request_threaded_irq(
				gpio_to_irq(ctrl->disp_te_gpio),
				samsung_te_check_handler,
				NULL,
				IRQF_TRIGGER_FALLING,
				"VSYNC_GPIO",
				(void *)ctrl);
		if (ret)
			pr_err("%s : Failed to request_irq, ret=%d\n",
					__func__, ret);
		else
			disable_irq(gpio_to_irq(ctrl->disp_te_gpio));
		te_info->te_time =
			kzalloc(sizeof(long long) * te_info->sampling_rate, GFP_KERNEL);
		INIT_WORK(&te_info->work, samsung_te_check_done_work);
	}

	for (lut_count = 0; lut_count < OSC_TE_FITTING_LUT_MAX; lut_count++) {
		init_completion(&te_info->te_check_comp);
		te_info->status |= TE_CHECK_ENABLE;
		te_info->te_duration = 0;

		pr_debug("%s: osc_te_fitting _irq : %d\n",
				__func__, gpio_to_irq(ctrl->disp_te_gpio));

		enable_irq(gpio_to_irq(ctrl->disp_te_gpio));
		ret = wait_for_completion_timeout(
				&te_info->te_check_comp, 1000);

		if (ret <= 0)
			pr_err("%s: timeout\n", __func__);

		for (i = 0; i < te_info->sampling_rate; i++) {
			te_info->te_duration +=
				(i != 0 ? (te_info->te_time[i] - te_info->te_time[i-1]) : 0);
			pr_debug("%s: vsync time : %lld, sum : %lld\n",
					__func__, te_info->te_time[i], te_info->te_duration);
		}
		do_div(te_info->te_duration, te_info->sampling_rate - 1);
		pr_info("%s: ave vsync time : %lld\n",
				__func__, te_info->te_duration);
		te_info->status &= ~TE_CHECK_ENABLE;

		if (vdd->panel_func.samsung_osc_te_fitting)
			ret = vdd->panel_func.samsung_osc_te_fitting(ctrl);

		if (!ret)
			mdss_samsung_send_cmd(ctrl, PANEL_OSC_TE_FITTING);
		else
			break;
	}
	pr_debug("%s:--\n", __func__);
}

static void samsung_te_check_done_work(struct work_struct *work)
{
	struct osc_te_fitting_info *te_info = NULL;

	te_info = container_of(work, struct osc_te_fitting_info, work);

	if (IS_ERR_OR_NULL(te_info)) {
		pr_err("%s: Invalid TE tuning data\n", __func__);
		return;
	}

	complete_all(&te_info->te_check_comp);
}

static irqreturn_t samsung_te_check_handler(int irq, void *handle)
{
	struct samsung_display_driver_data *vdd = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct osc_te_fitting_info *te_info = NULL;
	static bool skip_first_te = true;
	static u8 count;

	if (skip_first_te) {
		skip_first_te = false;
		goto end;
	}

	if (IS_ERR_OR_NULL(handle)) {
		pr_err("handle is null\n");
		goto end;
	}

	ctrl = (struct mdss_dsi_ctrl_pdata *)handle;

	vdd = (struct samsung_display_driver_data *)ctrl->panel_data.panel_private;

	te_info = &vdd->te_fitting_info;

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n",
				__func__, (size_t)ctrl, (size_t)vdd);
		return -EINVAL;
	}

	if (!(te_info->status & TE_CHECK_ENABLE))
		goto end;

	if (count < te_info->sampling_rate) {
		te_info->te_time[count++] =
			ktime_to_us(ktime_get());
	} else {
		disable_irq_nosync(gpio_to_irq(ctrl->disp_te_gpio));
		schedule_work(&te_info->work);
		skip_first_te = true;
		count = 0;
	}

end:
	return IRQ_HANDLED;
}

/*************************************************************
*
*		LDI FPS RELATED FUNCTION BELOW.
*
**************************************************************/
int ldi_fps(unsigned int input_fps)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd = samsung_get_vdd();

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd : 0x%zx\n", __func__, (size_t)vdd);
		return 0;
	}

	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	if (IS_ERR_OR_NULL(ctrl)) {
		pr_err("%s ctrl is error", __func__);
		return 0;
	}

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_change_ldi_fps))
		rc = vdd->panel_func.samsung_change_ldi_fps(ctrl, input_fps);

	if (rc)
		mdss_samsung_send_cmd(ctrl, PANEL_LDI_FPS_CHANGE);

	return rc;
}
EXPORT_SYMBOL(ldi_fps);

/*************************************************************
*
*		HMT RELATED FUNCTION BELOW.
*
**************************************************************/
static int get_candela_value_hmt(struct samsung_display_driver_data *vdd, int ndx)
{
	int index = vdd->dtsi_data[ndx].hmt_candela_map_table[vdd->panel_revision].bkl[vdd->hmt_stat.hmt_bl_level];

	return vdd->dtsi_data[ndx].hmt_candela_map_table[vdd->panel_revision].lux_tab[index];
}

static int get_cmd_idx_hmt(struct samsung_display_driver_data *vdd, int ndx)
{
	int index = vdd->dtsi_data[ndx].hmt_candela_map_table[vdd->panel_revision].bkl[vdd->hmt_stat.hmt_bl_level];

	return vdd->dtsi_data[ndx].hmt_candela_map_table[vdd->panel_revision].cmd_idx[index];
}

int mdss_samsung_hmt_brightenss_packet_set(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int cmd_cnt = 0;
	int level_key = 0;
	struct dsi_cmd_desc *packet = NULL;
	struct dsi_panel_cmds *tx_cmd = NULL;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	pr_info("%s : ++\n", __func__);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return 0;
	}

	/* init packet */
	packet = &vdd->brightness[ctrl->ndx].brightness_packet_dsi[0];

	if (vdd->smart_dimming_hmt_loaded_dsi[ctrl->ndx]) {
		/* aid/aor B2 */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_aid_hmt)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_aid_hmt(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		/* elvss B6 */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_elvss_hmt)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_elvss_hmt(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		/* vint F4 */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_vint_hmt)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_vint_hmt(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		/* gamma CA */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_gamma_hmt)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_gamma_hmt(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}
	}

	pr_info("%s : --\n", __func__);

	return cmd_cnt;
}

int mdss_samsung_brightness_dcs_hmt(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	struct samsung_display_driver_data *vdd = NULL;
	int cmd_cnt;
	int ret = 0;
	int i;
	int candela_map_tab_size;

	vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	vdd->hmt_stat.hmt_bl_level = level;

	pr_err("[HMT] hmt_bl_level(%d)\n", vdd->hmt_stat.hmt_bl_level);

	if ( level < 0 || level > 255 ) {
		pr_err("[HMT] hmt_bl_level(%d) is out of range! set to 150cd \n", level);
		vdd->hmt_stat.cmd_idx_hmt = 28;
		vdd->hmt_stat.candela_level_hmt = 150;
	} else {
		vdd->hmt_stat.cmd_idx_hmt = get_cmd_idx_hmt(vdd, ctrl->ndx);
		vdd->hmt_stat.candela_level_hmt = get_candela_value_hmt(vdd, ctrl->ndx);
	}

	if (vdd->hmt_stat.hmt_low_persistence) {
		cmd_cnt = mdss_samsung_hmt_brightenss_packet_set(ctrl);
	} else {
		// If low persistence is off, reset the bl level(use normal aid).
		pr_info("[HMT] [LOW PERSISTENCE OFF] - use normal aid for brightness\n");

		candela_map_tab_size =
			vdd->dtsi_data[ctrl->ndx].candela_map_table[vdd->panel_revision].lux_tab_size;

		for (i=0; i<candela_map_tab_size; i++) {
			if (vdd->hmt_stat.candela_level_hmt <=
				vdd->dtsi_data[ctrl->ndx].candela_map_table[vdd->panel_revision].lux_tab[i]) {
				pr_info("%s: hmt_cd_level (%d), normal_cd_level (%d) idx(%d)\n",
					__func__, vdd->hmt_stat.candela_level_hmt,
					vdd->dtsi_data[ctrl->ndx].candela_map_table[vdd->panel_revision].lux_tab[i], i);
				break;
			}
		}

		vdd->cmd_idx = i;
		vdd->candela_level = vdd->dtsi_data[ctrl->ndx].candela_map_table[vdd->panel_revision].lux_tab[i];

		pr_info("[HMT] [LOW PERSISTENCE OFF] hmt_cd_level (%d) idx(%d) cd_level(%d)\n",
			vdd->hmt_stat.candela_level_hmt, vdd->cmd_idx, vdd->candela_level);

		cmd_cnt = mdss_samsung_normal_brightenss_packet_set(ctrl);
	}

	/* sending tx cmds */
	if (cmd_cnt) {
		/* setting tx cmds cmt */
		vdd->brightness[ctrl->ndx].brightness_packet_tx_cmds_dsi.cmd_cnt = cmd_cnt;

		/* generate single tx packet */
		ret = mdss_samsung_single_transmission_packet(vdd->brightness);

		if (ret) {
			mdss_samsung_send_cmd(ctrl, PANEL_BRIGHT_CTRL);

			pr_info("%s : DSI(%d) cmd_idx(%d), candela_level(%d), hmt_bl_level(%d)", __func__,
		ctrl->ndx, vdd->hmt_stat.cmd_idx_hmt, vdd->hmt_stat.candela_level_hmt, vdd->hmt_stat.hmt_bl_level);
		} else
			pr_debug("%s DSDI%d single_transmission_fail error\n", __func__, ctrl->ndx);
	} else
		pr_info("%s DSI%d level : %d skip\n", __func__, ctrl->ndx, vdd->bl_level);

	return cmd_cnt;
}

/************************************************************

		PANEL DTSI PARSE FUNCTION.

		--- NEVER DELETE OR CHANGE ORDER ---
		---JUST ADD ITEMS AT LAST---

	"samsung,display_on_tx_cmds_rev%c"
	"samsung,display_off_tx_cmds_rev%c"
	"samsung,level1_key_enable_tx_cmds_rev%c"
	"samsung,level1_key_disable_tx_cmds_rev%c"
	"samsung,level2_key_enable_tx_cmds_rev%c"
	"samsung,level2_key_disable_tx_cmds_rev%c"
	"samsung,smart_dimming_mtp_rx_cmds_rev%c"
	"samsung,manufacture_read_pre_tx_cmds_rev%c"
	"samsung,manufacture_id0_rx_cmds_rev%c"
	"samsung,manufacture_id1_rx_cmds_rev%c"
	"samsung,manufacture_id2_rx_cmds_rev%c"
	"samsung,manufacture_date_rx_cmds_rev%c"
	"samsung,ddi_id_rx_cmds_rev%c"
	"samsung,rddpm_rx_cmds_rev%c"
	"samsung,mtp_read_sysfs_rx_cmds_rev%c"
	"samsung,vint_tx_cmds_rev%c"
	"samsung,vint_map_table_rev%c"
	"samsung,acl_off_tx_cmds_rev%c"
	"samsung,acl_map_table_rev%c"
	"samsung,candela_map_table_rev%c"
	"samsung,acl_percent_tx_cmds_rev%c"
	"samsung,acl_on_tx_cmds_rev%c"
	"samsung,gamma_tx_cmds_rev%c"
	"samsung,elvss_rx_cmds_rev%c"
	"samsung,elvss_tx_cmds_rev%c"
	"samsung,aid_map_table_rev%c"
	"samsung,aid_tx_cmds_rev%c"
	"samsung,hbm_rx_cmds_rev%c"
	"samsung,hbm2_rx_cmds_rev%c"
	"samsung,hbm_gamma_tx_cmds_rev%c"
	"samsung,hbm_etc_tx_cmds_rev%c"
	"samsung,hbm_off_tx_cmds_rev%c"
	"samsung,mdnie_read_rx_cmds_rev%c"
	"samsung,ldi_debug0_rx_cmds_rev%c"
	"samsung,ldi_debug1_rx_cmds_rev%c"
	"samsung,ldi_debug2_rx_cmds_rev%c"
	"samsung,elvss_lowtemp_tx_cmds_rev%c"
	"samsung,elvss_lowtemp2_tx_cmds_rev%c"
	"samsung,smart_acl_elvss_tx_cmds_rev%c"
	"samsung,smart_acl_elvss_map_table_rev%c"
	"samsung,partial_display_on_tx_cmds_rev%c"
	"samsung,partial_display_off_tx_cmds_rev%c"
	"samsung,partial_display_column_row_tx_cmds_rev%c"
	"samsung,alpm_on_tx_cmds_rev%c"
	"samsung,alpm_off_tx_cmds_rev%c"
	"samsung,hmt_gamma_tx_cmds_rev%c"
	"samsung,hmt_elvss_tx_cmds_rev%c"
	"samsung,hmt_vint_tx_cmds_rev%c"
	"samsung,hmt_enable_tx_cmds_rev%c"
	"samsung,hmt_disable_tx_cmds_rev%c"
	"samsung,hmt_reverse_enable_tx_cmds_rev%c"
	"samsung,hmt_reverse_disable_tx_cmds_rev%c"
	"samsung,hmt_aid_tx_cmds_rev%c"
	"samsung,hmt_reverse_aid_map_table_rev%c"
	"samsung,hmt_150cd_rx_cmds_rev%c"
	"samsung,hmt_candela_map_table_rev%c"
	"samsung,ldi_fps_change_tx_cmds_rev%c"
	"samsung,ldi_fps_rx_cmds_rev%c"
	"samsung,tft_pwm_tx_cmds_rev%c"
	"samsung,scaled_level_map_table_rev%c"
	"samsung,packet_size_tx_cmds_rev%c"
	"samsung,reg_read_pos_tx_cmds_rev%c"
	"samsung,osc_te_fitting_tx_cmds_rev%c"
	"samsung,cabc_on_tx_cmds_rev%c"
	"samsung,cabc_off_tx_cmds_rev%c"
*************************************************************/
void mdss_samsung_panel_parse_dt_cmds(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rev_value = 'A';
	int panel_revision;
	char string[PARSE_STRING];
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	/* At this time ctrl->ndx is not set */
	int ndx = ctrl->panel_data.panel_info.pdest;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return ;
	} else
		pr_info("%s DSI%d", __func__, ndx);

	for (panel_revision = 0; panel_revision < SUPPORT_PANEL_REVISION; panel_revision++) {
		snprintf(string, PARSE_STRING, "samsung,display_on_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].display_on_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].display_on_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].display_on_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,display_off_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].display_off_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].display_off_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].display_off_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,level1_key_enable_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].level1_key_enable_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].level1_key_enable_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].level1_key_enable_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,level1_key_disable_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].level1_key_disable_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].level1_key_disable_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].level1_key_disable_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hsync_on_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hsync_on_tx_cmds [panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hsync_on_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hsync_on_tx_cmds [panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,level2_key_enable_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].level2_key_enable_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].level2_key_enable_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].level2_key_enable_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,level2_key_disable_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].level2_key_disable_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].level2_key_disable_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].level2_key_disable_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,smart_dimming_mtp_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].smart_dimming_mtp_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].smart_dimming_mtp_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].smart_dimming_mtp_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,manufacture_read_pre_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].manufacture_read_pre_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].manufacture_read_pre_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].manufacture_read_pre_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,manufacture_id0_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].manufacture_id0_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].manufacture_id0_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].manufacture_id0_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,manufacture_id1_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].manufacture_id1_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].manufacture_id1_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].manufacture_id1_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,manufacture_id2_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].manufacture_id2_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].manufacture_id2_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].manufacture_id2_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,manufacture_date_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].manufacture_date_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].manufacture_date_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].manufacture_date_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,ddi_id_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].ddi_id_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].ddi_id_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].ddi_id_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,rddpm_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].rddpm_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].rddpm_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].rddpm_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,mtp_read_sysfs_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].mtp_read_sysfs_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].mtp_read_sysfs_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].mtp_read_sysfs_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,vint_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].vint_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].vint_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].vint_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,vint_map_table_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_panel_table(np, &vdd->dtsi_data[ndx].vint_map_table[panel_revision], string) && panel_revision > 0) /* VINT TABLE */
			memcpy(&vdd->dtsi_data[ndx].vint_map_table[panel_revision], &vdd->dtsi_data[ndx].vint_map_table[panel_revision - 1], sizeof(struct cmd_map));

		snprintf(string, PARSE_STRING, "samsung,acl_off_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].acl_off_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].acl_off_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].acl_off_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,acl_map_table_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_panel_table(np, &vdd->dtsi_data[ndx].acl_map_table[panel_revision], string) && panel_revision > 0) /* ACL TABLE */
			memcpy(&vdd->dtsi_data[ndx].acl_map_table[panel_revision], &vdd->dtsi_data[ndx].acl_map_table[panel_revision - 1], sizeof(struct cmd_map));

		snprintf(string, PARSE_STRING, "samsung,candela_map_table_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_candella_lux_mapping_table(np, &vdd->dtsi_data[ndx].candela_map_table[panel_revision], string) && panel_revision > 0) /* CANDELA MAP TABLE */
					memcpy(&vdd->dtsi_data[ndx].candela_map_table[panel_revision], &vdd->dtsi_data[ndx].candela_map_table[panel_revision - 1], sizeof(struct candella_lux_map));

		snprintf(string, PARSE_STRING, "samsung,acl_percent_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].acl_percent_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].acl_percent_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].acl_percent_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,acl_on_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].acl_on_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].acl_on_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].acl_on_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,gamma_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].gamma_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].gamma_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].gamma_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,elvss_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].elvss_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].elvss_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].elvss_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,elvss_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].elvss_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].elvss_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].elvss_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,aid_map_table_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_panel_table(np, &vdd->dtsi_data[ndx].aid_map_table[panel_revision], string) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].aid_map_table[panel_revision], &vdd->dtsi_data[ndx].aid_map_table[panel_revision - 1], sizeof(struct cmd_map));/* AID TABLE */

		snprintf(string, PARSE_STRING, "samsung,aid_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].aid_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].aid_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].aid_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		/* CONFIG_HBM_RE */
		snprintf(string, PARSE_STRING, "samsung,hbm_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hbm_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hbm_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].hbm_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hbm2_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hbm2_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hbm2_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].hbm2_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hbm_gamma_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hbm_gamma_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hbm_gamma_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hbm_gamma_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hbm_etc_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hbm_etc_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hbm_etc_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hbm_etc_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hbm_off_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hbm_off_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hbm_off_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hbm_off_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		/* CONFIG_TCON_MDNIE_LITE */
		snprintf(string, PARSE_STRING, "samsung,mdnie_read_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].mdnie_read_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].mdnie_read_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].mdnie_read_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		/* CONFIG_DEBUG_LDI_STATUS */
		snprintf(string, PARSE_STRING, "samsung,ldi_debug0_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].ldi_debug0_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].ldi_debug0_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].ldi_debug0_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,ldi_debug1_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].ldi_debug1_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].ldi_debug1_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].ldi_debug1_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,ldi_debug2_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].ldi_debug2_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].ldi_debug2_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].ldi_debug2_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		/* CONFIG_TEMPERATURE_ELVSS */
		snprintf(string, PARSE_STRING, "samsung,elvss_lowtemp_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].elvss_lowtemp_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].elvss_lowtemp_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].elvss_lowtemp_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,elvss_lowtemp2_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].elvss_lowtemp2_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].elvss_lowtemp2_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].elvss_lowtemp2_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		/* CONFIG_SMART_ACL */
		snprintf(string, PARSE_STRING, "samsung,smart_acl_elvss_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].smart_acl_elvss_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].smart_acl_elvss_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].smart_acl_elvss_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,smart_acl_elvss_map_table_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_panel_table(np, &vdd->dtsi_data[ndx].smart_acl_elvss_map_table[panel_revision], string) && panel_revision > 0) /* TABLE */
			memcpy(&vdd->dtsi_data[ndx].smart_acl_elvss_map_table[panel_revision], &vdd->dtsi_data[ndx].smart_acl_elvss_map_table[panel_revision - 1], sizeof(struct cmd_map));

		/* CONFIG_PARTIAL_UPDATE */
		snprintf(string, PARSE_STRING, "samsung,partial_display_on_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].partial_display_on_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].partial_display_on_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].partial_display_on_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,partial_display_off_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].partial_display_off_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].partial_display_off_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].partial_display_off_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,partial_display_column_row_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].partial_display_column_row_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].partial_display_column_row_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].partial_display_column_row_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		/* CONFIG_ALPM_MODE */
		snprintf(string, PARSE_STRING, "samsung,alpm_on_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].alpm_on_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].alpm_on_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].alpm_on_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,alpm_off_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].alpm_off_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].alpm_off_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].alpm_off_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		/* HMT */
		snprintf(string, PARSE_STRING, "samsung,hmt_gamma_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hmt_gamma_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hmt_gamma_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hmt_gamma_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hmt_elvss_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hmt_elvss_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hmt_elvss_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hmt_elvss_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hmt_vint_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hmt_vint_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hmt_vint_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hmt_vint_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hmt_enable_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hmt_enable_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hmt_enable_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hmt_enable_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hmt_disable_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hmt_disable_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hmt_disable_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hmt_disable_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hmt_reverse_enable_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hmt_reverse_enable_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hmt_reverse_enable_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hmt_reverse_enable_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hmt_reverse_disable_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hmt_reverse_disable_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hmt_reverse_disable_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hmt_reverse_disable_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hmt_aid_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hmt_aid_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hmt_aid_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].hmt_aid_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,hmt_reverse_aid_map_table_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_panel_table(np, &vdd->dtsi_data[ndx].hmt_reverse_aid_map_table[panel_revision], string) && panel_revision > 0) /* TABLE */
			memcpy(&vdd->dtsi_data[ndx].hmt_reverse_aid_map_table[panel_revision], &vdd->dtsi_data[ndx].hmt_reverse_aid_map_table[panel_revision - 1], sizeof(struct cmd_map));
		snprintf(string, PARSE_STRING, "samsung,hmt_150cd_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].hmt_150cd_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].hmt_150cd_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].hmt_150cd_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));


		snprintf(string, PARSE_STRING, "samsung,hmt_candela_map_table_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_candella_lux_mapping_table(np, &vdd->dtsi_data[ndx].hmt_candela_map_table[panel_revision], string) && panel_revision > 0) /* TABLE */
			memcpy(&vdd->dtsi_data[ndx].hmt_candela_map_table[panel_revision], &vdd->dtsi_data[ndx].hmt_candela_map_table[panel_revision - 1], sizeof(struct candella_lux_map));

		/* CONFIG_FPS_CHANGE */
		snprintf(string, PARSE_STRING, "samsung,ldi_fps_change_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].ldi_fps_change_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].ldi_fps_change_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].ldi_fps_change_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,ldi_fps_rx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].ldi_fps_rx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].ldi_fps_rx_cmds[panel_revision], &vdd->dtsi_data[ndx].ldi_fps_rx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		/* TFT PWM CONTROL */
		snprintf(string, PARSE_STRING, "samsung,tft_pwm_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].tft_pwm_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].tft_pwm_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].tft_pwm_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,scaled_level_map_table_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_candella_lux_mapping_table(np, &vdd->dtsi_data[ndx].scaled_level_map_table[panel_revision], string) && panel_revision > 0) /* SCALED LEVEL MAP TABLE */
			memcpy(&vdd->dtsi_data[ndx].scaled_level_map_table[panel_revision], &vdd->dtsi_data[ndx].scaled_level_map_table[panel_revision - 1], sizeof(struct candella_lux_map));

		/* Additional Command */
		snprintf(string, PARSE_STRING, "samsung,packet_size_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].packet_size_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].packet_size_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].packet_size_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,reg_read_pos_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].reg_read_pos_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].reg_read_pos_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].reg_read_pos_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,osc_te_fitting_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].osc_te_fitting_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].osc_te_fitting_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].osc_te_fitting_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		/* VDDM OFFSET */
		snprintf(string, PARSE_STRING, "samsung,panel_ldi_vdd_read_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].read_vdd_ref_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].read_vdd_ref_cmds[panel_revision], &vdd->dtsi_data[ndx].read_vdd_ref_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,panel_ldi_vddm_read_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].read_vddm_ref_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].read_vddm_ref_cmds[panel_revision], &vdd->dtsi_data[ndx].read_vddm_ref_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,panel_ldi_vdd_offset_write_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].write_vdd_offset_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].write_vdd_offset_cmds[panel_revision], &vdd->dtsi_data[ndx].write_vdd_offset_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,panel_ldi_vddm_offset_write_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].write_vddm_offset_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].write_vddm_offset_cmds[panel_revision], &vdd->dtsi_data[ndx].write_vddm_offset_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		/* TFT CABC CONTROL */
		snprintf(string, PARSE_STRING, "samsung,cabc_on_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].cabc_on_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].cabc_on_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].cabc_on_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));

		snprintf(string, PARSE_STRING, "samsung,cabc_off_tx_cmds_rev%c", panel_revision + rev_value);
		if (mdss_samsung_parse_dcs_cmds(np, &vdd->dtsi_data[ndx].cabc_off_tx_cmds[panel_revision], string, NULL) && panel_revision > 0)
			memcpy(&vdd->dtsi_data[ndx].cabc_off_tx_cmds[panel_revision], &vdd->dtsi_data[ndx].cabc_off_tx_cmds[panel_revision - 1], sizeof(struct dsi_panel_cmds));
	}
}


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
u8 alpm_status_func(u8 flag)
{
	static u8 current_status = 0;
	static u8 previous_status = 0;
	u8 ret = 0;

	if(!vdd_data.support_alpm)
		return 0;

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

void mdss_samsung_panel_pbaboot_config(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_panel_info *pinfo = NULL;
	struct mdss_debug_data *mdd =
		(struct mdss_debug_data *)((mdss_mdp_get_mdata())->debug_inf.debug_data);
	struct samsung_display_driver_data *vdd = NULL;

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(mdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx, mdd : 9x%zx\n", __func__,
				(size_t)ctrl, (size_t)mdd);
		return;
	}

	pinfo = &ctrl->panel_data.panel_info;
	vdd = check_valid_ctrl(ctrl);

	/* Support PBA boot without lcd */
	if (!get_lcd_attached("GET") &&
			!IS_ERR_OR_NULL(pinfo) &&
			!IS_ERR_OR_NULL(vdd)) {
		pr_err("%s force VIDEO_MODE : %d\n", __func__, ctrl->ndx);
		pinfo->type = MIPI_VIDEO_PANEL;
		pinfo->mipi.mode = DSI_VIDEO_MODE;
		pinfo->mipi.traffic_mode = DSI_BURST_MODE;
		pinfo->mipi.bllp_power_stop = true;
		pinfo->mipi.te_sel = 0;
		pinfo->mipi.vsync_enable = 0;
		pinfo->mipi.hw_vsync_mode = 0;
		pinfo->mipi.force_clk_lane_hs = true;
		pinfo->mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB888;

		pinfo->cont_splash_enabled = false;

		pinfo->esd_check_enabled = false;
		ctrl->on_cmds.link_state = DSI_LP_MODE;
		ctrl->off_cmds.link_state = DSI_LP_MODE;

		/* To avoid underrun panic*/
		mdd->logd.xlog_enable = 0;
		vdd->dtsi_data[ctrl->ndx].samsung_osc_te_fitting = false;
	}
}

void mdss_samsung_panel_parse_dt_esd(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc = 0;
	const char *data;

	struct samsung_display_driver_data *vdd = NULL;
	struct mdss_panel_info *pinfo = NULL;

	if (!ctrl) {
		pr_err("%s: ctrl is null\n", __func__);
		return;
	}

	vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd : 0x%zx\n", __func__, (size_t)vdd);
		return;
	}

	pinfo = &ctrl->panel_data.panel_info;

	if (!pinfo->esd_check_enabled)
		return;

	vdd->esd_recovery.esd_gpio = of_get_named_gpio(np, "qcom,esd_irq_gpio", 0);

	if (gpio_is_valid(vdd->esd_recovery.esd_gpio)) {
		pr_info("%s:ndx(%d) esd gpio : %d, irq : %d\n",
				__func__,ctrl->ndx,
				vdd->esd_recovery.esd_gpio,
				gpio_to_irq(vdd->esd_recovery.esd_gpio));
	}

	rc = of_property_read_string(np, "qcom,mdss-dsi-panel-status-check-mode", &data);
	if (!rc) {
		if (!strcmp(data, "reg_read_irq")) {

			pr_info("%s: ESD_REG_IRQ set\n", __func__);
			ctrl->status_mode = ESD_REG_IRQ;
			ctrl->status_cmds_rlen = 0;
			ctrl->check_read_status = mdss_dsi_esd_irq_status;
		}
	}

	rc = of_property_read_string(np, "qcom,mdss-dsi-panel-status-irq-trigger", &data);
	if (!rc) {
		vdd->esd_recovery.irqflags = IRQF_ONESHOT;

		pr_info("%s: mdss-dsi-panel-status-irq-trigger set\n", __func__);
		if (!strcmp(data, "rising"))
			vdd->esd_recovery.irqflags |= IRQF_TRIGGER_RISING;
		else if (!strcmp(data, "falling"))
			vdd->esd_recovery.irqflags |= IRQF_TRIGGER_FALLING;
		else if (!strcmp(data, "high"))
			vdd->esd_recovery.irqflags |= IRQF_TRIGGER_HIGH;
		else if (!strcmp(data, "low"))
			vdd->esd_recovery.irqflags |= IRQF_TRIGGER_LOW;
	}
}

void mdss_samsung_panel_parse_dt(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc, i;
	u32 tmp[2];
	char panel_extra_power_gpio[] = "samsung,panel-extra-power-gpio1";
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	rc = of_property_read_u32(np, "samsung,support_panel_max", tmp);
	vdd->support_panel_max = !rc ? tmp[0] : 1;


	/* Set LP11 init flag */
	vdd->dtsi_data[ctrl->ndx].samsung_lp11_init = of_property_read_bool(np, "samsung,dsi-lp11-init");

	pr_info("%s: LP11 init %s\n", __func__,
		vdd->dtsi_data[ctrl->ndx].samsung_lp11_init ? "enabled" : "disabled");

	vdd->support_alpm  = of_property_read_bool(np, "qcom,mdss-dsi-alpm-enable");

	pr_err("%s: alpm enable %s\n", __func__,
		vdd->dtsi_data[ctrl->ndx].samsung_alpm_enable ? "enabled" : "disabled");

	/*Set OSC TE fitting flag */
	vdd->dtsi_data[ctrl->ndx].samsung_osc_te_fitting =
		of_property_read_bool(np, "samsung,osc-te-fitting-enable");

	if (vdd->dtsi_data[ctrl->ndx].samsung_osc_te_fitting) {
		rc = of_property_read_u32_array(np, "samsung,osc-te-fitting-cmd-index", tmp, 2);

		if (!rc) {
			vdd->dtsi_data[ctrl->ndx].samsung_osc_te_fitting_cmd_index[0] =
				tmp[0];
			vdd->dtsi_data[ctrl->ndx].samsung_osc_te_fitting_cmd_index[1] =
				tmp[1];
		}

		rc = of_property_read_u32(np, "samsung,osc-te-fitting-sampling-rate", tmp);

		vdd->te_fitting_info.sampling_rate = !rc ? tmp[0] : 2;

	}

	pr_info("%s: OSC TE fitting %s\n", __func__,
		vdd->dtsi_data[0].samsung_osc_te_fitting ? "enabled" : "disabled");

	/* Set HMT flag */
	vdd->dtsi_data[0].hmt_enabled = of_property_read_bool(np, "samsung,hmt_enabled");
	if (vdd->dtsi_data[0].hmt_enabled)
		for (i = 1; i < vdd->support_panel_max; i++)
			vdd->dtsi_data[i].hmt_enabled = true;

	pr_info("%s: hmt %s\n", __func__,
		vdd->dtsi_data[0].hmt_enabled ? "enabled" : "disabled");

	/* Set TFT flag */
	vdd->dtsi_data[ctrl->ndx].tft_common_support  = of_property_read_bool(np,
		"samsung,tft-common-support");
	vdd->dtsi_data[ctrl->ndx].backlight_gpio_config = of_property_read_bool(np,
		"samsung,backlight-gpio-config");

	/* Set extra power gpio */
	for (i = 0; i < MAX_EXTRA_POWER_GPIO; i++) {
		panel_extra_power_gpio[strlen(panel_extra_power_gpio) - 1] = '1' + i;
		vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i] =
				 of_get_named_gpio(np,
						panel_extra_power_gpio, 0);
		if (!gpio_is_valid(vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i]))
			pr_err("%s:%d, panel_extra_power gpio%d not specified\n",
							__func__, __LINE__, i+1);
		else
			pr_err("extra gpio num : %d\n", vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i]);
	}


	mdss_samsung_panel_parse_dt_cmds(np, ctrl);
	mdss_samsung_panel_parse_dt_esd(np, ctrl);
	mdss_samsung_panel_pbaboot_config(np, ctrl);
}


/************************************************************
*
*		SYSFS RELATED FUNCTION
*
**************************************************************/
#if defined(CONFIG_LCD_CLASS_DEVICE)
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

static void sending_tune_cmd(struct device *dev, char *src, int len)
{
	int data_pos;
	int cmd_step;
	int cmd_pos;

	char *mdnie_tuning1;
	char *mdnie_tuning2;
	struct dsi_cmd_desc *mdnie_tune_cmd;

	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error\n", __func__);
		return;
	}

	if (!vdd->mdnie_tune_size1 || !vdd->mdnie_tune_size2) {
		pr_err("%s : mdnie_tune_size is zero 1(%d) 2(%d)\n",
			__func__, vdd->mdnie_tune_size1, vdd->mdnie_tune_size2);
		return;
	}

	mdnie_tune_cmd = kzalloc(2 * sizeof(struct dsi_cmd_desc), GFP_KERNEL);

	mdnie_tuning1 = kzalloc(sizeof(char) * vdd->mdnie_tune_size1, GFP_KERNEL);
	mdnie_tuning2 = kzalloc(sizeof(char) * vdd->mdnie_tune_size2, GFP_KERNEL);

	cmd_step = 0;
	cmd_pos = 0;

	for (data_pos = 0; data_pos < len;) {
		if (*(src + data_pos) == '0') {
			if (*(src + data_pos + 1) == 'x') {
				if (!cmd_step) {
					mdnie_tuning1[cmd_pos] =
					char_to_dec(*(src + data_pos + 2),
							*(src + data_pos + 3));
				} else {
					mdnie_tuning2[cmd_pos] =
					char_to_dec(*(src + data_pos + 2),
							*(src + data_pos + 3));
				}

				data_pos += 3;
				cmd_pos++;

				if (cmd_pos == vdd->mdnie_tune_size1 && !cmd_step) {
					cmd_pos = 0;
					cmd_step = 1;
				}
			} else
				data_pos++;
		} else {
			data_pos++;
		}
	}

	mdnie_tune_cmd[0].dchdr.dtype = DTYPE_DCS_LWRITE;
	mdnie_tune_cmd[0].dchdr.last = 1;
	mdnie_tune_cmd[0].dchdr.dlen = vdd->mdnie_tune_size1;
	mdnie_tune_cmd[0].payload = mdnie_tuning1;

	mdnie_tune_cmd[1].dchdr.dtype = DTYPE_DCS_LWRITE;
	mdnie_tune_cmd[1].dchdr.last = 1;
	mdnie_tune_cmd[1].dchdr.dlen = vdd->mdnie_tune_size2;
	mdnie_tune_cmd[1].payload = mdnie_tuning2;

	printk(KERN_ERR "\n");
	for (data_pos = 0; data_pos < vdd->mdnie_tune_size1 ; data_pos++)
		printk(KERN_ERR "0x%x ", mdnie_tuning1[data_pos]);
	printk(KERN_ERR "\n");
	for (data_pos = 0; data_pos < vdd->mdnie_tune_size2 ; data_pos++)
		printk(KERN_ERR "0x%x ", mdnie_tuning2[data_pos]);
	printk(KERN_ERR "\n");

	if (IS_ERR_OR_NULL(vdd))
		pr_err("%s vdd is error", __func__);
	else {
#if defined(CONFIG_DUAL_PANEL)
		vdd->mdnie_tune_data[0].mdnie_tune_packet_tx_cmds_dsi.cmds = mdnie_tune_cmd;
		vdd->mdnie_tune_data[0].mdnie_tune_packet_tx_cmds_dsi.cmd_cnt = 2;

		vdd->mdnie_tune_data[1].mdnie_tune_packet_tx_cmds_dsi.cmds = mdnie_tune_cmd;
		vdd->mdnie_tune_data[1].mdnie_tune_packet_tx_cmds_dsi.cmd_cnt = 2;

		/* TODO: Tx command */
#else
		if((vdd->ctrl_dsi[DSI_CTRL_0]->cmd_sync_wait_broadcast)
		&& (vdd->ctrl_dsi[DSI_CTRL_1]->cmd_sync_wait_trigger)){ /* Dual DSI & dsi 1 trigger */
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_1], PANEL_LEVE2_KEY_ENABLE);
			vdd->mdnie_tune_data[DSI_CTRL_1].mdnie_tune_packet_tx_cmds_dsi.cmds = mdnie_tune_cmd;
			vdd->mdnie_tune_data[DSI_CTRL_1].mdnie_tune_packet_tx_cmds_dsi.cmd_cnt = 2;
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_1], PANEL_MDNIE_TUNE);
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_1], PANEL_LEVE2_KEY_DISABLE);
		} else { /* Single DSI, dsi 0 trigger */
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_0], PANEL_LEVE2_KEY_ENABLE);
			vdd->mdnie_tune_data[DSI_CTRL_0].mdnie_tune_packet_tx_cmds_dsi.cmds = mdnie_tune_cmd;
			vdd->mdnie_tune_data[DSI_CTRL_0].mdnie_tune_packet_tx_cmds_dsi.cmd_cnt = 2;
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_0], PANEL_MDNIE_TUNE);
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_0], PANEL_LEVE2_KEY_DISABLE);
		}
#endif
	}
}

static void load_tuning_file(struct device *dev, char *filename)
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
		goto err;
	}

	l = filp->f_path.dentry->d_inode->i_size;
	pr_info("%s Loading File Size : %ld(bytes)", __func__, l);

	dp = kmalloc(l + 10, GFP_KERNEL);
	if (dp == NULL) {
		pr_info("Can't not alloc memory for tuning file load\n");
		filp_close(filp, current->files);
		goto err;
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
		goto err;
	}

	filp_close(filp, current->files);

	set_fs(fs);

	sending_tune_cmd(dev, dp, l);

	kfree(dp);

	return;
err:
	set_fs(fs);
}

static ssize_t tuning_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, MAX_FILE_NAME, "Tunned File Name : %s\n", tuning_file);
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

	load_tuning_file(dev, tuning_file);

	return size;
}

static DEVICE_ATTR(tuning, 0664, tuning_show, tuning_store);
#endif

static ssize_t mdss_samsung_disp_lcdtype_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	static int string_size = 100;
	char temp[string_size];
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return strnlen(buf, string_size);
	}

	if(vdd->manufacture_id_dsi[vdd->support_panel_max - 1])
		snprintf(temp, 20, "SDC_%x\n", vdd->manufacture_id_dsi[vdd->support_panel_max - 1]);
	else
		pr_info("no manufacture id\n");

	strlcat(buf, temp, string_size);

	return strnlen(buf, string_size);
}

static ssize_t mdss_samsung_disp_windowtype_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	static int string_size = 15;
	char temp[string_size];
	int id, id1, id2, id3;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return strnlen(buf, string_size);
	}

	id = vdd->manufacture_id_dsi[vdd->support_panel_max - 1];

	id1 = (id & 0x00FF0000) >> 16;
	id2 = (id & 0x0000FF00) >> 8;
	id3 = id & 0xFF;

	snprintf(temp, sizeof(temp), "%x %x %x\n", id1, id2, id3);

	strlcat(buf, temp, string_size);

	return strnlen(buf, string_size);
}

static ssize_t mdss_samsung_disp_manufacture_date_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	static int string_size = 30;
	char temp[string_size];
	int date;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return strnlen(buf, string_size);
	}

	date = vdd->manufacture_date_dsi[vdd->support_panel_max - 1];
	snprintf((char *)temp, sizeof(temp), "manufacture date : %d\n", date);

	strlcat(buf, temp, string_size);

	pr_info("manufacture date : %d\n",date);

	return strnlen(buf, string_size);
}

static ssize_t mdss_samsung_disp_manufacture_code_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	static int string_size = 30;
	char temp[string_size];
	int *ddi_id;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return strnlen(buf, string_size);
	}

	ddi_id = &vdd->ddi_id_dsi[vdd->support_panel_max - 1][0];

	snprintf((char *)temp, sizeof(temp), "%02x%02x%02x%02x%02x\n",
		ddi_id[0], ddi_id[1], ddi_id[2], ddi_id[3], ddi_id[4]);

	strlcat(buf, temp, string_size);

	pr_info("%s : %02x %02x %02x %02x %02x\n", __func__,
		ddi_id[0], ddi_id[1], ddi_id[2], ddi_id[3], ddi_id[4]);

	return strnlen(buf, string_size);
}

static ssize_t mdss_samsung_disp_acl_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return rc;
	}

	rc = snprintf((char *)buf, sizeof(vdd->acl_status), "%d\n", vdd->acl_status);

	pr_info("acl status: %d\n", *buf);

	return rc;
}

static ssize_t mdss_samsung_disp_acl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int acl_set = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);
	struct mdss_panel_data *pdata;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		pr_err("%s vdd is error", __func__);
		return size;
	}

	if (sysfs_streq(buf, "1"))
		acl_set = true;
	else if (sysfs_streq(buf, "0"))
		acl_set = false;
	else
		pr_info("%s: Invalid argument!!", __func__);

	pr_info("%s (%d) \n", __func__, acl_set);

	pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;
	mutex_lock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);

	if (acl_set && !vdd->acl_status) {
		vdd->acl_status = acl_set;
		pdata->set_backlight(pdata, vdd->bl_level);
	} else if (!acl_set && vdd->acl_status) {
		vdd->acl_status = acl_set;
		pdata->set_backlight(pdata, vdd->bl_level);
	} else {
		vdd->acl_status = acl_set;
		pr_info("%s: skip acl update!! acl %d", __func__, vdd->acl_status);
	}

	mutex_unlock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);

	return size;
}

static ssize_t mdss_samsung_disp_siop_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return rc;
	}

	rc = snprintf((char *)buf, sizeof(vdd->siop_status), "%d\n", vdd->siop_status);

	pr_info("siop status: %d\n", *buf);

	return rc;
}

static ssize_t mdss_samsung_disp_siop_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int siop_set = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);
	struct mdss_panel_data *pdata;


	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		pr_err("%s vdd is error", __func__);
		return size;
	}

	pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;

	if (sysfs_streq(buf, "1"))
		siop_set = true;
	else if (sysfs_streq(buf, "0"))
		siop_set = false;
	else
		pr_info("%s: Invalid argument!!", __func__);

	pr_info("%s (%d) \n", __func__, siop_set);

	mutex_lock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);

	if (siop_set && !vdd->siop_status) {
		vdd->siop_status = siop_set;
		pdata->set_backlight(pdata, vdd->bl_level);
	} else if (!siop_set && vdd->siop_status) {
		vdd->siop_status = siop_set;
		pdata->set_backlight(pdata, vdd->bl_level);
	} else {
		vdd->siop_status = siop_set;
		pr_info("%s: skip siop update!! acl %d", __func__, vdd->acl_status);
	}

	mutex_unlock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);

	return size;
}

static ssize_t mdss_samsung_aid_log_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int loop = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return rc;
	}

	for (loop = 0; loop < vdd->support_panel_max; loop++) {
		if (vdd->smart_dimming_dsi[loop] && vdd->smart_dimming_dsi[loop]->print_aid_log)
			vdd->smart_dimming_dsi[loop]->print_aid_log(vdd->smart_dimming_dsi[loop]);
		else
			pr_err("%s DSI%d smart dimming is not loaded\n", __func__, loop);
	}

	if (vdd->dtsi_data[0].hmt_enabled) {
		for (loop = 0; loop < vdd->support_panel_max; loop++) {
			if (vdd->smart_dimming_dsi_hmt[loop] && vdd->smart_dimming_dsi_hmt[loop]->print_aid_log)
				vdd->smart_dimming_dsi_hmt[loop]->print_aid_log(vdd->smart_dimming_dsi_hmt[loop]);
			else
				pr_err("%s DSI%d smart dimming hmt is not loaded\n", __func__, loop);
		}
	}

	return rc;
}

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
static ssize_t mdss_samsung_auto_brightness_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc =0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return rc;
	}

	rc = snprintf((char *)buf, sizeof(vdd->auto_brightness), "%d\n", vdd->auto_brightness);

	pr_info("auto_brightness: %d\n", *buf);

	return rc;
}

static ssize_t mdss_samsung_auto_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);
	struct mdss_panel_data *pdata;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		pr_err("%s vdd is error", __func__);
		return size;
	}

	pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;

	if (sysfs_streq(buf, "0"))
		vdd->auto_brightness = 0;
	else if (sysfs_streq(buf, "1"))
		vdd->auto_brightness = 1;
	else if (sysfs_streq(buf, "2"))
		vdd->auto_brightness = 2;
	else if (sysfs_streq(buf, "3"))
		vdd->auto_brightness = 3;
	else if (sysfs_streq(buf, "4"))
		vdd->auto_brightness = 4;
	else if (sysfs_streq(buf, "5"))
		vdd->auto_brightness = 5;
	else if (sysfs_streq(buf, "6")) // HBM mode
		vdd->auto_brightness = 6;
	else if (sysfs_streq(buf, "7"))
		vdd->auto_brightness = 7;
	else
		pr_info("%s: Invalid argument!!", __func__);

	pr_info("%s (%d) \n", __func__, vdd->auto_brightness);

	if (!alpm_status_func(CHECK_PREVIOUS_STATUS)) {
		mutex_lock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);
		pdata->set_backlight(pdata, vdd->bl_level);
		mutex_unlock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);

		if (vdd->support_mdnie_lite)
			update_dsi_tcon_mdnie_register(vdd);
	} else
		pr_err("[ALPM_DEBUG]  %s : ALPM is on. do not set brightness and mdnie..  \n", __func__);

	return size;
}
#endif

static ssize_t mdss_samsung_read_mtp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int addr, len, start;
	char *read_buf = NULL;
	char read_size, read_startoffset;
	struct dsi_panel_cmds *rx_cmds;
	struct mdss_dsi_ctrl_pdata *ctrl;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		pr_err("%s vdd is error", __func__);
		return size;
	}
	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	if (IS_ERR_OR_NULL(ctrl)) {
		pr_err("%s ctrl is error", __func__);
		return size;
	}

	sscanf(buf, "%x %d %x" , &addr, &len, &start);

	read_buf = kmalloc(len * sizeof(char), GFP_KERNEL);

	pr_info("%x %d %x\n", addr, len, start);

	rx_cmds = &(vdd->dtsi_data[ctrl->ndx].mtp_read_sysfs_rx_cmds[vdd->panel_revision]);

	rx_cmds->cmds[0].payload[0] =  addr;
	rx_cmds->cmds[0].payload[1] =  len;
	rx_cmds->cmds[0].payload[2] =  start;

	read_size = len;
	read_startoffset = start;

	rx_cmds->read_size =  &read_size;
	rx_cmds->read_startoffset =  &read_startoffset;


	pr_info("%x %x %x %x %x %x %x %x %x\n",
		rx_cmds->cmds[0].dchdr.dtype,
		rx_cmds->cmds[0].dchdr.last,
		rx_cmds->cmds[0].dchdr.vc,
		rx_cmds->cmds[0].dchdr.ack,
		rx_cmds->cmds[0].dchdr.wait,
		rx_cmds->cmds[0].dchdr.dlen,
		rx_cmds->cmds[0].payload[0],
		rx_cmds->cmds[0].payload[1],
		rx_cmds->cmds[0].payload[2]);

	mdss_samsung_panel_data_read(ctrl, rx_cmds, read_buf, PANEL_LEVE2_KEY);

	kfree(read_buf);

	return size;
}

static ssize_t mdss_samsung_temperature_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return rc;
	}

	rc = snprintf((char *)buf, 40,"-20, -19, 0, 1, 30, 40\n");

	pr_info("%s temperature : %d", __func__, vdd->temperature);

	return rc;
}

static ssize_t mdss_samsung_temperature_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);
	struct mdss_panel_data *pdata;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		pr_err("%s vdd is error", __func__);
		return size;
	}

	pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;

	sscanf(buf, "%d" , &vdd->temperature);

	mutex_lock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);
	pdata->set_backlight(pdata, vdd->bl_level);
	mutex_unlock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);

	pr_info("%s temperature : %d", __func__, vdd->temperature);

	return size;
}

static ssize_t mdss_samsung_disp_partial_disp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	pr_debug("TDB");

	return 0;
}

static ssize_t mdss_samsung_disp_partial_disp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("TDB");

	return size;
}

static ssize_t mdss_samsung_alpm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;
	struct samsung_display_driver_data *vdd =
	(struct samsung_display_driver_data *)dev_get_drvdata(dev);
	struct mdss_dsi_ctrl_pdata *ctrl;
	u8 current_status = 0;

	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	if (!vdd->support_alpm)
		return rc;

	current_status = alpm_status_func(CHECK_CURRENT_STATUS);

	rc = snprintf((char *)buf, 30, "%d\n", current_status);
	pr_info("[ALPM_DEBUG] %s: current status : %d \n",\
					 __func__, (int)current_status);

	return rc;
}

static ssize_t mdss_samsung_alpm_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int mode = 0;
	struct samsung_display_driver_data *vdd =
	(struct samsung_display_driver_data *)dev_get_drvdata(dev);
	struct mdss_dsi_ctrl_pdata *ctrl;
	struct mdss_panel_data *pdata;
	struct mdss_panel_info *pinfo;
	static int backup_bl_level;

	pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;
	pinfo = &pdata->panel_info;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		pr_err("%s vdd is error", __func__);
		return size;
	}

	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	if (!vdd->support_alpm)
		return size;


	sscanf(buf, "%d" , &mode);
	pr_info("[ALPM_DEBUG] %s: mode : %d\n", __func__, mode);

	/*
	 * Possible mode status for Blank(0) or Unblank(1)
	 *	* Blank *
	 *		1) ALPM_MODE_ON
	 *			-> That will set during wakeup
	 *	* Unblank *
	 *		1) NORMAL_MODE_ON
	 *			-> That will send partial update commands
	 */
	alpm_status_func(mode ? ALPM_MODE_ON : MODE_OFF);
	if (mode == ALPM_MODE_ON) {
		/*
		 * This will work if the ALPM must be on or chagne partial area
		 * if that already in the status of unblank
		 */
		if (pinfo->panel_state) {
			if (!alpm_status_func(CHECK_PREVIOUS_STATUS)\
					&& alpm_status_func(CHECK_CURRENT_STATUS)) {
				/* Set min brightness to prevent panel malfunction with ALPM */
				pr_info("[ALPM_DEBUG] Set Min Birghtness \n");
				mutex_lock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);

				if (vdd->bl_level)
					backup_bl_level = vdd->bl_level;

				pdata->set_backlight(pdata, ALPM_BRIGHTNESS);
				mutex_unlock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);
				/* Turn On ALPM Mode */
				mdss_samsung_send_cmd(ctrl, PANEL_ALPM_ON);
				msleep(20); /* wait 1 frame(more than 16ms) */
				mdss_samsung_send_cmd(ctrl, PANEL_DISPLAY_ON);
				alpm_status_func(STORE_CURRENT_STATUS);
				pr_info("[ALPM_DEBUG] %s: Send ALPM mode on cmds\n", __func__);
			}
		}
	} else if (mode == MODE_OFF) {
		if (alpm_status_func(CHECK_PREVIOUS_STATUS) == ALPM_MODE_ON) {
				if (pinfo->panel_state) {
					mdss_samsung_send_cmd(ctrl, PANEL_ALPM_OFF);
					msleep(20); /* wait 1 frame(more than 16ms) */
					mdss_samsung_send_cmd(ctrl, PANEL_DISPLAY_ON);
					alpm_status_func(CLEAR_MODE_STATUS);

					mutex_lock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);

					if (vdd->bl_level)
						backup_bl_level = vdd->bl_level;

					pdata->set_backlight(pdata, backup_bl_level);
					mutex_unlock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);

					pr_info("[ALPM_DEBUG] %s: Send ALPM off cmds\n", __func__);
				}
			}
	} else
		pr_info("[ALPM_DEBUG] %s: no operation \n:", __func__);

	return size;
}
static ssize_t mdss_samsung_alpm_backlight_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	pr_debug("TDB");

	return 0;
}

static ssize_t mdss_samsung_alpm_backlight_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("TDB");

	return size;
}

int hmt_bright_update(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_panel_data *pdata;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;

	msleep(20);

	if (vdd->hmt_stat.hmt_on) {
		mdss_samsung_brightness_dcs_hmt(ctrl, vdd->hmt_stat.hmt_bl_level);
	} else {
		mutex_lock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);
		pdata->set_backlight(pdata, vdd->bl_level);
		mutex_unlock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);
		pr_info("%s : hmt off state! \n",__func__);
	}

	return 0;
}

int hmt_enable(struct mdss_dsi_ctrl_pdata *ctrl, int enable)
{
	if (enable) {
		pr_info("Single Scan Enable ++ \n");
		mdss_samsung_send_cmd(ctrl, PANEL_HMT_ENABLE);
		pr_info("Single Scan Enable -- \n");
	} else {
		mdss_samsung_send_cmd(ctrl, PANEL_HMT_DISABLE);
		pr_info("HMT OFF.. \n");
	}

	return 0;
}

int hmt_reverse_update(struct mdss_dsi_ctrl_pdata *ctrl, int enable)
{
	if (enable) {
		pr_info("REVERSE ENABLE ++\n");
		mdss_samsung_send_cmd(ctrl, PANEL_HMT_REVERSE_ENABLE);
		pr_info("REVERSE ENABLE --\n");
	} else {
		pr_info("REVERSE DISABLE ++ \n");
		mdss_samsung_send_cmd(ctrl, PANEL_HMT_REVERSE_DISABLE);
		pr_info("REVERSE DISABLE -- \n");
	}

	return 0;
}

static ssize_t mipi_samsung_hmt_bright_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;

	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return -ENODEV;
	}

	if (!vdd->dtsi_data[0].hmt_enabled) {
		pr_err("%s : hmt is not supported..\n", __func__);
		return -ENODEV;
	}

	rc = snprintf((char *)buf, 30, "%d\n", vdd->hmt_stat.hmt_bl_level);
	pr_info("[HMT] hmt bright : %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_hmt_bright_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int input;
	struct mdss_panel_info *pinfo = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl;

	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return -ENODEV;
	}

	if (!vdd->dtsi_data[0].hmt_enabled) {
		pr_err("%s : hmt is not supported..\n", __func__);
		return -ENODEV;
	}

	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	pinfo = &ctrl->panel_data.panel_info;

	sscanf(buf, "%d" , &input);
	pr_info("[HMT] %s: input (%d) ++ \n", __func__, input);

	if (!vdd->hmt_stat.hmt_on) {
		pr_info("[HMT] hmt is off!\n");
		return size;
	}

	if (!pinfo->blank_state) {
		pr_err("[HMT] panel is off!\n");
		vdd->hmt_stat.hmt_bl_level = input;
		return size;
	}

	if (vdd->hmt_stat.hmt_bl_level == input) {
		pr_err("[HMT] hmt bright already %d!\n", vdd->hmt_stat.hmt_bl_level);
		return size;
	}

	vdd->hmt_stat.hmt_bl_level = input;
	hmt_bright_update(ctrl);

	pr_info("[HMT] %s: input (%d) -- \n", __func__, input);

	return size;
}

static ssize_t mipi_samsung_hmt_on_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;

	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return -ENODEV;
	}

	if (!vdd->dtsi_data[0].hmt_enabled) {
		pr_err("%s : hmt is not supported..\n", __func__);
		return -ENODEV;
	}

	rc = snprintf((char *)buf, 30, "%d\n", vdd->hmt_stat.hmt_on);
	pr_info("[HMT] hmt on input : %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_hmt_on_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int input;
	struct mdss_panel_info *pinfo = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl;

	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return -ENODEV;
	}

	if (!vdd->dtsi_data[0].hmt_enabled) {
		pr_err("%s : hmt is not supported..\n", __func__);
		return -ENODEV;
	}

	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	pinfo = &ctrl->panel_data.panel_info;

	sscanf(buf, "%d" , &input);
	pr_info("[HMT] %s: input (%d) ++ \n", __func__, input);

	if (!pinfo->blank_state) {
		pr_err("[HMT] panel is off!\n");
		vdd->hmt_stat.hmt_on = input;
		return size;
	}

	if (vdd->hmt_stat.hmt_on == input) {
		pr_info("[HMT] hmt already %s !\n", vdd->hmt_stat.hmt_on?"ON":"OFF");
		return size;
	}

	vdd->hmt_stat.hmt_on = input;

	if (vdd->hmt_stat.hmt_on && vdd->hmt_stat.hmt_low_persistence) {
		hmt_enable(ctrl, 1);
		hmt_reverse_update(ctrl, 1);
	} else {
		hmt_enable(ctrl, 0);
		hmt_reverse_update(ctrl, 0);
	}

	hmt_bright_update(ctrl);

	pr_info("[HMT] %s: input hmt (%d) hmt lp (%d)-- \n",
		__func__, input, vdd->hmt_stat.hmt_low_persistence);

	return size;
}

static ssize_t mipi_samsung_hmt_low_persistence_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;

	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return -ENODEV;
	}

	if (!vdd->dtsi_data[0].hmt_enabled) {
		pr_err("%s : hmt is not supported..\n", __func__);
		return -ENODEV;
	}

	rc = snprintf((char *)buf, 30, "%d\n", vdd->hmt_stat.hmt_low_persistence);
	pr_info("[HMT] hmt low persistence : %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_hmt_low_persistence_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int input;
	struct mdss_panel_info *pinfo = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl;

	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return -ENODEV;
	}

	if (!vdd->dtsi_data[0].hmt_enabled) {
		pr_err("%s : hmt is not supported..\n", __func__);
		return -ENODEV;
	}

	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	pinfo = &ctrl->panel_data.panel_info;

	sscanf(buf, "%d" , &input);
	pr_info("[HMT] %s: input (%d) ++ \n", __func__, input);

	if (!vdd->hmt_stat.hmt_on) {
		pr_info("[HMT] hmt is off!\n");
		return size;
	}

	if (vdd->hmt_stat.hmt_low_persistence == input) {
		pr_err("[HMT] already low_persistence (%d)\n",
			vdd->hmt_stat.hmt_low_persistence);
		return size;
	}

	if (!pinfo->blank_state) {
		pr_err("[HMT] panel is off!\n");
		vdd->hmt_stat.hmt_low_persistence = input;
		return size;
	}

	vdd->hmt_stat.hmt_low_persistence = input;

	if (!vdd->hmt_stat.hmt_low_persistence) {
		hmt_enable(ctrl, 0);
		hmt_reverse_update(ctrl, 0);
	} else {
		hmt_enable(ctrl, 1);
		hmt_reverse_update(ctrl, 1);
	}

	hmt_bright_update(ctrl);

	pr_info("[HMT] %s: input hmt (%d) hmt lp (%d)-- \n",
		__func__, vdd->hmt_stat.hmt_on, vdd->hmt_stat.hmt_low_persistence);

	return size;
}

void mdss_samsung_cabc_update()
{
	struct samsung_display_driver_data *vdd = samsung_get_vdd();

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return;
	}

	if(vdd->siop_status)
		mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], PANEL_CABC_ON);
	else
		mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], PANEL_CABC_OFF);
}

int config_cabc(int value)
{
	struct samsung_display_driver_data *vdd = samsung_get_vdd();

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return value;
	}

	vdd->siop_status = value;
	mdss_samsung_cabc_update();
	return 0;
}

static ssize_t mipi_samsung_esd_check_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct msm_fb_data_type *mfd;
	int rc;

	struct samsung_display_driver_data *vdd = samsung_get_vdd();
	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd : 0x%zx\n", __func__, (size_t)vdd);
		return 0;
	}
	pr_info("%s++  \n", __func__);

	if (vdd->ctrl_dsi[DISPLAY_1]->panel_data.panel_info.esd_check_enabled){
		rc = snprintf((char *)buf, 20, "esd_irq_handler %d\n", 0);
		mfd = vdd->mfd_dsi[DISPLAY_1]; /*esd enabled only on dsi 0*/

		esd_irq_handler(0, mfd);
	}else
		rc = snprintf((char *)buf, 20, "no esd_irq_handler %d\n", 0);

	return rc;
}

static DEVICE_ATTR(lcd_type, S_IRUGO, mdss_samsung_disp_lcdtype_show, NULL);
static DEVICE_ATTR(window_type, S_IRUGO, mdss_samsung_disp_windowtype_show, NULL);
static DEVICE_ATTR(manufacture_date, S_IRUGO, mdss_samsung_disp_manufacture_date_show, NULL);
static DEVICE_ATTR(manufacture_code, S_IRUGO, mdss_samsung_disp_manufacture_code_show, NULL);
static DEVICE_ATTR(power_reduce, S_IRUGO | S_IWUSR | S_IWGRP, mdss_samsung_disp_acl_show, mdss_samsung_disp_acl_store);
static DEVICE_ATTR(siop_enable, S_IRUGO | S_IWUSR | S_IWGRP, mdss_samsung_disp_siop_show, mdss_samsung_disp_siop_store);
static DEVICE_ATTR(read_mtp, S_IRUGO | S_IWUSR | S_IWGRP, NULL, mdss_samsung_read_mtp_store);
static DEVICE_ATTR(temperature, S_IRUGO | S_IWUSR | S_IWGRP, mdss_samsung_temperature_show, mdss_samsung_temperature_store);
static DEVICE_ATTR(aid_log, S_IRUGO | S_IWUSR | S_IWGRP, mdss_samsung_aid_log_show, NULL);
static DEVICE_ATTR(partial_disp, S_IRUGO | S_IWUSR | S_IWGRP, mdss_samsung_disp_partial_disp_show, mdss_samsung_disp_partial_disp_store);
static DEVICE_ATTR(alpm, S_IRUGO | S_IWUSR | S_IWGRP, mdss_samsung_alpm_show, mdss_samsung_alpm_store);
static DEVICE_ATTR(alpm_backlight, S_IRUGO | S_IWUSR | S_IWGRP, mdss_samsung_alpm_backlight_show, mdss_samsung_alpm_backlight_store);
static DEVICE_ATTR(hmt_bright, S_IRUGO | S_IWUSR | S_IWGRP, mipi_samsung_hmt_bright_show, mipi_samsung_hmt_bright_store);
static DEVICE_ATTR(hmt_on, S_IRUGO | S_IWUSR | S_IWGRP,	mipi_samsung_hmt_on_show, mipi_samsung_hmt_on_store);
static DEVICE_ATTR(hmt_low_persistence, S_IRUGO | S_IWUSR | S_IWGRP, mipi_samsung_hmt_low_persistence_show, mipi_samsung_hmt_low_persistence_store);
static DEVICE_ATTR(esd_check, /*S_IRUGO*/0664 , mipi_samsung_esd_check_show, NULL);

static struct attribute *panel_sysfs_attributes[] = {
	&dev_attr_lcd_type.attr,
	&dev_attr_window_type.attr,
	&dev_attr_manufacture_date.attr,
	&dev_attr_manufacture_code.attr,
	&dev_attr_power_reduce.attr,
	&dev_attr_siop_enable.attr,
	&dev_attr_aid_log.attr,
	&dev_attr_read_mtp.attr,
	&dev_attr_temperature.attr,
	&dev_attr_partial_disp.attr,
	&dev_attr_alpm.attr,
	&dev_attr_alpm_backlight.attr,
	&dev_attr_hmt_bright.attr,
	&dev_attr_hmt_on.attr,
	&dev_attr_hmt_low_persistence.attr,
	&dev_attr_esd_check.attr,
	NULL
};
static const struct attribute_group panel_sysfs_group = {
	.attrs = panel_sysfs_attributes,
};

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
static DEVICE_ATTR(auto_brightness, S_IRUGO | S_IWUSR | S_IWGRP,
			mdss_samsung_auto_brightness_show,
			mdss_samsung_auto_brightness_store);

static struct attribute *bl_sysfs_attributes[] = {
	&dev_attr_auto_brightness.attr,
	NULL
};
static const struct attribute_group bl_sysfs_group = {
	.attrs = bl_sysfs_attributes,
};
#endif /* END CONFIG_LCD_CLASS_DEVICE*/

static ssize_t csc_read_cfg(struct device *dev,
               struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", csc_update);
	return ret;
}

static ssize_t csc_write_cfg(struct device *dev,
               struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int err;
	int mode;

	err =  kstrtoint(buf, 0, &mode);
	if (err)
	       return ret;

	csc_update = (u8)mode;
	csc_change = 1;
	pr_info(" csc ctrl set to csc_update(%d)\n", csc_update);

	return ret;
}

static DEVICE_ATTR(csc_cfg, S_IRUGO | S_IWUSR, csc_read_cfg, csc_write_cfg);

int mdss_samsung_create_sysfs(void *data)
{
	static int sysfs_enable = 0;
	int rc = 0;
#if defined(CONFIG_LCD_CLASS_DEVICE)
	struct lcd_device *lcd_device;
#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	struct backlight_device *bd = NULL;
#endif
#endif
	struct device *csc_dev = vdd_data.mfd_dsi[0]->fbi->dev;

	/* sysfs creat func should be called one time in dual dsi mode */
	if (sysfs_enable)
		return 0;

#if defined(CONFIG_LCD_CLASS_DEVICE)
	lcd_device = lcd_device_register("panel", NULL, data, NULL);

	if (IS_ERR_OR_NULL(lcd_device)) {
		rc = PTR_ERR(lcd_device);
		pr_err("Failed to register lcd device..\n");
		return rc;
	}

	rc = sysfs_create_group(&lcd_device->dev.kobj, &panel_sysfs_group);
	if (rc) {
		pr_err("Failed to create panel sysfs group..\n");
		sysfs_remove_group(&lcd_device->dev.kobj, &panel_sysfs_group);
		return rc;
	}

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	bd = backlight_device_register("panel", &lcd_device->dev,
						data, NULL, NULL);
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

	rc = sysfs_create_file(&lcd_device->dev.kobj, &dev_attr_tuning.attr);
	if (rc) {
		pr_err("sysfs create fail-%s\n", dev_attr_tuning.attr.name);
		return rc;
	}
#endif

	rc = sysfs_create_file(&csc_dev->kobj, &dev_attr_csc_cfg.attr);
	if (rc) {
		pr_err("sysfs create fail-%s\n", dev_attr_csc_cfg.attr.name);
		return rc;
	}

	sysfs_enable = 1;

	pr_info("%s: done!! \n", __func__);

	return rc;
}

/************************************************************
*
*		MDSS & DSI REGISTER DUMP FUNCTION
*
**************************************************************/
void mdss_samsung_dump_regs(void)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	char name[32];
	int loop;
	size_t mdss_physical_address = mdata->physical_base;

	char *mdss_phy_base = (char *)mdata->mdss_base;
	size_t physical_base;

	snprintf(name, sizeof(name), "MDP BASE");
	physical_base = mdss_physical_address;
	pr_err("=============%s 0x%08zx ==============\n", name, physical_base);
	mdss_dump_reg(mdss_phy_base, 0x100);

	snprintf(name, sizeof(name), "MDP REG");
	physical_base = mdss_physical_address + mdata->physical_base_offset;
	pr_err("=============%s 0x%08zx ==============\n", name, physical_base);
	mdss_dump_reg(mdata->mdp_base, 0x500);

	for(loop = 0; loop < mdata->nctl ; loop++) {
		snprintf(name, sizeof(name), "CTRL%d", loop);
		physical_base = mdss_physical_address + mdata->ctl_off[loop].base - mdss_phy_base;
		pr_err("=============%s 0x%08zx ==============\n", name, physical_base);

		mdss_dump_reg(mdata->ctl_off[loop].base, 0x200);
	}

	for(loop = 0; loop < mdata->nvig_pipes ; loop++) {
		snprintf(name, sizeof(name), "VG%d", loop);
		physical_base = mdss_physical_address + mdata->vig_pipes[loop].base - mdss_phy_base;
		pr_err("=============%s 0x%08zx ==============\n", name, physical_base);

		mdss_dump_reg(mdata->vig_pipes[loop].base, 0x100);
	}

	for(loop = 0; loop < mdata->nrgb_pipes ; loop++) {
		snprintf(name, sizeof(name), "RGB%d", loop);
		physical_base = mdss_physical_address + mdata->rgb_pipes[loop].base - mdss_phy_base;
		pr_err("=============%s 0x%08zx ==============\n", name, physical_base);

		mdss_dump_reg(mdata->rgb_pipes[loop].base, 0x100);
	}

	for(loop = 0; loop < mdata->ndma_pipes ; loop++) {
		snprintf(name, sizeof(name), "DMA%d", loop);
		physical_base = mdss_physical_address + mdata->dma_pipes[loop].base - mdss_phy_base;
		pr_err("=============%s 0x%08zx ==============\n", name, physical_base);

		mdss_dump_reg(mdata->dma_pipes[loop].base, 0x100);
	}

	for(loop = 0; loop < mdata->nmixers_intf ; loop++) {
		snprintf(name, sizeof(name), "MIXER_INTF_%d", loop);
		physical_base = mdss_physical_address + mdata->mixer_intf[loop].base - mdss_phy_base;
		pr_err("=============%s 0x%08zx ==============\n", name, physical_base);

		mdss_dump_reg(mdata->mixer_intf[loop].base, 0x100);
	}

	for(loop = 0; loop < mdata->nmixers_wb ; loop++) {
		snprintf(name, sizeof(name), "MIXER_WB_%d", loop);
		physical_base = mdss_physical_address + mdata->mixer_wb[loop].base - mdss_phy_base;
		pr_err("=============%s 0x%08zx ==============\n", name, physical_base);

		mdss_dump_reg(mdata->mixer_wb[loop].base, 0x100);
	}

	for(loop = 0; loop < mdata->nmixers_intf ; loop++) {
		snprintf(name, sizeof(name), "PING_PONG%d", loop);
		physical_base = mdss_physical_address + mdata->mixer_intf[loop].pingpong_base - mdss_phy_base;
		pr_err("=============%s 0x%08zx ==============\n", name, physical_base);

		mdss_dump_reg(mdata->mixer_intf[loop].pingpong_base, 0x40);
	}
}

void mdss_samsung_dsi_dump_regs(int dsi_num)
{
	struct mdss_dsi_ctrl_pdata **dsi_ctrl = mdss_dsi_get_ctrl();
	size_t physical_base;
	char name[32];

	char *dsi_ctrl_base = (char *)dsi_ctrl[dsi_num]->ctrl_io.base;
	size_t dsi_ctrl_size = (size_t)dsi_ctrl[dsi_num]->ctrl_io.len;

	char *dsi_phy_base = (char *)dsi_ctrl[dsi_num]->phy_io.base;
	size_t dsi_phy_size = (size_t)dsi_ctrl[dsi_num]->phy_io.len;

	snprintf(name, sizeof(name), "DSI%d CTL", dsi_num);

	//physical_base = dsi_ctrl[dsi_num]->ctrl_io.physical_base;
	physical_base = 0;

	pr_err("=============%s 0x%08zx ==============\n", name, physical_base);
	mdss_dump_reg(dsi_ctrl_base, dsi_ctrl_size);

	snprintf(name, sizeof(name), "DSI%d PHY", dsi_num);

	//physical_base = dsi_ctrl[dsi_num]->phy_io.physical_base;

	pr_err("=============%s 0x%08zx ==============\n", name, physical_base);
	mdss_dump_reg(dsi_phy_base, dsi_phy_size);
}

void mdss_samsung_dsi_te_check(void)
{
	struct mdss_dsi_ctrl_pdata **dsi_ctrl = mdss_dsi_get_ctrl();
	struct samsung_display_driver_data *vdd = samsung_get_vdd();
	int rc, te_count = 0;
	int te_max = 20000; /*sampling 200ms */
	char rddpm_reg = 0;

	if (dsi_ctrl[DISPLAY_1]->panel_mode == DSI_VIDEO_MODE)
		return;

	if (gpio_is_valid(dsi_ctrl[DISPLAY_1]->disp_te_gpio)) {
		pr_err(" ============ start waiting for TE ============\n");

		for (te_count = 0;  te_count < te_max; te_count++) {
			rc = gpio_get_value(dsi_ctrl[DISPLAY_1]->disp_te_gpio);
			if (rc == 1) {
				pr_err("%s: gpio_get_value(disp_te_gpio) = %d ",
									__func__, rc);
				pr_err("te_count = %d\n", te_count);
				break;
			}
			/* usleep suspends the calling thread whereas udelay is a
			 * busy wait. Here the value of te_gpio is checked in a loop of
			 * max count = 250. If this loop has to iterate multiple
			 * times before the te_gpio is 1, the calling thread will end
			 * up in suspend/wakeup sequence multiple times if usleep is
			 * used, which is an overhead. So use udelay instead of usleep.
			 */
			udelay(10);
		}

		if(te_count == te_max)
		{
			pr_err("LDI doesn't generate TE");
			if (!IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_2].ldi_debug0_rx_cmds[vdd->panel_revision].cmds))
				mdss_samsung_read_nv_mem(dsi_ctrl[DISPLAY_2], &vdd->dtsi_data[DISPLAY_2].ldi_debug0_rx_cmds[vdd->panel_revision], &rddpm_reg, 0);
			if (!IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_1].ldi_debug0_rx_cmds[vdd->panel_revision].cmds))
				mdss_samsung_read_nv_mem(dsi_ctrl[DISPLAY_1], &vdd->dtsi_data[DISPLAY_1].ldi_debug0_rx_cmds[vdd->panel_revision], &rddpm_reg, 0);
		}
		else
			pr_err("LDI generate TE");

		pr_err(" ============ finish waiting for TE ============\n");
	} else
		pr_err("disp_te_gpio is not valid\n");
}
void mdss_mdp_underrun_dump_info(void)
{
	struct mdss_mdp_pipe *pipe;
	struct mdss_data_type *mdss_res = mdss_mdp_get_mdata();
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(vdd_data.mfd_dsi[0]);
	int pcount = mdp5_data->mdata->nrgb_pipes+ mdp5_data->mdata->nvig_pipes+mdp5_data->mdata->ndma_pipes;

	pr_err(" ============ %s start ===========\n",__func__);
	mutex_lock(&mdp5_data->list_lock);
	list_for_each_entry(pipe, &mdp5_data->pipes_used, list) {
		if (pipe)
			pr_err(" [%4d, %4d, %4d, %4d] -> [%4d, %4d, %4d, %4d]"
				"|flags = %8d|src_format = %2d|bpp = %2d|ndx = %3d|\n",
				pipe->src.x, pipe->src.y, pipe->src.w, pipe->src.h,
				pipe->dst.x, pipe->dst.y, pipe->dst.w, pipe->dst.h,
				pipe->flags, pipe->src_fmt->format, pipe->src_fmt->bpp,
				pipe->ndx);
		pr_err("pipe addr : %pK\n", pipe);
		pcount--;
		if (!pcount) break;
	}
	mutex_unlock(&mdp5_data->list_lock);

	pr_err("mdp_clk = %ld, bus_ab = %llu, bus_ib = %llu\n", mdss_mdp_get_clk_rate(MDSS_CLK_MDP_SRC),
			mdss_res->bus_scale_table->usecase[mdss_res->curr_bw_uc_idx].vectors[0].ab,
			mdss_res->bus_scale_table->usecase[mdss_res->curr_bw_uc_idx].vectors[0].ib);
	pr_err(" ============ %s end =========== \n", __func__);
}

struct samsung_display_driver_data *samsung_get_vdd(void)
{
	return &vdd_data;
}
