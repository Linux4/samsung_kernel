/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

/* file
 *
 * The tfa_device interface controls a single I2C device instance by
 * referencing to the device specific context provided by means of the
 * tfa_device structure pointer.
 * Multiple instances of tfa_device structures will be created and maintained
 * by the caller.
 *
 * The API is functionally grouped as:
 *  - tfa_dev basic codec interface to probe,
 *    start/stop and control the device state
 *  - access to internal MTP storage
 *  - abstraction for interrupt bits and handling
 *  - container reading support
 */
#ifndef __TFA_DEVICE_H__
#define __TFA_DEVICE_H__

#include "config.h"

struct tfa_device;

/*
 * hw/sw feature bit settings in MTP
 */
enum feature_support {
	SUPPORT_NOT_SET, /**< default means not set yet */
	SUPPORT_NO,      /**< no support */
	SUPPORT_YES      /**< supported */
};
/*
 * supported Digital Audio Interfaces bitmap
 */
enum tfa98xx_dai_bitmap {
	TFA98XX_DAI_I2S = 0x01, /**< I2S only */
	TFA98XX_DAI_TDM = 0x02, /**< TDM, I2S */
	TFA98XX_DAI_PDM = 0x04, /**< PDM */
};

/*
 * device ops function structure
 */
struct tfa_device_ops {
	int (*dsp_msg)(void *tfa, int length, const char *buf);
	int (*dsp_msg_read)(void *tfa, int length, unsigned char *bytes);
	enum tfa98xx_error (*reg_read)(struct tfa_device *tfa,
		unsigned char subaddress, unsigned short *value);
	enum tfa98xx_error (*reg_write)(struct tfa_device *tfa,
		unsigned char subaddress, unsigned short value);
	enum tfa98xx_error (*mem_read)(struct tfa_device *tfa,
		unsigned int start_offset, int num_words, int *p_values);
	enum tfa98xx_error (*mem_write)(struct tfa_device *tfa,
		unsigned short address, int value, int memtype);

	enum tfa98xx_error (*tfa_init)(struct tfa_device *tfa);
	enum tfa98xx_error (*dsp_reset)(struct tfa_device *tfa, int state);
	enum tfa98xx_error (*dsp_system_stable)(struct tfa_device *tfa,
		int *ready);
	enum tfa98xx_error (*dsp_write_tables)(struct tfa_device *tfa,
		int sample_rate);
	enum tfa98xx_error (*auto_copy_mtp_to_iic)(struct tfa_device *tfa);
	enum tfa98xx_error (*factory_trimmer)(struct tfa_device *tfa);
	int (*set_swprof)(struct tfa_device *tfa, unsigned short new_value);
	int (*get_swprof)(struct tfa_device *tfa);
	int (*set_swvstep)(struct tfa_device *tfa, unsigned short new_value);
	int (*get_swvstep)(struct tfa_device *tfa);
	int (*get_mtpb)(struct tfa_device *tfa);
	enum tfa98xx_error (*set_mute)(struct tfa_device *tfa, int mute);
	enum tfa98xx_error (*faim_protect)(struct tfa_device *tfa, int state);
	enum tfa98xx_error (*set_osc_powerdown)(struct tfa_device *tfa,
		int state);
	enum tfa98xx_error (*update_lpm)(struct tfa_device *tfa, int state);
	int (*set_bitfield)(struct tfa_device *tfa,
		uint16_t bitfield, uint16_t value);
	enum tfa98xx_error (*get_status)(struct tfa_device *tfa);
};

/*
 * Device states and modifier flags to allow a device/type independent fine
 * grained control of the internal state.\n
 * Values below 0x10 are referred to as base states which can be or-ed with
 * state modifiers, from 0x10 and higher.
 *
 */
enum tfa_state {
	TFA_STATE_UNKNOWN,
	TFA_STATE_POWERDOWN,
	TFA_STATE_INIT_HW,
	TFA_STATE_INIT_CF,
	TFA_STATE_INIT_FW,
	TFA_STATE_OPERATING,
	TFA_STATE_FAULT,
	TFA_STATE_RESET,
	/* --sticky state modifiers-- */
	TFA_STATE_MUTE = 0x10,
	TFA_STATE_UNMUTE = 0x20,
	TFA_STATE_CLOCK_ALWAYS = 0x40,
	TFA_STATE_CLOCK_AUDIO = 0x80,
	TFA_STATE_LOW_POWER = 0x100,
};

