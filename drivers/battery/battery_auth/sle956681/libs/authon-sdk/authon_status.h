#ifndef _ON_STATUS_H_
#define _ON_STATUS_H_
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
 * @file   authon_status.h
 * @date   January, 2021
 * @brief  Authenticate On status and error codes
 */

#include "authon_config.h"

#define ON_SUCCESS (0x0000u) /*!< 0x0000 - Generic success status. */

#define ON_L_SDK (0xB000u) /*!< 0xB000 - SDK layer. */
#define ON_L_EXE (0xA000u) /*!< 0xA000 - Exe layer. */
#define ON_L_HA (0x9000u)  /*!< 0x9000 - Host Authentication layer. */
#define ON_L_ECC (0x8000u) /*!< 0x8000 - ECC layer. */
#define ON_L_LSC (0x6000u) /*!< 0x6000 - LSC layer. */
#define ON_L_NVM (0x5000u) /*!< 0x5000 - NVM layer. */
#define ON_L_CRC (0x4000u) /*!< 0x4000 - CRC layer. */
#define ON_L_SID (0x3000u) /*!< 0x3000 - SID layer. */
#define ON_L_SWI (0x1000u) /*!< 0x1000 - SWI layer. */

/**
 * @brief SDK layer status code.
 */
#define SDK_SUCCESS  (ON_SUCCESS) /*!< 0x0000 - SDK success. */
#define SDK_INTERFACE_SUCCESS (ON_SUCCESS)    /*!< 0x0000 - SDK interface communication success. */
#define SDK_E_INPUT   (ON_L_SDK + 0xFE) /*!< 0xB0FE - SDK input error. */
#define SDK_INIT (ON_L_SDK + 0xFF)    /*!< 0xB0FF - SDK status init. */

#define SDK_VERSION_NULL (ON_L_SDK + 0x01) /*!< 0xB001 - SDK version buffer is NULL. */
#define SDK_DATE_NULL (ON_L_SDK + 0x02)    /*!< 0xB002 - SDK date buffer is NULL. */
#define SDK_E_INTERFACE_UNDEFINED (ON_L_SDK + 0x04)    /*!< 0xB004 - SDK communication interface undefined. */
#define SDK_E_READSFR (ON_L_SDK + 0x05)       /*!< 0xB005 - SDK read SFR error */
#define SDK_E_WRITEADDRESS (ON_L_SDK + 0x06)  /*!< 0xB006 - SDK write address error */
#define SDK_E_CONFIG (ON_L_SDK + 0x07)     /*!< 0xB007 - Invalid configuration */
#define SDK_E_UID_NOT_ENUM (ON_L_SDK + 0x08)     /*!< 0xB008 - UID not enumerated */

#define SDK_E_HOST_AUTH (ON_L_SDK + 0x50)     /*!< 0xB050 - Host auth must be perform prior to this operation */


/**
 * @brief Exe layer status code.
 */
#define EXE_SUCCESS  (ON_SUCCESS) /*!< 0x0000 - Exe success. */
#define EXE_E_INPUT   (ON_L_EXE + 0xFE) /*!< 0xA0FE - Exe input error. */
#define EXE_INIT   (ON_L_EXE + 0xFF) /*!< 0xA0FF - Exe status init. */

#define EXE_READUIDFAILED (ON_L_EXE + 0x01) /*!< 0xA001 - Exe Read UID failed. */
#define EXE_E_POWERDOWN (ON_L_EXE + 0x03)    /*!< 0xA003 - Power down error */
#define EXE_E_NO_UID (ON_L_EXE + 0x04)    /*!< 0xA004 - UID not found in capability structure */
#define EXE_E_READ_ODC (ON_L_EXE + 0x05)    /*!< 0xA005 - Read ODC, Public key, Hash error */
#define EXE_E_ODC_VERIFY (ON_L_EXE + 0x06)    /*!< 0xA006 - Verify ODC error */
#define EXE_E_VIDPID (ON_L_EXE + 0x07)    /*!< 0xA007 - VIDPID error */

/**
 * @brief Host Auth layer status code.
 */
#define APP_HA_SUCCESS (ON_SUCCESS)        /*!< 0x0000 - Success */
#define APP_HA_E_INPUT   (ON_L_HA + 0xFE)        /*!< 0x90FE - HA input error. */
#define APP_HA_INIT (ON_L_HA + 0xFF)             /*!< 0x90FF - Init return code */

