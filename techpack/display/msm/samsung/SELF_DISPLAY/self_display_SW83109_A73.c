/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
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
#include "self_display_SW83109_A73.h"

/* #define SELF_DISPLAY_TEST */

#define ONE_FRAME_US	((1000000 / vdd->vrr.cur_refresh_rate) + 1)

/*
 * make dsi_panel_cmds using image data
 */
void make_self_dispaly_img_cmds_SW83109_A73(struct samsung_display_driver_data *vdd,
		int cmd, u32 op)
{
	struct dsi_cmd_desc *tcmds;
	struct dsi_panel_cmd_set *pcmds;

	u32 data_size = vdd->self_disp.operation[op].img_size;
	char *data = vdd->self_disp.operation[op].img_buf;
	int i, j;
	int data_idx = 0;
	u32 check_sum_0 = 0;
	u32 check_sum_1 = 0;
	u32 check_sum_2 = 0;
	u32 check_sum_3 = 0;

	u32 p_size = CMD_ALIGN;
	u32 paylod_size = 0;
	u32 cmd_size = 0;

	LCD_INFO(vdd, "[SW83109_BOE_A73] ++\n");

	if (!data) {
		LCD_ERR(vdd, "data is null..\n");
		return;
	}

	if (!data_size) {
		LCD_ERR(vdd, "data size is zero..\n");
		return;
	}

	/* ss_txbuf size */
	while (p_size < MAX_PAYLOAD_SIZE) {
		 if (data_size % p_size == 0) {
			paylod_size = p_size;
		 }
		 p_size += CMD_ALIGN;
	}
	/* cmd size */
	cmd_size = data_size / paylod_size;

	LCD_INFO(vdd, "[%d] total data size [%d]\n", cmd, data_size);
	LCD_INFO(vdd, "cmd size [%d] ss_txbuf size [%d]\n", cmd_size, paylod_size);

	pcmds = ss_get_cmds(vdd, cmd);
	if (IS_ERR_OR_NULL(pcmds->cmds)) {
		LCD_INFO(vdd, "allocate pcmds->cmds\n");
		pcmds->cmds = kzalloc(cmd_size * sizeof(struct dsi_cmd_desc), GFP_KERNEL);
		if (IS_ERR_OR_NULL(pcmds->cmds)) {
			LCD_ERR(vdd, "fail to kzalloc for self_mask cmds \n");
			return;
		}
	}

	pcmds->state = DSI_CMD_SET_STATE_HS;
	pcmds->count = cmd_size;

	tcmds = pcmds->cmds;
	if (tcmds == NULL) {
		LCD_ERR(vdd, "tcmds is NULL \n");
		return;
	}

	for (i = 0; i < pcmds->count; i++) {
		tcmds[i].msg.type = MIPI_DSI_GENERIC_LONG_WRITE;
		tcmds[i].last_command = 1;

		/* fill image data */
		if (tcmds[i].ss_txbuf == NULL) {
			/* +1 means HEADER TYPE 0x4C or 0x5C */
			tcmds[i].ss_txbuf = kzalloc(paylod_size + 1, GFP_KERNEL);
			if (tcmds[i].ss_txbuf == NULL) {
				LCD_ERR(vdd, "fail to kzalloc for self_mask cmds ss_txbuf \n");
				return;
			}
		}

		tcmds[i].ss_txbuf[0] = (i == 0) ? 0x4C : 0x5C;

		for (j = 1; (j <= paylod_size) && (data_idx < data_size); j++)
			tcmds[i].ss_txbuf[j] = data[data_idx++];

		tcmds[i].msg.tx_len = j;

		ss_alloc_ss_txbuf(&tcmds[i], tcmds[i].ss_txbuf);

		LCD_DEBUG(vdd, "dlen (%d), data_idx (%d)\n", j, data_idx);
	}

	/* Image Check Sum Calculation */
	for (i = 0; i < data_size; i=i+4)
		check_sum_0 += data[i];

	for (i = 1; i < data_size; i=i+4)
		check_sum_1 += data[i];

	for (i = 2; i < data_size; i=i+4)
		check_sum_2 += data[i];

	for (i = 3; i < data_size; i=i+4)
		check_sum_3 += data[i];

	LCD_INFO(vdd, "[CheckSum] cmd=%d, data_size = %d, cs_0 = 0x%X, cs_1 = 0x%X, cs_2 = 0x%X, cs_3 = 0x%X\n", cmd, data_size, check_sum_0, check_sum_1, check_sum_2, check_sum_3);

	vdd->self_disp.operation[op].img_checksum_cal = (check_sum_3 & 0xFF);
	vdd->self_disp.operation[op].img_checksum_cal |= ((check_sum_2 & 0xFF) << 8);
	vdd->self_disp.operation[op].img_checksum_cal |= ((check_sum_1 & 0xFF) << 16);
	vdd->self_disp.operation[op].img_checksum_cal |= ((check_sum_0 & 0xFF) << 24);

	LCD_INFO(vdd, "[SW83109_BOE_A73] --\n");
	return;
}