enum pool_control {
	POOL_NOT_SUPPORT,
	POOL_ALLOC,
	POOL_FREE,
	POOL_GET,
	POOL_RETURN,
	POOL_MAX_CONTROL
};

#define POOL_MAX_INDEX 6

struct tfa98xx_buffer_pool {
	int size;
	unsigned char in_use;
	void *pool;
};

/*
 * This is the main tfa device context structure, it will carry all information
 * that is needed to handle a single I2C device instance.
 * All functions dealing with the device will need access to the fields herein.
 */
struct tfa_device {
	int dev_idx;
	int in_use;
	int buffer_size;
	int has_msg;
	unsigned char resp_address;
	unsigned short rev;
	unsigned char tfa_family;
	enum feature_support support_drc;
	enum feature_support support_framework;
	enum feature_support support_saam;
	int sw_feature_bits[2];
	int hw_feature_bits;
	int profile;
	int next_profile;
	int vstep;
	unsigned char spkr_count;
	unsigned char spkr_select;
	unsigned char support_tcoef;
	enum tfa98xx_dai_bitmap daimap;
	int mohm[3];
	struct tfa_device_ops dev_ops;
	uint16_t interrupt_enable[3];
	uint16_t interrupt_status[3];
	int ext_dsp;
	int bus;
	int tfadsp_event;
	int verbose;
	enum tfa_state state;
	struct tfa_container *cnt;
	struct tfa_volume_step_register_info *p_reg_info;
	int partial_enable;
	void *data;
	int convert_dsp32;
	int sync_iv_delay;
	int is_probus_device;
	int advance_keys_handling;
	int needs_reset;
	struct kmem_cache *cachep;
	char fw_itf_ver[4];

	int dev_count;
	int dev_tfadsp;
	int set_active;
	int mute_state;
	int pause_state;
	int spkgain;
	int temp;
	int spkr_damaged; /* 0: okay, 1: damaged */
	int is_cold;
	int is_bypass;
	int is_calibrating;
	int is_configured;
	int mtpex;
	int reset_mtpex;
	int stream_state; /* b0: pstream (Rx), b1: cstream (Tx), b2:SaaM */
	int prev_samstream;
	int first_after_boot;
	int active_handle;
	int active_count;
	int swprof;
	int ampgain;
	int ramp_steps;
	int individual_msg;
	int set_device;
	int set_config;
	struct tfa98xx_buffer_pool buf_pool[POOL_MAX_INDEX];
	int lower_limit_cal;
	int upper_limit_cal;
	char fw_lib_ver[3];
	int irq_all;
	int irq_max;
	int disable_auto_cal;
	int inchannel;
	int ipcid[3];
};

#if defined(TFA_STEREO_NODE)
/* stereo */
/* ref. device order in container file */
/* confirmed by customer on 07/19/2022 v3 */
#define INDEX_0 0 /* dev 0 - left; top (receiver) */
#define INDEX_1 1 /* dev 1 - right; bottom (speaker) */
#else
/* mono */
#define INDEX_0 0 /* dev 0 - mono; bottom */
#define INDEX_1 0 /* dev 0 - mono; bottom */
#endif /* TFA_STEREO_NODE */

/*
 * The tfa_dev_probe is called before accessing any device accessing functions.
 * Access to the tfa device register 3 is attempted and will record the
 * returned id for further use. If no device responds the function will abort.
 * The recorded id will by used by the query functions to fill the remaining
 * relevant data fields of the device structure.
 * Data such as MTP features that requires device access will only be read when
 * explicitly called and the result will be then cached in the struct.
 *
 * A structure pointer passed to this device needs to refer to existing memory
 * space allocated by the caller.
 *
 *  @param resp_addr = I2C responder address of the target device (not shifted)
 *  @param tfa struct = points to memory that holds the context for this device
 *  instance
 *
 *  @return
 *   - 0 if the I2C device responded to a read of register address 3\n
 *       when device responds but with an unknown id a warning will be printed
 *   - -1 if no response from the I2C device
 *
 */
int tfa_dev_probe(int resp_addr, struct tfa_device *tfa);

/*
 * Start this instance at the profile and vstep as provided.
 * The profile and vstep will be loaded first in case the current value differs
 * from the requested values.
 * Note that this call will not change the mute state of the tfa, which means
 * that of this instance was called in muted state the caller will have to
 * unmute in order to get audio.
 *
 *  @param tfa struct = pointer to context of this device instance
 *  @param profile the selected profile to run
 *  @param vstep the selected vstep to use
 *  @return tfa_error enum
 */
