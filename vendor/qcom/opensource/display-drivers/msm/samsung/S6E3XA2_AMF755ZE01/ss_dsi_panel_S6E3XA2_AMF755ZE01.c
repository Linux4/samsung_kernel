/*
 * =================================================================
 *
 *
 *	Description:  samsung display panel file
 *
 *	Author: jb09.kim
 *	Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2017, Samsung Electronics. All rights reserved.

*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/
#include "ss_dsi_panel_S6E3XA2_AMF755ZE01.h"
#include "ss_dsi_mdnie_S6E3XA2_AMF755ZE01.h"

#if 0
static void ss_read_dsc_1(struct samsung_display_driver_data *vdd)
{
	u8 rbuf;
	int ret;

	ret = ss_panel_data_read(vdd, RX_LDI_DEBUG7, &rbuf, LEVEL_KEY_NONE);
	if (ret) {
		LCD_ERR(vdd, "fail to read RX_LDI_DEBUG7(ret=%d)\n", ret);
		return;
	}

	return;
}

static void ss_read_dsc_2(struct samsung_display_driver_data *vdd)
{
	u8 rbuf[89];
	int ret;
	char pBuffer[512];
	int j;

	ret = ss_panel_data_read(vdd, RX_LDI_DEBUG8, rbuf, LEVEL_KEY_NONE);
	if (ret) {
		LCD_ERR(vdd, "fail to read RX_LDI_DEBUG8(ret=%d)\n", ret);
		return;
	}

	memset(pBuffer, 0x00, 512);
	for (j = 0; j < 89; j++) {
		snprintf(pBuffer + strnlen(pBuffer, 512), 512, " %02x", rbuf[j]);
	}
	LCD_INFO(vdd, "PPS(read) : %s\n", pBuffer);
#if 0
	for (j = 0; j < 89; j++) {
		if (vdd->qc_pps_cmd[j] != rbuf[j]) {
			LCD_ERR(vdd, "[%d] pps different!! %02x %02x\n", j, vdd->qc_pps_cmd[j], rbuf[j]);
			//panic("PPS");
		}
	}
#endif
	return;
}

static int ss_cmd_log_read(struct samsung_display_driver_data *vdd)
{
	char buf[15];
	memset(buf, 0x00, 15);

	ss_read_ddi_cmd_log(vdd, buf);
	return 0;
}
#endif

static int samsung_panel_on_pre(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd : 0x%zx", __func__, (size_t)vdd);
		return false;
	}

	LCD_INFO(vdd, "+: ndx=%d\n", vdd->ndx);
	ss_panel_attach_set(vdd, true);

	return true;
}

#if 0
static int samsung_display_on_post(struct samsung_display_driver_data *vdd)
{
	ss_read_dsc_1(vdd);
	ss_read_dsc_2(vdd);

	return 0;
}
#endif

static int samsung_panel_on_post(struct samsung_display_driver_data *vdd)
{
	/*
	 * self mask is enabled from bootloader.
	 * so skip self mask setting during splash booting.
	 */
	if (!vdd->samsung_splash_enabled) {
		if (vdd->self_disp.self_mask_img_write)
			vdd->self_disp.self_mask_img_write(vdd);
	} else {
		LCD_INFO(vdd, "samsung splash enabled.. skip image write\n");
	}

	if (vdd->self_disp.self_mask_on)
		vdd->self_disp.self_mask_on(vdd, true);

	/* mafpc */
	if (vdd->mafpc.is_support) {
		vdd->mafpc.need_to_write = true;
		LCD_INFO(vdd, "Need to write mafpc image data to DDI\n");
	}

	return true;
}

static char ss_panel_revision(struct samsung_display_driver_data *vdd)
{
	if (vdd->manufacture_id_dsi == PBA_ID)
		ss_panel_attach_set(vdd, false);
	else
		ss_panel_attach_set(vdd, true);

	switch (ss_panel_rev_get(vdd)) {
	case 0x00:
	case 0x01:
		vdd->panel_revision = 'A';	/* 0x01 */
		break;
	case 0x02:
		vdd->panel_revision = 'B'; 	/* 0x02 */
		break;
	case 0x03:
		vdd->panel_revision = 'C'; 	/* 0x03 */
		break;
	default:
		vdd->panel_revision = 'C';
		LCD_ERR(vdd, "Invalid panel_rev(default rev : %c)\n",
				vdd->panel_revision);
		break;
	}

	vdd->panel_revision -= 'A';

	LCD_INFO(vdd, "panel_revision = %c %d \n",
					vdd->panel_revision + 'A', vdd->panel_revision);

	return (vdd->panel_revision + 'A');
}

#define get_bit(value, shift, width)	((value >> shift) & (GENMASK(width - 1, 0)))

static u8 IRC_MEDERATO_VAL1[IRC_MAX_MODE][4] = {
	{0xE1, 0x55, 0x01, 0x00},
	{0xA1, 0xE3, 0x02, 0xAA},
};

static u8 IRC_MEDERATO_VAL2[IRC_MAX_MODE][18] = {
	{0x77, 0x7E, 0xBE, 0x04, 0x04, 0x04, 0x12, 0x12, 0x12, 0x18, 0x18, 0x18, 0x1C, 0x1C, 0x1C, 0x1E, 0x1E, 0x1E},
	{0x3F, 0x42, 0x65, 0x08, 0x08, 0x08, 0x22, 0x22, 0x22, 0x2D, 0x2D, 0x2D, 0x36, 0x36, 0x36, 0x39, 0x39, 0x39},
};

static struct dsi_panel_cmd_set *ss_irc(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	int idx;
	int irc_mode = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	pcmds = ss_get_cmds(vdd, TX_IRC);
	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_IRC..\n");
		return NULL;
	}

	irc_mode = vdd->br_info.common_br.irc_mode;

	/* IRC mode*/
	idx = ss_get_cmd_idx(pcmds, 0x156, 0x63);
	if (idx > 0) {
		if (irc_mode < IRC_MAX_MODE) {
			memcpy(&pcmds->cmds[idx].ss_txbuf[2], IRC_MEDERATO_VAL1[irc_mode], sizeof(IRC_MEDERATO_VAL1[0]) / sizeof(u8));
			memcpy(&pcmds->cmds[idx].ss_txbuf[14], IRC_MEDERATO_VAL2[irc_mode], sizeof(IRC_MEDERATO_VAL2[0]) / sizeof(u8));
		}
	}

	LCD_INFO(vdd, "irc_mode : %d\n", irc_mode);

	return pcmds;
}

static struct dsi_panel_cmd_set * ss_brightness_gamma_mode2_normal(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	struct vrr_info *vrr = &vdd->vrr;
	int idx = 0;
	int smooth_idx = 0;
	int tset;
	bool dim_off = false;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, ": Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_NORMAL);

	/* ELVSS Temp comp */
	tset =  vdd->br_info.temperature > 0 ?  vdd->br_info.temperature : (char)(BIT(7) | (-1 * vdd->br_info.temperature));
	idx = ss_get_cmd_idx(pcmds, 0x09, 0xB1);
	pcmds->cmds[idx].ss_txbuf[1] = tset;

	/* WRDISBV */
	idx = ss_get_cmd_idx(pcmds, 0x00, 0x51);
	pcmds->cmds[idx].ss_txbuf[1] = (vdd->br_info.common_br.gm2_wrdisbv & 0xFF00) >> 8;
	pcmds->cmds[idx].ss_txbuf[2] = vdd->br_info.common_br.gm2_wrdisbv & 0xFF;

	/* smooth */
	if (vrr->cur_refresh_rate == 48 || vrr->cur_refresh_rate == 96)
		dim_off = true;
	else
		dim_off = false;

	smooth_idx = ss_get_cmd_idx(pcmds, 0x0D, 0x66);
	pcmds->cmds[smooth_idx].ss_txbuf[1] = dim_off ? 0x20 : 0x60;

	smooth_idx = ss_get_cmd_idx(pcmds, 0x0E, 0x66);
	pcmds->cmds[smooth_idx].ss_txbuf[1] = vdd->display_on ? (dim_off ? 0x01 : 0x18) : 0x01;

	smooth_idx = ss_get_cmd_idx(pcmds, 0x00, 0x53);
	pcmds->cmds[smooth_idx].ss_txbuf[1] = dim_off ? 0x20 : 0x28;

	LCD_INFO(vdd, "cd_idx: %d, cd_level: %d, WRDISBV: %x %x, fps : %d, smooth : %x\n",
				vdd->br_info.common_br.cd_idx, vdd->br_info.common_br.cd_level,
				pcmds->cmds[idx].ss_txbuf[1], pcmds->cmds[idx].ss_txbuf[2],
				vrr->cur_refresh_rate,
				pcmds->cmds[smooth_idx].ss_txbuf[1]);
#if 0
	/* IRC mode*/
	idx = ss_get_cmd_idx(pcmds, 0x156, 0x63);
	if (idx > 0) {
		if (irc_mode < IRC_MAX_MODE) {
			memcpy(&pcmds->cmds[idx].ss_txbuf[2], IRC_MEDERATO_VAL1[irc_mode], sizeof(IRC_MEDERATO_VAL1[0]) / sizeof(u8));
			memcpy(&pcmds->cmds[idx].ss_txbuf[14], IRC_MEDERATO_VAL2[irc_mode], sizeof(IRC_MEDERATO_VAL2[0]) / sizeof(u8));
		} else {
			LCD_ERR(vdd, "irc_mode is abnormal - %d\n", irc_mode);
		}
	}
#endif
	return pcmds;
}

static struct dsi_panel_cmd_set * ss_brightness_gamma_mode2_hbm(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	int idx = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, ": Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_HBM);

	idx = ss_get_cmd_idx(pcmds, 0x00, 0x51);
	pcmds->cmds[idx].ss_txbuf[1] = (vdd->br_info.common_br.gm2_wrdisbv & 0xFF00) >> 8;
	pcmds->cmds[idx].ss_txbuf[2] = vdd->br_info.common_br.gm2_wrdisbv & 0xFF;

	LCD_INFO(vdd, "cd_idx: %d, cd_level: %d, WRDISBV: %x %x\n",
		vdd->br_info.common_br.cd_idx,
		vdd->br_info.common_br.cd_level,
		pcmds->cmds[idx].ss_txbuf[1],
		pcmds->cmds[idx].ss_txbuf[2]);

#if 0
	/* IRC mode*/
	idx = ss_get_cmd_idx(pcmds, 0x156, 0x63);
	if (idx > 0) {
		if (irc_mode < IRC_MAX_MODE) {
			memcpy(&pcmds->cmds[idx].ss_txbuf[2], IRC_MEDERATO_VAL1[irc_mode], sizeof(IRC_MEDERATO_VAL1[0]) / sizeof(u8));
			memcpy(&pcmds->cmds[idx].ss_txbuf[14], IRC_MEDERATO_VAL2[irc_mode], sizeof(IRC_MEDERATO_VAL2[0]) / sizeof(u8));
		} else {
			LCD_ERR("irc_mode is abnormal - %d\n", irc_mode);
		}
	}
#endif

	return pcmds;
}

#if 0
/* flash map
 * F0000 ~ F0053: 2 mode * 3 temperature * 14 vbias cmd = 84 bytes
 * F0054 ~ F0055: checksum = 2 bytes
 * F0056: 1 byte
 * F0057 ~ F0058: NS/HS setting = 2 bytes
 */
static int ss_gm2_ddi_flash_prepare_revA(struct samsung_display_driver_data *vdd)
{
	u8 checksum_buf[2];
	int checksum_tot_flash;
	int checksum_tot_cal = 0;

	struct flash_gm2 *gm2_table = &vdd->br_info.gm2_table;
	u8 *ddi_flash_raw_buf;
	int ddi_flash_raw_len = gm2_table->ddi_flash_raw_len;
	int start_addr = gm2_table->ddi_flash_start_addr;

	u8 *vbias_data; /* 2 modes (NS/HS) * 3 temperature modes * 22 bytes cmd = 132 */

	int mode;
	int i;
	char read_buf = 0;
	int addr, loop_cnt;

	ddi_flash_raw_buf = gm2_table->ddi_flash_raw_buf;
	vbias_data = gm2_table->vbias_data;

	LCD_INFO(vdd, "ddi_flash_raw_len: %d\n", ddi_flash_raw_len);

	ss_send_cmd(vdd, TX_FLASH_GAMMA_PRE1);

	for (loop_cnt = 0, addr = start_addr; addr < start_addr + ddi_flash_raw_len ; addr++, loop_cnt++) {
		read_buf = flash_read_one_byte(vdd, addr);
		ddi_flash_raw_buf[loop_cnt] = read_buf;
	}

	ss_send_cmd(vdd, TX_FLASH_GAMMA_POST);

	for (i = 0; i < ddi_flash_raw_len; i++)
		LCD_INFO(vdd, "ddi_flash_raw_buf[%d]: %X\n", i, ddi_flash_raw_buf[i]);

	/* save vbias data to vbias & vint buffer according to panel spec. */
	for (mode = VBIAS_MODE_NS_WARM; mode < VBIAS_MODE_MAX; mode++) {
		u8 *vbias_one_mode = ss_get_vbias_data(gm2_table, mode);

		memcpy(vbias_one_mode, ddi_flash_raw_buf + (mode * 14), 14);
		memset(vbias_one_mode + 14, 0, 7);
	}

	/* flash: F0054 ~ F0055: off_gm2_flash_checksum = 0x56 -2 0x54 = 84 */
	memcpy(checksum_buf, ddi_flash_raw_buf + gm2_table->off_gm2_flash_checksum - 2, 2);

	checksum_tot_flash = (checksum_buf[0] << 8) | checksum_buf[1];

	/* 16bit sum chec: F0000 ~ F0053 ff_gm2_flash_checksum = 0x56, -2 54*/
	for (i = 0; i < gm2_table->off_gm2_flash_checksum - 2; i++) {
		checksum_tot_cal += ddi_flash_raw_buf[i];
		LCD_INFO(vdd, "ddi_flash_raw_buf[%d] = 0x%x\n", i, ddi_flash_raw_buf[i]);
	}

	checksum_tot_cal &= ERASED_MMC_16BIT;

	LCD_INFO(vdd, "checksum: %08X %08X, one mode comp: %s\n",
			checksum_tot_flash,
			checksum_tot_cal,
			"not SUPPORT");


	gm2_table->is_flash_checksum_ok = 0;
	LCD_ERR(vdd, "ddi flash test: FAIL..\n");
	if (checksum_tot_flash != checksum_tot_cal)
		LCD_ERR(vdd, "total flash checksum err: flash: %X, cal: %X\n",
				checksum_tot_flash, checksum_tot_cal);
	else
		LCD_ERR(vdd, "total flash checksum SUCCESS\n");

	LCD_ERR(vdd, "one vbias mode err, not SUPPORT (under REVD)\n");

	gm2_table->checksum_tot_flash = checksum_tot_flash;
	gm2_table->checksum_tot_cal = checksum_tot_cal;
	gm2_table->checksum_one_mode_mtp = 0xDE;
	gm2_table->checksum_one_mode_flash = 0xAD;

	gm2_table->is_diff_one_vbias_mode = 0xAD;

	return 0;
}

