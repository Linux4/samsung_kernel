/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>

#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>

#include "fimc-is-control-sensor.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-device-sensor-peri.h"

int fimc_is_actuator_ctl_set_position(struct fimc_is_device_sensor *device,
					u32 position)
{
	int ret = 0;
	struct v4l2_control v4l2_ctrl;

	BUG_ON(!device);

	v4l2_ctrl.id = V4L2_CID_ACTUATOR_SET_POSITION;
	v4l2_ctrl.value = position;

	ret = fimc_is_sensor_s_ctrl(device, &v4l2_ctrl);
	if (ret < 0) {
		err("Actuator set position fail\n");
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_actuator_ctl_convert_position(u32 *pos,
				u32 src_max_pos, u32 src_direction,
				u32 tgt_max_pos, u32 tgt_direction)
{
	int ret = 0;
	u32 convert_pos = *pos;

	if (convert_pos >= (1 << src_max_pos)) {
		err("Actuator convert position size error\n");
		ret = -EINVAL;
		goto p_err;
	}

	if ((src_direction > ACTUATOR_RANGE_MAC_TO_INF) ||
			(tgt_direction > ACTUATOR_RANGE_MAC_TO_INF)) {
		err("Actuator convert direction error\n");
		ret = -EINVAL;
		goto p_err;
	}

	/* Convert bitage */
	if (src_max_pos < tgt_max_pos)
		convert_pos <<= (tgt_max_pos - src_max_pos);
	else if (src_max_pos > tgt_max_pos)
		convert_pos >>= (src_max_pos - tgt_max_pos);

	/* Convert Direction */
	if (src_direction != tgt_direction)
		convert_pos = ((1 << tgt_max_pos) - 1) - convert_pos;

	*pos = convert_pos;

p_err:

	return ret;
}

int fimc_is_actuator_ctl_search_position(u32 position,
		u32 *position_table,
		enum fimc_is_actuator_direction direction,
		u32 *searched_pos)
{
	int middle = 0, left = 0;
	int right = ACTUATOR_MAX_FOCUS_POSITIONS - 1;

	BUG_ON(!position_table);
	BUG_ON(!searched_pos);

	*searched_pos = 0;

	while (right >= left) {
		middle = (right + left) >> 1;

		if ((middle < 0) || (middle >= ACTUATOR_MAX_FOCUS_POSITIONS)) {
			err("%s: Invalid search argument\n", __func__);
			return -EINVAL;
		}

		if (position == position_table[middle]) {
			*searched_pos = (u32)middle;
			return 0;
		}

		if (direction == ACTUATOR_RANGE_INF_TO_MAC) {
			if (position < position_table[middle])
				right = middle - 1;
			else
				left = middle + 1;
		} else {
			if (position > position_table[middle])
				right = middle - 1;
			else
				left = middle + 1;
		}
	}

	warn("No item in array! HW pos: %d(Closest pos: real %d, virtual %d)\n",
			position, position_table[middle], middle);
	*searched_pos = (u32)middle;

	return 0;
}
