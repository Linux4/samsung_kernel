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

#include "votf/pablo-votf.h"
#include "is-votfmgr.h"
#include "is-votf-id-table.h"
#include "is-groupmgr.h"
#include "is-device-ischain.h"
#include "pablo-debug.h"

static struct pablo_votfitf_ops is_votfitf_ops = {
	.check_wait_con = votfitf_check_wait_con,
	.check_invalid_state = votfitf_check_invalid_state,
	.create_link = votfitf_create_link,
	.set_flush = votfitf_set_flush,
};

struct pablo_votfitf_ops *pablo_get_votfitf(void)
{
	return &is_votfitf_ops;
}
KUNIT_EXPORT_SYMBOL(pablo_get_votfitf);

static int is_votf_get_votf_info(struct is_group *group, struct votf_info *src_info,
	struct votf_info *dst_info, char *caller_fn)
{
	unsigned int src_id, src_entry, dst_id, dst_entry;
	struct is_group *src_gr = NULL;
	struct is_group *dst_gr = NULL;
	u32 votf_mode = VOTF_NORMAL;

	FIMC_BUG(!group);

	is_votf_get_master_vinfo(group, &src_gr, &src_id, &src_entry);
	if (!src_gr) {
		mgerr("Failed to get master vinfo(%s)", group, group, caller_fn);
		return -EINVAL;
	}

	is_votf_get_slave_vinfo(group, &dst_gr, &dst_id, &dst_entry);

	src_id = src_gr->id;

	src_info->service = TWS;
	src_info->ip = is_get_votf_ip(src_id);
	src_info->id = is_get_votf_id(src_id, src_entry);
	src_info->mode = votf_mode;

	dst_info->service = TRS;
	dst_info->ip = is_get_votf_ip(dst_id);
	dst_info->id = is_get_votf_id(dst_id, dst_entry);
	dst_info->mode = votf_mode;

	if (caller_fn)
		mginfo("[%s] IMG: %s(TWS[ip/id:0x%x/%d]-TRS[ip/id:0x%x:%d] mode %u)\n",
			group, group, caller_fn, __func__, src_info->ip, src_info->id, dst_info->ip, dst_info->id, votf_mode);

	return 0;
}

static int is_votf_flush(struct is_group *group)
{
	int ret;
	struct votf_info src_info, dst_info;

	ret = is_votf_get_votf_info(group, &src_info, &dst_info, (char *)__func__);
	if (ret)
		return ret;

	ret = __is_votf_force_flush(&src_info, &dst_info);
	if (ret)
		mgerr("__is_votf_flush is fail(%d)", group, group, ret);

	is_votf_subdev_flush(group);

	return 0;
}

static int is_votf_check_status(struct is_group *group, bool check_wait)
{
	int ret;
	struct votf_info src_info, dst_info;

	ret = is_votf_get_votf_info(group, &src_info, &dst_info, NULL);
	if (ret)
		return ret;

	if (check_wait)
		ret = pablo_get_votfitf()->check_wait_con(&src_info, &dst_info);
	else
		ret = pablo_get_votfitf()->check_invalid_state(&src_info, &dst_info);
	if (ret)
		mgerr("votfitf_check_%s is fail(%d)",
			group, group, check_wait?"wait_con":"invalid_state", ret);

	return 0;
}

int is_votf_check_wait_con(struct is_group *group)
{
	return is_votf_check_status(group, true);
}
KUNIT_EXPORT_SYMBOL(is_votf_check_wait_con);

int is_votf_check_invalid_state(struct is_group *group)
{
	return is_votf_check_status(group, false);
}
KUNIT_EXPORT_SYMBOL(is_votf_check_invalid_state);

