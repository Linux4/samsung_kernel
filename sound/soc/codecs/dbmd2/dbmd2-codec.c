/*
 * DSPG DBMD2 codec driver
 *
 * Copyright (C) 2014 DSP Group
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* #define DEBUG */
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/serial_core.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/kthread.h>
#include <linux/kfifo.h>
#include <linux/vmalloc.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/spi/spi.h>
#include <linux/wait.h>

#include "sbl.h"
#include "dbmd2-export.h"

#include <linux/platform_data/msm_serial_hs.h>

#define MSM_G1
//#define USE_WAKEUP_FROM_UART 1
#define USE_WAKEUP_GPIO_FOR_DETECTION 1
#define USE_SLEEPMODE_GPIO_FOR_WAKEUP 1

/* This register has changed - the buffer size is no longer specified.
 * Instead set it either to 0 (use part of D2's memory for noise reduction) or
 * 1 (use complete memory for audio buffering). */
#define MAX_AUDIO_BUFFER_SIZE		1 /* complete memory available */
/* Size must be power of 2 */
#define MAX_KFIFO_BUFFER_SIZE		(131072*2) /* >4 seconds */
#define MAX_RETRIES_TO_WRITE_TOBUF	3
#define MAX_AMODEL_SIZE			(128*1024)

struct dbd2_platform_data {
	unsigned gpio_wakeup;
	unsigned gpio_reset;
#ifndef USE_WAKEUP_FROM_UART
	unsigned gpio_sv;
	int sv_irq;
#endif
	int irq_inuse;
};

/* To support sysfs node  */
static struct class *ns_class;
static struct device *dbd2_dev, *gram_dev, *net_dev;

#ifndef USE_WAKEUP_FROM_UART
irqreturn_t dbmd2_sv_interrupt(int irq, void *dev);
#endif


#define DRIVER_VERSION			"1.520"
#define DBD2_FIRMWARE_NAME		"dbd2_fw.bin"
#define DBD2_MODEL_NAME			"A-Model.bin"

#define DBD2_GRAM_NAME			"voice_grammar.bin"
#define DBD2_NET_NAME			"voice_net.bin"

#define MAX_CMD_SEND_SIZE		32
#define MAX_CMD_SPI_SEND_SIZE		16
#define RETRY_COUNT			5
/* UART probe retry count in seconds */
#define PROBE_RETRY_COUNT		30

/* DBD2 commands and values */

#define DBD2_SYNC_POLLING				0x80000000

#define DBD2_SET_POWER_STATE_SLEEP			0x80100001

#define DBD2_GET_FW_VER					0x80000000
#define DBD2_OPR_MODE					0x80010000
#define DBD2_TG_THERSHOLD				0x80020000
#define DBD2_VERIFICATION_THRESHOLD			0x80030000
#define DBD2_GAIN_SHIFT_VALUE				0x80040000
#define DBD2_IO_PORT_ADDR_LO				0x80050000
#define DBD2_IO_PORT_ADDR_HI				0x80060000
#define DBD2_IO_PORT_VALUE_LO				0x80070000
#define DBD2_IO_PORT_VALUE_HI				0x80080000
#define DBD2_AUDIO_BUFFER_SIZE				0x80090000
#define DBD2_NUM_OF_SMP_IN_BUF				0x800A0000
#define DBD2_LAST_MAX_SMP_VALUE				0x800B0000
#define DBD2_LAST_DETECT_WORD_NUM			0x800C0000
#define DBD2_DETECT_TRIGER_LEVEL			0x800D0000
#define DBD2_DETECT_VERIFICATION_LEVEL			0x800E0000
#define DBD2_LOAD_NEW_ACUSTIC_MODEL			0x800F0000
#define DBD2_PRESENT_CLOCK_CONFIG			0x80100000
#define DBD2_UART_XON					0x80110000
#define DBD2_AUDIO_BUFFER_CONVERSION			0x80120000
#define DBD2_AUDIO_HISTORY				0x80120000
#define DBD2_UART_XOFF					0x80130000
#define DBD2_LAST_DURATION				0x80140000
#define DBD2_LAST_ERROR					0x80150000
#define DBD2_MIC_GAIN					0x80160000
#define DBD2_WAKEUP_FROM_UART				0x80170000
#define DBD2_FW_ID					0x80190000
#define DBD2_FAST_CLK_FREQ				0x801B0000
#define DBD2_POST_TRIGGER_AUDIO_BUF			0x80200000
#define DBD2_POST_DETECTION_CLOCK_CONFIG		0x80210000
#define DBD2_GPIO_CFG					0x80220000

#define DBD2_MICROPHONE1_CONFIGURATION			0x80240000
#define DBD2_MICROPHONE2_CONFIGURATION			0x80250000

#define DBD2_SET_D2PARAM_ADDR				0x801C0000
#define DBD2_GET_D2PARAM				0x80270000
#define DBD2_SET_D2PARAM				0x80260000

#define DBD2_EXIT_FW_SLEEP_MODE_GPIO			0x802A0000

#define DBD2_D2PARAM_WORDID             0x54
#define DBD2_D2PARAM_ALTWORDID          0x55

#define DBD2_READ_CHECKSUM				0x805A0E00
#define DBD2_FIRMWARE_BOOT				0x805A0B00

#define DBD2_8KHZ				0x0008

#define DBD2_AUDIO_MODE_PCM			0
#define DBD2_AUDIO_MODE_MU_LAW			0x1000

#define UART_TTY_MAX_WRITE_SZ			4096

#define UART_HW_FIFO_SIZE			16

#define UART_TTY_WRITE_SZ			8
#define UART_TTY_READ_SZ			UART_TTY_MAX_READ_SZ

#ifdef MSM_G1
#define UART_TTY_MAX_READ_SZ			512
#define UART_TTY_BOOT_BAUD_RATE			115200
#define UART_SYNC_LENGTH			1000 /* in msec */
#define TTY_MAX_HW_BUF_SIZE			512
#else
#define UART_TTY_MAX_READ_SZ			512
#define UART_TTY_BOOT_BAUD_RATE			115200
#define UART_SYNC_LENGTH			300 /* in msec */
#define TTY_MAX_HW_BUF_SIZE			2048
#endif
#define UART_TTY_STOP_BITS			1
#define UART_TTY_BOOT_STOP_BITS			2
#define UART_TTY_MAX_EAGAIN_RETRY		50


#define DETECTION_MODE_PHRASE			0
#define DETECTION_MODE_VOICE_ENERGY		0x10
#define DMD2_OPERATION_MODE_MASK		0x0f

#define DBMD2_EXITSLEEP_ENABLE			0x80
#define DBMD2_EXITSLEEP_DISABLE			0x00

#ifdef MSM_G1
#define DBMD2_EXITSLEEP_GPIO			0x0A
#else
#define DBMD2_EXITSLEEP_GPIO			0x07
#endif

/* DBD2 intrnal data stucture */


struct dbd2_data {
	struct dbd2_platform_data		pdata;
	struct platform_device			*pdev;
	struct device				*dev;
	struct i2c_client			*client;
	struct spi_device			*client_spi;
	const struct firmware			*fw;
	struct mutex				lock;
	bool					asleep;
	bool					device_ready;
	bool					change_speed;
	struct clk				*clk;
	struct work_struct			sv_work;
	struct work_struct			uevent_work;
	unsigned int				audio_buffer_size;
	unsigned int				audio_mode;
	unsigned int				bytes_per_sample;
	dev_t					record_chrdev;
	struct cdev				record_cdev;
	struct device				*record_dev;
	int					audio_processed;
	char					*amodel_fw_name;
	struct tty_struct			*uart_tty;
	struct file				*uart_file;
	struct tty_ldisc			*uart_ld;
	const char				*uart_dev;
	int					uart_open;
	struct kfifo				pcm_kfifo;
	int					a_model_loaded;
	atomic_t				audio_owner;
#ifdef USE_WAKEUP_FROM_UART
	atomic_t				do_detection;
#endif
	int					auto_buffering;
	int					auto_detection;
	int					buffering;
	struct input_dev			*input;
	char					*amodel_buf;
	int					amodel_len;
	char					*gram_data, *net_data;
	size_t					gram_size, net_size;
	int					detection_state;
	int					irq_on;
	struct firmware				*dspg_gram;
	struct firmware				*dspg_net;
	unsigned long				rxsize;
	unsigned long				rsize;
	unsigned long				wsize;
	atomic_t				stop_uart_probing;
	struct task_struct			*uart_probe_thread;
	struct uart_port			*uport;
	struct completion			uart_done;
	void					(*event_callback)(int);
	int					firmware_retries;
	u32					freq; /* fast/high clock */
	int					channels;
	u16					clock_config[3];
	u32					uart_speed[3];
	u16					mic_config[3];
	u32					init_mic_config;
	u32					gpio_reg_config;
	u32					detection_mode;
	u32					lpm_mode;
	atomic_t				uart_tty_enabled;
	atomic_t				keep_uart_tty_enabled;
	atomic_t				uart_prt_counter;
#ifdef USE_WAKEUP_FROM_UART
	wait_queue_head_t			irq_wq;
	struct task_struct			*detection_thread;
#endif
};

/* Global Variables */

struct dbd2_data *dbd2_data;

extern unsigned int system_rev;

/* function definition */

static int dbd2_reset(struct dbd2_data *dbd2);
static int dbd2_load_firmware(struct dbd2_data *dbd2);
static int dbd2_send_cmd(struct dbd2_data *dbd2, u32 command, u16 *response);
static int dbd2_send_cmd_short(struct dbd2_data *dbd2, u32 command,
			       u16 *response);
static int dbmd2_common_probe(struct dbd2_data *dbd2);
static int dbmd2_uart_read(struct dbd2_data *dbd2, u8 *buf, int len);
static int dbmd2_uart_read_sync(struct dbd2_data *dbd2, u8 *buf, int len);
static int dbmd2_uart_write_sync(struct dbd2_data *dbd2, const u8 *buf,
				 int len);
static void dbmd2_uart_close_file(struct dbd2_data *dbd2);
static int dbmd2_uart_open_file(struct dbd2_data *dbd2);
static int dbmd2_uart_open_file_only(struct dbd2_data *dbd2);
static int dbmd2_uart_configure_tty(struct dbd2_data *dbd2,
				    struct tty_struct *tty, u32 bps, int stop,
				    int parity, int flow);
static int dbd2_wait_till_alive(struct dbd2_data *dbd2);
static int dbd2_set_uart_speed(struct dbd2_data *dbd2, int index);
static int dbd2_set_uart_speed1(struct dbd2_data *dbd2, int index);

static int dbd2_set_microphone_mode(struct dbd2_data *dbd2, int mode);

static int dbd2_wake(struct dbd2_data *dbd2);

static void dbd2_clk_enable(bool enable)
{
	pr_info("%s start (%s)\n", __func__, enable ? "ON" : "OFF");
	dbd2_data->clk = clk_get(dbd2_data->dev, "dbmd2_clk");
	if (IS_ERR(dbd2_data->clk))
		return;
	if (enable) {
		int rc;
		rc = clk_prepare_enable(dbd2_data->clk);
		if (rc < 0)
			pr_info("%s: clk_prepare_enable failed\n", __func__);
	} else {
		clk_disable_unprepare(dbd2_data->clk);
		clk_put(dbd2_data->clk);
	}
}
static void dbd2_uart_tty_enable(bool enable)
{
	if (!dbd2_data)
		return;
	if (enable) {
		if (atomic_read(&dbd2_data->uart_tty_enabled) == 0) {
			atomic_set(&dbd2_data->uart_tty_enabled,1);
			atomic_inc(&dbd2_data->uart_prt_counter);
			if (atomic_read(&dbd2_data->uart_prt_counter) > 1)
				dev_err(dbd2_data->dev,
					"%s: uart_prt_counter >1 after open\n",
					__func__);
			if (!(dbd2_data->uart_open))
				dbmd2_uart_open_file_only(dbd2_data);

		}
	} else {
		if (atomic_read(&dbd2_data->keep_uart_tty_enabled) == 0) {
			if (atomic_read(&dbd2_data->uart_tty_enabled) == 1) {
				if ((dbd2_data->uart_open))
					dbmd2_uart_close_file(dbd2_data);

				atomic_set(&dbd2_data->uart_tty_enabled,0);
				atomic_dec(&dbd2_data->uart_prt_counter);
				if (atomic_read(&dbd2_data->uart_prt_counter) != 0)
					dev_err(dbd2_data->dev,
					"%s: uart_prt_counter !=0 after close\n",
					__func__);
			}

		}
	}
}

static void uart_lock(struct mutex *lock)
{
	mutex_lock(lock);
	dbd2_uart_tty_enable(1);
}

static void uart_unlock(struct mutex *lock)
{
	dbd2_uart_tty_enable(0);
	mutex_unlock(lock);
}

static void dbd2_flush_rx_fifo(struct dbd2_data *dbd2)
{
	if (!dbd2->uart_open) {
		dev_err(dbd2->dev, "%s: Uart is not opened !!!\n", __func__);
		return;
	}

	tty_buffer_flush(dbd2->uart_tty);
	tty_ldisc_flush(dbd2->uart_tty);
}

enum dbd2_uart_speeds {
	DBD2_UART_SPEED_57600 = 0,
	DBD2_UART_SPEED_1000000,
	DBD2_UART_SPEED_3000000,
	DBD2_UART_SPEEDS,
};

static int dbd2_buf_to_int(const char *buf)
{
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	return (int)val;
}

static int dbd2_set_bytes_per_sample(struct dbd2_data *dbd2, unsigned int mode)
{
	int ret;
	u16 history;
	u16 reg_val;

	/* Changing the buffer conversion causes trouble: adapting the
	 * UART baudrate doesn't work anymore (firmware bug?) */
	if (((mode == DBD2_AUDIO_MODE_PCM) && (dbd2->bytes_per_sample == 2)) ||
	    ((mode == DBD2_AUDIO_MODE_MU_LAW) &&
	     (dbd2->bytes_per_sample == 1)))
		return 0;

	ret = dbd2_send_cmd_short(dbd2, DBD2_AUDIO_BUFFER_CONVERSION, &history);

	if (ret < 0) {
		dev_err(dbd2->dev, "%s: failed to read register: %d\n",
				__func__, ret);
		return ret;
	}

	/* Keep history configuration */
	history &= 0x0fff;

	reg_val = (((u16)mode | history) & 0xffff);

	dev_info(dbd2->dev, "%s: setting audio_buf_conv register to 0x%x\n"
		, __func__, (u32)reg_val);


	ret = dbd2_send_cmd(dbd2,
				  DBD2_AUDIO_BUFFER_CONVERSION | reg_val ,
				  NULL);
	if (ret < 0) {
		dev_err(dbd2->dev,
			"failed to set DBD2_AUDIO_BUFFER_CONVERSION\n");
		return ret;
	}

	switch (mode) {
	case DBD2_AUDIO_MODE_PCM:
		dbd2->bytes_per_sample = 2;
		dbd2->audio_mode = mode;
		break;
	case DBD2_AUDIO_MODE_MU_LAW:
		dbd2->bytes_per_sample = 1;
		dbd2->audio_mode = mode;
		break;
	default:
		break;
	}

	return 0;
}

static int dbd2_set_backlog_len(struct dbd2_data *dbd2, u16 history)
{
	int ret;
	u16 mode;

	history &= 0x0fff;

	mode = ((u16)dbd2->audio_mode | history);

	dev_info(dbd2->dev, "%s: history 0x%x, mode 0x%x\n",
		__func__, (u32)history, (u32)mode);

	ret = dbd2_send_cmd(dbd2,
				  DBD2_AUDIO_BUFFER_CONVERSION | mode ,
				  NULL);
	if (ret < 0) {
		dev_err(dbd2->dev,
			"failed to set backlog size\n");
		return ret;
	}

	return 0;
}


static int dbd2_sleeping(struct dbd2_data *dbd2)
{
	return dbd2->asleep;
}

enum dbmd2_states {
	DBMD2_IDLE = 0,
	DBMD2_DETECTION,
	DBMD2_RESERVED,
	DBMD2_BUFFERING,
	DBMD2_SLEEP_PLL_ON,
	DBMD2_SLEEP_PLL_OFF,
	DBMD2_HIBERNATE,
};

static int dbd2_get_mode(struct dbd2_data *dbd2)
{
	int ret;
	u16 state;

	ret = dbd2_send_cmd_short(dbd2, DBD2_OPR_MODE, &state);
	if (ret < 0) {
		dev_err(dbd2->dev, "failed to read DBD2_OPR_MODE\n");
		return ret;
	}

	state &= DMD2_OPERATION_MODE_MASK;

	return (int)state;
}

static int dbd2_set_mode(struct dbd2_data *dbd2, int mode)
{
	int ret;

	/* set new mode and return old one */

	dev_info(dbd2->dev, "%s: mode: %d\n", __func__, mode);

	/* nothing to do */
	if (dbd2_sleeping(dbd2) && mode == DBMD2_SLEEP_PLL_OFF)
		return DBMD2_SLEEP_PLL_OFF;

	/* wakeup chip */
	ret = dbd2_wake(dbd2);
	if (ret)
		return -EIO;

	atomic_set(&dbd2->keep_uart_tty_enabled,1);

	dbd2->buffering = 0;

	dbd2->pdata.irq_inuse = 0;
#ifdef USE_WAKEUP_FROM_UART
	if (atomic_read(&dbd2->do_detection) == 1) {
		atomic_set(&dbd2->do_detection, 0);
		msleep(15);
	}
#endif

	/* anything special to do */
	switch (mode) {
	case DBMD2_HIBERNATE:
		dbd2->asleep = true;
#ifndef USE_WAKEUP_GPIO_FOR_DETECTION
		gpio_set_value(dbd2->pdata.gpio_wakeup, 0);
#endif
		break;
	case DBMD2_IDLE:

		break;
	case DBMD2_BUFFERING:
		ret = dbd2_set_uart_speed(dbd2, DBD2_UART_SPEED_1000000);
		if (ret) {
			dev_err(dbd2->dev, "failed switch to higher speed\n");
		}
		break;
	case DBMD2_DETECTION:
		dbd2->pdata.irq_inuse = 1;
		mode |= dbd2->detection_mode;
#ifdef USE_WAKEUP_FROM_UART
		atomic_set(&dbd2->do_detection, 1);
		wake_up_interruptible(&dbd2->irq_wq);
#endif
		atomic_set(&dbd2->keep_uart_tty_enabled,0);
		break;
	case DBMD2_SLEEP_PLL_ON:
		break;
	case DBMD2_SLEEP_PLL_OFF:
#if (USE_SLEEPMODE_GPIO_FOR_WAKEUP)
		dbd2->asleep = true;
		gpio_set_value(dbd2->pdata.gpio_wakeup, 0);
#endif
		atomic_set(&dbd2->keep_uart_tty_enabled,0);
		break;

	default:
		break;
	}

	/* set operation mode register */
	ret = dbd2_send_cmd(dbd2, DBD2_OPR_MODE |
			    (mode & 0xffff), NULL);
	if (ret < 0)
		dev_err(dbd2->dev, "failed to set mode 0x%x\n", mode);

	if (mode == DBMD2_BUFFERING) {
		dbd2->buffering = 1;
		schedule_work(&dbd2->sv_work);
	}
	if (mode != DBMD2_SLEEP_PLL_OFF) {
		if ((mode & DMD2_OPERATION_MODE_MASK) != dbd2_get_mode(dbd2))
				dev_err(dbd2->dev, "failed to set mode !!!\n");

	}

	return 0;
}



#if !USE_WAKEUP_GPIO_FOR_DETECTION
static int dbd2_wake_from_wakeup_gpio(struct dbd2_data *dbd2)
{
	int ret = 0;
	u16 fwver = 0xffff;

	/* if chip not sleeping there is nothing to do */
	if (!dbd2_sleeping(dbd2))
		return 0;
	/* deassert wake pin */
	gpio_set_value(dbd2->pdata.gpio_wakeup, 0);
	/* give some time to wakeup */
	usleep_range(5000, 6000);
	/* make it not sleeping */
	dbd2->asleep = false;

	/* test if firmware is up */
	ret = dbd2_send_cmd_short(dbd2, DBD2_GET_FW_VER, &fwver);
	if (ret < 0) {
		dev_err(dbd2->dev, "sync error did not not wakeup\n");
		/* make it not sleeping */
		dbd2->asleep = true;
		return -EIO;
	}
	/* assert gpio pin */
	gpio_set_value(dbd2->pdata.gpio_wakeup, 1);

	dev_info(dbd2->dev, "%s: wake up\n", __func__);
	return 0;
}
#endif

#if USE_SLEEPMODE_GPIO_FOR_WAKEUP
static int dbd2_wake_from_exitfromsleep_gpio(struct dbd2_data *dbd2)
{
	int mode;
	int retries = RETRY_COUNT;

	/* if chip not sleeping there is nothing to do */
	if (!dbd2_sleeping(dbd2))
		return 0;

	/* deassert wake pin */
	gpio_set_value(dbd2->pdata.gpio_wakeup, 1);
	/* give some time to wakeup */
	//usleep_range(5000, 6000);
	/* make it not sleeping */
	dbd2->asleep = false;

	do {
		msleep(200);

		mode = dbd2_get_mode(dbd2);

		if (mode < 0) {
			dev_dbg(dbd2->dev, "wakeup, readmode retries left %d\n",
				retries);
		}
	} while (mode < 0 && --retries > 0);

	if (retries == 0) {
		dev_err(dbd2->dev, "Unable to read mode after wakeup\n");
		/* make it not sleeping */
		dbd2->asleep = true;
		gpio_set_value(dbd2->pdata.gpio_wakeup, 0);
		return -EIO;
	}


	if (mode != DBMD2_IDLE) {
		dev_err(dbd2->dev, "FW woke up not in IDLE mode (ret %d)\n"
			, mode);
		/* make it not sleeping */
		dbd2->asleep = true;
		gpio_set_value(dbd2->pdata.gpio_wakeup, 0);
		return -EIO;
	}

	atomic_set(&dbd2->keep_uart_tty_enabled,1);

	dev_info(dbd2->dev, "\n%s: woke up\n", __func__);
	return 0;
}
#endif


