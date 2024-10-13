// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of_platform.h>

#include "is-video.h"
#include "is-core.h"
#include "pablo-debug.h"
#include "pablo-kernel-variant.h"
#include "pablo-v4l2.h"
#include "pablo-video-leader.h"
#include "pablo-video-subdev.h"
#include "pablo-device-iommu-group.h"

static inline void vref_init(struct is_video *video)
{
	atomic_set(&video->refcount, 0);
}

static enum is_device_type device_type_vid(int vid)
{
	if ((vid >= IS_VIDEO_SS0_NUM && vid <= IS_VIDEO_SS5_NUM)
	    || (vid >= IS_VIDEO_SS0VC0_NUM && vid < IS_VIDEO_SS5VC3_NUM))
		return IS_DEVICE_SENSOR;
	else
		return IS_DEVICE_ISCHAIN;
}

static void pablo_video_init(struct is_video *iv)
{
	struct is_core *core = is_get_is_core();

	vref_init(iv);
	mutex_init(&iv->lock);
	iv->resourcemgr = &core->resourcemgr;
}

int is_video_probe(struct is_video *video,
	const char *video_name,
	u32 video_number,
	u32 vfl_dir,
	struct v4l2_device *v4l2_dev,
	const struct v4l2_ioctl_ops *ioctl_ops)
{
	int ret = 0;
	u32 video_id;

	/* FIXME: remove after remove all 'is_video_probe()' call */
	pablo_video_init(video);

	snprintf(video->vd.name, sizeof(video->vd.name), "%s", video_name);
	video->id		= video_number;
	if (video_number == IS_VIDEO_COMMON)
		video->video_type = IS_VIDEO_TYPE_COMMON;
	else
		video->video_type = (vfl_dir == VFL_DIR_RX) ?
					IS_VIDEO_TYPE_CAPTURE : IS_VIDEO_TYPE_LEADER;
	video->device_type	= device_type_vid(video_number);

	video->vd.vfl_dir	= vfl_dir;
	video->vd.v4l2_dev	= v4l2_dev;
	video->vd.fops		= pablo_get_v4l2_file_ops_default();
	video->vd.ioctl_ops	= ioctl_ops ? ioctl_ops : pablo_get_v4l2_ioctl_ops_default();
	video->vd.minor		= -1;
	video->vd.release	= video_device_release;
	video->vd.lock		= &video->lock;
	video->vd.device_caps	= (vfl_dir == VFL_DIR_RX) ?
					VIDEO_CAPTURE_DEVICE_CAPS : VIDEO_OUTPUT_DEVICE_CAPS;
	video_set_drvdata(&video->vd, video);

	if (video->video_type == IS_VIDEO_TYPE_LEADER)
		video->pv_ops = pablo_get_pv_ops_leader();
	else if (video->video_type == IS_VIDEO_TYPE_CAPTURE)
		video->pv_ops = pablo_get_pv_ops_subdev();

	video_id = EXYNOS_VIDEONODE_FIMC_IS + video_number;
	ret = video_register_device(&video->vd, VFL_TYPE_PABLO, video_id);
	if (ret) {
		err("[V%02d] Failed to register video device", video->id);
		goto p_err;
	}

p_err:
	info("[VID] %s(%d) is created. minor(%d)\n", video_name, video_id, video->vd.minor);
	return ret;
}

static const struct of_device_id pablo_video_match[] = {
	{
		.name = "pablo-video",
		.compatible = "samsung,pablo-video",
	},
	{},
};
MODULE_DEVICE_TABLE(of, pablo_video_match);

static int pablo_video_set_mem(struct device *dev, struct is_video *iv)
{
	struct device_node *np = dev->of_node;
	struct device_node *iommu_np;
	struct platform_device *iommu_pdev;
	struct pablo_device_iommu_group *iommu_group;

	iommu_np = of_parse_phandle(np, "pablo,iommu-group", 0);
	if (iommu_np) {
		iommu_pdev = of_find_device_by_node(iommu_np);
		if (iommu_pdev) {
			iommu_group = platform_get_drvdata(iommu_pdev);
			iv->mem = &iommu_group->mem;
		} else {
			dev_warn(dev, "cannot find iommu group, deferring\n");
			return -EPROBE_DEFER;
		}
	} else {
		iv->mem = &iv->resourcemgr->mem;
	}

	return 0;
}

