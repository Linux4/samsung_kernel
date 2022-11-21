/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * DDI mAFPC operation
 * Author: Samsung Display Driver Team <cj1225.jang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "ss_dsi_panel_common.h"
#include "ss_dsi_mafpc_FAB.h"

/*
 * make dsi_panel_cmds using image data
 */
int ss_mafpc_make_img_cmds_FAB(struct samsung_display_driver_data *vdd, char *data,
									u32 data_size, int cmd_type)
{
	struct dsi_cmd_desc *tcmds;
	struct dsi_panel_cmd_set *pcmds;

	int i, j;
	int data_idx = 0;
	int ret = 0;

	u32 p_size = MAFPC_CMD_ALIGN;
	u32 paylod_size = 0;
	u32 cmd_size = 0;

	if (!data) {
		LCD_ERR(vdd, "data is null..\n");
		return -EINVAL;
	}

	if (!data_size) {
		LCD_ERR(vdd, "data size is zero..\n");
		return -EINVAL;
	}

	/* ss_txbuf size */
	while (p_size < MAFPC_MAX_PAYLOAD_SIZE) {
		 if (data_size % p_size == 0) {
			paylod_size = p_size;
		 }
		 p_size += MAFPC_CMD_ALIGN;
	}
	/* cmd size */
	if(!paylod_size) {
		LCD_ERR(vdd, "invalid data size..\n");
		return -EINVAL;
	}
	cmd_size = data_size / paylod_size;

	LCD_INFO(vdd, "Command[%d] Total data size [%d]\n", cmd_type, data_size);
	LCD_INFO(vdd, "cmd size [%d] ss_txbuf size [%d]\n", cmd_size, paylod_size);

	pcmds = ss_get_cmds(vdd, cmd_type);
	if (IS_ERR_OR_NULL(pcmds->cmds)) {
		LCD_ERR(vdd, "pcmds->cmds is null!!\n");
		pcmds->cmds = kzalloc(cmd_size * sizeof(struct dsi_cmd_desc), GFP_KERNEL);
		if (IS_ERR_OR_NULL(pcmds->cmds)) {
			LCD_ERR(vdd, "fail to kzalloc for mafpc cmds \n");
			return -ENOMEM;
		}
	}

	pcmds->state = DSI_CMD_SET_STATE_HS;
	pcmds->count = cmd_size;

	tcmds = pcmds->cmds;
	if (tcmds == NULL) {
		LCD_ERR(vdd, "tcmds is NULL \n");
		return -ENOMEM;
	}

	for (i = 0; i < pcmds->count; i++) {
		tcmds[i].msg.type = MIPI_DSI_GENERIC_LONG_WRITE;
		tcmds[i].last_command = 1;

		/* fill image data */
		if (tcmds[i].ss_txbuf == NULL) {
			/* +1 means HEADER TYPE 0x4C or 0x5C */
			tcmds[i].ss_txbuf = kzalloc(paylod_size + 1, GFP_KERNEL);
			if (tcmds[i].ss_txbuf == NULL) {
				LCD_ERR(vdd, "fail to kzalloc for mafpc cmds ss_txbuf \n");
				return -ENOMEM;
			}
		}

		tcmds[i].ss_txbuf[0] = (i == 0) ? 0x4C : 0x5C;

		for (j = 1; (j <= paylod_size) && (data_idx < data_size); j++)
			tcmds[i].ss_txbuf[j] = data[data_idx++];

		tcmds[i].msg.tx_len = j;

		ss_alloc_ss_txbuf(&tcmds[i], tcmds[i].ss_txbuf);

		LCD_DEBUG(vdd, "dlen (%d), data_idx (%d)\n", j, data_idx);
	}

	return ret;
}