enum tfa_error tfa_dev_start(struct tfa_device *tfa, int profile, int vstep);

enum tfa_error tfa_dev_switch_profile(struct tfa_device *tfa,
	int profile, int vstep);

/*
 * Stop audio for this instance as gracefully as possible.
 * Audio will be muted and the PLL will be shutdown together with any other
 * device/type specific settings needed to prevent audio artifacts or
 * workarounds.
 *
 * Note that this call will change state of the tfa to mute and powered down.
 *
 *  @param tfa struct = pointer to context of this device instance
 *  @return tfa_error enum
 */
enum tfa_error tfa_dev_stop(struct tfa_device *tfa);

/*
 * This interface allows a device/type independent fine grained control of
 * internal state of the instance.
 * Whenever a base state is requested an attempt is made to  bring device
 * into this state. However this may depend on external conditions beyond
 * this software layer. Therefore in case the state cannot be set an erro will
 * be returned and the current state remains unchanged.
 * The base states, lower values below 0x10, are all mutually exclusive,
 * higher ones can also function as a sticky modifier which means for example
 * that operating state could be in either muted or unmuted state.
 * Or in case of init_cf it can be
 * internal clock (always) or external audio clock.
 * This function is intended to be used for device mute/unmute synchronization
 * when called from higher layers. Mostly internal calls will use this
 * to control the startup and profile transitions in a independent way.
 *
 *  @param tfa struct = pointer to context of this device instance
 *  @param state struct = desired device state after function return
 *  @return tfa_error enum
 */
enum tfa_error tfa_dev_set_state(struct tfa_device *tfa,
	enum tfa_state state, int is_calibration);

/*
 * Retrieve the current state of this instance in an active way.
 * The state field in tfa structure will reflect the result unless an error is
 * returned.
 * Note that the hardware state may change on external events an as such this
 * field should be treated as volatile.
 *
 *  @param tfa struct = pointer to context of this device instance
 *  @return tfa_error enum
 *
 */
enum tfa_state tfa_dev_get_state(struct tfa_device *tfa);

/*****************************************************************************/
/*
 *  MTP support functions
 */
enum tfa_mtp {
	TFA_MTP_OTC,
	TFA_MTP_EX,
	TFA_MTP_RE25,
	TFA_MTP_RE25_PRIM,
	TFA_MTP_RE25_SEC,
	TFA_MTP_LOCK,
};

int tfa_dev_mtp_get(struct tfa_device *tfa, enum tfa_mtp item);
enum tfa_error tfa_dev_mtp_set(struct tfa_device *tfa,
	enum tfa_mtp item, int value);

/*
 * tfa2 interrupt support
 * interrupt bit function to clear
 */
int tfa_irq_clear(struct tfa_device *tfa, int bit);
/*
 * return state of irq or -1 if illegal bit
 */
int tfa_irq_get(struct tfa_device *tfa, int bit);
/*
 * interrupt bit function that operates on the shadow regs in the handle
 */
int tfa_irq_ena(struct tfa_device *tfa, int bit, int state);
/*
 * interrupt bit function that sets the polarity
 */
int tfa_irq_set_pol(struct tfa_device *tfa, int bit, int state);
/*
 * mask interrupts by disabling them
 */
int tfa_irq_mask(struct tfa_device *tfa);
/*
 * unmask interrupts by enabling them again
 */
int tfa_irq_unmask(struct tfa_device *tfa);
/*
 * initialize interrupt registers (for single IRQ register)
 */
void tfa_irq_init(struct tfa_device *tfa);
/*
 * report interrupt status (for single IRQ register)
 */
int tfa_irq_report(struct tfa_device *tfa);

enum tfa98xx_error tfa_get_fw_api_version(struct tfa_device *tfa,
	unsigned char *pfw_version);
enum tfa98xx_error tfa_get_fw_lib_version(struct tfa_device *tfa,
	unsigned char *plib_version);

#define RAMPDOWN_SHORT 2
#define RAMPDOWN_DEFAULT 5 /* 5 or higher if usleep_range works */
enum tfa98xx_error tfa_gain_rampdown(struct tfa_device *tfa, int count);
enum tfa98xx_error tfa_gain_restore(struct tfa_device *tfa, int count);

#endif /* __TFA_DEVICE_H__ */
