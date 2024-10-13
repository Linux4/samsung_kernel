/*
 * drivers/base/gsp_sync.c
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
#include "gsp_sync.h"
#include "gsp_drv_common.h"
#include "gsp_work_queue.h"

static int gsp_sync_cmp(u32 a, u32 b)
{
	if (a == b)
		return 0;

	return ((s32)a - (s32)b) < 0 ? -1 : 1;
}

struct sync_pt *gsp_sync_pt_create(struct gsp_sync_timeline *obj, u32 value)
{
	struct gsp_sync_pt *pt;

	pt = (struct gsp_sync_pt *)
		sync_pt_create(&obj->obj, sizeof(struct gsp_sync_pt));

	pt->value = value;

	return (struct sync_pt *)pt;
}

static struct sync_pt *gsp_sync_pt_dup(struct sync_pt *sync_pt)
{
	struct gsp_sync_pt *pt = (struct gsp_sync_pt *) sync_pt;
	struct gsp_sync_timeline *obj =
		(struct gsp_sync_timeline *)sync_pt->parent;

	return (struct sync_pt *) gsp_sync_pt_create(obj, pt->value);
}

static int gsp_sync_pt_has_signaled(struct sync_pt *sync_pt)
{
	struct gsp_sync_pt *pt = (struct gsp_sync_pt *)sync_pt;
	struct gsp_sync_timeline *obj =
		(struct gsp_sync_timeline *)sync_pt->parent;

	return gsp_sync_cmp(obj->value, pt->value) >= 0;
}

static int gsp_sync_pt_compare(struct sync_pt *a, struct sync_pt *b)
{
	struct gsp_sync_pt *pt_a = (struct gsp_sync_pt *)a;
	struct gsp_sync_pt *pt_b = (struct gsp_sync_pt *)b;

	return gsp_sync_cmp(pt_a->value, pt_b->value);
}

static int gsp_sync_fill_driver_data(struct sync_pt *sync_pt,
				    void *data, int size)
{
	struct gsp_sync_pt *pt = (struct gsp_sync_pt *)sync_pt;

	if (size < sizeof(pt->value))
		return -ENOMEM;

	memcpy(data, &pt->value, sizeof(pt->value));

	return sizeof(pt->value);
}

static void gsp_sync_timeline_value_str(struct sync_timeline *sync_timeline,
				       char *str, int size)
{
	struct gsp_sync_timeline *timeline =
		(struct gsp_sync_timeline *)sync_timeline;
	snprintf(str, size, "%d", timeline->value);
}

static void gsp_sync_pt_value_str(struct sync_pt *sync_pt,
				       char *str, int size)
{
	struct gsp_sync_pt *pt = (struct gsp_sync_pt *)sync_pt;
	snprintf(str, size, "%d", pt->value);
}

static struct sync_timeline_ops gsp_sync_timeline_ops = {
	.driver_name = "gsp_sync",
	.dup = gsp_sync_pt_dup,
	.has_signaled = gsp_sync_pt_has_signaled,
	.compare = gsp_sync_pt_compare,
	.fill_driver_data = gsp_sync_fill_driver_data,
	.timeline_value_str = gsp_sync_timeline_value_str,
	.pt_value_str = gsp_sync_pt_value_str,
};

static void gsp_sync_record_inc(struct gsp_sync_timeline *tl)
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

static void gsp_sync_record_get(unsigned int *value,
		struct gsp_sync_timeline *tl)
{
	mutex_lock(&tl->lock);
	*value = tl->record;
	mutex_unlock(&tl->lock);
}

static void gsp_sync_record_set(unsigned int *value,
		unsigned int record)
{
	*value = record;
}

int gsp_sync_timeline_destroy(struct gsp_sync_timeline* obj)
{
	sync_timeline_destroy(&obj->obj);
	return 0;
}

struct gsp_sync_timeline *gsp_sync_timeline_create(const char *name)
{
	struct gsp_sync_timeline *obj = (struct gsp_sync_timeline *)
		sync_timeline_create(&gsp_sync_timeline_ops,
				     sizeof(struct gsp_sync_timeline),
				     name);
	mutex_init(&obj->lock);
	obj->value = 0;
	obj->record = 1;
	return obj;
}

void gsp_sync_timeline_inc(struct gsp_sync_timeline *obj, u32 inc)
{
	int r = -1;

	mutex_lock(&obj->lock);
	if (obj->value < UINT_MAX)
		obj->value += inc;
	else
		obj->value = 0;
	mutex_unlock(&obj->lock);
	sync_timeline_signal(&obj->obj);

	mutex_lock(&obj->lock);
	r = obj->value;
	mutex_unlock(&obj->lock);
	GSP_DEBUG("signal timeline value: %u\n", r);
}

void gsp_sync_fence_signal(struct gsp_sync_timeline *obj)
{
	if (obj==NULL)
		GSP_ERR("timeline is null\n");
	gsp_sync_timeline_inc(obj, 1);
}

extern void fence_sprintf(char* str,char* tag,int n);
int gsp_sync_fence_create_to_user(struct gsp_sync_timeline *obj,
							struct gsp_sync_create_fence_info *info,
							struct sync_fence **fen)
{
	int fd = get_unused_fd();
	char name[32];
	int err;
	struct sync_pt *pt;
	struct sync_fence *fence;

	if (fd < 0) {
		GSP_ERR("fd overflow, fd: %d\n", fd);
		return fd;
	}

	pt = gsp_sync_pt_create(obj, info->value);
	if (pt == NULL) {
		GSP_ERR("pt create failed\n");
		err = -ENOMEM;
		goto err;
	}

#if 0
	info->name[sizeof(info->name) - 1] = '\0';
	fence = sync_fence_create(info->name, pt);
#else
	fence_sprintf(name,"GSP",info->value);
	fence = sync_fence_create(name, pt);
#endif

	if (fence == NULL) {
		GSP_ERR("fence create failed\n");
		sync_pt_free(pt);
		err = -ENOMEM;
		goto err;
	}

	if (put_user(fd, info->fd_addr)) {
		GSP_ERR("fence put user failed\n");
		sync_fence_put(fence);
		err = -EFAULT;
		goto err;
	}
	sync_fence_install(fence, fd);
	/*store the new fence with the sig_fen pointer*/
	*fen = fence;

	return 0;