int ss_mafpc_make_img_mass_cmds_FAB(struct samsung_display_driver_data *vdd, char* data,
									u32 data_size, int cmd_type)
{
	struct dsi_cmd_desc *tcmds;
	struct dsi_panel_cmd_set *pcmds;
	int ret = 0;

	u32 data_idx = 0;
	u32 payload_len = 0;
	u32 cmd_cnt = 0;
	u32 p_len = 0;
	u32 c_cnt = 0;

	if (!data) {
		LCD_ERR(vdd, "data is null..\n");
		return -EINVAL;
	}

	if (!data_size) {
		LCD_ERR(vdd, "data size is zero..\n");
		return -EINVAL;
	}

	payload_len = data_size + (data_size + MAFPC_MASS_CMD_ALIGN - 1)/MAFPC_MASS_CMD_ALIGN;
	cmd_cnt = (payload_len + MAFPC_MAX_PAYLOAD_SIZE_MASS - 1) / MAFPC_MAX_PAYLOAD_SIZE_MASS;
	LCD_INFO(vdd, "Command [%s], Total data size [%d], total cmd len [%d], cmd count [%d]\n",
			ss_get_cmd_name(cmd_type), data_size, payload_len, cmd_cnt);

	pcmds = ss_get_cmds(vdd, cmd_type);
	if (IS_ERR_OR_NULL(pcmds->cmds)) {
		LCD_INFO(vdd, "alloc mem for mafpc cmd\n");
		pcmds->cmds = kzalloc(cmd_cnt * sizeof(struct dsi_cmd_desc), GFP_KERNEL);
		if (IS_ERR_OR_NULL(pcmds->cmds)) {
			LCD_ERR(vdd, "fail to kzalloc for mafpc cmds \n");
		}
	}

	pcmds->state = DSI_CMD_SET_STATE_HS;
	pcmds->count = cmd_cnt;

	tcmds = pcmds->cmds;
	if (tcmds == NULL) {
		LCD_ERR(vdd, "tcmds is NULL \n");
		return -ENOMEM;
	}
	/* fill image data */

	SDE_ATRACE_BEGIN("mafpc_mass_cmd_generation");
	mutex_lock(&vdd->mafpc.vdd_mafpc_lock);
	for (c_cnt = 0; c_cnt < pcmds->count ; c_cnt++) {
		tcmds[c_cnt].msg.type = MIPI_DSI_GENERIC_LONG_WRITE;
		tcmds[c_cnt].last_command = 1;

		/* Memory Alloc for each cmds */
		if (tcmds[c_cnt].ss_txbuf == NULL) {
			/* HEADER TYPE 0x4C or 0x5C */
			tcmds[c_cnt].ss_txbuf = vzalloc(MAFPC_MAX_PAYLOAD_SIZE_MASS);
			if (tcmds[c_cnt].ss_txbuf == NULL) {
				LCD_ERR(vdd, "fail to vzalloc for mafpc cmds ss_txbuf \n");
				mutex_unlock(&vdd->mafpc.vdd_mafpc_lock);
				return -ENOMEM;
			}
		}

		/* Copy from Data Buffer to each cmd Buffer */
		for (p_len = 0; p_len < MAFPC_MAX_PAYLOAD_SIZE_MASS && data_idx < data_size ; p_len++) {
			if (p_len % MAFPC_MASS_CMD_ALIGN)
				tcmds[c_cnt].ss_txbuf[p_len] = data[data_idx++];
			else
				tcmds[c_cnt].ss_txbuf[p_len] = (p_len == 0 && c_cnt == 0) ? 0x4C : 0x5C;

		}
		tcmds[c_cnt].msg.tx_len = p_len;

		ss_alloc_ss_txbuf(&tcmds[c_cnt], tcmds[c_cnt].ss_txbuf);
	}
	mutex_unlock(&vdd->mafpc.vdd_mafpc_lock);
	SDE_ATRACE_END("mafpc_mass_cmd_generation");

	LCD_INFO(vdd, "Total Cmd Count(%d), Last Cmd Payload Len(%d)\n", c_cnt, tcmds[c_cnt-1].msg.tx_len);

	return ret;
}

#define BUF_LEN 200
void ss_mafpc_update_enable_cmds_FAB(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *pcmds;

	u32 cmd_size = vdd->mafpc.enable_cmd_size;
	char *cmd_buf = vdd->mafpc.enable_cmd_buf;
	char *cmd_pload = NULL;
	char show_buf[BUF_LEN] = { 0, };
	int loop, pos;

	if (!cmd_buf) {
		LCD_ERR(vdd, "Enable cmd buffer is null..\n");
		return;
	}

	mutex_lock(&vdd->mafpc.vdd_mafpc_lock);

	pcmds = ss_get_cmds(vdd, TX_MAFPC_ON);
	cmd_pload = &pcmds->cmds[1].ss_txbuf[6];

	memcpy(cmd_pload, cmd_buf, cmd_size);
	loop = pos = 0;
	for (loop = 0; (loop < cmd_size) && (pos < (BUF_LEN - 5)); loop++) {
		pos += snprintf(show_buf + pos, sizeof(show_buf) - pos, "%02x ", cmd_pload[loop]);
	}

	mutex_unlock(&vdd->mafpc.vdd_mafpc_lock);

	LCD_INFO(vdd, "Enable Cmd = %s\n", show_buf);

	return;
}

