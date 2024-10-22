/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __UARTHUB_UART0_REGS_MT6989_H__
#define __UARTHUB_UART0_REGS_MT6989_H__

#include "common_def_id.h"

#ifndef REG_BASE_C_MODULE
// ----------------- UARTHUB_UART0 Bit Field Definitions -------------------

#define RBR_ADDR(_baseaddr)                                    (_baseaddr + 0x00)
#define THR_ADDR(_baseaddr)                                    (_baseaddr + 0x00)
#define IER_ADDR(_baseaddr)                                    (_baseaddr + 0x04)
#define IIR_ADDR(_baseaddr)                                    (_baseaddr + 0x08)
#define FCR_ADDR(_baseaddr)                                    (_baseaddr + 0x08)
#define LCR_ADDR(_baseaddr)                                    (_baseaddr + 0x0C)
#define MCR_ADDR(_baseaddr)                                    (_baseaddr + 0x10)
#define LSR_ADDR(_baseaddr)                                    (_baseaddr + 0x14)
#define MSR_ADDR(_baseaddr)                                    (_baseaddr + 0x18)
#define SCR_ADDR(_baseaddr)                                    (_baseaddr + 0x1C)
#define AUTOBAUD_EN_ADDR(_baseaddr)                            (_baseaddr + 0x20)
#define HIGHSPEED_ADDR(_baseaddr)                              (_baseaddr + 0x24)
#define SAMPLE_COUNT_ADDR(_baseaddr)                           (_baseaddr + 0x28)
#define SAMPLE_POINT_ADDR(_baseaddr)                           (_baseaddr + 0x2C)
#define AUTOBAUD_REG_ADDR(_baseaddr)                           (_baseaddr + 0x30)
#define RATEFIX_AD_ADDR(_baseaddr)                             (_baseaddr + 0x34)
#define AUTOBAUDSAMPLE_ADDR(_baseaddr)                         (_baseaddr + 0x38)
#define GUARD_ADDR(_baseaddr)                                  (_baseaddr + 0x3C)
#define ESCAPE_DAT_ADDR(_baseaddr)                             (_baseaddr + 0x40)
#define ESCAPE_EN_ADDR(_baseaddr)                              (_baseaddr + 0x44)
#define SLEEP_EN_ADDR(_baseaddr)                               (_baseaddr + 0x48)
#define DMA_EN_ADDR(_baseaddr)                                 (_baseaddr + 0x4C)
#define RXTRI_AD_ADDR(_baseaddr)                               (_baseaddr + 0x50)
#define FRACDIV_L_ADDR(_baseaddr)                              (_baseaddr + 0x54)
#define FRACDIV_M_ADDR(_baseaddr)                              (_baseaddr + 0x58)
#define FCR_RD_ADDR(_baseaddr)                                 (_baseaddr + 0x5C)
#define RTO_CFG_ADDR(_baseaddr)                                (_baseaddr + 0x88)
#define DLL_ADDR(_baseaddr)                                    (_baseaddr + 0x90)
#define DLM_ADDR(_baseaddr)                                    (_baseaddr + 0x94)
#define EFR_ADDR(_baseaddr)                                    (_baseaddr + 0x98)
#define FEATURE_SEL_ADDR(_baseaddr)                            (_baseaddr + 0x9C)
#define XON1_ADDR(_baseaddr)                                   (_baseaddr + 0xA0)
#define XON2_ADDR(_baseaddr)                                   (_baseaddr + 0xA4)
#define XOFF1_ADDR(_baseaddr)                                  (_baseaddr + 0xA8)
#define XOFF2_ADDR(_baseaddr)                                  (_baseaddr + 0xAC)
#define USB_RX_SEL_ADDR(_baseaddr)                             (_baseaddr + 0xB0)
#define SLEEP_REQ_ADDR(_baseaddr)                              (_baseaddr + 0xB4)
#define SLEEP_ACK_ADDR(_baseaddr)                              (_baseaddr + 0xB8)
#define SPM_SEL_ADDR(_baseaddr)                                (_baseaddr + 0xBC)
#define INB_ESC_CHAR_ADDR(_baseaddr)                           (_baseaddr + 0xC0)
#define INB_STA_CHAR_ADDR(_baseaddr)                           (_baseaddr + 0xC4)
#define INB_IRQ_CTL_ADDR(_baseaddr)                            (_baseaddr + 0xC8)
#define INB_STA_ADDR(_baseaddr)                                (_baseaddr + 0xCC)


#endif


#define RBR_FLD_RBR                                            REG_FLD(8, 0)

#define THR_FLD_THR                                            REG_FLD(8, 0)

#define IER_FLD_CTSI                                           REG_FLD(1, 7)
#define IER_FLD_RTSI                                           REG_FLD(1, 6)
#define IER_FLD_XOFFI                                          REG_FLD(1, 5)
#define IER_FLD_EDSSI                                          REG_FLD(1, 3)
#define IER_FLD_ELSI                                           REG_FLD(1, 2)
#define IER_FLD_ETBEI                                          REG_FLD(1, 1)
#define IER_FLD_ERBFI                                          REG_FLD(1, 0)

#define IIR_FLD_FIFOE                                          REG_FLD(2, 6)
#define IIR_FLD_ID                                             REG_FLD(6, 0)

#define FCR_FLD_RFTL1_RFTL0                                    REG_FLD(2, 6)
#define FCR_FLD_TFTL1_TFTL0                                    REG_FLD(2, 4)
#define FCR_FLD_CLRT                                           REG_FLD(1, 2)
#define FCR_FLD_CLRR                                           REG_FLD(1, 1)
#define FCR_FLD_FIFOE                                          REG_FLD(1, 0)

#define LCR_FLD_DLAB                                           REG_FLD(1, 7)
#define LCR_FLD_SB                                             REG_FLD(1, 6)
#define LCR_FLD_SP                                             REG_FLD(1, 5)
#define LCR_FLD_EPS                                            REG_FLD(1, 4)
#define LCR_FLD_PEN                                            REG_FLD(1, 3)
#define LCR_FLD_STB                                            REG_FLD(1, 2)
#define LCR_FLD_WLS1_WLS0                                      REG_FLD(2, 0)

#define MCR_FLD_XOFF_STATUS                                    REG_FLD(1, 7)
#define MCR_FLD_Loop                                           REG_FLD(1, 4)
#define MCR_FLD_RTS                                            REG_FLD(1, 1)

#define LSR_FLD_FIFOERR                                        REG_FLD(1, 7)
#define LSR_FLD_TEMT                                           REG_FLD(1, 6)
#define LSR_FLD_THRE                                           REG_FLD(1, 5)
#define LSR_FLD_BI                                             REG_FLD(1, 4)
#define LSR_FLD_FE                                             REG_FLD(1, 3)
#define LSR_FLD_PE                                             REG_FLD(1, 2)
#define LSR_FLD_OE                                             REG_FLD(1, 1)
#define LSR_FLD_DR                                             REG_FLD(1, 0)