static int ss_gm2_ddi_flash_prepare_revD(struct samsung_display_driver_data *vdd)
{
	u8 checksum_buf[2];
	int checksum_tot_flash;
	int checksum_tot_cal = 0;
	int checksum_one_mode_flash;
	int is_diff_one_vbias_mode;

	struct flash_gm2 *gm2_table = &vdd->br_info.gm2_table;
	u8 *ddi_flash_raw_buf;
	int ddi_flash_raw_len = gm2_table->ddi_flash_raw_len;
	int start_addr = gm2_table->ddi_flash_start_addr;

	u8 *vbias_data; /* 2 modes (NS/HS) * 3 temperature modes * 22 bytes cmd = 132 */

	u8 *mtp_one_vbias_mode; /* 22 bytes, F4h 42nd ~ 55th, and 63rd TBD_TOP */
	u8 *flash_one_vbias_mode; /* NS and T > 0 mode, 22 bytes */
	int len_one_vbias_mode = gm2_table->len_one_vbias_mode; // 22 bytes TBD_TOP

	int mode;
	int i;
	char read_buf = 0;
	int addr, loop_cnt;

	ddi_flash_raw_buf = gm2_table->ddi_flash_raw_buf;
	vbias_data = gm2_table->vbias_data;
	mtp_one_vbias_mode = gm2_table->mtp_one_vbias_mode;

	LCD_INFO(vdd, "ddi_flash_raw_len: %d\n", ddi_flash_raw_len);

	ss_send_cmd(vdd, TX_FLASH_GAMMA_PRE1);

	for (loop_cnt = 0, addr = start_addr; addr < start_addr + ddi_flash_raw_len ; addr++, loop_cnt++) {
		read_buf = flash_read_one_byte(vdd, addr);
		ddi_flash_raw_buf[loop_cnt] = read_buf;
	}

	ss_send_cmd(vdd, TX_FLASH_GAMMA_POST);

	for (i = 0; i < ddi_flash_raw_len; i++)
		LCD_DEBUG(vdd, "ddi_flash_raw_buf[%d]: %X\n", i, ddi_flash_raw_buf[i]);

	/* save vbias data to vbias & vint buffer according to panel spec. */
	for (mode = VBIAS_MODE_NS_WARM; mode < VBIAS_MODE_MAX; mode++) {
		u8 *vbias_one_mode = ss_get_vbias_data(gm2_table, mode);

		memcpy(vbias_one_mode, ddi_flash_raw_buf + (mode * 14), 14);
		memset(vbias_one_mode + 14, 0, 7);
		if (i <= VBIAS_MODE_NS_COLD) /* NS: flash: F0054 ddi_flash_raw_len = 0x59*/
			vbias_one_mode[21] = ddi_flash_raw_buf[ddi_flash_raw_len - 5];
		else 	/* HS: flash: F0055 */
			vbias_one_mode[21] = ddi_flash_raw_buf[ddi_flash_raw_len - 4];
	}

	/* flash: F0056 ~ F0057: off_gm2_flash_checksum = 0x56 = 86 */
	memcpy(checksum_buf, ddi_flash_raw_buf + gm2_table->off_gm2_flash_checksum, 2);

	checksum_tot_flash = (checksum_buf[0] << 8) | checksum_buf[1];

	/* 16bit sum chec: F0000 ~ F0053 */
	for (i = 0; i < gm2_table->off_gm2_flash_checksum; i++)
		checksum_tot_cal += ddi_flash_raw_buf[i];

	checksum_tot_cal &= ERASED_MMC_16BIT;

	/* checksum of one mode from flash (15bytes, NS and T > 0) */
	flash_one_vbias_mode = ss_get_vbias_data(gm2_table, VBIAS_MODE_HS_WARM);
	checksum_one_mode_flash = 0;
	for (i = 0; i < len_one_vbias_mode; i++) {
		checksum_one_mode_flash += flash_one_vbias_mode[i];
		LCD_DEBUG(vdd, "flash_one_vbias_mode[%d] = 0x%x\n", i, flash_one_vbias_mode[i]);
	}
	checksum_one_mode_flash &= ERASED_MMC_16BIT;

	/* configure and calculate checksum of one vbias mode from MTP.
	 * F4h 42nd ~ 55th, and 63rd, and calculate checksum
	 */
	memset(gm2_table->mtp_one_vbias_mode + 7, 0, 17); /* 48th~54th & 72th~79th */
	gm2_table->checksum_one_mode_mtp = 0;
	for (i = 0; i < gm2_table->len_mtp_one_vbias_mode; i++) {
		gm2_table->checksum_one_mode_mtp += gm2_table->mtp_one_vbias_mode[i];
		LCD_DEBUG(vdd, "gm2_table->mtp_one_vbias_mode[%d] = 0x%x\n", i, gm2_table->mtp_one_vbias_mode[i]);
	}
	gm2_table->checksum_one_mode_mtp &= ERASED_MMC_16BIT;

	/* compare one vbias mode data: FLASH Vs. MTP (22 bytes) */
	is_diff_one_vbias_mode = gm2_table->checksum_one_mode_mtp == checksum_one_mode_flash ? 0 : 1;

	LCD_INFO(vdd, "checksum: %08X %08X %08X %08X, one mode comp: %s\n",
			checksum_tot_flash,
			checksum_tot_cal,
			gm2_table->checksum_one_mode_mtp,
			checksum_one_mode_flash,
			is_diff_one_vbias_mode ? "NG" : "PASS");

	/* save flash test result */
	if (checksum_tot_flash == checksum_tot_cal && !is_diff_one_vbias_mode) {
		gm2_table->is_flash_checksum_ok = 1;
		LCD_INFO(vdd, "ddi flash test: OK\n");
	} else {
		gm2_table->is_flash_checksum_ok = 0;
		LCD_ERR(vdd, "ddi flash test: FAIL..\n");
		if (checksum_tot_flash != checksum_tot_cal)
			LCD_ERR(vdd, "total flash checksum err: flash: %X, cal: %X\n",
					checksum_tot_flash, checksum_tot_cal);

		if (is_diff_one_vbias_mode) {
			LCD_ERR(vdd, "one vbias mode err..\n");
			for (i = 0; i < len_one_vbias_mode; i++)
				LCD_DEBUG(vdd, "  [%d] %X - %X\n", i, mtp_one_vbias_mode[i], flash_one_vbias_mode[i]);
		}
	}

	gm2_table->checksum_tot_flash = checksum_tot_flash;
	gm2_table->checksum_tot_cal = checksum_tot_cal;
	gm2_table->checksum_one_mode_flash = checksum_one_mode_flash;
	gm2_table->is_diff_one_vbias_mode = is_diff_one_vbias_mode;

	return 0;
}
static int ss_gm2_ddi_flash_prepare(struct samsung_display_driver_data *vdd)
{

	if (vdd->panel_revision < 3)
		ss_gm2_ddi_flash_prepare_revA(vdd);
	else /*from 0x91XX04*/
		ss_gm2_ddi_flash_prepare_revD(vdd);

	return 0;
}
#endif

static void ss_read_flash(struct samsung_display_driver_data *vdd, u32 raddr, u32 rsize, u8 *rbuf)
{
	char showbuf[256];
	int i, pos = 0;
	u32 fsize = 0xC00000;

	if (rsize > 0x100) {
		LCD_ERR(vdd, "rsize(%x) is not supported..\n", rsize);
		return;
	}

	ss_send_cmd(vdd, TX_POC_ENABLE);
	ss_send_cmd(vdd, TX_FLASH_GAMMA_PRE1);

	fsize |= rsize;
	flash_read_bytes(vdd, raddr, fsize, rsize, rbuf);

	ss_send_cmd(vdd, TX_FLASH_GAMMA_POST);
	ss_send_cmd(vdd, TX_POC_DISABLE);

	for (i = 0; i < rsize; i++)
		pos += snprintf(showbuf + pos, 256 - pos, "%02x ", rbuf[i]);
	LCD_INFO(vdd, "buf : %s\n", showbuf);

	return;
}

#define FLASH_TEST_MODE_NUM     (1)
static int ss_test_ddi_flash_check(struct samsung_display_driver_data *vdd, char *buf)
{
	int len = 0;
	int ret = 0;
	char FLASH_LOADING[1];

	ss_panel_data_read(vdd, RX_FLASH_LOADING_CHECK, FLASH_LOADING, LEVEL1_KEY);

	if ((int)FLASH_LOADING[0] == 0)
		ret = 1;

	len += sprintf(buf + len, "%d\n", FLASH_TEST_MODE_NUM);
	len += sprintf(buf + len, "%d %02x000000\n",
			ret,
			(int)FLASH_LOADING[0]);

	return len;
}

enum VRR_CMD_RR {
	/* 1Hz is PSR mode in LPM (AOD) mode, 10Hz is PSR mode in 120HS mode */
	VRR_48NS = 0,
	VRR_60NS,
	VRR_48HS,
	VRR_60HS,
	VRR_96HS,
	VRR_120HS,
	VRR_MAX
};

#if 0
static u8 cmd_lfd_set[VRR_MAX][23] = {
	{0x01, 0x00, 0x00, 0x80, 0x00, 0x02, 0x40, 0x00,
		0x01, 0x70, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x14},/*VRR_48HS*/
	{0x01, 0x00, 0x00, 0x80, 0x00, 0x02, 0x40, 0x00,
		0x01, 0x70, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x14},/*VRR_60HS*/
	{0x01, 0x00, 0x00, 0x80, 0x00, 0x01, 0x40, 0x00,
		0x01, 0x70, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x14},/*VRR_96HS*/
	{0x01, 0x00, 0x00, 0x80, 0x00, 0x01, 0x40, 0x00,
		0x01, 0x70, 0x00, 0x00,	0x00, 0x0B, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x14},/*VRR_120HS*/
};

static u8 cmd_aor_set[VRR_MAX][24] = {
	{0x08, 0xD4, 0x08, 0xD4, 0x08, 0xD4, 0x08, 0xD4,
		0x08, 0xD4, 0x08, 0xD4, 0x08, 0xD4, 0x08, 0xD4,
		0x08, 0xD4, 0x08, 0x20, 0x08, 0x20, 0x01, 0x6A},/*VRR_48HS*/
	{0x07, 0x10, 0x07, 0x10, 0x07, 0x10, 0x07, 0x10,
		0x07, 0x10, 0x07, 0x10, 0x07, 0x10, 0x07, 0x10,
		0x07, 0x10, 0x06, 0x80, 0x06, 0x80, 0x01, 0x22},/*VRR_60HS*/
	{0x08, 0xD4, 0x08, 0xD4, 0x08, 0xD4, 0x08, 0xD4,
		0x08, 0xD4, 0x08, 0xD4, 0x08, 0xD4, 0x08, 0xD4,
		0x08, 0xD4, 0x08, 0x20, 0x08, 0x20, 0x01, 0x6A},/*VRR_96HS*/
	{0x07, 0x10, 0x07, 0x10, 0x07, 0x10, 0x07, 0x10,
		0x07, 0x10, 0x07, 0x10, 0x07, 0x10, 0x07, 0x10,
		0x07, 0x10, 0x06, 0x80, 0x06, 0x80, 0x01, 0x22},/*VRR_120HS*/
};

static u8 cmd_vfp_change[VRR_MAX][2] = {
	{0x01, 0xCC},/*VRR_48HS*/
	{0x00, 0x08},/*VRR_60HS*/
	{0x01, 0xCC},/*VRR_96HS*/
	{0x00, 0x08},/*VRR_120HS*/
};

static u8 cmd_hw_te_mod[VRR_MAX][1] = {
	{0x11}, // 48HS, org TE is 96hz -> 48hz
	{0x11}, // 60HS, org TE is 120hz -> 60hz
	{0x01}, // 96HS, org TE is 96hz
	{0x01}, // 120HS, org TE is 120hz
};
#endif

static enum VRR_CMD_RR ss_get_vrr_id(struct samsung_display_driver_data *vdd)
{
	enum VRR_CMD_RR vrr_id;
	int cur_rr = vdd->vrr.cur_refresh_rate;
	int cur_hs = vdd->vrr.cur_sot_hs_mode;

	switch (cur_rr) {
	case 48:
		vrr_id = (cur_hs) ? VRR_48HS : VRR_48NS ;
		break;
	case 60:
		vrr_id = (cur_hs) ? VRR_60HS : VRR_60NS ;
		break;
	case 96:
		vrr_id = VRR_96HS;
		break;
	case 120:
		vrr_id = VRR_120HS;
		break;
	default:
		LCD_ERR(vdd, "invalid refresh rate (%d, %d), set default 120HS..\n", cur_rr, cur_hs);
		vrr_id = VRR_120HS;
		break;
	}

	return vrr_id;
}

static int ss_update_base_lfd_val(struct vrr_info *vrr,
			enum LFD_SCOPE_ID scope, struct lfd_base_str *lfd_base)
{
	u32 base_rr, max_div_def, min_div_def, min_div_lowest;
	enum VRR_CMD_RR vrr_id;
	struct samsung_display_driver_data *vdd =
		container_of(vrr, struct samsung_display_driver_data, vrr);

	if (scope == LFD_SCOPE_LPM) {
		base_rr = 30;
		max_div_def = 1;
		min_div_def = 30;
		min_div_lowest = 30;
		goto done;
	}

	vrr_id = ss_get_vrr_id(vdd);

	switch (vrr_id) {
	case VRR_48NS:
		base_rr = 48;
		max_div_def = min_div_def = min_div_lowest = 1; // 48hz
		break;
	case VRR_48HS:
		base_rr = 96;
		max_div_def = 2; // 48hz
		min_div_def = 8; // 12hz
		min_div_lowest = 96; // 1hz
		break;
	case VRR_60NS:
		base_rr = 60;
		max_div_def = 1; // 60hz
		min_div_def = min_div_lowest = 2; // 30hz
		break;
	case VRR_60HS:
		base_rr = 120;
		max_div_def = 2; // 60hz
		min_div_def = 12; // 10hz
		min_div_lowest = 120; // 1hz
		break;
	case VRR_96HS:
		base_rr = 96;
		max_div_def = 1; // 96hz
		min_div_def = 8; // 12hz
		min_div_lowest = 96; // 1hz
		break;
	case VRR_120HS:
		base_rr = 120;
		max_div_def = 1; // 120hz
		min_div_def = 12; // 10hz
		min_div_lowest = 120; // 1hz
		break;
	default:
		LCD_ERR(vdd, "invalid vrr_id\n");
		base_rr = 120;
		max_div_def = 1; // 120hz
		min_div_def = 12; // 10hz
		min_div_lowest = 120; // 1hz
		break;
	}

done:
	lfd_base->base_rr = base_rr;
	lfd_base->max_div_def = max_div_def;
	lfd_base->min_div_def = min_div_def;
	lfd_base->min_div_lowest = min_div_lowest;

	vrr->lfd.base_rr = base_rr;

	LCD_DEBUG(vdd, "LFD(%s): base_rr: %uhz, def: %uhz(%u)~%uhz(%u), lowest: %uhz(%u)\n",
			lfd_scope_name[scope],
			base_rr,
			DIV_ROUND_UP(base_rr, min_div_def), min_div_def,
			DIV_ROUND_UP(base_rr, max_div_def), max_div_def,
			DIV_ROUND_UP(base_rr, min_div_lowest), min_div_lowest);

	return 0;
}


/* 1. LFD min freq. and frame insertion (BDh 0x06th): BDh 0x6 offset, for LFD control
 * divider: 2nd param: (divder - 1)
 * frame insertion: 5th param: repeat count of current fps
 * 3rd param[7:4] : repeat count of half fps
 * 3rd param[3:0] : repeat count of quad fps
 *
 * ---------------------------------------------------------
 *     LFD_MIN  120HS~60HS  48HS~11HS  1HS       60NS~30NS
 * LFD_MAX
 * ---------------------------------------------------------
 * 120HS~96HS   10 00 01    14 00 01   EF 00 01
 *   60HS~1HS   10 00 01    00 00 02   EF 00 01
 *  60NS~30NS                                    00 00 01
 * ---------------------------------------------------------
 */
static int ss_get_frame_insert_cmd(struct samsung_display_driver_data *vdd,
	u8 *out_cmd, u32 base_rr, u32 min_div, u32 max_div, bool cur_hs)
{
	u32 min_freq = DIV_ROUND_UP(base_rr, min_div);
	u32 max_freq = DIV_ROUND_UP(base_rr, max_div);

	out_cmd[1] = 0;
	if (cur_hs) {
		if (min_freq > 48) {
			/* LFD_MIN=120HS~60HS: 10 00 01 */
			out_cmd[0] = 0x10;
			out_cmd[2] = 0x01;
		} else if (min_freq == 1) {
			/* LFD_MIN=1HS: EF 00 01 */
			out_cmd[0] = 0xEF;
			out_cmd[2] = 0x01;
		} else if (max_freq > 60) {
			/* LFD_MIN=48HS~11HS, LFD_MAX=120HS~96HS: 14 00 01 */
			out_cmd[0] = 0x14;
			out_cmd[2] = 0x01;
		} else {
			/* LFD_MIN=48HS~11HS, LFD_MAX=60HS~1HS: 00 00 02 */
			out_cmd[0] = 0x00;
			out_cmd[2] = 0x02;
		}
	} else {
		/* NS mode: 00 00 01 */
		out_cmd[0] = 0x00;
		out_cmd[2] = 0x01;
	}
	LCD_DEBUG(vdd, "frame insert: %02X %02X %02X (LFD %uhz~%uhz, %s)\n",
			out_cmd[0], out_cmd[1], out_cmd[2], min_freq, max_freq,
			cur_hs ? "HS" : "NS");

	return 0;
}

