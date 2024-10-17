/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef TFA98XX_INTERNALS_H
#define TFA98XX_INTERNALS_H

#include "config.h"
#include "tfa_service.h"  /* TODO cleanup for enum tfa98xx_status_id */

/*
 * tfadsp_fw_api.c
 */
/*
 * Return a text version of the firmware status ID code
 * @param status the given status ID code
 * @return the firmware status ID string
 */
const char *tfadsp_fw_status_string(enum tfa98xx_status_id status);
int tfadsp_fw_start(struct tfa_device *tfa, int prof_idx, int vstep_idx);
int tfadsp_fw_get_api_version(struct tfa_device *tfa, uint8_t *buffer);
#define FW_MAXTAG 150
int tfadsp_fw_get_tag(struct tfa_device *tfa, uint8_t *buffer);
int tfadsp_fw_get_status_change(struct tfa_device *tfa, uint8_t *buffer);
int tfadsp_fw_set_re25(struct tfa_device *tfa, int prim, int sec);
int tfadsp_fw_get_re25(struct tfa_device *tfa, uint8_t *buffer);

/*
 * the order matches the ACK bits order in TFA98XX_CF_STATUS
 */
enum tfa_fw_event { /* not all available on each device */
	TFA_FW_I2C_CMD_ACK,
	TFA_FW_RESET_START,
	TFA_FW_SHORT_ON_MIPS,
	TFA_FW_SOFT_MUTE_READY,
	TFA_FW_VOLUME_READY,
	TFA_FW_ERROR_DAMAGE,
	TFA_FW_CALIBRATE_DONE,
	TFA_FW_MAX
};

#define TFA_API_SBFW_BIG_M_88	2
#define TFA_API_SBFW_8_09_00_BIG_M		8
#define TFA_API_SBFW_PO_BIG_M	10
#define TFA_API_SBFW_8_09_00_SMALL_M	31
#define TFA_API_SBFW_9_00_00_SMALL_M	33
#define TFA_API_SBFW_10_00_00_SMALL_M	34

/* the following type mappings are compiler specific */
#define subaddress_t unsigned char

/* module Ids */
#define MODULE_FRAMEWORK		0
#define MODULE_SPEAKERBOOST		1
#define MODULE_BIQUADFILTERBANK	2
#define MODULE_CUSTOM			15

/* FW command ID */
/* SET */
#define FW_PAR_ID_SET_VBAT_FACTORS      0x02
#define FW_PAR_ID_SET_SENSES_DELAY      0x04
#define FW_PAR_ID_SET_SENSES_CAL        0x05
#define FW_PAR_ID_SET_INPUT_SELECTOR    0x06
#define FW_PAR_ID_SET_OUTPUT_SELECTOR   0x08
#define FW_PAR_ID_SET_PROGRAM_CONFIG    0x09
#define FW_PAR_ID_SET_GAINS             0x0A
#define FW_PAR_ID_SET_MEMTRACK          0x0B
#define FW_PAR_ID_SET_AMPLIFIER_EQ      0x0C
#define FW_PAR_ID_SET_HW_CONFIG         0x13
#define FW_PAR_ID_SET_CHIP_TEMP_SELECTOR 0x14
#define FW_PAR_ID_SET_MULTI_MESSAGE     0x15

/* GET */
#define FW_PAR_ID_GET_MEMTRACK          0x8B
#define FW_PAR_ID_GET_STATUS_CHANGE     0x8D
#define FW_PAR_ID_GET_LIBRARY_VERSION   0xFD
#define FW_PAR_ID_GET_API_VERSION       0xFE

/* SB command ID */
/* SET */
#define SB_PARAM_SET_ALGO_PARAMS        0x00
#define SB_PARAM_SET_MUTE               0x03
#define SB_PARAM_SET_VOLUME             0x04
#define SB_PARAM_SET_RE25C              0x05
#define SB_PARAM_SET_LSMODEL            0x06
#define SB_PARAM_SET_PARM_STEREO_BOOST  0x0A
#define SB_PARAM_SET_DATA_LOGGER        0x0D
#define SB_PARAM_SET_POWER_SAVER        0x0E

/* GET */
#define SB_PARAM_GET_ALGO_PARAMS        0x80
#define SB_PARAM_GET_MUTE               0x83
#define SB_PARAM_GET_VOLUME             0x84
#define SB_PARAM_GET_RE25C              0x85
#define SB_PARAM_GET_LSMODEL            0x86
#define SB_PARAM_GET_MBDRC              0x87
#define SB_PARAM_GET_MBDRC_DYNAMICS     0x89
#define SB_PARAM_GET_PARM_STEREO_BOOST  0x8A
#define SB_PARAM_GET_DATA_LOGGER        0x8D
#define SB_PARAM_GET_TSPKR              0xA8

/* Custom commands: aligned with wrapper */
/* SET */
#define CUSTOM_PARAM_SET_TSURF          0x01
#define CUSTOM_PARAM_SET_BYPASS         0x02

/* GET */
#define CUSTOM_PARAM_GET_CONFIGURED     0x81

#define BFB_PAR_ID_SET_COEFS            0x00
#define BFB_PAR_ID_GET_COEFS            0x80
#define BFB_PAR_ID_GET_CONFIG           0x81

#define TFA2_FW_ReZ_SCALE	65536
#define TFA1_FW_ReZ_SCALE	16384
#define TFA_FW_ReZ_SCALE	TFA_FAM_FW(tfa, ReZ_SCALE)

#define TFA2_FW_ReZ_SHIFT	16
#define TFA1_FW_ReZ_SHIFT	14
#define TFA_FW_ReZ_SHIFT	TFA_FAM_FW(tfa, ReZ_SHIFT)

#define TFA_ReZ_FP_INT(value, shift) ((value) >> (shift))
#define TFA_ReZ_FP_FRAC(value, shift) \
((((value) & ((1 << (shift)) - 1)) * 1000) >> (shift))
#define TFA_ReZ_CALC(value, shift) (((value) << (shift)) / 1000)

#define TFA2_FW_X_DATA_SCALE	262144 /* 2097152 */
#define TFA2_FW_X_DATA_UM_SCALE	262 /* 2097 */
#define TFA2_FW_T_DATA_SCALE	16384
#define TFA2_FW_T_DATA_MAX	512 /* 10-bit signed integer */
#endif /* TFA98XX_INTERNALS_H */
