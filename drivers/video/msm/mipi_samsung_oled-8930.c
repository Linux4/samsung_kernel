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
#include <linux/gpio.h>
#include <mach/msm8930-gpio.h>

#include "mipi_samsung_oled-8930.h"
#include "mdp4.h"

#if defined(CONFIG_FB_MDP4_ENHANCE)
#include "mdp4_video_enhance.h"
#elif defined(CONFIG_MDNIE_LITE_TUNING)
#include "mdnie_lite_tuning.h"
#endif

#define DDI_VIDEO_ENHANCE_TUNING
#if defined(DDI_VIDEO_ENHANCE_TUNING)
#include <linux/syscalls.h>
#endif

static struct mipi_samsung_driver_data msd;
static unsigned int recovery_boot_mode;

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
struct work_struct  err_fg_work;
static int err_fg_gpio;	/* GPIO 8 */
static int esd_count;
static int err_fg_working;
#endif

#define WA_FOR_FACTORY_MODE
#define READ_MTP_ONCE

unsigned char bypass_lcd_id;
static char elvss_value;
int is_lcd_connected = 1;
struct mutex dsi_tx_mutex;
#if defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT)
static int lcd_attached = 1;
#endif

#if defined(CONFIG_RUNTIME_MIPI_CLK_CHANGE)
static int current_fps;
#endif

#ifdef USE_READ_ID

#ifdef CONFIG_MIPI_DSI_LP_RX
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

#ifdef CONFIG_MIPI_DSI_LP_RX
	char *cmd;
#else
	struct dsi_cmd_desc *cmd;
#endif
	uint32 id;

	mutex_lock(&dsi_tx_mutex);

	tp = &msd.samsung_tx_buf;
	rp = &msd.samsung_rx_buf;
	mipi_dsi_buf_init(rp);
	mipi_dsi_buf_init(tp);

	cmd = &samsung_manufacture_id1_cmd;
#ifdef CONFIG_MIPI_DSI_LP_RX
	mipi_dsi_cmds_rx_lp(mfd, tp, rp, cmd, 1);
#else
	mipi_dsi_cmds_rx(mfd, tp, rp, cmd, 1);
#endif

	pr_info("%s: manufacture_id1=%x\n", __func__, *rp->data);
	id = *((uint8 *)rp->data);
	id <<= 8;

	mipi_dsi_buf_init(rp);
	mipi_dsi_buf_init(tp);

	cmd = &samsung_manufacture_id2_cmd;
#ifdef CONFIG_MIPI_DSI_LP_RX
	mipi_dsi_cmds_rx_lp(mfd, tp, rp, cmd_lp, 1);
#else
	mipi_dsi_cmds_rx(mfd, tp, rp, cmd, 1);
#endif

	pr_info("%s: manufacture_id2=%x\n", __func__, *rp->data);
	bypass_lcd_id = *rp->data;
	id |= *((uint8*)rp->data);
	id <<= 8;

	mipi_dsi_buf_init(rp);
	mipi_dsi_buf_init(tp);

	cmd = &samsung_manufacture_id3_cmd;
#ifdef CONFIG_MIPI_DSI_LP_RX
	mipi_dsi_cmds_rx_lp(mfd, tp, rp, cmd_lp, 1);
#else
	mipi_dsi_cmds_rx(mfd, tp, rp, cmd, 1);
#endif

	pr_info("%s: manufacture_id3=%x\n", __func__, *rp->data);
	elvss_value = *rp->data;
	id |= *((uint8 *)rp->data);

	pr_info("%s: manufacture_id=%x\n", __func__, id);

#ifdef FACTORY_TEST
	if (id == 0x00) {
		pr_info("Lcd is not connected\n");
		is_lcd_connected = 0;
	}
