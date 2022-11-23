/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *
 * Boojin Kim <boojin.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/iio/iio.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/random.h>
#include <linux/rtc.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/timekeeping.h>
#include <linux/of_gpio.h>
#include <linux/fcntl.h>
#include <uapi/linux/sched/types.h>

#ifdef CONFIG_EXYNOS_ITMON
#include <soc/samsung/exynos-itmon.h>
#endif

#ifdef CONFIG_CHRE_SENSORHUB_HAL
#include "main.h"
#endif

#ifdef CONFIG_SENSORS_SSP
#include "../../sensorhub/ssp_platform.h"
#elif defined(CONFIG_SHUB)
#include "../../sensorhub/vendor/shub_helper.h"
#endif

#include "bl.h"
#include "comms.h"
#include "chub.h"
#include "chub_ipc.h"
#include "chub_dbg.h"
#include "../../soc/samsung/cal-if/pmucal_shub.h"

#define WAIT_TIMEOUT_MS (1000)
enum { CHUB_ON, CHUB_OFF };
enum { C2A_ON, C2A_OFF };

const char *os_image[SENSOR_VARIATION] = {
	"os.checked_0.bin",
	"os.checked_1.bin",
	"os.checked_2.bin",
	"os.checked_3.bin",
	"os.checked_4.bin",
	"os.checked_5.bin",
	"os.checked_6.bin",
	"os.checked_7.bin",
	"os.checked_8.bin",
};

static DEFINE_MUTEX(reset_mutex);
static DEFINE_MUTEX(pmu_shutdown_mutex);
static DEFINE_MUTEX(log_mutex);
static DEFINE_MUTEX(wt_mutex);

void chub_wake_event(struct chub_alive *event)
{
	atomic_set(&event->flag, 1);
	wake_up_interruptible_sync(&event->event);
}

static int chub_wait_event(struct chub_alive *event, int timeout)
{
	atomic_set(&event->flag, 0);
	return wait_event_interruptible_timeout(event->event,
						 atomic_read(&event->flag),
						 msecs_to_jiffies(timeout));
}

#if defined(CONFIG_SENSORS_SSP) || defined(CONFIG_SHUB)
int contexthub_get_token(struct contexthub_ipc_info *ipc)
#else
static int contexthub_get_token(struct contexthub_ipc_info *ipc)
#endif
{
	if (atomic_read(&ipc->in_reset))
		return -EINVAL;

	atomic_inc(&ipc->in_use_ipc);
	return 0;
}

#if defined(CONFIG_SENSORS_SSP) || defined(CONFIG_SHUB)
void contexthub_put_token(struct contexthub_ipc_info *ipc)
#else
static void contexthub_put_token(struct contexthub_ipc_info *ipc)
#endif
{
	atomic_dec(&ipc->in_use_ipc);
}

/* host interface functions */
int contexthub_is_run(struct contexthub_ipc_info *ipc)
{
	if (!ipc->powermode)
		return 1;

#ifdef CONFIG_CHRE_SENSORHUB_HAL
	return nanohub_irq1_fired(ipc->data);
#else
	return 1;
#endif
}

/* request contexthub to host driver */
int contexthub_request(struct contexthub_ipc_info *ipc)
{
	if (!ipc->powermode)
		return 0;

#ifdef CONFIG_CHRE_SENSORHUB_HAL
	return request_wakeup_timeout(ipc->data, WAIT_TIMEOUT_MS);
#else
	return 0;
#endif
}

/* rlease contexthub to host driver */
void contexthub_release(struct contexthub_ipc_info *ipc)
{
	if (!ipc->powermode)
		return;

#ifdef CONFIG_CHRE_SENSORHUB_HAL
	release_wakeup(ipc->data);
#endif
}

static inline void contexthub_notify_host(struct contexthub_ipc_info *ipc)
{
#ifdef CONFIG_CHRE_SENSORHUB_HAL
	nanohub_handle_irq1(ipc->data);
#else
	/* TODO */
#endif
}

#ifdef CONFIG_CHRE_SENSORHUB_HAL
/* by nanohub kernel RxBufStruct. packet header is 10 + 2 bytes to align */
struct rxbuf {
	u8 pad;
	u8 pre_preamble;
	u8 buf[PACKET_SIZE_MAX];
	u8 post_preamble;
};

static int nanohub_mailbox_open(void *data)
{
	return 0;
}

static void nanohub_mailbox_close(void *data)
{
	(void)data;
}

static int nanohub_mailbox_write(void *data, uint8_t *tx, int length,
				 int timeout)
{
	struct nanohub_data *ipc = data;

	return contexthub_ipc_write(ipc->pdata->mailbox_client, tx, length, timeout);
}

static int nanohub_mailbox_read(void *data, uint8_t *rx, int max_length,
				int timeout)
{
	struct nanohub_data *ipc = data;

	return contexthub_ipc_read(ipc->pdata->mailbox_client, rx, max_length, timeout);
}

void nanohub_mailbox_comms_init(struct nanohub_comms *comms)
{
	comms->seq = 1;
	comms->timeout_write = 544;
	comms->timeout_ack = 272;
	comms->timeout_reply = 512;
	comms->open = nanohub_mailbox_open;
	comms->close = nanohub_mailbox_close;
	comms->write = nanohub_mailbox_write;
	comms->read = nanohub_mailbox_read;
}
#endif

static int contexthub_read_process(uint8_t *rx, u8 *raw_rx, u32 size)
{
#ifdef CONFIG_CHRE_SENSORHUB_HAL
	struct rxbuf *rxstruct;
	struct nanohub_packet *packet;

	rxstruct = (struct rxbuf *)raw_rx;
	packet = (struct nanohub_packet *)&rxstruct->pre_preamble;
	memcpy_fromio(rx, (void *)packet, size);

	return NANOHUB_PACKET_SIZE(packet->len);
#else
	memcpy_fromio(rx, (void *)raw_rx, size);
	return size;
#endif
}

static int contexthub_ipc_drv_init(struct contexthub_ipc_info *chub)
{
	struct device *chub_dev = chub->dev;
	int ret = 0;

	chub->ipc_map = ipc_get_chub_map();
	if (!chub->ipc_map)
		return -EINVAL;

	/* init debug-log */
	/* HACK for clang */
	chub->ipc_map->logbuf.logbuf.eq = 0;
	chub->ipc_map->logbuf.logbuf.dq = 0;
	chub->fw_log = log_register_buffer(chub_dev, 0,
					   (void *)&chub->ipc_map->logbuf.logbuf,
					   "fw", 1);
	if (!chub->fw_log)
		return -EINVAL;

	if (chub->irq_pin_len) {
		int i;

		for (i = 0; i < chub->irq_pin_len; i++) {
			u32 irq = gpio_to_irq(chub->irq_pins[i]);

			disable_irq_nosync(irq);
			dev_info(chub_dev, "%s: %d irq (pin:%d) is for chub. disable it\n",
				__func__, irq, chub->irq_pins[i]);
		}
	}

#ifdef LOWLEVEL_DEBUG
	chub->dd_log_buffer = vmalloc(SZ_256K + sizeof(struct LOG_BUFFER *));
	chub->dd_log_buffer->index_reader = 0;
	chub->dd_log_buffer->index_writer = 0;
	chub->dd_log_buffer->size = SZ_256K;
	chub->dd_log =
	    log_register_buffer(chub_dev, 1, chub->dd_log_buffer, "dd", 0);
#endif
	ret = chub_dbg_init(chub, chub->chub_rt_log.buffer, chub->chub_rt_log.buffer_size);
	if (ret)
		dev_err(chub_dev, "%s: fails in chub_dbg_init. ret:%d\n", __func__, ret);

	return ret;
}

#ifdef PACKET_LOW_DEBUG
static void debug_dumpbuf(unsigned char *buf, int len)
{
	print_hex_dump(KERN_CONT, "", DUMP_PREFIX_OFFSET, 16, 1, buf, len,
		       false);
}
#endif

static inline int get_recv_channel(struct recv_ctrl *recv)
{
	int i;
	unsigned long min_order = 0;
	int min_order_evt = INVAL_CHANNEL;

	for (i = 0; i < IPC_BUF_NUM; i++) {
		if (recv->container[i]) {
			if (!min_order) {
				min_order = recv->container[i];
				min_order_evt = i;
			} else if (recv->container[i] < min_order) {
				min_order = recv->container[i];
				min_order_evt = i;
			}
		}
	}

	if (min_order_evt != INVAL_CHANNEL)
		recv->container[min_order_evt] = 0;

	return min_order_evt;
}

/* simple alive check function : don't use ipc map */
static bool contexthub_lowlevel_alive(struct contexthub_ipc_info *ipc)
{
	int ret;

	atomic_set(&ipc->chub_alive_lock.flag, 0);
	ipc_hw_gen_interrupt(AP, IRQ_EVT_CHUB_ALIVE);
	ret = chub_wait_event(&ipc->chub_alive_lock, 200);
	dev_info(ipc->dev, "%s done: ret:%d\n", __func__, ret);
	return atomic_read(&ipc->chub_alive_lock.flag);
}

/* contexhub slient reset support */
void contexthub_handle_debug(struct contexthub_ipc_info *ipc,
	enum chub_err_type err)
{
        int thold = (err < CHUB_ERR_CRITICAL) ? 1 :
                   ((err < CHUB_ERR_MAJER) ? CHUB_RESET_THOLD : CHUB_RESET_THOLD_MINOR);

        /* update error count */
        ipc->err_cnt[err]++;

        dev_info(ipc->dev, "%s: err:%d(cnt:%d), cur_err:%d, thold:%d, chub_status:%d\n",
                __func__, err, ipc->err_cnt[err], ipc->cur_err, thold, atomic_read(&ipc->chub_status));

        /* set chub_status to CHUB_ST_ERR. request silent reset */
        if ((ipc->err_cnt[err] >= thold) && (atomic_read(&ipc->chub_status) != CHUB_ST_ERR)) {
                atomic_set(&ipc->chub_status, CHUB_ST_ERR);
                dev_info(ipc->dev, "%s: err:%d(cnt:%d), enter error status\n",
                        __func__, err, ipc->err_cnt[err]);
                /* handle err */
                ipc->cur_err = err;
                schedule_work(&ipc->debug_work);
        }
}

