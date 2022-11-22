#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/errno.h>
//#include "clock_common.h"
//#include "clock_sc8810.h"
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/kthread.h>

#include <asm/cacheflush.h>
#include <linux/irqchip/arm-gic.h>
#include <asm/irqflags.h>

#include <mach/adi.h>
#include <mach/pm_debug.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>

#include "emc_repower.h"

#define REG32(x)             (*((volatile u32 *)(x)))
#define BASE_ADDR_AHBREG     (0x20900200)
#define ADDR_AHBREG_ARMCLK   (BASE_ADDR_AHBREG+0x0024)
#define ADDR_AHBREG_AHB_CTRL0 (BASE_ADDR_AHBREG+0x0000)
#define GLB_REG_WR_REG_GEN1  (0x4B000018)
#define GLB_REG_DPLL_CTRL    (0x4B000040)

#define DQSR_VALUE        4
#define GATE_TRAING_ON    1
#define GATE_EARLY_LATE   2
//#define mem_drv         34
#define EMC_FREQ          200   //ddr clock
#define uint32 u32
//#define EMC_SINGLE_CS         //single ddr cs

//const static u32 mem_drv  = 34;

#define ANALOG_DIE_REG_BASE  0x42000600
#define ANALOG_DCDC_CTRL_CAL 0x50
#define ANALOG_DCDC_CTRL_DS  0x4C
static inline void modify_reg_field(u32 addr, u32 start_bit, u32 bit_num, u32 value);
//=====================================================================================
//CAUTIONS:
//There are some configuration restrictions for SNPS uMCTL/uPCTL controller
//refer to P287 section M1.7 for details. Also copied here for notification
//- Memory burst type sequential is not supported for mDDR and LPDDR2 with BL8.
//- Memory burst type interleaved is not supported for mDDR with BL16.
//Also, Must read M2.2.10.5 Considerations for Memory Initialization
//=====================================================================================
#define UMCTL_REG_BASE     0x60200000
#define PUBL_REG_BASE      0x60201000
#define EMC_MEM_BASE_ADDR  0x80000000
#define _LPDDR1_BL_        8 //2,4,8,16. seems must be 8 now
#define _LPDDR1_BT_        1 //0: sequential
//1: interleaving
#define _LPDDR1_CL_        3 //2,3,4
#define _LPDDR2_BL_        4 //4,8,16
#define _LPDDR2_BT_        0 //0: sequential
//1: interleaving
#define _LPDDR2_WC_        1 //0: wrap
//1: no wrap

typedef enum  { Init_mem = 0, Config = 1, Config_req = 2, Access = 3, Access_req = 4, Low_power = 5,
		Low_power_entry_req = 6, Low_power_exit_req = 7
              } uPCTL_STATE_ENUM;
typedef enum  {INIT = 0, CFG = 1, GO = 2, SLEEP = 3, WAKEUP = 4} uPCTL_STATE_CMD_ENUM;
typedef enum  {LPDDR2, LPDDR1, DDR2, DDR3} MEM_TYPE_ENUM;
typedef enum  {MEM_64Mb, MEM_128Mb, MEM_256Mb, MEM_512Mb, MEM_1024Mb, MEM_2048Mb, MEM_4096Mb, MEM_8192Mb} MEM_DENSITY_ENUM;
typedef enum  {X8, X16, X32} MEM_WIDTH_ENUM;
typedef enum  {LPDDR2_S2, LPDDR2_S4} MEM_COL_TYPE_ENUM;
typedef enum  {BL2 = 2, BL4 = 4, BL8 = 8, BL16 = 16} MEM_BL_ENUM;
typedef enum  {SEQ, INTLV} MEM_BT_ENUM;

typedef enum
{
	SDLL_PHS_DLY_MIN = 0,
	SDLL_PHS_DLY_DEF = 0,
	SDLL_PHS_DLY_36  = 0,
	SDLL_PHS_DLY_54  = 1,
	SDLL_PHS_DLY_72  = 2,
	SDLL_PHS_DLY_90  = 3,
	SDLL_PHS_DLY_108 = 4,
	SDLL_PHS_DLY_126 = 5,
	SDLL_PHS_DLY_144 = 6,
	SDLL_PHS_DLY_MAX = 6,
}SDLL_PHS_DLY_E;

typedef enum
{
	DQS_PHS_DLY_MIN = 0,
	DQS_PHS_DLY_90  = 0,
	DQS_PHS_DLY_180 = 1,
	DQS_PHS_DLY_DEF = 1,
	DQS_PHS_DLY_270 = 2,
	DQS_PHS_DLY_360 = 3,
	DQS_PHS_DLY_MAX = 3,
}DQS_PHS_DLY_E;

typedef enum
{
	DXN_MIN = 0,
	DXN0    = 0,
	DXN1    = 1,
	DXN2    = 2,
	DXN3    = 3,
	DXN_MAX = 3,
}DXN_E;

//#define MEMORY_TYPE    LPDDR2   //typedef enum int {LPDDR2,LPDDR1,DDR3} MEM_TYPE_ENUM;
//#define MEM_WIDTH      X32

//umctl/upctl registers declaration//{{{
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
#define PUBL_CFG_ADD_ZQ0SR0        (0x62 * 4) // R/W - ZQ 0 Impedance Status Register 0
#define PUBL_CFG_ADD_ZQ0SR1        (0x63 * 4) // R/W - ZQ 0 Impedance Status Register 1
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
#define EMC_TRAINING_ERR	"emc_training error"
#define EMC_TRAINING_SUCESS	"emc training ok"


static inline void reset_ddrphy_dll(void);
static inline void __emc_init_repowered(u32 power_off, struct emc_repower_param *param, u32 clk_emc_div);
static inline void uart_init(void)
{
	REG32(0x4b000008) |= (1 << 20) | (1 << 21);
}
static inline void uart_trace(u8 *ch, u32 len)
{
	u32 i;
	for(i = 0 ;i < len; i++)
	{
		REG32(0x44000000) = ch[i];
	}
}
static inline void write_upctl_state_cmd(uPCTL_STATE_CMD_ENUM cmd)
{
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_SCTL) = cmd;
}

static inline void poll_upctl_state (uPCTL_STATE_ENUM state)
{
	uPCTL_STATE_ENUM state_poll;
	u32 value_temp;
	do
	{
		value_temp = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_STAT);
		state_poll = value_temp & 0x7;
	}
	while(state_poll != state);
	return;
}

static inline void wait_n_pclk_cycle(u32 num)
{
	volatile u32 i;
	u32 value_temp;
	for(i=0;i<num;i++)
	{
			value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PGSR);
	}
}
static inline void move_upctl_state_to_initmem(void)
{
	uPCTL_STATE_ENUM upctl_state;
	//uPCTL_STATE_CMD_ENUM  upctl_state_cmd;
	u32 tmp_val ;
	//tmp_val = upctl_state ;
	tmp_val = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_STAT);
	//while(upctl_state!= Init_mem) {
	while((tmp_val & 0x7) != Init_mem)
	{
		switch((tmp_val & 0x7))
		{
			case Config:
				{
					write_upctl_state_cmd(INIT);
					poll_upctl_state(Init_mem);
					upctl_state = Init_mem;
					tmp_val = upctl_state ;
					break;
				}
			case Access:
				{
					write_upctl_state_cmd(CFG);
					poll_upctl_state(Config);
					upctl_state = Config;
					tmp_val = upctl_state ;
					break;
				}
			case Low_power:
				{
					write_upctl_state_cmd(WAKEUP);
					poll_upctl_state(Access);
					upctl_state = Access;
					tmp_val = upctl_state ;
					break;
				}
			default://transitional state
				{
					tmp_val = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_STAT);
					break;
				}
		}
	}
}

static inline void move_upctl_state_to_config(void)
{
	uPCTL_STATE_ENUM upctl_state;
	//uPCTL_STATE_CMD_ENUM  upctl_state_cmd;
	u32  tmp_val ;
	tmp_val = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_STAT);
	upctl_state = tmp_val & 0x7;
	while(upctl_state != Config)
	{

		switch(upctl_state)
		{
			case Low_power:
				{
					write_upctl_state_cmd(WAKEUP);
					poll_upctl_state(Access);
					upctl_state = Access;
					break;
				}
			case Init_mem:
				{
					write_upctl_state_cmd(CFG);
					poll_upctl_state(Config);
					upctl_state = Config;
					break;
				}
			case Access:
				{
					write_upctl_state_cmd(CFG);
					poll_upctl_state(Config);
					upctl_state = Config;
					break;
				}
			default://transitional state
				{
					tmp_val = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_STAT);
					upctl_state = tmp_val & 0x7;
				}
		}
	}
}

