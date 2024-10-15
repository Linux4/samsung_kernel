/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef TFA2_GENREGS_H
#define TFA2_GENREGS_H

#define TFA98XX_SYS_CONTROL0              0x00
#define TFA98XX_SYS_CONTROL1              0x01
#define TFA98XX_SYS_CONTROL2              0x02
#define TFA98XX_DEVICE_REVISION           0x03
#define TFA98XX_CLOCK_CONTROL             0x04
#define TFA98XX_CLOCK_GATING_CONTROL      0x05
#define TFA98XX_DEVICE_REVISION1          0x06
#define TFA98XX_SIDE_TONE_CONFIG          0x0d
#define TFA98XX_CTRL_DIGTOANA_REG         0x0e
#define TFA98XX_STATUS_FLAGS0             0x10
#define TFA98XX_STATUS_FLAGS1             0x11
#define TFA98XX_STATUS_FLAGS2             0x12
#define TFA98XX_STATUS_FLAGS3             0x13
#define TFA98XX_STATUS_FLAGS4             0x14
#define TFA98XX_BATTERY_VOLTAGE           0x15
#define TFA98XX_TEMPERATURE               0x16
#define TFA98XX_TDM_CONFIG0               0x20
#define TFA98XX_TDM_CONFIG1               0x21
#define TFA98XX_TDM_CONFIG2               0x22
#define TFA98XX_TDM_CONFIG3               0x23
#define TFA98XX_TDM_CONFIG4               0x24
#define TFA98XX_TDM_CONFIG5               0x25
#define TFA98XX_TDM_CONFIG6               0x26
#define TFA98XX_TDM_CONFIG7               0x27
#define TFA98XX_TDM_CONFIG8               0x28
#define TFA98XX_TDM_CONFIG9               0x29
#define TFA98XX_PDM_CONFIG0               0x31
#define TFA98XX_PDM_CONFIG1               0x32
#define TFA98XX_HAPTIC_DRIVER_CONFIG      0x33
#define TFA98XX_GPIO_DATAIN_REG           0x34
#define TFA98XX_GPIO_CONFIG               0x35
#define TFA98XX_INTERRUPT_OUT_REG1        0x40
#define TFA98XX_INTERRUPT_OUT_REG2        0x41
#define TFA98XX_INTERRUPT_OUT_REG3        0x42
#define TFA98XX_INTERRUPT_IN_REG1         0x44
#define TFA98XX_INTERRUPT_IN_REG2         0x45
#define TFA98XX_INTERRUPT_IN_REG3         0x46
#define TFA98XX_INTERRUPT_ENABLE_REG1     0x48
#define TFA98XX_INTERRUPT_ENABLE_REG2     0x49
#define TFA98XX_INTERRUPT_ENABLE_REG3     0x4a
#define TFA98XX_STATUS_POLARITY_REG1      0x4c
#define TFA98XX_STATUS_POLARITY_REG2      0x4d
#define TFA98XX_STATUS_POLARITY_REG3      0x4e
#define TFA98XX_BAT_PROT_CONFIG           0x50
#define TFA98XX_AUDIO_CONTROL             0x51
#define TFA98XX_AMPLIFIER_CONFIG          0x52
#define TFA98XX_AUDIO_CONTROL2            0x5a
#define TFA98XX_CTRL_SAAM_PGA             0x60
#define TFA98XX_DCDC_CONTROL0             0x70
#define TFA98XX_CF_CONTROLS               0x90
#define TFA98XX_CF_MAD                    0x91
#define TFA98XX_CF_MEM                    0x92
#define TFA98XX_CF_STATUS                 0x93
#define TFA98XX_MTPKEY2_REG               0xa1
#define TFA98XX_MTP_STATUS                0xa2
#define TFA98XX_KEY_PROTECTED_MTP_CONTROL 0xa3
#define TFA98XX_MTP_DATA_OUT_MSB          0xa5
#define TFA98XX_MTP_DATA_OUT_LSB          0xa6
#define TFA98XX_TEMP_SENSOR_CONFIG        0xb1
#define TFA98XX_KEY2_PROTECTED_MTP0       0xf0
#define TFA98XX_KEY1_PROTECTED_MTP4       0xf4
#define TFA98XX_KEY1_PROTECTED_MTP5       0xf5

/*
 * (0xf0)-KEY2_protected_MTP0
 */

/*
 * calibration_onetime
 */
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC                (0x1<<0)
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_POS                   0
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_LEN                   1
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_MAX                   1
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_MSK                 0x1

/*
 * calibr_ron_done
 */
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPEX                 (0x1<<1)
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_POS                    1
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_LEN                    1
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MAX                    1
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MSK                  0x2
#endif /* TFA98XX_GENREGS_H */