#define APP_HA_E_HRREQ (ON_L_HA + 0x02u)         /*!< 0x9002 - Send HRREQ failed */
#define APP_HA_E_HRRES (ON_L_HA + 0x03u)         /*!< 0x9003 - Send HRRES failed */
#define APP_HA_Get_NOUNCEA_FAILED (ON_L_HA + 0x04u)       /*!< 0x9004 - Get Nounce A failed */
#define APP_HA_MAC_BUSY (ON_L_HA + 0x05u)         /*!< 0x9005 - MAC_BUSY */
#define APP_HA_E_DRES1 (ON_L_HA + 0x06u)         /*!< 0x9006 - Get Device response 1 failed */
#define APP_HA_E_DRRES (ON_L_HA + 0x07u)         /*!< 0x9007 - DRRES failed */
#define APP_HA_E_HREQ1 (ON_L_HA + 0x08u)         /*!< 0x9008 - HREQ1 failed */
#define APP_HA_E_TAGA (ON_L_HA + 0x09u)         /*!< 0x9009 - Generate TagA failed */
#define APP_HA_E_TAGB (ON_L_HA + 0x0Au)      /*!< 0x900A - Verify tagB fail */

#define APP_HA_DISABLE (ON_L_HA + 0x20)    /*!< 0x9020 - Host Auth feature is disabled */
#define APP_HA_ENABLE (ON_L_HA + 0x21)    /*!< 0x9021 - Host Auth feature is enabled */

/**
 * @brief ECC status code
 */
#define APP_ECC_SUCCESS (ON_SUCCESS)          /*!< 0x0000 - Success */
#define APP_ECC_E_INPUT (ON_L_ECC + 0xFE)     /*!< 0x80FE - Input error */
#define APP_ECC_INIT (ON_L_ECC + 0xFF)     /*!< 0x80FF - Init return code */