#define MSR_FLD_CTS                                            REG_FLD(1, 4)
#define MSR_FLD_DCTS                                           REG_FLD(1, 0)

#define SCR_FLD_SCR                                            REG_FLD(8, 0)

#define AUTOBAUD_EN_FLD_AUTOBAUD_SEL                           REG_FLD(1, 1)
#define AUTOBAUD_EN_FLD_AUTOBAUD_EN                            REG_FLD(1, 0)

#define HIGHSPEED_FLD_SPEED                                    REG_FLD(2, 0)

#define SAMPLE_COUNT_FLD_SAMPLECOUNT                           REG_FLD(8, 0)

#define SAMPLE_POINT_FLD_SAMPLEPOINT                           REG_FLD(8, 0)

#define AUTOBAUD_REG_FLD_BAUD_STAT                             REG_FLD(4, 4)
#define AUTOBAUD_REG_FLD_BAUD_RATE                             REG_FLD(4, 0)

#define RATEFIX_AD_FLD_FREQ_SEL                                REG_FLD(1, 2)
#define RATEFIX_AD_FLD_AUTOBAUD_RATE_FIX                       REG_FLD(1, 1)
#define RATEFIX_AD_FLD_RATE_FIX                                REG_FLD(1, 0)

#define AUTOBAUDSAMPLE_FLD_AUTOBAUDSAMPLE                      REG_FLD(6, 0)

#define GUARD_FLD_GUARD_EN                                     REG_FLD(1, 4)
#define GUARD_FLD_GUARD_CNT                                    REG_FLD(4, 0)

#define ESCAPE_DAT_FLD_ESCAPE_DAT                              REG_FLD(8, 0)

#define ESCAPE_EN_FLD_ESC_EN                                   REG_FLD(1, 0)

#define SLEEP_EN_FLD_SLEEP_EN                                  REG_FLD(1, 0)

#define DMA_EN_FLD_FIFO_lsr_sel                                REG_FLD(1, 3)
#define DMA_EN_FLD_TO_CNT_AUTORST                              REG_FLD(1, 2)
#define DMA_EN_FLD_TX_DMA_EN                                   REG_FLD(1, 1)
#define DMA_EN_FLD_RX_DMA_EN                                   REG_FLD(1, 0)

#define RXTRI_AD_FLD_RXTRIG                                    REG_FLD(4, 0)

#define FRACDIV_L_FLD_FRACDIV_L                                REG_FLD(8, 0)

#define FRACDIV_M_FLD_FRACDIV_M                                REG_FLD(2, 0)

#define FCR_RD_FLD_RFTL1_RFTL0                                 REG_FLD(2, 6)
#define FCR_RD_FLD_TFTL1_TFTL0                                 REG_FLD(2, 4)
#define FCR_RD_FLD_CLRT                                        REG_FLD(1, 2)
#define FCR_RD_FLD_CLRR                                        REG_FLD(1, 1)
#define FCR_RD_FLD_FIFOE                                       REG_FLD(1, 0)

#define RTO_CFG_FLD_RTO_SEL                                    REG_FLD(1, 6)
#define RTO_CFG_FLD_RTO_LENGTH                                 REG_FLD(6, 0)

#define DLL_FLD_DLL                                            REG_FLD(8, 0)

#define DLM_FLD_DLM                                            REG_FLD(8, 0)

#define EFR_FLD_AUTO_CTS                                       REG_FLD(1, 7)
#define EFR_FLD_AUTO_RTS                                       REG_FLD(1, 6)
#define EFR_FLD_ENABLE_E                                       REG_FLD(1, 4)
#define EFR_FLD_SW_FLOW_CONT                                   REG_FLD(4, 0)

#define FEATURE_SEL_FLD_FEATURE_SEL                            REG_FLD(1, 0)

#define XON1_FLD_XON1                                          REG_FLD(8, 0)

#define XON2_FLD_XON2                                          REG_FLD(8, 0)

#define XOFF1_FLD_XOFF1                                        REG_FLD(8, 0)

#define XOFF2_FLD_XOFF2                                        REG_FLD(8, 0)

#define USB_RX_SEL_FLD_USB_RX_SEL                              REG_FLD(1, 0)

#define SLEEP_REQ_FLD_SLEEP_REQ                                REG_FLD(1, 0)

#define SLEEP_ACK_FLD_SLEEP_ACK                                REG_FLD(1, 0)

#define SPM_SEL_FLD_SPM_SEL                                    REG_FLD(1, 0)

#define INB_ESC_CHAR_FLD_INB_ESC_CHAR                          REG_FLD(8, 0)

#define INB_STA_CHAR_FLD_INB_STA_CHAR                          REG_FLD(8, 0)

#define INB_IRQ_CTL_FLD_INB_IRQ_IND                            REG_FLD(1, 7)
#define INB_IRQ_CTL_FLD_INB_TX_COMP                            REG_FLD(1, 6)
#define INB_IRQ_CTL_FLD_INB_STA_CLR                            REG_FLD(1, 4)
#define INB_IRQ_CTL_FLD_INB_IRQ_CLR                            REG_FLD(1, 3)
#define INB_IRQ_CTL_FLD_INB_TRIG                               REG_FLD(1, 2)
#define INB_IRQ_CTL_FLD_INB_IRQ_EN                             REG_FLD(1, 1)
#define INB_IRQ_CTL_FLD_INB_EN                                 REG_FLD(1, 0)

#define INB_STA_FLD_INB_STA                                    REG_FLD(8, 0)

#define RBR_GET_RBR(reg32)                                     REG_FLD_GET(RBR_FLD_RBR, (reg32))

#define THR_GET_THR(reg32)                                     REG_FLD_GET(THR_FLD_THR, (reg32))

#define IER_GET_CTSI(reg32)                                    REG_FLD_GET(IER_FLD_CTSI, (reg32))
#define IER_GET_RTSI(reg32)                                    REG_FLD_GET(IER_FLD_RTSI, (reg32))
#define IER_GET_XOFFI(reg32)                                   REG_FLD_GET(IER_FLD_XOFFI, (reg32))
#define IER_GET_EDSSI(reg32)                                   REG_FLD_GET(IER_FLD_EDSSI, (reg32))
#define IER_GET_ELSI(reg32)                                    REG_FLD_GET(IER_FLD_ELSI, (reg32))
#define IER_GET_ETBEI(reg32)                                   REG_FLD_GET(IER_FLD_ETBEI, (reg32))
#define IER_GET_ERBFI(reg32)                                   REG_FLD_GET(IER_FLD_ERBFI, (reg32))

