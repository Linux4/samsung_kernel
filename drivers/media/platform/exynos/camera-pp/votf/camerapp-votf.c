/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung EXYNOS VOTF driver
 * (FIMC-IS PostProcessing Generic Distortion Correction driver)
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include "camerapp-votf.h"

static struct votf_dev *votfdev;

static bool check_vinfo(struct votf_info *vinfo)
{
	if (!vinfo) {
		pr_err("%s: votf info is null\n", __func__);
		return false;
	}
	if (vinfo->service >= SERVICE_CNT || vinfo->ip >= IP_MAX || vinfo->id >= ID_MAX) {
		pr_err("%s: invalid votf input(svc:%d, ip:%d, id:%d)\n", __func__,
			vinfo->service, vinfo->ip, vinfo->id);
		return false;
	}
	return true;
}

u32 get_offset(struct votf_info *vinfo, int c2s_tws, int c2s_trs, int c2a_tws, int c2a_trs)
{
	u32 offset = 0;
	u32 reg_offset = 0;
	u32 service_offset = 0;
	u32 gap = 0;
	int module;
	int module_type;
	int ip, id, service;

	if (!votfdev || !check_vinfo(vinfo))
		return VOTF_INVALID;

	service = vinfo->service;
	ip = vinfo->ip;
	id = vinfo->id;

	module = votfdev->votf_table[service][ip][id].module;
	module_type = votfdev->votf_table[service][ip][id].module_type;

	if (module == C2SERV) {
		if (service == TWS) {
			if (c2s_tws == -1)
				return VOTF_INVALID;
			reg_offset = c2serv_regs[c2s_tws].sfr_offset;
			service_offset = votfdev->votf_module_addr[module_type].tws_addr;
			gap = votfdev->votf_module_addr[module_type].tws_gap;

			/* Between m16s16 tws8 and tws9, the gap is not 0x1C but 0x20 */
			if ((module_type == M16S16) && (id >= 9))
				offset += 0x4;
		} else {
			if (c2s_trs == -1)
				return VOTF_INVALID;
			reg_offset = c2serv_regs[c2s_trs].sfr_offset;
			service_offset = votfdev->votf_module_addr[module_type].trs_addr;
			gap = votfdev->votf_module_addr[module_type].trs_gap;

			if (module_type == M16S16) {
				if (id >= 5)
					offset += 0x24;
				if (id >= 10)
					offset += 0x24;
				if (id >= 15)
					offset += 0x24;
			}
		}
	} else {
		if (service == TWS) {
			if (c2a_tws == -1)
				return VOTF_INVALID;
			reg_offset = c2agent_regs[c2a_tws].sfr_offset;
			service_offset = votfdev->votf_module_addr[module_type].tws_addr;
			gap = votfdev->votf_module_addr[module_type].tws_gap;
		} else {
			if (c2a_trs == -1)
				return VOTF_INVALID;
			reg_offset = c2agent_regs[c2a_trs].sfr_offset;
			service_offset = votfdev->votf_module_addr[module_type].trs_addr;
			gap = votfdev->votf_module_addr[module_type].trs_gap;
		}
	}
	offset += service_offset + (id * gap) + reg_offset;

	return offset;
}

void votf_init(void)
{
	if (!votfdev) {
		pr_err("%s: votf devices is NULL\n", __func__);
		return;
	}
	mutex_init(&votfdev->votf_lock);
	mutex_lock(&votfdev->votf_lock);

	votfdev->ring_create = false;
	votfdev->ring_request = 0;
	votfdev->dev = NULL;

	memset(votfdev->ring_pair, VS_DISCONNECTED, sizeof(votfdev->ring_pair));
	memset(votfdev->votf_cfg, 0, sizeof(struct votf_service_cfg));
	memset(votfdev->votf_module_addr, 0, sizeof(struct votf_module_type_addr));
	memset(votfdev->votf_table, 0, sizeof(struct votf_table_info));

	mutex_unlock(&votfdev->votf_lock);
}