err:
	put_unused_fd(fd);
	return err;
}

int gsp_sync_wait_fence_collect(struct sync_fence **wait_fen_arr,
									uint32_t *count, int fd)
{
	struct sync_fence *fence = NULL;
	int ret = -1;

	if (fd < 0) {
		GSP_ERR("wait fence fd less than zero\n");
		return ret;
	}

	if (*count >= GSP_WAIT_FENCE_MAX) {
		GSP_ERR("wait fence overflow, cnt: %d\n", *count);
		return ret;
	}

	fence = sync_fence_fdget(fd);
	if (fence != NULL) {
		/*store the wait fence at the wait_fen array*/	
		wait_fen_arr[*count] = fence;
		/*update the count of sig_fen*/
		(*count)++;
		ret = 0;
	} else
		GSP_ERR("wait fence get from fd: %d error\n", fd);
	
	return ret;
}

int gsp_img_layer_fence_process(struct gsp_fence_data *data,
								struct gsp_sync_timeline *timeline,
								unsigned int record)
{
	int fd = 0;
	int ret = 0;

	if (data->enable.img_en) {
		fd = data->img_wait_fd;
		if (fd < 0)
			GSP_DEBUG("img wait fen fd has been closed\n");
		else
			ret |= gsp_sync_wait_fence_collect(data->wait_fen_arr,
					&data->wait_cnt, fd);
	}

	return ret;
}

int gsp_osd_layer_fence_process(struct gsp_fence_data *data,
								struct gsp_sync_timeline *timeline,
								unsigned int record)
{
	int ret = 0;
	int fd = 0;

	if (data->enable.osd_en) {
		fd = data->osd_wait_fd;
		if (fd < 0)
			GSP_DEBUG("osd wait fen fd has been closed\n");
		else
			ret |= gsp_sync_wait_fence_collect(data->wait_fen_arr,
					&data->wait_cnt, fd);
	}

	return ret;
}

