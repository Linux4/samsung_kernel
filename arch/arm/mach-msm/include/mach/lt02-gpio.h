/*
 * lt02-gpio.h
 *
 * header file supporting gpio functions for Samsung device
 *
 * COPYRIGHT(C) Samsung Electronics Co., Ltd. 2006-2011 All Right Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/* MSM8930 GPIO */
#define	GPIO_CAM_IO_EN			0
#define	GPIO_MSM_FLASH_NOW		3
#define	GPIO_SUB_CAM_MCLK		4
#define	GPIO_MAIN_CAM_MCLK		5
#define GPIO_CAM_CORE_EN		6
#define	GPIO_VT_STBY				18
#define	GPIO_CAM_A_EN				47
#define	GPIO_MSM_FLASH_CNTL_EN	64
#define	GPIO_CAM_AF_EN			66
#define	GPIO_CAM2_RST_N			76
#define	GPIO_CAM1_RST_N			107
#define GPIO_I2C_DATA_CAM			20
#define GPIO_I2C_CLK_CAM			21
#define GPIO_I2C_DATA_AF			51
#define GPIO_I2C_CLK_AF				0

#if defined(CONFIG_ADC_STMPE811)
#define GPIO_ADC_SDA		71
#define GPIO_ADC_SCL		72
#endif

#define GPIO_UART_RXD			23
#define GPIO_HOME_KEY		35
#define GPIO_VOLUME_UP		50
#define GPIO_TKEY_LED			51
#define GPIO_MAIN_STBY			2
#define GPIO_VIB_EN			63
#define GPIO_FLASH_LED_UNLOCK   64

#define GPIO_TOUCH_IRQ		11
#define GPIO_TOUCH_SDA		16
#define GPIO_TOUCH_SCL		17
#define GPIO_TKEY_INT		1
#define GPIO_TOUCHKEY_I2C_SDA		8
#define GPIO_TOUCHKEY_I2C_SCL		9

#define GPIO_SD_CARD_DET_N	65
#define GPIO_VIB_PWM			70
#define GPIO_S_LED_I2C_SDA		71
#define GPIO_S_LED_I2C_SCL		72
#define GPIO_USB_I2C_SDA		73 
#define GPIO_USB_I2C_SCL        	74
#define GPIO_TSP_D_EN			79
#define GPIO_TSP_A_EN			80
#define GPIO_VOLUME_DOWN	81
#define GPIO_TKEY_LDO_EN		99

/* AUDIO */
#define GPIO_CODEC_INT			10
#define GPIO_CODEC_RESET		42
#define GPIO_CODEC_SDA			36
#define GPIO_CODEC_SCK			37

#define GPIO_SPKR_I2S_TX_SCK		55
#define GPIO_SPKR_I2S_TX_WS		56
#define GPIO_SPKR_I2S_TX_DIN		57
#define GPIO_AUDIO_MCLK_REV10			59
#define GPIO_AUDIO_MCLK			53
#define GPIO_SPKR_I2S_RX_SCK		60
#define GPIO_SPKR_I2S_RX_DOUT		61
#define GPIO_SPKR_I2S_RX_WS		62

#define GPIO_VPS_AMP_EN		63
#define GPIO_SPK_AMP_EN		PM8038_GPIO_PM_TO_SYS(14)
#define GPIO_EAR_MIC_BIAS_EN		151

#define GPIO_SHORT_SENDEND		94

#if defined(CONFIG_SAMSUNG_JACK_GNDLDET)
#define GPIO_EAR_DET			43
#define GPIO_EAR_GND			46
#else
#define GPIO_EAR_DET			46
#define GPIO_EAR_GND			43
#endif

