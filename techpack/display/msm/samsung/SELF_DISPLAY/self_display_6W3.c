/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
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
#include "self_display_6W3.h"

/* #define SELF_DISPLAY_TEST */

/*
 * make dsi_panel_cmds using image data
 */
void make_self_display_img_cmds_6W3(struct samsung_display_driver_data *vdd,
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

	return;
}

static void self_mask_img_write(struct samsung_display_driver_data *vdd)
{
	int wait_cnt = 1000; /* 1000 * 0.5ms = 500ms */

	if (!vdd->self_disp.is_support) {
		LCD_ERR(vdd, "self display is not supported..(%d) \n",
						vdd->self_disp.is_support);
		return;
	}

	/*To prevent self image disabled at boot time, during send these cmd
	  => by block with samsung_splash_enabled, make it uses self mask img from UEFI */
	if (vdd->samsung_splash_enabled) {
		LCD_INFO(vdd, "samsung_splash_enabled - skip to send self_mask_img\n");
		return;
	}

	LCD_INFO(vdd, "++\n");

	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.enable = 1;
	while (!list_empty(&vdd->cmd_lock.wait_list) && --wait_cnt)
		usleep_range(500, 500);

	ss_set_exclusive_tx_packet(vdd, TX_LEVEL1_KEY_ENABLE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_SELF_MASK_SET_PRE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_SELF_MASK_IMAGE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_SELF_MASK_SET_POST, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL1_KEY_DISABLE, 1);

	ss_send_cmd(vdd, TX_LEVEL1_KEY_ENABLE);

	ss_send_cmd(vdd, TX_SELF_MASK_SET_PRE);

	ss_send_cmd(vdd, TX_SELF_MASK_IMAGE);

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
		LCD_ERR(vdd, "self display is not supported..(%d) \n",
						vdd->self_disp.is_support);
		return -EACCES;
	}

	/*To prevent self image disabled at boot time, during send these cmd
	  => by block with samsung_splash_enabled, make it uses self mask img from UEFI */
	if (vdd->samsung_splash_enabled) {
		LCD_INFO(vdd, "samsung_splash_enabled - skip to send self_mask_on\n");
		return ret;
	}

	LCD_INFO(vdd, "++ (%d)\n", enable);

	mutex_lock(&vdd->self_disp.vdd_self_display_lock);

	if (enable) {
		if (vdd->is_factory_mode && vdd->self_disp.factory_support)
			ss_send_cmd(vdd, TX_SELF_MASK_ON_FACTORY);
		else
			ss_send_cmd(vdd, TX_SELF_MASK_ON);
	} else
		ss_send_cmd(vdd, TX_SELF_MASK_OFF);

	mutex_unlock(&vdd->self_disp.vdd_self_display_lock);

	LCD_INFO(vdd, "-- \n");

	return ret;
}

static int self_display_debug(struct samsung_display_driver_data *vdd)
{
	char buf[4];

	if (ss_get_cmds(vdd, RX_SELF_DISP_DEBUG)->count) {

		ss_panel_data_read(vdd, RX_SELF_DISP_DEBUG, buf, LEVEL1_KEY);
		vdd->self_disp.debug.SM_SUM_O = ((buf[0] & 0xFF) << 24);
		vdd->self_disp.debug.SM_SUM_O |= ((buf[1] & 0xFF) << 16);
		vdd->self_disp.debug.SM_SUM_O |= ((buf[2] & 0xFF) << 8);
		vdd->self_disp.debug.SM_SUM_O |= (buf[3] & 0xFF);

		LCD_INFO(vdd, "SM_SUM_O(0x%X)\n",
			vdd->self_disp.debug.SM_SUM_O);

		if (vdd->self_disp.operation[FLAG_SELF_MASK].img_checksum !=
					vdd->self_disp.debug.SM_SUM_O) {
			LCD_ERR(vdd, "self mask img checksum fail!!\n");
			return -1;
		}
	}

	return 0;
}

/*
 * self_display_ioctl() : get ioctl from aod framework.
 *                           set self display related registers.
 */