static void contexthub_select_os(struct contexthub_ipc_info *ipc)
{
	int trycnt = 0;
	u8 val = (u8) ipc_read_val(AP);
	if (!val) {
		dev_warn(ipc->dev, "%s os number is invalid\n");
		val = 1;
	}
	ipc->sel_os = true;

	strcpy(ipc->os_name, os_image[val]);
	dev_info(ipc->dev, "%s selected os_name = %s\n", __func__, ipc->os_name);

	log_flush_all();
	contexthub_download_image(ipc, IPC_REG_OS);
	ipc_hw_write_shared_reg(AP, ipc->os_load, SR_BOOT_MODE);
	ipc_write_val(AP, READY_TO_GO);
	do {
		msleep(WAIT_CHUB_MS);
		contexthub_ipc_write_event(ipc, MAILBOX_EVT_CHUB_ALIVE);
		if (++trycnt > 10)
			break;
	} while ((atomic_read(&ipc->chub_status) != CHUB_ST_RUN));

	if (atomic_read(&ipc->chub_status) == CHUB_ST_RUN)
		dev_info(ipc->dev, "%s done. contexthub status is %d\n",
				__func__, atomic_read(&ipc->chub_status));
	else {
		dev_warn(ipc->dev, "%s failed. contexthub status is %d\n",
				__func__, atomic_read(&ipc->chub_status));
		atomic_set(&ipc->chub_status, CHUB_ST_NO_RESPONSE);
		contexthub_handle_debug(ipc, CHUB_ERR_CHUB_NO_RESPONSE);
	}
	dev_info(ipc->dev, "%s done: wakeup interrupt\n", __func__);
	chub_wake_event(&ipc->poweron_lock);
}

static DEFINE_MUTEX(chub_err_mutex);
static void handle_debug_work_func(struct work_struct *work)
{
	struct contexthub_ipc_info *ipc =
	    container_of(work, struct contexthub_ipc_info, debug_work);

	/* 1st work: os select on booting */
	if ((atomic_read(&ipc->chub_status) == CHUB_ST_POWER_ON) && (ipc->sel_os == false)) {
		contexthub_select_os(ipc);
		return;
	}

	mutex_lock(&chub_err_mutex);
	/* 2nd work: slient reset */
	if (ipc->cur_err) {
		int ret;
		int err = ipc->cur_err;

		ipc->cur_err = 0;
		dev_info(ipc->dev, "%s: request silent reset. err:%d, status:%d, in-reset:%d\n",
			__func__, err, __raw_readl(&ipc->chub_status), __raw_readl(&ipc->in_reset));
		ret = contexthub_reset(ipc, 1, err);
		if (ret) {
			dev_warn(ipc->dev, "%s: fails to reset:%d. status:%d\n",
				__func__, ret, __raw_readl(&ipc->chub_status));
		} else {
			ipc->cur_err = 0;
			dev_info(ipc->dev, "%s: chub reset! should be recovery\n",
				__func__);
		}
	}
	mutex_unlock(&chub_err_mutex);
}

void contexthub_print_rtlog(struct contexthub_ipc_info *ipc, bool loop)
{
	if (!atomic_read(&ipc->log_work_active)) {
		if (contexthub_get_token(ipc)) {
			dev_warn(ipc->dev, "%s: get token\n", __func__);
			return;
		}
		if (ipc_logbuf_outprint(&ipc->chub_rt_log, loop))
			chub_dbg_dump_hw(ipc, CHUB_ERR_NANOHUB_LOG);
		contexthub_put_token(ipc);
	}
}

static void handle_log_work_func(struct work_struct *work)
{
	struct contexthub_ipc_info *ipc =
	    container_of(work, struct contexthub_ipc_info, log_work);
	int retrycnt = 0;

retry:
	if (contexthub_get_token(ipc)) {
		chub_wait_event(&ipc->reset_lock, WAIT_TIMEOUT_MS * 2);
		if (!retrycnt) {
			retrycnt++;
			goto retry;
		}
		atomic_set(&ipc->log_work_active, 0);
		return;
	}
	ipc_logbuf_flush_on(1);
	mutex_lock(&log_mutex);
	if (ipc_logbuf_outprint(&ipc->chub_rt_log, 100))
		chub_dbg_dump_hw(ipc, CHUB_ERR_NANOHUB_LOG);
	mutex_unlock(&log_mutex);
	ipc_logbuf_flush_on(0);
	contexthub_put_token(ipc);
	atomic_set(&ipc->log_work_active, 0);
}

static inline void clear_err_cnt(struct contexthub_ipc_info *ipc, enum chub_err_type err)
{
	if (ipc->err_cnt[err])
		ipc->err_cnt[err] = 0;
}

int contexthub_ipc_read(struct contexthub_ipc_info *ipc, uint8_t *rx, int max_length,
				int timeout)
{
	unsigned long flag;
	int size = 0;
	int ret = 0;
	void *rxbuf;
	u64 time = 0; /* for debug */

	if (__raw_readl(&ipc->chub_status) != CHUB_ST_RUN) {
		dev_warn(ipc->dev, "%s: chub isn't run:%d\n",
				__func__, __raw_readl(&ipc->chub_status));
		contexthub_handle_debug(ipc, CHUB_ERR_CHUB_ST_ERR);
		return 0;
	} else {
		clear_err_cnt(ipc, CHUB_ERR_CHUB_ST_ERR);
	}

	if (!atomic_read(&ipc->read_lock.cnt)) {
		time = sched_clock();

		spin_lock_irqsave(&ipc->read_lock.event.lock, flag);
		atomic_inc(&ipc->read_lock.flag);
		ret =
			wait_event_interruptible_timeout_locked(ipc->read_lock.event,
								atomic_read(&ipc->read_lock.cnt),
								msecs_to_jiffies(timeout));
		atomic_dec(&ipc->read_lock.flag);
		spin_unlock_irqrestore(&ipc->read_lock.event.lock, flag);
		if (ret < 0)
			dev_warn(ipc->dev,
				 "fails to get read ret:%d timeout:%d\n", ret, timeout);
	}

	if (contexthub_get_token(ipc)) {
		dev_warn(ipc->dev, "no-active: read fails\n");
		return 0;
	}

	if (atomic_read(&ipc->read_lock.cnt)) {
		rxbuf = ipc_read_data(IPC_DATA_C2A, &size);
		if (size > 0)
			ret = contexthub_read_process(rx, rxbuf, size);
		atomic_dec(&ipc->read_lock.cnt);
	} else {
		if (ipc->err_cnt[CHUB_ERR_READ_FAIL])
			dev_info(ipc->dev, "%s: read timeout(%d): c2aq_cnt:%d, recv_cnt:%d during %lld ns\n",
				__func__, ipc->err_cnt[CHUB_ERR_READ_FAIL],
				ipc_get_data_cnt(IPC_DATA_C2A), atomic_read(&ipc->read_lock.cnt),
				sched_clock() - time);
		if (ipc_get_data_cnt(IPC_DATA_C2A)) {
			rxbuf = ipc_read_data(IPC_DATA_C2A, &size);
			if (size > 0)
				ret = contexthub_read_process(rx, rxbuf, size);
		} else {
			ret = -EINVAL;
		}
	}
	if (ret < 0) {
		if (ipc->err_cnt[CHUB_ERR_READ_FAIL]) {
			pr_err("%s: fails to read data: ret:%d, len:%d errcnt:%d\n",
				__func__, ret, ipc->err_cnt[CHUB_ERR_READ_FAIL]);
			ipc_dump();
			contexthub_handle_debug(ipc, CHUB_ERR_READ_FAIL);
		} else
			ipc->err_cnt[CHUB_ERR_READ_FAIL]++;
	} else {
		clear_err_cnt(ipc, CHUB_ERR_READ_FAIL);
	}
	contexthub_put_token(ipc);
	return ret;
}

int contexthub_ipc_write(struct contexthub_ipc_info *ipc,
				uint8_t *tx, int length, int timeout)
{
	int ret;

	if (__raw_readl(&ipc->chub_status) != CHUB_ST_RUN) {
		dev_warn(ipc->dev, "%s: chub isn't run:%d\n",
				__func__, __raw_readl(&ipc->chub_status));
		contexthub_handle_debug(ipc, CHUB_ERR_CHUB_ST_ERR);
		return 0;
	}  else {
		clear_err_cnt(ipc, CHUB_ERR_CHUB_ST_ERR);
	}

	if (contexthub_get_token(ipc)) {
		dev_warn(ipc->dev, "no-active: write fails\n");
		return 0;
	}

	mutex_lock(&wt_mutex);
	ret = ipc_write_data(IPC_DATA_A2C, tx, (u16)length);
	mutex_unlock(&wt_mutex);
	contexthub_put_token(ipc);
	if (ret) {
		pr_err("%s: fails to write data: ret:%d, len:%d errcnt:%d\n",
			__func__, ret, length, ipc->err_cnt[CHUB_ERR_WRITE_FAIL]);
		contexthub_handle_debug(ipc, CHUB_ERR_WRITE_FAIL);
		length = 0;
	} else {
		clear_err_cnt(ipc, CHUB_ERR_WRITE_FAIL);
	}
	return length;
}

static void check_rtc_time(void)
{
	struct rtc_device *chub_rtc = rtc_class_open(CONFIG_RTC_SYSTOHC_DEVICE);
	struct rtc_device *ap_rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	struct rtc_time chub_tm, ap_tm;
	time64_t chub_t, ap_t;

	rtc_read_time(ap_rtc, &chub_tm);
	rtc_read_time(chub_rtc, &ap_tm);

	chub_t = rtc_tm_sub(&chub_tm, &ap_tm);

	if (chub_t) {
		pr_info("nanohub %s: diff_time: %llu\n", __func__, chub_t);
		rtc_set_time(chub_rtc, &ap_tm);
	};

	chub_t = rtc_tm_to_time64(&chub_tm);
	ap_t = rtc_tm_to_time64(&ap_tm);
}

static int contexthub_hw_reset(struct contexthub_ipc_info *ipc,
				 enum mailbox_event event)
{
#if defined(CONFIG_SOC_EXYNOS9610)
	u32 val;
#endif
	int trycnt = 0;
	int ret = 0;
	int i;
	bool first_poweron = false;