static inline void move_upctl_state_to_low_power(void)
{
	uPCTL_STATE_ENUM upctl_state;
	//uPCTL_STATE_CMD_ENUM  upctl_state_cmd;
	u32  tmp_val ;
	tmp_val = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_STAT);
	upctl_state = tmp_val & 0x7;
	while(upctl_state != Low_power)
	{
		switch(upctl_state)
		{
			case Access:
				{
					write_upctl_state_cmd(SLEEP);
					poll_upctl_state(Low_power);
					upctl_state = Low_power;
					break;
				}
			case Config:
				{
					write_upctl_state_cmd(GO);
					poll_upctl_state(Access);
					upctl_state = Access;
					break;
				}
			case Init_mem:
				{
					write_upctl_state_cmd(CFG);
					poll_upctl_state(Config);
					upctl_state = Config;
					break;
				}
			default://transitional state
				{
					tmp_val = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_STAT);
					upctl_state = tmp_val & 0x7;
				}
		}
	}
}


static inline void move_upctl_state_to_access(void)
{
	uPCTL_STATE_ENUM upctl_state;
	u32  tmp_val ;
	tmp_val = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_STAT);
	upctl_state = tmp_val & 0x7;

	while(upctl_state != Access)
	{
		switch(upctl_state)
		{
			case Access:
				{
					break;
				}
			case Config:
				{
					write_upctl_state_cmd(GO);
					poll_upctl_state(Access);
					upctl_state = Access;
					break;
				}
			case Init_mem:
				{
					write_upctl_state_cmd(CFG);
					poll_upctl_state(Config);
					upctl_state = Config;
					break;
				}
			case Low_power:
				{
					write_upctl_state_cmd(WAKEUP);
					poll_upctl_state(Access);
					upctl_state = Access;
					break;
				}
			default://transitional state
				{
					tmp_val = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_STAT);
					upctl_state = tmp_val & 0x7;
				}
		}
	}
}

static inline void emc_publ_do_gate_training(void)
{
	u32  value_temp;
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PIR) = 0x81;

	//check data training done
	//according to PUBL databook on PIR operation.
	//10 configuration clock cycle must be waited before polling PGSR
	//repeat(10)@(posedge `HIER_ARM_SYS.u_sys_wrap.u_DWC_ddr3phy_pub.pclk);
	wait_n_pclk_cycle(5);
	do
		value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PGSR);
	while((value_temp & 0x10) != 0x10);
	if((value_temp&(0x3 << 5)) != 0x0)
	{
		if((value_temp & 0x40) != 0)
		{
			//uart_trace(EMC_TRAINING_ERR, sizeof(EMC_TRAINING_ERR));
			while(1);
		}
		if((value_temp & 0x20) != 0)
		{
			//uart_trace(EMC_TRAINING_ERR, sizeof(EMC_TRAINING_ERR));
			while(1);
		}
	}
	value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX0GSR0);
	value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX1GSR0);
	value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX2GSR0);
	value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX3GSR0);
	//uart_trace(EMC_TRAINING_SUCESS, sizeof(EMC_TRAINING_SUCESS));
}

