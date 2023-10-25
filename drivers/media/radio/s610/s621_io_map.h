/*
 * drivers/media/radio/s610/s621_io_map.h
 *
 * control register define header for SAMSUNG S621 chip
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */

#ifndef S621_IO_MAP_H
#define S621_IO_MAP_H

/*------------------------------------------------------------------------------*/
/* block: fm_top */
/*------------------------------------------------------------------------------*/

/* Declarations of read/write registers */
#define FM_INT_MASK                                   (0x10000000) /* RW  10 bits */
#define FM_INT_CLEAR                                  (0x10000004) /* RW  10 bits */

#define FM_ENABLE                                     (0x10000010) /* RW   9 bits */

/* Declarations of read-only registers */
#define FM_INT_CAUSE                                  (0x10000008) /* R   10 bits */

/*------------------------------------------------------------------------------*/
/* block: fm_agc */
/*------------------------------------------------------------------------------*/

/* Declarations of read/write registers */
#define FM_AGC_CONFIG                                 (0x10001004) /* RW  15 bits */

#define FM_AGC_GAIN_FORCE                             (0x10001010) /* RW  10 bits */
#define FM_AGC_THRESH_RSSI                            (0x10001014) /* RW  16 bits */
#define FM_AGC_THRESH_ADC                             (0x10001018) /* RW  16 bits */

/* Declarations of read-only registers */
#define FM_AGC_GAIN_DEBUG                             (0x10001000) /* R   16 bits */

/*------------------------------------------------------------------------------*/
/* block: fm_demod */
/*------------------------------------------------------------------------------*/

/* Declarations of read/write registers */

#define FM_DEMOD_AUDIO_PAUSE_THRESHOLD                (0x10002008) /* RW   8 bits */
#define FM_DEMOD_AUDIO_PAUSE_DURATION                 (0x1000200C) /* RW   6 bits */

#define FM_DEMOD_IF                                   (0x10002014) /* RW  12 bits */
#define FM_DEMOD_IF_THRESHOLD                         (0x10002018) /* RW   8 bits */
#define FM_DEMOD_RSSI_THRESHOLD                       (0x1000201C) /* RW   9 bits */
#define FM_DEMOD_RDS_BYTE_COUNT_FOR_INT               (0x10002020) /* RW   8 bits */
#define FM_DEMOD_NARROW_THRESHOLDS                    (0x10002024) /* RW   8 bits */

#define FM_DEMOD_SOFT_MUTE_COEFFS                     (0x1000202C) /* RW  16 bits */
#define FM_DEMOD_SOFT_MUFFLE_COEFFS                   (0x10002030) /* RW  16 bits */
#define FM_DEMOD_STEREO_BLEND_COEFFS                  (0x10002034) /* RW  16 bits */

#define FM_DEMOD_SNR_ADJUST                           (0x1000203C) /* RW   9 bits */
#define FM_DEMOD_RSSI_ADJUST                          (0x10002040) /* RW   9 bits */

#define FM_DEMOD_IF_COUNTER_INT_TIME                  (0x10002044) /* RW   6 bits */
#define FM_DEMOD_STEREO_THRESHOLD                     (0x10002048) /* RW   8 bits */

#define FM_DEMOD_CONFIG                               (0x10002050) /* RW  20 bits */
#define FM_DEMOD_CONFIG2                              (0x10002054) /* RW  13 bits */
#define FM_DEMOD_NEW_TRJ_REF_ANGLE_FIRST              (0x10002058) /* RW  30 bits */
#define FM_DEMOD_NEW_TRJ_REF_ANGLE_MIDDLE             (0x1000205C) /* RW  30 bits */
#define FM_DEMOD_NEW_TRJ_REF_ANGLE_LAST               (0x10002060) /* RW  30 bits */

#define FM_DEMOD_FILTER_SELECT                        (0x10002064) /* RW  13 bits */
#define FM_DEMOD_TONEREJ_FREQ                         (0x10002068) /* RW  16 bits */
#define FM_DEMOD_TONEREJ_THRESH_ADAPT                 (0x1000206C) /* RW   8 bits */

#define FM_DEMOD_IMAGE_TRIM_PHASE                     (0x10002078) /* RW  11 bits */
#define FM_DEMOD_IMAGE_TRIM_AMPLI                     (0x1000207C) /* RW  17 bits */

#define FM_DEMOD_CLOCK_RATE_CONFIG                    (0x100020E0) /* RW   9 bits */
#define FM_DEMOD_SNR_SMOOTH_CONFIG                    (0x100020E4) /* RW  12 bits */
#define FM_DEMOD_IMAGE_TRIM_SMOOTH_CONFIG             (0x100020E8) /* RW  12 bits */
#define FM_RDS_DEC_CONFIG                             (0x100020EC) /* RW  25 bits */

#define FM_VOLUME_CTRL                                (0x100020F4) /* RW  12 bits */

/* Declarations of read-only registers */

#define FM_DEMOD_IF_COUNTER_ABS                       (0x100020B0) /* R   16 bits */
#define FM_DEMOD_IF_COUNTER                           (0x100020B4) /* R   16 bits */
#define FM_DEMOD_DIG_RSSI                             (0x100020B8) /* R   16 bits */
#define FM_DEMOD_SIGNAL_QUALITY                       (0x100020BC) /* R    9 bits */

