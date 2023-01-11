/*
 * b52isp.c
 *
 * Marvell B52 ISP driver, based on soc-isp framework
 *
 * Copyright:  (C) Copyright 2013 Marvell Technology Shanghai Ltd.
 *
 */
#ifndef _HOST_ISD_H
#define _HOST_ISD_H

#include <media/b52socisp/b52socisp-mdev.h>

enum tlist_entry_type {
	TLIST_ENTRY_NONE	= 0,
	TLIST_ENTRY_MLINK,
	TLIST_ENTRY_SUBDEV,
};
struct isp_host_subdev {
	struct isp_subdev	isd;
	struct device		*dev;

	struct list_head	trvrs_list;
	enum tlist_entry_type	type;

	int (*group)(struct isp_host_subdev *hsd, u32 group);
};

struct host_subdev_pdata {
	const struct v4l2_subdev_ops	*subdev_ops;
	int	(*group)(struct isp_host_subdev *hsd, u32 group);
	const char	*name;
};

int host_subdev_add_guest(struct isp_host_subdev *hsd,
				struct v4l2_subdev *guest);
int host_subdev_remove_guest(struct isp_host_subdev *hsd,
				struct v4l2_subdev *guest);
struct isp_host_subdev *host_subdev_create(struct device *dev,
					const char *name, int id,
					struct host_subdev_pdata *pdata);
extern const struct v4l2_subdev_ops hsd_cascade_ops;
extern struct host_subdev_pdata hsd_cascade_behaviors;
extern const struct v4l2_subdev_ops hsd_bundle_ops;
extern struct host_subdev_pdata hsd_bundle_behaviors;
#endif
