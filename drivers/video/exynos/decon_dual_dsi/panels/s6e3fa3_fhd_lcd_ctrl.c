/*
 * drivers/video/decon/panels/s6e3hf2_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2014 Samsung Electronics
 *
 * Jiun Yu, <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <video/mipi_display.h>
#include "../dsim.h"
#include "panel_info.h"


static const unsigned int s6e3fa3_ref_br_tbl[256] = {
	2, 2, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	16, 17, 19, 20, 21, 22, 24, 25, 25, 27, 29, 30, 32, 34, 37, 39,
	41, 44, 47, 47, 50, 50, 53, 56, 56, 60, 60, 64, 64, 68, 68, 68,
	72, 72, 72, 77, 77, 77, 82, 82, 82, 87, 87, 87, 87, 93, 93, 93,
	98, 98, 98, 98, 105, 105, 105, 105, 111, 111, 111, 111, 119, 119, 119, 119,
	119, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 134, 134, 134, 134, 134,
	143, 143, 143, 143, 143, 143, 152, 152, 152, 152, 152, 152, 152, 162, 162, 162,
	162, 162, 162, 172, 172, 172, 172, 172, 172, 172, 172, 183, 183, 183, 183, 183,
	183, 183, 195, 195, 195, 195, 195, 195, 195, 195, 207, 207, 207, 207, 207, 207,
	207, 207, 207, 220, 220, 220, 220, 220, 220, 220, 234, 234, 234, 234, 234, 234,
	234, 234, 234, 249, 249, 249, 249, 249, 249, 249, 249, 249, 265, 265, 265, 265,
	265, 265, 265, 265, 265, 265, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
	300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 316, 316, 316, 316, 316, 316,
	316, 316, 316, 316, 316, 316, 333, 333, 333, 333, 350, 350, 350, 350, 350, 350,
	350, 350, 350, 357, 357, 357, 357, 357, 372, 372, 372, 372, 372, 372, 380, 380,
	380, 380, 387, 387, 387, 395, 395, 395, 395, 395, 403, 403, 403, 412, 412, 420,
};

static const unsigned int s6e3fa3_idx_br_tbl[256] = {
	0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 21, 22, 23, 24, 25, 26, 27, 28, 29,
	30, 31, 31, 32, 32, 33, 34, 34, 35, 35, 36, 36, 37, 37, 37, 38,
	38, 38, 39, 39, 39, 40, 40, 40, 41, 41, 41, 41, 42, 42, 42, 43,
	43, 43, 43, 44, 44, 44, 44, 45, 45, 45, 45, 46, 46, 46, 46, 46,
	47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 48, 48, 48, 48, 48, 49,
	49, 49, 49, 49, 49, 50, 50, 50, 50, 50, 50, 50, 51, 51, 51, 51,
	51, 51, 52, 52, 52, 52, 52, 52, 52, 52, 53, 53, 53, 53, 53, 53,
	53, 54, 54, 54, 54, 54, 54, 54, 54, 55, 55, 55, 55, 55, 55, 55,
	55, 55, 56, 56, 56, 56, 56, 56, 56, 57, 57, 57, 57, 57, 57, 57,
	57, 57, 58, 58, 58, 58, 58, 58, 58, 58, 58, 59, 59, 59, 59, 59,
	59, 59, 59, 59, 59, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 61,
	61, 61, 61, 61, 61, 61, 61, 61, 61, 62, 62, 62, 62, 62, 62, 62,
	62, 62, 62, 62, 62, 63, 63, 63, 63, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 65, 65, 65, 65, 65, 67, 67, 67, 67, 67, 67, 68, 68, 68,
	68, 69, 69, 69, 70, 70, 70, 70, 70, 71, 71, 71, 72, 72, 73,
};


#define S6E3HF3_RDDPM_ADDR		0x0A
#define S6E3HF3_RDDPM_SLPOUT	0x01 << 4
#define S6E3HF3_RDDPM_DISP_ON	0x01 << 3

unsigned char syncmode_reg[10] = {0x01, 0x00, 0x02, 0x5A, 0xF0, 0x03, 0x0D, 0x01, 0x00, 0x00};
#define S6E3HF3_SYNCMODE 		0xF2
#define S6E3HF3_SYNCMODE_CNT	10


static int s6e3fa3_fhd_init(struct dsim_device *dsim, int mode);

static int s6e3fa3_read_init_info(struct dsim_device *dsim, unsigned char* mtp, unsigned char* hbm)
{
	int i = 0;
	int ret = 0;
#if 0
	unsigned char rddpm[4];
#endif
	struct panel_private *panel = &dsim->priv;

	unsigned char tmtp[S6E3FA3_MTP_DATE_SIZE] = {0, };
	unsigned char bufForCoordi[S6E3FA3_COORDINATE_LEN] = {0,};
	unsigned char t_sync_reg[10] = {0, };

	dsim_info("%s:id-%d:was called\n", __func__, dsim->id);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_ON_F0\n", __func__, dsim->id);
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_ON_FC\n", __func__, dsim->id);
	}

	ret = dsim_read_hl_data(dsim, S6E3FA3_ID_REG, S6E3FA3_ID_LEN, dsim->priv.id);
	if (ret != S6E3FA3_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n",__func__);
		panel->lcdConnected = PANEL_DISCONNEDTED;
		goto read_exit;
	}

	dsim_info("READ ID : ");
	for (i = 0; i < S6E3FA3_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

	ret = dsim_read_hl_data(dsim, S6E3FA3_MTP_ADDR, S6E3FA3_MTP_DATE_SIZE, tmtp);
	if (ret != S6E3FA3_MTP_DATE_SIZE) {
		dsim_err("ERR:%s:failed to read mtp value : %d\n", __func__, ret);
		goto read_fail;
	}

	memcpy(mtp, tmtp, S6E3FA3_MTP_SIZE);
	memcpy(dsim->priv.date, &tmtp[40], ARRAY_SIZE(dsim->priv.date));
	dsim_info("READ MTP SIZE : %d\n", S6E3FA3_MTP_SIZE);
	dsim_info("=========== MTP INFO =========== \n");
	for (i = 0; i < S6E3FA3_MTP_SIZE; i++)
		dsim_info("MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);

	ret = dsim_read_hl_data(dsim, S6E3FA3_COORDINATE_REG, S6E3FA3_COORDINATE_LEN, bufForCoordi);
	if (ret != S6E3FA3_COORDINATE_LEN) {
		dsim_err("fail to read coordinate on command.\n");
		goto read_fail;
	}
	dsim->priv.coordinate[0] = bufForCoordi[0] << 8 | bufForCoordi[1];	/* X */
	dsim->priv.coordinate[1] = bufForCoordi[2] << 8 | bufForCoordi[3];	/* Y */
	dsim_info("READ coordi : ");
	for(i = 0; i < 2; i++)
		dsim_info("%d, ", dsim->priv.coordinate[i]);
	dsim_info("\n");

	if (dsim->id == 0) {
		ret = dsim_read_hl_data(dsim, S6E3HF3_SYNCMODE, S6E3HF3_SYNCMODE_CNT, t_sync_reg);
		if (ret != S6E3HF3_SYNCMODE_CNT) {
			dsim_err("ERR:DSIM:%s:fail to read syncmode command\n", __func__);
			goto read_fail;
		}
		memcpy(syncmode_reg, t_sync_reg, S6E3HF3_SYNCMODE_CNT);
	}
	memcpy(dsim->priv.syncReg, syncmode_reg, S6E3HF3_SYNCMODE_CNT);
	dsim_info("READ Sync Mode Reg : ");
	for(i = 0; i < S6E3HF3_SYNCMODE_CNT; i++)
		dsim_info("%x, ", dsim->priv.syncReg[i]);
	dsim_info("\n");

	// elvss
	ret = dsim_read_hl_data(dsim, ELVSS_REG, ELVSS_LEN - 1, dsim->priv.elvss_set);
	if (ret < ELVSS_LEN - 1) {
		dsim_err("fail to read elvss on command.\n");
		goto read_fail;
	}
	dsim_info("READ elvss : ");
	for(i = 0; i < ELVSS_LEN - 1; i++)
		dsim_info("%x \n", dsim->priv.elvss_set[i]);


