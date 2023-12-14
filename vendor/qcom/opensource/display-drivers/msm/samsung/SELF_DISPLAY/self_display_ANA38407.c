/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * DDI operation : self clock, self mask, self icon.. etc.
 * Author: QC LCD driver <cj1225.jang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "ss_dsi_panel_common.h"
#include "self_display_ANA38407.h"

void make_mass_self_display_img_cmds_ANA38407(struct samsung_display_driver_data *vdd,
		int cmd, u32 op)
{
	struct dsi_cmd_desc *tcmds;
	struct dsi_panel_cmd_set *pcmds;

	u32 data_size = vdd->self_disp.operation[op].img_size;
	char *data = vdd->self_disp.operation[op].img_buf;
	u32 data_idx = 0;
	u32 payload_len = 0;
	u32 cmd_cnt = 0;
	u32 p_len = 0;
	u32 c_cnt = 0;
	int i;
	u32 check_sum_0 = 0;
	u32 check_sum_1 = 0;
	u32 check_sum_2 = 0;
	u32 check_sum_3 = 0;

	if (!data) {
		LCD_ERR(vdd, "data is null..\n");
		return;
	}

	if (!data_size) {
		LCD_ERR(vdd, "data size is zero..\n");
		return;
	}

	payload_len = data_size + (data_size + MASS_CMD_ALIGN - 2) / (MASS_CMD_ALIGN - 1);
	cmd_cnt = (payload_len + payload_len - 1) / payload_len;
	LCD_INFO(vdd, "[%s] total data size [%d], total cmd len[%d], cmd count [%d]\n",
			ss_get_cmd_name(cmd), data_size, payload_len, cmd_cnt);

	pcmds = ss_get_cmds(vdd, cmd);
	if (IS_ERR_OR_NULL(pcmds->cmds)) {
		LCD_INFO(vdd, "alloc mem for self_display cmd\n");
		pcmds->cmds = kzalloc(cmd_cnt * sizeof(struct dsi_cmd_desc), GFP_KERNEL);
		if (IS_ERR_OR_NULL(pcmds->cmds)) {
			LCD_ERR(vdd, "fail to kzalloc for self_mask cmds\n");
			return;
		}
	}

	pcmds->state = DSI_CMD_SET_STATE_HS;
	pcmds->count = cmd_cnt;

	tcmds = pcmds->cmds;
	if (tcmds == NULL) {
		LCD_ERR(vdd, "tcmds is NULL\n");
		return;
	}
	/* fill image data */

	SDE_ATRACE_BEGIN("mass_cmd_generation");
	for (c_cnt = 0; c_cnt < pcmds->count ; c_cnt++) {
		tcmds[c_cnt].msg.type = MIPI_DSI_GENERIC_LONG_WRITE;
		tcmds[c_cnt].last_command = 1;

		/* Memory Alloc for each cmds */
		if (tcmds[c_cnt].ss_txbuf == NULL) {
			/* HEADER TYPE 0x4C or 0x5C */
			tcmds[c_cnt].ss_txbuf = vzalloc(payload_len);
			if (tcmds[c_cnt].ss_txbuf == NULL) {
				LCD_ERR(vdd, "fail to vzalloc for self_mask cmds ss_txbuf\n");
				return;
			}
		}

		/* Copy from Data Buffer to each cmd Buffer */
		for (p_len = 0; p_len < payload_len && data_idx < data_size ; p_len++) {
			if (p_len % MASS_CMD_ALIGN)
				tcmds[c_cnt].ss_txbuf[p_len] = data[data_idx++];
			else
				tcmds[c_cnt].ss_txbuf[p_len] = (p_len == 0 && c_cnt == 0) ? 0x4C : 0x5C;

		}
		tcmds[c_cnt].msg.tx_len = p_len;

		ss_alloc_ss_txbuf(&tcmds[c_cnt], tcmds[c_cnt].ss_txbuf);
	}

	/* Image Check Sum Calculation */
	for (i = 0; i < data_size; i = i+4)
		check_sum_0 += data[i];

	for (i = 1; i < data_size; i = i+4)
		check_sum_1 += data[i];

	for (i = 2; i < data_size; i = i+4)
		check_sum_2 += data[i];

	for (i = 3; i < data_size; i = i+4)
		check_sum_3 += data[i];

	LCD_INFO(vdd, "[CheckSum] cmd=%s, data_size = %d, cs_0 = 0x%X, cs_1 = 0x%X, cs_2 = 0x%X, cs_3 = 0x%X\n",
			ss_get_cmd_name(cmd), data_size, check_sum_0, check_sum_1, check_sum_2, check_sum_3);

	vdd->self_disp.operation[op].img_checksum_cal = (check_sum_3 & 0xFF);
	vdd->self_disp.operation[op].img_checksum_cal |= ((check_sum_2 & 0xFF) << 8);
	vdd->self_disp.operation[op].img_checksum_cal |= ((check_sum_1 & 0xFF) << 16);
	vdd->self_disp.operation[op].img_checksum_cal |= ((check_sum_0 & 0xFF) << 24);

	SDE_ATRACE_END("mass_cmd_generation");

	LCD_INFO(vdd, "Total Cmd Count(%d), Last Cmd Payload Len(%d)\n", c_cnt, tcmds[c_cnt-1].msg.tx_len);
}