static inline void enable_lpddr12_low_power_feature(uint32 clk_stop_idle, uint32 pd_idle, uint32 sr_idle){
        REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_MCFG) &= ~( (0xff<<24) | (0xff<<8) );
        REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_MCFG) |= ( (clk_stop_idle <<24) | (pd_idle <<8) );
        REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_MCFG1) &= ~0xff;
        REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_MCFG1) |= sr_idle;
}
//stall NIF from application port
static inline void enable_umctl_dtu(void) {
	REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_DTUCFG) |= 0x1;
}
//grant NIF to application port
static inline void disable_umctl_dtu(void) {
	REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_DTUCFG) &= ~0x1;
}
static inline void emc_init_common_reg(struct emc_repower_param *param)
{
	u32  value_temp;
	u32 TOGCNT100N;
	u32 TOGCNT1U;
	MEM_TYPE_ENUM mem_type_enum;
	MEM_WIDTH_ENUM mem_width_enum;
	mem_type_enum = param->mem_type;
	mem_width_enum = param->mem_width;

	if(mem_type_enum == LPDDR1) {
		value_temp = (1 << 31) | (1 << 28) | (0xc << 5) | (0xc); //zq power down and override
		REG32(PUBL_REG_BASE + PUBL_CFG_ADD_ZQ0CR0) = value_temp;
	}
	//program common registers for ASIC/FPGA
	//Memory Timing Configuration Registers
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_SCFG) = 0x00000420;
	value_temp = 0x00e60041; //no auto low power enabled
	value_temp &= ~(0x1 << 17);
	value_temp &= ~(0x3 << 22);
	value_temp &= ~(0x3 << 20);

	//int i;
	//for(i=0; i<0x80000000; i++);
	switch(mem_type_enum)
	{
		case LPDDR1:
			{
				value_temp |= (0x2 << 22); //LPDDR1 enabled
				value_temp |= (_LPDDR1_BL_ == 2  ? 0x0 :
						_LPDDR1_BL_ == 4  ? 0x1 :
						_LPDDR1_BL_ == 8  ? 0x2 :
						_LPDDR1_BL_ == 16 ? 0x3 : 0x0) << 20;
				value_temp |= (0x1 << 17); //pd_exit_mode
				break;
			}
		case LPDDR2:
			{
				value_temp |= 0x3 << 22; //LPDDR2 enabled
				value_temp |= (_LPDDR2_BL_ == 4  ? 0x1 :
						_LPDDR2_BL_ == 8  ? 0x2 :
						_LPDDR2_BL_ == 16 ? 0x3 : 0x2) << 20;
				value_temp |= (0x1 << 17); //pd_exit_mode
				break;
			}
		default:
			while(1);
	}
	value_temp &= ~(0xff << 24); //Clock stop idle period in n_clk cycles
	value_temp &= ~(0xff << 8); //Power-down idle period in n_clk cycles
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_MCFG) =        value_temp;

	//sr_idle
	value_temp = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_MCFG1);
	value_temp &= ~0xff; //
	value_temp &= ~(0xff << 16); //
	//value_temp[7:0] = 0x7D; //125*32=4000 cycles
	value_temp |= 0x00; //
	value_temp |= (0x00 << 16); //hw_idle
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_MCFG1) =       value_temp;

	value_temp = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_PPCFG);
	value_temp &= ~0x1;
	switch(mem_width_enum)
	{
		case X16:
			{
				value_temp |= 0x1;    //enable partial populated memory
				break;
			}
		case X8:
		default:
			break;
	}
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_PPCFG) =       value_temp;

	if(0)
	{
		TOGCNT100N = 100;
		TOGCNT1U = 1000;
	}
	else
	{
		switch(param->emc_freq) {
			case 100:
				TOGCNT100N = 0xa;
				TOGCNT1U = 0x64;
				break;
			case 200:
				TOGCNT100N = 0x14;
				TOGCNT1U = 0xc8;
				break;
			case 400:
				TOGCNT100N = 0x28;
				TOGCNT1U = 0x190;
				break;
			default:
				while(1);
		}
	}

	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TOGCNT1U) =    TOGCNT1U;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TOGCNT100N) =  TOGCNT100N;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TRFC) =        0x00000034;

	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TINIT) =       0x000000c8;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TRSTH) =       0x00000000;

	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TREFI) =       0x00000027;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TMRD) =        0x00000005;


	value_temp = (mem_type_enum == LPDDR2 ) ? 0x00010008 : 0x00000004 ; //compenstate for clk jitter
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TRP) =         value_temp;

	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TRTW) =        0x00000003;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TAL) =         0x00000000; //no al for lpddr1/lpddr2

	value_temp = (mem_type_enum == LPDDR2 ) ? 6 : 3;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TCL) =         value_temp;

	value_temp = (mem_type_enum == LPDDR2 ) ? 3 : 1;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TCWL) =        value_temp;


	value_temp = (mem_type_enum == LPDDR2 ) ? 0x00000011 : 0x00000008;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TRAS) =        value_temp;
	value_temp = (mem_type_enum == LPDDR2 ) ? 0x00000019 : 0x0000000c;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TRC) =         value_temp;

	value_temp = (mem_type_enum == LPDDR2 ) ? 0x00000008 : 0x00000003; //compenstate for clk jitter
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TRCD) =        value_temp;
	value_temp = (mem_type_enum == LPDDR2 ) ? 0x00000004 : 0x00000002;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TRRD) =        value_temp;
	value_temp = (mem_type_enum == LPDDR2 ) ? 0x00000003 : 0x00000001;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TRTP) =        value_temp;
	value_temp = (mem_type_enum == LPDDR2 ) ? 0x00000006 : 0x00000003;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TWR) =         value_temp;

	//xiaohui change from 3 to 4 due to tWTR=7.5ns whereas clk_emc=2.348ns
	value_temp = (mem_type_enum == LPDDR2 ) ? 0x00000003 : 0x00000002;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TWTR) =        value_temp;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TEXSR) =       0x00000038;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TXP) =         0x00000004;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TXPDLL) =      0x00000000;
	//xiaohui change from 0x24 to 0x27 due to tZQCS=90ns whereas clk_emc=2.348ns
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TZQCS) =       0x00000024;

	//value_temp = mem_type_enum == LPDDR2 ? 0x5 : 0x0;
	value_temp = 0;//assume 0.4s need one calibration, see samsung lpddr2 on page 140

	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TZQCSI) =      value_temp;

	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TDQS) =        0x00000001;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TCKSRE) =      0x00000000;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TCKSRX) =      0x00000002;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TCKE) =        0x00000003;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TMOD) =        0x00000000;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TRSTL) =       0x00000002;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TZQCL) =       0x00000090;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TCKESR) =      0x00000006;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_TDPD) =        0x00000001;

	//default value not compatible with real condition
	value_temp = 0x00000011;
	value_temp |= 1 << 8; //addr_map: bank based
	value_temp &= ~(0xf << 2); //density
	value_temp |= 0x1 << 6;   //check:!!dram_type:1: LPDDR2 S4 or MDDR row width 13 and col width 10, 0:    LPDDR2 S2 or MDDR row width 14 and col width 9
	switch(param->cs0_size)
	{
		case 1024*1024*1024:
			value_temp |= (0x7 << 2);
			break;
		case 512*1024*1024:
			value_temp |= (0x6 << 2);
			break;
		case 256*1024*1024:
			value_temp |= (0x5 << 2);
			break;
		case 128*1024*1024:
			value_temp |= (0x4 << 2);
			break;
		case 64*1024*1024:
			value_temp |= (0x3 << 2);
			break;
		case 32*1024*1024:
			value_temp |= (0x2 << 2);
			break;
		case 16*1024*1024:
			value_temp |= (0x1 << 2);
			break;
		case 8*1024*1024:
			value_temp |= (0x0 << 2);
			break;
		default:
			value_temp |= (0x6 << 2);
			break;
	}
	value_temp |= (mem_width_enum == X32) ? 0x3 :
		(mem_width_enum == X16) ? 0x2 :
		0x0;  //dram_io_width
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_DCFG_CS0) = value_temp;

	value_temp &= ~(0xf << 2); //density
	switch(param->cs1_size)
	{
		case 1024*1024*1024:
			value_temp |= (0x7 << 2);
			break;
		case 512*1024*1024:
			value_temp |= (0x6 << 2);
			break;
		case 256*1024*1024:
			value_temp |= (0x5 << 2);
			break;
		case 128*1024*1024:
			value_temp |= (0x4 << 2);
			break;
		case 64*1024*1024:
			value_temp |= (0x3 << 2);
			break;
		case 32*1024*1024:
			value_temp |= (0x2 << 2);
			break;
		case 16*1024*1024:
			value_temp |= (0x1 << 2);
			break;
		case 8*1024*1024:
			value_temp |= (0x0 << 2);
			break;
		default:
			value_temp |= (0x6 << 2);
			break;
	}
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_DCFG_CS1) = value_temp;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_DFITPHYWRDATA) = 0x00000001;

	value_temp = (mem_type_enum == LPDDR2 ) ? 3 : 0; //WL-1, see PUBL P143
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_DFITPHYWRLAT) = value_temp;

	switch(mem_type_enum)
	{
		case LPDDR2:
			{
				value_temp = 5;
				break;
			};//??? RL-2, see PUBL P143
		case LPDDR1:
			{
				value_temp = 1;
				break;
			};
		default:
			break;//$stop(2);
	}
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_DFITRDDATAEN) = value_temp;

	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_DFITPHYRDLAT) = 0x0000000f;

	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_DFISTCFG0) =   0x00000007;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_DFISTCFG1) =   0x00000003;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_DFISTCFG2) =   0x00000003;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_DFILPCFG0) =   0x00078101; //dfi_lp_wakeup_sr=8

	//the priority settings is DSP > Disp > others > GPU
	// so DSP/Disp/Camera are set to LL and all others set to BE
	value_temp = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_PCFG_0);
	value_temp &= ~(0xff << 0);
	value_temp &= ~(0xff << 16);
	value_temp |= (0x10 << 16); //quantum
	value_temp |= (0x0 << 7); //rdwr_ordered
	value_temp |= (0x1 << 6); //st_fw_en
	value_temp |= (0x1 << 5); //bp_rd_en, as per the coreconsultant
	value_temp |= (0x1 << 4); //bp_wr_en
	//value_temp[1:0] = 0x3; //qos dynamic mode by port signal
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_PCFG_0) =      value_temp;

	value_temp = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_PCFG_1);
	value_temp &= ~(0xff << 0);
	value_temp &= ~(0xff << 16);
	value_temp |= (0x10 << 16); //quantum
	value_temp |= (0x0 << 7); //rdwr_ordered
	value_temp |= (0x1 << 6); //st_fw_en
	value_temp |= (0x1 << 5); //bp_rd_en, as per the coreconsultant
	value_temp |= (0x1 << 4); //bp_wr_en
	//value_temp[1:0] = 0x3; //qos dynamic mode by port signal
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_PCFG_1) =      value_temp;

	value_temp = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_PCFG_2);
	value_temp &= ~(0xff << 0);
	value_temp &= ~(0xff << 16);
	value_temp |= (0x10 << 16); //quantum
	value_temp |= (0x1 << 7); //rdwr_ordered
	value_temp |= (0x1 << 6); //st_fw_en
	value_temp |= (0x1 << 5); //bp_rd_en, as per the coreconsultant
	value_temp |= (0x1 << 4); //bp_wr_en
	//value_temp[1:0] = 0x3; //qos dynamic mode by port signal
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_PCFG_2) =      value_temp;

	value_temp = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_PCFG_3);
	value_temp &= ~(0xff << 0);
	value_temp &= ~(0xff << 16);
	value_temp |= (0x10 << 16); //quantum
	value_temp |= (0x1 << 7); //rdwr_ordered
	value_temp |= (0x1 << 6); //st_fw_en
	value_temp |= (0x1 << 5); //bp_rd_en, as per the coreconsultant
	value_temp |= (0x1 << 4); //bp_wr_en
	value_temp |= 1;	//qos:LL
	//value_temp[1:0] = 0x3; //qos dynamic mode by port signal
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_PCFG_3) =      value_temp;

	value_temp = REG32( UMCTL_REG_BASE + UMCTL_CFG_ADD_PCFG_4);
	value_temp &= ~(0xff << 0);
	value_temp &= ~(0xff << 16);
	value_temp |= (0x10 << 16); //quantum
	value_temp |= (0x1 << 7); //rdwr_ordered
	value_temp |= (0x1 << 6); //st_fw_en
	value_temp |= (0x1 << 5); //bp_rd_en, as per the coreconsultant
	value_temp |= (0x1 << 4); //bp_wr_en
	value_temp |= 1;	//qos:LL
	//value_temp[1:0] = 0x3; //qos dynamic mode by port signal
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_PCFG_4) =      value_temp;

	value_temp = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_PCFG_5);
	value_temp &= ~(0xff << 0);
	value_temp &= ~(0xff << 16);
	value_temp |= (0x10 << 16); //quantum
	value_temp |= (0x0 << 7); //rdwr_ordered
	value_temp |= (0x1 << 6); //st_fw_en
	value_temp |= (0x1 << 5); //bp_rd_en, as per the coreconsultant
	value_temp |= (0x1 << 4); //bp_wr_en
	value_temp |= 1;	//qos:LL
	//value_temp[1:0] = 0x3; //qos dynamic mode by port signal
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_PCFG_5) =      value_temp;

	value_temp = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_PCFG_6);
	value_temp &= ~(0xff << 0);
	value_temp &= ~(0xff << 16);
	value_temp |= (0x10 << 16); //quantum
	value_temp |= (0x0 << 7); //rdwr_ordered
	value_temp |= (0x1 << 6); //st_fw_en
	value_temp |= (0x1 << 5); //bp_rd_en, as per the coreconsultant
	value_temp |= (0x1 << 4); //bp_wr_en
	value_temp |= 1;//qos:LL
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_PCFG_6) =      value_temp;

	value_temp = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_PCFG_7);
	value_temp &= ~(0xff << 0);
	value_temp &= ~(0xff << 16);
	value_temp |= (0x10 << 16); //quantum
	value_temp |= (0x1 << 7); //rdwr_ordered
	value_temp |= (0x1 << 6); //st_fw_en
	value_temp |= (0x1 << 5); //bp_rd_en, as per the coreconsultant
	value_temp |= (0x1 << 4); //bp_wr_en
	//value_temp[1:0] = 0x3; //qos dynamic mode by port signal
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_PCFG_7) =      value_temp;

	//REG32(PUBL_REG_BASE+PUBL_CFG_ADD_DTAR) = (0x7<<28)|(0x3fff<<12)|(0x3f0<<0);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DTAR) = (0x0 << 28) | (0x0 << 12) | (0x0 << 0);

	value_temp = REG32(PUBL_REG_BASE+PUBL_CFG_ADD_ZQ0CR1);
	switch(mem_type_enum)
	{
		case LPDDR2:
			value_temp &= ~(0xf << 4);
			break;
		case LPDDR1:
			value_temp &= ~(0xf << 4);
			break;
		default:
			while(1);
	}


	value_temp &= ~0xf;

	switch (param->mem_drv)
	{
		case 34:
			value_temp |= 0xD;
			break;
		case 40:
			value_temp |= 0xB;
			break;
		case 48:
			value_temp |= 9;
			break;
		case 60:
			value_temp |= 7;
			break;
		case 80:
			value_temp |= 5;
			break;
		default:
			while(1);
	}

	REG32(PUBL_REG_BASE+PUBL_CFG_ADD_ZQ0CR1) = value_temp;
	//value_temp = (8<<18) | (2750<<6) |27;	//per 533MHz,actually ytDLLLOCK is counted in PCLK domain
	value_temp = (8<<18) | (393<<6) |27;	//caution: you must make sure PCLK is 76.8MHz
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PTR0) = value_temp;

	//PTR1
	//value_temp[18:0] = count(200us/clk_emc_period);//CKE high time to first command (200 us)
	//value_temp[26:19] = don't care;//CKE low time with power and clock stable (100 ns) for lpddr2
	value_temp = (200 * 1000 * 10) / 25; //CKE high time to first command (200 us)
	value_temp |= (1000 / 25) << 19; //CKE low time with power and clock stable (100 ns) for lpddr2
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PTR1 ) = value_temp;

	//PTR2
	//value_temp[16:0] = count((200us_for_ddr3 or 11us_for_lpddr2) /clk_emc_period);
	//value_temp[26:17] = count(1us/clk_emc_period);
	switch(mem_type_enum)
	{
		case LPDDR1:
			{
				value_temp = 0;
				break;
			}
		case LPDDR2:
			{
				value_temp = (11 * 1000 * 10) / (25);
				break;
			}
		case DDR3:
			{
				value_temp = (200 * 1000 * 10) / (25);
				break;
			}
		default:
			break;
	}
	value_temp |= ((1 * 1000 * 10) / 25) << 17;
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PTR2 ) = value_temp;

	//ACIOCR
	value_temp = (mem_type_enum == LPDDR1) ? 0x1 : 0x0;
	modify_reg_field(PUBL_REG_BASE + PUBL_CFG_ADD_ACIOCR, 0, 1, value_temp);


	value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DCR);
	value_temp &= ~(0x7);
	value_temp &= ~(0x1 << 3);
	value_temp &= ~(0x3 << 8);
	value_temp |= (mem_type_enum == LPDDR2 ) ? 0x4 : 0x0;
	value_temp |= ( (mem_type_enum == LPDDR2 ) ? 0x1 : 0x0) << 3;
	value_temp |= 0x00 << 8; //lpddr2-S4?????? xiaohui
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DCR) = value_temp;

	//MR0
	value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_MR0);
	switch(mem_type_enum)
	{
		case LPDDR2:
			{
				value_temp = 0x0; //not applicable
				break;
			}
		case LPDDR1:
			{
				value_temp &= ~(0x7);
				value_temp &= ~(0x1 << 3);
				value_temp &= ~(0x7 << 4);
				value_temp &= ~(0x1 << 7);
				value_temp |= (_LPDDR1_BL_ == 2 ? 0x1 :
					_LPDDR1_BL_ == 4 ? 0x2 :
					_LPDDR1_BL_ == 8 ? 0x3 :
					_LPDDR1_BL_ == 16 ? 0x4 : 0x1);
				value_temp |= (_LPDDR1_BT_ << 3);
				value_temp |= (_LPDDR1_CL_ == 2 ? 0x2 :
					_LPDDR1_CL_ == 3 ? 0x3 :
					_LPDDR1_CL_ == 4 ? 0x4 : 0x3) << 4;
				value_temp |= 0x0 << 7; //normal operation
				break;
			}
		default:
			break;
	}
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_MR0) = value_temp;

	//MR1
	value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_MR1);
	switch(mem_type_enum)
	{
		case LPDDR2:
			{
				value_temp &= ~0x7;
				value_temp &= ~(0x3 << 3);
				value_temp &= ~(0x7 << 5);
				value_temp |= (_LPDDR2_BL_ == 4 ? 0x2 :
					_LPDDR2_BL_ == 8 ? 0x3 :
					_LPDDR2_BL_ == 16 ? 0x4 : 0x3);
				value_temp |= _LPDDR2_BT_ << 3;
				value_temp |= _LPDDR2_WC_ << 4;
				value_temp |= (0x4 << 5); //nWR=6 //!check
				break;
			}
		case LPDDR1: {break;}
		default:
			break;
	}
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_MR1) = value_temp;

	//MR2
	switch(mem_type_enum)
	{
		case LPDDR1:
			{
				value_temp = 0x0;    //extend mode
				break;
			}
		case LPDDR2:
			{
				value_temp = 0x4;    //RL = 6 / WL = 3
				break;
			}
		default:
			break;
	}
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_MR2) = value_temp;

	//MR3
	value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_MR3);
	value_temp &= ~0xf;
	value_temp |= 0x2;
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_MR3) = value_temp;
	//0DTCR. disable 0DT for write and read for LPDDR2/LPDDR1
	value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_ODTCR);
	switch(mem_type_enum)
	{
		case LPDDR1:
			value_temp &= ~0xff;
			value_temp &= ~(0xff << 16);
			break;
		case LPDDR2:
			value_temp &= ~0xff;
			value_temp &= ~(0xff << 16);
			break;
		default:
			while(1);
	}
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_ODTCR) = value_temp;

	//DTPR0
	value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DTPR0);
	switch(mem_type_enum)
	{
		case LPDDR1:
			{
				//value_temp[1:0] = 2;    //tMRD
				//value_temp[4:2] = 2;    //tRTP
				//value_temp[7:5] = (mem_type_enum == LPDDR2 ) ? 0x00000004: 0x00000002;
				//value_temp[11:8] = 4;    //tRP
				//value_temp[15:12] = 4;    //tRCD
				//value_temp[20:16] = 8;    //tRAS
				//value_temp[24:21] = 2;    //tRRD
				//value_temp[30:25] = 12;    //tRC
				//value_temp[31] = 0x0;    //tCCD: BL/2
				value_temp = (0x0 << 31) | (12 << 25) | (2 << 21) | (8 << 16) | (4 << 12) | (4 << 8) | (2 << 2) | (2);
				value_temp |= 0x00000002 << 5;
				break;
			}
		case LPDDR2:
			{
				value_temp = 0x36916a6d;//??? xiaohui
				break;
			}
		default:
			break;
	}
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DTPR0) = value_temp;

	//DTPR1
	value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DTPR1);
	value_temp &= ~0x3;
	value_temp &= ~(0xff << 16);
	switch(mem_type_enum)
	{
		case LPDDR1:
			{
				value_temp |= 0x1; //same with lpddr2 to avoid additional code size
				value_temp |= (32 << 16);  //tRFC
				break;
			}
		case LPDDR2:
			{
				value_temp = 0x193400a0;//??? xiaohui
				break;
			}
		default:
			break;
	}
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DTPR1) = value_temp;

	//DTPR2
	value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DTPR2);
	switch(mem_type_enum)
	{
		case LPDDR2:
			{
				value_temp = 0x1001a0c8; //actually is default value
				break;
			}
		case LPDDR1:
		default:
			break;
	}
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DTPR2) = value_temp;

	value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DXCCR);
	value_temp &= ~(0x1 << 1);
	value_temp &= ~(0xf << 4);
	value_temp &= ~(0xf << 8);
	value_temp &= ~(0x1 << 14);
	value_temp |= ((mem_type_enum == LPDDR1 ) ? 0x1 : 0x0) << 1; //iom. 0:LPDDR1, 1: others
	value_temp |= (((mem_type_enum == LPDDR2)|(mem_type_enum==LPDDR1)) ?   (DQSR_VALUE) : 0x0) << 4; //pull down resistor for DQS
	value_temp |= (((mem_type_enum == LPDDR2)|(mem_type_enum==LPDDR1)) ? (8|DQSR_VALUE) : 0x0) << 8; //pull down resistor for DQS_N
	value_temp |= ((mem_type_enum == LPDDR2 ) ? 0x0 : 0x1) << 14; //DQS# Reset
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DXCCR) = value_temp;

	value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX0GCR);
	value_temp &= ~((0x3 << 9) | (1 << 3)); //disable DQ/DQS Dynamic RTT Control
	value_temp |= ((mem_type_enum == LPDDR1)? 0x1 : 0x0) << 3;
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX0GCR) = value_temp;

	value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX1GCR);
	value_temp &= ~((0x3 << 9) | (1 << 3)); //disable DQ/DQS Dynamic RTT Control
	value_temp |= ((mem_type_enum == LPDDR1)? 0x1 : 0x0) << 3;
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX1GCR) = value_temp;

	value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX2GCR);
	value_temp &= ~((0x3 << 9) | (1 << 3)); //disable DQ/DQS Dynamic RTT Control
	value_temp |= ((mem_type_enum == LPDDR1)? 0x1 : 0x0) << 3;
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX2GCR) = value_temp;

	value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX3GCR);
	value_temp &= ~((0x3 << 9) | (1 << 3)); //disable DQ/DQS Dynamic RTT Control
	value_temp |= ((mem_type_enum == LPDDR1)? 0x1 : 0x0) << 3;
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX3GCR) = value_temp;

	value_temp = (0 << 30) | (0x7fff << 13) | (0x7 << 10) | (0x300);
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_DTUWACTL) = value_temp;
	value_temp = (0 << 30) | (0x7fff << 13) | (0x7 << 10) | (0x300);
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_DTURACTL) = value_temp;

	value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PGCR);
	value_temp &= ~0x7;
	value_temp &= ~(0xf << 18);
	value_temp |= (mem_type_enum == LPDDR2 ) ? 0x0 : 0x1; //0 = ITMS uses DQS and DQS#
	//1 = ITMS uses DQS only
	value_temp |= (0x1 << 1);
