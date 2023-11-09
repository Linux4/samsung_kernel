/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_HW_FD_H
#define FIMC_IS_HW_FD_H

#include "fimc-is-hw-control.h"
#include "fimc-is-api-common.h"
#include "api/fimc-is-hw-api-fd.h"

struct fimc_is_hw_fd_shadow_sw {
	bool first_trigger;
	enum control_command cur_cmd;
	enum control_command next_cmd;
};

struct fimc_is_hw_fd {
	enum hw_api_fd_format_mode mode;
	struct fimc_is_hw_fd_shadow_sw shadow_sw;
	struct hw_api_fd_setfile *setfile;
};

int fimc_is_hw_fd_probe(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_interface *itf,
	struct fimc_is_interface_ischain *itfc,
	int id);
int fimc_is_hw_fd_open(struct fimc_is_hw_ip *hw_ip, u32 instance, u32 *size);
int fimc_is_hw_fd_init(struct fimc_is_hw_ip *hw_ip, struct fimc_is_group *group,
		bool flag, u32 module_id);
int fimc_is_hw_fd_close(struct fimc_is_hw_ip *hw_ip, u32 instance);
int fimc_is_hw_fd_enable(struct fimc_is_hw_ip *hw_ip, u32 instance, unsigned long hw_map);
int fimc_is_hw_fd_disable(struct fimc_is_hw_ip *hw_ip, u32 instance, unsigned long hw_map);
int fimc_is_hw_fd_shot(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame, unsigned long hw_map);
int fimc_is_hw_fd_set_param(struct fimc_is_hw_ip *hw_ip,
	struct is_region *region,
	u32 lindex,
	u32 hindex,
	u32 instance,
	unsigned long hw_map);
int fimc_is_hw_fd_update_register(struct fimc_is_hw_ip *hw_ip);
int fimc_is_hw_fd_frame_ndone(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame, u32 instance, bool late_flag);
int fimc_is_hw_fd_reset(struct fimc_is_hw_ip *hw_ip);
int fimc_is_hw_fd_load_setfile(struct fimc_is_hw_ip *hw_ip, int index, u32 instance, unsigned long hw_map);
int fimc_is_hw_fd_apply_setfile(struct fimc_is_hw_ip *hw_ip, int index, u32 instance, unsigned long hw_map);
int fimc_is_hw_fd_delete_setfile(struct fimc_is_hw_ip *hw_ip, u32 instance, unsigned long hw_map);
int fimc_is_hw_fd_shadow_trigger(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame);
#endif
