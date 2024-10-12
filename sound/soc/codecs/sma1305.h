/*
 * sma1305.h -- sma1305 ALSA SoC Audio driver
 *
 * r006, 2020.12.18
 *
 * Copyright 2020 Silicon Mitus Corporation / Iron Device Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _SMA1305_H
#define _SMA1305_H

#define SMA1305_I2C_ADDR_00		0x1e
#define SMA1305_I2C_ADDR_01		0x3e
#define SMA1305_I2C_ADDR_10		0x5e
#define SMA1305_I2C_ADDR_11		0x7e

#define SMA1305_EXTERNAL_CLOCK_19_2		0x00
#define SMA1305_EXTERNAL_CLOCK_24_576	0x01
#define SMA1305_PLL_CLKIN_MCLK			0x02
#define SMA1305_PLL_CLKIN_BCLK			0x03

#define SMA1305_SDO_ONE_CH			0x00
#define SMA1305_SDO_TWO_CH_48			0x01
#define SMA1305_SDO_TWO_CH_24			0x02

#define SMA1305_SDO_DISABLE			0x00
#define SMA1305_FORMAT_C			0x01
#define SMA1305_MONO_MIX			0x02
#define SMA1305_AFTER_DSP			0x03
#define SMA1305_VRMS2_AVG			0x04
#define SMA1305_VBAT_MON			0x05
#define SMA1305_TEMP_MON			0x06
#define SMA1305_AFTER_DELAY			0x07

#define SMA1305_SPEAKER_MODE			0x00
#define SMA1305_RECEIVER_0P1W_MODE		0x01
#define SMA1305_RECEIVER_0P5W_MODE		0x02

#define SMA1305_OFFSET_DEFAULT_MODE		0x00
#define SMA1305_OFFSET_BURNING_MODE		0x01
/*
 * SMA1305 Register Definition
 */

/* SMA1305 Register Addresses */
#define SMA1305_00_SYSTEM_CTRL			0x00
#define SMA1305_01_INPUT_CTRL1			0x01
#define SMA1305_02_BROWN_OUT_PROT1		0x02
#define SMA1305_03_BROWN_OUT_PROT2		0x03
#define SMA1305_04_BROWN_OUT_PROT3		0x04
#define SMA1305_05_BROWN_OUT_PROT8		0x05
#define SMA1305_06_BROWN_OUT_PROT9		0x06
#define SMA1305_07_BROWN_OUT_PROT10		0x07
#define SMA1305_08_BROWN_OUT_PROT11		0x08
#define SMA1305_09_OUTPUT_CTRL			0x09
#define SMA1305_0A_SPK_VOL			0x0A
#define SMA1305_0B_BST_TEST			0x0B
#define SMA1305_0C_BOOST_CTRL8			0x0C
#define SMA1305_0D_SPK_TEST			0x0D
#define SMA1305_0E_MUTE_VOL_CTRL		0x0E
#define SMA1305_0F_VBAT_TEMP_SENSING		0x0F

#define SMA1305_10_SYSTEM_CTRL1			0x10
#define SMA1305_11_SYSTEM_CTRL2			0x11
#define SMA1305_12_SYSTEM_CTRL3			0x12
#define SMA1305_13_DELAY			0x13
#define SMA1305_14_MODULATOR			0x14
#define SMA1305_15_BASS_SPK1			0x15
#define SMA1305_16_BASS_SPK2			0x16
#define SMA1305_17_BASS_SPK3			0x17
#define SMA1305_18_BASS_SPK4			0x18
#define SMA1305_19_BASS_SPK5			0x19
#define SMA1305_1A_BASS_SPK6			0x1A
#define SMA1305_1B_BASS_SPK7			0x1B
#define SMA1305_1C_BROWN_OUT_PROT20		0x1C
#define SMA1305_1D_BROWN_OUT_PROT0		0x1D
#define SMA1305_1E_TONE_GENERATOR		0x1E
#define SMA1305_1F_TONE_FINE_VOLUME		0x1F

#define SMA1305_22_COMP_HYS_SEL			0x22
#define SMA1305_23_COMPLIM1			0x23
#define SMA1305_24_COMPLIM2			0x24
#define SMA1305_25_COMPLIM3			0x25
#define SMA1305_26_COMPLIM4			0x26
#define SMA1305_27_BROWN_OUT_PROT4		0x27
#define SMA1305_28_BROWN_OUT_PROT5		0x28
#define SMA1305_29_BROWN_OUT_PROT12		0x29
#define SMA1305_2A_BROWN_OUT_PROT13		0x2A
#define SMA1305_2B_BROWN_OUT_PROT14		0x2B
#define SMA1305_2C_BROWN_OUT_PROT15		0x2C
#define SMA1305_2D_BROWN_OUT_PROT6		0x2D
#define SMA1305_2E_BROWN_OUT_PROT7		0x2E
#define SMA1305_2F_BROWN_OUT_PROT16		0x2F