static u8 AOR_96_48HZ[6] = {0x01, 0x7C, 0x01, 0x7C, 0x01, 0x7C};
static u8 AOR_120HZ[6] = {0x01, 0x30, 0x01, 0x30, 0x01, 0x30};
static u8 AOR_revA[6] = {0x01, 0x22, 0x01, 0x22, 0x01, 0x22};

static u8 VFP_96_48HZ_revA[2] = {0x01, 0xCC};
static u8 VFP_96_48HZ[2] = {0x02, 0xBC};

static u8 VFP_120HZ_revA[2] = {0x00, 0x08};
static u8 VFP_120HZ[2] = {0x00, 0xC8};

static struct dsi_panel_cmd_set *ss_vrr(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel *panel = GET_DSI_PANEL(vdd);
	struct dsi_panel_cmd_set  *vrr_cmds = ss_get_cmds(vdd, TX_VRR);
	bool is_hbm = vdd->br_info.common_br.bl_level > MAX_BL_PF_LEVEL ? true : false;
	struct vrr_info *vrr = &vdd->vrr;
	int cur_rr;
	bool cur_hs;
	enum VRR_CMD_RR vrr_id;
	u32 min_div, max_div;
	enum LFD_SCOPE_ID scope;
	u8 cmd_frame_insert[3];
	int idx;

	if (SS_IS_CMDS_NULL(vrr_cmds)) {
		LCD_INFO(vdd, "no vrr cmds\n");
		return NULL;
	}

	if (panel && panel->cur_mode) {
		LCD_INFO(vdd, "VRR: cur_mode: %dx%d@%d%s, is_hbm: %d rev: %d\n",
				panel->cur_mode->timing.h_active,
				panel->cur_mode->timing.v_active,
				panel->cur_mode->timing.refresh_rate,
				panel->cur_mode->timing.sot_hs_mode ? "HS" : "NM",
				is_hbm, vdd->panel_revision);

		if (panel->cur_mode->timing.refresh_rate != vdd->vrr.adjusted_refresh_rate ||
				panel->cur_mode->timing.sot_hs_mode != vdd->vrr.adjusted_sot_hs_mode)
			LCD_ERR(vdd, "VRR: unmatched RR mode (%dhz%s / %dhz%s)\n",
					panel->cur_mode->timing.refresh_rate,
					panel->cur_mode->timing.sot_hs_mode ? "HS" : "NM",
					vdd->vrr.adjusted_refresh_rate,
					vdd->vrr.adjusted_sot_hs_mode ? "HS" : "NM");
	}

	cur_rr = vrr->cur_refresh_rate;
	cur_hs = vrr->cur_sot_hs_mode;

	vrr_id = ss_get_vrr_id(vdd);

	scope = LFD_SCOPE_NORMAL;
	if (ss_get_lfd_div(vdd, scope, &min_div, &max_div)) {
		LCD_ERR(vdd, "fail to get LFD divider.. set default 1..\n");
		max_div = min_div = 1;
	}
	vrr->lfd.min_div = min_div;
	vrr->lfd.max_div = max_div;

	/* AOR Setting */
	idx = ss_get_cmd_idx(vrr_cmds, 0xAE, 0x68);
	if (vdd->panel_revision + 'A' < 'B') {
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], AOR_revA, sizeof(AOR_revA) / sizeof(u8));
	} else {
		if (cur_rr == 96 || cur_rr == 48)
			memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], AOR_96_48HZ, sizeof(AOR_96_48HZ) / sizeof(u8));
		else
			memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], AOR_120HZ, sizeof(AOR_120HZ) / sizeof(u8));
	}
#if 0	// in comp function
	/* Manual AID Enable */
	idx = ss_get_cmd_idx(vrr_cmds, 0x37, 0x68);
	if (cur_rr == 48 || cur_rr == 96)
		vrr_cmds->cmds[idx].ss_txbuf[1] = 0x01;
	else
		vrr_cmds->cmds[idx].ss_txbuf[1] = 0x00;
#endif

	/* LFD Setting */
	idx = ss_get_cmd_idx(vrr_cmds, 0x06, 0xBD);
	if (cur_rr == 48) {
		vrr_cmds->cmds[idx].ss_txbuf[1] = 0x00;
		vrr_cmds->cmds[idx].ss_txbuf[2] = 0x01;
	} else if (cur_rr == 96) {
		vrr_cmds->cmds[idx].ss_txbuf[1] = 0x00;
		vrr_cmds->cmds[idx].ss_txbuf[2] = 0x00;
	} else {
		vrr_cmds->cmds[idx].ss_txbuf[2] = min_div - 1;
		ss_get_frame_insert_cmd(vdd, cmd_frame_insert, vrr->lfd.base_rr, min_div, max_div, cur_hs);
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[3], cmd_frame_insert, sizeof(cmd_frame_insert));
	}

	/* VFP Setting */
	idx = ss_get_cmd_idx(vrr_cmds, 0x06, 0xF2);

	if (vdd->panel_revision + 'A' < 'B') {
		if (cur_rr == 96 || cur_rr == 48)
			memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], VFP_96_48HZ_revA, sizeof(VFP_96_48HZ_revA) / sizeof(u8));
		else
			memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], VFP_120HZ_revA, sizeof(VFP_120HZ_revA) / sizeof(u8));
	} else {
		if (cur_rr == 96 || cur_rr == 48)
			memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], VFP_96_48HZ, sizeof(VFP_96_48HZ) / sizeof(u8));
		else
			memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], VFP_120HZ, sizeof(VFP_120HZ) / sizeof(u8));
	}

	idx = ss_get_cmd_idx(vrr_cmds, 0x0D, 0xF2);

	if (vdd->panel_revision + 'A' < 'B') {
		if (cur_rr == 96 || cur_rr == 48)
			memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], VFP_96_48HZ_revA, sizeof(VFP_96_48HZ_revA) / sizeof(u8));
		else
			memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], VFP_120HZ_revA, sizeof(VFP_120HZ_revA) / sizeof(u8));
	} else {
		if (cur_rr == 96 || cur_rr == 48)
			memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], VFP_96_48HZ, sizeof(VFP_96_48HZ) / sizeof(u8));
		else
			memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], VFP_120HZ, sizeof(VFP_120HZ) / sizeof(u8));
	}

	idx = ss_get_cmd_idx(vrr_cmds, 0x00, 0xB9);
	if (cur_rr == 60 || cur_rr == 48) {	// TE_FRAME_SEL enable
		vrr_cmds->cmds[idx].ss_txbuf[2] = 0x01;
	} else {	// TE_FRAME_SEL disable
		vrr_cmds->cmds[idx].ss_txbuf[2] = 0x00;
	}

	/* FREQ Setting */
	idx = ss_get_cmd_idx(vrr_cmds, 0x00, 0x60);
	vrr_cmds->cmds[idx].ss_txbuf[2] = max_div - 1;

	LCD_INFO(vdd, "VRR: (cur: %d%s, adj: %d%s, te_mod: (%d %d), LFD: %uhz(%u)~%uhz(%u)\n",
			cur_rr,
			cur_hs ? "HS" : "NM",
			vrr->adjusted_refresh_rate,
			vrr->adjusted_sot_hs_mode ? "HS" : "NM",
			vrr->te_mod_on, vrr->te_mod_divider,
			DIV_ROUND_UP(vrr->lfd.base_rr, min_div), min_div,
			DIV_ROUND_UP(vrr->lfd.base_rr, max_div), max_div);

	return vrr_cmds;
}

#if 0
/* TE moudulation (reports vsync as 60hz even TE is 120hz, in 60HS mode)
 * Some HOP display, like C2 XA1 UB, supports self scan function.
 * In this case, if it sets to 60HS mode, panel fetches pixel data from GRAM in 60hz,
 * but DDI generates TE as 120hz.
 * This architecture is for preventing brightness flicker in VRR change and keep fast touch responsness.
 *
 * In 60HS mode, TE period is 120hz, but display driver reports vsync to graphics HAL as 60hz.
 * So, do TE modulation in 60HS mode, reports vsync as 60hz.
 * In 30NS mode, TE is 60hz, but reports vsync as 30hz.
 */
static int ss_vrr_set_te_mode(struct samsung_display_driver_data *vdd, int cur_rr, int cur_hs)
{
	if (cur_hs) {
		if (cur_rr == 60 || cur_rr == 48) {
			vdd->vrr.te_mod_divider = 2; /* 120/60  or 96/48 */
			vdd->vrr.te_mod_cnt = vdd->vrr.te_mod_divider;
			vdd->vrr.te_mod_on = 1;
			LCD_INFO(vdd, "%dHS: enable te_mode: div = %d\n", cur_rr, vdd->vrr.te_mod_divider);

			return 0;
		}
	}

	LCD_INFO(vdd, "disable te_mod\n");
	vdd->vrr.te_mod_divider = 0;
	vdd->vrr.te_mod_cnt = vdd->vrr.te_mod_divider;
	vdd->vrr.te_mod_on = 0;

	return 0;
}
#endif

static int ss_ddi_id_read(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *pcmds;
	char *read_buf;
	int i, len = 0;
	u8 temp[20];
	int ddi_id_len;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	pcmds = ss_get_cmds(vdd, RX_DDI_ID);

	/* Read mtp (D6h 1~5th) for CHIP ID */
	if (pcmds->count) {
		ddi_id_len = pcmds->cmds[0].msg.rx_len;
		read_buf = kzalloc(ddi_id_len, GFP_KERNEL);
		if (!read_buf) {
			LCD_ERR(vdd, "fail to kzalloc for read_buf\n");
			return false;
		}

		ss_panel_data_read(vdd, RX_DDI_ID, read_buf, LEVEL1_KEY);

		for (i = 0; i < ddi_id_len; i++)
			len += sprintf(temp + len, "%02x", read_buf[i]);
		len += sprintf(temp + len, "\n");

		vdd->ddi_id_dsi = kzalloc(len, GFP_KERNEL);
		if (!vdd->ddi_id_dsi)
			LCD_ERR(vdd, "fail to kzalloc for ddi_id_dsi\n");
		else {
			vdd->ddi_id_len = len;
			strlcat(vdd->ddi_id_dsi, temp, len);
			LCD_INFO(vdd, "[%d] %s\n", vdd->ddi_id_len, vdd->ddi_id_dsi);
		}
	} else {
		LCD_ERR(vdd, "DSI%d no ddi_id_rx_cmds cmds", vdd->ndx);
		return false;
	}

	kfree(read_buf);

	return true;
}


#define COORDINATE_DATA_SIZE 6
#define MDNIE_SCR_WR_ADDR	0x32
#define RGB_INDEX_SIZE 4
#define COEFFICIENT_DATA_SIZE 8

#define F1(x, y) (((y << 10) - (((x << 10) * 56) / 55) - (102 << 10)) >> 10)
#define F2(x, y) (((y << 10) + (((x << 10) * 5) / 1) - (18483 << 10)) >> 10)

static char coordinate_data_1[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xfa, 0x00, 0xf9, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xfc, 0x00, 0xff, 0x00}, /* Tune_2 */
	{0xf8, 0x00, 0xf7, 0x00, 0xff, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfd, 0x00, 0xf9, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_5 */
	{0xf8, 0x00, 0xfa, 0x00, 0xff, 0x00}, /* Tune_6 */
	{0xfd, 0x00, 0xff, 0x00, 0xf8, 0x00}, /* Tune_7 */
	{0xfb, 0x00, 0xff, 0x00, 0xfc, 0x00}, /* Tune_8 */
	{0xf8, 0x00, 0xfd, 0x00, 0xff, 0x00}, /* Tune_9 */
};

static char coordinate_data_2[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xf8, 0x00, 0xf0, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xf9, 0x00, 0xf6, 0x00}, /* Tune_2 */
	{0xff, 0x00, 0xfb, 0x00, 0xfd, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfb, 0x00, 0xf0, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xfc, 0x00, 0xf6, 0x00}, /* Tune_5 */
	{0xff, 0x00, 0xff, 0x00, 0xfd, 0x00}, /* Tune_6 */
	{0xff, 0x00, 0xfe, 0x00, 0xf0, 0x00}, /* Tune_7 */
	{0xff, 0x00, 0xff, 0x00, 0xf6, 0x00}, /* Tune_8 */
	{0xfc, 0x00, 0xff, 0x00, 0xfa, 0x00}, /* Tune_9 */
};

static char (*coordinate_data[MAX_MODE])[COORDINATE_DATA_SIZE] = {
	coordinate_data_2,
	coordinate_data_2,
	coordinate_data_2,
	coordinate_data_1,
	coordinate_data_1,
	coordinate_data_1
};

static int rgb_index[][RGB_INDEX_SIZE] = {
	{ 5, 5, 5, 5 }, /* dummy */
	{ 5, 2, 6, 3 },
	{ 5, 2, 4, 1 },
	{ 5, 8, 4, 7 },
	{ 5, 8, 6, 9 },
};

static int coefficient[][COEFFICIENT_DATA_SIZE] = {
	{       0,        0,      0,      0,      0,       0,      0,      0 }, /* dummy */
	{  -52615,   -61905,  21249,  15603,  40775,   80902, -19651, -19618 },
	{ -212096,  -186041,  61987,  65143, -75083,  -27237,  16637,  15737 },
	{   69454,    77493, -27852, -19429, -93856, -133061,  37638,  35353 },
	{  192949,   174780, -56853, -60597,  57592,   13018, -11491, -10757 },
};

static int mdnie_coordinate_index(int x, int y)
{
	int tune_number = 0;

	if (F1(x, y) > 0) {
		if (F2(x, y) > 0) {
			tune_number = 1;
		} else {
			tune_number = 2;
		}
	} else {
		if (F2(x, y) > 0) {
			tune_number = 4;
		} else {
			tune_number = 3;
		}
	}

	return tune_number;
}

static int mdnie_coordinate_x(int x, int y, int index)
{
	int result = 0;

	result = (coefficient[index][0] * x) + (coefficient[index][1] * y) + (((coefficient[index][2] * x + 512) >> 10) * y) + (coefficient[index][3] * 10000);

	result = (result + 512) >> 10;

	if (result < 0)
		result = 0;
	if (result > 1024)
		result = 1024;

	return result;
}

static int mdnie_coordinate_y(int x, int y, int index)
{
	int result = 0;

	result = (coefficient[index][4] * x) + (coefficient[index][5] * y) + (((coefficient[index][6] * x + 512) >> 10) * y) + (coefficient[index][7] * 10000);

	result = (result + 512) >> 10;

	if (result < 0)
		result = 0;
	if (result > 1024)
		result = 1024;

	return result;
}

static int ss_module_info_read(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *pcmds;
	char *buf;
	int year, month, day;
	int hour, min;
	int x, y;
	int mdnie_tune_index = 0;
	int ret;
	char temp[50];
	int buf_len, len = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	pcmds = ss_get_cmds(vdd, RX_MODULE_INFO);

	if (pcmds->count) {
		buf_len = pcmds->cmds[0].msg.rx_len;

		buf = kzalloc(buf_len, GFP_KERNEL);
		if (!buf) {
			LCD_ERR(vdd, "fail to kzalloc for buf\n");
			return false;
		}

		ret = ss_panel_data_read(vdd, RX_MODULE_INFO, buf, LEVEL1_KEY);
		if (ret) {
			LCD_ERR(vdd, "fail to read module ID, ret: %d", ret);
			kfree(buf);
			return false;
		}

		/* Manufacture Date */

		year = buf[4] & 0xf0;
		year >>= 4;
		year += 2011; // 0 = 2011 year
		month = buf[4] & 0x0f;
		day = buf[5] & 0x1f;
		hour = buf[6] & 0x0f;
		min = buf[7] & 0x1f;

		vdd->manufacture_date_dsi = year * 10000 + month * 100 + day;
		vdd->manufacture_time_dsi = hour * 100 + min;

		LCD_INFO(vdd, "manufacture_date DSI%d = (%d%04d) - year(%d) month(%d) day(%d) hour(%d) min(%d)\n",
			vdd->ndx, vdd->manufacture_date_dsi, vdd->manufacture_time_dsi,
			year, month, day, hour, min);

		/* While Coordinates */

		vdd->mdnie.mdnie_x = buf[0] << 8 | buf[1];	/* X */
		vdd->mdnie.mdnie_y = buf[2] << 8 | buf[3];	/* Y */

		mdnie_tune_index = mdnie_coordinate_index(vdd->mdnie.mdnie_x, vdd->mdnie.mdnie_y);

		if (((vdd->mdnie.mdnie_x - 3050) * (vdd->mdnie.mdnie_x - 3050) + (vdd->mdnie.mdnie_y - 3210) * (vdd->mdnie.mdnie_y - 3210)) <= 225) {
			x = 0;
			y = 0;
		} else {
			x = mdnie_coordinate_x(vdd->mdnie.mdnie_x, vdd->mdnie.mdnie_y, mdnie_tune_index);
			y = mdnie_coordinate_y(vdd->mdnie.mdnie_x, vdd->mdnie.mdnie_y, mdnie_tune_index);
		}

		coordinate_tunning_calculate(vdd, x, y, coordinate_data,
				rgb_index[mdnie_tune_index],
				MDNIE_SCR_WR_ADDR, COORDINATE_DATA_SIZE);

		LCD_INFO(vdd, "X-%d Y-%d \n", vdd->mdnie.mdnie_x, vdd->mdnie.mdnie_y);

		/* CELL ID (manufacture date + white coordinates) */
		/* Manufacture Date */
		len += sprintf(temp + len, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
			buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10],
			buf[0], buf[1], buf[2], buf[3]);

		vdd->cell_id_dsi = kzalloc(len, GFP_KERNEL);
		if (!vdd->cell_id_dsi)
			LCD_ERR(vdd, "fail to kzalloc for cell_id_dsi\n");
		else {
			vdd->cell_id_len = len;
			strlcat(vdd->cell_id_dsi, temp, vdd->cell_id_len);
			LCD_INFO(vdd, "CELL ID : [%d] %s\n", vdd->cell_id_len, vdd->cell_id_dsi);
		}
	} else {
		LCD_ERR(vdd, "no module_info_rx_cmds cmds(%d)", vdd->panel_revision);
		return false;
	}

	kfree(buf);

	return true;
}


