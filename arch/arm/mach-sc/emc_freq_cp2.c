//#include "dram_phy.h"
//#include "umctl2_reg.h"


/**---------------------------------------------------------------------------*
 **                            Macro Define
 **---------------------------------------------------------------------------*/
#define REG32(x)                           (*((volatile uint32 *)(x)))
#define REG16(x)                           (*((volatile uint16 *)(x)))
#define UMCTL2_REG_GET(reg_addr)             (*((volatile uint32 *)(reg_addr)))
#define UMCTL2_REG_SET( reg_addr, value )    *(volatile uint32 *)(reg_addr) = value
#define UMCTL_REG_BASE (0x01000000)
#define PUBL_REG_BASE  (0x01010000)

#define UMCTL2_REG_(x) (UMCTL_REG_BASE+(x))

/*
 *uMCTL2 DDRC registers
*/
#define UMCTL_MSTR       UMCTL2_REG_(0x0000) /*master*/
#define UMCTL_STAT       UMCTL2_REG_(0x0004) /*oeration mode status*/
#define UMCTL_MRCTRL0    UMCTL2_REG_(0x0010)/*mode register read/werite control*/ 
#define UMCTL_MRCTRL1    UMCTL2_REG_(0x0014) 
#define UMCTL_MRSTAT     UMCTL2_REG_(0x0018)/*mode register read/weite status*/ 

#define UMCTL_DERATEEN   UMCTL2_REG_(0x0020)/*temperature derate enable*/
#define UMCTL_DERATEINT  UMCTL2_REG_(0x0024)/*temperature derate interval*/
#define UMCTL_PWRCTL     UMCTL2_REG_(0x0030)/*low power control*/
#define UMCTL_PWRTMG     UMCTL2_REG_(0x0034)/*low power timing*/

#define UMCTL_RFSHCTL0   UMCTL2_REG_(0x0050)/*refresh control0*/
#define UMCTL_RFSHCTL1   UMCTL2_REG_(0x0054)
#define UMCTL_RFSHCTL2   UMCTL2_REG_(0x0058)
#define UMCTL_RFSHCTL3   UMCTL2_REG_(0x0060)
#define UMCTL_RFSHTMG    UMCTL2_REG_(0x0064)/*refresh Timing*/
#define UMCTL_ECCCFG0    UMCTL2_REG_(0x0070)/*ecc configuration*/
#define UMCTL_ECCCFG1    UMCTL2_REG_(0x0074)
#define UMCTL_ECCSTAT    UMCTL2_REG_(0x0078)/*ecc status*/
#define UMCTL_ECCCLR     UMCTL2_REG_(0x007C)/*ecc clear*/
#define UMCTL_ECCERRCNT  UMCTL2_REG_(0x0080)/*ecc error counter*/

#define UMCTL_ECCADDR0   UMCTL2_REG_(0x0084)/*ecc corrected error address reg0*/
#define UMCTL_ECCADDR1   UMCTL2_REG_(0x0088)
#define UMCTL_ECCSYN0    UMCTL2_REG_(0x008C)/*ecc corected syndrome reg0*/
#define UMCTL_ECCSYN1    UMCTL2_REG_(0x0090)
#define UMCTL_ECCSYN2    UMCTL2_REG_(0x0094)
#define UMCTL_ECCBITMASK0    UMCTL2_REG_(0x0098)
#define UMCTL_ECCBITMASK1    UMCTL2_REG_(0x009C)
#define UMCTL_ECCBITMASK2    UMCTL2_REG_(0x00A0)
#define UMCTL_ECCUADDR0      UMCTL2_REG_(0x00A4)/*ecc uncorrected error address reg0*/
#define UMCTL_ECCUADDR1      UMCTL2_REG_(0x00A8)
#define UMCTL_ECCUSYN0       UMCTL2_REG_(0x00AC)/*ecc UNcorected syndrome reg0*/
#define UMCTL_ECCUSYN1       UMCTL2_REG_(0x00B0)
#define UMCTL_ECCUSYN2       UMCTL2_REG_(0x00B4)
#define UMCTL_ECCPOISONADDR0 UMCTL2_REG_(0x00B8)/*ecc data poisoning address reg0*/
#define UMCTL_ECCPOISONADDR1 UMCTL2_REG_(0x00BC)


#define UMCTL_PARCTL     UMCTL2_REG_(0x00C0)/*parity control register*/
#define UMCTL_PARSTAT    UMCTL2_REG_(0x00C4)/*parity status register*/

#define UMCTL_INIT0      UMCTL2_REG_(0x00D0)/*SDRAM initialization register0*/
#define UMCTL_INIT1      UMCTL2_REG_(0x00D4)
#define UMCTL_INIT2      UMCTL2_REG_(0x00D8)
#define UMCTL_INIT3      UMCTL2_REG_(0x00DC)
#define UMCTL_INIT4      UMCTL2_REG_(0x00E0)
#define UMCTL_INIT5      UMCTL2_REG_(0x00E4)


#define UMCTL_DIMMCTL    UMCTL2_REG_(0x00F0)/*DIMM control register*/
#define UMCTL_RANKCTL    UMCTL2_REG_(0x00F4)

#define UMCTL_DRAMTMG0   UMCTL2_REG_(0x0100)/*SDRAM timing register0*/
#define UMCTL_DRAMTMG1   UMCTL2_REG_(0x0104)
#define UMCTL_DRAMTMG2   UMCTL2_REG_(0x0108)
#define UMCTL_DRAMTMG3   UMCTL2_REG_(0x010C)
#define UMCTL_DRAMTMG4   UMCTL2_REG_(0x0110)
#define UMCTL_DRAMTMG5   UMCTL2_REG_(0x0114)
#define UMCTL_DRAMTMG6   UMCTL2_REG_(0x0118)
#define UMCTL_DRAMTMG7   UMCTL2_REG_(0x011C)
#define UMCTL_DRAMTMG8   UMCTL2_REG_(0x0120)

#define UMCTL_ZQCTL0     UMCTL2_REG_(0x0180)/*ZQ control register0*/
#define UMCTL_ZQCTL1     UMCTL2_REG_(0x0184)
#define UMCTL_ZQCTL2     UMCTL2_REG_(0x0188)
#define UMCTL_ZQSTAT     UMCTL2_REG_(0x018C)