void votf_sfr_dump(void)
{
	int svc, ip, id;
	int module;
	int module_type;

	for (svc = 0; svc < SERVICE_CNT; svc++) {
		for (ip = 0; ip < IP_MAX; ip++) {
			for (id = 0; id < ID_MAX; id++) {
				if (!votfdev->votf_table[svc][ip][id].use)
					continue;

				module = votfdev->votf_table[svc][ip][id].module;
				module_type = votfdev->votf_table[svc][ip][id].module_type;
				if (module == C2SERV) {
					switch (module_type) {
					case M0S4:
						is_hw_dump_regs(votfdev->votf_addr[ip], c2serv_m0s4_regs,
							C2SERV_M0S4_REG_CNT);
						break;
					case M2S2:
						is_hw_dump_regs(votfdev->votf_addr[ip], c2serv_m2s2_regs,
							C2SERV_M2S2_REG_CNT);
						break;
					case M3S1:
						is_hw_dump_regs(votfdev->votf_addr[ip], c2serv_m3s1_regs,
							C2SERV_M3S1_REG_CNT);
						break;
					case M16S16:
						is_hw_dump_regs(votfdev->votf_addr[ip], c2serv_m16s16_regs,
							C2SERV_M16S16_REG_CNT);
						break;
					default:
						pr_info("%s: not supported module type\n", __func__);
						break;
					}
				} else
					is_hw_dump_regs(votfdev->votf_addr[ip], c2agent_m6s4_regs,
						C2AGENT_M4S6_REG_CNT);
				break;
			}
		}
	}
}

int votfitf_create_ring(void)
{
	bool do_create = false;
	bool check_ring = false;
	int svc, ip, id;
	int module;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return 0;
	}

	mutex_lock(&votfdev->votf_lock);
	votfdev->ring_request++;
	pr_info("%s: votf_request(%d)\n", __func__, votfdev->ring_request);

	/* To check the reference count mismatch caused by sudden close */
	for (svc = 0; svc < SERVICE_CNT && !check_ring; svc++) {
		for (ip = 0; ip < IP_MAX && !check_ring; ip++) {
			for (id = 0; id < ID_MAX && !check_ring; id++) {
				if (!votfdev->votf_table[svc][ip][id].use)
					continue;
				module = votfdev->votf_table[svc][ip][id].module;
				if (camerapp_check_votf_ring(votfdev->votf_addr[ip], module))
					check_ring = true;
			}
		}
	}

	if (votfdev->ring_create) {
		if (!check_ring) {
			pr_err("%s: votf reference count is mismatched, do reset\n", __func__);
			votfdev->ring_create = false;
			votfdev->ring_request = 1;
			memset(votfdev->ring_pair, VS_DISCONNECTED, sizeof(votfdev->ring_pair));
		} else {
			pr_err("%s: votf ring has already been created(%d)\n", __func__, votfdev->ring_request);
			mutex_unlock(&votfdev->votf_lock);
			return 0;
		}
	}
	for (svc = 0; svc < SERVICE_CNT; svc++) {
		for (ip = 0; ip < IP_MAX; ip++) {
			for (id = 0; id < ID_MAX; id++) {
				if (!votfdev->votf_table[svc][ip][id].use)
					continue;
				module = votfdev->votf_table[svc][ip][id].module;
				camerapp_hw_votf_create_ring(votfdev->votf_addr[ip], ip, module);
				do_create = true;

				/* support only setA register and immediately set mode */
				camerapp_hw_votf_set_sel_reg(votfdev->votf_addr[ip], 0x1, 0x1);
				break;
			}
		}
	}
	if (do_create)
		votfdev->ring_create = true;
	else
		pr_err("%s: invalid request to destroy votf ring\n", __func__);

	mutex_unlock(&votfdev->votf_lock);
	return 1;
}

