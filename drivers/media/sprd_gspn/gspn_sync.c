/*
 * drivers/base/gspn_sync.c
 *
 * Copyright (C) 2012 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include "gspn_sync.h"
#include "gspn_drv.h"

static int gspn_sync_cmp(u32 a, u32 b)
{
	int ret = -1;
	if (a == b)
		return 0;

	ret = ((s32)a - (s32)b) < 0 ? -1 : 1;
	if ((a + GSPN_KCMD_MAX < a)
		&& (b < a + GSPN_KCMD_MAX))
		ret = -1;
	return ret;
}

struct sync_pt *gspn_sync_pt_create(struct gspn_sync_timeline *obj, u32 value)
{
	struct gspn_sync_pt *pt;

	pt = (struct gspn_sync_pt *)
		sync_pt_create(&obj->obj, sizeof(struct gspn_sync_pt));

	pt->value = value;

	return (struct sync_pt *)pt;
}

static struct sync_pt *gspn_sync_pt_dup(struct sync_pt *sync_pt)
{
	struct gspn_sync_pt *pt = (struct gspn_sync_pt *) sync_pt;
	struct gspn_sync_timeline *obj =
		(struct gspn_sync_timeline *)sync_pt->parent;

	return (struct sync_pt *) gspn_sync_pt_create(obj, pt->value);
}

static int gspn_sync_pt_has_signaled(struct sync_pt *sync_pt)
{
	struct gspn_sync_pt *pt = (struct gspn_sync_pt *)sync_pt;
	struct gspn_sync_timeline *obj =
		(struct gspn_sync_timeline *)sync_pt->parent;

	return gspn_sync_cmp(obj->value, pt->value) >= 0;
}

static int gspn_sync_pt_compare(struct sync_pt *a, struct sync_pt *b)
{
	struct gspn_sync_pt *pt_a = (struct gspn_sync_pt *)a;
	struct gspn_sync_pt *pt_b = (struct gspn_sync_pt *)b;

	return gspn_sync_cmp(pt_a->value, pt_b->value);
}

static int gspn_sync_fill_driver_data(struct sync_pt *sync_pt,
				    void *data, int size)
{
	struct gspn_sync_pt *pt = (struct gspn_sync_pt *)sync_pt;

	if (size < sizeof(pt->value))
		return -ENOMEM;

	memcpy(data, &pt->value, sizeof(pt->value));

	return sizeof(pt->value);
}

static void gspn_sync_timeline_value_str(struct sync_timeline *sync_timeline,
				       char *str, int size)
{
	struct gspn_sync_timeline *timeline =
		(struct gspn_sync_timeline *)sync_timeline;
	snprintf(str, size, "%d", timeline->value);
}

static void gspn_sync_pt_value_str(struct sync_pt *sync_pt,
				       char *str, int size)
{
	struct gspn_sync_pt *pt = (struct gspn_sync_pt *)sync_pt;
	snprintf(str, size, "%d", pt->value);
}

static struct sync_timeline_ops gspn_sync_timeline_ops = {
	.driver_name = "gspn_sync",
	.dup = gspn_sync_pt_dup,
	.has_signaled = gspn_sync_pt_has_signaled,
	.compare = gspn_sync_pt_compare,
	.fill_driver_data = gspn_sync_fill_driver_data,
	.timeline_value_str = gspn_sync_timeline_value_str,
	.pt_value_str = gspn_sync_pt_value_str,
};

static void gspn_sync_record_inc(struct gspn_sync_timeline *tl)
{
	int v = 0;
	mutex_lock(&tl->lock);
	/*protect the record overflow*/
	if (tl->record < UINT_MAX)
		tl->record++;
	else
		tl->record = 0;
	v = tl->record;
	mutex_unlock(&tl->lock);
}

static void gspn_sync_record_get(unsigned int *value,
		struct gspn_sync_timeline *tl)
{
	mutex_lock(&tl->lock);
	*value = tl->record;
	mutex_unlock(&tl->lock);
}

static void gspn_sync_record_set(int *value,
		int record)
{
	*value = record;
}

int gspn_sync_timeline_destroy(struct gspn_sync_timeline* obj)
{
	sync_timeline_destroy(&obj->obj);
	return 0;
}

struct gspn_sync_timeline *gspn_sync_timeline_create(const char *name)
{
	struct gspn_sync_timeline *obj = (struct gspn_sync_timeline *)
		sync_timeline_create(&gspn_sync_timeline_ops,
				     sizeof(struct gspn_sync_timeline),
				     name);
	mutex_init(&obj->lock);
	obj->value = 0;
	obj->record = 0;
	return obj;
}