#define UMCTL_DFITMG0    UMCTL2_REG_(0x0190)/*DFI timing register0*/
#define UMCTL_DFITMG1    UMCTL2_REG_(0x0194)

#define UMCTL_DFILPCFG0  UMCTL2_REG_(0x0198)/*DFI low power configuration*/
#define UMCTL_DFIUPD0    UMCTL2_REG_(0x01A0)/*DFI update register0*/
#define UMCTL_DFIUPD1    UMCTL2_REG_(0x01A4)
#define UMCTL_DFIUPD2    UMCTL2_REG_(0x01A8)
#define UMCTL_DFIUPD3    UMCTL2_REG_(0x01AC)

#define UMCTL_DFIMISC    UMCTL2_REG_(0x01B0)

#define UMCTL_TRAINCTL0  UMCTL2_REG_(0x01D0)/*PHY eval training control reg0*/
#define UMCTL_TRAINCTL1  UMCTL2_REG_(0x01D4)
#define UMCTL_TRAINCTL2  UMCTL2_REG_(0x01D8)
#define UMCTL_TRAINSTAT  UMCTL2_REG_(0x01DC)

#define UMCTL_ADDRMAP0   UMCTL2_REG_(0x0200)/*address map register0*/
#define UMCTL_ADDRMAP1   UMCTL2_REG_(0x0204)
#define UMCTL_ADDRMAP2   UMCTL2_REG_(0x0208)
#define UMCTL_ADDRMAP3   UMCTL2_REG_(0x020C)
#define UMCTL_ADDRMAP4   UMCTL2_REG_(0x0210)
#define UMCTL_ADDRMAP5   UMCTL2_REG_(0x0214)
#define UMCTL_ADDRMAP6   UMCTL2_REG_(0x0218)

#define UMCTL_ODTCFG     UMCTL2_REG_(0x0240)/*ODT configuration register*/
#define UMCTL_ODTMAP     UMCTL2_REG_(0x0244)

#define UMCTL_SCHED      UMCTL2_REG_(0x0250)/*scheduler control register*/
#define UMCTL_PERFHPR0   UMCTL2_REG_(0x0258)/*high priority read CAM reg0*/
#define UMCTL_PERFHPR1   UMCTL2_REG_(0x025C)
#define UMCTL_PERFLPR0   UMCTL2_REG_(0x0260)
#define UMCTL_PERFLPR1   UMCTL2_REG_(0x0264)

#define UMCTL_PERFWR0    UMCTL2_REG_(0x0268)/*write CAM reg0*/
#define UMCTL_PERFWR1    UMCTL2_REG_(0x026C)
#define UMCTL_DBG0       UMCTL2_REG_(0x0300)/*debug register0*/
#define UMCTL_DBG1       UMCTL2_REG_(0x0304)
#define UMCTL_DBGCAM     UMCTL2_REG_(0x0308)/*cam debug register*/


/*
 *uMCTL2 Multi-Port registers
*/
#define UMCTL_PCCFG      UMCTL2_REG_(0x0400)/*port common configuration*/

#define UMCTL_PCFGR_0    UMCTL2_REG_(0x0404+(0x00)*0xB0)/*Port n configuration read reg*/
#define UMCTL_PCFGR_1    UMCTL2_REG_(0x0404+(0x01)*0xB0)
#define UMCTL_PCFGR_2    UMCTL2_REG_(0x0404+(0x02)*0xB0)
#define UMCTL_PCFGR_3    UMCTL2_REG_(0x0404+(0x03)*0xB0)
#define UMCTL_PCFGR_4    UMCTL2_REG_(0x0404+(0x04)*0xB0)
#define UMCTL_PCFGR_5    UMCTL2_REG_(0x0404+(0x05)*0xB0)
#define UMCTL_PCFGR_6    UMCTL2_REG_(0x0404+(0x06)*0xB0)
#define UMCTL_PCFGR_7    UMCTL2_REG_(0x0404+(0x07)*0xB0)
#define UMCTL_PCFGR_8    UMCTL2_REG_(0x0404+(0x08)*0xB0)
#define UMCTL_PCFGR_9    UMCTL2_REG_(0x0404+(0x09)*0xB0)
#define UMCTL_PCFGR_10   UMCTL2_REG_(0x0404+(0x0A)*0xB0)
#define UMCTL_PCFGR_11   UMCTL2_REG_(0x0404+(0x0B)*0xB0)
#define UMCTL_PCFGR_12   UMCTL2_REG_(0x0404+(0x0C)*0xB0)
#define UMCTL_PCFGR_13   UMCTL2_REG_(0x0404+(0x0D)*0xB0)
#define UMCTL_PCFGR_14   UMCTL2_REG_(0x0404+(0x0E)*0xB0)
#define UMCTL_PCFGR_15   UMCTL2_REG_(0x0404+(0x0F)*0xB0)


#define UMCTL_PCFGW_0    UMCTL2_REG_(0x0408+(0x00)*0xB0)/*Port n configuration write reg*/
#define UMCTL_PCFGW_1    UMCTL2_REG_(0x0408+(0x01)*0xB0)
#define UMCTL_PCFGW_2    UMCTL2_REG_(0x0408+(0x02)*0xB0)
#define UMCTL_PCFGW_3    UMCTL2_REG_(0x0408+(0x03)*0xB0)
#define UMCTL_PCFGW_4    UMCTL2_REG_(0x0408+(0x04)*0xB0)
#define UMCTL_PCFGW_5    UMCTL2_REG_(0x0408+(0x05)*0xB0)
#define UMCTL_PCFGW_6    UMCTL2_REG_(0x0408+(0x06)*0xB0)
#define UMCTL_PCFGW_7    UMCTL2_REG_(0x0408+(0x07)*0xB0)
#define UMCTL_PCFGW_8    UMCTL2_REG_(0x0408+(0x08)*0xB0)
#define UMCTL_PCFGW_9    UMCTL2_REG_(0x0408+(0x09)*0xB0)
#define UMCTL_PCFGW_10   UMCTL2_REG_(0x0408+(0x0A)*0xB0)
#define UMCTL_PCFGW_11   UMCTL2_REG_(0x0408+(0x0B)*0xB0)
#define UMCTL_PCFGW_12   UMCTL2_REG_(0x0408+(0x0C)*0xB0)
#define UMCTL_PCFGW_13   UMCTL2_REG_(0x0408+(0x0D)*0xB0)
#define UMCTL_PCFGW_14   UMCTL2_REG_(0x0408+(0x0E)*0xB0)
#define UMCTL_PCFGW_15   UMCTL2_REG_(0x0408+(0x0F)*0xB0)