#if 0
	ret = dsim_read_hl_data(dsim, S6E3HF3_RDDPM_ADDR, 3, rddpm);
	if (ret != 3) {
		dsim_err("%s : can't read RDDPM Reg\n",__func__);
		goto read_exit;
	}

	if (rddpm[0] & S6E3HF3_RDDPM_DISP_ON) {
		dsim_info("* Display On and Working Ok\n");
	} else {
		dsim_info("* Display Off\n");
	}

	if (rddpm[0] & S6E3HF3_RDDPM_SLPOUT) {
		dsim_info("* Sleep OUT and Working Ok\n");
		if (dsim->id == 0) {
			panel->op_state = PANEL_STAT_STANDALONE;
		} else {

		}
	} else {
		dsim_info("* Sleep IN\n");
		panel->op_state = PANEL_STAT_OFF;
	}
#endif
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_OFF_FC\n", __func__, dsim->id);
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_OFF_F0\n", __func__, dsim->id);
		goto read_fail;
	}
read_exit:
	return 0;

read_fail:
	return -ENODEV;

}

static int s6e3fa3_fhd_dump(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	return ret;
}



extern int init_smart_dimming(struct panel_info *panel, char *refgamma, char *mtp);

static int s6e3fa3_fhd_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
	unsigned char mtp[S6E3FA3_MTP_SIZE] = {0, };
	unsigned char hbm[S6E3FA3_HBMGAMMA_LEN] = {0, };
	unsigned char refgamma[S6E3FA3_MTP_SIZE] = {
		0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
		0x80, 0x80, 0x80,
		0x80, 0x80, 0x80,
		0x80, 0x80, 0x80,
		0x80, 0x80, 0x80,
		0x80, 0x80, 0x80,
		0x80, 0x80, 0x80,
		0x80, 0x80, 0x80,
		0x80, 0x80, 0x80,
		0x00, 0x00, 0x00,
		0x00, 0x00
	};

	dsim_info("DSIM Panel : %d : %s was called\n", dsim->id, __func__);

	panel->dim_data = (void *)NULL;
	//panel->lcdConnected = PANEL_CONNECTED;
	panel->panel_type = 0;

	ret = s6e3fa3_read_init_info(dsim, mtp, hbm);
	if (panel->lcdConnected == PANEL_DISCONNEDTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}