#define SMA1305_30_BROWN_OUT_PROT17		0x30
#define SMA1305_31_BROWN_OUT_PROT18		0x31
#define SMA1305_32_BROWN_OUT_PROT19		0x32
#define SMA1305_34_OCP_SPK				0x34
#define SMA1305_35_FDPEC_CTRL0			0x35
#define SMA1305_36_PROTECTION			0x36
#define SMA1305_37_SLOPECTRL			0x37
#define SMA1305_38_POWER_METER			0x38
#define SMA1305_39_PMT_NZ_VAL			0x39
#define SMA1305_3B_TEST1			0x3B
#define SMA1305_3C_TEST2			0x3C
#define SMA1305_3D_TEST3			0x3D
#define SMA1305_3E_IDLE_MODE_CTRL		0x3E
#define SMA1305_3F_ATEST2			0x3F
#define SMA1305_8B_PLL_POST_N			0x8B
#define SMA1305_8C_PLL_N			0x8C
#define SMA1305_8D_PLL_A_SETTING		0x8D
#define SMA1305_8E_PLL_P_CP			0x8E
#define SMA1305_8F_ANALOG_TEST			0x8F

#define SMA1305_90_CRESTLIM1			0x90
#define SMA1305_91_CRESTLIM2			0x91
#define SMA1305_92_FDPEC_CTRL1			0x92
#define SMA1305_93_INT_CTRL				0x93
#define SMA1305_94_BOOST_CTRL9			0x94
#define SMA1305_95_BOOST_CTRL10			0x95
#define SMA1305_96_BOOST_CTRL11			0x96
#define SMA1305_97_OTP_TRM0				0x97
#define SMA1305_98_OTP_TRM1				0x98
#define SMA1305_99_OTP_TRM2				0x99
#define SMA1305_9A_OTP_TRM3				0x9A

#define SMA1305_A0_PAD_CTRL0			0xA0
#define	SMA1305_A1_PAD_CTRL1			0xA1
#define SMA1305_A2_TOP_MAN1				0xA2
#define SMA1305_A3_TOP_MAN2				0xA3
#define SMA1305_A4_TOP_MAN3				0xA4
#define SMA1305_A5_TDM1					0xA5
#define SMA1305_A6_TDM2					0xA6
#define SMA1305_A7_CLK_MON				0xA7
#define SMA1305_A8_BOOST_CTRL1			0xA8
#define SMA1305_A9_BOOST_CTRL2			0xA9
#define SMA1305_AA_BOOST_CTRL3			0xAA
#define SMA1305_AB_BOOST_CTRL4			0xAB
#define SMA1305_AC_BOOST_CTRL5			0xAC
#define SMA1305_AD_BOOST_CTRL6			0xAD
#define SMA1305_AE_BOOST_CTRL7			0xAE
#define SMA1305_AF_LPF					0xAF

#define SMA1305_B0_RMS_TC1				0xB0
#define SMA1305_B1_RMS_TC2				0xB1
#define SMA1305_B2_AVG_TC1				0xB2
#define SMA1305_B3_AVG_TC2				0xB3
#define SMA1305_B4_PRVALUE1				0xB4
#define SMA1305_B5_PRVALUE2				0xB5

/* Status Register Read Only */
#define SMA1305_F5_READY_FOR_V_SAR		0xF5
#define SMA1305_F7_READY_FOR_T_SAR		0xF7
#define SMA1305_F8_STATUS_T1			0xF8
#define SMA1305_F9_STATUS_T2			0xF9
#define SMA1305_FA_STATUS1			0xFA
#define SMA1305_FB_STATUS2				0xFB
#define SMA1305_FC_STATUS3				0xFC
#define SMA1305_FD_STATUS4				0xFD
#define SMA1305_FF_DEVICE_INDEX			0xFF

/* SMA1303 Registers Bit Fields */
/* Power On/Off */
#define POWER_MASK			(1<<0)
#define POWER_OFF			(0<<0)
#define POWER_ON			(1<<0)