#define UMCTL_PORT_EN_0    0xFFFF0000//UMCTL2_REG_(0x0490+(0x00)*0xB0)/*Port n enable reg*/
#define UMCTL_PORT_EN_1    0xFFFF0000//UMCTL2_REG_(0x0490+(0x01)*0xB0)/*Port n enable reg*/
#define UMCTL_PORT_EN_2    0xFFFF0000//UMCTL2_REG_(0x0490+(0x02)*0xB0)/*Port n enable reg*/
#define UMCTL_PORT_EN_3    0xFFFF0000//UMCTL2_REG_(0x0490+(0x03)*0xB0)/*Port n enable reg*/
#define UMCTL_PORT_EN_4    0xFFFF0000//UMCTL2_REG_(0x0490+(0x04)*0xB0)/*Port n enable reg*/
#define UMCTL_PORT_EN_5    0xFFFF0000//UMCTL2_REG_(0x0490+(0x05)*0xB0)/*Port n enable reg*/
#define UMCTL_PORT_EN_6    0xFFFF0000//UMCTL2_REG_(0x0490+(0x06)*0xB0)/*Port n enable reg*/
#define UMCTL_PORT_EN_7    0xFFFF0000//UMCTL2_REG_(0x0490+(0x07)*0xB0)/*Port n enable reg*/
#define UMCTL_PORT_EN_8    0xFFFF0000//UMCTL2_REG_(0x0490+(0x08)*0xB0)/*Port n enable reg*/
#define UMCTL_PORT_EN_9    0xFFFF0000//UMCTL2_REG_(0x0490+(0x09)*0xB0)/*Port n enable reg*/


/*
#define UMCTL2_PCFGIDMASKCH_m_N   
#define UMCTL2_PCFGIDVALUECH_m_N   
*/