static void self_mask_img_write(struct samsung_display_driver_data *vdd)
{
	int wait_cnt = 1000; /* 1000 * 0.5ms = 500ms */

	if (!vdd->self_disp.is_support) {
		LCD_ERR(vdd, "self display is not supported..(%d)\n",
						vdd->self_disp.is_support);
		return;
	}

	/* To prevent self image disabled at boot time, during send these cmd
	 * => by block with samsung_splash_enabled, make it uses self mask img from UEFI
	 */
	if (vdd->samsung_splash_enabled) {
		LCD_INFO(vdd, "samsung_splash_enabled - skip to send self_mask_img\n");
		return;
	}

	LCD_INFO(vdd, "++\n");

	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.enable = 1;
	while (!list_empty(&vdd->cmd_lock.wait_list) && --wait_cnt)
		usleep_range(500, 510);

	ss_set_exclusive_tx_packet(vdd, TX_LEVEL1_KEY_ENABLE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_SELF_MASK_SET_PRE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_SELF_MASK_IMAGE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_SELF_MASK_SET_POST, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL1_KEY_DISABLE, 1);

	ss_send_cmd(vdd, TX_LEVEL1_KEY_ENABLE);
	ss_send_cmd(vdd, TX_SELF_MASK_SET_PRE);
	usleep_range(1000, 1100);
	ss_send_cmd(vdd, TX_SELF_MASK_IMAGE);
	usleep_range(1000, 1100);
	ss_send_cmd(vdd, TX_SELF_MASK_SET_POST);
	ss_send_cmd(vdd, TX_LEVEL1_KEY_DISABLE);

	ss_set_exclusive_tx_packet(vdd, TX_LEVEL1_KEY_ENABLE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_SELF_MASK_SET_PRE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_SELF_MASK_IMAGE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_SELF_MASK_SET_POST, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL1_KEY_DISABLE, 0);

	vdd->exclusive_tx.enable = 0;
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);

	LCD_INFO(vdd, "--\n");
}

static int self_mask_on(struct samsung_display_driver_data *vdd, int enable)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	if (!vdd->self_disp.is_support) {
		LCD_ERR(vdd, "self display is not supported..(%d)\n",
						vdd->self_disp.is_support);
		return -EACCES;
	}

	/* To prevent self image disabled at boot time, during send these cmd
	 * => by block with samsung_splash_enabled, make it uses self mask img from UEFI
	 */
	if (vdd->samsung_splash_enabled) {
		LCD_INFO(vdd, "samsung_splash_enabled - skip to send self mask on cmds\n");
		return ret;
	}

	mutex_lock(&vdd->self_disp.vdd_self_display_lock);

	if (enable) {
		if (vdd->is_factory_mode && vdd->self_disp.factory_support)
			ss_send_cmd(vdd, TX_SELF_MASK_ON_FACTORY);
		else
			ss_send_cmd(vdd, TX_SELF_MASK_ON);
	} else
		ss_send_cmd(vdd, TX_SELF_MASK_OFF);

	mutex_unlock(&vdd->self_disp.vdd_self_display_lock);

	LCD_INFO(vdd, "enable=%d, factory=%d\n", enable, vdd->is_factory_mode);

	return ret;
}