int votfitf_destroy_ring(void)
{
	bool do_destroy = false;
	int svc, ip, id;
	int module;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return 0;
	}

	mutex_lock(&votfdev->votf_lock);
	votfdev->ring_request--;
	pr_info("%s: votf_request(%d)\n", __func__, votfdev->ring_request);

	if (!votfdev->ring_create) {
		pr_err("%s: votf ring has already been destroyed(%d)\n", __func__, votfdev->ring_request);
		mutex_unlock(&votfdev->votf_lock);
		return 0;
	}
	if (votfdev->ring_request) {
		pr_err("%s: other IPs are still using the votf ring(%d)\n", __func__, votfdev->ring_request);
		mutex_unlock(&votfdev->votf_lock);
		return 0;
	}

	for (svc = 0; svc < SERVICE_CNT; svc++) {
		for (ip = 0; ip < IP_MAX; ip++) {
			for (id = 0; id < ID_MAX; id++) {
				if (!votfdev->votf_table[svc][ip][id].use)
					continue;
				module = votfdev->votf_table[svc][ip][id].module;
				camerapp_hw_votf_destroy_ring(votfdev->votf_addr[ip], ip, module);
				do_destroy = true;
				break;
			}
		}
	}
	if (do_destroy) {
		votfdev->ring_create = false;
		votfdev->ring_request = 0;
		memset(votfdev->ring_pair, VS_DISCONNECTED, sizeof(votfdev->ring_pair));
		memset(votfdev->votf_cfg, 0, sizeof(struct votf_service_cfg));
	} else
		pr_err("%s: invalid request to destroy votf ring\n", __func__);

	mutex_unlock(&votfdev->votf_lock);
	return 1;
}

int votfitf_set_service_cfg(struct votf_info *vinfo, struct votf_service_cfg *cfg)
{
	u32 dest;
	u32 offset;
	u32 token_size;
	u32 frame_size;
	int id, ip;
	int service;
	int module;
	u32 change;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return 0;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return 0;
	}

	ip = vinfo->ip;
	id = vinfo->id;
	service = vinfo->service;

	if (!cfg || !votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return 0;
	}

	module = votfdev->votf_table[service][ip][id].module;
	change = cfg->option & VOTF_OPTION_MSK_CHANGE;

	mutex_lock(&votfdev->votf_lock);
	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: uninitialized table(%d, %d, %d)\n",	__func__, service, ip, id);
		return 0;
	}

	if (!change && votfdev->ring_pair[service][ip][id] == VS_CONNECTED) {
		pr_err("%s: invalid request for already connected service(%d, %d, %d)\n", __func__, service, ip, id);
		mutex_unlock(&votfdev->votf_lock);
		return 0;
	}

	if (service == TWS) {
		if (!change && votfdev->ring_pair[TRS][cfg->connected_ip][cfg->connected_id] == VS_CONNECTED) {
			pr_err("%s: invalid request for already connected service(%d, %d, %d)->(%d, %d, %d)\n",
					__func__, service, ip, id, TRS, cfg->connected_ip, cfg->connected_id);
			mutex_unlock(&votfdev->votf_lock);
			return 0;
		}

		dest = (cfg->connected_ip << 4) | (cfg->connected_id);
		offset = get_offset(vinfo, C2SERV_R_TWS_DEST, -1, C2AGENT_R_TWS_DEST, -1);

		if (offset != VOTF_INVALID) {
			camerapp_hw_votf_set_dest(votfdev->votf_addr[ip], offset, dest);
			votfdev->votf_cfg[service][ip][id].connected_ip = cfg->connected_ip;
			votfdev->votf_cfg[service][ip][id].connected_id = cfg->connected_id;

			votfdev->ring_pair[service][ip][id] = VS_READY;
		}

		if ((votfdev->ring_pair[service][ip][id] == VS_READY) &&
			(votfdev->ring_pair[TRS][cfg->connected_ip][cfg->connected_id] == VS_READY)) {
			votfdev->ring_pair[service][ip][id] = VS_CONNECTED;
			votfdev->ring_pair[TRS][cfg->connected_ip][cfg->connected_id] = VS_CONNECTED;

			votfdev->votf_cfg[TRS][cfg->connected_ip][cfg->connected_id].connected_ip = ip;
			votfdev->votf_cfg[TRS][cfg->connected_ip][cfg->connected_id].connected_id = id;
		}
	} else {
		if (!change && votfdev->ring_pair[TWS][cfg->connected_ip][cfg->connected_id] == VS_CONNECTED) {
			pr_err("%s: invalid request for already connected service(%d, %d, %d)->(%d, %d, %d)\n",
					__func__, service, ip, id, TWS, cfg->connected_ip, cfg->connected_id);
			mutex_unlock(&votfdev->votf_lock);
			return 0;
		}

		votfdev->ring_pair[service][ip][id] = VS_READY;

		if ((votfdev->ring_pair[service][ip][id] == VS_READY) &&
			(votfdev->ring_pair[TWS][cfg->connected_ip][cfg->connected_id] == VS_READY)) {
			/* TRS cannot guarantee valid connected ip and id, so it is stable to check valid ip and id */
			if ((votfdev->votf_cfg[TWS][cfg->connected_ip][cfg->connected_id].connected_ip == ip) &&
				(votfdev->votf_cfg[TWS][cfg->connected_ip][cfg->connected_id].connected_id == id)) {
				votfdev->ring_pair[service][ip][id] = VS_CONNECTED;
				votfdev->ring_pair[TWS][cfg->connected_ip][cfg->connected_id] = VS_CONNECTED;
			}
		}
	}

	offset = get_offset(vinfo, C2SERV_R_TWS_ENABLE, C2SERV_R_TRS_ENABLE, C2AGENT_R_TWS_ENABLE,
			C2AGENT_R_TRS_ENABLE);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_enable(votfdev->votf_addr[ip], offset, cfg->enable);
		votfdev->votf_cfg[service][ip][id].enable = cfg->enable;
	}

	offset = get_offset(vinfo, C2SERV_R_TWS_LIMIT, C2SERV_R_TRS_LIMIT, C2AGENT_R_TWS_LIMIT, C2AGENT_R_TRS_LIMIT);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_limit(votfdev->votf_addr[ip], offset, cfg->limit);
		votfdev->votf_cfg[service][ip][id].limit = cfg->limit;
	}

	token_size = cfg->token_size;
	frame_size = cfg->height;
	if (module == C2AGENT) {
		token_size = cfg->bitwidth * cfg->width * cfg->token_size / 8;
		frame_size = cfg->bitwidth * cfg->width * cfg->height / 8;
	}
	/* set frame size internally */
	if (service == TRS)
		votfitf_set_frame_size(vinfo, frame_size);

	offset = get_offset(vinfo, C2SERV_R_TWS_LINES_IN_TOKEN, C2SERV_R_TRS_LINES_IN_TOKEN, C2AGENT_R_TWS_TOKEN_SIZE,
			C2AGENT_R_TRS_TOKEN_SIZE);
	if (offset != VOTF_INVALID) {
		votfitf_set_token_size(vinfo, token_size);
		if (service == TRS)
			votfitf_set_first_token_size(vinfo, token_size);
		/* save token_size in line unit(not calculated token size) */
		votfdev->votf_cfg[service][ip][id].token_size = cfg->token_size;
	}

	mutex_unlock(&votfdev->votf_lock);
	return 1;
}