/*
 *Refer to PUBL databook 1.44a for detail,Chapter3.3 Registers.
*/
#define PUBL_RIDR           (PUBL_REG_BASE+0x00*4) // R   - Revision Identification Register
#define PUBL_PIR            (PUBL_REG_BASE+0x01*4) // R/W - PHY Initialization Register
#define PUBL_PGCR           (PUBL_REG_BASE+0x02*4) // R/W - PHY General Configuration Register
#define PUBL_PGSR           (PUBL_REG_BASE+0x03*4) // R   - PHY General Status Register                                     
#define PUBL_DLLGCR         (PUBL_REG_BASE+0x04*4) // R/W - DLL General Control Register
#define PUBL_ACDLLCR        (PUBL_REG_BASE+0x05*4) // R/W - AC DLL Control Register
#define PUBL_PTR0           (PUBL_REG_BASE+0x06*4) // R/W - PHY Timing Register 0
#define PUBL_PTR1           (PUBL_REG_BASE+0x07*4) // R/W - PHY Timing Register 1
#define PUBL_PTR2           (PUBL_REG_BASE+0x08*4) // R/W - PHY Timing Register 2
#define PUBL_ACIOCR         (PUBL_REG_BASE+0x09*4) // R/W - AC I/O Configuration Register                                
#define PUBL_DXCCR          (PUBL_REG_BASE+0x0A*4) // R/W - DATX8 I/O Configuration Register
#define PUBL_DSGCR          (PUBL_REG_BASE+0x0B*4) // R/W - DFI Configuration Register
#define PUBL_DCR            (PUBL_REG_BASE+0x0C*4) // R/W - DRAM Configuration Register
#define PUBL_DTPR0          (PUBL_REG_BASE+0x0D*4) // R/W - SDRAM Timing Parameters Register 0
#define PUBL_DTPR1          (PUBL_REG_BASE+0x0E*4) // R/W - SDRAM Timing Parameters Register 1
#define PUBL_DTPR2          (PUBL_REG_BASE+0x0F*4) // R/W - SDRAM Timing Parameters Register 2
#define PUBL_MR0            (PUBL_REG_BASE+0x10*4) // R/W - Mode Register
#define PUBL_MR1            (PUBL_REG_BASE+0x11*4) // R/W - Ext}ed Mode Register
#define PUBL_MR2            (PUBL_REG_BASE+0x12*4) // R/W - Ext}ed Mode Register 2
#define PUBL_MR3            (PUBL_REG_BASE+0x13*4) // R/W - Ext}ed Mode Register 3
#define PUBL_ODTCR          (PUBL_REG_BASE+0x14*4) // R/W - ODT Configuration Register
#define PUBL_DTAR           (PUBL_REG_BASE+0x15*4) // R/W - Data Training Address Register
#define PUBL_DTDR0          (PUBL_REG_BASE+0x16*4) // R/W - Data Training Data Register 0
#define PUBL_DTDR1          (PUBL_REG_BASE+0x17*4) // R/W - Data Training Data Register 1
#define PUBL_DCUAR          (PUBL_REG_BASE+0X30*4) // R/W - DCU Address Resiter
#define PUBL_DCUDR          (PUBL_REG_BASE+0x31*4) // R/W - DCU Data Register
#define PUBL_DCURR          (PUBL_REG_BASE+0x32*4) // R/W - DCU Run Register
#define PUBL_DCULR          (PUBL_REG_BASE+0x33*4) // R/W - DCU Loop Register
#define PUBL_DCUGCR         (PUBL_REG_BASE+0x34*4) // R/W - DCU General Configuration Register
#define PUBL_DCUTPR         (PUBL_REG_BASE+0x35*4) // R/W - DCU Timing Parameters Registers
#define PUBL_DCUSR0         (PUBL_REG_BASE+0x36*4) // R   - DCU Status Register 0
#define PUBL_DCUSR1         (PUBL_REG_BASE+0x37*4) // R   - DCU Status Register 1
#define PUBL_BISTRR         (PUBL_REG_BASE+0x40*4) // R/W - BIST Run Register
#define PUBL_BISTMSKR0      (PUBL_REG_BASE+0x41*4) // R/W - BIST Mask Register 0
#define PUBL_BISTMSKR1      (PUBL_REG_BASE+0x42*4) // R/W - BIST Mask Register 1
#define PUBL_BISTWCR        (PUBL_REG_BASE+0x43*4) // R/W - BIST Word Count Register
#define PUBL_BISTLSR        (PUBL_REG_BASE+0x44*4) // R/W - BIST LFSR Seed Register
#define PUBL_BISTAR0        (PUBL_REG_BASE+0x45*4) // R/W - BIST Address Register 0
#define PUBL_BISTAR1        (PUBL_REG_BASE+0x46*4) // R/W - BIST Address Register 1
#define PUBL_BISTAR2        (PUBL_REG_BASE+0x47*4) // R/W - BIST Address Register 2
#define PUBL_BISTUDPR       (PUBL_REG_BASE+0x48*4) // R/W - BIST User Data Pattern Register
#define PUBL_BISTGSR        (PUBL_REG_BASE+0x49*4) // R   - BIST General Status Register
#define PUBL_BISTWER        (PUBL_REG_BASE+0x4A*4) // R   - BIST Word Error Register
#define PUBL_BISTBER0       (PUBL_REG_BASE+0x4B*4) // R   - BIST Bit Error Register 0
#define PUBL_BISTBER1       (PUBL_REG_BASE+0x4C*4) // R   - BIST Bit Error Register 1
#define PUBL_BISTBER2       (PUBL_REG_BASE+0x4D*4) // R   - BIST Bit Error Register 2
#define PUBL_BISTWCSR       (PUBL_REG_BASE+0x4E*4) // R   - BIST Word Count Status Register
#define PUBL_BISTFWR0       (PUBL_REG_BASE+0x4F*4) // R   - BIST Fail Word Register 0
#define PUBL_BISTFWR1       (PUBL_REG_BASE+0x50*4) // R   - BIST Fail Word Register 1
#define PUBL_ZQ0CR0         (PUBL_REG_BASE+0x60*4) // R/W - ZQ 0 Impedance Control Register 0
#define PUBL_ZQ0CR1         (PUBL_REG_BASE+0x61*4) // R/W - ZQ 0 Impedance Control Register 1
#define PUBL_ZQ0SR0         (PUBL_REG_BASE+0x62*4) // R   - ZQ 0 Impedance Status Register 0
#define PUBL_ZQ0SR1         (PUBL_REG_BASE+0x63*4) // R   - ZQ 0 Impedance Status Register 1
#define PUBL_ZQ1CR0         (PUBL_REG_BASE+0x64*4) // R/W - ZQ 1 Impedance Control Register 0
#define PUBL_ZQ1CR1         (PUBL_REG_BASE+0x65*4) // R/W - ZQ 1 Impedance Control Register 1
#define PUBL_ZQ1SR0         (PUBL_REG_BASE+0x66*4) // R   - ZQ 1 Impedance Status Register 0
#define PUBL_ZQ1SR1         (PUBL_REG_BASE+0x67*4) // R   - ZQ 1 Impedance Status Register 1
#define PUBL_ZQ2CR0         (PUBL_REG_BASE+0x68*4) // R/W - ZQ 2 Impedance Control Register 0
#define PUBL_ZQ2CR1         (PUBL_REG_BASE+0x69*4) // R/W - ZQ 2 Impedance Control Register 1
#define PUBL_ZQ2SR0         (PUBL_REG_BASE+0x6A*4) // R   - ZQ 2 Impedance Status Register 0
#define PUBL_ZQ2SR1         (PUBL_REG_BASE+0x6B*4) // R   - ZQ 2 Impedance Status Register 1
#define PUBL_ZQ3CR0         (PUBL_REG_BASE+0x6C*4) // R/W - ZQ 3 Impedance Control Register 0
#define PUBL_ZQ3CR1         (PUBL_REG_BASE+0x6D*4) // R/W - ZQ 3 Impedance Control Register 1
#define PUBL_ZQ3SR0         (PUBL_REG_BASE+0x6E*4) // R   - ZQ 3 Impedance Status Register 0
#define PUBL_ZQ3SR1         (PUBL_REG_BASE+0x6F*4) // R   - ZQ 3 Impedance Status Register 1
#define PUBL_DX0GCR         (PUBL_REG_BASE+0x70*4) // R/W - DATX8 0 General Configuration Register
#define PUBL_DX0GSR0        (PUBL_REG_BASE+0x71*4) // R   - DATX8 0 General Status Register
#define PUBL_DX0GSR1        (PUBL_REG_BASE+0x72*4) // R   - DATX8 0 General Status Register 1
#define PUBL_DX0DLLCR       (PUBL_REG_BASE+0x73*4) // R   - DATX8 0 DLL Control Register
#define PUBL_DX0DQTR        (PUBL_REG_BASE+0x74*4) // R/W - DATX8 0 DQ Timing Register
#define PUBL_DX0DQSTR       (PUBL_REG_BASE+0x75*4) // R/W - DATX8 0 DQS Timing Register
#define PUBL_DX1GCR         (PUBL_REG_BASE+0x80*4) // R   - DATX8 1 General Configration Register
#define PUBL_DX1GSR0        (PUBL_REG_BASE+0x81*4) // R   - DATX8 1 General Status Register
#define PUBL_DX1GSR1        (PUBL_REG_BASE+0x82*4) // R   - DATX8 1 General Status Register
#define PUBL_DX1DLLCR       (PUBL_REG_BASE+0x83*4) // R   - DATX8 1 DLL Control Register
#define PUBL_DX1DQTR        (PUBL_REG_BASE+0x84*4) // R/W - DATX8 1 DQ Timing Register
#define PUBL_DX1DQSTR       (PUBL_REG_BASE+0x85*4) // R/W - DATX8 1 DQS Timing Register
#define PUBL_DX2GCR         (PUBL_REG_BASE+0x90*4) // R   - DATX8 2 General Configration Register
#define PUBL_DX2GSR0        (PUBL_REG_BASE+0x91*4) // R   - DATX8 2 General Status Register
#define PUBL_DX2GSR1        (PUBL_REG_BASE+0x92*4) // R   - DATX8 2 General Status Register
#define PUBL_DX2DLLCR       (PUBL_REG_BASE+0x93*4) // R   - DATX8 2 DLL Control Register
#define PUBL_DX2DQTR        (PUBL_REG_BASE+0x94*4) // R/W - DATX8 2 DQ Timing Register
#define PUBL_DX2DQSTR       (PUBL_REG_BASE+0x95*4) // R/W - DATX8 2 DQS Timing Register
#define PUBL_DX3GCR         (PUBL_REG_BASE+0xA0*4) // R   - DATX8 3 General Configration Register
#define PUBL_DX3GSR0        (PUBL_REG_BASE+0xA1*4) // R   - DATX8 3 General Status Register
#define PUBL_DX3GSR1        (PUBL_REG_BASE+0xA2*4) // R   - DATX8 3 General Status Register
#define PUBL_DX3DLLCR       (PUBL_REG_BASE+0xA3*4) // R   - DATX8 3 DLL Control Register
#define PUBL_DX3DQTR        (PUBL_REG_BASE+0xA4*4) // R/W - DATX8 3 DQ Timing Register
#define PUBL_DX3DQSTR       (PUBL_REG_BASE+0xA5*4) // R/W - DATX8 3 DQS Timing Register
#define PUBL_DX4GCR         (PUBL_REG_BASE+0xB0*4) // R   - DATX8 4 General Configration Register
#define PUBL_DX4GSR0        (PUBL_REG_BASE+0xB1*4) // R   - DATX8 4 General Status Register      
#define PUBL_DX4GSR1        (PUBL_REG_BASE+0xB2*4) // R   - DATX8 4 General Status Register      
#define PUBL_DX4DLLCR       (PUBL_REG_BASE+0xB3*4) // R   - DATX8 4 DLL Control Register         
#define PUBL_DX4DQTR        (PUBL_REG_BASE+0xB4*4) // R/W - DATX8 4 DQ Timing Register           
#define PUBL_DX4DQSTR       (PUBL_REG_BASE+0xB5*4) // R/W - DATX8 4 DQS Timing Register                                                   
#define PUBL_DX5GCR         (PUBL_REG_BASE+0xC0*4) // R   - DATX8 5 General Configration Register
#define PUBL_DX5GSR0        (PUBL_REG_BASE+0xC1*4) // R   - DATX8 5 General Status Register      
#define PUBL_DX5GSR1        (PUBL_REG_BASE+0xC2*4) // R   - DATX8 5 General Status Register      
#define PUBL_DX5DLLCR       (PUBL_REG_BASE+0xC3*4) // R   - DATX8 5 DLL Control Register         
#define PUBL_DX5DQTR        (PUBL_REG_BASE+0xC4*4) // R/W - DATX8 5 DQ Timing Register           
#define PUBL_DX5DQSTR       (PUBL_REG_BASE+0xC5*4) // R/W - DATX8 5 DQS Timing Register                                                   
#define PUBL_DX6GCR         (PUBL_REG_BASE+0xD0*4) // R   - DATX8 6 General Configration Register
#define PUBL_DX6GSR0        (PUBL_REG_BASE+0xD1*4) // R   - DATX8 6 General Status Register      
#define PUBL_DX6GSR1        (PUBL_REG_BASE+0xD2*4) // R   - DATX8 6 General Status Register      
#define PUBL_DX6DLLCR       (PUBL_REG_BASE+0xD3*4) // R   - DATX8 6 DLL Control Register         
#define PUBL_DX6DQTR        (PUBL_REG_BASE+0xD4*4) // R/W - DATX8 6 DQ Timing Register           
#define PUBL_DX6DQSTR       (PUBL_REG_BASE+0xD5*4) // R/W - DATX8 6 DQS Timing Register                    
#define PUBL_DX7GCR         (PUBL_REG_BASE+0xE0*4) // R   - DATX8 7 General Configration Register
#define PUBL_DX7GSR0        (PUBL_REG_BASE+0xE1*4) // R   - DATX8 7 General Status Register      
#define PUBL_DX7GSR1        (PUBL_REG_BASE+0xE2*4) // R   - DATX8 7 General Status Register      
#define PUBL_DX7DLLCR       (PUBL_REG_BASE+0xE3*4) // R   - DATX8 7 DLL Control Register         
#define PUBL_DX7DQTR        (PUBL_REG_BASE+0xE4*4) // R/W - DATX8 7 DQ Timing Register           
#define PUBL_DX7DQSTR       (PUBL_REG_BASE+0xE5*4) // R/W - DATX8 7 DQS Timing Register                                                   
#define PUBL_DX8GCR         (PUBL_REG_BASE+0xF0*4) // R   - DATX8 8 General Configration Register
#define PUBL_DX8GSR0        (PUBL_REG_BASE+0xF1*4) // R   - DATX8 8 General Status Register      
#define PUBL_DX8GSR1        (PUBL_REG_BASE+0xF2*4) // R   - DATX8 8 General Status Register      
#define PUBL_DX8DLLCR       (PUBL_REG_BASE+0xF3*4) // R   - DATX8 8 DLL Control Register         
#define PUBL_DX8DQTR        (PUBL_REG_BASE+0xF4*4) // R/W - DATX8 8 DQ Timing Register           
#define PUBL_DX8DQSTR       (PUBL_REG_BASE+0xF5*4) // R/W - DATX8 8 DQS Timing Register