	dev_info(ipc->dev, "%s. status:%d\n",
		__func__, __raw_readl(&ipc->chub_status));

	/* clear ipc value */
	atomic_set(&ipc->wakeup_chub, CHUB_OFF);
	atomic_set(&ipc->irq1_apInt, C2A_OFF);
	atomic_set(&ipc->read_lock.cnt, 0);
	atomic_set(&ipc->read_lock.flag, 0);
	atomic_set(&ipc->log_work_active, 0);

	/* chub err init */
	for (i = 0; i < CHUB_ERR_COMMS; i++)
		ipc->err_cnt[i] = 0;

	ipc_hw_write_shared_reg(AP, ipc->os_load, SR_BOOT_MODE);
	ipc_set_chub_clk((u32)ipc->clkrate);
	ipc->chub_rt_log.loglevel = CHUB_RT_LOG_DUMP_PRT;
	ipc_set_chub_bootmode(BOOTMODE_COLD, ipc->chub_rt_log.loglevel);
	switch (event) {
	case MAILBOX_EVT_POWER_ON:
#ifdef NEED_TO_RTC_SYNC
		check_rtc_time();
#endif
		if (atomic_read(&ipc->chub_status) == CHUB_ST_NO_POWER) {
			if (ipc->sel_os == false)
				first_poweron = true;

			atomic_set(&ipc->chub_status, CHUB_ST_POWER_ON);

			/* enable Dump gpr */
			IPC_HW_WRITE_DUMPGPR_CTRL(ipc->chub_dumpgpr, 0x1);

#if defined(CONFIG_SOC_EXYNOS9610)
			/* cmu cm4 clock - gating */
			val = __raw_readl(ipc->cmu_chub_qch +
					REG_QCH_CON_CM4_SHUB_QCH);
			val &= ~(IGNORE_FORCE_PM_EN | CLOCK_REQ | ENABLE);
			__raw_writel((val | IGNORE_FORCE_PM_EN),
				     ipc->cmu_chub_qch +
				     REG_QCH_CON_CM4_SHUB_QCH);
#endif
			/* pmu reset-release on CHUB */
			ret = pmucal_shub_reset_release();

#if defined(CONFIG_SOC_EXYNOS9610)
			/* cmu cm4 clock - release */
			val = __raw_readl(ipc->cmu_chub_qch +
					REG_QCH_CON_CM4_SHUB_QCH);
			val &= ~(IGNORE_FORCE_PM_EN | CLOCK_REQ | ENABLE);
			__raw_writel((val | IGNORE_FORCE_PM_EN | CLOCK_REQ),
				     ipc->cmu_chub_qch +
				    REG_QCH_CON_CM4_SHUB_QCH);

			val = __raw_readl(ipc->cmu_chub_qch +
					REG_QCH_CON_CM4_SHUB_QCH);
			val &= ~(IGNORE_FORCE_PM_EN | CLOCK_REQ | ENABLE);
			__raw_writel((val | CLOCK_REQ),
				     ipc->cmu_chub_qch +
				    REG_QCH_CON_CM4_SHUB_QCH);
#endif
		} else {
			ret = -EINVAL;
			dev_warn(ipc->dev,
				 "fails to contexthub power on. Status is %d\n",
				 atomic_read(&ipc->chub_status));
		}
		break;
	case MAILBOX_EVT_RESET:
		ret = pmucal_shub_reset_release();
		break;
	default:
		break;
	}

	/* don't send alive with first poweron of multi-os */
	if (first_poweron) {
		dev_info(ipc->dev, "%s -> os select\n", __func__);
		return 0;
	}

	if (ret)
		return ret;
	else {
		/* wait active */
		dev_info(ipc->dev, "%s: alive check\n", __func__);
		trycnt = 0;
		do {
			msleep(WAIT_CHUB_MS);
			contexthub_ipc_write_event(ipc, MAILBOX_EVT_CHUB_ALIVE);
			if (++trycnt > WAIT_TRY_CNT)
				break;
		} while ((atomic_read(&ipc->chub_status) != CHUB_ST_RUN));

		if (atomic_read(&ipc->chub_status) == CHUB_ST_RUN) {
			dev_info(ipc->dev, "%s done. contexthub status is %d\n",
				 __func__, atomic_read(&ipc->chub_status));
			return 0;
		} else {
			dev_warn(ipc->dev, "%s fails. contexthub status is %d\n",
				 __func__, atomic_read(&ipc->chub_status));
			atomic_set(&ipc->chub_status, CHUB_ST_NO_RESPONSE);
			contexthub_handle_debug(ipc, CHUB_ERR_CHUB_NO_RESPONSE);
			return -ETIMEDOUT;
		}
	}
}

static void contexthub_config_init(struct contexthub_ipc_info *chub)
{
	/* BAAW-P-APM-CHUB for CHUB to access APM_CMGP. 1 window is used */
	/* SAME AS BAAW-C-CHUB FOR EXYNOS3830 */
	if (chub->chub_baaw) {
		IPC_HW_WRITE_BAAW_CHUB0(chub->chub_baaw,
					chub->baaw_info.baaw_p_apm_chub_start);
		IPC_HW_WRITE_BAAW_CHUB1(chub->chub_baaw,
					chub->baaw_info.baaw_p_apm_chub_end);
		IPC_HW_WRITE_BAAW_CHUB2(chub->chub_baaw,
					chub->baaw_info.baaw_p_apm_chub_remap);
		IPC_HW_WRITE_BAAW_CHUB3(chub->chub_baaw, BAAW_RW_ACCESS_ENABLE);
	}

	/* enable mailbox ipc */
	ipc_set_base(chub->sram);
	ipc_set_owner(AP, chub->mailbox, IPC_SRC);
}
#ifdef CONFIG_CHRE_SENSORHUB_HAL
#define os_name_idx (11)

int contexthub_get_sensortype(struct contexthub_ipc_info *ipc, char *buf)
{
	struct sensor_map *sensor_map;
	struct saved_setting *pack = (struct saved_setting *) buf;
	int len = 0;
	int trycnt = 0;
	int ret;
	unsigned int *tmp = (unsigned int *)pack;
	int i;

	if (atomic_read(&ipc->chub_status) != CHUB_ST_RUN) {
		dev_warn(ipc->dev, "%s :fails chub isn't active, status:%d, inreset:%d\n",
			__func__, atomic_read(&ipc->chub_status), atomic_read(&ipc->in_reset));
		contexthub_handle_debug(ipc, CHUB_ERR_CHUB_ST_ERR);
		return -EINVAL;
	}  else {
		clear_err_cnt(ipc, CHUB_ERR_CHUB_ST_ERR);
	}

	ret = contexthub_get_token(ipc);
	if (ret) {
		do {
			msleep(WAIT_CHUB_MS);
			if (++trycnt > WAIT_TRY_CNT)
				break;
			ret = contexthub_get_token(ipc);
		} while (ret);

		if (ret) {
			dev_warn(ipc->dev, "%s fails to get token\n", __func__);
			return -EINVAL;
		}
	}
	sensor_map = ipc_get_base(IPC_REG_IPC_SENSORINFO);
	if (ipc_have_sensor_info(sensor_map)) {

		pack->num_os = ipc->os_name[os_name_idx] - '0';
		len = ipc_get_offset(IPC_REG_IPC_SENSORINFO);
		dev_info(ipc->dev, "%s: get sensorinfo: %p (os:%d, size:%d, %d / %d %d %d)\n", __func__, sensor_map, pack->num_os, len, sizeof(struct saved_setting),
			sizeof(pack->magic), sizeof(pack->num_os), sizeof(pack->readbuf));
		memcpy(&pack->readbuf, ipc_get_sensor_base(), len);
		for (i = 0; i < SENSOR_TYPE_MAX; i++)
			if (sensor_map->active_sensor_list[i])
				dev_info(ipc->dev, "%s: get sensorinfo: type:%d, id:%d - %d\n", __func__, i, sensor_map->active_sensor_list[i], pack->readbuf[i]);
	} else {
		dev_err(ipc->dev, "%s: fails to get sensorinfo: %p\n", __func__, sensor_map);
	}
	contexthub_put_token(ipc);

	for (i = 0; i < sizeof(struct saved_setting) / sizeof(int); i++, tmp++)
		pr_info("%s: %d: 0x%x\n", __func__, i, *tmp);
	return sizeof(struct saved_setting);
}
#endif

void contexthub_ipc_status_reset(struct contexthub_ipc_info *ipc)
{
	/* clear ipc value */
	atomic_set(&ipc->wakeup_chub, CHUB_OFF);
	atomic_set(&ipc->irq1_apInt, C2A_OFF);
	atomic_set(&ipc->read_lock.cnt, 0x0);
	atomic_set(&ipc->log_work_active, 0);
	memset_io(ipc_get_base(IPC_REG_IPC_A2C), 0, ipc_get_offset(IPC_REG_IPC_A2C));
	memset_io(ipc_get_base(IPC_REG_IPC_C2A), 0, ipc_get_offset(IPC_REG_IPC_C2A));
	memset_io(ipc_get_base(IPC_REG_IPC_EVT_A2C), 0, ipc_get_offset(IPC_REG_IPC_EVT_A2C));
	memset_io(ipc_get_base(IPC_REG_IPC_EVT_C2A), 0, ipc_get_offset(IPC_REG_IPC_EVT_C2A));
	memset_io(ipc_get_base(IPC_REG_IPC_EVT_A2C_CTRL), 0, ipc_get_offset(IPC_REG_IPC_EVT_A2C_CTRL));
	memset_io(ipc_get_base(IPC_REG_IPC_EVT_C2A_CTRL), 0, ipc_get_offset(IPC_REG_IPC_EVT_C2A_CTRL));
	ipc_hw_clear_all_int_pend_reg(AP);
	ipc_hw_set_mcuctrl(AP, 0x1);
}

int contexthub_ipc_write_event(struct contexthub_ipc_info *ipc,
				enum mailbox_event event)
{
	u32 val;
	int ret = 0;
	int need_ipc = 0;

