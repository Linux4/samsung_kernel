// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/spinlock.h>
#include <linux/slab.h>

#include <lpm_sys_res.h>
#include <lpm_sys_res_plat.h>
#include <lpm_sys_res_mbrain_dbg.h>
#include <lpm_sys_res_mbrain_plat.h>

enum {
	ALL_SCENE = 0,
	LAST_SUSPEND_RES = 1,
	LAST_SUSPEND_STATS = 2,
};

static int group_release[NR_SPM_GRP] = {
	RELEASE_GROUP,
	RELEASE_GROUP,
	NOT_RELEASE,
	NOT_RELEASE,
	NOT_RELEASE,
	RELEASE_GROUP,
	NOT_RELEASE,
	RELEASE_GROUP,
	NOT_RELEASE,
	NOT_RELEASE,
};

struct sys_res_mbrain_header header;
static unsigned int sys_res_sig_num, sys_res_grp_num;
static uint32_t spm_res_sig_tbl_num;
static struct sys_res_sig_info *spm_res_sig_tbl_ptr;

static void get_sys_res_header(int type)
{
	header.data_offset = sizeof(struct sys_res_mbrain_header);
	header.version = SYS_RES_DATA_VERSION;
	switch(type) {
	case ALL_SCENE:
		header.module = SYS_RES_ALL_DATA_MODULE_ID;
		header.index_data_length = SCENE_RELEASE_NUM *
					   (sizeof(struct sys_res_scene_info) +
					   sys_res_sig_num * sizeof(struct sys_res_sig_info));
		break;
	case LAST_SUSPEND_RES:
		header.module = SYS_RES_LVL_DATA_MODULE_ID;
		header.index_data_length = sizeof(struct sys_res_scene_info) +
					   (sizeof(uint32_t) + sizeof(struct sys_res_sig_info)) *
					   sys_res_grp_num;
		break;
	case LAST_SUSPEND_STATS:
		header.module = SYS_RES_LVL_DATA_MODULE_ID;
		header.index_data_length = sizeof(struct sys_res_scene_info);
		break;
	}
}

static void *sys_res_data_copy(void *dest, void *src, uint64_t size)
{
	memcpy(dest, src, size);
	dest += size;
	return dest;
}

static unsigned int lpm_mbrain_get_sys_res_length(void)
{
	get_sys_res_header(ALL_SCENE);
	return header.index_data_length;
}

static int lpm_mbrain_get_sys_res_data(void *address, uint32_t size)
{
	int i = 0;
	int j = 0;
	int ret = 0, sys_res_update = 0;
	unsigned long flag;
	struct lpm_sys_res_ops *sys_res_ops;
	struct sys_res_record *sys_res_record[SCENE_RELEASE_NUM];
	struct sys_res_scene_info scene_info;
	void *sig_info;

	get_sys_res_header(ALL_SCENE);

	if (!address ||
	    header.index_data_length == 0 ||
	    size < header.index_data_length + header.data_offset) {
		pr_info("[name:spm&][SPM][Mbrain] mbrain address/buffer size error\n");
		ret = -1;
	}

	sys_res_ops = get_lpm_sys_res_ops();
	if (!sys_res_ops ||
	    !sys_res_ops->update ||
	    !sys_res_ops->get ||
	    !sys_res_ops->get_detail) {
		pr_info("[name:spm&][SPM][Mbrain] Get sys res operations fail\n");
		ret = -1;
	}

	if (ret)
		return ret;

	/* Copy header */
	address = sys_res_data_copy(address, &header, sizeof(struct sys_res_mbrain_header));

	sys_res_update = sys_res_ops->update();
	if (sys_res_update)
		pr_info("[name:spm&][SPM][Mbrain] SWPM data invalid, Error %d\n", sys_res_update);

	/* Copy scenario data */
	sys_res_record[SYS_RES_RELEASE_SCENE_COMMON] = sys_res_ops->get(SYS_RES_COMMON);
	sys_res_record[SYS_RES_RELEASE_SCENE_SUSPEND] = sys_res_ops->get(SYS_RES_SUSPEND);
	sys_res_record[SYS_RES_RELEASE_SCENE_LAST_SUSPEND] = sys_res_ops->get(SYS_RES_LAST_SUSPEND);

	spin_lock_irqsave(sys_res_ops->lock, flag);
	scene_info.res_sig_num = sys_res_sig_num;
	for (i = 0; i < SCENE_RELEASE_NUM; i++) {
		scene_info.duration_time = sys_res_ops->get_detail(sys_res_record[i],
								   SYS_RES_DURATION, 0);
		scene_info.suspend_time = sys_res_ops->get_detail(sys_res_record[i],
								   SYS_RES_SUSPEND_TIME, 0);
		address = sys_res_data_copy(address,
					    &scene_info,
					    sizeof(struct sys_res_scene_info));
	}

	/* Copy signal data */
	for (i = 0; i < SCENE_RELEASE_NUM; i++) {
		for (j = 0; j < SYS_RES_SYS_RESOURCE_NUM; j++){
			if (!group_release[j])
				continue;

			sig_info = (void *)sys_res_ops->get_detail(sys_res_record[i],
						SYS_RES_SIG_ADDR,
						sys_res_group_info[j].sys_index);
			if (sig_info)
				address = sys_res_data_copy(address,
							sig_info,
							sizeof(struct sys_res_sig_info));

			sig_info = (void *)sys_res_ops->get_detail(sys_res_record[i],
						SYS_RES_SIG_ADDR,
						sys_res_group_info[j].sig_table_index);
			if (sig_info)
				address = sys_res_data_copy(address,
							sig_info,
							sizeof(struct sys_res_sig_info) *
							sys_res_group_info[j].group_num);
		}
	}
	spin_unlock_irqrestore(sys_res_ops->lock, flag);

	return ret;
}

