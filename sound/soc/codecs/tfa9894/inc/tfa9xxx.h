/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __TFA9XXX_INC__
#define __TFA9XXX_INC__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/leds.h>
#include <sound/pcm.h>
#include <linux/regulator/consumer.h>

#include "tfa2_dev.h"
#include "tfa2_container.h"
#include "tfa2_haptic.h"
#include "tfa2_dsp_fw.h"
#include "tfa_haptic_fw_defs.h"

#define TFA9XXX_MAX_REGISTER              0xff

#define TFA9XXX_FLAG_SKIP_INTERRUPTS	(1 << 0)
#define TFA9XXX_FLAG_REMOVE_PLOP_NOISE	(1 << 1)
#define TFA9XXX_FLAG_LP_MODES			(1 << 2)

#if defined(TFA_NO_SND_FORMAT_CHECK)
#define TFA9XXX_NUM_RATES		14
#else
#define TFA9XXX_NUM_RATES		9
#endif

#define FIXED_MCLK_RATE 19200000 /* 19.2 MHz */
#define MAX_LEN_VIB_TYPE 32

#define TFA9XXX_INDEX_PBQ		0x0000FFFE /*trigger queue cmd*/

#define TFA9XXX_PBQ_SEG_LEN_MAX		10
#define TFA9XXX_PBQ_DEPTH_MAX		256
#define TFA9XXX_PBQ_SCALE_MAX		100
#define TFA9XXX_PBQ_DELAY_MAX		10000
#define TFA9XXX_PBQ_REPEAT_MAX		32

#define TFA9XXX_PBQ_TAG_START		0x8000
#define TFA9XXX_PBQ_TAG_STOP		0x8001
#define TFA9XXX_PBQ_TAG_SILENCE		0x8002

#define DEBUG_PRINT_PLAYBACK_QUEUE 40

enum pbq_hap_state {
	TFA_PBQ_STATE_IDLE,
	TFA_PBQ_STATE_TIMEOUT
};

/* DSP init status */
enum tfa9xxx_dsp_init_state {
	TFA9XXX_DSP_INIT_STOPPED,	/* DSP not running */
	TFA9XXX_DSP_INIT_RECOVER,	/* DSP error detected at runtime */
	TFA9XXX_DSP_INIT_FAIL,		/* DSP init failed */
	TFA9XXX_DSP_INIT_PENDING,	/* DSP start requested */
	TFA9XXX_DSP_INIT_DONE,		/* DSP running */
	TFA9XXX_DSP_INIT_INVALIDATED,	/* DSP was running, requires re-init */
};

enum tfa9xxx_dsp_fw_state {
	TFA9XXX_DSP_FW_NONE = 0,
	TFA9XXX_DSP_FW_PENDING,
	TFA9XXX_DSP_FW_FAIL,
	TFA9XXX_DSP_FW_OK,
};

struct tfa9xxx_firmware {
	void *base;
	struct tfa9xxx_device *dev;
	char name[9]; /* TODO get length from tfa parameter defs */
};

struct tfa9xxx_baseprofile {
	char basename[MAX_CONTROL_NAME];    /* profile basename */
	int len;                            /* profile length */
	int item_id;                        /* profile id */
	int sr_rate_sup[TFA9XXX_NUM_RATES]; /* sample rates for profile */
	struct list_head list;              /* list of all profiles */
};

enum tfa_haptic_state {
	STATE_STOP = 0,
	STATE_START,
	STATE_RESTART
};

struct tfa9xxx_pbq_pair {
	unsigned int tag;
	unsigned int mag;
	unsigned int repeat;
	unsigned int remain;
};

struct drv_object {
	enum tfa_haptic_object_type type;
	struct work_struct update_work;
	struct hrtimer active_timer;
	enum tfa_haptic_state state;
	int index;
};

