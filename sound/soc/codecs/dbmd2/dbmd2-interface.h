/*
 * dbmd2-interface.h  --  DBMD2 interface definitions
 *
 * Copyright (C) 2014 DSP Group
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _DBMD2_INTERFACE_H
#define _DBMD2_INTERFACE_H

#include <linux/device.h>
#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/kfifo.h>
#include <linux/dbmd2.h>
#include <sound/dbmd2-export.h>

#define MAX_REQ_SIZE				512
#define MAX_WRITE_SZ				4096
#define MAX_READ_SZ				512

struct chip_interface;

enum dbmd2_firmware_active {
	/* firmware pre-boot */
	DBMD2_FW_PRE_BOOT = 0,
	/* voice authentication */
	DBMD2_FW_VA,
	/* voice quality enhancement */
	DBMD2_FW_VQE,
};

enum dbmd2_clocks {
	DBMD2_CLK_CONSTANT = 0,
	DBMD2_CLK_MASTER,
};

struct va_flags {
	int	irq_inuse;
	int	a_model_loaded;
	int	amodel_len;
	int	buffering;
	unsigned int	mode;
};

struct vqe_flags {
	int	in_call;
	int	use_case;
	int	speaker_volume_level;
};

enum xtal_id {
	DBMD2_XTAL_FREQ_24M_IMG1 = 0,
	DBMD2_XTAL_FREQ_24M_IMG2,
	DBMD2_XTAL_FREQ_24M_IMG3,
	DBMD2_XTAL_FREQ_9M_IMG4,
	DBMD2_XTAL_FREQ_24M_IMG5,
	DBMD2_XTAL_FREQ_19M_IMG6,
	DBMD2_XTAL_FREQ_32K_IMG7,
};


enum dbmd2_power_modes {
		/*
		 * no firmware is active and the device is booting
		 */
		DBMD2_PM_BOOTING = 0,
		/*
		 * a firmware is active and the device can be used
		 */
		DBMD2_PM_ACTIVE,
		/*
		 * a firmware is active and the device is going to sleep
		 */
		DBMD2_PM_FALLING_ASLEEP,
		/*
		 * chip is sleeping and needs to be woken up to be functional
		 */
		DBMD2_PM_SLEEPING,
		/* number of PM states */
		DBMD2_PM_STATES,
};

struct dbmd2_private {
	struct dbd2_platform_data		*pdata;
	/* lock for private data */
	struct mutex			p_lock;
	enum dbmd2_firmware_active	active_fw;
	enum dbmd2_power_modes		power_mode;
	int				sv_irq;
	struct platform_device		*pdev;
	struct device			*dev;
	const struct firmware		*va_fw;
	const struct firmware		*vqe_fw;
	const struct firmware		*vqe_non_overlay_fw;
	struct firmware			*dspg_gram;
	struct firmware			*dspg_net;
	bool				asleep;
	bool				device_ready;
	struct clk			*constant_clk;
	struct clk			*master_clk;
	struct work_struct		sv_work;
	struct work_struct		uevent_work;
	unsigned int			audio_mode;
	unsigned int			clk_type;
	unsigned int			bytes_per_sample;
	dev_t				record_chrdev;
	struct cdev			record_cdev;
	struct device			*record_dev;
	struct cdev			record_cdev_block;
	struct device			*record_dev_block;
	struct kfifo			pcm_kfifo;
	atomic_t			audio_owner;
	char				*amodel_buf;
	u8				*sbl_data;
	struct va_flags			va_flags;
	struct vqe_flags		vqe_flags;
	u32				vqe_vc_syscfg;
	u32				va_current_mic_config;
	u32				va_detection_mode;
	u32				va_backlog_length;
	bool				va_capture_on_detect;

	struct delayed_work		delayed_pm_work;
	struct workqueue_struct		*dbmd2_workq;

	/* limit request size of audio data from the firmware */
	unsigned long				rxsize;
	/* limit the maxmimal size to read from bus at once */
	unsigned long				rsize;
	/* limit the maximal size to write to the bus at once */
	unsigned long				wsize;