static int ss_elvss_read(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (C66h 0x17th) for grayspot */
	ss_panel_data_read(vdd, RX_ELVSS, vdd->br_info.common_br.elvss_value, LEVEL1_KEY);

	LCD_INFO(vdd, "elvss mtp : %0x\n", vdd->br_info.common_br.elvss_value[0]);

	return true;
}

static void ss_gray_spot(struct samsung_display_driver_data *vdd, int enable)
{
	struct dsi_panel_cmd_set *pcmds;
	int idx = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return;
	}

	pcmds = ss_get_cmds(vdd, TX_GRAY_SPOT_TEST_OFF);
	if (IS_ERR_OR_NULL(pcmds)) {
        LCD_ERR(vdd, "Invalid pcmds\n");
        return;
    }

	if (!enable) { /* grayspot off */
		/* restore ELVSS_CAL_OFFSET */
		idx = ss_get_cmd_idx(pcmds, 0x17, 0x66);
		pcmds->cmds[idx].ss_txbuf[1] = vdd->br_info.common_br.elvss_value[0];
	}
}

static int dsi_update_mdnie_data(struct samsung_display_driver_data *vdd)
{
	struct mdnie_lite_tune_data *mdnie_data;

	mdnie_data = kzalloc(sizeof(struct mdnie_lite_tune_data), GFP_KERNEL);
	if (!mdnie_data) {
		LCD_ERR(vdd, "fail to allocate mdnie_data memory\n");
		return -ENOMEM;
	}

	/* Update mdnie command */
	mdnie_data->DSI_COLOR_BLIND_MDNIE_1 = COLOR_BLIND_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_1 = RGB_SENSOR_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_2 = RGB_SENSOR_MDNIE_2;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_3 = RGB_SENSOR_MDNIE_3;
	mdnie_data->DSI_TRANS_DIMMING_MDNIE = RGB_SENSOR_MDNIE_3;
	mdnie_data->DSI_UI_DYNAMIC_MDNIE_2 = UI_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_UI_STANDARD_MDNIE_2 = UI_STANDARD_MDNIE_2;
	mdnie_data->DSI_UI_AUTO_MDNIE_2 = UI_AUTO_MDNIE_2;
	mdnie_data->DSI_VIDEO_DYNAMIC_MDNIE_2 = VIDEO_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_VIDEO_STANDARD_MDNIE_2 = VIDEO_STANDARD_MDNIE_2;
	mdnie_data->DSI_VIDEO_AUTO_MDNIE_2 = VIDEO_AUTO_MDNIE_2;
	mdnie_data->DSI_CAMERA_AUTO_MDNIE_2 = CAMERA_AUTO_MDNIE_2;
	mdnie_data->DSI_GALLERY_DYNAMIC_MDNIE_2 = GALLERY_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_GALLERY_STANDARD_MDNIE_2 = GALLERY_STANDARD_MDNIE_2;
	mdnie_data->DSI_GALLERY_AUTO_MDNIE_2 = GALLERY_AUTO_MDNIE_2;
	mdnie_data->DSI_BROWSER_DYNAMIC_MDNIE_2 = BROWSER_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_BROWSER_STANDARD_MDNIE_2 = BROWSER_STANDARD_MDNIE_2;
	mdnie_data->DSI_BROWSER_AUTO_MDNIE_2 = BROWSER_AUTO_MDNIE_2;
	mdnie_data->DSI_EBOOK_DYNAMIC_MDNIE_2 = EBOOK_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_EBOOK_STANDARD_MDNIE_2 = EBOOK_STANDARD_MDNIE_2;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE_2 = EBOOK_AUTO_MDNIE_2;
	mdnie_data->DSI_TDMB_DYNAMIC_MDNIE_2 = TDMB_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_TDMB_STANDARD_MDNIE_2 = TDMB_STANDARD_MDNIE_2;
	mdnie_data->DSI_TDMB_AUTO_MDNIE_2 = TDMB_AUTO_MDNIE_2;

	mdnie_data->DSI_BYPASS_MDNIE = BYPASS_MDNIE;
	mdnie_data->DSI_NEGATIVE_MDNIE = NEGATIVE_MDNIE;
	mdnie_data->DSI_COLOR_BLIND_MDNIE = COLOR_BLIND_MDNIE;
	mdnie_data->DSI_HBM_CE_MDNIE = HBM_CE_MDNIE;
	mdnie_data->DSI_HBM_CE_D65_MDNIE = HBM_CE_D65_MDNIE;
	mdnie_data->DSI_RGB_SENSOR_MDNIE = RGB_SENSOR_MDNIE;
	mdnie_data->DSI_UI_DYNAMIC_MDNIE = UI_DYNAMIC_MDNIE;
	mdnie_data->DSI_UI_STANDARD_MDNIE = UI_STANDARD_MDNIE;
	mdnie_data->DSI_UI_NATURAL_MDNIE = UI_NATURAL_MDNIE;
	mdnie_data->DSI_UI_AUTO_MDNIE = UI_AUTO_MDNIE;
	mdnie_data->DSI_VIDEO_DYNAMIC_MDNIE = VIDEO_DYNAMIC_MDNIE;
	mdnie_data->DSI_VIDEO_STANDARD_MDNIE = VIDEO_STANDARD_MDNIE;
	mdnie_data->DSI_VIDEO_NATURAL_MDNIE = VIDEO_NATURAL_MDNIE;
	mdnie_data->DSI_VIDEO_AUTO_MDNIE = VIDEO_AUTO_MDNIE;
	mdnie_data->DSI_CAMERA_AUTO_MDNIE = CAMERA_AUTO_MDNIE;
	mdnie_data->DSI_GALLERY_DYNAMIC_MDNIE = GALLERY_DYNAMIC_MDNIE;
	mdnie_data->DSI_GALLERY_STANDARD_MDNIE = GALLERY_STANDARD_MDNIE;
	mdnie_data->DSI_GALLERY_NATURAL_MDNIE = GALLERY_NATURAL_MDNIE;
	mdnie_data->DSI_GALLERY_AUTO_MDNIE = GALLERY_AUTO_MDNIE;
	mdnie_data->DSI_BROWSER_DYNAMIC_MDNIE = BROWSER_DYNAMIC_MDNIE;
	mdnie_data->DSI_BROWSER_STANDARD_MDNIE = BROWSER_STANDARD_MDNIE;
	mdnie_data->DSI_BROWSER_NATURAL_MDNIE = BROWSER_NATURAL_MDNIE;
	mdnie_data->DSI_BROWSER_AUTO_MDNIE = BROWSER_AUTO_MDNIE;
	mdnie_data->DSI_EBOOK_DYNAMIC_MDNIE = EBOOK_DYNAMIC_MDNIE;
	mdnie_data->DSI_EBOOK_STANDARD_MDNIE = EBOOK_STANDARD_MDNIE;
	mdnie_data->DSI_EBOOK_NATURAL_MDNIE = EBOOK_NATURAL_MDNIE;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE = EBOOK_AUTO_MDNIE;
	mdnie_data->DSI_EMAIL_AUTO_MDNIE = EMAIL_AUTO_MDNIE;
	mdnie_data->DSI_GAME_LOW_MDNIE = BYPASS_MDNIE;
	mdnie_data->DSI_GAME_MID_MDNIE = BYPASS_MDNIE;
	mdnie_data->DSI_GAME_HIGH_MDNIE = BYPASS_MDNIE;
	mdnie_data->DSI_TDMB_DYNAMIC_MDNIE = TDMB_DYNAMIC_MDNIE;
	mdnie_data->DSI_TDMB_STANDARD_MDNIE = TDMB_STANDARD_MDNIE;
	mdnie_data->DSI_TDMB_NATURAL_MDNIE = TDMB_NATURAL_MDNIE;
	mdnie_data->DSI_TDMB_AUTO_MDNIE = TDMB_AUTO_MDNIE;
	mdnie_data->DSI_GRAYSCALE_MDNIE = GRAYSCALE_MDNIE;
	mdnie_data->DSI_GRAYSCALE_NEGATIVE_MDNIE = GRAYSCALE_NEGATIVE_MDNIE;
	mdnie_data->DSI_CURTAIN = SCREEN_CURTAIN_MDNIE;
	mdnie_data->DSI_NIGHT_MODE_MDNIE = NIGHT_MODE_MDNIE;
	mdnie_data->DSI_NIGHT_MODE_MDNIE_SCR = NIGHT_MODE_MDNIE_1;
	mdnie_data->DSI_COLOR_LENS_MDNIE = COLOR_LENS_MDNIE;
	mdnie_data->DSI_COLOR_LENS_MDNIE_SCR = COLOR_LENS_MDNIE_1;
	mdnie_data->DSI_COLOR_BLIND_MDNIE_SCR = COLOR_BLIND_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_SCR = RGB_SENSOR_MDNIE_1;
	mdnie_data->DSI_AFC = DSI_AFC;
	mdnie_data->DSI_AFC_ON = DSI_AFC_ON;
	mdnie_data->DSI_AFC_OFF = DSI_AFC_OFF;

	mdnie_data->mdnie_tune_value_dsi = mdnie_tune_value_dsi;
	mdnie_data->hmt_color_temperature_tune_value_dsi = hmt_color_temperature_tune_value_dsi;

	mdnie_data->hdr_tune_value_dsi = hdr_tune_value_dsi;

	mdnie_data->light_notification_tune_value_dsi = light_notification_tune_value_dsi;

	/* Update MDNIE data related with size, offset or index */
	mdnie_data->dsi_bypass_mdnie_size = ARRAY_SIZE(BYPASS_MDNIE);
	mdnie_data->mdnie_color_blinde_cmd_offset = MDNIE_COLOR_BLINDE_CMD_OFFSET;
	mdnie_data->mdnie_step_index[MDNIE_STEP1] = MDNIE_STEP1_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP2] = MDNIE_STEP2_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP3] = MDNIE_STEP3_INDEX;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_RED_OFFSET] = ADDRESS_SCR_WHITE_RED;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_GREEN_OFFSET] = ADDRESS_SCR_WHITE_GREEN;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_BLUE_OFFSET] = ADDRESS_SCR_WHITE_BLUE;
	mdnie_data->dsi_rgb_sensor_mdnie_1_size = RGB_SENSOR_MDNIE_1_SIZE;
	mdnie_data->dsi_rgb_sensor_mdnie_2_size = RGB_SENSOR_MDNIE_2_SIZE;
	mdnie_data->dsi_rgb_sensor_mdnie_3_size = RGB_SENSOR_MDNIE_3_SIZE;

	mdnie_data->dsi_trans_dimming_data_index = MDNIE_TRANS_DIMMING_DATA_INDEX;

	mdnie_data->dsi_adjust_ldu_table = adjust_ldu_data;
	mdnie_data->dsi_max_adjust_ldu = 6;
	mdnie_data->dsi_night_mode_table = night_mode_data;
	mdnie_data->dsi_max_night_mode_index = 102;
	mdnie_data->dsi_color_lens_table = color_lens_data;
	mdnie_data->dsi_white_default_r = 0xff;
	mdnie_data->dsi_white_default_g = 0xff;
	mdnie_data->dsi_white_default_b = 0xff;
	mdnie_data->dsi_white_balanced_r = 0;
	mdnie_data->dsi_white_balanced_g = 0;
	mdnie_data->dsi_white_balanced_b = 0;
	mdnie_data->dsi_scr_step_index = MDNIE_STEP1_INDEX;
	mdnie_data->dsi_afc_size = 71;
	mdnie_data->dsi_afc_index = 56;

	vdd->mdnie.mdnie_data = mdnie_data;

	return 0;
}

static int ss_ecc_read(struct samsung_display_driver_data *vdd)
{
	u8 ecc = 0;
	int enable = 0;

	ss_panel_data_read(vdd, RX_GCT_ECC, &ecc, LEVEL1_KEY);
	LCD_INFO(vdd, "GCT ECC = 0x%02X\n", ecc);

	if (ecc == 0x00)
		enable = 1;
	else
		enable = 0;

	return enable;
}

#if 0
static int ss_gct_read(struct samsung_display_driver_data *vdd)
{
	u8 valid_checksum[4] = {0x1F, 0x1F, 0x1F, 0x1F};
	int res;

	return GCT_RES_CHECKSUM_PASS;

	if (!vdd->gct.is_support)
		return GCT_RES_CHECKSUM_NOT_SUPPORT;

	if (!vdd->gct.on)
		return GCT_RES_CHECKSUM_OFF;

	if (!memcmp(vdd->gct.checksum, valid_checksum, 4))
		res = GCT_RES_CHECKSUM_PASS;
	else
		res = GCT_RES_CHECKSUM_NG;

	return res;
}