#if 1
	if(param->cs_number == 1)
	{
		value_temp |= (0x1<<18); //only enable CS0 for data training
	}
	else
	{
		value_temp |= (0x3 << 18); //enable CS0/1 for data training
	}
#else
	value_temp |= (0x1<<18); //only enable CS0 for data training
#endif
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PGCR) = value_temp;

	value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DSGCR);
	value_temp &= ~0xfff;

	//CAUTION:[7:5] DQSGX, [10:8] DQSGE
	value_temp |= (0x1f | ((GATE_EARLY_LATE)<<8) | ((GATE_EARLY_LATE<<5)));
	value_temp &= ~(1<<2); // zq Update Enable,CHECK!!!!
	value_temp &= ~(1<<4); // Low Power DLL Power Down
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DSGCR) = value_temp;
	if(0){
            enable_lpddr12_low_power_feature(32,32,0);
	}
} //emc_init_common_reg

//bps200, control the DLL disable mode. 0, DLL freq is below 100MHz. 1: DLL freq is in 100~200MHz
static inline void set_ddrphy_dll_bps200_mode(u32 bps200) {
	modify_reg_field(PUBL_REG_BASE+PUBL_CFG_ADD_DLLGCR, 23, 1, bps200);
}
static inline void update_umctl_timing_cfg(MEM_TYPE_ENUM mem_type, u32 clk_emc_div) {
	if(mem_type!=LPDDR2)
		while(1);
    switch(clk_emc_div) {
        case 3: { //100MHz
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRTW )        = 0x1;//timing_cfg.TRTW;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRFC )        = 0xd;//timing_cfg.TRFC;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRP  )        = 0x00010003;//timing_cfg.TRP;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRAS )        = 0x5;//timing_cfg.TRAS;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRC  )        = 0x8;//timing_cfg.TRC;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRCD )        = 0x3;//timing_cfg.TRCD;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRRD )        = 0x2;//timing_cfg.TRRD;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRTP )        = 0x2;//timing_cfg.TRTP;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TWR  )        = 0x3;//timing_cfg.TWR;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TWTR )        = 0x2;//timing_cfg.TWTR;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TZQCS)        = 0x9;//timing_cfg.TZQCS;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TZQCSI)       = 0x0;//zqcsi_en ? timing_cfg.TZQCSI : 0x0;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TEXSR)        = 0xe;//timing_cfg.TXSR ;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TOGCNT1U)     = 100;//timing_cfg.TOGCNT1U ;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TOGCNT100N)   = 10 ;//timing_cfg.TOGCNT100N ;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TCL)          = 0x3;//timing_cfg.TCL;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TCWL)         = 0x1;//timing_cfg.TCWL;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_DFITRDDATAEN) = 0x2;//timing_cfg.DFITRDDATAEN ;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_DFITPHYWRLAT) = 0x1;//timing_cfg.DFITPHYWRLAT ;
	        break;
        }
        case 1: { //200MHz
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRTW )        = 0x1; //timing_cfg.TRTW;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRFC )        = 0x1a;//timing_cfg.TRFC;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRP  )        = 0x00010004; //timing_cfg.TRP;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRAS )        = 0x9; //timing_cfg.TRAS;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRC  )        = 0xD; //timing_cfg.TRC;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRCD )        = 0x4; //timing_cfg.TRCD;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRRD )        = 0x2; //timing_cfg.TRRD;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRTP )        = 0x2; //timing_cfg.TRTP;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TWR  )        = 0x3; //timing_cfg.TWR;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TWTR )        = 0x2; //timing_cfg.TWTR;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TZQCS)        = 0x12;//timing_cfg.TZQCS;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TZQCSI)       = 0x0; //zqcsi_en ? timing_cfg.TZQCSI : 0x0;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TEXSR)        = 0x1c;//timing_cfg.TXSR ;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TOGCNT1U)     = 200; //timing_cfg.TOGCNT1U ;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TOGCNT100N)   = 20 ; //timing_cfg.TOGCNT100N ;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TCL)          = 0x3; //timing_cfg.TCL;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TCWL)         = 0x1; //timing_cfg.TCWL;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_DFITRDDATAEN) = 0x2; //timing_cfg.DFITRDDATAEN ;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_DFITPHYWRLAT) = 0x1; //timing_cfg.DFITPHYWRLAT ;

	        break;
        }
	    case 0: { //400MHz
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRTW )        = 0x1;//timing_cfg.TRTW;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRFC )        = 0x34;//timing_cfg.TRFC;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRP  )        = 0x00020008;//timing_cfg.TRP;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRAS )        = 0x11;//timing_cfg.TRAS;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRC  )        = 0x19;//timing_cfg.TRC;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRCD )        = 0x8;//timing_cfg.TRCD;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRRD )        = 0x4;//timing_cfg.TRRD;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TRTP )        = 0x3;//timing_cfg.TRTP;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TWR  )        = 0x6;//timing_cfg.TWR;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TWTR )        = 0x3;//timing_cfg.TWTR;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TZQCS)        = 0x24;//timing_cfg.TZQCS;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TZQCSI)       = 0x0;//zqcsi_en ? timing_cfg.TZQCSI : 0x0;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TEXSR)        = 0x38;//timing_cfg.TXSR ;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TOGCNT1U)     = 400;//timing_cfg.TOGCNT1U ;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TOGCNT100N)   = 40 ;//timing_cfg.TOGCNT100N ;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TCL)          = 0x6;//timing_cfg.TCL;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_TCWL)         = 0x3;//timing_cfg.TCWL;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_DFITRDDATAEN) = 0x5;//timing_cfg.DFITRDDATAEN ;
            REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_DFITPHYWRLAT) = 0x3;//timing_cfg.DFITPHYWRLAT ;
	        break;
        }
	    default: {
	        while(1);
	    }
    }
}