#if 0
	panel->command.CMD_GAMMA_UPDATE_1 = SEQ_GAMMA_UPDATE;
	panel->command.CMD_L1_TEST_KEY_ON = SEQ_TEST_KEY_ON_F0;
	panel->command.CMD_L1_TEST_KEY_OFF = SEQ_TEST_KEY_OFF_F0;
	panel->command.CMD_ACL_ON_OPR = SEQ_OPR_ACL_ON;
	panel->command.CMD_ACL_OFF_OPR = SEQ_OPR_ACL_OFF;
	panel->command.CMD_ACL_ON = SEQ_ACL_ON;
	panel->command.CMD_ACL_OFF = SEQ_ACL_OFF;
#endif
#ifdef CONFIG_PANEL_AID_DIMMING
	panel->mapping_tbl.idx_br_tbl = s6e3fa3_idx_br_tbl;
	panel->mapping_tbl.ref_br_tbl = s6e3fa3_ref_br_tbl;
	panel->mapping_tbl.phy_br_tbl = s6e3fa3_ref_br_tbl;

	//panel->aid_info = (struct aid_dimming_info *)aid_info;

	init_smart_dimming(&panel->command, refgamma, mtp);

#endif
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	panel->mdnie_support = 1;
#endif

probe_exit:
	return ret;

}


