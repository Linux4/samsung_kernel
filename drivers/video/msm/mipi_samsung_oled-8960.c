/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/lcd.h>
#include <linux/wakelock.h>
#include "msm_fb.h"
#include "mipi_samsung_oled-8960.h"
#include "mdp4.h"
#ifdef CONFIG_SAMSUNG_CMC624
#include "samsung_cmc624-8960.h"
#endif
#if defined(CONFIG_MIPI_SAMSUNG_ESD_REFRESH)
#include "mipi_samsung_esd_refresh-8960.h"
#endif
#if defined(CONFIG_FB_MDP4_ENHANCE)
#include "mdp4_video_enhance-8960.h"
#endif

static struct mipi_samsung_driver_data msd;
static struct wake_lock idle_wake_lock;
static unsigned int recovery_boot_mode;

#define WA_FOR_FACTORY_MODE
#define READ_MTP_ONCE

#ifdef READ_REGISTER_ESD
#define ESD_INTERVAL 3
#endif

unsigned char bypass_lcd_id;
static char elvss_value;
int is_lcd_connected = 1;

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT)
struct dcs_cmd_req cmdreq;
#endif

#ifdef USE_READ_ID
#ifdef CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT
static DEFINE_MUTEX(mipi_lp_mutex);
char samsung_manufacture_id1_cmd[8] = {
	0x00, 0x00, 0x09, 0x40, 0xda, 0x00, 0x06, 0xA0};
char samsung_manufacture_id2_cmd[8] = {
	0x00, 0x00, 0x09, 0x40, 0xdb, 0x00, 0x06, 0xA0};
char samsung_manufacture_id3_cmd[8] = {
	0x00, 0x00, 0x09, 0x40, 0xdc, 0x00, 0x06, 0xA0};
#else
static char manufacture_id1[2] = {0xDA, 0x00}; /* DTYPE_DCS_READ */
static char manufacture_id2[2] = {0xDB, 0x00}; /* DTYPE_DCS_READ */
static char manufacture_id3[2] = {0xDC, 0x00}; /* DTYPE_DCS_READ */

static struct dsi_cmd_desc samsung_manufacture_id1_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(manufacture_id1), manufacture_id1};
static struct dsi_cmd_desc samsung_manufacture_id2_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(manufacture_id2), manufacture_id2};
static struct dsi_cmd_desc samsung_manufacture_id3_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(manufacture_id3), manufacture_id3};
#endif

static uint32 mipi_samsung_manufacture_id(struct msm_fb_data_type *mfd)
{
	struct dsi_buf *rp, *tp;
#ifdef CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT
	char *cmd_lp;
#else
	struct dsi_cmd_desc *cmd;
#endif
	uint32 id;

	tp = &msd.samsung_tx_buf;
	rp = &msd.samsung_rx_buf;
	mipi_dsi_buf_init(rp);
	mipi_dsi_buf_init(tp);

#ifdef CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT

	cmd_lp = samsung_manufacture_id1_cmd;

	mipi_dsi_cmds_rx_lp(mfd, tp, rp, cmd_lp, 1);
#else
	cmd = &samsung_manufacture_id1_cmd;
	mipi_dsi_cmds_rx(mfd, tp, rp, cmd, 1);
#endif
	pr_info("%s: manufacture_id1=%x\n", __func__, *rp->data);
	id = *((uint32 *)rp->data);
	id <<= 8;
#if defined(CONFIG_MACH_COMANCHE)
	mipi_dsi_buf_init((struct dsi_buf *)rp);
#else
	mipi_dsi_buf_init(rp);
#endif
	mipi_dsi_buf_init(tp);

#ifdef CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT

	cmd_lp = samsung_manufacture_id2_cmd;

	mipi_dsi_cmds_rx_lp(mfd, tp, rp, cmd_lp, 1);
#else
#if defined(CONFIG_MACH_COMANCHE)
	cmd = samsung_manufacture_id2_cmd;
#else
	cmd = &samsung_manufacture_id2_cmd;
#endif
	mipi_dsi_cmds_rx(mfd, tp, rp, cmd, 1);
#endif
	pr_info("%s: manufacture_id2=%x\n", __func__, *rp->data);
	bypass_lcd_id = *rp->data;
	id |= *((uint32 *)rp->data);
	id <<= 8;
#if defined(CONFIG_MACH_COMANCHE)
	mipi_dsi_buf_init((struct dsi_buf *)rp);
#else
	
	mipi_dsi_buf_init(rp);
#endif
	mipi_dsi_buf_init(tp);
#ifdef CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT

	cmd_lp = samsung_manufacture_id3_cmd;

	mipi_dsi_cmds_rx_lp(mfd, tp, rp, cmd_lp, 1);
#else
#if defined(CONFIG_MACH_COMANCHE)
	cmd_lp = samsung_manufacture_id3_cmd;
#else
	cmd = &samsung_manufacture_id3_cmd;
#endif
	mipi_dsi_cmds_rx(mfd, tp, rp, cmd, 1);
#endif
	pr_info("%s: manufacture_id3=%x\n", __func__, *rp->data);
	elvss_value = *rp->data;
	id |= *((uint32 *)rp->data);

	pr_info("%s: manufacture_id=%x\n", __func__, id);

#ifdef FACTORY_TEST
	if (id == 0x00) {
		pr_info("Lcd is not connected\n");
		is_lcd_connected = 0;
	}
#endif
	return id;
}
#endif
unsigned char bypass_LCD_Id(void){
	return bypass_lcd_id;
}
bool Is_4_8LCD_bypass(void){
	if ((bypass_lcd_id == 0x20) || (bypass_lcd_id == 0x40) ||
					(bypass_lcd_id == 0x60))
		return true;
	else
		return false;
}

bool Is_4_65LCD_bypass(void){
#if defined(CONFIG_MACH_STRETTO) || defined(CONFIG_MACH_SUPERIORLTE_SKT)
	return true;
#else
	if (bypass_lcd_id == 0xAE)
		return true;
	else
		return false;
#endif
}

#if  defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
unsigned int get_lcd_manufacture_id(void)
{
	return msd.mpd->manufacture_id;
}
#endif

