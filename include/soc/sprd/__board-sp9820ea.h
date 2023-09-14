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

#define GPIO_TOUCH_RESET         145
#define GPIO_TOUCH_IRQ           144

#define GPIO_MAIN_SENSOR_RESET   44
#define GPIO_MAIN_SENSOR_PWN     46
#define GPIO_SUB_SENSOR_RESET    45
#define GPIO_SUB_SENSOR_PWN      47

#define USB_OTG_CABLE_DETECT     126

#define SPRD_FLASH_OFST          0x890
#define SPRD_FLASH_CTRL_BIT      0x8000
#define SPRD_FLASH_LOW_VAL       0x3
#define SPRD_FLASH_HIGH_VAL      0xF
#define SPRD_FLASH_LOW_CUR       110
#define SPRD_FLASH_HIGH_CUR      470

#define GPIO_KEY_VOLUMEDOWN 124
#define GPIO_KEY_VOLUMEUP 125

#define EIC_CHARGER_DETECT		(A_EIC_START + 0)
#define EIC_POWER_PBINT2        (A_EIC_START + 1)
#define EIC_POWER_PBINT         (A_EIC_START + 2)
#define EIC_AUD_HEAD_BUTTON     (A_EIC_START + 3)
#define EIC_CHG_CV_STATE       (A_EIC_START + 4)
#define EIC_AUD_HEAD_INST      (A_EIC_START + 5)
#define EIC_VCHG_OVI            (A_EIC_START + 6)
#define EIC_VBAT_OVI            (A_EIC_START + 7)
#define EIC_AUD_HEAD_INST2      (A_EIC_START + 8)

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
#define HEADSET_ADC_THRESHOLD_3POLE_DETECT 100
#define HEADSET_ADC_THRESHOLD_4POLE_DETECT 3100
#define HEADSET_IRQ_THRESHOLD_BUTTON 1
#define HEADSET_HEADMICBIAS_VOLTAGE 3000000

#define SPI0_CMMB_CS_GPIO        32
#define SPI1_WIFI_CS_GPIO        44

#define GPIO_BK                  143
#define MSENSOR_DRDY_GPIO        53
#define GPIO_PLSENSOR_IRQ	213
/*For bcm4343 power on/off and sleep/wake */
#define GPIO_BT_RESET 122
#define GPIO_BT_POWER 131
#define GPIO_BT2AP_WAKE 133
#define GPIO_AP2BT_WAKE 132

/*#define GPIO_BT_RESET		194 */
#define GPIO_WIFI_POWERON	189
#define GPIO_WIFI_SHUTDOWN	130
#define GPIO_WIFI_IRQ		97

#define GPIO_PROX_INT       140
#define GPIO_GPS_ONOFF	        174

#define GPIO_SDIO_DETECT      75

#define SPRD_PIN_SDIO0_OFFSET     0x01E0
#define SPRD_PIN_SDIO0_SIZE       7
#define SPRD_PIN_SDIO0_D3_INDEX 0
#define SPRD_PIN_SDIO0_D3_GPIO 100
#define SPRD_PIN_SDIO0_SD_FUNC 0
#define SPRD_PIN_SDIO0_GPIO_FUNC 3

#define SIPC_SMEM_ADDR		(CONFIG_PHYS_OFFSET + 120 * SZ_1M)
#define CPT_START_ADDR		(CONFIG_PHYS_OFFSET + 128 * SZ_1M)
#define CPT_TOTAL_SIZE		(SZ_1M * 18)
#define CPT_RING_ADDR		(CPT_START_ADDR + CPT_TOTAL_SIZE - SZ_4K)
#define CPT_RING_SIZE		(SZ_4K)
#define CPT_SMEM_SIZE		(SZ_1M * 2)
#define CPW_START_ADDR		(CONFIG_PHYS_OFFSET + 150 * SZ_1M)
#define CPW_TOTAL_SIZE		(SZ_1M * 33)
#define CPW_RING_ADDR		(CPW_START_ADDR + CPW_TOTAL_SIZE - SZ_4K)
#define CPW_RING_SIZE		(SZ_4K)
#define CPW_SMEM_SIZE		(SZ_1M * 2)
#define WCN_START_ADDR		(CONFIG_PHYS_OFFSET + 320 * SZ_1M)
#define WCN_TOTAL_SIZE		(SZ_1M * 5)
#define WCN_RING_ADDR		(WCN_START_ADDR + WCN_TOTAL_SIZE - SZ_4K)
#define WCN_RING_SIZE		(SZ_4K)
#define WCN_SMEM_SIZE		(SZ_1M * 2)
#define GGE_START_ADDR		(CONFIG_PHYS_OFFSET + 128 * SZ_1M)
#define GGE_TOTAL_SIZE		(SZ_1M * 22)
#define GGE_RING_ADDR		(GGE_START_ADDR + GGE_TOTAL_SIZE - SZ_4K)
#define GGE_RING_SIZE		(SZ_4K)
#define GGE_SMEM_SIZE		(SZ_1M * 2)
#define LTE_START_ADDR		(CONFIG_PHYS_OFFSET + 150 * SZ_1M)
#define LTE_TOTAL_SIZE		(SZ_1M * 64)
#define LTE_RING_ADDR		(LTE_START_ADDR + LTE_TOTAL_SIZE - SZ_4K)
#define LTE_RING_SIZE		(SZ_4K)
#define LTE_SMEM_SIZE		(SZ_1M * 2)
#endif
