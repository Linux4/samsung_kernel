/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


/*****************************************************************************
 *
 * Filename:
 * ---------
 *     s5k3m3_setting_v2.h
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     CMOS sensor setting file
 *
 * Setting Release Date:
 * ------------
 *     2016.09.01
 *
 ****************************************************************************/
#ifndef _s5k3m3MIPI_SETTING_V2_H_
#define _s5k3m3MIPI_SETTING_V2_H_


static void sensor_init_v2(void)
{
	LOG_INF("E+ Sensor init.\n");
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6010, 0x0000);
	mdelay(3);
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6010, 0x0001);
	mdelay(3);
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0300, 0x0004);
	write_cmos_sensor(0x0302, 0x0001);
	write_cmos_sensor(0x0304, 0x0006);
	write_cmos_sensor(0x0308, 0x0008);
	write_cmos_sensor(0x030A, 0x0001);
	write_cmos_sensor(0x030C, 0x0004);
	write_cmos_sensor(0x3644, 0x0060);
	write_cmos_sensor(0x0216, 0x0000);
	write_cmos_sensor(0xF496, 0x0048);
	write_cmos_sensor(0x1118, 0x43C8);
	write_cmos_sensor(0x1124, 0x43C8);
	write_cmos_sensor(0x3058, 0x0000);
	write_cmos_sensor(0x0100, 0x0000);
	LOG_INF("E- Sensor init.\n");
}	/*	sensor_init  */


static void preview_setting_v2(void)
{
	LOG_INF("E+ Preview setting.\n");
	write_cmos_sensor(0x0100, 0x0000);
	check_stremoff();
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0306, 0x0078);
	write_cmos_sensor(0x030E, 0x0063);
	write_cmos_sensor(0x0344, 0x0020);
	write_cmos_sensor(0x0346, 0x0008);
	write_cmos_sensor(0x0348, 0x105F);
	write_cmos_sensor(0x034A, 0x0C37);
	write_cmos_sensor(0x034C, 0x1040);
	write_cmos_sensor(0x034E, 0x0C30);
	write_cmos_sensor(0x0340, 0x0D2F);
	write_cmos_sensor(0x0342, 0x1260);
	write_cmos_sensor(0x3000, 0x0001);
	write_cmos_sensor(0xF440, 0x002F);
	write_cmos_sensor(0xF494, 0x0030);
	write_cmos_sensor(0x3604, 0x0000);
	write_cmos_sensor(0xF4CC, 0x0029);
	write_cmos_sensor(0xF4CE, 0x002C);
	write_cmos_sensor(0xF4D0, 0x0034);
	write_cmos_sensor(0xF4D2, 0x0035);
	write_cmos_sensor(0xF4D4, 0x0038);
	write_cmos_sensor(0xF4D6, 0x0039);
	write_cmos_sensor(0xF4D8, 0x0034);
	write_cmos_sensor(0xF4DA, 0x0035);
	write_cmos_sensor(0xF4DC, 0x0038);
	write_cmos_sensor(0xF4DE, 0x0039);
	write_cmos_sensor(0x0200, 0x0618);
	write_cmos_sensor(0x0202, 0x0D27);
	write_cmos_sensor(0x021E, 0x0400);
	write_cmos_sensor(0x021C, 0x0000);
	write_cmos_sensor(0x0900, 0x0011);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0001);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0001);
	write_cmos_sensor(0x0400, 0x0000);
	write_cmos_sensor(0x0404, 0x0010);
	write_cmos_sensor(0x0100, 0x0100);
	LOG_INF("E- Preview setting.\n");
}	/*	preview_setting  */

static void capture_setting_v2(kal_uint16 currefps)
{
	write_cmos_sensor(0x0100, 0x0000);
	check_stremoff();
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0306, 0x0078);
	write_cmos_sensor(0x030E, 0x0063);
	write_cmos_sensor(0x0344, 0x0020);
	write_cmos_sensor(0x0346, 0x0008);
	write_cmos_sensor(0x0348, 0x105F);
	write_cmos_sensor(0x034A, 0x0C37);
	write_cmos_sensor(0x034C, 0x1040);
	write_cmos_sensor(0x034E, 0x0C30);
	write_cmos_sensor(0x0340, 0x0D2F);
	write_cmos_sensor(0x0342, 0x1260);
	write_cmos_sensor(0x3000, 0x0001);
	write_cmos_sensor(0xF440, 0x002F);
	write_cmos_sensor(0xF494, 0x0030);
	write_cmos_sensor(0x3604, 0x0000);
	write_cmos_sensor(0xF4CC, 0x0029);
	write_cmos_sensor(0xF4CE, 0x002C);
	write_cmos_sensor(0xF4D0, 0x0034);
	write_cmos_sensor(0xF4D2, 0x0035);
	write_cmos_sensor(0xF4D4, 0x0038);
	write_cmos_sensor(0xF4D6, 0x0039);
	write_cmos_sensor(0xF4D8, 0x0034);
	write_cmos_sensor(0xF4DA, 0x0035);
	write_cmos_sensor(0xF4DC, 0x0038);
	write_cmos_sensor(0xF4DE, 0x0039);
	write_cmos_sensor(0x0200, 0x0618);
	write_cmos_sensor(0x0202, 0x0D27);
	write_cmos_sensor(0x021E, 0x0400);
	write_cmos_sensor(0x021C, 0x0000);
	write_cmos_sensor(0x0900, 0x0011);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0001);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0001);
	write_cmos_sensor(0x0400, 0x0000);
	write_cmos_sensor(0x0404, 0x0010);
	write_cmos_sensor(0x0100, 0x0100);
}

