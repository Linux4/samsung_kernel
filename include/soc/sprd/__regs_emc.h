#ifndef REGS_EMC_H_
#define REGS_EMC_H_

#ifndef __SCI_GLB_REGS_H__
#error  "Don't include this file directly, include <mach/sci_glb_regs.h>"
#endif

#define UMCTL_CFG_ADD_SCFG            0x000
#define UMCTL_CFG_ADD_SCTL            0x004//moves the uPCTL from one state to another
#define UMCTL_CFG_ADD_STAT            0x008//provides information about the current state of the uPCTL
#define UMCTL_CFG_ADD_MCMD            0x040
#define UMCTL_CFG_ADD_POWCTL          0x044
#define UMCTL_CFG_ADD_POWSTAT         0x048
#define UMCTL_CFG_ADD_MCFG1           0x07C
#define UMCTL_CFG_ADD_MCFG            0x080
#define UMCTL_CFG_ADD_PPCFG           0x084
#define UMCTL_CFG_ADD_TOGCNT1U        0x0c0
#define UMCTL_CFG_ADD_TINIT           0x0c4
#define UMCTL_CFG_ADD_TRSTH           0x0c8
#define UMCTL_CFG_ADD_TOGCNT100N      0x0cc
#define UMCTL_CFG_ADD_TREFI           0x0d0
#define UMCTL_CFG_ADD_TMRD            0x0d4
#define UMCTL_CFG_ADD_TRFC            0x0d8
#define UMCTL_CFG_ADD_TRP             0x0dc
#define UMCTL_CFG_ADD_TRTW            0x0e0
#define UMCTL_CFG_ADD_TAL             0x0e4
#define UMCTL_CFG_ADD_TCL             0x0e8
#define UMCTL_CFG_ADD_TCWL            0x0ec
#define UMCTL_CFG_ADD_TRAS            0x0f0
#define UMCTL_CFG_ADD_TRC             0x0f4
#define UMCTL_CFG_ADD_TRCD            0x0f8
#define UMCTL_CFG_ADD_TRRD            0x0fc
#define UMCTL_CFG_ADD_TRTP            0x100
#define UMCTL_CFG_ADD_TWR             0x104
#define UMCTL_CFG_ADD_TWTR            0x108
#define UMCTL_CFG_ADD_TEXSR           0x10c
#define UMCTL_CFG_ADD_TXP             0x110
#define UMCTL_CFG_ADD_TXPDLL          0x114
#define UMCTL_CFG_ADD_TZQCS           0x118
#define UMCTL_CFG_ADD_TZQCSI          0x11C
#define UMCTL_CFG_ADD_TDQS            0x120
#define UMCTL_CFG_ADD_TCKSRE          0x124
#define UMCTL_CFG_ADD_TCKSRX          0x128
#define UMCTL_CFG_ADD_TCKE            0x12c
#define UMCTL_CFG_ADD_TMOD            0x130
#define UMCTL_CFG_ADD_TRSTL           0x134
#define UMCTL_CFG_ADD_TZQCL           0x138
#define UMCTL_CFG_ADD_TCKESR          0x13c
#define UMCTL_CFG_ADD_TDPD            0x140
#define UMCTL_CFG_ADD_DTUWACTL        0x200
#define UMCTL_CFG_ADD_DTURACTL        0x204
#define UMCTL_CFG_ADD_DTUCFG          0x208
#define UMCTL_CFG_ADD_DTUECTL         0x20C
#define UMCTL_CFG_ADD_DTUWD0          0x210
#define UMCTL_CFG_ADD_DTUWD1          0x214
#define UMCTL_CFG_ADD_DTUWD2          0x218
#define UMCTL_CFG_ADD_DTUWD3          0x21c
#define UMCTL_CFG_ADD_DTURD0          0x224
#define UMCTL_CFG_ADD_DTURD1          0x228
#define UMCTL_CFG_ADD_DTURD2          0x22C
#define UMCTL_CFG_ADD_DTURD3          0x230
#define UMCTL_CFG_ADD_DFITPHYWRDATA   0x250
#define UMCTL_CFG_ADD_DFITPHYWRLAT    0x254
#define UMCTL_CFG_ADD_DFITRDDATAEN    0x260
#define UMCTL_CFG_ADD_DFITPHYRDLAT    0x264
#define UMCTL_CFG_ADD_DFISTSTAT0      0x2c0
#define UMCTL_CFG_ADD_DFISTCFG0       0x2c4
#define UMCTL_CFG_ADD_DFISTCFG1       0x2c8
#define UMCTL_CFG_ADD_DFISTCFG2       0x2d8
#define UMCTL_CFG_ADD_DFILPCFG0       0x2f0
#define UMCTL_CFG_ADD_PCFG_0          0x400
#define UMCTL_CFG_ADD_PCFG_1          0x404
#define UMCTL_CFG_ADD_PCFG_2          0x408
#define UMCTL_CFG_ADD_PCFG_3          0x40c
#define UMCTL_CFG_ADD_PCFG_4          0x410
#define UMCTL_CFG_ADD_PCFG_5          0x414
#define UMCTL_CFG_ADD_PCFG_6          0x418
#define UMCTL_CFG_ADD_PCFG_7          0x41c
#define UMCTL_CFG_ADD_DCFG_CS0        0x484
#define UMCTL_CFG_ADD_DCFG_CS1        0x494
//}}}