void gspn_sync_timeline_inc(struct gspn_sync_timeline *obj, u32 inc)
{
	int v = -1, r = -1;;
	mutex_lock(&obj->lock);
	v = obj->value;
	r = obj->record;
	mutex_unlock(&obj->lock);
	GSPN_LOGI("signal timeline value: %u\n", r);
	if (v > r)
		GSPN_LOGE("timeline value bigger than record, it maybe an error\n");
	sync_timeline_signal(&obj->obj);

	mutex_lock(&obj->lock);
	if (obj->value < UINT_MAX)
		obj->value += inc;
	else
		obj->value = 0;
	mutex_unlock(&obj->lock);
}

void gspn_sync_fence_signal(struct gspn_sync_timeline *obj)
{
	if (obj==NULL)
		GSPN_LOGE("timeline is null\n");

	gspn_sync_timeline_inc(obj, 1);
}

int gspn_sync_fence_create_to_user(struct gspn_sync_timeline *obj,
							struct gspn_sync_create_fence_data * data,
							struct sync_fence **fen_arr, uint32_t *count)
{
	int fd = get_unused_fd();
	int err;
	struct sync_pt *pt;
	struct sync_fence *fence;

	if (fd < 0) {
		GSPN_LOGE("fd overflow, fd: %d\n", fd);
		return fd;
	}

	if (*count >= GSPN_FENCE_MAX_PER_CMD) {
		GSPN_LOGE("signal fence overflow, cnt: %d\n", *count);
		err = -EFAULT;
		goto err;
	}

	pt = gspn_sync_pt_create(obj, data->value);
	if (pt == NULL) {
		GSPN_LOGE("gspn pt create failed\n");
		err = -ENOMEM;
		goto err;
	}

	data->name[sizeof(data->name) - 1] = '\0';
	fence = sync_fence_create(data->name, pt);
	if (fence == NULL) {
		GSPN_LOGE("gspn fence create failed\n");
		sync_pt_free(pt);
		err = -ENOMEM;
		goto err;
	}

	if (put_user(fd, data->fd_addr)) {
		GSPN_LOGE("gspn fence put user failed\n");
		sync_fence_put(fence);
		err = -EFAULT;
		goto err;
	}

	sync_fence_install(fence, fd);
	/*store the new fence at the rls_fen array*/	
	fen_arr[*count] = fence;
	/*update the count of rls_fen*/
	(*count)++;

	return 0;

err:
	put_unused_fd(fd);
	return err;
}

int gspn_sync_acquire_fence_collect(struct sync_fence **acq_fen_arr,
									uint32_t *count, int fd)
{
	struct sync_fence *fence = NULL;
	int ret = -1;
	if (fd < 0) {
		GSPN_LOGE("acq fence fd less than zero\n");
		return ret;
	}	
	if (*count >= GSPN_FENCE_MAX_PER_CMD) {
		GSPN_LOGE("wait fence overflow, cnt: %d\n", *count);
		return ret;
	}
	fence = sync_fence_fdget(fd);
	if (fence != NULL) {
		/*store the acquire fence at the acq_fen array*/	
		acq_fen_arr[*count] = fence;
		/*update the count of rls_fen*/
		(*count)++;
		ret = 0;
	} else
		GSPN_LOGE("acquire fence get from fd error\n");
	
	return ret;
}

int gspn_image_layer_fence_process(GSPN_LAYER0_INFO_T *layer_info,
									struct gspn_layer_list_fence_data *list_data,
									int32_t __user *fd_addr,
									struct gspn_sync_timeline *timeline,
									unsigned int record)
{
	struct gspn_sync_create_fence_data data;
	int fd = 0;
	int ret = 0;
	uint32_t *rls_cnt = list_data->rls_cnt;
	uint32_t *acq_cnt = list_data->acq_cnt;
	struct sync_fence **rls_fen_arr = list_data->rls_fen_arr;
	struct sync_fence **acq_fen_arr = list_data->acq_fen_arr;
	int layer_enable = layer_info->layer_en;

	if (layer_enable) {
		data.name[sizeof(data.name) - 1] = '\0';
		strlcpy(data.name, "img_rls_fen", sizeof(data.name));

		gspn_sync_record_set(&data.value, record);

		data.fd_addr = fd_addr;

		ret = gspn_sync_fence_create_to_user(timeline, &data, rls_fen_arr, rls_cnt);
		if (ret) {
			GSPN_LOGE("img create fence failed\n");
			return ret;
		}

		fd = layer_info->acq_fen_fd;
		if (fd < 0)
			GSPN_LOGI("img acq fen fd has been closed\n");
		else
			ret |= gspn_sync_acquire_fence_collect(acq_fen_arr,
					acq_cnt, fd);
	}

	return ret;
}