/**---------------------------------------------------------------------------*
 **                            Extern Declare
 **---------------------------------------------------------------------------*/
#define IRQ_STACK_LIMIT	0x10
#define FIQ_STACK_LIMIT	0x10
#define SVC_STACK_LIMIT	0x200
#define SVC_HEAP_LIMIT	0x10


#define EMC_DLL_SWITCH_DISABLE_MODE		0x0
#define EMC_DLL_SWITCH_ENABLE_MODE		0x1
#define EMC_DLL_NOT_SWITCH_MODE			0x2
#define EMC_DLL_MODE_MASK			(0x3)
#define EMC_CLK_DIV_OFFSET			2
#define EMC_CLK_DIV_MASK			(0X3 << EMC_CLK_DIV_OFFSET)


#define EMC_FREQ_SWITCH_STATUS_OFFSET		(4)
#define EMC_FREQ_SWITCH_COMPLETE			(1)
#define EMC_FREQ_SWITCH_STATUS_MASK			(0xf << EMC_FREQ_SWITCH_STATUS_OFFSET)

#define EMC_DDR_TYPE_LPDDR1			0
#define EMC_DDR_TYPE_LPDDR2			1
#define EMC_DDR_TYPE_LPDDR3			2
#define EMC_DDR_TYPE_DDR2			3
#define EMC_DDR_TYPE_DDR3			4
#define EMC_DDR_TYPE_OFFSET			8
#define EMC_DDR_TYPE_MASK			(0xf << EMC_DDR_TYPE_OFFSET)