	/* sysfs */
	struct class				*ns_class;
	struct device				*dbmd2_dev;
	struct device				*gram_dev;
	struct device				*net_dev;
	/* common helper functions */
	void (*reset_set)(struct dbmd2_private *p);
	void (*reset_release)(struct dbmd2_private *p);
	void (*reset_sequence)(struct dbmd2_private *p);
	void (*wakeup_set)(struct dbmd2_private *p);
	void (*wakeup_release)(struct dbmd2_private *p);
	void (*lock)(struct dbmd2_private *p);
	void (*unlock)(struct dbmd2_private *p);
	int (*verify_checksum)(struct dbmd2_private *p,
			       const u8 *expect, const u8 *got, size_t size);
	int (*va_set_speed)(struct dbmd2_private *p,
			    enum dbmd2_va_speeds speed);
	unsigned long (*clk_get_rate)(struct dbmd2_private *p,
				      enum dbmd2_clocks clk);
	int (*clk_enable)(struct dbmd2_private *p, enum dbmd2_clocks clk);
	int (*clk_disable)(struct dbmd2_private *p, enum dbmd2_clocks clk);

	/* external callbacks */
	set_i2c_freq_cb	set_i2c_freq_callback;
	event_cb		event_callback;

	/* interface to the chip */
	struct chip_interface			*chip;
};

/*
 * main interface between the core layer and the chip
 */
struct chip_interface {
	/* wait till booting is allowed */
	int (*can_boot)(struct dbmd2_private *p);
	/* prepare booting (e.g. increase speed) */
	int (*prepare_boot)(struct dbmd2_private *p);
	/* send firmware to the chip and boot it */
	int (*boot)(const struct firmware *fw, struct dbmd2_private *p,
		    const void *checksum, size_t chksum_len, int load_fw);
	/* finish booting */
	int (*finish_boot)(struct dbmd2_private *p);
	/* dump chip state */
	int (*dump)(struct dbmd2_private *p, char *buf);
	/* set VA firmware ready, (e.g. lower speed) */
	int (*set_va_firmware_ready)(struct dbmd2_private *p);
	/* set VQE firmware ready */
	int (*set_vqe_firmware_ready)(struct dbmd2_private *p);
	/* Enable/Disable Transport layer (UART/I2C/SPI) */
	void (*transport_enable)(struct dbmd2_private *p, bool enable);
	/* read data from the chip */
	ssize_t (*read)(struct dbmd2_private *p, void *buf, size_t len);
	/* write data to the chip */
	ssize_t (*write)(struct dbmd2_private *p, const void *buf, size_t len);
	/* send command in VQE protocol format to the chip */
	ssize_t (*send_cmd_vqe)(struct dbmd2_private *p,
			    u32 command, u16 *response);
	/* send command in VA protocol format to the chip */
	ssize_t (*send_cmd_va)(struct dbmd2_private *p,
				  u32 command, u16 *response);
	/* prepare buffering of audio data (e.g. increase speed) */
	int (*prepare_buffering)(struct dbmd2_private *p);
	/* read audio data */
	int (*read_audio_data)(struct dbmd2_private *p, size_t samples);
	/* finish buffering of audio data (e.g. lower speed) */
	int (*finish_buffering)(struct dbmd2_private *p);
	/* prepare amodel loading (e.g. increase speed) */
	int (*prepare_amodel_loading)(struct dbmd2_private *p);
	/* load acoustic model */
	int (*load_amodel)(struct dbmd2_private *p,  const void *data,
		size_t size, const void *checksum, size_t chksum_len, int load);
	/* finish amodel loading (e.g. lower speed) */
	int (*finish_amodel_loading)(struct dbmd2_private *p);
	/* private data */
	void *pdata;
};

/*
 * character device
 */
int dbmd2_register_cdev(struct dbmd2_private *p);
void dbmd2_deregister_cdev(struct dbmd2_private *p);

#endif
