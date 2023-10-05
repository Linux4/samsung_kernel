/*
 * sound/soc/codec/s5m3700x.h
 *
 * ALSA SoC Audio Layer - Samsung Codec Driver
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _S5M3700X_H
#define _S5M3700X_H

#define S5M3700X_REGISTER_START                        0x0000
#define S5M3700X_REGISTER_END                  0x030B

#define S5M3700X_REGISTER_BLOCKS               4
#define S5M3700X_BLOCK_SIZE                            0x100
#define S5M3700X_DIGITAL_BLOCK                 0
#define S5M3700X_ANALOG_BLOCK                  1
#define S5M3700X_OTP_BLOCK                             2
#define S5M3700X_MV_BLOCK                              3


/* Device Status */
#define DEVICE_NONE					0x0000
#define DEVICE_AMIC1				0x0001
#define DEVICE_AMIC2				0x0002
#define DEVICE_AMIC3				0x0004
#define DEVICE_AMIC4				0x0008
#define DEVICE_AMIC_ON				(DEVICE_AMIC1|DEVICE_AMIC2|DEVICE_AMIC3|DEVICE_AMIC4)

#define DEVICE_DMIC1				0x0010
#define DEVICE_DMIC2				0x0020
#define DEVICE_DMIC_ON				(DEVICE_DMIC1|DEVICE_DMIC2)
#define DEVICE_ADC_ON				(DEVICE_AMIC_ON|DEVICE_DMIC_ON)

#define DEVICE_EP					0x0100
#define DEVICE_HP					0x0200
#define DEVICE_LINE					0x0400
#define DEVICE_DAC_ON				(DEVICE_EP|DEVICE_HP|DEVICE_LINE)

#define DEVICE_PLAYBACK_ON			(DEVICE_DAC_ON)
#define DEVICE_CAPTURE_ON			(DEVICE_AMIC_ON|DEVICE_DMIC_ON)

#define DEVICE_VTS_AMIC1			0x1000
#define DEVICE_VTS_AMIC2			0x2000
#define DEVICE_VTS_AMIC_ON			(DEVICE_VTS_AMIC1|DEVICE_VTS_AMIC2)

#define DEVICE_VTS_DMIC1			0x4000
#define DEVICE_VTS_DMIC2			0x8000
#define DEVICE_VTS_DMIC_ON			(DEVICE_VTS_DMIC1|DEVICE_VTS_DMIC2)

#define DEVICE_VTS_ON				(DEVICE_VTS_AMIC_ON|DEVICE_VTS_DMIC_ON)

/* HW Params */
#define S5M3700X_SAMPLE_RATE_48KHZ		48000
#define S5M3700X_SAMPLE_RATE_192KHZ		192000
#define S5M3700X_SAMPLE_RATE_384KHZ		384000

#define BIT_RATE_16						16
#define BIT_RATE_24						24
#define BIT_RATE_32						32

#define CHANNEL_2						2
#define CHANNEL_4						4

#define ADC_INP_SEL_ZERO				3
#define HP_TRIM_CNT						50

/* Impedance */
enum s5m3700x_rcv_imp {
	RCV_IMP_DEFAULT = 0,
	RCV_IMP_32 = 0,
	RCV_IMP_12 = 1,
	RCV_IMP_6 = 2,
};

enum s5m3700x_hp_imp {
	HP_IMP_DEFAULT = 0,
	HP_IMP_32 = HP_IMP_DEFAULT,
	HP_IMP_6 = 1,
};

/* VTS */
enum s5m3700x_vts_mic {
	VTS_UNDEFINED = 0,
	VTS_AMIC = 1,
	VTS_DMIC = 2,
};

enum s5m3700x_vts_type {
	UNDEFINED = 0,
	VTS_AMIC_OFF = 1,
	VTS_AMIC1_ON = 2,
	VTS_AMIC2_ON = 3,
	VTS_AMIC_DUAL_ON = 4,
	VTS_DMIC_OFF = 5,
	VTS_DMIC1_ON = 6,
	VTS_DMIC2_ON = 7,
};

