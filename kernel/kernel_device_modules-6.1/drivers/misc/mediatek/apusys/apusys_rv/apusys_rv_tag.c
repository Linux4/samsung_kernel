// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

/*
 *=============================================================
 * Include files
 *=============================================================
 */

#include <linux/atomic.h>
#include <linux/proc_fs.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/tracepoint.h>

#if IS_ENABLED(CONFIG_MTK_APUSYS_DEBUG)
#include "apu_tags.h"
#include "apu_tp.h"
#include "apusys_rv_tag.h"
#include "apu.h"

/* tags */
static struct apu_tags *apusys_rv_drv_tags;

enum apusys_rv_tag_type {
	APUSYS_RV_TAG_IPI_SEND,
	APUSYS_RV_TAG_IPI_HANDLE,
	APUSYS_RV_TAG_PWR_CTRL,
};

/* The parameters must aligned with trace_apusys_rv_ipi_send() */
static void probe_apusys_rv_ipi_send(void *data, unsigned int id, unsigned int len,
	unsigned int serial_no, unsigned int csum, unsigned int user_id, int usage_cnt,
	uint64_t elapse)
{
	struct apusys_rv_tag t;

	if (!apusys_rv_drv_tags)
		return;

	t.type = APUSYS_RV_TAG_IPI_SEND;

	t.d.ipi_send.id = id;
	t.d.ipi_send.len = len;
	t.d.ipi_send.serial_no = serial_no;
	t.d.ipi_send.csum = csum;
	t.d.ipi_send.user_id = user_id;
	t.d.ipi_send.usage_cnt = usage_cnt;
	t.d.ipi_send.elapse = elapse;

	apu_tag_add(apusys_rv_drv_tags, &t);
}

/* The parameters must aligned with trace_apusys_rv_ipi_handle() */
static void probe_apusys_rv_ipi_handle(void *data, unsigned int id, unsigned int len,
	unsigned int serial_no, unsigned int csum, unsigned int user_id, int usage_cnt,
	uint64_t top_start_time, uint64_t bottom_start_time, uint64_t latency, uint64_t elapse,
	uint64_t t_handler)
{
	struct apusys_rv_tag t;

	if (!apusys_rv_drv_tags)
		return;

	t.type = APUSYS_RV_TAG_IPI_HANDLE;

	t.d.ipi_handle.id = id;
	t.d.ipi_handle.len = len;
	t.d.ipi_handle.serial_no = serial_no;
	t.d.ipi_handle.csum = csum;
	t.d.ipi_handle.user_id = user_id;
	t.d.ipi_handle.usage_cnt = usage_cnt;
	t.d.ipi_handle.top_start_time = top_start_time;
	t.d.ipi_handle.bottom_start_time = bottom_start_time;
	t.d.ipi_handle.latency = latency;
	t.d.ipi_handle.elapse = elapse;
	t.d.ipi_handle.t_handler = t_handler;

	apu_tag_add(apusys_rv_drv_tags, &t);
}

/* The parameters must aligned with trace_apusys_rv_pwr_ctrl() */
static void probe_apusys_rv_pwr_ctrl(void *data, unsigned int id,
	unsigned int on, unsigned int off, uint64_t latency,
	uint64_t sub_latency_0, uint64_t sub_latency_1, uint64_t sub_latency_2,
	uint64_t sub_latency_3, uint64_t sub_latency_4, uint64_t sub_latency_5,
	uint64_t sub_latency_6, uint64_t sub_latency_7)
{
	struct apusys_rv_tag t;

	if (!apusys_rv_drv_tags)
		return;

	t.type = APUSYS_RV_TAG_PWR_CTRL;

	t.d.pwr_ctrl.id = id;
	t.d.pwr_ctrl.on = on;
	t.d.pwr_ctrl.off = off;
	t.d.pwr_ctrl.latency = latency;
	t.d.pwr_ctrl.sub_latency_0 = sub_latency_0;
	t.d.pwr_ctrl.sub_latency_1 = sub_latency_1;
	t.d.pwr_ctrl.sub_latency_2 = sub_latency_2;
	t.d.pwr_ctrl.sub_latency_3 = sub_latency_3;
	t.d.pwr_ctrl.sub_latency_4 = sub_latency_4;
	t.d.pwr_ctrl.sub_latency_5 = sub_latency_5;
	t.d.pwr_ctrl.sub_latency_6 = sub_latency_6;
	t.d.pwr_ctrl.sub_latency_7 = sub_latency_7;

	apu_tag_add(apusys_rv_drv_tags, &t);
}