#define EMC_CLK_FREQ_OFFSET			(12)
#define EMC_CLK_FREQ_MASK			(0xfff << EMC_CLK_FREQ_OFFSET)

#define EMC_FREQ_NORMAL_SCENE			0x0 //normal lcd power off
#define EMC_FREQ_MP4_SENE			0x1 //play mp4 mode
#define EMC_FREQ_SENE_OFFSET			24

#define EMC_BSP_BPS_200_CLR			0x0
#define EMC_BSP_BPS_200_SET			0x1
#define EMC_BSP_BPS_200_NOT_CHANGE		0x2
#define EMC_BSP_BPS_200_OFFSET			28
#define EMC_BSP_BPS_200_MASK			(0xf << EMC_BSP_BPS_200_OFFSET)

#define  u32 unsigned int

#define PMU_APB_BASE	(0x022B0000)
#define AON_CKG_BASE	(0x022D0000)
#define AON_APB_CKG_BASE	(0x022E0000)
#define FLAG_ADDR	(0xFFC)
typedef unsigned char		BOOLEAN;
typedef unsigned char		uint8;
typedef unsigned short		uint16;
typedef unsigned long int	uint32;
typedef unsigned long int 	uint64;
typedef unsigned int		uint;

typedef signed char			int8;
typedef signed short		int16;
typedef signed long int		int32;
typedef struct
{
    uint32 ddr_clk;
    //umctl reg
    uint32 umctl2_rfshtmg;
    uint32 umctl2_init0;
    uint32 umctl2_init1;
    uint32 umctl2_init2;
    uint32 umctl2_init3;
    uint32 umctl2_init4;
    uint32 umctl2_init5;
    uint32 umctl2_dramtmg0;
    uint32 umctl2_dramtmg1;
    uint32 umctl2_dramtmg2;
    uint32 umctl2_dramtmg3;
    uint32 umctl2_dramtmg4;
    uint32 umctl2_dramtmg5;
    uint32 umctl2_dramtmg6;
    uint32 umctl2_dramtmg7;
    uint32 umctl2_dramtmg8;
    uint32 umctl2_dfitmg0;
    uint32 umctl2_dfitmg1;
    //publ reg
    uint32 publ_ptr0;
    uint32 publ_ptr1;
    uint32 publ_dtpr0;
    uint32 publ_dtpr1;
    uint32 publ_dtpr2;
    uint32 publ_mr0;
    uint32 publ_mr1;
    uint32 publ_mr2;
    uint32 publ_mr3;
    uint32 publ_dx0gcr;
    uint32 publ_dx1gcr;
    uint32 publ_dx2gcr;
    uint32 publ_dx3gcr;
    uint32 publ_dx0dqstr;
    uint32 publ_dx1dqstr;
    uint32 publ_dx2dqstr;
    uint32 publ_dx3dqstr;
}ddr_dfs_val_t;
ddr_dfs_val_t *timing;
static void assert_reset_dll(void)
{
	REG32(PUBL_ACDLLCR)  &= ~(0x1<<30);
	REG32(PUBL_DX0DLLCR) &= ~(0x1<<30);
	REG32(PUBL_DX1DLLCR) &= ~(0x1<<30);
	REG32(PUBL_DX2DLLCR) &= ~(0x1<<30);
	REG32(PUBL_DX3DLLCR) &= ~(0x1<<30);	
}

static void deassert_reset_dll(void)
{
	REG32(PUBL_ACDLLCR)  |= (0x1<<30);
	REG32(PUBL_DX0DLLCR) |= (0x1<<30);
	REG32(PUBL_DX1DLLCR) |= (0x1<<30);
	REG32(PUBL_DX2DLLCR) |= (0x1<<30);
	REG32(PUBL_DX3DLLCR) |= (0x1<<30);	
}

void disable_ddrphy_dll() {
	REG32(PUBL_PIR)      |= (1 << 17);
}

void enable_ddrphy_dll() {
	REG32(PUBL_PIR)      &= ~(1 << 17);
}

static void move_upctl_state_to_self_refresh(void)
{
	uint32 val;

	REG32(UMCTL_PWRCTL) = 0x8;
	val = REG32(UMCTL_STAT);
	//wait umctl2 core is in self-refresh mode
	while((val & 0x7) != 1) {
		val = REG32(UMCTL_STAT);
	}

	REG32(UMCTL_DFILPCFG0) = 0x0700f000;
//	val = REG32(UMCTL_STAT);
	//wait umctl2 core is in self-refresh mode
//	while((val & 0x7) != 3) {
//		val = REG32(UMCTL_STAT);
//	}
	//check self refresh was due to software
}
static void move_upctl_state_exit_self_refresh(u32 dll_enable)
{
	if(dll_enable == EMC_DLL_SWITCH_DISABLE_MODE) {
		REG32(UMCTL_DFILPCFG0) = 0x0700f100;
		REG32(UMCTL_PWRCTL)    = 0x9;
	}
	else{
//		REG32(UMCTL_DFILPCFG0) = 0x0700f000;
		REG32(UMCTL_PWRCTL)    = 0xa;
	}

//	val = REG32(UMCTL_STAT);
	//wait umctl2 core is in self-refresh mode
//	while((val & 0x7) != 1) {
//		val = REG32(UMCTL_STAT);
//	}

}