/* Left Polarity */
#define LEFTPOL_MASK			(1<<3)
#define LOW_FIRST_CH			(0<<3)
#define HIGH_FIRST_CH			(1<<3)

/* SCK Falling/Rising */
#define SCK_RISING_MASK			(1<<2)
#define SCK_FALLING_EDGE		(0<<2)
#define SCK_RISING_EDGE			(1<<2)

/* SPK Mute */
#define SPK_MUTE_MASK		(1<<0)
#define SPK_UNMUTE			(0<<0)
#define SPK_MUTE			(1<<0)

/* SPK Mode */
#define SPK_MODE_MASK		(7<<2)
#define SPK_OFF				(0<<2)
#define SPK_MONO			(1<<2)
#define SPK_STEREO			(4<<2)

/* Mono Mix */
#define MONOMIX_MASK (1<<0)
#define MONOMIX_OFF (0<<0)
#define MONOMIX_ON (1<<0)

/* PLL On/Off */
#define PLL_MASK			(1<<6)
#define PLL_ON				(0<<6)
#define PLL_OFF				(1<<6)

/* Input Format */
#define I2S_MODE_MASK		(7<<4)
#define STANDARD_I2S		(0<<4)
#define LJ					(1<<4)
#define RJ_16BIT			(4<<4)
#define RJ_18BIT			(5<<4)
#define RJ_20BIT			(6<<4)
#define RJ_24BIT			(7<<4)

/* Master / Slave Setting */
#define MASTER_SLAVE_MASK	(1<<7)
#define SLAVE_MODE			(0<<7)
#define MASTER_MODE			(1<<7)

/* Port Config */
#define PORT_CONFIG_MASK	(3<<6)
#define INPUT_PORT_ONLY		(0<<6)
#define OUTPUT_PORT_ENABLE	(2<<6)

/* SDO Output */
#define SDO_OUTPUT_MASK		(1<<3)
#define LOGIC_OUTPUT		(0<<3)
#define HIGH_Z_OUTPUT		(1<<3)

/* SDO Output2 */
#define SDO_OUTPUT2_MASK	(1<<0)
#define ONE_SDO_PER_CH		(0<<0)
#define TWO_SDO_PER_CH		(1<<0)

/* SDO Output3 */
#define SDO_OUTPUT3_MASK	(1<<2)
#define SDO_OUTPUT3_DIS		(0<<2)
#define TWO_SDO_PER_CH_24K	(1<<2)

/* SDO OUT1 Select*/
#define SDO_OUT1_SEL_MASK	(7<<3)
#define SDO1_DISABLE		(0<<3)
#define SDO1_FORMAT_C		(1<<3)
#define SDO1_MONO_MIX		(2<<3)
#define SDO1_AFTER_DSP		(3<<3)
#define SDO1_VRMS2_AVG		(4<<3)
#define SDO1_VBAT_MON		(5<<3)
#define SDO1_TEMP_MON		(6<<3)
#define SDO1_AFTER_DELAY	(7<<3)

/* SDO OUT0 Select*/
#define SDO_OUT0_SEL_MASK	(7<<0)
#define SDO0_DISABLE		(0<<0)
#define SDO0_FORMAT_C		(1<<0)
#define SDO0_MONO_MIX		(2<<0)
#define SDO0_AFTER_DSP		(3<<0)
#define SDO0_VRMS2_AVG		(4<<0)
#define SDO0_VBAT_MON		(5<<0)
#define SDO0_TEMP_MON		(6<<0)
#define SDO0_AFTER_DELAY	(7<<0)

/* INTERRUPT Operation */
#define SEL_INT_MASK		(1<<2)
#define INT_CLEAR_AUTO		(0<<2)
#define INT_CLEAR_MANUAL	(1<<2)

/* INTERRUPT CLEAR */
#define CLR_INT_MASK		(1<<1)
#define INT_READY			(0<<1)
#define INT_CLEAR			(1<<1)

/* INTERRUPT Disable */
#define DIS_INT_MASK		(1<<0)
#define NORMAL_INT			(0<<0)
#define HIGH_Z_INT			(1<<0)

/* Interface Control */
#define INTERFACE_MASK		(7<<5)
#define LJ_FORMAT			(1<<5)
#define I2S_FORMAT			(2<<5)
#define TDM_FORMAT			(4<<5)

#define SCK_RATE_MASK		(3<<3)
#define SCK_64FS		(0<<3)
#define SCK_32FS		(2<<3)