#endif
	mutex_unlock(&dsi_tx_mutex);
	return id;
}
#endif

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
unsigned int get_lcd_manufacture_id(void)
{
	return msd.mpd->manufacture_id;
}
#endif

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT)\
	|| defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
	static void read_reg(char srcReg, int srcLength, char *destBuffer,
	      const int isUseMutex, struct msm_fb_data_type *pMFD)
{
#if defined(CONFIG_MIPI_DSI_LP_RX)
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
	mdelay(20);
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
	mdelay(20);
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

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
static int find_mtp(struct msm_fb_data_type *mfd, char * mtp)
{

	char first_mtp[MTP_DATA_SIZE_S6E63M0];
	char second_mtp[MTP_DATA_SIZE_S6E63M0];
	char third_mtp[MTP_DATA_SIZE_S6E63M0];
	int mtp_size = MTP_DATA_SIZE_S6E63M0;

	int correct_mtp;

	pr_info("first time mtp read\n");
	read_reg(MTP_REGISTER, mtp_size, first_mtp, FALSE, mfd);

	pr_info("second time mtp read\n");
	read_reg(MTP_REGISTER, mtp_size, second_mtp, FALSE, mfd);

	if (memcmp(first_mtp, second_mtp, mtp_size) != 0) {
		pr_info("third time mtp read\n");
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

	/*
	*	checking V1 MTP value. V0 value must be 0.
	*/
	if ((mtp[0] != 0) || (mtp[7] != 0) || (mtp[14] != 0)) {
		memset(mtp, 0, mtp_size);
		correct_mtp = 0;
	}
	return correct_mtp;
}
#endif

static int mipi_samsung_disp_send_cmd(struct msm_fb_data_type *mfd,
		enum mipi_samsung_cmd_list cmd,
		unsigned char lock)
{
	struct dsi_cmd_desc *cmd_desc;
	struct dcs_cmd_req cmdreq;
	int cmd_size = 0;

#ifdef CMD_DEBUG
	int i,j;
#endif

	pr_info("%s cmd = 0x%x\n", __func__, cmd);

#ifdef CONFIG_MIPI_DSI_LP_RX
	mutex_lock(&mipi_lp_mutex);
#endif
	if (mfd->panel.type == MIPI_VIDEO_PANEL)
		mutex_lock(&dsi_tx_mutex);
	else {
		if (lock)
			mutex_lock(&mfd->dma->ov_mutex);
	}

		switch (cmd) {
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT) \
	|| defined(CONFIG_FB_MSM_MIPI_AMS367_OLED_VIDEO_WVGA_PT_PANEL)
		case PANEL_ON:
			cmd_desc = msd.mpd->on.cmd;
			cmd_size = msd.mpd->on.size;
			break;
		
		case PANEL_OFF:
			cmd_desc = msd.mpd->off.cmd;
			cmd_size = msd.mpd->off.size;
			break;
#if 0	/*temp*/			
		case PANEL_LATE_ON:
			cmd_desc = msd.mpd->late_on.cmd;
			cmd_size = msd.mpd->late_on.size;
			break;
		case PANEL_EARLY_OFF:
			cmd_desc = msd.mpd->early_off.cmd;
			cmd_size = msd.mpd->early_off.size;
			break;
#endif				
		case PANEL_BRIGHT_CTRL:
			cmd_desc = msd.mpd->brightness.cmd;
			cmd_size = msd.mpd->brightness.size;
			break;
		
		case PANEL_MTP_ENABLE:
			cmd_desc = msd.mpd->mtp_enable.cmd;
			cmd_size = msd.mpd->mtp_enable.size;
			break;
		case PANEL_MTP_DISABLE:
			cmd_desc = msd.mpd->mtp_disable.cmd;
			cmd_size = msd.mpd->mtp_disable.size;
			break;
		case PANEL_NEED_FLIP:
			cmd_desc = msd.mpd->need_flip.cmd;
			cmd_size = msd.mpd->need_flip.size;
			break;
		case PANEL_ACL_CONTROL:
			cmd_desc = msd.mpd->acl_cmds.cmd;
			cmd_size = msd.mpd->acl_cmds.size;
			break;
		case PANLE_TEMPERATURE:
			cmd_desc = msd.mpd->temperature.cmd;
			cmd_size = msd.mpd->temperature.size;
			break;
		case PANLE_TOUCH_KEY:
			cmd_desc = msd.mpd->touch_key.cmd;
			cmd_size = msd.mpd->touch_key.size;
			break;

#elif defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT)
		case PANEL_READY_TO_ON:
			cmd_desc = msd.mpd->ready_to_on.cmd;
			cmd_size = msd.mpd->ready_to_on.size;
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
		case MTP_READ_ENABLE:
			cmd_desc = msd.mpd->mtp_read_enable.cmd;
			cmd_size = msd.mpd->mtp_read_enable.size;
			break;

#elif defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
		case PANEL_READY_TO_ON:
			cmd_desc = msd.mpd->ready_to_on.cmd;
			cmd_size = msd.mpd->ready_to_on.size;
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
			msd.mpd->set_elvss(mfd->bl_level);
			cmd_desc = msd.mpd->elvss_update.cmd;
			cmd_size = msd.mpd->elvss_update.size;
			break;
#endif
		case PANEL_BRIGHT_CTRL:
			cmd_desc = msd.mpd->combined_ctrl.cmd;
			cmd_size = msd.mpd->combined_ctrl.size;
			break;

#endif		
		default:
			pr_info("%s UNKNOW CMD", __func__);
			goto unknown_command;
			;
	}

#ifdef CMD_DEBUG
	if (cmd == PANEL_BRIGHT_CTRL || cmd == PANEL_ACL_CONTROL) {
		pr_info("+++ cmd_size = %d\n",cmd_size);
		for(i=0; i<cmd_size; i++){
			printk("cmd[%d] : ",i);
			for(j=0; j<cmd_desc[i].dlen; j++)
				printk("%x ",cmd_desc[i].payload[j]);
			printk("\n");
		}
		pr_info("--- end\n");
	}
#endif

	if (!cmd_size)
		goto unknown_command;

	cmdreq.cmds = cmd_desc;
	cmdreq.cmds_cnt = cmd_size;

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT)
	if (cmd == PANEL_BRIGHT_CTRL)
		cmdreq.flags = CMD_REQ_COMMIT | CMD_REQ_SINGLE_TX;
	else 
		cmdreq.flags = CMD_REQ_COMMIT;
#else
	cmdreq.flags = CMD_REQ_COMMIT;
#endif
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mipi_dsi_cmdlist_put(&cmdreq);

	if (mfd->panel.type == MIPI_VIDEO_PANEL)
		mutex_unlock(&dsi_tx_mutex);
	else {
		if (lock)
			mutex_unlock(&mfd->dma->ov_mutex);
	}
#ifdef CONFIG_MIPI_DSI_LP_RX
	mutex_unlock(&mipi_lp_mutex);
#endif
	return 0;

unknown_command:
	if (mfd->panel.type == MIPI_VIDEO_PANEL)
		mutex_unlock(&dsi_tx_mutex);
	else {
		if (lock)
			mutex_unlock(&mfd->dma->ov_mutex);
	}

#ifdef CONFIG_MIPI_DSI_LP_RX
	mutex_unlock(&mipi_lp_mutex);
#endif
	return 0;
}

static unsigned char first_on;

extern void qct_clock_dump(void);
extern void dumpreg (int);

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT)\
	|| defined(CONFIG_FB_MSM_MIPI_AMS367_OLED_VIDEO_WVGA_PT_PANEL)
#if 0
static void read_mtp(char srcReg, int srcLength, char *destBuffer,
				struct msm_fb_data_type *pMFD)
{
	struct dcs_cmd_req cmdreq;
	int one_read_size = 6;
	int loop_limit = (srcLength / one_read_size) + 1;