static int lpm_mbrain_get_last_suspend_res_data(void *address, uint32_t size)
{
	int i, ret = 0, sys_res_update = 0;
	unsigned long flag;
	struct sys_res_scene_info scene_info;
	struct sys_res_record *sys_res_last_suspend_record;
	struct lpm_sys_res_ops *sys_res_ops;
	void *sig_info;

	get_sys_res_header(LAST_SUSPEND_RES);

	if (!address ||
	    header.index_data_length == 0 ||
	    size < header.index_data_length + header.data_offset) {
		pr_info("[name:spm&][SPM][Mbrain] mbrain address/buffer size error\n");
		ret = -1;
	}

	sys_res_ops = get_lpm_sys_res_ops();
	if (!sys_res_ops ||
		!sys_res_ops->update ||
	    !sys_res_ops->get ||
	    !sys_res_ops->get_detail) {
		pr_info("[name:spm&][SPM][Mbrain] Get sys res operations fail\n");
		ret = -1;
	}

	if (ret)
		return ret;

	address = sys_res_data_copy(address, &header, sizeof(struct sys_res_mbrain_header));

	sys_res_update = sys_res_ops->update();
	if (sys_res_update)
		pr_info("[name:spm&][SPM][Mbrain] SWPM data invalid, Error %d\n", sys_res_update);

	sys_res_last_suspend_record = sys_res_ops->get(SYS_RES_LAST_SUSPEND);

	spin_lock_irqsave(sys_res_ops->lock, flag);
	scene_info.res_sig_num = sys_res_sig_num;
	scene_info.duration_time = sys_res_ops->get_detail(sys_res_last_suspend_record,
							   SYS_RES_DURATION,
							   0);
	scene_info.suspend_time = sys_res_ops->get_detail(sys_res_last_suspend_record,
							  SYS_RES_SUSPEND_TIME,
							  0);
	address = sys_res_data_copy(address, &scene_info, sizeof(struct sys_res_scene_info));

	for (i = 0; i < SYS_RES_SYS_RESOURCE_NUM; i++) {
		if (!group_release[i])
			continue;

		address = sys_res_data_copy(address,
					    &(sys_res_group_info[i].threshold),
					    sizeof(uint32_t));
	}

	for (i = 0; i < SYS_RES_SYS_RESOURCE_NUM; i++) {
		if (!group_release[i])
			continue;

		sig_info = (void *)sys_res_ops->get_detail(sys_res_last_suspend_record,
							   SYS_RES_SIG_ADDR,
							   sys_res_group_info[i].sys_index);
		if (sig_info)
			address = sys_res_data_copy(address,
						    sig_info,
						    sizeof(struct sys_res_sig_info));
	}
	spin_unlock_irqrestore(sys_res_ops->lock, flag);
	return 0;
}