struct tfa9xxx {
	struct regmap *regmap;
	struct i2c_client *i2c;
	struct clk *mclk;
#if defined(TFA_FORCED_POR)
	struct regulator *vddd;
#endif
	struct snd_soc_codec *codec;
	struct workqueue_struct *tfa9xxx_wq;
	struct delayed_work init_work;
	struct delayed_work monitor_work;
#if defined(TFA_ENABLE_INTERRUPT)
	struct delayed_work interrupt_work;
#endif
	struct mutex dsp_lock;
	int dsp_init;
	int dsp_fw_state;
	int sysclk;
	u16 rev;
	struct tfa9xxx_firmware fw;
	char *fw_name;
	int rate;
	wait_queue_head_t wq;
	struct device *dev;
	int pstream;
	int cstream;
	int trg_pstream;
	int trg_cstream;
	bool enable_monitor;

/* index queue */
	unsigned int pbq_depth;
	unsigned int pbq_index;
	unsigned int pbq_state;
	int pbq_repeat;
	int pbq_remain;

	struct tfa9xxx_pbq_pair pbq_pairs[TFA9XXX_PBQ_DEPTH_MAX];
	struct hrtimer pbq_timer;
	struct work_struct vibe_pbq_work;
	wait_queue_head_t waitq_pbq;

	unsigned int rate_constraint_list[TFA9XXX_NUM_RATES];
	struct snd_pcm_hw_constraint_list rate_constraint;

	int reset_gpio;
	int power_gpio;
	int irq_gpio;
#if defined(TFA_USE_GPIO_FOR_MCLK)
	int mclk_gpio;
#endif

	struct list_head list;
	struct tfa2_device *tfa;
	int vstep;
	int profile;
#ifdef VOLUME_FIXED
	/* store vstep per profile (single device) */
	int prof_vsteps[TFACONT_MAXPROFS];
#endif

#ifdef CONFIG_DEBUG_FS
	struct dentry *dbg_dir;
#endif
	char *rpc_buffer;
	int rpc_buffer_size; /* amount of valid data in rpc_buffer */
	u8 reg;
	unsigned int flags;

	bool haptic_mode;

	/* Haptic */
	struct led_classdev led_dev;
	struct workqueue_struct *tfa9xxx_hap_wq;
	struct class *to_class;
	struct device *to_dev;
	struct delayed_work pll_off_work;
	struct drv_object tone_object;
#ifdef PARALLEL_OBJECTS
	struct drv_object wave_object;
#endif
	int timeout; /* max value */
	int intensity;
	u32 tfa2_fw_intensity_max;
	char vib_type[MAX_LEN_VIB_TYPE];
	unsigned int cp_trigger_index;
	bool is_index_triggered;
	bool update_object_index;
	int object_index;
	bool update_object_intensity;
	int object_intensity;
	int object_duration;
	int running_index;
	int clk_users;
	int pattern_duration;
	int f0_trc_users;
	unsigned int  pll_off_delay;
	bool patch_loaded;
	bool bck_running;
};

/* tfa9xxx_haptic.c */
int __init tfa9xxx_haptic_init(void);
void __exit tfa9xxx_haptic_exit(void);
int tfa9xxx_haptic_probe(struct tfa9xxx *drv);
int tfa9xxx_haptic_remove(struct tfa9xxx *drv);
int tfa9xxx_bck_starts(struct tfa9xxx *drv);
int tfa9xxx_bck_stops(struct tfa9xxx *drv);
int tfa9xxx_disable_f0_tracking(struct tfa9xxx *drv, bool disable);
int tfa9xxx_haptic_dump_objs(struct seq_file *f, void *p);

/* mclk control */
int tfa9xxx_mclk_open(struct tfa9xxx *drv);
int tfa9xxx_mclk_enable(struct tfa9xxx *drv);
int tfa9xxx_mclk_disable(struct tfa9xxx *drv);
int tfa9xxx_mclk_close(struct tfa9xxx *drv);

/* calibration */
enum tfa_cal_type {
	RDC_CAL = 0,
	F0_CAL
};

int tfa2_run_calibrate(enum tfa_cal_type type, uint16_t *val);
int tfa2_read_calibrate(enum tfa_cal_type type, uint16_t *val);
int tfa2_read_cal_temp(uint16_t *val);

#endif /* __TFA9XXX_INC__ */