static void hs_video_setting_v2(void)
{
	LOG_INF("E+ video 16:9 setting.\n");
	write_cmos_sensor(0x0100, 0x0000);
	check_stremoff();
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0306, 0x0078);
	write_cmos_sensor(0x030E, 0x0063);
	write_cmos_sensor(0x0344, 0x0020);
	write_cmos_sensor(0x0346, 0x0188);
	write_cmos_sensor(0x0348, 0x105F);
	write_cmos_sensor(0x034A, 0x0AAF);
	write_cmos_sensor(0x034C, 0x1040);
	write_cmos_sensor(0x034E, 0x0924);
	write_cmos_sensor(0x0340, 0x0D2F);
	write_cmos_sensor(0x0342, 0x1260);
	write_cmos_sensor(0x3000, 0x0001);
	write_cmos_sensor(0xF440, 0x002F);
	write_cmos_sensor(0xF494, 0x0030);
	write_cmos_sensor(0x3604, 0x0000);
	write_cmos_sensor(0xF4CC, 0x0029);
	write_cmos_sensor(0xF4CE, 0x002C);
	write_cmos_sensor(0xF4D0, 0x0034);
	write_cmos_sensor(0xF4D2, 0x0035);
	write_cmos_sensor(0xF4D4, 0x0038);
	write_cmos_sensor(0xF4D6, 0x0039);
	write_cmos_sensor(0xF4D8, 0x0034);
	write_cmos_sensor(0xF4DA, 0x0035);
	write_cmos_sensor(0xF4DC, 0x0038);
	write_cmos_sensor(0xF4DE, 0x0039);
	write_cmos_sensor(0x0200, 0x0618);
	write_cmos_sensor(0x0202, 0x0D27);
	write_cmos_sensor(0x021E, 0x0400);
	write_cmos_sensor(0x021C, 0x0000);
	write_cmos_sensor(0x0900, 0x0011);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0001);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0001);
	write_cmos_sensor(0x0400, 0x0000);
	write_cmos_sensor(0x0404, 0x0010);
	write_cmos_sensor(0x0100, 0x0100);
}

static void slim_video_setting_v2(void)
{
	write_cmos_sensor(0x0100, 0x0000);
	check_stremoff();
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0306, 0x0078);
	write_cmos_sensor(0x030E, 0x0063);
	write_cmos_sensor(0x0344, 0x0020);
	write_cmos_sensor(0x0346, 0x0008);
	write_cmos_sensor(0x0348, 0x105F);
	write_cmos_sensor(0x034A, 0x0C37);
	write_cmos_sensor(0x034C, 0x0820);
	write_cmos_sensor(0x034E, 0x0618);
	write_cmos_sensor(0x0340, 0x069E);
	write_cmos_sensor(0x0342, 0x24C0);
	write_cmos_sensor(0x3000, 0x0001);
	write_cmos_sensor(0xF440, 0x006F);
	write_cmos_sensor(0xF494, 0x0020);
	write_cmos_sensor(0x3604, 0x0000);
	write_cmos_sensor(0xF4CC, 0x0029);
	write_cmos_sensor(0xF4CE, 0x002C);
	write_cmos_sensor(0xF4D0, 0x0034);
	write_cmos_sensor(0xF4D2, 0x0035);
	write_cmos_sensor(0xF4D4, 0x0038);
	write_cmos_sensor(0xF4D6, 0x0039);
	write_cmos_sensor(0xF4D8, 0x0034);
	write_cmos_sensor(0xF4DA, 0x0035);
	write_cmos_sensor(0xF4DC, 0x0038);
	write_cmos_sensor(0xF4DE, 0x0039);
	write_cmos_sensor(0x0200, 0x0618);
	write_cmos_sensor(0x0202, 0x059D);
	write_cmos_sensor(0x021E, 0x0400);
	write_cmos_sensor(0x021C, 0x0000);
	write_cmos_sensor(0x0900, 0x0112);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0001);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0003);
	write_cmos_sensor(0x0400, 0x0001);
	write_cmos_sensor(0x0404, 0x0020);
	write_cmos_sensor(0x0100, 0x0100);
}


#endif
