/*****************************************************************************
	Copyright(c) 2017 FCI Inc. All Rights Reserved

	File name : fc8350_tun_table.h

	Description : header of FC8350 tuner driver

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

	History :
	----------------------------------------------------------------------
******************************************************************************/
#include "fci_types.h"

#ifndef __FC8350_TUN_TABLE_H__
#define __FC8350_TUN_TABLE_H__

#ifdef __cplusplus
extern "C" {
#endif

extern u32 ch_mode_saw[121][11];
extern u32 ch_mode_nosaw[121][11];

#ifdef __cplusplus
}
#endif

#endif /* __FC8350_TUN_TABLE_H__ */

