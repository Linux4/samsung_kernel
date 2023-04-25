/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef __ILITEK_V3_FW_H
#define __ILITEK_V3_FW_H

/* define names and paths for the variety of tp modules */
#define DEF_INI_NAME_PATH		"/sdcard/mp.ini"
#define DEF_FW_FILP_PATH		"/sdcard/ILITEK_FW"
#define DEF_INI_REQUEST_PATH		"mp.ini"
#define DEF_FW_REQUEST_PATH		"ILITEK_FW"
static unsigned char CTPM_FW_DEF[] = {
	#include "FW_TDDI_TRUNK_FB.ili"
};

#define CSOT_INI_NAME_PATH		"/sdcard/mp_csot.ini"
#define CSOT_FW_FILP_PATH		"/sdcard/ILITEK_FW_CSOT"
#define CSOT_INI_REQUEST_PATH		"mp_csot.ini"
#define CSOT_FW_REQUEST_PATH		"ILITEK_FW_CSOT"
static unsigned char CTPM_FW_CSOT[] = {
	0xFF,
};

#define LS_PANDA_INI_NAME_PATH		"/vendor/firmware/mp_ls_panda.ini"
#define LS_PANDA_FW_FILP_PATH		"/sdcard/ILITEK_FW_CSOT"
#define LS_PANDA_INI_REQUEST_PATH		"mp_csot.ini"
#define LS_PANDA_FW_REQUEST_PATH		"ILITEK_FW_CSOT"
static unsigned char CTPM_FW_LS_PANDA[] = {
	#include "ILI9882H_O3_LS_PANDA_6517_0x14_AP_0x01_MP_20210401.ili"
};

/*HS50 code for HS50-1707 by gaozhengwei at 2020/09/14 start*/
#define LS_PANDA_LRPOL_INI_NAME_PATH		"/vendor/firmware/mp_ls_panda_lrpol.ini"
#define LS_PANDA_LRPOL_FW_FILP_PATH		"/sdcard/ILITEK_FW_CSOT"
#define LS_PANDA_LRPOL_INI_REQUEST_PATH		"mp_csot.ini"
#define LS_PANDA_LRPOL_FW_REQUEST_PATH		"ILITEK_FW_CSOT"
static unsigned char CTPM_FW_LS_PANDA_LRPOL[] = {
	#include "ILI9882H_O3_LS_PANDA_6517_0x14_AP_0x01_MP_20210401.ili"
};
/*HS50 code for HS50-1707 by gaozhengwei at 2020/09/14 end*/
/*HS50 code for SR-QL3095-01-743 by fengzhigang at 2020/09/27 start*/
#define LS_PANDA_SWID22_INI_NAME_PATH		"/vendor/firmware/mp_ls_panda_swid22.ini"
#define LS_PANDA_SWID22_FW_FILP_PATH		"/sdcard/ILITEK_FW_CSOT"
#define LS_PANDA_SWID22_INI_REQUEST_PATH		"mp_csot.ini"
#define LS_PANDA_SWID22_FW_REQUEST_PATH		"ILITEK_FW_CSOT"
static unsigned char CTPM_FW_LS_PANDA_SWID22[] = {
	#include "ILI9882H_O3_LS_PANDA_6517_0x14_AP_0x01_MP_20210401.ili"
};
/*HS50 code for SR-QL3095-01-743 by fengzhigang at 2020/09/27 end*/

/*HS50 code for HS50-4157 by gaozhengwei at 2020/11/07 start*/
#define LS_PANDA_SBLPOL_LJFPC_INI_NAME_PATH		"/vendor/firmware/mp_ls_panda_sblpol_ljfpc.ini"
#define LS_PANDA_SBLPOL_LJFPC_FW_FILP_PATH		"/sdcard/ILITEK_FW_CSOT"
#define LS_PANDA_SBLPOL_LJFPC_INI_REQUEST_PATH		"mp_csot.ini"
#define LS_PANDA_SBLPOL_LJFPC_FW_REQUEST_PATH		"ILITEK_FW_CSOT"
static unsigned char CTPM_FW_LS_PANDA_SBLPOL_LJFPC[] = {
	#include "ILI9882N_O3_LS_12__PANDA_6517_0x02_AP_0x02_MP_20201230.ili"
};
/*HS50 code for HS50-4157 by gaozhengwei at 2020/11/07 end*/