int gspn_osd_layer_fence_process(GSPN_LAYER1_INFO_T **layer_info,
									struct gspn_layer_list_fence_data *list_data,
									int32_t __user** fd_addr,
									struct gspn_sync_timeline *timeline,
									unsigned int record)
{
	int ret = 0;
	struct gspn_sync_create_fence_data data;
	int fd = 0;
	int i = 0;
	GSPN_LAYER1_INFO_T *info;
	uint32_t *rls_cnt = list_data->rls_cnt;
	uint32_t *acq_cnt = list_data->acq_cnt;
	struct sync_fence **rls_fen_arr = list_data->rls_fen_arr;
	struct sync_fence **acq_fen_arr = list_data->acq_fen_arr;
	int layer_enable;

	for(i = 0; i < 3; i++) {
		info = layer_info[i];
		layer_enable = info->layer_en;

		if (layer_enable) {
			data.name[sizeof(data.name) - 1] = '\0';
			strlcpy(data.name, "osd_rls_fen", sizeof(data.name));

			gspn_sync_record_set(&data.value, record);

			data.fd_addr = fd_addr[i];

			ret = gspn_sync_fence_create_to_user(timeline, &data,
					rls_fen_arr, rls_cnt);
			if (ret) {
				GSPN_LOGE("osd create fence failed\n");
				return ret;
			}

			fd = info->acq_fen_fd;
			if (fd < 0)
				GSPN_LOGI("osd acq fen fd has been closed\n");
			else
				ret |= gspn_sync_acquire_fence_collect(acq_fen_arr,
						acq_cnt, fd);
		}

		if (ret)
			break;
	}
	return ret;
}

int gspn_des1_layer_fence_process(GSPN_LAYER_DES1_INFO_T *layer_info,
									struct gspn_layer_list_fence_data *list_data,
									int32_t __user *fd_addr,
									struct gspn_sync_timeline *timeline,
									unsigned int record)
{
	struct gspn_sync_create_fence_data data;
	int fd = 0;
	int ret = 0;
	uint32_t *rls_cnt = list_data->rls_cnt;
	uint32_t *acq_cnt = list_data->acq_cnt;
	struct sync_fence **rls_fen_arr = list_data->rls_fen_arr;
	struct sync_fence **acq_fen_arr = list_data->acq_fen_arr;

	int layer_enable = layer_info->layer_en;
	if (layer_enable) {
		data.name[sizeof(data.name) - 1] = '\0';
		strlcpy(data.name, "des1_rls_fen", sizeof(data.name));

		gspn_sync_record_set(&data.value, record);

		data.fd_addr = fd_addr;

		ret = gspn_sync_fence_create_to_user(timeline, &data,
				rls_fen_arr, rls_cnt);
		if (ret) {
			GSPN_LOGE("des1 create fence failed, ret: %d\n", ret);
			return ret;
		}

		fd = layer_info->acq_fen_fd;
		if (fd < 0)
			GSPN_LOGI("des1 acq fen fd has been closed\n");
		else
			ret |= gspn_sync_acquire_fence_collect(acq_fen_arr,
					acq_cnt, fd);
	}

	return ret;
}

int gspn_des2_layer_fence_process(GSPN_LAYER_DES2_INFO_T *layer_info,
									struct gspn_layer_list_fence_data *list_data,
									int32_t __user *fd_addr,
									struct gspn_sync_timeline *timeline,
									unsigned int record)
{
	struct gspn_sync_create_fence_data data;
	int fd = 0;
	int ret = 0;
	uint32_t *rls_cnt = list_data->rls_cnt;
	uint32_t *acq_cnt = list_data->acq_cnt;
	struct sync_fence **rls_fen_arr = list_data->rls_fen_arr;
	struct sync_fence **acq_fen_arr = list_data->acq_fen_arr;

	int layer_enable = layer_info->layer_en;
	if (layer_enable) {
		data.name[sizeof(data.name) - 1] = '\0';
		strlcpy(data.name, "des2_rls_fen", sizeof(data.name));

		gspn_sync_record_set(&data.value, record);

		data.fd_addr = fd_addr;

		ret = gspn_sync_fence_create_to_user(timeline, &data,
				rls_fen_arr, rls_cnt);
		if (ret) {
			GSPN_LOGE("des2 create fence failed\n");
			return ret;
		}

		fd = layer_info->acq_fen_fd;
		if (fd < 0)
			GSPN_LOGI("des2 acq fen fd has been closed\n");
		else
			ret |= gspn_sync_acquire_fence_collect(acq_fen_arr,
					acq_cnt, fd);
	}

	return ret;
}

void gspn_layer_list_release_fence_free(struct sync_fence **fen_arr,
											uint32_t *count)
{
	uint32_t i = 0;
	uint32_t total = *count;
	if (fen_arr == NULL || count == 0)
		GSPN_LOGE("fence array is null,can't destroy\n");
	for(i = 0; i < total; i++) {
		if (fen_arr[i] != NULL) {
			sync_fence_put(fen_arr[i]);
			(*count)--;
		}
		else
			GSPN_LOGE("release fen_arr[%d] is NULL\n", i);
	}
	if (*count != 0)
		GSPN_LOGE("wait fence free leakage\n");
}