static int lpm_mbrain_get_over_threshold_num(void *address, uint32_t size,
		uint32_t *threshold, uint32_t threshold_num)
{
	int i, j, k = 0, ret = 0;
	unsigned long flag;
	struct sys_res_scene_info scene_info;
	struct lpm_sys_res_ops *sys_res_ops;
	struct sys_res_record *sys_res_last_suspend_record;
	struct sys_res_sig_info *ptr = spm_res_sig_tbl_ptr;
	void *sig_info;
	uint64_t ratio;
	uint32_t sig_index;

	get_sys_res_header(LAST_SUSPEND_STATS);

	if (!address ||
	    header.index_data_length == 0 ||
	    size < header.index_data_length + header.data_offset) {
		pr_info("[name:spm&][SPM][Mbrain] mbrain address/buffer size error\n");
		ret = -1;
	}

	sys_res_ops = get_lpm_sys_res_ops();
	if (!sys_res_ops ||
	    !sys_res_ops->get ||
	    !sys_res_ops->get_detail) {
		pr_info("[name:spm&][SPM][Mbrain] Get sys res operations fail\n");
		ret = -1;
	}

	if (ret)
		return ret;

	address = sys_res_data_copy(address, &header, sizeof(struct sys_res_mbrain_header));

	sys_res_last_suspend_record = sys_res_ops->get(SYS_RES_LAST_SUSPEND);

	spin_lock_irqsave(sys_res_ops->lock, flag);
	scene_info.duration_time = sys_res_ops->get_detail(sys_res_last_suspend_record,
							   SYS_RES_DURATION, 0);
	scene_info.suspend_time = sys_res_ops->get_detail(sys_res_last_suspend_record,
							  SYS_RES_SUSPEND_TIME, 0);
	spm_res_sig_tbl_num = 0;

	for (i = 0; i < SYS_RES_SYS_RESOURCE_NUM; i++){
		if (!group_release[i])
			continue;

		if(k >= threshold_num) {
			ret = -1;
			pr_info("[name:spm&][SPM][Mbrain] number of threshold is error\n");
			break;
		}

		for (j = 0; j < sys_res_group_info[i].group_num + 1; j++) {
			if (j == 0)
				sig_index = sys_res_group_info[i].sys_index;
			else
				sig_index = j + sys_res_group_info[i].sig_table_index - 1;

			ratio = sys_res_ops->get_detail(sys_res_last_suspend_record,
							SYS_RES_SIG_SUSPEND_RATIO,
							sig_index);

			if (ratio < threshold[k])
				continue;

			spm_res_sig_tbl_num++;
			sig_info = (void *)sys_res_ops->get_detail(sys_res_last_suspend_record,
								   SYS_RES_SIG_ADDR,
								   sig_index);
			if (sig_info)
				ptr = sys_res_data_copy(ptr,
							sig_info,
							sizeof(struct sys_res_sig_info));
		}
		k++;
	}
	spin_unlock_irqrestore(sys_res_ops->lock, flag);

	scene_info.res_sig_num = spm_res_sig_tbl_num;
	address = sys_res_data_copy(address,
				    &scene_info,
				    sizeof(struct sys_res_scene_info));
	return ret;
}

static int lpm_mbrain_get_over_threshold_data(void *address, uint32_t size)
{
	if (!address ||
	    size < spm_res_sig_tbl_num * sizeof(struct sys_res_sig_info)) {
		pr_info("[name:spm&][SPM][Mbrain] mbrain address/buffer size error\n");
		return -1;
	}

	address = sys_res_data_copy(address,
				    spm_res_sig_tbl_ptr,
				    spm_res_sig_tbl_num * sizeof(struct sys_res_sig_info));

	return 0;
}

static struct lpm_sys_res_mbrain_dbg_ops sys_res_mbrain_ops = {
	.get_length = lpm_mbrain_get_sys_res_length,
	.get_data = lpm_mbrain_get_sys_res_data,
	.get_last_suspend_res_data = lpm_mbrain_get_last_suspend_res_data,
	.get_over_threshold_num = lpm_mbrain_get_over_threshold_num,
	.get_over_threshold_data = lpm_mbrain_get_over_threshold_data
};

int lpm_sys_res_mbrain_plat_init (void)
{
	int i = 0;

	for (i = 0; i < NR_SPM_GRP; i++) {
		if (group_release[i]) {
			sys_res_grp_num += 1;
			sys_res_sig_num += sys_res_group_info[i].group_num;
		}
	}
	sys_res_sig_num += sys_res_grp_num;

	spm_res_sig_tbl_ptr = kcalloc(sys_res_sig_num,
				      sizeof(struct sys_res_sig_info),
				      GFP_KERNEL);

	return register_lpm_mbrain_dbg_ops(&sys_res_mbrain_ops);
}

void lpm_sys_res_mbrain_plat_deinit (void)
{
	unregister_lpm_mbrain_dbg_ops();
}
