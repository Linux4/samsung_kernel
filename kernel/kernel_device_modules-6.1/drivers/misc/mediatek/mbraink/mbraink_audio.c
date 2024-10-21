// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <swpm_module_ext.h>
#include <linux/string.h>
#include <linux/timekeeping.h>
#include <linux/workqueue.h>
#include "mbraink_audio.h"

#if IS_ENABLED(CONFIG_SND_SOC_MTK_SMART_PHONE)
#define MAX_AUDIO_UDM_BUFFER_BANK 2
#define MAX_AUDIO_UDM_LOG_SIZE 256 //Target: 32
#define MAX_AUDIO_UDM_QUEUE 8 //Target: 60
#define MAX_AUDIO_UDM_HEAD_SIZE 32
#define AudioUDMLogLogToken     "__"
#define AudioUDMLogFormatToken  "M&"
#define MIN(a, b) (((a) < (b))?(a):(b))

struct mbraink_udm_buffer {
	char buffer[MAX_AUDIO_UDM_QUEUE][MAX_AUDIO_UDM_LOG_SIZE];
	long long timestamp[MAX_AUDIO_UDM_QUEUE];
	int level[MAX_AUDIO_UDM_QUEUE];
	unsigned int counter;
};

#if (MBRAINK_LANDING_FEATURE_CHECK == 1)
void (*audiokeylog2mbrain_fp)(int level, const char *buf);
#endif

static int udm_wq_init(void);
static int udm_wq_exit(void);
static void udm_work_handler(struct work_struct *work);
static void udm_buffer_init(void);
static void udm_buffer_deinit(void);
static int udm_buffer_write(struct mbraink_udm_buffer *pMbraink_udm_buffer,
								const char *pData,
								unsigned char size,
								int level);
static void udm_buffer_reset(struct mbraink_udm_buffer *pMbraink_udm_buffer);

struct mbraink_udm_buffer gUdmBufferBank[MAX_AUDIO_UDM_BUFFER_BANK];
unsigned int gBankId = 0xFF;

DECLARE_WORK(udm_work, udm_work_handler);
struct workqueue_struct *udm_wq;

static void udm_work_handler(struct work_struct *work)
{
	unsigned int targetBankId = (gBankId == 0) ? 1:0;
	int i = 0;
	char temp[NETLINK_EVENT_MESSAGE_SIZE];
	int posi = 0;
	int length = 0;

	memset(temp, 0x00, sizeof(char)*NETLINK_EVENT_MESSAGE_SIZE);

	for (i = 0; i < MAX_AUDIO_UDM_QUEUE; i++) {
		if ((posi+(MAX_AUDIO_UDM_HEAD_SIZE)+
			strlen(gUdmBufferBank[targetBankId].buffer[i])) >=
			NETLINK_EVENT_MESSAGE_SIZE) {
			//send to mbrian
			if (strlen(temp) != 0)
				mbraink_netlink_send_msg(temp);

			//reset.
			memset(temp, 0x00, sizeof(char)*NETLINK_EVENT_MESSAGE_SIZE);
			length = 0;
			posi = 0;
		}

		//MSG Format M&TimestampM&LevelM&Log__
		length = scnprintf(temp + posi,
					NETLINK_EVENT_MESSAGE_SIZE - posi,
					"%s%lld%s%d%s%s%s",
					AudioUDMLogFormatToken,
					gUdmBufferBank[targetBankId].timestamp[i],
					AudioUDMLogFormatToken,
					gUdmBufferBank[targetBankId].level[i],
					AudioUDMLogFormatToken,
					gUdmBufferBank[targetBankId].buffer[i],
					AudioUDMLogLogToken);
		posi += length;
	}

	//send to mbrain.
	if (strlen(temp) != 0)
		mbraink_netlink_send_msg(temp);

	//reset bank.
	udm_buffer_reset(&gUdmBufferBank[targetBankId]);

}

static int udm_wq_init(void)
{
	if (!udm_wq)
		udm_wq = create_workqueue("udm_wq");

	if (!udm_wq) {
		pr_notice("allocate udm_wq fail!\n");
		return -ENOMEM;
	}

	return 0;
}

static int udm_wq_exit(void)
{
	if (udm_wq) {
		flush_workqueue(udm_wq);
		destroy_workqueue(udm_wq);
		udm_wq = NULL;
	}

	return 0;
}

static int udm_buffer_write(struct mbraink_udm_buffer *pMbraink_udm_buffer,
					const char *pData,
					unsigned char size, int level)
{
	unsigned char pos = pMbraink_udm_buffer->counter;
	struct timespec64 tv = { 0 };

	if (pMbraink_udm_buffer->counter == MAX_AUDIO_UDM_QUEUE) {
		pr_notice("ERROR UDM Buffer not available");
		return -1;
	} else if (gBankId == 0xFF) {
		pr_info("audio udm not init yet.");
		return -1;
	}

	ktime_get_real_ts64(&tv);
	strncpy(pMbraink_udm_buffer->buffer[pos], pData, size-1);
	pMbraink_udm_buffer->timestamp[pos] = (tv.tv_sec*1000)+(tv.tv_nsec/1000000);
	pMbraink_udm_buffer->level[pos] = level;
	pMbraink_udm_buffer->counter++;
	if (pMbraink_udm_buffer->counter == MAX_AUDIO_UDM_QUEUE) {
		pr_info("Queue full (%d) at bankId (%d) ", MAX_AUDIO_UDM_QUEUE, gBankId);
		gBankId = (gBankId == 0) ? 1:0;
		queue_work(udm_wq, &udm_work);
		return 1;
	}

	return 0;
}