#if defined(CONFIG_FB_MSM_MIPI_MAGNA_OLED_VIDEO_WVGA_PT)
/*
*	EA8868 has HW limitation. EA8868 deosn't have
*	global parameter to set read address point.
*/
static void read_reg(char srcReg, int srcLength, char *destBuffer,
	      const int isUseMutex, struct msm_fb_data_type *pMFD)
{
	/*
	*	EA8868 has 3 bank MTP register
	*	Each bank has 7 byte MTP data.
	*/
	char ea8868_read_address[] = {0xD3, 0xD4, 0xE0};
	const int one_read_size = 7;

	/* second byte is read address set */
	static char reg_set_read_register[] = { 0xFD, 0x00 };
	static struct dsi_cmd_desc ea8868_read_pos_cmd = {
		DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(reg_set_read_register),
		reg_set_read_register };

	/* first byte = read buffer register */
	static char read_reg[2] = { 0xFE, 0x00 };
	static struct dsi_cmd_desc ea8868_read_reg_cmd = {
		DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(read_reg), read_reg };

	/* first byte is size of Register */
	static char packet_size[] = { 0x04, 0 };
	static struct dsi_cmd_desc ea8868_packet_size_cmd = {
		DTYPE_MAX_PKTSIZE, 1, 0, 0, 0,
					sizeof(packet_size), packet_size };

	int readed_size, rx_cnt;
	int show_cnt = 0;

	int read_addr_cnt;
	char show_buffer[256] = {0,};
	int show_buffer_pos = 0;

	for (read_addr_cnt = 0; read_addr_cnt < sizeof(ea8868_read_address);
							read_addr_cnt++) {
		readed_size = 0;

		/* set read register address */
		reg_set_read_register[1] = ea8868_read_address[read_addr_cnt];
		mipi_dsi_buf_init(&msd.samsung_tx_buf);
		mipi_dsi_cmds_tx(&msd.samsung_tx_buf,
						&(ea8868_read_pos_cmd), 1);

		/*set maxpacket size */
		packet_size[0] = (char)one_read_size;
		mipi_dsi_buf_init(&msd.samsung_tx_buf);
		mipi_dsi_cmds_tx(&msd.samsung_tx_buf,
						&(ea8868_packet_size_cmd), 1);

		mipi_dsi_buf_init(&msd.samsung_tx_buf);
		mipi_dsi_buf_init(&msd.samsung_rx_buf);
		readed_size =
		    mipi_dsi_cmds_rx(pMFD, &msd.samsung_tx_buf,
				     &msd.samsung_rx_buf,
				     &ea8868_read_reg_cmd, one_read_size);

		for (rx_cnt = 0; rx_cnt < readed_size; rx_cnt++, show_cnt++) {
			if (destBuffer != NULL && show_cnt < srcLength)
				destBuffer[show_cnt] =
					msd.samsung_rx_buf.data[rx_cnt];
		}
	}

	/* only LSB is available */
	destBuffer[14] = 0x01;

	/* for debug */
	show_buffer_pos +=
		    snprintf(show_buffer, 256, "read_reg : ");

	for (read_addr_cnt = 0; read_addr_cnt < srcLength;) {
		show_buffer_pos +=
			snprintf(show_buffer + show_buffer_pos, 256,
				"%02x ", destBuffer[read_addr_cnt]);

		read_addr_cnt++;

		if (read_addr_cnt % 7 == 0)
			show_buffer_pos +=
			snprintf(show_buffer + show_buffer_pos, 256, ". ");
	}

	pr_info("%s\n", show_buffer);
}


#else
static void read_reg(char srcReg, int srcLength, char *destBuffer,
	      const int isUseMutex, struct msm_fb_data_type *pMFD)
{
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
	const int one_read_size = 7;
#else
	const int one_read_size = 4;
#endif
	struct dcs_cmd_req cmdreq;

	const int loop_limit = 16;
	/* first byte = read-register */
	static char read_reg[2] = { 0xFF, 0x00 };
	static struct dsi_cmd_desc s6e8aa0_read_reg_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(read_reg), read_reg };
	/* first byte is size of Register */
	static char packet_size[] = { 0x04, 0 };
	static struct dsi_cmd_desc s6e8aa0_packet_size_cmd = {
	DTYPE_MAX_PKTSIZE, 1, 0, 0, 0, sizeof(packet_size), packet_size };

	/* second byte is Read-position */
	static char reg_read_pos[] = { 0xB0, 0x00 };
	static struct dsi_cmd_desc s6e8aa0_read_pos_cmd = {
		DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(reg_read_pos),
		reg_read_pos };

	int read_pos;
	int readed_size;
	int show_cnt;

	int i, j;
	char show_buffer[256];
	int show_buffer_pos = 0;

	read_reg[0] = srcReg;

	show_buffer_pos +=
	    snprintf(show_buffer, 256, "read_reg : %X[%d] : ",
		 srcReg, srcLength);

	read_pos = 0;
	readed_size = 0;

	packet_size[0] = (char)srcLength;
	mipi_dsi_buf_init(&msd.samsung_tx_buf);
	cmdreq.cmds = &s6e8aa0_packet_size_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mipi_dsi_cmdlist_put(&cmdreq);

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
	msleep(20);
#endif

	show_cnt = 0;
	for (j = 0; j < loop_limit; j++) {
		reg_read_pos[1] = read_pos;
		cmdreq.cmds = &s6e8aa0_read_pos_cmd;
		cmdreq.cmds_cnt = 1;
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;

		if (mipi_dsi_cmdlist_put(&cmdreq) < 1) {
			show_buffer_pos +=
				snprintf(show_buffer + show_buffer_pos, 256,
					"Tx command FAILED");
			break;
		}
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
		msleep(20);
#endif
		mipi_dsi_buf_init(&msd.samsung_tx_buf);
		mipi_dsi_buf_init(&msd.samsung_rx_buf);
		readed_size =
		    mipi_dsi_cmds_rx(pMFD, &msd.samsung_tx_buf,
				     &msd.samsung_rx_buf, &s6e8aa0_read_reg_cmd,
				     one_read_size);
		for (i = 0; i < readed_size; i++, show_cnt++) {
			show_buffer_pos +=
			 snprintf(show_buffer + show_buffer_pos, 256, "%02x ",
				    msd.samsung_rx_buf.data[i]);
			if (destBuffer != NULL && show_cnt < srcLength) {
				destBuffer[show_cnt] =
				    msd.samsung_rx_buf.data[i];
			}
		}
		show_buffer_pos += snprintf(show_buffer +
			show_buffer_pos, 256, ".");
		read_pos += readed_size;
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
		msleep(20);

		if (read_pos >= srcLength)
			break;
#else
		if (read_pos > srcLength)
			break;
#endif
	}

	if (j == loop_limit)
		show_buffer_pos +=
		    snprintf(show_buffer + show_buffer_pos, 256, "Overrun");

	pr_info("%s\n", show_buffer);
}
#endif