	switch (event) {
	case MAILBOX_EVT_INIT_IPC:
		ret = contexthub_ipc_drv_init(ipc);
		break;
	case MAILBOX_EVT_POWER_ON:
		ret = contexthub_hw_reset(ipc, event);
		if (!ret)
			log_flush_all();
		break;
	case MAILBOX_EVT_RESET:
		if (atomic_read(&ipc->chub_shutdown)) {
			ret = contexthub_hw_reset(ipc, event);
		} else {
			dev_err(ipc->dev,
				"contexthub status isn't shutdown. fails to reset: %d, %d\n",
					atomic_read(&ipc->chub_shutdown),
					atomic_read(&ipc->chub_status));
			ret = -EINVAL;
		}
		break;
	case MAILBOX_EVT_SHUTDOWN:
		/* assert */
		atomic_set(&ipc->chub_shutdown, 1);
		atomic_set(&ipc->chub_status, CHUB_ST_SHUTDOWN);
		if (ipc->block_reset) {
			/* pmu call assert */
			ret = pmucal_shub_reset_assert();
			if (ret) {
				pr_err("%s: reset assert fail\n", __func__);
				return ret;
			}
		} else {
#if defined(CONFIG_SOC_EXYNOS9610)
			val = __raw_readl(ipc->pmu_chub_reset +
					  REG_CHUB_CPU_STATUS);
			if (val & (1 << REG_CHUB_CPU_STATUS_BIT_STANDBYWFI)) {
				val = __raw_readl(ipc->pmu_chub_reset +
						  REG_CHUB_CPU_CONFIGURATION);
				__raw_writel(val & ~(1 << 0),
						 ipc->pmu_chub_reset +
						 REG_CHUB_CPU_CONFIGURATION);
			} else {
				dev_err(ipc->dev,
					"fails to shutdown contexthub. cpu_status: 0x%x\n",
					val);
				return -EINVAL;
			}
#else
			pr_err("%s: core reset is not implemented yet except exynos9610\n", __func__);
			return -EINVAL;
#endif
		}
		break;
	case MAILBOX_EVT_CHUB_ALIVE:
		ipc_hw_write_shared_reg(AP, AP_WAKE, SR_3);
		val = contexthub_lowlevel_alive(ipc);
		if (val) {
			atomic_set(&ipc->chub_status, CHUB_ST_RUN);
			dev_info(ipc->dev, "%s : chub is alive", __func__);
		} else if (ipc->sel_os == true) {
			dev_err(ipc->dev,
				"%s : chub isn't alive, status:%d, inreset:%d\n",
				__func__, atomic_read(&ipc->chub_status), atomic_read(&ipc->in_reset));
			ret = -EINVAL;
		}
		break;
	case MAILBOX_EVT_ENABLE_IRQ:
		/* if enable, mask from CHUB IRQ, else, unmask from CHUB IRQ */
		ipc_hw_unmask_irq(AP, IRQ_EVT_C2A_INT);
		ipc_hw_unmask_irq(AP, IRQ_EVT_C2A_INTCLR);
		break;
	case MAILBOX_EVT_DISABLE_IRQ:
		ipc_hw_mask_irq(AP, IRQ_EVT_C2A_INT);
		ipc_hw_mask_irq(AP, IRQ_EVT_C2A_INTCLR);
		break;
	default:
		need_ipc = 1;
		break;
	}

	if (need_ipc) {
		if (atomic_read(&ipc->chub_status) != CHUB_ST_RUN) {
			dev_warn(ipc->dev, "%s event:%d/%d fails chub isn't active, status:%d, inreset:%d\n",
				__func__, event, MAILBOX_EVT_MAX, atomic_read(&ipc->chub_status), atomic_read(&ipc->in_reset));
			contexthub_handle_debug(ipc, CHUB_ERR_CHUB_ST_ERR);
			return -EINVAL;
		}  else {
			clear_err_cnt(ipc, CHUB_ERR_CHUB_ST_ERR);
		}

		if (contexthub_get_token(ipc))
			return -EINVAL;

		/* handle ipc */
		switch (event) {
		case MAILBOX_EVT_RT_LOGLEVEL:
			ipc_logbuf_loglevel(ipc->chub_rt_log.loglevel, 1);
			break;
		case MAILBOX_EVT_ERASE_SHARED:
			memset(ipc_get_base(IPC_REG_SHARED), 0, ipc_get_offset(IPC_REG_SHARED));
			break;
		case MAILBOX_EVT_DUMP_STATUS:
			/* dump nanohub kernel status */
			dev_info(ipc->dev, "Request to dump chub fw status\n");
			ipc_write_debug_event(AP, (u32)MAILBOX_EVT_DUMP_STATUS);
			ret = ipc_add_evt(IPC_EVT_A2C, IRQ_EVT_A2C_DEBUG);
			break;
		case MAILBOX_EVT_WAKEUP_CLR:
			if (atomic_read(&ipc->wakeup_chub) == CHUB_ON) {
				ret = ipc_add_evt(IPC_EVT_A2C, IRQ_EVT_A2C_WAKEUP_CLR);
				if (ret >= 0) {
					atomic_set(&ipc->wakeup_chub, CHUB_OFF);
					clear_err_cnt(ipc, CHUB_ERR_EVTQ_WAKEUP_CLR);
				} else {
					dev_warn(ipc->dev, "%s: fails to set wakeup. ret:%d, err:%d",
						__func__, ret, ipc->err_cnt[CHUB_ERR_EVTQ_WAKEUP_CLR]);
					contexthub_handle_debug(ipc, CHUB_ERR_EVTQ_WAKEUP_CLR);
				}
			}
			break;
		case MAILBOX_EVT_WAKEUP:
			if (atomic_read(&ipc->wakeup_chub) == CHUB_OFF) {
				ret = ipc_add_evt(IPC_EVT_A2C, IRQ_EVT_A2C_WAKEUP);
				if (ret >= 0) {
					atomic_set(&ipc->wakeup_chub, CHUB_ON);
					clear_err_cnt(ipc, CHUB_ERR_EVTQ_WAKEUP);
				} else {
					dev_warn(ipc->dev, "%s: fails to set wakeupclr. ret:%d, err:%d",
						__func__, ret, ipc->err_cnt[CHUB_ERR_EVTQ_WAKEUP]);
					contexthub_handle_debug(ipc, CHUB_ERR_EVTQ_WAKEUP);
				}
			}
			break;
		default:
			/* handle ipc utc */
			if ((int)event < IPC_DEBUG_UTC_MAX) {
				ipc->utc_run = event;
				if ((int)event == IPC_DEBUG_UTC_TIME_SYNC)
					check_rtc_time();
				ipc_write_debug_event(AP, (u32)event);
				ret = ipc_add_evt(IPC_EVT_A2C, IRQ_EVT_A2C_DEBUG);
			}
			break;
		}
		contexthub_put_token(ipc);

		if (ret < 0) {
			contexthub_handle_debug(ipc, CHUB_ERR_EVTQ_ADD);
			dev_warn(ipc->dev, "%s: errcnt: evtq:%d, wake:%d, clr:%d\n",
				__func__, ipc->err_cnt[CHUB_ERR_EVTQ_ADD],
				ipc->err_cnt[CHUB_ERR_EVTQ_WAKEUP],
				ipc->err_cnt[CHUB_ERR_EVTQ_WAKEUP_CLR]);
		} else
			clear_err_cnt(ipc, CHUB_ERR_EVTQ_ADD);
	}
	return ret;
}

int contexthub_poweron(struct contexthub_ipc_info *ipc)
{
	int ret = 0;
	struct device *dev = ipc->dev;
#if !defined(CONFIG_SENSORS_SSP) && !defined(CONFIG_SHUB)
	struct chub_bootargs *map;
#endif

	if (!atomic_read(&ipc->chub_status)) {
		memset_io(ipc->sram, 0, ipc->sram_size);
		ret = contexthub_download_image(ipc, IPC_REG_BL);
		if (ret) {
			dev_warn(dev, "fails to download bootloader\n");
			return ret;
		}

		if (ipc_get_offset(IPC_REG_DUMP) != ipc->sram_size)
			dev_warn(dev, "sram size doen't match kernel:%d, fw:%d\n", ipc->sram_size, ipc_get_offset(IPC_REG_DUMP));

		ret = contexthub_ipc_write_event(ipc, MAILBOX_EVT_INIT_IPC);
		if (ret) {
			dev_warn(dev, "fails to init ipc\n");
			return ret;
		}

#if !defined(CONFIG_SENSORS_SSP) && !defined(CONFIG_SHUB)
		if (!strcmp(ipc->os_name, "os.checked_0.bin") || ipc->os_name[0] != 'o') {
			map = ipc_get_base(IPC_REG_BL_MAP);
			ipc->sel_os = !(map->bootmode);
		} else
#endif
			dev_info(dev, "saved os_name: %s", ipc->os_name);

		ret = contexthub_download_image(ipc, IPC_REG_OS);
		if (ret) {
			dev_warn(dev, "fails to download kernel\n");
			return ret;
		}

		ret = contexthub_ipc_write_event(ipc, MAILBOX_EVT_POWER_ON);
		if (ret) {
			dev_warn(dev, "fails to poweron\n");
			return ret;
		}
		if (atomic_read(&ipc->chub_status) == CHUB_ST_RUN)
			dev_info(dev, "%s: contexthub power-on", __func__);
		else {
			if (ipc->sel_os)
				dev_warn(dev, "contexthub failed to power-on");
			else {
				dev_info(dev, "%s: wait for multi-os poweron\n", __func__);
				ret = chub_wait_event(&ipc->poweron_lock, WAIT_TIMEOUT_MS * 2);
				dev_info(dev, "%s: multi-os poweron %s, status:%d, ret:%d, flag:%d\n", __func__,
					atomic_read(&ipc->chub_status) == CHUB_ST_RUN ? "success" : "fails",
					atomic_read(&ipc->chub_status), ret, ipc->poweron_lock.flag);
			}
		}
	} else
	/* CHUB already went through poweron sequence */
		return -EINVAL;

	return 0;
}

