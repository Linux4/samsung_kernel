/**
 * \copyright
 * MIT License
 *
 * Copyright (c) 2024 Infineon Technologies AG
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE
 *
 * \endcopyright
 */
/**
 * @file   register.h
 * @date   January, 2021
 * @brief  List of Authenticate On register
 */
#ifndef INTERFACE_REGISTER_H_
#define INTERFACE_REGISTER_H_

#include "../authon_config.h"

#define  UID_BYTE_LEN			    (12u)

/* Device  Register */    
// NVM User Address Space (0x0000-0x001F)
#define ON_USR_NVM_BASE				(uint16_t)0x0000u	// Base address of User NVM


// NVM IFX Address Space (0x00A4-0x00DB)
#define ON_IFX_NVM_ADDR				(uint16_t)(ON_USR_NVM_BASE + 0x40u)	// Base address of IFX NVM
#define ON_IFX_NVM_END				(uint16_t)(ON_USR_NVM_BASE + 0xA7u)	// End address of IFX NVM

// ODC 
#define ON_ODC1_ADDR				(uint16_t)(ON_USR_NVM_BASE + 0x40u) // ODC1 base address
#define ON_ODC1_END				    (uint16_t)(ON_USR_NVM_BASE + 0x4Bu) // ODC1 end address,first two bytes
#define ON_ODC2_ADDR				(uint16_t)(ON_USR_NVM_BASE + 0x52u) // ODC2 base address
#define ON_ODC2_END				    (uint16_t)(ON_USR_NVM_BASE + 0x5Du) // ODC2 end address,first two bytes

// AUTH PKey
#define ON_AUTH_PK1_ADDR			(uint16_t)(ON_USR_NVM_BASE + 0x4Cu) // Auth PKey1 base address,start from byte2
#define ON_AUTH_PK1_END			    (uint16_t)(ON_USR_NVM_BASE + 0x51u) // Auth Pkey1 End address
#define ON_AUTH_PK2_ADDR			(uint16_t)(ON_USR_NVM_BASE + 0x5Eu) // Auth PKey2 base address,start from byte2
#define ON_AUTH_PK2_END			    (uint16_t)(ON_USR_NVM_BASE + 0x63u) // Auth Pkey2 End address

// UID
#define ON_UID_ADDR		            (uint16_t)(ON_USR_NVM_BASE + 0x64u) // UID base address
#define ON_UID_END		            (uint16_t)(ON_USR_NVM_BASE + 0x67u) // UID End address
#define ON_UID_SIZE		            (uint16_t)((ON_UID_END-ON_UID_ADDR + 1u)*4u) //
#define ON_UID_PAGE_SIZE	        (uint16_t)(ON_UID_END-ON_UID_ADDR + 1u) //

// Life Span Counter
#define ON_LSC_BUF_1		        (uint16_t)(ON_USR_NVM_BASE + 0x6Cu) // Life Span Counter Buffer1
#define ON_LSC_BUF_2		        (uint16_t)(ON_USR_NVM_BASE + 0x6Du) // Life Span Counter Buffer2
#define ON_LSC_1			        (uint16_t)(ON_USR_NVM_BASE + 0x70u) // Life Span Counter1
#define ON_LSC_2			        (uint16_t)(ON_USR_NVM_BASE + 0x71u) // Life Span Counter2