static int s6e3fa3_fhd_displayon(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	dsim_info("DSIM Panel : %d : %s was called\n", dsim->id, __func__);

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_DISPLAY_ON\n", __func__, dsim->id);
 		goto displayon_err;
	}

	panel->op_state |= PANEL_STAT_DISPLAY_ON;

displayon_err:
	return ret;

}


static int s6e3fa3_fhd_exit(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	dsim_info("DSIM Panel : %d : %s was called\n", dsim->id, __func__);

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_DISPLAY_OFF\n", __func__, dsim->id);
		goto exit_err;
	}

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_SLEEP_IN\n", __func__, dsim->id);
		goto exit_err;
	}

	msleep(120);

	panel->op_state = PANEL_STAT_OFF;

exit_err:

	return ret;
}


#define S6E3FA3_MASTER_MODE			0x01 << 4
#define S6E3FA3_STANDALONE_MODE 	0x03 << 4
#define S6E3FA3_SLAVE_MODE			0x02 << 4
#define S6E3FA3_SYNCMODE_MASK		0x03 << 4

#define S6E3FA3_DD_VSYNC_ON			0x01 << 5
#define S6E3FA3_DD_HSYNC_ON			0x01 << 5

static int s6e3fa3_fhd_set_syncmode(struct dsim_device *dsim, unsigned int syncmode)
{
	int ret = 0;
	unsigned char sync_reg[S6E3HF3_SYNCMODE_CNT+1] = {0xF2, };
	unsigned char syncval, vsync_on, hsync_on;

	struct panel_private *panel = &dsim->priv;

	memcpy(&sync_reg[1], panel->syncReg, S6E3HF3_SYNCMODE_CNT);

	syncval = sync_reg[1];
	syncval &= ~(S6E3FA3_SYNCMODE_MASK);

	vsync_on = sync_reg[9];
	vsync_on &= ~(S6E3FA3_DD_VSYNC_ON);

	hsync_on = sync_reg[10];
	hsync_on &= ~(S6E3FA3_DD_HSYNC_ON);

	switch (syncmode) {
		case PANEL_STAT_MASTER :
			syncval |= S6E3FA3_MASTER_MODE;
			vsync_on |= S6E3FA3_DD_VSYNC_ON;
			hsync_on |= S6E3FA3_DD_HSYNC_ON;
			break;
		case PANEL_STAT_SLAVE :
			syncval |= S6E3FA3_SLAVE_MODE;
			break;
		case PANEL_STAT_STANDALONE:
			syncval |= S6E3FA3_STANDALONE_MODE;
			break;
	}

	sync_reg[1] = syncval;
	sync_reg[9] = vsync_on;
	sync_reg[10] = hsync_on;

	dsim_info("%s: syncval : %x, vsync_on : %x, hsync_on : %x\n", __func__, syncval, vsync_on, hsync_on);

	ret = dsim_write_hl_data(dsim, sync_reg, S6E3HF3_SYNCMODE_CNT+1);
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write sync mode\n", __func__, dsim->id);
	}

	return ret;
}

static int s6e3fa3_fhd_part_init(struct dsim_device *dsim, unsigned int syncmode)
{
	int ret;

	dsim_info("%s : %d\n", __func__, syncmode);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_ON_F0\n", __func__, dsim->id);
		goto err_part_init;
	}

	ret = s6e3fa3_fhd_set_syncmode(dsim, syncmode);
	if (ret) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to set syncmode\n", __func__, dsim->id);
		goto err_part_init;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_OFF_F0\n", __func__, dsim->id);
		goto err_part_init;
	}

	return ret;

err_part_init:
	return ret;

}