#define SKW_PANDA_INI_NAME_PATH		"/vendor/firmware/mp_skw_panda.ini"
#define SKW_PANDA_FW_FILP_PATH		"/sdcard/ILITEK_FW_CSOT"
#define SKW_PANDA_INI_REQUEST_PATH		"mp_csot.ini"
#define SKW_PANDA_FW_REQUEST_PATH		"ILITEK_FW_CSOT"
static unsigned char CTPM_FW_SKW_PANDA[] = {
	#include "ILI9882H_O3_SKW_PANDA_6517_0x0F_AP_0x02_MP_20201026.ili"
};

/*HS50 code for HS50-1707 by gaozhengwei at 2020/09/14 start*/
#define SKW_PANDA_LRPOL_INI_NAME_PATH		"/vendor/firmware/mp_skw_panda_lrpol.ini"
#define SKW_PANDA_LRPOL_FW_FILP_PATH		"/sdcard/ILITEK_FW_CSOT"
#define SKW_PANDA_LRPOL_INI_REQUEST_PATH		"mp_csot.ini"
#define SKW_PANDA_LRPOL_FW_REQUEST_PATH		"ILITEK_FW_CSOT"
static unsigned char CTPM_FW_SKW_PANDA_LRPOL[] = {
	#include "ILI9882H_O3_SKW_PANDA_6517_0x0F_AP_0x02_MP_20201026.ili"
};
/*HS50 code for HS50-1707 by gaozhengwei at 2020/09/14 end*/

#define LS_MDT_INI_NAME_PATH		"/vendor/firmware/mp_ls_mdt.ini"
#define LS_MDT_FW_FILP_PATH		"/sdcard/ILITEK_FW_CSOT"
#define LS_MDT_INI_REQUEST_PATH		"mp_csot.ini"
#define LS_MDT_FW_REQUEST_PATH		"ILITEK_FW_CSOT"
static unsigned char CTPM_FW_LS_MDT[] = {
	#include "ILI9982N_O3_LS_MDT_6517_LongV_V0xA1_0x26_0x01_AP_0x01_MP_20200723.ili"
};

#define TXD_INX_ILI9882N_INI_NAME_PATH		"/vendor/firmware/mp_txd_inx_ili9882n.ini"
#define TXD_INX_ILI9882N_FW_FILP_PATH		"/sdcard/ILITEK_FW"
#define TXD_INX_ILI9882N_INI_REQUEST_PATH		"mp_txd_inx_ili9882n.ini"
#define TXD_INX_ILI9882N_FW_REQUEST_PATH		"ILITEK_FW"
static unsigned char CTPM_FW_TXD_INX_ILI9882N[] = {
	#include "ILI9882H_O3_TXD_INX_6517_0x09_AP_0x82_MP_20201022.ili"
};

#define GX_BOE_EPD6301_INI_NAME_PATH		"/vendor/firmware/mp_gx_boe_epd6301.ini"
#define GX_BOE_EPD6301_FW_FILP_PATH		"/sdcard/ILITEK_FW"
#define GX_BOE_EPD6301_INI_REQUEST_PATH		"mp_gx_boe_epd6301.ini"
#define GX_BOE_EPD6301_FW_REQUEST_PATH		"ILITEK_FW"
static unsigned char CTPM_FW_GX_BOE_EPD6301[] = {
	#include "9882FLH_V0x00.0x00_AP_0x04_MP_0x01_20201225.ili"
};

#define JDD_PANDA_ILI9882N_INI_NAME_PATH		"/vendor/firmware/mp_jdd_panda_ili9882n.ini"
#define JDD_PANDA_ILI9882N_FW_FILP_PATH		"/sdcard/ILITEK_FW"
#define JDD_PANDA_ILI9882N_INI_REQUEST_PATH		"mp_jdd_panda_ili9882n.ini"
#define JDD_PANDA_ILI9882N_FW_REQUEST_PATH		"ILITEK_FW"
static unsigned char CTPM_FW_JDD_PANDA_ILI9882N[] = {
	#include "ILI9882H_O3_JDD_PANDA_6517_0x04_AP_0x01_MP_20210118.ili"
};