struct dsi_panel_cmd_set *ss_mafpc_brightness_scale_FAB(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *scale_cmds = ss_get_cmds(vdd, TX_MAFPC_BRIGHTNESS_SCALE);
	int bl_level;
	int idx;

	if (!vdd->mafpc.is_support) {
		LCD_ERR(vdd, "mafpc is not supported..(%d) \n", vdd->mafpc.is_support);
		return NULL;
	}

	if (SS_IS_CMDS_NULL(scale_cmds)) {
		LCD_DEBUG(vdd, "no Brightness Scale cmd\n");
		return NULL;
	}

	if (!vdd->mafpc.en) {
		LCD_ERR(vdd, "mAFPC is not enabled\n");
		return NULL;
	}

	if (!vdd->mafpc.is_br_table_updated) {
		LCD_ERR(vdd, "Brightness Table for mAFPC is not updated yet\n");
		return NULL;
	}

	/* 0 ~ 73 for Noraml Brightness, 74 for HBM */

	/* HBM */
	bl_level = vdd->br_info.common_br.bl_level;
	if (bl_level >= MAX_MAFPC_BL_SCALE)
		bl_level = MAX_MAFPC_BL_SCALE - 1;

	idx = brightness_scale_idx[bl_level];

	scale_cmds->cmds[2].ss_txbuf[1] = brightness_scale_table[idx][0];
	scale_cmds->cmds[2].ss_txbuf[2] = brightness_scale_table[idx][1];
	scale_cmds->cmds[2].ss_txbuf[3] = brightness_scale_table[idx][2];

	LCD_INFO(vdd, "Brightness idx(%d), candela(%d), cmd(0x%x 0x%x 0x%x)\n",
			idx, vdd->br_info.common_br.cd_level,
			scale_cmds->cmds[2].ss_txbuf[1],
			scale_cmds->cmds[2].ss_txbuf[2],
			scale_cmds->cmds[2].ss_txbuf[3]);

	return scale_cmds;
}

#define WAIT_FRAME (1)