int votfitf_set_first_token_size(struct votf_info *vinfo, u32 size)
{
	u32 offset;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return 0;
	}

	service = vinfo->service;
	ip = vinfo->ip;
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	if (service != TRS)
		return ret;

	offset = get_offset(vinfo, -1, C2SERV_R_TRS_LINES_IN_FIRST_TOKEN, -1, C2AGENT_TRS_CROP_FIRST_TOKEN_SIZE);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_first_token_size(votfdev->votf_addr[ip], offset, size);
		ret = 1;
	}

	return ret;
}

/* set token size */
int votfitf_set_token_size(struct votf_info *vinfo, u32 size)
{
	u32 offset;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = vinfo->ip;
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, C2SERV_R_TWS_LINES_IN_TOKEN, C2SERV_R_TRS_LINES_IN_TOKEN, C2AGENT_R_TWS_TOKEN_SIZE,
			C2AGENT_R_TRS_TOKEN_SIZE);

	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_token_size(votfdev->votf_addr[ip], offset, size);
		ret = 1;
	}
	return ret;
}

/* set frame size */
int votfitf_set_frame_size(struct votf_info *vinfo, u32 size)
{
	u32 offset;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return 0;
	}

	service = vinfo->service;
	ip = vinfo->ip;
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	if (service != TRS)
		return ret;

	offset = get_offset(vinfo, -1, C2SERV_R_TRS_LINES_COUNT, -1, C2AGENT_TRS_FRAME_SIZE);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_frame_size(votfdev->votf_addr[ip], offset, size);
		ret = 1;
	}

	return ret;
}