static int dbd2_wake(struct dbd2_data *dbd2)
{
#if USE_SLEEPMODE_GPIO_FOR_WAKEUP
	return dbd2_wake_from_exitfromsleep_gpio(dbd2);
#elif !USE_WAKEUP_GPIO_FOR_DETECTION
	return dbd2_wake_from_wakeup_gpio(dbd2);
#else
	return 0;
#endif
}

static int dbd2_wait_till_alive(struct dbd2_data *dbd2)
{
	u16 result;
	int ret = 0;
	unsigned long stimeout = jiffies + msecs_to_jiffies(1000);

	msleep(100);

	if (!dbd2->client && !dbd2->client_spi) {
		dbd2_flush_rx_fifo(dbd2);

		/* Poll to wait for firmware completing its wakeup procedure:
		 * Read the firmware ID number (0xdbd2) */
		do {
			ret = dbd2_send_cmd_short(dbd2, DBD2_FW_ID, &result);
			if (ret < 0) {
				/* dev_err(dbd2->dev,
					"failed to read firmware id\n"); */
				continue;
			}
			if (result == 0xdbd2)
				ret = 0;
			else
				ret = -1;
		} while (time_before(jiffies, stimeout) && ret != 0);
		if (ret != 0)
			dev_err(dbd2->dev, "failed to read firmware id\n");
		ret = (ret >= 0 ? 1 : 0);
		if (!ret)
			dev_err(dbd2->dev, "%s(): failed = 0x%d\n", __func__,
				ret);
	} else {
		int state;
		int timeout = 2000000; /* 2s timeout */
		do {
			state = dbd2_get_mode(dbd2);
			/* wait around 50ms if state not changed */
			/* XXX reconsider this */
			if (state < 0) {
				usleep_range(50000, 51000);
				timeout -= 50000;
			}
		} while (state < 0 && timeout > 0);
		ret = (state < 0 ? 0 : 1);
	}

	return ret;
}


static int dbmd2_spi_configure(struct dbd2_data *dbd2, int speed)
{
	struct spi_device *spi = dbd2->client_spi;
	int ret = 0;

/* uart_lock(&dbd2->lock); */
	spi->max_speed_hz = speed;
	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(dbd2->dev, "dbmd2_spi_configure() failed %x\n", ret);
		return -EIO;
	}

	dev_info(dbd2->dev, "dbmd2_spi_configure() speed %d hZ\n", speed);
	spi_dev_put(spi);
/* uart_lock(&dbd2->lock); */
	return ret;

}

static int dbmd2_spi_read(struct dbd2_data *dbd2, void *buf, int len)
{
	struct spi_device *spi = dbd2->client_spi;
	int rc;

	rc = spi_read(spi, buf, len);

	if (rc < 0) {
		dev_err(dbd2->dev, "%s(): error %d reading SR\n",
			__func__, rc);
		return rc;
	}
	return rc;
}

static int dbmd2_spi_write(struct dbd2_data *dbd2, const void *buf, int len)
{
	struct spi_device *spi = dbd2->client_spi;
	int rc;

	rc = spi_write(spi, buf, len);
	if (rc != 0)
		dev_err(dbd2->dev, "%s(): error %d writing SR\n",
				__func__, rc);
	return rc;
}

static int dbmd2_uart_read(struct dbd2_data *dbd2, u8 *buf, int len)
{
	mm_segment_t oldfs;
	int rc;
	int bytes_to_read = len;
	int retry = UART_TTY_MAX_EAGAIN_RETRY;
	/* XXX hack */
	unsigned long us_per_transfer = (1000000 * 10) / (57600 + 28800);

	if (!dbd2->uart_open) {
		dev_err(dbd2->dev, "%s: Uart is not opened !!!\n", __func__);
		return -EIO;
	}

	us_per_transfer *= (unsigned long)bytes_to_read;

	/* we may call from user context via char dev, so allow
	 * read buffer in kernel address space */
	oldfs = get_fs();
	set_fs(KERNEL_DS);

	do {
		rc = dbd2->uart_ld->ops->read(dbd2->uart_tty,
					      dbd2->uart_file,
					      (char __user *)buf,
					      bytes_to_read);
		if (rc < 0)
			usleep_range(us_per_transfer, us_per_transfer + 1);
		retry--;
	} while (rc == -EAGAIN && retry);

	/* restore old fs context */
	set_fs(oldfs);

	return rc;
}

static int dbmd2_uart_write(struct dbd2_data *dbd2, const u8 *buf, int len)
{
	int ret = 0;
	unsigned long count_remain = len;
	int bytes_wr = 0;
	mm_segment_t oldfs;

	if (!dbd2->uart_open) {
		dev_err(dbd2->dev, "%s: Uart is not opened !!!\n", __func__);
		return -EIO;
	}

	/* we may call from user context via char dev, so allow
	 * read buffer in kernel address space */
	oldfs = get_fs();
	set_fs(KERNEL_DS);

	while (count_remain > 0) {
		while (tty_write_room(dbd2->uart_tty) <
		       min(dbd2->wsize, count_remain))
			usleep_range(2000, 2100);

		ret = dbd2->uart_ld->ops->write(dbd2->uart_tty,
						dbd2->uart_file,
						(char __user *)buf + bytes_wr,
						min(dbd2->wsize,
						    count_remain));
		if (ret < 0) {
			bytes_wr = ret;
			goto err_out;
		}

		bytes_wr += ret;
		count_remain -= ret;
	}

err_out:
	set_fs(oldfs);

	return bytes_wr;
}

static int dbmd2_uart_read_sync(struct dbd2_data *dbd2, u8 *buf, int len)
{
	mm_segment_t oldfs;
	int rc;
	int i = 0;
	int bytes_to_read = len;
	unsigned long timeout;

	timeout = jiffies + msecs_to_jiffies(1000);

	if (!dbd2->uart_open) {
		dev_err(dbd2->dev, "%s: Uart is not opened !!!\n", __func__);
		return -ENODEV;
	}

	/* we may call from user context via char dev, so allow
	 * read buffer in kernel address space */
	oldfs = get_fs();
	set_fs(KERNEL_DS);

	do {
		rc = dbd2->uart_ld->ops->read(dbd2->uart_tty,
					      dbd2->uart_file,
					      (char __user *)buf + i,
					      bytes_to_read);
		if (rc <= 0)
			continue;
		bytes_to_read -= rc;
		i += rc;
	} while (time_before(jiffies, timeout) && bytes_to_read);

	/* restore old fs context */
	set_fs(oldfs);

	if (bytes_to_read)
		return -EIO;

	return len;
}

static int dbmd2_uart_write_sync(struct dbd2_data *dbd2, const u8 *buf, int len)
{
	int ret = 0;
	unsigned long count_remain = len;
	int bytes_wr = 0;
	unsigned int count;
	mm_segment_t oldfs;

	if (!dbd2->uart_open)
		return -EIO;

	/* we may call from user context via char dev, so allow
	 * read buffer in kernel address space */
	oldfs = get_fs();
	set_fs(KERNEL_DS);

	while (count_remain > 0) {
		if (count_remain > dbd2->wsize)
			count = dbd2->wsize;
		else
			count = count_remain;
		/* block until tx buffer space is available */
		while (tty_write_room(dbd2->uart_tty) < count)
			usleep_range(100, 110);

		ret = dbd2->uart_ld->ops->write(dbd2->uart_tty,
						dbd2->uart_file,
						buf + bytes_wr,
						min(dbd2->wsize,
						count_remain));

		if (ret < 0) {
			bytes_wr = ret;
			goto err_out;
		}

		bytes_wr += ret;
		count_remain -= ret;
	}

	tty_wait_until_sent(dbd2->uart_tty, 0);
	usleep_range(50, 60);

err_out:
	/* restore old fs context */
	set_fs(oldfs);

	return bytes_wr;
}


static int dbmd2_uart_configure_tty(struct dbd2_data *dbd2,
			struct tty_struct *tty, u32 bps, int stop, int parity,
			int flow)
{
	int rc = 0;
	struct ktermios termios;

	if (!dbd2->uart_open) {
		dev_err(dbd2->dev, "%s: Uart is not opened !!!\n", __func__);
		return -EIO;
	}

	memcpy(&termios, &(tty->termios), sizeof(termios));

	tty_wait_until_sent(tty, 0);
	usleep_range(50, 60);

	/* clear csize, baud */
	termios.c_cflag &= ~(CBAUD | CSIZE | PARENB | CSTOPB);
	termios.c_cflag |= BOTHER; /* allow arbitrary baud */
	termios.c_cflag |= CS8;
	if (parity)
		termios.c_cflag |= PARENB;

	if (stop == 2)
		termios.c_cflag |= CSTOPB;

	/* set uart port to raw mode (see termios man page for flags) */
	termios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
		| INLCR | IGNCR | ICRNL | IXON | IXOFF);

	if (flow)
		termios.c_iflag |= IXOFF; /* enable XON/OFF for input */

	termios.c_oflag &= ~(OPOST);
	termios.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

	/* set baud rate */
	termios.c_ospeed = bps;
	termios.c_ispeed = bps;

	/* XXX */
	set_bit(TTY_HW_COOK_IN, &tty->flags);

	rc = tty_set_termios(tty, &termios);
/* tty->low_latency = 1 ; */
	return rc;
}

/* This function sets the uart speed and also can set the software flow
 * control according to the define */
static int dbd2_set_uart_speed1(struct dbd2_data *dbd2, int index)
{
	if (dbd2->client || dbd2->client_spi)
		return 0;

	/* set baudrate to FW baud (common case) */
	dbmd2_uart_configure_tty(dbd2,
				 dbd2->uart_tty,
				 dbd2->uart_speed[index],
				 UART_TTY_STOP_BITS,
				 0, 0);

	dbd2_flush_rx_fifo(dbd2);

	return 0;
}

/* this set the uart speed no flow control  */

static int dbd2_set_uart_speed(struct dbd2_data *dbd2, int index)
{
	int rvalue = -EIO;
	int ret;
	int state = DBMD2_IDLE;

	if (dbd2->client || dbd2->client_spi)
		return 0;

	if (index > DBD2_UART_SPEED_1000000) {
		/* A speed of 3Mbaud available only in sleep mode */
		state = dbd2_get_mode(dbd2);
		if (state < 0) {
			dev_err(dbd2->dev, "device not responding\n");
			goto out;
		}
		ret = dbd2_set_mode(dbd2, DBMD2_SLEEP_PLL_ON);
		if (ret) {
			dev_err(dbd2->dev,
				"failed to set device to sleep mode\n");
			goto out;
		}
	}

	ret = dbd2_send_cmd(dbd2, DBD2_PRESENT_CLOCK_CONFIG |
			    dbd2->clock_config[index], NULL);
	if (ret < 0) {
		dev_err(dbd2->dev, "could not set new uart speed\n");
		goto out_change_mode;
	}

	usleep_range(10000, 11000);

	/* set baudrate to FW baud (common case) */
	dbmd2_uart_configure_tty(dbd2,
				 dbd2->uart_tty,
				 dbd2->uart_speed[index],
				 UART_TTY_STOP_BITS,
				 0, 0);

	dbd2_flush_rx_fifo(dbd2);

	ret = dbd2_wait_till_alive(dbd2);
	if (!ret) {
		dev_dbg(dbd2->dev, "device not responding\n");
		goto out;
	}

	rvalue = 0;
	goto out;
out_change_mode:
	if (index > DBD2_UART_SPEED_1000000) {
		/* leave sleep mode */
		ret = dbd2_set_mode(dbd2, state);
		if (ret) {
			dev_err(dbd2->dev, "failed to set old mode\n");
			goto out;
		}
	}
	dbd2_flush_rx_fifo(dbd2);
out:
	return rvalue;
}

static void dbmd2_uart_close_file(struct dbd2_data *dbd2)
{
	if (dbd2->uart_probe_thread) {
		atomic_inc(&dbd2->stop_uart_probing);
		kthread_stop(dbd2->uart_probe_thread);
		dbd2->uart_probe_thread = NULL;
	}
	if (dbd2->uart_open) {
		tty_ldisc_deref(dbd2->uart_ld);
		filp_close(dbd2->uart_file, 0);
		dbd2->uart_open = 0;
	}
	atomic_set(&dbd2->stop_uart_probing, 0);
}

static int dbmd2_uart_open_file_only(struct dbd2_data *dbd2)
{
	int ret = 0;
	struct file *fp;
	int attempt = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(1000);

	if (dbd2->uart_open)
		return 0;

	/*
	 * Wait for the device node to appear in the filesystem. This can take
	 * some time if the kernel is still booting up and filesystems are
	 * being mounted.
	 */
	do {
		if (attempt > 0)
			msleep(50);
#if 0
		dev_info(dbd2->dev,
			"%s(): probing for tty on %s (attempt %d)\n",
			 __func__, dbd2->uart_dev, ++attempt);
#endif
		fp = filp_open(dbd2->uart_dev, O_RDWR | O_NONBLOCK | O_NOCTTY,
			       0);

		ret = PTR_ERR(fp);
	} while (time_before(jiffies, timeout) && IS_ERR_OR_NULL(fp));

	if (IS_ERR_OR_NULL(fp)) {
		dev_err(dbd2->dev, "UART device node open failed %d\n", ret);
		return -ENODEV;
	}

	/* set uart_dev members */
	dbd2->uart_file = fp;
	dbd2->uart_tty = ((struct tty_file_private *)fp->private_data)->tty;
	dbd2->uart_ld = tty_ldisc_ref(dbd2->uart_tty);
	dbd2->uart_open = 1;

	dbmd2_uart_configure_tty(dbd2,
				 dbd2->uart_tty,
				 dbd2->uart_speed[0],
				 UART_TTY_STOP_BITS,
				 0, 0);

	return 0;
}


static int dbmd2_uart_open_file(struct dbd2_data *dbd2)
{
	int ret = 0;
	struct file *fp;
	int attempt = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(60000);
	u16 fwver = 0xffff;
	int fw_load_retry = RETRY_COUNT;
	if (dbd2->uart_open)
		return 0;

	/*
	 * Wait for the device node to appear in the filesystem. This can take
	 * some time if the kernel is still booting up and filesystems are
	 * being mounted.
	 */
	do {
		msleep(50);
		dev_dbg(dbd2->dev,
			"%s(): probing for tty on %s (attempt %d)\n",
			 __func__, dbd2->uart_dev, ++attempt);

		fp = filp_open(dbd2->uart_dev, O_RDWR | O_NONBLOCK | O_NOCTTY,
			       0);

		ret = PTR_ERR(fp);
	} while (time_before(jiffies, timeout) && (ret == -ENOENT) &&
		 (atomic_read(&dbd2->stop_uart_probing) == 0));

	if (atomic_read(&dbd2->stop_uart_probing)) {
		dev_dbg(dbd2->dev, "UART probe thread stopped\n");
		atomic_set(&dbd2->stop_uart_probing, 0);
		return -EIO;
	}

	if (IS_ERR_OR_NULL(fp)) {
		dev_err(dbd2->dev, "UART device node open failed\n");
		return -ENODEV;
	}

	mutex_lock(&dbd2->lock);
	/* set uart_dev members */
	dbd2->uart_file = fp;
	dbd2->uart_tty = ((struct tty_file_private *)fp->private_data)->tty;
	dbd2->uart_ld = tty_ldisc_ref(dbd2->uart_tty);
	dbd2->uart_open = 1;
	mutex_unlock(&dbd2->lock);

	atomic_inc(&dbd2->stop_uart_probing);
	dbd2->uart_probe_thread = NULL;

	dev_info(dbd2->dev, "%s: request_firmware - %s\n",
			__func__, DBD2_FIRMWARE_NAME);
	do {
		ret = request_firmware(&dbd2->fw, DBD2_FIRMWARE_NAME,
				       dbd2->dev);
		if (ret < 0) {
			dev_dbg(dbd2->dev, "request firmware failed\n");
			msleep(500);
		}
	} while (ret < 0 && dbd2->firmware_retries++ < RETRY_COUNT*2);

	if (!dbd2->fw) {
		dev_err(dbd2->dev, "firmware request failed\n");
		return -ENODEV;
	}

	dev_info(dbd2->dev, "loading firmware\n");

	/* enable high speed clock for firmware loading */
	dbd2_clk_enable(1);
	uart_lock(&dbd2->lock);

	do {
		ret = dbd2_reset(dbd2);
		if (ret < 0) {
			dev_err(dbd2->dev, "unable to reset device\n");
			uart_unlock(&dbd2->lock);
			return -ENODEV;
		}

		ret = dbd2_load_firmware(dbd2);
		if (ret < 0) {
			dev_dbg(dbd2->dev, "unable to load firmware\n");
			continue;
		}

		ret = dbd2_wait_till_alive(dbd2);
		if (!ret) {
			dev_dbg(dbd2->dev, "failed to boot, device not responding\n");
			continue;
		}

		/* everything went well */
		break;
	} while (--fw_load_retry);

	if (!fw_load_retry) {
		dev_err(dbd2->dev, "%s: exceeded max attepmts to load fw\n",
				__func__);
		goto out;
	}

	dev_info(dbd2->dev,
		"Setting GPIO & System Config 0x%04x\n",
		dbd2->gpio_reg_config);

	(void)dbd2_send_cmd(dbd2, DBD2_GPIO_CFG | dbd2->gpio_reg_config, NULL);

#if USE_SLEEPMODE_GPIO_FOR_WAKEUP
	ret = dbd2_send_cmd(dbd2, DBD2_EXIT_FW_SLEEP_MODE_GPIO |
		DBMD2_EXITSLEEP_ENABLE | DBMD2_EXITSLEEP_GPIO, NULL);
	if (ret < 0)
		dev_err(dbd2->dev, "error setting exit from sleep gpio\n");
#endif

	dbd2_set_bytes_per_sample(dbd2, dbd2->audio_mode);

	/* Set Backlog to 2 */
	ret = dbd2_set_backlog_len(dbd2, 2);


	if (ret < 0)
		dev_err(dbd2->dev,
			"could not backlog history configuration\n");

	ret = dbd2_set_microphone_mode(dbd2, dbd2->init_mic_config);

	if (ret < 0)
		dev_err(dbd2->dev,
			"could not set microphone mode\n");

	ret = dbd2_send_cmd(dbd2, DBD2_POST_DETECTION_CLOCK_CONFIG |
			    dbd2->clock_config[0], NULL);
	if (ret < 0)
		dev_err(dbd2->dev,
			"could not set post detection clock configuration\n");
#ifdef USE_WAKEUP_FROM_UART
	ret = dbd2_send_cmd(dbd2, DBD2_WAKEUP_FROM_UART |
			    0x40, NULL);
	if (ret < 0)
		dev_err(dbd2->dev,
			"could not set wakeup from uart configuration\n");
#endif

	(void)dbd2_send_cmd(dbd2,
			    DBD2_FAST_CLK_FREQ | (dbd2->freq / 1000),
			    NULL);

	dbd2->device_ready = true;

	ret = dbd2_send_cmd_short(dbd2, DBD2_GET_FW_VER, &fwver);
	if (ret < 0) {
		dev_err(dbd2->dev, "could not read firmware version\n");
		goto out;
	}

	dev_info(dbd2->dev, "firmware 0x%x ready\n", fwver);

	dbd2_set_uart_speed(dbd2, DBD2_UART_SPEED_57600);

	ret = dbd2_set_mode(dbd2, DBMD2_IDLE);
	if (ret) {
		dev_err(dbd2->dev, "failed to set device to idle mode\n");
		goto out;
	}


	ret = dbd2_set_mode(dbd2, DBMD2_SLEEP_PLL_OFF);
	if (ret) {
		dev_err(dbd2->dev, "failed to set device to idle mode\n");
		goto out;
	}


out:
	/* disable high speed clock after firmware loading */
	dbd2_clk_enable(0);

	release_firmware(dbd2->fw);
	dbd2->fw = NULL;

	uart_unlock(&dbd2->lock);

	return ret;
}

static int dbmd2_uart_open_thread(void *data)
{
	int ret;
	struct dbd2_data *dbd2 = (struct dbd2_data *)data;

	ret = dbmd2_uart_open_file(dbd2);
	while (!kthread_should_stop())
		usleep_range(10000, 11000);
	return ret;
}

static int dbmd2_uart_open(struct dbd2_data *dbd2)
{
	int rc = 0;

	dev_dbg(dbd2->dev, "%s():\n", __func__);

	BUG_ON(dbd2->uart_probe_thread);

	dbd2->uart_probe_thread = kthread_run(dbmd2_uart_open_thread,
					      (void *)dbd2,
					      "dbmd2 probe thread");
	if (IS_ERR_OR_NULL(dbd2->uart_probe_thread)) {
		dev_err(dbd2->dev,
			"%s(): can't create dbmd2 uart probe thread = %p\n",
			__func__, dbd2->uart_probe_thread);
		rc = -ENOMEM;
	}

	return rc;
}

static void dbmd2_uart_close(struct dbd2_data *dbd2)
{
	dbmd2_uart_close_file(dbd2);
}

static int dbd2_send_i2c_cmd(struct dbd2_data *dbd2, u32 command, u16 *response)
{
	u8 send[4];
	u8 recv[4];
	int ret = 0;
	int retry = RETRY_COUNT;

	send[0] = (command >> 24) & 0xff;
	send[1] = (command >> 16) & 0xff;
	send[2] = (command >> 8) & 0xff;
	send[3] = command & 0xff;

	ret = i2c_master_send(dbd2->client, send, 4);
	if (ret < 0) {
		dev_dbg(dbd2->dev, "i2c_master_send failed ret = %d\n", ret);
		return ret;
	}

	/* The sleep command cannot be acked before the device goes to sleep */
	if (command == DBD2_SET_POWER_STATE_SLEEP)
		return ret;
	else if (command == DBD2_SYNC_POLLING)
		usleep_range(1000, 2000);
	/* A host command received will blocked until the current audio frame
	   processing is finished, which can take up to 10 ms */
	else
		usleep_range(10000, 11000);

	if (response) {
		while (retry--) {
			ret = i2c_master_recv(dbd2->client, recv, 4);
			if (ret < 0) {
				dev_err(dbd2->dev, "i2c_master_recv failed\n");
				return ret;
			}
			/*
			 * Check that the first two bytes of the response match
			 * (the ack is in those bytes)
			 */
			if ((send[0] == recv[0]) && (send[1] == recv[1])) {
				if (response)
					*response = (recv[2] << 8) | recv[3];
				ret = 0;
				break;
			}

			dev_dbg(dbd2->dev,
				"incorrect ack (got 0x%.2x%.2x)\n",
				recv[0], recv[1]);
			ret = -EINVAL;

			/* Wait before polling again */
			if (retry > 0)
				msleep(20);
		}
	}

	return ret;
}