static int ss_mafpc_img_write(struct samsung_display_driver_data *vdd, bool is_instant)
{
	struct dsi_panel_cmd_set *pcmds;
	int fps, wait_time;
	int wait_cnt = 1000; /* 1000 * 0.5ms = 500ms */
	int ret = 0;

	if (!vdd->mafpc.is_support) {
		LCD_ERR(vdd, "mafpc is not supported..(%d) \n", vdd->mafpc.is_support);
		return -EACCES;
	}

	fps = vdd->vrr.cur_refresh_rate;

	LCD_INFO(vdd, "++(%d)\n", is_instant);

	mutex_lock(&vdd->self_disp.vdd_self_display_ioctl_lock);

	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.enable = 1;
	while (!list_empty(&vdd->cmd_lock.wait_list) && --wait_cnt)
		usleep_range(500, 500);
	vdd->exclusive_tx.permit_frame_update = 0;

	ss_set_exclusive_tx_packet(vdd, TX_LEVEL1_KEY_ENABLE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL2_KEY_ENABLE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_MAFPC_SET_PRE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_MAFPC_IMAGE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_MAFPC_CRC_CHECK_IMAGE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_MAFPC_SET_POST, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL1_KEY_DISABLE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL2_KEY_DISABLE, 1);

	mutex_lock(&vdd->mafpc.vdd_mafpc_lock);
	ss_send_cmd(vdd, TX_LEVEL1_KEY_ENABLE);
	ss_send_cmd(vdd, TX_LEVEL2_KEY_ENABLE);
	ss_send_cmd(vdd, TX_MAFPC_SET_PRE);

	/* 2 Frame Delay (Worst Case of 48FPS) */
	if (vdd->is_factory_mode || is_instant || vdd->mafpc.force_delay) {
		/*
		 * +1 means padding.
		 * WAIT_FRAME means wait 2 frame for exact frame update locking.
		 * TE-> frame tx -> frame flush -> lock exclusive_tx -> max wait 16ms -> TE -> frame tx.
		 */
		wait_time = ((1000 / fps) + 1) * WAIT_FRAME;
		LCD_INFO(vdd, "fps : %d, wait %d ms\n", fps, wait_time);
		usleep_range(wait_time*1000, wait_time*1000);
		vdd->mafpc.force_delay = false;
	}

	if (vdd->is_factory_mode)
		ss_send_cmd(vdd, TX_MAFPC_CRC_CHECK_IMAGE);
	else
		ss_send_cmd(vdd, TX_MAFPC_IMAGE);

	usleep_range(100, 110); /* needs 100us delay between 0x5C& 0x75 */

	pcmds = ss_get_cmds(vdd, TX_MAFPC_SET_POST);
	if (vdd->self_disp.sd_info.en | vdd->self_disp.sa_info.en) {
		LCD_INFO(vdd, "AOD Analog(Digital) Clock is enabled!\n");
		pcmds->cmds[0].ss_txbuf[1] = 0x00;
	} else {
		LCD_INFO(vdd, "AOD Analog(Digital) Clock is disabled!\n");
		pcmds->cmds[0].ss_txbuf[1] = 0x01;
	}

	ss_send_cmd(vdd, TX_MAFPC_SET_POST);

	/* 1 frame Delay */
	wait_time = (1000 / fps) + 1; /* add dummy 1ms */
	usleep_range(wait_time * 1000, wait_time * 1000);

	ss_send_cmd(vdd, TX_LEVEL1_KEY_DISABLE);
	ss_send_cmd(vdd, TX_LEVEL2_KEY_DISABLE);
	mutex_unlock(&vdd->mafpc.vdd_mafpc_lock);

	ss_set_exclusive_tx_packet(vdd, TX_LEVEL1_KEY_ENABLE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL2_KEY_ENABLE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_MAFPC_SET_PRE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_MAFPC_IMAGE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_MAFPC_CRC_CHECK_IMAGE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_MAFPC_SET_POST, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL1_KEY_DISABLE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL2_KEY_DISABLE, 0);

	vdd->exclusive_tx.enable = 0;
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);

	mutex_unlock(&vdd->self_disp.vdd_self_display_ioctl_lock);

	LCD_INFO(vdd, "--(%d)\n", is_instant);

	return ret;
}

static int ss_mafpc_enable(struct samsung_display_driver_data *vdd, int enable)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	if (!vdd->mafpc.is_support) {
		LCD_ERR(vdd, "mafpc is not supported..(%d) \n", vdd->mafpc.is_support);
		return -EACCES;
	}

	mutex_lock(&vdd->mafpc.vdd_mafpc_lock);

	if (enable) {
		if (vdd->is_factory_mode)
			ss_send_cmd(vdd, TX_MAFPC_ON_FACTORY);
		else
			ss_send_cmd(vdd, TX_MAFPC_ON);
		/* To update mAFPC brightness scale factor */
		ss_brightness_dcs(vdd, USE_CURRENT_BL_LEVEL, BACKLIGHT_NORMAL);
	} else
		ss_send_cmd(vdd, TX_MAFPC_OFF);

	mutex_unlock(&vdd->mafpc.vdd_mafpc_lock);

	LCD_INFO(vdd, "%s\n", enable ? "Enable" : "Disable");

	return 0;
}

