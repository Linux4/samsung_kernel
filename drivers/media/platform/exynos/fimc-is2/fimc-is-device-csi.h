#ifndef FIMC_IS_DEVICE_CSI_H
#define FIMC_IS_DEVICE_CSI_H

#include <media/v4l2-device.h>
#include "mach/exynos-fimc-is2-sensor.h"
#include "fimc-is-type.h"

#ifndef ENABLE_IS_CORE
#define CSI_NOTIFY_VSYNC	10
#define CSI_NOTIFY_FLASH	11
#define CSI_NOTIFY_ACTUATOR	12
#define CSI_NOTIFY_M2M_ACTUATOR	13
#define SENSOR_NOTIFY_ACTUATOR_INIT 14
#endif

struct fimc_is_device_csi {
	/* channel information */
	u32				instance;
	u32 __iomem			*base_reg;

	/* for settle time */
	u32				sensor_cfgs;
	struct fimc_is_sensor_cfg	*sensor_cfg;

	/* for vci setting */
	u32				vcis;
	struct fimc_is_vci		*vci;
	int				irq[CSI_ID_MAX];

	/* image configuration */
	u32				mode;
	u32				lanes;
	struct fimc_is_image		image;
	/* pointer address from device sensor */
	struct v4l2_subdev		**subdev;

#ifndef ENABLE_IS_CORE
	/* for M2M AF */
	ulong						start_sec;
	ulong						start_usec;
	ulong						end_sec;
	ulong						end_usec;
	u32							valid_flag;
	u32							valid_time;
#endif

#ifndef ENABLE_IS_CORE
	/* DICO */
	atomic_t			vsync_count; /* Increase at CSI frame start */
	atomic_t			vblank_count; /* Increase at CSI frame end */

	atomic_t			vvalid; /* set 1 while vvalid period */
#endif
};

int __must_check fimc_is_csi_probe(void *parent, u32 instance);
int __must_check fimc_is_csi_open(struct v4l2_subdev *subdev);
int __must_check fimc_is_csi_close(struct v4l2_subdev *subdev);

#endif