static int dbd2_send_spi_cmd(struct dbd2_data *dbd2, u32 command, u16 *response)
{
	char tmp[3];
	u8 send[7];
	u8 recv[7];
	int ret = 0;

	ret = snprintf(tmp, 3, "%02x", (command >> 16) & 0xff);
	if (ret < 0) {
		dev_err(dbd2->dev, "%s: invalid command\n", __func__);
		return ret;
	}
	send[0] = tmp[0];
	send[1] = tmp[1];
	send[2] = 'w';

	ret = snprintf(tmp, 3, "%02x", (command >> 8) & 0xff);
	if (ret < 0) {
		dev_err(dbd2->dev, "%s: invalid command\n", __func__);
		return ret;
	}
	send[3] = tmp[0];
	send[4] = tmp[1];

	ret = snprintf(tmp, 3, "%02x", command & 0xff);
	if (ret < 0) {
		dev_err(dbd2->dev, "%s: invalid command\n", __func__);
		return ret;
	}
	send[5] = tmp[0];
	send[6] = tmp[1];

	ret = dbmd2_spi_write(dbd2, send, 7);
	if (ret < 0) {
		dev_err(dbd2->dev, "dbmd2_spi_write failed ret = %d\n", ret);
		return ret;
	}

	/* The sleep command cannot be acked before the device goes to sleep */
	if (command == DBD2_SET_POWER_STATE_SLEEP)
		return ret;
	/* A host command received will blocked until the current audio frame
	   processing is finished, which can take up to 10 ms */
	usleep_range(10000, 11000);

	if (response) {
		ret = dbmd2_spi_read(dbd2, recv, 5);
		if (ret < 0) {
			dev_err(dbd2->dev, "dbmd2_spi_read failed =%d\n", ret);
			return ret;
		}
		recv[5] = 0;
		ret = kstrtou16((const char *)&recv[1], 16, response);
		if (ret < 0) {
			dev_err(dbd2->dev, "dbmd2_spi_read failed -%d\n", ret);
			dev_err(dbd2->dev, "%x:%x:%x:%x:\n", recv[1], recv[2],
				recv[3], recv[4]);
			return ret;
		}
	}

	return ret;
}

static int dbd2_send_uart_cmd(
		struct dbd2_data *dbd2, u32 command, u16 *response)
{
	char tmp[3];
	u8 send[7];
	u8 recv[6];
	int ret = 0;

	if (response)
		dbd2_flush_rx_fifo(dbd2);

	ret = snprintf(tmp, 3, "%02x", (command >> 16) & 0xff);
	if (ret < 0)
		return ret;
	send[0] = tmp[0];
	send[1] = tmp[1];
	send[2] = 'w';

	ret = snprintf(tmp, 3, "%02x", (command >> 8) & 0xff);
	if (ret < 0)
		return ret;
	send[3] = tmp[0];
	send[4] = tmp[1];

	ret = snprintf(tmp, 3, "%02x", command & 0xff);
	if (ret < 0)
		return ret;
	send[5] = tmp[0];
	send[6] = tmp[1];

	ret = dbmd2_uart_write_sync(dbd2, send, 7);
	if (ret < 0)
		return ret;

	/* The sleep command cannot be acked before the device goes to sleep */
	if (command == DBD2_SET_POWER_STATE_SLEEP)
		return ret;

	if (response) {
		ret = dbmd2_uart_read_sync(dbd2, recv, 5);
		if (ret < 0)
			return ret;
		recv[5] = 0;
		ret = kstrtou16(recv, 16, response);


		if (ret < 0) {
			dev_err(dbd2->dev, "dbmd2_uart_read failed\n");
			dev_err(dbd2->dev, "%x:%x:%x:%x:\n", recv[0], recv[1],
				recv[2], recv[3]);
			return ret;
		}
	}

	return ret;
}

static int dbd2_send_cmd(struct dbd2_data *dbd2, u32 command, u16 *response)
{
	if (dbd2->client_spi)
		return dbd2_send_spi_cmd(dbd2, command, response);
	else if (dbd2->client)
		return dbd2_send_i2c_cmd(dbd2, command, response);

	return dbd2_send_uart_cmd(dbd2, command, response);
}

static int dbd2_send_spi_cmd_boot(struct dbd2_data *dbd2, u32 command)
{
	u8 send[3];
	int ret = 0;

	dev_err(dbd2->dev, "dbd2_send_spi_cmd_boot = %x\n", command);

	send[0] = (command >> 16) & 0xff;
	send[1] = (command >>  8) & 0xff;

	ret = dbmd2_spi_write(dbd2, send, 2);
	if (ret < 0) {
		dev_err(dbd2->dev, "send_spi_cmd_boot ret = %d\n", ret);
		return ret;
	}

	/* A host command received will blocked until the current audio frame
	   processing is finished, which can take up to 10 ms */
	usleep_range(10000, 11000);

	return ret;
}

static int dbd2_send_i2c_cmd_boot(struct dbd2_data *dbd2, u32 command)
{
	u8 send[3];
	int ret = 0;

	send[0] = (command >> 16) & 0xff;
	send[1] = (command >>  8) & 0xff;

	ret = i2c_master_send(dbd2->client, send, 2);
	if (ret < 0) {
		dev_err(dbd2->dev, "i2c_master_send failed ret = %d\n", ret);
		return ret;
	}

	/* A host command received will blocked until the current audio frame
	   processing is finished, which can take up to 10 ms */
	usleep_range(10000, 11000);

	return ret;
}

static int dbd2_send_i2c_cmd_short(
		struct dbd2_data *dbd2, u32 command, u16 *response)
{
	u8 send[2];
	u8 recv[2];
	int ret = 0;

	send[0] = (command >> 16) & 0xff;
	send[1] = (command >>  8) & 0xff;

	ret = i2c_master_send(dbd2->client, send, 2);
	if (ret < 0) {
		dev_err(dbd2->dev, "i2c_master_send failed ret = %d\n", ret);
		return ret;
	}

	/* The sleep command cannot be acked before the device goes to sleep */
	if (command == DBD2_SET_POWER_STATE_SLEEP)
		return ret;
	/* A host command received will blocked until the current audio frame
	   processing is finished, which can take up to 10 ms */
	usleep_range(10000, 11000);

	if (response) {
		ret = i2c_master_recv(dbd2->client, recv, 2);
		if (ret < 0) {
			dev_err(dbd2->dev, "i2c_master_recv failed\n");
			return ret;
		}
		if (response)
			*response = (recv[0] << 8) | recv[1];
		/* memcpy(response, recv, 2); */
		ret = 0;
	}

	return ret;
}

static int dbd2_send_spi_cmd_short(
		struct dbd2_data *dbd2, u32 command, u16 *response)
{
	char tmp[3];
	u8 send[3];
	char recv[7];
	int ret = 0;

	snprintf(tmp, 3, "%02x", (command >> 16) & 0xff);
	send[0] = tmp[0];
	send[1] = tmp[1];
	send[2] = 'r';

	ret = dbmd2_spi_write(dbd2, send, 3);
	if (ret < 0) {
		dev_err(dbd2->dev, "dbmd2_spi_write failed ret = %d\n", ret);
		return ret;
	}

	/* The sleep command cannot be acked before the device goes to sleep */
	if (command == DBD2_SET_POWER_STATE_SLEEP)
		return ret;
	/* A host command received will blocked until the current audio frame
	   processing is finished, which can take up to 10 ms */
	usleep_range(10000, 11000);

	if (response) {
		ret = dbmd2_spi_read(dbd2, recv, 6);
		if (ret < 0) {
			dev_err(dbd2->dev, "dbmd2_spi_read failed\n");
			return ret;
		}
		recv[6] = 0;
		ret = kstrtou16((const char *)&recv[2], 16, response);
		if (ret < 0) {
			dev_err(dbd2->dev, "dbmd2_spi_read conversion failed\n");
			dev_err(dbd2->dev, "%x:%x:%x:%x:\n",
				recv[1], recv[2], recv[3], recv[4]);
			return ret;
		}
	}

	return ret;
}

static int dbd2_send_uart_cmd_short(
		struct dbd2_data *dbd2, u32 command, u16 *response)
{
	char tmp[3];
	u8 send[3];
	u8 recv[6] = {0, 0, 0, 0, 0, 0};
	int ret = 0;

	if (response)
		dbd2_flush_rx_fifo(dbd2);

	ret = snprintf(tmp, 3, "%02x", (command >> 16) & 0xff);
	send[0] = tmp[0];
	send[1] = tmp[1];
	send[2] = 'r';

	ret = dbmd2_uart_write_sync(dbd2, send, 3);
	if (ret < 0)
		return ret;

	/* The sleep command cannot be acked before the device goes to sleep */
	if (command == DBD2_SET_POWER_STATE_SLEEP)
		return ret;
	/* A host command received will blocked until the current audio frame
	   processing is finished, which can take up to 10 ms */
	/* usleep_range(10000, 11000); */

	if (response) {
		ret = dbmd2_uart_read_sync(dbd2, recv, 5);
		if (ret < 0)
			return ret;
		/* recv[5] = 0; */
		ret = kstrtou16(recv, 16, response);
		if (ret < 0) {
			dev_err(dbd2->dev,
				"dbmd2_uart_read conversion failed\n");
			dev_err(dbd2->dev, "%x:%x:%x:%x:\n", recv[0], recv[1],
				recv[2], recv[3]);
			return ret;
		}
	}

	return ret;
}

static int dbd2_send_cmd_short(
		struct dbd2_data *dbd2, u32 command, u16 *response)
{
	if (dbd2->client_spi)
		return dbd2_send_spi_cmd_short(dbd2, command, response);
	else if (dbd2->client)
		return dbd2_send_i2c_cmd_short(dbd2, command, response);

	return dbd2_send_uart_cmd_short(dbd2, command, response);
}


static int dbd2_send_spi_data(
		struct dbd2_data *dbd2, const u8 *data, size_t size)
{
	int ret = 0;
	const u8 *i2c_cmds = data;
	int to_copy = size;

	while (to_copy > 0) {
		ret = dbmd2_spi_write(dbd2, i2c_cmds,
				min(to_copy, MAX_CMD_SPI_SEND_SIZE));
		if (ret < 0) {
			dev_err(dbd2->dev, "dbmd2_spi_write ret=%d\n",
				ret);
			break;
		}
		to_copy -= MAX_CMD_SPI_SEND_SIZE;
		i2c_cmds += MAX_CMD_SPI_SEND_SIZE;
	}

	return ret;
}

static int dbd2_send_i2c_data(
		struct dbd2_data *dbd2, const u8 *data, size_t size)
{
	int ret = 0;
	const u8 *i2c_cmds = data;
	int to_copy = size;

	while (to_copy > 0) {
		ret = i2c_master_send(dbd2->client, i2c_cmds,
				min(to_copy, MAX_CMD_SEND_SIZE));
		if (ret < 0) {
			dev_err(dbd2->dev, "i2c_master_send failed ret=%d\n",
				ret);
			break;
		}
		to_copy -= MAX_CMD_SEND_SIZE;
		i2c_cmds += MAX_CMD_SEND_SIZE;
	}

	return ret;
}

static int dbd2_send_data(
		struct dbd2_data *dbd2, const u8 *data, size_t size)
{
	if (dbd2->client_spi)
		return dbd2_send_spi_data(dbd2, data, size);
	else if (dbd2->client)
		return dbd2_send_i2c_data(dbd2, data, size);

	return dbmd2_uart_write_sync(dbd2, data, size);
}

static int dbmd2_wait_for_ok(struct dbd2_data *dbd2)
{
	unsigned char resp[5];
	unsigned char match[] = "OK\n\r";
	int rc;


	rc = dbmd2_uart_read_sync(dbd2, (u8 *)(&(resp[0])), 3);
	if (rc < 0) {
		dev_err(dbd2->dev, "fail to read ok from uart: error = %d\n",
			rc);
		return rc;
	}
	rc = strncmp(match, resp, 2);
	if (rc)
		dev_err(dbd2->dev, "result = %d : %x:%x:%x\n", rc,
			resp[0], resp[1], resp[2]);
	if (rc)
		rc = strncmp(match + 1, resp, 2);
	if (rc)
		rc = strncmp(match, resp + 1, 2);
	return rc;
}

static int dbmd2_uart_sync(struct dbd2_data *dbmd2)
{
	struct dbd2_platform_data *pdata = &dbmd2->pdata;
	int rc;
	char *buf;
	int i;
	/* Send init sequence for up to 100ms at 115200baud.
	 * 1 start bit, 8 data bits, 1 parity bit, 2 stop bits = 12 bits
	 * FIXME: make sure it is multiple of 8 */
	unsigned int size = ((UART_TTY_BOOT_BAUD_RATE / 12) *
				UART_SYNC_LENGTH) / 1000;

	dev_info(dbmd2->dev, "%s: start boot sync\n", __func__);

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf) {
		dev_err(dbmd2->dev,
				"%s: failure: no memory for sync buffer\n",
				__func__);
		return -ENOMEM;
	}

	for (i = 0; i < size; i += 8) {
		buf[i]   = 0x00;
		buf[i+1] = 0x00;
		buf[i+2] = 0x00;
		buf[i+3] = 0x00;
		buf[i+4] = 0x00;
		buf[i+5] = 0x00;
		buf[i+6] = 0x41;
		buf[i+7] = 0x63;
	}

	dbmd2_uart_write(dbmd2, (char *)buf, size);

	/* get D2 out of reset state */
	if ( dbmd2->freq > 32768)
		gpio_set_value(pdata->gpio_reset, 1);

	kfree(buf);
	/* check if synchronization succeeded */
	rc = dbmd2_wait_for_ok(dbmd2);
	if (rc != 0) {
		dev_dbg(dbmd2->dev, "%s: boot fail: no sync found err = %d\n",
				__func__, rc);
		return  -EAGAIN;
	}
	dbd2_flush_rx_fifo(dbmd2);

	dev_info(dbmd2->dev, "%s: boot sync successfully\n", __func__);

	return rc;
}


static int dbd2_verify_checksum(
		struct dbd2_data *dbd2, const char *fw_checksum)
{
	int ret = 0;
	char rx_checksum[7];
	u8 cmd[2] = {0x5a, 0x0e};

	if (!dbd2->client && !dbd2->client_spi) {
		dbd2_flush_rx_fifo(dbd2);
		ret = dbmd2_uart_write(dbd2, (char *)&cmd, 2);
		if (ret < 0)
			return -1;
		dbmd2_uart_read_sync(dbd2, (char *)&rx_checksum[0], 6);
		ret = memcmp(fw_checksum, (void *)&rx_checksum[2], 4);
	} else if (!dbd2->client_spi) {
		ret = dbd2_send_i2c_cmd_boot(dbd2, DBD2_READ_CHECKSUM);
		if (ret < 0) {
			dev_dbg(dbd2->dev, "could not read checksum\n");
			return -1;
		}

		ret = i2c_master_recv(dbd2->client, rx_checksum, 6);
		if (ret < 0) {
			dev_dbg(dbd2->dev, "could not read checksum data\n");
			return -1;
		}
		ret = memcmp(fw_checksum, (void *)&rx_checksum[2], 4);
	} else {
		dev_dbg(dbd2->dev, "%s: dbd2 verify spi boot checksum\n",
			__func__);
		ret = dbd2_send_spi_cmd_boot(dbd2, DBD2_READ_CHECKSUM);
		if (ret < 0) {
			dev_dbg(dbd2->dev, "could not read checksum\n");
			return -1;
		}

		ret = dbmd2_spi_read(dbd2, rx_checksum, 7);
		if (ret < 0) {
			dev_dbg(dbd2->dev, "could not read checksum data\n");
			return -1;
		}
		ret = memcmp(fw_checksum, (void *)&rx_checksum[3], 4);
	}


	if (ret) {
		dev_dbg(dbd2->dev, "%s: checksum mismatch\n", __func__);
		dev_dbg(dbd2->dev,
			"%s: Got:\n0x%x 0x%x - 0x%x 0x%x 0x%x 0x%x 0x%x\n",
			__func__, rx_checksum[0], rx_checksum[1],
			rx_checksum[2], rx_checksum[3], rx_checksum[4],
			rx_checksum[5], rx_checksum[6]);
		dev_dbg(dbd2->dev,
			"%s: Expected:\n           0x%x 0x%x 0x%x 0x%x\n",
			__func__, fw_checksum[0], fw_checksum[1],
			fw_checksum[2], fw_checksum[3]);
	}

	return ret;
}

static int dbd2_calc_amodel_checksum(const char *amodel,
		unsigned long len, unsigned long *chksum)
{
	unsigned long sum = 0;
	u16 val;
	unsigned long i;
	u32 pos = 0, chunk_len;

	*chksum = (unsigned long)0 - 1;

	while (pos < len) {
		val = *(u16 *)(&amodel[pos]);
		pos += 2;
		if (pos >= len) {
			dev_dbg(dbd2_data->dev, "%s:%d %u", __func__,
				__LINE__, pos);
			return -1;
		}

		if (val == 0x025a) {
			sum += 0x5a + 0x02;

			chunk_len = *(u32 *)(&amodel[pos]);
			pos += 4;
			if (pos >= len) {
				dev_dbg(dbd2_data->dev, "%s:%d %u", __func__,
					__LINE__, pos);
				return -1;
			}

			sum += chunk_len;

			sum += *(u32 *)(&amodel[pos]);
			pos += 4;

			if ((pos + (chunk_len * 2)) > len) {
				dev_dbg(dbd2_data->dev, "%s:%d %u, %u",
					__func__, __LINE__, pos, chunk_len);
				return -1;
			}

			for (i = 0; i < chunk_len; i++) {
				sum += *(u16 *)(&amodel[pos]);
				pos += 2;
			}
		} else
			return -1;
	}

	sum += 0x5A + 0x0e;
	*chksum = sum;

	return 0;
}

static int dbd2_boot_firmware(struct dbd2_data *dbd2)
{
	int ret;
	u8 bcmd[2] = {0x5a, 0x0b};

	if (!dbd2->client && !dbd2->client_spi) {
		ret = dbmd2_uart_write(dbd2, (char *)&bcmd, 2);
		if (ret < 0)
			return -1;
	} else if (!dbd2->client_spi) {
		ret = dbd2_send_i2c_cmd_boot(dbd2, DBD2_FIRMWARE_BOOT);
		if (ret < 0) {
			dev_err(dbd2->dev, "could not boot firmware\n");
			return -1;
		}
	} else {
		ret = dbd2_send_spi_cmd_boot(dbd2, DBD2_FIRMWARE_BOOT);
		if (ret < 0) {
			dev_err(dbd2->dev, "could not boot firmware\n");
			return -1;
		}
	}
	return 0;
}


static int dbd2_reset(struct dbd2_data *dbd2)
{
	int ret = 0;
	struct dbd2_platform_data *pdata = &dbd2->pdata;
	int retry = RETRY_COUNT;

	while (retry--) {
		if (!dbd2->client && !dbd2->client_spi) {
			/* set baudrate to BOOT baud (common case) */
			dbmd2_uart_configure_tty(dbd2,
						 dbd2->uart_tty,
						 UART_TTY_BOOT_BAUD_RATE,
						 UART_TTY_BOOT_STOP_BITS,
						 1, 0);

			dbd2_flush_rx_fifo(dbd2);

			usleep_range(10000, 20000);
		}

		/* Reset DBD2 chip */
		gpio_set_value(pdata->gpio_reset, 0);
		usleep_range(1000, 1100);

		if (dbd2->freq > 0 && dbd2->freq <= 32768) {
			gpio_set_value(pdata->gpio_reset, 1);
			msleep(275);  /* for 32khz clock */
		}
		if (!dbd2->client && !dbd2->client_spi) {
			ret = dbmd2_uart_sync(dbd2);
			if (ret != 0) {
				dev_err(dbd2->dev, "sync failed, retry\n");
				continue;
			}

			dbd2_flush_rx_fifo(dbd2);
		}
		break;
	}

	return (retry < 0 ? -1 : 0);
}