#define IIR_GET_FIFOE(reg32)                                   REG_FLD_GET(IIR_FLD_FIFOE, (reg32))
#define IIR_GET_ID(reg32)                                      REG_FLD_GET(IIR_FLD_ID, (reg32))

#define FCR_GET_RFTL1_RFTL0(reg32)                             REG_FLD_GET(FCR_FLD_RFTL1_RFTL0, (reg32))
#define FCR_GET_TFTL1_TFTL0(reg32)                             REG_FLD_GET(FCR_FLD_TFTL1_TFTL0, (reg32))
#define FCR_GET_CLRT(reg32)                                    REG_FLD_GET(FCR_FLD_CLRT, (reg32))
#define FCR_GET_CLRR(reg32)                                    REG_FLD_GET(FCR_FLD_CLRR, (reg32))
#define FCR_GET_FIFOE(reg32)                                   REG_FLD_GET(FCR_FLD_FIFOE, (reg32))

#define LCR_GET_DLAB(reg32)                                    REG_FLD_GET(LCR_FLD_DLAB, (reg32))
#define LCR_GET_SB(reg32)                                      REG_FLD_GET(LCR_FLD_SB, (reg32))
#define LCR_GET_SP(reg32)                                      REG_FLD_GET(LCR_FLD_SP, (reg32))
#define LCR_GET_EPS(reg32)                                     REG_FLD_GET(LCR_FLD_EPS, (reg32))
#define LCR_GET_PEN(reg32)                                     REG_FLD_GET(LCR_FLD_PEN, (reg32))
#define LCR_GET_STB(reg32)                                     REG_FLD_GET(LCR_FLD_STB, (reg32))
#define LCR_GET_WLS1_WLS0(reg32)                               REG_FLD_GET(LCR_FLD_WLS1_WLS0, (reg32))

#define MCR_GET_XOFF_STATUS(reg32)                             REG_FLD_GET(MCR_FLD_XOFF_STATUS, (reg32))
#define MCR_GET_Loop(reg32)                                    REG_FLD_GET(MCR_FLD_Loop, (reg32))
#define MCR_GET_RTS(reg32)                                     REG_FLD_GET(MCR_FLD_RTS, (reg32))

#define LSR_GET_FIFOERR(reg32)                                 REG_FLD_GET(LSR_FLD_FIFOERR, (reg32))
#define LSR_GET_TEMT(reg32)                                    REG_FLD_GET(LSR_FLD_TEMT, (reg32))
#define LSR_GET_THRE(reg32)                                    REG_FLD_GET(LSR_FLD_THRE, (reg32))
#define LSR_GET_BI(reg32)                                      REG_FLD_GET(LSR_FLD_BI, (reg32))
#define LSR_GET_FE(reg32)                                      REG_FLD_GET(LSR_FLD_FE, (reg32))
#define LSR_GET_PE(reg32)                                      REG_FLD_GET(LSR_FLD_PE, (reg32))
#define LSR_GET_OE(reg32)                                      REG_FLD_GET(LSR_FLD_OE, (reg32))
#define LSR_GET_DR(reg32)                                      REG_FLD_GET(LSR_FLD_DR, (reg32))

#define MSR_GET_CTS(reg32)                                     REG_FLD_GET(MSR_FLD_CTS, (reg32))
#define MSR_GET_DCTS(reg32)                                    REG_FLD_GET(MSR_FLD_DCTS, (reg32))

#define SCR_GET_SCR(reg32)                                     REG_FLD_GET(SCR_FLD_SCR, (reg32))

#define AUTOBAUD_EN_GET_AUTOBAUD_SEL(reg32)                    REG_FLD_GET(AUTOBAUD_EN_FLD_AUTOBAUD_SEL, (reg32))
#define AUTOBAUD_EN_GET_AUTOBAUD_EN(reg32)                     REG_FLD_GET(AUTOBAUD_EN_FLD_AUTOBAUD_EN, (reg32))

#define HIGHSPEED_GET_SPEED(reg32)                             REG_FLD_GET(HIGHSPEED_FLD_SPEED, (reg32))

#define SAMPLE_COUNT_GET_SAMPLECOUNT(reg32)                    REG_FLD_GET(SAMPLE_COUNT_FLD_SAMPLECOUNT, (reg32))

#define SAMPLE_POINT_GET_SAMPLEPOINT(reg32)                    REG_FLD_GET(SAMPLE_POINT_FLD_SAMPLEPOINT, (reg32))

#define AUTOBAUD_REG_GET_BAUD_STAT(reg32)                      REG_FLD_GET(AUTOBAUD_REG_FLD_BAUD_STAT, (reg32))
#define AUTOBAUD_REG_GET_BAUD_RATE(reg32)                      REG_FLD_GET(AUTOBAUD_REG_FLD_BAUD_RATE, (reg32))

#define RATEFIX_AD_GET_FREQ_SEL(reg32)                         REG_FLD_GET(RATEFIX_AD_FLD_FREQ_SEL, (reg32))
#define RATEFIX_AD_GET_AUTOBAUD_RATE_FIX(reg32)                REG_FLD_GET(RATEFIX_AD_FLD_AUTOBAUD_RATE_FIX, (reg32))
#define RATEFIX_AD_GET_RATE_FIX(reg32)                         REG_FLD_GET(RATEFIX_AD_FLD_RATE_FIX, (reg32))

#define AUTOBAUDSAMPLE_GET_AUTOBAUDSAMPLE(reg32)               REG_FLD_GET(AUTOBAUDSAMPLE_FLD_AUTOBAUDSAMPLE, (reg32))

#define GUARD_GET_GUARD_EN(reg32)                              REG_FLD_GET(GUARD_FLD_GUARD_EN, (reg32))
#define GUARD_GET_GUARD_CNT(reg32)                             REG_FLD_GET(GUARD_FLD_GUARD_CNT, (reg32))

#define ESCAPE_DAT_GET_ESCAPE_DAT(reg32)                       REG_FLD_GET(ESCAPE_DAT_FLD_ESCAPE_DAT, (reg32))

#define ESCAPE_EN_GET_ESC_EN(reg32)                            REG_FLD_GET(ESCAPE_EN_FLD_ESC_EN, (reg32))

#define SLEEP_EN_GET_SLEEP_EN(reg32)                           REG_FLD_GET(SLEEP_EN_FLD_SLEEP_EN, (reg32))

#define DMA_EN_GET_FIFO_lsr_sel(reg32)                         REG_FLD_GET(DMA_EN_FLD_FIFO_lsr_sel, (reg32))
#define DMA_EN_GET_TO_CNT_AUTORST(reg32)                       REG_FLD_GET(DMA_EN_FLD_TO_CNT_AUTORST, (reg32))
#define DMA_EN_GET_TX_DMA_EN(reg32)                            REG_FLD_GET(DMA_EN_FLD_TX_DMA_EN, (reg32))
#define DMA_EN_GET_RX_DMA_EN(reg32)                            REG_FLD_GET(DMA_EN_FLD_RX_DMA_EN, (reg32))