static void apusys_rv_tag_seq_ipi_send(struct seq_file *s, struct apusys_rv_tag *t)
{
	seq_printf(s, "ipi_send:id=%d,len=%d,serial_no=%d,csum=0x%x,user_id=0x%x,",
		t->d.ipi_send.id, t->d.ipi_send.len, t->d.ipi_send.serial_no,
		t->d.ipi_send.csum, t->d.ipi_send.user_id);
	seq_printf(s, "usage_cnt=%d,elapse=%llu\n",
		t->d.ipi_send.usage_cnt, t->d.ipi_send.elapse);
}

static void apusys_rv_tag_seq_ipi_handle(struct seq_file *s, struct apusys_rv_tag *t)
{
	seq_printf(s, "ipi_handle:id=%d,len=%d,serial_no=%d,csum=0x%x,user_id=0x%x,",
		t->d.ipi_handle.id, t->d.ipi_handle.len, t->d.ipi_handle.serial_no,
		t->d.ipi_handle.csum, t->d.ipi_handle.user_id);
	seq_printf(s, "usage_cnt=%d,top_start_time=%llu,bottom_start_time=%llu,",
		t->d.ipi_handle.usage_cnt,
		t->d.ipi_handle.top_start_time, t->d.ipi_handle.bottom_start_time);
	seq_printf(s, "latency=%llu,elapse=%llu,t_handler=%llu\n",
		t->d.ipi_handle.latency, t->d.ipi_handle.elapse, t->d.ipi_handle.t_handler);
}

static void apusys_rv_tag_seq_pwr_ctrl(struct seq_file *s, struct apusys_rv_tag *t)
{
	seq_printf(s, "pwr_ctrl:id=%d,on=%d,off=%d,latency=%llu,",
		t->d.pwr_ctrl.id, t->d.pwr_ctrl.on, t->d.pwr_ctrl.off, t->d.pwr_ctrl.latency);
	seq_printf(s, "sub_lat0=%llu,sub_lat1=%llu,sub_lat2=%llu,",
		t->d.pwr_ctrl.sub_latency_0, t->d.pwr_ctrl.sub_latency_1, t->d.pwr_ctrl.sub_latency_2);
	seq_printf(s, "sub_lat3=%llu,sub_lat4=%llu,sub_lat5=%llu,",
		t->d.pwr_ctrl.sub_latency_3, t->d.pwr_ctrl.sub_latency_4, t->d.pwr_ctrl.sub_latency_5);
	seq_printf(s, "sub_lat6=%llu,sub_lat7=%llu\n",
		t->d.pwr_ctrl.sub_latency_6, t->d.pwr_ctrl.sub_latency_7);
}

static int apusys_rv_tag_seq(struct seq_file *s, void *tag, void *priv)
{
	struct apusys_rv_tag *t = (struct apusys_rv_tag *)tag;

	if (!t)
		return -ENOENT;

	if (t->type == APUSYS_RV_TAG_IPI_SEND)
		apusys_rv_tag_seq_ipi_send(s, t);
	else if (t->type == APUSYS_RV_TAG_IPI_HANDLE)
		apusys_rv_tag_seq_ipi_handle(s, t);
	else if (t->type == APUSYS_RV_TAG_PWR_CTRL)
		apusys_rv_tag_seq_pwr_ctrl(s, t);

	return 0;
}

static int apusys_rv_tag_seq_info(struct seq_file *s, void *tag, void *priv)
{
	return 0;
}

static struct apu_tp_tbl apusys_rv_tp_tbl[] = {
	{.name = "apusys_rv_ipi_send", .func = probe_apusys_rv_ipi_send},
	{.name = "apusys_rv_ipi_handle", .func = probe_apusys_rv_ipi_handle},
	{.name = "apusys_rv_pwr_ctrl", .func = probe_apusys_rv_pwr_ctrl},
	APU_TP_TBL_END
};

void apusys_rv_tags_show(struct seq_file *s)
{
	apu_tags_seq(apusys_rv_drv_tags, s);
}

int apusys_rv_init_drv_tags(struct mtk_apu *apu)
{
	int ret;

	apusys_rv_drv_tags = apu_tags_alloc("apusys_rv", sizeof(struct apusys_rv_tag),
		APUSYS_RV_TAGS_CNT, apusys_rv_tag_seq, apusys_rv_tag_seq_info, NULL);

	if (!apusys_rv_drv_tags)
		return -ENOMEM;

	ret = apu_tp_init(apusys_rv_tp_tbl);
	if (ret) {
		dev_info(apu->dev, "%s: unable to register\n", __func__);
		apu_tags_free(apusys_rv_drv_tags);
	}

	return ret;
}

void apusys_rv_exit_drv_tags(struct mtk_apu *apu)
{
	apu_tp_exit(apusys_rv_tp_tbl);
	apu_tags_free(apusys_rv_drv_tags);
}

#endif /* CONFIG_MTK_APUSYS_DEBUG */