//update MR0~MR3
//caution: now only support LPDDR2, for LPDDR1 you should double check MR0~MR3
//and modify accordingly
static inline void update_publ_timing_cfg(MEM_TYPE_ENUM mem_type_enum,
										u32 gate_early_late,
										u32 nwr,
										u32 mem_wc,
										u32 mem_bt,
										u32 mem_bl,
										u32 rl_wl)
{
	switch(mem_type_enum) {
		case LPDDR2: {
		REG32(PUBL_REG_BASE+PUBL_CFG_ADD_MR1) = (nwr<<5) | (mem_wc<<4) | (mem_bt<<3) | mem_bl;
		REG32(PUBL_REG_BASE+PUBL_CFG_ADD_MR2) = rl_wl;
		REG32(PUBL_REG_BASE+PUBL_CFG_ADD_DSGCR) &= ~(0x3f<<5);
		REG32(PUBL_REG_BASE+PUBL_CFG_ADD_DSGCR) |= (gate_early_late<<5)|(gate_early_late<<8);
			break;
		}
		default: {
			while(1);
		}
	}
}

static inline void update_lpddr2_modereg(u32 nwr,u32 mem_wc, u32 mem_bt,u32 mem_bl,u32 rl_wl) {
    u32 value_temp;
    REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_MCMD) = 0x80f00013 | (nwr<<17)|(mem_wc<<16)|(mem_bt<<15)|(mem_bl<<12);
    do  value_temp = REG32(UMCTL_REG_BASE+ UMCTL_CFG_ADD_MCMD);
    while((value_temp & 0x80000000) == 0x80000000);
    REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_MCMD) = 0x80f00023|(rl_wl<<12);
    do  value_temp = REG32(UMCTL_REG_BASE+ UMCTL_CFG_ADD_MCMD);
    while((value_temp & 0x80000000) == 0x80000000);
}