static int ss_mafpc_crc_check(struct samsung_display_driver_data *vdd)
{
	int i, ret = 0;
	int wait_cnt = 1000; /* 1000 * 0.5ms = 500ms */

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	if (!vdd->mafpc.is_support) {
		LCD_ERR(vdd, "mafpc is not supported..(%d) \n", vdd->mafpc.is_support);
		return -ENODEV;
	}


	if (!vdd->mafpc.crc_size) {
		LCD_ERR(vdd, "mAFPC crc size is zero..\n\n");
		return -EINVAL;
	}

	if (!vdd->mafpc.crc_read_data) {
		vdd->mafpc.crc_read_data = kzalloc(vdd->mafpc.crc_size, GFP_KERNEL);
		if (!vdd->mafpc.crc_read_data) {
			LCD_ERR(vdd, "fail to alloc for mAFPC crc_read_data \n");
			return -ENOMEM;
		}
	}

	LCD_INFO(vdd, "++ \n");
	mutex_lock(&vdd->mafpc.vdd_mafpc_crc_check_lock);

	/* prevent sw reset to trigger esd recovery */
	LCD_INFO(vdd, "disable esd interrupt\n");
	if (vdd->esd_recovery.esd_irq_enable)
		vdd->esd_recovery.esd_irq_enable(false, true, (void *)vdd);

	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.enable = 1;
	while (!list_empty(&vdd->cmd_lock.wait_list) && --wait_cnt)
		usleep_range(500, 500);

	ss_set_exclusive_tx_packet(vdd, TX_MAFPC_CRC_CHECK_PRE1, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_ENABLE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL1_KEY_ENABLE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL2_KEY_ENABLE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_DISABLE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL1_KEY_DISABLE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL2_KEY_DISABLE, 1);
	ss_set_exclusive_tx_packet(vdd, RX_MAFPC_CRC_CHECK, 1);
	ss_set_exclusive_tx_packet(vdd, TX_MAFPC_CRC_CHECK_POST, 1);

	ss_send_cmd(vdd, TX_MAFPC_CRC_CHECK_PRE1);
	ss_panel_data_read(vdd, RX_MAFPC_CRC_CHECK, vdd->mafpc.crc_read_data, LEVEL0_KEY | LEVEL1_KEY | LEVEL2_KEY);
	ss_send_cmd(vdd, TX_MAFPC_CRC_CHECK_POST);

	ss_set_exclusive_tx_packet(vdd, TX_MAFPC_CRC_CHECK_PRE1, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_ENABLE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL1_KEY_ENABLE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL2_KEY_ENABLE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_DISABLE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL1_KEY_DISABLE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL2_KEY_DISABLE, 0);
	ss_set_exclusive_tx_packet(vdd, RX_MAFPC_CRC_CHECK, 0);
	ss_set_exclusive_tx_packet(vdd, TX_MAFPC_CRC_CHECK_POST, 0);

	vdd->exclusive_tx.enable = 0;
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);

	for (i = 0; i < vdd->mafpc.crc_size; i++) {
		if (vdd->mafpc.crc_read_data[i] != vdd->mafpc.crc_pass_data[i]) {
			LCD_ERR(vdd, "mAFPC CRC check fail !!\n");
			ret = -EFAULT;
			break;
		}
	}

	/* enable esd interrupt */
	LCD_INFO(vdd, "enable esd interrupt\n");
	if (vdd->esd_recovery.esd_irq_enable)
		vdd->esd_recovery.esd_irq_enable(true, true, (void *)vdd);

	mutex_unlock(&vdd->mafpc.vdd_mafpc_crc_check_lock);
	LCD_INFO(vdd, "-- \n");

	return ret;
}


static int ss_mafpc_debug(struct samsung_display_driver_data *vdd)
{
	return 0;
}


/*
 * ss_mafpc_ioctl() : get ioctl from mdnie framework.
 */
