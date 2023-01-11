/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */

#ifndef _CI_STUB_TTC_MACRO_H_
#define _CI_STUB_TTC_MACRO_H_

#define CiNullProcId                                                    0x00
#define CiShRegisterReqProcId                   0x01
#define CiShDeregisterReqProcId         0x02
#define CiShRequestProcId                                       0x03

#define CiRequestProcId                                         0x04
#define CiRespondProcId                                         0x05
#define CiStubRequestStartProcId                 0x06
#define CiDataStubRequestStartProcId                 0x07
#define CiDataStubReqDataProcId                 0x08

#define CiShConfirmCbId                                 0x0001
#define CiConfirmDefCbId                                0x0002
#define CiIndicateDefCbId                               0x0003
#define CiConfirmCbId                                           0x0004
#define CiIndicateCbId                                  0x0005
#define CiStubConfirmStartProcId                0x0006
#define CiDatStubConfirmStartProcId   0x0007
#define CiDataStubIndDataProcId         0x0008

#define MAX_CI_STUB_RXMSG_LEN  2048
#define CI_DAT_INTERNAL_BUFFER 0xA

#endif /* _CI_STUB_TTC_MACRO_H_ */