static int dbd2_load_firmware(struct dbd2_data *dbd2)
{
	int ret = 0;
	const char *fw_checksum;
	u8 sync_spi[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	u8 sbl_spi[] = {
		0x5A, 0x04, 0x4c, 0x00, 0x00,
		0x03, 0x11, 0x55, 0x05, 0x00};
	u8 clr_crc[] = {
		0x5A, 0x03, 0x52, 0x0a, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00};

	if (dbd2->fw->size < 4)
		return -1;

	fw_checksum = &dbd2->fw->data[dbd2->fw->size - 4];

	if (!dbd2->client && !dbd2->client_spi) {
		/* sbl */
		ret = dbd2_send_data(dbd2, sbl, sizeof(sbl));
		if (ret < 0) {
			dev_err(dbd2->dev,
				"---------> load sbl error\n");
			return ret;
		}

		/* check if sbl is ok */
		ret = dbmd2_wait_for_ok(dbd2);
		if (ret != 0) {
			dev_err(dbd2->dev,
				"sbl does not respond with ok\n");
			return ret;
		}

		/* set baudrate to FW baud (common case) */
		dbmd2_uart_configure_tty(dbd2,
				dbd2->uart_tty,
				dbd2->uart_speed[DBD2_UART_SPEED_3000000],
				UART_TTY_STOP_BITS,
				0, 0);
		dbd2_flush_rx_fifo(dbd2);
	} else if (dbd2->client_spi) {
		dev_err(dbd2->dev, "---------> load spi sync\n");
		ret = dbd2_send_data(dbd2, sync_spi, sizeof(sync_spi));
		if (ret < 0) {
			dev_err(dbd2->dev,
				"---------> load spi sbl error\n");
			return ret;
		}
		dev_err(dbd2->dev, "---------> load spi sbl\n");
		ret = dbd2_send_data(dbd2, sbl_spi, sizeof(sbl_spi));
		if (ret < 0) {
			dev_err(dbd2->dev,
				"---------> load spi sbl error\n");
			return ret;
		}
		usleep_range(10000, 11000);
	}

	/* send clear check sum cmd
	 * ONLY in other-than-i2c mode as there's no patch for i2c
	 */
	if (!dbd2->client) {
		ret = dbd2_send_data(dbd2, clr_crc, sizeof(clr_crc));
		if (ret < 0) {
			dev_warn(dbd2->dev,
					"%s() failed to clear check sum\n",
					__func__);
			goto out;
		}
	}

	/* send firmware */
	ret = dbd2_send_data(dbd2, dbd2->fw->data, dbd2->fw->size - 4);
	if (ret < 0) {
		dev_dbg(dbd2->dev,
			"-----------> load firmware error\n");
		goto out;
	}

	msleep(50);

	/* verify checksum */
	ret = dbd2_verify_checksum(dbd2, fw_checksum);
	if (ret != 0) {
		dev_dbg(dbd2->dev,
			"-----------> load firmware checksum error\n");
		goto out;
	}

	dev_info(dbd2->dev, "---------> firmware loaded\n");

	ret = dbd2_boot_firmware(dbd2);
	if (ret) {
		dev_dbg(dbd2->dev, "booting the firmware failed\n");
		goto out;
	}

	/* FIXME: wait some time till the bytes went out */
	usleep_range(10000, 11000);

out:
	if (!dbd2->client && !dbd2->client_spi) {
		/* set baudrate to FW baud (common case) */
		dbmd2_uart_configure_tty(dbd2,
					 dbd2->uart_tty,
					 1000000,
					 UART_TTY_STOP_BITS,
					 0, 1);
		dbd2_flush_rx_fifo(dbd2);
	}

	if (dbd2->client_spi) {
		usleep_range(10000, 11000);
		dbmd2_spi_configure(dbd2, 10000);
		dev_info(dbd2->dev, "---------> spi speed change\n");
		usleep_range(10000, 11000);
	}

	if (dbd2->client) {
		dbd2->client->addr = 0x3f;
		/* msleep(4800); // for 32khz clcok */
	}

	return ret;
}

static void dbd2_acoustic_model_load(struct dbd2_data *dbd2, const u8 *data,
				     size_t size)
{
	int ret;
	u16 result;
	int retry = RETRY_COUNT;
	u32 checksum;

	flush_work(&dbd2_data->sv_work);

	uart_lock(&dbd2->lock);

	dbd2->device_ready = false;

	dev_dbg(dbd2->dev, "%s\n", __func__);

	/* wakeup chip */
	dbd2_wake(dbd2);

	/* set chip to idle mode */
	ret = dbd2_set_mode(dbd2, DBMD2_IDLE);
	if (ret) {
		dev_err(dbd2->dev, "failed to set device to idle mode\n");
		goto out_unlock;
	}

	if (dbd2->change_speed) {
		/* enable high speed clock */
		ret = dbd2_set_uart_speed(dbd2, DBD2_UART_SPEED_1000000);
		if (ret != 0) {
			dev_err(dbd2->dev,
				"failed to change UART speed to highspeed\n");
			goto out_change_speed;
		}
	}

	while (retry--) {
		ret = dbd2_send_cmd(dbd2, DBD2_LOAD_NEW_ACUSTIC_MODEL, NULL);
		if (ret < 0) {
			dev_err(dbd2->dev,
				"failed to set firmware to recieve new acoustic model\n");
			goto out_change_speed;
		}

		dev_info(dbd2->dev,
			 "---------> acoustic model download start\n");
		ret = dbd2_send_data(dbd2, data, size - 4);
		if (ret < 0) {
			dev_err(dbd2->dev,
				"sending of acoustic model data failed\n");
			goto out_change_speed;
		}
		dev_info(dbd2->dev,
			 "---------> acoustic model download done\n");

		checksum = *(u32 *)(&data[size - 4]);
		if (dbd2_verify_checksum(dbd2, (const char *)&checksum)) {
			dev_err(dbd2->dev, "checksum of A-model failed\n");
			continue;
		}

		break;
	}

	ret = dbd2_boot_firmware(dbd2);
	if (ret) {
		dev_err(dbd2->dev, "booting the firmware failed\n");
		goto out_change_speed;
	}

	dbd2_set_bytes_per_sample(dbd2, dbd2->audio_mode);

	usleep_range(500000,501000);
	ret = dbd2_send_cmd_short(dbd2, DBD2_FW_ID, &result);
	if (ret < 0)
		dev_err(dbd2->dev, "failed to read firmware id\n");

	if (result == 0xdbd2) {
		dev_info(dbd2->dev, "acoustic model sent succsefull\n");
		dbd2->device_ready = true;
		dbd2->a_model_loaded = 1;
		ret = 0;
	} else {
		dev_info(dbd2->dev, "acoustic model send failed\n");
		ret = -1;
	}

	if (dbd2->auto_detection) {
		dev_info(dbd2->dev, "%s: enforcing DETECTION opmode\n",
			 __func__);
		dbd2_set_mode(dbd2, DBMD2_DETECTION);
	}

out_change_speed:
	if (dbd2->change_speed) {
		dbd2->change_speed = 0;
		ret = dbd2_set_uart_speed(dbd2, DBD2_UART_SPEED_57600);
		if (ret != 0)
			dev_err(dbd2->dev,
				"failed to change UART speed to normal\n");
	}
out_unlock:
	dbd2->device_ready = true;
	uart_unlock(&dbd2->lock);
}

static int dbd2_acoustic_model_build(struct dbd2_data *dbd2)
{
	unsigned char head[10] = { 0 };
	size_t pos;
	unsigned long checksum;
	int ret;

	pos = 0;

	head[0] = 0x5A;
	head[1] = 0x02;
	head[2] =  (dbd2->gram_size/2)        & 0xff;
	head[3] = ((dbd2->gram_size/2) >>  8) & 0xff;
	head[4] = ((dbd2->gram_size/2) >> 16) & 0xff;
	head[5] = ((dbd2->gram_size/2) >> 24) & 0xff;
	head[6] = 0x01;
	head[7] = 0x00;
	memcpy(dbd2->amodel_buf, head, 10);

	pos += 10;

	if (pos + dbd2->gram_size > MAX_AMODEL_SIZE)
		return -1;

	memcpy(dbd2->amodel_buf + pos, dbd2->gram_data, dbd2->gram_size);

	pos += dbd2->gram_size;

	head[0] = 0x5A;
	head[1] = 0x02;
	head[2] =  (dbd2->net_size/2)        & 0xff;
	head[3] = ((dbd2->net_size/2) >>  8) & 0xff;
	head[4] = ((dbd2->net_size/2) >> 16) & 0xff;
	head[5] = ((dbd2->net_size/2) >> 24) & 0xff;
	head[6] = 0x02;
	head[7] = 0x00;
	memcpy(dbd2->amodel_buf + pos, head, 10);

	pos += 10;

	if (pos + dbd2->net_size + 6 > MAX_AMODEL_SIZE)
		return -1;

	memcpy(dbd2->amodel_buf + pos, dbd2->net_data, dbd2->net_size);

	ret = dbd2_calc_amodel_checksum((char *)dbd2->amodel_buf,
					pos + dbd2->net_size,
					&checksum);
	if (ret) {
		dev_err(dbd2->dev, "failed to calculate Amodel checksum\n");
		return -1;
	}

	*(unsigned long *)(dbd2->amodel_buf + pos + dbd2->net_size) = checksum;

	return pos + dbd2->net_size+4;
}

static void dbd2_gram_bin_ready(const struct firmware *fw, void *context)
{
	struct dbd2_data *dbd2 = (struct dbd2_data *)context;
	int amodel_size = 0;

	dev_dbg(dbd2->dev, "%s\n", __func__);

	if (!fw)
		return;

	dev_dbg(dbd2->dev, "%s after fw check\n", __func__);

	mutex_lock(&dbd2->lock);
	dbd2->gram_data = vmalloc(fw->size);
	if (!dbd2->gram_data) {
		mutex_unlock(&dbd2->lock);
		return;
	}
	memcpy(dbd2->gram_data, fw->data, fw->size);
	dbd2->gram_size = fw->size;
	if (dbd2->net_data && dbd2->net_size) {
		amodel_size = dbd2_acoustic_model_build(dbd2);
		vfree(dbd2->gram_data);
		vfree(dbd2->net_data);
		dbd2->gram_data = NULL;
		dbd2->net_data = NULL;
	}
	mutex_unlock(&dbd2->lock);

	dev_dbg(dbd2->dev, "%s: amodel_size = %d\n", __func__, amodel_size);

	if (amodel_size > 0)
		dbd2_acoustic_model_load(dbd2, dbd2->amodel_buf, amodel_size);

	release_firmware(fw);
}

static void dbd2_net_bin_ready(const struct firmware *fw, void *context)
{
	struct dbd2_data *dbd2 = (struct dbd2_data *)context;
	int amodel_size = 0;

	dev_dbg(dbd2->dev, "%s\n", __func__);

	if (!fw)
		return;

	dev_dbg(dbd2->dev, "%s after fw check\n", __func__);

	mutex_lock(&dbd2->lock);
	dbd2->net_data = vmalloc(fw->size);
	if (!dbd2->net_data) {
		mutex_unlock(&dbd2->lock);
		return;
	}
	memcpy(dbd2->net_data, fw->data, fw->size);
	dbd2->net_size = fw->size;
	if (dbd2->gram_data && dbd2->gram_size) {
		amodel_size = dbd2_acoustic_model_build(dbd2);
		vfree(dbd2->gram_data);
		vfree(dbd2->net_data);
		dbd2->gram_data = NULL;
		dbd2->net_data = NULL;
	}
	mutex_unlock(&dbd2->lock);

	dev_dbg(dbd2->dev, "%s: amodel_size = %d\n", __func__, amodel_size);

	if (amodel_size > 0)
		dbd2_acoustic_model_load(dbd2, dbd2->amodel_buf, amodel_size);

	release_firmware(fw);
}

/* ------------------------------------------------------------------------
 * sysfs attributes
 * ------------------------------------------------------------------------ */
static int dbd2_d2param_get(struct dbd2_data *dbd2, u32 addr)
{
	int ret = 0;
	u16 val = 0xffff;

	uart_lock(&dbd2->lock);
	ret = dbd2_wake(dbd2);
	if (ret < 0) {
		dev_err(dbd2->dev, "%s: unable to wake\n", __func__);
		goto out;
	}

	ret = dbd2_send_cmd(dbd2, DBD2_SET_D2PARAM_ADDR | addr, NULL);

	if (ret < 0) {
		dev_err(dbd2->dev, "%s: failed to set d2paramaddr: %d\n",
				__func__, ret);
		goto out;
	}

	ret = dbd2_send_cmd_short(dbd2, DBD2_GET_D2PARAM, &val);

	if (ret < 0) {
		dev_err(dbd2->dev, "%s: failed to read d2param: %d\n",
				__func__, ret);
		goto out;
	}

	ret = (int)val;

out:
	uart_unlock(&dbd2->lock);
	return ret;
}

static ssize_t dbd2_reg_show(struct device *dev, u32 command,
			     struct device_attribute *attr, char *buf)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);
	int ret;
	u16 val = 0;

	uart_lock(&dbd2->lock);

	ret = dbd2_wake(dbd2);
	if (ret < 0) {
		dev_err(dbd2->dev, "unable to wake\n");
		goto out_unlock;
	}


	ret = dbd2_send_cmd_short(dbd2, command, &val);
	if (ret < 0) {
		dev_err(dbd2->dev, "get reg %x error\n", command);
		goto out_unlock;
	}

	if (command == DBD2_AUDIO_HISTORY)
		val = val & 0x0fff;

	ret = sprintf(buf, "0x%x\n", val);
out_unlock:

	uart_unlock(&dbd2->lock);
	return ret;
}

static ssize_t dbd2_reg_show_long(struct device *dev, u32 command,
				  u32 command1, struct device_attribute *attr,
				  char *buf)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);
	int ret;
	u16 val = 0;
	u32 result;

	uart_lock(&dbd2->lock);

	ret = dbd2_wake(dbd2);
	if (ret < 0) {
		dev_err(dbd2->dev, "unable to wake\n");
		goto out_unlock;
	}


	ret = dbd2_send_cmd_short(dbd2, command1, &val);
	if (ret < 0) {
		dev_err(dbd2->dev, "get reg %u error\n", command);
		goto out_unlock;
	}

	dev_err(dbd2->dev, "dbd2_reg_show_long = val = %d\n", val);
	result = (u32)(val & 0xffff);
	val = 0;
	ret = dbd2_send_cmd_short(dbd2, command, &val);
	if (ret < 0) {
		dev_err(dbd2->dev, "get reg %u error\n", command1);
		goto out_unlock;
	}

	dev_err(dbd2->dev, "dbd2_reg_show_long = val = %d\n", val);

	result += ((u32)val << 16);

	dev_err(dbd2->dev, "dbd2_reg_show_long = val = %d\n", result);

	ret = sprintf(buf, "0x%x\n", result);
out_unlock:

	uart_unlock(&dbd2->lock);
	return ret;
}

static ssize_t dbd2_reg_store(struct device *dev, u32 command,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	if (!dbd2->device_ready)
		return -EAGAIN;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	uart_lock(&dbd2->lock);

	ret = dbd2_wake(dbd2);
	if (ret < 0) {
		dev_err(dbd2->dev, "unable to wake\n");
		size = ret;
		goto out_unlock;
	}

	if (command == DBD2_OPR_MODE) {
		ret = dbd2_set_mode(dbd2, val);
		if (ret)
			size = ret;
		goto out_unlock;
	}

	if (command == DBD2_AUDIO_BUFFER_CONVERSION) {
		u16 mode = val & 0x1000;
		ret = dbd2_set_bytes_per_sample(dbd2, mode);
		if (ret) {
			size = ret;
			goto out_unlock;
		}
		command = DBD2_AUDIO_HISTORY;
	}

	if (command == DBD2_AUDIO_HISTORY) {
		ret = dbd2_set_backlog_len(dbd2, val);
		if (ret < 0) {
			dev_err(dbd2->dev, "set history error\n");
			size = ret;
			goto out_unlock;
		}
	}

	ret = dbd2_send_cmd(dbd2, command | (u32)val, NULL);
	if (ret < 0) {
		dev_err(dbd2->dev, "set reg error\n");
		size = ret;
		goto out_unlock;
	}

out_unlock:

	uart_unlock(&dbd2->lock);
	return size;
}

static ssize_t dbd2_reg_store_long(struct device *dev, u32 command,
				   u32 command1,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	if (!dbd2->device_ready)
		return -EAGAIN;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;
	dev_err(dbd2->dev, "dbd2_reg_store_long  val = %u\n", (int)val);

	uart_lock(&dbd2->lock);
	ret = dbd2_wake(dbd2);
	if (ret < 0) {
		dev_err(dbd2->dev, "unable to wake\n");
		size = ret;
		goto out_unlock;
	}

	ret = dbd2_send_cmd(dbd2, command1 | (val & 0xffff), NULL);
	if (ret < 0) {
		dev_err(dbd2->dev, "set reg error\n");
		size = ret;
		goto out_unlock;
	}

	ret = dbd2_send_cmd(dbd2, command | (val >> 16), NULL);
	if (ret < 0) {
		dev_err(dbd2->dev, "set reg error\n");
		size = ret;
		goto out_unlock;
	}

out_unlock:

	uart_unlock(&dbd2->lock);
	return size;
}

static ssize_t dbd2_fw_ver_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return dbd2_reg_show(dev, DBD2_GET_FW_VER, attr, buf);
}

static ssize_t dbd2_opr_mode_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	return dbd2_reg_show(dev, DBD2_OPR_MODE, attr, buf);
}

static ssize_t dbd2_opr_mode_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	return dbd2_reg_store(dev, DBD2_OPR_MODE, attr, buf, size);
}

static ssize_t dbd2_uart_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);
	int ret;

	if (dbd2->uart_probe_thread)
		return -EBUSY;

	dbmd2_uart_close_file(dbd2);

	ret = dbmd2_uart_open_file(dbd2);
	if (ret < 0) {
		dev_err(dbd2->dev, "UART initialization failed\n");
		return ret;
	}

	request_firmware(&dbd2->fw,
			 DBD2_FIRMWARE_NAME,
			 dbd2->dev);

	return size;
}

static ssize_t dbd2_trigger_level_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	return dbd2_reg_show(dev, DBD2_TG_THERSHOLD, attr, buf);
}

static ssize_t dbd2_trigger_level_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return dbd2_reg_store(dev, DBD2_TG_THERSHOLD, attr, buf, size);
}

static ssize_t dbd2_verification_level_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	return dbd2_reg_show(dev, DBD2_VERIFICATION_THRESHOLD, attr, buf);
}

static ssize_t dbd2_verification_level_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf,
					     size_t size)
{
	return dbd2_reg_store(dev, DBD2_VERIFICATION_THRESHOLD, attr, buf,
			      size);
}

static ssize_t dbd2_gain_shift_factor_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	return dbd2_reg_show(dev, DBD2_GAIN_SHIFT_VALUE, attr, buf);
}

static ssize_t dbd2_gain_shift_factor_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t size)
{
	return dbd2_reg_store(dev, DBD2_GAIN_SHIFT_VALUE, attr, buf, size);
}

static ssize_t dbd2_io_addr_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	return dbd2_reg_show_long(dev, DBD2_IO_PORT_ADDR_HI,
				  DBD2_IO_PORT_ADDR_LO, attr, buf);
}

static ssize_t dbd2_io_addr_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	return dbd2_reg_store_long(dev, DBD2_IO_PORT_ADDR_HI,
				   DBD2_IO_PORT_ADDR_LO, attr, buf, size);
}

static int dbd2_load_new_acoustic_model(struct dbd2_data *dbd2)
{
	int ret;

	if (!dbd2->device_ready) {
		dev_err(dbd2->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	uart_lock(&dbd2->lock);
	ret = dbd2_wake(dbd2);
	uart_unlock(&dbd2->lock);
	if (ret < 0) {
		dev_err(dbd2->dev, "unable to wake\n");
		return ret;
	}

	ret = request_firmware_nowait(THIS_MODULE,
				      FW_ACTION_HOTPLUG,
				      DBD2_GRAM_NAME,
				      gram_dev,
				      GFP_KERNEL,
				      dbd2,
				      dbd2_gram_bin_ready);
	if (ret < 0) {
		dev_err(dbd2->dev, "request_firmware_nowait error(%d)\n", ret);
		return ret;
	}
	dev_err(dbd2->dev, "gram firmware requested\n");

	ret = request_firmware_nowait(THIS_MODULE,
				      FW_ACTION_HOTPLUG,
				      DBD2_NET_NAME,
				      net_dev,
				      GFP_KERNEL,
				      dbd2,
				      dbd2_net_bin_ready);
	if (ret < 0) {
		dev_err(dbd2->dev, "request_firmware_nowait error(%d)\n", ret);
		return ret;
	}
	dev_err(dbd2->dev, "net firmware requested\n");

	return 0;
}

static ssize_t dbd2_acoustic_model_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);
	int ret;
	int val = dbd2_buf_to_int(buf);

	if (!dbd2->device_ready)
		return -EAGAIN;

	if (val == 0) {
		mutex_lock(&dbd2->lock);
		dbd2->change_speed = 1;
		mutex_unlock(&dbd2->lock);
		/* 0 means load model */
		ret = dbd2_load_new_acoustic_model(dbd2);
	} else if (val == 1) {
		/* 1 means send 1 to register 0xF */
		uart_lock(&dbd2->lock);
		ret = dbd2_send_cmd(dbd2, DBD2_LOAD_NEW_ACUSTIC_MODEL |
				    (u32)val, NULL);
		uart_unlock(&dbd2->lock);
		if (ret < 0)
			dev_err(dbd2->dev,
				"failed to set DBD2_LOAD_NEW_ACUSTIC_MODEL to %d\n",
				val);
		else
			ret = 0;
	} else {
		/* don't know what to do */
		ret = 0;
	}
	return (ret < 0 ? ret : size);
}

static ssize_t dbd2_last_detect_verication_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	return dbd2_reg_show(dev, DBD2_DETECT_VERIFICATION_LEVEL, attr, buf);
}

static ssize_t dbd2_detect_trigger_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return dbd2_reg_show(dev, DBD2_DETECT_TRIGER_LEVEL, attr, buf);
}

static ssize_t dbd2_last_detect_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	return dbd2_reg_show(dev, DBD2_LAST_DETECT_WORD_NUM, attr, buf);
}

static ssize_t dbd2_max_sample_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	return dbd2_reg_show(dev, DBD2_LAST_MAX_SMP_VALUE, attr, buf);
}

static ssize_t dbd2_io_value_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	return dbd2_reg_show_long(dev, DBD2_IO_PORT_VALUE_HI,
				  DBD2_IO_PORT_VALUE_LO, attr, buf);
}

static ssize_t dbd2_io_value_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	return dbd2_reg_store_long(dev, DBD2_IO_PORT_VALUE_HI,
				   DBD2_IO_PORT_VALUE_LO, attr, buf, size);
}

static ssize_t dbd2_buffer_size_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	return dbd2_reg_show(dev, DBD2_AUDIO_BUFFER_SIZE, attr, buf);
}

static ssize_t dbd2_buffsmps_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	return dbd2_reg_show(dev, DBD2_NUM_OF_SMP_IN_BUF, attr, buf);
}

static ssize_t dbd2_audio_conv_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	return dbd2_reg_show(dev, DBD2_AUDIO_BUFFER_CONVERSION, attr, buf);
}