// Specific Function Register (SFR)(0x4000-0x40FF)
#define ON_SFR_BASE					(uint16_t)0x4000u	// Base address of Authenticate On SFR
#define ON_SFR_END					(uint16_t)(ON_SFR_BASE + 0xFFu)	// End address of Authenticate On SFR
#define ON_SFR_USER_NVM_LOCK_1		(uint16_t)(ON_SFR_BASE + 0x00u)	// User NVM Lock Control Reg 1 (Write-Only)
#define ON_SFR_USER_NVM_LOCK_2		(uint16_t)(ON_SFR_BASE + 0x01u)	// User NVM Lock Control Reg 2 (Write-Only)
#define ON_SFR_USER_NVM_LOCK_3		(uint16_t)(ON_SFR_BASE + 0x02u)	// User NVM Lock Control Reg 3 (Write-Only)
#define ON_SFR_USER_NVM_LOCK_4		(uint16_t)(ON_SFR_BASE + 0x03u)	// User NVM Lock Control Reg 4 (Write-Only)
#define ON_SFR_USER_NVM_LOCK_5		(uint16_t)(ON_SFR_BASE + 0x04u)	// User NVM Lock Control Reg 5 (Write-Only)
#define ON_SFR_USER_NVM_LOCK_6		(uint16_t)(ON_SFR_BASE + 0x05u)	// User NVM Lock Control Reg 6 (Write-Only)
#define ON_SFR_USER_NVM_LOCK_7		(uint16_t)(ON_SFR_BASE + 0x06u)	// User NVM Lock Control Reg 7 (Write-Only)
#define ON_SFR_USER_NVM_LOCK_8		(uint16_t)(ON_SFR_BASE + 0x07u)	// User NVM Lock Control Reg 8 (Write-Only)
#define ON_SFR_USER_NVM_LOCK_STS_1	(uint16_t)(ON_SFR_BASE + 0x08u)	// User NVM Lock Status Reg 1 (Read-Only)
#define ON_SFR_USER_NVM_LOCK_STS_2	(uint16_t)(ON_SFR_BASE + 0x09u)	// User NVM Lock Status Reg 2 (Read-Only)
#define ON_SFR_USER_NVM_LOCK_STS_3	(uint16_t)(ON_SFR_BASE + 0x0Au)	// User NVM Lock Status Reg 3 (Read-Only)
#define ON_SFR_USER_NVM_LOCK_STS_4	(uint16_t)(ON_SFR_BASE + 0x0Bu)	// User NVM Lock Status Reg 4 (Read-Only)
#define ON_SFR_USER_NVM_LOCK_STS_5	(uint16_t)(ON_SFR_BASE + 0x0Cu)	// User NVM Lock Status Reg 5 (Read-Only)
#define ON_SFR_USER_NVM_LOCK_STS_6	(uint16_t)(ON_SFR_BASE + 0x0Du)	// User NVM Lock Status Reg 6 (Read-Only)
#define ON_SFR_USER_NVM_LOCK_STS_7	(uint16_t)(ON_SFR_BASE + 0x0Eu)	// User NVM Lock Status Reg 7 (Read-Only)
#define ON_SFR_USER_NVM_LOCK_STS_8	(uint16_t)(ON_SFR_BASE + 0x0Fu)	// User NVM Lock Status Reg 8 (Read-Only)
#define ON_SFR_LSC_CTRL		    	(uint16_t)(ON_SFR_BASE + 0x10u)	// LifeSpanCounter Control (Write-Only)
#define ON_SFR_LSC_CONF	    		(uint16_t)(ON_SFR_BASE + 0x11u)	// LifeSpanCounter Configure (Write-Only)
#define ON_SFR_KILL_CONF	    	(uint16_t)(ON_SFR_BASE + 0x12u)	// Kill Configure (Write-Only)
#define ON_SFR_LSC_FEAT_STS			(uint16_t)(ON_SFR_BASE + 0x13u)	// LifeSpanCounter Feature Status (Read-Only)
#define ON_SFR_FEAT_STS				(uint16_t)(ON_SFR_BASE + 0x14u)	// Feature Status (Read-Only)
#define ON_SFR_LSC_KILL_ACT_STS	    (uint16_t)(ON_SFR_BASE + 0x15u)	// LifeSpanCounter Kill Act Status (Read-Only)
#define ON_SFR_BUSY_STS				(uint16_t)(ON_SFR_BASE + 0x16u)	// Busy Status (Read-Only)
#define ON_SFR_LOCK_CONF_STS		(uint16_t)(ON_SFR_BASE + 0x17u)	// Lock and Configuration Status Reg (Read-Only)
#define ON_SFR_LSC_DEC_VAL_L		(uint16_t)(ON_SFR_BASE + 0x18u)	// LifeSpanCounter Decrement Low (Read-Only)
#define ON_SFR_LSC_DEC_VAL_H		(uint16_t)(ON_SFR_BASE + 0x19u)	// LifeSpanCounter Decrement High (Read-Only)
#define	ON_SFR_SWI_CONF_0			(uint16_t)(ON_SFR_BASE + 0x20u)	// SWI Configure0 (Read/Write)
#define ON_SFR_SWI_CONF_1			(uint16_t)(ON_SFR_BASE + 0x21u)	// SWI Configure1 (Read/Write)

