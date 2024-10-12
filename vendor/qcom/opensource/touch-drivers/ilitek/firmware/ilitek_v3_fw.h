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
#define TM_ILI7807S_INI_NAME_PATH		"/sdcard/mp.ini"
#define TM_ILI7807S_FW_FILP_PATH		"/sdcard/ILITEK_FW"
#define TM_ILI7807S_INI_REQUEST_PATH		"mp.ini"
#define TM_ILI7807S_FW_REQUEST_PATH		"ILITEK_FW"
static unsigned char CTPM_FW_TM_ILI7807S[] = {
	#include "ILITEK.ili"
};
/*ILITEK ILI77600A TM path*/
#define TM_ILI77600A_INI_NAME_PATH		    "/sdcard/ILI77600A_mp.ini"
#define TM_ILI77600A_FW_FILP_PATH		    "/sdcard/ILITEK_ILI77600A_FW"
#define TM_ILI77600A_INI_REQUEST_PATH		"ILI77600A_mp.ini"
#define TM_ILI77600A_FW_REQUEST_PATH		"ILITEK_ILI77600A_FW"
static unsigned char CTPM_FW_TM_ILI77600A[] = {
	#include "ILITEK_ILI77600A.ili"
};
/*ILITEK ILI77600A INX path*/
#define INX_ILI77600A_INI_NAME_PATH		    "/sdcard/ILI77600A_INX_mp.ini"
#define INX_ILI77600A_FW_FILP_PATH		    "/sdcard/ILITEK_ILI77600A_INX_FW"
#define INX_ILI77600A_INI_REQUEST_PATH		"ILI77600A_INX_mp.ini"
#define INX_ILI77600A_FW_REQUEST_PATH		"ILITEK_ILI77600A_INX_FW"
static unsigned char CTPM_FW_INX_ILI77600A[] = {
	#include "ILITEK_ILI77600A_INX.ili"
};
/*ILITEK ILI77600A XL path*/
#define XL_ILI77600A_INI_NAME_PATH		"/sdcard/ILI77600A_XL_mp.ini"
#define XL_ILI77600A_FW_FILP_PATH		"/sdcard/ILITEK_ILI77600A_XL_FW"
#define XL_ILI77600A_INI_REQUEST_PATH		"ILI77600A_XL_mp.ini"
#define XL_ILI77600A_FW_REQUEST_PATH		"ILITEK_ILI77600A_XL_FW"
static unsigned char CTPM_FW_XL_ILI77600A[] = {
	#include "ILITEK_ILI77600A_XL.ili"
};

/*ILITEK ILI7807S XL path*/
#define XL_ILI7807S_INI_NAME_PATH		"/sdcard/ILI7807S_XL_mp.ini"
#define XL_ILI7807S_FW_FILP_PATH		"/sdcard/ILITEK_ILI7807S_XL_FW"
#define XL_ILI7807S_INI_REQUEST_PATH		"ILI7807S_XL_mp.ini"
#define XL_ILI7807S_FW_REQUEST_PATH		"ILITEK_ILI7807S_XL_FW"
static unsigned char CTPM_FW_XL_ILI7807S[] = {
	#include "ILITEK_ILI7807S_XL.ili"
};
/*ILITEK ILI7807S TMB path*/
#define TMB_ILI7807S_INI_NAME_PATH		"/sdcard/ILI7807S_TMB_mp.ini"
#define TMB_ILI7807S_FW_FILP_PATH		"/sdcard/ILITEK_ILI7807S_TMB_FW"
#define TMB_ILI7807S_INI_REQUEST_PATH		"ILI7807S_TMB_mp.ini"
#define TMB_ILI7807S_FW_REQUEST_PATH		"ILITEK_ILI7807S_TMB_FW"
static unsigned char CTPM_FW_TMB_ILI7807S[] = {
	#include "ILITEK_ILI7807S_TMB.ili"
};
#endif