static int s6e3fa3_fhd_full_init(struct dsim_device *dsim, unsigned int syncmode)
{
	int ret = 0;

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_ON_F0\n", __func__, dsim->id);
		goto err_full_init;
	}

	ret = s6e3fa3_fhd_set_syncmode(dsim, syncmode);
	if (ret) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to set syncmode\n", __func__, dsim->id);
		goto err_full_init;
	}

	/* 1. Sleep Out(11h) */
	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_SLEEP_OUT\n", __func__, dsim->id);
		goto err_full_init;
	}
	msleep(20);

	ret = dsim_write_hl_data(dsim, SEQ_TE_MODE, ARRAY_SIZE(SEQ_TE_MODE));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_ON_F0\n", __func__, dsim->id);
		goto err_full_init;
	}
	/* Common Setting */
	ret = dsim_write_hl_data(dsim, SEQ_TE_ON, ARRAY_SIZE(SEQ_TE_ON));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TE_ON\n", __func__, dsim->id);
		goto err_full_init;
	}

#ifndef CONFIG_PANEL_AID_DIMMING
	/* Brightness Setting */
	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_GAMMA_CONDITION_SET\n", __func__, dsim->id);
		goto err_full_init;
	}

	ret = dsim_write_hl_data(dsim, SEQ_DEFAULT_AID_SETTING, ARRAY_SIZE(SEQ_DEFAULT_AID_SETTING));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_DEFAULT_AID_SETTING\n", __func__, dsim->id);
		goto err_full_init;
	}

	ret = dsim_write_hl_data(dsim, SEQ_DEFAULT_ELVSS_SET, ARRAY_SIZE(SEQ_DEFAULT_ELVSS_SET));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_DEFAULT_ELVSS_SET\n", __func__, dsim->id);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_GAMMA_UPDATE\n", __func__, dsim->id);
		goto err_full_init;
	}

	/* ACL Setting */
	ret = dsim_write_hl_data(dsim, SEQ_OPR_ACL_OFF, ARRAY_SIZE(SEQ_OPR_ACL_OFF));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_OPR_ACL_OFF\n", __func__, dsim->id);
		goto err_full_init;
	}

	ret = dsim_write_hl_data(dsim, SEQ_ACL_OFF, ARRAY_SIZE(SEQ_ACL_OFF));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_ACL_OFF\n", __func__, dsim->id);
		goto err_full_init;
	}
#endif

	msleep(120);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_OFF_F0\n", __func__, dsim->id);
		goto err_full_init;
	}

	return ret;

err_full_init :
	dsim_err("ERR:PANEL:%s:id-%d:failed to full init\n", __func__, dsim->id);
	return ret;

}


static int s6e3fa3_fhd_init(struct dsim_device *dsim, int mode)
{
	int ret = 0;
	int sync_mode = GET_PANEL_STAT(mode);
	struct panel_private *panel = &dsim->priv;

	dsim_info("DSIM Panel:id-%d:%s was called\n", dsim->id, __func__);

	if (mode & FULL_UPDATE)
		ret = s6e3fa3_fhd_full_init(dsim, sync_mode);
	else
		ret = s6e3fa3_fhd_part_init(dsim, sync_mode);

	if (ret) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to full init\n", __func__, dsim->id);
		goto err_init;
	}

	panel->op_state &= MASK_PANEL_STAT;
	panel->op_state |= sync_mode;
	return ret;

err_init:
	dsim_err("%s : failed to init\n", __func__);
	return ret;
}


struct dsim_panel_ops s6e3fa3_panel_ops = {
	.early_probe = NULL,
	.probe		= s6e3fa3_fhd_probe,
	.displayon	= s6e3fa3_fhd_displayon,
	.exit		= s6e3fa3_fhd_exit,
	.init		= s6e3fa3_fhd_init,
	.dump 		= s6e3fa3_fhd_dump,
};

struct dsim_panel_ops *dsim_panel_get_priv_ops(struct dsim_device *dsim)
{
	return &s6e3fa3_panel_ops;
}

static int __init s6e3fa3_get_lcd_type(char *arg)
{
	unsigned int lcdtype;

	get_option(&arg, &lcdtype);

	dsim_info("--- Parse LCD TYPE ---\n");
	dsim_info("LCDTYPE : %x\n", lcdtype);

	return 0;
}
early_param("lcdtype", s6e3fa3_get_lcd_type);