int is_votf_link_set_service_cfg(struct votf_info *src_info,
		struct votf_info *dst_info,
		u32 width, u32 height, u32 change)
{
	int ret = 0;
	struct votf_service_cfg cfg;
	struct votf_lost_cfg lost_cfg;

	memset(&cfg, 0, sizeof(struct votf_service_cfg));

	/* TWS: Master */
	src_info->service = TWS;

	cfg.enable = 0x1;
	/*
	 * 0xFF is max value.
	 * Buffer size is (limit x token_size).
	 * But VOTF can hold only 1 frame.
	 */
	cfg.limit = 0xFF;
	cfg.width = width;
	cfg.height = height & 0xFFFF;
	cfg.token_size = is_votf_get_token_size(src_info);
	cfg.connected_ip = dst_info->ip;
	cfg.connected_id = dst_info->id;
	cfg.option = change;

	ret = votfitf_set_service_cfg(src_info, &cfg);
	if (ret < 0) {
		ret = -EINVAL;
		err("TWS votf set_service_cfg fail. TWS 0x%04X-%d TRS 0x%04X-%d",
				src_info->ip, src_info->id,
				dst_info->ip, dst_info->id);

		return ret;
	}

	/* TRS: Slave */
	dst_info->service = TRS;

	cfg.enable = 0x1;
	/*
	 * 0xFF is max value.
	 * Buffer size is (limit x token_size).
	 * But VOTF can hold only 1 frame.
	 */
	cfg.limit = 0xFF;
	cfg.width = width;
	cfg.height = (height >> 16) ? (height >> 16) : height;
	cfg.token_size = is_votf_get_token_size(dst_info);
	cfg.connected_ip = src_info->ip;
	cfg.connected_id = src_info->id;
	cfg.option = change;

	ret = votfitf_set_service_cfg(dst_info, &cfg);
	if (ret < 0) {
		ret = -EINVAL;
		err("TRS votf set_service_cfg fail. TWS 0x%04X-%d TRS 0x%04X-%d",
				src_info->ip, src_info->id,
				dst_info->ip, dst_info->id);

		return ret;
	}

	memset(&lost_cfg, 0, sizeof(struct votf_lost_cfg));

	lost_cfg.recover_enable = 0x1;
	/* lost_cfg.flush_enable = 0x1; */
	ret = votfitf_set_trs_lost_cfg(dst_info, &lost_cfg);
	if (ret < 0) {
		ret = -EINVAL;
		err("TRS votf set_lost_cfg fail. TWS 0x%04X-%d TRS 0x%04X-%d",
				src_info->ip, src_info->id,
				dst_info->ip, dst_info->id);
		return ret;
	}

	info(" VOTF create link size %dx%d TWS 0x%04X-%d(%d) TRS 0x%04X-%d(%d)\n",
			width, height,
			src_info->ip, src_info->id, src_info->mode,
			dst_info->ip, dst_info->id, dst_info->mode);

	return 0;
}
KUNIT_EXPORT_SYMBOL(is_votf_link_set_service_cfg);

int __is_votf_force_flush(struct votf_info *src_info,
		struct votf_info *dst_info)
{
	int ret = 0;

	/* TWS: Master */
	src_info->service = TWS;

	ret = pablo_get_votfitf()->set_flush(src_info);
	if (ret < 0)
		err("TWS votf_flush fail. TWS 0x%04X-%d TRS 0x%04X-%d",
			src_info->ip, src_info->id,
			dst_info->ip, dst_info->id);

	/* TRS: Slave */
	dst_info->service = TRS;

	ret = pablo_get_votfitf()->set_flush(dst_info);
	if (ret < 0)
		err("TRS votf_flush fail. TWS 0x%04X-%d TRS 0x%04X-%d",
			src_info->ip, src_info->id,
			dst_info->ip, dst_info->id);

	info("VOTF flush TWS 0x%04X-%d TRS 0x%04X-%d\n",
		src_info->ip, src_info->id,
		dst_info->ip, dst_info->id);

	return 0;
}
KUNIT_EXPORT_SYMBOL(__is_votf_force_flush);

int is_votf_create_link(struct is_group *group, u32 width, u32 height)
{
	int ret = 0;
	struct votf_info src_info, dst_info;
	u32 change_option = VOTF_OPTION_MSK_COUNT;

	if (group->prev && group->prev->device_type == IS_DEVICE_SENSOR)
		return 0;

	ret = is_votf_get_votf_info(group, &src_info, &dst_info, (char *)__func__);
	if (ret)
		return ret;

#if defined(USE_VOTF_AXI_APB)
	pablo_get_votfitf()->create_link(src_info.ip, dst_info.ip);

	ret = is_votf_subdev_create_link(group);
	if (ret) {
		mgerr("failed to create suvdev link", group, group);
		goto err_subdev_create_link;
	}
#endif

	ret = is_votf_link_set_service_cfg(&src_info, &dst_info,
			width, height, change_option);
	if (ret)
		goto err_link_set_service_cfg;

	return 0;

err_link_set_service_cfg:
#if defined(USE_VOTF_AXI_APB)
	is_votf_subdev_destroy_link(group);

err_subdev_create_link:
	votfitf_destroy_link(src_info.ip, dst_info.ip);
#endif

	return ret;
}
KUNIT_EXPORT_SYMBOL(is_votf_create_link);

int is_votf_destroy_link(struct is_group *group)
{
	int ret = 0;
	struct votf_info src_info, dst_info;

	if (group->id >= GROUP_ID_MAX) {
		mgerr("group ID is invalid(%u)", group, group, group->id);

		/* Do not access C2SERV after power off */
		return 0;
	}

	if (group->prev && group->prev->device_type == IS_DEVICE_SENSOR)
		return 0;

	ret = is_votf_get_votf_info(group, &src_info, &dst_info, (char *)__func__);
	if (ret)
		return ret;

	ret = is_votf_flush(group);
	if (ret)
		mgerr("is_votf_flush is fail(%d)", group, group, ret);

	/*
	 * All VOTF control such as flush must be set in ring clock enable and ring enable.
	 * So, calling votfitf_destroy_ring must be called at the last.
	 */
#if defined(USE_VOTF_AXI_APB)
	votfitf_destroy_link(src_info.ip, dst_info.ip);

	is_votf_subdev_destroy_link(group);
#endif

	return 0;
}
KUNIT_EXPORT_SYMBOL(is_votf_destroy_link);

int is_votf_check_link(struct is_group *group)
{
	return test_bit(IS_GROUP_VOTF_CONN_LINK, &group->state);
}
KUNIT_EXPORT_SYMBOL(is_votf_check_link);