#define HLT_BOE_ILI7806S_INI_NAME_PATH		"/vendor/firmware/mp_hlt_boe_ili7806s.ini"
#define HLT_BOE_ILI7806S_FW_FILP_PATH		"/sdcard/ILITEK_FW"
#define HLT_BOE_ILI7806S_INI_REQUEST_PATH		"mp_hlt_boe_ili7806s.ini"
#define HLT_BOE_ILI7806S_FW_REQUEST_PATH		"ILITEK_FW"
static unsigned char CTPM_FW_HLT_BOE_ILI7806S[] = {
	#include "ILI7807S_O3_LH_AP_0x04_MP_0x04_20210121.ili"
};

#define TXD_BOE_9MASK_ILI7806S_INI_NAME_PATH		"/vendor/firmware/mp_txd_boe_9mask_ili7806s.ini"
#define TXD_BOE_9MASK_ILI7806S_FW_FILP_PATH		"/sdcard/ILITEK_FW"
#define TXD_BOE_9MASK_ILI7806S_INI_REQUEST_PATH		"mp_txd_boe_9mask_ili7806s.ini"
#define TXD_BOE_9MASK_ILI7806S_FW_REQUEST_PATH		"ILITEK_FW"
static unsigned char CTPM_FW_TXD_BOE_9MASK_ILI7806S[] = {
	#include "ILI7807S_O3_LH_AP_0x04_MP_0x04_20210121.ili"
};

#define AUO_INI_NAME_PATH		"/sdcard/mp_auo.ini"
#define AUO_FW_FILP_PATH		"/sdcard/ILITEK_FW_AUO"
#define AUO_INI_REQUEST_PATH		"mp_auo.ini"
#define AUO_FW_REQUEST_PATH		"ILITEK_FW_AUO"
static unsigned char CTPM_FW_AUO[] = {
	0xFF,
};

#define BOE_INI_NAME_PATH		"/sdcard/mp_boe.ini"
#define BOE_FW_FILP_PATH		"/sdcard/ILITEK_FW_BOE"
#define BOE_INI_REQUEST_PATH		"mp_boe.ini"
#define BOE_FW_REQUEST_PATH		"ILITEK_FW_BOE"
static unsigned char CTPM_FW_BOE[] = {
	0xFF,
};

#define INX_INI_NAME_PATH		"/sdcard/mp_inx.ini"
#define INX_FW_FILP_PATH		"/sdcard/ILITEK_FW_INX"
#define INX_INI_REQUEST_PATH		"mp_inx.ini"
#define INX_FW_REQUEST_PATH		"ILITEK_FW_INX"
static unsigned char CTPM_FW_INX[] = {
	0xFF,
};

#define DJ_INI_NAME_PATH		"/sdcard/mp_dj.ini"
#define DJ_FW_FILP_PATH                 "/sdcard/ILITEK_FW_DJ"
#define DJ_INI_REQUEST_PATH		"mp_dj.ini"
#define DJ_FW_REQUEST_PATH		"ILITEK_FW_DJ"
static unsigned char CTPM_FW_DJ[] = {
	0xFF,
};

#define TXD_INI_NAME_PATH		"/sdcard/mp_txd.ini"
#define TXD_FW_FILP_PATH		"/sdcard/ILITEK_FW_TXD"
#define TXD_INI_REQUEST_PATH		"mp_txd.ini"
#define TXD_FW_REQUEST_PATH		"ILITEK_FW_TXD"
static unsigned char CTPM_FW_TXD[] = {
	0xFF,
};

#define TM_INI_NAME_PATH		"/sdcard/mp_tm.ini"
#define TM_FW_FILP_PATH                 "/sdcard/ILITEK_FW_TM"
#define TM_INI_REQUEST_PATH		"mp_tm.ini"
#define TM_FW_REQUEST_PATH		"ILITEK_FW_TM"
static unsigned char CTPM_FW_TM[] = {
	0xFF,
};

#endif