#define RXTRI_AD_GET_RXTRIG(reg32)                             REG_FLD_GET(RXTRI_AD_FLD_RXTRIG, (reg32))

#define FRACDIV_L_GET_FRACDIV_L(reg32)                         REG_FLD_GET(FRACDIV_L_FLD_FRACDIV_L, (reg32))

#define FRACDIV_M_GET_FRACDIV_M(reg32)                         REG_FLD_GET(FRACDIV_M_FLD_FRACDIV_M, (reg32))

#define FCR_RD_GET_RFTL1_RFTL0(reg32)                          REG_FLD_GET(FCR_RD_FLD_RFTL1_RFTL0, (reg32))
#define FCR_RD_GET_TFTL1_TFTL0(reg32)                          REG_FLD_GET(FCR_RD_FLD_TFTL1_TFTL0, (reg32))
#define FCR_RD_GET_CLRT(reg32)                                 REG_FLD_GET(FCR_RD_FLD_CLRT, (reg32))
#define FCR_RD_GET_CLRR(reg32)                                 REG_FLD_GET(FCR_RD_FLD_CLRR, (reg32))
#define FCR_RD_GET_FIFOE(reg32)                                REG_FLD_GET(FCR_RD_FLD_FIFOE, (reg32))

#define RTO_CFG_GET_RTO_SEL(reg32)                             REG_FLD_GET(RTO_CFG_FLD_RTO_SEL, (reg32))
#define RTO_CFG_GET_RTO_LENGTH(reg32)                          REG_FLD_GET(RTO_CFG_FLD_RTO_LENGTH, (reg32))

#define DLL_GET_DLL(reg32)                                     REG_FLD_GET(DLL_FLD_DLL, (reg32))

#define DLM_GET_DLM(reg32)                                     REG_FLD_GET(DLM_FLD_DLM, (reg32))

#define EFR_GET_AUTO_CTS(reg32)                                REG_FLD_GET(EFR_FLD_AUTO_CTS, (reg32))
#define EFR_GET_AUTO_RTS(reg32)                                REG_FLD_GET(EFR_FLD_AUTO_RTS, (reg32))
#define EFR_GET_ENABLE_E(reg32)                                REG_FLD_GET(EFR_FLD_ENABLE_E, (reg32))
#define EFR_GET_SW_FLOW_CONT(reg32)                            REG_FLD_GET(EFR_FLD_SW_FLOW_CONT, (reg32))

#define FEATURE_SEL_GET_FEATURE_SEL(reg32)                     REG_FLD_GET(FEATURE_SEL_FLD_FEATURE_SEL, (reg32))

#define XON1_GET_XON1(reg32)                                   REG_FLD_GET(XON1_FLD_XON1, (reg32))

#define XON2_GET_XON2(reg32)                                   REG_FLD_GET(XON2_FLD_XON2, (reg32))

#define XOFF1_GET_XOFF1(reg32)                                 REG_FLD_GET(XOFF1_FLD_XOFF1, (reg32))

#define XOFF2_GET_XOFF2(reg32)                                 REG_FLD_GET(XOFF2_FLD_XOFF2, (reg32))

#define USB_RX_SEL_GET_USB_RX_SEL(reg32)                       REG_FLD_GET(USB_RX_SEL_FLD_USB_RX_SEL, (reg32))

#define SLEEP_REQ_GET_SLEEP_REQ(reg32)                         REG_FLD_GET(SLEEP_REQ_FLD_SLEEP_REQ, (reg32))

#define SLEEP_ACK_GET_SLEEP_ACK(reg32)                         REG_FLD_GET(SLEEP_ACK_FLD_SLEEP_ACK, (reg32))

#define SPM_SEL_GET_SPM_SEL(reg32)                             REG_FLD_GET(SPM_SEL_FLD_SPM_SEL, (reg32))

#define INB_ESC_CHAR_GET_INB_ESC_CHAR(reg32)                   REG_FLD_GET(INB_ESC_CHAR_FLD_INB_ESC_CHAR, (reg32))

#define INB_STA_CHAR_GET_INB_STA_CHAR(reg32)                   REG_FLD_GET(INB_STA_CHAR_FLD_INB_STA_CHAR, (reg32))

#define INB_IRQ_CTL_GET_INB_IRQ_IND(reg32)                     REG_FLD_GET(INB_IRQ_CTL_FLD_INB_IRQ_IND, (reg32))
#define INB_IRQ_CTL_GET_INB_TX_COMP(reg32)                     REG_FLD_GET(INB_IRQ_CTL_FLD_INB_TX_COMP, (reg32))
#define INB_IRQ_CTL_GET_INB_STA_CLR(reg32)                     REG_FLD_GET(INB_IRQ_CTL_FLD_INB_STA_CLR, (reg32))
#define INB_IRQ_CTL_GET_INB_IRQ_CLR(reg32)                     REG_FLD_GET(INB_IRQ_CTL_FLD_INB_IRQ_CLR, (reg32))
#define INB_IRQ_CTL_GET_INB_TRIG(reg32)                        REG_FLD_GET(INB_IRQ_CTL_FLD_INB_TRIG, (reg32))
#define INB_IRQ_CTL_GET_INB_IRQ_EN(reg32)                      REG_FLD_GET(INB_IRQ_CTL_FLD_INB_IRQ_EN, (reg32))
#define INB_IRQ_CTL_GET_INB_EN(reg32)                          REG_FLD_GET(INB_IRQ_CTL_FLD_INB_EN, (reg32))

#define INB_STA_GET_INB_STA(reg32)                             REG_FLD_GET(INB_STA_FLD_INB_STA, (reg32))

#define RBR_SET_RBR(reg32, val)                                REG_FLD_SET(RBR_FLD_RBR, (reg32), (val))

#define THR_SET_THR(reg32, val)                                REG_FLD_SET(THR_FLD_THR, (reg32), (val))

#define IER_SET_CTSI(reg32, val)                               REG_FLD_SET(IER_FLD_CTSI, (reg32), (val))
#define IER_SET_RTSI(reg32, val)                               REG_FLD_SET(IER_FLD_RTSI, (reg32), (val))
#define IER_SET_XOFFI(reg32, val)                              REG_FLD_SET(IER_FLD_XOFFI, (reg32), (val))
#define IER_SET_EDSSI(reg32, val)                              REG_FLD_SET(IER_FLD_EDSSI, (reg32), (val))
#define IER_SET_ELSI(reg32, val)                               REG_FLD_SET(IER_FLD_ELSI, (reg32), (val))
#define IER_SET_ETBEI(reg32, val)                              REG_FLD_SET(IER_FLD_ETBEI, (reg32), (val))
#define IER_SET_ERBFI(reg32, val)                              REG_FLD_SET(IER_FLD_ERBFI, (reg32), (val))