void make_mass_self_display_img_cmds_SW83109_A73(struct samsung_display_driver_data *vdd,
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

	payload_len = data_size + (data_size + MASS_CMD_ALIGN - 1)/MASS_CMD_ALIGN;
	cmd_cnt = (payload_len + MAX_PAYLOAD_SIZE_MASS - 1) / MAX_PAYLOAD_SIZE_MASS;

	LCD_INFO(vdd, "[SW83109] [%s] total data size [%d], total cmd len[%d], cmd count [%d]\n",
			ss_get_cmd_name(cmd), data_size, payload_len, cmd_cnt);

	pcmds = ss_get_cmds(vdd, cmd);
	if (IS_ERR_OR_NULL(pcmds->cmds)) {
		LCD_INFO(vdd, "alloc mem for self_display cmd\n");
		pcmds->cmds = kzalloc(cmd_cnt * sizeof(struct dsi_cmd_desc), GFP_KERNEL);
		if (IS_ERR_OR_NULL(pcmds->cmds)) {
			LCD_ERR(vdd, "fail to kzalloc for self_mask cmds \n");
			return;
		}
	}

	pcmds->state = DSI_CMD_SET_STATE_HS;
	pcmds->count = cmd_cnt;

	tcmds = pcmds->cmds;
	if (tcmds == NULL) {
		LCD_ERR(vdd, "tcmds is NULL \n");
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
			tcmds[c_cnt].ss_txbuf = vzalloc(MAX_PAYLOAD_SIZE_MASS);
			if (tcmds[c_cnt].ss_txbuf == NULL) {
				LCD_ERR(vdd, "fail to vzalloc for self_mask cmds ss_txbuf\n");
				return;
			}
		}

		/* Copy from Data Buffer to each cmd Buffer */
		for (p_len = 0; p_len < MAX_PAYLOAD_SIZE_MASS && data_idx < data_size ; p_len++) {
			if (p_len % MASS_CMD_ALIGN)
				tcmds[c_cnt].ss_txbuf[p_len] = data[data_idx++];
			else
				tcmds[c_cnt].ss_txbuf[p_len] = (p_len == 0 && c_cnt == 0) ? 0x4C : 0x5C;

		}
		tcmds[c_cnt].msg.tx_len = p_len;

		ss_alloc_ss_txbuf(&tcmds[c_cnt], tcmds[c_cnt].ss_txbuf);
	}

	/* Image Check Sum Calculation */
	for (i = 0; i < data_size; i=i+4)
		check_sum_0 += data[i];

	for (i = 1; i < data_size; i=i+4)
		check_sum_1 += data[i];

	for (i = 2; i < data_size; i=i+4)
		check_sum_2 += data[i];

	for (i = 3; i < data_size; i=i+4)
		check_sum_3 += data[i];

	LCD_INFO(vdd, "[CheckSum] cmd=%s, data_size = %d, cs_0 = 0x%X, cs_1 = 0x%X, cs_2 = 0x%X, cs_3 = 0x%X\n",
			ss_get_cmd_name(cmd), data_size, check_sum_0, check_sum_1, check_sum_2, check_sum_3);

	vdd->self_disp.operation[op].img_checksum_cal = (check_sum_3 & 0xFF);
	vdd->self_disp.operation[op].img_checksum_cal |= ((check_sum_2 & 0xFF) << 8);
	vdd->self_disp.operation[op].img_checksum_cal |= ((check_sum_1 & 0xFF) << 16);
	vdd->self_disp.operation[op].img_checksum_cal |= ((check_sum_0 & 0xFF) << 24);

	SDE_ATRACE_END("mass_cmd_generation");

	LCD_INFO(vdd, "[SW83109_BOE_A73] Total Cmd Count(%d), Last Cmd Payload Len(%d)\n", c_cnt, tcmds[c_cnt-1].msg.tx_len);

	return;
}

