/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * DDI operation :DDI Firmware Update etc.
 * Author: QC LCD driver <kyonghee.ma@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "ss_dsi_panel_common.h"
#include "fw_update_ANA38407.h"

static u32 fw_id_read(struct samsung_display_driver_data *vdd)
{
	u8 ddi_fw_id[4];
	int rx_len;

	/* Read mtp (F7h 1~4th) for DDI FW REV */
	if (ss_get_cmds(vdd, RX_DDI_FW_ID)) {
		rx_len = ss_send_cmd_get_rx(vdd, RX_DDI_FW_ID, ddi_fw_id);

		vdd->fw.fw_id = 0x00000000 | ddi_fw_id[0] << 8 | ddi_fw_id[1];
		LCD_INFO(vdd, "fw_id: 0x%x  Working info: 0x%x(4:1st, C:2nd)\n",
			vdd->fw.fw_id, ddi_fw_id[2]);
		vdd->fw.fw_working = ddi_fw_id[2];
	} else {
		LCD_ERR(vdd, "DSI%d no ddi_fw_id_rx_cmds cmds", vdd->ndx);
		return false;
	}
	return vdd->fw.fw_id;
}

/* Return 1 : If update need */
static int fw_check_ANA38407(struct samsung_display_driver_data *vdd, u32 fw_id)
{
	int ret = 0;
	u32 tried;

	/* FW Print debug data & get try_count */
	tried = ss_read_fw_up_debug_partition();
	LCD_INFO(vdd, "FW_UP try_count:%x\n", tried);

	/* FirmWare Update : Limit tried number to avoid abnormal panel */
	if (vdd->samsung_splash_enabled && !vdd->is_recovery_mode && (tried < 0x00000005)) {
		if (fw_id == 0x00000004) { // Add below for test: || fw_id == 0x00000005
			LCD_INFO(vdd, "fw_id:%x => Need for FirmWare Update!!\n", fw_id);
			ret = 1; /* Need update */
		}  /* FW id && working check: 1stFW seems broken -> retry */
		else if ((fw_id == 0x00000000 && vdd->fw.fw_working == 0x0f) || vdd->fw.fw_working == 0x0c) {
			LCD_INFO(vdd, "fw_id=%x, Working:0x%x-> Might have been failed.\n",
				fw_id, vdd->fw.fw_working);
			ret  = 1;
		}
	}

	return ret;
}