static int ss_gct_write(struct samsung_display_driver_data *vdd)
{
	u8 *checksum;
	int i, idx;
	/* vddm set, 0x0: 1.0V, 0x04: 0.9V LV, 0x08: 1.1V HV */
	u8 vddm_set[MAX_VDDM] = {0x0, 0x04, 0x08};
	u8 vddm_set2[MAX_VDDM] = {0x0, 0x05, 0x25};
	int ret = 0;
	struct dsi_panel *panel = GET_DSI_PANEL(vdd);
	int wait_cnt = 1000; /* 1000 * 0.5ms = 500ms */
	struct dsi_panel_cmd_set *set;

	LCD_INFO(vdd, "+\n");

	ss_set_test_mode_state(vdd, PANEL_TEST_GCT);

	set = ss_get_cmds(vdd, TX_GCT_ENTER);
	if (SS_IS_CMDS_NULL(set)) {
		LCD_ERR(vdd, "No cmds for TX_GCT_ENTER..\n");
		return ret;
	}

	vdd->gct.is_running = true;

	/* prevent sw reset to trigger esd recovery */
	LCD_INFO(vdd, "disable esd interrupt\n");
	if (vdd->esd_recovery.esd_irq_enable)
		vdd->esd_recovery.esd_irq_enable(false, true, (void *)vdd);

	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.enable = 1;
	while (!list_empty(&vdd->cmd_lock.wait_list) && --wait_cnt)
		usleep_range(500, 500);

	for (i = TX_GCT_ENTER; i <= TX_GCT_EXIT; i++)
		ss_set_exclusive_tx_packet(vdd, i, 1);
	ss_set_exclusive_tx_packet(vdd, RX_GCT_CHECKSUM, 1);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 1);

	usleep_range(10000, 11000);

	checksum = vdd->gct.checksum;
	for (i = VDDM_LV; i < MAX_VDDM; i++) {
		struct dsi_panel_cmd_set *set;

		LCD_INFO(vdd, "(%d) TX_GCT_ENTER\n", i);
		/* VDDM LV set (0x0: 1.0V, 0x10: 0.9V, 0x30: 1.1V) */
		set = ss_get_cmds(vdd, TX_GCT_ENTER);
		if (SS_IS_CMDS_NULL(set)) {
			LCD_ERR(vdd, "No cmds for TX_GCT_ENTER..\n");
			break;
		}

		idx = ss_get_cmd_idx(set, 0x02, 0xD7);
		set->cmds[idx].ss_txbuf[1] = vddm_set[i];

		idx = ss_get_cmd_idx(set, 0x26, 0xF4);
		if (idx >= 0)
			set->cmds[idx].ss_txbuf[1] = vddm_set2[i];

		ss_send_cmd(vdd, TX_GCT_ENTER);

		msleep(150);

		ss_panel_data_read(vdd, RX_GCT_CHECKSUM, checksum++,
				LEVEL_KEY_NONE);
		LCD_INFO(vdd, "(%d) read checksum: %x\n", i, *(checksum - 1));

		LCD_INFO(vdd, "(%d) TX_GCT_MID\n", i);
		ss_send_cmd(vdd, TX_GCT_MID);

		msleep(150);

		ss_panel_data_read(vdd, RX_GCT_CHECKSUM, checksum++,
				LEVEL_KEY_NONE);

		LCD_INFO(vdd, "(%d) read checksum: %x\n", i, *(checksum - 1));

		LCD_INFO(vdd, "(%d) TX_GCT_EXIT\n", i);
		ss_send_cmd(vdd, TX_GCT_EXIT);
	}

	vdd->gct.on = 1;

	LCD_INFO(vdd, "checksum = {%x %x %x %x}\n",
			vdd->gct.checksum[0], vdd->gct.checksum[1],
			vdd->gct.checksum[2], vdd->gct.checksum[3]);

	/* exit exclusive mode*/
	for (i = TX_GCT_ENTER; i <= TX_GCT_EXIT; i++)
		ss_set_exclusive_tx_packet(vdd, i, 0);
	ss_set_exclusive_tx_packet(vdd, RX_GCT_CHECKSUM, 0);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 0);

	/* Reset panel to enter nornal panel mode.
	 * The on commands also should be sent before update new frame.
	 * Next new frame update is blocked by exclusive_tx.enable
	 * in ss_event_frame_update_pre(), and it will be released
	 * by wake_up exclusive_tx.ex_tx_waitq.
	 * So, on commands should be sent before wake up the waitq
	 * and set exclusive_tx.enable to false.
	 */
	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_OFF, 1);
	ss_send_cmd(vdd, DSI_CMD_SET_OFF);

	ss_set_panel_state(vdd, PANEL_PWR_OFF);
	dsi_panel_power_off(panel);
	dsi_panel_power_on(panel);
	ss_set_panel_state(vdd, PANEL_PWR_ON_READY);

	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_ON, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_ENABLE, 1);
	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_PPS, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_DISABLE, 1);

	ss_send_cmd(vdd, DSI_CMD_SET_ON);
	dsi_panel_update_pps(panel);

	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_OFF, 0);
	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_ON, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_ENABLE, 0);
	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_PPS, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_DISABLE, 0);

	vdd->exclusive_tx.enable = 0;
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);

	vdd->mafpc.force_delay = true;
	ss_panel_on_post(vdd);

	vdd->gct.is_running = false;

	/* enable esd interrupt */
	LCD_INFO(vdd, "enable esd interrupt\n");
	if (vdd->esd_recovery.esd_irq_enable)
		vdd->esd_recovery.esd_irq_enable(true, true, (void *)vdd);

	ss_set_test_mode_state(vdd, PANEL_TEST_NONE);

	return ret;
}
#endif

static int ss_self_display_data_init(struct samsung_display_driver_data *vdd)
{
	u32 panel_type = 0;
	u32 panel_color = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	if (!vdd->self_disp.is_support) {
		LCD_ERR(vdd, "Self Display is not supported\n");
		return -EINVAL;
	}

	LCD_INFO(vdd, "Self Display Panel Data init\n");

	panel_type = (ss_panel_id0_get(vdd) & 0x30) >> 4;
	panel_color = ss_panel_id0_get(vdd) & 0xF;

	LCD_INFO(vdd, "Panel Type=0x%x, Panel Color=0x%x\n", panel_type, panel_color);

	vdd->self_disp.operation[FLAG_SELF_MASK].img_buf = self_mask_img_data;
	vdd->self_disp.operation[FLAG_SELF_MASK].img_size = ARRAY_SIZE(self_mask_img_data);
	vdd->self_disp.operation[FLAG_SELF_MASK].img_checksum = SELF_MASK_IMG_CHECKSUM;
	make_mass_self_display_img_cmds_XA2(vdd, TX_SELF_MASK_IMAGE, FLAG_SELF_MASK);

	vdd->self_disp.operation[FLAG_SELF_MASK_CRC].img_buf = self_mask_img_crc_data;
	vdd->self_disp.operation[FLAG_SELF_MASK_CRC].img_size = ARRAY_SIZE(self_mask_img_crc_data);
	make_mass_self_display_img_cmds_XA2(vdd, TX_SELF_MASK_IMAGE_CRC, FLAG_SELF_MASK_CRC);
	vdd->self_disp.operation[FLAG_SELF_ICON].img_buf = self_icon_img_data;
	vdd->self_disp.operation[FLAG_SELF_ICON].img_size = ARRAY_SIZE(self_icon_img_data);
	make_mass_self_display_img_cmds_XA2(vdd, TX_SELF_ICON_IMAGE, FLAG_SELF_ICON);

	vdd->self_disp.operation[FLAG_SELF_ACLK].img_buf = self_aclock_img_data;
	vdd->self_disp.operation[FLAG_SELF_ACLK].img_size = ARRAY_SIZE(self_aclock_img_data);
	make_mass_self_display_img_cmds_XA2(vdd, TX_SELF_ACLOCK_IMAGE, FLAG_SELF_ACLK);

	vdd->self_disp.operation[FLAG_SELF_DCLK].img_buf = self_dclock_img_data;
	vdd->self_disp.operation[FLAG_SELF_DCLK].img_size = ARRAY_SIZE(self_dclock_img_data);
	make_mass_self_display_img_cmds_XA2(vdd, TX_SELF_DCLOCK_IMAGE, FLAG_SELF_DCLK);

	vdd->self_disp.operation[FLAG_SELF_VIDEO].img_buf = self_video_img_data;
	vdd->self_disp.operation[FLAG_SELF_VIDEO].img_size = ARRAY_SIZE(self_video_img_data);
	make_mass_self_display_img_cmds_XA2(vdd, TX_SELF_VIDEO_IMAGE, FLAG_SELF_VIDEO);
	return 1;
}

static int ss_mafpc_data_init(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	if (!vdd->mafpc.is_support) {
		LCD_ERR(vdd, "mAFPC is not supported\n");
		return -EINVAL;
	}

	LCD_INFO(vdd, "mAFPC Panel Data init\n");

	vdd->mafpc.img_buf = mafpc_img_data;
	vdd->mafpc.img_size = ARRAY_SIZE(mafpc_img_data);

	if (vdd->mafpc.make_img_mass_cmds)
		vdd->mafpc.make_img_mass_cmds(vdd, vdd->mafpc.img_buf, vdd->mafpc.img_size, TX_MAFPC_IMAGE); /* Image Data */
	else if (vdd->mafpc.make_img_cmds)
		vdd->mafpc.make_img_cmds(vdd, vdd->mafpc.img_buf, vdd->mafpc.img_size, TX_MAFPC_IMAGE); /* Image Data */
	else {
		LCD_ERR(vdd, "Can not make mafpc image commands\n");
		return -EINVAL;
	}

	/* CRC Check For Factory Mode */
	vdd->mafpc.crc_img_buf = mafpc_img_data_crc_check;
	vdd->mafpc.crc_img_size = ARRAY_SIZE(mafpc_img_data_crc_check);

	if (vdd->mafpc.make_img_mass_cmds)
		vdd->mafpc.make_img_mass_cmds(vdd, vdd->mafpc.crc_img_buf, vdd->mafpc.crc_img_size, TX_MAFPC_CRC_CHECK_IMAGE); /* CRC Check Image Data */
	else if (vdd->mafpc.make_img_cmds)
		vdd->mafpc.make_img_cmds(vdd, vdd->mafpc.crc_img_buf, vdd->mafpc.crc_img_size, TX_MAFPC_CRC_CHECK_IMAGE); /* CRC Check Image Data */
	else {
		LCD_ERR(vdd, "Can not make mafpc image commands\n");
		return -EINVAL;
	}

	return true;
}

/* Please enable 'ss_copr_panel_init' in panel.c file to cal cd_avr for ABC(mafpc)*/
static void ss_copr_panel_init(struct samsung_display_driver_data *vdd)
{
	vdd->copr.ver = COPR_VER_5P0;
	vdd->copr.display_read = 0;
	ss_copr_init(vdd);
}

static int ss_octa_id_read(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *pcmds;
	char *read_buf;
	int read_len;
	char temp[50];
	int len = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	pcmds = ss_get_cmds(vdd, RX_OCTA_ID);

	/* Read Panel Unique OCTA ID (C9h 2nd~21th) */
	if (pcmds->count) {
		read_len = pcmds->cmds[0].msg.rx_len;
		read_buf = kzalloc(read_len, GFP_KERNEL);
		if (!read_buf) {
			LCD_ERR(vdd, "fail to kzalloc for buf\n");
			return false;
		}

		ss_panel_data_read(vdd, RX_OCTA_ID, read_buf, LEVEL1_KEY);

		LCD_INFO(vdd, "octa id (read buf): %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			read_buf[0], read_buf[1], read_buf[2], read_buf[3],
			read_buf[4], read_buf[5], read_buf[6], read_buf[7],
			read_buf[8], read_buf[9], read_buf[10], read_buf[11],
			read_buf[12], read_buf[13], read_buf[14], read_buf[15],
			read_buf[16], read_buf[17],	read_buf[18], read_buf[19]);

		len += sprintf(temp + len, "%d", (read_buf[0] & 0xF0) >> 4);
		len += sprintf(temp + len, "%d", (read_buf[0] & 0x0F));
		len += sprintf(temp + len, "%d", (read_buf[1] & 0x0F));
		len += sprintf(temp + len, "%02x", read_buf[2]);
		len += sprintf(temp + len, "%02x", read_buf[3]);

		len += sprintf(temp + len, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
			read_buf[4], read_buf[5], read_buf[6], read_buf[7],
			read_buf[8], read_buf[9], read_buf[10], read_buf[11],
			read_buf[12], read_buf[13], read_buf[14], read_buf[15],
			read_buf[16], read_buf[17], read_buf[18], read_buf[19]);

		vdd->octa_id_dsi = kzalloc(len, GFP_KERNEL);
		if (!vdd->octa_id_dsi)
			LCD_ERR(vdd, "fail to kzalloc for octa_id_dsi\n");
		else {
			vdd->octa_id_len = len;
			strlcat(vdd->octa_id_dsi, temp, vdd->octa_id_len);
			LCD_INFO(vdd, "octa id : [%d] %s \n", vdd->octa_id_len, vdd->octa_id_dsi);
		}
	} else {
		LCD_ERR(vdd, "DSI%d no octa_id_rx_cmds cmd\n", vdd->ndx);
		return false;
	}

	kfree(read_buf);

	return true;
}

static struct dsi_panel_cmd_set *ss_acl_on(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	int idx;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	pcmds = ss_get_cmds(vdd, TX_ACL_ON);
	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_ACL_ON..\n");
		return NULL;
	}

	idx = ss_get_cmd_idx(pcmds, 0x00, 0x55);
	pcmds->cmds[idx].ss_txbuf[1] = 0x01;

	if (vdd->br_info.gradual_acl_val == 2) {
		pcmds->cmds[idx].ss_txbuf[1] = 0x03;	/* ACL 30% */
	}

	LCD_DEBUG(vdd, "gradual_acl: %d, acl per: 0x%x",
			vdd->br_info.gradual_acl_val, pcmds->cmds[idx].ss_txbuf[1]);

	return pcmds;
}

static struct dsi_panel_cmd_set *ss_acl_off(struct samsung_display_driver_data *vdd, int *level_key)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	LCD_DEBUG(vdd, "off\n");
	return ss_get_cmds(vdd, TX_ACL_OFF);
}

static struct dsi_panel_cmd_set *ss_vint(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *vint_cmds = ss_get_cmds(vdd, TX_VINT);
	int idx;

	if (IS_ERR_OR_NULL(vdd) || SS_IS_CMDS_NULL(vint_cmds)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)vint_cmds);
		return NULL;
	}

	idx = ss_get_cmd_idx(vint_cmds, 0x00, 0xF4);
	if (vdd->xtalk_mode) {
		vint_cmds->cmds[idx].ss_txbuf[1] = 0x06;	/* VGH 6.2 V */
		LCD_INFO(vdd, "xtalk_mode on\n");
	}
	else {
		vint_cmds->cmds[idx].ss_txbuf[1] = 0x10;	/* VGH return V */
	}

	return vint_cmds;
}


enum LPMBL_CMD_ID {
	LPMBL_CMDID_CTRL = 1,
	LPMBL_CMDID_LFD = 2,
	LPMBL_CMDID_LPM_ON = 3,
};

static u8 hlpm_aor[4][30] = {
	{0x01, 0x30, 0x01, 0x30, 0x01, 0x30, 0x01, 0x30, 0x01, 0x30, 0x01, 0x30, 0x01, 0x30,
	 0x01, 0x30, 0x01, 0x30, 0x01, 0x30, 0x01, 0x30, 0x01, 0x30, 0x01, 0x30, 0x01, 0x30, 0x01, 0x30},
	{0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E,
	 0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E},
	{0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3,
	 0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3},
	{0x07, 0x95, 0x07, 0x95, 0x07, 0x95, 0x07, 0x95, 0x07, 0x95, 0x07, 0x95, 0x07, 0x95,
	 0x07, 0x95, 0x07, 0x95, 0x07, 0x95, 0x07, 0x95, 0x07, 0x95, 0x07, 0x95, 0x07, 0x95, 0x07, 0x95},
};

static void ss_set_panel_lpm_brightness(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *set = ss_get_cmds(vdd, TX_LPM_BL_CMD);
	struct dsi_panel_cmd_set *set_lpm_bl;
	int cmd_idx, aor_idx;

	if (SS_IS_CMDS_NULL(set)) {
		LCD_ERR(vdd, "No cmds for TX_LPM_BL_CMD\n");
		return;
	}

	aor_idx = ss_get_cmd_idx(set, 0xCD, 0x68);

	/* LPM_ON: 3. HLPM brightness */
	switch (vdd->panel_lpm.lpm_bl_level) {
	case LPM_60NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_60NIT_CMD);
		memcpy(&set->cmds[aor_idx].ss_txbuf[1], hlpm_aor[0], sizeof(hlpm_aor[0]));
		break;
	case LPM_30NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_30NIT_CMD);
		memcpy(&set->cmds[aor_idx].ss_txbuf[1], hlpm_aor[1], sizeof(hlpm_aor[0]));
		break;
	case LPM_10NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_10NIT_CMD);
		memcpy(&set->cmds[aor_idx].ss_txbuf[1], hlpm_aor[2], sizeof(hlpm_aor[0]));
		break;
	case LPM_2NIT:
	default:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_2NIT_CMD);
		memcpy(&set->cmds[aor_idx].ss_txbuf[1], hlpm_aor[3], sizeof(hlpm_aor[0]));
		break;
	}

	if (SS_IS_CMDS_NULL(set_lpm_bl)) {
		LCD_ERR(vdd, "No cmds for alpm_ctrl..\n");
		return;
	}
	cmd_idx = ss_get_cmd_idx(set, 0x37, 0x68);
	memcpy(&set->cmds[cmd_idx].ss_txbuf[1],
			&set_lpm_bl->cmds->ss_txbuf[1],
			sizeof(char) * (set->cmds[cmd_idx].msg.tx_len - 1));

	/* send lpm bl cmd */
	ss_send_cmd(vdd, TX_LPM_BL_CMD);

	LCD_INFO(vdd, "[Panel LPM] bl_level : %s\n",
			/* Check current brightness level */
			vdd->panel_lpm.lpm_bl_level == LPM_2NIT ? "2NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_10NIT ? "10NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_30NIT ? "30NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_60NIT ? "60NIT" : "UNKNOWN");
}

