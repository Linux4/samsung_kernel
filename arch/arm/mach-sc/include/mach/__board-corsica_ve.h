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

#define HEADSET_DETECT_GPIO_ACTIVE_LOW    0
#define HEADSET_BUTTVOL_VOLUP_MIN         86
#define HEADSET_BUTTVOL_VOLUP_MAX         164
#define HEADSET_BUTTVOL_VOLDOWN_MIN       187
#define HEADSET_BUTTVOL_VOLDOWN_MAX       373
#define HEADSET_BUTTVOL_MEDIA_MIN         0
#define HEADSET_BUTTVOL_MEDIA_MAX         83
#define HEADSET_TYPEVOL_NOMIC_MIN         0
#define HEADSET_TYPEVOL_NOMIC_MAX         556
#define HEADSET_TYPEVOL_MIC_MIN           628
#define HEADSET_TYPEVOL_MIC_MAX           2453

#define EIC_KEY_POWER           (EIC_POWER_PBINT)
#define HEADSET_BUTTON_GPIO		(EIC_AUD_HEAD_BUTTON)
#define HEADSET_DETECT_GPIO		(EIC_AUD_HEAD_INST2)
#define HEADSET_SWITCH_GPIO	0

#define HEADSET_IRQ_TRIGGER_LEVEL_DETECT 1
#define HEADSET_IRQ_TRIGGER_LEVEL_BUTTON 1

#define HEADSET_ADC_MIN_KEY_MEDIA 0
#define HEADSET_ADC_MAX_KEY_MEDIA 170
#define HEADSET_ADC_MIN_KEY_VOLUMEUP 171
#define HEADSET_ADC_MAX_KEY_VOLUMEUP 430
#define HEADSET_ADC_MIN_KEY_VOLUMEDOWN 431
#define HEADSET_ADC_MAX_KEY_VOLUMEDOWN 760

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