static int contexthub_download_and_check_image(struct contexthub_ipc_info *ipc, enum ipc_region reg)
{
	u32 *fw = vmalloc(ipc_get_offset(reg));
	int ret = 0;

	if (!fw)
		return contexthub_download_image(ipc, reg);

	memcpy_fromio(fw, ipc_get_base(reg), ipc_get_offset(reg));
	ret = contexthub_download_image(ipc, reg);
	if (ret) {
		dev_err(ipc->dev, "%s: download bl(%d) fails\n", __func__, reg == IPC_REG_BL);
		goto out;
	}

	ret = memcmp(fw, ipc_get_base(reg), ipc_get_offset(reg));
	if (ret) {
		int i;
		u32 *fw_image = (u32 *)ipc_get_base(reg);

		dev_err(ipc->dev, "%s: fw(%lx) doens't match with size %d\n",
			__func__, (unsigned long)ipc_get_base(reg), ipc_get_offset(reg));
		for (i = 0; i < ipc_get_offset(reg) / 4; i++)
			if (fw[i] != fw_image[i]) {
				dev_err(ipc->dev, "fw[%d] %x -> wrong %x\n", i, fw_image[i], fw[i]);
				print_hex_dump(KERN_CONT, "before:", DUMP_PREFIX_OFFSET, 16, 1, &fw[i], 64, false);
				print_hex_dump(KERN_CONT, "after:", DUMP_PREFIX_OFFSET, 16, 1, &fw_image[i], 64, false);
				ret = -EINVAL;
				break;
			}
	}
out:
	dev_info(ipc->dev, "%s: download and checked bl(%d) ret:%d \n", __func__, reg == IPC_REG_BL, ret);
	vfree(fw);
	return ret;
}

int contexthub_reset_prepare(struct contexthub_ipc_info *ipc) {
	int ret;
	/* pmu call reset-release_config */
	ret = pmucal_shub_reset_release_config();
	if (ret) {
		pr_err("%s: reset release cfg fail\n", __func__);
		return ret;
	}
	/* tzpc setting */
	ret = exynos_smc(SMC_CMD_CONN_IF,
		(EXYNOS_SHUB << 32) |
		EXYNOS_SET_CONN_TZPC, 0, 0);
	if (ret) {
		pr_err("%s: TZPC setting fail\n",
			__func__);
		return -EINVAL;
	}
	dev_info(ipc->dev, "%s: tzpc set\n", __func__);
	/* baaw config */
	contexthub_config_init(ipc);
	return ret;
}

int contexthub_reset(struct contexthub_ipc_info *ipc, bool force_load, enum chub_err_type err)
{
	int ret = 0;
	int trycnt = 0;
	bool irq_disabled;

	mutex_lock(&reset_mutex);
	dev_info(ipc->dev, "%s: force:%d, status:%d, in-reset:%d, err:%d, user:%d\n",
		__func__, force_load, atomic_read(&ipc->chub_status),
		atomic_read(&ipc->in_reset), err, atomic_read(&ipc->in_use_ipc));
	if (!force_load && (atomic_read(&ipc->chub_status) == CHUB_ST_RUN)) {
		mutex_unlock(&reset_mutex);
		dev_info(ipc->dev, "%s: out status:%d\n", __func__, atomic_read(&ipc->chub_status));
		return 0;
	}

	/* disable mailbox interrupt to prevent sram access during chub reset */
	disable_irq(ipc->irq_mailbox);
	irq_disabled = true;

	atomic_inc(&ipc->in_reset);
	__pm_stay_awake(&ipc->ws_reset);

	/* wait for ipc free */
	while (atomic_read(&ipc->in_use_ipc)) {
		msleep(WAIT_CHUB_MS);
		if (++trycnt > RESET_WAIT_TRY_CNT) {
			dev_info(ipc->dev, "%s: can't get lock. in_use_ipc: %d\n",
				__func__, atomic_read(&ipc->in_use_ipc));
			ret = -EINVAL;
			goto out;
		}
		dev_info(ipc->dev, "%s: wait for ipc user free: %d\n",
			__func__, atomic_read(&ipc->in_use_ipc));
	}

	/* debug dump */
	if (atomic_read(&ipc->chub_status) != CHUB_ST_SHUTDOWN && err != CHUB_ERR_ITMON)
		chub_dbg_dump_hw(ipc, err);

	dev_info(ipc->dev, "%s: start reset status:%d\n", __func__, atomic_read(&ipc->chub_status));

	if (!ipc->block_reset) {
		/* core reset */
		ipc_add_evt(IPC_EVT_A2C, IRQ_EVT_A2C_SHUTDOWN);
		msleep(100);	/* wait for shut down time */
	}

	/* shutdown */
	mutex_lock(&pmu_shutdown_mutex);
	atomic_set(&ipc->chub_shutdown, 0);
	dev_info(ipc->dev, "%s: shutdown\n", __func__);
	ret = contexthub_ipc_write_event(ipc, MAILBOX_EVT_SHUTDOWN);
	if (ret) {
		dev_err(ipc->dev, "%s: shutdown fails, ret:%d\n", __func__, ret);
		mutex_unlock(&pmu_shutdown_mutex);
		goto out;
	}
	/* deassert and config */
	dev_info(ipc->dev, "%s: prepare\n", __func__);
	ret = contexthub_reset_prepare(ipc);
	if (ret) {
		dev_err(ipc->dev, "%s: prepare fails, ret:%d\n", __func__, ret);
		mutex_unlock(&pmu_shutdown_mutex);
		goto out;
	}
	mutex_unlock(&pmu_shutdown_mutex);

	/* image download */
	dev_info(ipc->dev, "%s: clear ipc:%p, %d\n", __func__, ipc_get_base(IPC_REG_IPC), ipc_get_offset(IPC_REG_IPC));
	memset_io(ipc_get_base(IPC_REG_IPC_A2C), 0, ipc_get_offset(IPC_REG_IPC_A2C));
	memset_io(ipc_get_base(IPC_REG_IPC_C2A), 0, ipc_get_offset(IPC_REG_IPC_C2A));
	memset_io(ipc_get_base(IPC_REG_IPC_EVT_A2C), 0, ipc_get_offset(IPC_REG_IPC_EVT_A2C));
	memset_io(ipc_get_base(IPC_REG_IPC_EVT_C2A), 0, ipc_get_offset(IPC_REG_IPC_EVT_C2A));
	memset_io(ipc_get_base(IPC_REG_LOG), 0, ipc_get_offset(IPC_REG_LOG));
	if (ipc->block_reset || force_load) {
		ret = contexthub_download_image(ipc, IPC_REG_BL);
		if (!ret) {
			if (force_load) /* can use new binary */
				ret = contexthub_download_image(ipc, IPC_REG_OS);
			else /* use previous binary */
				ret = contexthub_download_and_check_image(ipc, IPC_REG_OS);

			if (ret) {
				dev_err(ipc->dev, "%s: download os fails\n", __func__);
				ret = -EINVAL;
				goto out;
			}
		} else {
				dev_err(ipc->dev, "%s: download bl fails\n", __func__);
				ret = -EINVAL;
				goto out;
		}
	}

	/* enable mailbox interrupt to get 'alive' event */
	enable_irq(ipc->irq_mailbox);
	irq_disabled = false;

	/* reset release */
	ret = contexthub_ipc_write_event(ipc, MAILBOX_EVT_RESET);
	if (ret) {
	        dev_err(ipc->dev, "%s: chub reset fail! (ret:%d)\n", __func__, ret);
	} else {
	        dev_info(ipc->dev, "%s: chub reset done! (cnt:%d)\n",
	                __func__, ipc->err_cnt[CHUB_ERR_RESET_CNT]);
	        ipc->err_cnt[CHUB_ERR_RESET_CNT]++;
	        atomic_set(&ipc->in_use_ipc, 0);
	}
out:
	if (ret) {
		atomic_set(&ipc->chub_status, CHUB_ST_RESET_FAIL);
		if (irq_disabled)
			enable_irq(ipc->irq_mailbox);
		dev_err(ipc->dev, "%s: chub reset fail! should retry to reset (ret:%d), irq_disabled:%d\n",
			__func__, ret, irq_disabled);
	}
	msleep(100); /* wakeup delay */
	chub_wake_event(&ipc->reset_lock);
	__pm_relax(&ipc->ws_reset);
	atomic_dec(&ipc->in_reset);

#ifdef CONFIG_SENSORS_SSP
	if (!ret)
		ssp_platform_start_refrsh_task();
#elif defined(CONFIG_SHUB)
	if (!ret)
		shub_start_refresh_task();
#endif
	mutex_unlock(&reset_mutex);
#ifdef CONFIG_CHRE_SENSORHUB_HAL
	nanohub_reset_status(ipc->data);
#endif
	return ret;
}

int contexthub_download_image(struct contexthub_ipc_info *ipc, enum ipc_region reg)
{
	const struct firmware *entry;
	int ret = 0;

	if (reg == IPC_REG_BL) {
		dev_info(ipc->dev, "%s: download bl\n", __func__);
#if defined(CONFIG_SENSORS_SSP)
		ret = request_firmware(&entry, SSP_BOOTLOADER_FILE, ipc->dev);
#elif defined(CONFIG_SHUB)
		ret = request_firmware(&entry, BOOTLOADER_FILE, ipc->dev);
#else
		ret = request_firmware(&entry, "bl.unchecked.bin", ipc->dev);
#endif
	} else if (reg == IPC_REG_OS) {
#ifdef CONFIG_SENSORS_SSP
		ret = ssp_download_firmware(ipc_get_base(reg));
		return ret;
#elif defined(CONFIG_SHUB)
		ret = shub_download_firmware(ipc_get_base(reg));
		return ret;
#else
		dev_info(ipc->dev, "%s: download %s\n", __func__, ipc->os_name);
		ret = request_firmware(&entry, ipc->os_name, ipc->dev);
#endif
	} else {
		dev_err(ipc->dev, "%s: invalid reg:%d\n", __func__, reg);
		return -EINVAL;
	}

	if (ret) {
		dev_err(ipc->dev, "%s, bl(%d) request_firmware failed\n",
			reg == IPC_REG_BL, __func__);
		release_firmware(entry);
		return ret;
	}
	memcpy_toio(ipc_get_base(reg), entry->data, entry->size);
	dev_info(ipc->dev, "%s: bl:%d, bin(size:%d) on %lx\n",
		 __func__, reg == IPC_REG_BL, (int)entry->size, (unsigned long)ipc_get_base(reg));
	release_firmware(entry);

	return 0;
}