	/* first byte = read-register */
	char read_reg[2] = { 0xFF, 0x00 };
	struct dsi_cmd_desc read_reg_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(read_reg), read_reg };

	/* first byte is size of Register */
	char packet_size[] = { 0x00, 0x00 };
	struct dsi_cmd_desc packet_size_cmd = {
	DTYPE_MAX_PKTSIZE, 1, 0, 0, 0, sizeof(packet_size), packet_size };

	/* second byte is Read-position */
	char reg_read_pos[] = { 0xB0, 0x00 };
	struct dsi_cmd_desc read_pos_cmd = {
	DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(reg_read_pos), reg_read_pos };

	int read_pos;
	int readed_size;
	int show_cnt;

	int read_size, read_loop;
	char show_buffer[256] = {0,};
	int show_buffer_pos = 0;

	mutex_lock(&dsi_tx_mutex);

	read_reg[0] = srcReg;

	show_buffer_pos +=
	    snprintf(show_buffer, 256, "read_reg : %X[%d] : ",
		 srcReg, srcLength);

	read_pos = 0;
	readed_size = 0;

	packet_size[0] = (char)srcLength;
	mipi_dsi_buf_init(&msd.samsung_tx_buf);

	cmdreq.cmds = &packet_size_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mipi_dsi_cmdlist_put(&cmdreq);

	show_cnt = 0;
	for (read_loop = 0; read_loop < loop_limit; read_loop++) {
		reg_read_pos[1] = read_pos;

		mipi_dsi_buf_init(&msd.samsung_tx_buf);

		cmdreq.cmds = &read_pos_cmd;
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

		mipi_dsi_buf_init(&msd.samsung_tx_buf);
		mipi_dsi_buf_init(&msd.samsung_rx_buf);
		readed_size =
		    mipi_dsi_cmds_rx(pMFD, &msd.samsung_tx_buf,
				     &msd.samsung_rx_buf, &read_reg_cmd,
				     one_read_size);
		
		for (read_size = 0; read_size < readed_size;
						read_size++, show_cnt++) {
			show_buffer_pos +=
			 snprintf(show_buffer + show_buffer_pos, 256, "%02x ",
				    msd.samsung_rx_buf.data[read_size]);
			if (destBuffer != NULL && show_cnt < srcLength) {
				destBuffer[show_cnt] =
				    msd.samsung_rx_buf.data[read_size];
			}
		}

		show_buffer_pos += snprintf(show_buffer +
			show_buffer_pos, 256, ".");
		read_pos += readed_size;

		if (read_pos >= srcLength)
			break;
	}

	mutex_unlock(&dsi_tx_mutex);

	if (read_loop == loop_limit)
		show_buffer_pos +=
		    snprintf(show_buffer + show_buffer_pos, 256, "Overrun");

	pr_info("%s\n", show_buffer);
}
#else
static void read_mtp(char srcReg, int readFrom, int srcLength, char *destBuffer,
				struct msm_fb_data_type *pMFD)
{
	struct dcs_cmd_req cmdreq;
	int one_read_size = 6;
	int loop_limit = (srcLength / one_read_size) + 1;

	/* first byte = read-register */
	char read_reg[2] = { 0xFF, 0x00 };
	struct dsi_cmd_desc read_reg_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(read_reg), read_reg };

	/* first byte is size of Register */
	char packet_size[] = { 0x00, 0x00 };
	struct dsi_cmd_desc packet_size_cmd = {
	DTYPE_MAX_PKTSIZE, 1, 0, 0, 0, sizeof(packet_size), packet_size };

	/* second byte is Read-position */
	char reg_read_pos[] = { 0xB0, 0x00 };
	struct dsi_cmd_desc read_pos_cmd = {
	DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(reg_read_pos), reg_read_pos };

	int read_pos;
	int readed_size;
	int show_cnt;

	int read_size, read_loop;
	char show_buffer[256] = {0,};
	int show_buffer_pos = 0;

	mutex_lock(&dsi_tx_mutex);

	read_reg[0] = srcReg;

	show_buffer_pos +=
	    snprintf(show_buffer, 256, "read_reg : %X[%d] : ",
		 srcReg, srcLength);

	read_pos = readFrom;
	readed_size = 0;

	packet_size[0] = (char)srcLength;
	mipi_dsi_buf_init(&msd.samsung_tx_buf);

	cmdreq.cmds = &packet_size_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mipi_dsi_cmdlist_put(&cmdreq);

	show_cnt = 0;
	for (read_loop = 0; read_loop < loop_limit; read_loop++) {
		reg_read_pos[1] = read_pos;

		mipi_dsi_buf_init(&msd.samsung_tx_buf);

		cmdreq.cmds = &read_pos_cmd;
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

		mipi_dsi_buf_init(&msd.samsung_tx_buf);
		mipi_dsi_buf_init(&msd.samsung_rx_buf);
		readed_size =
		    mipi_dsi_cmds_rx(pMFD, &msd.samsung_tx_buf,
				     &msd.samsung_rx_buf, &read_reg_cmd,
				     one_read_size);
		for (read_size = 0; read_size < readed_size;
						read_size++, show_cnt++) {
			show_buffer_pos +=
			 snprintf(show_buffer + show_buffer_pos, 256, "%02x ",
				    msd.samsung_rx_buf.data[read_size]);
			if (destBuffer != NULL && show_cnt < srcLength) {
				pr_debug("show_cnt(%d),srcLength(%d), msd.samsung_rx_buf.data[%d] = %x \n",
					show_cnt,srcLength,read_size,msd.samsung_rx_buf.data[read_size]);
				destBuffer[show_cnt] =
				    msd.samsung_rx_buf.data[read_size];
			}
		}

		show_buffer_pos += snprintf(show_buffer +
			show_buffer_pos, 256, ".");

		read_pos += readed_size;

		if ((read_pos - readFrom) >= srcLength)
			break;
	}

	mutex_unlock(&dsi_tx_mutex);

	if (read_loop == loop_limit)
		show_buffer_pos +=
		    snprintf(show_buffer + show_buffer_pos, 256, "Overrun");

	pr_info("%s\n", show_buffer);
}
#endif

#endif
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT)\
	|| defined(CONFIG_FB_MSM_MIPI_AMS367_OLED_VIDEO_WVGA_PT_PANEL)