//the argument power_off should be 0x1 if EMC controller power is shut down in deep sleep mode.
//otherwise it should be 0x0.This argument indicates whether the common control register should be
//re-programmed
static inline void __emc_init_repowered(u32 power_off, struct emc_repower_param *param, u32 clk_emc_div)   //{{{
{
	u32  value_temp;
	u32 volatile  i;
	u32  value_temp1;
	MEM_TYPE_ENUM mem_type_enum;

	//MEM_DENSITY_ENUM mem_density_enum;
	mem_type_enum = param->mem_type;

	////cfg clk emc to 200MHz if lpddr1
	////it must use AHB to config
	//if(mem_type_enum == LPDDR1)
	//{
	//	value_temp = REG32(ADDR_AHBREG_ARMCLK);
	//	value_temp &= ~(0x3 << 12);
	//	value_temp &= ~(0xf << 8);
	//	value_temp |= 1 << 12; //clk_emc_sel 0->(mpll/2), 1->dpll 2->256 3->26
	//	value_temp |= 1 << 8;
	//	REG32(ADDR_AHBREG_ARMCLK) = value_temp;
	//}

	//value_temp = REG32(0x4B000080);
	//value_temp |= 0x1 << 1;
	//REG32(0x4B000080) = value_temp;

	//value_temp = REG32(0x2090_0308);
	//value_temp[9] = 0x1;
	//REG32(0x2090_0308) = value_temp;
	enable_umctl_dtu();
	if(power_off) {
		emc_init_common_reg(param);
		#if 1
		update_umctl_timing_cfg(mem_type_enum, clk_emc_div);
		switch(clk_emc_div) {
			case 0: {
				update_publ_timing_cfg(mem_type_enum,      //MEM_TYPE_ENUM mem_type,
									2,                  //u32 gate_early_late,
									0x4,                //u32 nwr
									_LPDDR2_WC_,        //u32 mem_wc,
									_LPDDR2_BT_,        //u32 mem_bt,
									_LPDDR2_BL_==4?2:   //u32 mem_bl,
									_LPDDR2_BL_==8?3:4,
									0x4                 //u32 rl_wl
										);
				break;
			}
			default: {
				update_publ_timing_cfg(mem_type_enum,     //MEM_TYPE_ENUM mem_type,
									   1,                  //u32 gate_early_late,
									   0x1,               //,u32 nwr
									_LPDDR2_WC_,       //u32 mem_wc,
									_LPDDR2_BT_,       //u32 mem_bt,
									_LPDDR2_BL_==4?2:  //u32 mem_bl,
									_LPDDR2_BL_==8?3:4,
									0x1                //u32 rl_wl
										);
				break;
			}
		}
		#endif
	}

	do  value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PGSR);
	while((value_temp & 0x1) == 0);

	//value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PIR);
	value_temp = 0x0;
	value_temp |= 1 << 30; //zq calibration bypass
	value_temp |= 1 << 18; //Controller DRAM Initialization
	value_temp |= 1;
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PIR) = value_temp;

	//according to PUBL databook on PIR operation.
	//10 configuration clock cycle must be waited before polling PGSR
	for (i=0; i<100; i++);

	do value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PGSR);
	while((value_temp & 0x1) == 0);

	do value_temp = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_DFISTSTAT0);
	while((value_temp & 0x1) == 0);

	/*
	do
	{
		value_temp = REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_DFISTSTAT0);
	} while((value_temp&1)==0);
	REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_POWCTL)=1;
	do
	{
		value_temp = REG32(UMCTL_REG_BASE+UMCTL_CFG_ADD_POWSTAT);
	} while((value_temp&1)==0);
	*/

	value_temp1 = REG32(0x20900260);  //read the pre-retention value
	value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_ZQ0CR0);
	value_temp &= ~0xfffff;
	value_temp &= ~(0x1 << 28);
	value_temp |= 1 << 28;
	value_temp |= (value_temp1 & 0xfffff);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_ZQ0CR0) = value_temp;
	//if EMC is not powered off, EMC may has been moved to acces state by the hardware low power
	//interfae. In this state, all the DFI signals from PUBL to DDRPHY may be toggling. So, We'd better
	//first move upctl state to low power to make signals return the state before power is turned on.
	//Then we can deassert retention control signal
	if(power_off == 0x0) {
		move_upctl_state_to_low_power();
	}

	value_temp = REG32(0x4B000080);
	value_temp &= ~(0x1 << 2);
	value_temp |= 0x1 << 2;
	REG32(0x4B000080) = value_temp;
	value_temp &= ~(0x1 << 2);
	REG32(0x4B000080) = value_temp;

	value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_ZQ0CR0);
	value_temp   &= ~(0x1 << 28);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_ZQ0CR0) = value_temp;
#if 0
	value_temp = 0x1 | (0x1 << 3);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PIR) = value_temp;
	for(i = 0; i < 100; i++) {}
	do value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PGSR);
	while((value_temp & 0x1) == 0);
#endif
	value_temp = 0x1 | (0x1 << 18); //Controller DRAM Initialization
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PIR) = value_temp;
	for(i = 0; i < 100; i++) {}
	do value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PGSR);
	while((value_temp & 0x1) == 0);

	//first move upctl to low power state before issuing wakeup command
	if(power_off) {
		move_upctl_state_to_low_power();
	}
	move_upctl_state_to_access();

#if 0
	value_temp = 0x4;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_SCTL) = value_temp;
	do value_temp = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_STAT);
	while((value_temp & 0x7) != 0x3);
#endif
	move_upctl_state_to_config();
	#if 1
	//now memory mode update only support LPDDR2, for other memory, you should check
	if(mem_type_enum!=LPDDR2)
		while(1);
	switch(clk_emc_div) {
	    case 0:{
	        update_lpddr2_modereg(0x4,   //u32 nwr,
				_LPDDR2_WC_,       //u32 mem_wc,
				_LPDDR2_BT_,       //u32 mem_bt,
				_LPDDR2_BL_==4?2:  //u32 mem_bl,
				_LPDDR2_BL_==8?3:4,
				0x4                //u32 rl_wl
				);
	        break;
	    }
	    default: { //for frequency less than 200MHz
	        update_lpddr2_modereg(0x1,   //u32 nwr,
				_LPDDR2_WC_,       //u32 mem_wc,
				_LPDDR2_BT_,       //u32 mem_bt,
				_LPDDR2_BL_==4?2:  //u32 mem_bl,
				_LPDDR2_BL_==8?3:4,
				0x1                //u32 rl_wl
				  );
	        break;
	    }
	}