static int fw_write_ANA38407(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *erase_tx_cmds = NULL;
	struct dsi_panel_cmd_set *read_rx_cmds = NULL;
	struct dsi_panel_cmd_set *write_tx_cmds = NULL;
	int cur_addr, last_addr, write_size, buf_pos/*, loop*/;
	int ret = 0;
	int read_cnt;
	bool mem_check_fail;
	u8 read_data[256];
	u32 img_size, start_addr;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "no vdd\n");
		return -EINVAL;
	}

	erase_tx_cmds = ss_get_cmds(vdd, TX_FW_ERASE);
	if (SS_IS_CMDS_NULL(erase_tx_cmds)) {
		LCD_ERR(vdd, "No cmds for TX_FW_ERASE..\n");
		return -ENODEV;
	}
	write_tx_cmds = ss_get_cmds(vdd, TX_FW_WRITE);
	if (SS_IS_CMDS_NULL(write_tx_cmds)) {
		LCD_ERR(vdd, "No cmds for TX_FW_WRITE..\n");
		return -ENODEV;
	}
	read_rx_cmds = ss_get_cmds(vdd, RX_FW_READ);
	if (SS_IS_CMDS_NULL(read_rx_cmds)) {
		LCD_ERR(vdd, "No cmds for TX_FW_READ..\n");
		return -ENODEV;
	}

	read_rx_cmds->cmds[0].msg.rx_buf = read_data;
	read_rx_cmds->state = DSI_CMD_SET_STATE_HS;

	start_addr = vdd->fw.start_addr;
	img_size = vdd->fw.image_size;
	last_addr = vdd->fw.start_addr + vdd->fw.image_size;
	write_size = vdd->fw.write_data_size;
	buf_pos = 0;

	/* FW PROTECTION DISABLE */
	LCD_INFO(vdd, "FW Protection Disable\n");
	ss_send_cmd(vdd, TX_FW_PROT_DISABLE); //1st FW only

	/* FW Erase */  // 2 SEC
	cur_addr = start_addr;
	LCD_INFO(vdd, "FW ERASE addr:4000h, 5000h delay 1SEC included\n");
	ss_send_cmd(vdd, TX_FW_ERASE); // 4000h, 5000h

	/* FW WRITE */  // 7 SEC
	LCD_INFO(vdd, "FW Write Start Address(0x%x), Write Size(%d), Last Address(0x%x)\n",
		start_addr, write_size, last_addr);

	for (cur_addr = start_addr; (cur_addr < last_addr) && (buf_pos < img_size); ) {

		if (cur_addr >= last_addr)
			break;
		/* Write Data */
		memcpy(&write_tx_cmds->cmds[2].ss_txbuf[1], &fw_img_data[buf_pos], write_size);

		write_tx_cmds->cmds[4].ss_txbuf[vdd->fw.write_addr_idx[0]] = (cur_addr & 0xFF0000) >> 16;
		write_tx_cmds->cmds[4].ss_txbuf[vdd->fw.write_addr_idx[1]] = (cur_addr & 0x00FF00) >> 8;
		write_tx_cmds->cmds[4].ss_txbuf[vdd->fw.write_addr_idx[2]] = (cur_addr & 0x0000FF);
		LCD_INFO(vdd, "FW Write cur_addr:0x%x, 100ms delay included\n", cur_addr);
		/* Write Data */
		ss_send_cmd(vdd, TX_FW_WRITE);

		cur_addr += write_size;
		buf_pos += write_size;
		LCD_INFO(vdd, "FW Write will jump to cur_addr(0x%x), buf_pos(0x%x)\n",
			cur_addr, buf_pos);

	}

	/* FW READ & compare  */ // 15sec
	LCD_INFO(vdd, "FW READ Start Address(0x%x), Write Size(%d), Last Address(0x%x)\n",
		start_addr, write_size, last_addr);

	buf_pos = 0;
	for (cur_addr = start_addr; (cur_addr < last_addr) && (buf_pos < img_size); ) {

		if (cur_addr >= last_addr)
			break;
		/* Read Data */
		read_rx_cmds->cmds[3].ss_txbuf[vdd->fw.write_addr_idx[0]] = (cur_addr & 0xFF0000) >> 16;
		read_rx_cmds->cmds[3].ss_txbuf[vdd->fw.write_addr_idx[1]] = (cur_addr & 0x00FF00) >> 8;
		read_rx_cmds->cmds[3].ss_txbuf[vdd->fw.write_addr_idx[2]] = (cur_addr & 0x0000FF);
		LCD_INFO(vdd, "FW Read cur_addr:0x%x, 20ms included\n", cur_addr);

		memset(read_data, 0x00, 256);
		read_cnt = ss_send_cmd_get_rx(vdd, RX_FW_READ, read_data);

		/* Compare Write Data & Read Data */
		mem_check_fail = false;

		if (!memcmp(read_data, fw_img_data+buf_pos, 128))
			mem_check_fail = false; // the same
		else
			mem_check_fail = true; // not same

		if (mem_check_fail) {
			LCD_ERR(vdd, "FW Check Fail!! Address [0x%02x%02x%02x]\n",
				read_rx_cmds->cmds[3].ss_txbuf[vdd->fw.write_addr_idx[0]],
				read_rx_cmds->cmds[3].ss_txbuf[vdd->fw.write_addr_idx[1]],
				read_rx_cmds->cmds[3].ss_txbuf[vdd->fw.write_addr_idx[2]]);
		} else {
			LCD_INFO(vdd, "FW Check Pass!! Address [0x%02x%02x%02x] \n",
				read_rx_cmds->cmds[3].ss_txbuf[vdd->fw.write_addr_idx[0]],
				read_rx_cmds->cmds[3].ss_txbuf[vdd->fw.write_addr_idx[1]],
				read_rx_cmds->cmds[3].ss_txbuf[vdd->fw.write_addr_idx[2]]);
		}

		cur_addr += write_size;
		buf_pos += write_size;
		LCD_INFO(vdd, "FW read %dbytes => go to cur_addr(0x%x), buf_pos(0x%x)\n",
			read_cnt, cur_addr, buf_pos);
	}

	if (mem_check_fail) {
		ss_write_fw_up_debug_partition(FW_UP_FAIL, 0);
		LCD_ERR(vdd, "FW Write/Read Failed!!!\n");
	} else {
		ss_write_fw_up_debug_partition(FW_UP_PASS, 0);
		LCD_ERR(vdd, "FW Write/Read Passed!!!\n");
	}

	/* FW PROTECTION ENBLE */
	LCD_INFO(vdd, "FW Protection Enable\n");
	ss_send_cmd(vdd, TX_FW_PROT_ENABLE); //1st FW only

	LCD_INFO(vdd, "FW Write&Read Finish\n");

	return ret;
}