static void self_mask_img_write(struct samsung_display_driver_data *vdd)
{
	int wait_cnt = 1000; /* 1000 * 0.5ms = 500ms */

	if (!vdd->self_disp.is_support) {
		LCD_ERR(vdd, "self display is not supported..(%d)\n",
						vdd->self_disp.is_support);
		return;
	}

	LCD_INFO(vdd, "[SW83109_BOE_A73] ++\n");

	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.enable = 1;
	while (!list_empty(&vdd->cmd_lock.wait_list) && --wait_cnt)
		usleep_range(500, 500);

	ss_set_exclusive_tx_packet(vdd, TX_SELF_MASK_SET_PRE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_SELF_MASK_IMAGE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_SELF_MASK_SET_POST, 1);

	/* TODO: pack below sets to one command set, and remove exclusive feature */
	LCD_DEBUG(vdd, "[SW83109_BOE_A73] tx self mask ++ (cur_rr:%d)\n", vdd->vrr.cur_refresh_rate);
	LCD_DEBUG(vdd, "[SW83109_BOE_A73] TX_SELF_MASK_SET_PRE\n");
	ss_send_cmd(vdd, TX_SELF_MASK_SET_PRE);

	/* skip one frame delay because self mask is default off at display on sequence.. */
	LCD_DEBUG(vdd, "[SW83109_BOE_A73] TX_SELF_MASK_IMAGE\n");
	ss_send_cmd(vdd, TX_SELF_MASK_IMAGE);
	usleep_range(500, 500);
	LCD_DEBUG(vdd, "[SW83109_BOE_A73] TX_SELF_MASK_SET_POST\n");
	ss_send_cmd(vdd, TX_SELF_MASK_SET_POST);
	LCD_DEBUG(vdd, "[SW83109_BOE_A73] tx self mask --\n");

	ss_set_exclusive_tx_packet(vdd, TX_SELF_MASK_SET_PRE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_SELF_MASK_IMAGE, 0);

	usleep_range(100, 110); /* needs 100us delay between 0x5C& 0x75 */

	ss_set_exclusive_tx_packet(vdd, TX_SELF_MASK_SET_POST, 0);
	vdd->exclusive_tx.enable = 0;
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);
	LCD_INFO(vdd, "[SW83109_BOE_A73] --\n");
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

	LCD_DEBUG(vdd, "[SW83109_BOE_A73] ++ (%d)\n", enable);

	mutex_lock(&vdd->self_disp.vdd_self_display_lock);

	if (enable) {
		if (vdd->is_factory_mode && vdd->self_disp.factory_support)
			ss_send_cmd(vdd, TX_SELF_MASK_ON_FACTORY);
		else
			ss_send_cmd(vdd, TX_SELF_MASK_ON);
	} else {
		ss_send_cmd(vdd, TX_SELF_MASK_OFF);
	}

	mutex_unlock(&vdd->self_disp.vdd_self_display_lock);

	LCD_DEBUG(vdd, "[SW83109_BOE_A73] --\n");

	return ret;
}

#define WAIT_FRAME (2)

static int self_mask_check(struct samsung_display_driver_data *vdd)
{
	int i, ret = 1;
	int wait_cnt = 1000; /* 1000 * 0.5ms = 500ms */

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return 0;
	}

	if (!vdd->self_disp.is_support) {
		LCD_ERR(vdd, "self display is not supported..(%d)\n",
						vdd->self_disp.is_support);
		return 0;
	}

	if (!vdd->self_disp.mask_crc_size) {
		LCD_ERR(vdd, "mask crc size is zero..\n\n");
		return 0;
	}

	if (!vdd->self_disp.mask_crc_read_data) {
		vdd->self_disp.mask_crc_read_data = kzalloc(vdd->self_disp.mask_crc_size, GFP_KERNEL);
		if (!vdd->self_disp.mask_crc_read_data) {
			LCD_ERR(vdd, "fail to alloc for mask_crc_read_data\n");
			return 0;
		}
	}

	LCD_INFO(vdd, "[SW83109_BOE_A73] ++\n");

	mutex_lock(&vdd->self_disp.vdd_self_display_lock);

	ss_send_cmd(vdd, TX_SELF_MASK_CHECK_PRE1);

	/* Do not permit image update (2C, 3C) during sending self mask image (4C, 5C) */
	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.enable = 1;
	while (!list_empty(&vdd->cmd_lock.wait_list) && --wait_cnt)
		usleep_range(500, 500);

	ss_set_exclusive_tx_packet(vdd, TX_SELF_MASK_IMAGE_CRC, 1);

	/* self mask data write (4C, 5C) */
	ss_send_cmd(vdd, TX_SELF_MASK_IMAGE_CRC);

	ss_set_exclusive_tx_packet(vdd, TX_SELF_MASK_IMAGE_CRC, 0);
	vdd->exclusive_tx.enable = 0;
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);

	ss_send_cmd(vdd, TX_SELF_MASK_CHECK_PRE2);

	ss_panel_data_read(vdd, RX_SELF_MASK_CHECK, vdd->self_disp.mask_crc_read_data, LEVEL_KEY_NONE);
	ss_send_cmd(vdd, TX_SELF_MASK_CHECK_POST);

	for (i = 0; i < vdd->self_disp.mask_crc_size; i++) {
		if (vdd->self_disp.mask_crc_read_data[i] != vdd->self_disp.mask_crc_pass_data[i]) {
			LCD_ERR(vdd, "self mask check fail !!\n");
			ret = 0;
			break;
		}
	}

	mutex_unlock(&vdd->self_disp.vdd_self_display_lock);

	LCD_INFO(vdd, "[SW83109_BOE_A73] --\n");

	return ret;
}