#define IIR_SET_FIFOE(reg32, val)                              REG_FLD_SET(IIR_FLD_FIFOE, (reg32), (val))
#define IIR_SET_ID(reg32, val)                                 REG_FLD_SET(IIR_FLD_ID, (reg32), (val))

#define FCR_SET_RFTL1_RFTL0(reg32, val)                        REG_FLD_SET(FCR_FLD_RFTL1_RFTL0, (reg32), (val))
#define FCR_SET_TFTL1_TFTL0(reg32, val)                        REG_FLD_SET(FCR_FLD_TFTL1_TFTL0, (reg32), (val))
#define FCR_SET_CLRT(reg32, val)                               REG_FLD_SET(FCR_FLD_CLRT, (reg32), (val))
#define FCR_SET_CLRR(reg32, val)                               REG_FLD_SET(FCR_FLD_CLRR, (reg32), (val))
#define FCR_SET_FIFOE(reg32, val)                              REG_FLD_SET(FCR_FLD_FIFOE, (reg32), (val))

#define LCR_SET_DLAB(reg32, val)                               REG_FLD_SET(LCR_FLD_DLAB, (reg32), (val))
#define LCR_SET_SB(reg32, val)                                 REG_FLD_SET(LCR_FLD_SB, (reg32), (val))
#define LCR_SET_SP(reg32, val)                                 REG_FLD_SET(LCR_FLD_SP, (reg32), (val))
#define LCR_SET_EPS(reg32, val)                                REG_FLD_SET(LCR_FLD_EPS, (reg32), (val))
#define LCR_SET_PEN(reg32, val)                                REG_FLD_SET(LCR_FLD_PEN, (reg32), (val))
#define LCR_SET_STB(reg32, val)                                REG_FLD_SET(LCR_FLD_STB, (reg32), (val))
#define LCR_SET_WLS1_WLS0(reg32, val)                          REG_FLD_SET(LCR_FLD_WLS1_WLS0, (reg32), (val))

#define MCR_SET_XOFF_STATUS(reg32, val)                        REG_FLD_SET(MCR_FLD_XOFF_STATUS, (reg32), (val))
#define MCR_SET_Loop(reg32, val)                               REG_FLD_SET(MCR_FLD_Loop, (reg32), (val))
#define MCR_SET_RTS(reg32, val)                                REG_FLD_SET(MCR_FLD_RTS, (reg32), (val))

#define LSR_SET_FIFOERR(reg32, val)                            REG_FLD_SET(LSR_FLD_FIFOERR, (reg32), (val))
#define LSR_SET_TEMT(reg32, val)                               REG_FLD_SET(LSR_FLD_TEMT, (reg32), (val))
#define LSR_SET_THRE(reg32, val)                               REG_FLD_SET(LSR_FLD_THRE, (reg32), (val))
#define LSR_SET_BI(reg32, val)                                 REG_FLD_SET(LSR_FLD_BI, (reg32), (val))
#define LSR_SET_FE(reg32, val)                                 REG_FLD_SET(LSR_FLD_FE, (reg32), (val))
#define LSR_SET_PE(reg32, val)                                 REG_FLD_SET(LSR_FLD_PE, (reg32), (val))
#define LSR_SET_OE(reg32, val)                                 REG_FLD_SET(LSR_FLD_OE, (reg32), (val))
#define LSR_SET_DR(reg32, val)                                 REG_FLD_SET(LSR_FLD_DR, (reg32), (val))

#define MSR_SET_CTS(reg32, val)                                REG_FLD_SET(MSR_FLD_CTS, (reg32), (val))
#define MSR_SET_DCTS(reg32, val)                               REG_FLD_SET(MSR_FLD_DCTS, (reg32), (val))

#define SCR_SET_SCR(reg32, val)                                REG_FLD_SET(SCR_FLD_SCR, (reg32), (val))

#define AUTOBAUD_EN_SET_AUTOBAUD_SEL(reg32, val)               REG_FLD_SET(AUTOBAUD_EN_FLD_AUTOBAUD_SEL, (reg32), (val))
#define AUTOBAUD_EN_SET_AUTOBAUD_EN(reg32, val)                REG_FLD_SET(AUTOBAUD_EN_FLD_AUTOBAUD_EN, (reg32), (val))

#define HIGHSPEED_SET_SPEED(reg32, val)                        REG_FLD_SET(HIGHSPEED_FLD_SPEED, (reg32), (val))

#define SAMPLE_COUNT_SET_SAMPLECOUNT(reg32, val)               REG_FLD_SET(SAMPLE_COUNT_FLD_SAMPLECOUNT, (reg32), (val))

#define SAMPLE_POINT_SET_SAMPLEPOINT(reg32, val)               REG_FLD_SET(SAMPLE_POINT_FLD_SAMPLEPOINT, (reg32), (val))

#define AUTOBAUD_REG_SET_BAUD_STAT(reg32, val)                 REG_FLD_SET(AUTOBAUD_REG_FLD_BAUD_STAT, (reg32), (val))
#define AUTOBAUD_REG_SET_BAUD_RATE(reg32, val)                 REG_FLD_SET(AUTOBAUD_REG_FLD_BAUD_RATE, (reg32), (val))

#define RATEFIX_AD_SET_FREQ_SEL(reg32, val)                    REG_FLD_SET(RATEFIX_AD_FLD_FREQ_SEL, (reg32), (val))
#define RATEFIX_AD_SET_AUTOBAUD_RATE_FIX(reg32, val)           REG_FLD_SET(RATEFIX_AD_FLD_AUTOBAUD_RATE_FIX, (reg32), (val))
#define RATEFIX_AD_SET_RATE_FIX(reg32, val)                    REG_FLD_SET(RATEFIX_AD_FLD_RATE_FIX, (reg32), (val))

#define AUTOBAUDSAMPLE_SET_AUTOBAUDSAMPLE(reg32, val)          REG_FLD_SET(AUTOBAUDSAMPLE_FLD_AUTOBAUDSAMPLE, (reg32), (val))

#define GUARD_SET_GUARD_EN(reg32, val)                         REG_FLD_SET(GUARD_FLD_GUARD_EN, (reg32), (val))
#define GUARD_SET_GUARD_CNT(reg32, val)                        REG_FLD_SET(GUARD_FLD_GUARD_CNT, (reg32), (val))

#define ESCAPE_DAT_SET_ESCAPE_DAT(reg32, val)                  REG_FLD_SET(ESCAPE_DAT_FLD_ESCAPE_DAT, (reg32), (val))