#define FM_DEMOD_STATUS                               (0x100020C4) /* R    8 bits */

/*------------------------------------------------------------------------------*/
/* block: fm_ana */
/*------------------------------------------------------------------------------*/

/* Declarations of read/write registers */
#define AUX_CON                                       (0x10003000) /* RW  32 bits */
#define BPLL_CON1                                     (0x10003004) /* RW  32 bits */
#define BPLL_CON2                                     (0x10003008) /* RW  32 bits */
#define BPLL_CON3                                     (0x1000300C) /* RW  32 bits */
#define BPLL_CON4                                     (0x10003010) /* RW  32 bits */
#define BPLL_CON5                                     (0x10003014) /* RW  32 bits */
#define BPLL_CON6                                     (0x10003018) /* R   32 bits */
#define BPLL_CON7                                     (0x1000301C) /* RW  32 bits */
#define BPLL_CON8                                     (0x10003020) /* RW  32 bits */
#define BPLL_CON9                                     (0x10003024) /* RW  32 bits */

#define PLL_CON1                                      (0x10003028) /* RW  32 bits */
#define PLL_CON2                                      (0x1000302C) /* RW  32 bits */
#define PLL_CON3                                      (0x10003030) /* RW  32 bits */
#define PLL_CON4                                      (0x10003034) /* RW  32 bits */
#define PLL_CON5                                      (0x10003038) /* RW  32 bits */
#define PLL_CON6                                      (0x1000303C) /* RW  32 bits */
#define PLL_CON7                                      (0x10003040) /* RW  32 bits */
#define PLL_CON8                                      (0x10003044) /* RW  32 bits */
#define PLL_CON9                                      (0x10003048) /* RW  32 bits */
#define PLL_CON10                                     (0x1000304C) /* RW  32 bits */
#define PLL_CON11                                     (0x10003050) /* R   32 bits */
#define PLL_CON12                                     (0x10003054) /* RW  32 bits */
#define PLL_CON13                                     (0x10003058) /* RW  32 bits */
#define PLL_CON14                                     (0x1000305C) /* RW  32 bits */
#define PLL_CON15                                     (0x10003060) /* RW  32 bits */
#define PLL_CON16                                     (0x10003064) /* RW  32 bits */
#define PLL_CON17                                     (0x10003068) /* RW  32 bits */
#define PLL_CON18                                     (0x1000306C) /* RW  32 bits */
#define PLL_CON19                                     (0x10003070) /* RW  32 bits */
#define PLL_CON20                                     (0x10003074) /* RW  32 bits */

#define ADC_CON0                                      (0x10003078) /* R   32 bits */
#define ADC_CON1                                      (0x1000307C) /* RW  32 bits */
#define ADC_CON2                                      (0x10003080) /* RW  32 bits */

#define RX_RF_CON0                                    (0x10003084) /* R   32 bits */
#define RX_RF_CON1                                    (0x10003088) /* RW  32 bits */
#define RX_RF_CON2                                    (0x1000308C) /* RW  32 bits */
#define RX_RF_CON3                                    (0x10003090) /* RW  32 bits */
#define RX_RF_CON4                                    (0x10003094) /* RW  32 bits */

/*------------------------------------------------------------------------------*/
/* block: fm_spdy */
/*------------------------------------------------------------------------------*/

/* Declarations of read/write registers */
#define FM_SPEEDY_SLEEP_EN                            (0x10004004) /* RW   1 bits */

#define FM_SPEEDY_PAD_CTRL                            (0x10004014) /* RW   9 bits */

/* Declarations of read-only registers */
#define FM_SPEEDY_STAT                                (0x10004018) /* R   26 bits */
#define FM_VERSION                                    (0x1000401C) /* R   24 bits */

/*------------------------------------------------------------------------------*/
/* block: fm_rds */
/*------------------------------------------------------------------------------*/

/* Declarations of read-only registers */
#define FM_RDS_DATA                                   (0x10006000) /* R   32 bits */

/*------------------------------------------------------------------------------*/
/* block: fm_scg */
/*------------------------------------------------------------------------------*/

/* Declarations of read/write registers */
#define CLK_SEL                                       (0x10007000) /* RW   3 bits */
#define PLL_CLK_EN                                    (0x10007004) /* RW   2 bits */
#define FORCE_REG_CLK                                 (0x10007008) /* RW   2 bits */
#define CLK_ENABLE                                    (0x1000700C) /* RW   8 bits */
#define WARMUP_CTRL                                   (0x10007010) /* RW  32 bits */
#define FM_RESET_ASSERT                               (0x10007014) /* RW   1 bits */
#define FM_TO_BTWL                                    (0x10007018) /* RW  16 bits */

/* Declarations of read-only registers */
#define BTWL_TO_FM                                    (0x1000701C) /* R   16 bits */

#define FM_SPEEDY_ADDR_MIN                            (0x10000000)
#define FM_SPEEDY_ADDR_MAX                            (0x80000000)

#endif /* S621_IO_MAP_H */
