/*
 * drivers/media/radio/s610/s61x_io_map.h
 *
 * control register define header for SAMSUNG S61X chip
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
#ifndef S61X_IO_MAP_H
#define S61X_IO_MAP_H

#define FM_INT_CAUSE                                  (0xFFF301)
#define FM_DBG_SEL                                    (0xFFF300)
#define FM_INT_CLEAR                                  (0xFFF302)
#define FM_INT_MASK                                   (0xFFF303)
#define FM_ENABLE                                     (0xFFF304)
#define FM_AGC_ADC_RESET_CONFIG                       (0xFFF272)
#define FM_AGC_CONFIG                                 (0xFFF280)
#define FM_AGC_GAIN_DEBUG                             (0xFFF285)
#define FM_AGC_GAIN_FORCE                             (0xFFF286)
#define FM_AGC_THRESH_ADC                             (0xFFF299)
#define FM_AGC_THRESH_RSSI                            (0xFFF29A)
#define FM_DEMOD_AUDIO_PAUSE_DURATION                 (0xFFF2A2)
#define FM_DEMOD_AUDIO_PAUSE_THRESHOLD                (0xFFF2A4)
#define FM_DEMOD_CLOCK_RATE_CONFIG                    (0xFFF2A8)
#define FM_DEMOD_CONFIG                               (0xFFF2A9)
#define FM_DEMOD_CONFIG2                              (0xFFF2AA)
#define FM_DEMOD_DIG_RSSI                             (0xFFF2AD)
#define FM_DEMOD_FILTER_SELECT                        (0xFFF2AE)
#define FM_DEMOD_IF                                   (0xFFF2AF)
#define FM_DEMOD_IF_COUNTER                           (0xFFF2B0)
#define FM_DEMOD_IF_COUNTER_ABS                       (0xFFF2B1)
#define FM_DEMOD_IF_COUNTER_INT_TIME                  (0xFFF2B2)
#define FM_DEMOD_IF_THRESHOLD                         (0xFFF2B3)
#define FM_DEMOD_IMAGE_TRIM_AMPLI                     (0xFFF2B4)
#define FM_DEMOD_IMAGE_TRIM_PHASE                     (0xFFF2B5)
#define FM_DEMOD_IMAGE_TRIM_SMOOTH_CONFIG             (0xFFF2B6)
#define FM_DEMOD_NARROW_THRESHOLDS                    (0xFFF2B9)
#define FM_DEMOD_RDS_BYTE_COUNT_FOR_INT               (0xFFF2BF)
#define FM_DEMOD_RSSI_ADJUST                          (0xFFF2C2)
#define FM_DEMOD_RSSI_THRESHOLD                       (0xFFF2C4)
#define FM_DEMOD_SIGNAL_QUALITY                       (0xFFF2C5)
#define FM_DEMOD_SNR_ADJUST                           (0xFFF2C6)
#define FM_DEMOD_SNR_SMOOTH_CONFIG                    (0xFFF2C8)
#define FM_DEMOD_SOFT_MUFFLE_COEFFS                   (0xFFF2C9)
#define FM_DEMOD_SOFT_MUTE_COEFFS                     (0xFFF2CA)
#define FM_DEMOD_STATUS                               (0xFFF2CB)
#define FM_DEMOD_STEREO_BLEND_COEFFS                  (0xFFF2CC)
#define FM_DEMOD_STEREO_BLEND_THRESH                  (0xFFF2CD)
#define FM_DEMOD_STEREO_THRESHOLD                     (0xFFF2CE)
#define FM_DEMOD_TONEREJ_FREQ                         (0xFFF2D2)
#define FM_DEMOD_TONEREJ_THRESH_ADAPT                 (0xFFF2D3)
#define FM_RDS_DEC_CONFIG                             (0xFFF2D7)
#define PLL_CON1                                      (0xFFF240)
#define PLL_CON2                                      (0xFFF241)
#define PLL_CON3                                      (0xFFF242)
#define PLL_CON4                                      (0xFFF243)
#define PLL_CON5                                      (0xFFF244)
#define PLL_CON6                                      (0xFFF245)
#define PLL_CON7                                      (0xFFF246)
#define PLL_CON8                                      (0xFFF247)
#define PLL_CON9                                      (0xFFF248)
#define PLL_CON10                                     (0xFFF249)
#define PLL_CON11                                     (0xFFF24A)
#define PLL_CON12                                     (0xFFF24B)
#define PLL_CON13                                     (0xFFF24C)
#define PLL_CON14                                     (0xFFF24D)
#define PLL_CON15                                     (0xFFF24E)
#define PLL_CON16                                     (0xFFF24F)
#define PLL_CON17                                     (0xFFF250)
#define PLL_CON18                                     (0xFFF251)
#define PLL_CON19                                     (0xFFF252)
#define PLL_CON20                                     (0xFFF253)
#define PLL_CON21                                     (0xFFF254)
#define AUX_CON0                                      (0xFFF255)
#define AUX_CON1                                      (0xFFF256)
#define AUX_CON2                                      (0xFFF257)
#define AUX_CON3                                      (0xFFF258)
#define AUX_CON4                                      (0xFFF259)
#define AUX_CON5                                      (0xFFF25A)
#define AUX_CON6                                      (0xFFF25B)
#define ADC_CON0                                      (0xFFF25F)
#define ADC_CON1                                      (0xFFF260)
#define ADC_CON2                                      (0xFFF261)
#define RX_RF_CON0                                    (0xFFF262)
#define RX_RF_CON1                                    (0xFFF263)
#define RX_RF_CON2                                    (0xFFF264)
#define RX_RF_CON3                                    (0xFFF265)
#define FM_SPEEDY_PAD_CTRL                            (0xFFF390)
#define FM_SPEEDY_FIFO_STAT                           (0xFFF398)
#define FM_DEMOD_VERSION                              (0xFFF399)
#define FM_TOP_VERSION                                (0xFFF39E)
#define FM_AUDIO_DATA                                 (0xFFF3A0)
#define FM_RDS_DATA                                   (0xFFF3C0)
#define FM_ISO_EN                                     (0xFFF210)
#define FM_PWR_OFF_FIRST                              (0xFFF211)
#define FM_PWR_OFF_LAST                               (0xFFF212)
#define CLK_SEL                                       (0xFFF220)
#define PLL_CLK_EN                                    (0xFFF221)
#define DIV_VAL                                       (0xFFF222)
#define FM_RESET_ASSERT                               (0xFFF226)
#define FM_RESET_DEASSERT                             (0xFFF227)
#define FM_NEW_TRJ_REF_ANGLE_FIRST                    (0xFFF2D9)
#define FM_NEW_TRJ_REF_ANGLE_LAST                     (0xFFF2DA)

#define FM_SPEEDY_ADDR_MIN                            (0xFFF210)
#define FM_SPEEDY_ADDR_MAX                            (0xFFF3DF)
#endif /* S61X_IO_MAP_H */