static int mipi_samsung_disp_send_cmd(struct msm_fb_data_type *mfd,
				       enum mipi_samsung_cmd_list cmd,
				       unsigned char lock)
{
	struct dsi_cmd_desc *cmd_desc;
	struct dcs_cmd_req cmdreq;
	int cmd_size = 0;

	wake_lock(&idle_wake_lock);
#ifdef CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT
	mutex_lock(&mipi_lp_mutex);
#endif
	if (lock)
		mutex_lock(&mfd->dma->ov_mutex);

	switch (cmd) {
#ifdef CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_HD_PT
	case PANEL_READY_TO_ON_FAST:
		if (Is_4_65LCD_cmc() || Is_4_65LCD_bypass()) { /*4.65 LCD_ID*/
			pr_info("Select 4.65 = %x\n", LCD_Get_Value());
			cmd_desc = msd.mpd->ready_to_on_fast.cmd;
			cmd_size = msd.mpd->ready_to_on_fast.size;
		} else {
			pr_info("Select 4.8 = %x\n", LCD_Get_Value());
			cmd_desc = msd.mpd->ready_to_on_4_8_fast.cmd;
			cmd_size = msd.mpd->ready_to_on_4_8_fast.size;
		}
			break;
#endif
	case PANEL_READY_TO_ON:
#ifdef CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_HD_PT
		if (Is_4_65LCD_cmc() || Is_4_65LCD_bypass()) { /*4.65 LCD_ID*/
			pr_info("Select 4.65 = %x(%x)\n", LCD_Get_Value(),
				bypass_LCD_Id());
			cmd_desc = msd.mpd->ready_to_on.cmd;
			cmd_size = msd.mpd->ready_to_on.size;
		} else { /* 4.8 LCD_ID*/
			pr_info("Select 4.8 = %x(%x)\n", LCD_Get_Value(),
				bypass_LCD_Id());
			cmd_desc = msd.mpd->ready_to_on_4_8.cmd;
			cmd_size = msd.mpd->ready_to_on_4_8.size;
		}
#else
		cmd_desc = msd.mpd->ready_to_on.cmd;
		cmd_size = msd.mpd->ready_to_on.size;
#endif
		break;
	case PANEL_READY_TO_OFF:
		cmd_desc = msd.mpd->ready_to_off.cmd;
		cmd_size = msd.mpd->ready_to_off.size;
		break;
	case PANEL_ON:
		cmd_desc = msd.mpd->on.cmd;
		cmd_size = msd.mpd->on.size;
		break;
	case PANEL_OFF:
		cmd_desc = msd.mpd->off.cmd;
		cmd_size = msd.mpd->off.size;
		break;
	case PANEL_LATE_ON:
		cmd_desc = msd.mpd->late_on.cmd;
		cmd_size = msd.mpd->late_on.size;
		break;
	case PANEL_EARLY_OFF:
		cmd_desc = msd.mpd->early_off.cmd;
		cmd_size = msd.mpd->early_off.size;
		break;
	case PANEL_GAMMA_UPDATE:
		cmd_desc = msd.mpd->gamma_update.cmd;
		cmd_size = msd.mpd->gamma_update.size;
		break;
	case MTP_READ_ENABLE:
		cmd_desc = msd.mpd->mtp_read_enable.cmd;
		cmd_size = msd.mpd->mtp_read_enable.size;
		break;
#ifdef USE_ACL
	case PANEL_ACL_ON:
		cmd_desc = msd.mpd->acl_on.cmd;
		cmd_size = msd.mpd->acl_on.size;
		msd.mpd->ldi_acl_stat = true;
		break;
	case PANEL_ACL_OFF:
		cmd_desc = msd.mpd->acl_off.cmd;
		cmd_size = msd.mpd->acl_off.size;
		msd.mpd->ldi_acl_stat = false;
		break;
	case PANEL_ACL_UPDATE:
		cmd_desc = msd.mpd->acl_update.cmd;
		cmd_size = msd.mpd->acl_update.size;
		break;
#endif
#ifdef USE_ELVSS
	case PANEL_ELVSS_UPDATE:
#ifdef CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_HD_PT
		if (Is_4_65LCD_cmc() || Is_4_65LCD_bypass()) { /*4.65 LCD_ID*/
			msd.mpd->set_elvss(mfd->bl_level);
			cmd_desc = msd.mpd->elvss_update.cmd;
			cmd_size = msd.mpd->elvss_update.size;
			}
		else { /* 4.8 LCD_ID*/
			msd.mpd->set_elvss_4_8(mfd->bl_level);
			cmd_desc = msd.mpd->elvss_update_4_8.cmd;
			cmd_size = msd.mpd->elvss_update_4_8.size;
			}
#else
		msd.mpd->set_elvss(mfd->bl_level);
		cmd_desc = msd.mpd->elvss_update.cmd;
		cmd_size = msd.mpd->elvss_update.size;
#endif
		break;
#endif
#if  defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_HD_PT) \
		|| defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
	case PANEL_BRIGHT_CTRL:
		cmd_desc = msd.mpd->combined_ctrl.cmd;
		cmd_size = msd.mpd->combined_ctrl.size;
		break;
#endif
	default:
		goto unknown_command;
		;
	}

	if (!cmd_size)
		goto unknown_command;

	mipi_dsi_mdp_busy_wait();
	/* Added to resolved cmd loss during dimming factory test */
	mdelay(1);
	cmdreq.cmds = cmd_desc;
	cmdreq.cmds_cnt = cmd_size;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;
	if (cmd == PANEL_BRIGHT_CTRL)
		cmdreq.flags = CMD_REQ_COMMIT | CMD_REQ_SINGLE_TX;
	else
		cmdreq.flags = CMD_REQ_COMMIT;

	mipi_dsi_cmdlist_put(&cmdreq);

unknown_command:
	if (lock)
		mutex_unlock(&mfd->dma->ov_mutex);

#ifdef CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT
	mutex_unlock(&mipi_lp_mutex);
#endif
	wake_unlock(&idle_wake_lock);

	return 0;
}

static unsigned char first_on;

#if  defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_HD_PT) \
	|| defined(CONFIG_FB_MSM_MIPI_MAGNA_OLED_VIDEO_WVGA_PT) \
	|| defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
static int find_mtp(struct msm_fb_data_type *mfd, char * mtp)
{
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
	char first_mtp[MTP_DATA_SIZE_S6E63M0];
	char second_mtp[MTP_DATA_SIZE_S6E63M0];
	char third_mtp[MTP_DATA_SIZE_S6E63M0];
	int mtp_size = MTP_DATA_SIZE_S6E63M0;
#elif defined(CONFIG_FB_MSM_MIPI_MAGNA_OLED_VIDEO_WVGA_PT)
	char first_mtp[MTP_DATA_SIZE_EA8868];
	char second_mtp[MTP_DATA_SIZE_EA8868];
	char third_mtp[MTP_DATA_SIZE_EA8868];
	int mtp_size = MTP_DATA_SIZE_EA8868;
#else
	/*
	*	s6e8aa0x01 & s6e39a0x02 has same MTP length.
	*/
	char first_mtp[MTP_DATA_SIZE];
	char second_mtp[MTP_DATA_SIZE];
	char third_mtp[MTP_DATA_SIZE];
	int mtp_size = MTP_DATA_SIZE;
#endif
	int correct_mtp;

	pr_info("first time mpt read\n");
	read_reg(MTP_REGISTER, mtp_size, first_mtp, FALSE, mfd);

	pr_info("second time mpt read\n");
	read_reg(MTP_REGISTER, mtp_size, second_mtp, FALSE, mfd);

	if (memcmp(first_mtp, second_mtp, mtp_size) != 0) {
		pr_info("third time mpt read\n");
		read_reg(MTP_REGISTER, mtp_size, third_mtp, FALSE, mfd);

		if (memcmp(first_mtp, third_mtp, mtp_size) == 0) {
			pr_info("MTP data is used from first read mtp");
			memcpy(mtp, first_mtp, mtp_size);
			correct_mtp = 1;
		} else if (memcmp(second_mtp, third_mtp, mtp_size) == 0) {
			pr_info("MTP data is used from second read mtp");
			memcpy(mtp, second_mtp, mtp_size);
			correct_mtp = 2;
		} else {
			pr_info("MTP data is used 0 read mtp");
			memset(mtp, 0, mtp_size);
			correct_mtp = 0;
		}
	} else {
		pr_info("MTP data is used from first read mtp");
		memcpy(mtp, first_mtp, mtp_size);
		correct_mtp = 1;
	}

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
	/*
	*	checking V1 MTP value. V0 value must be 0.
	*/
	if ((mtp[0] != 0) || (mtp[7] != 0) || (mtp[14] != 0)) {
		memset(mtp, 0, mtp_size);
		correct_mtp = 0;
	}
#endif
	return correct_mtp;
}
#else
static int find_mtp(struct msm_fb_data_type *mfd, char * mtp)
{
	return 0;
}
#endif
static int mipi_samsung_disp_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_HD_PT) \
	|| defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT) \
	|| defined(CONFIG_FB_MSM_MIPI_MAGNA_OLED_VIDEO_WVGA_PT)
	static int boot_on;
#endif

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT) && defined(READ_REGISTER_ESD)
	int id2;