static void handle_irq(struct contexthub_ipc_info *ipc, enum irq_evt_chub evt)
{
	int err;
	int dbg;

	switch (evt) {
	case IRQ_EVT_C2A_DEBUG:
		if (contexthub_get_token(ipc)) {
			dev_warn(ipc->dev, "%s: get token\n", __func__);
			return;
		}
		dbg = ipc_read_debug_event(AP);
		err = (dbg == IPC_DEBUG_CHUB_FAULT) ? CHUB_ERR_FW_FAULT : CHUB_ERR_FW_ERROR;
		ipc_write_debug_event(AP, 0);
		contexthub_put_token(ipc);
		dev_err(ipc->dev, "%s: c2a_debug: debug:%d, err:%d\n", __func__, dbg, err);
		contexthub_handle_debug(ipc, err);
		break;
	case IRQ_EVT_C2A_INT:
		if (atomic_read(&ipc->irq1_apInt) == C2A_OFF) {
			atomic_set(&ipc->irq1_apInt, C2A_ON);

			if (atomic_read(&ipc->chub_status) == CHUB_ST_RUN)
				contexthub_notify_host(ipc);
		}
		break;
	case IRQ_EVT_C2A_INTCLR:
		atomic_set(&ipc->irq1_apInt, C2A_OFF);
		break;
	case IRQ_EVT_C2A_LOG:
		break;
	default:
		if (evt < IRQ_EVT_CH_MAX) {
#if defined(CONFIG_SENSORS_SSP) || defined(CONFIG_SHUB)
			int size = 0;
			char rx_buf[PACKET_SIZE_MAX] = {0,};
			void *raw_rx_buf = 0;

			raw_rx_buf = ipc_read_data(IPC_DATA_C2A, &size);
			if (size > 0 && raw_rx_buf) {
				memcpy_fromio(rx_buf, (void *)raw_rx_buf, size);
#if defined(CONFIG_SENSORS_SSP)
				ssp_handle_recv_packet(rx_buf, size);
#else
				shub_handle_recv_packet(rx_buf, size);
#endif
			}
			/* check : ipc_read err handle */

#else
			int lock;

			atomic_inc(&ipc->read_lock.cnt);
			/* TODO: requered.. ? */
			spin_lock(&ipc->read_lock.event.lock);
			lock = atomic_read(&ipc->read_lock.flag);
			spin_unlock(&ipc->read_lock.event.lock);
			if (lock)
				wake_up_interruptible_sync(&ipc->read_lock.event);
#endif
		} else {
			dev_warn(ipc->dev, "%s: invalid %d event",
				 __func__, evt);
			return;
		}
		break;
	};
#if 0
	if (ipc_logbuf_filled() && !atomic_read(&ipc->log_work_active)) {
		ipc->log_work_reqcnt++; /* debug */
		atomic_set(&ipc->log_work_active, 1);
		schedule_work(&ipc->log_work);
	}
#endif
}

static irqreturn_t contexthub_irq_handler(int irq, void *data)
{
	struct contexthub_ipc_info *ipc = data;
	int start_index = ipc_hw_read_int_start_index(AP);
	unsigned int status = ipc_hw_read_int_status_reg(AP);
	struct ipc_evt_buf *cur_evt;
	enum chub_err_type err = 0;
	enum irq_chub evt = 0;
	int irq_num = IRQ_EVT_CHUB_ALIVE + start_index;
	u32 status_org = status; /* for debug */
	struct ipc_evt *ipc_evt = ipc_get_base(IPC_REG_IPC_EVT_C2A);

	/* chub alive interrupt handle */
	if (status & (1 << irq_num)) {
		status &= ~(1 << irq_num);
		ipc_hw_clear_int_pend_reg(AP, irq_num);
		if (atomic_read(&ipc->chub_status) == CHUB_ST_POWER_ON && ipc->sel_os == false) {
			schedule_work(&ipc->debug_work);
			return IRQ_HANDLED;
		}

		if (ipc_hw_read_shared_reg(AP, SR_3) == CHUB_REBOOT_REQ) {
			dev_err(ipc->dev," chub sends to request reboot\n");
			contexthub_handle_debug(ipc, CHUB_ERR_FW_REBOOT);
		} else {
			/* set wakeup flag for chub_alive_lock */
			chub_wake_event(&ipc->chub_alive_lock);
		}
	}

	if (contexthub_get_token(ipc)) {
		dev_err(ipc->dev, "%s: in reset irq_status:%d", __func__, status);
		ipc_hw_clear_all_int_pend_reg(AP);
		return IRQ_HANDLED;
	}

	irq_num = IRQ_EVT_C2A_LOG + start_index;
	if (status & (1 << irq_num)) {
		status &= ~(1 << irq_num);
		ipc_hw_clear_int_pend_reg(AP, irq_num);
		handle_irq(ipc, IRQ_EVT_C2A_LOG);
	}

#ifdef CHECK_HW_TRIGGER
	/* chub ipc interrupt handle */
	while (status) {
		cur_evt = ipc_get_evt(IPC_EVT_C2A);

		if (cur_evt) {
			evt = cur_evt->evt;
			irq_num = cur_evt->irq + start_index;

			if (!ipc_evt->ctrl.pending[cur_evt->irq]) {
				err = CHUB_ERR_EVTQ_NO_HW_TRIGGER;
				dev_err(ipc->dev,
					"%s: no-sw-trigger irq:%d(%d+%d), evt:%d, status:0x%x->0x%x(SR:0x%x)\n", __func__,
					irq_num, cur_evt->irq, start_index, evt, status_org, status, ipc_hw_read_int_status_reg(AP));
				status = 0;
				break;
			}

			/* check match evtq and hw interrupt pending */
			if (!(status & (1 << irq_num))) {
				err = CHUB_ERR_EVTQ_NO_HW_TRIGGER;
				CSP_PRINTF_ERROR
					 ("%s: no-hw-trigger irq:%d(%d+%d), evt:%d, status:0x%x->0x%x(SR:0x%x)\n", __func__,
					  irq_num, cur_evt->irq, start_index, evt, status_org, status, ipc_hw_read_int_status_reg(AP));
				status = 0;
				break;
			}
		} else {
			err = CHUB_ERR_EVTQ_EMTPY;
			CSP_PRINTF_ERROR
				 ("%s: evt-empty irq:%d(%d), evt:%d, status:0x%x->0x%x(SR:0x%x)\n", __func__,
				  irq_num, start_index, evt, status_org, status, ipc_hw_read_int_status_reg(AP));
			status = 0;
			break;
		}
		ipc_hw_clear_int_pend_reg(AP, irq_num);
		ipc_evt->ctrl.pending[cur_evt->irq] = 0;
		handle_irq(ipc, (u32)evt);
		status &= ~(1 << irq_num);
	}
#else
	if (status) {
		int i;

		for (i = start_index; i < irq_num; i++) {
			if (status & (1 << i)) {
				cur_evt = ipc_get_evt(IPC_EVT_C2A);
				if (cur_evt) {
					evt = cur_evt->evt;
					handle_irq(ipc, (u32)evt);
					ipc_hw_clear_int_pend_reg(AP, i);
				} else {
					err = CHUB_ERR_EVTQ_EMTPY;
					dev_err(ipc->dev,
						"%s: evt-empty status: irq:%d(%d), evt:%d, status:0x%x->0x%x(SR:0x%x)\n",
						__func__, irq_num, start_index, evt, status_org, status, ipc_hw_read_int_status_reg(AP));
					break;
				}
			}
		}
	}
#endif

	contexthub_put_token(ipc);
	if (err) {
		contexthub_handle_debug(ipc, err);
		dev_err(ipc->dev, "nanohub: inval irq err(%d),cnt:%d:start_irqnum:%d,evt(%p):%d,irq_hw:%d,status_reg:0x%x->0x%x(0x%x,0x%x)\n",
		       err, ipc->err_cnt[err], start_index, cur_evt, evt, irq_num,
		       status_org, status, ipc_hw_read_int_status_reg(AP),
		       ipc_hw_read_int_gen_reg(AP));
		ipc_hw_clear_all_int_pend_reg(AP);
	} else {
		clear_err_cnt(ipc, CHUB_ERR_EVTQ_EMTPY);
		clear_err_cnt(ipc, CHUB_ERR_EVTQ_NO_HW_TRIGGER);
	}
	return IRQ_HANDLED;
}

static irqreturn_t contexthub_irq_wdt_handler(int irq, void *data)
{
	struct contexthub_ipc_info *ipc = data;

	dev_info(ipc->dev, "%s called\n", __func__);
	disable_irq_nosync(ipc->irq_wdt);
	ipc->irq_wdt_disabled = 1;
	contexthub_handle_debug(ipc, CHUB_ERR_FW_WDT);

	return IRQ_HANDLED;
}

static struct clk *devm_clk_get_and_prepare(struct device *dev,
	const char *name)
{
	struct clk *clk = NULL;
	int ret;

	clk = devm_clk_get(dev, name);
	if (IS_ERR(clk)) {
		dev_err(dev, "Failed to get clock %s\n", name);
		goto error;
	}

	ret = clk_prepare(clk);
	if (ret < 0) {
		dev_err(dev, "Failed to prepare clock %s\n", name);
		goto error;
	}

	ret = clk_enable(clk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable clock %s\n", name);
		goto error;
	}

error:
	return clk;
}

#if defined(CONFIG_SOC_EXYNOS9610)
extern int cal_dll_apm_enable(void);
#endif

static void __iomem *get_iomem(struct platform_device *pdev,
                const char *name, u32 *size)
{
	struct resource *res;
	void __iomem *ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	if (IS_ERR_OR_NULL(res)) {
		dev_err(&pdev->dev, "Failed to get %s\n", name);
		return ERR_PTR(-EINVAL);
	}

	ret = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ret)) {
		dev_err(&pdev->dev, "fails to get %s\n", name);
		return ERR_PTR(-EINVAL);
	}

	if (size)
		*size = resource_size(res);
	dev_info(&pdev->dev, "%s: %s(%p) is mapped on %p with size of %zu",
		__func__, name, (void *)res->start, ret, (size_t)resource_size(res));

	return ret;
}