static long ss_mafpc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *c = file->private_data;
	struct dsi_display *display = dev_get_drvdata(c->parent);
	struct dsi_panel *panel = display->panel;
	struct samsung_display_driver_data *vdd = panel->panel_private;
	int ret = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	if ((_IOC_TYPE(cmd) != MAFPC_IOCTL_MAGIC) ||
				(_IOC_NR(cmd) >= IOCTL_MAFPC_MAX)) {
		LCD_ERR(vdd, "TYPE(%u) NR(%u) is wrong..\n",
			_IOC_TYPE(cmd), _IOC_NR(cmd));
		return -EINVAL;
	}

	LCD_INFO(vdd, "cmd = %s\n", cmd == IOCTL_MAFPC_ON ? "IOCTL_MAFPC_ON" :
				cmd == IOCTL_MAFPC_OFF ? "IOCTL_MAFPC_OFF" :
				cmd == IOCTL_MAFPC_ON_INSTANT ? "IOCTL_MAFPC_ON_INSTANT" :
				cmd == IOCTL_MAFPC_OFF_INSTANT ? "IOCTL_MAFPC_OFF_INSTANT" : "IOCTL_ERR");

	switch (cmd) {
	case IOCTL_MAFPC_ON:
		vdd->mafpc.en = true;
		break;
	case IOCTL_MAFPC_ON_INSTANT:
		vdd->mafpc.en = true;
		if (!ss_is_ready_to_send_cmd(vdd)) {
			LCD_INFO(vdd, "Panel is not ready(%d), will apply next display on\n",
					vdd->panel_state);
			return -ENODEV;
		}

		ss_mafpc_img_write(vdd, true);
		ss_mafpc_enable(vdd, true);
		break;
	case IOCTL_MAFPC_OFF:
		vdd->mafpc.en = false;
		break;
	case IOCTL_MAFPC_OFF_INSTANT:
		if (!ss_is_ready_to_send_cmd(vdd)) {
			LCD_ERR(vdd, "Panel is not ready. Panel State(%d)\n", vdd->panel_state);
			return -ENODEV;
		}
		vdd->mafpc.en = false;
		ss_mafpc_enable(vdd, false);
		break;
	default:
		LCD_ERR(vdd, "invalid cmd : %u \n", cmd);
		break;
	}

	return ret;
}

/*
 * ss_mafpc_write_from_user() : get mfapc image data from mdnie framework.
 *                           	prepare for dsi_panel_cmds.
 */
static ssize_t ss_mafpc_write_from_user(struct file *file, const char __user *user_buf,
			 size_t total_count, loff_t *ppos)
{
	struct miscdevice *c = file->private_data;
	struct dsi_display *display = dev_get_drvdata(c->parent);
	struct dsi_panel *panel = display->panel;
	struct samsung_display_driver_data *vdd = panel->panel_private;

	int ret = 0;

	u32 enable_cmd_size = vdd->mafpc.enable_cmd_size;
	char *enable_cmd_buf = vdd->mafpc.enable_cmd_buf;
	u32 img_size = vdd->mafpc.img_size;
	char *img_buf = vdd->mafpc.img_buf;
	u32 br_table_size = vdd->mafpc.brightness_scale_table_size;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "no vdd");
		return -ENODEV;
	}

	if (unlikely(!enable_cmd_buf)) {
		LCD_ERR(vdd, "No mafpc Enable cmd Buffer\n");
		return -ENODEV;
	}

	if (unlikely(!img_buf)) {
		LCD_ERR(vdd, "No mafpc Image Buffer\n");
		return -ENODEV;
	}

	if (unlikely(!user_buf)) {
		LCD_ERR(vdd, "invalid user buffer\n");
		return -EINVAL;
	}

	if (total_count != (enable_cmd_size + 1 + img_size + br_table_size)) {
		LCD_ERR(vdd, "Invalid size %zu, should be %u\n",
				total_count, (enable_cmd_size + 1 + img_size + br_table_size));
		return -EINVAL;
	}

	LCD_INFO(vdd, "Total_Count(%zu), cmd_size(%u), img_size(%u), br_table_size(%u)\n",
			total_count, enable_cmd_size + 1, img_size, br_table_size);

	/*
	 * Get 40bytes Enable Command to match with mafpc image data
	   (1Byte(Padding) + 39Byte(Payload))
	 */
	ret = copy_from_user(enable_cmd_buf, user_buf + 1, enable_cmd_size);
	if (unlikely(ret < 0)) {
		LCD_ERR(vdd, "failed to copy_from_user (Enable Command)\n");
		return -EINVAL;
	}

	/* Get 865,080 Bytes for mAFPC Image Data from user space (mDNIE Service) */
	ret = copy_from_user(img_buf, user_buf + enable_cmd_size + 1, img_size);
	if (unlikely(ret < 0)) {
		LCD_ERR(vdd, "failed to copy_from_user (Image Data)\n");
		return -EINVAL;
	}

	/* Get 225(75 x 3)Bytes for brightness scale cmd table from user space (mDNIE Service) */
	ret = copy_from_user(brightness_scale_table, user_buf + enable_cmd_size + 1 + img_size, br_table_size);
	if (unlikely(ret < 0)) {
		LCD_ERR(vdd, "failed to copy_from_user (Brightness Scale Table)\n");
		return -EINVAL;
	}
	vdd->mafpc.is_br_table_updated = true;

	ss_mafpc_update_enable_cmds_FAB(vdd);
	ss_mafpc_make_img_mass_cmds_FAB(vdd, vdd->mafpc.img_buf, vdd->mafpc.img_size, TX_MAFPC_IMAGE);

	return total_count;
}