static void execute_panel_init(struct msm_fb_data_type *mfd)
{
	struct SMART_DIM *psmart = &(msd.mpd->smart_se6e8fa);
	char *mtp_buffer = (char *)&(msd.mpd->smart_se6e8fa.MTP_ORIGN);
	
	/* LSI HBM */
	char *mtp_buffer2 = (char *)&(msd.mpd->smart_se6e8fa.hbm_reg.b5_reg);
	char *mtp_buffer3 = (char *)&(msd.mpd->smart_se6e8fa.hbm_reg.b6_reg);
	char *mtp_buffer4 = (char *)&(msd.mpd->smart_se6e8fa.hbm_reg.b6_reg_17);
	int i;
#if 1 /*temp: making mtp offset values like J's*/
	int id3;

	id3 = msd.mpd->manufacture_id & 0xFF;
#endif

	mipi_samsung_disp_send_cmd(mfd, PANEL_MTP_ENABLE, false);

	/* read LDi ID */
	msd.mpd->manufacture_id = mipi_samsung_manufacture_id(mfd);
	msd.mpd->cmd_set_change(PANEL_ON, msd.mpd->manufacture_id);

	/* smart dimming & AID*/
	psmart->plux_table = msd.mpd->lux_table;
	psmart->lux_table_max = msd.mpd->lux_table_max_cnt;
	psmart->ldi_revision = msd.mpd->manufacture_id;
	
	read_mtp(MTP_START_ADDR, 0, GAMMA_SET_MAX, mtp_buffer, mfd);

	/* LSI HBM */
	read_mtp(0xb5, 13-1, 16, mtp_buffer2, mfd); // read b5h 13~28th
	read_mtp(0xb6, 3-1, 12, mtp_buffer3, mfd); // read b6h 3~14th
	msd.mpd->smart_se6e8fa.hbm_reg.b5_reg_19 = mtp_buffer2[6]; // save b5h 19th
	read_mtp(0xb6, 17-1, 1, mtp_buffer4, mfd); // recover original's ELVSS offset b6's 17th

	i = 0;
#ifdef CONFIG_HBM_PSRE_DEBUG
	printk("[HBM] b5_reg : ");
	for(i=0; i<16; i++)
		printk("%x ",msd.mpd->smart_se6e8fa.hbm_reg.b5_reg[i]);
	pr_info("\n");
	printk("[HBM] b6_reg : ");
	for(i=0; i<12; i++)
		printk("%x ",msd.mpd->smart_se6e8fa.hbm_reg.b6_reg[i]);
	pr_info("\n");
	printk("[HBM] b5_19th_reg : ");
	printk("%x ",msd.mpd->smart_se6e8fa.hbm_reg.b5_reg_19);
#endif

#if 1 /*temp: making mtp offset values like J's*/
	if (id3 == 0x00 || id3 == 0x01 || id3 == 0x02) {
		mtp_buffer[30] = 0x02;
		mtp_buffer[31] = 0x03;
		mtp_buffer[32] = 0x02;
	}
#endif

#ifdef CONFIG_HBM_PSRE_DEBUG
	pr_info("%s c8[34~39](%x)(%x)(%x)(%x)(%x)(%x) [40](%x)b6_17(%x)", __func__,
		msd.mpd->smart_se6e8fa.MTP_ORIGN.mtp_400cd[0],
		msd.mpd->smart_se6e8fa.MTP_ORIGN.mtp_400cd[1], 
		msd.mpd->smart_se6e8fa.MTP_ORIGN.mtp_400cd[2],
		msd.mpd->smart_se6e8fa.MTP_ORIGN.mtp_400cd[3], 
		msd.mpd->smart_se6e8fa.MTP_ORIGN.mtp_400cd[4],
		msd.mpd->smart_se6e8fa.MTP_ORIGN.mtp_400cd[5],
		msd.mpd->smart_se6e8fa.MTP_ORIGN.elvss_400cd ,b6_17[16]
		);
#endif

#if defined(CONFIG_MDNIE_LITE_TUNING)
	msd.mpd->coordinate[0] =  mtp_buffer2[7] << 8 | mtp_buffer2[8];	/* X */ /*b5h 20, 21th*/
	msd.mpd->coordinate[1] = mtp_buffer2[9] << 8 | mtp_buffer2[10];	/* Y */ /*b5h 22, 23th*/
	coordinate_tunning(msd.mpd->coordinate[0], msd.mpd->coordinate[1]);
#endif

	smart_dimming_init(&(msd.mpd->smart_se6e8fa));
}
#endif

#if defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT)\
	|| defined(CONFIG_FB_MSM_MIPI_AMS367_OLED_VIDEO_WVGA_PT_PANEL)
static int mipi_samsung_late_init(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);
	if (unlikely(!mfd))
		return -ENODEV;
	if (unlikely(mfd->key != MFD_KEY))
		return -EINVAL;

	mipi_samsung_disp_send_cmd(mfd, PANEL_ON, false);
	mfd->resume_state = MIPI_RESUME_STATE;

	pr_info("[%s] ID : 0x%x", __func__, msd.mpd->manufacture_id);

	return 0;
}
#endif

static int mipi_samsung_disp_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;
#if defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT) \
	|| defined(CONFIG_FB_MSM_MIPI_AMS367_OLED_VIDEO_WVGA_PT_PANEL)\
	|| defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
	static int boot_on;
#endif

    pr_info("mipi_samsung_disp_on");
    
	mfd = platform_get_drvdata(pdev);
	if (unlikely(!mfd))
		return -ENODEV;
	if (unlikely(mfd->key != MFD_KEY))
		return -EINVAL;

	mipi = &mfd->panel_info.mipi;

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT)
	mipi_samsung_disp_send_cmd(mfd, MTP_READ_ENABLE, false);
#ifdef USE_READ_ID
	msd.mpd->manufacture_id = mipi_samsung_manufacture_id(mfd);
#endif
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


	if (unlikely(first_on)) {
		first_on = false;
		return 0;
	}
	mipi_samsung_disp_send_cmd(mfd, PANEL_READY_TO_ON, false);
	if (mipi->mode == DSI_VIDEO_MODE)
		mipi_samsung_disp_send_cmd(mfd, PANEL_ON, false);

	mipi_samsung_disp_send_cmd(mfd, PANEL_LATE_ON, false);

#if !defined(CONFIG_HAS_EARLYSUSPEND)
	mipi_samsung_disp_send_cmd(mfd, PANEL_LATE_ON, false);
#endif

#elif defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT)\
	|| defined(CONFIG_FB_MSM_MIPI_AMS367_OLED_VIDEO_WVGA_PT_PANEL)
	if (!boot_on) {
		execute_panel_init(mfd);
		boot_on = 1;
	}

	if (get_auto_brightness() >= 6)
		msd.mpd->first_bl_hbm_psre = 1;

#if defined(CONFIG_FB_MSM_MIPI_AMS367_OLED_VIDEO_WVGA_PT_PANEL)	
	mipi_samsung_late_init(pdev);
#endif