static void disable_cam_command_deque(void)
{
	REG32(UMCTL_DBG1) |= (1 << 0);
}
static void enable_cam_command_deque(void)
{
	REG32(UMCTL_DBG1) &= ~(1 << 0);
}
//bps200, control the DLL disbale mode. 0, DLL freq is below 100MHz, 1:DLL freq is in 100-200MHz
static void set_ddrphy_dll_bps200_mode(u32 bps200)
{
	if(bps200) {
		REG32(PUBL_DLLGCR) |= (1 << 23);
	}
	else {
		REG32(PUBL_DLLGCR) &= ~(1 << 23);
	}
}
static void ddr_clk_set(uint32 new_clk, uint32 delay);
static void ddr_timing_update(void);

static void switch_100_200(u32 bsp_200)
{
	volatile uint32 i;
	disable_cam_command_deque();
	for(i = 0 ; i < 5; i++);
	if(EMC_BSP_BPS_200_SET == bsp_200) {
		set_ddrphy_dll_bps200_mode(1);
		REG32(AON_CKG_BASE + 0x24) &= ~(3 << 8);
		ddr_timing_update();
	}
	else if(EMC_BSP_BPS_200_CLR == bsp_200){
		set_ddrphy_dll_bps200_mode(0);
		REG32(AON_CKG_BASE + 0x24) |= (1 << 8);
		ddr_timing_update();
	}
	for(i = 0 ; i < 2; i++);
	enable_cam_command_deque();
}
static void manual_issue_autorefresh()
{
	uint32 val;
	val = REG32(UMCTL_RFSHCTL3);
	val |= (1 << 0) ;
	val ^= (1 << 1);
	REG32(UMCTL_RFSHCTL3) = val;

	val = REG32(UMCTL_RFSHCTL3);
	val &= ~(1 << 0) ;
	val ^=  (1 << 1);
	REG32(UMCTL_RFSHCTL3) = val;
}
static void wait_queue_complete(void)
{
	volatile u32 i = 0;
	u32 value_temp;
	while(i < 5)
	{
		value_temp = REG32(UMCTL_DBG1);
		if(value_temp == 1) {
			i++;
		}
	}
}
#define AON_CLK_CONTROL_BASE	(0x022d0000)
#define CLK_EMC_CFG	(AON_CLK_CONTROL_BASE + 0x24)
static void ddr_timing_update(void);
static int emc_clk_select(uint32 new_clk, uint32 div)
{
	volatile uint32 i;
	uint32 clk_emc_div;
	uint32 clk_emc_sel;
	uint32 val;

	switch(new_clk){
	case 384:
		val = REG32(CLK_EMC_CFG);
		val &= ~(1 << 8);
		REG32(CLK_EMC_CFG) = val;
		val &= ~(0x3);
		val |= 2;
		REG32(CLK_EMC_CFG) = val;
		break;
	case 192:
		val = REG32(CLK_EMC_CFG);
		val |= (1 << 8);
		REG32(CLK_EMC_CFG) = val;
		val &= ~(0x3);
		val |= 2;
		REG32(CLK_EMC_CFG) = val;
		break;
	default:
		val = REG32(CLK_EMC_CFG);
		val &= ~(1 << 8);
		val |= (div << 8);
		REG32(CLK_EMC_CFG) = val;
		val |= 3;
		REG32(CLK_EMC_CFG) = val;
		break;
	}
//	val = (clk_emc_div << 8) | clk_emc_sel;
//	REG32(CLK_EMC_CFG) = val;
}
#if 0
void timer_init(void)
{
	REG32(0x40060018) = 0x820;
	REG32(0x40020040) = 0xffffffff;
	REG32(0x40020048) = 0xc0;
}
uint32 timer_get(void)
{
	return REG32(0x40020044);
}
#endif
static void umctl2_clk_switch(uint32 clk, uint32 DDR_TYPE, uint32 dll_mode, uint32 div)
{
	volatile uint32 i;
	//uint32 time_before;
	//time_before = timer_get();
	move_upctl_state_to_self_refresh();
	disable_cam_command_deque();
	REG32(UMCTL_DFIMISC) &= ~(1 << 0);
	wait_queue_complete();
	//disable auto refresh
	REG32(UMCTL_RFSHCTL3) |= (1 << 0);
	for(i = 0 ; i < 2; i++);
	emc_clk_select(clk, div);
	for(i = 0 ; i < 2; i++);

	//phy clock close
//	REG32(PMU_APB_BASE + 0xc8) &= ~(1 << 2);
	REG32(PMU_APB_BASE + 0xc8) &= ~(1 << 6);

	for(i = 0 ; i < 2; i++);
	if(dll_mode == EMC_DLL_SWITCH_DISABLE_MODE) {
		disable_ddrphy_dll();
		//phy clock open
//		REG32(PMU_APB_BASE + 0xc8) |= (1 << 2);
		REG32(PMU_APB_BASE + 0xc8) |= (1 << 6);
		for(i = 0 ; i < 2; i++);
//		deassert_reset_dll();
		REG32(PUBL_PIR) |= (1 << 4) | (1 << 0);
		while((REG32(PUBL_PGSR) & 1) != 1);
		while((REG32(PUBL_PGSR) & 1) != 1);
//		REG32(PUBL_PIR) |= (1 << 4);
		REG32(UMCTL_PERFLPR1) &= ~0xFFFF;
		REG32(UMCTL_PERFWR1) &= ~0xFFFF;
	}
	else if((dll_mode == EMC_DLL_SWITCH_ENABLE_MODE) || (dll_mode == EMC_DLL_NOT_SWITCH_MODE)){

		assert_reset_dll();
		for(i = 0 ; i < 2; i++);
		enable_ddrphy_dll();
		//phy clock open
//		REG32(PMU_APB_BASE + 0xc8) |= (1 << 2);
		REG32(PMU_APB_BASE + 0xc8) |= (1 << 6);
		for(i = 0 ; i < 2; i++);
		deassert_reset_dll();
		for(i = 0 ; i < 10; i++);
		REG32(UMCTL_PERFLPR1) |= 0x100;
		REG32(UMCTL_PERFWR1) |= 0x20;
	}
	//enable auto refresh
	REG32(UMCTL_RFSHCTL3) &= ~(1 << 0);
	ddr_timing_update();
	REG32(UMCTL_DFIMISC) |= (1 << 0);
	enable_cam_command_deque();
	move_upctl_state_exit_self_refresh(dll_mode);
	//REG32(FLAG_ADDR - 8) = time_before - timer_get();
}
void umctl2_freq_set(uint32 clk, uint32 DDR_TYPE, uint32 dll_mode,uint32 div)
{
	umctl2_clk_switch(clk, DDR_TYPE, dll_mode, div);
	return;
}
#define CP2_PARAM_ADDR	(0xF00)
static void ddr_timing_update(void)
{
	REG32(UMCTL_RFSHTMG)  = timing->umctl2_rfshtmg;
	REG32(UMCTL_RFSHCTL3) ^= (1 << 1);
	REG32(UMCTL_DRAMTMG0) = timing->umctl2_dramtmg0;
	REG32(UMCTL_DRAMTMG1) = timing->umctl2_dramtmg1;
	REG32(UMCTL_DRAMTMG2) = timing->umctl2_dramtmg2;
	//REG32(UMCTL_DRAMTMG3) = timing->umctl2_dramtmg3;
	REG32(UMCTL_DRAMTMG4) = timing->umctl2_dramtmg4;
	//REG32(UMCTL_DRAMTMG5) = timing->umctl2_dramtmg5;
	REG32(UMCTL_DRAMTMG6) = timing->umctl2_dramtmg6;
	//REG32(UMCTL_DRAMTMG7) = timing->umctl2_dramtmg7;
	REG32(UMCTL_DRAMTMG8) = 1;
	REG32(PUBL_DX0DQSTR) = timing->publ_dx0dqstr;
	REG32(PUBL_DX1DQSTR) = timing->publ_dx1dqstr;
	REG32(PUBL_DX2DQSTR) = timing->publ_dx2dqstr;
	REG32(PUBL_DX3DQSTR) = timing->publ_dx3dqstr;
	REG32(PUBL_DX0GCR) = timing->publ_dx0gcr;
	REG32(PUBL_DX1GCR) = timing->publ_dx1gcr;
	REG32(PUBL_DX2GCR) = timing->publ_dx2gcr;
	REG32(PUBL_DX3GCR) = timing->publ_dx3gcr;
}