int gsp_des_layer_fence_process(struct gsp_fence_data *data,
								struct gsp_sync_timeline *timeline,
								unsigned int record)
{
	struct gsp_sync_create_fence_info info;
	int fd = 0;
	int ret = 0;

	info.name[sizeof(info.name) - 1] = '\0';
	strlcpy(info.name, "gsp_sig_fen", sizeof(info.name));

	gsp_sync_record_set(&info.value, record);

	info.fd_addr = data->des_sig_ufd;

	ret = gsp_sync_fence_create_to_user(timeline, &info,
			&data->sig_fen);
	if (ret) {
		GSP_ERR("des create fence failed, ret: %d\n", ret);
		return ret;
	}

	fd = data->des_wait_fd;
	if (fd < 0)
		GSP_DEBUG("des wait fen fd has been closed\n");
	else
		ret |= gsp_sync_wait_fence_collect(data->wait_fen_arr,
				&data->wait_cnt, fd);

	return ret;
}

void gsp_layer_list_signal_fence_free(struct sync_fence *fen)
{
	if (fen == NULL)
		GSP_ERR("fence is null,can't destroy\n");
	sync_fence_put(fen);
}

void gsp_layer_list_wait_fence_free(struct sync_fence **fen_arr,
											uint32_t *count)
{
	uint32_t i = 0;
	uint32_t total = *count;
	if (fen_arr == NULL || count == 0)
		GSP_ERR("fence array is null,can't destroy\n");
	for(i = 0; i < total; i++) {
		if (fen_arr[i] != NULL) {
			sync_fence_put(fen_arr[i]);
			(*count)--;
		}
		else
			GSP_ERR("wait fen_arr[%d] is NULL\n", i);
	}
	if (*count != 0)
		GSP_ERR("wait fence free leakage\n");
}

void gsp_layer_list_user_fd_destroy(int __user **ufd)
{
	put_user(-1, *ufd);
}

void gsp_layer_list_fence_free(struct gsp_fence_data* data)
{
	uint32_t *count;
	struct sync_fence *fen = data->sig_fen;
	struct sync_fence **fen_arr = data->wait_fen_arr;
	/*destroy signal fence*/
	if (fen == NULL)
		GSP_ERR("free signal fence data paras error"
				"fen: %p.\n", fen);
	else
		gsp_layer_list_signal_fence_free(fen);

	/*destroy wait fence*/
	fen_arr = data->wait_fen_arr;
	count = &data->wait_cnt;
	if (fen_arr == NULL || count == 0)
		GSP_ERR("free wait fence data paras error"
				"fen_arr: %p, count: %d\n", fen_arr, *count);
	else
		gsp_layer_list_wait_fence_free(fen_arr, count);

	/*destroy user space fd*/
	gsp_layer_list_user_fd_destroy(&data->des_sig_ufd);
}

int gsp_layer_list_fence_process(struct gsp_fence_data *data,
									struct gsp_sync_timeline *timeline)
{
	int ret = -1;
	unsigned int r = 0;
	if (data == NULL || timeline ==  NULL) {
		GSP_ERR("gsp layer list fence process NULL pointer\n");
	}

	gsp_sync_record_get(&r, timeline);
	GSP_DEBUG("gsp set record: %u\n", r);
	ret = gsp_img_layer_fence_process(data, timeline, r);
	if (ret) {
		GSP_ERR("image layer fence process failed\n");
		goto free;
	}
	ret |= gsp_osd_layer_fence_process(data, timeline, r);
	if (ret) {
		GSP_ERR("osd layer fence process failed\n");
		goto free;
	}

	ret |= gsp_des_layer_fence_process(data, timeline, r);
	if (ret) {
		GSP_ERR("des layer fence process failed\n");
		goto free;
	}
	gsp_sync_record_inc(timeline);

	return ret;

free:
	GSP_ERR("layer list fence process failed\n");
	gsp_layer_list_fence_free(data);
	return ret;	
}