static ssize_t dbd2_audio_conv_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	return dbd2_reg_store(dev, DBD2_AUDIO_BUFFER_CONVERSION, attr, buf,
			      size);
}

static ssize_t dbd2_uartspeed_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int ret;
	u16 reg;
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);

	if (!dbd2->device_ready)
		return -EAGAIN;

	uart_lock(&dbd2->lock);
	ret = dbd2_send_cmd_short(dbd2, DBD2_PRESENT_CLOCK_CONFIG, &reg);
	uart_unlock(&dbd2->lock);

	if (ret < 0)
		return -EIO;

	switch (reg) {
	case 0x0301:
	case 0x0501: return sprintf(buf, "57600");
	case 0x0000: return sprintf(buf, "1000000");
	case 0x1000: return sprintf(buf, "3000000");
	}

	return sprintf(buf, "unknown value %u\n", reg);
}

static ssize_t dbd2_uartspeed_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	int ret = 0, index = -1;
	int val = dbd2_buf_to_int(buf);
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);

	if (!dbd2->device_ready)
		return -EAGAIN;

	switch (val) {
	case DBD2_UART_SPEED_57600:
	case DBD2_UART_SPEED_1000000:
	case DBD2_UART_SPEED_3000000:
		dev_dbg(dbd2->dev,
				"%s: setting D2 clock to: 0x%04x\n",
				__func__, (u16)val);
		uart_lock(&dbd2->lock);
		ret = dbd2_set_uart_speed(dbd2, val);
		uart_unlock(&dbd2->lock);
		break;

	case 57600:
		index = DBD2_UART_SPEED_57600;
		break;
	case 1000000:
		index = DBD2_UART_SPEED_1000000;
		break;
	case 3000000:
		index = DBD2_UART_SPEED_3000000;
		break;

	default:
		dev_err(dbd2->dev,
				"%s: illegal value - 0x%04x\n",
				__func__, (u16)val);
		return -EIO;

	}

	if (index >= 0) {
		dev_err(dbd2->dev,
				"%s: setting TTY speed to: %d Baud/s\n",
				__func__, (u16)val);
		uart_lock(&dbd2->lock);
		ret = dbd2_set_uart_speed1(dbd2, index);
		uart_unlock(&dbd2->lock);
	}

	if (ret != 0)
		return ret;

	return size;
}

static ssize_t dbd2_lastduration_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	return dbd2_reg_show(dev, DBD2_LAST_DURATION, attr, buf);
}

static ssize_t dbd2_lasterror_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	return dbd2_reg_show(dev, DBD2_LAST_ERROR, attr, buf);
}

static ssize_t dbd2_micgain_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	return dbd2_reg_show(dev, DBD2_MIC_GAIN, attr, buf);
}

static ssize_t dbd2_micgain_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	return dbd2_reg_store(dev, DBD2_MIC_GAIN, attr, buf, size);
}

static ssize_t dbd2_backlog_size_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	return dbd2_reg_show(dev, DBD2_AUDIO_HISTORY, attr, buf);
}

static ssize_t dbd2_backlog_size_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf,
				       size_t size)
{
	return dbd2_reg_store(dev, DBD2_AUDIO_HISTORY, attr, buf,
			      size);
}

static ssize_t dbd2_detection_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);
	int ret;

	ret = sprintf(buf, "0x%x\n", dbd2->detection_state);

	return ret;
}

static ssize_t dbd2_detection_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t size)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);

	dbd2->detection_state = 0;

	return size;
}

static ssize_t dbd2_buffering_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);
	int ret;

	ret = sprintf(buf, "0x%x\n", dbd2->buffering);

	return ret;
}

static ssize_t dbd2_buffering_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t size)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	if (!dbd2->device_ready)
		return -EAGAIN;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	uart_lock(&dbd2->lock);
	ret = dbd2_wake(dbd2);
	if (ret < 0) {
		dev_err(dbd2->dev, "unable to wake\n");
		uart_unlock(&dbd2->lock);
		return ret;
	}

	dbd2->buffering = (val ? 1 : 0);
	uart_unlock(&dbd2->lock);

	return size;
}

static ssize_t dbd2_reset_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t size)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);
	struct dbd2_platform_data *pdata = &dbd2->pdata;
	int ret;
	int retry = 0;

	if (!dbd2->device_ready)
		return -EAGAIN;


	if (dbd2->client || dbd2->client_spi)
		return -EIO;

	dbd2->buffering = 0;

	flush_work(&dbd2->sv_work);

	uart_lock(&dbd2->lock);

	dbd2_set_mode(dbd2, DBMD2_IDLE);

	dbd2->device_ready = false;

	dbd2_flush_rx_fifo(dbd2);
	usleep_range(10000, 11000);
	/* set baudrate to FW baud (common case) */
	dbmd2_uart_configure_tty(dbd2,
				 dbd2->uart_tty,
				 UART_TTY_BOOT_BAUD_RATE,
				 UART_TTY_BOOT_STOP_BITS,
				 1, 0);

	do {
		dev_info(dbd2->dev, "resetting...(try %d/5)\n", retry+1);

		dbd2_flush_rx_fifo(dbd2);
		usleep_range(20000, 30000);

		/* Reset DBD2 chip */
		gpio_set_value(pdata->gpio_reset, 0);
		usleep_range(300, 400);
		gpio_set_value(pdata->gpio_reset, 1);

		/* Delay before sending commands */
		usleep_range(15000, 20000);

		ret = dbmd2_uart_sync(dbd2);

		dbd2_flush_rx_fifo(dbd2);

		if (!ret)
			break;

		retry++;
	} while (retry < 5);

	if (retry == 5) {
		dev_err(dbd2->dev, "multiple sync errors\n");

		uart_unlock(&dbd2->lock);
		return -EIO;
	}

	ret = dbd2_boot_firmware(dbd2);
	if (ret) {
		dev_err(dbd2->dev, "booting the firmware failed\n");

		uart_unlock(&dbd2->lock);
		return -EIO;
	}

	/* wait some time till the bytes went out */
	usleep_range(10000, 11000);

	/* set baudrate to FW baud (common case) */
	dbmd2_uart_configure_tty(dbd2,
				 dbd2->uart_tty,
				1000000 , // firmware always wakeup at this Baud
				 UART_TTY_STOP_BITS,
				 0, 1);
	dbd2_flush_rx_fifo(dbd2);

	ret = dbd2_wait_till_alive(dbd2);
	if (!ret) {
		dev_err(dbd2->dev, "failed to boot, device not responding\n");
		uart_unlock(&dbd2->lock);
		return -EIO;
	}

	if (dbd2->a_model_loaded) {
		ret = dbd2_send_cmd(dbd2, DBD2_LOAD_NEW_ACUSTIC_MODEL | 0x1,
				    NULL);
		if (ret < 0) {
			dev_err(dbd2->dev,
				"failed to set DBD2_LOAD_NEW_ACUSTIC_MODEL\n");
			ret = -EIO;
			goto out;
		}
	}

out:
	dbd2->device_ready = true;
	uart_unlock(&dbd2->lock);

	return size;
}

static ssize_t dbd2_readsmps_start(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf,
				   size_t size)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);
	int ret;

	if (!dbd2->device_ready)
		return -EAGAIN;

	uart_lock(&dbd2->lock);
	ret = dbd2_wake(dbd2);
	if (ret < 0) {
		dev_err(dbd2->dev, "unable to wake\n");
		goto out;
	}

	dbd2->buffering = 1;
	schedule_work(&dbd2->sv_work);

out:
	uart_unlock(&dbd2->lock);
	return size;
}


static ssize_t dbd2_readsmps_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);
	int ret;
	u16 val = 0;
	int i = 0;

	if (!dbd2->device_ready)
		return -EAGAIN;

	uart_lock(&dbd2->lock);
	ret = dbd2_wake(dbd2);
	if (ret < 0) {
		dev_err(dbd2->dev, "unable to wake\n");
		uart_unlock(&dbd2->lock);
		return ret;
	}

	/* A host command received will blocked until the current audio frame
	   processing is finished, which can take up to 10 ms */
	usleep_range(10000, 11000);

	val = 4000;

	#define SIZE 16
	for (i = 0; i <= val; i += SIZE) {
		char local_buf[SIZE];

		if (!dbd2->client && !dbd2->client_spi) {
			if (dbmd2_uart_read(dbd2, local_buf, SIZE) < 0) {
				dev_err(dbd2->dev, "dbmd2_uart_read failed\n");

				uart_unlock(&dbd2->lock);
				return -EIO;
			}
		} else {
			ret = i2c_master_recv(dbd2->client, local_buf, SIZE);
			if (ret < 0) {
				dev_err(dbd2->dev, "i2c_master_recv failed\n");

				uart_unlock(&dbd2->lock);
				return ret;
			}
		}
		memcpy(buf + i, local_buf, SIZE);
	}
	#undef SIZE

	uart_unlock(&dbd2->lock);
	return val;
}

static ssize_t dbd2_d2param_addr_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf,
				       size_t size)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	if (!dbd2->device_ready)
		return -EAGAIN;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	uart_lock(&dbd2->lock);
	ret = dbd2_wake(dbd2);
	if (ret < 0) {
		dev_err(dbd2->dev, "unable to wake\n");
		uart_unlock(&dbd2->lock);
		return ret;
	}

	ret = dbd2_send_cmd(dbd2, DBD2_SET_D2PARAM_ADDR | (u32)val, NULL);

	if (ret < 0) {
		dev_err(dbd2->dev, "set d2paramaddr error\n");
		uart_unlock(&dbd2->lock);
		return ret;
	}

	uart_unlock(&dbd2->lock);
	return size;
}

static ssize_t dbd2_d2param_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);
	int ret;
	u16 val;

	if (!dbd2->device_ready)
		return -EAGAIN;

	uart_lock(&dbd2->lock);
	ret = dbd2_wake(dbd2);
	if (ret < 0) {
		dev_err(dbd2->dev, "unable to wake\n");
		uart_unlock(&dbd2->lock);
		return ret;
	}

	ret = dbd2_send_cmd_short(dbd2, DBD2_GET_D2PARAM, &val);

	if (ret < 0) {
		dev_err(dbd2->dev, "get d2param error\n");
		uart_unlock(&dbd2->lock);
		return ret;
	}

	uart_unlock(&dbd2->lock);
	return sprintf(buf, "%u\n", val);
}

static ssize_t dbd2_d2param_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t size)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	if (!dbd2->device_ready)
		return -EAGAIN;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	uart_lock(&dbd2->lock);
	ret = dbd2_wake(dbd2);
	if (ret < 0) {
		dev_err(dbd2->dev, "unable to wake\n");
		uart_unlock(&dbd2->lock);
		return ret;
	}

	ret = dbd2_send_cmd(dbd2, DBD2_SET_D2PARAM | (u32)val, NULL);
	if (ret < 0) {
		dev_err(dbd2->dev, "set d2param error\n");
		uart_unlock(&dbd2->lock);
		return ret;
	}

	uart_unlock(&dbd2->lock);
	return size;
}

static ssize_t dbd2_acoustic_model_filename_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);
	char *c;
	size_t len;

	mutex_lock(&dbd2->lock);
	kfree(dbd2->amodel_fw_name);
	dbd2->amodel_fw_name = kstrndup(buf, 100, GFP_KERNEL);
	len = strlen(dbd2->amodel_fw_name);
	if (len > 0) {
		c = &dbd2->amodel_fw_name[len - 1];
		if (*c == '\n')
			*c = '\0';
	}
	mutex_unlock(&dbd2->lock);
	return (size > 100 ? 100 : size);
}

static ssize_t dbd2_acoustic_model_filename_show(struct device *dev,
						 struct device_attribute *attr,
						 char *buf)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);
	ssize_t ret;

	mutex_lock(&dbd2->lock);
	ret = sprintf(buf, "%s", dbd2->amodel_fw_name);
	mutex_unlock(&dbd2->lock);

	return ret;
}

static ssize_t dbd2_rxsize_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);

	return sprintf(buf, "%lu\n", dbd2->rxsize);
}

static ssize_t dbd2_rxsize_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf,
				 size_t size)
{
	int ret;
	unsigned long val;
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	if (val % 16 != 0)
		return -EINVAL;

	dbd2->rxsize = val;

	return size;
}

static ssize_t dbd2_rsize_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);

	return sprintf(buf, "%lu\n", dbd2->rsize);
}

static ssize_t dbd2_rsize_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t size)
{
	int ret;
	unsigned long val;
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	if (val == 0 || (val % 2 != 0) || val > UART_TTY_MAX_READ_SZ)
		return -EINVAL;

	dbd2->rsize = val;

	return size;
}

static ssize_t dbd2_wsize_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);

	return sprintf(buf, "%lu\n", dbd2->wsize);
}

static ssize_t dbd2_wsize_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t size)
{
	int ret;
	unsigned long val;
	struct dbd2_data *dbd2 = dev_get_drvdata(dev);

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	if (val == 0 || (val % 2 != 0) || val > UART_TTY_MAX_WRITE_SZ)
		return -EINVAL;

	dbd2->wsize = val;

	return size;
}

static ssize_t dbd2_hw_rev_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%d\n", system_rev);
}

static DEVICE_ATTR(fwver, S_IRUGO,
		   dbd2_fw_ver_show, NULL);
static DEVICE_ATTR(opmode,  S_IRUGO | S_IWUSR ,
		   dbd2_opr_mode_show, dbd2_opr_mode_store);
static DEVICE_ATTR(trigger, S_IRUGO | S_IWUSR,
		   dbd2_trigger_level_show, dbd2_trigger_level_store);
static DEVICE_ATTR(uart, S_IWUSR,
		   NULL, dbd2_uart_store);
static DEVICE_ATTR(verif, S_IRUGO | S_IWUSR,
		   dbd2_verification_level_show, dbd2_verification_level_store);
static DEVICE_ATTR(gain, S_IRUGO | S_IWUSR,
		   dbd2_gain_shift_factor_show, dbd2_gain_shift_factor_store);
static DEVICE_ATTR(io_addr, S_IRUGO | S_IWUSR,
		   dbd2_io_addr_show, dbd2_io_addr_store);
static DEVICE_ATTR(io_value, S_IRUGO | S_IWUSR,
		   dbd2_io_value_show, dbd2_io_value_store);
static DEVICE_ATTR(buffsize, S_IRUGO,
		   dbd2_buffer_size_show, NULL);
static DEVICE_ATTR(buffsmps, S_IRUGO,
		   dbd2_buffsmps_show, NULL);
static DEVICE_ATTR(max_sample, S_IRUGO,
		   dbd2_max_sample_show, NULL);
static DEVICE_ATTR(detect_word, S_IRUGO,
		   dbd2_last_detect_show, NULL);
static DEVICE_ATTR(detect_trigger, S_IRUGO,
		   dbd2_detect_trigger_show, NULL);
static DEVICE_ATTR(detect_verification, S_IRUGO,
		   dbd2_last_detect_verication_show, NULL);
static DEVICE_ATTR(load_model, S_IRUGO | S_IWUSR ,
		   NULL, dbd2_acoustic_model_store);
static DEVICE_ATTR(audioconv, S_IRUGO | S_IWUSR,
		   dbd2_audio_conv_show, dbd2_audio_conv_store);
static DEVICE_ATTR(readbuf, S_IRUGO | S_IWUSR,
		   dbd2_readsmps_show, dbd2_readsmps_start);
static DEVICE_ATTR(uartspeed, S_IRUGO | S_IWUSR,
		   dbd2_uartspeed_show, dbd2_uartspeed_store);
static DEVICE_ATTR(lastduration, S_IRUGO,
		   dbd2_lastduration_show, NULL);
static DEVICE_ATTR(lasterror, S_IRUGO,
		   dbd2_lasterror_show, NULL);
static DEVICE_ATTR(micgain, S_IRUGO | S_IWUSR,
		   dbd2_micgain_show, dbd2_micgain_store);
static DEVICE_ATTR(reset, S_IWUSR,
		   NULL, dbd2_reset_store);
static DEVICE_ATTR(d2paramaddr, S_IWUSR,
		   NULL, dbd2_d2param_addr_store);
static DEVICE_ATTR(d2param, S_IRUGO | S_IWUSR,
		   dbd2_d2param_show, dbd2_d2param_store);
static DEVICE_ATTR(amodel_filename, S_IRUGO | S_IWUSR,
		   dbd2_acoustic_model_filename_show,
		   dbd2_acoustic_model_filename_store);
static DEVICE_ATTR(backlog_size, S_IRUGO | S_IWUSR,
		   dbd2_backlog_size_show, dbd2_backlog_size_store);
static DEVICE_ATTR(detection, S_IRUGO | S_IWUSR,
		   dbd2_detection_show, dbd2_detection_store);
static DEVICE_ATTR(buffering, S_IRUGO | S_IWUSR,
		   dbd2_buffering_show, dbd2_buffering_store);
static DEVICE_ATTR(rxsize, S_IRUGO | S_IWUSR,
		   dbd2_rxsize_show, dbd2_rxsize_store);
static DEVICE_ATTR(rsize, S_IRUGO | S_IWUSR,
		   dbd2_rsize_show, dbd2_rsize_store);
static DEVICE_ATTR(wsize, S_IRUGO | S_IWUSR,
		   dbd2_wsize_show, dbd2_wsize_store);

static DEVICE_ATTR(hw_rev, S_IRUGO | S_IWUSR,
		   dbd2_hw_rev_show, NULL);

static struct attribute *dbd2_attributes[] = {
	&dev_attr_fwver.attr,
	&dev_attr_opmode.attr,
	&dev_attr_trigger.attr,
	&dev_attr_uart.attr,
	&dev_attr_verif.attr,
	&dev_attr_gain.attr,
	&dev_attr_io_addr.attr,
	&dev_attr_io_value.attr,
	&dev_attr_buffsize.attr,
	&dev_attr_buffsmps.attr,
	&dev_attr_max_sample.attr,
	&dev_attr_detect_word.attr,
	&dev_attr_detect_trigger.attr,
	&dev_attr_detect_verification.attr,
	&dev_attr_uartspeed.attr,
	&dev_attr_lastduration.attr,
	&dev_attr_lasterror.attr,
	&dev_attr_micgain.attr,
	&dev_attr_load_model.attr,
	&dev_attr_readbuf.attr,
	&dev_attr_audioconv.attr,
	&dev_attr_reset.attr,
	&dev_attr_d2paramaddr.attr,
	&dev_attr_d2param.attr,
	&dev_attr_amodel_filename.attr,
	&dev_attr_backlog_size.attr,
	&dev_attr_detection.attr,
	&dev_attr_buffering.attr,
	&dev_attr_rxsize.attr,
	&dev_attr_rsize.attr,
	&dev_attr_wsize.attr,
	&dev_attr_hw_rev.attr,
	NULL,
};

static const struct attribute_group dbd2_attribute_group = {
	.attrs = dbd2_attributes,
};

int dbmd2_get_frames(char *buffer, unsigned int channels, unsigned int frames)
{
	struct dbd2_data *dbd2 = dbd2_data;
	unsigned int samples_avail;
	unsigned int samples;
	unsigned int total;
	u16 tmp[2];

	if (!dbd2)
		return -EAGAIN;

	if (!dbd2->device_ready) {
		dev_err(dbd2->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	total = frames * dbd2->bytes_per_sample * channels;

	if (channels > 2 || channels < 1)
		return -EINVAL;

	samples = frames * channels;

	samples_avail = kfifo_len(&dbd2->pcm_kfifo) / dbd2->bytes_per_sample;

	if (samples_avail < samples)
		return -1;

	if (channels == 1) {
		unsigned int i;
		for (i = 0; i < samples; i++) {
			if (kfifo_out(&dbd2->pcm_kfifo,
				      tmp,
				      dbd2->bytes_per_sample * 2))
				return -1;
			memcpy(&buffer[i * dbd2->bytes_per_sample],
			       tmp,
			       dbd2->bytes_per_sample);
		}
	} else {
		if (kfifo_out(&dbd2->pcm_kfifo, buffer, total))
			return -1;
	}

	return 0;
}
EXPORT_SYMBOL(dbmd2_get_frames);

int dbmd2_codec_lock(void)
{
	if (!atomic_add_unless(&dbd2_data->audio_owner, 1, 1))
		return -EBUSY;

	return 0;
}
EXPORT_SYMBOL(dbmd2_codec_lock);

int dbmd2_codec_unlock(void)
{
	atomic_dec(&dbd2_data->audio_owner);

	return 0;
}
EXPORT_SYMBOL(dbmd2_codec_unlock);

void dbmd2_start_buffering(void)
{
	int ret;

	if (!dbd2_data->auto_buffering)
		return;

	uart_lock(&dbd2_data->lock);
	dbd2_data->buffering = 1;
	ret = dbd2_set_mode(dbd2_data, DBMD2_BUFFERING);
	uart_unlock(&dbd2_data->lock);
}
EXPORT_SYMBOL(dbmd2_start_buffering);

void dbmd2_stop_buffering(void)
{
	if (!dbd2_data->auto_buffering)
		return;

	dbd2_data->buffering = 0;

	flush_work(&dbd2_data->sv_work);

	uart_lock(&dbd2_data->lock);
	dbd2_set_mode(dbd2_data, DBMD2_IDLE);
	uart_unlock(&dbd2_data->lock);
}
EXPORT_SYMBOL(dbmd2_stop_buffering);


/* ------------------------------------------------------------------------
 * codec driver section
 * ------------------------------------------------------------------------ */

#define DUMMY_REGISTER 0

static int dbmd2_dai_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int ret = 0;
	struct dbd2_data *dbd2 = snd_soc_codec_get_drvdata(rtd->codec);

	dbd2->channels = params_channels(params);

	/* nothing to do on firmware side */
	dev_info(dbd2->dev, "streaming with %d channels\n",
		 dbd2->channels);

	mutex_lock(&dbd2->lock);
	if (dbd2_sleeping(dbd2)) {
		ret = -EIO;
		goto out_unlock;
	}
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		dbd2_set_bytes_per_sample(dbd2, DBD2_AUDIO_MODE_PCM);
		break;
	case SNDRV_PCM_FORMAT_MU_LAW:
		dbd2_set_bytes_per_sample(dbd2, DBD2_AUDIO_MODE_MU_LAW);
		break;
	default:
		ret = -EINVAL;
	}

out_unlock:
	mutex_unlock(&dbd2->lock);

	return ret;
}