#endif
	if(power_off) {
		#if 0 //manually set DQSTR control register
		REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX0DQSTR) = 0x3DB06000;
		REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX1DQSTR) = 0x3DB06000;
		REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX2DQSTR) = 0x3DB06000;
		REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX3DQSTR) = 0x3DB06000;
		#else
		emc_publ_do_gate_training();
		#endif

	}
	////disable emc auto self-refresh
	//value_temp = REG32(0x20900308);
	//value_temp &= ~(0x1 << 8);
	//REG32(0x20900308) = value_temp;
	REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_SCFG) = 0x00000421;
	move_upctl_state_to_access();
	disable_umctl_dtu();
}


static inline void modify_reg_field(u32 addr, u32 start_bit, u32 bit_num, u32 value)
{
	u32 temp, i;
	temp = REG32(addr);
	for (i=0; i<bit_num; i++)
	{
		temp &= ~(1<<(start_bit+i));
	}
	temp |= value<<start_bit;
	REG32(addr) = temp;
}
static inline u32 polling_reg_bit_field(u32 addr, u32 start_bit, u32 bit_num, u32 value)
{
		uint32 temp, i;
		uint32 exp_value;
		uint32 mask;
		mask = 0;
		for(i=0;i<bit_num;i++)
		{
				mask |= (1<<(start_bit+i));
		}
		exp_value = (value << start_bit);
		do {temp = REG32(addr);}
		while((temp&mask)!=exp_value);
		return temp;
}
static inline void disable_clk_emc(void)
{
	REG32(ADDR_AHBREG_AHB_CTRL0) &= ~(0x1<<28);
}

static inline void enable_clk_emc(void)
{
	REG32(ADDR_AHBREG_AHB_CTRL0) |= (0x1<<28);
}

static inline void assert_reset_acdll(void)
{
	REG32(PUBL_REG_BASE+PUBL_CFG_ADD_ACDLLCR) &= ~(0x1<<30);
}

static inline void deassert_reset_acdll(void)
{
	REG32(PUBL_REG_BASE+PUBL_CFG_ADD_ACDLLCR) |= (0x1<<30);
}

static inline void assert_reset_dxdll(void)
{
	REG32(PUBL_REG_BASE+PUBL_CFG_ADD_DX0DLLCR) &= ~(0x1<<30);
	REG32(PUBL_REG_BASE+PUBL_CFG_ADD_DX1DLLCR) &= ~(0x1<<30);
	REG32(PUBL_REG_BASE+PUBL_CFG_ADD_DX2DLLCR) &= ~(0x1<<30);
	REG32(PUBL_REG_BASE+PUBL_CFG_ADD_DX3DLLCR) &= ~(0x1<<30);
}

static inline void deassert_reset_dxdll(void)
{
	REG32(PUBL_REG_BASE+PUBL_CFG_ADD_DX0DLLCR) |= (0x1<<30);
	REG32(PUBL_REG_BASE+PUBL_CFG_ADD_DX1DLLCR) |= (0x1<<30);
	REG32(PUBL_REG_BASE+PUBL_CFG_ADD_DX2DLLCR) |= (0x1<<30);
	REG32(PUBL_REG_BASE+PUBL_CFG_ADD_DX3DLLCR) |= (0x1<<30);	}

static inline void assert_reset_ddrphy_dll(void)
{
	assert_reset_acdll();
	assert_reset_dxdll();
}
static inline void deassert_reset_ddrphy_dll(void)
{
	deassert_reset_acdll();
	deassert_reset_dxdll();
}
static inline void modify_dpll_freq (u32 freq) {
	u32 temp;
	//step1: enable register write
	modify_reg_field(GLB_REG_WR_REG_GEN1,9,1,1);
	//step2: modify dpll parameter N
	/*
	switch(freq) {
	  case 100: {temp=25; break;}
	  case 133: {temp=34; break;}
	  case 166: {temp=42; break;}
	  case 200: {temp=50; break;}
	  case 233: {temp=58; break;}
	  case 266: {temp=66; break;}
	  case 333: {temp=83; break;}
	  case 400: {temp=100; break;}
	  default: {temp=100; break;}
	}
	*/
	temp = (freq >> 2);
#if defined(CONFIG_ARCH_SC8825)
       modify_reg_field(GLB_REG_DPLL_CTRL,0,11,temp);
#elif defined(CONFIG_ARCH_SCX35)
       modify_reg_field(REG_AON_APB_DPLL_CFG,0,11,temp);
#endif
	//step2: disable register write
	modify_reg_field(GLB_REG_WR_REG_GEN1,9,1,0);
}

//get the dpll frequency
static inline u32 get_dpll_freq_value(void){
	u32 temp;
	//step1: enable register write
	modify_reg_field(GLB_REG_WR_REG_GEN1,9,1,1);
	//step2: get the value
	temp = 0x7ff;
#if defined(CONFIG_ARCH_SC8825)
	temp &= REG32(GLB_REG_DPLL_CTRL);
#elif defined(CONFIG_ARCH_SCX35)
	temp &= REG32(REG_AON_APB_DPLL_CFG);
#endif
	temp = (temp<<2);
	//step3: disable register write
	modify_reg_field(GLB_REG_WR_REG_GEN1,9,1,0);
	return temp;
}
static inline void modify_clk_emc_div(u32 div) {
	modify_reg_field(ADDR_AHBREG_ARMCLK, 8, 4, div);
}
/*
static void wait_100ns(void)
{
	u32 volatile i;
	for (i=0; i<100; i++);
}
static inline void wait_1us(void)
{
	u32 volatile i;
	for (i=0; i<(1000 / 100); i++);
}

static inline void wait_us(u32 us)
{
	u32 volatile i;
	for (i=0; i<us; i++)
		wait_1us();
}
*/
/*
static void modify_emc_clk(u32 freq)
{
	disable_clk_emc();
	assert_reset_acdll();
	assert_reset_dxdll();
	modify_dpll_freq(freq);

	modify_reg_field(ADDR_AHBREG_ARMCLK, 12, 2, 1);
	modify_reg_field(ADDR_AHBREG_ARMCLK, 8, 4, 0);	//
	modify_reg_field(ADDR_AHBREG_ARMCLK, 3, 1, 1);	//

	wait_us(150);

	enable_clk_emc();
	deassert_reset_acdll();
	deassert_reset_dxdll();
	wait_us(10);
}
*/

//default dll enable mode
static inline void modify_clk_emc_freq(u32 clk_emc_div)
{
	u32 value_temp;
	disable_clk_emc();
	assert_reset_acdll();
	assert_reset_dxdll();

	modify_clk_emc_div(clk_emc_div);	//

	enable_clk_emc();
	deassert_reset_acdll();
	deassert_reset_dxdll();

	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PIR) = 0x5; //
	wait_n_pclk_cycle(5);
	do{
	    value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PGSR);
	} while((value_temp&0x1)==0);
}
static inline void reset_ddrphy_dll(void)
{
	u32 value_temp;
	u32 i;
	do{
	    value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PGSR);
	} while((value_temp&0x1)==0);

	disable_clk_emc();

	for(i = 0; i < 10; i++);	//wait for clock tree propaation done
	assert_reset_acdll();
	assert_reset_dxdll();
	enable_clk_emc();           //wait for clock tree propaation done
	for(i = 0; i < 10; i++);
	deassert_reset_acdll();
	deassert_reset_dxdll();
//	timer2_value_10 = REG32(0x41000044);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PIR) = 0x5; //
	for(i = 0; i < 10; i++);
	do{
	    value_temp = REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PGSR);
//	timer2_value_11 = REG32(0x41000044);
	} while((value_temp&0x1)==0);
}
/*
static void bypass_ddrphy_dll()
{
	//modify_reg_field(PUBL_REG_BASE + PUBL_CFG_ADD_ACDLLCR, 31, 1, 0x1);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_ACDLLCR) |= (1 << 31);
	//modify_reg_field(PUBL_REG_BASE + PUBL_CFG_ADD_DX0DLLCR, 31, 1, 0x1);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX0DLLCR) |= (1 << 31);
	//modify_reg_field(PUBL_REG_BASE + PUBL_CFG_ADD_DX1DLLCR, 31, 1, 0x1);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX1DLLCR) |= (1 << 31);
	//modify_reg_field(PUBL_REG_BASE + PUBL_CFG_ADD_DX2DLLCR, 31, 1, 0x1);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX2DLLCR) |= (1 << 31);
	//modify_reg_field(PUBL_REG_BASE + PUBL_CFG_ADD_DX3DLLCR, 31, 1, 0x1);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX3DLLCR) |= (1 << 31);
}
*/