#endif

	mfd = platform_get_drvdata(pdev);
	if (unlikely(!mfd))
		return -ENODEV;
	if (unlikely(mfd->key != MFD_KEY))
		return -EINVAL;

	mipi = &mfd->panel_info.mipi;

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT)
	mipi_samsung_disp_send_cmd(mfd, MTP_READ_ENABLE, false);
#endif

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT) \
	|| defined(CONFIG_FB_MSM_MIPI_MAGNA_OLED_VIDEO_WVGA_PT)
	if (boot_on == 0)
		mipi_samsung_disp_send_cmd(mfd, MTP_READ_ENABLE, false);
#endif

#ifdef USE_READ_ID
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT) \
	|| defined(CONFIG_FB_MSM_MIPI_MAGNA_OLED_VIDEO_WVGA_PT)
	if (boot_on == 0)
		msd.mpd->manufacture_id = mipi_samsung_manufacture_id(mfd);
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT)
	msd.mpd->manufacture_id = mipi_samsung_manufacture_id(mfd);
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_HD_PT)
	if (!samsung_has_cmc624())
		msd.mpd->manufacture_id = mipi_samsung_manufacture_id(mfd);
	else
		msd.mpd->manufacture_id = LCD_Get_Value();
#endif
#endif

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT)
	if (!msd.dstat.is_elvss_loaded) {
		read_reg(ELVSS_REGISTER, ELVSS_DATA_SIZE,
			msd.mpd->lcd_elvss_data,
				 FALSE, mfd);  /* read ELVSS data */
		msd.dstat.is_elvss_loaded = true;
	}


	if (!msd.dstat.is_smart_dim_loaded) {
		/* Load MTP Data */
		int i;
		read_reg(MTP_REGISTER, MTP_DATA_SIZE,
				(u8 *)&(msd.mpd->smart_s6e39a0x02.MTP),
						FALSE, mfd);
		for (i = 0; i < MTP_DATA_SIZE; i++) {
			pr_info("%s MTP DATA[%d] : %02x\n", __func__, i,
			((char *)&(msd.mpd->smart_s6e39a0x02.MTP))[i]);
		}

		smart_dimming_init(&(msd.mpd->smart_s6e39a0x02));
#ifdef READ_MTP_ONCE
		msd.dstat.is_smart_dim_loaded = true;
#else
		msd.dstat.is_smart_dim_loaded = false;
#endif
		msd.dstat.gamma_mode = GAMMA_SMART;
	}
#endif
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_HD_PT)
	if (!msd.dstat.is_elvss_loaded) {
		if (!samsung_has_cmc624())
			msd.mpd->lcd_elvss_data[0] = elvss_value;
		else
			msd.mpd->lcd_elvss_data[0] = LCD_ID3();

		msd.dstat.is_elvss_loaded = true;
	}

	if (!msd.dstat.is_smart_dim_loaded) {
		/*Load MTP Data*/
		char pBuffer[256] = {0,};
		int i;
		struct SMART_DIM *psmart;
		char *mtp_data;
		int mtp_cnt;

		psmart = &(msd.mpd->smart_s6e8aa0x01);
		mtp_data = (char *)&(msd.mpd->smart_s6e8aa0x01.MTP);

		if (samsung_has_cmc624()) {
			memcpy(mtp_data, mtp_read_data, GAMMA_SET_MAX);
			pr_info("%s This board support CMC", __func__);
		} else {
			mtp_cnt = find_mtp(mfd, mtp_data);
			pr_info("%s MTP is determined : %d", __func__, mtp_cnt);
		}


		for (i = 0; i < MTP_DATA_SIZE; i++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x",
				mtp_data[i]);
		pr_info("MTP: %s", pBuffer);

		psmart->plux_table = msd.mpd->lux_table;
		psmart->lux_table_max = msd.mpd->lux_table_max_cnt;

		if (samsung_has_cmc624())
			psmart->ldi_revision = LCD_Get_Value();
		else
			psmart->ldi_revision = bypass_LCD_Id();

		smart_dimming_init(psmart);

		msd.dstat.is_smart_dim_loaded = true;
		msd.dstat.gamma_mode = GAMMA_SMART;
	}

	if (msd.mpd->gamma_initial && boot_on == 0) {
		msd.mpd->smart_s6e8aa0x01.brightness_level = 180;
		generate_gamma(&msd.mpd->smart_s6e8aa0x01,
			&(msd.mpd->gamma_initial[2]), GAMMA_SET_MAX);

		if (recovery_boot_mode == 0)
			boot_on = 1;
	} else {
		get_min_lux_table(&(msd.mpd->gamma_initial[2]),
					GAMMA_SET_MAX);
		reset_gamma_level();
	}
#endif

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
	if (!msd.dstat.is_smart_dim_loaded) {
		/* Load MTP Data */
		int i, mtp_cnt, err_cnt;
		char *mtp_data = (char *)&(msd.mpd->smart_s6e63m0.MTP);

		for (err_cnt = 0; err_cnt < 10; err_cnt++) {
			mtp_cnt = find_mtp(mfd, mtp_data);

			if (mtp_cnt != 0)
				break;
		}

		pr_info("%s MTP is determined:%d err_cnt:%d",
					__func__, mtp_cnt, err_cnt);

		for (i = 0; i < MTP_DATA_SIZE_S6E63M0; i++) {
			pr_info("%s MTP DATA[%d] : %02x\n", __func__, i,
				mtp_data[i]);
		}

		smart_dimming_init(&(msd.mpd->smart_s6e63m0));

		msd.dstat.is_smart_dim_loaded = true;
		msd.dstat.gamma_mode = GAMMA_SMART;
	}

	if (msd.mpd->gamma_initial && boot_on == 0) {
		msd.mpd->smart_s6e63m0.brightness_level = 140;
		generate_gamma(&msd.mpd->smart_s6e63m0,
			&(msd.mpd->gamma_initial[2]), GAMMA_SET_MAX);

		if (recovery_boot_mode == 0)
			boot_on = 1;

	} else {
		msd.mpd->smart_s6e63m0.brightness_level = 30;
		generate_gamma(&msd.mpd->smart_s6e63m0,
			&(msd.mpd->gamma_initial[2]), GAMMA_SET_MAX);
		reset_gamma_level();
	}
#endif

#if defined(CONFIG_FB_MSM_MIPI_MAGNA_OLED_VIDEO_WVGA_PT)
	if (!msd.dstat.is_smart_dim_loaded) {
		/* Load MTP Data */
		int i, mtp_cnt, err_cnt;
		char *mtp_data = (char *)&(msd.mpd->smart_ea8868.MTP);

		if (bypass_lcd_id == 0x4a) {
			for (err_cnt = 0; err_cnt < 10; err_cnt++) {
				mtp_cnt = find_mtp(mfd, mtp_data);

				if (mtp_cnt != 0)
					break;
			}

			pr_info("%s MTP is determined:%d err_cnt:%d",
						__func__, mtp_cnt, err_cnt);

			for (i = 0; i < MTP_DATA_SIZE_EA8868; i++) {
				pr_info("%s MTP DATA[%d] : %02x\n",
						__func__, i, mtp_data[i]);
			}
		} else {
			pr_info("%s MTP is not used LDI_ID: 0x%x",
						__func__, bypass_lcd_id);
			memset(mtp_data, 0x0, MTP_DATA_SIZE_EA8868);
		}

		smart_dimming_init(&(msd.mpd->smart_ea8868));

		msd.dstat.is_smart_dim_loaded = true;
		msd.dstat.gamma_mode = GAMMA_SMART;
	}

	if (msd.mpd->gamma_initial && boot_on == 0) {
		msd.mpd->smart_ea8868.brightness_level = 180;
		generate_gamma(&msd.mpd->smart_ea8868,
			&(msd.mpd->gamma_initial[1]), GAMMA_SET_MAX);

		if (recovery_boot_mode == 0)
			boot_on = 1;
	} else {
		msd.mpd->smart_ea8868.brightness_level = get_gamma_lux();
		generate_gamma(&msd.mpd->smart_ea8868,
			&(msd.mpd->gamma_initial[1]), GAMMA_SET_MAX);
		reset_gamma_level();
	}
