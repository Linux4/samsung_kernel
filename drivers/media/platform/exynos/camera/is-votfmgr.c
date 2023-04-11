/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>

#include "votf/camerapp-votf.h"
#include "is-votfmgr.h"
#include "is-votf-id-table.h"
#include "is-groupmgr.h"
#include "is-device-ischain.h"
#include "is-device-sensor.h"

static int __is_votf_create_link(int src_ip, int src_id, int dst_ip, int dst_id,
	u32 width, u32 height, u32 change)
{
	int ret = 0;
	struct votf_info vinfo;
	struct votf_service_cfg cfg;
	struct votf_lost_cfg lost_cfg;

	memset(&vinfo, 0, sizeof(struct votf_info));
	memset(&cfg, 0, sizeof(struct votf_service_cfg));

	/* TWS: Master */
	vinfo.service = TWS;
	vinfo.ip = src_ip;
	vinfo.id = src_id;

	cfg.enable = 0x1;
	/*
	 * 0xFF is max value.
	 * Buffer size is (limit x token_size).
	 * But VOTF can hold only 1 frame.
	 */
	cfg.limit = 0xFF;
	cfg.width = width;
	cfg.height = height;
	cfg.token_size = 0x1F;
	cfg.connected_ip = dst_ip;
	cfg.connected_id = dst_id;
	cfg.option = change << VOTF_OPTION_BIT_CHANGE;

	ret = votfitf_set_service_cfg(&vinfo, &cfg);
	if (ret < 0) {
		ret = -EINVAL;
		err("votfitf_set_service_cfg for TWS is fail (src(%d, %d), dst(%d, %d))",
			src_ip, src_id, dst_ip, dst_id);
		return ret;
	}

	/* TRS: Slave */
	vinfo.service = TRS;
	vinfo.ip = dst_ip;
	vinfo.id = dst_id;

	cfg.enable = 0x1;
	/*
	 * 0xFF is max value.
	 * Buffer size is (limit x token_size).
	 * But VOTF can hold only 1 frame.
	 */
	cfg.limit = 0xFF;
	cfg.width = width;
	cfg.height = height;
	cfg.token_size = 0x1F;
	cfg.connected_ip = src_ip;
	cfg.connected_id = src_id;
	cfg.option = change << VOTF_OPTION_BIT_CHANGE;

	ret = votfitf_set_service_cfg(&vinfo, &cfg);
	if (ret < 0) {
		ret = -EINVAL;
		err("votfitf_set_service_cfg for TRS is fail (src(%d, %d), dst(%d, %d))",
			src_ip, src_id, dst_ip, dst_id);
		return ret;
	}

	memset(&lost_cfg, 0, sizeof(struct votf_lost_cfg));

	lost_cfg.recover_enable = 0x1;
	/* lost_cfg.flush_enable = 0x1; */
	ret = votfitf_set_trs_lost_cfg(&vinfo, &lost_cfg);
	if (ret < 0) {
		ret = -EINVAL;
		err("votfitf_set_trs_lost_cfg is fail (src(%d, %d), dst(%d, %d))",
			src_ip, src_id, dst_ip, dst_id);
		return ret;
	}

	info(" VOTF create link (size(%dx%d), src(%d, %d), dst(%d, %d))\n",
		width, height, src_ip, src_id, dst_ip, dst_id);

	return 0;
}

static int __is_votf_flush(int src_ip, int src_id, int dst_ip, int dst_id)
{
	int ret = 0;
	struct votf_info vinfo;

	memset(&vinfo, 0, sizeof(struct votf_info));

	/* TWS: Master */
	vinfo.service = TWS;
	vinfo.ip = src_ip;
	vinfo.id = src_id;

	ret = votfitf_set_flush(&vinfo);
	if (ret < 0)
		err("votfitf_set_flush for TWS is fail (src(%d, %d), dst(%d, %d))",
			src_ip, src_id, dst_ip, dst_id);

	/* TRS: Slave */
	vinfo.service = TRS;
	vinfo.ip = dst_ip;
	vinfo.id = dst_id;

	ret = votfitf_set_flush(&vinfo);
	if (ret < 0)
		err("votfitf_set_flush for TRS is fail (src(%d, %d), dst(%d, %d))",
			src_ip, src_id, dst_ip, dst_id);

	info("VOTF flush(src(%d, %d), dst(%d, %d))\n", src_ip, src_id, dst_ip, dst_id);

	return 0;
}