void gspn_layer_list_acquire_fence_free(struct sync_fence **fen_arr,
											uint32_t *count)
{
	uint32_t i = 0;
	uint32_t total = *count;
	if (fen_arr == NULL || count == 0)
		GSPN_LOGE("fence array is null,can't destroy\n");
	for(i = 0; i < total; i++) {
		if (fen_arr[i] != NULL) {
			sync_fence_put(fen_arr[i]);
			(*count)--;
		}
		else
			GSPN_LOGE("acquire fen_arr[%d] is NULL\n", i);
	}
	if (*count != 0)
		GSPN_LOGE("acquire fence free leakage\n");
}

void gspn_layer_list_fence_free(
		struct gspn_layer_list_fence_data* list_data)
{
	struct sync_fence **fen_arr;
	uint32_t *count;

	fen_arr = list_data->rls_fen_arr;
	count = list_data->rls_cnt;
	GSPN_LOGE("count: %u\n", *count);
	if (fen_arr == NULL || count == 0)
		GSPN_LOGE("free release fence data paras error"
				"fen_arr: %p, count: %u\n", fen_arr, *count);
	else
		gspn_layer_list_release_fence_free(fen_arr, count);


	fen_arr = list_data->acq_fen_arr;
	count = list_data->acq_cnt;
	if (fen_arr == NULL || count == 0)
		GSPN_LOGE("free acquire fence data paras error"
				"fen_arr: %p, count: %u\n", fen_arr, *count);
	else
		gspn_layer_list_acquire_fence_free(fen_arr, count);

}

int gspn_layer_list_fence_process(GSPN_KCMD_INFO_T *kcmd,
									struct gspn_sync_timeline *timeline)
{
	int ret = -1;
	unsigned int r = 0;
	GSPN_CMD_INFO_T cmd = kcmd->src_cmd;
	GSPN_LAYER0_INFO_T *img_layer = &cmd.l0_info;
	GSPN_LAYER1_INFO_T *osd_layer = &cmd.l1_info;
	GSPN_LAYER_DES1_INFO_T *des1_layer = &cmd.des1_info;
	GSPN_LAYER_DES2_INFO_T *des2_layer = &cmd.des2_info;

	struct gspn_layer_list_fence_data list_data;
	int32_t __user *img_rls_fd;
	int32_t __user *des1_rls_fd;
	int32_t __user *des2_rls_fd;
	int32_t __user *osd_rls_fd_arr[3];

	list_data.rls_cnt = &kcmd->rls_fence_cnt;
	list_data.acq_cnt = &kcmd->acq_fence_cnt;
	*list_data.rls_cnt = 0;
	*list_data.acq_cnt = 0;
	list_data.rls_fen_arr = kcmd->rls_fence;
	list_data.acq_fen_arr = kcmd->acq_fence;

	img_rls_fd = &(kcmd->ucmd->l0_info).rls_fen_fd;
	des1_rls_fd = &(kcmd->ucmd->des1_info).rls_fen_fd;
	des2_rls_fd = &(kcmd->ucmd->des2_info).rls_fen_fd;

	osd_rls_fd_arr[0] = &(kcmd->ucmd->l1_info).rls_fen_fd;
	osd_rls_fd_arr[1] = &(kcmd->ucmd->l2_info).rls_fen_fd;
	osd_rls_fd_arr[2] = &(kcmd->ucmd->l3_info).rls_fen_fd;

	gspn_sync_record_get(&r, timeline);
	GSPN_LOGI("set record: %u", r);

	ret = gspn_image_layer_fence_process(img_layer, &list_data,
										img_rls_fd, timeline, r);
	if (ret) {
		GSPN_LOGE("gspn image layer fence process failed\n");
		goto error;
	}
	ret |= gspn_osd_layer_fence_process(&osd_layer, &list_data,
										osd_rls_fd_arr, timeline, r);
	if (ret) {
		GSPN_LOGE("gspn osd layer fence process failed\n");
		goto error;
	}

	ret |= gspn_des1_layer_fence_process(des1_layer, &list_data,
										des1_rls_fd, timeline, r);
	if (ret) {
		GSPN_LOGE("gspn des1 layer fence process failed\n");
		goto error;
	}

	ret |= gspn_des2_layer_fence_process(des2_layer, &list_data,
										des2_rls_fd, timeline, r);
	if (ret) {
		GSPN_LOGE("gspn des2 layer fence process failed\n");
		goto error;
	}

	gspn_sync_record_inc(timeline);
	return ret;
error:
	GSPN_LOGE("layer list fence process failed\n");
	gspn_layer_list_fence_free(&list_data);
	return ret;	
}