#endif

	if (unlikely(first_on)) {
		first_on = false;
		return 0;
	}

	mipi_samsung_disp_send_cmd(mfd, PANEL_READY_TO_ON, false);
	if (mipi->mode == DSI_VIDEO_MODE)
		mipi_samsung_disp_send_cmd(mfd, PANEL_ON, false);

#if !defined(CONFIG_HAS_EARLYSUSPEND)
	mipi_samsung_disp_send_cmd(mfd, PANEL_LATE_ON, false);
#endif
#if defined(CONFIG_MIPI_SAMSUNG_ESD_REFRESH)
#if defined(CONFIG_MACH_JAGUAR)
	if  (system_rev >= 16)
		set_esd_enable();
	if (msd.esd_refresh == true)
		mipi_samsung_disp_send_cmd(mfd, PANEL_LATE_ON, false);
#else
	set_esd_enable();
#endif
#endif

#ifdef READ_REGISTER_ESD
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
	id2 = (msd.mpd->manufacture_id>>8) & 0xFF;

	if ((id2 == 0xA6) || (id2 == 0xB6)) {
		queue_delayed_work(msd.mpd->esd_workqueue,
				&(msd.mpd->esd_work), ESD_INTERVAL * HZ);
		pr_info("%s ESD FUNCTION QUEUED", __func__);
	} else
		pr_info("%s ESD FUNCTION NOT QUEUED", __func__);

	wake_lock(&(msd.mpd->esd_wake_lock));
#else
	queue_delayed_work(msd.mpd->esd_workqueue,
				&(msd.mpd->esd_work), ESD_INTERVAL * HZ);
	wake_lock(&(msd.mpd->esd_wake_lock));
#endif
#endif

	return 0;
}

static int mipi_samsung_disp_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

#ifdef READ_REGISTER_ESD
	cancel_delayed_work(&(msd.mpd->esd_work));
#endif

#if defined(CONFIG_MIPI_SAMSUNG_ESD_REFRESH)
#if defined(CONFIG_MACH_JAGUAR)
	if  (system_rev >= 16)
		set_esd_disable();
#else
	set_esd_disable();
#endif

#endif
	mfd = platform_get_drvdata(pdev);
	if (unlikely(!mfd))
		return -ENODEV;
	if (unlikely(mfd->key != MFD_KEY))
		return -EINVAL;

	mipi_samsung_disp_send_cmd(mfd, PANEL_READY_TO_OFF, false);
#if !defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
	mipi_samsung_disp_send_cmd(mfd, PANEL_OFF, false);
#endif

	msd.mpd->ldi_acl_stat = false;
	mfd->resume_state = MIPI_SUSPEND_STATE;

	return 0;
}

static void __devinit mipi_samsung_disp_shutdown(struct platform_device *pdev)
{
	static struct mipi_dsi_platform_data *mipi_dsi_pdata;

	if (pdev->id != 0)
		return;

	 mipi_dsi_pdata = pdev->dev.platform_data;
	if (mipi_dsi_pdata == NULL) {
		pr_err("LCD Power off failure: No Platform Data\n");
		return;
	}

/* Power off Seq:2: Sleepout mode->ResetLow -> delay 120ms->VCI,VDD off */
	if (mipi_dsi_pdata &&  mipi_dsi_pdata->dsi_power_save) {
		mipi_dsi_pdata->lcd_rst_down();
		mdelay(120);
		pr_info("LCD POWER OFF\n");
	}

}
#if defined(CONFIG_MIPI_SAMSUNG_ESD_REFRESH)
void set_esd_refresh(boolean stat)
{
	msd.esd_refresh = stat;
}
boolean get_esd_refresh_stat(void)
{
	return msd.esd_refresh;
}
#endif

static void mipi_samsung_disp_backlight(struct msm_fb_data_type *mfd)
{

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT)
	mfd->backlight_ctrl_ongoing = TRUE;
#else
	mfd->backlight_ctrl_ongoing = FALSE;
#endif

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_HD_PT) \
	|| defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
if (msd.mpd->prepare_brightness_control_cmd_array) {
	int cmds_sent;

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_HD_PT)
	if (!samsung_has_cmc624())
		cmds_sent = msd.mpd->prepare_brightness_control_cmd_array(
					bypass_LCD_Id(), mfd->bl_level);
	else
		cmds_sent = msd.mpd->prepare_brightness_control_cmd_array(
					LCD_Get_Value(), mfd->bl_level);
#else
		cmds_sent = msd.mpd->prepare_brightness_control_cmd_array(
					0, mfd->bl_level);
#endif
	pr_debug("cmds_sent: %x\n", cmds_sent);
	if (cmds_sent < 0)
		goto end;

	mipi_samsung_disp_send_cmd(mfd, PANEL_BRIGHT_CTRL, true);
	goto end;
}
#endif

#if defined(CONFIG_MIPI_SAMSUNG_ESD_REFRESH)
	if (msd.esd_refresh == true)
		goto end;
#endif
	pr_info("mipi_samsung_disp_backlight %d\n", mfd->bl_level);
	if (!msd.mpd->set_gamma ||  !mfd->panel_power_on ||\
		mfd->resume_state == MIPI_SUSPEND_STATE)
		goto end;

	if (msd.mpd->set_gamma(mfd->bl_level, msd.dstat.gamma_mode) < 0)
		goto end;

	pr_info("mipi_samsung_disp_backlight %d\n", mfd->bl_level);
	mipi_samsung_disp_send_cmd(mfd, PANEL_GAMMA_UPDATE, true);

#ifdef USE_ELVSS
if (msd.mpd->set_elvss)
	mipi_samsung_disp_send_cmd(mfd, PANEL_ELVSS_UPDATE, true);
#endif

#ifdef USE_ACL
if (msd.mpd->set_acl && (msd.dstat.acl_on || msd.dstat.siop_status) && msd.mpd->set_acl(mfd->bl_level))
	mipi_samsung_disp_send_cmd(mfd, PANEL_ACL_OFF, true);

if (msd.mpd->set_acl && (msd.dstat.acl_on || msd.dstat.siop_status) && !msd.mpd->set_acl(mfd->bl_level)) {
#if !defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT)
	mipi_samsung_disp_send_cmd(mfd, PANEL_ACL_ON, true);
#endif
	mipi_samsung_disp_send_cmd(mfd, PANEL_ACL_UPDATE, true);
}
#endif

end:
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT)
	mfd->backlight_ctrl_ongoing = FALSE;
#endif
	return;
}