enum LPMON_CMD_ID {
	LPMON_CMDID_CTRL = 1,
	LPMON_CMDID_LFD = 2,
	LPMON_CMDID_LPM_ON = 5,
};
/*
 * This function will update parameters for LPM
 */
static void ss_update_panel_lpm_ctrl_cmd(struct samsung_display_driver_data *vdd, int enable)
{
	struct dsi_panel_cmd_set *set_lpm;
	struct dsi_panel_cmd_set *set_lpm_bl;
	int lpm_bl_ctrl_idx, wrdisvb_idx, aor_idx;
	int gm2_wrdisbv;

	if (enable) {
		set_lpm = ss_get_cmds(vdd, TX_LPM_ON);
		aor_idx = ss_get_cmd_idx(set_lpm, 0xCD, 0x68);
	} else {
		set_lpm = ss_get_cmds(vdd, TX_LPM_OFF);
	}

	if (SS_IS_CMDS_NULL(set_lpm)) {
		LCD_ERR(vdd, "No cmds for TX_LPM_ON/OFF\n");
		return;
	}

	/* 0x53 - LPM ON/OFF */
	switch (vdd->panel_lpm.lpm_bl_level) {
	case LPM_60NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_60NIT_CMD);
		if (aor_idx > 0)
			memcpy(&set_lpm->cmds[aor_idx].ss_txbuf[1], hlpm_aor[0], sizeof(hlpm_aor[0]));
		gm2_wrdisbv = 248;
		break;
	case LPM_30NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_30NIT_CMD);
		if (aor_idx > 0)
			memcpy(&set_lpm->cmds[aor_idx].ss_txbuf[1], hlpm_aor[1], sizeof(hlpm_aor[0]));
		gm2_wrdisbv = 124;
		break;
	case LPM_10NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_10NIT_CMD);
		if (aor_idx > 0)
			memcpy(&set_lpm->cmds[aor_idx].ss_txbuf[1], hlpm_aor[2], sizeof(hlpm_aor[0]));
		gm2_wrdisbv = 43;
		break;
	case LPM_2NIT:
	default:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_2NIT_CMD);
		if (aor_idx > 0)
			memcpy(&set_lpm->cmds[aor_idx].ss_txbuf[1], hlpm_aor[3], sizeof(hlpm_aor[0]));
		gm2_wrdisbv = 9;
		break;
	}

	if (enable) {
		if (SS_IS_CMDS_NULL(set_lpm_bl)) {
			LCD_ERR(vdd, "No cmds for lpm_bl..\n");
			return;
		}

		lpm_bl_ctrl_idx = ss_get_cmd_idx(set_lpm, 0x37, 0x68);
		memcpy(&set_lpm->cmds[lpm_bl_ctrl_idx].ss_txbuf[1],
				&set_lpm_bl->cmds->ss_txbuf[1],
				sizeof(char) * (set_lpm->cmds[lpm_bl_ctrl_idx].msg.tx_len - 1));
	} else {
		wrdisvb_idx = ss_get_cmd_idx(set_lpm, 0x00, 0x51);
		set_lpm->cmds[wrdisvb_idx].ss_txbuf[1] = (gm2_wrdisbv & 0xFF00) >> 8;
		set_lpm->cmds[wrdisvb_idx].ss_txbuf[2] = gm2_wrdisbv & 0xFF;
	}

	return;
}

static int ss_brightness_tot_level(struct samsung_display_driver_data *vdd)
{
	int tot_level_key = 0;

	/* HAB brightness setting requires F0h and FCh level key unlocked */
	/* DBV setting require TEST KEY3 (FCh) */
	tot_level_key = LEVEL0_KEY | LEVEL1_KEY | LEVEL2_KEY;

	return tot_level_key;
}

static int samsung_panel_off_pre(struct samsung_display_driver_data *vdd)
{
	int rc = 0;
	return rc;
}

static int samsung_panel_off_post(struct samsung_display_driver_data *vdd)
{
	int rc = 0;
	return rc;
}

static int ss_vrr_init(struct vrr_info *vrr)
{
	struct lfd_mngr *mngr;
	int i, scope;
	struct samsung_display_driver_data *vdd =
		container_of(vrr, struct samsung_display_driver_data, vrr);

	LCD_INFO(vdd, "+++\n");

	mutex_init(&vrr->vrr_lock);
	mutex_init(&vrr->brr_lock);

	vrr->running_vrr_mdp = false;
	vrr->running_vrr = false;

	/* Bootloader: 120HS */
	vrr->cur_refresh_rate = vrr->adjusted_refresh_rate = 120;
	vrr->cur_sot_hs_mode = vrr->adjusted_sot_hs_mode = true;

	vrr->hs_nm_seq = HS_NM_OFF;
	vrr->delayed_perf_normal = false;
	vrr->skip_vrr_in_brightness = false;

	vrr->brr_mode = BRR_OFF_MODE;
	vrr->brr_rewind_on = false;

	vrr->vrr_workqueue = create_singlethread_workqueue("vrr_workqueue");
	INIT_WORK(&vrr->vrr_work, ss_panel_vrr_switch_work);

	/* LFD mode */
	for (i = 0, mngr = &vrr->lfd.lfd_mngr[i]; i < LFD_CLIENT_MAX; i++, mngr++) {
		for (scope = 0; scope < LFD_SCOPE_MAX; scope++) {
			mngr->fix[scope] = LFD_FUNC_FIX_OFF;
			mngr->scalability[scope] = LFD_FUNC_SCALABILITY0;
			mngr->min[scope] = LFD_FUNC_MIN_CLEAR;
			mngr->max[scope] = LFD_FUNC_MAX_CLEAR;
		}
	}

#if IS_ENABLED(CONFIG_SEC_FACTORY)
	mngr = &vrr->lfd.lfd_mngr[LFD_CLIENT_FAC];
	mngr->fix[LFD_SCOPE_NORMAL] = LFD_FUNC_FIX_HIGH;
	mngr->fix[LFD_SCOPE_LPM] = LFD_FUNC_FIX_HIGH;
	mngr->fix[LFD_SCOPE_HMD] = LFD_FUNC_FIX_HIGH;
#endif

	/* TE modulation */
	vrr->te_mod_on = 0;
	vrr->te_mod_divider = 0;
	vrr->te_mod_cnt = 0;

	LCD_INFO(vdd, "---\n");
	return 0;
}

static bool ss_check_support_mode(struct samsung_display_driver_data *vdd, enum CHECK_SUPPORT_MODE mode)
{
	bool is_support = true;
	int cur_rr = vdd->vrr.cur_refresh_rate;
	bool cur_hs = vdd->vrr.cur_sot_hs_mode;

	switch (mode) {

	case CHECK_SUPPORT_BRIGHTDOT:
		if (!(cur_rr == 120 && cur_hs)) {
			is_support = false;
			LCD_ERR(vdd, "BRIGHT DOT fail: supported on 120HS(cur: %d%s)\n",
					cur_rr, cur_hs ? "HS" : "NS");
		}
		break;

	default:
		break;
	}

	return is_support;
}

#define FFC_REG	(0xC5)
static int ss_ffc(struct samsung_display_driver_data *vdd, int idx)
{
	struct dsi_panel_cmd_set *ffc_set;
	struct dsi_panel_cmd_set *dyn_ffc_set;
	struct dsi_panel_cmd_set *cmd_list[1];
	static int reg_list[1][2] = { {FFC_REG, -EINVAL} };
	int pos_ffc;

	LCD_INFO(vdd, "[DISPLAY_%d] +++ clk idx: %d, tx FFC\n", vdd->ndx, idx);

	ffc_set = ss_get_cmds(vdd, TX_FFC);
	dyn_ffc_set = ss_get_cmds(vdd, TX_DYNAMIC_FFC_SET);

	if (SS_IS_CMDS_NULL(ffc_set) || SS_IS_CMDS_NULL(dyn_ffc_set)) {
		LCD_ERR(vdd, "No cmds for TX_FFC..\n");
		return -EINVAL;
	}

	if (unlikely((reg_list[0][1] == -EINVAL))) {
		cmd_list[0] = ffc_set;
		ss_find_reg_offset(reg_list, cmd_list, ARRAY_SIZE(cmd_list));
	}

	pos_ffc = reg_list[0][1];
	if (pos_ffc == -EINVAL) {
		LCD_ERR(vdd, "fail to find FFC(C5h) offset in set\n");
		return -EINVAL;
	}

	memcpy(ffc_set->cmds[pos_ffc].ss_txbuf,
			dyn_ffc_set->cmds[idx].ss_txbuf,
			ffc_set->cmds[pos_ffc].msg.tx_len);

	ss_send_cmd(vdd, TX_FFC);

	LCD_INFO(vdd, "[DISPLAY_%d] --- clk idx: %d, tx FFC\n", vdd->ndx, idx);

	return 0;
}

static void make_brightness_packet(struct samsung_display_driver_data *vdd,
	struct dsi_cmd_desc *packet, int *cmd_cnt, enum BR_TYPE br_type)
{
	if (br_type == BR_TYPE_NORMAL) {
		/* ACL */
		if (vdd->br_info.acl_status || vdd->siop_status)
			ss_add_brightness_packet(vdd, BR_FUNC_ACL_ON, packet, cmd_cnt);
		else
			ss_add_brightness_packet(vdd, BR_FUNC_ACL_OFF, packet, cmd_cnt);

		ss_add_brightness_packet(vdd, BR_FUNC_IRC, packet, cmd_cnt);

		/* mAFPC */
		if (vdd->mafpc.is_support)
			ss_add_brightness_packet(vdd, BR_FUNC_MAFPC_SCALE, packet, cmd_cnt);

		/* gamma */
		ss_add_brightness_packet(vdd, BR_FUNC_GAMMA, packet, cmd_cnt);

		/* gamma compensation for gamma mode2 VRR modes */
		ss_add_brightness_packet(vdd, BR_FUNC_GAMMA_COMP, packet, cmd_cnt);

		/* vint */
		ss_add_brightness_packet(vdd, BR_FUNC_VINT, packet, cmd_cnt);

		/* VRR */
		ss_add_brightness_packet(vdd, BR_FUNC_VRR, packet, cmd_cnt);
	} else if (br_type == BR_TYPE_HBM) {
		/* acl */
		if (vdd->br_info.acl_status || vdd->siop_status) {
			ss_add_brightness_packet(vdd, BR_FUNC_HBM_ACL_ON, packet, cmd_cnt);
		} else {
			ss_add_brightness_packet(vdd, BR_FUNC_HBM_ACL_OFF, packet, cmd_cnt);
		}

		ss_add_brightness_packet(vdd, BR_FUNC_IRC, packet, cmd_cnt);

		/* Gamma */
		if (vdd->panel_revision + 'A' < 'B')
			ss_add_brightness_packet(vdd, BR_FUNC_HBM_GAMMA, packet, cmd_cnt);
		else
			ss_add_brightness_packet(vdd, BR_FUNC_GAMMA, packet, cmd_cnt);

		/* gamma compensation for gamma mode2 VRR modes */
		ss_add_brightness_packet(vdd, BR_FUNC_GAMMA_COMP, packet, cmd_cnt);

		/* mAFPC */
		if (vdd->mafpc.is_support)
			ss_add_brightness_packet(vdd, BR_FUNC_MAFPC_SCALE, packet, cmd_cnt);

		/* VRR */
		ss_add_brightness_packet(vdd, BR_FUNC_HBM_VRR, packet, cmd_cnt);
	} else {
		LCD_ERR(vdd, "undefined br_type (%d) \n", br_type);
	}

	return;
}

static int poc_erase(struct samsung_display_driver_data *vdd, u32 erase_pos, u32 erase_size, u32 target_pos)
{
	struct dsi_display *display = NULL;
	struct dsi_panel_cmd_set *poc_erase_sector_tx_cmds = NULL;
	int delay_us = 0;
	int image_size = 0;
	int type;
	int ret = 0;
	int wait_cnt = 1000; /* 1000 * 0.5ms = 500ms */

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "no vdd\n");
		return -EINVAL;
	}

	display = GET_DSI_DISPLAY(vdd);
	if (IS_ERR_OR_NULL(display)) {
		LCD_ERR(vdd, "no display");
		return -EINVAL;
	}

	if (!ss_is_ready_to_send_cmd(vdd)) {
		LCD_ERR(vdd, "Panel is not ready. Panel State(%d)\n", vdd->panel_state);
		return -EBUSY;
	}

	if (vdd->poc_driver.erase_sector_addr_idx[0] < 0) {
		LCD_ERR(vdd, "sector addr index is not implemented.. %d\n",
			vdd->poc_driver.erase_sector_addr_idx[0]);
		return -EINVAL;
	}

	poc_erase_sector_tx_cmds = ss_get_cmds(vdd, TX_POC_ERASE_SECTOR);
	if (SS_IS_CMDS_NULL(poc_erase_sector_tx_cmds)) {
		LCD_ERR(vdd, "No cmds for TX_POC_ERASE_SECTOR..\n");
		return -ENODEV;
	}

	image_size = vdd->poc_driver.image_size;
	delay_us = vdd->poc_driver.erase_delay_us;

	LCD_INFO(vdd, "[ERASE] (%6d / %6d), erase_size (%d), delay %dus\n",
		erase_pos, target_pos, erase_size, delay_us);

	/* Enter exclusive mode */
	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.permit_frame_update = 1;
	vdd->exclusive_tx.enable = 1;
	while (!list_empty(&vdd->cmd_lock.wait_list) && --wait_cnt)
		usleep_range(500, 500);

	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 1);

	/* POC MODE ENABLE */
	ss_send_cmd(vdd, TX_POC_ENABLE);

	LCD_INFO(vdd, "WRITE [TX_POC_PRE_ERASE_SECTOR]");
	ss_send_cmd(vdd, TX_POC_PRE_ERASE_SECTOR);

	poc_erase_sector_tx_cmds->cmds[2].ss_txbuf[vdd->poc_driver.erase_sector_addr_idx[0]]
											= (erase_pos & 0xFF0000) >> 16;
	poc_erase_sector_tx_cmds->cmds[2].ss_txbuf[vdd->poc_driver.erase_sector_addr_idx[1]]
											= (erase_pos & 0x00FF00) >> 8;
	poc_erase_sector_tx_cmds->cmds[2].ss_txbuf[vdd->poc_driver.erase_sector_addr_idx[2]]
											= erase_pos & 0x0000FF;

	ss_send_cmd(vdd, TX_POC_ERASE_SECTOR);

	usleep_range(delay_us, delay_us);

	if ((erase_pos + erase_size >= target_pos) || ret == -EIO) {
		LCD_INFO(vdd, "WRITE [TX_POC_POST_ERASE_SECTOR] - cur_erase_pos(%d) target_pos(%d) ret(%d)\n",
			erase_pos, target_pos, ret);
		ss_send_cmd(vdd, TX_POC_POST_ERASE_SECTOR);
	}

	/* POC MODE DISABLE */
	ss_send_cmd(vdd, TX_POC_DISABLE);

	/* exit exclusive mode*/
	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 0);
	vdd->exclusive_tx.permit_frame_update = 0;
	vdd->exclusive_tx.enable = 0;
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);

	return ret;
}