void SLEEP_Init(void)
{
	#define APB_SLP_CTL0								(0x40060030)
	#define APB_FORCE_ON								(1 << 2)
	#define APB_FORCE_SLEEP							(1 << 1)
	
	#define AHB_SLP_CTL0								(0x604000D0)
	#define APB_STOP										(1 << 5)
	#define MCU_FORCE_SYS_SLP						(1 << 0)
	#define MCU_CORE_SLP								(1 << 1)
	
	#define AHB_SLP_CTL1								(0x604000D4)
	#define MCU_SYS_SLP_EN							(1 << 3)
	#define MCU_DEEP_SLP_EN							(1 << 4)
	#define MCU_LIGHT_SLP_EN						(1 << 8)
	
	#define PD_CP_ARM9_CFG							0x022B0054
	#define PD_CP_ARM9_AUTO_SHUTDOWN_EN	(1 << 24)
	#define PD_CP_WIFI_CFG							0x022B0058
	#define PD_CP_WIFI_AUTO_SHUTDOWN_EN	(1 << 24)
	
	#define APB_EB0_STS									0x40060018
	#define APB_EB0_MSK									0x01FBFFFC
	
	#define AHB_EB0_STS									0x60400008
	#define AHB_EB0_MSK									0x0000000F
	
//	REG32(APB_SLP_CTL0) |= APB_FORCE_SLEEP;
	
//	REG32(AHB_SLP_CTL1) |= (MCU_SYS_SLP_EN | MCU_LIGHT_SLP_EN | MCU_DEEP_SLP_EN);
	
	REG32(PD_CP_ARM9_CFG) |= PD_CP_ARM9_AUTO_SHUTDOWN_EN;
	REG32(PD_CP_WIFI_CFG) |= PD_CP_WIFI_AUTO_SHUTDOWN_EN;
	
	REG32(APB_EB0_STS)		&= ~APB_EB0_MSK;
	REG32(AHB_EB0_STS)    &= ~AHB_EB0_MSK;
	
}
#define INTC0_REG_BASE		0x40000000
#define TB_AP2CP_INT0		0xa
#define	TB_AP2CP_INT1		0xb
void main_entry(void)
{
	
	uint32 ddr_type;
	uint32 clk;
	uint32 reg_val;
	uint32 dll_mode;
	uint32 div;
	volatile uint32 i;

	//enable ipi interrupt
	REG32(INTC0_REG_BASE + 0x8) = (1 << TB_AP2CP_INT0);

	REG32(0x40060044) |= (1 << 0);//AHB clk div set to 1
	for(i = 0; i < 20; i++);
	REG32(0x40060040) |= (3 <<5);//ARM clk set to 312M
	REG32(FLAG_ADDR - 4) = 0X11223344;
	SLEEP_Init();
	while(1){
		REG32(AHB_SLP_CTL1) |= (MCU_SYS_SLP_EN | MCU_LIGHT_SLP_EN | MCU_DEEP_SLP_EN);
		WaitForInt();
		//timer_init();
		REG32(FLAG_ADDR - 4) = 0x11223345;
		REG32(0x400E0004) = 0x1;//clear ipi int0
		reg_val = REG32(FLAG_ADDR);
		ddr_type = (reg_val & EMC_DDR_TYPE_MASK) >> EMC_DDR_TYPE_OFFSET;
		clk = (reg_val & EMC_CLK_FREQ_MASK) >> EMC_CLK_FREQ_OFFSET;
		dll_mode = (reg_val & EMC_DLL_MODE_MASK);
		div = (reg_val & EMC_CLK_DIV_MASK) >> EMC_CLK_DIV_OFFSET;
		REG32(PUBL_DSGCR) &= ~(0x10);
		timing = (ddr_dfs_val_t *)(CP2_PARAM_ADDR);
		umctl2_freq_set(clk, ddr_type, dll_mode, div);
		REG32(FLAG_ADDR) |= (EMC_FREQ_SWITCH_COMPLETE << EMC_FREQ_SWITCH_STATUS_OFFSET);
		REG32(FLAG_ADDR - 4) = 0x11223367;

	}

}