int  mipi_samsung_disp_blank(struct platform_device *pdev,int blank)
{
	static int curr_status;
	struct msm_fb_data_type *mfd;
	
	mfd = platform_get_drvdata(pdev);
	if (unlikely(!mfd))
		return -ENODEV;
	if (unlikely(mfd->key != MFD_KEY))
		return -EINVAL;
	
	pr_info("%s(%d)", __func__, blank);
	if (curr_status == blank)
		return 0;

	pr_info("%s(%d)++", __func__, blank);
	if (!blank) {
		#if !defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_HD_PT) && \
			!defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
			mipi_samsung_disp_send_cmd(mfd, PANEL_LATE_ON, true);
		#endif
	}
	else {
		#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
			mipi_samsung_disp_send_cmd(mfd, PANEL_OFF, false);
		#endif
		
		#if !defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_HD_PT)
			mipi_samsung_disp_send_cmd(mfd, PANEL_EARLY_OFF, true);
		#else
			mipi_samsung_disp_send_cmd(mfd, PANEL_READY_TO_OFF, true);
		#endif
		mdelay(20);
	}

	curr_status = blank;
	return 0;
}

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void mipi_samsung_disp_early_suspend(struct early_suspend *h)
{
	struct msm_fb_data_type *mfd;
	pr_info("%s", __func__);

#ifdef READ_REGISTER_ESD
	mfd = platform_get_drvdata(msd.msm_pdev);
	if (unlikely(!mfd)) {
		pr_info("%s NO PDEV.\n", __func__);
		return;
	}
	if (unlikely(mfd->key != MFD_KEY)) {
		pr_info("%s MFD_KEY is not matched.\n", __func__);
		return;
	}

	mfd->resume_state = MIPI_SUSPEND_STATE;
	cancel_delayed_work_sync(&(msd.mpd->esd_work));
	wake_unlock(&(msd.mpd->esd_wake_lock));

#if defined(CONFIG_MIPI_SAMSUNG_ESD_REFRESH)
		set_esd_disable();
#endif
#else
#if defined(CONFIG_MIPI_SAMSUNG_ESD_REFRESH)
	set_esd_disable();
#endif

	mfd = platform_get_drvdata(msd.msm_pdev);
	if (unlikely(!mfd)) {
		pr_info("%s NO PDEV.\n", __func__);
		return;
	}
	if (unlikely(mfd->key != MFD_KEY)) {
		pr_info("%s MFD_KEY is not matched.\n", __func__);
		return;
	}
#endif
	mfd->resume_state = MIPI_SUSPEND_STATE;
}

static void mipi_samsung_disp_late_resume(struct early_suspend *h)
{
	struct msm_fb_data_type *mfd;
#if defined(CONFIG_FB_MDP4_ENHANCE)
	is_negativeMode_on();
#endif
	mfd = platform_get_drvdata(msd.msm_pdev);
	if (unlikely(!mfd)) {
		pr_info("%s NO PDEV.\n", __func__);
		return;
	}
	if (unlikely(mfd->key != MFD_KEY)) {
		pr_info("%s MFD_KEY is not matched.\n", __func__);
		return;
	}

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT) \
	|| defined(CONFIG_FB_MSM_MIPI_MAGNA_OLED_VIDEO_WVGA_PT)
	reset_gamma_level();
	mfd->resume_state = MIPI_RESUME_STATE;
	mipi_samsung_disp_backlight(mfd);
#endif

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT)
	mfd->fbi->fbops->fb_blank(FB_BLANK_UNBLANK, mfd->fbi);
	mfd->fbi->fbops->fb_pan_display(&mfd->fbi->var, mfd->fbi);
#endif

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_HD_PT) \
	|| defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT) \
	|| defined(CONFIG_FB_MSM_MIPI_MAGNA_OLED_VIDEO_QHD_PT) \
	|| defined(CONFIG_FB_MSM_MIPI_MAGNA_OLED_VIDEO_WVGA_PT)
	mfd->resume_state = MIPI_RESUME_STATE;
#endif
	mipi_samsung_disp_blank(msd.msm_pdev, 0);
	pr_info("%s", __func__);
}
#endif

#if defined(CONFIG_LCD_CLASS_DEVICE)
#ifdef WA_FOR_FACTORY_MODE
static ssize_t mipi_samsung_disp_get_power(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct msm_fb_data_type *mfd;
	int rc;

	mfd = platform_get_drvdata(msd.msm_pdev);
	if (unlikely(!mfd))
		return -ENODEV;
	if (unlikely(mfd->key != MFD_KEY))
		return -EINVAL;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n", mfd->panel_power_on);
	pr_info("mipi_samsung_disp_get_power(%d)\n", mfd->panel_power_on);

	return rc;
}

static ssize_t mipi_samsung_disp_set_power(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd;
	unsigned int power;

	mfd = platform_get_drvdata(msd.msm_pdev);

	if (sscanf(buf, "%u", &power) != 1)
		return -EINVAL;

	if (power == mfd->panel_power_on)
		return 0;

	if (power) {
		mfd->fbi->fbops->fb_blank(FB_BLANK_UNBLANK, mfd->fbi);
		mfd->fbi->fbops->fb_pan_display(&mfd->fbi->var, mfd->fbi);
		mipi_samsung_disp_send_cmd(mfd, PANEL_LATE_ON, true);
		mipi_samsung_disp_backlight(mfd);
	} else {
		mfd->fbi->fbops->fb_blank(FB_BLANK_POWERDOWN, mfd->fbi);
	}

	pr_info("mipi_samsung_disp_set_power\n");

	return size;
}
#else
static int mipi_samsung_disp_get_power(struct lcd_device *dev)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(msd.msm_pdev);
	if (unlikely(!mfd))
		return -ENODEV;
	if (unlikely(mfd->key != MFD_KEY))
		return -EINVAL;

	pr_info("mipi_samsung_disp_get_power(%d)\n", mfd->panel_power_on);

	return mfd->panel_power_on;
}

static int mipi_samsung_disp_set_power(struct lcd_device *dev, int power)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(msd.msm_pdev);

	if (power == mfd->panel_power_on)
		return 0;

	if (power) {
		mfd->fbi->fbops->fb_blank(FB_BLANK_UNBLANK, mfd->fbi);
		mfd->fbi->fbops->fb_pan_display(&mfd->fbi->var, mfd->fbi);
		mipi_samsung_disp_send_cmd(mfd, PANEL_LATE_ON, true);
		mipi_samsung_disp_backlight(mfd);
	} else {
		mfd->fbi->fbops->fb_blank(FB_BLANK_POWERDOWN, mfd->fbi);
	}

	pr_info("mipi_samsung_disp_set_power\n");
	return 0;
}
#endif

static ssize_t mipi_samsung_disp_lcdtype_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char temp[20];

	snprintf(temp, strnlen(msd.mpd->panel_name, 20) + 1,
						msd.mpd->panel_name);
	strncat(buf, temp, 20);
	return strnlen(buf, 20);
}

static ssize_t mipi_samsung_disp_gamma_mode_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n", msd.dstat.gamma_mode);
	pr_info("gamma_mode: %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_disp_gamma_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	if (sysfs_streq(buf, "1") && !msd.dstat.gamma_mode) {
		/* 1.9 gamma */
		msd.dstat.gamma_mode = GAMMA_1_9;
	} else if (sysfs_streq(buf, "0") && msd.dstat.gamma_mode) {
		/* 2.2 gamma */
		msd.dstat.gamma_mode = GAMMA_2_2;
	} else {
		pr_info("%s: Invalid argument!!", __func__);
	}

	return size;
}