static __init int contexthub_ipc_hw_init(struct platform_device *pdev,
					 struct contexthub_ipc_info *chub)
{
	int ret;
	int irq;
	const char *os;
	const char *resetmode;
	const char *selectos;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct clk *clk;
#if defined(CONFIG_SOC_EXYNOS9610)
	struct resource *res;
	const char *string_array[10];
	int chub_clk_len;
	int i;
#endif
	if (!node) {
		dev_err(dev, "driver doesn't support non-dt\n");
		return -ENODEV;
	}

	/* get os type from dt */
	os = of_get_property(node, "os-type", NULL);
	if (!os || !strcmp(os, "none") || !strcmp(os, "pass")) {
		dev_err(dev, "no use contexthub\n");
		chub->os_load = 0;
		return -ENODEV;
	} else {
		chub->os_load = 1;
		strcpy(chub->os_name, os);
	}

	/* get resetmode from dt */
	resetmode = of_get_property(node, "reset-mode", NULL);
	if (!resetmode || !strcmp(resetmode, "block"))
		chub->block_reset = 1;
	else
		chub->block_reset = 0;

	/* get os select from dt */
	selectos = of_get_property(node, "os-select", NULL);
	if (!selectos || strcmp(selectos, "true")) {
		dev_info(dev,"multi os disabled : %s\n", selectos);
		chub->sel_os = true;
	} else {
		dev_info(dev,"multi os enabled : %s\n", selectos);
		chub->sel_os = false;
	}

	/* get mailbox interrupt */
	chub->irq_mailbox = irq_of_parse_and_map(node, 0);
	if (chub->irq_mailbox < 0) {
		dev_err(dev, "failed to get irq:%d\n", irq);
		return -EINVAL;
	}

	/* request irq handler */
#if defined(CONFIG_SENSORS_SSP) || defined(CONFIG_SHUB)
	ret = devm_request_threaded_irq(dev, chub->irq_mailbox, NULL, contexthub_irq_handler,
			       IRQF_ONESHOT, dev_name(dev), chub);
#else
	ret = devm_request_irq(dev, chub->irq_mailbox, contexthub_irq_handler,
			       IRQ_TYPE_LEVEL_HIGH, dev_name(dev), chub);
#endif
	if (ret) {
		dev_err(dev, "failed to request irq:%d, ret:%d\n",
			chub->irq_mailbox, ret);
		return ret;
	}

	/* get wdt interrupt optionally */
	chub->irq_wdt = irq_of_parse_and_map(node, 1);
	if (chub->irq_wdt > 0) {
		/* request irq handler */
		ret = devm_request_irq(dev, chub->irq_wdt, contexthub_irq_wdt_handler,
					IRQ_TYPE_LEVEL_HIGH, dev_name(dev), chub);
		if (ret) {
			dev_err(dev, "failed to request wdt irq:%d, ret:%d\n",
				chub->irq_wdt, ret);
			return ret;
		}
		chub->irq_wdt_disabled = 0;
	} else {
		dev_info(dev, "don't use wdt irq:%d\n", irq);
	}

	/* get MAILBOX SFR */
	chub->mailbox = get_iomem(pdev, "mailbox", NULL);
	if (IS_ERR(chub->mailbox))
		return PTR_ERR(chub->mailbox);

	/* get SRAM base */
	chub->sram = get_iomem(pdev, "sram", &chub->sram_size);
	if (IS_ERR(chub->sram))
			return PTR_ERR(chub->sram);

	/* get chub gpr base */
	chub->chub_dumpgpr = get_iomem(pdev, "dumpgpr", NULL);
	if (IS_ERR(chub->chub_dumpgpr))
		return PTR_ERR(chub->chub_dumpgpr);

#if defined(CONFIG_SOC_EXYNOS9610)
	chub->pmu_chub_reset = get_iomem(pdev, "chub_reset", NULL);
	if (IS_ERR(chub->pmu_chub_reset))
		return PTR_ERR(chub->pmu_chub_reset);
#endif

	chub->chub_baaw = get_iomem(pdev, "chub_baaw", NULL);
	if (IS_ERR(chub->chub_baaw))
		return PTR_ERR(chub->chub_baaw);

#if defined(CONFIG_SOC_EXYNOS9610)
	/* get cmu qch base */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cmu_chub_qch");
	chub->cmu_chub_qch = devm_ioremap_resource(dev, res);
	if (IS_ERR(chub->cmu_chub_qch)) {
		pr_err("driver failed to get cmu_chub_qch\n");
		return PTR_ERR(chub->cmu_chub_qch);
	}
#endif
	/* get addresses information to set BAAW */
	if (of_property_read_u32_index
		(node, "baaw,baaw-p-apm-chub", 0,
		 &chub->baaw_info.baaw_p_apm_chub_start)) {
		dev_err(&pdev->dev,
			"driver failed to get baaw-p-apm-chub, start\n");
		return -ENODEV;
	}

	if (of_property_read_u32_index
		(node, "baaw,baaw-p-apm-chub", 1,
		 &chub->baaw_info.baaw_p_apm_chub_end)) {
		dev_err(&pdev->dev,
			"driver failed to get baaw-p-apm-chub, end\n");
		return -ENODEV;
	}

	if (of_property_read_u32_index
		(node, "baaw,baaw-p-apm-chub", 2,
		 &chub->baaw_info.baaw_p_apm_chub_remap)) {
		dev_err(&pdev->dev,
			"driver failed to get baaw-p-apm-chub, remap\n");
		return -ENODEV;
	}

#if defined(CONFIG_SOC_EXYNOS9610)
	/* disable chub irq list (for sensor irq) */
	of_property_read_u32(node, "chub-irq-pin-len", &chub->irq_pin_len);
	if (chub->irq_pin_len) {
		if (chub->irq_pin_len > sizeof(chub->irq_pins)) {
			dev_err(&pdev->dev,
			"failed to get irq pin length %d, %d\n",
			chub->irq_pin_len, sizeof(chub->irq_pins));
			chub->irq_pin_len = 0;
			return -ENODEV;
		}

		dev_info(&pdev->dev, "get chub irq_pin len:%d\n", chub->irq_pin_len);
		for (i = 0; i < chub->irq_pin_len; i++) {
			chub->irq_pins[i] = of_get_named_gpio(node, "chub-irq-pin", i);
			if (!gpio_is_valid(chub->irq_pins[i])) {
				dev_err(&pdev->dev, "get invalid chub irq_pin:%d\n", chub->irq_pins[i]);
				return -EINVAL;
			}
			dev_info(&pdev->dev, "get chub irq_pin:%d\n", chub->irq_pins[i]);
		}
	}
	cal_dll_apm_enable();
#endif
	clk = devm_clk_get_and_prepare(dev, "chub_bus");
	if (IS_ERR(clk)) {
		dev_info(dev, "fail to get chub_bus: %lu\n", clk);
		return -ENODEV;
	}
	chub->clkrate = clk_get_rate(clk);
#if defined(CONFIG_SOC_EXYNOS9630) || defined(CONFIG_SOC_EXYNOS3830)
	dev_info(dev, "clk set before: %lu\n", chub->clkrate);
	clk_set_rate(clk, CHUB_DLL_CLK);
	msleep(10);
	chub->clkrate = clk_get_rate(clk);
	dev_info(dev, "clk set after: %lu\n", chub->clkrate);
#endif
	if (!chub->clkrate)
		/* default clkrate */
		chub->clkrate = 400000000;
#if defined(CONFIG_SOC_EXYNOS9610)
	chub_clk_len = of_property_count_strings(node, "clock-names");
	of_property_read_string_array(node, "clock-names", string_array, chub_clk_len);
	for (i = 0; i < chub_clk_len; i++) {
		clk = devm_clk_get_and_prepare(dev, string_array[i]);
		if (!clk)
			return -ENODEV;
		dev_info(&pdev->dev, "clk_name: %s enable\n", __clk_get_name(clk));
	}
#endif
	return 0;
}

static ssize_t chub_poweron(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct contexthub_ipc_info *ipc = dev_get_drvdata(dev);
	int ret = contexthub_poweron(ipc);

#ifdef CONFIG_SENSORS_SSP
	if (ret < 0)
		dev_err(dev, "poweron failed %d\n", ret);
	else
		ssp_platform_start_refrsh_task();
#elif defined(CONFIG_SHUB)
	if (ret < 0)
		dev_err(dev, "poweron failed %d\n", ret);
	else
		shub_start_refresh_task();
#endif
	return ret < 0 ? ret : count;
}

static ssize_t chub_reset(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct contexthub_ipc_info *ipc = dev_get_drvdata(dev);
	int ret;

#ifdef CONFIG_CHRE_SENSORHUB_HAL
	ret = nanohub_hw_reset(ipc->data);
#else
	ret = contexthub_reset(ipc, 1, CHUB_ERR_NONE);
#endif

	return ret < 0 ? ret : count;
}

static struct device_attribute attributes[] = {
	__ATTR(poweron, 0220, NULL, chub_poweron),
	__ATTR(reset, 0220, NULL, chub_reset),
};

#ifdef CONFIG_EXYNOS_ITMON
static int chub_itmon_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct contexthub_ipc_info *data = container_of(nb, struct contexthub_ipc_info, itmon_nb);
	struct itmon_notifier *itmon_data = nb_data;

	if (itmon_data && itmon_data->master &&
	    (strstr(itmon_data->master, "CHUB") || strstr(itmon_data->master, "SHUB"))) {
		dev_info(data->dev, "%s: chub(%s) itmon detected: action:%d!!\n",
			__func__, itmon_data->master, action);
		if (atomic_read(&data->chub_status) == CHUB_ST_RUN) {
			atomic_set(&data->chub_status, CHUB_ST_ERR);
			chub_dbg_dump_hw(data, CHUB_ERR_ITMON);
			contexthub_ipc_write_event(data, MAILBOX_EVT_SHUTDOWN);
			contexthub_handle_debug(data, CHUB_ERR_ITMON);
		}
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}
#endif

static int chub_panic_handler(struct notifier_block *nb,
                               unsigned long action, void *data)
{
	chub_dbg_dump_ram(CHUB_ERR_KERNEL_PANIC);
	return NOTIFY_OK;
}