#elif defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
	if (!boot_on){
		mipi_samsung_disp_send_cmd(mfd, MTP_READ_ENABLE, false);
#ifdef USE_READ_ID
		down(&mfd->dma->mutex);
		msleep(20);
		mipi_set_tx_power_mode(0);
		mipi_dsi_cmd_bta_sw_trigger();
		msd.mpd->manufacture_id = mipi_samsung_manufacture_id(mfd);
		mipi_set_tx_power_mode(1);
		up(&mfd->dma->mutex);
#endif
	}
	if (!msd.dstat.is_smart_dim_loaded) {
		/* Load MTP Data */
		int i, mtp_cnt, err_cnt;
		char *mtp_data = (char *)&(msd.mpd->smart_s6e63m0.MTP);

		for (err_cnt = 0; err_cnt < 10; err_cnt++) {
			down(&mfd->dma->mutex);
			msleep(20);
			mipi_set_tx_power_mode(0);
			mipi_dsi_cmd_bta_sw_trigger();
			mtp_cnt = find_mtp(mfd, mtp_data);
			mipi_set_tx_power_mode(1);
			up(&mfd->dma->mutex);

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
	if (unlikely(first_on)) {
		first_on = false;
		return 0;
	}

#ifdef CONFIG_FB_MDP4_ENHANCE
	is_negativeMode_on();
#endif

	mipi_samsung_disp_send_cmd(mfd, PANEL_READY_TO_ON, false);
	if (mipi->mode == DSI_VIDEO_MODE)
		mipi_samsung_disp_send_cmd(mfd, PANEL_ON, false);
#if 0//!defined(CONFIG_HAS_EARLYSUSPEND)
	mipi_samsung_disp_send_cmd(mfd, PANEL_LATE_ON, false);
#endif
#endif
#if (!defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT)) \
	&& (!defined(CONFIG_FB_MSM_MIPI_AMS367_OLED_VIDEO_WVGA_PT_PANEL))
	mfd->resume_state = MIPI_RESUME_STATE;
#endif

#if defined(CONFIG_RUNTIME_MIPI_CLK_CHANGE)
	current_fps = mfd->panel_info.mipi.frame_rate;
#endif

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	enable_irq(err_fg_gpio);
#endif

	pr_info("[lcd] %s\n", __func__);

	return 0;
}

static int mipi_samsung_disp_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);
	if (unlikely(!mfd))
		return -ENODEV;
	if (unlikely(mfd->key != MFD_KEY))
		return -EINVAL;

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	if (!err_fg_working) {
		disable_irq_nosync(err_fg_gpio);
		cancel_work_sync(&err_fg_work);
	}
#endif

	mfd->resume_state = MIPI_SUSPEND_STATE;

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT)
	mipi_samsung_disp_send_cmd(mfd, PANEL_READY_TO_OFF, false);
	mipi_samsung_disp_send_cmd(mfd, PANEL_OFF, false);
	msd.mpd->ldi_acl_stat = false;

#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT)\
	|| defined(CONFIG_FB_MSM_MIPI_AMS367_OLED_VIDEO_WVGA_PT_PANEL)
	mipi_samsung_disp_send_cmd(mfd, PANEL_OFF, false);
	if (msd.mpd->reset_bl_level != NULL)
		msd.mpd->reset_bl_level();
	
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
	mipi_samsung_disp_send_cmd(mfd, PANEL_READY_TO_OFF, false);
	mipi_samsung_disp_send_cmd(mfd, PANEL_OFF, false);
	/*mipi_samsung_disp_send_cmd(mfd, PANEL_EARLY_OFF, true);*/
#endif

	pr_info("[lcd] %s\n", __func__);

	return 0;
}

#ifndef CONFIG_MDP_SHUTDOWN
static void __devinit mipi_samsung_disp_shutdown(struct platform_device *pdev)
{
	static struct mipi_dsi_platform_data *mipi_dsi_pdata;
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);

	if (pdev->id != 0)
		return;

	mipi_dsi_pdata = pdev->dev.platform_data;

	if (mipi_dsi_pdata == NULL) {
		pr_err("LCD Power off failure: No Platform Data\n");
		return;
	}

	if (mfd)
		mipi_samsung_disp_send_cmd(mfd, PANEL_OFF, false);
}
#endif

static void mipi_samsung_disp_backlight(struct msm_fb_data_type *mfd)
{

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT)
	mfd->backlight_ctrl_ongoing = TRUE;

	pr_info("mipi_samsung_disp_backlight %d\n", mfd->bl_level);
	if (!msd.mpd->set_gamma ||  !mfd->panel_power_on ||\
		mfd->resume_state == MIPI_SUSPEND_STATE)
		goto end;

	if (msd.mpd->set_gamma(mfd->bl_level, msd.dstat.gamma_mode) < 0)
		goto end;

	pr_info("mipi_samsung_disp_backlight %d\n", mfd->bl_level);
	mipi_samsung_disp_send_cmd(mfd, PANEL_GAMMA_UPDATE, true);

end:
	mfd->backlight_ctrl_ongoing = FALSE;

#elif defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT)\
	|| defined(CONFIG_FB_MSM_MIPI_AMS367_OLED_VIDEO_WVGA_PT_PANEL)

	mfd->backlight_ctrl_ongoing = FALSE;
	if (mfd->resume_state == MIPI_RESUME_STATE) {		
		if (msd.mpd->backlight_control(mfd->bl_level)) {
			mipi_samsung_disp_send_cmd(mfd, PANEL_BRIGHT_CTRL, true);
			pr_info("mipi_samsung_disp_backlight %d\n", mfd->bl_level);
		}
		msd.mpd->first_bl_hbm_psre = 0;
	} else {
		msd.mpd->first_bl_hbm_psre = 0;
		pr_info("%s : panel is off state!!\n", __func__);
	}
	
#elif defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
	if (msd.mpd->prepare_brightness_control_cmd_array) {
		int cmds_sent;

		cmds_sent = msd.mpd->prepare_brightness_control_cmd_array(
						0, mfd->bl_level);
		pr_debug("cmds_sent: %x\n", cmds_sent);
		if (cmds_sent < 0)
			goto end;

		mipi_samsung_disp_send_cmd(mfd, PANEL_BRIGHT_CTRL, true);
		goto end;
	}
	
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
	if (msd.mpd->set_acl && msd.dstat.acl_on && msd.mpd->set_acl(mfd->bl_level))
		mipi_samsung_disp_send_cmd(mfd, PANEL_ACL_OFF, true);
	
	if (msd.mpd->set_acl && msd.dstat.acl_on && !msd.mpd->set_acl(mfd->bl_level)) {
		mipi_samsung_disp_send_cmd(mfd, PANEL_ACL_ON, true);
		mipi_samsung_disp_send_cmd(mfd, PANEL_ACL_UPDATE, true);
	}