static int poc_write(struct samsung_display_driver_data *vdd, u8 *data, u32 write_pos, u32 write_size)
{
	struct dsi_display *display = NULL;
	struct dsi_panel_cmd_set *write_cmd = NULL;
	struct dsi_panel_cmd_set *write_data_add = NULL;
	int pos, type, ret = 0;
	int last_pos, delay_us, image_size, loop_cnt, poc_w_size, start_addr;
	/*int tx_size, tx_size1, tx_size2;*/
	int wait_cnt = 1000; /* 1000 * 0.5ms = 500ms */

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "no vdd\n");
		return -EINVAL;
	}

	display = GET_DSI_DISPLAY(vdd);
	if (IS_ERR_OR_NULL(display)) {
		LCD_ERR(vdd, "no display");
		return -EINVAL;
	}

	write_cmd = ss_get_cmds(vdd, TX_POC_WRITE_LOOP_256BYTE);
	if (SS_IS_CMDS_NULL(write_cmd)) {
		LCD_ERR(vdd, "no cmds for TX_POC_WRITE_LOOP_256BYTE..\n");
		return -EINVAL;
	}

	write_data_add = ss_get_cmds(vdd, TX_POC_WRITE_LOOP_DATA_ADD);
	if (SS_IS_CMDS_NULL(write_data_add)) {
		LCD_ERR(vdd, "no cmds for TX_POC_WRITE_LOOP_DATA_ADD..\n");
		return -EINVAL;
	}

	if (vdd->poc_driver.write_addr_idx[0] < 0) {
		LCD_ERR(vdd, "write addr index is not implemented.. %d\n",
			vdd->poc_driver.write_addr_idx[0]);
		return -EINVAL;
	}

	delay_us = vdd->poc_driver.write_delay_us; /* Panel dtsi set */
	image_size = vdd->poc_driver.image_size;
	last_pos = write_pos + write_size;
	poc_w_size = vdd->poc_driver.write_data_size;
	loop_cnt = vdd->poc_driver.write_loop_cnt;
	start_addr = vdd->poc_driver.start_addr;

	LCD_INFO(vdd, "[WRITE++] start_addr : %6d(%X) : write_pos : %6d, write_size : %6d, last_pos %6d, poc_w_sise : %d delay %dus\n",
		start_addr, start_addr, write_pos, write_size, last_pos, poc_w_size, delay_us);

	/* Enter exclusive mode */
	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.permit_frame_update = 1;
	vdd->exclusive_tx.enable = 1;
	while (!list_empty(&vdd->cmd_lock.wait_list) && --wait_cnt)
		usleep_range(500, 500);

	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 1);

	/* POC MODE ENABLE */
	ss_send_cmd(vdd, TX_POC_ENABLE);

	if (write_pos == start_addr) {
		LCD_INFO(vdd, "WRITE [TX_POC_PRE_WRITE]");
		ss_send_cmd(vdd, TX_POC_PRE_WRITE);
	}

	for (pos = write_pos; pos < last_pos; pos += poc_w_size) {
		if (!(pos % DEBUG_POC_CNT))
			LCD_INFO(vdd, "cur_write_pos : %d data : 0x%x\n", pos, data[pos - write_pos]);

		if (unlikely(atomic_read(&vdd->poc_driver.cancel))) {
			LCD_ERR(vdd, "cancel poc write by user\n");
			ret = -EIO;
			goto cancel_poc;
		}

		/* 128 Byte Write*/
		LCD_INFO(vdd, "pos[%X] buf_pos[%X] poc_w_size (%d)\n", pos, pos - write_pos, poc_w_size);

		/* data copy */
		memcpy(&write_cmd->cmds[0].ss_txbuf[1], &data[pos - write_pos], poc_w_size);

		ss_send_cmd(vdd, TX_POC_WRITE_LOOP_256BYTE);

		if (pos % loop_cnt == 0) {
			/*	Multi Data Address */
			write_data_add->cmds[2].ss_txbuf[vdd->poc_driver.write_addr_idx[0]]
											= (pos & 0xFF0000) >> 16;
			write_data_add->cmds[2].ss_txbuf[vdd->poc_driver.write_addr_idx[1]]
											= (pos & 0x00FF00) >> 8;
			write_data_add->cmds[2].ss_txbuf[vdd->poc_driver.write_addr_idx[2]]
											= (pos & 0x0000FF);
			ss_send_cmd(vdd, TX_POC_WRITE_LOOP_DATA_ADD);
		}

		usleep_range(delay_us, delay_us);
	}

cancel_poc:
	if (unlikely(atomic_read(&vdd->poc_driver.cancel))) {
		LCD_ERR(vdd, "cancel poc write by user\n");
		atomic_set(&vdd->poc_driver.cancel, 0);
		ret = -EIO;
	}

	if (pos == (start_addr + image_size) || ret == -EIO) {
		LCD_DEBUG(vdd, "WRITE_LOOP_END pos : %d \n", pos);
		ss_send_cmd(vdd, TX_POC_WRITE_LOOP_END);

		LCD_INFO(vdd, "WRITE [TX_POC_POST_WRITE] - image_size(%d) cur_write_pos(%d) ret(%d)\n", image_size, pos, ret);
		ss_send_cmd(vdd, TX_POC_POST_WRITE);
	}

	/* POC MODE DISABLE */
	ss_send_cmd(vdd, TX_POC_DISABLE);

	/* exit exclusive mode*/
	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 0);
	vdd->exclusive_tx.permit_frame_update = 0;
	vdd->exclusive_tx.enable = 0;
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);

	LCD_INFO(vdd, "[WRITE--] start_addr : %6d(%X) : write_pos : %6d, write_size : %6d, last_pos %6d, poc_w_sise : %d delay %dus\n",
			start_addr, start_addr, write_pos, write_size, last_pos, poc_w_size, delay_us);

	return ret;
}

#define FLASH_READ_SIZE 128
static int poc_read(struct samsung_display_driver_data *vdd, u8 *buf, u32 read_pos, u32 read_size) /* read_size = 256 */
{
	struct dsi_display *display = NULL;
	struct dsi_panel_cmd_set *poc_read_tx_cmds = NULL;
	struct dsi_panel_cmd_set *poc_read_rx_cmds = NULL;
	int delay_us;
	int image_size, start_addr;
	u8 rx_buf[FLASH_READ_SIZE];
	int pos;
	int type;
	int ret = 0;
	int wait_cnt = 1000; /* 1000 * 0.5ms = 500ms */
	int temp;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "no vdd\n");
		return -EINVAL;
	}

	display = GET_DSI_DISPLAY(vdd);
	if (IS_ERR_OR_NULL(display)) {
		LCD_ERR(vdd, "no display");
		return -EINVAL;
	}

	poc_read_tx_cmds = ss_get_cmds(vdd, TX_POC_READ);
	if (SS_IS_CMDS_NULL(poc_read_tx_cmds)) {
		LCD_ERR(vdd, "no cmds for TX_POC_READ..\n");
		return -EINVAL;
	}

	poc_read_rx_cmds = ss_get_cmds(vdd, RX_POC_READ);
	if (SS_IS_CMDS_NULL(poc_read_rx_cmds)) {
		LCD_ERR(vdd, "no cmds for RX_POC_READ..\n");
		return -EINVAL;
	}

	if (vdd->poc_driver.read_addr_idx[0] < 0) {
		LCD_ERR(vdd, "read addr index is not implemented.. %d\n",
			vdd->poc_driver.read_addr_idx[0]);
		return -EINVAL;
	}

	delay_us = vdd->poc_driver.read_delay_us; /* Panel dtsi set */
	image_size = vdd->poc_driver.image_size;
	start_addr = vdd->poc_driver.start_addr;

	LCD_INFO(vdd, "[READ++] read_pos : start : %6d(%X), pos : %6d(%X), read_size : %6d, delay %dus\n",
		start_addr, start_addr, read_pos, read_pos, read_size, delay_us);

	/* Enter exclusive mode */
	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.permit_frame_update = 1;
	vdd->exclusive_tx.enable = 1;
	while (!list_empty(&vdd->cmd_lock.wait_list) && --wait_cnt)
		usleep_range(500, 500);

	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 1);
	ss_set_exclusive_tx_packet(vdd, RX_POC_READ, 1);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 1);

	/* For sending direct rx cmd  */
	poc_read_rx_cmds->state = DSI_CMD_SET_STATE_HS;

	/* POC MODE ENABLE */
	ss_send_cmd(vdd, TX_POC_ENABLE);

	/* Before read poc data, need to read mca (only for B0) */
	if (read_pos == vdd->poc_driver.start_addr) {
		LCD_INFO(vdd, "WRITE [TX_POC_PRE_READ]");
		ss_send_cmd(vdd, TX_POC_PRE_READ);
	}

	LCD_INFO(vdd, "%d(%x) ~ %d(%x) \n", read_pos, read_pos, read_pos + read_size, read_pos + read_size);

	for (pos = read_pos; pos < (read_pos + read_size); pos += FLASH_READ_SIZE) {
		if (unlikely(atomic_read(&vdd->poc_driver.cancel))) {
			LCD_ERR(vdd, "cancel poc read by user\n");
			ret = -EIO;
			goto cancel_poc;
		}

		poc_read_tx_cmds->cmds[0].ss_txbuf[vdd->poc_driver.read_addr_idx[0]]
									= (pos & 0xFF0000) >> 16;
		poc_read_tx_cmds->cmds[0].ss_txbuf[vdd->poc_driver.read_addr_idx[1]]
									= (pos & 0x00FF00) >> 8;
		poc_read_tx_cmds->cmds[0].ss_txbuf[vdd->poc_driver.read_addr_idx[2]]
									= pos & 0x0000FF;

		ss_send_cmd(vdd, TX_POC_READ);

		usleep_range(delay_us, delay_us);

		// gara is not supported.
		temp = vdd->gpara;
		vdd->gpara = false;
		ss_panel_data_read(vdd, RX_POC_READ, rx_buf, LEVEL_KEY_NONE);
		vdd->gpara = temp;

		LCD_INFO(vdd, "buf[%d][%x] copy\n", pos - read_pos, pos - read_pos);
		memcpy(&buf[pos - read_pos], rx_buf, FLASH_READ_SIZE);
	}

cancel_poc:
	if (unlikely(atomic_read(&vdd->poc_driver.cancel))) {
		LCD_ERR(vdd, "cancel poc read by user\n");
		atomic_set(&vdd->poc_driver.cancel, 0);
		ret = -EIO;
	}

	if (pos == (start_addr + image_size) || ret == -EIO) {
		LCD_INFO(vdd, "WRITE [TX_POC_POST_READ] - image_size(%d) cur_read_pos(%d) ret(%d)\n", image_size, pos, ret);
		ss_send_cmd(vdd, TX_POC_POST_READ);
	}

	/* POC MODE DISABLE */
	ss_send_cmd(vdd, TX_POC_DISABLE);

	/* Exit exclusive mode*/
	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 0);
	ss_set_exclusive_tx_packet(vdd, RX_POC_READ, 0);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 0);

	vdd->exclusive_tx.permit_frame_update = 0;
	vdd->exclusive_tx.enable = 0;
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);

	LCD_INFO(vdd, "[READ--] read_pos : %6d, read_size : %6d, delay %dus\n", read_pos, read_size, delay_us);

	return ret;
}

static int ss_read_udc_data(struct samsung_display_driver_data *vdd)
{
	int i = 0;
	u8 *read_buf;
	u32 fsize = 0xC00000 | vdd->udc.size;

	read_buf = kzalloc(vdd->udc.size, GFP_KERNEL);

	ss_send_cmd(vdd, TX_POC_ENABLE);

	ss_send_cmd(vdd, TX_FLASH_GAMMA_PRE1);

	LCD_INFO(vdd, "[UDC] start_addr : %X, size : %X\n", vdd->udc.start_addr, vdd->udc.size);

	flash_read_bytes(vdd, vdd->udc.start_addr, fsize, vdd->udc.size, read_buf);
	memcpy(vdd->udc.data, read_buf, vdd->udc.size);

	ss_send_cmd(vdd, TX_FLASH_GAMMA_POST);

	ss_send_cmd(vdd, TX_POC_DISABLE);

	for (i = 0; i < vdd->udc.size/2; i++) {
		LCD_INFO(vdd, "[%d] %X.%X\n", i, vdd->udc.data[(i*2)+0], vdd->udc.data[(i*2)+1]);
	}

	vdd->udc.read_done = true;

	return 0;
}

/* mtp original data */
static u8 HS120_NORMAL_R_TYPE_BUF[GAMMA_R_SIZE];
/* mtp original data - V type */
static int HS120_NORMAL_V_TYPE_BUF[GAMMA_V_SIZE];

/* mtp original data */
static u8 HS120_HBM_R_TYPE_BUF[GAMMA_R_SIZE];
/* mtp original data - V type */
static int HS120_HBM_V_TYPE_BUF[GAMMA_V_SIZE];

/* HS96 compensated data */
static u8 HS96_R_TYPE_COMP[MTP_OFFSET_TAB_SIZE_96HS][GAMMA_R_SIZE];
/* HS96 compensated data V type*/
static int HS96_V_TYPE_COMP[MTP_OFFSET_TAB_SIZE_96HS][GAMMA_V_SIZE];

static void ss_print_gamma_comp(struct samsung_display_driver_data *vdd)
{
	char pBuffer[256];
	int i, j;

	LCD_INFO(vdd, "== HS120_NORMAL_R_TYPE_BUF ==\n");
	memset(pBuffer, 0x00, 256);
	for (i = 0; i < GAMMA_R_SIZE; i++)
		snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_NORMAL_R_TYPE_BUF[i]);
	LCD_INFO(vdd, "SET : %s\n", pBuffer);

	LCD_INFO(vdd, "== HS120_NORMAL_V_TYPE_BUF ==\n");
	memset(pBuffer, 0x00, 256);
	for (i = 0; i < GAMMA_V_SIZE; i++)
		snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_NORMAL_V_TYPE_BUF[i]);
	LCD_INFO(vdd, "SET : %s\n", pBuffer);


	LCD_INFO(vdd, "== HS120_HBM_R_TYPE_BUF ==\n");
	memset(pBuffer, 0x00, 256);
	for (i = 0; i < GAMMA_R_SIZE; i++)
		snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_HBM_R_TYPE_BUF[i]);
	LCD_INFO(vdd, "SET : %s\n", pBuffer);

	LCD_INFO(vdd, "== HS120_HBM_V_TYPE_BUF ==\n");
	memset(pBuffer, 0x00, 256);
	for (i = 0; i < GAMMA_V_SIZE; i++)
		snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_HBM_V_TYPE_BUF[i]);
	LCD_INFO(vdd, "SET : %s\n", pBuffer);

	LCD_INFO(vdd, "== HS96_V_TYPE_COMP ==\n");
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_96HS; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_V_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS96_V_TYPE_COMP[i][j]);
		LCD_INFO(vdd, "COMP[%d] : %s\n", i, pBuffer);
	}

	LCD_INFO(vdd, "== HS96_R_TYPE_COMP ==\n");
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_96HS; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_R_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS96_R_TYPE_COMP[i][j]);
		LCD_INFO(vdd, "COMP[%d] : %s\n", i, pBuffer);
	}
}