static struct snd_soc_dai_ops dbmd2_dai_ops = {
	.hw_params = dbmd2_dai_hw_params,
};

/* DBMD2 codec DAI: */
struct snd_soc_dai_driver dbmd2_dais[] = {
	{
		.name = "DBMD2_codec_dai",
		.capture = {
			.stream_name	= "vs_buffer",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_8000 |
					  SNDRV_PCM_RATE_16000,
			.formats	= SNDRV_PCM_FMTBIT_MU_LAW |
					  SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &dbmd2_dai_ops,
	},
};

/* ASoC controls */
static unsigned int dbmd2_dev_read(struct snd_soc_codec *codec,
				   unsigned int reg)
{
	struct dbd2_data *dbd2;
	int ret;
	u16 val = 0;

	dbd2 = dbd2_data;

	if (!dbd2)
		return -EAGAIN;

	if (!dbd2->device_ready) {
		dev_err(dbd2->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}


	if (reg == DUMMY_REGISTER)
		return 0;

	uart_lock(&dbd2->lock);

	/* just return 0 - the user needs to wakeup first */
	if (dbd2_sleeping(dbd2)) {
		dev_err(dbd2->dev, "device sleeping\n");
		goto out_unlock;
	}

	ret = dbd2_send_cmd_short(dbd2, 0x80000000 | ((reg & 0xff) << 16),
				  &val);
	if (ret < 0)
		dev_err(dbd2->dev, "read 0x%x error\n", reg);

out_unlock:

	uart_unlock(&dbd2->lock);
	return (unsigned int)val;
}

static int dbmd2_dev_write(struct snd_soc_codec *codec,
			   unsigned int reg,
			   unsigned int val)
{
	struct dbd2_data *dbd2;
	int ret;

	dbd2 = dbd2_data;

	if (!dbd2)
		return -EAGAIN;

	if (!dbd2->device_ready) {
		dev_err(dbd2->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}


	if (!snd_soc_codec_writable_register(codec, reg)) {
		dev_err(dbd2->dev, "register not writable\n");
		return -EIO;
	}

	if (reg == DUMMY_REGISTER)
		return 0;

	uart_lock(&dbd2->lock);

	/* TODO maybe just return and the user needs to wakeup */
	if (dbd2_sleeping(dbd2)) {
		ret = -EIO;
		dev_err(dbd2->dev, "device sleeping\n");
		goto out_unlock;
	}

	ret = dbd2_send_cmd(dbd2, 0x80000000 | ((reg & 0xff) << 16) |
			    (val & 0xffff), NULL);
	if (ret < 0)
		dev_err(dbd2->dev, "write 0x%x to 0x%x error\n", val, reg);

out_unlock:
	uart_unlock(&dbd2->lock);

	return ret;
}

static int dbmd2_control_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	struct dbd2_data *dbd2;
	unsigned short val, reg = mc->reg;
	int max = mc->max;
	int mask = (1 << fls(max)) - 1;
	int ret;

	dbd2 = dbd2_data;

	if (!dbd2)
		return -EAGAIN;

	if (!dbd2->device_ready) {
		dev_err(dbd2->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}


	uart_lock(&dbd2->lock);

	/* TODO maybe just return and the user needs to wakeup */
	if (dbd2_sleeping(dbd2)) {
		dev_err(dbd2->dev, "device sleeping\n");
		goto out_unlock;
	}

	ret = dbd2_send_cmd_short(dbd2, 0x80000000 | ((reg & 0xff) << 16),
				  &val);
	if (ret < 0)
		dev_err(dbd2->dev, "read 0x%x error\n", reg);

	val &= mask;

	ucontrol->value.integer.value[0] = val;

out_unlock:
	uart_unlock(&dbd2->lock);

	return 0;
}

static int dbmd2_control_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	struct dbd2_data *dbd2;
	unsigned short val = ucontrol->value.integer.value[0];
	unsigned short reg = mc->reg;
	int max = mc->max;
	int mask = (1 << fls(max)) - 1;
	int ret;

	dbd2 = dbd2_data;

	if (!dbd2)
		return -EAGAIN;

	if (!dbd2->device_ready) {
		dev_err(dbd2->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}


	if (!snd_soc_codec_writable_register(codec, reg)) {
		dev_err(dbd2->dev, "register not writable\n");
		return -EIO;
	}

	val &= mask;

	uart_lock(&dbd2->lock);


	/* TODO maybe just return and the user needs to wakeup */
	if (dbd2_sleeping(dbd2)) {
		dev_err(dbd2->dev, "device sleeping\n");
		goto out_unlock;
	}

	ret = dbd2_send_cmd(dbd2, 0x80000000 | ((reg & 0xff) << 16) |
			    (val & 0xffff), NULL);
	if (ret < 0)
		dev_err(dbd2->dev, "write 0x%x to 0x%x error\n", val, reg);

out_unlock:
	uart_unlock(&dbd2->lock);

	return 0;
}

/* Operation modes */
static int dbmd2_operation_mode_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct dbd2_data *dbd2;
	unsigned short val;
	int ret;

	dbd2 = dbd2_data;

	if (!dbd2)
		return -EAGAIN;

	if (!dbd2->device_ready) {
		dev_err(dbd2->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	if (dbd2_sleeping(dbd2)) {
		/* report hibernate */
		ucontrol->value.integer.value[0] = 3;
		return 0;
	}

	uart_lock(&dbd2->lock);

	ret = dbd2_send_cmd_short(dbd2, DBD2_OPR_MODE, &val);
	if (ret < 0) {
		dev_err(dbd2->dev, "failed to read DBD2_OPR_MODE\n");
		goto out_unlock;
	}

	/* Remove detectinon mode type from mode */
	val &= DMD2_OPERATION_MODE_MASK;

	if (val == DBMD2_SLEEP_PLL_ON)
		ucontrol->value.integer.value[0] = 1;
	else if (val == DBMD2_SLEEP_PLL_OFF)
		ucontrol->value.integer.value[0] = 2;
	else if (val == DBMD2_HIBERNATE)
		ucontrol->value.integer.value[0] = 3;
	else if (val == DBMD2_BUFFERING)
		ucontrol->value.integer.value[0] = 5;
	else if (val == DBMD2_DETECTION)
		ucontrol->value.integer.value[0] = 4;
	else if (val == DBMD2_IDLE)
		ucontrol->value.integer.value[0] = 0;
	else
		dev_err(dbd2->dev, "unknown operation mode: %u\n", val);

out_unlock:
	uart_unlock(&dbd2->lock);

	return ret;
}

static int dbmd2_operation_mode_set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct dbd2_data *dbd2;
	int ret = 0;

	dbd2 = dbd2_data;

	if (!dbd2)
		return -EAGAIN;

	if (!dbd2->device_ready) {
		dev_err(dbd2->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	uart_lock(&dbd2->lock);

	ret = dbd2_wake(dbd2);
	if (ret < 0) {
		dev_err(dbd2->dev, "%s: unable to wake\n", __func__);
		goto out_unlock;
	}


	if (ucontrol->value.integer.value[0] == 0)
		dbd2_set_mode(dbd2, DBMD2_IDLE);
	else if (ucontrol->value.integer.value[0] == 1)
		dbd2_set_mode(dbd2, DBMD2_SLEEP_PLL_ON);
	else if (ucontrol->value.integer.value[0] == 2)
		dbd2_set_mode(dbd2, DBMD2_SLEEP_PLL_OFF);
	else if (ucontrol->value.integer.value[0] == 3)
		dbd2_set_mode(dbd2, DBMD2_HIBERNATE);
	else if (ucontrol->value.integer.value[0] == 4)
		dbd2_set_mode(dbd2, DBMD2_DETECTION);
	else if (ucontrol->value.integer.value[0] == 5)
		dbd2_set_mode(dbd2, DBMD2_BUFFERING);
	else
		ret = -EINVAL;

out_unlock:
	uart_unlock(&dbd2->lock);

	return ret;
}

static const char * const dbmd2_operation_mode_texts[] = {
	"Idle", "SleepPllOn", "SleepPllOff",
	"Hibernate", "Detection", "Buffering"
};

static const struct soc_enum dbmd2_operation_mode_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dbmd2_operation_mode_texts),
			    dbmd2_operation_mode_texts);

static DECLARE_TLV_DB_SCALE(dbmd2_db_tlv, -3276700, 3276800, 0);

static const unsigned int dbmd2_mic_analog_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	0, 21, TLV_DB_SCALE_ITEM(-400, 200, 0),
};

static int dbmd2_amodel_load_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 3;
	return 0;
}

static int dbmd2_amodel_load_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct dbd2_data *dbd2;
	unsigned short value = ucontrol->value.integer.value[0];

	dbd2 = dbd2_data;

	if (!dbd2)
		return -EAGAIN;

	dev_dbg(dbd2->dev, "%s: value - %d\n", __func__, value);

	if (!dbd2->device_ready) {
		dev_err(dbd2->dev, "%s: device not ready\n", __func__);
		return 0;
	}


	if (value == 0) {
		int ret;
		dev_info(dbd2->dev, "sleep mode: %d\n", value);

		uart_lock(&dbd2->lock);

		if (dbd2_sleeping(dbd2)) {
			uart_unlock(&dbd2->lock);
			return 0;
		}


		ret = dbd2_set_mode(dbd2, DBMD2_IDLE);

		if (ret)
			dev_err(dbd2->dev, "failed to set device to idle mode\n");

		ret = dbd2_set_uart_speed(dbd2, DBD2_UART_SPEED_57600);

		if (ret)
			dev_err(dbd2->dev, "failed to change UART speed to normal amodel\n");

		ret = dbd2_set_mode(dbd2, DBMD2_SLEEP_PLL_OFF);

		if (ret)
			dev_err(dbd2->dev, "failed to set device to sleep mode\n");



		uart_unlock(&dbd2->lock);

		return 0;
	} else if (value > 2) {
		dev_err(dbd2->dev, "Unsupported mode:%d\n", value);
		return -1;
	}

	dev_info(dbd2->dev, "Detection mode mode: %d\n", value);

	mutex_lock(&dbd2->lock);

	dbd2->change_speed = 1;

	if (value == 2)
		dbd2->detection_mode = DETECTION_MODE_VOICE_ENERGY;
	else
		dbd2->detection_mode = DETECTION_MODE_PHRASE;

	mutex_unlock(&dbd2->lock);

	/* trigger loading of new acoustic model */
	return dbd2_load_new_acoustic_model(dbd2);
}

#ifdef MSM_G1
static int dbmd2_lpm_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct dbd2_data *dbd2;

	dbd2 = dbd2_data;

	if (!dbd2)
		return -EAGAIN;

	if (!dbd2->device_ready) {
		dev_err(dbd2->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	ucontrol->value.enumerated.item[0] = dbd2->lpm_mode;

	return 0;
}

static int dbmd2_lpm_set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct dbd2_data *dbd2;
	int value = ucontrol->value.enumerated.item[0];

	dbd2 = dbd2_data;

	if (!dbd2)
		return -EAGAIN;

	if (!dbd2->device_ready) {
		dev_err(dbd2->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	dbd2->lpm_mode = value;
	return 0;
}
#endif

static int dbmd2_wakeup_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct dbd2_data *dbd2;

	dbd2 = dbd2_data;

	if (!dbd2)
		return -EAGAIN;

	ucontrol->value.enumerated.item[0] = dbd2_sleeping(dbd2);

	return 0;
}

static int dbmd2_wakeup_set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct dbd2_data *dbd2;
	int value = ucontrol->value.enumerated.item[0];

	dbd2 = dbd2_data;

	if (!dbd2)
		return -EAGAIN;

	mutex_lock(&dbd2->lock);
	if (value) {
#if USE_SLEEPMODE_GPIO_FOR_WAKEUP
		gpio_set_value(dbd2->pdata.gpio_wakeup, 1);
		dbd2->asleep = true;
#elif !USE_WAKEUP_GPIO_FOR_DETECTION
		gpio_set_value(dbd2->pdata.gpio_wakeup, 0);
		dbd2->asleep = true;
#endif
	} else {
#if USE_SLEEPMODE_GPIO_FOR_WAKEUP
		gpio_set_value(dbd2->pdata.gpio_wakeup, 0);
		dbd2->asleep = false;
#elif !USE_WAKEUP_GPIO_FOR_DETECTION
		gpio_set_value(dbd2->pdata.gpio_wakeup, 1);
		dbd2->asleep = false;
#endif
	}
	mutex_unlock(&dbd2->lock);

	return 0;
}

/* Microphone modes */
enum dbmd2_microphone_mode {
	DBMD2_MIC_MODE_DIGITAL_LEFT = 0,
	DBMD2_MIC_MODE_DIGITAL_RIGHT,
	DBMD2_MIC_MODE_DIGITAL_STEREO_TRIG_ON_LEFT,
	DBMD2_MIC_MODE_DIGITAL_STEREO_TRIG_ON_RIGHT,
	DBMD2_MIC_MODE_ANALOG,
	DBMD2_MIC_MODE_DISABLE
};

enum dbmd2_microphone_type {
	DBMD2_MIC_TYPE_DIGITAL_LEFT = 0,
	DBMD2_MIC_TYPE_DIGITAL_RIGHT,
	DBMD2_MIC_TYPE_ANALOG
};

static int dbd2_set_microphone_mode(struct dbd2_data *dbd2, int mode)
{
	int ret1 = 0;
	int ret2 = 0;

	/* set new mode and return old one */

	dev_info(dbd2->dev, "%s: mode: %d\n", __func__, mode);

	ret1 = dbd2_send_cmd(dbd2, DBD2_MICROPHONE2_CONFIGURATION,
			    NULL);
	if (ret1 < 0) {
		dev_err(dbd2->dev, "failed to set mode 0x%x\n", mode);
		return -1;
	}

	/* anything special to do */
	switch (mode) {
	case DBMD2_MIC_MODE_DISABLE:
		ret1 = dbd2_send_cmd(dbd2, DBD2_MICROPHONE1_CONFIGURATION,
				    NULL);
		break;
	case DBMD2_MIC_MODE_DIGITAL_LEFT:
		ret1 = dbd2_send_cmd(dbd2,
			DBD2_MICROPHONE1_CONFIGURATION |
			dbd2->mic_config[DBMD2_MIC_TYPE_DIGITAL_LEFT], NULL);

		break;
	case DBMD2_MIC_MODE_DIGITAL_RIGHT:
		ret1 = dbd2_send_cmd(dbd2,
			DBD2_MICROPHONE1_CONFIGURATION |
			dbd2->mic_config[DBMD2_MIC_TYPE_DIGITAL_RIGHT], NULL);
		break;
	case DBMD2_MIC_MODE_DIGITAL_STEREO_TRIG_ON_LEFT:
		ret1 = dbd2_send_cmd(dbd2,
			DBD2_MICROPHONE1_CONFIGURATION |
			dbd2->mic_config[DBMD2_MIC_TYPE_DIGITAL_LEFT], NULL);

		ret2 = dbd2_send_cmd(dbd2,
			DBD2_MICROPHONE2_CONFIGURATION |
			dbd2->mic_config[DBMD2_MIC_TYPE_DIGITAL_RIGHT], NULL);
		break;
	case DBMD2_MIC_MODE_DIGITAL_STEREO_TRIG_ON_RIGHT:
		ret1 = dbd2_send_cmd(dbd2,
			DBD2_MICROPHONE1_CONFIGURATION |
			dbd2->mic_config[DBMD2_MIC_TYPE_DIGITAL_RIGHT], NULL);

		ret2 = dbd2_send_cmd(dbd2,
			DBD2_MICROPHONE2_CONFIGURATION |
			dbd2->mic_config[DBMD2_MIC_TYPE_DIGITAL_LEFT], NULL);
		break;
	case DBMD2_MIC_MODE_ANALOG:
		ret1 = dbd2_send_cmd(dbd2,
			DBD2_MICROPHONE1_CONFIGURATION |
			dbd2->mic_config[DBMD2_MIC_TYPE_ANALOG], NULL);
		break;
	default:
		dev_err(dbd2->dev, "Unsupported microphone mode 0x%x\n", mode);
		return -EINVAL;
		break;
	}

	if (ret1 < 0 || ret2 < 0) {
		dev_err(dbd2->dev, "failed to set mode 0x%x\n", mode);
		return -1;
	}

	return 0;
}


static int dbmd2_microphone_mode_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct dbd2_data *dbd2;
	unsigned short val, val2;
	int ret = 0;

	dbd2 = dbd2_data;

	if (!dbd2)
		return -EAGAIN;

	if (!dbd2->device_ready) {
		dev_err(dbd2->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}



	uart_lock(&dbd2->lock);

	/* just return 0 - the user needs to wakeup first */
	if (dbd2_sleeping(dbd2)) {
		dev_err(dbd2->dev, "device sleeping\n");
		goto out_unlock;
	}


	ret = dbd2_send_cmd_short(dbd2, DBD2_MICROPHONE1_CONFIGURATION, &val);
	if (ret < 0) {
		dev_err(dbd2->dev, "failed to read microphone configuration\n");
		goto out_unlock;
	}

	val &= 0x7;
	if (val == 0)
		ucontrol->value.integer.value[0] = 5; /* DISABLE */
	else if (val == 5)
		ucontrol->value.integer.value[0] = 4; /* ANALOG */
	else if (val > 0 && val < 5) { /* DIGITAL */
		ret = dbd2_send_cmd_short(dbd2,
			DBD2_MICROPHONE2_CONFIGURATION, &val2);

		if (ret < 0) {
			dev_err(dbd2->dev,
				"failed to read microphone2  configuration\n");
			goto out_unlock;
		}

		val2 &= 0x7;

		if (val2 == 0) {
			/* DIGITAL LEFT */
			if (val == 1 || val == 3)
				ucontrol->value.integer.value[0] = 0;
			/* DIGITAL RIGHT */
			else if (val == 2 || val == 4)
				ucontrol->value.integer.value[0] = 1;
		} else {
			/* DIGITAL LEFT */
			if (val == 1 || val == 3)
				ucontrol->value.integer.value[0] = 2;
			/*DIGITAL RIGHT*/
			else if (val == 2 || val == 4)
				ucontrol->value.integer.value[0] = 3;
		}
	} else
		dev_err(dbd2->dev, "unknown microphone mode: %u\n", val);

out_unlock:
	uart_unlock(&dbd2->lock);

	return ret;
}

static int dbmd2_microphone_mode_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct dbd2_data *dbd2;
	int ret = 0;

	dbd2 = dbd2_data;

	if (!dbd2)
		return -EAGAIN;

	if (!dbd2->device_ready) {
		dev_err(dbd2->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}


	uart_lock(&dbd2->lock);

	if (dbd2_sleeping(dbd2)) {
		dev_err(dbd2->dev, "device sleeping\n");
		goto out_unlock;
	}


	dev_dbg(dbd2->dev, "%s: value - %d\n", __func__,
			(int)ucontrol->value.integer.value[0]);

	/* anything special to do */
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		ret = dbd2_set_microphone_mode(dbd2,
			DBMD2_MIC_MODE_DIGITAL_LEFT);
		break;
	case 1:
		ret = dbd2_set_microphone_mode(dbd2,
			DBMD2_MIC_MODE_DIGITAL_RIGHT);
		break;
	case 2:
		ret = dbd2_set_microphone_mode(dbd2,
			DBMD2_MIC_MODE_DIGITAL_STEREO_TRIG_ON_LEFT);
		break;
	case 3:
		ret = dbd2_set_microphone_mode(dbd2,
			DBMD2_MIC_MODE_DIGITAL_STEREO_TRIG_ON_RIGHT);
		break;
	case 4:
		ret = dbd2_set_microphone_mode(dbd2,
			DBMD2_MIC_MODE_ANALOG);
		break;
	case 5:
		ret = dbd2_set_microphone_mode(dbd2, DBMD2_MIC_MODE_DISABLE);
		break;
	default:
		dev_err(dbd2->dev, "Unsupported microphone mode %d\n",
			(int)(ucontrol->value.integer.value[0]));
		ret = -EINVAL;
		break;
	}
out_unlock:
	uart_unlock(&dbd2->lock);
	return ret;
}

static const char * const dbmd2_microphone_mode_texts[] = {
	"DigitalLeft",
	"DigitalRight",
	"DigitalStereoTrigOnLeft",
	"DigitalStereoTrigOnRight",
	"Analog",
	"Disable",
};

static const struct soc_enum dbmd2_microphone_mode_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dbmd2_microphone_mode_texts),
			    dbmd2_microphone_mode_texts);

static const struct snd_kcontrol_new dbmd2_snd_controls[] = {
	SOC_ENUM_EXT("Operation mode", dbmd2_operation_mode_enum,
		dbmd2_operation_mode_get, dbmd2_operation_mode_set),
	SOC_SINGLE_EXT_TLV("Trigger threshold", 0x02, 0, 0xffff, 0,
		dbmd2_control_get, dbmd2_control_put, dbmd2_db_tlv),
	SOC_SINGLE_EXT("Verification threshold", 0x03, 0, 8192, 0,
		dbmd2_control_get, dbmd2_control_put),
	SOC_SINGLE_EXT("Gain shift value", 0x04, 0, 15, 0,
		dbmd2_control_get, dbmd2_control_put),
	SOC_SINGLE_EXT_TLV("Microphone analog gain", 0x16, 0, 0xffff, 0,
		dbmd2_control_get, dbmd2_control_put,
		dbmd2_mic_analog_gain_tlv),
#ifdef MSM_G1
	SOC_SINGLE_EXT("ES705 Voice Wakeup Enable", 0, 0, 3, 0,
			    dbmd2_amodel_load_get,
			    dbmd2_amodel_load_set),
	SOC_SINGLE_BOOL_EXT("ES705 Voice LPM Enable",
			    0,
			    dbmd2_lpm_get,
			    dbmd2_lpm_set),

#endif
	SOC_SINGLE_EXT("Load acoustic model", 0, 0, 3, 0,
		dbmd2_amodel_load_get, dbmd2_amodel_load_set),
	SOC_SINGLE_BOOL_EXT("Set wakeup",
			    0,
			    dbmd2_wakeup_get,
			    dbmd2_wakeup_set),
	SOC_SINGLE("Word ID", 0x0c, 0, 0xffff, 0),
	SOC_SINGLE("Trigger Level", 0x0d, 0, 0xffff, 0),
	SOC_SINGLE("Verification Level", 0x0e, 0, 0xffff, 0),
	SOC_SINGLE("Duration", 0x14, 0, 0xffff, 0),
	SOC_SINGLE("Error", 0x15, 0, 0xffff, 0),
	SOC_SINGLE_EXT("Backlog size", 0x12, 0, 0x0fff, 0,
		dbmd2_control_get, dbmd2_control_put),
	SOC_ENUM_EXT("Microphone mode", dbmd2_microphone_mode_enum,
		dbmd2_microphone_mode_get, dbmd2_microphone_mode_set),
};

