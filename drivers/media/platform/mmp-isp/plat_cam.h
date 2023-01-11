/*
 * plat_cam.h
 *
 * Marvell B52 ISP - Top level module
 *
 * Copyright:  (C) Copyright 2014 Marvell International Ltd.
 *              Jiaquan Su <jqsu@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */


#ifndef _PLAT_CAM_H
#define _PLAT_CAM_H

#include <media/v4l2-device.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/wait.h>

#include <media/b52socisp/b52socisp-mdev.h>
#include <media/b52socisp/b52socisp-vdev.h>

struct plat_cam {
	/* platform HW resources */
	struct isp_build	*isb;
	struct msc2_mmu_dev	*mmu_dev;
	atomic_t		mmu_ref;
	struct list_head	vnode_pool;
	struct list_head	host_pool;
};

struct plat_vnode {
	int no_zoom;
	struct list_head	hook;
	struct mmu_chs_desc	mmu_ch_dsc;
	int	(*alloc_mmu_chnl)(struct plat_cam *pcam, __u8 ip_id,
					__u8 blk_id, __u8 port_id, __u8 nr_chnl,
					struct mmu_chs_desc *ch_dsc);
	void			(*free_mmu_chnl)(struct plat_cam *pcam,
					struct mmu_chs_desc *ch_dsc);
	int			(*fill_mmu_chnl)(struct plat_cam *pcam,
					struct vb2_buffer *vb, int num_planes);
	int (*reset_mmu_chnl)(struct plat_cam *pcam,
					struct isp_vnode *vnode, int num_planes);
	__u16			(*get_axi_id)(__u8 port_id, __u8 yuv_id);
	struct isp_vnode	vnode;
	struct plat_topology	*plat_topo;
};

#define MAX_OUTPUT_PER_PIPELINE	6
enum plat_src_type {
	PLAT_SRC_T_SNSR	= 0,
	PLAT_SRC_T_VDEV,
	PLAT_SRC_T_CNT,
};
struct plat_topology {
	struct media_pipeline	mpipe;
	enum plat_src_type	src_type;
	union {
		struct v4l2_subdev	*sensor;
		struct isp_vnode	*vnode;
	} src;
	struct isp_subdev	*scalar_a;
	struct v4l2_rect	*crop_a;
	struct isp_subdev	*path;
	struct v4l2_rect	*crop_b;
	struct isp_subdev	*scalar_b[MAX_OUTPUT_PER_PIPELINE];
	struct isp_vnode	*dst[MAX_OUTPUT_PER_PIPELINE];
	int			dst_map;
};

#ifdef CONFIG_HOST_SUBDEV
struct plat_hsd {
	struct isp_host_subdev	*hsd;
	struct list_head	hook;
	char	name[64];
	int	drv_own;
	int	(*link)(struct plat_hsd *hsd, u32 link,
			struct media_entity *start,
			struct media_entity *end);
	int	(*init)(struct plat_hsd *hsd, u32 init,
			struct media_entity *start,
			struct media_entity *end);
};
#endif

enum {
	PCAM_IP_B52ISP	= 0,
	PCAM_IP_CCICV2,
	PCAM_IP_CNT,
};

enum plat_subdev_code {
	SDCODE_B52ISP_IDI1	= 0,
	SDCODE_B52ISP_IDI2,
	SDCODE_B52ISP_PIPE1,
	SDCODE_B52ISP_DUMP1,
	SDCODE_B52ISP_MS1,
	SDCODE_B52ISP_PIPE2,
	SDCODE_B52ISP_DUMP2,
	SDCODE_B52ISP_MS2,
	SDCODE_B52ISP_HS,
	SDCODE_B52ISP_HDR,
	SDCODE_B52ISP_3D,
	SDCODE_B52ISP_A1W1,
	SDCODE_B52ISP_A1W2,
	SDCODE_B52ISP_A1R1,
	SDCODE_B52ISP_A2W1,
	SDCODE_B52ISP_A2W2,
	SDCODE_B52ISP_A2R1,
	SDCODE_B52ISP_A3W1,
	SDCODE_B52ISP_A3W2,
	SDCODE_B52ISP_A3R1,
	SDCODE_CCICV2_CSI0,
	SDCODE_CCICV2_CSI1,
	SDCODE_CCICV2_DMA0,
	SDCODE_CCICV2_DMA1,
	SDCODE_HOST_SUBDEV_BASE,
	SDCODE_HOST_SUBDEV_END	= SDCODE_HOST_SUBDEV_BASE + 5,
	SDCODE_CNT,
};

int plat_ispsd_register(struct isp_subdev *ispsd);
void plat_ispsd_unregister(struct isp_subdev *ispsd);
int plat_resrc_register(struct device *dev, struct resource *res,
	const char *name, struct block_id mask,
	int res_id, void *handle, void *priv);
int plat_vnode_get_topology(struct plat_vnode *pvnode,
				struct plat_topology *topo);
int plat_topo_get_output(struct plat_topology *ppl);
__u32 plat_get_src_tag(struct plat_topology *ppl);
int b52isp_idi_change_clock(struct isp_block *block, int w, int h, int fps);
int plat_tune_isp(int on);
#endif	/* PLAT_CAM_H */