static int ss_gm2_gamma_comp_init(struct samsung_display_driver_data *vdd)
{
	u8 readbuf[GAMMA_R_SIZE];
	int i, j, m;
	int val, *v_val_120;
	int start_addr;
	struct flash_raw_table *gamma_tbl;
	u32 fsize = 0xC00000 | GAMMA_R_SIZE;

	gamma_tbl = vdd->br_info.br_tbl->gamma_tbl;

	LCD_INFO(vdd, "++\n");
	vdd->debug_data->print_cmds = true;

	memcpy(MTP_OFFSET_96HS_VAL, MTP_OFFSET_96HS_VAL_revA, sizeof(MTP_OFFSET_96HS_VAL));

	if (vdd->panel_revision + 'A' <= 'A') {
		memcpy(aor_table_96, aor_table_96_revA, sizeof(aor_table_96));
		memcpy(aor_table_120, aor_table_120_revA, sizeof(aor_table_120));
	} else {
		memcpy(aor_table_96, aor_table_96_revB, sizeof(aor_table_96));
		memcpy(aor_table_120, aor_table_120_revB, sizeof(aor_table_120));
	}

	/***********************************************************/
	/* 1. [HBM/ NORMAL] Read HS120 ORIGINAL GAMMA Flash      */
	/***********************************************************/
	ss_send_cmd(vdd, TX_POC_ENABLE);
	ss_send_cmd(vdd, TX_FLASH_GAMMA_PRE1);

	start_addr = gamma_tbl->flash_table_hbm_gamma_offset;
	LCD_INFO(vdd, "[HBM] start_addr : %X\n", start_addr);
	flash_read_bytes(vdd, start_addr, fsize, GAMMA_R_SIZE, readbuf);
	memcpy(&HS120_HBM_R_TYPE_BUF, readbuf, GAMMA_R_SIZE);

	start_addr = gamma_tbl->flash_table_normal_gamma_offset;
	LCD_INFO(vdd, "[NORMAL] start_addr : %X\n", start_addr);
	flash_read_bytes(vdd, start_addr, fsize, GAMMA_R_SIZE, readbuf);
	memcpy(&HS120_NORMAL_R_TYPE_BUF, readbuf, GAMMA_R_SIZE);

	ss_send_cmd(vdd, TX_FLASH_GAMMA_POST);
	ss_send_cmd(vdd, TX_POC_DISABLE);

	vdd->debug_data->print_cmds = false;

	/***********************************************************/
	/* 2-1. [HBM] translate Register type to V type            */
	/***********************************************************/
	j = 0;
	for (i = 0; i < GAMMA_R_SIZE; ) {
		if (i == 0 || i == GAMMA_R_SIZE - 5) { /* R0, R12 */
			HS120_HBM_V_TYPE_BUF[j++] = (GET_BITS(HS120_HBM_R_TYPE_BUF[i], 0, 3) << 8)
									| GET_BITS(HS120_HBM_R_TYPE_BUF[i+2], 0, 7);
			HS120_HBM_V_TYPE_BUF[j++] = (GET_BITS(HS120_HBM_R_TYPE_BUF[i+1], 4, 7) << 8)
									| GET_BITS(HS120_HBM_R_TYPE_BUF[i+3], 0, 7);
			HS120_HBM_V_TYPE_BUF[j++] = (GET_BITS(HS120_HBM_R_TYPE_BUF[i+1], 0, 3) << 8)
									| GET_BITS(HS120_HBM_R_TYPE_BUF[i+4], 0, 7);
			i += 5;
		} else {	/* R1 ~ R11 */
			HS120_HBM_V_TYPE_BUF[j++] = (GET_BITS(HS120_HBM_R_TYPE_BUF[i], 4, 5) << 8)
									| GET_BITS(HS120_HBM_R_TYPE_BUF[i+1], 0, 7);
			HS120_HBM_V_TYPE_BUF[j++] = (GET_BITS(HS120_HBM_R_TYPE_BUF[i], 2, 3) << 8)
									| GET_BITS(HS120_HBM_R_TYPE_BUF[i+2], 0, 7);
			HS120_HBM_V_TYPE_BUF[j++] = (GET_BITS(HS120_HBM_R_TYPE_BUF[i], 0, 1) << 8)
									| GET_BITS(HS120_HBM_R_TYPE_BUF[i+3], 0, 7);
			i += 4;
		}
	}

#if 0
	/***********************************************************/
	/* 1-3. [HBM] Make HS96_V_TYPE_COMP[MTP_OFFSET_HBM_IDX]	   */
	/***********************************************************/

	for (j = 0; j < GAMMA_V_SIZE; j++) {
		/* check underflow & overflow */
		if (HS120_HBM_V_TYPE_BUF[j] + MTP_OFFSET_96HS_VAL[MTP_OFFSET_HBM_IDX][j] < 0) {
			HS96_V_TYPE_COMP[MTP_OFFSET_HBM_IDX][j] = 0;
		} else {
			if (j <= 2 || j >= (GAMMA_V_SIZE - 3)) {	/* 1th & 14th 12bit(0xFFF) */
				val = HS120_HBM_V_TYPE_BUF[j] + MTP_OFFSET_96HS_VAL[i][j];
				if (val > 0xFFF)	/* check overflow */
					HS96_V_TYPE_COMP[MTP_OFFSET_HBM_IDX][j] = 0xFFF;
				else
					HS96_V_TYPE_COMP[MTP_OFFSET_HBM_IDX][j] = val;
			} else {	/* 2th - 13th 11bit(0x7FF) */
				val = HS120_HBM_V_TYPE_BUF[j] + MTP_OFFSET_96HS_VAL[0][j];
				if (val > 0x3FF)	/* check overflow */
					HS96_V_TYPE_COMP[MTP_OFFSET_HBM_IDX][j] = 0x3FF;
				else
					HS96_V_TYPE_COMP[MTP_OFFSET_HBM_IDX][j] = val;
			}
		}
	}
#endif

	/***********************************************************/
	/* 2-2. [NORMAL] translate Register type to V type 	       */
	/***********************************************************/

	/* HS120 */
	j = 0;
	for (i = 0; i < GAMMA_R_SIZE; ) {
		if (i == 0 || i == GAMMA_R_SIZE - 5) { /* R0, R12 */
			HS120_NORMAL_V_TYPE_BUF[j++] = (GET_BITS(HS120_NORMAL_R_TYPE_BUF[i], 0, 3) << 8)
									| GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+2], 0, 7);
			HS120_NORMAL_V_TYPE_BUF[j++] = (GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+1], 4, 7) << 8)
									| GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+3], 0, 7);
			HS120_NORMAL_V_TYPE_BUF[j++] = (GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+1], 0, 3) << 8)
									| GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+4], 0, 7);
			i += 5;
		} else {	/* R1 ~ R11 */
			HS120_NORMAL_V_TYPE_BUF[j++] = (GET_BITS(HS120_NORMAL_R_TYPE_BUF[i], 4, 5) << 8)
									| GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+1], 0, 7);
			HS120_NORMAL_V_TYPE_BUF[j++] = (GET_BITS(HS120_NORMAL_R_TYPE_BUF[i], 2, 3) << 8)
									| GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+2], 0, 7);
			HS120_NORMAL_V_TYPE_BUF[j++] = (GET_BITS(HS120_NORMAL_R_TYPE_BUF[i], 0, 1) << 8)
									| GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+3], 0, 7);
			i += 4;
		}
	}

	/*************************************************************/
	/* 3. [ALL] Make HS96_V_TYPE_COMP (NORMAL + HBM) */
	/*************************************************************/

	/* 96HS */
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_96HS; i++) {
		if (i <= MAX_BL_PF_LEVEL)
			v_val_120 = HS120_NORMAL_V_TYPE_BUF;
		else
			v_val_120 = HS120_HBM_V_TYPE_BUF;

		for (j = 0; j < GAMMA_V_SIZE; j++) {
			/* check underflow & overflow */
			if (v_val_120[j] + MTP_OFFSET_96HS_VAL[i][j] < 0) {
				HS96_V_TYPE_COMP[i][j] = 0;
			} else {
				if (j <= 2 || j >= (GAMMA_V_SIZE - 3)) {/* 1th & 14th 12bit(0xFFF) */
					val = v_val_120[j] + MTP_OFFSET_96HS_VAL[i][j];
					if (val > 0xFFF)	/* check overflow */
						HS96_V_TYPE_COMP[i][j] = 0xFFF;
					else
						HS96_V_TYPE_COMP[i][j] = val;
				} else {	/* 2th - 13th 11bit(0x7FF) */
					val = v_val_120[j] + MTP_OFFSET_96HS_VAL[i][j];
					if (val > 0x3FF)	/* check overflow */
						HS96_V_TYPE_COMP[i][j] = 0x3FF;
					else
						HS96_V_TYPE_COMP[i][j] = val;
				}
			}
		}
	}

	/******************************************************/
	/* 4. translate HS96_V_TYPE_COMP type to Register type*/
	/******************************************************/
	/* 96HS */
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_96HS; i++) {
		m = 0; // GAMMA SET size
		for (j = 0; j < GAMMA_V_SIZE; j += RGB_MAX) {
			if (j == 0 || j == GAMMA_V_SIZE - 3) {
				HS96_R_TYPE_COMP[i][m++] = GET_BITS(HS96_V_TYPE_COMP[i][j+R], 8, 11);
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+G], 8, 11) << 4)
											| (GET_BITS(HS96_V_TYPE_COMP[i][j+B], 8, 11));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+R], 0, 7));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+G], 0, 7));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+B], 0, 7));
			} else {	/* 1st ~ 10th */
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+R], 8, 9) << 4)
											| (GET_BITS(HS96_V_TYPE_COMP[i][j+G], 8, 9) << 2)
											| GET_BITS(HS96_V_TYPE_COMP[i][j+B], 8, 9);
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+R], 0, 7));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+G], 0, 7));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+B], 0, 7));
			}
		}
	}

	/* print all results */
//	ss_print_gamma_comp(vdd);

	LCD_INFO(vdd, " --\n");

	return 0;
}

static struct dsi_panel_cmd_set *ss_brightness_gm2_gamma_comp(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds = ss_get_cmds(vdd, TX_VRR_GM2_GAMMA_COMP);
	struct dsi_panel_cmd_set *vrr_cmds = ss_get_cmds(vdd, TX_VRR);
	int offset_idx = -1;
	int cd_idx, cmd_idx = 0;
	int aor_val = 0;

	if (SS_IS_CMDS_NULL(vrr_cmds)) {
		LCD_INFO(vdd, "no vrr cmds\n");
		return NULL;
	}

	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_VRR_GM2_GAMMA_COMP.. \n");
		return NULL;
	}

//	return NULL;

	if (vdd->br_info.common_br.bl_level > MAX_BL_PF_LEVEL)
		cd_idx = MAX_BL_PF_LEVEL + 1;
	else
		cd_idx = vdd->br_info.common_br.cd_idx;

	LCD_INFO(vdd, "vdd->vrr.cur_refresh_rate [%d], cd_idx [%d] ++\n",
		vdd->vrr.cur_refresh_rate, cd_idx);

	if (vdd->vrr.cur_refresh_rate == 96 || vdd->vrr.cur_refresh_rate == 48) {
		aor_val = aor_table_96[cd_idx];

		//offset_idx = mtp_offset_idx_table_96hs[cd_idx];
		offset_idx = cd_idx;
		if (cd_idx >= 0) {
			LCD_INFO(vdd, "aor [%X]\n", aor_val);
			cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x37, 0x68);
			vrr_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x01;		/* AOR Enable Flag */
			vrr_cmds->cmds[cmd_idx].ss_txbuf[2] = (aor_val & 0xFF00) >> 8;
			vrr_cmds->cmds[cmd_idx].ss_txbuf[3] = (aor_val & 0xFF);
			vrr_cmds->cmds[cmd_idx].ss_txbuf[4] = 0x01;

			cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x3DA, 0x67);	/* Gamma Enable Flag */
			vrr_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x01;

			memcpy(&pcmds->cmds[2].ss_txbuf[1], &HS96_R_TYPE_COMP[cd_idx][0], 37);
			memcpy(&pcmds->cmds[3].ss_txbuf[1], &HS96_R_TYPE_COMP[cd_idx][37], 17);
		} else {
			/* restore original register */
			LCD_ERR(vdd, "ERROR!! check idx[%d]\n", cd_idx);
		}

		return pcmds;
	} else if (vdd->vrr.cur_refresh_rate == 120 || vdd->vrr.cur_refresh_rate == 60) {
		cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x37, 0x68);		/* AOR Enable Flag */
		vrr_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x00;
		vrr_cmds->cmds[cmd_idx].ss_txbuf[4] = 0x00;

		cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x3DA, 0x67);	/* Gamma Enable Flag */
		vrr_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x00;

		return pcmds;
	}

	LCD_INFO(vdd, " --\n");

	return NULL;
}

void S6E3XA2_AMF755ZE01_QXGA_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "%s\n", ss_get_panel_name(vdd));

	/* Default Panel Power Status is OFF */
	vdd->panel_state = PANEL_PWR_OFF;

	/* ON/OFF */
	vdd->panel_func.samsung_panel_on_pre = samsung_panel_on_pre;
	vdd->panel_func.samsung_panel_on_post = samsung_panel_on_post;
//	vdd->panel_func.samsung_display_on_post_debug = samsung_display_on_post;
	vdd->panel_func.samsung_panel_off_pre = samsung_panel_off_pre;
	vdd->panel_func.samsung_panel_off_post = samsung_panel_off_post;

	/* DDI RX */
	vdd->panel_func.samsung_panel_revision = ss_panel_revision;
	vdd->panel_func.samsung_module_info_read = ss_module_info_read;
	vdd->panel_func.samsung_ddi_id_read = ss_ddi_id_read;
	vdd->panel_func.samsung_octa_id_read = ss_octa_id_read;
	vdd->panel_func.samsung_elvss_read = ss_elvss_read;
//	vdd->panel_func.samsung_cmd_log_read = ss_cmd_log_read;

	/* Make brightness packer */
	vdd->panel_func.make_brightness_packet = make_brightness_packet;

	/* Brightness */
	vdd->panel_func.br_func[BR_FUNC_GAMMA] = ss_brightness_gamma_mode2_normal;
	vdd->panel_func.br_func[BR_FUNC_ACL_ON] = ss_acl_on;
	vdd->panel_func.br_func[BR_FUNC_ACL_OFF] = ss_acl_off;
	vdd->panel_func.br_func[BR_FUNC_VINT] = ss_vint;
	vdd->panel_func.br_func[BR_FUNC_VRR] = ss_vrr;
	vdd->panel_func.br_func[BR_FUNC_GAMMA_COMP] = ss_brightness_gm2_gamma_comp;
//	vdd->panel_func.br_func[BR_FUNC_HBM_GAMMA_COMP] = ss_brightness_gm2_hbm_gamma_comp;
	vdd->panel_func.br_func[BR_FUNC_IRC] = ss_irc;

	/* HBM */
	vdd->panel_func.br_func[BR_FUNC_HBM_GAMMA] = ss_brightness_gamma_mode2_hbm;
	vdd->panel_func.br_func[BR_FUNC_HBM_VRR] = ss_vrr;
	vdd->panel_func.br_func[BR_FUNC_HBM_ACL_ON] = ss_acl_on;
	vdd->panel_func.br_func[BR_FUNC_HBM_ACL_OFF] = ss_acl_off;

	/* Total level key in brightness */
	vdd->panel_func.samsung_brightness_tot_level = ss_brightness_tot_level;

	/* Panel LPM */
	vdd->panel_func.samsung_update_lpm_ctrl_cmd = ss_update_panel_lpm_ctrl_cmd;
	vdd->panel_func.samsung_set_lpm_brightness = ss_set_panel_lpm_brightness;

	/* Grayspot test */
	vdd->panel_func.samsung_gray_spot = ss_gray_spot;

	/* default brightness */
	vdd->br_info.common_br.bl_level = 255;

	/* mdnie */
	vdd->mdnie.support_mdnie = true;
	//vdd->mdnie.support_mdnie = false;

	vdd->mdnie.support_trans_dimming = true;
	vdd->mdnie.mdnie_tune_size[0] = sizeof(BYPASS_MDNIE_1);
	vdd->mdnie.mdnie_tune_size[1] = sizeof(BYPASS_MDNIE_2);
	vdd->mdnie.mdnie_tune_size[2] = sizeof(BYPASS_MDNIE_3);

	dsi_update_mdnie_data(vdd);

	/* COLOR WEAKNESS */
	vdd->panel_func.color_weakness_ccb_on_off =  NULL;

	/* Support DDI HW CURSOR */
	vdd->panel_func.ddi_hw_cursor = NULL;

	/* COPR */
	vdd->copr.panel_init = ss_copr_panel_init;

	/* ACL default ON */
	vdd->br_info.acl_status = 1;
	vdd->br_info.temperature = 20; // default temperature

	/* ACL default status in acl on */
	vdd->br_info.gradual_acl_val = 1;

	/* Gram Checksum Test */
//	vdd->panel_func.samsung_gct_write = ss_gct_write;
//	vdd->panel_func.samsung_gct_read = ss_gct_read;

	vdd->panel_func.ecc_read = ss_ecc_read;

	/* Self display */
	vdd->self_disp.factory_support = true;
	vdd->self_disp.init = self_display_init_XA2;
	vdd->self_disp.data_init = ss_self_display_data_init;

	/* mAPFC */
	vdd->mafpc.init = ss_mafpc_init_XA2;
	vdd->mafpc.data_init = ss_mafpc_data_init;

	/* FFC */
	vdd->panel_func.set_ffc = ss_ffc;

	/* DDI flash test */
//	vdd->panel_func.samsung_gm2_ddi_flash_prepare = ss_gm2_ddi_flash_prepare;
	vdd->panel_func.samsung_gm2_ddi_flash_prepare = NULL;
	vdd->panel_func.samsung_test_ddi_flash_check = ss_test_ddi_flash_check;

	/* gamma compensation for 48/96hz VRR mode in gamma mode2 */
	vdd->panel_func.samsung_gm2_gamma_comp_init = ss_gm2_gamma_comp_init;

	vdd->panel_func.samsung_print_gamma_comp = ss_print_gamma_comp;

	/* UDC */
	vdd->panel_func.read_udc_data = ss_read_udc_data;

	/* VRR */
//	vdd->panel_func.samsung_vrr_set_te_mod = ss_vrr_set_te_mode;
	vdd->panel_func.samsung_lfd_get_base_val = ss_update_base_lfd_val;
	ss_vrr_init(&vdd->vrr);

	vdd->panel_func.samsung_check_support_mode = ss_check_support_mode;

	/* change  MIPI Drive strength values for this panel - request by HW group */
	vdd->motto_info.motto_swing = 0xFF;

	/* FLSH */
	vdd->poc_driver.poc_erase = poc_erase;
	vdd->poc_driver.poc_write = poc_write;
	vdd->poc_driver.poc_read = poc_read;
	vdd->panel_func.read_flash = ss_read_flash;
}