#define APP_ECC_E_READ_UID (ON_L_ECC + 0x01u)      /*!< 0x8001 - Read UID error */
#define APP_ECC_E_READ_ODC (ON_L_ECC + 0x02u)      /*!< 0x8002 - Read ODC error */
#define APP_ECC_E_READ_PK (ON_L_ECC + 0x03u)       /*!< 0x8003 - Read ECC Public Key error */
#define APP_ECC_E_READ_HASH (ON_L_ECC + 0x04u)     /*!< 0x8004 - Read ECC Hash value error */
#define APP_ECC_E_VERIFY_ODC (ON_L_ECC + 0x06u)    /*!< 0x8006 - Verify ODC error */
#define APP_ECC_E_RANDOM (ON_L_ECC + 0x07u)        /*!< 0x8007 - Random number error */
#define APP_ECC_E_CHECKVALUE (ON_L_ECC + 0x08u)    /*!< 0x8008 - Generate Checkvalue error */
#define APP_ECC_E_SEND_CHG (ON_L_ECC + 0x09u)      /*!< 0x8009 - Send Challenge error */
#define APP_ECC_E_TIMEOUT (ON_L_ECC + 0x0Au)       /*!< 0x800A - ECC timeout */
#define APP_ECC_E_READ_RESP (ON_L_ECC + 0x0Bu)     /*!< 0x800B - Get response error */
#define APP_ECC_E_READ_RESP_X (ON_L_ECC + 0x0Cu)   /*!< 0x800C - Get Response X error */
#define APP_ECC_E_READ_RESP_Z (ON_L_ECC + 0x0Du)   /*!< 0x800D - Get Response Z error */
#define APP_ECC_E_READ_RESP_Z1 (ON_L_ECC + 0x0Eu)  /*!< 0x800E - Get Response Z last byte error */
#define APP_ECC_E_VERIFY_RESP (ON_L_ECC + 0x0Fu)   /*!< 0x800F - ECC Verify response error */
#define APP_ECC_E_NO_IRQ (ON_L_ECC + 0x10u)        /*!< 0x8010 - No IRQ received */
#define APP_ECC_E_GEN_CHALLENGE (ON_L_ECC + 0x11u) /*!< 0x8011 - Generate challenge fail */
#define APP_ECC_E_WRITE_CAP7 (ON_L_ECC + 0x12u)    /*!< 0x8012 - Write Reg CAP7 error */
#define APP_ECC_E_WRITE_INT0 (ON_L_ECC + 0x13u)    /*!< 0x8013 - Write Reg INT0 error */
#define APP_ECC_E_ECCC (ON_L_ECC + 0x14u)    /*!< 0x8014 - ECCC error */
#define APP_ECC_E_ECCR (ON_L_ECC + 0x15u)    /*!< 0x8015 - ECCR error */
#define APP_ECC_E_ECCS1 (ON_L_ECC + 0x16u)    /*!< 0x8016 - ECCS1 error */
#define APP_ECC_E_ECCS2 (ON_L_ECC + 0x17u)    /*!< 0x8017 - ECCS2 error */
#define APP_ECC_E_CHALLENGERESPONSE (ON_L_ECC + 0x18u)    /*!< 0x8018 - Send Challenge and Get Response error */
#define APP_ECC_E_ECCMACVERIFY (ON_L_ECC + 0x19u)    /*!< 0x8019 - ECC MAC verify failed */
#define APP_ECC_E_WAIT (ON_L_ECC + 0x20u)    /*!< 0x8020 - ECC MAC wait and timeout*/
#define APP_ECC_E_ECCMACVERIF_FAIL (ON_L_ECC + 0x21)    /*!< 0x8021 - ECCMACVerify failed */
#define APP_ECC_E_KILL_FAIL (ON_L_ECC + 0x22)      /*!< 0x8022 - Kill failed error */
#define APP_ECC_E_AUTHMAC_BUSY (ON_L_ECC + 0x23)  /*!< 0x8023 - Auth Mac Busy */
#define APP_ECC_E_KILL_SET (ON_L_ECC + 0x24)      /*!< 0x8024 - Kill Bit Set */
#define APP_ECC_AUTH_FAIL (ON_L_ECC + 0x25)      /*!< 0x8025 - ECC Auth failed */
#define APP_ECC_AUTHMAC_EINT_NOINT (ON_L_ECC + 0x26)      /*!< 0x8026 - ECC MAC no interrupt */
#define APP_ECC_E_RESPONSE_ALL_ZERO (ON_L_ECC + 0x27)      /*!< 0x8027 - ECC Response all zero */
#define APP_ECC_RESPONSE_NOT_ZERO (ON_L_ECC + 0x28)      /*!< 0x8028 - ECC Response not all zero */
#define APP_ECC_E_ECC_NOT_DONE (ON_L_ECC + 0x29)      /*!< 0x8029 - ECC not performed */
#define APP_ECC_E_MAC_FAILED (ON_L_ECC + 0x2A)      /*!< 0x802A - MAC on ECC kill error */
#define APP_ECC_E_CRC (ON_L_ECC + 0x30u)           /*!< 0x8030 - CRC Error Bit-1: PAGE */

#define APP_ECC_DISABLE_AUTOKILL (ON_L_ECC + 0x40)    /*!< 0x8040 - Auto Kill config feature is disabled */
#define APP_ECC_ENABLE_AUTOKILL (ON_L_ECC + 0x41)    /*!< 0x8041 - Auto Kill config feature is enabled */
#define APP_ECC_DISABLE_KILL (ON_L_ECC + 0x42)    /*!< 0x8042 - KILL config feature is disabled */
#define APP_ECC_ENABLE_KILL (ON_L_ECC + 0x43)    /*!< 0x8043 - Kill config feature is enabled */

//Special error code for killed device
#define APP_ECC_KILLED (0xDEAD)                  /*!< 0xDEAD - ECC Killed */
#define APP_ECC_NOTKILLED (ON_L_ECC + 0x51u)     /*!< 0x8051 - ECC not kill */
#define APP_ECC_SETMAC_OK (ON_L_ECC + 0x52u)     /*!< 0x8052 - Set MAC success */
#define APP_ECC_SETMAC_NOK (ON_L_ECC + 0x53u)    /*!< 0x8053 - Unable to set MAC */
#define APP_ECC_E_CRC_WRITE (ON_L_ECC + 0x54u)   /*!< 0x8054 - ECC Pass but CRC page writing fail */

/**
 * @brief LSC status code
 */
#define APP_LSC_SUCCESS (ON_SUCCESS)        /*!< 0x0000 - Success */
#define APP_LSC_E_INPUT (ON_L_LSC + 0xFE) /*!< 0x60FE - Input error */
#define APP_LSC_INIT (ON_L_LSC + 0xFF) /*!< 0x60FF - Init return code */