int votfitf_set_flush(struct votf_info *vinfo)
{
	u32 offset;
	u32 try_cnt = 0;
	int ret = 0;
	int service, ip, id;
	int connected_ip, connected_id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = vinfo->ip;
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}
	offset = get_offset(vinfo, C2SERV_R_TWS_FLUSH, C2SERV_R_TRS_FLUSH, C2AGENT_R_TWS_FLUSH, -1);

	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_flush(votfdev->votf_addr[ip], offset);
		ret = 1;
		while (votfitf_get_busy(vinfo)) {
			if (++try_cnt >= 10000) {
				pr_err("[VOTF] timeout waiting clear busy - flush fail");
				ret = -ETIME;
				break;
			}
			udelay(10);
		}
	}

	connected_ip = votfdev->votf_cfg[service][ip][id].connected_ip;
	connected_id = votfdev->votf_cfg[service][ip][id].connected_id;

	if (service == TWS) {
		votfdev->ring_pair[TWS][ip][id] = VS_DISCONNECTED;
		votfdev->ring_pair[TRS][connected_ip][connected_id] = VS_DISCONNECTED;
	} else {
		votfdev->ring_pair[TRS][ip][id] = VS_DISCONNECTED;
		votfdev->ring_pair[TWS][connected_ip][connected_id] = VS_DISCONNECTED;
	}

	return ret;
}

int votfitf_set_crop_start(struct votf_info *vinfo, bool start)
{
	u32 offset;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = vinfo->ip;
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, -1, C2AGENT_TRS_CROP_TOKENS_START);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_crop_start(votfdev->votf_addr[ip], offset, start);
		ret = 1;
	}
	return ret;
}

int votfitf_get_crop_start(struct votf_info *vinfo)
{
	u32 offset;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = vinfo->ip;
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, -1, C2AGENT_TRS_CROP_TOKENS_START);
	if (offset != VOTF_INVALID) {
		ret = camerapp_hw_votf_get_crop_start(votfdev->votf_addr[ip], offset);
	}
	return ret;
}

int voifitf_set_crop_enable(struct votf_info *vinfo, bool enable)
{
	u32 offset;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = vinfo->ip;
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, -1, C2AGENT_TRS_CROP_ENABLE);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_crop_enable(votfdev->votf_addr[ip], offset, enable);
		ret = 1;
	}
	return ret;
}

int voifitf_get_crop_enable(struct votf_info *vinfo)
{
	u32 offset;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = vinfo->ip;
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, -1, C2AGENT_TRS_CROP_ENABLE);
	if (offset != VOTF_INVALID) {
		ret = camerapp_hw_votf_get_crop_enable(votfdev->votf_addr[ip], offset);
	}
	return ret;
}

int votfitf_set_trs_lost_cfg(struct votf_info *vinfo, struct votf_lost_cfg *cfg)
{
	u32 offset;
	u32 val = 0;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = vinfo->ip;
	id = vinfo->id;

	if (!cfg || !votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, C2SERV_R_TRS_CONNECTION_LOST_RECOVER_ENABLE, -1, -1);

	if (cfg->recover_enable)
		val |= (0x1 << 0);
	if (cfg->flush_enable)
		val |= (0x1 << 1);

	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_recover_enable(votfdev->votf_addr[ip], offset, val);
		ret = 1;
	}

	return ret;
}