//publ configure registers declaration.//{{{
//copied from PUBL FPGA cfg module. here shift them(<< 2)
#define PUBL_CFG_ADD_RIDR          (0x00 * 4) // R   - Revision Identification Register
#define PUBL_CFG_ADD_PIR           (0x01 * 4) // R/W - PHY Initialization Register
#define PUBL_CFG_ADD_PGCR          (0x02 * 4) // R/W - PHY General Configuration Register
#define PUBL_CFG_ADD_PGSR          (0x03 * 4) // R   - PHY General Status Register
#define PUBL_CFG_ADD_DLLGCR        (0x04 * 4) // R/W - DLL General Control Register
#define PUBL_CFG_ADD_ACDLLCR       (0x05 * 4) // R/W - AC DLL Control Register
#define PUBL_CFG_ADD_PTR0          (0x06 * 4) // R/W - PHY Timing Register 0
#define PUBL_CFG_ADD_PTR1          (0x07 * 4) // R/W - PHY Timing Register 1
#define PUBL_CFG_ADD_PTR2          (0x08 * 4) // R/W - PHY Timing Register 2
#define PUBL_CFG_ADD_ACIOCR        (0x09 * 4) // R/W - AC I/O Configuration Register
#define PUBL_CFG_ADD_DXCCR         (0x0A * 4) // R/W - DATX8 I/O Configuration Register
#define PUBL_CFG_ADD_DSGCR         (0x0B * 4) // R/W - DFI Configuration Register
#define PUBL_CFG_ADD_DCR           (0x0C * 4) // R/W - DRAM Configuration Register
#define PUBL_CFG_ADD_DTPR0         (0x0D * 4) // R/W - SDRAM Timing Parameters Register 0
#define PUBL_CFG_ADD_DTPR1         (0x0E * 4) // R/W - SDRAM Timing Parameters Register 1
#define PUBL_CFG_ADD_DTPR2         (0x0F * 4) // R/W - SDRAM Timing Parameters Register 2
#define PUBL_CFG_ADD_MR0           (0x10 * 4) // R/W - Mode Register
#define PUBL_CFG_ADD_MR1           (0x11 * 4) // R/W - Ext}ed Mode Register
#define PUBL_CFG_ADD_MR2           (0x12 * 4) // R/W - Ext}ed Mode Register 2
#define PUBL_CFG_ADD_MR3           (0x13 * 4) // R/W - Ext}ed Mode Register 3
#define PUBL_CFG_ADD_ODTCR         (0x14 * 4) // R/W - ODT Configuration Register
#define PUBL_CFG_ADD_DTAR          (0x15 * 4) // R/W - Data Training Address Register
#define PUBL_CFG_ADD_DTDR0         (0x16 * 4) // R/W - Data Training Data Register 0
#define PUBL_CFG_ADD_DTDR1         (0x17 * 4) // R/W - Data Training Data Register 1
#define PUBL_CFG_ADD_DCUAR         (0x30 * 4) // R/W - DCU Address Register
#define PUBL_CFG_ADD_DCUDR         (0x31 * 4) // R/W - DCU Data Register
#define PUBL_CFG_ADD_DCURR         (0x32 * 4) // R/W - DCU Run Register
#define PUBL_CFG_ADD_DCUSR0        (0x36 * 4) // R/W - DCU status register
#define PUBL_CFG_ADD_BISTRR        (0x40 * 4) // R/W - BIST run register
#define PUBL_CFG_ADD_BISTMSKR0     (0x41 * 4) // R/W - BIST Mask Register 0
#define PUBL_CFG_ADD_BISTMSKR1     (0x42 * 4) // R/W - BIST Mask Register 1
#define PUBL_CFG_ADD_BISTWCR       (0x43 * 4) // R/W - BIST Word Count Register
#define PUBL_CFG_ADD_BISTLSR       (0x44 * 4) // R/W - BIST LFSR Seed Register 
#define PUBL_CFG_ADD_BISTAR0       (0x45 * 4) // R/W - BIST Address Register 0
#define PUBL_CFG_ADD_BISTAR1       (0x46 * 4) // R/W - BIST Address Register 1
#define PUBL_CFG_ADD_BISTAR2       (0x47 * 4) // R/W - BIST Address Register 2
#define PUBL_CFG_ADD_BISTUDPR      (0x48 * 4) // R/W - BIST User Data Pattern Register
#define PUBL_CFG_ADD_BISTGSR       (0x49 * 4) // R/W - BIST General Status Register
#define PUBL_CFG_ADD_BISTWER       (0x4a * 4) // R/W - BIST Word Error Register
#define PUBL_CFG_ADD_BISTBER0      (0x4b * 4) // R/W - BIST Bit Error Register 0
#define PUBL_CFG_ADD_BISTBER1      (0x4c * 4) // R/W - BIST Bit Error Register 1
#define PUBL_CFG_ADD_BISTBER2      (0x4d * 4) // R/W - BIST Bit Error Register 2
#define PUBL_CFG_ADD_BISTWCSR      (0x4e * 4) // R/W - BIST Word Count Status Register
#define PUBL_CFG_ADD_BISTFWR0      (0x4f * 4) // R/W - BIST Fail Word Register 0
#define PUBL_CFG_ADD_BISTFWR1      (0x50 * 4) // R/W - BIST Fail Word Register 1
#define PUBL_CFG_ADD_ZQ0CR0        (0x60 * 4) // R/W - ZQ 0 Impedance Control Register 0
#define PUBL_CFG_ADD_ZQ0CR1        (0x61 * 4) // R/W - ZQ 1
#define PUBL_CFG_ADD_DX0GCR        (0x70 * 4) // R/W - DATX8 0 General Configuration Register
#define PUBL_CFG_ADD_DX0GSR0       (0x71 * 4) // R   - DATX8 0 General Status Register 0
#define PUBL_CFG_ADD_DX0GSR1       (0x72 * 4) // R   - DATX8 0 General Status Register 1
#define PUBL_CFG_ADD_DX0DLLCR      (0x73 * 4) // R   - DATX8 0 DLL Control Register
#define PUBL_CFG_ADD_DX0DQTR       (0x74 * 4) // R   - DATX8 0 DLL Control Register
#define PUBL_CFG_ADD_DX0DQSTR      (0x75 * 4) // R/W