#define ESCAPE_EN_SET_ESC_EN(reg32, val)                       REG_FLD_SET(ESCAPE_EN_FLD_ESC_EN, (reg32), (val))

#define SLEEP_EN_SET_SLEEP_EN(reg32, val)                      REG_FLD_SET(SLEEP_EN_FLD_SLEEP_EN, (reg32), (val))

#define DMA_EN_SET_FIFO_lsr_sel(reg32, val)                    REG_FLD_SET(DMA_EN_FLD_FIFO_lsr_sel, (reg32), (val))
#define DMA_EN_SET_TO_CNT_AUTORST(reg32, val)                  REG_FLD_SET(DMA_EN_FLD_TO_CNT_AUTORST, (reg32), (val))
#define DMA_EN_SET_TX_DMA_EN(reg32, val)                       REG_FLD_SET(DMA_EN_FLD_TX_DMA_EN, (reg32), (val))
#define DMA_EN_SET_RX_DMA_EN(reg32, val)                       REG_FLD_SET(DMA_EN_FLD_RX_DMA_EN, (reg32), (val))

#define RXTRI_AD_SET_RXTRIG(reg32, val)                        REG_FLD_SET(RXTRI_AD_FLD_RXTRIG, (reg32), (val))

#define FRACDIV_L_SET_FRACDIV_L(reg32, val)                    REG_FLD_SET(FRACDIV_L_FLD_FRACDIV_L, (reg32), (val))

#define FRACDIV_M_SET_FRACDIV_M(reg32, val)                    REG_FLD_SET(FRACDIV_M_FLD_FRACDIV_M, (reg32), (val))

#define FCR_RD_SET_RFTL1_RFTL0(reg32, val)                     REG_FLD_SET(FCR_RD_FLD_RFTL1_RFTL0, (reg32), (val))
#define FCR_RD_SET_TFTL1_TFTL0(reg32, val)                     REG_FLD_SET(FCR_RD_FLD_TFTL1_TFTL0, (reg32), (val))
#define FCR_RD_SET_CLRT(reg32, val)                            REG_FLD_SET(FCR_RD_FLD_CLRT, (reg32), (val))
#define FCR_RD_SET_CLRR(reg32, val)                            REG_FLD_SET(FCR_RD_FLD_CLRR, (reg32), (val))
#define FCR_RD_SET_FIFOE(reg32, val)                           REG_FLD_SET(FCR_RD_FLD_FIFOE, (reg32), (val))

#define RTO_CFG_SET_RTO_SEL(reg32, val)                        REG_FLD_SET(RTO_CFG_FLD_RTO_SEL, (reg32), (val))
#define RTO_CFG_SET_RTO_LENGTH(reg32, val)                     REG_FLD_SET(RTO_CFG_FLD_RTO_LENGTH, (reg32), (val))

#define DLL_SET_DLL(reg32, val)                                REG_FLD_SET(DLL_FLD_DLL, (reg32), (val))

#define DLM_SET_DLM(reg32, val)                                REG_FLD_SET(DLM_FLD_DLM, (reg32), (val))

#define EFR_SET_AUTO_CTS(reg32, val)                           REG_FLD_SET(EFR_FLD_AUTO_CTS, (reg32), (val))
#define EFR_SET_AUTO_RTS(reg32, val)                           REG_FLD_SET(EFR_FLD_AUTO_RTS, (reg32), (val))
#define EFR_SET_ENABLE_E(reg32, val)                           REG_FLD_SET(EFR_FLD_ENABLE_E, (reg32), (val))
#define EFR_SET_SW_FLOW_CONT(reg32, val)                       REG_FLD_SET(EFR_FLD_SW_FLOW_CONT, (reg32), (val))

#define FEATURE_SEL_SET_FEATURE_SEL(reg32, val)                REG_FLD_SET(FEATURE_SEL_FLD_FEATURE_SEL, (reg32), (val))

#define XON1_SET_XON1(reg32, val)                              REG_FLD_SET(XON1_FLD_XON1, (reg32), (val))

#define XON2_SET_XON2(reg32, val)                              REG_FLD_SET(XON2_FLD_XON2, (reg32), (val))

#define XOFF1_SET_XOFF1(reg32, val)                            REG_FLD_SET(XOFF1_FLD_XOFF1, (reg32), (val))

#define XOFF2_SET_XOFF2(reg32, val)                            REG_FLD_SET(XOFF2_FLD_XOFF2, (reg32), (val))

#define USB_RX_SEL_SET_USB_RX_SEL(reg32, val)                  REG_FLD_SET(USB_RX_SEL_FLD_USB_RX_SEL, (reg32), (val))

#define SLEEP_REQ_SET_SLEEP_REQ(reg32, val)                    REG_FLD_SET(SLEEP_REQ_FLD_SLEEP_REQ, (reg32), (val))

#define SLEEP_ACK_SET_SLEEP_ACK(reg32, val)                    REG_FLD_SET(SLEEP_ACK_FLD_SLEEP_ACK, (reg32), (val))

#define SPM_SEL_SET_SPM_SEL(reg32, val)                        REG_FLD_SET(SPM_SEL_FLD_SPM_SEL, (reg32), (val))

#define INB_ESC_CHAR_SET_INB_ESC_CHAR(reg32, val)              REG_FLD_SET(INB_ESC_CHAR_FLD_INB_ESC_CHAR, (reg32), (val))

#define INB_STA_CHAR_SET_INB_STA_CHAR(reg32, val)              REG_FLD_SET(INB_STA_CHAR_FLD_INB_STA_CHAR, (reg32), (val))

#define INB_IRQ_CTL_SET_INB_IRQ_IND(reg32, val)                REG_FLD_SET(INB_IRQ_CTL_FLD_INB_IRQ_IND, (reg32), (val))
#define INB_IRQ_CTL_SET_INB_TX_COMP(reg32, val)                REG_FLD_SET(INB_IRQ_CTL_FLD_INB_TX_COMP, (reg32), (val))
#define INB_IRQ_CTL_SET_INB_STA_CLR(reg32, val)                REG_FLD_SET(INB_IRQ_CTL_FLD_INB_STA_CLR, (reg32), (val))
#define INB_IRQ_CTL_SET_INB_IRQ_CLR(reg32, val)                REG_FLD_SET(INB_IRQ_CTL_FLD_INB_IRQ_CLR, (reg32), (val))
#define INB_IRQ_CTL_SET_INB_TRIG(reg32, val)                   REG_FLD_SET(INB_IRQ_CTL_FLD_INB_TRIG, (reg32), (val))
#define INB_IRQ_CTL_SET_INB_IRQ_EN(reg32, val)                 REG_FLD_SET(INB_IRQ_CTL_FLD_INB_IRQ_EN, (reg32), (val))
#define INB_IRQ_CTL_SET_INB_EN(reg32, val)                     REG_FLD_SET(INB_IRQ_CTL_FLD_INB_EN, (reg32), (val))