static int fw_update_ANA38407(struct samsung_display_driver_data *vdd)
{
	int rc = 0;
	int wait_cnt = 1000; /* 1000 * 0.5ms = 500ms */
	struct dsi_display *display = NULL;
	struct msm_drm_private *priv = NULL;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "no vdd\n");
		return -EINVAL;
	}

	display = GET_DSI_DISPLAY(vdd);
	if (IS_ERR_OR_NULL(display)) {
		LCD_ERR(vdd, "no display");
		return -EINVAL;
	}

	if (!vdd->fw.is_support) {
		LCD_ERR(vdd, "FirmWare Support is not supported\n");
		return -EINVAL;
	}

	LCD_INFO(vdd, "FirmWare Update Start\n");

	/* MAX CPU/BW ON */
	priv = display->drm_dev->dev_private;
	ss_set_max_cpufreq(vdd, true, CPUFREQ_CLUSTER_ALL);
	ss_set_max_mem_bw(vdd, true);
	dsi_display_clk_ctrl(display->dsi_clk_handle, DSI_ALL_CLKS, DSI_CLK_ON);

	/* Enter exclusive mode */
	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.permit_frame_update = false;
	vdd->exclusive_tx.enable = true;
	while (!list_empty(&vdd->cmd_lock.wait_list) && --wait_cnt)
		usleep_range(500, 500);
	ss_set_exclusive_tx_packet(vdd, TX_FW_PROT_DISABLE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_FW_PROT_ENABLE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_FW_ERASE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_FW_WRITE, 1);
	ss_set_exclusive_tx_packet(vdd, RX_FW_READ, 1);
	ss_set_exclusive_tx_packet(vdd, RX_FW_READ_CHECK, 1);
	ss_set_exclusive_tx_packet(vdd, RX_FW_READ_MTPID, 1);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 1);

	ss_write_fw_up_debug_partition(FW_UP_TRY, 0);
	fw_write_ANA38407(vdd);

	/* exit exclusive mode*/
	ss_set_exclusive_tx_packet(vdd, TX_FW_PROT_DISABLE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_FW_PROT_ENABLE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_FW_ERASE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_FW_WRITE, 0);
	ss_set_exclusive_tx_packet(vdd, RX_FW_READ, 0);
	ss_set_exclusive_tx_packet(vdd, RX_FW_READ_CHECK, 0);
	ss_set_exclusive_tx_packet(vdd, RX_FW_READ_MTPID, 0);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 0);
	vdd->exclusive_tx.enable = false;
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);

	/* MAX CPU OFF */
	dsi_display_clk_ctrl(display->dsi_clk_handle, DSI_ALL_CLKS, DSI_CLK_OFF);
	ss_set_max_mem_bw(vdd, false);
	ss_set_max_cpufreq(vdd, false, CPUFREQ_CLUSTER_ALL);

	LCD_INFO(vdd, "FirmWare Update Finish\n");
	return rc;
}

int fw_update_init_ANA38407(struct samsung_display_driver_data *vdd)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	vdd->fw.fw_check = fw_check_ANA38407;
	vdd->fw.fw_update = fw_update_ANA38407;
	vdd->fw.fw_id_read = fw_id_read;

	LCD_INFO(vdd, "Success to init fw_update files..(%d)\n", ret);

	return ret;
}


MODULE_DESCRIPTION("fw update driver");
MODULE_LICENSE("GPL");
