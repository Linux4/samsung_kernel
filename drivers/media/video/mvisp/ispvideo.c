/*
* ispvideo.c
*
* Marvell DxO ISP - video node
*	Based on omap3isp
*
*
* Copyright:  (C) Copyright 2011 Marvell International Ltd.
*			   Henry Zhao <xzhao10@marvell.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
* 02110-1301 USA
*/


#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>
#include <media/ispvideo.h>

/* -----------------------------------------------------------------------------
 * ISP video core
 */

int mvisp_video_register(struct map_vnode *vnode, struct v4l2_device *vdev,
			enum isp_video_type video_type)
{
	int dir, nr;

	switch (video_type) {
	case ISP_VIDEO_INPUT:
		dir = 1;
		nr = ISP_VIDEO_NR_BASE + 2;
		snprintf(vnode->vdev.name, sizeof(vnode->vdev.name),
			"mvisp %s %s", ISP_VIDEO_INPUT_NAME, "input");
		break;
	case ISP_VIDEO_DISPLAY:
		dir = 0;
		nr = ISP_VIDEO_NR_BASE;
		snprintf(vnode->vdev.name, sizeof(vnode->vdev.name),
			"mvisp %s %s", ISP_VIDEO_DISPLAY_NAME, "output");
		break;
	case ISP_VIDEO_CODEC:
		dir = 0;
		nr = ISP_VIDEO_NR_BASE + 1;
		snprintf(vnode->vdev.name, sizeof(vnode->vdev.name),
			"mvisp %s %s", ISP_VIDEO_CODEC_NAME, "output");
		break;
	case ISP_VIDEO_CCIC:
		dir = 0;
		nr = ISP_VIDEO_NR_BASE + 3;
		snprintf(vnode->vdev.name, sizeof(vnode->vdev.name),
			"mvisp %s %s", ISP_VIDEO_CCIC1_NAME, "output");
		break;
	default:
		return -EINVAL;
	}

	return map_vnode_add(vnode, vdev, dir, MIN_DRV_BUF, nr);
}

void mvisp_video_unregister(struct map_vnode *vnode)
{
	map_vnode_remove(vnode);
}