#define PUBL_CFG_ADD_DX1GCR        (0x80 * 4) // R/W - DATX8 1 General Configuration Register
#define PUBL_CFG_ADD_DX1GSR0       (0x81 * 4) // R   - DATX8 1 General Status Register 0
#define PUBL_CFG_ADD_DX1GSR1       (0x82 * 4) // R   - DATX8 1 General Status Register 1
#define PUBL_CFG_ADD_DX1DLLCR      (0x83 * 4) // R   - DATX8 1 DLL Control Register
#define PUBL_CFG_ADD_DX1DQTR       (0x84 * 4) // R   - DATX8 1 DLL Control Register
#define PUBL_CFG_ADD_DX1DQSTR      (0x85 * 4) // R/W

#define PUBL_CFG_ADD_DX2GCR        (0x90 * 4) // R/W - DATX8 2 General Configuration Register
#define PUBL_CFG_ADD_DX2GSR0       (0x91 * 4) // R   - DATX8 2 General Status Register 0
#define PUBL_CFG_ADD_DX2GSR1       (0x92 * 4) // R   - DATX8 2 General Status Register 1
#define PUBL_CFG_ADD_DX2DLLCR      (0x93 * 4) // R   - DATX8 2 DLL Control Register
#define PUBL_CFG_ADD_DX2DQTR       (0x94 * 4) // R   - DATX8 2 DLL Control Register
#define PUBL_CFG_ADD_DX2DQSTR      (0x95 * 4) // R/W

#define PUBL_CFG_ADD_DX3GCR        (0xa0 * 4) // R/W - DATX8 3 General Configuration Register
#define PUBL_CFG_ADD_DX3GSR0       (0xa1 * 4) // R   - DATX8 3 General Status Register 0
#define PUBL_CFG_ADD_DX3GSR1       (0xa2 * 4) // R   - DATX8 3 General Status Register 1
#define PUBL_CFG_ADD_DX3DLLCR      (0xa3 * 4) // R   - DATX8 3 DLL Control Register
#define PUBL_CFG_ADD_DX3DQTR       (0xa4 * 4) // R   - DATX8 3 DLL Control Register
#define PUBL_CFG_ADD_DX3DQSTR      (0xa5 * 4) // R/W
#endif