#if defined(CONFIG_2MIC_ES305)
#if defined(CONFIG_2MIC_QUP_I2C_GSBI11)
#define GPIO_2MIC_I2C_SDA		40
#define GPIO_2MIC_I2C_SCL		41
#else
#define GPIO_2MIC_I2C_SDA		24
#define GPIO_2MIC_I2C_SCL		25
#endif
#define GPIO_2MIC_PW_DN			52
#endif
#define GPIO_TOUCHKEY_I2C_SDA		8
#define GPIO_TOUCHKEY_I2C_SCL		9

/* BATTERY */
/*#define GPIO_BATT_INT                   7*/
#define GPIO_TA_DET 43
#define GPIO_FUELGAUGE_I2C_SDA  101
#define GPIO_FUELGAUGE_I2C_SCL  100
#define GPIO_FUEL_INT               98
#if defined(CONFIG_MACH_LT02_ATT) || defined(CONFIG_MACH_LT02_SPR) || defined(CONFIG_MACH_LT02_TMO)
#define GPIO_VBATT_IF		67
#endif
/*#define GPIO_CHARGER_CHG_EN             82*/
/*#define GPIO_CHARGER_CHG_nSTAT  150*/

/* FPGA */
#define GPIO_IRDA_SDA			33
#define GPIO_IRDA_SCL			34
#define GPIO_FPGA_RST_N			1	
#define GPIO_IRDA_IRQ			77
#define GPIO_IRDA_EN			0
#define GPIO_FPGA_CDONE 		80
#define GPIO_CRESET_B			82

/* SENSORS */
#define GPIO_GRIP_SDA			95
#define GPIO_GRIP_SCL			96
#define GPIO_GRIP_INT			92

#define GPIO_SENSOR_ALS_SDA		12
#define GPIO_SENSOR_ALS_SCL		13
#define GPIO_ACC_INT_N			67
#define GPIO_GYRO_INT			67
#define GPIO_LEDA_EN			89
#define GPIO_PROX_INT			49
/* MHL/HDMI */
#define GPIO_MHL_RST			18
#define GPIO_MHL_EN				18
#define GPIO_MHL_SDA			18
#define GPIO_MHL_SCL			18
#define GPIO_MHL_WAKE_UP		-1

#define LVDS_I2C_CLK	41
#define LVDS_I2C_SDA	40
#define LCD_BLIC_ON		48
#define LCD_BL_PWM		19

#define GPIO_MHL_INT			78 /* 55 for Rev00 */
#define GPIO_MHL_SEL			-1

#define GPIO_SENSOR_GEO_SDA		26
#define GPIO_SENSOR_GEO_SCL		27

/* PMIC8038 GPIO */
/* AUDIO */
#define PMIC_GPIO_2MIC_RST		11
/*I2C BUS ID*/
#define I2C_LEDS_BUS_ID			21
#define GPIO_MAIN_MIC_BIAS_EN   66 
/* OTG */
#define GPIO_OTG_TEST		150
#define GPIO_OVP_CTRL		PM8038_GPIO_PM_TO_SYS(15)




//#define GPIO_NFC_IRQ                    106
//#define GPIO_NFC_EN                     144
//#define GPIO_NFC_FIRMWARE                       92
//#define GPIO_NFC_CLK_REQ                90
//#define GPIO_NFC_SDA                    95
//#define GPIO_NFC_SCL                    96
//#define GPIO_PM_WCD9304_RESET           PM8917_GPIO_PM_TO_SYS(29)




#if defined(CONFIG_WCD9304_CLK_9600)
#define CLK_REVISION 0
#endif
/* gpio for changed list */
enum {
	BOARD_REV00,
	BOARD_REV01,
	BOARD_REV02,
	BOARD_REV03,
	BOARD_REV04,
	BOARD_REV05,
	BOARD_REV06,
	BOARD_REV07,
	BOARD_REV08,
	BOARD_REV09,
	BOARD_REV10,
	BOARD_REV11,
	BOARD_REV12,
	BOARD_REV13,
	BOARD_REV14,
	BOARD_REV15,
	GPIO_REV_MAX,
};

