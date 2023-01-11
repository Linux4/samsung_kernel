/*
 * linux/include/video/mmpfb.h
 * Header file for Marvell MMP Display Controller
 *
 * Copyright (C) 2012 Marvell Technology Group Ltd.
 * Authors: Yonghai Huang <huangyh@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _MMPFB_H_
#define _MMPFB_H_
struct mmpfb_global_info {
/* Define FB version */
/* pxa168fb is version 0 */
#define FB_VERSION_0	0x0
/* mmp is version 1 */
#define FB_VERSION_1	0x1
#define FB_VERSION_2	0x2
	int version;
	int fb_counts;
};
#endif	/* _MMPFB_H_ */