#define INB_STA_SET_INB_STA(reg32, val)                        REG_FLD_SET(INB_STA_FLD_INB_STA, (reg32), (val))

#define RBR_VAL_RBR(val)                                       REG_FLD_VAL(RBR_FLD_RBR, (val))

#define THR_VAL_THR(val)                                       REG_FLD_VAL(THR_FLD_THR, (val))

#define IER_VAL_CTSI(val)                                      REG_FLD_VAL(IER_FLD_CTSI, (val))
#define IER_VAL_RTSI(val)                                      REG_FLD_VAL(IER_FLD_RTSI, (val))
#define IER_VAL_XOFFI(val)                                     REG_FLD_VAL(IER_FLD_XOFFI, (val))
#define IER_VAL_EDSSI(val)                                     REG_FLD_VAL(IER_FLD_EDSSI, (val))
#define IER_VAL_ELSI(val)                                      REG_FLD_VAL(IER_FLD_ELSI, (val))
#define IER_VAL_ETBEI(val)                                     REG_FLD_VAL(IER_FLD_ETBEI, (val))
#define IER_VAL_ERBFI(val)                                     REG_FLD_VAL(IER_FLD_ERBFI, (val))

#define IIR_VAL_FIFOE(val)                                     REG_FLD_VAL(IIR_FLD_FIFOE, (val))
#define IIR_VAL_ID(val)                                        REG_FLD_VAL(IIR_FLD_ID, (val))

#define FCR_VAL_RFTL1_RFTL0(val)                               REG_FLD_VAL(FCR_FLD_RFTL1_RFTL0, (val))
#define FCR_VAL_TFTL1_TFTL0(val)                               REG_FLD_VAL(FCR_FLD_TFTL1_TFTL0, (val))
#define FCR_VAL_CLRT(val)                                      REG_FLD_VAL(FCR_FLD_CLRT, (val))
#define FCR_VAL_CLRR(val)                                      REG_FLD_VAL(FCR_FLD_CLRR, (val))
#define FCR_VAL_FIFOE(val)                                     REG_FLD_VAL(FCR_FLD_FIFOE, (val))

#define LCR_VAL_DLAB(val)                                      REG_FLD_VAL(LCR_FLD_DLAB, (val))
#define LCR_VAL_SB(val)                                        REG_FLD_VAL(LCR_FLD_SB, (val))
#define LCR_VAL_SP(val)                                        REG_FLD_VAL(LCR_FLD_SP, (val))
#define LCR_VAL_EPS(val)                                       REG_FLD_VAL(LCR_FLD_EPS, (val))
#define LCR_VAL_PEN(val)                                       REG_FLD_VAL(LCR_FLD_PEN, (val))
#define LCR_VAL_STB(val)                                       REG_FLD_VAL(LCR_FLD_STB, (val))
#define LCR_VAL_WLS1_WLS0(val)                                 REG_FLD_VAL(LCR_FLD_WLS1_WLS0, (val))

#define MCR_VAL_XOFF_STATUS(val)                               REG_FLD_VAL(MCR_FLD_XOFF_STATUS, (val))
#define MCR_VAL_Loop(val)                                      REG_FLD_VAL(MCR_FLD_Loop, (val))
#define MCR_VAL_RTS(val)                                       REG_FLD_VAL(MCR_FLD_RTS, (val))

#define LSR_VAL_FIFOERR(val)                                   REG_FLD_VAL(LSR_FLD_FIFOERR, (val))
#define LSR_VAL_TEMT(val)                                      REG_FLD_VAL(LSR_FLD_TEMT, (val))
#define LSR_VAL_THRE(val)                                      REG_FLD_VAL(LSR_FLD_THRE, (val))
#define LSR_VAL_BI(val)                                        REG_FLD_VAL(LSR_FLD_BI, (val))
#define LSR_VAL_FE(val)                                        REG_FLD_VAL(LSR_FLD_FE, (val))
#define LSR_VAL_PE(val)                                        REG_FLD_VAL(LSR_FLD_PE, (val))
#define LSR_VAL_OE(val)                                        REG_FLD_VAL(LSR_FLD_OE, (val))
#define LSR_VAL_DR(val)                                        REG_FLD_VAL(LSR_FLD_DR, (val))

#define MSR_VAL_CTS(val)                                       REG_FLD_VAL(MSR_FLD_CTS, (val))
#define MSR_VAL_DCTS(val)                                      REG_FLD_VAL(MSR_FLD_DCTS, (val))

#define SCR_VAL_SCR(val)                                       REG_FLD_VAL(SCR_FLD_SCR, (val))

#define AUTOBAUD_EN_VAL_AUTOBAUD_SEL(val)                      REG_FLD_VAL(AUTOBAUD_EN_FLD_AUTOBAUD_SEL, (val))
#define AUTOBAUD_EN_VAL_AUTOBAUD_EN(val)                       REG_FLD_VAL(AUTOBAUD_EN_FLD_AUTOBAUD_EN, (val))

#define HIGHSPEED_VAL_SPEED(val)                               REG_FLD_VAL(HIGHSPEED_FLD_SPEED, (val))

#define SAMPLE_COUNT_VAL_SAMPLECOUNT(val)                      REG_FLD_VAL(SAMPLE_COUNT_FLD_SAMPLECOUNT, (val))

#define SAMPLE_POINT_VAL_SAMPLEPOINT(val)                      REG_FLD_VAL(SAMPLE_POINT_FLD_SAMPLEPOINT, (val))

#define AUTOBAUD_REG_VAL_BAUD_STAT(val)                        REG_FLD_VAL(AUTOBAUD_REG_FLD_BAUD_STAT, (val))
#define AUTOBAUD_REG_VAL_BAUD_RATE(val)                        REG_FLD_VAL(AUTOBAUD_REG_FLD_BAUD_RATE, (val))

#define RATEFIX_AD_VAL_FREQ_SEL(val)                           REG_FLD_VAL(RATEFIX_AD_FLD_FREQ_SEL, (val))
#define RATEFIX_AD_VAL_AUTOBAUD_RATE_FIX(val)                  REG_FLD_VAL(RATEFIX_AD_FLD_AUTOBAUD_RATE_FIX, (val))
#define RATEFIX_AD_VAL_RATE_FIX(val)                           REG_FLD_VAL(RATEFIX_AD_FLD_RATE_FIX, (val))

#define AUTOBAUDSAMPLE_VAL_AUTOBAUDSAMPLE(val)                 REG_FLD_VAL(AUTOBAUDSAMPLE_FLD_AUTOBAUDSAMPLE, (val))

#define GUARD_VAL_GUARD_EN(val)                                REG_FLD_VAL(GUARD_FLD_GUARD_EN, (val))
#define GUARD_VAL_GUARD_CNT(val)                               REG_FLD_VAL(GUARD_FLD_GUARD_CNT, (val))