#define DATA_WIDTH_MASK		(3<<1)
#define DATA_24BIT		(0<<1)
#define DATA_16BIT		(3<<1)

#define TDM_TX_MODE_MASK		(1<<6)
#define TDM_TX_MONO		(0<<6)
#define TDM_TX_STEREO		(1<<6)

#define TDM_SLOT1_RX_POS_MASK (7<<3)
#define TDM_SLOT1_RX_POS_0 (0<<3)
#define TDM_SLOT1_RX_POS_1 (1<<3)
#define TDM_SLOT1_RX_POS_2 (2<<3)
#define TDM_SLOT1_RX_POS_3 (3<<3)
#define TDM_SLOT1_RX_POS_4 (4<<3)
#define TDM_SLOT1_RX_POS_5 (5<<3)
#define TDM_SLOT1_RX_POS_6 (6<<3)
#define TDM_SLOT1_RX_POS_7 (7<<3)

#define TDM_SLOT2_RX_POS_MASK (7<<0)
#define TDM_SLOT2_RX_POS_0 (0<<0)
#define TDM_SLOT2_RX_POS_1 (1<<0)
#define TDM_SLOT2_RX_POS_2 (2<<0)
#define TDM_SLOT2_RX_POS_3 (3<<0)
#define TDM_SLOT2_RX_POS_4 (4<<0)
#define TDM_SLOT2_RX_POS_5 (5<<0)
#define TDM_SLOT2_RX_POS_6 (6<<0)
#define TDM_SLOT2_RX_POS_7 (7<<0)

/* TDM2 FORMAT : 0xA6 */
#define TDM_DL_MASK	(1<<7)
#define TDM_DL_16	(0<<7)
#define TDM_DL_32	(1<<7)

#define TDM_N_SLOT_MASK	(1<<6)
#define TDM_N_SLOT_4	(0<<6)
#define TDM_N_SLOT_8	(1<<6)

#define TDM_SLOT1_TX_POS_MASK	(7<<3)
#define TDM_SLOT1_TX_POS_0	(0<<3)
#define TDM_SLOT1_TX_POS_1	(1<<3)
#define TDM_SLOT1_TX_POS_2	(2<<3)
#define TDM_SLOT1_TX_POS_3	(3<<3)
#define TDM_SLOT1_TX_POS_4	(4<<3)
#define TDM_SLOT1_TX_POS_5	(5<<3)
#define TDM_SLOT1_TX_POS_6	(6<<3)
#define TDM_SLOT1_TX_POS_7	(7<<3)

#define TDM_SLOT2_TX_POS_MASK	(7<<0)
#define TDM_SLOT2_TX_POS_0	(0<<0)
#define TDM_SLOT2_TX_POS_1	(1<<0)
#define TDM_SLOT2_TX_POS_2	(2<<0)
#define TDM_SLOT2_TX_POS_3	(3<<0)
#define TDM_SLOT2_TX_POS_4	(4<<0)
#define TDM_SLOT2_TX_POS_5	(5<<0)
#define TDM_SLOT2_TX_POS_6	(6<<0)
#define TDM_SLOT2_TX_POS_7	(7<<0)

/* OTP Trim SPK Offset */
#define SPK_OFFS2_MSB_MASK	(1<<5)
#define SPK_OFFS2_MASK		(31<<0)

/* OTP Trim RCV Offset */
#define RCV_OFFS2_MSB_MASK	(1<<7)
#define RCV_OFFS2_MASK		(15<<4)
#define RCV_OFFS2_DEFAULT_VALUE (0<<4)

/* OTP STATUS */
#define OTP_STAT_MASK		(1<<6)
#define OTP_STAT_0			(0<<6)
#define OTP_STAT_1			(1<<6)

/* STATUS */
#define OT1_OK_STATUS		(1<<7)
#define OT2_OK_STATUS		(1<<6)
#define UVLO_STATUS			(1<<5)
#define OVP_BST_STATUS		(1<<4)
#define POWER_FLAG			(1<<3)

#define SCAN_CHK			(1<<7)
#define OCP_SPK_STATUS		(1<<5)
#define OCP_BST_STATUS		(1<<4)
#define BOP_STATE			(7<<1)
#define CLK_MON_STATUS		(1<<0)

#define DEVICE_ID			(3<<3)
#define REV_NUM_STATUS		(3<<0)
#define REV_NUM_REV0		(0<<0)
#define REV_NUM_REV1		(1<<0)

extern int sma1305_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id);
extern int sma1305_i2c_remove(struct i2c_client *client);
#endif