#endif

end:

#endif

	return;
}

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void mipi_samsung_disp_early_suspend(struct early_suspend *h)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(msd.msm_pdev);
	if (unlikely(!mfd)) {
		pr_info("%s NO PDEV.\n", __func__);
		return;
	}
	if (unlikely(mfd->key != MFD_KEY)) {
		pr_info("%s MFD_KEY is not matched.\n", __func__);
		return;
	}

	pr_info("[lcd] %s\n", __func__);
}

static void mipi_samsung_disp_late_resume(struct early_suspend *h)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(msd.msm_pdev);
	if (unlikely(!mfd)) {
		pr_info("%s NO PDEV.\n", __func__);
		return;
	}
	if (unlikely(mfd->key != MFD_KEY)) {
		pr_info("%s MFD_KEY is not matched.\n", __func__);
		return;
	}

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT)
	reset_gamma_level();
	mipi_samsung_disp_backlight(mfd);
#endif

	pr_info("[lcd] %s", __func__);
}
#endif

static int atoi(const char *name)
{
	int val = 0;

	for (;; name++) {
		switch (*name) {
		case '0' ... '9':
			val = 10*val+(*name-'0');
			break;
		default:
			return val;
		}
	}
}

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT)

unsigned char get_auto_brightness(void)
{
	return  msd.dstat.auto_brightness;
}
char* get_b5_reg(void)
{
	return msd.mpd->smart_se6e8fa.hbm_reg.b5_reg;
}
char get_b5_reg_19(void)
{
	return msd.mpd->smart_se6e8fa.hbm_reg.b5_reg_19;
}
char* get_b6_reg(void)
{
	return msd.mpd->smart_se6e8fa.hbm_reg.b6_reg;
}

char get_b6_reg_17(void)
{
	return msd.mpd->smart_se6e8fa.hbm_reg.b6_reg_17;
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

	rc = snprintf((char *)buf, sizeof(*buf), "%d\n", mfd->panel_power_on);
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

static ssize_t mipi_samsung_disp_windowtype_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char temp[15];
	int id1, id2, id3;
	id1 = (msd.mpd->manufacture_id & 0x00FF0000) >> 16;
	id2 = (msd.mpd->manufacture_id & 0x0000FF00) >> 8;
	id3 = msd.mpd->manufacture_id & 0xFF;

	snprintf(temp, sizeof(temp), "%x %x %x\n",	id1, id2, id3);
	strlcat(buf, temp, 15);
	return strnlen(buf, 15);
}

static ssize_t mipi_samsung_disp_gamma_mode_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, sizeof(*buf), "%d\n", msd.dstat.gamma_mode);
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

	rc = snprintf((char *)buf, sizeof(*buf), "%d\n", msd.dstat.acl_on);
	pr_info("acl status: %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_disp_acl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(msd.msm_pdev);

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT) \
	|| defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
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
			}
			msd.dstat.acl_on = true;
		} else if (sysfs_streq(buf, "0") && msd.dstat.acl_on) {
			mipi_samsung_disp_send_cmd(mfd, PANEL_ACL_OFF, true);
			msd.dstat.acl_on = false;
		} else {
			pr_info("%s: Invalid argument!!", __func__);
		}
	}
#else
	if (sysfs_streq(buf, "1"))
		msd.mpd->acl_status = true;
	else if (sysfs_streq(buf, "0"))
		msd.mpd->acl_status = false;
	else {
		pr_info("%s: Invalid argument!!", __func__);
		return size;
	}

	if (mfd->panel_power_on) {
		if (msd.mpd->acl_control(mfd->bl_level))
			mipi_samsung_disp_send_cmd(mfd,
						PANEL_ACL_CONTROL, true);
	} else
		pr_info("%s : panel is off state. updating state value.\n", __func__);

		pr_info("%s : acl_status (%d) siop_status (%d)",
				__func__, msd.mpd->acl_status, msd.mpd->siop_status);

#endif



	return size;
}

static ssize_t mipi_samsung_disp_siop_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;
	
#if defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT)\
	|| defined(CONFIG_FB_MSM_MIPI_AMS367_OLED_VIDEO_WVGA_PT_PANEL)
	rc = snprintf((char *)buf, sizeof(buf), "%d\n", msd.mpd->siop_status);
	pr_info("siop status: %d\n", *buf);
#else
	rc = 0; /*temp*/
#endif

	return rc;
}

static ssize_t mipi_samsung_disp_siop_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(msd.msm_pdev);
	
#if defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT)\
	|| defined(CONFIG_FB_MSM_MIPI_AMS367_OLED_VIDEO_WVGA_PT_PANEL)
	if (sysfs_streq(buf, "1"))
		msd.mpd->siop_status = true;
	else if (sysfs_streq(buf, "0"))
		msd.mpd->siop_status = false;
	else {
		pr_info("%s: Invalid argument!!", __func__);
		return size;
	}

	if (mfd->panel_power_on) {
		if (msd.mpd->acl_control(mfd->bl_level))
			mipi_samsung_disp_send_cmd(mfd,
						PANEL_ACL_CONTROL, true);
	} else
		pr_info("%s : panel is off state. updating state value.\n", __func__);

	pr_info("%s : acl_status (%d) siop_status (%d)",
			__func__, msd.mpd->acl_status, msd.mpd->siop_status);
#endif

	return size;
}

static ssize_t mipi_samsung_auto_brightness_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n",
					msd.dstat.auto_brightness);
	pr_info("%s : %d\n", __func__, msd.dstat.auto_brightness);

	return rc;
}

static ssize_t mipi_samsung_auto_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
#if defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT)
	static int first_auto_br;
	struct msm_fb_data_type *mfd;
	mfd = platform_get_drvdata(msd.msm_pdev);
#endif
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
	else if (sysfs_streq(buf, "6"))
		msd.dstat.auto_brightness = 6;	
	else
		pr_info("%s: Invalid argument!!", __func__);