static struct notifier_block chub_panic_notifier = {
	.notifier_call  = chub_panic_handler,
	.next           = NULL,
	.priority       = 0     /* priority: INT_MAX >= x >= 0 */
};

static int contexthub_ipc_probe(struct platform_device *pdev)
{
	struct contexthub_ipc_info *chub;
	int need_to_free = 0;
	int ret = 0;
	int i;
#ifdef CONFIG_CHRE_SENSORHUB_HAL
	struct iio_dev *iio_dev;
#endif
	chub = chub_dbg_get_memory(DBG_NANOHUB_DD_AREA);
	if (!chub) {
		chub =
		    devm_kzalloc(&pdev->dev, sizeof(struct contexthub_ipc_info),
				 GFP_KERNEL);
		need_to_free = 1;
	}
	if (IS_ERR(chub)) {
		dev_err(&pdev->dev, "%s failed to get ipc memory\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	/* parse dt and hw init */
	ret = contexthub_ipc_hw_init(pdev, chub);
	if (ret) {
		dev_err(&pdev->dev, "%s failed to get init hw with ret %d\n",
			__func__, ret);
		goto err;
	}

#ifdef CONFIG_CHRE_SENSORHUB_HAL
	/* nanohub probe */
	iio_dev = nanohub_probe(&pdev->dev, NULL);
	if (IS_ERR(iio_dev))
		goto err;

	/* set wakeup irq number on nanohub driver */
	chub->data = iio_priv(iio_dev);
	nanohub_mailbox_comms_init(&chub->data->comms);
	chub->pdata = chub->data->pdata;
	chub->pdata->mailbox_client = chub;
	chub->data->irq1 = IRQ_EVT_A2C_WAKEUP;
	chub->data->irq2 = 0;
#endif
#ifdef CONFIG_SENSORS_SSP
	ret = ssp_device_probe(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s failed to probe ssp %d\n",
			__func__, ret);
		goto err;
	}
	ssp_platform_init(chub);
	ssp_set_firmware_name(chub->os_name);
#elif defined(CONFIG_SHUB)
	ret = sensorhub_device_probe(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s failed to probe shub %d\n",
			__func__, ret);
		goto err;
	}
	shub_set_vendor_drvdata(chub);
#endif
	chub->chub_rt_log.loglevel = 0;
	spin_lock_init(&chub->logout_lock);
	atomic_set(&chub->in_use_ipc, 0);
	atomic_set(&chub->chub_status, CHUB_ST_NO_POWER);
	atomic_set(&chub->in_reset, 0);
	chub->powermode = 0; /* updated by fw bl */
	chub->cur_err = 0;
	for (i = 0; i < CHUB_ERR_MAX; i++)
		chub->err_cnt[i] = 0;
	chub->dev = &pdev->dev;
	platform_set_drvdata(pdev, chub);
	contexthub_config_init(chub);

	for (i = 0, ret = 0; i < ARRAY_SIZE(attributes); i++) {
		ret = device_create_file(chub->dev, &attributes[i]);
		if (ret)
			dev_warn(chub->dev, "Failed to create file: %s\n",
				 attributes[i].attr.name);
	}
	init_waitqueue_head(&chub->poweron_lock.event);
	init_waitqueue_head(&chub->reset_lock.event);
	init_waitqueue_head(&chub->read_lock.event);
	init_waitqueue_head(&chub->chub_alive_lock.event);
	atomic_set(&chub->poweron_lock.flag, 0);
	atomic_set(&chub->chub_alive_lock.flag, 0);
	INIT_WORK(&chub->debug_work, handle_debug_work_func);
	INIT_WORK(&chub->log_work, handle_log_work_func);
	chub->log_work_reqcnt = 0;
#ifdef CONFIG_EXYNOS_ITMON
	chub->itmon_nb.notifier_call = chub_itmon_notifier;
	itmon_notifier_chain_register(&chub->itmon_nb);
#endif
	atomic_notifier_chain_register(&panic_notifier_list,
					&chub_panic_notifier);

	/* init fw runtime log */
	chub->chub_rt_log.buffer = vzalloc(SZ_512K * 2);
	if (!chub->chub_rt_log.buffer) {
		ret = -ENOMEM;
		goto err;
	}
	chub->chub_rt_log.buffer_size = SZ_512K * 2;
	chub->chub_rt_log.write_index = 0;

	wakeup_source_init(&chub->ws_reset, "chub_reboot");
	dev_info(chub->dev, "%s with %s FW and %lu clk is done\n",
					__func__, chub->os_name, chub->clkrate);
	return 0;
err:
	if (chub)
		if (need_to_free)
			devm_kfree(&pdev->dev, chub);

	dev_err(&pdev->dev, "%s is fail with ret %d\n", __func__, ret);
	return ret;
}

static int contexthub_ipc_remove(struct platform_device *pdev)
{
	struct contexthub_ipc_info *chub = platform_get_drvdata(pdev);

	wakeup_source_trash(&chub->ws_reset);
#ifdef CONFIG_SENSORS_SSP
	ssp_device_remove(pdev);
#elif defined(CONFIG_SHUB)
	sensorhub_device_shutdown(pdev);
#endif
	return 0;
}

static int contexthub_alive_noirq(struct contexthub_ipc_info *ipc, int ap_state)
{
	int cnt = 100;
	int start_index = ipc_hw_read_int_start_index(AP);
	unsigned int status;
	int irq_num = IRQ_EVT_CHUB_ALIVE + start_index;

	pr_info("%s start\n", __func__);
	ipc_hw_write_shared_reg(AP, ap_state, SR_3);
	ipc_hw_gen_interrupt(AP, IRQ_EVT_CHUB_ALIVE);

	atomic_set(&ipc->chub_alive_lock.flag, 0);
	while (cnt--) {
		mdelay(1);
		status = ipc_hw_read_int_status_reg(AP);
		if (status & (1 << irq_num)) {
			ipc_hw_clear_int_pend_reg(AP, irq_num);
			atomic_set(&ipc->chub_alive_lock.flag, 1);
			pr_info("%s end\n", __func__);
			return 0;
		}
	}
	pr_err("%s alive check fail!!\n", __func__);
	return -1;
}

static int contexthub_suspend_noirq(struct device *dev)
{
	struct contexthub_ipc_info *ipc = dev_get_drvdata(dev);
#ifdef CONFIG_CHRE_SENSORHUB_HAL
	struct nanohub_data *data = ipc->data;
#endif

	if (atomic_read(&ipc->chub_status) != CHUB_ST_RUN)
		return 0;

	dev_info(dev, "nanohub log to kernel off\n");
	ipc_hw_write_shared_reg(AP, AP_SLEEP, SR_3);
	ipc_hw_gen_interrupt(AP, IRQ_EVT_CHUB_ALIVE);

#ifdef CONFIG_CHRE_SENSORHUB_HAL
	return nanohub_suspend(data->iio_dev);
#else
	return 0;
#endif
}

static int contexthub_resume_noirq(struct device *dev)
{
	struct contexthub_ipc_info *ipc = dev_get_drvdata(dev);
#ifdef CONFIG_CHRE_SENSORHUB_HAL
	struct nanohub_data *data = ipc->data;
#endif
	if (atomic_read(&ipc->chub_status) != CHUB_ST_RUN)
		return 0;

	dev_info(dev, "nanohub log to kernel on\n");
	contexthub_alive_noirq(ipc, AP_WAKE);

#ifdef CONFIG_CHRE_SENSORHUB_HAL
	return nanohub_resume(data->iio_dev);
#endif
	return 0;
}

static int contexthub_prepare(struct device *dev)
{
	struct contexthub_ipc_info *ipc = dev_get_drvdata(dev);

	if (atomic_read(&ipc->chub_status) != CHUB_ST_RUN)
		return 0;

	pr_info("%s\n", __func__);
	ipc_hw_write_shared_reg(AP, AP_PREPARE, SR_3);
	ipc_hw_gen_interrupt(AP, IRQ_EVT_CHUB_ALIVE);
#ifdef CONFIG_SENSORS_SSP
	ssp_device_suspend(dev);
#elif defined(CONFIG_SHUB)
	sensorhub_device_prepare(dev);
#endif

	return 0;
}

static void contexthub_complete(struct device *dev)
{
	struct contexthub_ipc_info *ipc = dev_get_drvdata(dev);

	if (atomic_read(&ipc->chub_status) != CHUB_ST_RUN)
		return;

	pr_info("%s irq disabled\n", __func__);
	disable_irq(ipc->irq_mailbox);
	contexthub_alive_noirq(ipc, AP_COMPLETE);
	enable_irq(ipc->irq_mailbox);
#ifdef CONFIG_SENSORS_SSP
	ssp_device_resume(dev);
#elif defined(CONFIG_SHUB)
	sensorhub_device_complete(dev);
#endif
}

//static SIMPLE_DEV_PM_OPS(contexthub_pm_ops, contexthub_suspend, contexthub_resume);
static const struct dev_pm_ops contexthub_pm_ops = {
	.prepare = contexthub_prepare,
	.complete = contexthub_complete,
	.suspend_noirq = contexthub_suspend_noirq,
	.resume_noirq = contexthub_resume_noirq,
};

static const struct of_device_id contexthub_ipc_match[] = {
	{.compatible = "samsung,exynos-nanohub"},
	{},
};

static struct platform_driver samsung_contexthub_ipc_driver = {
	.probe = contexthub_ipc_probe,
	.remove = contexthub_ipc_remove,
	.driver = {
		   .name = "nanohub-ipc",
		   .owner = THIS_MODULE,
		   .of_match_table = contexthub_ipc_match,
		   .pm = &contexthub_pm_ops,
	},
};

int nanohub_mailbox_init(void)
{
	return platform_driver_register(&samsung_contexthub_ipc_driver);
}

static void __exit nanohub_mailbox_cleanup(void)
{
	platform_driver_unregister(&samsung_contexthub_ipc_driver);
}

module_init(nanohub_mailbox_init);
module_exit(nanohub_mailbox_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Exynos contexthub mailbox Driver");
MODULE_AUTHOR("Boojin Kim <boojin.kim@samsung.com>");