int votfitf_reset(struct votf_info *vinfo, int type)
{
	int ret = 0;
	int ip, id;
	int service;
	int module;
	int connected_ip, connected_id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	ip = vinfo->ip;
	id = vinfo->id;
	service = vinfo->service;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	module = votfdev->votf_table[service][ip][id].module;
	connected_ip = votfdev->votf_cfg[service][ip][id].connected_ip;
	connected_id = votfdev->votf_cfg[service][ip][id].connected_id;

	if (type == SW_CORE_RESET) {
		/*
		 * Writing to this signal, automatically generate SW core reset pulse
		 * that flushes the DMA and resets all registers but the APB registers
		 */
		camerapp_hw_votf_sw_core_reset(votfdev->votf_addr[ip], module);
		camerapp_hw_votf_sw_core_reset(votfdev->votf_addr[connected_ip], module);
	} else {
		/* Writing to this signal, automatically generates SW reset pulse */
		camerapp_hw_votf_reset(votfdev->votf_addr[ip], module);
		camerapp_hw_votf_reset(votfdev->votf_addr[connected_ip], module);
	}

	if (service == TWS) {
		votfdev->ring_pair[TWS][ip][id] = VS_DISCONNECTED;
		votfdev->ring_pair[TRS][connected_ip][connected_id] = VS_DISCONNECTED;
	} else {
		votfdev->ring_pair[TRS][ip][id] = VS_DISCONNECTED;
		votfdev->ring_pair[TWS][connected_ip][connected_id] = VS_DISCONNECTED;
	}
	ret = 1;

	return ret;
}

int votfitf_set_start(struct votf_info *vinfo)
{
	u32 offset;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = vinfo->ip;
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, C2AGENT_R_TWS_START, C2AGENT_R_TRS_START);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_start(votfdev->votf_addr[ip], offset, 0x1);
		ret = 1;
	}

	return ret;
}

int votfitf_set_finish(struct votf_info *vinfo)
{
	u32 offset;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = vinfo->ip;
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, C2AGENT_R_TWS_FINISH, C2AGENT_R_TRS_FINISH);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_finish(votfdev->votf_addr[ip], offset, 0x1);
		ret = 1;
	}

	return ret;
}

int votfitf_set_threshold(struct votf_info *vinfo, bool high, u32 value)
{
	u32 offset;
	int ret = 0;
	int c2a_tws, c2a_trs;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = vinfo->ip;
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	if (high) {
		c2a_tws = C2AGENT_R_TWS_HIGH_THRESHOLD;
		c2a_trs = C2AGENT_R_TRS_HIGH_THRESHOLD;
	} else {
		c2a_tws = C2AGENT_R_TWS_LOW_THRESHOLD;
		c2a_trs = C2AGENT_R_TRS_LOW_THRESHOLD;
	}

	offset = get_offset(vinfo, -1, -1, c2a_tws, c2a_trs);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_threshold(votfdev->votf_addr[ip], offset, value);
		ret = 1;
	}

	return ret;
}

u32 votfitf_get_threshold(struct votf_info *vinfo, bool high)
{
	u32 offset;
	u32 ret = 0;
	int c2a_tws, c2a_trs;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = vinfo->ip;
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	if (high) {
		c2a_tws = C2AGENT_R_TWS_HIGH_THRESHOLD;
		c2a_trs = C2AGENT_R_TRS_HIGH_THRESHOLD;
	} else {
		c2a_tws = C2AGENT_R_TWS_LOW_THRESHOLD;
		c2a_trs = C2AGENT_R_TRS_LOW_THRESHOLD;
	}

	offset = get_offset(vinfo, -1, -1, c2a_tws, c2a_trs);
	if (offset != VOTF_INVALID)
		ret = camerapp_hw_votf_get_threshold(votfdev->votf_addr[ip], offset);

	return ret;
}

int votfitf_set_read_bytes(struct votf_info *vinfo, u32 bytes)
{
	u32 offset;
	int ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = vinfo->ip;
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, -1, C2AGENT_R_TRS_READ_BYTES);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_read_bytes(votfdev->votf_addr[ip], offset, bytes);
		ret = 1;
	}

	return ret;
}

int votfitf_get_fullness(struct votf_info *vinfo)
{
	u32 offset;
	u32 ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = vinfo->ip;
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, C2AGENT_R_TWS_FULLNESS, C2AGENT_R_TRS_FULLNESS);
	if (offset != VOTF_INVALID)
		ret = camerapp_hw_votf_get_fullness(votfdev->votf_addr[ip], offset);

	return ret;
}

u32 votfitf_get_busy(struct votf_info *vinfo)
{
	u32 offset;
	u32 ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = vinfo->ip;
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, C2SERV_R_TWS_BUSY, C2SERV_R_TRS_BUSY, C2AGENT_R_TWS_BUSY, C2AGENT_R_TRS_BUSY);
	if (offset != VOTF_INVALID)
		ret = camerapp_hw_votf_get_busy(votfdev->votf_addr[ip], offset);

	return ret;
}