#if defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT)
	if (!first_auto_br) {
		pr_info("%s : skip first auto brightness store (%d) (%d)!!\n", 
				__func__, msd.dstat.auto_brightness, mfd->bl_level);
		first_auto_br++;
		return size;
	}

	if (mfd->resume_state == MIPI_RESUME_STATE) {
		msd.mpd->first_bl_hbm_psre = 1;
		mipi_samsung_disp_backlight(mfd);
		pr_info("%s : %d\n",__func__,msd.dstat.auto_brightness);
	} else {
		msd.mpd->first_bl_hbm_psre = 0;
		pr_info("%s : panel is off state!!\n", __func__);
	}
#endif
	return size;
}

#if defined(CONFIG_RUNTIME_MIPI_CLK_CHANGE)
static ssize_t mipi_samsung_fps_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, 20, "%d\n", current_fps);

	return rc;
}

static ssize_t mipi_samsung_fps_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd;
	int goal_fps;
	int level = atoi(buf);

	mfd = platform_get_drvdata(msd.msm_pdev);

	if (mfd->panel_power_on == FALSE) {
		pr_err("%s fps set error, panel power off 1", __func__);
		return size;
	}

	if (level == 0)
		goal_fps = 60;
	else if (level == 1)
		goal_fps = 42;
	else if (level == 2)
		goal_fps = 51;
	else {
		pr_info("%s fps set error : invalid level %d", __func__, level);
		return size;
	}

	if (current_fps != goal_fps)
		current_fps = goal_fps;
	else
		return size;

	mutex_lock(&dsi_tx_mutex);

	if (mfd->panel_power_on == FALSE) {
		mutex_unlock(&dsi_tx_mutex);
		pr_info("%s fps set error, panel power off 2", __func__);
		return size;
	} else {
		mipi_runtime_clk_change(current_fps);
		mutex_unlock(&dsi_tx_mutex);
	}

	pr_info("%s goal_fps : %d", __func__, goal_fps);

	return size;
}

#endif

static ssize_t mipi_samsung_disp_backlight_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;
	struct msm_fb_data_type *mfd;
	mfd = platform_get_drvdata(msd.msm_pdev);

	rc = snprintf((char *)buf, sizeof(buf), "%d\n", mfd->bl_level);
	pr_info("%s : %d\n", __func__, mfd->bl_level);

	return rc;
}

static ssize_t mipi_samsung_disp_backlight_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd;
	int level = atoi(buf);

	mfd = platform_get_drvdata(msd.msm_pdev);

	mfd->bl_level = level;
	
	if (mfd->resume_state == MIPI_RESUME_STATE) {
		mipi_samsung_disp_backlight(mfd);
		pr_info("%s : level (%d)\n",__func__,level);
	} else {
		pr_info("%s : panel is off state!!\n", __func__);
	}

	return size;
}

static struct lcd_ops mipi_samsung_disp_props = {
#ifdef WA_FOR_FACTORY_MODE
	.get_power = NULL,
	.set_power = NULL,
#else
	.get_power = mipi_samsung_disp_get_power,
	.set_power = mipi_samsung_disp_set_power,
#endif
};

#ifdef WA_FOR_FACTORY_MODE
static DEVICE_ATTR(lcd_power, S_IRUGO | S_IWUSR,
			mipi_samsung_disp_get_power,
			mipi_samsung_disp_set_power);
#endif
static DEVICE_ATTR(lcd_type, S_IRUGO, mipi_samsung_disp_lcdtype_show, NULL);
 
static DEVICE_ATTR(window_type, S_IRUGO,
			mipi_samsung_disp_windowtype_show, NULL);

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

static DEVICE_ATTR(backlight, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_disp_backlight_show,
			mipi_samsung_disp_backlight_store);

#endif

#if defined(CONFIG_RUNTIME_MIPI_CLK_CHANGE)
static DEVICE_ATTR(fps_change, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_fps_show,
			mipi_samsung_fps_store);
#endif


#ifdef DDI_VIDEO_ENHANCE_TUNING
#define MAX_FILE_NAME 128
#define TUNING_FILE_PATH "/sdcard/"
#define TUNE_FIRST_SIZE 5
#define TUNE_SECOND_SIZE 108
static char tuning_file[MAX_FILE_NAME];

static char mdni_tuning1[TUNE_FIRST_SIZE];
static char mdni_tuning2[TUNE_SECOND_SIZE];

static struct dsi_cmd_desc mdni_tune_cmd[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(mdni_tuning2), mdni_tuning2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(mdni_tuning1), mdni_tuning1},
};

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
	struct msm_fb_data_type *mfd;
	struct dcs_cmd_req cmdreq;

	int data_pos;
	int cmd_step;
	int cmd_pos;

	cmd_step = 0;
	cmd_pos = 0;

	for (data_pos = 0; data_pos < len;) {
		if (*(src + data_pos) == '0') {
			if (*(src + data_pos + 1) == 'x') {
				if (!cmd_step) {
					mdni_tuning1[cmd_pos] =
					char_to_dec(*(src + data_pos + 2),
							*(src + data_pos + 3));
				} else {
					mdni_tuning2[cmd_pos] =
					char_to_dec(*(src + data_pos + 2),
							*(src + data_pos + 3));
				}

				data_pos += 3;
				cmd_pos++;

				if (cmd_pos == TUNE_FIRST_SIZE && !cmd_step) {
					cmd_pos = 0;
					cmd_step = 1;
				}
			} else
				data_pos++;
		} else {
			data_pos++;
		}
	}

	printk(KERN_INFO "\n");
	for (data_pos = 0; data_pos < TUNE_FIRST_SIZE ; data_pos++)
		printk(KERN_INFO "0x%x ", mdni_tuning1[data_pos]);
	printk(KERN_INFO "\n");
	for (data_pos = 0; data_pos < TUNE_SECOND_SIZE ; data_pos++)
		printk(KERN_INFO"0x%x ", mdni_tuning2[data_pos]);
	printk(KERN_INFO "\n");

	mfd = platform_get_drvdata(msd.msm_pdev);

	mutex_lock(&dsi_tx_mutex);

	cmdreq.cmds = mdni_tune_cmd;
	cmdreq.cmds_cnt = ARRAY_SIZE(mdni_tune_cmd);
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mipi_dsi_cmdlist_put(&cmdreq);

	mutex_unlock(&dsi_tx_mutex);
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

	ret = snprintf(buf, MAX_FILE_NAME, "Tunned File Name : %s\n",
								tuning_file);

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

	load_tuning_file(tuning_file);

	return size;
}

