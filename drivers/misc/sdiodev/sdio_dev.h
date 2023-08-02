/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Filename : sdio_dev.h
 * Abstract : This file is a implementation for itm sipc command/event function
 *
 * Authors	: gaole.zhang
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __SDIO_DEV_H__
#define __SDIO_DEV_H__

#include <linux/module.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>
#include <mach/gpio.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/irq.h>
#include <linux/suspend.h>
#include <mach/hardware.h>
#include <mach/sci_glb_regs.h>
#include <mach/sci.h>
#include <mach/board.h>
#include <linux/sprd_iommu.h>
#include <linux/spinlock.h>
#include <asm/system.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/syscalls.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>

typedef unsigned int uint32;
typedef unsigned char uint8;
typedef irqreturn_t (*sdiodev_handler_t)(int, void *);
typedef void (*SDIO_TRAN_CALLBACK)(void);
typedef void (*SDIO_TRAN_CALLBACK_PARA)(int);
typedef struct _sdio_chn_handler
{
	uint32 chn;
	SDIO_TRAN_CALLBACK tran_callback;
	SDIO_TRAN_CALLBACK_PARA tran_callback_para;
}SDIO_CHN_HANDLER;

typedef struct _sleep_policy
{
	struct timer_list gpio_timer;
	uint32 marlin_waketime;
	uint32 gpio_opt_tag; //  1:dont't pull gpio    0:pull gpio
	unsigned long gpioreq_up_time;
	unsigned long gpioreq_need_pulldown;
	unsigned long gpio_up_time;
	unsigned long gpio_down_time;
}SLEEP_POLICY_T;

typedef struct _marlin_sdio_ready
{
	bool marlin_sdio_init_start_tag;
	bool marlin_sdio_init_end_tag;

}MARLIN_SDIO_READY_T;

struct sdio_data {
	u32 wake_ack;
	u32 data_irq;
	u32 wake_out;
	u32 io_ready;
	u32 rfctl_off;
	const char *sdhci;
};

extern volatile bool marlin_mmc_suspend;


#define MARLIN_SDIO_VERSION	"1.0"
#define INVALID_SDIO_CHN 16
#define INVALID_SDIO_WCHN 8

#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))
#define LINUX_VERSION_CODE KERNEL_VERSION(3, 10, 0)
#define BIT_0    0x01
#define SDIODEV_MAX_FUNCS 2
#define SDIODEV_FUNC_0  0
#define SDIODEV_FUNC_1  1
#define MARLIN_VENDOR_ID 0x00
#define MARLIN_DEVICE_ID  0x2331
#define SDIO_CHN_8  0x1 
#define SDIO_CHN_9	0x2
#define SDIO_CHN_11	0x8
#define SDIO_CHN_12	0x10
#define SDIO_CHN_13	0x20

#define SDIO_CHN_14	0x40
#define SDIO_CHN_15	0x80
#define SDIOLOG_CHN  14
#define SDIO_SYNC_CHN 10
#define DOWNLOAD_CHANNEL_READ	12
#define PSEUDO_ATC_CHANNEL_READ	11
#define PSEUDO_ATC_CHANNEL_LOOPCHECK         (15)
#define MARLIN_ASSERTINFO_CHN	(13)


#define WIFI_CHN_8	8
#define WIFI_CHN_9	9


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
#define SMP_BARRIER(x) smp_read_barrier_depends(x)
#else
#define SMP_BARRIER(x) smp_rmb(x)
#endif



#define SDIOTRAN_HEADER "[SDIOTRAN]"

#define SDIOTRAN_ERR(fmt,args...)	\
	pr_err(SDIOTRAN_HEADER "%s:" fmt "\n", __func__, ## args)

#define SDIOTRAN_DEBUG(fmt,args...)	\
	pr_debug(SDIOTRAN_HEADER "%s:" fmt "\n", __func__, ## args)

int set_wlan_status(int status);

int set_marlin_wakeup(uint32 chn,uint32 user_id);
int set_marlin_sleep(uint32 chn,uint32 user_id);



#if 0//#if !defined(CONFIG_MARLIN_NO_SLEEP)

#define MARLIN_PM_RESUME_WAIT_INIT(a) DECLARE_WAIT_QUEUE_HEAD(a);
#define _MARLIN_PM_RESUME_WAIT(a, b) do {\
		int retry = 0; \
		SMP_BARRIER(); \
		while (marlin_mmc_suspend && retry++ != b) { \
			SMP_BARRIER(); \
			wait_event_interruptible_timeout(a, !marlin_mmc_suspend, HZ/100); \
		} \
	} while (0)
#define MARLIN_PM_RESUME_WAIT(a)		_MARLIN_PM_RESUME_WAIT(a, 200)
#define MARLIN_PM_RESUME_WAIT_FOREVER(a)	_MARLIN_PM_RESUME_WAIT(a, ~0)
#define MARLIN_PM_RESUME_RETURN_ERROR(a)	do { \
	if (marlin_mmc_suspend) {\
		SDIOTRAN_ERR("ERR!!!");\
		return a;} \
	} while (0)
#else
#define MARLIN_PM_RESUME_WAIT_INIT(a) 
#define _MARLIN_PM_RESUME_WAIT(a, b)
#define MARLIN_PM_RESUME_WAIT(a)		
#define MARLIN_PM_RESUME_WAIT_FOREVER(a)
#define MARLIN_PM_RESUME_RETURN_ERROR(a)
#endif





int  sdio_dev_chn_idle(uint32 chn);
int  sdio_dev_get_chn_datalen(uint32 chn);
int  sdio_dev_get_read_chn(void);
bool get_sdiohal_status(void);
bool get_apsdiohal_status(void);
void marlin_sdio_sync_uninit(void);
int  sdio_dev_write(uint32 chn,void* data_buf,uint32 count);
int  sdio_dev_read(uint32 chn,void* read_buf,uint32 *count);
int sdiodev_readchn_init(uint32 chn,void* callback,bool with_para);
int sdiodev_readchn_uninit(uint32 chn);
void set_sprd_download_fin(int dl_tag);
int marlin_sdio_init(void);
bool get_sprd_marlin_status(void);

void set_blklen(int blklen);
void flush_blkchn(void);


#endif
