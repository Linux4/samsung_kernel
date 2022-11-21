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
#define DEF_INI_REQUEST_PATH	"mp.ini"
#define DEF_FW_REQUEST_PATH		"ILITEK_FW"
static unsigned char CTPM_FW_DEF[] = {
	0xFF,
};

#define TXD_7806S_NA_INI_NAME_PATH		"/vendor/firmware/mp_txd_7806s_na.ini"
#define TXD_7806S_NA_FW_FILP_PATH		"/vendor/firmware/FW_TDDI_TXD_7806S_NA_NA"
#define TXD_7806S_NA_INI_REQUEST_PATH	"mp_txd_7806s_na.ini"
#define TXD_7806S_NA_FW_REQUEST_PATH	"FW_TDDI_TXD_7806S_NA_NA"
static unsigned char CTPM_FW_TXD_7806S_NA[] = {
	#include "FW_TDDI_TXD_7806S_NA.ili"
};
#endif