static int dbmd2_set_bias_level(struct snd_soc_codec *codec,
				enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		break;
	case SND_SOC_BIAS_OFF:
		break;
	default:
		return -EINVAL;
	}

	/* change to new state */
	codec->dapm.bias_level = level;

	return 0;
}

static int dbmd2_dev_probe(struct snd_soc_codec *codec)
{
	codec->control_data = NULL;

	return 0;
}

static int dbmd2_dev_remove(struct snd_soc_codec *codec)
{
	return 0;
}

static int dbmd2_dev_suspend(struct snd_soc_codec *codec)
{
	return 0;
}

static int dbmd2_dev_resume(struct snd_soc_codec *codec)
{
	return 0;
}

static int dbmd2_is_writeable_register(struct snd_soc_codec *codec,
				       unsigned int reg)
{
	struct dbd2_data *dbd2;
	int ret = 1;

	dbd2 = dbd2_data;

	if (!dbd2)
		return -EAGAIN;

	if (!dbd2->device_ready) {
		dev_err(dbd2->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	switch (reg) {
	case 0xc:
	case 0xd:
	case 0xe:
	case 0x14:
	case 0x15:
		ret = 0;
		break;
	default:
		break;
	}
	return ret;
}

static struct snd_soc_codec_driver soc_codec_dev_dbmd2 = {
	.probe   = dbmd2_dev_probe,
	.remove  = dbmd2_dev_remove,
	.suspend = dbmd2_dev_suspend,
	.resume  = dbmd2_dev_resume,
	.set_bias_level = dbmd2_set_bias_level,
	.read = dbmd2_dev_read,
	.write = dbmd2_dev_write,
	.controls = dbmd2_snd_controls,
	.num_controls = ARRAY_SIZE(dbmd2_snd_controls),
	.writable_register = dbmd2_is_writeable_register,

	.reg_cache_size = 0,
	.reg_word_size = 0,
	.reg_cache_default = NULL,
	.ignore_pmdown_time = true,
};

int dbmd2_remote_add_codec_controls(struct snd_soc_codec *codec)
{
	int rc;

	dev_dbg(codec->dev, "%s start\n", __func__);

	rc = snd_soc_add_codec_controls(codec, dbmd2_snd_controls,
				ARRAY_SIZE(dbmd2_snd_controls));

	if (rc)
		dev_err(codec->dev,
			"%s(): dbmd2_remote_add_codec_controls failed\n",
			__func__);

	return rc;
}

static int dbmd2_read_data(struct dbd2_data *dbd2, unsigned int bytes_to_read)
{
#define SIZE 8
	unsigned int count = SIZE;
	int ret = -EIO;
	unsigned int i;
	char tbuf[UART_HW_FIFO_SIZE];
	unsigned long timeout;

	if (count > bytes_to_read)
		count = bytes_to_read;

	/* stuck for more than 1s means something went wrong */
	timeout = jiffies + msecs_to_jiffies(10000);

	for (i = 0; i < bytes_to_read; i += count) {
		if (dbd2->client_spi) {
			ret = dbmd2_spi_read(dbd2, tbuf, dbd2->rsize);
			if (ret > 0) {
				kfifo_in(&dbd2->pcm_kfifo, tbuf, ret);
				count = ret;
			} else
				count = 0;
		} else if (dbd2->client) {
			char local_buf[SIZE];

			if ((i + count) > bytes_to_read)
				count = bytes_to_read - i;

			if (i2c_master_recv(dbd2->client, local_buf, count)
			    < 0) {
				dev_err(dbd2->dev, "i2c_master_recv failed\n");
				goto out;
			}
			kfifo_in(&dbd2->pcm_kfifo, local_buf, count);
		}
		if (!time_before(jiffies, timeout)) {
			dev_err(dbd2->dev,
				"read data timed out after %u bytes\n",
				i);
			ret = -EIO;
			goto out;
		}
	}
	#undef SIZE
	ret = 0;
out:
	return ret;
}

static char tbuf[UART_TTY_MAX_READ_SZ];
static char tbuf_temp[TTY_MAX_HW_BUF_SIZE];

static int dbmd2_read_data_serial(struct dbd2_data *dbd2,
				  unsigned int bytes_to_read)
{
	int ret;
	unsigned int count;
	/* unsigned int i = 0; */
	/* stuck for more than 1s means something went wrong */
	unsigned long timeout = jiffies + msecs_to_jiffies(1000);
	int cur_temp_buf_index = 0;
	mm_segment_t oldfs;

	/* we may call from user context via char dev, so allow
	 * read buffer in kernel address space */
	oldfs = get_fs();
	set_fs(KERNEL_DS);

	do {
		if (bytes_to_read > dbd2->rsize)
			count = dbd2->rsize;
		else
			count = bytes_to_read;

		ret = dbd2->uart_ld->ops->read(dbd2->uart_tty,
					       dbd2->uart_file,
					       (char __user *)tbuf, count);

		if (ret > 0) {
			bytes_to_read -= ret;
			memcpy(tbuf_temp+cur_temp_buf_index, tbuf, ret);
			cur_temp_buf_index += ret;
		} else if (ret == 0 || ret == -EAGAIN) {
			usleep_range(2000, 2100);
		} else {
			dev_err(dbd2->dev,
				"fail to read err= %d bytes to read=%d\n",
				ret, bytes_to_read);
		}
	} while (time_before(jiffies, timeout) && bytes_to_read);

	if (bytes_to_read) {
		dev_err(dbd2->dev,
			"read data timed out, still %u bytes to read\n",
			bytes_to_read);
		ret = -EIO;
		goto out;
	} else {
		kfifo_in(&dbd2->pcm_kfifo, tbuf_temp, cur_temp_buf_index);
	}


	ret = 0;
out:
	/* restore old fs context */
	set_fs(oldfs);
	return ret;
}

#ifdef MSM_G1
static void dbmd2_uevent_work(struct work_struct *work)
{
	struct dbd2_data *dbd2 = container_of(work, struct dbd2_data,
		uevent_work);
	char tmp[100];
	char * const envp[] = { tmp, NULL };


	if (dbd2->detection_mode == DETECTION_MODE_VOICE_ENERGY) {
		sprintf(tmp, "VOICE_WAKEUP_WORD_ID=LPSD");
		dev_info(dbd2->dev, "%s: VOICE ENERGY\n", __func__);
	} else {
		int event_id = 0xffff;
		int ret;

		uart_lock(&dbd2->lock);

		ret = dbd2_set_uart_speed(dbd2, DBD2_UART_SPEED_1000000);
		if (ret)
			dev_warn(dbd2->dev,
					"%s: failed switch to higher speed\n",
					__func__);
		uart_unlock(&dbd2->lock);

		event_id = dbd2_d2param_get(dbd2, DBD2_D2PARAM_ALTWORDID);

		if (event_id < 0) {
			dev_err(dbd2->dev,
					"%s: failed reading WordID\n",
					__func__);
			return;
		}

		if (event_id == 3) {
			/* as per SV Algo implementer's recommendation
			 * --> it 1 */
			dev_dbg(dbd2->dev, "%s: fixing to 1\n", __func__);
			event_id = 1;
		}

		dev_info(dbd2->dev, "%s: last WordID:%d\n", __func__, event_id);

		snprintf(tmp, sizeof(tmp), "VOICE_WAKEUP_WORD_ID=%d", event_id);
	}
	kobject_uevent_env(&dbd2->dev->kobj, KOBJ_CHANGE, (char **)envp);
	dbd2->detection_mode = 0;
	dev_info(dbd2->dev, "%s - %s\n", __func__,tmp);
}

#else

void dbmd2_remote_register_event_callback(event_cb func)
{
	dbd2_data->event_callback = func;
	return;
}

static void dbmd2_uevent_work(struct work_struct *work)
{
	struct dbd2_data *dbd2 = container_of(
			work, struct dbd2_data,	uevent_work);
	int event_id;
	int ret;

	if (dbd2->detection_mode == DETECTION_MODE_VOICE_ENERGY) {
		dev_info(dbd2->dev, "%s: VOICE ENERGY\n", __func__);
		event_id = 0;
	} else {
		dev_info(dbd2->dev, "%s: PASSPHRASE\n", __func__);

		uart_lock(&dbd2->lock);
		ret = dbd2_set_uart_speed(dbd2, DBD2_UART_SPEED_1000000);
		if (ret)
			dev_warn(dbd2->dev,
					"%s: failed switch to higher speed\n",
					__func__);
		uart_unlock(&dbd2->lock);

		event_id = dbd2_d2param_get(dbd2, DBD2_D2PARAM_ALTWORDID);

		if (event_id < 0) {
			dev_err(dbd2->dev,
					"%s: failed reading WordID\n",
					__func__);
			return;
		}

		dev_info(dbd2->dev, "%s: last WordID:%d\n", __func__, event_id);

		if (event_id == 3) {
			/* as per SV Algo implementer's recommendation
			 * --> it 1 */
			dev_dbg(dbd2->dev, "%s: fixing to 1\n", __func__);
			event_id = 1;
		}
	}

	if (dbd2->event_callback)
		dbd2->event_callback(event_id);
}
#endif
static int dbmd2_serial_read_audio_data(struct dbd2_data *dbd2,
					unsigned int nr_samples)
{
	int ret;

	/* request all available samples */
	if (dbd2_send_cmd(dbd2, DBD2_POST_TRIGGER_AUDIO_BUF | nr_samples,
			  NULL) < 0) {
		dev_err(dbd2->dev,
			"failed to write DBD2_POST_TRIGGER_AUDIO_BUF\n");
		nr_samples = 0;
		goto out;
	}

	/* read data */
	ret = dbmd2_read_data_serial(dbd2, nr_samples * 8 *
				     dbd2->bytes_per_sample);
	if (ret < 0) {
		dev_err(dbd2->dev, "failed to read block of audio data\n");
		nr_samples = 0;
	}
out:
	return nr_samples;
}

static void dbmd2_sv_work(struct work_struct *work)
{
	struct dbd2_data *dbd2 = container_of(work, struct dbd2_data, sv_work);
	int ret;
	int bytes_per_sample = dbd2->bytes_per_sample;
	unsigned int bytes_to_read;
	u16 nr_samples;
	unsigned int total = 0;
	int kfifo_space = 0;
	int retries = 0;

	dbd2->pdata.irq_inuse = 0;

	uart_lock(&dbd2->lock);

	/* flush fifo */
	kfifo_reset(&dbd2->pcm_kfifo);

	if (!dbd2->client && !dbd2->client_spi) {
		ret = dbd2_set_uart_speed(dbd2, DBD2_UART_SPEED_1000000);
		if (ret) {
			dev_err(dbd2->dev, "failed switch to higher speed\n");
			goto out_fail_unlock;
		}
	}

	uart_unlock(&dbd2->lock);

	do {
		uart_lock(&dbd2->lock);
		bytes_to_read = 0;
		/* read number of samples available in audio buffer */
		if (dbd2_send_cmd_short(dbd2, DBD2_NUM_OF_SMP_IN_BUF,
					&nr_samples) < 0) {
			dev_err(dbd2->dev,
				"failed to read DBD2_NUM_OF_SMP_IN_BUF\n");
			uart_unlock(&dbd2->lock);
			goto out_fail;
		}

		if (nr_samples == 0xffff) {
			dev_err(dbd2->dev,
				"buffering mode left with %u samples\n",
				nr_samples);
			uart_unlock(&dbd2->lock);
			break;
		}

		uart_unlock(&dbd2->lock);

		/* Now fill the kfifo. The user can access the data in
		 * parallel. The kfifo is safe for concurrent access of one
		 * reader (ALSA-capture/character device) and one writer (this
		 * work-queue). */
		if (nr_samples) {
			bytes_to_read = nr_samples * 8 * bytes_per_sample;

			/* limit transaction size (no software flow control ) */
			if (bytes_to_read > dbd2->rxsize && dbd2->rxsize)
				bytes_to_read = dbd2->rxsize;

			kfifo_space = kfifo_avail(&dbd2->pcm_kfifo);

			if (bytes_to_read > kfifo_space)
				bytes_to_read = kfifo_space;

			nr_samples = bytes_to_read / (8 * bytes_per_sample);
			if (nr_samples) {
				uart_lock(&dbd2->lock);
				if (!dbd2->client && !dbd2->client_spi) {
					/* UART case */
					ret = dbmd2_serial_read_audio_data(dbd2,
							      nr_samples);
					bytes_to_read = ret * 8 *
							bytes_per_sample;
				} else {
					/* I2C, SPI case */
					ret = dbmd2_read_data(dbd2,
							      bytes_to_read);
					if (ret < 0) {
						dev_err(dbd2->dev, "failed to read block of audio data\n");
						break;
					}
				}
				uart_unlock(&dbd2->lock);
				total += bytes_to_read;
			} else {
				usleep_range(5000, 6000);
				retries++;
				if (retries > MAX_RETRIES_TO_WRITE_TOBUF)
					break;
			}
		} else
			usleep_range(10000, 11000);

	} while (dbd2->buffering);

	dbd2->audio_processed = 0;
	dev_info(dbd2->dev, "audio buffer read, total of %u bytes\n", total);

out_fail:
	uart_lock(&dbd2->lock);
	dbd2->buffering = 0;

	if (dbd2_sleeping(dbd2)) {
		uart_unlock(&dbd2->lock);
		return;
	}

	ret = dbd2_set_mode(dbd2, DBMD2_IDLE);
	if (ret) {
		dev_err(dbd2->dev, "failed to set device to idle mode\n");
		goto out_fail_unlock;
	}
	if (!dbd2->client && !dbd2->client_spi) {
		ret = dbd2_set_uart_speed(dbd2, DBD2_UART_SPEED_57600);
		if (ret)
			dev_err(dbd2->dev, "failed to change UART speed to normal\n");
	}
	ret = dbd2_set_mode(dbd2, DBMD2_SLEEP_PLL_OFF);
	if (ret) {
		dev_err(dbd2->dev, "failed to set device to sleep mode\n");
		goto out_fail_unlock;
	}

out_fail_unlock:
	uart_unlock(&dbd2->lock);
}

static void dbmd2_detection(struct dbd2_data *dbd2)
{
	dbd2->detection_state = 1;

	schedule_work(&dbd2->uevent_work);

	dbd2->buffering = 1;
	schedule_work(&dbd2->sv_work);
	//queue_work(system_long_wq, &dbd2->sv_work);

	dev_info(dbd2->dev, "%s - SV EVENT\n", __func__);
}

#ifndef USE_WAKEUP_FROM_UART

irqreturn_t dbmd2_sv_interrupt(int irq, void *dev)
{
	struct dbd2_data *dbd2 = (struct dbd2_data *)dev;
	u16 state;

	dev_dbg(dbd2->dev, "%s\n", __func__);
	if ((dbd2->device_ready) && (dbd2->pdata.irq_inuse)) {

		uart_lock(&dbd2->lock);
		state = dbd2_get_mode(dbd2);
		atomic_set(&dbd2->keep_uart_tty_enabled,1);
		uart_unlock(&dbd2->lock);

		if(state < 0) {
			dev_err(dbd2->dev, "%s - Failed to read mode\n", __func__);
		} else if ( state != DBMD2_DETECTION){
			dbd2->pdata.irq_inuse = 0;
			dbmd2_detection(dbd2);
		}
	}
	return IRQ_HANDLED;
}

irqreturn_t dbmd2_sv_interrupt_fn(int irq, void *dev)
{
	struct dbd2_data *dbd2 = (struct dbd2_data *)dev;

	if (dbd2 && (dbd2->device_ready) && (dbd2->pdata.irq_inuse))

		return IRQ_WAKE_THREAD;

	return IRQ_HANDLED;
}

#else

static int dbmd2_detect_interrupt_thread(void *dev)
{
	int ret;
	struct dbd2_data *dbd2 = (struct dbd2_data *)dev;
	u8 data[2];

	while (!kthread_should_stop()) {
		wait_event_interruptible(dbd2->irq_wq,
					 kthread_should_stop() ||
					 atomic_read(&dbd2->do_detection) == 1);

		while (atomic_read(&dbd2->do_detection) == 1) {
			ret = dbmd2_uart_read(dbd2, data, 2);
			if (ret > 0 && (data[0] == 0xfd || data[1] == 0xfd)) {
				dbmd2_detection(dbd2);
				break;
			}
			msleep(10);
		}
		atomic_set(&dbd2->do_detection, 0);
	}

	return 0;
}

#endif

/* Access to the audio buffer is controlled through "audio_owner". Either the
 * character device or the ALSA-capture device can be opened. */
static int dbmd2_record_open(struct inode *inode, struct file *file)
{
	if (!atomic_add_unless(&dbd2_data->audio_owner, 1, 1))
		return -EBUSY;

	file->private_data = dbd2_data;

	return 0;
}

static int dbmd2_record_release(struct inode *inode, struct file *file)
{
	uart_lock(&dbd2_data->lock);
	dbd2_data->buffering = 0;
	uart_unlock(&dbd2_data->lock);

	flush_work(&dbd2_data->sv_work);

	atomic_dec(&dbd2_data->audio_owner);

	return 0;
}

/* The write function is a hack to load the A-model on systems where the
 * firmware files are not accesible to the user. */
static ssize_t dbmd2_record_write(struct file *file,
				  const char __user *buf,
				  size_t count_want,
				  loff_t *f_pos)
{
	return count_want;
}

/* Read out of the kfifo (as long as data is available). */
static ssize_t dbmd2_record_read(struct file *file,
				 char __user *buf,
				 size_t count_want,
				 loff_t *f_pos)
{
	struct dbd2_data *dbd2 = (struct dbd2_data *)file->private_data;

	size_t not_copied;
	ssize_t to_copy = count_want;
	int avail;
	unsigned int copied, total_copied = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(2000);

	avail = kfifo_len(&dbd2->pcm_kfifo);

	if ((avail == 0) &&  (dbd2->buffering)) {
		usleep_range(100000, 110000);
		avail = kfifo_len(&dbd2->pcm_kfifo);
	}

	while ((total_copied < count_want) && time_before(jiffies, timeout)
		&& avail) {
		int ret;

		to_copy = avail;
		if (count_want - total_copied < avail)
			to_copy = count_want - total_copied;

		ret = kfifo_to_user(&dbd2->pcm_kfifo, buf + total_copied,
							to_copy, &copied);
		if (ret)
			return -EIO;

		total_copied += copied;

		avail = kfifo_len(&dbd2->pcm_kfifo);

		if (avail == 0 &&  dbd2->buffering) {
			usleep_range(100000, 110000);
			avail = kfifo_len(&dbd2->pcm_kfifo);
		}
	}

	if (avail && (total_copied < count_want))
		dev_err(dbd2->dev, "dbmd2: timeout during reading\n");

	not_copied = count_want - total_copied;
	*f_pos = *f_pos + (count_want - not_copied);

	return count_want - not_copied;
}

static const struct file_operations record_fops = {
	.owner   = THIS_MODULE,
	.open    = dbmd2_record_open,
	.release = dbmd2_record_release,
	.read    = dbmd2_record_read,
	.write   = dbmd2_record_write,
};

static int dbmd2_common_probe(struct dbd2_data *dbd2)
{
	struct device_node *np = dbd2->dev->of_node;
	struct dbd2_platform_data *pdata;
	int ret = 0;

	dbd2_data = dbd2;
	dev_set_drvdata(dbd2->dev, dbd2);
	pdata = &dbd2->pdata;

	dbd2->amodel_fw_name = kzalloc(strlen(DBD2_MODEL_NAME), GFP_KERNEL);
	if (!dbd2->amodel_fw_name) {
		dev_err(dbd2->dev, "out of memory\n");
		goto err_kfree;
	}
	strncpy(dbd2->amodel_fw_name, DBD2_MODEL_NAME, strlen(DBD2_MODEL_NAME));

	dbd2->amodel_buf = vmalloc(MAX_AMODEL_SIZE);
	if (!dbd2->amodel_buf) {
		dev_err(dbd2->dev, "out of memory\n");
		goto err_kfree2;
	}

	atomic_set(&dbd2->audio_owner, 0);

	if (!np) {
		dev_err(dbd2->dev, "error no devicetree entry\n");
		ret = -ENODEV;
		goto err_kfree3;
	}

/* reset */
#ifdef MSM_G1
	ret = of_property_read_u32(np, "reset-gpio", &pdata->gpio_reset);
	if (ret && ret != -EINVAL) {
		dev_err(dbd2->dev, "invalid 'reset-gpio'\n");
		goto err_kfree3;
	}

#else
	pdata->gpio_reset = of_get_named_gpio(np, "reset-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_reset)) {
		dev_err(dbd2->dev, "reset gpio invalid\n");
		ret = -EINVAL;
		goto err_kfree3;
	}
#endif
	ret = gpio_request(pdata->gpio_reset, "DBMD2 reset");
	if (ret < 0) {
		dev_err(dbd2->dev, "error requesting reset gpio\n");
		goto err_kfree3;
	}
	gpio_direction_output(pdata->gpio_reset, 0);
	gpio_set_value(pdata->gpio_reset, 0);

	ret = of_property_read_u32(np, "freq", &dbd2->freq);
	if (ret && ret != -EINVAL) {
		dev_err(dbd2->dev, "invalid 'freq'\n");
		goto err_kfree3;
	}
	if (dbd2->freq == 0) {
		/* default is 24.576MHz */
#ifdef MSM_G1
		dbd2->freq = 32768;
#else
		dbd2->freq = 32768;
#endif
	}

	dbd2_clk_enable(true);
	msleep(100);
#ifndef USE_WAKEUP_FROM_UART
	/* Speaker Verification */
	pdata->gpio_sv = of_get_named_gpio(np, "sv-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_sv)) {
		dev_err(dbd2->dev, "sv gpio invalid %d\n",
			pdata->gpio_sv);
		goto err_gpio_free;
	}

	ret = gpio_request(pdata->gpio_sv, "DBMD2 sv");
	if (ret < 0) {
		dev_err(dbd2->dev, "error requesting sv gpio\n");
		goto err_gpio_free;
	}
	gpio_direction_input(pdata->gpio_sv);

/* interrupt gpio */
	pdata->sv_irq = ret = gpio_to_irq(pdata->gpio_sv);
	if (ret < 0) {
		dev_err(dbd2->dev, "cannot mapped gpio to irq\n");
		goto err_gpio_free2;
	}
#else
	init_waitqueue_head(&dbd2->irq_wq);
	atomic_set(&dbd2->do_detection, 0);

	dbd2->detection_thread = kthread_run(dbmd2_detect_interrupt_thread,
					     (void *)dbd2,
					     "dbmd2 detection thread");
	if (IS_ERR_OR_NULL(dbd2->detection_thread)) {
		dev_err(dbd2->dev, "cannot start detection thread\n");
		goto err_gpio_free;
	}
#endif

/* wakeup */
#if (USE_SLEEPMODE_GPIO_FOR_WAKEUP || !USE_WAKEUP_GPIO_FOR_DETECTION)
	pdata->gpio_wakeup = of_get_named_gpio(np, "wakeup-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_wakeup)) {
		dev_err(dbd2->dev, "wakeup gpio invalid %d\n",
			pdata->gpio_wakeup);
		goto err_gpio_free2;
	}

	ret = gpio_request(pdata->gpio_wakeup, "DBD2 wakeup");
	if (ret < 0) {
		dev_err(dbd2->dev, "error requesting wakeup gpio\n");
		goto err_gpio_free2;
	}
	/* keep the wakeup pin low */
	gpio_direction_output(pdata->gpio_wakeup, 0);
	gpio_set_value(pdata->gpio_wakeup, 0);
#endif
	INIT_WORK(&dbd2->sv_work, dbmd2_sv_work);
	INIT_WORK(&dbd2->uevent_work, dbmd2_uevent_work);
	mutex_init(&dbd2->lock);

	ret = kfifo_alloc(&dbd2->pcm_kfifo, MAX_KFIFO_BUFFER_SIZE,
			  GFP_KERNEL);
	if (ret) {
		dev_err(dbd2->dev, "no kfifo memory\n");
		goto err_gpio_free3;
	}

	dbd2->audio_buffer_size = MAX_AUDIO_BUFFER_SIZE;
	dbd2->audio_mode = DBD2_AUDIO_MODE_PCM;
	dbd2->audio_processed = 1;

	dbd2->channels = 2;
	ret = of_property_read_u32(np, "channels", &dbd2->channels);
	if ((ret && ret != -EINVAL) ||
	    (dbd2->channels < 1 || dbd2->channels > 2)) {
		dev_err(dbd2->dev, "invalid 'channels'\n");
		goto err_kfifo_free;
	}

	dbd2->clock_config[0] = 0x0301; /* DSP clock 12MHz; UART 57600 */
	dbd2->clock_config[1] = 0x0000; /* DSP clock 49MHz; UART 1Mb */
#ifdef MSM_G1
	dbd2->clock_config[2] = 0x0000; /* DSP clock 148MHz; UART 3Mb */
#else
	dbd2->clock_config[2] = 0x0000; /* DSP clock 148MHz; UART 3Mb */
#endif
	ret = of_property_read_u16_array(np, "clock_config",
					 (u16 *)&(dbd2->clock_config), 3);
	if (ret && ret != -EINVAL) {
		dev_err(dbd2->dev, "invalid 'clock_config'\n");
		goto err_kfifo_free;
	}

	dbd2->uart_speed[0] = 57600;
	dbd2->uart_speed[1] = 1000000;
#ifdef MSM_G1
	dbd2->uart_speed[2] = 1000000;
#else
	dbd2->uart_speed[2] = 1000000;
#endif
	ret = of_property_read_u32_array(np, "uart_speed",
					 (u32 *)&(dbd2->uart_speed), 3);
	if (ret && ret != -EINVAL) {
		dev_err(dbd2->dev, "invalid 'uart_speed'\n");
		goto err_kfifo_free;
	}

	dbd2->mic_config[DBMD2_MIC_TYPE_DIGITAL_LEFT] = 0x003b;
	dbd2->mic_config[DBMD2_MIC_TYPE_DIGITAL_RIGHT] = 0x0002;
	dbd2->mic_config[DBMD2_MIC_TYPE_ANALOG] = 0x0005;

	ret = of_property_read_u16_array(np, "mic_config",
					 (u16 *)&(dbd2->mic_config), 3);
	if (ret && ret != -EINVAL) {
		dev_err(dbd2->dev, "invalid 'mic_config'\n");
		goto err_kfifo_free;
	}

#ifdef MSM_G1
	dbd2->init_mic_config =  DBMD2_MIC_TYPE_DIGITAL_LEFT;
#else
	dbd2->init_mic_config =  0;

#endif
	ret = of_property_read_u32(np, "mic_mode", &dbd2->init_mic_config);
	if ((ret && ret != -EINVAL) ||
	    (dbd2->init_mic_config > 5)) {
		dev_err(dbd2->dev, "invalid 'mic_mode'\n");
		goto err_kfifo_free;
	}
#if USE_WAKEUP_GPIO_FOR_DETECTION
	dbd2->gpio_reg_config =  0x53F;
#else
#ifdef MSM_G1
	dbd2->gpio_reg_config =  0x53A;
#else
	dbd2->gpio_reg_config =  0x537;
#endif
#endif
	ret = of_property_read_u32(np,
		"dbd2_gpio_cfg", &dbd2->gpio_reg_config);
	if (ret && ret != -EINVAL) {
		dev_err(dbd2->dev, "invalid 'gpio_reg_config'\n");
		goto err_kfifo_free;
	}

	ret = of_property_read_u32(np, "buffer_size", &dbd2->audio_buffer_size);
	if ((ret && ret != -EINVAL) ||
	    (dbd2->audio_buffer_size > MAX_AUDIO_BUFFER_SIZE)) {
		dev_err(dbd2->dev, "invalid 'buffer_size'\n");
		goto err_kfifo_free;
	}

	ret = of_property_read_u32(np, "audio_mode", &dbd2->audio_mode);
	if ((ret && ret != -EINVAL) ||
	    (dbd2->audio_mode != DBD2_AUDIO_MODE_PCM
	&& dbd2->audio_mode != DBD2_AUDIO_MODE_MU_LAW)) {
		dev_err(dbd2->dev, "invalid 'audio_mode'\n");
		goto err_kfifo_free;
	}

	ret = of_property_read_u32(np, "auto_buffering", &dbd2->auto_buffering);
	if ((ret && ret != -EINVAL) ||
	    (dbd2->auto_buffering != 0 && dbd2->auto_buffering != 1)) {
		dev_err(dbd2->dev, "invalid 'auto_buffering'\n");
		goto err_kfifo_free;
	}

	ret = of_property_read_u32(np, "auto_detection", &dbd2->auto_detection);
	if ((ret && ret != -EINVAL) ||
			(dbd2->auto_detection != 0 &&
				dbd2->auto_detection != 1)) {
		dev_err(dbd2->dev, "invalid 'auto_detection'\n");
		goto err_kfifo_free;
	}

	pdata->irq_inuse = 0;
#ifndef USE_WAKEUP_FROM_UART
	ret = request_threaded_irq(dbd2->pdata.sv_irq,
				  dbmd2_sv_interrupt_fn,
				  dbmd2_sv_interrupt,
				  IRQF_TRIGGER_RISING,
				  "dbmd2_sv", dbd2);
	if (ret < 0) {
		dev_err(dbd2->dev, "cannot get irq\n");
		goto err_kfifo_free;
	}

	ret = irq_set_irq_wake(dbd2->pdata.sv_irq, 1);
	if (ret < 0) {
			dev_err(dbd2->dev, "cannot set irq_set_irq_wake\n");
			goto err_free_irq;
	}
#endif
	ns_class = class_create(THIS_MODULE, "voice_trigger");
	if (IS_ERR(ns_class)) {
		dev_err(dbd2->dev, "failed to create class\n");
		goto err_free_irq;
	}

	dbd2_dev = device_create(ns_class, NULL, 0, dbd2, "dbd2");
	if (IS_ERR(dbd2_dev)) {
		dev_err(dbd2->dev, "could not create device\n");
		goto err_class_destroy;
	}

	gram_dev = device_create(ns_class, NULL, 0, dbd2, "gram");
	if (IS_ERR(gram_dev)) {
		dev_err(dbd2->dev, "could not create device\n");
		goto err_class_destroy;
	}

	net_dev = device_create(ns_class, NULL, 0, dbd2, "net");
	if (IS_ERR(net_dev)) {
		dev_err(dbd2->dev, "could not create device\n");
		goto err_class_destroy;
	}

	ret = sysfs_create_group(&dbd2_dev->kobj, &dbd2_attribute_group);
	if (ret) {
		dev_err(dbd2_dev, "failed to create sysfs group\n");
		goto err_device_unregister;
	}

	ret = alloc_chrdev_region(&dbd2->record_chrdev, 0, 1,
				  "dbd2");
	if (ret) {
		dev_err(dbd2_dev, "failed to allocate character device\n");
		goto err_sysfs_remove_group;
	}

	cdev_init(&dbd2->record_cdev, &record_fops);

	dbd2->record_cdev.owner = THIS_MODULE;

	ret = cdev_add(&dbd2->record_cdev, dbd2->record_chrdev, 1);
	if (ret) {
		dev_err(dbd2_dev, "failed to add character device\n");
		goto err_unregister_chrdev_region;
	}

	dbd2->record_dev = device_create(ns_class, &platform_bus,
					 MKDEV(MAJOR(dbd2->record_chrdev), 0),
					 dbd2, "dbd2%d", 0);
	if (IS_ERR(dbd2->record_dev)) {
		dev_err(dbd2->dev, "could not create device\n");
		goto err_device_unregister2;
	}

	/* register the codec */
	ret = snd_soc_register_codec(dbd2->dev,
				     &soc_codec_dev_dbmd2,
				     dbmd2_dais,
				     ARRAY_SIZE(dbmd2_dais));
	if (ret != 0) {
		dev_err(dbd2->dev,
			"Failed to register codec and its DAI: %d\n",  ret);
		goto err_device_unregister2;
	}

	dev_info(dbd2->dev, "registered DBMD2 codec driver\n");

	return 0;

err_device_unregister2:
	if (dbd2->record_dev && !IS_ERR(dbd2->record_dev))
		device_unregister(dbd2->record_dev);
	cdev_del(&dbd2->record_cdev);
err_unregister_chrdev_region:
	unregister_chrdev_region(dbd2->record_chrdev, 1);
err_sysfs_remove_group:
	sysfs_remove_group(&dbd2->dev->kobj, &dbd2_attribute_group);
err_device_unregister:
	device_unregister(dbd2_dev);
err_class_destroy:
	class_destroy(ns_class);
err_kfifo_free:
	kfifo_free(&dbd2->pcm_kfifo);
err_free_irq:
	dbd2->pdata.irq_inuse = 0;
#ifndef USE_WAKEUP_FROM_UART
	irq_set_irq_wake(dbd2->pdata.sv_irq, 0);
	free_irq(dbd2->pdata.sv_irq, dbd2);
#endif
err_gpio_free3:
#if (USE_SLEEPMODE_GPIO_FOR_WAKEUP || !USE_WAKEUP_GPIO_FOR_DETECTION)
	gpio_free(dbd2->pdata.gpio_wakeup);
#endif
err_gpio_free2:
#ifndef USE_WAKEUP_FROM_UART
	gpio_free(dbd2->pdata.gpio_sv);
#else
	kthread_stop(dbd2->detection_thread);
	dbd2->detection_thread = NULL;
#endif
err_gpio_free:
	gpio_free(dbd2->pdata.gpio_reset);
err_kfree3:
	kfree(dbd2->amodel_fw_name);
err_kfree2:
	vfree(dbd2->amodel_buf);
err_kfree:
	kfree(dbd2);
	dev_err(dbd2->dev, "registered DBMD2 codec driver failed!!!\n");

	return ret;
}

static int dbmd2_spi_probe(struct spi_device *spi)
{
	struct  dbd2_data *dbd2;
	struct  device_node *np;
	int     cs_gpio, ret;
	u32     spi_speed;

	np = spi->dev.of_node;
	if (np == NULL)
		dev_err(&spi->dev, "of node is null\n");

	cs_gpio = of_get_named_gpio(np, "cs-gpio", 0);
	if (cs_gpio < 0) {
		cs_gpio = -1;
		dev_info(&spi->dev, "of node is null\n");
	}
	if (!gpio_is_valid(cs_gpio)) {
		dev_err(&spi->dev, "sp cs gpio invalid\n");
		ret = -EINVAL;
	}

	ret = gpio_request(cs_gpio, "DBMD2 spi_cs");
	if (ret < 0)
		dev_err(&spi->dev, "error requesting reset gpio\n");

	gpio_direction_output(cs_gpio, 1);
	dev_info(&spi->dev, "spi cs configured\n");

	ret = of_property_read_u32(np, "spi-max-frequency", &spi_speed);
	if (ret && ret != -EINVAL)
		spi_speed = 2000000;

	dev_info(&spi->dev, "spi speed is %u\n", spi_speed);


	/*
	* setup spi parameters; this makes sure that parameters we request
	* are acceptable by the spi driver
	*/

	spi->mode = SPI_MODE_0; /* clk active low */
	spi->bits_per_word = 8;
	spi->max_speed_hz = spi_speed;

	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(&spi->dev, "spi_setup() failed\n");
		return -EIO;
	}

	dbd2 = kzalloc(sizeof(*dbd2), GFP_KERNEL);
	if (dbd2 == NULL)
		return -ENOMEM;

	dbd2->client_spi = spi;
	dbd2->dev = &spi->dev;

	dbd2->rxsize = TTY_MAX_HW_BUF_SIZE;
	dbd2->rsize = UART_TTY_READ_SZ;
	dbd2->wsize = UART_TTY_WRITE_SZ;

	return dbmd2_common_probe(dbd2);
}


static int dbmd2_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct dbd2_data *dbd2;

	dbd2 = kzalloc(sizeof(*dbd2), GFP_KERNEL);
	if (dbd2 == NULL) {
		dev_err(&client->dev, "out of memory\n");
		return -ENOMEM;
	}

	dbd2->client = client;
	dbd2->dev = &client->dev;

	dbd2->rxsize = TTY_MAX_HW_BUF_SIZE;
	dbd2->rsize = UART_TTY_READ_SZ;
	dbd2->wsize = UART_TTY_WRITE_SZ;

	return dbmd2_common_probe(dbd2);
}