#define APP_LSC_E_ZERO (ON_L_LSC + 0x01u)        /*!< 0x6001 - Counter zero */
#define APP_LSC_E_READ (ON_L_LSC + 0x02u)        /*!< 0x6002 - Read error */
#define APP_LSC_E_TIMEOUT (ON_L_LSC + 0x03u)     /*!< 0x6003 - Timeout error */
#define APP_LSC_E_BUSY (ON_L_LSC + 0x04u)        /*!< 0x6004 - LSC busy error */
#define APP_LSC_E_READ_STATUS (ON_L_LSC + 0x05u) /*!< 0x6005 - LSC read status error */
#define APP_LSC_LOCKED (ON_L_LSC + 0x06u)   /*!< 0x6006 - LSC is locked */
#define APP_LSC_NOT_LOCKED (ON_L_LSC + 0x07u) /*!< 0x6007 - LSC is not locked */
#define APP_LSC_DISABLE_AUTODEC (ON_L_LSC + 0x08u) /*!< 0x6008 - Auto Dec LSC is disabled */
#define APP_LSC_ENABLE_AUTODEC (ON_L_LSC + 0x09u)  /*!< 0x6009 - Auto Dec LSC is enabled */
#define APP_LSC_EN_STATUS (ON_L_LSC + 0x0a)        /*!< 0x600a - LSC Enable is set */

#define APP_LSC_E_MACCR1   (ON_L_LSC + 0x0b)       /*!< 0x600b - MACCR1 failed */
#define APP_LSC_E_MACCR2   (ON_L_LSC + 0x0c)       /*!< 0x600c - MACCR2 failed */
#define APP_LSC_E_MACCR3   (ON_L_LSC + 0x0d)       /*!< 0x600d - MACCR3 failed */
#define APP_LSC_E_MACCR4   (ON_L_LSC + 0xe)        /*!< 0x600e- MACCR4 failed */

#define APP_LSC_DISABLE_RESET (ON_L_LSC + 0x20)    /*!< 0x6020 - LSC reset feature is disabled */
#define APP_LSC_ENABLE_RESET (ON_L_LSC + 0x21)    /*!< 0x6021 - LSC reset feature is enabled */


/**
 * @brief NVM status code
 */
#define APP_NVM_SUCCESS (ON_SUCCESS)             /*!< 0x0000 - Success */
#define APP_NVM_E_INPUT (ON_L_NVM + 0xFE)         /*!< 0x50FE - NVM Input error */
#define APP_NVM_INIT (ON_L_NVM + 0xFF)         /*!< 0x50FF - NVM Init status */

#define APP_NVM_E_READ (ON_L_NVM + 0x01u)             /*!< 0x5001 - NVM Read error */
#define APP_NVM_E_WRITE (ON_L_NVM + 0x02u)            /*!< 0x5002 - NVM Write error */
#define APP_NVM_E_TIMEOUT (ON_L_NVM + 0x03u)          /*!< 0x5003 - NVM timeout */
#define APP_NVM_E_BUSY (ON_L_NVM + 0x04u)             /*!< 0x5004 - NVM busy */
#define APP_NVM_E_READ_PAGE (ON_L_NVM + 0x05u)        /*!< 0x5005 - Read page error */
#define APP_NVM_E_ILLEGAL_PARA (ON_L_NVM + 0x06u)     /*!< 0x5006 - Illegal parameter */
#define APP_NVM_E_READ_STATUS (ON_L_NVM + 0x07u)      /*!< 0x5007 - Read NVM status error */
#define APP_NVM_E_PROG_PAGE (ON_L_NVM + 0x08u)        /*!< 0x5008 - NVM program page error */
#define APP_NVM_E_READ_DEVICEADDR (ON_L_NVM + 0x09u)  /*!< 0x5009 - Unable to read device address */
#define APP_NVM_E_WRITE_DEVICEADDR (ON_L_NVM + 0x0Au) /*!< 0x500A - Unable to write device address */
#define APP_NVM_E_DEVICEADDR (ON_L_NVM + 0x0Bu)       /*!< 0x500B - Invalid device address */
#define APP_NVM_E_LOCK_PAGE (ON_L_NVM + 0x0Cu)        /*!< 0x500C - Invalid NVM locked page */
#define APP_NVM_E_INVALID_PAGE (ON_L_NVM + 0x0Du)     /*!< 0x500D - Invalid NVM page number */
#define APP_NVM_E_READBACK (ON_L_NVM + 0x0Eu)         /*!< 0x500E - Write and read back verify error */
#define APP_NVM_E_READ_BURST (ON_L_NVM + 0x0Fu)       /*!< 0x500F - Read burst error */
#define APP_NVM_E_READ_KILL (ON_L_NVM + 0x10u)        /*!< 0x5010 - Read reading the kill status register */
#define APP_NVM_E_READ_CAP4 (ON_L_NVM + 0x11u)        /*!< 0x5011 - Read reading the capability4 register */
#define APP_NVM_E_READ_LOCK_STS (ON_L_NVM + 0x13u)    /*!< 0x5013 - Read reading the lock status register */
#define APP_NVM_DISABLE_UNLOCK (ON_L_NVM + 0x14)    /*!< 0x5014 - Disable unlock nvm register */
#define APP_NVM_E_MACCR5 (ON_L_NVM + 0x15)          /*!< 0x5015 - MACCR5 failed */
#define APP_NVM_E_LSC_ENDURANCE (ON_L_NVM + 0x16)   /*!< 0x5016 - Warning: LSC value exceed NVM write endurance */
#define APP_NVM_E_PAGE_UNLOCKED_FAILED (ON_L_NVM + 0x17u) /*!< 0x5017 - NVM Page unlock failed */

