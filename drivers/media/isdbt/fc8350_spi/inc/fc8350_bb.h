/*****************************************************************************
	Copyright(c) 2017 FCI Inc. All Rights Reserved

	File name : fc8350_bb.h

	Description : header of baseband driver

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
*******************************************************************************/
#ifndef __FC8350_BB__
#define __FC8350_BB__

#ifdef __cplusplus
extern "C" {
#endif

extern s32 fc8350_reset(HANDLE handle, DEVICEID devid);
extern s32 fc8350_probe(HANDLE handle, DEVICEID devid);
extern s32 fc8350_init(HANDLE handle, DEVICEID devid);
extern s32 fc8350_deinit(HANDLE handle, DEVICEID devid);
extern s32 fc8350_scan_status(HANDLE handle, DEVICEID devid);
extern s32 fc8350_set_broadcast_mode(HANDLE handle, DEVICEID devid,
		enum BROADCAST_TYPE broadcast);
extern s32 fc8350_set_core_clk(HANDLE handle, DEVICEID devid,
		enum BROADCAST_TYPE broadcast, u32 freq);

#ifdef __cplusplus
}
#endif

#endif /* __FC8350_BB__ */