int is_votf_flush(struct is_group *group)
{
	int ret = 0;
	struct is_device_sensor *sensor;
	unsigned int src_ip, src_id, dst_ip, dst_id;
	struct is_group *src_gr;
	unsigned int src_gr_id;
	struct is_subdev *src_sd;
	int dma_ch;
	struct is_device_ischain *device;
	struct is_sensor_cfg *sensor_cfg;
	int pd_mode;

	FIMC_BUG(!group);
	FIMC_BUG(!group->prev);
	FIMC_BUG(!group->prev->junction);

	src_gr = group->prev;
	src_sd = group->prev->junction;

	if (src_gr->device_type == IS_DEVICE_SENSOR) {
		/*
		 * The sensor group id should be re calculated,
		 * because CSIS WDMA is not matched with sensor group id.
		 */
		sensor = src_gr->sensor;

		sensor_cfg = sensor->cfg;
		if (!sensor_cfg) {
			mgerr("failed to get sensor_cfg", group, group);
			return -EINVAL;
		}

		dma_ch = src_sd->dma_ch[sensor_cfg->scm];
		src_gr_id = GROUP_ID_SS0 + dma_ch;
	} else {
		src_gr_id = src_gr->id;
	}

	src_ip = is_votf_ip[src_gr_id];
	src_id = is_votf_id[src_gr_id][src_sd->id];

	dst_ip = is_votf_ip[group->id];
	dst_id = is_votf_id[group->id][group->leader.id];

	mginfo(" VOTF flush(TWS[%s:%s]-TRS[%s:%s])\n",
		group, group,
		group_id_name[src_gr->id], src_sd->name,
		group_id_name[group->id], group->leader.name);

	ret = __is_votf_flush(src_ip, src_id, dst_ip, dst_id);
	if (ret)
		mgerr("__is_votf_flush is fail(%d)", group, group, ret);

	if (src_gr->device_type == IS_DEVICE_SENSOR) {
		device = group->device;
		if (!device) {
			mgerr("failed to get devcie", group, group);
			return -ENODEV;
		}

		pd_mode = sensor_cfg->pd_mode;

		if (pd_mode == PD_MOD3) {
			src_id = is_votf_id[src_gr_id][ENTRY_SSVC1];
			dst_id = is_votf_id[group->id][ENTRY_PDAF];

			ret = __is_votf_flush(src_ip, src_id, dst_ip, dst_id);
			if (ret)
				mgerr("__is_votf_flush of subdev_pdaf is fail(%d)", group, group, ret);
		}
	}

	return 0;
}

int is_votf_create_link(struct is_group *group, u32 width, u32 height)
{
	int ret = 0;
	struct is_device_sensor *sensor;
	unsigned int src_ip, src_id, dst_ip, dst_id;
	struct is_group *src_gr;
	unsigned int src_gr_id;
	struct is_subdev *src_sd;
	int dma_ch;
	struct is_device_ischain *device;
	struct is_sensor_cfg *sensor_cfg;
	struct is_subdev *subdev_pdaf;
	int pd_mode;
	u32 pd_width, pd_height;
	u32 change_option = 0;

	FIMC_BUG(!group);
	FIMC_BUG(!group->prev);
	FIMC_BUG(!group->prev->junction);

	src_gr = group->prev;
	src_sd = group->prev->junction;

	if (src_gr->device_type == IS_DEVICE_SENSOR) {
		/*
		 * The sensor group id should be re calculated,
		 * because CSIS WDMA is not matched with sensor group id.
		 */
		sensor = src_gr->sensor;

		sensor_cfg = sensor->cfg;
		if (!sensor_cfg) {
			mgerr("failed to get sensor_cfg", group, group);
			return -EINVAL;
		}

		dma_ch = src_sd->dma_ch[sensor_cfg->scm];
		src_gr_id = GROUP_ID_SS0 + dma_ch;
	} else {
		src_gr_id = src_gr->id;
	}

	src_ip = is_votf_ip[src_gr_id];
	src_id = is_votf_id[src_gr_id][src_sd->id];

	dst_ip = is_votf_ip[group->id];
	dst_id = is_votf_id[group->id][group->leader.id];

	mginfo(" VOTF create link(TWS[%s:%s]-TRS[%s:%s])\n",
		group, group,
		group_id_name[src_gr->id], src_sd->name,
		group_id_name[group->id], group->leader.name);

	/* Call VOTF API */
	votfitf_create_ring();

	ret = __is_votf_create_link(src_ip, src_id, dst_ip, dst_id, width, height, change_option);
	if (ret)
		return ret;

	if (src_gr->device_type == IS_DEVICE_SENSOR) {
		device = group->device;
		if (!device) {
			mgerr("failed to get devcie", group, group);
			return -ENODEV;
		}

		pd_mode = sensor_cfg->pd_mode;

		if (pd_mode == PD_MOD3) {
			subdev_pdaf = &device->pdaf;

			pd_width = sensor_cfg->input[CSI_VIRTUAL_CH_1].width;
			pd_height = sensor_cfg->input[CSI_VIRTUAL_CH_1].height;

			ret = is_subdev_internal_open(device, IS_DEVICE_ISCHAIN, subdev_pdaf);
			if (ret) {
				merr("is_subdev_internal_open is fail(%d)", device, ret);
				return ret;
			}

			ret = is_subdev_internal_s_format(device, 0, subdev_pdaf,
						pd_width, pd_height, 2, NUM_OF_VOTF_BUF, "VOTF");
			if (ret) {
				merr("is_subdev_internal_s_format is fail(%d)", device, ret);
				return ret;
			}

			ret = is_subdev_internal_start(device, IS_DEVICE_ISCHAIN, subdev_pdaf);
			if (ret) {
				merr("subdev_pdaf internal start is fail(%d)", device, ret);
				return ret;
			}

			src_id = is_votf_id[src_gr_id][ENTRY_SSVC1];
			dst_id = is_votf_id[group->id][ENTRY_PDAF];

			ret = __is_votf_create_link(src_ip, src_id, dst_ip, dst_id,
				pd_width, pd_height, change_option);
			if (ret)
				return ret;
		}
	}

	return 0;
}