static DEVICE_ATTR(tuning, 0664, tuning_show, tuning_store);
#endif

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
static irqreturn_t err_fg_irq_handler(int irq, void *handle)
{
	pr_info("%s : handler start", __func__);
	disable_irq_nosync(err_fg_gpio);
	schedule_work(&err_fg_work);
	pr_info("%s : handler end", __func__);

	return IRQ_HANDLED;
}
static void err_fg_work_func(struct work_struct *work)
{
	int bl_backup;
	struct msm_fb_data_type *mfd;
	mfd = platform_get_drvdata(msd.msm_pdev);

	bl_backup = mfd->bl_level;

	pr_info("%s : start", __func__);
	err_fg_working = 1;
	esd_recovery();
	esd_count++;
	err_fg_working = 0;

	mdelay(50);

	/* brightness off */
	mfd->bl_level = 0;
	mipi_samsung_disp_backlight(mfd);

	/* Restore brightness */
	mfd->bl_level = bl_backup;
	mipi_samsung_disp_backlight(mfd);

	pr_info("%s : end", __func__);
	return;
}

#ifdef ESD_DEBUG
static ssize_t mipi_samsung_esd_check_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, 20, "esd count : %d\n", esd_count);

	return rc;
}
static ssize_t mipi_samsung_esd_check_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd;
	mfd = platform_get_drvdata(msd.msm_pdev);

	err_fg_irq_handler(0, mfd);
	return 1;
}

static DEVICE_ATTR(esd_check, S_IRUGO , mipi_samsung_esd_check_show,\
			 mipi_samsung_esd_check_store);
#endif
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
		first_on = false;

		return 0;
	}

	msm_fb_added_dev = msm_fb_add_device(pdev);

	mutex_init(&dsi_tx_mutex);

#if defined(CONFIG_HAS_EARLYSUSPEND) || defined(CONFIG_LCD_CLASS_DEVICE)
	msd.msm_pdev = msm_fb_added_dev;
#endif

#if defined(CONFIG_HAS_EARLYSUSPEND)
	msd.early_suspend.suspend = mipi_samsung_disp_early_suspend;
	msd.early_suspend.resume = mipi_samsung_disp_late_resume;
	msd.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	register_early_suspend(&msd.early_suspend);

#endif

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	INIT_WORK(&err_fg_work, err_fg_work_func);

	err_fg_gpio = MSM_GPIO_TO_INT(GPIO_ERR_FG);

	ret = gpio_request(GPIO_ERR_FG, "err_fg");
	if (ret) {
		pr_err("request gpio GPIO_LCD_ESD_DET failed, ret=%d\n",ret);
		gpio_free(GPIO_ERR_FG);
		return ret;
	}

	gpio_tlmm_config(GPIO_CFG(GPIO_ERR_FG,  0, GPIO_CFG_INPUT,
					GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),GPIO_CFG_ENABLE);

	gpio_tlmm_config(GPIO_CFG(GPIO_ESD_VGH_DET,  0, GPIO_CFG_INPUT,
					GPIO_CFG_NO_PULL, GPIO_CFG_2MA),GPIO_CFG_ENABLE);

	ret = request_threaded_irq(err_fg_gpio, NULL, err_fg_irq_handler, 
		IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "esd_detect", NULL);
	if (ret) {
		pr_err("%s : Failed to request_irq. :ret=%d", __func__, ret);
	}

	disable_irq(err_fg_gpio);
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
			&dev_attr_window_type.attr);
	if (ret) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_window_type.attr.name);
	}

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

	ret = sysfs_create_file(&lcd_device->dev.kobj,
					&dev_attr_backlight.attr);
	if (ret) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_backlight.attr.name);
	}

#if defined(CONFIG_RUNTIME_MIPI_CLK_CHANGE)
	ret = sysfs_create_file(&lcd_device->dev.kobj,
						&dev_attr_fps_change.attr);
	if (ret) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_fps_change.attr.name);
	}
#endif

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

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
#ifdef ESD_DEBUG
	ret = sysfs_create_file(&lcd_device->dev.kobj,
							&dev_attr_esd_check.attr);
	if (ret) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_esd_check.attr.name);
	}
#endif
#endif

#endif

#if defined(CONFIG_MDNIE_LITE_TUNING) \
	|| defined(CONFIG_FB_MDP4_ENHANCE)
	init_mdnie_class();
#endif

#if defined(DDI_VIDEO_ENHANCE_TUNING)
		ret = sysfs_create_file(&lcd_device->dev.kobj,
				&dev_attr_tuning.attr);
		if (ret) {
			pr_info("sysfs create fail-%s\n",
					dev_attr_tuning.attr.name);
		}
#endif
	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mipi_samsung_disp_probe,
	.driver = {
		.name   = "mipi_samsung_oled",
	},
#ifdef CONFIG_MDP_SHUTDOWN
	.shutdown = NULL,
#else
	.shutdown = mipi_samsung_disp_shutdown
#endif
};

static struct msm_fb_panel_data samsung_panel_data = {
#if defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT)
//	|| defined(CONFIG_FB_MSM_MIPI_AMS367_OLED_VIDEO_WVGA_PT_PANEL)
	.late_init = mipi_samsung_late_init,
#endif
	.on		= mipi_samsung_disp_on,
	.off		= mipi_samsung_disp_off,
	.set_backlight	= mipi_samsung_disp_backlight,
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

#if defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT)
int get_lcd_attached(void)
{
	return lcd_attached;
}
EXPORT_SYMBOL(get_lcd_attached);

static int __init lcd_attached_status(char *mode)
{
	/*
	*	1 is lcd attached
	*	0 is lcd detached
	*/

	if (strncmp(mode, "1", 1) == 0)
		lcd_attached = 1;
	else
		lcd_attached = 0;

	pr_info("%s %s", __func__, lcd_attached == 1 ?
				"lcd_attached" : "lcd_detached");
	return 1;
}
__setup("lcd_attached=", lcd_attached_status);
#endif

static int __init mipi_samsung_disp_init(void)
{
	mipi_dsi_buf_alloc(&msd.samsung_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&msd.samsung_rx_buf, DSI_BUF_SIZE);

	return platform_driver_register(&this_driver);
}
module_init(mipi_samsung_disp_init);