static long self_display_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct samsung_display_driver_data *vdd = file->private_data;
	int ret = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	if (!ss_is_ready_to_send_cmd(vdd)) {
		LCD_ERR(vdd, "Panel is not ready. Panel State(%d)\n", vdd->panel_state);
		return -ENODEV;
	}

	if (!vdd->self_disp.on) {
		LCD_ERR(vdd, "self_display was turned off\n");
		return -EPERM;
	}

	if ((_IOC_TYPE(cmd) != SELF_DISPLAY_IOCTL_MAGIC) ||
				(_IOC_NR(cmd) >= IOCTL_SELF_MAX)) {
		LCD_ERR(vdd, "TYPE(%u) NR(%u) is wrong..\n",
			_IOC_TYPE(cmd), _IOC_NR(cmd));
		return -EINVAL;
	}

	LCD_INFO(vdd, "cmd = %s\n", cmd == IOCTL_SELF_MOVE_EN ? "IOCTL_SELF_MOVE_EN" :
				cmd == IOCTL_SELF_MOVE_OFF ? "IOCTL_SELF_MOVE_OFF" :
				cmd == IOCTL_SET_ICON ? "IOCTL_SET_ICON" :
				cmd == IOCTL_SET_GRID ? "IOCTL_SET_GRID" :
				cmd == IOCTL_SET_ANALOG_CLK ? "IOCTL_SET_ANALOG_CLK" :
				cmd == IOCTL_SET_DIGITAL_CLK ? "IOCTL_SET_DIGITAL_CLK" :
				cmd == IOCTL_SET_TIME ? "IOCTL_SET_TIME" :
				cmd == IOCTL_SET_PARTIAL_HLPM_SCAN ? "IOCTL_PARTIAL_HLPM_SCAN" : "IOCTL_ERR");

	switch (cmd) {
	default:
		LCD_ERR(vdd, "invalid cmd : %u \n", cmd);
		break;
	}

	return ret;
}

/*
 * self_display_write() : get image data from aod framework.
 *                           prepare for dsi_panel_cmds.
 */
static ssize_t self_display_write(struct file *file, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	struct samsung_display_driver_data *vdd = file->private_data;
	char op_buf[IMAGE_HEADER_SIZE];
	u32 op = 0;
	int ret = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "no vdd");
		return -ENODEV;
	}

	if (unlikely(!buf)) {
		LCD_ERR(vdd, "invalid read buffer\n");
		return -EINVAL;
	}

	if (count <= IMAGE_HEADER_SIZE) {
		LCD_ERR(vdd, "Invalid Buffer Size (%d)\n", (int)count);
		return -EINVAL;
	}

	/*
	 * get 2byte flas to distinguish what operation is passing
	 */
	ret = copy_from_user(op_buf, buf, IMAGE_HEADER_SIZE);
	if (unlikely(ret < 0)) {
		LCD_ERR(vdd, "failed to copy_from_user (header)\n");
		return ret;
	}

	LCD_INFO(vdd, "Header Buffer = %c%c\n", op_buf[0], op_buf[1]);

	if (op_buf[0] == 'I' && op_buf[1] == 'C')
		op = FLAG_SELF_ICON;
	else if (op_buf[0] == 'A' && op_buf[1] == 'C')
		op = FLAG_SELF_ACLK;
	else if (op_buf[0] == 'D' && op_buf[1] == 'C')
		op = FLAG_SELF_DCLK;
	else {
		LCD_ERR(vdd, "Invalid Header, (%c%c)\n", op_buf[0], op_buf[1]);
		return -EINVAL;
	}

	LCD_INFO(vdd, "flag (%d) \n", op);

	if (op >= FLAG_SELF_DISP_MAX) {
		LCD_ERR(vdd, "invalid data flag : %d \n", op);
		return -EINVAL;
	}

	if (count > vdd->self_disp.operation[op].img_size+IMAGE_HEADER_SIZE) {
		LCD_ERR(vdd, "Buffer OverFlow Detected!! Buffer_Size(%d) Write_Size(%d)\n",
			vdd->self_disp.operation[op].img_size, (int)count);
		return -EINVAL;
	}

	vdd->self_disp.operation[op].wpos = *ppos;
	vdd->self_disp.operation[op].wsize = count;

	ret = copy_from_user(vdd->self_disp.operation[op].img_buf, buf+IMAGE_HEADER_SIZE, count-IMAGE_HEADER_SIZE);
	if (unlikely(ret < 0)) {
		LCD_ERR(vdd, "failed to copy_from_user (data)\n");
		return ret;
	}

	switch (op) {
	case FLAG_SELF_MASK:
		make_self_display_img_cmds_6W3(vdd, TX_SELF_MASK_IMAGE, op);
		vdd->self_disp.operation[op].select = true;
		break;
	default:
		LCD_ERR(vdd, "invalid data flag %d \n", op);
		break;
	}

	return ret;
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
	.unlocked_ioctl = self_display_ioctl,
	.open = self_display_open,
	.release = self_display_release,
	.write = self_display_write,
};

#define DEV_NAME_SIZE 24
int self_display_init_6W3(struct samsung_display_driver_data *vdd)
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

