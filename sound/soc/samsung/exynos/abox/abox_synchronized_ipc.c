/* sound/soc/samsung/abox/abox_synchronized_ipc.c
 *
 * ALSA SoC Audio Layer - Samsung Abox synchronized IPC driver
 *
 * Copyright (c) 2020 Samsung Electronics Co. Ltd.
  *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/kernel.h>
#include <linux/version.h>

/*#include <sound/abox_synchronized_ipc.h>*/
#if IS_ENABLED(CONFIG_SND_SOC_APS_ALGO)
#include <sound/ff_prot_spk.h>
#endif
#include "abox.h"
#include "abox_dma.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0))
#include <linux/time64.h>
#else
#include <linux/time.h>
#endif

#define AWINIC_IPC_VERSION	"v0.0.6"
#define AW_DSP_CMD_WRITE    (0x1001)
#define AW_DSP_CMD_READ     (0x1002)
#define AW_DSP_CMD_RECOVERY_DATA    (0x1003)

#define AW_DSP_RES_WRITE_SUCCESS 	(0x2001)
#define AW_DSP_RES_READ_SUCCESS  	(0x2002)
#define AW_DSP_RES_WRITE_FAILED		(0x2003)
#define AW_DSP_RES_READ_FAILED  	(0x2004)

#define AW_DSP_RES_BACKUP_SUCCESS   (0x2005)
#define AW_DSP_RES_BACKUP_FAILED    (0x2006)
#define AW_DSP_RES_RECOVERY_SUCCESS (0x2007)
#define AW_DSP_RES_RECOVERY_FAILED  (0x2008)

#define AW_DSP_SUCCESS	(0)
#define AW_DSP_FAILED	(-1)

#define VENDORID_AWINIC_ID	0x0825

#define TIMEOUT_MS 100
#define AW_MAX_SIZE		(740)  /* data_max_size = (int[188] - sizeof(aw_msg_hdr)) */
#define ALGO_PACKET_MEM_ADDR	(0x7002D800) /* reserved 6KB for spkamp_params.bin from 0x7002c000 */
#define AW_IPC_HEADER_SIZE	(12) /* 3 * sizeof(int) */

static DECLARE_WAIT_QUEUE_HEAD(wq_read);
static DECLARE_WAIT_QUEUE_HEAD(wq_write);

typedef struct state_data{
	int32_t interval_time;
	int64_t v_rms2_pre;
	int64_t temp_p_pre;
	int64_t vrms_pre;
	int32_t gain_ppc_pre;
	int32_t t_pre;
	int32_t t_now;
	int32_t p_now;
}state_data_t;

struct  abox_algo_packet {
	long long last_time;
	long long cur_time;
	state_data_t data[2];
};

struct abox_synchronized_data {
	struct device *dev;
	struct device *dev_abox;
	struct abox_data *abox_data;
};

static struct abox_synchronized_data *drv_data;

static struct abox_algo_packet aw_algo_packet;

char *aw_drv_read_buf = NULL;
char *aw_drv_write_buf = NULL;
char *shmem_buf = NULL;
struct ERAP_RAW_PARAM rd_tem_buf;

int abox_ipc_read_error = 0;
int abox_ipc_write_error = 0;
bool abox_ipc_read_avail = false;
bool abox_ipc_write_avail = false;
bool g_aw882xx_ipc_loaded = false;