#define APP_NVM_DISABLE_RESET (ON_L_NVM + 0x20)    /*!< 0x5020 - NVM reset feature is disabled */
#define APP_NVM_ENABLE_RESET (ON_L_NVM + 0x21)    /*!< 0x5021 - NVM reset feature is enabled */
#define APP_NVM_DISABLE_CHIPLOCK (ON_L_NVM + 0x22)    /*!< 0x5022 - Chip lock feature is disabled */
#define APP_NVM_ENABLE_CHIPLOCK (ON_L_NVM + 0x23)    /*!< 0x5023 - Chip lock feature is enabled */

#define APP_NVM_READY (ON_L_NVM + 0xF0u)           /*!< 0x50F0 - NVM ready for access */
#define APP_NVM_PAGE_LOCKED (ON_L_NVM + 0xF1u)     /*!< 0x50F1 - NVM Page locked unable to write */
#define APP_NVM_PAGE_NOT_LOCKED (ON_L_NVM + 0xF2u) /*!< 0x50F2 - NVM Page not lock */

#define APP_NVM_DEV_KILLED (ON_L_NVM + 0xF3u)   /*!< 0x50F3 - Device is killed */
#define APP_NVM_DEV_NOT_KILL (ON_L_NVM + 0xF4u) /*!< 0x50F4 - Device cannot be killed */
#define APP_NVM_KILL_CAP (ON_L_NVM + 0xF5u)     /*!< 0x50F5 - Device can be killed */
#define APP_NVM_KILL_NOT_CAP (ON_L_NVM + 0xF6u) /*!< 0x50F6 - Device cannot not killed */

/**
 * @brief CRC status code
 */
#define APP_CRC_SUCCESS (ON_SUCCESS)    /*!< 0x0000 - Success */
#define APP_CRC_E_INPUT (ON_L_CRC + 0xFF)     /*!< 0x40FE - Input error code */
#define APP_CRC_INIT (ON_L_CRC + 0xFF)        /*!< 0x40FF - Init return code */

#define APP_CRC_E_NO_DATA (ON_L_CRC + 0x01u)         /*!< 0x4001 - No input Data */
#define APP_CRC_E_FAIL (ON_L_CRC + 0x02u)            /*!< 0x4002 - Crc check fail */
#define APP_CRC_E_READ (ON_L_CRC + 0x03u)            /*!< 0x4003 - Reading CRC error */
#define APP_CRC_E_READ_PAGELOCK (ON_L_CRC + 0x04u)   /*!< 0x4004 - Fail to get NVM page lock info */
#define APP_CRC_E_WRITE (ON_L_CRC + 0x05u)           /*!< 0x4005 - Write CRC page fail */
#define APP_CRC_E_CRC_PAGE_LOCKED (ON_L_CRC + 0x06u) /*!< 0x4006 - CRC page is locked */
#define APP_CRC_E_CRC_LOCK_FAIL (ON_L_CRC + 0x07u)   /*!< 0x4007 - CRC page lock fail */

/**
 * @brief Search and Select ID status code
 */
