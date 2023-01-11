#ifndef _FLASH_SUBDEV_H_
#define _FLASH_SUBDEV_H_

#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/b52socisp/host_isd.h>
struct flash_data {
	struct v4l2_device *v4l2_dev;
	struct isp_host_subdev *hsd;
};

struct flash_subdev {
	char			*name;
	struct v4l2_subdev	subdev;
	struct device		*dev;
	struct media_pad	pad;
	/* Controls */
};

int flash_subdev_create(struct device *parent,
		const char *name, int id, void *pdata);

#endif
