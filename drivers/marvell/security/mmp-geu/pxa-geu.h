/*
 *  pxa-geu driver for generic encrypt unit
 *
 *  Copyright (C) 2009, Marvell International Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#ifndef	_PXA_GEU_H_
#define	_PXA_GEU_H_

/*
 *	THE REGISTER DEFINES
 */
#define	GEU_STATUS							(0x0000)
#define	GEU_CONFIG							(0x0004)
#define	GEU_INIT_KEY_VALUE					(0x0008)
#define	GEU_INPUT_DATA_ENC_DEC				(0x0028)
#define	GEU_1ST_OFF_CODE_OCB_MODE			(0x0038)
#define	GEU_FUSE_PROG_VAL1					(0x0038)
#define	GEU_2ND_OFF_CODE_OCB_MODE			(0x0048)
#define	GEU_FUSE_PROG_VAL2					(0x0048)
#define	GEU_OUT_DATA_AFTER_ENC_DEC			(0x0058)
#define	GEU_FUSE_VAL_APCFG1					(0x0404)
#define	GEU_FUSE_VAL_APCFG2					(0x0408)
#define	GEU_FUSE_VAL_APCFG3					(0x040C)
#define	GEU_FUSE_VAL_USB_VER_ID				(0x0410)
#define	GEU_FUSE_VAL_SW_VER_ID_0			(0x0414)
#define	GEU_FUSE_VAL_SW_VER_ID_1			(0x0418)
#define	BLOCK0_RESERVED_0					(0x041C)
#define	BLOCK0_RESERVED_1					(0x0420)
#define	GEU_FUSE_VAL_ROOT_KEY1				(0x0424)
#define	GEU_FUSE_VAL_ROOT_KEY2				(0x0428)
#define	GEU_FUSE_VAL_ROOT_KEY3				(0x042C)
#define	GEU_FUSE_VAL_ROOT_KEY4				(0x0430)
#define	GEU_FUSE_VAL_ROOT_KEY5				(0x0434)
#define	GEU_FUSE_VAL_ROOT_KEY6				(0x0438)
#define	GEU_FUSE_VAL_ROOT_KEY7				(0x043C)
#define	GEU_FUSE_VAL_ROOT_KEY8				(0x0440)
#define	GEU_FUSE_VAL_OEM_HASH_KEY			(0x0444)
#define	GEU_FUSE_VAL_OEM_JTAG_HASH			(0x0464)
#define	GEU_FUSE_STATUS						(0x0484)
#define	GEU_HW_RANDOM_NUM_GEN				(0x0488)
#define	GEU_FUSE_VAL_OEM_JTAG_KEY_HASH_ECC	(0x048C)
#define	BLOCK7_RESERVED_0					(0x0490)
#define	BLOCK7_RESERVED_1					(0x0494)
#define	BLOCK7_RESERVED_2					(0x0498)
#define	BLOCK7_RESERVED_3					(0x049C)
#define	BLOCK7_RESERVED_4					(0x04A0)
#define	BLOCK7_RESERVED_5					(0x04A4)
#define	OEM_KEY_HASH_ECC					(0x04A8)
#define	GEU_ECC_CAL							(0x04C0)
#define	GEU_ECC_STATUS						(0x04C4)
#define	GEU_FUSE_VAL_APCP_ECC				(0x04C8)


/*	GEU_STATUS	0x0000	Status Register */
/*		Bit(s) GEU_STATUS_RSRV_31_10 reserved */
#define	GEU_STATUS_DATAISR					(1<<9)	/* Data Interrupt Status Register */
#define	GEU_STATUS_KEYISR					(1<<8)	/* Key Interrupt Status Register */
/*		Bit(s) GEU_STATUS_RSRV_7_6 reserved */
#define	GEU_STATUS_DATAPROC					(1<<5)	/* Data Encryption/Decryption Status Indicator */
#define	GEU_STATUS_KEYPROC					(1<<4)	/* Round Key Computation Status Indicator */
#define	GEU_STATUS_DOUTREADY				(1<<3)	/* Encrypted/Decrypted Data Ready Indicator */
#define	GEU_STATUS_KEYDONE					(1<<2)	/* Round Key Ready Indicator */
#define	GEU_STATUS_DINREADY					(1<<1)	/* Data Encryption Ready Indicator */
#define	GEU_STATUS_KEYREADY					(1<<0)	/* Start of Round Key Computation Indicator */