#define APP_SID_SUCCESS (ON_SUCCESS)        /*!< 0x0000 - Success */
#define APP_SID_E_INPUT (ON_L_SID + 0xFE) /*!< 0x30FE - Input error */
#define APP_SID_INIT (ON_L_SID + 0xFF) /*!< 0x30FF - Init return code */

#define APP_SID_E_STACK_EMPTY (ON_L_SID + 0x01u) /*!< 0x3001 - Stack empty */
#define APP_SID_E_STACK_FULL (ON_L_SID + 0x02u)  /*!< 0x3002 - Stack full */
#define APP_SID_E_NO_DEV (ON_L_SID + 0x03u)      /*!< 0x3003 - No device found */
#define APP_SID_E_SELECT (ON_L_SID + 0x04u)      /*!< 0x3004 - Unable to select device UID */
#define APP_SID_E_UID96 (ON_L_SID + 0x05u)       /*!< 0x3005 - UID is not 96 bits in length */
#define APP_SID_E_ADD_BRANCH (ON_L_SID + 0x06u)       /*!< 0x3006 - Add branch error */
#define APP_SID_E_SEARCH_DEV (ON_L_SID + 0x07u)       /*!< 0x3007 - Search device error */

/**
 * @brief SWI interface status code
 */
#define INF_SWI_SUCCESS (ON_SUCCESS)           /*!< 0x0000 - Success. */
#define INF_SWI_E_INPUT (ON_L_SWI + 0xFEu)           /*!< 0x10FE - SWI Input error */
#define INF_SWI_INIT (ON_L_SWI + 0xFF)               /*!< 0x10FF - Init Return code */

#define INF_SWI_E_TIMEOUT (ON_L_SWI + 0x01u)          /*!< 0x1001 - Receive time out */
#define INF_SWI_E_TRAINING (ON_L_SWI + 0x02u)         /*!< 0x1002 - Training error  */
#define INF_SWI_E_INV (ON_L_SWI + 0x03u)              /*!< 0x1003 - Invert bit error */
#define INF_SWI_E_FRAME (ON_L_SWI + 0x04u)            /*!< 0x1004 - Framing error */
#define INF_SWI_E_NO_BIT_SEL (ON_L_SWI + 0x05u)       /*!< 0x1005 - No bit selected for writing */
#define INF_SWI_E_READ_ODC_TIMEOUT (ON_L_SWI + 0x06u) /*!< 0x1006 - Read ODC timeout */
#define INF_SWI_E_READ_ODC_DATA (ON_L_SWI + 0x07u)    /*!< 0x1007 - Read ODC info error */
#define INF_SWI_E_BURST_TYPE (ON_L_SWI + 0x08u)       /*!< 0x1008 - Invalid burst type */
#define INF_SWI_E_READ_TYPE (ON_L_SWI + 0x09u)        /*!< 0x1009 - Invalid read type */
#define INF_SWI_E_WRITE_TYPE (ON_L_SWI + 0x0Au)       /*!< 0x100A - Invalid write type */
#define INF_SWI_E_TIMING_INIT (ON_L_SWI + 0x0Bu)      /*!< 0x100B - SWI timing initialized */
#define INF_SWI_E_SFRREAD  (ON_L_SWI + 0x0Cu)         /*!< 0x100C - SWI error reading SFR */
#define INF_SWI_E_RD_ACK (ON_L_SWI + 0x0Du)           /*!< 0x100D - Read Acknowledge error  */
#define INF_SWI_E_RD_PARITY (ON_L_SWI + 0x0Eu)        /*!< 0x100E - Read Parity error  */
#define INF_SWI_E_SET_ADDRESS (ON_L_SWI + 0x0Fu)      /*!< 0x100F - Set Address error  */
#define INF_SWI_E_RD_CRC (ON_L_SWI + 0x10u)        	  /*!< 0x1010 - Read CRC error  */

/**
 * @brief Crypto library status definition
 */
typedef enum {FALSE=0, TRUE=1} CRYPTO_LIB_RETCODE;
/**
 * @brief Interrupt status definition
 */
typedef enum INT_STATUS_{NO_INTERRUPT=0, GOT_INTERRUPT=1} Int_Status;
/**
 * @brief Lock status definition
 */
typedef enum LOCK_STATUS_{NOT_LOCK=0, LOCKED=1} Lock_Status;

#endif /*!< _ON_STATUS_H_ */
