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

#ifndef __GPIO_SC8810_H__
#define __GPIO_SC8810_H__

#ifndef __ASM_ARCH_BOARD_H
#error  "Don't include this file directly, include <mach/board.h>"
#endif

#define GPIO_TOUCH_RESET         142
#define GPIO_TOUCH_IRQ           141

#define HEADSET_SWITCH_GPIO	237
#define GPIO_SENSOR_RESET        41
#define GPIO_MAIN_SENSOR_PWN     42
#define GPIO_SUB_SENSOR_PWN      43

#define SPRD_FLASH_OFST          0x890
#define SPRD_FLASH_CTRL_BIT      0x8000
#define SPRD_FLASH_LOW_VAL       0x3
#define SPRD_FLASH_HIGH_VAL      0xF
#define SPRD_FLASH_LOW_CUR       110
#define SPRD_FLASH_HIGH_CUR      470

#define EIC_CHARGER_DETECT		(A_EIC_START + 2)
#define EIC_KEY_POWER           (A_EIC_START + 3)
#define HEADSET_BUTTON_GPIO		(A_EIC_START + 4)
#define HEADSET_DETECT_GPIO		139

#define HEADSET_IRQ_TRIGGER_LEVEL_DETECT 1
#define HEADSET_IRQ_TRIGGER_LEVEL_BUTTON 1

#define HEADSET_ADC_MIN_KEY_MEDIA 0
#define HEADSET_ADC_MAX_KEY_MEDIA 170
#define HEADSET_ADC_MIN_KEY_VOLUMEUP 171
#define HEADSET_ADC_MAX_KEY_VOLUMEUP 430
#define HEADSET_ADC_MIN_KEY_VOLUMEDOWN 431
#define HEADSET_ADC_MAX_KEY_VOLUMEDOWN 760
#define HEADSET_ADC_THRESHOLD_3POLE_DETECT 100
#define HEADSET_ADC_THRESHOLD_4POLE_DETECT 3100
#define HEADSET_IRQ_THRESHOLD_BUTTON 1
#define HEADSET_HEADMICBIAS_VOLTAGE 3000000

#define SPI0_CMMB_CS_GPIO        32
#define SPI1_WIFI_CS_GPIO        44

#define GPIO_BK                  143
#define MSENSOR_DRDY_GPIO        53
#define GPIO_PLSENSOR_IRQ	213

#define GPIO_BT_RESET		194
#define GPIO_WIFI_POWERON	189
#define GPIO_WIFI_RESET		190
#define GPIO_WIFI_IRQ	    -1

#define GPIO_GPS_ONOFF	        174

#define GPIO_SDIO_DETECT      75

#define SPRD_PIN_SDIO0_OFFSET     0x01E0
#define SPRD_PIN_SDIO0_SIZE       7
#define SPRD_PIN_SDIO0_D3_INDEX 0
#define SPRD_PIN_SDIO0_D3_GPIO 100
#define SPRD_PIN_SDIO0_SD_FUNC 0
#define SPRD_PIN_SDIO0_GPIO_FUNC 3

#endif