static int self_display_aod_enter(struct samsung_display_driver_data *vdd)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	if (!vdd->self_disp.is_support) {
		LCD_DEBUG(vdd, "self display is not supported..(%d)\n",
								vdd->self_disp.is_support);
		return -ENODEV;
	}

	LCD_DEBUG(vdd, "[SW83109_BOE_A73] ++\n");

	if (!vdd->self_disp.on)
		self_mask_on(vdd, false);

	vdd->self_disp.on = true;

	LCD_DEBUG(vdd, "[SW83109_BOE_A73] --\n");

	return ret;
}

static int self_display_aod_exit(struct samsung_display_driver_data *vdd)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	if (!vdd->self_disp.is_support) {
		LCD_DEBUG(vdd, "self display is not supported..(%d)\n",
								vdd->self_disp.is_support);
		return -ENODEV;
	}

	LCD_DEBUG(vdd, "[SW83109_BOE_A73] ++\n");

	self_mask_on(vdd, true);

	vdd->self_disp.on = false;

	LCD_DEBUG(vdd, "[SW83109_BOE_A73] --\n");

	return ret;
}

static int self_display_open(struct inode *inode, struct file *file)
{
	struct miscdevice *c = file->private_data;
	struct dsi_display *display = dev_get_drvdata(c->parent);
	struct dsi_panel *panel = display->panel;
	struct samsung_display_driver_data *vdd = panel->panel_private;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	vdd->self_disp.file_open = 1;

	LCD_INFO(vdd, "[SW83109] [open]\n");

	return 0;
}

static int self_display_release(struct inode *inode, struct file *file)
{
	struct miscdevice *c = file->private_data;
	struct dsi_display *display = dev_get_drvdata(c->parent);
	struct dsi_panel *panel = display->panel;
	struct samsung_display_driver_data *vdd = panel->panel_private;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	vdd->self_disp.file_open = 0;

	LCD_INFO(vdd, "[SW83109] [release]\n");

	return 0;
}

static const struct file_operations self_display_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = NULL,
	.open = self_display_open,
	.release = self_display_release,
	.write = NULL,
};

#define DEV_NAME_SIZE 24
int self_display_init_SW83109_A73(struct samsung_display_driver_data *vdd)
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

	if (!vdd->self_disp.is_support) {
		LCD_ERR(vdd, "Self Display is not supported\n");
		return -EINVAL;
	}

	panel = (struct dsi_panel *)vdd->msm_private;
	host = panel->mipi_device.host;
	display = container_of(host, struct dsi_display, host);

	mutex_init(&vdd->self_disp.vdd_self_display_lock);
	mutex_init(&vdd->self_disp.vdd_self_display_ioctl_lock);

	if (vdd->ndx == PRIMARY_DISPLAY_NDX)
		snprintf(devname, DEV_NAME_SIZE, "self_display");
	else
		snprintf(devname, DEV_NAME_SIZE, "self_display%d", vdd->ndx);

	vdd->self_disp.dev.minor = MISC_DYNAMIC_MINOR;
	vdd->self_disp.dev.name = devname;
	vdd->self_disp.dev.fops = &self_display_fops;
	vdd->self_disp.dev.parent = &display->pdev->dev;

	vdd->self_disp.aod_enter = self_display_aod_enter;
	vdd->self_disp.aod_exit = self_display_aod_exit;
	vdd->self_disp.self_mask_img_write = self_mask_img_write;
	vdd->self_disp.self_mask_on = self_mask_on;
	vdd->self_disp.self_mask_check = self_mask_check;

	ret = ss_wrapper_misc_register(vdd, &vdd->self_disp.dev);
	if (ret) {
		LCD_ERR(vdd, "failed to register driver : %d\n", ret);
		vdd->self_disp.is_support = false;
		return -ENODEV;
	}

	LCD_INFO(vdd, "[SW83109_BOE_A73] Success to register self_disp device..(%d)\n", ret);

	return ret;
}

MODULE_DESCRIPTION("Self Display driver");
MODULE_LICENSE("GPL");
