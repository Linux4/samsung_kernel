/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VERTEX_SYSTEM_H__
#define __VERTEX_SYSTEM_H__

#include "platform/vertex-clk.h"
#include "platform/vertex-ctrl.h"
#include "vertex-interface.h"
#include "vertex-memory.h"
#include "vertex-binary.h"
#include "vertex-graphmgr.h"

struct vertex_device;

struct vertex_system {
	struct device			*dev;
	void __iomem			*reg_ss[VERTEX_REG_MAX];
	resource_size_t			reg_ss_size[VERTEX_REG_MAX];
	void __iomem			*itcm;
	resource_size_t			itcm_size;
	void __iomem			*dtcm;
	resource_size_t			dtcm_size;

	unsigned int			cam_qos;
	unsigned int			mif_qos;

	const struct vertex_clk_ops	*clk_ops;
	const struct vertex_ctrl_ops	*ctrl_ops;
	struct pinctrl                  *pinctrl;
	struct vertex_memory		memory;
	struct vertex_interface		interface;
	struct vertex_binary		binary;
	struct vertex_graphmgr		graphmgr;

	struct vertex_device		*device;
};

int vertex_system_fw_bootup(struct vertex_system *sys);

int vertex_system_resume(struct vertex_system *sys);
int vertex_system_suspend(struct vertex_system *sys);

int vertex_system_start(struct vertex_system *sys);
int vertex_system_stop(struct vertex_system *sys);
int vertex_system_open(struct vertex_system *sys);
int vertex_system_close(struct vertex_system *sys);

int vertex_system_probe(struct vertex_device *device);
void vertex_system_remove(struct vertex_system *sys);

#endif