int votfitf_set_irq_enable(struct votf_info *vinfo, u32 irq)
{
	u32 offset;
	u32 ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = vinfo->ip;
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, C2AGENT_R_TWS_IRQ_ENABLE, C2AGENT_R_TRS_IRQ_ENBALE);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_irq_enable(votfdev->votf_addr[ip], offset, irq);
		ret = 1;
	}

	return ret;
}

int votfitf_set_irq_status(struct votf_info *vinfo, u32 irq)
{
	u32 offset;
	u32 ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = vinfo->ip;
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, C2AGENT_R_TWS_IRQ_ENABLE, C2AGENT_R_TRS_IRQ_ENBALE);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_irq_status(votfdev->votf_addr[ip], offset, irq);
		ret = 1;
	}

	return ret;

}

int votfitf_set_irq(struct votf_info *vinfo, u32 irq)
{
	u32 offset;
	u32 ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = vinfo->ip;
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, C2AGENT_R_TWS_IRQ_ENABLE, C2AGENT_R_TRS_IRQ_ENBALE);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_irq(votfdev->votf_addr[ip], offset, irq);
		ret = 1;
	}

	return ret;

}

int votfitf_set_irq_clear(struct votf_info *vinfo, u32 irq)
{
	u32 offset;
	u32 ret = 0;
	int service, ip, id;

	if (!votfdev) {
		pr_err("%s: votf devices is null\n", __func__);
		return ret;
	}
	if (!check_vinfo(vinfo)) {
		pr_err("%s: invalid votf_info\n", __func__);
		return ret;
	}

	service = vinfo->service;
	ip = vinfo->ip;
	id = vinfo->id;

	if (!votfdev->votf_table[service][ip][id].use) {
		pr_err("%s: invalid input\n", __func__);
		return ret;
	}

	offset = get_offset(vinfo, -1, -1, C2AGENT_R_TWS_IRQ_CLEAR, C2AGENT_R_TRS_IRQ_CLEAR);
	if (offset != VOTF_INVALID) {
		camerapp_hw_votf_set_irq_clear(votfdev->votf_addr[ip], offset, irq);
		ret = 1;
	}

	return ret;
}

int votf_runtime_resume(struct device *dev)
{
	pr_err("%s: votf_runtime_resume was called\n", __func__);
	return 0;
}

int votf_runtime_suspend(struct device *dev)
{
	pr_err("%s: votf_runtime_suspend was called\n", __func__);
	return 0;
}

int parse_votf_table(struct device_node *np)
{
	int ret = 0;
	int ip_num, id_num;
	int id_cnt;
	int idx = 0;
	int service;
	struct device_node *table_np;
	struct votf_table_info temp;

	for_each_child_of_node(np, table_np) {
		of_property_read_u32_index(table_np, "info", idx++, &temp.addr);
		of_property_read_u32_index(table_np, "info", idx++, &ip_num);
		of_property_read_u32_index(table_np, "info", idx++, &id_cnt);
		of_property_read_u32_index(table_np, "info", idx++, &temp.module);
		of_property_read_u32_index(table_np, "info", idx++, &temp.service);
		of_property_read_u32_index(table_np, "info", idx++, &temp.module_type);

		for (id_num = 0; id_num < id_cnt; id_num++) {
			service = temp.service;
			votfdev->votf_table[service][ip_num][id_num].use = true;
			votfdev->votf_table[service][ip_num][id_num].addr = temp.addr;
			votfdev->votf_table[service][ip_num][id_num].ip = ip_num;
			votfdev->votf_table[service][ip_num][id_num].id = id_num;
			votfdev->votf_table[service][ip_num][id_num].service = temp.service;
			votfdev->votf_table[service][ip_num][id_num].module = temp.module;
			votfdev->votf_table[service][ip_num][id_num].module_type = temp.module_type;
		}
		idx = 0;
	}
	return ret;
}