int is_votf_destroy_link(struct is_group *group)
{
	int ret = 0;
	struct is_device_sensor *sensor;
	struct is_group *src_gr;
	struct is_subdev *src_sd;
	struct is_device_ischain *device;
	struct is_sensor_cfg *sensor_cfg;
	struct is_subdev *subdev_pdaf;
	int pd_mode;

	FIMC_BUG(!group);
	FIMC_BUG(!group->prev);
	FIMC_BUG(!group->prev->junction);

	src_gr = group->prev;
	src_sd = group->prev->junction;

	if (src_gr->device_type == IS_DEVICE_SENSOR) {
		sensor = src_gr->sensor;

		device = group->device;
		if (!device) {
			mgerr("failed to get devcie", group, group);
			goto p_err;
		}

		sensor_cfg = sensor->cfg;
		if (!sensor_cfg) {
			mgerr("failed to get sensor_cfg", group, group);
			goto p_err;
		}

		pd_mode = sensor_cfg->pd_mode;

		if (pd_mode == PD_MOD3) {
			subdev_pdaf = &device->pdaf;
			ret = is_subdev_internal_stop(device, 0, subdev_pdaf);
			if (ret)
				merr("subdev internal stop is fail(%d)", device, ret);

			ret = is_subdev_internal_close(device, IS_DEVICE_ISCHAIN, subdev_pdaf);
			if (ret)
				merr("is_subdev_internal_close is fail(%d)", device, ret);
		}
	}

	if (group->id >= GROUP_ID_MAX) {
		mgerr("group ID is invalid(%d)", group, group);

		/* Do not access C2SERV after power off */
		return 0;
	}

	ret = is_votf_flush(group);
	if (ret)
		mgerr("is_votf_flush is fail(%d)", group, group, ret);

	mginfo(" VOTF destroy link(TWS[%s:%s]-TRS[%s:%s])\n",
		group, group,
		group_id_name[src_gr->id], src_sd->name,
		group_id_name[group->id], group->leader.name);

p_err:
	/*
	 * All VOTF control such as flush must be set in ring clock enable and ring enable.
	 * So, calling votfitf_destroy_ring must be called at the last.
	 */
	votfitf_destroy_ring();

	return 0;
}