/* Jack type */
enum {
	JACK = 0,
	USB_JACK,
};

struct hw_params {
	unsigned int aifrate;
	unsigned int width;
	unsigned int channels;
};

struct s5m3700x_priv {
	/* codec driver default */
	struct device *dev;
	struct i2c_client *i2c_priv;
	struct regmap *regmap;

	/* Alsa codec driver default */
	struct snd_soc_component *codec;

	/* codec mutex */
	struct mutex regcache_lock;
	struct mutex regsync_lock;
	struct mutex mute_lock;
	struct mutex hp_mbias_lock;

	/* codec workqueue */
	struct delayed_work adc_mute_work;
	struct workqueue_struct *adc_mute_wq;

	/* Power & regulator */
	int resetb_gpio;
	int external_ldo_3_4v;
	int external_ldo_1_8v;
	struct regulator *vdd;

	/* saved information */
	int codec_power_ref_cnt;
	int cache_bypass;
	int cache_only;
	bool power_always_on;
	unsigned int rcv_imp;
	unsigned int hp_imp;
	unsigned int hp_normal_trim[HP_TRIM_CNT];
	unsigned int hp_uhqa_trim[HP_TRIM_CNT];
	struct hw_params playback_params;
	struct hw_params capture_params;
	int dmic_delay;
	int amic_delay;
	int hp_bias_cnt;

	/* device_status */
	unsigned int status;
	unsigned int vts_mic;

	/* Jack */
	unsigned int jack_type;
	struct s5m3700x_jack *p_jackdet;

	/*debugfs*/
#ifdef CONFIG_DEBUG_FS
	struct dentry *dbg_root;
#endif
};

/* Forward Declarations */
/* Main Functions */
/* Regmap Function */
int s5m3700x_write(struct s5m3700x_priv *s5m3700x,
	unsigned int addr, unsigned int value);
int s5m3700x_read(struct s5m3700x_priv *s5m3700x,
	unsigned int addr, unsigned int *value);
int s5m3700x_update_bits(struct s5m3700x_priv *s5m3700x,
	unsigned int addr, unsigned int mask, unsigned int value);

int s5m3700x_read_only_cache(struct s5m3700x_priv *s5m3700x,
	unsigned int addr, unsigned int *value);
int s5m3700x_write_only_cache(struct s5m3700x_priv *s5m3700x,
	unsigned int addr, unsigned int value);
int s5m3700x_update_bits_only_cache(struct s5m3700x_priv *s5m3700x,
	 int addr, unsigned int mask, unsigned int value);

int s5m3700x_read_only_hardware(struct s5m3700x_priv *s5m3700x,
	unsigned int addr, unsigned int *value);
int s5m3700x_write_only_hardware(struct s5m3700x_priv *s5m3700x,
	unsigned int addr, unsigned int value);
int s5m3700x_update_bits_only_hardware(struct s5m3700x_priv *s5m3700x,
	unsigned int addr, unsigned int mask, unsigned int value);

void s5m3700x_regcache_cache_only_switch(struct s5m3700x_priv *s5m3700x, bool on);
void s5m3700x_regcache_cache_bypass_switch(struct s5m3700x_priv *s5m3700x, bool on);
int s5m3700x_regmap_register_sync(struct s5m3700x_priv *s5m3700x);
int s5m3700x_hardware_register_sync(struct s5m3700x_priv *s5m3700x, int start, int end);
int s5m3700x_cache_register_sync_default(struct s5m3700x_priv *s5m3700x);
int s5m3700x_regmap_reinit_cache(struct s5m3700x_priv *s5m3700x);
void s5m3700x_update_reg_defaults(const struct reg_sequence *patch, const int array_size);

void s5m3700x_usleep(unsigned int u_sec);
bool s5m3700x_check_device_status(struct s5m3700x_priv *s5m3700x, unsigned int status);
void s5m3700x_adc_digital_mute(struct s5m3700x_priv *s5m3700x, unsigned int channel, bool on);
#endif /* _S5M3700X_H */