static inline void disable_ddrphy_dll(void) {
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_ACDLLCR) |= (1 << 31);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX0DLLCR) |= (1 << 31);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX1DLLCR) |= (1 << 31);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX2DLLCR) |= (1 << 31);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX3DLLCR) |= (1 << 31);
}
static inline void enable_ddrphy_dll(void) {
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_ACDLLCR) &= ~(1 << 31);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX0DLLCR) &= ~(1 << 31);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX1DLLCR) &= ~(1 << 31);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX2DLLCR) &= ~(1 << 31);
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_DX3DLLCR) &= ~(1 << 31);

}


static inline void publ_grant_dfi_to_umctl(void) {
	REG32(PUBL_REG_BASE + PUBL_CFG_ADD_PIR) = (1<<18)|1;
	polling_reg_bit_field(PUBL_REG_BASE + PUBL_CFG_ADD_PGSR, 0, 1, 1); //polling phy init done
}
static inline u32 get_emc_size(u32 reg_value)
{
	u32 size;
	size = 1 << (reg_value + 23);
	return size;
}
void set_emc_repower_param(struct emc_repower_param *param, u32 umctl_base, u32 publ_base)
{
//	u32 umctl_base;
//	u32 publ_base;
	u32 value;
	u32 div = 0;
	printk("set_emc_repower_param param = 0x%x, umctl_base = 0x%x, publ_base = 0x%x\r\n", (u32)param, umctl_base, publ_base);
	param->flag = EMC_REPOWER_PARAM_FLAG;
	value = REG32(umctl_base + UMCTL_CFG_ADD_MCFG);
	//check LPDDR2 enabled
	if((value & (0x3 << 22)) == (0x3 << 22))
	{
		//check DDR3 enable
		if((value & (0x1 << 5)) == (0x1 << 5))
		{
			param->mem_type = DDR3;
		}
		else
		{
			param->mem_type = LPDDR2;
		}
	}
	else
	{
		param->mem_type = LPDDR1;
	}
	value = REG32(umctl_base + UMCTL_CFG_ADD_PPCFG);
	if(value & 0X1)
	{
		param->mem_width = X16;
	}
	else
	{
		param->mem_width = X32;
	}
	value = REG32(publ_base + PUBL_CFG_ADD_ZQ0CR1);
	value &= 0xf;
	switch(value)
	{
	case 0xd:
		param->mem_drv = 34;
		break;
	case 0xb:
		param->mem_drv = 40;
		break;
	case 0x9:
		param->mem_drv = 48;
		break;
	case 0x7:
		param->mem_drv = 60;
		break;
	case 0x5:
		param->mem_drv = 80;
		break;
	default:
		while(1);
	}
	value = REG32(publ_base + PUBL_CFG_ADD_PGCR);
	if((value & (0x3 << 18)) == (0x3 << 18))
	{
		param->cs_number = 2;
	}
	else
	{
		param->cs_number = 1;
	}
	value = REG32(umctl_base + UMCTL_CFG_ADD_DCFG_CS0);
	value = (value >> 2);
	value &= 0xf;
	param->cs0_size = get_emc_size(value);
	if(param->cs_number == 2)
	{
		value = REG32(umctl_base + UMCTL_CFG_ADD_DCFG_CS1);
		value = (value >> 2);
		value &= 0xf;
		param->cs1_size = get_emc_size(value);
	}
#if defined(CONFIG_ARCH_SC8825)
	value = sci_glb_read(REG_GLB_D_PLL_CTL, -1UL);
#elif defined(CONFIG_ARCH_SCX35)
	value = sci_glb_read(REG_AON_APB_DPLL_CFG, -1UL);
#endif
	value &= 0x7ff;
#if defined(CONFIG_ARCH_SC8825)
	div = sci_glb_read(REG_AHB_ARM_CLK, -1UL);
#elif defined(CONFIG_ARCH_SCX35)
	//will be add fot the future
#endif
	div >>= 8;
	div &= 0xf;
	param->emc_freq = (value * 0x4) / (div + 1);
	param->cs0_training_addr_v = 0;
	param->cs0_training_addr_p = EMC_MEM_BASE_ADDR;
	param->cs0_training_data_size = 32;

	param->cs1_training_addr_v = 0;
	param->cs1_training_addr_p = EMC_MEM_BASE_ADDR + param->cs0_size;
	param->cs1_training_data_size = 32;
	printk("emc repower param\r\n");
	printk("emc repower param mem_type %x\n", param->mem_type);
	printk("emc repower param emc_freq %dMHz\n", param->emc_freq);
	printk("emc repower param mem_drv %x\n", param->mem_drv);
	printk("emc repower param cs_number %x\n", param->cs_number);
	printk("emc repower param cs0_size %x\n", param->cs0_size);
	printk("emc repower param cs1_size %x\n", param->cs1_size);
	printk("emc repower param cs0_training_addr_v %x\n", param->cs0_training_addr_v);
	printk("emc repower param cs0_training_addr_p %x\n", param->cs0_training_addr_p);
	printk("emc repower param cs0_training_data_size %x\n", param->cs0_training_data_size);

	printk("emc repower param cs1_training_addr_v %x\n", param->cs1_training_addr_v);
	printk("emc repower param cs1_training_addr_p %x\n", param->cs1_training_addr_p);
	printk("emc repower param cs1_training_data_size %x\n", param->cs1_training_data_size);
}
void save_emc_trainig_data(struct emc_repower_param *param)
{
	u32 *dst;
	u32 *src;
	u32 i;
	dst = (u32 *)param->cs0_saved_data;
	src = (u32 *)param->cs0_training_addr_v;
#if 0
	//CS0 traininig data saved by DSP code
	for(i = 0; i < param->cs0_training_data_size; i++) {
		*(dst + i) = *(src + i);
	}
#endif
	if(param->cs_number == 2) {
		dst = (u32 *)param->cs1_saved_data;
		src = (u32 *)param->cs1_training_addr_v;
		for(i = 0; i < param->cs1_training_data_size / 4; i++) {
			*(dst + i) = *(src + i);
		}
	}
}
static inline void restore_emc_training_data(struct emc_repower_param *param)
{
	u32 *dst;
	u32 *src;
	u32 i;
	dst = (u32 *)param->cs0_training_addr_p;
	src = (u32 *)param->cs0_saved_data;
#if 0
	//CS0 traininig data restored by DSP code
	for(i = 0; i < param->cs0_training_data_size; i++) {
		*(dst + i) = *(src + i);
	}
#endif
	if(param->cs_number == 2) {
		dst = (u32 *)param->cs1_training_addr_p;
		src = (u32 *)param->cs1_saved_data;
		for(i = 0; i < param->cs1_training_data_size / 4; i++) {
			*(dst + i) = *(src + i);
		}
	}
}
inline struct emc_repower_param * get_emc_repower_param(void)
{
	struct emc_repower_param * param;
	param = (struct emc_repower_param *)(SPRD_IRAM_PHYS + 15 * 1024);
	return param;
}

void emc_init_repowered(u32 power_off)
{
	u32 clk_emc_div;
	u32 tmp_val;
	struct emc_repower_param *param_p;
	param_p = (struct emc_repower_param *)(SPRD_IRAM_PHYS + 15 * 1024);
	tmp_val = REG32(UMCTL_REG_BASE + UMCTL_CFG_ADD_STAT);
	if((tmp_val & 0x7) != 0) {
		return;
	}
	clk_emc_div = 3;
	param_p->emc_freq = 400 / (clk_emc_div + 1);
	if(0) {
		modify_clk_emc_freq(clk_emc_div);
	    __emc_init_repowered(power_off, param_p, clk_emc_div);
	}
	else {
            disable_clk_emc();
            set_ddrphy_dll_bps200_mode((clk_emc_div>=3)? 0x0: 0x1);
            disable_ddrphy_dll();
	    modify_clk_emc_div(clk_emc_div);
            enable_clk_emc();
	    __emc_init_repowered(power_off, param_p, clk_emc_div);
	}
	//__emc_init_repowered(power_off, param_p, clk_emc_div);
	if(power_off)
	       restore_emc_training_data(param_p);
}