#define ESCAPE_DAT_VAL_ESCAPE_DAT(val)                         REG_FLD_VAL(ESCAPE_DAT_FLD_ESCAPE_DAT, (val))

#define ESCAPE_EN_VAL_ESC_EN(val)                              REG_FLD_VAL(ESCAPE_EN_FLD_ESC_EN, (val))

#define SLEEP_EN_VAL_SLEEP_EN(val)                             REG_FLD_VAL(SLEEP_EN_FLD_SLEEP_EN, (val))

#define DMA_EN_VAL_FIFO_lsr_sel(val)                           REG_FLD_VAL(DMA_EN_FLD_FIFO_lsr_sel, (val))
#define DMA_EN_VAL_TO_CNT_AUTORST(val)                         REG_FLD_VAL(DMA_EN_FLD_TO_CNT_AUTORST, (val))
#define DMA_EN_VAL_TX_DMA_EN(val)                              REG_FLD_VAL(DMA_EN_FLD_TX_DMA_EN, (val))
#define DMA_EN_VAL_RX_DMA_EN(val)                              REG_FLD_VAL(DMA_EN_FLD_RX_DMA_EN, (val))

#define RXTRI_AD_VAL_RXTRIG(val)                               REG_FLD_VAL(RXTRI_AD_FLD_RXTRIG, (val))

#define FRACDIV_L_VAL_FRACDIV_L(val)                           REG_FLD_VAL(FRACDIV_L_FLD_FRACDIV_L, (val))

#define FRACDIV_M_VAL_FRACDIV_M(val)                           REG_FLD_VAL(FRACDIV_M_FLD_FRACDIV_M, (val))

#define FCR_RD_VAL_RFTL1_RFTL0(val)                            REG_FLD_VAL(FCR_RD_FLD_RFTL1_RFTL0, (val))
#define FCR_RD_VAL_TFTL1_TFTL0(val)                            REG_FLD_VAL(FCR_RD_FLD_TFTL1_TFTL0, (val))
#define FCR_RD_VAL_CLRT(val)                                   REG_FLD_VAL(FCR_RD_FLD_CLRT, (val))
#define FCR_RD_VAL_CLRR(val)                                   REG_FLD_VAL(FCR_RD_FLD_CLRR, (val))
#define FCR_RD_VAL_FIFOE(val)                                  REG_FLD_VAL(FCR_RD_FLD_FIFOE, (val))

#define RTO_CFG_VAL_RTO_SEL(val)                               REG_FLD_VAL(RTO_CFG_FLD_RTO_SEL, (val))
#define RTO_CFG_VAL_RTO_LENGTH(val)                            REG_FLD_VAL(RTO_CFG_FLD_RTO_LENGTH, (val))

#define DLL_VAL_DLL(val)                                       REG_FLD_VAL(DLL_FLD_DLL, (val))

#define DLM_VAL_DLM(val)                                       REG_FLD_VAL(DLM_FLD_DLM, (val))

#define EFR_VAL_AUTO_CTS(val)                                  REG_FLD_VAL(EFR_FLD_AUTO_CTS, (val))
#define EFR_VAL_AUTO_RTS(val)                                  REG_FLD_VAL(EFR_FLD_AUTO_RTS, (val))
#define EFR_VAL_ENABLE_E(val)                                  REG_FLD_VAL(EFR_FLD_ENABLE_E, (val))
#define EFR_VAL_SW_FLOW_CONT(val)                              REG_FLD_VAL(EFR_FLD_SW_FLOW_CONT, (val))

#define FEATURE_SEL_VAL_FEATURE_SEL(val)                       REG_FLD_VAL(FEATURE_SEL_FLD_FEATURE_SEL, (val))

#define XON1_VAL_XON1(val)                                     REG_FLD_VAL(XON1_FLD_XON1, (val))

#define XON2_VAL_XON2(val)                                     REG_FLD_VAL(XON2_FLD_XON2, (val))

#define XOFF1_VAL_XOFF1(val)                                   REG_FLD_VAL(XOFF1_FLD_XOFF1, (val))

#define XOFF2_VAL_XOFF2(val)                                   REG_FLD_VAL(XOFF2_FLD_XOFF2, (val))

#define USB_RX_SEL_VAL_USB_RX_SEL(val)                         REG_FLD_VAL(USB_RX_SEL_FLD_USB_RX_SEL, (val))

#define SLEEP_REQ_VAL_SLEEP_REQ(val)                           REG_FLD_VAL(SLEEP_REQ_FLD_SLEEP_REQ, (val))

#define SLEEP_ACK_VAL_SLEEP_ACK(val)                           REG_FLD_VAL(SLEEP_ACK_FLD_SLEEP_ACK, (val))

#define SPM_SEL_VAL_SPM_SEL(val)                               REG_FLD_VAL(SPM_SEL_FLD_SPM_SEL, (val))

#define INB_ESC_CHAR_VAL_INB_ESC_CHAR(val)                     REG_FLD_VAL(INB_ESC_CHAR_FLD_INB_ESC_CHAR, (val))

#define INB_STA_CHAR_VAL_INB_STA_CHAR(val)                     REG_FLD_VAL(INB_STA_CHAR_FLD_INB_STA_CHAR, (val))

#define INB_IRQ_CTL_VAL_INB_IRQ_IND(val)                       REG_FLD_VAL(INB_IRQ_CTL_FLD_INB_IRQ_IND, (val))
#define INB_IRQ_CTL_VAL_INB_TX_COMP(val)                       REG_FLD_VAL(INB_IRQ_CTL_FLD_INB_TX_COMP, (val))
#define INB_IRQ_CTL_VAL_INB_STA_CLR(val)                       REG_FLD_VAL(INB_IRQ_CTL_FLD_INB_STA_CLR, (val))
#define INB_IRQ_CTL_VAL_INB_IRQ_CLR(val)                       REG_FLD_VAL(INB_IRQ_CTL_FLD_INB_IRQ_CLR, (val))
#define INB_IRQ_CTL_VAL_INB_TRIG(val)                          REG_FLD_VAL(INB_IRQ_CTL_FLD_INB_TRIG, (val))
#define INB_IRQ_CTL_VAL_INB_IRQ_EN(val)                        REG_FLD_VAL(INB_IRQ_CTL_FLD_INB_IRQ_EN, (val))
#define INB_IRQ_CTL_VAL_INB_EN(val)                            REG_FLD_VAL(INB_IRQ_CTL_FLD_INB_EN, (val))

#define INB_STA_VAL_INB_STA(val)                               REG_FLD_VAL(INB_STA_FLD_INB_STA, (val))

#endif // __UARTHUB_UART0_REGS_MT6989_H__