int parse_votf_service(struct device_node *np)
{
	int idx = 0;
	int module_type;
	struct device_node *table_np;

	struct votf_module_type_addr temp;

	for_each_child_of_node(np, table_np) {
		of_property_read_u32_index(table_np, "info", idx++, &module_type);
		of_property_read_u32_index(table_np, "info", idx++, &temp.tws_addr);
		of_property_read_u32_index(table_np, "info", idx++, &temp.trs_addr);
		of_property_read_u32_index(table_np, "info", idx++, &temp.tws_gap);
		of_property_read_u32_index(table_np, "info", idx++, &temp.trs_gap);

		votfdev->votf_module_addr[module_type].tws_addr = temp.tws_addr;
		votfdev->votf_module_addr[module_type].trs_addr = temp.trs_addr;
		votfdev->votf_module_addr[module_type].tws_gap = temp.tws_gap;
		votfdev->votf_module_addr[module_type].trs_gap = temp.trs_gap;

		pr_info("%s tws_addr(%x), .tws_addr(%x), tws_gap(%x), trs_gat(%x)\n",
			__func__, temp.tws_addr, temp.trs_addr, temp.tws_gap, temp.trs_gap);

		idx = 0;
	}
	return 0;
}

int votf_probe(struct platform_device *pdev)
{
	u32 addr = 0;
	int ip;
	int ret = 1;
	struct device *dev;
	struct device_node *votf_np = NULL;
	struct device_node *service_np = NULL;
	struct device_node *np;

	votfdev = devm_kzalloc(&pdev->dev, sizeof(struct votf_dev), GFP_KERNEL);
	if (!votfdev)
		return -ENOMEM;

	votf_init();
	dev = &pdev->dev;
	votfdev->dev = &pdev->dev;
	np = dev->of_node;

	votf_np = of_get_child_by_name(np, "table0");
	if (votf_np) {
		ret = parse_votf_table(votf_np);
		if (ret) {
			pr_err("%s: parse_votf_table is fail(%d)", __func__, ret);
			ret = 0;
		}
	}

	service_np = of_get_child_by_name(np, "service");
	if (service_np) {
		ret = parse_votf_service(service_np);
		if (ret) {
			pr_err("%s: parse_votf_service is fail(%d)", __func__, ret);
			ret = 0;
		}
	}

	for (ip = 0; ip < IP_MAX; ip++) {
		if (votfdev->votf_table[TWS][ip][0].use || votfdev->votf_table[TRS][ip][0].use) {
			if (votfdev->votf_table[TWS][ip][0].addr)
				addr = votfdev->votf_table[TWS][ip][0].addr;
			else if (votfdev->votf_table[TRS][ip][0].addr)
				addr = votfdev->votf_table[TRS][ip][0].addr;
			else
				pr_err("%s: Invalid votf address\n", __func__);
			votfdev->votf_addr[ip] = ioremap(addr, 0x1000);
		}
	}

	platform_set_drvdata(pdev, votfdev);

	dev_info(&pdev->dev, "Driver probed successfully\n");
	pr_info("%s successfully done.\n", __func__);

	return ret;
}

int votf_remove(struct platform_device *pdev)
{
	u32 addr;
	int ip;

	if (!votfdev) {
		dev_err(&pdev->dev, "no memory for VOTF device\n");
		return -ENOMEM;
	}

	for (ip = 0; ip < IP_MAX; ip++) {
		if (votfdev->votf_table[TWS][ip][0].addr)
			addr = votfdev->votf_table[TWS][ip][0].addr;
		else if (votfdev->votf_table[TRS][ip][0].addr)
			addr = votfdev->votf_table[TRS][ip][0].addr;
		else
			pr_err("%s: Invalid votf address\n", __func__);

		iounmap(votfdev->votf_addr[ip]);
	}

	devm_kfree(&pdev->dev, votfdev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

void votf_shutdown(struct platform_device *pdev)
{
}

const struct of_device_id exynos_votf_match[] = {
	{
		.compatible = "samsung,exynos-camerapp-votf",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_votf_match);

struct platform_driver votf_driver = {
	.probe		= votf_probe,
	.remove		= votf_remove,
	.shutdown	= votf_shutdown,
	.driver = {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_votf_match),
	}
};

module_platform_driver(votf_driver);

MODULE_AUTHOR("SamsungLSI Camera");
MODULE_DESCRIPTION("EXYNOS CameraPP VOTF driver");
MODULE_LICENSE("GPL");