static int ss_mafpc_open(struct inode *inode, struct file *file)
{
	struct miscdevice *c = file->private_data;
	struct dsi_display *display = dev_get_drvdata(c->parent);
	struct dsi_panel *panel = display->panel;
	struct samsung_display_driver_data *vdd = panel->panel_private;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	vdd->mafpc.file_open = true;

	LCD_DEBUG(vdd, "[OPEN]\n");

	return 0;
}

static int ss_mafpc_release(struct inode *inode, struct file *file)
{
	struct miscdevice *c = file->private_data;
	struct dsi_display *display = dev_get_drvdata(c->parent);
	struct dsi_panel *panel = display->panel;
	struct samsung_display_driver_data *vdd = panel->panel_private;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	vdd->mafpc.file_open = false;

	LCD_DEBUG(vdd, "[RELEASE]\n");

	return 0;
}

static const struct file_operations mafpc_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = ss_mafpc_ioctl,
	.open = ss_mafpc_open,
	.release = ss_mafpc_release,
	.write = ss_mafpc_write_from_user,
};

#define DEV_NAME_SIZE 24
int ss_mafpc_init_FAB(struct samsung_display_driver_data *vdd)
{
	int ret = 0;
	static char devname[DEV_NAME_SIZE] = {'\0', };

	struct dsi_panel *panel = NULL;
	struct mipi_dsi_host *host = NULL;
	struct dsi_display *display = NULL;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	if (!vdd->mafpc.is_support) {
		LCD_ERR(vdd, "mAFPC is not supported\n");
		return -EINVAL;
	}

	panel = (struct dsi_panel *)vdd->msm_private;
	host = panel->mipi_device.host;
	display = container_of(host, struct dsi_display, host);

	mutex_init(&vdd->mafpc.vdd_mafpc_lock);
	mutex_init(&vdd->mafpc.vdd_mafpc_crc_check_lock);

	if (vdd->ndx == PRIMARY_DISPLAY_NDX)
		snprintf(devname, DEV_NAME_SIZE, "mafpc");
	else
		snprintf(devname, DEV_NAME_SIZE, "mafpc%d", vdd->ndx);

	vdd->mafpc.dev.minor = MISC_DYNAMIC_MINOR;
	vdd->mafpc.dev.name = devname;
	vdd->mafpc.dev.fops = &mafpc_fops;
	vdd->mafpc.dev.parent = &display->pdev->dev;;

	vdd->mafpc.enable = ss_mafpc_enable;
	vdd->mafpc.crc_check = ss_mafpc_crc_check;
	vdd->mafpc.make_img_mass_cmds = ss_mafpc_make_img_mass_cmds_FAB;
	vdd->mafpc.make_img_cmds = ss_mafpc_make_img_cmds_FAB;
	vdd->mafpc.img_write = ss_mafpc_img_write;
	vdd->mafpc.debug = ss_mafpc_debug;
	vdd->panel_func.br_func[BR_FUNC_MAFPC_SCALE] = ss_mafpc_brightness_scale_FAB;

	vdd->mafpc.brightness_scale_table_size = sizeof(brightness_scale_table);

	vdd->mafpc.enable_cmd_size = MAFPC_ENABLE_COMMAND_LEN_FAB;
	vdd->mafpc.enable_cmd_buf = kzalloc(MAFPC_ENABLE_COMMAND_LEN_FAB, GFP_KERNEL);
	if (IS_ERR_OR_NULL(vdd->mafpc.enable_cmd_buf))
		LCD_ERR(vdd, "Failed to alloc mafpc enable cmd buffer\n");

	ret = ss_wrapper_misc_register(vdd, &vdd->mafpc.dev);
	if (ret) {
		LCD_ERR(vdd, "failed to register driver : %d\n", ret);
		vdd->mafpc.is_support = false;
		return -ENODEV;
	}

	LCD_INFO(vdd, "Success to register mafpc device..(%d)\n", ret);

	return ret;
}

MODULE_DESCRIPTION("mAFPC driver");
MODULE_LICENSE("GPL");