static void udm_buffer_reset(struct mbraink_udm_buffer *pMbraink_udm_buffer)
{
	pMbraink_udm_buffer->counter = 0;
	memset(pMbraink_udm_buffer->buffer,
		0x00,
		MAX_AUDIO_UDM_QUEUE*MAX_AUDIO_UDM_LOG_SIZE*sizeof(char));
	memset(pMbraink_udm_buffer->timestamp,
		0x00,
		MAX_AUDIO_UDM_QUEUE*sizeof(long long));
}

static void udm_buffer_init(void)
{
	int i = 0;

	for (i = 0; i < MAX_AUDIO_UDM_BUFFER_BANK; i++)
		udm_buffer_reset(&gUdmBufferBank[i]);
	gBankId = 0;
}

static void udm_buffer_deinit(void)
{
	int i = 0;

	for (i = 0; i < MAX_AUDIO_UDM_BUFFER_BANK; i++)
		udm_buffer_reset(&gUdmBufferBank[i]);
	gBankId = 0;
}

static int udm_init(void)
{
	int ret = 0;
	//1. callback register.
	audiokeylog2mbrain_fp = audiokeylog2mbrain;

	//2. udm buffer create.
	udm_buffer_init();

	//3. udm wq create.
	ret = udm_wq_init();

	return ret;
}

static int udm_deinit(void)
{
	int ret = 0;

	//1. callback de-register
	audiokeylog2mbrain_fp = NULL;

	//2. udm buffer reset.
	udm_buffer_deinit();

	//3. release udm wq
	ret = udm_wq_exit();

	return ret;
}

int mbraink_audio_init(void)
{
	udm_init();
	return 0;
}

int mbraink_audio_deinit(void)
{
	udm_deinit();
	return 0;
}

void audiokeylog2mbrain(int level, const char *buf)
{
	udm_buffer_write(&gUdmBufferBank[gBankId], buf, strlen(buf), level);
}

#else
int mbraink_audio_init(void)
{
	pr_info("%s: not enable do nothing\n", __func__);
	return 0;
}

int mbraink_audio_deinit(void)
{
	pr_info("%s: not enable do nothing\n", __func__);
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_MTK_SWPM_MODULE)
int mbraink_audio_getIdleRatioInfo(struct mbraink_audio_idleRatioInfo *pmbrainkAudioIdleRatioInfo)
{
	int ret = 0;
	struct ddr_sr_pd_times *ddr_sr_pd_times_ptr = NULL;
	struct timespec64 tv = { 0 };

	ddr_sr_pd_times_ptr = kmalloc(sizeof(struct ddr_sr_pd_times), GFP_KERNEL);
	if (!ddr_sr_pd_times_ptr) {
		ret = -1;
		goto End;
	}

	sync_latest_data();
	ktime_get_real_ts64(&tv);

	get_ddr_sr_pd_times(ddr_sr_pd_times_ptr);

	pmbrainkAudioIdleRatioInfo->timestamp = (tv.tv_sec*1000)+(tv.tv_nsec/1000000);
	pmbrainkAudioIdleRatioInfo->adsp_active_time = 0;
	pmbrainkAudioIdleRatioInfo->adsp_wfi_time = 0;
	pmbrainkAudioIdleRatioInfo->adsp_pd_time = 0;
	pmbrainkAudioIdleRatioInfo->s0_time = ddr_sr_pd_times_ptr->pd_time;
	pmbrainkAudioIdleRatioInfo->s1_time = ddr_sr_pd_times_ptr->sr_time;
	pmbrainkAudioIdleRatioInfo->mcusys_active_time = 0;
	pmbrainkAudioIdleRatioInfo->mcusys_pd_time = 0;
	pmbrainkAudioIdleRatioInfo->cluster_active_time = 0;
	pmbrainkAudioIdleRatioInfo->cluster_idle_time = 0;
	pmbrainkAudioIdleRatioInfo->cluster_pd_time = 0;
	pmbrainkAudioIdleRatioInfo->audio_hw_time = 0;

End:

	if (ddr_sr_pd_times_ptr != NULL)
		kfree(ddr_sr_pd_times_ptr);

	return ret;
}

#else
int mbraink_audio_getIdleRatioInfo(struct mbraink_audio_idleRatioInfo *pmbrainkAudioIdleRatioInfo)
{
	pr_info("%s: Do not support ioctl idle ratio info query.\n", __func__);
	return 0;
}

#endif //#if IS_ENABLED(CONFIG_MTK_SWPM_MODULE)

