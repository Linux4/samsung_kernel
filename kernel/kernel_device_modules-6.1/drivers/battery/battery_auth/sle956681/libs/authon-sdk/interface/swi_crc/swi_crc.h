#ifndef SWI_CRC_H_
#define SWI_CRC_H_

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
 * @file   swi_crc.h
 * @date   January, 2021
 * @brief  Implementation of SWI bus protocol
 */

#include "../../authon_status.h"

#define SWI_FRAME_BIT_SIZE    17    /*!< SWI consists of 2 training + 2 CMD + 8 DATA + 1 INV bits + 4 Parity bits*/

/**
 * @brief Defines the speed of SWI baud rate
 */
typedef enum {
  HIGH_SPEED = 0, /*!< High speed SWI baud rate */
  LOW_SPEED       /*!< Low speed SWI baud rate */
} SWI_SPEED;

/* Transaction Elements */
/* BroadCast */
#define SWI_BC              (0x08u)    /* Bus Command */
#define SWI_EDA             (0x09u)    /* Extended Device Address */
#define SWI_SDA             (0x0Au)    /* Slav Device Address */

/* MultiCast */
#define SWI_WD              (0x04u)    /* Write Data */
#define SWI_ERA             (0x05u)    /* Extended Register Address */
#define SWI_WRA             (0x06u)    /* Write Register Address */
#define SWI_RRA             (0x07u)    /* Read Register Address */

/* Unicast */
#define SWI_RD_ACK          (0x06u)    /* ACK and not End of transmission */
#define SWI_RD_NACK         (0x04u)    /* NACK and not End of transmission */
#define SWI_RD_ACK_EOT      (0x07u)    /* ACK and End of transmission */
#define SWI_RD_NACK_EOT     (0x05u)    /* NACK and End of transmission */

/* Bus Command */
#define SWI_BRES            (0x00u)    /* Bus Reset */
#define SWI_PDWN            (0x02u)    /* Power Down */
#define SWI_EINT            (0x10u)    /* Enable Interrupt */
#define SWI_WDA	            (0x18u)    /* Write Device Address */
#define SWI_RBL0            (0x20u)    /* RBLn Set Read Burst Length 2^n */
#define SWI_RBL1            (0x21u)    /* RBLn Set Read Burst Length 2^n */
#define SWI_RBL2            (0x22u)    /* RBLn Set Read Burst Length 2^n */
#define SWI_RBL3            (0x23u)    /* RBLn Set Read Burst Length 2^n */
#define SWI_DISS            (0x30u)    /* Device ID Search Start */
#define SWI_DIMM            (0x32u)    /* Device ID Search Memory */
#define SWI_DIRC            (0x33u)    /* Device ID Search Recall */
#define SWI_DIE0            (0x34u)    /* Device ID Search Enter 0 */
#define SWI_DIE1            (0x35u)    /* Device ID Search Enter 1 */
#define SWI_DIP0            (0x36u)    /* Device ID Search Probe 0 */
#define SWI_DIP1            (0x37u)    /* Device ID Search Probe 1 */
#define SWI_DI00            (0x38u)    /* DIS Enter 0 Probe 0 (DIE0 + DIP0) */
#define SWI_DI01            (0x39u)    /* DIS Enter 0 Probe 1 (DIE0 + DIP1) */
#define SWI_DI10            (0x3Au)    /* DIS Enter 1 Probe 0 (DIE1 + DIP0) */
#define SWI_DI11            (0x3Bu)    /* DIS Enter 1 Probe 1 (DIE1 + DIP1) */
#define SWI_DASM            (0x40u)    /* Device Activation Stick Mode */
#define SWI_DACL            (0x41u)    /* Device Activation Clear */
#define SWI_CURD            (0x70u)    /* Call for Un-registered Devices */

#define SWI_ECCS1	        (0xC1)    /* Start ECC1 */
#define SWI_ECCS2	        (0xC2)    /* Start ECC2 */
#define SWI_ECCC	        (0xC3)    /* Send ECC challenge */
#define SWI_ECCR	        (0xC4)    /* ECC response */

#define SWI_MACS            (0xD2)    /* Start MAC  Process*/
#define SWI_MACR            (0xD4)    /* MAC response */
#define SWI_MACK            (0xD8)    /* MAC kill */
#define SWI_MACCR5          (0xdd)


#define SWI_HRREQ	          (0xE0)    /* Host Request Random Number */
#define SWI_HRRES	          (0xE1)    /* Host Receive Random Number */
#define SWI_DRRES			  (0xE2)
#define SWI_HREQ1		      (0xE3)
#define SWI_DRES1		      (0xE4)

#define SWI_ERA_DUMMYADDR 			(0x50)
#define SWI_WRARRA_DUMMYADDR 		(0x80)

extern uint8_t g_ulBaudStop;

void Swi_SendRawWord( uint32_t uw_SwiCMD, uint8_t b_WaitForInterrupt, uint8_t b_ImmediateInterrupt, uint8_t * bp_IrqDetected );
void Swi_SendRawWordNoIrq( uint8_t ub_Code, uint8_t ub_Data );
uint16_t Swi_ReceiveData( uint8_t * up_Byte , uint16_t rd_len);
uint16_t Swi_ReceiveRawWord( uint32_t * up_wData );
uint16_t Swi_ComputeParity(uint16_t uw_SwiCmd);
uint32_t Swi_AddInvertFlag(uint16_t uw_swi_cmdWParity);
void Swi_AbortIrq(void);
void Swi_WaitForIrq( Int_Status *bp_IrqDetected, uint8_t b_Immediate );
#if (ENABLE_SWI_SEARCH ==1)
uint16_t Swi_SearchUID96(uint8_t *ubp_dev_cnt, uint16_t *uwp_Dev_Addr);
#endif
void Swi_SelectByAddress(uint16_t uw_DeviceAddress);
uint16_t Swi_SelectByPuid(uint8_t *stp_DeviceToSelect);
void Swi_PowerUp(void);
void Swi_PowerDown(void);
void Swi_PowerDown_HardRst(void);
void Swi_PowerUp_HardRst(void);


#endif /* SWI_CRC_H_ */