static void pablo_video_set_quirks(struct device *dev, struct is_video *iv)
{
	#define QUIRKS "pablo,quirks"
	#define DEC_MAP(id) { PABLO_VIDEO_QUIRK_STR_##id,\
			      PABLO_VIDEO_QUIRK_BIT_##id }

	struct device_node *np = dev->of_node;
	static const struct {
		const char *str;
		unsigned long bit;
	} map[] = {
		DEC_MAP(NEED_TO_KMAP),
		DEC_MAP(NEED_TO_REMAP),
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(map); i++) {
		if (of_property_match_string(np, QUIRKS, map[i].str) >= 0) {
			iv->quirks |= map[i].bit;
			dev_info(dev, "%s\n", map[i].str);
		}
	}
}

static const char * const pablo_device_type_name[] = {
	[IS_DEVICE_SENSOR] = "sensor",
	[IS_DEVICE_ISCHAIN] = "ischain",
};

static const char * const pablo_video_type_name[] = {
	[IS_VIDEO_TYPE_LEADER] = "leader",
	[IS_VIDEO_TYPE_CAPTURE] = "capture",
	[IS_VIDEO_TYPE_COMMON] = "common",
};

static int pablo_video_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct is_core *core = is_get_is_core();
	struct is_video *iv;
	unsigned int device_type, video_type;
	const char *video_name;
	unsigned int video_number, group_id, group_slot;
	unsigned int vfl_dir;
	int ret;

	iv = devm_kzalloc(dev, sizeof(*iv), GFP_KERNEL);
	if (!iv) {
		dev_err(dev, "failed to allocate video\n");
		return -ENOMEM;
	}

	pablo_video_init(iv);

	ret = of_property_read_u32(np, "device-type", &device_type);
	if (ret) {
		dev_err(dev, "failed to get device-type property\n");
		goto err_parse_dt;
	}

	ret = of_property_read_u32(np, "video-type", &video_type);
	if (ret) {
		dev_err(dev, "failed to get video-type property\n");
		goto err_parse_dt;
	}

	ret = of_property_read_string(np, "video-name", &video_name);
	if (ret) {
		dev_err(dev, "failed to get video-name property\n");
		goto err_parse_dt;
	}

	ret = of_property_read_u32(np, "video-number", &video_number);
	if (ret) {
		dev_err(dev, "failed to get video-number property\n");
		goto err_parse_dt;
	}

	ret = of_property_read_u32(np, "group-id", &group_id);
	if (ret) {
		dev_err(dev, "failed to get group-id property\n");
		goto err_parse_dt;
	}

	ret = of_property_read_u32(np, "group-slot", &group_slot);
	if (ret) {
		dev_err(dev, "failed to get group-slot property\n");
		goto err_parse_dt;
	}

	ret = pablo_video_set_mem(dev, iv);
	if (ret)
		goto err_parse_dt;

	pablo_video_set_quirks(dev, iv);

	dev_info(dev, "adding %s %s %s for group(id: %d, slot: [%d])",
		 video_name, pablo_device_type_name[device_type],
		 pablo_video_type_name[video_type], group_id, group_slot);

	if (video_type == IS_VIDEO_TYPE_LEADER)
		vfl_dir = VFL_DIR_M2M;
	else
		vfl_dir = VFL_DIR_RX;

	iv->group_id = group_id;
	iv->group_slot = group_slot;
	iv->subdev_ofs = offsetof(struct is_group, leader);

	ret = is_video_probe(iv, video_name, video_number, vfl_dir,
			     &core->v4l2_dev, NULL);
	if (!ret)
		return 0;

err_parse_dt:
	devm_kfree(dev, iv);

	return ret;
}

static struct platform_driver pablo_video_driver = {
	.probe  = pablo_video_probe,
	.driver = {
		.name = "pablo-video",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(pablo_video_match),
	},
};

struct platform_driver *pablo_video_get_platform_driver(void)
{
	return &pablo_video_driver;
}

MODULE_DESCRIPTION("Samsung EXYNOS SoC Pablo video driver");
MODULE_ALIAS("platform:samsung-pablo-video");
MODULE_LICENSE("GPL v2");