#define ON_SFR_SWI_WCNT_STAT        (uint16_t)(ON_SFR_BASE + 0x23u)	// SWI Status Word Count (Read-Only)
#define ON_SFR_SWI_ERR_CNT_STS      (uint16_t)(ON_SFR_BASE + 0x24u)	// SWI Status Error Count (Read-Only)
#define ON_SFR_SWI_ERR_CODE_STS     (uint16_t)(ON_SFR_BASE + 0x25u)	// SWI Status Last Error Code (Read-Only)

#define ON_SFR_SWI_INT_EN	        (uint16_t)(ON_SFR_BASE + 0x26u)	// SWI Interrupt enable Register (Read/Write)
#define ON_SFR_SWI_INT_STS	        (uint16_t)(ON_SFR_BASE + 0x27u)	// SWI Interrupt status Register (Read-Only)
#define ON_SFR_SWI_DADR0_L		    (uint16_t)(ON_SFR_BASE + 0x28u)	// SWI Device Address Register0_L (Read/Write)
#define ON_SFR_SWI_DADR0_H		    (uint16_t)(ON_SFR_BASE + 0x29u)	// SWI Device Address Register0_H (Read/Write)
#define ON_SFR_SWI_RD_LEN		    (uint16_t)(ON_SFR_BASE + 0x2Au)	// SWI Read Data Length (Read/Write)
#define ON_SFR_SWI_WD_LEN		    (uint16_t)(ON_SFR_BASE + 0x2Bu)	// SWI Write Data Length (Read/Write)


#define HiByte(din_16) ((din_16 >> 8) & 0xff)
#define LoByte(din_16) ((din_16) & 0xff)

// Flag
#define BUSY	1u
#define	NOTBUSY	0u

// ON_SFR_LSC_CTRL bit mapping
#define BIT_DEC_LSC_1				 1
#define BIT_DEC_LSC_2				 3

// ON_SFR_LSC_CONF bit mapping
#define BIT_LSC_1_EN				 1
#define BIT_LSC_2_EN				 3

// ON_SFR_KILL_CONF bit mapping
#define BIT_KILL_EN					 6

// ON_SFR_LSC_FEAT_STS bit mapping
#define BIT_LSC_1_EN_STS			1
#define BIT_LSC_2_EN_STS			3

// ON_SFR_FEAT_STS bit mapping
#define BIT_KILL_FEATURE_EN_STS				6
#define BIT_NVM_USER_LOCK_RESET_STS			7

// ON_SFR_LSC_KILL_ACT_STS bit mapping
#define BIT_LSC_DEC_1_STS			1
#define BIT_LSC_DEC_2_STS			2
#define BIT_KILL_IND				6

// ON_SFR_BUSY_STS bit mapping
#define BIT_NVM_BUSY					(0)
#define BIT_RANDOM_NUM_BUSY				(6)
#define BIT_AUTH_MAC_BUSY				(7)


// ON_SFR_LOCK_CONF_STS
#define BIT_IFX_KILL_CONF			 0
#define BIT_IFX_AUTO_KILL_CONF		 1
#define BIT_HOSTAUTH_CONF			 2
#define BIT_CHIP_LOCK				 4
#define BIT_IFX_USERNVM_RESET_CONFIG 6

// ON_SFR_SWI_CONF_0
#define BIT_DEV_REG	  				0
#define BIT_DEV_RES	  				1
#define BIT_ERR_RES	  				4
#define BIT_WCNT_RES	  			5

// ON_SFR_SWI_CONF_1
#define BIT_DEV_INT_EN				0

// ON_SFR_SWI_INT_EN
#define BIT_ECC_DONE_INT_EN			0
#define BIT_DEC_LFSP_END_INT_EN		7

// ON_SFR_SWI_INT_STS
#define BIT_ECC_DONE_INT_STS		0
#define BIT_DEC_LFSP_END_INT_STS	7


typedef enum REGISTER_ACCESS_{
    ACCESS_AFTER_HOST_AUTH, 
    ACCESS_ALWAYS 
} Register_Access;

#endif /* INTERFACE_REGISTER_H_ */
