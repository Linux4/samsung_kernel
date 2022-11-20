/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#ifndef __GPIO_CORSICA_VE_H__
#define __GPIO_CORSICA_VE_H__

#ifndef __ASM_ARCH_BOARD_H
#error  "Don't include this file directly, include <mach/board.h>"
#endif

#define GPIO_TOUCH_RESET         53
#define GPIO_TOUCH_IRQ           52

#define GPIO_SENSOR_RESET        162
#define GPIO_MAIN_SENSOR_PWN     163
#define GPIO_SUB_SENSOR_PWN      164

#define SPRD_FLASH_OFST          0x890
#define SPRD_FLASH_CTRL_BIT      0x8000
#define SPRD_FLASH_LOW_VAL       0x3
#define SPRD_FLASH_HIGH_VAL      0xF
#define SPRD_FLASH_LOW_CUR       110
#define SPRD_FLASH_HIGH_CUR      470

#define USB_OTG_CABLE_DETECT     72

#define EIC_CHARGER_DETECT		(A_EIC_START + 0)
#define EIC_POWER_PBINT2        (A_EIC_START + 1)
#define EIC_POWER_PBINT         (A_EIC_START + 2)
#define EIC_AUD_HEAD_BUTTON     (A_EIC_START + 3)
#define EIC_CHG_CV_STATE       (A_EIC_START + 4)
#define EIC_AUD_HEAD_INST2      (A_EIC_START + 5)
#define EIC_VCHG_OVI            (A_EIC_START + 6)
#define EIC_VBAT_OVI            (A_EIC_START + 7)

#define EIC_KEY_POWER           (EIC_POWER_PBINT)
#define HEADSET_BUTTON_GPIO		(EIC_AUD_HEAD_BUTTON)
#define HEADSET_DETECT_GPIO		180
#define HEADSET_SWITCH_GPIO	0

#define HEADSET_IRQ_TRIGGER_LEVEL_DETECT 0
#define HEADSET_IRQ_TRIGGER_LEVEL_BUTTON 1

#define HEADSET_ADC_MIN_KEY_MEDIA 0
#define HEADSET_ADC_MAX_KEY_MEDIA 137
#define HEADSET_ADC_MIN_KEY_VOLUMEUP 157
#define HEADSET_ADC_MAX_KEY_VOLUMEUP 280
#define HEADSET_ADC_MIN_KEY_VOLUMEDOWN 282
#define HEADSET_ADC_MAX_KEY_VOLUMEDOWN 610
#define HEADSET_ADC_THRESHOLD_3POLE_DETECT 920
#define HEADSET_ADC_THRESHOLD_4POLE_DETECT 2700
#define HEADSET_IRQ_THRESHOLD_BUTTON 10
#define HEADSET_HEADMICBIAS_VOLTAGE 3000000

#define SPI0_CMMB_CS_GPIO        156
#define SPI1_WIFI_CS_GPIO        44

#define GPIO_BK                  136

#define GPIO_GPS_RESET 			 143
#define GPIO_GPS_ONOFF           144

#if 0
#define GPIO_CMMB_RESET         144
#define GPIO_CMMB_INT           143
#endif

#define GPIO_CMMB_26M_CLK_EN    197

#define GPIO_BT_RESET       189
#define GPIO_BT_POWER       110
#define GPIO_BT2AP_WAKE     192
#define GPIO_AP2BT_WAKE     188

#define BOARD_DEFAULT_REV 1
#define GPIO_HW_REV0 39
#define GPIO_HW_REV1 40
#define GPIO_HW_REV2 41
#define GPIO_HW_REV3 42

#define GPIO_WIFI_SHUTDOWN	230
#define GPIO_WIFI_IRQ		234

#define GPIO_PROX_INT 192
#define GPIO_GYRO_INT1 191
#define GPIO_M_RSTN	188
#define GPIO_M_DRDY 189

#define GPIO_SDIO_DETECT      75

/* ION config info
LCD:
   960x540
*/
#define SPRD_ION_OVERLAY_SIZE    (4 * SZ_1M)

#endif