/*	GEU_CONFIG	0x0004	Configuration Register */
#define	GEU_CONFIG_EN_DMA_MODE_AES_CIPHER	(1<<31)	/* Enable DMA Mode of AES Cipher */
#define	GEU_CONFIG_STICKY_CONTROL_BIT		(1<<30)	/* Sticky Control Bit */
#define	GEU_CONFIG_CBC_ECB_MODE				(1<<29)	/* CBC/ECB Mode */
#define	GEU_CONFIG_AES_MEMORY_POWER_DOWN	(1<<28)	/* AES Memory Power Down */
#define	GEU_CONFIG_FUSE_CLOCK_DIVIDER_MASK	(0x7<<25)	/* Clock Divider Value Mask*/
#define	GEU_CONFIG_FUSE_CLOCK_DIVIDER_SHIFT	(25)	/* Clock Divider Value Shift */
#define	GEU_CONFIG_WRITE_INI_VAL_IN_CBC_MODE	(1<<24)	/* Write Initial Value in CBC Mode */
#define	GEU_CONFIG_FUSE_CLOCK_DISABLE		(1<<23)	/* Fuse Clock Disable */
#define	GEU_CONFIG_FUSE_SOFTWARE_RESET		(1<<22)	/* Fuse Software Reset */
#define	GEU_CONFIG_FAB_PATH_SEL				(1<<21)	/* Fabric #1 and #2 register path selection */
#define	GEU_CONFIG_FUSE_BLOCK_NUMBER_MASK	(0x7<<18)	/* Fuse Block Number Mask */
#define	GEU_CONFIG_FUSE_BLOCK_NUMBER_SHIFT	(18)	/* Fuse Block Number Shift */
#define	GEU_CONFIG_HIGH_VOLT_ENABLE			(1<<17)	/* High Volt Enable */
#define	GEU_CONFIG_BURN_FUSE_ENABLE			(1<<16)	/* Burn Fuse Enable */
#define	GEU_CONFIG_FUSE_LOCK				(1<<15)	/* Fuse Lock */
#define	GEU_CONFIG_ENABLE_SOFT_FUSE_PROG	(1<<14)	/* Enable Software Fuse Programming */
#define	GEU_CONFIG_AES_KEY_SIZE_SEL			(1<<13)	/* AES key select */
#define	GEU_CONFIG_USEOFS0					(1<<12)	/* Offset Control */
#define	GEU_CONFIG_DATARSR					(1<<11)	/* Data Interrupt Reset Select Register */
#define	GEU_CONFIG_DATAIMR					(1<<10)	/* Data Interrupt Mask Register */
#define	GEU_CONFIG_KEYRSR					(1<<9)	/* Key Interrupt Reset Select Register */
#define	GEU_CONFIG_KEYIMR					(1<<8)	/* Key Interrupt Mask Register */
#define	GEU_CONFIG_JTAG_ACCESS_CTRL			(1<<7)	/* JTAG Access Control */
#define	GEU_CONFIG_DMA_REQ_SEL_SIG			(1<<6)	/* DMA Request Select Signal */
#define	GEU_CONFIG_RNDKEYHA					(1<<5)	/* Round Key Host Access */
#define	GEU_CONFIG_SBOXHA					(1<<4)	/* Sbox Host Access */
#define	GEU_CONFIG_ENCDEC					(1<<3)	/* Encryption/Decryption Enabled */
#define	GEU_CONFIG_OCBBYP					(1<<2)	/* OCB Bypass */
#define	GEU_CONFIG_KEYSZ_MASK				(0x3)	/* Key Size */
#define	GEU_CONFIG_KEYSZ_SHIFT				(0)

/*	GEU_FUSE_STATUS	0x0484	Fuse Operation Status Register */
/*		Bit(s) GEU_FUSE_STATUS_RSRV_31_30 reserved */
#define	GEU_FUSE_STATUS_SPARE_FUSE			(1<<29)	/* Spare fuse bit */
/*		Bit(s) GEU_FUSE_STATUS_RSRV_28_26 reserved */
#define	GEU_FUSE_STATUS_LIFE_CYCLE_MASK		(0xffff<<10)	/* Life Cycle Fuse Bits */
#define	GEU_FUSE_STATUS_LIFE_CYCLE_SHIFT	(10)	/* Life Cycle Fuse Bits */
#define	GEU_FUSE_STATUS_FUSE_READY			(1<<9)	/* Fuse Ready */
#define	GEU_FUSE_STATUS_FUSE_BURN_DONE		(1<<8)	/* Fuse Burn Done */
#define	GEU_FUSE_STATUS_LOCK_BIT_MASK		(0xff)	/* Fuse Lock Bit */
#define	GEU_FUSE_STATUS_LOCK_BIT_SHIFT		(0)