static ssize_t mipi_samsung_disp_acl_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n", msd.dstat.acl_on);
	pr_info("acl status: %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_disp_acl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(msd.msm_pdev);

	if (!mfd->panel_power_on) {
		pr_info("%s: panel is off state. updating state value.\n",
			__func__);
		if (sysfs_streq(buf, "1") && !msd.dstat.acl_on)
			msd.dstat.acl_on = true;
		else if (sysfs_streq(buf, "0") && msd.dstat.acl_on)
			msd.dstat.acl_on = false;
		else
			pr_info("%s: Invalid argument!!", __func__);
	} else {
		if (sysfs_streq(buf, "1") && !msd.dstat.acl_on) {
			if (msd.mpd->set_acl(mfd->bl_level))
				mipi_samsung_disp_send_cmd(
					mfd, PANEL_ACL_OFF, true);
			else {
				mipi_samsung_disp_send_cmd(
					mfd, PANEL_ACL_ON, true);
				mipi_samsung_disp_send_cmd(
					mfd, PANEL_ACL_UPDATE, true);
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_HD_PT)
				if (samsung_has_cmc624())
					cabc_onoff_ctrl(1);
#endif
			}
			msd.dstat.acl_on = true;
		} else if (sysfs_streq(buf, "0") && msd.dstat.acl_on) {
			mipi_samsung_disp_send_cmd(mfd, PANEL_ACL_OFF, true);
			msd.dstat.acl_on = false;
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_HD_PT)
			if (samsung_has_cmc624())
				cabc_onoff_ctrl(0);
#endif
		} else {
			pr_info("%s: Invalid argument!!", __func__);
		}
	}

	return size;
}

static ssize_t mipi_samsung_auto_brightness_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n",
					msd.dstat.auto_brightness);
	pr_info("auot_brightness: %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_auto_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	if (sysfs_streq(buf, "0"))
		msd.dstat.auto_brightness = 0;
	else if (sysfs_streq(buf, "1"))
		msd.dstat.auto_brightness = 1;
	else if (sysfs_streq(buf, "2"))
		msd.dstat.auto_brightness = 2;
	else if (sysfs_streq(buf, "3"))
		msd.dstat.auto_brightness = 3;	
	else if (sysfs_streq(buf, "4"))
		msd.dstat.auto_brightness = 4;
	else if (sysfs_streq(buf, "5"))
		msd.dstat.auto_brightness = 5;	
	else
		pr_info("%s: Invalid argument!!", __func__);

	return size;
}

#ifdef CONFIG_SAMSUNG_CMC624
void mipi_samsung_oled_display_fast_init(void)
{
	struct msm_fb_data_type *mfd;
	if (samsung_has_cmc624())
		msd.mpd->prepare_fast_cmd_array(LCD_Get_Value());
	else
		msd.mpd->prepare_fast_cmd_array(bypass_LCD_Id());
	mfd = platform_get_drvdata(msd.msm_pdev);
	mipi_samsung_disp_send_cmd(mfd, PANEL_READY_TO_ON_FAST, false);
	mipi_samsung_disp_send_cmd(mfd, PANEL_ON, false);
}
#endif

static struct lcd_ops mipi_samsung_disp_props = {
#ifdef WA_FOR_FACTORY_MODE
	.get_power = NULL,
	.set_power = NULL,
#else
	.get_power = mipi_samsung_disp_get_power,
	.set_power = mipi_samsung_disp_set_power,
#endif
};

static ssize_t mipi_samsung_disp_siop_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n", msd.dstat.siop_status);
	pr_info("siop status: %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_disp_siop_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(msd.msm_pdev);

	if (sysfs_streq(buf, "1"))
		msd.dstat.siop_status = true;
	else if (sysfs_streq(buf, "0"))
		msd.dstat.siop_status = false;
	else {
		pr_info("%s: Invalid argument!!", __func__);
		return size;
	}

	if (mfd->panel_power_on) {
		if (msd.dstat.siop_status) {
			if (msd.mpd->set_acl(mfd->bl_level))
				mipi_samsung_disp_send_cmd(
					mfd, PANEL_ACL_OFF, true);
			else {
				mipi_samsung_disp_send_cmd(
					mfd, PANEL_ACL_ON, true);
				mipi_samsung_disp_send_cmd(
					mfd, PANEL_ACL_UPDATE, true);
			}
		}
		else
			mipi_samsung_disp_send_cmd(mfd, PANEL_ACL_OFF, true);
	}
	else
		pr_info("%s : panel is off state. updating state value.\n", __func__);

	pr_info("%s : acl_status (%d) siop_status (%d)",
			__func__, msd.dstat.acl_on, msd.dstat.siop_status);

	return size;
}

#ifdef WA_FOR_FACTORY_MODE
static DEVICE_ATTR(lcd_power, S_IRUGO | S_IWUSR,
			mipi_samsung_disp_get_power,
			mipi_samsung_disp_set_power);
#endif
static DEVICE_ATTR(lcd_type, S_IRUGO, mipi_samsung_disp_lcdtype_show, NULL);
static DEVICE_ATTR(gamma_mode, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_disp_gamma_mode_show,
			mipi_samsung_disp_gamma_mode_store);
static DEVICE_ATTR(power_reduce, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_disp_acl_show,
			mipi_samsung_disp_acl_store);
static DEVICE_ATTR(auto_brightness, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_auto_brightness_show,
			mipi_samsung_auto_brightness_store);

static DEVICE_ATTR(siop_enable, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_disp_siop_show,
			mipi_samsung_disp_siop_store);

#endif


#ifdef READ_REGISTER_ESD
#define ID_E5H_IDLE 0x80
#define ID_E5H_IDLE_2 0x84
#define ID_E5H_IDLE_3 0x81
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
char error_id2_cmd[8] = {
	0x00, 0x00, 0x09, 0x40, 0xE5, 0x00, 0x06, 0xA0};
#else
static char error_id2[2] = {0xE5, 0x00}; /* DTYPE_DCS_READ */

static struct dsi_cmd_desc error_id2_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 0, sizeof(error_id2), error_id2};
#endif
static char error_buf[2];

static void read_error_register(struct msm_fb_data_type *mfd)
{
	struct dsi_buf *rp, *tp;
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
	char *cmd_lp;
#else
	struct dsi_cmd_desc *cmd;
#endif

	wake_lock(&idle_wake_lock);

#if CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT
	mutex_lock(&mipi_lp_mutex);
#endif
	mutex_lock(&mfd->dma->ov_mutex);

	mipi_dsi_mdp_busy_wait();	

	tp = &msd.samsung_tx_buf;
	rp = &msd.samsung_rx_buf;

	mipi_dsi_buf_init(rp);
	mipi_dsi_buf_init(tp);

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)

	cmd_lp = error_id2_cmd;

	mipi_dsi_cmds_rx_lp(mfd, tp, rp, cmd_lp, 1);
	error_buf[0] = *rp->data;
	mipi_dsi_cmds_rx_lp(mfd, tp, rp, cmd_lp, 1);

	pr_debug("############ error buf E5 = %x\n", error_buf[0]);
#else
	cmd = &error_id2_cmd;
	mipi_dsi_cmds_rx(mfd, tp, rp, cmd, 1);
	error_buf[0] = *rp->data;
	pr_debug("############ error buf E5 %x\n", error_buf[0]);
#endif

	mutex_unlock(&mfd->dma->ov_mutex);
#if CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT
	mutex_unlock(&mipi_lp_mutex);
#endif
	wake_unlock(&idle_wake_lock);
}

