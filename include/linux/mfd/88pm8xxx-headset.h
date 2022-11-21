/*
 * 88pm8xxx-headset.h
 *
 * The headset detect driver based on levante
 *
 * Copyright (2008) Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __LINUX_MFD_88PM8XXX_HEADSET_H
#define __LINUX_MFD_88PM8XXX_HEADSET_H

#define PM8XXX_HEADSET_REMOVE			0
#define PM8XXX_HEADSET_ADD				1
#define PM8XXX_HEADPHONE_ADD			2
#define PM8XXX_HEADSET_MODE_STEREO		0
#define PM8XXX_HEADSET_MODE_MONO		1
#define PM8XXX_HS_MIC_ADD				1
#define PM8XXX_HS_MIC_REMOVE			0
#define PM8XXX_HOOKSWITCH_PRESSED		1
#define PM8XXX_HOOKSWITCH_RELEASED		0
#define PM8XXX_HOOK_VOL_PRESSED			1
#define PM8XXX_HOOK_VOL_RELEASED		0


enum {
	HOOK_RELEASED = 0,
	VOL_UP_RELEASED,
	VOL_DOWN_RELEASED,

	HOOK_VOL_ALL_RELEASED,

	HOOK_PRESSED,
	VOL_UP_PRESSED,
	VOL_DOWN_PRESSED,
};

struct PM8XXX_HS_IOCTL {
	int hsdetect_status;
	int hsdetect_mode;	/* for future stereo/mono */
	int hsmic_status;
	int hookswitch_status;
};

/*
 * ioctl calls that are permitted to the /dev/micco_hsdetect interface.
 */

/* Headset detection status */
#define PM8XXX_HSDETECT_STATUS		_IO('L', 0x01)
/* Hook switch status */
#define PM8XXX_HOOKSWITCH_STATUS	_IO('L', 0x02)

#define ENABLE_HS_DETECT_POLES

#endif /* __LINUX_MFD_88PM8XXX_HEADSET_H */