/*	GEU_ECC_STATUS	0x04C4	ECC Status Register */
#define	GEU_ECC_STATUS_ERR_TYPE_9_MASK		(0x3<<30)	/* Error Type 9 */
#define	GEU_ECC_STATUS_ERR_TYPE_9_SHIFT		(30)
#define	GEU_ECC_STATUS_ERR_TYPE_8_MASK		(0x3<<28)	/* Error Type 8 */
#define	GEU_ECC_STATUS_ERR_TYPE_8_SHIFT		(28)
#define	GEU_ECC_STATUS_ERR_TYPE_7_MASK		(0x3<<26)	/* Error Type 7 */
#define	GEU_ECC_STATUS_ERR_TYPE_7_SHIFT		(26)
#define	GEU_ECC_STATUS_ERR_TYPE_6_MASK		(0x3<<24)	/* Error Type 6 */
#define	GEU_ECC_STATUS_ERR_TYPE_6_SHIFT		(24)
/*		Bit(s) GEU_ECC_STATUS_RSRV_23_22 reserved */
#define	GEU_ECC_STATUS_ERR_TYPE_5_MASK		(0x3<<20)	/* Error Type 5 */
#define	GEU_ECC_STATUS_ERR_TYPE_5_SHIFT		(20)
/*		Bit(s) GEU_ECC_STATUS_RSRV_19_18 reserved */
#define	GEU_ECC_STATUS_ERR_TYPE_4_MASK		(0x3<<16)	/* Error Type 4 */
#define	GEU_ECC_STATUS_ERR_TYPE_4_SHIFT		(16)
/*		Bit(s) GEU_ECC_STATUS_RSRV_15_14 reserved */
#define	GEU_ECC_STATUS_ERR_TYPE_3_MASK		(0x3<<12)	/* Error Type 3 */
#define	GEU_ECC_STATUS_ERR_TYPE_3_SHIFT		(12)
/*		Bit(s) GEU_ECC_STATUS_RSRV_11_10 reserved */
#define	GEU_ECC_STATUS_ERR_TYPE_2_MASK		(0x3<<8)	/* Error Type 2 */
#define	GEU_ECC_STATUS_ERR_TYPE_2_SHIFT		(8)
/*		Bit(s) GEU_ECC_STATUS_RSRV_7_6 reserved */
#define	GEU_ECC_STATUS_ERR_TYPE_1_MASK		(0x3<<4)	/* Error Type 1 */
#define	GEU_ECC_STATUS_ERR_TYPE_1_SHIFT		(4)
/*		Bit(s) GEU_ECC_STATUS_RSRV_3_2 reserved */
#define	GEU_ECC_STATUS_ERR_TYPE_0_MASK		(0x3)	/* Error Type 0 */
#define	GEU_ECC_STATUS_ERR_TYPE_0_SHIFT		(0)

// FB 2 - [255:192] GEU_ECC_STATUS [13:12] ERR_TYPE_3
// FB 2 - [191:128] GEU_ECC_STATUS [09:08] ERR_TYPE_2
// FB 2 - [127:64]  GEU_ECC_STATUS [05:04] ERR_TYPE_1
// FB 2 - [63:0]    GEU_ECC_STATUS [01:00] ERR_TYPE_0
#define K_OEM_UNCORRECTABLE_ECC_ERROR_MASK \
((GEU_ECC_STATUS_ERR_TYPE_3_MASK & (0x02 << GEU_ECC_STATUS_ERR_TYPE_3_SHIFT)) | \
 (GEU_ECC_STATUS_ERR_TYPE_2_MASK & (0x02 << GEU_ECC_STATUS_ERR_TYPE_2_SHIFT)) | \
 (GEU_ECC_STATUS_ERR_TYPE_1_MASK & (0x02 << GEU_ECC_STATUS_ERR_TYPE_1_SHIFT)) | \
 (GEU_ECC_STATUS_ERR_TYPE_0_MASK & (0x02 << GEU_ECC_STATUS_ERR_TYPE_0_SHIFT)))

#endif	/* _PXA_GEU_H_ */