#if IS_ENABLED(CONFIG_SND_SOC_APS_ALGO)
struct ff_prot_spk_data *ff_prot_spk_rd_data;
struct ff_prot_spk_data ff_prot_spk_rd_data_tmp;
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0))
static void aw_get_kernel_rtc_time(long long *rtc_ms)
{
	struct timespec64 tv;

	ktime_get_real_ts64(&tv);

	*rtc_ms = tv.tv_sec * 1000 + tv.tv_nsec / 1000000;
}
#else
static void aw_get_kernel_rtc_time(long long *rtc_ms)
{
	struct timeval tv;

	do_gettimeofday(&tv);

	*rtc_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
#endif

static void aw_clear_rtc_time(long long *rtc_ms)
{
	*rtc_ms = 0;
}

static void aw_get_interval_time(int *interval_time)
{
	long long time = 0;

	/* first playback or backup res failed*/
	if (aw_algo_packet.last_time == 0) {
		*interval_time = -1;
		return;
	}

	time = aw_algo_packet.cur_time - aw_algo_packet.last_time;
	if ((time > 0x7FFFFFFF) || (time <= 0)) {
		*interval_time = -1;
		return;
	}

	*interval_time = time / 100;
}

void aw882xx_abox_backup_algo_packet(unsigned int length)
{

	dev_dbg(drv_data->dev, "%s: enter\n", __func__);

	if (length > sizeof(aw_algo_packet.data)) {
		dev_err(drv_data->dev, "%s: algo_packet length overflow:%d\n", __func__, length);
		return;
	}

	aw_get_kernel_rtc_time(&aw_algo_packet.last_time);
	dev_dbg(drv_data->dev, "%s: cur time:%lld, last time:%lld\n", __func__,
		aw_algo_packet.cur_time, aw_algo_packet.last_time);

	shmem_buf = (char *)abox_iova_to_virt(drv_data->dev_abox, ALGO_PACKET_MEM_ADDR);

	memcpy(&aw_algo_packet.data[0], shmem_buf, length);
}

int aw882xx_abox_recover_algo_packet(void)
{
	ABOX_IPC_MSG msg;
	int ret = 0;
	struct IPC_ERAP_MSG *erap_msg = &msg.msg.erap;

	dev_dbg(drv_data->dev, "%s: enter\n", __func__);

	aw_get_kernel_rtc_time(&aw_algo_packet.cur_time);
	aw_get_interval_time(&aw_algo_packet.data[0].interval_time);
	aw_get_interval_time(&aw_algo_packet.data[1].interval_time);
	dev_dbg(drv_data->dev, "%s: cur time:%lld, last time:%lld, interval:%d\n", __func__,
		aw_algo_packet.cur_time, aw_algo_packet.last_time, aw_algo_packet.data[0].interval_time);

	msg.ipcid = IPC_ERAP;
	erap_msg->msgtype = REALTIME_EXTRA;
	erap_msg->param.raw.params[0] = VENDORID_AWINIC_ID;
	erap_msg->param.raw.params[1] = AW_DSP_CMD_RECOVERY_DATA;
	erap_msg->param.raw.params[2] = sizeof(aw_algo_packet.data);

	shmem_buf = (char *)abox_iova_to_virt(drv_data->dev_abox, ALGO_PACKET_MEM_ADDR);
	memcpy(shmem_buf, (char *)&aw_algo_packet.data[0], sizeof(aw_algo_packet.data));

	abox_ipc_write_avail = false;
	abox_ipc_write_error = AW_DSP_SUCCESS;

	ret = abox_request_ipc(drv_data->dev_abox, IPC_ERAP,
					&msg, sizeof(msg), 0, 0);
	if (ret < 0) {
		dev_err(drv_data->dev, "%s: abox_start_ipc_transaction is failed, error=%d\n", __func__, ret);
		return -1;
	}

	ret = wait_event_timeout(wq_write,
		abox_ipc_write_avail, msecs_to_jiffies(TIMEOUT_MS));

	if (!ret) {
		dev_err(drv_data->dev, "%s: wait_event timeout\n", __func__);
		return -1;
	}
	if (abox_ipc_write_error == AW_DSP_FAILED) {
		dev_err(drv_data->dev, "%s: abox_ipc_write_error = %d\n", __func__, abox_ipc_write_error);
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(aw882xx_abox_recover_algo_packet);

static void aw882xx_set_abox_ipc_loaded(bool enable)
{
	g_aw882xx_ipc_loaded = enable;
}

int aw882xx_abox_read(char *buf, int length)
{
	ABOX_IPC_MSG msg;
	int ret = 0;
	struct IPC_ERAP_MSG *erap_msg = &msg.msg.erap;

	if (!g_aw882xx_ipc_loaded) {
		dev_err(drv_data->dev, "%s: aw882xx ipc not loaded\n", __func__);
		return -1;
	}

	dev_dbg(drv_data->dev, "%s: length = %d\n", __func__, length);

	aw_drv_read_buf = (char *)buf;

	msg.ipcid = IPC_ERAP;
	erap_msg->msgtype = REALTIME_EXTRA;
	erap_msg->param.raw.params[0] = VENDORID_AWINIC_ID;
	erap_msg->param.raw.params[1] = AW_DSP_CMD_READ;
	erap_msg->param.raw.params[2] = length;
	abox_ipc_read_avail = false;
	abox_ipc_read_error = AW_DSP_SUCCESS;
	ret = abox_request_ipc(drv_data->dev_abox, IPC_ERAP,
					&msg, sizeof(msg), 0, 0);
	if (ret < 0) {
		dev_err(drv_data->dev, "%s: abox_start_ipc_transaction is failed, error=%d\n", __func__, ret);
		return -1;
	}

	ret = wait_event_timeout(wq_read,
			abox_ipc_read_avail, msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		dev_err(drv_data->dev, "%s: wait_event timeout\n", __func__);
		return -1;
	}

	if (abox_ipc_read_error == AW_DSP_FAILED) {
		dev_err(drv_data->dev, "%s: error = %d\n", __func__, abox_ipc_read_error);
		return -1;
	}

	memcpy((void *)aw_drv_read_buf, (void *)&rd_tem_buf.params[0], min(length, AW_MAX_SIZE));
	dev_dbg(drv_data->dev, "%s: read length = %d done\n", __func__, min(length, AW_MAX_SIZE));

	return 0;
}
EXPORT_SYMBOL_GPL(aw882xx_abox_read);

int aw882xx_abox_write(const char *buf, int length)
{
	ABOX_IPC_MSG msg;
	int ret = 0;
	struct IPC_ERAP_MSG *erap_msg = &msg.msg.erap;

	if (!g_aw882xx_ipc_loaded) {
		dev_err(drv_data->dev, "%s: aw882xx ipc not loaded\n", __func__);
		return -1;
	}

	if (length > AW_MAX_SIZE) {
		dev_err(drv_data->dev, "%s: length overflow= %d\n", __func__, length);
		return -1;
	}

	dev_dbg(drv_data->dev, "%s: length = %d\n", __func__, length);

	aw_drv_write_buf = (char *)buf;

	msg.ipcid = IPC_ERAP;
	erap_msg->msgtype = REALTIME_EXTRA;
	erap_msg->param.raw.params[0] = VENDORID_AWINIC_ID;
	erap_msg->param.raw.params[1] = AW_DSP_CMD_WRITE;
	erap_msg->param.raw.params[2] = length;

	memcpy(&erap_msg->param.raw.params[3], aw_drv_write_buf, length);
	abox_ipc_write_avail = false;
	abox_ipc_write_error = AW_DSP_SUCCESS;

	ret = abox_request_ipc(drv_data->dev_abox, IPC_ERAP,
					&msg, sizeof(msg), 0, 0);
	if (ret < 0) {
		dev_err(drv_data->dev, "%s: abox_start_ipc_transaction is failed, error=%d\n", __func__, ret);
		return -1;
	}

	ret = wait_event_timeout(wq_write,
		abox_ipc_write_avail, msecs_to_jiffies(TIMEOUT_MS));

	if (!ret) {
		dev_err(drv_data->dev, "%s: wait_event timeout\n", __func__);
		return -1;
	}
	if (abox_ipc_write_error == AW_DSP_FAILED) {
		dev_err(drv_data->dev, "%s: abox_ipc_write_error = %d\n", __func__, abox_ipc_write_error);
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(aw882xx_abox_write);

#if IS_ENABLED(CONFIG_SND_SOC_APS_ALGO)
int ff_prot_spk_read(void *prm_data, int offset, int size)
{
	ABOX_IPC_MSG msg;
	int ret = 0;
	struct IPC_ERAP_MSG *erap_msg = &msg.msg.erap;

	ff_prot_spk_rd_data = (struct ff_prot_spk_data *)prm_data;

	msg.ipcid = IPC_ERAP;
	erap_msg->msgtype = REALTIME_EXTRA;
	erap_msg->param.raw.params[0] = VENDOR_IRONDEVICE_ID;
	erap_msg->param.raw.params[1] = RD_DATA;
	erap_msg->param.raw.params[2] = offset;
	erap_msg->param.raw.params[3] = size;

	dev_dbg(drv_data->dev, "%s\n", __func__);
	abox_ipc_read_avail = false;

	if(!drv_data) {
		dev_err(drv_data->dev, "%s: data is NULL\n", __func__);
		return ret;
	}

	ret = abox_request_ipc(drv_data->dev_abox, IPC_ERAP,
					 &msg, sizeof(msg), 0, 0);
	if (ret) {
		dev_err(drv_data->dev, "%s: abox_request_ipc is failed: %d\n",
			__func__, ret);
		return ret;
	}

	ret = wait_event_timeout(wq_read,
		abox_ipc_read_avail != false, msecs_to_jiffies(TIMEOUT_MS));
	if (!ret)
		dev_err(drv_data->dev, "%s: wait_event timeout\n", __func__);
	else if (ret < 0)
		dev_err(drv_data->dev, "%s: wait_event err(%d)\n",
			__func__, ret);
	else
		memcpy(&ff_prot_spk_rd_data->payload[0],
				&ff_prot_spk_rd_data_tmp.payload[0], size);

	return ret;
}
EXPORT_SYMBOL_GPL(ff_prot_spk_read);

int ff_prot_spk_write(void *prm_data, int offset, int size)
{
	ABOX_IPC_MSG msg;
	int ret = 0;
	struct IPC_ERAP_MSG *erap_msg = &msg.msg.erap;

	msg.ipcid = IPC_ERAP;
	erap_msg->msgtype = REALTIME_EXTRA;
	erap_msg->param.raw.params[0] = VENDOR_IRONDEVICE_ID;
	erap_msg->param.raw.params[1] = WR_DATA;
	erap_msg->param.raw.params[2] = offset;
	erap_msg->param.raw.params[3] = size;

	memcpy(&erap_msg->param.raw.params[4], prm_data,
		min((sizeof(uint32_t) * size), sizeof(erap_msg->param.raw)));

	dev_dbg(drv_data->dev, "%s\n", __func__);
	abox_ipc_write_avail = false;

	if(!drv_data) {
		dev_err(drv_data->dev, "%s: data is NULL\n", __func__);
		return ret;
	}

	ret = abox_request_ipc(drv_data->dev_abox, IPC_ERAP,
			&msg, sizeof(msg), 0, 0);
	if (ret) {
		dev_err(drv_data->dev, "%s: abox_request_ipc is failed: %d\n",
			__func__, ret);
		return ret;
	}

	ret = wait_event_timeout(wq_write,
		abox_ipc_write_avail != false, msecs_to_jiffies(TIMEOUT_MS));
	if (!ret)
		dev_err(drv_data->dev, "%s: wait_event timeout\n", __func__);

	return ret;
}
EXPORT_SYMBOL_GPL(ff_prot_spk_write);
#endif

static irqreturn_t abox_synchronized_ipc_handler(int irq,
					void *dev_id, ABOX_IPC_MSG *msg)
{
	struct IPC_ERAP_MSG *erap_msg = &msg->msg.erap;
	unsigned int vendor_id = erap_msg->param.raw.params[0];
	unsigned int res_id = erap_msg->param.raw.params[1];
	int size = erap_msg->param.raw.params[2];
	irqreturn_t ret = IRQ_HANDLED;

	if (irq != IPC_ERAP) {
		dev_err(drv_data->dev, "%s: unknown irq = %d\n",
			__func__, irq);
		return IRQ_NONE;
	}

	if (erap_msg->msgtype != REALTIME_EXTRA) { 
		dev_err(drv_data->dev, "%s: unknown message type = %d\n",
			__func__, erap_msg->msgtype);
		return IRQ_NONE;
	}

	switch (vendor_id) {
#if IS_ENABLED(CONFIG_SND_SOC_APS_ALGO)
	case VENDOR_IRONDEVICE_ID:
		switch(res_id) {
		case RD_DATA:
			memcpy(&ff_prot_spk_rd_data_tmp.payload[0], &erap_msg->param.raw.params[4],
				min(sizeof(struct ff_prot_spk_data), sizeof(erap_msg->param.raw)));
			abox_ipc_read_avail = true;
			dev_dbg(drv_data->dev, "%s: read_avail after parital read[%d]\n",
				__func__, abox_ipc_read_avail);

			if (abox_ipc_read_avail && waitqueue_active(&wq_read))
				wake_up(&wq_read);
		break;
		case WR_DATA:
			abox_ipc_write_avail = true;
			dev_dbg(drv_data->dev, "%s: write_avail after parital read[%d]\n",
				__func__, abox_ipc_write_avail);
			if (abox_ipc_write_avail && waitqueue_active(&wq_write))
				wake_up(&wq_write);
		break;
		default:
			dev_dbg(drv_data->dev, "%s: Invalid callback, %d\n", __func__,
				erap_msg->param.raw.params[1]);
			ret = IRQ_NONE;
		break;
		}
	break;
#endif
	case VENDORID_AWINIC_ID:
		switch(res_id) {
		case AW_DSP_RES_READ_SUCCESS:
			size -=AW_IPC_HEADER_SIZE;
			memcpy((void *)&rd_tem_buf.params[0], (void *)&erap_msg->param.raw.params[3],
				min(size, AW_MAX_SIZE));
			abox_ipc_read_avail = true;
			abox_ipc_read_error = AW_DSP_SUCCESS;
			dev_dbg(drv_data->dev, "%s: AW_DSP_CMD_READ DONE size= %d\n", __func__, size);
			if (waitqueue_active(&wq_read))
				wake_up(&wq_read);
		break;
		case AW_DSP_RES_READ_FAILED:
			abox_ipc_read_avail = true;
			abox_ipc_read_error = AW_DSP_FAILED;
			dev_err(drv_data->dev, "%s: aw_dsp_read() AW_DSP_RES_READ_FAILED\n", __func__);
			if (waitqueue_active(&wq_read))
				wake_up(&wq_read);
		break;
		case AW_DSP_RES_BACKUP_SUCCESS:
			size -=AW_IPC_HEADER_SIZE;
			aw882xx_abox_backup_algo_packet(size);
			dev_dbg(drv_data->dev, "%s: AW_DSP_RES_BACKUP_SUCCESS, size:%d\n", __func__, size);
		break;
		case AW_DSP_RES_BACKUP_FAILED:
			dev_err(drv_data->dev, "%s: AW_DSP_RES_BACKUP_FAILED\n", __func__);
			aw_clear_rtc_time(&aw_algo_packet.last_time);
		break;
		case AW_DSP_RES_WRITE_SUCCESS:
			abox_ipc_write_avail = true;
			abox_ipc_write_error = AW_DSP_SUCCESS;
			dev_dbg(drv_data->dev, "%s: AW_DSP_CMD_WRITE done\n", __func__);
			if (waitqueue_active(&wq_write))
				wake_up(&wq_write);
		break;
		case AW_DSP_RES_WRITE_FAILED:
			abox_ipc_write_avail = true;
			abox_ipc_write_error = AW_DSP_FAILED;
			dev_err(drv_data->dev, "%s: aw_dsp_write() AW_DSP_RES_WRITE_FAILED\n", __func__);
			if (waitqueue_active(&wq_write))
				wake_up(&wq_write);
		break;
		case AW_DSP_RES_RECOVERY_SUCCESS:
			abox_ipc_write_avail = true;
			abox_ipc_write_error = AW_DSP_SUCCESS;
			dev_dbg(drv_data->dev, "%s: AW_DSP_RES_RECOVERY_SUCCESS\n", __func__);
			if (waitqueue_active(&wq_write))
				wake_up(&wq_write);
		break;
		case AW_DSP_RES_RECOVERY_FAILED:
			abox_ipc_write_avail = true;
			abox_ipc_write_error = AW_DSP_FAILED;
			dev_err(drv_data->dev, "%s: AW_DSP_RES_RECOVERY_FAILED\n", __func__);
			if (waitqueue_active(&wq_write))
				wake_up(&wq_write);
		break;
		default:
			dev_err(drv_data->dev, "%s: unknown response type, RES_ID = 0x%x, size=%d\n",
				__func__, res_id, size);
			ret = IRQ_NONE;
		break;
		}
	break;
	default:
		dev_err(drv_data->dev, "%s: unsupported vendor_id = %d\n",
			__func__, vendor_id);
		ret = IRQ_NONE;
	break;
	}

	return ret;
}

static int samsung_abox_synchronized_ipc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *np_abox;
	struct platform_device *pdev_abox;

	dev_dbg(dev, "%s\n", __func__);

	drv_data = devm_kzalloc(dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data) {
		dev_err(dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, drv_data);

	np_abox = of_parse_phandle(np, "abox", 0);
	if (!np_abox) {
		dev_err(dev, "Failed to get abox device node\n");
		return -EPROBE_DEFER;
	}
	drv_data->dev = dev;
	pdev_abox = of_find_device_by_node(np_abox);
	drv_data->dev_abox = &pdev_abox->dev;
	if (!drv_data->dev_abox) {
		dev_err(dev, "Failed to get abox platform device\n");
		return -EPROBE_DEFER;
	}
	drv_data->abox_data = platform_get_drvdata(pdev_abox);

	abox_register_ipc_handler(drv_data->dev_abox, IPC_ERAP,
				abox_synchronized_ipc_handler, pdev);

	aw882xx_set_abox_ipc_loaded(true);

#if IS_ENABLED(CONFIG_SND_SOC_APS_ALGO)
	sma_ext_register((sma_send_msg_t)ff_prot_spk_write,
			(sma_read_msg_t)ff_prot_spk_read);
#endif

	return 0;
}

static int samsung_abox_synchronized_ipc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_dbg(dev, "%s\n", __func__);

	return 0;
}

static const struct of_device_id samsung_abox_synchronized_ipc_match[] = {
	{
		.compatible = "samsung,abox-synchronized-ipc",
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_abox_synchronized_ipc_match);

static struct platform_driver samsung_abox_synchronized_ipc_driver = {
	.probe  = samsung_abox_synchronized_ipc_probe,
	.remove = samsung_abox_synchronized_ipc_remove,
	.driver = {
		.name = "abox-synchronized-ipc",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_abox_synchronized_ipc_match),
	},
};
module_platform_driver(samsung_abox_synchronized_ipc_driver);

/* Module information */
MODULE_DESCRIPTION("Samsung ASoC A-Box Synchronized IPC Driver");
MODULE_ALIAS("platform:abox-synchronized-ipc");
MODULE_LICENSE("GPL");
