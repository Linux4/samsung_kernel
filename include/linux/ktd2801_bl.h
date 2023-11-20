 /*******************************************************************************
 * Copyright 2010 Broadcom Corporation.  All rights reserved.
 *
 * 		 @file		 include/linux/ktd253b_bl.h
 *
 * Unless you and Broadcom execute a separate written software license agreement
 * governing use of this software, this software is licensed to you under the
 * terms of the GNU General Public License version 2, available at
 * http://www.gnu.org/copyleft/gpl.html (the "GPL").
 *
 * Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *******************************************************************************/

 /*
  * Generic S2C based backlight driver data
  * - see drivers/video/backlightcat4253_bl.c
  */
 #ifndef __LINUX_KTD253B_BL_H
 #define __LINUX_KTD253B_BL_H
extern int is_poweron;
extern int wakeup_brightness;
void ktd_backlight_set_brightness(int level);
 #endif