static void dbmd2_common_remove(struct dbd2_data *dbd2)
{
	snd_soc_unregister_codec(dbd2->dev);
	device_unregister(dbd2->record_dev);
	cdev_del(&dbd2->record_cdev);
	unregister_chrdev_region(dbd2->record_chrdev, 1);
	sysfs_remove_group(&dbd2->dev->kobj, &dbd2_attribute_group);
	device_unregister(dbd2_dev);
	class_destroy(ns_class);
	dbd2->pdata.irq_inuse = 0;
#ifndef USE_WAKEUP_FROM_UART
	disable_irq(dbd2->pdata.sv_irq);
	irq_set_irq_wake(dbd2->pdata.sv_irq, 0);
	free_irq(dbd2->pdata.sv_irq, dbd2);
#endif
#if (USE_SLEEPMODE_GPIO_FOR_WAKEUP || !USE_WAKEUP_GPIO_FOR_DETECTION)
	gpio_free(dbd2->pdata.gpio_wakeup);
#endif
	gpio_free(dbd2->pdata.gpio_reset);
#ifndef USE_WAKEUP_FROM_UART
	gpio_free(dbd2->pdata.gpio_sv);
#else
	kthread_stop(dbd2->detection_thread);
	dbd2->detection_thread = NULL;
#endif
	kfifo_free(&dbd2->pcm_kfifo);
	kfree(dbd2->amodel_fw_name);
	vfree(dbd2->amodel_buf);
	kfree(dbd2);
}

static int dbmd2_spi_remove(struct spi_device *spi)
{
	struct dbd2_data *dbd2 = spi_get_drvdata(spi);

	spi_set_drvdata(spi, NULL);
	dbmd2_common_remove(dbd2);

	return 0;
}


static int dbmd2_i2c_remove(struct i2c_client *client)
{
	struct dbd2_data *dbd2 = i2c_get_clientdata(client);

	i2c_set_clientdata(client, NULL);
	dbmd2_common_remove(dbd2);

	return 0;
}

static int dbmd2_platform_probe(struct platform_device *pdev)
{
	struct device_node *np;
	struct dbd2_data *dbd2;
	int ret;

	dbd2 = kzalloc(sizeof(*dbd2), GFP_KERNEL);
	if (dbd2 == NULL) {
		dev_err(&pdev->dev, "out of memory\n");
		return -ENOMEM;
	}

	dbd2->rxsize = TTY_MAX_HW_BUF_SIZE;
	dbd2->rsize = UART_TTY_READ_SZ;
	dbd2->wsize = UART_TTY_WRITE_SZ;
	init_completion(&dbd2->uart_done);

	dbd2->dev = &pdev->dev;
	np = dbd2->dev->of_node;
	atomic_set(&dbd2->stop_uart_probing, 0);

	ret = of_property_read_string(np, "uart_device", &dbd2->uart_dev);
	if (ret && ret != -EINVAL) {
		dev_err(dbd2->dev, "invalid 'uart_device'\n");
		goto err_kfree;
	}

	ret = dbmd2_uart_open(dbd2);
	if (ret < 0) {
		dev_err(dbd2->dev, "UART initialization failed\n");
		goto err_kfree;
	}

	ret = dbmd2_common_probe(dbd2);
	if (ret < 0) {
		dev_err(dbd2->dev, "common probe failed\n");
		goto err_stop_uart_probe;
	}

	return 0;
err_stop_uart_probe:
	dev_dbg(dbd2->dev, "stopping UART probe thread\n");
	atomic_inc(&dbd2->stop_uart_probing);
	if (kthread_stop(dbd2->uart_probe_thread) == -EINTR)
		dev_err(dbd2->dev, "could not stop UART probe thread\n");
	dbd2->uart_probe_thread = NULL;
err_kfree:
	kfree(dbd2);

	return ret;
}

static int dbmd2_platform_remove(struct platform_device *pdev)
{
	struct dbd2_data *dbd2 = dev_get_drvdata(&pdev->dev);

	dbmd2_uart_close(dbd2);
	dbmd2_common_remove(dbd2);

	return 0;
}

static const struct of_device_id dbmd2_of_spi_match[] = {
	{ .compatible = "dspg,dbmd2-spi", },
	{}
};

MODULE_DEVICE_TABLE(of, dbmd2_of_spi_match);


static struct spi_driver dbmd2_spi_driver = {
	.driver = {
		.name	= "dbmd2-spi",
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
		.of_match_table = dbmd2_of_spi_match,
	},
	.probe  = dbmd2_spi_probe,
	.remove = dbmd2_spi_remove,
};

static const struct of_device_id dbmd2_i2c_of_match[] = {
	{ .compatible = "dspg,dbmd2-i2c", },
	{},
};
MODULE_DEVICE_TABLE(of, dbmd2_i2c_of_match);

static const struct i2c_device_id dbmd2_i2c_id[] = {
	{ "dbmd2", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, dbmd2_i2c_id);

static struct i2c_driver dbmd2_i2c_driver = {
	.driver = {
		.name = "dbmd2",
		.owner = THIS_MODULE,
		.of_match_table = dbmd2_i2c_of_match,
	},
	.probe =    dbmd2_i2c_probe,
	.remove =   dbmd2_i2c_remove,
	.id_table = dbmd2_i2c_id,
};

static struct of_device_id dbmd2_of_uart_match[] = {
	{ .compatible = "dspg,dbmd2-uart", 0 },
	{ }
};
MODULE_DEVICE_TABLE(of, dbmd2_of_uart_match);

static struct platform_driver dbmd2_platform_driver = {
	.driver = {
		.name = "dbmd2",
		.owner = THIS_MODULE,
		.of_match_table = dbmd2_of_uart_match,
	},
	.probe =    dbmd2_platform_probe,
	.remove =   dbmd2_platform_remove,
};

static int __init dbmd2_modinit(void)
{
	int ret;

	ret = spi_register_driver(&dbmd2_spi_driver);
	if (ret)
		return ret;

	ret = i2c_add_driver(&dbmd2_i2c_driver);
	if (ret)
		return ret;

	ret = platform_driver_register(&dbmd2_platform_driver);
	if (ret)
		i2c_del_driver(&dbmd2_i2c_driver);

	return ret;
}
module_init(dbmd2_modinit);

static void __exit dbmd2_exit(void)
{
	platform_driver_unregister(&dbmd2_platform_driver);
	i2c_del_driver(&dbmd2_i2c_driver);
	spi_unregister_driver(&dbmd2_spi_driver);
}
module_exit(dbmd2_exit);

MODULE_FIRMWARE(DBD2_FIRMWARE_NAME);
MODULE_VERSION(DRIVER_VERSION);
MODULE_DESCRIPTION("DSPG DBD2 Voice trigger codec driver");
MODULE_LICENSE("GPL");