int is_votf_change_link(struct is_group *group)
{
	int ret = 0;
	struct is_device_sensor *sensor;
	unsigned int src_ip, src_id, dst_ip, dst_id;
	struct is_group *src_gr;
	unsigned int src_gr_id;
	struct is_subdev *src_sd;
	int dma_ch;
	struct is_sensor_cfg *sensor_cfg;
	int pd_mode;
	u32 pd_width, pd_height;
	u32 width, height;
	u32 change_option = 1;

	FIMC_BUG(!group);
	FIMC_BUG(!group->prev);
	FIMC_BUG(!group->prev->junction);

	src_gr = group->prev;
	src_sd = group->prev->junction;

	if (src_gr->device_type == IS_DEVICE_SENSOR) {
		/*
		 * The sensor group id should be re calculated,
		 * because CSIS WDMA is not matched with sensor group id.
		 */
		sensor = src_gr->sensor;

		sensor_cfg = sensor->cfg;
		if (!sensor_cfg) {
			mgerr("failed to get sensor_cfg", group, group);
			return -EINVAL;
		}

		dma_ch = src_sd->dma_ch[sensor_cfg->scm];
		src_gr_id = GROUP_ID_SS0 + dma_ch;
	} else {
		src_gr_id = src_gr->id;
	}

	src_ip = is_votf_ip[src_gr_id];
	src_id = is_votf_id[src_gr_id][src_sd->id];

	dst_ip = is_votf_ip[group->id];
	dst_id = is_votf_id[group->id][group->leader.id];

	mginfo(" VOTF change link(TWS[%s:%s]-TRS[%s:%s])\n",
		group, group,
		group_id_name[src_gr->id], src_sd->name,
		group_id_name[group->id], group->leader.name);

	width = group->leader.input.width;
	height = group->leader.input.height;

	ret = __is_votf_create_link(src_ip, src_id, dst_ip, dst_id, width, height, change_option);
	if (ret)
		return ret;

	if (src_gr->device_type == IS_DEVICE_SENSOR) {
		pd_mode = sensor_cfg->pd_mode;

		if (pd_mode == PD_MOD3) {
			pd_width = sensor_cfg->input[CSI_VIRTUAL_CH_1].width;
			pd_height = sensor_cfg->input[CSI_VIRTUAL_CH_1].height;

			src_id = is_votf_id[src_gr_id][ENTRY_SSVC1];
			dst_id = is_votf_id[group->id][ENTRY_PDAF];

			ret = __is_votf_create_link(src_ip, src_id, dst_ip, dst_id,
				pd_width, pd_height, change_option);
			if (ret)
				return ret;
		}
	}

	return 0;
}

struct is_framemgr *is_votf_get_framemgr(struct is_group *group,  enum votf_service type,
	unsigned long subdev_id)
{
	struct is_framemgr *framemgr;
	struct is_subdev *subdev;
	struct is_device_ischain *device;
	struct is_subdev *subdev_pdaf;

	FIMC_BUG_NULL(!group);

	if (type == TWS) {
		if (!group->next) {
			mgerr("The next group is NULL (subdev_id: %ld)\n", group, group, subdev_id);
			return NULL;
		}

		subdev = &group->next->leader;
	} else {
		subdev = &group->leader;
	}

	framemgr = GET_SUBDEV_I_FRAMEMGR(subdev);

	/* FIXME */
	if (subdev_id == ENTRY_SSVC1 || subdev_id == ENTRY_PDAF) {
		device = group->device;
		subdev_pdaf = &device->pdaf;

		framemgr = GET_SUBDEV_I_FRAMEMGR(subdev_pdaf);
	}

	FIMC_BUG_NULL(!framemgr);

	return framemgr;
}

struct is_frame *is_votf_get_frame(struct is_group *group,  enum votf_service type,
	unsigned long subdev_id, u32 fcount)
{
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	framemgr = is_votf_get_framemgr(group, type, subdev_id);
	if (!framemgr) {
		mgerr("framemgr is NULL (subdev_id: %ld)\n", group, group, subdev_id);
		return NULL;
	}

	frame = &framemgr->frames[framemgr->num_frames ? (fcount % framemgr->num_frames) : 0];

	return frame;
}

int is_votf_register_framemgr(struct is_group *group, enum votf_service type,
	void *data, votf_s_addr fn, unsigned long subdev_id)
{
	struct is_framemgr *framemgr;

	framemgr = is_votf_get_framemgr(group, type, subdev_id);
	if (!framemgr) {
		mgerr("framemgr is NULL. (subdev_id: %ld)\n", group, group, subdev_id);
		return -EINVAL;
	}

	if (type == TWS) {
		framemgr->master.id = subdev_id;
		framemgr->master.data = data;
		framemgr->master.s_addr = fn;
		mginfo("Register VOTF master callback (subdev_id: %ld)\n", group, group, subdev_id);
	} else {
		framemgr->slave.id = subdev_id;
		framemgr->slave.data = data;
		framemgr->slave.s_addr = fn;
		mginfo("Register VOTF slave callback (subdev_id: %ld)\n", group, group, subdev_id);
	}

	return 0;
}

int is_votf_register_oneshot(struct is_group *group, enum votf_service type,
	void *data, votf_s_oneshot fn, unsigned long subdev_id)
{
	struct is_framemgr *framemgr;

	framemgr = is_votf_get_framemgr(group, type, subdev_id);
	if (!framemgr) {
		mgerr("framemgr is NULL. (subdev_id: %ld)\n", group, group, subdev_id);
		return -EINVAL;
	}

	if (type == TWS) {
		mgwarn("Invalid type for VOTF oneshot trigger register (subdev_id: %ld)\n", group, group, subdev_id);
	} else {
		framemgr->slave.id = subdev_id;
		framemgr->slave.data = data;
		framemgr->slave.s_oneshot = fn;
		mginfo("Register VOTF slave oneshot trigger (subdev_id: %ld)\n", group, group, subdev_id);
	}

	return 0;
}