static void esd_test_work_func(struct work_struct *work)
{
	static unsigned int esd_cnt;
	struct msm_fb_data_type *mfd;
	mfd = platform_get_drvdata(msd.msm_pdev);

	if (unlikely(!mfd)) {
		pr_info("%s NO PDEV.\n", __func__);
		return;
	}
#if !defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
	return;
#endif
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
	read_error_register(mfd);
	pr_info("%s E5H=0x%x esd_cnt:%d\n", __func__, error_buf[0], esd_cnt);
#else
	read_error_register(mfd);
	pr_info("%s end E5H=0x%x\n", __func__,
			error_buf[0]);
#endif
	if (ID_E5H_IDLE != error_buf[0]) {
		if ((ID_E5H_IDLE_2 != error_buf[0]) && \
			(error_buf[0] != ID_E5H_IDLE_3)) {
			pr_info("%s: E5H=%x 0AH=%x\n", __func__,
				error_buf[0], error_buf[1]);
			esd_execute();

			esd_cnt++;
		}
	}

	if (mfd->resume_state != MIPI_SUSPEND_STATE)
		queue_delayed_work(msd.mpd->esd_workqueue,
				&(msd.mpd->esd_work), ESD_INTERVAL * HZ);

}
#endif

static int __devinit mipi_samsung_disp_probe(struct platform_device *pdev)
{
	int ret;
	struct platform_device *msm_fb_added_dev;
#if defined(CONFIG_LCD_CLASS_DEVICE)
	struct lcd_device *lcd_device;
#endif
#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	struct backlight_device *bd = NULL;
#endif
	msd.dstat.acl_on = false;

	if (pdev->id == 0) {
		msd.mipi_samsung_disp_pdata = pdev->dev.platform_data;
#ifdef CONFIG_SAMSUNG_CMC624
	if (samsung_has_cmc624()) {
		printk(KERN_DEBUG "Is_There_cmc624 : CMC624 is there!!!!");
		samsung_cmc624_init();
		first_on = false;
	} else {
		printk(KERN_DEBUG "Is_There_cmc624 : CMC624 is not there!!!!");
		first_on = false;
	}
#else
		first_on = false;
#endif
		return 0;
	}

	msm_fb_added_dev = msm_fb_add_device(pdev);

#if defined(CONFIG_HAS_EARLYSUSPEND) || defined(CONFIG_LCD_CLASS_DEVICE)
	msd.msm_pdev = msm_fb_added_dev;
#endif

#if defined(CONFIG_HAS_EARLYSUSPEND)
	msd.early_suspend.suspend = mipi_samsung_disp_early_suspend;
	msd.early_suspend.resume = mipi_samsung_disp_late_resume;
	msd.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	register_early_suspend(&msd.early_suspend);

#endif

#if defined(CONFIG_LCD_CLASS_DEVICE)
	lcd_device = lcd_device_register("panel", &pdev->dev, NULL,
					&mipi_samsung_disp_props);

	if (IS_ERR(lcd_device)) {
		ret = PTR_ERR(lcd_device);
		printk(KERN_ERR "lcd : failed to register device\n");
		return ret;
	}

#ifdef WA_FOR_FACTORY_MODE
	sysfs_remove_file(&lcd_device->dev.kobj,
					&dev_attr_lcd_power.attr);

	ret = sysfs_create_file(&lcd_device->dev.kobj,
					&dev_attr_lcd_power.attr);
	if (ret) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_lcd_power.attr.name);
	}
#endif

	ret = sysfs_create_file(&lcd_device->dev.kobj,
					&dev_attr_lcd_type.attr);
	if (ret) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_lcd_type.attr.name);
	}

	ret = sysfs_create_file(&lcd_device->dev.kobj,
					&dev_attr_gamma_mode.attr);
	if (ret) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_gamma_mode.attr.name);
	}

	ret = sysfs_create_file(&lcd_device->dev.kobj,
					&dev_attr_power_reduce.attr);
	if (ret) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_power_reduce.attr.name);
	}

	ret = sysfs_create_file(&lcd_device->dev.kobj,
					&dev_attr_siop_enable.attr);
	if (ret) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_siop_enable.attr.name);
	}

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	bd = backlight_device_register("panel", &lcd_device->dev,
						NULL, NULL, NULL);
	if (IS_ERR(bd)) {
		ret = PTR_ERR(bd);
		pr_info("backlight : failed to register device\n");
		return ret;
	}

	ret = sysfs_create_file(&bd->dev.kobj,
					&dev_attr_auto_brightness.attr);
	if (ret) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_auto_brightness.attr.name);
	}
#endif
#endif
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_HD_PT)
	if (samsung_has_cmc624()) {
		ret = cmc624_sysfs_init();
		if (ret < 0)
			pr_debug("CMC624 sysfs initialize FAILED\n");
	} else {
#endif /* CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_HD_PT */
#if defined(CONFIG_FB_MDP4_ENHANCE)
		init_mdnie_class();
#endif /* CONFIG_FB_MDP4_ENHANCE */
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_HD_PT)
	}
#endif /* CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_HD_PT */

#ifdef READ_REGISTER_ESD
	msd.mpd->esd_workqueue = create_singlethread_workqueue("esd_workqueue");

	if (!msd.mpd->esd_workqueue) {
		pr_info("esd_workqueue create fail\n");
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&(msd.mpd->esd_work), esd_test_work_func);

	wake_lock_init(&(msd.mpd->esd_wake_lock),
		 WAKE_LOCK_SUSPEND, "esd_workqueue_lock");
#endif

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mipi_samsung_disp_probe,
	.driver = {
		.name   = "mipi_samsung_oled",
	},
	.shutdown = mipi_samsung_disp_shutdown
};

static struct msm_fb_panel_data samsung_panel_data = {
	.on		= mipi_samsung_disp_on,
	.off		= mipi_samsung_disp_off,
	.set_backlight	= mipi_samsung_disp_backlight,
#ifdef MIPI_SAMSUNG_PANEL_BLANKING
	.panel_blank = mipi_samsung_disp_blank,
#endif /* MIPI_SAMSUNG_PANEL_BLANKING */
};

static int ch_used[3];

int mipi_samsung_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel,
					struct mipi_panel_data *mpd)
{
	struct platform_device *pdev = NULL;
	int ret = 0;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	pdev = platform_device_alloc("mipi_samsung_oled",
					   (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	samsung_panel_data.panel_info = *pinfo;
	msd.mpd = mpd;
	if (!msd.mpd) {
		printk(KERN_ERR
		  "%s: get mipi_panel_data failed!\n", __func__);
		goto err_device_put;
	}
	mpd->msd = &msd;
	ret = platform_device_add_data(pdev, &samsung_panel_data,
		sizeof(samsung_panel_data));
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

	return ret;

err_device_put:
	platform_device_put(pdev);
	return ret;
}

int get_lcd_attached(void)
{
	return is_lcd_connected;
}
EXPORT_SYMBOL(get_lcd_attached);

static int __init current_boot_mode(char *mode)
{
	/*
	*	1 is recovery booting
	*	0 is normal booting
	*/

	if (strncmp(mode, "1", 1) == 0)
		recovery_boot_mode = 1;
	else
		recovery_boot_mode = 0;

	pr_debug("%s %s", __func__, recovery_boot_mode == 1 ?
						"recovery" : "normal");
	return 1;
}
__setup("androidboot.boot_recovery=", current_boot_mode);

static int __init mipi_samsung_disp_init(void)
{
	mipi_dsi_buf_alloc(&msd.samsung_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&msd.samsung_rx_buf, DSI_BUF_SIZE);

	wake_lock_init(&idle_wake_lock, WAKE_LOCK_IDLE, "MIPI idle lock");

	return platform_driver_register(&this_driver);
}
module_init(mipi_samsung_disp_init);