static int self_display_debug(struct samsung_display_driver_data *vdd)
{
	int rx_len;
	char buf[7] = {0,};

	if (is_ss_style_cmd(vdd, RX_SELF_DISP_DEBUG)) {
		rx_len = ss_send_cmd_get_rx(vdd, RX_SELF_DISP_DEBUG, buf);
		if (rx_len < 0) {
			LCD_ERR(vdd, "invalid rx_len(%d)\n", rx_len);
			return false;
		}
	} else {
		ss_panel_data_read(vdd, RX_SELF_DISP_DEBUG, buf, LEVEL1_KEY);
	}

	vdd->self_disp.debug.SM_SUM_O = ((buf[1] & 0xFF) << 24);
	vdd->self_disp.debug.SM_SUM_O |= ((buf[2] & 0xFF) << 16);
	vdd->self_disp.debug.SM_SUM_O |= ((buf[3] & 0xFF) << 8);
	vdd->self_disp.debug.SM_SUM_O |= (buf[4] & 0xFF);

	if (vdd->self_disp.operation[FLAG_SELF_MASK].img_checksum != vdd->self_disp.debug.SM_SUM_O) {
		LCD_ERR(vdd, "self mask img checksum fail!! 0x%X 0x%X\n",
			vdd->self_disp.operation[FLAG_SELF_MASK].img_checksum, vdd->self_disp.debug.SM_SUM_O);
		SS_XLOG(vdd->self_disp.debug.SM_SUM_O);
		return -1;
	} else
		LCD_INFO(vdd, "Self Mask Checksum Pass!\n");

	return 0;
}

static int self_display_open(struct inode *inode, struct file *file)
{
	/* TODO: get appropriate vdd..primary or secondary... */
	struct samsung_display_driver_data *vdd = ss_get_vdd(PRIMARY_DISPLAY_NDX);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	vdd->self_disp.file_open = 1;
	file->private_data = vdd;

	LCD_DEBUG(vdd, "[open]\n");

	return 0;
}

static int self_display_release(struct inode *inode, struct file *file)
{
	struct samsung_display_driver_data *vdd = file->private_data;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	vdd->self_disp.file_open = 0;

	LCD_DEBUG(vdd, "[release]\n");

	return 0;
}

static const struct file_operations self_display_fops = {
	.owner = THIS_MODULE,
	.open = self_display_open,
	.release = self_display_release,
};

#define DEV_NAME_SIZE 24
int self_display_init_ANA38407(struct samsung_display_driver_data *vdd)
{
	int ret = 0;
	static char devname[DEV_NAME_SIZE] = {'\0', };

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	if (!vdd->self_disp.is_support) {
		LCD_ERR(vdd, "Self Display is not supported\n");
		return -EINVAL;
	}

	mutex_init(&vdd->self_disp.vdd_self_display_lock);
	if (vdd->ndx == PRIMARY_DISPLAY_NDX)
		snprintf(devname, DEV_NAME_SIZE, "self_display");
	else
		snprintf(devname, DEV_NAME_SIZE, "self_display%d", vdd->ndx);

	vdd->self_disp.dev.minor = MISC_DYNAMIC_MINOR;
	vdd->self_disp.dev.name = devname;
	vdd->self_disp.dev.fops = &self_display_fops;
	vdd->self_disp.dev.parent = NULL;

	vdd->self_disp.debug.SM_SUM_O = 0xFF; /* initial value */

	vdd->self_disp.self_mask_img_write = self_mask_img_write;
	vdd->self_disp.self_mask_on = self_mask_on;
	vdd->self_disp.self_display_debug = self_display_debug;
	ret = ss_wrapper_misc_register(vdd, &vdd->self_disp.dev);
	if (ret) {
		LCD_ERR(vdd, "failed to register driver : %d\n", ret);
		vdd->self_disp.is_support = false;
		return -ENODEV;
	}

	LCD_INFO(vdd, "Success to register self_disp device..(%d)\n", ret);

	return ret;
}

MODULE_DESCRIPTION("Self Display driver");
MODULE_LICENSE("GPL");
